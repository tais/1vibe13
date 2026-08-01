#include "TacticalActorDamageResolution.h"

	#include "Overhead.h"
#include "SoldierRepository.h"
#include "TacticalWorldAdapter.h"
	#include "worldman.h"
	#include "Soldier Profile.h"
	#include "Dialogue Control.h"
	#include "End Game.h"
	#include "Intro.h"
	#include "Exit Grids.h"
	#include "strategicmap.h"
	#include "Quests.h"
	#include "SaveLoadMap.h"
	#include "Sound Control.h"
	#include "renderworld.h"
	#include "Isometric Utils.h"
	#include "Soldier macros.h"
	#include "Strategic Movement.h"
	#include "screenids.h"
	#include "TacticalEntityHost.h"
	#include "ub_config.h"
	#include "Ja25_Tactical.h"
	#include "Handle UI.h"
	#include "GameContext.h"

#include "NPC.h"
#include "Music Control.h"
#include "qarray.h"
#include "LOS.h"
#include "Strategic AI.h"
#include "Squads.h"
#include "PreBattle Interface.h"
#include "strategic.h"
#include "Queen Command.h"
#include "Strategic Town Loyalty.h"
#include "Player Command.h"
#include "Tactical Save.h"
#include "email.h"
#include "Game Clock.h"
#include "Ja25_Tactical.h"
#include "Game Init.h"
#include "interface Dialogue.h"
#include "Handle UI.h"

void HandleAddingTheEndGameEmails();
void EndFadeToCredits( void );
void FadeToCredits( void );
void InFinalSectorAfterFadeIn( void );
void FadeOutToLaptopOnEndGame( void );

BOOLEAN			gfPlayersLaptopWasntWorkingAtEndOfGame;

//forward declarations of common classes to eliminate includes
class OBJECTTYPE;
class TacticalActor;

//victory ending scene sector
#define VICTORY_X	gModSettings.ubEndGameVictorySectorX
#define VICTORY_Y	gModSettings.ubEndGameVictorySectorY

INT32 sStatueGridNos[] = { 13829, 13830, 13669, 13670 };

namespace
{
struct EndGameDeathCallbackContext
{
	Ja2TacticalEntityReference killer;
	INT32 grid = NOWHERE;
	INT8 level = 0;

	void capture(
		TacticalEntityId selectedKiller,
		INT32 selectedGrid,
		INT8 selectedLevel) noexcept
	{
		reset();
		if (selectedKiller.valid())
			(void)killer.capture(selectedKiller);
		grid = selectedGrid;
		level = selectedLevel;
	}

	void reset() noexcept
	{
		killer.reset();
		grid = NOWHERE;
		level = 0;
	}
};

EndGameDeathCallbackContext gEndGameDeathCallback;
}


// This function checks if our statue exists in the current sector at given gridno
BOOLEAN DoesO3SectorStatueExistHere( INT32 sGridNo )
{
	INT32 cnt;
	EXITGRID								ExitGrid;

	// First check current sector......
	if ( gWorldSectorX == 3 && gWorldSectorY == MAP_ROW_O && gbWorldSectorZ == 0 )
	{
		// Check for exitence of and exit grid here...
		// ( if it doesn't then the change has already taken place )
		if ( !GetExitGrid( 13669, &ExitGrid ) )
		{
			for ( cnt = 0; cnt < 4; cnt++ )
			{
				if ( sStatueGridNos[ cnt ] == sGridNo )
				{
					return( TRUE );
				}
			}
		}
	}

	return( FALSE );
}

