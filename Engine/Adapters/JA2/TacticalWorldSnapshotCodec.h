#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SNAPSHOT_CODEC_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SNAPSHOT_CODEC_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

// Version 12 appends bounded authority medical-service outcomes after kit points.
// Versions 10 and 11 are reserved intermediate medical formats and are rejected.
// Version 12 appends the canonical medical fields to the released version-9 actor.
inline constexpr std::uint16_t TacticalWorldSnapshotWireVersion = 12;
inline constexpr std::size_t EncodedTacticalMapAssetKeyBytes =
	TacticalMapAssetKeyStorageBytes;
inline constexpr std::size_t EncodedTacticalSectorSnapshotBytes =
	6 + EncodedTacticalMapAssetKeyBytes;
inline constexpr std::size_t EncodedTacticalWorldSnapshotHeaderBytes = 314;
inline constexpr std::size_t EncodedTacticalHandItemSnapshotBytes = 12;
inline constexpr std::size_t EncodedTacticalActorPresentationSnapshotBytes = 44;
inline constexpr std::size_t EncodedTacticalMedicalServiceSnapshotBytes = 8;
inline constexpr std::size_t EncodedTacticalActorSnapshotBytes =
	96 + EncodedTacticalActorPresentationSnapshotBytes +
	EncodedTacticalMedicalServiceSnapshotBytes;
inline constexpr std::size_t EncodedTacticalDoorSnapshotBytes = 7;
inline constexpr std::size_t MaximumEncodedTacticalWorldSnapshotBytes =
	EncodedTacticalWorldSnapshotHeaderBytes +
	TacticalWorldSnapshot::DefaultMaximumActors *
		EncodedTacticalActorSnapshotBytes +
	TacticalWorldSnapshot::DefaultMaximumDoors *
		EncodedTacticalDoorSnapshotBytes;

enum class TacticalWorldSnapshotEncodeResult
{
	Success,
	Invalid,
	TooManyActors,
	TooManyDoors,
	AllocationFailure
};

enum class TacticalWorldSnapshotDecodeResult
{
	Success,
	Invalid,
	UnsupportedVersion,
	TooManyActors,
	TooManyDoors,
	AllocationFailure
};

// Both operations are transactional. Failure leaves the caller's previous
// byte buffer or snapshot untouched. maximumActors may lower, but never raise,
// the format's fixed allocation bound.
TacticalWorldSnapshotEncodeResult EncodeTacticalWorldSnapshot(
	const TacticalWorldSnapshot& snapshot,
	std::vector<std::uint8_t>& bytes,
	std::size_t maximumActors =
		TacticalWorldSnapshot::DefaultMaximumActors,
	std::size_t maximumDoors =
		TacticalWorldSnapshot::DefaultMaximumDoors) noexcept;

TacticalWorldSnapshotDecodeResult DecodeTacticalWorldSnapshot(
	const std::vector<std::uint8_t>& bytes,
	TacticalWorldSnapshot& snapshot,
	std::size_t maximumActors =
		TacticalWorldSnapshot::DefaultMaximumActors,
	std::size_t maximumDoors =
		TacticalWorldSnapshot::DefaultMaximumDoors) noexcept;

#endif
