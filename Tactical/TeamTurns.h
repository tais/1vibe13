#ifndef TEAMTURNS_H
#define TEAMTURNS_H

#include "Overhead Types.h"

class TacticalActor;

extern UINT16 gubOutOfTurnPersons;
extern UINT16 gubOutOfTurnOrder[MAXMERCS] ;
extern SoldierID gubLastInterruptedGuy;
extern BOOLEAN gfHiddenInterrupt;
extern BOOLEAN gfHiddenTurnbased;

#define INTERRUPT_QUEUED (gubOutOfTurnPersons > 0)

extern BOOLEAN StandardInterruptConditionsMet( TacticalActor * pSoldier, SoldierID ubOpponentID, INT8 bOldOppList);
extern INT8 CalcInterruptDuelPts( TacticalActor * pSoldier, SoldierID ubOpponentID, BOOLEAN fUseWatchSpots );
extern void EndAITurn( BOOLEAN fReplicateInterrupt = TRUE );
extern void DisplayHiddenInterrupt( TacticalActor * pSoldier );
extern BOOLEAN InterruptDuel( TacticalActor * pSoldier, TacticalActor * pOpponent);
extern void AddToIntList( UINT16 ubID, BOOLEAN fGainControl, BOOLEAN fCommunicate );
extern void DoneAddingToIntList( TacticalActor * pSoldier, BOOLEAN fChange, UINT8 ubInterruptType);

void FreezeInterfaceForEnemyTurn( void );
void ClearIntList( void );
void EndInterrupt(
	BOOLEAN fMarkInterruptOccurred,
	BOOLEAN fReplicateInterrupt);

BOOLEAN	SaveTeamTurnsToTheSaveGameFile( HWFILE hFile );

BOOLEAN	LoadTeamTurnsFromTheSavedGameFile( HWFILE hFile );

void EndAllAITurns( void );
void EndTurnEvents( void );

BOOLEAN NPCFirstDraw( TacticalActor * pSoldier, TacticalActor * pTargetSoldier );

#endif
