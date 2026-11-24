#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_H

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

struct TacticalWorldResetEvent
{
	std::uint64_t previousEpoch;
	std::uint64_t currentEpoch;
};

struct TacticalSectorChangedEvent
{
	TacticalSectorSnapshot previous;
	TacticalSectorSnapshot current;
};

struct TacticalTurnChangedEvent
{
	TacticalTurnSnapshot previous;
	TacticalTurnSnapshot current;
};

struct TacticalLightingChangedEvent
{
	TacticalWorldLightingSnapshot previous;
	TacticalWorldLightingSnapshot current;
};

struct TacticalActorEnteredEvent
{
	TacticalActorSnapshot actor;
};

struct TacticalActorLeftEvent
{
	TacticalEntityId actor;
};

// One current full record coalesces every persistent-actor field change. The
// delta's exact base revision identifies the predecessor, avoiding four
// redundant previous-value copies and keeping renderer-rich worst cases under
// the co-op transport ceiling.
struct TacticalActorUpdatedEvent
{
	TacticalActorSnapshot actor;
};

struct TacticalDoorEnteredEvent
{
	TacticalDoorSnapshot door;
};

struct TacticalDoorLeftEvent
{
	std::int32_t baseGrid = -1;
};

struct TacticalDoorChangedEvent
{
	TacticalDoorSnapshot previous;
	TacticalDoorSnapshot current;
};

using TacticalWorldEvent = std::variant<
	TacticalWorldResetEvent,
	TacticalSectorChangedEvent,
	TacticalTurnChangedEvent,
	TacticalLightingChangedEvent,
	TacticalActorEnteredEvent,
	TacticalActorLeftEvent,
	TacticalActorUpdatedEvent,
	TacticalDoorEnteredEvent,
	TacticalDoorLeftEvent,
	TacticalDoorChangedEvent>;

struct TacticalWorldDelta
{
	std::uint64_t previousEpoch = 0;
	std::uint64_t currentEpoch = 0;
	std::vector<TacticalWorldEvent> events;
};

enum class TacticalWorldDiffResult
{
	Success,
	InvalidSnapshot,
	CapacityReached,
	AllocationFailure
};

// Produces events in deterministic category/entity order. Epoch changes emit
// one reset instead of comparing unrelated tactical worlds. Capacity or
// allocation failure leaves the caller's previous delta untouched. Successful
// calls retain storage for maximumEvents, making repeated bounded diffs
// allocation-free after the first successful call on each output object.
TacticalWorldDiffResult DiffTacticalWorldSnapshots(
	const TacticalWorldSnapshot& previous,
	const TacticalWorldSnapshot& current,
	std::size_t maximumEvents,
	TacticalWorldDelta& output) noexcept;

#endif
