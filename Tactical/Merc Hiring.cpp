#include "TacticalActorAnimationTransitions.h"
	#include "builddefines.h"
	#include <stdio.h>
	#include "DEBUG.H"
	#include "math.h"
	#include "worlddef.h"
	#include "Assignments.h"
	#include "merc entering.h"
	#include "Animation Control.h"
	#include "Handle UI.h"
	#include <Font Control.h>
	#include "random.h"
	#include "Overhead.h"
#include "SoldierRepository.h"
	#include "Soldier Profile.h"
	#include "Game Clock.h"
	#include "Soldier Create.h"
	#include "Merc Hiring.h"
	#include "Game Event Hook.h"
	#include "message.h"
	#include "strategicmap.h"
	#include "strategic.h"
	#include "Items.h"
	#include "history.h"
	#include "Squads.h"
	#include "Strategic Merc Handler.h"
	#include "Dialogue Control.h"
	#include "Map Screen Interface.h"
	#include "Map Screen Interface Map.h"
	#include "screenids.h"
	#include "jascreens.h"
	#include "Text.h"
	#include "Merc Contract.h"
	#include "LaptopSave.h"
	#include "personnel.h"
	#include "Map Screen Interface Bottom.h"
	#include "Quests.h"
	#include "GameSettings.h"
	#include "DynamicDialogue.h"// added by Flugente
#include "GameContext.h"
#include "CampaignAimHire.h"
#include "CampaignAimArrival.h"
#include "StrategicGroupHost.h"
#include "Strategic Movement.h"
#include <array>
#include "StrategicSquadHost.h"
#include "Soldier Profile Constants.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "CampaignEventScheduling.h"
#include "Game Events.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include <limits>
#include "CampaignMercenaryArrivalContent.h"
#include "CampaignMercenaryPolicy.h"
#include "connect.h"
#include "Map Information.h"

	#include "TacticalActor.h"
	#include "TacticalActorEmploymentTypes.h"
	#include "TacticalActorStateFlags.h"
#include "Ja25 Strategic Ai.h"
#include "Ja25_Tactical.h"
#include "Campaign Types.h"
#include "MapScreen Quotes.h"
#include "opplist.h"
#include "Ja25Update.h"
#include "email.h"

//forward declarations of common classes to eliminate includes
class OBJECTTYPE;
class TacticalActor;


#define	MIN_FLIGHT_PREP_TIME	6

#ifdef JA2TESTVERSION
	BOOLEAN	gForceHireMerc=FALSE;
	void SetFlagToForceHireMerc( BOOLEAN fForceHire );
#endif

extern BOOLEAN		gfTacticalDoHeliRun;
extern BOOLEAN		gfFirstHeliRun;

// ATE: Globals that dictate where the mercs will land once being hired
// Default to Omerta
// Saved in general saved game structure
// HEADROCK HAM 3.5: Externalized coordinates
INT16 gsMercArriveSectorX = gGameExternalOptions.ubDefaultArrivalSectorX;
INT16 gsMercArriveSectorY = gGameExternalOptions.ubDefaultArrivalSectorY;

void CheckForValidArrivalSector( );
static bool CheckForValidArrivalSectorImpl(bool presentNotice);

void AddItemToMerc( UINT8 ubNewMerc, INT16 sItemType );

#define	NUM_INITIAL_GRIDNOS_FOR_HELI_CRASH		7

UINT32	gsInitialHeliGridNo[ NUM_INITIAL_GRIDNOS_FOR_HELI_CRASH ] =
{
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};

INT16	gsInitialHeliRandomTimes[ NUM_INITIAL_GRIDNOS_FOR_HELI_CRASH ] =
{
	0,
	0,
	0,
	0,
	0,
	0,
	0
};


UINT32		GetInitialHeliGridNo( );
UINT16	GetInitialHeliRandomTime();
static INT8 HireMercImpl( MERC_HIRE_STRUCT *pHireMerc,
	CampaignAimHireResult* checkedResult )
{
	const CampaignMercenaryPolicy mercenaryPolicy(
		GetGameContext().capabilities());
	TacticalActor	*pSoldier;
	SoldierID	iNewIndex;
	UINT8		ubCurrentSoldier = pHireMerc->ubProfileID;
	MERCPROFILESTRUCT				*pMerc;
	SOLDIERCREATE_STRUCT		MercCreateStruct;
	BOOLEAN fReturn = FALSE;
	pMerc = &gMercProfiles[ ubCurrentSoldier ];

	//If we are to disregard the ststus of the merc
	#ifdef JA2TESTVERSION
		if( !gForceHireMerc )
	#endif
	//If the merc is away, Dont hire him, or if the merc is only slightly annoyed at the player
	if( ( pMerc->bMercStatus != 0 ) && (pMerc->bMercStatus != MERC_ANNOYED_BUT_CAN_STILL_CONTACT ) && ( pMerc->bMercStatus != MERC_HIRED_BUT_NOT_ARRIVED_YET ) )
		return( MERC_HIRE_FAILED );


    //hayden, 7 member team limit setable in ini
	if( NumberOfMercsOnPlayerTeam() >= OUR_TEAM_SIZE_NO_VEHICLE || (is_client && NumberOfMercsOnPlayerTeam() >= cMaxMercs) )
		return( MERC_HIRE_OVER_PLAYER_LIMIT );

	// ATE: if we are to use landing zone, update to latest value
	// they will be updated again just before arrival...
	if ( pHireMerc->fUseLandingZoneForArrival )
	{
		pHireMerc->sSectorX	= gsMercArriveSectorX;
		pHireMerc->sSectorY	= gsMercArriveSectorY;
		pHireMerc->bSectorZ	= 0;
	}

	// BUILD STRUCTURES
	MercCreateStruct.initialize();
	MercCreateStruct.ubProfile						= ubCurrentSoldier;
	MercCreateStruct.fPlayerMerc					= TRUE;
	MercCreateStruct.sSectorX							= pHireMerc->sSectorX;
	MercCreateStruct.sSectorY							= pHireMerc->sSectorY;
	MercCreateStruct.bSectorZ							= pHireMerc->bSectorZ;
	MercCreateStruct.bTeam								= SOLDIER_CREATE_AUTO_TEAM;
	MercCreateStruct.fCopyProfileItemsOver= pHireMerc->fCopyProfileItemsOver;
	
	if(!cAllowMercEquipment && is_networked)
		MercCreateStruct.fCopyProfileItemsOver=0;//hayden : server overide

	// Construction can consume identity and mutate profile/record storage before
	// returning null. The checked authority must fail-stop from this point.
	if (checkedResult) checkedResult->mutationMayHaveStarted = true;
	if ( !TacticalCreateSoldier( &MercCreateStruct, &iNewIndex ) )
	{
		if (checkedResult) checkedResult->error = CampaignAimHireError::CreationFailed;
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "TacticalCreateSoldier in HireMerc():	Failed to Add Merc");
		return( MERC_HIRE_FAILED );
	}

	if ( mercenaryPolicy.givesUnfinishedBusinessHireGear() )
	{
	//JA25 UB
	//MErc mercs come with an umbrella
	if ( gMercProfiles[ubCurrentSoldier].Type == PROFILETYPE_MERC )
	{
		AddItemToMerc( iNewIndex, MERC_UMBRELLA ); //Data-1.13\TableData\Items\items.xml, uiIndex = 1361 or Data\TableData\Items\items.xml, uiIndex = 336
	}
	
	//if this is an AIM or MERC merc
	if( gJa25SaveStruct.fHaveAimandMercOffferItems )
	{
		//if its an aim merc
		if ( gMercProfiles[ubCurrentSoldier].Type == PROFILETYPE_AIM )
		{
			//give the mercs one of the promo items
			AddItemToMerc( iNewIndex, SAM_GARVER_COMBAT_KNIFE ); //1353
		}
		// if its a merc merc
		else if ( gMercProfiles[ubCurrentSoldier].Type == PROFILETYPE_MERC )
		{
			//give the mercs one of the promo items
			AddItemToMerc( iNewIndex, CHE_GUEVARA_CANTEEN ); //1359
			AddItemToMerc( iNewIndex, MERC_WRISTWATCH ); //1360
			AddItemToMerc( iNewIndex, SAM_GARVER_COMBAT_KNIFE ); //1353
		}
	}
	}
	else if( mercenaryPolicy.givesInitialArulcoLetter() && DidGameJustStart() )
	{
		// OK, CHECK FOR FIRST GUY, GIVE HIM SPECIAL ITEM!
		if ( iNewIndex == 0 )
		{
			// OK, give this item to our merc!
			// make an objecttype
			CreateItem(LETTER, 100, &gTempObject);
			// Give it
			TacticalActor* newMerc =
				GetJa2SoldierRepository().resolve(iNewIndex.i);
			fReturn = AutoPlaceObject( newMerc, &gTempObject, FALSE );
			// CHRISL: This condition should resolve the issue of the letter not being issued to the first merc
			if(!fReturn)
			{
				if (UsingNewInventorySystem())
				{
					newMerc->inventory()[NUM_INV_SLOTS-1] = gTempObject;
					fReturn=TRUE;
				}
				else
				{
					newMerc->inventory()[SMALLPOCK8POS] = gTempObject;
					fReturn = TRUE;
				}
			}
			Assert( fReturn );
		}

		// Set insertion for first time in chopper

		// ATE: Insert for demo , not using the heli sequence....
		pHireMerc->ubInsertionCode				= INSERTION_CODE_CHOPPER;
	}

	//record how long the merc will be gone for
	pMerc->bMercStatus = (UINT8)pHireMerc->iTotalContractLength;

	pSoldier = GetJa2SoldierRepository().resolve(iNewIndex.i);
	if (checkedResult)
	{
		checkedResult->actor = pSoldier ? GetJa2TacticalEntityId(*pSoldier) : TacticalEntityId{};
		if (!checkedResult->actor.valid() || ResolveJa2TacticalEntity(checkedResult->actor) != pSoldier)
		{
			checkedResult->error = CampaignAimHireError::PostconditionFailed;
			return MERC_HIRE_FAILED;
		}
	}

	//Copy over insertion data....
	pSoldier->deployment().strategicInsertionCode() = pHireMerc->ubInsertionCode;
	pSoldier->deployment().strategicInsertionData() = pHireMerc->usInsertionData;
	// ATE: Copy over value for using alnding zone to soldier type
	pSoldier->deployment().setUseLandingZoneForArrival( pHireMerc->fUseLandingZoneForArrival != FALSE );


	// Set assignment
	//ATE: If first time, make ON_DUTY, otherwise GUARD
	if( ( pSoldier->assignment().current() != IN_TRANSIT ) )
	{
		SetTimeOfAssignmentChangeForMerc( pSoldier );
	}
	ChangeSoldiersAssignment( pSoldier, IN_TRANSIT );

	//set the contract length
	pSoldier->employment().totalLength() = pHireMerc->iTotalContractLength;

	//reset the insurance values
	pSoldier->employment().insuranceStartDay() = 0;
	pSoldier->employment().insuranceLengthDays() = 0;

	//Init the contract charge
