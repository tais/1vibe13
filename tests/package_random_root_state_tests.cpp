#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/ByteStorage.h>
#include <Engine/Core/PackageRandomSource.h>
#include <Engine/Core/PackageSaveArchive.h>
#include <Engine/Core/PersistenceService.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr std::uint32_t PackageSaveArchiveMagic = 0x54534750u;
constexpr std::uint16_t LegacyPackageSaveArchiveVersion = 3;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::fprintf(stderr, "FAIL: %s\n", message);
	std::exit(1);
}

void Check(PackageRandomResult result, const char* message)
{
	Check(static_cast<bool>(result), message);
}

void CheckUnchanged(const PackageRandomSource& random,
	const PackageRandomCheckpoint& before, const char* message)
{
	Check(random.checkpoint() == before, message);
}

PackageRandomCheckpoint LegacyCheckpoint(PackageRandomCheckpoint checkpoint)
{
	checkpoint.schema = PackageRandomCheckpoint::LegacySchema;
	checkpoint.rootSeed = 0;
	checkpoint.maximumStreams = 0;
	return checkpoint;
}

void TestCurrentCheckpointAdoptsRootBeforeCreatingANewStream()
{
	PackageRandomSource source("rules.campaign", 0x1020304050607080ULL, 4);
	Check(source.next("existing", 1000000),
		"source creates an existing stream before capture");
	const PackageRandomCheckpoint saved = source.checkpoint();
	Check(saved.schema == PackageRandomCheckpoint::CurrentSchema &&
		saved.packageId == "rules.campaign" &&
		saved.rootSeed == source.rootSeed() && saved.rootSeed != 0 &&
		saved.maximumStreams == 4 && saved.streams.size() == 1,
		"schema 2 binds package identity, package-local root seed, and stream limit");

	const PackageRandomResult expectedExisting = source.next("existing", 1000000);
	const PackageRandomResult expectedFirstLate =
		source.next("created.after.restore", 1000000);
	PackageRandomSource resumed("rules.campaign", 0xfedcba9876543210ULL, 4);
	Check(resumed.checkpoint().rootSeed != saved.rootSeed,
		"resume fixture begins with a different externally supplied host seed");
	Check(resumed.next("discarded.before.restore", 1000000),
		"resume fixture contains state that restore must replace");
	Check(resumed.restoreCheckpoint(saved) == PackageRandomCheckpointError::None &&
		resumed.checkpoint() == saved,
		"schema 2 restore adopts the complete saved root and stream state");
	const PackageRandomResult resumedExisting = resumed.next("existing", 1000000);
	const PackageRandomResult resumedFirstLate =
		resumed.next("created.after.restore", 1000000);
	Check(expectedExisting && resumedExisting &&
		expectedExisting.value == resumedExisting.value &&
		expectedFirstLate && resumedFirstLate &&
		expectedFirstLate.value == resumedFirstLate.value,
		"a stream first created after resume derives from the saved package root");
}

void TestValidationFailuresAreTransactional()
{
	PackageRandomSource source("rules.campaign", 11, 2);
	Check(source.next("first", 1000), "source creates its first stream");
	const PackageRandomCheckpoint valid = source.checkpoint();

	PackageRandomSource target("rules.campaign", 22, 2);
	Check(target.next("keep", 1000), "target establishes rollback state");
	const PackageRandomCheckpoint before = target.checkpoint();

	PackageRandomCheckpoint wrongPackage = valid;
	wrongPackage.packageId = "rules.other";
	Check(target.restoreCheckpoint(wrongPackage) ==
			PackageRandomCheckpointError::PackageMismatch,
		"a checkpoint for another package is rejected");
	CheckUnchanged(target, before,
		"package mismatch leaves root seed and streams unchanged");

	PackageRandomCheckpoint wrongLimit = valid;
	wrongLimit.maximumStreams = 3;
	Check(target.restoreCheckpoint(wrongLimit) ==
			PackageRandomCheckpointError::StreamLimitMismatch,
		"a schema 2 checkpoint with a different stream contract is rejected");
	CheckUnchanged(target, before,
		"stream-limit mismatch leaves root seed and streams unchanged");

	PackageRandomCheckpoint invalidSchema = valid;
	invalidSchema.schema = 99;
	Check(target.restoreCheckpoint(invalidSchema) ==
			PackageRandomCheckpointError::InvalidSchema,
		"an unsupported checkpoint schema is rejected");
	CheckUnchanged(target, before,
		"schema rejection leaves root seed and streams unchanged");

	PackageRandomCheckpoint nonCanonicalLegacy = LegacyCheckpoint(valid);
	nonCanonicalLegacy.rootSeed = 1;
	Check(target.restoreCheckpoint(nonCanonicalLegacy) ==
			PackageRandomCheckpointError::InvalidSchema,
		"legacy checkpoints reject metadata that their wire format never carried");
	CheckUnchanged(target, before,
		"noncanonical legacy metadata leaves root seed and streams unchanged");

	PackageRandomCheckpoint duplicate = valid;
	duplicate.streams.push_back(duplicate.streams.front());
	Check(target.restoreCheckpoint(duplicate) ==
			PackageRandomCheckpointError::DuplicateStream,
		"duplicate stream identities are rejected before mutation");
	CheckUnchanged(target, before,
		"duplicate rejection leaves root seed and streams unchanged");

	PackageRandomCheckpoint tooMany = LegacyCheckpoint(valid);
	tooMany.streams.push_back({"second", 7, 0});
	PackageRandomSource tightlyBounded("rules.campaign", 22, 1);
	const PackageRandomCheckpoint tightlyBoundedBefore = tightlyBounded.checkpoint();
	Check(tightlyBounded.restoreCheckpoint(tooMany) ==
			PackageRandomCheckpointError::TooManyStreams,
		"legacy checkpoints still obey the target's resource bound");
	CheckUnchanged(tightlyBounded, tightlyBoundedBefore,
		"resource rejection is transactional for legacy checkpoints");
}

