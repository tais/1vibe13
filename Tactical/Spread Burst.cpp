#include "TacticalActorWeaponHandling.h"
#include "TacticalActor.h"
	#include "stdlib.h"
	#include "DEBUG.H"
	#include "Weapons.h"
	#include "Soldier Find.h"
	#include "Isometric Utils.h"
	#include "renderworld.h"
	#include "Render Dirty.h"
	#include "Interface.h"
	#include "Spread burst.h"
	#include "Points.h"

//forward declarations of common classes to eliminate includes
class OBJECTTYPE;
class TacticalActor;



BURST_LOCATIONS			gsBurstLocations[ MAX_BURST_LOCATIONS ];
INT8					gbNumBurstLocations = 0;

extern BOOLEAN gfBeginBurstSpreadTracking;


void ResetBurstLocations( )
{
	gbNumBurstLocations = 0;
}


void InternalAccumulateBurstLocation( INT32 sGridNo )
{
	INT32 cnt;
	if ( gbNumBurstLocations < MAX_BURST_LOCATIONS )
	{
		// Check if it already exists!
		for ( cnt = 0; cnt < gbNumBurstLocations; cnt++ )
		{
			if ( gsBurstLocations[ cnt ].sGridNo == sGridNo )
			{
				return;
			}
		}

		gsBurstLocations[ gbNumBurstLocations ].sGridNo = sGridNo;

		// Get cell X, Y from mouse...
		GetMouseWorldCoords( &( gsBurstLocations[ gbNumBurstLocations ].sX ), &( gsBurstLocations[ gbNumBurstLocations ].sY ) );

		gbNumBurstLocations++;
	}
}

//Madd: to add a bit more usefulness to spread fire, I'm making it so that
//it will automatically latch onto enemies within iSearchRange tiles of the mouse drag.
void AccumulateBurstLocation( INT32 sGridNo )
{
	TacticalActor* pTarget;
	int iSearchRange = 2; // number of tiles beside the mouse drag to look at
	INT32	sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset, sAdjacentGridNo;
	BOOLEAN foundTarget = FALSE;

		//first see if we can find a guy standing right on the spot
		pTarget = SimpleFindSoldier(sGridNo,0);
		if (!pTarget)
			pTarget = SimpleFindSoldier(sGridNo, 1); //try on a roof

		if (pTarget)
		{
			InternalAccumulateBurstLocation(sGridNo);
			foundTarget = TRUE;
		}
		//let's now look around this square - maybe there are some adjacent enemies we can latch onto

		// stay away from the edges

		// determine maximum horizontal limits
		sMaxLeft	= min( iSearchRange, (sGridNo % MAXCOL));
		sMaxRight = min( iSearchRange, MAXCOL - ((sGridNo % MAXCOL) + 1));

		// determine maximum vertical limits
		sMaxUp	= min( iSearchRange, (sGridNo / MAXROW));
		sMaxDown = min( iSearchRange, MAXROW - ((sGridNo / MAXROW) + 1));

		// reset the "reachable" flags in the region we're looking at
		for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
		{
			for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
			{
				sAdjacentGridNo = sGridNo + sXOffset + (MAXCOL * sYOffset);
				if ( !(sAdjacentGridNo >=0 && sAdjacentGridNo < WORLD_MAX) )
				{
					continue;
				}

				pTarget = SimpleFindSoldier(sAdjacentGridNo,0); // look for a guy in that gridno

				if (!pTarget)
					pTarget = SimpleFindSoldier(sAdjacentGridNo, 1); //try on a roof

				if (pTarget)
				{
					InternalAccumulateBurstLocation(sAdjacentGridNo); //there's somebody there! let's latch onto him
					foundTarget = TRUE;
				}
			}
		}


		if ( !foundTarget )
		{
			//didn't find anyone nearby, but we should target this space anyway to prevent players from abusing this feature
			InternalAccumulateBurstLocation(sGridNo);
		}
}


