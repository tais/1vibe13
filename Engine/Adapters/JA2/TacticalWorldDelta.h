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

struct TacticalActorEnteredEvent
{
	TacticalActorSnapshot actor;
};

struct TacticalActorLeftEvent
{
	TacticalEntityId actor;
};

struct TacticalActorMovedEvent
{
	TacticalEntityId actor;
	std::int32_t previousGrid;
	std::int32_t currentGrid;
	std::int8_t previousLevel;
	std::int8_t currentLevel;
	std::uint8_t previousDirection;
	std::uint8_t currentDirection;
};

struct TacticalActorStanceChangedEvent
{
	TacticalEntityId actor;
	TacticalStance previous;
	TacticalStance current;
	std::uint16_t previousAnimation;
	std::uint16_t currentAnimation;
};

struct TacticalActorVitalsChangedEvent
{
	TacticalEntityId actor;
	std::int16_t previousActionPoints;
	std::int16_t currentActionPoints;
	std::int16_t previousLife;
	std::int16_t currentLife;
	std::int16_t previousMaximumLife;
	std::int16_t currentMaximumLife;
	std::int16_t previousBreath;
	std::int16_t currentBreath;
	std::int16_t previousMaximumBreath;
	std::int16_t currentMaximumBreath;
};

using TacticalWorldEvent = std::variant<
	TacticalWorldResetEvent,
	TacticalSectorChangedEvent,
	TacticalTurnChangedEvent,
	TacticalActorEnteredEvent,
	TacticalActorLeftEvent,
	TacticalActorMovedEvent,
	TacticalActorStanceChangedEvent,
	TacticalActorVitalsChangedEvent>;

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
// allocation failure leaves the caller's previous delta untouched.
TacticalWorldDiffResult DiffTacticalWorldSnapshots(
	const TacticalWorldSnapshot& previous,
	const TacticalWorldSnapshot& current,
	std::size_t maximumEvents,
	TacticalWorldDelta& output) noexcept;

#endif
