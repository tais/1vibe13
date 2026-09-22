#include <Ja2/FullEngineCoopClientCampaignScratch.h>

#include <Ja2/DedicatedCampaignFilesystem.h>
#include <Ja2/DedicatedCampaignSaveBridge.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using namespace CoopSession;

namespace
{
int Failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL: %s\n", message); \
		++Failures; \
	} \
} while (false)

std::filesystem::path ActiveProfile;
std::vector<std::uint8_t> ExpectedCheckpoint;
std::vector<DedicatedCampaignSlot> ValidatedSlots;
std::vector<DedicatedCampaignSlot> LoadedSlots;
bool ValidationSucceeds = true;
bool LoadSucceeds = true;
unsigned SaveCalls = 0;

class TemporaryStateRoot
{
public:
	TemporaryStateRoot()
	{
		static std::atomic<std::uint64_t> sequence{1};
		try
		{
			path_ = std::filesystem::temp_directory_path() /
				("ja2-coop-client-scratch-" + std::to_string(
					static_cast<std::uint64_t>(
						std::chrono::steady_clock::now().time_since_epoch().count())) +
				 "-" + std::to_string(sequence.fetch_add(1)));
			std::filesystem::create_directory(path_);
			std::filesystem::permissions(path_,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
		}
		catch (...)
		{
			path_.clear();
		}
	}

	~TemporaryStateRoot()
	{
		if (path_.empty()) return;
		std::error_code ignored;
		std::filesystem::remove_all(path_, ignored);
	}

	const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_;
};

bool WriteBytes(const std::filesystem::path& path,
	const std::vector<std::uint8_t>& bytes)
{
	try
	{
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output) return false;
		if (!bytes.empty())
			output.write(reinterpret_cast<const char*>(bytes.data()),
				static_cast<std::streamsize>(bytes.size()));
		output.close();
		return static_cast<bool>(output);
	}
	catch (...)
	{
		return false;
	}
}

std::vector<std::uint8_t> ReadBytes(const std::filesystem::path& path)
{
	try
	{
		std::ifstream input(path, std::ios::binary);
		if (!input) return {};
		return std::vector<std::uint8_t>(
			std::istreambuf_iterator<char>(input),
			std::istreambuf_iterator<char>());
	}
	catch (...)
	{
		return {};
	}
}

std::uint64_t FileSize(const std::filesystem::path& path)
{
	std::error_code error;
	const std::uintmax_t size = std::filesystem::file_size(path, error);
	return error ? UINT64_MAX : static_cast<std::uint64_t>(size);
}

std::filesystem::path CampaignDirectory(
	const std::filesystem::path& stateRoot)
{
	try
	{
		const std::filesystem::path campaigns = stateRoot / "campaigns";
		for (const std::filesystem::directory_entry& entry :
			std::filesystem::directory_iterator(campaigns))
		{
			if (entry.is_directory()) return entry.path();
		}
	}
	catch (...)
	{
	}
	return {};
}

CoopCampaignBootstrapDescriptor Bootstrap(std::uint8_t marker = 0x20)
{
	CoopCampaignBootstrapDescriptor descriptor;
	descriptor.sessionEpoch = UINT64_C(0x1122334455667788);
	descriptor.campaignSeed = UINT64_C(0x8877665544332211);
	for (std::size_t index = 0;
		index < descriptor.campaignIdentitySha256.size(); ++index)
	{
		descriptor.campaignIdentitySha256[index] =
			static_cast<std::uint8_t>(marker + index);
		descriptor.contentManifestSha256[index] =
			static_cast<std::uint8_t>(0x80u + index);
	}
	descriptor.runtimeFingerprint = {
		0x01020304u, UINT64_C(0x1020304050607080),
		UINT64_C(0x90a0b0c0d0e0f001)};
	return descriptor;
}

AdmissionAck Credential(const CoopCampaignBootstrapDescriptor& bootstrap,
	std::uint8_t marker)
{
	AdmissionAck credential;
	credential.protocolVersion = bootstrap.protocolVersion;
	credential.sessionEpoch = bootstrap.sessionEpoch;
	for (std::size_t index = 0; index < credential.peerIdentity.size(); ++index)
		credential.peerIdentity[index] =
			static_cast<std::uint8_t>(marker + index);
	for (std::size_t index = 0; index < credential.reconnectToken.size(); ++index)
		credential.reconnectToken[index] =
			static_cast<std::uint8_t>(marker + 0x40u + index);
	return credential;
}

CoopCampaignCheckpointSha256 Digest(const char* hex)
{
	CoopCampaignCheckpointSha256 digest{};
	const auto nibble = [](char value) -> std::uint8_t {
		if (value >= '0' && value <= '9')
			return static_cast<std::uint8_t>(value - '0');
		return static_cast<std::uint8_t>(value - 'a' + 10);
	};
	for (std::size_t index = 0; index < digest.size(); ++index)
		digest[index] = static_cast<std::uint8_t>(
			(nibble(hex[index * 2]) << 4) | nibble(hex[index * 2 + 1]));
	return digest;
}

const CoopCampaignCheckpointSha256 AbcDigest = Digest(
	"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
const CoopCampaignCheckpointSha256 HelloDigest = Digest(
	"2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");

CoopCampaignSyncMetadata Metadata(
	const CoopCampaignBootstrapDescriptor& bootstrap,
	std::uint64_t generation, std::uint64_t totalSize,
	const CoopCampaignCheckpointSha256& digest)
{
	CoopCampaignSyncMetadata metadata;
	metadata.transfer.protocolVersion = bootstrap.protocolVersion;
	metadata.transfer.sessionEpoch = bootstrap.sessionEpoch;
	metadata.transfer.transferId = generation + 100;
	metadata.transfer.campaignSeed = bootstrap.campaignSeed;
	metadata.transfer.campaignIdentitySha256 =
		bootstrap.campaignIdentitySha256;
	metadata.transfer.checkpointGeneration = generation;
	metadata.transfer.totalSize = totalSize;
	metadata.transfer.checkpointSha256 = digest;
	metadata.worldMinutes = generation * 60;
	return metadata;
}

void ResetBridge()
{
	ActiveProfile.clear();
	ExpectedCheckpoint.clear();
	ValidatedSlots.clear();
	LoadedSlots.clear();
	ValidationSucceeds = true;
	LoadSucceeds = true;
	SaveCalls = 0;
}

bool ExactProfileAllowlist(const std::filesystem::path& profile)
{
	try
	{
		std::vector<std::string> names;
		for (const std::filesystem::directory_entry& entry :
			std::filesystem::directory_iterator(profile))
		{
			names.push_back(entry.path().filename().string());
			std::error_code error;
			if (!entry.is_regular_file(error) || error ||
				std::filesystem::hard_link_count(entry.path(), error) != 1 || error)
				return false;
		}
		std::sort(names.begin(), names.end());
		std::vector<std::string> expected{
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A),
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::B)};
		std::sort(expected.begin(), expected.end());
		return names == expected;
	}
	catch (...)
	{
		return false;
	}
}

std::vector<std::filesystem::path> QuarantinedProfiles(
	const std::filesystem::path& campaign)
{
	std::vector<std::filesystem::path> profiles;
	try
	{
		for (const std::filesystem::directory_entry& entry :
			std::filesystem::directory_iterator(campaign))
		{
			const std::string name = entry.path().filename().string();
			std::error_code error;
			if (name.rfind("profile.orphan.", 0) == 0 &&
				entry.is_directory(error) && !error &&
				!entry.is_symlink(error) && !error)
				profiles.push_back(entry.path());
		}
		std::sort(profiles.begin(), profiles.end());
	}
	catch (...)
	{
		profiles.clear();
	}
	return profiles;
}

