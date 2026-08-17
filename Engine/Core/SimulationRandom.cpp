#include <Engine/Core/SimulationRandom.h>

#include <limits>

namespace
{
constexpr std::uint64_t Pcg32Multiplier = 6364136223846793005ULL;

void WriteU32(SimulationRandomCheckpointBytes& bytes,
	std::size_t offset, std::uint32_t value) noexcept
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

void WriteU64(SimulationRandomCheckpointBytes& bytes,
	std::size_t offset, std::uint64_t value) noexcept
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

std::uint32_t ReadU32(const std::uint8_t* bytes, std::size_t offset) noexcept
{
	std::uint32_t value = 0;
	for (unsigned shift = 0; shift < 32; shift += 8)
		value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
	return value;
}

std::uint64_t ReadU64(const std::uint8_t* bytes, std::size_t offset) noexcept
{
	std::uint64_t value = 0;
	for (unsigned shift = 0; shift < 64; shift += 8)
		value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
	return value;
}

SimulationRandomCheckpointError ValidateStructure(
	const SimulationRandomCheckpoint& checkpoint) noexcept
{
	if (checkpoint.schema != SimulationRandomCheckpoint::CurrentSchema)
		return SimulationRandomCheckpointError::InvalidSchema;
	if (checkpoint.algorithm != SimulationRandomCheckpoint::CurrentAlgorithm)
		return SimulationRandomCheckpointError::UnsupportedAlgorithm;
	if (checkpoint.increment != SimulationRandomPcg32Increment)
		return SimulationRandomCheckpointError::InvalidStream;
	return SimulationRandomCheckpointError::None;
}
}

bool EncodeSimulationRandomCheckpoint(
	const SimulationRandomCheckpoint& checkpoint,
	SimulationRandomCheckpointBytes& bytes) noexcept
{
	if (ValidateStructure(checkpoint) !=
		SimulationRandomCheckpointError::None)
		return false;

	SimulationRandomCheckpointBytes encoded{};
	WriteU32(encoded, 0, checkpoint.schema);
	WriteU32(encoded, 4, checkpoint.algorithm);
	WriteU64(encoded, 8, checkpoint.campaignSeed);
	WriteU64(encoded, 16, checkpoint.state);
	WriteU64(encoded, 24, checkpoint.increment);
	WriteU64(encoded, 32, checkpoint.rawValuesGenerated);
	bytes = encoded;
	return true;
}

SimulationRandomCheckpointDecodeError DecodeSimulationRandomCheckpoint(
	const std::uint8_t* bytes, std::size_t size,
	SimulationRandomCheckpoint& checkpoint) noexcept
{
	if (size != SimulationRandomCheckpointWireSize || bytes == nullptr)
		return SimulationRandomCheckpointDecodeError::WrongSize;

	SimulationRandomCheckpoint decoded;
	decoded.schema = ReadU32(bytes, 0);
	decoded.algorithm = ReadU32(bytes, 4);
	decoded.campaignSeed = ReadU64(bytes, 8);
	decoded.state = ReadU64(bytes, 16);
	decoded.increment = ReadU64(bytes, 24);
	decoded.rawValuesGenerated = ReadU64(bytes, 32);
	switch (ValidateStructure(decoded))
	{
		case SimulationRandomCheckpointError::None:
			checkpoint = decoded;
			return SimulationRandomCheckpointDecodeError::None;
		case SimulationRandomCheckpointError::InvalidSchema:
			return SimulationRandomCheckpointDecodeError::UnsupportedSchema;
		case SimulationRandomCheckpointError::UnsupportedAlgorithm:
			return SimulationRandomCheckpointDecodeError::UnsupportedAlgorithm;
		case SimulationRandomCheckpointError::InvalidStream:
			return SimulationRandomCheckpointDecodeError::InvalidStream;
		case SimulationRandomCheckpointError::CampaignSeedMismatch:
			break;
	}
	return SimulationRandomCheckpointDecodeError::InvalidStream;
}

SimulationRandom::SimulationRandom(std::uint64_t campaignSeed) noexcept
	: SimulationRandom(campaignSeed, 0)
{
}

SimulationRandom::SimulationRandom(std::uint64_t campaignSeed,
	std::uint64_t initialConsumptionEpoch) noexcept
	: campaignSeed_(campaignSeed), state_(seededState(campaignSeed)),
	  consumptionEpoch_(initialConsumptionEpoch)
{
}

