#include "worlddef.h"
#include "Timer Control.h"
#include "Tile Animation.h"
#include "DEBUG.H"
#include "worldman.h"
#include "lighting.h"
#include "renderworld.h"
#include "Overhead.h"
#include "ai.h"
#include "Sound Control.h"
#include "Tile Cache.h"
#include "Explosion Control.h"
#include "Keys.h"
#include "Bullets.h"
#include "LightEffects.h"
#include "SmokeEffects.h"

#include <climits>
#include <cstring>


namespace
{
INT32 gAniTileAllocationCountdown = -1;
BOOLEAN gFailAfterLevelNodeInsertion = FALSE;
constexpr UINT32 ANIMATION_OWNED_LEVELNODE_FLAGS =
	LEVELNODE_ANIMATION | LEVELNODE_USEZ | LEVELNODE_DYNAMIC |
	LEVELNODE_NOZBLITTER | LEVELNODE_REVEAL | LEVELNODE_USEBESTTRANSTYPE |
	LEVELNODE_DYNAMICZ | LEVELNODE_LASTDYNAMIC |
	LEVELNODE_UPDATESAVEBUFFERONCE | LEVELNODE_NOWRITEZ;

void *AllocateAniTileMemory( UINT32 size )
{
	if ( gAniTileAllocationCountdown == 0 )
		return NULL;
	if ( gAniTileAllocationCountdown > 0 )
		--gAniTileAllocationCountdown;
	return MemAlloc( size );
}

BOOLEAN IsValidAnimationLevel( UINT8 ubLevel )
{
	switch ( ubLevel )
	{
	case ANI_STRUCT_LEVEL:
	case ANI_SHADOW_LEVEL:
	case ANI_OBJECT_LEVEL:
	case ANI_ROOF_LEVEL:
	case ANI_ONROOF_LEVEL:
	case ANI_TOPMOST_LEVEL:
		return TRUE;
	default:
		return FALSE;
	}
}

BOOLEAN IsValidTileAnimation( UINT16 usTileIndex, UINT16 *pusNumFrames )
{
	if ( usTileIndex >= giNumberOfTiles )
		return FALSE;
	const TILE_ANIMATION_DATA *pAnimation = gTileDatabase[ usTileIndex ].pAnimData;
	if ( pAnimation == NULL || pAnimation->pusFrames == NULL ||
		pAnimation->ubNumFrames == 0 )
		return FALSE;
	*pusNumFrames = pAnimation->ubNumFrames;
	return TRUE;
}

BOOLEAN CalculateStartFrame( const ANITILE_PARAMS *pAniParams,
	UINT16 usNumFrames, INT16 *psStartFrame )
{
	const UINT32 uiDirectionFlags = pAniParams->uiFlags &
		( ANITILE_USE_DIRECTION_FOR_START_FRAME |
		ANITILE_USE_4DIRECTION_FOR_START_FRAME );
	if ( uiDirectionFlags ==
		( ANITILE_USE_DIRECTION_FOR_START_FRAME |
		ANITILE_USE_4DIRECTION_FOR_START_FRAME ) )
		return FALSE;
	if ( pAniParams->sStartFrame < 0 ||
		static_cast<UINT16>( pAniParams->sStartFrame ) >= usNumFrames )
		return FALSE;

	UINT32 uiStartFrame = static_cast<UINT16>( pAniParams->sStartFrame );
	if ( uiDirectionFlags != 0 )
	{
		if ( pAniParams->uiUserData3 >= NUM_WORLD_DIRECTIONS )
			return FALSE;
		const UINT8 ubDirection =
			( uiDirectionFlags & ANITILE_USE_DIRECTION_FOR_START_FRAME )
			? gOneCDirection[ pAniParams->uiUserData3 ]
			: gb4DirectionsFrom8[ pAniParams->uiUserData3 ];
		uiStartFrame += static_cast<UINT32>( usNumFrames ) * ubDirection;
	}
	if ( uiStartFrame > INT16_MAX )
		return FALSE;
	*psStartFrame = static_cast<INT16>( uiStartFrame );
	return TRUE;
}

LEVELNODE *AddAnimationLevelNode( UINT8 ubLevel, INT32 sGridNo,
	UINT16 usTileIndex )
{
	switch ( ubLevel )
	{
	case ANI_STRUCT_LEVEL:
		return ForceStructToTail( sGridNo, usTileIndex );
	case ANI_SHADOW_LEVEL:
		return AddShadowToHead( sGridNo, usTileIndex )
			? gpWorldLevelData[ sGridNo ].pShadowHead : NULL;
	case ANI_OBJECT_LEVEL:
		return AddObjectToHead( sGridNo, usTileIndex )
			? gpWorldLevelData[ sGridNo ].pObjectHead : NULL;
	case ANI_ROOF_LEVEL:
		return AddRoofToHead( sGridNo, usTileIndex )
			? gpWorldLevelData[ sGridNo ].pRoofHead : NULL;
	case ANI_ONROOF_LEVEL:
		return AddOnRoofToHead( sGridNo, usTileIndex )
			? gpWorldLevelData[ sGridNo ].pOnRoofHead : NULL;
	case ANI_TOPMOST_LEVEL:
		return AddTopmostToHead( sGridNo, usTileIndex )
			? gpWorldLevelData[ sGridNo ].pTopmostHead : NULL;
	default:
		return NULL;
	}
}

BOOLEAN RemoveAnimationLevelNode( UINT8 ubLevel, INT32 sGridNo,
	LEVELNODE *pNode )
{
	switch ( ubLevel )
	{
	case ANI_STRUCT_LEVEL:
		return RemoveStructFromLevelNode( sGridNo, pNode );
	case ANI_SHADOW_LEVEL:
		return RemoveShadowFromLevelNode( sGridNo, pNode );
	case ANI_OBJECT_LEVEL:
		return RemoveObjectFromLevelNode( sGridNo, pNode );
	case ANI_ROOF_LEVEL:
		return RemoveRoofFromLevelNode( sGridNo, pNode );
	case ANI_ONROOF_LEVEL:
		return RemoveOnRoofFromLevelNode( sGridNo, pNode );
	case ANI_TOPMOST_LEVEL:
		return RemoveTopmostFromLevelNode( sGridNo, pNode );
	default:
		return FALSE;
	}
}

void ResetAnimationLayerOptimizing( UINT8 ubLevel )
{
	switch ( ubLevel )
	{
	case ANI_STRUCT_LEVEL:
		ResetSpecificLayerOptimizing( TILES_DYNAMIC_STRUCTURES );
		break;
	case ANI_SHADOW_LEVEL:
		ResetSpecificLayerOptimizing( TILES_DYNAMIC_SHADOWS );
		break;
	case ANI_OBJECT_LEVEL:
		ResetSpecificLayerOptimizing( TILES_DYNAMIC_OBJECTS );
		break;
	case ANI_ROOF_LEVEL:
		ResetSpecificLayerOptimizing( TILES_DYNAMIC_ROOF );
		break;
	case ANI_ONROOF_LEVEL:
		ResetSpecificLayerOptimizing( TILES_DYNAMIC_ONROOF );
		break;
	case ANI_TOPMOST_LEVEL:
		ResetSpecificLayerOptimizing( TILES_DYNAMIC_TOPMOST );
		break;
	}
}

constexpr UINT32 gAniTileFlags[] = {
	ANITILE_DOOR, ANITILE_PAUSE_AFTER_LOOP, ANITILE_BACKWARD,
	ANITILE_FORWARD, ANITILE_PAUSED, ANITILE_EXISTINGTILE,
	ANITILE_USEABSOLUTEPOS, ANITILE_CACHEDTILE, ANITILE_LOOPING,
	ANITILE_NOZBLITTER, ANITILE_REVERSE_LOOPING,
	ANITILE_ALWAYS_TRANSLUCENT, ANITILE_USEBEST_TRANSLUCENT,
	ANITILE_OPTIMIZEFORSLOWMOVING, ANITILE_ANIMATE_Z,
	ANITILE_USE_DIRECTION_FOR_START_FRAME,
	ANITILE_USE_4DIRECTION_FOR_START_FRAME,
	ANITILE_ERASEITEMFROMSAVEBUFFFER, ANITILE_OPTIMIZEFORSMOKEEFFECT,
	ANITILE_SMOKE_EFFECT, ANITILE_EXPLOSION,
	ANITILE_RELEASE_ATTACKER_WHEN_DONE, ANITILE_LIGHT
};

constexpr bool AnimationFlagsAreUnique()
{
	for ( unsigned i = 0; i < sizeof( gAniTileFlags ) / sizeof( gAniTileFlags[ 0 ] ); ++i )
	{
		for ( unsigned j = i + 1; j < sizeof( gAniTileFlags ) / sizeof( gAniTileFlags[ 0 ] ); ++j )
		{
			if ( gAniTileFlags[ i ] == gAniTileFlags[ j ] )
				return false;
		}
	}
	return true;
}

static_assert( AnimationFlagsAreUnique(), "animation tile flags must be unique" );
}

