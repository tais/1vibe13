#ifndef _SPREAD_BURST_H
#define _SPREAD_BURST_H

#define		MAX_BURST_LOCATIONS		50

typedef struct
{
	INT16 sX;
	INT16 sY;
	INT32 sGridNo;

} BURST_LOCATIONS;


extern BURST_LOCATIONS			gsBurstLocations[ MAX_BURST_LOCATIONS ];
extern INT8					gbNumBurstLocations;


void ResetBurstLocations( );
void AccumulateBurstLocation( INT32 sGridNo );
void PickBurstLocations( TacticalActor *pSoldier );
void AIPickBurstLocations( TacticalActor *pSoldier, INT8 bTargets, TacticalActor *pTargets[5] );

void RenderAccumulatedBurstLocations( );

#endif
