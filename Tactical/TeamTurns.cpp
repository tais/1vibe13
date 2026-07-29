	#include "types.h"
#include "TacticalWorldAdapter.h"
	#include "Overhead.h"
	#include "Animation Control.h"
	#include "Points.h"
	#include "opplist.h"
	#include "Sound Control.h"
	#include "Interface.h"
	#include "Isometric Utils.h"
	#include "Font Control.h"
	#include "ai.h"
	#include "message.h"
	#include "Text.h"
	#include "TeamTurns.h"
	#include "Smell.h"
	#include "Game Clock.h"
	#include "Queen Command.h"
	#include "PATHAI.H"
	#include "lighting.h"
	#include "environment.h"
	#include "Explosion Control.h"
	#include "Dialogue Control.h"
	#include "soldier profile type.h"
	#include "SmokeEffects.h"
	#include "LightEffects.h"
	#include "Air Raid.h"
	#include "SkillCheck.h"
	#include "AIInternals.h"
	#include "AIList.h"
	#ifdef DEBUG_INTERRUPTS
		#include "DEBUG.H"
	#endif
	#include "renderworld.h"
	#include "Rotting Corpses.h"
	#include "Squads.h"
	#include "Soldier macros.h"
	#include "Soldier Profile.h"
	#include "NPC.h"
	#include "Drugs And Alcohol.h"	// added by Flugente
#include "GameSettings.h"
#include "Reinforcement.h"
#include "fresh_header.h"
#include "connect.h"
#include "Map Information.h"
#include "SoldierRepository.h"


#ifdef JA2UB
#include "Ja25_Tactical.h"
#include "Ja25 Strategic Ai.h"
#else
#include "Meanwhile.h"
#endif // JA2UB

//forward declarations of common classes to eliminate includes
class OBJECTTYPE;
class TacticalActor;

extern INT8 STRAIGHT;
//extern UINT8 gubSpeedUpAnimationFactor;
void SetSoldierAniSpeed( TacticalActor *pSoldier );

// sevenfm
time_t gtTimeSinceMercAIStart;

void RecalculateSoldiersAniSpeed()
{
	UINT32 uiLoop;
	TacticalActor *pSoldier;

//	if( gubSpeedUpAnimationFactor == 1 )return;

	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pSoldier = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, dead, unconscious
		if (!pSoldier)
		{
			continue;			// next merc
		}

		SetSoldierAniSpeed( pSoldier );
	}

}



extern void DecayPublicOpplist(INT8 bTeam);
extern void VerifyAndDecayOpplist(TacticalActor *pSoldier);
void EndInterrupt( BOOLEAN fMarkInterruptOccurred );
void DeleteFromIntList( UINT16 ubIndex, BOOLEAN fCommunicate);

#define END_OF_INTERRUPTS 255

UINT16 gubOutOfTurnOrder[MAXMERCS] = { END_OF_INTERRUPTS, 0 };
UINT16 gubOutOfTurnPersons = 0;

#define LATEST_INTERRUPT_GUY (gubOutOfTurnOrder[gubOutOfTurnPersons])
#define REMOVE_LATEST_INTERRUPT_GUY()	(DeleteFromIntList( (gubOutOfTurnPersons), TRUE ))
#define INTERRUPTS_OVER (gubOutOfTurnPersons == 1)

SoldierID	InterruptOnlyGuynum = NOBODY;
BOOLEAN		InterruptsAllowed = TRUE;
BOOLEAN		gfHiddenInterrupt = FALSE;
SoldierID	gubLastInterruptedGuy = NOBODY;

extern SoldierID gsWhoThrewRock;
extern UINT8 gubSightFlags;

typedef struct
{
	UINT16		ubOutOfTurnPersons;

	SoldierID	InterruptOnlyGuynum;
	SoldierID	sWhoThrewRock;
	BOOLEAN		InterruptsAllowed;
	BOOLEAN		fHiddenInterrupt;
	SoldierID	ubLastInterruptedGuy;

	UINT8	ubFiller[16];
} TEAM_TURN_SAVE_STRUCT;

// WANNE: Moved to APBPConstants
//#define MIN_APS_TO_INTERRUPT 4

void ClearIntList( void )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"ClearIntList");
	memset( gubOutOfTurnOrder, 0, sizeof(gubOutOfTurnOrder) );   // UINT16[] -> byte size, not element count (was half-cleared)
	gubOutOfTurnOrder[0] = END_OF_INTERRUPTS;
	gubOutOfTurnPersons = 0;
}

BOOLEAN BloodcatsPresent( void )
{
	TacticalActor *pSoldier;

	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"BloodcatsPresent");
	if ( gTacticalStatus.Team[ CREATURE_TEAM ].bTeamActive == FALSE )
	{
		return( FALSE );
	}

	for ( SoldierID iLoop = gTacticalStatus.Team[ CREATURE_TEAM ].bFirstID; iLoop <= gTacticalStatus.Team[ CREATURE_TEAM ].bLastID; ++iLoop )
	{
		pSoldier =
			GetJa2SoldierRepository().resolve(iLoop.i);
		if (!pSoldier)
		{
			continue;
		}

		if ( pSoldier->roster().active() && pSoldier->roster().inSector() && pSoldier->vitals().health() > 0 && pSoldier->identity().bodyType() == BLOODCAT )
		{
			return( TRUE );
		}
	}

	return( FALSE );
}

void StartPlayerTeamTurn( BOOLEAN fDoBattleSnd, BOOLEAN fEnteringCombatMode )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"StartPlayerTeamTurn");
//	TacticalActor		*pSoldier;
//	EV_S_BEGINTURN	SBeginTurn;

	SetFastForwardMode(FALSE);
	SetClockSpeedPercent(gGameExternalOptions.fClockSpeedPercent);	// sevenfm: set default clock speed

	gTacticalStatus.ubDisablePlayerInterrupts = FALSE;

	// Start the turn of player charactors

	//
	// PATCH 1.06:
	//
	// make sure set properly in gTacticalStatus:
	SetJa2TacticalCurrentTeam( OUR_TEAM );

	InitPlayerUIBar( FALSE );

	if ( IsJa2TacticalTurnBased() )
	{
		// Are we in combat already?
		if ( IsJa2TacticalCombatActive() )
		{
			PlayJA2Sample( ENDTURN_1, RATE_11025, MIDVOLUME, 1, MIDDLEPAN );
		}

		// Remove deadlock message
		EndDeadlockMsg( );

		// Check for victory conditions

		// ATE: Commented out - looks like this message is called earlier for our team
		// look for all mercs on the same team,
		//SoldierID cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
		//for each repository-owned soldier slot on the player team
		//{
		//	if ( pSoldier->roster().active() && pSoldier->vitals().health() > 0 )
		//	{
		//		SBeginTurn.usSoldierID		= (UINT16)cnt;
		//		AddGameEvent( S_BEGINTURN, 0, &SBeginTurn );
		//	}
		//}

		// Are we in combat already?
		if ( IsJa2TacticalCombatActive() )
		{
			if ( gusSelectedSoldier != NOBODY )
			{
				TacticalActor* selectedSoldier =
					GetJa2SoldierRepository().resolve(
						gusSelectedSoldier.i);
				if (!selectedSoldier)
				{
					gusSelectedSoldier = NOBODY;
				}
				// Check if this guy is able to be selected....
				else if ( selectedSoldier->vitals().health() < OKLIFE )
				{
					DebugMsg(TOPIC_JA2INTERRUPT,DBG_LEVEL_3,String("StartPlayerTeamTurn: SelectNextAvailSoldier"));
					SelectNextAvailSoldier( selectedSoldier );
				}
				else
				{
					SelectSoldier( gusSelectedSoldier, FALSE, FALSE);
				}

				// Slide to selected guy...
				if ( gusSelectedSoldier != NOBODY )
				{
					SlideTo(gusSelectedSoldier, SETLOCATOR);

					if ( fDoBattleSnd )
					{
						// Say ATTENTION SOUND...
						TacticalActor* selectedSoldier =
							GetJa2SoldierRepository().resolve(
								gusSelectedSoldier.i);
						if (selectedSoldier)
						{
							selectedSoldier->DoMercBattleSound( BATTLE_SOUND_ATTN1 );
						}
					}

					if ( gsInterfaceLevel == 1 )
					{
						gTacticalStatus.uiFlags |= SHOW_ALL_ROOFS;
						InvalidateWorldRedundency( );
						SetRenderFlags(RENDER_FLAG_FULL);
						ErasePath(FALSE);
					}
				}
			}
		}

		// Dirty panel interface!
		fInterfacePanelDirty = DIRTYLEVEL2;

		if ( !fEnteringCombatMode )
		{
			CheckForEndOfCombatMode( TRUE );
		}

	}
	// Signal UI done enemy's turn
	guiPendingOverrideEvent = LU_ENDUILOCK;
	
	if (is_networked)
		guiPendingOverrideEvent = LA_ENDUIOUTURNLOCK;

	// ATE: Reset killed on attack variable.. this is because sometimes timing is such
	/// that a baddie can die and still maintain it's attacker ID
	gTacticalStatus.fKilledEnemyOnAttack = FALSE;
	
	gTacticalStatus.ubInterruptPending	= DISABLED_INTERRUPT;

	HandleTacticalUI( );
}

void FreezeInterfaceForEnemyTurn( void )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"FreezeInterfaceForEnemyTurn");
	// Reset flags
	gfPlotNewMovement = TRUE;

	// Erase path
	ErasePath( TRUE );

	// Setup locked UI
	if(is_client)
	{
		guiPendingOverrideEvent = LA_BEGINUIOURTURNLOCK;
	}
	else 
	{
		guiPendingOverrideEvent = LU_BEGINUILOCK;
	}

	// Remove any UI messages!
	if ( giUIMessageOverlay != -1 )
	{
		EndUIMessage( );
	}
}


void EndTurn( UINT8 ubNextTeam )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"EndTurn");
	TacticalActor * pSoldier;

	//Check for enemy pooling (add enemies if there happens to be more than the max in the
	//current battle.	If one or more slots have freed up, we can add them now.

	EndDeadlockMsg( );

/*
	if ( CheckForEndOfCombatMode( FALSE ) )
	{
		return;
	}
	*/

	if (INTERRUPT_QUEUED)
	{
			if(is_networked)
			{
				end_interrupt( FALSE );//this tells other client to go on from where he was
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Player ended interrupt." );
			}
			EndInterrupt( FALSE );
	}
	else
	{
		if(gGameExternalOptions.gfAllowReinforcements)//dnl ch68 100913 agree with Flugente, put all under one condition
		{
			guiTurnCnt++;
			// Flugente: I'm not really sure why we check here for gGameExternalOptions.ubReinforcementsFirstTurnFreeze, shouldn't that be a check for ALLOW_REINFORCEMENTS?
			// as this inhibits reinforcements during turnbased-mode, I'll check for that instead
			// HEADROCK HAM 3.2: Experimental fix to force reinforcements enter battle with 0 APs.
			//if (gGameExternalOptions.ubReinforcementsFirstTurnFreeze != 1 && gGameExternalOptions.ubReinforcementsFirstTurnFreeze != 2)
			AddPossiblePendingEnemiesToBattle();
			//if (gGameExternalOptions.ubReinforcementsFirstTurnFreeze != 1 && gGameExternalOptions.ubReinforcementsFirstTurnFreeze != 3)
			AddPossiblePendingMilitiaToBattle();
		}

//		InitEnemyUIBar( );

		FreezeInterfaceForEnemyTurn();

		// Loop through all mercs and set to moved
		SoldierID cnt = gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bFirstID;
		for ( ; cnt <= gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bLastID; ++cnt )
		{
			pSoldier =
				GetJa2SoldierRepository().resolve(cnt.i);
			if (!pSoldier)
			{
				continue;
			}
			if ( pSoldier->roster().active() )
			{
				pSoldier->turnState().moved() = TRUE;
			}
		}

		SetJa2TacticalCurrentTeam( ubNextTeam );
		
		gTacticalStatus.ubInterruptPending	= DISABLED_INTERRUPT;

		if(is_server || !is_client) BeginTeamTurn( GetJa2TacticalCurrentTeam() );

		// WANNE: Disabled Headrocks Experimental fix, because it causes assertion in AddPossiblePendingMilitiaToBattle();
		// HEADROCK HAM 3.2: Experimental fix to force reinforcements enter battle with 0 APs.
		/*
		if (gGameExternalOptions.ubReinforcementsFirstTurnFreeze == 1 || gGameExternalOptions.ubReinforcementsFirstTurnFreeze == 2)
		{
			AddPossiblePendingEnemiesToBattle();
		}
		if (gGameExternalOptions.ubReinforcementsFirstTurnFreeze == 1 || gGameExternalOptions.ubReinforcementsFirstTurnFreeze == 3)
		{
			AddPossiblePendingMilitiaToBattle();
		}
		*/

		BetweenTurnsVisibilityAdjustments();
	}
}

