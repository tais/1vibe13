#include "TacticalWorldAdapter.h"

#include <cstdint>
#include <limits>

#include "Animation Control.h"
#include "Map Information.h"
#include "Overhead.h"
#include "Soldier Control.h"
#include "strategicmap.h"

namespace
{
constexpr std::uint64_t IncrementSaturated(std::uint64_t value) noexcept
{
	return value == std::numeric_limits<std::uint64_t>::max() ? value : value + 1;
}

static_assert(IncrementSaturated(0) == 1, "turn serials must become nonzero");
static_assert(
	IncrementSaturated(std::numeric_limits<std::uint64_t>::max()) ==
		std::numeric_limits<std::uint64_t>::max(),
	"turn serials must saturate instead of wrapping");

TacticalStance SnapshotStance(const SOLDIERTYPE& soldier)
{
	if (soldier.usAnimState >= NUMANIMATIONSTATES) return TacticalStance::Unknown;
	switch (gAnimControl[soldier.usAnimState].ubHeight)
	{
		case ANIM_STAND: return TacticalStance::Standing;
		case ANIM_CROUCH: return TacticalStance::Crouched;
		case ANIM_PRONE: return TacticalStance::Prone;
		default: return TacticalStance::Unknown;
	}
}
}

void Ja2TacticalWorldAdapter::onWorldLoaded(std::uint64_t worldGeneration) noexcept
{
	turnIdentity_.worldGeneration = worldGeneration;
	turnIdentity_.serial = worldGeneration == 0 ? 0 : 1;
}

void Ja2TacticalWorldAdapter::onWorldUnloaded() noexcept
{
	turnIdentity_ = {};
}

void Ja2TacticalWorldAdapter::onTeamTurnBegan(
	std::uint64_t worldGeneration) noexcept
{
	if (worldGeneration == 0)
	{
		onWorldUnloaded();
		return;
	}
	if (turnIdentity_.worldGeneration != worldGeneration)
	{
		onWorldLoaded(worldGeneration);
		return;
	}
	turnIdentity_.serial = IncrementSaturated(turnIdentity_.serial);
}

void Ja2TacticalWorldAdapter::synchronizeWorldGeneration(
	std::uint64_t worldGeneration) noexcept
{
	if (turnIdentity_.worldGeneration != worldGeneration || turnIdentity_.serial == 0)
		onWorldLoaded(worldGeneration);
}

Ja2TacticalTurnIdentity Ja2TacticalWorldAdapter::liveTurnIdentity() noexcept
{
	if (!gfWorldLoaded || guiWorldLoadGeneration == 0)
		onWorldUnloaded();
	else
		synchronizeWorldGeneration(guiWorldLoadGeneration);
	return turnIdentity_;
}

TacticalWorldCaptureResult Ja2TacticalWorldAdapter::capture(
	TacticalWorldSnapshot& output) noexcept
{
	if (!gfWorldLoaded || guiWorldLoadGeneration == 0)
	{
		onWorldUnloaded();
		return TacticalWorldCaptureResult::Unavailable;
	}
	synchronizeWorldGeneration(guiWorldLoadGeneration);
	const std::uint64_t turnSerial = turnIdentity_.serial;

	try
	{
		actorScratch_.clear();
		actorScratch_.reserve(
			maximumActors_ < TOTAL_SOLDIERS ? maximumActors_ : TOTAL_SOLDIERS);
		for (std::uint16_t slot = 0; slot < TOTAL_SOLDIERS; ++slot)
		{
			const SOLDIERTYPE* soldier = MercPtrs[slot];
			if (!soldier || !soldier->bActive) continue;
			if (static_cast<std::uint16_t>(soldier->ubID) != slot ||
				soldier->uiUniqueSoldierIdValue == 0)
				return TacticalWorldCaptureResult::AdapterFailure;
			if (actorScratch_.size() >= maximumActors_)
				return TacticalWorldCaptureResult::CapacityReached;
			actorScratch_.push_back(TacticalActorSnapshot{
				TacticalEntityId{slot, soldier->uiUniqueSoldierIdValue},
				static_cast<std::uint8_t>(soldier->bTeam),
				static_cast<std::uint16_t>(soldier->ubProfile),
				soldier->sGridNo,
				static_cast<std::int8_t>(soldier->pathing.bLevel),
				soldier->ubDirection,
				soldier->usAnimState,
				SnapshotStance(*soldier),
				soldier->bActionPoints,
				soldier->stats.bLife,
				soldier->stats.bLifeMax,
				soldier->bBreath,
				soldier->bBreathMax,
				soldier->bActive != FALSE,
				soldier->bInSector != FALSE});
		}

		const TacticalSnapshotCreateError result =
			TacticalWorldSnapshot::createReusableOrdered(
			guiWorldLoadGeneration,
			TacticalSectorSnapshot{
				gWorldSectorX, gWorldSectorY, gbWorldSectorZ, gfWorldLoaded != FALSE},
			TacticalTurnSnapshot{
				(gTacticalStatus.uiFlags & TURNBASED) != 0,
				(gTacticalStatus.uiFlags & INCOMBAT) != 0,
				gTacticalStatus.ubCurrentTeam,
				turnSerial},
			actorScratch_, output, maximumActors_);
		if (result == TacticalSnapshotCreateError::TooManyActors)
			return TacticalWorldCaptureResult::CapacityReached;
		if (result != TacticalSnapshotCreateError::None)
			return TacticalWorldCaptureResult::AdapterFailure;
		return TacticalWorldCaptureResult::Success;
	}
	catch (...)
	{
		return TacticalWorldCaptureResult::AllocationFailure;
	}
}

Ja2TacticalWorldAdapter& GetJa2TacticalWorldAdapter()
{
	static Ja2TacticalWorldAdapter adapter(TOTAL_SOLDIERS);
	return adapter;
}

void NotifyJa2TacticalWorldLoaded(std::uint64_t worldGeneration) noexcept
{
	GetJa2TacticalWorldAdapter().onWorldLoaded(worldGeneration);
}

void NotifyJa2TacticalWorldUnloaded() noexcept
{
	GetJa2TacticalWorldAdapter().onWorldUnloaded();
}

void NotifyJa2TacticalTeamTurnBegan(std::uint64_t worldGeneration) noexcept
{
	GetJa2TacticalWorldAdapter().onTeamTurnBegan(worldGeneration);
}
