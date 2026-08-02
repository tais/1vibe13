#include "TacticalActorConditionPresentation.h"

#include "Cheats.h"
#include "Disease.h"
#include "Food.h"
#include "GameSettings.h"
#include "TacticalActor.h"
#include "Soldier Profile Constants.h"
#include "Soldier Profile.h"
#include "TacticalActorAssignments.h"
#include "TacticalActorDisease.h"
#include "TacticalActorStateFlags.h"
#include "Text.h"

#include <cstddef>
#include <cwchar>

namespace
{
bool hasPresentableProfile(const TacticalActor& actor)
{
	const auto profile = actor.identity().profile();
	return !(actor.status().flags() & SOLDIER_VEHICLE) &&
		profile != NO_PROFILE &&
		profile < NUM_PROFILES;
}

template <std::size_t Size>
void appendFoodModifiers(
	const FoodMoraleMod& modifiers,
	wchar_t* destination,
	wchar_t (&scratch)[Size])
{
	if (modifiers.bMoraleModifier)
	{
		swprintf(
			scratch,
			szFoodText[2],
			modifiers.bMoraleModifier > 0 ? L"+" : L"",
			modifiers.bMoraleModifier);
		wcscat(destination, scratch);
	}

	if (modifiers.bSleepModifier)
	{
		swprintf(
			scratch,
			szFoodText[3],
			modifiers.bSleepModifier > 0 ? L"+" : L"",
			modifiers.bSleepModifier);
		wcscat(destination, scratch);
	}

	if (modifiers.bBreathRegenModifier)
	{
		swprintf(
			scratch,
			szFoodText[4],
			modifiers.bBreathRegenModifier > 0 ? L"+" : L"",
			modifiers.bBreathRegenModifier);
		wcscat(destination, scratch);
	}

	if (modifiers.bAssignmentEfficiencyModifier)
	{
		swprintf(
			scratch,
			szFoodText[5],
			modifiers.bAssignmentEfficiencyModifier > 0 ? L"+" : L"",
			modifiers.bAssignmentEfficiencyModifier);
		wcscat(destination, scratch);
	}

	if (modifiers.ubStatDamageChance)
	{
		swprintf(
			scratch,
			szFoodText[6],
			L"+",
			modifiers.ubStatDamageChance);
		wcscat(destination, scratch);
	}
}
}

void TacticalActorConditionPresentation::appendFoodDescription(
	TacticalActor& actor,
	wchar_t* destination)
{
	if (!destination ||
		!UsingFoodSystem() ||
		!hasPresentableProfile(actor))
	{
		return;
	}

	UINT8 foodSituation = FOOD_NORMAL;
	UINT8 waterSituation = FOOD_NORMAL;
	GetFoodSituation(&actor, &foodSituation, &waterSituation);

	wchar_t scratch[500] = L"";
	swprintf(
		scratch,
		szFoodText[0],
		static_cast<INT32>(
			100 * (actor.condition().drinkLevel() - FOOD_MIN) /
			FOOD_HALF_RANGE));
	wcscat(destination, scratch);
	if (waterSituation != FOOD_NORMAL)
	{
		appendFoodModifiers(
			FoodMoraleMods[waterSituation],
			destination,
			scratch);
	}

	swprintf(
		scratch,
		szFoodText[1],
		static_cast<INT32>(
			100 * (actor.condition().foodLevel() - FOOD_MIN) /
			FOOD_HALF_RANGE));
	wcscat(destination, scratch);
	if (foodSituation != FOOD_NORMAL)
	{
		appendFoodModifiers(
			FoodMoraleMods[foodSituation],
			destination,
			scratch);
	}
}

