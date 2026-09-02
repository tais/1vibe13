#include "FullEngineCoopClientCampaignScratch.h"

#include "DedicatedCampaignFilesystem.h"
#include "DedicatedCampaignSaveAdapter.h"
#include "DedicatedCampaignSaveBridge.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#if defined(__APPLE__)
#include <sys/stdio.h>
#elif defined(__linux__)
#include <sys/syscall.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace CoopSession
{
namespace
{
constexpr char CampaignIdentityFileName[] = "client-campaign-identity.sha256";
constexpr char ReconnectCredentialFileName[] =
	"client-reconnect-credential.bin";
constexpr char RetiredReconnectCredentialFileName[] =
	"client-reconnect-credential.retired";
constexpr char ReconnectCredentialStagingFileName[] =
	"client-reconnect-credential.staging";
constexpr char CheckpointAFileName[] = "checkpoint-a.sav";
constexpr char CheckpointBFileName[] = "checkpoint-b.sav";
constexpr char ProfileQuarantinePrefix[] = "profile.orphan.";
constexpr std::size_t HashBlockBytes = 64;
constexpr std::size_t HashReadBytes = 64u * 1024u;
constexpr std::size_t ReconnectCredentialPayloadBytes =
	CoopCampaignBootstrapWireSize + AdmissionAckWireSize;
constexpr std::size_t ReconnectCredentialRecordBytes =
	ReconnectCredentialPayloadBytes + 32;
using ReconnectCredentialRecord =
	std::array<std::uint8_t, ReconnectCredentialRecordBytes>;

const std::filesystem::path& EmptyPath() noexcept
{
	static const std::filesystem::path empty;
	return empty;
}

bool ZeroDigest(const CoopCampaignIdentitySha256& digest) noexcept
{
	return std::all_of(digest.begin(), digest.end(),
		[](std::uint8_t value) { return value == 0; });
}

DedicatedCampaignSlot OtherSlot(DedicatedCampaignSlot slot) noexcept
{
	return slot == DedicatedCampaignSlot::A
		? DedicatedCampaignSlot::B : DedicatedCampaignSlot::A;
}

const char* CheckpointName(DedicatedCampaignSlot slot) noexcept
{
	return slot == DedicatedCampaignSlot::A
		? CheckpointAFileName : CheckpointBFileName;
}

std::string DerivedCampaignId(
	const CoopCampaignIdentitySha256& identity)
{
	static constexpr char Hex[] = "0123456789abcdef";
	// The backend's portable key is capped at 48 bytes. Persisting and checking
	// the complete digest separately below makes this deliberate 160-bit path
	// prefix safe even if two full identities ever share it.
	std::string result = "client-";
	result.reserve(DedicatedCampaignMaximumIdBytes);
	for (std::size_t index = 0; index < 20; ++index)
	{
		result.push_back(Hex[identity[index] >> 4]);
		result.push_back(Hex[identity[index] & 0x0fu]);
	}
	return result;
}

bool TransferMatchesBootstrap(const CoopCampaignSyncTransferIdentity& transfer,
	const CoopCampaignBootstrapDescriptor& bootstrap) noexcept
{
	return transfer.protocolVersion == bootstrap.protocolVersion &&
		transfer.sessionEpoch == bootstrap.sessionEpoch &&
		transfer.campaignSeed == bootstrap.campaignSeed &&
		transfer.campaignIdentitySha256 == bootstrap.campaignIdentitySha256;
}

bool SameMetadata(const CoopCampaignSyncMetadata& left,
	const CoopCampaignSyncMetadata& right) noexcept
{
	return left.worldMinutes == right.worldMinutes &&
		SameCoopCampaignSyncTransfer(left.transfer, right.transfer);
}

// A reconnect receives a fresh transfer id for the same immutable active
// checkpoint. Permit that exact replay while still rejecting rollback or a
// same-generation descriptor which equivocates about any checkpoint field.
bool SameCheckpointMetadata(const CoopCampaignSyncMetadata& left,
	const CoopCampaignSyncMetadata& right) noexcept
{
	return left.worldMinutes == right.worldMinutes &&
		left.transfer.wireVersion == right.transfer.wireVersion &&
		left.transfer.protocolVersion == right.transfer.protocolVersion &&
		left.transfer.sessionEpoch == right.transfer.sessionEpoch &&
		left.transfer.campaignSeed == right.transfer.campaignSeed &&
		left.transfer.campaignIdentitySha256 ==
			right.transfer.campaignIdentitySha256 &&
		left.transfer.checkpointGeneration ==
			right.transfer.checkpointGeneration &&
		left.transfer.totalSize == right.transfer.totalSize &&
		left.transfer.checkpointSha256 == right.transfer.checkpointSha256 &&
		left.transfer.canonicalChunkBytes ==
			right.transfer.canonicalChunkBytes;
}

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
				size, HashBlockBytes - bufferedBytes_);
			std::memcpy(buffer_.data() + bufferedBytes_, bytes, copied);
			bufferedBytes_ += copied;
			bytes += copied;
			size -= copied;
			if (bufferedBytes_ == HashBlockBytes)
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

	std::array<std::uint32_t, 8> state_{};
	std::array<std::uint8_t, HashBlockBytes> buffer_{};
	std::uint64_t totalBytes_ = 0;
	std::size_t bufferedBytes_ = 0;
};

bool SameBootstrapExceptSessionEpoch(
	const CoopCampaignBootstrapDescriptor& left,
	const CoopCampaignBootstrapDescriptor& right) noexcept
{
	return left.protocolVersion == right.protocolVersion &&
		left.campaignSeed == right.campaignSeed &&
		left.campaignIdentitySha256 == right.campaignIdentitySha256 &&
		left.runtimeFingerprint == right.runtimeFingerprint &&
		left.contentManifestSha256 == right.contentManifestSha256;
}

bool EncodeReconnectCredentialRecord(
	const CoopCampaignBootstrapDescriptor& bootstrap,
	const AdmissionAck& credential,
	ReconnectCredentialRecord& record) noexcept
{
	if (credential.protocolVersion != bootstrap.protocolVersion ||
		credential.sessionEpoch != bootstrap.sessionEpoch)
		return false;
	CoopCampaignBootstrapBytes bootstrapBytes{};
	AdmissionAckBytes credentialBytes{};
	if (!EncodeCoopCampaignBootstrap(bootstrap, bootstrapBytes) ||
		!EncodeAdmissionAck(credential, credentialBytes))
		return false;

	ReconnectCredentialRecord encoded{};
	std::copy(bootstrapBytes.begin(), bootstrapBytes.end(), encoded.begin());
	std::copy(credentialBytes.begin(), credentialBytes.end(),
		encoded.begin() + CoopCampaignBootstrapWireSize);
	Sha256 hasher;
	hasher.update(encoded.data(), ReconnectCredentialPayloadBytes);
	const DedicatedCampaignCheckpointSha256 digest = hasher.finish();
	std::copy(digest.begin(), digest.end(),
		encoded.begin() + ReconnectCredentialPayloadBytes);
	record = encoded;
	return true;
}

bool DecodeReconnectCredentialRecord(
	const ReconnectCredentialRecord& record,
	CoopCampaignBootstrapDescriptor& bootstrap,
	AdmissionAck& credential) noexcept
{
	Sha256 hasher;
	hasher.update(record.data(), ReconnectCredentialPayloadBytes);
	const DedicatedCampaignCheckpointSha256 expected = hasher.finish();
	if (!std::equal(expected.begin(), expected.end(),
		record.begin() + ReconnectCredentialPayloadBytes))
		return false;

	CoopCampaignBootstrapDescriptor decodedBootstrap;
	AdmissionAck decodedCredential;
	if (DecodeCoopCampaignBootstrap(record.data(),
			CoopCampaignBootstrapWireSize, decodedBootstrap) !=
			CoopCampaignBootstrapDecodeResult::Success ||
		DecodeAdmissionAck(
			record.data() + CoopCampaignBootstrapWireSize,
			AdmissionAckWireSize, decodedCredential) != DecodeResult::Ok ||
		decodedCredential.protocolVersion != decodedBootstrap.protocolVersion ||
		decodedCredential.sessionEpoch != decodedBootstrap.sessionEpoch)
		return false;
	bootstrap = decodedBootstrap;
	credential = decodedCredential;
	return true;
}