namespace AniTileTestHooks
{
void FailAllocationAfter( INT32 successfulAllocations )
{
	gAniTileAllocationCountdown = successfulAllocations;
}

void FailAfterLevelNodeInsertion()
{
	gFailAfterLevelNodeInsertion = TRUE;
}

void ResetFailures()
{
	gAniTileAllocationCountdown = -1;
	gFailAfterLevelNodeInsertion = FALSE;
}
}


ANITILE *pAniTileHead = NULL;


ANITILE *CreateAnimationTile( ANITILE_PARAMS *pAniParams )
{
	if ( pAniParams == NULL || !IsValidAnimationLevel( pAniParams->ubLevelID ) ||
		pAniParams->sGridNo < 0 || pAniParams->sGridNo >= WORLD_MAX ||
		pAniParams->sDelay < 0 )
		return NULL;

	const UINT32 uiFlags = pAniParams->uiFlags;
	const BOOLEAN fExistingTile = ( uiFlags & ANITILE_EXISTINGTILE ) != 0;
	const BOOLEAN fCachedTile = ( uiFlags & ANITILE_CACHEDTILE ) != 0;
	if ( ( fExistingTile && fCachedTile ) ||
		( fExistingTile && ( pAniParams->pGivenLevelNode == NULL ||
			pAniParams->pGivenLevelNode->pAniTile != NULL ) ) ||
		( !fExistingTile && gpWorldLevelData == NULL ) )
		return NULL;
	if ( fCachedTile && ( pAniParams->zCachedFile[ 0 ] == '\0' ||
		std::memchr( pAniParams->zCachedFile, '\0',
			sizeof( pAniParams->zCachedFile ) ) == NULL ) )
		return NULL;

	UINT16 usTileIndex = pAniParams->usTileIndex;
	UINT16 usNumFrames = 0;
	if ( !fCachedTile &&
		!IsValidTileAnimation( usTileIndex, &usNumFrames ) )
		return NULL;

	ANITILE *pNewAniNode = static_cast<ANITILE *>(
		AllocateAniTileMemory( sizeof( ANITILE ) ) );
	if ( pNewAniNode == NULL )
		return NULL;
	memset( pNewAniNode, 0, sizeof( ANITILE ) );
	pNewAniNode->lightSprite = -1;

	INT32 iCachedTile = -1;
	if ( fCachedTile )
	{
		iCachedTile = GetCachedTile( pAniParams->zCachedFile );
		if ( iCachedTile < 0 )
		{
			MemFree( pNewAniNode );
			return NULL;
		}
		const UINT32 uiCachedTileIndex =
			static_cast<UINT32>( iCachedTile ) + TILE_CACHE_START_INDEX;
		if ( iCachedTile > INT16_MAX || uiCachedTileIndex > UINT16_MAX )
		{
			RemoveCachedTile( iCachedTile );
			MemFree( pNewAniNode );
			return NULL;
		}
		usNumFrames = GetCachedTileFrameCount( iCachedTile );
		usTileIndex = static_cast<UINT16>( uiCachedTileIndex );
	}

	INT16 sStartFrame = 0;
	if ( !CalculateStartFrame( pAniParams, usNumFrames, &sStartFrame ) )
	{
		if ( iCachedTile != -1 )
			RemoveCachedTile( iCachedTile );
		MemFree( pNewAniNode );
		return NULL;
	}

	LEVELNODE *pNode = pAniParams->pGivenLevelNode;
	if ( !fExistingTile )
	{
		pNode = AddAnimationLevelNode( pAniParams->ubLevelID,
			pAniParams->sGridNo, usTileIndex );
		if ( pNode == NULL )
		{
			if ( iCachedTile != -1 )
				RemoveCachedTile( iCachedTile );
			MemFree( pNewAniNode );
			return NULL;
		}

		if ( gFailAfterLevelNodeInsertion )
		{
			gFailAfterLevelNodeInsertion = FALSE;
			RemoveAnimationLevelNode( pAniParams->ubLevelID,
				pAniParams->sGridNo, pNode );
			if ( iCachedTile != -1 )
				RemoveCachedTile( iCachedTile );
			MemFree( pNewAniNode );
			return NULL;
		}

		pNode->ubShadeLevel = DEFAULT_SHADE_LEVEL;
		pNode->ubNaturalShadeLevel = DEFAULT_SHADE_LEVEL;
	}

	pNewAniNode->pLevelNode = pNode;
	if ( fExistingTile )
	{
		pNewAniNode->uiOriginalLevelNodeFlags =
			pNode->uiFlags & ANIMATION_OWNED_LEVELNODE_FLAGS;
		pNewAniNode->sOriginalLevelNodeFrame = pNode->sCurrentFrame;
	}
	pNewAniNode->ubLevelID = pAniParams->ubLevelID;
	pNewAniNode->usTileIndex = usTileIndex;
	pNewAniNode->usNumFrames = usNumFrames;
	pNewAniNode->usTileType = pAniParams->usTileType;
	pNewAniNode->pNext = pAniTileHead;
	pNewAniNode->uiFlags = uiFlags;
	pNewAniNode->sDelay = pAniParams->sDelay;
	pNewAniNode->sCurrentFrame = sStartFrame;
	pNewAniNode->sStartFrame = sStartFrame;
	pNewAniNode->uiTimeLastUpdate = GetJA2Clock();
	pNewAniNode->sGridNo = pAniParams->sGridNo;
	pNewAniNode->ubOwner = pAniParams->ubOwner;
	pNewAniNode->ubKeyFrame1 = pAniParams->ubKeyFrame1;
	pNewAniNode->uiKeyFrame1Code = pAniParams->uiKeyFrame1Code;
	pNewAniNode->ubKeyFrame2 = pAniParams->ubKeyFrame2;
	pNewAniNode->uiKeyFrame2Code = pAniParams->uiKeyFrame2Code;
	pNewAniNode->uiUserData = pAniParams->uiUserData;
	pNewAniNode->ubUserData2 = pAniParams->ubUserData2;
	pNewAniNode->uiUserData3 = pAniParams->uiUserData3;

	if ( !fExistingTile && ( uiFlags & ANITILE_LIGHT ) )
	{
		if ( !IsLightEffectAtTile( pAniParams->sGridNo ) &&
			( pNewAniNode->lightSprite =
				LightSpriteCreate( "L-R03.LHT", 0 ) ) != -1 )
		{
			LightSpritePower( pNewAniNode->lightSprite, TRUE );
			INT16 sXPos, sYPos;
			ConvertGridNoToCenterCellXY( pAniParams->sGridNo, &sXPos, &sYPos );
			LightSpritePosition( pNewAniNode->lightSprite,
				static_cast<INT16>( sXPos / CELL_X_SIZE ),
				static_cast<INT16>( sYPos / CELL_Y_SIZE ) );
		}
	}

	if ( fCachedTile )
	{
		pNode->uiFlags |= LEVELNODE_CACHEDANITILE;
		pNewAniNode->sCachedTileID = static_cast<INT16>( iCachedTile );
		pNewAniNode->usCachedTileSubIndex = pAniParams->usTileType;
		pNewAniNode->sRelativeX = pAniParams->sX;
		pNewAniNode->sRelativeY = pAniParams->sY;
		pNode->sRelativeZ = pAniParams->sZ;
	}
	else if ( !fExistingTile && ( uiFlags & ANITILE_USEABSOLUTEPOS ) )
	{
		pNode->sRelativeX = pAniParams->sX;
		pNode->sRelativeY = pAniParams->sY;
		pNode->sRelativeZ = pAniParams->sZ;
		pNode->uiFlags |= LEVELNODE_USEABSOLUTEPOS;
	}

	pNode->uiFlags |= LEVELNODE_ANIMATION | LEVELNODE_USEZ | LEVELNODE_DYNAMIC;
	if ( uiFlags & ANITILE_NOZBLITTER )
		pNode->uiFlags |= LEVELNODE_NOZBLITTER;
	if ( uiFlags & ANITILE_ALWAYS_TRANSLUCENT )
		pNode->uiFlags |= LEVELNODE_REVEAL;
	if ( uiFlags & ANITILE_USEBEST_TRANSLUCENT )
		pNode->uiFlags |= LEVELNODE_USEBESTTRANSTYPE;
	if ( uiFlags & ANITILE_ANIMATE_Z )
		pNode->uiFlags |= LEVELNODE_DYNAMICZ;
	if ( uiFlags & ANITILE_PAUSED )
	{
		pNode->uiFlags |= LEVELNODE_LASTDYNAMIC |
			LEVELNODE_UPDATESAVEBUFFERONCE;
		pNode->uiFlags &= ~LEVELNODE_DYNAMIC;
	}
	if ( uiFlags & ANITILE_OPTIMIZEFORSMOKEEFFECT )
		pNode->uiFlags |= LEVELNODE_NOWRITEZ;
	if ( fExistingTile || fCachedTile )
		pNode->pAniTile = pNewAniNode;

	ResetAnimationLayerOptimizing( pAniParams->ubLevelID );
	pAniTileHead = pNewAniNode;
	return pNewAniNode;
}

