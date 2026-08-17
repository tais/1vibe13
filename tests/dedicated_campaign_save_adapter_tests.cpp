#include "Ja2/DedicatedCampaignSaveAdapter.h"
#include "Ja2/DedicatedCampaignSaveBridge.h"

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace
{
std::filesystem::path ActiveProfile;
bool SaveResult = true;
bool ValidateResult = true;
unsigned SaveCalls = 0;
unsigned ValidateCalls = 0;
DedicatedCampaignSlot LastSaveSlot = DedicatedCampaignSlot::A;
DedicatedCampaignSlot LastValidateSlot = DedicatedCampaignSlot::A;
std::vector<std::uint8_t> SaveBytes{'c', 'a', 'm', 'p', 'a', 'i', 'g', 'n'};

void Check(bool condition, const char* message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

struct FileIdentity
{
	std::uint64_t first = 0;
	std::uint64_t second = 0;
};

bool operator==(const FileIdentity& left, const FileIdentity& right) noexcept
{
	return left.first == right.first && left.second == right.second;
}

bool ReadFileIdentity(const std::filesystem::path& path,
	FileIdentity& identity) noexcept
{
#ifdef _WIN32
	const HANDLE file = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;
	BY_HANDLE_FILE_INFORMATION information{};
	const bool success = GetFileInformationByHandle(file, &information) != FALSE;
	(void)CloseHandle(file);
	if (!success) return false;
	identity.first = information.dwVolumeSerialNumber;
	identity.second = (static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) |
		information.nFileIndexLow;
	return true;
#else
	struct stat status{};
	if (::lstat(path.c_str(), &status) != 0) return false;
	identity.first = static_cast<std::uint64_t>(status.st_dev);
	identity.second = static_cast<std::uint64_t>(status.st_ino);
	return true;
#endif
}

class TemporaryRoot
{
public:
	TemporaryRoot()
	{
		const auto seed = static_cast<unsigned long long>(
			std::chrono::steady_clock::now().time_since_epoch().count());
		for (unsigned attempt = 0; attempt < 256; ++attempt)
		{
			path_ = std::filesystem::temp_directory_path() /
				("ja2-campaign-save-adapter-" + std::to_string(seed) + "-" +
					std::to_string(attempt));
			std::error_code error;
			if (std::filesystem::create_directory(path_, error)) return;
		}
		Check(false, "temporary root can be created");
	}

	~TemporaryRoot()
	{
		std::error_code error;
		std::filesystem::remove_all(path_, error);
	}

	const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_;
};

void WriteBytes(const std::filesystem::path& path,
	const std::vector<std::uint8_t>& bytes)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char*>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	output.close();
	Check(static_cast<bool>(output), "fixture writes completely");
}

std::vector<std::uint8_t> ReadBytes(const std::filesystem::path& path)
{
	std::ifstream input(path, std::ios::binary);
	return std::vector<std::uint8_t>(
		std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void ResetBridge()
{
	SaveResult = true;
	ValidateResult = true;
	SaveCalls = 0;
	ValidateCalls = 0;
	SaveBytes = {'c', 'a', 'm', 'p', 'a', 'i', 'g', 'n'};
}

void TestFixedSlotCopy()
{
	TemporaryRoot root;
	const std::filesystem::path campaign = root.path() / "campaign-example";
	ActiveProfile = campaign / "profile";
	std::filesystem::create_directories(ActiveProfile);
	DedicatedCampaignSaveAdapter adapter(ActiveProfile);
	Check(adapter.profileDirectory() == ActiveProfile &&
		adapter.logicalScratchPath(DedicatedCampaignSlot::A).filename() ==
			"DedicatedCheckpointA.sav" &&
		adapter.logicalScratchPath(DedicatedCampaignSlot::B).filename() ==
			"DedicatedCheckpointB.sav" &&
		std::string(DedicatedCampaignLogicalScratch(
			DedicatedCampaignSlot::A)) == "DedicatedCheckpointA.sav" &&
		std::string(DedicatedCampaignLogicalScratch(
			DedicatedCampaignSlot::B)) == "DedicatedCheckpointB.sav",
		"only two fixed relative campaign scratch names are exposed");

	for (const DedicatedCampaignSlot slot :
		{DedicatedCampaignSlot::A, DedicatedCampaignSlot::B})
	{
		ResetBridge();
		const std::string base = slot == DedicatedCampaignSlot::A
			? "checkpoint-a.sav.pending.1" : "checkpoint-b.sav.pending.2";
		const std::filesystem::path staging = campaign / base;
		WriteBytes(staging, {'o', 'l', 'd'});
		FileIdentity before;
		FileIdentity after;
		std::error_code error;
		const std::uintmax_t linksBefore =
			std::filesystem::hard_link_count(staging, error);
		Check(!error && ReadFileIdentity(staging, before) &&
			adapter.writeCheckpoint(slot, staging) &&
			SaveCalls == 1 && ValidateCalls == 1 &&
			LastSaveSlot == slot && LastValidateSlot == slot &&
			ReadBytes(staging) == SaveBytes &&
			ReadFileIdentity(staging, after) && after == before &&
			std::filesystem::hard_link_count(staging, error) == linksBefore && !error,
			"slot scratch is semantically validated then copied into exact staging");
	}
}

void TestFailuresFailClosed()
{
	TemporaryRoot root;
	const std::filesystem::path campaign = root.path() / "campaign-example";
	ActiveProfile = campaign / "profile";
	std::filesystem::create_directories(ActiveProfile);
	DedicatedCampaignSaveAdapter adapter(ActiveProfile);
	const std::filesystem::path staging =
		campaign / "checkpoint-a.sav.pending.3";
	WriteBytes(staging, {'o', 'l', 'd'});

	ResetBridge();
	SaveResult = false;
	Check(!adapter.writeCheckpoint(DedicatedCampaignSlot::A, staging) &&
		SaveCalls == 1 && ValidateCalls == 0 &&
		ReadBytes(staging) == std::vector<std::uint8_t>({'o', 'l', 'd'}),
		"legacy save failure leaves reserved staging untouched");

	ResetBridge();
	ValidateResult = false;
	Check(!adapter.writeCheckpoint(DedicatedCampaignSlot::A, staging) &&
		SaveCalls == 1 && ValidateCalls == 1 &&
		ReadBytes(staging) == std::vector<std::uint8_t>({'o', 'l', 'd'}),
		"semantic preflight failure leaves reserved staging untouched");

	ResetBridge();
	Check(!adapter.writeCheckpoint(static_cast<DedicatedCampaignSlot>(0), staging) &&
		SaveCalls == 0 && ValidateCalls == 0 &&
		!adapter.writeCheckpoint(DedicatedCampaignSlot::A,
			ActiveProfile / "checkpoint-a.sav.pending.4") &&
		SaveCalls == 0 && ValidateCalls == 0,
		"unknown slots and staging outside the campaign parent fail before saving");

	ResetBridge();
	SaveBytes.clear();
	Check(!adapter.writeCheckpoint(DedicatedCampaignSlot::A, staging),
		"an empty checkpoint cannot enter campaign staging");

	ResetBridge();
	const std::filesystem::path outside = root.path() / "outside";
	WriteBytes(outside, {'x'});
	std::error_code error;
	std::filesystem::remove(staging, error);
	error.clear();
	std::filesystem::create_hard_link(outside, staging, error);
	Check(!error &&
		!adapter.writeCheckpoint(DedicatedCampaignSlot::A, staging) &&
		SaveCalls == 0 && ValidateCalls == 0 && ReadBytes(outside) ==
			std::vector<std::uint8_t>({'x'}),
		"a linked staging entry is rejected before legacy save or truncation");

	std::filesystem::remove(staging, error);
	const std::filesystem::path scratch =
		ActiveProfile / DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A);
	WriteBytes(outside, {'x'});
	std::filesystem::remove(scratch, error);
	error.clear();
	std::filesystem::create_hard_link(outside, scratch, error);
	ResetBridge();
	WriteBytes(staging, {'o', 'l', 'd'});
	Check(!error &&
		!adapter.writeCheckpoint(DedicatedCampaignSlot::A, staging) &&
		SaveCalls == 0 && ValidateCalls == 0 &&
		ReadBytes(outside) == std::vector<std::uint8_t>({'x'}),
		"a linked scratch entry is rejected before the legacy writer runs");
	std::filesystem::remove(scratch, error);

	error.clear();
	std::filesystem::create_directory(scratch, error);
	ResetBridge();
	Check(!error &&
		!adapter.writeCheckpoint(DedicatedCampaignSlot::A, staging) &&
		SaveCalls == 0 && ValidateCalls == 0,
		"a non-file scratch entry is rejected before the legacy writer runs");
	std::filesystem::remove(scratch, error);

#ifndef _WIN32
	error.clear();
	std::filesystem::create_symlink(outside, scratch, error);
	ResetBridge();
	Check(!error &&
		!adapter.writeCheckpoint(DedicatedCampaignSlot::A, staging) &&
		SaveCalls == 0 && ValidateCalls == 0 &&
		ReadBytes(outside) == std::vector<std::uint8_t>({'x'}),
		"a symlink scratch entry is rejected before the legacy writer runs");
#endif
}
}

bool SaveDedicatedCampaignGame(DedicatedCampaignSlot slot) noexcept
{
	++SaveCalls;
	LastSaveSlot = slot;
	if (!SaveResult) return false;
	try
	{
		WriteBytes(ActiveProfile / DedicatedCampaignLogicalScratch(slot), SaveBytes);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool ValidateDedicatedCampaignGame(DedicatedCampaignSlot slot) noexcept
{
	++ValidateCalls;
	LastValidateSlot = slot;
	return ValidateResult;
}

bool LoadDedicatedCampaignGame(DedicatedCampaignSlot) noexcept
{
	return false;
}

int main()
{
	TestFixedSlotCopy();
	TestFailuresFailClosed();
	std::puts("dedicated campaign save adapter tests passed");
	return 0;
}
