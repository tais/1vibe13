#include "TacticalActorConsumables.h"

#include "Drugs And Alcohol.h"
#include "Dialogue Control.h"
#include "Food.h"
#include "GameSettings.h"
#include "Interface.h"
#include "Points.h"
#include "Soldier Background Types.h"
#include "TacticalActor.h"
#include "TacticalActorBattleSounds.h"
#include "TacticalActorModifiers.h"
#include "TacticalActorSkills.h"
#include "TacticalWorldAdapter.h"
#include "faces.h"

#include <algorithm>
#include <cstdint>

namespace TacticalActorConsumables
{
bool autoUseDrug(TacticalActor& actor)
{
	if (!TacticalActorModifiers::hasBackgroundFlag(
			actor,
			BACKGROUND_DRUGUSE))
	{
		return false;
	}

	if (!IsJa2TacticalCombatActive() && !IsJa2TacticalTurnBased())
		return false;

	if (actor.skillState().cooldown(SOLDIER_COOLDOWN_DRUGUSER_COMBAT))
		return false;

	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		OBJECTTYPE& object = actor.inventory()[slot];
		if (!object.exists() ||
			object.usItem >= MAXITEMS ||
			object.objectStack.empty())
		{
			continue;
		}

		const std::uint32_t drugType = Item[object.usItem].drugtype;
		if (drugType == 0 || drugType >= NEW_DRUGS_MAX)
			continue;

		const int portionSize =
			Item[object.usItem].usPortionSize != 0
				? Item[object.usItem].usPortionSize
				: 100;
		const int usable = std::min(
			portionSize,
			static_cast<int>(object[0]->data.objectStatus));
		if (usable <= 0)
			continue;

		for (const DRUG_EFFECT& effect : NewDrug[drugType].drug_effects)
		{
			if (effect.size <= 0)
				continue;

			bool shouldUse = false;
			if (effect.effect == DRUG_EFFECT_HP &&
				actor.vitals().bleeding() > 1)
			{
				const std::int64_t restoredHealth =
					static_cast<std::int64_t>(effect.size) *
					effect.duration *
					usable /
					100;
				shouldUse =
					restoredHealth <
					static_cast<std::int64_t>(
						actor.vitals().bleeding()) *
						2;
			}
			else if (effect.effect == DRUG_EFFECT_BP &&
					 actor.vitals().breath() < 50)
			{
				shouldUse = true;
			}

			if (!shouldUse)
				continue;

			if (ApplyConsumable(&actor, &object, TRUE, FALSE) == TRUE)
			{
				actor.skillState().cooldown(
					SOLDIER_COOLDOWN_DRUGUSER_COMBAT) += 6;
				return true;
			}
		}
	}

	return false;
}
}

