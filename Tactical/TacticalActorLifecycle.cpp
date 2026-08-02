#include "TacticalActorLifecycle.h"

#include "TacticalActorStateFlags.h"

#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorAppearance.h"
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


extern INT16 DirIncrementer[8];

bool TacticalActorLifecycle::create(TacticalActor& subject, UINT8 ubBodyType, SoldierID usSoldierID, UINT16 usState)
{
	if (ubBodyType >= TOTALBODYTYPES ||
		usState >= NUMANIMATIONSTATES ||
		static_cast<std::size_t>(usSoldierID.i) >=
			GetJa2SoldierRepository().capacity())
	{
		return false;
	}

	BOOLEAN fSuccess = FALSE;

	//if we are loading a saved game, we DO NOT want to reset the opplist, look for enemies, or say a dying commnet
	if ( !(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) )
	{
		// Set initial values for opplist!
		InitSoldierOppList( &subject );
		HandleSight( &subject, SIGHT_LOOK );

		// Set some quote flags
		if ( subject.vitals().health() >= OKLIFE )
		{
			subject.dialogue().clearDyingComment();
		}
		else
		{
			subject.dialogue().markDyingCommentSpoken();
		}
	}

	// ATE: Reset some timer flags...
	subject.dialogue().repeatedBattleSoundAt() = 0;
	// ATE: Reset every time.....
	subject.movement().syncPresentationMotion(true);
	subject.audio().clearTurningSound();
	subject.vitals().lastBleedGruntAt() = 0;

	if ( subject.identity().bodyType() == QUEENMONSTER )
	{
		subject.audio().startPositionSound(
			NewPositionSnd( NOWHERE, POSITION_SOUND_FROM_SOLDIER,
				(UINT32)subject.identity().id(), QUEEN_AMBIENT_NOISE, 15 ) );
	}


	// ANYTHING AFTER HERE CAN FAIL
	do
	{

		if ( usSoldierID <= gTacticalStatus.Team[OUR_TEAM].bLastID )
		{
			subject.keyRing().activate();
		}
		else
		{
			subject.keyRing().deactivate();
		}
		// Initialize the allocation-free animation surface working set.
		subject.animationCache().initialize( usSoldierID );

		if ( !(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) )
		{
			// Init new soldier state
			// OFFSET FIRST ANIMATION FRAME FOR NEW MERCS
			if ( usState != STANDING )
			{
				TacticalActorAnimationTransitions::initializeAnimation(subject,  usState, (UINT8)0, TRUE );
			}
			else
			{
				TacticalActorAnimationTransitions::initializeAnimation(subject,  usState, (UINT8)Random( 10 ), TRUE );
			}
		}
		else
		{
			/// if we don't have a world loaded, and are in a bad anim, goto standing.
			// bad anims are: HOPFENCE,
			// CLIMBDOWNROOF, FALLFORWARD_ROOF,FALLOFF, CLIMBUPROOF
			if ( !IsJa2TacticalWorldLoaded() &&
				 (usState == HOPFENCE || usState == JUMPWINDOWS ||
				 usState == CLIMBDOWNROOF ||

				 usState == JUMPDOWNWALL ||
				 usState == JUMPUPWALL ||

				 usState == FALLFORWARD_ROOF ||
				 usState == FALLOFF ||
				 usState == CLIMBUPROOF) )
			{
				TacticalActorAnimationTransitions::initializeAnimation(subject,  STANDING, 0, TRUE );
			}
			else
			{
				TacticalActorAnimationTransitions::initializeAnimation(subject,  usState, subject.animationPlayback().code(), TRUE );
			}

		}


		// Init palettes
		if (!TacticalActorAppearance::rebuildPalettes(subject))
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_0, String( "Soldier: Failed in creating soldier palettes" ) );
			break;
		}

		fSuccess = TRUE;

	} while ( FALSE );

	if ( !fSuccess )
	{
		(void)TacticalActorLifecycle::destroy(subject);
	}

	return(fSuccess);

}