void EndAITurn( void )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"EndAITurn");
	TacticalActor * pSoldier;

	// Remove any deadlock message
	EndDeadlockMsg( );
	if (INTERRUPT_QUEUED)
	{
			if(is_networked)
			{
				end_interrupt( FALSE );//this tells other client to go on from where he was
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"AI ended interrupt." );
			}
		EndInterrupt( FALSE );
	}
	else
	{
		SoldierID cnt = gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bFirstID;
		for ( ; cnt <= gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bLastID; ++cnt )
		{
			pSoldier =
				GetJa2SoldierRepository().resolve(cnt.i);
			if (!pSoldier)
			{
				continue;
			}
			if ( pSoldier->roster().active() )
			{
				pSoldier->turnState().moved() = TRUE;
				// record old life value... for creature AI; the human AI might
				// want to use this too at some point
				pSoldier->vitals().snapshotHealth();
			}
		}

		AdvanceJa2TacticalCurrentTeam();
		BeginTeamTurn( GetJa2TacticalCurrentTeam() );
	}
}

void EndAllAITurns( void )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"EndAllAITurns");
	// warp turn to the player's turn
	TacticalActor * pSoldier;

	// Remove any deadlock message
	EndDeadlockMsg( );
	if (INTERRUPT_QUEUED)
	{
		EndInterrupt( FALSE );
	}

	if ( GetJa2TacticalCurrentTeam() != gbPlayerNum )
	{
		SoldierID id = gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bFirstID;
		for ( ; id <= gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bLastID; ++id )
		{
			pSoldier =
				GetJa2SoldierRepository().resolve(id.i);
			if (!pSoldier)
			{
				continue;
			}
			if ( pSoldier->roster().active() )
			{
				pSoldier->turnState().moved() = TRUE;
				pSoldier->status().flags() &= (~SOLDIER_UNDERAICONTROL);
				// record old life value... for creature AI; the human AI might
				// want to use this too at some point
				pSoldier->vitals().snapshotHealth();
			}
		}

		SetJa2TacticalCurrentTeam( gbPlayerNum );
		//BeginTeamTurn( GetJa2TacticalCurrentTeam() );
		
		gTacticalStatus.ubInterruptPending	= DISABLED_INTERRUPT;
	}
}

void EndTurnEvents( void )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"EndTurnEvents");
	// HANDLE END OF TURN EVENTS
	// handle team services like healing
	HandleTeamServices( gbPlayerNum );
	// handle smell and blood decay
	DecaySmells();
	// decay bomb timers and maybe set some off!
	DecayBombTimers();

	DecayLightEffects( GetWorldTotalSeconds( ) );
	DecaySmokeEffects( GetWorldTotalSeconds( ) );

	TacticalActor* pSoldier = NULL;
	SoldierID  id = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
	for ( ; id <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++id )
	{
		pSoldier =
			GetJa2SoldierRepository().resolve(id.i);
		if (!pSoldier)
		{
			continue;
		}
		if ( pSoldier->roster().active() && pSoldier->vitals().health() > 0 )//&& !( pSoldier->status().flags() & SOLDIER_VEHICLE ) && !( AM_A_ROBOT( pSoldier ) ) )
		{
			// Flugente: update multi-turn actions
			pSoldier->UpdateMultiTurnAction();
		}
	}

	// Flugente: Cool down/decay all items not in a soldier's inventory
	CoolDownWorldItems( );	

	// Flugente: raise zombies if in gamescreen and option set
	if ( GetCurrentScreen() == GAME_SCREEN )
	{
		RaiseZombies();
	}

	// decay AI warning values from corpses
	DecayRottingCorpseAIWarnings();

	HandleEnvironmentHazard( );

#ifdef JA2UB	
	//Ja25 UB
	
	//increment the number of tactical turns that have gone by in turn based mode
	gJa25SaveStruct.uiTacticalTurnCounter++;

	//if the fan should start up
	HandleStartingFanBackUp();
#endif
}

//rain
BOOLEAN LightningEndOfTurn( UINT8 ubTeam );
//end rain

void BeginTeamTurn( UINT8 ubTeam )
{

	// MP safety sweep: no remote copy should be auto-walking across a turn boundary --
	// a copy whose stop event was missed cycles its walk anim (footsteps) forever.
	if ( is_networked )
	{
		for ( SoldierID id = 0; id < TOTAL_SOLDIERS; ++id )
		{
			TacticalActor* pStop =
				GetJa2SoldierRepository().resolve(id.i);
			if ( pStop && pStop->roster().active() && pStop->roster().inSector() && pStop->roster().team() >= LAN_TEAM_ONE
				&& pStop->position().gridNo() >= 0 && pStop->position().gridNo() < WORLD_MAX
				&& ( gAnimControl[ pStop->animationPlayback().state() ].uiFlags & ANIM_MOVING ) )
			{
				pStop->EVENT_StopMerc( pStop->position().gridNo(), pStop->position().direction() );
			}
		}
	}
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"BeginTeamTurn");
	TacticalActor		*pSoldier;

	//rain
	if( !LightningEndOfTurn( ubTeam ) )return;
	//end rain

	// disable for our turn and enable for other teams
	if ( gGameSettings.fOptions[TOPTION_AUTO_FAST_FORWARD_MODE] )
	{
		if (is_networked)
		{
			// Only allow fast forward mode on enemy team!
			SetFastForwardMode( (ubTeam == ENEMY_TEAM) );
		}
		else
		{
			// Allow fast forward mode on all teams except our team!
			SetFastForwardMode( (ubTeam != OUR_TEAM) );
		}		
	}

	while( 1 )
	{
		if ( ubTeam > LAST_TEAM )
		{
			if ( HandleAirRaidEndTurn( ubTeam ) )
			{
				// End turn!!
				ubTeam = gbPlayerNum;
				SetJa2TacticalCurrentTeam( gbPlayerNum );
				EndTurnEvents();
				if(is_server)
				{
					numreadyteams =0;//beginning round 
					memset( &readyteamreg , 0 , sizeof (int) * 10);
				}
			}
			else
			{
				break;
			}
		}
		else if (!(gTacticalStatus.Team[ ubTeam ].bTeamActive))
		{
			// inactive team, skip to the next one
			ubTeam++;
			AdvanceJa2TacticalCurrentTeam();
			// skip back to the top, as we are processing another team now.
			continue;
		}
		if ((!(gTacticalStatus.Team[ ubTeam ].bTeamActive))&& is_networked)
		{
			// inactive team, skip to the next one
			ubTeam++;
			AdvanceJa2TacticalCurrentTeam();
			// skip back to the top, as we are processing another team now.
			continue;
		}

		// This is the first point at which skipped/inactive teams have been
		// resolved and a real team turn is about to begin.
		NotifyJa2TacticalTeamTurnBegan(
			CaptureJa2TacticalWorld().worldGeneration);

		if ( IsJa2TacticalTurnBased() )
		{
			BeginLoggingForBleedMeToos( TRUE );

			// decay team's public opplist
			DecayPublicOpplist( ubTeam );

			SoldierID id = gTacticalStatus.Team[ ubTeam ].bFirstID;
			for ( ; id <= gTacticalStatus.Team[ ubTeam ].bLastID; ++id )
			{
				pSoldier =
					GetJa2SoldierRepository().resolve(id.i);
				if (!pSoldier)
				{
					continue;
				}
				if ( pSoldier->roster().active() && pSoldier->vitals().health() > 0)
				{
					// decay personal opplist, and refresh APs and BPs
					pSoldier->EVENT_BeginMercTurn( FALSE, 0 );
				}
			}

			if (gTacticalStatus.bBoxingState == LOST_ROUND || gTacticalStatus.bBoxingState == WON_ROUND || gTacticalStatus.bBoxingState == DISQUALIFIED )
			{
				// we have no business being in here any more!
				return;
			}

			BeginLoggingForBleedMeToos( FALSE );

		}

		RecalculateSoldiersAniSpeed();

		if (ubTeam == gbPlayerNum )
		{
			// ATE: Check if we are still in a valid battle...
			// ( they could have blead to death above )
			if ( ( IsJa2TacticalCombatActive() ) )
			{
				extern BOOLEAN gfDedicatedServer;
				// Coordinator host has no mercs and no UI to end a turn -- never run
				// its own player turn; just advance the round to the real players.
				if ( !gfDedicatedServer )
					StartPlayerTeamTurn( TRUE, FALSE );
				if(is_server) 
				{
					numreadyteams =0;//beginning round 
					memset( &readyteamreg , 0 , sizeof (int) * 10);
					send_EndTurn(netbTeam);
				}
			}
			break;
		}
		else if (ubTeam > 4 || (is_client && !is_server )) //hayden
		{
			
			InitEnemyUIBar( 0, 0 );
			fInterfacePanelDirty = DIRTYLEVEL2;
			AddTopMessage( COMPUTER_TURN_MESSAGE, TeamTurnString[ ubTeam ] );
			/*if(is_server && !net_turn) send_EndTurn(ubTeam);
			if(net_turn == true) net_turn = false;*/
			SetJa2TacticalCurrentTeam( ubTeam );
			if(is_server) send_EndTurn(ubTeam);
			
			
			
			//return;
			break;
		}
		else
		{
#ifdef NETWORKED
			// Only the host should do this
			if(!gfAmIHost)
				break;
#endif
				if( is_client && !is_server ) //hayden //disable independant client AI
					break;


			// Set First enemy merc to AI control	
			if ( BuildAIListForTeam( ubTeam ) )
			{

				SoldierID ubID = RemoveFirstAIListEntry();
				if (ubID != NOBODY)
				{
					// Dirty panel interface!
					fInterfacePanelDirty = DIRTYLEVEL2;
					if ( ubTeam == CREATURE_TEAM && BloodcatsPresent() )
					{
						AddTopMessage( COMPUTER_TURN_MESSAGE, Message[ STR_BLOODCATS_TURN ] );
					}
					else
					{
						AddTopMessage( COMPUTER_TURN_MESSAGE, TeamTurnString[ ubTeam ] );
					}
					TacticalActor* firstAiSoldier =
						GetJa2SoldierRepository().resolve(ubID.i);
					if (!firstAiSoldier)
					{
						ubTeam++;
						AdvanceJa2TacticalCurrentTeam();
						continue;
					}
					StartNPCAI( firstAiSoldier );
					/*if(is_server && !net_turn) send_EndTurn(ubTeam);
					if(net_turn == true) net_turn = false;*/
					if(is_server) send_EndTurn(ubTeam);
					
					return;
				}
			}

			// This team is dead/inactive/being skipped in boxing
			// skip back to the top to process the next team
			ubTeam++;
			AdvanceJa2TacticalCurrentTeam();
		}
	}
}

void DisplayHiddenInterrupt( TacticalActor * pSoldier )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"DisplayHiddenInterrupt");
	// If the AI got an interrupt but this has been hidden from the player until this point,
	// this code will display the interrupt

	if (!gfHiddenInterrupt)
	{
		return;
	}
	EndDeadlockMsg( );

	if (pSoldier->awareness().visibility() != -1 )
	{
		SlideTo( pSoldier->identity().id(), SETLOCATOR);
	}

		if(is_client)
	{
		guiPendingOverrideEvent = LA_BEGINUIOURTURNLOCK;
	}
	else 
	{
		guiPendingOverrideEvent = LU_BEGINUILOCK;
	}

	// Dirty panel interface!
	fInterfacePanelDirty = DIRTYLEVEL2;

	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"about to call ErasePath");
	// Erase path!
	ErasePath( TRUE );

	// Reset flags
	gfPlotNewMovement = TRUE;

	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"about to call AdjustNoAPToFinishMove");
	// Stop our guy. The latest-entry field may contain the empty-list sentinel.
	TacticalActor* interruptedSoldier =
		GetJa2SoldierRepository().resolve(
			LATEST_INTERRUPT_GUY);
	if ( LATEST_INTERRUPT_GUY != END_OF_INTERRUPTS &&
		interruptedSoldier )
	{
		interruptedSoldier->AdjustNoAPToFinishMove( TRUE );
		// Stop him from going to prone position if doing a turn while prone
		interruptedSoldier->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
	}

	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"about to call AddTopMessage");
	// get rid of any old overlay message
	if ( pSoldier->roster().team() == MILITIA_TEAM )
	{
		AddTopMessage( MILITIA_INTERRUPT_MESSAGE, Message[ STR_INTERRUPT ] );
	}
	else
	{
		AddTopMessage( COMPUTER_INTERRUPT_MESSAGE, Message[ STR_INTERRUPT ] );
	}

	gfHiddenInterrupt = FALSE;

	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"DisplayHiddenInterrupt completed");
}

