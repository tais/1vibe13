	#include "sgp.h"
	#include "SoldierRepository.h"
	#include "Soldier Control.h"
	#include "Soldier Profile.h"
	#include "Drugs And Alcohol.h"
	#include "Items.h"
	#include "Points.h"
	#include "message.h"
	#include "GameSettings.h" // SANDRO - had to add this, dammit!
	#include "random.h"
	#include "Text.h"
	#include "Interface.h"
	#include "Overhead.h"
	#include "DynamicDialogue.h"// added by Flugente

//forward declarations of common classes to eliminate includes
class OBJECTTYPE;

INT32	giDrunkModifier[] =
{
	100,		// Sober
	75,			// Feeling good,
	65,			// Bporderline
	50,			// Drunk
	100,		// HungOver
};

#define HANGOVER_AP_REDUCE			5
#define HANGOVER_BP_REDUCE			200

BOOLEAN ApplyDrugs_New( SOLDIERTYPE *pSoldier, UINT16 usItem, UINT16 uStatusUsed )
{
	// If not a drug, return
	if ( !Item[usItem].drugtype || !uStatusUsed || !pSoldier )
		return(FALSE);

	UINT32 drugused = Item[usItem].drugtype;

	// to stop Larry from getting stoned via unsanitary bandages etc., note whether this is a 'real' drug
	BOOL complainworthyeffects = FALSE;

	// we might not use up the entire item, so reduce effects accordingly
	FLOAT effectivepercentage = uStatusUsed / 100.0;

	// if this alcohol, alcohol resistance can lower the effects
	if ( Item[usItem].alcohol > 0.0f )
	{
		effectivepercentage = effectivepercentage * ((100.0 - pSoldier->GetBackgroundValue( BG_RESI_ALCOHOL )) / 100.0);

		FLOAT weight = pSoldier->GetBodyWeight( );

		// the alcohol amounts in the xml are intended for a person with weight 80. We thus have to alter the effective value
		if ( weight > 0.0f )
		{
			effectivepercentage *= 80.0f / weight;
		}
	}

	// we now add drug, disease, personality and disability effects
	// every effect has a chance of happening (not entering a chance, so 0, always results in a effect for xml editing simplicity reasons)
	
	// add effects
	std::vector<DRUG_EFFECT> vec_drug = NewDrug[drugused].drug_effects;

	std::vector<DRUG_EFFECT>::iterator drug_effects_itend = vec_drug.end( );
	for ( std::vector<DRUG_EFFECT>::iterator drug_effects_it = vec_drug.begin( ); drug_effects_it != drug_effects_itend; ++drug_effects_it )
	{
		if ( !(*drug_effects_it).chance || Chance( (*drug_effects_it).chance ) )
		{
			pSoldier->drugState().mergeEffect(
				(*drug_effects_it).effect,
				(*drug_effects_it).duration,
				(*drug_effects_it).size,
				effectivepercentage);

			complainworthyeffects = TRUE;
		}
	}

	// add diseases
	std::vector<DISEASE_EFFECT> vec_disease = NewDrug[drugused].disease_effects;

	std::vector<DISEASE_EFFECT>::iterator disease_effects_itend = vec_disease.end( );
	for ( std::vector<DISEASE_EFFECT>::iterator disease_effects_it = vec_disease.begin( ); disease_effects_it != disease_effects_itend; ++disease_effects_it )
	{
		if ( !(*disease_effects_it).chance || Chance( (*disease_effects_it).chance ) )
		{
			pSoldier->AddDiseasePoints( (*disease_effects_it).disease, (*disease_effects_it).size * effectivepercentage );
		}
	}

	// add disability
	std::vector<DISABILITY_EFFECT> vec_disability = NewDrug[drugused].disability_effects;

	std::vector<DISABILITY_EFFECT>::iterator disability_effects_itend = vec_disability.end( );
	for ( std::vector<DISABILITY_EFFECT>::iterator disability_effects_it = vec_disability.begin( ); disability_effects_it != disability_effects_itend; ++disability_effects_it )
	{
		if ( !(*disability_effects_it).chance || Chance( (*disability_effects_it).chance ) )
		{
			pSoldier->drugState().applyTemporaryDisability(
				(*disability_effects_it).disability,
				static_cast<UINT16>(
					(*disability_effects_it).duration * effectivepercentage));
		}
	}

	// add personality
	std::vector<PERSONALITY_EFFECT> vec_personality = NewDrug[drugused].personality_effects;

	std::vector<PERSONALITY_EFFECT>::iterator personality_effects_itend = vec_personality.end( );
	for ( std::vector<PERSONALITY_EFFECT>::iterator personality_effects_it = vec_personality.begin( ); personality_effects_it != personality_effects_itend; ++personality_effects_it )
	{
		if ( !(*personality_effects_it).chance || Chance( (*personality_effects_it).chance ) )
		{
			pSoldier->drugState().applyTemporaryPersonality(
				(*personality_effects_it).personality,
				static_cast<UINT16>(
					(*personality_effects_it).duration * effectivepercentage));
		}
	}
	
	if ( complainworthyeffects )
	{
		// do switch for Larry!!
		if ( pSoldier->identity().profile() == LARRY_NORMAL )
		{
			SwapToProfile( pSoldier, LARRY_DRUNK );

			gMercProfiles[LARRY_NORMAL].bNPCData = LARRY_FALLS_OFF_WAGON;
		}
		else if ( pSoldier->identity().profile() == LARRY_DRUNK )
		{
			// NB store all drunkenness info in LARRY_NORMAL profile (to use same values)
			// so long as he keeps consuming, keep number above level at which he cracked						
			gMercProfiles[LARRY_NORMAL].bNPCData += (INT8)Random( 5 );

			// allow value to keep going up to 24 (about 2 days since we subtract Random( 2 ) when he has no access )
			gMercProfiles[LARRY_NORMAL].bNPCData = __min( gMercProfiles[LARRY_NORMAL].bNPCData, 24 );
			gMercProfiles[LARRY_NORMAL].bNPCData = __max( gMercProfiles[LARRY_NORMAL].bNPCData, LARRY_FALLS_OFF_WAGON );
		}

		if (gGameExternalOptions.fDynamicOpinions && NewDrug[drugused].opinionevent )
		{
			HandleDynamicOpinionChange( pSoldier, OPINIONEVENT_ADDICT, TRUE, TRUE );
		}
	}
	
	if ( Item[usItem].alcohol > 0.0f )
	{
		FLOAT weight = pSoldier->GetBodyWeight( );

		// the alcohol amounts in the xml are intended for a person with weight 80. We thus have to alter the effective value
		if ( weight > 0.0f )
		{
			// added promille = alcohol added (g) / (weight of person (kg) * 0.7)
			FLOAT addedpromille = (Item[usItem].alcohol * effectivepercentage) / (weight * 0.7);

			pSoldier->drugState().addAlcohol( addedpromille );
		}

		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, pMessageStrings[MSG_DRANK_SOME], pSoldier->GetName( ), ShortItemNames[usItem] );

		if (gGameExternalOptions.fDynamicOpinions)
		{
			HandleDynamicOpinionTeamDrinking(pSoldier);
		}
	}
	else
	{
		// set flag: we are on non-alcoholic drugs
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DRUGGED;

		if ( complainworthyeffects && gMercProfiles[pSoldier->identity().profile()].ubNumTimesDrugUseInLifetime != 255 )
		{
			gMercProfiles[pSoldier->identity().profile()].ubNumTimesDrugUseInLifetime++;
		}

		if (ItemIsCigarette(usItem))
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, pMessageStrings[MSG_MERC_TOOK_CIGARETTE], pSoldier->GetName( ), ShortItemNames[usItem] );
		}
		else
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, pMessageStrings[MSG_MERC_TOOK_DRUG], pSoldier->GetName( ), ShortItemNames[usItem] );
		}
	}

	// Dirty panel
	fInterfacePanelDirty = DIRTYLEVEL2;
	fCharacterInfoPanelDirty = TRUE;

	return TRUE;
}

