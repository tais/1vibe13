#include "TacticalActorInteractions.h"

#include "Animation Control.h"
#include "Campaign.h"
#include "Dialogue Control.h"
#include "Disease.h"
#include "GameSettings.h"
#include "Handle Items.h"
#include "Handle UI.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Overhead.h"
#include "Points.h"
#include "SkillCheck.h"
#include "Soldier Control.h"
#include "Soldier Functions.h"
#include "Soldier Profile.h"
#include "SoldierRepository.h"
#include "TacticalActorConditions.h"
#include "TacticalActorCovertOps.h"
#include "TacticalActorDisease.h"
#include "TacticalActorExplosives.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "connect.h"
#include "message.h"
#include "opplist.h"
#include "random.h"
#include "worldman.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

extern void ReduceAttachmentsOnGunForNonPlayerChars(
	TacticalActor* actor,
	OBJECTTYPE* object);

namespace
{
bool hasLiveInteractionContext(
	const TacticalActor& actor) noexcept
{
	return IsJa2TacticalWorldLoaded() &&
		actor.roster().active() &&
		actor.roster().inSector() &&
		actor.vitals().health() >= OKLIFE &&
		!TileIsOutOfBounds(actor.position().gridNo()) &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL &&
		actor.animationPlayback().state() <
			NUMANIMATIONSTATES;
}

bool hasValidIntent(
	const TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction) noexcept
{
	return hasLiveInteractionContext(actor) &&
		!TileIsOutOfBounds(targetGrid) &&
		direction < NUM_WORLD_DIRECTIONS;
}

OBJECTTYPE* usableHandItem(TacticalActor& actor) noexcept
{
	if (HANDPOS >= actor.inventory().size())
		return nullptr;

	OBJECTTYPE& object = actor.inventory()[HANDPOS];
	if (!object.exists() ||
		object.usItem >= MAXITEMS ||
		object.objectStack.empty() ||
		!object[0])
	{
		return nullptr;
	}
	return &object;
}

TacticalActor* resolvePersonAt(
	const TacticalActor& actor,
	std::int32_t targetGrid) noexcept
{
	if (TileIsOutOfBounds(targetGrid))
		return nullptr;

	const SoldierID personId =
		WhoIsThere2(
			targetGrid,
			actor.position().level());
	TacticalActor* const person =
		GetJa2SoldierRepository().resolve(personId);
	if (!person ||
		!person->roster().active() ||
		!person->roster().inSector() ||
		person->position().gridNo() != targetGrid ||
		person->position().level() != actor.position().level())
	{
		return nullptr;
	}
	return person;
}

void releaseUi(TacticalActor& actor)
{
	UnSetUIBusy(actor.identity().id());
}

void faceDirection(
	TacticalActor& actor,
	std::uint8_t direction)
{
	if (actor.position().direction() == direction)
		return;

	actor.status().flags() |=
		SOLDIER_LOOK_NEXT_TURNSOLDIER;
	actor.EVENT_SetSoldierDesiredDirection(direction);
	actor.EVENT_SetSoldierDirection(direction);
}

void playNetworkAwareAnimation(
	TacticalActor& actor,
	std::uint16_t animation)
{
	if (!is_networked)
		actor.EVENT_InitNewSoldierAnim(animation, 0, FALSE);
	else
		actor.ChangeSoldierState(animation, 0, 0);
}

bool beginBombFallback(TacticalActor& actor)
{
	OBJECTTYPE* const object = usableHandItem(actor);
	if (!object ||
		Item[object->usItem].usItemClass != IC_BOMB)
	{
		return false;
	}

	return TacticalActorExplosives::beginBombPlacement(actor);
}

void dropCaptiveHandItem(
	TacticalActor& captive,
	std::size_t slot)
{
	if (slot >= captive.inventory().size())
		return;

	OBJECTTYPE& object = captive.inventory()[slot];
	if (!object.exists() ||
		(object.fFlags & OBJECT_UNDROPPABLE))
	{
		return;
	}

	if (object.usItem >= MAXITEMS ||
		object.objectStack.empty())
	{
		DeleteObj(&object);
		return;
	}

	const INT8 visible =
		captive.roster().team() == gbPlayerNum ? 1 : 0;
	const UINT16 itemFlags =
		captive.roster().team() == ENEMY_TEAM
			? 0
			: WORLD_ITEM_DROPPED_FROM_ENEMY;
	if (UsingNewAttachmentSystem())
	{
		ReduceAttachmentsOnGunForNonPlayerChars(
			&captive,
			&object);
	}
	AddItemToPool(
		captive.position().gridNo(),
		&object,
		visible,
		captive.position().level(),
		itemFlags,
		-1);
	DeleteObj(&object);
}
}

