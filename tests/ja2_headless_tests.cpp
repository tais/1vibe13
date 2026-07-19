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
#include <utility>

#include "types.h"
#include "MemMan.h"
#include "FileMan.h"
#include "video.h"
#include "vobject.h"
#include "vsurface.h"
#include "../Engine/Core/UniqueResourceHandle.h"
#include "../Engine/Core/DeterministicCommandQueue.h"
#include "../Engine/Core/BinaryArchive.h"
#include "../Engine/Core/StateStack.h"
#include "../Engine/Core/ContentApi.h"
#include "../Engine/Core/TimeSource.h"
#include "../Engine/Core/RandomSource.h"
#include "../Engine/Core/PersistenceService.h"
#include "PlatformFileSystem.h"
#include "KeyMap.h"
#include "input.h"
#include "sdl_input.h"
#include "english.h"
#include "GameContext.h"
#include "CampaignPackage.h"
#include "Soldier Control.h"
#include <vfs/Tools/vfs_hp_timer.h>
#include <vfs/Tools/vfs_profiler.h>
#include <vfs/Core/vfs_init.h>

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

struct TestResourceTag {};
static UINT32 g_releasedResource = 0;
static UINT32 g_resourceReleaseCount = 0;
struct TestResourceReleaser
{
	void operator()(UINT32 value) const
	{
		g_releasedResource = value;
		++g_resourceReleaseCount;
	}
};
using TestResourceHandle = UniqueResourceHandle<TestResourceTag, TestResourceReleaser>;

