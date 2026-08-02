#include "TacticalActorAssignments.h"

#include "TacticalActorModifiers.h"

#include "Assignments.h"
#include "Campaign Types.h"
#include "Drugs And Alcohol.h"
#include "Facilities.h"
#include "Food.h"
#include "Game Clock.h"
#include "GameSettings.h"
#include "IMP Skill Trait.h"
#include "Interface.h"
#include "Items.h"
#include "Rebel Command.h"
#include "SkillCheck.h"
#include "Soldier Control.h"
#include "Soldier Profile.h"
#include "Soldier macros.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

float TacticalActorAssignments::burialPoints(
	TacticalActor& actor,
	std::uint16_t* apCorpses)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE || self->deployment().sectorZ() || ( self->featureFlags().primaryFlags() & SOLDIER_POW ) )
		return 0.0f;

	if ( apCorpses )
		*apCorpses =
			SectorInfo[SECTOR(
				self->deployment().sectorX(),
				self->deployment().sectorY())].usNumCorpses;

	// if not on correct assignment, no gain
	if ( self->assignment().current() != BURIAL )
		return 0.0f;

	UINT32 val = 4 * EffectiveStrength( self, FALSE );

	ReducePointsForFatigue( self, &val );

	// personality/disability modifiers
	FLOAT persmodifier = 1.0f;
	if ( DoesMercHaveDisability( self, HEAT_INTOLERANT ) )	persmodifier -= 0.01f;
	if ( DoesMercHaveDisability( self, FEAR_OF_INSECTS ) )	persmodifier -= 0.03f;

	// background modifier
	persmodifier += ( TacticalActorModifiers::backgroundValue(*self, BG_BURIAL_ASSIGNMENT ) ) / 100.0f;

	// equipment modifier
	FLOAT bestequipmentmodifier = 1.0f;

	INT8 invsize = (INT8)self->inventory().size();									// remember inventorysize, so we don't call size() repeatedly

	for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )						// ... for all items in our inventory ...
	{
		if ( self->inventory()[bLoop].exists() == true &&
			Item[self->inventory()[bLoop].usItem].usBurialModifier )
		{
			OBJECTTYPE& object = self->inventory()[bLoop];
			for ( INT16 i = 0; i < object.ubNumberOfObjects; ++i )
			{
				FLOAT modifier = 1.0f +
					(Item[object.usItem].usBurialModifier *
						object[i]->data.objectStatus) /
						10000.0f;

				if ( modifier > bestequipmentmodifier )
					bestequipmentmodifier = modifier;
			}
		}
	}

	FLOAT administrationmodifier =
		TacticalActorAssignments::administrationModifier(actor);

	FLOAT totalvalue =
		val * max(0.0f, persmodifier) *
		bestequipmentmodifier * administrationmodifier * 0.01f;

	// A most awesome merc in Meduna palace, disguised as a soldier, would have a value of 1.15 * 4.63 * 2 = 10.649 at this point.
	// This would be the place where we modify our intel gain rate.

	return totalvalue;
}

