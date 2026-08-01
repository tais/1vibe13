#include "TacticalActorOrientation.h"

#include "Animation Control.h"
#include "Animation Data.h"
#include "Event Pump.h"
#include "GameSettings.h"
#include "Handle UI.h"
#include "IMP Skill Trait.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Overhead.h"
#include "Points.h"
#include "Sound Control.h"
#include "Soldier Ani.h"
#include "Soldier Control.h"
#include "Soldier Functions.h"
#include "Soldier macros.h"
#include "Structure Wrap.h"
#include "TacticalActorAnimationFootprint.h"
#include "TacticalActorCombatReactions.h"
#include "TacticalActorEquipment.h"
#include "TacticalActorMobility.h"
#include "TacticalActorRouteExecution.h"
#include "TacticalWorldAdapter.h"
#include "Vehicles.h"
#include "ai.h"
#include "opplist.h"
#include "soldier tile.h"
#include "soundman.h"
#include "worlddef.h"
#include "worldman.h"

#include <cstdint>

extern UINT16 usForceAnimState;
extern UINT8 gubWaitingForAllMercsToExitCode;

void AdjustForFastTurnAnimation(TacticalActor* actor);
void HandleCrowShadowNewDirection(TacticalActor* actor);
void HandleSystemNewAISituation(TacticalActor* actor, BOOLEAN reset);
void PlaySoldierFootstepSound(TacticalActor* actor);
UINT16 PickSoldierReadyAnimation(
	TacticalActor* actor,
	BOOLEAN endReady,
	BOOLEAN alternateWeaponHolding);
UINT16 SelectFireAnimation(TacticalActor* actor, UINT8 height);
void SelectFallAnimation(TacticalActor* actor);
void SetSoldierLocatorOffsets(TacticalActor* actor);

