#include "TacticalActorAiBehavior.h"
#include "TacticalActorEquipment.h"
	#include "sgp.h"
#include "TacticalActorConditions.h"
#include "TacticalActorCovertOps.h"
#include "TacticalActorDragging.h"
#include "TacticalActorModifiers.h"
#include "TacticalWorldAdapter.h"
	#include "Isometric Utils.h"
	#include "Overhead.h"
	#include "Event Pump.h"
	#include "random.h"
	#include "Overhead Types.h"
	#include "opplist.h"
	#include "ai.h"
	#include "Font Control.h"
	#include "Animation Control.h"
	#include "LOS.h"
	#include "fov.h"
	#include "Dialogue Control.h"
	#include "lighting.h"
	#include "environment.h"
	#include "interface Dialogue.h"
	#include "message.h"
	#include "Soldier Profile.h"
	#include "TeamTurns.h"
	#include "Interactive Tiles.h"
	#include "Render Fun.h"
	#include "Text.h"
	#include "Timer Control.h"
	#include "Soldier macros.h"
	#include "Soldier Functions.h"
	#include "Handle UI.h"
	#include "Keys.h"
	#include "Campaign.h"
	#include "Soldier Init List.h"
	#include "Music Control.h"
	#include "strategicmap.h"
	#include "Quests.h"
	#include "worldman.h"
	#include "SkillCheck.h"
	#include "GameSettings.h"
	#include "Smell.h"
	#include "Game Clock.h"
	#include "Civ Quotes.h"
	#include "Sound Control.h"
	#include "Interface.h"
	#include "Explosion Control.h"//dnl ch40 200909
	#include "Vehicles.h"
#include "GameInitOptionsScreen.h"
#include "connect.h"
#include "../ModularizedTacticalAI/include/Plan.h"
#include "../ModularizedTacticalAI/include/PlanFactoryLibrary.h"
#include "../ModularizedTacticalAI/include/AbstractPlanFactory.h"
#include "Ja25_Tactical.h"
#include "Meanwhile.h"
#include "GameContext.h"
#include "CampaignProfileCodes.h"
#include "SoldierRepository.h"



//rain
//#define VIS_DIST_DECREASE_PER_RAIN_INTENSITY 20
//end rain

#define WE_SEE_WHAT_MILITIA_SEES_AND_VICE_VERSA

extern void SetSoldierAniSpeed( TacticalActor *pSoldier );
// HEADROCK HAM 3.6: Moved to header
//void MakeBloodcatsHostile( void );

void OurNoise( SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel, UINT8 ubTerrType, UINT8 ubVolume,	UINT8 ubNoiseType, STR16 zNoiseMessage );
void TheirNoise( SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel, UINT8 ubTerrType, UINT8 ubVolume, UINT8 ubNoiseType, STR16 zNoiseMessage = NULL );
void ProcessNoise( SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel, UINT8 ubTerrType, UINT8 ubBaseVolume, UINT8 ubNoiseType, STR16 zNoiseMessage = NULL );
UINT8 CalcEffVolume(TacticalActor *pSoldier, INT32 sGridNo, INT8 bLevel, UINT8 ubNoiseType, UINT8 ubBaseVolume, UINT8 ubTerrType1, UINT8 ubTerrType2);
void HearNoise(TacticalActor *pSoldier, SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel, UINT8 ubVolume, UINT8 ubNoiseType, UINT8 *ubSeen);
void TellPlayerAboutNoise(TacticalActor *pSoldier, SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel, UINT8 ubVolume, UINT8 ubNoiseType, UINT8 ubNoiseDir,  STR16 zNoiseMessage = NULL );
void OurTeamSeesSomeone( TacticalActor * pSoldier, INT8 bNumReRevealed, INT8 bNumNewEnemies );

void IncrementWatchedLoc( UINT16 ubID, INT32 sGridNo, INT8 bLevel );
void SetWatchedLocAsUsed( UINT16 ubID, INT32 sGridNo, INT8 bLevel );
void DecayWatchedLocs( INT8 bTeam );

void HandleManNoLongerSeen( TacticalActor * pSoldier, TacticalActor * pOpponent, INT8 * pPersOL, INT8 * pbPublOL );

// The_Bob - real time sneaking code 01/06/09
extern void CancelItemPointer(void);
extern BOOLEAN NobodyAlerted(void);
extern void ShowRadioLocator( SoldierID ubID, UINT8 ubLocatorSpeed );
//#define TESTOPPLIST

// for ManSeesMan()
#define MANLOOKSFORMAN		0
#define HEARNOISE		1
#define NOTICEUNSEENATTACKER	2


// for ManLooksForMan()
#define MANLOOKSFOROTHERTEAMS	0
#define OTHERTEAMSLOOKFORMAN	1
#define VERIFYANDDECAYOPPLIST	2
#define HANDLESTEPPEDLOOKAT	 3
#define LOOKANDLISTEN			4
#define UPDATEPUBLIC						5
#define CALLER_UNKNOWN					6

// this variable is a flag used in HandleSight to determine whether (while in non-combat RT)
// someone has just been seen, EITHER THE MOVER OR SOMEONE THE MOVER SEES
BOOLEAN		gfPlayerTeamSawCreatures = FALSE;
BOOLEAN	gfPlayerTeamSawJoey			= FALSE;
BOOLEAN	gfMikeShouldSayHi				= FALSE;
//JA25 UB
BOOLEAN   gfMorrisShouldSayHi				 = FALSE;

SoldierID		gubBestToMakeSighting[BEST_SIGHTING_ARRAY_SIZE];
UINT8			gubBestToMakeSightingSize = 0;
//BOOLEAN		gfHumanSawSomeoneInRealtime;

BOOLEAN		gfDelayResolvingBestSightingDueToDoor = FALSE;

#define SHOULD_BECOME_HOSTILE_SIZE 32

SoldierID		gubShouldBecomeHostileOrSayQuote[ SHOULD_BECOME_HOSTILE_SIZE ];
UINT8			gubNumShouldBecomeHostileOrSayQuote;

// NB this ID is set for someone opening a door
SoldierID		gubInterruptProvoker = NOBODY;

INT8 gbPublicOpplist[MAXTEAMS][TOTAL_SOLDIERS];
INT8 gbSeenOpponents[TOTAL_SOLDIERS][TOTAL_SOLDIERS];
INT32 gsLastKnownOppLoc[TOTAL_SOLDIERS][TOTAL_SOLDIERS];		// merc vs. merc
INT8 gbLastKnownOppLevel[TOTAL_SOLDIERS][TOTAL_SOLDIERS];
INT32 gsPublicLastKnownOppLoc[MAXTEAMS][TOTAL_SOLDIERS];	// team vs. merc
INT8 gbPublicLastKnownOppLevel[MAXTEAMS][TOTAL_SOLDIERS];
UINT8 gubPublicNoiseVolume[MAXTEAMS];
INT32 gsPublicNoiseGridNo[MAXTEAMS];
INT8	gbPublicNoiseLevel[MAXTEAMS];

UINT8 gubKnowledgeValue[10][10] =
 {
	//	P E R S O N A L	O P P L I S T	//
	// -4	-3	-2	-1	0	1	2	3	4	5	//
	{	0,	1,	2,	3,	0,	5,	4,	3,	2,	1}, // -4
	{	0,	0,	1,	2,	0,	4,	3,	2,	1,	0}, // -3	O
	{	0,	0,	0,	1,	0,	3,	2,	1,	0,	0}, // -2	P P
	{	0,	0,	0,	0,	0,	2,	1,	0,	0,	0}, // -1	U P
	{	0,	1,	2,	3,	0,	5,	4,	3,	2,	1}, //	0	B L
	{	0,	0,	0,	0,	0,	0,	0,	0,	0,	0}, //	1	L I
	{	0,	0,	0,	0,	0,	1,	0,	0,	0,	0}, //	2	I S
	{	0,	0,	0,	1,	0,	2,	1,	0,	0,	0}, //	3	C T
	{	0,	0,	1,	2,	0,	3,	2,	1,	0,	0}, //	4
	{	0,	1,	2,	3,	0,	4,	3,	2,	1,	0}	//	5

/*
	//	P E R S O N A L	O P P L I S T	//
	// -3	-2	-1	0	1	2	3	4	//
	{	0,	1,	2,	0,	4,	3,	2,	1	}, // -3	O
	{	0,	0,	1,	0,	3,	2,	1,	0	}, // -2	P P
	{	0,	0,	0,	0,	2,	1,	0,	0	}, // -1	U P
	{	1,	2,	3,	0,	5,	4,	3,	2	}, //	0	B L
	{	0,	0,	0,	0,	0,	0,	0,	0	}, //	1	L I
	{	0,	0,	0,	0,	1,	0,	0,	0	}, //	2	I S
	{	0,	0,	1,	0,	2,	1,	0,	0	}, //	3	C T
	{	0,	1,	2,	0,	3,	2,	1,	0	}	//	4
	*/
 };

#define MAX_WATCHED_LOC_POINTS 4
#define WATCHED_LOC_RADIUS 1

INT32			gsWatchedLoc[ TOTAL_SOLDIERS ][ NUM_WATCHED_LOCS ];
INT8			gbWatchedLocLevel[ TOTAL_SOLDIERS ][ NUM_WATCHED_LOCS ];
UINT8			gubWatchedLocPoints[ TOTAL_SOLDIERS ][ NUM_WATCHED_LOCS ];
BOOLEAN		gfWatchedLocReset[ TOTAL_SOLDIERS ][ NUM_WATCHED_LOCS ];
BOOLEAN		gfWatchedLocHasBeenIncremented[ TOTAL_SOLDIERS ][ NUM_WATCHED_LOCS ];

INT8 gbLookDistance[8][8];

INT8 BEHIND;
INT8 SBEHIND;
INT8 SIDE;
INT8 ANGLE;
INT8 STRAIGHT;

void InitSightRange()
{
	BEHIND	 =	 (INT8)( BEHIND_RATIO	* gGameExternalOptions.ubStraightSightRange );
	SBEHIND	=	 (INT8)( SBEHIND_RATIO	* gGameExternalOptions.ubStraightSightRange );
	SIDE		=	 (INT8)( SIDE_RATIO		* gGameExternalOptions.ubStraightSightRange );
	ANGLE		=	 (INT8)( ANGLE_RATIO	* gGameExternalOptions.ubStraightSightRange );
	STRAIGHT	=	 (INT8)( STRAIGHT_RATIO * gGameExternalOptions.ubStraightSightRange );

	INT8 dummy[15][15];


	{	//Pulmu: VC6 compatibility
	for (int i=0; i<8; i++)
	{
			dummy[i][i]=STRAIGHT;
			dummy[i][i+1]=ANGLE;
			dummy[i+1][i]=ANGLE;
			dummy[i][i+2]=SIDE;
			dummy[i+2][i]=SIDE;
			dummy[i][i+3]=SBEHIND;
			dummy[i+3][i]=SBEHIND;
			dummy[i][i+4]=BEHIND;
			dummy[i+4][i]=BEHIND;
			dummy[i][i+5]=SBEHIND;
			dummy[i+5][i]=SBEHIND;
			dummy[i][i+6]=SIDE;
			dummy[i+6][i]=SIDE;
			dummy[i][i+7]=ANGLE;
			dummy[i+7][i]=ANGLE;
	}
	}
	for (int i=0; i<8; i++)
	{
		for (int j=0; j<8; j++)
		{
			gbLookDistance[i][j] = dummy[i][j];
		}
	}
	//gbLookDistance[8][8] =
	//{
	//	//	LOOKER DIR		LOOKEE DIR
	//	//					NORTH	| NORTHEAST	|	EAST	|	SOUTHEAST	|	SOUTH	|	SOUTHWEST	|	WEST	|	NORTHWEST
	//	/* NORTH		*/	 STRAIGHT,	 ANGLE,		SIDE,	 SBEHIND,	 BEHIND,	 SBEHIND,		SIDE,		ANGLE,
	//	/* NORTHEAST	*/	 ANGLE,	 STRAIGHT,		ANGLE,		SIDE,	SBEHIND,		BEHIND,	SBEHIND,		SIDE,
	//	/* EAST		*/	 SIDE,		 ANGLE,	STRAIGHT,		ANGLE,		SIDE,	 SBEHIND,	 BEHIND,	 SBEHIND,
	//	/* SOUTHEAST	*/	 SBEHIND,		SIDE,		ANGLE,	STRAIGHT,		ANGLE,		SIDE,	SBEHIND,		BEHIND,
	//	/* SOUTH		*/	 BEHIND,	 SBEHIND,		SIDE,		ANGLE,	STRAIGHT,		ANGLE,		SIDE,	 SBEHIND,
	//	/* SOUTHWEST	*/	 SBEHIND,	 BEHIND,	SBEHIND,		SIDE,		ANGLE,	STRAIGHT,		ANGLE,		SIDE,
	//	/* WEST		*/	 SIDE,		SBEHIND,	 BEHIND,	 SBEHIND,		SIDE,		ANGLE,	STRAIGHT,		ANGLE,
	//	/* NORTHWEST	*/	 ANGLE,		 SIDE,	 SBEHIND,	 BEHIND,	SBEHIND,		SIDE,		ANGLE,	STRAIGHT
	//	};
}


INT8 gbSmellStrength[3] =
{
	NORMAL_HUMAN_SMELL_STRENGTH, // normal
	NORMAL_HUMAN_SMELL_STRENGTH + 2, // slob
	NORMAL_HUMAN_SMELL_STRENGTH - 1	// snob
};


SoldierID gsWhoThrewRock = NOBODY;

// % values of sighting distance at various light levels
//DBrot: use gGameExternalOptions.ubBrightnessVisionMod instead
/*
INT8 gbLightSighting[1][SHADE_MIN+1] =
{
{ // human
	80, // brightest
	86,
	93,
	100, // normal daylight, 3
	94,
	88,
	82,
	76,
	70, // mid-dawn, 8
	64,
	58,
	51,
	43, // normal nighttime, 12 (11 tiles)
	30,
	17,
	9
}
};*/
/*
{
{ // human
	80, // brightest
	86,
	93,
	100, // normal daylight, 3
	93,
	86,
	79,
	72,
	65, // mid-dawn, 8
	58,
	53,
	43, // normal nighttime, 11	(11 tiles)
	35,
	26,
	17,
	9
}
};
*/

UINT8			gubSightFlags = 0;

void DECAY_OPPLIST_VALUE( INT8& value )
{
	if ( (value) >= SEEN_THIS_TURN)
	{
		(value)++;
		if ( (value) > OLDEST_SEEN_VALUE )
		{
			(value) = NOT_HEARD_OR_SEEN;
		}
	}
	else
	{
		if ( (value) <= HEARD_THIS_TURN)
		{
			(value)--;
			if ( (value) < OLDEST_HEARD_VALUE)
			{
				(value) = NOT_HEARD_OR_SEEN;
			}
		}
	}
}


//rain
extern BOOLEAN gfLightningInProgress;
extern BOOLEAN gfHaveSeenSomeone;
extern UINT8 ubRealAmbientLightLevel;
//end rain


INT16 AdjustMaxSightRangeForEnvEffects( TacticalActor *pSoldier, INT8 bLightLevel, INT16 sDistVisible )
{
	INT16 sNewDist = sDistVisible * gGameExternalOptions.ubBrightnessVisionMod[bLightLevel] / 100;

	// Adjust it based on weather...
	if ( !pSoldier->deployment().sectorZ() )
	{
		FLOAT weatherpenalty = gGameExternalOptions.dVisDistDecrease[SectorInfo[SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )].usWeather];

		FLOAT appliedpenalty = 1.0f;
		if ( HAS_SKILL_TRAIT( pSoldier, SURVIVAL_NT ) && (gGameOptions.fNewTraitSystem) )
			appliedpenalty = min( 1.0f, max( 0.0f, appliedpenalty - gSkillTraitValues.dSVWeatherPenaltiesReduction * NUM_SKILL_TRAITS( pSoldier, SURVIVAL_NT ) ) );

		sNewDist -= (INT16)(sNewDist * weatherpenalty * appliedpenalty);

		//rain
		if ( gfLightningInProgress )
			sNewDist += sNewDist * (ubRealAmbientLightLevel) / 10;	// 10% per dark level
		//end rain
	}
	
	return( sNewDist );
}

static TacticalActor* ResolveBestSighter( UINT16 position )
{
	if ( position >= BEST_SIGHTING_ARRAY_SIZE )
	{
		return nullptr;
	}

	return GetJa2SoldierRepository().resolve(
		gubBestToMakeSighting[ position ] );
}

void SwapBestSightingPositions( INT8 bPos1, INT8 bPos2 )
{
	SoldierID ubTemp = gubBestToMakeSighting[ bPos1 ];

	gubBestToMakeSighting[ bPos1 ] = gubBestToMakeSighting[ bPos2 ];
	gubBestToMakeSighting[ bPos2 ] = ubTemp;
}

void ReevaluateBestSightingPosition( TacticalActor * pSoldier, INT8 bInterruptDuelPts )
{
	UINT8 ubLoop, ubLoop2;
	BOOLEAN		fFound = FALSE;
	BOOLEAN		fPointsGotLower = FALSE;

	if ( bInterruptDuelPts == NO_INTERRUPT )
	{
		return;
	}

	if ( !( pSoldier->status().flags() & SOLDIER_MONSTER ) )
	{
		//gfHumanSawSomeoneInRealtime = TRUE;
	}

	if ( (pSoldier->turnState().interruptDuelPoints() != NO_INTERRUPT) && (bInterruptDuelPts < pSoldier->turnState().interruptDuelPoints()) )
	{
		fPointsGotLower = TRUE;
	}

	if ( fPointsGotLower )
	{
		// loop to end of array less 1 entry since we can't swap the last entry out of the array
		for ( ubLoop = 0; ubLoop < gubBestToMakeSightingSize - 1; ubLoop++ )
		{
			if ( pSoldier->identity().id() == gubBestToMakeSighting[ ubLoop ] )
			{
				fFound = TRUE;
				break;
			}
		}

		// this guy has fewer interrupt pts vs another enemy!	reduce position unless in last place
		if (fFound)
		{
			// set new points
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "RBSP: reducing points for %d to %d", pSoldier->identity().id(), bInterruptDuelPts ) );
			pSoldier->turnState().interruptDuelPoints() = bInterruptDuelPts;

			// must percolate him down
			for ( ubLoop2 = ubLoop + 1; ubLoop2 < gubBestToMakeSightingSize; ubLoop2++ )
			{
				const TacticalActor* previous =
					ResolveBestSighter( ubLoop2 - 1 );
				const TacticalActor* current =
					ResolveBestSighter( ubLoop2 );
				if ( previous != nullptr && current != nullptr &&
					previous->turnState().interruptDuelPoints() <
						current->turnState().interruptDuelPoints() )
				{
					SwapBestSightingPositions( (UINT8) (ubLoop2 - 1), ubLoop2 );
				}
				else
				{
					break;
				}
			}
		}
		else if ( pSoldier->identity().id() == gubBestToMakeSighting[ gubBestToMakeSightingSize - 1] )
		{
			// in list but can't be bumped down... set his new points
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "RBSP: reduced points for last individual %d to %d", pSoldier->identity().id(), bInterruptDuelPts ) );
			pSoldier->turnState().interruptDuelPoints() = bInterruptDuelPts;
		}
	}
	else
	{
		// loop through whole array
		for ( ubLoop = 0; ubLoop < gubBestToMakeSightingSize; ubLoop++ )
		{
			if ( pSoldier->identity().id() == gubBestToMakeSighting[ ubLoop ] )
			{
				fFound = TRUE;
				break;
			}
		}

		if (!fFound)
		{
			for ( ubLoop = 0; ubLoop < gubBestToMakeSightingSize; ubLoop++ )
			{
				const SoldierID currentId =
					gubBestToMakeSighting[ ubLoop ];
				TacticalActor* current = ResolveBestSighter( ubLoop );
				const bool emptySlot =
					currentId == NOBODY || current == nullptr;
				if ( (emptySlot &&
						TacticalActorCovertOps::recognizesCombatant(*pSoldier, currentId)) ||
					(current != nullptr &&
						bInterruptDuelPts >
							current->turnState().interruptDuelPoints()) )
				{
					TacticalActor* last =
						ResolveBestSighter( gubBestToMakeSightingSize - 1 );
					if ( last != nullptr )
					{
						last->turnState().interruptDuelPoints() = NO_INTERRUPT;
						DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "RBSP: resetting points for %d to zilch", pSoldier->identity().id() ) );
					}

					// set new points
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "RBSP: setting points for %d to %d", pSoldier->identity().id(), bInterruptDuelPts ) );
					pSoldier->turnState().interruptDuelPoints() = bInterruptDuelPts;

					// insert here!
					for ( ubLoop2 = gubBestToMakeSightingSize - 1; ubLoop2 > ubLoop; ubLoop2-- )
					{
						gubBestToMakeSighting[ ubLoop2 ] = gubBestToMakeSighting[ ubLoop2 - 1 ];
					}
					gubBestToMakeSighting[ ubLoop ] = pSoldier->identity().id();
					break;
				}
			}
		}
		// else points didn't get lower, so do nothing (because we want to leave each merc with as low int points as possible)
	}

	for ( ubLoop = 0; ubLoop < BEST_SIGHTING_ARRAY_SIZE; ubLoop++ )
	{
		if ( (gubBestToMakeSighting[ ubLoop ] != NOBODY) )
		{
			const TacticalActor* current = ResolveBestSighter( ubLoop );
			if ( current != nullptr )
			{
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "RBSP entry %d: %d (%d pts)", ubLoop, gubBestToMakeSighting[ ubLoop ], current->turnState().interruptDuelPoints() ) );
			}
		}
	}

}

void HandleBestSightingPositionInRealtime( void )
{
	// This function is called for handling interrupts when opening a door in non-combat or
	// just sighting in non-combat, deciding who gets the first turn

	UINT16 ubLoop;

	if ( gfDelayResolvingBestSightingDueToDoor )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "HBSPIR: skipping due to door flag" );
		return;
	}

	// Also delay until attack busy returns
//	if (GetJa2PendingTacticalCombatActions() > 0)
//	{
//		return;
//	}

	if (gubBestToMakeSighting[ 0 ] != NOBODY)
	{
		TacticalActor* bestSighter = ResolveBestSighter( 0 );
		if ( bestSighter == nullptr )
		{
			gubBestToMakeSighting[ 0 ] = NOBODY;
			return;
		}

		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "HBSPIR called and there is someone in the list" );

		// MP diagnostic: this realtime sighting is what triggers turn-based in networked
		// play. Report which team/merc saw which enemy team/merc, and at what range, so
		// it's clear what kicked off combat.
		if ( is_networked && !( IsJa2TacticalCombatActive() ) )
		{
			TacticalActor* pSighter = bestSighter;
			TacticalActor* pSeen = NULL; INT16 sBest = 9999;
			for ( UINT32 uiL = 0; uiL < Ja2ActiveTacticalActorSlotCount(); uiL++ )
			{
				TacticalActor* pE = ResolveJa2ActiveTacticalActorSlot(uiL);
				if ( pE && pE->roster().active() && pE->roster().inSector() && pE->vitals().health() > 0
				    && pE->roster().team() != pSighter->roster().team()
				    && pSighter->awareness().opponentKnowledge()[ pE->identity().id() ] == SEEN_CURRENTLY )
				{
					INT16 d = PythSpacesAway( pSighter->position().gridNo(), pE->position().gridNo() );
					if ( d < sBest ) { sBest = d; pSeen = pE; }
				}
			}
			mp_log_sighting( pSighter, pSeen, pSeen ? (int)sBest : -1 );
		}

		//if (gfHumanSawSomeoneInRealtime)
		{
			TacticalActor* secondSighter = ResolveBestSighter( 1 );
			if (secondSighter == nullptr)
			{	// The_Bob - real time sneaking code 01/06/09
				// if real time sneaking conditions are met...
				// this is now in the preferences window - SANDRO				
				// MP: in PvP both clients see "MY merc spotted them" so BOTH would
				// realtime-sneak and neither enters combat -> first-contact deadlock.
				// Don't sneak over the network: enter combat on sight and let the
				// server arbitrate who acts first (the startCOMBAT first-arrival guard).
				if (gGameSettings.fOptions[TOPTION_ALLOW_REAL_TIME_SNEAK] &&
					bestSighter->roster().team() == OUR_TEAM && NobodyAlerted() &&
					!is_networked )
				{
					// get rid of the item under cursor (we gotta react FAST)
					CancelItemPointer();
					// select (and center screen on) the merc who saw the enemy
					// HEADROCK HAM 3.6: A much-requested toggle.
					if (gusSelectedSoldier != bestSighter->identity().id() &&
						!gGameExternalOptions.fNoAutoFocusChangeInRealtimeSneak)
						SelectSoldier(bestSighter->identity().id(), false, true);
					// if not quiet, emit a message warning the player
					if (!gGameExternalOptions.fQuietRealTimeSneak)
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_RTM_ENEMIES_SPOOTED]);

					return;	// and do nothing
				}
				else if ( bestSighter->roster().team() != OUR_TEAM &&
					bestSighter->awareness().opponentCount() > 0 )
				{
					// otherwise, simply award the turn to the team that saw the enemy first
					EnterCombatMode( bestSighter->roster().team() );
				}
				else
					// otherwise, simply award the turn to the team that saw the enemy first
					EnterCombatMode( bestSighter->roster().team() );
			}
			else
			{
				TacticalActor* thirdSighter = ResolveBestSighter( 2 );


				// if 1st and 2nd on same team, or 1st and 3rd on same team, or there IS no 3rd, award turn to 1st
				if (  /*gubBestToMakeSighting[ 0 ]->awareness().opponentCount() > 0 &&*/
						(bestSighter->roster().team() == secondSighter->roster().team()) ||
							(thirdSighter == nullptr ||
							 bestSighter->roster().team() == thirdSighter->roster().team())
					)
				{
					EnterCombatMode( bestSighter->roster().team() );
				}
				else //if ( gubBestToMakeSighting[ 1 ]->awareness().opponentCount() > 0 ) // give turn to 2nd best but interrupt to 1st
				{
					if ( is_networked )
					{
						// MP has interrupts disabled (they desync over the relay), so don't
						// OPEN combat with one either. Give the first sighter the turn and
						// let the server arbitrate -- no out-of-turn interrupt state.
						EnterCombatMode( bestSighter->roster().team() );
					}
					else
					{
						DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Entering combat mode: turn for 2nd best, int for best" );

						EnterCombatMode( secondSighter->roster().team() );
						// 2nd guy loses control
						AddToIntList( gubBestToMakeSighting[ 1 ], FALSE, TRUE);
						// 1st guy gains control
						AddToIntList( gubBestToMakeSighting[ 0 ], TRUE, TRUE);
						// done
						DoneAddingToIntList( bestSighter, TRUE, SIGHTINTERRUPT );
					}
				}
			}
		}

		for ( ubLoop = 0; ubLoop < BEST_SIGHTING_ARRAY_SIZE; ubLoop++ )
		{
			if ( gubBestToMakeSighting[ ubLoop ] != NOBODY )
			{
				TacticalActor* sighter = ResolveBestSighter( ubLoop );
				if ( sighter != nullptr )
				{
					sighter->turnState().interruptDuelPoints() = NO_INTERRUPT;
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "RBSP: done, resetting points for %d to zilch", sighter->identity().id() ) );
				}
			}
		}

		for ( ubLoop = 0; ubLoop < Ja2ActiveTacticalActorSlotCount(); ubLoop++ )
		{
			TacticalActor* soldier =
				ResolveJa2ActiveTacticalActorSlot(ubLoop);
			if ( soldier )
			{
				AssertMsg(
					soldier->turnState().interruptDuelPoints() ==
						NO_INTERRUPT,
					String("%S (%d) still has interrupt pts!",
						soldier->identity().name(),
						soldier->identity().id()));
			}
		}
	}


}

void HandleBestSightingPositionInTurnbased( void )
{
	// This function is called for handling interrupts when opening a door in turnbased

	UINT16 ubLoop, ubLoop2;
	BOOLEAN	fOk = FALSE;

	if ( gubBestToMakeSighting[ 0 ] != NOBODY )
	{
		TacticalActor* bestSighter = ResolveBestSighter( 0 );
		if ( bestSighter == nullptr )
		{
			gubBestToMakeSighting[ 0 ] = NOBODY;
			return;
		}

		if ( bestSighter->roster().team() != GetJa2TacticalCurrentTeam() )
		{

			// interrupt!
			for ( ubLoop = 0; ubLoop < gubBestToMakeSightingSize; ubLoop++ )
			{
				if ( gubBestToMakeSighting[ ubLoop ] == NOBODY )
				{
					if (gubInterruptProvoker == NOBODY)
					{
						// do nothing (for now) - abort!
						return;
					}
					else
					{
						// use this guy as the "interrupted" fellow
						gubBestToMakeSighting[ ubLoop ] = gubInterruptProvoker;
						fOk = TRUE;
						break;
					}

				}
				else
				{
					const TacticalActor* current =
						ResolveBestSighter( ubLoop );
					if ( current != nullptr &&
						current->roster().team() == GetJa2TacticalCurrentTeam() )
					{
						fOk = TRUE;
						break;
					}
				}
			}

			if ( fOk )
			{
				TacticalActor* interrupted = ResolveBestSighter( ubLoop );
				if ( interrupted != nullptr )
				{
					// this is the guy who gets "interrupted"; all else before him interrupted him
					AddToIntList( gubBestToMakeSighting[ ubLoop ], FALSE, TRUE);
					for ( ubLoop2 = 0; ubLoop2 < ubLoop; ubLoop2++ )
					{
						AddToIntList( gubBestToMakeSighting[ ubLoop2 ], TRUE, TRUE);
					}
					// done
					DoneAddingToIntList( interrupted, TRUE, SIGHTINTERRUPT );
				}
			}

		}
		for ( ubLoop = 0; ubLoop < BEST_SIGHTING_ARRAY_SIZE; ubLoop++ )
		{
			if ( gubBestToMakeSighting[ ubLoop ] != NOBODY )
			{
				TacticalActor* sighter = ResolveBestSighter( ubLoop );
				if ( sighter != nullptr )
				{
					sighter->turnState().interruptDuelPoints() = NO_INTERRUPT;
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "RBSP (TB): done, resetting points for %d to zilch", sighter->identity().id() ) );
				}
			}
		}

		for ( ubLoop = 0; ubLoop < Ja2ActiveTacticalActorSlotCount(); ubLoop++ )
		{
			TacticalActor* soldier =
				ResolveJa2ActiveTacticalActorSlot(ubLoop);
			if ( soldier )
			{
				AssertMsg(
					soldier->turnState().interruptDuelPoints() ==
						NO_INTERRUPT,
					String("%S (%d) still has interrupt pts!",
						soldier->identity().name(),
						soldier->identity().id()));
			}
		}


	}

}

void InitSightArrays( void )
{
	UINT32		uiLoop;

	for ( uiLoop = 0; uiLoop < BEST_SIGHTING_ARRAY_SIZE; uiLoop++ )
	{
		gubBestToMakeSighting[ uiLoop ] = NOBODY;
	}
	//gfHumanSawSomeoneInRealtime = FALSE;

	// It is assumed that once the sight arrays are (re)initialized, the enemies should all have no
	// interrupt points.	At least a paranoia check in the HandleSightingInRealtime and HandleSightingInTurnBased
	// both make sure no interrupt points remain after they drain the sighting list.
	// I do not fully grok the interrupt system, but it IS possible for this function to be called when there are
	// still soldiers on the sighting list, which gives them lingering interrupt points.	So the best course of
	// action is to reset their points.	However, I think I'll just do a clean sweep here to make sure I get them all.
	for ( uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++ )
	{
		TacticalActor* soldier =
			ResolveJa2ActiveTacticalActorSlot(uiLoop);
		if (soldier)
		{
			soldier->turnState().interruptDuelPoints() = NO_INTERRUPT;
		}
	}
}

void AddToShouldBecomeHostileOrSayQuoteList( SoldierID ubID )
{
	UINT16 ubLoop;

	Assert( gubNumShouldBecomeHostileOrSayQuote < SHOULD_BECOME_HOSTILE_SIZE );

	const TacticalActor* soldier =
		GetJa2SoldierRepository().resolve( ubID );
	if ( soldier == nullptr || soldier->vitals().health() < OKLIFE )
	{
		return;
	}

	// make sure not already in list
	for ( ubLoop = 0; ubLoop < gubNumShouldBecomeHostileOrSayQuote; ubLoop++ )
	{
		if ( gubShouldBecomeHostileOrSayQuote[ ubLoop ] == ubID )
		{
			return;
		}
	}

	gubShouldBecomeHostileOrSayQuote[ gubNumShouldBecomeHostileOrSayQuote ] = ubID;
	gubNumShouldBecomeHostileOrSayQuote++;
}

SoldierID SelectSpeakerFromHostileOrSayQuoteList( void )
{
	SoldierID IDList[ SHOULD_BECOME_HOSTILE_SIZE ];
	UINT8 ubLoop, ubNumProfiles = 0;
	TacticalActor *		pSoldier;

	for ( ubLoop = 0; ubLoop < gubNumShouldBecomeHostileOrSayQuote; ubLoop++ )
	{
		pSoldier = GetJa2SoldierRepository().resolve(
			gubShouldBecomeHostileOrSayQuote[ ubLoop ] );
		if ( pSoldier == nullptr )
		{
			continue;
		}

		if ( pSoldier->identity().profile() != NO_PROFILE )
		{

			// make sure person can say quote!!!!
			gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags2 |= PROFILE_MISC_FLAG2_NEEDS_TO_SAY_HOSTILE_QUOTE;

			if ( NPCHasUnusedHostileRecord( pSoldier->identity().profile(), APPROACH_DECLARATION_OF_HOSTILITY ) )
			{
				IDList[ ubNumProfiles ] = gubShouldBecomeHostileOrSayQuote[ ubLoop ];
				ubNumProfiles++;
			}
			else
			{
				// turn flag off again
				gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags2 &= ~PROFILE_MISC_FLAG2_NEEDS_TO_SAY_HOSTILE_QUOTE;
			}

		}
	}

	if ( ubNumProfiles == 0 )
	{
		return( NOBODY );
	}
	else
	{
		return(IDList[ Random( ubNumProfiles ) ] );
	}
}

extern BOOLEAN gfWaitingForTriggerTimer;

void CheckHostileOrSayQuoteList( void )
{
	if ( gubNumShouldBecomeHostileOrSayQuote == 0 || !DialogueQueueIsEmpty() || gfInTalkPanel || gfWaitingForTriggerTimer )
	{
		return;
	}
	else
	{
		SoldierID ubSpeaker;
		UINT16 ubLoop;
		TacticalActor * pSoldier;

		ubSpeaker = SelectSpeakerFromHostileOrSayQuoteList();
		if ( ubSpeaker == NOBODY )
		{
			// make sure everyone on this list is hostile
			for ( ubLoop = 0; ubLoop < gubNumShouldBecomeHostileOrSayQuote; ubLoop++ )
			{
				pSoldier = GetJa2SoldierRepository().resolve(
					gubShouldBecomeHostileOrSayQuote[ ubLoop ] );
				if ( pSoldier == nullptr )
				{
					continue;
				}

				if ( pSoldier->aiBehavior().neutral() )
				{
					MakeCivHostile(pSoldier);
					// make civ group, if any, hostile
					if ( pSoldier->roster().team() == CIV_TEAM && pSoldier->roster().civilianGroup() != NON_CIV_GROUP && gTacticalStatus.fCivGroupHostile[ pSoldier->roster().civilianGroup() ] == CIV_GROUP_WILL_BECOME_HOSTILE )
					{
						gTacticalStatus.fCivGroupHostile[ pSoldier->roster().civilianGroup() ] = CIV_GROUP_HOSTILE;
					}

					// reevaluate sight - we might already see people that weren't enemies until now
					ManLooksForOtherTeams(pSoldier);

					pSoldier->aiBehavior().alertStatus() = STATUS_RED;
				}
			}

			// unpause all AI
			UnPauseAI();
			// reset the list
			memset( &gubShouldBecomeHostileOrSayQuote, NOBODY, SHOULD_BECOME_HOSTILE_SIZE );
			gubNumShouldBecomeHostileOrSayQuote = 0;
			//and return/go into combat
			if ( !(IsJa2TacticalCombatActive() ) )
			{
				EnterCombatMode( CIV_TEAM );
			}
		}
		else
		{
			TacticalActor* speaker =
				GetJa2SoldierRepository().resolve( ubSpeaker );
			if ( speaker == nullptr )
			{
				return;
			}

			// pause all AI
			PauseAIUntilManuallyUnpaused();
			// stop everyone?

			// We want to make this guy visible to the player.
			if ( speaker->awareness().visibility() != TRUE )
			{
				gbPublicOpplist[ gbPlayerNum ][ ubSpeaker ] = HEARD_THIS_TURN;
				HandleSight( speaker, SIGHT_LOOK | SIGHT_RADIO );
			}
			// trigger hater
			TriggerNPCWithIHateYouQuote( speaker->identity().profile() );
		}
	}
}

