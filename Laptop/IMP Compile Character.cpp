	#include "laptop.h"
	#include "CharProfile.h"
	#include "Utilities.h"
	#include "DEBUG.H"
	#include "WordWrap.h"
	#include "Encrypted File.h"
	#include "GameSettings.h"
	#include "IMPVideoObjects.h"
	#include "IMP MainPage.h"
	#include "IMP Personality Entrance.h"
	#include "IMP Skill Trait.h"
	#include "IMP Attribute Selection.h"
	#include "IMP Portraits.h"
	#include "IMP Compile Character.h"
	#include "soldier profile type.h"
	#include "Soldier Profile.h"
	#include "Animation Data.h"
	#include "random.h"
	#include "LaptopSave.h"
	// These 4 added - SANDRO
	#include "IMP Character Trait.h"
	#include "IMP Disability Trait.h"
	#include "IMP Color Choosing.h"
	#include "IMP Minor Trait.h"
	#include "IMP Voices.h"
	#include "ImpCreationStateModel.h"
	#include "LocalizationInputModel.h"

	#include <algorithm>

#include "IMP Confirm.h"

// how many times should a 'die' be rolled for skills of the same type?

#define HOW_MANY_ROLLS_FOR_SAME_SKILL_CHECK 20
#define IMP_NEED_FOR_SLEEP	7

INT32 AttitudeList[ ATTITUDE_LIST_SIZE ];
INT32 iLastElementInAttitudeList = 0;

INT32 SkillsList[ ATTITUDE_LIST_SIZE ];
INT32 BackupSkillsList[ ATTITUDE_LIST_SIZE ];
INT32 iLastElementInSkillsList = 0;

INT32 PersonalityList[ ATTITUDE_LIST_SIZE ];
INT32 iLastElementInPersonalityList = 0;

extern BOOLEAN fLoadingCharacterForPreviousImpProfile;

// function declarations
BOOLEAN SelectMercFace( void );
void SetMercSkinAndHairColors( void );
BOOLEAN ShouldThisMercHaveABigBody( void );