bool OwnerOnlyPath(const std::filesystem::path& path)
{
#ifdef _WIN32
	(void)path;
	return true;
#else
	std::error_code error;
	const std::filesystem::perms permissions =
		std::filesystem::status(path, error).permissions();
	if (error) return false;
	const std::filesystem::perms forbidden =
		std::filesystem::perms::group_read |
		std::filesystem::perms::group_write |
		std::filesystem::perms::group_exec |
		std::filesystem::perms::others_read |
		std::filesystem::perms::others_write |
		std::filesystem::perms::others_exec;
	return (permissions & forbidden) == std::filesystem::perms::none;
#endif
}

bool OwnerReadWriteOnlyFile(const std::filesystem::path& path)
{
#ifdef _WIN32
	(void)path;
	return true;
#else
	std::error_code error;
	const std::filesystem::perms permissions =
		std::filesystem::status(path, error).permissions();
	return !error && permissions ==
		(std::filesystem::perms::owner_read |
		 std::filesystem::perms::owner_write);
#endif
}

void TestPrepareLeaseRestartAndIdentity()
{
	ResetBridge();
	TemporaryStateRoot root;
	const CoopCampaignBootstrapDescriptor bootstrap = Bootstrap();
	FullEngineCoopClientCampaignScratch scratch;
	CHECK(scratch.prepare(std::filesystem::path("relative"), bootstrap) ==
		FullEngineCoopClientCampaignScratchPrepareResult::InvalidConfiguration,
		"prepare rejects a relative state root before creating writable state");
	CoopCampaignBootstrapDescriptor invalid = bootstrap;
	invalid.campaignIdentitySha256 = {};
	CHECK(scratch.prepare(root.path(), invalid) ==
		FullEngineCoopClientCampaignScratchPrepareResult::InvalidConfiguration,
		"prepare rejects an incomplete bootstrap identity");

#ifndef _WIN32
	TemporaryStateRoot permissive;
	std::filesystem::permissions(permissive.path(),
		std::filesystem::perms::owner_all |
			std::filesystem::perms::group_read,
		std::filesystem::perm_options::replace);
	FullEngineCoopClientCampaignScratch permissionProbe;
	CHECK(permissionProbe.prepare(permissive.path(), bootstrap) !=
		FullEngineCoopClientCampaignScratchPrepareResult::Success,
		"prepare refuses a state root that is not private mode 0700");
#endif

	const FullEngineCoopClientCampaignScratchPrepareResult prepared =
		scratch.prepare(root.path(), bootstrap);
	CHECK(prepared == FullEngineCoopClientCampaignScratchPrepareResult::Success,
		"prepare acquires the backend lease and creates private scratch state");
	const std::filesystem::path profile = scratch.profileDirectory();
	CHECK(scratch.prepared() && profile.is_absolute() &&
		ExactProfileAllowlist(profile) && OwnerOnlyPath(profile) &&
		OwnerOnlyPath(profile /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A)) &&
		OwnerOnlyPath(profile /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::B)) &&
		FileSize(profile /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A)) == 0 &&
		FileSize(profile /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::B)) == 0,
		"prepared profile exposes only the two empty fixed VFS scratch files");
	const std::filesystem::path campaign = CampaignDirectory(root.path());
	const std::filesystem::path identity =
		campaign / "client-campaign-identity.sha256";
	CHECK(identity.parent_path() != profile && FileSize(identity) == 32 &&
		ReadBytes(identity) == std::vector<std::uint8_t>(
			bootstrap.campaignIdentitySha256.begin(),
			bootstrap.campaignIdentitySha256.end()),
		"the complete campaign identity is durable outside the VFS profile");

	FullEngineCoopClientCampaignScratch leaseProbe;
	CHECK(leaseProbe.prepare(root.path(), bootstrap) ==
		FullEngineCoopClientCampaignScratchPrepareResult::LeaseHeld,
		"a second scratch cannot enter while the first owns the process lease");
	CHECK(scratch.prepare(root.path(), bootstrap) ==
		FullEngineCoopClientCampaignScratchPrepareResult::AlreadyPrepared,
		"prepare never replaces its own live lease or mounted profile");
	scratch.close();
	CHECK(!scratch.prepared() && scratch.profileDirectory().empty() &&
		leaseProbe.prepare(root.path(), bootstrap) ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success &&
		ExactProfileAllowlist(leaseProbe.profileDirectory()),
		"only explicit post-VFS close releases the lease for a strict restart");
	leaseProbe.close();

	CoopCampaignBootstrapDescriptor collision = bootstrap;
	collision.campaignIdentitySha256.back() ^= 0x5au;
	FullEngineCoopClientCampaignScratch collisionProbe;
	CHECK(collisionProbe.prepare(root.path(), collision) ==
		FullEngineCoopClientCampaignScratchPrepareResult::IdentityMismatch,
		"a derived-path collision cannot inherit another full campaign identity");
}

void TestRestartAllowlistRejectsWritableAliases()
{
	const CoopCampaignBootstrapDescriptor bootstrap = Bootstrap(0x30);
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch first;
		CHECK(first.prepare(root.path(), bootstrap) ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success,
			"extra-entry fixture prepares");
		const std::filesystem::path profile = first.profileDirectory();
		first.close();
		CHECK(WriteBytes(profile / "unexpected.dat", {1, 2, 3}),
			"extra writable profile entry is created");
		FullEngineCoopClientCampaignScratch restarted;
		const auto result = restarted.prepare(root.path(), bootstrap);
		const std::vector<std::filesystem::path> quarantined =
			QuarantinedProfiles(CampaignDirectory(root.path()));
		CHECK(result ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			ExactProfileAllowlist(restarted.profileDirectory()) &&
			quarantined.size() == 1 &&
			ReadBytes(quarantined.front() / "unexpected.dat") ==
				std::vector<std::uint8_t>({1, 2, 3}),
			"restart quarantines an untrusted disposable profile intact before "
			"publishing a fresh strict allowlist");
	}
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch first;
		CHECK(first.prepare(root.path(), bootstrap) ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success,
			"hard-link fixture prepares");
		const std::filesystem::path profile = first.profileDirectory();
		first.close();
		const std::filesystem::path firstScratch = profile /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A);
		const std::filesystem::path secondScratch = profile /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::B);
		std::error_code error;
		std::filesystem::remove(firstScratch, error);
		error.clear();
		std::filesystem::create_hard_link(secondScratch, firstScratch, error);
		CHECK(!error, "profile hard-link fixture is created");
		FullEngineCoopClientCampaignScratch restarted;
		const auto result = restarted.prepare(root.path(), bootstrap);
		const std::vector<std::filesystem::path> quarantined =
			QuarantinedProfiles(CampaignDirectory(root.path()));
		error.clear();
		CHECK(result ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			ExactProfileAllowlist(restarted.profileDirectory()) &&
			quarantined.size() == 1 &&
			std::filesystem::hard_link_count(quarantined.front() /
				DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A), error) ==
				2 && !error,
			"restart quarantines aliased scratch identities without traversing or "
			"truncating either alias");
	}
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch first;
		CHECK(first.prepare(root.path(), bootstrap) ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success,
			"symlink fixture prepares");
		const std::filesystem::path profile = first.profileDirectory();
		first.close();
		const std::filesystem::path firstScratch = profile /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A);
		std::error_code error;
		std::filesystem::remove(firstScratch, error);
		error.clear();
		std::filesystem::create_symlink(
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::B),
			firstScratch, error);
		if (!error)
		{
			FullEngineCoopClientCampaignScratch restarted;
			const auto result = restarted.prepare(root.path(), bootstrap);
			const std::vector<std::filesystem::path> quarantined =
				QuarantinedProfiles(CampaignDirectory(root.path()));
			CHECK(result ==
					FullEngineCoopClientCampaignScratchPrepareResult::Success &&
				ExactProfileAllowlist(restarted.profileDirectory()) &&
				quarantined.size() == 1 &&
				std::filesystem::is_symlink(
					quarantined.front() /
					DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A)),
				"restart makes a hostile profile symlink inert by quarantining the "
				"complete tree before recreating scratch files");
		}
	}
}