void HandleSight(TacticalActor *pSoldier, UINT8 ubSightFlags)
{
	UINT32 uiLoop;
	TacticalActor *pThem;
	INT8			bTempNewSituation;

	if (!pSoldier->roster().active() || !pSoldier->roster().inSector() || pSoldier->status().flags() & SOLDIER_DEAD )
	{
		// I DON'T THINK SO!
		return;
	}

	gubSightFlags = ubSightFlags;

	if ( gubBestToMakeSightingSize != BEST_SIGHTING_ARRAY_SIZE_ALL_TEAMS_LOOK_FOR_ALL )
	{
		// if this is not being called as a result of all teams look for all, reset array size
		if ( (IsJa2TacticalCombatActive()) )
		{
			// NB the incombat size is 0
			gubBestToMakeSightingSize = BEST_SIGHTING_ARRAY_SIZE_INCOMBAT;
		}
		else
		{
			gubBestToMakeSightingSize = BEST_SIGHTING_ARRAY_SIZE_NONCOMBAT;
		}

		InitSightArrays();
	}

	for ( uiLoop = 0; uiLoop < NUM_WATCHED_LOCS; ++uiLoop )
	{
		gfWatchedLocHasBeenIncremented[ pSoldier->identity().id() ][ uiLoop ] = FALSE;
	}

	gfPlayerTeamSawCreatures = FALSE;

	// store new situation value
	bTempNewSituation = pSoldier->aiBehavior().newSituation();
	pSoldier->aiBehavior().newSituation() = FALSE;

	// if we've been told to make this soldier look (& others look back at him)
	if (ubSightFlags & SIGHT_LOOK)
	{
		// if this soldier's under our control and well enough to look
		if (pSoldier->vitals().health() >= OKLIFE )
		{
		/*
#ifdef RECORDOPPLIST
	 fprintf(OpplistFile,"ManLooksForOtherTeams (HandleSight/Look) for %d\n",pSoldier->guynum);
#endif
		*/
			// he looks for all other soldiers not on his own team
			ManLooksForOtherTeams(pSoldier);

	 // if "Show only enemies seen" option is ON and it's this guy looking
	 //if (pSoldier->identity().id() == ShowOnlySeenPerson)
		//NewShowOnlySeenPerson(pSoldier);					// update the string
		}


	/*
#ifdef RECORDOPPLIST
	fprintf(OpplistFile,"OtherTeamsLookForMan (HandleSight/Look) for %d\n",ptr->guynum);
#endif
	*/

		// all soldiers under our control but not on ptr's team look for him
		OtherTeamsLookForMan(pSoldier);
	} // end of SIGHT_LOOK

	// if we've been told that interrupts are possible as a result of sighting
	if (IsJa2TacticalTurnBasedCombat() &&
		(ubSightFlags & SIGHT_INTERRUPT) && 
		(!UsingImprovedInterruptSystem() || gGameExternalOptions.fAllowInstantInterruptsOnSight ) )
	{
		ResolveInterruptsVs( pSoldier, SIGHTINTERRUPT );
	}

	if ( gubBestToMakeSightingSize == BEST_SIGHTING_ARRAY_SIZE_NONCOMBAT )
	{
		HandleBestSightingPositionInRealtime();
	}

	if ( pSoldier->aiBehavior().newSituation() && !(pSoldier->status().flags() & SOLDIER_PC) )
	{
		pSoldier->HaultSoldierFromSighting( TRUE );
	}
	pSoldier->aiBehavior().newSituation() = __max( pSoldier->aiBehavior().newSituation(), bTempNewSituation );

	// if we've been told to radio the results
	if (ubSightFlags & SIGHT_RADIO)
	{
		if (pSoldier->status().flags() & SOLDIER_PC )
		{
			// update our team's public knowledge
			RadioSightings(pSoldier,EVERYBODY, pSoldier->roster().team() );
/*  comm by ddd
#ifdef WE_SEE_WHAT_MILITIA_SEES_AND_VICE_VERSA
			RadioSightings(pSoldier,EVERYBODY, MILITIA_TEAM);
#endif*/
//ddd{
			if(gGameExternalOptions.bWeSeeWhatMilitiaSeesAndViceVersa)
				RadioSightings(pSoldier,EVERYBODY, MILITIA_TEAM);

#ifdef ENABLE_MP_FRIENDLY_PLAYERS_SHARE_SAME_FOV
			//haydent
			if(is_networked &&  pSoldier->roster().side() == 0 && pSoldier->roster().team() != OUR_TEAM)
				RadioSightings(pSoldier,EVERYBODY, OUR_TEAM);
#endif

//ddd}
			// if it's our local player's merc
			if (PTR_OURTEAM)
				// revealing roofs and looking for items handled here, too
				RevealRoofsAndItems(pSoldier,TRUE, TRUE, pSoldier->position().level(), FALSE );
		}
		// unless in easy mode allow alerted enemies to radio	
		else if ( zDiffSetting[gGameOptions.ubDifficultyLevel].bRadioSightings )
		{
			// don't allow admins to radio
			//Madd: Huh?	why not admins?	removed.
			if ( pSoldier->roster().team() == ENEMY_TEAM && gTacticalStatus.Team[ ENEMY_TEAM ].bAwareOfOpposition ) //&& pSoldier->roster().soldierClass() != 	SOLDIER_CLASS_ADMINISTRATOR )
			{
				RadioSightings(pSoldier,EVERYBODY, pSoldier->roster().team() );
			}
		}

		pSoldier->awareness().clearNewOpponents();
		pSoldier->pathing().needsLook() = FALSE;

// Temporary for opplist synching - disable random order radioing
#ifndef RECORDOPPLIST
		// if this soldier's NOT on our team (MAY be under our control, though!)
		if (!PTR_OURTEAM)
			OurTeamRadiosRandomlyAbout(pSoldier->identity().id());	// radio about him only
#endif

		// all non-humans under our control would now radio, if they were allowed
		// to radio automatically (but they're not).	So just nuke new opp cnt
		// NEW: under LOCALOPPLIST, humans on other teams now also radio in here
		for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); ++uiLoop)
		{
			pThem = ResolveJa2ActiveTacticalActorSlot(uiLoop);

			if (pThem != NULL && pThem->vitals().health() >= OKLIFE)
			{
				// if this merc is on the same team as the target soldier
				if (pThem->roster().team() == pSoldier->roster().team())
					continue;		// he doesn't look (he ALWAYS knows about him)

				// other human team's merc report sightings to their teams now
				if (pThem->status().flags() & SOLDIER_PC)
				{
// Temporary for opplist synching - disable random order radioing
#ifdef RECORDOPPLIST
					// do our own team, too, since we've bypassed random radioing
					if (TRUE)
#else
					// exclude our own team, we've already done them, randomly
					if (pThem->roster().team() != gbPlayerNum)
#endif
						RadioSightings(pThem,pSoldier->identity().id(), pThem->roster().team());
				}
				// unless in easy mode allow alerted enemies to radio		
				else if ( zDiffSetting[gGameOptions.ubDifficultyLevel].bRadioSightings2 )
				{
					// don't allow admins to radio
					// silversurfer: Why not? Removed just like above.
					if ( pThem->roster().team() == ENEMY_TEAM && gTacticalStatus.Team[ ENEMY_TEAM ].bAwareOfOpposition ) //&& pThem->roster().soldierClass() != SOLDIER_CLASS_ADMINISTRATOR )
					{
						RadioSightings(pThem,EVERYBODY, pThem->roster().team() );
					}
				}

				pThem->awareness().clearNewOpponents();
				pThem->pathing().needsLook() = FALSE;
			}
		}
	}

	// CJC August 13 2002: at the end of handling sight, reset sight flags to allow interrupts in case an audio cue should
	// cause someone to see an enemy
	gubSightFlags |= SIGHT_INTERRUPT;
}


void OurTeamRadiosRandomlyAbout(UINT16 ubAbout)
{
	// WDS - make number of mercenaries, etc. be configurable
	INT16 radioCnt = 0,radioMan[CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS];
	TacticalActor	*pSoldier;


	// Temporary for opplist synching - disable random order radioing
#ifdef RECORDOPPLIST
	for (INT16 iLoop = Status.team[Net.pnum].guystart;
		 iLoop < Status.team[Net.pnum].guyend; ++iLoop)
	{
		pSoldier = GetJa2SoldierRepository().resolve( iLoop );
		// if this merc is active, in this sector, and well enough to look
		if (pSoldier && pSoldier->roster().active() && pSoldier->roster().inSector() &&
			(pSoldier->vitals().health() >= OKLIFE))
		{
			RadioSightings(pSoldier,ubAbout,pSoldier->roster().team());
			pSoldier->awareness().clearNewOpponents();
		}
	}

	return;
#endif


	 // All mercs on our local team check if they should radio about him
	SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;

	// make a list of all of our team's mercs
	for ( ; id <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++id )
	{
		pSoldier = GetJa2SoldierRepository().resolve( id );
		// if this merc is active, in this sector, and well enough to look
		if (pSoldier && pSoldier->roster().active() && pSoldier->roster().inSector() && (pSoldier->vitals().health() >= OKLIFE))
			// put him on our list, and increment the counter
			radioMan[radioCnt++] = (INT16)id;
	}


	// now RANDOMLY handle each of the mercs on our list, until none remain
	// (this is all being done ONLY so that the mercs in the earliest merc
	//	slots do not arbitrarily get the bulk of the sighting speech quote
	//	action, while the later ones almost never pipe up, and is NOT
	//	strictly necessary, but a nice improvement over original JA)
	while (radioCnt)
	{
		// pick a merc from one of the remaining slots at random
		INT16 iLoop = Random(radioCnt);

		// handle radioing for that merc
		TacticalActor* radioSoldier =
			GetJa2SoldierRepository().resolve( radioMan[iLoop] );
		if ( radioSoldier )
		{
			RadioSightings(
				radioSoldier, ubAbout, radioSoldier->roster().team() );
			radioSoldier->awareness().clearNewOpponents();
		}

		// unless it WAS the last used slot that we happened to pick
		if (iLoop != (radioCnt - 1))
			// move the contents of the last slot into the one just handled
			radioMan[iLoop] = radioMan[radioCnt - 1];

		radioCnt--;
	}
}










INT16 TeamNoLongerSeesMan( UINT8 ubTeam, TacticalActor *pOpponent, SoldierID ubExcludeID, INT8 bIteration )
{
	TacticalActor *pMate;
	SoldierID bLoop = gTacticalStatus.Team[ubTeam].bFirstID;

	// look for all mercs on the same team, check opplists for this soldier
	for ( ; bLoop <= gTacticalStatus.Team[ubTeam].bLastID; ++bLoop )
	{
		pMate = GetJa2SoldierRepository().resolve( bLoop );
		if ( pMate == nullptr )
		{
			continue;
		}

		// if this "teammate" is me, myself, or I (whom we want to exclude)
		if ( bLoop == ubExcludeID )
			continue;			// skip to next teammate, I KNOW I don't see him...

		// if this merc is not on the same team
		if ( pMate->roster().team() != ubTeam )
			continue;	// skip him, he's no teammate at all!

		// if this merc is not active, at base, on assignment, dead, unconscious
		if ( !pMate->roster().active() || !pMate->roster().inSector() || (pMate->vitals().health() < OKLIFE) )
			continue;	// next merc

		// if this teammate currently sees this opponent
		if ( pMate->awareness().opponentKnowledge()[pOpponent->identity().id()] == SEEN_CURRENTLY )
			return(FALSE);	 // that's all I need to know, get out of here
	}

	/* comm by ddd
	#ifdef WE_SEE_WHAT_MILITIA_SEES_AND_VICE_VERSA
		if ( bIteration == 0 )
		{
			if ( ubTeam == gbPlayerNum && gTacticalStatus.Team[ MILITIA_TEAM ].bTeamActive )
			{
				// check militia team as well
				return( TeamNoLongerSeesMan( MILITIA_TEAM, pOpponent, ubExcludeID, 1 ) );
			}
			else if ( ubTeam == MILITIA_TEAM && gTacticalStatus.Team[ gbPlayerNum ].bTeamActive )
			{
				// check player team as well
				return( TeamNoLongerSeesMan( gbPlayerNum, pOpponent, ubExcludeID, 1 ) );
			}
		}
	#endif
	*/
	//ddd
	if ( gGameExternalOptions.bWeSeeWhatMilitiaSeesAndViceVersa )
	{
		if ( bIteration == 0 )
		{
			if ( ubTeam == gbPlayerNum && gTacticalStatus.Team[MILITIA_TEAM].bTeamActive )
			{
				// check militia team as well
				return(TeamNoLongerSeesMan( MILITIA_TEAM, pOpponent, ubExcludeID, 1 ));
			}
			else if ( ubTeam == MILITIA_TEAM && gTacticalStatus.Team[gbPlayerNum].bTeamActive )
			{
				// check player team as well
				return(TeamNoLongerSeesMan( gbPlayerNum, pOpponent, ubExcludeID, 1 ));
			}
		}

	}
	//ddd

	 // none of my friends is currently seeing the guy, so return success
	return(TRUE);
}

INT16 DistanceSmellable( TacticalActor *pSoldier, TacticalActor * pSubject )
{
	INT16 sDistVisible = STRAIGHT; // as a base

	//if (IsJa2TacticalTurnBased())
	//{
		sDistVisible *= 2;
	//}
	//else
	//{

	//	sDistVisible += 3;
	//}

	if (pSubject)
	{
		if (pSubject->status().flags() & SOLDIER_MONSTER)
		{
			// trying to smell a friend; change nothing
		}
		else
		{
			// smelling a human or animal; if they are coated with monster smell, distance shrinks
			sDistVisible = sDistVisible * (pSubject->perception().normalSmell() - pSubject->perception().monsterSmell()) / NORMAL_HUMAN_SMELL_STRENGTH;
			if (sDistVisible < 0)
			{
				sDistVisible = 0;
			}
		}
	}
	return( sDistVisible );
}

INT16 MaxNormalDistanceVisible( void )
{
	return( STRAIGHT * 2 );
}

INT16 TacticalActor::GetMaxDistanceVisible(INT32 sGridNo, INT8 bLevel, int calcAsType, TacticalActor *pKnownSubject)
{
	if (sGridNo == NOWHERE)
	{
		return MaxNormalDistanceVisible();
	}

	if (bLevel == -1)
	{
		bLevel = this->position().level();
	}

	if (calcAsType == CALC_FROM_ALL_DIRS)
	{
		return DistanceVisible( this, DIRECTION_IRRELEVANT, DIRECTION_IRRELEVANT, sGridNo, bLevel, TacticalActorConditions::isCowering(*this), GetPercentTunnelVision(this), pKnownSubject );
	}

	return DistanceVisible( this, (SoldierHasLimitedVision(this) ? this->pathing().desiredDirection() : DIRECTION_IRRELEVANT), DIRECTION_IRRELEVANT, sGridNo, bLevel, TacticalActorConditions::isCowering(*this), GetPercentTunnelVision(this), pKnownSubject);
}

INT16 DistanceVisible(TacticalActor *pSoldier, INT8 bFacingDir, INT8 bSubjectDir, INT32 sSubjectGridNo, INT8 bLevel, const BOOLEAN& isCowering, const UINT8& tunnelVision, TacticalActor *pKnownSubject)
{
	INT16	sDistVisible;
	INT8	bLightLevel;
	BOOLEAN sideViewLimit = FALSE;
	// When the caller already holds the subject standing at sSubjectGridNo/bLevel
	// (e.g. ManLooksForMan passing pOpponent), reuse it instead of re-walking the
	// tile's structure list via SimpleFindSoldier()/WhoIsThere2().
	TacticalActor* pSubject = pKnownSubject ? pKnownSubject : SimpleFindSoldier( sSubjectGridNo, bLevel );
	INT16 tunnelVisionInPercent = 0;

	if (pSoldier->status().flags() & SOLDIER_MONSTER)
	{
		if ( !pSubject )
		{
			return( FALSE );
		}

		return( DistanceSmellable( pSoldier, pSubject ) );
	}

	if (pSoldier->perception().isBlinded())
	{
		// we're bliiiiiiiiind!!!
		return( 0 );
	}

	// sevenfm: if soldier is unconscious, he can't see anything
	if ( pSoldier->collapseState().tactical() && pSoldier->vitals().breath() == 0 )
	{
		return( 0 );
	}

	// Bob: if gridNo isn't set, this would cause a access violation later on
	if (pSoldier->position().gridNo() < 0) {
		// ScreenMsg(FONT_MCOLOR_LTRED, MSG_INTERFACE, L"DistanceVisible(): Caught bad LOS distance check!");
		return(0);
	}

	// anv: some places in vehicle don't give passenger any view outside
	INT8 bSeatIndex = GetSeatIndexFromSoldier( pSoldier );
	if( bSeatIndex != (-1) )
	{	
		TacticalActor *pVehicle = GetSoldierStructureForVehicle( pSoldier ->deployment().vehicleId() );
		// need to check this even if bSubjectDir is DIRECTION_IRRELEVANT
		if( gNewVehicle[ pVehicleList[ pSoldier->deployment().vehicleId() ].ubVehicleType ].VehicleSeats[ bSeatIndex ].fBlockedView )
		{
			return( 0 );
		}
	}

	if ( bFacingDir == DIRECTION_IRRELEVANT && ARMED_VEHICLE( pSoldier ) )
	{
		// always calculate direction for tanks so we have something to work with
		bFacingDir = pSoldier->pathing().desiredDirection();
		bSubjectDir = (INT8) GetDirectionToGridNoFromGridNo( pSoldier->position().gridNo(), sSubjectGridNo );
		//bSubjectDir = atan8(pSoldier->position().worldXInt(),pSoldier->position().worldYInt(),pOpponent->position().worldXInt(),pOpponent->position().worldYInt());
	}

	if ( !ARMED_VEHICLE( pSoldier ) && (bFacingDir == DIRECTION_IRRELEVANT || (pSoldier->status().flags() & SOLDIER_ROBOT) || (pSubject && pSubject->renderState().muzzleFlashVisible())) )
	{
		sDistVisible = MaxNormalDistanceVisible();
	}
	else
	{
		if (pSoldier->position().gridNo() == sSubjectGridNo)
		{
			// looking up or down or two people accidentally in same tile... don't want it to be 0!
			sDistVisible = MaxNormalDistanceVisible();
		}
		else
		{
			// Flugente: no need to calculate this multiple times.
			// SoldierHasLimitedVision(pSoldier) is exactly
			//   gGameExternalOptions.gfAllowLimitedVision || GetPercentTunnelVision(pSoldier) > 0
			// and every DistanceVisible caller already passes
			// tunnelVision == GetPercentTunnelVision(pSoldier), so reuse it instead of
			// re-scanning the soldier's inventory inside GetPercentTunnelVision again.
			BOOLEAN fLimitedVision = ( gGameExternalOptions.gfAllowLimitedVision || tunnelVision > 0 );

			// Lesh: added this
			if( fLimitedVision )
			{
				bSubjectDir = (INT8) GetDirectionToGridNoFromGridNo( pSoldier->position().gridNo(), sSubjectGridNo );
			}

			if (bFacingDir == DIRECTION_IRRELEVANT)
			{
				bFacingDir = pSoldier->position().direction();
			}

			sDistVisible = gbLookDistance[bFacingDir][bSubjectDir];

			// Lesh: and this
			if ( sDistVisible == 0 && fLimitedVision )
				return 0;

			if ( sDistVisible == ANGLE && (pSoldier->roster().team() == OUR_TEAM || pSoldier->aiBehavior().alertStatus() >= STATUS_RED ) )
			{
				sDistVisible = STRAIGHT;
			}

			if ( sDistVisible != STRAIGHT || ( fLimitedVision && (bFacingDir != bSubjectDir) ) )
			{
				tunnelVisionInPercent = tunnelVision;
			}

			sDistVisible *= 2;

			if ( pSoldier->animationPlayback().state() == RUNNING )
			{
				if ( gbLookDistance[bFacingDir][bSubjectDir] != STRAIGHT )
				{
					// reduce sight when we're not looking in that direction...
					sDistVisible = (INT16) (sDistVisible * ANGLE_RATIO);
				}
			}

			// Flugente: we only apply tunnelvision now, after we've possibly extended the sight range, which results in finer differentiation of effects
			if ( tunnelVisionInPercent > 0 )
			{
				sideViewLimit = TRUE;
				sDistVisible = sDistVisible * (100 - tunnelVisionInPercent) / 100;
			}
		}
	}

	if (pSoldier->position().level() != bLevel)
	{
		// add two tiles distance to visibility to/from roofs
		// sDistVisible += (STRAIGHT_RATIO * 2); //2;
		sDistVisible += ( sDistVisible / 6 ); //lal changed distance to 1/6 from visible range
	}

	// now reduce based on light level; SHADE_MIN is the define for the
	// highest number the light can be

	// If we're about to ask for a light level for a location outside of our
	// valid map references then use the ambient light level instead.	
	if ( TileIsOutOfBounds( sSubjectGridNo ) )
	{
		DebugMsg(TOPIC_JA2, DBG_LEVEL_3, String("113/UC Warning! Tried to detect the light level when character %ls[%d] looks at a location outside of the valid map (gridno %d). Assigning default %d",
				pSoldier->identity().name(), pSoldier->identity().id(), pSoldier->position().gridNo(), ubAmbientLightLevel));

		bLightLevel = ubAmbientLightLevel;
	}
	else
	{
		bLightLevel = LightTrueLevel(sSubjectGridNo, bLevel);
	}

	// Snap: I think this was intended to give maximum visibility to targets with muzzle flash...
	// Corrected accordingly:
	//if ( pSubject && !( pSubject->renderState().muzzleFlashVisible() && (bLightLevel > NORMAL_LIGHTLEVEL_DAY) ) )
	if ( !( pSubject && pSubject->renderState().muzzleFlashVisible() && (bLightLevel > NORMAL_LIGHTLEVEL_DAY) ) )
	{
		// ATE: Made function to adjust light distance...
		sDistVisible = AdjustMaxSightRangeForEnvEffects( pSoldier, bLightLevel, sDistVisible );
	}

	// Snap: this takes care of all equipment bonuses at all light levels
	// The rest is special code for robots, bloodcats and NO specialists
	// Lalien: change to % instead of tiles, add bonus only to front view when using scope
	if (!sideViewLimit)
	{
		sDistVisible += sDistVisible * GetTotalVisionRangeBonus(pSoldier, bLightLevel) / 100;

		// HEADROCK HAM 3.2: Further reduce sightrange for cowering characters.
		// SANDRO - this calls many sub-functions over and over, we should at least skip this for civilians and such  
		// Flugente: we can check for more conditions before calculating suppression tolerance
		if ( (gGameExternalOptions.ubCoweringReducesSightRange == 1 || gGameExternalOptions.ubCoweringReducesSightRange == 2) &&
			IS_MERC_BODY_TYPE(pSoldier) && (pSoldier->roster().team() == ENEMY_TEAM || pSoldier->roster().team() == MILITIA_TEAM || pSoldier->roster().team() == gbPlayerNum) &&
			gGameExternalOptions.ubMaxSuppressionShock > 0 && sDistVisible > 0 )
		{
			// Make sure character is cowering.
			if (isCowering)
			{
				sDistVisible = __max(1,(sDistVisible * (gGameExternalOptions.ubMaxSuppressionShock - pSoldier->suppression().shock())) / gGameExternalOptions.ubMaxSuppressionShock);
			}
		}
	}

	// give one step better vision for people with nightops
	// old/new traits check - SANDRO
	if (gGameOptions.fNewTraitSystem)
	{
		if (HAS_SKILL_TRAIT( pSoldier, NIGHT_OPS_NT ))
			sDistVisible += NightBonusScale( gSkillTraitValues.ubNOeSightRangeBonusInDark, bLightLevel);
	}
	else
	{
		if (HAS_SKILL_TRAIT( pSoldier, NIGHTOPS_OT ))
			sDistVisible += NightBonusScale( 1 * NUM_SKILL_TRAITS( pSoldier, NIGHTOPS_OT ), bLightLevel);
	}
	// Bloodcat bonus only works above ground
	if ( pSoldier->identity().bodyType() == BLOODCAT && gbWorldSectorZ == 0 )
	{
		sDistVisible += NightBonusScale( UVGOGGLES_BONUS, bLightLevel);
	}
	else if ( AM_A_ROBOT( pSoldier ) )
	{
		sDistVisible += NightBonusScale( NIGHTSIGHTGOGGLES_BONUS, bLightLevel);
	}

	// let tanks see and be seen further (at night)
	if ( (ARMED_VEHICLE( pSoldier ) && sDistVisible > 0) || (pSubject && ARMED_VEHICLE( pSubject )) )
	{
		sDistVisible = sDistVisible + 5;
	}

	if ( gpWorldLevelData[ pSoldier->position().gridNo() ].ubExtFlags[ bLevel ] & (MAPELEMENT_EXT_TEARGAS | MAPELEMENT_EXT_MUSTARDGAS) )
	{
		//dnl ch40 200909
		INT8 bPosOfMask = FindGasMask(pSoldier);
		if(bPosOfMask == HEAD1POS || bPosOfMask == HEAD2POS)
		{
			if(pSoldier->inventory()[bPosOfMask][0]->data.objectStatus < GASMASK_MIN_STATUS)
				sDistVisible = __min(sDistVisible, 2+pSoldier->inventory()[bPosOfMask][0]->data.objectStatus/15);
		}
		else
			sDistVisible = __min(sDistVisible, 2);
	}
	if ( gpWorldLevelData[ pSoldier->position().gridNo() ].ubExtFlags[ bLevel ] & MAPELEMENT_EXT_BURNABLEGAS )
	{
		{
			// in FLAMETHERgas ; reduce max distance visible to 2 tiles at most
			sDistVisible = __min( sDistVisible, 2 );
		}
	}

	return(sDistVisible);
}



void EndMuzzleFlash( TacticalActor * pSoldier )
{
	UINT32					uiLoop;
	TacticalActor *		pOtherSoldier;

	pSoldier->renderState().hideMuzzleFlash();
/*comm by ddd
#ifdef WE_SEE_WHAT_MILITIA_SEES_AND_VICE_VERSA
	if ( pSoldier->roster().team() != gbPlayerNum && pSoldier->roster().team() != MILITIA_TEAM )
#else
	if ( pSoldier->roster().team() != gbPlayerNum )
#endif

	{
		pSoldier->awareness().markIndeterminate(); // indeterminate state
	}
*/	


#ifdef ENABLE_MP_FRIENDLY_PLAYERS_SHARE_SAME_FOV
//haydent
if(is_networked &&  pSoldier->roster().side() == 0)
{
	//stay visible
}
else
#endif
{
	if(gGameExternalOptions.bWeSeeWhatMilitiaSeesAndViceVersa)	
	{	if ( pSoldier->roster().team() != gbPlayerNum && pSoldier->roster().team() != MILITIA_TEAM )
			pSoldier->awareness().markIndeterminate(); // indeterminate state
	}
	else
	{
		if ( pSoldier->roster().team() != gbPlayerNum )
			pSoldier->awareness().markIndeterminate(); // indeterminate state
	}
}//haydent



	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOtherSoldier = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		if ( pOtherSoldier != NULL )
		{
			if ( pOtherSoldier->awareness().opponentKnowledge()[ pSoldier->identity().id() ] == SEEN_CURRENTLY )
			{				
				if (!TileIsOutOfBounds(pOtherSoldier->position().gridNo()))
				{
					if ( PythSpacesAway( pOtherSoldier->position().gridNo(), pSoldier->position().gridNo() ) > pOtherSoldier->GetMaxDistanceVisible(pSoldier->position().gridNo(), pSoldier->position().level(), CALC_FROM_WANTED_DIR ) )
					{
						// if this guy can no longer see us, change to seen this turn
						HandleManNoLongerSeen( pOtherSoldier, pSoldier, &(pOtherSoldier->awareness().opponentKnowledge()[ pSoldier->identity().id() ]), &(gbPublicOpplist[ pOtherSoldier->roster().team() ][ pSoldier->identity().id() ] ) );
					}

					// else this person is still seen, if the looker is on our side or the militia the person should stay visible
/* comm by ddd
#ifdef WE_SEE_WHAT_MILITIA_SEES_AND_VICE_VERSA
					else if ( pOtherSoldier->roster().team() == gbPlayerNum || pOtherSoldier->roster().team() == MILITIA_TEAM )
#else
					else if ( pOtherSoldier->roster().team() == gbPlayerNum )
#endif

					{
						pSoldier->awareness().markVisible(); // yes, still seen
					}
*/					
					//ddd{
					else if(gGameExternalOptions.bWeSeeWhatMilitiaSeesAndViceVersa)
					{
						if ( pOtherSoldier->roster().team() == gbPlayerNum || pOtherSoldier->roster().team() == MILITIA_TEAM )
							{pSoldier->awareness().markVisible();} // yes, still seen
					}
					else
					{
						if ( pOtherSoldier->roster().team() == gbPlayerNum )
							pSoldier->awareness().markVisible(); // yes, still seen
					}
					//ddd}

					
#ifdef ENABLE_MP_FRIENDLY_PLAYERS_SHARE_SAME_FOV
					//haydent
					if(is_networked &&  pOtherSoldier->roster().side() == 0 && pOtherSoldier->roster().team() != OUR_TEAM)
						pSoldier->awareness().markVisible(); // yes, still seen
#endif
				}
			}
		}
	}
	DecideTrueVisibility( pSoldier, FALSE );

}

void TurnOffEveryonesMuzzleFlashes( void )
{
	UINT32					uiLoop;
	TacticalActor *		pSoldier;

	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pSoldier = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		if ( pSoldier != NULL && pSoldier->renderState().muzzleFlashVisible() )
		{
			EndMuzzleFlash( pSoldier );
		}
	}
}

void TurnOffTeamsMuzzleFlashes( UINT8 ubTeam )
{
	TacticalActor *		pSoldier;

	for ( SoldierID ubLoop = gTacticalStatus.Team[ ubTeam ].bFirstID; ubLoop <= gTacticalStatus.Team[ ubTeam ].bLastID; ++ubLoop )
	{
		pSoldier = GetJa2SoldierRepository().resolve( ubLoop );
		if ( pSoldier == nullptr )
		{
			continue;
		}

		if ( pSoldier->renderState().muzzleFlashVisible() )
		{
			EndMuzzleFlash( pSoldier );
		}
	}
}

INT8 DecideHearing( TacticalActor * pSoldier )
{
	// calculate the hearing value for the merc...
	INT8		bHearing;

	if ( ARMED_VEHICLE( pSoldier ) || ENEMYROBOT( pSoldier ) )
	{
		return( -5 );
	}
	else if ( pSoldier->status().flags() & SOLDIER_MONSTER )
	{
		return( -10 );
	}

	bHearing = 0;

	if (EffectiveExpLevel( pSoldier ) > 3) // SANDRO - changed to calculate effective level
	{
		bHearing++;
	}

	// old/new traits check - SANDRO
	if (gGameOptions.fNewTraitSystem)
	{
		if (HAS_SKILL_TRAIT( pSoldier, NIGHT_OPS_NT ))
			bHearing += gSkillTraitValues.ubNOHearingRangeBonus;
		if (HAS_SKILL_TRAIT( pSoldier, SNITCH_NT ))
			bHearing += gSkillTraitValues.ubSNTHearingRangeBonus;
	}
	else
	{
		if (HAS_SKILL_TRAIT( pSoldier, NIGHTOPS_OT ))
			bHearing += 1 * NUM_SKILL_TRAITS( pSoldier, NIGHTOPS_OT );
	}

	bHearing += TacticalActorModifiers::hearingBonus(*pSoldier);

	// adjust for dark conditions
	switch ( ubAmbientLightLevel )
	{
		case 8:
		case 9:
			bHearing += 1;
			break;
		case 10:
			bHearing += 2;
			break;
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			bHearing += 3;
			// yet another bonus for nighttime
			// old/new traits check - SANDRO
			if (gGameOptions.fNewTraitSystem)
			{
				if (HAS_SKILL_TRAIT( pSoldier, NIGHT_OPS_NT ))
					bHearing += gSkillTraitValues.ubNOHearingRangeBonusInDark;
			}
			else
			{
				if (HAS_SKILL_TRAIT( pSoldier, NIGHTOPS_OT ))
					bHearing += 1 * NUM_SKILL_TRAITS( pSoldier, NIGHTOPS_OT );
			}
			break;
		default:
			break;
	}

	// adjust for weather
	if ( !pSoldier->deployment().sectorZ() )
	{
		FLOAT weatherpenalty = gGameExternalOptions.dHearingReduction[SectorInfo[SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )].usWeather];

		// Added a feature to reduce rain effect on regaining breath with Ranger trait - SANDRO
		FLOAT appliedpenalty = 1.0f;
		if ( HAS_SKILL_TRAIT( pSoldier, SURVIVAL_NT ) && gGameOptions.fNewTraitSystem )
			appliedpenalty = min( 1.0f, max( 0.0f, appliedpenalty - gSkillTraitValues.dSVWeatherPenaltiesReduction * NUM_SKILL_TRAITS( pSoldier, SURVIVAL_NT ) ) );

		bHearing -= (INT16)(bHearing * weatherpenalty * appliedpenalty);
	}

	return( bHearing );
}

void InitOpplistForDoorOpening( void )
{
	// this is called before generating a noise for opening a door so that
	// the results of hearing the noise are lumped in with the results from AllTeamsLookForAll
	gubBestToMakeSightingSize = BEST_SIGHTING_ARRAY_SIZE_ALL_TEAMS_LOOK_FOR_ALL;
	gfDelayResolvingBestSightingDueToDoor = TRUE; // will be turned off in allteamslookforall
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "HBSPIR: setting door flag on" );
	// must init sight arrays here
	InitSightArrays();
}


void AllTeamsLookForAll(UINT8 ubAllowInterrupts)
{
	TacticalActor *pSoldier;

	if ( (gTacticalStatus.uiFlags & LOADING_SAVED_GAME) )
	{
		return;
	}

	if ( ubAllowInterrupts || !(IsJa2TacticalCombatActive()) )
	{
		gubBestToMakeSightingSize = BEST_SIGHTING_ARRAY_SIZE_ALL_TEAMS_LOOK_FOR_ALL;
		if ( gfDelayResolvingBestSightingDueToDoor )
		{
			// turn off flag now, and skip init of sight arrays
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "HBSPIR: turning door flag off" );
			gfDelayResolvingBestSightingDueToDoor = FALSE;
		}
		else
		{
			InitSightArrays();
		}
	}

	for ( UINT16 uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); ++uiLoop )
	{
		pSoldier = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		if ( pSoldier != NULL && pSoldier->vitals().health() >= OKLIFE )
		{
			HandleSight( pSoldier, SIGHT_LOOK );	// no radio or interrupts yet
		}
	}

	// the player team now radios about all sightings
	for ( SoldierID uiLoop = gTacticalStatus.Team[gbPlayerNum].bFirstID; uiLoop <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++uiLoop )
	{
		TacticalActor* player =
			GetJa2SoldierRepository().resolve( uiLoop );
		if ( player != nullptr )
		{
			HandleSight( player, SIGHT_RADIO );		// looking was done above
		}
	}

	if ( !(IsJa2TacticalCombatActive()) )
	{
		// decide who should get first turn
		HandleBestSightingPositionInRealtime();
		// this could have made us switch to combat mode
		if ( (IsJa2TacticalCombatActive()) )
		{
			gubBestToMakeSightingSize = BEST_SIGHTING_ARRAY_SIZE_INCOMBAT;
		}
		else
		{
			gubBestToMakeSightingSize = BEST_SIGHTING_ARRAY_SIZE_NONCOMBAT;
		}
	}
	else if ( ubAllowInterrupts )
	{
		HandleBestSightingPositionInTurnbased();
		// reset sighting size to 0
		gubBestToMakeSightingSize = BEST_SIGHTING_ARRAY_SIZE_INCOMBAT;
	}

	/*

	// do this here as well as in overhead so the looks/interrupts are combined!

	// if a door was recently opened/closed (doesn't matter if we could see it)
	// this is done here so we can first handle everyone looking through the
	// door, and deal with the resulting opplist changes, interrupts, etc.
	if ( !TileIsOutOfBounds(Status.doorCreakedGridno))
	   {
	   // opening/closing a door makes a bit of noise (constant volume)
	   MakeNoise(Status.doorCreakedGuynum,Status.doorCreakedGridno,TTypeList[Grid[Status.doorCreakedGridno].land],DOOR_NOISE_VOLUME,NOISE_CREAKING,EXPECTED_NOSEND);

	   Status.doorCreakedGridno = NOWHERE;
	   Status.doorCreakedGuynum = NOBODY;
	   }


	// all soldiers now radio their findings (NO interrupts permitted this early!)
	// NEW: our entire team must radio first, so that they radio about EVERYBODY
	// rather radioing about individuals one a a time (repeats see 1 enemy quote)
	for each local-team soldier
	   {
	   if (ptr->active && ptr->in_sector && (ptr->life >= OKLIFE))
		HandleSight(ptr,SIGHT_RADIO);		// looking was done above
	   }

	for each soldier
	   {
	   if (ptr->active && ptr->in_sector && (ptr->life >= OKLIFE) && !PTR_OURTEAM)
		HandleSight(ptr,SIGHT_RADIO);		// looking was done above
	   }


	// if interrupts were allowed
	if (allowInterrupts)
	   // resolve interrupts against the selected character (others disallowed)
	   HandleSight(the selected soldier,SIGHT_INTERRUPT);


	// revert to normal interrupt operation
	InterruptOnlyGuynum = NOBODY;
	InterruptsAllowed = TRUE;
	*/

	// reset interrupt only guynum which may have been used
	gubInterruptProvoker = NOBODY;
}




void ManLooksForOtherTeams(TacticalActor *pSoldier)
{
 UINT32 uiLoop;
 TacticalActor *pOpponent;


#ifdef TESTOPPLIST
 DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
	String("MANLOOKSFOROTHERTEAMS ID %d(%S) team %d side %d",pSoldier->identity().id(),pSoldier->identity().name(),pSoldier->roster().team(),pSoldier->roster().side()));
