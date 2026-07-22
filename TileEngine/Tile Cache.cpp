#include "builddefines.h"

	#include <climits>
	#include <cstdio>
	#include <cstring>
	#include "DEBUG.H"
	#include "Debug Control.h"
	#include "FileMan.h"
	#include "MemMan.h"
	#include "structure.h"
	#include "Tile Surface.h"
	#include "Tile Cache.h"
#ifdef JA2TESTVERSION
	#include "Sys Globals.h"
#endif

UINT32	guiNumTileCacheStructs = 0;
UINT32 guiMaxTileCacheSize		= 50;
UINT32 guiCurTileCacheSize		= 0;
INT32	giDefaultStructIndex	= -1;

TILE_CACHE_ELEMENT		*gpTileCache = NULL;
TILE_CACHE_STRUCT			*gpTileCacheStructInfo = NULL;

namespace
{
	void ResetTileCacheElement( TILE_CACHE_ELEMENT& element )
	{
		std::memset( &element, 0, sizeof( element ) );
		element.sStructRefID = -1;
	}

	BOOLEAN IsLiveTileCacheIndex( INT32 index )
	{
		return gpTileCache != NULL && index >= 0 &&
			static_cast<UINT32>( index ) < guiCurTileCacheSize &&
			static_cast<UINT32>( index ) < guiMaxTileCacheSize &&
			gpTileCache[ index ].pImagery != NULL && gpTileCache[ index ].sHits > 0;
	}

	void ReportTileCacheFailure( const CHAR8* reason, const STR8 filename = NULL )
	{
		std::fprintf( stderr, "[tile-cache] %s%s%s\n", reason,
			filename != NULL ? ": " : "", filename != NULL ? filename : "" );
	}

	BOOLEAN CopyRootName( CHAR8* destination, size_t destinationSize, const STR8 source )
	{
		if ( destination == NULL || destinationSize == 0 || source == NULL )
			return FALSE;

		const CHAR8* root = source;
		const CHAR8* backslash = std::strrchr( source, '\\' );
		const CHAR8* slash = std::strrchr( source, '/' );
		if ( backslash != NULL && ( slash == NULL || backslash > slash ) )
			root = backslash + 1;
		else if ( slash != NULL )
			root = slash + 1;

		const CHAR8* extension = std::strchr( root, '.' );
		const size_t length = extension != NULL
			? static_cast<size_t>( extension - root ) : std::strlen( root );
		if ( length >= destinationSize )
		{
			destination[ 0 ] = '\0';
			return FALSE;
		}

		std::memcpy( destination, root, length );
		destination[ length ] = '\0';
		return TRUE;
	}
}