void DisplayHiddenTurnbased( TacticalActor * pActingSoldier )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"DisplayHiddenTurnBased");
	// This code should put the game in turn-based and give control to the AI-controlled soldier
	// whose pointer has been passed in as an argument (we were in non-combat and the AI is doing
	// something visible, i.e. making an attack)
#ifdef JA2UB
//Ja25 No meanwhiles
#else
	if ( AreInMeanwhile( ) )
	{
		return;
	}
#endif
	if (gTacticalStatus.uiFlags & REALTIME || IsJa2TacticalCombatActive())
	{
		// pointless call here; do nothing
		return;
	}

	// Enter combat mode starting with this side's turn
	SetJa2TacticalCurrentTeam( pActingSoldier->roster().team() );

	CommonEnterCombatModeCode( );

	//JA2Gold: use function to make sure flags turned off everywhere else
	//pActingSoldier->status().flags() |= SOLDIER_UNDERAICONTROL;
	pActingSoldier->SetSoldierAsUnderAiControl(	);
	DebugAI( String( "Giving AI control to %d", pActingSoldier->identity().id() ) );
	pActingSoldier->movement().beginTurn();
	gTacticalStatus.uiTimeSinceMercAIStart = GetJA2Clock();	
	gtTimeSinceMercAIStart = time(0);	// sevenfm: also remember system time

	if ( gTacticalStatus.combatUI.ubTopMessageType != COMPUTER_TURN_MESSAGE)
	{
		// Dirty panel interface!
		fInterfacePanelDirty = DIRTYLEVEL2;
		if ( GetJa2TacticalCurrentTeam() == CREATURE_TEAM && BloodcatsPresent() )
		{
			AddTopMessage( COMPUTER_TURN_MESSAGE, Message[ STR_BLOODCATS_TURN ] );
		}
		else
		{
			AddTopMessage( COMPUTER_TURN_MESSAGE, TeamTurnString[ GetJa2TacticalCurrentTeam() ] );
		}

	}


	// freeze the user's interface
	FreezeInterfaceForEnemyTurn();
}

BOOLEAN EveryoneInInterruptListOnSameTeam( void )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"EveryoneInInterruptListOnSameTeam");
	UINT16 ubLoop;
	UINT8	ubTeam = 255;

	for (ubLoop = 1; ubLoop <= gubOutOfTurnPersons; ubLoop++)
	{
		UINT16 ubID = gubOutOfTurnOrder[ ubLoop ];
		TacticalActor* soldier =
			GetJa2SoldierRepository().resolve(ubID);
		if ( !soldier )   // skip sentinel/NOBODY/empty slots (list can be wire-fed)
			continue;
		if ( ubTeam == 255 )
		{
			ubTeam = soldier->roster().team();
		}
		else
		{
			if ( soldier->roster().team() != ubTeam )
			{
				return( FALSE );
			}
		}
	}
	return( TRUE );
}

void StartInterrupt( void )
{
	INT8			bTeam;
	TacticalActor *pSoldier;
	TacticalActor *pTempSoldier;
	SoldierID 	ubFirstInterrupter;
	SoldierID 	ubInterrupter;
	TacticalActor *pInterrupter;
	INT32		cnt;

	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"StartInterrupt");
	ubFirstInterrupter = LATEST_INTERRUPT_GUY;
	AssertLE(ubFirstInterrupter, MAX_NUM_SOLDIERS);
	pSoldier =
		GetJa2SoldierRepository().resolve(
			ubFirstInterrupter.i);
	AssertNotNIL(pSoldier);
	if (!pSoldier)
	{
		ClearIntList();
		return;
	}
	bTeam = pSoldier->roster().team();
	ubInterrupter = ubFirstInterrupter;

#ifdef _DEBUG
	// display everyone on int queue!
	for ( cnt = gubOutOfTurnPersons; cnt > 0; cnt-- )
	{
		DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("STARTINT:	Q position %d: %d", cnt, gubOutOfTurnOrder[ cnt ] ) );
	}
#endif

	//DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("INTERRUPT: %d is now on top of the interrupt queue", ubFirstInterrupter ) );

	gTacticalStatus.fInterruptOccurred = TRUE;

	for ( SoldierID id = 0; id < MAX_NUM_SOLDIERS; ++id )
	{
		pTempSoldier =
			GetJa2SoldierRepository().resolve(id.i);
		if (!pTempSoldier)
		{
			continue;
		}
		if ( pTempSoldier->roster().active() )
		{
			pTempSoldier->turnState().captureMoved(pTempSoldier->turnState().moved());
			pTempSoldier->turnState().moved() = TRUE;
		}
	}

	if (pSoldier->roster().team() == OUR_TEAM)
	{
		// start interrupts for everyone on our side at once
		CHAR16		sTemp[ 255 ];
		UINT8		ubInterrupters = 0;
		INT32		iSquad, iCounter;

		// disable ff mode
		SetFastForwardMode(FALSE);
		SetClockSpeedPercent(gGameExternalOptions.fClockSpeedPercent);	// sevenfm: set default clock speed

		// build string for display of who gets interrupt
		//while( 1 )
		for( iCounter = 0; iCounter < MAX_NUM_SOLDIERS; iCounter++ )
		{
			pInterrupter =
				GetJa2SoldierRepository().resolve(
					ubInterrupter.i);
			if (!pInterrupter)
			{
				break;
			}
			pInterrupter->turnState().moved() = FALSE;
			DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("INTERRUPT: popping %d off of the interrupt queue", ubInterrupter ) );

			REMOVE_LATEST_INTERRUPT_GUY();
			// now LATEST_INTERRUPT_GUY is the guy before the previous
			ubInterrupter = LATEST_INTERRUPT_GUY;

			if (ubInterrupter == NOBODY) // previously emptied slot!
			{
				continue;
			}
			else if (!(pInterrupter =
				GetJa2SoldierRepository().resolve(
					ubInterrupter.i)) ||
				pInterrupter->roster().team() != bTeam)
			{
				break;
			}
		}

		// TODO: check here to see if we really can do anything with available mercs on this team
		//   if not then end interrupt.  Probably should check if actions for for the interrupter but then we
		//   would lose potential XP for being able to radio rest of team about enemy from the interrupt 
		// Theoretically ubInterrupter is enemy causing interrupt


		BOOL handleInterrupt = TRUE;
		if (ubInterrupter != NOBODY)
		{
			handleInterrupt = FALSE;

			// build string in separate loop here, want to linearly process squads...
			TacticalActor *pInterruptedSoldier =
				GetJa2SoldierRepository().resolve(
					ubInterrupter.i);
			if (!pInterruptedSoldier)
			{
				handleInterrupt = FALSE;
			}
			else
			{
				for ( iSquad = 0; iSquad < NUMBER_OF_SQUADS; iSquad++ )
				{
					for ( iCounter = 0; iCounter < NUMBER_OF_SOLDIERS_PER_SQUAD; iCounter++ )
					{
						pTempSoldier = ResolveSquadMember( iSquad, iCounter );
						if ( pTempSoldier && pTempSoldier->roster().active() && pTempSoldier->roster().inSector() && !pTempSoldier->turnState().moved() )
						{
							INT16 ubMinAPcost = MinAPsToAttack(pSoldier,pInterruptedSoldier->position().gridNo(),ADDTURNCOST, 0);
							// if we don't have enough APs left to shoot even a snap-shot at this guy
							if (ubMinAPcost < pSoldier->actionPoints().current())
							{
								handleInterrupt = TRUE;
							}
						}
					}
				}
			}
		}
		if (!handleInterrupt)
		{
			// no mercs can take even a snapshot at the guy
			EndInterrupt(TRUE);
		}
		else
		{
			wcscpy( sTemp, Message[ STR_INTERRUPT_FOR ] );

			// build string in separate loop here, want to linearly process squads...
			ubInterrupters = 0;
			for ( iSquad = 0; iSquad < NUMBER_OF_SQUADS; iSquad++ )
			{
				for ( iCounter = 0; iCounter < NUMBER_OF_SOLDIERS_PER_SQUAD; iCounter++ )
				{
					pTempSoldier = ResolveSquadMember( iSquad, iCounter );
					if ( pTempSoldier && pTempSoldier->roster().active() && pTempSoldier->roster().inSector() && !pTempSoldier->turnState().moved() )
					{
						// then this guy got an interrupt...
						ubInterrupters++;
						if ( ubInterrupters > 6 )
						{
							// flush... display string, then clear it (we could have 20 names!)
							// add comma to end, we know we have another person after this...
							wcscat( sTemp, L", " );
							ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE,	sTemp );
							wcscpy( sTemp, L"" );
							ubInterrupters = 1;
						}

						if ( ubInterrupters > 1 )
						{
							wcscat( sTemp, L", " );
						}
						wcscat( sTemp, pTempSoldier->identity().name() );
					}
				}
			}

			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE,	sTemp );

			DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("INTERRUPT: starting interrupt for %d", ubFirstInterrupter ) );
			// gusSelectedSoldier should become the topmost guy on the interrupt list
			//gusSelectedSoldier = ubFirstInterrupter;

			// Remove deadlock message
			EndDeadlockMsg( );

			// Select guy....
			SelectSoldier( ubFirstInterrupter, TRUE, TRUE );

			// ATE; Slide to guy who got interrupted!
			SlideTo( gubLastInterruptedGuy, SETLOCATOR);

			// Dirty panel interface!
			fInterfacePanelDirty						= DIRTYLEVEL2;
			SetJa2TacticalCurrentTeam( pSoldier->roster().team() );

			// Signal UI done enemy's turn
			guiPendingOverrideEvent = LU_ENDUILOCK;
			
			if (is_networked)
				guiPendingOverrideEvent = LA_ENDUIOUTURNLOCK;
			
			HandleTacticalUI( );

			InitPlayerUIBar( TRUE );
			//AddTopMessage( PLAYER_INTERRUPT_MESSAGE, Message[STR_INTERRUPT] );

			PlayJA2Sample( ENDTURN_1, RATE_11025, MIDVOLUME, 1, MIDDLEPAN );

			// report any close call quotes for us here
			for ( SoldierID id = gTacticalStatus.Team[ gbPlayerNum ].bFirstID; id <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++id )
			{
				TacticalActor *pSoldier =
					GetJa2SoldierRepository().resolve(id.i);
				if (!pSoldier)
				{
					continue;
				}

				if ( OK_INSECTOR_MERC( pSoldier ) )
				{
					if ( pSoldier->suppression().closeCall() )
					{
						if ( pSoldier->combatResult().hitsThisTurn() == 0 && !pSoldier->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL) && Random( 3 ) == 0 )
						{
							// say close call quote!
							TacticalCharacterDialogue( pSoldier, QUOTE_CLOSE_CALL );
							pSoldier->dialogue().markSaidExtended(SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL);
						}
						pSoldier->suppression().clearCloseCall();
					}
				}
			}
		}
	}
	else
	{
		// start interrupts for everyone on that side at once... and start AI with the lowest # guy

		// what we do is set everyone to moved except for people with interrupts at the moment
		/*
		cnt = gTacticalStatus.Team[ pSoldier->roster().team() ].bFirstID;
		for each repository-owned soldier on pSoldier's team
		{
			if ( pTempSoldier->roster().active() )
			{
				pTempSoldier->turnState().captureMoved(pTempSoldier->turnState().moved());
				pTempSoldier->turnState().moved() = TRUE;
			}
		}
		*/

		//while( 1 )
		UINT16 usCounter;
		for( usCounter = 0; usCounter < MAX_NUM_SOLDIERS; usCounter++ )
		{
			pInterrupter =
				GetJa2SoldierRepository().resolve(
					ubInterrupter.i);
			if (!pInterrupter)
			{
				break;
			}
			pInterrupter->turnState().moved() = FALSE;

			DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("INTERRUPT: popping %d off of the interrupt queue", ubInterrupter ) );

			REMOVE_LATEST_INTERRUPT_GUY();
			// now LATEST_INTERRUPT_GUY is the guy before the previous
			ubInterrupter = LATEST_INTERRUPT_GUY;
			if (ubInterrupter == NOBODY) // previously emptied slot!
			{
				continue;
			}
			else if (!(pInterrupter =
				GetJa2SoldierRepository().resolve(
					ubInterrupter.i)) ||
				pInterrupter->roster().team() != bTeam)
			{
				break;
			}
			else if (ubInterrupter < ubFirstInterrupter)
			{
				ubFirstInterrupter = ubInterrupter;
			}
		}

		// here we have to rebuilt the AI list!
		BuildAIListForTeam( bTeam );

		// set to the new first interrupter
		SoldierID id = RemoveFirstAIListEntry();

		// sevenfm: RemoveFirstAIListEntry() can return NOBODY
		if( id != NOBODY )
		{
			pTempSoldier =
				GetJa2SoldierRepository().resolve(id.i);

			// sevenfm: don't do anything if pTempSoldier is NULL
			if( pTempSoldier != NULL )
			{
				// SANDRO - we don't use the "hidden interrupt" feature with IIS
				// sevenfm: all interrupts in original interrupt system start as hidden and revealed later if soldier decides something
				if ( !is_networked && pTempSoldier->roster().team() != OUR_TEAM && !UsingImprovedInterruptSystem() )
				{
					// we're being interrupted by the computer!
					// we delay displaying any interrupt message until the computer does something...
					gfHiddenInterrupt = TRUE;
					gTacticalStatus.fUnLockUIAfterHiddenInterrupt = FALSE;
				}
				// otherwise it's the AI interrupting another AI team

				SetJa2TacticalCurrentTeam( pTempSoldier->roster().team() );

#ifdef JA2BETAVERSION
				if (is_networked)
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_TESTVERSION, L"Interrupt ( could be hidden )" );
				}