// This function changes the graphic of the statue and adds the exit grid...
void ChangeO3SectorStatue( BOOLEAN fFromExplosion )
{
	EXITGRID								ExitGrid;
	UINT16									usTileIndex;
	INT16 sX, sY;

	// Remove old graphic
	ApplyMapChangesToMapTempFile( TRUE );
	// Remove it!
	// Get index for it...
	GetTileIndexFromTypeSubIndex( EIGHTOSTRUCT, (INT8)( 5 ), &usTileIndex );
	RemoveStruct( 13830, usTileIndex );

	// Add new one...
	if ( fFromExplosion )
	{
		// Use damaged peice
		GetTileIndexFromTypeSubIndex( EIGHTOSTRUCT, (INT8)( 7 ), &usTileIndex );
	}
	else
	{
		GetTileIndexFromTypeSubIndex( EIGHTOSTRUCT, (INT8)( 8 ), &usTileIndex );
		// Play sound...

	PlayJA2Sample( OPEN_STATUE, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );

	}
	AddStructToHead( 13830, usTileIndex );

	// Add exit grid
	ExitGrid.ubGotoSectorX = 3;
	ExitGrid.ubGotoSectorY = MAP_ROW_O;
	ExitGrid.ubGotoSectorZ = 1;
	ExitGrid.usGridNo = 13037;	 //dnl!!!

	AddExitGridToWorld( 13669, &ExitGrid );
	gpWorldLevelData[ 13669 ].uiFlags |= MAPELEMENT_REVEALED;

	// Turn off permenant changes....
	ApplyMapChangesToMapTempFile( FALSE );

	// Re-render the world!
	gTacticalStatus.uiFlags |= NOHIDE_REDUNDENCY;
	// FOR THE NEXT RENDER LOOP, RE-EVALUATE REDUNDENT TILES
	InvalidateWorldRedundency( );
	SetRenderFlags(RENDER_FLAG_FULL);

	// Redo movement costs....
	ConvertGridNoToXY( 13830, &sX, &sY );

	RecompileLocalMovementCostsFromRadius( 13830, 5 );

}
static void DeidrannaTimerCallback( void )
{
	TacticalActor* killer = gEndGameDeathCallback.killer.resolve();
	const INT32 grid = gEndGameDeathCallback.grid;
	const INT8 level = gEndGameDeathCallback.level;
	gEndGameDeathCallback.reset();
	HandleDeidrannaDeath( killer, grid, level );
}


void BeginHandleDeidrannaDeath( TacticalActor *pKillerSoldier, INT32 sGridNo, INT8 bLevel )
{
	gEndGameDeathCallback.capture(
		pKillerSoldier
			? GetJa2TacticalEntityId(*pKillerSoldier)
			: TacticalEntityId{},
		sGridNo, bLevel);

	// Lock the UI.....
	gTacticalStatus.uiFlags |= ENGAGED_IN_CONV;
	// Increment refrence count...
	giNPCReferenceCount = 1;

	gTacticalStatus.uiFlags |= IN_DEIDRANNA_ENDGAME;

	SetCustomizableTimerCallbackAndDelay( 2000, DeidrannaTimerCallback, FALSE );

}

void HandleDeidrannaDeath( TacticalActor *pKillerSoldier, INT32 sGridNo, INT8 bLevel )
{
	TacticalActor *pTeamSoldier;
	SoldierID ubKillerSoldierID = NOBODY;

	// Start victory music here...
	SetMusicMode( MUSIC_TACTICAL_VICTORY );
	
	if ( pKillerSoldier )
	{
		TacticalCharacterDialogue( pKillerSoldier, QUOTE_KILLING_DEIDRANNA );
		ubKillerSoldierID = pKillerSoldier->identity().id();
	}

	// STEP 1 ) START ALL QUOTES GOING!
	// OK - loop through all witnesses and see if they want to say something abou this...
	SoldierID cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;

	// run through list
	for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt.i);

		if ( cnt != ubKillerSoldierID )
		{
			if ( OK_INSECTOR_MERC( pTeamSoldier ) && !( pTeamSoldier->status().flags() & SOLDIER_GASSED ) && !AM_AN_EPC( pTeamSoldier ) )
			{
				if ( QuoteExp[ pTeamSoldier->identity().profile() ].QuoteExpWitnessDeidrannaDeath )
				{
					if ( SoldierTo3DLocationLineOfSightTest( pTeamSoldier, sGridNo,  bLevel, 3, TRUE, CALC_FROM_ALL_DIRS ) )
					{
						TacticalCharacterDialogue( pTeamSoldier, QUOTE_KILLING_DEIDRANNA );
					}
				}
			}
		}
	}

	// Set fact that she is dead!
	SetFactTrue( FACT_QUEEN_DEAD );

	ExecuteStrategicAIAction( STRATEGIC_AI_ACTION_QUEEN_DEAD, 0, 0 );

	// AFTER LAST ONE IS DONE - PUT SPECIAL EVENT ON QUEUE TO BEGIN FADE< ETC
	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_MULTIPURPOSE, JA2_MULTIPURPOSE_EVENT_DONE_KILLING_DEIDRANNA, 0,0,0,0 );
}