std::uint32_t SimulationRandom::next(std::uint32_t upperBound) noexcept
{
	const SimulationRandomResult result = tryNext(upperBound);
	return result ? result.value : 0;
}

SimulationRandomResult SimulationRandom::nextRaw() noexcept
{
	if (health_ != SimulationRandomError::None) return {health_, 0};
	std::uint64_t nextState = state_;
	std::uint64_t nextGenerated = rawValuesGenerated_;
	std::uint32_t value = 0;
	if (!takeRaw(nextState, nextGenerated, value))
	{
		health_ = SimulationRandomError::SequenceExhausted;
		return {SimulationRandomError::SequenceExhausted, 0};
	}
	if (!consumptionEpoch_.tryAdvance())
	{
		health_ = SimulationRandomError::SequenceExhausted;
		return {SimulationRandomError::SequenceExhausted, 0};
	}
	state_ = nextState;
	rawValuesGenerated_ = nextGenerated;
	return {SimulationRandomError::None, value};
}

SimulationRandomResult SimulationRandom::tryNext(
	std::uint32_t upperBound) noexcept
{
	if (upperBound <= 1) return {SimulationRandomError::None, 0};
	if (health_ != SimulationRandomError::None) return {health_, 0};

	std::uint64_t nextState = state_;
	std::uint64_t nextGenerated = rawValuesGenerated_;
	const std::uint32_t threshold =
		static_cast<std::uint32_t>(0u - upperBound) % upperBound;
	for (;;)
	{
		std::uint32_t value = 0;
		if (!takeRaw(nextState, nextGenerated, value))
		{
			health_ = SimulationRandomError::SequenceExhausted;
			return {SimulationRandomError::SequenceExhausted, 0};
		}
		if (value >= threshold)
		{
			const std::uint64_t consumed =
				nextGenerated - rawValuesGenerated_;
			if (!consumptionEpoch_.tryAdvance(consumed))
			{
				health_ = SimulationRandomError::SequenceExhausted;
				return {SimulationRandomError::SequenceExhausted, 0};
			}
			state_ = nextState;
			rawValuesGenerated_ = nextGenerated;
			return {SimulationRandomError::None, value % upperBound};
		}
	}
}

SimulationRandomCheckpoint SimulationRandom::checkpoint() const noexcept
{
	SimulationRandomCheckpoint saved;
	saved.campaignSeed = campaignSeed_;
	saved.state = state_;
	saved.rawValuesGenerated = rawValuesGenerated_;
	return saved;
}

SimulationRandomCheckpointError SimulationRandom::validateCheckpoint(
	const SimulationRandomCheckpoint& saved) const noexcept
{
	const SimulationRandomCheckpointError structure = ValidateStructure(saved);
	if (structure != SimulationRandomCheckpointError::None) return structure;
	if (saved.campaignSeed != campaignSeed_)
		return SimulationRandomCheckpointError::CampaignSeedMismatch;
	return SimulationRandomCheckpointError::None;
}

SimulationRandomCheckpointError SimulationRandom::restoreCheckpoint(
	const SimulationRandomCheckpoint& saved) noexcept
{
	const SimulationRandomCheckpointError validation =
		validateCheckpoint(saved);
	if (validation != SimulationRandomCheckpointError::None) return validation;
	state_ = saved.state;
	rawValuesGenerated_ = saved.rawValuesGenerated;
	health_ = SimulationRandomError::None;
	return SimulationRandomCheckpointError::None;
}

std::uint64_t SimulationRandom::seededState(
	std::uint64_t campaignSeed) noexcept
{
	std::uint64_t state = 0;
	advance(state);
	state += campaignSeed;
	advance(state);
	return state;
}

std::uint32_t SimulationRandom::output(std::uint64_t state) noexcept
{
	const std::uint32_t xorshifted = static_cast<std::uint32_t>(
		((state >> 18u) ^ state) >> 27u);
	const std::uint32_t rotation = static_cast<std::uint32_t>(state >> 59u);
	return (xorshifted >> rotation) |
		(xorshifted << ((0u - rotation) & 31u));
}

void SimulationRandom::advance(std::uint64_t& state) noexcept
{
	state = state * Pcg32Multiplier + SimulationRandomPcg32Increment;
}

bool SimulationRandom::takeRaw(std::uint64_t& state,
	std::uint64_t& generated, std::uint32_t& value) noexcept
{
	if (generated == std::numeric_limits<std::uint64_t>::max())
		return false;
	value = output(state);
	advance(state);
	++generated;
	return true;
}
