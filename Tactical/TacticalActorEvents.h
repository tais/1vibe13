#pragma once

#include "types.h"

class TacticalActor;

// Compatibility entry points that translate actor requests into tactical
// events (and multiplayer messages where applicable).
void SendSoldierPositionEvent(
	TacticalActor* actor, FLOAT newX, FLOAT newY);
void SendSoldierDestinationEvent(
	TacticalActor* actor, UINT32 destination);
void SendGetNewSoldierPathEvent(
	TacticalActor* actor, INT32 destinationGrid, UINT16 movementAnimation);
void SendSoldierSetDirectionEvent(
	TacticalActor* actor, UINT16 direction);
void SendSoldierSetDesiredDirectionEvent(
	TacticalActor* actor, UINT16 desiredDirection);
void SendChangeSoldierStanceEvent(
	TacticalActor* actor, UINT8 newStance);
void SendBeginFireWeaponEvent(
	TacticalActor* actor, INT32 targetGrid);
void SendBeginFireWeaponEvent(
	TacticalActor* actor, INT32 targetGrid,
	INT8 targetLevel, INT8 targetCubeLevel);