static void DoneFadeInKilledQueen( void )
{
	TacticalActor *pNPCSoldier;

	// Locate gridno.....

	// Run NPC script
	// EXPECT pilot: lookup-then-deref guard. FindSoldierByProfileID can legitimately
	// return NULL (NPC 136 not present); EXPECT logs that edge (file:line) and returns
	// gracefully -- the same recovery the hand-written guard did, now diagnosable.
	pNPCSoldier = FindSoldierByProfileID( 136, FALSE );
	EXPECT( pNPCSoldier );

	// Converse!
	//InitiateConversation( pNPCSoldier, pSoldier, 0, 1 );
	TriggerNPCRecordImmediately( pNPCSoldier->identity().profile(), 6 );

}

static void DoneFadeOutKilledQueen( void )
{
	SoldierID cnt;
	TacticalActor *pSoldier, *pTeamSoldier;

	// For one, loop through our current squad and move them over
	cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;

	// look for all mercs on the same team,
	for (; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
	{
		pSoldier = GetJa2SoldierRepository().resolve(cnt.i);
		// Are we in this sector, On the current squad?
		if ( pSoldier->roster().active() && pSoldier->vitals().health() >= OKLIFE && pSoldier->roster().inSector() && pSoldier->assignment().current() == CurrentSquad( ) )
		{
			gfTacticalTraversal = TRUE;
			SetGroupSectorValue( VICTORY_X, VICTORY_Y, 0, pSoldier->deployment().groupId() );

			// Set next sectore
			pSoldier->deployment().sectorX() = VICTORY_X;
			pSoldier->deployment().sectorY() = VICTORY_Y;
			pSoldier->deployment().sectorZ() = 0;

			// Set gridno
			pSoldier->deployment().strategicInsertionCode() = INSERTION_CODE_GRIDNO;
			pSoldier->deployment().strategicInsertionData() = gModSettings.iEndGameVictoryGridNo; //5687 dnl!!!
			// Set direction to face....
			pSoldier->deployment().insertionDirection()		= 100 + NORTHWEST;
		}
	}

	// Kill all enemies in world.....
	cnt = gTacticalStatus.Team[ ENEMY_TEAM ].bFirstID;

	// look for all mercs on the same team,
	for ( ; cnt <= gTacticalStatus.Team[ ENEMY_TEAM ].bLastID; ++cnt )
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt.i);
		// Are we active and in sector.....
		if ( pTeamSoldier->roster().active()	)
		{
			// For sure for flag thet they are dead is not set
			// Check for any more badguys
			// ON THE STRAGETY LAYER KILL BAD GUYS!
			if ( !pTeamSoldier->aiBehavior().neutral() && (pTeamSoldier->roster().side() != gbPlayerNum ) )
			{
				ProcessQueenCmdImplicationsOfDeath( pTeamSoldier );
			}
		}
	}

	// 'End' battle
	ExitCombatMode();
	gTacticalStatus.fLastBattleWon = TRUE;
	// Set enemy presence to false
	gTacticalStatus.fEnemyInSector = FALSE;

	SetMusicMode( MUSIC_TACTICAL_VICTORY );

	HandleMoraleEvent( NULL, MORALE_QUEEN_BATTLE_WON, VICTORY_X, VICTORY_Y, 0 );
	HandleGlobalLoyaltyEvent( GLOBAL_LOYALTY_QUEEN_BATTLE_WON, VICTORY_X, VICTORY_Y, 0 );

	SetMusicMode( MUSIC_TACTICAL_VICTORY );

	SetThisSectorAsPlayerControlled( gWorldSectorX, gWorldSectorY, gbWorldSectorZ, TRUE );

	// ATE: Force change of level set z to 1 to allow reloading of same sector
	SetJa2TacticalWorldDepth(1);

	// Clear out dudes.......
	SectorInfo[ SECTOR( VICTORY_X, VICTORY_Y ) ].ubNumAdmins = 0;
	SectorInfo[ SECTOR( VICTORY_X, VICTORY_Y ) ].ubNumTroops = 0;
	SectorInfo[ SECTOR( VICTORY_X, VICTORY_Y ) ].ubNumElites = 0;
	SectorInfo[ SECTOR( VICTORY_X, VICTORY_Y ) ].ubNumTanks = 0;
	SectorInfo[ SECTOR( VICTORY_X, VICTORY_Y ) ].ubAdminsInBattle = 0;
	SectorInfo[ SECTOR( VICTORY_X, VICTORY_Y ) ].ubTroopsInBattle = 0;
	SectorInfo[ SECTOR( VICTORY_X, VICTORY_Y ) ].ubElitesInBattle = 0;
	SectorInfo[ SECTOR( VICTORY_X, VICTORY_Y ) ].ubRobotsInBattle = 0;
	SectorInfo[ SECTOR( VICTORY_X, VICTORY_Y ) ].ubTanksInBattle = 0;
	SectorInfo[ SECTOR( VICTORY_X, VICTORY_Y ) ].ubJeepsInBattle = 0;

	// ATE: banish elliot... dead or alive
	gMercProfiles[ ELLIOT ].sSectorX = 0;
	gMercProfiles[ ELLIOT ].sSectorY = 0;
	gMercProfiles[ ELLIOT ].bSectorZ = 0;

	ChangeNpcToDifferentSector( DEREK, VICTORY_X, VICTORY_Y, 0 );
	ChangeNpcToDifferentSector( OLIVER, VICTORY_X, VICTORY_Y, 0 );


	// OK, insertion data found, enter sector!
	SetCurrentWorldSector( VICTORY_X, VICTORY_Y, 0 );

	// OK, once down here, adjust the above map with crate info....
	gfTacticalTraversal = FALSE;
	ResetTacticalTraversalContext();

	gFadeInDoneCallback = DoneFadeInKilledQueen;

	FadeInGameScreen( );
}