int main( int, char** )
{
	std::printf( "== ja2_headless_tests: data-free SGP boot ==\n" );

	// Run headless: no window server / audio device required.
	SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "dummy" );
	SDL_SetHint( SDL_HINT_AUDIO_DRIVER, "dummy" );

	{
		GAME_SETTINGS settings = {};
		GAME_OPTIONS options = {};
		GameContext context( settings, options );
		CHECK( &context.settings() == &settings && &context.options() == &options,
		       "game context exposes bound legacy state" );
		GameCapabilities editorCapabilities;
		editorCapabilities.editor = true;
		CHECK( context.setCapabilities( editorCapabilities ) && context.capabilities().isEditor(),
		       "game context accepts runtime capabilities before initialization" );
		CHECK( context.beginInitialization() && context.markRunning(),
		       "game context enters running lifecycle" );
		CHECK( !context.setCapabilities( GameCapabilities{} ),
		       "game context freezes runtime capabilities while running" );
		CHECK( !context.beginInitialization(), "game context rejects duplicate initialization" );
		CHECK( context.beginShutdown() && context.markStopped(),
		       "game context completes shutdown lifecycle" );
		{
			GameInitializationGuard initialization( context );
			CHECK( initialization, "game initialization guard starts from stopped state" );
		}
		CHECK( context.lifecycle() == GameLifecycle::Stopped,
		       "game initialization guard rolls back incomplete initialization" );
	}

	{
		g_resourceReleaseCount = 0;
		TestResourceHandle first( 42 );
		TestResourceHandle second( std::move( first ) );
		CHECK( !first && second.get() == 42, "resource handle move transfers ownership" );
		second.reset( 84 );
		CHECK( g_releasedResource == 42 && second.get() == 84, "resource handle reset releases previous value" );
		CHECK( second.release() == 84 && !second, "resource handle release returns an unowned value" );
		{
			TestResourceHandle scoped( 126 );
		}
		CHECK( g_releasedResource == 126 && g_resourceReleaseCount == 2,
		       "resource handle destructor releases exactly once" );
	}

	{
		DeterministicCommandQueue<int> commands;
		commands.enqueue( 20, 200 );
		commands.enqueue( 10, 100 );
		commands.enqueue( 10, 101 );
		const auto firstTick = commands.drainThrough( 10 );
		CHECK( firstTick.size() == 2 && firstTick[0].command == 100 && firstTick[1].command == 101,
		       "simulation commands order by tick then insertion sequence" );
		CHECK( commands.size() == 1, "future simulation commands remain queued" );
		CHECK( commands.enqueueRecorded( 15, 50, 150 ) && !commands.enqueueRecorded( 15, 50, 999 ),
		       "recorded simulation commands reject duplicate sequence IDs" );
		const auto replay = commands.drainThrough( 20 );
		CHECK( replay.size() == 2 && replay[0].command == 150 && replay[1].command == 200,
		       "recorded simulation commands replay deterministically" );
	}

	{
		BinaryWriter writer;
		WritePersistenceHeader( writer, PersistenceHeader{ 0x32414A31u, 2 } );
		writer.writeU32( 0x12345678u );
		writer.writeString( "Arulco" );
		BinaryReader reader( writer.bytes() );
		PersistenceHeader header = {};
		std::uint32_t value = 0;
		std::string text;
		CHECK( ReadPersistenceHeader( reader, 0x32414A31u, 1, 2, header ) &&
		       reader.readU32( value ) && reader.readString( text ) &&
		       value == 0x12345678u && text == "Arulco" && reader.remaining() == 0,
		       "versioned persistence round-trips portable values" );
		std::vector<std::uint8_t> truncated = writer.bytes();
		truncated.pop_back();
		BinaryReader truncatedReader( truncated );
		ReadPersistenceHeader( truncatedReader, 0x32414A31u, 1, 2, header );
		truncatedReader.readU32( value );
		const std::size_t stringPosition = truncatedReader.position();
		CHECK( !truncatedReader.readString( text ) && truncatedReader.position() == stringPosition,
		       "persistence reader rejects truncated fields without consuming input" );
		BinaryReader versionReader( writer.bytes() );
		CHECK( !ReadPersistenceHeader( versionReader, 0x32414A31u, 3, 4, header ),
		       "persistence reader rejects unsupported schema versions" );
	}

	{
		StateStack<int> screens;
		screens.reset( 1 );
		CHECK( screens.pushOverlay( 2 ) && screens.current()->overlay && screens.underlay()->state == 1,
		       "state stack preserves a screen beneath an overlay" );
		CHECK( screens.popOverlay() && screens.current()->state == 1,
		       "state stack returns to the underlay when an overlay closes" );
		screens.replace( 3 );
		CHECK( screens.size() == 1 && screens.current()->state == 3 && !screens.popOverlay(),
		       "state replacement does not create false navigation history" );
	}

	{
		ContentRegistry content( ContentApiVersion{ 1, 2 } );
		CHECK( content.registerContent( ContentManifest{ "core", "0.9.0", { 1, 0 } } ) ==
		       ContentRegistrationError::None,
		       "content registry accepts a compatible versioned manifest" );
		CHECK( content.registerContent( ContentManifest{ "future", "1.0.0", { 2, 0 } } ) ==
		       ContentRegistrationError::IncompatibleApi,
		       "content registry rejects incompatible API majors" );
		CHECK( content.registerContent( ContentManifest{ "core", "0.9.1", { 1, 1 } } ) ==
		       ContentRegistrationError::DuplicateId,
		       "content registry rejects ambiguous duplicate IDs" );
		const ContentManifest* manifest = content.find( "core" );
		CHECK( manifest && manifest->version == "0.9.0",
		       "content registry resolves the validated manifest" );
	}

	{
		ContentRegistry content( ContentApiVersion{ 1, 0 } );
		PackageRegistry packages( content );
		LegacyCampaignPackage arulco( GameCapabilities{} );
		GameCapabilities ubCapabilities;
		ubCapabilities.campaign = GameCampaign::UnfinishedBusiness;
		LegacyCampaignPackage unfinishedBusiness( ubCapabilities );
		CHECK( packages.registerPackage( arulco ) == PackageRegistrationError::None &&
		       packages.registerPackage( unfinishedBusiness ) == PackageRegistrationError::None,
		       "campaign packages register through the versioned engine API" );
		CHECK( packages.activate( "ja2.arulco" ) == PackageActivationError::None && arulco.active(),
		       "campaign package activation is selected at runtime" );
		CHECK( packages.activate( "ja2.unfinished-business" ) ==
		       PackageActivationError::CampaignAlreadyActive,
		       "package registry prevents conflicting active campaigns" );
		CHECK( packages.deactivate( "ja2.arulco" ) &&
		       packages.activate( "ja2.unfinished-business" ) == PackageActivationError::None &&
		       unfinishedBusiness.active(),
		       "campaign packages can be switched without compile-time selection" );
	}

	{
		GameContext& compiledContext = GetGameContext();
		LegacyCampaignPackage& compiledPackage = GetCompiledCampaignPackage();
		const std::string& packageId = compiledPackage.descriptor().content.id;
		CHECK( compiledContext.packages().activeCampaign() == packageId &&
		       compiledContext.packages().isActive( packageId ) && compiledPackage.active(),
		       "legacy compiled campaign is bound through the runtime package registry" );
		CHECK( compiledPackage.capabilities().campaign == compiledContext.capabilities().campaign,
		       "campaign adapter preserves the compiled JA2 or UB compatibility default" );
	}

	{
		ManualTimeSource time;
		time.setMicroseconds( 1000 );
		time.advanceMicroseconds( 250 );
		MonotonicTimeSource& source = time;
		CHECK( source.nowMicroseconds() == 1250,
		       "engine time source supports deterministic injected time" );
	}

	{
		SequenceRandomSource random( { 9, 4, 7 } );
		CHECK( random.next( 10 ) == 9 && random.next( 3 ) == 1 && random.next( 5 ) == 2,
		       "engine random source produces a deterministic bounded sequence" );
		random.rewind();
		CHECK( random.next( 10 ) == 9 && random.position() == 1,
		       "engine random source rewinds for deterministic replay" );
	}

	// --- hard asserts: the fully data-free managers ---
	CHECK( InitializeMemoryManager(), "InitializeMemoryManager()" );

	{
		SOLDIERTYPE soldier;
		SoldierVitalsComponent vitals = soldier.vitals();
		vitals.maximumHealth() = 90;
		vitals.health() = 75;
		vitals.breath() = 60;
		SoldierPositionComponent position = soldier.position();
		position.gridNo() = 1234;
		position.level() = 1;
		CHECK( vitals.alive() && soldier.stats.bLife == 75 && soldier.bBreath == 60,
		       "soldier vitals component aliases the compatible serialized fields" );
		CHECK( soldier.sGridNo == 1234 && soldier.pathing.bLevel == 1,
		       "soldier position component aliases the compatible serialized fields" );
	}

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

	// FileMan is a facade over the VFS. The full game configures its profiles
	// during application boot; this standalone harness supplies the smallest
	// equivalent writable profile so storage integration follows the real path.
	vfs_init::VfsConfig vfsConfig;
	vfs_init::Profile* testProfile = new vfs_init::Profile();
	testProfile->m_name = L"headless-tests";
	testProfile->m_root = L".";
	testProfile->m_writable = true;
	vfsConfig.addProfile( testProfile, true );
	CHECK( vfs_init::initVirtualFileSystem( vfsConfig ), "initialize writable headless VFS profile" );
	CHECK( InitializeFileManager( NULL ), "InitializeFileManager(NULL)" );

	{
		MemoryByteStorage memoryStorage;
		PersistenceService memoryPersistence( memoryStorage );
		std::vector<std::uint8_t> emptyPayload;
		PersistenceHeader emptyHeader = {};
		CHECK( memoryPersistence.save( "empty", PersistenceHeader{ 0x454E4730u, 1 }, emptyPayload ) &&
		       memoryPersistence.load( "empty", 0x454E4730u, 1, 1, emptyHeader, emptyPayload ) ==
		       PersistenceLoadResult::Success && emptyPayload.empty(),
		       "pure persistence service supports an empty payload" );
		memoryStorage.writeAll( "truncated", std::vector<std::uint8_t>{ 1, 2, 3 } );
		CHECK( memoryPersistence.load( "truncated", 0x454E4730u, 1, 1, emptyHeader, emptyPayload ) ==
		       PersistenceLoadResult::InvalidOrUnsupported,
		       "pure persistence service rejects a truncated header" );

		const std::string path = "engine_persistence_test.bin";
		PersistenceService persistence( GetPlatformByteStorage() );
		const std::vector<std::uint8_t> saved = { 1, 3, 3, 7 };
		CHECK( persistence.save( path, PersistenceHeader{ 0x454E4731u, 1 }, saved ),
		       "platform persistence writes through FileMan" );
		PersistenceHeader header = {};
		std::vector<std::uint8_t> loaded;
		CHECK( persistence.load( path, 0x454E4731u, 1, 1, header, loaded ) == PersistenceLoadResult::Success &&
		       loaded == saved,
		       "platform persistence reads a validated versioned payload" );
		CHECK( persistence.load( path, 0x454E4731u, 2, 2, header, loaded ) ==
		       PersistenceLoadResult::InvalidOrUnsupported,
		       "platform persistence rejects unsupported content versions" );
		FileDelete( const_cast<char*>(path.c_str()) );
	}

