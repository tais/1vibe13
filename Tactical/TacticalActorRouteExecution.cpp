#include "TacticalActorRouteExecution.h"

#include "Animation Control.h"
#include "Dialogue Control.h"
#include "Event Pump.h"
#include "Handle UI.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Overhead.h"
#include "PATHAI.H"
#include "Soldier Ani.h"
#include "Soldier Control.h"
#include "Soldier Functions.h"
#include "Soldier Profile.h"
#include "TacticalActorAiBehavior.h"
#include "TacticalActorCombatReactions.h"
#include "TacticalActorConditions.h"
#include "TacticalActorMedicalServices.h"
#include "TacticalActorMedicalSession.h"
#include "TacticalActorMobility.h"
#include "TacticalActorRecovery.h"
#include "TacticalActorWorldPlacement.h"
#include "TacticalWorldAdapter.h"
#include "Vehicles.h"
#include "connect.h"
#include "opplist.h"
#include "worlddef.h"
#include "worldman.h"
#include "soldier tile.h"

#include <cstdint>
#include <cstring>

extern BOOLEAN gfGetNewPathThroughPeople;

void HandleVehicleMovementSound(TacticalActor* actor, BOOLEAN enabled);
UINT16 PickSoldierReadyAnimation(
	TacticalActor* actor,
	BOOLEAN endReady,
	BOOLEAN alternateWeaponHolding);