// Called after all player quotes are done....
void HandleDoneLastKilledQueenQuote( )
{
	gFadeOutDoneCallback = DoneFadeOutKilledQueen;

	FadeOutGameScreen( );
}


void EndQueenDeathEndgameBeginEndCimenatic( )
{
	SoldierID cnt;
	TacticalActor *pSoldier;

	// Start end cimimatic....
	gTacticalStatus.uiFlags |= IN_ENDGAME_SEQUENCE;

	// first thing is to loop through team and say end quote...
	cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;

	// look for all mercs on the same team,
	for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
	{
		pSoldier = GetJa2SoldierRepository().resolve(cnt.i);
		// Are we in this sector, On the current squad?
		if ( pSoldier->roster().active() && pSoldier->vitals().health() >= OKLIFE && !AM_AN_EPC( pSoldier ) )
		{
			TacticalCharacterDialogue( pSoldier, QUOTE_END_GAME_COMMENT );
		}
	}

	// Add queue event to proceed w/ smacker cimimatic
	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_MULTIPURPOSE, JA2_MULTIPURPOSE_EVENT_TEAM_MEMBERS_DONE_TALKING, 0,0,0,0 );

}

void EndQueenDeathEndgame( )
{
	// Unset flags...
	gTacticalStatus.uiFlags &= (~ENGAGED_IN_CONV );
	// Increment refrence count...
	giNPCReferenceCount = 0;

	gTacticalStatus.uiFlags &= (~IN_DEIDRANNA_ENDGAME);
}