void PickBurstLocations( TacticalActor *pSoldier )
{
	UINT8		ubShotsPerBurst = 0;
	FLOAT		dAccumulator = 0;
	FLOAT		dStep = 0;
	INT32		cnt;
	UINT8		ubLocationNum;

	// OK, using the # of locations, spread them evenly between our current weapon shots per burst value

	// Get shots per burst
	//DIGICRAB: Burst UnCap
	//if we fire more than MAX_BURST_SPREAD_TARGETS bullets, make sure there's no buffer overflow
	if(pSoldier->fireControl().autofireShots())
	{
		INT16	sAPCosts;

		if ( pSoldier->fireControl().autofireShots() <= gbNumBurstLocations )
		{
			pSoldier->fireControl().autofireShots() = 1;
			do
			{
				pSoldier->fireControl().autofireShots()++;
				sAPCosts = CalcTotalAPsToAttack( pSoldier, gsBurstLocations[0].sGridNo, TRUE, pSoldier->aiPlanning().shownAimTime());
			}
			while(EnoughPoints( pSoldier, sAPCosts, 0, FALSE ) && pSoldier->inventory()[ pSoldier->attackSelection().hand() ][0]->data.gun.ubGunShotsLeft >= pSoldier->fireControl().autofireShots() && gbNumBurstLocations >= pSoldier->fireControl().autofireShots());
			pSoldier->fireControl().autofireShots()--;

			ubShotsPerBurst = pSoldier->fireControl().autofireShots();
		}
		else if ( gbNumBurstLocations > 0 )
			ubShotsPerBurst = pSoldier->fireControl().autofireShots(); // / gbNumBurstLocations;

	}
	else
	{
		if ( pSoldier->attackSelection().weaponMode() == WM_ATTACHED_GL_BURST )
			ubShotsPerBurst = Weapon[GetAttachedGrenadeLauncher(&pSoldier->inventory()[HANDPOS])].ubShotsPerBurst;
		else
			ubShotsPerBurst = GetShotsPerBurst(&pSoldier->inventory()[ HANDPOS ]);
	}

	ubShotsPerBurst =
		SoldierFireControlComponent::clampSpreadTargetCount(ubShotsPerBurst);

	if (ubShotsPerBurst == 1)
	{
		pSoldier->fireControl().spreadIndex() = FALSE;
		return;
	}

	// Use # gridnos accululated and # burst shots to determine accululator
	// Calculate it so that the actual last chosen shot location is the last spread point
	dStep = (gbNumBurstLocations-1) / (FLOAT)(ubShotsPerBurst-1);

	//Loop through our shots!
	for ( cnt = 0; cnt < ubShotsPerBurst; cnt++ )
	{
		// Get index into list
		ubLocationNum = (UINT8)( dAccumulator );

		// Add to merc location
		pSoldier->fireControl().spreadLocations()[ cnt ] = gsBurstLocations[ ubLocationNum ].sGridNo;
		DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("PickBurstLocations: loc#%d = %d", cnt, pSoldier->fireControl().spreadLocations()[ cnt ]));
		// Acculuate index value
		dAccumulator += dStep;
	}

	for (; cnt < SoldierFireControlComponent::SpreadTargetCapacity; cnt++)
	{
		pSoldier->fireControl().spreadLocations()[ cnt ] = 0;
	}

	// OK, they have been added
}

