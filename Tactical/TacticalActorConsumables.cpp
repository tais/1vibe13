#include "TacticalActorConsumables.h"

#include "Drugs And Alcohol.h"
#include "TacticalActor.h"
#include "TacticalActorModifiers.h"
#include "TacticalWorldAdapter.h"

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
