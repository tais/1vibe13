#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorAnimationSelection.h"
#include "TacticalActorRangedActions.h"

#include "TacticalActorOrientation.h"
#include "TacticalActorRouteExecution.h"

#include "Animation Control.h"
#include "Grid Direction.h"
#include "GameSettings.h"
#include "Handle UI.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Overhead.h"
#include "TacticalActor.h"
#include "TacticalActorAnimationState.h"
#include "TacticalActorEvents.h"
#include "TacticalActorInterrupts.h"
#include "TacticalActorStateFlags.h"
#include "Soldier Functions.h"
#include "Sound Control.h"
#include "TacticalActorMedicalServices.h"
#include "TacticalActorWeaponHandling.h"
#include "TacticalWorldAdapter.h"
#include "Weapons.h"
#include "connect.h"
#include "opplist.h"
#include "renderworld.h"
#include "soundman.h"
#include "worlddef.h"
#include "worldman.h"

#include <cstddef>
#include <cstdint>
#include <map>

namespace
{
bool hasValidAnimation(const TacticalActor& actor) noexcept
{
	return actor.animationPlayback().state() <
		NUMANIMATIONSTATES;
}

bool hasValidInventoryItem(
	const TacticalActor& actor,
	std::size_t slot) noexcept
{
	if (slot >= actor.inventory().size())
		return false;

	const OBJECTTYPE& object = actor.inventory()[slot];
	return object.usItem < MAXITEMS;
}

bool hasValidItem(std::uint16_t item) noexcept
{
	return item == NOTHING || item < MAXITEMS;
}

bool hasLiveFireContext(
	const TacticalActor& actor,
	std::int32_t targetGridNo) noexcept
{
	return IsJa2TacticalWorldLoaded() &&
		actor.roster().active() &&
		actor.roster().inSector() &&
		actor.vitals().health() >= OKLIFE &&
		actor.identity().id().i < TOTAL_SOLDIERS &&
		!TileIsOutOfBounds(actor.position().gridNo()) &&
		!TileIsOutOfBounds(targetGridNo) &&
		actor.position().gridNo() != targetGridNo &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL &&
		actor.targeting().level() >= FIRST_LEVEL &&
		actor.targeting().level() <= SECOND_LEVEL &&
		actor.position().direction() <
			NUM_WORLD_DIRECTIONS &&
		hasValidAnimation(actor) &&
		hasValidInventoryItem(actor, HANDPOS);
}

void playWeaponRaisingSound(TacticalActor& actor)
{
	if (actor.awareness().visibility() < 0 ||
		WeaponReady(&actor) ||
		TileIsOutOfBounds(actor.position().gridNo()) ||
		!hasValidInventoryItem(actor, HANDPOS))
	{
		return;
	}

	const OBJECTTYPE& hand = actor.inventory()[HANDPOS];
	if (!hand.exists() ||
		hand.usItem == NOTHING ||
		Item[hand.usItem].usItemClass != IC_GUN ||
		Item[hand.usItem].ubClassIndex >= MAXITEMS)
	{
		return;
	}

	const char* filename = "sounds\\equip\\Draw.ogg";
	switch (Weapon[Item[hand.usItem].ubClassIndex].ubWeaponType)
	{
	case GUN_PISTOL:
		filename = "sounds\\equip\\Draw_Pistol.ogg";
		break;
	case GUN_M_PISTOL:
		filename = "sounds\\equip\\Draw_MP.ogg";
		break;
	case GUN_SMG:
		filename = "sounds\\equip\\Draw_SMG.ogg";
		break;
	case GUN_RIFLE:
		filename = "sounds\\equip\\Draw_Rifle.ogg";
		break;
	case GUN_SN_RIFLE:
		filename = "sounds\\equip\\Draw_Sniper.ogg";
		break;
	case GUN_AS_RIFLE:
		filename = "sounds\\equip\\Draw_AR.ogg";
		break;
	case GUN_LMG:
		filename = "sounds\\equip\\Draw_LMG.ogg";
		break;
	case GUN_SHOTGUN:
		filename = "sounds\\equip\\Draw_Shotgun.ogg";
		break;
	default:
		break;
	}

	if (!FileExists(filename))
		filename = "sounds\\equip\\Draw.ogg";

	if (FileExists(filename))
	{
		PlayJA2SampleFromFile(
			filename,
			RATE_11025,
			SoundVolume(
				HIGHVOLUME,
				actor.position().gridNo()),
			1,
			SoundDir(actor.position().gridNo()));
	}
}

bool isRifle(std::uint16_t item) noexcept
{
	return item != NOTHING &&
		item < MAXITEMS &&
		Item[item].usItemClass == IC_GUN &&
		ItemIsTwoHanded(item) &&
		!ItemIsRocketLauncher(item);
}
}

