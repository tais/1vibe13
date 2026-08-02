#include "TacticalActorTurnBudget.h"
#include "Soldier Control.h"
	#include <stdlib.h>
	#include "TacticalActorConditions.h"
	#include "Isometric Utils.h"
	#include "ai.h"
	#include "AIInternals.h"
	#include "LOS.h"
	#include "Weapons.h"
	#include "opplist.h"
	#include "PATHAI.H"
	#include "Items.h"
	#include "World Items.h"
	#include "Points.h"
	#include "message.h"
	#include "Map Edgepoints.h"
	#include "renderworld.h"
	#include "Render Fun.h"
	#include "Boxing.h"
	#include "Text.h"
	#ifdef _DEBUG
		#include "renderworld.h"
		#include "video.h"
	#endif
	#include "worldman.h"
	#include "strategicmap.h"
	#include "environment.h"
	#include "lighting.h"
	#include "Buildings.h"
	#include "GameSettings.h"
	#include "Soldier Profile.h"
	#include "Rotting Corpses.h"	// sevenfm
	#include "SoldierRepository.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////
// SANDRO - all "APBPConstants[AP_CROUCH]" and "APBPConstants[AP_PRONE]" here    
//           were changed to GetAPsCrouch() and GetAPsProne()					 
//		  - also all "APBPConstants[AP_PICKUP_ITEM]" were replaced by GetBasicAPsToPickupItem()
////////////////////////////////////////////////////////////////////////////////////////////////////////

	INT16 * gsCoverValue = NULL;
#ifdef _DEBUG

	INT16	gsBestCover;
	#ifndef PATHAI_VISIBLE_DEBUG
		// NB Change this to true to get visible cover debug -- CJC
		BOOLEAN gfDisplayCoverValues = FALSE;
	#endif
	extern void RenderCoverDebug( void );
#endif

INT16	gubAIPathCosts[19][19];

// FindBestNearbyCover - "Net" related stuff commented out
extern UINT8 gubAICounter;
extern BOOLEAN gfTurnBasedAI;

INT32 CalcPercentBetter(INT32 iOldValue, INT32 iNewValue, INT32 iOldScale, INT32 iNewScale)
{
 INT32 iValueChange,iScaleSum,iPercentBetter;//,loopCnt,tempInt;

 // calcalate how much better the new cover would be than the current cover
 iValueChange = iNewValue - iOldValue;


 // here, the change in cover HAS to be an improvement over current cover
 if (iValueChange <= 0)
	{
#ifdef BETAVERSION
	sprintf(tempstr,"CalcPercentBetter: ERROR - invalid valueChange = %d",valueChange);

#ifdef RECORDNET
	fprintf(NetDebugFile,"\n\t%s\n\n",tempstr);
#endif

	PopMessage(tempstr);
#endif

	return(NOWHERE);
	}


 iScaleSum = iOldScale + iNewScale;

 // here, the change in cover HAS to be an improvement over current cover
 if (iScaleSum <= 0)
	{
#ifdef BETAVERSION
	sprintf(tempstr,"CalcPercentBetter: ERROR - invalid scaleSum = %d",iScaleSum);

#ifdef RECORDNET
	fprintf(NetDebugFile,"\n\t%s\n\n",tempstr);
#endif

	PopMessage(tempstr);
#endif

	return(NOWHERE);
	}



 iPercentBetter = (iValueChange * 100) / iScaleSum;

#ifdef DEBUGCOVER
 DebugAI( String( "CalcPercentBetter: %%Better %ld, old %ld, new %ld, change %ld\n\t\toldScale %ld, newScale %ld, scaleSum %ld\n",
	iPercentBetter,iOldValue,iNewValue,iValueChange,iOldScale,iNewScale,iScaleSum ) );
#endif


 return(iPercentBetter);
}

void AICenterXY( INT32 sGridNo, FLOAT * pdX, FLOAT * pdY )
{
	INT16		sXPos, sYPos;

	sXPos = sGridNo % WORLD_COLS;
	sYPos = sGridNo / WORLD_COLS;

	*pdX = (FLOAT) (sXPos * CELL_X_SIZE + CELL_X_SIZE / 2);
	*pdY = (FLOAT) (sYPos * CELL_Y_SIZE + CELL_Y_SIZE / 2);
}

INT8 CalcWorstCTGTForPosition( TacticalActor * pSoldier, SoldierID ubOppID, INT32 sOppGridNo, INT8 bLevel, INT32 iMyAPsLeft )
{
	// When considering a gridno for cover, we want to take into account cover if we
	// lie down, so we return the LOWEST chance to get through for that location.
	INT8		bCubeLevel, bThisCTGT,bWorstCTGT = 100;

	for (bCubeLevel = 1; bCubeLevel <= 3; bCubeLevel++)
	{
		switch (bCubeLevel)
		{
			case 1:
				if (iMyAPsLeft < GetAPsCrouch(pSoldier,TRUE) + GetAPsProne(pSoldier,TRUE))
				{
					continue;
				}
				break;
			case 2:
				if (iMyAPsLeft < GetAPsCrouch(pSoldier,TRUE))
				{
					continue;
				}
				break;
			default:
				break;
		}

		bThisCTGT = SoldierToLocationChanceToGetThrough( pSoldier, sOppGridNo, bLevel, bCubeLevel, ubOppID );
		if (bThisCTGT < bWorstCTGT)
		{
			bWorstCTGT = bThisCTGT;
			// if there is perfect cover
			if (bWorstCTGT == 0)
				// then bail from the for loop, it can't possible get any better
				break;
		}
	}
	return( bWorstCTGT );
}

INT8 CalcAverageCTGTForPosition( TacticalActor * pSoldier, SoldierID ubOppID, INT32 sOppGridNo, INT8 bLevel, INT32 iMyAPsLeft )
{
	// When considering a gridno for cover, we want to take into account cover if we
	// lie down, so we return the LOWEST chance to get through for that location.
	INT8		bCubeLevel;
	INT32		iTotalCTGT = 0, bValidCubeLevels = 0;

	for (bCubeLevel = 1; bCubeLevel <= 3; bCubeLevel++)
	{
			switch (bCubeLevel)
		{
			case 1:
				if (iMyAPsLeft < GetAPsCrouch(pSoldier,TRUE) + GetAPsProne(pSoldier,TRUE))
				{
					continue;
				}
				break;
			case 2:
				if (iMyAPsLeft < GetAPsCrouch(pSoldier,TRUE))
				{
					continue;
				}
				break;
			default:
				break;
		}
		iTotalCTGT += SoldierToLocationChanceToGetThrough( pSoldier, sOppGridNo, bLevel, bCubeLevel, ubOppID );
		bValidCubeLevels++;
	}
	iTotalCTGT /= bValidCubeLevels;
	return( (INT8) iTotalCTGT );
}


INT8 CalcBestCTGT( TacticalActor *pSoldier, SoldierID ubOppID, INT32 sOppGridNo, INT8 bLevel, INT32 iMyAPsLeft )
{
	// NOTE: CTGT stands for "ChanceToGetThrough..."

	// using only ints for maximum execution speed here
	// CJC: Well, so much for THAT idea!
	INT32 sCentralGridNo, sAdjSpot, sNorthGridNo, sSouthGridNo, sCheckSpot;
	BOOLEAN sOKTest;

	INT8 bThisCTGT, bBestCTGT = 0;

	sCheckSpot = -1;

	sCentralGridNo = pSoldier->position().gridNo();

	// precalculate these for speed
	// what was struct for?
	sOKTest = NewOKDestination( pSoldier, sCentralGridNo, IGNOREPEOPLE , bLevel );
	sNorthGridNo = NewGridNo( sCentralGridNo, DirectionInc(NORTH) );
	sSouthGridNo = NewGridNo( sCentralGridNo, DirectionInc(SOUTH) );

	// look into all 8 adjacent tiles & determine where the cover is the worst
	// Lalien: shouldn't this start at 0 than?
	for (UINT8 sDir = 0; sDir < NUM_WORLD_DIRECTIONS; ++sDir)
	{
		// get the gridno of the adjacent spot lying in that direction
		sAdjSpot = NewGridNo( sCentralGridNo, DirectionInc( sDir ) );

		// if it wasn't out of bounds
		if (sAdjSpot != sCentralGridNo)
		{
			// if the adjacent spot can we walked on and isn't in water or gas
			if ( NewOKDestination( pSoldier, sAdjSpot, IGNOREPEOPLE, bLevel ) && !InWaterOrGas( pSoldier, sAdjSpot ))
			{
				switch (sDir)
				{
					case NORTH:
					case EAST:
					case SOUTH:
					case WEST:
						sCheckSpot = sAdjSpot;
						break;
					case NORTHEAST:
					case NORTHWEST:
						// spot to the NORTH is guaranteed to be in bounds since NE/NW was
						sCheckSpot = sNorthGridNo;
						break;
					case SOUTHEAST:
					case SOUTHWEST:
						// spot to the SOUTH is guaranteed to be in bounds since SE/SW was
						sCheckSpot = sSouthGridNo;
						break;
				}

				// ATE: OLD STUFF
				// if the adjacent gridno is reachable from the starting spot
				if ( NewOKDestination( pSoldier, sCheckSpot, FALSE, bLevel ) )
				{
					// the dude could move to this adjacent gridno, so put him there
					// "virtually" so we can calculate what our cover is from there

					// NOTE: GOTTA SET THESE 3 FIELDS *BACK* AFTER USING THIS FUNCTION!!!
					pSoldier->position().gridNo() = sAdjSpot;	 // pretend he's standing at 'sAdjSpot'
					AICenterXY( sAdjSpot, &(pSoldier->position().worldX()), &(pSoldier->position().worldY()) );
					bThisCTGT = CalcWorstCTGTForPosition( pSoldier, ubOppID, sOppGridNo, bLevel, iMyAPsLeft );
					if (bThisCTGT > bBestCTGT)
					{
						bBestCTGT = bThisCTGT;
						// if there is no cover
						if (bBestCTGT == 100)
							// then bail from the for loop, it can't possible get any better
							break;
					}
				}
			}
		}
	}

	return( bBestCTGT );
}