//	pSoldier->iTotalContractCharge = 0;

	// store arrival time in soldier structure so map screen can display it
	pSoldier->deployment().arrivalTime() = pHireMerc->uiTimeTillMercArrives;


	//Set the type of merc

	if (!is_networked)
	{
	if( DidGameJustStart() )
	{
		CampaignMercenaryArrivalContent arrivalContent;
		if ( mercenaryPolicy.usesUnfinishedBusinessRules() )
		{
			arrivalContent = ReadCampaignMercenaryArrivalContent();
			//set a flag so we know we are doing the heli crash
			gfFirstTimeInGameHeliCrash =
				arrivalContent.inGameHelicopterCrash ||
				!arrivalContent.inGameHelicopter;
		}

		pHireMerc->uiTimeTillMercArrives = ( gGameExternalOptions.iGameStartingTime + gGameExternalOptions.iFirstArrivalDelay ) / NUM_SEC_IN_MIN;

	if ( mercenaryPolicy.usesGroundArrival(
			arrivalContent.inGameHelicopter) )
	{
		// Set the gridno for the soldier
		pSoldier->deployment().strategicInsertionCode() = INSERTION_CODE_GRIDNO;
		pSoldier->deployment().strategicInsertionData() = GetInitialHeliGridNo( );

		//Set a "code" to enable the merc to be in the direction we set!
		pSoldier->deployment().insertionDirection() = Random( NUM_WORLD_DIRECTIONS ) + 100;

		if( pSoldier->deployment().strategicInsertionCode() == 0 )
		{
			Assert( 0 );
		}

		pSoldier->deployment().beginArrivalGetup();

		RESETTIMECOUNTER( pSoldier->deployment().arrivalGetupCounter(), GetInitialHeliRandomTime() );
		}
		else
		{
		pHireMerc->ubInsertionCode				= INSERTION_CODE_CHOPPER;
		}
		//set when the merc's contract is finished
		pSoldier->employment().endTime() = GetMidnightOfFutureDayInMinutes( pSoldier->employment().totalLength() ) + ( GetHourWhenContractDone( pSoldier ) * 60 );
	}
	else
	{
		//set when the merc's contract is finished ( + 1 cause it takes a day for the merc to arrive )
		pSoldier->employment().endTime() = GetMidnightOfFutureDayInMinutes( 1 + pSoldier->employment().totalLength() ) + ( GetHourWhenContractDone( pSoldier ) * 60 );
	}
	}
	// WANNE - MP: We need this, so the merc contract is correct!
	else
	{
		pSoldier->employment().endTime() = GetMidnightOfFutureDayInMinutes( pSoldier->employment().totalLength() ) + ( GetHourWhenContractDone( pSoldier ) * 60 );
	}

	//Set the time and ID of the last hired merc will arrive
	LaptopSaveInfo.sLastHiredMerc.iIdOfMerc = pHireMerc->ubProfileID;
	LaptopSaveInfo.sLastHiredMerc.uiArrivalTime = pHireMerc->uiTimeTillMercArrives;

	if (!is_client)
	{
		//if we are trying to hire a merc that should arrive later, put the merc in the queue
		if( pHireMerc->uiTimeTillMercArrives	!= 0 )
		{
			if (checkedResult)
			{
				const auto scheduled = AddStrategicEventChecked(
					EVENT_DELAYED_HIRING_OF_MERC, pHireMerc->uiTimeTillMercArrives,
					pSoldier->identity().id().i);
				if (!scheduled)
				{
					checkedResult->error = CampaignAimHireError::EventSchedulingFailed;
					return MERC_HIRE_FAILED;
				}
			}
			else
				AddStrategicEvent( EVENT_DELAYED_HIRING_OF_MERC, pHireMerc->uiTimeTillMercArrives,	pSoldier->identity().id() );
				
			//specify that the merc is hired but hasnt arrived yet
			pMerc->bMercStatus = MERC_HIRED_BUT_NOT_ARRIVED_YET;
		}
	}
	else
	{
		if(is_client)send_hire( iNewIndex, ubCurrentSoldier, pHireMerc->iTotalContractLength, MercCreateStruct.fCopyProfileItemsOver  );
		//send off hire info to network, also avail possibility for net-game exclusive hired pSoldier changes...
	}

	//if the merc is an AIM merc
	if ( gMercProfiles[ubCurrentSoldier].Type == PROFILETYPE_AIM )
	{
		pSoldier->employment().mercenaryType() = MERC_TYPE__AIM_MERC;
		//determine how much the contract is, and remember what type of contract he got
		if( pHireMerc->iTotalContractLength == 1 )
		{
			//pSoldier->iTotalContractCharge = gMercProfiles[ pSoldier->identity().profile() ].sSalary;
			pSoldier->employment().lastContractType() = CONTRACT_EXTEND_1_DAY;
		pSoldier->employment().timeCanSignElsewhere() = GetWorldTotalMin();
		}
		else if( pHireMerc->iTotalContractLength == 7 )
		{
			//pSoldier->iTotalContractCharge = gMercProfiles[ pSoldier->identity().profile() ].uiWeeklySalary;
			pSoldier->employment().lastContractType() = CONTRACT_EXTEND_1_WEEK;
		pSoldier->employment().timeCanSignElsewhere() = GetWorldTotalMin();
		}
		else if( pHireMerc->iTotalContractLength == 14 )
		{
			//pSoldier->iTotalContractCharge = gMercProfiles[ pSoldier->identity().profile() ].uiBiWeeklySalary;
			pSoldier->employment().lastContractType() = CONTRACT_EXTEND_2_WEEK;
		// These luck fellows need to stay the whole duration!
		pSoldier->employment().timeCanSignElsewhere() = pSoldier->employment().endTime();
		}

		// remember the medical deposit we PAID.	The one in his profile can increase when he levels!
		pSoldier->employment().medicalDeposit() = gMercProfiles[ pSoldier->identity().profile() ].sMedicalDepositAmount;
	}
	//if the merc is from M.E.R.C.
	else if ( gMercProfiles[ubCurrentSoldier].Type == PROFILETYPE_MERC )
	{
		pSoldier->employment().mercenaryType() = MERC_TYPE__MERC;
		//pSoldier->iTotalContractCharge = -1;

		gMercProfiles[ pSoldier->identity().profile() ].iMercMercContractLength = 1;

		//Set starting conditions for the merc
		pSoldier->employment().startTime() = GetWorldDay( );

		if(!is_client)AddHistoryToPlayersLog(HISTORY_HIRED_MERC_FROM_MERC, ubCurrentSoldier, GetWorldTotalMin(), -1, -1 );
	}
	//If the merc is from IMP, (ie a player character)
	else if ( gMercProfiles[ubCurrentSoldier].Type == PROFILETYPE_IMP )
	{
		pSoldier->employment().mercenaryType() = MERC_TYPE__PLAYER_CHARACTER;
		//pSoldier->iTotalContractCharge = -1;
	}
	//else its a NPC merc
	else
	{
		pSoldier->employment().mercenaryType() = MERC_TYPE__NPC;
		//pSoldier->iTotalContractCharge = -1;
	}
	if ( mercenaryPolicy.setsStartDayForEveryHire() )
		pSoldier->employment().startTime() = GetWorldDay( );
	//remove the merc from the Personnel screens departed list ( if they have never been hired before, its ok to call it )
	RemoveNewlyHiredMercFromPersonnelDepartedList(
		pSoldier->identity().profile(), checkedResult == nullptr );

	// Flugente: dynamic opinions
	if (gGameExternalOptions.fDynamicOpinions)
	{
		CheckForFriendsofHated(pSoldier);
	}

	gfAtLeastOneMercWasHired = TRUE;

	return( MERC_HIRE_OK );
}


INT8 HireMerc( MERC_HIRE_STRUCT* pHireMerc )
{
	return HireMercImpl(pHireMerc, nullptr);
}

