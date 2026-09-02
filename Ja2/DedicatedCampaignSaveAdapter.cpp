#include "DedicatedCampaignSaveAdapter.h"

#include "DedicatedCampaignSaveBridge.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
constexpr std::size_t CopyBufferBytes = 64u * 1024u;
constexpr std::size_t Sha256BlockBytes = 64;

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
			const std::size_t copied = std::min(
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

bool KnownSlot(DedicatedCampaignSlot slot) noexcept
{
	return slot == DedicatedCampaignSlot::A ||
		slot == DedicatedCampaignSlot::B;
}

const char* StagingPrefix(DedicatedCampaignSlot slot) noexcept
{
	return slot == DedicatedCampaignSlot::A
		? "checkpoint-a.sav.pending." : "checkpoint-b.sav.pending.";
}

enum class NativeEntrySafety : std::uint8_t
{
	Missing,
	SafeRegularFile,
	Unsafe
};

NativeEntrySafety InspectNativeEntry(
	const std::filesystem::path& path) noexcept
{
#ifdef _WIN32
	const HANDLE file = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING,
		FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return GetLastError() == ERROR_FILE_NOT_FOUND
			? NativeEntrySafety::Missing : NativeEntrySafety::Unsafe;
	BY_HANDLE_FILE_INFORMATION information{};
	const bool read = GetFileInformationByHandle(file, &information) != FALSE;
	(void)CloseHandle(file);
	return read &&
		(information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
		(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
		information.nNumberOfLinks == 1
		? NativeEntrySafety::SafeRegularFile : NativeEntrySafety::Unsafe;
#else
	struct stat status{};
	if (::lstat(path.c_str(), &status) != 0)
		return errno == ENOENT
			? NativeEntrySafety::Missing : NativeEntrySafety::Unsafe;
	return S_ISREG(status.st_mode) && status.st_nlink == 1
		? NativeEntrySafety::SafeRegularFile : NativeEntrySafety::Unsafe;
#endif
}

bool SafeSingleLinkRegularFile(const std::filesystem::path& path) noexcept
{
	return InspectNativeEntry(path) == NativeEntrySafety::SafeRegularFile;
}

bool SafeSingleLinkRegularFileOrMissing(
	const std::filesystem::path& path) noexcept
{
	const NativeEntrySafety safety = InspectNativeEntry(path);
	return safety == NativeEntrySafety::Missing ||
		safety == NativeEntrySafety::SafeRegularFile;
}

bool HasExpectedStagingName(DedicatedCampaignSlot slot,
	const std::filesystem::path& path) noexcept
{
	try
	{
		const std::string name = path.filename().string();
		const std::string prefix = StagingPrefix(slot);
		return name.size() > prefix.size() &&
			name.compare(0, prefix.size(), prefix) == 0;
	}
	catch (...)
	{
		return false;
	}
}

bool CopyIntoReservedStaging(const std::filesystem::path& source,
	const std::filesystem::path& destination) noexcept
{
	try
	{
		if (!SafeSingleLinkRegularFile(source) ||
			!SafeSingleLinkRegularFile(destination))
			return false;
		std::error_code error;
		const std::uintmax_t sourceSize = std::filesystem::file_size(source, error);
		if (error || sourceSize == 0 ||
			sourceSize > DedicatedCampaignMaximumCheckpointBytes)
			return false;

		std::ifstream input(source, std::ios::binary);
		std::ofstream output(destination,
			std::ios::binary | std::ios::out | std::ios::trunc);
		if (!input || !output) return false;
		std::array<char, CopyBufferBytes> buffer{};
		std::uint64_t copied = 0;
		while (input)
		{
			input.read(buffer.data(),
				static_cast<std::streamsize>(buffer.size()));
			const std::streamsize count = input.gcount();
			if (count < 0) return false;
			if (count)
			{
				output.write(buffer.data(), count);
				if (!output) return false;
				copied += static_cast<std::uint64_t>(count);
			}
		}
		if (!input.eof()) return false;
		const bool readComplete = !input.bad();
		output.flush();
		if (!output) return false;
		input.clear();
		input.close();
		const bool inputClosed = !input.fail();
		output.close();
		if (!readComplete || !inputClosed || !output || copied != sourceSize ||
			!SafeSingleLinkRegularFile(destination))
			return false;
		error.clear();
		return std::filesystem::file_size(destination, error) == sourceSize &&
			!error;
	}
	catch (...)
	{
		return false;
	}
}

bool ZeroDigest(const DedicatedCampaignCheckpointSha256& digest) noexcept
{
	return std::all_of(digest.begin(), digest.end(),
		[](std::uint8_t value) { return value == 0; });
}

#ifdef _WIN32
using NativeFile = HANDLE;
constexpr NativeFile InvalidNativeFile = INVALID_HANDLE_VALUE;

bool SafeNativeFile(NativeFile file) noexcept
{
	BY_HANDLE_FILE_INFORMATION information{};
	return file != InvalidNativeFile && GetFileType(file) == FILE_TYPE_DISK &&
		GetFileInformationByHandle(file, &information) != FALSE &&
		(information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
		(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
		information.nNumberOfLinks == 1;
}

NativeFile OpenNativeFile(const std::filesystem::path& path,
	bool write, bool createIfMissing, bool& created) noexcept
{
	created = false;
	const DWORD access = GENERIC_READ | (write ? GENERIC_WRITE : 0);
	NativeFile file = CreateFileW(path.c_str(), access, FILE_SHARE_READ,
		nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (file == InvalidNativeFile && createIfMissing &&
		GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		file = CreateFileW(path.c_str(), access, FILE_SHARE_READ, nullptr,
			CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
			nullptr);
		created = file != InvalidNativeFile;
	}
	if (!SafeNativeFile(file))
	{
		if (file != InvalidNativeFile) (void)CloseHandle(file);
		return InvalidNativeFile;
	}
	return file;
}

bool NativeFileSize(NativeFile file, std::uint64_t& size) noexcept
{
	LARGE_INTEGER value{};
	if (!GetFileSizeEx(file, &value) || value.QuadPart < 0) return false;
	size = static_cast<std::uint64_t>(value.QuadPart);
	return true;
}

bool SeekNativeStart(NativeFile file) noexcept
{
	LARGE_INTEGER zero{};
	return SetFilePointerEx(file, zero, nullptr, FILE_BEGIN) != FALSE;
}

bool TruncateNative(NativeFile file) noexcept
{
	return SeekNativeStart(file) && SetEndOfFile(file) != FALSE;
}

bool ReadNative(NativeFile file, std::uint8_t* bytes,
	std::size_t capacity, std::size_t& count) noexcept
{
	DWORD read = 0;
	const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(
		capacity, (std::numeric_limits<DWORD>::max)()));
	if (!ReadFile(file, bytes, requested, &read, nullptr)) return false;
	count = static_cast<std::size_t>(read);
	return true;
}

bool WriteNative(NativeFile file, const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	std::size_t offset = 0;
	while (offset < size)
	{
		const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(
			size - offset, (std::numeric_limits<DWORD>::max)()));
		DWORD written = 0;
		if (!WriteFile(file, bytes + offset, requested, &written, nullptr) ||
			written == 0)
			return false;
		offset += static_cast<std::size_t>(written);
	}
	return true;
}

bool SyncNative(NativeFile file) noexcept
{
	return FlushFileBuffers(file) != FALSE;
}

bool CloseNative(NativeFile file) noexcept
{
	return file == InvalidNativeFile || CloseHandle(file) != FALSE;
}

bool PathStillNamesFile(const std::filesystem::path& path,
	NativeFile expected) noexcept
{
	NativeFile current = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (!SafeNativeFile(expected) || !SafeNativeFile(current))
	{
		if (current != InvalidNativeFile) (void)CloseHandle(current);
		return false;
	}
	BY_HANDLE_FILE_INFORMATION expectedInformation{};
	BY_HANDLE_FILE_INFORMATION currentInformation{};
	const bool same = GetFileInformationByHandle(expected,
		&expectedInformation) != FALSE &&
		GetFileInformationByHandle(current, &currentInformation) != FALSE &&
		expectedInformation.dwVolumeSerialNumber ==
			currentInformation.dwVolumeSerialNumber &&
		expectedInformation.nFileIndexHigh == currentInformation.nFileIndexHigh &&
		expectedInformation.nFileIndexLow == currentInformation.nFileIndexLow;
	return CloseHandle(current) != FALSE && same;
}
#else
using NativeFile = int;
constexpr NativeFile InvalidNativeFile = -1;

bool SafeNativeFile(NativeFile file) noexcept
{
	struct stat status{};
	return file >= 0 && ::fstat(file, &status) == 0 &&
		S_ISREG(status.st_mode) && status.st_nlink == 1;
}

NativeFile OpenNativeFile(const std::filesystem::path& path,
	bool write, bool createIfMissing, bool& created) noexcept
{
	created = false;
	int flags = write ? O_RDWR : O_RDONLY;
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
	flags |= O_NOFOLLOW;
#endif
	NativeFile file = ::open(path.c_str(), flags);
	if (file < 0 && createIfMissing && errno == ENOENT)
	{
		file = ::open(path.c_str(), flags | O_CREAT | O_EXCL,
			S_IRUSR | S_IWUSR);
		created = file >= 0;
	}
	if (!SafeNativeFile(file))
	{
		if (file >= 0) (void)::close(file);
		return InvalidNativeFile;
	}
	return file;
}

bool NativeFileSize(NativeFile file, std::uint64_t& size) noexcept
{
	struct stat status{};
	if (::fstat(file, &status) != 0 || status.st_size < 0) return false;
	size = static_cast<std::uint64_t>(status.st_size);
	return true;
}

bool SeekNativeStart(NativeFile file) noexcept
{
	return ::lseek(file, 0, SEEK_SET) == 0;
}

bool TruncateNative(NativeFile file) noexcept
{
	return ::ftruncate(file, 0) == 0 && SeekNativeStart(file);
}

bool ReadNative(NativeFile file, std::uint8_t* bytes,
	std::size_t capacity, std::size_t& count) noexcept
{
	ssize_t result;
	do { result = ::read(file, bytes, capacity); }
	while (result < 0 && errno == EINTR);
	if (result < 0) return false;
	count = static_cast<std::size_t>(result);
	return true;
}

bool WriteNative(NativeFile file, const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	std::size_t offset = 0;
	while (offset < size)
	{
		ssize_t written;
		do { written = ::write(file, bytes + offset, size - offset); }
		while (written < 0 && errno == EINTR);
		if (written <= 0) return false;
		offset += static_cast<std::size_t>(written);
	}
	return true;
}

bool SyncNative(NativeFile file) noexcept
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

bool CloseNative(NativeFile file) noexcept
{
	return file < 0 || ::close(file) == 0;
}

bool PathStillNamesFile(const std::filesystem::path& path,
	NativeFile expected) noexcept
{
	struct stat expectedStatus{};
	struct stat currentStatus{};
	return ::fstat(expected, &expectedStatus) == 0 &&
		::lstat(path.c_str(), &currentStatus) == 0 &&
		S_ISREG(currentStatus.st_mode) && currentStatus.st_nlink == 1 &&
		expectedStatus.st_nlink == 1 &&
		expectedStatus.st_dev == currentStatus.st_dev &&
		expectedStatus.st_ino == currentStatus.st_ino;
}
#endif

bool HashNativeFile(NativeFile file, std::uint64_t expectedSize,
	DedicatedCampaignCheckpointSha256& digest) noexcept
{
	if (!SafeNativeFile(file) || !SeekNativeStart(file)) return false;
	Sha256 hasher;
	std::array<std::uint8_t, CopyBufferBytes> buffer{};
	std::uint64_t remaining = expectedSize;
	while (remaining)
	{
		const std::size_t wanted = static_cast<std::size_t>(
			std::min<std::uint64_t>(remaining, buffer.size()));
		std::size_t read = 0;
		if (!ReadNative(file, buffer.data(), wanted, read) || read == 0 ||
			read > wanted)
			return false;
		hasher.update(buffer.data(), read);
		remaining -= read;
	}
	std::uint8_t trailing = 0;
	std::size_t trailingRead = 0;
	std::uint64_t confirmedSize = 0;
	if (!ReadNative(file, &trailing, 1, trailingRead) || trailingRead != 0 ||
		!SafeNativeFile(file) ||
		!NativeFileSize(file, confirmedSize) || confirmedSize != expectedSize)
		return false;
	digest = hasher.finish();
	return true;
}

bool PrepareScratchPair(const std::filesystem::path (&paths)[2]) noexcept
{
	NativeFile files[2] = {InvalidNativeFile, InvalidNativeFile};
	bool created[2] = {false, false};
	bool success = true;
	for (std::size_t index = 0; index < 2; ++index)
	{
		files[index] = OpenNativeFile(paths[index], true, true, created[index]);
		if (files[index] == InvalidNativeFile ||
			!PathStillNamesFile(paths[index], files[index]))
		{
			success = false;
			break;
		}
	}
	for (std::size_t index = 0; success && index < 2; ++index)
	{
		std::uint64_t size = 1;
		success = SafeNativeFile(files[index]) &&
			PathStillNamesFile(paths[index], files[index]) &&
			TruncateNative(files[index]) && SyncNative(files[index]) &&
			NativeFileSize(files[index], size) && size == 0 &&
			PathStillNamesFile(paths[index], files[index]);
	}
	for (NativeFile file : files)
		success = CloseNative(file) && success;
	(void)created;
	return success;
}

bool CopyAuthenticatedCheckpoint(const std::filesystem::path& sourcePath,
	const std::filesystem::path& destinationPath, std::uint64_t expectedSize,
	const DedicatedCampaignCheckpointSha256& expectedSha256) noexcept
{
	bool ignoredCreated = false;
	NativeFile source = OpenNativeFile(
		sourcePath, false, false, ignoredCreated);
	NativeFile destination = OpenNativeFile(
		destinationPath, true, false, ignoredCreated);
	bool success = source != InvalidNativeFile &&
		destination != InvalidNativeFile &&
		PathStillNamesFile(sourcePath, source) &&
		PathStillNamesFile(destinationPath, destination);

	DedicatedCampaignCheckpointSha256 sourceDigest{};
	success = success && HashNativeFile(source, expectedSize, sourceDigest) &&
		sourceDigest == expectedSha256 && SafeNativeFile(destination) &&
		PathStillNamesFile(destinationPath, destination) &&
		TruncateNative(destination) &&
		SeekNativeStart(source);

	Sha256 copiedHasher;
	std::array<std::uint8_t, CopyBufferBytes> buffer{};
	std::uint64_t remaining = expectedSize;
	while (success && remaining)
	{
		const std::size_t wanted = static_cast<std::size_t>(
			std::min<std::uint64_t>(remaining, buffer.size()));
		std::size_t read = 0;
		if (!ReadNative(source, buffer.data(), wanted, read) || read == 0 ||
			read > wanted || !WriteNative(destination, buffer.data(), read))
		{
			success = false;
			break;
		}
		copiedHasher.update(buffer.data(), read);
		remaining -= read;
	}
	std::uint8_t trailing = 0;
	std::size_t trailingRead = 0;
	std::uint64_t sourceSize = 0;
	std::uint64_t destinationSize = 0;
	success = success &&
		ReadNative(source, &trailing, 1, trailingRead) && trailingRead == 0 &&
		copiedHasher.finish() == expectedSha256 && SyncNative(destination) &&
		NativeFileSize(source, sourceSize) && sourceSize == expectedSize &&
		NativeFileSize(destination, destinationSize) &&
		destinationSize == expectedSize && SafeNativeFile(source) &&
		SafeNativeFile(destination) && PathStillNamesFile(sourcePath, source) &&
		PathStillNamesFile(destinationPath, destination);
	DedicatedCampaignCheckpointSha256 destinationDigest{};
	success = success &&
		HashNativeFile(destination, expectedSize, destinationDigest) &&
		destinationDigest == expectedSha256 &&
		PathStillNamesFile(destinationPath, destination);
	const bool destinationClosed = CloseNative(destination);
	const bool sourceClosed = CloseNative(source);
	return success && destinationClosed && sourceClosed;
}
}

const char* DedicatedCampaignLogicalScratch(
	DedicatedCampaignSlot slot) noexcept
{
	switch (slot)
	{
		case DedicatedCampaignSlot::A: return "DedicatedCheckpointA.sav";
		case DedicatedCampaignSlot::B: return "DedicatedCheckpointB.sav";
	}
	return "";
}

DedicatedCampaignSaveAdapter::DedicatedCampaignSaveAdapter(
	std::filesystem::path profileDirectory) noexcept
{
	try { profileDirectory_ = std::move(profileDirectory); }
	catch (...) { profileDirectory_.clear(); }
}

bool DedicatedCampaignSaveAdapter::writeCheckpoint(
	DedicatedCampaignSlot slot,
	const std::filesystem::path& reservedStagingPath) noexcept
{
	try
	{
		const std::filesystem::path scratch = logicalScratchPath(slot);
		if (!KnownSlot(slot) || profileDirectory_.empty() ||
			!profileDirectory_.is_absolute() ||
			reservedStagingPath.parent_path() != profileDirectory_.parent_path() ||
			!HasExpectedStagingName(slot, reservedStagingPath) ||
			!SafeSingleLinkRegularFile(reservedStagingPath) || scratch.empty() ||
			!SafeSingleLinkRegularFileOrMissing(scratch))
			return false;
		if (!SaveDedicatedCampaignGame(slot) ||
			!ValidateDedicatedCampaignGame(slot))
			return false;
		return CopyIntoReservedStaging(scratch, reservedStagingPath);
	}
	catch (...)
	{
		return false;
	}
}

bool DedicatedCampaignSaveAdapter::prepareLogicalScratchFiles() noexcept
{
	try
	{
		if (profileDirectory_.empty() || !profileDirectory_.is_absolute())
			return false;
		const std::filesystem::path paths[2] = {
			logicalScratchPath(DedicatedCampaignSlot::A),
			logicalScratchPath(DedicatedCampaignSlot::B)};
		return !paths[0].empty() && !paths[1].empty() &&
			paths[0].parent_path() == profileDirectory_ &&
			paths[1].parent_path() == profileDirectory_ &&
			PrepareScratchPair(paths);
	}
	catch (...)
	{
		return false;
	}
}

bool DedicatedCampaignSaveAdapter::materializeCheckpoint(
	DedicatedCampaignSlot slot,
	const std::filesystem::path& sourceCheckpoint,
	std::uint64_t expectedSize,
	const DedicatedCampaignCheckpointSha256& expectedSha256) noexcept
{
	try
	{
		if (!KnownSlot(slot) || profileDirectory_.empty() ||
			!profileDirectory_.is_absolute() || expectedSize == 0 ||
			expectedSize > DedicatedCampaignMaximumCheckpointBytes ||
			ZeroDigest(expectedSha256) ||
			sourceCheckpoint.parent_path() != profileDirectory_.parent_path())
			return false;
		const char* expectedSourceName = slot == DedicatedCampaignSlot::A
			? "checkpoint-a.sav" : "checkpoint-b.sav";
		if (sourceCheckpoint.filename() != expectedSourceName) return false;
		const std::filesystem::path destination = logicalScratchPath(slot);
		return !destination.empty() &&
			destination.parent_path() == profileDirectory_ &&
			CopyAuthenticatedCheckpoint(sourceCheckpoint, destination,
				expectedSize, expectedSha256);
	}
	catch (...)
	{
		return false;
	}
}

const std::filesystem::path&
DedicatedCampaignSaveAdapter::profileDirectory() const noexcept
{
	return profileDirectory_;
}

std::filesystem::path DedicatedCampaignSaveAdapter::logicalScratchPath(
	DedicatedCampaignSlot slot) const noexcept
{
	try
	{
		const char* name = DedicatedCampaignLogicalScratch(slot);
		return name && *name ? profileDirectory_ / name : std::filesystem::path{};
	}
	catch (...)
	{
		return {};
	}
}
