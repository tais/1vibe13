#include "builddefines.h"
#include <stdio.h>
#include "types.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "Game Events.h"
#include "SaveSerializer.h"
#include "Game Clock.h"
#include "DEBUG.H"
#include "Font Control.h"
	#include "message.h"
	#include "MiniEvents.h"
	#include "Text.h"

#ifdef JA2TESTVERSION

CHAR16 gEventName[NUMBER_OF_EVENT_TYPES_PLUS_ONE][40]={
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	L"Null",
	L"ChangeLightValue",
	L"WeatherStart",
	L"WeatherEnd",
	L"CheckForQuests",
	L"Ambient",
	L"AIMResetMercAnnoyance",
	L"BobbyRayPurchase",
	L"DailyUpdateBobbyRayInventory",
	L"UpdateBobbyRayInventory",
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	L"DailyUpdateOfMercSite",
	L"Day3AddEMailFromSpeck",
	L"DelayedHiringOfMerc",
	L"HandleInsuredMercs",
	L"PayLifeInsuranceForDeadMerc",
	L"MercDailyUpdate",
	L"MercAboutToLeaveComment",
	L"MercContractOver",
	L"GroupArrival",
	L"Day2AddEMailFromIMP",
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	L"MercComplainEquipment",
	L"HourlyUpdate",
	L"HandleMineIncome",
	L"SetupMineIncome",
	L"QueuedBattle",
	L"LeavingMercArriveInDrassen",
	L"LeavingMercArriveInOmerta",
	L"SetByNPCSystem",
	L"SecondAirportAttendantArrived",
	L"HelicopterHoverTooLong",
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	L"HelicopterHoverWayTooLong",
	L"HelicopterDoneRefuelling",
	L"MercLeaveEquipInOmerta",
	L"MercLeaveEquipInDrassen",
	L"DailyEarlyMorningEvents",
	L"GroupAboutToArrive",
	L"ProcessTacticalSchedule",
	L"BeginRainStorm",
	L"EndRainStorm",
	L"HandleTownOpinion",
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	L"SetupTownOpinion",
	L"DelayedDeathHandling",
	L"BeginAirRaid",
	L"TownLoyaltyUpdate",
	L"Meanwhile",
	L"BeginCreatureQuest",
	L"CreatureSpread",
	L"DecayCreatures",
	L"CreatureNightPlanning",
	L"CreatureAttack",
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	L"EvaluateQueenSituation",
	L"CheckEnemyControlledSector",
	L"TurnOnNightLights",
	L"TurnOffNightLights",
	L"TurnOnPrimeLights",
	L"TurnOffPrimeLights",
	L"MercAboutToLeaveComment",
	L"ForceTimeInterupt",
	L"EnricoEmailEvent",
	L"InsuranceInvestigationStarted",
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	L"InsuranceInvestigationOver",
	L"HandleMinuteUpdate",
	L"TemperatureUpdate",
	L"Keith going out of business",
	L"MERC site back online",
	L"Investigate Sector",
	L"CheckIfMineCleared",
	L"RemoveAssassin",
	L"BandageBleedingMercs",
	L"ShowUpdateMenu",
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	L"SetMenuReason",
	L"AddSoldierToUpdateBox",
	L"BeginContractRenewalSequence",
	L"RPC_WHINE_ABOUT_PAY",
	L"HaventMadeImpCharacterEmail",
	L"Rainstorm",
	L"Quarter Hour Update",
	L"MERC Merc went up level email delay",
	L"CPostalService delivery",
	L".",
#ifdef CRIPPLED_VERSION
	L"Crippled version end game check",
#endif
	L"HelicopterHoverForAMinute",
	L"HelicopterRefuelForAMinute",
	L"MilitiaMovementOrder",
	L"PMCEmail",
	L"PMCReinforcementArrival",
	L"KingpinBounty1",
	L"KingpinBounty2",
	L"KingpinBounty3",
	L"ASDUpdate",
	L"ASDPurchaseFuel",
	L"ASDPurchaseJeep",
	L"ASDPurchaseTank",
	L"ASDPurchaseHeli",
	L"ASDPurchaseRobot",
	L"EnemyHeliUpdate",
	L"EnemyHeliRepair",
	L"EnemyHeliRefuel",
	L"SAMsiteRepaired",
	L"MilitiaWebsiteEmail",
	L"Weather Normal",
	L"Weather Rain",
	L"Weather Thunderstorm",
	L"Weather Sandstorm",
	L"Weather Snow",
	L"Intel Enrico Email",
	L"Intel Photofact verify",
	L"Daily raid events",
	L"bloodcat attack",
	L"zombie attack",
	L"bandit attack",
	L"ArmyFinishTraining",
	L"MiniEvent",
	L"ARC_Event",
	L"ReturnTransportGroup",
};