INT32 CalcCoverValue(TacticalActor *pMe, INT32 sMyGridNo, INT32 iMyThreat, INT32 iMyAPsLeft,
					UINT32 uiThreatIndex, INT32 iRange, INT32 morale, INT32 *iTotalScale, INT32 iRangeChangeDesire)
{
	DebugMsg(TOPIC_JA2AI,DBG_LEVEL_3,String("CalcCoverValue"));

	// all 32-bit integers for max. speed
	INT32	iMyPosValue, iHisPosValue, iCoverValue;
	INT32	iReductionFactor, iThisScale;
	INT32	sHisGridNo, sMyRealGridNo = NOWHERE, sHisRealGridNo = NOWHERE;
	INT16 sTempX, sTempY;
	FLOAT dMyX, dMyY, dHisX, dHisY;
	INT8	bHisBestCTGT, bHisActualCTGT, bHisCTGT, bMyCTGT;
	INT32	iRangeChange, iRangeFactor, iRCD;
	TacticalActor *pHim;

	// sevenfm
	INT8	bHisLevel;
	INT8	bMyLevel;
	INT32	sDist;

	dMyX = dMyY = dHisX = dHisY = -1.0;

	pHim = Threat[uiThreatIndex].pOpponent;
	sHisGridNo = Threat[uiThreatIndex].sGridNo;

	// sevenfm
	bHisLevel = pHim->position().level();
	bMyLevel = pMe->position().level();
	UINT8 ubFriendlyFireChance = 0;

	// THE FOLLOWING STUFF IS *VEERRRY SCAARRRY*, BUT SHOULD WORK.	IF YOU REALLY
	// HATE IT, THEN CHANGE ChanceToGetThrough() TO WORK FROM A GRIDNO TO GRIDNO

	// if this is theoretical, and I'm not actually at sMyGridNo right now
	if (pMe->position().gridNo() != sMyGridNo)
	{
		sMyRealGridNo = pMe->position().gridNo();		// remember where I REALLY am
		dMyX = pMe->position().worldX();
		dMyY = pMe->position().worldY();

		pMe->position().gridNo() = sMyGridNo;				// but pretend I'm standing at sMyGridNo
		ConvertGridNoToCenterCellXY( sMyGridNo, &sTempX, &sTempY );
		pMe->position().worldX() = (FLOAT) sTempX;
		pMe->position().worldY() = (FLOAT) sTempY;
	}

	// if this is theoretical, and he's not actually at hisGrid right now
	if (pHim->position().gridNo() != sHisGridNo)
	{
		sHisRealGridNo = pHim->position().gridNo();		// remember where he REALLY is
		dHisX = pHim->position().worldX();
		dHisY = pHim->position().worldY();

		pHim->position().gridNo() = sHisGridNo;			// but pretend he's standing at sHisGridNo
		ConvertGridNoToCenterCellXY( sHisGridNo, &sTempX, &sTempY );
		pHim->position().worldX() = (FLOAT) sTempX;
		pHim->position().worldY() = (FLOAT) sTempY;
	}


	if (DeepWater(sHisGridNo, bHisLevel) || InGas(pHim, sHisGridNo))
	//if (InWaterOrGas(pHim,sHisGridNo))
	{
		bHisActualCTGT = 0;
	}
	else
	{
		// optimistically assume we'll be behind the best cover available at this spot

		//bHisActualCTGT = ChanceToGetThrough(pHim,sMyGridNo,FAKE,ACTUAL,TESTWALLS,9999,M9PISTOL,NOT_FOR_LOS); // assume a gunshot
		bHisActualCTGT = CalcWorstCTGTForPosition( pHim, pMe->identity().id(), sMyGridNo, pMe->position().level(), iMyAPsLeft );
	}

	// normally, that will be the cover I'll use, unless worst case over-rides it
	bHisCTGT = bHisActualCTGT;

	// only calculate his best case CTGT if there is room for improvement!
	if (bHisActualCTGT < 100)
	{
		// if we didn't remember his real gridno earlier up above, we got to now,
		// because calculating worst case is about to play with it in a big way!		
		if (TileIsOutOfBounds(sHisRealGridNo))
		{
			sHisRealGridNo = pHim->position().gridNo();		// remember where he REALLY is
			dHisX = pHim->position().worldX();
			dHisY = pHim->position().worldY();
		}

		// calculate where my cover is worst if opponent moves just 1 tile over
		bHisBestCTGT = CalcBestCTGT(pHim, pMe->identity().id(), sMyGridNo, pMe->position().level(), iMyAPsLeft);

		// if he can actually improve his CTGT by moving to a nearby gridno
		if (bHisBestCTGT > bHisActualCTGT)
		{
			// he may not take advantage of his best case, so take only 2/3 of best
			bHisCTGT = ((2 * bHisBestCTGT) + bHisActualCTGT) / 3;
		}
	}

	// if my intended gridno is in water or gas, I can't attack at all from there
	// here, for smoke, consider bad
	if (DeepWater(sMyGridNo, bMyLevel) || InGas(pMe, sMyGridNo))
	//if (InWaterGasOrSmoke(pMe,sMyGridNo))
	{
		bMyCTGT = 0;
	}
	else
	{
		// put him at sHisGridNo if necessary!
		if (pHim->position().gridNo() != sHisGridNo )
		{
			pHim->position().gridNo() = sHisGridNo;
			ConvertGridNoToCenterCellXY( sHisGridNo, &sTempX, &sTempY );
			pHim->position().worldX() = (FLOAT) sTempX;
			pHim->position().worldY() = (FLOAT) sTempY;
		}

		// sevenfm: also check friendly fire chance for each position
		gUnderFire.Clear();
		gUnderFire.Enable();
		// let's not assume anything about the stance the enemy might take, so take an average
		// value... no cover give a higher value than partial cover
		bMyCTGT = CalcAverageCTGTForPosition( pMe, pHim->identity().id(), sHisGridNo, pHim->position().level(), iMyAPsLeft );
		gUnderFire.Disable();
		ubFriendlyFireChance = gUnderFire.Chance(pMe->roster().team(), pMe->roster().side(), TRUE);
		
		if (gGameExternalOptions.fAIBetterCover)
		{
			// sevenfm: penalize position if friendly fire chance is high
			if (ubFriendlyFireChance > MIN_CHANCE_TO_ACCIDENTALLY_HIT_SOMEONE)
				bMyCTGT = 1;
		}
		else
		{
			// since NPCs are too dumb to shoot "blind", ie. at opponents that they
			// themselves can't see (mercs can, using another as a spotter!), if the
			// cover is below the "see_thru" threshold, it's equivalent to perfect cover!
			if (bMyCTGT < SEE_THRU_COVER_THRESHOLD)
				bMyCTGT = 0;
		}
	}

	// UNDO ANY TEMPORARY "DAMAGE" DONE ABOVE	
	if (!TileIsOutOfBounds(sMyRealGridNo))
	{
		pMe->position().gridNo() = sMyRealGridNo;		// put me back where I belong!
		pMe->position().worldX() = dMyX;						// also change the 'x'
		pMe->position().worldY() = dMyY;						// and the 'y'
	}
	
	if (!TileIsOutOfBounds(sHisRealGridNo))
	{
		pHim->position().gridNo() = sHisRealGridNo;		// put HIM back where HE belongs!
		pHim->position().worldX() = dHisX;					// also change the 'x'
		pHim->position().worldY() = dHisY;					// and the 'y'
	}

	sDist = PythSpacesAway(sMyGridNo, sHisGridNo);

	if (gGameExternalOptions.fAIBetterCover)
	{
		// sevenfm: special calculations for zombies: zombie is very dangerous at close range
		if (TacticalActorConditions::isZombie(*pHim) || !AICheckHasGun(pHim))
		{
			if (sDist < (INT32)(TACTICAL_RANGE / 2))
			{
				// 100 - 25
				bHisCTGT = 100 - 150 * sDist / TACTICAL_RANGE;
			}
			else
			{
				// 25 - 0
				bHisCTGT = 100 * TACTICAL_RANGE * TACTICAL_RANGE / (16 * sDist * sDist);
			}
		}

		// sevenfm: if soldier has no gun, use different calculation
		if (TacticalActorConditions::isZombie(*pMe) || !AICheckHasGun(pMe))
		{
			if (sDist < (INT32)(TACTICAL_RANGE / 2))
			{
				bMyCTGT = 100 - 150 * sDist / TACTICAL_RANGE;
			}
			else
			{
				bMyCTGT = 100 * TACTICAL_RANGE * TACTICAL_RANGE / (16 * sDist * sDist);
			}
		}
	}	

	// these value should be < 1 million each
	iHisPosValue = bHisCTGT * Threat[uiThreatIndex].iValue * Threat[uiThreatIndex].iAPs;
	iMyPosValue =	bMyCTGT *	iMyThreat * iMyAPsLeft;

	// add penalty to enemy position, bonus to my position if soldier has cover at spot
	// max 25% at TACTICAL_RANGE / 2, 0 at zero range
	if( gGameExternalOptions.fAIBetterCover )
	{
		UINT8 ubCoverBonus = 25;
		if (sDist < (INT32)(TACTICAL_RANGE / 2))
		{
			ubCoverBonus = ubCoverBonus * 2 * sDist / TACTICAL_RANGE;
		}
		if (!TacticalActorConditions::isZombie(*pHim) && AICheckHasGun(pHim) && AnyCoverFromSpot(sMyGridNo, bMyLevel, sHisGridNo, bHisLevel))
		{
			iHisPosValue -= iHisPosValue * ubCoverBonus / 100;
			iMyPosValue += iMyPosValue * ubCoverBonus / 100;
		}
		if (!TacticalActorConditions::isZombie(*pMe) && AICheckHasGun(pMe) && AnyCoverFromSpot(sHisGridNo, bHisLevel, sMyGridNo, bMyLevel))
		{
			iHisPosValue += iHisPosValue * ubCoverBonus / 100;
			iMyPosValue -= iMyPosValue * ubCoverBonus / 100;
		}
	}

	// try to account for who outnumbers who: the side with the advantage thus
	// (hopefully) values offense more, while those in trouble will play defense
	if (pHim->awareness().opponentCount() > 1)
	{
		iHisPosValue /= pHim->awareness().opponentCount();
	}

	if (pMe->awareness().opponentCount() > 1)
	{
		iMyPosValue /= pMe->awareness().opponentCount();
	}

	// if my positional value is worth something at all here
	if (iMyPosValue > 0)
	{
		// if I CAN'T crouch when I get there, that makes it significantly less
		// appealing a spot (how much depends on range), so that's a penalty to me
		if (iMyAPsLeft < GetAPsCrouch(pMe, TRUE))
			// subtract another 1 % penalty for NOT being able to crouch per tile
			// the farther away we are, the bigger a difference crouching will make!
			iMyPosValue -= ((iMyPosValue * (AIM_PENALTY_TARGET_CROUCHED + (iRange / CELL_X_SIZE))) / 100);
	}


	// high morale prefers decreasing the range (positive factor), while very
	// low morale (HOPELESS) prefers increasing it

	// opt: RangeChangeDesire(pMe) is invariant across the whole cover search and is
	// hoisted in FindBestNearbyCover; reuse it via the param instead of re-scanning.
	iRCD = iRangeChangeDesire;

	if (iRCD)
	{
		iRangeChange = Threat[uiThreatIndex].iOrigRange - iRange;

		if (iRangeChange)
		{
			//iRangeFactor = (iRangeChange * (morale - 1)) / 4;
			iRangeFactor = (iRangeChange * iRCD) / 2;

			// sevenfm: reduce range bonus depending on cover
			if (gGameExternalOptions.fAIBetterCover &&
				pMe->morale().aiMorale() < MORALE_FEARLESS &&
				AIGunRange(pMe) >= PythSpacesAway(sMyGridNo, sHisGridNo) &&
				!AnyCoverFromSpot(sMyGridNo, bMyLevel, sHisGridNo, bHisLevel))
			{
				iRangeFactor = iRangeFactor * (100 - bHisCTGT * Threat[uiThreatIndex].iAPs / (2 * APBPConstants[AP_MAXIMUM])) / 100;
			}

#ifdef DEBUGCOVER
			DebugAI( String( "CalcCoverValue: iRangeChange %d, iRangeFactor %d\n", iRangeChange, iRangeFactor ) );
#endif

			// aggression booster for stupider enemies
			iMyPosValue += 100 * iRangeFactor * (5 - SoldierDifficultyLevel(pMe)) / 5;

			// if factor is positive increase positional value, else decrease it
			// change both values, since one or the other could be 0
			if (iRangeFactor > 0)
			{

				iMyPosValue = (iMyPosValue * (100 + iRangeFactor)) / 100;
				iHisPosValue = (100 * iHisPosValue) / (100 + iRangeFactor);
			}
			else if (iRangeFactor < 0)
			{
				iMyPosValue = (100 * iMyPosValue) / (100 - iRangeFactor);
				iHisPosValue = (iHisPosValue * (100 - iRangeFactor)) / 100;
			}
		}
	}

	// the farther apart we are, the less important the cover differences are
	// the less certain his position, the less important cover differences are
	iReductionFactor = ((MAX_THREAT_RANGE - iRange) * Threat[uiThreatIndex].iCertainty) /
		 MAX_THREAT_RANGE;

	// divide by a 100 to make the numbers more manageable and avoid 32-bit limit
	iThisScale = max( iMyPosValue, iHisPosValue) / 100;
	iThisScale = (iThisScale * iReductionFactor) / 100;
	*iTotalScale += iThisScale;
	// this helps to decide the percent improvement later

	// POSITIVE COVER VALUE INDICATES THE COVER BENEFITS ME, NEGATIVE RESULT
	// MEANS IT BENEFITS THE OTHER GUY.
	// divide by a 100 to make the numbers more manageable and avoid 32-bit limit
	iCoverValue = (iMyPosValue - iHisPosValue) / 100;
	iCoverValue = (iCoverValue * iReductionFactor) / 100;

#ifdef DEBUGCOVER
	DebugAI( String( "CalcCoverValue: iCoverValue %d, sMyGridNo %d, sHisGrid %d, iRange %d, morale %d\n",iCoverValue,sMyGridNo,sHisGridNo,iRange,morale) );
	DebugAI( String( "CalcCoverValue: iCertainty %d, his bOppCnt %d, my bOppCnt %d\n",Threat[uiThreatIndex].iCertainty,pHim->awareness().opponentCount(),pMe->awareness().opponentCount()) );
	DebugAI( String( "CalcCoverValue: bHisCTGT = %d, hisThreat = %d, hisFullAPs = %d\n",bHisCTGT,Threat[uiThreatIndex].iValue,Threat[uiThreatIndex].iAPs) );
	DebugAI( String( "CalcCoverValue: bMyCTGT = %d,	iMyThreat = %d,	iMyAPsLeft = %d\n", bMyCTGT, iMyThreat,iMyAPsLeft) );
	DebugAI( String( "CalcCoverValue: hisPosValue = %d, myPosValue = %d\n",iHisPosValue,iMyPosValue) );
	DebugAI( String( "CalcCoverValue: iThisScale = %d, iTotalScale = %d, iReductionFactor %d\n\n",iThisScale,*iTotalScale, iReductionFactor) );
#endif

	return( iCoverValue );
}


UINT8 NumberOfTeamMatesAdjacent( TacticalActor * pSoldier, INT32 sGridNo )
{
	UINT8	ubLoop, ubCount;
	SoldierID ubWhoIsThere;
	INT32	sTempGridNo;

	ubCount = 0;

	for( ubLoop = 0; ubLoop < NUM_WORLD_DIRECTIONS; ubLoop++ )
	{
		sTempGridNo = NewGridNo( sGridNo, DirectionInc( ubLoop ) );
		if ( sTempGridNo != sGridNo )
		{
			ubWhoIsThere = WhoIsThere2( sTempGridNo, pSoldier->position().level() );
			TacticalActor* adjacentSoldier =
				GetJa2SoldierRepository().resolve(ubWhoIsThere.i);
			if ( ubWhoIsThere != NOBODY &&
				ubWhoIsThere != pSoldier->identity().id() &&
				adjacentSoldier &&
				adjacentSoldier->roster().team() == pSoldier->roster().team() )
			{
				ubCount++;
			}
		}
	}

	return( ubCount );
}

INT32 FindBestNearbyCover(TacticalActor *pSoldier, INT32 morale, INT32 *piPercentBetter)
{
	DebugMsg(TOPIC_JA2AI,DBG_LEVEL_3,String("FindBestNearbyCover"));

	// all 32-bit integers for max. speed
	UINT32 uiLoop;
	INT32 iCurrentCoverValue, iCoverValue, iBestCoverValue;
	INT32	iCurrentScale = -1, iCoverScale = -1, iBestCoverScale = -1;
	INT32	iDistFromOrigin, iDistCoverFromOrigin;
	//INT32 iThreatCertainty;
	INT32 sGridNo, sBestCover = NOWHERE;
	INT32 iPathCost;
	INT32	iThreatRange, iClosestThreatRange = 1500;
//	INT16 sClosestThreatGridno = NOWHERE;
	INT32	iMyThreatValue;
	//INT32	sThreatLoc;
	//INT32 iMaxThreatRange;
	UINT32	uiThreatCnt = 0;
	INT32 iMaxMoveTilesLeft, iSearchRange, iRoamRange;
	INT16	sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;
	INT32	sOrigin;	// has to be a short, need a pointer
	INT32	*		pusLastLoc;
	INT8 *		pbPersOL;
	INT8 *		pbPublOL;
	//TacticalActor *pOpponent;
	UINT16 usMovementMode;

	UINT8	ubBackgroundLightLevel;
	UINT8	ubBackgroundLightPercent = 0;
	UINT8	ubLightPercentDifference;
	BOOLEAN fNight;

	// sevenfm
	UINT8 ubNearbyFriends;
	BOOLEAN fProneCover;
	UINT8 ubDiff = SoldierDifficultyLevel( pSoldier );

	// There's no cover when boxing!
	if (gTacticalStatus.bBoxingState == BOXING)
	{
		return (NOWHERE);
	}

	if ( gbWorldSectorZ > 0 )
	{
		fNight = FALSE;
	}
	else
	{
		ubBackgroundLightLevel = GetTimeOfDayAmbientLightLevel();

		if ( ubBackgroundLightLevel < NORMAL_LIGHTLEVEL_DAY + 2 )
		{
			fNight = FALSE;
		}
		else
		{
			fNight = TRUE;
			ubBackgroundLightPercent = gGameExternalOptions.ubBrightnessVisionMod[ ubBackgroundLightLevel ];
			//ubBackgroundLightPercent = gbLightSighting[ 0 ][ ubBackgroundLightLevel ];
		}
	}

	iBestCoverValue = -1;

	// RangeChangeDesire(pSoldier) is invariant across this whole cover search;
	// compute it once instead of 2-4 times (it scans the enemy teams via
	// GuySawEnemy + AICheckHasGun/ShortWeaponRange). It is also the only consumer
	// of the per-tile prone-sight-cover LOS raycasts (used only when < 4, i.e.
	// defending posture), so when it is >= 4 those raycasts are dead work --
	// fWantProneCover gates them.
	const INT32 iRangeChangeDesire = RangeChangeDesire( pSoldier );
	const BOOLEAN fWantProneCover = ( gGameExternalOptions.fAIBetterCover && iRangeChangeDesire < 4 );

#if defined( _DEBUG ) && defined( COVER_DEBUG )
	if (gfDisplayCoverValues)
	{
		memset( gsCoverValue, 0x7F, sizeof( INT16 ) * WORLD_MAX );
	}
#endif

	//NameMessage(pSoldier,"looking for some cover...");

	// BUILD A LIST OF THREATENING GRID #s FROM PERSONAL & PUBLIC opplists

	pusLastLoc = &(gsLastKnownOppLoc[pSoldier->identity().id()][0]);

	// hang a pointer into personal opplist
	pbPersOL = &(pSoldier->awareness().opponentKnowledge()[0]);
	// hang a pointer into public opplist
	pbPublOL = &(gbPublicOpplist[pSoldier->roster().team()][0]);

	// decide how far we're gonna be looking
	iSearchRange = gbDiff[DIFF_MAX_COVER_RANGE][ SoldierDifficultyLevel( pSoldier ) ];

/*
	switch (pSoldier->aiBehavior().attitude())
	{
		case DEFENSIVE:		iSearchRange += 2; break;
		case BRAVESOLO:		iSearchRange -= 4; break;
		case BRAVEAID:		iSearchRange -= 4; break;
		case CUNNINGSOLO:	iSearchRange += 4; break;
		case CUNNINGAID:	iSearchRange += 4; break;
		case AGGRESSIVE:	iSearchRange -= 2; break;
	}*/

	// maximum search range is 1 tile / 8 pts of wisdom
	if (iSearchRange > (pSoldier->statistics().wisdom() / 8))
	{
		iSearchRange = (pSoldier->statistics().wisdom() / 8);
	}

	if (!gfTurnBasedAI)
	{
		// don't search so far in realtime
		iSearchRange /= 2;
	}

	usMovementMode = DetermineMovementMode( pSoldier, AI_ACTION_TAKE_COVER );

	if (pSoldier->aiBehavior().alertStatus() >= STATUS_RED)			// if already in battle
	{
		// must be able to reach the cover, so it can't possibly be more than
		// action points left (rounded down) tiles away, since minimum
		// cost to move per tile is 1 points.
		// HEADROCK HAM 3.6: This doesn't take into account the 100AP system. Adjusting.
		// Please note, I used a calculation that may have a better representation in some global variable.
		//iMaxMoveTilesLeft = __max( 0, pSoldier->actionPoints().current() - MinAPsToStartMovement( pSoldier, usMovementMode ) );
		// WarmSteel - Bugfix:  wrong parentheses
		iMaxMoveTilesLeft = __max( 0, (pSoldier->actionPoints().current() - MinAPsToStartMovement( pSoldier, usMovementMode )) / (APBPConstants[AP_MAXIMUM] / 25) );

		//NumMessage("In BLACK, maximum tiles to move left = ",maxMoveTilesLeft);

		// if we can't go as far as the usual full search range
		if (iMaxMoveTilesLeft < iSearchRange)
		{
			// then limit the search range to only as far as we CAN go
			iSearchRange = iMaxMoveTilesLeft;
		}
	}

	if (iSearchRange <= 0)
	{
		return(NOWHERE);
	}

	// calculate OUR OWN general threat value (not from any specific location)
	iMyThreatValue = CalcManThreatValue(pSoldier, NOWHERE, FALSE, pSoldier);

	// prepare threat list from known enemies
	uiThreatCnt = PrepareThreatlist(pSoldier);

	// if no known opponents were found to threaten us, can't worry about cover
	if (!uiThreatCnt)
	{
		//NameMessage(pSoldier,"has no threats - WON'T take cover");
		return(sBestCover);
	}

	// calculate our current cover value in the place we are now, since the
	// cover we are searching for must be better than what we have now!
	iCurrentCoverValue = 0;
	iCurrentScale = 0;

	// sevenfm: sight cover
	fProneCover = TRUE;

	// for every opponent that threatens, consider this spot's cover vs. him
	for (uiLoop = 0; uiLoop < uiThreatCnt; uiLoop++)
	{
		// if this threat is CURRENTLY within 20 tiles
		if (Threat[uiLoop].iOrigRange <= MAX_THREAT_RANGE)
		{
			// add this opponent's cover value to our current total cover value
			iCurrentCoverValue += CalcCoverValue(pSoldier,pSoldier->position().gridNo(),iMyThreatValue,pSoldier->actionPoints().current(),uiLoop,Threat[uiLoop].iOrigRange,morale,&iCurrentScale,iRangeChangeDesire);
		}
		// sevenfm: sight test -- only matters when defending (fWantProneCover); and
		// once any threat sees the prone spot, fProneCover is decided, so stop testing.
		if( fWantProneCover && fProneCover )
		{
			if ( LocationToLocationLineOfSightTest( Threat[uiLoop].sGridNo, Threat[uiLoop].pOpponent->position().level(), pSoldier->position().gridNo(), pSoldier->position().level(), TRUE, MAX_VISION_RANGE, STANDING_LOS_POS, PRONE_LOS_POS ) )
			{
				fProneCover = FALSE;
			}
		}
		//sprintf(tempstr,"iCurrentCoverValue after opponent %d is now %d",iLoop,iCurrentCoverValue);
		//PopMessage(tempstr);
	}

	// reduce cover for each person adjacent to this gridno who is on our team,
	// by 10% (so locations next to several people will be very much frowned upon
	if ( iCurrentCoverValue >= 0 )
	{
		iCurrentCoverValue -= (iCurrentCoverValue / 10) * NumberOfTeamMatesAdjacent( pSoldier, pSoldier->position().gridNo() );
	}
	else
	{
		// when negative, must add a negative to decrease the total
		iCurrentCoverValue += (iCurrentCoverValue / 10) * NumberOfTeamMatesAdjacent( pSoldier, pSoldier->position().gridNo() );
	}

	if( gGameExternalOptions.fAIBetterCover )
	{
		// sevenfm: when defending (range change <= 3), prefer locations with sight cover
		if( iRangeChangeDesire < 4 )
		{
			if( fProneCover )
			{
				iCurrentCoverValue += abs(iCurrentCoverValue) / __max(2, 2*iRangeChangeDesire);
			}
		}

		// sevenfm: check for nearby friends, add bonus/penalty
		ubNearbyFriends = __min(5, CountNearbyFriends( pSoldier, pSoldier->position().gridNo(), 5 ));
		iCurrentCoverValue -= ubNearbyFriends * abs(iCurrentCoverValue) / (10-ubDiff);

		// sevenfm: penalize locations with fresh corpses
		if(GetNearestRottingCorpseAIWarning( pSoldier->position().gridNo() ) > 0)
		{
			iCurrentCoverValue -= abs(iCurrentCoverValue) / (8-ubDiff);
		}

		// sevenfm: penalize locations near red smoke
		iCurrentCoverValue -= abs(iCurrentCoverValue) * RedSmokeDanger(pSoldier->position().gridNo(), pSoldier->position().level()) / 100;
	}	

#ifdef DEBUGCOVER
//	AINumMessage("Search Range = ",iSearchRange);
#endif

	// determine maximum horizontal limits
	sMaxLeft  = min(iSearchRange,(pSoldier->position().gridNo() % MAXCOL));
	//NumMessage("sMaxLeft = ",sMaxLeft);
	sMaxRight = min(iSearchRange,MAXCOL - ((pSoldier->position().gridNo() % MAXCOL) + 1));
	//NumMessage("sMaxRight = ",sMaxRight);

	// determine maximum vertical limits
	sMaxUp   = min(iSearchRange,(pSoldier->position().gridNo() / MAXROW));
	//NumMessage("sMaxUp = ",sMaxUp);
	sMaxDown = min(iSearchRange,MAXROW - ((pSoldier->position().gridNo() / MAXROW) + 1));
	//NumMessage("sMaxDown = ",sMaxDown);

	iRoamRange = RoamingRange(pSoldier,&sOrigin);

	// if status isn't black (life & death combat), and roaming range is limited	
	if ((pSoldier->aiBehavior().alertStatus() != STATUS_BLACK) && (iRoamRange < MAX_ROAMING_RANGE) &&
		(!TileIsOutOfBounds(sOrigin)))
	{
		// must try to stay within or return to the point of origin
		iDistFromOrigin = SpacesAway(sOrigin,pSoldier->position().gridNo());
	}
	else
	{
		// don't care how far from origin we go
		iDistFromOrigin = -1;
	}


#ifdef DEBUGCOVER
	DebugAI( String( "FBNC: iRoamRange %d, sMaxLeft %d, sMaxRight %d, sMaxUp %d, sMaxDown %d\n",iRoamRange,sMaxLeft,sMaxRight,sMaxUp,sMaxDown) );
#endif

	// the initial cover value to beat is our current cover value
	iBestCoverValue = iCurrentCoverValue;

#ifdef DEBUGDECISIONS
	STR tempstr="";
	sprintf( tempstr, "FBNC: CURRENT iCoverValue = %d\n",iCurrentCoverValue );
	DebugAI( tempstr );
#endif

	if (pSoldier->aiBehavior().alertStatus() >= STATUS_RED)			// if already in battle
	{
		// to speed this up, tell PathAI to cancel any paths beyond our AP reach!
		gubNPCAPBudget = pSoldier->actionPoints().current();
	}
	else
	{
		// even if not under pressure, limit to 1 turn's travelling distance
		// hope this isn't too expensive...
		gubNPCAPBudget = TacticalActorTurnBudget::calculateTurnGrant(*pSoldier);
		//gubNPCAPBudget = pSoldier->bInitialAPs;
	}

	// Call FindBestPath to set flags in all locations that we can
	// walk into within range.	We have to set some things up first...

	// set the distance limit of the square region
	gubNPCDistLimit = (UINT8) iSearchRange;
	gusNPCMovementMode = usMovementMode;

	// reset the "reachable" flags in the region we're looking at
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			if ( !(sGridNo >=0 && sGridNo < WORLD_MAX) )
			{
				continue;
			}
			gpWorldLevelData[sGridNo].uiFlags &= ~(MAPELEMENT_REACHABLE);
		}
	}

	FindBestPath( pSoldier, GRIDSIZE, pSoldier->position().level(), DetermineMovementMode( pSoldier, AI_ACTION_TAKE_COVER ), COPYREACHABLE_AND_APS, 0 );//dnl ch50 071009

	// Turn off the "reachable" flag for his current location
	// so we don't consider it
	gpWorldLevelData[pSoldier->position().gridNo()].uiFlags &= ~(MAPELEMENT_REACHABLE);

	// SET UP DOUBLE-LOOP TO STEP THROUGH POTENTIAL GRID #s
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			//HandleMyMouseCursor(KEYBOARDALSO);

			// calculate the next potential gridno
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			if ( !(sGridNo >=0 && sGridNo < WORLD_MAX) )
			{
				continue;
			}

			//NumMessage("Testing gridno #",sGridNo);

			// if we are limited to staying/returning near our place of origin
			if (iDistFromOrigin != -1)
			{
				iDistCoverFromOrigin = SpacesAway(sOrigin,sGridNo);

				// if this is outside roaming range, and doesn't get us closer to it
				if ((iDistCoverFromOrigin > iRoamRange) &&
					(iDistFromOrigin <= iDistCoverFromOrigin))
				{
					continue;	// then we can't go there
				}
		}

