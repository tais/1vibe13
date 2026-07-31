#include "TacticalActorTraversal.h"

#include "Animation Control.h"
#include "Dialogue Control.h"
#include "Drugs And Alcohol.h"
#include "DynamicDialogue.h"
#include "GameSettings.h"
#include "Handle UI.h"
#include "Isometric Utils.h"
#include "Morale.h"
#include "Points.h"
#include "Soldier Control.h"
#include "Structure Wrap.h"
#include "TacticalActorMedicalServices.h"
#include "TacticalActorMobility.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "Weapons.h"
#include "ai.h"
#include "builddefines.h"
#include "connect.h"
#include "message.h"
#include "random.h"
#include "soldier tile.h"
#include "structure.h"
#include "worlddef.h"
#include "worldman.h"

#include <cstdint>

void EVENT_InternalSetSoldierDesiredDirection(
	TacticalActor* actor,
	UINT8 direction,
	BOOLEAN initialMove,
	UINT16 animation);

namespace
{
bool hasLiveTraversalContext(
	const TacticalActor& actor) noexcept
{
	return IsJa2TacticalWorldLoaded() &&
		actor.roster().active() &&
		actor.roster().inSector() &&
		actor.vitals().health() >= OKLIFE &&
		actor.identity().id().i < TOTAL_SOLDIERS &&
		actor.identity().bodyType() < TOTALBODYTYPES &&
		!TileIsOutOfBounds(actor.position().gridNo()) &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL &&
		actor.position().direction() <
			NUM_WORLD_DIRECTIONS &&
		actor.animationPlayback().state() <
			NUMANIMATIONSTATES;
}

bool resolveTraversalGrid(
	const TacticalActor& actor,
	std::uint8_t direction,
	std::int32_t& grid)
{
	if (direction >= NUM_WORLD_DIRECTIONS)
		return false;

	grid = NewGridNo(
		actor.position().gridNo(),
		static_cast<std::uint16_t>(
			DirectionInc(direction)));
	return !TileIsOutOfBounds(grid) &&
		grid != actor.position().gridNo();
}

bool destinationIsOccupied(
	const TacticalActor& actor,
	std::int32_t grid,
	std::int8_t level)
{
	const SoldierID occupant = WhoIsThere2(grid, level);
	return occupant != NOBODY &&
		occupant != actor.identity().id();
}

bool traversalDirection(
	const TacticalActor& actor,
	std::int8_t& direction)
{
	if (actor.pathing().pathSize() > MAX_PATH_LIST_SIZE ||
		actor.pathing().pathIndex() > MAX_PATH_LIST_SIZE)
	{
		return false;
	}

	direction =
		actor.pathing().pathIndex() <
				actor.pathing().pathSize()
			? static_cast<std::int8_t>(
				actor.pathing().path()[
					actor.pathing().pathIndex()])
			: actor.position().direction();
	return direction >= 0 &&
		direction < NUM_WORLD_DIRECTIONS;
}

void cancelMedicalServices(TacticalActor& actor)
{
	TacticalActorMedicalServices::
		cancelReceiving(actor, false);
	TacticalActorMedicalServices::
		cancelProviding(actor, false);
}

void rejectAiTraversal(TacticalActor& actor)
{
	actor.aiPlanning().action() = AI_ACTION_NONE;
}
}

