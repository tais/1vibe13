#include "TacticalActorInterrupts.h"

#include "GameSettings.h"
#include "Drugs And Alcohol.h"
#include "Isometric Utils.h"
#include "Overhead.h"
#include "Points.h"
#include "Soldier Profile.h"
#include "SkillCheck.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorPredicates.h"
#include "TacticalActorSkills.h"
#include "TacticalActorStateFlags.h"
#include "TacticalActorTurnBudget.h"
#include "TacticalWorldAdapter.h"
#include "TeamTurns.h"
#include "opplist.h"
#include "random.h"

BOOLEAN ResolvePendingInterrupt(
	TacticalActor* pSoldier,
	UINT8 ubInterruptType)
{
	if (!IsJa2TacticalTurnBased() ||
		!IsJa2TacticalCombatActive())
	{
		SetJa2PendingInterrupt(DISABLED_INTERRUPT);
		ClearIntList();
		return FALSE;
	}

	if (pSoldier == nullptr)
		return FALSE;

	if (GetJa2TacticalCurrentTeam() != pSoldier->roster().team())
		return FALSE;

	const UINT8 pendingInterrupt = GetJa2PendingInterrupt();
	if (pendingInterrupt == DISABLED_INTERRUPT ||
		pendingInterrupt == UNTRIGGERED_INTERRUPT)
	{
		return FALSE;
	}

	if (pendingInterrupt != ubInterruptType &&
		ubInterruptType != INSTANT_INTERRUPT)
	{
		return FALSE;
	}

	TacticalActor* pInterrupter;
	UINT8 ubInterruptersFound = 0;
	UINT16 ubaInterruptersList[64];
	UINT16 uCnt = 0;
	UINT16 uiReactionTime;
	INT16 iInjuryPenalty;

	for (uCnt = 0; uCnt < MAX_NUM_SOLDIERS; ++uCnt)
	{
		pInterrupter = GetJa2SoldierRepository().resolve(uCnt);
		if (pInterrupter == nullptr)
			continue;
		if (pInterrupter->vitals().health() < OKLIFE ||
			pInterrupter->collapseState().tactical() ||
			!pInterrupter->roster().active() ||
			!pInterrupter->roster().inSector() ||
			pInterrupter->actionPoints().current() < 4)
		{
			continue;
		}
		if (pInterrupter->vitals().breath() < OKBREATH &&
			pInterrupter->roster().team() != OUR_TEAM)
		{
			continue;
		}
		if (pSoldier->roster().team() == pInterrupter->roster().team() ||
			pSoldier->roster().side() == pInterrupter->roster().side() ||
			CONSIDERED_NEUTRAL(pSoldier, pInterrupter) ||
			CONSIDERED_NEUTRAL(pInterrupter, pSoldier))
		{
			continue;
		}

		if (pInterrupter->awareness().opponentKnowledge()[
				pSoldier->identity().id()] == SEEN_CURRENTLY ||
			(pInterrupter->awareness().opponentKnowledge()[
				 pSoldier->identity().id()] == HEARD_THIS_TURN &&
			 (ubInterruptType == AFTERSHOT_INTERRUPT ||
			  ubInterruptType == AFTERACTION_INTERRUPT ||
			  PythSpacesAway(
				  pInterrupter->position().gridNo(),
				  pSoldier->position().gridNo()) < 3)))
		{
			uiReactionTime =
				gGameExternalOptions.ubBasicReactionTimeLengthIIS;
		}
		else
		{
			continue;
		}

		uiReactionTime *= 10;
		if (pInterrupter->statistics().agility() >= 80)
		{
			uiReactionTime = uiReactionTime *
				(100 - 2 *
					(pInterrupter->statistics().agility() - 80)) /
				100;
		}
		else if (pInterrupter->statistics().agility() > 50)
		{
			uiReactionTime = uiReactionTime *
				(100 + 2 *
					(80 - pInterrupter->statistics().agility())) /
				100;
		}
		else
		{
			uiReactionTime = uiReactionTime * 8 / 5;
		}

		const INT16 turnGrant =
			TacticalActorTurnBudget::calculateTurnGrant(*pInterrupter);
		if (turnGrant <= 0)
			continue;
		uiReactionTime = uiReactionTime *
			(100 +
			 (50 - 50 * pInterrupter->actionPoints().current() /
				turnGrant)) /
			100;

		if (pInterrupter->vitals().health() <
			pInterrupter->vitals().maximumHealth())
		{
			if (pInterrupter->vitals().maximumHealth() <= 0)
				continue;
			iInjuryPenalty =
				200 *
				(pInterrupter->vitals().maximumHealth() -
				 pInterrupter->vitals().health() +
				 (pInterrupter->vitals().maximumHealth() -
				  pInterrupter->vitals().health() -
				  pInterrupter->vitals().bleeding()) /
					 2) /
				pInterrupter->vitals().maximumHealth();
			uiReactionTime = uiReactionTime *
				(100 +
				 iInjuryPenalty *
					 (100 - 3 * EffectiveExpLevel(pInterrupter)) /
					 100) /
				100;
		}

		if (pSoldier->vitals().breath() < 100)
		{
			uiReactionTime = uiReactionTime *
				(100 + (100 - pSoldier->vitals().breath()) / 2) /
				100;
		}
		if (pInterrupter->status().flags() & SOLDIER_GASSED)
		{
			uiReactionTime = uiReactionTime *
				(100 + AIM_PENALTY_GASSED) / 100;
		}
		if (pInterrupter->service().hasProviders())
		{
			uiReactionTime = uiReactionTime *
				(100 + AIM_PENALTY_GETTINGAID) / 100;
		}
		if (pInterrupter->suppression().shock())
		{
			uiReactionTime = uiReactionTime *
				(100 + pInterrupter->suppression().shock() * 20) /
				100;
		}
		if (DoesMercHavePersonality(
				pSoldier,
				CHAR_TRAIT_PHLEGMATIC))
		{
			uiReactionTime = uiReactionTime * 110 / 100;
		}
		uiReactionTime = (uiReactionTime + 5) / 10;

		if (pInterrupter->turnState().interruptCounters()[
				pSoldier->identity().id()] < uiReactionTime)
		{
			continue;
		}

		if (ubInterruptersFound == 0)
			AddToIntList(pSoldier->identity().id(), FALSE, TRUE);
		if (ubInterruptersFound < 64)
		{
			ubaInterruptersList[ubInterruptersFound++] =
				pInterrupter->identity().id();
		}
		AddToIntList(pInterrupter->identity().id(), TRUE, TRUE);
		pInterrupter->turnState().interruptCounters()[
			pSoldier->identity().id()] = 0;
	}

	if (ubInterruptersFound == 0)
	{
		SetJa2PendingInterrupt(DISABLED_INTERRUPT);
		return FALSE;
	}

	if (gGameExternalOptions.fAllowCollectiveInterrupts)
	{
		const UINT8 originalInterrupterCount = ubInterruptersFound;
		for (uCnt = 0; uCnt < originalInterrupterCount; ++uCnt)
		{
			pInterrupter = GetJa2SoldierRepository().resolve(
				ubaInterruptersList[uCnt]);
			if (pInterrupter == nullptr)
				continue;

			for (SoldierID teammateId =
					 gTacticalStatus.Team[
						 pInterrupter->roster().team()].bFirstID;
				 teammateId <=
					 gTacticalStatus.Team[
						 pInterrupter->roster().team()].bLastID;
				 ++teammateId)
			{
				TacticalActor* teammate =
					GetJa2SoldierRepository().resolve(teammateId);
				if (teammate == nullptr ||
					teammate->roster().team() !=
						pInterrupter->roster().team() ||
					teammate->vitals().health() < OKLIFE ||
					teammate->collapseState().tactical() ||
					!teammate->roster().active() ||
					!teammate->roster().inSector() ||
					teammate->actionPoints().current() < 4)
				{
					continue;
				}

				BOOLEAN alreadyIn = FALSE;
				for (UINT8 index = 0;
					 index < ubInterruptersFound;
					 ++index)
				{
					if (teammate->identity().id() ==
						ubaInterruptersList[index])
					{
						alreadyIn = TRUE;
						break;
					}
				}
				if (alreadyIn ||
					PythSpacesAway(
						pInterrupter->position().gridNo(),
						teammate->position().gridNo()) > 5)
				{
					continue;
				}

				UINT16 collectiveChance = 10 *
					((pInterrupter->statistics().leadership() * 3 +
					  EffectiveExpLevel(pInterrupter) * 20 +
					  EffectiveExpLevel(teammate) * 20 +
					  teammate->statistics().agility() * 2 +
					  teammate->statistics().wisdom()) /
					 100);
				if (HAS_SKILL_TRAIT(
						pInterrupter,
						SQUADLEADER_NT) &&
					gGameOptions.fNewTraitSystem)
				{
					collectiveChance +=
						gSkillTraitValues.
							ubSLCollectiveInterruptsBonus *
						NUM_SKILL_TRAITS(
							pInterrupter,
							SQUADLEADER_NT);
				}
				if (!PreChance(collectiveChance))
					continue;

				if (ubInterruptersFound < 64)
				{
					ubaInterruptersList[ubInterruptersFound++] =
						teammate->identity().id();
				}
				AddToIntList(teammate->identity().id(), TRUE, TRUE);
				teammate->turnState().interruptCounters()[
					pSoldier->identity().id()] = 0;
			}
		}
	}

	if (GetJa2TacticalCurrentTeam() != pSoldier->roster().team() &&
		!gTacticalStatus.Team[GetJa2TacticalCurrentTeam()].bHuman &&
		(pSoldier->status().flags() & SOLDIER_UNDERAICONTROL))
	{
		pSoldier->status().flags() &= ~SOLDIER_UNDERAICONTROL;
	}

	SetJa2PendingInterrupt(DISABLED_INTERRUPT);
	DoneAddingToIntList(pSoldier, TRUE, 1);
	return TRUE;
}
