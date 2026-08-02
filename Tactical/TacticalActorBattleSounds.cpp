#include "TacticalActorBattleSounds.h"

#include "TacticalActorStateFlags.h"

#include "TacticalActorOrientation.h"
#include "TacticalActorWorldPlacement.h"
#include "Soldier Functions.h"
#include "TacticalActorAnimationFootprint.h"
#include "TacticalActorAnimationFrames.h"
#include "TacticalActorConsumables.h"
#include "TacticalActorEquipment.h"
#include "TacticalActorModifiers.h"
#include "TacticalActorRadio.h"
#include "TacticalActorRobotics.h"
#include "TacticalActorSkills.h"
#include "TacticalActorSpotting.h"
#include "TacticalActorTurnBudget.h"
#include "TacticalActorTurnMaintenance.h"
#include "TacticalActorTurncoats.h"
#include "TacticalActorAssignments.h"
#include "TacticalActorConditions.h"
#include "TacticalActorCombatReactions.h"
#include "TacticalActorRecovery.h"
#include "TacticalActorCovertOps.h"
#include "TacticalActorDisease.h"
#include "TacticalActorDragging.h"
#include "TacticalActorAiBehavior.h"
#include "TacticalActorDamageQueue.h"
#include "TacticalActorDamageFeedback.h"
#include "TacticalActorLongActions.h"
#include "TacticalActorLighting.h"
#include "TacticalActorMedicalSession.h"
#include "TacticalActorMedicalServices.h"
#include "TacticalActorMobility.h"
#include "TacticalActorProfileClassification.h"
#include "TacticalActorRangedActions.h"
#include "TacticalActorRouteExecution.h"
#include "TacticalActorWeaponHandling.h"
#include "SoldierRepository.h"
#include "TacticalWorldAdapter.h"
#include "builddefines.h"
#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include "WCheck.h"
#include "stdlib.h"
#include "DEBUG.H"
#include "MemMan.h"
#include "Overhead Types.h"
#include "Animation Cache.h"
#include "Animation Data.h"
#include "Animation Control.h"
#define _USE_MATH_DEFINES // for C
#include <math.h>
#include "PATHAI.H"
#include "random.h"
#include "worldman.h"
#include "Isometric Utils.h"
#include "renderworld.h"
#include "render_palette_registry.h"
#include "video.h"
#include "Points.h"
#include "Sound Control.h"
#include "Weapons.h"
#include "shading.h"
#include "Handle UI.h"
#include "Soldier Ani.h"
#include "Event Pump.h"
#include "opplist.h"
#include "ai.h"
#include "Interface.h"
#include "lighting.h"
#include "faces.h"
#include "Soldier Profile.h"
#include "Campaign.h"
#include "Soldier macros.h"
#include "english.h"
#include "Squads.h"
#ifdef NETWORKED
#include "Networking.h"
#include "NetworkEvent.h"
#endif
#include "Structure Wrap.h"
#include "Items.h"
#include "soundman.h"
#include "Utilities.h"
#include "strategic.h"
#include "soldier tile.h"
#include "Smell.h"
#include "Keys.h"
#include "Dialogue Control.h"
#include "rt time defines.h"
#include "Quests.h"
#include "message.h"
#include "NPC.h"
#include "SkillCheck.h"
#include "Handle Doors.h"
#include "interface Dialogue.h"
#include "SmokeEffects.h"
#include	"GameSettings.h"
#include "Tile Animation.h"
#include "ShopKeeper Interface.h"
#include "Vehicles.h"
#include "Rotting Corpses.h"
#include "Interface Control.h"
#include "strategicmap.h"
#include "Morale.h"
#include "Drugs And Alcohol.h"
#include "Boxing.h"
#include "overhead map.h"
#include "Map Information.h"
#include "environment.h"
#include "Game Clock.h"
#include "Explosion Control.h"
#include "Buildings.h"
#include "Text.h"
#include "Strategic Merc Handler.h"
#include "Campaign Types.h"
#include "Strategic Status.h"
#include "Civ Quotes.h"
#include "Debug Control.h"
#include "LOS.h" // added by SANDRO
#include "CampaignStats.h"		// added by Flugente
#include "Interface Panels.h"
#include "Queen Command.h"		// added by Flugente
#include "Town Militia.h"		// added by Flugente
#include "Auto Bandage.h"		// added by Flugente
#include "Facilities.h"			// added by Flugente
#include "Cheats.h"				// added by Flugente
#include "MilitiaIndividual.h"	// added by Flugente
#include "Arms Dealer Init.h"	// added by Flugente for armsDealerInfo[]
#include "LuaInitNPCs.h"		// added by Flugente
#include "qarray.h"				// added by Flugente
#include "GameInitOptionsScreen.h"
#include "fresh_header.h"
#include "IMP Skill Trait.h"	// added by Flugente
#include "Food.h"				// added by Flugente
#include "Tactical Save.h"		// added by Flugente for AddItemsToUnLoadedSector()
#include "LightEffects.h"		// added by Flugente for CreatePersonalLight()
#include "DynamicDialogue.h"	// added by Flugente for HandleDynamicOpinions()
#include "Strategic Town Loyalty.h"		// added by Flugente for gTownLoyalty
#include "Rebel Command.h"
#include "Simulation Command Legacy.h"
#include "Simulation Commands.h"
#include "Strategic Movement.h"
#include "StrategicSquadHost.h"
#include "TacticalEntityHost.h"
#include "VehiclePassengerHost.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>


