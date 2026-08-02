#include "TacticalActorSpotting.h"

#include "TacticalActorStateFlags.h"
#include "TacticalActorLongActions.h"
#include "TacticalActorModifiers.h"
#include "TacticalActorVisibility.h"
#include "TacticalWorldAdapter.h"

#include "Animation Control.h"
#include "DynamicDialogue.h"
#include "GameSettings.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "LOS.h"
#include "Overhead.h"
#include "Points.h"
#include "TacticalActor.h"
#include "Soldier Profile.h"
#include "SoldierRepository.h"
#include "SkillCheck.h"
#include "Text.h"
#include "Weapons.h"
#include "message.h"
#include "opplist.h"
#include "worldman.h"

#include <algorithm>
#include <cstdint>

// Flugente: spotter
bool TacticalActorSpotting::isSpotting(TacticalActor& actor)
{
	if (actor.skillState().counter(SOLDIER_COUNTER_SPOTTER) > 0)
	{
		// do we still fulfil the requirements?
		if (canSpot(actor))
		{
			// we are only a spotter if we did this long enough
			return actor.skillState().counter(SOLDIER_COUNTER_SPOTTER) >=
				gGameExternalOptions.usSpotterPreparationTurns;
		}

		// no item -> lose status
		actor.skillState().clearCounter(SOLDIER_COUNTER_SPOTTER);
	}

	return false;
}

bool TacticalActorSpotting::canSpot(
	TacticalActor& actor,
	std::int32_t targetGridNo)
{
	auto* const self = &actor;

	if (actor.vitals().health() < OKLIFE ||
		actor.assignment().isAsleep() ||
		actor.collapseState().tactical() ||
		(actor.featureFlags().primaryFlags() & SOLDIER_POW) ||
		actor.animationPlayback().state() >= NUMANIMATIONSTATES)
	{
		return false;
	}

	// additional checks if we want to know wether we can target a specific location
	if (targetGridNo != NOWHERE)
	{
		if (TileIsOutOfBounds(actor.position().gridNo()) ||
			TileIsOutOfBounds(targetGridNo))
		{
			return false;
		}

		if (PythSpacesAway(actor.position().gridNo(), targetGridNo) >=
			2 * gGameExternalOptions.usSpotterRange)
		{
			UINT16 usSightLimit = TacticalActorVisibility::maximumDistance(actor,
				targetGridNo,
				actor.position().level(),
				CALC_FROM_WANTED_DIR);

			INT32 val = SoldierToVirtualSoldierLineOfSightTest(
				self,
				targetGridNo,
				actor.position().level(),
				gAnimControl[actor.animationPlayback().state()]
					.ubEndHeight,
				FALSE,
				usSightLimit);

			// error if we cannot see the target
			if (!val)
				return false;
		}
	}

	const auto stance =
		gAnimControl[actor.animationPlayback().state()].ubEndHeight;
	const bool hasPrimarySpotterItem =
		HANDPOS < actor.inventory().size() &&
		actor.inventory()[HANDPOS].exists() &&
		actor.inventory()[HANDPOS].usItem < MAXITEMS &&
		GetObjectModifier(
			self,
			&actor.inventory()[HANDPOS],
			stance,
			ITEMMODIFIER_SPOTTER);
	const bool hasSecondarySpotterItem =
		SECONDHANDPOS < actor.inventory().size() &&
		actor.inventory()[SECONDHANDPOS].exists() &&
		actor.inventory()[SECONDHANDPOS].usItem < MAXITEMS &&
		GetObjectModifier(
			self,
			&actor.inventory()[SECONDHANDPOS],
			stance,
			ITEMMODIFIER_SPOTTER);

	// no item -> no spotting
	return hasPrimarySpotterItem || hasSecondarySpotterItem;
}

bool TacticalActorSpotting::startSpotting(
	TacticalActor& actor,
	std::int32_t targetGridNo)
{
	if (!IsJa2TacticalWorldLoaded())
		return false;

	// not possible if already scanning
	if (actor.skillState().counter(SOLDIER_COUNTER_SPOTTER))
	{
		if (New113Message[MSG113_ALREADY_SPOTTING] != nullptr)
		{
			ScreenMsg(
				FONT_MCOLOR_LTYELLOW,
				MSG_INTERFACE,
				New113Message[MSG113_ALREADY_SPOTTING]);
		}
		return false;
	}

	if (!canSpot(actor, targetGridNo))
	{
		if (New113Message[MSG113_CANNOT_SPOT_LOCATION] != nullptr)
		{
			ScreenMsg(
				FONT_MCOLOR_LTYELLOW,
				MSG_INTERFACE,
				New113Message[MSG113_CANNOT_SPOT_LOCATION]);
		}
		return false;
	}

	// deduct APs
	DeductPoints(&actor, APBPConstants[AP_SPOTTER], 0, 0);

	// add to counter
	actor.skillState().counter(SOLDIER_COUNTER_SPOTTER) = 1;

	// stop any multi-turn action
	TacticalActorLongActions::cancel(actor, false);

	return true;
}

