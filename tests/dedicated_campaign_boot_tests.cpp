#include "Ja2/DedicatedCampaignBoot.h"
#include "Ja2/DedicatedCampaignSaveBridge.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
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
std::vector<std::uint8_t> LegacySaveBytes{'l', 'i', 'v', 'e'};
bool LegacySaveResult = true;
bool LegacyValidateResult = true;
unsigned LegacySaveCalls = 0;
unsigned LegacyValidateCalls = 0;

void Check(bool condition, const char* message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
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
				("ja2-campaign-boot-" + std::to_string(seed) + "-" +
					std::to_string(attempt));
			std::error_code error;
			if (std::filesystem::create_directory(path_, error))
			{
#ifndef _WIN32
				Check(::chmod(path_.c_str(), S_IRWXU) == 0,
					"temporary campaign root becomes private");
#endif
				return;
			}
		}
		Check(false, "temporary campaign root can be created");
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
	identity.second =
		(static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) |
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

void WriteBytes(const std::filesystem::path& path,
	const std::vector<std::uint8_t>& bytes)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char*>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	output.close();
	Check(static_cast<bool>(output), "fixture bytes write completely");
}

std::vector<std::uint8_t> ReadBytes(const std::filesystem::path& path)
{
	std::ifstream input(path, std::ios::binary);
	return std::vector<std::uint8_t>(
		std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::uintmax_t FileSize(const std::filesystem::path& path)
{
	std::error_code error;
	const std::uintmax_t size = std::filesystem::file_size(path, error);
	Check(!error, "fixture file size is readable");
	return size;
}

DedicatedCampaignRuntimeFingerprint Fingerprint()
{
	return {9, UINT64_C(0x1020304050607080),
		UINT64_C(0x8877665544332211)};
}

DedicatedCampaignContentManifestSha256 ContentDigest()
{
	DedicatedCampaignContentManifestSha256 digest{};
	for (std::size_t index = 0; index < digest.size(); ++index)
		digest[index] = static_cast<std::uint8_t>(index + 1);
	return digest;
}

DedicatedCampaignIdentity Identity(const std::string& campaignId,
	std::uint64_t seed = UINT64_C(0xfedcba9876543210))
{
	DedicatedCampaignIdentity identity;
	identity.campaignId = campaignId;
	identity.mode = DedicatedCampaignMode::Coop;
	identity.runtimeFingerprint = Fingerprint();
	identity.contentManifestSha256 = ContentDigest();
	identity.campaignSeed = seed;
	return identity;
}

DedicatedServerOptions Options(const TemporaryRoot& root,
	const std::string& campaignId, DedicatedCampaignAction action,
	std::uint64_t seed = UINT64_C(0xfedcba9876543210))
{
	DedicatedServerOptions options;
	options.enabled = true;
	options.mode = DedicatedServerMode::Coop;
	options.campaignAction = action;
	options.campaignId = campaignId;
	options.campaignSeed = seed;
	options.stateDirectory = root.path().string();
	return options;
}

std::filesystem::path CampaignDirectory(const TemporaryRoot& root,
	const std::string& campaignId)
{
	return root.path() / "campaigns" / ("campaign-" + campaignId);
}

std::filesystem::path ProfileDirectory(const TemporaryRoot& root,
	const std::string& campaignId)
{
	return CampaignDirectory(root, campaignId) / "profile";
}

class FixtureWriter final : public DedicatedCampaignCheckpointWriter
{
public:
	bool writeCheckpoint(DedicatedCampaignSlot,
		const std::filesystem::path& path) noexcept override
	{
		try
		{
			WriteBytes(path, bytes);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	std::vector<std::uint8_t> bytes{'f', 'i', 'x', 't', 'u', 'r', 'e'};
};

void PublishFixture(const TemporaryRoot& root,
	const DedicatedCampaignIdentity& identity,
	const std::vector<std::vector<std::uint8_t>>& generations)
{
	FixtureWriter writer;
	DedicatedCampaignFilesystemBackend backend(writer);
	Check(backend.open(root.path(), identity.campaignId) ==
		DedicatedCampaignFilesystemError::None,
		"fixture backend opens");
	DedicatedCampaignStore store(backend);
	Check(store.create(identity) == DedicatedCampaignStoreError::None,
		"fixture campaign creates");
	std::uint64_t worldMinutes = 100;
	for (const std::vector<std::uint8_t>& generation : generations)
	{
		writer.bytes = generation;
		Check(store.checkpoint(worldMinutes++) ==
			DedicatedCampaignStoreError::None,
			"fixture checkpoint publishes");
	}
	backend.close();
}

void RewriteManifestSeed(const std::filesystem::path& manifestPath,
	std::uint64_t seed)
{
	DedicatedCampaignManifestBytes bytes{};
	std::ifstream input(manifestPath, std::ios::binary);
	input.read(reinterpret_cast<char*>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	Check(input.gcount() == static_cast<std::streamsize>(bytes.size()),
		"manifest fixture reads completely");
	DedicatedCampaignManifest manifest;
	Check(DecodeDedicatedCampaignManifest(bytes.data(), bytes.size(), manifest) ==
		DedicatedCampaignManifestDecodeError::None,
		"manifest fixture decodes");
	manifest.identity.campaignSeed = seed;
	Check(EncodeDedicatedCampaignManifest(manifest, bytes),
		"manifest fixture re-encodes");
	std::ofstream output(manifestPath, std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char*>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	output.close();
	Check(static_cast<bool>(output), "manifest fixture rewrites completely");
}

void ResetLegacyBridge()
{
	LegacySaveBytes = {'l', 'i', 'v', 'e'};
	LegacySaveResult = true;
	LegacyValidateResult = true;
	LegacySaveCalls = 0;
	LegacyValidateCalls = 0;
}

void TestCreateAndCheckpointLifecycle()
{
	TemporaryRoot root;
	DedicatedCampaignBoot boot;
	const DedicatedServerOptions options = Options(root, "create",
		DedicatedCampaignAction::Create, 0);
	Check(boot.prepare(options) &&
		boot.state() == DedicatedCampaignBootState::PreparedCreate &&
		boot.entry() == DedicatedCampaignBootEntry::None &&
		boot.campaignSeed() == 0 && !boot.profileDirectory().empty(),
		"create preparation holds its profile and zero seed");
	const std::filesystem::path scratchA = boot.profileDirectory() /
		DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A);
	const std::filesystem::path scratchB = boot.profileDirectory() /
		DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::B);
	FileIdentity scratchAIdentity;
	FileIdentity scratchBIdentity;
	Check(ReadFileIdentity(scratchA, scratchAIdentity) &&
		ReadFileIdentity(scratchB, scratchBIdentity) &&
		FileSize(scratchA) == 0 && FileSize(scratchB) == 0,
		"both fixed scratch entries exist before late open");

	Check(boot.openCampaign(Fingerprint(), ContentDigest()) &&
		boot.state() == DedicatedCampaignBootState::OpenCreate &&
		boot.entry() == DedicatedCampaignBootEntry::CreateNewCampaign &&
		boot.campaignState() && !boot.campaignState()->hasCheckpoint,
		"late create opens the exact campaign identity");
	ActiveProfile = boot.profileDirectory();
	ResetLegacyBridge();
	LegacySaveBytes = {'f', 'i', 'r', 's', 't'};
	Check(boot.checkpoint(321) && LegacySaveCalls == 1 &&
		LegacyValidateCalls == 1 && boot.campaignState() &&
		boot.campaignState()->activeSlot == DedicatedCampaignSlot::A &&
		boot.campaignState()->generation == 1 &&
		boot.campaignState()->worldMinutes == 321,
		"first live checkpoint publishes slot A");
	LegacySaveBytes = {'s', 'e', 'c', 'o', 'n', 'd'};
	Check(boot.checkpoint(654) && boot.campaignState() &&
		boot.campaignState()->activeSlot == DedicatedCampaignSlot::B &&
		boot.campaignState()->generation == 2 &&
		boot.campaignState()->worldMinutes == 654,
		"second live checkpoint publishes slot B");
	FileIdentity scratchAAfter;
	FileIdentity scratchBAfter;
	Check(ReadFileIdentity(scratchA, scratchAAfter) &&
		ReadFileIdentity(scratchB, scratchBAfter) &&
		scratchAAfter == scratchAIdentity && scratchBAfter == scratchBIdentity,
		"legacy checkpoints retain the pre-scanned scratch entries");
	boot.close();
	Check(boot.state() == DedicatedCampaignBootState::Closed &&
		boot.entry() == DedicatedCampaignBootEntry::None &&
		boot.profileDirectory().empty() && boot.campaignState() == nullptr,
		"close withdraws every live campaign view");
	FixtureWriter writer;
	DedicatedCampaignFilesystemBackend probe(writer);
	Check(probe.open(root.path(), "other") ==
		DedicatedCampaignFilesystemError::None,
		"close releases the process lease");
}

void TestActiveCheckpointReaderLifecycle()
{
	TemporaryRoot root;
	DedicatedCampaignBoot boot;
	DedicatedCampaignCheckpointReader firstReader;
	Check(!boot.openActiveCheckpointReader(firstReader) &&
		!firstReader.isOpen(),
		"a fresh boot cannot expose a checkpoint reader");
	Check(boot.prepare(Options(root, "reader_lifecycle",
			DedicatedCampaignAction::Create)) &&
		!boot.openActiveCheckpointReader(firstReader) &&
		!firstReader.isOpen(),
		"a prepared boot has no committed active checkpoint");
	Check(boot.openCampaign(Fingerprint(), ContentDigest()) &&
		!boot.openActiveCheckpointReader(firstReader),
		"an open new campaign still refuses a reader before its first commit");

	ActiveProfile = boot.profileDirectory();
	ResetLegacyBridge();
	LegacySaveBytes.resize(70000);
	for (std::size_t index = 0; index < LegacySaveBytes.size(); ++index)
		LegacySaveBytes[index] =
			static_cast<std::uint8_t>(index * 29u + 3u);
	const std::vector<std::uint8_t> firstBytes = LegacySaveBytes;
	Check(boot.checkpoint(700) && boot.campaignState() &&
		boot.openActiveCheckpointReader(firstReader) &&
		firstReader.slot() == boot.campaignState()->activeSlot &&
		firstReader.generation() == boot.campaignState()->generation &&
		firstReader.worldMinutes() == boot.campaignState()->worldMinutes &&
		firstReader.size() == boot.campaignState()->checkpointSize &&
		firstReader.checkpointSha256() ==
			boot.campaignState()->checkpointSha256,
		"boot captures one exact committed store generation in its reader");
	std::array<std::uint8_t, 43> firstChunk{};
	Check(firstReader.readExact(65513, firstChunk.data(), firstChunk.size()) &&
		std::equal(firstChunk.begin(), firstChunk.end(),
			firstBytes.begin() + 65513),
		"the boot reader streams bytes through the filesystem handle");

	LegacySaveBytes.assign(321, 0x9au);
	Check(boot.checkpoint(701) && boot.campaignState() &&
		boot.campaignState()->generation == 2 &&
		firstReader.generation() == 1 && firstReader.worldMinutes() == 700,
		"a later commit cannot mutate a reader's captured generation");
	DedicatedCampaignCheckpointReader currentReader;
	Check(boot.openActiveCheckpointReader(currentReader) &&
		currentReader.generation() == 2 &&
		currentReader.worldMinutes() == 701 && currentReader.size() == 321,
		"a subsequent open captures the new current store state");
	std::array<std::uint8_t, 9> currentChunk{};
	Check(currentReader.readExact(100, currentChunk.data(),
			currentChunk.size()) &&
		std::all_of(currentChunk.begin(), currentChunk.end(),
			[](std::uint8_t byte) { return byte == 0x9au; }),
		"the newer reader holds its own active slot bytes");

	boot.close();
	Check(!boot.openActiveCheckpointReader(currentReader) &&
		currentReader.isOpen() && currentReader.generation() == 2 &&
		currentReader.readExact(100, currentChunk.data(),
			currentChunk.size()) &&
		std::all_of(currentChunk.begin(), currentChunk.end(),
			[](std::uint8_t byte) { return byte == 0x9au; }) &&
		firstReader.readExact(65513, firstChunk.data(), firstChunk.size()) &&
		std::equal(firstChunk.begin(), firstChunk.end(),
			firstBytes.begin() + 65513),
		"closed boot refuses new sessions without invalidating published readers");
	FixtureWriter successorWriter;
	DedicatedCampaignFilesystemBackend successor(successorWriter);
	Check(successor.open(root.path(), "reader_successor") ==
		DedicatedCampaignFilesystemError::None,
		"published checkpoint readers do not retain the boot process lease");
}

void TestResumeMaterializationAndNonEmptyProfile()
{
	TemporaryRoot root;
	const DedicatedCampaignIdentity identity = Identity("resume");
	const std::vector<std::uint8_t> checkpoint{
		'r', 'e', 's', 'u', 'm', 'e', 'd'};
	PublishFixture(root, identity, {checkpoint});
	const std::filesystem::path profile = ProfileDirectory(root, "resume");
	WriteBytes(profile / "unrelated.ini", {'x'});
	const std::filesystem::path scratch = profile /
		DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A);
	WriteBytes(scratch, {'s', 't', 'a', 'l', 'e'});
	FileIdentity before;
	Check(ReadFileIdentity(scratch, before), "stale scratch identity is known");

	DedicatedCampaignBoot boot;
	Check(boot.prepare(Options(root, "resume",
			DedicatedCampaignAction::Resume, 1)) &&
		boot.state() == DedicatedCampaignBootState::PreparedResume &&
		boot.campaignSeed() == identity.campaignSeed && FileSize(scratch) == 0 &&
		ReadBytes(profile / "unrelated.ini") == std::vector<std::uint8_t>({'x'}),
		"resume derives seed and confines cleanup to fixed scratch files");
	FileIdentity prepared;
	Check(ReadFileIdentity(scratch, prepared) && prepared == before,
		"resume truncates an existing safe scratch in place");
	Check(boot.openCampaign(identity.runtimeFingerprint,
			identity.contentManifestSha256) &&
		boot.state() == DedicatedCampaignBootState::OpenResume &&
		boot.entry() == DedicatedCampaignBootEntry::ResumeCheckpoint &&
		boot.campaignState() &&
		boot.campaignState()->activeSlot == DedicatedCampaignSlot::A &&
		ReadBytes(scratch) == checkpoint,
		"late resume materializes the validated active checkpoint");
	FileIdentity after;
	Check(ReadFileIdentity(scratch, after) && after == before,
		"materialization preserves the pre-scanned scratch entry");
}

void TestCorruptNewerFallsBackAndCorruptOnlyFails()
{
	{
		TemporaryRoot root;
		const DedicatedCampaignIdentity identity = Identity("fallback");
		const std::vector<std::uint8_t> first{'o', 'l', 'd'};
		PublishFixture(root, identity, {first, {'n', 'e', 'w'}});
		WriteBytes(CampaignDirectory(root, "fallback") / "checkpoint-b.sav",
			{'b', 'a', 'd'});
		DedicatedCampaignBoot boot;
		Check(boot.prepare(Options(root, "fallback",
				DedicatedCampaignAction::Resume)) &&
			boot.openCampaign(identity.runtimeFingerprint,
				identity.contentManifestSha256) &&
			boot.campaignState() &&
			boot.campaignState()->activeSlot == DedicatedCampaignSlot::A &&
			boot.campaignState()->generation == 1 &&
			ReadBytes(boot.profileDirectory() /
				DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A)) == first,
			"resume falls back to the older valid checkpoint pair");
	}
	{
		TemporaryRoot root;
		const DedicatedCampaignIdentity identity = Identity("corrupt");
		PublishFixture(root, identity, {{'g', 'o', 'o', 'd'}});
		WriteBytes(CampaignDirectory(root, "corrupt") / "checkpoint-a.sav",
			{'b', 'a', 'd'});
		DedicatedCampaignBoot boot;
		Check(static_cast<bool>(boot.prepare(Options(root, "corrupt",
			DedicatedCampaignAction::Resume))),
			"bounded seed inspection does not trust checkpoint bytes");
		const DedicatedCampaignBootResult opened = boot.openCampaign(
			identity.runtimeFingerprint, identity.contentManifestSha256);
		Check(!opened &&
			opened.error == DedicatedCampaignBootError::StoreOpenFailed &&
			opened.storeError == DedicatedCampaignStoreError::NoValidCheckpoint &&
			boot.state() == DedicatedCampaignBootState::Poisoned &&
			boot.profileDirectory().empty(),
			"a resume with no valid checkpoint fails stop");
	}
}

void TestProfileAndLinkedScratchRejections()
{
	{
		TemporaryRoot root;
		DedicatedCampaignBoot interrupted;
		Check(static_cast<bool>(interrupted.prepare(
			Options(root, "dirty", DedicatedCampaignAction::Create))),
			"interrupted create prepares its pre-VFS scratch entries");
		const std::filesystem::path oldProfile = interrupted.profileDirectory();
		WriteBytes(oldProfile / "startup.log", {'x'});
		interrupted.close();

		DedicatedCampaignBoot retry;
		const DedicatedCampaignBootResult prepared = retry.prepare(
			Options(root, "dirty", DedicatedCampaignAction::Create));
		bool preserved = false;
		for (const std::filesystem::directory_entry& entry :
			std::filesystem::directory_iterator(CampaignDirectory(root, "dirty")))
		{
			const std::string name = entry.path().filename().string();
			if (name.compare(0, 15, "profile.orphan.") == 0 &&
				ReadBytes(entry.path() / "startup.log") ==
					std::vector<std::uint8_t>({'x'}))
				preserved = true;
		}
		Check(prepared &&
			retry.state() == DedicatedCampaignBootState::PreparedCreate &&
			!std::filesystem::exists(retry.profileDirectory() / "startup.log") &&
			preserved,
			"new retries quarantine an uncommitted profile without deleting it");
	}
	{
		TemporaryRoot root;
		const DedicatedCampaignIdentity identity = Identity("committed-new");
		PublishFixture(root, identity, {{'c', 'o', 'm', 'm', 'i', 't'}});
		const std::filesystem::path profile =
			ProfileDirectory(root, "committed-new");
		WriteBytes(profile / "runtime.log", {'x'});
		DedicatedCampaignBoot boot;
		const DedicatedCampaignBootResult prepared = boot.prepare(
			Options(root, "committed-new", DedicatedCampaignAction::Create));
		Check(!prepared && prepared.error ==
				DedicatedCampaignBootError::CreateProfileNotEmpty &&
			boot.state() == DedicatedCampaignBootState::Poisoned &&
			ReadBytes(profile / "runtime.log") ==
				std::vector<std::uint8_t>({'x'}),
			"new never rotates a profile when any committed manifest exists");
	}
	for (unsigned corruptKind = 0; corruptKind < 2; ++corruptKind)
	{
		TemporaryRoot root;
		const std::string campaign = corruptKind == 0
			? "zero-manifest" : "corrupt-manifest";
		FixtureWriter writer;
		DedicatedCampaignFilesystemBackend setup(writer);
		Check(setup.open(root.path(), campaign) ==
				DedicatedCampaignFilesystemError::None,
			"corrupt manifest recovery fixture opens");
		const std::filesystem::path profile = setup.profileDirectory();
		const std::filesystem::path manifest =
			setup.manifestPath(DedicatedCampaignSlot::A);
		setup.close();
		WriteBytes(profile / "runtime.log", {'x'});
		WriteBytes(manifest, corruptKind == 0
			? std::vector<std::uint8_t>{}
			: std::vector<std::uint8_t>{'b', 'a', 'd'});
		DedicatedCampaignBoot boot;
		const DedicatedCampaignBootResult prepared = boot.prepare(
			Options(root, campaign, DedicatedCampaignAction::Create));
		Check(!prepared && prepared.error ==
				DedicatedCampaignBootError::CreateProfileNotEmpty &&
			ReadBytes(profile / "runtime.log") ==
				std::vector<std::uint8_t>({'x'}),
			"new refuses zero-length and corrupt manifest bytes without rotation");
	}
	{
		TemporaryRoot root;
		const DedicatedCampaignIdentity identity = Identity("hardlink");
		PublishFixture(root, identity, {{'o', 'k'}});
		const std::filesystem::path outside = root.path() / "outside";
		WriteBytes(outside, {'s', 'a', 'f', 'e'});
		const std::filesystem::path scratch = ProfileDirectory(root, "hardlink") /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A);
		std::error_code error;
		std::filesystem::create_hard_link(outside, scratch, error);
		Check(!error, "hard-link scratch fixture is created");
		DedicatedCampaignBoot boot;
		const DedicatedCampaignBootResult prepared = boot.prepare(
			Options(root, "hardlink", DedicatedCampaignAction::Resume));
		Check(!prepared && prepared.error ==
				DedicatedCampaignBootError::ScratchPreparationFailed &&
			ReadBytes(outside) ==
				std::vector<std::uint8_t>({'s', 'a', 'f', 'e'}),
			"hard-linked scratch is rejected without truncating its target");
	}
#ifndef _WIN32
	{
		TemporaryRoot root;
		const DedicatedCampaignIdentity identity = Identity("symlink");
		PublishFixture(root, identity, {{'o', 'k'}});
		const std::filesystem::path outside = root.path() / "outside";
		WriteBytes(outside, {'s', 'a', 'f', 'e'});
		const std::filesystem::path scratch = ProfileDirectory(root, "symlink") /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A);
		std::error_code error;
		std::filesystem::create_symlink(outside, scratch, error);
		Check(!error, "symlink scratch fixture is created");
		DedicatedCampaignBoot boot;
		const DedicatedCampaignBootResult prepared = boot.prepare(
			Options(root, "symlink", DedicatedCampaignAction::Resume));
		Check(!prepared && prepared.error ==
				DedicatedCampaignBootError::ScratchPreparationFailed &&
			ReadBytes(outside) ==
				std::vector<std::uint8_t>({'s', 'a', 'f', 'e'}),
			"symlink scratch is rejected without following its target");
	}
#endif
}