bool TacticalActorTraversal::beginRoofClimb(
	TacticalActor& actor)
{
	if (!hasLiveTraversalContext(actor))
		return false;

	if (!TacticalActorMobility::
			canClimbWithCurrentBackpack(actor))
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			NewInvMessage[NIV_NO_CLIMB]);
		return false;
	}

	if (is_client)
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			MPClientMessage[43]);
		return false;
	}

	std::int8_t direction = 0;
	if (actor.position().level() != FIRST_LEVEL ||
		!FindHeigherLevel(
			&actor,
			actor.position().gridNo(),
			actor.position().direction(),
			&direction) ||
		direction < 0 ||
		direction >= NUM_WORLD_DIRECTIONS)
	{
		DebugAttackBusy(
			String(
				"Soldier %d tried to climb where no roof is.\n",
				actor.identity().id()));
		rejectAiTraversal(actor);
		return false;
	}

	if (!EnoughPoints(
			&actor,
			GetAPsToClimbRoof(&actor, FALSE),
			0,
			TRUE))
	{
		DebugAttackBusy(
			String(
				"Soldier %d tried to climb without AP.\n",
				actor.identity().id()));
		rejectAiTraversal(actor);
		return false;
	}

	std::int32_t destination = NOWHERE;
	if (!resolveTraversalGrid(
			actor,
			static_cast<std::uint8_t>(direction),
			destination))
	{
		rejectAiTraversal(actor);
		return false;
	}
	if (destinationIsOccupied(
			actor,
			destination,
			SECOND_LEVEL))
	{
		DebugAttackBusy(
			String(
				"Soldier %d tried to climb up on someone.\n",
				actor.identity().id()));
		rejectAiTraversal(actor);
		return false;
	}

	if (actor.roster().team() == gbPlayerNum)
		SetUIBusy(actor.identity().id());

	actor.position().temporaryGrid() = destination;
	actor.animationIntent().pendingDirection() = direction;
	if (!IsAnimationValidForBodyType(
			&actor,
			CLIMBUPROOF))
	{
		actor.SetSoldierHeight(50.0f);
		TeleportSoldier(&actor, destination, TRUE);
		EndAIGuysTurn(&actor);
	}
	else
	{
		actor.EVENT_InitNewSoldierAnim(
			CLIMBUPROOF,
			0,
			FALSE);
	}

	if (DoesMercHaveDisability(
			&actor,
			AFRAID_OF_HEIGHTS))
	{
		if (!actor.dialogue().hasSaid(
				SOLDIER_QUOTE_SAID_PERSONALITY))
		{
			HandleMoraleEvent(
				&actor,
				MORALE_FEAR_OF_HEIGHTS,
				actor.deployment().sectorX(),
				actor.deployment().sectorY(),
				actor.deployment().sectorZ());
			TacticalCharacterDialogue(
				&actor,
				QUOTE_PERSONALITY_TRAIT);
			actor.dialogue().markSaid(
				SOLDIER_QUOTE_SAID_PERSONALITY);
			if (gGameExternalOptions.fDynamicOpinions)
			{
				HandleDynamicOpinionChange(
					&actor,
					OPINIONEVENT_ANNOYINGDISABILITY,
					TRUE,
					TRUE);
			}
		}
		else
		{
			actor.dialogue().clearSaid(
				SOLDIER_QUOTE_SAID_PERSONALITY);
		}
	}

	cancelMedicalServices(actor);
	return true;
}

bool TacticalActorTraversal::beginRoofDescent(
	TacticalActor& actor)
{
	if (!hasLiveTraversalContext(actor))
		return false;

	std::int8_t direction = 0;
	if (actor.position().level() != SECOND_LEVEL ||
		!FindLowerLevel(
			&actor,
			actor.position().gridNo(),
			actor.position().direction(),
			&direction) ||
		direction < 0 ||
		direction >= NUM_WORLD_DIRECTIONS)
	{
		DebugAttackBusy(
			String(
				"Soldier %d tried to climb down where no roof is.\n",
				actor.identity().id()));
		rejectAiTraversal(actor);
		return false;
	}

	if (!EnoughPoints(
			&actor,
			GetAPsToClimbRoof(&actor, TRUE),
			0,
			TRUE))
	{
		DebugAttackBusy(
			String(
				"Soldier %d tried to climb down without AP.\n",
				actor.identity().id()));
		rejectAiTraversal(actor);
		return false;
	}

	std::int32_t destination = NOWHERE;
	if (!resolveTraversalGrid(
			actor,
			static_cast<std::uint8_t>(direction),
			destination))
	{
		rejectAiTraversal(actor);
		return false;
	}
	if (destinationIsOccupied(
			actor,
			destination,
			FIRST_LEVEL))
	{
		DebugAttackBusy(
			String(
				"Soldier %d tried to climb down on someone.\n",
				actor.identity().id()));
		rejectAiTraversal(actor);
		return false;
	}

	if (actor.roster().team() == gbPlayerNum)
		SetUIBusy(actor.identity().id());

	actor.position().temporaryGrid() = destination;
	direction = gTwoCDirection[direction];
	actor.animationIntent().pendingDirection() = direction;
	if (!IsAnimationValidForBodyType(
			&actor,
			JUMPDOWNWALL))
	{
		actor.SetSoldierHeight(0.0f);
		TeleportSoldier(&actor, destination, TRUE);
		EndAIGuysTurn(&actor);
	}
	else
	{
		actor.EVENT_InitNewSoldierAnim(
			JUMPDOWNWALL,
			0,
			FALSE);
	}

	cancelMedicalServices(actor);
	return true;
}