/*
			if (Net.pnum != Net.turnActive)
			{
				KeepInterfaceGoing(1);
			}
*/
			if (!(gpWorldLevelData[sGridNo].uiFlags & MAPELEMENT_REACHABLE))
			{
				continue;
			}

			if ( InGas( pSoldier, sGridNo ) )
			{
				continue;
			}

			// ignore blacklisted spot
			if ( sGridNo == pSoldier->pathing().blackListGrid() )
			{
				continue;
			}

			// sevenfm: avoid tiles near bombs
			if (FindBombNearby(pSoldier, sGridNo, BOMB_DETECTION_RANGE))
			{
				continue;
			}

			// sevenfm: avoid staying at north edge
			if (NorthSpot(sGridNo, pSoldier->position().level()))
			{
				continue;
			}

			// sevenfm: avoid moving into light
			if (InLightAtNight(sGridNo, pSoldier->position().level()) &&
				!InLightAtNight(pSoldier->position().gridNo(), pSoldier->position().level()) &&
				!pSoldier->suppression().underFire())
			{
				continue;
			}

			// avoid moving into red smoke
			if (gGameExternalOptions.fAIBetterCover &&
				RedSmokeDanger(sGridNo, pSoldier->position().level()) &&
				!RedSmokeDanger(pSoldier->position().gridNo(), pSoldier->position().level()))
			{
				//DebugCover(pSoldier, String("moving into red smoke, skip"));
				continue;
			}

			// zombies only want to hide
			if (TacticalActorConditions::isZombie(*pSoldier) && !SightCoverAtSpot(pSoldier, sGridNo, TRUE))
			{
				//DebugCover(pSoldier, String("zombie: no sight cover at new spot, skip"));
				continue;
			}

			iPathCost = gubAIPathCosts[AI_PATHCOST_RADIUS + sXOffset][AI_PATHCOST_RADIUS + sYOffset];
			/*
			// water is OK, if the only good hiding place requires us to get wet, OK
			iPathCost = LegalNPCDestination(pSoldier,sGridNo,ENSURE_PATH_COST,WATEROK);

			if (!iPathCost)
			{
				continue;		// skip on to the next potential grid
			}

			// CJC: This should be a redundent check because the path code is given an
			// AP limit to begin with!
			if (pSoldier->aiBehavior().alertStatus() == STATUS_BLACK)		// in battle
			{
				// must be able to afford the APs to get to this cover this turn
				if (iPathCost > pSoldier->actionPoints().current())
				{
					//NumMessage("In BLACK, and can't afford to get there, cost = ",iPathCost);
					continue;		// skip on to the next potential grid
				}
			}
			*/

			// OK, this place shows potential.	How useful is it as cover?
			// EVALUATE EACH GRID #, remembering the BEST PROTECTED ONE
			iCoverValue = 0;
			iCoverScale = 0;

			// sevenfm: check sight cover
			fProneCover = TRUE;

			// for every opponent that threatens, consider this spot's cover vs. him
			for (uiLoop = 0; uiLoop < uiThreatCnt; uiLoop++)
			{
				// calculate the range we would be at from this opponent
				iThreatRange = GetRangeInCellCoordsFromGridNoDiff( sGridNo, Threat[uiLoop].sGridNo );
				// if this threat would be within 20 tiles, count it
				if (iThreatRange <= MAX_THREAT_RANGE)
				{
					iCoverValue += CalcCoverValue(pSoldier,sGridNo,iMyThreatValue,
						(pSoldier->actionPoints().current() - iPathCost),
						uiLoop,iThreatRange,morale,&iCoverScale,iRangeChangeDesire);
				}

				// sevenfm: sight test -- only matters when defending (fWantProneCover);
				// stop once fProneCover is decided for this tile.
				if( fWantProneCover && fProneCover )
				{
					if ( LocationToLocationLineOfSightTest( Threat[uiLoop].sGridNo, Threat[uiLoop].pOpponent->position().level(), sGridNo, pSoldier->position().level(), TRUE, MAX_VISION_RANGE, STANDING_LOS_POS, PRONE_LOS_POS ) )
					{
						fProneCover = FALSE;
					}
				}

				//sprintf(tempstr,"iCoverValue after opponent %d is now %d",iLoop,iCoverValue);
				//PopMessage(tempstr);
			}

			// reduce cover for each person adjacent to this gridno who is on our team,
			// by 10% (so locations next to several people will be very much frowned upon
			if ( iCoverValue >= 0 )
			{
				iCoverValue -= (iCoverValue / 10) * NumberOfTeamMatesAdjacent( pSoldier, sGridNo );
			}
			else
			{
				// when negative, must add a negative to decrease the total
				iCoverValue += (iCoverValue / 10) * NumberOfTeamMatesAdjacent( pSoldier, sGridNo );
			}

			if( gGameExternalOptions.fAIBetterCover )
			{
				// sevenfm: when defending (range change <= 3), prefer locations with sight cover
				if( iRangeChangeDesire < 4 )
				{
					if( fProneCover )
						iCoverValue += abs(iCoverValue) / __max(2, 2*iRangeChangeDesire);
				}

				// sevenfm: check for nearby friends in 10 radius, add bonus/penalty 10%
				ubNearbyFriends = __min(5, CountNearbyFriends( pSoldier, sGridNo, 5 ));
				iCoverValue -= ubNearbyFriends * abs(iCoverValue) / (10-ubDiff);

				// sevenfm: penalize locations with fresh corpses
				if(GetNearestRottingCorpseAIWarning( sGridNo ) > 0)
				{
					iCoverValue -= abs(iCoverValue) / (8-ubDiff);
				}

				// sevenfm: penalize locations near red smoke			
				iCoverValue -= abs(iCoverValue) * RedSmokeDanger(sGridNo, pSoldier->position().level()) / 100;
			}			

			if ( fNight && !( InARoom( sGridNo, NULL ) ) ) // ignore in buildings in case placed there
			{
				// reduce cover at nighttime based on how bright the light is at that location
				// using the difference in sighting distance between the background and the
				// light for this tile
				//ubLightPercentDifference = (gbLightSighting[ 0 ][ LightTrueLevel( sGridNo, pSoldier->position().level() ) ] - ubBackgroundLightPercent );
				ubLightPercentDifference = (gGameExternalOptions.ubBrightnessVisionMod[ LightTrueLevel( sGridNo, pSoldier->position().level() ) ] - ubBackgroundLightPercent );
				
				if ( iCoverValue >= 0 )
				{
					iCoverValue -= (iCoverValue / 100) * ubLightPercentDifference;
				}
				else
				{
					iCoverValue += (iCoverValue / 100) * ubLightPercentDifference;
				}
			}


#ifdef DEBUGCOVER
			// if there ARE multiple opponents
			if (uiThreatCnt > 1)
			{
				DebugAI( String( "FBNC: Total iCoverValue at gridno %d is %d\n\n",sGridNo,iCoverValue ) );
			}
#endif

#if defined( _DEBUG ) && defined( COVER_DEBUG )
			if (gfDisplayCoverValues)
			{
				gsCoverValue[sGridNo] = (INT16) (iCoverValue / 100);
			}
#endif

			// if this is better than the best place found so far

			if (iCoverValue > iBestCoverValue)
			{
				// ONLY DO THIS CHECK HERE IF WE'RE WAITING FOR OPPCHANCETODECIDE,
				// OTHERWISE IT WOULD USUALLY BE A WASTE OF TIME
				// ok to comment out for now?
				/*
				if (Status.team[Net.turnActive].allowOppChanceToDecide)
				{
					// if this cover value qualifies as "better" enough to get used
					if (CalcPercentBetter( iCurrentCoverValue,iCoverValue,iCurrentScale,iCoverScale) >= MIN_PERCENT_BETTER)
					{
						// then we WILL do something (take this cover, at least)
						NPCDoesAct(pSoldier);
					}
				}
				*/

#ifdef DEBUGDECISIONS
				STR tempstr;
				sprintf( tempstr,"FBNC: NEW BEST iCoverValue at gridno %d is %d\n",sGridNo,iCoverValue );
				DebugAI( tempstr );
#endif
				// remember it instead
				sBestCover = sGridNo;
				iBestCoverValue = iCoverValue;
				iBestCoverScale = iCoverScale;
			}
		}
	}

	gubNPCAPBudget = 0;
	gubNPCDistLimit = 0;

	#if defined( _DEBUG ) && !defined( PATHAI_VISIBLE_DEBUG )
	if (gfDisplayCoverValues)
	{
		// do a locate?
		LocateSoldier( pSoldier->identity().id(), SETLOCATORFAST );
		gsBestCover = sBestCover;
		SetRenderFlags( RENDER_FLAG_FULL );
		RenderWorld();
		RenderCoverDebug( );
		InvalidateScreen( );
		EndFrameBufferRender();
		RefreshScreen( NULL );
		/*
	iLoop = GetJA2Clock();
	do
	{

	} while( ( GetJA2Clock( ) - iLoop ) < 2000 );
	*/
	}
	#endif

	// if a better cover location was found	
	if (!TileIsOutOfBounds(sBestCover))
	{
		#if defined( _DEBUG ) && !defined( PATHAI_VISIBLE_DEBUG )
		gsBestCover = sBestCover;
		#endif
		// cover values already take the AP cost of getting there into account in
		// a BIG way, so no need to worry about that here, even small improvements
		// are actually very significant once we get our APs back (if we live!)
		*piPercentBetter = CalcPercentBetter(iCurrentCoverValue,iBestCoverValue,iCurrentScale,iBestCoverScale);

		// if best cover value found was at least 5% better than our current cover
		if (*piPercentBetter >= MIN_PERCENT_BETTER)
		{
#ifdef DEBUGDECISIONS
			STR tempstr;
			sprintf( tempstr,"Found Cover: current %ld, best %ld, %%%%Better %ld\n", iCurrentCoverValue,iBestCoverValue,*piPercentBetter  );
			DebugAI( tempstr );
#endif

#ifdef BETAVERSION
			SnuggleDebug(pSoldier,"Found Cover");
#endif

			return(sBestCover);       // return the gridno of that cover
		}
	}
	return(NOWHERE);		// return that no suitable cover was found
}