void TestSeedAndIdentityRechecks()
{
	{
		TemporaryRoot root;
		const DedicatedCampaignIdentity identity = Identity("seed", 17);
		PublishFixture(root, identity, {{'s', 'e', 'e', 'd'}});
		DedicatedCampaignBoot boot;
		Check(boot.prepare(Options(root, "seed",
				DedicatedCampaignAction::Resume)) && boot.campaignSeed() == 17,
			"early resume fixes the immutable campaign seed");
		RewriteManifestSeed(
			CampaignDirectory(root, "seed") / "manifest-a.bin", 18);
		const DedicatedCampaignBootResult opened = boot.openCampaign(
			identity.runtimeFingerprint, identity.contentManifestSha256);
		Check(!opened &&
			opened.error == DedicatedCampaignBootError::ResumeSeedChanged &&
			boot.state() == DedicatedCampaignBootState::Poisoned,
			"late resume detects manifest seed drift before store open");
	}
	{
		TemporaryRoot root;
		const DedicatedCampaignIdentity identity = Identity("identity");
		PublishFixture(root, identity, {{'i', 'd'}});
		DedicatedCampaignBoot boot;
		Check(static_cast<bool>(boot.prepare(Options(root, "identity",
			DedicatedCampaignAction::Resume))),
			"identity mismatch fixture prepares");
		DedicatedCampaignRuntimeFingerprint incompatible = Fingerprint();
		++incompatible.low;
		const DedicatedCampaignBootResult opened = boot.openCampaign(
			incompatible, identity.contentManifestSha256);
		Check(!opened &&
			opened.error == DedicatedCampaignBootError::StoreOpenFailed &&
			opened.storeError ==
				DedicatedCampaignStoreError::IncompatibleManifest &&
			boot.state() == DedicatedCampaignBootState::Poisoned,
			"runtime identity mismatch poisons resume without materialization");
	}
	{
		TemporaryRoot root;
		DedicatedCampaignBoot boot;
		Check(static_cast<bool>(boot.prepare(Options(root, "missing-digest",
			DedicatedCampaignAction::Create))),
			"missing digest fixture prepares");
		const DedicatedCampaignContentManifestSha256 missing{};
		const DedicatedCampaignBootResult opened =
			boot.openCampaign(Fingerprint(), missing);
		Check(!opened && opened.error ==
				DedicatedCampaignBootError::MissingContentManifestSha256 &&
			boot.state() == DedicatedCampaignBootState::Poisoned,
			"late open requires a canonical nonzero content digest");
	}
}

