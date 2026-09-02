#include "DedicatedCampaignFilesystem.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(__linux__)
#include <sys/syscall.h>
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif
#endif
#include <unistd.h>
#endif

namespace
{
constexpr std::size_t Sha256BlockBytes = 64;
std::atomic<std::uint64_t> TemporarySequence{1};

const std::filesystem::path& EmptyPath() noexcept
{
	static const std::filesystem::path empty;
	return empty;
}

std::size_t SlotIndex(DedicatedCampaignSlot slot) noexcept
{
	return slot == DedicatedCampaignSlot::A ? 0u : 1u;
}

bool KnownSlot(DedicatedCampaignSlot slot) noexcept
{
	return slot == DedicatedCampaignSlot::A ||
		slot == DedicatedCampaignSlot::B;
}

bool PortableCampaignId(const std::string& campaignId) noexcept
{
	if (campaignId.empty() ||
		campaignId.size() > DedicatedCampaignMaximumIdBytes)
		return false;
	for (const unsigned char value : campaignId)
	{
		if ((value >= 'a' && value <= 'z') ||
			(value >= '0' && value <= '9') || value == '-' || value == '_')
			continue;
		return false;
	}
	return true;
}

std::string CanonicalCampaignKey(const std::string& campaignId)
{
	std::string key = campaignId;
	std::transform(key.begin(), key.end(), key.begin(),
		[](unsigned char value) {
			return static_cast<char>(value >= 'A' && value <= 'Z'
				? value - 'A' + 'a' : value);
		});
	return key;
}

#ifdef _WIN32
HANDLE OpenHeldDirectory(
	const std::filesystem::path& path, bool shareDelete = false) noexcept;

enum class ManagedDirectoryResult
{
	Success,
	Unsafe,
	Failure
};

ManagedDirectoryResult PrepareManagedDirectory(
	const std::filesystem::path& parent,
	const std::filesystem::path& child,
	std::filesystem::path& canonicalChild,
	HANDLE& heldChild,
	bool shareDelete = false) noexcept
{
	heldChild = INVALID_HANDLE_VALUE;
	try
	{
		std::error_code error;
		const std::filesystem::file_status before =
			std::filesystem::symlink_status(child, error);
		if (error && error != std::errc::no_such_file_or_directory)
			return ManagedDirectoryResult::Failure;
		if (std::filesystem::exists(before))
		{
			if (std::filesystem::is_symlink(before) ||
				!std::filesystem::is_directory(before))
				return ManagedDirectoryResult::Unsafe;
		}
		else
		{
			error.clear();
			if (!std::filesystem::create_directory(child, error) && error)
				return ManagedDirectoryResult::Failure;
		}

		error.clear();
		const std::filesystem::file_status after =
			std::filesystem::symlink_status(child, error);
		if (error) return ManagedDirectoryResult::Failure;
		if (std::filesystem::is_symlink(after) ||
			!std::filesystem::is_directory(after))
			return ManagedDirectoryResult::Unsafe;
		heldChild = OpenHeldDirectory(child, shareDelete);
		if (heldChild == INVALID_HANDLE_VALUE)
			return ManagedDirectoryResult::Unsafe;

		canonicalChild = std::filesystem::canonical(child, error);
		if (error)
		{
			CloseHandle(heldChild);
			heldChild = INVALID_HANDLE_VALUE;
			return ManagedDirectoryResult::Failure;
		}
		const std::filesystem::path canonicalParent =
			std::filesystem::canonical(parent, error);
		if (error)
		{
			CloseHandle(heldChild);
			heldChild = INVALID_HANDLE_VALUE;
			return ManagedDirectoryResult::Failure;
		}
		if (canonicalChild.parent_path() != canonicalParent)
		{
			CloseHandle(heldChild);
			heldChild = INVALID_HANDLE_VALUE;
			return ManagedDirectoryResult::Unsafe;
		}
		return ManagedDirectoryResult::Success;
	}
	catch (...)
	{
		if (heldChild != INVALID_HANDLE_VALUE)
		{
			CloseHandle(heldChild);
			heldChild = INVALID_HANDLE_VALUE;
		}
		return ManagedDirectoryResult::Failure;
	}
}

bool SafeRegularPathOrMissing(const std::filesystem::path& path) noexcept
{
	try
	{
		std::error_code error;
		const std::filesystem::file_status status =
			std::filesystem::symlink_status(path, error);
		if (error == std::errc::no_such_file_or_directory) return true;
		if (error) return false;
		if (!std::filesystem::exists(status)) return true;
		if (std::filesystem::is_symlink(status) ||
			!std::filesystem::is_regular_file(status))
			return false;
		HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (file == INVALID_HANDLE_VALUE) return false;
		FILE_ATTRIBUTE_TAG_INFO attributes{};
		BY_HANDLE_FILE_INFORMATION information{};
		const bool safe = GetFileType(file) == FILE_TYPE_DISK &&
			GetFileInformationByHandleEx(file, FileAttributeTagInfo,
				&attributes, sizeof(attributes)) &&
			GetFileInformationByHandle(file, &information) &&
			information.nNumberOfLinks == 1 &&
			!(attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
			!(attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		return CloseHandle(file) != FALSE && safe;
	}
	catch (...)
	{
		return false;
	}
}
#endif

std::uint32_t RotateRight(std::uint32_t value, unsigned count) noexcept
{
	return (value >> count) | (value << (32u - count));
}

class Sha256
{
public:
	Sha256() noexcept
		: state_{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u,
			0xa54ff53au, 0x510e527fu, 0x9b05688cu,
			0x1f83d9abu, 0x5be0cd19u}
	{
	}

	void update(const std::uint8_t* bytes, std::size_t size) noexcept
	{
		totalBytes_ += size;
		while (size)
		{
			const std::size_t copied = (std::min)(
				size, Sha256BlockBytes - bufferedBytes_);
			std::memcpy(buffer_.data() + bufferedBytes_, bytes, copied);
			bufferedBytes_ += copied;
			bytes += copied;
			size -= copied;
			if (bufferedBytes_ == Sha256BlockBytes)
			{
				transform(buffer_.data());
				bufferedBytes_ = 0;
			}
		}
	}

	DedicatedCampaignCheckpointSha256 finish() noexcept
	{
		const std::uint64_t bitLength = totalBytes_ * 8u;
		buffer_[bufferedBytes_++] = 0x80u;
		if (bufferedBytes_ > 56)
		{
			std::fill(buffer_.begin() + bufferedBytes_, buffer_.end(), 0);
			transform(buffer_.data());
			bufferedBytes_ = 0;
		}
		std::fill(buffer_.begin() + bufferedBytes_, buffer_.begin() + 56, 0);
		for (unsigned index = 0; index < 8; ++index)
			buffer_[56 + index] = static_cast<std::uint8_t>(
				bitLength >> (56u - index * 8u));
		transform(buffer_.data());

		DedicatedCampaignCheckpointSha256 digest{};
		for (std::size_t word = 0; word < state_.size(); ++word)
			for (unsigned byte = 0; byte < 4; ++byte)
				digest[word * 4 + byte] = static_cast<std::uint8_t>(
					state_[word] >> (24u - byte * 8u));
		return digest;
	}

private:
	void transform(const std::uint8_t* block) noexcept
	{
		static constexpr std::uint32_t constants[64] = {
			0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
			0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
			0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
			0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
			0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
			0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
			0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
			0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
			0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
			0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
			0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
			0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
			0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
			0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
			0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
			0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

		std::uint32_t words[64]{};
		for (std::size_t index = 0; index < 16; ++index)
		{
			const std::size_t offset = index * 4;
			words[index] =
				(static_cast<std::uint32_t>(block[offset]) << 24) |
				(static_cast<std::uint32_t>(block[offset + 1]) << 16) |
				(static_cast<std::uint32_t>(block[offset + 2]) << 8) |
				static_cast<std::uint32_t>(block[offset + 3]);
		}
		for (std::size_t index = 16; index < 64; ++index)
		{
			const std::uint32_t first = RotateRight(words[index - 15], 7) ^
				RotateRight(words[index - 15], 18) ^
				(words[index - 15] >> 3);
			const std::uint32_t second = RotateRight(words[index - 2], 17) ^
				RotateRight(words[index - 2], 19) ^
				(words[index - 2] >> 10);
			words[index] = words[index - 16] + first + words[index - 7] + second;
		}

		std::uint32_t a = state_[0];
		std::uint32_t b = state_[1];
		std::uint32_t c = state_[2];
		std::uint32_t d = state_[3];
		std::uint32_t e = state_[4];
		std::uint32_t f = state_[5];
		std::uint32_t g = state_[6];
		std::uint32_t h = state_[7];
		for (std::size_t index = 0; index < 64; ++index)
		{
			const std::uint32_t sum1 = RotateRight(e, 6) ^
				RotateRight(e, 11) ^ RotateRight(e, 25);
			const std::uint32_t choose = (e & f) ^ (~e & g);
			const std::uint32_t temporary1 = h + sum1 + choose +
				constants[index] + words[index];
			const std::uint32_t sum0 = RotateRight(a, 2) ^
				RotateRight(a, 13) ^ RotateRight(a, 22);
			const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
			const std::uint32_t temporary2 = sum0 + majority;
			h = g;
			g = f;
			f = e;
			e = d + temporary1;
			d = c;
			c = b;
			b = a;
			a = temporary1 + temporary2;
		}
		state_[0] += a;
		state_[1] += b;
		state_[2] += c;
		state_[3] += d;
		state_[4] += e;
		state_[5] += f;
		state_[6] += g;
		state_[7] += h;
	}

	std::array<std::uint32_t, 8> state_;
	std::array<std::uint8_t, Sha256BlockBytes> buffer_{};
	std::uint64_t totalBytes_ = 0;
	std::size_t bufferedBytes_ = 0;
};

struct NativeFileIdentity
{
	std::uint64_t first = 0;
	std::uint64_t second = 0;
};

bool SameNativeFileIdentity(const NativeFileIdentity& left,
	const NativeFileIdentity& right) noexcept
{
	return left.first == right.first && left.second == right.second;
}

#ifdef _WIN32
using NativeFile = HANDLE;
using NativeDirectory = HANDLE;
const NativeFile InvalidNativeFile = INVALID_HANDLE_VALUE;
const NativeDirectory InvalidNativeDirectory = INVALID_HANDLE_VALUE;

bool MissingFileError(DWORD error) noexcept
{
	return error == ERROR_FILE_NOT_FOUND;
}

bool IsRegularNativeFile(NativeFile file) noexcept
{
	if (GetFileType(file) != FILE_TYPE_DISK) return false;
	FILE_ATTRIBUTE_TAG_INFO attributes{};
	BY_HANDLE_FILE_INFORMATION information{};
	return GetFileInformationByHandleEx(file, FileAttributeTagInfo,
		&attributes, sizeof(attributes)) &&
		GetFileInformationByHandle(file, &information) &&
		information.nNumberOfLinks == 1 &&
		!(attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
		!(attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool NativeFileSize(NativeFile file, std::uint64_t& size) noexcept
{
	LARGE_INTEGER value{};
	if (!GetFileSizeEx(file, &value) || value.QuadPart < 0) return false;
	size = static_cast<std::uint64_t>(value.QuadPart);
	return true;
}

bool CaptureNativeFileIdentity(NativeFile file,
	NativeFileIdentity& identity) noexcept
{
	FILE_ATTRIBUTE_TAG_INFO attributes{};
	BY_HANDLE_FILE_INFORMATION information{};
	if (GetFileType(file) != FILE_TYPE_DISK ||
		!GetFileInformationByHandleEx(file, FileAttributeTagInfo,
			&attributes, sizeof(attributes)) ||
		!GetFileInformationByHandle(file, &information) ||
		information.nNumberOfLinks != 1 ||
		(attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
		(attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		return false;
	identity.first = information.dwVolumeSerialNumber;
	identity.second =
		(static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) |
		information.nFileIndexLow;
	return true;
}

bool MatchesNativeFileIdentity(NativeFile file,
	const NativeFileIdentity& expected) noexcept
{
	FILE_ATTRIBUTE_TAG_INFO attributes{};
	BY_HANDLE_FILE_INFORMATION information{};
	if (GetFileType(file) != FILE_TYPE_DISK ||
		!GetFileInformationByHandleEx(file, FileAttributeTagInfo,
			&attributes, sizeof(attributes)) ||
		!GetFileInformationByHandle(file, &information) ||
		information.nNumberOfLinks > 1 ||
		(attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
		(attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		return false;
	NativeFileIdentity current;
	current.first = information.dwVolumeSerialNumber;
	current.second =
		(static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) |
		information.nFileIndexLow;
	return SameNativeFileIdentity(current, expected);
}

NativeFile OpenReadOnly(NativeDirectory,
	const std::filesystem::path& path) noexcept
{
	return CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
		nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL |
		FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
}

NativeFile OpenCheckpointReadOnly(NativeDirectory,
	const std::filesystem::path& path) noexcept
{
	// The held campaign-directory handle does not share delete access, so this
	// fixed child path cannot be redirected through a replaced parent. The file
	// itself does share delete access so a later atomic checkpoint publication
	// can replace the slot while this handle continues to name the old bytes.
	// Omitting FILE_SHARE_WRITE prevents a concurrently writable handle from
	// being introduced while this validated reader exists.
	return CreateFileW(path.c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
}

bool ReadNative(NativeFile file, std::uint8_t* bytes,
	std::size_t size) noexcept
{
	std::size_t offset = 0;
	while (offset < size)
	{
		const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(
			size - offset, (std::numeric_limits<DWORD>::max)()));
		DWORD read = 0;
		if (!ReadFile(file, bytes + offset, chunk, &read, nullptr) ||
			read == 0)
			return false;
		offset += read;
	}
	return true;
}

bool ReadNativeAt(NativeFile file, std::uint64_t fileOffset,
	std::uint8_t* bytes, std::size_t size) noexcept
{
	LARGE_INTEGER position{};
	if (fileOffset > static_cast<std::uint64_t>(
			(std::numeric_limits<LONGLONG>::max)()))
		return false;
	position.QuadPart = static_cast<LONGLONG>(fileOffset);
	if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN)) return false;
	return ReadNative(file, bytes, size);
}

bool SyncFilePath(NativeDirectory,
	const std::filesystem::path& path) noexcept
{
	NativeFile file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (file == InvalidNativeFile) return false;
	const bool success = IsRegularNativeFile(file) && FlushFileBuffers(file);
	const bool closed = CloseHandle(file) != FALSE;
	return success && closed;
}

bool SyncDirectory(NativeDirectory,
	const std::filesystem::path&) noexcept
{
	// Windows exposes no portable directory-fsync equivalent here. Staging
	// files are flushed before publication, and checkpoint targets are flushed
	// again before their manifests become authoritative.
	return true;
}

std::uint64_t ProcessId() noexcept
{
	return static_cast<std::uint64_t>(GetCurrentProcessId());
}

NativeDirectory OpenHeldDirectory(
	const std::filesystem::path& path, bool shareDelete) noexcept
{
	NativeDirectory directory = CreateFileW(path.c_str(),
		FILE_LIST_DIRECTORY | READ_CONTROL,
		FILE_SHARE_READ | FILE_SHARE_WRITE |
			(shareDelete ? FILE_SHARE_DELETE : 0), nullptr, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (directory == InvalidNativeDirectory) return InvalidNativeDirectory;
	FILE_ATTRIBUTE_TAG_INFO attributes{};
	if (GetFileType(directory) != FILE_TYPE_DISK ||
		!GetFileInformationByHandleEx(directory, FileAttributeTagInfo,
			&attributes, sizeof(attributes)) ||
		!(attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
		(attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
	{
		CloseHandle(directory);
		return InvalidNativeDirectory;
	}
	return directory;
}

bool SameNativeDirectory(NativeDirectory left, NativeDirectory right) noexcept
{
	BY_HANDLE_FILE_INFORMATION leftInformation{};
	BY_HANDLE_FILE_INFORMATION rightInformation{};
	return left != InvalidNativeDirectory && right != InvalidNativeDirectory &&
		GetFileInformationByHandle(left, &leftInformation) &&
		GetFileInformationByHandle(right, &rightInformation) &&
		leftInformation.dwVolumeSerialNumber ==
			rightInformation.dwVolumeSerialNumber &&
		leftInformation.nFileIndexHigh == rightInformation.nFileIndexHigh &&
		leftInformation.nFileIndexLow == rightInformation.nFileIndexLow;
}

bool HeldDirectoryState(NativeDirectory held,
	const std::filesystem::path& path, bool& empty) noexcept
{
	const NativeDirectory current = OpenHeldDirectory(path);
	if (current == InvalidNativeDirectory) return false;
	const bool same = SameNativeDirectory(held, current);
	const bool closed = CloseHandle(current) != FALSE;
	if (!same || !closed) return false;
	try
	{
		std::error_code error;
		const std::filesystem::directory_iterator iterator(path, error);
		if (error) return false;
		empty = iterator == std::filesystem::directory_iterator();
		return true;
	}
	catch (...)
	{
		return false;
	}
}
#else
using NativeFile = int;
using NativeDirectory = int;
constexpr NativeFile InvalidNativeFile = -1;
constexpr NativeDirectory InvalidNativeDirectory = -1;

bool MissingFileError(int error) noexcept
{
	return error == ENOENT;
}

bool IsRegularNativeFile(NativeFile file) noexcept
{
	struct stat status{};
	return ::fstat(file, &status) == 0 && S_ISREG(status.st_mode) &&
		status.st_nlink == 1;
}

bool NativeFileSize(NativeFile file, std::uint64_t& size) noexcept
{
	struct stat status{};
	if (::fstat(file, &status) != 0 || status.st_size < 0) return false;
	size = static_cast<std::uint64_t>(status.st_size);
	return true;
}

bool CaptureNativeFileIdentity(NativeFile file,
	NativeFileIdentity& identity) noexcept
{
	struct stat status{};
	if (::fstat(file, &status) != 0 || !S_ISREG(status.st_mode) ||
		status.st_nlink != 1)
		return false;
	identity.first = static_cast<std::uint64_t>(status.st_dev);
	identity.second = static_cast<std::uint64_t>(status.st_ino);
	return true;
}

bool MatchesNativeFileIdentity(NativeFile file,
	const NativeFileIdentity& expected) noexcept
{
	struct stat status{};
	if (::fstat(file, &status) != 0 || !S_ISREG(status.st_mode) ||
		status.st_nlink > 1)
		return false;
	NativeFileIdentity current;
	current.first = static_cast<std::uint64_t>(status.st_dev);
	current.second = static_cast<std::uint64_t>(status.st_ino);
	return SameNativeFileIdentity(current, expected);
}

NativeFile OpenReadOnly(NativeDirectory directory,
	const std::filesystem::path& path) noexcept
{
	int flags = O_RDONLY;
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
	flags |= O_NOFOLLOW;
#endif
	return ::openat(directory, path.filename().c_str(), flags);
}

NativeFile OpenCheckpointReadOnly(NativeDirectory directory,
	const std::filesystem::path& path) noexcept
{
	// The state root is same-UID private and the process lease serializes the
	// trusted publisher. That publisher only atomically replaces slot entries;
	// it never rewrites an active file in place. Arbitrary same-UID mutation is
	// outside this filesystem lease's threat model.
	return OpenReadOnly(directory, path);
}

NativeFile OpenReadWrite(NativeDirectory directory,
	const std::filesystem::path& path) noexcept
{
	int flags = O_RDWR;
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
	flags |= O_NOFOLLOW;
#endif
	return ::openat(directory, path.filename().c_str(), flags);
}

bool ReadNative(NativeFile file, std::uint8_t* bytes,
	std::size_t size) noexcept
{
	std::size_t offset = 0;
	while (offset < size)
	{
		const ssize_t read = ::read(file, bytes + offset, size - offset);
		if (read < 0 && errno == EINTR) continue;
		if (read <= 0) return false;
		offset += static_cast<std::size_t>(read);
	}
	return true;
}

bool ReadNativeAt(NativeFile file, std::uint64_t fileOffset,
	std::uint8_t* bytes, std::size_t size) noexcept
{
	if (fileOffset > static_cast<std::uint64_t>(
			(std::numeric_limits<off_t>::max)()))
		return false;
	std::size_t copied = 0;
	while (copied < size)
	{
		const std::uint64_t nextOffset = fileOffset + copied;
		if (nextOffset > static_cast<std::uint64_t>(
				(std::numeric_limits<off_t>::max)()))
			return false;
		const ssize_t read = ::pread(file, bytes + copied, size - copied,
			static_cast<off_t>(nextOffset));
		if (read < 0 && errno == EINTR) continue;
		if (read <= 0) return false;
		copied += static_cast<std::size_t>(read);
	}
	return true;
}

bool SyncDescriptor(NativeFile file) noexcept
{
#if defined(__APPLE__) && defined(F_FULLFSYNC)
	if (::fcntl(file, F_FULLFSYNC) == 0) return true;
	if (errno != EINVAL && errno != ENOTSUP && errno != ENOSYS)
		return false;
#endif
	int result;
	do { result = ::fsync(file); } while (result != 0 && errno == EINTR);
	return result == 0;
}

bool SyncFilePath(NativeDirectory directory,
	const std::filesystem::path& path) noexcept
{
	// POSIX permits fsync() to reject a descriptor not opened for writing.
	const NativeFile file = OpenReadWrite(directory, path);
	if (file == InvalidNativeFile) return false;
	const bool success = IsRegularNativeFile(file) && SyncDescriptor(file);
	const bool closed = ::close(file) == 0;
	return success && closed;
}

bool SyncDirectory(NativeDirectory directory,
	const std::filesystem::path&) noexcept
{
	return SyncDescriptor(directory);
}

std::uint64_t ProcessId() noexcept
{
	return static_cast<std::uint64_t>(::getpid());
}

NativeDirectory OpenPrivateRootDirectory(
	const std::filesystem::path& path) noexcept
{
	int flags = O_RDONLY;
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
#ifdef O_DIRECTORY
	flags |= O_DIRECTORY;
#endif
#ifdef O_NOFOLLOW
	flags |= O_NOFOLLOW;
#endif
	const int directory = ::open(path.c_str(), flags);
	if (directory < 0) return InvalidNativeDirectory;
	struct stat status{};
	if (::fstat(directory, &status) != 0 || !S_ISDIR(status.st_mode) ||
		status.st_uid != ::geteuid() || (status.st_mode & 0077) != 0)
	{
		::close(directory);
		return InvalidNativeDirectory;
	}
	return directory;
}

NativeDirectory OpenOrCreateManagedDirectoryAt(NativeDirectory parent,
	const char* name, bool& unsafe) noexcept
{
	unsafe = false;
	if (::mkdirat(parent, name, S_IRWXU) != 0 && errno != EEXIST)
		return InvalidNativeDirectory;

	struct stat entry{};
	if (::fstatat(parent, name, &entry, AT_SYMLINK_NOFOLLOW) != 0 ||
		!S_ISDIR(entry.st_mode) || entry.st_uid != ::geteuid())
	{
		unsafe = true;
		return InvalidNativeDirectory;
	}
	int flags = O_RDONLY;
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
#ifdef O_DIRECTORY
	flags |= O_DIRECTORY;
#endif
#ifdef O_NOFOLLOW
	flags |= O_NOFOLLOW;
#endif
	const int directory = ::openat(parent, name, flags);
	if (directory < 0)
	{
		unsafe = errno == ELOOP || errno == ENOTDIR;
		return InvalidNativeDirectory;
	}
	struct stat opened{};
	if (::fstat(directory, &opened) != 0 || !S_ISDIR(opened.st_mode) ||
		opened.st_uid != ::geteuid() ||
		opened.st_dev != entry.st_dev || opened.st_ino != entry.st_ino ||
		::fchmod(directory, S_IRWXU) != 0 || !SyncDescriptor(parent))
	{
		::close(directory);
		return InvalidNativeDirectory;
	}
	return directory;
}

bool SameNativeDirectory(NativeDirectory left, NativeDirectory right) noexcept
{
	struct stat leftStatus{};
	struct stat rightStatus{};
	return left != InvalidNativeDirectory && right != InvalidNativeDirectory &&
		::fstat(left, &leftStatus) == 0 &&
		::fstat(right, &rightStatus) == 0 &&
		S_ISDIR(leftStatus.st_mode) && S_ISDIR(rightStatus.st_mode) &&
		leftStatus.st_dev == rightStatus.st_dev &&
		leftStatus.st_ino == rightStatus.st_ino;
}

bool HeldDirectoryState(NativeDirectory held,
	const std::filesystem::path& path, bool& empty) noexcept
{
	int flags = O_RDONLY;
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
#ifdef O_DIRECTORY
	flags |= O_DIRECTORY;
#endif
#ifdef O_NOFOLLOW
	flags |= O_NOFOLLOW;
#endif
	const NativeDirectory current = ::open(path.c_str(), flags);
	if (current == InvalidNativeDirectory) return false;
	const bool same = SameNativeDirectory(held, current);
	const bool currentClosed = ::close(current) == 0;
	if (!same || !currentClosed) return false;

	const NativeDirectory view = ::openat(held, ".", flags);
	if (view == InvalidNativeDirectory) return false;
	DIR* directory = ::fdopendir(view);
	if (!directory)
	{
		(void)::close(view);
		return false;
	}
	empty = true;
	errno = 0;
	while (const dirent* entry = ::readdir(directory))
	{
		if (std::strcmp(entry->d_name, ".") != 0 &&
			std::strcmp(entry->d_name, "..") != 0)
		{
			empty = false;
			break;
		}
	}
	const int readError = errno;
	const bool closed = ::closedir(directory) == 0;
	return readError == 0 && closed;
}
#endif

bool SafeRegularEntryOrMissing(NativeDirectory directory,
	const std::filesystem::path& path) noexcept
{
#ifdef _WIN32
	(void)directory;
	return SafeRegularPathOrMissing(path);
#else
	struct stat status{};
	if (::fstatat(directory, path.filename().c_str(), &status,
		AT_SYMLINK_NOFOLLOW) == 0)
		return S_ISREG(status.st_mode) && status.st_nlink == 1;
	return errno == ENOENT;
#endif
}

bool ReserveEmptyFile(NativeDirectory directory,
	const std::filesystem::path& path) noexcept
{
#ifdef _WIN32
	(void)directory;
	NativeFile file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
		0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL |
		FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (file == InvalidNativeFile) return false;
	const bool valid = IsRegularNativeFile(file);
	const bool closed = CloseHandle(file) != FALSE;
	if (!valid || !closed) (void)DeleteFileW(path.c_str());
	return valid && closed;
#else
	int flags = O_RDWR | O_CREAT | O_EXCL;
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
	flags |= O_NOFOLLOW;
#endif
	const int file = ::openat(directory, path.filename().c_str(), flags,
		S_IRUSR | S_IWUSR);
	if (file < 0) return false;
	const bool valid = IsRegularNativeFile(file);
	const bool closed = ::close(file) == 0;
	if (!valid || !closed)
		(void)::unlinkat(directory, path.filename().c_str(), 0);
	return valid && closed;
#endif
}

void RemoveFileEntry(NativeDirectory directory,
	const std::filesystem::path& path) noexcept
{
#ifdef _WIN32
	(void)directory;
	(void)DeleteFileW(path.c_str());
#else
	(void)::unlinkat(directory, path.filename().c_str(), 0);
#endif
}

bool ReplaceCheckpointFile(NativeDirectory directory,
	const std::filesystem::path& directoryPath,
	const std::filesystem::path& staging,
	const std::filesystem::path& target) noexcept
{
#ifdef _WIN32
	(void)directory;
	(void)directoryPath;
	// ReplaceFileW is specifically able to replace an existing path while a
	// reader that shared delete access keeps an immutable handle to the old
	// bytes. MoveFileExW cannot provide that contract on Windows. ReplaceFileW
	// requires the target to exist, so use the no-replace move only for a slot's
	// first publication. Any other replacement failure stays fail-closed: the
	// caller will not publish a manifest for these checkpoint bytes.
	if (ReplaceFileW(target.c_str(), staging.c_str(), nullptr, 0,
			nullptr, nullptr) != FALSE)
		return true;
	const DWORD error = GetLastError();
	if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
		return false;
	return MoveFileExW(staging.c_str(), target.c_str(),
		MOVEFILE_WRITE_THROUGH) != FALSE;
#else
	if (::renameat(directory, staging.filename().c_str(),
		directory, target.filename().c_str()) != 0)
		return false;
	return SyncDirectory(directory, directoryPath);
#endif
}

DedicatedCampaignBackendResult ReadManifestFile(
	NativeDirectory directory,
	const std::filesystem::path& path,
	DedicatedCampaignManifestRead& manifest) noexcept
{
	manifest = {};
	const NativeFile file = OpenReadOnly(directory, path);
	if (file == InvalidNativeFile)
	{
#ifdef _WIN32
		return MissingFileError(GetLastError())
#else
		return MissingFileError(errno)
#endif
			? DedicatedCampaignBackendResult::Missing
			: DedicatedCampaignBackendResult::Failure;
	}

	std::uint64_t size = 0;
	bool success = IsRegularNativeFile(file) && NativeFileSize(file, size);
	if (success && size > DedicatedCampaignManifestWireSize)
	{
		manifest.size = DedicatedCampaignManifestWireSize + 1;
	}
	else if (success)
	{
		manifest.size = static_cast<std::size_t>(size);
		success = ReadNative(file, manifest.bytes.data(), manifest.size);
		std::uint64_t confirmed = 0;
		success = success && NativeFileSize(file, confirmed) && confirmed == size;
	}
#ifdef _WIN32
	const bool closed = CloseHandle(file) != FALSE;
#else
	const bool closed = ::close(file) == 0;
#endif
	return success && closed
		? DedicatedCampaignBackendResult::Present
		: DedicatedCampaignBackendResult::Failure;
}

DedicatedCampaignBackendResult ProbeCheckpointFile(
	NativeDirectory directory,
	const std::filesystem::path& path,
	DedicatedCampaignCheckpointProbe& probe) noexcept
{
	probe = {};
	const NativeFile file = OpenReadOnly(directory, path);
	if (file == InvalidNativeFile)
	{
#ifdef _WIN32
		return MissingFileError(GetLastError())
#else
		return MissingFileError(errno)
#endif
			? DedicatedCampaignBackendResult::Missing
			: DedicatedCampaignBackendResult::Failure;
	}

	std::uint64_t size = 0;
	bool success = IsRegularNativeFile(file) && NativeFileSize(file, size);
	if (success &&
		(size == 0 || size > DedicatedCampaignMaximumCheckpointBytes))
	{
#ifdef _WIN32
		const bool closed = CloseHandle(file) != FALSE;
#else
		const bool closed = ::close(file) == 0;
#endif
		if (!closed) return DedicatedCampaignBackendResult::Failure;
		// These are readable, safe regular files but cannot match a canonical
		// manifest. Surface their size without hashing so resume can reject the
		// newer pair semantically and fall back to an older valid generation.
		probe.size = size;
		return DedicatedCampaignBackendResult::Present;
	}
	Sha256 hasher;
	std::array<std::uint8_t, 64u * 1024u> buffer{};
	std::uint64_t remaining = size;
	while (success && remaining)
	{
		const std::size_t chunk = static_cast<std::size_t>(
			std::min<std::uint64_t>(remaining, buffer.size()));
		if (!ReadNative(file, buffer.data(), chunk))
		{
			success = false;
			break;
		}
		hasher.update(buffer.data(), chunk);
		remaining -= chunk;
	}
	std::uint64_t confirmed = 0;
	success = success && NativeFileSize(file, confirmed) && confirmed == size;
#ifdef _WIN32
	const bool closed = CloseHandle(file) != FALSE;
#else
	const bool closed = ::close(file) == 0;
#endif
	if (!success || !closed) return DedicatedCampaignBackendResult::Failure;
	probe.size = size;
	probe.checkpointSha256 = hasher.finish();
	return DedicatedCampaignBackendResult::Present;
}

DedicatedCampaignStoreBackend::ManifestPublishResult WriteManifestAtomically(
	NativeDirectory nativeDirectory,
	const std::filesystem::path& directory,
	const std::filesystem::path& target,
	const DedicatedCampaignManifestBytes& bytes) noexcept
{
	using PublishResult = DedicatedCampaignStoreBackend::ManifestPublishResult;
	if (!SafeRegularEntryOrMissing(nativeDirectory, target))
		return PublishResult::NotPublished;
#ifdef _WIN32
	DedicatedCampaignManifestRead previousManifest;
	const DedicatedCampaignBackendResult previousResult =
		ReadManifestFile(nativeDirectory, target, previousManifest);
	if (previousResult == DedicatedCampaignBackendResult::Failure)
		return PublishResult::NotPublished;
	const auto sameRead = [](DedicatedCampaignBackendResult leftResult,
		const DedicatedCampaignManifestRead& left,
		DedicatedCampaignBackendResult rightResult,
		const DedicatedCampaignManifestRead& right) noexcept {
		if (leftResult != rightResult) return false;
		if (leftResult == DedicatedCampaignBackendResult::Missing) return true;
		return leftResult == DedicatedCampaignBackendResult::Present &&
			left.size <= DedicatedCampaignManifestWireSize &&
			left.size == right.size && std::equal(left.bytes.begin(),
				left.bytes.end(), right.bytes.begin());
	};
#endif
	try
	{
		for (unsigned attempt = 0; attempt < 256; ++attempt)
		{
			const std::uint64_t sequence =
				TemporarySequence.fetch_add(1, std::memory_order_relaxed);
			const std::filesystem::path temporary = directory /
				(target.filename().string() + ".tmp." +
				 std::to_string(ProcessId()) + "." +
				 std::to_string(sequence));
#ifdef _WIN32
			NativeFile file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0,
				nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL |
				FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
			if (file == InvalidNativeFile)
			{
				const DWORD error = GetLastError();
				if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
					continue;
				return PublishResult::NotPublished;
			}
			DWORD written = 0;
			const bool complete = IsRegularNativeFile(file) &&
				WriteFile(file, bytes.data(),
					static_cast<DWORD>(bytes.size()), &written, nullptr) &&
				written == bytes.size() && FlushFileBuffers(file);
			const bool closed = CloseHandle(file) != FALSE;
			if (!complete || !closed)
			{
				DeleteFileW(temporary.c_str());
				return PublishResult::NotPublished;
			}
				if (!MoveFileExW(temporary.c_str(), target.c_str(),
					MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				{
					DeleteFileW(temporary.c_str());
					DedicatedCampaignManifestRead visibleManifest;
					const DedicatedCampaignBackendResult visibleResult =
						ReadManifestFile(nativeDirectory, target, visibleManifest);
					const bool visibleIntended =
						visibleResult == DedicatedCampaignBackendResult::Present &&
						visibleManifest.size == bytes.size() &&
						std::equal(visibleManifest.bytes.begin(),
							visibleManifest.bytes.end(), bytes.begin());
					if (visibleIntended)
						return PublishResult::PublishedDurabilityUnknown;
					return sameRead(visibleResult, visibleManifest,
						previousResult, previousManifest)
						? PublishResult::NotPublished
						: PublishResult::PublicationStateUnknown;
				}
#else
			int flags = O_WRONLY | O_CREAT | O_EXCL;
#ifdef O_CLOEXEC
			flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
			flags |= O_NOFOLLOW;
#endif
			const std::string temporaryName = temporary.filename().string();
			const std::string targetName = target.filename().string();
			const int file = ::openat(nativeDirectory, temporaryName.c_str(), flags,
				S_IRUSR | S_IWUSR);
			if (file < 0)
			{
				if (errno == EEXIST) continue;
				return PublishResult::NotPublished;
			}
			std::size_t offset = 0;
			bool complete = IsRegularNativeFile(file);
			while (complete && offset < bytes.size())
			{
				const ssize_t written = ::write(file, bytes.data() + offset,
					bytes.size() - offset);
				if (written < 0 && errno == EINTR) continue;
				if (written <= 0) complete = false;
				else offset += static_cast<std::size_t>(written);
			}
			complete = complete && SyncDescriptor(file);
			const bool closed = ::close(file) == 0;
			if (!complete || !closed)
			{
				::unlinkat(nativeDirectory, temporaryName.c_str(), 0);
				return PublishResult::NotPublished;
			}
			if (::renameat(nativeDirectory, temporaryName.c_str(),
				nativeDirectory, targetName.c_str()) != 0)
			{
				::unlinkat(nativeDirectory, temporaryName.c_str(), 0);
				return PublishResult::NotPublished;
			}
#endif
			// Publication is already visible here, so a directory-sync failure
			// cannot be reported as false without making the store retain the old
			// in-memory slot while readers see the new manifest.
			return SyncDirectory(nativeDirectory, directory)
				? PublishResult::PublishedDurable
				: PublishResult::PublishedDurabilityUnknown;
		}
	}
	catch (...)
	{
		return PublishResult::NotPublished;
	}
	return PublishResult::NotPublished;
}
}

struct DedicatedCampaignCheckpointReader::Impl
{
	NativeFile file = InvalidNativeFile;
	NativeFileIdentity identity;
	DedicatedCampaignSlot slot = DedicatedCampaignSlot::A;
	std::uint64_t generation = 0;
	std::uint64_t worldMinutes = 0;
	std::uint64_t size = 0;
	DedicatedCampaignCheckpointSha256 checkpointSha256{};
	std::array<std::uint8_t,
		DedicatedCampaignCheckpointMaximumReadBytes> scratch{};

	~Impl() noexcept
	{
#ifdef _WIN32
		if (file != InvalidNativeFile) (void)CloseHandle(file);
#else
		if (file != InvalidNativeFile) (void)::close(file);
#endif
	}
};

DedicatedCampaignCheckpointReader::DedicatedCampaignCheckpointReader() noexcept =
	default;

DedicatedCampaignCheckpointReader::~DedicatedCampaignCheckpointReader() noexcept =
	default;

DedicatedCampaignCheckpointReader::DedicatedCampaignCheckpointReader(
	DedicatedCampaignCheckpointReader&&) noexcept = default;

DedicatedCampaignCheckpointReader&
DedicatedCampaignCheckpointReader::operator=(
	DedicatedCampaignCheckpointReader&&) noexcept = default;

bool DedicatedCampaignCheckpointReader::isOpen() const noexcept
{
	return impl_ && impl_->file != InvalidNativeFile;
}

DedicatedCampaignSlot DedicatedCampaignCheckpointReader::slot() const noexcept
{
	return isOpen() ? impl_->slot : DedicatedCampaignSlot::A;
}

std::uint64_t DedicatedCampaignCheckpointReader::generation() const noexcept
{
	return isOpen() ? impl_->generation : 0;
}

std::uint64_t DedicatedCampaignCheckpointReader::worldMinutes() const noexcept
{
	return isOpen() ? impl_->worldMinutes : 0;
}

std::uint64_t DedicatedCampaignCheckpointReader::size() const noexcept
{
	return isOpen() ? impl_->size : 0;
}

const DedicatedCampaignCheckpointSha256&
DedicatedCampaignCheckpointReader::checkpointSha256() const noexcept
{
	static const DedicatedCampaignCheckpointSha256 empty{};
	return isOpen() ? impl_->checkpointSha256 : empty;
}

bool DedicatedCampaignCheckpointReader::readExact(std::uint64_t offset,
	std::uint8_t* bytes, std::size_t requestedSize) noexcept
{
	if (!isOpen() ||
		requestedSize > DedicatedCampaignCheckpointMaximumReadBytes ||
		(requestedSize != 0 && bytes == nullptr) || offset > impl_->size ||
		static_cast<std::uint64_t>(requestedSize) > impl_->size - offset)
		return false;

	const auto currentHandleMatches = [this]() noexcept {
		std::uint64_t currentSize = 0;
		return MatchesNativeFileIdentity(impl_->file, impl_->identity) &&
			NativeFileSize(impl_->file, currentSize) &&
			currentSize == impl_->size;
	};
	if (!currentHandleMatches()) return false;
	if (requestedSize == 0) return true;
	if (!ReadNativeAt(impl_->file, offset, impl_->scratch.data(),
			requestedSize))
		return false;
	// A truncation or identity anomaly after the native read must fail before
	// even one caller byte is changed. Atomic path replacement is still valid:
	// an unlinked held file keeps the same identity and may have link count zero.
	if (!currentHandleMatches()) return false;
	std::memcpy(bytes, impl_->scratch.data(), requestedSize);
	return true;
}

struct DedicatedCampaignFilesystemBackend::Impl
{
	std::filesystem::path stateRoot;
	std::filesystem::path campaignDirectory;
	std::filesystem::path profileDirectory;
	std::string campaignKey;
	std::array<std::filesystem::path, 2> checkpointPaths;
	std::array<std::filesystem::path, 2> manifestPaths;
	std::filesystem::path lockPath;
	bool open = false;
#ifdef _WIN32
	HANDLE lock = INVALID_HANDLE_VALUE;
	HANDLE rootDirectory = INVALID_HANDLE_VALUE;
	HANDLE campaignsDirectory = INVALID_HANDLE_VALUE;
	HANDLE campaignDirectoryHandle = INVALID_HANDLE_VALUE;
	HANDLE profileDirectoryHandle = INVALID_HANDLE_VALUE;
#else
	int lock = -1;
	int rootDirectory = -1;
	int campaignsDirectory = -1;
	int campaignDirectoryHandle = -1;
	int profileDirectoryHandle = -1;
#endif

	~Impl() noexcept
	{
#ifdef _WIN32
		if (profileDirectoryHandle != INVALID_HANDLE_VALUE)
			(void)CloseHandle(profileDirectoryHandle);
		if (campaignDirectoryHandle != INVALID_HANDLE_VALUE)
			(void)CloseHandle(campaignDirectoryHandle);
		if (campaignsDirectory != INVALID_HANDLE_VALUE)
			(void)CloseHandle(campaignsDirectory);
		if (rootDirectory != INVALID_HANDLE_VALUE)
			(void)CloseHandle(rootDirectory);
		if (lock != INVALID_HANDLE_VALUE)
		{
			OVERLAPPED overlapped{};
			(void)UnlockFileEx(lock, 0, 1, 0, &overlapped);
			(void)CloseHandle(lock);
		}
#else
		if (profileDirectoryHandle >= 0)
			(void)::close(profileDirectoryHandle);
		if (campaignDirectoryHandle >= 0)
			(void)::close(campaignDirectoryHandle);
		if (campaignsDirectory >= 0)
			(void)::close(campaignsDirectory);
		if (rootDirectory >= 0)
			(void)::close(rootDirectory);
		if (lock >= 0)
		{
			(void)::flock(lock, LOCK_UN);
			(void)::close(lock);
		}
#endif
	}
};

DedicatedCampaignFilesystemBackend::DedicatedCampaignFilesystemBackend() noexcept =
	default;

DedicatedCampaignFilesystemBackend::DedicatedCampaignFilesystemBackend(
	DedicatedCampaignCheckpointWriter& writer) noexcept
	: writer_(&writer)
{
}

DedicatedCampaignFilesystemBackend::~DedicatedCampaignFilesystemBackend() noexcept
{
	close();
}

DedicatedCampaignFilesystemError DedicatedCampaignFilesystemBackend::open(
	const std::filesystem::path& requestedRoot,
	const std::string& campaignId) noexcept
{
	if (isOpen()) return DedicatedCampaignFilesystemError::AlreadyOpen;
	if (requestedRoot.empty())
		return DedicatedCampaignFilesystemError::InvalidStateRoot;
	if (!PortableCampaignId(campaignId))
		return DedicatedCampaignFilesystemError::InvalidCampaignId;

	try
	{
		std::unique_ptr<Impl> opened(new Impl);
		std::error_code error;
		if (!requestedRoot.is_absolute())
			return DedicatedCampaignFilesystemError::InvalidStateRoot;
		const std::filesystem::file_status rootLinkStatus =
			std::filesystem::symlink_status(requestedRoot, error);
		if (error || std::filesystem::is_symlink(rootLinkStatus) ||
			!std::filesystem::is_directory(rootLinkStatus))
			return DedicatedCampaignFilesystemError::InvalidStateRoot;
#ifdef _WIN32
			// Open the exact operator-selected leaf before canonicalization so a
			// junction or other non-symlink reparse tag cannot resolve away first.
			opened->rootDirectory = OpenHeldDirectory(requestedRoot);
#else
			opened->stateRoot = std::filesystem::canonical(requestedRoot, error);
			if (error || opened->stateRoot.empty())
				return DedicatedCampaignFilesystemError::InvalidStateRoot;
			opened->rootDirectory = OpenPrivateRootDirectory(opened->stateRoot);
#endif
			if (opened->rootDirectory == InvalidNativeDirectory)
				return DedicatedCampaignFilesystemError::UnsafeManagedPath;
#ifdef _WIN32
			opened->stateRoot = std::filesystem::canonical(requestedRoot, error);
			if (error || opened->stateRoot.empty())
				return DedicatedCampaignFilesystemError::InvalidStateRoot;
#endif
		opened->lockPath = opened->stateRoot / "process.lock";

#ifdef _WIN32
		opened->lock = CreateFileW(opened->lockPath.c_str(),
			GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL |
			FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (opened->lock == INVALID_HANDLE_VALUE)
			return DedicatedCampaignFilesystemError::LockFailure;
		if (!IsRegularNativeFile(opened->lock))
		{
			CloseHandle(opened->lock);
			opened->lock = INVALID_HANDLE_VALUE;
			return DedicatedCampaignFilesystemError::UnsafeManagedPath;
		}
		OVERLAPPED overlapped{};
		if (!LockFileEx(opened->lock,
			LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
			0, 1, 0, &overlapped))
		{
			const DWORD lockError = GetLastError();
			CloseHandle(opened->lock);
			opened->lock = INVALID_HANDLE_VALUE;
			return lockError == ERROR_LOCK_VIOLATION ||
				lockError == ERROR_SHARING_VIOLATION
				? DedicatedCampaignFilesystemError::LockHeld
				: DedicatedCampaignFilesystemError::LockFailure;
		}
#else
		int flags = O_RDWR | O_CREAT;
#ifdef O_CLOEXEC
		flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
		flags |= O_NOFOLLOW;
#endif
		opened->lock = ::openat(opened->rootDirectory, "process.lock", flags,
			S_IRUSR | S_IWUSR);
		if (opened->lock < 0)
			return errno == ELOOP
				? DedicatedCampaignFilesystemError::UnsafeManagedPath
				: DedicatedCampaignFilesystemError::LockFailure;
		if (!IsRegularNativeFile(opened->lock))
		{
			::close(opened->lock);
			opened->lock = -1;
			return DedicatedCampaignFilesystemError::UnsafeManagedPath;
		}
		int lockResult;
		do { lockResult = ::flock(opened->lock, LOCK_EX | LOCK_NB); }
		while (lockResult != 0 && errno == EINTR);
		if (lockResult != 0)
		{
			const int lockError = errno;
			::close(opened->lock);
			opened->lock = -1;
			return lockError == EWOULDBLOCK || lockError == EAGAIN
				? DedicatedCampaignFilesystemError::LockHeld
				: DedicatedCampaignFilesystemError::LockFailure;
		}
#endif

		std::filesystem::path campaignsDirectory;
#ifdef _WIN32
			const ManagedDirectoryResult campaigns = PrepareManagedDirectory(
				opened->stateRoot, opened->stateRoot / "campaigns",
				campaignsDirectory, opened->campaignsDirectory);
		if (campaigns == ManagedDirectoryResult::Unsafe)
			return DedicatedCampaignFilesystemError::UnsafeManagedPath;
		if (campaigns != ManagedDirectoryResult::Success)
			return DedicatedCampaignFilesystemError::DirectoryFailure;
#else
		bool unsafeManagedDirectory = false;
		opened->campaignsDirectory = OpenOrCreateManagedDirectoryAt(
			opened->rootDirectory, "campaigns", unsafeManagedDirectory);
		if (opened->campaignsDirectory == InvalidNativeDirectory)
			return unsafeManagedDirectory
				? DedicatedCampaignFilesystemError::UnsafeManagedPath
				: DedicatedCampaignFilesystemError::DirectoryFailure;
		campaignsDirectory = opened->stateRoot / "campaigns";
#endif

		const std::string campaignKey = CanonicalCampaignKey(campaignId);
		opened->campaignKey = campaignKey;
		const std::string campaignComponent = "campaign-" + campaignKey;
#ifdef _WIN32
			const ManagedDirectoryResult campaign = PrepareManagedDirectory(
				campaignsDirectory,
				campaignsDirectory / campaignComponent,
				opened->campaignDirectory,
				opened->campaignDirectoryHandle);
		if (campaign == ManagedDirectoryResult::Unsafe)
			return DedicatedCampaignFilesystemError::UnsafeManagedPath;
		if (campaign != ManagedDirectoryResult::Success)
			return DedicatedCampaignFilesystemError::DirectoryFailure;
#else
		opened->campaignDirectoryHandle = OpenOrCreateManagedDirectoryAt(
			opened->campaignsDirectory, campaignComponent.c_str(),
			unsafeManagedDirectory);
		if (opened->campaignDirectoryHandle == InvalidNativeDirectory)
			return unsafeManagedDirectory
				? DedicatedCampaignFilesystemError::UnsafeManagedPath
				: DedicatedCampaignFilesystemError::DirectoryFailure;
		opened->campaignDirectory = campaignsDirectory / campaignComponent;
#endif

#ifdef _WIN32
		const ManagedDirectoryResult profile = PrepareManagedDirectory(
			opened->campaignDirectory,
			opened->campaignDirectory / "profile",
			opened->profileDirectory,
			opened->profileDirectoryHandle, true);
		if (profile == ManagedDirectoryResult::Unsafe)
			return DedicatedCampaignFilesystemError::UnsafeManagedPath;
		if (profile != ManagedDirectoryResult::Success)
			return DedicatedCampaignFilesystemError::DirectoryFailure;
#else
		opened->profileDirectoryHandle = OpenOrCreateManagedDirectoryAt(
			opened->campaignDirectoryHandle, "profile", unsafeManagedDirectory);
		if (opened->profileDirectoryHandle == InvalidNativeDirectory)
			return unsafeManagedDirectory
				? DedicatedCampaignFilesystemError::UnsafeManagedPath
				: DedicatedCampaignFilesystemError::DirectoryFailure;
		opened->profileDirectory = opened->campaignDirectory / "profile";
#endif

		opened->checkpointPaths = {
			opened->campaignDirectory / "checkpoint-a.sav",
			opened->campaignDirectory / "checkpoint-b.sav"};
		opened->manifestPaths = {
			opened->campaignDirectory / "manifest-a.bin",
			opened->campaignDirectory / "manifest-b.bin"};
		opened->open = true;
		impl_ = std::move(opened);
		return DedicatedCampaignFilesystemError::None;
	}
	catch (...)
	{
		return DedicatedCampaignFilesystemError::DirectoryFailure;
	}
}

void DedicatedCampaignFilesystemBackend::close() noexcept
{
	if (!impl_) return;
	impl_->open = false;
	impl_.reset();
}

bool DedicatedCampaignFilesystemBackend::isOpen() const noexcept
{
	return impl_ && impl_->open;
}

const std::filesystem::path&
DedicatedCampaignFilesystemBackend::stateRoot() const noexcept
{
	return impl_ ? impl_->stateRoot : EmptyPath();
}

const std::filesystem::path&
DedicatedCampaignFilesystemBackend::campaignDirectory() const noexcept
{
	return impl_ ? impl_->campaignDirectory : EmptyPath();
}

const std::filesystem::path&
DedicatedCampaignFilesystemBackend::profileDirectory() const noexcept
{
	return impl_ ? impl_->profileDirectory : EmptyPath();
}

DedicatedCampaignProfileDirectoryState
DedicatedCampaignFilesystemBackend::profileDirectoryState() const noexcept
{
	if (!isOpen()) return DedicatedCampaignProfileDirectoryState::Failure;
	bool empty = false;
	if (!HeldDirectoryState(impl_->profileDirectoryHandle,
		impl_->profileDirectory, empty))
		return DedicatedCampaignProfileDirectoryState::Failure;
	return empty ? DedicatedCampaignProfileDirectoryState::Empty
		: DedicatedCampaignProfileDirectoryState::NonEmpty;
}

DedicatedCampaignProfileRecoveryResult
DedicatedCampaignFilesystemBackend::recoverProfileForNewCampaign() noexcept
{
	if (!isOpen() || writer_ != nullptr)
		return DedicatedCampaignProfileRecoveryResult::Failure;
	try
	{
		DedicatedCampaignManifestRead firstManifest;
		DedicatedCampaignManifestRead secondManifest;
		const DedicatedCampaignBackendResult first =
			readManifest(DedicatedCampaignSlot::A, firstManifest);
		const DedicatedCampaignBackendResult second =
			readManifest(DedicatedCampaignSlot::B, secondManifest);
		if (first == DedicatedCampaignBackendResult::Present ||
			second == DedicatedCampaignBackendResult::Present)
			return DedicatedCampaignProfileRecoveryResult::CommittedStatePresent;
		if (first != DedicatedCampaignBackendResult::Missing ||
			second != DedicatedCampaignBackendResult::Missing ||
			firstManifest.size != 0 || secondManifest.size != 0)
			return DedicatedCampaignProfileRecoveryResult::Failure;

		bool empty = false;
		if (!HeldDirectoryState(impl_->profileDirectoryHandle,
			impl_->profileDirectory, empty))
			return DedicatedCampaignProfileRecoveryResult::Failure;
		if (empty) return DedicatedCampaignProfileRecoveryResult::Ready;

#ifdef _WIN32
		const NativeDirectory oldProfile = impl_->profileDirectoryHandle;
		if (oldProfile == InvalidNativeDirectory)
			return DedicatedCampaignProfileRecoveryResult::Failure;
		std::filesystem::path orphan;
		for (unsigned attempt = 0; attempt < 256; ++attempt)
		{
			bool profileEmpty = false;
			if (!HeldDirectoryState(oldProfile, impl_->profileDirectory,
					profileEmpty) || profileEmpty)
				return DedicatedCampaignProfileRecoveryResult::Failure;
			const std::uint64_t sequence = TemporarySequence.fetch_add(
				1, std::memory_order_relaxed);
			orphan = impl_->campaignDirectory /
				("profile.orphan." + std::to_string(ProcessId()) + "." +
					std::to_string(sequence));
			if (MoveFileExW(impl_->profileDirectory.c_str(), orphan.c_str(),
				MOVEFILE_WRITE_THROUGH)) break;
			const DWORD moveError = GetLastError();
			orphan.clear();
			if (moveError != ERROR_ALREADY_EXISTS &&
				moveError != ERROR_FILE_EXISTS)
				return DedicatedCampaignProfileRecoveryResult::Failure;
		}
		if (orphan.empty())
			return DedicatedCampaignProfileRecoveryResult::Failure;
		const NativeDirectory archived = OpenHeldDirectory(orphan, true);
		if (archived == InvalidNativeDirectory ||
			!SameNativeDirectory(oldProfile, archived))
		{
			if (archived != InvalidNativeDirectory) (void)CloseHandle(archived);
			return DedicatedCampaignProfileRecoveryResult::Failure;
		}
		std::filesystem::path preparedProfile;
		NativeDirectory freshProfile = InvalidNativeDirectory;
		const ManagedDirectoryResult profile = PrepareManagedDirectory(
			impl_->campaignDirectory, impl_->profileDirectory,
			preparedProfile, freshProfile, true);
		bool freshEmpty = false;
		const bool freshValid = profile == ManagedDirectoryResult::Success &&
			preparedProfile == impl_->profileDirectory &&
			!SameNativeDirectory(oldProfile, freshProfile) &&
			HeldDirectoryState(freshProfile,
				impl_->profileDirectory, freshEmpty) && freshEmpty;
		const bool archiveClosed = CloseHandle(archived) != FALSE;
		const bool oldClosed = CloseHandle(oldProfile) != FALSE;
		if (!freshValid || !archiveClosed || !oldClosed)
		{
			if (freshProfile != InvalidNativeDirectory)
				(void)CloseHandle(freshProfile);
			impl_->profileDirectoryHandle = InvalidNativeDirectory;
			return DedicatedCampaignProfileRecoveryResult::Failure;
		}
		impl_->profileDirectoryHandle = freshProfile;
#else
		if (impl_->profileDirectoryHandle == InvalidNativeDirectory)
			return DedicatedCampaignProfileRecoveryResult::Failure;
		std::string orphanName;
		for (unsigned attempt = 0; attempt < 256; ++attempt)
		{
			bool profileEmpty = false;
			if (!HeldDirectoryState(impl_->profileDirectoryHandle,
					impl_->profileDirectory, profileEmpty) || profileEmpty)
				return DedicatedCampaignProfileRecoveryResult::Failure;
			const std::uint64_t sequence = TemporarySequence.fetch_add(
				1, std::memory_order_relaxed);
			orphanName = "profile.orphan." + std::to_string(ProcessId()) +
				"." + std::to_string(sequence);
			int renamed = -1;
#if defined(__APPLE__)
			renamed = ::renameatx_np(impl_->campaignDirectoryHandle, "profile",
				impl_->campaignDirectoryHandle, orphanName.c_str(), RENAME_EXCL);
#elif defined(__linux__) && defined(SYS_renameat2)
			renamed = static_cast<int>(::syscall(SYS_renameat2,
				impl_->campaignDirectoryHandle, "profile",
				impl_->campaignDirectoryHandle, orphanName.c_str(),
				RENAME_NOREPLACE));
			if (renamed != 0 && (errno == ENOSYS || errno == EINVAL
#ifdef EOPNOTSUPP
				|| errno == EOPNOTSUPP
#endif
				))
			{
				if (::mkdirat(impl_->campaignDirectoryHandle,
						orphanName.c_str(), S_IRWXU) == 0)
				{
					renamed = ::renameat(impl_->campaignDirectoryHandle,
						"profile", impl_->campaignDirectoryHandle,
						orphanName.c_str());
					if (renamed != 0)
					{
						const int renameError = errno;
						(void)::unlinkat(impl_->campaignDirectoryHandle,
							orphanName.c_str(), AT_REMOVEDIR);
						errno = renameError;
					}
				}
			}
#else
			if (::mkdirat(impl_->campaignDirectoryHandle, orphanName.c_str(),
					S_IRWXU) == 0)
			{
				renamed = ::renameat(impl_->campaignDirectoryHandle, "profile",
					impl_->campaignDirectoryHandle, orphanName.c_str());
				if (renamed != 0)
				{
					const int renameError = errno;
					(void)::unlinkat(impl_->campaignDirectoryHandle,
						orphanName.c_str(), AT_REMOVEDIR);
					errno = renameError;
				}
			}
#endif
			if (renamed == 0) break;
			if (errno != EEXIST)
				return DedicatedCampaignProfileRecoveryResult::Failure;
			orphanName.clear();
		}
		if (orphanName.empty())
			return DedicatedCampaignProfileRecoveryResult::Failure;
		const NativeDirectory oldProfile = impl_->profileDirectoryHandle;
		impl_->profileDirectoryHandle = InvalidNativeDirectory;
		bool archivedEmpty = true;
		const std::filesystem::path orphan =
			impl_->campaignDirectory / orphanName;
		if (!HeldDirectoryState(oldProfile, orphan, archivedEmpty) || archivedEmpty)
		{
			(void)::close(oldProfile);
			return DedicatedCampaignProfileRecoveryResult::Failure;
		}
		if (!SyncDirectory(impl_->campaignDirectoryHandle,
			impl_->campaignDirectory))
		{
			(void)::close(oldProfile);
			return DedicatedCampaignProfileRecoveryResult::Failure;
		}
		bool unsafeManagedDirectory = false;
		const NativeDirectory freshProfile = OpenOrCreateManagedDirectoryAt(
			impl_->campaignDirectoryHandle, "profile", unsafeManagedDirectory);
		bool freshEmpty = false;
		const bool freshValid = freshProfile != InvalidNativeDirectory &&
			!SameNativeDirectory(oldProfile, freshProfile) &&
			HeldDirectoryState(freshProfile, impl_->profileDirectory, freshEmpty) &&
			freshEmpty;
		const bool oldClosed = ::close(oldProfile) == 0;
		if (!freshValid || !oldClosed)
		{
			if (freshProfile != InvalidNativeDirectory)
				(void)::close(freshProfile);
			return DedicatedCampaignProfileRecoveryResult::Failure;
		}
		impl_->profileDirectoryHandle = freshProfile;
#endif
		return DedicatedCampaignProfileRecoveryResult::Ready;
	}
	catch (...)
	{
		return DedicatedCampaignProfileRecoveryResult::Failure;
	}
}

const std::filesystem::path&
DedicatedCampaignFilesystemBackend::checkpointPath(
	DedicatedCampaignSlot slot) const noexcept
{
	return impl_ && KnownSlot(slot)
		? impl_->checkpointPaths[SlotIndex(slot)] : EmptyPath();
}

const std::filesystem::path&
DedicatedCampaignFilesystemBackend::manifestPath(
	DedicatedCampaignSlot slot) const noexcept
{
	return impl_ && KnownSlot(slot)
		? impl_->manifestPaths[SlotIndex(slot)] : EmptyPath();
}

bool DedicatedCampaignFilesystemBackend::openCheckpointReader(
	const DedicatedCampaignStoreState& expectedState,
	DedicatedCampaignCheckpointReader& reader) noexcept
{
	if (!isOpen() || !expectedState.hasCheckpoint ||
		!KnownSlot(expectedState.activeSlot) || expectedState.generation == 0 ||
		expectedState.checkpointSize == 0 ||
		expectedState.checkpointSize > DedicatedCampaignMaximumCheckpointBytes ||
		std::all_of(expectedState.checkpointSha256.begin(),
			expectedState.checkpointSha256.end(),
			[](std::uint8_t value) { return value == 0; }))
		return false;

	try
	{
		std::unique_ptr<DedicatedCampaignCheckpointReader::Impl> opened(
			new DedicatedCampaignCheckpointReader::Impl);
		opened->file = OpenCheckpointReadOnly(
			impl_->campaignDirectoryHandle,
			impl_->checkpointPaths[SlotIndex(expectedState.activeSlot)]);
		if (opened->file == InvalidNativeFile) return false;

		std::uint64_t openedSize = 0;
		if (!CaptureNativeFileIdentity(opened->file, opened->identity) ||
			!NativeFileSize(opened->file, openedSize) ||
			openedSize != expectedState.checkpointSize)
			return false;

		Sha256 hasher;
		std::uint64_t remaining = openedSize;
		while (remaining)
		{
			const std::size_t chunk = static_cast<std::size_t>(
				std::min<std::uint64_t>(remaining, opened->scratch.size()));
			if (!ReadNative(opened->file, opened->scratch.data(), chunk))
				return false;
			hasher.update(opened->scratch.data(), chunk);
			remaining -= chunk;
		}
		std::uint64_t confirmedSize = 0;
		if (!MatchesNativeFileIdentity(opened->file, opened->identity) ||
			!NativeFileSize(opened->file, confirmedSize) ||
			confirmedSize != expectedState.checkpointSize ||
			hasher.finish() != expectedState.checkpointSha256)
			return false;

		opened->slot = expectedState.activeSlot;
		opened->generation = expectedState.generation;
		opened->worldMinutes = expectedState.worldMinutes;
		opened->size = expectedState.checkpointSize;
		opened->checkpointSha256 = expectedState.checkpointSha256;
		// Publish only after complete validation. Replacing an existing output is
		// deliberately the final, non-throwing operation.
		reader.impl_ = std::move(opened);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool DedicatedCampaignFilesystemBackend::checkpointWriterBound() const noexcept
{
	return writer_ != nullptr;
}

bool DedicatedCampaignFilesystemBackend::bindCheckpointWriter(
	DedicatedCampaignCheckpointWriter& writer) noexcept
{
	if (writer_ != nullptr) return false;
	writer_ = &writer;
	return true;
}

bool DedicatedCampaignFilesystemBackend::acceptsIdentity(
	const DedicatedCampaignIdentity& identity) const noexcept
{
	return isOpen() && identity.campaignId == impl_->campaignKey;
}

DedicatedCampaignBackendResult
DedicatedCampaignFilesystemBackend::readManifest(
	DedicatedCampaignSlot slot,
	DedicatedCampaignManifestRead& manifest)
{
	manifest = {};
	if (!isOpen() || !KnownSlot(slot))
		return DedicatedCampaignBackendResult::Failure;
	return ReadManifestFile(impl_->campaignDirectoryHandle,
		impl_->manifestPaths[SlotIndex(slot)], manifest);
}

bool DedicatedCampaignFilesystemBackend::writeCheckpoint(
	DedicatedCampaignSlot slot)
{
	if (!isOpen() || !KnownSlot(slot) || writer_ == nullptr) return false;
	const std::filesystem::path& target =
		impl_->checkpointPaths[SlotIndex(slot)];
	if (!SafeRegularEntryOrMissing(
		impl_->campaignDirectoryHandle, target))
		return false;
	try
	{
		for (unsigned attempt = 0; attempt < 256; ++attempt)
		{
			const std::uint64_t sequence =
				TemporarySequence.fetch_add(1, std::memory_order_relaxed);
			const std::filesystem::path staging =
				impl_->campaignDirectory /
				(target.filename().string() + ".pending." +
				 std::to_string(ProcessId()) + "." +
				 std::to_string(sequence));
			if (!ReserveEmptyFile(impl_->campaignDirectoryHandle, staging))
				continue;
			if (!writer_->writeCheckpoint(slot, staging) ||
				!SafeRegularEntryOrMissing(
					impl_->campaignDirectoryHandle, staging) ||
				!SyncFilePath(impl_->campaignDirectoryHandle, staging))
			{
				RemoveFileEntry(impl_->campaignDirectoryHandle, staging);
				return false;
			}
			DedicatedCampaignCheckpointProbe stagingProbe;
			if (ProbeCheckpointFile(impl_->campaignDirectoryHandle, staging,
					stagingProbe) != DedicatedCampaignBackendResult::Present ||
				stagingProbe.size == 0 ||
				stagingProbe.size > DedicatedCampaignMaximumCheckpointBytes)
			{
				RemoveFileEntry(impl_->campaignDirectoryHandle, staging);
				return false;
			}
			if (!ReplaceCheckpointFile(impl_->campaignDirectoryHandle,
				impl_->campaignDirectory, staging, target))
			{
				RemoveFileEntry(impl_->campaignDirectoryHandle, staging);
				return false;
			}
			return true;
		}
	}
	catch (...)
	{
		return false;
	}
	return false;
}

DedicatedCampaignBackendResult
DedicatedCampaignFilesystemBackend::probeCheckpoint(
	DedicatedCampaignSlot slot,
	DedicatedCampaignCheckpointProbe& probe)
{
	probe = {};
	if (!isOpen() || !KnownSlot(slot))
		return DedicatedCampaignBackendResult::Failure;
	return ProbeCheckpointFile(impl_->campaignDirectoryHandle,
		impl_->checkpointPaths[SlotIndex(slot)], probe);
}

bool DedicatedCampaignFilesystemBackend::syncCheckpoint(
	DedicatedCampaignSlot slot)
{
	if (!isOpen() || !KnownSlot(slot)) return false;
	return SyncFilePath(impl_->campaignDirectoryHandle,
			impl_->checkpointPaths[SlotIndex(slot)]) &&
		SyncDirectory(impl_->campaignDirectoryHandle,
			impl_->campaignDirectory);
}

DedicatedCampaignStoreBackend::ManifestPublishResult
DedicatedCampaignFilesystemBackend::publishManifest(
	DedicatedCampaignSlot slot,
	const DedicatedCampaignManifestBytes& bytes)
{
	if (!isOpen() || !KnownSlot(slot))
		return ManifestPublishResult::NotPublished;
	return WriteManifestAtomically(impl_->campaignDirectoryHandle,
		impl_->campaignDirectory,
		impl_->manifestPaths[SlotIndex(slot)], bytes);
}

const char* DedicatedCampaignFilesystemErrorName(
	DedicatedCampaignFilesystemError error) noexcept
{
	switch (error)
	{
		case DedicatedCampaignFilesystemError::None: return "none";
		case DedicatedCampaignFilesystemError::AlreadyOpen: return "already open";
		case DedicatedCampaignFilesystemError::InvalidStateRoot: return "invalid state root";
		case DedicatedCampaignFilesystemError::InvalidCampaignId: return "invalid campaign id";
		case DedicatedCampaignFilesystemError::UnsafeManagedPath: return "unsafe managed path";
		case DedicatedCampaignFilesystemError::DirectoryFailure: return "directory failure";
		case DedicatedCampaignFilesystemError::LockHeld: return "campaign lock held";
		case DedicatedCampaignFilesystemError::LockFailure: return "campaign lock failure";
	}
	return "unknown campaign filesystem error";
}
