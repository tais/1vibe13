#include "TacticalActorDisease.h"
#include "TacticalActorAssignments.h"
#include "TacticalActorModifiers.h"

#include "Disease.h"
#include "Drugs And Alcohol.h"
#include "GameSettings.h"
#include "IMP Skill Trait.h"
#include "Interface.h"
#include "Items.h"
#include "SkillCheck.h"
#include "TacticalActor.h"
#include "Soldier Profile Constants.h"
#include "TacticalActorSkills.h"
#include "TacticalActorStateFlags.h"
#include "Soldier Profile.h"
#include "Soldier macros.h"
#include "Text.h"
#include "TacticalWorldAdapter.h"
#include "message.h"
#include "random.h"

#include <algorithm>
#include <cstdint>
#include <vector>

// Flugente: disease
void TacticalActorDisease::infect(
	TacticalActor& actor,
	std::uint8_t aDisease)
{
	auto* const self = &actor;

	if ( !gGameExternalOptions.fDisease
		|| aDisease >= NUM_DISEASES )
		return;

	// diseases should not affect machines
	if ( (self->status().flags() & SOLDIER_VEHICLE) || AM_A_ROBOT( self ) )
		return;

	// do not infect us if we are already infected
	if ( !( Disease[aDisease].usDiseaseProperties & DISEASE_PROPERTY_CANREINFECT ) && self->condition().infected(aDisease) )
		return;

	// we are getting infected. Raise our disease points, but not over the level of an infection
	if ( self->condition().diseasePoints(aDisease) <= Disease[aDisease].sInfectionPtsFull )
	{
		self->condition().diseasePoints(aDisease) = min( self->condition().diseasePoints(aDisease) + Disease[aDisease].sInfectionPtsInitial, Disease[aDisease].sInfectionPtsFull );

		// possibly add a new disability
		if ( Disease[aDisease].usDiseaseProperties & DISEASE_PROPERTY_ADD_DISABILITY )
		{
			// take a random disability we don't yet have and give it to us
			std::vector<UINT8> disabilitieswedonthaveset;
			for ( UINT8 i = NO_DISABILITY + 1; i < min( 31, NUM_DISABILITIES ); ++i )
			{
				if ( !DoesMercHaveDisability( self, i ) )
					disabilitieswedonthaveset.push_back(i);
			}

			if ( !disabilitieswedonthaveset.empty() )
			{
				UINT8 newdisability = disabilitieswedonthaveset[Random( disabilitieswedonthaveset.size() )];
				TacticalActorDisease::addDisability(*self, newdisability );
			}
		}

		if ( !self->condition().hasDiseaseFlag(aDisease, TacticalActorDisease::outbreakFlag) && self->condition().diseasePoints(aDisease) > Disease[aDisease].sInfectionPtsOutbreak )
		{
			self->condition().markDiseaseFlag(aDisease, TacticalActorDisease::outbreakFlag);

			TacticalActorDisease::announce(*self, aDisease );
		}

		// remove later on, for testing only
		//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"%s was infected with %s", gMercProfiles[self->ubProfile].zNickname, Disease[aDisease].szName );
	}
}