void TestMaterializationAndBindingFailures()
{
	{
		TemporaryRoot root;
		const DedicatedCampaignIdentity identity = Identity("materialize");
		PublishFixture(root, identity, {{'c', 'h', 'e', 'c', 'k'}});
		DedicatedCampaignBoot boot;
		Check(static_cast<bool>(boot.prepare(Options(root, "materialize",
			DedicatedCampaignAction::Resume))),
			"materialization failure fixture prepares");
		const std::filesystem::path outside = root.path() / "outside";
		WriteBytes(outside, {'u', 'n', 't', 'o', 'u', 'c', 'h', 'e', 'd'});
		const std::filesystem::path scratch = boot.profileDirectory() /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A);
		std::error_code error;
		std::filesystem::remove(scratch, error);
		error.clear();
		std::filesystem::create_hard_link(outside, scratch, error);
		Check(!error, "materialization hard-link race fixture is installed");
		const DedicatedCampaignBootResult opened = boot.openCampaign(
			identity.runtimeFingerprint, identity.contentManifestSha256);
		Check(!opened && opened.error ==
				DedicatedCampaignBootError::CheckpointMaterializationFailed &&
			boot.state() == DedicatedCampaignBootState::Poisoned &&
			ReadBytes(outside) == std::vector<std::uint8_t>(
				{'u', 'n', 't', 'o', 'u', 'c', 'h', 'e', 'd'}),
			"linked materialization destination fails stop without truncation");
	}
	{
		TemporaryRoot root;
		FixtureWriter preboundWriter;
		DedicatedCampaignFilesystemBackend backend(preboundWriter);
		DedicatedCampaignBoot boot(backend);
		const DedicatedCampaignBootResult prepared = boot.prepare(
			Options(root, "bind", DedicatedCampaignAction::Create));
		Check(!prepared && prepared.error ==
				DedicatedCampaignBootError::CheckpointWriterBindFailed &&
			boot.state() == DedicatedCampaignBootState::Poisoned &&
			!backend.isOpen(),
			"a pre-bound backend rejects ambiguous writer ownership");
		FixtureWriter probeWriter;
		DedicatedCampaignFilesystemBackend probe(probeWriter);
		Check(probe.open(root.path(), "after-bind") ==
			DedicatedCampaignFilesystemError::None,
			"bind failure releases the process lease");
	}
}

