#include <Engine/Core/EngineHost.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace
{
constexpr std::uint32_t CheckpointMagic = 0x504b4843u;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::fprintf(stderr, "FAIL: %s\n", message);
	std::exit(1);
}

RuntimeCheckpoint ValidCheckpoint()
{
	RuntimeCheckpoint checkpoint;
	checkpoint.compatibility = RuntimeCompatibilityFingerprint{
		7, 0x0102030405060708ull, 0x1112131415161718ull};
	checkpoint.completedFrames = 7;
	checkpoint.completedSimulationTicks = 5;
	checkpoint.activePackages = {{"rules.alpha", "2.0"}};
	checkpoint.frameBoundary = FrameDriverBoundaryState{7, 10, false};
	checkpoint.simulationTickBoundary =
		SimulationTickBoundaryState{10, 4, 5, 50, 3, false};
	return checkpoint;
}

std::vector<std::uint8_t> Envelope(PersistenceService& persistence,
	std::uint16_t version, const std::vector<std::uint8_t>& payload)
{
	std::vector<std::uint8_t> encoded;
	Check(persistence.encodeEnvelope(
		PersistenceHeader{CheckpointMagic, version}, payload, encoded) ==
		PersistenceSaveResult::Success, "test fixture envelope encodes");
	return encoded;
}

void TestV2RoundTripAndRedundantCounters()
{
	MemoryByteStorage storage;
	PersistenceService persistence(storage, 4096);
	RuntimeCheckpointService service(persistence, 4);
	const RuntimeCheckpoint original = ValidCheckpoint();

	std::vector<std::uint8_t> encoded;
	Check(service.encode(original, encoded) ==
		RuntimeCheckpointSaveError::None && !encoded.empty(),
		"v2 checkpoint encodes a complete deterministic boundary");
	PersistenceHeader header{};
	std::vector<std::uint8_t> payload;
	Check(persistence.decodeEnvelope(encoded, CheckpointMagic,
		RuntimeCheckpointService::LegacyVersion,
		RuntimeCheckpointService::CurrentVersion, header, payload) ==
		PersistenceLoadResult::Success &&
		header.version == RuntimeCheckpointService::CurrentVersion,
		"the current encoder publishes CHKP v2");

	RuntimeCheckpoint decoded;
	const RuntimeCheckpointLoadResult result = service.decode(
		encoded, original.compatibility, decoded);
	Check(result &&
		result.storedVersion == RuntimeCheckpointService::CurrentVersion &&
		result.hasDeterministicBoundary &&
		decoded.compatibility == original.compatibility &&
		decoded.completedFrames == original.completedFrames &&
		decoded.completedSimulationTicks ==
			original.completedSimulationTicks &&
		decoded.activePackages.size() == 1 &&
		decoded.activePackages[0].id == "rules.alpha" &&
		decoded.frameBoundary == original.frameBoundary &&
		decoded.simulationTickBoundary == original.simulationTickBoundary,
		"v2 decode publishes metadata and the complete boundary");

	std::vector<std::uint8_t> unchanged{9, 8, 7};
	RuntimeCheckpoint mismatched = original;
	++mismatched.completedFrames;
	Check(service.encode(mismatched, unchanged) ==
			RuntimeCheckpointSaveError::InvalidCheckpoint &&
		unchanged == std::vector<std::uint8_t>({9, 8, 7}),
		"frame metadata cannot disagree with its authoritative boundary");
	mismatched = original;
	++mismatched.completedSimulationTicks;
	Check(service.encode(mismatched, unchanged) ==
			RuntimeCheckpointSaveError::InvalidCheckpoint &&
		unchanged == std::vector<std::uint8_t>({9, 8, 7}),
		"tick metadata cannot disagree with its authoritative boundary");
}