void DoneFadeOutEndCinematic( void )
{
	// DAVE PUT SMAKER STUFF HERE!!!!!!!!!!!!
	// :)
	gTacticalStatus.uiFlags &= (~IN_ENDGAME_SEQUENCE);


	// For now, just quit the freaken game...
//	InternalLeaveTacticalScreen( MAINMENU_SCREEN );

	InternalLeaveTacticalScreen( INTRO_SCREEN );
//	GetCurrentScreen() = INTRO_SCREEN;


	SetIntroType( INTRO_ENDING );
}
// OK, end death UI - fade to smaker....
void HandleDoneLastEndGameQuote( )
{
	if (GetGameContext().capabilities().isUnfinishedBusiness())
	{
		gFadeOutDoneCallback = DoneFadeOutJa25EndCinematic;
	}
	else
	{
		EndQueenDeathEndgame( );
		gFadeOutDoneCallback = DoneFadeOutEndCinematic;
	}

	FadeOutGameScreen( );
}



static void QueenBitchTimerCallback( void )
{
	TacticalActor* killer = gEndGameDeathCallback.killer.resolve();
	const INT32 grid = gEndGameDeathCallback.grid;
	const INT8 level = gEndGameDeathCallback.level;
	gEndGameDeathCallback.reset();
	HandleQueenBitchDeath( killer, grid, level );
}

void EndGameEveryoneSayTheirGoodByQuotes( void )
{
	INT32 cnt;
	TacticalActor *pSoldier;
	auto& soldiers = GetJa2SoldierRepository();

	// Start end cimimatic....
  gTacticalStatus.uiFlags |= IN_ENDGAME_SEQUENCE;

	//lock the interface
	guiPendingOverrideEvent = LU_BEGINUILOCK;
	HandleTacticalUI( );

	//
	// first thing is to loop through team and look for QUALIFIED mercs on the team to say special end game quote
	//
	cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
	for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
	{
		pSoldier = soldiers.resolve(cnt);
		// Are we in this sector, On the current squad?
		if( pSoldier->roster().active() && pSoldier->vitals().health() >= OKLIFE && !AM_AN_EPC( pSoldier ) && IsSoldierQualifiedMerc( pSoldier ) )
		{
			TacticalCharacterDialogue( pSoldier, QUOTE_RENEWING_CAUSE_BUDDY_2_ON_TEAM );	
		}
	}

	//
	// Next is to loop through ENTIRE team and say end quote...
	//
	cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
	for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
	{
		pSoldier = soldiers.resolve(cnt);
		// Are we in this sector, On the current squad?
		if ( pSoldier->roster().active() && pSoldier->vitals().health() >= OKLIFE && !AM_AN_EPC( pSoldier ) )
		{
			TacticalCharacterDialogue( pSoldier, QUOTE_END_GAME_COMMENT );	
		}
	}

	// Add queue event to proceed w/ smacker cimimatic
	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_MULTIPURPOSE, JA25_MULTIPURPOSE_EVENT_TEAM_MEMBERS_DONE_TALKING, 0,0,0,0 );
}