// bonus for snipers firing at this location (we get this if there are spotters)
std::uint16_t TacticalActorSpotting::chanceToHitBonus(
	TacticalActor* sniper,
	std::int32_t targetGridNo,
	std::int8_t team)
{
	if (sniper == nullptr ||
		team < 0 ||
		team >= MAXTEAMS ||
		TileIsOutOfBounds(sniper->position().gridNo()) ||
		TileIsOutOfBounds(targetGridNo) ||
		gGameExternalOptions.usSpotterPreparationTurns == 0)
	{
		return 0;
	}

	std::uint64_t bestValue = 0;
	SoldierID cnt = gTacticalStatus.Team[team].bFirstID;
	const SoldierID lastId = gTacticalStatus.Team[team].bLastID;
	for (; cnt <= lastId; ++cnt)
	{
		TacticalActor* const spotter =
			GetJa2SoldierRepository().resolve(
				cnt);
		if (spotter == nullptr ||
			spotter == sniper ||
			!spotter->roster().active() ||
			!spotter->roster().inSector() ||
			spotter->deployment().sectorX() != gWorldSectorX ||
			spotter->deployment().sectorY() != gWorldSectorY ||
			spotter->deployment().sectorZ() != gbWorldSectorZ ||
			TileIsOutOfBounds(spotter->position().gridNo()) ||
			!isSpotting(*spotter) ||
			PythSpacesAway(
				spotter->position().gridNo(),
				sniper->position().gridNo()) >
				gGameExternalOptions.usSpotterRange ||
			PythSpacesAway(
				spotter->position().gridNo(),
				targetGridNo) <
				2 * gGameExternalOptions.usSpotterRange)
		{
			continue;
		}

		const SoldierID targetId =
			WhoIsThere2(targetGridNo, sniper->targeting().level());
		TacticalActor* const target =
			GetJa2SoldierRepository().resolve(targetId);

		const bool targetSeen =
			(target != nullptr &&
			 SoldierToSoldierLineOfSightTest(
				 spotter,
				 target,
				 0,
				 NO_DISTANCE_LIMIT,
				 AIM_SHOT_HEAD) > 0) ||
			SoldierToVirtualSoldierLineOfSightTest(
				spotter,
				targetGridNo,
				sniper->position().level(),
				ANIM_PRONE,
				FALSE,
				NO_DISTANCE_LIMIT) > 0;
		if (!targetSeen)
			continue;

		if (spotter->animationPlayback().state() >=
			NUMANIMATIONSTATES)
		{
			continue;
		}

		const auto stance =
			gAnimControl[spotter->animationPlayback().state()]
				.ubEndHeight;
		std::uint32_t itemBonus = 0;
		if (HANDPOS < spotter->inventory().size() &&
			spotter->inventory()[HANDPOS].exists() &&
			spotter->inventory()[HANDPOS].usItem < MAXITEMS)
		{
			itemBonus += std::clamp<int>(
				GetObjectModifier(
					spotter,
					&spotter->inventory()[HANDPOS],
					stance,
					ITEMMODIFIER_SPOTTER),
				0,
				100);
		}

		if (SECONDHANDPOS < spotter->inventory().size() &&
			spotter->inventory()[SECONDHANDPOS].exists() &&
			spotter->inventory()[SECONDHANDPOS].usItem < MAXITEMS)
		{
			itemBonus += std::clamp<int>(
				GetObjectModifier(
					spotter,
					&spotter->inventory()[SECONDHANDPOS],
					stance,
					ITEMMODIFIER_SPOTTER),
				0,
				100);
		}

		// Base effectiveness is 40% equipment, 30% experience,
		// 20% marksmanship, and 10% leadership.
		UINT32 fatiguedValue =
			2 * itemBonus +
			30 * EffectiveExpLevel(spotter) +
			2 * EffectiveMarksmanship(spotter) +
			EffectiveLeadership(spotter);
		ReducePointsForFatigue(spotter, &fatiguedValue);

		if (spotter->vitals().maximumHealth() <= 0)
			continue;

		std::uint64_t value =
			static_cast<std::uint64_t>(fatiguedValue) *
			std::max<int>(0, spotter->vitals().health()) /
			spotter->vitals().maximumHealth();

		std::int32_t effectiveness = 100;
		const auto spotterProfile = spotter->identity().profile();
		if (spotterProfile < NUM_PROFILES &&
			OKToCheckOpinion(spotterProfile))
		{
			switch (gMercProfiles[spotterProfile].bCharacterTrait)
			{
			case CHAR_TRAIT_SOCIABLE:
				effectiveness += 10;
				break;
			case CHAR_TRAIT_LONER:
				effectiveness -= 10;
				break;
			}
		}

		const auto sniperProfile = sniper->identity().profile();
		if (sniperProfile < NUM_PROFILES &&
			OKToCheckOpinion(sniperProfile))
		{
			switch (gMercProfiles[sniperProfile].bCharacterTrait)
			{
			case CHAR_TRAIT_SOCIABLE:
				effectiveness += 10;
				break;
			case CHAR_TRAIT_LONER:
				effectiveness -= 10;
				break;
			}
		}

		const INT8 relation = std::clamp(
			SoldierRelation(spotter, sniper) +
				SoldierRelation(sniper, spotter),
			2 * HATED_OPINION,
			2 * BUDDY_OPINION);
		effectiveness = std::max<std::int32_t>(
			0,
			effectiveness +
				2 * relation +
				TacticalActorModifiers::backgroundValue(
					*spotter,
					BG_PERC_SPOTTER));

		value = value *
			static_cast<std::uint32_t>(effectiveness) /
			100;

		const auto preparationTurns =
			gGameExternalOptions.usSpotterPreparationTurns;
		const std::uint64_t preparedTurns =
			std::min<std::uint64_t>(
				spotter->skillState().counter(
					SOLDIER_COUNTER_SPOTTER),
				2ULL * preparationTurns);
		value = value * preparedTurns / preparationTurns;
		value = value *
			gGameExternalOptions.usSpotterMaxCTHBoost /
			2000;

		bestValue = std::max(bestValue, value);
	}

	return static_cast<std::uint16_t>(
		std::min<std::uint64_t>(
			bestValue,
			gGameExternalOptions.usSpotterMaxCTHBoost));
}