#define LOW_MORALE_BATTLE_SND_THREASHOLD 35
#define MIN_SUBSEQUENT_SNDS_DELAY 2000

typedef struct
{
	CHAR8				zName[20];
	BOOLEAN			fPreload;
	BOOLEAN			fBadGuy;
	BOOLEAN			fDontAllowTwoInRow;
	BOOLEAN			fStopDialogue;

} BATTLESNDS_STRUCT;


BATTLESNDS_STRUCT	 gBattleSndsData[] =
{
	"ok", 1, 1, 1, 2,
	"cool", 1, 0, 1, 0,
	"curse", 1, 1, 1, 0,
	"hit", 1, 1, 1, 1,
	"laugh", 1, 1, 1, 0,
	"attn", 1, 0, 1, 0,
	"dying", 1, 1, 1, 1,
	"humm", 0, 0, 1, 1,
	"noth", 0, 0, 1, 1,
	"gotit", 0, 0, 1, 1,
	"lmok", 1, 0, 1, 2,
	"lmattn", 1, 0, 1, 0,
	"locked", 0, 0, 1, 0,
	"enem", 1, 1, 1, 0,
	"punch", 1, 0, 0, 0,
	"knife", 1, 0, 0, 0,
};

// Flugente: a little helper struct for npc sounds
typedef struct
{
	CHAR8			zName[20];
} BATTLESNDS_NPC_HELPER_STRUCT;

BATTLESNDS_NPC_HELPER_STRUCT	 gBattleSndsNpcHelperData[] =
{
	"bad",
	"kid",
	"zombie",
};

extern BOOLEAN IsMercSayingDialogue( UINT8 ubProfileID );

// Flugente: store how many sounds we've found for each npc type
#define BATTLESOUND_NPC_TYPES		3
#define BATTLESOUND_NPC_SOUNDSETS	8
UINT32 numBattleSounds_Npc[BATTLESOUND_NPC_TYPES][BATTLESOUND_NPC_SOUNDSETS][NUM_MERC_BATTLE_SOUNDS];
bool BattleSoundSearchDone_Npc[BATTLESOUND_NPC_TYPES][BATTLESOUND_NPC_SOUNDSETS][NUM_MERC_BATTLE_SOUNDS];

