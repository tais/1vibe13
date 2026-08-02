#ifndef _OPPLIST_H
#define _OPPLIST_H

#include "Overhead Types.h"

class TacticalActor;

#define EVERYBODY		MAXMERCS

#define MAX_MISC_NOISE_DURATION 12		// max dur for VERY loud NOBODY noises

#define DOOR_NOISE_VOLUME		2
#define WINDOW_CRACK_VOLUME			4
#define WINDOW_SMASH_VOLUME			8
#define MACHETE_VOLUME			9
#define TRIMMER_VOLUME			18
#define CHAINSAW_VOLUME		 30
#define SMASHING_DOOR_VOLUME		6
#define CROWBAR_DOOR_VOLUME			4
#define ITEM_THROWN_VOLUME			2

#define TIME_BETWEEN_RT_OPPLIST_DECAYS 20

// this is a fake "level" value (0 on ground, 1 on roof) for
// HearNoise to ignore the effects of lighting(?)
#define LIGHT_IRRELEVANT 127

#define AUTOMATIC_INTERRUPT 100
#define NO_INTERRUPT 127

#define MOVEINTERRUPT	0
#define SIGHTINTERRUPT	1
#define NOISEINTERRUPT	2

// SANDRO - enum for improved interrupt system
enum
{
	DISABLED_INTERRUPT = 0,
	UNTRIGGERED_INTERRUPT,
	UNDEFINED_INTERRUPT,
	MOVEMENT_INTERRUPT,
	SP_MOVEMENT_INTERRUPT,
	BEFORESHOT_INTERRUPT,
	AFTERSHOT_INTERRUPT,
	AFTERACTION_INTERRUPT,
	INSTANT_INTERRUPT,
	MAX_INTERRUPT_TYPES
};

// noise type constants
enum
{
	NOISE_UNKNOWN = 0,
	NOISE_MOVEMENT,
	NOISE_CREAKING,
	NOISE_SPLASHING,
	NOISE_BULLET_IMPACT,
	NOISE_GUNFIRE,
	NOISE_EXPLOSION,
	NOISE_SCREAM,
	NOISE_ROCK_IMPACT,
	NOISE_GRENADE_IMPACT,
	NOISE_WINDOW_SMASHING,
	NOISE_DOOR_SMASHING,
	NOISE_SILENT_ALARM, // only heard by enemies
	NOISE_VOICE, // anv: for enemy taunts
	MAX_NOISES
};

enum
{
	EXPECTED_NOSEND,	// other nodes expecting noise & have all info
	EXPECTED_SEND,		// other nodes expecting noise, but need info
	UNEXPECTED				// other nodes are NOT expecting this noise
};

#define NUM_WATCHED_LOCS 3

extern INT8 gbPublicOpplist[MAXTEAMS][ TOTAL_SOLDIERS ];
extern INT8 gbSeenOpponents[TOTAL_SOLDIERS][TOTAL_SOLDIERS];
extern INT32 gsLastKnownOppLoc[TOTAL_SOLDIERS][TOTAL_SOLDIERS];		// merc vs. merc
extern INT8 gbLastKnownOppLevel[TOTAL_SOLDIERS][TOTAL_SOLDIERS];
extern INT32 gsPublicLastKnownOppLoc[MAXTEAMS][TOTAL_SOLDIERS];	// team vs. merc
extern INT8 gbPublicLastKnownOppLevel[MAXTEAMS][TOTAL_SOLDIERS];
extern UINT8 gubPublicNoiseVolume[MAXTEAMS];
extern INT32 gsPublicNoiseGridNo[MAXTEAMS];
extern INT8 gbPublicNoiseLevel[MAXTEAMS];
extern UINT8 gubKnowledgeValue[10][10];
extern INT8 gfKnowAboutOpponents;

extern BOOLEAN	gfPlayerTeamSawJoey;
extern BOOLEAN	gfMikeShouldSayHi;
extern BOOLEAN   gfMorrisShouldSayHi; // JA25 UB

extern INT32			gsWatchedLoc[ TOTAL_SOLDIERS ][ NUM_WATCHED_LOCS ];
extern INT8				gbWatchedLocLevel[ TOTAL_SOLDIERS ][ NUM_WATCHED_LOCS ];
extern UINT8			gubWatchedLocPoints[ TOTAL_SOLDIERS ][ NUM_WATCHED_LOCS ];
extern BOOLEAN		gfWatchedLocReset[ TOTAL_SOLDIERS ][ NUM_WATCHED_LOCS ];
#define BEST_SIGHTING_ARRAY_SIZE 6
#define BEST_SIGHTING_ARRAY_SIZE_ALL_TEAMS_LOOK_FOR_ALL 6
#define BEST_SIGHTING_ARRAY_SIZE_NONCOMBAT 3
#define BEST_SIGHTING_ARRAY_SIZE_INCOMBAT 0
extern UINT8 gubBestToMakeSightingSize;