// Loop throug all ani tiles and remove...
void DeleteAniTiles( )
{
	ANITILE *pAniNode			= NULL;
	ANITILE *pNode				= NULL;

	// LOOP THROUGH EACH NODE
	// And call delete function...
	pAniNode = pAniTileHead;

	while( pAniNode != NULL )
	{
		pNode = pAniNode;
		pAniNode = pAniNode->pNext;

		DeleteAniTile( pNode );
	}
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("DeleteAniTiles done") );
}


void DeleteAniTile( ANITILE *pAniTile )
{
	ANITILE				*pAniNode				= NULL;
	ANITILE				*pOldAniNode		= NULL;
	TILE_ELEMENT	*TileElem;

	pAniNode = pAniTileHead;

	while( pAniNode!= NULL )
	{
		if ( pAniNode == pAniTile )
		{
			// OK, set links
			// Check for head or tail
			if ( pOldAniNode == NULL )
			{
				// It's the head
				pAniTileHead = pAniTile->pNext;
			}
			else
			{
				pOldAniNode->pNext = pAniNode->pNext;
			}

			if ( !(pAniNode->uiFlags & ANITILE_EXISTINGTILE	) )
			{

				// Delete memory assosiated with item
				switch( pAniNode->ubLevelID )
				{
				case ANI_STRUCT_LEVEL:

					RemoveStructFromLevelNode( pAniNode->sGridNo, pAniNode->pLevelNode );
					break;

				case ANI_SHADOW_LEVEL:

					RemoveShadowFromLevelNode( pAniNode->sGridNo, pAniNode->pLevelNode );
					break;

				case ANI_OBJECT_LEVEL:

					RemoveObjectFromLevelNode( pAniNode->sGridNo,
						pAniNode->pLevelNode );
					break;

				case ANI_ROOF_LEVEL:

					RemoveRoofFromLevelNode( pAniNode->sGridNo,
						pAniNode->pLevelNode );
					break;

				case ANI_ONROOF_LEVEL:

					RemoveOnRoofFromLevelNode( pAniNode->sGridNo,
						pAniNode->pLevelNode );
					break;

				case ANI_TOPMOST_LEVEL:

					RemoveTopmostFromLevelNode( pAniNode->sGridNo, pAniNode->pLevelNode );
					break;

				}
				if ( pAniNode->uiFlags & ANITILE_LIGHT && pAniNode->lightSprite >= 0 )
				{
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@1 Destroying light sprite %d", pAniNode->lightSprite) );
					LightSpriteDestroy(pAniNode->lightSprite);
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@1 Light sprite destroyed") );
				}

				if ( ( pAniNode->uiFlags & ANITILE_CACHEDTILE ) )
				{
					RemoveCachedTile( pAniNode->sCachedTileID );
				}

				if ( pAniNode->uiFlags & ANITILE_EXPLOSION )
				{
					// Talk to the explosion data...
					RemoveExplosionData( pAniNode->uiUserData3 );

					if ( !gfExplosionQueueActive )
					{
						// turn on sighting again
						// the explosion queue handles all this at the end of the queue
						gTacticalStatus.uiFlags &= (~DISALLOW_SIGHT);
					}

					// Freeup attacker from explosion
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Reducing attacker busy count..., EXPLOSION effect gone off") );
					DebugAttackBusy( "@@@@@@@ EXPLOSION effect finished.\n");
					ReduceAttackBusyCount( );

				}


				if ( pAniNode->uiFlags & ANITILE_RELEASE_ATTACKER_WHEN_DONE )
				{
					// First delete the bullet!
					RemoveBullet( pAniNode->uiUserData3 );

					// 0verhaul:	Removed because it's handled by RemoveBullet.
					// DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Freeing up attacker - miss finished animation") );
					// FreeUpAttacker( (UINT8) pAniNode->ubAttackerMissed );
				}
			}
			else
			{
				TileElem = &( gTileDatabase[ pAniNode->usTileIndex ] );

				// OK, update existing tile usIndex....
				if ( TileElem->pAnimData != NULL &&
					TileElem->pAnimData->pusFrames != NULL &&
					pAniNode->pLevelNode->sCurrentFrame >= 0 &&
					pAniNode->pLevelNode->sCurrentFrame <
						TileElem->pAnimData->ubNumFrames )
				{
					pAniNode->pLevelNode->usIndex =
						TileElem->pAnimData->pusFrames[
							pAniNode->pLevelNode->sCurrentFrame ];
					// The node now references a static frame tile, whose local frame is 0.
					pAniNode->pLevelNode->sCurrentFrame = 0;
				}
				else
				{
					// If final-frame publication is impossible, preserve the caller's
					// original frame instead of manufacturing a new state.
					pAniNode->pLevelNode->sCurrentFrame =
						pAniNode->sOriginalLevelNodeFrame;
				}

				// Restore exactly the flag bits this animation was allowed to own,
				// while preserving unrelated changes made during its lifetime.
				pAniNode->pLevelNode->uiFlags =
					( pAniNode->pLevelNode->uiFlags &
						~ANIMATION_OWNED_LEVELNODE_FLAGS ) |
					pAniNode->uiOriginalLevelNodeFlags;
				if ( pAniNode->pLevelNode->pAniTile == pAniNode )
					pAniNode->pLevelNode->pAniTile = NULL;

				if (pAniNode->uiFlags & ANITILE_DOOR)
				{
					// unset door busy!
					DOOR_STATUS * pDoorStatus;

					pDoorStatus = GetDoorStatus( pAniNode->sGridNo );
					if (pDoorStatus)
					{
						pDoorStatus->ubFlags &= ~(DOOR_BUSY);
					}

					if ( GridNoOnScreen( pAniNode->sGridNo ) )
					{
						SetRenderFlags(RENDER_FLAG_FULL);
					}

				}
			}

			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ freeing up animation memory") );
			MemFree( pAniNode );
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ DeleteAniTile: done") );
			return;
		}

		pOldAniNode = pAniNode;
		pAniNode		= pAniNode->pNext;

	}


}