#endif


	// one soldier (pSoldier) looks for every soldier on another team (pOpponent)


 for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
 {
	pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

	// if this soldier is around and alive
	if (pOpponent && pOpponent->vitals().health())
	{
	 // and if he's on another team...
	 if (pSoldier->roster().team() != pOpponent->roster().team())
	 {

			// use both sides actual x,y co-ordinates (neither side's moving)
			// if he sees this opponent...
			ManLooksForMan(pSoldier,pOpponent,MANLOOKSFOROTHERTEAMS);

			// OK, We now want to , if in non-combat, set visiblity to 0 if not visible still....
			// This allows us to walk away from buddy and have them disappear instantly
			if ( IsJa2TacticalTurnBased() && !( IsJa2TacticalCombatActive() ) )
			{
				if ( pOpponent->awareness().visibility() == 0)
				{
					pOpponent->awareness().markHidden();
				}
			}

		}
	}
 }
}

void HandleManNoLongerSeen( TacticalActor * pSoldier, TacticalActor * pOpponent, INT8 * pPersOL, INT8 * pbPublOL )
{
	// if neither side is neutral AND
	// if this soldier is an opponent (fights for different side)
	if (pSoldier->roster().active() && pOpponent->roster().active() && !CONSIDERED_NEUTRAL( pOpponent, pSoldier ) && !CONSIDERED_NEUTRAL( pSoldier, pOpponent ) && (pSoldier->roster().side() != pOpponent->roster().side()) && TacticalActorCovertOps::recognizesCombatant(*pSoldier, pOpponent->identity().id()) )
	{
		RemoveOneOpponent(pSoldier);
	}

	// change personal opplist to indicate "seen this turn"
	// don't use UpdatePersonal() here, because we're changing to a *lower*
	// opplist value (which UpdatePersonal ignores) and we're not updating
	// the lastKnown gridno at all, we're keeping it at its previous value
	/*
#ifdef RECORDOPPLIST
	fprintf(OpplistFile,"ManLooksForMan: changing personalOpplist to %d for guynum %d, opp %d\n",SEEN_THIS_TURN,ptr->guynum,oppPtr->guynum);
#endif
	*/

	*pPersOL = SEEN_THIS_TURN;

	if ( (pSoldier->roster().civilianGroup() == KINGPIN_CIV_GROUP) && (pOpponent->roster().team() == gbPlayerNum ) )
	{
		//DBrot: More Rooms
		//UINT8 ubRoom;
		UINT16 usRoom;

		if ( InARoom( pOpponent->position().gridNo(), &usRoom ) && IN_BROTHEL( usRoom ) && ( IN_BROTHEL_GUARD_ROOM( usRoom ) ) )
		{
			// unauthorized!
			// make guard run to block guard room
			DebugAI(AI_MSG_INFO, pSoldier, String("CancelAIAction: make guard run to block guard room"));			CancelAIAction( pSoldier, TRUE );
			pSoldier->timing().start(SoldierTimingComponent::Timer::Ai, 0);
			pSoldier->aiPlanning().nextAction() = AI_ACTION_RUN;
			pSoldier->aiPlanning().nextActionData() = 13250;
		}
	}

	// if opponent was seen publicly last time
	if (*pbPublOL == SEEN_CURRENTLY)
	{
		// check if I was the only one who was seeing this guy (exlude ourselves)
		// THIS MUST HAPPEN EVEN FOR ENEMIES, TO MAKE THEIR PUBLIC opplist DECAY!
		if (TeamNoLongerSeesMan(pSoldier->roster().team(), pOpponent, pSoldier->identity().id(), 0))
		{
#ifdef TESTOPPLIST
			DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3, String( "TeamNoLongerSeesMan: ID %d(%S) to ID %d",pSoldier->identity().id(),pSoldier->identity().name(),pOpponent->identity().id()) );
#endif


#ifdef RECORDOPPLIST
			fprintf(OpplistFile,"TeamNoLongerSeesMan returns TRUE for team %d, opp %d\n",ptr->team,oppPtr->guynum);
			fprintf(OpplistFile,"ManLooksForMan: changing publicOpplist to %d for team %d, opp %d\n",SEEN_THIS_TURN,ptr->team,oppPtr->guynum);
#endif

			// don't use UpdatePublic() here, because we're changing to a *lower*
			// opplist value (which UpdatePublic ignores) and we're not updating
			// the lastKnown gridno at all, we're keeping it at its previous value
			*pbPublOL = SEEN_THIS_TURN;

			// ATE: Set visiblity to 0
/*comm by ddd			
#ifdef WE_SEE_WHAT_MILITIA_SEES_AND_VICE_VERSA
			if ( (pSoldier->roster().team() == gbPlayerNum || pSoldier->roster().team() == MILITIA_TEAM) && !(pOpponent->roster().team() == gbPlayerNum || pOpponent->roster().team() == MILITIA_TEAM ) )
#else
			if ( pSoldier->roster().team() == gbPlayerNum && pOpponent->roster().team() != gbPlayerNum )
#endif
			{
				pOpponent->awareness().markIndeterminate();
			}
*/			


#ifdef ENABLE_MP_FRIENDLY_PLAYERS_SHARE_SAME_FOV
			//haydent
			if(is_networked && pSoldier->roster().side() == 0)
			{
				//stay visible
			}
			else
#endif
			{
				if(gGameExternalOptions.bWeSeeWhatMilitiaSeesAndViceVersa)
				{
					if ( (pSoldier->roster().team() == gbPlayerNum || pSoldier->roster().team() == MILITIA_TEAM) && !(pOpponent->roster().team() == gbPlayerNum || pOpponent->roster().team() == MILITIA_TEAM ) )
						pOpponent->awareness().markIndeterminate();
				}
				else
				{ 
					if ( pSoldier->roster().team() == gbPlayerNum && pOpponent->roster().team() != gbPlayerNum )
						pOpponent->awareness().markIndeterminate();
				}
			}//haydent
		}
	}
#ifdef TESTOPPLIST
	else
		DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3, String("ManLooksForMan: ID %d(%S) to ID %d Personally seen, public %d",pSoldier->identity().id(),pSoldier->identity().name(),pOpponent->identity().id(),*pbPublOL) );
#endif

	// if we had only seen the guy for an instant and now lost sight of him
	if (gbSeenOpponents[pSoldier->identity().id()][pOpponent->identity().id()] == -1)
		// we can't leave it -1, because InterruptDuel() uses the special -1
		// value to know if we're only JUST seen the guy and screw up otherwise
		// it's enough to know we have seen him before
		gbSeenOpponents[pSoldier->identity().id()][pOpponent->identity().id()] = TRUE;

}


INT16 ManLooksForMan(TacticalActor *pSoldier, TacticalActor *pOpponent, UINT8 ubCaller)
{
 INT8 bDir,bAware = FALSE,bSuccess = FALSE;
 INT16 sDistVisible,sDistAway;
 INT8	*pPersOL,*pbPublOL;


 /*
 if (ptr->guynum >= TOTAL_SOLDIERS)
	{
#ifdef BETAVERSION
	NumMessage("ManLooksForMan: ERROR - ptr->guynum = ",ptr->guynum);
#endif
	return(success);
	}

 if (oppPtr->guynum >= TOTAL_SOLDIERS)
	{
#ifdef BETAVERSION
	NumMessage("ManLooksForMan: ERROR - oppPtr->guynum = ",oppPtr->guynum);
#endif
	return(success);
	}
*/

	// if we're somehow looking while inactive, at base, dead or dying
	if (!pSoldier->roster().active() || !pSoldier->roster().inSector() || (pSoldier->vitals().health() < OKLIFE))
	{
/*
#ifdef BETAVERSION
	sprintf(tempstr,"ManLooksForMan: ERROR - %s is looking while inactive/at base/dead/dying.	Caller %s",
			ExtMen[ptr->guynum].name,LastCaller2Text[caller]);

#ifdef RECORDNET
	fprintf(NetDebugFile,"\n\t%s\n\n",tempstr);
#endif

	PopMessage(tempstr);
#endif
*/

#ifdef TESTOPPLIST
	DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
			String("ERROR: ManLooksForMan - WE are inactive/dead etc ID %d(%S)to ID %d",pSoldier->identity().id(),pSoldier->identity().name(),pOpponent->identity().id()) );
#endif

		return(FALSE);
	}

	// if we're somehow looking for a guy who is inactive, at base, or already dead 
	if (!pOpponent->roster().active() || !pOpponent->roster().inSector() || pOpponent->vitals().health() <= 0 || TileIsOutOfBounds(pOpponent->position().gridNo()))
	{
/*
#ifdef BETAVERSION
	sprintf(tempstr,"ManLooksForMan: ERROR - %s looks for %s, who is inactive/at base/dead.	Caller %s",
		ExtMen[ptr->guynum].name,ExtMen[oppPtr->guynum].name,LastCaller2Text[caller]);

#ifdef RECORDNET
	fprintf(NetDebugFile,"\n\t%s\n\n",tempstr);
#endif

	PopMessage(tempstr);
#endif
*/

#ifdef TESTOPPLIST
		DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
			String("ERROR: ManLooksForMan - TARGET is inactive etc ID %d(%S)to ID %d",pSoldier->identity().id(),pSoldier->identity().name(),pOpponent->identity().id()) );
#endif

		return(FALSE);
	}


	// if he's looking for a guy who is on the same team
	if (pSoldier->roster().team() == pOpponent->roster().team())
	{
/*
#ifdef BETAVERSION
	sprintf(tempstr,"ManLooksFormMan: ERROR - on SAME TEAM.	ptr->guynum = %d, oppPtr->guynum = %d",
					ptr->guynum,oppPtr->guynum);
#ifdef RECORDNET
	fprintf(NetDebugFile,"\n\t%s\n\n",tempstr);
#endif

	PopMessage(tempstr);
#endif
*/

#ifdef TESTOPPLIST
	DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
			String("ERROR: ManLooksForMan - SAME TEAM ID %d(%S)to ID %d",pSoldier->identity().id(),pSoldier->identity().name(),pOpponent->identity().id()) );
#endif

		return(FALSE);
	}

	// Flugente: we already checked for OKLIFE above..
	if ( pSoldier->assignment().isAsleep() )
	{
		return( FALSE );
	}

 // NEED TO CHANGE THIS
 /*
 // don't allow unconscious persons to look, but COLLAPSED, etc. is OK
 if (ptr->anitype[ptr->anim] == UNCONSCIOUS)
	return(success);
*/

	if ( pSoldier->identity().bodyType() == LARVAE_MONSTER || (pSoldier->status().flags() & SOLDIER_VEHICLE && pSoldier->roster().team() == OUR_TEAM) )
	{
		// don't do sight for these
		return( FALSE );
	}


 /*
 if (ptrProjected)
	{
	// use looker's PROJECTED x,y co-ordinates (those of his next gridno)
	fromX = ptr->destx;
	fromY = ptr->desty;
	fromGridno = ExtMen[ptr->guynum].nextGridno;
	}
 else
	{
	// use looker's ACTUAL x,y co-ordinates (those of gridno he's in now)
	fromX = ptr->x;
	fromY = ptr->y;
	fromGridno = ptr->sGridNo;
	}


 if (oppPtrProjected)
	{
	// use target's PROJECTED x,y co-ordinates (those of his next gridno)
	toX = oppPtr->destx;
	toY = oppPtr->desty;
	toGridno = ExtMen[oppPtr->guynum].nextGridno;
	}
 else
	{
	// use target's ACTUAL x,y co-ordinates (those of gridno he's in now)
	toX = oppPtr->x;
	toY = oppPtr->y;
	toGridno = oppPtr->gridno;
	}
*/

	pPersOL = &(pSoldier->awareness().opponentKnowledge()[pOpponent->identity().id()]);
	pbPublOL = &(gbPublicOpplist[pSoldier->roster().team()][pOpponent->identity().id()]);

	// if soldier is known about (SEEN or HEARD within last few turns)
	if (*pPersOL || *pbPublOL)
	{
		bAware = TRUE;

		// then we look for him full viewing distance in EVERY direction

		//ADB the comment above says EVERY direction but the code used to be:
		//sDistVisible = DistanceVisible(pSoldier, (SoldierHasLimitedVision(pSoldier) ? pSoldier->bDesiredDirection : DIRECTION_IRRELEVANT), 0, pOpponent->sGridNo, pOpponent->bLevel, pOpponent );
		//if the code below says CALC_FROM_ALL_DIRS, then the opponent will NOT be greyed out if a merc sees him and a second merc turns away from him
		//calcing from the wanted dir will make the opponent be greyed out, which I think is the intended effect
		sDistVisible = pSoldier->GetMaxDistanceVisible( pOpponent->position().gridNo(), pOpponent->position().level(), CALC_FROM_WANTED_DIR, pOpponent );
		//if (pSoldier->identity().id() == 0)
		//sprintf(gDebugStr,"ALREADY KNOW: ME %d him %d val %d",pSoldier->identity().id(),pOpponent->identity().id(),pSoldier->bOppList[pOpponent->identity().id()]);
	}
	else   // soldier is not currently known about
	{
		// distance we "see" then depends on the direction he is located from us
		bDir = atan8(pSoldier->position().worldXInt(),pSoldier->position().worldYInt(),pOpponent->position().worldXInt(),pOpponent->position().worldYInt());
		// BIG NOTE: must use desdir instead of direction, since in a projected
		// situation, the direction may still be changing if it's one of the first
		// few animation steps when this guy's turn to do his stepped look comes up
		sDistVisible = DistanceVisible(pSoldier, pSoldier->pathing().desiredDirection(), bDir, pOpponent->position().gridNo(), pOpponent->position().level(), TacticalActorConditions::isCowering(*pSoldier), GetPercentTunnelVision(pSoldier), pOpponent);
		//if (pSoldier->identity().id() == 0)
		//sprintf(gDebugStr,"dist visible %d: my dir %d to him %d",sDistVisible,pSoldier->bDesiredDirection,bDir);
	}

	// calculate how many spaces away soldier is (using Pythagoras' theorem)
	sDistAway = PythSpacesAway(pSoldier->position().gridNo(),pOpponent->position().gridNo());

#ifdef TESTOPPLIST
	DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3, String( "MANLOOKSFORMAN: ID %d(%S) to ID %d: sDistAway %d sDistVisible %d",pSoldier->identity().id(),pSoldier->identity().name(),pOpponent->identity().id(),sDistAway,sDistVisible) );
#endif

	// if we see close enough to see the soldier
	if (sDistAway <= sDistVisible)
	{
		// and we can trace a line of sight to his x,y coordinates
		// must use the REAL opplist value here since we may or may not know of him
		if (SoldierToSoldierLineOfSightTest(pSoldier,pOpponent,bAware,sDistVisible))
		{
			ManSeesMan(pSoldier,pOpponent,pOpponent->position().gridNo(),pOpponent->position().level(),MANLOOKSFORMAN,ubCaller);
			bSuccess = TRUE;
		}
#ifdef TESTOPPLIST
	else
			DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3, String("FAILED LINEOFSIGHT: ID %d (%S)to ID %d Personally %d, public %d",pSoldier->identity().id(),pSoldier->identity().name(),pOpponent->identity().id(),*pPersOL,*pbPublOL) );
#endif

/*
	// if we're looking for a local merc, and changed doors were in the way
	if (PTR_OURTEAM && (NextFreeDoorIndex > 0))
	 // make or fail, if we passed through any "changed" doors along the way,
	 // reveal their true status (change the structure to its real value)
	 // (do this even if we don't have LOS, to close doors that *BREAK* LOS)
	 RevealDoorsAlongLOS();
*/
	}



/*
#ifdef RECORDOPPLIST
 fprintf(OpplistFile,"MLFM: %s by %2d(g%4d,x%3d,y%3d,%s) at %2d(g%4d,x%3d,y%3d,%s), aware %d, dA=%d,dV=%d, desDir=%d, %s\n",
		(success) ? "SCS" : "FLR",
		ptr->guynum,fromGridno,fromX,fromY,(ptrProjected)?"PROJ":"REG.",
		oppPtr->guynum,toGridno,toX,toY,(oppPtrProjected)?"PROJ":"REG.",
		aware,distAway,distVisible,ptr->desdir,
		LastCaller2Text[caller]);
#endif
*/

	// if soldier seen personally LAST time could not be seen THIS time
	if (!bSuccess && (*pPersOL == SEEN_CURRENTLY))
	{
		HandleManNoLongerSeen( pSoldier, pOpponent, pPersOL, pbPublOL );
	}
#ifdef TESTOPPLIST
	else
	{
		if (!bSuccess)
		{
			DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3, String("NO LONGER VISIBLE ID %d (%S)to ID %d Personally %d, public %d success: %d",pSoldier->identity().id(),pSoldier->identity().name(),pOpponent->identity().id(),*pPersOL,*pbPublOL,bSuccess) );

			// we didn't see the opponent, but since we didn't last time, we should be
			//if (*pbPublOL)
				//pOpponent->awareness().markVisible();
		}
		else
			DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3, String("COOL. STILL VISIBLE ID %d (%S)to ID %d Personally %d, public %d success: %d",pSoldier->identity().id(),pSoldier->identity().name(),pOpponent->identity().id(),*pPersOL,*pbPublOL,bSuccess) );
	}
#endif
	
	return(bSuccess);
}




void ManSeesMan(TacticalActor *pSoldier, TacticalActor *pOpponent, INT32 sOppGridNo, INT8 bOppLevel, UINT8 ubCaller, UINT8 ubCaller2)
{
	INT8 bDoLocate = FALSE;
	BOOLEAN fNewOpponent = FALSE;
	BOOLEAN fNotAddedToList = TRUE;
	INT8 bOldOppList = pSoldier->awareness().opponentKnowledge()[pOpponent->identity().id()];
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"ManSeesMan");

	if (pSoldier->identity().id() >= TOTAL_SOLDIERS)
		return;
	if (pOpponent->identity().id() >= TOTAL_SOLDIERS)
		return;
	// if we're somehow looking while inactive, at base, dying or already dead
	if (!pSoldier->roster().active() || !pSoldier->roster().inSector() || (pSoldier->vitals().health() < OKLIFE))
		return;
	// if we're somehow seeing a guy who is inactive, at base, or already dead
	if (!pOpponent->roster().active() || !pOpponent->roster().inSector() || pOpponent->vitals().health() <= 0)
		return;
	// if we're somehow seeing a guy who is on the same team
	if (pSoldier->roster().team() == pOpponent->roster().team())
		return;
	// Flugente: if the other guy is in med or deep water and wearing scua gear, then we cannot see him as he is submerged
	if ( TacticalActorEquipment::usesScubaGear(*pOpponent) )
		return;
	// Flugente: update our sight concerning this guy, otherwise we could get way with open attacks because this does not get updated
	if (TacticalActorCovertOps::recognizesCombatant(*pSoldier, pOpponent->identity().id()))
	{
		// Flugente: note that this enemy has been seen by mercs this turn
		if ( pOpponent->roster().team() == ENEMY_TEAM && pSoldier->roster().team() == OUR_TEAM )
			pOpponent->featureFlags().primaryFlags() |= SOLDIER_ENEMY_OBSERVEDTHISTURN;
	}
	// sevenfm: if soldier is unconscious, he can't see anybody
	if ( pSoldier->collapseState().tactical() && pSoldier->vitals().breath() == 0 )
	{
		return;
	}

	// if we're seeing a guy we didn't see on our last chance to look for him
	if (pSoldier->awareness().opponentKnowledge()[pOpponent->identity().id()] != SEEN_CURRENTLY)
	{
		if ( pOpponent->roster().team() == gbPlayerNum )
		{
			if ( pSoldier->identity().profile() != NO_PROFILE )
			{
				if ( pSoldier->roster().team() == CIV_TEAM )
				{
					// if this person doing the sighting is a member of a civ group that hates us but
					// this fact hasn't been revealed, change the side of these people now. This will
					// make them non-neutral so AddOneOpponent will be called, and the guy will say his
					// "I hate you" quote
					if ( pSoldier->aiBehavior().neutral() )
					{
						if ( pSoldier->roster().civilianGroup() != NON_CIV_GROUP && gTacticalStatus.fCivGroupHostile[ pSoldier->roster().civilianGroup() ] >= CIV_GROUP_WILL_BECOME_HOSTILE )
						{
							AddToShouldBecomeHostileOrSayQuoteList( pSoldier->identity().id() );
							fNotAddedToList = FALSE;
						}
					}
					else if ( NPCHasUnusedRecordWithGivenApproach( pSoldier->identity().profile(), APPROACH_DECLARATION_OF_HOSTILITY ) )
					{
						// only add if have something to say
						AddToShouldBecomeHostileOrSayQuoteList( pSoldier->identity().id() );
						fNotAddedToList = FALSE;
					}

					if ( fNotAddedToList )
					{
						switch( pSoldier->identity().profile() )
						{
						case CARMEN:
							if ( !GetGameContext().capabilities().isUnfinishedBusiness() &&
							     CampaignProfileCode::matches(
								     GameCampaign::Arulco,
								     CampaignProfileCode::Role::Slay,
								     pOpponent->identity().profile() ) )
							{
								// Carmen goes to war (against Slay)
								if ( pSoldier->aiBehavior().neutral() )
								{
									//SetSoldierNonNeutral( pSoldier );
									pSoldier->aiBehavior().attitude() = ATTACKSLAYONLY;
									TriggerNPCRecord( pSoldier->identity().profile(), 28 );
								}
								/*
								if ( ! IsJa2TacticalCombatActive() )
								{
								EnterCombatMode( pSoldier->roster().team() );
								}
								*/

							}


							break;
						case ELDIN:
							if ( pSoldier->aiBehavior().neutral() )
							{
								//DBrot: More Rooms
								//UINT8 ubRoom = 0;
								UINT16 usRoom = 0;
								// if player is in behind the ropes of the museum display
								// or if alarm has gone off (status red)
								InARoom( pOpponent->position().gridNo(), &usRoom );

								if ( ( CheckFact( FACT_MUSEUM_OPEN, 0 ) == FALSE && usRoom >= 22 && usRoom <= 41 ) || CheckFact( FACT_MUSEUM_ALARM_WENT_OFF, 0 ) || ( usRoom == 39 || usRoom == 40 ) || ( FindObj( pOpponent, CHALICE ) != NO_SLOT ) )
								{
									SetFactTrue( FACT_MUSEUM_ALARM_WENT_OFF );
									AddToShouldBecomeHostileOrSayQuoteList( pSoldier->identity().id() );
								}
							}
							break;	
						case JIM:
						case JACK:
						case OLAF:
						case RAY:
						case OLGA:
						case TYRONE:
							// change orders, reset action!
							if ( pSoldier->aiBehavior().orders() != SEEKENEMY )
							{
								pSoldier->aiBehavior().orders() = SEEKENEMY;
								if ( pSoldier->awareness().opponentCount() == 0 )
								{
									// didn't see anyone before!
									DebugAI(AI_MSG_INFO, pSoldier, String("CancelAIAction: NPC with profile: change orders, reset action"));
									CancelAIAction( pSoldier, TRUE );
									SetNewSituation( pSoldier );
								}
							}
							break;
						case ANGEL:
							if ( pOpponent->identity().profile() == MARIA )
							{
								if ( CheckFact( FACT_MARIA_ESCORTED_AT_LEATHER_SHOP, MARIA ) == TRUE )
								{
									// she was rescued! yay!
									TriggerNPCRecord( ANGEL, 12 );
								}
							}
							else if ( ( CheckFact( FACT_ANGEL_LEFT_DEED, ANGEL ) == TRUE ) && ( CheckFact( FACT_ANGEL_MENTIONED_DEED, ANGEL ) == FALSE ) )
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("CancelAIAction: NPC: Angel code"));
								CancelAIAction( pSoldier, TRUE );
								pSoldier->movement().absoluteDestination() = NOWHERE;
								pSoldier->EVENT_StopMerc( pSoldier->position().gridNo(), pSoldier->position().direction() );
								TriggerNPCRecord( ANGEL, 20 );
								// trigger Angel to walk off afterwards
								//TriggerNPCRecord( ANGEL, 24 );
							}
							break;
							//case QUEEN:
						case JOE:
						case ELLIOT:
							if ( !GetGameContext().capabilities().isUnfinishedBusiness() &&
							     ! ( gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags2 & PROFILE_MISC_FLAG2_SAID_FIRSTSEEN_QUOTE ) )
							{
								if ( !AreInMeanwhile() )
								{
									TriggerNPCRecord( pSoldier->identity().profile(), 4 );
									gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags2 |= PROFILE_MISC_FLAG2_SAID_FIRSTSEEN_QUOTE;
								}
							}
							break;
						default:
							break;
						}
					}
				}
				else
				{
					switch( pSoldier->identity().profile() )
					{
					case IGGY:
						if ( ! ( gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags2 & PROFILE_MISC_FLAG2_SAID_FIRSTSEEN_QUOTE ) )
						{
							TriggerNPCRecord( pSoldier->identity().profile(), 9 );
							gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags2 |= PROFILE_MISC_FLAG2_SAID_FIRSTSEEN_QUOTE;
							gbPublicOpplist[ gbPlayerNum ][ pSoldier->identity().id() ] = HEARD_THIS_TURN;
						}
						break;
					}
				}
			}
			// Flugente: for assassins without profiles
			else if ( TacticalActorConditions::isAssassin(*pSoldier) && pSoldier->roster().team() == CIV_TEAM )
			{
				// if we are an assassin and still neutral and undercover, approach target and then become hostile
				if ( pSoldier->aiBehavior().neutral() && pSoldier->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV|SOLDIER_COVERT_SOLDIER) )
				{
					// only if this guy isn't disguised himself!
					if ( (pOpponent->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV|SOLDIER_COVERT_SOLDIER)) == 0)
					{
						if ( pSoldier->roster().civilianGroup() != NON_CIV_GROUP && gTacticalStatus.fCivGroupHostile[ pSoldier->roster().civilianGroup() ] >= CIV_GROUP_WILL_BECOME_HOSTILE )
						{
							// measure distance to our opponent, only go hostile if he is close enough
							if ( PythSpacesAway( pSoldier->position().gridNo(), pOpponent->position().gridNo() ) <= NPC_TALK_RADIUS * 2 )
							{
								AddToShouldBecomeHostileOrSayQuoteList( pSoldier->identity().id() );
								fNotAddedToList = FALSE;
							}
						}
					}
				}

				if ( fNotAddedToList )
				{
					// change orders, reset action!
					if ( pSoldier->aiBehavior().orders() != SEEKENEMY )
					{
						pSoldier->aiBehavior().orders() = SEEKENEMY;
						if ( pSoldier->awareness().opponentCount() == 0 )
						{
							// didn't see anyone before!
							DebugAI(AI_MSG_INFO, pSoldier, String("CancelAIAction: assasin: didn't see anyone before!"));
							CancelAIAction( pSoldier, TRUE );
							SetNewSituation( pSoldier );
						}
					}
				}
			}
			else
			{
				if ( pSoldier->roster().team() == CIV_TEAM )
				{
					if ( pSoldier->roster().civilianGroup() != NON_CIV_GROUP && gTacticalStatus.fCivGroupHostile[ pSoldier->roster().civilianGroup() ] >= CIV_GROUP_WILL_BECOME_HOSTILE && pSoldier->aiBehavior().neutral() )
					{
						AddToShouldBecomeHostileOrSayQuoteList( pSoldier->identity().id() );
					}
					else if ( pSoldier->roster().civilianGroup() == KINGPIN_CIV_GROUP )
					{
						// generic kingpin goon...

						// check to see if we are looking at Maria or unauthorized personnel in the brothel
						if (pOpponent->identity().profile() == MARIA)
						{
							MakeCivHostile(pSoldier);
							if ( ! (IsJa2TacticalCombatActive()) )
							{
								EnterCombatMode( pSoldier->roster().team() );
							}
							SetFactTrue( FACT_MARIA_ESCAPE_NOTICED );
						}
						else
						{
							//DBrot: More Rooms
							//UINT8 ubRoom;
							UINT16 usRoom;

							// JA2 Gold: only go hostile if see player IN guard room
							//if ( InARoom( pOpponent->sGridNo, &ubRoom ) && IN_BROTHEL( ubRoom ) && ( gMercProfiles[ MADAME ].bNPCData == 0 || IN_BROTHEL_GUARD_ROOM( ubRoom ) ) )
							if ( InARoom( pOpponent->position().gridNo(), &usRoom ) && IN_BROTHEL_GUARD_ROOM( usRoom ) )
							{
								// unauthorized!
								MakeCivHostile(pSoldier);
								if ( ! (IsJa2TacticalCombatActive()) )
								{
									EnterCombatMode( pSoldier->roster().team() );
								}
							}
						}
					}
					else if ( pSoldier->roster().civilianGroup() == HICKS_CIV_GROUP && CheckFact( FACT_HICKS_MARRIED_PLAYER_MERC, 0 ) == FALSE && TacticalActorCovertOps::recognizesCombatant(*pSoldier, pOpponent->identity().id()) )
					{
						UINT32	uiTime;
						INT16	sX, sY;

						// if before 6:05 or after 22:00, make hostile and enter combat
						uiTime = GetWorldMinutesInDay();
						if ( uiTime < 365 || uiTime > 1320 )
						{
							// get off our farm!
							MakeCivHostile(pSoldier);
							if ( ! (IsJa2TacticalCombatActive()) )
							{
								EnterCombatMode( pSoldier->roster().team() );

								LocateSoldier( pSoldier->identity().id(), TRUE );
								GetSoldierScreenPos( pSoldier, &sX, &sY );
								// begin quote
								BeginCivQuote( pSoldier, CIV_QUOTE_HICKS_SEE_US_AT_NIGHT, 0, sX, sY );
							}
						}
					}
				}
			}
		}
		else if ( pSoldier->roster().team() == gbPlayerNum )
		{
			if ( GetGameContext().capabilities().isUnfinishedBusiness() &&
				 (pOpponent->identity().profile() == MORRIS_UB ) &&
				 ( GetNumSoldierIdAndProfileIdOfTheNewMercsOnPlayerTeam( NULL, NULL ) > 0 ) &&
				 !pSoldier->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_EXT_MORRIS) &&
				 !( gMercProfiles[ MORRIS_UB ].ubMiscFlags2 & PROFILE_MISC_FLAG2_SAID_FIRSTSEEN_QUOTE ) )
			{
				gfMorrisShouldSayHi = TRUE;
			}
			else if ( !GetGameContext().capabilities().isUnfinishedBusiness() &&
			          (pOpponent->identity().profile() == MIKE) &&
			          ( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC ||
			            pSoldier->employment().mercenaryType() == MERC_TYPE__MERC ) &&
			          !pSoldier->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_EXT_MIKE) )
			{
				if (gfMikeShouldSayHi == FALSE)
				{
					gfMikeShouldSayHi = TRUE;
				}
				TacticalCharacterDialogue( pSoldier, QUOTE_AIM_SEEN_MIKE );
				pSoldier->dialogue().markSaidExtended(SOLDIER_QUOTE_SAID_EXT_MIKE);
			}
			else if ( pOpponent->identity().profile() == JOEY && gfPlayerTeamSawJoey == FALSE )
			{
				TacticalCharacterDialogue( pSoldier, QUOTE_SPOTTED_JOEY );
				gfPlayerTeamSawJoey = TRUE;
			}
		}

		// as soon as a bloodcat sees someone, it becomes hostile
		// this is safe to do here because we haven't made this new person someone we've seen yet
		// (so we are assured we won't count 'em twice for oppcnt purposes)
		if ( pSoldier->identity().bodyType() == BLOODCAT )
		{
			if ( pSoldier->aiBehavior().neutral() )
			{
				MakeBloodcatsHostile();
				/*
				SetSoldierNonNeutral( pSoldier );
				RecalculateOppCntsDueToNoLongerNeutral( pSoldier );
				if ( ( IsJa2TacticalCombatActive() ) )
				{
				CheckForPotentialAddToBattleIncrement( pSoldier );
				}
				*/

				//PlayJA2Sample( BLOODCAT_ROAR, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );
				PlayJA2Sample( BLOODCAT_ROAR, RATE_11025, MIDVOLUME +10, 1, MIDDLEPAN );
			}
			else
			{
				if ( pSoldier->awareness().opponentCount() == 0 )
				{
					if ( Random( 2 ) == 0 )
					{
						//PlayJA2Sample( BLOODCAT_ROAR, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );
						PlayJA2Sample( BLOODCAT_ROAR, RATE_11025, MIDVOLUME + 10, 1, MIDDLEPAN );
					}
				}
			}
		}
		else if ( pOpponent->identity().bodyType() == BLOODCAT && pOpponent->aiBehavior().neutral())
		{
			// HEADROCK HAM 3.6: If bloodcats are set as affiliated with civilians, do not trigger hostilities.
			UINT8 DiffLevel;
			if( gGameOptions.ubDifficultyLevel == DIF_LEVEL_EASY )
				DiffLevel = 1;
			else if( gGameOptions.ubDifficultyLevel == DIF_LEVEL_MEDIUM )
				DiffLevel = 2;
			else if( gGameOptions.ubDifficultyLevel == DIF_LEVEL_HARD )
				DiffLevel = 3;
			else if( gGameOptions.ubDifficultyLevel == DIF_LEVEL_INSANE )
				DiffLevel = 4;	
			else
				DiffLevel = 1;
				
			if ( gBloodcatPlacements[SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY())][ 0 ].PlacementType != BLOODCAT_PLACEMENT_STATIC ||
				gBloodcatPlacements[SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY())][ DiffLevel - 1 ].ubFactionAffiliation == NON_CIV_GROUP ||
				gBloodcatPlacements[SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY())][ DiffLevel - 1 ].ubFactionAffiliation == QUEENS_CIV_GROUP )
			/*
			if ( gBloodcatPlacements[SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY())][ 0 ].PlacementType != BLOODCAT_PLACEMENT_STATIC ||
				gBloodcatPlacements[SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY())][ gGameOptions.ubDifficultyLevel - 1 ].ubFactionAffiliation == NON_CIV_GROUP ||
				gBloodcatPlacements[SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY())][ gGameOptions.ubDifficultyLevel - 1 ].ubFactionAffiliation == QUEENS_CIV_GROUP )
			*/
			{
				MakeBloodcatsHostile();
			}
			/*
			SetSoldierNonNeutral( pOpponent );
			RecalculateOppCntsDueToNoLongerNeutral( pOpponent );
			if ( ( IsJa2TacticalCombatActive() ) )
			{
			CheckForPotentialAddToBattleIncrement( pOpponent );
			}
			*/
		}

		// Flugente: reworked this to account for covert ops and assassin mechanisms
		// if we are not neutral against this guy, we are truly opponents (we're not on the same side) and recognize him as an opponent...
		BOOLEAN fAddAsOpponent = FALSE;
		if ( !CONSIDERED_NEUTRAL( pSoldier, pOpponent ) && (pSoldier->roster().side() != pOpponent->roster().side()) && TacticalActorCovertOps::recognizesCombatant(*pSoldier, pOpponent->identity().id()) )
		{
			// ... check wether he is not neutral against us (account for the fact that we might be covert!)
			// if we are an NPC assassin
			if ( TacticalActorConditions::isAssassin(*pSoldier) && pSoldier->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER) )
			{
				// check wether our opponent would see us as an opponent if we weren't covert
				if ( !( (pSoldier->aiBehavior().neutral() || pSoldier->featureFlags().primaryFlags() & SOLDIER_POW) && ( pOpponent->roster().team() != CREATURE_TEAM || pOpponent->status().flags() & SOLDIER_VEHICLE ) ) )
					fAddAsOpponent = TRUE;
			}
			else
			{
				// simply check wether this guy sees us as an opponent too
				if ( !CONSIDERED_NEUTRAL( pOpponent, pSoldier ) )
					fAddAsOpponent = TRUE;
			}
		}


		if ( fAddAsOpponent )
		{
			AddOneOpponent(pSoldier);

#ifdef TESTOPPLIST
			DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3, String( "ManSeesMan: ID %d(%S) to ID %d NEW TO ME",pSoldier->identity().id(),pSoldier->identity().name(),pOpponent->identity().id()) );