// Flugente: hourly breath regen calculation
std::int8_t TacticalActorAssignments::sleepBreathRegeneration(
	TacticalActor& actor)
{
	auto* const self = &actor;
	if (self->identity().profile() == NO_PROFILE ||
		self->identity().profile() >= NUM_PROFILES ||
		self->vitals().maximumHealth() <= 0)
	{
		return 0;
	}

	// handle the sleep of this character, update bBreathMax based on sleep they have
	INT8 bMaxBreathRegain = 0;
	INT16 sSectorModifier = 100;
	FLOAT bDivisor = 0;

	// Determine how many hours a day this merc must sleep. Normally this would range between 6 and 12 hours.
	// Injuries and/or martial arts trait can change the limits to between 3 and 18 hours a day.
	bDivisor = CalcSoldierNeedForSleep( self );

	// HEADROCK HAM 3.6:
	// Night ops specialists sleep better during the day. Others sleep better during the night.
	// silversurfer: The code below did the complete opposite. A higher bDivisor means LESS regeneration. Fixed.
	if ( DayTime( ) )	//if (NightTime())
	{
		if ( gGameOptions.fNewTraitSystem ) // SANDRO - Old/New traits
		{
			if ( !HAS_SKILL_TRAIT( self, NIGHT_OPS_NT ) )
				bDivisor += 3;
		}
		else
			bDivisor += 4 - (2 * NUM_SKILL_TRAITS( self, NIGHTOPS_OT ));
	}
	else
	{
		if ( gGameOptions.fNewTraitSystem ) // SANDRO - Old/New traits
		{
			if ( HAS_SKILL_TRAIT( self, NIGHT_OPS_NT ) )
				bDivisor += 3;
		}
		else
			bDivisor += (2 * NUM_SKILL_TRAITS( self, NIGHTOPS_OT ));
	}

	// HEADROCK HAM 3.5: Read adjustment from local sector facilities
	if ( self->deployment().sectorZ() == 0 )
	{
		if ( self->assignment().isAsleep() )
		{
			sSectorModifier = GetSectorModifier( self, FACILITY_SLEEP_MOD );
		}
		else
		{
			// Resting can be done at a facility now, and the program will automatically apply a performance bonus
			// to this if the facility has one. If the character is simply resting ("On Duty", assigned to a squad),
			// then only Ambient effects take place.
			sSectorModifier = GetSectorModifier( self, FACILITY_PERFORMANCE_MOD );
		}
		if (sSectorModifier <= 0)
			sSectorModifier = 100;
		bDivisor = (bDivisor * 100) / sSectorModifier;
	}

	// silversurfer: Items can provide a bonus to regeneration, sleeping bags for example.
	// They will not provide such bonus if the merc is already using a bed in a facility.
	if ( GetSoldierFacilityAssignmentIndex( self ) != FAC_PATIENT && GetSoldierFacilityAssignmentIndex( self ) != FAC_REST )
	{
		INT16 inventorySleepModifier =
			100 + GetInventorySleepModifier(self);
		if (inventorySleepModifier <= 0)
			inventorySleepModifier = 100;
		bDivisor = (bDivisor * 100) / inventorySleepModifier;
	}

	// silversurfer: I moved all modifiers above this point because we don't want anybody to rest faster or slower than the already extreme thresholds.
	// Re-enforce limits
	bDivisor = __min( 18, __max( 3, bDivisor ) );

	// round up so the bonuses above make more sense
	bMaxBreathRegain = (50 / bDivisor + 0.5);

	// Limit so that characters can't regain faster than 3 hours, ever
	if ( bMaxBreathRegain > 17 )
	{
		bMaxBreathRegain = 17;
	}

	// if breath max is below the "really tired" threshold
	if ( self->vitals().maximumBreath() < BREATHMAX_PRETTY_TIRED )
	{
		// real tired, rest rate is 50% higher (this is to prevent absurdly long sleep times for totally exhausted mercs)
		bMaxBreathRegain = (UINT8)(bMaxBreathRegain * 3 / 2);
	}

	return bMaxBreathRegain;
}

// Flugente: fortification
float TacticalActorAssignments::constructionPoints(TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE ||
		self->vitals().maximumHealth() <= 0 ||
		self->assignment().isAsleep() ||
		self->collapseState().tactical() ||
		(self->featureFlags().primaryFlags() & SOLDIER_POW) )
		return 0;

	UINT32 val = EffectiveStrength( self, FALSE );

	ReducePointsForFatigue( self, &val );

	FLOAT dval = val * (100 + TacticalActorModifiers::backgroundValue(*self, BG_FORTIFY_ASSIGNMENT )) / 100.0f;

	dval = (dval * self->vitals().health() / self->vitals().maximumHealth());

	dval *= TacticalActorAssignments::administrationModifier(actor);

	return max(0.0f, dval);
}