BOOLEAN InitTileCache(	)
{
	UINT32				cnt;
	GETFILESTRUCT FileInfo;
	UINT32					sFiles = 0;

	// Repeated initialization must neither leak the old cache nor invalidate live IDs.
	if ( gpTileCache != NULL )
		return TRUE;

	// Recover safely from any earlier partial initialization.
	DeleteTileCache();

	gpTileCache = (TILE_CACHE_ELEMENT *)MemAlloc( sizeof( TILE_CACHE_ELEMENT ) * guiMaxTileCacheSize );
	if ( gpTileCache == NULL )
		return FALSE;

	// Zero entries
	for ( cnt = 0; cnt < guiMaxTileCacheSize; cnt++ )
	{
		ResetTileCacheElement( gpTileCache[ cnt ] );
	}

	guiCurTileCacheSize = 0;
	guiNumTileCacheStructs = 0;
	giDefaultStructIndex = -1;


	// OK, look for JSD files in the tile cache directory and
	// load any we find....
	if( GetFileFirst("TILECACHE\\*.jsd", &FileInfo) )
	{
		do
		{
			sFiles++;
		} while( GetFileNext(&FileInfo) );
		GetFileClose(&FileInfo);
	}

	// Allocate memory...
	if ( sFiles > 0 )
	{
		cnt = 0;

		gpTileCacheStructInfo = (TILE_CACHE_STRUCT *)MemAlloc( sizeof( TILE_CACHE_STRUCT ) * sFiles );
		if ( gpTileCacheStructInfo == NULL )
		{
			DeleteTileCache();
			return FALSE;
		}
		std::memset( gpTileCacheStructInfo, 0, sizeof( TILE_CACHE_STRUCT ) * sFiles );

		// Loop through and set filenames
		if( GetFileFirst("TILECACHE\\*.jsd", &FileInfo) )
		{
			do
			{
				// The directory can change between the count and load passes.
				if ( cnt >= sFiles )
					break;
				const int filenameLength = snprintf( gpTileCacheStructInfo[ cnt ].Filename,
					sizeof(gpTileCacheStructInfo[cnt].Filename), "TILECACHE\\%s", FileInfo.zFileName );
				if ( filenameLength < 0 ||
					static_cast<size_t>( filenameLength ) >= sizeof( gpTileCacheStructInfo[ cnt ].Filename ) )
				{
					ReportTileCacheFailure( "structure filename is too long", FileInfo.zFileName );
					continue;
				}

				// Get root name
				if ( !CopyRootName( gpTileCacheStructInfo[ cnt ].zRootName,
					sizeof( gpTileCacheStructInfo[ cnt ].zRootName ),
					gpTileCacheStructInfo[ cnt ].Filename ) )
				{
					ReportTileCacheFailure( "structure root name is too long",
						gpTileCacheStructInfo[ cnt ].Filename );
					continue;
				}

				// Load struc data....
				gpTileCacheStructInfo[ cnt ].pStructureFileRef = LoadStructureFile( gpTileCacheStructInfo[ cnt ].Filename );

#ifdef JA2TESTVERSION
				if ( gpTileCacheStructInfo[ cnt ].pStructureFileRef == NULL )
				{
					SET_ERROR(	"Cannot load tilecache JSD: %s", gpTileCacheStructInfo[ cnt ].Filename );
				}
#endif
				if ( _stricmp( gpTileCacheStructInfo[ cnt ].zRootName, "l_dead1" ) == 0 )
				{
					giDefaultStructIndex = cnt;
				}

				cnt++;
			} while( GetFileNext(&FileInfo) );
			guiNumTileCacheStructs = cnt;
			GetFileClose(&FileInfo);
		}

		if ( guiNumTileCacheStructs == 0 )
		{
			MemFree( gpTileCacheStructInfo );
			gpTileCacheStructInfo = NULL;
		}
	}

	return( TRUE );
}

void DeleteTileCache( )
{
	UINT32 cnt;

	// Allocate entries
	if ( gpTileCache != NULL )
	{
		// Loop through and delete any entries
		for ( cnt = 0; cnt < guiMaxTileCacheSize; cnt++ )
		{
			if ( gpTileCache[ cnt ].pImagery != NULL )
			{
				DeleteTileSurface( gpTileCache[ cnt ].pImagery );
			}
			ResetTileCacheElement( gpTileCache[ cnt ] );
		}
		MemFree( gpTileCache );
		gpTileCache = NULL;
	}

	if ( gpTileCacheStructInfo != NULL )
	{
		for ( cnt = 0; cnt < guiNumTileCacheStructs; ++cnt )
		{
			if ( gpTileCacheStructInfo[ cnt ].pStructureFileRef != NULL )
			{
				FreeStructureFile( gpTileCacheStructInfo[ cnt ].pStructureFileRef );
				gpTileCacheStructInfo[ cnt ].pStructureFileRef = NULL;
			}
		}
		MemFree( gpTileCacheStructInfo );
		gpTileCacheStructInfo = NULL;
	}

	guiCurTileCacheSize = 0;
	guiNumTileCacheStructs = 0;
	giDefaultStructIndex = -1;
}

INT16 FindCacheStructDataIndex( STR8 cFilename )
{
	UINT32 cnt;
	if ( cFilename == NULL || gpTileCacheStructInfo == NULL )
		return -1;

	for ( cnt = 0; cnt < guiNumTileCacheStructs; cnt++ )
	{
		if ( _stricmp( gpTileCacheStructInfo[ cnt ].zRootName, cFilename ) == 0 )
		{
			return(	(INT16)cnt );
		}
	}

	return( -1 );
}

