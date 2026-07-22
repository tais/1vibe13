#include "TacticalWorldAdapter.h"

#include <cstdint>
#include <vector>

#include "Animation Control.h"
#include "Map Information.h"
#include "Overhead.h"
#include "Soldier Control.h"
#include "strategicmap.h"

namespace
{
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

TacticalWorldCaptureResult Ja2TacticalWorldAdapter::capture(
	TacticalWorldSnapshot& output) noexcept
{
	if (!gfWorldLoaded || guiWorldLoadGeneration == 0)
		return TacticalWorldCaptureResult::Unavailable;

	try
	{
		std::vector<TacticalActorSnapshot> actors;
		actors.reserve(maximumActors_ < TOTAL_SOLDIERS ? maximumActors_ : TOTAL_SOLDIERS);
		for (std::uint16_t slot = 0; slot < TOTAL_SOLDIERS; ++slot)
		{
			const SOLDIERTYPE* soldier = MercPtrs[slot];
			if (!soldier || !soldier->bActive) continue;
			if (static_cast<std::uint16_t>(soldier->ubID) != slot ||
				soldier->uiUniqueSoldierIdValue == 0)
				return TacticalWorldCaptureResult::AdapterFailure;
			if (actors.size() >= maximumActors_)
				return TacticalWorldCaptureResult::CapacityReached;
			actors.push_back(TacticalActorSnapshot{
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

		TacticalWorldSnapshot captured;
		const TacticalSnapshotCreateError result = TacticalWorldSnapshot::create(
			guiWorldLoadGeneration,
			TacticalSectorSnapshot{
				gWorldSectorX, gWorldSectorY, gbWorldSectorZ, gfWorldLoaded != FALSE},
			TacticalTurnSnapshot{
				(gTacticalStatus.uiFlags & TURNBASED) != 0,
				(gTacticalStatus.uiFlags & INCOMBAT) != 0,
				gTacticalStatus.ubCurrentTeam,
				0},
			std::move(actors), captured, maximumActors_);
		if (result == TacticalSnapshotCreateError::TooManyActors)
			return TacticalWorldCaptureResult::CapacityReached;
		if (result != TacticalSnapshotCreateError::None)
			return TacticalWorldCaptureResult::AdapterFailure;
		output = std::move(captured);
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