#ifndef _WIN32
	{
		const int parsed = ParseKeyString( "CTRL + F12 + A + LEFT + B" );
		const UINT8* keys = (const UINT8*)&parsed;
		CHECK( keys[0] == 0x11 && keys[1] == 0x7B && keys[2] == 'A' && keys[3] == 0x25,
		       "portable key parser preserves four packed VK-compatible keys" );
	}
#endif

	{
		CHECK( InitializeInputManager(), "InitializeInputManager()" );
		SDL_Event keyEvent = {};
		keyEvent.type = SDL_EVENT_KEY_DOWN;
		keyEvent.key.scancode = SDL_SCANCODE_F1;
		keyEvent.key.key = SDLK_F1;
		SgpHandleSDLEvent( &keyEvent );
		InputAtom atom = {};
		CHECK( DequeueEvent( &atom ) && atom.usEvent == KEY_DOWN && atom.usParam == F1,
		       "SDL F1 queues the JA2 symbolic function-key value" );
		keyEvent.type = SDL_EVENT_KEY_UP;
		SgpHandleSDLEvent( &keyEvent );
		DequeueEvent( &atom );
		ShutdownInputManager();
	}

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
		UINT32 framePitch = 0;
		UINT32 backPitch = 0;
		CHECK( LockFrameBuffer( &framePitch ) != NULL && framePitch == SCREEN_WIDTH * sizeof( PIXEL ),
		       "framebuffer ownership exposes the expected legacy lock view" );
		CHECK( LockBackBuffer( &backPitch ) != NULL && backPitch == SCREEN_WIDTH * sizeof( PIXEL ),
		       "backbuffer ownership exposes the expected legacy lock view" );
		if ( InitializeVideoObjectManager() )  std::printf( "ok    InitializeVideoObjectManager()\n" );
		if ( InitializeVideoSurfaceManager() ) std::printf( "ok    InitializeVideoSurfaceManager()\n" );
		ShutdownVideoManager();
		CHECK( LockFrameBuffer( NULL ) == NULL && LockBackBuffer( NULL ) == NULL,
		       "video shutdown clears legacy buffer views" );
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