namespace
{
enum ExtendedWorldDirection
{
	EX_NORTH = 0,
	EX_NORTHEAST = 4,
	EX_EAST = 8,
	EX_SOUTHEAST = 12,
	EX_SOUTH = 16,
	EX_SOUTHWEST = 20,
	EX_WEST = 24,
	EX_NORTHWEST = 28
};

constexpr UINT8 extendedDirection[NUM_WORLD_DIRECTIONS] =
{
	EX_NORTH,
	EX_NORTHEAST,
	EX_EAST,
	EX_SOUTHEAST,
	EX_SOUTH,
	EX_SOUTHWEST,
	EX_WEST,
	EX_NORTHWEST
};

bool hasOrientationContext(const TacticalActor& actor) noexcept
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

bool hasVehicleTurnContext(const TacticalActor& actor) noexcept
{
	if (!(actor.status().flags() & SOLDIER_VEHICLE))
		return true;

	const INT32 vehicleId = actor.vehicleState().tacticalVehicleId();
	if (pVehicleList == nullptr ||
		vehicleId < 0 ||
		vehicleId >= ubNumberOfVehicles ||
		pVehicleList[vehicleId].fValid == FALSE ||
		pVehicleList[vehicleId].ubVehicleType >= NUM_PROFILES)
	{
		return false;
	}

	const INT32 seatCount =
		gNewVehicle[pVehicleList[vehicleId].ubVehicleType]
			.iNewSeatingCapacities;
	return seatCount >= 0 && seatCount <= 10;
}

bool hasValidVehiclePassengers(const TacticalActor& actor) noexcept
{
	if (!(actor.status().flags() & SOLDIER_VEHICLE))
		return true;

	const INT32 vehicleId = actor.vehicleState().tacticalVehicleId();
	const INT32 seatCount =
		gNewVehicle[pVehicleList[vehicleId].ubVehicleType]
			.iNewSeatingCapacities;
	for (INT32 seat = 0; seat < seatCount; ++seat)
	{
		const TacticalActor* passenger =
			ResolveVehiclePassenger(vehicleId, seat);
		if (passenger != nullptr &&
			(!hasOrientationContext(*passenger) ||
			 !hasVehicleTurnContext(*passenger) ||
			 passenger->pathing().desiredDirection() < 0 ||
			 passenger->pathing().desiredDirection() >=
				NUM_WORLD_DIRECTIONS ||
			 passenger->movement().mode() >= NUMANIMATIONSTATES))
		{
			return false;
		}
	}
	return true;
}

bool isStance(std::uint8_t stance) noexcept
{
	return stance == ANIM_STAND ||
		stance == ANIM_CROUCH ||
		stance == ANIM_PRONE;
}

bool isCreatureOrBloodcat(const TacticalActor& actor) noexcept
{
	return (actor.status().flags() & SOLDIER_MONSTER) != 0 ||
		actor.identity().bodyType() == BLOODCAT;
}

INT8 multiTiledTurnDirection(
	TacticalActor& actor,
	INT8 startDirection,
	INT8 desiredDirection)
{
	INT8 turningIncrement =
		static_cast<INT8>(QuickestDirection(
			startDirection,
			desiredDirection));
	const UINT16 animationSurface =
		DetermineSoldierAnimationSurface(
			&actor,
			actor.movement().mode());
	STRUCTURE_FILE_REF* structureFile = GetAnimationStructureRef(
		actor.identity().id(),
		animationSurface,
		actor.movement().mode());
	if (structureFile == nullptr)
		return turningIncrement;

	const UINT16 structureId =
		actor.renderBindings().levelNode() != nullptr &&
		actor.renderBindings().levelNode()->pStructureData != nullptr
			? actor.renderBindings().levelNode()->pStructureData->usStructureID
			: INVALID_STRUCTURE_ID;
	INT8 currentDirection = startDirection;
	for (INT8 attempt = 0; attempt < 2; ++attempt)
	{
		BOOLEAN valid = FALSE;
		while (currentDirection != desiredDirection)
		{
			currentDirection += turningIncrement;
			if (currentDirection < 0)
				currentDirection = MAXDIR - 1;
			else if (currentDirection >= MAXDIR)
				currentDirection = 0;

			valid = OkayToAddStructureToWorld(
				actor.position().gridNo(),
				actor.position().level(),
				&structureFile->pDBStructureRef[
					gOneCDirection[currentDirection]],
				structureId);
			if (!valid)
				break;
		}
		if (currentDirection == desiredDirection && valid)
			return turningIncrement;

		currentDirection = startDirection;
		turningIncrement *= -1;
	}
	return turningIncrement;
}

void setDirectionUnchecked(
	TacticalActor& actor,
	UINT8 direction)
{
	(void)TacticalActorAnimationFootprint::remove(
		actor,
		actor.animationPlayback().state());
	const BOOLEAN changed =
		actor.position().direction() != static_cast<INT8>(direction);
	actor.position().direction() = static_cast<INT8>(direction);
	actor.movement().highResolutionDirection() =
		extendedDirection[actor.position().direction()];
	(void)TacticalActorAnimationFootprint::add(
		actor,
		actor.animationPlayback().state());
	if (!actor.targeting().retainLastTargetFromTurn())
		actor.targeting().lastGridNo() = NOWHERE;
	AdjustForFastTurnAnimation(&actor);
	UpdateMercStructureInfo(&actor);
	(void)TacticalActorAnimationFootprint::remove(
		actor,
		actor.animationPlayback().state());
	HandleCrowShadowNewDirection(&actor);
	SetSoldierLocatorOffsets(&actor);
	if (changed)
		TacticalActorEquipment::refreshFlashlights(actor);
}

void setDesiredDirectionUnchecked(
	TacticalActor& actor,
	UINT8 direction,
	BOOLEAN initialMove,
	UINT16 animationState)
{
	INT16 actionPointCost = 0;
	INT32 breathPointCost = 0;
	if (actor.movement().reverse() &&
		animationState != SIDE_STEP &&
		animationState != ROLL_PRONE_R &&
		animationState != ROLL_PRONE_L &&
		animationState != SIDE_STEP_CROUCH_RIFLE &&
		animationState != SIDE_STEP_CROUCH_PISTOL &&
		animationState != SIDE_STEP_CROUCH_DUAL &&
		animationState != SIDE_STEP_WEAPON_RDY &&
		animationState != SIDE_STEP_DUAL_RDY &&
		animationState != SIDE_STEP_ALTERNATIVE_RDY)
	{
		direction = gOppositeDirection[direction];
	}

	actor.pathing().desiredDirection() = static_cast<INT8>(direction);
	if (actor.movement().outOfActionPoints() &&
		(gAnimControl[animationState].uiFlags & ANIM_MOVING))
	{
		(void)TacticalActorRouteExecution::setOutOfActionPoints(
			actor,
			false);
	}

	if (actor.pathing().desiredDirection() !=
		actor.position().direction())
	{
		if (((gAnimControl[animationState].uiFlags &
			(ANIM_BREATH | ANIM_OK_CHARGE_AP_FOR_TURN |
			 ANIM_FIREREADY | ANIM_TURNING)) ||
			usForceAnimState != INVALID_ANIMATION) &&
			!initialMove &&
			!actor.animationActivity().turningCostWaived())
		{
			const UINT16 costAnimation =
				usForceAnimState != INVALID_ANIMATION
					? usForceAnimState : animationState;
			switch (gAnimControl[costAnimation].ubEndHeight)
			{
			case ANIM_STAND:
				actionPointCost = APBPConstants[AP_LOOK_STANDING];
				break;
			case ANIM_CROUCH:
				actionPointCost = APBPConstants[AP_LOOK_CROUCHED];
				break;
			case ANIM_PRONE:
				actionPointCost = APBPConstants[AP_LOOK_PRONE];
				break;
			}
			if (HAS_SKILL_TRAIT(&actor, MARTIAL_ARTS_NT) &&
				gGameOptions.fNewTraitSystem)
			{
				actionPointCost = max(
					1,
					static_cast<INT16>(actionPointCost *
						(100 - gSkillTraitValues.ubMAApsTurnAroundReduction *
						 NUM_SKILL_TRAITS(&actor, MARTIAL_ARTS_NT)) /
						100.0f + 0.5f));
			}
			breathPointCost = gGameExternalOptions.ubEnergyCostForWeaponWeight
				? actionPointCost * GetBPCostPer10APsForGunHolding(&actor) / 10
				: 0;
			DeductPoints(&actor, actionPointCost, breathPointCost);
			if (usForceAnimState != INVALID_ANIMATION)
				actor.targeting().retainLastTargetFromTurn() = FALSE;
		}

		actor.animationActivity().turningCostWaived() = FALSE;
		if (initialMove &&
			gAnimControl[animationState].ubHeight == ANIM_PRONE &&
			actor.animationActivity().turningFromProneMode() !=
				TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE)
		{
			actor.animationActivity().turningFromProneMode() =
				TURNING_FROM_PRONE_START_UP_FROM_MOVE;
		}
		if ((gAnimControl[animationState].uiFlags & ANIM_STATIONARY ||
			 actor.movement().outOfActionPoints() || initialMove) &&
			gAnimControl[animationState].ubHeight == ANIM_PRONE &&
			!initialMove)
		{
			actor.animationActivity().turningFromProneMode() =
				TURNING_FROM_PRONE_ON;
			SendChangeSoldierStanceEvent(&actor, ANIM_CROUCH);
		}
	}

	actor.movement().highResolutionDesiredDirection() =
		extendedDirection[actor.pathing().desiredDirection()];
	if (actor.pathing().desiredDirection() !=
		actor.position().direction() &&
		(actor.status().flags() & SOLDIER_VEHICLE ||
		 isCreatureOrBloodcat(actor)))
	{
		actor.status().flags() |= SOLDIER_PAUSEANIMOVE;
	}

	if (actor.status().flags() & SOLDIER_VEHICLE)
	{
		actor.animationActivity().turningIncrement() =
			static_cast<INT8>(ExtQuickestDirection(
				actor.movement().highResolutionDirection(),
				actor.movement().highResolutionDesiredDirection()));
	}
	else if (actor.status().flags() & SOLDIER_MULTITILE)
	{
		actor.animationActivity().turningIncrement() =
			multiTiledTurnDirection(
				actor,
				actor.position().direction(),
				actor.pathing().desiredDirection());
	}
	else
	{
		actor.animationActivity().turningIncrement() =
			static_cast<INT8>(QuickestDirection(
				actor.position().direction(),
				actor.pathing().desiredDirection()));
	}
}

void setMovementDestinationUnchecked(
	TacticalActor& actor,
	UINT8 direction,
	BOOLEAN initialMove,
	UINT16 animationState)
{
	const INT32 destinationGrid = NewGridNo(
		actor.position().gridNo(),
		DirectionInc(direction));
	INT16 destinationX = 0;
	INT16 destinationY = 0;
	ConvertGridNoToCenterCellXY(
		destinationGrid,
		&destinationX,
		&destinationY);
	actor.pathing().destinationGrid() = destinationGrid;
	actor.pathing().destinationX() = destinationX;
	actor.pathing().destinationY() = destinationY;
	actor.movement().animationDirection() = static_cast<INT8>(direction);

	if (actor.movement().reverse() &&
		(animationState == SIDE_STEP ||
		 animationState == ROLL_PRONE_R ||
		 animationState == ROLL_PRONE_L ||
		 animationState == SIDE_STEP_CROUCH_RIFLE ||
		 animationState == SIDE_STEP_CROUCH_PISTOL ||
		 animationState == SIDE_STEP_CROUCH_DUAL ||
		 animationState == SIDE_STEP_ALTERNATIVE_RDY ||
		 animationState == SIDE_STEP_WEAPON_RDY ||
		 animationState == SIDE_STEP_DUAL_RDY))
	{
		const UINT8 perpendicularDirection =
			gPurpendicularDirection[
				actor.position().direction()][direction];
		setDirectionUnchecked(actor, perpendicularDirection);
		actor.pathing().desiredDirection() =
			actor.position().direction();
	}
	else if (!(gAnimControl[animationState].uiFlags & ANIM_SPECIALMOVE))
	{
		setDesiredDirectionUnchecked(
			actor,
			direction,
			initialMove,
			animationState);
	}
}
}