bool TacticalActorInteractions::stopChatting(
	TacticalActor& actor)
{
	if (!actor.interaction().chatting())
		return false;

	TacticalActor* const chatPartner =
		GetJa2SoldierRepository().resolve(
			actor.interaction().chatPartner());
	if (chatPartner)
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			TacticalStr[DISTRACT_STOP_STR],
			actor.GetName(),
			chatPartner->GetName());

		if (chatPartner == &actor ||
			chatPartner->interaction().chatPartner() ==
				actor.identity().id())
		{
			chatPartner->interaction().endChat();
		}
	}

	actor.interaction().endChat();
	return true;
}

bool TacticalActorInteractions::beginGivingItem(
	TacticalActor& actor)
{
	TacticalActor* target = nullptr;
	OBJECTTYPE* const pendingObject =
		actor.pendingItem().object();
	const std::int32_t targetGrid =
		actor.pendingAction().secondaryData();
	const std::int32_t rawDirection =
		actor.pendingAction().tertiaryData();
	const std::uint32_t rawTarget =
		actor.pendingAction().quaternaryData();

	if (!hasLiveInteractionContext(actor) ||
		TileIsOutOfBounds(targetGrid) ||
		rawDirection < 0 ||
		rawDirection >= NUM_WORLD_DIRECTIONS ||
		rawTarget >= TOTAL_SOLDIERS ||
		!pendingObject ||
		!pendingObject->exists() ||
		pendingObject->usItem >= MAXITEMS ||
		pendingObject->objectStack.empty() ||
		!VerifyGiveItem(&actor, &target) ||
		!target ||
		!target->roster().active() ||
		!target->roster().inSector() ||
		target->identity().id() != SoldierID{rawTarget} ||
		target->position().gridNo() != targetGrid ||
		target->position().level() != actor.position().level())
	{
		UnSetEngagedInConvFromPCAction(&actor);
		actor.pendingItem().clearObject();
		releaseUi(actor);
		return false;
	}

	const auto direction =
		static_cast<std::uint8_t>(rawDirection);
	actor.pathing().desiredDirection() = direction;
	actor.position().direction() = direction;
	actor.EVENT_InitNewSoldierAnim(GIVE_ITEM, 0, FALSE);
	return true;
}