enum class PrivateFileEntryState
{
	Missing,
	Safe,
	Unsafe,
	Failure
};

enum class PrivateFileRemoveResult
{
	Missing,
	Removed,
	Unsafe,
	Failure
};

#ifdef _WIN32
using NativeFile = HANDLE;
using NativeDirectory = HANDLE;
constexpr NativeFile InvalidNativeFile = INVALID_HANDLE_VALUE;
constexpr NativeDirectory InvalidNativeDirectory = INVALID_HANDLE_VALUE;

struct NativeFileIdentity
{
	DWORD volume = 0;
	DWORD indexHigh = 0;
	DWORD indexLow = 0;
};

bool SafeRegularFile(NativeFile file) noexcept
{
	BY_HANDLE_FILE_INFORMATION information{};
	return file != InvalidNativeFile && GetFileType(file) == FILE_TYPE_DISK &&
		GetFileInformationByHandle(file, &information) != FALSE &&
		(information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
		(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
		information.nNumberOfLinks == 1;
}

bool CaptureIdentity(NativeFile file, NativeFileIdentity& identity) noexcept
{
	BY_HANDLE_FILE_INFORMATION information{};
	if (!SafeRegularFile(file) ||
		GetFileInformationByHandle(file, &information) == FALSE)
		return false;
	identity.volume = information.dwVolumeSerialNumber;
	identity.indexHigh = information.nFileIndexHigh;
	identity.indexLow = information.nFileIndexLow;
	return true;
}

bool SameIdentity(const NativeFileIdentity& left,
	const NativeFileIdentity& right) noexcept
{
	return left.volume == right.volume && left.indexHigh == right.indexHigh &&
		left.indexLow == right.indexLow;
}

NativeDirectory OpenNativeDirectory(
	const std::filesystem::path& path) noexcept
{
	NativeDirectory directory = CreateFileW(path.c_str(),
		FILE_LIST_DIRECTORY | READ_CONTROL,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS |
		FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (directory == InvalidNativeDirectory) return InvalidNativeDirectory;
	BY_HANDLE_FILE_INFORMATION information{};
	if (GetFileInformationByHandle(directory, &information) == FALSE ||
		(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
		(information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
	{
		(void)CloseHandle(directory);
		return InvalidNativeDirectory;
	}
	return directory;
}

bool CloseNativeDirectory(NativeDirectory directory) noexcept
{
	return directory == InvalidNativeDirectory || CloseHandle(directory) != FALSE;
}

bool SyncNativeDirectory(NativeDirectory directory) noexcept
{
	// Windows does not provide a supported directory-fsync operation. The
	// identity file itself is flushed, and CREATE_NEW prevents replacement
	// while the private backend lease is held.
	return directory != InvalidNativeDirectory;
}

NativeFile OpenRelativeFile(NativeDirectory,
	const std::filesystem::path& directoryPath, const char* name,
	bool write, bool create, bool& created) noexcept
{
	created = false;
	try
	{
		const std::filesystem::path path = directoryPath / name;
		const DWORD access = GENERIC_READ | (write ? GENERIC_WRITE : 0);
		NativeFile file = CreateFileW(path.c_str(), access,
			FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (file == InvalidNativeFile && create &&
			GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			file = CreateFileW(path.c_str(), access,
				FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
			created = file != InvalidNativeFile;
		}
		if (!SafeRegularFile(file))
		{
			if (file != InvalidNativeFile) (void)CloseHandle(file);
			return InvalidNativeFile;
		}
		return file;
	}
	catch (...)
	{
		return InvalidNativeFile;
	}
}

bool NativeFileSize(NativeFile file, std::uint64_t& size) noexcept
{
	LARGE_INTEGER nativeSize{};
	if (!GetFileSizeEx(file, &nativeSize) || nativeSize.QuadPart < 0)
		return false;
	size = static_cast<std::uint64_t>(nativeSize.QuadPart);
	return true;
}

bool TruncateNative(NativeFile file, std::uint64_t size) noexcept
{
	LARGE_INTEGER position{};
	position.QuadPart = static_cast<LONGLONG>(size);
	return SetFilePointerEx(file, position, nullptr, FILE_BEGIN) != FALSE &&
		SetEndOfFile(file) != FALSE;
}

bool ReadNativeAt(NativeFile file, std::uint64_t offset,
	std::uint8_t* bytes, std::size_t size) noexcept
{
	std::size_t completed = 0;
	while (completed < size)
	{
		LARGE_INTEGER position{};
		position.QuadPart = static_cast<LONGLONG>(offset + completed);
		if (SetFilePointerEx(file, position, nullptr, FILE_BEGIN) == FALSE)
			return false;
		const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(
			size - completed, (std::numeric_limits<DWORD>::max)()));
		DWORD read = 0;
		if (!ReadFile(file, bytes + completed, requested, &read, nullptr) ||
			read == 0)
			return false;
		completed += static_cast<std::size_t>(read);
	}
	return true;
}

bool WriteNativeAt(NativeFile file, std::uint64_t offset,
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	std::size_t completed = 0;
	while (completed < size)
	{
		LARGE_INTEGER position{};
		position.QuadPart = static_cast<LONGLONG>(offset + completed);
		if (SetFilePointerEx(file, position, nullptr, FILE_BEGIN) == FALSE)
			return false;
		const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(
			size - completed, (std::numeric_limits<DWORD>::max)()));
		DWORD written = 0;
		if (!WriteFile(file, bytes + completed, requested, &written, nullptr) ||
			written == 0)
			return false;
		completed += static_cast<std::size_t>(written);
	}
	return true;
}

bool SyncNativeFile(NativeFile file) noexcept
{
	return FlushFileBuffers(file) != FALSE;
}

bool CloseNativeFile(NativeFile file) noexcept
{
	return file == InvalidNativeFile || CloseHandle(file) != FALSE;
}

bool PathNamesIdentity(NativeDirectory,
	const std::filesystem::path& directoryPath, const char* name,
	const NativeFileIdentity& expected) noexcept
{
	try
	{
		const std::filesystem::path path = directoryPath / name;
		NativeFile current = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		NativeFileIdentity identity{};
		const bool same = current != InvalidNativeFile &&
			CaptureIdentity(current, identity) && SameIdentity(identity, expected);
		return CloseNativeFile(current) && same;
	}
	catch (...)
	{
		return false;
	}
}

bool SafePrivateCredentialFile(NativeFile file) noexcept
{
	// The backend contract requires the operator-selected Windows root to have
	// an equivalently private ACL. Children inherit that ACL; native checks here
	// additionally reject reparse points, directories, and hard links.
	return SafeRegularFile(file);
}

PrivateFileEntryState CredentialEntryState(NativeDirectory,
	const std::filesystem::path& directoryPath, const char* name) noexcept
{
	try
	{
		const std::filesystem::path path = directoryPath / name;
		const DWORD attributes = GetFileAttributesW(path.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			const DWORD error = GetLastError();
			return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
				? PrivateFileEntryState::Missing
				: PrivateFileEntryState::Failure;
		}
		if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
			(attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			return PrivateFileEntryState::Unsafe;
		NativeFile file = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		NativeFileIdentity identity{};
		const bool safe = file != InvalidNativeFile &&
			SafePrivateCredentialFile(file) && CaptureIdentity(file, identity) &&
			PathNamesIdentity(InvalidNativeDirectory, directoryPath, name, identity);
		const bool closed = CloseNativeFile(file);
		return safe && closed
			? PrivateFileEntryState::Safe
			: PrivateFileEntryState::Unsafe;
	}
	catch (...)
	{
		return PrivateFileEntryState::Failure;
	}
}

NativeFile CreateExclusivePrivateFile(NativeDirectory,
	const std::filesystem::path& directoryPath, const char* name) noexcept
{
	try
	{
		const std::filesystem::path path = directoryPath / name;
		NativeFile file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (!SafePrivateCredentialFile(file))
		{
			(void)CloseNativeFile(file);
			return InvalidNativeFile;
		}
		return file;
	}
	catch (...)
	{
		return InvalidNativeFile;
	}
}

PrivateFileRemoveResult RemovePrivateFile(NativeDirectory directory,
	const std::filesystem::path& directoryPath, const char* name) noexcept
{
	const PrivateFileEntryState state =
		CredentialEntryState(directory, directoryPath, name);
	if (state == PrivateFileEntryState::Missing)
		return PrivateFileRemoveResult::Missing;
	if (state == PrivateFileEntryState::Unsafe)
		return PrivateFileRemoveResult::Unsafe;
	if (state != PrivateFileEntryState::Safe)
		return PrivateFileRemoveResult::Failure;
	try
	{
		const std::filesystem::path path = directoryPath / name;
		NativeFile file = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		NativeFileIdentity identity{};
		const bool exact = file != InvalidNativeFile &&
			CaptureIdentity(file, identity) &&
			PathNamesIdentity(directory, directoryPath, name, identity);
		const bool closed = CloseNativeFile(file);
		if (!exact || !closed) return PrivateFileRemoveResult::Unsafe;
		if (DeleteFileW(path.c_str()) == FALSE)
			return PrivateFileRemoveResult::Failure;
		return SyncNativeDirectory(directory)
			? PrivateFileRemoveResult::Removed
			: PrivateFileRemoveResult::Failure;
	}
	catch (...)
	{
		return PrivateFileRemoveResult::Failure;
	}
}

bool ReplacePrivateFile(NativeDirectory directory,
	const std::filesystem::path& directoryPath, const char* stagingName,
	const char* targetName, const NativeFileIdentity& stagingIdentity) noexcept
{
	const PrivateFileEntryState target =
		CredentialEntryState(directory, directoryPath, targetName);
	if (target != PrivateFileEntryState::Missing &&
		target != PrivateFileEntryState::Safe)
		return false;
	if (!PathNamesIdentity(directory, directoryPath,
		stagingName, stagingIdentity))
		return false;
	try
	{
		const std::filesystem::path staging = directoryPath / stagingName;
		const std::filesystem::path targetPath = directoryPath / targetName;
		return MoveFileExW(staging.c_str(), targetPath.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE &&
			PathNamesIdentity(directory, directoryPath,
				targetName, stagingIdentity);
	}
	catch (...)
	{
		return false;
	}
}

bool RetirePrivateFileNoReplace(NativeDirectory directory,
	const std::filesystem::path& directoryPath, const char* sourceName,
	const char* retiredName) noexcept
{
	if (CredentialEntryState(directory, directoryPath, sourceName) !=
			PrivateFileEntryState::Safe ||
		CredentialEntryState(directory, directoryPath, retiredName) !=
			PrivateFileEntryState::Missing)
		return false;
	bool created = false;
	NativeFile source = OpenRelativeFile(directory, directoryPath,
		sourceName, false, false, created);
	NativeFileIdentity identity{};
	const bool exact = source != InvalidNativeFile &&
		SafePrivateCredentialFile(source) && CaptureIdentity(source, identity) &&
		PathNamesIdentity(directory, directoryPath, sourceName, identity);
	const bool closed = CloseNativeFile(source);
	if (!exact || !closed) return false;
	try
	{
		const std::filesystem::path sourcePath = directoryPath / sourceName;
		const std::filesystem::path retiredPath = directoryPath / retiredName;
		// Deliberately omit MOVEFILE_REPLACE_EXISTING. A pre-existing terminal
		// marker is evidence which must be validated, never overwritten.
		if (MoveFileExW(sourcePath.c_str(), retiredPath.c_str(),
				MOVEFILE_WRITE_THROUGH) == FALSE)
			return false;
	}
	catch (...)
	{
		return false;
	}
	return CredentialEntryState(directory, directoryPath, sourceName) ==
			PrivateFileEntryState::Missing &&
		PathNamesIdentity(directory, directoryPath, retiredName, identity);
}
#else
using NativeFile = int;
using NativeDirectory = int;
constexpr NativeFile InvalidNativeFile = -1;
constexpr NativeDirectory InvalidNativeDirectory = -1;

struct NativeFileIdentity
{
	dev_t device = 0;
	ino_t inode = 0;
};

bool SafeRegularFile(NativeFile file) noexcept
{
	struct stat status{};
	return file >= 0 && ::fstat(file, &status) == 0 &&
		S_ISREG(status.st_mode) && status.st_nlink == 1;
}

bool CaptureIdentity(NativeFile file, NativeFileIdentity& identity) noexcept
{
	struct stat status{};
	if (!SafeRegularFile(file) || ::fstat(file, &status) != 0) return false;
	identity.device = status.st_dev;
	identity.inode = status.st_ino;
	return true;
}

bool SameIdentity(const NativeFileIdentity& left,
	const NativeFileIdentity& right) noexcept
{
	return left.device == right.device && left.inode == right.inode;
}

NativeDirectory OpenNativeDirectory(
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
	struct stat status{};
	if (directory < 0 || ::fstat(directory, &status) != 0 ||
		!S_ISDIR(status.st_mode))
	{
		if (directory >= 0) (void)::close(directory);
		return InvalidNativeDirectory;
	}
	return directory;
}

bool CloseNativeDirectory(NativeDirectory directory) noexcept
{
	return directory < 0 || ::close(directory) == 0;
}

bool SyncNativeDirectory(NativeDirectory directory) noexcept
{
	int result;
	do { result = ::fsync(directory); } while (result != 0 && errno == EINTR);
	return result == 0;
}

NativeFile OpenRelativeFile(NativeDirectory directory,
	const std::filesystem::path&, const char* name,
	bool write, bool create, bool& created) noexcept
{
	created = false;
	int flags = write ? O_RDWR : O_RDONLY;
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
	flags |= O_NOFOLLOW;
#endif
	NativeFile file = ::openat(directory, name, flags);
	if (file < 0 && create && errno == ENOENT)
	{
		file = ::openat(directory, name, flags | O_CREAT | O_EXCL,
			S_IRUSR | S_IWUSR);
		created = file >= 0;
	}
	if (!SafeRegularFile(file))
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

bool TruncateNative(NativeFile file, std::uint64_t size) noexcept
{
	if (size > static_cast<std::uint64_t>(
			(std::numeric_limits<off_t>::max)()))
		return false;
	int result;
	do { result = ::ftruncate(file, static_cast<off_t>(size)); }
	while (result != 0 && errno == EINTR);
	return result == 0;
}

bool ReadNativeAt(NativeFile file, std::uint64_t offset,
	std::uint8_t* bytes, std::size_t size) noexcept
{
	if (offset > static_cast<std::uint64_t>(
			(std::numeric_limits<off_t>::max)()))
		return false;
	std::size_t completed = 0;
	while (completed < size)
	{
		ssize_t read;
		do
		{
			read = ::pread(file, bytes + completed, size - completed,
				static_cast<off_t>(offset + completed));
		}
		while (read < 0 && errno == EINTR);
		if (read <= 0) return false;
		completed += static_cast<std::size_t>(read);
	}
	return true;
}

bool WriteNativeAt(NativeFile file, std::uint64_t offset,
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (offset > static_cast<std::uint64_t>(
			(std::numeric_limits<off_t>::max)()))
		return false;
	std::size_t completed = 0;
	while (completed < size)
	{
		ssize_t written;
		do
		{
			written = ::pwrite(file, bytes + completed, size - completed,
				static_cast<off_t>(offset + completed));
		}
		while (written < 0 && errno == EINTR);
		if (written <= 0) return false;
		completed += static_cast<std::size_t>(written);
	}
	return true;
}

bool SyncNativeFile(NativeFile file) noexcept
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

bool CloseNativeFile(NativeFile file) noexcept
{
	return file < 0 || ::close(file) == 0;
}

bool PathNamesIdentity(NativeDirectory directory,
	const std::filesystem::path&, const char* name,
	const NativeFileIdentity& expected) noexcept
{
	struct stat status{};
	return ::fstatat(directory, name, &status, AT_SYMLINK_NOFOLLOW) == 0 &&
		S_ISREG(status.st_mode) && status.st_nlink == 1 &&
		status.st_dev == expected.device && status.st_ino == expected.inode;
}

bool SafePrivateCredentialFile(NativeFile file) noexcept
{
	struct stat status{};
	return file >= 0 && ::fstat(file, &status) == 0 &&
		S_ISREG(status.st_mode) && status.st_nlink == 1 &&
		status.st_uid == ::geteuid() &&
		(status.st_mode & 0777) == (S_IRUSR | S_IWUSR);
}

PrivateFileEntryState CredentialEntryState(NativeDirectory directory,
	const std::filesystem::path&, const char* name) noexcept
{
	struct stat status{};
	if (::fstatat(directory, name, &status, AT_SYMLINK_NOFOLLOW) != 0)
		return errno == ENOENT
			? PrivateFileEntryState::Missing
			: PrivateFileEntryState::Failure;
	return S_ISREG(status.st_mode) && status.st_nlink == 1 &&
		status.st_uid == ::geteuid() &&
		(status.st_mode & 0777) == (S_IRUSR | S_IWUSR)
		? PrivateFileEntryState::Safe
		: PrivateFileEntryState::Unsafe;
}

NativeFile CreateExclusivePrivateFile(NativeDirectory directory,
	const std::filesystem::path&, const char* name) noexcept
{
	int flags = O_RDWR | O_CREAT | O_EXCL;
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
	flags |= O_NOFOLLOW;
#endif
	const NativeFile file = ::openat(directory, name, flags,
		S_IRUSR | S_IWUSR);
	if (file < 0) return InvalidNativeFile;
	if (::fchmod(file, S_IRUSR | S_IWUSR) != 0 ||
		!SafePrivateCredentialFile(file))
	{
		(void)::close(file);
		return InvalidNativeFile;
	}
	return file;
}

PrivateFileRemoveResult RemovePrivateFile(NativeDirectory directory,
	const std::filesystem::path& directoryPath, const char* name) noexcept
{
	const PrivateFileEntryState state =
		CredentialEntryState(directory, directoryPath, name);
	if (state == PrivateFileEntryState::Missing)
		return PrivateFileRemoveResult::Missing;
	if (state == PrivateFileEntryState::Unsafe)
		return PrivateFileRemoveResult::Unsafe;
	if (state != PrivateFileEntryState::Safe)
		return PrivateFileRemoveResult::Failure;
	if (::unlinkat(directory, name, 0) != 0)
		return PrivateFileRemoveResult::Failure;
	return SyncNativeDirectory(directory)
		? PrivateFileRemoveResult::Removed
		: PrivateFileRemoveResult::Failure;
}

bool ReplacePrivateFile(NativeDirectory directory,
	const std::filesystem::path& directoryPath, const char* stagingName,
	const char* targetName, const NativeFileIdentity& stagingIdentity) noexcept
{
	const PrivateFileEntryState target =
		CredentialEntryState(directory, directoryPath, targetName);
	if (target != PrivateFileEntryState::Missing &&
		target != PrivateFileEntryState::Safe)
		return false;
	if (!PathNamesIdentity(directory, directoryPath,
		stagingName, stagingIdentity) ||
		::renameat(directory, stagingName, directory, targetName) != 0 ||
		!SyncNativeDirectory(directory))
		return false;
	return PathNamesIdentity(directory, directoryPath,
		targetName, stagingIdentity);
}

bool RetirePrivateFileNoReplace(NativeDirectory directory,
	const std::filesystem::path& directoryPath, const char* sourceName,
	const char* retiredName) noexcept
{
	if (CredentialEntryState(directory, directoryPath, sourceName) !=
			PrivateFileEntryState::Safe ||
		CredentialEntryState(directory, directoryPath, retiredName) !=
			PrivateFileEntryState::Missing)
		return false;
	bool created = false;
	NativeFile source = OpenRelativeFile(directory, directoryPath,
		sourceName, false, false, created);
	NativeFileIdentity identity{};
	const bool exact = source != InvalidNativeFile &&
		SafePrivateCredentialFile(source) && CaptureIdentity(source, identity) &&
		PathNamesIdentity(directory, directoryPath, sourceName, identity);
	const bool closed = CloseNativeFile(source);
	if (!exact || !closed) return false;

	int renamed = -1;
#if defined(__APPLE__)
	do
	{
		renamed = ::renameatx_np(directory, sourceName, directory, retiredName,
			RENAME_EXCL);
	}
	while (renamed != 0 && errno == EINTR);
#elif defined(__linux__) && defined(SYS_renameat2)
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif
	long result;
	do
	{
		result = ::syscall(SYS_renameat2, directory, sourceName,
			directory, retiredName, RENAME_NOREPLACE);
	}
	while (result != 0 && errno == EINTR);
	renamed = result == 0 ? 0 : -1;
#else
	// No portable POSIX primitive can promise a no-replace rename. Refuse to
	// weaken terminality on platforms without an exclusive rename operation.
	(void)directoryPath;
	(void)identity;
	return false;
#endif
	if (renamed != 0) return false;
	const bool published =
		CredentialEntryState(directory, directoryPath, sourceName) ==
			PrivateFileEntryState::Missing &&
		PathNamesIdentity(directory, directoryPath, retiredName, identity);
	// Once the rename has happened, a directory-sync failure is reported as a
	// failure to the current process. Recovery sees either the old live bearer
	// or this terminal marker; an atomic rename can never recover as Missing.
	return published && SyncNativeDirectory(directory);
}
#endif

bool SafeRegularPath(const std::filesystem::path& path) noexcept
{
	try
	{
		std::error_code error;
		const std::filesystem::file_status linkStatus =
			std::filesystem::symlink_status(path, error);
		if (error || std::filesystem::is_symlink(linkStatus) ||
			!std::filesystem::is_regular_file(linkStatus))
			return false;
		error.clear();
		return std::filesystem::hard_link_count(path, error) == 1 && !error;
	}
	catch (...)
	{
		return false;
	}
}

bool ProfileQuarantineName(const std::string& name) noexcept
{
	constexpr std::size_t prefixBytes = sizeof(ProfileQuarantinePrefix) - 1;
	if (name.size() <= prefixBytes ||
		name.compare(0, prefixBytes, ProfileQuarantinePrefix) != 0)
		return false;
	const std::size_t separator = name.find('.', prefixBytes);
	if (separator == std::string::npos || separator == prefixBytes ||
		separator + 1 == name.size())
		return false;
	for (std::size_t index = prefixBytes; index < separator; ++index)
		if (name[index] < '0' || name[index] > '9') return false;
	for (std::size_t index = separator + 1; index < name.size(); ++index)
		if (name[index] < '0' || name[index] > '9') return false;
	return true;
}

bool SafePrivateDirectoryPath(const std::filesystem::path& path) noexcept
{
	try
	{
		std::error_code error;
		const std::filesystem::file_status status =
			std::filesystem::symlink_status(path, error);
		if (error || std::filesystem::is_symlink(status) ||
			!std::filesystem::is_directory(status))
			return false;
#ifndef _WIN32
		const std::filesystem::perms forbidden =
			std::filesystem::perms::group_read |
			std::filesystem::perms::group_write |
			std::filesystem::perms::group_exec |
			std::filesystem::perms::others_read |
			std::filesystem::perms::others_write |
			std::filesystem::perms::others_exec;
		if ((status.permissions() & forbidden) !=
			std::filesystem::perms::none)
			return false;
#endif
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool ProfileAllowlist(const std::filesystem::path& profile,
	bool requireBoth, bool& empty) noexcept
{
	empty = true;
	bool foundA = false;
	bool foundB = false;
	try
	{
		std::error_code error;
		for (std::filesystem::directory_iterator iterator(profile, error), end;
			!error && iterator != end; iterator.increment(error))
		{
			empty = false;
			const std::string name = iterator->path().filename().string();
			if (name == DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A))
				foundA = true;
			else if (name ==
				DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::B))
				foundB = true;
			else
				return false;
			if (!SafeRegularPath(iterator->path())) return false;
		}
		if (error) return false;
		return !requireBoth || (foundA && foundB);
	}
	catch (...)
	{
		return false;
	}
}

enum class CampaignDirectoryScan
{
	CleanWithoutIdentity,
	IdentityPresent,
	Unsafe
};

CampaignDirectoryScan ScanCampaignDirectory(
	const std::filesystem::path& campaignDirectory,
	const std::filesystem::path& profileDirectory) noexcept
{
	bool identityPresent = false;
	bool checkpointPresent = false;
	bool credentialStatePresent = false;
	bool profileQuarantinePresent = false;
	try
	{
		std::error_code error;
		for (std::filesystem::directory_iterator iterator(
				campaignDirectory, error), end;
			!error && iterator != end; iterator.increment(error))
		{
			const std::filesystem::path path = iterator->path();
			const std::string name = path.filename().string();
			if (path == profileDirectory)
			{
				const std::filesystem::file_status status =
					std::filesystem::symlink_status(path, error);
				if (error || std::filesystem::is_symlink(status) ||
					!std::filesystem::is_directory(status))
					return CampaignDirectoryScan::Unsafe;
				continue;
			}
			if (name == CampaignIdentityFileName)
			{
				if (identityPresent || !SafeRegularPath(path))
					return CampaignDirectoryScan::Unsafe;
				identityPresent = true;
				continue;
			}
			if (name == CheckpointAFileName || name == CheckpointBFileName)
			{
				if (!SafeRegularPath(path))
					return CampaignDirectoryScan::Unsafe;
				checkpointPresent = true;
				continue;
			}
			if (name == ReconnectCredentialFileName ||
				name == RetiredReconnectCredentialFileName ||
				name == ReconnectCredentialStagingFileName)
			{
				if (!SafeRegularPath(path))
					return CampaignDirectoryScan::Unsafe;
				credentialStatePresent = true;
				continue;
			}
			if (ProfileQuarantineName(name))
			{
				if (!SafePrivateDirectoryPath(path))
					return CampaignDirectoryScan::Unsafe;
				profileQuarantinePresent = true;
				continue;
			}
			return CampaignDirectoryScan::Unsafe;
		}
		if (error) return CampaignDirectoryScan::Unsafe;
		if (identityPresent) return CampaignDirectoryScan::IdentityPresent;
		bool profileEmpty = false;
		if (checkpointPresent || credentialStatePresent ||
			profileQuarantinePresent ||
			!ProfileAllowlist(profileDirectory, false, profileEmpty) ||
			!profileEmpty)
			return CampaignDirectoryScan::Unsafe;
		return CampaignDirectoryScan::CleanWithoutIdentity;
	}
	catch (...)
	{
		return CampaignDirectoryScan::Unsafe;
	}
}

enum class IdentityFileResult
{
	Ready,
	Mismatch,
	Unsafe,
	StorageFailure
};

IdentityFileResult ValidateOrCreateIdentityFile(
	NativeDirectory campaignDirectory,
	const std::filesystem::path& campaignPath,
	const CoopCampaignIdentitySha256& expected,
	bool mustExist) noexcept
{
	bool created = false;
	NativeFile file = OpenRelativeFile(campaignDirectory, campaignPath,
		CampaignIdentityFileName, !mustExist, !mustExist, created);
	if (file == InvalidNativeFile)
		return mustExist ? IdentityFileResult::Unsafe
			: IdentityFileResult::StorageFailure;
	NativeFileIdentity identity{};
	std::uint64_t size = 0;
	bool valid = CaptureIdentity(file, identity) &&
		PathNamesIdentity(campaignDirectory, campaignPath,
			CampaignIdentityFileName, identity);
	if (created)
	{
		valid = valid && WriteNativeAt(file, 0, expected.data(), expected.size()) &&
			TruncateNative(file, expected.size()) && SyncNativeFile(file) &&
			NativeFileSize(file, size) && size == expected.size() &&
			PathNamesIdentity(campaignDirectory, campaignPath,
				CampaignIdentityFileName, identity);
	}
	else
	{
		CoopCampaignIdentitySha256 stored{};
		valid = valid && NativeFileSize(file, size) && size == stored.size() &&
			ReadNativeAt(file, 0, stored.data(), stored.size()) &&
			PathNamesIdentity(campaignDirectory, campaignPath,
				CampaignIdentityFileName, identity);
		if (valid && stored != expected)
		{
			const bool closed = CloseNativeFile(file);
			(void)closed;
			return IdentityFileResult::Mismatch;
		}
	}
	const bool closed = CloseNativeFile(file);
	if (!valid || !closed) return IdentityFileResult::Unsafe;
	if (created && !SyncNativeDirectory(campaignDirectory))
		return IdentityFileResult::StorageFailure;
	return IdentityFileResult::Ready;
}

bool HashNativeFile(NativeFile file, const NativeFileIdentity& identity,
	NativeDirectory directory, const std::filesystem::path& directoryPath,
	const char* name, std::uint64_t expectedSize,
	DedicatedCampaignCheckpointSha256& digest) noexcept
{
	std::uint64_t size = 0;
	if (!SafeRegularFile(file) || !NativeFileSize(file, size) ||
		size != expectedSize ||
		!PathNamesIdentity(directory, directoryPath, name, identity))
		return false;
	Sha256 hasher;
	std::array<std::uint8_t, HashReadBytes> bytes{};
	std::uint64_t offset = 0;
	while (offset < expectedSize)
	{
		const std::size_t count = static_cast<std::size_t>(
			std::min<std::uint64_t>(bytes.size(), expectedSize - offset));
		if (!ReadNativeAt(file, offset, bytes.data(), count)) return false;
		hasher.update(bytes.data(), count);
		offset += count;
	}
	NativeFileIdentity confirmed{};
	if (!CaptureIdentity(file, confirmed) || !SameIdentity(identity, confirmed) ||
		!NativeFileSize(file, size) || size != expectedSize ||
		!PathNamesIdentity(directory, directoryPath, name, identity))
		return false;
	digest = hasher.finish();
	return true;
}
}

struct FullEngineCoopClientCampaignScratch::Impl
{
	DedicatedCampaignFilesystemBackend backend;
	std::unique_ptr<DedicatedCampaignSaveAdapter> adapter;
	CoopCampaignBootstrapDescriptor bootstrap{};
	NativeDirectory campaignDirectory = InvalidNativeDirectory;
	NativeFile staging = InvalidNativeFile;
	NativeFileIdentity stagingIdentity{};
	CoopCampaignSyncMetadata stagingMetadata{};
	CoopCampaignSyncMetadata activeMetadata{};
	DedicatedCampaignSlot stagingSlot = DedicatedCampaignSlot::A;
	std::uint64_t written = 0;
	DedicatedCampaignSlot activeSlot = DedicatedCampaignSlot::A;
	std::uint64_t activeGeneration = 0;
	bool transferActive = false;
	bool hasActive = false;
	bool failStopped = false;

	~Impl() noexcept
	{
		if (staging != InvalidNativeFile) (void)CloseNativeFile(staging);
		adapter.reset();
		if (campaignDirectory != InvalidNativeDirectory)
			(void)CloseNativeDirectory(campaignDirectory);
		backend.close();
	}
};

namespace
{
template<typename State>
bool OpenFreshStaging(
	State& state,
	DedicatedCampaignSlot slot) noexcept
{
	const char* const name = CheckpointName(slot);
	const std::filesystem::path& path = state.backend.checkpointPath(slot);
	if (path.empty() || path.parent_path() != state.backend.campaignDirectory() ||
		path.filename() != name)
		return false;
	bool created = false;
	NativeFile file = OpenRelativeFile(state.campaignDirectory,
		state.backend.campaignDirectory(), name, true, true, created);
	NativeFileIdentity identity{};
	std::uint64_t size = 0;
	const bool valid = file != InvalidNativeFile &&
		CaptureIdentity(file, identity) && NativeFileSize(file, size) &&
		size <= DedicatedCampaignMaximumCheckpointBytes &&
		PathNamesIdentity(state.campaignDirectory,
			state.backend.campaignDirectory(), name, identity) &&
		TruncateNative(file, 0) && SyncNativeFile(file) &&
		NativeFileSize(file, size) && size == 0 &&
		PathNamesIdentity(state.campaignDirectory,
			state.backend.campaignDirectory(), name, identity);
	if (!valid)
	{
		(void)CloseNativeFile(file);
		return false;
	}
	state.staging = file;
	state.stagingIdentity = identity;
	return true;
}

template<typename State>
bool RollbackWrite(State& state,
	std::uint64_t size) noexcept
{
	std::uint64_t confirmed = 0;
	return state.staging != InvalidNativeFile &&
		SafeRegularFile(state.staging) &&
		PathNamesIdentity(state.campaignDirectory,
			state.backend.campaignDirectory(), CheckpointName(state.stagingSlot),
			state.stagingIdentity) &&
		TruncateNative(state.staging, size) &&
		NativeFileSize(state.staging, confirmed) && confirmed == size;
}

template<typename State>
void ClearTransfer(State& state) noexcept
{
	state.staging = InvalidNativeFile;
	state.stagingIdentity = {};
	state.stagingMetadata = {};
	state.stagingSlot = DedicatedCampaignSlot::A;
	state.written = 0;
	state.transferActive = false;
}

template<typename State>
void AbortTransfer(State& state) noexcept
{
	if (!state.transferActive) return;
	NativeFile file = state.staging;
	bool opened = false;
	if (file == InvalidNativeFile)
	{
		bool created = false;
		file = OpenRelativeFile(state.campaignDirectory,
			state.backend.campaignDirectory(), CheckpointName(state.stagingSlot),
			true, false, created);
		opened = file != InvalidNativeFile;
		NativeFileIdentity identity{};
		if (!opened || !CaptureIdentity(file, identity) ||
			!SameIdentity(identity, state.stagingIdentity))
		{
			(void)CloseNativeFile(file);
			state.failStopped = true;
			ClearTransfer(state);
			return;
		}
	}
	std::uint64_t size = 1;
	const bool truncated = SafeRegularFile(file) &&
		PathNamesIdentity(state.campaignDirectory,
			state.backend.campaignDirectory(), CheckpointName(state.stagingSlot),
			state.stagingIdentity) &&
		TruncateNative(file, 0) && SyncNativeFile(file) &&
		NativeFileSize(file, size) && size == 0;
	if (!CloseNativeFile(file) || !truncated) state.failStopped = true;
	(void)opened;
	ClearTransfer(state);
}
}

FullEngineCoopClientCampaignScratch::FullEngineCoopClientCampaignScratch()
	noexcept = default;

FullEngineCoopClientCampaignScratch::~FullEngineCoopClientCampaignScratch()
	noexcept
{
	close();
}

FullEngineCoopClientCampaignScratchPrepareResult
FullEngineCoopClientCampaignScratch::prepare(
	const std::filesystem::path& absoluteStateRoot,
	const CoopCampaignBootstrapDescriptor& bootstrap) noexcept
{
	if (impl_)
		return FullEngineCoopClientCampaignScratchPrepareResult::AlreadyPrepared;
	if (!IsValidCoopCampaignBootstrapDescriptor(bootstrap) ||
		ZeroDigest(bootstrap.campaignIdentitySha256) ||
		absoluteStateRoot.empty() || !absoluteStateRoot.is_absolute())
		return FullEngineCoopClientCampaignScratchPrepareResult::InvalidConfiguration;
	try
	{
		std::unique_ptr<Impl> prepared(new Impl);
		const DedicatedCampaignFilesystemError opened = prepared->backend.open(
			absoluteStateRoot, DerivedCampaignId(bootstrap.campaignIdentitySha256));
		if (opened != DedicatedCampaignFilesystemError::None)
		{
			if (opened == DedicatedCampaignFilesystemError::LockHeld)
				return FullEngineCoopClientCampaignScratchPrepareResult::LeaseHeld;
			if (opened == DedicatedCampaignFilesystemError::InvalidStateRoot ||
				opened == DedicatedCampaignFilesystemError::InvalidCampaignId)
				return FullEngineCoopClientCampaignScratchPrepareResult::InvalidConfiguration;
			if (opened == DedicatedCampaignFilesystemError::UnsafeManagedPath)
				return FullEngineCoopClientCampaignScratchPrepareResult::UnsafeProfile;
			return FullEngineCoopClientCampaignScratchPrepareResult::StorageFailure;
		}

		prepared->campaignDirectory = OpenNativeDirectory(
			prepared->backend.campaignDirectory());
		if (prepared->campaignDirectory == InvalidNativeDirectory)
			return FullEngineCoopClientCampaignScratchPrepareResult::UnsafeProfile;
		const CampaignDirectoryScan scan = ScanCampaignDirectory(
			prepared->backend.campaignDirectory(),
			prepared->backend.profileDirectory());
		if (scan == CampaignDirectoryScan::Unsafe)
			return FullEngineCoopClientCampaignScratchPrepareResult::UnsafeProfile;
		const IdentityFileResult identity = ValidateOrCreateIdentityFile(
			prepared->campaignDirectory, prepared->backend.campaignDirectory(),
			bootstrap.campaignIdentitySha256,
			scan == CampaignDirectoryScan::IdentityPresent);
		if (identity == IdentityFileResult::Mismatch)
			return FullEngineCoopClientCampaignScratchPrepareResult::IdentityMismatch;
		if (identity == IdentityFileResult::Unsafe)
			return FullEngineCoopClientCampaignScratchPrepareResult::UnsafeProfile;
		if (identity != IdentityFileResult::Ready)
			return FullEngineCoopClientCampaignScratchPrepareResult::StorageFailure;
		const PrivateFileRemoveResult staleCredentialStaging =
			RemovePrivateFile(prepared->campaignDirectory,
				prepared->backend.campaignDirectory(),
				ReconnectCredentialStagingFileName);
		if (staleCredentialStaging == PrivateFileRemoveResult::Unsafe)
			return FullEngineCoopClientCampaignScratchPrepareResult::UnsafeProfile;
		if (staleCredentialStaging == PrivateFileRemoveResult::Failure)
			return FullEngineCoopClientCampaignScratchPrepareResult::StorageFailure;

		// This profile is only a passive VFS/load workspace. Once the complete
		// campaign identity above has been exact-checked, none of its prior bytes
		// are authority: checkpoints and reconnect/retirement evidence live in the
		// held parent directory. Atomically quarantine the complete old tree before
		// VFS discovery instead of traversing or trusting legacy-created Temp,
		// ShadeTables, settings, aliases, or partial scratch files.
		const DedicatedCampaignProfileDirectoryState profileState =
			prepared->backend.profileDirectoryState();
		if (profileState == DedicatedCampaignProfileDirectoryState::Failure)
			return FullEngineCoopClientCampaignScratchPrepareResult::UnsafeProfile;
		if (profileState == DedicatedCampaignProfileDirectoryState::NonEmpty)
		{
			const DedicatedCampaignProfileRecoveryResult reset =
				prepared->backend.recoverProfileForNewCampaign();
			if (reset ==
				DedicatedCampaignProfileRecoveryResult::CommittedStatePresent)
				return FullEngineCoopClientCampaignScratchPrepareResult::UnsafeProfile;
			if (reset != DedicatedCampaignProfileRecoveryResult::Ready)
				return FullEngineCoopClientCampaignScratchPrepareResult::StorageFailure;
		}

		bool profileEmpty = false;
		if (!ProfileAllowlist(prepared->backend.profileDirectory(), false,
				profileEmpty) || !profileEmpty)
			return FullEngineCoopClientCampaignScratchPrepareResult::UnsafeProfile;
		prepared->adapter.reset(new DedicatedCampaignSaveAdapter(
			prepared->backend.profileDirectory()));
		if (!prepared->adapter->prepareLogicalScratchFiles())
			return FullEngineCoopClientCampaignScratchPrepareResult::StorageFailure;
		if (!ProfileAllowlist(prepared->backend.profileDirectory(), true,
				profileEmpty))
			return FullEngineCoopClientCampaignScratchPrepareResult::UnsafeProfile;
		prepared->bootstrap = bootstrap;
		impl_ = std::move(prepared);
		return FullEngineCoopClientCampaignScratchPrepareResult::Success;
	}
	catch (...)
	{
		return FullEngineCoopClientCampaignScratchPrepareResult::StorageFailure;
	}
}

void FullEngineCoopClientCampaignScratch::close() noexcept
{
	if (!impl_) return;
	AbortTransfer(*impl_);
	impl_->adapter.reset();
	if (impl_->campaignDirectory != InvalidNativeDirectory)
	{
		(void)CloseNativeDirectory(impl_->campaignDirectory);
		impl_->campaignDirectory = InvalidNativeDirectory;
	}
	// The backend is deliberately last: its reset releases the process lease
	// only after callers have torn down VFS and this object has retired every
	// profile/checkpoint handle.
	impl_->backend.close();
	impl_.reset();
}

bool FullEngineCoopClientCampaignScratch::prepared() const noexcept
{
	return impl_ && impl_->backend.isOpen() && impl_->adapter != nullptr;
}

bool FullEngineCoopClientCampaignScratch::failStopped() const noexcept
{
	return impl_ && impl_->failStopped;
}

const std::filesystem::path&
FullEngineCoopClientCampaignScratch::profileDirectory() const noexcept
{
	return prepared() ? impl_->backend.profileDirectory() : EmptyPath();
}

bool FullEngineCoopClientCampaignScratch::hasActiveCheckpoint() const noexcept
{
	return impl_ && impl_->hasActive;
}

DedicatedCampaignSlot FullEngineCoopClientCampaignScratch::activeSlot()
	const noexcept
{
	return impl_ && impl_->hasActive
		? impl_->activeSlot : DedicatedCampaignSlot::A;
}

std::uint64_t FullEngineCoopClientCampaignScratch::activeGeneration()
	const noexcept
{
	return impl_ && impl_->hasActive ? impl_->activeGeneration : 0;
}

FullEngineCoopReconnectCredentialLoadResult
FullEngineCoopClientCampaignScratch::loadReconnectCredential(
	AdmissionAck& credential) noexcept
{
	if (!impl_ || !prepared() || impl_->failStopped)
		return FullEngineCoopReconnectCredentialLoadResult::InvalidState;
	const std::filesystem::path& campaign =
		impl_->backend.campaignDirectory();
	const PrivateFileEntryState activeEntry = CredentialEntryState(
		impl_->campaignDirectory, campaign, ReconnectCredentialFileName);
	const PrivateFileEntryState retiredEntry = CredentialEntryState(
		impl_->campaignDirectory, campaign,
		RetiredReconnectCredentialFileName);
	if (activeEntry == PrivateFileEntryState::Unsafe ||
		retiredEntry == PrivateFileEntryState::Unsafe ||
		(activeEntry == PrivateFileEntryState::Safe &&
		 retiredEntry == PrivateFileEntryState::Safe))
	{
		impl_->failStopped = true;
		return FullEngineCoopReconnectCredentialLoadResult::UnsafeStorage;
	}
	if (activeEntry == PrivateFileEntryState::Failure ||
		retiredEntry == PrivateFileEntryState::Failure)
		return FullEngineCoopReconnectCredentialLoadResult::StorageFailure;
	if (activeEntry == PrivateFileEntryState::Missing &&
		retiredEntry == PrivateFileEntryState::Missing)
		return FullEngineCoopReconnectCredentialLoadResult::Missing;
	const bool terminal = retiredEntry == PrivateFileEntryState::Safe;
	const char* const name = terminal
		? RetiredReconnectCredentialFileName : ReconnectCredentialFileName;

	bool created = false;
	NativeFile file = OpenRelativeFile(impl_->campaignDirectory, campaign,
		name, false, false, created);
	NativeFileIdentity identity{};
	std::uint64_t size = 0;
	if (file == InvalidNativeFile || !SafePrivateCredentialFile(file) ||
		!CaptureIdentity(file, identity) ||
		!PathNamesIdentity(impl_->campaignDirectory, campaign,
			name, identity))
	{
		(void)CloseNativeFile(file);
		impl_->failStopped = true;
		return FullEngineCoopReconnectCredentialLoadResult::UnsafeStorage;
	}
	if (!NativeFileSize(file, size))
	{
		(void)CloseNativeFile(file);
		return FullEngineCoopReconnectCredentialLoadResult::StorageFailure;
	}
	if (size != ReconnectCredentialRecordBytes)
	{
		(void)CloseNativeFile(file);
		return FullEngineCoopReconnectCredentialLoadResult::CorruptRecord;
	}
	ReconnectCredentialRecord record{};
	const bool read = ReadNativeAt(file, 0, record.data(), record.size());
	std::uint64_t confirmedSize = 0;
	NativeFileIdentity confirmedIdentity{};
	const bool unchanged = read && SafePrivateCredentialFile(file) &&
		CaptureIdentity(file, confirmedIdentity) &&
		SameIdentity(identity, confirmedIdentity) &&
		NativeFileSize(file, confirmedSize) && confirmedSize == record.size() &&
		PathNamesIdentity(impl_->campaignDirectory, campaign,
			name, identity);
	const bool closed = CloseNativeFile(file);
	if (!read || !closed)
		return FullEngineCoopReconnectCredentialLoadResult::StorageFailure;
	if (!unchanged)
	{
		impl_->failStopped = true;
		return FullEngineCoopReconnectCredentialLoadResult::UnsafeStorage;
	}

	CoopCampaignBootstrapDescriptor storedBootstrap;
	AdmissionAck storedCredential;
	if (!DecodeReconnectCredentialRecord(
		record, storedBootstrap, storedCredential))
		return FullEngineCoopReconnectCredentialLoadResult::CorruptRecord;
	const bool exactBootstrap = SameCoopCampaignBootstrapDescriptor(
		storedBootstrap, impl_->bootstrap);
	const bool exactExceptEpoch =
		storedBootstrap.sessionEpoch != impl_->bootstrap.sessionEpoch &&
		SameBootstrapExceptSessionEpoch(storedBootstrap, impl_->bootstrap);
	if (terminal && (exactBootstrap || exactExceptEpoch))
	{
		// Preserve the invalidated bytes as evidence. The caller normally does
		// not need them, but returning the exact record makes retirement
		// idempotence checkable without weakening marker validation.
		credential = storedCredential;
		return FullEngineCoopReconnectCredentialLoadResult::Retired;
	}
	if (exactBootstrap)
	{
		credential = storedCredential;
		return FullEngineCoopReconnectCredentialLoadResult::Loaded;
	}
	if (exactExceptEpoch)
		return FullEngineCoopReconnectCredentialLoadResult::StaleSession;
	return FullEngineCoopReconnectCredentialLoadResult::BindingMismatch;
}

bool FullEngineCoopClientCampaignScratch::persistReconnectCredential(
	const AdmissionAck& credential) noexcept
{
	if (!impl_ || !prepared() || impl_->failStopped ||
		credential.protocolVersion != impl_->bootstrap.protocolVersion ||
		credential.sessionEpoch != impl_->bootstrap.sessionEpoch)
		return false;
	ReconnectCredentialRecord record{};
	if (!EncodeReconnectCredentialRecord(
		impl_->bootstrap, credential, record))
		return false;
	AdmissionAck existing;
	const FullEngineCoopReconnectCredentialLoadResult existingResult =
		loadReconnectCredential(existing);
	if (existingResult ==
			FullEngineCoopReconnectCredentialLoadResult::Loaded &&
		existing.protocolVersion == credential.protocolVersion &&
		existing.sessionEpoch == credential.sessionEpoch &&
		existing.peerIdentity == credential.peerIdentity &&
		existing.reconnectToken == credential.reconnectToken)
		return true;
	if (existingResult !=
			FullEngineCoopReconnectCredentialLoadResult::Missing &&
		existingResult !=
			FullEngineCoopReconnectCredentialLoadResult::Loaded)
		return false;

	const std::filesystem::path& campaign =
		impl_->backend.campaignDirectory();
	const PrivateFileRemoveResult removed = RemovePrivateFile(
		impl_->campaignDirectory, campaign,
		ReconnectCredentialStagingFileName);
	if (removed == PrivateFileRemoveResult::Unsafe ||
		removed == PrivateFileRemoveResult::Failure)
	{
		impl_->failStopped = true;
		return false;
	}

	NativeFile staging = CreateExclusivePrivateFile(
		impl_->campaignDirectory, campaign,
		ReconnectCredentialStagingFileName);
	NativeFileIdentity stagingIdentity{};
	std::uint64_t size = 0;
	bool valid = staging != InvalidNativeFile &&
		SafePrivateCredentialFile(staging) &&
		CaptureIdentity(staging, stagingIdentity) &&
		PathNamesIdentity(impl_->campaignDirectory, campaign,
			ReconnectCredentialStagingFileName, stagingIdentity) &&
		WriteNativeAt(staging, 0, record.data(), record.size()) &&
		TruncateNative(staging, record.size()) && SyncNativeFile(staging) &&
		NativeFileSize(staging, size) && size == record.size() &&
		SafePrivateCredentialFile(staging) &&
		PathNamesIdentity(impl_->campaignDirectory, campaign,
			ReconnectCredentialStagingFileName, stagingIdentity);
	if (!CloseNativeFile(staging)) valid = false;
	if (!valid)
	{
		const PrivateFileRemoveResult cleanup = RemovePrivateFile(
			impl_->campaignDirectory, campaign,
			ReconnectCredentialStagingFileName);
		if (cleanup == PrivateFileRemoveResult::Unsafe ||
			cleanup == PrivateFileRemoveResult::Failure)
			impl_->failStopped = true;
		return false;
	}
	if (!ReplacePrivateFile(impl_->campaignDirectory, campaign,
		ReconnectCredentialStagingFileName, ReconnectCredentialFileName,
		stagingIdentity))
	{
		(void)RemovePrivateFile(impl_->campaignDirectory, campaign,
			ReconnectCredentialStagingFileName);
		impl_->failStopped = true;
		return false;
	}
	return true;
}

bool FullEngineCoopClientCampaignScratch::retireReconnectCredential(
	const AdmissionAck& credential) noexcept
{
	if (!impl_ || !prepared() || impl_->failStopped) return false;
	AdmissionAck stored;
	const FullEngineCoopReconnectCredentialLoadResult loaded =
		loadReconnectCredential(stored);
	const bool exactCredential =
		stored.protocolVersion == credential.protocolVersion &&
		stored.sessionEpoch == credential.sessionEpoch &&
		stored.peerIdentity == credential.peerIdentity &&
		stored.reconnectToken == credential.reconnectToken;
	if (loaded == FullEngineCoopReconnectCredentialLoadResult::Retired)
		return exactCredential;
	if (loaded != FullEngineCoopReconnectCredentialLoadResult::Loaded ||
		!exactCredential)
		return false;
	if (!RetirePrivateFileNoReplace(impl_->campaignDirectory,
		impl_->backend.campaignDirectory(), ReconnectCredentialFileName,
		RetiredReconnectCredentialFileName))
	{
		// Whether the exclusive rename itself failed or only its durability
		// barrier failed, this process must not resume play. A new process will
		// validate either the old bearer or the terminal marker.
		impl_->failStopped = true;
		return false;
	}
	return true;
}

bool FullEngineCoopClientCampaignScratch::eraseStaleReconnectCredential()
	noexcept
{
	if (!impl_ || !prepared() || impl_->failStopped) return false;
	AdmissionAck ignored;
	if (loadReconnectCredential(ignored) !=
		FullEngineCoopReconnectCredentialLoadResult::StaleSession)
		return false;
	const PrivateFileRemoveResult removed = RemovePrivateFile(
		impl_->campaignDirectory, impl_->backend.campaignDirectory(),
		ReconnectCredentialFileName);
	if (removed == PrivateFileRemoveResult::Removed)
		return true;
	if (removed == PrivateFileRemoveResult::Unsafe)
		impl_->failStopped = true;
	return false;
}

FullEngineCoopCampaignScratchBeginResult
FullEngineCoopClientCampaignScratch::begin(
	const CoopCampaignSyncMetadata& metadata) noexcept
{
	if (!impl_ || !prepared() || impl_->failStopped)
		return FullEngineCoopCampaignScratchBeginResult::StorageFailure;
	if (metadata.transfer.totalSize > DedicatedCampaignMaximumCheckpointBytes)
		return FullEngineCoopCampaignScratchBeginResult::CapacityReached;
	if (!IsValidCoopCampaignSyncTransferIdentity(metadata.transfer) ||
		!TransferMatchesBootstrap(metadata.transfer, impl_->bootstrap) ||
		metadata.transfer.totalSize == 0 ||
		(impl_->hasActive &&
			(metadata.transfer.checkpointGeneration < impl_->activeGeneration ||
			 (metadata.transfer.checkpointGeneration == impl_->activeGeneration &&
			  !SameCheckpointMetadata(metadata, impl_->activeMetadata)))))
		return FullEngineCoopCampaignScratchBeginResult::StorageFailure;
	AbortTransfer(*impl_);
	if (impl_->failStopped)
		return FullEngineCoopCampaignScratchBeginResult::StorageFailure;
	const DedicatedCampaignSlot stagingSlot = impl_->hasActive
		? OtherSlot(impl_->activeSlot) : DedicatedCampaignSlot::A;
	if (!OpenFreshStaging(*impl_, stagingSlot))
		return FullEngineCoopCampaignScratchBeginResult::StorageFailure;
	impl_->stagingMetadata = metadata;
	impl_->stagingSlot = stagingSlot;
	impl_->written = 0;
	impl_->transferActive = true;
	return FullEngineCoopCampaignScratchBeginResult::Success;
}

FullEngineCoopCampaignScratchWriteResult
FullEngineCoopClientCampaignScratch::writeExact(
	std::uint64_t offset, const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	if (!impl_ || impl_->failStopped || !impl_->transferActive ||
		impl_->staging == InvalidNativeFile || bytes == nullptr || size == 0 ||
		offset != impl_->written ||
		offset > impl_->stagingMetadata.transfer.totalSize ||
		size > impl_->stagingMetadata.transfer.totalSize - offset)
		return FullEngineCoopCampaignScratchWriteResult::StorageFailure;
	std::uint64_t currentSize = 0;
	if (!SafeRegularFile(impl_->staging) ||
		!NativeFileSize(impl_->staging, currentSize) ||
		currentSize != impl_->written ||
		!PathNamesIdentity(impl_->campaignDirectory,
			impl_->backend.campaignDirectory(), CheckpointName(impl_->stagingSlot),
			impl_->stagingIdentity))
		return FullEngineCoopCampaignScratchWriteResult::StorageFailure;
	if (!WriteNativeAt(impl_->staging, offset, bytes, size) ||
		!NativeFileSize(impl_->staging, currentSize) ||
		currentSize != offset + size ||
		!SafeRegularFile(impl_->staging) ||
		!PathNamesIdentity(impl_->campaignDirectory,
			impl_->backend.campaignDirectory(), CheckpointName(impl_->stagingSlot),
			impl_->stagingIdentity))
	{
		if (!RollbackWrite(*impl_, impl_->written)) impl_->failStopped = true;
		return FullEngineCoopCampaignScratchWriteResult::StorageFailure;
	}
	impl_->written += size;
	return FullEngineCoopCampaignScratchWriteResult::Success;
}

FullEngineCoopCampaignScratchCommitResult
FullEngineCoopClientCampaignScratch::commitAndLoad(
	const CoopCampaignSyncMetadata& metadata) noexcept
{
	if (!impl_ || impl_->failStopped || !impl_->transferActive ||
		impl_->adapter == nullptr)
		return FullEngineCoopCampaignScratchCommitResult::StorageFailure;
	if (!SameMetadata(metadata, impl_->stagingMetadata))
		return FullEngineCoopCampaignScratchCommitResult::CompatibilityMismatch;
	if (impl_->written != metadata.transfer.totalSize)
		return FullEngineCoopCampaignScratchCommitResult::HashMismatch;
	if (impl_->staging == InvalidNativeFile ||
		!SyncNativeFile(impl_->staging))
		return FullEngineCoopCampaignScratchCommitResult::StorageFailure;
	DedicatedCampaignCheckpointSha256 digest{};
	if (!HashNativeFile(impl_->staging, impl_->stagingIdentity,
			impl_->campaignDirectory, impl_->backend.campaignDirectory(),
			CheckpointName(impl_->stagingSlot), metadata.transfer.totalSize,
			digest))
		return FullEngineCoopCampaignScratchCommitResult::StorageFailure;
	if (digest != metadata.transfer.checkpointSha256)
		return FullEngineCoopCampaignScratchCommitResult::HashMismatch;
	const NativeFile closing = impl_->staging;
	impl_->staging = InvalidNativeFile;
	if (!CloseNativeFile(closing))
		return FullEngineCoopCampaignScratchCommitResult::StorageFailure;
	if (!impl_->adapter->materializeCheckpoint(impl_->stagingSlot,
			impl_->backend.checkpointPath(impl_->stagingSlot),
			metadata.transfer.totalSize, metadata.transfer.checkpointSha256))
		return FullEngineCoopCampaignScratchCommitResult::StorageFailure;
	if (!ValidateDedicatedCampaignGame(impl_->stagingSlot))
		return FullEngineCoopCampaignScratchCommitResult::CompatibilityMismatch;
	if (!LoadDedicatedCampaignGame(impl_->stagingSlot))
	{
		impl_->failStopped = true;
		return FullEngineCoopCampaignScratchCommitResult::LoadFailed;
	}
	impl_->activeSlot = impl_->stagingSlot;
	impl_->activeGeneration = metadata.transfer.checkpointGeneration;
	impl_->activeMetadata = metadata;
	impl_->hasActive = true;
	ClearTransfer(*impl_);
	return FullEngineCoopCampaignScratchCommitResult::Committed;
}

void FullEngineCoopClientCampaignScratch::abort() noexcept
{
	if (impl_) AbortTransfer(*impl_);
}
}