void TestV1FixtureRemainsMetadataOnly()
{
	// Produced by the former CHKP v1 encoder. This literal pins the original
	// envelope, checksum, field order, string lengths, and payload bytes.
	const std::vector<std::uint8_t> fixture = {
		0x43, 0x48, 0x4b, 0x50, 0x01, 0x00, 0x3e, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0xe8,
		0x7c, 0xd7, 0x07, 0x00, 0x00, 0x00, 0x08, 0x07,
		0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x18, 0x17,
		0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x11, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
		0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x72, 0x75,
		0x6c, 0x65, 0x73, 0x2e, 0x61, 0x6c, 0x70, 0x68,
		0x61, 0x03, 0x00, 0x00, 0x00, 0x32, 0x2e, 0x30};
	MemoryByteStorage storage;
	PersistenceService persistence(storage, 4096);
	RuntimeCheckpointService service(persistence, 4);
	const RuntimeCompatibilityFingerprint compatibility{
		7, 0x0102030405060708ull, 0x1112131415161718ull};
	RuntimeCheckpoint legacyMetadata;
	legacyMetadata.compatibility = compatibility;
	legacyMetadata.completedFrames = 17;
	legacyMetadata.completedSimulationTicks = 23;
	legacyMetadata.activePackages = {{"rules.alpha", "2.0"}};
	legacyMetadata.frameBoundary = FrameDriverBoundaryState{99, 100, false};
	legacyMetadata.simulationTickBoundary =
		SimulationTickBoundaryState{10, 4, 8, 80, 0, false};
	std::vector<std::uint8_t> reencoded;
	Check(service.encodeLegacyMetadata(legacyMetadata, reencoded) ==
			RuntimeCheckpointSaveError::None && reencoded == fixture,
		"the explicit legacy encoder preserves the byte-pinned CHKP v1 wire");
	RuntimeCheckpoint decoded;
	const RuntimeCheckpointLoadResult result = service.decode(
		fixture, compatibility, decoded);
	Check(result &&
		result.storedVersion == RuntimeCheckpointService::LegacyVersion &&
		!result.hasDeterministicBoundary &&
		decoded.completedFrames == 17 &&
		decoded.completedSimulationTicks == 23 &&
		decoded.activePackages.size() == 1 &&
		decoded.activePackages[0].id == "rules.alpha" &&
		decoded.activePackages[0].version == "2.0" &&
		decoded.frameBoundary == FrameDriverBoundaryState{} &&
		decoded.simulationTickBoundary == SimulationTickBoundaryState{},
		"the byte-pinned v1 fixture decodes as metadata only");

	RuntimeCheckpoint sentinel = ValidCheckpoint();
	const RuntimeCheckpointLoadResult incompatible = service.decode(fixture,
		RuntimeCompatibilityFingerprint{7, 1, 2}, sentinel);
	Check(incompatible.error ==
			RuntimeCheckpointLoadError::IncompatibleRuntime &&
		incompatible.storedCompatibility == compatibility &&
		incompatible.storedVersion == RuntimeCheckpointService::LegacyVersion &&
		!incompatible.hasDeterministicBoundary &&
		sentinel.frameBoundary == ValidCheckpoint().frameBoundary,
		"v1 compatibility rejection reports its version without publishing state");
}

void TestV2CanonicalWireRejectionIsTransactional()
{
	MemoryByteStorage storage;
	PersistenceService persistence(storage, 4096);
	RuntimeCheckpointService service(persistence, 4);
	const RuntimeCheckpoint original = ValidCheckpoint();
	std::vector<std::uint8_t> encoded;
	Check(service.encode(original, encoded) ==
		RuntimeCheckpointSaveError::None, "canonical fixture encodes");
	PersistenceHeader header{};
	std::vector<std::uint8_t> payload;
	Check(persistence.decodeEnvelope(encoded, CheckpointMagic, 2, 2,
		header, payload) == PersistenceLoadResult::Success &&
		payload.size() == 120,
		"v2 fixture has the expected fixed suffix after one package");

	// The common metadata and one package consume 62 bytes. The two booleans
	// are the ninth and final bytes of their respective boundary records.
	const std::size_t frameBoolean = 62 + 16;
	const std::size_t tickBoolean = 62 + 17 + 40;
	auto rejected = [&](std::vector<std::uint8_t> malformed,
		const char* message)
	{
		RuntimeCheckpoint destination = ValidCheckpoint();
		destination.completedFrames = 99;
		destination.frameBoundary = FrameDriverBoundaryState{99, 100, false};
		const RuntimeCheckpointLoadResult result = service.decode(
			Envelope(persistence, 2, malformed), original.compatibility,
			destination);
		Check(result.error == RuntimeCheckpointLoadError::MalformedPayload &&
			result.storedVersion == 2 && !result.hasDeterministicBoundary &&
			destination.completedFrames == 99 &&
			destination.frameBoundary ==
				FrameDriverBoundaryState{99, 100, false}, message);
	};

	std::vector<std::uint8_t> malformed = payload;
	malformed[frameBoolean] = 2;
	rejected(std::move(malformed),
		"noncanonical frame booleans are rejected transactionally");
	malformed = payload;
	malformed[tickBoolean] = 0xff;
	rejected(std::move(malformed),
		"noncanonical tick booleans are rejected transactionally");
	malformed = payload;
	malformed[62] ^= 1; // Redundant frame-boundary completed count.
	rejected(std::move(malformed),
		"disagreeing completed-frame fields are rejected transactionally");
	malformed = payload;
	malformed[62 + 17 + 16] ^= 1; // Redundant completed tick count.
	rejected(std::move(malformed),
		"disagreeing completed-tick fields are rejected transactionally");
	malformed = payload;
	malformed.push_back(0);
	rejected(std::move(malformed),
		"otherwise-valid v2 payloads reject trailing bytes");
}