bool TacticalActorBattleSounds::playWithCode(TacticalActor& subject, UINT8 ubBattleSoundID, INT8 bSpecialCode)
{
	// pSoldier stands in for the subject pointer because a vehicle's sound is
	// spoken by one of the mercenaries inside it.
	SGPFILENAME		zFilename;
	SGPFILENAME		zFilename_Used;
	SOUNDPARMS		spParms;
	UINT8				ubSoundID = 0;
	UINT32				uiSoundID;
	UINT32				iFaceIndex;
	BOOLEAN				fDoSub = FALSE;
	INT32					uiSubSoundID = 0;
	TacticalActor*		pSoldier = &subject;

	// DOUBLECHECK RANGE
	CHECKF( ubBattleSoundID < NUM_MERC_BATTLE_SOUNDS );

	if ( (subject.status().flags() & SOLDIER_VEHICLE) )
	{
		// Pick a passenger from vehicle....
		//pSoldier = PickRandomPassengerFromVehicle( &subject );
		// anv: as vehicles can be controlled, get a driver
		pSoldier = GetDriver( subject.vehicleState().tacticalVehicleId() );

		if ( pSoldier == NULL )
		{
			return(FALSE);
		}
	}

	// If a death sound, and we have already done ours...
	if ( ubBattleSoundID == BATTLE_SOUND_DIE1 )
	{
		if ( pSoldier->dialogue().deathBattleSoundUsed() )
		{
			return(TRUE);
		}
	}

	// Are we mute?
	if ( pSoldier->status().flags() & SOLDIER_MUTE )
	{
		return(FALSE);
	}

	//	uiTimeSameBattleSndDone

	// If we are a creature, etc, pick a better sound...
	if ( ubBattleSoundID == BATTLE_SOUND_HIT1 )
	{
		switch ( pSoldier->identity().bodyType() )
		{
		case COW:

			fDoSub = TRUE;
			uiSubSoundID = COW_HIT_SND;
			break;

		case YAF_MONSTER:
		case YAM_MONSTER:
		case ADULTFEMALEMONSTER:
		case AM_MONSTER:

			fDoSub = TRUE;

			if ( Random( 2 ) == 0 )
			{
				uiSubSoundID = ACR_DIE_PART1;
			}
			else
			{
				uiSubSoundID = ACR_LUNGE;
			}
			break;

		case INFANT_MONSTER:

			fDoSub = TRUE;
			uiSubSoundID = BCR_SHRIEK;
			break;

		case QUEENMONSTER:

			fDoSub = TRUE;
			uiSubSoundID = LQ_SHRIEK;
			break;

		case LARVAE_MONSTER:

			fDoSub = TRUE;
			uiSubSoundID = BCR_SHRIEK;
			break;

		case BLOODCAT:

			fDoSub = TRUE;
			uiSubSoundID = BLOODCAT_HIT_1;
			break;

		case ROBOTNOWEAPON:

			fDoSub = TRUE;
			uiSubSoundID = (UINT32)(S_METAL_IMPACT1 + Random( 2 ));
			break;
		}
	}

	if ( ubBattleSoundID == BATTLE_SOUND_DIE1 )
	{
		switch ( pSoldier->identity().bodyType() )
		{
		case COW:

			fDoSub = TRUE;
			uiSubSoundID = COW_DIE_SND;
			break;

		case YAF_MONSTER:
		case YAM_MONSTER:
		case ADULTFEMALEMONSTER:
		case AM_MONSTER:

			fDoSub = TRUE;
			uiSubSoundID = CREATURE_FALL_PART_2;
			break;

		case INFANT_MONSTER:

			fDoSub = TRUE;
			uiSubSoundID = BCR_DYING;
			break;

		case LARVAE_MONSTER:

			fDoSub = TRUE;
			uiSubSoundID = LCR_RUPTURE;
			break;

		case QUEENMONSTER:

			fDoSub = TRUE;
			uiSubSoundID = LQ_DYING;
			break;

		case BLOODCAT:

			fDoSub = TRUE;
			uiSubSoundID = BLOODCAT_DIE_1;
			break;

		case ROBOTNOWEAPON:

			fDoSub = TRUE;
			uiSubSoundID = (UINT32)(EXPLOSION_1);
			PlayJA2Sample( ROBOT_DEATH, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );
			break;
		}
	}

	// OK. any other sound, not hits, robot makes a beep
	if ( pSoldier->identity().bodyType() == ROBOTNOWEAPON && !fDoSub )
	{
		fDoSub = TRUE;
		if ( ubBattleSoundID == BATTLE_SOUND_ATTN1 )
		{
			uiSubSoundID = ROBOT_GREETING;
		}
		else
		{
			uiSubSoundID = ROBOT_BEEP;
		}
	}

	if ( fDoSub )
	{
		if ( GetCurrentScreen() != GAME_SCREEN )
		{
			PlayJA2Sample( uiSubSoundID, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );
		}
		else
		{
			PlayJA2Sample( uiSubSoundID, RATE_11025, SoundVolume( (UINT8)CalculateSpeechVolume( HIGHVOLUME ), pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
		}

		return(TRUE);
	}

	// Check if this is the same actor we just played...
	if ( pSoldier->dialogue().previousBattleSound() == ubBattleSoundID && gBattleSndsData[ubBattleSoundID].fDontAllowTwoInRow )
	{
		// Are we below the min delay?
		if ( (GetJA2Clock( ) - pSoldier->dialogue().repeatedBattleSoundAt()) < MIN_SUBSEQUENT_SNDS_DELAY )
		{
			return(TRUE);
		}
	}

	// If a battle snd is STILL playing....
	if ( SoundIsPlaying( pSoldier->dialogue().activeBattleSound() ) )
	{
		// We can do a few things here....
		// Is &subject a crutial one...?
		if ( gBattleSndsData[ubBattleSoundID].fStopDialogue == 1 )
		{
			// Stop playing origonal
			SoundStop( pSoldier->dialogue().activeBattleSound() );
		}
		else
		{
			// Skip &subject one...
			return(TRUE);
		}
	}

	// If we are talking now....
	if ( IsMercSayingDialogue( pSoldier->identity().profile() ) )
	{
		// We can do a couple of things now...
		if ( gBattleSndsData[ubBattleSoundID].fStopDialogue == 1 )
		{
			// Stop dialigue...
			DialogueAdvanceSpeech( );
		}
		else if ( gBattleSndsData[ubBattleSoundID].fStopDialogue == 2 )
		{
			// Skip battle snd...
			return(TRUE);
		}
	}

	// Save &subject one we're doing...
	pSoldier->dialogue().recordBattleSound(ubBattleSoundID, GetJA2Clock());

	//if the sound to be played is a confirmation, check to see if we are to play it
	if ( ubBattleSoundID == BATTLE_SOUND_OK1 )
	{
		if ( gGameSettings.fOptions[TOPTION_MUTE_CONFIRMATIONS] )
			return( TRUE );

		//ddd
		if ( !Chance( gGameExternalOptions.iChanceSayAnnoyingPhrase ) )
			return( TRUE );
	}

	// Adjust based on morale...
	if ( pSoldier->morale().morale() < LOW_MORALE_BATTLE_SND_THREASHOLD )
	{
		if ( ubBattleSoundID == BATTLE_SOUND_OK1 )
			ubBattleSoundID = BATTLE_SOUND_LOWMARALE_OK1;
		else if ( ubBattleSoundID == BATTLE_SOUND_ATTN1 )
			ubBattleSoundID = BATTLE_SOUND_LOWMARALE_ATTN1;
	}

	ubSoundID = ubBattleSoundID;

	// OK, build file and play!
	if ( pSoldier->identity().profile() != NO_PROFILE )
	{
		// Flugente: For the voice set itself, use &subject number
		UINT32 usVoiceSet = gMercProfiles[pSoldier->identity().profile()].usVoiceIndex;

		// Flugente: check if perhaps a sound with a higher number is present, if so, increase number of found sounds
		// if not, mark that &subject search is finished (no need to constantly check for sounds)
		// This way we don't have to add new xml data, but can still use any soundfile we add (provided we numbered it correctly)
		// Soundfiles are named just like before, with increasing numbers
		// Due to legacy reasons, the first sound can either have a '1' at the end (212_OK1.xx) or no number at all (212_HUMM.xxx)
		// Otherwise we'd have to rename quite a lot of vanilla files
		while ( !QuoteExp[pSoldier->identity().profile()].BattleSoundSearchDone[ubSoundID] )
		{
			// at least one sound exists (if not, we use a fallback solution anyway)
			QuoteExp[pSoldier->identity().profile()].numBattleSounds[ubSoundID] = max( 1, QuoteExp[pSoldier->identity().profile()].numBattleSounds[ubSoundID] );

			UINT16 numsounds = QuoteExp[pSoldier->identity().profile()].numBattleSounds[ubSoundID];

			// check: is there a sound with a bigger number?
			sprintf( zFilename, "BATTLESNDS\\%03d_%s%d", usVoiceSet, gBattleSndsData[ubSoundID].zName, numsounds + 1 );

			if ( SoundFileExists( zFilename, zFilename_Used ) )
			{
				QuoteExp[pSoldier->identity().profile()].numBattleSounds[ubSoundID]++;
			}
			else
			{
				QuoteExp[pSoldier->identity().profile()].BattleSoundSearchDone[ubSoundID] = TRUE;
			}
		}

		UINT16 soundtoplay = 1 + Random( QuoteExp[pSoldier->identity().profile()].numBattleSounds[ubSoundID] );

		if ( soundtoplay > 1 )
		{
			sprintf( zFilename, "BATTLESNDS\\%03d_%s%d", usVoiceSet, gBattleSndsData[ubSoundID].zName, soundtoplay );
		}
		else
		{
			sprintf( zFilename, "BATTLESNDS\\%03d_%s", usVoiceSet, gBattleSndsData[ubSoundID].zName );

			// due to legacy reasons, we both have to check for versions with '1' and without a number here
			if ( !SoundFileExists( zFilename, zFilename_Used ) )
				sprintf( zFilename, "BATTLESNDS\\%03d_%s%d", usVoiceSet, gBattleSndsData[ubSoundID].zName, 1 );
		}

		if ( !SoundFileExists( zFilename, zFilename_Used ) )
		{
			// OK, temp build file...
			if ( pSoldier->identity().bodyType() == REGFEMALE )
			{
				sprintf( zFilename, "BATTLESNDS\\f_%s", gBattleSndsData[ubSoundID].zName );
			}
			else
			{
				sprintf( zFilename, "BATTLESNDS\\m_%s", gBattleSndsData[ubSoundID].zName );
			}
		}
	}
	else
	{
		// Check if we can play &subject!
		if ( !gBattleSndsData[ubSoundID].fBadGuy )
		{
			return(FALSE);
		}

		int entrynum = 0;
		if ( TacticalActorConditions::isZombie(*pSoldier) ) // Madd: add zombie sounds
		{
			entrynum = 2;
			pSoldier->dialogue().battleSoundSet() = 0;		// atm only one soundset for zombies
		}
		else if ( pSoldier->identity().bodyType() == HATKIDCIV || pSoldier->identity().bodyType() == KIDCIV )
			entrynum = 1;

		// Flugente: check if perhaps a sound with a higher number is present, if so, increase number of found sounds
		// if not, mark that &subject search is finished (no need to constantly check for sounds)
		// This way we don't have to add new xml data, but can still use any soundfile we add (provided we numbered it correctly)
		// Soundfiles are named just like before, with increasing numbers
		// There are three categories for npc sound files here: bad (for ordinary humans), kid for kids and zombie
		// Due to legacy reasons, the first sound can either have a '1' at the end (212_OK1.xx) or no number at all (212_HUMM.xxx)
		// Otherwise we'd have to rename quite a lot of vanilla files
		while ( !BattleSoundSearchDone_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID] )
		{
			// at least one sound exists (if not, we use a fallback solution anyway)
			numBattleSounds_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID] = max( 1, numBattleSounds_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID] );

			UINT32 numsounds = numBattleSounds_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID];

			// check: is there a sound with a bigger number?
			sprintf( zFilename, "BATTLESNDS\\%s%d_%s%d", gBattleSndsNpcHelperData[entrynum].zName, pSoldier->dialogue().battleSoundSet(), gBattleSndsData[ubSoundID].zName, numsounds + 1 );

			if ( SoundFileExists( zFilename, zFilename_Used ) )
			{
				numBattleSounds_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID]++;
			}
			else
			{
				BattleSoundSearchDone_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID] = true;
			}
		}

		UINT32 soundtoplay = 1 + Random( numBattleSounds_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID] );

		sprintf( zFilename, "BATTLESNDS\\%s%d_%s%d", gBattleSndsNpcHelperData[entrynum].zName, pSoldier->dialogue().battleSoundSet(), gBattleSndsData[ubSoundID].zName, soundtoplay );

		// due to legacy reasons, we both have to check for versions with '1' and without a number here
		if ( !SoundFileExists( zFilename, zFilename_Used ) )
		{
			sprintf( zFilename, "BATTLESNDS\\%s%d_%s", gBattleSndsNpcHelperData[entrynum].zName, pSoldier->dialogue().battleSoundSet(), gBattleSndsData[ubSoundID].zName );
		}
	}

	if ( !SoundFileExists( zFilename, zFilename_Used ) )
		return FALSE;

	// Play sound!
	memset( &spParms, 0xff, sizeof(SOUNDPARMS) );

	spParms.uiSpeed = RATE_11025;
	//spParms.uiVolume = CalculateSpeechVolume( pSoldier->dialogue().vocalVolume() );

	spParms.uiVolume = (INT8)CalculateSpeechVolume( HIGHVOLUME );

	// ATE: Reduce volume for OK sounds...
	// ( Only for all-moves or multi-selection cases... )
	if ( bSpecialCode == BATTLE_SND_LOWER_VOLUME )
	{
		spParms.uiVolume = (INT8)CalculateSpeechVolume( MIDVOLUME );
	}

	// If we are an enemy.....reduce due to volume
	if ( pSoldier->roster().team() != gbPlayerNum )
	{
		if( ubBattleSoundID == BATTLE_SOUND_CURSE1 )
			spParms.uiVolume = (INT8)CalculateSpeechVolume( MIDVOLUME );
		else if ( GetSpeechVolume() != 0 )
			spParms.uiVolume = SoundVolume( (UINT8)spParms.uiVolume, pSoldier->position().gridNo() );
	}

	spParms.uiLoop = 1;
	spParms.uiPan = SoundDir( pSoldier->position().gridNo() );
	spParms.uiPriority = GROUP_PLAYER;

	if ( (uiSoundID = SoundPlay( zFilename_Used, &spParms )) == SOUND_ERROR )
	{
		return(FALSE);
	}
	else
	{
		pSoldier->dialogue().activeBattleSound() = uiSoundID;

		if ( pSoldier->identity().profile() != NO_PROFILE )
		{
			// Get soldier's face ID
			iFaceIndex = pSoldier->renderBindings().faceIndex();

			// Check face index
			if ( iFaceIndex != -1 )
			{
				ExternSetFaceTalking( iFaceIndex, uiSoundID );
			}
		}
	}

	return( TRUE );
}

