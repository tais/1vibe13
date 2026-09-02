#ifndef _DOORS_H
#define _DOORS_H

#include <cstdint>

#define HANDLE_DOOR_OPEN				1
#define HANDLE_DOOR_EXAMINE			2
#define HANDLE_DOOR_LOCKPICK		3
#define HANDLE_DOOR_FORCE				4
#define HANDLE_DOOR_LOCK				5
#define HANDLE_DOOR_UNLOCK			6
#define HANDLE_DOOR_EXPLODE			7
#define HANDLE_DOOR_UNTRAP			8
#define HANDLE_DOOR_CROWBAR			9

extern BOOLEAN gfSetPerceivedDoorState;


BOOLEAN HandleOpenableStruct( TacticalActor *pSoldier, INT32 sGridNo, STRUCTURE *pStructure );

void InteractWithOpenableStruct( TacticalActor *pSoldier, STRUCTURE *pStructure, UINT8 ubDirection, BOOLEAN fDoor );

void InteractWithClosedDoor( TacticalActor *pSoldier, UINT8 ubHandleCode );

void SetDoorString( INT32 sGridNo );

void HandleDoorChangeFromGridNo( TacticalActor *pSoldier, INT32 sGridNo, BOOLEAN fNoAnimations );

enum class ImmediateDoorOpenCloseResult
{
	Rejected,
	Applied,
	IntegrityFailure
};

// Synchronous no-animation door mutation used only after the authoritative
// command has validated actor policy, identity, adjacency, visibility, and
// point costs. Presentation audio and animation are suppressed; the caller
// emits gameplay noise only after this mutation and AP/BP deduction succeed.
// On success resultingBase names the newly installed partner.
ImmediateDoorOpenCloseResult TryHandleDoorOpenCloseImmediately(
	TacticalActor& actor,
	INT32 baseGrid,
	std::uint16_t expectedStructureId,
	bool desiredOpen,
	STRUCTURE*& resultingBase) noexcept;



#endif
