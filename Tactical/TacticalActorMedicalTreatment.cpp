#include "TacticalActorMedicalTreatment.h"

#include "TacticalActorBloodState.h"
#include "TacticalActorSkills.h"
#include "TacticalActorStateFlags.h"

#include "Animation Control.h"
#include "Campaign.h"
#include "Campaign Types.h"
#include "CampaignStats.h"
#include "Dialogue Control.h"
#include "DynamicDialogue.h"
#include "Food.h"
#include "Font Control.h"
#include "GameSettings.h"
#include "Interface.h"
#include "Items.h"
#include "Overhead.h"
#include "Points.h"
#include "SkillCheck.h"
#include "TacticalActor.h"
#include "Soldier macros.h"
#include "Soldier Profile.h"
#include "TacticalActorModifiers.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "connect.h"
#include "fresh_header.h"
#include "message.h"
#include "random.h"
#include "rt time defines.h"

#ifdef NETWORKED
#include "NetworkEvent.h"
#endif

#include <algorithm>
#include <cstdint>
#include <limits>

namespace
{
MERCPROFILESTRUCT* persistentProfile(
	TacticalActor& actor) noexcept
{
	const auto profile = actor.identity().profile();
	if (profile == NO_PROFILE || profile >= NUM_PROFILES)
		return nullptr;

	return &gMercProfiles[profile];
}

bool hasUsableKit(const OBJECTTYPE& kit) noexcept
{
	return kit.usItem < MAXITEMS &&
		kit.exists() &&
		!kit.objectStack.empty();
}

std::uint32_t surgeryConsumptionRate() noexcept
{
	return std::max<std::uint32_t>(
		1,
		gSkillTraitValues.usDOSurgeryMedBagConsumption);
}

template <typename Value>
bool restoreAttribute(
	Value& value,
	std::uint16_t amount) noexcept
{
	const int restored =
		std::min(
			100,
			std::max(0, static_cast<int>(value)) +
				static_cast<int>(amount));
	value = static_cast<Value>(restored);
	return restored == 100;
}

bool restoreHealth(
	TacticalActor& actor,
	std::uint16_t amount) noexcept
{
	const int restoredMaximum =
		std::max(
			0,
			static_cast<int>(
				actor.vitals().maximumHealth())) +
		static_cast<int>(amount);
	const int restoredCurrent =
		std::max(
			0,
			static_cast<int>(
				actor.vitals().health())) +
		static_cast<int>(amount);
	const bool capped =
		restoredMaximum >= 100 ||
		restoredCurrent >= 100;
	actor.vitals().maximumHealth() =
		static_cast<INT8>(
			capped ? 100 : restoredMaximum);
	actor.vitals().health() =
		static_cast<INT8>(
			capped ? 100 : restoredCurrent);
	actor.vitals().healableInjury() =
		std::max<INT32>(
			0,
			actor.vitals().healableInjury() -
				static_cast<INT32>(amount) * 100);
	if (capped)
		actor.vitals().healableInjury() = 0;
	return capped;
}
}

std::uint16_t TacticalActorMedicalTreatment::damagedStatCount(
	const TacticalActor& actor) noexcept
{
	std::uint32_t total = 0;
	for (std::uint8_t index = 0;
		 index < NUM_DAMAGABLE_STATS;
		 ++index)
	{
		const auto damage =
			actor.vitals().criticalStatDamage()[index];
		if (damage > 0)
			total += static_cast<std::uint32_t>(damage);
	}

	// Flugente: stats can also be damaged by lack of food
	if (actor.condition().starvationHealthDamage() > 0)
	{
		total += static_cast<std::uint32_t>(
			actor.condition().starvationHealthDamage());
	}
	if (actor.condition().starvationStrengthDamage() > 0)
	{
		total += static_cast<std::uint32_t>(
			actor.condition().starvationStrengthDamage());
	}

	return static_cast<std::uint16_t>(
		std::min<std::uint32_t>(
			total,
			std::numeric_limits<std::uint16_t>::max()));
}

