#include "builddefines.h"

#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "FileMan.h"
#include "structure.h"
#include "Tile Surface.h"
#include "Tile Cache.h"

namespace
{
	int failures = 0;
	int tileLoads = 0;
	int tileDeletes = 0;
	int structureLoads = 0;
	int structureDeletes = 0;
	std::vector<std::string> structureFiles;

	void Check( bool condition, const char* message )
	{
		if ( condition )
			std::printf( "ok    %s\n", message );
		else
		{
			++failures;
			std::printf( "FAIL  %s\n", message );
		}
	}

	void FillFindResult( GETFILESTRUCT* result, size_t index )
	{
		result->iFindHandle = static_cast<INT32>( index );
		std::snprintf( result->zFileName, sizeof( result->zFileName ), "%s",
			structureFiles[ index ].c_str() );
		result->uiFileSize = 1;
		result->uiFileAttribs = FILE_IS_NORMAL;
	}
}

BOOLEAN GetFileFirst( const CHAR8*, GETFILESTRUCT* result )
{
	if ( result == NULL || structureFiles.empty() )
		return FALSE;
	FillFindResult( result, 0 );
	return TRUE;
}

BOOLEAN GetFileNext( GETFILESTRUCT* result )
{
	if ( result == NULL )
		return FALSE;
	const size_t next = static_cast<size_t>( result->iFindHandle ) + 1;
	if ( next >= structureFiles.size() )
		return FALSE;
	FillFindResult( result, next );
	return TRUE;
}

void GetFileClose( GETFILESTRUCT* result )
{
	if ( result != NULL )
		result->iFindHandle = -1;
}

TILE_IMAGERY* LoadTileSurface( STR8 )
{
	TILE_IMAGERY* imagery = static_cast<TILE_IMAGERY*>( std::calloc( 1, sizeof( TILE_IMAGERY ) ) );
	if ( imagery == NULL )
		return NULL;
	imagery->vo = reinterpret_cast<HVOBJECT>( std::malloc( 1 ) );
	if ( imagery->vo == NULL )
	{
		std::free( imagery );
		return NULL;
	}
	++tileLoads;
	return imagery;
}

void DeleteTileSurface( PTILE_IMAGERY imagery )
{
	if ( imagery == NULL )
		return;
	std::free( reinterpret_cast<void*>( imagery->vo ) );
	std::free( imagery );
	++tileDeletes;
}

STRUCTURE_FILE_REF* LoadStructureFile( STR )
{
	STRUCTURE_FILE_REF* structure = static_cast<STRUCTURE_FILE_REF*>(
		std::calloc( 1, sizeof( STRUCTURE_FILE_REF ) ) );
	if ( structure != NULL )
		++structureLoads;
	return structure;
}

BOOLEAN FreeStructureFile( STRUCTURE_FILE_REF* structure )
{
	if ( structure == NULL )
		return FALSE;
	std::free( structure );
	++structureDeletes;
	return TRUE;
}

BOOLEAN AddZStripInfoToVObject( HVOBJECT, STRUCTURE_FILE_REF*, BOOLEAN, INT16 )
{
	return TRUE;
}

BOOLEAN AddStructureToWorld( INT32, INT8, DB_STRUCTURE_REF*, PTR )
{
	return TRUE;
}

BOOLEAN DeleteStructureFromWorld( STRUCTURE* )
{
	return TRUE;
}

