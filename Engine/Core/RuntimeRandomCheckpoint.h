#ifndef ENGINE_CORE_RUNTIME_RANDOM_CHECKPOINT_H
#define ENGINE_CORE_RUNTIME_RANDOM_CHECKPOINT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeFingerprint.h>
#include <Engine/Core/SimulationRandom.h>

// GRNG is both the runtime-save section type and the magic of the independently
// checksummed envelope stored in that section. Version 1 has one canonical,
// fixed-size payload; changing its field order or size requires a new version.
constexpr std::uint32_t RuntimeRandomCheckpointMagic = 0x474e5247u;
constexpr std::uint32_t RuntimeRandomCheckpointSectionType =
	RuntimeRandomCheckpointMagic;
constexpr std::uint16_t RuntimeRandomCheckpointVersion = 1;
constexpr std::size_t RuntimeRandomCheckpointPayloadBytes = 68;

struct RuntimeRandomCheckpoint
{
	RuntimeCompatibilityFingerprint compatibility;
	SimulationRandomCheckpoint simulationRandom;
	std::uint64_t packageRandomHostSeed = 0;

	bool operator==(const RuntimeRandomCheckpoint& other) const noexcept
	{
		return compatibility == other.compatibility &&
			simulationRandom == other.simulationRandom &&
			packageRandomHostSeed == other.packageRandomHostSeed;
	}

	bool operator!=(const RuntimeRandomCheckpoint& other) const noexcept
	{
		return !(*this == other);
	}
};

enum class RuntimeRandomCheckpointSaveError : std::uint8_t
{
	None,
	InvalidCompatibility,
	UnsupportedSimulationSchema,
	UnsupportedSimulationAlgorithm,
	InvalidSimulationStream,
	TooLarge,
	StorageError
};

enum class RuntimeRandomCheckpointLoadError : std::uint8_t
{
	None,
	NotFound,
	InvalidOrUnsupported,
	TooLarge,
	IntegrityFailure,
	StorageError,
	MalformedPayload,
	IncompatibleRuntime,
	UnsupportedSimulationSchema,
	UnsupportedSimulationAlgorithm,
	InvalidSimulationStream,
	CampaignSeedMismatch,
	PackageRandomHostSeedMismatch
};

struct RuntimeRandomCheckpointLoadResult
{
	RuntimeRandomCheckpointLoadError error =
		RuntimeRandomCheckpointLoadError::None;
	std::uint16_t storedVersion = 0;
	RuntimeCompatibilityFingerprint storedCompatibility{0, 0, 0};

	explicit operator bool() const noexcept
	{
		return error == RuntimeRandomCheckpointLoadError::None;
	}
};

// Wire payload, all little-endian:
// compatibility schema:u32, high:u64, low:u64;
// canonical SimulationRandomCheckpoint:40 bytes;
// packageRandomHostSeed:u64.
//
// Decode additionally binds the persisted roots to the campaign/runtime that
// requested the load. Every operation is transactional: failed encoding keeps
// encoded unchanged and failed decoding/loading keeps checkpoint unchanged.
class RuntimeRandomCheckpointService
{
public:
	explicit RuntimeRandomCheckpointService(const PersistenceService& persistence)
		: persistence_(persistence) {}

	RuntimeRandomCheckpointSaveError save(const std::string& path,
		const RuntimeRandomCheckpoint& checkpoint) const noexcept;
	RuntimeRandomCheckpointSaveError encode(
		const RuntimeRandomCheckpoint& checkpoint,
		std::vector<std::uint8_t>& encoded) const noexcept;
	RuntimeRandomCheckpointLoadResult load(const std::string& path,
		RuntimeCompatibilityFingerprint expectedCompatibility,
		std::uint64_t expectedCampaignSeed,
		std::uint64_t expectedPackageRandomHostSeed,
		RuntimeRandomCheckpoint& checkpoint) const noexcept;
	RuntimeRandomCheckpointLoadResult decode(
		const std::vector<std::uint8_t>& encoded,
		RuntimeCompatibilityFingerprint expectedCompatibility,
		std::uint64_t expectedCampaignSeed,
		std::uint64_t expectedPackageRandomHostSeed,
		RuntimeRandomCheckpoint& checkpoint) const noexcept;

private:
	const PersistenceService& persistence_;
};

#endif