std::uint8_t TacticalActorMedicalTreatment::restoreDamagedStats(
	TacticalActor& actor,
	std::uint16_t usAmountRegainedHundredths)
{
	TacticalActor* const pSoldier = &actor;
	MERCPROFILESTRUCT* const profile =
		persistentProfile(actor);
	UINT16 usStatIncreasement;
	//BOOLEAN fAnyStatToBeRepaired = FALSE;
	UINT8 cnt;
	UINT16 ubAmountRegained;
	STR16 sStat = L"";

	UINT16 bStatsReturned = 0;

	// First determine how much we can return
	ubAmountRegained = std::min<UINT16>(
		std::numeric_limits<UINT8>::max(),
		usAmountRegainedHundredths / 100); //transfer to whole numbers
	usAmountRegainedHundredths %= 100; // keep rest
	if ( usAmountRegainedHundredths > 0 &&
		 ubAmountRegained <
			std::numeric_limits<UINT8>::max() ) // if some reamins, solve it as a chance
	{
		if ( Chance( usAmountRegainedHundredths ) ) // if rolled true, add one point
		{
			ubAmountRegained =
				std::min<UINT16>(
					std::numeric_limits<UINT8>::max(),
					ubAmountRegained + 1);
			usAmountRegainedHundredths = 0;
		}
		else // otherwise ignore the rest
		{
			usAmountRegainedHundredths = 0;
		}
	}
	// return zero if we are not able to heal anything
	if ( ubAmountRegained <= 0 )
		return(0);

	// Second, run through all damagable stats
	for ( cnt = 0; cnt < NUM_DAMAGABLE_STATS; ++cnt )
	{
		// if we have a damaged stat here
		if ( pSoldier->vitals().criticalStatDamage()[cnt] > 0 )
		{
			if ( ubAmountRegained >= pSoldier->vitals().criticalStatDamage()[cnt] )
			{
				// if the amount we can return is bigger than what we need, keep the rest, for other stats
				usStatIncreasement = pSoldier->vitals().criticalStatDamage()[cnt];
				ubAmountRegained = max( 0, (ubAmountRegained - usStatIncreasement) );
				pSoldier->vitals().criticalStatDamage()[cnt] = 0;
			}
			else
			{
				// if not having full amount, heal what we can
				usStatIncreasement = ubAmountRegained;
				ubAmountRegained = 0;
				pSoldier->vitals().criticalStatDamage()[cnt] = max( 0, (pSoldier->vitals().criticalStatDamage()[cnt] - usStatIncreasement) );
			}
			// so we can start regaining the stats
			if ( usStatIncreasement > 0 )
			{
				bStatsReturned += usStatIncreasement; // keep value for feedback

				switch ( cnt ) // look on the stat
				{
					// actually we only test Health, Dexterity, Agility, Strength and Wisdom now,
					// as there are no ways to lost other stats in the current code
				case DAMAGED_STAT_HEALTH:
					sStat = sStatGainStrings[0]; // set string
					if (restoreHealth(
							*pSoldier,
							usStatIncreasement))
					{
						pSoldier->vitals().criticalStatDamage()[cnt] = 0;
					}
					if (profile)
						profile->bLifeMax =
							pSoldier->vitals().maximumHealth();
					break;
				case DAMAGED_STAT_DEXTERITY:
					sStat = sStatGainStrings[2]; // set string
					if (restoreAttribute(
							pSoldier->statistics().dexterity(),
							usStatIncreasement))
					{
						pSoldier->vitals().criticalStatDamage()[cnt] = 0;
					}
					if (profile)
						profile->bDexterity =
							pSoldier->statistics().dexterity();
					break;
				case DAMAGED_STAT_AGILITY:
					sStat = sStatGainStrings[1]; // set string
					if (restoreAttribute(
							pSoldier->statistics().agility(),
							usStatIncreasement))
					{
						pSoldier->vitals().criticalStatDamage()[cnt] = 0;
					}
					if (profile)
						profile->bAgility =
							pSoldier->statistics().agility();
					break;
				case DAMAGED_STAT_STRENGTH:
					sStat = sStatGainStrings[9]; // set string
					if (restoreAttribute(
							pSoldier->statistics().strength(),
							usStatIncreasement))
					{
						pSoldier->vitals().criticalStatDamage()[cnt] = 0;
					}
					if (profile)
						profile->bStrength =
							pSoldier->statistics().strength();
					break;
				case DAMAGED_STAT_WISDOM:
					sStat = sStatGainStrings[3]; // set string
					if (restoreAttribute(
							pSoldier->statistics().wisdom(),
							usStatIncreasement))
					{
						pSoldier->vitals().criticalStatDamage()[cnt] = 0;
					}
					if (profile)
						profile->bWisdom =
							pSoldier->statistics().wisdom();
					break;
				}
				// Throw a message if healed anything
				if ( gSkillTraitValues.fDORepStShouldThrowMessage && pSoldier->roster().team() != ENEMY_TEAM )
				{
					if ( usStatIncreasement == 1 )
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_REGAINED_ONE_POINTS_OF_STAT], pSoldier->GetName( ), sStat );
					else
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_REGAINED_X_POINTS_OF_STATS], pSoldier->GetName( ), usStatIncreasement, sStat );
				}
			}
			//if( pSoldier->vitals().criticalStatDamage()[cnt] > 0 )
			//	fAnyStatToBeRepaired = TRUE;

		}
	}

	// Flugente: Third, heal damage from starvation if possible
	if ( !UsingFoodSystem() || ( ubAmountRegained > 0 && pSoldier->condition().foodLevel() > FoodMoraleMods[FOOD_NORMAL].bThreshold && pSoldier->condition().drinkLevel() > FoodMoraleMods[FOOD_NORMAL].bThreshold) )
	{
		// if we have a damaged stat here
		if ( pSoldier->condition().starvationHealthDamage() > 0 )
		{
			if ( ubAmountRegained >= pSoldier->condition().starvationHealthDamage() )
			{
				// if the amount we can return is bigger than what we need, keep the rest, for other stats
				usStatIncreasement = pSoldier->condition().starvationHealthDamage();
				ubAmountRegained = max( 0, (ubAmountRegained - usStatIncreasement) );
				pSoldier->condition().starvationHealthDamage() = 0;
			}
			else
			{
				// if not having full amount, heal what we can
				usStatIncreasement = ubAmountRegained;
				ubAmountRegained = 0;
				pSoldier->condition().starvationHealthDamage() = max( 0, (pSoldier->condition().starvationHealthDamage() - usStatIncreasement) );
			}

			// so we can start regaining the stats
			if ( usStatIncreasement > 0 )
			{
				bStatsReturned += usStatIncreasement; // keep value for feedback

				sStat = sStatGainStrings[0]; // set string
				if (restoreHealth(
						*pSoldier,
						usStatIncreasement))
				{
					pSoldier->condition().starvationHealthDamage() = 0;
				}
				if (profile)
					profile->bLifeMax =
						pSoldier->vitals().maximumHealth();

				// Throw a message if healed anything
				if ( gSkillTraitValues.fDORepStShouldThrowMessage && pSoldier->roster().team() != ENEMY_TEAM )
				{
					if ( usStatIncreasement == 1 )
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_REGAINED_ONE_POINTS_OF_STAT], pSoldier->GetName( ), sStat );
					else
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_REGAINED_X_POINTS_OF_STATS], pSoldier->GetName( ), usStatIncreasement, sStat );
				}
			}
		}

		// if we have a damaged stat here
		if ( pSoldier->condition().starvationStrengthDamage() > 0 )
		{
			if ( ubAmountRegained >= pSoldier->condition().starvationStrengthDamage() )
			{
				// if the amount we can return is bigger than what we need, keep the rest, for other stats
				usStatIncreasement = pSoldier->condition().starvationStrengthDamage();
				ubAmountRegained = max( 0, (ubAmountRegained - usStatIncreasement) );
				pSoldier->condition().starvationStrengthDamage() = 0;
			}
			else
			{
				// if not having full amount, heal what we can
				usStatIncreasement = ubAmountRegained;
				ubAmountRegained = 0;
				pSoldier->condition().starvationStrengthDamage() = max( 0, (pSoldier->condition().starvationStrengthDamage() - usStatIncreasement) );
			}

			// so we can start regaining the stats
			if ( usStatIncreasement > 0 )
			{
				bStatsReturned += usStatIncreasement; // keep value for feedback

				sStat = sStatGainStrings[9]; // set string
				if (restoreAttribute(
						pSoldier->statistics().strength(),
						usStatIncreasement))
				{
					pSoldier->condition().starvationStrengthDamage() = 0;
				}
				if (profile)
					profile->bStrength =
						pSoldier->statistics().strength();

				// Throw a message if healed anything
				if ( gSkillTraitValues.fDORepStShouldThrowMessage && pSoldier->roster().team() != ENEMY_TEAM )
				{
					if ( usStatIncreasement == 1 )
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_REGAINED_ONE_POINTS_OF_STAT], pSoldier->GetName( ), sStat );
					else
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_REGAINED_X_POINTS_OF_STATS], pSoldier->GetName( ), usStatIncreasement, sStat );
				}
			}
		}
	}

	// Done, return what we healed
	return static_cast<std::uint8_t>(
		std::min<UINT16>(
			bStatsReturned,
			std::numeric_limits<UINT8>::max()));
}

