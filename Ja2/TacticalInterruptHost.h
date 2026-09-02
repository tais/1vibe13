#ifndef JA2_TACTICAL_INTERRUPT_HOST_H
#define JA2_TACTICAL_INTERRUPT_HOST_H

#include <cstdint>

#include <Engine/Adapters/JA2/TacticalEntity.h>

// Private JA2-side lifecycle. The public tactical snapshot maps this state to
// its own value-only vocabulary; raw queue entries and interrupt kinds never
// cross the adapter boundary.
enum class Ja2TacticalInterruptPhase : std::uint8_t
{
	None,
	Resolving,
	Active
};

struct Ja2TacticalInterruptProjection
{
	Ja2TacticalInterruptPhase phase = Ja2TacticalInterruptPhase::None;
	// World-local grant generation. It advances on every successful
	// StartInterrupt, survives the following None phase, and resets only when a
	// new tactical world is committed.
	std::uint64_t serial = 0;
};

enum class Ja2TacticalInterruptPassResult : std::uint8_t
{
	Applied,
	AppliedAndReleased,
	InvalidWorld,
	NotActive,
	SerialMismatch,
	ActorUnavailable,
	ActorIneligible,
	AlreadyPassed
};

Ja2TacticalInterruptProjection CaptureJa2TacticalInterruptProjection() noexcept;

// Effective co-op authority predicate: exact live identity, active player-team
// interrupt, native !moved eligibility, established co-op actor role, and no
// prior pass vote in this grant.
bool IsJa2TacticalInterruptActorEligible(TacticalEntityId actor) noexcept;

// Records one exact actor vote without mutating JA2's native moved flag. The
// final vote performs the one native whole-interrupt release synchronously.
Ja2TacticalInterruptPassResult PassJa2TacticalInterruptActor(
	std::uint64_t expectedWorldGeneration,
	std::uint64_t expectedInterruptSerial,
	TacticalEntityId actor) noexcept;

// Main-thread frame boundary fails safe when death/removal leaves no actor able
// to submit the final vote. Returns true only when it released an interrupt.
bool PollJa2TacticalInterruptRelease() noexcept;

// Native TeamTurns lifecycle hooks.
void NotifyJa2TacticalInterruptStarted() noexcept;
void NotifyJa2TacticalInterruptCleared() noexcept;
void ResetJa2TacticalInterruptForNewWorld() noexcept;

#endif