void TestRestartResetsVfsOwnedDisposableProfile()
{
	TemporaryStateRoot root;
	const CoopCampaignBootstrapDescriptor bootstrap = Bootstrap(0x34);
	const AdmissionAck credential = Credential(bootstrap, 0x2b);
	FullEngineCoopClientCampaignScratch first;
	CHECK(first.prepare(root.path(), bootstrap) ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success &&
		first.persistReconnectCredential(credential),
		"VFS-owned restart fixture prepares with durable outer credentials");
	const std::filesystem::path firstProfile = first.profileDirectory();
	std::error_code error;
	std::filesystem::create_directories(
		firstProfile / "Temp" / "sector", error);
	error.clear();
	std::filesystem::create_directories(
		firstProfile / "ShadeTables", error);
	CHECK(!error &&
		WriteBytes(firstProfile / "Temp" / "sector" / "world.tmp", {1, 4}) &&
		WriteBytes(firstProfile / "ShadeTables" / "RGBDist.dat", {2, 5}) &&
		WriteBytes(firstProfile / "ja2_mp.ini", {3, 6}) &&
		WriteBytes(firstProfile /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A), {7, 8}),
		"ordinary VFS cache, Temp, settings, and loaded scratch outputs exist");
	first.close();

	FullEngineCoopClientCampaignScratch restarted;
	AdmissionAck loaded;
	const auto restartResult = restarted.prepare(root.path(), bootstrap);
	const std::filesystem::path campaign = CampaignDirectory(root.path());
	std::vector<std::filesystem::path> quarantined =
		QuarantinedProfiles(campaign);
	CHECK(restartResult ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success &&
		restarted.loadReconnectCredential(loaded) ==
			FullEngineCoopReconnectCredentialLoadResult::Loaded &&
		loaded.peerIdentity == credential.peerIdentity &&
		loaded.reconnectToken == credential.reconnectToken &&
		restarted.profileDirectory() == firstProfile &&
		ExactProfileAllowlist(restarted.profileDirectory()) &&
		FileSize(restarted.profileDirectory() /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A)) == 0 &&
		quarantined.size() == 1 &&
		ReadBytes(quarantined.front() / "Temp" / "sector" / "world.tmp") ==
			std::vector<std::uint8_t>({1, 4}) &&
		ReadBytes(quarantined.front() / "ShadeTables" / "RGBDist.dat") ==
			std::vector<std::uint8_t>({2, 5}) &&
		ReadBytes(quarantined.front() / "ja2_mp.ini") ==
			std::vector<std::uint8_t>({3, 6}),
		"restart preserves outer reconnect evidence while atomically quarantining "
		"all legitimate VFS-created profile outputs");

	error.clear();
	std::filesystem::create_directory(
		restarted.profileDirectory() / "Temp", error);
	CHECK(!error && WriteBytes(
		restarted.profileDirectory() / "Temp" / "second.tmp", {9}),
		"a second ordinary process lifetime creates disposable VFS output");
	restarted.close();
	FullEngineCoopClientCampaignScratch secondRestart;
	CHECK(secondRestart.prepare(root.path(), bootstrap) ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success &&
		ExactProfileAllowlist(secondRestart.profileDirectory()) &&
		QuarantinedProfiles(campaign).size() == 2,
		"a prior private quarantine remains inert and does not block another "
		"process restart");
}

void TestDurableReconnectCredentialLifecycle()
{
	TemporaryStateRoot root;
	const CoopCampaignBootstrapDescriptor bootstrap = Bootstrap(0x36);
	const AdmissionAck firstCredential = Credential(bootstrap, 0x21);
	const AdmissionAck replacement = Credential(bootstrap, 0x31);
	std::filesystem::path campaign;
	std::filesystem::path credentialPath;
	std::vector<std::uint8_t> firstRecord;
	{
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success,
			"credential fixture prepares its private campaign lease");
		AdmissionAck untouched = Credential(bootstrap, 0x61);
		const AdmissionAck sentinel = untouched;
		CHECK(scratch.loadReconnectCredential(untouched) ==
				FullEngineCoopReconnectCredentialLoadResult::Missing &&
			untouched.peerIdentity == sentinel.peerIdentity &&
			untouched.reconnectToken == sentinel.reconnectToken,
			"missing credential leaves caller output untouched");
		CHECK(scratch.persistReconnectCredential(firstCredential),
			"first credential is durably published before admission ACK");
		campaign = CampaignDirectory(root.path());
		credentialPath = campaign / "client-reconnect-credential.bin";
		firstRecord = ReadBytes(credentialPath);
		CoopCampaignBootstrapDescriptor decodedBootstrap;
		AdmissionAck decodedCredential;
		CHECK(credentialPath.parent_path() != scratch.profileDirectory() &&
			FileSize(credentialPath) ==
				CoopCampaignBootstrapWireSize + AdmissionAckWireSize + 32 &&
			OwnerReadWriteOnlyFile(credentialPath) &&
			!std::filesystem::exists(
				campaign / "client-reconnect-credential.staging") &&
			ExactProfileAllowlist(scratch.profileDirectory()) &&
			DecodeCoopCampaignBootstrap(firstRecord.data(),
				CoopCampaignBootstrapWireSize, decodedBootstrap) ==
					CoopCampaignBootstrapDecodeResult::Success &&
			SameCoopCampaignBootstrapDescriptor(decodedBootstrap, bootstrap) &&
			DecodeAdmissionAck(
				firstRecord.data() + CoopCampaignBootstrapWireSize,
				AdmissionAckWireSize, decodedCredential) == DecodeResult::Ok &&
			decodedCredential.peerIdentity == firstCredential.peerIdentity &&
			decodedCredential.reconnectToken == firstCredential.reconnectToken,
			"fixed record contains canonical exact bootstrap and AdmissionAck outside VFS");
#ifndef _WIN32
		std::filesystem::permissions(campaign,
			std::filesystem::perms::owner_read |
				std::filesystem::perms::owner_exec,
			std::filesystem::perm_options::replace);
		CHECK(scratch.persistReconnectCredential(firstCredential) &&
			ReadBytes(credentialPath) == firstRecord,
			"exact validated reconnect is idempotent without requiring a rewrite");
		std::filesystem::permissions(campaign,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
#endif
		AdmissionAck loaded;
		CHECK(scratch.loadReconnectCredential(loaded) ==
				FullEngineCoopReconnectCredentialLoadResult::Loaded &&
			loaded.peerIdentity == firstCredential.peerIdentity &&
			loaded.reconnectToken == firstCredential.reconnectToken,
			"same-process load returns the exact published bearer");
		scratch.close();
	}
	{
		FullEngineCoopClientCampaignScratch restarted;
		CHECK(restarted.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success,
			"credential campaign reopens after process-lifetime teardown");
		AdmissionAck loaded;
		CHECK(restarted.loadReconnectCredential(loaded) ==
				FullEngineCoopReconnectCredentialLoadResult::Loaded &&
			loaded.peerIdentity == firstCredential.peerIdentity &&
			loaded.reconnectToken == firstCredential.reconnectToken,
			"restart restores the exact identity and reconnect token");
		CHECK(restarted.persistReconnectCredential(replacement) &&
			ReadBytes(credentialPath) != firstRecord,
			"replacement credential atomically supersedes the previous record");
		loaded = {};
		CHECK(restarted.loadReconnectCredential(loaded) ==
				FullEngineCoopReconnectCredentialLoadResult::Loaded &&
			loaded.peerIdentity == replacement.peerIdentity &&
			loaded.reconnectToken == replacement.reconnectToken,
			"replacement publication is immediately exact-readable");
		restarted.close();
	}

	CoopCampaignBootstrapDescriptor nextEpoch = bootstrap;
	++nextEpoch.sessionEpoch;
	{
		FullEngineCoopClientCampaignScratch rolled;
		CHECK(rolled.prepare(root.path(), nextEpoch) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success,
			"same campaign can prepare under the server's next process epoch");
		AdmissionAck untouched = Credential(nextEpoch, 0x71);
		const AdmissionAck sentinel = untouched;
		CHECK(rolled.loadReconnectCredential(untouched) ==
				FullEngineCoopReconnectCredentialLoadResult::StaleSession &&
			std::filesystem::exists(credentialPath) &&
			untouched.peerIdentity == sentinel.peerIdentity,
			"epoch rollover is reported without early preflight-only credential loss");
		CHECK(rolled.eraseStaleReconnectCredential() &&
			!std::filesystem::exists(credentialPath) &&
			rolled.loadReconnectCredential(untouched) ==
				FullEngineCoopReconnectCredentialLoadResult::Missing,
			"late verified runtime may durably erase the unreachable old epoch");
	}
}