std::uint32_t TacticalActorMedicalTreatment::treatInSector(
	TacticalActor& doctor,
	TacticalActor& patient,
	std::int16_t sKitPts,
	std::int16_t sStatus)
{
	TacticalActor* const pSoldier = &doctor;
	TacticalActor* const pVictim = &patient;
	if (sKitPts <= 0 ||
		sStatus < 0 ||
		pSoldier->inventory().size() <= HANDPOS ||
		!hasUsableKit(
			pSoldier->inventory()[HANDPOS]) ||
		pSoldier->animationPlayback().state() >=
			NUMANIMATIONSTATES ||
		pSoldier->actionPoints().current() < 0 ||
		pVictim->vitals().health() <= 0)
	{
		return 0;
	}
	sStatus = std::min<std::int16_t>(sStatus, 100);
	const std::uint32_t surgeryConsumption =
		surgeryConsumptionRate();

	UINT32 uiDressSkill, uiPossible, uiActual, uiMedcost, uiDeficiency, uiAvailAPs, uiUsedAPs;
	UINT8 ubBelowOKlife = 0, ubPtsLeft = 0;
	BOOLEAN	fRanOut = FALSE;
	BOOLEAN	fOnSurgery = FALSE;
	INT8 bInitialBleeding;

	if ( (pVictim->vitals().bleeding() < 1 && pVictim->vitals().health() >= OKLIFE) && !(pVictim->vitals().hasHealableInjury() && pSoldier->vitals().isUndergoingSurgery()) )
	{
		return(0);		// nothing to do, shouldn't have even been called!
	}

	// Flugente: dynamic opinions
	if (gGameExternalOptions.fDynamicOpinions &&
		persistentProfile(*pVictim) &&
		persistentProfile(*pSoldier))
	{
		AddOpinionEvent(pVictim->identity().profile(), pSoldier->identity().profile(), OPINIONEVENT_BANDAGED);
	}

	bInitialBleeding = pVictim->vitals().bleeding();

	// in case he has multiple kits in hand, limit influence of kit status to 100%!
	if ( sStatus >= 100 )
	{
		sStatus = 100;
	}

	// if we are going to do the surgery
	// Flugente: AI medics are allowed to perform surgery without first aid kits, and can do pSoldier on themselves
	if ( pVictim->vitals().hasHealableInjury() && pSoldier->vitals().isUndergoingSurgery() && (pSoldier->identity().id() != pVictim->identity().id() || (gGameExternalOptions.fEnemyMedicsHealSelf && pSoldier->roster().team() == ENEMY_TEAM))
		 && gGameOptions.fNewTraitSystem && (NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ) >= gSkillTraitValues.ubDONumberTraitsNeededForSurgery)
		 && (ItemIsMedicalKit(pSoldier->inventory()[HANDPOS].usItem) || pSoldier->roster().team() == ENEMY_TEAM) )
	{
		fOnSurgery = TRUE;
	}

	// calculate wound-dressing skill (3x medical, 2x equip, 1x level, 1x dex)
	if ( gGameOptions.fNewTraitSystem )
	{
		uiDressSkill = ((7 * EffectiveMedical( pSoldier )) +					// medical knowledge
						 (sStatus)+ 																// state of medical kit
						 (10 * EffectiveExpLevel( pSoldier )) +					// battle injury experience
						 EffectiveDexterity( pSoldier, FALSE )) / 10;		// general "handiness"
	}
	else
	{
		uiDressSkill = ((3 * EffectiveMedical( pSoldier )) +					// medical knowledge
						 (2 * sStatus) + 																// state of medical kit
						 (10 * EffectiveExpLevel( pSoldier )) +					// battle injury experience
						 EffectiveDexterity( pSoldier, FALSE )) / 7;		// general "handiness"
	}

	// try to use every AP that the merc has left
	uiAvailAPs = pSoldier->actionPoints().current();

	// OK, If we are in real-time, use another value...
	if ( !(IsJa2TacticalTurnBased()) || !(IsJa2TacticalCombatActive()) )
	{
		// Set to a value which looks good based on our tactical turns duration
		uiAvailAPs = RT_FIRST_AID_GAIN_MODIFIER;
	}

	// calculate how much bandaging CAN be done pSoldier turn
	uiPossible = (uiAvailAPs * uiDressSkill) / 50;	// max rate is 2 * fullAPs

	// if no healing is possible (insufficient APs or insufficient dressSkill)
	if ( !uiPossible )
		return(0);

	// using the GOOD medic stuff
	if (ItemIsMedicalKit(pSoldier->inventory()[HANDPOS].usItem) && !(fOnSurgery) ) // added check
	{
		uiPossible += (uiPossible / 2);			// add extra 50 %
	}

	// when prone, bandaging is harder
	if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_PRONE )
	{
		// if we bandage ourselves, make it rather had when prone
		if ( pSoldier->identity().id() == pVictim->identity().id() )
			uiPossible = uiPossible / 2; // -50% speed
		else
			uiPossible = uiPossible * 4 / 5; // -20% speed
	}

	// Doctor trait improves basic bandaging ability
	if ( !(fOnSurgery) && gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT( pSoldier, DOCTOR_NT ) )
	{
		uiPossible = uiPossible * (100 - gSkillTraitValues.bSpeedModifierBandaging) / 100;
		uiPossible += (uiPossible * gSkillTraitValues.ubDOBandagingSpeedPercent * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ) + TacticalActorModifiers::backgroundValue(*pSoldier, BG_PERC_BANDAGING )) / 100;
	}
	uiPossible = std::min<std::uint32_t>(
		uiPossible,
		std::numeric_limits<std::uint8_t>::max());

	uiActual = uiPossible;		// start by assuming maximum possible

	// figure out how far below OKLIFE the victim is
	// SANDRO - only if we are actually here to bandage the target
	if ( pVictim->vitals().bleeding() )
	{
		if ( pVictim->vitals().health() >= OKLIFE )
		{
			ubBelowOKlife = 0;
		}
		else
		{
			ubBelowOKlife = OKLIFE - pVictim->vitals().health();
		}

		// figure out how many healing pts we need to stop dying (2x cost)
		uiDeficiency = (2 * ubBelowOKlife);

		// if, after that, the patient will still be bleeding
		if ( (pVictim->vitals().bleeding() - ubBelowOKlife) > 0 )
		{
			// then add how many healing pts we need to stop bleeding (1x cost)
			uiDeficiency += (pVictim->vitals().bleeding() - ubBelowOKlife);
		}
		// On surgery, alter pSoldier by amount of life we can heal
		if ( fOnSurgery )
		{
			uiDeficiency += (pVictim->vitals().healableInjury() / 100);
		}

		// now, make sure we weren't going to give too much
		if ( uiActual > uiDeficiency )	// if we were about to apply too much
			uiActual = uiDeficiency;	// reduce actual not to waste anything
	}

	// now make sure we HAVE that much
	if (ItemIsMedicalKit(pSoldier->inventory()[HANDPOS].usItem))
	{
		if ( fOnSurgery )
			uiMedcost = (uiActual * surgeryConsumption) / 100;		// surgery drains the kit a lot
		else
			uiMedcost = (uiActual + 1) / 2;		// cost is only half, rounded up

		if ( uiMedcost > (UINT32)sKitPts )     		// if we can't afford pSoldier
		{
			fRanOut = TRUE;
			uiMedcost = sKitPts;		// what CAN we afford?
			if ( fOnSurgery ) // surgery check
				uiActual = (uiMedcost * 100) / surgeryConsumption;
			else
				uiActual = uiMedcost * 2;		// give double pSoldier as aid
		}
	}
	else
	{
		uiMedcost = uiActual;

		if ( uiMedcost > (UINT32)sKitPts )		// can't afford it
		{
			fRanOut = TRUE;
			uiMedcost = uiActual = sKitPts;   	// recalc cost AND aid
		}
	}

	ubPtsLeft = (UINT8)uiActual;

	// heal real life points first (if below OKLIFE) because we don't want the
	// patient still DYING if bandages run out, or medic is disabled/distracted!
	// NOTE: Dressing wounds for life below OKLIFE now costs 2 pts/life point!
	if ( ubPtsLeft && pVictim->vitals().health() < OKLIFE )
	{
		// if we have enough points to bring him all the way to OKLIFE pSoldier turn
		if ( ubPtsLeft >= (2 * ubBelowOKlife) )
		{
			// insta-healable injury check
			if ( pVictim->vitals().healableInjury() > 0 )
			{
				pVictim->vitals().healableInjury() -= ((OKLIFE - pVictim->vitals().health()) * 100);
				if ( pVictim->vitals().healableInjury() < 0 )
					pVictim->vitals().healableInjury() = 0;
			}

			// raise life to OKLIFE
			pVictim->vitals().health() = OKLIFE;

			// reduce bleeding by the same number of life points healed up
			pVictim->vitals().bleeding() -= ubBelowOKlife;

			// use up appropriate # of actual healing points
			ubPtsLeft -= (2 * ubBelowOKlife);
		}
		else
		{
			// SANDRO - insta-healable injury check
			if ( pVictim->vitals().healableInjury() > 0 )
			{
				pVictim->vitals().healableInjury() -= ((ubPtsLeft / 2) * 100);
				if ( pVictim->vitals().healableInjury() < 0 )
					pVictim->vitals().healableInjury() = 0;
			}

			pVictim->vitals().health() += (ubPtsLeft / 2);
			pVictim->vitals().bleeding() -= (ubPtsLeft / 2);

			ubPtsLeft = ubPtsLeft % 2;	// if ptsLeft was odd, ptsLeft = 1
		}

		// pSoldier should never happen any more, but make sure bleeding not negative
		if ( pVictim->vitals().bleeding() < 0 )
		{
			pVictim->vitals().bleeding() = 0;
		}

		// if pSoldier healing brought the patient out of the worst of it, cancel dying
		if ( pVictim->vitals().health() >= OKLIFE )
		{
			//pVictim->dying = pVictim->dyingComment = FALSE;
			//pVictim->shootOn = TRUE;

			// turn off merc QUOTE flags
			pVictim->dialogue().clearDyingComment();
		}

		// update patient's entire panel (could have regained consciousness, etc.)
	}

	// SURGERY
	// first return the real life back, then bandage the rest if possible
	if ( fOnSurgery && gGameOptions.fNewTraitSystem ) // double check for new traits
	{
		UINT16 usLifeReturned = 0;
		UINT16 usReturnDamagedStatRate = 0;
		// find out if we will repair any stats...
		if ( TacticalActorMedicalTreatment::damagedStatCount(*pVictim) > 0 )
		{
			usReturnDamagedStatRate = ((gSkillTraitValues.usDORepairStatsRateBasic + gSkillTraitValues.usDORepairStatsRateOnTop * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT )));
			usReturnDamagedStatRate -= max( 0, ((usReturnDamagedStatRate * gSkillTraitValues.ubDORepStPenaltyIfAlsoHealing) / 100) );

			// ... in which case, reduce the points
			ubPtsLeft = max( 0, ((ubPtsLeft * (100 - gSkillTraitValues.ubDOHealingPenaltyIfAlsoStatRepair)) / 100) );
		}

		// Important note! : HealableInjury is always stores the total HPs the victim is missing, not the amount which we will heal,
		// so we always take a portion of patient's damage here, reduce the HealableInjury by pSoldier portion, while only healing a portion of pSoldier portion in actual HPs;
		// pSoldier means the rest of HPs will remain as "unhealable", the patient will miss X HPs but has no HealableInjury on self..
		if ( ubPtsLeft >= (pVictim->vitals().healableInjury() / 100) )
		{
			if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_SURGERY_BOOSTED )
				usLifeReturned = pVictim->vitals().healableInjury() * ( gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentBloodbag + gSkillTraitValues.ubDOSurgeryHealPercentOnTop * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ) ) / 100;
			else
				usLifeReturned = pVictim->vitals().healableInjury() * (gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentOnTop * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT )) / 100;

			pVictim->vitals().healableInjury() = 0;
			//CHRISL: Why would we arbitrarily use all ubPtsLeft when a victim isn't bleeding?  And why would the medical bag, which we have to use in order to
			//	do surgery, have any extra benefit?  Plus, the medical back bonus can actually result in ubPtsLeft being HIGHER then it was before we healed the
			//	victim, which makes no sense.
			// keep the rest of the points to bandaging if neccessary
			//if (pVictim->vitals().bleeding() > 0)
			//{
			ubPtsLeft = max( 0, (ubPtsLeft - (usLifeReturned / 100)) );
			//	ubPtsLeft += (ubPtsLeft/2); // we use medical bag so add the bonus for that.
			//}
			//else
			//{
			//	ubPtsLeft = 0;
			//}

			// We are finished !!!
			pSoldier->vitals().finishSurgery();
			gTacticalStatus.ubLastRequesterSurgeryTargetID = NOBODY; // reset last target

			if ( pSoldier->roster().team() != ENEMY_TEAM )
			{
				// throw message
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_SURGERY_FINISHED], pVictim->GetName( ) );
			}

			// add to record - another surgery undergoed
			if (MERCPROFILESTRUCT* profile =
					persistentProfile(*pVictim);
				profile && usLifeReturned >= 100)
			{
				profile->records.usTimesSurgeryUndergoed++;
			}

			// add to record - another surgery made
			if (MERCPROFILESTRUCT* profile =
					persistentProfile(*pSoldier);
				profile && usLifeReturned >= 100)
			{
				profile->records.usSurgeriesMade++;
			}
		}
		else
		{
			if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_SURGERY_BOOSTED )
				usLifeReturned = ubPtsLeft * ( gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentBloodbag + gSkillTraitValues.ubDOSurgeryHealPercentOnTop * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ) );
			else
				usLifeReturned = ubPtsLeft * (gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentOnTop * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ));

			pVictim->vitals().healableInjury() -= (ubPtsLeft * 100);
			ubPtsLeft = 0;
		}
		// repair the stats here!
		if ( usReturnDamagedStatRate > 0 )
		{
			TacticalActorMedicalTreatment::restoreDamagedStats(*pVictim, (usLifeReturned * usReturnDamagedStatRate / 100) );
		}

		// some paranoya checks for sure
		if ( (pVictim->vitals().health() + (usLifeReturned / 100)) <= pVictim->vitals().maximumHealth() )
		{
			pVictim->vitals().health() += (usLifeReturned / 100);
			if ( pVictim->vitals().bleeding() >= (usLifeReturned / 100) )
			{
				pVictim->vitals().bleeding() -= (usLifeReturned / 100);
				uiMedcost += (usLifeReturned / 200); // add medkit points cost for unbandaged part
			}
			else
			{
				pVictim->vitals().bleeding() = 0;
				uiMedcost += max( 0, (((usLifeReturned / 100) - pVictim->vitals().bleeding()) / 2) ); // add medkit points cost for unbandaged part
			}

			// display healing done
			pVictim->damageDisplay().displayFlag() = TRUE;
			pVictim->combatResult().accumulatedDamage() -= (usLifeReturned / 100);
		}
		else // pSoldier shouldn't even happen, but we still want to have it here for sure
		{
			// display healing done
			pVictim->damageDisplay().displayFlag() = TRUE;
			pVictim->combatResult().accumulatedDamage() -= (pVictim->vitals().maximumHealth() - pVictim->vitals().health());

			pVictim->vitals().health() = pVictim->vitals().maximumHealth();
			pVictim->vitals().healableInjury() = 0;
			pVictim->vitals().bleeding() = 0;
		}

		// Reduce max breath based on life returned
		if ( (pVictim->vitals().maximumBreath() - (((usLifeReturned / 100) * gSkillTraitValues.usDOSurgeryMaxBreathLoss) / 100)) <= BREATHMAX_ABSOLUTE_MINIMUM )
		{
			pVictim->vitals().maximumBreath() = BREATHMAX_ABSOLUTE_MINIMUM;
		}
		else
		{
			pVictim->vitals().maximumBreath() -= (((usLifeReturned / 100) * gSkillTraitValues.usDOSurgeryMaxBreathLoss) / 100);
		}

		if ( pVictim->vitals().healableInjury() > ((pVictim->vitals().maximumHealth() - pVictim->vitals().health()) * 100) )
			pVictim->vitals().healableInjury() = ((pVictim->vitals().maximumHealth() - pVictim->vitals().health()) * 100);
		else if ( pVictim->vitals().healableInjury() < 0 )
			pVictim->vitals().healableInjury() = 0;

		// Flugente: campaign stats
		gCurrentIncident.usIncidentFlags |= INCIDENT_SURGERY;
	}

	// if any healing points remain, apply that to any remaining bleeding (1/1)
	// DON'T spend any APs/kit pts to cure bleeding until merc is no longer dying
	//if ( ubPtsLeft && pVictim->vitals().bleeding() && !pVictim->dying)
	if ( ubPtsLeft && pVictim->vitals().bleeding() )
	{
		// if we have enough points to bandage all remaining bleeding pSoldier turn
		if ( ubPtsLeft >= pVictim->vitals().bleeding() )
		{
			ubPtsLeft -= pVictim->vitals().bleeding();
			pVictim->vitals().bleeding() = 0;
		}
		else		// bandage what we can
		{
			pVictim->vitals().bleeding() -= ubPtsLeft;
			ubPtsLeft = 0;
		}

		// update patient's life bar only
	}

	// if wound has been dressed enough so that bleeding won't occur, turn off
	// the "warned about bleeding" flag so merc tells us about the next bleeding
	if ( pVictim->vitals().bleeding() <= MIN_BLEEDING_THRESHOLD )
	{
		pVictim->dialogue().clearBleedingWarning();
	}

	//CHRISL: If by some chance ubPtsLeft ends up being higher then uiActual, we'll end up with a huge value since uiActual is an unsigned variable.
	// if there are any ptsLeft now, then we didn't actually get to use them
	uiActual = max( 0, (INT32)(uiActual - ubPtsLeft) );

	// usedAPs equals (actionPts) * (%of possible points actually used)
	uiUsedAPs = (uiActual * uiAvailAPs) / uiPossible;

	if (ItemIsMedicalKit(pSoldier->inventory()[HANDPOS].usItem) && !(fOnSurgery) )	// using the GOOD medic stuff
	{
		uiUsedAPs = (uiUsedAPs * 2) / 3;	// reverse 50% bonus by taking 2/3rds
	}

	// SANDRO - surgery is harder so cost more BPs
	if ( fOnSurgery )
	{
		DeductPoints( pSoldier, (INT16)uiUsedAPs, (INT16)(uiUsedAPs * 15) );
	}
	else
	{
		DeductPoints( pSoldier, (INT16)uiUsedAPs, (INT16)((uiUsedAPs * APBPConstants[BP_PER_AP_LT_EFFORT])) );
	}

	if ( pSoldier->roster().team() == gbPlayerNum )
	{
		// surgery is harder so gives more exp
		if ( fOnSurgery )
		{
			// MEDICAL GAIN   (actual / 2):  Helped someone by giving first aid
			StatChange( pSoldier, MEDICALAMT, (UINT16)(uiActual + 2), FALSE );

			// DEXTERITY GAIN (actual / 6):  Helped someone by giving first aid
			StatChange( pSoldier, DEXTAMT, (UINT16)((uiActual / 3) + 2), FALSE );
		}
		else
		{
			// MEDICAL GAIN   (actual / 2):  Helped someone by giving first aid
			StatChange( pSoldier, MEDICALAMT, (UINT16)(uiActual / 2), FALSE );

			// DEXTERITY GAIN (actual / 6):  Helped someone by giving first aid
			StatChange( pSoldier, DEXTAMT, (UINT16)(uiActual / 6), FALSE );
		}
	}

	// merc records - bandaging
	if (MERCPROFILESTRUCT* profile =
			persistentProfile(*pSoldier);
		profile &&
		bInitialBleeding > 1 &&
		pVictim->vitals().bleeding() == 0)
	{
		profile->records.usMercsBandaged++;
	}

	if ( is_networked && pVictim->identity().id() > 19 )send_heal( pVictim );

	return(uiMedcost);
}

