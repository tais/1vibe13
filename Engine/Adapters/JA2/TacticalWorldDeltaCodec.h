#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_CODEC_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_CODEC_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/TacticalWorldDelta.h>

// Version 6 adds compact interrupt state. It is a
// standalone little-endian transport
// contract: no JA2 savegame or command replay bytes are read or written here.
inline constexpr std::uint16_t TacticalWorldDeltaWireVersion = 6;
inline constexpr std::size_t EncodedTacticalActorLoadoutChangedEventBytes =
	1 + 6 + 2 * 5 * 12;
static_assert(EncodedTacticalActorLoadoutChangedEventBytes == 127,
	"the five-slot loadout-change event is a fixed wire contract");

// A default snapshot can produce at most four changes per actor plus bounded
// door, sector, and turn changes. This also prevents an untrusted event count
// from driving an unbounded allocation during decode.
inline constexpr std::size_t MaximumTacticalWorldDeltaEvents =
	TacticalWorldSnapshot::DefaultMaximumActors * 4 +
	TacticalWorldSnapshot::DefaultMaximumDoors * 2 + 2;

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