INT32 GetCachedTile( const STR8 cFilename )
{
	UINT32			cnt;

	if ( gpTileCache == NULL )
	{
		ReportTileCacheFailure( "request before initialization", cFilename );
		return -1;
	}
	if ( cFilename == NULL || cFilename[ 0 ] == '\0' )
	{
		ReportTileCacheFailure( "empty tile filename" );
		return -1;
	}
	if ( std::strlen( cFilename ) >= sizeof( gpTileCache[ 0 ].zName ) )
	{
		ReportTileCacheFailure( "tile filename is too long", cFilename );
		return -1;
	}

	// Check to see if surface exists already
	for ( cnt = 0; cnt < guiCurTileCacheSize; cnt++ )
	{
		if ( gpTileCache[ cnt ].pImagery != NULL )
		{
			if ( _stricmp( gpTileCache[ cnt ].zName, cFilename ) == 0 )
			{
				if ( gpTileCache[ cnt ].sHits <= 0 || gpTileCache[ cnt ].sHits == INT16_MAX )
				{
					ReportTileCacheFailure( "invalid or saturated tile reference count", cFilename );
					return -1;
				}

				// Found surface, retain it and preserve its stable cache ID.
				gpTileCache[ cnt ].sHits++;
				return( (INT32)cnt );
			}
		}
	}

	// Zero-reference entries are deleted by RemoveCachedTile. Therefore every
	// occupied slot is pinned and must never be selected as an eviction victim.
	for ( cnt = 0; cnt < guiMaxTileCacheSize; cnt++ )
	{
		if ( gpTileCache[ cnt ].pImagery == NULL )
		{
			TILE_IMAGERY* imagery = LoadTileSurface( cFilename );
			if ( imagery == NULL )
			{
				ReportTileCacheFailure( "could not load tile surface", cFilename );
				return( -1 );
			}

			ResetTileCacheElement( gpTileCache[ cnt ] );
			gpTileCache[ cnt ].pImagery = imagery;
			std::strcpy( gpTileCache[ cnt ].zName, cFilename );
			gpTileCache[ cnt ].sHits = 1;

			// Get root name
			if ( !CopyRootName( gpTileCache[ cnt ].zRootName,
				sizeof( gpTileCache[ cnt ].zRootName ), cFilename ) )
			{
				ReportTileCacheFailure( "tile root name is too long", cFilename );
				DeleteTileSurface( gpTileCache[ cnt ].pImagery );
				ResetTileCacheElement( gpTileCache[ cnt ] );
				return -1;
			}

			gpTileCache[ cnt ].sStructRefID = FindCacheStructDataIndex( gpTileCache[ cnt ].zRootName );

			// ATE: Add z-strip info
			if ( gpTileCache[ cnt ].sStructRefID != -1 )
			{
				STRUCTURE_FILE_REF* structureRef =
					gpTileCacheStructInfo[ gpTileCache[ cnt ].sStructRefID ].pStructureFileRef;
				if ( structureRef != NULL )
					AddZStripInfoToVObject( gpTileCache[ cnt ].pImagery->vo,
						structureRef, TRUE, 0 );
			}

			if ( gpTileCache[ cnt ].pImagery->pAuxData != NULL )
			{
				gpTileCache[ cnt ].ubNumFrames = gpTileCache[ cnt ].pImagery->	pAuxData->ubNumberOfFrames;
			}
			else
			{
				gpTileCache[ cnt ].ubNumFrames = 1;
			}

			// Has our cache size increased?
			if ( cnt >= guiCurTileCacheSize )
			{
				guiCurTileCacheSize = cnt + 1;;
			}

			return( cnt );
		}
	}

	UINT32 references = 0;
	for ( cnt = 0; cnt < guiMaxTileCacheSize; ++cnt )
	{
		if ( gpTileCache[ cnt ].sHits > 0 )
			references += static_cast<UINT32>( gpTileCache[ cnt ].sHits );
	}
	CHAR8 capacityFailure[ 128 ];
	snprintf( capacityFailure, sizeof( capacityFailure ),
		"capacity %u exhausted by %u live reference%s", guiMaxTileCacheSize,
		references, references == 1 ? "" : "s" );
	ReportTileCacheFailure( capacityFailure, cFilename );
	return( -1 );
}