// Fixed protocol-8 record, independently produced from the documented durable
// layout and SHA-256. These are synthetic test identities, never live bearers.
std::vector<std::uint8_t> LegacyReconnectRecord()
{
	const char* hex =
		"4a3243420100010008000000887766554433221111223344556677882021"
		"22232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
		"04030201807060504030201001f0e0d0c0b0a09080818283848586878889"
		"8a8b8c8d8e8f909192939495969798999a9b9c9d9e9fcdc9c1c200000000"
		"00000000000000004a324341080003008877665544332211212223242526"
		"2728292a2b2c2d2e2f306162636465666768696a6b6c6d6e6f7071727374"
		"75767778797a7b7c7d7e7f806aa4611cf279b05a77423ee6d43930033d29"
		"72f8e6b8beb774d64112c39bffb8";
	std::vector<std::uint8_t> result;
	const auto nibble = [](char value) {
		return value <= '9' ? value - '0' : value - 'a' + 10;
	};
	for (std::size_t index = 0; hex[index] != '\0'; index += 2)
		result.push_back(static_cast<std::uint8_t>(
			(nibble(hex[index]) << 4) | nibble(hex[index + 1])));
	return result;
}

bool SameCredential(const AdmissionAck& left, const AdmissionAck& right)
{
	return left.protocolVersion == right.protocolVersion &&
		left.sessionEpoch == right.sessionEpoch &&
		left.peerIdentity == right.peerIdentity &&
		left.reconnectToken == right.reconnectToken;
}

void TestLegacyReconnectCredentialMigration()
{
	static_assert(CurrentProtocolVersion == 9,
		"review the explicit protocol-8 migration policy for the next protocol");
	const auto original = LegacyReconnectRecord();
	CHECK(original.size() == 224, "protocol-8 golden record has its fixed durable size");
	CoopCampaignBootstrapDescriptor decoded = Bootstrap(0x61);
	const auto decodedSentinel = decoded;
	AdmissionAck ack = Credential(decoded, 0x62);
	const auto ackSentinel = ack;
	CHECK(DecodeCoopCampaignBootstrap(original.data(), 128, decoded) ==
			CoopCampaignBootstrapDecodeResult::UnsupportedProtocol &&
		SameCoopCampaignBootstrapDescriptor(decoded, decodedSentinel) &&
		DecodeAdmissionAck(original.data() + 128, 64, ack) ==
			DecodeResult::UnsupportedProtocol && SameCredential(ack, ackSentinel),
		"persistence migration never weakens live protocol rejection or mutates decoder outputs");

	for (unsigned binding = 0; binding < 8; ++binding)
	{
		TemporaryStateRoot root;
		auto live = Bootstrap();
		if (binding != 1) ++live.sessionEpoch;
		if (binding == 2) ++live.campaignSeed;
		if (binding == 3) ++live.campaignIdentitySha256[0];
		if (binding == 4) ++live.runtimeFingerprint.schema;
		if (binding == 5) ++live.runtimeFingerprint.high;
		if (binding == 6) ++live.runtimeFingerprint.low;
		if (binding == 7) ++live.contentManifestSha256[0];
		FullEngineCoopClientCampaignScratch scratch;
		const auto currentCredential = Credential(live, 0x71);
		CHECK(scratch.prepare(root.path(), live) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			scratch.persistReconnectCredential(currentCredential),
			"migration fixture creates a safe current campaign credential path");
		const auto path = CampaignDirectory(root.path()) / "client-reconnect-credential.bin";
		CHECK(WriteBytes(path, original), "install immutable legacy fixture bytes");
		AdmissionAck untouched = currentCredential;
		const auto expected = binding == 0 ?
			FullEngineCoopReconnectCredentialLoadResult::StaleSession :
			FullEngineCoopReconnectCredentialLoadResult::BindingMismatch;
		CHECK(scratch.loadReconnectCredential(untouched) == expected &&
			SameCredential(untouched, currentCredential) && ReadBytes(path) == original &&
			!scratch.persistReconnectCredential(currentCredential) &&
			ReadBytes(path) == original,
			"only exact campaign binding plus a different epoch may classify legacy bytes stale; load and rejected replacement preserve evidence and output");
		if (binding == 0)
		{
			CHECK(scratch.eraseStaleReconnectCredential() &&
				scratch.loadReconnectCredential(untouched) ==
					FullEngineCoopReconnectCredentialLoadResult::Missing &&
				scratch.persistReconnectCredential(currentCredential) &&
				scratch.loadReconnectCredential(untouched) ==
					FullEngineCoopReconnectCredentialLoadResult::Loaded &&
				SameCredential(untouched, currentCredential) && ReadBytes(path) != original,
				"explicit late verified stale erasure allows a fresh current bearer, never an old bearer relabelled current");
		}
		else CHECK(!scratch.eraseStaleReconnectCredential() && ReadBytes(path) == original,
			"binding mismatch cannot erase the old record through stale migration");
	}

	TemporaryStateRoot root;
	auto live = Bootstrap();
	++live.sessionEpoch;
	FullEngineCoopClientCampaignScratch scratch;
	CHECK(scratch.prepare(root.path(), live) ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success &&
		scratch.persistReconnectCredential(Credential(live, 0x73)),
		"retired legacy fixture creates a safe record");
	const auto campaign = CampaignDirectory(root.path());
	const auto active = campaign / "client-reconnect-credential.bin";
	const auto retired = campaign / "client-reconnect-credential.retired";
	CHECK(WriteBytes(active, original), "retired fixture uses exact legacy record");
	std::filesystem::rename(active, retired);
	AdmissionAck retiredCredential;
	auto expectedRetired = Credential(Bootstrap(), 0x21);
	expectedRetired.protocolVersion = 8;
	CHECK(scratch.loadReconnectCredential(retiredCredential) ==
			FullEngineCoopReconnectCredentialLoadResult::Retired &&
		SameCredential(retiredCredential, expectedRetired) &&
		!scratch.eraseStaleReconnectCredential() &&
		!scratch.persistReconnectCredential(Credential(live, 0x74)) &&
		!std::filesystem::exists(active) && ReadBytes(retired) == original,
		"legacy retirement remains terminal and preserves its exact original protocol, epoch, identity and bearer bytes");
}