BOOLEAN CreateACharacterFromPlayerEnteredStats(INT32 profileId)
{
	if (!IsIMPSlotFree(profileId) ||
		!IsValidSelectedIMPPortrait(iPortraitNumber))
	{
		return FALSE;
	}

	CHAR16 fullName[NAME_LENGTH]{};
	CHAR16 nickname[NICKNAME_LENGTH]{};
	if (!LaptopLocalizationModel::CopyText(fullName, pFullName) ||
		!LaptopLocalizationModel::CopyText(nickname, pNickName))
	{
		return FALSE;
	}

	// Kaiden: Seems like as good a place as any to stash this function call to
	// ensure that these lists don't get overwritten or Nulled due to the amount
	// of changes and revisions that have been made to personalities and attitudes.
	CreatePlayersPersonalitySkillsAndAttitude();
	
	LaptopSaveInfo.iIMPIndex = profileId;

	//shadooow: fixes many old values and items remaining when replacing dead/pow IMP
	gMercProfiles[LaptopSaveInfo.iIMPIndex].initialize();

	// copy over full name
	std::copy_n(fullName, NAME_LENGTH,
		gMercProfiles[LaptopSaveInfo.iIMPIndex].zName);

	// the nickname
	std::copy_n(nickname, NICKNAME_LENGTH,
		gMercProfiles[LaptopSaveInfo.iIMPIndex].zNickname);

	// gender
	if ( fCharacterIsMale == TRUE )
	{
		// male
		gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bSex = MALE;
	}
	else
	{
		// female
		gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bSex = FEMALE;
	}
	
	// attributes
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bLifeMax		= ( INT8 )iHealth;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bLife		= ( INT8 )iHealth;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bAgility	= ( INT8 )iAgility;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bStrength	= ( INT8 )iStrength;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bDexterity	= ( INT8 )iDexterity;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bWisdom	 = ( INT8 )iWisdom;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bLeadership = ( INT8 )iLeadership;
	
		// skills
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bMarksmanship = ( INT8 )iMarksmanship;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bMedical		= ( INT8 )iMedical;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bMechanical	= ( INT8 )iMechanical;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bExplosive	= ( INT8 )iExplosives;
	
	// personality
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bDisability = ( INT8 )iPersonality;

	// attitude
	// SANDRO - decide here, if we use the default attitude or the new so called Character Trait for new traits
	if ( gGameOptions.fNewTraitSystem)
		gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bCharacterTrait = ( INT8 )iAttitude;
	else
		gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bAttitude = ( INT8 )iAttitude;

	// Flugente: background
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].usBackground = usBackground;

	// Flugente: sexism, racism etc.
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bRace							= ( INT8 )bRace;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bNationality					= ( INT8 )bNationality;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bAppearance					= ( INT8 )bAppearance;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bAppearanceCareLevel			= ( INT8 )bAppearanceCareLevel;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bRefinement					= ( INT8 )bRefinement;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bRefinementCareLevel			= ( INT8 )bRefinementCareLevel;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bHatedNationality				= ( INT8 )bHatedNationality;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bHatedNationalityCareLevel	= ( INT8 )bHatedNationalityCareLevel;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bRacist						= ( INT8 )bRacist;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bSexist						= ( UINT8 )bSexist;

	// Flugente: voice set used
	gMercProfiles[LaptopSaveInfo.iIMPIndex].usVoiceIndex = iSelectedIMPVoiceSet;

	gMercProfiles[LaptopSaveInfo.iIMPIndex].Type = PROFILETYPE_IMP;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].ubNeedForSleep = IMP_NEED_FOR_SLEEP;
		
	// WDS: Advanced start 
	//gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bExpLevel = gGameExternalOptions.ubIMPStartingLevel;
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bExpLevel = StartingLevelChosen(); // We now choose the starting level on IMP creation - SANDRO

	// set time away
	gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bMercStatus = 0;

	// face
	if (!SelectMercFace())
		return FALSE;

	//  Option for badass added - SANDRO
	if (bBadAssSelected())
		gMercProfiles[ LaptopSaveInfo.iIMPIndex ].uiBodyTypeSubFlags = 1;

	gMercProfiles[LaptopSaveInfo.iIMPIndex].usApproachFactor[0] = 100;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].usApproachFactor[1] = 100;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].usApproachFactor[2] = 100;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].usApproachFactor[3] = 100;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].uiBlinkFrequency = 3000;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].uiExpressionFrequency = 2000;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].bBuddy[0] = 255;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].bBuddy[1] = 255;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].bBuddy[2] = 255;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].bBuddy[3] = 255;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].bBuddy[4] = 255;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].bLearnToLike = 255;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].bHated[0] = 255;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].bHated[1] = 255;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].bHated[2] = 255;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].bHated[3] = 255;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].bHated[4] = 255;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].bLearnToHate = 255;
	return TRUE;
}

void CreatePlayerAttitude( void )
{
	// DLETED UNUSED PART OF THE CODE - SANDRO

	iAttitude =	iChosenCharacterTrait();
}


void AddAnAttitudeToAttitudeList( INT8 bAttitude )
{
	// adds an attitude to attitude list

	if( iLastElementInAttitudeList < ATTITUDE_LIST_SIZE)
	{
		// add element
		AttitudeList[ iLastElementInAttitudeList ] = ( INT32 )bAttitude;

		// increment attitude list counter
		iLastElementInAttitudeList++;
	}
}


void AddSkillToSkillList( INT8 bSkill )
{
	// adds a skill to skills list

	if( iLastElementInSkillsList < ATTITUDE_LIST_SIZE)
	{
		// add element
		SkillsList[ iLastElementInSkillsList ] = ( INT32 )bSkill;

		// increment attitude list counter
		++iLastElementInSkillsList;
	}
}

// Kaiden : Added to clear the list when first visiting the IMP homepage,
// Or for each visit there after.
void ClearAllSkillsList( void )
{
	for ( INT32 iLoop = 0; iLoop < ATTITUDE_LIST_SIZE; ++iLoop ) // SANDRO - better clear everything to be sure
	{
		SkillsList[ iLoop ] = 0;
	}

	iLastElementInSkillsList = 0;
}

