// ja2_headless_tests.cpp -- first slice of the engine test harness.
//
// Proves that the JA2 engine links into a standalone test binary (i.e. the
// engine is testable OUTSIDE the game executable) and that the DATA-FREE part
// of the Standard Gaming Platform boot sequence -- the memory / file / video
// managers, everything InitializeStandardGamingPlatform() runs BEFORE
// initVirtualFileSystem() -- comes up and shuts down cleanly. Runs headless
// (SDL dummy drivers) and, under the ASan build preset, catches init-order,
// leak and uninitialized-read regressions in the SGP layer.
//
// The game DATA (Data-1.13, maps, tilesets) is NOT in the repo, so the fuller
// "InitializeJA2 -> LoadWorld -> tick 300 frames" test is a deliberate
// follow-up gated on a small checked-in test-data fixture. This slice is the
// linkable-engine foundation that one builds on.

#define SDL_MAIN_HANDLED   // this file owns main(), not SDL
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "types.h"
#include "MemMan.h"
#include "FileMan.h"
#include "video.h"
#include "vobject.h"
#include "vsurface.h"
#include "KeyMap.h"
#include <vfs/Tools/vfs_hp_timer.h>
#include <vfs/Tools/vfs_profiler.h>

// Globals that sgp/sgp.cpp (the game's app shell) defines and the engine
// libraries reference. This harness supplies its own main() instead of linking
// sgp.cpp, so it must provide them itself.
int      iWindowedMode        = 1;
BOOLEAN  gfProgramIsRunning   = TRUE;
BOOLEAN  gfDedicatedServer    = FALSE;
BOOLEAN  gfDontUseDDBlits     = FALSE;
bool     g_bUseXML_Structures = false;
CHAR8    gzCommandLine[ 100 ] = { 0 };

// The engine libs also call ShutdownWithErrorBox() (defined in sgp/sgp.cpp) on a fatal error.
// Windows' lld-link pulls the referencing object into the test binary even though the macOS/
// Linux archive linkers don't, so the harness must define it. Behave like a fatal handler.
void ShutdownWithErrorBox( const CHAR8* pcMessage )
{
	std::fprintf( stderr, "ShutdownWithErrorBox: %s\n", pcMessage ? pcMessage : "" );
	std::exit( 1 );
}

static int g_failures = 0;
#define CHECK( cond, msg ) \
	do { if ( !( cond ) ) { ++g_failures; std::printf( "FAIL  %s\n", msg ); } \
	     else std::printf( "ok    %s\n", msg ); } while ( 0 )

int main( int, char** )
{
	std::printf( "== ja2_headless_tests: data-free SGP boot ==\n" );

	// Run headless: no window server / audio device required.
	SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "dummy" );
	SDL_SetHint( SDL_HINT_AUDIO_DRIVER, "dummy" );

	// --- hard asserts: the fully data-free managers ---
	CHECK( InitializeMemoryManager(), "InitializeMemoryManager()" );

	// MemAlloc round-trip -- exercises the allocator whose 500+ unchecked call
	// sites this project keeps hand-guarding.
	{
		const UINT32 n = 4096;
		UINT8* p = (UINT8*) MemAlloc( n );
		CHECK( p != NULL, "MemAlloc(4096) returns non-NULL" );
		if ( p )
		{
			std::memset( p, 0xAB, n );
			CHECK( p[ 0 ] == 0xAB && p[ n - 1 ] == 0xAB, "MemAlloc block writable end-to-end" );
			MemFree( p );
		}
	}

	CHECK( InitializeFileManager( NULL ), "InitializeFileManager(NULL)" );

#ifndef _WIN32
	{
		const int parsed = ParseKeyString( "CTRL + F12 + A + LEFT + B" );
		const UINT8* keys = (const UINT8*)&parsed;
		CHECK( keys[0] == 0x11 && keys[1] == 0x7B && keys[2] == 'A' && keys[3] == 0x25,
		       "portable key parser preserves four packed VK-compatible keys" );
	}
#endif

	// The VFS profiler/logger timer used to have no macOS return path and its
	// Linux timeval calculation lost whole seconds. Exercise the portable
	// monotonic implementation without depending on a wall-clock epoch.
	{
		vfs::HPTimer timer;
		timer.startTimer();
		SDL_Delay( 10 );
		CHECK( timer.running() > 0.0, "HPTimer reports running elapsed time" );
		timer.stopTimer();
		CHECK( timer.ticks() > 0, "HPTimer reports positive monotonic ticks" );
		CHECK( timer.getElapsedTimeInSeconds() > 0.0, "HPTimer reports stopped elapsed time" );
	}

	// Registration used to write past a fixed 1,024-element marker array.
	// Exceed that boundary under ASan and exercise both valid and invalid IDs.
	{
		vfs::Profiler& profiler = vfs::Profiler::getProfiler();
		profiler.clear();
		vfs::Profiler::tMarkerID marker = 0;
		for ( int i = 0; i < 1100; ++i )
			marker = profiler.registerMarker( "headless-marker" );
		CHECK( marker == 1099, "Profiler grows beyond 1024 markers" );
		profiler.startMarker( marker );
		profiler.stopMarker( marker, true );
		profiler.startMarker( 999999 );
		profiler.stopMarker( 999999, false );
	}

	// --- best-effort: the video managers. They need an SDL video backend; the
	//     dummy driver may or may not provide a renderer depending on platform,
	//     so a failure here is reported but does NOT fail the suite (it is a
	//     headless-environment limitation, not an engine regression). ---
	if ( InitializeVideoManager() )
	{
		std::printf( "ok    InitializeVideoManager() [headless]\n" );
		if ( InitializeVideoObjectManager() )  std::printf( "ok    InitializeVideoObjectManager()\n" );
		if ( InitializeVideoSurfaceManager() ) std::printf( "ok    InitializeVideoSurfaceManager()\n" );
		ShutdownVideoManager();
	}
	else
	{
		std::printf( "skip  InitializeVideoManager() (no headless video backend available)\n" );
	}

	ShutdownMemoryManager();

	std::printf( "\n%s  (%d failure%s)\n",
		g_failures ? "HEADLESS TESTS FAILED" : "HEADLESS TESTS PASSED",
		g_failures, g_failures == 1 ? "" : "s" );
	return g_failures ? 1 : 0;
}