bool TacticalActorLifecycle::destroy(TacticalActor& subject)
{
	INT32			iGridNo;
	INT8			bDir;
	BOOLEAN		fRet;
	TacticalEntityId actor;

	{
		actor = GetJa2TacticalEntityId(subject);
		// Invalidate the exact incarnation before dismantling its legacy
		// resources. A late delete for a reused slot cannot remove its successor.
		(void)ReleaseJa2TacticalEntity(subject);

		if ( !TileIsOutOfBounds( subject.position().gridNo() ) )
		{
			// Remove adjacency records
			for ( bDir = 0; bDir < NUM_WORLD_DIRECTIONS; bDir++ )
			{
				iGridNo = subject.position().gridNo() + DirIncrementer[bDir];
				if ( iGridNo >= 0 && iGridNo < WORLD_MAX )
				{
					GetMapElement(
						static_cast<UINT32>(iGridNo)).ubAdjacentSoldierCnt--;
				}
			}
		}

		// Clear inline key-ring storage and its historical presence marker.
		subject.keyRing().deactivate();
		// Tear down any interrupted give/drop/reload/throw transaction.
		subject.pendingItem().reset();
		// Modular AI plans retain a back-reference to &subject exact record.
		subject.aiPlan().reset();
		// Strategic routes are owned by the record and must not survive slot
		// teardown or alias the next soldier incarnation.
		subject.strategicPath().reset();

		// Delete faces
		DeleteSoldierFace( &subject );

		// Release all registered palette storage and borrowed active aliases as
		// one owned graphics boundary.
		subject.palette().reset();


		if ( subject.identity().bodyType() == QUEENMONSTER )
		{
			DeletePositionSnd( subject.audio().positionSoundId() );
			subject.audio().clearPositionSound();
		}

		// Release any globally shared surfaces locked by this soldier slot and
		// clear its inline working set.
		subject.animationCache().release( subject.identity().id() );

		// Soldier is not active
		subject.roster().active() = FALSE;

		// Remove light
		(void)TacticalActorLighting::destroyPersonalLight(subject);

		// Remove reseved movement value
		UnMarkMovementReserved( &subject );

	}

	// REMOVE SOLDIER FROM SLOT!
	fRet = RemoveJa2ActiveTacticalActor(actor);

	if ( !fRet )
	{
		RemoveJa2AwayTacticalActor(actor);
	}
	// Tactical removal normally performs the strategic side effects first.
	// This exact-ID cleanup also covers teardown paths that release the record
	// directly, so a reused repository slot cannot inherit retained membership.
	(void)RemoveJa2StrategicSquadActor(actor);
	(void)RemoveJa2VehiclePassengerActor(actor);
	(void)RemovePlayerFromStrategicGroups(actor);

	return(TRUE);
}

// FUNCTIONS CALLED BY EVENT PUMP
/////////////////////////////////

void TacticalActorLifecycle::revive(TacticalActor& subject)
{
	INT16					sX, sY;

	if ( subject.vitals().health() < OKLIFE  && subject.roster().active() )
	{
		// If dead or unconscious, revive!
		subject.status().flags() &= (~SOLDIER_DEAD);

		subject.vitals().health() = subject.vitals().maximumHealth();
		subject.vitals().bleeding() = 0;
		subject.vitals().healableInjury() = 0; // added by SANDRO
		subject.animationIntent().desiredHeight() = ANIM_STAND;

		AddManToTeam( subject.roster().team() );

		// Set to standing
		subject.animationActivity().clearInterruptibility();

		// Change to standing,unless we can getup with an animation
		TacticalActorAnimationTransitions::initializeAnimation(subject,  STANDING, 0, TRUE );
		(void)TacticalActorRecovery::beginGetUp(subject);

		// Makesure center of tile
		ConvertGridNoToCenterCellXY(subject.position().gridNo(), &sX, &sY);

		(void)TacticalActorWorldPlacement::setPosition(subject, (FLOAT)sX, (FLOAT)sY );

		// Dirty INterface
		fInterfacePanelDirty = DIRTYLEVEL2;
	}
}