#endif
			// SANDRO - if this is an enemy guy, who was unaware of us till now, and the combat didn't started yet, throw "taunt" and indicator we have been seen
			if ( pSoldier->awareness().opponentKnowledge()[pOpponent->identity().id()] <= NOT_HEARD_OR_SEEN &&	pSoldier->aiBehavior().alertStatus() != STATUS_RED && pSoldier->aiBehavior().alertStatus() != STATUS_BLACK )
			{
				PossiblyStartEnemyTaunt( pSoldier, TAUNT_NOTICED_UNSEEN, pOpponent->identity().id() );
			}
			// anv: we're already in fight, but we still can say hi to new enemy
			else if ( pSoldier->awareness().opponentKnowledge()[pOpponent->identity().id()] <= NOT_HEARD_OR_SEEN )
			{
				PossiblyStartEnemyTaunt( pSoldier, TAUNT_SAY_HI, pOpponent->identity().id() );
			}

			ShowRadioLocator( pSoldier->identity().id(), 1 );


			// if we also haven't seen him earlier this turn
			if (pSoldier->awareness().opponentKnowledge()[pOpponent->identity().id()] != SEEN_THIS_TURN)
			{
				fNewOpponent = TRUE;
				pSoldier->awareness().recordNewOpponent();        // increment looker's NEW opponent count
				//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Soldier %d sees soldier %d!", pSoldier->identity().id(), pOpponent->identity().id() );

				//ExtMen[ptr->guynum].lastCaller = caller;
				//ExtMen[ptr->guynum].lastCaller2 = caller2;

				IncrementWatchedLoc( pSoldier->identity().id(), pOpponent->position().gridNo(), pOpponent->position().level() );

				if ( pSoldier->roster().team() == OUR_TEAM && pOpponent->roster().team() == ENEMY_TEAM )
				{
					if ( CheckFact( FACT_FIRST_BATTLE_FOUGHT, 0 ) == FALSE )
					{
						SetFactTrue( FACT_FIRST_BATTLE_BEING_FOUGHT );
					}
				}
			}
			else
			{
				SetWatchedLocAsUsed( pSoldier->identity().id(), pOpponent->position().gridNo(), pOpponent->position().level() );
			}

			// we already know the soldier isn't SEEN_CURRENTLY,
			// now check if he is really "NEW" ie. not expected to be there

			// if the looker hasn't seen this opponent at all earlier this turn, OR
			// if the opponent is not where the looker last thought him to be
			// sevenfm: only call SetNewSituation if location is different to reduce frequency of AI re-evaluation
			//if ((pSoldier->awareness().opponentKnowledge()[pOpponent->identity().id()] != SEEN_THIS_TURN) || (gsLastKnownOppLoc[pSoldier->identity().id()][pOpponent->identity().id()] != sOppGridNo))
			if (KnownLocation(pSoldier, pOpponent->identity().id()) != sOppGridNo || KnownLevel(pSoldier, pOpponent->identity().id()) != bOppLevel)
			{
				SetNewSituation( pSoldier );  // force the looker to re-evaluate
				// anv: simulate informing buddies about detected enemy's position
				// sevenfm: check that he is really new
				//if( gbPublicOpplist[pSoldier->roster().team()][pOpponent->identity().id()] != SEEN_CURRENTLY && gbPublicOpplist[pSoldier->roster().team()][pOpponent->identity().id()] != SEEN_THIS_TURN )
				if (gbSeenOpponents[pSoldier->identity().id()][pOpponent->identity().id()] < 1 && gbPublicOpplist[pSoldier->roster().team()][pOpponent->identity().id()] < SEEN_CURRENTLY)
					PossiblyStartEnemyTaunt( pSoldier, TAUNT_INFORM_ABOUT, pOpponent->identity().id() );
			}
			else
			{
				// if we in a non-combat movement decision, presumably this is not
				// something we were quite expecting, so make a new decision.  For
				// other (combat) movement decisions, we took his position into account
				// when we made it, so don't make us think again & slow things down.
				switch (pSoldier->aiPlanning().action())
				{
				case AI_ACTION_RANDOM_PATROL:
				case AI_ACTION_SEEK_OPPONENT:
				case AI_ACTION_SEEK_FRIEND:
				case AI_ACTION_POINT_PATROL:
				case AI_ACTION_LEAVE_WATER_GAS:
				case AI_ACTION_SEEK_NOISE:
					SetNewSituation( pSoldier );  // force the looker to re-evaluate
					break;
				}
			}
		}

	}
#ifdef TESTOPPLIST
	else
		DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3, String( "ManSeesMan: ID %d(%S) to ID %d ALREADYSEENCURRENTLY",pSoldier->identity().id(),pSoldier->identity().name(),pOpponent->identity().id()) );
#endif
	//bOldOppValue = pSoldier->awareness().opponentKnowledge()[ pOpponent->identity().id() ];
	// remember that the soldier is currently seen and his new location
 UpdatePersonal(pSoldier,pOpponent->identity().id(),SEEN_CURRENTLY,sOppGridNo,bOppLevel);

	if ( ubCaller2 == MANLOOKSFOROTHERTEAMS || ubCaller2 == OTHERTEAMSLOOKFORMAN || ubCaller2 == CALLER_UNKNOWN ) // unknown->hearing
	{

		if ( gubBestToMakeSightingSize != BEST_SIGHTING_ARRAY_SIZE_INCOMBAT && gTacticalStatus.bBoxingState == NOT_BOXING )
		{
			if ( fNewOpponent )
			{
				if ( IsJa2TacticalCombatActive() )
				{
					// presumably a door opening... we do require standard interrupt conditions				
					if (StandardInterruptConditionsMet(pSoldier,pOpponent->identity().id(),bOldOppList))
					{
						ReevaluateBestSightingPosition( pSoldier, CalcInterruptDuelPts( pSoldier, pOpponent->identity().id(), TRUE ) );
					}
				}
				// require the enemy not to be dying if we are the sighter; in other words,
				// always add for AI guys, and always add for people with life >= OKLIFE
				else if ( !(pSoldier->roster().team() == gbPlayerNum && pOpponent->vitals().health() < OKLIFE ) )
				{
					ReevaluateBestSightingPosition( pSoldier, CalcInterruptDuelPts( pSoldier, pOpponent->identity().id(), TRUE ) );
				}
			}
		}
	}

	// if this man has never seen this opponent before in this sector
	if (gbSeenOpponents[pSoldier->identity().id()][pOpponent->identity().id()] == FALSE)
		// remember that he is just seeing him now for the first time (-1)
		gbSeenOpponents[pSoldier->identity().id()][pOpponent->identity().id()] = -1;
	else
		// man is seeing an opponent AGAIN whom he has seen at least once before
		gbSeenOpponents[pSoldier->identity().id()][pOpponent->identity().id()] = TRUE;

//ddd{
BOOLEAN SEE_MENT = FALSE;

if(gGameExternalOptions.bWeSeeWhatMilitiaSeesAndViceVersa)
{
	if ( ( PTR_OURTEAM || (pSoldier->roster().team() == MILITIA_TEAM) ) && (pOpponent->awareness().visibility() <= 0))
	SEE_MENT = TRUE;
}
else
{
	if (PTR_OURTEAM && (pOpponent->awareness().visibility() <= 0))
	SEE_MENT = TRUE;
}

#ifdef ENABLE_MP_FRIENDLY_PLAYERS_SHARE_SAME_FOV
	//haydent
	if((is_networked &&  pSoldier->roster().side() == 0 && pSoldier->roster().team() != OUR_TEAM) && (pOpponent->awareness().visibility() <= 0))
		SEE_MENT = TRUE;
#endif

//ddd}

/*comm by ddd
	// if looker is on local team, and the enemy was invisible or "maybe"
	// visible just prior to this
#ifdef WE_SEE_WHAT_MILITIA_SEES_AND_VICE_VERSA
	if ( ( PTR_OURTEAM || (pSoldier->roster().team() == MILITIA_TEAM) ) && (pOpponent->awareness().visibility() <= 0))
#else
	if (PTR_OURTEAM && (pOpponent->awareness().visibility() <= 0))
#endif
*/
if(SEE_MENT)
	{
		// if opponent was truly invisible, not just turned off temporarily (FALSE)
		if (pOpponent->awareness().visibility() == -1)
		{
			// then locate to him and set his locator flag
			bDoLocate = TRUE;

			//rain
			if( gfLightningInProgress ) gfHaveSeenSomeone = TRUE;
			//end rain
		}

		// make opponent visible (to us)
		// must do this BEFORE the locate since it checks for visibility
		pOpponent->awareness().markVisible();

		//ATE: Cancel any fading going on!
		// ATE: Added for fade in.....
		if ( pOpponent->renderState().fadeMode() == 1 || pOpponent->renderState().fadeMode() == 2 )
		{
			pOpponent->renderState().finishFade();

			if ( pOpponent->position().level() > 0 && gpWorldLevelData[ pOpponent->position().gridNo() ].pRoofHead != NULL )
			{
				pOpponent->renderState().fadeLevel() = gpWorldLevelData[ pOpponent->position().gridNo() ].pRoofHead->ubShadeLevel;
			}
			else
			{
				pOpponent->renderState().fadeLevel() = gpWorldLevelData[ pOpponent->position().gridNo() ].pLandHead->ubShadeLevel;
			}

			// Set levelnode shade level....
			if ( pOpponent->renderBindings().levelNode() )
			{
				pOpponent->renderBindings().levelNode()->ubShadeLevel = pOpponent->renderState().fadeLevel();
			}
		}


#ifdef TESTOPPLIST
		DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3, String("!!! ID %d (%S) MAKING %d VISIBLE",pSoldier->identity().id(),pSoldier->identity().name(),pOpponent->identity().id()) );
#endif

		// update variable for STATUS screen
		//pOpponent->bLastKnownLife = pOpponent->life;

		if (bDoLocate)
		{

			// Change his anim speed!
			SetSoldierAniSpeed( pOpponent );

			// if show enemies is ON, then we must have already revealed these roofs
			// and we're also following his movements, so don't bother sliding
			if (!gbShowEnemies)
			{
				//DoSoldierRoofs(pOpponent);

				// slide to the newly seen opponent, and if appropriate, start his locator
				//SlideToMe = oppPtr->guynum;
			}

			//LastOpponentLocatedTo = oppPtr->guynum;

			/*
			#ifdef RECORDNET
			fprintf(NetDebugFile,"\tManSeesMan - LOCATE\n");
			#endif
			*/


			if ( IsJa2TacticalTurnBased() && ( ( IsJa2TacticalCombatActive() ) | gTacticalStatus.fVirginSector ) )
			{
				if (!pOpponent->aiBehavior().neutral() && (pSoldier->roster().side() != pOpponent->roster().side()))
				{
					//SlideTo(0,pOpponent->identity().id(), pSoldier->identity().id(), SETLOCATOR);
					//rain
					SlideTo(pOpponent->identity().id(), SETLOCATORFAST);
					//end rain
				}
			}
		}


	}
	else if (!PTR_OURTEAM)
	{
		// ATE: Check stance, change to threatending
		ReevaluateEnemyStance( pSoldier, pSoldier->animationPlayback().state() );
	}
    AI::tactical::AIInputData ai_input(AI::tactical::AIInputData::Visual(), pOpponent, sOppGridNo, bOppLevel, ubCaller, ubCaller2);
    AI::tactical::PlanInputData plan_input((IsJa2TacticalTurnBased())!=0, gTacticalStatus);
    AI::tactical::PlanFactoryLibrary* plan_lib(AI::tactical::PlanFactoryLibrary::instance());
    plan_lib->update_plan(pSoldier->aiPlanning().planIndex(), pSoldier, ai_input);
}


void DecideTrueVisibility(TacticalActor *pSoldier, UINT8 ubLocate)
{
 // if his visibility is still in the special "limbo" state (FALSE)
 if (pSoldier->awareness().visibility() == FALSE)
 {
	// then none of our team's merc turned him visible,
	// therefore he now becomes truly invisible
	pSoldier->awareness().markHidden();

	// Don;t adjust anim speed here, it's done once fade is over!
	}


 // If soldier is not visible, make sure his red "locator" is turned off
 //if ((pSoldier->awareness().visibility() < 0) && !gbShowEnemies)
	//	pSoldier->bLocator = FALSE;


 if (ubLocate)
	{
	// if he remains visible (or ShowEnemies ON)
	if ((pSoldier->awareness().visibility() >= 0) || gbShowEnemies)
	{
		/*
#ifdef RECORDNET
	 fprintf(NetDebugFile,"\tDecideTrueVisibility - LOCATE\n");
#endif
	*/

	if (PTR_OURTEAM)
	 {
		//if (ConfigOptions[FOLLOWMODE] && Status.stopSlidingAt == NOBODY)
		//	LocateMember(ptr->guynum,DONTSETLOCATOR);
	 }
	else // not our team - if we're NOT allied then locate...
	 //if (pSoldier->side != gTacticalStatus.Team[gbPlayerNum].side && ConfigOptions[FOLLOWMODE])
		//if (Status.stopSlidingAt == NOBODY)
				if (IsJa2TacticalTurnBasedCombat() )
			//LocateSoldier(pSoldier->identity().id(),DONTSETLOCATOR);
					SlideTo(pSoldier->identity().id(), DONTSETLOCATOR);

	 // follow his movement on our screen as he moves around...
	 //LocateMember(ptr->guynum,DONTSETLOCATOR);
	}
	}
}





void OtherTeamsLookForMan(TacticalActor *pOpponent)
{
	UINT32 uiLoop;
	INT8 bOldOppList;
	TacticalActor *pSoldier;


	//NumMessage("OtherTeamsLookForMan, guy#",oppPtr->guynum);

/* comm by ddd	
	// if the guy we're looking for is NOT on our team AND is currently visible
#ifdef WE_SEE_WHAT_MILITIA_SEES_AND_VICE_VERSA
	if ((pOpponent->roster().team() != gbPlayerNum && pOpponent->roster().team() != MILITIA_TEAM) && (pOpponent->awareness().visibility() >= 0 && pOpponent->awareness().visibility() < 2) && pOpponent->vitals().health())
#else
	if ((pOpponent->roster().team() != gbPlayerNum) && (pOpponent->awareness().visibility() >= 0 && pOpponent->awareness().visibility() < 2) && pOpponent->vitals().health())
#endif
	{
		// assume he's no longer visible, until one of our mercs sees him again
		pOpponent->awareness().markIndeterminate();
	}
*/	


#ifdef ENABLE_MP_FRIENDLY_PLAYERS_SHARE_SAME_FOV
//haydent
if(is_networked &&  pOpponent->roster().side() == 0)
{
	//stay visible
}
else
#endif
{

	if(gGameExternalOptions.bWeSeeWhatMilitiaSeesAndViceVersa)
	{	
		if ((pOpponent->roster().team() != gbPlayerNum && pOpponent->roster().team() != MILITIA_TEAM) && (pOpponent->awareness().visibility() >= 0 && pOpponent->awareness().visibility() < 2) && pOpponent->vitals().health())
			pOpponent->awareness().markIndeterminate();
	}
	else
	{
	if ((pOpponent->roster().team() != gbPlayerNum) && (pOpponent->awareness().visibility() >= 0 && pOpponent->awareness().visibility() < 2) && pOpponent->vitals().health())
		pOpponent->awareness().markIndeterminate();
	}

}//haydent

#ifdef TESTOPPLIST
	DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
			String("OTHERTEAMSLOOKFORMAN ID %d(%S) team %d side %d",pOpponent->identity().id(),pOpponent->identity().name(),pOpponent->roster().team(),pOpponent->roster().side() ));
#endif


	// all soldiers not on oppPtr's team now look for him
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pSoldier = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is active, in this sector, and well enough to look
		if (pSoldier != NULL && pSoldier->vitals().health() >= OKLIFE	&& (pSoldier->identity().bodyType() != LARVAE_MONSTER))
		{
			// if this merc is on the same team as the target soldier
			if (pSoldier->roster().team() == pOpponent->roster().team())
			{
				continue;		// he doesn't look (he ALWAYS knows about him)
			}

			bOldOppList = pSoldier->awareness().opponentKnowledge()[pOpponent->identity().id()];

			// this merc looks for the soldier in question
			// use both sides actual x,y co-ordinates (neither side's moving)
			if (ManLooksForMan(pSoldier,pOpponent,OTHERTEAMSLOOKFORMAN))
			{
				// if a new opponent is seen (which must be oppPtr himself)
				//if (IsJa2TacticalTurnBasedCombat() && pSoldier->awareness().newOpponentCount())
				// Calc interrupt points in non-combat because we might get an interrupt or be interrupted
				// on our first turn

				// if doing regular in-combat sighting (not on opening doors!)
				if ( gubBestToMakeSightingSize == BEST_SIGHTING_ARRAY_SIZE_INCOMBAT )
				{
					if ( IsJa2TacticalTurnBasedCombat() && pSoldier->awareness().newOpponentCount() )
					{
						// as long as viewer meets minimum interrupt conditions
						if ( gubSightFlags & SIGHT_INTERRUPT && StandardInterruptConditionsMet(pSoldier,pOpponent->identity().id(),bOldOppList))
						{
							// calculate the interrupt duel points
							pSoldier->turnState().interruptDuelPoints() = CalcInterruptDuelPts(pSoldier, pOpponent->identity().id(), TRUE);
							DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Calculating int duel pts in OtherTeamsLookForMan, %d has %d points", pSoldier->identity().id(), pSoldier->turnState().interruptDuelPoints() ) );
						}
						else
						{
							pSoldier->turnState().interruptDuelPoints() = NO_INTERRUPT;
						}
					}
				}
			}

			// if "Show only enemies seen" option is ON and it's this guy looking
			//if (ptr->guynum == ShowOnlySeenPerson)
			//NewShowOnlySeenPerson(ptr);					// update the string
		}
	}


	// if he's not on our team
	if (pOpponent->roster().team() != gbPlayerNum)
	{
	// don't do a locate here, it's already done by Man Sees Man for new opps.
	DecideTrueVisibility(pOpponent,NOLOCATE);
	}
}

void AddOneOpponent(TacticalActor *pSoldier)
{
	INT8 bOldOppCnt = pSoldier->awareness().opponentCount();

	pSoldier->awareness().opponentCount()++;

	if (!bOldOppCnt)
	{
		// if we hadn't known about opponents being here for sure prior to this
		if (pSoldier->identity().bodyType() == LARVAE_MONSTER)
		{
			// never become aware of you!
			return;
		}

		if (pSoldier->aiBehavior().alertStatus() < STATUS_RED)
		{
			CheckForChangingOrders(pSoldier);
		}

		pSoldier->aiBehavior().alertStatus() = STATUS_BLACK;	// force black AI status right away

		if (pSoldier->status().flags() & SOLDIER_MONSTER)
		{
			pSoldier->aiCommunication().caller() = NOBODY;
			pSoldier->aiCommunication().callPriority() = 0;
		}
	}

	if (pSoldier->roster().team() == gbPlayerNum)
	{
		// adding an opponent for player; reset # of turns that we haven't seen an enemy
		gTacticalStatus.bConsNumTurnsNotSeen = 0;
	}

}



void RemoveOneOpponent(TacticalActor *pSoldier)
{
 pSoldier->awareness().opponentCount()--;

 if ( pSoldier->awareness().opponentCount() < 0 )
 {
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("Oppcnt for %d (%s) tried to go below 0", pSoldier->identity().id(), pSoldier->identity().name() ) );
	#ifdef JA2BETAVERSION
		ScreenMsg( MSG_FONT_YELLOW, MSG_UI_FEEDBACK,	L"Opponent counter dropped below 0 for person %d (%s). Please inform Sir-tech of this, and what has just been happening in the game.", pSoldier->identity().id(), pSoldier->identity().name() );
	#endif
	pSoldier->awareness().opponentCount() = 0;
 }

 // if no opponents remain in sight, drop status to RED (but NOT newSit.!)
 if (!pSoldier->awareness().opponentCount())
	pSoldier->aiBehavior().alertStatus() = STATUS_RED;
}




void RemoveManAsTarget(TacticalActor *pSoldier)
{
	TacticalActor *pOpponent;
	UINT8 ubLoop;
	SoldierID ubTarget = pSoldier->identity().id();

	// clean up the public opponent lists and locations
	for (ubLoop = 0; ubLoop < MAXTEAMS; ubLoop++)
	{ 	// never causes any additional looks
		UpdatePublic(ubLoop, ubTarget, NOT_HEARD_OR_SEEN, NOWHERE, 0);
	}

	/*
	IAN COMMENTED THIS OUT MAY 1997 - DO WE NEED THIS?

	// make sure this guy is no longer a possible target for anyone
	for each soldier
	{
		if (pOpponent->bOppNum == ubTarget)
			pOpponent->bOppNum = NOBODY;
	}
	*/


	// clean up all opponent's opplists
	for (ubLoop = 0; ubLoop < Ja2ActiveTacticalActorSlotCount(); ubLoop++)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(ubLoop);

	// if the target is active, a true opponent and currently seen by this merc
		if (pOpponent)
		{
			// check to see if OPPONENT considers US neutral
			if ( (pOpponent->awareness().opponentKnowledge()[ubTarget] == SEEN_CURRENTLY) && !pOpponent->aiBehavior().neutral() && (pSoldier->roster().side() != pOpponent->roster().side()) )
			{
				// Flugente: we consider enemies to be neutral if they are prisoners of war (otherwise the AI would kill prisoners). Bu as we want to remove them, we have to account for that
				// we also move RecognizeAsCombatant to be the last condition checked, because it is the most computationally expensive one
				if ( ( !CONSIDERED_NEUTRAL( pOpponent, pSoldier ) || pSoldier->featureFlags().primaryFlags() & SOLDIER_POW ) && TacticalActorCovertOps::recognizesCombatant(*pOpponent, pSoldier->identity().id()) )
					RemoveOneOpponent(pOpponent);
			}
			UpdatePersonal(pOpponent, ubTarget, NOT_HEARD_OR_SEEN, NOWHERE, 0);
			gbSeenOpponents[ubLoop][ubTarget] = FALSE;
		}
	}

	/*
	for each soldier
		{
		// if the target is a true opponent and currently seen by this merc
		if (!pSoldier->aiBehavior().neutral() && !pSoldier->aiBehavior().neutral() &&
			(pOpponent->awareness().opponentKnowledge()[ubTarget] == SEEN_CURRENTLY)

				)
				//*** UNTIL ANDREW GETS THE SIDE PARAMETERS WORKING
			// && (pSoldier->side != pOpponent->side))
		{
		 RemoveOneOpponent(pOpponent);
		}

		UpdatePersonal(pOpponent,ubTarget,NOT_HEARD_OR_SEEN,NOWHERE,0);

		gbSeenOpponents[ubLoop][ubTarget] = FALSE;
		}
	*/

	ResetLastKnownLocs(pSoldier);

	if (gTacticalStatus.Team[pSoldier->roster().team()].ubLastMercToRadio == ubTarget)
		gTacticalStatus.Team[pSoldier->roster().team()].ubLastMercToRadio = NOBODY;
}



void UpdatePublic(UINT8 ubTeam, SoldierID ubID, INT8 bNewOpplist, INT32 sGridNo, INT8 bLevel)
{
	SoldierID cnt;
	UINT8 ubTeamMustLookAgain = FALSE, ubMadeDifference = FALSE;
	TacticalActor *pSoldier;
	TacticalActor* opponent =
		GetJa2SoldierRepository().resolve( ubID );
	if ( opponent == nullptr )
	{
		return;
	}

	INT8* pbPublOL = &(gbPublicOpplist[ubTeam][ubID]);

	// if new opplist is more up-to-date, or we are just wiping it for some reason
	if ((gubKnowledgeValue[*pbPublOL - OLDEST_HEARD_VALUE][bNewOpplist - OLDEST_HEARD_VALUE] > 0) ||
		(bNewOpplist == NOT_HEARD_OR_SEEN))
	{
		// if this team is becoming aware of a soldier it wasn't previously aware of
		if ((bNewOpplist != NOT_HEARD_OR_SEEN) && (*pbPublOL == NOT_HEARD_OR_SEEN))
			ubTeamMustLookAgain = TRUE;

		// change the public opplist *BEFORE* anyone looks again or we'll recurse!
		*pbPublOL = bNewOpplist;
	}


	// always update the gridno, no matter what
	gsPublicLastKnownOppLoc[ubTeam][ubID] = sGridNo;
	gbPublicLastKnownOppLevel[ubTeam][ubID] = bLevel;

	// if team has been told about a guy the team was completely unaware of
	if (ubTeamMustLookAgain)
	{
		// then everyone on team who's not aware of guynum must look for him
		cnt = gTacticalStatus.Team[ubTeam].bFirstID;

		for ( ; cnt <= gTacticalStatus.Team[ubTeam].bLastID; ++cnt )
		{
			pSoldier = GetJa2SoldierRepository().resolve( cnt );
			if ( pSoldier == nullptr )
			{
				continue;
			}

			// if this soldier is active, in this sector, and well enough to look
			if (pSoldier->roster().active() && pSoldier->roster().inSector() && (pSoldier->vitals().health() >= OKLIFE) && !( pSoldier->status().flags() & SOLDIER_GASSED ) )
			{
				// if soldier isn't aware of guynum, give him another chance to see
				if (pSoldier->awareness().opponentKnowledge()[ubID] == NOT_HEARD_OR_SEEN)
				{
					if ( ManLooksForMan( pSoldier, opponent, UPDATEPUBLIC ) )
						// then he actually saw guynum because of our new public knowledge
						ubMadeDifference = TRUE;

					// whether successful or not, whack newOppCnt.	Since this is a
					// delayed reaction to a radio call, there's no chance of interrupt!
					pSoldier->awareness().clearNewOpponents();

					// if "Show only enemies seen" option is ON and it's this guy looking
					//if (pSoldier->identity().id() == ShowOnlySeenPerson)
					// NewShowOnlySeenPerson(pSoldier);					// update the string
				}
			}
		}
	}
}



void UpdatePersonal(TacticalActor *pSoldier, SoldierID ubID, INT8 bNewOpplist, INT32 sGridNo, INT8 bLevel)
{
	/*
#ifdef RECORDOPPLIST
	fprintf(OpplistFile,"UpdatePersonal - for %d about %d to %d (was %d) at g%d\n",	ptr->guynum,guynum,newOpplist,ptr->opplist[guynum],gridno);
#endif
	*/
	
	// if new opplist is more up-to-date, or we are just wiping it for some reason
	if ((gubKnowledgeValue[pSoldier->awareness().opponentKnowledge()[ubID] - OLDEST_HEARD_VALUE][bNewOpplist - OLDEST_HEARD_VALUE] > 0) || (bNewOpplist == NOT_HEARD_OR_SEEN))
	{
		pSoldier->awareness().opponentKnowledge()[ubID] = bNewOpplist;
	}

	// always update the gridno, no matter what
	gsLastKnownOppLoc[pSoldier->identity().id()][ubID] = sGridNo;
	gbLastKnownOppLevel[pSoldier->identity().id()][ubID] = bLevel;
}



INT8 OurMaxPublicOpplist()
{
	UINT32 uiLoop;
	INT8 bHighestOpplist = 0;
	UINT8 ubOppValue,ubHighestValue = 0;
	TacticalActor * pSoldier;

	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pSoldier = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, or dead
		if (!pSoldier || !pSoldier->vitals().health())
			continue;		// next merc

		// if this man is NEUTRAL / on our side, he's not an opponent
		if (pSoldier->aiBehavior().neutral() ||
			(gTacticalStatus.Team[gbPlayerNum].bSide == pSoldier->roster().side()))
			continue;		// next merc

		// opponent, check our public opplist value for him
		ubOppValue = gubKnowledgeValue[0 - OLDEST_HEARD_VALUE][gbPublicOpplist[gbPlayerNum][pSoldier->identity().id()] - OLDEST_HEARD_VALUE];

		if (ubOppValue > ubHighestValue)
		{
			ubHighestValue = ubOppValue;
			bHighestOpplist = gbPublicOpplist[gbPlayerNum][pSoldier->identity().id()];
		}
	}

	return(bHighestOpplist);
}



/*
BOOLEAN VisibleAnywhere(TacticalActor *pSoldier)
{
 INT8 team,cnt;
 TacticalActor *pOpponent;


 // this takes care of any mercs on our own team
 if (pSoldier->awareness().visibility() >= 0)
	return(TRUE);

 // if playing alone, "anywhere" is just over here!
 //if (!Net.multiType || Net.activePlayers < 2)
	//return(FALSE);


 for (bTeam = 0; bTeam < MAXTEAMS; bTeam++)
	{
	// skip our team (local visible flag will do for them)
	if (bTeam == gbPlayerNum)
	 continue;

	// skip any inactive teams
	if (!gTacticalStatus.team[bTeam].teamActive)
	 continue;

	// skip non-human teams (they don't communicate for their machines!)
	if (!gTacticalStatus.Team[bTeam].human)
	 continue;

	// so we're left with another human player's team of mercs...

	// check if soldier is currently visible to any human mercs on other teams
	for each soldier on the team
	{
	 // if this merc is inactive, or in no condition to care
	 if (!oppPtr->active || !oppPtr->in_sector || oppPtr->deadAndRemoved || (oppPtr->life < OKLIFE))
		continue;			// skip him!

	 if (oppPtr->opplist[ptr->guynum] == SEEN_CURRENTLY)
		return(TRUE);
	}
	}


 // nobody anywhere sees him
 return(FALSE);
}

*/


void ResetLastKnownLocs(TacticalActor *pSoldier)
{
	UINT32 uiLoop;

	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		TacticalActor* opponent =
			ResolveJa2ActiveTacticalActorSlot(uiLoop);
		if (opponent)
		{
			gsLastKnownOppLoc[pSoldier->identity().id()]
				[opponent->identity().id()] = NOWHERE;

			// IAN added this June 14/97
			gsPublicLastKnownOppLoc[pSoldier->roster().team()]
				[opponent->identity().id()] = NOWHERE;
		}
	}
}



/*
// INITIALIZATION STUFF
-------------------------
// Upon loading a scenario, call these:
InitOpponentKnowledgeSystem();

// loop through all soldiers and for each soldier call
InitSoldierOpplist(pSoldier);

// call this once
AllTeamsLookForAll(NO_INTERRUPTS);	// no interrupts permitted this early


// for each additional soldier created, call
InitSoldierOpplist(pSoldier);
HandleSight(pSoldier,SIGHT_LOOK);



MOVEMENT STUFF
-----------------
// whenever new tile is reached, call
HandleSight(pSoldier,SIGHT_LOOK);

*/

void InitOpponentKnowledgeSystem(void)
{
	INT32	iTeam, cnt, cnt2;

	memset(gbSeenOpponents,0,sizeof(gbSeenOpponents));
	memset(gbPublicOpplist,NOT_HEARD_OR_SEEN,sizeof(gbPublicOpplist));

	for (iTeam=0; iTeam < MAXTEAMS; iTeam++)
	{
		gubPublicNoiseVolume[iTeam] = 0;
		gsPublicNoiseGridNo[iTeam] = NOWHERE;
		gbPublicNoiseLevel[iTeam] = 0;
		for (cnt = 0; cnt < MAX_NUM_SOLDIERS; cnt++)
		{
			gsPublicLastKnownOppLoc[ iTeam ][ cnt ] = NOWHERE;
		}
	}

	// initialize public last known locations for all teams
	for (cnt = 0; cnt < MAX_NUM_SOLDIERS; cnt++)
	{
		for (cnt2 = 0; cnt2 < NUM_WATCHED_LOCS; cnt2++ )
		{
			gsWatchedLoc[ cnt ][ cnt2 ] = NOWHERE;
			gubWatchedLocPoints[ cnt ][ cnt2 ] = 0;
			gfWatchedLocReset[ cnt ][ cnt2 ] = FALSE;
		}
	}

	for ( cnt = 0; cnt < SHOULD_BECOME_HOSTILE_SIZE; cnt++ )
	{
		gubShouldBecomeHostileOrSayQuote[ cnt ] = NOBODY;
	}

	gubNumShouldBecomeHostileOrSayQuote = 0;
}



void InitSoldierOppList(TacticalActor *pSoldier)
{
	memset(pSoldier->awareness().opponentKnowledge(),NOT_HEARD_OR_SEEN,sizeof(pSoldier->awareness().opponentKnowledge()));
	pSoldier->awareness().opponentCount() = 0;
	ResetLastKnownLocs(pSoldier);
	memset(gbSeenOpponents[pSoldier->identity().id()], 0, TOTAL_SOLDIERS);
}


void BetweenTurnsVisibilityAdjustments(void)
{
	UINT32 cnt;
	TacticalActor *pSoldier;


	// make all soldiers on other teams that are no longer seen not visible
	// iterate only the active merc slots (skips the inactive backing records)
	for (cnt = 0; cnt < Ja2ActiveTacticalActorSlotCount(); cnt++)
	{
		pSoldier = ResolveJa2ActiveTacticalActorSlot(cnt);

		if (pSoldier && pSoldier->roster().active() && pSoldier->roster().inSector() && pSoldier->vitals().health())
		{
			BOOLEAN SEE_MENT = FALSE;

#ifdef ENABLE_MP_FRIENDLY_PLAYERS_SHARE_SAME_FOV
		if(is_networked &&  pSoldier->roster().side() == 0)//haydent
		{
			//stay visible
		}
		else
#endif
		{
			
			if (gGameExternalOptions.bWeSeeWhatMilitiaSeesAndViceVersa)
			{if (!PTR_OURTEAM && pSoldier->roster().team() != MILITIA_TEAM)
			SEE_MENT = TRUE;
			}
			else
			{ if (!PTR_OURTEAM) SEE_MENT = TRUE;
			}

		}//haydent
			
/*comm by ddd
#ifdef WE_SEE_WHAT_MILITIA_SEES_AND_VICE_VERSA
			if (!PTR_OURTEAM && pSoldier->roster().team() != MILITIA_TEAM)
#else
			if (!PTR_OURTEAM)
#endif
				*/

			if(SEE_MENT)
			{
				// check if anyone on our team currently sees him (exclude NOBODY)
				if (TeamNoLongerSeesMan(gbPlayerNum,pSoldier,NOBODY,0))
				{
					// then our team has lost sight of him
					pSoldier->awareness().markHidden();		// make him fully invisible

					// Allow fade to adjust anim speed
				}
			}
		}
	}
}


void SaySeenQuote( TacticalActor *pSoldier, BOOLEAN fSeenCreature, BOOLEAN fVirginSector, BOOLEAN fSeenJoey )
{
	TacticalActor		*pTeamSoldier;
	UINT16				ubNumEnemies = 0;
	UINT16				ubNumAllies = 0;
	UINT32			cnt;
	if ( !GetGameContext().capabilities().isUnfinishedBusiness() &&
	     AreInMeanwhile( ) )
	{
		return;
	}
	// Check out for our under large fire quote
	if ( !pSoldier->dialogue().hasSaid(SOLDIER_QUOTE_SAID_IN_SHIT) )
	{
		// Get total enemies.
		// Loop through all mercs in sector and count # of enemies
		for ( cnt = 0; cnt < Ja2ActiveTacticalActorSlotCount(); ++cnt )
		{
			pTeamSoldier = ResolveJa2ActiveTacticalActorSlot(cnt);

			if ( pTeamSoldier != NULL )
			{
				if ( OK_ENEMY_MERC( pTeamSoldier ) )
				{
					++ubNumEnemies;
				}
			}
		}

		// OK, after this, check our guys
		for ( cnt = 0; cnt < Ja2ActiveTacticalActorSlotCount(); ++cnt )
		{
			pTeamSoldier = ResolveJa2ActiveTacticalActorSlot(cnt);

			if ( pTeamSoldier != NULL )
			{
				if ( !OK_ENEMY_MERC( pTeamSoldier ) )
				{
					if ( pTeamSoldier->awareness().opponentCount() >= ( ubNumEnemies / 2 ) )
					{
						++ubNumAllies;
					}
				}
			}
		}

		// now check!
		if ( ( pSoldier->awareness().opponentCount() - ubNumAllies ) > 2 )
		{
			// Say quote!
			TacticalCharacterDialogue( pSoldier, QUOTE_IN_TROUBLE_SLASH_IN_BATTLE );
			//pSoldier->ubLastEnemyAttackingProvokingQuote = 
			pSoldier->dialogue().markSaid(SOLDIER_QUOTE_SAID_IN_SHIT);

			return;
		}
	}

	if ( fSeenCreature == 1 )
	{
		// Is this our first time seeing them?
		if ( gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags & PROFILE_MISC_FLAG_HAVESEENCREATURE )
		{
			// Are there multiplaes and we have not said this quote during this battle?
			if ( !pSoldier->dialogue().hasSaid(SOLDIER_QUOTE_SAID_MULTIPLE_CREATURES) )
			{
				// Check for multiples!
				ubNumEnemies = 0;

				// Get total enemies.
				// Loop through all mercs in sector and count # of enemies
				for ( cnt = 0; cnt < Ja2ActiveTacticalActorSlotCount(); cnt++ )
				{
					pTeamSoldier = ResolveJa2ActiveTacticalActorSlot(cnt);

					if ( pTeamSoldier != NULL )
					{
						if ( OK_ENEMY_MERC( pTeamSoldier ) )
						{
							if ( pTeamSoldier->status().flags() & SOLDIER_MONSTER && pSoldier->awareness().opponentKnowledge()[ pTeamSoldier->identity().id() ] == SEEN_CURRENTLY )
							{
								ubNumEnemies++;
							}
						}
					}
				}

				if ( ubNumEnemies > 2 )
				{
					// Yes, set flag
					pSoldier->dialogue().markSaid(SOLDIER_QUOTE_SAID_MULTIPLE_CREATURES);

					// Say quote
					TacticalCharacterDialogue( pSoldier, QUOTE_ATTACKED_BY_MULTIPLE_CREATURES );
				}
				else
				{
					TacticalCharacterDialogue( pSoldier, QUOTE_SEE_CREATURE );
				}
			}
			else
			{
				TacticalCharacterDialogue( pSoldier, QUOTE_SEE_CREATURE );
			}
		}
		else
		{
			// Yes, set flag
			gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags |= PROFILE_MISC_FLAG_HAVESEENCREATURE;

			TacticalCharacterDialogue( pSoldier, QUOTE_FIRSTTIME_GAME_SEE_CREATURE );
		}
	}
	// 2 is for bloodcat...
	else if ( fSeenCreature == 2 )
	{
		TacticalCharacterDialogue( pSoldier, QUOTE_SPOTTED_BLOODCAT );
	}
	else
	{
		if ( fVirginSector )
		{
			// First time we've seen a guy this sector
			TacticalCharacterDialogue( pSoldier, QUOTE_SEE_ENEMY_VARIATION );
		}
		else
		{
			// Flugente: no quotes on seeing enemy when covert
			if ( (pSoldier->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV|SOLDIER_COVERT_SOLDIER) ) == 0 )
			{
				// Flugente: apparently the goal was to have mercs only announce enemies shorter occasionally
				if ( Chance( gGameExternalOptions.iChanceSayAnnoyingPhrase ) )
				{
					TacticalCharacterDialogue( pSoldier, QUOTE_SEE_ENEMY );
				}
				else
				{
					pSoldier->DoMercBattleSound( BATTLE_SOUND_ENEMY );
				}
			}
		}
	}
}