std::uint32_t TacticalActorMedicalTreatment::treatAbstract(
	TacticalActor& doctor,
	TacticalActor& patient,
	OBJECTTYPE& kit,
	std::int16_t sKitPts,
	std::int16_t sStatus,
	bool fOnSurgery)
{
	TacticalActor* const pSoldier = &doctor;
	TacticalActor* const pVictim = &patient;
	OBJECTTYPE* const pKit = &kit;
	if (sKitPts <= 0 ||
		sStatus < 0 ||
		!hasUsableKit(*pKit) ||
		pSoldier->actionPoints().current() < 0 ||
		pVictim->vitals().health() <= 0)
	{
		return 0;
	}
	sStatus = std::min<std::int16_t>(sStatus, 100);
	const std::uint32_t surgeryConsumption =
		surgeryConsumptionRate();

	UINT32 uiDressSkill, uiPossible, uiActual, uiMedcost, uiDeficiency, uiAvailAPs, uiUsedAPs;
	UINT8 bBelowOKlife, bPtsLeft;
	INT8 bInitialBleeding;

	if ( pVictim->vitals().bleeding() < 1 && !fOnSurgery )
		return 0;		// nothing to do, shouldn't have even been called!
	if ( fOnSurgery && pVictim->identity().id() == pSoldier->identity().id() ) // cannot make surgery on self
		return 0;

	bInitialBleeding = pVictim->vitals().bleeding();

	if ( !gGameOptions.fNewTraitSystem && fOnSurgery ) // cannot make surgery if not new traits
		fOnSurgery = FALSE;

	// calculate wound-dressing skill (3x medical, 2x equip, 1x level, 1x dex)
	if ( gGameOptions.fNewTraitSystem )
	{
		uiDressSkill = ((7 * EffectiveMedical( pSoldier )) +					// medical knowledge
						 (sStatus)+ 																// state of medical kit
						 (10 * EffectiveExpLevel( pSoldier )) +					// battle injury experience
						 EffectiveDexterity( pSoldier, FALSE )) / 10;		// general "handiness"
	}
	else
	{
		uiDressSkill = ((3 * EffectiveMedical( pSoldier )) +					// medical knowledge
						 (2 * sStatus) + 																// state of medical kit
						 (10 * EffectiveExpLevel( pSoldier )) +					// battle injury experience
						 EffectiveDexterity( pSoldier, FALSE )) / 7;		// general "handiness"
	}

	// try to use every AP that the merc has left
	uiAvailAPs = pSoldier->actionPoints().current();

	// OK, If we are in real-time, use another value...
	if ( !(IsJa2TacticalTurnBased()) || !(IsJa2TacticalCombatActive()) )
	{	// Set to a value which looks good based on out tactical turns duration
		uiAvailAPs = RT_FIRST_AID_GAIN_MODIFIER;
	}

	// calculate how much bandaging CAN be done this turn
	uiPossible = (uiAvailAPs * uiDressSkill) / 50;	// max rate is 2 * fullAPs

	// if no healing is possible (insufficient APs or insufficient dressSkill)
	if ( !uiPossible )
		return 0;

	if (ItemIsMedicalKit(pKit->usItem) && !(fOnSurgery) )		// using the GOOD medic stuff
		uiPossible += (uiPossible / 2);			// add extra 50 %

	// Doctor trait improves basic bandaging ability
	if ( !(fOnSurgery) && gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT( pSoldier, DOCTOR_NT ) )
	{
		uiPossible = uiPossible * (100 - gSkillTraitValues.bSpeedModifierBandaging) / 100;
		uiPossible += (uiPossible * gSkillTraitValues.ubDOBandagingSpeedPercent * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ) + TacticalActorModifiers::backgroundValue(*pSoldier, BG_PERC_BANDAGING )) / 100;
	}
	uiPossible = std::min<std::uint32_t>(
		uiPossible,
		std::numeric_limits<std::uint8_t>::max());

	uiActual = uiPossible;		// start by assuming maximum possible

	// figure out how far below OKLIFE the victim is
	if ( pVictim->vitals().health() >= OKLIFE )
		bBelowOKlife = 0;
	else
		bBelowOKlife = OKLIFE - pVictim->vitals().health();

	// figure out how many healing pts we need to stop dying (2x cost)
	uiDeficiency = (2 * bBelowOKlife);

	// if, after that, the patient will still be bleeding
	if ( (pVictim->vitals().bleeding() - bBelowOKlife) > 0 )
	{ // then add how many healing pts we need to stop bleeding (1x cost)
		uiDeficiency += (pVictim->vitals().bleeding() - bBelowOKlife);
	}
	// On surgery, alter this by amount of life we can heal
	if ( fOnSurgery )
	{
		uiDeficiency += (pVictim->vitals().healableInjury() / 100);
	}
	// now, make sure we weren't going to give too much
	if ( uiActual > uiDeficiency )	// if we were about to apply too much
		uiActual = uiDeficiency;	// reduce actual not to waste anything

	// now make sure we HAVE that much
	if (ItemIsMedicalKit(pKit->usItem))
	{
		if ( fOnSurgery )
			uiMedcost = (uiActual * surgeryConsumption) / 100;		// surgery drains the kit a lot
		else
			uiMedcost = (uiActual + 1) / 2;		// cost is only half, rounded up

		if ( uiMedcost == 0 && uiActual > 0 )
			uiMedcost = 1;
		if ( uiMedcost > (UINT32)sKitPts )     		// if we can't afford this
		{
			uiMedcost = sKitPts;		// what CAN we afford?
			if ( fOnSurgery ) // surgery check
				uiActual = (uiMedcost * 100) / surgeryConsumption;
			else
				uiActual = uiMedcost * 2;		// give double this as aid
		}
	}
	else
	{
		uiMedcost = uiActual;
		if ( uiMedcost == 0 && uiActual > 0 )
			uiMedcost = 1;
		if ( uiMedcost > (UINT32)sKitPts )		// can't afford it
			uiMedcost = uiActual = sKitPts;		// recalc cost AND aid
	}

	bPtsLeft = (INT8)uiActual;
	// heal real life points first (if below OKLIFE) because we don't want the
	// patient still DYING if bandages run out, or medic is disabled/distracted!
	// NOTE: Dressing wounds for life below OKLIFE now costs 2 pts/life point!
	if ( bPtsLeft && pVictim->vitals().health() < OKLIFE )
	{
		// if we have enough points to bring him all the way to OKLIFE this turn
		if ( bPtsLeft >= (2 * bBelowOKlife) )
		{
			// insta-healable injury check
			if ( pVictim->vitals().healableInjury() > 0 )
			{
				pVictim->vitals().healableInjury() -= ((OKLIFE - pVictim->vitals().health()) * 100);
				if ( pVictim->vitals().healableInjury() < 0 )
					pVictim->vitals().healableInjury() = 0;
			}
			// raise life to OKLIFE
			pVictim->vitals().health() = OKLIFE;
			// reduce bleeding by the same number of life points healed up
			pVictim->vitals().bleeding() -= bBelowOKlife;

			// use up appropriate # of actual healing points
			bPtsLeft -= (2 * bBelowOKlife);
		}
		else
		{
			// insta-healable injury check
			if ( pVictim->vitals().healableInjury() > 0 )
			{
				pVictim->vitals().healableInjury() -= ((bPtsLeft / 2) * 100);
				if ( pVictim->vitals().healableInjury() < 0 )
					pVictim->vitals().healableInjury() = 0;
			}
			pVictim->vitals().health() += (bPtsLeft / 2);
			pVictim->vitals().bleeding() -= (bPtsLeft / 2);

			bPtsLeft = bPtsLeft % 2;	// if ptsLeft was odd, ptsLeft = 1
		}

		// this should never happen any more, but make sure bleeding not negative
		if ( pVictim->vitals().bleeding() < 0 )
		{
			pVictim->vitals().bleeding() = 0;
		}

		// if this healing brought the patient out of the worst of it, cancel dying
		if ( pVictim->vitals().health() >= OKLIFE )
		{ // turn off merc QUOTE flags
			pVictim->dialogue().clearDyingComment();
		}

		if ( pVictim->vitals().bleeding() <= MIN_BLEEDING_THRESHOLD )
		{
			pVictim->dialogue().clearBleedingWarning();
		}
	}

	// SURGERY
	// first return the real life back, then bandage the rest if possible
	if ( fOnSurgery && gGameOptions.fNewTraitSystem ) // double check for new traits
	{
		INT32 iLifeReturned = 0;
		UINT16 usReturnDamagedStatRate = 0;
		// find out if we will repair any stats...
		if ( TacticalActorMedicalTreatment::damagedStatCount(*pVictim) > 0 )
		{
			usReturnDamagedStatRate = ((gSkillTraitValues.usDORepairStatsRateBasic + gSkillTraitValues.usDORepairStatsRateOnTop * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT )));
			usReturnDamagedStatRate -= max( 0, ((usReturnDamagedStatRate * gSkillTraitValues.ubDORepStPenaltyIfAlsoHealing) / 100) );

			// ... in which case, reduce the points
			bPtsLeft = max( 0, ((bPtsLeft * (100 - gSkillTraitValues.ubDOHealingPenaltyIfAlsoStatRepair)) / 100) );
		}

		// Important note! : HealableInjury is always stores the total HPs the victim is missing, not the amount which we will heal,
		// so we always take a portion of patient's damage here, reduce the HealableInjury by this portion, while only healing a portion of this portion in actual HPs;
		// this means the rest of HPs will remain as "unhealable", the patient will miss X HPs but has no HealableInjury on self..
		if ( bPtsLeft >= (pVictim->vitals().healableInjury() / 100) )
		{
			if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_SURGERY_BOOSTED )
				iLifeReturned = pVictim->vitals().healableInjury() * ( gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentBloodbag + gSkillTraitValues.ubDOSurgeryHealPercentOnTop * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ) ) / 100;
			else
				iLifeReturned = pVictim->vitals().healableInjury() * ( gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentOnTop * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ) ) / 100;

			pVictim->vitals().healableInjury() = 0;
			// keep the rest of the points to bandaging if neccessary
			if ( pVictim->vitals().bleeding() > 0 )
			{
				bPtsLeft = max( 0, (bPtsLeft - (iLifeReturned / 100)) );
				bPtsLeft += (bPtsLeft / 2); // we use medical bag so add the bonus for that.
			}
			else
			{
				bPtsLeft = 0;
			}

			// add to record - another surgery undergoed
			if (MERCPROFILESTRUCT* profile =
					persistentProfile(*pVictim);
				profile && iLifeReturned >= 100)
			{
				profile->records.usTimesSurgeryUndergoed++;
			}

			// add to record - another surgery made
			if (MERCPROFILESTRUCT* profile =
					persistentProfile(*pSoldier);
				profile && iLifeReturned >= 100)
			{
				profile->records.usSurgeriesMade++;
			}
		}
		else
		{
			if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_SURGERY_BOOSTED )
				iLifeReturned = bPtsLeft * (gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentBloodbag + gSkillTraitValues.ubDOSurgeryHealPercentOnTop * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ));
			else
				iLifeReturned = bPtsLeft * ( gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentOnTop * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ) );

			pVictim->vitals().healableInjury() -= (bPtsLeft * 100);
			bPtsLeft = 0;
		}
		// repair the stats here!
		if ( usReturnDamagedStatRate > 0 )
		{
			TacticalActorMedicalTreatment::restoreDamagedStats(*pVictim, (iLifeReturned * usReturnDamagedStatRate / 100) );
		}

		// some paranoya checks for sure
		if ( (pVictim->vitals().health() + (iLifeReturned / 100)) <= pVictim->vitals().maximumHealth() )
		{
			pVictim->vitals().health() += (iLifeReturned / 100);
			if ( pVictim->vitals().bleeding() >= (iLifeReturned / 100) )
			{
				pVictim->vitals().bleeding() -= (iLifeReturned / 100);
				uiMedcost += (iLifeReturned / 200); // add medkit points cost for unbandaged part
			}
			else
			{
				pVictim->vitals().bleeding() = 0;
				uiMedcost += max( 0, (((iLifeReturned / 100) - pVictim->vitals().bleeding()) / 2) ); // add medkit points cost for unbandaged part
			}

			// display healing done
			pVictim->damageDisplay().displayFlag() = TRUE;
			pVictim->combatResult().accumulatedDamage() -= (iLifeReturned / 100);
		}
		else // this shouldn't even happen, but we still want to have it here for sure
		{
			// display healing done
			pVictim->damageDisplay().displayFlag() = TRUE;
			pVictim->combatResult().accumulatedDamage() -= (pVictim->vitals().maximumHealth() - pVictim->vitals().health());

			pVictim->vitals().health() = pVictim->vitals().maximumHealth();
			pVictim->vitals().healableInjury() = 0;
			pVictim->vitals().bleeding() = 0;
		}
		// Reduce max breath based on life returned
		if ( (pVictim->vitals().maximumBreath() - (((iLifeReturned / 100) * gSkillTraitValues.usDOSurgeryMaxBreathLoss) / 100)) <= BREATHMAX_ABSOLUTE_MINIMUM )
		{
			pVictim->vitals().maximumBreath() = BREATHMAX_ABSOLUTE_MINIMUM;
		}
		else
		{
			pVictim->vitals().maximumBreath() -= (((iLifeReturned / 100) * gSkillTraitValues.usDOSurgeryMaxBreathLoss) / 100);
		}

		if ( pVictim->vitals().healableInjury() > ((pVictim->vitals().maximumHealth() - pVictim->vitals().health()) * 100) )
			pVictim->vitals().healableInjury() = ((pVictim->vitals().maximumHealth() - pVictim->vitals().health()) * 100);
		else if ( pVictim->vitals().healableInjury() < 0 )
			pVictim->vitals().healableInjury() = 0;

		// Flugente: campaign stats
		gCurrentIncident.usIncidentFlags |= INCIDENT_SURGERY;
	}

	// if any healing points remain, apply that to any remaining bleeding (1/1)
	// DON'T spend any APs/kit pts to cure bleeding until merc is no longer dying
	//if ( bPtsLeft && pVictim->vitals().bleeding() && !pVictim->dying)
	if ( bPtsLeft && pVictim->vitals().bleeding() )
	{
		// if we have enough points to bandage all remaining bleeding this turn
		if ( bPtsLeft >= pVictim->vitals().bleeding() )
		{
			bPtsLeft -= pVictim->vitals().bleeding();
			pVictim->vitals().bleeding() = 0;
		}
		else		// bandage what we can
		{
			pVictim->vitals().bleeding() -= bPtsLeft;
			bPtsLeft = 0;
		}
	}

	//CHRISL: If by some chance ubPtsLeft ends up being higher then uiActual, we'll end up with a huge value since uiActual is an unsigned variable.
	// if there are any ptsLeft now, then we didn't actually get to use them
	uiActual = max( 0, (INT32)(uiActual - bPtsLeft) );

	// usedAPs equals (actionPts) * (%of possible points actually used)
	uiUsedAPs = (uiActual * uiAvailAPs) / uiPossible;

	if (ItemIsMedicalKit(pKit->usItem) && !(fOnSurgery) )	// using the GOOD medic stuff
		uiUsedAPs = (uiUsedAPs * 2) / 3;	// reverse 50% bonus by taking 2/3rds

	// surgery is harder so cost more BPs
	if ( fOnSurgery )
		DeductPoints( pSoldier, (INT16)uiUsedAPs, (INT16)(uiUsedAPs * 15) );
	else
		DeductPoints( pSoldier, (INT16)uiUsedAPs, (INT16)((uiUsedAPs * APBPConstants[BP_PER_AP_LT_EFFORT])) );

	// surgery is harder so gives more exp
	if ( fOnSurgery )
	{
		// MEDICAL GAIN   (actual / 2):  Helped someone by giving first aid
		StatChange( pSoldier, MEDICALAMT, (UINT16)(uiActual + 2), FALSE );

		// DEXTERITY GAIN (actual / 6):  Helped someone by giving first aid
		StatChange( pSoldier, DEXTAMT, (UINT16)((uiActual / 3) + 2), FALSE );
	}
	else
	{
		if ( uiActual / 2 )
			// MEDICAL GAIN (actual / 2):	Helped someone by giving first aid
			StatChange( pSoldier, MEDICALAMT, ((UINT16)(uiActual / 2)), FALSE );

		if ( uiActual / 4 )
			// DEXTERITY GAIN (actual / 4):	Helped someone by giving first aid
			StatChange( pSoldier, DEXTAMT, (UINT16)((uiActual / 4)), FALSE );
	}

	// merc records - bandaging
	if (MERCPROFILESTRUCT* profile =
			persistentProfile(*pSoldier);
		profile &&
		bInitialBleeding > 1 &&
		pVictim->vitals().bleeding() == 0)
	{
		profile->records.usMercsBandaged++;
	}

	return uiMedcost;
}
