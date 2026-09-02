#include "TacticalInterruptHost.h"

#include "DedicatedCoopMissionPolicy.h"
#include "SoldierRepository.h"
#include "TacticalWorldAdapter.h"

#include "Assignments.h"
#include "Overhead.h"
#include "Soldier macros.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "TeamTurns.h"
#include "opplist.h"

#include <array>
#include <limits>

namespace
{
struct InterruptCoordinator
{
	std::uint64_t serial = 0;
	bool active = false;
	bool serialExhausted = false;
	std::array<TacticalEntityId, TOTAL_SOLDIERS> passed{};
};

InterruptCoordinator Coordinator;

struct AdvancedInterruptSerial
{
	std::uint64_t value = 0;
	bool exhausted = false;
};

constexpr AdvancedInterruptSerial AdvanceInterruptSerial(
	std::uint64_t value) noexcept
{
	return value == std::numeric_limits<std::uint64_t>::max()
		? AdvancedInterruptSerial{value, true}
		: AdvancedInterruptSerial{value + 1, false};
}

static_assert(AdvanceInterruptSerial(0).value == 1 &&
	!AdvanceInterruptSerial(0).exhausted &&
	AdvanceInterruptSerial(std::numeric_limits<std::uint64_t>::max()).value ==
		std::numeric_limits<std::uint64_t>::max() &&
	AdvanceInterruptSerial(std::numeric_limits<std::uint64_t>::max()).exhausted,
	"interrupt serial exhaustion must saturate and fail closed, never reuse");

bool TriggerableInterruptPending() noexcept
{
	const std::uint8_t pending = GetJa2PendingInterrupt();
	return pending != DISABLED_INTERRUPT &&
		pending != UNTRIGGERED_INTERRUPT;
}

bool ExactVotePresent(TacticalEntityId actor) noexcept
{
	return actor.slot < Coordinator.passed.size() &&
		Coordinator.passed[actor.slot] == actor;
}

bool NativeActorEligible(TacticalEntityId actorId) noexcept
{
	if (!Coordinator.active || Coordinator.serialExhausted ||
		actorId.slot >= TOTAL_SOLDIERS ||
		GetJa2TacticalCurrentTeam() != gbPlayerNum)
		return false;
	TacticalActor* const actor = ResolveJa2TacticalEntity(actorId);
	if (!actor || actor->roster().team() != GetJa2TacticalCurrentTeam() ||
		OK_CONTROLLABLE_MERC(actor) == FALSE ||
		actor->turnState().moved() != FALSE)
		return false;
	const std::uint32_t flags = actor->status().flags();
	return DedicatedCoopEstablishedActorRoleEligible(
		actor->assignment().current() < ON_DUTY,
		(flags & SOLDIER_VEHICLE) != 0,
		(flags & SOLDIER_DRIVER) != 0,
		(flags & SOLDIER_PASSENGER) != 0);
}

bool HasRemainingEligibleActor() noexcept
{
	const Ja2SoldierRepository& repository = GetJa2SoldierRepository();
	for (std::size_t slot = 0; slot < repository.capacity(); ++slot)
	{
		if (slot >= TOTAL_SOLDIERS) break;
		const TacticalEntityId actor = GetJa2TacticalEntityId(
			static_cast<std::uint16_t>(slot));
		if (actor.valid() && NativeActorEligible(actor) &&
			!ExactVotePresent(actor))
			return true;
	}
	return false;
}
}

Ja2TacticalInterruptProjection CaptureJa2TacticalInterruptProjection() noexcept
{
	Ja2TacticalInterruptProjection projection;
	projection.serial = Coordinator.serial;
	if (!IsJa2TacticalWorldLoaded() || !IsJa2TacticalTurnBasedCombat())
		return projection;
	if (TriggerableInterruptPending())
		projection.phase = Ja2TacticalInterruptPhase::Resolving;
	else if (Coordinator.active && INTERRUPT_QUEUED)
		projection.phase = Ja2TacticalInterruptPhase::Active;
	return projection;
}

bool IsJa2TacticalInterruptActorEligible(TacticalEntityId actor) noexcept
{
	return CaptureJa2TacticalInterruptProjection().phase ==
			Ja2TacticalInterruptPhase::Active &&
		NativeActorEligible(actor) && !ExactVotePresent(actor);
}

Ja2TacticalInterruptPassResult PassJa2TacticalInterruptActor(
	std::uint64_t expectedWorldGeneration,
	std::uint64_t expectedInterruptSerial,
	TacticalEntityId actor) noexcept
{
	const TacticalWorldSession::Snapshot& world = CaptureJa2TacticalWorld();
	if (!world.loaded || world.worldGeneration == 0 ||
		world.worldGeneration != expectedWorldGeneration)
		return Ja2TacticalInterruptPassResult::InvalidWorld;
	const Ja2TacticalInterruptProjection interrupt =
		CaptureJa2TacticalInterruptProjection();
	if (interrupt.phase != Ja2TacticalInterruptPhase::Active)
		return Ja2TacticalInterruptPassResult::NotActive;
	if (expectedInterruptSerial == 0 ||
		expectedInterruptSerial != interrupt.serial)
		return Ja2TacticalInterruptPassResult::SerialMismatch;
	if (!actor.valid() || actor.slot >= TOTAL_SOLDIERS ||
		ResolveJa2TacticalEntity(actor) == nullptr)
		return Ja2TacticalInterruptPassResult::ActorUnavailable;
	if (ExactVotePresent(actor))
		return Ja2TacticalInterruptPassResult::AlreadyPassed;
	if (!NativeActorEligible(actor))
		return Ja2TacticalInterruptPassResult::ActorIneligible;

	Coordinator.passed[actor.slot] = actor;
	if (HasRemainingEligibleActor())
		return Ja2TacticalInterruptPassResult::Applied;

	EndInterrupt(FALSE, FALSE);
	return Ja2TacticalInterruptPassResult::AppliedAndReleased;
}

bool PollJa2TacticalInterruptRelease() noexcept
{
	if (CaptureJa2TacticalInterruptProjection().phase !=
			Ja2TacticalInterruptPhase::Active ||
		GetJa2TacticalCurrentTeam() != gbPlayerNum ||
		HasRemainingEligibleActor())
		return false;
	EndInterrupt(FALSE, FALSE);
	return true;
}

void NotifyJa2TacticalInterruptStarted() noexcept
{
	const AdvancedInterruptSerial advanced =
		AdvanceInterruptSerial(Coordinator.serial);
	Coordinator.serial = advanced.value;
	Coordinator.serialExhausted =
		Coordinator.serialExhausted || advanced.exhausted;
	Coordinator.active = true;
	Coordinator.passed = {};
}

void NotifyJa2TacticalInterruptCleared() noexcept
{
	Coordinator.active = false;
	Coordinator.passed = {};
}

void ResetJa2TacticalInterruptForNewWorld() noexcept
{
	Coordinator = InterruptCoordinator{};
}