#endif

void ValidateGameEvents();

STRATEGICEVENT									*gpEventList = NULL;

namespace
{
constexpr UINT32 CAMPAIGN_EVENT_QUEUE_MAGIC = 0x32515645; // "EVQ2"
constexpr UINT16 CAMPAIGN_EVENT_QUEUE_VERSION = 1;

CampaignEventQueue& StrategicEventQueue() noexcept
{
	return GetJa2CampaignEventQueue();
}
}

extern BOOLEAN gfTimeInterruptPause;
BOOLEAN gfPreventDeletionOfAnyEvent = FALSE;
BOOLEAN gfEventDeletionPending = FALSE;

BOOLEAN gfProcessingGameEvents = FALSE;
UINT32	guiTimeStampOfCurrentlyExecutingEvent = 0;

//Determines if there are any events that will be processed between the current global time,
//and the beginning of the next global time.
BOOLEAN GameEventsPending( UINT32 uiAdjustment )
{
	#ifdef CRIPPLED_VERSION
	if( guiDay >= 8 )
	{
		return FALSE;
	}
	#endif
	const STRATEGICEVENT* const nextEvent = StrategicEventQueue().head();
	if( !nextEvent )
		return FALSE;
	if( nextEvent->uiTimeStamp <= GetWorldTotalSeconds() + uiAdjustment )
		return TRUE;
	return FALSE;
}

//returns TRUE if any events were deleted
BOOLEAN DeleteEventsWithDeletionPending()
{
	CampaignEventQueue& queue = StrategicEventQueue();
	STRATEGICEVENT *curr, *prev;
	BOOLEAN fEventDeleted = FALSE;
	//ValidateGameEvents();
	curr = queue.head();
	prev = NULL;
	while( curr )
	{
		//ValidateGameEvents();
		if( curr->ubFlags & SEF_DELETION_PENDING )
		{
			curr = queue.eraseAfter( prev );
			fEventDeleted = TRUE;
			continue;
		}
		prev = curr;
		curr = curr->next;
	}
	SynchronizeJa2CampaignEventListMirror();
	gfEventDeletionPending = FALSE;
	return fEventDeleted;
}


static void AdjustClockToEventStamp( STRATEGICEVENT *pEvent, UINT32 *puiAdjustment )
{
	UINT32 uiDiff;

	uiDiff = pEvent->uiTimeStamp - guiGameClock;
	SetJa2CampaignClockEventTime( pEvent->uiTimeStamp );
	*puiAdjustment -= uiDiff;

	#ifdef CRIPPLED_VERSION
	if( guiDay >= 8 )
	{
		OverrideJa2CampaignClockCalendar( 8, 0, 0 );
		return;
	}

	#endif

	swprintf( WORLDTIMESTR, L"%s %d, %02d:%02d", gpGameClockString[ STR_GAMECLOCK_DAY_NAME ], guiDay, guiHour, guiMin );
}