bool TacticalActorRangedActions::readyFacing(
	TacticalActor& actor,
	std::uint8_t facingDirection,
	bool endReady,
	bool raiseToHipOnly)
{
	if (facingDirection >= NUM_WORLD_DIRECTIONS ||
		actor.position().direction() >= NUM_WORLD_DIRECTIONS ||
		!hasValidAnimation(actor) ||
		!hasValidInventoryItem(actor, HANDPOS))
	{
		return false;
	}

	if (actor.status().flags() & SOLDIER_MONSTER)
	{
		if (!endReady)
			(void)TacticalActorOrientation::setDesiredDirection(actor, facingDirection);
		return false;
	}

	DebugMsg(
		TOPIC_JA2,
		DBG_LEVEL_3,
		String("TacticalActorRangedActions::readyFacing: PickingAnimation"));
	UINT16 animationState =
		TacticalActorAnimationSelection::pickReady(
			actor,
			endReady,
			raiseToHipOnly);
	if (!endReady &&
		actor.position().direction() != facingDirection)
	{
		const UINT16 trueAnimationState =
			actor.animationPlayback().state();
		switch (gAnimControl[trueAnimationState].ubEndHeight)
		{
		case ANIM_STAND:
			actor.animationPlayback().state() = STANDING;
			if (actor.animationActivity().turningCostWaived() &&
				!actor.animationActivity().readyCostWaived() &&
				animationState == INVALID_ANIMATION)
			{
				animationState =
					TacticalActorAnimationSelection::pickReady(
						actor,
						false,
						raiseToHipOnly);
			}
			break;
		case ANIM_CROUCH:
			actor.animationPlayback().state() = CROUCHING;
			if (actor.animationActivity().turningCostWaived() &&
				!actor.animationActivity().readyCostWaived() &&
				animationState == INVALID_ANIMATION)
			{
				animationState =
					TacticalActorAnimationSelection::pickReady(
						actor,
						false,
						false);
			}
			break;
		case ANIM_PRONE:
			animationState = INVALID_ANIMATION;
			break;
		}
		actor.animationPlayback().state() = trueAnimationState;
	}

	bool changedAnimation = false;
	if (animationState != INVALID_ANIMATION)
	{
		if (is_networked)
		{
			TacticalActorAnimationTransitions::changeState(actor,
				animationState,
				0,
				FALSE);
		}
		else
		{
			TacticalActorAnimationTransitions::initializeAnimation(actor,
				animationState,
				0,
				FALSE);
		}
		changedAnimation = true;
	}

	if (!endReady)
	{
		if (animationState == INVALID_ANIMATION)
			animationState = actor.animationPlayback().state();

		const std::uint16_t item =
			actor.inventory()[HANDPOS].usItem;
		if (item < MAXITEMS &&
			(ItemIsRocketLauncher(item) || ItemIsMortar(item)))
		{
			usForceAnimState = actor.animationPlayback().state();
		}
		(void)TacticalActorOrientation::setDesiredDirection(
			actor,
			facingDirection,
			false,
			animationState);
		usForceAnimState = INVALID_ANIMATION;
	}

	if (changedAnimation && !endReady)
		playWeaponRaisingSound(actor);

	return changedAnimation;
}

bool TacticalActorRangedActions::ready(
	TacticalActor& actor)
{
	return readyFacing(
		actor,
		actor.position().direction(),
		false,
		false);
}

bool TacticalActorRangedActions::readyToward(
	TacticalActor& actor,
	std::int16_t targetX,
	std::int16_t targetY,
	bool endReady,
	bool raiseToHipOnly)
{
	if (!hasValidAnimation(actor))
		return false;

	const UINT8 facingDirection = GetDirectionFromXY(
		targetX,
		targetY,
		&actor);
	return readyFacing(
		actor,
		facingDirection,
		endReady,
		raiseToHipOnly);
}