void OurTeamSeesSomeone( TacticalActor * pSoldier, INT8 bNumReRevealed, INT8 bNumNewEnemies )
{
	if ( gTacticalStatus.fVirginSector )
	{
		// If we are in NPC dialogue now... stop!
		DeleteTalkingMenu( );

		// Say quote!
		SaySeenQuote( pSoldier, gfPlayerTeamSawCreatures, TRUE, gfPlayerTeamSawJoey );

		pSoldier->HaultSoldierFromSighting( TRUE );

		// Set virgin sector to false....
		gTacticalStatus.fVirginSector = FALSE;
	}
	else
	{
		// if this merc is selected and he's actually moving
		//if ((pSoldier->identity().id() == gusSelectedSoldier) && !pSoldier->position().level())
		// ATE: Change this to if the guy is ours....
		// How will this feel?
		if ( pSoldier->roster().team() == gbPlayerNum )
		{
			// Flugente: disguised mercs do not alert us if they see an enemy, as otherwise one has to continously give them new orders
			if ( !(pSoldier->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV|SOLDIER_COVERT_SOLDIER)) )
			{
				// STOP IF WE WERE MOVING....
				/// Speek up!
				if ( bNumReRevealed > 0 && bNumNewEnemies == 0 )
				{
					pSoldier->DoMercBattleSound( BATTLE_SOUND_CURSE1 );
				}
				else
				{
					SaySeenQuote( pSoldier, gfPlayerTeamSawCreatures, FALSE, gfPlayerTeamSawJoey );
				}

				pSoldier->HaultSoldierFromSighting( TRUE );

				if ( gTacticalStatus.fEnemySightingOnTheirTurn )
				{
					// Locate to our guy, then slide to enemy
					LocateSoldier( pSoldier->identity().id(), SETLOCATOR );

					// Now slide to other guy....
					SlideTo(gTacticalStatus.ubEnemySightingOnTheirTurnEnemyID, SETLOCATOR);

				}

				// Unset User's turn UI
				UnSetUIBusy( pSoldier->identity().id() );
			}
		}
	}

	// OK, check what music mode we are in, change to battle if we're in battle
	// If we are in combat....
	if ( ( IsJa2TacticalCombatActive() ) )
	{
		// If we are NOT in any music mode...
		if ( GetMusicMode() == MUSIC_NONE )
		{
			#ifdef NEWMUSIC
			GlobalSoundID  = MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalBattle[gbWorldSectorZ];
			if ( MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalBattle[gbWorldSectorZ] != -1 )
				SetMusicModeID( MUSIC_TACTICAL_BATTLE, MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalBattle[gbWorldSectorZ] );
			else
			#endif
			SetMusicMode( MUSIC_TACTICAL_BATTLE );
		}
	}


}

void RadioSightings(TacticalActor *pSoldier, UINT16 ubAbout, UINT8 ubTeamToRadioTo )
{
	TacticalActor *pOpponent;
	INT32 	iLoop;
	UINT16 	start, end, revealedEnemies = 0, unknownEnemies = 0, stillUnseen = TRUE;
	BOOLEAN sightedHatedOpponent = FALSE;
	//UINT8 	oppIsCivilian;
	INT8 	*pPersOL,*pbPublOL; //,dayQuote;
	BOOLEAN	fContactSeen;
	BOOLEAN fSawCreatureForFirstTime = FALSE;

#ifdef TESTOPPLIST
	DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
			String("RADIO SIGHTINGS: for %d about %d",pSoldier->identity().id(),ubAbout) );
#endif

#ifdef RECORDNET
	if (!ptr->human)
		fprintf(NetDebugFile,"\tNPC %d(%s) radios his sightings to his team\n",ptr->guynum,ExtMen[ptr->guynum].name);
#endif

	gTacticalStatus.Team[pSoldier->roster().team()].ubLastMercToRadio = pSoldier->identity().id();


	// who are we radioing about?
	if (ubAbout == EVERYBODY)
	{
		start	= 0;
		end		= MAXMERCS;
	}
	else
	{
		start	= ubAbout;
		end 		= ubAbout + 1;
	}


	 // hang a pointer to the start of our this guy's personal opplist
	 pPersOL = &(pSoldier->awareness().opponentKnowledge()[start]);

	 // hang a pointer to the start of this guy's opponents in the public opplist
	 pbPublOL = &(gbPublicOpplist[ubTeamToRadioTo][start]);

	// loop through every one of this guy's opponents
	for (iLoop = start; iLoop < end; ++iLoop,pPersOL++,pbPublOL++)
	{
		pOpponent = GetJa2SoldierRepository().resolve(
			static_cast<std::size_t>( iLoop ) );
		if ( pOpponent == nullptr )
		{
			continue;
		}

		fContactSeen = FALSE;

#ifdef TESTOPPLIST
		DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
			String("RS: checking %d",pOpponent->identity().id()) );
#endif


		// make sure this merc is active, here & still alive (unconscious OK)
		if (!pOpponent->roster().active() || !pOpponent->roster().inSector() || !pOpponent->vitals().health())
		{
#ifdef TESTOPPLIST
			DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
				String("RS: inactive/notInSector/life %d",pOpponent->identity().id()) );
#endif
			continue;							// skip to the next merc
		}

		// if these two mercs are on the same SIDE, then they're NOT opponents
		// NEW: Apr. 21 '96: must allow ALL non-humans to get radioed about
		if ((pSoldier->roster().side() == pOpponent->roster().side()) && (pOpponent->status().flags() & SOLDIER_PC))
		{
#ifdef TESTOPPLIST
			DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
				String("RS: same side %d",pSoldier->roster().side()) );
#endif
		 continue;							// skip to the next merc
		}

		// determine whether we think we're still unseen or if "our cover's blown"
		// if we know about this opponent's location for any reason
		if ((pOpponent->awareness().visibility() >= 0) || gbShowEnemies)
		{
			// and he can see us, then gotta figure we KNOW that he can see us
			if (pOpponent->awareness().opponentKnowledge()[pSoldier->identity().id()] == SEEN_CURRENTLY)
				stillUnseen = FALSE;
		}


		// if we personally don't know a thing about this opponent
		if (*pPersOL == NOT_HEARD_OR_SEEN)
		{
#ifdef RECORDOPPLIST
			//fprintf(OpplistFile,"not heard or seen\n");
#endif
#ifdef TESTOPPLIST
			DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
				String("RS: not heard or seen") );
#endif
			continue;							// skip to the next opponent
		}

		// if personal knowledge is NOT more up to date and NOT the same as public
		if ((!gubKnowledgeValue[*pbPublOL - OLDEST_HEARD_VALUE][*pPersOL - OLDEST_HEARD_VALUE]) &&
			(*pbPublOL != *pPersOL))
		{
#ifdef RECORDOPPLIST
			//fprintf(OpplistFile,"no new knowledge (per %d, pub %d)\n",*pPersOL,*pbPublOL);
#endif
#ifdef TESTOPPLIST
			DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
				String("RS: no new knowledge per %d pub %d",*pPersOL,*pbPublOL) );
#endif
			continue;							// skip to the next opponent
		}

#ifdef RECORDOPPLIST
		//fprintf(OpplistFile,"made it!\n");
#endif
#ifdef TESTOPPLIST
		DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
			String("RS: made it!") );
#endif



		// if it's our merc, and he currently sees this opponent
		if (PTR_OURTEAM && (*pPersOL == SEEN_CURRENTLY) && !(( pOpponent->roster().side() == pSoldier->roster().side()) || pOpponent->aiBehavior().neutral()))
		{
			// don't care whether and how many new enemies are seen if everyone visible
			// and he's healthy enough to be a threat (so is worth talking about)

			// do the following if we're radioing to our own team; if radioing to militia
			// then alert them instead
			if ( ubTeamToRadioTo != MILITIA_TEAM )
			{
				if (!gbShowEnemies && (pOpponent->vitals().health() >= OKLIFE))
				{
					// if this enemy has not been publicly seen or heard recently
					if (*pbPublOL == NOT_HEARD_OR_SEEN)
					{
						// chalk up another "unknown" enemy
						unknownEnemies++;

						fContactSeen = TRUE;
						// if this enemy is hated by the merc doing the sighting
						//if (MercHated(Proptr[ptr->characternum].p_bias,oppPtr->characternum))
							//sightedHatedOpponent = TRUE;

						// now the important part: does this enemy see him/her back?
						if (pOpponent->awareness().opponentKnowledge()[pSoldier->identity().id()] != SEEN_CURRENTLY)
	 					{
							// EXPERIENCE GAIN (10): Discovered a new enemy without being seen
							StatChange(pSoldier,EXPERAMT,10,FALSE);
	 					}
					}
					else
					{
						// if he has publicly not been seen now, or anytime during this turn
						if ((*pbPublOL != SEEN_CURRENTLY) && (*pbPublOL != SEEN_THIS_TURN))
						{
							// chalk up another "revealed" enemy
							++revealedEnemies;
							fContactSeen = TRUE;
						}
					}

					if ( fContactSeen )
					{
						if ( pSoldier->roster().team() == gbPlayerNum )
						{
							if ( GetJa2TacticalCurrentTeam() != gbPlayerNum )
							{
								// Save some stuff!
								if (gTacticalStatus.fEnemySightingOnTheirTurn)
								{
									// this has already come up so turn OFF the pause-all-anims flag for the previous
									// person and set it for this next person
									TacticalActor* previousEnemy =
										GetJa2SoldierRepository().resolve(
											gTacticalStatus.ubEnemySightingOnTheirTurnEnemyID );
									if ( previousEnemy != nullptr )
									{
										previousEnemy->animationActivity().resume();
									}
								}
								else
								{
									gTacticalStatus.fEnemySightingOnTheirTurn = TRUE;
								}
								gTacticalStatus.ubEnemySightingOnTheirTurnEnemyID = pOpponent->identity().id();
								gTacticalStatus.ubEnemySightingOnTheirTurnPlayerID = pSoldier->identity().id();
								gTacticalStatus.uiTimeSinceDemoOn = GetJA2Clock( );

								pOpponent->animationActivity().pause();
							}
						}

						if ( pOpponent->status().flags() & SOLDIER_MONSTER )
						{
							gfPlayerTeamSawCreatures = TRUE;
						}

						// ATE: Added for bloodcat...
						if ( pOpponent->identity().bodyType() == BLOODCAT )
						{
							// 2 is for bloodcat
							gfPlayerTeamSawCreatures = 2;
						}
					}

					if ( pOpponent->status().flags() & SOLDIER_MONSTER )
					{
						if ( !(gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags & PROFILE_MISC_FLAG_HAVESEENCREATURE) )
						{
							fSawCreatureForFirstTime = TRUE;
						}
					}
				}
			}
			else
			{
				// radioing to militia that we saw someone! alert them!
				if ( gTacticalStatus.Team[ MILITIA_TEAM ].bTeamActive && !gTacticalStatus.Team[ MILITIA_TEAM ].bAwareOfOpposition )
				{
					HandleInitialRedAlert( MILITIA_TEAM, FALSE );
				}
			}
		} 	// end of our team's merc sees new opponent

		// IF WE'RE HERE, OUR PERSONAL INFORMATION IS AT LEAST AS UP-TO-DATE
		// AS THE PUBLIC KNOWLEDGE, SO WE WILL REPLACE THE PUBLIC KNOWLEDGE
#ifdef RECORDOPPLIST
		fprintf(OpplistFile,"UpdatePublic (RadioSightings) for team %d about %d\n",ptr->team,oppPtr->guynum);
#endif
#ifdef TESTOPPLIST
		DebugMsg( TOPIC_JA2OPPLIST, DBG_LEVEL_3,
			String("...............UPDATE PUBLIC: soldier %d SEEING soldier %d",pSoldier->identity().id(),pOpponent->identity().id()) );
#endif
		UpdatePublic(ubTeamToRadioTo,pOpponent->identity().id(),*pPersOL,gsLastKnownOppLoc[pSoldier->identity().id()][pOpponent->identity().id()],gbLastKnownOppLevel[pSoldier->identity().id()][pOpponent->identity().id()]);
	}


	// if soldier heard a misc noise more important that his team's public one
	if (pSoldier->perception().noiseVolume() > gubPublicNoiseVolume[ubTeamToRadioTo])
	{
		// replace the soldier's team's public noise with his
		gsPublicNoiseGridNo[ubTeamToRadioTo] 	= pSoldier->perception().noiseGrid();
		gbPublicNoiseLevel[ubTeamToRadioTo] 	= pSoldier->perception().heardNoiseLevel();
		gubPublicNoiseVolume[ubTeamToRadioTo] 	= pSoldier->perception().noiseVolume();
	}


	// if this soldier is on the local team
	if (PTR_OURTEAM)
	{
		// don't trigger sighting quotes or stop merc's movement if everyone visible
		//if (!(gTacticalStatus.uiFlags & SHOW_ALL_MERCS))
		{
			// if we've revealed any enemies, or seen any previously unknown enemies
			if (revealedEnemies || unknownEnemies)
			{
				// First check for a virgin map and set to false if we see our first guy....
				// Only if this guy is an ememy!
				OurTeamSeesSomeone( pSoldier, revealedEnemies, unknownEnemies );
			}
			else if (fSawCreatureForFirstTime)
			{
				gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags |= PROFILE_MISC_FLAG_HAVESEENCREATURE;
				TacticalCharacterDialogue( pSoldier, QUOTE_FIRSTTIME_GAME_SEE_CREATURE );
			}
		}
	}
}



#define COLOR1 FONT_MCOLOR_BLACK<<8 | FONT_MCOLOR_LTGREEN
#define COLOR2 FONT_MCOLOR_BLACK<<8 | FONT_MCOLOR_LTGRAY2

#define LINE_HEIGHT 15


extern UINT32 guiNumBackSaves;

void DebugSoldierPage1( )
{
	TacticalActor	*pSoldier;
	SoldierID	usSoldierIndex;
	UINT32		uiMercFlags;
	INT32		usMapPos;
	UINT8		ubLine=0;

	if ( FindSoldierFromMouse( &usSoldierIndex, &uiMercFlags ) )
	{
		// Get Soldier
		GetSoldier( &pSoldier, usSoldierIndex );

		SetFont( LARGEFONT1 );
		gprintf( 0,0,L"DEBUG SOLDIER PAGE ONE, GRIDNO %d", pSoldier->position().gridNo() );
		SetFont( LARGEFONT1 );

		ubLine = 2;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"ID:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->identity().id() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"TEAM:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->roster().team() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SIDE:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->roster().side() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"STATUS FLAGS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%x", pSoldier->status().flags() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"HUMAN:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", gTacticalStatus.Team[pSoldier->roster().team()].bHuman);
		ubLine++;
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"APs:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->actionPoints().current() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Breath:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->vitals().breath() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Life:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->vitals().health() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"LifeMax:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->vitals().maximumHealth() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Bleeding:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->vitals().bleeding() );

		ubLine = 2;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Agility:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 350, LINE_HEIGHT * ubLine, L"%d ( %d )", pSoldier->statistics().agility(), EffectiveAgility( pSoldier, FALSE ) );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Dexterity:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 350, LINE_HEIGHT * ubLine, L"%d( %d )", pSoldier->statistics().dexterity(), EffectiveDexterity( pSoldier, FALSE ) );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Strength:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 350, LINE_HEIGHT * ubLine, L"%d", pSoldier->statistics().strength() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Wisdom:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 350, LINE_HEIGHT * ubLine, L"%d ( %d )", pSoldier->statistics().wisdom(), EffectiveWisdom( pSoldier ) );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Exp Lvl:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 350, LINE_HEIGHT * ubLine, L"%d ( %d )", pSoldier->statistics().experienceLevel(), EffectiveExpLevel( pSoldier ) );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Mrksmnship:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 350, LINE_HEIGHT * ubLine, L"%d ( %d )", pSoldier->statistics().marksmanship(), EffectiveMarksmanship( pSoldier ) );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Mechanical:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 350, LINE_HEIGHT * ubLine, L"%d", pSoldier->statistics().mechanical());
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Explosive:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 350, LINE_HEIGHT * ubLine, L"%d", pSoldier->statistics().explosives());
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Medical:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 350, LINE_HEIGHT * ubLine, L"%d", pSoldier->statistics().medical());
		ubLine++;

		/*SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Drug Effects:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 400, LINE_HEIGHT * ubLine, L"%d", pSoldier->drugs.bDrugEffect[0] );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Drug Side Effects:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 400, LINE_HEIGHT * ubLine, L"%d", pSoldier->drugs.bDrugSideEffect[0] );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Booze Effects:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 400, LINE_HEIGHT * ubLine, L"%d", pSoldier->drugs.bDrugEffect[1] );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"Hangover Side Effects:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 400, LINE_HEIGHT * ubLine, L"%d", pSoldier->drugs.bDrugSideEffect[1] );
		ubLine++;*/

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 200, LINE_HEIGHT * ubLine, L"AI has Keys:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 400, LINE_HEIGHT * ubLine, L"%d", pSoldier->inventory().keyAccess() );
		ubLine++;
	}
	else if ( GetMouseMapPos( &usMapPos ) )
	{
		SetFont( LARGEFONT1 );
		gprintf( 0,0,L"DEBUG LAND PAGE ONE" );
		SetFont( LARGEFONT1 );

		ubLine++;
		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Num dirty rects:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 200, LINE_HEIGHT * ubLine, L"%d", guiNumBackSaves );
		ubLine++;


	}

}

void DebugSoldierPage2( )
{
	TacticalActor		*pSoldier;
	SoldierID		usSoldierIndex;
	UINT32			uiMercFlags;
	INT32			usMapPos;
	TILE_ELEMENT		TileElem;
	LEVELNODE		*pNode;
	UINT8			ubLine;

	if ( FindSoldierFromMouse( &usSoldierIndex, &uiMercFlags ) )
	{
		// Get Soldier
		GetSoldier( &pSoldier, usSoldierIndex );

		SetFont( LARGEFONT1 );
		gprintf( 0,0,L"DEBUG SOLDIER PAGE TWO, GRIDNO %d", pSoldier->position().gridNo() );
		SetFont( LARGEFONT1 );

		ubLine = 2;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"ID:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->identity().id() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Body Type:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->identity().bodyType() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Opp Cnt:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->awareness().opponentCount());
		ubLine++;

		if (pSoldier->roster().team() == OUR_TEAM || pSoldier->roster().team() == MILITIA_TEAM)	// look at 8 to 15 opplist entries
		{
			SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
			gprintf( 0, LINE_HEIGHT * ubLine, L"Opplist B:");
			SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
			gprintf( 150, LINE_HEIGHT * ubLine, L"%d %d %d %d %d %d %d %d", pSoldier->awareness().opponentKnowledge()[20],pSoldier->awareness().opponentKnowledge()[21],pSoldier->awareness().opponentKnowledge()[22],
							pSoldier->awareness().opponentKnowledge()[23],pSoldier->awareness().opponentKnowledge()[24],pSoldier->awareness().opponentKnowledge()[25],pSoldier->awareness().opponentKnowledge()[26],pSoldier->awareness().opponentKnowledge()[27]);
			ubLine++;
		}
		else	// team 1 - enemies so look at first 8 (0-7) opplist entries
		{
			SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
			gprintf( 0, LINE_HEIGHT * ubLine, L"OppList A:");
			SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
			gprintf( 150, LINE_HEIGHT * ubLine, L"%d %d %d %d %d %d %d %d", pSoldier->awareness().opponentKnowledge()[0],pSoldier->awareness().opponentKnowledge()[1],pSoldier->awareness().opponentKnowledge()[2],
							pSoldier->awareness().opponentKnowledge()[3],pSoldier->awareness().opponentKnowledge()[4],pSoldier->awareness().opponentKnowledge()[5],pSoldier->awareness().opponentKnowledge()[6],
							pSoldier->awareness().opponentKnowledge()[7]);
			ubLine++;
		}

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Visible:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->awareness().visibility());
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Direction:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%S", gzDirectionStr[ pSoldier->position().direction()] );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"DesDirection:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%S", gzDirectionStr[ pSoldier->pathing().desiredDirection()] );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"GridNo:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->position().gridNo() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Dest:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->pathing().finalDestinationGrid() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Path Size:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->pathing().pathSize());
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Path Index:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->pathing().pathIndex() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"First 3 Steps:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d %d %d", pSoldier->pathing().path()[0],
		pSoldier->pathing().path()[1],
		pSoldier->pathing().path()[2] );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Next 3 Steps:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d %d %d", pSoldier->pathing().path()[pSoldier->pathing().pathIndex()],
		pSoldier->pathing().path()[pSoldier->pathing().pathIndex() + 1],
		pSoldier->pathing().path()[pSoldier->pathing().pathIndex() + 2] );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"FlashInd:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->uiPresentation().locatorFlashCycle() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"ShowInd:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->uiPresentation().locatorVisibleState() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Main hand:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[HANDPOS].usItem] );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Second hand:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SECONDHANDPOS].usItem] );
		ubLine++;

		if ( GetMouseMapPos( &usMapPos ) )
		{
			SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
			gprintf( 0, LINE_HEIGHT * ubLine, L"CurrGridNo:");
			SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
			gprintf( 150, LINE_HEIGHT * ubLine, L"%d", usMapPos );
			ubLine++;
		}

	}
	else if ( GetMouseMapPos( &usMapPos ) )
	{
		SetFont( LARGEFONT1 );
		gprintf( 0,0,L"DEBUG LAND PAGE TWO" );
		SetFont( LARGEFONT1 );

		ubLine = 1;

		SetFontColors(COLOR1);
		mprintf( 0, LINE_HEIGHT * ubLine, L"Land Raised:");
		SetFontColors(COLOR2);
		mprintf( 150, LINE_HEIGHT * ubLine, L"%d", gpWorldLevelData[ usMapPos ].sHeight );
		ubLine++;

		SetFontColors(COLOR1);
		mprintf( 0, LINE_HEIGHT * ubLine, L"Land Node:");
		SetFontColors(COLOR2);
		mprintf( 150, LINE_HEIGHT * ubLine, L"%x", gpWorldLevelData[ usMapPos ].pLandHead );
		ubLine++;

		if ( gpWorldLevelData[ usMapPos ].pLandHead != NULL )
		{
			SetFontColors(COLOR1);
			mprintf( 0, LINE_HEIGHT * ubLine, L"Land Node:");
			SetFontColors(COLOR2);
			mprintf( 150, LINE_HEIGHT * ubLine, L"%d", gpWorldLevelData[ usMapPos ].pLandHead->usIndex );
			ubLine++;

			TileElem = gTileDatabase[ gpWorldLevelData[ usMapPos ].pLandHead->usIndex	];

			// Check for full tile
			SetFontColors(COLOR1);
			mprintf( 0, LINE_HEIGHT * ubLine, L"Full Land:");
			SetFontColors(COLOR2);
			mprintf( 150, LINE_HEIGHT * ubLine, L"%d", TileElem.ubFullTile );
			ubLine++;
		}

		SetFontColors(COLOR1);
		mprintf( 0, LINE_HEIGHT * ubLine, L"Land St Node:");
		SetFontColors(COLOR2);
		mprintf( 150, LINE_HEIGHT * ubLine, L"%x", gpWorldLevelData[ usMapPos ].pLandStart );
		ubLine++;

		SetFontColors(COLOR1);
		mprintf( 0, LINE_HEIGHT * ubLine, L"GRIDNO:");
		SetFontColors(COLOR2);
		//dnl ch85 060214
		INT16 sX, sY;
		ConvertGridNoToXY(usMapPos, &sX, &sY);
		mprintf( 150, LINE_HEIGHT * ubLine, L"%d (%d,%d)", usMapPos, sX, sY );
		ubLine++;

		if ( gpWorldLevelData[ usMapPos ].uiFlags & MAPELEMENT_MOVEMENT_RESERVED )
		{
			SetFontColors(COLOR2);
			mprintf( 0, LINE_HEIGHT * ubLine, L"Merc: %d",	gpWorldLevelData[ usMapPos ].ubReservedSoldierID );
			SetFontColors(COLOR2);
			mprintf( 150, LINE_HEIGHT * ubLine, L"RESERVED MOVEMENT FLAG ON:" );
			ubLine++;
		}


		pNode =	GetCurInteractiveTile( );

		if ( pNode != NULL )
		{
			SetFontColors(COLOR2);
			mprintf( 0, LINE_HEIGHT * ubLine, L"Tile: %d",	pNode->usIndex );
			SetFontColors(COLOR2);
			mprintf( 150, LINE_HEIGHT * ubLine, L"ON INT TILE" );
			ubLine++;
		}


		if ( gpWorldLevelData[ usMapPos ].uiFlags & MAPELEMENT_REVEALED )
		{
			SetFontColors(COLOR2);
			//mprintf( 0, LINE_HEIGHT * 9, L"Merc: %d",	gpWorldLevelData[ sMapPos ].ubReservedSoldierID );
			SetFontColors(COLOR2);
			mprintf( 150, LINE_HEIGHT * ubLine, L"REVEALED" );
			ubLine++;
		}

		if ( gpWorldLevelData[ usMapPos ].uiFlags & MAPELEMENT_RAISE_LAND_START )
		{
			SetFontColors(COLOR2);
			//mprintf( 0, LINE_HEIGHT * 9, L"Merc: %d",	gpWorldLevelData[ sMapPos ].ubReservedSoldierID );
			SetFontColors(COLOR2);
			mprintf( 150, LINE_HEIGHT * ubLine, L"Land Raise Start" );
			ubLine++;
		}

		if ( gpWorldLevelData[ usMapPos ].uiFlags & MAPELEMENT_RAISE_LAND_END )
		{
			SetFontColors(COLOR2);
			//mprintf( 0, LINE_HEIGHT * 9, L"Merc: %d",	gpWorldLevelData[ usMapPos ].ubReservedSoldierID );
			SetFontColors(COLOR2);
			mprintf( 150, LINE_HEIGHT * ubLine, L"Raise Land End" );
			ubLine++;
		}

		if (gusWorldRoomInfo[ usMapPos ] != NO_ROOM )
		{
			SetFontColors(COLOR2);
			mprintf( 0, LINE_HEIGHT * ubLine, L"Room Number" );
			SetFontColors(COLOR2);
			mprintf( 150, LINE_HEIGHT * ubLine, L"%d", gusWorldRoomInfo[ usMapPos ] );
			ubLine++;
		}

		if ( gpWorldLevelData[ usMapPos ].ubExtFlags[0] & MAPELEMENT_EXT_NOBURN_STRUCT )
		{
			SetFontColors(COLOR2);
			mprintf( 0, LINE_HEIGHT * ubLine, L"Don't Use Burn Through For Soldier" );
			ubLine++;
		}

	}

}


void DebugSoldierPage3( )
{
	TacticalActor	*pSoldier;
	SoldierID	usSoldierIndex;
	UINT32		uiMercFlags;
	INT32		usMapPos;
	UINT8		ubLine;

	if ( FindSoldierFromMouse( &usSoldierIndex, &uiMercFlags ) )
	{
		// Get Soldier
		GetSoldier( &pSoldier, usSoldierIndex );

		SetFont( LARGEFONT1 );
		gprintf( 0,0,L"DEBUG SOLDIER PAGE THREE, GRIDNO %d", pSoldier->position().gridNo() );
		SetFont( LARGEFONT1 );

		ubLine = 2;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"ID:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->identity().id() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Action:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%S", gzActionStr[ pSoldier->aiPlanning().action() ] );
		if (pSoldier->status().flags() & SOLDIER_ENEMY )
		{
			gprintf( 350, LINE_HEIGHT * ubLine, L"Alert %S", gzAlertStr[ pSoldier->aiBehavior().alertStatus() ] );
		}
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Action Data:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->aiPlanning().actionData() );

		if (pSoldier->status().flags() & SOLDIER_ENEMY )
		{
			gprintf( 350, LINE_HEIGHT * ubLine, L"AIMorale %d", pSoldier->morale().aiMorale() );
		}
		else
		{
			gprintf( 350, LINE_HEIGHT * ubLine, L"Morale %d", pSoldier->morale().morale() );
		}
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Delayed Movement:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->movement().delayCounter() );
		if ( gubWatchedLocPoints[ pSoldier->identity().id() ][ 0 ] > 0 )
		{
			gprintf( 350, LINE_HEIGHT * ubLine, L"Watch %d/%d for %d pts",
				gsWatchedLoc[ pSoldier->identity().id() ][ 0 ],
				gbWatchedLocLevel[ pSoldier->identity().id() ][ 0 ],
				gubWatchedLocPoints[ pSoldier->identity().id() ][ 0 ]
				);
		}

		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"ActionInProg:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->aiPlanning().actionInProgress());
		ubLine++;
		if ( gubWatchedLocPoints[ pSoldier->identity().id() ][ 1 ] > 0 )
		{
			gprintf( 350, LINE_HEIGHT * ubLine, L"Watch %d/%d for %d pts",
				gsWatchedLoc[ pSoldier->identity().id() ][ 1 ],
				gbWatchedLocLevel[ pSoldier->identity().id() ][ 1 ],
				gubWatchedLocPoints[ pSoldier->identity().id() ][ 1 ]
				);
		}

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Last Action:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%S", gzActionStr[ pSoldier->aiPlanning().lastAction() ]	);
		ubLine++;

		if ( gubWatchedLocPoints[ pSoldier->identity().id() ][ 2 ] > 0 )
		{
			gprintf( 350, LINE_HEIGHT * ubLine, L"Watch %d/%d for %d pts",
				gsWatchedLoc[ pSoldier->identity().id() ][ 2 ],
				gbWatchedLocLevel[ pSoldier->identity().id() ][ 2 ],
				gubWatchedLocPoints[ pSoldier->identity().id() ][ 2 ]
				);
		}

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Animation:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%S", gAnimControl[ pSoldier->animationPlayback().state() ].zAnimStr );
		ubLine++;

/*
		if ( gubWatchedLocPoints[ pSoldier->identity().id() ][ 3 ] > 0 )
		{
			gprintf( 350, LINE_HEIGHT * ubLine, L"Watch %d/%d for %d pts",
				gsWatchedLoc[ pSoldier->identity().id() ][ 3 ],
				gbWatchedLocLevel[ pSoldier->identity().id() ][ 3 ],
				gubWatchedLocPoints[ pSoldier->identity().id() ][ 3 ]
				);
		}
*/

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Getting Hit:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->animationActivity().hitPhase() );

		if (pSoldier->roster().civilianGroup() != 0)
		{
			gprintf( 350, LINE_HEIGHT * ubLine, L"Civ group %d", pSoldier->roster().civilianGroup() );
		}
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Suppress pts:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->suppression().points() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Attacker ID:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->combatResult().currentAttacker() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"EndAINotCalled:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->movement().turnActive() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"PrevAnimation:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%S", gAnimControl[ pSoldier->animationPlayback().previousState() ].zAnimStr );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"PrevAniCode:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", gusAnimInst[ pSoldier->animationPlayback().previousState() ][ pSoldier->animationPlayback().previousCode() ] );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"GridNo:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->position().gridNo());
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"AniCode:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", gusAnimInst[ pSoldier->animationPlayback().state() ][ pSoldier->animationPlayback().code() ] );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"No APS To fin Move:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->movement().outOfActionPoints() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Reload Delay:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->timing().reloadDelay() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Reloading:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->fireControl().reloading() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Bullets out:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->fireControl().bulletsLeft() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Anim non-int:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->animationActivity().nonInterruptible() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"RT Anim non-int:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->animationActivity().realtimeNonInterruptible() );
		ubLine++;

		// OPINION OF SELECTED MERC
		const TacticalActor* selectedSoldier =
			GetJa2SoldierRepository().resolve( gusSelectedSoldier );
		if ( selectedSoldier != nullptr &&
			selectedSoldier->identity().profile() != NO_PROFILE &&
			pSoldier->identity().profile() != NO_PROFILE && (
			gMercProfiles[selectedSoldier->identity().profile()].Type == PROFILETYPE_AIM ||
			gMercProfiles[selectedSoldier->identity().profile()].Type == PROFILETYPE_MERC ||
			gMercProfiles[selectedSoldier->identity().profile()].Type == PROFILETYPE_RPC ||
			gMercProfiles[selectedSoldier->identity().profile()].Type == PROFILETYPE_IMP ) )
		{
			SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
			gprintf( 0, LINE_HEIGHT * ubLine, L"NPC Opinion:");
			SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
			if (OKToCheckOpinion(selectedSoldier->identity().profile()))
				gprintf( 150, LINE_HEIGHT * ubLine, L"%d", gMercProfiles[ pSoldier->identity().profile() ].bMercOpinion[ selectedSoldier->identity().profile() ] );
			ubLine++;
		}
	}
	else if ( GetMouseMapPos( &usMapPos ) )
	{
		DOOR_STATUS	*pDoorStatus;
		STRUCTURE *pStructure;

		SetFont( LARGEFONT1 );
		gprintf( 0,0,L"DEBUG LAND PAGE THREE" );
		SetFont( LARGEFONT1 );

		// OK, display door information here.....
		pDoorStatus = GetDoorStatus( usMapPos );

		ubLine = 1;

		if ( pDoorStatus == NULL )
		{
			SetFontColors(COLOR1);
			mprintf( 0, LINE_HEIGHT * ubLine, L"No Door Status");
			ubLine++;
			ubLine++;
			ubLine++;
		}
		else
		{
			SetFontColors(COLOR1);
			mprintf( 0, LINE_HEIGHT * ubLine, L"Door Status Found:");
			SetFontColors(COLOR2);
			mprintf( 150, LINE_HEIGHT * ubLine, L" %d", usMapPos );
			ubLine++;

			SetFontColors(COLOR1);
			mprintf( 0, LINE_HEIGHT * ubLine, L"Actual Status:");
			SetFontColors(COLOR2);

			if ( pDoorStatus->ubFlags & DOOR_OPEN )
			{
				mprintf( 200, LINE_HEIGHT * ubLine, L"OPEN" );
			}
			else
			{
				mprintf( 200, LINE_HEIGHT * ubLine, L"CLOSED" );
			}
			ubLine++;


			SetFontColors(COLOR1);
			mprintf( 0, LINE_HEIGHT * ubLine, L"Perceived Status:");
			SetFontColors(COLOR2);

			if ( pDoorStatus->ubFlags & DOOR_PERCEIVED_NOTSET )
			{
				mprintf( 200, LINE_HEIGHT * ubLine, L"NOT SET" );
			}
			else
			{
				if ( pDoorStatus->ubFlags & DOOR_PERCEIVED_OPEN )
				{
					mprintf( 200, LINE_HEIGHT * ubLine, L"OPEN" );
				}
				else
				{
					mprintf( 200, LINE_HEIGHT * ubLine, L"CLOSED" );
				}
			}
			ubLine++;
		}

		//Find struct data and se what it says......
		pStructure = FindStructure( usMapPos, STRUCTURE_ANYDOOR );

		if ( pStructure == NULL )
		{
			SetFontColors(COLOR1);
			mprintf( 0, LINE_HEIGHT * ubLine, L"No Door Struct Data");
			ubLine++;
		}
		else
		{

			SetFontColors(COLOR1);
			mprintf( 0, LINE_HEIGHT * ubLine, L"State:");
			SetFontColors(COLOR2);
			if ( !(pStructure->fFlags & STRUCTURE_OPEN) )
			{
				mprintf( 200, LINE_HEIGHT * ubLine, L"CLOSED" );
			}
			else
			{
				mprintf( 200, LINE_HEIGHT * ubLine, L"OPEN" );
			}
			ubLine++;
		}
	}

}

void AppendAttachmentCode( UINT16 usItem, CHAR16 *str )
{
	switch( usItem )
	{
		case SILENCER:
			wcscat( str, L" Sil" );
			break;
		case SNIPERSCOPE:
			wcscat( str, L" Scp" );
			break;
		case BIPOD:
			wcscat( str, L" Bip" );
			break;
		case LASERSCOPE:
			wcscat( str, L" Las" );
			break;
	}
}

void WriteQuantityAndAttachments( OBJECTTYPE *pObject, INT32 yp )
{
	CHAR16 szAttach[30];
	BOOLEAN fAttachments;
	//100%	Qty: 2	Attach:
	//100%	Qty: 2
	//100%	Attach:
	//100%
	if( pObject->exists() == false )
		return;
	//Build attachment string
	fAttachments = FALSE;
	if( (*pObject)[0]->AttachmentListSize() > 0 )
	{
		fAttachments = TRUE;
		swprintf( szAttach, L"(" );
		for (attachmentList::iterator iter = (*pObject)[0]->attachments.begin(); iter != (*pObject)[0]->attachments.end(); ++iter) {
			if(iter->exists())
				AppendAttachmentCode( iter->usItem, szAttach );
		}
		wcscat( szAttach, L" )" );
	}

	if( Item[pObject->usItem].usItemClass == IC_AMMO )
	{ //ammo
		if( pObject->ubNumberOfObjects > 1 )
		{
			CHAR16 str[100];
			CHAR16 temp[10];
			UINT8 i;
			swprintf( str, L"Clips:	%d	(%d", pObject->ubNumberOfObjects, (*pObject)[0]->data.objectStatus );
			for( i = 1; i < pObject->ubNumberOfObjects; i++ )
			{
				swprintf( temp, L", %d", (*pObject)[0]->data.objectStatus );
				wcscat( str, temp );
			}
			wcscat( str, L")" );
			gprintf( 320, yp, str );
		}
		else
			gprintf( 320, yp, L"%d rounds", (*pObject)[0]->data.objectStatus );
		return;
	}
	if( pObject->ubNumberOfObjects > 1 && fAttachments )
	{ //everything
		gprintf( 320, yp, L"%d%%	Qty:	%d	%s",
			(*pObject)[0]->data.objectStatus, pObject->ubNumberOfObjects, szAttach );
	}
	else if( pObject->ubNumberOfObjects > 1 )
	{ //condition and quantity
		gprintf( 320, yp, L"%d%%	Qty:	%d	",
			(*pObject)[0]->data.objectStatus, pObject->ubNumberOfObjects );
	}
	else if( fAttachments )
	{ //condition and attachments
		gprintf( 320, yp, L"%d%%	%s", (*pObject)[0]->data.objectStatus, szAttach );
	}
	else
	{ //condition
		gprintf( 320, yp, L"%d%%", (*pObject)[0]->data.objectStatus );
	}
}