namespace
{
bool hasLiveRouteContext(const TacticalActor& actor) noexcept
{
	return IsJa2TacticalWorldLoaded() &&
		actor.roster().active() &&
		actor.roster().inSector() &&
		actor.identity().id().i < TOTAL_SOLDIERS &&
		actor.identity().bodyType() < TOTALBODYTYPES &&
		!TileIsOutOfBounds(actor.position().gridNo()) &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL &&
		actor.position().direction() < NUM_WORLD_DIRECTIONS &&
		actor.animationPlayback().state() < NUMANIMATIONSTATES;
}

bool isActiveEpc(const TacticalActor& actor) noexcept
{
	const auto profile = actor.identity().profile();
	return profile != NO_PROFILE &&
		profile < NUM_PROFILES &&
		(gMercProfiles[profile].ubMiscFlags &
		 PROFILE_MISC_FLAG_EPCACTIVE) != 0;
}

bool hasValidVehicleContext(const TacticalActor& actor) noexcept
{
	if (!(actor.status().flags() & SOLDIER_VEHICLE))
		return true;

	const auto vehicleId =
		actor.vehicleState().tacticalVehicleId();
	return pVehicleList != nullptr &&
		vehicleId >= 0 &&
		vehicleId < ubNumberOfVehicles &&
		pVehicleList[vehicleId].fValid != FALSE;
}

bool isValidPathOrigin(
	TacticalActorRouteExecution::PathOrigin origin) noexcept
{
	switch (origin)
	{
	case TacticalActorRouteExecution::PathOrigin::System:
	case TacticalActorRouteExecution::PathOrigin::PlayerUi:
	case TacticalActorRouteExecution::PathOrigin::ContinueMovement:
	case TacticalActorRouteExecution::PathOrigin::TeamAwareUi:
		return true;
	}
	return false;
}

bool continuesFromExistingDestination(
	const TacticalActor& actor,
	TacticalActorRouteExecution::PathOrigin origin) noexcept
{
	if (origin ==
		TacticalActorRouteExecution::PathOrigin::ContinueMovement)
	{
		return true;
	}

	const bool fromPlayerUi =
		origin == TacticalActorRouteExecution::PathOrigin::PlayerUi ||
		(origin == TacticalActorRouteExecution::PathOrigin::TeamAwareUi &&
		 actor.roster().team() == gbPlayerNum);
	return !IsJa2TacticalCombatActive() &&
		fromPlayerUi &&
		(gAnimControl[actor.animationPlayback().state()].uiFlags &
		 ANIM_MOVING) != 0;
}

void setOutOfActionPointsUnchecked(
	TacticalActor& actor,
	BOOLEAN stopped)
{
	if (actor.identity().bodyType() == CROW)
		return;

	if ((actor.status().flags() & SOLDIER_VEHICLE) && stopped)
		HandleVehicleMovementSound(&actor, FALSE);

	if (is_networked && actor.identity().id() < 120)
	{
		EV_S_STOP_MERC stopEvent;
		stopEvent.sGridNo = actor.position().gridNo();
		stopEvent.ubDirection = actor.position().direction();
		stopEvent.usSoldierID = actor.identity().id();
		stopEvent.fset = stopped;
		stopEvent.sXPos = actor.position().worldXInt();
		stopEvent.sYPos = actor.position().worldYInt();

		// Remote copies apply the halt locally but never rebroadcast it.
		if (is_client &&
			!(!is_server && actor.identity().id() >= 20))
		{
			send_stop(&stopEvent);
		}
	}

	actor.movement().setOutOfActionPoints(stopped != FALSE);
	if (!stopped)
		actor.movement().stopReason() = REASON_STOPPED_NO_APS;
}

void settleIntoStationaryStanceUnchecked(TacticalActor& actor)
{
	if (actor.identity().bodyType() == QUEENMONSTER &&
		(actor.awareness().opponentCount() > 0 ||
		 actor.roster().team() == gbPlayerNum))
	{
		actor.EVENT_InitNewSoldierAnim(QUEEN_READY, 0, TRUE);
		return;
	}

	if (TacticalActorMobility::inDeepWater(actor))
	{
		actor.EVENT_InitNewSoldierAnim(DEEP_WATER_TRED, 0, FALSE);
		return;
	}

	if (TacticalActorMedicalSession::resumeProvidingAnimation(actor))
		return;

	switch (gAnimControl[actor.animationPlayback().state()].ubEndHeight)
	{
	case ANIM_STAND:
		if (actor.status().flags() & SOLDIER_COWERING)
			actor.EVENT_InitNewSoldierAnim(START_COWER, 0, FALSE);
		else if (actor.animationPlayback().state() == WALKING_WEAPON_RDY ||
			actor.animationPlayback().state() == AIM_RIFLE_STAND)
			actor.EVENT_InitNewSoldierAnim(AIM_RIFLE_STAND, 0, FALSE);
		else if (actor.animationPlayback().state() == WALKING_DUAL_RDY ||
			actor.animationPlayback().state() == AIM_DUAL_STAND)
			actor.EVENT_InitNewSoldierAnim(AIM_DUAL_STAND, 0, FALSE);
		else
			actor.EVENT_InitNewSoldierAnim(STANDING, 0, FALSE);
		break;

	case ANIM_CROUCH:
		if (actor.status().flags() & SOLDIER_COWERING)
			actor.EVENT_InitNewSoldierAnim(COWERING, 0, FALSE);
		else if (actor.animationPlayback().state() ==
				 CROUCHEDMOVE_RIFLE_READY ||
			actor.animationPlayback().state() ==
				 CROUCHEDMOVE_PISTOL_READY)
			actor.EVENT_InitNewSoldierAnim(AIM_RIFLE_CROUCH, 0, FALSE);
		else if (actor.animationPlayback().state() ==
			CROUCHEDMOVE_DUAL_READY)
			actor.EVENT_InitNewSoldierAnim(AIM_DUAL_CROUCH, 0, FALSE);
		else
			actor.EVENT_InitNewSoldierAnim(CROUCHING, 0, FALSE);
		break;

	case ANIM_PRONE:
		actor.EVENT_InitNewSoldierAnim(PRONE, 0, FALSE);
		break;
	}
}

void stopAtUnchecked(
	TacticalActor& actor,
	INT32 gridNo,
	INT8 direction)
{
	INT16 worldX = 0;
	INT16 worldY = 0;
	ConvertGridNoToCenterCellXY(gridNo, &worldX, &worldY);

	if (!actor.movement().delayed())
	{
		actor.animationIntent().clearPendingAnimations();
		actor.animationIntent().clearPendingDirection();
		actor.pendingAction().clearAction();
	}

	actor.schedule().cancelDoorContinuation();
	actor.animationActivity().turningFromProneMode() = 0;
	actor.pathing().pathIndex() = actor.pathing().pathSize() = 0;
	actor.movement().clearDelay();
	actor.movement().setReverse(false);

	(void)TacticalActorWorldPlacement::setPosition(
		actor,
		static_cast<FLOAT>(worldX),
		static_cast<FLOAT>(worldY));
	actor.pathing().destinationX() =
		static_cast<INT16>(actor.position().worldX());
	actor.pathing().destinationY() =
		static_cast<INT16>(actor.position().worldY());
	actor.EVENT_SetSoldierDirection(direction);

	if (gAnimControl[actor.animationPlayback().state()].uiFlags & ANIM_MOVING)
		settleIntoStationaryStanceUnchecked(actor);

	if (actor.animationActivity().turningToShoot())
	{
		actor.animationActivity().turningToShoot() = FALSE;
		DebugMsg(
			TOPIC_JA2,
			DBG_LEVEL_3,
			String("@@@@@@@ Reducing attacker busy count..., ending fire because saw something"));
		DebugAttackBusy(
			"@@@@@@@ Reducing attacker busy count..., ending fire because saw something\n");
		FreeUpAttacker();
	}

	if (actor.position().gridNo() == actor.pathing().finalDestinationGrid())
		actor.movement().clearMoveSpeedOverride();

	UnSetUIBusy(actor.identity().id());
	UnMarkMovementReserved(&actor);
}

bool requestPathUnchecked(
	TacticalActor& actor,
	INT32 destinationGrid,
	UINT16 movementAnimation,
	TacticalActorRouteExecution::PathOrigin origin,
	BOOLEAN forceRestart)
{
	INT32 destination;
	INT32 newGridNo;
	BOOLEAN continueMovement;
	UINT32 distance;
	UINT16 animationState;
	UINT16 moveAnimationState = movementAnimation;
	INT32 actorGridNo;
	UINT16 pathingData[MAX_PATH_LIST_SIZE];
	BOOLEAN advancePath = TRUE;
	UINT8 pathFlags = 0;

	if (actor.collapseState().tactical() &&
		actor.vitals().breath() >= OKBREATH)
	{
		(void)TacticalActorRecovery::beginGetUp(actor);
		if (!actor.collapseState().tactical())
			return false;
	}

	BOOLEAN fromUi = static_cast<BOOLEAN>(origin);
	if (origin ==
		TacticalActorRouteExecution::PathOrigin::TeamAwareUi)
	{
		fromUi = actor.roster().team() == gbPlayerNum ? TRUE : FALSE;
	}

	if (isActiveEpc(actor) && fromUi &&
		(actor.status().flags() & SOLDIER_COWERING))
	{
		(void)TacticalActorCombatReactions::setCowering(actor, false);
		moveAnimationState = WALKING;
	}

	if (fromUi && actor.identity().bodyType() <= REGFEMALE &&
		TacticalActorConditions::isCowering(actor))
	{
		TacticalActorAiBehavior::stopCowering(actor);
	}

	if (moveAnimationState == RUNNING && actor.movement().reverse())
		moveAnimationState = WALKING;

	actor.movement().clearContinuedPath();
	if (actor.movement().delayed())
	{
		pathFlags = actor.movement().delayedFlags() &
			DELAYED_MOVEMENT_FLAG_PATH_THROUGH_PEOPLE
			? PATH_THROUGH_PEOPLE
			: PATH_IGNORE_PERSON_AT_DEST;
		actor.movement().clearDelay();
	}

	if (gfGetNewPathThroughPeople)
		pathFlags = PATH_THROUGH_PEOPLE;

	if ((!(IsJa2TacticalCombatActive()) &&
		 (gAnimControl[actor.animationPlayback().state()].uiFlags &
			 ANIM_MOVING) &&
		 fromUi == TRUE) ||
		origin ==
			TacticalActorRouteExecution::PathOrigin::ContinueMovement)
	{
		if (actor.collapseState().tactical())
			return false;

		actorGridNo = actor.position().gridNo();
		actor.position().gridNo() = actor.pathing().destinationGrid();
		distance = FindBestPath(
			&actor,
			destinationGrid,
			actor.position().level(),
			actor.movement().mode(),
			COPYROUTE,
			pathFlags);
		if (!distance)
		{
			actor.position().gridNo() = actorGridNo;
			return false;
		}

		actor.position().gridNo() = actorGridNo;
		actor.pathing().finalDestinationGrid() = destinationGrid;
		if (advancePath)
		{
			std::memcpy(
				pathingData,
				actor.pathing().path(),
				sizeof(pathingData));
			std::memcpy(
				&actor.pathing().path()[1],
				pathingData,
				sizeof(pathingData) - sizeof(UINT16));
			if (actor.pathing().pathSize() != MAX_PATH_LIST_SIZE)
				++actor.pathing().pathSize();
		}

		moveAnimationState = actor.movement().mode();
		if (TacticalActorMobility::inDeepWater(actor))
			moveAnimationState = DEEP_WATER_SWIM;

		if (moveAnimationState != actor.animationPlayback().state())
		{
			actor.movement().requestGridUpdateSuppression();
			actor.EVENT_InitNewSoldierAnim(
				moveAnimationState,
				0,
				FALSE);
			if (is_server ||
				(is_client && actor.identity().id() < 20))
			{
				send_path(
					&actor,
					destinationGrid,
					moveAnimationState,
					0,
					FALSE);
			}
			return true;
		}

		if (is_server || (is_client && actor.identity().id() < 20))
		{
			send_path(
				&actor,
				destinationGrid,
				actor.animationPlayback().state(),
				255,
				FALSE);
		}
		return true;
	}

	destination = FindBestPath(
		&actor,
		destinationGrid,
		actor.position().level(),
		movementAnimation,
		COPYROUTE,
		pathFlags);
	continueMovement = destination != 0;
	if (!continueMovement)
		return false;

	DebugMsg(
		TOPIC_JA2,
		DBG_LEVEL_0,
		String("Soldier %d: Get new path", actor.identity().id()));
	actor.pathing().finalDestinationGrid() = destinationGrid;
	actor.movement().clearPastDestination();

	newGridNo = NewGridNo(
		actor.position().gridNo(),
		DirectionInc(static_cast<UINT8>(
			actor.pathing().path()[actor.pathing().pathIndex()])));
	(void)newGridNo;

	if (TacticalActorMobility::inDeepWater(actor))
		moveAnimationState = DEEP_WATER_SWIM;
	else if (TacticalActorMobility::inWater(actor))
		moveAnimationState = WALKING;

	if ((gAnimControl[actor.animationPlayback().state()].uiFlags &
		 (ANIM_FIREREADY | ANIM_FIRE)) &&
		(moveAnimationState == WALKING ||
		 moveAnimationState == SIDE_STEP) &&
		!TacticalActorMobility::inWater(actor))
	{
		animationState = INVALID_ANIMATION;
	}
	else
	{
		animationState = PickSoldierReadyAnimation(&actor, TRUE, FALSE);
	}

	if (animationState != INVALID_ANIMATION &&
		gAnimControl[actor.animationPlayback().state()].ubEndHeight ==
			ANIM_STAND)
	{
		actor.EVENT_InitNewSoldierAnim(animationState, 0, FALSE);
		actor.animationIntent().pendingAnimation() = moveAnimationState;
		if (is_server || (is_client && actor.identity().id() < 20))
		{
			send_path(
				&actor,
				destinationGrid,
				animationState,
				0,
				FALSE);
		}
	}
	else
	{
		actor.EVENT_InitNewSoldierAnim(
			moveAnimationState,
			0,
			forceRestart);
		if (is_server || (is_client && actor.identity().id() < 20))
		{
			send_path(
				&actor,
				destinationGrid,
				movementAnimation,
				0,
				forceRestart);
		}
	}
	return true;
}

void haltForSightingUnchecked(
	TacticalActor& actor,
	BOOLEAN sightingEnemy)
{
	DebugMsg(
		TOPIC_JA2,
		DBG_LEVEL_3,
		String("TacticalActorRouteExecution::haltForSighting"));

	EV_S_STOP_MERC stopEvent;
	stopEvent.sGridNo = actor.position().gridNo();
	stopEvent.ubDirection = actor.position().direction();
	stopEvent.usSoldierID = actor.identity().id();
	stopEvent.fset = TRUE;
	stopEvent.sXPos = actor.position().worldXInt();
	stopEvent.sYPos = actor.position().worldYInt();

	if (is_networked && actor.identity().id() >= 120)
		return;
	if (is_client &&
		!(!is_server && actor.identity().id() >= 20))
	{
		send_stop(&stopEvent);
	}

	if (gAnimControl[actor.animationPlayback().state()].uiFlags &
		ANIM_SPECIALMOVE)
	{
		return;
	}

	if (actor.pendingItem().hasObject() && sightingEnemy)
	{
		THROW_PARAMS* throwParameters =
			actor.pendingItem().throwParameters();
		if (throwParameters != nullptr &&
			throwParameters->ubActionCode == THROW_ARM_ITEM)
		{
			if (!actor.inventory()[HANDPOS].exists() &&
				!PlaceObject(
					&actor,
					HANDPOS,
					actor.pendingItem().object()))
			{
				AutoPlaceObject(
					&actor,
					actor.pendingItem().object(),
					FALSE);
			}
		}
		else
		{
			AutoPlaceObject(
				&actor,
				actor.pendingItem().object(),
				FALSE);
		}

		actor.pendingItem().clearThrowTransaction();
		actor.animationIntent().clearPendingAnimations();
		DebugMsg(
			TOPIC_JA2,
			DBG_LEVEL_3,
			String("@@@@@@@ Reducing attacker busy count..., ending throw because saw something"));
		DebugAttackBusy(
			"@@@@@@@ Reducing attacker busy count..., ending throw because saw something\n");
		FreeUpAttacker();
		settleIntoStationaryStanceUnchecked(actor);
		DirtyMercPanelInterface(&actor, DIRTYLEVEL2);
	}

	if (actor.animationIntent().pendingAnimation() == THROW_KNIFE ||
		actor.animationIntent().pendingAnimation() == THROW_KNIFE_SP_BM)
	{
		DebugMsg(
			TOPIC_JA2,
			DBG_LEVEL_3,
			String("@@@@@@@ Reducing attacker busy count..., ending throw knife because saw something"));
		DebugAttackBusy(
			"@@@@@@@ Reducing attacker busy count..., ending throw knife because saw something");
		FreeUpAttacker();
		settleIntoStationaryStanceUnchecked(actor);
		DirtyMercPanelInterface(&actor, DIRTYLEVEL2);
	}

	if (!IsJa2TacticalCombatActive())
	{
		stopAtUnchecked(
			actor,
			actor.position().gridNo(),
			actor.position().direction());
	}
	else
	{
		if (is_networked && actor.roster().team() >= LAN_TEAM_ONE &&
			(gAnimControl[actor.animationPlayback().state()].uiFlags &
			 ANIM_MOVING))
		{
			stopAtUnchecked(
				actor,
				actor.position().gridNo(),
				actor.position().direction());
		}

		setOutOfActionPointsUnchecked(actor, TRUE);
		actor.movement().stopReason() = REASON_STOPPED_SIGHT;

		if (actor.animationActivity().turningToShoot() && sightingEnemy)
		{
			actor.animationActivity().turningToShoot() = FALSE;
			actor.targeting().retainLastTargetFromTurn() = TRUE;
			DebugMsg(
				TOPIC_JA2,
				DBG_LEVEL_3,
				String("@@@@@@@ Reducing attacker busy count..., ending fire because saw something"));
			DebugAttackBusy(
				"@@@@@@@ Reducing attacker busy count..., ending fire because saw something\n");
			FreeUpAttacker();
		}

		if (sightingEnemy)
		{
			if (actor.pendingAction().active() &&
				actor.position().gridNo() ==
					actor.pathing().finalDestinationGrid())
			{
				actor.pendingAction().clearAction();
			}
			actor.animationIntent().clearPendingAnimations();
			actor.animationIntent().clearContinuation();
		}

		if (!actor.animationActivity().turningToShoot())
			actor.animationActivity().turningFromProneMode() = FALSE;
	}

	if (sightingEnemy ||
		(!actor.pendingItem().hasObject() &&
		 !actor.animationActivity().turningToShoot()))
	{
		UnSetUIBusy(actor.identity().id());
	}
	actor.animationIntent().clearTurningFromUi();
	UnSetEngagedInConvFromPCAction(&actor);
}
}