bool TacticalActorOrientation::changeStance(
	TacticalActor& actor,
	std::uint8_t desiredStance)
{
	if (!hasOrientationContext(actor) || !isStance(desiredStance))
		return false;
	if (desiredStance ==
		gAnimControl[actor.animationPlayback().state()].ubEndHeight)
	{
		FreeUpNPCFromStanceChange(&actor);
		return true;
	}
	if (!IsValidStance(&actor, desiredStance))
		return false;

	const bool isCivilian = actor.identity().bodyType() >= FATCIV &&
		actor.identity().bodyType() <= KIDCIV;
	UINT16 animationState = INVALID_ANIMATION;
	if (!isCivilian)
	{
		animationState = TacticalActorMobility::transitionStateForStance(
			actor,
			desiredStance);
		if (animationState >= NUMANIMATIONSTATES)
			return false;
	}

	SetUIBusy(actor.identity().id());
	if (isCivilian)
	{
		(void)TacticalActorCombatReactions::setCowering(
			actor,
			desiredStance != ANIM_STAND);
	}
	else
	{
		actor.animationIntent().desiredHeight() = desiredStance;
		actor.EVENT_InitNewSoldierAnim(animationState, 0, FALSE);
	}
	actor.featureFlags().primaryFlags() |= SOLDIER_REDOFLASHLIGHT;
	return true;
}