void TestLegacyReconnectCredentialMalformedRecords()
{
	struct Patch { std::size_t offset; std::size_t count; std::uint8_t value; };
	struct Fixture
	{
		const char* name;
		std::array<Patch, 2> patches;
		std::uint32_t repairedFnv;
		const char* sha256;
	};
	// Independent precomputed hashes keep each inner malformed record protected
	// by a valid outer SHA; an outer checksum failure cannot mask parser gaps.
	const Fixture fixtures[] = {
		{"mismatched embedded protocol", {{{132, 1, 9}, {0, 0, 0}}},
			UINT32_C(0x00000000), "331c5ffa97863dd820813b8c33543c943441a7960a7bac4b490a32ea0fd6c791"},
		{"mismatched embedded epoch", {{{136, 1, 0}, {0, 0, 0}}},
			UINT32_C(0x00000000), "8df9d031b2fe0b2a4b395048f531bb497922fb2b81593f5247c985a4507b4f4d"},
		{"original bootstrap checksum corruption", {{{112, 1, 0}, {0, 0, 0}}},
			UINT32_C(0x00000000), "427f41727cf5b06d0d969baaffc377a52ae7e92a996906b2684c22b644cfb6d9"},
		{"bootstrap reserved byte", {{{116, 1, 1}, {0, 0, 0}}},
			UINT32_C(0x00000000), "95c79766633602b0ebc7681a59fb2c77b14bc8face2af5ba46dad1dcdf045f34"},
		{"ACK reserved byte", {{{135, 1, 1}, {0, 0, 0}}},
			UINT32_C(0x00000000), "0d3c9a9695943b2c5e35ffa198a3bb2e9ac3bccb557c9e29255a52aa1260140f"},
		{"ACK wrong message kind", {{{134, 1, 2}, {0, 0, 0}}},
			UINT32_C(0x00000000), "a4e38bbfe01e68eab1bf6e78f09baa0213cb12c93a6c4cf665148dd03d00c235"},
		{"zero bootstrap and ACK epochs", {{{12, 8, 0}, {136, 8, 0}}},
			UINT32_C(0x3ebec3d5), "6af6634c860a3589dfb799ecec57ff7b806b74b9b5e0709af4eed6e05a632367"},
		{"zero campaign identity", {{{28, 32, 0}, {0, 0, 0}}},
			UINT32_C(0x7722f50d), "54b3e4a99cf31e5722aac30506dd2cff8fb4f79b3a7a79a42b0ce05003629de0"},
		{"zero runtime schema", {{{60, 4, 0}, {0, 0, 0}}},
			UINT32_C(0x21a904e9), "867ed0bc830da030a0f9820d227ffcf7cecc95693353e6dce160182eedfdf9cf"},
		{"zero content identity", {{{80, 32, 0}, {0, 0, 0}}},
			UINT32_C(0x5ea9ab6d), "6bf93288bff6d171a0ec671a79167f55d60627ba9e67de2f9d302f83e3e9992d"},
		{"zero peer identity", {{{144, 16, 0}, {0, 0, 0}}},
			UINT32_C(0x00000000), "11fb0484140dedda042211a38fd2f5d9b94ed89a11282c91f7a9906ed48f20d2"},
		{"zero reconnect token", {{{160, 32, 0}, {0, 0, 0}}},
			UINT32_C(0x00000000), "02c0478a04937ac87458423178c50b578978c9e1644527eaa34392647a575eeb"},
		{"future protocol is not an implicit migration", {{{8, 1, 10}, {132, 1, 10}}},
			UINT32_C(0xa13b41bf), "217aefd591eee97c6ab1895547d191237695abc477165a7d13ebf909d70d8097"},
		{"older unreviewed protocol remains rejected", {{{8, 1, 7}, {132, 1, 7}}},
			UINT32_C(0x80543166), "ecef3cea235fa0c4cdd7ea77dc1c1798d2a7575a2dee22cb2be67c52c002cb58"},
	};
	for (const auto& fixture : fixtures)
	{
		TemporaryStateRoot root;
		auto live = Bootstrap();
		++live.sessionEpoch;
		FullEngineCoopClientCampaignScratch scratch;
		const auto sentinel = Credential(live, 0x76);
		CHECK(scratch.prepare(root.path(), live) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			scratch.persistReconnectCredential(sentinel), "malformed migration fixture prepares");
		auto bytes = LegacyReconnectRecord();
		for (const auto& patch : fixture.patches)
			std::fill_n(bytes.begin() + patch.offset, patch.count, patch.value);
		if (fixture.repairedFnv != 0)
			for (unsigned index = 0; index < 4; ++index)
				bytes[112 + index] = static_cast<std::uint8_t>(fixture.repairedFnv >> (8 * index));
		const auto digest = Digest(fixture.sha256);
		std::copy(digest.begin(), digest.end(), bytes.begin() + 192);
		const auto path = CampaignDirectory(root.path()) / "client-reconnect-credential.bin";
		CHECK(WriteBytes(path, bytes), "malformed fixture retains safe storage permissions");
		AdmissionAck untouched = sentinel;
		CHECK(scratch.loadReconnectCredential(untouched) ==
				FullEngineCoopReconnectCredentialLoadResult::CorruptRecord &&
			SameCredential(untouched, sentinel) && ReadBytes(path) == bytes &&
			!scratch.eraseStaleReconnectCredential(), fixture.name);
	}
	TemporaryStateRoot root;
	auto live = Bootstrap();
	++live.sessionEpoch;
	FullEngineCoopClientCampaignScratch scratch;
	const auto sentinel = Credential(live, 0x77);
	CHECK(scratch.prepare(root.path(), live) ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success &&
		scratch.persistReconnectCredential(sentinel), "legacy outer checksum fixture prepares");
	auto bytes = LegacyReconnectRecord();
	bytes.back() ^= 1;
	const auto path = CampaignDirectory(root.path()) / "client-reconnect-credential.bin";
	CHECK(WriteBytes(path, bytes), "install damaged legacy outer checksum");
	AdmissionAck untouched = sentinel;
	CHECK(scratch.loadReconnectCredential(untouched) ==
			FullEngineCoopReconnectCredentialLoadResult::CorruptRecord &&
		SameCredential(untouched, sentinel) && ReadBytes(path) == bytes,
		"legacy parsing requires original outer SHA and never modifies output or damaged evidence");
}