#endif

				// sevenfm: don't show hidden interrupt
				if( !gfHiddenInterrupt )
				{
					// SANDRO - show correct top message
					if (pTempSoldier->roster().team() == MILITIA_TEAM )
						AddTopMessage( MILITIA_INTERRUPT_MESSAGE, Message[STR_INTERRUPT] );
					else
						AddTopMessage( COMPUTER_INTERRUPT_MESSAGE, Message[STR_INTERRUPT] );
				}

				// Flugente 12-11-13: I observed an instance where the pTempSoldier was a player merc, leading to a deadlock here. The reason was that during an interrupt by a civilian, he somehow did not win
				// instead the game used the last merc entry... which overflowed, and thus started at merc 0, which is always a player merc
				// I am not sure if this is the best solution... however it seems to work for me.
				// If anybody knows a better solution, feel free to do so
				if ( pTempSoldier->roster().team() != OUR_TEAM )
					StartNPCAI( pTempSoldier );
				else
					EndInterrupt(TRUE);
			}
		}
	}

	if ( !gfHiddenInterrupt )
	{
		// Stop this guy....
		TacticalActor* latestInterrupter =
			GetJa2SoldierRepository().resolve(
				LATEST_INTERRUPT_GUY);
		if ( LATEST_INTERRUPT_GUY != END_OF_INTERRUPTS // BOB: is this just a blank?
			&& latestInterrupter
			)
		{
			latestInterrupter->AdjustNoAPToFinishMove( TRUE );
			latestInterrupter->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
		}
	}

	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"StartInterrupt done");
}

void EndInterrupt( BOOLEAN fMarkInterruptOccurred )
{
	SoldierID	ubInterruptedSoldier;
	TacticalActor *pSoldier;
	TacticalActor *pTempSoldier;
	BOOLEAN		fFound;
	INT16	ubMinAPsToAttack;

	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"EndInterrupt");

	for ( UINT16 cnt = gubOutOfTurnPersons; cnt > 0; cnt-- )
	{
		DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("ENDINT:	Q position %d: %d", cnt, gubOutOfTurnOrder[ cnt ] ) );
	}

	// ATE: OK, now if this all happended on one frame, we may not have to stop
	// guy from walking... so set this flag to false if so...
	if ( fMarkInterruptOccurred )
	{
		// flag as true if an int occurs which ends an interrupt (int loop)
		gTacticalStatus.fInterruptOccurred = TRUE;
	}
	else
	{
		gTacticalStatus.fInterruptOccurred = FALSE;
	}

	// Loop through all mercs and see if any passed on this interrupt
	SoldierID id = gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bFirstID;
	for ( ; id <= gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bLastID; ++id )
	{
		pTempSoldier =
			GetJa2SoldierRepository().resolve(id.i);
		if (!pTempSoldier)
		{
			continue;
		}
		if ( pTempSoldier->roster().active() && pTempSoldier->roster().inSector() && !pTempSoldier->turnState().moved() && (pTempSoldier->actionPoints().current() == pTempSoldier->turnState().interruptStartActionPoints()))
		{
			ubMinAPsToAttack = MinAPsToAttack( pTempSoldier, pTempSoldier->targeting().lastGridNo(), FALSE, 0 );
			if ( (ubMinAPsToAttack <= pTempSoldier->actionPoints().current()) && (ubMinAPsToAttack > 0) )
			{
				pTempSoldier->turnState().passedLastInterrupt() = TRUE;
			}
		}
	}

	if ( !EveryoneInInterruptListOnSameTeam() )
	{
		gfHiddenInterrupt = FALSE;

		// resume interrupted interrupt
		//hayden
		if ( !is_networked )
		{
			StartInterrupt();
		}
		else
		{
			SoldierID 	nubFirstInterrupter;
			INT8			nbTeam;
			TacticalActor *npSoldier;

			nubFirstInterrupter = LATEST_INTERRUPT_GUY;
			npSoldier =
				GetJa2SoldierRepository().resolve(
					nubFirstInterrupter.i);
			if (!npSoldier)
			{
				ClearIntList();
				return;
			}
			nbTeam = npSoldier->roster().team();

			//pSoldier is interrupted //but its not available //needs calculating
			//npSoldier,nbTeam is interruptor
			//hayden

#ifdef BETAVERSION
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"unchecked interrupt call area:(resume interrupted interrupt)..." );
#endif

			if ( (nbTeam > 0) && (nbTeam < 6) && is_server ) // AI interrupt resume and im server
			{
				send_interrupt( npSoldier );
				StartInterrupt();
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Continuing interrupt with %s and AI", TeamNameStrings[npSoldier->roster().team()] );//tried to use pSoldier, but its not available. find another way to get correct team

			}
			else if ( is_server && GetJa2TacticalCurrentTeam() == 1 )// resume AI interrupted and im server
			{
				//hayden
				send_interrupt( npSoldier );

				if ( nbTeam != 0 )
					intAI( npSoldier );
				else
					StartInterrupt();

				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Continuing interrupt of AI by %s", TeamNameStrings[npSoldier->roster().team()] );

			}

#ifdef	INTERRUPT_MP_DEADLOCK_FIX
			//its our turn//else// pure client awarding interrupt resume //its our turn
			else if ( GetJa2TacticalCurrentTeam() == 0 )
#else
			// pure client awarding interrupt resume
			else
#endif
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Continuing interrupt with %s", TeamNameStrings[npSoldier->roster().team()] );//this can be simplified if above comment is implemented
				//ClearIntList();
				//hayden//may need more work.
				StartInterrupt();
				send_interrupt( npSoldier ); //
			}
		}
	}
	else
	{
		ubInterruptedSoldier = LATEST_INTERRUPT_GUY;

		DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("INTERRUPT: interrupt over, %d's team regains control", ubInterruptedSoldier ) );

		pSoldier =
			GetJa2SoldierRepository().resolve(
				ubInterruptedSoldier.i);
		if (!pSoldier)
		{
			ClearIntList();
			return;
		}

		for ( SoldierID id = 0; id < MAX_NUM_SOLDIERS; ++id)
		{
			pTempSoldier =
				GetJa2SoldierRepository().resolve(id.i);
			if (!pTempSoldier)
			{
				continue;
			}
			if ( pTempSoldier->roster().active() )
			{
				// AI guys only here...
				if ( pTempSoldier->actionPoints().current() == 0 )
				{
					pTempSoldier->turnState().moved() = TRUE;
				}
				else if ( pTempSoldier->roster().team() != gbPlayerNum && pTempSoldier->aiBehavior().newSituation() == IS_NEW_SITUATION )
				{
					pTempSoldier->turnState().moved() = FALSE;
				}
				else
				{
					pTempSoldier->turnState().moved() = pTempSoldier->turnState().movedBeforeInterrupt();
				}
			}
		}


		// change team
		SetJa2TacticalCurrentTeam( pSoldier->roster().team() );

		// MP: tell the interrupted player's machine the interrupt is over. The
		// complete handshake for this (end_interrupt -> "endINTERRUPT" relay ->
		// resume_turn -> EndInterrupt) has existed since 1.13 MP was written but
		// the sender was never called from anywhere -- so the moving side stayed
		// frozen forever re-detecting interrupts (the historical "press ALT+E on
		// the server" hang). Only the holder sends: the interrupted soldier is on
		// another player's team here.
		if ( is_networked && is_client && pSoldier->roster().team() != gbPlayerNum )
		{
			end_interrupt( fMarkInterruptOccurred );
		}

		// switch appropriate messages & flags
		if ( pSoldier->roster().team() == OUR_TEAM)
		{
			// set everyone on the team to however they were set moved before the interrupt
			// must do this before selecting soldier...
			/*
			cnt = gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bFirstID;
			for each repository-owned soldier on the current team
			{
				if ( pTempSoldier->roster().active() )
				{
					pTempSoldier->turnState().moved() = pTempSoldier->turnState().movedBeforeInterrupt();
				}
			}
			*/

			ClearIntList();

			// Select soldier....
			if ( pSoldier->vitals().health() < OKLIFE )
			{
				DebugMsg(TOPIC_JA2INTERRUPT,DBG_LEVEL_3,String("EndInterrupt: SelectNextAvailSoldier"));
				SelectNextAvailSoldier( pSoldier );
			}
			else
			{
				SelectSoldier( ubInterruptedSoldier, FALSE, FALSE );
			}

			if (gfHiddenInterrupt)
			{
				TacticalActor* selectedSoldier =
					GetJa2SoldierRepository().resolve(
						gusSelectedSoldier.i);
				// Try to make things look like nothing happened at all.
				gfHiddenInterrupt = FALSE;

				// If we can continue a move, do so!
				if ( selectedSoldier &&
					selectedSoldier->movement().outOfActionPoints() &&
					pSoldier->movement().stopReason() != REASON_STOPPED_SIGHT )
				{
					// Continue
					selectedSoldier->AdjustNoAPToFinishMove( FALSE );

					if ( selectedSoldier->position().gridNo() != selectedSoldier->pathing().finalDestinationGrid() )
					{
						selectedSoldier->EVENT_GetNewSoldierPath( selectedSoldier->pathing().finalDestinationGrid(), selectedSoldier->movement().mode() );
					}
					else
					{
						UnSetUIBusy( pSoldier->identity().id() );
					}
				}
				else
				{
					UnSetUIBusy( pSoldier->identity().id() );
				}

				if ( gTacticalStatus.fUnLockUIAfterHiddenInterrupt )
				{
					gTacticalStatus.fUnLockUIAfterHiddenInterrupt = FALSE;
					UnSetUIBusy( pSoldier->identity().id() );
				}
			}
			else
			{
				// Signal UI done enemy's turn
				/// ATE: This used to be ablow so it would get done for
				// both hidden interrupts as well - NOT good because
				// hidden interrupts should leave it locked if it was already...
				guiPendingOverrideEvent = LU_ENDUILOCK;
				
				if (is_networked)
					guiPendingOverrideEvent = LA_ENDUIOUTURNLOCK;
				
				HandleTacticalUI( );

				if ( gusSelectedSoldier != NOBODY )
				{
					SlideTo( gusSelectedSoldier, SETLOCATOR);

					// Say ATTENTION SOUND...
					TacticalActor* selectedSoldier =
						GetJa2SoldierRepository().resolve(
							gusSelectedSoldier.i);
					if (selectedSoldier)
					{
						selectedSoldier->DoMercBattleSound( BATTLE_SOUND_ATTN1 );
					}

					if ( gsInterfaceLevel == 1 )
					{
						gTacticalStatus.uiFlags |= SHOW_ALL_ROOFS;
						InvalidateWorldRedundency( );
						SetRenderFlags(RENDER_FLAG_FULL);
						ErasePath(FALSE);
					}
				}
				// 2 indicates that we're ending an interrupt and going back to
				// normal player's turn without readjusting time left in turn (for
				// timed turns)
				InitPlayerUIBar( 2 );
				
				// SANDRO - shouldn't we unset ui here too?
				UnSetUIBusy( pSoldier->identity().id() );
			}

		}
		else if (!is_networked || (pSoldier->roster().team() < 6))//hayden : is Ai or LAN ?
		{
			// this could be set to true for AI-vs-AI interrupts
			gfHiddenInterrupt = FALSE;

			// Dirty panel interface!
			fInterfacePanelDirty = DIRTYLEVEL2;

			// Erase path!
			ErasePath( TRUE );

			// Reset flags
			gfPlotNewMovement = TRUE;

			// restart AI with first available soldier
			fFound = FALSE;

			// rebuild list for this team if anyone on the team is still available
			SoldierID id = gTacticalStatus.Team[ ENEMY_TEAM ].bFirstID;
			for ( ; id <= gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bLastID; ++id )
			{
				pTempSoldier =
					GetJa2SoldierRepository().resolve(id.i);
				if (!pTempSoldier)
				{
					continue;
				}
				if ( pTempSoldier->roster().active() && pTempSoldier->roster().inSector() && pTempSoldier->vitals().health() >= OKLIFE )
				{
					fFound = TRUE;
					break;
				}
			}

			if ( fFound )
			{
				// reset found flag because we are rebuilding the AI list
				fFound = FALSE;

				if ( BuildAIListForTeam( GetJa2TacticalCurrentTeam() ) )
				{
					// now bubble up everyone left in the interrupt queue, starting
					// at the front of the array
					for (UINT16 cnt = 1; cnt <= gubOutOfTurnPersons; cnt++)
					{
						MoveToFrontOfAIList( gubOutOfTurnOrder[ cnt ] );
					}

					SoldierID id = RemoveFirstAIListEntry();
					if (id != NOBODY)
					{
						TacticalActor* firstAiSoldier =
							GetJa2SoldierRepository().resolve(id.i);
						if (firstAiSoldier)
						{
							fFound = TRUE;
							StartNPCAI( firstAiSoldier );
						}
					}
				}

			}

			if (fFound)
			{
				// back to the computer!
				if ( GetJa2TacticalCurrentTeam() == CREATURE_TEAM && BloodcatsPresent() )
				{
					AddTopMessage( COMPUTER_TURN_MESSAGE, Message[ STR_BLOODCATS_TURN ] );
				}
				else
				{
					AddTopMessage( COMPUTER_TURN_MESSAGE, TeamTurnString[ GetJa2TacticalCurrentTeam() ] );
				}

				// Signal UI done enemy's turn
				if(is_client)
				{
					guiPendingOverrideEvent = LA_BEGINUIOURTURNLOCK;
				}
				else 
				{
					guiPendingOverrideEvent = LU_BEGINUILOCK;
				}

				ClearIntList();
			}
			else
			{
				// back to the computer!
				if ( GetJa2TacticalCurrentTeam() == CREATURE_TEAM && BloodcatsPresent() )
				{
					AddTopMessage( COMPUTER_TURN_MESSAGE, Message[ STR_BLOODCATS_TURN ] );
				}
				else
				{
					AddTopMessage( COMPUTER_TURN_MESSAGE, TeamTurnString[ GetJa2TacticalCurrentTeam() ] );
				}

				// Signal UI done enemy's turn
					if(is_client)
					{
						guiPendingOverrideEvent = LA_BEGINUIOURTURNLOCK;
					}
					else 
					{
						guiPendingOverrideEvent = LU_BEGINUILOCK;
					}

				// must clear int list before ending turn
				ClearIntList();
				EndAITurn();
			}
		}

		else if (is_networked) //its going to another Lan client..//hayden
		{
	
			SetJa2TacticalCurrentTeam( pSoldier->roster().team() );
			AddTopMessage( COMPUTER_TURN_MESSAGE, TeamTurnString[ GetJa2TacticalCurrentTeam() ] );
			if(is_client)
			{
				guiPendingOverrideEvent = LA_BEGINUIOURTURNLOCK;
			}
			else 
			{
				guiPendingOverrideEvent = LU_BEGINUILOCK;
			}

			// must clear int list before ending turn
			ClearIntList();

		}
		// Reset our interface!
		fInterfacePanelDirty = DIRTYLEVEL2;

	}

	if ( gGameSettings.fOptions[TOPTION_AUTO_FAST_FORWARD_MODE] )
	{
		if (is_networked)
		{
			// Only allow fast forward mode on enemy team!
			SetFastForwardMode( (GetJa2TacticalCurrentTeam() == ENEMY_TEAM) );
		}
		else
		{
			// Allow fast forward mode on all teams except our team!
			SetFastForwardMode( (GetJa2TacticalCurrentTeam() != OUR_TEAM) );
		}	
	}
}