void UpdateAniTiles( )
{
	ANITILE *pAniNode			= NULL;
	ANITILE *pNode				= NULL;
	UINT32	uiClock				= GetJA2Clock( );
	UINT16	usMaxFrames, usMinFrames;
	UINT8		ubTempDir;

	// LOOP THROUGH EACH NODE
	pAniNode = pAniTileHead;

	while( pAniNode != NULL )
	{
		pNode = pAniNode;
		pAniNode = pAniNode->pNext;

		if ( (uiClock - pNode->uiTimeLastUpdate ) > (UINT32)pNode->sDelay && !( pNode->uiFlags & ANITILE_PAUSED ) )
		{
			pNode->uiTimeLastUpdate = GetJA2Clock( );

			if ( pNode->uiFlags & ( ANITILE_OPTIMIZEFORSLOWMOVING ) )
			{
				pNode->pLevelNode->uiFlags |= (LEVELNODE_DYNAMIC );
				pNode->pLevelNode->uiFlags &= (~LEVELNODE_LASTDYNAMIC);
			}
			else if ( pNode->uiFlags & ( ANITILE_OPTIMIZEFORSMOKEEFFECT ) )
			{
				//	pNode->pLevelNode->uiFlags |= LEVELNODE_DYNAMICZ;
				ResetSpecificLayerOptimizing( TILES_DYNAMIC_STRUCTURES );
				pNode->pLevelNode->uiFlags &= (~LEVELNODE_LASTDYNAMIC);
				pNode->pLevelNode->uiFlags |= (LEVELNODE_DYNAMIC );
			}

			if ( pNode->uiFlags & ANITILE_FORWARD )
			{
				usMaxFrames = pNode->usNumFrames;

				if ( pNode->uiFlags & ANITILE_USE_DIRECTION_FOR_START_FRAME )
				{
					ubTempDir = gOneCDirection[ pNode->uiUserData3 ];
					usMaxFrames = (UINT16)usMaxFrames + ( pNode->usNumFrames * ubTempDir );
				}

				if ( pNode->uiFlags & ANITILE_USE_4DIRECTION_FOR_START_FRAME )
				{
					ubTempDir = gb4DirectionsFrom8[ pNode->uiUserData3 ];
					usMaxFrames = (UINT16)usMaxFrames + ( pNode->usNumFrames * ubTempDir );
				}

				if ( ( pNode->sCurrentFrame + 1 ) < usMaxFrames )
				{
					pNode->sCurrentFrame++;
					pNode->pLevelNode->sCurrentFrame = pNode->sCurrentFrame;

					if ( pNode->uiFlags & ANITILE_EXPLOSION )
					{
						// Talk to the explosion data...
						UpdateExplosionFrame( pNode->uiUserData3, pNode->sCurrentFrame );
					}

					// CHECK IF WE SHOULD BE DISPLAYING TRANSLUCENTLY!
					if ( pNode->sCurrentFrame == pNode->ubKeyFrame1 )
					{
						switch( pNode->uiKeyFrame1Code )
						{
						case ANI_KEYFRAME_BEGIN_TRANSLUCENCY:

							pNode->pLevelNode->uiFlags |= LEVELNODE_REVEAL;
							break;

						case ANI_KEYFRAME_CHAIN_WATER_EXPLOSION:

							IgniteExplosion( pNode->ubUserData2, pNode->pLevelNode->sRelativeX, pNode->pLevelNode->sRelativeY, 0, pNode->sGridNo, (UINT16)( pNode->uiUserData ), 0 );
							DebugAttackBusy( "Reducing attack busy from water explosion delay.\n");
							ReduceAttackBusyCount( );
							break;

						case ANI_KEYFRAME_DO_SOUND:

							PlayJA2Sample( pNode->uiUserData, RATE_11025, SoundVolume( MIDVOLUME, (INT16)pNode->uiUserData3 ), 1, SoundDir( (INT16)pNode->uiUserData3 ) );
							break;
						}

					}

					// CHECK IF WE SHOULD BE DISPLAYING TRANSLUCENTLY!
					if ( pNode->sCurrentFrame == pNode->ubKeyFrame2 )
					{
						UINT16	 ubExpType;

						switch( pNode->uiKeyFrame2Code )
						{
						case ANI_KEYFRAME_BEGIN_DAMAGE:

							ubExpType = Explosive[ Item[ (UINT16)pNode->uiUserData ].ubClassIndex ].ubType;

							// Flugente: if tile has a fire retardant effect, don't create new fire
							if ( ubExpType == EXPLOSV_BURNABLEGAS )
							{
								if ( gpWorldLevelData[pNode->sGridNo].ubExtFlags[gExplosionData[pNode->uiUserData3].Params.bLevel] & MAPELEMENT_EXT_FIRERETARDANT_SMOKE )
								{
									// don't add fire
									return;
								}
							}

							if ( ubExpType == EXPLOSV_TEARGAS || ubExpType == EXPLOSV_MUSTGAS ||
								ubExpType == EXPLOSV_SMOKE || ubExpType == EXPLOSV_BURNABLEGAS || ubExpType == EXPLOSV_SIGNAL_SMOKE || ubExpType == EXPLOSV_SMOKE_DEBRIS || ubExpType == EXPLOSV_CREATUREGAS ||
								ubExpType == EXPLOSV_SMOKE_FIRERETARDANT )
							{
								// Do sound....
								// PlayJA2Sample( AIR_ESCAPING_1, RATE_11025, SoundVolume( HIGHVOLUME, pNode->sGridNo ), 1, SoundDir( pNode->sGridNo ) );
								NewSmokeEffect( pNode->sGridNo, (UINT16)pNode->uiUserData, gExplosionData[ pNode->uiUserData3 ].Params.bLevel, pNode->ubUserData2 );
							}
							else
							{
								SpreadEffect( pNode->sGridNo, (UINT8)Explosive[ Item[ (UINT16)pNode->uiUserData ].ubClassIndex ].ubRadius, (UINT16)pNode->uiUserData, pNode->ubUserData2, FALSE, gExplosionData[ pNode->uiUserData3 ].Params.bLevel, -1 );
							}
							// Forfait any other animations this frame....
							return;
						}
					}
				}
				else
				{
					// We are done!
					if ( pNode->uiFlags & ANITILE_LOOPING )
					{
						pNode->sCurrentFrame = pNode->sStartFrame;

						if ( ( pNode->uiFlags & ANITILE_USE_DIRECTION_FOR_START_FRAME ) )
						{
							// Our start frame is actually a direction indicator
							ubTempDir = gOneCDirection[ pNode->uiUserData3 ];
							pNode->sCurrentFrame = (UINT16)( pNode->usNumFrames * ubTempDir );
						}

						if ( ( pNode->uiFlags & ANITILE_USE_4DIRECTION_FOR_START_FRAME ) )
						{
							// Our start frame is actually a direction indicator
							ubTempDir = gb4DirectionsFrom8[ pNode->uiUserData3 ];
							pNode->sCurrentFrame = (UINT16)( pNode->usNumFrames * ubTempDir );
						}

					}
					else if ( pNode->uiFlags & ANITILE_REVERSE_LOOPING )
					{
						// Turn off backwards flag
						pNode->uiFlags &= (~ANITILE_FORWARD );

						// Turn onn forwards flag
						pNode->uiFlags |= ANITILE_BACKWARD;
					}
					else
					{

						// Delete from world!
						DeleteAniTile( pNode );

						// Turn back on redunency checks!
						gTacticalStatus.uiFlags &= (~NOHIDE_REDUNDENCY);
						DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("UpdateAniTiles: tile deleted, flags updated - done") );

						return;
					}
				}
			}

			if ( pNode->uiFlags & ANITILE_BACKWARD )
			{
				if ( pNode->uiFlags & ANITILE_ERASEITEMFROMSAVEBUFFFER )
				{
					// ATE: Check if bounding box is on the screen...
					if ( pNode->bFrameCountAfterStart == 0 )
					{
						pNode->bFrameCountAfterStart = 1;
						pNode->pLevelNode->uiFlags |= (LEVELNODE_DYNAMIC );

						// Dangerous here, since we may not even be on the screen...
						SetRenderFlags( RENDER_FLAG_FULL );

						continue;
					}
				}

				usMinFrames = 0;

				if ( pNode->uiFlags & ANITILE_USE_DIRECTION_FOR_START_FRAME )
				{
					ubTempDir = gOneCDirection[ pNode->uiUserData3 ];
					usMinFrames = ( pNode->usNumFrames * ubTempDir );
				}

				if ( pNode->uiFlags & ANITILE_USE_4DIRECTION_FOR_START_FRAME )
				{
					ubTempDir = gb4DirectionsFrom8[ pNode->uiUserData3 ];
					usMinFrames = ( pNode->usNumFrames * ubTempDir );
				}

				if ( ( pNode->sCurrentFrame - 1 ) >= usMinFrames )
				{
					pNode->sCurrentFrame--;
					pNode->pLevelNode->sCurrentFrame = pNode->sCurrentFrame;

					if ( pNode->uiFlags & ANITILE_EXPLOSION )
					{
						// Talk to the explosion data...
						UpdateExplosionFrame( pNode->uiUserData3, pNode->sCurrentFrame );
					}

				}
				else
				{
					// We are done!
					if ( pNode->uiFlags & ANITILE_PAUSE_AFTER_LOOP )
					{
						// Turn off backwards flag
						pNode->uiFlags &= (~ANITILE_BACKWARD );

						// Pause
						pNode->uiFlags |= ANITILE_PAUSED;

					}
					else if ( pNode->uiFlags & ANITILE_LOOPING )
					{
						pNode->sCurrentFrame = pNode->sStartFrame;

						if ( ( pNode->uiFlags & ANITILE_USE_DIRECTION_FOR_START_FRAME ) )
						{
							// Our start frame is actually a direction indicator
							ubTempDir = gOneCDirection[ pNode->uiUserData3 ];
							pNode->sCurrentFrame = (UINT16)( pNode->usNumFrames * ubTempDir );
						}
						if ( ( pNode->uiFlags & ANITILE_USE_4DIRECTION_FOR_START_FRAME ) )
						{
							// Our start frame is actually a direction indicator
							ubTempDir = gb4DirectionsFrom8[ pNode->uiUserData3 ];
							pNode->sCurrentFrame = (UINT16)( pNode->usNumFrames * ubTempDir );
						}

					}
					else if ( pNode->uiFlags & ANITILE_REVERSE_LOOPING )
					{
						// Turn off backwards flag
						pNode->uiFlags &= (~ANITILE_BACKWARD );

						// Turn onn forwards flag
						pNode->uiFlags |= ANITILE_FORWARD;
					}
					else
					{
						// Delete from world!
						DeleteAniTile( pNode );
						DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("UpdateAniTiles: tile deleted - done") );

						return;
					}

					if ( pNode->uiFlags & ANITILE_ERASEITEMFROMSAVEBUFFFER )
					{
						// ATE: Check if bounding box is on the screen...
						pNode->bFrameCountAfterStart = 0;
						//pNode->pLevelNode->uiFlags |= LEVELNODE_UPDATESAVEBUFFERONCE;

						// Dangerous here, since we may not even be on the screen...
						SetRenderFlags( RENDER_FLAG_FULL );

					}

				}

			}

		}
		else
		{
			if ( pNode->uiFlags & ( ANITILE_OPTIMIZEFORSLOWMOVING ) )
			{
				// ONLY TURN OFF IF PAUSED...
				if ( ( pNode->uiFlags & ANITILE_ERASEITEMFROMSAVEBUFFFER ) )
				{
					if ( pNode->uiFlags & ANITILE_PAUSED )
					{
						if ( pNode->pLevelNode->uiFlags & LEVELNODE_DYNAMIC )
						{
							pNode->pLevelNode->uiFlags &= (~LEVELNODE_DYNAMIC );
							pNode->pLevelNode->uiFlags |= (LEVELNODE_LASTDYNAMIC);
							SetRenderFlags( RENDER_FLAG_FULL );
						}
					}
				}
				else
				{
					pNode->pLevelNode->uiFlags &= (~LEVELNODE_DYNAMIC );
					pNode->pLevelNode->uiFlags |= (LEVELNODE_LASTDYNAMIC);
				}
			}
			else if ( pNode->uiFlags & ( ANITILE_OPTIMIZEFORSMOKEEFFECT ) )
			{
				pNode->pLevelNode->uiFlags |= (LEVELNODE_LASTDYNAMIC);
				pNode->pLevelNode->uiFlags &= (~LEVELNODE_DYNAMIC );
			}

		}

	}

}