void TacticalActorDisease::addPoints(
	TacticalActor& actor,
	std::uint8_t aDisease,
	std::int32_t aVal)
{
	auto* const self = &actor;

	if ( !gGameExternalOptions.fDisease )
		return;

	// diseases should not affect machines
	if ( (self->status().flags() & SOLDIER_VEHICLE) || AM_A_ROBOT( self ) )
		return;

	if ( aDisease < NUM_DISEASES )
	{
		self->condition().diseasePoints(aDisease) = min( Disease[aDisease].sInfectionPtsFull, max( self->condition().diseasePoints(aDisease) + aVal, -Disease[aDisease].sInfectionPtsOutbreak ) );

		// if the disease 'breaks out', make it known
		if ( self->condition().diseasePoints(aDisease) > Disease[aDisease].sInfectionPtsOutbreak )
		{
			self->condition().markDiseaseFlag(aDisease, TacticalActorDisease::outbreakFlag);

			if ( !self->condition().hasDiseaseFlag(aDisease, TacticalActorDisease::diagnosedFlag) )
				TacticalActorDisease::announce(*self, aDisease );
		}

		// once disease is fullblown, some diseases reverse themself
		if ( (Disease[aDisease].usDiseaseProperties & DISEASE_PROPERTY_REVERSEONFULL) && self->condition().diseasePoints(aDisease) >= Disease[aDisease].sInfectionPtsFull )
		{
			self->condition().markDiseaseFlag(aDisease, TacticalActorDisease::reversingFlag);
		}

		// if disease is cured, remove traces of it
		if ( self->condition().diseasePoints(aDisease) <= 0 )
		{
			// if disease was known and this guy is under player control, let the player know the good news
			if ( self->condition().hasDiseaseFlag(aDisease, TacticalActorDisease::diagnosedFlag) && self->roster().team() == gbPlayerNum )
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szDiseaseText[TEXT_DISEASE_CURED], self->GetName( ), Disease[aDisease].szName );

			self->condition().clearDiseaseFlags(aDisease, TacticalActorDisease::diagnosedFlag | TacticalActorDisease::outbreakFlag | TacticalActorDisease::legSplintFlag | TacticalActorDisease::armSplintFlag);
		}
	}
}

void TacticalActorDisease::announce(
	TacticalActor& actor,
	std::uint8_t aDisease)
{
	auto* const self = &actor;

	if (aDisease >= NUM_DISEASES)
		return;

	self->condition().markDiseaseFlag(aDisease, TacticalActorDisease::diagnosedFlag);

	if ( self->roster().team() == gbPlayerNum )
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szDiseaseText[TEXT_DISEASE_DIAGNOSE_GENERAL], self->GetName( ), Disease[aDisease].szName );

	// add to our records.
	if ( self->identity().profile() != NO_PROFILE )
		gMercProfiles[self->identity().profile()].records.usTimesInfected += 1;
}

void TacticalActorDisease::addDisability(
	TacticalActor& actor,
	std::uint8_t aDisability)
{
	actor.condition().addDisability(aDisability);
}

