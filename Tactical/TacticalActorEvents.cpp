#include "TacticalActorEvents.h"

#include "Debug Control.h"
#include "Event Pump.h"
#include "TacticalActor.h"
#include "TacticalActorOrientation.h"
#include "connect.h"

void SendSoldierPositionEvent(
	TacticalActor* actor, FLOAT newX, FLOAT newY)
{
	EV_S_SETPOSITION event{};
	event.usSoldierID = actor->identity().id();
	event.uiUniqueId = actor->identity().incarnation();
	event.dNewXPos = newX;
	event.dNewYPos = newY;
	AddGameEvent(S_SETPOSITION, 0, &event);
}

void SendSoldierDestinationEvent(
	TacticalActor* actor, UINT32 destination)
{
	EV_S_CHANGEDEST event{};
	event.usSoldierID = actor->identity().id();
	event.usNewDestination = destination;
	event.uiUniqueId = actor->identity().incarnation();
	AddGameEvent(S_CHANGEDEST, 0, &event);
}

void SendSoldierSetDirectionEvent(
	TacticalActor* actor, UINT16 direction)
{
	EV_S_SETDIRECTION event{};
	event.usSoldierID = actor->identity().id();
	event.usNewDirection = direction;
	event.uiUniqueId = actor->identity().incarnation();
	AddGameEvent(S_SETDIRECTION, 0, &event);
}

void SendSoldierSetDesiredDirectionEvent(
	TacticalActor* actor, UINT16 desiredDirection)
{
	EV_S_SETDESIREDDIRECTION event{};
	event.usSoldierID = actor->identity().id();
	event.usDesiredDirection = desiredDirection;
	event.uiUniqueId = actor->identity().incarnation();
	AddGameEvent(S_SETDESIREDDIRECTION, 0, &event);
	if (is_server || (is_client && actor->identity().id() < 20))
		send_dir(actor, desiredDirection);
}

void SendGetNewSoldierPathEvent(
	TacticalActor* actor,
	INT32 destinationGrid,
	UINT16 movementAnimation)
{
	EV_S_GETNEWPATH event{};
	event.usSoldierID = actor->identity().id();
	event.sDestGridNo = destinationGrid;
	event.usMovementAnim = movementAnimation;
	event.uiUniqueId = actor->identity().incarnation();
	AddGameEvent(S_GETNEWPATH, 0, &event);
}

void SendChangeSoldierStanceEvent(
	TacticalActor* actor, UINT8 newStance)
{
	if (((actor->identity().id() > 19 && !is_server) ||
		 (actor->identity().id() > 119 && is_server)) &&
		is_networked)
	{
		return;
	}

	(void)TacticalActorOrientation::changeStance(*actor, newStance);
	if (is_server || (is_client && actor->identity().id() < 20))
		send_stance(actor, newStance);
}

void SendBeginFireWeaponEvent(
	TacticalActor* actor, INT32 targetGrid)
{
	SendBeginFireWeaponEvent(
		actor,
		targetGrid,
		actor->targeting().level(),
		actor->targeting().cubeLevel());
}

void SendBeginFireWeaponEvent(
	TacticalActor* actor,
	INT32 targetGrid,
	INT8 targetLevel,
	INT8 targetCubeLevel)
{
	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, String("SendBeginFireWeaponEvent"));
	EV_S_BEGINFIREWEAPON event{};
	event.usSoldierID = actor->identity().id();
	event.sTargetGridNo = targetGrid;
	event.bTargetLevel = targetLevel;
	event.bTargetCubeLevel = targetCubeLevel;
	event.uiUniqueId = actor->identity().incarnation();
	AddGameEvent(S_BEGINFIREWEAPON, 0, &event);
}