void RemoveSkillFromSkillsList( INT32 iIndex )
{
	INT32		iLoop;

	// remove a skill from the index given and shorten the list
	if ( iIndex < iLastElementInSkillsList )
	{
		memset( BackupSkillsList, 0, ATTITUDE_LIST_SIZE * sizeof( INT32 ) );

		// use the backup array to create a version of the array without
		// this index
		for ( iLoop = 0; iLoop < iIndex; iLoop++ )
		{
			BackupSkillsList[ iLoop ] = SkillsList[ iLoop ];
		}
		for ( iLoop = iIndex + 1; iLoop < iLastElementInSkillsList; iLoop++ )
		{
			BackupSkillsList[ iLoop - 1 ] = SkillsList[ iLoop ];
		}
		// now copy this over to the skills list
		memcpy( SkillsList, BackupSkillsList, ATTITUDE_LIST_SIZE * sizeof( INT32 ) );

		// reduce recorded size by 1
		iLastElementInSkillsList--;
	}
}

INT32	FindSkillInSkillsList( INT32 iSkill )
{
	for ( INT32 iLoop = 0; iLoop < iLastElementInSkillsList; iLoop++ )
	{
		if ( SkillsList[ iLoop ] == iSkill )
		{
			return( iLoop );
		}
	}

	return( -1 );
}

void ValidateSkillsList( void )
{
	INT32	iIndex;//, iValue;
	MERCPROFILESTRUCT * pProfile;

	// remove from the generated traits list any traits that don't match
	// the character's skills
	pProfile = &(gMercProfiles[ LaptopSaveInfo.iIMPIndex ]);
	if ( pProfile->bMechanical == 0 )
	{
		// without mechanical, electronics is useless
		// Electronics/Technician check - SANDRO
		if ( gGameOptions.fNewTraitSystem )
		{
			iIndex = FindSkillInSkillsList( TECHNICIAN_NT ); 
			if ( iIndex != -1 )
				pProfile->bMechanical = 1;
		}
		else
		{
			iIndex = FindSkillInSkillsList( ELECTRONICS_OT );
			if ( iIndex != -1 )
				pProfile->bMechanical = 1;
			
			iIndex = FindSkillInSkillsList( LOCKPICKING_OT );
			if ( iIndex != -1 )
				pProfile->bMechanical = 1;
		}
	}

	// SANDRO - added to give 1 medical skill to doctors
	if ( pProfile->bMedical == 0 && gGameOptions.fNewTraitSystem )
	{
		// without medical, Doctor trait is useless
		iIndex = FindSkillInSkillsList( DOCTOR_NT ); 
		if ( iIndex != -1 )
			pProfile->bMedical = 1;
	}

	// SANDRO - added to give 1 explosive skill to dmolitions
	if ( pProfile->bExplosive == 0 && gGameOptions.fNewTraitSystem )
	{
		iIndex = FindSkillInSkillsList( DEMOLITIONS_NT ); 
		if ( iIndex != -1 )
			pProfile->bExplosive = 1;
	}

	// SANDRO - added to give 1 Leadership skill to Squadleaders
	if ( pProfile->bLeadership == 0 && gGameOptions.fNewTraitSystem )
	{
		// without medical, Doctor trait is useless
		iIndex = FindSkillInSkillsList( SQUADLEADER_NT ); 
		if ( iIndex != -1 )
			pProfile->bLeadership = 1;
	}

	if ( pProfile->bMarksmanship == 0 )
	{
		// without marksmanship, the following traits are useless:
		// auto weapons, heavy weapons, sniper, ranger, gunslinger

		if ( gGameOptions.fNewTraitSystem ) // old/new traits - SANDRO 
		{
			iIndex = FindSkillInSkillsList( AUTO_WEAPONS_NT );
			if( iIndex != -1 )
				pProfile->bMarksmanship = 1;

			iIndex = FindSkillInSkillsList( HEAVY_WEAPONS_NT );
			if( iIndex != -1 )
				pProfile->bMarksmanship = 1;

			iIndex = FindSkillInSkillsList( SNIPER_NT );
			if( iIndex != -1 )
				pProfile->bMarksmanship = 1;

			iIndex = FindSkillInSkillsList( RANGER_NT );
			if( iIndex != -1 )
				pProfile->bMarksmanship = 1;

			iIndex = FindSkillInSkillsList( GUNSLINGER_NT );
			if( iIndex != -1 )
				pProfile->bMarksmanship = 1;
		}
		else
		{
			iIndex = FindSkillInSkillsList( AUTO_WEAPS_OT );
			if( iIndex != -1 )
			{
				//RemoveSkillFromSkillsList( iIndex );
				//iIndex = FindSkillInSkillsList( AUTO_WEAPS_OT );
				pProfile->bMarksmanship = 1;
			}
			iIndex = FindSkillInSkillsList( HEAVY_WEAPS_OT );
			if( iIndex != -1 )
			{
				//RemoveSkillFromSkillsList( iIndex );
				//iIndex = FindSkillInSkillsList( HEAVY_WEAPONS_NT );
				pProfile->bMarksmanship = 1;
			}
		}
	}
}