bool TacticalActorRouteExecution::setOutOfActionPoints(
	TacticalActor& actor,
	bool stopped)
{
	if (!hasLiveRouteContext(actor) ||
		!hasValidVehicleContext(actor))
		return false;

	setOutOfActionPointsUnchecked(actor, stopped ? TRUE : FALSE);
	return true;
}

bool TacticalActorRouteExecution::requestPath(
	TacticalActor& actor,
	std::int32_t destinationGrid,
	std::uint16_t movementAnimation,
	PathOrigin origin,
	bool forceRestart)
{
	if (!hasLiveRouteContext(actor) ||
		TileIsOutOfBounds(destinationGrid) ||
		movementAnimation >= NUMANIMATIONSTATES ||
		!isValidPathOrigin(origin) ||
		actor.pathing().pathIndex() >= MAX_PATH_LIST_SIZE ||
		actor.pathing().pathSize() > MAX_PATH_LIST_SIZE)
	{
		return false;
	}
	if (continuesFromExistingDestination(actor, origin) &&
		(TileIsOutOfBounds(actor.pathing().destinationGrid()) ||
		 actor.movement().mode() >= NUMANIMATIONSTATES))
	{
		return false;
	}

	return requestPathUnchecked(
		actor,
		static_cast<INT32>(destinationGrid),
		static_cast<UINT16>(movementAnimation),
		origin,
		forceRestart ? TRUE : FALSE);
}