INT32 FindSpotMaxDistFromOpponents(TacticalActor *pSoldier)
{
	INT32	sGridNo;
	INT32	sBestSpot = NOWHERE;
	UINT32	uiLoop;
	INT32	iThreatRange, iClosestThreatRange = 1500, iSpotClosestThreatRange;
	INT32	sThreatLoc, sThreatGridNo[MAXMERCS];
	UINT32	uiThreatCnt = 0;
	INT32	iSearchRange;
	INT16	sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;
	INT8	*pbPersOL, *pbPublOL, bEscapeDirection, bBestEscapeDirection = -1;
	TacticalActor *pOpponent;
	INT32	sOrigin;
	INT32	iRoamRange;

	// BUILD A LIST OF THREATENING GRID #s FROM PERSONAL & PUBLIC opplistS

	// look through all opponents for those we know of
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, dead, unconscious
		if (!pOpponent || (pOpponent->vitals().health() < OKLIFE))
		{
			continue;			// next merc
		}

		// if this man is neutral / on the same side, he's not an opponent
		if ( CONSIDERED_NEUTRAL( pSoldier, pOpponent ) || (pSoldier->roster().side() == pOpponent->roster().side()))
		{
			continue;			// next merc
		}

		pbPersOL = &(pSoldier->awareness().opponentKnowledge()[pOpponent->identity().id()]);
		pbPublOL = &(gbPublicOpplist[pSoldier->roster().team()][pOpponent->identity().id()]);

		// if this opponent is unknown personally and publicly
		if ((*pbPersOL == NOT_HEARD_OR_SEEN) && (*pbPublOL == NOT_HEARD_OR_SEEN))
		{
			continue;			// check next opponent
		}

		// Special stuff for Carmen the bounty hunter
		if (pSoldier->aiBehavior().attitude() == ATTACKSLAYONLY && pOpponent->identity().profile() != SLAY)
		{
			continue;	// next opponent
		}

		// if the opponent is no threat at all for some reason
		if (CalcManThreatValue(pOpponent,pSoldier->position().gridNo(),FALSE,pSoldier) == -999)
		{
			continue;			// check next opponent
		}

		// if personal knowledge is more up to date or at least equal
		if ((gubKnowledgeValue[*pbPublOL - OLDEST_HEARD_VALUE][*pbPersOL - OLDEST_HEARD_VALUE] > 0) ||
		(*pbPersOL == *pbPublOL))
		{
			// using personal knowledge, obtain opponent's "best guess" gridno
			sThreatLoc = gsLastKnownOppLoc[pSoldier->identity().id()][pOpponent->identity().id()];
		}
		else
		{
			// using public knowledge, obtain opponent's "best guess" gridno
			sThreatLoc = gsPublicLastKnownOppLoc[pSoldier->roster().team()][pOpponent->identity().id()];
		}

		// calculate how far away this threat is (in adjusted pixels)
		iThreatRange = GetRangeInCellCoordsFromGridNoDiff( pSoldier->position().gridNo(), sThreatLoc );

		if (iThreatRange < iClosestThreatRange)
		{
			iClosestThreatRange = iThreatRange;
			//NumMessage("New Closest Threat Range = ",iClosestThreatRange);
		}

		// remember this threat's gridno
		sThreatGridNo[uiThreatCnt] = sThreatLoc;
		uiThreatCnt++;
	}

	// if no known opponents were found to threaten us, can't worry about them
	if (!uiThreatCnt)
	{
		//NameMessage(pSoldier,"has no known threats - WON'T run away");
		return( sBestSpot );
	}

	// get roaming range here; for civilians, running away is limited by roam range
	if ( pSoldier->roster().team() == CIV_TEAM )
	{
		iRoamRange = RoamingRange( pSoldier, &sOrigin );
		if ( iRoamRange == 0 )
		{
			return( sBestSpot );
		}
	}
	else
	{
		// dummy values
		iRoamRange = 100;
		sOrigin = pSoldier->position().gridNo();
	}

	// DETERMINE CO-ORDINATE LIMITS OF SQUARE AREA TO BE CHECKED
	// THIS IS A LOT QUICKER THAN COVER, SO DO A LARGER AREA, NOT AFFECTED BY
	// DIFFICULTY SETTINGS...

	if (pSoldier->aiBehavior().alertStatus() == STATUS_BLACK)			// if already in battle
	{
		iSearchRange = pSoldier->actionPoints().current() / 2;

		// to speed this up, tell PathAI to cancel any paths beyond our AP reach!
		gubNPCAPBudget = pSoldier->actionPoints().current();
	}
	else
	{
		// even if not under pressure, limit to 1 turn's travelling distance
		gubNPCAPBudget = __min( pSoldier->actionPoints().current() / 2, TacticalActorTurnBudget::calculateTurnGrant(*pSoldier) );

		iSearchRange = gubNPCAPBudget / 2;
	}

	if (!gfTurnBasedAI)
	{
		// search only half as far in realtime
		// but always allow a certain minimum!

		if ( iSearchRange > 4 )
		{
			iSearchRange /= 2;
			gubNPCAPBudget /= 2;
		}
	}


	// assume we have to stand up!
	// use the min macro here to make sure we don't wrap the UINT8 to 255...
	// Lesh: for some reason this code still allows to wrap UINT8 at low values of gubNPCAPBudget
	//gubNPCAPBudget = 	gubNPCAPBudget = __min( gubNPCAPBudget, gubNPCAPBudget - GetAPsToChangeStance( pSoldier, ANIM_STAND ) );
	// Lesh: will be using this form
	if ( gubNPCAPBudget > GetAPsToChangeStance( pSoldier, ANIM_STAND ) )
		gubNPCAPBudget = gubNPCAPBudget - (UINT8) GetAPsToChangeStance( pSoldier, ANIM_STAND );
	//NumMessage("Search Range = ",iSearchRange);
	//NumMessage("gubNPCAPBudget = ",gubNPCAPBudget);

	// determine maximum horizontal limits
	sMaxLeft  = min( iSearchRange, (pSoldier->position().gridNo() % MAXCOL));
	//NumMessage("sMaxLeft = ",sMaxLeft);
	sMaxRight = min( iSearchRange, MAXCOL - ((pSoldier->position().gridNo() % MAXCOL) + 1));
	//NumMessage("sMaxRight = ",sMaxRight);

	// determine maximum vertical limits
	sMaxUp   = min( iSearchRange, (pSoldier->position().gridNo() / MAXROW));
	//NumMessage("sMaxUp = ",sMaxUp);
	sMaxDown = min( iSearchRange, MAXROW - ((pSoldier->position().gridNo() / MAXROW) + 1));
	//NumMessage("sMaxDown = ",sMaxDown);

	// Call FindBestPath to set flags in all locations that we can
	// walk into within range.	We have to set some things up first...

	// set the distance limit of the square region
	gubNPCDistLimit = (UINT8) iSearchRange;

	// reset the "reachable" flags in the region we're looking at
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			if ( !(sGridNo >=0 && sGridNo < WORLD_MAX) )
			{
				continue;
			}

			gpWorldLevelData[sGridNo].uiFlags &= ~(MAPELEMENT_REACHABLE);
		}
	}

	FindBestPath( pSoldier, GRIDSIZE, pSoldier->position().level(), DetermineMovementMode( pSoldier, AI_ACTION_RUN_AWAY ), COPYREACHABLE, 0 );//dnl ch50 121009

	// Turn off the "reachable" flag for his current location
	// so we don't consider it
	gpWorldLevelData[pSoldier->position().gridNo()].uiFlags &= ~(MAPELEMENT_REACHABLE);
	//dnl ch58 170813 also don't use last two locations to avoid looping same decisions per turn
	const SoldierMovementHistoryComponent::RecentLocations& recentLocations =
		pSoldier->movementHistory().recentLocations();
	if(!TileIsOutOfBounds(recentLocations[0]))
		gpWorldLevelData[recentLocations[0]].uiFlags &= ~(MAPELEMENT_REACHABLE);
	else if(!TileIsOutOfBounds(recentLocations[1]))
		gpWorldLevelData[recentLocations[1]].uiFlags &= ~(MAPELEMENT_REACHABLE);

	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			// calculate the next potential gridno
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			//NumMessage("Testing gridno #",gridno);
			if ( !(sGridNo >=0 && sGridNo < WORLD_MAX) )
			{
				continue;
			}

			if (!(gpWorldLevelData[sGridNo].uiFlags & MAPELEMENT_REACHABLE))
			{
				continue;
			}

			if ( sGridNo == pSoldier->pathing().blackListGrid() )
			{
				continue;
			}

			if ( pSoldier->roster().team() == CIV_TEAM )
			{
				// iRoamRange/sOrigin are loop-invariant here (RoamingRange depends only on
				// pSoldier state, which doesn't change in this loop); reuse the values
				// already computed above instead of recomputing per candidate tile
				if ( PythSpacesAway( sOrigin, sGridNo ) > iRoamRange )
				{
					continue;
				}
			}

			// exclude locations with tear/mustard gas (at this point, smoke is cool!)
			if ( InGas( pSoldier, sGridNo ) )
			{
				continue;
			}

			if (!CheckNPCDestination(pSoldier, sGridNo))
			{
				continue;
			}

			// check that spot is allowed
			if (!LegalNPCDestination(pSoldier, sGridNo, IGNORE_PATH, NOWATER, 0))
			{
				continue;		// skip on to the next potential grid
			}

			// OK, this place shows potential.	How useful is it as cover?
			//NumMessage("Promising seems gridno #",gridno);

			iSpotClosestThreatRange = 1500;

			if (gGameExternalOptions.fAITacticalRetreat &&
				pSoldier->roster().team() == ENEMY_TEAM &&
				GridNoOnEdgeOfMap(sGridNo, &bEscapeDirection) &&
				EscapeDirectionIsValid(&bEscapeDirection))
			{
				// We can escape!	This is better than anything else except a closer spot which we can
				// cross over from.

				// Subtract the straight-line distance from our location to this one as an estimate of
				// path cost and for looks...

				// The edge spot closest to us which is on the edge will have the highest value, so
				// it will be picked over locations further away.
				// Only reachable gridnos will be picked so this should hopefully look okay
				iSpotClosestThreatRange -= PythSpacesAway( pSoldier->position().gridNo(), sGridNo );

			}
			else
			{
				bEscapeDirection = -1;
				// for every opponent that threatens, consider this spot's cover vs. him
				for (uiLoop = 0; uiLoop < uiThreatCnt; uiLoop++)
				{
					//iThreatRange = AdjPixelsAway(CenterX(sGridNo),CenterY(sGridNo), CenterX(sThreatGridNo[iLoop]),CenterY(sThreatGridNo[iLoop]));
					iThreatRange = GetRangeInCellCoordsFromGridNoDiff( sGridNo, sThreatGridNo[uiLoop] );
					if (iThreatRange < iSpotClosestThreatRange)
					{
						iSpotClosestThreatRange = iThreatRange;
					}
				}
			}

			// if this is better than the best place found so far
			// (i.e. the closest guy would be farther away than previously)
			if (iSpotClosestThreatRange > iClosestThreatRange)
			{
				// remember it instead
				iClosestThreatRange = iSpotClosestThreatRange;
				//NumMessage("New best range = ",iClosestThreatRange);
				sBestSpot = sGridNo;
				bBestEscapeDirection = bEscapeDirection;
				//NumMessage("New best grid = ",bestSpot);
			}
		}
	}

	gubNPCAPBudget = 0;
	gubNPCDistLimit = 0;

	if (bBestEscapeDirection != -1)
	{
		// Woohoo!	We can escape!	Fake some stuff with the quote-related actions
		pSoldier->dialogue().quoteActionId() = GetTraversalQuoteActionID( bBestEscapeDirection );
	}

	return( sBestSpot );
}

INT32 FindNearestUngassedLand(TacticalActor *pSoldier)
{
	INT32 sGridNo, sClosestLand = NOWHERE, sPathCost, sShortestPath = 1000;
	INT16 sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;
	INT32 iSearchRange, iIgnoreRange = 0;
	INT32 iMaxSearchRange = 35;
	UINT16 usMovementMode = DetermineMovementMode(pSoldier, AI_ACTION_LEAVE_WATER_GAS);
	BOOLEAN fFoundReachable = FALSE;

	if (!gfTurnBasedAI)
	{
		iMaxSearchRange = 15;
	}

	// start with a small search area, and expand it if we're unsuccessful
	// this should almost never need to search farther than 5 or 10 squares...
	for (iSearchRange = 5; iSearchRange <= iMaxSearchRange && (fFoundReachable || iSearchRange <= 5); iIgnoreRange = iSearchRange, iSearchRange += 10)
	{
		//NumMessage("Trying iSearchRange = ", iSearchRange);

		// determine maximum horizontal limits
		sMaxLeft = min(iSearchRange, (pSoldier->position().gridNo() % MAXCOL));
		//NumMessage("sMaxLeft = ",sMaxLeft);
		sMaxRight = min(iSearchRange, MAXCOL - ((pSoldier->position().gridNo() % MAXCOL) + 1));
		//NumMessage("sMaxRight = ",sMaxRight);

		// determine maximum vertical limits
		sMaxUp = min(iSearchRange, (pSoldier->position().gridNo() / MAXROW));
		//NumMessage("sMaxUp = ",sMaxUp);
		sMaxDown = min(iSearchRange, MAXROW - ((pSoldier->position().gridNo() / MAXROW) + 1));
		//NumMessage("sMaxDown = ",sMaxDown);

		// Call FindBestPath to set flags in all locations that we can
		// walk into within range.	We have to set some things up first...		

		// reset the "reachable" flags in the region we're looking at
		for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
		{
			for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
			{
				sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
				if (!(sGridNo >= 0 && sGridNo < WORLD_MAX))
				{
					continue;
				}

				gpWorldLevelData[sGridNo].uiFlags &= ~(MAPELEMENT_REACHABLE);
			}
		}

		//gubNPCAPBudget = pSoldier->actionPoints().current();
		gubNPCAPBudget = 0;
		gubNPCDistLimit = (UINT8)iSearchRange;
		FindBestPath(pSoldier, GRIDSIZE, pSoldier->position().level(), usMovementMode, COPYREACHABLE, 0);	//dnl ch50 071009
		gubNPCAPBudget = 0;
		gubNPCDistLimit = 0;

		// Turn off the "reachable" flag for his current location
		// so we don't consider it
		gpWorldLevelData[pSoldier->position().gridNo()].uiFlags &= ~(MAPELEMENT_REACHABLE);

		// SET UP DOUBLE-LOOP TO STEP THROUGH POTENTIAL GRID #s
		for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
		{
			for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
			{
				// sevenfm: optimization - skip spots checked in previous loop
				if (sXOffset <= iIgnoreRange && sXOffset >= -iIgnoreRange && sYOffset <= iIgnoreRange && sYOffset >= -iIgnoreRange)
				{
					continue;
				}

				// calculate the next potential gridno
				sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
				//NumMessage("Testing gridno #",gridno);
				if (!(sGridNo >= 0 && sGridNo < WORLD_MAX))
				{
					continue;
				}

				if (!(gpWorldLevelData[sGridNo].uiFlags & MAPELEMENT_REACHABLE))
				{
					continue;
				}

				fFoundReachable = TRUE;

				// ignore blacklisted spot
				if (sGridNo == pSoldier->pathing().blackListGrid())
				{
					continue;
				}

				if (!CheckNPCDestination(pSoldier, sGridNo))
				{
					continue;
				}

				// CJC: here, unfortunately, we must calculate a path so we have an AP cost

				// obviously, we're looking for LAND, so water is out!
				//sPathCost = LegalNPCDestination(pSoldier,sGridNo,ENSURE_PATH_COST,NOWATER,0);

				if (!LegalNPCDestination(pSoldier, sGridNo, IGNORE_PATH, NOWATER, 0))
				{
					continue;		// skip on to the next potential grid
				}

				sPathCost = PlotPath(pSoldier, sGridNo, FALSE, FALSE, FALSE, usMovementMode, pSoldier->movement().stealthMode(), pSoldier->movement().reverse(), 0);

				// check if spot is reachable
				if(sPathCost == 0)
				{
					continue;
				}

				// if this path is shorter than the one to the closest land found so far
				if (sPathCost < sShortestPath)
				{
					// remember it instead
					sShortestPath = sPathCost;
					//NumMessage("New shortest route = ",shortestPath);

					sClosestLand = sGridNo;
					//NumMessage("New closest land at grid = ",closestLand);
				}
			}
		}

		// if we found a piece of land in this search area		
		if (!TileIsOutOfBounds(sClosestLand))	// quit now, no need to look any farther
			break;
	}

	//NumMessage("closestLand = ",closestLand);
	return(sClosestLand);
}