void HandleAddingTheEndGameEmails()
{
	BOOLEAN				fMiguelAlive=FALSE;
	BOOLEAN				fManuelAlive=FALSE;
	BOOLEAN				fManuelHired=FALSE;


	//Miguel alive
	if( gubFact[ FACT_PLAYER_IMPORTED_SAVE_MIGUEL_DEAD ] )
		fMiguelAlive = FALSE;
	else
		fMiguelAlive = TRUE;

	//manuel alive
	if( gMercProfiles[ MANUEL_UB ].bMercStatus == MERC_IS_DEAD )
		fManuelAlive = FALSE;
	else
		fManuelAlive = TRUE;

	//manuel hired
	if( gMercProfiles[ MANUEL_UB ].ubMiscFlags & PROFILE_MISC_FLAG_RECRUITED )
		fManuelHired = TRUE;
	else
		fManuelHired = FALSE;

	//
	// Determine the EMAIL to be sent out to the player
	//

	#ifdef JA113DEMO
	//no demo
	#else
	// email # 12a - Miguel dead, Manuel never recruited
	if( !fMiguelAlive && !fManuelHired )
	{
		AddEmail(JA25_EMAIL_CONGRATS, JA25_EMAIL_CONGRATS_LENGTH, MAIL_ENRICO, GetWorldTotalMin(), -1, -1, TYPE_EMAIL_EMAIL_EDT, XML_JA2UB_ENRICO_CONGRATS);
	}
		
	// email # 12b - Miguel alive, Manuel never recruited
	else if( fMiguelAlive && !fManuelHired )
	{
		AddEmail(JA25_EMAIL_CONGRATS_MIGUEL_SICK, JA25_EMAIL_CONGRATS_MIGUEL_SICK_LENGTH, MAIL_ENRICO, GetWorldTotalMin(), -1, -1, TYPE_EMAIL_EMAIL_EDT, XML_JA2UB_ENRICO_CONGRATS_MIGUELSICK);
	}

	// email # 12c - Miguel alive, Manuel dead
	else if( fMiguelAlive && !fManuelAlive )
	{
		AddEmail(JA25_EMAIL_CONGRATS_MIGUEL_SICK_MANUEL_DEAD, JA25_EMAIL_CONGRATS_MIGUEL_SICK_MANUEL_DEAD_LENGTH, MAIL_ENRICO, GetWorldTotalMin(), -1, -1, TYPE_EMAIL_EMAIL_EDT, XML_JA2UB_ENRICO_CONGRATS_MIGUELSICK_MANUELDEAD);
	}

	// email # 12d - Miguel alive, Manuel recruited and alive
	else if( fMiguelAlive && fManuelAlive && fManuelHired )
	{
		AddEmail(JA25_EMAIL_CONGRATS_MIGUEL_SICK_MANUEL_ALIVE, JA25_EMAIL_CONGRATS_MIGUEL_SICK_MANUEL_ALIVE_LENGTH, MAIL_ENRICO, GetWorldTotalMin(), -1, -1, TYPE_EMAIL_EMAIL_EDT, XML_JA2UB_ENRICO_CONGRATS_MIGUELSICK_MANUELALIVE);
	}

	// email # 12e - Miguel dead, Manuel dead
	else if( !fMiguelAlive && !fManuelAlive )
	{
		AddEmail(JA25_EMAIL_CONGRATS_MANUEL_DEAD, JA25_EMAIL_CONGRATS_MANUEL_DEAD_LENGTH, MAIL_ENRICO, GetWorldTotalMin(), -1, -1, TYPE_EMAIL_EMAIL_EDT, XML_JA2UB_ENRICO_CONGRATS_MANUELDEAD);
	}

	// email # 12f -  Miguel dead, Manuel recruited and alive
	else if( !fMiguelAlive && fManuelAlive && fManuelHired )
	{
		AddEmail(JA25_EMAIL_CONGRATS_MANUEL_ALIVE, JA25_EMAIL_CONGRATS_MANUEL_ALIVE_LENGTH, MAIL_ENRICO, GetWorldTotalMin(), -1, -1, TYPE_EMAIL_EMAIL_EDT, XML_JA2UB_ENRICO_CONGRATS_MANUELALIVE);
	}

	else
	{
		Assert( 0 );
	}
	#endif
}


void HandleEveryoneDoneTheirEndGameQuotes()
{
	// UnLock UI!
	guiPendingOverrideEvent = LU_ENDUILOCK;
	HandleTacticalUI( );

	//if laptop is still BROKEN
	if( gubQuest[ QUEST_FIX_LAPTOP ] == QUESTINPROGRESS && gGameUBOptions.LaptopQuestEnabled == TRUE )
	{
		gfPlayersLaptopWasntWorkingAtEndOfGame = TRUE;

		// otherwise, go to the credits screen
		HandleJa25EndGameAndGoToCreditsScreen( TRUE );
	}
	else
	{
		gfPlayersLaptopWasntWorkingAtEndOfGame = FALSE;

		gFadeOutDoneCallback = FadeOutToLaptopOnEndGame;

		FadeOutGameScreen( );	
	}
}


