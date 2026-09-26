#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_CODEC_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_CODEC_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/TacticalWorldDelta.h>
#include <Engine/Adapters/JA2/TacticalWorldSnapshotCodec.h>

// Version 8 coalesces current actor records with renderer inputs and adds
// world lighting changes. It is a
// standalone little-endian transport
// contract: no JA2 savegame or command replay bytes are read or written here.
inline constexpr std::uint16_t TacticalWorldDeltaWireVersion = 8;
inline constexpr std::size_t EncodedTacticalActorFullEventBytes =
	1 + EncodedTacticalActorSnapshotBytes;
inline constexpr std::size_t EncodedTacticalLightingChangedEventBytes = 3;

// Disjoint old/new identity sets can produce one enter and one leave per actor
// or door. Sector, turn, and lighting are singleton categories. This also
// prevents an untrusted event count from driving an unbounded allocation.
inline constexpr std::size_t MaximumTacticalWorldDeltaEvents =
	TacticalWorldSnapshot::DefaultMaximumActors * 2 +
	TacticalWorldSnapshot::DefaultMaximumDoors * 2 + 3;

enum class TacticalWorldDeltaEncodeResult
{
	Success,
	Invalid,
	TooManyEvents,
	AllocationFailure
};

enum class TacticalWorldDeltaDecodeResult
{
	Success,
	Invalid,
	UnsupportedVersion,
	TooManyEvents,
	AllocationFailure
};

// Both operations are transactional. Failure leaves the caller's previous
// byte buffer or delta untouched. maximumEvents may lower, but never raise,
// the format's fixed allocation bound.
TacticalWorldDeltaEncodeResult EncodeTacticalWorldDelta(
	const TacticalWorldDelta& delta,
	std::vector<std::uint8_t>& bytes,
	std::size_t maximumEvents = MaximumTacticalWorldDeltaEvents) noexcept;

TacticalWorldDeltaDecodeResult DecodeTacticalWorldDelta(
	const std::vector<std::uint8_t>& bytes,
	TacticalWorldDelta& delta,
	std::size_t maximumEvents = MaximumTacticalWorldDeltaEvents) noexcept;

#endif