CampaignAimHireError ReadCampaignAimHireArrival(
	CampaignAimHireArrival& arrivalOut) noexcept
{
	using Error = CampaignAimHireError;
	const auto& game = GetGameContext();
	if (game.lifecycle() != GameLifecycle::Running ||
		game.capabilities().isEditor() ||
		CampaignMercenaryPolicy(game.capabilities()).usesUnfinishedBusinessRules() ||
		DidGameJustStart() || IsJa2TacticalWorldLoaded() ||
		is_networked || is_client || is_server ||
		(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) ||
		GetCurrentScreen() == AUTORESOLVE_SCREEN)
		return Error::UnsupportedCampaignState;
	if (gsMercArriveSectorX < 1 || gsMercArriveSectorX > 16 ||
		gsMercArriveSectorY < 1 || gsMercArriveSectorY > 16)
		return Error::InvalidLandingZone;

	const auto& clock = CaptureJa2CampaignClock();
	const std::uint64_t now = clock.totalSeconds / NUM_SEC_IN_MIN;
	const std::uint64_t midnight = now - now % 1440;
	const std::uint32_t hour = static_cast<std::uint32_t>((now % 1440) / 60);
	// Legacy debug calendar overrides must not make the native arrival and
	// contract calculations disagree with the canonical total-seconds clock.
	if (clock.day != clock.totalSeconds / NUM_SEC_IN_DAY ||
		clock.hour != hour || clock.minute != now % 60)
		return Error::TimeOutOfRange;
	std::uint64_t arrival = midnight;
	if (hour > 13) arrival += 1440 + MERC_ARRIVE_TIME_SLOT_1;
	else if (hour + MIN_FLIGHT_PREP_TIME <= 7) arrival += MERC_ARRIVE_TIME_SLOT_1;
	else if (hour + MIN_FLIGHT_PREP_TIME <= 13) arrival += MERC_ARRIVE_TIME_SLOT_2;
	else arrival += MERC_ARRIVE_TIME_SLOT_3;
	if (arrival <= now ||
		arrival > std::numeric_limits<std::uint32_t>::max() / NUM_SEC_IN_MIN)
		return Error::TimeOutOfRange;
	arrivalOut = {static_cast<std::uint8_t>(gsMercArriveSectorX),
		static_cast<std::uint8_t>(gsMercArriveSectorY),
		static_cast<std::uint32_t>(arrival)};
	return Error::None;
}

CampaignAimHireError PrepareCampaignAimHire(
	const CampaignAimHireRequest& request,
	CampaignAimHirePlan& planOut) noexcept
{
	using Error = CampaignAimHireError;
	if (request.profile >= NUM_PROFILES || request.profile == NO_PROFILE)
		return Error::InvalidProfile;
	if (request.contractDays != 1 && request.contractDays != 7 &&
		request.contractDays != 14) return Error::InvalidContract;
	if (request.copyProfileEquipment) return Error::UnsupportedEquipment;
	CampaignAimHireArrival arrival;
	const auto contextError = ReadCampaignAimHireArrival(arrival);
	if (contextError != Error::None) return contextError;
	const auto profileId = static_cast<std::uint8_t>(request.profile);
	const auto& profile = gMercProfiles[profileId];
	if (profile.Type != PROFILETYPE_AIM) return Error::InvalidProfile;
	if ((profile.bMercStatus != 0 &&
		profile.bMercStatus != MERC_ANNOYED_BUT_CAN_STILL_CONTACT) ||
		!IsMercHireable(profileId)) return Error::Unavailable;
	if (profile.ubBodyType > REGFEMALE || profile.bLife < OKLIFE ||
		profile.bLifeMax > 100 || profile.bLife > profile.bLifeMax)
		return Error::InvalidProfileState;

	auto& repository = GetJa2SoldierRepository();
	// A profile can already be active outside the player team's slot range,
	// including a delayed-arrival actor. Never reuse its identity implicitly.
	for (std::size_t slot = 0; slot < repository.capacity(); ++slot)
	{
		const auto* actor = repository.resolve(slot);
		if (actor && actor->roster().active() &&
			actor->identity().profile() == profileId)
			return Error::DuplicateProfile;
	}
	if (gbPlayerNum != OUR_TEAM) return Error::InvalidTeam;
	const std::size_t first = gTacticalStatus.Team[OUR_TEAM].bFirstID.i;
	const std::size_t last = gTacticalStatus.Team[OUR_TEAM].bLastID.i;
	if (first > last || last >= repository.capacity() || last >= TOTAL_SOLDIERS)
		return Error::InvalidTeam;
	const std::size_t teamSlots = last - first + 1;
	const std::size_t vehicleReserve = gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles;
	if (teamSlots > CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS ||
		vehicleReserve > CODE_MAXIMUM_NUMBER_OF_PLAYER_VEHICLES ||
		(teamSlots > vehicleReserve &&
		 teamSlots - vehicleReserve > CODE_MAXIMUM_NUMBER_OF_PLAYER_MERCS))
		return Error::InvalidTeam;
	if (gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles >= teamSlots)
		return Error::CapacityReached;
	std::size_t mercs = 0;
	bool freeSlot = false;
	for (std::size_t slot = first; slot <= last; ++slot)
	{
		const auto* actor = repository.resolve(slot);
		if (!actor || !repository.contains(slot, *actor) ||
			(actor->roster().active() && actor->roster().team() != OUR_TEAM))
			return Error::InvalidTeam;
		if (!actor->roster().active()) freeSlot = true;
		else if (!(actor->status().flags() & SOLDIER_VEHICLE)) ++mercs;
	}
	if (!freeSlot || mercs >= teamSlots -
		gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles)
		return Error::CapacityReached;

	const std::uint64_t now = GetWorldTotalMin();
	const std::uint64_t contractEnd = now - now % 1440 +
		(static_cast<std::uint64_t>(request.contractDays) + 1) * 1440 +
		((arrival.arrivalMinute % 1440) / 60) * 60;
	if (contractEnd > static_cast<std::uint64_t>(
		std::numeric_limits<std::int32_t>::max())) return Error::TimeOutOfRange;
	planOut = {request, arrival.landingX, arrival.landingY,
		arrival.arrivalMinute, static_cast<std::uint32_t>(contractEnd)};
	return Error::None;
}

namespace
{
bool ValidateCampaignAimHire(const CampaignAimHirePlan& plan,
	const CampaignAimHireResult& result) noexcept
{
	const auto* actor = ResolveJa2TacticalEntity(result.actor);
	if (!actor || GetJa2TacticalEntityId(*actor) != result.actor ||
		!actor->roster().active() || actor->roster().team() != OUR_TEAM ||
		actor->roster().inSector() || actor->identity().profile() != plan.request.profile ||
		actor->identity().id().i < gTacticalStatus.Team[OUR_TEAM].bFirstID.i ||
		actor->identity().id().i > gTacticalStatus.Team[OUR_TEAM].bLastID.i ||
		actor->assignment().current() != IN_TRANSIT ||
		actor->vitals().health() < OKLIFE ||
		actor->employment().totalLength() != plan.request.contractDays ||
		actor->employment().endTime() != plan.contractEndMinute ||
		actor->employment().mercenaryType() != MERC_TYPE__AIM_MERC ||
		actor->employment().lastContractType() !=
			(plan.request.contractDays == 1 ? CONTRACT_EXTEND_1_DAY :
			 plan.request.contractDays == 7 ? CONTRACT_EXTEND_1_WEEK : CONTRACT_EXTEND_2_WEEK) ||
		actor->employment().medicalDeposit() != gMercProfiles[plan.request.profile].sMedicalDepositAmount ||
		actor->employment().insuranceStartDay() != 0 ||
		actor->employment().insuranceLengthDays() != 0 ||
		actor->deployment().sectorX() != plan.landingX ||
		actor->deployment().sectorY() != plan.landingY ||
		actor->deployment().sectorZ() != 0 ||
		actor->deployment().arrivalTime() != plan.arrivalMinute ||
		!actor->deployment().usesLandingZoneForArrival() ||
		actor->deployment().strategicInsertionCode() != INSERTION_CODE_ARRIVING_GAME ||
		gMercProfiles[plan.request.profile].bMercStatus != MERC_HIRED_BUT_NOT_ARRIVED_YET ||
		LaptopSaveInfo.sLastHiredMerc.iIdOfMerc != plan.request.profile ||
		LaptopSaveInfo.sLastHiredMerc.uiArrivalTime != plan.arrivalMinute)
		return false;
	std::size_t profiles = 0;
	auto& repository = GetJa2SoldierRepository();
	for (std::size_t slot = 0; slot < repository.capacity(); ++slot)
	{
		const auto* candidate = repository.resolve(slot);
		if (candidate && candidate->roster().active() &&
			candidate->identity().profile() == plan.request.profile) ++profiles;
	}
	if (profiles != 1 || !GetJa2CampaignEventQueue().validate()) return false;
	std::size_t events = 0;
	for (const auto* event = GetStrategicEventListHead(); event; event = event->next)
	{
		if (event->ubCallbackID != EVENT_DELAYED_HIRING_OF_MERC ||
			event->uiParam != actor->identity().id().i) continue;
		if (event->ubEventType != ONETIME_EVENT || event->ubFlags != 0 ||
			event->uiTimeOffset != 0 ||
			event->uiTimeStamp != plan.arrivalMinute * NUM_SEC_IN_MIN) return false;
		++events;
	}
	return events == 1;
}
}

CampaignAimHireResult HireAimMercChecked(
	const CampaignAimHireRequest& request) noexcept
{
	CampaignAimHireResult result;
	CampaignAimHirePlan plan;
	result.error = PrepareCampaignAimHire(request, plan);
	if (result.error != CampaignAimHireError::None) return result;
	result.error = CampaignAimHireError::NativeFailure;
	try
	{
		MERC_HIRE_STRUCT hire{};
		hire.ubProfileID = static_cast<std::uint8_t>(request.profile);
		hire.iTotalContractLength = static_cast<std::int16_t>(request.contractDays);
		hire.fCopyProfileItemsOver = request.copyProfileEquipment;
		hire.sSectorX = plan.landingX;
		hire.sSectorY = plan.landingY;
		hire.fUseLandingZoneForArrival = TRUE;
		hire.ubInsertionCode = INSERTION_CODE_ARRIVING_GAME;
		hire.uiTimeTillMercArrives = plan.arrivalMinute;
		if (HireMercImpl(&hire, &result) != MERC_HIRE_OK) return result;
		if (!ValidateCampaignAimHire(plan, result))
		{
			result.error = CampaignAimHireError::PostconditionFailed;
			return result;
		}
		result.error = CampaignAimHireError::None;
		result.arrivalMinute = plan.arrivalMinute;
	}
	catch (...)
	{
		result.error = CampaignAimHireError::NativeFailure;
	}
	return result;
}