void TestDurableRetirementMarkerLifecycle()
{
	TemporaryStateRoot root;
	const CoopCampaignBootstrapDescriptor bootstrap = Bootstrap(0x3b);
	const AdmissionAck credential = Credential(bootstrap, 0x29);
	const AdmissionAck replacement = Credential(bootstrap, 0x39);
	std::filesystem::path active;
	std::filesystem::path retired;
	std::vector<std::uint8_t> exactRecord;
	{
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			scratch.persistReconnectCredential(credential),
			"retirement fixture durably publishes the live bearer");
		const std::filesystem::path campaign = CampaignDirectory(root.path());
		active = campaign / "client-reconnect-credential.bin";
		retired = campaign / "client-reconnect-credential.retired";
		exactRecord = ReadBytes(active);

		AdmissionAck wrong = credential;
		++wrong.reconnectToken.front();
		CHECK(!scratch.retireReconnectCredential(wrong) &&
			std::filesystem::exists(active) &&
			!std::filesystem::exists(retired),
			"retirement cannot rename a different in-memory bearer");
		CHECK(scratch.retireReconnectCredential(credential) &&
			!std::filesystem::exists(active) &&
			std::filesystem::exists(retired) &&
			ReadBytes(retired) == exactRecord &&
			OwnerReadWriteOnlyFile(retired),
			"exact completion atomically renames the 0600 bearer to a fixed terminal marker");
		AdmissionAck loaded;
		CHECK(scratch.loadReconnectCredential(loaded) ==
				FullEngineCoopReconnectCredentialLoadResult::Retired &&
			loaded.peerIdentity == credential.peerIdentity &&
			loaded.reconnectToken == credential.reconnectToken &&
			scratch.retireReconnectCredential(credential) &&
			!scratch.persistReconnectCredential(replacement) &&
			!scratch.eraseStaleReconnectCredential(),
			"validated terminal marker is idempotent and can never be overwritten or erased as stale");
		scratch.close();
	}
	{
		CoopCampaignBootstrapDescriptor nextEpoch = bootstrap;
		++nextEpoch.sessionEpoch;
		FullEngineCoopClientCampaignScratch restarted;
		AdmissionAck loaded;
		CHECK(restarted.prepare(root.path(), nextEpoch) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			restarted.loadReconnectCredential(loaded) ==
				FullEngineCoopReconnectCredentialLoadResult::Retired &&
			std::filesystem::exists(retired) &&
			!std::filesystem::exists(active),
			"durable retirement remains terminal across a server process epoch change");
	}
	{
		CoopCampaignBootstrapDescriptor foreignBinding = bootstrap;
		++foreignBinding.runtimeFingerprint.low;
		FullEngineCoopClientCampaignScratch restarted;
		AdmissionAck loaded;
		CHECK(restarted.prepare(root.path(), foreignBinding) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			restarted.loadReconnectCredential(loaded) ==
				FullEngineCoopReconnectCredentialLoadResult::BindingMismatch &&
			std::filesystem::exists(retired),
			"terminal marker still exact-checks every non-epoch campaign binding");
	}
}

void TestReconnectCredentialAdversarialStorage()
{
	const CoopCampaignBootstrapDescriptor bootstrap = Bootstrap(0x46);
	const AdmissionAck credential = Credential(bootstrap, 0x24);
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			scratch.persistReconnectCredential(credential),
			"corrupt credential fixture publishes");
		const std::filesystem::path record = CampaignDirectory(root.path()) /
			"client-reconnect-credential.bin";
		std::vector<std::uint8_t> corrupted = ReadBytes(record);
		scratch.close();
		CHECK(corrupted.size() > CoopCampaignBootstrapWireSize + 32 &&
			(++corrupted[CoopCampaignBootstrapWireSize + 32],
			 WriteBytes(record, corrupted)),
			"credential bearer byte is corrupted without updating checksum");
		FullEngineCoopClientCampaignScratch restarted;
		AdmissionAck untouched = Credential(bootstrap, 0x75);
		const AdmissionAck sentinel = untouched;
		CHECK(restarted.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			restarted.loadReconnectCredential(untouched) ==
				FullEngineCoopReconnectCredentialLoadResult::CorruptRecord &&
			std::filesystem::exists(record) &&
			untouched.peerIdentity == sentinel.peerIdentity,
			"checksum corruption fails closed, retains evidence, and never returns a bearer");
	}
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			scratch.persistReconnectCredential(credential),
			"binding mismatch fixture publishes");
		const std::filesystem::path record = CampaignDirectory(root.path()) /
			"client-reconnect-credential.bin";
		scratch.close();
		CoopCampaignBootstrapDescriptor foreignRuntime = bootstrap;
		++foreignRuntime.runtimeFingerprint.low;
		FullEngineCoopClientCampaignScratch restarted;
		AdmissionAck loaded;
		CHECK(restarted.prepare(root.path(), foreignRuntime) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			restarted.loadReconnectCredential(loaded) ==
				FullEngineCoopReconnectCredentialLoadResult::BindingMismatch &&
			std::filesystem::exists(record),
			"non-epoch descriptor mismatch is retained and never downgraded to fresh join");
	}
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			scratch.persistReconnectCredential(credential),
			"staging recovery fixture publishes");
		const std::filesystem::path staging = CampaignDirectory(root.path()) /
			"client-reconnect-credential.staging";
		scratch.close();
		CHECK(WriteBytes(staging, {1, 2, 3}),
			"crash-left credential staging file is materialized");
#ifndef _WIN32
		std::filesystem::permissions(staging,
			std::filesystem::perms::owner_read |
				std::filesystem::perms::owner_write,
			std::filesystem::perm_options::replace);
#endif
		FullEngineCoopClientCampaignScratch restarted;
		AdmissionAck loaded;
		CHECK(restarted.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			!std::filesystem::exists(staging) &&
			restarted.loadReconnectCredential(loaded) ==
				FullEngineCoopReconnectCredentialLoadResult::Loaded &&
			loaded.peerIdentity == credential.peerIdentity,
			"validated stale staging is removed while the atomic target survives");
	}
#ifndef _WIN32
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			scratch.persistReconnectCredential(credential),
			"permission fixture publishes");
		const std::filesystem::path record = CampaignDirectory(root.path()) /
			"client-reconnect-credential.bin";
		scratch.close();
		std::filesystem::permissions(record,
			std::filesystem::perms::owner_read |
				std::filesystem::perms::owner_write |
				std::filesystem::perms::group_read,
			std::filesystem::perm_options::replace);
		FullEngineCoopClientCampaignScratch restarted;
		AdmissionAck loaded;
		CHECK(restarted.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			restarted.loadReconnectCredential(loaded) ==
				FullEngineCoopReconnectCredentialLoadResult::UnsafeStorage &&
			restarted.failStopped() && std::filesystem::exists(record),
			"non-0600 bearer storage is retained but permanently refused");
	}
#endif
}

void TestRetiredMarkerAdversarialStorage()
{
	const CoopCampaignBootstrapDescriptor bootstrap = Bootstrap(0x4b);
	const AdmissionAck credential = Credential(bootstrap, 0x2a);
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			scratch.persistReconnectCredential(credential),
			"ambiguous terminal-state fixture publishes an active bearer");
		const std::filesystem::path campaign = CampaignDirectory(root.path());
		const std::filesystem::path active =
			campaign / "client-reconnect-credential.bin";
		const std::filesystem::path retired =
			campaign / "client-reconnect-credential.retired";
		CHECK(WriteBytes(retired, ReadBytes(active)),
			"adversary creates a competing terminal record");
#ifndef _WIN32
		std::filesystem::permissions(retired,
			std::filesystem::perms::owner_read |
				std::filesystem::perms::owner_write,
			std::filesystem::perm_options::replace);
#endif
		AdmissionAck loaded;
		CHECK(scratch.loadReconnectCredential(loaded) ==
				FullEngineCoopReconnectCredentialLoadResult::UnsafeStorage &&
			scratch.failStopped() && std::filesystem::exists(active) &&
			std::filesystem::exists(retired),
			"active plus retired evidence is ambiguous and fails closed without replacement");
	}
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			scratch.persistReconnectCredential(credential),
			"corrupt retired-marker fixture publishes an active bearer");
		const std::filesystem::path campaign = CampaignDirectory(root.path());
		const std::filesystem::path active =
			campaign / "client-reconnect-credential.bin";
		const std::filesystem::path retired =
			campaign / "client-reconnect-credential.retired";
		scratch.close();
		std::error_code error;
		std::filesystem::rename(active, retired, error);
		std::vector<std::uint8_t> corrupted = ReadBytes(retired);
		CHECK(!error && !corrupted.empty() &&
			(++corrupted.back(), WriteBytes(retired, corrupted)),
			"post-rename marker bytes are corrupted without updating the digest");
		FullEngineCoopClientCampaignScratch restarted;
		AdmissionAck loaded;
		CHECK(restarted.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			restarted.loadReconnectCredential(loaded) ==
				FullEngineCoopReconnectCredentialLoadResult::CorruptRecord &&
			std::filesystem::exists(retired) &&
			!std::filesystem::exists(active),
			"corrupt terminal evidence never degrades to Missing or first admission");
	}
