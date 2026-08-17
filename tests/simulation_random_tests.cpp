#include <Engine/Core/SimulationRandom.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
void Check(bool condition, const char* message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

void WriteU32(SimulationRandomCheckpointBytes& bytes,
	std::size_t offset, std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

void WriteU64(SimulationRandomCheckpointBytes& bytes,
	std::size_t offset, std::uint64_t value)
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

void CheckUnchanged(const SimulationRandom& random,
	const SimulationRandomCheckpoint& before, const char* message)
{
	Check(random.checkpoint() == before, message);
}

void TestPcg32GoldenVector()
{
	SimulationRandom random(42);
	const SimulationRandomCheckpoint initial = random.checkpoint();
	Check(initial.schema == 1 && initial.algorithm == 1 &&
		initial.campaignSeed == 42 &&
		initial.state == 0x185706b82c2e03f8ULL &&
		initial.increment == 109 && initial.rawValuesGenerated == 0,
		"PCG32 reference seeding state is pinned");

	const std::array<std::uint32_t, 6> golden{
		0xa15c02b7u, 0x7b47f409u, 0xba1d3330u,
		0x83d2f293u, 0xbfa4784bu, 0xcbed606eu};
	for (const std::uint32_t expected : golden)
	{
		const SimulationRandomResult result = random.nextRaw();
		Check(result && result.value == expected,
			"PCG32 raw value matches the reference golden vector");
	}
	Check(random.rawValuesGenerated() == golden.size(),
		"raw golden draws are counted exactly");
}

void TestSeedAndBoundSemantics()
{
	SimulationRandom zeroSeed(0);
	const SimulationRandomCheckpoint seeded = zeroSeed.checkpoint();
	Check(seeded.increment != 0 && (seeded.increment & 1u) == 1u,
		"zero campaign seed still selects a valid nonzero PCG stream");
	bool sawNonZero = false;
	for (unsigned index = 0; index < 8; ++index)
	{
		const SimulationRandomResult result = zeroSeed.nextRaw();
		Check(static_cast<bool>(result), "zero-seeded stream produces raw values");
		sawNonZero = sawNonZero || result.value != 0;
	}
	Check(sawNonZero, "zero campaign seed is not an all-zero stream");

	SimulationRandom random(7);
	const SimulationRandomCheckpoint beforeZero = random.checkpoint();
	const SimulationRandomResult zero = random.tryNext(0);
	Check(zero && zero.value == 0, "bound zero deterministically returns zero");
	CheckUnchanged(random, beforeZero, "bound zero does not advance the stream");

	const SimulationRandomResult one = random.tryNext(1);
	Check(one && one.value == 0, "bound one deterministically returns zero");
	CheckUnchanged(random, beforeZero, "bound one does not advance the stream");

	RandomSource& legacy = random;
	Check(legacy.next(0) == 0 && legacy.next(1) == 0,
		"RandomSource override preserves NewRandom small-bound behavior");
	CheckUnchanged(random, beforeZero,
		"legacy bounds zero and one do not advance the stream");
	SimulationRandom explicitSource(0x9988776655443322ULL);
	SimulationRandom legacySource(0x9988776655443322ULL);
	RandomSource& polymorphic = legacySource;
	for (std::uint32_t bound : {2u, 5u, 0x80000001u, 17u, 0xffffffffu})
	{
		const SimulationRandomResult explicitResult = explicitSource.tryNext(bound);
		Check(explicitResult && polymorphic.next(bound) == explicitResult.value,
			"RandomSource override matches the authoritative result surface");
		Check(legacySource.checkpoint() == explicitSource.checkpoint(),
			"legacy and authoritative draws advance identical state");
	}
	for (std::uint32_t bound : {2u, 3u, 7u, 255u, 65537u,
		std::numeric_limits<std::uint32_t>::max()})
	{
		for (unsigned draw = 0; draw < 128; ++draw)
		{
			const SimulationRandomResult result = random.tryNext(bound);
			Check(result && result.value < bound,
				"bounded results are within the half-open interval");
		}
	}

	SimulationRandom rejection(9);
	SimulationRandomCheckpoint forced = rejection.checkpoint();
	forced.state = 0;
	forced.rawValuesGenerated = 1;
	Check(rejection.restoreCheckpoint(forced) ==
		SimulationRandomCheckpointError::None,
		"valid PCG state can be restored for rejection fixture");
	const SimulationRandomResult rejected = rejection.tryNext(0x80000001u);
	Check(rejected && rejected.value == 0x398f6a26u,
		"large-bound rejection result matches its golden value");
	Check(rejection.rawValuesGenerated() == 5,
		"every discarded golden candidate is included in the raw counter");
}

void TestBoundedGoldenVector()
{
	SimulationRandom random(42);
	Check(random.tryNext(0) && random.tryNext(1),
		"small bounds succeed before the bounded golden vector");
	const std::array<std::uint32_t, 8> bounds{
		2u, 3u, 7u, 0x80000001u, 1000u, 0xffffffffu,
		17u, 0xf0000001u};
	const std::array<std::uint32_t, 8> golden{
		0x00000001u, 0x00000000u, 0x00000006u, 0x03d2f292u,
		0x000003bbu, 0xcbed606eu, 0x0000000bu, 0x812fff6du};
	for (std::size_t index = 0; index < bounds.size(); ++index)
	{
		const SimulationRandomResult result = random.tryNext(bounds[index]);
		Check(result && result.value == golden[index],
			"bounded mapping matches the pinned golden vector");
	}
	const SimulationRandomCheckpoint final = random.checkpoint();
	Check(final.rawValuesGenerated == 8 &&
		final.state == 0x0730f84eec16daf0ULL,
		"bounded golden vector advances the exact pinned PCG state");
}

void TestCheckpointReplayAndValidation()
{
	SimulationRandom random(0x0102030405060708ULL);
	for (unsigned index = 0; index < 31; ++index)
		Check(static_cast<bool>(random.tryNext(1009)), "warmup draw succeeds");
	const SimulationRandomCheckpoint saved = random.checkpoint();

	std::array<std::uint32_t, 64> replay{};
	for (std::uint32_t& value : replay)
	{
		const SimulationRandomResult result = random.tryNext(0xf0000001u);
		Check(static_cast<bool>(result), "post-checkpoint draw succeeds");
		value = result.value;
	}
	Check(random.restoreCheckpoint(saved) ==
		SimulationRandomCheckpointError::None,
		"matching checkpoint restores");
	for (const std::uint32_t expected : replay)
	{
		const SimulationRandomResult result = random.tryNext(0xf0000001u);
		Check(result && result.value == expected,
			"restored checkpoint replays every bounded draw");
	}

	SimulationRandom peer(saved.campaignSeed);
	Check(peer.restoreCheckpoint(saved) ==
		SimulationRandomCheckpointError::None,
		"checkpoint restores into a peer for the same campaign");
	Check(random.restoreCheckpoint(saved) ==
		SimulationRandomCheckpointError::None,
		"source rewinds for next-draw equality");
	for (unsigned index = 0; index < 32; ++index)
	{
		const SimulationRandomResult left = random.nextRaw();
		const SimulationRandomResult right = peer.nextRaw();
		Check(left && right && left.value == right.value,
			"restored peers have exact next-draw equality");
	}

	auto RejectRestore = [&](SimulationRandomCheckpoint invalid,
		SimulationRandomCheckpointError expected, const char* message) {
		const SimulationRandomCheckpoint before = random.checkpoint();
		const SimulationRandomError healthBefore = random.health();
		Check(random.validateCheckpoint(invalid) == expected,
			"nonmutating validation reports the expected error");
		CheckUnchanged(random, before, "validation never mutates the stream");
		Check(random.health() == healthBefore,
			"validation does not mutate stream health");
		Check(random.restoreCheckpoint(invalid) == expected, message);
		CheckUnchanged(random, before, "failed restore is transactional");
		Check(random.health() == healthBefore,
			"failed restore preserves stream health");
	};
	SimulationRandomCheckpoint invalid = saved;
	invalid.schema = 2;
	RejectRestore(invalid, SimulationRandomCheckpointError::InvalidSchema,
		"unknown schema restore is rejected");
	invalid = saved;
	invalid.algorithm = 2;
	RejectRestore(invalid, SimulationRandomCheckpointError::UnsupportedAlgorithm,
		"unknown algorithm restore is rejected");
	invalid = saved;
	++invalid.campaignSeed;
	RejectRestore(invalid, SimulationRandomCheckpointError::CampaignSeedMismatch,
		"cross-campaign restore is rejected");
	invalid = saved;
	invalid.increment = 0;
	RejectRestore(invalid, SimulationRandomCheckpointError::InvalidStream,
		"zero stream increment is rejected");
	invalid.increment = 108;
	RejectRestore(invalid, SimulationRandomCheckpointError::InvalidStream,
		"even stream increment is rejected");
	invalid.increment = 111;
	RejectRestore(invalid, SimulationRandomCheckpointError::InvalidStream,
		"noncanonical PCG stream increment is rejected");
}

void TestWireCodec()
{
	SimulationRandom random(42);
	const SimulationRandomCheckpoint original = random.checkpoint();
	SimulationRandomCheckpointBytes encoded{};
	Check(EncodeSimulationRandomCheckpoint(original, encoded),
		"valid checkpoint encodes");
	const SimulationRandomCheckpointBytes golden{
		0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
		0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xf8, 0x03, 0x2e, 0x2c, 0xb8, 0x06, 0x57, 0x18,
		0x6d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	Check(encoded == golden,
		"checkpoint wire bytes are the pinned little-endian fixture");

	SimulationRandomCheckpoint decoded;
	decoded.campaignSeed = 99;
	Check(DecodeSimulationRandomCheckpoint(encoded.data(), encoded.size(),
		decoded) == SimulationRandomCheckpointDecodeError::None,
		"golden checkpoint decodes");
	Check(decoded == original, "checkpoint codec round trip is exact");

	const SimulationRandomCheckpoint retained = decoded;
	for (std::size_t size = 0; size < encoded.size(); ++size)
	{
		Check(DecodeSimulationRandomCheckpoint(encoded.data(), size, decoded) ==
			SimulationRandomCheckpointDecodeError::WrongSize,
			"every truncated checkpoint is rejected");
		Check(decoded == retained, "truncated decode preserves output");
	}
	std::vector<std::uint8_t> trailing(encoded.begin(), encoded.end());
	for (unsigned count = 1; count <= 32; ++count)
	{
		trailing.push_back(static_cast<std::uint8_t>(count));
		Check(DecodeSimulationRandomCheckpoint(trailing.data(), trailing.size(),
			decoded) == SimulationRandomCheckpointDecodeError::WrongSize,
			"every trailing-byte checkpoint is rejected");
		Check(decoded == retained, "trailing-byte decode preserves output");
	}
	Check(DecodeSimulationRandomCheckpoint(nullptr, encoded.size(), decoded) ==
		SimulationRandomCheckpointDecodeError::WrongSize,
		"null exact-size input is rejected");
	Check(decoded == retained, "null decode preserves output");

	auto RejectBytes = [&](SimulationRandomCheckpointBytes changed,
		SimulationRandomCheckpointDecodeError expected, const char* message) {
		Check(DecodeSimulationRandomCheckpoint(changed.data(), changed.size(),
			decoded) == expected, message);
		Check(decoded == retained, "invalid wire decode is transactional");
	};
	SimulationRandomCheckpointBytes changed = encoded;
	WriteU32(changed, 0, 2);
	RejectBytes(changed,
		SimulationRandomCheckpointDecodeError::UnsupportedSchema,
		"unknown wire schema is rejected");
	changed = encoded;
	WriteU32(changed, 4, 2);
	RejectBytes(changed,
		SimulationRandomCheckpointDecodeError::UnsupportedAlgorithm,
		"unknown wire algorithm is rejected");
	changed = encoded;
	WriteU64(changed, 24, 0);
	RejectBytes(changed, SimulationRandomCheckpointDecodeError::InvalidStream,
		"zero wire stream is rejected");
	changed = encoded;
	WriteU64(changed, 24, 108);
	RejectBytes(changed, SimulationRandomCheckpointDecodeError::InvalidStream,
		"even wire stream is rejected");
	changed = encoded;
	WriteU64(changed, 24, 111);
	RejectBytes(changed, SimulationRandomCheckpointDecodeError::InvalidStream,
		"noncanonical wire stream is rejected");

	SimulationRandomCheckpoint invalid = original;
	invalid.schema = 2;
	SimulationRandomCheckpointBytes preserved{};
	preserved.fill(0xa5);
	const SimulationRandomCheckpointBytes sentinel = preserved;
	Check(!EncodeSimulationRandomCheckpoint(invalid, preserved),
		"invalid schema is not encoded");
	Check(preserved == sentinel, "failed encode preserves previous bytes");
	invalid = original;
	invalid.algorithm = 2;
	Check(!EncodeSimulationRandomCheckpoint(invalid, preserved),
		"invalid algorithm is not encoded");
	Check(preserved == sentinel, "algorithm encode failure is transactional");
	invalid = original;
	invalid.increment = 3;
	Check(!EncodeSimulationRandomCheckpoint(invalid, preserved),
		"noncanonical stream is not encoded");
	Check(preserved == sentinel, "stream encode failure is transactional");
}

void TestCounterExhaustion()
{
	const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
	SimulationRandom random(77);
	SimulationRandomCheckpoint exhausted = random.checkpoint();
	exhausted.rawValuesGenerated = maximum;
	Check(random.restoreCheckpoint(exhausted) ==
		SimulationRandomCheckpointError::None,
		"exhausted checkpoint is a valid terminal state");
	const SimulationRandomCheckpoint before = random.checkpoint();
	Check(random.nextRaw().error == SimulationRandomError::SequenceExhausted,
		"raw draw fails closed at counter maximum");
	CheckUnchanged(random, before, "failed raw overflow does not advance state");
	Check(!random.healthy() &&
		random.health() == SimulationRandomError::SequenceExhausted,
		"counter exhaustion is visible on the persistent health surface");
	const SimulationRandomResult terminalOne = random.tryNext(1);
	Check(terminalOne && terminalOne.value == 0,
		"bound one remains a non-consuming constant at counter maximum");
	CheckUnchanged(random, before,
		"bound-one terminal query does not advance state");
	Check(random.next(2) == 0 && !random.healthy(),
		"legacy RandomSource fails closed and retains explicit failure health");

	SimulationRandomCheckpoint last = before;
	last.rawValuesGenerated = maximum - 1;
	Check(random.restoreCheckpoint(last) ==
		SimulationRandomCheckpointError::None,
		"penultimate counter state restores");
	Check(random.healthy(), "successful restore clears prior failure health");
	Check(static_cast<bool>(random.nextRaw()),
		"final representable raw draw succeeds");
	Check(random.rawValuesGenerated() == maximum,
		"final raw draw reaches the terminal counter exactly");
	const SimulationRandomCheckpoint terminal = random.checkpoint();
	Check(random.nextRaw().error == SimulationRandomError::SequenceExhausted,
		"draw after final value is rejected");
	CheckUnchanged(random, terminal, "terminal stream remains stable");

	SimulationRandom rejection(77);
	SimulationRandomCheckpoint almost = rejection.checkpoint();
	almost.state = 0;
	almost.rawValuesGenerated = maximum - 1;
	Check(rejection.restoreCheckpoint(almost) ==
		SimulationRandomCheckpointError::None,
		"near-overflow rejection fixture restores");
	const SimulationRandomCheckpoint rejectionBefore = rejection.checkpoint();
	Check(rejection.tryNext(0x80000001u).error ==
		SimulationRandomError::SequenceExhausted,
		"rejection needing an unrepresentable second raw value fails closed");
	CheckUnchanged(rejection, rejectionBefore,
		"partially sampled overflow is fully transactional");

	SimulationRandomCheckpointBytes encoded{};
	Check(EncodeSimulationRandomCheckpoint(terminal, encoded),
		"terminal counter checkpoint remains serializable");
	SimulationRandomCheckpoint decoded;
	Check(DecodeSimulationRandomCheckpoint(encoded.data(), encoded.size(),
		decoded) == SimulationRandomCheckpointDecodeError::None &&
		decoded == terminal,
		"terminal counter checkpoint round trips exactly");
}
}

int main()
{
	static_assert(SimulationRandomCheckpointWireSize == 40,
		"simulation RNG checkpoint size is a wire contract");
	static_assert(SimulationRandomPcg32Increment == 109,
		"PCG32 stream selector is a deterministic contract");
	static_assert(std::is_nothrow_constructible<SimulationRandom,
		std::uint64_t>::value, "campaign seeding cannot fail");
	static_assert(std::is_base_of<RandomSource, SimulationRandom>::value,
		"simulation RNG is directly usable through the engine random contract");
	static_assert(noexcept(std::declval<const SimulationRandom&>().checkpoint()),
		"checkpoint capture cannot fail");
	static_assert(noexcept(std::declval<SimulationRandom&>().restoreCheckpoint(
		std::declval<const SimulationRandomCheckpoint&>())),
		"checkpoint restore cannot throw");

	TestPcg32GoldenVector();
	TestSeedAndBoundSemantics();
	TestBoundedGoldenVector();
	TestCheckpointReplayAndValidation();
	TestWireCodec();
	TestCounterExhaustion();
	std::puts("simulation random tests passed");
	return 0;
}