INT32 FindNearbyDarkerSpot(TacticalActor *pSoldier)
{
	INT32 sGridNo, sClosestSpot = NOWHERE, sPathCost;
	INT32	iSpotValue, iBestSpotValue = 1000;
	INT16 sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;
	INT32 iSearchRange, iIgnoreRange = 0;
	INT32 iMaxSearchRange = 35;
	INT8 bLightLevel, bCurrLightLevel, bLightDiff;
	INT32 iRoamRange;
	INT32 sOrigin;
	UINT16 usMovementMode = DetermineMovementMode(pSoldier, AI_ACTION_LEAVE_WATER_GAS);
	BOOLEAN fFoundReachable = FALSE;

	bCurrLightLevel = LightTrueLevel(pSoldier->position().gridNo(), pSoldier->position().level());

	iRoamRange = RoamingRange(pSoldier, &sOrigin);

	if (!gfTurnBasedAI)
	{
		iMaxSearchRange = 15;
	}

	// start with a small search area, and expand it if we're unsuccessful
	// this should almost never need to search farther than 5 or 10 squares...
	for (iSearchRange = 5; iSearchRange <= iMaxSearchRange && (fFoundReachable || iSearchRange <= 5); iIgnoreRange = iSearchRange, iSearchRange += 10)
	{
		// determine maximum horizontal limits
		sMaxLeft = min(iSearchRange, (pSoldier->position().gridNo() % MAXCOL));
		//NumMessage("sMaxLeft = ",sMaxLeft);
		sMaxRight = min(iSearchRange, MAXCOL - ((pSoldier->position().gridNo() % MAXCOL) + 1));
		//NumMessage("sMaxRight = ",sMaxRight);

		// determine maximum vertical limits
		sMaxUp = min(iSearchRange, (pSoldier->position().gridNo() / MAXROW));
		//NumMessage("sMaxUp = ",sMaxUp);
		sMaxDown = min(iSearchRange, MAXROW - ((pSoldier->position().gridNo() / MAXROW) + 1));
		//NumMessage("sMaxDown = ",sMaxDown);

		// Call FindBestPath to set flags in all locations that we can
		// walk into within range.	We have to set some things up first...

		// reset the "reachable" flags in the region we're looking at
		for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
		{
			for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
			{
				sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
				if (!(sGridNo >= 0 && sGridNo < WORLD_MAX))
				{
					continue;
				}

				gpWorldLevelData[sGridNo].uiFlags &= ~(MAPELEMENT_REACHABLE);
			}
		}

		//gubNPCAPBudget = pSoldier->actionPoints().current();
		gubNPCAPBudget = 0;
		gubNPCDistLimit = (UINT8)iSearchRange;
		FindBestPath(pSoldier, GRIDSIZE, pSoldier->position().level(), usMovementMode, COPYREACHABLE, 0);	//dnl ch50 071009
		gubNPCAPBudget = 0;
		gubNPCDistLimit = 0;

		// Turn off the "reachable" flag for his current location
		// so we don't consider it
		gpWorldLevelData[pSoldier->position().gridNo()].uiFlags &= ~(MAPELEMENT_REACHABLE);

		// SET UP DOUBLE-LOOP TO STEP THROUGH POTENTIAL GRID #s
		for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
		{
			for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
			{
				// sevenfm: optimization - skip spots checked in previous loop
				if (sXOffset <= iIgnoreRange && sXOffset >= -iIgnoreRange && sYOffset <= iIgnoreRange && sYOffset >= -iIgnoreRange)
				{
					continue;
				}

				// calculate the next potential gridno
				sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
				//NumMessage("Testing gridno #",gridno);
				if (!(sGridNo >= 0 && sGridNo < WORLD_MAX))
				{
					continue;
				}

				if (!(gpWorldLevelData[sGridNo].uiFlags & MAPELEMENT_REACHABLE))
				{
					continue;
				}

				fFoundReachable = TRUE;

				// ignore blacklisted spot
				if (sGridNo == pSoldier->pathing().blackListGrid())
				{
					continue;
				}

				// require this character to stay within their roam range
				if (PythSpacesAway(sOrigin, sGridNo) > iRoamRange)
				{
					continue;
				}

				if (!CheckNPCDestination(pSoldier, sGridNo))
				{
					continue;
				}

				if (InLightAtNight(sGridNo, pSoldier->position().level()))
				{
					continue;
				}

				// screen out anything brighter than our current best spot
				bLightLevel = LightTrueLevel(sGridNo, pSoldier->position().level());

				//bLightDiff = gbLightSighting[0][ bCurrLightLevel ] - gbLightSighting[0][ bLightLevel ];
				bLightDiff = gGameExternalOptions.ubBrightnessVisionMod[bCurrLightLevel] - gGameExternalOptions.ubBrightnessVisionMod[bLightLevel];
				// if the spot is darker than our current location, then bLightDiff > 0
				// plus ignore differences of just 1 light level
				if (bLightDiff <= 1)
				{
					continue;
				}

				// CJC: here, unfortunately, we must calculate a path so we have an AP cost
				//sPathCost = LegalNPCDestination(pSoldier,sGridNo,ENSURE_PATH_COST,NOWATER,0);

				if (!LegalNPCDestination(pSoldier, sGridNo, IGNORE_PATH, NOWATER, 0))
				{
					continue;		// skip on to the next potential grid
				}

				sPathCost = PlotPath(pSoldier, sGridNo, FALSE, FALSE, FALSE, usMovementMode, pSoldier->movement().stealthMode(), pSoldier->movement().reverse(), 0);

				// check if spot is reachable
				if (sPathCost == 0)
				{
					continue;		// skip on to the next potential grid
				}

				// decrease the "cost" of the spot by the amount of light/darkness
				iSpotValue = sPathCost * 2 - bLightDiff;

				if (iSpotValue < iBestSpotValue)
				{
					// remember it instead
					iBestSpotValue = iSpotValue;
					//NumMessage("New shortest route = ",shortestPath);

					sClosestSpot = sGridNo;
					//NumMessage("New closest land at grid = ",closestLand);
				}
			}
		}

		// if we found a piece of land in this search area		
		if (!TileIsOutOfBounds(sClosestSpot))	// quit now, no need to look any farther
		{
			break;
		}
	}

	return(sClosestSpot);
}

#define MINIMUM_REQUIRED_STATUS 70

INT8 SearchForItems( TacticalActor * pSoldier, INT8 bReason, UINT16 usItem )
{
	DebugMsg(TOPIC_JA2AI,DBG_LEVEL_3,String("SearchForItems"));
	DebugAI(AI_MSG_INFO, pSoldier, String("SearchForItems [%d] bReason %d usItem %d", pSoldier->identity().id(), bReason, usItem));

	INT32					iSearchRange;
	INT16					sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;
	INT32 sGridNo;
	INT32					sBestSpot = NOWHERE;
	INT32					iTempValue, iValue, iBestValue = 0;
	ITEM_POOL *				pItemPool;
	OBJECTTYPE *			pObj;
	INVTYPE *				pItem;
	INT32					iItemIndex, iBestItemIndex;
	BOOLEAN					fDumbEnoughtoPickup = FALSE;

	iTempValue = -1;
	iItemIndex = iBestItemIndex = -1;

	// No fair picking up weapons while boxing!
	if (gTacticalStatus.bBoxingState == BOXING)
	{
		DebugAI(AI_MSG_INFO, pSoldier, String("no picking up items for boxers!"));
		return AI_ACTION_NONE;
	}

	if (pSoldier->actionPoints().current() < GetBasicAPsToPickupItem( pSoldier ))
	{
		DebugAI(AI_MSG_INFO, pSoldier, String("not enough AP!"));
		return( AI_ACTION_NONE );
	}

	if ( !IS_MERC_BODY_TYPE( pSoldier ) )
	{
		DebugAI(AI_MSG_INFO, pSoldier, String("not merc bodytype!"));
		return( AI_ACTION_NONE );
	}

	iSearchRange = gbDiff[DIFF_MAX_COVER_RANGE][ SoldierDifficultyLevel( pSoldier ) ];
	DebugAI(AI_MSG_INFO, pSoldier, String("use search range %d", iSearchRange));

	switch (pSoldier->aiBehavior().attitude())
	{
		case DEFENSIVE:		iSearchRange --;	break;
		case BRAVESOLO:		iSearchRange += 2; break;
		case BRAVEAID:		iSearchRange += 2; break;
		case CUNNINGSOLO:	iSearchRange -= 2; break;
		case CUNNINGAID:	iSearchRange -= 2; break;
		case AGGRESSIVE:	iSearchRange ++;	break;
	}

	// maximum search range is 1 tile / 10 pts of wisdom
	if (iSearchRange > (pSoldier->statistics().wisdom() / 10))
	{
		iSearchRange = (pSoldier->statistics().wisdom() / 10);
	}

	if (!gfTurnBasedAI)
	{
		// don't search so far in realtime
		iSearchRange /= 2;
	}

	// don't search so far for items
	iSearchRange /= 2;

	// determine maximum horizontal limits
	sMaxLeft  = min( iSearchRange, (pSoldier->position().gridNo() % MAXCOL));
	//NumMessage("sMaxLeft = ",sMaxLeft);
	sMaxRight = min( iSearchRange, MAXCOL - ((pSoldier->position().gridNo() % MAXCOL) + 1));
	//NumMessage("sMaxRight = ",sMaxRight);

	// determine maximum vertical limits
	sMaxUp   = min( iSearchRange, (pSoldier->position().gridNo() / MAXROW));
	//NumMessage("sMaxUp = ",sMaxUp);
	sMaxDown = min( iSearchRange, MAXROW - ((pSoldier->position().gridNo() / MAXROW) + 1));
	//NumMessage("sMaxDown = ",sMaxDown);

	// Call FindBestPath to set flags in all locations that we can
	// walk into within range.	We have to set some things up first...

	// set the distance limit of the square region
	gubNPCDistLimit = (UINT8) iSearchRange;

	// set an AP limit too, to our APs less the cost of picking up an item
	// and less the cost of dropping an item since we might need to do that
	gubNPCAPBudget = pSoldier->actionPoints().current() - GetBasicAPsToPickupItem( pSoldier );

	// reset the "reachable" flags in the region we're looking at
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			if ( !(sGridNo >=0 && sGridNo < WORLD_MAX) )
			{
				continue;
			}

			gpWorldLevelData[sGridNo].uiFlags &= ~(MAPELEMENT_REACHABLE);
		}
	}

	FindBestPath( pSoldier, GRIDSIZE, pSoldier->position().level(), DetermineMovementMode( pSoldier, AI_ACTION_PICKUP_ITEM ), COPYREACHABLE, 0 );//dnl ch50 071009

	// Flugente: if the soldier is 'dumb enough', he may pick up certain items... which can be used to lure the AI into traps
	if (pSoldier->statistics().wisdom() < 70)
	{
		DebugAI(AI_MSG_INFO, pSoldier, String("dumb enough to pick up items"));
		fDumbEnoughtoPickup = TRUE;
	}

	// SET UP DOUBLE-LOOP TO STEP THROUGH POTENTIAL GRID #s
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			// calculate the next potential gridno
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			if ( !(sGridNo >=0 && sGridNo < WORLD_MAX) )
			{
				continue;
			}

			if (!CheckNPCDestination(pSoldier, sGridNo))
			{
				continue;
			}

			if (!LegalNPCDestination(pSoldier, sGridNo, IGNORE_PATH, NOWATER, 0))
			{
				continue;		// skip on to the next potential grid
			}

			if ( (gpWorldLevelData[sGridNo].uiFlags & MAPELEMENT_ITEMPOOL_PRESENT)
					&& (gpWorldLevelData[sGridNo].uiFlags & MAPELEMENT_REACHABLE) )
			{

				// ignore blacklisted spot
				if ( sGridNo == pSoldier->pathing().blackListGrid() )
				{
					continue;
				}

				DebugAI(AI_MSG_INFO, pSoldier, String("check spot %d, found items", sGridNo));

				iValue = 0;
				GetItemPool( sGridNo, &pItemPool, pSoldier->position().level() );
				switch( bReason )
				{
					case SEARCH_AMMO:
						// we are looking for ammo to match the gun in usItem
						DebugAI(AI_MSG_INFO, pSoldier, String("SEARCH_AMMO"));
						while( pItemPool )
						{
							pObj = &(gWorldItems[ pItemPool->iItemIndex ].object);
							pItem = &(Item[pObj->usItem]);
							DebugAI(AI_MSG_INFO, pSoldier, String("check item %d at %d status %d", pObj->usItem, sGridNo, (*pObj)[0]->data.objectStatus));

							if ( pItem->usItemClass == IC_GUN && (*pObj)[0]->data.objectStatus >= MINIMUM_REQUIRED_STATUS )
							{
								// maybe this gun has ammo (adjust for whether it is better than ours!)
								if ( (*pObj)[0]->data.gun.bGunAmmoStatus < 0 || (*pObj)[0]->data.gun.ubGunShotsLeft == 0 || (ItemHasFingerPrintID(pObj->usItem) && (*pObj)[0]->data.ubImprintID != NO_PROFILE && (*pObj)[0]->data.ubImprintID != pSoldier->identity().profile()) )
								{
									iTempValue = 0;
								}
								else
								{
									iTempValue = (*pObj)[0]->data.gun.ubGunShotsLeft * Weapon[pObj->usItem].ubDeadliness / Weapon[usItem].ubDeadliness;
								}
							}
							else if (ValidAmmoType( usItem, pObj->usItem ) )
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("good ammo"));
								iTempValue = TotalPoints( pObj );
							}
							else
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("not gun, not ammo - skip"));
								iTempValue = 0;
							}
							DebugAI(AI_MSG_INFO, pSoldier, String("iTempValue %d", iTempValue));

							if (iTempValue > iValue )
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("select this ammo"));
								iValue = iTempValue;
								iItemIndex = pItemPool->iItemIndex;
							}
							pItemPool = pItemPool->pNext;
						}
						break;
					case SEARCH_WEAPONS:
						DebugAI(AI_MSG_INFO, pSoldier, String("SEARCH_WEAPONS"));
						while( pItemPool )
						{
							pObj = &(gWorldItems[ pItemPool->iItemIndex ].object);
							pItem = &(Item[pObj->usItem]);
							DebugAI(AI_MSG_INFO, pSoldier, String("check item %d at %d status %d", pObj->usItem, sGridNo, (*pObj)[0]->data.objectStatus));

							if (pItem->usItemClass & IC_WEAPON && (*pObj)[0]->data.objectStatus >= MINIMUM_REQUIRED_STATUS )
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("weapon has good status"));
								if ( (pItem->usItemClass & IC_GUN) && ((*pObj)[0]->data.gun.bGunAmmoStatus < 0 || (*pObj)[0]->data.gun.ubGunShotsLeft == 0 || (ItemHasFingerPrintID(pObj->usItem) && (*pObj)[0]->data.ubImprintID != NO_PROFILE && (*pObj)[0]->data.ubImprintID != pSoldier->identity().profile()) ) )
								{
									// jammed or out of ammo, skip it!
									DebugAI(AI_MSG_INFO, pSoldier, String("jammed or out of ammo, skip it!"));
									iTempValue = 0;
								}
								else if ( Item[pSoldier->inventory()[HANDPOS].usItem].usItemClass & IC_WEAPON )
								{
									DebugAI(AI_MSG_INFO, pSoldier, String("compare with gun in hand"));
									if (Weapon[pObj->usItem].ubDeadliness > Weapon[pSoldier->inventory()[HANDPOS].usItem].ubDeadliness)
									{
										iTempValue = 100 * Weapon[pObj->usItem].ubDeadliness / Weapon[pSoldier->inventory()[HANDPOS].usItem].ubDeadliness;
									}
									else
									{
										iTempValue = 0;
									}
								}
								else
								{
									iTempValue = 200 + Weapon[pObj->usItem].ubDeadliness;
								}
							}
							else
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("not weapon or bad status"));
								iTempValue = 0;
							}
							DebugAI(AI_MSG_INFO, pSoldier, String("iTempValue %d", iTempValue));

							if (iTempValue > iValue )
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("select this weapon"));
								iValue = iTempValue;
								iItemIndex = pItemPool->iItemIndex;
							}
							pItemPool = pItemPool->pNext;
						}
						break;
					default:
						DebugAI(AI_MSG_INFO, pSoldier, String("search items"));

						while( pItemPool )
						{
							pObj = &(gWorldItems[ pItemPool->iItemIndex ].object);
							pItem = &(Item[pObj->usItem]);
							DebugAI(AI_MSG_INFO, pSoldier, String("check item %d at %d status %d", pObj->usItem, sGridNo, (*pObj)[0]->data.objectStatus));

							if ( pItem->usItemClass & IC_WEAPON && (*pObj)[0]->data.objectStatus >= MINIMUM_REQUIRED_STATUS )
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("gun has good status"));
								if ( (pItem->usItemClass & IC_GUN) && ((*pObj)[0]->data.gun.bGunAmmoStatus < 0 || (*pObj)[0]->data.gun.ubGunShotsLeft == 0 || (ItemHasFingerPrintID(pObj->usItem) && (*pObj)[0]->data.ubImprintID != NO_PROFILE && (*pObj)[0]->data.ubImprintID != pSoldier->identity().profile()) ) )
								{
									// jammed or out of ammo, skip it!
									DebugAI(AI_MSG_INFO, pSoldier, String("jammed or out of ammo, skip it!"));
									iTempValue = 0;
								}
								else if (pSoldier->inventory()[HANDPOS].exists() && (Item[pSoldier->inventory()[HANDPOS].usItem].usItemClass & IC_WEAPON))
								{
									DebugAI(AI_MSG_INFO, pSoldier, String("compare with weapon in hand"));
									if (Weapon[pObj->usItem].ubDeadliness > Weapon[pSoldier->inventory()[HANDPOS].usItem].ubDeadliness)
									{
										if ((Weapon[pSoldier->inventory()[HANDPOS].usItem].ubDeadliness != NULL) && (Weapon[pSoldier->inventory()[HANDPOS].usItem].ubDeadliness > 0))
										{
											iTempValue = 100 * Weapon[pObj->usItem].ubDeadliness / Weapon[pSoldier->inventory()[HANDPOS].usItem].ubDeadliness;
										}
										else
										{
											iTempValue = 100 * Weapon[pObj->usItem].ubDeadliness;
										}
									}
									else
									{
										iTempValue = 0;
									}
								}
								else
								{
									iTempValue = 200 + Weapon[pObj->usItem].ubDeadliness;
								}
								DebugAI(AI_MSG_INFO, pSoldier, String("iTempValue %d", iTempValue));
							}
							else if	(pItem->usItemClass == IC_ARMOUR && (*pObj)[0]->data.objectStatus >= MINIMUM_REQUIRED_STATUS )
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("armour has good status"));
								iTempValue = 0;

								switch( Armour[pItem->ubClassIndex].ubArmourClass )
								{
									case ARMOURCLASS_HELMET:
										DebugAI(AI_MSG_INFO, pSoldier, String("ARMOURCLASS_HELMET"));
										if (pSoldier->inventory()[HELMETPOS].exists() == false)
										{
											iTempValue = 200 + EffectiveArmour( pObj );
										}
										else if ( EffectiveArmour( &(pSoldier->inventory()[HELMETPOS]) ) < EffectiveArmour( pObj ) )
										{
											iTempValue = 100 * EffectiveArmour( pObj ) / (EffectiveArmour( &(pSoldier->inventory()[HELMETPOS]) ) + 1);
										}
										DebugAI(AI_MSG_INFO, pSoldier, String("iTempValue %d", iTempValue));
										break;
									case ARMOURCLASS_VEST:
										DebugAI(AI_MSG_INFO, pSoldier, String("ARMOURCLASS_VEST"));
										if (pSoldier->inventory()[VESTPOS].exists() == false)
										{
											iTempValue = 200 + EffectiveArmour( pObj );
										}
										else if ( EffectiveArmour( &(pSoldier->inventory()[VESTPOS]) ) < EffectiveArmour( pObj ) )
										{
											iTempValue = 100 * EffectiveArmour( pObj ) / (EffectiveArmour( &(pSoldier->inventory()[VESTPOS]) ) + 1);
										}
										DebugAI(AI_MSG_INFO, pSoldier, String("iTempValue %d", iTempValue));
										break;
									case ARMOURCLASS_LEGGINGS:
										DebugAI(AI_MSG_INFO, pSoldier, String("ARMOURCLASS_LEGGINGS"));
										if (pSoldier->inventory()[LEGPOS].exists() == false)
										{
											iTempValue = 200 + EffectiveArmour( pObj );
										}
										else if ( EffectiveArmour( &(pSoldier->inventory()[LEGPOS]) ) < EffectiveArmour( pObj ) )
										{
											iTempValue = 100 * EffectiveArmour( pObj ) / (EffectiveArmour( &(pSoldier->inventory()[LEGPOS]) ) + 1);
										}
										DebugAI(AI_MSG_INFO, pSoldier, String("iTempValue %d", iTempValue));
										break;
									default:
										{
											break;
										}
								}
							}
							// Flugente: if the soldier is 'dumb enough', he may pick up 'interesting items'. This can be used to lure him into traps (a certain scene in FMJ comes to mind)
							else if ( fDumbEnoughtoPickup && pItem->usItemClass == IC_MISC && HasItemFlag(pObj->usItem, ATTENTION_ITEM) )
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("dumb soldier picks up item!"));
								// oooh... shiny!
								iTempValue = 1000;
							}
							else
							{
								iTempValue = 0;
							}
							DebugAI(AI_MSG_INFO, pSoldier, String("iTempValue %d", iTempValue));

							if (iTempValue > iValue )
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("select item"));
								iValue = iTempValue;
								iItemIndex = pItemPool->iItemIndex;
							}
							pItemPool = pItemPool->pNext;
						}
						break;
				}
				iValue = (3 * iValue) / (3 + PythSpacesAway( sGridNo, pSoldier->position().gridNo() ));
				DebugAI(AI_MSG_INFO, pSoldier, String("iBestValue %d", iBestValue));
				DebugAI(AI_MSG_INFO, pSoldier, String("value modified by distance %d", iValue));

				if (iValue > iBestValue )
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("select item at %d", sGridNo));
					sBestSpot = sGridNo;
					iBestValue = iValue;
					iBestItemIndex = iItemIndex;
				}
			}
		}
	}
	
	if (!TileIsOutOfBounds(sBestSpot))
	{
		DebugAI(AI_MSG_INFO, pSoldier, String("%d decides to pick up %S", pSoldier->identity().id(), ItemNames[gWorldItems[iBestItemIndex].object.usItem]));
		if (Item[gWorldItems[ iBestItemIndex ].object.usItem].usItemClass == IC_GUN)
		{
			//CHRISL: This is the line from ADB's code but I removed it, for now, to match what 0verhaul has been working on
			//if (pSoldier->inventory()[HANDPOS].exists() == true && PlaceInAnyPocket(pSoldier, &pSoldier->inventory()[HANDPOS], false) == false)
			if (FindBetterSpotForItem( pSoldier, HANDPOS ) == FALSE)
			{
				if (pSoldier->actionPoints().current() < GetBasicAPsToPickupItem( pSoldier ) + GetBasicAPsToPickupItem( pSoldier ))
				{
					return( AI_ACTION_NONE );
				}
				if (pSoldier->inventory()[HANDPOS].fFlags & OBJECT_UNDROPPABLE)
				{
					// destroy this item!
					DebugAI(AI_MSG_INFO, pSoldier, String("%d decides he must drop %S first so destroys it", pSoldier->identity().id(), ItemNames[pSoldier->inventory()[HANDPOS].usItem]));
					DeleteObj( &(pSoldier->inventory()[HANDPOS]) );
					DeductPoints( pSoldier, GetBasicAPsToPickupItem( pSoldier ), 0, AFTERACTION_INTERRUPT );
				}
				else
				{
					// we want to drop this item!
					DebugAI(AI_MSG_INFO, pSoldier, String("%d decides he must drop %S first", pSoldier->identity().id(), ItemNames[pSoldier->inventory()[HANDPOS].usItem]));

					pSoldier->aiPlanning().nextAction() = AI_ACTION_PICKUP_ITEM;
					pSoldier->aiPlanning().nextActionData() = sBestSpot;
					pSoldier->pendingAction().nextSpecialData() = iBestItemIndex;
					return( AI_ACTION_DROP_ITEM );
				}
			}
		}
		pSoldier->pendingAction().primaryData() = iBestItemIndex;
		pSoldier->aiPlanning().actionData() = sBestSpot;
		return( AI_ACTION_PICKUP_ITEM );
	}

	return( AI_ACTION_NONE );
}