std::uint32_t TacticalActorAssignments::administrationPoints(
	TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE || self->deployment().sectorZ() || ( self->featureFlags().primaryFlags() & SOLDIER_POW ) )
		return 0;

	// if not on correct assignment, no gain
	if ( self->assignment().current() != ADMINISTRATION )
		return 0;
	if (!(gGameExternalOptions.fAdministrationPointsPerPercent > 0.0f))
		return 0;

	UINT32 val = 250 + 4 * EffectiveWisdom( self ) +
		3 * EffectiveLeadership( self ) +
		5 * EffectiveExpLevel( self, FALSE );

	// personality/disability modifiers
	FLOAT persmodifier = 1.0f;
	if ( DoesMercHaveDisability( self, NERVOUS ) )		persmodifier -= 0.01f;
	if ( DoesMercHaveDisability( self, FORGETFUL ) )	persmodifier -= 0.60f;
	if ( DoesMercHaveDisability( self, PSYCHO ) )		persmodifier -= 0.03f;
	if ( DoesMercHaveDisability( self, DEAF ) )			persmodifier -= 0.15f;
	if ( DoesMercHaveDisability( self, SHORTSIGHTED ) )	persmodifier -= 0.10f;

	if ( gGameOptions.fNewTraitSystem )
	{
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_SOCIABLE ) )		persmodifier += 0.10f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_LONER ) )		persmodifier -= 0.10f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_OPTIMIST ) )		persmodifier += 0.02f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_ASSERTIVE ) )	persmodifier += 0.08f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_INTELLECTUAL ) )	persmodifier += 0.15f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_PRIMITIVE ) )	persmodifier -= 0.15f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_AGGRESSIVE ) )	persmodifier -= 0.04f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_PHLEGMATIC ) )	persmodifier -= 0.05f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_SHOWOFF ) )		persmodifier -= 0.03f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_COWARD ) )		persmodifier -= 0.07f;
	}

	// background modifier
	persmodifier += ( TacticalActorModifiers::backgroundValue(*self, BG_ADMINISTRATION_ASSIGNMENT ) ) / 100.0f;

	// equipment modifier
	FLOAT bestequipmentmodifier = 1.0f;

	INT8 invsize = (INT8)self->inventory().size();									// remember inventorysize, so we don't call size() repeatedly

	for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )						// ... for all items in our inventory ...
	{
		if ( self->inventory()[bLoop].exists() == true &&
			Item[self->inventory()[bLoop].usItem].usAdministrationModifier )
		{
			OBJECTTYPE& object = self->inventory()[bLoop];
			for ( INT16 i = 0; i < object.ubNumberOfObjects; ++i )
			{
				FLOAT modifier = 1.0f +
					(Item[object.usItem].usAdministrationModifier *
						object[i]->data.objectStatus) /
						10000.0f;

				if ( modifier > bestequipmentmodifier )
					bestequipmentmodifier = modifier;
			}
		}
	}

	// the best friendly/direct/recruit approach factor can alter the value up to 10%
	FLOAT approachmodifier = 1.0f;
	if (self->identity().profile() != NO_PROFILE &&
		self->identity().profile() < NUM_PROFILES)
	{
		const auto& profile =
			gMercProfiles[self->identity().profile()];
		FLOAT approachmax = max(
			profile.usApproachFactor[0],
			max(profile.usApproachFactor[1], profile.usApproachFactor[2]));
		approachmodifier =
			1.0f + max(-0.1f, min(0.1f, (approachmax - 100.0f) / 100.0f));
	}

	const FLOAT scaledValue =
		val * max(0.0f, persmodifier) *
		bestequipmentmodifier * approachmodifier /
		gGameExternalOptions.fAdministrationPointsPerPercent;
	UINT32 totalvalue = static_cast<UINT32>(scaledValue);

	ReducePointsForFatigue( self, &totalvalue );

	return totalvalue;
}