bool TacticalActorOrientation::setMovementDestination(
	TacticalActor& actor,
	std::uint8_t direction)
{
	return setMovementDestination(
		actor,
		direction,
		false,
		actor.animationPlayback().state());
}

bool TacticalActorOrientation::setMovementDestination(
	TacticalActor& actor,
	std::uint8_t direction,
	bool initialMove,
	std::uint16_t animationState)
{
	if (!hasOrientationContext(actor) ||
		!hasVehicleTurnContext(actor) ||
		direction >= NUM_WORLD_DIRECTIONS ||
		animationState >= NUMANIMATIONSTATES ||
		actor.movement().mode() >= NUMANIMATIONSTATES ||
		(usForceAnimState != INVALID_ANIMATION &&
		 usForceAnimState >= NUMANIMATIONSTATES))
	{
		return false;
	}
	const INT32 destinationGrid = NewGridNo(
		actor.position().gridNo(),
		DirectionInc(direction));
	if (TileIsOutOfBounds(destinationGrid))
		return false;

	setMovementDestinationUnchecked(
		actor,
		direction,
		initialMove ? TRUE : FALSE,
		animationState);
	return true;
}

bool TacticalActorOrientation::setDesiredDirection(
	TacticalActor& actor,
	std::uint8_t direction)
{
	return setDesiredDirection(
		actor,
		direction,
		false,
		actor.animationPlayback().state());
}

