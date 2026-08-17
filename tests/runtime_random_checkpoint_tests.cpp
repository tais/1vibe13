#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/ByteStorage.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeRandomCheckpoint.h>
#include <Engine/Core/SimulationRandom.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
constexpr std::size_t EnvelopeHeaderBytes = 18;
constexpr std::size_t CompatibilityHighOffset = 4;
constexpr std::size_t SimulationSchemaOffset = 20;
constexpr std::size_t SimulationAlgorithmOffset = 24;
constexpr std::size_t SimulationCampaignSeedOffset = 28;
constexpr std::size_t SimulationIncrementOffset = 44;
constexpr std::size_t PackageHostSeedOffset = 60;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::fprintf(stderr, "FAIL: %s\n", message);
	std::exit(1);
}

void WriteU32(std::vector<std::uint8_t>& bytes, std::size_t offset,
	std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

void WriteU64(std::vector<std::uint8_t>& bytes, std::size_t offset,
	std::uint64_t value)
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

RuntimeRandomCheckpoint MakeCheckpoint()
{
	SimulationRandom random(0x1020304050607080ULL);
	Check(random.tryNext(37) && random.nextRaw() && random.tryNext(1000000),
		"fixture advances the authoritative simulation stream");
	return RuntimeRandomCheckpoint{
		RuntimeCompatibilityFingerprint{
			RuntimeCompatibilityFingerprint::CurrentSchema,
			0x1122334455667788ULL, 0x99aabbccddeeff00ULL},
		random.checkpoint(), 0x0f1e2d3c4b5a6978ULL};
}

RuntimeRandomCheckpoint SentinelCheckpoint()
{
	SimulationRandom random(77);
	Check(static_cast<bool>(random.nextRaw()),
		"sentinel advances independently");
	return RuntimeRandomCheckpoint{
		RuntimeCompatibilityFingerprint{
			RuntimeCompatibilityFingerprint::CurrentSchema, 8, 9},
		random.checkpoint(), 10};
}

std::vector<std::uint8_t> ExtractPayload(PersistenceService& persistence,
	const std::vector<std::uint8_t>& encoded)
{
	PersistenceHeader header{};
	std::vector<std::uint8_t> payload;
	Check(persistence.decodeEnvelope(encoded, RuntimeRandomCheckpointMagic,
		RuntimeRandomCheckpointVersion, RuntimeRandomCheckpointVersion,
		header, payload) == PersistenceLoadResult::Success,
		"test helper extracts a valid GRNG envelope");
	Check(header.magic == RuntimeRandomCheckpointMagic &&
		header.version == RuntimeRandomCheckpointVersion,
		"test helper observes canonical GRNG identity");
	return payload;
}

std::vector<std::uint8_t> MakeEnvelope(PersistenceService& persistence,
	const std::vector<std::uint8_t>& payload,
	std::uint32_t magic = RuntimeRandomCheckpointMagic,
	std::uint16_t version = RuntimeRandomCheckpointVersion)
{
	std::vector<std::uint8_t> encoded;
	Check(persistence.encodeEnvelope(PersistenceHeader{magic, version}, payload,
		encoded) == PersistenceSaveResult::Success,
		"test helper builds a checksummed envelope");
	return encoded;
}

void CheckFailedDecode(const RuntimeRandomCheckpointService& service,
	const std::vector<std::uint8_t>& encoded,
	const RuntimeRandomCheckpoint& expected,
	RuntimeRandomCheckpointLoadError error, const char* message)
{
	RuntimeRandomCheckpoint output = SentinelCheckpoint();
	const RuntimeRandomCheckpoint before = output;
	const RuntimeRandomCheckpointLoadResult result = service.decode(encoded,
		expected.compatibility, expected.simulationRandom.campaignSeed,
		expected.packageRandomHostSeed, output);
	Check(result.error == error && output == before, message);
}

void TestCanonicalWireRoundTripAndStorage()
{
	MemoryByteStorage storage;
	PersistenceService persistence(storage);
	RuntimeRandomCheckpointService service(persistence);
	const RuntimeRandomCheckpoint checkpoint = MakeCheckpoint();
	std::vector<std::uint8_t> encoded = {0xaa, 0xbb};
	Check(service.encode(checkpoint, encoded) ==
			RuntimeRandomCheckpointSaveError::None,
		"a complete deterministic root checkpoint encodes");
	Check(RuntimeRandomCheckpointSectionType == 0x474e5247u &&
		RuntimeRandomCheckpointMagic == 0x474e5247u &&
		RuntimeRandomCheckpointVersion == 1 &&
		RuntimeRandomCheckpointPayloadBytes == 68 &&
		encoded.size() == EnvelopeHeaderBytes + RuntimeRandomCheckpointPayloadBytes,
		"GRNG v1 exposes one fixed section, envelope, and payload contract");

	const std::vector<std::uint8_t> payload = ExtractPayload(persistence, encoded);
	Check(payload.size() == RuntimeRandomCheckpointPayloadBytes,
		"the verified envelope contains exactly 68 payload bytes");
	BinaryReader reader(payload);
	RuntimeCompatibilityFingerprint compatibility{0, 0, 0};
	Check(reader.readU32(compatibility.schema) &&
		reader.readU64(compatibility.high) && reader.readU64(compatibility.low) &&
		compatibility == checkpoint.compatibility,
		"the payload starts with the canonical runtime fingerprint fields");
	SimulationRandomCheckpointBytes simulationBytes{};
	Check(EncodeSimulationRandomCheckpoint(
		checkpoint.simulationRandom, simulationBytes) &&
		std::equal(simulationBytes.begin(), simulationBytes.end(),
			payload.begin() + static_cast<std::ptrdiff_t>(reader.position())),
		"the payload embeds the exact canonical 40-byte simulation checkpoint");
	BinaryReader hostSeedReader(payload.data() + PackageHostSeedOffset,
		sizeof(std::uint64_t));
	std::uint64_t hostSeed = 0;
	Check(hostSeedReader.readU64(hostSeed) && hostSeedReader.remaining() == 0 &&
		hostSeed == checkpoint.packageRandomHostSeed,
		"the final payload field is the package RNG host root seed");

	RuntimeRandomCheckpoint decoded = SentinelCheckpoint();
	const RuntimeRandomCheckpointLoadResult decodedResult = service.decode(encoded,
		checkpoint.compatibility, checkpoint.simulationRandom.campaignSeed,
		checkpoint.packageRandomHostSeed, decoded);
	Check(decodedResult && decoded == checkpoint &&
		decodedResult.storedVersion == RuntimeRandomCheckpointVersion &&
		decodedResult.storedCompatibility == checkpoint.compatibility,
		"decode publishes all roots only after returning verified version and identity");

	const std::string path = "runtime/random.grng";
	Check(service.save(path, checkpoint) ==
			RuntimeRandomCheckpointSaveError::None,
		"the standalone codec persists its canonical envelope");
	RuntimeRandomCheckpoint loaded = SentinelCheckpoint();
	const RuntimeRandomCheckpointLoadResult loadedResult = service.load(path,
		checkpoint.compatibility, checkpoint.simulationRandom.campaignSeed,
		checkpoint.packageRandomHostSeed, loaded);
	Check(loadedResult && loaded == checkpoint &&
		loadedResult.storedVersion == RuntimeRandomCheckpointVersion &&
		loadedResult.storedCompatibility == checkpoint.compatibility,
		"storage load verifies and publishes the same canonical roots");
}

void TestEncodeValidationIsTransactional()
{
	MemoryByteStorage storage;
	PersistenceService persistence(storage);
	RuntimeRandomCheckpointService service(persistence);
	const RuntimeRandomCheckpoint valid = MakeCheckpoint();
	const std::vector<std::uint8_t> sentinel = {9, 8, 7};

	RuntimeRandomCheckpoint invalid = valid;
	invalid.compatibility.schema = 2;
	std::vector<std::uint8_t> output = sentinel;
	Check(service.encode(invalid, output) ==
			RuntimeRandomCheckpointSaveError::InvalidCompatibility &&
		output == sentinel,
		"encode rejects a noncanonical compatibility schema transactionally");

	invalid = valid;
	invalid.simulationRandom.schema = 2;
	output = sentinel;
	Check(service.encode(invalid, output) ==
			RuntimeRandomCheckpointSaveError::UnsupportedSimulationSchema &&
		output == sentinel,
		"encode rejects an unsupported simulation schema transactionally");

	invalid = valid;
	invalid.simulationRandom.algorithm = 2;
	output = sentinel;
	Check(service.encode(invalid, output) ==
			RuntimeRandomCheckpointSaveError::UnsupportedSimulationAlgorithm &&
		output == sentinel,
		"encode rejects an unsupported RNG algorithm transactionally");

	invalid = valid;
	invalid.simulationRandom.increment ^= 2;
	output = sentinel;
	Check(service.encode(invalid, output) ==
			RuntimeRandomCheckpointSaveError::InvalidSimulationStream &&
		output == sentinel,
		"encode rejects a noncanonical PCG stream increment transactionally");

	PersistenceService tooTightPersistence(
		storage, RuntimeRandomCheckpointPayloadBytes - 1);
	RuntimeRandomCheckpointService tooTight(tooTightPersistence);
	output = sentinel;
	Check(tooTight.encode(valid, output) ==
			RuntimeRandomCheckpointSaveError::TooLarge && output == sentinel,
		"the persistence bound rejects the fixed payload without publishing bytes");
}

void TestEnvelopeFailuresAreTransactional()
{
	MemoryByteStorage storage;
	PersistenceService persistence(storage);
	RuntimeRandomCheckpointService service(persistence);
	const RuntimeRandomCheckpoint expected = MakeCheckpoint();
	std::vector<std::uint8_t> encoded;
	Check(service.encode(expected, encoded) ==
			RuntimeRandomCheckpointSaveError::None,
		"envelope rejection fixture encodes");
	const std::vector<std::uint8_t> payload = ExtractPayload(persistence, encoded);

	CheckFailedDecode(service,
		MakeEnvelope(persistence, payload, 0x474e5246u), expected,
		RuntimeRandomCheckpointLoadError::InvalidOrUnsupported,
		"a checksummed envelope with the wrong magic is rejected transactionally");
	CheckFailedDecode(service,
		MakeEnvelope(persistence, payload, RuntimeRandomCheckpointMagic, 2), expected,
		RuntimeRandomCheckpointLoadError::InvalidOrUnsupported,
		"a checksummed envelope with the wrong version is rejected transactionally");

	std::vector<std::uint8_t> shortEnvelope = encoded;
	shortEnvelope.pop_back();
	CheckFailedDecode(service, shortEnvelope, expected,
		RuntimeRandomCheckpointLoadError::InvalidOrUnsupported,
		"a truncated envelope is rejected transactionally");
	std::vector<std::uint8_t> trailingEnvelope = encoded;
	trailingEnvelope.push_back(0);
	CheckFailedDecode(service, trailingEnvelope, expected,
		RuntimeRandomCheckpointLoadError::InvalidOrUnsupported,
		"trailing envelope bytes are rejected transactionally");
	std::vector<std::uint8_t> corruptEnvelope = encoded;
	corruptEnvelope.back() ^= 0x80u;
	CheckFailedDecode(service, corruptEnvelope, expected,
		RuntimeRandomCheckpointLoadError::IntegrityFailure,
		"payload corruption is rejected by checksum before semantic publication");

	std::vector<std::uint8_t> shortPayload = payload;
	shortPayload.pop_back();
	CheckFailedDecode(service, MakeEnvelope(persistence, shortPayload), expected,
		RuntimeRandomCheckpointLoadError::MalformedPayload,
		"a checksum-valid short payload is rejected transactionally");
	std::vector<std::uint8_t> longPayload = payload;
	longPayload.push_back(0);
	CheckFailedDecode(service, MakeEnvelope(persistence, longPayload), expected,
		RuntimeRandomCheckpointLoadError::MalformedPayload,
		"a checksum-valid long payload is rejected transactionally");

	RuntimeRandomCheckpoint missingOutput = SentinelCheckpoint();
	const RuntimeRandomCheckpoint missingBefore = missingOutput;
	Check(service.load("missing.grng", expected.compatibility,
		expected.simulationRandom.campaignSeed,
		expected.packageRandomHostSeed, missingOutput).error ==
			RuntimeRandomCheckpointLoadError::NotFound &&
		missingOutput == missingBefore,
		"a missing stored envelope leaves the caller's roots unchanged");
	storage.writeAll("corrupt.grng", corruptEnvelope);
	RuntimeRandomCheckpoint corruptOutput = SentinelCheckpoint();
	const RuntimeRandomCheckpoint corruptBefore = corruptOutput;
	Check(service.load("corrupt.grng", expected.compatibility,
		expected.simulationRandom.campaignSeed,
		expected.packageRandomHostSeed, corruptOutput).error ==
			RuntimeRandomCheckpointLoadError::IntegrityFailure &&
		corruptOutput == corruptBefore,
		"storage load also rejects corruption transactionally");
}

void TestSemanticRootFailuresAreTransactional()
{
	MemoryByteStorage storage;
	PersistenceService persistence(storage);
	RuntimeRandomCheckpointService service(persistence);
	const RuntimeRandomCheckpoint expected = MakeCheckpoint();
	std::vector<std::uint8_t> encoded;
	Check(service.encode(expected, encoded) ==
			RuntimeRandomCheckpointSaveError::None,
		"semantic rejection fixture encodes");
	const std::vector<std::uint8_t> originalPayload =
		ExtractPayload(persistence, encoded);

	auto CheckPayloadFailure = [&](std::vector<std::uint8_t> payload,
		RuntimeRandomCheckpointLoadError error, const char* message) {
		RuntimeRandomCheckpoint output = SentinelCheckpoint();
		const RuntimeRandomCheckpoint before = output;
		const RuntimeRandomCheckpointLoadResult result = service.decode(
			MakeEnvelope(persistence, payload), expected.compatibility,
			expected.simulationRandom.campaignSeed,
			expected.packageRandomHostSeed, output);
		Check(result.error == error &&
			result.storedVersion == RuntimeRandomCheckpointVersion &&
			result.storedCompatibility ==
				(error == RuntimeRandomCheckpointLoadError::IncompatibleRuntime
					? RuntimeCompatibilityFingerprint{
						expected.compatibility.schema,
						expected.compatibility.high ^ 1u,
						expected.compatibility.low}
					: expected.compatibility) &&
			output == before, message);
	};

	std::vector<std::uint8_t> payload = originalPayload;
	WriteU64(payload, CompatibilityHighOffset,
		expected.compatibility.high ^ 1u);
	CheckPayloadFailure(payload,
		RuntimeRandomCheckpointLoadError::IncompatibleRuntime,
		"a different verified runtime fingerprint is exposed but not published");

	payload = originalPayload;
	WriteU32(payload, SimulationSchemaOffset, 2);
	CheckPayloadFailure(payload,
		RuntimeRandomCheckpointLoadError::UnsupportedSimulationSchema,
		"an unsupported embedded simulation schema is rejected transactionally");

	payload = originalPayload;
	WriteU32(payload, SimulationAlgorithmOffset, 2);
	CheckPayloadFailure(payload,
		RuntimeRandomCheckpointLoadError::UnsupportedSimulationAlgorithm,
		"an unsupported embedded simulation algorithm is rejected transactionally");

	payload = originalPayload;
	WriteU64(payload, SimulationIncrementOffset,
		SimulationRandomPcg32Increment ^ 2u);
	CheckPayloadFailure(payload,
		RuntimeRandomCheckpointLoadError::InvalidSimulationStream,
		"a malformed embedded PCG increment is rejected transactionally");

	payload = originalPayload;
	WriteU64(payload, SimulationCampaignSeedOffset,
		expected.simulationRandom.campaignSeed ^ 1u);
	CheckPayloadFailure(payload,
		RuntimeRandomCheckpointLoadError::CampaignSeedMismatch,
		"a different campaign seed cannot replace the authoritative stream");

	payload = originalPayload;
	WriteU64(payload, PackageHostSeedOffset,
		expected.packageRandomHostSeed ^ 1u);
	CheckPayloadFailure(payload,
		RuntimeRandomCheckpointLoadError::PackageRandomHostSeedMismatch,
		"a different package host root cannot be adopted during strict resume");
}
}

int main()
{
	TestCanonicalWireRoundTripAndStorage();
	TestEncodeValidationIsTransactional();
	TestEnvelopeFailuresAreTransactional();
	TestSemanticRootFailuresAreTransactional();
	std::puts("runtime random checkpoint tests passed");
	return 0;
}