void SetAniTileFrame( ANITILE *pAniTile, INT16 sFrame )
{
	UINT8 ubTempDir;
	INT16	sStartFrame = 0;

	if ( (pAniTile->uiFlags & ANITILE_USE_DIRECTION_FOR_START_FRAME ) )
	{
		// Our start frame is actually a direction indicator
		ubTempDir = gOneCDirection[ pAniTile->uiUserData3 ];
		sStartFrame = (UINT16)sFrame + ( pAniTile->usNumFrames * ubTempDir );
	}

	if ( (pAniTile->uiFlags & ANITILE_USE_4DIRECTION_FOR_START_FRAME ) )
	{
		// Our start frame is actually a direction indicator
		ubTempDir = gb4DirectionsFrom8[ pAniTile->uiUserData3 ];
		sStartFrame = (UINT16)sFrame + ( pAniTile->usNumFrames * ubTempDir );
	}

	pAniTile->sCurrentFrame = sStartFrame;

}


ANITILE *GetCachedAniTileOfType( INT32 sGridNo, UINT8 ubLevelID, UINT32 uiFlags )
{
	LEVELNODE *pNode = NULL;

	switch( ubLevelID )
	{
	case ANI_STRUCT_LEVEL:

		pNode = gpWorldLevelData[ sGridNo ].pStructHead;
		break;

	case ANI_SHADOW_LEVEL:

		pNode = gpWorldLevelData[ sGridNo ].pShadowHead;
		break;

	case ANI_OBJECT_LEVEL:

		pNode = gpWorldLevelData[ sGridNo ].pObjectHead;
		break;

	case ANI_ROOF_LEVEL:

		pNode = gpWorldLevelData[ sGridNo ].pRoofHead;
		break;

	case ANI_ONROOF_LEVEL:

		pNode = gpWorldLevelData[ sGridNo ].pOnRoofHead;
		break;

	case ANI_TOPMOST_LEVEL:

		pNode = gpWorldLevelData[ sGridNo ].pTopmostHead;
		break;

	default:

		return( NULL );
	}

	while( pNode != NULL )
	{
		if ( pNode->uiFlags & LEVELNODE_CACHEDANITILE )
		{
			if ( pNode->pAniTile->uiFlags & uiFlags )
			{
				return( pNode->pAniTile );
			}

		}

		pNode = pNode->pNext;
	}

	return( NULL );
}


void HideAniTile( ANITILE *pAniTile, BOOLEAN fHide )
{
	if ( fHide )
	{
		pAniTile->pLevelNode->uiFlags |= LEVELNODE_HIDDEN;
	}
	else
	{
		pAniTile->pLevelNode->uiFlags &= (~LEVELNODE_HIDDEN );
	}
}

void PauseAniTile( ANITILE *pAniTile, BOOLEAN fPause )
{
	if ( fPause )
	{
		pAniTile->uiFlags |= ANITILE_PAUSED;
	}
	else
	{
		pAniTile->uiFlags &= (~ANITILE_PAUSED );
	}
}


void PauseAllAniTilesOfType( UINT32 uiType, BOOLEAN fPause )
{
	ANITILE *pAniNode			= NULL;
	ANITILE *pNode				= NULL;

	// LOOP THROUGH EACH NODE
	pAniNode = pAniTileHead;

	while( pAniNode != NULL )
	{
		pNode = pAniNode;
		pAniNode = pAniNode->pNext;

		if ( pNode->uiFlags & uiType )
		{
			PauseAniTile( pNode, fPause );
		}

	}

}