void CreatePlayerSkills( void )
{
	ValidateSkillsList();

	// Kaiden: This section was added in it's place:
	// SANDRO - Note: this is actually used only for old trait system
	if( iLastElementInSkillsList > 0 )
	{
		iSkillA = SkillsList[ 0 ];
	}
	if( iLastElementInSkillsList > 1 )
	{
		iSkillB = SkillsList[ 1 ];
	}
}


void AddAPersonalityToPersonalityList( INT8 bPersonlity )
{
	// CJC, Oct 26 98: prevent personality list from being generated
	// because no dialogue was written to support PC personality quotes

	// BUT we can manage this for PSYCHO okay

	//Kaiden: But we're going to try it anyway screw dialoge.
	// Commenting out the below if test

	//if ( bPersonlity != PSYCHO )
	//{
	//	return;
	//}

	// will add a persoanlity to persoanlity list
	if( iLastElementInPersonalityList < ATTITUDE_LIST_SIZE)
	{
		// add element
		PersonalityList[ iLastElementInPersonalityList ] = ( INT32 )bPersonlity;

		// increment attitude list counter
		iLastElementInPersonalityList++;
	}
}

void CreatePlayerPersonality( void )
{
	// DELETED UNUSED PART OF THE CODE - SANDRO

	iPersonality = iChosenDisabilityTrait();
}

void CreatePlayersPersonalitySkillsAndAttitude( void )
{
	// creates personality, skills and attitudes from curretly built list

	// personality
	CreatePlayerPersonality( );

	// skills are now created later after stats have been chosen
	//CreatePlayerSkills( );

	// attitude
	CreatePlayerAttitude( );
}

void ResetSkillsAttributesAndPersonality( void )
{
	std::fill_n(PersonalityList, ATTITUDE_LIST_SIZE, 0);
	std::fill_n(SkillsList, ATTITUDE_LIST_SIZE, 0);
	std::fill_n(BackupSkillsList, ATTITUDE_LIST_SIZE, 0);
	std::fill_n(AttitudeList, ATTITUDE_LIST_SIZE, 0);
	iLastElementInPersonalityList = 0;
	iLastElementInSkillsList = 0;
	iLastElementInAttitudeList = 0;
}

void ResetIncrementCharacterAttributes( void )
{
	// this resets any increments due to character generation

	// attributes
	iAddStrength = 0;
	iAddDexterity = 0;
	iAddWisdom = 0;
	iAddAgility = 0;
	iAddHealth = 0;
	iAddLeadership = 0;

	// skills
	iAddMarksmanship = 0;
	iAddExplosives = 0;
	iAddMedical = 0;
	iAddMechanical = 0;
}

BOOLEAN SelectMercFace( void )
{
	if (!LaptopImpModel::IsIndexInRange(NUM_PROFILES, LaptopSaveInfo.iIMPIndex) ||
		!IsValidSelectedIMPPortrait(iPortraitNumber))
	{
		return FALSE;
	}

	// Select the face and copy its data-driven animation offsets.

	gMercProfiles[LaptopSaveInfo.iIMPIndex].ubFaceIndex = (UINT8)iPortraitNumber;

	// eyes
	gMercProfiles[LaptopSaveInfo.iIMPIndex].usEyesX = gIMPValues[iPortraitNumber].uiEyeXPositions;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].usEyesY = gIMPValues[iPortraitNumber].uiEyeYPositions;

	// mouth
	gMercProfiles[LaptopSaveInfo.iIMPIndex].usMouthX = gIMPValues[iPortraitNumber].uiMouthXPositions;
	gMercProfiles[LaptopSaveInfo.iIMPIndex].usMouthY = gIMPValues[iPortraitNumber].uiMouthYPositions;

	// set merc skins and hair color
	SetMercSkinAndHairColors( );
	return TRUE;
}

