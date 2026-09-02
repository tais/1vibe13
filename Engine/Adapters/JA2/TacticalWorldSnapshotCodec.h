#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SNAPSHOT_CODEC_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SNAPSHOT_CODEC_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

// Version 7 adds canonical interrupt phase/serial and actor eligibility.
inline constexpr std::uint16_t TacticalWorldSnapshotWireVersion = 7;
inline constexpr std::size_t EncodedTacticalWorldSnapshotHeaderBytes = 53;
inline constexpr std::size_t EncodedTacticalHandItemSnapshotBytes = 12;
inline constexpr std::size_t EncodedTacticalActorSnapshotBytes = 92;
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