//If there are any events pending, they are processed, until the time limit is reached, or
//a major event is processed (one that requires the player's attention).
void ProcessPendingGameEvents( UINT32 uiAdjustment, UINT8 ubWarpCode )
{
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"ProcessPendingGameEvents");
	CampaignEventQueue& queue = StrategicEventQueue();
	STRATEGICEVENT *curr, *pEvent, *prev;
	BOOLEAN fDeleteEvent = FALSE;

	#ifdef CRIPPLED_VERSION
	if( guiDay >= 8 )
	{
		return;
	}
	#endif

	gfTimeInterrupt = FALSE;
	gfProcessingGameEvents = TRUE;

	//While we have events inside the time range to be updated, process them...
	curr = queue.head();
	prev = NULL; //prev only used when warping time to target time.
	while( !gfTimeInterrupt && curr && curr->uiTimeStamp <= guiGameClock + uiAdjustment )
	{
		fDeleteEvent = FALSE;
		//Update the time by the difference, but ONLY if the event comes after the current time.
		//In the beginning of the game, series of events are created that are placed in the list
		//BEFORE the start time.	Those events will be processed without influencing the actual time.
		if( curr->uiTimeStamp > guiGameClock && ubWarpCode != WARPTIME_PROCESS_TARGET_TIME_FIRST )
		{
			AdjustClockToEventStamp( curr, &uiAdjustment );
		}
		//Process the event
		if( ubWarpCode != WARPTIME_PROCESS_TARGET_TIME_FIRST )
		{
			fDeleteEvent = ExecuteStrategicEvent( curr );
		}
		else if( curr->uiTimeStamp == guiGameClock + uiAdjustment )
		{ //if we are warping to the target time to process that event first,
			if( !curr->next || curr->next->uiTimeStamp > guiGameClock + uiAdjustment )
			{ //make sure that we are processing the last event for that second
				AdjustClockToEventStamp( curr, &uiAdjustment );

				fDeleteEvent = ExecuteStrategicEvent( curr );
			}
			else
			{ //We are at the current target warp time however, there are still other events following in this time cycle.
				//We will only target the final event in this time.	NOTE:	Events are posted using a FIFO method
				prev = curr;
				curr = curr->next;
				continue;
			}
		}
		else
		{ //We are warping time to the target time.	We haven't found the event yet,
			//so continuing will keep processing the list until we find it.	NOTE:	Events are posted using a FIFO method
			prev = curr;
			curr = curr->next;
			continue;
		}
		if( fDeleteEvent )
		{
			//Determine if event node is a special event requiring reposting
			switch( curr->ubEventType )
			{
				case RANGED_EVENT:
					AddAdvancedStrategicEvent( ENDRANGED_EVENT, curr->ubCallbackID, curr->uiTimeStamp+curr->uiTimeOffset, curr->uiParam );
					break;
				case PERIODIC_EVENT:
					pEvent = AddAdvancedStrategicEvent( PERIODIC_EVENT, curr->ubCallbackID, curr->uiTimeStamp+curr->uiTimeOffset, curr->uiParam );
					if( pEvent )
						pEvent->uiTimeOffset = curr->uiTimeOffset;
					break;
				case EVERYDAY_EVENT:
					AddAdvancedStrategicEvent( EVERYDAY_EVENT, curr->ubCallbackID, curr->uiTimeStamp+NUM_SEC_IN_DAY, curr->uiParam );
					break;
			}
			curr = queue.eraseAfter( prev );
			SynchronizeJa2CampaignEventListMirror();
		}
		else
		{
			prev = curr;
			curr = curr->next;
		}
	}

	gfProcessingGameEvents = FALSE;

	if( gfEventDeletionPending )
	{
		DeleteEventsWithDeletionPending();
	}

	if( uiAdjustment && !gfTimeInterrupt )
		AdvanceJa2CampaignClockUncommitted( uiAdjustment );

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"ProcessPendingGameEvents done");
}


BOOLEAN AddSameDayStrategicEvent( UINT8 ubCallbackID, UINT32 uiMinStamp, UINT32 uiParam )
{
	return( AddStrategicEvent( ubCallbackID, uiMinStamp + GetWorldDayInMinutes(), uiParam ) );
}

BOOLEAN AddSameDayStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiSecondStamp, UINT32 uiParam )
{
	return( AddStrategicEventUsingSeconds( ubCallbackID, uiSecondStamp + GetWorldDayInSeconds(), uiParam ) );
}

BOOLEAN AddFutureDayStrategicEvent( UINT8 ubCallbackID, UINT32 uiMinStamp, UINT32 uiParam, UINT32 uiNumDaysFromPresent )
{
	UINT32 uiDay;
	uiDay = GetWorldDay();
	return( AddStrategicEvent( ubCallbackID, uiMinStamp + GetFutureDayInMinutes( uiDay + uiNumDaysFromPresent ), uiParam ) );
}

BOOLEAN AddFutureDayStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiSecondStamp, UINT32 uiParam, UINT32 uiNumDaysFromPresent )
{
	UINT32 uiDay;
	uiDay = GetWorldDay();
	return( AddStrategicEventUsingSeconds( ubCallbackID, uiSecondStamp + GetFutureDayInMinutes( uiDay + uiNumDaysFromPresent ) * 60, uiParam ) );
}

