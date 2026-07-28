	#include "sgp.h"
#include "TacticalWorldAdapter.h"
	#include "Game Clock.h"
	#include "Timer Control.h"
	#include "Overhead.h"
	#include "worlddef.h"
	#include "Rotting Corpses.h"
	#include "Tactical Turns.h"
	#include "Smell.h"
	#include "opplist.h"
	#include "Queen Command.h"
	#include "Dialogue Control.h"
	#include "SmokeEffects.h"
	#include "LightEffects.h"
	#include "Soldier macros.h"
	#include "Explosion Control.h"
#include "CampaignProfileCodes.h"
#include "GameContext.h"
#include "SoldierRepository.h"
#include "strategicmap.h"
#include "random.h"
#include "Reinforcement.h"


extern void DecayPublicOpplist( INT8 bTeam );

//not in overhead.h!
extern UINT16 NumEnemyInSector();

void HandleRPCDescription(	)
{
// WDS - make number of mercenaries, etc. be configurable
	std::vector<UINT16>	ubMercsInSector (CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS, 0);
//	UINT8	ubMercsInSector[ CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS ] = { 0 };
	UINT16	ubNumMercs = 0;
	UINT16	ubChosenMerc = 0;
	SOLDIERTYPE *pTeamSoldier;
	BOOLEAN fSAMSite = FALSE;


	if ( !gTacticalStatus.fCountingDownForGuideDescription )
	{
		return;
	}

	// ATE: postpone if we are not in tactical
	if ( GetCurrentScreen() != GAME_SCREEN )
	{
	return;
	}

	if ( ( gTacticalStatus.uiFlags & ENGAGED_IN_CONV ) )
	{
	return;
	}

	// Are we a SAM site?
	if ( gTacticalStatus.ubGuideDescriptionToUse == 27 ||
		gTacticalStatus.ubGuideDescriptionToUse == 30 ||
		gTacticalStatus.ubGuideDescriptionToUse == 32 ||
		gTacticalStatus.ubGuideDescriptionToUse == 25 ||
		gTacticalStatus.ubGuideDescriptionToUse == 31 )
	{
	 fSAMSite = TRUE;
	 gTacticalStatus.bGuideDescriptionCountDown = 1;
	}

	// ATE; Don't do in combat
	if ( ( IsJa2TacticalCombatActive() ) && !fSAMSite )
	{
		return;
	}

	// Don't do if enemy in sector
	if ( NumEnemyInSector( ) && !fSAMSite )
	{
		return;
	}
	
	gTacticalStatus.bGuideDescriptionCountDown--;

	if ( gTacticalStatus.bGuideDescriptionCountDown == 0 )
	{
		gTacticalStatus.fCountingDownForGuideDescription = FALSE;

		// OK, count how many rpc guys we have....
		// set up soldier ptr as first element in mercptrs list
		SoldierID cnt2 = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
		if (gTacticalStatus.ubGuideDescriptionToUse != 100)
		{
			// run through list
			for ( ; cnt2 <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++cnt2 )
			{
				pTeamSoldier =
					GetJa2SoldierRepository().resolve(cnt2.i);
				if (!pTeamSoldier)
				{
					continue;
				}
				// Add guy if he's a candidate...
				if ( RPC_RECRUITED( pTeamSoldier ) )
				{
					if ( pTeamSoldier->vitals().health() >= OKLIFE && pTeamSoldier->bActive &&
						pTeamSoldier->deployment().sectorX() == gTacticalStatus.bGuideDescriptionSectorX && pTeamSoldier->deployment().sectorY() == gTacticalStatus.bGuideDescriptionSectorY &&
						pTeamSoldier->deployment().sectorZ() == gbWorldSectorZ &&
						!pTeamSoldier->deployment().isBetweenSectors() )
					{
						const GameCampaign campaign =
							GetGameContext().capabilities().campaign;
						if ( CampaignProfileCode::matches(
								 campaign, CampaignProfileCode::Role::Ira,
								 pTeamSoldier->ubProfile) ||
							 CampaignProfileCode::matches(
								 campaign, CampaignProfileCode::Role::Miguel,
								 pTeamSoldier->ubProfile) ||
							 CampaignProfileCode::matches(
								 campaign, CampaignProfileCode::Role::Carlos,
								 pTeamSoldier->ubProfile) ||
							 CampaignProfileCode::matches(
								 campaign, CampaignProfileCode::Role::Dimitri,
								 pTeamSoldier->ubProfile) )
						{
							ubMercsInSector[ubNumMercs] = (UINT16)cnt2;
							++ubNumMercs;
						}
					}
				}
			}

			// If we are > 0
			if ( ubNumMercs > 0 )
			{
				ubChosenMerc = (UINT16)Random( ubNumMercs );

				SOLDIERTYPE* chosenMerc =
					GetJa2SoldierRepository().resolve(
						ubMercsInSector[ubChosenMerc]);
				if (chosenMerc)
				{
					TacticalCharacterDialogueWithSpecialEvent( chosenMerc, gTacticalStatus.ubGuideDescriptionToUse, DIALOGUE_SPECIAL_EVENT_USE_ALTERNATE_FILES, 0, 0 );
				}
			}
		}

		// Flugente: special value signifies lua-based quotes
		if ( !ubNumMercs || gTacticalStatus.ubGuideDescriptionToUse == 100 )
		{
			// run through list
			cnt2 = gTacticalStatus.Team[gbPlayerNum].bFirstID;
			for ( ; cnt2 <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++cnt2 )
			{
				pTeamSoldier =
					GetJa2SoldierRepository().resolve(cnt2.i);
				if (!pTeamSoldier)
				{
					continue;
				}
				if ( pTeamSoldier->vitals().health() >= OKLIFE && pTeamSoldier->bActive &&
					pTeamSoldier->deployment().sectorX() == gTacticalStatus.bGuideDescriptionSectorX && pTeamSoldier->deployment().sectorY() == gTacticalStatus.bGuideDescriptionSectorY &&
					pTeamSoldier->deployment().sectorZ() == gbWorldSectorZ &&
					!pTeamSoldier->deployment().isBetweenSectors() )
				{
					AdditionalTacticalCharacterDialogue_CallsLua( pTeamSoldier, ADE_SECTOR_COMMENTARY );
				}
			}
		}
	}
}