void TestRestoreAllocationAndReusablePaths()
{
	PackageRandomSource source("rules.paths", 0x1111, 3);
	Check(source.next("alpha", 1000) && source.next("beta", 1000),
		"source establishes two saved streams");
	const PackageRandomCheckpoint saved = source.checkpoint();

	PackageRandomSource allocationPath("rules.paths", 0x2222, 3);
	Check(allocationPath.next("stale", 1000),
		"allocation-path target starts with a disjoint stream");
	Check(allocationPath.restoreCheckpoint(saved) ==
			PackageRandomCheckpointError::None &&
		allocationPath.checkpoint() == saved,
		"restore builds and publishes a complete replacement map atomically");

	PackageRandomSource reusablePath("rules.paths", 0x3333, 3);
	Check(reusablePath.next("alpha", 10) && reusablePath.next("beta", 10) &&
		reusablePath.next("extra", 10),
		"reusable-path target contains every saved stream and one extra");
	Check(reusablePath.restoreCheckpoint(saved) ==
			PackageRandomCheckpointError::None &&
		reusablePath.checkpoint() == saved,
		"non-allocating restore updates retained streams, removes extras, and adopts root");
}

void TestLegacyCheckpointRetainsExternalRoot()
{
	PackageRandomSource legacySource("rules.legacy", 41, 4);
	Check(legacySource.next("existing", 1000),
		"legacy source establishes persisted stream state");
	const PackageRandomCheckpoint legacy =
		LegacyCheckpoint(legacySource.checkpoint());
	const PackageRandomResult expectedExisting = legacySource.next("existing", 1000);

	PackageRandomSource target("rules.legacy", 99, 4);
	PackageRandomSource externalSeedBaseline("rules.legacy", 99, 4);
	const std::uint64_t externalRoot = target.checkpoint().rootSeed;
	Check(target.restoreCheckpoint(legacy) == PackageRandomCheckpointError::None &&
		target.checkpoint().rootSeed == externalRoot,
		"schema 1 restores existing streams while retaining the runtime root seed");
	const PackageRandomResult restoredExisting = target.next("existing", 1000);
	const PackageRandomResult legacyLate = target.next("created.after.restore", 1000);
	const PackageRandomResult baselineLate =
		externalSeedBaseline.next("created.after.restore", 1000);
	Check(expectedExisting && restoredExisting &&
		expectedExisting.value == restoredExisting.value &&
		legacyLate && baselineLate && legacyLate.value == baselineLate.value,
		"legacy stream history restores exactly and future streams use the external root");
}

std::vector<std::uint8_t> EncodeLegacyV3Archive(
	PersistenceService& persistence,
	RuntimeCompatibilityFingerprint compatibility,
	const PackageRandomStreamCheckpoint& stream)
{
	BinaryWriter payload;
	payload.writeU32(compatibility.schema);
	payload.writeU64(compatibility.high);
	payload.writeU64(compatibility.low);
	payload.writeU32(0); // No package-owned payload records.
	payload.writeU32(1); // One engine-owned record.
	payload.writeString("rules.legacy");
	payload.writeString("1.0");
	payload.writeU32(PackageRandomCheckpoint::LegacySchema);
	payload.writeU32(1);
	payload.writeString(stream.id);
	payload.writeU64(stream.state);
	payload.writeU64(stream.valuesGenerated);
	std::vector<std::uint8_t> encoded;
	Check(persistence.encodeEnvelope(
		PersistenceHeader{PackageSaveArchiveMagic,
			LegacyPackageSaveArchiveVersion},
		payload.bytes(), encoded) == PersistenceSaveResult::Success,
		"legacy archive fixture is wrapped in a valid integrity envelope");
	return encoded;
}