BOOLEAN StandardInterruptConditionsMet( TacticalActor * pSoldier, SoldierID ubOpponentID, INT8 bOldOppList)
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"StandardInterruptConditionsMet");
//	UINT8 ubAniType;
	INT16						ubMinPtsNeeded;
	INT8						bDir;
	TacticalActor *		pOpponent;

	// Server-arbitrated interrupts (ja2server is the authority): interrupt DETECTION
	// is re-enabled in MP; the client requests the interrupt and the coordinator
	// grants at most one at a time, so the out-of-turn stack can't diverge. To go
	// back to interrupt-free combat, restore `if (is_networked) return FALSE;` here.

	if ( IsJa2TacticalTurnBasedCombat() && !(gubSightFlags & SIGHT_INTERRUPT) )
	{
		return( FALSE );
	}

	if ( GetJa2PendingTacticalCombatActions() > 0 )
	{
		return( FALSE );
	}

	if (ubOpponentID < TOTAL_SOLDIERS)
	{
		/*
		// only the OPPONENT'S controller's decision matters
		if (the opponent's controller is not this network peer)
		{
			return(FALSE);
		}
		*/

		// ALEX
		// if interrupts are restricted to a particular opponent only & he's not it
		if ((InterruptOnlyGuynum != NOBODY) && (ubOpponentID != InterruptOnlyGuynum))
		{
			return(FALSE);
		}

		pOpponent =
			GetJa2SoldierRepository().resolve(
				ubOpponentID.i);
		if (!pOpponent)
		{
			return FALSE;
		}
	}
	else	// no opponent, so controller of 'ptr' makes the call instead
	{
		// ALEX
		if (gsWhoThrewRock >= TOTAL_SOLDIERS)
		{
#ifdef BETAVERSION
			NumMessage("StandardInterruptConditions: ERROR - ubOpponentID is NOBODY, don't know who threw rock, guynum = ",pSoldier->guynum);
#endif

			return(FALSE);
		}

		// the machine that controls the guy who threw the rock makes the decision
		/*
		if (the thrower's controller is not this network peer)
			return(FALSE);
		*/
		pOpponent = NULL;
	}

	// if interrupts have been disabled for any reason
	if (!InterruptsAllowed)
	{
		return(FALSE);
	}

	// in non-combat allow interrupt points to be calculated freely (everyone's in control!)
	// also allow calculation for storing in AllTeamsLookForAll
	if ( (IsJa2TacticalCombatActive()) && ( gubBestToMakeSightingSize != BEST_SIGHTING_ARRAY_SIZE_ALL_TEAMS_LOOK_FOR_ALL ) )
	{
		// if his team's already in control
		if (pSoldier->roster().team() == GetJa2TacticalCurrentTeam() )
		{
			// if this is a player's a merc or civilian
			if ((pSoldier->status().flags() & SOLDIER_PC) || PTR_CIVILIAN)
			{
				// then they are not allowed to interrupt their own team
				return(FALSE);
			}
			else
			{
				// enemies, MAY interrupt each other, but NOT themselves!
				//if ( pSoldier->status().flags() & SOLDIER_UNDERAICONTROL )
				//{
					return(FALSE);
				//}
			}

		// CJC, July 9 1998
			// NO ONE EVER interrupts his own team
			//return( FALSE );
		}
		else if ( gTacticalStatus.bBoxingState != NOT_BOXING )
		{
			// while anything to do with boxing is going on, skip interrupts!
			return( FALSE );
		}

	}

	if ( !(pSoldier->roster().active()) || !(pSoldier->roster().inSector() ) )
	{
		return( FALSE );
	}

	// soldiers at less than OKLIFE can't perform any actions
	if (pSoldier->vitals().health() < OKLIFE)
	{
		return(FALSE);
	}

	// soldiers out of breath are about to fall over, no interrupt
	if (pSoldier->vitals().breath() < OKBREATH || pSoldier->collapseState().tactical() )
	{
		return(FALSE);
	}

	// if soldier doesn't have enough APs
	if ( pSoldier->actionPoints().current() < APBPConstants[MIN_APS_TO_INTERRUPT] )
	{
		return( FALSE );
	}

	// soldiers gagging on gas are too busy about holding their cookies down...
	if ( pSoldier->status().flags() & SOLDIER_GASSED )
	{
		return(FALSE);
	}

	// a soldier already engaged in a life & death battle is too busy doing his
	// best to survive to worry about "getting the jump" on additional threats
	if (pSoldier->suppression().underFire())
	{
		return(FALSE);
	}

	if (pSoldier->collapseState().tactical())
	{
		return( FALSE );
	}

	// don't allow neutral folks to get interrupts
	if (pSoldier->aiBehavior().neutral())
	{
		return( FALSE );
	}

	// no EPCs allowed to get interrupts
	if ( AM_AN_EPC( pSoldier ) && !AM_A_ROBOT( pSoldier ) )
	{
		return( FALSE );
	}


	// don't let mercs on assignment get interrupts
	if ( pSoldier->roster().team() == gbPlayerNum && pSoldier->assignment().current() >= ON_DUTY)
	{
		return( FALSE );
	}


	// the bare minimum default is enough APs left to TURN
	ubMinPtsNeeded = APBPConstants[AP_CHANGE_FACING];

	// if the opponent is SOMEBODY
	if (ubOpponentID < TOTAL_SOLDIERS)
	{
		// if the soldiers are on the same side
		if (pSoldier->roster().side() == pOpponent->roster().side())
		{
			// human/civilians on same side can't interrupt each other
			if ((pSoldier->status().flags() & SOLDIER_PC) || PTR_CIVILIAN)
			{
				return(FALSE);
			}
			else	// enemy
			{
				// enemies can interrupt EACH OTHER, but enemies and civilians on the
				// same side (but different teams) can't interrupt each other.
				if (pSoldier->roster().team() != pOpponent->roster().team())
				{
					return(FALSE);
				}
			}
		}

		// if the interrupted opponent is not the selected character, then the only
		// people eligible to win an interrupt are those on the SAME SIDE AS
		// the selected character, ie. his friends...
		if ( pOpponent->roster().team() == gbPlayerNum )
		{
			TacticalActor* selectedSoldier =
				GetJa2SoldierRepository().resolve(
					gusSelectedSoldier.i);
			if ((ubOpponentID != gusSelectedSoldier) &&
				(!selectedSoldier ||
				 pSoldier->roster().side() != selectedSoldier->roster().side()))
			{
				return( FALSE );
			}
		}
		else
		{
			if (!is_networked) {
				if ( !(pOpponent->status().flags() & SOLDIER_UNDERAICONTROL) && (pSoldier->roster().side() != pOpponent->roster().side()))
				{
					return( FALSE );
				}
			} else {
				if ( !(is_client || (pOpponent->status().flags() & SOLDIER_UNDERAICONTROL)) && (pSoldier->roster().side() != pOpponent->roster().side()))
				{
					return( FALSE );
				}
			}

		}
		/* old DG code for same:

		if ((ubOpponentID != gusSelectedSoldier) && (pSoldier->roster().side() != gusSelectedSoldier->roster().side()))
		{
			return(FALSE);
		}
		*/

		// an non-active soldier can't interrupt a soldier who is also non-active!
		if ((pOpponent->roster().team() != GetJa2TacticalCurrentTeam()) && (pSoldier->roster().team() != GetJa2TacticalCurrentTeam()))
		{
			return(FALSE);
		}


		// if this is a "SEEING" interrupt
		if (pSoldier->awareness().opponentKnowledge()[ubOpponentID] == SEEN_CURRENTLY)
		{
			// if pSoldier already saw the opponent last "look" or at least this turn
			if ((bOldOppList == SEEN_CURRENTLY) || (bOldOppList == SEEN_THIS_TURN))
			{
				return(FALSE);	 // no interrupt is possible
			}

			// if the soldier is behind him and not very close, forget it
			bDir = atan8( pSoldier->position().worldXInt(), pSoldier->position().worldYInt(), pOpponent->position().worldXInt(), pOpponent->position().worldYInt() );
			if ( gOppositeDirection[ pSoldier->pathing().desiredDirection() ] == bDir )
			{
				// directly behind; allow interrupts only within # of tiles equal to level
				if ( PythSpacesAway( pSoldier->position().gridNo(), pOpponent->position().gridNo() ) > EffectiveExpLevel( pSoldier ) )
				{
					return( FALSE );
				}
			}

			// if the soldier isn't currently crouching
			if (!PTR_CROUCHED)
			{
				ubMinPtsNeeded = GetAPsCrouch(pSoldier, TRUE); // Changed from APBPConstants[AP_CROUCH] - SANDRO
			}
			else
			{
				ubMinPtsNeeded = MinPtsToMove(pSoldier);
			}
		}
		else	// this is a "HEARING" interrupt
		{
			// if the opponent can't see the "interrupter" either, OR
			// if the "interrupter" already has any opponents already in sight, OR
			// if the "interrupter" already heard the active soldier this turn
			if ((pOpponent->awareness().opponentKnowledge()[pSoldier->identity().id()] != SEEN_CURRENTLY) || (pSoldier->awareness().opponentCount() > 0) || (bOldOppList == HEARD_THIS_TURN))
			{
				return(FALSE);	 // no interrupt is possible
			}
		}
	}


	// soldiers without sufficient APs to do something productive can't interrupt
	if (pSoldier->actionPoints().current() < ubMinPtsNeeded)
	{
		return(FALSE);
	}

	// soldier passed on the chance to react during previous interrupt this turn
	if (pSoldier->turnState().passedLastInterrupt())
	{
#ifdef RECORDNET
		fprintf(NetDebugFile,"\tStandardInterruptConditionsMet: FAILING because PassedLastInterrupt %d(%s)\n",
			pSoldier->guynum,ExtMen[pSoldier->guynum].name);
#endif

//		return(FALSE);
	}