void TacticalActorConditionPresentation::appendDiseaseDescription(
	TacticalActor& actor,
	wchar_t* destination,
	bool fullDescription)
{
	if (!destination ||
		!gGameExternalOptions.fDisease ||
		!hasPresentableProfile(actor))
	{
		return;
	}

	const bool showExactPoints = DEBUG_CHEAT_LEVEL();
	wchar_t scratch[500] = L"\n  \n";
	wcscat(destination, scratch);

	for (int disease = 0; disease < NUM_DISEASES; ++disease)
	{
		if (actor.condition().hasDiseaseFlag(
				disease,
				TacticalActorDisease::diagnosedFlag))
		{
			if (showExactPoints)
			{
				swprintf(
					scratch,
					L"\n\n%s - %d / %d\n",
					Disease[disease].szFatName,
					actor.condition().diseasePoints(disease),
					Disease[disease].sInfectionPtsFull);
			}
			else
			{
				swprintf(
					scratch,
					L"\n\n%s\n",
					Disease[disease].szFatName);
			}
			wcscat(destination, scratch);

			if (!fullDescription)
				continue;

			swprintf(
				scratch,
				L"%s\n",
				Disease[disease].szDescription);
			wcscat(destination, scratch);

			const FLOAT magnitude =
				TacticalActorDisease::magnitude(actor, disease);
			for (int stat = 0; stat < INFST_MAX; ++stat)
			{
				const INT8 value =
					Disease[disease].sEffStat[stat] * magnitude;
				if (value)
				{
					swprintf(
						scratch,
						szDiseaseText[stat],
						value > 0 ? L"+" : L"",
						value);
					wcscat(destination, scratch);
				}
			}

			const auto appendSignedEffect =
				[&](int textIndex, int value)
				{
					if (!value)
						return;
					swprintf(
						scratch,
						szDiseaseText[textIndex],
						value > 0 ? L"+" : L"",
						value);
					wcscat(destination, scratch);
				};

			appendSignedEffect(
				TEXT_DISEASE_AP,
				static_cast<INT8>(
					Disease[disease].sEffAP * magnitude));

			const UINT8 maximumBreathEffect =
				Disease[disease].usMaxBreath * magnitude;
			if (maximumBreathEffect)
			{
				swprintf(
					scratch,
					szDiseaseText[TEXT_DISEASE_MAXBREATH],
					L"-",
					maximumBreathEffect);
				wcscat(destination, scratch);
			}

			appendSignedEffect(
				TEXT_DISEASE_CARRYSTRENGTH,
				static_cast<INT8>(
					Disease[disease].sEffCarryStrength * magnitude));

			const FLOAT lifeRegenerationEffect =
				static_cast<FLOAT>(Disease[disease].sLifeRegenHundreds) *
				magnitude / 100;
			if (lifeRegenerationEffect)
			{
				swprintf(
					scratch,
					szDiseaseText[TEXT_DISEASE_LIFEREGENHUNDREDS],
					lifeRegenerationEffect > 0 ? L"+" : L"",
					lifeRegenerationEffect);
				wcscat(destination, scratch);
			}

			appendSignedEffect(
				TEXT_DISEASE_NEEDTOSLEEP,
				static_cast<INT8>(
					Disease[disease].sNeedToSleep * magnitude));
			appendSignedEffect(
				TEXT_DISEASE_DRINK,
				static_cast<INT16>(
					Disease[disease].sDrinkModifier * magnitude));
			appendSignedEffect(
				TEXT_DISEASE_FOOD,
				static_cast<INT16>(
					Disease[disease].sFoodModifier * magnitude));

			if ((actor.identity().profile() == BUNS ||
				 actor.identity().profile() == BUNS_CHAOTIC) &&
				(Disease[disease].usDiseaseProperties &
				 DISEASE_PROPERTY_PTSD_BUNS))
			{
				swprintf(
					scratch,
					szDiseaseText[TEXT_DISEASE_PTSD_BUNS_SPECIAL]);
				wcscat(destination, scratch);
			}

			if (!gGameExternalOptions.fDiseaseSevereLimitations)
				continue;

			if (Disease[disease].usDiseaseProperties &
				DISEASE_PROPERTY_ADD_DISABILITY)
			{
				swprintf(
					scratch,
					szDiseaseText[TEXT_DISEASE_ADD_DISABILITY]);
				wcscat(destination, scratch);
			}

			const bool splintApplied =
				actor.condition().hasDiseaseFlag(
					disease,
					TacticalActorDisease::armSplintFlag |
					TacticalActorDisease::legSplintFlag);
			if (Disease[disease].usDiseaseProperties &
				DISEASE_PROPERTY_LIMITED_USE_ARMS)
			{
				swprintf(
					scratch,
					szDiseaseText[
						splintApplied
							? TEXT_DISEASE_LIMITED_ARMS_SPLINT
							: TEXT_DISEASE_LIMITED_ARMS]);
				wcscat(destination, scratch);
			}

			if (Disease[disease].usDiseaseProperties &
				DISEASE_PROPERTY_LIMITED_USE_LEGS)
			{
				swprintf(
					scratch,
					szDiseaseText[
						splintApplied
							? TEXT_DISEASE_LIMITED_LEGS_SPLINT
							: TEXT_DISEASE_LIMITED_LEGS]);
				wcscat(destination, scratch);
			}
		}
		else if (showExactPoints &&
			actor.condition().infected(disease))
		{
			swprintf(
				scratch,
				szDiseaseText[TEXT_DISEASE_UNDIAGNOSED],
				Disease[disease].szFatName,
				actor.condition().diseasePoints(disease),
				Disease[disease].sInfectionPtsFull);
			wcscat(destination, scratch);
		}
	}
}

void TacticalActorConditionPresentation::appendSleepDescription(
	TacticalActor& actor,
	wchar_t* destination)
{
	if (!destination || !hasPresentableProfile(actor))
		return;

	wchar_t scratch[100] = L"\n  \n";
	wcscat(destination, scratch);
	swprintf(
		scratch,
		gpStrategicString[STR_BREATH_REGEN_SLEEP],
		TacticalActorAssignments::sleepBreathRegeneration(actor));
	wcscat(destination, scratch);
}

void TacticalActorConditionPresentation::appendSummary(
	TacticalActor& actor,
	wchar_t* destination,
	bool fullDescription)
{
	appendFoodDescription(actor, destination);
	appendDiseaseDescription(actor, destination, fullDescription);
	appendSleepDescription(actor, destination);
}