bool TacticalActorInteractions::handcuffPerson(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	OBJECTTYPE* const handcuffs = usableHandItem(actor);
	TacticalActor* target =
		hasValidIntent(actor, targetGrid, direction)
			? resolvePersonAt(actor, targetGrid)
			: nullptr;
	if (!gGameExternalOptions.fAllowPrisonerSystem ||
		!handcuffs ||
		!HasItemFlag(handcuffs->usItem, HANDCUFFS) ||
		!target ||
		target == &actor ||
		!TacticalActorConditions::canBeCaptured(*target))
	{
		actor.DoMercBattleSound(BATTLE_SOUND_NOTHING);
		releaseUi(actor);
		return false;
	}

	bool success =
		target->assignment().isAsleep() ||
		target->collapseState().tactical();
	if (!success)
	{
		std::uint32_t attackerRating =
			10 * EffectiveExpLevel(&actor) +
			EffectiveStrength(&actor, FALSE) +
			2 * EffectiveDexterity(&actor, FALSE) +
			EffectiveAgility(&actor, FALSE);
		std::uint32_t defenderRating =
			10 * EffectiveExpLevel(target) +
			2 * EffectiveStrength(target, FALSE) +
			2 * EffectiveDexterity(target, FALSE) +
			2 * EffectiveAgility(target, FALSE);

		if (gGameOptions.fNewTraitSystem)
		{
			attackerRating +=
				25 * NUM_SKILL_TRAITS(
					&actor,
					MARTIAL_ARTS_NT) +
				10 * HAS_SKILL_TRAIT(
					&actor,
					MELEE_NT);
			defenderRating +=
				25 * NUM_SKILL_TRAITS(
					target,
					MARTIAL_ARTS_NT) +
				10 * HAS_SKILL_TRAIT(
					target,
					MELEE_NT);
		}
		else
		{
			attackerRating +=
				25 * NUM_SKILL_TRAITS(
					&actor,
					MARTIALARTS_OT) +
				25 * NUM_SKILL_TRAITS(
					&actor,
					HANDTOHAND_OT) +
				10 * HAS_SKILL_TRAIT(
					&actor,
					KNIFING_OT);
			defenderRating +=
				25 * NUM_SKILL_TRAITS(
					target,
					MARTIALARTS_OT) +
				25 * NUM_SKILL_TRAITS(
					target,
					HANDTOHAND_OT) +
				10 * HAS_SKILL_TRAIT(
					target,
					KNIFING_OT);
		}

		ReducePointsForFatigue(&actor, &attackerRating);
		ReducePointsForFatigue(target, &defenderRating);
		success =
			Random(attackerRating) >
			Random(defenderRating) + 100;
	}

	faceDirection(actor, direction);
	actor.EVENT_InitNewSoldierAnim(
		RELOAD_ROBOT,
		0,
		FALSE);

	if (success)
	{
		target->featureFlags().primaryFlags() |= SOLDIER_POW;
		RemoveManAsTarget(target);
		dropCaptiveHandItem(*target, HANDPOS);
		dropCaptiveHandItem(*target, SECONDHANDPOS);

		if (Item[handcuffs->usItem].usItemClass == IC_KIT)
		{
			UseKitPoints(handcuffs, 10, target);
		}
		else
		{
			AutoPlaceObject(target, handcuffs, FALSE);
			DeleteObj(handcuffs);
		}

		StatChange(&actor, STRAMT, 2, TRUE);
		StatChange(&actor, DEXTAMT, 3, TRUE);
		StatChange(&actor, EXPERAMT, 2, TRUE);
		DeductPoints(
			&actor,
			GetAPsToHandcuff(&actor, targetGrid),
			APBPConstants[BP_HANDCUFF],
			AFTERACTION_INTERRUPT);
		CheckForEndOfBattle(FALSE);
		return true;
	}

	StatChange(&actor, DEXTAMT, 2, TRUE);
	DeductPoints(
		&actor,
		GetAPsToHandcuff(&actor, targetGrid),
		APBPConstants[BP_HANDCUFF],
		AFTERACTION_INTERRUPT);
	actor.DoMercBattleSound(BATTLE_SOUND_CURSE1);
	if (actor.featureFlags().primaryFlags() &
		(SOLDIER_COVERT_CIV |
		 SOLDIER_COVERT_SOLDIER))
	{
		TacticalActorCovertOps::loseDisguise(actor);
		if (gSkillTraitValues.fCOStripIfUncovered)
			TacticalActorCovertOps::strip(actor);

		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			szCovertTextStr[STR_COVERT_ACTIVITIES],
			actor.GetName());
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			szCovertTextStr[STR_COVERT_UNCOVERED],
			target->GetName(),
			actor.GetName());
		target->aiBehavior().alertStatus() =
			std::max<INT8>(
				target->aiBehavior().alertStatus(),
				STATUS_RED);
		ProcessImplicationsOfPCAttack(
			&actor,
			&target,
			REASON_NORMAL_ATTACK);
	}
	return true;
}