static void MercArrivesCallbackImpl( SoldierID ubSoldierID,
	CampaignAimArrivalResult* checkedResult = nullptr, std::uint32_t checkedContractEnd = 0 )
{
	const CampaignMercenaryPolicy mercenaryPolicy(
		GetGameContext().capabilities());
	MERCPROFILESTRUCT				*pMerc;
	TacticalActor							*pSoldier;
	UINT32									uiTimeOfPost;


	if (!is_networked)
	{
		// hayden - maybe you want to duke it out in omerta ;)
		// HEADROCK HAM 3.5: Externalized starting (safe) sector
		// HEADROCK HAM 3.5: Actually, this is really ridiculous. Why should enemies at the LZ be eliminated at all?
		// I'm taking the initiative and removing this from the code. Mainly because it ends up interfering with
		// externalized LZs combined with other features like "Always Real Time" and "Forced Turn Based".
		//if( !DidGameJustStart() && gsMercArriveSectorX == gGameExternalOptions.ubDefaultArrivalSectorX && gsMercArriveSectorY == gGameExternalOptions.ubDefaultArrivalSectorY )
		//	{ 
		//		//Mercs arriving in A9.  This sector has been deemed as the always safe sector.
		//		//Seeing we don't support entry into a hostile sector (except for the beginning),
		//		//we will nuke any enemies in this sector first.
		//		if( gWorldSectorX != gGameExternalOptions.ubDefaultArrivalSectorX || gWorldSectorY != gGameExternalOptions.ubDefaultArrivalSectorY || gbWorldSectorZ )
		//		{
		//			EliminateAllEnemies( (UINT8)gsMercArriveSectorX, (UINT8)gsMercArriveSectorY );
		//		}
		//	}
	}

	// This will update ANY soldiers currently schedules to arrive too
	if (checkedResult)
	{
		if (!CheckForValidArrivalSectorImpl(false))
		{
			checkedResult->error = CampaignAimArrivalError::NoSafeLandingZone;
			return;
		}
	}
	else CheckForValidArrivalSector();

	// stop time compression until player restarts it
	StopTimeCompression();

	pSoldier = GetJa2SoldierRepository().resolve(ubSoldierID.i);

	pMerc = &gMercProfiles[ pSoldier->identity().profile() ];

// anv: handle Kulba's odyssey
	if( mercenaryPolicy.runsJohnKulbaArrivalDelay() &&
		pSoldier->identity().profile() == JOHN_MERC )
	{
		// just in case
		if( LaptopSaveInfo.ubJohnPossibleMissedFlights > 3 )
			LaptopSaveInfo.ubJohnPossibleMissedFlights = 3;
		// every time Kulba delays his arrival, chances of next delay decrease
		if( Random( 100 ) < LaptopSaveInfo.ubJohnPossibleMissedFlights * 25 ) 
		{			
			pSoldier->deployment().arrivalTime() = pSoldier->deployment().arrivalTime() + 720 + Random ( 720 );
			AddStrategicEvent( EVENT_DELAYED_HIRING_OF_MERC, pSoldier->deployment().arrivalTime(),	pSoldier->identity().id() );
			if ( LaptopSaveInfo.ubJohnPossibleMissedFlights == 3 )
				AddEmail(JA2_EMAIL_JOHN_KULBA_MISSED_FLIGHT_1, JA2_EMAIL_JOHN_KULBA_MISSED_FLIGHT_1_LENGTH, JOHN_KULBA, GetWorldTotalMin(), -1, -1, TYPE_EMAIL_EMAIL_EDT, XML_JOHNKULBA_MISSEDTRANSFERFLIGHT);
			else if ( LaptopSaveInfo.ubJohnPossibleMissedFlights == 2 )
				AddEmail(JA2_EMAIL_JOHN_KULBA_MISSED_FLIGHT_2, JA2_EMAIL_JOHN_KULBA_MISSED_FLIGHT_2_LENGTH, JOHN_KULBA, GetWorldTotalMin(), -1, -1, TYPE_EMAIL_EMAIL_EDT, XML_JOHNKULBA_CRASHLANDEDHELI);
			else if ( LaptopSaveInfo.ubJohnPossibleMissedFlights == 1 )
				AddEmail(JA2_EMAIL_JOHN_KULBA_MISSED_FLIGHT_3, JA2_EMAIL_JOHN_KULBA_MISSED_FLIGHT_3_LENGTH, JOHN_KULBA, GetWorldTotalMin(), -1, -1, TYPE_EMAIL_EMAIL_EDT, XML_JOHNKULBA_AMBUSHEDBYCATS);
			LaptopSaveInfo.ubJohnPossibleMissedFlights--;
			return;
		}
		else
		{
			// reset possible missed flights, for the future Kulba's adventures
			LaptopSaveInfo.ubJohnPossibleMissedFlights = 3;
		}
	}
	//shadooow: if all mercs were killed or captured and default arrival sector is Omerta, force helidrop arrival animation
	if (!checkedResult && GetCurrentScreen() == MAP_SCREEN && pSoldier->deployment().usesLandingZoneForArrival() && !gWorldSectorX && !gWorldSectorY && gbWorldSectorZ == -1 &&
		gsMercArriveSectorX == gGameExternalOptions.ubDefaultArrivalSectorX && gsMercArriveSectorY == gGameExternalOptions.ubDefaultArrivalSectorY)
	{
		bool force_helidrop = true;
		TacticalActor	*pTeamSoldier;
		for (UINT16 cnt = 0; cnt < giMAXIMUM_NUMBER_OF_PLAYER_SLOTS; cnt++)
		{
			if (gCharactersList[cnt].fValid)
			{
				pTeamSoldier = GetJa2SoldierRepository().resolve(
					gCharactersList[cnt].usSolID.i);
				if (pTeamSoldier != pSoldier && pTeamSoldier->assignment().current() != ASSIGNMENT_DEAD && pTeamSoldier->assignment().current() != ASSIGNMENT_POW && pTeamSoldier->assignment().current() != IN_TRANSIT && pSoldier->deployment().strategicInsertionCode() != INSERTION_CODE_CHOPPER)
				{
					force_helidrop = false;
				}
			}
		}
		if (force_helidrop)
		{
			SetCurrentWorldSector(gGameExternalOptions.ubDefaultArrivalSectorX, gGameExternalOptions.ubDefaultArrivalSectorY, 0);
		}
	}

	// add the guy to a squad
	if (!AddCharacterToAnySquad(pSoldier) && checkedResult)
	{
		checkedResult->error = CampaignAimArrivalError::SquadAssignmentFailed;
		return;
	}

	// ATE: Make sure we use global.....
	if ( pSoldier->deployment().usesLandingZoneForArrival() )
	{
		pSoldier->deployment().sectorX()	= gsMercArriveSectorX;
		pSoldier->deployment().sectorY()	= gsMercArriveSectorY;
		pSoldier->deployment().sectorZ()	= 0;
	}

	// Add merc to sector ( if it's the current one )
	if ( (!checkedResult || IsJa2TacticalWorldLoaded()) && gWorldSectorX == pSoldier->deployment().sectorX() && gWorldSectorY == pSoldier->deployment().sectorY() && pSoldier->deployment().sectorZ() == gbWorldSectorZ )
	{
		// OK, If this sector is currently loaded, and guy does not have CHOPPER insertion code....
		// ( which means we are at beginning of game if so )
		// Setup chopper....
		const bool isAtDefaultArrivalSector =
			pSoldier->deployment().sectorX() ==
				gGameExternalOptions.ubDefaultArrivalSectorX &&
			pSoldier->deployment().sectorY() ==
				gGameExternalOptions.ubDefaultArrivalSectorY;
		CampaignMercenaryArrivalContent arrivalContent;
		if ( mercenaryPolicy.usesUnfinishedBusinessRules() )
		{
			arrivalContent = ReadCampaignMercenaryArrivalContent();
		}
		if ( mercenaryPolicy.shouldStartArrivalHelicopter(
				pSoldier->deployment().strategicInsertionCode() ==
					INSERTION_CODE_CHOPPER,
				isAtDefaultArrivalSector,
				arrivalContent.inGameHelicopter) )
		{
			gfTacticalDoHeliRun = TRUE;
			if (gfFirstHeliRun)
			{
				SetHelicopterDroppoint(gGameExternalOptions.iInitialMercArrivalLocation);
			}
			else
			{
				SetHelicopterDroppoint(gMapInformation.sCenterGridNo);
			}

			// OK, If we are in mapscreen, get out...
			if ( GetCurrentScreen() == MAP_SCREEN )
			{
				// ATE: Make sure the current one is selected!
				ChangeSelectedMapSector( gWorldSectorX, gWorldSectorY, 0 );

				RequestTriggerExitFromMapscreen( MAP_EXIT_TO_TACTICAL );
			}

			pSoldier->deployment().strategicInsertionCode() = INSERTION_CODE_CHOPPER;
		}

		UpdateMercInSector( pSoldier, pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );
	}
	// Strategic map arrival to a sector that's not loaded
	else
	{
		if ( mercenaryPolicy.usesGridInsertionForOffscreenArrival() )
		{
			pSoldier->deployment().strategicInsertionCode() = INSERTION_CODE_GRIDNO;
			const CampaignMercenaryArrivalContent arrivalContent =
				ReadCampaignMercenaryArrivalContent();
			pSoldier->deployment().strategicInsertionData() =
				arrivalContent.offscreenArrivalGridNo;
		}
		else
		{
			pSoldier->deployment().strategicInsertionCode() = INSERTION_CODE_CENTER;
		}
	}

	if ( pSoldier->deployment().strategicInsertionCode() != INSERTION_CODE_CHOPPER )
	{
		if (!checkedResult)
			ScreenMsg( FONT_MCOLOR_WHITE, MSG_INTERFACE, TacticalStr[ MERC_HAS_ARRIVED_STR ], pSoldier->GetName() );

		// ATE: He's going to say something, now that they've arrived...
		if ( gTacticalStatus.bMercArrivingQuoteBeingUsed == FALSE && !gfFirstHeliRun )
		{
			gTacticalStatus.bMercArrivingQuoteBeingUsed = TRUE;

			//Setup the highlight sector value (note this isn't for mines but using same system)
			gsSectorLocatorX = pSoldier->deployment().sectorX();
			gsSectorLocatorY = pSoldier->deployment().sectorY();

			TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_MINESECTOREVENT, 2, 0 );
			
			if( mercenaryPolicy.shouldPlayReachedDestinationQuote(
					gfFirstTimeInGameHeliCrash != FALSE) )
			{
				TacticalCharacterDialogue(
					pSoldier, QUOTE_MERC_REACHED_DESTINATION );
			}

			TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_MINESECTOREVENT, 3, 0 );
			TacticalCharacterDialogueWithSpecialEventEx( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_UNSET_ARRIVES_FLAG, 0, 0, 0 );
		}
	}

	//record how long the merc will be gone for
	pMerc->bMercStatus = (UINT8)pSoldier->employment().totalLength();

	// remember when excatly he ARRIVED in Arulco, in case he gets fired early
	pSoldier->employment().lastContractUpdateTime() = GetWorldTotalMin();

	//set when the merc's contract is finished
	pSoldier->employment().endTime() = checkedResult ? checkedContractEnd :
		GetMidnightOfFutureDayInMinutes( pSoldier->employment().totalLength() ) + ( GetHourWhenContractDone( pSoldier ) * 60 );

	// Do initial check for bad items
	if ( pSoldier->roster().team() == gbPlayerNum )
	{
		//ATE: Try to see if our equipment sucks!
		if ( SoldierHasWorseEquipmentThanUsedTo( pSoldier ) )
		{
			// Randomly anytime between 9:00, and 10:00
			uiTimeOfPost =	540 + Random( 660 );

			if ( GetWorldMinutesInDay() < uiTimeOfPost )
			{
				if (checkedResult)
				{
					const std::uint64_t minute = static_cast<std::uint64_t>(GetWorldDayInMinutes()) + uiTimeOfPost;
					if (minute > std::numeric_limits<std::uint32_t>::max() / NUM_SEC_IN_MIN ||
						!AddStrategicEventChecked(EVENT_MERC_COMPLAIN_EQUIPMENT,
							static_cast<std::uint32_t>(minute), pSoldier->identity().profile()))
					{
						checkedResult->error = CampaignAimArrivalError::EventSchedulingFailed;
						return;
					}
				}
				else AddSameDayStrategicEvent( EVENT_MERC_COMPLAIN_EQUIPMENT, uiTimeOfPost , pSoldier->identity().profile() );
			}
		}
	}

	HandleMercArrivesQuotes( pSoldier );

	fTeamPanelDirty = TRUE;

	// if the currently selected sector has no one in it, select this one instead
	if ( !CanGoToTacticalInSector( sSelMapX, sSelMapY, ( UINT8 )iCurrentMapSectorZ ) )
	{
		ChangeSelectedMapSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), 0 );
	}

	return;
}