void HandleTacticalEndTurn( )
{
	SOLDIERTYPE		*pSoldier;
	UINT32				uiTime;
	static UINT32 uiTimeSinceLastStrategicUpdate = 0;

	// OK, Do a number of things here....
	// Every few turns......

	SetFastForwardMode(FALSE); // Cancel FF at end of battle
	SetClockSpeedPercent(gGameExternalOptions.fClockSpeedPercent);	// sevenfm: set default clock speed

	// Get time elasped
	uiTime = GetWorldTotalSeconds( );

	if ( ( uiTimeSinceLastStrategicUpdate - uiTime ) > 1200 )
	{
		HandleRottingCorpses( );
		//DecayTacticalMoraleModifiers();

	uiTimeSinceLastStrategicUpdate = uiTime;
	}

	DecayBombTimers( );
	
	DecayLightEffects( uiTime );
	DecaySmokeEffects( uiTime );

	// Decay smells
	//DecaySmells();

	// Decay blood
	DecayBloodAndSmells( uiTime );

	// decay AI warning values from corpses
	DecayRottingCorpseAIWarnings();

	HandleEnvironmentHazard( );

	if(gGameExternalOptions.gfAllowReinforcements)//dnl ch68 100913
	{
		if ( gTacticalStatus.Team[ENEMY_TEAM].bTeamActive || gfPendingNonPlayerTeam[ENEMY_TEAM] || 
			 gTacticalStatus.Team[MILITIA_TEAM].bTeamActive || gfPendingNonPlayerTeam[MILITIA_TEAM] )
			 ++guiTurnCnt;
		else
			guiTurnCnt = 0;

		//Check for enemy pooling (add enemies if there happens to be more than the max in the
		//current battle.	If one or more slots have freed up, we can add them now.
		AddPossiblePendingEnemiesToBattle();
		AddPossiblePendingMilitiaToBattle();
	}

	// Loop through each active team and decay public opplist...
	// May want this done every few times too
	NonCombatDecayPublicOpplist( uiTime );
	/*
	for( cnt = 0; cnt < MAXTEAMS; cnt++ )
	{
		if ( gTacticalStatus.Team[ cnt ].bMenInSector > 0 )
		{
			// decay team's public opplist
			DecayPublicOpplist( (INT8)cnt );
		}
	}
*/

	// First pass:
	// Loop through our own mercs:
	//	Check things like ( even if not in our sector )
	//		1 ) All updates of breath, shock, bleeding, etc
	//	2 ) Updating First AID, etc
	//	( If in our sector: )
	//		3 ) Update things like decayed opplist, etc

	// Second pass:
	//	Loop through all mercs in tactical engine
	//	If not a player merc ( ubTeam ) , do things like 1 , 2 , 3 above


	// First exit if we are not in realtime combat or realtime noncombat
	if (!(IsJa2TacticalTurnBased()) || !(IsJa2TacticalCombatActive() ) )
	{

		BeginLoggingForBleedMeToos( TRUE );

		SoldierID cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
		for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
		{
			pSoldier = GetJa2SoldierRepository().resolve(cnt.i);
			if (!pSoldier)
			{
				continue;
			}
			if ( pSoldier->bActive && pSoldier->vitals().health() > 0 && !( pSoldier->status().flags() & SOLDIER_VEHICLE ) )
			{
				// Handle everything from getting breath back, to bleeding, etc
				pSoldier->EVENT_BeginMercTurn( TRUE, 0 );

				if (!AM_A_ROBOT(pSoldier))
				{
					// Handle Player services
					HandlePlayerServices( pSoldier );

					// if time is up, turn off xray
					if ( pSoldier->perception().xrayActive() && uiTime > pSoldier->perception().xrayActivatedAt() + XRAY_TIME )
					{
						TurnOffXRayEffects( pSoldier );
					}

					// Handle stat changes if ness.
					//if ( fCheckStats )
					//{
					////	UpdateStats( pSoldier );
					//}
									
					// Flugente: update multi-turn actions
					pSoldier->UpdateMultiTurnAction();
				}
			}
		}

		BeginLoggingForBleedMeToos( FALSE );

		// OK, loop through the mercs to perform 'end turn' events on each...
		// We're looping through only mercs in tactical engine, ignoring our mercs
		// because they were done earilier...
		for ( UINT32 cnt = 0; cnt < guiNumMercSlots; cnt++ )
		{
			pSoldier = MercSlots[ cnt ];

			if ( pSoldier != NULL )
			{
				if ( pSoldier->bTeam != gbPlayerNum )
				{
					// Handle everything from getting breath back, to bleeding, etc
					pSoldier->EVENT_BeginMercTurn( TRUE, 0 );

					// Handle Player services
					HandlePlayerServices( pSoldier );
				}
			}
		}
	}
	if ( !GetGameContext().capabilities().isUnfinishedBusiness() )
	{
		HandleRPCDescription( );
	}

	// Flugente: Cool down/decay all items not in a soldier's inventory
	CoolDownWorldItems();

	// Flugente: raise zombies if in gamescreen and option set
	if ( GetCurrentScreen() == GAME_SCREEN )
	{
		RaiseZombies();
	}
}



