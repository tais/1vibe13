#ifndef ENGINE_CORE_SIMULATION_RANDOM_H
#define ENGINE_CORE_SIMULATION_RANDOM_H

#include <array>
#include <cstddef>
#include <cstdint>

#include <Engine/Core/RandomSource.h>

// The simulation stream is a PCG XSH-RR 64/32 generator with the reference
// multiplier and a fixed stream selector of 54 (increment 109). These values,
// the seeding procedure, and the bounded rejection mapping are wire contracts:
// changing any of them requires a new algorithm identifier.
constexpr std::uint32_t SimulationRandomPcg32Algorithm = 1;
constexpr std::uint64_t SimulationRandomPcg32StreamSelector = 54;
constexpr std::uint64_t SimulationRandomPcg32Increment =
	(SimulationRandomPcg32StreamSelector << 1u) | 1u;

struct SimulationRandomCheckpoint
{
	static constexpr std::uint32_t CurrentSchema = 1;
	static constexpr std::uint32_t CurrentAlgorithm =
		SimulationRandomPcg32Algorithm;

	std::uint32_t schema = CurrentSchema;
	std::uint32_t algorithm = CurrentAlgorithm;
	std::uint64_t campaignSeed = 0;
	std::uint64_t state = 0;
	std::uint64_t increment = SimulationRandomPcg32Increment;
	// Counts raw PCG32 values consumed, including values discarded by bounded
	// rejection sampling. The two private seeding transitions are not counted.
	std::uint64_t rawValuesGenerated = 0;

	bool operator==(const SimulationRandomCheckpoint& other) const noexcept
	{
		return schema == other.schema && algorithm == other.algorithm &&
			campaignSeed == other.campaignSeed && state == other.state &&
			increment == other.increment &&
			rawValuesGenerated == other.rawValuesGenerated;
	}

	bool operator!=(const SimulationRandomCheckpoint& other) const noexcept
	{
		return !(*this == other);
	}
};

constexpr std::size_t SimulationRandomCheckpointWireSize = 40;
using SimulationRandomCheckpointBytes =
	std::array<std::uint8_t, SimulationRandomCheckpointWireSize>;

enum class SimulationRandomError : std::uint8_t
{
	None,
	SequenceExhausted
};

struct SimulationRandomResult
{
	SimulationRandomError error = SimulationRandomError::None;
	std::uint32_t value = 0;

	explicit operator bool() const noexcept
	{
		return error == SimulationRandomError::None;
	}
};

enum class SimulationRandomCheckpointError : std::uint8_t
{
	None,
	InvalidSchema,
	UnsupportedAlgorithm,
	CampaignSeedMismatch,
	InvalidStream
};

enum class SimulationRandomCheckpointDecodeError : std::uint8_t
{
	None,
	WrongSize,
	UnsupportedSchema,
	UnsupportedAlgorithm,
	InvalidStream
};

// Encodes exactly 40 bytes in this order, all little-endian:
// schema:u32, algorithm:u32, campaignSeed:u64, state:u64, increment:u64,
// rawValuesGenerated:u64. Failed encode/decode operations preserve the caller's
// output object. The fixed increment makes the stream representation canonical.
bool EncodeSimulationRandomCheckpoint(
	const SimulationRandomCheckpoint& checkpoint,
	SimulationRandomCheckpointBytes& bytes) noexcept;

SimulationRandomCheckpointDecodeError DecodeSimulationRandomCheckpoint(
	const std::uint8_t* bytes, std::size_t size,
	SimulationRandomCheckpoint& checkpoint) noexcept;

class SimulationRandom final : public RandomSource
{
public:
	explicit SimulationRandom(std::uint64_t campaignSeed) noexcept;

	// The RandomSource override is the legacy-compatible surface. It returns
	// zero if the counter is exhausted and records that failure in health().
	// Authoritative callers use tryNext() so exhaustion cannot be mistaken for
	// a random zero. Bounds zero and one both return zero without consuming.
	std::uint32_t next(std::uint32_t upperBound) noexcept override;
	SimulationRandomResult nextRaw() noexcept;
	SimulationRandomResult tryNext(std::uint32_t upperBound) noexcept;
	SimulationRandomError health() const noexcept { return health_; }
	bool healthy() const noexcept
	{
		return health_ == SimulationRandomError::None;
	}

	SimulationRandomCheckpoint checkpoint() const noexcept;
	SimulationRandomCheckpointError validateCheckpoint(
		const SimulationRandomCheckpoint& checkpoint) const noexcept;
	// Validation completes before any member is changed. An invalid checkpoint
	// leaves the object unchanged. An exhausted bounded draw leaves checkpointed
	// stream state unchanged and records the failure through health().
	SimulationRandomCheckpointError restoreCheckpoint(
		const SimulationRandomCheckpoint& checkpoint) noexcept;

	std::uint64_t campaignSeed() const noexcept { return campaignSeed_; }
	std::uint64_t rawValuesGenerated() const noexcept
	{
		return rawValuesGenerated_;
	}

private:
	static std::uint64_t seededState(std::uint64_t campaignSeed) noexcept;
	static std::uint32_t output(std::uint64_t state) noexcept;
	static void advance(std::uint64_t& state) noexcept;
	static bool takeRaw(std::uint64_t& state, std::uint64_t& generated,
		std::uint32_t& value) noexcept;

	std::uint64_t campaignSeed_ = 0;
	std::uint64_t state_ = 0;
	std::uint64_t rawValuesGenerated_ = 0;
	SimulationRandomError health_ = SimulationRandomError::None;
};

#endif