STRATEGICEVENT* AddAdvancedStrategicEvent( UINT8 ubEventType, UINT8 ubCallbackID, UINT32 uiTimeStamp, UINT32 uiParam )
{
	if( gfProcessingGameEvents && uiTimeStamp <= guiTimeStampOfCurrentlyExecutingEvent )
	{ //Prevents infinite loops of posting events that are the same time or earlier than the event
		//currently being processed.
		#ifdef JA2TESTVERSION
			//if( ubCallbackID == EVENT_PROCESS_TACTICAL_SCHEDULE )
			{
				ScreenMsg( FONT_RED, MSG_DEBUG, L"%s Event Rejected:	Can't post events <= time while inside an event callback.	This is a special case situation that isn't a bug.", gEventName[ ubCallbackID ] );
			}
			//else
			//{
			//	AssertMsg( 0, String( "%S Event Rejected:	Can't post events <= time while inside an event callback.", gEventName[ ubCallbackID ] ) );
			//}
		#endif
		return NULL;
	}

	const CampaignEventScheduleResult scheduled =
		StrategicEventQueue().schedule(CampaignEventSnapshot{
			uiTimeStamp, uiParam, 0, ubEventType, ubCallbackID, 0});
	if( !scheduled )
	{
		AssertMsg( FALSE, "Campaign event queue rejected a strategic event" );
		return NULL;
	}
	SynchronizeJa2CampaignEventListMirror();
	return scheduled.event;
}

BOOLEAN AddStrategicEvent( UINT8 ubCallbackID, UINT32 uiMinStamp, UINT32 uiParam )
{
	if( AddAdvancedStrategicEvent( ONETIME_EVENT, ubCallbackID, uiMinStamp*60, uiParam ) )
		return TRUE;
	return FALSE;
}

BOOLEAN AddStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiSecondStamp, UINT32 uiParam )
{
	if( AddAdvancedStrategicEvent( ONETIME_EVENT, ubCallbackID, uiSecondStamp, uiParam ) )
		return TRUE;
	return FALSE;
}


BOOLEAN AddRangedStrategicEvent( UINT8 ubCallbackID, UINT32 uiStartMin, UINT32 uiLengthMin, UINT32 uiParam )
{
	STRATEGICEVENT *pEvent;
	pEvent = AddAdvancedStrategicEvent( RANGED_EVENT, ubCallbackID, uiStartMin*60, uiParam );
	if( pEvent )
	{
		pEvent->uiTimeOffset = uiLengthMin * 60;
		return TRUE;
	}
	return FALSE;
}

BOOLEAN AddSameDayRangedStrategicEvent( UINT8 ubCallbackID, UINT32 uiStartMin, UINT32 uiLengthMin, UINT32 uiParam)
{
	return AddRangedStrategicEvent( ubCallbackID, uiStartMin + GetWorldDayInMinutes(), uiLengthMin, uiParam );
}

BOOLEAN AddFutureDayRangedStrategicEvent( UINT8 ubCallbackID, UINT32 uiStartMin, UINT32 uiLengthMin, UINT32 uiParam, UINT32 uiNumDaysFromPresent )
{
	return AddRangedStrategicEvent( ubCallbackID, uiStartMin + GetFutureDayInMinutes( GetWorldDay() + uiNumDaysFromPresent ), uiLengthMin, uiParam );
}

BOOLEAN AddRangedStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiStartSeconds, UINT32 uiLengthSeconds, UINT32 uiParam )
{
	STRATEGICEVENT *pEvent;
	pEvent = AddAdvancedStrategicEvent( RANGED_EVENT, ubCallbackID, uiStartSeconds, uiParam );
	if( pEvent )
	{
		pEvent->uiTimeOffset = uiLengthSeconds;
		return TRUE;
	}
	return FALSE;
}

BOOLEAN AddSameDayRangedStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiStartSeconds, UINT32 uiLengthSeconds, UINT32 uiParam)
{
	return AddRangedStrategicEventUsingSeconds( ubCallbackID, uiStartSeconds + GetWorldDayInSeconds(), uiLengthSeconds, uiParam );
}

BOOLEAN AddFutureDayRangedStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiStartSeconds, UINT32 uiLengthSeconds, UINT32 uiParam, UINT32 uiNumDaysFromPresent )
{
	return AddRangedStrategicEventUsingSeconds( ubCallbackID, uiStartSeconds + GetFutureDayInMinutes( GetWorldDay() + uiNumDaysFromPresent ) * 60, uiLengthSeconds, uiParam );
}