bool TacticalActorRangedActions::beginFire(
	TacticalActor& actor,
	std::int32_t targetGridNo)
{
	if (!hasLiveFireContext(actor, targetGridNo))
		return false;

	std::uint16_t item = actor.inventory()[HANDPOS].usItem;
	const auto weaponMode = actor.attackSelection().weaponMode();
	if (weaponMode == WM_ATTACHED_GL ||
		weaponMode == WM_ATTACHED_GL_BURST ||
		weaponMode == WM_ATTACHED_GL_AUTO)
	{
		item = GetAttachedGrenadeLauncher(
			&actor.inventory()[HANDPOS]);
	}
	if (item >= MAXITEMS)
		return false;

	DebugMsg(
		TOPIC_JA2,
		DBG_LEVEL_3,
		String("TacticalActorRangedActions::beginFire"));
	DebugMsg(
		TOPIC_JA2,
		DBG_LEVEL_3,
		String(
			"!!!!!!! Starting attack, attack count now %d",
			GetJa2PendingTacticalCombatActions()));
	DebugAttackBusy(String(
		"!!!!!!! Starting fire weapon attack, attack count now %d\n",
		GetJa2PendingTacticalCombatActions()));

	actor.targeting().gridNo() = targetGridNo;
	actor.targeting().targetId() = WhoIsThere2(
		targetGridNo,
		actor.targeting().level());

	INT16 targetX;
	INT16 targetY;
	ConvertGridNoToXY(targetGridNo, &targetX, &targetY);

	bool fireImmediately = false;
	if (ItemIsRocketLauncher(item) ||
		ItemIsGrenadeLauncher(item) ||
		ItemIsMortar(item))
	{
		const UINT8 height =
			gAnimControl[actor.animationPlayback().state()]
				.ubEndHeight;
		if (height == ANIM_PRONE ||
			(ItemIsMortar(item) && height == ANIM_STAND))
		{
			SendChangeSoldierStanceEvent(&actor, ANIM_CROUCH);
		}
		fireImmediately = true;
	}

	(void)readyToward(
		actor,
		targetX,
		targetY,
		false,
		TacticalActorWeaponHandling::isValidAlternativeFireMode(
			actor,
			actor.aiPlanning().aimTime(),
			targetGridNo));

	if (UsingImprovedInterruptSystem() &&
		ResolvePendingInterrupt(&actor, BEFORESHOT_INTERRUPT))
	{
		actor.animationIntent().clearFacingAnimation();
		if (actor.roster().team() == gbPlayerNum)
		{
			guiPendingOverrideEvent = LU_BEGINUILOCK;
			HandleTacticalUI();
		}
		return false;
	}

	if (actor.status().flags() & SOLDIER_MONSTER)
	{
		(void)TacticalActorOrientation::setDirection(actor,
			actor.pathing().desiredDirection());
		TacticalActorAnimationTransitions::initializeAnimation(actor,
			TacticalActorAnimationSelection::selectFire(
				actor,
				gAnimControl[actor.animationPlayback().state()]
					.ubEndHeight),
			0,
			FALSE);
	}
	else if (fireImmediately)
	{
		actor.targeting().retainLastTargetFromTurn() = TRUE;
		actor.animationActivity().turningFromProneMode() =
			TURNING_FROM_PRONE_OFF;
		(void)TacticalActorOrientation::setDirection(actor,
			actor.pathing().desiredDirection());
		TacticalActorAnimationTransitions::initializeAnimation(actor,
			TacticalActorAnimationSelection::selectFire(
				actor,
				gAnimControl[actor.animationPlayback().state()]
					.ubEndHeight),
			0,
			FALSE);
	}
	else
	{
		actor.animationActivity().turningToShoot() = TRUE;
		if (actor.roster().team() != gbPlayerNum)
		{
			if (actor.awareness().visibility() != -1)
			{
				LocateSoldier(
					actor.identity().id(),
					DONTSETLOCATOR);
			}
			else if (!GridNoOnScreen(targetGridNo))
			{
				INT16 centerX;
				INT16 centerY;
				ConvertGridNoToCenterCellXY(
					targetGridNo,
					&centerX,
					&centerY);
				SetRenderCenter(centerX, centerY);
				gfPlotNewMovement = TRUE;
			}
		}
	}

	return true;
}