#ifdef RECORDINTERRUPT
	// this usually starts a new series of logs, so that's why the blank line
	fprintf(InterruptFile,"\nStandardInterruptConditionsMet by %d vs. %d\n",pSoldier->guynum,ubOpponentID);
#endif

	return(TRUE);
}


INT8 CalcInterruptDuelPts( TacticalActor * pSoldier, SoldierID ubOpponentID, BOOLEAN fUseWatchSpots )
{
	INT32 iPoints;
	INT8 bLightLevel;
	UINT8	ubDistance;
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"CalcInterruptDuelPts");

	// sevenfm: safety check
	Assert(pSoldier);
	TacticalActor* opponent =
		GetJa2SoldierRepository().resolve(ubOpponentID.i);
	if (!pSoldier || !opponent)
	{
		return NO_INTERRUPT;
	}

	// extra check to make sure neutral folks never get interrupts
	if (pSoldier->aiBehavior().neutral())
	{
		return( NO_INTERRUPT );
	}

	// Old: BASE is one point for each experience level.
	// Snap: Agility should be a factor, since it is a measure of
	// a person's reactions.	We'll give more weight to experience
	// though, since combat initiative is too important to be left up
	// to an ordinary skill like agility.
	// BASE = (2*lev + agi/10) / 3
	// Robot has interrupt points based on the controller...
	// Controller's interrupt points are reduced by 2 for being distracted...
	if ( pSoldier->status().flags() & SOLDIER_ROBOT && pSoldier->CanRobotBeControlled( ) )
	{
		TacticalActor* controller =
			GetJa2SoldierRepository().resolve(
				pSoldier->vehicleState().robotRemoteHolder().i);
		if (!controller)
		{
			return NO_INTERRUPT;
		}
		// Snap: (do some proper rounding here)
		iPoints = ( 20*EffectiveExpLevel( controller )
			+ EffectiveAgility( controller, FALSE ) + 15 ) / 30 - 2;
	}
	else
	{
		//iPoints = EffectiveExpLevel( pSoldier );
		// Snap:
		iPoints = ( 20*EffectiveExpLevel( pSoldier ) + EffectiveAgility( pSoldier, FALSE ) + 15 ) / 30;

		/*
		if ( pSoldier->roster().team() == ENEMY_TEAM )
		{
			// modify by the difficulty level setting
			iPoints += gbDiff[ DIFF_ENEMY_INTERRUPT_MOD ][ SoldierDifficultyLevel( pSoldier ) ];
			iPoints = __max( iPoints, 9 );
		}
		*/

		if ( pSoldier->ControllingRobot( ) )
		{
			iPoints -= 2;
		}
	}

	// sevenfm: no watch spot bonus when focusing
	if (fUseWatchSpots && !(pSoldier->featureFlags().secondaryFlags() & SOLDIER_TRAIT_FOCUS))
	{
		// if this is a previously noted spot of enemies, give bonus points!
		iPoints += GetWatchedLocPoints( pSoldier->identity().id(), opponent->position().gridNo(), opponent->position().level() );
	}

	// LOSE one point for each 2 additional opponents he currently sees, above 2
	if (pSoldier->awareness().opponentCount() > 2)
	{
		// subtract 1 here so there is a penalty of 1 for seeing 3 enemies
		iPoints -= (pSoldier->awareness().opponentCount() - 1) / 2;
	}

	// LOSE one point if he's trying to interrupt only by hearing
	if (pSoldier->awareness().opponentKnowledge()[ubOpponentID] == HEARD_THIS_TURN)
	{
		iPoints--;
	}

	//hayden, multiplayer add advantage for a ready'd reapon
	if(is_networked)
	{
		if ( ( gAnimControl[ pSoldier->animationPlayback().state() ].uiFlags &( ANIM_FIREREADY | ANIM_FIRE ) ))
		{
			iPoints=(iPoints + cWeaponReadyBonus);
			
		}
	}

	// if soldier is still in shock from recent injuries, that penalizes him
	iPoints -= pSoldier->suppression().shock();

	ubDistance = (UINT8) PythSpacesAway( pSoldier->position().gridNo(), opponent->position().gridNo() );

	// if we are in combat mode - thus doing an interrupt rather than determine who gets first turn -
	// then give bonus
	if ( (IsJa2TacticalCombatActive()) && (pSoldier->roster().team() != GetJa2TacticalCurrentTeam()) )
	{
		// passive player gets penalty due to range
		iPoints -= (ubDistance / 10);
	}
	else
	{
		// either non-combat or the player with the current turn... i.e. active...
		// unfortunately we can't use opplist here to record whether or not we saw this guy before, because at this point
		// the opplist has been updated to seen.	But we can use gbSeenOpponents ...

		// this soldier is moving, so give them a bonus for crawling or swatting at long distances
		if ( !gbSeenOpponents[ ubOpponentID ][ pSoldier->identity().id() ] )
		{
			if (pSoldier->animationPlayback().state() == SWATTING && ubDistance > (MaxNormalDistanceVisible() / 2) ) // more than 1/2 sight distance
			{
				iPoints++;
			}
			else if (pSoldier->animationPlayback().state() == CRAWLING && ubDistance > (MaxNormalDistanceVisible() / 4) ) // more than 1/4 sight distance
			{
				iPoints += ubDistance / STRAIGHT;
			}
		}
	}

	// whether active or not, penalize people who are running
	if ( pSoldier->animationPlayback().state() == RUNNING && !gbSeenOpponents[ pSoldier->identity().id() ][ ubOpponentID ] )
	{
		iPoints -= 2;
	}

	if (pSoldier->service().hasPartner())
	{
		// distracted by being bandaged/doing bandaging
		iPoints -= 2;
	}

	if (gGameOptions.fNewTraitSystem) // new/old traits check - SANDRO
	{
		if ( HAS_SKILL_TRAIT( pSoldier, NIGHT_OPS_NT ) )
		{
			bLightLevel = LightTrueLevel(pSoldier->position().gridNo(), pSoldier->position().level());
			if (bLightLevel > NORMAL_LIGHTLEVEL_DAY + 3)
			{
				// it's dark, give a bonus for interrupts
				iPoints += gSkillTraitValues.ubNOIterruptsBonusInDark;
			}
		}

		// Phlegmatics get a small penalty to interrupts
		if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_PHLEGMATIC ) )
		{
			iPoints -= 1;
		}

		// Flugente: focus skill
		if ( (pSoldier->featureFlags().secondaryFlags() & SOLDIER_TRAIT_FOCUS) )
		{
			if ( pSoldier->CanUseSkill( SKILLS_FOCUS, FALSE, pSoldier->skillState().focusGrid() ) )
			{
				// if target is in focus, increase interrupt chance, otherwise lower it
				// radius depends on range
				INT16 range = PythSpacesAway( pSoldier->skillState().focusGrid(), pSoldier->position().gridNo() );
				INT16 radius = gSkillTraitValues.ubSNFocusRadius * range / 20;

				INT16 range_opponent = PythSpacesAway( pSoldier->skillState().focusGrid(), opponent->position().gridNo() );

				if ( range_opponent <= radius )
					iPoints += gSkillTraitValues.sSNFocusInterruptBonus;
				else
					iPoints -= 2 * gSkillTraitValues.sSNFocusInterruptBonus;
			}
			else
			{
				pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_TRAIT_FOCUS;
				pSoldier->skillState().clearFocus();
			}
		}
	}
	else
	{
		if ( HAS_SKILL_TRAIT( pSoldier, NIGHTOPS_OT ) )
		{
			bLightLevel = LightTrueLevel(pSoldier->position().gridNo(), pSoldier->position().level());
			if (bLightLevel > NORMAL_LIGHTLEVEL_DAY + 3)
			{
				// it's dark, give a bonus for interrupts
				iPoints += 1 * NUM_SKILL_TRAITS( pSoldier, NIGHTOPS_OT );
			}
		}
	}
		
	// Flugente: interrupt modifier from special stats
	iPoints += pSoldier->GetInterruptModifier( ubDistance );

	// if he's a computer soldier

	// CJC note: this will affect friendly AI as well...

	if ( pSoldier->status().flags() & SOLDIER_PC )
	{
		if ( pSoldier->assignment().current() >= ON_DUTY )
		{
			// make sure don't get interrupts!
			iPoints = -10;
		}

		// GAIN one point if he's previously seen the opponent
		// check for TRUE because -1 means we JUST saw him (always so here)
		if (gbSeenOpponents[pSoldier->identity().id()][ubOpponentID] == TRUE)
		{
			iPoints++;	// seen him before, easier to react to him
		}
	}
	else if ( pSoldier->roster().team() == ENEMY_TEAM )
	{
		// GAIN one point if he's previously seen the opponent
		// check for TRUE because -1 means we JUST saw him (always so here)
		if (gbSeenOpponents[pSoldier->identity().id()][ubOpponentID] == TRUE)
		{
			iPoints++;	// seen him before, easier to react to him
		}
		else if (gbPublicOpplist[pSoldier->roster().team()][ubOpponentID] != NOT_HEARD_OR_SEEN)
		{
			// GAIN one point if opponent has been recently radioed in by his team
			iPoints++;
		}
	}

	if ( ARMED_VEHICLE( pSoldier ) || ENEMYROBOT( pSoldier ) )
	{
		// reduce interrupt possibilities for tanks!
		iPoints /= 2;
	}

	if (iPoints >= AUTOMATIC_INTERRUPT)
	{
#ifdef BETAVERSION
		NumMessage("CalcInterruptDuelPts: ERROR - Invalid bInterruptDuelPts calculated for soldier ",pSoldier->guynum);
#endif
		iPoints = AUTOMATIC_INTERRUPT - 1;	// hack it to one less than max so its legal
	}

	#ifdef DEBUG_INTERRUPTS
		DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("Calculating int pts for %d vs %d, number is %d", pSoldier->identity().id(), ubOpponentID, iPoints ) );
	#endif
	if(is_networked)
	{
		TacticalActor	*pOpp = opponent;
		#ifdef JA2BETAVERSION
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_MPSYSTEM, L"Interrupt: '%s' vs '%s' = %d points.",pSoldier->identity().name(),pOpp->identity().name(), iPoints );
		#endif
	}
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"CalcInterruptDuelPts done");
	return( (INT8)iPoints );
}

BOOLEAN InterruptDuel( TacticalActor * pSoldier, TacticalActor * pOpponent)
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"InterruptDuel");
	BOOLEAN fResult = FALSE;

	// sevenfm: if Ctrl+D pressed - skip all player interrupts for this turn
	if( !is_networked && !UsingImprovedInterruptSystem() && pSoldier->roster().team() == OUR_TEAM && gTacticalStatus.ubDisablePlayerInterrupts )
		return FALSE;

	// if opponent can't currently see us and we can see them
	if ( pSoldier->awareness().opponentKnowledge()[ pOpponent->identity().id() ] == SEEN_CURRENTLY && pOpponent->awareness().opponentKnowledge()[pSoldier->identity().id()] != SEEN_CURRENTLY )
	{
		fResult = TRUE;		// we automatically interrupt
		// fix up our interrupt duel pts if necessary
		if (pSoldier->turnState().interruptDuelPoints() < pOpponent->turnState().interruptDuelPoints())
		{
			pSoldier->turnState().interruptDuelPoints() = pOpponent->turnState().interruptDuelPoints();
		}
	}
	else
	{
		// If our total points is HIGHER, then we interrupt him anyway
		if (pSoldier->turnState().interruptDuelPoints() > pOpponent->turnState().interruptDuelPoints())
		{
			fResult = TRUE;
		}
	}
//	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Interrupt duel %d (%d pts) vs %d (%d pts)", pSoldier->identity().id(), pSoldier->turnState().interruptDuelPoints(), pOpponent->identity().id(), pOpponent->turnState().interruptDuelPoints() );
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"InterruptDuel done");
	return( fResult );
}


