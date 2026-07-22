#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_CODEC_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_CODEC_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/TacticalWorldDelta.h>

// Version 1 uses a little-endian, field-by-field representation. It is a
// standalone transport contract: no JA2 savegame or command replay bytes are
// read or written by this codec.
inline constexpr std::uint16_t TacticalWorldDeltaWireVersion = 1;

// A default snapshot can produce at most three changes per actor plus sector
// and turn changes. Keeping the wire contract bounded also prevents an
// untrusted event count from driving an unbounded allocation during decode.
inline constexpr std::size_t MaximumTacticalWorldDeltaEvents =
	TacticalWorldSnapshot::DefaultMaximumActors * 3 + 2;

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