void TestLifecycleOrderingAndCheckpointFailure()
{
	{
		TemporaryRoot root;
		DedicatedCampaignBoot boot;
		Check(boot.openCampaign(Fingerprint(), ContentDigest()).error ==
				DedicatedCampaignBootError::InvalidState &&
			boot.checkpoint(1).error ==
				DedicatedCampaignBootError::InvalidState &&
			boot.state() == DedicatedCampaignBootState::Fresh,
			"late operations cannot run before early preparation");
		Check(static_cast<bool>(boot.prepare(Options(root, "ordering",
			DedicatedCampaignAction::Create))),
			"ordering fixture prepares");
		Check(boot.prepare(Options(root, "ordering",
				DedicatedCampaignAction::Create)).error ==
				DedicatedCampaignBootError::InvalidState &&
			boot.checkpoint(1).error ==
				DedicatedCampaignBootError::InvalidState &&
			boot.state() == DedicatedCampaignBootState::PreparedCreate,
			"duplicate early work cannot disturb a prepared lease");
		Check(static_cast<bool>(
			boot.openCampaign(Fingerprint(), ContentDigest())),
			"ordering fixture opens");
		Check(boot.openCampaign(Fingerprint(), ContentDigest()).error ==
				DedicatedCampaignBootError::InvalidState &&
			boot.state() == DedicatedCampaignBootState::OpenCreate,
			"duplicate late open cannot disturb a running campaign");
		boot.close();
		Check(boot.prepare(Options(root, "ordering",
				DedicatedCampaignAction::Create)).error ==
				DedicatedCampaignBootError::InvalidState &&
			boot.state() == DedicatedCampaignBootState::Closed,
			"a closed coordinator is one-shot");
	}
	{
		TemporaryRoot root;
		DedicatedCampaignBoot boot;
		Check(boot.prepare(Options(root, "checkpoint-failure",
				DedicatedCampaignAction::Create)) &&
			boot.openCampaign(Fingerprint(), ContentDigest()),
			"checkpoint failure fixture opens");
		ActiveProfile = boot.profileDirectory();
		ResetLegacyBridge();
		LegacySaveResult = false;
		const DedicatedCampaignBootResult checkpointed = boot.checkpoint(10);
		Check(!checkpointed && checkpointed.error ==
				DedicatedCampaignBootError::CheckpointFailed &&
			checkpointed.storeError ==
				DedicatedCampaignStoreError::BackendFailure &&
			boot.state() == DedicatedCampaignBootState::Poisoned &&
			boot.campaignState() == nullptr && boot.profileDirectory().empty(),
			"checkpoint failure poisons and withdraws partial state");

		FixtureWriter competingWriter;
		DedicatedCampaignFilesystemBackend competingBackend(competingWriter);
		Check(competingBackend.open(root.path(), "checkpoint-failure") ==
				DedicatedCampaignFilesystemError::LockHeld,
			"post-mount checkpoint failure retains the campaign lease");
		boot.close();
		FixtureWriter successorWriter;
		DedicatedCampaignFilesystemBackend successorBackend(successorWriter);
		Check(successorBackend.open(root.path(), "checkpoint-failure") ==
				DedicatedCampaignFilesystemError::None,
			"explicit close releases a poisoned campaign lease");
	}
}
}

bool SaveDedicatedCampaignGame(DedicatedCampaignSlot slot) noexcept
{
	++LegacySaveCalls;
	if (!LegacySaveResult) return false;
	try
	{
		WriteBytes(ActiveProfile / DedicatedCampaignLogicalScratch(slot),
			LegacySaveBytes);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool ValidateDedicatedCampaignGame(DedicatedCampaignSlot) noexcept
{
	++LegacyValidateCalls;
	return LegacyValidateResult;
}

bool LoadDedicatedCampaignGame(DedicatedCampaignSlot) noexcept
{
	return false;
}

int main()
{
	TestCreateAndCheckpointLifecycle();
	TestActiveCheckpointReaderLifecycle();
	TestResumeMaterializationAndNonEmptyProfile();
	TestCorruptNewerFallsBackAndCorruptOnlyFails();
	TestProfileAndLinkedScratchRejections();
	TestSeedAndIdentityRechecks();
	TestMaterializationAndBindingFailures();
	TestLifecycleOrderingAndCheckpointFailure();
	std::puts("dedicated campaign boot tests passed");
	return 0;
}