void DeleteFromIntList( UINT16 ubIndex, BOOLEAN fCommunicate)
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"DeleteFromIntList");
	UINT16 ubLoop;
	UINT16 ubID;

	if ( ubIndex > gubOutOfTurnPersons)
	{
		return;
	}

	// remember who we're getting rid of
	ubID = gubOutOfTurnOrder[ubIndex];

	#ifdef DEBUG_INTERRUPTS
		DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("INTERRUPT: removing ID %d", ubID ) );
	#endif
//	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"%d removed from int list", ubID );
	// if we're NOT deleting the LAST entry in the int list
	if (ubIndex < gubOutOfTurnPersons)
	{
		// not the last entry, must move all those behind it over to fill the gap
		for (ubLoop = ubIndex; ubLoop < gubOutOfTurnPersons; ubLoop++)
		{
			gubOutOfTurnOrder[ubLoop] = gubOutOfTurnOrder[ubLoop + 1];
		}
	}

	// either way, whack the last entry to NOBODY and decrement the list size
	gubOutOfTurnOrder[gubOutOfTurnPersons] = NOBODY;
	gubOutOfTurnPersons--;

	// once the last interrupted guy gets deleted from the list, he's no longer
	// the last interrupted guy!
	/*
	if (Status.lastInterruptedWas == ubID)
	{
		Status.lastInterruptedWas = NOBODY;
	}
	*/
}

void AddToIntList( UINT16 ubID, BOOLEAN fGainControl, BOOLEAN fCommunicate )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"AddToIntList");
	UINT16 ubLoop;
	TacticalActor* soldier =
		GetJa2SoldierRepository().resolve(ubID);
	if (!soldier)
	{
		return;
	}

//	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"%d added to int list", ubID );
	DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("INTERRUPT: adding ID %d who %s", ubID, fGainControl ? "gains control" : "loses control" ) );

	// check whether 'who' is already anywhere on the queue after the first index
	// which we want to preserve so we can restore turn order
	for (ubLoop = 2; ubLoop <= gubOutOfTurnPersons; ubLoop++)
	{
		if (gubOutOfTurnOrder[ubLoop] == ubID)
		{
			if (!fGainControl)
			{
				// he's LOSING control; that's it, we're done, DON'T add him to the queue again
				gubLastInterruptedGuy = ubID;
				return;
			}
			else
			{
				// GAINING control, so delete him from this slot (because later he'll
				// get added to the end and we don't want him listed more than once!)
				DeleteFromIntList( ubLoop, FALSE );
			}
		}
	}

	// increment total (making index valid) and add him to list
	if ( gubOutOfTurnPersons + 1 >= MAXMERCS )   // never grow past the fixed-size queue (OOB write)
		return;
	gubOutOfTurnPersons++;
	gubOutOfTurnOrder[gubOutOfTurnPersons] = ubID;

/*
	// the guy being interrupted HAS to be the currently selected character
	if (Status.lastInterruptedWas != CharacterSelected)
	{
		// if we don't already do so, remember who that was
		Status.lastInterruptedWas = CharacterSelected;
	}
*/

	// if the guy is gaining control
	if (fGainControl)
	{
		// record his initial APs at the start of his interrupt at this time
		// this is not the ideal place for this, but it's the best I could do...
		soldier->turnState().interruptStartActionPoints() = soldier->actionPoints().current();
	}
	else
	{
		gubLastInterruptedGuy = ubID;
		// turn off AI control flag if they lost control
		if (soldier->status().flags() & SOLDIER_UNDERAICONTROL)
		{
			DebugAI( String( "Taking away AI control from %d", ubID ) );
			soldier->status().flags() &= (~SOLDIER_UNDERAICONTROL);
		}
	}
}

void VerifyOutOfTurnOrderArray()
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"VerifyOutOfTurnOrderArray");
	UINT16		ubTeamHighest[ MAXTEAMS ] = { 0 };
	UINT8		ubTeamsInList;
	UINT16		ubNextInArrayOnTeam, ubNextIndex;
	UINT8		ubTeam;
	UINT16 ubLoop, ubLoop2;
	BOOLEAN	fFoundLoop = FALSE;

	for (ubLoop = 1; ubLoop <= gubOutOfTurnPersons; ubLoop++)
	{
		TacticalActor* queuedSoldier =
			GetJa2SoldierRepository().resolve(
				gubOutOfTurnOrder[ubLoop]);
		if ( !queuedSoldier )   // skip sentinel/garbage entries from save or wire data
			continue;
		ubTeam = queuedSoldier->roster().team();
		if ( ubTeam >= MAXTEAMS )   // corrupt bTeam would overrun the MAXTEAMS-sized ubTeamHighest[]
			continue;
		if (ubTeamHighest[ ubTeam ] > 0)
		{
			// check the other teams to see if any of them are between our last team's mention in
			// the array and this
			for (ubLoop2 = 0; ubLoop2 < MAXTEAMS; ubLoop2++)
			{
				if (ubLoop2 == ubTeam)
				{
					continue;
				}
				else
				{
					if (ubTeamHighest[ ubLoop2 ] > ubTeamHighest[ ubTeam ])
					{
						// there's a loop!! delete it!
						ubNextInArrayOnTeam = gubOutOfTurnOrder[ ubLoop ];
						ubNextIndex = ubTeamHighest[ ubTeam ] + 1;

						while( gubOutOfTurnOrder[ ubNextIndex ] != ubNextInArrayOnTeam )
						{
							TacticalActor* interruptedSoldier =
								GetJa2SoldierRepository().resolve(
									gubOutOfTurnOrder[ubNextIndex]);
							// Pause them...
							if (interruptedSoldier)
							{
								interruptedSoldier->AdjustNoAPToFinishMove( TRUE );

								// If they were turning from prone, stop them
								interruptedSoldier->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
							}

							DeleteFromIntList( ubNextIndex, FALSE );
						}

						fFoundLoop = TRUE;
						break;
					}
				}
			}

			if (fFoundLoop)
			{
				// at this point we should restart our outside loop (ugh)
				fFoundLoop = FALSE;
				for (ubLoop2 = 0; ubLoop2 < MAXTEAMS; ubLoop2++)
				{
					ubTeamHighest[ ubLoop2 ] = 0;
				}
				ubLoop = 0;
				continue;

			}

		}

		ubTeamHighest[ ubTeam ] = ubLoop;
	}

	// Another potential problem: the player is interrupted by the enemy who is interrupted by
	// the militia.	In this situation the enemy should just lose their interrupt.
	// (Or, the militia is interrupted by the enemy who is interrupted by the player.)

	// Check for 3+ teams in the interrupt queue.	If three exist then abort all interrupts (return
	// control to the first team)
	ubTeamsInList = 0;
	for ( ubLoop = 0; ubLoop < MAXTEAMS; ubLoop++ )
	{
		if ( ubTeamHighest[ ubLoop ] > 0 )
		{
			ubTeamsInList++;
		}
	}
	if ( ubTeamsInList >= 3 )
	{
		// This is bad.	Loop through everyone but the first person in the INT list and remove 'em
		for (ubLoop = 2; ubLoop <= gubOutOfTurnPersons; )
		{
			TacticalActor* queuedSoldier =
				GetJa2SoldierRepository().resolve(
					gubOutOfTurnOrder[ubLoop]);
			TacticalActor* firstSoldier =
				GetJa2SoldierRepository().resolve(
					gubOutOfTurnOrder[1]);
			if ( !queuedSoldier || !firstSoldier )
			{
				ClearIntList();
				return;
			}
			if ( queuedSoldier->roster().team() != firstSoldier->roster().team() )
			{
				// remove!

				// Pause them...
				queuedSoldier->AdjustNoAPToFinishMove( TRUE );

				// If they were turning from prone, stop them
				queuedSoldier->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;

				DeleteFromIntList( ubLoop, FALSE );

				// since we deleted someone from the list, we want to check the same index in the
				// array again, hence we DON'T increment.
			}
			else
			{
				ubLoop++;
			}
		}
	}

}

void DoneAddingToIntList( TacticalActor * pSoldier, BOOLEAN fChange, UINT8 ubInterruptType)
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"DoneAddingToIntList");
	if (fChange)
	{
		VerifyOutOfTurnOrderArray();
		if ( EveryoneInInterruptListOnSameTeam() )
		{
			EndInterrupt( TRUE );
		}
		else
		{
			if (!is_networked) 
			{
				StartInterrupt();
			} 
			else 
			{
				UINT16						nubFirstInterrupter;
				INT8						nbTeam;
				TacticalActor *				npSoldier;
						
				nubFirstInterrupter = LATEST_INTERRUPT_GUY;
				npSoldier =
					GetJa2SoldierRepository().resolve(
						nubFirstInterrupter);
				if (!npSoldier)
				{
					ClearIntList();
					return;
				}
				nbTeam = npSoldier->roster().team();

				//pSoldier is interrupted
				//npSoldier is interruptor
				//hayden

				// INTERRUPT is calculated on the server
				if ((nbTeam > 0) && (nbTeam <6 ) && is_server) //is for AI and are server
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"%s is interrupt by AI", TeamNameStrings[pSoldier->roster().team()]);
					
					// Only display the top message if we (the server) got interrupted
					if (pSoldier->roster().team() == 0)
						AddTopMessage( COMPUTER_INTERRUPT_MESSAGE, TeamTurnString[ nbTeam ] );

					send_interrupt( npSoldier );
					StartInterrupt();

				}
				// INTERRUPT is calculated on the server
				else if(is_server && GetJa2TacticalCurrentTeam() == 1)//  against ai and are server
				{
					//hayden
					send_interrupt( npSoldier ); //
					if(nbTeam !=0)
						intAI(npSoldier);						
					else 
						StartInterrupt();//
					
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"AI is interrupted by %s", TeamNameStrings[npSoldier->roster().team()]);
				}
				// INTERRUPT is calculated on the pure client
				else if(GetJa2TacticalCurrentTeam() == 0)//its our turn (we are moving)
				{																	
#ifdef	INTERRUPT_MP_DEADLOCK_FIX
					// Do nothing
#else
					if (cGameType == MP_TYPE_COOP)
						ScreenMsg( FONT_MCOLOR_LTRED, MSG_INTERFACE, MPClientMessage[79]);
#endif

					send_interrupt( npSoldier );


					TacticalActor* pMerc =
						GetJa2SoldierRepository().resolve(
							gusSelectedSoldier.i);
					//AdjustNoAPToFinishMove( pMerc, TRUE );	
					if (pMerc)
					{
						pMerc->HaultSoldierFromSighting(TRUE);
					}
					//pMerc->fTurningFromPronePosition = FALSE;// hmmm ??
					FreezeInterfaceForEnemyTurn();
					InitEnemyUIBar( 0, 0 );
					fInterfacePanelDirty = DIRTYLEVEL2;
					AddTopMessage( COMPUTER_INTERRUPT_MESSAGE, TeamTurnString[ nbTeam ] );
					gTacticalStatus.fInterruptOccurred = TRUE;

					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"You have interrupted %s", TeamNameStrings[npSoldier->roster().team()]);
				}
				// Coordinator MP: OUR merc (nbTeam==0) gets the interrupt during the
				// opponent's turn. There is no is_server host to award it, so REQUEST it
				// from the coordinator and WAIT -- do NOT take control locally. We act on
				// the server's recieveINTERRUPT grant (it grants one at a time, so turn
				// ownership can't diverge). Without this a pure client just ClearIntList'd
				// and interrupts never fired over the standalone server.
				else if ( is_networked && !is_server && nbTeam == 0 )
				{
					mp_log_soldier( npSoldier, "REQUESTING interrupt from the server" );
					send_interrupt( npSoldier );
				}
				else
				{
					ClearIntList();//no interrupt to be awarded, clear generated list.
				}
			}
		}
	}
}