void HandleJa25EndGameAndGoToCreditsScreen( BOOLEAN fFromTactical )
{
	if( fFromTactical )
	{
		FadeToCredits( );
	}
	else
	{
		//Reset flag indicating we are in the end game sequence
		gTacticalStatus.uiFlags &= ~IN_ENDGAME_SEQUENCE;

		//We want to reinitialize the game
		ReStartingGame();	
	}
}

void EnterTacticalInFinalSector()
{
	gFadeInDoneCallback = InFinalSectorAfterFadeIn;

	FadeInGameScreen( );
}

void InFinalSectorAfterFadeIn( void )
{
	//Have everyone start talking
	DelayedMercQuote( NOBODY, DQ__START_EVERYONE_TALKING_AT_END_OF_GAME, GetWorldTotalSeconds() + 2 );
}

void FadeToCredits( void )
{
	gFadeOutDoneCallback = EndFadeToCredits;

	FadeOutGameScreen( );	
}

void EndFadeToCredits( void )
{
	//then we can go strait to the laptop screen
	InternalLeaveTacticalScreen( CREDIT_SCREEN );

	//Reset flag indicating we are in the end game sequence
	gTacticalStatus.uiFlags &= ~IN_ENDGAME_SEQUENCE;

	//We want to reinitialize the game
	ReStartingGame();	
}

void FadeOutToLaptopOnEndGame( void )
{

	//then we can go strait to the laptop screen
	InternalLeaveTacticalScreen( LAPTOP_SCREEN );

	//Add the end Game Emails
	HandleAddingTheEndGameEmails();
}

void BeginHandleQueenBitchDeath( TacticalActor *pKillerSoldier, INT32 sGridNo, INT8 bLevel )
{
	TacticalActor *pTeamSoldier;
	SoldierID cnt;


	gEndGameDeathCallback.capture(
		pKillerSoldier
			? GetJa2TacticalEntityId(*pKillerSoldier)
			: TacticalEntityId{},
		sGridNo, bLevel);

	// Lock the UI.....
	gTacticalStatus.uiFlags |= ENGAGED_IN_CONV;
	// Increment refrence count...
	giNPCReferenceCount = 1;

	// gTacticalStatus.uiFlags |= IN_DEIDRANNA_ENDGAME;

	SetCustomizableTimerCallbackAndDelay( 3000, QueenBitchTimerCallback, FALSE );


	// Kill all enemies in creature team.....
	cnt = gTacticalStatus.Team[ CREATURE_TEAM ].bFirstID;

	// look for all mercs on the same team,
	for ( ; cnt <= gTacticalStatus.Team[ CREATURE_TEAM ].bLastID; ++cnt )
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt.i);
		// Are we active and ALIVE and in sector.....
		if ( pTeamSoldier->roster().active() && pTeamSoldier->vitals().health() > 0 )
		{
			// For sure for flag thet they are dead is not set
			// Check for any more badguys
			// ON THE STRAGETY LAYER KILL BAD GUYS!

			// HELLO!	THESE ARE CREATURES!	THEY CAN'T BE NEUTRAL!
			//if ( !pTeamSoldier->aiBehavior().neutral() && (pTeamSoldier->roster().side() != gbPlayerNum ) )
			{
//	 		GetJa2PendingTacticalCombatActions()++;
				DebugAttackBusy( "Killing off a queen ally.\n");
				TacticalActorDamageResolution::applyHit(*pTeamSoldier,  0, 10000, 0, pTeamSoldier->position().direction(), 320, NOBODY, FIRE_WEAPON_NO_SPECIAL, pTeamSoldier->attackSelection().shotLocation(), 0, NOWHERE );
			}
		}
	}


}