bool TacticalActorInteractions::applyItemToPerson(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	if (!hasValidIntent(actor, targetGrid, direction))
	{
		releaseUi(actor);
		return false;
	}

	TacticalActor* target =
		resolvePersonAt(actor, targetGrid);
	OBJECTTYPE* const object = usableHandItem(actor);
	if (!target)
	{
		const bool startedBomb = beginBombFallback(actor);
		if (!startedBomb)
			releaseUi(actor);
		return startedBomb;
	}
	if (!object)
	{
		actor.DoMercBattleSound(BATTLE_SOUND_NOTHING);
		releaseUi(actor);
		return false;
	}

	const UINT16 item = object->usItem;
	if (!ItemCanBeAppliedToOthers(item))
	{
		DeductPoints(
			&actor,
			GetAPsToApplyItem(&actor, targetGrid),
			APBPConstants[BP_APPLYITEM],
			AFTERACTION_INTERRUPT);
		actor.DoMercBattleSound(BATTLE_SOUND_CURSE1);
		return true;
	}

	bool success = true;
	if (actor.roster().side() != target->roster().side() &&
		!target->collapseState().tactical())
	{
		const std::uint32_t attackerValue =
			30 +
			4 * EffectiveExpLevel(&actor) +
			EffectiveDexterity(&actor, FALSE) +
			20 * HAS_SKILL_TRAIT(
				&actor,
				STEALTHY_NT);
		const std::int32_t alertModifier =
			100 *
			(static_cast<std::int32_t>(
				 target->aiBehavior().alertStatus()) -
			 1);
		const std::uint32_t defenderValue =
			static_cast<std::uint32_t>(
				std::max<std::int32_t>(
					1,
					100 +
						3 * EffectiveExpLevel(target) +
						alertModifier));
		const std::uint32_t weight =
			std::max<std::uint32_t>(
				1,
				object->GetWeightOfObjectInStack(0));
		success =
			Random(
				std::max<std::uint32_t>(
					1,
					attackerValue / weight)) >
			Random(defenderValue);
		if (!success)
		{
			if (actor.featureFlags().primaryFlags() &
				(SOLDIER_COVERT_CIV |
				 SOLDIER_COVERT_SOLDIER))
			{
				TacticalActorCovertOps::loseDisguise(actor);
				if (gSkillTraitValues.fCOStripIfUncovered)
					TacticalActorCovertOps::strip(actor);

				ScreenMsg(
					FONT_MCOLOR_LTYELLOW,
					MSG_INTERFACE,
					szCovertTextStr[
						STR_COVERT_APPLYITEM_STEAL_FAIL],
					actor.GetName(),
					target->GetName());
				ScreenMsg(
					FONT_MCOLOR_LTYELLOW,
					MSG_INTERFACE,
					szCovertTextStr[
						STR_COVERT_UNCOVERED],
					target->GetName(),
					actor.GetName());
			}

			target->aiBehavior().alertStatus() =
				std::max<INT8>(
					target->aiBehavior().alertStatus(),
					STATUS_RED);
			ProcessImplicationsOfPCAttack(
				&actor,
				&target,
				REASON_NORMAL_ATTACK);
		}
	}

	if (success)
	{
		if (ItemIsGasmask(item))
		{
			const INT8 gasMaskSlot = FindGasMask(target);
			if (gasMaskSlot == NO_SLOT ||
				(gasMaskSlot != HEAD1POS &&
				 gasMaskSlot != HEAD2POS))
			{
				if (!target->inventory()[HEAD1POS].exists())
				success = PlaceObject(target, HEAD1POS, object);
				else if (!target->inventory()[HEAD2POS].exists())
				success = PlaceObject(target, HEAD2POS, object);
				else
				{
					AddItemToPool(
						target->position().gridNo(),
						&target->inventory()[HEAD2POS],
						1,
						target->position().level(),
						0,
						-1);
					success =
						PlaceObject(
							target,
							HEAD2POS,
							object);
				}
			}
			else
				success = false;
		}
		else if (Item[item].usItemClass == IC_BOMB)
			success = AutoPlaceObject(target, object, FALSE);
		else
			success =
				ApplyConsumable(
					target,
					object,
					TRUE,
					TRUE) == TRUE;
	}

	DeductPoints(
		&actor,
		GetAPsToApplyItem(&actor, targetGrid),
		APBPConstants[BP_APPLYITEM],
		AFTERACTION_INTERRUPT);
	if (!success)
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			New113Message[MSG113_COULD_NOT_APPLY],
			actor.GetName(),
			Item[item].szLongItemName,
			target->GetName());
	}
	else
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			New113Message[MSG113_X_APPLY_Y_TO_Z],
			actor.GetName(),
			Item[item].szLongItemName,
			target->GetName());
		actor.DoMercBattleSound(BATTLE_SOUND_COOL1);
	}
	return true;
}