void DebugSoldierPage4( )
{
	TacticalActor	*pSoldier;
	UINT32		uiMercFlags;
	CHAR16		szOrders[20];
	CHAR16		szAttitude[20];
	SoldierID	usSoldierIndex;
	UINT8		ubLine;

	if ( FindSoldierFromMouse( &usSoldierIndex, &uiMercFlags ) )
	{
		// Get Soldier
		GetSoldier( &pSoldier, usSoldierIndex );

		SetFont( LARGEFONT1 );
		gprintf( 0,0,L"DEBUG SOLDIER PAGE FOUR, GRIDNO %d", pSoldier->position().gridNo() );
		SetFont( LARGEFONT1 );
		ubLine = 2;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"Exp. Level:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d ( %d )", pSoldier->statistics().experienceLevel(), EffectiveExpLevel(pSoldier) ); // SANDRO - added effective level calc
		switch( pSoldier->roster().soldierClass() )
		{
			case SOLDIER_CLASS_ADMINISTRATOR:		gprintf( 320, LINE_HEIGHT * ubLine, L"(Administrator)" );	break;
			case SOLDIER_CLASS_ELITE:				gprintf( 320, LINE_HEIGHT * ubLine, L"(Army Elite)" );		break;
			case SOLDIER_CLASS_ARMY:				gprintf( 320, LINE_HEIGHT * ubLine, L"(Army Troop)" );		break;
			case SOLDIER_CLASS_CREATURE:			gprintf( 320, LINE_HEIGHT * ubLine, L"(Creature)" );		break;
			case SOLDIER_CLASS_GREEN_MILITIA:		gprintf( 320, LINE_HEIGHT * ubLine, L"(Green Militia)" );	break;
			case SOLDIER_CLASS_REG_MILITIA:			gprintf( 320, LINE_HEIGHT * ubLine, L"(Reg Militia)" );		break;
			case SOLDIER_CLASS_ELITE_MILITIA:		gprintf( 320, LINE_HEIGHT * ubLine, L"(Elite Militia)" );	break;
			case SOLDIER_CLASS_MINER:				gprintf( 320, LINE_HEIGHT * ubLine, L"(Miner)" );			break;
			case SOLDIER_CLASS_ZOMBIE:				gprintf( 320, LINE_HEIGHT * ubLine, L"(Zombie)" );			break;
			case SOLDIER_CLASS_BANDIT:				gprintf( 320, LINE_HEIGHT * ubLine, L"(Bandit)" );			break;
			case SOLDIER_CLASS_ROBOT:				gprintf( 320, LINE_HEIGHT * ubLine, L"(Army Robot)" );		break;

			default:	break; //don't care (don't write anything)
		}
		++ubLine;

		if( pSoldier->roster().team() != OUR_TEAM )
		{
			SOLDIERINITNODE		*pNode;
			switch( pSoldier->aiBehavior().orders() )
			{
				case STATIONARY:	swprintf( szOrders, L"STATIONARY" );			break;
				case ONGUARD:			swprintf( szOrders, L"ON GUARD" );				break;
				case ONCALL:			swprintf( szOrders, L"ON CALL" );					break;
				case SEEKENEMY:		swprintf( szOrders, L"SEEK ENEMY" );			break;
				case CLOSEPATROL:	swprintf( szOrders, L"CLOSE PATROL" );		break;
				case FARPATROL:		swprintf( szOrders, L"FAR PATROL" );			break;
				case POINTPATROL:	swprintf( szOrders, L"POINT PATROL" );		break;
				case RNDPTPATROL:	swprintf( szOrders, L"RND PT PATROL" );		break;
				case SNIPER:		swprintf( szOrders, L"SNIPER" );		break;
				default:					swprintf( szOrders, L"UNKNOWN" );					break;
			}
			switch( pSoldier->aiBehavior().attitude() )
			{
				case DEFENSIVE:		swprintf( szAttitude, L"DEFENSIVE" );			break;
				case BRAVESOLO:		swprintf( szAttitude, L"BRAVE SOLO" );		break;
				case BRAVEAID:		swprintf( szAttitude, L"BRAVE AID" );			break;
				case AGGRESSIVE:	swprintf( szAttitude, L"AGGRESSIVE" );		break;
				case CUNNINGSOLO:	swprintf( szAttitude, L"CUNNING SOLO" );	break;
				case CUNNINGAID:	swprintf( szAttitude, L"CUNNING AID"	);	break;
				default:					swprintf( szAttitude, L"UNKNOWN" );				break;
			}
			pNode = gSoldierInitHead;
			while( pNode )
			{
				if( pNode->pSoldier == pSoldier )
					break;
				pNode = pNode->next;
			}
			SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
			if( pNode )
			{
				gprintf( 0, LINE_HEIGHT * ubLine, L"%s, %s, REL EQUIP: %d, REL ATTR: %d",
					szOrders, szAttitude, pNode->pBasicPlacement->bRelativeEquipmentLevel,
					pNode->pBasicPlacement->bRelativeAttributeLevel );
			}
			else
			{
				gprintf( 0, LINE_HEIGHT * ubLine, L"%s, %s", szOrders, szAttitude );
			}
			ubLine++;
		}

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"ID:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		gprintf( 150, LINE_HEIGHT * ubLine, L"%d", pSoldier->identity().id() );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"HELMETPOS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[HELMETPOS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[HELMETPOS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[HELMETPOS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"VESTPOS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[VESTPOS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[VESTPOS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[VESTPOS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"LEGPOS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[LEGPOS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[LEGPOS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[LEGPOS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"HEAD1POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[HEAD1POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[HEAD1POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[HEAD1POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"HEAD2POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[HEAD2POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[HEAD2POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[HEAD2POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"HANDPOS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[HANDPOS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[HANDPOS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[HANDPOS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SECONDHANDPOS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SECONDHANDPOS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SECONDHANDPOS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SECONDHANDPOS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"BIGPOCK1POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[BIGPOCK1POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[BIGPOCK1POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[BIGPOCK1POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"BIGPOCK2POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[BIGPOCK2POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[BIGPOCK2POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[BIGPOCK2POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"BIGPOCK3POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[BIGPOCK3POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[BIGPOCK3POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[BIGPOCK3POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"BIGPOCK4POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[BIGPOCK4POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[BIGPOCK4POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[BIGPOCK4POS], LINE_HEIGHT*ubLine );
		ubLine++;

		// CHRISL: Added entries for all the new inventory pockets.
		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"BIGPOCK5POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[BIGPOCK5POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[BIGPOCK5POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[BIGPOCK5POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"BIGPOCK6POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[BIGPOCK6POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[BIGPOCK6POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[BIGPOCK6POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"BIGPOCK7POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[BIGPOCK7POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[BIGPOCK7POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[BIGPOCK7POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"MEDPOCK1POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[MEDPOCK1POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[MEDPOCK1POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[MEDPOCK1POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"MEDPOCK2POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[MEDPOCK2POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[MEDPOCK2POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[MEDPOCK2POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"MEDPOCK3POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[MEDPOCK3POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[MEDPOCK3POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[MEDPOCK3POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"MEDPOCK4POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[MEDPOCK4POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[MEDPOCK4POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[MEDPOCK4POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK1POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK1POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK1POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK1POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK2POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK2POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK2POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK2POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK3POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK3POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK3POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK3POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK4POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK4POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK4POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK4POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK5POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK5POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK5POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK5POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK6POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK6POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK6POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK6POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK7POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK7POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK7POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK7POS], LINE_HEIGHT*ubLine );
		ubLine++;

		// CHRISL: Added entries for all the new inventory pockets
		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK8POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK8POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK8POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK8POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK9POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK9POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK9POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK9POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK10POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK10POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK10POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK10POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK11POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK11POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK11POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK11POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK12POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK12POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK12POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK12POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK13POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK13POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK13POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK13POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK14POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK14POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK14POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK14POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK15POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK15POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK15POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK15POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK16POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK16POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK16POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK16POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK17POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK17POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK17POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK17POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK18POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK18POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK18POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK18POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK19POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK19POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK19POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK19POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK20POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK20POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK20POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK20POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK21POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK21POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK21POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK21POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK22POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK22POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK22POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK22POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK23POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK23POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK23POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK23POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK24POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK24POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK24POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK24POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK25POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK25POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK25POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK25POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK26POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK26POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK26POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK26POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK27POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK27POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK27POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK27POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK28POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK28POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK28POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK28POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK29POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK29POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK29POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK29POS], LINE_HEIGHT*ubLine );
		ubLine++;

		SetFontShade(LARGEFONT1, FONT_SHADE_GREEN);
		gprintf( 0, LINE_HEIGHT * ubLine, L"SMALLPOCK30POS:");
		SetFontShade(LARGEFONT1, FONT_SHADE_NEUTRAL);
		if( pSoldier->inventory()[SMALLPOCK30POS].usItem )
			gprintf( 150, LINE_HEIGHT * ubLine, L"%s", ShortItemNames[pSoldier->inventory()[SMALLPOCK30POS].usItem] );
		WriteQuantityAndAttachments( &pSoldier->inventory()[SMALLPOCK30POS], LINE_HEIGHT*ubLine );
		ubLine++;
	}
	else
	{
		SetFont( LARGEFONT1 );
		gprintf( 0,0,L"DEBUG LAND PAGE FOUR" );
		SetFont( LARGEFONT1 );
	}
}

//
// Noise stuff
//

#define MAX_MOVEMENT_NOISE 9
#define VEHICLE_FAST_MOVEMENT_NOISE 25
#define VEHICLE_NORMAL_MOVEMENT_NOISE 15

UINT8 MovementNoise(TacticalActor *pSoldier)
{
	INT32	iStealthSkill, iRoll;
	INT16	sMaxVolume, sVolume;
	INT8	bBandaged, bEffLife;

	// anv: vehicle and passengers
	if (pSoldier->status().flags() & (SOLDIER_DRIVER | SOLDIER_PASSENGER))
	{
		return(0);
	}
	else if (pSoldier->status().flags() & (SOLDIER_VEHICLE))
	{
		if (pSoldier->animationPlayback().state() == RUNNING)
		{
			// driving fast makes engine work louder
			return(VEHICLE_FAST_MOVEMENT_NOISE);
		}
		else
		{
			return(VEHICLE_NORMAL_MOVEMENT_NOISE);
		}
	}

	// anv: additional tile properties
	// modify amount of noise and stealth difficulty depending on surface type

	INT8 bGroundVolumeModifier = 0;
	INT8 bGroundStealthDifficultyModifier = 0;
	if (gGameExternalOptions.fAdditionalTileProperties)
	{
		ADDITIONAL_TILE_PROPERTIES_VALUES zGivenTileProperties = GetAllAdditonalTilePropertiesForGrid(pSoldier->position().gridNo(), pSoldier->position().level());
		bGroundVolumeModifier = zGivenTileProperties.bSoundModifier;
		bGroundStealthDifficultyModifier = zGivenTileProperties.bStealthDifficultyModifer;
	}
	if (pSoldier->roster().team() == ENEMY_TEAM)
	{
		return((UINT8)(MAX_MOVEMENT_NOISE - PreRandom(2)) + bGroundVolumeModifier);
	}

	// CHANGED BY SANDRO - LET'S MAKE THE STEALTH BASED ON AGILITY LIKE IT SHOULD BE
	iStealthSkill = 20 + 4 * EffectiveExpLevel(pSoldier) + ((EffectiveAgility(pSoldier, FALSE) * 4) / 10); // 24-100

	// big bonus for those "extra stealthy" mercs
	if (pSoldier->identity().bodyType() == BLOODCAT)
	{
		iStealthSkill += 50;
	}
	// SANDRO - new/old traits
	else if (gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT(pSoldier, STEALTHY_NT))
	{
		iStealthSkill += gSkillTraitValues.ubSTBonusToMoveQuietly;
	}
	else if (!gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT(pSoldier, STEALTHY_OT))
	{
		iStealthSkill += 25 * NUM_SKILL_TRAITS(pSoldier, STEALTHY_OT);
	}

	INT16 wornstealth = GetWornStealth(pSoldier);
	if (wornstealth > 0)
		iStealthSkill += wornstealth / 2;

	bBandaged = pSoldier->vitals().maximumHealth() - pSoldier->vitals().health() - pSoldier->vitals().bleeding();
	bEffLife = pSoldier->vitals().health() + (bBandaged / 2);

	// IF "SNEAKER'S" "EFFECTIVE LIFE" IS AT LESS THAN 50
	if (bEffLife < 50)
	{
		// reduce effective stealth skill by up to 50% for low life
		iStealthSkill -= (iStealthSkill * (50 - bEffLife)) / 100;
	}

	// if breath is below 50%
	if (pSoldier->vitals().breath() < 50)
	{
		// reduce effective stealth skill by up to 50%
		iStealthSkill -= (iStealthSkill * (50 - pSoldier->vitals().breath())) / 100;
	}

	// if sneaker is moving through water
	if (Water(pSoldier->position().gridNo(), pSoldier->position().level()))
	{
		iStealthSkill -= 10; // 10% penalty
	}
	else if (DeepWater(pSoldier->position().gridNo(), pSoldier->position().level()))
	{
		iStealthSkill -= 20; // 20% penalty
	}

	iStealthSkill = __max(iStealthSkill, 0);

	if (!pSoldier->movement().stealthMode())	// REGULAR movement
	{
		//ubMaxVolume = MAX_MOVEMENT_NOISE - (iStealthSkill / 16);	// 9 - (0 to 6) => 3 to 9
		sMaxVolume = MAX_MOVEMENT_NOISE - (iStealthSkill / 16) + bGroundVolumeModifier;	// 9 - (0 to 6) => 3 to 9

		if (Water(pSoldier->position().gridNo(), pSoldier->position().level()))
		{
			sMaxVolume++;		// in water, can be even louder
		}
		switch (pSoldier->movement().mode())
		{
		case CRAWLING:
			sMaxVolume -= 2;
			break;
		case SWATTING:
		case SWATTING_WK:
			sMaxVolume -= 1;
			break;
		case RUNNING:
			sMaxVolume += 3;
			break;
		}

		if (sMaxVolume < 2)
		{
			sVolume = sMaxVolume;
		}
		else
		{
			sVolume = 1 + (UINT8)PreRandom(sMaxVolume);	// actual volume is 1 to max volume
		}
	}
	else			// in STEALTH mode
	{
		iRoll = (INT32)PreRandom(100) + bGroundStealthDifficultyModifier;	// roll them bones!

		if (iRoll >= iStealthSkill)	// v1.13 modification: give a second chance!
		{
			iRoll = (INT32)PreRandom(100) + bGroundStealthDifficultyModifier;
		}

		if (iRoll < iStealthSkill)
		{
			sVolume = 0;	// made it, stayed quiet moving through this tile
		}
		else	// OOPS!
		{
			sVolume = 1 + ((iRoll - iStealthSkill + 1) / 16);	// volume is 1 - 7 ...
			switch (pSoldier->movement().mode())
			{
			case CRAWLING:
				sVolume -= 2;
				break;
			case SWATTING:
			case SWATTING_WK:
				sVolume -= 1;
				break;
			case RUNNING:
				sVolume += 3;
				break;
			}
		}
	}

	// sevenfm: if dragging something, add dragging sound volume
	if (TacticalActorDragging::isDragging(*pSoldier))
	{
		sVolume = max(sVolume, MAX_MOVEMENT_NOISE / 2 + Random(MAX_MOVEMENT_NOISE) + bGroundVolumeModifier);
	}

	sVolume = max(0, sVolume);
	sVolume = min(255, sVolume);

	return (UINT8)sVolume;
}

UINT8 DoorOpeningNoise( TacticalActor *pSoldier )
{
	// door being opened gridno is always the pending-action-data2 value
	INT32 sGridNo = pSoldier->pendingAction().secondaryData();
	DOOR_STATUS	*pDoorStatus = GetDoorStatus( sGridNo );
	UINT8 ubDoorNoise = 0;

	// Find the base tile for the door structure and use that gridno
	STRUCTURE *pStructure = FindStructure(sGridNo, STRUCTURE_ANYDOOR);
	if (pStructure)
	{
		ubDoorNoise = 8;//shadooow: this indicates at how many tiles can be the noise heard (was 2 originally)
		// OK, check if this door is sliding and is multi-tiled...
		if (pStructure->fFlags & STRUCTURE_SLIDINGDOOR)
		{
			// Get database value...
			if (pStructure->pDBStructureRef->pDBStructure->ubNumberOfTiles > 1)
			{
				// garage doors
				ubDoorNoise += 4;
			}
			else if (pStructure->pDBStructureRef->pDBStructure->ubArmour == MATERIAL_CLOTH)
			{
				// curtains
				ubDoorNoise -= 4;
			}
		}
		else if (pStructure->pDBStructureRef->pDBStructure->ubArmour == MATERIAL_LIGHT_METAL ||
			pStructure->pDBStructureRef->pDBStructure->ubArmour == MATERIAL_THICKER_METAL ||
			pStructure->pDBStructureRef->pDBStructure->ubArmour == MATERIAL_HEAVY_METAL)
		{
			// metal doors
			ubDoorNoise += 2;
		}

		if (pDoorStatus && pDoorStatus->ubFlags & DOOR_HAS_TIN_CAN)
		{
			//shadooow: do not allow stealth to work if there is can attached to doors
			ubDoorNoise += 4;
		}
		else if (pSoldier->movement().stealthMode())
		{
			// CHANGED BY SANDRO - LET'S MAKE THE STEALTH BASED ON AGILITY LIKE IT SHOULD BE
			INT32 iStealthSkill = 20 + 4 * EffectiveExpLevel(pSoldier) + ((EffectiveAgility(pSoldier, FALSE) * 4) / 10); // 24-100

			INT8 bEffLife = pSoldier->vitals().health() + ((pSoldier->vitals().maximumHealth() - pSoldier->vitals().health() - pSoldier->vitals().bleeding()) / 2);

			// IF "SNEAKER'S" "EFFECTIVE LIFE" IS AT LESS THAN 50
			if (bEffLife < 50)
			{
				// reduce effective stealth skill by up to 50% for low life
				iStealthSkill -= (iStealthSkill * (50 - bEffLife)) / 100;
			}

			// if breath is below 50%
			if (pSoldier->vitals().breath() < 50)
			{
				// reduce effective stealth skill by up to 50%
				iStealthSkill -= (iStealthSkill * (50 - pSoldier->vitals().breath())) / 100;
			}

			iStealthSkill = __max(iStealthSkill, 0);

			INT32 iRoll = (INT32)PreRandom(100);	// roll them bones!

			if (iRoll >= iStealthSkill)	// v1.13 modification: give a second chance!
			{
				iRoll = (INT32)PreRandom(100);
			}

			// succeeded in being stealthy!
			if (iRoll < iStealthSkill)
			{
				ubDoorNoise = 0;
			}
		}
	}

	return( ubDoorNoise );
}

void MakeNoise(SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel, UINT8 ubTerrType, UINT8 ubVolume, UINT8 ubNoiseType,  STR16 zNoiseMessage )
{
	EV_S_NOISE	SNoise;

	SNoise.ubNoiseMaker = ubNoiseMaker;
	SNoise.sGridNo = sGridNo;
	SNoise.bLevel = bLevel;
	SNoise.ubTerrType = ubTerrType;
	SNoise.ubVolume = ubVolume;
	SNoise.ubNoiseType = ubNoiseType;
	swprintf( SNoise.zNoiseMessage, L"%s", zNoiseMessage );
	//SNoise.zNoiseMessage = zNoiseMessage;

	if ( GetJa2PendingTacticalCombatActions() )
	{
		// delay these events until the attack is over!
		AddGameEvent( S_NOISE, DEMAND_EVENT_DELAY, &SNoise );
	}
	else
	{
		// AddGameEvent( S_NOISE, 0, &SNoise );

		// now call directly
		OurNoise( SNoise.ubNoiseMaker, SNoise.sGridNo, SNoise.bLevel, SNoise.ubTerrType, SNoise.ubVolume, SNoise.ubNoiseType, SNoise.zNoiseMessage );
	}

/*
	INT8 bWeControlNoise = FALSE;

	if (ubNoiseMode == UNEXPECTED)
	{
		bWeControlNoise = TRUE;
	}
	else	// EXPECTED noise
	{
		if (ubNoiseMaker < TOTAL_SOLDIERS)
		{
			if (ubNoiseMaker->controller == Net.pnum)
			{
				bWeControlNoise = TRUE;
			}
		}
		else
		{
			// expected noise by NOBODY is sent by LEADER, received by others
			if (Net.pnum == LEADER)
			{
				bWeControlNoise = TRUE;
			}
		}
	}

	if (bWeControlNoise)
	{
		OurNoise(ubNoiseMaker,sGridNo,ubTerrType,ubVolume,ubNoiseType,ubNoiseMode);
	}
	else
	{
		// can't be UNEXPECTED, check if it's a SEND or NO_SEND
		if (ubNoiseMode == EXPECTED_NOSEND)
		{
			// no NET_NOISE message is required, trigger TheirNoise() right here
			TheirNoise(ubNoiseMaker,sGridNo,ubTerrType,ubVolume,ubNoiseType,ubNoiseMode);
		}
		else
		{

			// EXPECTED_SEND, TheirNoise() will be triggered by the arrival of the
			// NET_NOISE message, not by us.	Wait here until that's all done...

			// wait for the NET_NOISE to arrive (it will set noiseReceived flag)
			//stopAction = TRUE;		// prevent real-time events from passing us by
			MarkTime(&LoopTime);
			while (Status.noiseReceived != ubNoiseType)
			{
				LoopTimePast = Elapsed(&LoopTime);
				if (LoopTimePast > 50 && LoopTimePast < 2000)
				{
					KeepInterfaceGoing(19); // xxx yyy zzz experimental Aug 16/96 9:15 pm
				}
				else
				{
					KeyHitReport("MakeNoise: Waiting for NET_NOISE, need ubNoiseType ",ubNoiseType);
				}
				CheckForNetIncoming();
			};
			//stopAction = FALSE;	// re-enable real-time events

			// turn off the oppChk flag again
			Status.noiseReceived = -1;

		}
	}
*/
}


void OurNoise( SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel, UINT8 ubTerrType, UINT8 ubVolume, UINT8 ubNoiseType, STR16 zNoiseMessage )
{
#ifdef BYPASSNOISE
	return;
#endif

#ifdef BETAVERSION
	tempstr = String("OurNoise: ubNoiseType = %s, ubNoiseMaker = %d, ubNoiseMode = %d, sGridNo = %d, ubVolume = %d",
			NoiseTypeStr[ubNoiseType],ubNoiseMaker,ubNoiseMode,sGridNo,ubVolume);
#ifdef RECORDNET
	fprintf(NetDebugFile,"\t%s\n",tempstr);
#endif
#ifdef TESTNOISE
	PopMessage(tempstr);
#endif
#endif

	// see if anyone actually hears this noise, sees ubNoiseMaker, etc.
	ProcessNoise(ubNoiseMaker, sGridNo, bLevel, ubTerrType,	ubVolume,	ubNoiseType, zNoiseMessage );

	if (IsJa2TacticalTurnBasedCombat() && (ubNoiseMaker < TOTAL_SOLDIERS) && !gfDelayResolvingBestSightingDueToDoor )
	{
		// interrupts are possible, resolve them now (we're in control here)
		// (you can't interrupt NOBODY, even if you hear the noise)
		TacticalActor* noiseMaker =
			GetJa2SoldierRepository().resolve( ubNoiseMaker );
		if ( noiseMaker != nullptr )
		{
			ResolveInterruptsVs(noiseMaker, NOISEINTERRUPT);
		}
	}
}



void TheirNoise( SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel, UINT8 ubTerrType,
 UINT8 ubVolume,
	UINT8 ubNoiseType, STR16 zNoiseMessage )
{
//	TacticalActor *pSoldier;


#ifdef BYPASSNOISE
	return;
#endif


#ifdef BETAVERSION
	tempstr = String("TheirNoise: ubNoiseType = %s, ubNoiseMaker = %d, ubNoiseMode = %d, sGridNo = %d, ubVolume = %d",
			NoiseTypeStr[ubNoiseType],ubNoiseMaker,ubNoiseMode,sGridNo,ubVolume);
#ifdef RECORDNET
	fprintf(NetDebugFile,"\t%s\n",tempstr);
#endif

#ifdef TESTNOISE
	PopMessage(tempstr);
#endif
#endif

	// see if anyone actually hears this noise, sees noiseMaker, etc.
	ProcessNoise(ubNoiseMaker,sGridNo,bLevel,ubTerrType,ubVolume,ubNoiseType,zNoiseMessage);

	// if noiseMaker is SOMEBODY
	if (ubNoiseMaker < TOTAL_SOLDIERS)
	{
		/*
		pSoldier = ubNoiseMaker;

		//stopAction = TRUE;		// prevent real-time events from passing us by
		MarkTime(&LoopTime);
		do
		{
			LoopTimePast = Elapsed(&LoopTime);
			if (LoopTimePast > 50 && LoopTimePast < 2000)
			{
				KeepInterfaceGoing(20); // xxx yyy zzz experimental Aug 16/96 9:15 pm
			}
			else
			{
				// the gridno is added to end of the string by KeyHitReport itself...
				tempstr = String("TheirNoise: Waiting for NOISE_INT_DONE for guynum %d, ubNoiseType %d(%s), sGridNo ",
					pSoldier->guynum,ubNoiseType,NoiseTypeStr[ubNoiseType]);
				KeyHitReport(tempstr,sGridNo);
			}

			CheckForNetIncoming();
		} while ((ExtMen[pSoldier->guynum].noiseRcvdGridno[ubNoiseType] != sGridNo) && pSoldier->in_sector);
		//stopAction = FALSE;	// re-enable real-time events

		// reset the gridno flag for next time
		ExtMen[pSoldier->guynum].noiseRcvdGridno[ubNoiseType] = NOWHERE;
		*/
	}
	// else if noiseMaker's NOBODY, no opplist changes or interrupts are possible
}

void ProcessNoise( SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel, UINT8 ubTerrType, UINT8 ubBaseVolume, UINT8 ubNoiseType, STR16 zNoiseMessage )
{
	TacticalActor *pSoldier;
	UINT8 bTeam;
	UINT8 ubLoudestEffVolume, ubEffVolume;
//	UINT8 ubPlayVolume;
	UINT8 ubSourceTerrType;
	SoldierID ubSource;
	INT8 bTellPlayer = FALSE, bHeard, bSeen;
	SoldierID ubHeardLoudestBy = NOBODY;
	UINT8 ubNoiseDir = 0xff, ubLoudestNoiseDir = 0xff;
	TacticalActor* noiseMaker =
		GetJa2SoldierRepository().resolve( ubNoiseMaker );


#ifdef RECORDOPPLIST
	fprintf(OpplistFile,"PN: nType=%s, nMaker=%d, g=%d, tType=%d, bVol=%d\n",
		NoiseTypeStr[noiseType],ubNoiseMaker,sGridNo,ubTerrType,baseVolume);
#endif

	// if the base volume itself was negligible
	if (!ubBaseVolume)
		return;


	// EXPLOSIONs are special, because they COULD be caused by a merc who is
	// no longer alive (but he placed the bomb or flaky grenade in the past).
	// Later noiseMaker gets whacked to NOBODY anyway, so that's OK.	So a
	// dead noiseMaker is only used here to decide WHICH soldiers HearNoise().

	// if noise is made by a person, AND it's not noise from an explosion
	if (noiseMaker != nullptr && (ubNoiseType != NOISE_EXPLOSION))
	{
		// inactive/not in sector/dead soldiers, shouldn't be making noise!
		if (!noiseMaker->roster().active() || !noiseMaker->roster().inSector() ||
			noiseMaker->status().flags() & SOLDIER_DEAD)
		{
#ifdef BETAVERSION
			NumMessage("ProcessNoise: ERROR - Noisemaker is inactive/not in sector/dead, Guy #",ubNoiseMaker);
#endif
			return;
		}

		// if he's out of life, and this isn't just his "dying scream" which is OK
		if (!noiseMaker->vitals().health() && (ubNoiseType != NOISE_SCREAM))
		{
#ifdef BETAVERSION
			NumMessage("ProcessNoise: ERROR - Noisemaker is lifeless, Guy #",ubNoiseMaker);
#endif
			return;
		}
	}


	// DETERMINE THE TERRAIN TYPE OF THE GRIDNO WHERE NOISE IS COMING FROM

	ubSourceTerrType = gpWorldLevelData[sGridNo].ubTerrainID;
/*
	// start with the terrain type passed in to us
	ubSourceTerrType = ubTerrType;

	// if this isn't enough to get a valid terrain type
	if ((ubSourceTerrType < GROUNDTYPE) || (ubSourceTerrType > OCEANTYPE))
	{
		// use the source gridno of the noise itself
		ubSourceTerrType = TTypeList[Terrain(sGridNo)];
	}
	*/

	// DETERMINE THE *PERCEIVED* SOURCE OF THE NOISE
	switch (ubNoiseType)
	{
		// for noise generated by an OBJECT shot/thrown/dropped by the noiseMaker
		case NOISE_ROCK_IMPACT:
			gsWhoThrewRock = ubNoiseMaker;
			//fall through here!!!
		case NOISE_BULLET_IMPACT:
		case NOISE_GRENADE_IMPACT:
		case NOISE_EXPLOSION:
			// the source of the noise is not at all obvious, so hide it from
			// the listener and maintain noiseMaker's cover by making source NOBODY
			ubSource = NOBODY;
			break;

		default:
			// normal situation: the noiseMaker is obviously the source of the noise
			ubSource = noiseMaker != nullptr ? ubNoiseMaker : NOBODY;
			break;
	}

	// LOOP THROUGH EACH TEAM
	for (bTeam = 0; bTeam < MAXTEAMS; bTeam++)
	{
		// skip any inactive teams
		if (!gTacticalStatus.Team[bTeam].bTeamActive)
		{
			continue;
		}

		// if a the noise maker is a person, not just NOBODY
		if (noiseMaker != nullptr)
		{
			// if this team is the same TEAM as the noise maker's
			// (for now, assume we will report noises by unknown source on same SIDE)
			// OR, if the noise maker is currently in sight to this HUMAN team

			// CJC: changed to if the side is the same side as the noise maker's!
			// CJC: changed back!

			if (bTeam == noiseMaker->roster().team())
			{
				continue;
			}

			if (gTacticalStatus.Team[bTeam].bHuman)
			{
				if (gbPublicOpplist[bTeam][ubNoiseMaker] == SEEN_CURRENTLY && ubNoiseType != NOISE_VOICE)
				{
					continue;
				}
			}
		}

#ifdef REPORTTHEIRNOISE
		// if this is any team
		if (TRUE)
#else
		// if this is our team
		if (TRUE)
		//if (bTeam == Net.pnum)
#endif
		{
			// tell player about noise if enemies are present
			bTellPlayer = gTacticalStatus.fEnemyInSector && ( !(IsJa2TacticalCombatActive()) || (GetJa2TacticalCurrentTeam()) );

#ifndef TESTNOISE
			switch (ubNoiseType)
			{
				case NOISE_GUNFIRE:
				case NOISE_BULLET_IMPACT:
				case NOISE_ROCK_IMPACT:
				case NOISE_GRENADE_IMPACT:
					// It's noise caused by a projectile.	If the projectile was seen by
					// the local player while in flight (PublicBullet), then don't bother
					// giving him a message about the noise it made, he's obviously aware.
					if (1 /*PublicBullet*/)
					{
						bTellPlayer = FALSE;
					}

					break;

				case NOISE_EXPLOSION:
					// if center of explosion is in visual range of team, don't report
					// noise, because the player is already watching the thing go BOOM!
					if (TeamMemberNear(bTeam,sGridNo,STRAIGHT))
					{
						bTellPlayer = FALSE;
					}
					break;

				case NOISE_SILENT_ALARM:
				case NOISE_CREAKING://shadooow: doors will make sound of being opened/closed so I see no reason to write it to player
					bTellPlayer = FALSE;
					break;
			}

			// if noise was made by a person
			if (noiseMaker != nullptr)
			{
				// if noisemaker has been *PUBLICLY* SEEN OR HEARD during THIS TURN
				if ((gbPublicOpplist[bTeam][ubNoiseMaker] == SEEN_CURRENTLY) || // seen now
					(gbPublicOpplist[bTeam][ubNoiseMaker] == SEEN_THIS_TURN) || // seen this turn
					(gbPublicOpplist[bTeam][ubNoiseMaker] == HEARD_THIS_TURN))	// heard this turn
				{
					// then don't bother reporting any noise made by him to the player
					bTellPlayer = FALSE;
				}
				/*
				else if ( (ubNoiseMaker->awareness().visibility() == TRUE) && (bTeam == gbPlayerNum) )
				{
					ScreenMsg( MSG_FONT_YELLOW, MSG_TESTVERSION, L"Handling noise from person not currently seen in player's public opplist" );
				}
				*/

				// anv: special exception: we want to report enemy taunt, because of text content
				if ( ubNoiseType == NOISE_VOICE )
				{
					bTellPlayer = TRUE;
				}

				if ( noiseMaker->vitals().health() == 0 )
				{
					// this guy is dead (just dying) so don't report to player
					bTellPlayer = FALSE;
				}

			}
		}
#endif

		// refresh flags for this new team
		bHeard = FALSE;
		bSeen = FALSE;
		ubLoudestEffVolume = 0;
		ubHeardLoudestBy = NOBODY;

		// All mercs on this team check if they are eligible to hear this noise
		for ( SoldierID bLoop = gTacticalStatus.Team[bTeam].bFirstID; bLoop <= gTacticalStatus.Team[bTeam].bLastID; ++bLoop )
		{
			pSoldier = GetJa2SoldierRepository().resolve( bLoop );
			if ( pSoldier == nullptr )
			{
				continue;
			}

			// if this "listener" is inactive, or in no condition to care
			if (!pSoldier->roster().active() || !pSoldier->roster().inSector() || pSoldier->status().flags() & SOLDIER_DEAD || (pSoldier->vitals().health() < OKLIFE) || pSoldier->identity().bodyType() == LARVAE_MONSTER)
			{
				continue;			// skip him!
			}

			if ( pSoldier->status().flags() & SOLDIER_VEHICLE && pSoldier->roster().team() == OUR_TEAM	)
			{
				continue; // skip
			}

			if ( bTeam == gbPlayerNum && (pSoldier->assignment().current() == ASSIGNMENT_POW || pSoldier->assignment().current() == ASSIGNMENT_MINIEVENT || pSoldier->assignment().current() == ASSIGNMENT_REBELCOMMAND) )
			{
				// POWs should not be processed for noise
				continue;
			}

			// Can the listener hear noise of that volume given his circumstances?
			ubEffVolume = CalcEffVolume(pSoldier, sGridNo, bLevel, ubNoiseType, ubBaseVolume, pSoldier->position().terrainType(), ubSourceTerrType);

			// if a the noise maker is a person, not just NOBODY
			if (noiseMaker != nullptr)
			{
				// if this listener can see this noise maker
				if (pSoldier->awareness().opponentKnowledge()[ubNoiseMaker] == SEEN_CURRENTLY)
				{
					// civilians care about gunshots even if they come from someone they can see
					// ChrisL: Crows will fly away if they hear any noise
					if ( !( pSoldier->aiBehavior().neutral() && ubNoiseType == NOISE_GUNFIRE ) && pSoldier->identity().bodyType() != CROW )
					{
						// anv: we want to report taunt even if we see noisemaker
						if(ubNoiseType != NOISE_VOICE)
						{
							continue;		// then who cares whether he can also hear the guy?
						}
					}
				}

				// screen out allied militia from hearing us
				switch( noiseMaker->roster().team() )
				{
					case OUR_TEAM:
						// if the listener is militia and still on our side, ignore noise from us
						if ( pSoldier->roster().team() == MILITIA_TEAM && pSoldier->roster().side() == 0 )
						{
							continue;
						}
						break;
					case ENEMY_TEAM:
						switch( pSoldier->identity().profile() )
						{
							case WARDEN:
							case GENERAL:
							case SERGEANT:
							case CONRAD:	
								// WANNE: This fixes the bug, that Conrad, when he is in OUR team, cannot interrupt enemies!!!
								if (pSoldier->roster().team() == OUR_TEAM)
								{
									// WANNE: Allow interrupt, if one of those guys is in OUR team!
									break;
								}
								else
								{
									// No interrupt for those 4 guys
									continue;
								}
							default:
								break;
						}
						break;
					case MILITIA_TEAM:
						// if the noisemaker is militia and still on our side, ignore noise if we're listening
						// sevenfm: allow taunts from militia
						if (pSoldier->roster().team() == OUR_TEAM && noiseMaker->roster().side() == 0 && (ubNoiseType != NOISE_VOICE || !gTauntsSettings.fTauntVoice))
						//if ( pSoldier->roster().team() == OUR_TEAM && ubNoiseMaker->roster().side() == 0 )
						{
							continue;
						}
						break;
				}

				// HEADROCK HAM 3.6: Bloodcat "static" sectors have been externalized, and there can be more than one.
				// Also, there's a toggle that determines whether or not bloodcats can sense enemies in this sector.
				UINT8 ubSectorID = SECTOR(gWorldSectorX, gWorldSectorY);
				UINT8 PlacementType = gBloodcatPlacements[ ubSectorID ][0].PlacementType;

			UINT8 DiffLevel;
			if( gGameOptions.ubDifficultyLevel == DIF_LEVEL_EASY )
				DiffLevel = 1;
			else if( gGameOptions.ubDifficultyLevel == DIF_LEVEL_MEDIUM )
				DiffLevel = 2;
			else if( gGameOptions.ubDifficultyLevel == DIF_LEVEL_HARD )
				DiffLevel = 3;
			else if( gGameOptions.ubDifficultyLevel == DIF_LEVEL_INSANE )
				DiffLevel = 4;	
			else
				DiffLevel = 1;
				
				if (PlacementType == BLOODCAT_PLACEMENT_STATIC)
				{
					if (gBloodcatPlacements[ ubSectorID ][ DiffLevel-1 ].ubFactionAffiliation == QUEENS_CIV_GROUP)
					//if (gBloodcatPlacements[ ubSectorID ][ gGameOptions.ubDifficultyLevel-1 ].ubFactionAffiliation == QUEENS_CIV_GROUP)				
					{
						// skip noises between army & bloodcats
						if ( pSoldier->roster().team() == ENEMY_TEAM && noiseMaker->identity().bodyType() == BLOODCAT && noiseMaker->roster().team() == CREATURE_TEAM )
						{
							continue;
						}
						if ( pSoldier->roster().team() == CREATURE_TEAM && pSoldier->identity().bodyType() == BLOODCAT && noiseMaker->roster().team() == ENEMY_TEAM )
						{
							continue;
						}
					}				
					else if (gBloodcatPlacements[ ubSectorID ][ DiffLevel-1 ].ubFactionAffiliation > NON_CIV_GROUP)
					//else if (gBloodcatPlacements[ ubSectorID ][ gGameOptions.ubDifficultyLevel-1 ].ubFactionAffiliation > NON_CIV_GROUP)
					{
						if ( noiseMaker->identity().bodyType() == BLOODCAT && noiseMaker->roster().team() == CREATURE_TEAM && pSoldier->roster().side() != gbPlayerNum)
						{
							// Target is a bloodcat. He can't be heard by civilians no matter what.
							{
								continue;
							}
						}
						else if ( pSoldier->roster().team() == CREATURE_TEAM && pSoldier->identity().bodyType() == BLOODCAT )
						{
							// Source is a bloodcat. He can only hear player-side soldiers, and only if hostile.
							if ( noiseMaker->roster().side() != gbPlayerNum || pSoldier->aiBehavior().neutral() )
							{
								continue;
							}
						}
					}
				}
			}
			else
			{
				// screen out allied militia from hearing us
				if ( (ubNoiseMaker == NOBODY) && pSoldier->roster().team() == MILITIA_TEAM && pSoldier->roster().side() == 0 )
				{
					continue;
				}
			}

			if ( (pSoldier->roster().team() == CIV_TEAM) && (ubNoiseType == NOISE_GUNFIRE || ubNoiseType == NOISE_EXPLOSION) )
			{
				pSoldier->featureFlags().eventFlags() |= SOLDIER_MISC_HEARD_GUNSHOT;
			}

#ifdef RECORDOPPLIST
			fprintf(OpplistFile,"PN: guy %d - effVol=%d,pSoldier->tType=%d,srcTType=%d\n",
			bLoop,effVolume,pSoldier->terrtype,ubSourceTerrType);
#endif

			if (ubEffVolume > 0)
			{
				// ALL RIGHT!	Passed all the tests, this listener hears this noise!!!
				HearNoise(pSoldier,ubSource,sGridNo,bLevel,ubEffVolume,ubNoiseType, (UINT8 *)&bSeen);
				bHeard = TRUE;
				ubNoiseDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sGridNo);

				// check the 'noise heard & reported' bit for that soldier & direction
				if ( ubNoiseType != NOISE_MOVEMENT || bTeam != OUR_TEAM || (pSoldier->turnState().interruptDuelPoints() != NO_INTERRUPT) || !pSoldier->perception().hasHeardMovementFrom(ubNoiseDir) )
				{
					if (ubEffVolume > ubLoudestEffVolume)
					{
						ubLoudestEffVolume = ubEffVolume;
						ubHeardLoudestBy = pSoldier->identity().id();
						ubLoudestNoiseDir = ubNoiseDir;
					}
				}

			}
			else
			{
				//NameMessage(pSoldier," can't hear this noise",2500);
				ubEffVolume = 0;
			}
		}


		// if the noise was heard at all
		if (bHeard)
		{
			// and we're doing our team
			if (bTeam == OUR_TEAM)
			/*
			if (team == Net.pnum)
			*/
			{
				// if we are to tell the player about this type of noise
				if (bTellPlayer && ubHeardLoudestBy != NOBODY )
				{
					// the merc that heard it the LOUDEST is the one to comment
					// should add level to this function call
					TacticalActor* listener =
						GetJa2SoldierRepository().resolve(
							ubHeardLoudestBy );
					if ( listener )
						TellPlayerAboutNoise(listener,ubNoiseMaker,sGridNo,bLevel,ubLoudestEffVolume,ubNoiseType, ubLoudestNoiseDir, zNoiseMessage);

					if ( listener && ubNoiseType == NOISE_MOVEMENT)
					{
						listener->perception().rememberMovementFrom(ubNoiseDir);
					}

				}
				else if (ubNoiseType == NOISE_CREAKING)
				{
					DOOR_STATUS	*pDoorStatus = GetDoorStatus(sGridNo);
					//shadooow: show locator when there is can with string attached to doors
					if (pDoorStatus && pDoorStatus->ubFlags & DOOR_HAS_TIN_CAN)
						BeginMultiPurposeLocator(sGridNo, bLevel, (INT8)(IsJa2TacticalTurnBasedCombat()));
				}
				//if ( !(pSoldier->perception().movementNoiseDirections() & (1 << ubNoiseDir) ) )
			}
#ifdef REPORTTHEIRNOISE
			else	// debugging: report noise heard by other team's soldiers
			{
				if (bTellPlayer)
				{
					TacticalActor* listener =
						GetJa2SoldierRepository().resolve(
							ubHeardLoudestBy );
					if ( listener )
						TellPlayerAboutNoise(listener,ubNoiseMaker,sGridNo,bLevel,ubLoudestEffVolume,ubNoiseType, ubLoudestNoiseDir);
				}
			}
#endif
		}
		else if(bTeam == OUR_TEAM && ubNoiseType == NOISE_CREAKING)
		{
			//shadooow: this will indicate doors not to make animation/sound of doors opening or closing
			if ( noiseMaker != nullptr )
			{
				noiseMaker->audio().clearDoorOpeningNoise();
			}
		}
		// if the listening team is human-controlled AND
		// the noise's source is another soldier
		// (computer-controlled teams don't radio or automatically report NOISE)
		if (gTacticalStatus.Team[bTeam].bHuman && (ubSource < TOTAL_SOLDIERS))
		{
			// if ubNoiseMaker was seen by at least one member of this team
			if (bSeen)
			{
// Temporary for opplist synching - disable random order radioing
#ifdef RECORDOPPLIST
				// insure all machines radio in synch to keep logs the same
				for (bLoop = Status.team[team].guystart;
					 bLoop < Status.team[team].guyend; ++bLoop)
				{
					pSoldier =
						GetJa2SoldierRepository().resolve( bLoop );
					// if this merc is active, in this sector, and well enough to look
					if (pSoldier && pSoldier->roster().active() &&
						pSoldier->roster().inSector() &&
						(pSoldier->vitals().health() >= OKLIFE))
					{
						RadioSightings(pSoldier,ubSource,pSoldier->roster().team());
						pSoldier->awareness().clearNewOpponents();
					}
				}
#else
				// if this human team is OURS
				if (1 /* bTeam == Net.pnum */)
				{
					// this team is now allowed to report sightings and set Public flags
					OurTeamRadiosRandomlyAbout(ubSource);
				}
				else	// noise was heard by another human-controlled team (not ours)
				{
					// mark noise maker as being seen currently
					//UpdatePublic(bTeam,ubSource,SEEN_CURRENTLY,sGridNo,NOUPDATE,ACTUAL);
					UpdatePublic(bTeam,ubSource,SEEN_CURRENTLY,sGridNo,bLevel);
				}
#endif
			}
			else // not seen
			{
				if (bHeard)
				{
#ifdef RECORDOPPLIST
					fprintf(OpplistFile,"UpdatePublic (ProcessNoise/heard) for team %d about %d\n",team,ubSource);
#endif

					// mark noise maker as having been PUBLICLY heard THIS TURN
					//UpdatePublic(team,ubSource,HEARD_THIS_TURN,sGridNo,NOUPDATE,ACTUAL);
					UpdatePublic(bTeam,ubSource,HEARD_THIS_TURN,sGridNo,bLevel);
				}
			}
		}
	}

	gsWhoThrewRock = NOBODY;
}



UINT8 CalcEffVolume(TacticalActor *pSoldier, INT32 sGridNo, INT8 bLevel, UINT8 ubNoiseType, UINT8 ubBaseVolume, UINT8 ubTerrType1, UINT8 ubTerrType2)
{
	INT32 iEffVolume, iDistance;

	// Lesh: deafness
	if ( pSoldier->perception().isDeafened() )
	{
		return( 0 );
	}

	if ( FindWalkman(pSoldier) != ITEM_NOT_FOUND	)
	{
		return( 0 );
	}

	if ( IsJa2TacticalCombatActive() )
	{
		// ATE: Funny things happen to ABC stuff if bNewSituation set....
		// anv: added exception to NOISE_VOICE
		if ( GetJa2TacticalCurrentTeam() == pSoldier->roster().team() && ubNoiseType != NOISE_VOICE )
		{
			return( 0 );
		}
	}

	//sprintf(tempstr,"CalcEffVolume BY %s for gridno %d, baseVolume = %d",pSoldier->identity().name(),gridno,baseVolume);
	//PopMessage(tempstr);

	// adjust default noise volume by listener's hearing capability
	iEffVolume = (INT32) ubBaseVolume + (INT32) DecideHearing( pSoldier );


	// effective volume reduced by listener's number of opponents in sight
	iEffVolume -= pSoldier->awareness().opponentCount();


 // calculate the distance (in adjusted pixels) between the source of the
 // noise (gridno) and the location of the would-be listener (pSoldier->gridno)
 iDistance = (INT32) PythSpacesAway( pSoldier->position().gridNo(), sGridNo );
 /*
 distance = AdjPixelsAway(pSoldier->x,pSoldier->y,CenterX(sGridNo),CenterY(sGridNo));

	distance /= 15;		// divide by 15 to convert from adj. pixels to tiles
	*/
	//NumMessage("Distance = ",distance);

 // effective volume fades over distance beyond 1 tile away
 iEffVolume -= (iDistance - 1);

 //ddd{ civilians have bad hearing ;)
 if(gGameExternalOptions.bLazyCivilians)
	if (pSoldier->roster().team() == CIV_TEAM && pSoldier->identity().bodyType() != CROW )
		if (pSoldier->roster().civilianGroup() == 0 && pSoldier->identity().profile() == NO_PROFILE)
			iEffVolume =-100;
	//ddd}

	/*
	if (pSoldier->roster().team() == CIV_TEAM && pSoldier->identity().bodyType() != CROW )
	{
		if (pSoldier->roster().civilianGroup() == 0 && pSoldier->identity().profile() == NO_PROFILE)
		{
			// nameless civs reduce effective volume by 2 for gunshots etc
			// (double the reduction due to distance)
			// so that they don't cower from attacks that are really far away
			switch (ubNoiseType)
			{
				case NOISE_GUNFIRE:
				case NOISE_BULLET_IMPACT:
				case NOISE_GRENADE_IMPACT:
				case NOISE_EXPLOSION:
					iEffVolume -= iDistance;
					break;
				default:
					break;
			}
		}
		else if (pSoldier->aiBehavior().neutral())
		{
			// NPCs and people in groups ignore attack noises unless they are no longer neutral
			switch (ubNoiseType)
			{
				case NOISE_GUNFIRE:
				case NOISE_BULLET_IMPACT:
				case NOISE_GRENADE_IMPACT:
				case NOISE_EXPLOSION:
					iEffVolume = 0;
					break;
				default:
					break;
			}
		}
	}
	*/

	if (pSoldier->animationPlayback().state() == RUNNING)
	{
		iEffVolume -= 5;
	}

	//if (pSoldier->assignment().current() == SLEEPING )
	if( pSoldier->assignment().isAsleep() )
	{
		// decrease effective volume since we're asleep!
		iEffVolume -= 5;
	}

	// check for floor/roof difference
	if (bLevel > pSoldier->position().level())
	{
		// sound is amplified by roof
		iEffVolume += 5;
	}
	else if (bLevel < pSoldier->position().level())
	{
		// sound is muffled
		iEffVolume -= 5;
	}

	// if we still have a chance of hearing this, and the terrain types are known
	if (iEffVolume > 0)
	{
		// if, between noise and listener, one is outside and one is inside

		// NOTE: This is a pretty dumb way of doing things, since it won't detect
		// the presence of walls between 2 spots both inside or both outside, but
		// given our current system it's the best that we can do

		if (((ubTerrType1 == FLAT_FLOOR) && (ubTerrType2 != FLAT_FLOOR)) ||
			((ubTerrType1 != FLAT_FLOOR) && (ubTerrType2 == FLAT_FLOOR)))
		{
			//PopMessage("Sound is muffled by wall(s)");

			// sound is muffled, reduce the effective volume of the noise
			iEffVolume -= 5;
		}
	}

	//NumMessage("effVolume = ",ubEffVolume);
	if (iEffVolume > 0)
	{
		return( (UINT8) iEffVolume );
	}
	else
	{
		return( 0 );
	}
}




void HearNoise(TacticalActor *pSoldier, SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel,
 UINT8 ubVolume, UINT8 ubNoiseType, UINT8 *ubSeen)
{
	INT16		sNoiseX, sNoiseY;
	INT8		bHadToTurn = FALSE, bSourceSeen = FALSE;
	INT8		bOldOpplist;
	INT8		bDirection;
	BOOLEAN fMuzzleFlash = FALSE;
	TacticalActor* noiseMaker =
		GetJa2SoldierRepository().resolve( ubNoiseMaker );

	if ( pSoldier->identity().bodyType() == CROW )
	{
		CrowsFlyAway( pSoldier->roster().team() );
		return;
	}
//	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "%d hears noise from %d (%d/%d) volume %d", pSoldier->identity().id(), ubNoiseMaker, sGridNo, bLevel, ubVolume ) );

	// sevenfm: scale ubVolume to remember noise
	UINT8 ubVolumeScaled = (UINT8)sqrt((float)ubVolume);

	// "Turn head" towards the source of the noise and try to see what's there

	// don't use DistanceVisible here, but use maximum visibility distance
	// in as straight line instead.	Represents guy "turning just his head"

	// CJC 97/10: CHANGE!	Since STRAIGHT can not reliably be used as a
	// max sighting distance (varies based on realtime/turnbased), call
	// the function with the new DIRECTION_IRRELEVANT define

	// is he close enough to see that gridno if he turns his head?

	// ignore muzzle flashes when turning head to see noise
	if ( ubNoiseType == NOISE_GUNFIRE && noiseMaker != nullptr &&
		noiseMaker->renderState().muzzleFlashVisible() )
	{
		ConvertGridNoToCenterCellXY(sGridNo, &sNoiseX, &sNoiseY);
		bDirection = atan8(pSoldier->position().worldXInt(),pSoldier->position().worldYInt(),sNoiseX,sNoiseY);
		if ( pSoldier->position().direction() != bDirection && pSoldier->position().direction() != gOneCDirection[ bDirection ] && pSoldier->position().direction() != gOneCCDirection[ bDirection ] )
		{
			// temporarily turn off muzzle flash so DistanceVisible can be calculated without it
			noiseMaker->renderState().hideMuzzleFlash();
			fMuzzleFlash = TRUE;
		}
	}

    int sDistVisible = pSoldier->GetMaxDistanceVisible(sGridNo, bLevel, CALC_FROM_WANTED_DIR );

	if ( fMuzzleFlash )
	{
		// turn flash on again
		noiseMaker->renderState().showMuzzleFlash();
	}

	if (PythSpacesAway(pSoldier->position().gridNo(),sGridNo) <= sDistVisible )
	{
		// just use the XXadjustedXX center of the gridno
		ConvertGridNoToCenterCellXY(sGridNo, &sNoiseX, &sNoiseY);

		if (pSoldier->position().direction() != atan8(pSoldier->position().worldXInt(),pSoldier->position().worldYInt(),sNoiseX,sNoiseY))
		{
			bHadToTurn = TRUE;
		}
		else
		{
			bHadToTurn = FALSE;
		}

		// and we can trace a line of sight to his x,y coordinates?
		// (taking into account we are definitely aware of this guy now)

		// skip LOS check if we had to turn and we're a tank.	sorry Mr Tank, no looking out of the sides for you!
		if ( !(bHadToTurn && ARMED_VEHICLE( pSoldier )) )
		{
			if ( SoldierTo3DLocationLineOfSightTest( pSoldier, sGridNo, bLevel, 0, TRUE, sDistVisible ) )
			{
				// he can actually see the spot where the noise came from!
				bSourceSeen = TRUE;

				// if this sounds like a door opening/closing (could also be a crate)
				// Flugente: unused check
				/*if (ubNoiseType == NOISE_CREAKING)
				{
					// then look around and update ALL doors that have secretly changed
					//LookForDoors(pSoldier,AWARE);
				}*/
			}
		}

#ifdef RECORDOPPLIST
		fprintf(OpplistFile,"HN: %s by %2d(g%4d,x%3d,y%3d) at %2d(g%4d,x%3d,y%3d), hTT=%d\n",
			(bSourceSeen) ? "SCS" : "FLR",
			pSoldier->guynum,pSoldier->position().gridNo(),pSoldier->position().worldXInt(),pSoldier->position().worldYInt(),
			ubNoiseMaker.i,sGridNo,sNoiseX,sNoiseY,
			bHadToTurn);
#endif
	}

	// if noise is made by a person
	if (noiseMaker != nullptr)
	{
		bOldOpplist = pSoldier->awareness().opponentKnowledge()[ubNoiseMaker];

		// WE ALREADY KNOW THAT HE'S ON ANOTHER TEAM, AND HE'S NOT BEING SEEN
		// ProcessNoise() ALREADY DID THAT WORK FOR US

		if (bSourceSeen)
		{
			ManSeesMan( pSoldier, noiseMaker,
				noiseMaker->position().gridNo(),
				noiseMaker->position().level(), HEARNOISE, CALLER_UNKNOWN );

			// if it's an AI soldier, he is not allowed to automatically radio any
			// noise heard, but manSeesMan has set his newOppCnt, so clear it here
			if (!(pSoldier->status().flags() & SOLDIER_PC))
			{
				pSoldier->awareness().clearNewOpponents();
			}

			*ubSeen = TRUE;
			// RadioSightings() must only be called later on by ProcessNoise() itself
			// because we want the soldier who heard noise the LOUDEST to report it

			if ( pSoldier->aiBehavior().neutral() )
			{
				// could be a civilian watching us shoot at an enemy
				if (((ubNoiseType == NOISE_GUNFIRE) || (ubNoiseType == NOISE_BULLET_IMPACT)) && (ubVolume >= 3))
				{
					// if status is only GREEN or YELLOW
					if (pSoldier->aiBehavior().alertStatus() < STATUS_RED)
					{
						// then this soldier goes to status RED, has proof of enemy presence
						pSoldier->aiBehavior().alertStatus() = STATUS_RED;
						CheckForChangingOrders(pSoldier);
					}
				}
			}
			// sevenfm: remember noise even for seen opponents
			if( pSoldier->status().flags() & SOLDIER_PC && ubVolumeScaled > pSoldier->perception().noiseVolume() )
			{
				// yes it is, so remember this noise INSTEAD (old noise is forgotten)
				pSoldier->perception().noiseGrid() = sGridNo;
				pSoldier->perception().heardNoiseLevel() = bLevel;

				// no matter how loud noise was, don't remember it for more than 12 turns!
				pSoldier->perception().noiseVolume() = min(ubVolumeScaled, MAX_MISC_NOISE_DURATION);
			}

		}
		else		 // noise maker still can't be seen
		{
			SetNewSituation( pSoldier ); // re-evaluate situation

			// if noise type was unmistakably that of gunfire
			if (((ubNoiseType == NOISE_GUNFIRE) || (ubNoiseType == NOISE_BULLET_IMPACT)) && (ubVolume >= 3))
			{
				// if status is only GREEN or YELLOW
				if (pSoldier->aiBehavior().alertStatus() < STATUS_RED)
				{
					// then this soldier goes to status RED, has proof of enemy presence
					pSoldier->aiBehavior().alertStatus() = STATUS_RED;
					CheckForChangingOrders(pSoldier);
				}
			}

			// remember that the soldier has been heard and his new location
			UpdatePersonal( pSoldier, ubNoiseMaker, HEARD_THIS_TURN, sGridNo, bLevel );

			// sevenfm: increment watched location when soldier hears enemy
			if ((ubNoiseType == NOISE_GUNFIRE || ubNoiseType == NOISE_MOVEMENT || ubNoiseType == NOISE_SCREAM || ubNoiseType == NOISE_VOICE) &&
				!TileIsOutOfBounds(sGridNo) &&
				!(pSoldier->status().flags() & SOLDIER_PC) &&
				!pSoldier->aiBehavior().neutral() &&
				!noiseMaker->aiBehavior().neutral() &&
				!TacticalActorAiBehavior::isFlanking(*pSoldier))
			{
				// check that we can see enemy if we raise weapon
				gbForceWeaponReady = TRUE;
				if (SoldierToVirtualSoldierLineOfSightTest(pSoldier, sGridNo, bLevel, ANIM_STAND, TRUE, CALC_FROM_ALL_DIRS))
					IncrementWatchedLoc(pSoldier->identity().id(), sGridNo, bLevel);
				gbForceWeaponReady = FALSE;
			}

			// Public info is not set unless EVERYONE on the team fails to see the
			// ubnoisemaker, leaving the 'seen' flag FALSE.	See ProcessNoise().

			// CJC: set the noise gridno for the soldier, if appropriate - this is what is looked at by the AI!
			if (ubVolumeScaled >= pSoldier->perception().noiseVolume())
			{
				// yes it is, so remember this noise INSTEAD (old noise is forgotten)
				pSoldier->perception().noiseGrid() = sGridNo;
				pSoldier->perception().heardNoiseLevel() = bLevel;

				// no matter how loud noise was, don't remember it for more than 12 turns!
				pSoldier->perception().noiseVolume() = min(ubVolumeScaled, MAX_MISC_NOISE_DURATION);

				SetNewSituation( pSoldier );	// force a fresh AI decision to be made
			}

		}

		if ( pSoldier->aiBehavior().flags() & AI_ASLEEP )
		{
			switch( ubNoiseType )
			{
				case NOISE_BULLET_IMPACT:
				case NOISE_GUNFIRE:
				case NOISE_EXPLOSION:
				case NOISE_SCREAM:
				case NOISE_WINDOW_SMASHING:
				case NOISE_DOOR_SMASHING:
					// WAKE UP!
					pSoldier->aiBehavior().flags() &= (~AI_ASLEEP);
					break;
				default:
					break;
			}
		}

		// FIRST REQUIRE MUTUAL HOSTILES!
		if (!CONSIDERED_NEUTRAL( noiseMaker, pSoldier ) &&
			!CONSIDERED_NEUTRAL( pSoldier, noiseMaker ) &&
			(pSoldier->roster().side() != noiseMaker->roster().side()))
		{
			// regardless of whether the noisemaker (who's not NOBODY) was seen or not,
			// as long as listener meets minimum interrupt conditions
			if ( gfDelayResolvingBestSightingDueToDoor)
			{
				if ( bSourceSeen && (!( IsJa2TacticalTurnBasedCombat() ) || (gubSightFlags & SIGHTINTERRUPT && StandardInterruptConditionsMet(pSoldier, ubNoiseMaker, bOldOpplist)) ) )
				{
					// we should be adding this to the array for the AllTeamLookForAll to handle
					// since this is a door opening noise, add a bonus equal to half the door volume
					UINT8	ubPoints;

					ubPoints = CalcInterruptDuelPts( pSoldier, ubNoiseMaker, TRUE );
					if ( ubPoints != NO_INTERRUPT )
					{
						// require the enemy not to be dying if we are the sighter; in other words,
						// always add for AI guys, and always add for people with life >= OKLIFE
						if ( pSoldier->roster().team() != gbPlayerNum ||
							noiseMaker->vitals().health() >= OKLIFE )
						{
							ReevaluateBestSightingPosition( pSoldier, (UINT8) (ubPoints + (ubVolume / 2)) );
						}
					}
				}
			}
			else
			{
				if ( IsJa2TacticalTurnBasedCombat() )
				{
					if ( StandardInterruptConditionsMet( pSoldier, ubNoiseMaker, bOldOpplist ) )
					{
						// he gets a chance to interrupt the noisemaker
						pSoldier->turnState().interruptDuelPoints() = CalcInterruptDuelPts( pSoldier, ubNoiseMaker, TRUE );
						DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Calculating int duel pts in noise code, %d has %d points", pSoldier->identity().id(), pSoldier->turnState().interruptDuelPoints() ) );
					}
					else
					{
						pSoldier->turnState().interruptDuelPoints() = NO_INTERRUPT;
					}
				}
				else if ( bSourceSeen )
				{
					// seen source, in realtime, so check for sighting stuff
					HandleBestSightingPositionInRealtime();
				}
			}

		}
	}
	else	// noise made by NOBODY
	{
		// if noise type was unmistakably that of an explosion (seen or not) or alarm
		if (!(pSoldier->status().flags() & SOLDIER_PC))
		{
			if ( ( ubNoiseType == NOISE_EXPLOSION || ubNoiseType == NOISE_SILENT_ALARM ) && (ubVolume >= 3) )
			{
				if ( ubNoiseType == NOISE_SILENT_ALARM )
				{
					WearGasMaskIfAvailable( pSoldier );
				}
				// if status is only GREEN or YELLOW
				if (pSoldier->aiBehavior().alertStatus() < STATUS_RED)
				{
					// then this soldier goes to status RED, has proof of enemy presence
					pSoldier->aiBehavior().alertStatus() = STATUS_RED;
					CheckForChangingOrders(pSoldier);
				}
			}
		}
		// if the source of the noise can't be seen,
		// OR if it's a rock and the listener had to turn so that by the time he
		// looked all his saw was a bunch of rocks lying still
		if (!bSourceSeen || ((ubNoiseType == NOISE_ROCK_IMPACT) && (bHadToTurn) ) || ubNoiseType == NOISE_SILENT_ALARM )
		{
			// check if the effective volume of this new noise is greater than or at
			// least equal to the volume of the currently noticed noise stored
			if (ubVolumeScaled >= pSoldier->perception().noiseVolume())
			{
				// yes it is, so remember this noise INSTEAD (old noise is forgotten)
				pSoldier->perception().noiseGrid() = sGridNo;
				pSoldier->perception().heardNoiseLevel() = bLevel;

				// no matter how loud noise was, don't remember it for more than 12 turns!
				pSoldier->perception().noiseVolume() = min(ubVolumeScaled, MAX_MISC_NOISE_DURATION);

				SetNewSituation( pSoldier );	// force a fresh AI decision to be made
			}
		}
		else
		// if listener sees the source of the noise, AND it's either a grenade,
		//	or it's a rock that he watched land (didn't need to turn)
		{
			// sevenfm: allow player mercs to hear all noises
			if ( pSoldier->status().flags() & SOLDIER_PC && ubVolumeScaled > pSoldier->perception().noiseVolume())
			{
				// yes it is, so remember this noise INSTEAD (old noise is forgotten)
				pSoldier->perception().noiseGrid() = sGridNo;
				pSoldier->perception().heardNoiseLevel() = bLevel;

				// no matter how loud noise was, don't remember it for more than 12 turns!
				pSoldier->perception().noiseVolume() = min(ubVolumeScaled, MAX_MISC_NOISE_DURATION);
			}

			SetNewSituation( pSoldier );	// re-evaluate situation

			// if status is only GREEN or YELLOW
			if (pSoldier->aiBehavior().alertStatus() < STATUS_RED)
			{
				// then this soldier goes to status RED, has proof of enemy presence
				pSoldier->aiBehavior().alertStatus() = STATUS_RED;
				CheckForChangingOrders(pSoldier);
			}
		}

		if ( gubBestToMakeSightingSize == BEST_SIGHTING_ARRAY_SIZE_INCOMBAT )
		{
			// if the noise heard was the fall of a rock
			if (IsJa2TacticalTurnBasedCombat() && ubNoiseType == NOISE_ROCK_IMPACT )
			{
				// give every ELIGIBLE listener an automatic interrupt, since it's
				// reasonable to assume the guy throwing wants to wait for their reaction!
				if (StandardInterruptConditionsMet(pSoldier,NOBODY,FALSE))
				{
					pSoldier->turnState().interruptDuelPoints() = AUTOMATIC_INTERRUPT;			// force automatic interrupt
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Calculating int duel pts in noise code, %d has %d points", pSoldier->identity().id(), pSoldier->turnState().interruptDuelPoints() ) );
				}
				else
				{
					pSoldier->turnState().interruptDuelPoints() = NO_INTERRUPT;
				}
			}
		}
	}
    AI::tactical::AIInputData ai_input(AI::tactical::AIInputData::Auditive(), ubNoiseMaker, sGridNo, bLevel, ubVolume, ubNoiseType);
    AI::tactical::PlanInputData plan_input((IsJa2TacticalTurnBased())!=0, gTacticalStatus);
    AI::tactical::PlanFactoryLibrary* plan_lib(AI::tactical::PlanFactoryLibrary::instance());
    plan_lib->update_plan(pSoldier->aiPlanning().planIndex(), pSoldier, ai_input);
}

void TellPlayerAboutNoise( TacticalActor *pSoldier, SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel, UINT8 ubVolume, UINT8 ubNoiseType, UINT8 ubNoiseDir, STR16 zNoiseMessage )
{
	UINT8 ubVolumeIndex;
	TacticalActor* noiseMaker =
		GetJa2SoldierRepository().resolve( ubNoiseMaker );

	// CJC: tweaked the noise categories upwards a bit because our movement noises can be louder now.
	if (ubVolume < 4)
	{
		ubVolumeIndex = 0;		// 1-3: faint noise
	}
	else if (ubVolume < 8)	// 4-7: definite noise
	{
		ubVolumeIndex = 1;
	}
	else if (ubVolume < 12)	// 8-11: loud noise
	{
		ubVolumeIndex = 2;
	}
	else										// 12+: very loud noise
	{
		ubVolumeIndex = 3;
	}

	// display a message about a noise...
	// e.g. Sidney hears a loud splash from/to? the north.

	if ( noiseMaker != nullptr && pSoldier->roster().team() == gbPlayerNum &&
		pSoldier->roster().team() == noiseMaker->roster().team() )
	{
		#ifdef JA2BETAVERSION
			ScreenMsg( MSG_FONT_RED, MSG_ERROR, L"ERROR! TAKE SCREEN CAPTURE AND TELL CAMFIELD NOW!" );
			ScreenMsg( MSG_FONT_RED, MSG_ERROR, L"%s (%d) heard noise from %s (%d), noise at %dL%d, type %d", pSoldier->identity().name(), pSoldier->identity().id(), noiseMaker->identity().name(), ubNoiseMaker, sGridNo, bLevel, ubNoiseType );
		#endif
	}

	// anv: special treatment of NOISE_VOICE - also display taunt message
	if (ubNoiseType == NOISE_VOICE && noiseMaker != nullptr)
	{
		// information about direction etc. only displayed if we don't see noise maker
		// sevenfm: don't show noise messages from militia (if on our side)
		if (gbPublicOpplist[gbPlayerNum][ubNoiseMaker] != SEEN_CURRENTLY &&	pSoldier->awareness().opponentKnowledge()[ubNoiseMaker] != SEEN_CURRENTLY &&
			!(noiseMaker->roster().team() == MILITIA_TEAM && noiseMaker->roster().side() == 0))
		//if( gbPublicOpplist[gbPlayerNum][ubNoiseMaker] != SEEN_CURRENTLY && pSoldier->awareness().opponentKnowledge()[ubNoiseMaker] != SEEN_CURRENTLY )
		{
			if (bLevel == pSoldier->position().level())
			{
				ScreenMsg(MSG_FONT_YELLOW, MSG_INTERFACE, pNewNoiseStr[ubNoiseType], pSoldier->identity().name(), pNoiseVolStr[ubVolumeIndex], pDirectionStr[ubNoiseDir]);
			}
			else if (bLevel > pSoldier->position().level())
			{
				// from above!
				ScreenMsg(MSG_FONT_YELLOW, MSG_INTERFACE, pNewNoiseStr[ubNoiseType], pSoldier->identity().name(), pNoiseVolStr[ubVolumeIndex], gzLateLocalizedString[6]);
			}
			else
			{
				// from below!
				ScreenMsg(MSG_FONT_YELLOW, MSG_INTERFACE, pNewNoiseStr[ubNoiseType], pSoldier->identity().name(), pNoiseVolStr[ubVolumeIndex], gzLateLocalizedString[7]);
			}
		}

		if( ubVolumeIndex > 0 ) // definite noise - we're able to recognize words
		{
			CHAR8 filename[1024];

			if (gTauntsSettings.fTauntVoiceShowInfo)
				ScreenMsg(FONT_ORANGE, MSG_INTERFACE, zNoiseMessage);

			// convert wchar to char			
			wcstombs(filename, zNoiseMessage, wcslen(zNoiseMessage) + 1);
			// sevenfm: play voice taunt (check that noise string is a filename)
			if (gTauntsSettings.fTauntVoice	 &&
				strlen(filename) != 0 &&
				strstr(filename, "VoiceTaunts\\") != NULL)
			{
				UINT32 playResult = PlayJA2SampleFromFile(filename, RATE_11025, SoundVolume(HIGHVOLUME, pSoldier->position().gridNo()), 1, SoundDir(pSoldier->position().gridNo()));
				if (playResult == SOUND_ERROR && gTauntsSettings.fTauntVoiceShowInfo)
					ScreenMsg(FONT_MCOLOR_LTRED, MSG_INTERFACE, L"Noise: Failed to play taunt");
			}
			else
			{
				// do we know who said that?
				if (gbPublicOpplist[gbPlayerNum][ubNoiseMaker] == SEEN_CURRENTLY || pSoldier->awareness().opponentKnowledge()[ubNoiseMaker] == SEEN_CURRENTLY)
				{
					if (gTauntsSettings.fTauntShowPopupBox == TRUE)
						ShowTauntPopupBox(noiseMaker, zNoiseMessage);
					if (gTauntsSettings.fTauntShowInLog == TRUE)
						ScreenMsg(FONT_GRAY2, MSG_INTERFACE, L"%s: %s", noiseMaker->GetName(), zNoiseMessage);
				}
				else
				{
					if (gTauntsSettings.fTauntShowPopupBox == TRUE && gTauntsSettings.fTauntShowPopupBoxIfHeard == TRUE)
						ShowTauntPopupBox(noiseMaker, zNoiseMessage);
					if (gTauntsSettings.fTauntShowInLog == TRUE && gTauntsSettings.fTauntShowInLogIfHeard == TRUE)
						ScreenMsg(FONT_GRAY2, MSG_INTERFACE, L"%s: %s", pTauntUnknownVoice[0], zNoiseMessage);
				}
			}			
		}
	}
	else if ( bLevel == pSoldier->position().level() || ubNoiseType == NOISE_EXPLOSION || ubNoiseType == NOISE_SCREAM || ubNoiseType == NOISE_ROCK_IMPACT || ubNoiseType == NOISE_GRENADE_IMPACT )
	{
		ScreenMsg( MSG_FONT_YELLOW, MSG_INTERFACE, pNewNoiseStr[ubNoiseType], pSoldier->identity().name(), pNoiseVolStr[ubVolumeIndex], pDirectionStr[ubNoiseDir] );
	}
	else if ( bLevel > pSoldier->position().level() )
	{
		// from above!
		ScreenMsg( MSG_FONT_YELLOW, MSG_INTERFACE, pNewNoiseStr[ubNoiseType], pSoldier->identity().name(), pNoiseVolStr[ubVolumeIndex], gzLateLocalizedString[6] );
	}
	else
	{
		// from below!
		ScreenMsg( MSG_FONT_YELLOW, MSG_INTERFACE, pNewNoiseStr[ubNoiseType], pSoldier->identity().name(), pNoiseVolStr[ubVolumeIndex], gzLateLocalizedString[7] );
	}

	// if the quote was faint, say something
	if (ubVolumeIndex == 0)
	{
		if ( ( GetGameContext().capabilities().isUnfinishedBusiness() ||
		       !AreInMeanwhile( ) ) &&
		     !( gTacticalStatus.uiFlags & ENGAGED_IN_CONV) &&
		     pSoldier->dialogue().heardNoiseCooldownTurns() == 0)
		{
			TacticalCharacterDialogue( pSoldier, QUOTE_HEARD_SOMETHING );
			if ( IsJa2TacticalCombatActive() )
			{
				pSoldier->dialogue().startHeardNoiseCooldown(2);
			}
			else
			{
				pSoldier->dialogue().startHeardNoiseCooldown(5);
			}
		}
	}


	//DIGICRAB: Loud Sound Locator
	//show a locator for very loud noises if we have an extended ear
	if(ubVolumeIndex >= 2)
	{
		if ( FindHearingAid(pSoldier) )
			BeginMultiPurposeLocator(sGridNo, bLevel, (INT8)(IsJa2TacticalTurnBasedCombat()));
	}

	// flag soldier as having reported noise in a particular direction
}

void VerifyAndDecayOpplist(TacticalActor *pSoldier)
{
	UINT32 uiLoop;
	INT8 *pPersOL;			// pointer into soldier's opponent list
	TacticalActor *pOpponent;

	// reduce all seen/known opponent's turn counters by 1 (towards 0)
	// 1) verify accuracy of the opplist by testing sight vs known opponents
	// 2) increment opplist value if opponent is known but not currenly seen
	// 3) forget about known opponents who haven't been noticed in some time

	// if soldier is unconscious, make sure his opplist is wiped out & bail out
	if (pSoldier->vitals().health() < OKLIFE)
	{
		memset(pSoldier->awareness().opponentKnowledge(),NOT_HEARD_OR_SEEN,sizeof(pSoldier->awareness().opponentKnowledge()));
		pSoldier->awareness().opponentCount() = 0;
		return;
	}

	// if any new opponents were seen earlier and not yet radioed
	if (pSoldier->awareness().newOpponentCount())
	{
#ifdef BETAVERSION
		tempstr = String("VerifyAndDecayOpplist: WARNING - %d(%s) still has %d NEW OPPONENTS - lastCaller %s/%s",
			pSoldier->guynum,ExtMen[pSoldier->guynum].name,pSoldier->newOppCnt,
			LastCallerText[ExtMen[pSoldier->guynum].lastCaller],
			LastCaller2Text[ExtMen[pSoldier->guynum].lastCaller2]);

#ifdef TESTVERSION	// make this ERROR/BETA again when it's fixed!
		PopMessage(tempstr);
#endif

#ifdef RECORDNET
		fprintf(NetDebugFile,"\n\t%s\n\n",tempstr);
#endif

#endif

		if (pSoldier->status().flags() & SOLDIER_PC)
		{
			RadioSightings(pSoldier,EVERYBODY,pSoldier->roster().team());
		}

		pSoldier->awareness().clearNewOpponents();
	}

	// man looks for each of his opponents WHO ARE ALREADY KNOWN TO HIM
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is active, here, and alive
		if (pOpponent != NULL && pOpponent->vitals().health())
		{
			// if this merc is on the same team, he's no opponent, so skip him
			if (pSoldier->roster().team() == pOpponent->roster().team())
			{
				continue;
			}

		pPersOL = pSoldier->awareness().opponentKnowledge() + pOpponent->identity().id();

	 // if this opponent is "known" in any way (seen or heard recently)
	 if (*pPersOL != NOT_HEARD_OR_SEEN)
		{
		// use both sides actual x,y co-ordinates (neither side's moving)
		ManLooksForMan(pSoldier,pOpponent,VERIFYANDDECAYOPPLIST);

			// decay opplist value if necessary
			DECAY_OPPLIST_VALUE( *pPersOL );
			/*
		// if opponent was SEEN recently but is NOT visible right now
		if (*pPersOL >= SEEN_THIS_TURN)
		{
		 (*pPersOL)++;			// increment #turns it's been since last seen

		 // if it's now been longer than the maximum we care to remember
		 if (*pPersOL > SEEN_2_TURNS_AGO)
			*pPersOL = 0;		// forget that we knew this guy
		}
		else
		{
		 // if opponent was merely HEARD recently, not actually seen
		 if (*pPersOL <= HEARD_THIS_TURN)
			{
			(*pPersOL)--;		// increment #turns it's been since last heard

	// if it's now been longer than the maximum we care to remember
	if (*pPersOL < HEARD_2_TURNS_AGO)
		*pPersOL = 0;		// forget that we knew this guy
			}
				}
			*/
		}

	}
	}


 // if any new opponents were seen
 if (pSoldier->awareness().newOpponentCount())
	{
	// turns out this is NOT an error!	If this guy was gassed last time he
	// looked, his sight limit was 2 tiles, and now he may no longer be gassed
	// and thus he sees opponents much further away for the first time!
	// - Always happens if you STUNGRENADE an opponent by surprise...
#ifdef RECORDNET
	fprintf(NetDebugFile,"\tVerifyAndDecayOpplist: d(%s) saw %d new opponents\n",
		pSoldier->guynum,ExtMen[pSoldier->guynum].name,pSoldier->newOppCnt);
#endif

	if (pSoldier->status().flags() & SOLDIER_PC)
	 RadioSightings(pSoldier,EVERYBODY,pSoldier->roster().team());

	pSoldier->awareness().clearNewOpponents();
	}
}

void DecayIndividualOpplist(TacticalActor *pSoldier)
{
	UINT32 uiLoop;
	INT8 *pPersOL;			// pointer into soldier's opponent list
	TacticalActor *pOpponent;

	// reduce all currently seen opponent's turn counters by 1 (towards 0)

	// if soldier is unconscious, make sure his opplist is wiped out & bail out
	if (pSoldier->vitals().health() < OKLIFE)
	{
		// must make sure that public opplist is kept to match...
		for (uiLoop = 0; uiLoop < MAX_NUM_SOLDIERS; ++uiLoop)
		{
			if ( pSoldier->awareness().opponentKnowledge()[ uiLoop ] == SEEN_CURRENTLY )
			{
				TacticalActor* opponent =
					GetJa2SoldierRepository().resolve( uiLoop );
				if ( opponent )
					HandleManNoLongerSeen( pSoldier, opponent, &(pSoldier->awareness().opponentKnowledge()[ uiLoop ]), &(gbPublicOpplist[ pSoldier->roster().team() ][ uiLoop ]) );
			}
		}
	//void HandleManNoLongerSeen( TacticalActor * pSoldier, TacticalActor * pOpponent, INT8 * pPersOL, INT8 * pbPublOL )

		memset(pSoldier->awareness().opponentKnowledge(),NOT_HEARD_OR_SEEN,sizeof(pSoldier->awareness().opponentKnowledge()));
		pSoldier->awareness().opponentCount() = 0;
		return;
	}

	// man looks for each of his opponents WHO IS CURRENTLY SEEN
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); ++uiLoop)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is active, here, and alive
		if (pOpponent != NULL && pOpponent->vitals().health())
		{
			// if this merc is on the same team, he's no opponent, so skip him
			if (pSoldier->roster().team() == pOpponent->roster().team())
			{
				continue;
			}

			pPersOL = pSoldier->awareness().opponentKnowledge() + pOpponent->identity().id();

			 // if this opponent is seen currently
			 if (*pPersOL == SEEN_CURRENTLY)
			{
				// they are NOT visible now!
				(*pPersOL)++;
				if (!CONSIDERED_NEUTRAL( pOpponent, pSoldier ) && !CONSIDERED_NEUTRAL( pSoldier, pOpponent ) && (pSoldier->roster().side() != pOpponent->roster().side()) && TacticalActorCovertOps::recognizesCombatant(*pSoldier, pOpponent->identity().id()) )
				{
					RemoveOneOpponent(pSoldier);
				}
			}
		}
	}
}



void VerifyPublicOpplistDueToDeath(TacticalActor *pSoldier)
{
	UINT32 uiLoop,uiTeamMateLoop;
	INT8 *pPersOL,*pMatePersOL;	// pointers into soldier's opponent list
	TacticalActor *pOpponent,*pTeamMate;
	BOOLEAN bOpponentStillSeen;


	// OK, someone died. Anyone that the deceased ALONE saw has to decay
	// immediately in the Public Opplist.


	// If deceased didn't see ANYONE, don't bother
	if (pSoldier->awareness().opponentCount() == 0)
	{
		return;
	}


	// Deceased looks for each of his opponents who is "seen currently"
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		// first, initialize flag since this will be a "new" opponent
		bOpponentStillSeen = FALSE;

		// grab a pointer to the "opponent"
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this opponent is active, here, and alive
		if (pOpponent != NULL && pOpponent->vitals().health())
		{
			// if this opponent is on the same team, he's no opponent, so skip him
			if (pSoldier->roster().team() == pOpponent->roster().team())
			{
				continue;
			}

			// point to what the deceased's personal opplist value is
			pPersOL = pSoldier->awareness().opponentKnowledge() + pOpponent->identity().id();

			// if this opponent was CURRENTLY SEEN by the deceased (before his
			// untimely demise)
			if (*pPersOL == SEEN_CURRENTLY)
			{
				// then we need to know if any teammates ALSO see this opponent, so loop through
				// trying to find ONE witness to the death...
				for (uiTeamMateLoop = 0; uiTeamMateLoop < Ja2ActiveTacticalActorSlotCount(); uiTeamMateLoop++)
				{
					// grab a pointer to the potential teammate
					pTeamMate = ResolveJa2ActiveTacticalActorSlot(uiTeamMateLoop);

					// if this teammate is active, here, and alive
					if (pTeamMate != NULL && pTeamMate->vitals().health())
					{
						// if this opponent is NOT on the same team, then skip him
						if (pTeamMate->roster().team() != pSoldier->roster().team())
						{
							continue;
						}

						// point to what the teammate's personal opplist value is
						pMatePersOL = pTeamMate->awareness().opponentKnowledge() + pOpponent->identity().id();

						// test to see if this value is "seen currently"
						if (*pMatePersOL == SEEN_CURRENTLY)
						{
							// this opponent HAS been verified!
							bOpponentStillSeen = TRUE;

							// we can stop looking for other witnesses now
							break;
						}
					}
				}
			}

			// if no witnesses for this opponent, then decay the Public Opplist
			if ( !bOpponentStillSeen )
			{
				DECAY_OPPLIST_VALUE( gbPublicOpplist[pSoldier->roster().team()][pOpponent->identity().id()] );
			}
		}
	}
}


void DecayPublicOpplist(INT8 bTeam)
{
	UINT32 uiLoop;
	INT8 bNoPubliclyKnownOpponents = TRUE;
	TacticalActor *pSoldier;
	INT8 *pbPublOL;


	//NumMessage("Decay for team #",team);

	// decay the team's public noise volume, forget public noise gridno if <= 0
	// used to be -1 per turn but that's not fast enough!
	if (gubPublicNoiseVolume[bTeam] > 0)
	{
		if ( IsJa2TacticalCombatActive() )
		{
			gubPublicNoiseVolume[bTeam] = (UINT8) ( (UINT32) (gubPublicNoiseVolume[bTeam] * 7) / 10 );
		}
		else
		{
			gubPublicNoiseVolume[bTeam] = gubPublicNoiseVolume[bTeam] / 2;
		}

		if (gubPublicNoiseVolume[bTeam] <= 0)
		{
			gsPublicNoiseGridNo[bTeam] = NOWHERE;
		}
	}

	// decay the team's Public Opplist
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pSoldier = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// for every active, living soldier on ANOTHER team
		if (pSoldier && pSoldier->vitals().health() && (pSoldier->roster().team() != bTeam))
		{
			// hang a pointer to the byte holding team's public opplist for this merc
			pbPublOL = &gbPublicOpplist[bTeam][pSoldier->identity().id()];

			if (*pbPublOL == NOT_HEARD_OR_SEEN)
			{
				continue;
			}

			// well, that make this a "publicly known opponent", so nuke that flag
			bNoPubliclyKnownOpponents = FALSE;

			// if this person has been SEEN recently, but is not currently visible
			if (*pbPublOL >= SEEN_THIS_TURN)
			{
				(*pbPublOL)++;		// increment how long it's been
			}
			else
			{
				// if this person has been only HEARD recently
				if (*pbPublOL <= HEARD_THIS_TURN)
				{
					(*pbPublOL)--;	// increment how long it's been
				}
			}

			// if it's been longer than the maximum we care to remember
			if ((*pbPublOL > OLDEST_SEEN_VALUE) || (*pbPublOL < OLDEST_HEARD_VALUE))
			{
#ifdef RECORDOPPLIST
				fprintf(OpplistFile,"UpdatePublic (DecayPublicOpplist) for team %d about %d\n",team,pSoldier->guynum);
#endif

				// forget about him,
				// and also forget where he was last seen (it's been too long)
				// this is mainly so POINT_PATROL guys don't SEEK_OPPONENTs forever
				UpdatePublic(bTeam,pSoldier->identity().id(),NOT_HEARD_OR_SEEN,NOWHERE,0);
			}
		}
	}

	// if all opponents are publicly unknown (NOT_HEARD_OR_SEEN)
	if (bNoPubliclyKnownOpponents)
	{
		// forget about the last radio alert (ie. throw away who made the call)
		// this is mainly so POINT_PATROL guys don't SEEK_FRIEND forever after
		gTacticalStatus.Team[bTeam].ubLastMercToRadio = NOBODY;
	}

	// decay watched locs as well
	DecayWatchedLocs( bTeam );
}

// bit of a misnomer; this is now decay all opplists
void NonCombatDecayPublicOpplist( UINT32 uiTime )
{
	UINT32	cnt;

	if ( uiTime - gTacticalStatus.uiTimeSinceLastOpplistDecay >= TIME_BETWEEN_RT_OPPLIST_DECAYS)
	{
		// decay!
		for ( cnt = 0; cnt < Ja2ActiveTacticalActorSlotCount(); cnt++ )
		{
			TacticalActor* soldier =
				ResolveJa2ActiveTacticalActorSlot(cnt);
			if ( soldier )
			{
				VerifyAndDecayOpplist(soldier);
			}
		}


		for( cnt = 0; cnt < MAXTEAMS; cnt++ )
		{
			if ( gTacticalStatus.Team[ cnt ].bMenInSector > 0 )
			{
				// decay team's public opplist
				DecayPublicOpplist( (INT8)cnt );
			}
		}
		// update time
		gTacticalStatus.uiTimeSinceLastOpplistDecay = uiTime;
	}
}

void RecalculateOppCntsDueToNoLongerNeutral( TacticalActor * pSoldier )
{
	UINT32					uiLoop;
	TacticalActor *		pOpponent;

	pSoldier->awareness().opponentCount() = 0;

	if (!pSoldier->aiBehavior().neutral())
	{
		for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
		{
			pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

			// for every active, living soldier on ANOTHER team
			if (pOpponent && pOpponent->vitals().health() && !pOpponent->aiBehavior().neutral() && (pOpponent->roster().team() != pSoldier->roster().team()) && (!CONSIDERED_NEUTRAL( pOpponent, pSoldier ) && !CONSIDERED_NEUTRAL( pSoldier, pOpponent ) && (pSoldier->roster().side() != pOpponent->roster().side())) )
			{
				if ( pSoldier->awareness().opponentKnowledge()[pOpponent->identity().id()] == SEEN_CURRENTLY )
				{
					AddOneOpponent( pSoldier );
				}
				if ( pOpponent->awareness().opponentKnowledge()[pSoldier->identity().id()] == SEEN_CURRENTLY )
				{
					// WANNE: Chris, I reverted BUGZILLA #338 fix, because this leads to big problems on opponent count with enemies!
					// have to add to opponent's oppcount as well since we just became non-neutral
					//CHRISL: If we do this, Bloodcats get counted at opponents multiple times and never get removed from the OppCnt variable.
					AddOneOpponent( pOpponent );
				}
			}
		}
	}
}

void RecalculateOppCntsDueToBecomingNeutral( TacticalActor * pSoldier )
{
	UINT32					uiLoop;
	TacticalActor *		pOpponent;

	if (pSoldier->aiBehavior().neutral())
	{
		pSoldier->awareness().opponentCount() = 0;

		for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
		{
			pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

			// for every active, living soldier on ANOTHER team
			if (pOpponent && pOpponent->vitals().health() && !pOpponent->aiBehavior().neutral() && (pOpponent->roster().team() != pSoldier->roster().team()) && !CONSIDERED_NEUTRAL( pSoldier, pOpponent ) && (pSoldier->roster().side() != pOpponent->roster().side()) && TacticalActorCovertOps::recognizesCombatant(*pSoldier, pOpponent->identity().id()) )
			{
				if ( pOpponent->awareness().opponentKnowledge()[pSoldier->identity().id()] == SEEN_CURRENTLY )
				{
					// have to rem from opponent's oppcount as well since we just became neutral
					RemoveOneOpponent( pOpponent );
				}
			}
		}
	}
}

void NoticeUnseenAttacker( TacticalActor * pAttacker, TacticalActor * pDefender, INT8 bReason )
{
	INT8		bOldOppList;
	BOOLEAN fSeesAttacker = FALSE;
	INT8		bDirection;
	BOOLEAN	fMuzzleFlash = FALSE;

	if (!(IsJa2TacticalCombatActive()))
	{
		return;
	}

	if (AmmoTypes[pAttacker->inventory()[pAttacker->attackSelection().hand()][0]->data.gun.ubGunAmmoType].dart)
	{
		// rarely noticed
		if (SkillCheck(pDefender, NOTICE_DART_CHECK, 0) < 0)
		{
			return;
		}
	}

	// do we need to do checks for life/breath here?
	if (pDefender->identity().bodyType() == LARVAE_MONSTER || (pDefender->status().flags() & SOLDIER_VEHICLE && pDefender->roster().team() == OUR_TEAM))
	{
		return;
	}

	bOldOppList = pDefender->awareness().opponentKnowledge()[ pAttacker->identity().id() ];
	// check LOS, considering we are now aware of the attacker
	// ignore muzzle flashes when must turning head
	if ( pAttacker->renderState().muzzleFlashVisible() )
	{
		bDirection = atan8( pDefender->position().worldXInt(),pDefender->position().worldYInt(), pAttacker->position().worldXInt(), pAttacker->position().worldYInt() );
		if ( pDefender->position().direction() != bDirection && pDefender->position().direction() != gOneCDirection[ bDirection ] && pDefender->position().direction() != gOneCCDirection[ bDirection ] )
		{
			// temporarily turn off muzzle flash so DistanceVisible can be calculated without it
			pAttacker->renderState().hideMuzzleFlash();
			fMuzzleFlash = TRUE;
		}
	}

	if (SoldierToSoldierLineOfSightTest( pDefender, pAttacker, TRUE, CALC_FROM_WANTED_DIR ) != 0)
	{
		fSeesAttacker = TRUE;
	}
	if ( fMuzzleFlash )
	{
		pAttacker->renderState().showMuzzleFlash();
	}

	if (fSeesAttacker)
	{
		ManSeesMan( pDefender, pAttacker, pAttacker->position().gridNo(), pAttacker->position().level(), NOTICEUNSEENATTACKER, CALLER_UNKNOWN );

		// newOppCnt not needed here (no radioing), must get reset right away
		// CJC: Huh? well, leave it in for now
		pDefender->awareness().clearNewOpponents();


		if (pDefender->roster().team() == gbPlayerNum)
		{
			// EXPERIENCE GAIN (5): Victim notices/sees a previously UNSEEN attacker
			StatChange( pDefender, EXPERAMT, 5, FALSE );

			// mark attacker as being SEEN right now
			RadioSightings( pDefender, pAttacker->identity().id(), pDefender->roster().team() );

		}
		// NOTE: ENEMIES DON'T REPORT A SIGHTING PUBLICLY UNTIL THEY RADIO IT IN!
		else
		{
			// go to threatening stance
			ReevaluateEnemyStance( pDefender, pDefender->animationPlayback().state() );
		}
	}
	else	// victim NOTICED the attack, but CAN'T SEE the actual attacker
	{
		SetNewSituation( pDefender );			// re-evaluate situation

		// if victim's alert status is only GREEN or YELLOW
		if (pDefender->aiBehavior().alertStatus() < STATUS_RED)
		{
			// then this soldier goes to status RED, has proof of enemy presence
			pDefender->aiBehavior().alertStatus() = STATUS_RED;
			CheckForChangingOrders( pDefender );
		}

		UpdatePersonal( pDefender, pAttacker->identity().id(), HEARD_THIS_TURN, pAttacker->position().gridNo(), pAttacker->position().level() );

		// if the victim is a human-controlled soldier, instantly report publicly
		if (pDefender->status().flags() & SOLDIER_PC)
		{
			// mark attacker as having been PUBLICLY heard THIS TURN & remember where
			UpdatePublic( pDefender->roster().team(), pAttacker->identity().id(), HEARD_THIS_TURN, pAttacker->position().gridNo(), pAttacker->position().level() );
		}
	}

	if ( !UsingImprovedInterruptSystem() || gGameExternalOptions.fAllowInstantInterruptsOnSight )
	{
		if ( StandardInterruptConditionsMet( pDefender, pAttacker->identity().id(), bOldOppList ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("INTERRUPT: NoticeUnseenAttacker, standard conditions are met; defender %d, attacker %d", pDefender->identity().id(), pAttacker->identity().id() ) );

			// calculate the interrupt duel points
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Calculating int duel pts for defender in NUA" );
			pDefender->turnState().interruptDuelPoints() = CalcInterruptDuelPts( pDefender, pAttacker->identity().id(), FALSE);
		}
		else
		{
			pDefender->turnState().interruptDuelPoints() = NO_INTERRUPT;
		}

		// say quote

		if (pDefender->turnState().interruptDuelPoints() != NO_INTERRUPT)
		{
			// check for possible interrupt and handle control change if it happens
			// this code is basically ResolveInterruptsVs for 1 man only...

			// calculate active soldier's dueling pts for the upcoming interrupt duel
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Calculating int duel pts for attacker in NUA" );
			pAttacker->turnState().interruptDuelPoints() = CalcInterruptDuelPts( pAttacker, pDefender->identity().id(), FALSE );
			if ( InterruptDuel( pDefender, pAttacker ) )
			{
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("INTERRUPT: NoticeUnseenAttacker, defender pts %d, attacker pts %d, defender gets interrupt", pDefender->turnState().interruptDuelPoints(), pAttacker->turnState().interruptDuelPoints() ) );
				AddToIntList( pAttacker->identity().id(), FALSE, TRUE);
				AddToIntList( pDefender->identity().id(), TRUE, TRUE);
				DoneAddingToIntList( pDefender, TRUE, SIGHTINTERRUPT );
			}
			// either way, clear out both sides' duelPts fields to prepare next duel
			pDefender->turnState().interruptDuelPoints() = NO_INTERRUPT;
			#ifdef DEBUG_INTERRUPTS
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("Resetting int pts for %d in NUA", pDefender->identity().id() ) );
			#endif
			pAttacker->turnState().interruptDuelPoints() = NO_INTERRUPT;
			#ifdef DEBUG_INTERRUPTS
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("Resetting int pts for %d in NUA", pAttacker->identity().id() ) );
			#endif

		}
	}
}

void CheckForAlertWhenEnemyDies( TacticalActor * pDyingSoldier )
{
	SoldierID ubID;
	TacticalActor * pSoldier;
	INT8 bDir;
	INT16 sDistAway, sDistVisible;

	for ( ubID = gTacticalStatus.Team[ pDyingSoldier->roster().team() ].bFirstID; ubID <= gTacticalStatus.Team[ pDyingSoldier->roster().team() ].bLastID; ++ubID )
	{

		pSoldier = GetJa2SoldierRepository().resolve( ubID );
		if ( pSoldier == nullptr )
		{
			continue;
		}

		if ( pSoldier->roster().active() && pSoldier->roster().inSector() && (pSoldier != pDyingSoldier) && (pSoldier->vitals().health() >= OKLIFE) && (pSoldier->aiBehavior().alertStatus() < STATUS_RED ) )
		{
			// this guy might have seen the man die

			// distance we "see" then depends on the direction he is located from us
			bDir = atan8(pSoldier->position().worldXInt(),pSoldier->position().worldYInt(),pDyingSoldier->position().worldXInt(),pDyingSoldier->position().worldYInt());
			sDistVisible = DistanceVisible( pSoldier, pSoldier->pathing().desiredDirection(), bDir, pDyingSoldier->position().gridNo(), pDyingSoldier->position().level(), TacticalActorConditions::isCowering(*pSoldier), GetPercentTunnelVision(pSoldier));
			sDistAway = PythSpacesAway( pSoldier->position().gridNo(), pDyingSoldier->position().gridNo() );

			// if we see close enough to see the soldier
			if (sDistAway <= sDistVisible)
			{
				// and we can trace a line of sight to his x,y coordinates
				// assume enemies are always aware of their buddies...
				if ( SoldierTo3DLocationLineOfSightTest( pSoldier, pDyingSoldier->position().gridNo(), pDyingSoldier->position().level(), 0, TRUE, sDistVisible ) )
				{
					pSoldier->aiBehavior().alertStatus() = STATUS_RED;
					CheckForChangingOrders( pSoldier );
				}
			}
		}
	}
}

BOOLEAN ArmyKnowsOfPlayersPresence( void )
{
	SoldierID ubID;
	TacticalActor * pSoldier;

	// if anyone is still left...
	if (gTacticalStatus.Team[ ENEMY_TEAM ].bTeamActive && gTacticalStatus.Team[ ENEMY_TEAM ].bMenInSector > 0 )
	{
		for ( ubID = gTacticalStatus.Team[ ENEMY_TEAM ].bFirstID; ubID <= gTacticalStatus.Team[ ENEMY_TEAM ].bLastID; ++ubID )
		{
			pSoldier = GetJa2SoldierRepository().resolve( ubID );
			if ( pSoldier == nullptr )
			{
				continue;
			}

			if ( pSoldier->roster().active() && pSoldier->roster().inSector() && (pSoldier->vitals().health() >= OKLIFE) && (pSoldier->aiBehavior().alertStatus() >= STATUS_RED ) )
			{
				return( TRUE );
			}
		}
	}
	return( FALSE );
}

BOOLEAN MercSeesCreature( TacticalActor * pSoldier )
{
	SoldierID ubID;

	if ( pSoldier->awareness().opponentCount() > 0 )
	{
		for ( ubID = gTacticalStatus.Team[CREATURE_TEAM].bFirstID; ubID <= gTacticalStatus.Team[CREATURE_TEAM].bLastID; ++ubID )
		{
			const TacticalActor* creature =
				GetJa2SoldierRepository().resolve( ubID );
			if ( creature != nullptr &&
				pSoldier->awareness().opponentKnowledge()[ubID] == SEEN_CURRENTLY &&
				(creature->status().flags() & SOLDIER_MONSTER) )
			{
				return(TRUE);
			}
		}
	}
	return(FALSE);
}


INT8 FindUnusedWatchedLoc( UINT16 ubID )
{
	INT8 bLoop;

	for ( bLoop = 0; bLoop < NUM_WATCHED_LOCS; bLoop++ )
	{
		// WANNE: I think this was a bug, should be != NOWHERE!
		// sevenfm: the original code is ok, as we search for unused WatchedLoc which is initialized with NOWHERE value
		if ( gsWatchedLoc[ ubID ][ bLoop ] == NOWHERE )
		//if (!TileIsOutOfBounds(gsWatchedLoc[ ubID ][ bLoop ]))
		{
			return( bLoop );
		}
	}
	return( -1 );
}

INT8 FindWatchedLocWithLessThanXPointsLeft( UINT16 ubID, UINT8 ubPointLimit )
{
	INT8 bLoop;

	for ( bLoop = 0; bLoop < NUM_WATCHED_LOCS; bLoop++ )
	{		
		if (!TileIsOutOfBounds(gsWatchedLoc[ ubID ][ bLoop ]) && gubWatchedLocPoints[ ubID ][ bLoop ] <= ubPointLimit )
		{
			return( bLoop );
		}
	}
	return( -1 );
}

INT8 FindWatchedLoc( UINT16 ubID, INT32 sGridNo, INT8 bLevel )
{
	INT8	bLoop;

	for ( bLoop = 0; bLoop < NUM_WATCHED_LOCS; bLoop++ )
	{		
		if (!TileIsOutOfBounds(gsWatchedLoc[ ubID ][ bLoop ]) &&	gbWatchedLocLevel[ ubID ][ bLoop ] == bLevel )
		{
			if ( SpacesAway( gsWatchedLoc[ ubID ][ bLoop ], sGridNo ) <= WATCHED_LOC_RADIUS )
			{
				return( bLoop );
			}
		}
	}
	return( -1 );
}

INT8 GetWatchedLocPoints( UINT16 ubID, INT32 sGridNo, INT8 bLevel )
{
	INT8	bLoc;

	bLoc = FindWatchedLoc( ubID, sGridNo, bLevel );
	if (bLoc != -1)
	{
		#ifdef JA2BETAVERSION
			/*
			if (gubWatchedLocPoints[ ubID ][ bLoc ] > 1)
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_BETAVERSION, L"Soldier %d getting %d points for interrupt in watched location", ubID, gubWatchedLocPoints[ ubID ][ bLoc ] - 1 );
			}
			*/
		#endif
		// one loc point is worth nothing, so return number minus 1

		// experiment with 1 loc point being worth 1 point
		return( gubWatchedLocPoints[ ubID ][ bLoc ] );
	}

	return( 0 );
}


INT8 GetHighestVisibleWatchedLoc( UINT16 ubID )
{
	INT8	bLoop;
	INT8	bHighestLoc = -1;
	INT8	bHighestPoints = 0;
	TacticalActor* soldier =
		GetJa2SoldierRepository().resolve( ubID );
	if ( soldier == nullptr )
		return bHighestLoc;

	for ( bLoop = 0; bLoop < NUM_WATCHED_LOCS; bLoop++ )
	{		
		if (!TileIsOutOfBounds(gsWatchedLoc[ ubID ][ bLoop ]) && gubWatchedLocPoints[ ubID ][ bLoop ] > bHighestPoints )
		{
			// look at standing height
			if ( SoldierTo3DLocationLineOfSightTest( soldier, gsWatchedLoc[ ubID ][ bLoop ], gbWatchedLocLevel[ ubID ][ bLoop ], 3, TRUE, CALC_FROM_WANTED_DIR ) )
			{
				bHighestLoc = bLoop;
				bHighestPoints = gubWatchedLocPoints[ ubID ][ bLoop ];
			}
		}
	}
	return( bHighestLoc );
}

INT8 GetHighestWatchedLocPoints( UINT16 ubID )
{
	INT8	bLoop;
	INT8	bHighestPoints = 0;

	for ( bLoop = 0; bLoop < NUM_WATCHED_LOCS; bLoop++ )
	{		
		if (!TileIsOutOfBounds(gsWatchedLoc[ ubID ][ bLoop ]) && gubWatchedLocPoints[ ubID ][ bLoop ] > bHighestPoints )
		{
			bHighestPoints = gubWatchedLocPoints[ ubID ][ bLoop ];
		}
	}
	return( bHighestPoints );
}


void CommunicateWatchedLoc( SoldierID ubID, INT32 sGridNo, INT8 bLevel, UINT8 ubPoints )
{
	SoldierID ubLoop;
	INT8 bTeam, bLoopPoint, bPoint;

	const TacticalActor* source =
		GetJa2SoldierRepository().resolve( ubID );
	if ( source == nullptr )
	{
		return;
	}
	bTeam = source->roster().team();

	for ( ubLoop = gTacticalStatus.Team[ bTeam ].bFirstID; ubLoop <= gTacticalStatus.Team[ bTeam ].bLastID; ++ubLoop )
	{
		TacticalActor *pSoldier =
			GetJa2SoldierRepository().resolve( ubLoop );
		if ( pSoldier == nullptr )
		{
			continue;
		}

		if ( ubLoop == ubID || pSoldier->roster().active() == FALSE || pSoldier->roster().inSector() == FALSE || pSoldier->vitals().health() < OKLIFE )
		{
			continue;
		}
		bLoopPoint = FindWatchedLoc( ubLoop, sGridNo, bLevel );
		if ( bLoopPoint == -1 )
		{
			// add this as a watched point
			bPoint = FindUnusedWatchedLoc( ubLoop );
			if (bPoint == -1)
			{
				// if we have a point with only 1 point left, replace it
				bPoint = FindWatchedLocWithLessThanXPointsLeft( ubLoop, ubPoints );
			}
			if (bPoint != -1)
			{
				gsWatchedLoc[ ubLoop ][ bPoint ] = sGridNo;
				gbWatchedLocLevel[ ubLoop ][ bPoint ] = bLevel;
				gubWatchedLocPoints[ ubLoop ][ bPoint ] = ubPoints;
				gfWatchedLocReset[ ubLoop ][ bPoint ] = FALSE;
				gfWatchedLocHasBeenIncremented[ ubLoop ][ bPoint ] = TRUE;
			}
			// else no points available!
		}
		else
		{
			// increment to max
			gubWatchedLocPoints[ ubLoop ][ bLoopPoint ] = __max( gubWatchedLocPoints[ ubLoop ][ bLoopPoint ], ubPoints );

			gfWatchedLocReset[ ubLoop ][ bLoopPoint ] = FALSE;
			gfWatchedLocHasBeenIncremented[ ubLoop ][ bLoopPoint ] = TRUE;
		}
	}
}


void IncrementWatchedLoc( UINT16 ubID, INT32 sGridNo, INT8 bLevel )
{
	INT8 bPoint;

	bPoint = FindWatchedLoc( ubID, sGridNo, bLevel );
	if (bPoint == -1)
	{
		// try adding point
		bPoint = FindUnusedWatchedLoc( ubID );
		if (bPoint == -1)
		{
			// if we have a point with only 1 point left, replace it
			bPoint = FindWatchedLocWithLessThanXPointsLeft( ubID, 1 );
		}

		if (bPoint != -1)
		{
			gsWatchedLoc[ ubID ][ bPoint ] = sGridNo;
			gbWatchedLocLevel[ ubID ][ bPoint ] = bLevel;
			gubWatchedLocPoints[ ubID ][ bPoint ] = 1;
			gfWatchedLocReset[ ubID ][ bPoint ] = FALSE;
			gfWatchedLocHasBeenIncremented[ ubID ][ bPoint ] = TRUE;

			CommunicateWatchedLoc( ubID, sGridNo, bLevel, 1 );
		}
		// otherwise abort; no points available
	}
	else
	{
		if ( !gfWatchedLocHasBeenIncremented[ ubID ][ bPoint ] && gubWatchedLocPoints[ ubID ][ bPoint ] < MAX_WATCHED_LOC_POINTS )
		{
			gubWatchedLocPoints[ ubID ][ bPoint ]++;
			CommunicateWatchedLoc( ubID, sGridNo, bLevel, gubWatchedLocPoints[ ubID ][ bPoint ] );
		}
		gfWatchedLocReset[ ubID ][ bPoint ] = FALSE;
		gfWatchedLocHasBeenIncremented[ ubID ][ bPoint ] = TRUE;
	}
}

void SetWatchedLocAsUsed( UINT16 ubID, INT32 sGridNo, INT8 bLevel )
{
	INT8	bPoint;

	bPoint = FindWatchedLoc( ubID, sGridNo, bLevel );
	if (bPoint != -1)
	{
		gfWatchedLocReset[ ubID ][ bPoint ] = FALSE;
	}
}

BOOLEAN WatchedLocLocationIsEmpty( INT32 sGridNo, INT8 bLevel, INT8 bTeam )
{
	// look to see if there is anyone near the watched loc who is not on this team
	SoldierID	ubID;
	INT32		sTempGridNo;
	INT16		sX, sY;

	for ( sY = -WATCHED_LOC_RADIUS; sY <= WATCHED_LOC_RADIUS; sY++ )
	{
		for ( sX = -WATCHED_LOC_RADIUS; sX <= WATCHED_LOC_RADIUS; sX++ )
		{
			sTempGridNo = sGridNo + sX + sY * WORLD_ROWS;
			if ( sTempGridNo < 0 || sTempGridNo >= WORLD_MAX )
			{
				continue;
			}
			ubID = WhoIsThere2( sTempGridNo, bLevel );
			const TacticalActor* occupant =
				GetJa2SoldierRepository().resolve( ubID );
			if ( occupant != nullptr && occupant->roster().team() != bTeam )
			{
				return( FALSE );
			}
		}
	}
	return( TRUE );
}

void DecayWatchedLocs( INT8 bTeam )
{
	UINT16 cnt, cnt2;

	// loop through all soldiers
	for ( cnt = gTacticalStatus.Team[ bTeam ].bFirstID; cnt <= gTacticalStatus.Team[ bTeam ].bLastID; cnt++ )
	{
		// for each watched location
		for ( cnt2 = 0; cnt2 < NUM_WATCHED_LOCS; cnt2++ )
		{			
			if (!TileIsOutOfBounds(gsWatchedLoc[ cnt ][ cnt2 ]) && WatchedLocLocationIsEmpty( gsWatchedLoc[ cnt ][ cnt2 ], gbWatchedLocLevel[ cnt ][ cnt2 ], bTeam ) )
			{
				// if the reset flag is still set, then we should decay this point
				if (gfWatchedLocReset[ cnt ][ cnt2 ])
				{
					// turn flag off again
					gfWatchedLocReset[ cnt ][ cnt2 ] = FALSE;

					// halve points
					gubWatchedLocPoints[ cnt ][ cnt2 ] /= 2;
					// if points have reached 0, then reset the location
					if (gubWatchedLocPoints[ cnt ][ cnt2 ] == 0)
					{
						gsWatchedLoc[ cnt ][ cnt2 ] = NOWHERE;
					}
				}
				else
				{
					// flag was false so set to true (will be reset if new people seen there next turn)
					gfWatchedLocReset[ cnt ][ cnt2 ] = TRUE;
				}
			}
		}
	}
}

void MakeBloodcatsHostile( void )
{
	TacticalActor *pSoldier;
	SoldierID id = gTacticalStatus.Team[ CREATURE_TEAM ].bFirstID;

	for ( ; id <= gTacticalStatus.Team[ CREATURE_TEAM ].bLastID; ++id )
	{
		pSoldier = GetJa2SoldierRepository().resolve( id );
		if ( pSoldier == nullptr )
		{
			continue;
		}

		if ( pSoldier->identity().bodyType() == BLOODCAT && pSoldier->roster().active() && pSoldier->roster().inSector() && pSoldier->vitals().health() > 0 )
		{
			SetSoldierNonNeutral( pSoldier );
			RecalculateOppCntsDueToNoLongerNeutral( pSoldier );

			if ( ( IsJa2TacticalCombatActive() ) )
			{
				CheckForPotentialAddToBattleIncrement( pSoldier );
			}
		}
	}
}

BOOLEAN SoldierHasLimitedVision(TacticalActor * pSoldier)
{
	if ( gGameExternalOptions.gfAllowLimitedVision || GetPercentTunnelVision(pSoldier) > 0 )
		return TRUE;
	else
		return FALSE;
}

INT32 MaxDistanceVisible( void )
{
	return( STRAIGHT * 2 );
}
