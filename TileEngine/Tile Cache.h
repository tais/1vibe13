#ifndef __TILE_CACHE_H
#define __TILE_CACHE_H


#define	TILE_CACHE_START_INDEX		36000

typedef struct
{
	CHAR8					zName[ 128 ];			// Name of tile ( filename and directory here )
	CHAR8					zRootName[ 30 ];	// Root name
	TILE_IMAGERY	*pImagery;				// Tile imagery
	INT16					sHits;
	UINT8					ubNumFrames;
	INT16					sStructRefID;

} TILE_CACHE_ELEMENT;

typedef struct
{
	CHAR8										Filename[ 150 ];
	CHAR8										zRootName[ 30 ];	// Root name
	STRUCTURE_FILE_REF *		pStructureFileRef;

} TILE_CACHE_STRUCT;

// Non-owning compatibility views retained for native mods and diagnostics.
// Treat the pointed-to records as read-only: ownership and pin decisions live
// in the cache APIs, and the views are synchronized after each API mutation.
extern TILE_CACHE_ELEMENT* gpTileCache;
extern TILE_CACHE_STRUCT* gpTileCacheStructInfo;
extern UINT32 guiNumTileCacheStructs;
extern UINT32 guiMaxTileCacheSize;
extern UINT32 guiCurTileCacheSize;
extern INT32 giDefaultStructIndex;


BOOLEAN InitTileCache( );
void DeleteTileCache( );
BOOLEAN IsTileCacheInitialized( );


INT32 GetCachedTile( const STR8 cFilename );

BOOLEAN RemoveCachedTile( INT32 iCachedTile );
INT16 GetCachedTileReferenceCount( INT32 iCachedTile );

STRUCTURE_FILE_REF *GetCachedTileStructureRefFromFilename( const STR8 cFilename );

HVOBJECT						GetCachedTileVideoObject( INT32 iIndex );
UINT8 GetCachedTileFrameCount( INT32 iIndex );
STRUCTURE_FILE_REF *GetCachedTileStructureRef( INT32 iIndex );
void CheckForAndAddTileCacheStructInfo( LEVELNODE *pNode, INT32 sGridNo, INT8 bLevel, UINT16 usIndex, UINT16 usSubIndex );
void CheckForAndDeleteTileCacheStructInfo( LEVELNODE *pNode, UINT16 usIndex );

void GetRootName( CHAR8 *pDestStr, const STR8 pSrcStr );

#endif