bool TacticalActorInteractions::collectBloodFromPerson(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	if (!hasValidIntent(actor, targetGrid, direction))
	{
		releaseUi(actor);
		return false;
	}

	TacticalActor* const donor =
		resolvePersonAt(actor, targetGrid);
	OBJECTTYPE* const emptyBag = usableHandItem(actor);
	if (!donor)
	{
		const bool startedBomb = beginBombFallback(actor);
		if (!startedBomb)
			releaseUi(actor);
		return startedBomb;
	}
	if (donor == &actor ||
		!emptyBag ||
		!HasItemFlag(emptyBag->usItem, EMPTY_BLOOD_BAG) ||
		!TacticalActorConditions::canDonateBlood(*donor))
	{
		actor.DoMercBattleSound(BATTLE_SOUND_NOTHING);
		releaseUi(actor);
		return false;
	}

	static UINT16 bloodBagItem = 1757;
	if (bloodBagItem >= MAXITEMS ||
		!HasItemFlag(bloodBagItem, BLOOD_BAG))
	{
		UINT16 replacement = NOTHING;
		if (!GetFirstItemWithFlag(&replacement, BLOOD_BAG) ||
			replacement >= MAXITEMS)
		{
			ScreenMsg(
				FONT_MCOLOR_LTYELLOW,
				MSG_INTERFACE,
				L"Error: no blood bag item found in Items.xml!");
			releaseUi(actor);
			return false;
		}
		bloodBagItem = replacement;
	}

	OBJECTTYPE fullBag;
	if (!CreateItem(bloodBagItem, 100, &fullBag) ||
		!fullBag.exists() ||
		fullBag.objectStack.empty() ||
		!fullBag[0])
	{
		releaseUi(actor);
		return false;
	}

	DeleteObj(emptyBag);
	if (donor->condition().infected(0))
		fullBag[0]->data.sObjectFlag |= INFECTED;

	if (!AutoPlaceObject(&actor, &fullBag, FALSE))
	{
		AddItemToPool(
			donor->position().gridNo(),
			&fullBag,
			VISIBLE,
			actor.position().level(),
			0,
			-1);
	}

	const INT32 healableInjury =
		donor->vitals().healableInjury();
	const INT8 bleeding = donor->vitals().bleeding();
	donor->SoldierTakeDamage(
		0,
		TacticalActorConditions::bloodDonationAmount,
		0,
		TAKE_DAMAGE_BLOODLOSS,
		NOBODY,
		targetGrid,
		0,
		TRUE);
	donor->vitals().healableInjury() = healableInjury;
	donor->vitals().bleeding() = bleeding;

	DeductPoints(
		&actor,
		GetAPsToFillBloodbag(&actor, targetGrid),
		APBPConstants[BP_FILLBLOODBAG],
		AFTERACTION_INTERRUPT);
	return true;
}

bool TacticalActorInteractions::applySplintToPerson(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	OBJECTTYPE* const splint = usableHandItem(actor);
	TacticalActor* const target =
		hasValidIntent(actor, targetGrid, direction)
			? resolvePersonAt(actor, targetGrid)
			: nullptr;
	if (!target ||
		target == &actor ||
		!splint ||
		!HasItemFlag(splint->usItem, MEDICAL_SPLINT) ||
		!TacticalActorDisease::canReceiveSplint(*target) ||
		!((gGameOptions.fNewTraitSystem &&
		   NUM_SKILL_TRAITS(&actor, DOCTOR_NT) > 0) ||
		  (!gGameOptions.fNewTraitSystem &&
		   EffectiveMedical(&actor) >= 50)))
	{
		actor.DoMercBattleSound(BATTLE_SOUND_NOTHING);
		releaseUi(actor);
		return false;
	}

	const UINT16 item = splint->usItem;
	DeleteObj(splint);

	bool addToArm = true;
	bool addToLeg = true;
	for (int disease = 0;
			disease < NUM_DISEASES;
			++disease)
	{
		if (!target->condition().infected(disease))
			continue;

		if (addToArm &&
			(Disease[disease].usDiseaseProperties &
			 DISEASE_PROPERTY_LIMITED_USE_ARMS) &&
			!target->condition().hasDiseaseFlag(
				disease,
				TacticalActorDisease::armSplintFlag))
		{
			target->condition().markDiseaseFlag(
				disease,
				TacticalActorDisease::armSplintFlag);
			addToLeg = false;
		}

		if (addToLeg &&
			(Disease[disease].usDiseaseProperties &
			 DISEASE_PROPERTY_LIMITED_USE_LEGS) &&
			!target->condition().hasDiseaseFlag(
				disease,
				TacticalActorDisease::legSplintFlag))
		{
			target->condition().markDiseaseFlag(
				disease,
				TacticalActorDisease::legSplintFlag);
			addToArm = false;
		}
	}

	DeductPoints(
		&actor,
		GetAPsToApplyItem(&actor, targetGrid),
		APBPConstants[BP_APPLYITEM],
		AFTERACTION_INTERRUPT);
	ScreenMsg(
		FONT_MCOLOR_LTYELLOW,
		MSG_INTERFACE,
		New113Message[MSG113_X_APPLY_Y_TO_Z],
		actor.GetName(),
		Item[item].szLongItemName,
		target->GetName());
	playNetworkAwareAnimation(actor, CUTTING_FENCE);
	return true;
}