bool TacticalActorBattleSounds::play(TacticalActor& subject, UINT8 ubBattleSoundID)
{
	if (ubBattleSoundID >= NUM_MERC_BATTLE_SOUNDS)
		return FALSE;

	if ( subject.skillState().cooldown(SOLDIER_COOLDOWN_CRYO) )
		return FALSE;

	// We WANT to play some RIGHT AWAY.....
	if ( gBattleSndsData[ubBattleSoundID].fStopDialogue == 1 || (subject.identity().profile() == NO_PROFILE) || InOverheadMap( ) )
	{
		return(TacticalActorBattleSounds::playWithCode(subject,  ubBattleSoundID, 0 ));
	}

	// So here, only if we were currently saying dialogue.....
	if ( !IsMercSayingDialogue( subject.identity().profile() ) )
	{
		return(TacticalActorBattleSounds::playWithCode(subject,  ubBattleSoundID, 0 ));
	}

	// OK, queue it up otherwise!
	TacticalCharacterDialogueWithSpecialEvent( &subject, 0, DIALOGUE_SPECIAL_EVENT_DO_BATTLE_SND, ubBattleSoundID, 0 );

	return(TRUE);
}


bool TacticalActorBattleSounds::preload(TacticalActor& actor, bool remove)
{
	if (!actor.roster().active())
		return false;

	for ( UINT32 cnt = 0; cnt < NUM_MERC_BATTLE_SOUNDS; ++cnt )
	{
		// OK, build file and play!
		if ( actor.identity().profile() != NO_PROFILE )
		{
			if ( gBattleSndsData[cnt].fPreload )
			{
				if ( remove )
				{
					SoundUnlockSample( gBattleSndsData[cnt].zName );
				}
				else
				{
					SoundLockSample( gBattleSndsData[cnt].zName );
				}
			}
		}
		else
		{
			if ( gBattleSndsData[cnt].fPreload && gBattleSndsData[cnt].fBadGuy )
			{
				if ( remove )
				{
					SoundUnlockSample( gBattleSndsData[cnt].zName );
				}
				else
				{
					SoundLockSample( gBattleSndsData[cnt].zName );
				}
			}
		}
	}

	return true;
}

BOOLEAN PreloadSoldierBattleSounds(TacticalActor* actor, BOOLEAN remove)
{
	return actor != nullptr &&
		TacticalActorBattleSounds::preload(*actor, remove != FALSE);
}