#ifndef _WIN32
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
				FullEngineCoopClientCampaignScratchPrepareResult::Success &&
			scratch.persistReconnectCredential(credential),
			"exclusive-rename fault fixture publishes an active bearer");
		const std::filesystem::path campaign = CampaignDirectory(root.path());
		const std::filesystem::path active =
			campaign / "client-reconnect-credential.bin";
		const std::filesystem::path retired =
			campaign / "client-reconnect-credential.retired";
		std::filesystem::permissions(campaign,
			std::filesystem::perms::owner_read |
				std::filesystem::perms::owner_exec,
			std::filesystem::perm_options::replace);
		CHECK(!scratch.retireReconnectCredential(credential) &&
			scratch.failStopped() && std::filesystem::exists(active) &&
			!std::filesystem::exists(retired),
			"exclusive rename failure retains the live bearer and fail-stops the process");
		std::filesystem::permissions(campaign,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace);
	}
#endif
}

void TestSequentialCommitAlternationAndAbort()
{
	ResetBridge();
	TemporaryStateRoot root;
	const CoopCampaignBootstrapDescriptor bootstrap = Bootstrap(0x40);
	FullEngineCoopClientCampaignScratch scratch;
	CHECK(scratch.prepare(root.path(), bootstrap) ==
		FullEngineCoopClientCampaignScratchPrepareResult::Success,
		"sequential transfer scratch prepares");
	ActiveProfile = scratch.profileDirectory();
	const std::filesystem::path campaign = CampaignDirectory(root.path());
	const std::vector<std::uint8_t> abc{'a', 'b', 'c'};
	const CoopCampaignSyncMetadata first =
		Metadata(bootstrap, 1, abc.size(), AbcDigest);
	CHECK(scratch.begin(first) ==
			FullEngineCoopCampaignScratchBeginResult::Success &&
		!scratch.hasActiveCheckpoint(),
		"first transfer opens inactive backend slot A without publishing it");
	CHECK(scratch.writeExact(1, abc.data(), 1) ==
			FullEngineCoopCampaignScratchWriteResult::StorageFailure &&
		FileSize(campaign / "checkpoint-a.sav") == 0,
		"a gap is rejected without changing the held staging file");
	CHECK(scratch.writeExact(0, abc.data(), 1) ==
			FullEngineCoopCampaignScratchWriteResult::Success &&
		scratch.writeExact(0, abc.data() + 1, 1) ==
			FullEngineCoopCampaignScratchWriteResult::StorageFailure &&
		ReadBytes(campaign / "checkpoint-a.sav") ==
			std::vector<std::uint8_t>{'a'} &&
		scratch.writeExact(1, abc.data() + 1, 2) ==
			FullEngineCoopCampaignScratchWriteResult::Success,
		"overlap rejection preserves both cursor and previously accepted bytes");
	CoopCampaignSyncMetadata wrongMetadata = first;
	++wrongMetadata.worldMinutes;
	CHECK(scratch.commitAndLoad(wrongMetadata) ==
		FullEngineCoopCampaignScratchCommitResult::CompatibilityMismatch,
		"commit exact-matches the metadata that opened the held transfer");
	ExpectedCheckpoint = abc;
	CHECK(scratch.commitAndLoad(first) ==
			FullEngineCoopCampaignScratchCommitResult::Committed &&
		scratch.hasActiveCheckpoint() &&
		scratch.activeSlot() == DedicatedCampaignSlot::A &&
		scratch.activeGeneration() == 1 &&
		ValidatedSlots == std::vector<DedicatedCampaignSlot>{
			DedicatedCampaignSlot::A} &&
		LoadedSlots == ValidatedSlots && SaveCalls == 0 &&
		ReadBytes(ActiveProfile /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A)) == abc,
		"verified materialization validates and loads before publishing slot A");

	const std::vector<std::uint8_t> hello{'h', 'e', 'l', 'l', 'o'};
	const CoopCampaignSyncMetadata second =
		Metadata(bootstrap, 2, hello.size(), HelloDigest);
	ExpectedCheckpoint = hello;
	CHECK(scratch.begin(second) ==
			FullEngineCoopCampaignScratchBeginResult::Success &&
		scratch.writeExact(0, hello.data(), hello.size()) ==
			FullEngineCoopCampaignScratchWriteResult::Success &&
		scratch.commitAndLoad(second) ==
			FullEngineCoopCampaignScratchCommitResult::Committed &&
		scratch.activeSlot() == DedicatedCampaignSlot::B &&
		scratch.activeGeneration() == 2 &&
		ValidatedSlots.back() == DedicatedCampaignSlot::B &&
		LoadedSlots.back() == DedicatedCampaignSlot::B,
		"a later generation alternates to slot B and publishes only after load");
	CHECK(scratch.begin(first) ==
		FullEngineCoopCampaignScratchBeginResult::StorageFailure,
		"scratch independently refuses generation rollback after a committed load");

	CoopCampaignSyncMetadata sameGeneration = second;
	++sameGeneration.transfer.transferId;
	ExpectedCheckpoint = hello;
	CHECK(scratch.begin(sameGeneration) ==
			FullEngineCoopCampaignScratchBeginResult::Success &&
		scratch.writeExact(0, hello.data(), hello.size()) ==
			FullEngineCoopCampaignScratchWriteResult::Success &&
		scratch.commitAndLoad(sameGeneration) ==
			FullEngineCoopCampaignScratchCommitResult::Committed &&
		scratch.activeGeneration() == 2 &&
		scratch.activeSlot() == DedicatedCampaignSlot::A &&
		LoadedSlots.size() == 3,
		"a reconnect may recommit the exact active checkpoint under a fresh transfer id");
	CoopCampaignSyncMetadata equivocating = sameGeneration;
	++equivocating.transfer.transferId;
	++equivocating.worldMinutes;
	CHECK(scratch.begin(equivocating) ==
			FullEngineCoopCampaignScratchBeginResult::StorageFailure &&
		scratch.activeGeneration() == 2 &&
		scratch.activeSlot() == DedicatedCampaignSlot::A,
		"same-generation metadata equivocation is rejected without replacing the active checkpoint");

	CoopCampaignSyncMetadata third = first;
	third.transfer.transferId = 104;
	third.transfer.checkpointGeneration = 3;
	third.worldMinutes = 180;
	CHECK(scratch.begin(third) ==
			FullEngineCoopCampaignScratchBeginResult::Success &&
		scratch.writeExact(0, abc.data(), abc.size()) ==
			FullEngineCoopCampaignScratchWriteResult::Success,
		"third generation reuses the now-inactive slot");
	scratch.abort();
	CHECK(FileSize(campaign / "checkpoint-b.sav") == 0 &&
		scratch.activeSlot() == DedicatedCampaignSlot::A &&
		scratch.activeGeneration() == 2,
		"abort truncates only uncommitted staging and preserves published state");
	scratch.close();
}