void HandleEndTurnDrugAdjustments_New( SOLDIERTYPE *pSoldier )
{
	// some effects are handled here
	if ( pSoldier->drugState().magnitude(DRUG_EFFECT_HP) )
	{
		// note the current hp
		INT8 oldlife = pSoldier->vitals().health();

		// increase life
		pSoldier->vitals().health() = __min( pSoldier->vitals().health() + pSoldier->drugState().magnitude(DRUG_EFFECT_HP), pSoldier->vitals().maximumHealth() );

		//SANDRO - Insta-healable injury reduction
		if ( pSoldier->drugState().magnitude(DRUG_EFFECT_HP) > 0 )
		{
			pSoldier->vitals().healableInjury() = max( 0, (pSoldier->vitals().healableInjury() - (100 * pSoldier->drugState().magnitude(DRUG_EFFECT_HP))) );
		}

		if ( pSoldier->vitals().health() == pSoldier->vitals().maximumHealth() )
		{
			pSoldier->vitals().bleeding() = 0;
			pSoldier->vitals().healableInjury() = 0;
		}
		else if ( pSoldier->vitals().bleeding() + pSoldier->vitals().health() > pSoldier->vitals().maximumHealth() )
		{
			// got to reduce amount of bleeding
			pSoldier->vitals().bleeding() = (pSoldier->vitals().maximumHealth() - pSoldier->vitals().health());
		}

		// display health change next time we are in tactical
		pSoldier->damageDisplay().displayFlag() = TRUE;
		pSoldier->combatResult().accumulatedDamage() -= pSoldier->vitals().health() - oldlife;
	}

	pSoldier->condition().extraStrength() += pSoldier->drugState().magnitude(DRUG_EFFECT_STR);
	pSoldier->condition().extraDexterity() += pSoldier->drugState().magnitude(DRUG_EFFECT_DEX);
	pSoldier->condition().extraAgility() += pSoldier->drugState().magnitude(DRUG_EFFECT_AGI);
	pSoldier->condition().extraWisdom() += pSoldier->drugState().magnitude(DRUG_EFFECT_WIS);

	if ( !pSoldier->drugState().ageTurn() )
	{
		pSoldier->featureFlags().primaryFlags() &= ~SOLDIER_DRUGGED;

		fInterfacePanelDirty = DIRTYLEVEL1;
	}
}