BOOLEAN AddEveryDayStrategicEvent( UINT8 ubCallbackID, UINT32 uiStartMin, UINT32 uiParam )
{
	if( AddAdvancedStrategicEvent( EVERYDAY_EVENT, ubCallbackID, GetWorldDayInSeconds() + uiStartMin * 60, uiParam ) )
		return TRUE;
	return FALSE;
}

BOOLEAN AddEveryDayStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiStartSeconds, UINT32 uiParam )
{
	if( AddAdvancedStrategicEvent( EVERYDAY_EVENT, ubCallbackID, GetWorldDayInSeconds() + uiStartSeconds, uiParam ) )
		return TRUE;
	return FALSE;
}

//NEW:	Period Events
//Event will get processed automatically once every X minutes.
BOOLEAN AddPeriodStrategicEvent( UINT8 ubCallbackID, UINT32 uiOnceEveryXMinutes, UINT32 uiParam )
{
	STRATEGICEVENT *pEvent;
	pEvent = AddAdvancedStrategicEvent( PERIODIC_EVENT, ubCallbackID, GetWorldDayInSeconds() + uiOnceEveryXMinutes * 60, uiParam );
	if( pEvent )
	{
		pEvent->uiTimeOffset = uiOnceEveryXMinutes * 60;
		return TRUE;
	}
	return FALSE;
}

BOOLEAN AddPeriodStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiOnceEveryXSeconds, UINT32 uiParam )
{
	STRATEGICEVENT *pEvent;
	pEvent = AddAdvancedStrategicEvent( PERIODIC_EVENT, ubCallbackID, GetWorldDayInSeconds() + uiOnceEveryXSeconds, uiParam );
	if( pEvent )
	{
		pEvent->uiTimeOffset = uiOnceEveryXSeconds;
		return TRUE;
	}
	return FALSE;
}

BOOLEAN AddPeriodStrategicEventWithOffset( UINT8 ubCallbackID, UINT32 uiOnceEveryXMinutes, UINT32 uiOffsetFromCurrent, UINT32 uiParam )
{
	STRATEGICEVENT *pEvent;
	pEvent = AddAdvancedStrategicEvent( PERIODIC_EVENT, ubCallbackID, GetWorldDayInSeconds() + uiOffsetFromCurrent * 60, uiParam );
	if( pEvent )
	{
		pEvent->uiTimeOffset = uiOnceEveryXMinutes * 60;
		return TRUE;
	}
	return FALSE;
}

BOOLEAN AddPeriodStrategicEventUsingSecondsWithOffset( UINT8 ubCallbackID, UINT32 uiOnceEveryXSeconds, UINT32 uiOffsetFromCurrent, UINT32 uiParam )
{
	STRATEGICEVENT *pEvent;
	pEvent = AddAdvancedStrategicEvent( PERIODIC_EVENT, ubCallbackID, GetWorldDayInSeconds() + uiOffsetFromCurrent, uiParam );
	if( pEvent )
	{
		pEvent->uiTimeOffset = uiOnceEveryXSeconds;
		return TRUE;
	}
	return FALSE;
}

void DeleteAllStrategicEventsOfType( UINT8 ubCallbackID )
{
	CampaignEventQueue& queue = StrategicEventQueue();
	STRATEGICEVENT	*curr, *prev;
	prev = NULL;
	curr = queue.head();
	while( curr )
	{
		if( curr->ubCallbackID == ubCallbackID && !(curr->ubFlags & SEF_DELETION_PENDING) )
		{
			if( gfPreventDeletionOfAnyEvent )
			{
				curr->ubFlags |= SEF_DELETION_PENDING;
				gfEventDeletionPending = TRUE;
				prev = curr;
				curr = curr->next;
				continue;
			}
			curr = queue.eraseAfter( prev );
		}
		else
		{	//Advance all the nodes
			prev = curr;
			curr = curr->next;
		}
	}
	SynchronizeJa2CampaignEventListMirror();
}

void DeleteAllStrategicEvents()
{
	StrategicEventQueue().clear();
	SynchronizeJa2CampaignEventListMirror();
	gfEventDeletionPending = FALSE;
}