struct HostHarness
{
	ManualTimeSource time;
	MemoryByteStorage storage;
	EngineServices services{time, ZeroRandomSource::instance(), storage};
	EngineHostOptions options;
	EngineHost<> host;

	HostHarness() : options(makeOptions()), host(options, services) {}

	static EngineHostOptions makeOptions()
	{
		EngineHostOptions result;
		result.simulationStepMicroseconds = 10;
		result.limits.maximumSimulationCatchUpTicks = 4;
		return result;
	}

	FrameRunResult run()
	{
		return host.frameDriver().runFrame(
			[] { return FramePlan{false, FramePresentMode::Paced}; }, [] {});
	}
};

class CaptureDuringTick final : public SimulationTickSink
{
public:
	explicit CaptureDuringTick(EngineHost<>& host) : host_(host) {}

	void simulate(const SimulationTickContext&) override
	{
		captured = host_.captureRuntimeCheckpoint();
	}

	RuntimeCheckpointCaptureResult captured;

private:
	EngineHost<>& host_;
};

void TestHostCaptureValidationAndRestore()
{
	HostHarness source;
	RuntimeCheckpointCaptureResult duringFrame;
	RuntimeCheckpoint interactiveMetadata;
	source.host.frameDriver().runFrame([&] {
		duringFrame = source.host.captureRuntimeCheckpoint();
		interactiveMetadata = source.host.makeRuntimeCheckpoint();
		return FramePlan{false, FramePresentMode::Paced};
	}, [] {});
	Check(!duringFrame &&
		duringFrame.boundary.error ==
			RuntimeCheckpointBoundaryError::FrameStateRejected &&
			duringFrame.boundary.frameError ==
			FrameDriverBoundaryStateError::OperationInProgress,
		"host capture fails while a frame is in progress");
	std::vector<std::uint8_t> interactiveBytes;
	Check(interactiveMetadata.compatibility ==
			source.host.compatibilityFingerprint() &&
		interactiveMetadata.frameBoundary == FrameDriverBoundaryState{} &&
		source.host.runtimeCheckpoints().encode(interactiveMetadata,
			interactiveBytes) == RuntimeCheckpointSaveError::InvalidCheckpoint &&
		source.host.runtimeCheckpoints().encodeLegacyMetadata(
			interactiveMetadata, interactiveBytes) ==
				RuntimeCheckpointSaveError::None,
		"in-frame interactive metadata cannot claim v2 but retains explicit v1 encoding");
	RuntimeCheckpoint interactiveDecoded;
	const RuntimeCheckpointLoadResult interactiveResult =
		source.host.runtimeCheckpoints().decode(interactiveBytes,
			interactiveMetadata.compatibility, interactiveDecoded);
	Check(interactiveResult &&
		interactiveResult.storedVersion == RuntimeCheckpointService::LegacyVersion &&
		!interactiveResult.hasDeterministicBoundary,
		"interactive in-frame metadata remains an explicit CHKP v1 record");

	CaptureDuringTick tickCapture(source.host);
	Check(source.host.simulationTicks().addSink(tickCapture) ==
		SimulationTickSinkRegistrationError::None,
		"tick capture fixture registers");
	source.host.simulationTicks().advance(10);
	Check(!tickCapture.captured &&
		tickCapture.captured.boundary.error ==
			RuntimeCheckpointBoundaryError::SimulationTickStateRejected &&
		tickCapture.captured.boundary.simulationTickError ==
			SimulationTickBoundaryStateError::OperationInProgress,
		"host capture fails while a simulation tick is in progress");
	Check(source.host.simulationTicks().removeSink(tickCapture) ==
		SimulationTickSinkRegistrationError::None,
		"tick capture fixture unregisters");

	source.time.advanceMicroseconds(25);
	source.run();
	const RuntimeCheckpointCaptureResult saved =
		source.host.captureRuntimeCheckpoint();
	Check(saved && saved.checkpoint.completedFrames == 2 &&
		saved.checkpoint.completedSimulationTicks == 3 &&
		saved.checkpoint.frameBoundary ==
			FrameDriverBoundaryState{2, 3, false} &&
		saved.checkpoint.simulationTickBoundary ==
			SimulationTickBoundaryState{10, 4, 3, 30, 5, false},
		"host capture publishes one coherent frame and tick boundary");

	HostHarness destination;
	destination.run();
	const RuntimeCheckpointCaptureResult before =
		destination.host.captureRuntimeCheckpoint();
	RuntimeCheckpoint invalid = saved.checkpoint;
	invalid.frameBoundary.nextFrameSequence = 0;
	invalid.simulationTickBoundary.stepMicroseconds = 11;
	invalid.simulationTickBoundary.simulatedTimeMicroseconds = 33;
	const RuntimeCheckpointBoundaryResult rejected =
		destination.host.restoreRuntimeCheckpointBoundary(invalid);
	const RuntimeCheckpointCaptureResult afterRejected =
		destination.host.captureRuntimeCheckpoint();
	Check(!rejected &&
		rejected.error == RuntimeCheckpointBoundaryError::FrameStateRejected &&
		rejected.frameError ==
			FrameDriverBoundaryStateError::InvalidNextFrameSequence &&
		rejected.simulationTickError ==
			SimulationTickBoundaryStateError::IncompatibleConfiguration &&
		afterRejected &&
		afterRejected.checkpoint.frameBoundary ==
			before.checkpoint.frameBoundary &&
		afterRejected.checkpoint.simulationTickBoundary ==
			before.checkpoint.simulationTickBoundary,
		"host preflights both components before the first restore write");

	const RuntimeCheckpointBoundaryResult restored =
		destination.host.restoreRuntimeCheckpointBoundary(saved.checkpoint);
	const RuntimeCheckpointCaptureResult afterRestore =
		destination.host.captureRuntimeCheckpoint();
	Check(restored && afterRestore &&
		afterRestore.checkpoint.frameBoundary ==
			saved.checkpoint.frameBoundary &&
		afterRestore.checkpoint.simulationTickBoundary ==
			saved.checkpoint.simulationTickBoundary,
		"host restores tick then frame as one preflighted boundary");
	const FrameRunResult resumed = destination.run();
	Check(resumed.sequence == 3 && resumed.simulationTicks.executed == 0 &&
		destination.host.simulationTicks().completedTickSequence() == 3,
		"cold resume continues the next frame identity with a fresh time anchor");
	destination.time.advanceMicroseconds(5);
	const FrameRunResult continued = destination.run();
	Check(continued.sequence == 4 && continued.simulationTicks.executed == 1 &&
		destination.host.simulationTicks().completedTickSequence() == 4,
		"restored tick remainder continues deterministically after the new anchor");

	Check(source.host.saveRuntimeCheckpoint("runtime-v2") ==
		RuntimeCheckpointSaveError::None,
		"host save captures and persists the deterministic boundary");
	RuntimeCheckpoint loaded;
	const RuntimeCheckpointLoadResult loadedResult =
		source.host.loadRuntimeCheckpoint("runtime-v2", loaded);
	Check(loadedResult && loadedResult.storedVersion == 2 &&
		loadedResult.hasDeterministicBoundary,
		"host load exposes v2 deterministic-boundary metadata");
}
}

int main()
{
	TestV2RoundTripAndRedundantCounters();
	TestV1FixtureRemainsMetadataOnly();
	TestV2CanonicalWireRejectionIsTransactional();
	TestHostCaptureValidationAndRestore();
	return 0;
}