void TestArchiveV4RoundTripAndV3Compatibility()
{
	MemoryByteStorage storage;
	PersistenceService persistence(storage, 4096);
	PackageSaveArchiveService archives(persistence, 4, 1024, 4096, 4);
	const RuntimeCompatibilityFingerprint compatibility{1,
		0x0123456789abcdefULL, 0xfedcba9876543210ULL};

	PackageRandomSource currentSource("rules.current", 0x8877665544332211ULL, 4);
	Check(currentSource.next("combat", 1000),
		"current archive fixture contains one stream");
	PackageSaveArchive current{compatibility, {}};
	current.state.engineRecords.push_back(
		{"rules.current", "2.0", currentSource.checkpoint()});
	std::vector<std::uint8_t> encoded{0xff};
	Check(archives.encode(current, encoded) == PackageSaveArchiveSaveError::None &&
		encoded.size() > 6 && encoded[4] == 4 && encoded[5] == 0,
		"new package archives explicitly encode PGST v4");
	PackageSaveArchive decoded;
	Check(archives.decode(encoded, compatibility, decoded) &&
		decoded.state.engineRecords.size() == 1 &&
		decoded.state.engineRecords[0].random == currentSource.checkpoint(),
		"PGST v4 canonically round-trips package root seed and stream limit");

	PackageSaveArchive inconsistent = current;
	inconsistent.state.engineRecords[0].random.maximumStreams = 0;
	std::vector<std::uint8_t> unchanged{7, 8, 9};
	Check(archives.encode(inconsistent, unchanged) ==
			PackageSaveArchiveSaveError::TooManyRandomStreams &&
		unchanged == std::vector<std::uint8_t>({7, 8, 9}),
		"archive encoding rejects an inconsistent stream limit transactionally");

	PackageRandomSource oldSource("rules.legacy", 41, 4);
	Check(oldSource.next("existing", 1000),
		"PGST v3 fixture contains an existing stream");
	const PackageRandomCheckpoint oldState = oldSource.checkpoint();
	const std::vector<std::uint8_t> encodedV3 = EncodeLegacyV3Archive(
		persistence, compatibility, oldState.streams.front());
	PackageSaveArchive decodedV3;
	Check(archives.decode(encodedV3, compatibility, decodedV3) &&
		decodedV3.state.engineRecords.size() == 1 &&
		decodedV3.state.engineRecords[0].random.schema ==
			PackageRandomCheckpoint::LegacySchema &&
		decodedV3.state.engineRecords[0].random.rootSeed == 0 &&
		decodedV3.state.engineRecords[0].random.maximumStreams == 0,
		"PGST v3 decodes explicitly as an incomplete legacy RNG checkpoint");

	PackageRandomSource legacyTarget("rules.legacy", 99, 4);
	PackageRandomSource legacyRootBaseline("rules.legacy", 99, 4);
	Check(legacyTarget.restoreCheckpoint(
			decodedV3.state.engineRecords[0].random) ==
			PackageRandomCheckpointError::None,
		"an interactive runtime can restore the decoded PGST v3 stream state");
	const PackageRandomResult targetLate =
		legacyTarget.next("created.after.restore", 1000);
	const PackageRandomResult baselineLate =
		legacyRootBaseline.next("created.after.restore", 1000);
	Check(targetLate && baselineLate && targetLate.value == baselineLate.value,
		"decoded PGST v3 state retains the runtime's external root for future streams");
}
}

int main()
{
	static_assert(PackageRandomCheckpoint::LegacySchema == 1,
		"legacy package RNG schema is a stable compatibility contract");
	static_assert(PackageRandomCheckpoint::CurrentSchema == 2,
		"complete package RNG checkpoints use schema 2");
	TestCurrentCheckpointAdoptsRootBeforeCreatingANewStream();
	TestValidationFailuresAreTransactional();
	TestRestoreAllocationAndReusablePaths();
	TestLegacyCheckpointRetainsExternalRoot();
	TestArchiveV4RoundTripAndV3Compatibility();
	std::puts("package random root-state tests passed");
	return 0;
}