void MercArrivesCallback(SoldierID soldier)
{
	MercArrivesCallbackImpl(soldier);
}

namespace
{
bool ExactPendingAimArrivalEvent(const CampaignAimArrivalRequest& request,
	const TacticalActor& actor) noexcept
{
	if (!request.event || !GetJa2CampaignEventQueue().validate()) return false;
	std::size_t targets = 0, matches = 0;
	for (const STRATEGICEVENT* event = GetStrategicEventListHead(); event; event = event->next)
	{
		if (event->ubCallbackID != EVENT_DELAYED_HIRING_OF_MERC ||
			static_cast<std::uint16_t>(event->uiParam) != actor.identity().id().i) continue;
		++targets;
		if (event->id == request.event && event->uiParam == actor.identity().id().i &&
			event->ubEventType == ONETIME_EVENT && event->uiTimeOffset == 0 && event->ubFlags == 0 &&
			actor.deployment().arrivalTime() <= std::numeric_limits<std::uint32_t>::max() / NUM_SEC_IN_MIN &&
			event->uiTimeStamp == actor.deployment().arrivalTime() * NUM_SEC_IN_MIN &&
			event->uiTimeStamp == GetWorldTotalSeconds()) ++matches;
	}
	return targets == 1 && matches == 1;
}

bool PendingAimArrivalHasNoNativeMembership(const TacticalActor& actor) noexcept
{
	const std::uint16_t slot = actor.identity().id().i;
	for (std::size_t squad = 0; squad < kJa2StrategicSquadCount; ++squad)
		for (std::size_t member = 0; member < kJa2StrategicSquadCapacity; ++member)
		{
			const auto identity = GetJa2StrategicSquadActor(squad, member);
			if (identity.valid() && identity.slot == slot) return false;
		}
	std::size_t groups = 0;
	for (const GROUP* group = gpGroupList; group; group = group->next)
	{
		// IDs are uint8 and zero is reserved. This also bounds malformed cycles.
		if (++groups > 255) return false;
		// pPlayerList shares storage with the enemy group payload.
		if (group->usGroupTeam != OUR_TEAM) continue;
		std::size_t members = 0;
		for (const PLAYERGROUP* member = group->pPlayerList; member; member = member->next)
		{
			if (++members > CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS) return false;
			const auto identity = GetPlayerGroupMemberActor(member);
			// A stale incarnation for the same reusable slot is contradictory too.
			if (identity.slot == slot) return false;
		}
	}
	return true;
}

CampaignAimArrivalError PrepareCheckedAimArrival(
	const CampaignAimArrivalRequest& request, std::uint32_t& contractEnd) noexcept
{
	using Error = CampaignAimArrivalError;
	const auto& game = GetGameContext();
	if (game.lifecycle() != GameLifecycle::Running || game.capabilities().isEditor() ||
		CampaignMercenaryPolicy(game.capabilities()).usesUnfinishedBusinessRules() ||
		DidGameJustStart() || IsJa2TacticalWorldLoaded() || is_networked || is_client || is_server ||
		(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) || GetCurrentScreen() == AUTORESOLVE_SCREEN)
		return Error::UnsupportedCampaignState;
	const auto* actor = ResolveJa2TacticalEntity(request.actor);
	if (!actor || GetJa2TacticalEntityId(*actor) != request.actor ||
		!actor->roster().active() || actor->roster().team() != OUR_TEAM || gbPlayerNum != OUR_TEAM)
		return Error::InvalidActor;
	const auto profileId = actor->identity().profile();
	if (profileId >= NUM_PROFILES || profileId == NO_PROFILE) return Error::InvalidProfile;
	if (profileId == JOHN_MERC) return Error::UnsupportedProfile;
	const auto& profile = gMercProfiles[profileId];
	if (profile.Type != PROFILETYPE_AIM) return Error::InvalidProfile;
	auto& repository = GetJa2SoldierRepository();
	const std::size_t first = gTacticalStatus.Team[OUR_TEAM].bFirstID.i;
	const std::size_t last = gTacticalStatus.Team[OUR_TEAM].bLastID.i;
	if (first > last || last >= repository.capacity() || last >= TOTAL_SOLDIERS ||
		actor->identity().id().i < first || actor->identity().id().i > last ||
		gGameOptions.ubSquadSize == 0 || gGameOptions.ubSquadSize > NUMBER_OF_SOLDIERS_PER_SQUAD)
		return Error::InvalidActor;
	for (std::size_t slot = 0; slot < repository.capacity(); ++slot)
	{
		const auto* other = repository.resolve(slot);
		if (slot >= first && slot <= last && (!other || !repository.contains(slot, *other) ||
			(other->roster().active() && other->roster().team() != OUR_TEAM)))
			return Error::InvalidActor;
		if (other && other != actor && other->roster().active() && other->identity().profile() == profileId)
			return Error::InvalidActor;
	}
	const auto days = actor->employment().totalLength();
	if (profile.bMercStatus != MERC_HIRED_BUT_NOT_ARRIVED_YET ||
		actor->assignment().current() != IN_TRANSIT || actor->roster().inSector() ||
		actor->deployment().isBetweenSectors() || actor->deployment().groupId() != 0 ||
		(actor->status().flags() & (SOLDIER_VEHICLE | SOLDIER_DRIVER | SOLDIER_PASSENGER)) ||
		profile.ubBodyType > REGFEMALE || actor->identity().bodyType() != profile.ubBodyType ||
		actor->vitals().health() < OKLIFE || actor->vitals().maximumHealth() > 100 ||
		actor->vitals().health() > actor->vitals().maximumHealth() ||
		(days != 1 && days != 7 && days != 14) ||
		actor->employment().mercenaryType() != MERC_TYPE__AIM_MERC ||
		actor->employment().lastContractType() !=
			(days == 1 ? CONTRACT_EXTEND_1_DAY : days == 7 ? CONTRACT_EXTEND_1_WEEK : CONTRACT_EXTEND_2_WEEK) ||
		actor->employment().medicalDeposit() != profile.sMedicalDepositAmount ||
		actor->employment().insuranceStartDay() != 0 || actor->employment().insuranceLengthDays() != 0 ||
		!actor->deployment().usesLandingZoneForArrival() ||
		actor->deployment().strategicInsertionCode() != INSERTION_CODE_ARRIVING_GAME ||
		actor->deployment().strategicInsertionData() != 0)
		return Error::InvalidPendingState;
	if (gsMercArriveSectorX < 1 || gsMercArriveSectorX > 16 ||
		gsMercArriveSectorY < 1 || gsMercArriveSectorY > 16 ||
		actor->deployment().sectorX() != gsMercArriveSectorX ||
		actor->deployment().sectorY() != gsMercArriveSectorY || actor->deployment().sectorZ() != 0)
		return Error::InvalidLandingZone;
	if (!ExactPendingAimArrivalEvent(request, *actor)) return Error::InvalidEvent;
	const auto clock = CaptureJa2CampaignClock();
	const std::uint64_t now = clock.totalSeconds / NUM_SEC_IN_MIN;
	if (clock.day != clock.totalSeconds / NUM_SEC_IN_DAY ||
		clock.hour != (now % 1440) / 60 || clock.minute != now % 60)
		return Error::TimeOutOfRange;
	const std::uint64_t end = now - now % 1440 + static_cast<std::uint64_t>(days) * 1440 +
		((actor->deployment().arrivalTime() % 1440) / 60) * 60;
	const auto priorEnd = actor->employment().endTime();
	if (end > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) ||
		priorEnd <= 0 || (static_cast<std::uint64_t>(priorEnd) != end &&
		 static_cast<std::uint64_t>(priorEnd) != end + 1440) ||
		(days == 14 ? actor->employment().timeCanSignElsewhere() != priorEnd :
		 (actor->employment().timeCanSignElsewhere() < 0 ||
		  static_cast<std::uint64_t>(actor->employment().timeCanSignElsewhere()) > now)))
		return Error::TimeOutOfRange;
	if (!PendingAimArrivalHasNoNativeMembership(*actor)) return Error::InvalidPendingState;
	contractEnd = static_cast<std::uint32_t>(end);
	return Error::None;
}