INT32 FindClosestDoor( TacticalActor * pSoldier )
{
	INT32		sClosestDoor = NOWHERE;
	INT32		iSearchRange;
	INT16		sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;
	INT32 sGridNo;
	INT32		iDist, iClosestDist = 10;

	iSearchRange = 5;

	// determine maximum horizontal limits
	sMaxLeft  = min( iSearchRange, (pSoldier->position().gridNo() % MAXCOL));
	//NumMessage("sMaxLeft = ",sMaxLeft);
	sMaxRight = min( iSearchRange, MAXCOL - ((pSoldier->position().gridNo() % MAXCOL) + 1));
	//NumMessage("sMaxRight = ",sMaxRight);

	// determine maximum vertical limits
	sMaxUp   = min( iSearchRange, (pSoldier->position().gridNo() / MAXROW));
	//NumMessage("sMaxUp = ",sMaxUp);
	sMaxDown = min( iSearchRange, MAXROW - ((pSoldier->position().gridNo() / MAXROW) + 1));
	//NumMessage("sMaxDown = ",sMaxDown);
	// SET UP DOUBLE-LOOP TO STEP THROUGH POTENTIAL GRID #s
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			// calculate the next potential gridno
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			if (FindStructure( sGridNo, STRUCTURE_ANYDOOR ) != NULL)
			{
				iDist = PythSpacesAway( pSoldier->position().gridNo(), sGridNo );
				if (iDist < iClosestDist)
				{
					iClosestDist = iDist;
					sClosestDoor = sGridNo;
				}
			}
		}
	}

	return( sClosestDoor );
}

INT32 FindNearestEdgepointOnSpecifiedEdge( INT32 sGridNo, INT8 bEdgeCode )
{
	INT32			iLoop;
	INT32			*psEdgepointArray;
	INT32			iEdgepointArraySize;
	INT32			sClosestSpot = NOWHERE, sClosestDist = 0x7FFF, sTempDist;

	switch( bEdgeCode )
	{
		case NORTH_EDGEPOINT_SEARCH:
			psEdgepointArray = gps1stNorthEdgepointArray;
			iEdgepointArraySize = gus1stNorthEdgepointArraySize;
			break;
		case EAST_EDGEPOINT_SEARCH:
			psEdgepointArray = gps1stEastEdgepointArray;
			iEdgepointArraySize = gus1stEastEdgepointArraySize;
			break;
		case SOUTH_EDGEPOINT_SEARCH:
			psEdgepointArray = gps1stSouthEdgepointArray;
			iEdgepointArraySize = gus1stSouthEdgepointArraySize;
			break;
		case WEST_EDGEPOINT_SEARCH:
			psEdgepointArray = gps1stWestEdgepointArray;
			iEdgepointArraySize = gus1stWestEdgepointArraySize;
			break;
		default:
			// WTF???
			return( NOWHERE );
	}

	// Do a 2D search to find the closest map edgepoint and
	// try to create a path there

	for ( iLoop = 0; iLoop < iEdgepointArraySize; iLoop++ )
	{
		sTempDist = PythSpacesAway( sGridNo, psEdgepointArray[ iLoop ] );
		if ( sTempDist < sClosestDist )
		{
			sClosestDist = sTempDist;
			sClosestSpot = psEdgepointArray[ iLoop ];
		}
	}

	return( sClosestSpot );
}

INT32 FindNearestEdgePoint( INT32 sGridNo )
{
	INT16			sGridX, sGridY;
	INT16			sScreenX, sScreenY, sMaxScreenX, sMaxScreenY;
	INT16			sDist[5], sMinDist;
	INT32			iLoop;
	INT8			bMinIndex;
	INT32 *		psEdgepointArray;
	INT32			iEdgepointArraySize;
	INT32			sClosestSpot = NOWHERE, sClosestDist = 0x7FFF, sTempDist;

	ConvertGridNoToXY( sGridNo, &sGridX, &sGridY );
	GetWorldXYAbsoluteScreenXY( sGridX, sGridY, &sScreenX, &sScreenY );

	sMaxScreenX = gsBRX - gsTLX;
	sMaxScreenY = gsBRY - gsTLY;

	sDist[0] = 0x7FFF;
	sDist[1] = sScreenX;									// west
	sDist[2] = sMaxScreenX - sScreenX;		// east
	sDist[3] = sScreenY;									// north
	sDist[4] = sMaxScreenY - sScreenY;		// south

	sMinDist = sDist[0];
	bMinIndex = 0;
	for( iLoop = 1; iLoop < 5; iLoop++)
	{
		if ( sDist[ iLoop ] < sMinDist )
		{
			sMinDist = sDist[ iLoop ];
			bMinIndex = (INT8) iLoop;
		}
	}

	switch( bMinIndex )
	{
		case 1:
			psEdgepointArray = gps1stWestEdgepointArray;
			iEdgepointArraySize = gus1stWestEdgepointArraySize;
			break;
		case 2:
			psEdgepointArray = gps1stEastEdgepointArray;
			iEdgepointArraySize = gus1stEastEdgepointArraySize;
			break;
		case 3:
			psEdgepointArray = gps1stNorthEdgepointArray;
			iEdgepointArraySize = gus1stNorthEdgepointArraySize;
			break;
		case 4:
			psEdgepointArray = gps1stSouthEdgepointArray;
			iEdgepointArraySize = gus1stSouthEdgepointArraySize;
			break;
		default:
			// WTF???
			return( NOWHERE );
	}

	// Do a 2D search to find the closest map edgepoint and
	// try to create a path there

	for ( iLoop = 0; iLoop < iEdgepointArraySize; iLoop++ )
	{
		sTempDist = PythSpacesAway( sGridNo, psEdgepointArray[ iLoop ] );
		if ( sTempDist < sClosestDist )
		{
			sClosestDist = sTempDist;
			sClosestSpot = psEdgepointArray[ iLoop ];
		}
	}

	return( sClosestSpot );
}

#define EDGE_OF_MAP_SEARCH 5

INT32 FindNearbyPointOnEdgeOfMap( TacticalActor * pSoldier, INT8 * pbDirection )
{
	INT32		iSearchRange;
	INT16		sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;

	INT32 sGridNo, sClosestSpot = NOWHERE;
	INT8	bDirection, bClosestDirection;
	INT32 iPathCost, iClosestPathCost = 1000;

	bClosestDirection = -1;

	// An invalid origin -- an unplaced / off-world scheduled NPC whose sGridNo is
	// NOWHERE, or a gridno past this sector's map -- would index gpWorldLevelData
	// out of bounds below. Return NOWHERE (the not-found sentinel callers already
	// handle) instead of crashing.
	if ( pSoldier->position().gridNo() < 0 || pSoldier->position().gridNo() >= WORLD_MAX )
	{
		return( NOWHERE );
	}

	// Call FindBestPath to set flags in all locations that we can
	// walk into within range.	We have to set some things up first...

	// set the distance limit of the square region
	gubNPCDistLimit = EDGE_OF_MAP_SEARCH;

	iSearchRange = EDGE_OF_MAP_SEARCH;

	// determine maximum horizontal limits
	sMaxLeft  = min( iSearchRange, (pSoldier->position().gridNo() % MAXCOL));
	//NumMessage("sMaxLeft = ",sMaxLeft);
	sMaxRight = min( iSearchRange, MAXCOL - ((pSoldier->position().gridNo() % MAXCOL) + 1));
	//NumMessage("sMaxRight = ",sMaxRight);

	// determine maximum vertical limits
	sMaxUp   = min( iSearchRange, (pSoldier->position().gridNo() / MAXROW));
	//NumMessage("sMaxUp = ",sMaxUp);
	sMaxDown = min( iSearchRange, MAXROW - ((pSoldier->position().gridNo() / MAXROW) + 1));

	// reset the "reachable" flags in the region we're looking at
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			if ( !(sGridNo >=0 && sGridNo < WORLD_MAX) )
			{
				continue;
			}

			gpWorldLevelData[sGridNo].uiFlags &= ~(MAPELEMENT_REACHABLE);
		}
	}

	FindBestPath( pSoldier, GRIDSIZE, pSoldier->position().level(), WALKING, COPYREACHABLE, 0 );//dnl ch50 071009

	// Turn off the "reachable" flag for his current location
	// so we don't consider it
	gpWorldLevelData[pSoldier->position().gridNo()].uiFlags &= ~(MAPELEMENT_REACHABLE);

	// SET UP DOUBLE-LOOP TO STEP THROUGH POTENTIAL GRID #s
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			// calculate the next potential gridno
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			if ( !(sGridNo >=0 && sGridNo < WORLD_MAX) )
			{
				continue;
			}

			if (!(gpWorldLevelData[sGridNo].uiFlags & MAPELEMENT_REACHABLE))
			{
				continue;
			}

			if (GridNoOnEdgeOfMap( sGridNo, &bDirection ) )
			{
				iPathCost = PythSpacesAway( pSoldier->position().gridNo(), sGridNo );

				if (iPathCost < iClosestPathCost)
				{
					// this place is closer
					sClosestSpot = sGridNo;
					iClosestPathCost = iPathCost;
					bClosestDirection = bDirection;
				}
			}
		}
	}

	*pbDirection = bClosestDirection;
	return( sClosestSpot );
}

INT32 FindRouteBackOntoMap( TacticalActor * pSoldier, INT32 sDestGridNo )
{
	// the first thing to do is restore the soldier's gridno from the X and Y
	// values

	// well, let's TRY just taking a path to the place we're supposed to go...
	if ( FindBestPath( pSoldier, sDestGridNo, pSoldier->position().level(), WALKING, COPYROUTE, 0 ) )
	{
		pSoldier->pathing().stored() = TRUE;
		return( sDestGridNo );
	}
	else
	{
		return( NOWHERE );
	}

}

INT32 FindClosestBoxingRingSpot( TacticalActor * pSoldier, BOOLEAN fInRing )
{
	INT32		iSearchRange;
	INT16		sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;

	INT32 sGridNo, sClosestSpot = NOWHERE;
	INT32 iDistance, iClosestDistance = 9999;
	UINT16 usRoom;
	TacticalActor *pDarren;
	pDarren = FindSoldierByProfileID(DARREN, FALSE);

	// set the distance limit of the square region
	iSearchRange = 7;

	// determine maximum horizontal limits
	sMaxLeft  = min( iSearchRange, (pSoldier->position().gridNo() % MAXCOL));
	//NumMessage("sMaxLeft = ",sMaxLeft);
	sMaxRight = min( iSearchRange, MAXCOL - ((pSoldier->position().gridNo() % MAXCOL) + 1));
	//NumMessage("sMaxRight = ",sMaxRight);

	// determine maximum vertical limits
	sMaxUp   = min( iSearchRange, (pSoldier->position().gridNo() / MAXROW));
	//NumMessage("sMaxUp = ",sMaxUp);
	sMaxDown = min( iSearchRange, MAXROW - ((pSoldier->position().gridNo() / MAXROW) + 1));

	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			// calculate the next potential gridno
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			if (!TileIsOutOfBounds(sGridNo) &&
				InARoom(sGridNo, &usRoom))
			{
				// NB the parentheses around the ring test matter: && binds tighter
				// than ||, so without them the LegalNPCDestination() check only
				// applied to the "leave ring" case. The "move into ring" case then
				// accepted illegal/unreachable ring tiles, and the boxer would
				// GET_CLOSER toward a spot it could never stand on -> turn never
				// completes -> boxing match hangs.
				if (((fInRing && usRoom == BOXING_RING) || (!fInRing && usRoom != BOXING_RING)) &&
					LegalNPCDestination(pSoldier, sGridNo, IGNORE_PATH, NOWATER, 0))
				{
					// sevenfm: for player merc, find spot closest to Darren
					if (pDarren && pSoldier->roster().team() == gbPlayerNum)
						iDistance = PythSpacesAway(pDarren->position().gridNo(), sGridNo) + PythSpacesAway(pSoldier->position().gridNo(), sGridNo);
					else
						iDistance = abs(sXOffset) + abs(sYOffset);

					if (iDistance < iClosestDistance && WhoIsThere2(sGridNo, 0) == NOBODY)
					{
						sClosestSpot = sGridNo;
						iClosestDistance = iDistance;
					}
				}
			}
		}
	}

	return( sClosestSpot );
}