bool TacticalActorRouteExecution::stop(TacticalActor& actor)
{
	if (!hasLiveRouteContext(actor))
		return false;

	TacticalActorMedicalServices::cancelReceiving(actor);
	TacticalActorMedicalServices::cancelProviding(actor);
	if (!(gAnimControl[actor.animationPlayback().state()].uiFlags &
		ANIM_STATIONARY))
	{
		stopAtUnchecked(
			actor,
			actor.position().gridNo(),
			actor.position().direction());
	}
	actor.pathing().finalDestinationGrid() = actor.position().gridNo();
	return true;
}

bool TacticalActorRouteExecution::settleIntoStationaryStance(
	TacticalActor& actor)
{
	if (!hasLiveRouteContext(actor))
		return false;

	settleIntoStationaryStanceUnchecked(actor);
	return true;
}

bool TacticalActorRouteExecution::haltForSighting(
	TacticalActor& actor,
	bool sightingEnemy)
{
	if (!hasLiveRouteContext(actor) ||
		!hasValidVehicleContext(actor) ||
		HANDPOS >= actor.inventory().size())
		return false;

	haltForSightingUnchecked(actor, sightingEnemy ? TRUE : FALSE);
	return true;
}

bool TacticalActorRouteExecution::stopAt(
	TacticalActor& actor,
	std::int32_t gridNo,
	std::int8_t direction)
{
	if (!hasLiveRouteContext(actor) ||
		TileIsOutOfBounds(gridNo) ||
		direction < 0 || direction >= NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	stopAtUnchecked(
		actor,
		static_cast<INT32>(gridNo),
		static_cast<INT8>(direction));
	return true;
}