bool ValidateCheckedAimArrival(const CampaignAimArrivalRequest& request,
	std::uint32_t expectedEnd, CampaignAimArrivalResult& result) noexcept
{
	const auto* actor = ResolveJa2TacticalEntity(request.actor);
	if (!actor || actor->identity().profile() >= NUM_PROFILES || actor->identity().profile() == NO_PROFILE ||
		gsMercArriveSectorX < 1 || gsMercArriveSectorX > 16 ||
		gsMercArriveSectorY < 1 || gsMercArriveSectorY > 16 ||
		IsJa2TacticalWorldLoaded() || !actor->roster().active() ||
		actor->roster().team() != OUR_TEAM || actor->roster().inSector() ||
		actor->deployment().isBetweenSectors() ||
		actor->assignment().current() < 0 || actor->assignment().current() >= NUMBER_OF_SQUADS ||
		actor->deployment().sectorX() != gsMercArriveSectorX ||
		actor->deployment().sectorY() != gsMercArriveSectorY || actor->deployment().sectorZ() != 0 ||
		actor->deployment().strategicInsertionCode() != INSERTION_CODE_CENTER ||
		actor->employment().endTime() != expectedEnd ||
		actor->employment().lastContractUpdateTime() != GetWorldTotalMin() ||
		gMercProfiles[actor->identity().profile()].bMercStatus != actor->employment().totalLength() ||
		!ExactPendingAimArrivalEvent(request, *actor)) return false;
	const auto groupId = GetJa2StrategicGroupId(actor->deployment().groupId());
	const GROUP* group = ResolveJa2StrategicGroup(groupId);
	if (!group || group->usGroupTeam != OUR_TEAM || group->fVehicle || group->fBetweenSectors ||
		group->ubGroupID != actor->deployment().groupId() ||
		group->ubSectorX != gsMercArriveSectorX || group->ubSectorY != gsMercArriveSectorY || group->ubSectorZ != 0)
		return false;
	std::array<TacticalEntityId, CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS> members{};
	std::size_t count = 0, matches = 0, squadMatches = 0;
	for (const PLAYERGROUP* member = group->pPlayerList; member; member = member->next)
	{
		if (count == members.size()) return false;
		const auto id = GetPlayerGroupMemberActor(member);
		const auto* resolved = ResolvePlayerGroupMember(member);
		if (!id.valid() || !resolved || !resolved->roster().active() || resolved->roster().team() != OUR_TEAM ||
			member->ubProfileID != resolved->identity().profile() ||
			resolved->assignment().current() != actor->assignment().current() ||
			resolved->deployment().groupId() != group->ubGroupID || resolved->deployment().isBetweenSectors() ||
			resolved->deployment().sectorX() != group->ubSectorX || resolved->deployment().sectorY() != group->ubSectorY ||
			resolved->deployment().sectorZ() != 0) return false;
		for (std::size_t previous = 0; previous < count; ++previous)
			if (members[previous] == id) return false;
		members[count++] = id;
		if (id == request.actor) ++matches;
	}
	for (std::size_t squad = 0; squad < NUMBER_OF_SQUADS; ++squad)
		for (std::size_t slot = 0; slot < NUMBER_OF_SOLDIERS_PER_SQUAD; ++slot)
		{
			const auto id = GetJa2StrategicSquadActor(squad, slot);
			if (!id.valid() || id.slot != request.actor.slot) continue;
			if (id != request.actor || squad != static_cast<std::size_t>(actor->assignment().current()))
				return false;
			++squadMatches;
		}
	if (matches != 1 || squadMatches != 1 || count != group->ubGroupSize ||
		count != Ja2StrategicSquadSize(actor->assignment().current())) return false;
	std::size_t observedGroups = 0, allMemberships = 0;
	for (const GROUP* other = gpGroupList; other; other = other->next)
	{
		if (++observedGroups > 255) return false;
		if (other->usGroupTeam != OUR_TEAM) continue;
		std::size_t observedMembers = 0;
		for (const PLAYERGROUP* member = other->pPlayerList; member; member = member->next)
		{
			if (++observedMembers > CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS) return false;
			const auto id = GetPlayerGroupMemberActor(member);
			if (id.slot != request.actor.slot) continue;
			if (other != group || id != request.actor) return false;
			++allMemberships;
		}
	}
	if (allMemberships != 1) return false;
	result.group = groupId;
	result.landingX = static_cast<std::uint8_t>(gsMercArriveSectorX);
	result.landingY = static_cast<std::uint8_t>(gsMercArriveSectorY);
	result.contractEndMinute = expectedEnd;
	return true;
}
}

CampaignAimArrivalResult ArriveAimMercChecked(const CampaignAimArrivalRequest& request) noexcept
{
	CampaignAimArrivalResult result;
	try
	{
		std::uint32_t end = 0;
		result.error = PrepareCheckedAimArrival(request, end);
		if (result.error != CampaignAimArrivalError::None) return result;
		result.actor = request.actor;
		result.mutationMayHaveStarted = true;
		MercArrivesCallbackImpl(SoldierID{request.actor.slot}, &result, end);
		if (result.error == CampaignAimArrivalError::None &&
			!ValidateCheckedAimArrival(request, end, result))
			result.error = CampaignAimArrivalError::PostconditionFailed;
	}
	catch (...)
	{
		result.error = CampaignAimArrivalError::NativeFailure;
	}
	return result;
}

BOOLEAN IsMercHireable( UINT8 ubMercID )
{
	//If the merc has an .EDT file, is not away on assignment, and isnt already hired (but not arrived yet), he is not DEAD and he isnt returning home
	if( ( gMercProfiles[ ubMercID ].bMercStatus == MERC_HAS_NO_TEXT_FILE ) ||
			( gMercProfiles[ ubMercID ].bMercStatus > 0 ) ||
			( gMercProfiles[ ubMercID ].bMercStatus == MERC_HIRED_BUT_NOT_ARRIVED_YET ) ||
			( gMercProfiles[ ubMercID ].bMercStatus == MERC_IS_DEAD ) ||
			( gMercProfiles[ ubMercID ].uiDayBecomesAvailable > 0 ) ||
			( gMercProfiles[ ubMercID ].bMercStatus == MERC_WORKING_ELSEWHERE ) ||
			( gMercProfiles[ ubMercID ].bMercStatus == MERC_FIRED_AS_A_POW ) ||
			( gMercProfiles[ ubMercID ].bMercStatus == MERC_RETURNING_HOME ) )
		return(FALSE);
	else
		return(TRUE);
}

BOOLEAN IsMercDead( UINT8 ubMercID )
{
	if( gMercProfiles[ ubMercID ].bMercStatus == MERC_IS_DEAD )
		return(TRUE);
	else
		return(FALSE);
}

BOOLEAN IsTheSoldierAliveAndConcious( TacticalActor		*pSoldier )
{
	if( pSoldier->vitals().health() >= CONSCIOUSNESS )
		return(TRUE);
	else
		return(FALSE);
}

UINT16	NumberOfMercsOnPlayerTeam()
{
	SoldierID	cnt;
	TacticalActor	*pSoldier;
	SoldierID	bLastTeamID;
	UINT16		ubCount=0;

	// Set locator to first merc
	cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
	bLastTeamID = gTacticalStatus.Team[ gbPlayerNum ].bLastID;

	if (!GetJa2SoldierRepository().resolve(cnt.i))
		return 0;

	for ( ; cnt <= bLastTeamID; ++cnt )
	{
		pSoldier = GetJa2SoldierRepository().resolve(cnt.i);
		AssertNotNIL(pSoldier);

		//if the is active, and is not a vehicle
		if( pSoldier->roster().active() && !( pSoldier->status().flags() & SOLDIER_VEHICLE ) )
		{
			ubCount++;
		}
	}

	return( ubCount );
}