void AIPickBurstLocations( TacticalActor *pSoldier, INT8 bTargets, TacticalActor *pTargets[5] )
{
	UINT8		ubShotsPerBurst;
	FLOAT		dAccululator = 0;
	FLOAT		dStep = 0;
	INT32		cnt;
	UINT8		ubLocationNum;

	// OK, using the # of locations, spread them evenly between our current weapon shots per burst value

	// Get shots per burst
	//DIGICRAB: Burst UnCap
	//if we fire more than MAX_BURST_SPREAD_TARGETS bullets, make sure there's no buffer overflow
	if(pSoldier->fireControl().autofireShots())
		ubShotsPerBurst = __min(pSoldier->fireControl().autofireShots(), SoldierFireControlComponent::SpreadTargetCapacity);
	else
		ubShotsPerBurst = __min(GetShotsPerBurst (&pSoldier->inventory()[ HANDPOS ]), SoldierFireControlComponent::SpreadTargetCapacity);

	if ( TacticalActorWeaponHandling::isValidSecondHandBurst(*pSoldier) )
		ubShotsPerBurst = ubShotsPerBurst*2;

	ubShotsPerBurst =
		SoldierFireControlComponent::clampSpreadTargetCount(ubShotsPerBurst);

	if ( ubShotsPerBurst <= 0 )
		ubShotsPerBurst = 1;

	// Use # gridnos accululated and # burst shots to determine accululator
	//dStep = gbNumBurstLocations / (FLOAT)ubShotsPerBurst;
	// CJC: tweak!
	dStep = bTargets / (FLOAT)ubShotsPerBurst;

	//Loop through our shots!
	for ( cnt = 0; cnt < ubShotsPerBurst; cnt++ )
	{
		// Get index into list
		ubLocationNum = (UINT8)( dAccululator );

		// Add to merc location
		pSoldier->fireControl().spreadLocations()[ cnt ] = pTargets[ubLocationNum]->position().gridNo();

		// Acculuate index value
		dAccululator += dStep;
	}

	for (; cnt < SoldierFireControlComponent::SpreadTargetCapacity; cnt++)
	{
		pSoldier->fireControl().spreadLocations()[ cnt ] = 0;
	}
	// OK, they have been added
}


void RenderAccumulatedBurstLocations( )
{
	INT32			cnt;
	INT32 sGridNo;
	HVOBJECT	hVObject;

	if ( !gfBeginBurstSpreadTracking )
	{
		return;
	}

	if ( gbNumBurstLocations == 0 )
	{
		return;
	}

	// Loop through each location...
	GetVideoObject( &hVObject, guiBURSTACCUM );

	// If on screen, render

	// Check if it already exists!
	for ( cnt = 0; cnt < gbNumBurstLocations; cnt++ )
	{
		sGridNo = gsBurstLocations[ cnt ].sGridNo;

		if ( GridNoOnScreen( sGridNo ) )
		{
			FLOAT				dOffsetX, dOffsetY;
			FLOAT				dTempX_S, dTempY_S;
			INT16				sXPos, sYPos;
			INT32				iBack;

			dOffsetX = (FLOAT)( gsBurstLocations[ cnt ].sX - gsRenderCenterX );
			dOffsetY = (FLOAT)( gsBurstLocations[ cnt ].sY - gsRenderCenterY );

			// Calculate guy's position
			FloatFromCellToScreenCoordinates( dOffsetX, dOffsetY, &dTempX_S, &dTempY_S );

			sXPos = ( ( gsVIEWPORT_END_X - gsVIEWPORT_START_X ) /2 ) + (INT16)dTempX_S;
			sYPos = ( ( gsVIEWPORT_END_Y - gsVIEWPORT_START_Y ) /2 ) + (INT16)dTempY_S - gpWorldLevelData[ sGridNo ].sHeight;

			// Adjust for offset position on screen
			sXPos -= gsRenderWorldOffsetX;
			sYPos -= gsRenderWorldOffsetY;

			// Adjust for render height
			sYPos += gsRenderHeight;

			//sScreenY -= gpWorldLevelData[ sGridNo ].sHeight;

			// Center circle!
			//sXPos -= 10;
			//sYPos -= 10;

			iBack = RegisterBackgroundRect( BGND_FLAG_SINGLE, NULL, sXPos, sYPos, (INT16)(sXPos +40 ), (INT16)(sYPos + 40 ) );
			if ( iBack != -1 )
			{
				SetBackgroundRectFilled( iBack );
			}

			BltVideoObject(	FRAME_BUFFER, hVObject, 1, sXPos, sYPos, VO_BLT_SRCTRANSPARENCY, NULL );
		}
	}
}