INT8 GetDrunkLevel( SOLDIERTYPE *pSoldier )
{
	if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_HUNGOVER )
	{
		return HUNGOVER;
	}
	
	if ( pSoldier->drugState().alcoholLevel() <= 0.01 )
	{
		return SOBER;
	}
	else if ( pSoldier->drugState().alcoholLevel() <= 0.7 )
	{
		return FEELING_GOOD;
	}
	else if ( pSoldier->drugState().alcoholLevel() <= 2.0 )
	{
		return BORDERLINE;
	}
	
	return DRUNK;
}

// does a merc have a disability/personality, or is he under drugs that simulate this?
BOOLEAN DoesMercHaveDisability( const SOLDIERTYPE *pSoldier, UINT8 aVal )
{
	if ( pSoldier->identity().profile() != NO_PROFILE )
	{
		if ( gMercProfiles[pSoldier->identity().profile()].bDisability == aVal )
			return TRUE;
		
		if ( pSoldier->drugState().hasTemporaryDisability(aVal) )
			return TRUE;

		// Flugente: if disease with severe limitations is active, we can have multiple disabilities
		if ( gGameExternalOptions.fDisease
			&& gGameExternalOptions.fDiseaseSevereLimitations
			&& pSoldier->condition().hasDisability(aVal) )
			return TRUE;
	}

	return FALSE;
}

BOOLEAN DoesMercHavePersonality( SOLDIERTYPE *pSoldier, UINT8 aVal )
{
	// personalities are new trait system only!
	if ( !gGameOptions.fNewTraitSystem )
		return FALSE;

	if ( pSoldier->identity().profile() != NO_PROFILE )
	{
		if ( gMercProfiles[pSoldier->identity().profile()].bCharacterTrait == aVal )
			return TRUE;

		if ( pSoldier->drugState().hasTemporaryPersonality(aVal) )
			return TRUE;
	}

	return FALSE;
}

void HandleAPEffectDueToDrugs( SOLDIERTYPE *pSoldier, INT16 *pubPoints )
{
	*pubPoints += pSoldier->drugState().magnitude(DRUG_EFFECT_AP);
	
	if ( GetDrunkLevel( pSoldier ) == HUNGOVER )
	{
		// Reduce....
		*pubPoints -= HANGOVER_AP_REDUCE;

		if ( *pubPoints < APBPConstants[AP_MINIMUM] )
		{
			*pubPoints = APBPConstants[AP_MINIMUM];
		}
	}
}

void HandleBPEffectDueToDrugs( SOLDIERTYPE *pSoldier, INT16 *psPointReduction )
{
	*psPointReduction -= pSoldier->drugState().magnitude(DRUG_EFFECT_BP);
	
	if ( GetDrunkLevel( pSoldier ) == HUNGOVER )
	{
		// Reduce....
		(*psPointReduction) += HANGOVER_BP_REDUCE;
	}
}

INT32 EffectStatForBeingDrunk( SOLDIERTYPE *pSoldier, INT32 iStat )
{
	return( ( iStat * giDrunkModifier[ GetDrunkLevel( pSoldier ) ] / 100 ) );
}

BOOLEAN MercDruggedOrDrunk( SOLDIERTYPE *pSoldier )
{
	if ( pSoldier->drugState().hasAlcohol() )
		return TRUE;

	if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_DRUGGED )
		return TRUE;

	return FALSE;
}

BOOLEAN MercDrugged( SOLDIERTYPE *pSoldier )
{
	return (pSoldier->featureFlags().primaryFlags() & SOLDIER_DRUGGED);
}

void HourlyDrugUpdate( )
{
	for ( SoldierID ubID = gTacticalStatus.Team[OUR_TEAM].bFirstID; ubID <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++ubID )
	{
		SOLDIERTYPE* soldier =
			GetJa2SoldierRepository().resolve(ubID.i);
		// every hour, we lower our alcohol counter
		if ( soldier->drugState().hasAlcohol() )
		{
			if ( !soldier->drugState().metabolizeAlcohol(0.15f) )
			{
				soldier->featureFlags().secondaryFlags() &= ~SOLDIER_HUNGOVER;
			}
		}
	}
}