// Flugente: can we apply a medical splint to this guy?
bool TacticalActorDisease::canReceiveSplint(TacticalActor& actor)
{
	auto* const self = &actor;

	// not during combat
	if ( IsJa2TacticalCombatActive() )
		return FALSE;

	//  must be player team
	if ( self->roster().team() != gbPlayerNum )
		return FALSE;

	if ( !gGameExternalOptions.fDisease
		|| !gGameExternalOptions.fDiseaseSevereLimitations )
		return FALSE;

	// check whether we have a disease that limits arm/leg use without having a splint
	for ( int i = 0; i < NUM_DISEASES; ++i )
	{
		if ( self->condition().infected(i) && self->condition().hasDiseaseFlag(i, TacticalActorDisease::diagnosedFlag) )
		{
			if ( (Disease[i].usDiseaseProperties & DISEASE_PROPERTY_LIMITED_USE_ARMS && !self->condition().hasDiseaseFlag(i, TacticalActorDisease::armSplintFlag) )
				|| ( Disease[i].usDiseaseProperties & DISEASE_PROPERTY_LIMITED_USE_LEGS && !self->condition().hasDiseaseFlag(i, TacticalActorDisease::legSplintFlag) ) )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

// do we have any disease? fDiagnosedOnly: check for wether we know of this infection fHealableOnly: check wether it can be healed
bool TacticalActorDisease::hasAny(
	TacticalActor& actor,
	bool fDiagnosedOnly,
	bool fHealableOnly,
	bool fSymbolOnly)
{
	auto* const self = &actor;

	if ( !gGameExternalOptions.fDisease )
		return FALSE;

	for ( int i = 0; i < NUM_DISEASES; ++i )
	{
		// disease is relevant if we are infected and are not looking for symbols only while the disease has no symbol
		if ( self->condition().infected(i) && !(fSymbolOnly && (Disease[i].usDiseaseProperties & DISEASE_PROPERTY_HIDESYMBOL)) )
		{
			// only if we don't check for diagnosis, or we already know of this
			if ( !fDiagnosedOnly || self->condition().hasDiseaseFlag(i, TacticalActorDisease::diagnosedFlag) )
			{
				// only if we don't check for cure, or this can be cured
				if ( !fHealableOnly || (Disease[i].usDiseaseProperties & DISEASE_PROPERTY_CANBECURED) )
				{
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

// Do we have an outbroken disease with a special property?
bool TacticalActorDisease::hasOutbreakProperty(
	TacticalActor& actor,
	std::uint32_t aFlag)
{
	auto* const self = &actor;

	if ( !gGameExternalOptions.fDisease )
		return FALSE;

	for ( int i = 0; i < NUM_DISEASES; ++i )
	{
		// disease is relevant if we are infected and are not looking for symbols only while the disease has no symbol
		if ( ( Disease[i].usDiseaseProperties & aFlag ) && self->condition().infected(i) && self->condition().hasDiseaseFlag(i, TacticalActorDisease::outbreakFlag) )
		{
			return TRUE;
		}
	}

	return FALSE;
}

// get the magnitude of a disease we might have, used to determine wether there are any effects
float TacticalActorDisease::magnitude(
	const TacticalActor& actor,
	std::uint8_t aDisease)
{
	auto* const self = &actor;

	if ( !gGameExternalOptions.fDisease )
		return 0.0f;

	// Diseases only have effects once they have broken out (otherwise stuff
	// happens without the player having any clue as to why). Treat malformed
	// disease data as inactive instead of dividing by zero.
	if ( aDisease >= NUM_DISEASES ||
		!self->condition().infected(aDisease) ||
		!self->condition().hasDiseaseFlag(aDisease, TacticalActorDisease::outbreakFlag) ||
		Disease[aDisease].sInfectionPtsFull <= 0 )
		return 0.0f;

	return static_cast<float>(self->condition().diseasePoints(aDisease)) /
		static_cast<float>(Disease[aDisease].sInfectionPtsFull);
}

// get percentage protection from infections via contact
float TacticalActorDisease::contactProtection(TacticalActor& actor)
{
	auto* const self = &actor;

	FLOAT val = 0.0f;

	// if we wear special equipment, lower our chances of being infected
	FLOAT bestfacegear = 0.0f;
	FLOAT bestprotectivegear = 0.0f;
	for ( const auto &item : self->inventory().items() )
	{
		if ( item.exists( ) )
		{
			if ( item[0]->data.objectStatus >= USABLE )
			{
				if ( HasItemFlag( item.usItem, DISEASEPROTECTION_1 ) )
				{
					bestfacegear = max(
						bestfacegear,
						static_cast<float>(item[0]->data.objectStatus) / 100.0f);
				}
				if ( HasItemFlag( item.usItem, DISEASEPROTECTION_2 ) )
				{
					bestprotectivegear = max(
						bestprotectivegear,
						static_cast<float>(item[0]->data.objectStatus) / 100.0f);
				}
			}
		}
	}

	// up to 100% protection if face and hand protection is worn
	val += (bestfacegear + bestprotectivegear) / 2;

	// not higher than 100%
	return min( val, 1.0f );
}

std::int16_t TacticalActorDisease::resistance(TacticalActor& actor)
{
	auto* const self = &actor;

	// Flugente: resistance can per definition only be between -100 and 100 (at least that's my definition)
	INT16 val = 0;

	if ( HAS_SKILL_TRAIT( self, SURVIVAL_NT ) )
		val += gSkillTraitValues.usSVDiseaseResistance;

	val += TacticalActorModifiers::backgroundValue(*self, BG_RESI_DISEASE );

	val = max( -100, val );
	val = min( 100, val );

	return(val);
}

std::uint16_t TacticalActorDisease::diagnosisPoints(TacticalActor& actor)
{
	auto* const self = &actor;

	// determine our skill at detecting disease
	UINT16 skill = self->statistics().medical() / 2 + NUM_SKILL_TRAITS( self, DOCTOR_NT ) * 15;

	skill = ( skill * ( 100 + TacticalActorModifiers::backgroundValue(*self, BG_PERC_DISEASE_DIAGNOSE ) ) ) / 100;

	FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*self);
	skill *= administrationmodifier;

	return skill;
}