bool TacticalActorOrientation::setDesiredDirection(
	TacticalActor& actor,
	std::uint8_t direction,
	bool initialMove,
	std::uint16_t animationState)
{
	if (!hasOrientationContext(actor) ||
		!hasVehicleTurnContext(actor) ||
		direction >= NUM_WORLD_DIRECTIONS ||
		animationState >= NUMANIMATIONSTATES ||
		actor.movement().mode() >= NUMANIMATIONSTATES ||
		(usForceAnimState != INVALID_ANIMATION &&
		 usForceAnimState >= NUMANIMATIONSTATES))
	{
		return false;
	}
	setDesiredDirectionUnchecked(
		actor,
		direction,
		initialMove ? TRUE : FALSE,
		animationState);
	return true;
}

bool TacticalActorOrientation::setDirection(
	TacticalActor& actor,
	std::uint8_t direction)
{
	if (!hasOrientationContext(actor) ||
		direction >= NUM_WORLD_DIRECTIONS)
	{
		return false;
	}
	setDirectionUnchecked(actor, direction);
	return true;
}

bool TacticalActorOrientation::advanceTurn(TacticalActor& actor)
{
	if (!hasOrientationContext(actor) ||
		!hasVehicleTurnContext(actor) ||
		!hasValidVehiclePassengers(actor) ||
		actor.pathing().desiredDirection() < 0 ||
		actor.pathing().desiredDirection() >= NUM_WORLD_DIRECTIONS ||
		actor.movement().mode() >= NUMANIMATIONSTATES ||
		actor.movement().highResolutionDirection() >= 32 ||
		actor.movement().highResolutionDesiredDirection() >= 32 ||
		actor.animationActivity().turningIncrement() < -1 ||
		actor.animationActivity().turningIncrement() > 1 ||
		actor.animationActivity().hitPhase() > 2 ||
		(usForceAnimState != INVALID_ANIMATION &&
		 usForceAnimState >= NUMANIMATIONSTATES) ||
		(actor.animationIntent().pendingStance() != NO_PENDING_STANCE &&
		 !isStance(actor.animationIntent().pendingStance())) ||
		(actor.animationIntent().pendingAnimation() !=
			NO_PENDING_ANIMATION &&
		 actor.animationIntent().pendingAnimation() >=
			NUMANIMATIONSTATES) ||
		(actor.status().flags() & SOLDIER_TURNINGFROMHIT &&
		 actor.animationActivity().hitPhase() == 1 &&
		 actor.animationIntent().pendingAnimation() !=
			FALLFORWARD_ROOF &&
		 actor.animationIntent().pendingAnimation() != FALLOFF &&
		 actor.animationPlayback().state() != FALLFORWARD_ROOF &&
		 actor.animationPlayback().state() != FALLOFF &&
		 actor.pendingAction().primaryData() >=
			NUM_WORLD_DIRECTIONS))
	{
		return false;
	}

	if (actor.status().flags() & SOLDIER_LOOK_NEXT_TURNSOLDIER)
	{
		if ((gAnimControl[actor.animationPlayback().state()].uiFlags &
			 ANIM_STATIONARY) &&
			actor.animationPlayback().state() != CLIMBUPROOF &&
			actor.animationPlayback().state() != CLIMBDOWNROOF &&
			actor.animationPlayback().state() != JUMPUPWALL &&
			actor.animationPlayback().state() != JUMPDOWNWALL)
		{
			HandleSight(&actor, SIGHT_LOOK | SIGHT_RADIO);
		}
		actor.status().flags() &= ~SOLDIER_LOOK_NEXT_TURNSOLDIER;
		HandleSystemNewAISituation(&actor, FALSE);
	}

	if (actor.animationActivity().turningToShoot() &&
		actor.position().direction() ==
			actor.pathing().desiredDirection())
	{
		if (((gAnimControl[actor.animationPlayback().state()].uiFlags &
			  ANIM_FIREREADY) &&
			 actor.animationActivity().turningFromProneMode() ==
				TURNING_FROM_PRONE_OFF) ||
			actor.identity().bodyType() == ROBOTNOWEAPON ||
			ARMED_VEHICLE(&actor))
		{
			actor.EVENT_InitNewSoldierAnim(
				SelectFireAnimation(
					&actor,
					gAnimControl[actor.animationPlayback().state()]
						.ubEndHeight),
				0,
				FALSE);
			actor.animationActivity().turningToShoot() = FALSE;
		}
		else if (actor.animationActivity().turningFromProneMode())
		{
			if (IsValidStance(&actor, ANIM_PRONE))
			{
				const UINT16 trueAnimationState =
					actor.animationPlayback().state();
				actor.animationPlayback().state() = PRONE;
				actor.animationIntent().pendingAnimation() =
					PickSoldierReadyAnimation(
						&actor,
						FALSE,
						FALSE);
				actor.animationPlayback().state() =
					trueAnimationState;
				SendChangeSoldierStanceEvent(&actor, ANIM_PRONE);
			}
			else
			{
				actor.EVENT_InitNewSoldierAnim(
					PickSoldierReadyAnimation(
						&actor,
						FALSE,
						FALSE),
					0,
					FALSE);
			}
			actor.animationActivity().turningFromProneMode() =
				TURNING_FROM_PRONE_OFF;
			return true;
		}
	}

	if (actor.animationActivity().turningToFall() &&
		actor.position().direction() ==
			actor.pathing().desiredDirection())
	{
		SelectFallAnimation(&actor);
		actor.animationActivity().turningToFall() = FALSE;
	}

	if (actor.animationActivity().turningUntilDone() &&
		actor.animationIntent().pendingStance() != NO_PENDING_STANCE &&
		actor.position().direction() ==
			actor.pathing().desiredDirection())
	{
		SendChangeSoldierStanceEvent(
			&actor,
			actor.animationIntent().pendingStance());
		actor.animationIntent().clearPendingStance();
		actor.animationActivity().turningUntilDone() = FALSE;
	}

	if (actor.animationActivity().turningUntilDone() &&
		actor.animationIntent().pendingAnimation() !=
			NO_PENDING_ANIMATION &&
		actor.position().direction() ==
			actor.pathing().desiredDirection())
	{
		const UINT16 pendingAnimation =
			actor.animationIntent().pendingAnimation();
		actor.animationIntent().clearPendingAnimation();
		actor.EVENT_InitNewSoldierAnim(
			pendingAnimation,
			0,
			FALSE);
		actor.animationActivity().turningUntilDone() = FALSE;
	}

	if (actor.position().direction() ==
		actor.pathing().desiredDirection())
	{
		if (ARMED_VEHICLE(&actor) &&
			actor.audio().hasTurningSound())
		{
			SoundStop(actor.audio().turningSoundId());
			actor.audio().clearTurningSound();
			PlaySoldierJA2Sample(
				actor.identity().id(),
				TURRET_STOP,
				RATE_11025,
				SoundVolume(
					HIGHVOLUME,
					actor.position().gridNo()),
				1,
				SoundDir(actor.position().gridNo()),
				TRUE);
		}

		actor.status().flags() &= ~SOLDIER_LOOK_NEXT_TURNSOLDIER;
		actor.targeting().retainLastTargetFromTurn() = FALSE;
		if (actor.animationIntent().turningFromUi() &&
			actor.animationActivity().turningFromProneMode() !=
				TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE &&
			actor.animationActivity().turningFromProneMode() !=
				TURNING_FROM_PRONE_ON)
		{
			UnSetUIBusy(actor.identity().id());
			actor.animationIntent().clearTurningFromUi();
		}
		if (actor.status().flags() & SOLDIER_VEHICLE ||
			isCreatureOrBloodcat(actor))
		{
			actor.status().flags() &= ~SOLDIER_PAUSEANIMOVE;
		}

		FreeUpNPCFromTurning(&actor, LOOK);
		if (actor.animationActivity().turningFromProneMode() ==
			TURNING_FROM_PRONE_ON)
		{
			if (IsValidStance(&actor, ANIM_PRONE) &&
				!actor.pendingItem().hasObject())
			{
				SendChangeSoldierStanceEvent(&actor, ANIM_PRONE);
			}
			actor.animationActivity().turningFromProneMode() =
				TURNING_FROM_PRONE_OFF;
		}

		if (actor.animationActivity().turningFromProneMode() ==
				TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE &&
			actor.animationPlayback().state() != PRONE_UP &&
			actor.animationPlayback().state() != PRONE_DOWN)
		{
			actor.EVENT_InitNewSoldierAnim(
				IsValidStance(&actor, ANIM_PRONE)
					? CRAWLING : actor.movement().mode(),
				0,
				FALSE);
		}

		if (actor.status().flags() & SOLDIER_TURNINGFROMHIT)
		{
			if (actor.animationActivity().hitPhase() == 1)
			{
				if (actor.animationIntent().pendingAnimation() !=
						FALLFORWARD_ROOF &&
					actor.animationIntent().pendingAnimation() !=
						FALLOFF &&
					actor.animationPlayback().state() !=
						FALLFORWARD_ROOF &&
					actor.animationPlayback().state() != FALLOFF)
				{
					setDesiredDirectionUnchecked(
						actor,
						static_cast<UINT8>(
							actor.pendingAction().primaryData()),
						FALSE,
						actor.animationPlayback().state());
					actor.animationActivity().advanceHit();
				}
				else
				{
					actor.status().flags() &=
						~SOLDIER_TURNINGFROMHIT;
					actor.animationActivity().clearHit();
				}
			}
			else if (actor.animationActivity().hitPhase() == 2)
			{
				actor.status().flags() &= ~SOLDIER_TURNINGFROMHIT;
				DebugAttackBusy(
					"Finished turning from hit.  Not Reducing attack busy.\n");
				actor.animationActivity().clearHit();
			}
		}

		if (actor.animationActivity().turningFromProneMode() ==
			TURNING_FROM_PRONE_FOR_PUNCH_OR_STAB)
		{
			actor.animationActivity().turningFromProneMode() =
				TURNING_FROM_PRONE_OFF;
		}
		return true;
	}

	if (actor.movement().outOfActionPoints())
	{
		(void)TacticalActorRouteExecution::setOutOfActionPoints(
			actor,
			false);
	}

	INT16 direction = 0;
	BOOLEAN changeDirection = TRUE;
	if (actor.status().flags() & SOLDIER_VEHICLE)
	{
		changeDirection = FALSE;
		direction = actor.movement().highResolutionDirection() +
			actor.animationActivity().turningIncrement();
		if (direction > 31)
			direction = 0;
		else if (direction < 0)
			direction = 31;
		actor.movement().highResolutionDirection() =
			static_cast<UINT8>(direction);
		for (INT32 index = 0;
			 index < NUM_WORLD_DIRECTIONS;
			 ++index)
		{
			if (direction == extendedDirection[index])
			{
				changeDirection = TRUE;
				direction = static_cast<INT16>(index);
				break;
			}
		}
		if (ARMED_VEHICLE(&actor) &&
			!actor.audio().hasTurningSound())
		{
			actor.audio().startTurningSound(
				PlaySoldierJA2Sample(
					actor.identity().id(),
					TURRET_MOVE,
					RATE_11025,
					SoundVolume(
						HIGHVOLUME,
						actor.position().gridNo()),
					100,
					SoundDir(actor.position().gridNo()),
					TRUE));
		}
	}
	else
	{
		direction = actor.position().direction() +
			actor.animationActivity().turningIncrement();
		if (direction > NORTHWEST)
			direction = NORTH;
		else if (direction < NORTH)
			direction = NORTHWEST;
	}

	if (!changeDirection)
		return true;

	if (actor.status().flags() & SOLDIER_VEHICLE)
	{
		const INT32 vehicleId =
			actor.vehicleState().tacticalVehicleId();
		const INT16 directionChange = QuickestDirection(
			actor.position().direction(),
			actor.pathing().desiredDirection());
		const INT32 seatCount =
			gNewVehicle[pVehicleList[vehicleId].ubVehicleType]
				.iNewSeatingCapacities;
		for (INT32 seat = 0; seat < seatCount; ++seat)
		{
			TacticalActor* passenger =
				ResolveVehiclePassenger(vehicleId, seat);
			if (passenger != nullptr)
			{
				passenger->animationActivity().turningCostWaived() = TRUE;
				setDesiredDirectionUnchecked(
					*passenger,
					static_cast<UINT8>((
						passenger->pathing().desiredDirection() +
							directionChange + NUM_WORLD_DIRECTIONS) %
						NUM_WORLD_DIRECTIONS),
					FALSE,
					passenger->animationPlayback().state());
			}
		}
		UpdateAllVehiclePassengersGridNo(&actor);
	}

	if (OKToAddMercToWorld(&actor, static_cast<INT8>(direction)))
	{
		if (gubWaitingForAllMercsToExitCode !=
				WAIT_FOR_MERCS_TO_WALKOFF_SCREEN &&
			gubWaitingForAllMercsToExitCode !=
				WAIT_FOR_MERCS_TO_WALK_TO_GRIDNO &&
			(gAnimControl[actor.animationPlayback().state()].uiFlags &
			 ANIM_STATIONARY))
		{
			actor.status().flags() |= SOLDIER_LOOK_NEXT_TURNSOLDIER;
		}
		setDirectionUnchecked(
			actor,
			static_cast<UINT8>(direction));
		if (actor.identity().bodyType() != LARVAE_MONSTER &&
			!TacticalActorMobility::inWater(actor) &&
			actor.position().terrainType() != DIRT_ROAD &&
			actor.position().terrainType() != PAVED_ROAD &&
			!(actor.status().flags() &
				(SOLDIER_DRIVER | SOLDIER_PASSENGER)))
		{
			PlaySoldierFootstepSound(&actor);
		}
	}
	else if (actor.animationPlayback().state() == CRAWLING)
	{
		SendChangeSoldierStanceEvent(&actor, ANIM_CROUCH);
		actor.animationActivity().turningFromProneMode() =
			TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE;
	}
	else if (actor.status().flags() & SOLDIER_MULTITILE)
	{
		actor.pathing().desiredDirection() =
			actor.position().direction();
	}
	return true;
}