void TestFailureClassificationAndFailStop()
{
	ResetBridge();
	const std::vector<std::uint8_t> abc{'a', 'b', 'c'};
	const CoopCampaignBootstrapDescriptor bootstrap = Bootstrap(0x50);
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success,
			"size/hash failure fixture prepares");
		ActiveProfile = scratch.profileDirectory();
		const CoopCampaignSyncMetadata metadata =
			Metadata(bootstrap, 1, abc.size(), AbcDigest);
		CHECK(scratch.begin(metadata) ==
				FullEngineCoopCampaignScratchBeginResult::Success &&
			scratch.writeExact(0, abc.data(), 1) ==
				FullEngineCoopCampaignScratchWriteResult::Success &&
			scratch.commitAndLoad(metadata) ==
				FullEngineCoopCampaignScratchCommitResult::HashMismatch,
			"commit rejects an incomplete exact-size stream before materialization");
		scratch.abort();
		CoopCampaignSyncMetadata badHash = metadata;
		badHash.transfer.transferId = 102;
		badHash.transfer.checkpointSha256 = HelloDigest;
		CHECK(scratch.begin(badHash) ==
				FullEngineCoopCampaignScratchBeginResult::Success &&
			scratch.writeExact(0, abc.data(), abc.size()) ==
				FullEngineCoopCampaignScratchWriteResult::Success &&
			scratch.commitAndLoad(badHash) ==
				FullEngineCoopCampaignScratchCommitResult::HashMismatch &&
			ValidatedSlots.empty() && LoadedSlots.empty(),
			"full-size bytes with the wrong SHA never reach JA2 validation or load");
		scratch.abort();
		CoopCampaignSyncMetadata oversized = metadata;
		oversized.transfer.transferId = 103;
		oversized.transfer.totalSize =
			DedicatedCampaignMaximumCheckpointBytes + 1;
		CHECK(scratch.begin(oversized) ==
			FullEngineCoopCampaignScratchBeginResult::CapacityReached,
			"begin enforces the 256 MiB bound before opening a staging file");
	}
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success,
			"path replacement fixture prepares");
		const std::filesystem::path campaign = CampaignDirectory(root.path());
		const std::filesystem::path slot = campaign / "checkpoint-a.sav";
		const std::filesystem::path held = campaign / "held-away.sav";
		const CoopCampaignSyncMetadata metadata =
			Metadata(bootstrap, 1, abc.size(), AbcDigest);
		CHECK(scratch.begin(metadata) ==
			FullEngineCoopCampaignScratchBeginResult::Success,
			"path replacement transfer begins");
		std::error_code error;
		std::filesystem::rename(slot, held, error);
		CHECK(!error && WriteBytes(slot, {'x'}),
			"staging pathname is replaced while the original handle remains held");
		CHECK(scratch.writeExact(0, abc.data(), abc.size()) ==
				FullEngineCoopCampaignScratchWriteResult::StorageFailure &&
			ReadBytes(held).empty() && ReadBytes(slot) ==
				std::vector<std::uint8_t>{'x'},
			"identity rejection preserves both held bytes and hostile replacement");
		std::filesystem::remove(slot, error);
		error.clear();
		std::filesystem::rename(held, slot, error);
		CHECK(!error && scratch.writeExact(0, abc.data(), abc.size()) ==
			FullEngineCoopCampaignScratchWriteResult::Success,
			"restoring the exact held identity proves the rejected write kept its cursor");
		scratch.abort();
	}
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success,
			"materialization failure fixture prepares");
		ActiveProfile = scratch.profileDirectory();
		const CoopCampaignSyncMetadata metadata =
			Metadata(bootstrap, 1, abc.size(), AbcDigest);
		CHECK(scratch.begin(metadata) ==
				FullEngineCoopCampaignScratchBeginResult::Success &&
			scratch.writeExact(0, abc.data(), abc.size()) ==
				FullEngineCoopCampaignScratchWriteResult::Success,
			"materialization failure receives authenticated bytes");
		const std::filesystem::path logical = ActiveProfile /
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::A);
		std::error_code error;
		std::filesystem::remove(logical, error);
		error.clear();
		std::filesystem::create_symlink(
			DedicatedCampaignLogicalScratch(DedicatedCampaignSlot::B),
			logical, error);
		if (!error)
		{
			CHECK(scratch.commitAndLoad(metadata) ==
					FullEngineCoopCampaignScratchCommitResult::StorageFailure &&
				ValidatedSlots.empty() && LoadedSlots.empty(),
				"unsafe VFS destination makes materialization fail before JA2");
		}
		scratch.abort();
	}
	{
		TemporaryStateRoot root;
		FullEngineCoopClientCampaignScratch scratch;
		CHECK(scratch.prepare(root.path(), bootstrap) ==
			FullEngineCoopClientCampaignScratchPrepareResult::Success,
			"validation/load failure fixture prepares");
		ActiveProfile = scratch.profileDirectory();
		const CoopCampaignSyncMetadata metadata =
			Metadata(bootstrap, 1, abc.size(), AbcDigest);
		ExpectedCheckpoint = abc;
		ValidationSucceeds = false;
		CHECK(scratch.begin(metadata) ==
				FullEngineCoopCampaignScratchBeginResult::Success &&
			scratch.writeExact(0, abc.data(), abc.size()) ==
				FullEngineCoopCampaignScratchWriteResult::Success &&
			scratch.commitAndLoad(metadata) ==
				FullEngineCoopCampaignScratchCommitResult::CompatibilityMismatch &&
			!scratch.failStopped() && LoadedSlots.empty() &&
			!scratch.hasActiveCheckpoint(),
			"strict validation failure is classified without loading or publication");
		scratch.abort();
		ValidationSucceeds = true;
		LoadSucceeds = false;
		CoopCampaignSyncMetadata retry = metadata;
		retry.transfer.transferId = 102;
		CHECK(scratch.begin(retry) ==
				FullEngineCoopCampaignScratchBeginResult::Success &&
			scratch.writeExact(0, abc.data(), abc.size()) ==
				FullEngineCoopCampaignScratchWriteResult::Success &&
			scratch.commitAndLoad(retry) ==
				FullEngineCoopCampaignScratchCommitResult::LoadFailed &&
			scratch.failStopped() && !scratch.hasActiveCheckpoint() &&
			scratch.begin(retry) ==
				FullEngineCoopCampaignScratchBeginResult::StorageFailure,
			"load failure is fail-stop and can never report or later publish committed");
		scratch.abort();
	}
}
}

bool SaveDedicatedCampaignGame(DedicatedCampaignSlot) noexcept
{
	++SaveCalls;
	return false;
}

bool ValidateDedicatedCampaignGame(DedicatedCampaignSlot slot) noexcept
{
	ValidatedSlots.push_back(slot);
	return ValidationSucceeds && !ActiveProfile.empty() &&
		ReadBytes(ActiveProfile / DedicatedCampaignLogicalScratch(slot)) ==
			ExpectedCheckpoint;
}

bool LoadDedicatedCampaignGame(DedicatedCampaignSlot slot) noexcept
{
	LoadedSlots.push_back(slot);
	return LoadSucceeds && !ActiveProfile.empty() &&
		ReadBytes(ActiveProfile / DedicatedCampaignLogicalScratch(slot)) ==
			ExpectedCheckpoint;
}

int main()
{
	TestPrepareLeaseRestartAndIdentity();
	TestRestartAllowlistRejectsWritableAliases();
	TestRestartResetsVfsOwnedDisposableProfile();
	TestDurableReconnectCredentialLifecycle();
	TestLegacyReconnectCredentialMigration();
	TestLegacyReconnectCredentialMalformedRecords();
	TestDurableRetirementMarkerLifecycle();
	TestReconnectCredentialAdversarialStorage();
	TestRetiredMarkerAdversarialStorage();
	TestSequentialCommitAlternationAndAbort();
	TestFailureClassificationAndFailStop();
	if (Failures != 0)
	{
		std::printf("%d full-engine co-op client scratch test(s) failed\n",
			Failures);
		return 1;
	}
	std::puts("all full-engine co-op client scratch tests passed");
	return 0;
}