void SetMercSkinAndHairColors( void )
{
	// DELETED UNUSED PART OF THE CODE - SANDRO

	// skin strings
	STR sSkinStrings[]={
		"PINKSKIN",
		"TANSKIN",
		"DARKSKIN",
		"BLACKSKIN",
	};

	// the hair colors
	STR sHairStrings[]={
		"BROWNHEAD",
		"BLACKHEAD",
		"WHITEHEAD",
		"BLONDHEAD",
		"REDHEAD",
	};

	// shirts
	STR sShirtStrings[]={
		"WHITEVEST",
		"GYELLOWSHIRT",
		"YELLOWVEST",
		"greyVEST",
		"BROWNVEST",
		"PURPLESHIRT",
		"BLUEVEST",
		"JEANVEST",
		"GREENVEST",
		"REDVEST",
		"BLACKSHIRT",
	};//shirts


	// shirts
	STR sPantStrings[]={
		"BLUEPANTS",
		"BLACKPANTS",
		"JEANPANTS",
		"TANPANTS",
		"BEIGEPANTS",
		"GREENPANTS",
	};//pants

	// DELETED UNUSED PART OF THE CODE - SANDRO

	strcpy( gMercProfiles[ LaptopSaveInfo.iIMPIndex ].HAIR, sHairStrings[ iChosenHair() ] );
	strcpy( gMercProfiles[ LaptopSaveInfo.iIMPIndex ].SKIN, sSkinStrings[ iChosenSkin() ] );
	strcpy( gMercProfiles[ LaptopSaveInfo.iIMPIndex ].PANTS, sPantStrings[ iChosenPants() ] );
	strcpy( gMercProfiles[ LaptopSaveInfo.iIMPIndex ].VEST, sShirtStrings[ iChosenShirt() ] );
}


void HandleMercStatsForChangesInFace( )
{
	if ( fLoadingCharacterForPreviousImpProfile )
	{
		return;
	}

	//add the skills (major) to the skills list
	AddSelectedSkillsToSkillsList();

	if ( gGameOptions.fNewTraitSystem ) // SANDRO - add also minor traits
		AddSelectedMinorTraitsToSkillsList();

	// now figure out skills
	CreatePlayerSkills();

	// body type
	if ( fCharacterIsMale	)
	{
	// male
		// big or regular
		// Madd - don't override the skills - override the body type instead
		// this should not happen any more, we don't even offer the martial arts to big mercs - SANDRO
		//if( ShouldThisMercHaveABigBody() && iSkillA != MARTIALARTS_OT && iSkillB != MARTIALARTS_OT )
		if( ShouldThisMercHaveABigBody() ) // still added a safety check - SANDRO
		{
			if (!gGameOptions.fNewTraitSystem) // SANDRO - traits
			{
				if (iSkillA == MARTIALARTS_OT )
					iSkillA = HANDTOHAND_OT;
				if (iSkillB == MARTIALARTS_OT )
					iSkillB = HANDTOHAND_OT;
			}

			gMercProfiles[ LaptopSaveInfo.iIMPIndex ].ubBodyType = BIGMALE;
		}
		else
		{
			gMercProfiles[ LaptopSaveInfo.iIMPIndex ].ubBodyType = REGMALE;
		}
	}
	else
	{
		// female
		gMercProfiles[ LaptopSaveInfo.iIMPIndex ].ubBodyType = REGFEMALE;

		if (!gGameOptions.fNewTraitSystem) // SANDRO - traits
		{
			if( iSkillA == MARTIALARTS_OT )
				iSkillA = HANDTOHAND_OT;
			if( iSkillB == MARTIALARTS_OT )
				iSkillB = HANDTOHAND_OT;
		}
	}

	if (gGameOptions.fNewTraitSystem)
	{
		for ( UINT8 ubCnt = 0; ubCnt < gSkillTraitValues.ubMaxNumberOfTraitsForIMP; ubCnt++ )
		{
			gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bSkillTraits[ ubCnt ] = ( INT8 )SkillsList[ ubCnt ];
		}
	}
	else
	{
		// skill trait
		gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bSkillTraits[0] =	( INT8 )iSkillA;
		gMercProfiles[ LaptopSaveInfo.iIMPIndex ].bSkillTraits[1] = ( INT8 )iSkillB;
	}
}

BOOLEAN ShouldThisMercHaveABigBody( void )
{
	// We can now choose this ourselves - SANDRO
	return( bBigBodySelected() );
}