float TacticalActorAssignments::administrationModifier(
	const TacticalActor& actor)
{
	auto* const self = &actor;

	if ( ADMINISTRATION_BONUS( self->assignment().current() ) )
		return 1.0f + GetAdministrationPercentage( self->deployment().sectorX(), self->deployment().sectorY() ) / 100.0f + RebelCommand::GetAssignmentBonus(self->deployment().sectorX(), self->deployment().sectorY());

	return 1.0f;
}

std::uint32_t TacticalActorAssignments::explorationPoints(
	TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE || ( self->featureFlags().primaryFlags() & SOLDIER_POW ) )
		return 0;

	// if not on correct assignment, no gain
	if ( self->assignment().current() != EXPLORATION )
		return 0;

	UINT32 val = 400 + EffectiveWisdom( self ) +
		EffectiveAgility( self, FALSE ) +
		5 * EffectiveExpLevel( self, FALSE ) +
		150 * NUM_SKILL_TRAITS( self, SCOUTING_NT ) +
		50 * NUM_SKILL_TRAITS( self, SURVIVAL_NT ) +
		(TacticalActorModifiers::hasBackgroundFlag(*self, BACKGROUND_SCROUNGING ) ? 150 : 0);

	// personality/disability modifiers
	FLOAT persmodifier = 1.0f;
	//if ( DoesMercHaveDisability( this, HEAT_INTOLERANT ) )		persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, NERVOUS ) )				persmodifier -= 0.20f;
	if ( DoesMercHaveDisability( self, CLAUSTROPHOBIC ) )		persmodifier -= 0.03f;
	//if ( DoesMercHaveDisability( this, NONSWIMMER ) )			persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, FEAR_OF_INSECTS ) )		persmodifier -= 0.20f;
	if ( DoesMercHaveDisability( self, FORGETFUL ) )			persmodifier -= 0.30f;
	//if ( DoesMercHaveDisability( this, PSYCHO ) )				persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, DEAF ) )					persmodifier -= 0.15f;
	if ( DoesMercHaveDisability( self, SHORTSIGHTED ) )			persmodifier -= 0.30f;
	//if ( DoesMercHaveDisability( this, HEMOPHILIAC ) )			persmodifier -= 0.20f;
	if ( DoesMercHaveDisability( self, AFRAID_OF_HEIGHTS ) )	persmodifier -= 0.02f;
	//if ( DoesMercHaveDisability( this, SELF_HARM ) )			persmodifier -= 0.20f;

	if ( gGameOptions.fNewTraitSystem )
	{
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_SOCIABLE ) )		persmodifier += 0.25f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_LONER ) )		persmodifier -= 0.05f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_OPTIMIST ) )		persmodifier += 0.05f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_ASSERTIVE ) )	persmodifier += 0.15f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_INTELLECTUAL ) )	persmodifier += 0.15f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_PRIMITIVE ) )	persmodifier -= 0.15f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_AGGRESSIVE ) )	persmodifier -= 0.15f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_PHLEGMATIC ) )	persmodifier -= 0.05f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_DAUNTLESS ) )	persmodifier -= 0.13f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_PACIFIST ) )		persmodifier -= 0.03f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_MALICIOUS ) )	persmodifier -= 0.13f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_SHOWOFF ) )		persmodifier -= 0.08f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_COWARD ) )		persmodifier -= 0.02f;
	}

	// background modifier
	persmodifier += ( TacticalActorModifiers::backgroundValue(*self, BG_EXPLORATION_ASSIGNMENT ) ) / 100.0f;

	const FLOAT scaledValue =
		val * max(0.0f, persmodifier) *
		max(0.0f, gGameExternalOptions.fExplorationPointsModifier) /
		10.0f;
	UINT32 totalvalue = static_cast<UINT32>(scaledValue);

	ReducePointsForFatigue( self, &totalvalue );

	return totalvalue;
}