BOOLEAN ApplyConsumable(
	TacticalActor* actor,
	OBJECTTYPE* object,
	BOOLEAN force,
	BOOLEAN useActionPoints)
{
	if (actor == nullptr || object == nullptr ||
		!object->exists() || object->usItem >= MAXITEMS ||
		object->objectStack.empty())
	{
		return FALSE;
	}

	if (!(Item[object->usItem].usItemClass & (IC_KIT | IC_MISC)))
		return FALSE;

	BOOLEAN success = FALSE;
	BOOLEAN playSound = FALSE;
	UINT8 portionSize = Item[object->usItem].usPortionSize;
	if (portionSize == 0)
		portionSize = 100;

	UINT16 statusUsed = std::min<UINT16>(
		portionSize,
		(*object)[0]->data.objectStatus);
	if (statusUsed == 0 ||
		(statusUsed == 1 && ItemIsCanteen(object->usItem)))
	{
		return FALSE;
	}

	INT16 actionPointCost = 0;
	if (useActionPoints)
	{
		if (HasItemFlag(object->usItem, CAMO_REMOVAL) &&
			gGameExternalOptions.fCamoRemoving)
		{
			actionPointCost = std::max<INT16>(
				actionPointCost,
				APBPConstants[AP_CAMOFLAGE] / 2);
		}
		if (ItemIsCamoKit(object->usItem))
		{
			actionPointCost = std::max<INT16>(
				actionPointCost,
				APBPConstants[AP_CAMOFLAGE]);
		}
		if (ItemIsCanteen(object->usItem))
		{
			actionPointCost = std::max<INT16>(
				actionPointCost,
				APBPConstants[AP_DRINK]);
		}
		if (object->usItem == JAR_ELIXIR)
		{
			actionPointCost = std::max<INT16>(
				actionPointCost,
				APBPConstants[AP_CAMOFLAGE]);
		}
		if (Item[object->usItem].clothestype)
		{
			const INT16 disguiseCost =
				APBPConstants[AP_DISGUISE] *
				(100 -
				 gSkillTraitValues.sCODisguiseAPReduction *
					 NUM_SKILL_TRAITS(actor, COVERT_NT)) /
				100;
			actionPointCost = std::max(actionPointCost, disguiseCost);
		}
		if (Item[object->usItem].drugtype)
		{
			actionPointCost = std::max<INT16>(
				actionPointCost,
				APBPConstants[AP_DRINK]);
		}
		if (Item[object->usItem].foodtype)
		{
			const UINT8 costType =
				Food[Item[object->usItem].foodtype].bDrinkPoints >
					Food[Item[object->usItem].foodtype].bFoodPoints
				? AP_DRINK
				: AP_EAT;
			actionPointCost = std::max<INT16>(
				actionPointCost,
				APBPConstants[costType]);
		}

		if (!force &&
			!EnoughPoints(actor, actionPointCost, 0, TRUE))
		{
			return 2;
		}
	}

	if (!force)
	{
		if (DoesSoldierRefuseToEat(actor, object))
			return FALSE;
		if (ItemIsCigarette(object->usItem) &&
			TacticalActorModifiers::backgroundValue(
				*actor,
				BG_SMOKERTYPE) == 2)
		{
			TacticalCharacterDialogue(actor, QUOTE_REFUSE_TO_SMOKE);
			actor->morale().morale() = std::max(
				0,
				actor->morale().morale() - 1);
			return FALSE;
		}
	}

	if (ApplyCamo(actor, object->usItem, statusUsed))
	{
		success = TRUE;
		playSound = TRUE;
		if (gGameExternalOptions.fShowCamouflageFaces)
		{
			SetCamoFace(actor);
			DeleteSoldierFace(actor);
			actor->renderBindings().faceIndex() = InitSoldierFace(actor);
		}
	}
	if (ApplyCanteen(actor, object->usItem, statusUsed))
	{
		success = TRUE;
		playSound = FALSE;
	}
	if (ApplyElixir(actor, object->usItem, statusUsed))
	{
		success = TRUE;
		playSound = TRUE;
	}
	if (ApplyClothes(actor, object->usItem, statusUsed))
		success = TRUE;
	if (ApplyFood(actor, object, statusUsed))
	{
		success = TRUE;
		playSound = FALSE;
	}
	if (ApplyDrugs_New(actor, object->usItem, statusUsed))
	{
		success = TRUE;
		if (!ItemIsCigarette(object->usItem))
			playSound = TRUE;
	}

	if (!gGameExternalOptions.fFoodEatingSounds)
		playSound = FALSE;
	if (!success)
		return FALSE;

	AdditionalTacticalCharacterDialogue_CallsLua(
		actor,
		ADE_CONSUMEITEM,
		object->usItem);
	UseKitPoints(object, statusUsed, actor);
	if (useActionPoints)
	{
		DeductPoints(actor, actionPointCost, 0, false);
		fInterfacePanelDirty = DIRTYLEVEL2;
	}
	if (playSound)
	{
		TacticalActorBattleSounds::play(
			*actor,
			BATTLE_SOUND_COOL1);
	}
	return TRUE;
}
