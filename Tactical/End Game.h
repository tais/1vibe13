#ifndef __ENDGAME_H
#define __ENDGAME_H


BOOLEAN DoesO3SectorStatueExistHere( INT32 sGridNo );
void ChangeO3SectorStatue( BOOLEAN fFromExplosion );

void HandleDoneLastKilledQueenQuote( );
void HandleDoneLastEndGameQuote( );
void DoneFadeOutJa25EndCinematic( void );
void EndGameEveryoneSayTheirGoodByQuotes( void );

void HandleJa25EndGameAndGoToCreditsScreen( BOOLEAN fFromTactical );
void HandleEveryoneDoneTheirEndGameQuotes();
void EnterTacticalInFinalSector();
extern	BOOLEAN			gfPlayersLaptopWasntWorkingAtEndOfGame;
void HandleDeidrannaDeath( TacticalActor *pKillerSoldier, INT32 sGridNo, INT8 bLevel );
void BeginHandleDeidrannaDeath( TacticalActor *pKillerSoldier, INT32 sGridNo, INT8 bLevel );
void EndQueenDeathEndgameBeginEndCimenatic( );
void EndQueenDeathEndgame( );

void HandleQueenBitchDeath( TacticalActor *pKillerSoldier, INT32 sGridNo, INT8 bLevel );
void BeginHandleQueenBitchDeath( TacticalActor *pKillerSoldier, INT32 sGridNo, INT8 bLevel );

#endif
