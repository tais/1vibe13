#include "DedicatedCampaignSaveAdapter.h"

#include "DedicatedCampaignSaveBridge.h"

#include <array>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace
{
constexpr std::size_t CopyBufferBytes = 64u * 1024u;

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