//Searches for and removes the first event matching the supplied information.	There may very well be a need
//for more specific event removal, so let me know (Kris), of any support needs.	Function returns FALSE if
//no events were found or if the event wasn't deleted due to delete lock,
BOOLEAN DeleteStrategicEvent( UINT8 ubCallbackID, UINT32 uiParam )
{
	CampaignEventQueue& queue = StrategicEventQueue();
	STRATEGICEVENT *curr, *prev;
	curr = queue.head();
	prev = NULL;
	while( curr )
	{ //deleting middle
		if( curr->ubCallbackID == ubCallbackID && curr->uiParam == uiParam )
		{
			if( !(curr->ubFlags & SEF_DELETION_PENDING) )
			{
				if( gfPreventDeletionOfAnyEvent )
				{
					curr->ubFlags |= SEF_DELETION_PENDING;
					gfEventDeletionPending = TRUE;
					return FALSE;
				}
				(void)queue.eraseAfter( prev );
				SynchronizeJa2CampaignEventListMirror();
				return TRUE;
			}
		}
		prev = curr;
		curr = curr->next;
	}
	return FALSE;
}

std::vector< std::pair<UINT32, UINT32> > GetAllStrategicEventsOfType( UINT8 ubCallbackID )
{
	std::vector< std::pair<UINT32, UINT32> > vec;

	const STRATEGICEVENT* curr = StrategicEventQueue().head();
	while ( curr )
	{
		if ( curr->ubCallbackID == ubCallbackID )
		{
			vec.push_back( std::pair<UINT32, UINT32>( curr->uiTimeStamp, curr->uiParam ) );
		}

		curr = curr->next;
	}

	return vec;
}

//part of the game.sav files (not map files)
BOOLEAN SaveStrategicEventsToSavedGame( HWFILE hFile )
{
	std::vector<CampaignEventSnapshot> events;
	if( !StrategicEventQueue().capture( events ) )
		return FALSE;

	SaveWriter writer( hFile );
	writer.u32( CAMPAIGN_EVENT_QUEUE_MAGIC );
	writer.u16( CAMPAIGN_EVENT_QUEUE_VERSION );
	writer.u32( static_cast<UINT32>( events.size() ) );
	for( const CampaignEventSnapshot& event : events )
	{
		writer.u32( event.scheduledSeconds );
		writer.u32( event.parameter );
		writer.u32( event.timeOffsetSeconds );
		writer.u8( event.type );
		writer.u8( event.callbackId );
		writer.u8( event.flags );
	}
	return writer.good() ? TRUE : FALSE;
}


BOOLEAN LoadStrategicEventsFromSavedGame( HWFILE hFile )
{
	SaveReader reader( hFile );
	if( reader.u32() != CAMPAIGN_EVENT_QUEUE_MAGIC ||
		reader.u16() != CAMPAIGN_EVENT_QUEUE_VERSION )
		return FALSE;

	const UINT32 eventCount = reader.u32();
	CampaignEventQueue& queue = StrategicEventQueue();
	if( !reader.good() || eventCount > queue.maximumEvents() )
		return FALSE;

	std::vector<CampaignEventSnapshot> events;
	try
	{
		events.reserve( eventCount );
		for( UINT32 index = 0; index < eventCount; ++index )
		{
			CampaignEventSnapshot event;
			event.scheduledSeconds = reader.u32();
			event.parameter = reader.u32();
			event.timeOffsetSeconds = reader.u32();
			event.type = reader.u8();
			event.callbackId = reader.u8();
			event.flags = reader.u8();
			events.push_back( event );
		}
	}
	catch( ... )
	{
		return FALSE;
	}
	if( !reader.good() )
		return FALSE;

	if( queue.replace( events ) != CampaignEventQueueError::None )
		return FALSE;
	SynchronizeJa2CampaignEventListMirror();

	InitMiniEvents();

	return( TRUE );
}

void LockStrategicEventFromDeletion( STRATEGICEVENT *pEvent )
{
	pEvent->ubFlags |= SEF_PREVENT_DELETION;
}

void UnlockStrategicEventFromDeletion( STRATEGICEVENT *pEvent )
{
	pEvent->ubFlags &= ~SEF_PREVENT_DELETION;
}

void ValidateGameEvents()
{
	AssertMsg(
		StrategicEventQueue().validate(),
		"Campaign event queue ownership or ordering invariant failed" );
}