void HandleMercArrivesQuotes( TacticalActor *pSoldier )
{
	SoldierID	cnt, usLastTeamID;
	INT8			bHated;
	TacticalActor	*pTeamSoldier;
	const CampaignMercenaryPolicy mercenaryPolicy(
		GetGameContext().capabilities());
	if( mercenaryPolicy.shouldSkipBuddyArrivalHandling(
			pSoldier->deployment().arrivalGetupPending()) )
	{
		//we can "leave" this function cause we dont want to do anything with buddy system
		return;
	}
	// If we are approaching with helicopter, don't say any ( yet )
	if ( pSoldier->deployment().strategicInsertionCode() != INSERTION_CODE_CHOPPER )
	{
		// if we haven't met the rebels yet, characters issue some comments
		if ( gubQuest[QUEST_DELIVER_LETTER] == QUESTINPROGRESS )
		{
			// Player-generated characters issue a comment about arriving in Omerta.
			if ( pSoldier->employment().mercenaryType() == MERC_TYPE__PLAYER_CHARACTER )
			{
				TacticalCharacterDialogue( pSoldier, QUOTE_PC_DROPPED_OMERTA );
			}
		}

		// Check to see if anyone hates this merc and will now complain
		cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
		usLastTeamID = gTacticalStatus.Team[ gbPlayerNum ].bLastID;
		//loop though all the mercs
		for ( ; cnt <= usLastTeamID; ++cnt )
		{
			pTeamSoldier =
				GetJa2SoldierRepository().resolve(cnt.i);
			if ( pTeamSoldier->roster().active() )
			{
				if ( pTeamSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
				{
					bHated = WhichHated( pTeamSoldier->identity().profile(), pSoldier->identity().profile() );
					if ( bHated != -1 )
					{
						// hates the merc who has arrived and is going to gripe about it!
						switch( bHated )
						{
							case 0:
								TacticalCharacterDialogue( pTeamSoldier, QUOTE_HATED_1_ARRIVES );
								break;
							case 1:
								TacticalCharacterDialogue( pTeamSoldier, QUOTE_HATED_2_ARRIVES );
								break;
							case 2:
								TacticalCharacterDialogue( pTeamSoldier, QUOTE_HATED_3_ARRIVES );
								break;
							case 3:
								TacticalCharacterDialogue( pTeamSoldier, QUOTE_HATED_4_ARRIVES );
								break;
							case 4:
								TacticalCharacterDialogue( pTeamSoldier, QUOTE_HATED_5_ARRIVES );
								break;
							default:
								break;
						}
					}
				}

				// Flugente: additional dialogue
				AdditionalTacticalCharacterDialogue_CallsLua( pTeamSoldier, ADE_MERC_ARRIVES, pSoldier->identity().profile(), ( gubQuest[QUEST_DELIVER_LETTER] == QUESTINPROGRESS ) ? 1 : 0 );
			}
		}
	}
}


#ifdef JA2TESTVERSION
void SetFlagToForceHireMerc( BOOLEAN fForceHire )
{
	gForceHireMerc = fForceHire;
}
#endif


UINT32 GetMercArrivalTimeOfDay( )
{
	UINT32		uiCurrHour;
	UINT32		uiMinHour;

	// Pick a time...

	// First get the current time of day.....
	uiCurrHour = GetWorldHour( );

	// Subtract the min time for any arrival....
	uiMinHour	= uiCurrHour + MIN_FLIGHT_PREP_TIME;

	// OK, first check if we need to advance a whole day's time...
	// See if we have missed the last flight for the day...
	if ( ( uiCurrHour ) > 13	) // ( > 1:00 pm - too bad )
	{
		// 7:30 flight....
		return( GetMidnightOfFutureDayInMinutes( 1 ) + MERC_ARRIVE_TIME_SLOT_1 );
	}

	// Well, now we can handle flights all in one day....
	// Find next possible flight
	if ( uiMinHour <= 7 )
	{
		return( GetWorldDayInMinutes() + MERC_ARRIVE_TIME_SLOT_1 ); // 7:30 am
	}
	else if ( uiMinHour <= 13 )
	{
		return( GetWorldDayInMinutes() + MERC_ARRIVE_TIME_SLOT_2 ); // 1:30 pm
	}
	else
	{
		return( GetWorldDayInMinutes() + MERC_ARRIVE_TIME_SLOT_3 ); // 7:30 pm
	}
}


void UpdateAnyInTransitMercsWithGlobalArrivalSector( )
{
	TacticalActor		*pSoldier;
	SoldierID cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;

	// look for all mercs on the same team,
	for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
	{
		pSoldier = GetJa2SoldierRepository().resolve(cnt.i);
		if ( pSoldier->roster().active() )
		{
			if ( pSoldier->assignment().current() == IN_TRANSIT )
			{
				if ( pSoldier->deployment().usesLandingZoneForArrival() )
				{
					pSoldier->deployment().sectorX()	= gsMercArriveSectorX;
					pSoldier->deployment().sectorY()	= gsMercArriveSectorY;
					pSoldier->deployment().sectorZ()	= 0;
				}
			}
		}
	}
}

INT16 StrategicPythSpacesAway(INT16 sOrigin, INT16 sDest)
{
	INT16 sRows,sCols,sResult;

	sRows = abs((sOrigin / MAP_WORLD_X) - (sDest / MAP_WORLD_X));
	sCols = abs((sOrigin % MAP_WORLD_X) - (sDest % MAP_WORLD_X));


	// apply Pythagoras's theorem for right-handed triangle:
	// dist^2 = rows^2 + cols^2, so use the square root to get the distance
	sResult = (INT16)sqrt((double)(sRows * sRows) + (sCols * sCols));

	return(sResult);
}


// ATE: This function will check if the current arrival sector
// is valid
// if there are enemies present, it's invalid
// if so, search around for nearest non-occupied sector.
static bool CheckForValidArrivalSectorImpl(bool presentNotice)
{
	INT16	sTop, sBottom;
	INT16	sLeft, sRight;
	INT16	cnt1, cnt2, sGoodX, sGoodY;
	UINT8	ubRadius = 4;
	UINT32	leftmost;
	UINT32	 sSectorGridNo, sSectorGridNo2;
	INT32	uiRange, uiLowestRange = 999999;
	BOOLEAN	fFound = FALSE;
	CHAR16 sString[ 1024 ];
	CHAR16 zShortTownIDString1[ 50 ];
	CHAR16 zShortTownIDString2[ 50 ];

	sSectorGridNo = CALCULATE_STRATEGIC_INDEX( gsMercArriveSectorX, gsMercArriveSectorY );

	// Check if valid...
	if ( !StrategicMap[ sSectorGridNo ].fEnemyControlled )
	{
		return true;
	}

	if (presentNotice)
		GetShortSectorString( gsMercArriveSectorX ,gsMercArriveSectorY, zShortTownIDString1 );


	// If here - we need to do a search!
	sTop		= ubRadius;
	sBottom = -ubRadius;
	sLeft	= - ubRadius;
	sRight	= ubRadius;

	for( cnt1 = sBottom; cnt1 <= sTop; cnt1++ )
	{
		leftmost = ( ( sSectorGridNo + ( MAP_WORLD_X * cnt1 ) )/ MAP_WORLD_X ) * MAP_WORLD_X;

		for( cnt2 = sLeft; cnt2 <= sRight; cnt2++ )
		{
			sSectorGridNo2 = sSectorGridNo + ( MAP_WORLD_X * cnt1 ) + cnt2;

			if( sSectorGridNo2 >=1 && sSectorGridNo2 < ( ( MAP_WORLD_X - 1 ) * ( MAP_WORLD_X - 1 ) ) && sSectorGridNo2 >= leftmost && sSectorGridNo2 < ( leftmost + MAP_WORLD_X ) )
			{
				if ( !StrategicMap[ sSectorGridNo2 ].fEnemyControlled && (StrategicMap[ sSectorGridNo2 ].usAirType & AIRSPACE_ENEMY_ACTIVE) )
				{
					uiRange = StrategicPythSpacesAway( sSectorGridNo2, sSectorGridNo );

					if ( uiRange < uiLowestRange )
					{
						sGoodY = cnt1;
						sGoodX = cnt2;
						uiLowestRange = uiRange;
						fFound = TRUE;
					}
				}
			}
		}
	}

	if ( fFound )
	{
		if (!presentNotice && (gsMercArriveSectorX + sGoodX < 1 || gsMercArriveSectorX + sGoodX > 16 ||
			gsMercArriveSectorY + sGoodY < 1 || gsMercArriveSectorY + sGoodY > 16)) return false;
		gsMercArriveSectorX = gsMercArriveSectorX + sGoodX;
		gsMercArriveSectorY = gsMercArriveSectorY + sGoodY;

		UpdateAnyInTransitMercsWithGlobalArrivalSector( );

		if (presentNotice)
		{
			GetShortSectorString(gsMercArriveSectorX, gsMercArriveSectorY, zShortTownIDString2);
			swprintf(sString, New113Message[MSG113_ARRIVINGREROUTED], zShortTownIDString2, zShortTownIDString1);
			DoScreenIndependantMessageBox(sString, MSG_BOX_FLAG_OK, NULL);
		}
		else
			fprintf(stderr, "[campaign] AIM arrival rerouted to %d,%d\n", gsMercArriveSectorX, gsMercArriveSectorY);

	}
	return fFound != FALSE;
}

void CheckForValidArrivalSector()
{
	(void)CheckForValidArrivalSectorImpl(true);
}
UINT32	GetInitialHeliGridNo( )
{
	UINT8	ubCnt;
	UINT32	sGridNo;

	for( ubCnt=0; ubCnt<NUM_INITIAL_GRIDNOS_FOR_HELI_CRASH-1; ubCnt++)
	{
		if( gsInitialHeliGridNo[ ubCnt ] != 0 )
		{
			sGridNo = gsInitialHeliGridNo[ ubCnt ];
			gsInitialHeliGridNo[ ubCnt ] = 0;

			return( sGridNo );
		}
	}

	return( 16233 );
}

UINT16	GetInitialHeliRandomTime()
{
	BOOLEAN fDone=FALSE;
	UINT8		ubRandom;
	UINT16	usTime;
	UINT16	usCounter=0;

	while( !fDone )
	{
		ubRandom = Random( NUM_INITIAL_GRIDNOS_FOR_HELI_CRASH-1 );

		if( gsInitialHeliRandomTimes[ ubRandom ] != 0 )
		{
			usTime = gsInitialHeliRandomTimes[ ubRandom ];
			gsInitialHeliRandomTimes[ ubRandom ] = 0;
			return( usTime );
		}

		if( usCounter > 1000 )
			fDone = TRUE;

		usCounter++;
	}
	return( 1000 + Random( 2000 ) );
}

void InitializeHeliGridnoAndTime( BOOLEAN fLoading )
{
	const CampaignMercenaryPolicy mercenaryPolicy(
		GetGameContext().capabilities());
	if ( !mercenaryPolicy.usesUnfinishedBusinessRules() )
	{
		return;
	}

	Assert( NUM_INITIAL_GRIDNOS_FOR_HELI_CRASH == 7 );

	if( !fLoading )
	{
		gfFirstTimeInGameHeliCrash = FALSE;
	}

	const CampaignMercenaryArrivalContent arrivalContent =
		ReadCampaignMercenaryArrivalContent();
	gsInitialHeliGridNo[ 0 ] = arrivalContent.initialHelicopterGridNos[ 0 ];//14947;
	gsInitialHeliGridNo[ 1 ] = arrivalContent.initialHelicopterGridNos[ 1 ];//15584;//16067;
	gsInitialHeliGridNo[ 2 ] = arrivalContent.initialHelicopterGridNos[ 2 ];//15754;
	gsInitialHeliGridNo[ 3 ] = arrivalContent.initialHelicopterGridNos[ 3 ];//16232;
	gsInitialHeliGridNo[ 4 ] = arrivalContent.initialHelicopterGridNos[ 4 ];//16067;
	gsInitialHeliGridNo[ 5 ] = arrivalContent.initialHelicopterGridNos[ 5 ];//16230;
	gsInitialHeliGridNo[ 6 ] = arrivalContent.initialHelicopterGridNos[ 6 ];//15272;

	gsInitialHeliRandomTimes[ 0 ] = arrivalContent.initialHelicopterRandomTimes[ 0 ];//1300;
	gsInitialHeliRandomTimes[ 1 ] = arrivalContent.initialHelicopterRandomTimes[ 1 ];//2000;
	gsInitialHeliRandomTimes[ 2 ] = arrivalContent.initialHelicopterRandomTimes[ 2 ];//2750;
	gsInitialHeliRandomTimes[ 3 ] = arrivalContent.initialHelicopterRandomTimes[ 3 ];//3400;
	gsInitialHeliRandomTimes[ 4 ] = arrivalContent.initialHelicopterRandomTimes[ 4 ];//4160;
	gsInitialHeliRandomTimes[ 5 ] = arrivalContent.initialHelicopterRandomTimes[ 5 ];//4700;
	gsInitialHeliRandomTimes[ 6 ] = arrivalContent.initialHelicopterRandomTimes[ 6 ];//5630;
}

void InitJerryMiloInfo()
{
	const CampaignMercenaryPolicy mercenaryPolicy(
		GetGameContext().capabilities());
	if ( !mercenaryPolicy.usesUnfinishedBusinessRules() )
	{
		return;
	}

	const CampaignMercenaryArrivalContent arrivalContent =
		ReadCampaignMercenaryArrivalContent();
 if ( arrivalContent.includesJerry )
{
  //  return; //AA
	//Set Jerry Milo's Gridno h7
	gMercProfiles[ JERRY_MILO_UB ].sSectorX = JA2_5_START_SECTOR_X;
	gMercProfiles[ JERRY_MILO_UB ].sSectorY = JA2_5_START_SECTOR_Y;
	gMercProfiles[ JERRY_MILO_UB ].bSectorZ = 0;

	gMercProfiles[ JERRY_MILO_UB ].sGridNo = arrivalContent.jerryGridNo; //15109;

	gMercProfiles[ JERRY_MILO_UB ].fUseProfileInsertionInfo = TRUE;

	gMercProfiles[ JERRY_MILO_UB ].ubStrategicInsertionCode = INSERTION_CODE_GRIDNO;
	gMercProfiles[ JERRY_MILO_UB ].usStrategicInsertionData = arrivalContent.jerryGridNo; //15109;
	
}
	
if ( arrivalContent.inGameHelicopterCrash )
	{
	//init Jerry Milo quotes
	InitJerryQuotes();
	}
}


void UpdateJerryMiloInInitialSector()
{
	const CampaignMercenaryPolicy mercenaryPolicy(
		GetGameContext().capabilities());
	if ( !mercenaryPolicy.usesUnfinishedBusinessRules() )
	{
		return;
	}

	TacticalActor *pSoldier = NULL;
	TacticalActor *pJerrySoldier = NULL;


	//SectorInfo[ SEC_H7 ].fSurfaceWasEverPlayerControlled = TRUE;
	SectorInfo[(UINT8)SECTOR( gGameExternalOptions.ubDefaultArrivalSectorX, gGameExternalOptions.ubDefaultArrivalSectorY )].fSurfaceWasEverPlayerControlled = TRUE;
	//SectorInfo[ SEC_H7 ].ubNumAdmins = 2;
	StrategicMap[CALCULATE_STRATEGIC_INDEX( gGameExternalOptions.ubDefaultArrivalSectorX, gGameExternalOptions.ubDefaultArrivalSectorY )].fEnemyControlled = FALSE;

	const CampaignMercenaryArrivalContent arrivalContent =
		ReadCampaignMercenaryArrivalContent();
	if ( arrivalContent.inGameHelicopter )
		return; //AA

	if ( arrivalContent.inGameHelicopterCrash )
	{
		//if it is the first sector we are loading up, place Jerry in the map
		if ( !gfFirstTimeInGameHeliCrash )
			return;

		if ( arrivalContent.includesJerry )
		{
			pSoldier = FindSoldierByProfileID( JERRY_MILO_UB, FALSE ); //JERRY
			if ( pSoldier == NULL )
			{
				Assert( 0 );
				return;
			}

		}

		//the internet part of the laptop isnt working.  It gets broken in the heli crash.
		if ( arrivalContent.laptopQuestEnabled )
			StartQuest( QUEST_FIX_LAPTOP, -1, -1 );

		//Record the initial sector as ours
		//SectorInfo[ SEC_H7 ].fSurfaceWasEverPlayerControlled = TRUE;
		SectorInfo[(UINT8)SECTOR( gGameExternalOptions.ubDefaultArrivalSectorX, gGameExternalOptions.ubDefaultArrivalSectorY )].fSurfaceWasEverPlayerControlled = TRUE;
		StrategicMap[CALCULATE_STRATEGIC_INDEX( gGameExternalOptions.ubDefaultArrivalSectorX, gGameExternalOptions.ubDefaultArrivalSectorY )].fEnemyControlled = FALSE;

		if ( arrivalContent.includesJerry )
		{
			//Set some variable so Jerry will be on the ground
			pSoldier->deployment().beginArrivalGetup();

			//pSoldier->deployment().strategicInsertionCode() = INSERTION_CODE_GRIDNO; // was disabled
			//pSoldier->deployment().strategicInsertionData() = GetInitialHeliGridNo( ); // was disabled

			RESETTIMECOUNTER( pSoldier->deployment().arrivalGetupCounter(), gsInitialHeliRandomTimes[6] + 800 + Random( 400 ) );

			//should we be on our back or tummy
			if ( Random( 100 ) < 50 )
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  STAND_FALLFORWARD_STOP, 1, TRUE );
			else
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FALLBACKHIT_STOP, 1, TRUE );
		}

		//Wont work cause it gets reset every frame
			//make sure we can see Jerry

		if ( arrivalContent.includesJerry )
		{
			pJerrySoldier = FindSoldierByProfileID( JERRY_MILO_UB, FALSE );//JERRY
			if ( pJerrySoldier != NULL )
			{
				//Make sure we can see the pilot
				gbPublicOpplist[OUR_TEAM][pJerrySoldier->identity().id()] = SEEN_CURRENTLY;
				pJerrySoldier->awareness().markVisible();
			}

		}

		//Lock the interface
		guiPendingOverrideEvent = LU_BEGINUILOCK;
	}
}

void AddItemToMerc( UINT8 ubNewMerc, INT16 sItemType )
{


	// OK, give this item to our merc!
	OBJECTTYPE gTempObject;
	BOOLEAN	fReturn=FALSE;

	// make an objecttype
        CreateItem(sItemType, 100, &gTempObject);

	// Give it 
	TacticalActor* newMerc =
		GetJa2SoldierRepository().resolve(ubNewMerc);
	fReturn = AutoPlaceObject( newMerc, &gTempObject, FALSE );
	
			if(!fReturn && (UsingNewInventorySystem() == true))
			{
				newMerc->inventory()[NUM_INV_SLOTS-1] = gTempObject;
				fReturn=TRUE;
			}
	Assert( fReturn );


}