INT32 FindNearestOpenableNonDoor( INT32 sStartGridNo )
{
	INT32		iSearchRange;
	INT16		sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;

	INT32 sGridNo, sClosestSpot = NOWHERE;
	INT32		iDistance, iClosestDistance = 9999;
	STRUCTURE * pStructure;

	// set the distance limit of the square region
	iSearchRange = 7;

	// determine maximum horizontal limits
	sMaxLeft  = min( iSearchRange, (sStartGridNo % MAXCOL));
	//NumMessage("sMaxLeft = ",sMaxLeft);
	sMaxRight = min( iSearchRange, MAXCOL - ((sStartGridNo % MAXCOL) + 1));
	//NumMessage("sMaxRight = ",sMaxRight);

	// determine maximum vertical limits
	sMaxUp   = min( iSearchRange, (sStartGridNo / MAXROW));
	//NumMessage("sMaxUp = ",sMaxUp);
	sMaxDown = min( iSearchRange, MAXROW - ((sStartGridNo / MAXROW) + 1));

	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			// calculate the next potential gridno
			sGridNo = sStartGridNo + sXOffset + (MAXCOL * sYOffset);
			pStructure = FindStructure( sGridNo, STRUCTURE_OPENABLE );
			if (pStructure)
			{
				// skip any doors
				while ( pStructure && ( pStructure->fFlags & STRUCTURE_ANYDOOR ) )
				{
					pStructure = FindNextStructure( pStructure, STRUCTURE_OPENABLE );
				}
				// if we still have a pointer, then we have found a valid non-door openable structure
				if ( pStructure )
				{
					iDistance = CardinalSpacesAway( sGridNo, sStartGridNo );
					if ( iDistance < iClosestDistance )
					{
						sClosestSpot = sGridNo;
						iClosestDistance = iDistance;
					}
				}
			}
		}
	}

	return( sClosestSpot );

}

INT32 FindFlankingSpot(TacticalActor *pSoldier, INT32 sPos, INT8 bAction )
{
	INT32 sGridNo;
	INT32 sBestSpot = NOWHERE;
	INT32 iSearchRange = 8;	// sevenfm: increase search range
	INT16 sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;
	INT16 sDistanceVisible = VISION_RANGE;

	DebugMsg ( TOPIC_JA2AI , DBG_LEVEL_3 , String("FindFlankingSpot: orig loc = %d, loc to flank = %d", pSoldier->position().gridNo() , sPos));

	// hit the edge of the map
	/*if ( FindNearestEdgePoint ( pSoldier->sGridNo ) == pSoldier->sGridNo	)
		return NOWHERE;*/

	// sevenfm: use max AP at the start of new turn
	//gubNPCAPBudget=(UINT8) (iSearchRange*3);
	gubNPCAPBudget= TacticalActorTurnBudget::calculateTurnGrant(*pSoldier);

	// determine maximum horizontal limits
	sMaxLeft  = min( iSearchRange, (pSoldier->position().gridNo() % MAXCOL));
	//NumMessage("sMaxLeft = ",sMaxLeft);
	sMaxRight = min( iSearchRange, MAXCOL - ((pSoldier->position().gridNo() % MAXCOL) + 1));
	//NumMessage("sMaxRight = ",sMaxRight);

	// determine maximum vertical limits
	sMaxUp   = min( iSearchRange, (pSoldier->position().gridNo() / MAXROW));
	//NumMessage("sMaxUp = ",sMaxUp);
	sMaxDown = min( iSearchRange, MAXROW - ((pSoldier->position().gridNo() / MAXROW) + 1));
	//NumMessage("sMaxDown = ",sMaxDown);

	// Call FindBestPath to set flags in all locations that we can
	// walk into within range.	We have to set some things up first...

	// set the distance limit of the square region
	gubNPCDistLimit = (UINT8) iSearchRange;

	// reset the "reachable" flags in the region we're looking at
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			if ( !(sGridNo >=0 && sGridNo < WORLD_MAX) )
			{
				continue;
			}

			gpWorldLevelData[sGridNo].uiFlags &= ~(MAPELEMENT_REACHABLE);
		}
	}

	FindBestPath( pSoldier, GRIDSIZE, pSoldier->position().level(), DetermineMovementMode( pSoldier, bAction ), COPYREACHABLE, 0 );//dnl ch50 071009

	// Turn off the "reachable" flag for his current location
	// so we don't consider it
	gpWorldLevelData[pSoldier->position().gridNo()].uiFlags &= ~(MAPELEMENT_REACHABLE);

	// get direction of position to flank from soldier's position
	INT16 sDir = GetDirectionFromGridNo ( sPos, pSoldier) ;
	INT16 sDesiredDir;
	INT16 sTempDir;
	INT16 sTempDist, sBestDist=0;

	// sevenfm: better direction calculation (use arrays)
	switch ( bAction )
	{
	case AI_ACTION_FLANK_LEFT:
		sDesiredDir = gTwoCCDirection[ sDir ];
		break;
	case AI_ACTION_FLANK_RIGHT:
		sDesiredDir = gTwoCDirection[ sDir ];
		break;
	case AI_ACTION_WITHDRAW:
		sDesiredDir = gOppositeDirection[ sDir ];
		break;
	default:
		sDesiredDir = sDir;
	}

	DebugMsg ( TOPIC_JA2AI , DBG_LEVEL_3 , String("FindFlankingSpot: direction to loc = %d, dir to flank = %d", sDir , sDesiredDir ));

	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			// calculate the next potential gridno
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);

			//NumMessage("Testing gridno #",gridno);
			if ( !(sGridNo >=0 && sGridNo < WORLD_MAX) )
			{
				continue;
			}

			if (!(gpWorldLevelData[sGridNo].uiFlags & MAPELEMENT_REACHABLE))
			{
				continue;
			}

			if ( sGridNo == pSoldier->pathing().blackListGrid() )
			{
				continue;
			}

			// sevenfm: skip tiles too close to edge
			if ( PythSpacesAway( FindNearestEdgePoint ( sGridNo ), sGridNo ) <= 2 )
			{
				continue;
			}

			sTempDir = GetDirectionFromGridNo ( sGridNo, pSoldier );
			sTempDist = GetRangeInCellCoordsFromGridNoDiff( pSoldier->position().gridNo() , sGridNo );

			// sevenfm: don't go into deep water for flanking
			if (!AllowDeepWaterFlanking(pSoldier) &&
				DeepWater(sGridNo, pSoldier->position().level()) &&
				!DeepWater(pSoldier->position().gridNo(), pSoldier->position().level()))
			{
				continue;
			}

			// sevenfm: allow water flanking only for CUNNINGSOLO soldiers
			if( Water( sGridNo, pSoldier->position().level() ) &&
				pSoldier->aiBehavior().attitude() != CUNNINGSOLO &&
				pSoldier->aiBehavior().attitude() != CUNNINGAID )
			{
				continue;
			}

			// avoid fresh corpses
			if (GetNearestRottingCorpseAIWarning(sGridNo) > 0)
			{
				continue;
			}

			if (!CheckNPCDestination(pSoldier, sGridNo))
			{
				continue;
			}

			if (!LegalNPCDestination(pSoldier, sGridNo, IGNORE_PATH, WATEROK, 0))
			{
				continue;		// skip on to the next potential grid
			}

			// sevenfm: skip buildings if not in building already, because soldiers often run into buildings and stop flanking
			if (InARoom(sGridNo, NULL) && !InARoom(pSoldier->position().gridNo(), NULL))
			{
				continue;
			}

			// sevenfm: penalize locations too far from noise gridno
			if (PythSpacesAway(sGridNo, sPos) > MAX_FLANK_DIST_RED - 5)
			{
				sTempDist = sTempDist / 2;
			}

			// avoid moving too far from noise gridno
			if (PythSpacesAway(sGridNo, sPos) > MAX_FLANK_DIST_RED && pSoldier->aiBehavior().attitude() != CUNNINGSOLO)
			{
				continue;
			}

			// sevenfm: avoid moving too close to enemy vision range
			if (PythSpacesAway(sGridNo, sPos) < sDistanceVisible + 5)
			{
				sTempDist = sTempDist / 2;
			}

			// sevenfm: penalize locations with no sight cover from noise gridno (supposed that we are sneaking)
			if( PythSpacesAway( sGridNo, sPos) <= (INT16)MAX_VISION_RANGE &&
				LocationToLocationLineOfSightTest( sGridNo, pSoldier->position().level(), sPos, pSoldier->position().level(), TRUE, CALC_FROM_ALL_DIRS) )
			{
				//continue;
				sTempDist = sTempDist / 2;
			}

			// allow extra directions for flanking
			if ( bAction == AI_ACTION_FLANK_LEFT )
			{
				// sevenfm: allow two extra directions
				if ( sTempDir != sDesiredDir && sTempDir != gOneCDirection[sDesiredDir] && sTempDir != gOneCCDirection[sDesiredDir])
					continue;
				// prefer desired dir x1.5
				if( sTempDir == sDesiredDir )
					sTempDist = 3*sTempDist/2;
			}
			else if ( bAction == AI_ACTION_FLANK_RIGHT )
			{
				// sevenfm: allow two extra directions
				if ( sTempDir != sDesiredDir && sTempDir != gOneCDirection[sDesiredDir] && sTempDir != gOneCCDirection[sDesiredDir])
					continue;
				// prefer desired dir x1.5
				if( sTempDir == sDesiredDir )
					sTempDist = 3*sTempDist/2;
			}
			else
			{
				if ( sTempDir != sDesiredDir )
					continue;
			}

			// if this is better than the best place found so far
			if ( sTempDist > sBestDist )
			{
				// remember it instead
				sBestDist = sTempDist;
				sBestSpot = sGridNo;
			}
		}

	}

	DebugMsg ( TOPIC_JA2AI , DBG_LEVEL_3 , String("FindFlankingSpot: return grid no %d", sBestSpot ));

	gubNPCAPBudget = 0;
	gubNPCDistLimit = 0;

	return( sBestSpot );
}

//sevenfm: new calculation using FindHeigherLevel
INT32 FindClosestClimbPoint (TacticalActor *pSoldier, BOOLEAN fClimbUp )
{
	INT32 sBestSpot = NOWHERE;

	// sevenfm: safety check
	if(!pSoldier)
	{
		return NOWHERE;
	}

	//DebugMsg( TOPIC_JA2AI , DBG_LEVEL_3 , "FindClosestClimbPoint");

	if (fClimbUp)
	{
		// For the climb up case, search the nearby limits for a climb up point and take the closest.
		INT32 sGridNo;
		static const INT32 iSearchRange = 20;
		INT16	sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;
		//UINT8 ubTestDir;

		INT8 ubClimbDir;
		INT32 sClimbSpot;

		// determine maximum horizontal limits
		sMaxLeft  = min( iSearchRange, (pSoldier->position().gridNo() % MAXCOL));
		//NumMessage("sMaxLeft = ",sMaxLeft);
		sMaxRight = min( iSearchRange, MAXCOL - ((pSoldier->position().gridNo() % MAXCOL) + 1));
		//NumMessage("sMaxRight = ",sMaxRight);

		// determine maximum vertical limits
		sMaxUp   = min( iSearchRange, (pSoldier->position().gridNo() / MAXROW));
		//NumMessage("sMaxUp = ",sMaxUp);
		sMaxDown = min( iSearchRange, MAXROW - ((pSoldier->position().gridNo() / MAXROW) + 1));
		//NumMessage("sMaxDown = ",sMaxDown);

		//DebugMsg( TOPIC_JA2AI , DBG_LEVEL_3 , String("Max: Left %d Right %d Up %d Down %d", sMaxLeft, sMaxRight, sMaxUp, sMaxDown ) );

		for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
		{
			for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
			{
				// calculate the next potential gridno
				sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
				//DebugMsg( TOPIC_JA2AI , DBG_LEVEL_3 , String("Checking grid %d" , sGridNo ));

				//NumMessage("Testing gridno #",gridno);
				if ( !(sGridNo >=0 && sGridNo < WORLD_MAX) )
				{
					continue;
				}

				if ( sGridNo == pSoldier->pathing().blackListGrid() )
				{
					continue;
				}			

				// exclude locations with tear/mustard gas (at this point, smoke is cool!)
				if ( InGas( pSoldier, sGridNo ) )
				{
					continue;
				}

				// exclude water tiles
				if ( Water( sGridNo, pSoldier->position().level() ) )
				{
					continue;
				}

				// check that there's noone at sGridNo at level 0 (this soldier is allowed)
				if( WhoIsThere2( sGridNo, 0 ) != NOBODY &&
					WhoIsThere2( sGridNo, 0 ) != pSoldier->identity().id() )
				{
					continue;
				}

				if( FindHeigherLevel( pSoldier, sGridNo, pSoldier->position().direction(), &ubClimbDir ) )
				{
					// ubClimbDir is new direction
					// check that there's noone there
					sClimbSpot = NewGridNo( sGridNo, DirectionInc( ubClimbDir));
					if( WhoIsThere2( sClimbSpot, 1 ) == NOBODY )
					{
						// Good climb point.  Is it closer than the previous point?
						if( TileIsOutOfBounds(sBestSpot) ||
							GetRangeInCellCoordsFromGridNoDiff( pSoldier->position().gridNo() , sGridNo ) < GetRangeInCellCoordsFromGridNoDiff( pSoldier->position().gridNo() , sBestSpot ))
						{
							// If not, we have a new winnar!
							sBestSpot = sGridNo;
						}
					}
				}
			}
		}
	}
	else
	{
		// Climbing down is easier.  Just find the nearest climb point ;)
		sBestSpot = FindClosestClimbPoint( pSoldier, pSoldier->position().gridNo(), pSoldier->position().gridNo(), fClimbUp);
	}

	//	DebugMsg( TOPIC_JA2AI , DBG_LEVEL_3 , String("FindClosestClimbPoint Returning %d", sBestSpot ));
	return( sBestSpot );
}



BOOLEAN CanClimbFromHere (TacticalActor * pSoldier, BOOLEAN fUp )
{
	return FindDirectionForClimbing( pSoldier, pSoldier->position().gridNo(), pSoldier->position().level()) != DIRECTION_IRRELEVANT;
}
			// OK, this place shows potential.	How useful is it as cover?
			//NumMessage("Promising seems gridno #",gridno);

/*			if ( FindBuilding ( sGridNo ) != NULL )
				pBuilding = FindBuilding ( sGridNo );
		}
	}

	DebugMsg( TOPIC_JA2AI , DBG_LEVEL_3 , String("Adjacent Building climb spots = %d" , pBuilding->ubNumClimbSpots ));
	if ( pBuilding != NULL)
	{
		if ( fUp )
		{
			for (i = 0 ; i < pBuilding->ubNumClimbSpots; i++)
			{
				if (pBuilding->sUpClimbSpots[ i ] == pSoldier->sGridNo &&
					(WhoIsThere2( pBuilding->sUpClimbSpots[ i ], 0 ) == NOBODY)
					&& (WhoIsThere2( pBuilding->sDownClimbSpots[ i ], 1 ) == NOBODY) )
					return TRUE;
			}
		}
		else
		{
			for (i = 0 ; i < pBuilding->ubNumClimbSpots; i++)
			{
				if (pBuilding->sDownClimbSpots[ i ] == pSoldier->sGridNo &&
					(WhoIsThere2( pBuilding->sUpClimbSpots[ i ], 0 ) == NOBODY)
					&& (WhoIsThere2( pBuilding->sDownClimbSpots[ i ], 1 ) == NOBODY) )
					return TRUE;
			}
		}
	}
	return FALSE;
}
*/
extern BUILDING gBuildings[ MAX_BUILDINGS ];
extern UINT8 gubNumberOfBuildings;


INT32 FindBestCoverNearTheGridNo(TacticalActor *pSoldier, INT32 sGridNo, UINT8 ubSearchRadius )
{
	INT32 iPercentBetter;
//	INT16 sTrueGridNo;
	INT16 sResultGridNo;
	INT8 bRealWisdom = pSoldier->statistics().wisdom();

//	sTrueGridNo = pSoldier->sGridNo;
//	pSoldier->sGridNo = sGridNo;
	pSoldier->statistics().wisdom() = 8 * ubSearchRadius;// 5 tiles

	sResultGridNo = FindBestNearbyCover( pSoldier, MORALE_NORMAL, &iPercentBetter);

	pSoldier->statistics().wisdom() = bRealWisdom;
//	pSoldier->sGridNo = sTrueGridNo;
	
	if(!TileIsOutOfBounds(sResultGridNo))
		return sResultGridNo;
	else
		return sGridNo;

}

// sevenfm: new calculation using FindHeigherLevel/FindLowerLevel
INT8 FindDirectionForClimbing( TacticalActor *pSoldier, INT32 sGridNo, INT8 bLevel )
{
	INT8 ubClimbDir;
	INT32 sClimbSpot;

	if(!pSoldier)
	{
		return DIRECTION_IRRELEVANT;
	}

	if (pSoldier->position().level() == 0)
	{
		// check climb up
		if( FindHeigherLevel( pSoldier, sGridNo, pSoldier->position().direction(), &ubClimbDir ) )
		{
			// ubClimbDir is new direction
			// check that there's noone there
			sClimbSpot = NewGridNo( sGridNo, DirectionInc( ubClimbDir));
			if( WhoIsThere2( sClimbSpot, 1 ) == NOBODY &&
				!Water(sClimbSpot, 1) )
			{
				return ubClimbDir;
			}
		}
	}
	else
	{
		// check climb down
		if( FindLowerLevel( pSoldier, pSoldier->position().gridNo(), pSoldier->position().direction(), &ubClimbDir ) )
		{
			// ubClimbDir is new direction
			sClimbSpot = NewGridNo( sGridNo, DirectionInc( ubClimbDir));
			if( WhoIsThere2( sClimbSpot, 0 ) == NOBODY &&
				!Water(sClimbSpot, 0) )
			{
				return ubClimbDir;
			}
		}
	}

	return DIRECTION_IRRELEVANT;
}