BOOLEAN RemoveCachedTile( INT32 iCachedTile )
{
	if ( !IsLiveTileCacheIndex( iCachedTile ) )
		return FALSE;

	TILE_CACHE_ELEMENT& element = gpTileCache[ iCachedTile ];
	if ( element.sHits > 1 )
	{
		--element.sHits;
		return FALSE;
	}

	DeleteTileSurface( element.pImagery );
	ResetTileCacheElement( element );
	while ( guiCurTileCacheSize > 0 &&
		gpTileCache[ guiCurTileCacheSize - 1 ].pImagery == NULL )
	{
		--guiCurTileCacheSize;
	}
	return TRUE;
}


HVOBJECT GetCachedTileVideoObject( INT32 iIndex )
{
	if ( !IsLiveTileCacheIndex( iIndex ) )
		return( NULL );

	return( gpTileCache[ iIndex ].pImagery->vo );
}


UINT8 GetCachedTileFrameCount( INT32 iIndex )
{
	if ( !IsLiveTileCacheIndex( iIndex ) )
		return 0;

	return gpTileCache[ iIndex ].ubNumFrames;
}


STRUCTURE_FILE_REF *GetCachedTileStructureRef( INT32 iIndex )
{
	if ( !IsLiveTileCacheIndex( iIndex ) || gpTileCacheStructInfo == NULL )
		return( NULL );

	const INT16 structureIndex = gpTileCache[ iIndex ].sStructRefID;
	if ( structureIndex < 0 || static_cast<UINT32>( structureIndex ) >= guiNumTileCacheStructs )
	{
		return( NULL );
	}

	return( gpTileCacheStructInfo[ structureIndex ].pStructureFileRef );
}


STRUCTURE_FILE_REF *GetCachedTileStructureRefFromFilename( const STR8 cFilename )
{
	INT16 sStructDataIndex;

	// Given filename, look for index
	sStructDataIndex = FindCacheStructDataIndex( cFilename );

	if ( sStructDataIndex == -1 )
	{
		return( NULL );
	}

	return( gpTileCacheStructInfo[ sStructDataIndex ].pStructureFileRef );
}


void CheckForAndAddTileCacheStructInfo( LEVELNODE *pNode, INT32 sGridNo, INT8 bLevel, UINT16 usIndex, UINT16 usSubIndex )
{
	STRUCTURE_FILE_REF *pStructureFileRef;

	pStructureFileRef = GetCachedTileStructureRef( usIndex );

	if ( pNode != NULL && pStructureFileRef != NULL &&
		usSubIndex < pStructureFileRef->usNumberOfStructures )
	{
		if ( !AddStructureToWorld( sGridNo, bLevel, &( pStructureFileRef->pDBStructureRef[ usSubIndex ] ), pNode ) )
	{
		if ( giDefaultStructIndex >= 0 && gpTileCacheStructInfo != NULL &&
			static_cast<UINT32>( giDefaultStructIndex ) < guiNumTileCacheStructs )
		{
		pStructureFileRef = gpTileCacheStructInfo[ giDefaultStructIndex ].pStructureFileRef;

		if ( pStructureFileRef != NULL && usSubIndex < pStructureFileRef->usNumberOfStructures )
		{
			AddStructureToWorld( sGridNo, bLevel, &( pStructureFileRef->pDBStructureRef[ usSubIndex ] ), pNode );
		}
		}
	}
	}
}

void CheckForAndDeleteTileCacheStructInfo( LEVELNODE *pNode, UINT16 usIndex )
{
	STRUCTURE_FILE_REF *pStructureFileRef;

	if ( usIndex >= TILE_CACHE_START_INDEX )
	{
		pStructureFileRef = GetCachedTileStructureRef( ( usIndex - TILE_CACHE_START_INDEX ) );

		if ( pNode != NULL && pNode->pStructureData != NULL && pStructureFileRef != NULL)
		{
			DeleteStructureFromWorld( pNode->pStructureData );
		}
	}
}

void GetRootName( CHAR8 *pDestStr, const STR8 pSrcStr )
{
	// Legacy pointer-only API retained for compatibility. Its existing callers
	// all pass buffers at least 128 bytes wide; bounded cache-owned buffers use
	// CopyRootName directly above.
	CopyRootName( pDestStr, 128, pSrcStr );
}