bool TacticalActorRangedActions::refreshAfterHandItemChange(
	TacticalActor& actor,
	std::uint16_t oldItem,
	std::uint16_t newItem)
{
	if (!hasValidItem(oldItem) ||
		!hasValidItem(newItem) ||
		!hasValidAnimation(actor) ||
		!hasValidInventoryItem(actor, HANDPOS) ||
		!hasValidInventoryItem(actor, SECONDHANDPOS))
	{
		return false;
	}

	const std::uint16_t handItem =
		actor.inventory()[HANDPOS].usItem;
	if (Weapon[newItem].ubShotsPerBurst == 0 &&
		!Weapon[handItem].NoSemiAuto)
	{
		actor.fireControl().selectSingleShot();
		actor.attackSelection().weaponMode() = WM_NORMAL;
	}
	else if (Weapon[newItem].NoSemiAuto)
	{
		actor.fireControl().selectAutofire();
		actor.attackSelection().weaponMode() = WM_AUTOFIRE;
	}

	OBJECTTYPE& hand = actor.inventory()[HANDPOS];
	if (HasAttachmentOfClass(&hand, AC_RIFLEGRENADE))
	{
		OBJECTTYPE* const grenadeLauncher =
			FindAttachment_GrenadeLauncher(&hand);
		if (grenadeLauncher &&
			grenadeLauncher->usItem < MAXITEMS &&
			FindLaunchableAttachment(
				&hand,
				grenadeLauncher->usItem))
		{
			actor.attackSelection().weaponMode() =
				WM_ATTACHED_GL;
		}
	}

	if (ItemIsTwoHanded(newItem) &&
		Weapon[newItem].HeavyGun &&
		gGameExternalOptions.ubAllowAlternativeWeaponHolding == 3)
	{
		actor.attackSelection().scopeMode() =
			USE_ALT_WEAPON_HOLD;
	}
	else
	{
		actor.attackSelection().scopeMode() = USE_BEST_SCOPE;
	}

	actor.fireControl().selectBarrelMode(1);
	actor.fireControl().selectBarrelMode(
		GetNextBarrelMode(
			newItem,
			actor.fireControl().barrelMode()));

	if (gAnimControl[actor.animationPlayback().state()].uiFlags &
		ANIM_FIREREADY)
	{
		(void)TacticalActorRouteExecution::settleIntoStationaryStance(actor);
	}

	TacticalActorMedicalServices::cancelProviding(actor);

	const bool oldRifle = isRifle(oldItem);
	const bool newRifle = isRifle(newItem);
	if (newItem != NOTHING &&
		Item[newItem].usItemClass == IC_GUN)
	{
		const OBJECTTYPE& secondHand =
			actor.inventory()[SECONDHANDPOS];
		if ((Item[hand.usItem].usItemClass & IC_WEAPON) &&
			(Item[secondHand.usItem].usItemClass & IC_WEAPON))
		{
			std::map<INT8, OBJECTTYPE*> scopes;
			GetScopeLists(&actor, &hand, scopes);
			for (const auto& [scopeMode, scope] : scopes)
			{
				if (!scope)
					break;
				actor.attackSelection().scopeMode() =
					scopeMode;
			}
		}
	}

	switch (gAnimControl[actor.animationPlayback().state()]
		.ubEndHeight)
	{
	case ANIM_STAND:
		if (oldRifle && !newRifle)
		{
			TacticalActorAnimationTransitions::initializeAnimation(actor,
				LOWER_RIFLE,
				0,
				FALSE);
		}
		else if (!oldRifle && newRifle)
		{
			TacticalActorAnimationTransitions::initializeAnimation(actor,
				RAISE_RIFLE,
				0,
				FALSE);
		}
		else
		{
			SetSoldierAnimationSurface(
				&actor,
				actor.animationPlayback().state());
		}
		break;
	case ANIM_CROUCH:
	case ANIM_PRONE:
		SetSoldierAnimationSurface(
			&actor,
			actor.animationPlayback().state());
		break;
	}

	return true;
}