INT32 FindNearestPassableSpot( INT32 sGridNo, UINT8 usSearchRadius )
{
	INT32	sNearestSpot = NOWHERE;
	INT16	sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;
	INT32	sTestGridNo;
	INT32	iDist, iClosestDist = 10;

	// determine maximum horizontal limits
	sMaxLeft  = min( usSearchRadius, (sGridNo % MAXCOL));
	sMaxRight = min( usSearchRadius, MAXCOL - ((sGridNo % MAXCOL) + 1));

	// determine maximum vertical limits
	sMaxUp   = min( usSearchRadius, (sGridNo / MAXROW));
	sMaxDown = min( usSearchRadius, MAXROW - ((sGridNo / MAXROW) + 1));

	// SET UP DOUBLE-LOOP TO STEP THROUGH POTENTIAL GRID #s
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			// calculate the next potential gridno
			sTestGridNo = sGridNo + sXOffset + (MAXCOL * sYOffset);
			// is this an empty tile (no structure on it) or is the structure passable?
			if (gpWorldLevelData[sTestGridNo].pStructureHead == NULL || FindStructure( sTestGridNo, STRUCTURE_PASSABLE ) != NULL)
			{
				iDist = PythSpacesAway( sGridNo, sTestGridNo );
				if (iDist < iClosestDist)
				{
					iClosestDist = iDist;
					sNearestSpot = sTestGridNo;
				}
			}
		}
	}

	return( sNearestSpot );
}

INT32 FindAdvanceSpot(TacticalActor *pSoldier, INT32 sTargetSpot, INT8 bAction, UINT8 ubType, BOOLEAN fUnlimited)
{
	INT32	sGridNo, sRealGridNo;
	INT32	sBestSpot = NOWHERE;
	INT32	iSearchRange = min(AI_PATHCOST_RADIUS, MAX_TILES_MOVE_TURN);
	INT16	sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;
	INT32	iPathCost, iBestPathCost = 0;
	INT16	usMovementMode;
	INT32	iRoamRange, iDistFromOrigin, sOrigin;
	//BOOLEAN	fClimbingNecessary;
	//INT32	sClimbGridNo;
	BOOLEAN fHasMortar;

	if (!pSoldier)
	{
		return NOWHERE;
	}

	// check target location
	if (!NewOKDestination(pSoldier, sTargetSpot, FALSE, pSoldier->position().level()))
	{
		//ScreenMsg(FONT_ORANGE, MSG_INTERFACE, L"bad destination %d", sTargetSpot);
		return NOWHERE;
	}

	fHasMortar = AICheckIsMortarOperator(pSoldier);

	usMovementMode = DetermineMovementMode(pSoldier, bAction);

	UINT8 ubReserveAP = 0;

	if (ubType != ADVANCE_SPOT_SIGHT_COVER)
	{
		if (usMovementMode == RUNNING || usMovementMode == WALKING)
		{
			ubReserveAP = APBPConstants[AP_CHANGE_FACING] + GetAPsCrouch(pSoldier, TRUE) + GetAPsProne(pSoldier, TRUE);
		}
		else
		{
			ubReserveAP = APBPConstants[AP_LOOK_CROUCHED] + GetAPsProne(pSoldier, TRUE);
		}
	}

	if (pSoldier->actionPoints().current() <= ubReserveAP)
	{
		return NOWHERE;
	}

	// check that location is reachable
	iBestPathCost = EstimatePlotPath(pSoldier, sTargetSpot, FALSE, FALSE, FALSE, usMovementMode, pSoldier->movement().stealthMode(), FALSE, 0);
	if (iBestPathCost == 0)
	{
		return NOWHERE;
	}

	iRoamRange = RoamingRange(pSoldier, &sOrigin);

	// set AP limit
	gubNPCAPBudget = pSoldier->actionPoints().current() - ubReserveAP;
	// set the distance limit of the square region
	gubNPCDistLimit = (UINT8)iSearchRange;

	// determine maximum horizontal limits
	sMaxLeft = min(iSearchRange, (pSoldier->position().gridNo() % MAXCOL));
	//NumMessage("sMaxLeft = ",sMaxLeft);
	sMaxRight = min(iSearchRange, MAXCOL - ((pSoldier->position().gridNo() % MAXCOL) + 1));
	//NumMessage("sMaxRight = ",sMaxRight);

	// determine maximum vertical limits
	sMaxUp = min(iSearchRange, (pSoldier->position().gridNo() / MAXROW));
	//NumMessage("sMaxUp = ",sMaxUp);
	sMaxDown = min(iSearchRange, MAXROW - ((pSoldier->position().gridNo() / MAXROW) + 1));
	//NumMessage("sMaxDown = ",sMaxDown);

	// Call FindBestPath to set flags in all locations that we can
	// walk into within range.	We have to set some things up first...

	// reset the "reachable" flags in the region we're looking at
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			if (!(sGridNo >= 0 && sGridNo < WORLD_MAX))
			{
				continue;
			}

			gpWorldLevelData[sGridNo].uiFlags &= ~(MAPELEMENT_REACHABLE);
		}
	}

	FindBestPath(pSoldier, GRIDSIZE, pSoldier->position().level(), usMovementMode, COPYREACHABLE, 0);

	// Turn off the "reachable" flag for his current location
	// so we don't consider it
	gpWorldLevelData[pSoldier->position().gridNo()].uiFlags &= ~(MAPELEMENT_REACHABLE);

	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			// calculate the next potential gridno
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);

			if (!(sGridNo >= 0 && sGridNo < WORLD_MAX))
			{
				continue;
			}

			if (!(gpWorldLevelData[sGridNo].uiFlags & MAPELEMENT_REACHABLE))
			{
				continue;
			}

			if (sGridNo == pSoldier->pathing().blackListGrid())
			{
				continue;
			}

			// check roaming range
			if (iRoamRange < MAX_ROAMING_RANGE && !TileIsOutOfBounds(sOrigin))
			{
				iDistFromOrigin = SpacesAway(sOrigin, sGridNo);
				if (iDistFromOrigin > iRoamRange)
				{
					continue;
				}
			}

			// check if we will be closer to target spot
			if (PythSpacesAway(sGridNo, sTargetSpot) > PythSpacesAway(pSoldier->position().gridNo(), sTargetSpot))
			{
				continue;
			}

			// exclude locations with tear/mustard gas (at this point, smoke is cool!)
			if (InGas(pSoldier, sGridNo))
			{
				continue;
			}

			// skip lighted spots
			if (InLightAtNight(sGridNo, pSoldier->position().level()) &&
				!InLightAtNight(pSoldier->position().gridNo(), pSoldier->position().level()))
				continue;

			// skip deep water
			if (DeepWater(sGridNo, pSoldier->position().level()))
			{
				continue;
			}

			// skip water
			if (Water(sGridNo, pSoldier->position().level()))
			{
				continue;
			}

			// skip locations too close to target spot
			if (PythSpacesAway(sGridNo, sTargetSpot) < (INT16)(TACTICAL_RANGE / 4))
			{
				continue;
			}

			// avoid smoke
			if (InSmoke(sGridNo, pSoldier->position().level()))
			{
				continue;
			}

			// skip if this spot has no cover nearby
			if ((ubType == ADVANCE_SPOT_ANY_COVER || fUnlimited) &&
				!FindObstacleNearSpot(sGridNo, pSoldier->position().level()))
			{
				continue;
			}

			// avoid locations near fresh corpses
			if (!TacticalActorConditions::isZombie(*pSoldier) && GetNearestRottingCorpseAIWarning(sGridNo) > 0)
				//if (CorpseWarning(pSoldier, sGridNo, pSoldier->position().level(), TRUE))
			{
				continue;
			}

			// avoid overcrowding
			if (!TacticalActorConditions::isZombie(*pSoldier) && NumberOfTeamMatesAdjacent(pSoldier, sGridNo) > 1)
			{
				continue;
			}

			if (!CheckNPCDestination(pSoldier, sGridNo))
			{
				continue;
			}

			if (!LegalNPCDestination(pSoldier, sGridNo, IGNORE_PATH, NOWATER, 0))
			{
				continue;		// skip on to the next potential grid
			}

			// sevenfm: avoid rooms for mortar operators
			if (!AICheckUnderground() &&
				pSoldier->position().level() == 0 &&
				fHasMortar &&
				InARoom(sGridNo, NULL))
			{
				continue;
			}

			if (ubType == ADVANCE_SPOT_SIGHT_COVER)
			{
				// skip locations with no sight cover
				if (!SightCoverAtSpot(pSoldier, sGridNo, fUnlimited))
				{
					continue;
				}
			}
			else if (ubType == ADVANCE_SPOT_PRONE_COVER)
			{
				// check prone sight cover at spot
				if (!ProneSightCoverAtSpot(pSoldier, sGridNo, fUnlimited))
				{
					continue;
				}
				// check that we'll have enough APs to go prone at target spot
				/*iPathCost = EstimatePlotPath( pSoldier, sGridNo, FALSE, FALSE, FALSE, usMovementMode, pSoldier->movement().stealthMode(), FALSE, 0);
				if( pSoldier->actionPoints().current() - iPathCost < GetAPsCrouch(pSoldier, TRUE) + GetAPsProne(pSoldier, TRUE) + APBPConstants[AP_CHANGE_FACING] )
				{
				continue;
				}*/
			}
			else if (ubType == ADVANCE_SPOT_ANY_COVER)
			{
				// check any cover at spot
				if (!AnyCoverAtSpot(pSoldier, sGridNo))
				{
					continue;
				}
				// check that we'll have enough APs to go prone at target spot
				/*iPathCost = EstimatePlotPath( pSoldier, sGridNo, FALSE, FALSE, FALSE, usMovementMode, pSoldier->movement().stealthMode(), FALSE, 0);
				if( pSoldier->actionPoints().current() - iPathCost < GetAPsCrouch(pSoldier, TRUE) + GetAPsProne(pSoldier, TRUE) + APBPConstants[AP_CHANGE_FACING] )
				{
				continue;
				}*/
			}

			sRealGridNo = pSoldier->position().gridNo();
			pSoldier->position().gridNo() = sGridNo;
			iPathCost = EstimatePlotPath(pSoldier, sTargetSpot, FALSE, FALSE, FALSE, usMovementMode, pSoldier->movement().stealthMode(), FALSE, 0);
			//iPathCost = PlotPath(pSoldier, sTargetSpot, FALSE, FALSE, FALSE, usMovementMode, pSoldier->movement().stealthMode(), FALSE, 0);
			//iPathCost = EstimatePathCostToLocation( pSoldier, sTargetSpot, pSoldier->position().level(), TRUE, &fClimbingNecessary, &sClimbGridNo );
			pSoldier->position().gridNo() = sRealGridNo;

			// skip location if no path to target spot
			if (iPathCost == 0)
			{
				//ScreenMsg(FONT_ORANGE, MSG_INTERFACE, L"cannot find path to destination %d at %d", sTargetSpot, sGridNo);
				continue;
			}

			//if( sBestSpot == NOWHERE || iPathCost < iBestPathCost )
			if (iPathCost < iBestPathCost)
			{
				sBestSpot = sGridNo;
				iBestPathCost = iPathCost;
			}
		}
	}

	gubNPCAPBudget = 0;
	gubNPCDistLimit = 0;

	return(sBestSpot);
}

// find spot with cover, max dist from opponents
INT32 FindRetreatSpot(TacticalActor *pSoldier)
{
	INT32	sGridNo;
	INT32	sBestSpot = NOWHERE;
	INT32	iSearchRange = MAX_TILES_MOVE_TURN;
	INT16	sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;
	INT16	usMovementMode;
	INT32	iRoamRange, iDistFromOrigin, sOrigin;
	//BOOLEAN	fClimbingNecessary;
	//INT32	sClimbGridNo;
	UINT8	ubReserveAP;
	INT8	bAction = AI_ACTION_TAKE_COVER;
	INT32	sClosestOpponent;
	INT16	sDistance, sClosestDistance;

	if (!pSoldier)
	{
		return NOWHERE;
	}

	sClosestOpponent = ClosestKnownOpponent(pSoldier, NULL, NULL);
	if (TileIsOutOfBounds(sClosestOpponent))
	{
		// no one to hide from
		return NOWHERE;
	}

	sClosestDistance = PythSpacesAway(pSoldier->position().gridNo(), sClosestOpponent);

	//ubReserveAP = GetAPsCrouch(pSoldier, TRUE) + GetAPsProne(pSoldier, TRUE) + APBPConstants[AP_CHANGE_FACING];
	ubReserveAP = GetAPsCrouch(pSoldier, TRUE) + APBPConstants[AP_CHANGE_FACING];

	if (pSoldier->actionPoints().current() <= ubReserveAP)
	{
		return NOWHERE;
	}

	iRoamRange = RoamingRange(pSoldier, &sOrigin);

	// set AP limit
	gubNPCAPBudget = pSoldier->actionPoints().current();
	// set the distance limit of the square region
	gubNPCDistLimit = (UINT8)iSearchRange;

	// determine maximum horizontal limits
	sMaxLeft = min(iSearchRange, (pSoldier->position().gridNo() % MAXCOL));
	//NumMessage("sMaxLeft = ",sMaxLeft);
	sMaxRight = min(iSearchRange, MAXCOL - ((pSoldier->position().gridNo() % MAXCOL) + 1));
	//NumMessage("sMaxRight = ",sMaxRight);

	// determine maximum vertical limits
	sMaxUp = min(iSearchRange, (pSoldier->position().gridNo() / MAXROW));
	//NumMessage("sMaxUp = ",sMaxUp);
	sMaxDown = min(iSearchRange, MAXROW - ((pSoldier->position().gridNo() / MAXROW) + 1));
	//NumMessage("sMaxDown = ",sMaxDown);

	// Call FindBestPath to set flags in all locations that we can
	// walk into within range.	We have to set some things up first...

	// reset the "reachable" flags in the region we're looking at
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);
			if (!(sGridNo >= 0 && sGridNo < WORLD_MAX))
			{
				continue;
			}

			gpWorldLevelData[sGridNo].uiFlags &= ~(MAPELEMENT_REACHABLE);
		}
	}

	usMovementMode = DetermineMovementMode(pSoldier, bAction);
	FindBestPath(pSoldier, GRIDSIZE, pSoldier->position().level(), usMovementMode, COPYREACHABLE, 0);

	// Turn off the "reachable" flag for his current location
	// so we don't consider it
	gpWorldLevelData[pSoldier->position().gridNo()].uiFlags &= ~(MAPELEMENT_REACHABLE);

	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			// calculate the next potential gridno
			sGridNo = pSoldier->position().gridNo() + sXOffset + (MAXCOL * sYOffset);

			if (!(sGridNo >= 0 && sGridNo < WORLD_MAX))
			{
				continue;
			}

			if (!(gpWorldLevelData[sGridNo].uiFlags & MAPELEMENT_REACHABLE))
			{
				continue;
			}

			if (sGridNo == pSoldier->pathing().blackListGrid())
			{
				continue;
			}

			// check roaming range
			if (iRoamRange < MAX_ROAMING_RANGE && !TileIsOutOfBounds(sOrigin))
			{
				iDistFromOrigin = SpacesAway(sOrigin, sGridNo);
				if (iDistFromOrigin > iRoamRange)
				{
					continue;
				}
			}

			// exclude locations with tear/mustard gas (at this point, smoke is cool!)
			if (InGas(pSoldier, sGridNo))
			{
				continue;
			}

			// skip lighted spots
			if (InLightAtNight(sGridNo, pSoldier->position().level()) &&
				!InLightAtNight(pSoldier->position().gridNo(), pSoldier->position().level()))
				continue;

			// skip deep water
			if (DeepWater(sGridNo, pSoldier->position().level()))
			{
				continue;
			}

			// skip water
			if (Water(sGridNo, pSoldier->position().level()))
			{
				continue;
			}

			// avoid locations near fresh corpses
			if (!TacticalActorConditions::isZombie(*pSoldier) && GetNearestRottingCorpseAIWarning(sGridNo) > 0)
				//if (CorpseWarning(pSoldier, sGridNo, pSoldier->position().level(), TRUE))
			{
				continue;
			}

			if (!TacticalActorConditions::isZombie(*pSoldier) && NumberOfTeamMatesAdjacent(pSoldier, sGridNo) > 0)
			{
				continue;
			}

			if (!CheckNPCDestination(pSoldier, sGridNo))
			{
				continue;
			}

			// avoid smoke
			/*if( InSmoke(sGridNo, pSoldier->position().level(), FALSE) )
			{
			continue;
			}*/

			if (!LegalNPCDestination(pSoldier, sGridNo, IGNORE_PATH, NOWATER, 0))
			{
				continue;		// skip on to the next potential grid
			}

			// skip if too close to enemy
			//sClosestOpponent = ClosestKnownOpponent(pSoldier, NULL, NULL);
			sDistance = PythSpacesAway(sGridNo, sClosestOpponent);
			if (sDistance < (INT16)(TACTICAL_RANGE / 2))
			{
				continue;
			}

			// skip if this spot has no cover nearby
			if (!FindObstacleNearSpot(sGridNo, pSoldier->position().level()))
			{
				continue;
			}

			if (!AnyCoverAtSpot(pSoldier, sGridNo))
			{
				continue;
			}

			// check prone sight cover at spot
			/*if( !ProneSightCoverAtSpot(pSoldier, sGridNo) )
			{
			continue;
			}*/

			if (sDistance > sClosestDistance)
			{
				sBestSpot = sGridNo;
				sClosestDistance = sDistance;
			}
		}
	}

	gubNPCAPBudget = 0;
	gubNPCDistLimit = 0;

	return(sBestSpot);
}
