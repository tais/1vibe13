#include "TacticalActorTurnMaintenance.h"

#include "CampaignStats.h"
#include "Explosion Control.h"
#include "GameSettings.h"
#include "Items.h"
#include "Overhead.h"
#include "Soldier Control.h"
#include "SoldierRepository.h"
#include "Soldier macros.h"
#include "TacticalActorCovertOps.h"
#include "TacticalActorModifiers.h"
#include "TacticalActorRadio.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "ai.h"
#include "message.h"
#include "opplist.h"
#include "random.h"

#include <cstdint>

void TacticalActorTurnMaintenance::maintainAtTurnStart(
	TacticalActor& actor)
{
	actor.featureFlags().primaryFlags() &=
		~(SOLDIER_AIRDROP_TURN |
		  SOLDIER_ASSAULT_BONUS |
		  SOLDIER_RAISED_REDALERT);
	actor.featureFlags().secondaryFlags() &=
		~(SOLDIER_CONCEALINSERTION | SOLDIER_SPENT_AP);

	if (actor.featureFlags().secondaryFlags() &
		SOLDIER_CONCEALINSERTION_DISCOVERED)
	{
		actor.featureFlags().secondaryFlags() &=
			~SOLDIER_CONCEALINSERTION_DISCOVERED;
		TacticalActorCovertOps::loseDisguise(actor);
		if (gSkillTraitValues.fCOStripIfUncovered)
			TacticalActorCovertOps::strip(actor);
		HandleInitialRedAlert(ENEMY_TEAM, FALSE);
	}

	if (actor.renderState().muzzleFlashVisible())
		EndMuzzleFlash(&actor);

	if (TacticalActorModifiers::hasBackgroundFlag(
			actor,
			BACKGROUND_EXP_UNDERGROUND) &&
		actor.deployment().sectorZ())
	{
		++actor.condition().extraExperienceLevel();
	}

	if (actor.vitals().health() < OKLIFE)
		TacticalActorRadio::switchOff(actor);

	if (!gSkillTraitValues.fVOJammingBlocksRemoteBombs &&
		gSkillTraitValues.fVOEnemyVOSetsOffRemoteBombs &&
		actor.roster().team() == ENEMY_TEAM &&
		TacticalActorRadio::isJamming(actor) &&
		Chance(5))
	{
		SetOffBombsByFrequency(
			actor.identity().id(),
			1 + Random(8));
	}

	actor.skillState().ageTurnCounters();
	actor.skillState().decrementCooldown(SOLDIER_COOLDOWN_CRYO);
	actor.skillState().decrementCooldown(
		SOLDIER_COOLDOWN_DRUGUSER_COMBAT);

	const bool hasRobotUtilitySlot =
		ROBOT_UTILITY_SLOT < actor.inventory().size();
	const std::uint16_t robotUtilityItem =
		hasRobotUtilitySlot
			? actor.inventory()[ROBOT_UTILITY_SLOT].usItem
			: MAXITEMS;
	TacticalActor* const actorPointer = &actor;
	if (AM_A_ROBOT(actorPointer) &&
		robotUtilityItem < MAXITEMS &&
		ItemHasXRay(robotUtilityItem))
	{
		if (actor.skillState().cooldown(
				SOLDIER_COOLDOWN_ROBOT_XRAY))
		{
			actor.skillState().decrementCooldown(
				SOLDIER_COOLDOWN_ROBOT_XRAY);
		}
		else if (actor.roster().inSector())
		{
			for (std::int32_t index = 0;
				 index < TOTAL_SOLDIERS;
				 ++index)
			{
				TacticalActor* const candidate =
					GetJa2SoldierRepository().resolve(index);
				if (!candidate ||
					!candidate->roster().active() ||
					!candidate->roster().inSector() ||
					candidate->vitals().health() <= 0 ||
					candidate->aiBehavior().neutral() ||
					candidate->roster().side() == 0)
				{
					continue;
				}

				actor.skillState().cooldown(
					SOLDIER_COOLDOWN_ROBOT_XRAY) += 2;
				ActivateXRayDevice(&actor);
				ScreenMsg(
					FONT_MCOLOR_LTYELLOW,
					MSG_INTERFACE,
					szRobotText[ROBOT_TEXT_XRAY_ACTIVATED]);
				break;
			}
		}
	}

	if (actor.featureFlags().primaryFlags() &
		SOLDIER_ENEMY_OBSERVEDTHISTURN)
	{
		actor.featureFlags().primaryFlags() &=
			~SOLDIER_ENEMY_OBSERVEDTHISTURN;
		++actor.skillState().counter(
			SOLDIER_COUNTER_ROLE_OBSERVED);
	}

	if (actor.roster().inSector() &&
		(IsJa2TacticalCombatActive() ||
		 gTacticalStatus.fEnemyInSector))
	{
		if (!(actor.featureFlags().primaryFlags() &
			  SOLDIER_BATTLE_PARTICIPATION))
		{
			actor.featureFlags().primaryFlags() |=
				SOLDIER_BATTLE_PARTICIPATION;
			gCurrentIncident.AddStat(
				&actor,
				CAMPAIGNHISTORY_TYPE_PARTICIPANT);
		}
	}
	else
	{
		actor.featureFlags().primaryFlags() &=
			~SOLDIER_BATTLE_PARTICIPATION;
	}

	if (!gSkillTraitValues.fCOStripIfUncovered)
		TacticalActorCovertOps::disguise(actor);
}