bool TacticalActorTraversal::beginFenceJump(
	TacticalActor& actor)
{
	if (!hasLiveTraversalContext(actor))
		return false;

	std::int8_t direction = 0;
	if (!traversalDirection(actor, direction) ||
		!FindFenceJumpDirection(
			&actor,
			actor.position().gridNo(),
			direction,
			&direction) ||
		direction < 0 ||
		direction >= NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	std::int32_t destination = NOWHERE;
	if (!resolveTraversalGrid(
			actor,
			static_cast<std::uint8_t>(direction),
			destination))
	{
		return false;
	}
	const std::int32_t beyondFence =
		NewGridNo(
			destination,
			static_cast<std::uint16_t>(
				DirectionInc(direction)));
	if (TileIsOutOfBounds(beyondFence) ||
		beyondFence == destination)
	{
		return false;
	}

	actor.position().temporaryGrid() = beyondFence;
	actor.animationActivity().turningCostWaived() = TRUE;
	EVENT_InternalSetSoldierDesiredDirection(
		&actor,
		direction,
		FALSE,
		actor.animationPlayback().state());
	actor.animationActivity().turningUntilDone() = TRUE;
	actor.animationActivity().turningFromProneMode() =
		TURNING_FROM_PRONE_OFF;
	actor.animationIntent().pendingAnimation() = HOPFENCE;
	return true;
}

bool TacticalActorTraversal::beginWallClimb(
	TacticalActor& actor)
{
	if (!hasLiveTraversalContext(actor) ||
		actor.position().level() != FIRST_LEVEL)
	{
		return false;
	}

	std::int8_t direction = 0;
	if (!FindWallJumpDirection(
			&actor,
			actor.position().gridNo(),
			actor.position().direction(),
			&direction) ||
		direction < 0 ||
		direction >= NUM_WORLD_DIRECTIONS ||
		!EnoughPoints(
			&actor,
			GetAPsToClimbRoof(&actor, FALSE),
			0,
			TRUE))
	{
		return false;
	}

	std::int32_t destination = NOWHERE;
	if (!resolveTraversalGrid(
			actor,
			static_cast<std::uint8_t>(direction),
			destination) ||
		destinationIsOccupied(
			actor,
			destination,
			SECOND_LEVEL))
	{
		return false;
	}

	if (actor.roster().team() == gbPlayerNum)
		SetUIBusy(actor.identity().id());

	actor.position().temporaryGrid() = destination;
	actor.animationIntent().pendingDirection() = direction;
	actor.EVENT_InitNewSoldierAnim(
		JUMPUPWALL,
		0,
		FALSE);
	cancelMedicalServices(actor);
	return true;
}

bool TacticalActorTraversal::beginWindowJump(
	TacticalActor& actor)
{
	if (!hasLiveTraversalContext(actor) ||
		actor.position().level() != FIRST_LEVEL ||
		(actor.position().direction() != NORTH &&
		 actor.position().direction() != EAST &&
		 actor.position().direction() != SOUTH &&
		 actor.position().direction() != WEST))
	{
		return false;
	}

	std::int8_t direction = 0;
	if (!traversalDirection(actor, direction) ||
		!FindWindowJumpDirection(
			&actor,
			actor.position().gridNo(),
			direction,
			&direction) ||
		direction < 0 ||
		direction >= NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	std::int32_t destination = NOWHERE;
	if (!resolveTraversalGrid(
			actor,
			static_cast<std::uint8_t>(direction),
			destination))
	{
		return false;
	}

	actor.position().temporaryGrid() = destination;
	actor.animationActivity().turningCostWaived() = TRUE;
	EVENT_InternalSetSoldierDesiredDirection(
		&actor,
		direction,
		FALSE,
		actor.animationPlayback().state());
	actor.animationActivity().turningUntilDone() = TRUE;
	if (!IsAnimationValidForBodyType(
			&actor,
			JUMPWINDOWS))
	{
		TeleportSoldier(&actor, destination, TRUE);
	}
	else
	{
		actor.animationActivity().turningFromProneMode() =
			TURNING_FROM_PRONE_OFF;
		actor.animationIntent().pendingAnimation() =
			JUMPWINDOWS;
	}

	if (!gGameExternalOptions.fCanJumpThroughClosedWindows)
		return true;

	std::int32_t windowGrid = actor.position().gridNo();
	if (actor.position().direction() == NORTH ||
		actor.position().direction() == WEST)
	{
		windowGrid = NewGridNo(
			actor.position().gridNo(),
			static_cast<std::uint16_t>(
				DirectionInc(
					actor.position().direction())));
	}
	if (TileIsOutOfBounds(windowGrid) ||
		!IsJumpableWindowPresentAtGridNo(
			windowGrid,
			actor.position().direction(),
			TRUE) ||
		IsJumpableWindowPresentAtGridNo(
			windowGrid,
			actor.position().direction(),
			FALSE))
	{
		return true;
	}

	STRUCTURE* const structure =
		FindStructure(
			windowGrid,
			STRUCTURE_WALLNWINDOW);
	if (!structure ||
		(structure->fFlags & STRUCTURE_OPEN))
	{
		return true;
	}

	WindowHit(
		windowGrid,
		structure->usStructureID,
		actor.position().direction() == SOUTH ||
			actor.position().direction() == EAST,
		TRUE);
	actor.SoldierTakeDamage(
		0,
		static_cast<INT16>(2 + Random(4)),
		1000,
		TAKE_DAMAGE_ELECTRICITY,
		NOBODY,
		windowGrid,
		0,
		TRUE);
	return true;
}