void HandleQueenBitchDeath( TacticalActor *pKillerSoldier, INT32 sGridNo, INT8 bLevel )
{
	TacticalActor *pTeamSoldier;
	SoldierID		ubKillerSoldierID = NOBODY;

	// Start victory music here...
	SetMusicMode( MUSIC_TACTICAL_VICTORY );

	if ( pKillerSoldier )
	{
		TacticalCharacterDialogue( pKillerSoldier, QUOTE_KILLING_QUEEN );
		ubKillerSoldierID = pKillerSoldier->identity().id();
	}

	// STEP 1 ) START ALL QUOTES GOING!
	// OK - loop through all witnesses and see if they want to say something abou this...
	SoldierID cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;

	// run through list
	for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
	{
		if ( cnt != ubKillerSoldierID )
		{
			pTeamSoldier =
				GetJa2SoldierRepository().resolve(cnt.i);
			if ( OK_INSECTOR_MERC( pTeamSoldier ) && !( pTeamSoldier->status().flags() & SOLDIER_GASSED ) && !AM_AN_EPC( pTeamSoldier ) )
			{
				if ( QuoteExp[ pTeamSoldier->identity().profile() ].QuoteExpWitnessQueenBugDeath )
				{
					if ( SoldierTo3DLocationLineOfSightTest( pTeamSoldier, sGridNo,  bLevel, 3, TRUE, CALC_FROM_ALL_DIRS ) )
					{
						TacticalCharacterDialogue( pTeamSoldier, QUOTE_KILLING_QUEEN );
					}
				}
			}
		}
	}


	// Set fact that she is dead!
	if ( CheckFact( FACT_QUEEN_DEAD, 0 ) )
	{
	 EndQueenDeathEndgameBeginEndCimenatic( );
	}
	else
	{
	// Unset flags...
	gTacticalStatus.uiFlags &= (~ENGAGED_IN_CONV );
	// Increment refrence count...
		giNPCReferenceCount = 0;
		}
	}

//JA25UB
void DoneFadeOutJa25EndCinematic( void )
{
	INT32 cnt;
	TacticalActor *pSoldier;
	auto& soldiers = GetJa2SoldierRepository();

	//Change the currently selecter sector in mapscreen
	//ChangeSelectedMapSector( 16, 11, 0 );
	  ChangeSelectedMapSector( gGameUBOptions.ubEndDefaultSectorX, gGameUBOptions.ubEndDefaultSectorY, gGameUBOptions.ubEndDefaultSectorZ );
	//
	// Loop through all the soldiers and move any of them that are in the complex to be in the safe sector near ther
	//
	cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
	for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
	{
		pSoldier = soldiers.resolve(cnt);
		// if the soldier was in the complex
		if( pSoldier->roster().active() &&
				pSoldier->deployment().sectorX() == 15 && ( pSoldier->deployment().sectorY() == 11 || pSoldier->deployment().sectorY() == 12 ) )
		{
			if ( GetGroup( pSoldier->deployment().groupId() ) )
			{
				//move them to the 'fake' sector
				//PlaceGroupInSector( pSoldier->deployment().groupId(), 15, 11, 16, 11, 0, FALSE );
				  PlaceGroupInSector( pSoldier->deployment().groupId(), 15, 11, gGameUBOptions.ubEndDefaultSectorX, gGameUBOptions.ubEndDefaultSectorY, gGameUBOptions.ubEndDefaultSectorZ, FALSE );
			}
			else
			{
				pSoldier->deployment().sectorX() = gGameUBOptions.ubEndDefaultSectorX; //16;
				pSoldier->deployment().sectorY() = gGameUBOptions.ubEndDefaultSectorY; //11;
				pSoldier->deployment().sectorZ() = gGameUBOptions.ubEndDefaultSectorZ; //0;
			}
		}
	}

	//
	// Go watch the movies
	//
	InternalLeaveTacticalScreen( INTRO_SCREEN );
	SetIntroType( INTRO_ENDING );
}