int main()
{
	std::printf( "== tile_cache_tests ==\n" );

	Check( GetCachedTileVideoObject( -1 ) == NULL &&
	       GetCachedTileVideoObject( 0 ) == NULL &&
	       GetCachedTileFrameCount( 0 ) == 0 &&
	       GetCachedTileStructureRef( 0 ) == NULL &&
	       !RemoveCachedTile( 0 ) && GetCachedTile( const_cast<STR8>( "before-init.sti" ) ) == -1,
	       "uninitialized and out-of-range cache IDs are rejected" );

	structureFiles = { "l_dead1.jsd" };
	Check( InitTileCache(), "tile cache initializes with an owned structure reference" );
	STRUCTURE_FILE_REF* firstStructure =
		GetCachedTileStructureRefFromFilename( const_cast<STR8>( "l_dead1" ) );
	Check( firstStructure != NULL && structureLoads == 1,
	       "structure lookup returns the initialized owned reference" );
	Check( InitTileCache() && structureLoads == 1 &&
	       GetCachedTileStructureRefFromFilename( const_cast<STR8>( "l_dead1" ) ) == firstStructure,
	       "repeated initialization preserves live cache IDs and allocations" );
	DeleteTileCache();
	Check( gpTileCache == NULL && structureDeletes == 1 &&
	       GetCachedTileStructureRefFromFilename( const_cast<STR8>( "l_dead1" ) ) == NULL,
	       "delete frees and nulls cache-owned structure state" );
	DeleteTileCache();
	Check( structureDeletes == 1, "repeated delete is idempotent" );

	structureFiles.clear();
	Check( InitTileCache(), "tile cache initializes without game data" );
	std::array<HVOBJECT, 50> pinnedObjects = {};
	for ( INT32 index = 0; index < 50; ++index )
	{
		const std::string filename = "TILECACHE\\pinned-" + std::to_string( index ) + ".sti";
		const INT32 id = GetCachedTile( const_cast<STR8>( filename.c_str() ) );
		if ( id == index )
			pinnedObjects[ index ] = GetCachedTileVideoObject( id );
		else
			++failures;
	}
	Check( tileLoads == 50,
	       "fifty unique pinned tiles retain their legacy slot IDs" );

	const INT32 overflow = GetCachedTile( const_cast<STR8>( "TILECACHE\\pinned-50.sti" ) );
	bool unchanged = overflow == -1 && tileLoads == 50 && tileDeletes == 0;
	for ( INT32 index = 0; index < 50; ++index )
		unchanged = unchanged && GetCachedTileVideoObject( index ) == pinnedObjects[ index ] &&
			gpTileCache[ index ].sHits == 1;
	Check( unchanged,
	       "the 51st request fails without evicting or overwriting any pinned entry" );

	Check( GetCachedTile( const_cast<STR8>( "TILECACHE\\pinned-0.sti" ) ) == 0 &&
	       gpTileCache[ 0 ].sHits == 2 && !RemoveCachedTile( 0 ) &&
	       gpTileCache[ 0 ].sHits == 1 && GetCachedTileVideoObject( 0 ) == pinnedObjects[ 0 ],
	       "duplicate acquisition and release retain the original stable ID" );

	Check( RemoveCachedTile( 17 ) && GetCachedTileVideoObject( 17 ) == NULL &&
	       !RemoveCachedTile( 17 ) && tileDeletes == 1,
	       "final release deletes an entry and stale empty-slot release is rejected" );
	const INT32 reused = GetCachedTile( const_cast<STR8>( "TILECACHE\\replacement.sti" ) );
	Check( reused == 17 && GetCachedTileVideoObject( reused ) != NULL &&
	       GetCachedTileVideoObject( 18 ) == pinnedObjects[ 18 ],
	       "a released slot is deterministically reused without renumbering live entries" );

	Check( GetCachedTileVideoObject( -2 ) == NULL &&
	       GetCachedTileVideoObject( 50 ) == NULL &&
	       GetCachedTileFrameCount( INT_MAX ) == 0 &&
	       GetCachedTileStructureRef( 50 ) == NULL &&
	       !RemoveCachedTile( -1 ) && !RemoveCachedTile( 50 ),
	       "negative and out-of-range public cache access is safe" );

	gpTileCache[ 0 ].sHits = INT16_MAX;
	Check( GetCachedTile( const_cast<STR8>( "TILECACHE\\pinned-0.sti" ) ) == -1 &&
	       gpTileCache[ 0 ].sHits == INT16_MAX,
	       "reference-count saturation fails rather than wrapping" );
	gpTileCache[ 0 ].sHits = 1;

	DeleteTileCache();
	Check( gpTileCache == NULL && tileDeletes == 51,
	       "cache teardown deletes each remaining owned surface exactly once" );

	structureFiles = { "first.jsd", "second.jsd" };
	const int loadsBeforeCycles = structureLoads;
	const int deletesBeforeCycles = structureDeletes;
	bool lifecycleCycles = true;
	for ( int cycle = 0; cycle < 8; ++cycle )
	{
		lifecycleCycles = lifecycleCycles && InitTileCache() && gpTileCache != NULL;
		DeleteTileCache();
		DeleteTileCache();
		lifecycleCycles = lifecycleCycles && gpTileCache == NULL;
	}
	Check( lifecycleCycles && structureLoads - loadsBeforeCycles == 16 &&
	       structureDeletes - deletesBeforeCycles == 16,
	       "repeated init/delete cycles free all owned structure references" );

	std::printf( "\n%s (%d failure%s)\n",
		failures == 0 ? "TILE CACHE TESTS PASSED" : "TILE CACHE TESTS FAILED",
		failures, failures == 1 ? "" : "s" );
	return failures == 0 ? 0 : 1;
}
