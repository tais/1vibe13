#ifndef __GAME_EVENTS_H
#define __GAME_EVENTS_H

#include "Game Event Hook.h"
#include "FileMan.h"
#include <Engine/Adapters/JA2/CampaignEventQueue.h>

#define SEF_PREVENT_DELETION	0x01
#define SEF_DELETION_PENDING	0x02

using STRATEGICEVENT = CampaignEventQueueNode;

enum
{
	ONETIME_EVENT,
	RANGED_EVENT,
	ENDRANGED_EVENT,
	EVERYDAY_EVENT,
	PERIODIC_EVENT,
	QUEUED_EVENT
};

void LockStrategicEventFromDeletion( STRATEGICEVENT *pEvent );
void UnlockStrategicEventFromDeletion( STRATEGICEVENT *pEvent );

//part of the game.sav files (not map files)
BOOLEAN SaveStrategicEventsToSavedGame( HWFILE hFile );
BOOLEAN LoadStrategicEventsFromSavedGame( HWFILE hFile );

STRATEGICEVENT* AddAdvancedStrategicEvent( UINT8 ubEventType, UINT8 ubCallbackID, UINT32 uiTimeStamp, UINT32 uiParam );	

BOOLEAN ExecuteStrategicEvent( STRATEGICEVENT *pEvent );

extern BOOLEAN gfEventDeletionPending;

BOOLEAN DeleteEventsWithDeletionPending();

extern STRATEGICEVENT *gpEventList;

#endif