void ResolveInterruptsVs( TacticalActor * pSoldier, UINT8 ubInterruptType)
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,String("ResolveInterruptsVs: Soldier ID = %d, APs = %d (interrupt type = %d)",pSoldier->identity().id(),pSoldier->actionPoints().current(), ubInterruptType));
	UINT8 ubTeam;
	SoldierID ubOpp;
	UINT16 ubIntCnt;
	UINT16 ubIntList[MAXMERCS];
	UINT8 ubIntDiff[MAXMERCS];
	UINT8 ubSmallestDiff;
	UINT16 ubSlot, ubSmallestSlot;
	UINT16 ubLoop;
	BOOLEAN fIntOccurs;
	TacticalActor * pOpponent;
	BOOLEAN fControlChanged = FALSE;

	AssertNotNIL(pSoldier);

	if ( IsJa2TacticalTurnBasedCombat() )
	{
		ubIntCnt = 0;

		for (ubTeam = 0; ubTeam < MAXTEAMS; ubTeam++)
		{
			// WDS fix broken interrupts (I hope...)
			if (/*gTacticalStatus.Team[ubTeam].bTeamActive &&*/ (gTacticalStatus.Team[ubTeam].bSide != pSoldier->roster().side()) && ubTeam != CIV_TEAM)
			{
				for ( ubOpp = gTacticalStatus.Team[ ubTeam ].bFirstID; ubOpp <= gTacticalStatus.Team[ ubTeam ].bLastID; ++ubOpp)
				{
					pOpponent =
						GetJa2SoldierRepository().resolve(ubOpp.i);
					AssertNotNIL(pOpponent);
					if (!pOpponent)
					{
						continue;
					}
					if ( pOpponent->roster().active() && pOpponent->roster().inSector() && (pOpponent->vitals().health() >= OKLIFE) && (pOpponent->vitals().breath() >= OKBREATH) && !(pOpponent->collapseState().tactical()) )
					{
						if ( ubInterruptType == NOISEINTERRUPT )
						{
							// don't grant noise interrupts at greater than max. visible distance
							if ( PythSpacesAway( pSoldier->position().gridNo(), pOpponent->position().gridNo() ) > MaxNormalDistanceVisible() )
							{
								pOpponent->turnState().interruptDuelPoints() = NO_INTERRUPT;

								DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("Resetting int pts for %d - NOISE BEYOND SIGHT DISTANCE!?", pOpponent->identity().id() ) );

								continue;
							}
						}
						else if ( pOpponent->awareness().opponentKnowledge()[pSoldier->identity().id()] != SEEN_CURRENTLY )
						{
							pOpponent->turnState().interruptDuelPoints() = NO_INTERRUPT;

							DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("Resetting int pts for %d - DOESN'T SEE ON SIGHT INTERRUPT!?", pOpponent->identity().id() ) );


							continue;
						}

						switch (pOpponent->turnState().interruptDuelPoints())
						{
							case NO_INTERRUPT:		// no interrupt possible, no duel necessary
								DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("ResolveInterruptsVs: No interrupt for opponent %d", pOpponent->identity().id() ) );
								fIntOccurs = FALSE;
								break;

							case AUTOMATIC_INTERRUPT:	// interrupts occurs automatically
								pSoldier->turnState().interruptDuelPoints() = 0;	// just to have a valid intDiff later
								fIntOccurs = TRUE;

								DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("INTERRUPT: automatic interrupt on %d by %d", pSoldier->identity().id(), pOpponent->identity().id() ) );

								break;

							default:		// interrupt is possible, run a duel
								DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, "Calculating int duel pts for onlooker in ResolveInterruptsVs" );
								pSoldier->turnState().interruptDuelPoints() = CalcInterruptDuelPts(pSoldier, pOpponent->identity().id(), TRUE);
								fIntOccurs = InterruptDuel(pOpponent,pSoldier);
								#ifdef DEBUG_INTERRUPTS
								if (fIntOccurs)
								{
									DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("INTERRUPT: standard interrupt on %d (%d pts) by %d (%d pts)", pSoldier->identity().id(), pSoldier->turnState().interruptDuelPoints(), pOpponent->identity().id(), pOpponent->turnState().interruptDuelPoints()) );
								}
								#endif

								break;
						}

						if (fIntOccurs)
						{
							// remember that this opponent's scheduled to interrupt us
							ubIntList[ubIntCnt] = pOpponent->identity().id();

							// and by how much he beat us in the duel
							ubIntDiff[ubIntCnt] = pOpponent->turnState().interruptDuelPoints() - pSoldier->turnState().interruptDuelPoints();

							// increment counter of interrupts lost
							ubIntCnt++;
						}
						else
						{
						/*
							if (pOpponent->turnState().interruptDuelPoints() != NO_INTERRUPT)
							{
								ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"%d fails to interrupt %d (%d vs %d pts)", pOpponent->identity().id(), pSoldier->identity().id(), pOpponent->turnState().interruptDuelPoints(), pSoldier->turnState().interruptDuelPoints());
							}
							*/
						}

						// either way, clear out both sides' bInterruptDuelPts field to prepare next one

						if (pSoldier->turnState().interruptDuelPoints() != NO_INTERRUPT)
						{
							DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("Resetting int pts for %d", pSoldier->identity().id() ) );
						}


						pSoldier->turnState().interruptDuelPoints() = NO_INTERRUPT;


						if (pOpponent->turnState().interruptDuelPoints() != NO_INTERRUPT)
						{
							DebugMsg( TOPIC_JA2INTERRUPT, DBG_LEVEL_3, String("Resetting int pts for %d", pOpponent->identity().id() ) );
						}

						pOpponent->turnState().interruptDuelPoints() = NO_INTERRUPT;

					}

				}
			}
		}

		// if any interrupts are scheduled to occur (ie. I lost at least once)
		if (ubIntCnt)
		{
			// First add currently active character to the interrupt queue.	This is
			// USUALLY pSoldier->guynum, but NOT always, because one enemy can
			// "interrupt" on another enemy's turn if he hears another team's wound
			// victim's screaming...	the guy screaming is pSoldier here, it's not his turn!
			//AddToIntList( (UINT8) gusSelectedSoldier, FALSE, TRUE);

			if ( (GetJa2TacticalCurrentTeam() != pSoldier->roster().team()) && !(gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bHuman) )
			{
				// if anyone on this team is under AI control, remove
				// their AI control flag and put them on the queue instead of this guy
				for ( SoldierID id = gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bFirstID; id <= gTacticalStatus.Team[ GetJa2TacticalCurrentTeam() ].bLastID; ++id)
				{
					TacticalActor* controlledSoldier =
						GetJa2SoldierRepository().resolve(id.i);
					if ( controlledSoldier &&
						controlledSoldier->status().flags() & SOLDIER_UNDERAICONTROL)
					{
						// this guy lost control
						controlledSoldier->status().flags() &= (~SOLDIER_UNDERAICONTROL);
						AddToIntList( id, FALSE, TRUE);
						break;
					}
				}
			}
			else
			{
				// this guy lost control
				AddToIntList( pSoldier->identity().id(), FALSE, TRUE);
			}

			// loop once for each opponent who interrupted
			for (ubLoop = 0; ubLoop < ubIntCnt; ubLoop++)
			{
				// find the smallest intDiff still remaining in the list
				ubSmallestDiff = NO_INTERRUPT;
				ubSmallestSlot = NOBODY;

				for (ubSlot = 0; ubSlot < ubIntCnt; ubSlot++)
				{
					if (ubIntDiff[ubSlot] < ubSmallestDiff)
					{
						ubSmallestDiff = ubIntDiff[ubSlot];
						ubSmallestSlot = ubSlot;
					}
				}

				if (ubSmallestSlot < TOTAL_SOLDIERS)
				{
					// add this guy to everyone's interrupt queue
					AddToIntList(ubIntList[ubSmallestSlot],TRUE,TRUE);
					// SANDRO - for IIS, reset counter if we got here
					if ( UsingImprovedInterruptSystem() )
					{
						// reset the counter
						TacticalActor* interrupter =
							GetJa2SoldierRepository().resolve(
								ubIntList[ubSmallestSlot]);
						if (interrupter)
						{
							interrupter->turnState().interruptCounters()[pSoldier->identity().id()] = 0;
						}
					}
					if (INTERRUPTS_OVER)
					{
						// a loop was created which removed all the people in the interrupt queue!
						EndInterrupt( TRUE );
						return;
					}

					ubIntDiff[ubSmallestSlot] = NO_INTERRUPT;		// mark slot as been handled
				}
			}

			fControlChanged = TRUE;
		}

		// sends off an end-of-list msg telling everyone whether to switch control,
		// unless it's a MOVEMENT interrupt, in which case that is delayed til later
		DoneAddingToIntList(pSoldier,fControlChanged,ubInterruptType);
	}
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"ResolveInterruptsVs done");
}


BOOLEAN	SaveTeamTurnsToTheSaveGameFile( HWFILE hFile )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"SaveTeamTurnsToTheSaveGameFile");
	UINT32	uiNumBytesWritten;
	TEAM_TURN_SAVE_STRUCT TeamTurnStruct;

	//Save the gubTurn Order Array
	FileWrite( hFile, gubOutOfTurnOrder, sizeof( UINT16 ) * MAXMERCS, &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( UINT16 ) * MAXMERCS )
	{
		return( FALSE );
	}


	TeamTurnStruct.ubOutOfTurnPersons = gubOutOfTurnPersons;

	TeamTurnStruct.InterruptOnlyGuynum = InterruptOnlyGuynum;
	TeamTurnStruct.sWhoThrewRock = gsWhoThrewRock;
	TeamTurnStruct.InterruptsAllowed = InterruptsAllowed;
	TeamTurnStruct.fHiddenInterrupt = gfHiddenInterrupt;
	TeamTurnStruct.ubLastInterruptedGuy = gubLastInterruptedGuy;


	//Save the Team turn save structure
	FileWrite( hFile, &TeamTurnStruct, sizeof( TEAM_TURN_SAVE_STRUCT ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( TEAM_TURN_SAVE_STRUCT ) )
	{
		return( FALSE );
	}

	return( TRUE );
}

BOOLEAN	LoadTeamTurnsFromTheSavedGameFile( HWFILE hFile )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"LoadTeamTurnsFromTheSavedGameFile");
	UINT32	uiNumBytesRead;
	TEAM_TURN_SAVE_STRUCT TeamTurnStruct;

	//Load the gubTurn Order Array
	FileRead( hFile, gubOutOfTurnOrder, sizeof( UINT16 ) * MAXMERCS, &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( UINT16 ) * MAXMERCS )
	{
		return( FALSE );
	}


	//Load the Team turn save structure
	FileRead( hFile, &TeamTurnStruct, sizeof( TEAM_TURN_SAVE_STRUCT ), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( TEAM_TURN_SAVE_STRUCT ) )
	{
		return( FALSE );
	}

	gubOutOfTurnPersons = TeamTurnStruct.ubOutOfTurnPersons;
	// Corrupt/tampered save: an out-of-turn count past the fixed queue would OOB-index
	// Drop the whole interrupt queue rather than trust invalid downstream slot data.
	if ( gubOutOfTurnPersons >= MAXMERCS )
		ClearIntList();

	InterruptOnlyGuynum = TeamTurnStruct.InterruptOnlyGuynum;
	gsWhoThrewRock = TeamTurnStruct.sWhoThrewRock;
	InterruptsAllowed = TeamTurnStruct.InterruptsAllowed;
	gfHiddenInterrupt = TeamTurnStruct.fHiddenInterrupt;
	gubLastInterruptedGuy = TeamTurnStruct.ubLastInterruptedGuy;


	return( TRUE );
}

BOOLEAN NPCFirstDraw( TacticalActor * pSoldier, TacticalActor * pTargetSoldier )
{
	DebugMsg (TOPIC_JA2INTERRUPT,DBG_LEVEL_3,"NPCFirstDraw");
	// if attacking an NPC check to see who draws first!

	if ( pTargetSoldier->identity().profile() != NO_PROFILE && pTargetSoldier->identity().profile() != SLAY && pTargetSoldier->aiBehavior().neutral() && pTargetSoldier->awareness().opponentKnowledge()[ pSoldier->identity().id() ] == SEEN_CURRENTLY && (	FindAIUsableObjClass( pTargetSoldier, IC_WEAPON ) != NO_SLOT ) )
	{
		UINT8	ubLargerHalf, ubSmallerHalf, ubTargetLargerHalf, ubTargetSmallerHalf;

		// roll the dice!
		// e.g. if level 5, roll Random( 3 + 1 ) + 2 for result from 2 to 5
		// if level 4, roll Random( 2 + 1 ) + 2 for result from 2 to 4
		ubSmallerHalf = EffectiveExpLevel( pSoldier ) / 2;
		ubLargerHalf = EffectiveExpLevel( pSoldier ) - ubSmallerHalf;

		ubTargetSmallerHalf = EffectiveExpLevel( pTargetSoldier ) / 2;
		ubTargetLargerHalf = EffectiveExpLevel( pTargetSoldier ) - ubTargetSmallerHalf;
		if ( gMercProfiles[ pTargetSoldier->identity().profile() ].bApproached & gbFirstApproachFlags[ APPROACH_THREATEN - 1 ] )
		{
			// gains 1 to 2 points
			ubTargetSmallerHalf += 1;
			ubTargetLargerHalf += 1;
		}
		if ( Random( ubTargetSmallerHalf + 1) + ubTargetLargerHalf > Random( ubSmallerHalf + 1) + ubLargerHalf )
		{
			return( TRUE );
		}
	}
	return( FALSE );
}
