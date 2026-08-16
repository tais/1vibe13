#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SNAPSHOT_CODEC_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SNAPSHOT_CODEC_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

// Version 1 is an explicitly little-endian, field-by-field tactical baseline.
// It is independent from legacy multiplayer structs and JA2 save bytes.
inline constexpr std::uint16_t TacticalWorldSnapshotWireVersion = 1;
inline constexpr std::size_t EncodedTacticalWorldSnapshotHeaderBytes = 35;
inline constexpr std::size_t EncodedTacticalActorSnapshotBytes = 30;
inline constexpr std::size_t MaximumEncodedTacticalWorldSnapshotBytes =
	EncodedTacticalWorldSnapshotHeaderBytes +
	TacticalWorldSnapshot::DefaultMaximumActors *
		EncodedTacticalActorSnapshotBytes;

enum class TacticalWorldSnapshotEncodeResult
{
	Success,
	Invalid,
	TooManyActors,
	AllocationFailure
};

enum class TacticalWorldSnapshotDecodeResult
{
	Success,
	Invalid,
	UnsupportedVersion,
	TooManyActors,
	AllocationFailure
};

// Both operations are transactional. Failure leaves the caller's previous
// byte buffer or snapshot untouched. maximumActors may lower, but never raise,
// the format's fixed allocation bound.
TacticalWorldSnapshotEncodeResult EncodeTacticalWorldSnapshot(
	const TacticalWorldSnapshot& snapshot,
	std::vector<std::uint8_t>& bytes,
	std::size_t maximumActors =
		TacticalWorldSnapshot::DefaultMaximumActors) noexcept;

TacticalWorldSnapshotDecodeResult DecodeTacticalWorldSnapshot(
	const std::vector<std::uint8_t>& bytes,
	TacticalWorldSnapshot& snapshot,
	std::size_t maximumActors =
		TacticalWorldSnapshot::DefaultMaximumActors) noexcept;

#endif