INT16 ManLooksForMan(TacticalActor *pSoldier, TacticalActor *pOpponent, UINT8 ubCaller);
void HandleSight(TacticalActor *pSoldier, UINT8 ubSightFlags);
void AllTeamsLookForAll(UINT8 ubAllowInterrupts);
void GloballyDecideWhoSeesWho(void);
UINT16 GetClosestMerc( UINT16 usSoldierIndex );
void ManLooksForOtherTeams(TacticalActor *pSoldier);
void OtherTeamsLookForMan(TacticalActor *pOpponent);
void ManSeesMan(TacticalActor *pSoldier, TacticalActor *pOpponent, INT32 sOppGridNo, INT8 bOppLevel, UINT8 ubCaller, UINT8 ubCaller2);
void DecideTrueVisibility(TacticalActor *pSoldier, UINT8 ubLocate);
void AddOneOpponent(TacticalActor *pSoldier);
void RemoveOneOpponent(TacticalActor *pSoldier);
void UpdatePersonal(TacticalActor *pSoldier, SoldierID ubID, INT8 bNewOpplist, INT32 sGridNo, INT8 bLevel);
void ResetLastKnownLocs(TacticalActor *ptr);
void RecalculateOppCntsDueToNoLongerNeutral( TacticalActor * pSoldier );
void ReevaluateBestSightingPosition( TacticalActor * pSoldier, INT8 bInterruptDuelPts );


void InitOpponentKnowledgeSystem(void);
void InitSoldierOppList(TacticalActor *pSoldier);
void BetweenTurnsVisibilityAdjustments(void);
void RemoveManAsTarget(TacticalActor *pSoldier);
void UpdatePublic(UINT8 ubTeam, SoldierID ubID, INT8 bNewOpplist, INT32 sGridNo, INT8 bLevel );
void RadioSightings(TacticalActor *pSoldier, UINT16 ubAbout, UINT8 ubTeamToRadioTo );
void OurTeamRadiosRandomlyAbout(UINT16 ubAbout);
void DebugSoldierPage1( );
void DebugSoldierPage2( );
void DebugSoldierPage3( );
void DebugSoldierPage4( );

UINT8 MovementNoise( TacticalActor *pSoldier );
UINT8 DoorOpeningNoise( TacticalActor *pSoldier );
void MakeNoise( SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel, UINT8 ubTerrType, UINT8 ubVolume, UINT8 ubNoiseType, STR16 zNoiseMessage = NULL );
void OurNoise( SoldierID ubNoiseMaker, INT32 sGridNo, INT8 bLevel, UINT8 ubTerrType, UINT8 ubVolume, UINT8 ubNoiseType, STR16 zNoiseMessage = NULL );

void ResolveInterruptsVs( TacticalActor * pSoldier, UINT8 ubInterruptType);

void VerifyAndDecayOpplist(TacticalActor *pSoldier);
void DecayIndividualOpplist(TacticalActor *pSoldier);
void VerifyPublicOpplistDueToDeath( TacticalActor * pSoldier );
void NoticeUnseenAttacker( TacticalActor * pAttacker, TacticalActor * pDefender, INT8 bReason );

BOOLEAN MercSeesCreature( TacticalActor * pSoldier );

INT8 GetWatchedLocPoints( UINT16 ubID, INT32 sGridNo, INT8 bLevel );
INT8 GetHighestVisibleWatchedLoc( UINT16 ubID );
INT8 GetHighestWatchedLocPoints( UINT16 ubID );

void TurnOffEveryonesMuzzleFlashes( void );
void TurnOffTeamsMuzzleFlashes( UINT8 ubTeam );
void EndMuzzleFlash( TacticalActor * pSoldier );
void NonCombatDecayPublicOpplist( UINT32 uiTime );

void CheckHostileOrSayQuoteList( void );
void InitOpplistForDoorOpening( void );
UINT8 DoorOpeningNoise( TacticalActor * pSoldier );

void AddToShouldBecomeHostileOrSayQuoteList( SoldierID ubID );

//extern INT8 gbLightSighting[1][16];

// HEADROCK HAM 3.6: Moved here from cpp
void MakeBloodcatsHostile( void );
#endif
