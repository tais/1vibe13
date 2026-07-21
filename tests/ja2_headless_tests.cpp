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

#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "types.h"
#include "MemMan.h"
#include "FileMan.h"
#include "video.h"
#include "vobject.h"
#include "vsurface.h"
#include <Engine/Core/UniqueResourceHandle.h>
#include <Engine/Core/UniqueResourcePtr.h>
#include <Engine/Core/EngineHost.h>
#include <Engine/Core/DeterministicCommandQueue.h>
#include <Engine/Core/CommandDispatch.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/StateStack.h>
#include <Engine/Core/StateTransition.h>
#include <Engine/Core/ContentApi.h>
#include <Engine/Core/TimeSource.h>
#include <Engine/Core/RandomSource.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Adapters/Legacy/PlatformFileSystem.h>
#include <Engine/Adapters/Legacy/PlatformFramePresenter.h>
#include <Engine/Adapters/Legacy/PlatformAssets.h>
#include <Engine/Adapters/Legacy/PlatformInput.h>
#include <Engine/Adapters/Legacy/PlatformAudio.h>
#include <Engine/Adapters/Legacy/PlatformLog.h>
#include <Engine/Adapters/Legacy/PlatformTime.h>
#include "random.h"
#include "KeyMap.h"
#include "input.h"
#include "sdl_input.h"
#include "english.h"
#include "GameContext.h"
#include "CampaignPackage.h"
#include "PackageHost.h"
#include "popup_class.h"
#include "Soldier Control.h"
#include "MovementDestinationPolicy.h"
#include <vfs/Tools/vfs_hp_timer.h>
#include <vfs/Tools/vfs_profiler.h>
#include <vfs/Tools/vfs_property_container.h>
#include <vfs/Core/vfs.h>
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

class ScopedPackageFixture
{
public:
	ScopedPackageFixture()
	{
		static std::uint64_t sequence = 0;
		const std::uint64_t timestamp = static_cast<std::uint64_t>(
			std::chrono::steady_clock::now().time_since_epoch().count() );
		root_ = std::filesystem::temp_directory_path() /
			( "ja2-package-host-" + std::to_string( timestamp ) + "-" +
			  std::to_string( sequence++ ) );
		std::filesystem::create_directories( root_ );
	}

	~ScopedPackageFixture()
	{
		std::error_code ignored;
		std::filesystem::remove_all( root_, ignored );
	}

	ScopedPackageFixture( const ScopedPackageFixture& ) = delete;
	ScopedPackageFixture& operator=( const ScopedPackageFixture& ) = delete;

	const std::filesystem::path& root() const { return root_; }

	bool write( const std::filesystem::path& relative, const std::string& contents ) const
	{
		const std::filesystem::path path = root_ / relative;
		std::error_code error;
		std::filesystem::create_directories( path.parent_path(), error );
		if ( error ) return false;
		std::ofstream output( path, std::ios::binary | std::ios::trunc );
		output.write( contents.data(), static_cast<std::streamsize>( contents.size() ) );
		return static_cast<bool>( output );
	}

	bool makeDirectory( const std::filesystem::path& relative ) const
	{
		std::error_code error;
		std::filesystem::create_directories( root_ / relative, error );
		return !error;
	}

private:
	std::filesystem::path root_;
};

static std::string PackageFixtureManifest(
	const std::string& id, const std::string& requirements = {},
	const std::string& assetRoot = "Data", const std::string& optionalRequirements = {},
	const std::string& conflicts = {}, const std::string& loadAfter = {},
	const std::string& capabilities = {} )
{
	const bool policyV2 = !optionalRequirements.empty() || !conflicts.empty() ||
		!loadAfter.empty() || !capabilities.empty();
	std::string manifest =
		"[Package]\n"
		"MANIFEST_VERSION = " + std::string( policyV2 ? "2" : "1" ) + "\n"
		"ID = " + id + "\n"
		"VERSION = 1.0.0\n"
		"CONTENT_API = " + std::string( policyV2 ? "1.3" :
		                                  ( requirements.empty() ? "1.1" : "1.2" ) ) + "\n"
		"TYPE = extension\n"
		"ASSET_ROOT = " + assetRoot + "\n";
	if ( !requirements.empty() ) manifest += "REQUIRES = " + requirements + "\n";
	if ( !optionalRequirements.empty() )
		manifest += "OPTIONAL_REQUIRES = " + optionalRequirements + "\n";
	if ( !conflicts.empty() ) manifest += "CONFLICTS = " + conflicts + "\n";
	if ( !loadAfter.empty() ) manifest += "LOAD_AFTER = " + loadAfter + "\n";
	if ( !capabilities.empty() ) manifest += "CAPABILITIES = " + capabilities + "\n";
	return manifest;
}

static bool AddPackageFixture(
	const ScopedPackageFixture& fixture, const std::string& directory,
	const std::string& id, const std::string& requirements = {} )
{
	return fixture.makeDirectory( directory + "/Data" ) &&
		fixture.write( directory + "/package.ini",
		               PackageFixtureManifest( id, requirements ) );
}

class RecordingPackageAssetMounter final : public PackageAssetMounter
{
public:
	bool preflight( const std::string& packageId,
	                const std::filesystem::path&, std::string& error ) const override
	{
		preflighted.push_back( packageId );
		if ( packageId != failPreflightId ) return true;
		error = "injected preflight failure";
		return false;
	}

	bool mount( const std::string& packageId,
	            const std::filesystem::path&, std::string& error ) override
	{
		mounted.push_back( packageId );
		// Model a mounter that acquires state before it can discover a late
		// indexing failure. The host must unwind this even though mount returns
		// false.
		activeMounts.push_back( packageId );
		if ( packageId != failMountId ) return true;
		error = "injected mount failure";
		return false;
	}

	bool unmount( const std::string& packageId, std::string& error ) override
	{
		unmounted.push_back( packageId );
		if ( packageId == failUnmountId )
		{
			error = "injected unmount failure";
			return false;
		}
		const auto found = std::find( activeMounts.begin(), activeMounts.end(), packageId );
		if ( found == activeMounts.end() )
			return true;
		activeMounts.erase( found );
		return true;
	}

	mutable std::vector<std::string> preflighted;
	std::vector<std::string> mounted;
	std::vector<std::string> unmounted;
	std::vector<std::string> activeMounts;
	std::string failPreflightId;
	std::string failMountId;
	std::string failUnmountId;
};

static bool ReadFileManagerText( const std::string& logicalPath, std::string& contents )
{
	contents.clear();
	HWFILE file = FileOpen( const_cast<char*>( logicalPath.c_str() ),
	                        FILE_ACCESS_READ | FILE_OPEN_EXISTING );
	if ( !file ) return false;
	const UINT32 size = FileGetSize( file );
	contents.resize( size );
	UINT32 bytesRead = 0;
	const bool read = size == 0 ||
		( FileRead( file, &contents[0], size, &bytesRead ) && bytesRead == size );
	FileClose( file );
	if ( read ) return true;
	contents.clear();
	return false;
}

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

static UINT32 g_popupCallbackDestructionCount = 0;
static UINT32 g_popupCallbackCallCount = 0;
class CountingPopupCallback final : public popupCallback
{
public:
	~CountingPopupCallback() override { ++g_popupCallbackDestructionCount; }
	void bind(void*) override {}
	bool call() override
	{
		++g_popupCallbackCallCount;
		return true;
	}
};

struct TestPointerResource
{
	int value;
};
static TestPointerResource* g_releasedPointerResource = nullptr;
static UINT32 g_pointerResourceReleaseCount = 0;
struct TestPointerResourceReleaser
{
	void operator()(TestPointerResource* value) const
	{
		g_releasedPointerResource = value;
		++g_pointerResourceReleaseCount;
	}
};
using TestPointerResourceOwner =
	UniqueResourcePtr<TestPointerResource, TestPointerResourceReleaser>;

class FailingAssetSource final : public AssetSource
{
protected:
	bool existsNormalized(const std::string& logicalPath) const override
	{
		return logicalPath == "data/items.xml";
	}
	AssetReadResult readNormalized(const std::string& logicalPath, AssetData&,
		std::size_t) const override
	{
		return existsNormalized(logicalPath) ? AssetReadResult::IoError : AssetReadResult::NotFound;
	}
};

class ThrowingContainsAssetSource final : public AssetSource
{
public:
	bool containsSource(const AssetSource*) const override
	{
		throw "test asset composition exception";
	}

protected:
	bool existsNormalized(const std::string&) const override { return false; }
	AssetReadResult readNormalized(const std::string&, AssetData&,
		std::size_t) const override { return AssetReadResult::NotFound; }
};

class TestLifecyclePackage final : public EnginePackage
{
public:
	TestLifecyclePackage(std::string id, PackageKind kind, int failPhase = -1,
	                     AssetSource* assets = nullptr,
	                     std::vector<ContentRequirement> requirements = {},
	                     std::string version = "1.0",
	                     std::vector<std::string> capabilities = {})
		: descriptor_{ContentManifest{std::move(id), std::move(version),
		                              ContentApiVersion{1, static_cast<std::uint16_t>(
			                              !requirements.empty() ? 2 : (assets ? 1 : 0))},
		                              std::move(requirements)},
		              kind, std::move(capabilities)},
		  failPhase_(failPhase), assets_(assets)
	{
	}

	TestLifecyclePackage(ContentManifest manifest, PackageKind kind,
	                     int failPhase = -1, AssetSource* assets = nullptr)
		: descriptor_{std::move(manifest), kind}, failPhase_(failPhase), assets_(assets)
	{
	}

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override
	{
		++activateCalls;
		active_ = activationSucceeds;
		return active_;
	}
	void deactivate() noexcept override
	{
		++deactivateCalls;
		if (deactivationTrace)
			deactivationTrace->push_back(descriptor_.content.id);
		if (registryDuringDeactivate)
			nestedDeactivateResult = registryDuringDeactivate->deactivate( deactivateDuringDeactivate );
		active_ = false;
	}
	const AssetSource* assetSource() const noexcept override { return assets_; }
	bool bootstrap(PackageBootstrapContext& context, PackageBootstrapPhase phase) override
	{
		if (lifecycleTrace)
			lifecycleTrace->push_back("bootstrap:" + descriptor_.content.id);
		observedServices = &context.services;
		observedContentApi = context.content.supportedApi();
		observedTime = context.services.time.nowMicroseconds();
		observedRandom = context.services.random.next( 100 );
		AssetData asset;
		if (context.services.assets.read("Data/Rules/weapons.bin", asset) ==
			AssetReadResult::Success)
			observedAssetProvenance = asset.provenance;
		if (phase == PackageBootstrapPhase::Configure && registryDuringBootstrap)
		{
			nestedResolutionResult =
				registryDuringBootstrap->resolveActivation( activateDuringBootstrap );
			nestedActivationResult = registryDuringBootstrap->activate( activateDuringBootstrap );
			nestedBootstrapDeactivationResult =
				registryDuringBootstrap->deactivate( deactivateDuringBootstrap );
		}
		bootstrapCalls.push_back(static_cast<int>(phase));
		if (static_cast<int>(phase) == throwPhase) throw "test package bootstrap exception";
		return static_cast<int>(phase) != failPhase_;
	}
	void shutdown(PackageBootstrapContext&, PackageBootstrapPhase phase) override
	{
		if (lifecycleTrace)
			lifecycleTrace->push_back("shutdown:" + descriptor_.content.id);
		shutdownCalls.push_back(static_cast<int>(phase));
	}

	std::vector<int> bootstrapCalls;
	std::vector<int> shutdownCalls;
	ContentApiVersion observedContentApi{};
	std::uint64_t observedTime = 0;
	std::uint32_t observedRandom = 0;
	std::string observedAssetProvenance;
	EngineServices* observedServices = nullptr;
	int activateCalls = 0;
	int deactivateCalls = 0;
	bool activationSucceeds = true;
	int throwPhase = -1;
	PackageRegistry* registryDuringBootstrap = nullptr;
	std::string activateDuringBootstrap;
	std::string deactivateDuringBootstrap;
	PackageActivationError nestedActivationResult = PackageActivationError::None;
	PackageActivationPlan nestedResolutionResult;
	bool nestedBootstrapDeactivationResult = true;
	PackageRegistry* registryDuringDeactivate = nullptr;
	std::string deactivateDuringDeactivate;
	bool nestedDeactivateResult = true;
	std::vector<std::string>* lifecycleTrace = nullptr;
	std::vector<std::string>* deactivationTrace = nullptr;
	bool active() const { return active_; }

private:
	PackageDescriptor descriptor_;
	int failPhase_;
	AssetSource* assets_;
	bool active_ = false;
};

class ThrowingPackageEventSink final : public PackageEventSink
{
public:
	void publish( PackageEvent ) override
	{
		++calls;
		throw "test package event exception";
	}

	int calls = 0;
};

int main( int, char** )
{
	std::printf( "== ja2_headless_tests: data-free SGP boot ==\n" );

	// Run headless: no window server / audio device required.
	SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "dummy" );
	SDL_SetHint( SDL_HINT_AUDIO_DRIVER, "dummy" );

	{
		char tiny[5] = {};
		const int required = sprintf( tiny, "%s", "abcdef" );
		CHECK( required == 6 && std::strcmp( tiny, "abcd" ) == 0,
		       "legacy narrow formatting truncates safely and reports required size" );
	}

	{
		static_assert( !std::is_copy_constructible<EngineHost<unsigned>>::value,
		               "engine host must retain stable internal references" );
		static_assert( !std::is_move_constructible<EngineHost<unsigned>>::value,
		               "engine host must retain stable internal references" );
		static_assert( !std::is_copy_constructible<EngineRuntime<unsigned>>::value,
		               "engine runtime must retain stable internal references" );
		static_assert( !std::is_move_constructible<EngineRuntime<unsigned>>::value,
		               "engine runtime must retain stable internal references" );
		static_assert( !std::is_copy_constructible<PackageRegistry>::value &&
		               !std::is_move_constructible<PackageRegistry>::value,
		               "package registry must retain stable external references" );
		static_assert( !std::is_copy_constructible<CompositeAssetSource>::value &&
		               !std::is_move_constructible<CompositeAssetSource>::value,
		               "asset overlay identity must remain stable to prevent graph cycles" );
		EngineHost<unsigned> host;
		host.screenController().reset( 7 );
		CHECK( host.screens().current() && host.screens().current()->state == 7,
		       "command-agnostic engine host owns screen state" );
		CHECK( host.beginInitialization() && host.markRunning() &&
		       host.beginShutdown() && host.markStopped(),
		       "command-agnostic engine host owns lifecycle" );
	}

	{
		const auto shouldRetain = []( BOOLEAN fPolicyEnabled, UINT16 usAnimState )
		{
			return ShouldRetainMovementAnimationAtDestination(
				fPolicyEnabled, usAnimState, gAnimControl[ usAnimState ] );
		};
		const UINT16 crouchedLocomotion[] = {
			SWATTING, SWAT_BACKWARDS, SWATTING_WK, SWAT_BACKWARDS_WK,
			SWAT_BACKWARDS_NOTHING, SIDE_STEP_CROUCH_RIFLE, SIDE_STEP_CROUCH_PISTOL,
			SIDE_STEP_CROUCH_DUAL, CROUCHEDMOVE_RIFLE_READY,
			CROUCHEDMOVE_PISTOL_READY, CROUCHEDMOVE_DUAL_READY
		};
		bool crouchedMovementSettles = true;
		for ( const UINT16 animation : crouchedLocomotion )
		{
			crouchedMovementSettles =
				!shouldRetain( TRUE, animation ) &&
				crouchedMovementSettles;
		}
		CHECK( crouchedMovementSettles,
		       "crouched locomotion settles into a stationary crouch at its destination" );
		const UINT16 retainedLegacyAnimations[] = {
			WALKING, CROUCHING, RUNNING, CRAWLING, END_HURT_WALKING, RUNNING_W_PISTOL,
			SIDE_STEP_WEAPON_RDY, SIDE_STEP_DUAL_RDY, WALKING_WEAPON_RDY,
			WALKING_DUAL_RDY, WALKING_ALTERNATIVE_RDY, SIDE_STEP_ALTERNATIVE_RDY
		};
		bool legacyMovementRetained = true;
		for ( const UINT16 animation : retainedLegacyAnimations )
		{
			legacyMovementRetained = shouldRetain( TRUE, animation ) && legacyMovementRetained;
		}
		CHECK( legacyMovementRetained,
		       "standing, running, prone, and stationary-crouch legacy behavior is preserved" );
		CHECK( !shouldRetain( FALSE, RUNNING ) && !shouldRetain( FALSE, SWATTING ) &&
		       !shouldRetain( TRUE, STANDING ),
		       "disabled or ineligible destination animation retention settles normally" );
	}

	{
		GAME_SETTINGS settings = {};
		GAME_OPTIONS options = {};
		GameContext context( settings, options );
		CHECK( &context.settings() == &settings && &context.options() == &options,
		       "game context exposes bound legacy state" );
		CHECK( &context.runtime().services() == &context.services(),
		       "game context delegates reusable state to engine runtime" );
		GameCapabilities editorCapabilities;
		editorCapabilities.editor = true;
		CHECK( context.setCapabilities( editorCapabilities ) && context.capabilities().isEditor() &&
		       context.hasCapability( GameCapability::ApplicationMapEditor ),
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
		CHECK( !std::is_copy_constructible<POPUP_OPTION>::value &&
		       !std::is_copy_assignable<POPUP_OPTION>::value &&
		       !std::is_move_constructible<POPUP_OPTION>::value &&
		       !std::is_copy_constructible<POPUP>::value,
		       "popup owners cannot be copied or accidentally moved" );
		g_popupCallbackDestructionCount = 0;
		g_popupCallbackCallCount = 0;
		{
			POPUP_OPTION option;
			auto* action = new CountingPopupCallback;
			option.setAction( action );
			option.setAction( action );
			CHECK( g_popupCallbackDestructionCount == 0 && option.run(),
			       "assigning the same popup callback preserves its lifetime" );
			option.setAction( new CountingPopupCallback );
			option.setAvail( new CountingPopupCallback );
			option.setHover( new CountingPopupCallback );
			CHECK( g_popupCallbackDestructionCount == 1 && option.runHoverCallback( nullptr ),
			       "replacing a popup callback releases the previous owner once" );
			option.setAction( nullptr );
			option.setAvail( nullptr );
			option.setHover( nullptr );
			CHECK( g_popupCallbackDestructionCount == 4,
			       "clearing popup callbacks releases every owned callback" );
		}
		CHECK( g_popupCallbackDestructionCount == 4 && g_popupCallbackCallCount == 2,
		       "popup option teardown does not double-delete cleared callbacks" );
	}

	{
		static_assert( !std::is_copy_constructible<TestPointerResourceOwner>::value,
		               "pointer resource ownership must remain unique" );
		TestPointerResource firstValue{ 7 };
		TestPointerResource secondValue{ 9 };
		g_releasedPointerResource = nullptr;
		g_pointerResourceReleaseCount = 0;
		TestPointerResourceOwner first( &firstValue );
		TestPointerResourceOwner second( std::move( first ) );
		CHECK( !first && second->value == 7,
		       "pointer resource move transfers typed ownership" );
		second.reset( &secondValue );
		CHECK( g_releasedPointerResource == &firstValue && second.get() == &secondValue,
		       "pointer resource reset releases the previous API object" );
		CHECK( second.release() == &secondValue && !second,
		       "pointer resource release returns an unowned API pointer" );
		{
			TestPointerResourceOwner scoped( &firstValue );
		}
		CHECK( g_releasedPointerResource == &firstValue &&
		       g_pointerResourceReleaseCount == 2,
		       "pointer resource destructor releases exactly once" );
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
		DeterministicCommandQueue<int> commands;
		commands.enqueue( 5, 50 );
		commands.enqueue( 3, 30 );
		commands.enqueue( 8, 80 );
		std::vector<int> delivered;
		const std::size_t count = DispatchCommandsThrough(
			commands, 5,
			[&delivered]( int command, std::uint64_t, std::uint64_t ) {
				delivered.push_back( command );
			} );
		CHECK( count == 2 && delivered.size() == 2 && delivered[0] == 30 &&
		       delivered[1] == 50 && commands.size() == 1,
		       "engine command dispatch delivers ready commands in deterministic order" );
	}

	{
		DeterministicCommandQueue<int> commands;
		commands.enqueue( 3, 30 );
		const std::uint64_t blockedSequence = commands.enqueue( 4, 40 );
		commands.enqueue( 5, 50 );
		commands.enqueue( 9, 90 );
		std::vector<int> delivered;
		const CommandProcessingResult blocked = ProcessCommandsThrough(
			commands, 5,
			[&delivered]( int command, std::uint64_t, std::uint64_t ) {
				delivered.push_back( command );
				return command == 40 ? CommandDisposition::Retry : CommandDisposition::Applied;
			} );
		CHECK( blocked.status == CommandProcessStatus::Blocked && blocked.scheduled == 3 &&
		       blocked.applied == 1 && blocked.discarded == 0 &&
		       blocked.blockedTick == 4 && blocked.blockedSequence == blockedSequence &&
		       delivered == std::vector<int>({ 30, 40 }) && commands.size() == 3,
		       "retryable command processing acknowledges prior work without losing blocked work" );
		delivered.clear();
		const CommandProcessingResult resumed = ProcessCommandsThrough(
			commands, 5,
			[&delivered]( int command, std::uint64_t, std::uint64_t ) {
				delivered.push_back( command );
				return CommandDisposition::Applied;
			} );
		CHECK( resumed && resumed.scheduled == 2 && resumed.applied == 2 &&
		       delivered == std::vector<int>({ 40, 50 }) && commands.size() == 1,
		       "blocked deterministic commands resume in original order" );
	}

	{
		DeterministicCommandQueue<int> commands;
		commands.enqueue( 1, 10 );
		commands.enqueue( 1, 20 );
		commands.enqueue( 1, 30 );
		std::vector<int> attempted;
		bool handlerThrew = false;
		try
		{
			ProcessCommandsThrough(
				commands, 1,
				[&attempted]( int command, std::uint64_t, std::uint64_t ) {
					attempted.push_back( command );
					if ( command == 20 ) throw "test command handler exception";
					return CommandDisposition::Applied;
				} );
		}
		catch (...)
		{
			handlerThrew = true;
		}
		const auto remaining = commands.snapshotThrough( 1 );
		CHECK( handlerThrew && attempted == std::vector<int>({ 10, 20 }) &&
		       remaining.size() == 2 && remaining[0].command == 20 &&
		       remaining[1].command == 30,
		       "command handler exceptions retain the failing and later commands" );
	}

	{
		DeterministicCommandQueue<int> commands;
		commands.enqueue( 1, 10 );
		commands.enqueue( 1, 20 );
		const CommandProcessingResult bounded = ProcessCommandsThrough(
			commands, 1,
			[&commands]( int command, std::uint64_t, std::uint64_t ) {
				if ( command == 10 ) commands.enqueue( 1, 30 );
				return command == 20 ? CommandDisposition::Discard : CommandDisposition::Applied;
			} );
		const auto deferred = commands.snapshotThrough( 1 );
		CHECK( bounded && bounded.scheduled == 2 && bounded.applied == 1 &&
		       bounded.discarded == 1 && deferred.size() == 1 && deferred[0].command == 30,
		       "command passes count discards and defer commands produced by their handlers" );
	}

	{
		DeterministicCommandQueue<int> commands;
		commands.enqueue( 1, 10 );
		const CommandProcessingResult observed = ProcessCommandsThrough(
			commands, 1,
			[]( int, std::uint64_t, std::uint64_t ) {
				return CommandDisposition::Applied;
			},
			[]( int, std::uint64_t, std::uint64_t, CommandDisposition ) {
				throw "injected journal failure";
			} );
		CHECK( observed && observed.applied == 1 && commands.empty(),
		       "command observers cannot interfere with authoritative delivery" );
	}

	{
		GAME_SETTINGS settings = {};
		GAME_OPTIONS options = {};
		GameContext context( settings, options );
		const std::uint64_t localSequence = context.submitCommand(
			7, SimulationCommand{EndTurnCommand{2, SimulationCommandSource::LocalPlayer}} );
		context.submitCommand(
			7, SimulationCommand{EndTurnCommand{3, SimulationCommandSource::NetworkPeer}} );
		const auto ready = context.commands().drainThrough( 7 );
		const auto& local = std::get<EndTurnCommand>( ready[0].command );
		const auto& network = std::get<EndTurnCommand>( ready[1].command );
		CHECK( ready.size() == 2 && local.nextTeam == 2 && network.nextTeam == 3 &&
		       local.source == SimulationCommandSource::LocalPlayer &&
		       network.source == SimulationCommandSource::NetworkPeer,
		       "engine runtime owns ordered value-only tactical commands" );
		const auto submitted = context.commandJournal().snapshot();
		CHECK( submitted.size() == 2 && submitted[0].sequence == localSequence &&
		       submitted[0].status == CommandJournalStatus::Queued,
		       "engine runtime journals submitted production commands without owning executors" );
		context.submitCommand(
			8, SimulationCommand{ChangeStanceCommand{17, 2, SimulationCommandSource::LocalPlayer}} );
		const auto stanceReady = context.commands().drainThrough( 8 );
		const auto& stance = std::get<ChangeStanceCommand>( stanceReady[0].command );
		CHECK( stanceReady.size() == 1 && stance.soldierId == 17 && stance.stance == 2 &&
		       stance.source == SimulationCommandSource::LocalPlayer,
		       "engine runtime carries value-only soldier stance commands" );
		CHECK( context.submitRecordedCommand(
		           9, 500, SimulationCommand{BeginFireWeaponCommand{
		               17, 700, 1234, 0, 2, SimulationCommandSource::Replay}} ) &&
		       !context.submitRecordedCommand(
		           9, 500, SimulationCommand{BeginFireWeaponCommand{
		               17, 700, 1234, 0, 2, SimulationCommandSource::Replay}} ),
		       "engine runtime admits uniquely sequenced replay commands through the same journal" );
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
		StateStack<int> screens;
		auto overlay = []( int state ) { return state >= 100; };
		CHECK( ApplyStateTransition( screens, 1, overlay ) == StateTransitionResult::Initialized &&
		       ApplyStateTransition( screens, 100, overlay ) == StateTransitionResult::OverlayPushed &&
		       screens.underlay()->state == 1,
		       "engine state transitions preserve an underlay for overlays" );
		CHECK( ApplyStateTransition( screens, 1, overlay ) == StateTransitionResult::OverlayPopped &&
		       screens.current()->state == 1 &&
		       ApplyStateTransition( screens, 2, overlay ) == StateTransitionResult::Replaced,
		       "engine state transitions pop overlays and replace base states deterministically" );
	}

	{
		StateController<int> screens;
		auto overlay = []( int state ) { return state >= 100; };
		CHECK( screens.transitionTo( 1, overlay ) == StateTransitionResult::Initialized &&
		       screens.current() && *screens.current() == 1 && !screens.previous(),
		       "state controller initializes current state without false history" );
		CHECK( screens.request( 100 ) && screens.hasPending() &&
		       screens.pending() && *screens.pending() == 100 &&
		       screens.commitPending( overlay ) == StateTransitionResult::OverlayPushed &&
		       !screens.hasPending() && screens.current() && *screens.current() == 100 &&
		       screens.previous() && *screens.previous() == 1,
		       "state controller commits pending overlays and records previous state" );
		CHECK( screens.request( 100 ) &&
		       screens.commitPending( overlay ) == StateTransitionResult::Unchanged &&
		       screens.previous() && *screens.previous() == 1,
		       "unchanged state requests do not corrupt transition history" );
		CHECK( screens.request( 1 ) &&
		       screens.commitPending( overlay ) == StateTransitionResult::OverlayPopped &&
		       screens.current() && *screens.current() == 1 &&
		       screens.previous() && *screens.previous() == 100,
		       "state controller restores an overlay underlay deterministically" );
		CHECK( screens.request( 2 ) && screens.cancelPending() && !screens.hasPending() &&
		       screens.commitPending( overlay ) == StateTransitionResult::Unchanged &&
		       screens.current() && *screens.current() == 1,
		       "state controller can cancel a pending transition without changing current state" );
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
		CHECK( content.registerContent( ContentManifest{ "mod/assets", "1.0.0", { 1, 0 } } ) ==
		       ContentRegistrationError::InvalidManifest,
		       "content registry rejects identifiers unsafe for package provenance" );
		CHECK( content.registerContent( ContentManifest{ "self", "1.0.0", { 1, 2 },
		                                                {{ "self", "" }} } ) ==
		       ContentRegistrationError::InvalidRequirement &&
		       content.registerContent( ContentManifest{ "duplicate.requirements", "1.0.0", { 1, 2 },
		                                                {{ "core", "" }, { "core", "0.9.0" }} } ) ==
		       ContentRegistrationError::InvalidRequirement,
		       "content registry rejects self and duplicate package requirements" );
		CHECK( content.registerContent( ContentManifest{ "invalid.requirement", "1.0.0", { 1, 2 },
		                                                {{ "bad/id", "" }} } ) ==
		       ContentRegistrationError::InvalidRequirement &&
		       content.registerContent( ContentManifest{ "undeclared.requirement", "1.0.0", { 1, 1 },
		                                                {{ "core", "" }} } ) ==
		       ContentRegistrationError::InvalidRequirement,
		       "content requirements use safe IDs and explicitly require API 1.2" );
		CHECK( content.registerContent( ContentManifest{ "forward.requirement", "1.0.0", { 1, 2 },
		                                                {{ "not.registered.yet", "2.0" }} } ) ==
		       ContentRegistrationError::None,
		       "content registry accepts forward requirements independent of discovery order" );
		const ContentManifest* manifest = content.find( "core" );
		CHECK( manifest && manifest->version == "0.9.0",
		       "content registry resolves the validated manifest" );
		CHECK( content.unregisterContent( "missing" ) == ContentUnregistrationError::NotFound &&
		       content.unregisterContent( "core" ) == ContentUnregistrationError::None &&
		       content.find( "core" ) == nullptr && content.manifests().size() == 1 &&
		       content.manifests().front().id == "forward.requirement" &&
		       content.find( "forward.requirement" ) == &content.manifests().front(),
		       "content unregistration preserves manifest order and repairs lookup indices" );
		CHECK( content.registerContent( ContentManifest{ "core", "0.9.1", { 1, 1 } } ) ==
		       ContentRegistrationError::None && content.find( "core" )->version == "0.9.1",
		       "an unregistered content identifier can be registered again" );
	}

	{
		ContentRegistry content( CurrentContentApiVersion );
		CHECK( content.registerContent( ContentManifest{
		       "policy.old-api", "1.0", { 1, 2 }, {}, {{ "policy.optional", "" }} } ) ==
		       ContentRegistrationError::InvalidRelationship,
		       "dependency policy relationships explicitly require content API 1.3" );
		CHECK( content.registerContent( ContentManifest{
		       "policy.duplicate", "1.0", { 1, 3 }, {}, {{ "policy.target", "" }},
		       { "policy.target" } } ) == ContentRegistrationError::InvalidRelationship &&
		       content.registerContent( ContentManifest{
		       "policy.self", "1.0", { 1, 3 }, {}, {}, {}, { "policy.self" } } ) ==
		       ContentRegistrationError::InvalidRelationship,
		       "dependency policy rejects ambiguous cross-list and self relationships" );
		CHECK( content.registerContent( ContentManifest{
		       "policy.valid", "1.0", { 1, 3 }, {}, {{ "policy.optional", "2.0" }},
		       { "policy.conflict" }, { "policy.predecessor" } } ) ==
		       ContentRegistrationError::None,
		       "content API 1.3 accepts validated optional, conflict, and ordering policy" );
	}

	{
		EngineRuntime<unsigned> runtime;
		PackageRegistry& packages = runtime.packages();
		TestLifecyclePackage base( "policy.base", PackageKind::Rules );
		TestLifecyclePackage peer( "policy.peer", PackageKind::Extension );
		TestLifecyclePackage consumer( ContentManifest{
			"policy.consumer", "1.0", { 1, 3 }, {}, {{ "policy.base", "1.0" }},
			{}, { "policy.peer" } }, PackageKind::Extension );
		TestLifecyclePackage absentOptional( ContentManifest{
			"policy.absent", "1.0", { 1, 3 }, {}, {{ "policy.not-installed", "" }} },
			PackageKind::Extension );
		TestLifecyclePackage mismatch( ContentManifest{
			"policy.mismatch", "1.0", { 1, 3 }, {}, {{ "policy.base", "2.0" }} },
			PackageKind::Extension );
		TestLifecyclePackage conflictLeft( ContentManifest{
			"policy.conflict-left", "1.0", { 1, 3 }, {}, {},
			{ "policy.conflict-right" } }, PackageKind::Extension );
		TestLifecyclePackage conflictRight( "policy.conflict-right", PackageKind::Extension );
		TestLifecyclePackage orderA( ContentManifest{
			"policy.order-a", "1.0", { 1, 3 }, {}, {}, {}, { "policy.order-b" } },
			PackageKind::Extension );
		TestLifecyclePackage orderB( ContentManifest{
			"policy.order-b", "1.0", { 1, 3 }, {}, {}, {}, { "policy.order-a" } },
			PackageKind::Extension );
		CHECK( packages.registerPackage( consumer ) == PackageRegistrationError::None &&
		       packages.registerPackage( absentOptional ) == PackageRegistrationError::None &&
		       packages.registerPackage( peer ) == PackageRegistrationError::None &&
		       packages.registerPackage( mismatch ) == PackageRegistrationError::None &&
		       packages.registerPackage( base ) == PackageRegistrationError::None &&
		       packages.registerPackage( conflictLeft ) == PackageRegistrationError::None &&
		       packages.registerPackage( conflictRight ) == PackageRegistrationError::None &&
		       packages.registerPackage( orderA ) == PackageRegistrationError::None &&
		       packages.registerPackage( orderB ) == PackageRegistrationError::None,
		       "dependency policy fixtures register independent of discovery order" );

		const PackageActivationPlan absentPlan = packages.resolveActivation( "policy.absent" );
		const PackageActivationPlan orderedPlan =
			packages.resolveActivation({ "policy.consumer", "policy.peer" });
		const PackageActivationPlan mismatchPlan = packages.resolveActivation( "policy.mismatch" );
		CHECK( absentPlan && absentPlan.order == std::vector<std::string>({ "policy.absent" }) &&
		       orderedPlan && orderedPlan.order == std::vector<std::string>({
		       "policy.base", "policy.peer", "policy.consumer" }) &&
		       mismatchPlan.error == PackageResolutionError::VersionMismatch &&
		       mismatchPlan.diagnosticPath == std::vector<std::string>({
		       "policy.mismatch", "policy.base" }),
		       "optional dependencies are conditional and weak ordering is deterministic" );

		const PackageActivationPlan conflictPlan = packages.resolveActivation({
			"policy.conflict-left", "policy.conflict-right" });
		CHECK( conflictPlan.error == PackageResolutionError::PackageConflict &&
		       conflictPlan.diagnosticPath == std::vector<std::string>({
		       "policy.conflict-left", "policy.conflict-right" }) &&
		       packages.activate( "policy.conflict-left" ) == PackageActivationError::None &&
		       packages.resolveActivation( "policy.conflict-right" ).error ==
		       PackageResolutionError::PackageConflict && packages.deactivate( "policy.conflict-left" ),
		       "declared package conflicts are symmetric for planned and active packages" );

		const PackageActivationPlan weakOnly = packages.resolveActivation( "policy.order-a" );
		const PackageActivationPlan orderingCycle = packages.resolveActivation({
			"policy.order-a", "policy.order-b" });
		CHECK( weakOnly && weakOnly.order == std::vector<std::string>({ "policy.order-a" }) &&
		       orderingCycle.error == PackageResolutionError::OrderingCycle &&
		       orderingCycle.diagnosticPath == std::vector<std::string>({
		       "policy.order-a", "policy.order-b" }),
		       "LOAD_AFTER never selects a package and reports cycles without side effects" );

		const PackageActivationResult activation =
			packages.activateAll({ "policy.consumer", "policy.peer" });
		const PackageDeactivationResult optionalBlocked =
			packages.deactivateDetailed( "policy.base" );
		const PackageCatalogSnapshot policyCatalog = packages.catalog();
		const PackageCatalogEntry* baseEntry = policyCatalog.find( "policy.base" );
		CHECK( activation && activation.activated == orderedPlan.order &&
		       optionalBlocked.error == PackageDeactivationError::RequiredByActivePackage &&
		       optionalBlocked.dependentId == "policy.consumer" && baseEntry &&
		       baseEntry->dependents == std::vector<std::string>({
		       "policy.consumer", "policy.mismatch" }),
		       "present optional dependencies receive lifecycle protection and catalog visibility" );
		CHECK( packages.deactivateAll() && packages.unregisterPackage( "policy.base" ),
		       "an inactive optional dependency can be removed without unregistering its consumer" );
	}

	{
		EngineRuntime<unsigned> runtime;
		PackageRegistry& packages = runtime.packages();
		MemoryAssetSource modAssets( "catalog.mod" );
		TestLifecyclePackage mod(
			"catalog.mod", PackageKind::Extension, -1, &modAssets,
			{{ "catalog.rules", "" }} );
		TestLifecyclePackage campaign( "catalog.campaign", PackageKind::Campaign );
		TestLifecyclePackage rules(
			"catalog.rules", PackageKind::Rules, -1, nullptr,
			{{ "catalog.campaign", "1.0" }} );
		CHECK( packages.registerPackage( mod ) == PackageRegistrationError::None &&
		       packages.registerPackage( campaign ) == PackageRegistrationError::None &&
		       packages.registerPackage( rules ) == PackageRegistrationError::None,
		       "package catalog fixture registers packages independent of dependency order" );
		PackageCatalogSnapshot catalog = runtime.packageCatalog();
		const PackageCatalogEntry* campaignEntry = catalog.find( "catalog.campaign" );
		const PackageCatalogEntry* rulesEntry = catalog.find( "catalog.rules" );
		CHECK( catalog.supportedApi.major == CurrentContentApiVersion.major &&
		       catalog.supportedApi.minor == CurrentContentApiVersion.minor &&
		       catalog.packages.size() == 3 &&
		       catalog.packages[0].descriptor.content.id == "catalog.mod" &&
		       catalog.packages[1].descriptor.content.id == "catalog.campaign" &&
		       catalog.packages[2].descriptor.content.id == "catalog.rules" &&
		       campaignEntry && campaignEntry->dependents ==
		       std::vector<std::string>({ "catalog.rules" }) &&
		       rulesEntry && rulesEntry->dependents ==
		       std::vector<std::string>({ "catalog.mod" }),
		       "package catalog snapshots preserve discovery and dependent order" );
		CHECK( packages.activate( "catalog.mod" ) == PackageActivationError::None &&
		       packages.bootstrap( PackageBootstrapPhase::Configure ) ==
		       PackageBootstrapError::None,
		       "package catalog fixture reaches active bootstrap state" );
		catalog = runtime.packageCatalog();
		const PackageCatalogEntry* modEntry = catalog.find( "catalog.mod" );
		campaignEntry = catalog.find( "catalog.campaign" );
		rulesEntry = catalog.find( "catalog.rules" );
		CHECK( catalog.activationOrder == std::vector<std::string>({
		       "catalog.campaign", "catalog.rules", "catalog.mod" }) &&
		       catalog.activeCampaign == "catalog.campaign" &&
		       catalog.completedBootstrapPhases == 1 && modEntry && modEntry->active() &&
		       modEntry->assetsMounted && modEntry->activationIndex == 2 &&
		       campaignEntry && campaignEntry->active() && campaignEntry->activationIndex == 0 &&
		       rulesEntry && rulesEntry->active() && rulesEntry->activationIndex == 1,
		       "package catalog snapshots expose activation, assets, campaign, and bootstrap state" );
		packages.shutdownBootstrap();
		CHECK( packages.deactivateAll() &&
		       packages.unregisterPackage( "catalog.mod" ) &&
		       catalog.find( "catalog.mod" ) != nullptr &&
		       runtime.packageCatalog().find( "catalog.mod" ) == nullptr,
		       "package catalog snapshots retain values after later registry mutations" );
	}

	{
		ContentRegistry content( CurrentContentApiVersion );
		MemoryPackageEventSink events;
		PackageRegistry packages( content, EngineServices::defaults(), events );
		TestLifecyclePackage dependency( "events.dependency", PackageKind::Rules );
		TestLifecyclePackage failing(
			"events.failing", PackageKind::Extension,
			static_cast<int>( PackageBootstrapPhase::Configure ), nullptr,
			{{ "events.dependency", "" }} );
		CHECK( packages.registerPackage( dependency ) == PackageRegistrationError::None &&
		       packages.registerPackage( failing ) == PackageRegistrationError::None &&
		       packages.activate( "events.failing" ) == PackageActivationError::None &&
		       packages.bootstrap( PackageBootstrapPhase::Configure ) ==
		       PackageBootstrapError::CallbackFailed,
		       "package event fixture observes a failed bootstrapped dependency graph" );
		CHECK( packages.deactivateAll() &&
		       packages.unregisterPackages({ "events.failing", "events.dependency" }),
		       "package event fixture completes package teardown" );
		std::vector<PackageEventKind> eventKinds;
		for ( const PackageEvent& event : events.events() ) eventKinds.push_back( event.kind );
		CHECK( eventKinds == std::vector<PackageEventKind>({
		       PackageEventKind::Registered,
		       PackageEventKind::Registered,
		       PackageEventKind::Activated,
		       PackageEventKind::Activated,
		       PackageEventKind::BootstrapCompleted,
		       PackageEventKind::BootstrapFailed,
		       PackageEventKind::BootstrapRollbackCompleted,
		       PackageEventKind::BootstrapRollbackCompleted,
		       PackageEventKind::Deactivated,
		       PackageEventKind::Deactivated,
		       PackageEventKind::Unregistered,
		       PackageEventKind::Unregistered }) &&
		       events.events()[2].packageId == "events.dependency" &&
		       events.events()[3].packageId == "events.failing" &&
		       !events.events()[3].hasBootstrapPhase() &&
		       events.events()[4].bootstrapPhase == 0 &&
		       events.events()[6].packageId == "events.failing" &&
		       events.events()[7].packageId == "events.dependency",
		       "package events preserve lifecycle, dependency, and rollback order" );
		events.clear();
		TestLifecyclePackage successful( "events.successful", PackageKind::Tool );
		CHECK( packages.registerPackage( successful ) == PackageRegistrationError::None &&
		       packages.activate( "events.successful" ) == PackageActivationError::None &&
		       packages.bootstrap( PackageBootstrapPhase::Configure ) ==
		       PackageBootstrapError::None,
		       "package event fixture reaches a completed bootstrap phase" );
		packages.shutdownBootstrap();
		CHECK( packages.deactivate( "events.successful" ) &&
		       packages.unregisterPackage( "events.successful" ),
		       "package event fixture shuts down a completed bootstrap phase" );
		eventKinds.clear();
		for ( const PackageEvent& event : events.events() ) eventKinds.push_back( event.kind );
		CHECK( eventKinds == std::vector<PackageEventKind>({
		       PackageEventKind::Registered,
		       PackageEventKind::Activated,
		       PackageEventKind::BootstrapCompleted,
		       PackageEventKind::ShutdownCompleted,
		       PackageEventKind::Deactivated,
		       PackageEventKind::Unregistered }) &&
		       events.events()[2].bootstrapPhase == 0 &&
		       events.events()[3].bootstrapPhase == 0,
		       "package events report completed bootstrap shutdown in reverse-lifecycle order" );

		ContentRegistry isolatedContent( CurrentContentApiVersion );
		ThrowingPackageEventSink throwingEvents;
		PackageRegistry isolatedPackages(
			isolatedContent, EngineServices::defaults(), throwingEvents );
		TestLifecyclePackage isolated( "events.isolated", PackageKind::Tool );
		CHECK( isolatedPackages.registerPackage( isolated ) == PackageRegistrationError::None &&
		       isolatedPackages.activate( "events.isolated" ) == PackageActivationError::None &&
		       isolatedPackages.deactivate( "events.isolated" ) &&
		       isolatedPackages.unregisterPackage( "events.isolated" ) &&
		       throwingEvents.calls == 4,
		       "package event sink exceptions cannot corrupt lifecycle state" );
	}

	{
		ContentRegistry content( CurrentContentApiVersion );
		PackageRegistry packages( content );
		TestLifecyclePackage dependency( "unregister.dependency", PackageKind::Rules );
		TestLifecyclePackage consumer(
			"unregister.consumer", PackageKind::Extension, -1, nullptr,
			{{ "unregister.dependency", "" }} );
		CHECK( packages.registerPackage( dependency ) == PackageRegistrationError::None &&
		       packages.registerPackage( consumer ) == PackageRegistrationError::None,
		       "package unregistration fixture registers its dependency graph" );
		CHECK( packages.unregisterPackage( "unregister.missing" ).error ==
		       PackageUnregistrationError::NotFound,
		       "package unregistration reports unknown identifiers" );
		CHECK( packages.activate( "unregister.consumer" ) == PackageActivationError::None &&
		       packages.unregisterPackage( "unregister.consumer" ).error ==
		       PackageUnregistrationError::Active,
		       "active packages cannot be unregistered" );
		CHECK( packages.deactivate( "unregister.consumer" ) &&
		       packages.unregisterPackage( "unregister.dependency" ).error ==
		       PackageUnregistrationError::Active,
		       "an active dependency remains protected after its consumer deactivates" );
		CHECK( packages.deactivate( "unregister.dependency" ),
		       "package unregistration fixture deactivates its dependency" );
		const PackageUnregistrationResult blocked =
			packages.unregisterPackage( "unregister.dependency" );
		CHECK( blocked.error == PackageUnregistrationError::RequiredByRegisteredPackage &&
		       blocked.packageId == "unregister.dependency" &&
		       blocked.dependentId == "unregister.consumer",
		       "registered dependents prevent unsafe package removal" );
		CHECK( packages.unregisterPackage( "unregister.consumer" ) &&
		       packages.unregisterPackage( "unregister.dependency" ) &&
		       packages.find( "unregister.consumer" ) == nullptr &&
		       packages.find( "unregister.dependency" ) == nullptr &&
		       content.find( "unregister.consumer" ) == nullptr &&
		       content.find( "unregister.dependency" ) == nullptr,
		       "package unregistration removes both registry and content membership" );
		CHECK( packages.registerPackage( dependency ) == PackageRegistrationError::None,
		       "an unregistered package object can be registered again" );
	}

	{
		ContentRegistry content( CurrentContentApiVersion );
		PackageRegistry packages( content );
		TestLifecyclePackage cycleA(
			"unregister-cycle.a", PackageKind::Extension, -1, nullptr,
			{{ "unregister-cycle.b", "" }} );
		TestLifecyclePackage cycleB(
			"unregister-cycle.b", PackageKind::Extension, -1, nullptr,
			{{ "unregister-cycle.a", "" }} );
		TestLifecyclePackage external(
			"unregister-cycle.external", PackageKind::Extension, -1, nullptr,
			{{ "unregister-cycle.a", "" }} );
		CHECK( packages.registerPackage( cycleA ) == PackageRegistrationError::None &&
		       packages.registerPackage( cycleB ) == PackageRegistrationError::None &&
		       packages.registerPackage( external ) == PackageRegistrationError::None,
		       "batch unregistration fixture registers a cyclic internal graph" );
		const PackageUnregistrationBatchResult blocked = packages.unregisterPackages({
			"unregister-cycle.a", "unregister-cycle.b" });
		CHECK( blocked.error == PackageUnregistrationError::RequiredByRegisteredPackage &&
		       blocked.packageId == "unregister-cycle.a" &&
		       blocked.dependentId == "unregister-cycle.external",
		       "batch unregistration rejects dependents outside the transaction" );
		CHECK( packages.unregisterPackage( "unregister-cycle.external" ),
		       "external batch blocker can be removed independently" );
		const PackageUnregistrationBatchResult removed = packages.unregisterPackages({
			"unregister-cycle.a", "unregister-cycle.b" });
		CHECK( removed && removed.unregistered == std::vector<std::string>({
		       "unregister-cycle.a", "unregister-cycle.b" }) &&
		       packages.find( "unregister-cycle.a" ) == nullptr &&
		       packages.find( "unregister-cycle.b" ) == nullptr &&
		       content.manifests().empty(),
		       "batch unregistration removes a complete internal dependency cycle atomically" );
		CHECK( packages.unregisterPackages({}).unregistered.empty(),
		       "empty batch unregistration is an idempotent transaction" );
	}

	{
		ContentRegistry content( CurrentContentApiVersion );
		PackageRegistry packages( content );
		TestLifecyclePackage base( "deactivate.base", PackageKind::Rules );
		TestLifecyclePackage middle(
			"deactivate.middle", PackageKind::Extension, -1, nullptr,
			{{ "deactivate.base", "" }} );
		TestLifecyclePackage leaf(
			"deactivate.leaf", PackageKind::Extension, -1, nullptr,
			{{ "deactivate.middle", "" }} );
		std::vector<std::string> callbacks;
		base.deactivationTrace = &callbacks;
		middle.deactivationTrace = &callbacks;
		leaf.deactivationTrace = &callbacks;
		CHECK( packages.registerPackage( leaf ) == PackageRegistrationError::None &&
		       packages.registerPackage( base ) == PackageRegistrationError::None &&
		       packages.registerPackage( middle ) == PackageRegistrationError::None &&
		       packages.activate( "deactivate.leaf" ) == PackageActivationError::None,
		       "bulk package deactivation fixture activates a dependency chain" );
		CHECK( packages.bootstrap( PackageBootstrapPhase::Configure ) ==
		       PackageBootstrapError::None && packages.deactivateAll().error ==
		       PackageDeactivationError::BootstrapInProgress,
		       "bulk package deactivation cannot invalidate live bootstrap resources" );
		packages.shutdownBootstrap();
		const PackageDeactivationBatchResult teardown = packages.deactivateAll();
		CHECK( teardown && teardown.packageId.empty() &&
		       teardown.deactivated == std::vector<std::string>({
		       "deactivate.leaf", "deactivate.middle", "deactivate.base" }) &&
		       callbacks == teardown.deactivated && packages.activationOrder().empty() &&
		       !base.active() && !middle.active() && !leaf.active(),
		       "bulk package deactivation unwinds exact reverse activation order" );
		CHECK( packages.deactivateAll() && packages.activationOrder().empty(),
		       "bulk package deactivation is idempotent for an empty active set" );
	}

	{
		MemoryAssetSource campaignAssets( "campaign.arulco" );
		MemoryAssetSource modAssets( "mod.example" );
		CHECK( campaignAssets.put( "Data/Items.XML", { 1 } ) &&
		       campaignAssets.put( "Data/Maps/A9.dat", { 9 } ) &&
		       campaignAssets.put( "Data/Empty.bin", {} ) &&
		       modAssets.put( "data/items.xml", { 2 } ) &&
		       modAssets.put( "DATA//./ITEMS.xml", { 3 } ),
		       "memory asset sources register deterministic fixtures" );
		CompositeAssetSource layeredAssets;
		CHECK( layeredAssets.mount( "campaign.arulco", campaignAssets ) &&
		       layeredAssets.mount( "mod.example", modAssets ) &&
		       !layeredAssets.mount( "mod.duplicate", modAssets ) &&
		       !layeredAssets.mount( "self", layeredAssets ),
		       "asset overlay has deterministic unique mount order" );
		AssetData asset;
		CHECK( layeredAssets.read( "Data/Items.xml", asset ) == AssetReadResult::Success &&
		       asset.logicalPath == "data/items.xml" && asset.bytes[0] == 3 &&
		       asset.provenance == "mod.example",
		       "later package assets override case-insensitively with trusted provenance" );
		CHECK( layeredAssets.read( "Data/Maps/A9.dat", asset ) == AssetReadResult::Success &&
		       asset.bytes[0] == 9 &&
		       asset.provenance == "campaign.arulco",
		       "asset overlay falls back to lower-priority campaign content" );
		CHECK( layeredAssets.read( "data/empty.bin", asset ) == AssetReadResult::Success &&
		       asset.bytes.empty(), "asset source preserves empty assets" );
		CompositeAssetSource hostLayer( campaignAssets );
		CHECK( !hostLayer.unmount( "" ) &&
		       hostLayer.read( "Data/Items.xml", asset ) == AssetReadResult::Success &&
		       asset.provenance == "campaign.arulco" && asset.bytes[0] == 1,
		       "asset overlays preserve immutable host fallback and its provenance" );

		CompositeAssetSource firstComposite;
		CompositeAssetSource secondComposite;
		CHECK( firstComposite.mount( "second", secondComposite ) &&
		       !secondComposite.mount( "first", firstComposite ),
		       "asset overlays reject transitive mount cycles" );

		FailingAssetSource brokenOverride;
		CompositeAssetSource corruptedAssets;
		CHECK( corruptedAssets.mount( "campaign.arulco", campaignAssets ) &&
		       corruptedAssets.mount( "mod.broken", brokenOverride ) &&
		       corruptedAssets.read( "data/items.xml", asset ) == AssetReadResult::IoError &&
		       asset.logicalPath.empty() && asset.bytes.empty(),
		       "broken high-priority assets never resurrect lower-priority content" );

		asset = AssetData{ "stale", "stale", { 7 } };
		const std::string controlPath = std::string("data/") + static_cast<char>(1) + "bad";
		CHECK( layeredAssets.read( "../outside.bin", asset ) == AssetReadResult::InvalidPath &&
		       asset.logicalPath.empty() && asset.provenance.empty() && asset.bytes.empty() &&
		       layeredAssets.read( "C:\\absolute.bin", asset ) == AssetReadResult::InvalidPath &&
		       layeredAssets.read( "\\\\server\\asset", asset ) == AssetReadResult::InvalidPath &&
		       layeredAssets.read( controlPath, asset ) == AssetReadResult::InvalidPath,
		       "invalid asset reads reject traversal, drives, UNC, and control characters cleanly" );
		CHECK( layeredAssets.unmount( "mod.example" ) &&
		       layeredAssets.read( "data/items.xml", asset ) == AssetReadResult::Success &&
		       asset.bytes[0] == 1 && !layeredAssets.unmount( "mod.example" ),
		       "asset mounts can be removed before package destruction" );
	}

	{
		ContentRegistry content( ContentApiVersion{ 1, 0 } );
		LegacyCampaignPackage arulco( GameCapabilities{} );
		GameCapabilities ubCapabilities;
		ubCapabilities.campaign = GameCampaign::UnfinishedBusiness;
		LegacyCampaignPackage unfinishedBusiness( ubCapabilities );
		PackageRegistry packages( content );
		CHECK( packages.registerPackage( arulco ) == PackageRegistrationError::None &&
		       packages.registerPackage( unfinishedBusiness ) == PackageRegistrationError::None,
		       "campaign packages register through the versioned engine API" );
		CHECK( packages.activate( "ja2.arulco" ) == PackageActivationError::None && arulco.active() &&
		       packages.hasCapability( GameCapability::CampaignArulco ) &&
		       !packages.hasCapability( GameCapability::CampaignUnfinishedBusiness ),
		       "campaign package activation is selected at runtime" );
		CHECK( packages.activate( "ja2.unfinished-business" ) ==
		       PackageActivationError::CampaignAlreadyActive,
		       "package registry prevents conflicting active campaigns" );
		CHECK( packages.deactivate( "ja2.arulco" ) &&
		       packages.activate( "ja2.unfinished-business" ) == PackageActivationError::None &&
		       unfinishedBusiness.active() &&
		       !packages.hasCapability( GameCapability::CampaignArulco ) &&
		       packages.hasCapability( GameCapability::CampaignUnfinishedBusiness ) &&
		       packages.catalog().activeCapabilities.contains(
		           GameCapability::CampaignUnfinishedBusiness ),
		       "campaign packages can be switched without compile-time selection" );
		TestLifecyclePackage invalidCapabilities(
			"rules.invalid-capability", PackageKind::Rules, -1, nullptr, {}, "1.0",
			{ "invalid/capability" } );
		CHECK( packages.registerPackage( invalidCapabilities ) ==
		       PackageRegistrationError::InvalidManifest,
		       "package registration rejects non-portable capability declarations" );
	}

	{
		GameContext& compiledContext = GetGameContext();
		LegacyCampaignPackage& compiledPackage = GetCompiledCampaignPackage();
		const std::string& packageId = compiledPackage.descriptor().content.id;
		CHECK( compiledContext.packages().activeCampaign() == packageId &&
		       compiledContext.packages().isActive( packageId ) && compiledPackage.active() &&
		       compiledContext.hasCapability(
		           compiledContext.capabilities().isUnfinishedBusiness()
		               ? GameCapability::CampaignUnfinishedBusiness
		               : GameCapability::CampaignArulco ),
		       "legacy compiled campaign is bound through the runtime package registry" );
		CHECK( compiledPackage.capabilities().campaign == compiledContext.capabilities().campaign,
		       "campaign adapter preserves the compiled JA2 or UB compatibility default" );
		CHECK( &compiledContext.log() == &GetPlatformLogSink(),
		       "application composition root binds the SDL logging adapter" );
		CHECK( &compiledContext.services().time == &GetPlatformTimeSource() &&
		       &compiledContext.services().random == &GetGameRandomSource() &&
		       &compiledContext.services().storage == &GetPlatformByteStorage() &&
		       &compiledContext.services().input == &GetPlatformInputSource() &&
		       &compiledContext.services().audio == &GetPlatformAudioOutput() &&
		       &compiledContext.services().frames == &GetPlatformFramePresenter() &&
		       &compiledContext.services().assets == &compiledContext.packages().assets() &&
		       &compiledContext.persistence().storage() == &GetPlatformByteStorage() &&
		       compiledContext.services().assets.containsSource( &GetPlatformAssetSource() ),
		       "application composition root binds platform service adapters" );
	}

	{
		MemoryAssetSource hostAssets( "host.vfs" );
		MemoryAssetSource campaignAssets( "forged.campaign" );
		MemoryAssetSource modAssets( "forged.mod" );
		MemoryAssetSource invalidAssets( "forged.invalid" );
		CHECK( hostAssets.put( "Data/Rules/weapons.bin", { 0 } ) &&
		       hostAssets.put( "Data/Host/only.bin", { 9 } ) &&
		       campaignAssets.put( "Data/Rules/weapons.bin", { 1 } ) &&
		       modAssets.put( "Data/Rules/weapons.bin", { 2 } ) &&
		       invalidAssets.put( "Data/Rules/weapons.bin", { 3 } ),
		       "package lifecycle fixtures expose overlapping asset layers" );

		TestLifecyclePackage sourceLess( "engine.host", PackageKind::Extension );
		TestLifecyclePackage campaign( "campaign.assets", PackageKind::Campaign,
		                                -1, &campaignAssets );
		TestLifecyclePackage mod( "mod.assets", PackageKind::Rules, -1, &modAssets );
		TestLifecyclePackage duplicate( "mod.duplicate", PackageKind::Extension,
		                                 -1, &modAssets );
		TestLifecyclePackage invalid( "mod/assets", PackageKind::Extension,
		                               -1, &invalidAssets );
		MemoryLogSink lifecycleLog;
		EngineServices defaults = EngineServices::defaults();
		EngineServices services{defaults.time, defaults.random, defaults.storage, lifecycleLog,
		                        defaults.input, defaults.audio, defaults.frames, hostAssets};
		EngineRuntime<unsigned> runtime( services );
		PackageRegistry& packages = runtime.packages();
		campaign.registryDuringBootstrap = &packages;
		campaign.activateDuringBootstrap = "mod.duplicate";
		campaign.deactivateDuringBootstrap = "mod.assets";
		mod.registryDuringDeactivate = &packages;
		mod.deactivateDuringDeactivate = "campaign.assets";
		CHECK( packages.registerPackage( sourceLess ) == PackageRegistrationError::None &&
		       packages.registerPackage( campaign ) == PackageRegistrationError::None &&
		       packages.registerPackage( mod ) == PackageRegistrationError::None &&
		       packages.registerPackage( duplicate ) == PackageRegistrationError::None,
		       "asset-bearing and source-less packages register without mounting content" );
		CHECK( packages.registerPackage( invalid ) == PackageRegistrationError::InvalidManifest &&
		       packages.find( "mod/assets" ) == nullptr && invalid.activateCalls == 0,
		       "package registration rejects IDs that cannot become trusted provenance" );

		AssetData asset;
		CHECK( &runtime.services() == &packages.services() &&
		       &runtime.services().assets == &packages.assets() &&
		       packages.assets().containsSource( &hostAssets ) &&
		       runtime.services().assets.read( "Data/Rules/weapons.bin", asset ) ==
		       AssetReadResult::Success && asset.bytes == std::vector<std::uint8_t>({ 0 }) &&
		       asset.provenance == "host.vfs",
		       "engine runtime exposes the registry asset overlay with its host fallback" );
		CHECK( packages.activate( "engine.host" ) == PackageActivationError::None &&
		       packages.activate( "campaign.assets" ) == PackageActivationError::None &&
		       packages.activate( "mod.assets" ) == PackageActivationError::None &&
		       runtime.services().assets.read( "Data/Rules/weapons.bin", asset ) ==
		       AssetReadResult::Success && asset.bytes == std::vector<std::uint8_t>({ 2 }) &&
		       asset.provenance == "mod.assets" &&
		       runtime.services().assets.read( "Data/Host/only.bin", asset ) ==
		       AssetReadResult::Success && asset.bytes == std::vector<std::uint8_t>({ 9 }),
		       "package activation overlays assets in deterministic activation order" );
		CHECK( packages.activate( "mod.duplicate" ) == PackageActivationError::AssetMountFailed &&
		       !duplicate.active() && duplicate.activateCalls == 1 && duplicate.deactivateCalls == 1 &&
		       !packages.isActive( "mod.duplicate" ) && packages.activationOrder().size() == 3 &&
		       packages.activeCampaign() == "campaign.assets" && lifecycleLog.records().size() == 1 &&
		       runtime.services().assets.read( "Data/Rules/weapons.bin", asset ) ==
		       AssetReadResult::Success && asset.provenance == "mod.assets",
		       "asset mount failures roll back package activation without changing active state" );
		CHECK( packages.bootstrap( PackageBootstrapPhase::Configure ) == PackageBootstrapError::None &&
		       campaign.observedServices == &runtime.services() &&
		       campaign.observedAssetProvenance == "mod.assets" &&
		       mod.observedAssetProvenance == "mod.assets" &&
		       campaign.nestedResolutionResult.error ==
		       PackageResolutionError::OperationInProgress &&
		       campaign.nestedActivationResult == PackageActivationError::OperationInProgress &&
		       !campaign.nestedBootstrapDeactivationResult,
		       "package bootstrap sees composed services and rejects reentrant lifecycle changes" );
		CHECK( !packages.deactivate( "mod.assets" ) &&
		       runtime.services().assets.read( "Data/Rules/weapons.bin", asset ) ==
		       AssetReadResult::Success && asset.provenance == "mod.assets",
		       "bootstrap resources freeze package membership and keep asset mounts intact" );
		packages.shutdownBootstrap();
		const bool modDeactivated = packages.deactivate( "mod.assets" );
		const AssetReadResult afterMod =
			runtime.services().assets.read( "Data/Rules/weapons.bin", asset );
		CHECK( modDeactivated && !mod.nestedDeactivateResult &&
		       packages.isActive( "campaign.assets" ) && afterMod == AssetReadResult::Success &&
		       asset.bytes == std::vector<std::uint8_t>({ 1 }) &&
		       asset.provenance == "campaign.assets",
		       "deactivating a mod restores the campaign asset layer" );
		const bool campaignDeactivated = packages.deactivate( "campaign.assets" );
		const AssetReadResult afterCampaign =
			runtime.services().assets.read( "Data/Rules/weapons.bin", asset );
		CHECK( campaignDeactivated && afterCampaign == AssetReadResult::Success &&
		       asset.bytes == std::vector<std::uint8_t>({ 0 }) && asset.provenance == "host.vfs",
		       "deactivating a campaign restores the host asset layer" );
		const bool sourceLessDeactivated = packages.deactivate( "engine.host" );
		const AssetReadResult afterSourceLess =
			runtime.services().assets.read( "Data/Rules/weapons.bin", asset );
		CHECK( sourceLessDeactivated && afterSourceLess == AssetReadResult::Success &&
		       asset.bytes == std::vector<std::uint8_t>({ 0 }) &&
		       asset.provenance == "host.vfs" && packages.activationOrder().empty() &&
		       packages.activeCampaign().empty() && !sourceLess.active() &&
		       !campaign.active() && !mod.active(),
		       "source-less package teardown cannot remove the immutable host layer" );
	}

	{
		MemoryAssetSource hostAssets( "graph.host" );
		MemoryAssetSource campaignAssets( "forged.graph.campaign" );
		MemoryAssetSource rulesAssets( "forged.graph.rules" );
		MemoryAssetSource modAssets( "forged.graph.mod" );
		MemoryAssetSource siblingAssets( "forged.graph.sibling" );
		ThrowingContainsAssetSource throwingAssets;
		CHECK( hostAssets.put( "Data/Graph/value.bin", { 0 } ) &&
		       campaignAssets.put( "Data/Graph/value.bin", { 1 } ) &&
		       rulesAssets.put( "Data/Graph/value.bin", { 2 } ) &&
		       modAssets.put( "Data/Graph/value.bin", { 3 } ) &&
		       siblingAssets.put( "Data/Graph/value.bin", { 4 } ),
		       "dependency graph fixtures expose ordered asset overrides" );

		TestLifecyclePackage campaign( "graph.campaign", PackageKind::Campaign,
		                                -1, &campaignAssets );
		TestLifecyclePackage rules( "graph.rules", PackageKind::Rules, -1, &rulesAssets,
		                             {{ "graph.campaign", "1.0" }} );
		TestLifecyclePackage mod( "graph.mod", PackageKind::Extension, -1, &modAssets,
		                           {{ "graph.rules", "" }} );
		TestLifecyclePackage sibling( "graph.sibling", PackageKind::Extension, -1, &siblingAssets,
		                               {{ "graph.rules", "1.0" }} );
		TestLifecyclePackage missing( "graph.missing", PackageKind::Extension, -1, nullptr,
		                               {{ "graph.absent", "" }} );
		TestLifecyclePackage mismatch( "graph.mismatch", PackageKind::Extension, -1, nullptr,
		                                {{ "graph.rules", "2.0" }} );
		TestLifecyclePackage cycleA( "graph.cycle-a", PackageKind::Extension, -1, nullptr,
		                              {{ "graph.cycle-b", "" }} );
		TestLifecyclePackage cycleB( "graph.cycle-b", PackageKind::Extension, -1, nullptr,
		                              {{ "graph.cycle-a", "" }} );
		TestLifecyclePackage otherCampaign( "graph.other-campaign", PackageKind::Campaign );
		TestLifecyclePackage failing( "graph.failing", PackageKind::Extension, -1, nullptr,
		                              {{ "graph.rules", "" }} );
		failing.activationSucceeds = false;
		TestLifecyclePackage assetFail( "graph.asset-fail", PackageKind::Extension,
		                                -1, &rulesAssets, {{ "graph.rules", "" }} );
		TestLifecyclePackage throwingAsset( "graph.throwing-asset", PackageKind::Extension,
		                                    -1, &throwingAssets, {{ "graph.rules", "" }} );
		TestLifecyclePackage invalidSelf( "graph.self", PackageKind::Extension, -1, nullptr,
		                                  {{ "graph.self", "" }} );
		TestLifecyclePackage invalidDuplicate( "graph.duplicate", PackageKind::Extension,
		                                       -1, nullptr,
		                                       {{ "graph.rules", "" }, { "graph.rules", "1.0" }} );
		TestLifecyclePackage invalidRequirementId( "graph.invalid-requirement",
		                                           PackageKind::Extension, -1, nullptr,
		                                           {{ "graph/bad", "" }} );

		TestLifecyclePackage diamondBase( "diamond.base", PackageKind::Rules );
		TestLifecyclePackage diamondLeft( "diamond.left", PackageKind::Extension, -1, nullptr,
		                                    {{ "diamond.base", "" }} );
		TestLifecyclePackage diamondRight( "diamond.right", PackageKind::Extension, -1, nullptr,
		                                     {{ "diamond.base", "" }} );
		TestLifecyclePackage diamondRoot( "diamond.root", PackageKind::Tool, -1, nullptr,
		                                    {{ "diamond.left", "" }, { "diamond.right", "" }} );

		MemoryLogSink graphLog;
		EngineServices defaults = EngineServices::defaults();
		EngineServices services{defaults.time, defaults.random, defaults.storage, graphLog,
		                        defaults.input, defaults.audio, defaults.frames, hostAssets};
		EngineRuntime<unsigned> runtime( services );
		PackageRegistry& packages = runtime.packages();

		const PackageRegistrationError registerMod = packages.registerPackage( mod );
		const PackageRegistrationError registerCycleB = packages.registerPackage( cycleB );
		const PackageRegistrationError registerRules = packages.registerPackage( rules );
		const PackageRegistrationError registerMissing = packages.registerPackage( missing );
		const PackageRegistrationError registerOther = packages.registerPackage( otherCampaign );
		const PackageRegistrationError registerMismatch = packages.registerPackage( mismatch );
		const PackageRegistrationError registerCycleA = packages.registerPackage( cycleA );
		const PackageRegistrationError registerSibling = packages.registerPackage( sibling );
		const PackageRegistrationError registerFailing = packages.registerPackage( failing );
		const PackageRegistrationError registerAssetFail = packages.registerPackage( assetFail );
		const PackageRegistrationError registerThrowingAsset =
			packages.registerPackage( throwingAsset );
		const PackageRegistrationError registerCampaign = packages.registerPackage( campaign );
		const PackageRegistrationError registerDiamondRoot = packages.registerPackage( diamondRoot );
		const PackageRegistrationError registerDiamondRight = packages.registerPackage( diamondRight );
		const PackageRegistrationError registerDiamondLeft = packages.registerPackage( diamondLeft );
		const PackageRegistrationError registerDiamondBase = packages.registerPackage( diamondBase );
		CHECK( registerMod == PackageRegistrationError::None &&
		       registerCycleB == PackageRegistrationError::None &&
		       registerRules == PackageRegistrationError::None &&
		       registerMissing == PackageRegistrationError::None &&
		       registerOther == PackageRegistrationError::None &&
		       registerMismatch == PackageRegistrationError::None &&
		       registerCycleA == PackageRegistrationError::None &&
		       registerSibling == PackageRegistrationError::None &&
		       registerFailing == PackageRegistrationError::None &&
		       registerAssetFail == PackageRegistrationError::None &&
		       registerThrowingAsset == PackageRegistrationError::None &&
		       registerCampaign == PackageRegistrationError::None &&
		       registerDiamondRoot == PackageRegistrationError::None &&
		       registerDiamondRight == PackageRegistrationError::None &&
		       registerDiamondLeft == PackageRegistrationError::None &&
		       registerDiamondBase == PackageRegistrationError::None,
		       "packages register independently of dependency discovery order" );
		CHECK( packages.registerPackage( invalidSelf ) == PackageRegistrationError::InvalidRequirement &&
		       packages.registerPackage( invalidDuplicate ) ==
		       PackageRegistrationError::InvalidRequirement &&
		       packages.registerPackage( invalidRequirementId ) ==
		       PackageRegistrationError::InvalidRequirement &&
		       packages.find( "graph.self" ) == nullptr &&
		       packages.find( "graph.duplicate" ) == nullptr &&
		       packages.find( "graph.invalid-requirement" ) == nullptr,
		       "package registration rejects malformed requirement graphs transactionally" );

		const PackageActivationPlan diamondPlan = packages.resolveActivation( "diamond.root" );
		CHECK( diamondPlan && diamondPlan.order == std::vector<std::string>({
		       "diamond.base", "diamond.left", "diamond.right", "diamond.root" }),
		       "dependency planning is deterministic, topological, and de-duplicates diamonds" );
		CHECK( packages.resolveActivation( std::vector<std::string>{} ).error ==
		       PackageResolutionError::EmptyRequest &&
		       packages.activateAll({}).error == PackageActivationError::InvalidRequest,
		       "empty dependency requests are rejected without lifecycle work" );
		const PackageActivationPlan missingPlan = packages.resolveActivation( "graph.missing" );
		const PackageActivationPlan mismatchPlan = packages.resolveActivation( "graph.mismatch" );
		const PackageActivationPlan cyclePlan = packages.resolveActivation( "graph.cycle-a" );
		CHECK( missingPlan.error == PackageResolutionError::MissingRequirement &&
		       missingPlan.packageId == "graph.absent" &&
		       missingPlan.diagnosticPath ==
		       std::vector<std::string>({ "graph.missing", "graph.absent" }) &&
		       mismatchPlan.error == PackageResolutionError::VersionMismatch &&
		       mismatchPlan.packageId == "graph.rules" &&
		       mismatchPlan.diagnosticPath == std::vector<std::string>({
		       "graph.mismatch", "graph.rules" }) &&
		       cyclePlan.error == PackageResolutionError::DependencyCycle &&
		       cyclePlan.diagnosticPath == std::vector<std::string>({
		       "graph.cycle-a", "graph.cycle-b", "graph.cycle-a" }) &&
		       missing.activateCalls == 0 && mismatch.activateCalls == 0 &&
		       cycleA.activateCalls == 0 && cycleB.activateCalls == 0,
		       "dependency preflight reports paths without invoking package callbacks" );
		const PackageActivationResult batchPreflight =
			packages.activateAll({ "diamond.root", "graph.missing" });
		CHECK( batchPreflight.error == PackageActivationError::MissingRequirement &&
		       batchPreflight.packageId == "graph.absent" &&
		       batchPreflight.diagnosticPath == std::vector<std::string>({
		       "graph.missing", "graph.absent" }) && batchPreflight.activated.empty() &&
		       diamondBase.activateCalls == 0 && diamondLeft.activateCalls == 0 &&
		       diamondRight.activateCalls == 0 && diamondRoot.activateCalls == 0 &&
		       packages.activationOrder().empty(),
		       "multi-root graph errors preserve diagnostics and preflight the whole batch" );

		const PackageActivationPlan inactiveCampaignConflict =
			packages.resolveActivation({ "graph.campaign", "graph.other-campaign" });
		CHECK( inactiveCampaignConflict.error == PackageResolutionError::CampaignConflict &&
		       inactiveCampaignConflict.packageId == "graph.other-campaign" &&
		       inactiveCampaignConflict.diagnosticPath == std::vector<std::string>({
		       "graph.campaign", "graph.other-campaign" }) &&
		       inactiveCampaignConflict.order.empty() && campaign.activateCalls == 0 &&
		       otherCampaign.activateCalls == 0,
		       "a batch cannot plan two inactive campaigns" );

		CHECK( packages.activate( "graph.other-campaign" ) == PackageActivationError::None,
		       "dependency test activates an existing conflicting campaign" );
		const PackageActivationPlan conflictPlan = packages.resolveActivation( "graph.mod" );
		CHECK( conflictPlan.error == PackageResolutionError::CampaignConflict &&
		       conflictPlan.diagnosticPath == std::vector<std::string>({
		       "graph.other-campaign", "graph.campaign" }) &&
		       packages.activate( "graph.mod" ) == PackageActivationError::CampaignAlreadyActive &&
		       campaign.activateCalls == 0 && rules.activateCalls == 0 && mod.activateCalls == 0,
		       "transitive campaign conflicts fail before dependency side effects" );
		CHECK( packages.deactivate( "graph.other-campaign" ),
		       "dependency test removes the conflicting campaign" );

		std::vector<std::string> lifecycleTrace;
		campaign.lifecycleTrace = &lifecycleTrace;
		rules.lifecycleTrace = &lifecycleTrace;
		mod.lifecycleTrace = &lifecycleTrace;
		sibling.lifecycleTrace = &lifecycleTrace;
		const PackageActivationResult activation =
			packages.activateAll({ "graph.mod", "graph.sibling" });
		AssetData graphAsset;
		CHECK( activation && activation.activated == std::vector<std::string>({
		       "graph.campaign", "graph.rules", "graph.mod", "graph.sibling" }) &&
		       packages.activationOrder() == activation.activated &&
		       packages.activeCampaign() == "graph.campaign" &&
		       runtime.services().assets.read( "Data/Graph/value.bin", graphAsset ) ==
		       AssetReadResult::Success && graphAsset.bytes == std::vector<std::uint8_t>({ 4 }) &&
		       graphAsset.provenance == "graph.sibling" && campaign.activateCalls == 1 &&
		       rules.activateCalls == 1 && mod.activateCalls == 1 && sibling.activateCalls == 1,
		       "multi-root activation mounts dependencies and requested roots from low to high priority" );
		const PackageActivationResult idempotentBatch =
			packages.activateAll({ "graph.mod", "graph.sibling" });
		const PackageActivationPlan activeMismatch = packages.resolveActivation( "graph.mismatch" );
		CHECK( idempotentBatch && idempotentBatch.activated.empty() &&
		       packages.activate( "graph.mod" ) == PackageActivationError::AlreadyActive &&
		       activeMismatch.error == PackageResolutionError::VersionMismatch &&
		       activeMismatch.diagnosticPath == std::vector<std::string>({
		       "graph.mismatch", "graph.rules" }) && mismatch.activateCalls == 0,
		       "batch activation is idempotent while legacy activation and exact versions stay strict" );
		CHECK( packages.bootstrap( PackageBootstrapPhase::Configure ) == PackageBootstrapError::None,
		       "dependency-ordered packages bootstrap successfully" );
		packages.shutdownBootstrap();
		CHECK( lifecycleTrace == std::vector<std::string>({
		       "bootstrap:graph.campaign", "bootstrap:graph.rules", "bootstrap:graph.mod",
		       "bootstrap:graph.sibling", "shutdown:graph.sibling", "shutdown:graph.mod",
		       "shutdown:graph.rules", "shutdown:graph.campaign" }),
		       "bootstrap follows dependency order and shutdown reverses it" );

		const PackageDeactivationResult campaignBlocked =
			packages.deactivateDetailed( "graph.campaign" );
		const PackageDeactivationResult rulesBlocked =
			packages.deactivateDetailed( "graph.rules" );
		CHECK( campaignBlocked.error == PackageDeactivationError::RequiredByActivePackage &&
		       campaignBlocked.dependentId == "graph.rules" &&
		       rulesBlocked.error == PackageDeactivationError::RequiredByActivePackage &&
		       rulesBlocked.dependentId == "graph.mod",
		       "active dependents protect their transitive package closure" );
		const bool removedMod = packages.deactivate( "graph.mod" );
		const PackageDeactivationResult rulesStillRequired =
			packages.deactivateDetailed( "graph.rules" );
		const bool removedSibling = packages.deactivate( "graph.sibling" );
		const bool removedRules = packages.deactivate( "graph.rules" );
		const AssetReadResult afterConsumers =
			runtime.services().assets.read( "Data/Graph/value.bin", graphAsset );
		const bool removedCampaign = packages.deactivate( "graph.campaign" );
		CHECK( removedMod &&
		       rulesStillRequired.dependentId == "graph.sibling" && removedSibling &&
		       removedRules && afterConsumers == AssetReadResult::Success &&
		       graphAsset.provenance == "graph.campaign" && removedCampaign &&
		       packages.activationOrder().empty(),
		       "dependencies remain active until every consumer is explicitly removed" );

		campaign.lifecycleTrace = nullptr;
		rules.lifecycleTrace = nullptr;
		mod.lifecycleTrace = nullptr;
		sibling.lifecycleTrace = nullptr;
		CHECK( packages.activate( "graph.campaign" ) == PackageActivationError::None,
		       "transaction test preserves an explicitly active dependency" );
		const PackageActivationResult failedActivation =
			packages.activateAll({ "graph.failing" });
		CHECK( failedActivation.error == PackageActivationError::ActivationFailed &&
		       failedActivation.packageId == "graph.failing" && failedActivation.activated.empty() &&
		       packages.activationOrder() == std::vector<std::string>({ "graph.campaign" }) &&
		       packages.isActive( "graph.campaign" ) && !packages.isActive( "graph.rules" ) &&
		       !packages.isActive( "graph.failing" ) && campaign.activateCalls == 2 &&
		       rules.activateCalls == 2 && rules.deactivateCalls == 2 &&
		       failing.activateCalls == 1 && failing.deactivateCalls == 0 &&
		       runtime.services().assets.read( "Data/Graph/value.bin", graphAsset ) ==
		       AssetReadResult::Success && graphAsset.provenance == "graph.campaign",
		       "failed activation rolls back only packages newly activated by the request" );
		CHECK( packages.deactivate( "graph.campaign" ) && packages.activationOrder().empty(),
		       "callback-failure transaction fixture tears down cleanly" );

		const PackageActivationResult mountFailure =
			packages.activateAll({ "graph.asset-fail" });
		CHECK( mountFailure.error == PackageActivationError::AssetMountFailed &&
		       mountFailure.packageId == "graph.asset-fail" &&
		       mountFailure.diagnosticPath.empty() && mountFailure.activated.empty() &&
		       packages.activationOrder().empty() && packages.activeCampaign().empty() &&
		       !packages.isActive( "graph.campaign" ) && !packages.isActive( "graph.rules" ) &&
		       !packages.isActive( "graph.asset-fail" ) && campaign.activateCalls == 3 &&
		       campaign.deactivateCalls == 3 && rules.activateCalls == 3 &&
		       rules.deactivateCalls == 3 && assetFail.activateCalls == 1 &&
		       assetFail.deactivateCalls == 1 && !campaign.active() && !rules.active() &&
		       !assetFail.active() &&
		       runtime.services().assets.read( "Data/Graph/value.bin", graphAsset ) ==
		       AssetReadResult::Success && graphAsset.bytes == std::vector<std::uint8_t>({ 0 }) &&
		       graphAsset.provenance == "graph.host",
		       "asset-mount failure rolls back a newly activated dependency closure exactly" );

		bool assetCompositionThrew = false;
		try
		{
			packages.activateAll({ "graph.throwing-asset" });
		}
		catch (...)
		{
			assetCompositionThrew = true;
		}
		CHECK( assetCompositionThrew && packages.activationOrder().empty() &&
		       packages.activeCampaign().empty() && campaign.activateCalls == 4 &&
		       campaign.deactivateCalls == 4 && rules.activateCalls == 4 &&
		       rules.deactivateCalls == 4 && throwingAsset.activateCalls == 1 &&
		       throwingAsset.deactivateCalls == 1 && !campaign.active() && !rules.active() &&
		       !throwingAsset.active() &&
		       runtime.services().assets.read( "Data/Graph/value.bin", graphAsset ) ==
		       AssetReadResult::Success && graphAsset.provenance == "graph.host",
		       "asset-source exceptions unwind the whole newly activated dependency closure" );
	}

	{
		constexpr std::size_t chainLength = 1024;
		std::vector<std::unique_ptr<TestLifecyclePackage>> chain;
		chain.reserve( chainLength );
		for (std::size_t index = 0; index < chainLength; ++index)
		{
			std::vector<ContentRequirement> requirements;
			if (index > 0) requirements.push_back(
				ContentRequirement{ "chain." + std::to_string(index - 1), "" } );
			chain.emplace_back(new TestLifecyclePackage(
				"chain." + std::to_string(index), PackageKind::Extension,
				-1, nullptr, std::move(requirements) ));
		}
		ContentRegistry content( CurrentContentApiVersion );
		PackageRegistry packages( content );
		bool registered = true;
		for (auto package = chain.rbegin(); package != chain.rend(); ++package)
			registered = packages.registerPackage( **package ) == PackageRegistrationError::None &&
				registered;
		const PackageActivationPlan chainPlan =
			packages.resolveActivation( "chain." + std::to_string(chainLength - 1) );
		CHECK( registered && chainPlan && chainPlan.order.size() == chainLength &&
		       chainPlan.order.front() == "chain.0" &&
		       chainPlan.order.back() == "chain." + std::to_string(chainLength - 1),
		       "iterative dependency planning handles untrusted deep graphs without recursion" );
	}

	{
		ContentRegistry content( ContentApiVersion{ 1, 0 } );
		MemoryLogSink logSink;
		ManualTimeSource packageTime;
		packageTime.setMicroseconds( 42000 );
		SequenceRandomSource packageRandom( { 73 } );
		MemoryByteStorage packageStorage;
		MemoryInputSource packageInput;
		RecordingAudioOutput packageAudio;
		RecordingFramePresenter packageFrames;
		MemoryAssetSource packageAssets( "rules.test" );
		EngineServices services{packageTime, packageRandom, packageStorage, logSink, packageInput,
		                        packageAudio, packageFrames, packageAssets};
		packageInput.push( EngineInputEvent{ 17, 2, 1, 65, 0 } );
		EngineInputEvent injectedInput;
		CHECK( services.input.poll( injectedInput ) && injectedInput.timestamp == 17 &&
		       injectedInput.modifiers == 2 && injectedInput.primary == 65,
		       "engine services expose deterministic injected input" );
		const AudioPlaybackId playback = services.audio.play(
			AudioPlaybackRequest{ "sounds/test.wav", 22050, 80, 32, 1, false } );
		CHECK( playback != 0 && packageAudio.requests().size() == 1 &&
		       packageAudio.requests()[0].asset == "sounds/test.wav" &&
		       services.audio.isPlaying( playback ) && services.audio.stop( playback ) &&
		       !services.audio.isPlaying( playback ),
		       "engine services expose captureable headless audio" );
		services.frames.present( FramePresentMode::Paced );
		services.frames.present( FramePresentMode::Immediate );
		CHECK( packageFrames.presentations().size() == 2 &&
		       packageFrames.presentations()[0] == FramePresentMode::Paced &&
		       packageFrames.presentations()[1] == FramePresentMode::Immediate,
		       "engine services expose captureable headless frame presentation" );
		CHECK( packageAssets.put( "Data\\Rules//weapons.bin", { 1, 2, 3 } ),
		       "asset source accepts normalized package-relative paths" );
		AssetData packageAsset;
		CHECK( services.assets.read( "Data/Rules/weapons.bin", packageAsset ) ==
		       AssetReadResult::Success &&
		       packageAsset.logicalPath == "data/rules/weapons.bin" &&
		       packageAsset.provenance == "rules.test" &&
		       packageAsset.bytes == std::vector<std::uint8_t>({ 1, 2, 3 }),
		       "engine services expose package assets with provenance" );
		CHECK( services.assets.read( "Data/Rules/weapons.bin", packageAsset, 2 ) ==
		       AssetReadResult::TooLarge && packageAsset.bytes.empty(),
		       "asset reads enforce host-provided whole-file size limits" );
		CHECK( !services.assets.exists( "../outside.bin" ) &&
		       !services.assets.exists( "/absolute/path.bin" ),
		       "asset sources reject traversal and absolute paths" );
		TestLifecyclePackage first( "rules.first", PackageKind::Rules );
		TestLifecyclePackage failing( "rules.failing", PackageKind::Rules,
		                              static_cast<int>(PackageBootstrapPhase::LoadContent) );
		TestLifecyclePackage throwing( "rules.throwing", PackageKind::Rules );
		PackageRegistry packages( content, services );
		packages.registerPackage( first );
		packages.registerPackage( failing );
		packages.activate( "rules.first" );
		packages.activate( "rules.failing" );
		CHECK( packages.bootstrap( PackageBootstrapPhase::Configure ) == PackageBootstrapError::None &&
		       packages.completedBootstrapPhases() == 1 && first.observedContentApi.major == 1 &&
		       first.observedTime == 42000 && first.observedRandom == 73 &&
		       first.observedAssetProvenance == "rules.test",
		       "package bootstrap advances through ordered phases" );
		CHECK( packages.bootstrap( PackageBootstrapPhase::StartRuntime ) ==
		       PackageBootstrapError::OutOfOrder,
		       "package bootstrap rejects skipped phases" );
		CHECK( packages.bootstrap( PackageBootstrapPhase::LoadContent ) ==
		       PackageBootstrapError::CallbackFailed &&
		       first.shutdownCalls == std::vector<int>{ 1 } &&
		       failing.shutdownCalls == std::vector<int>{ 1 } &&
		       logSink.records().size() == 1 &&
		       logSink.records()[0].severity == LogSeverity::Error &&
		       logSink.records()[0].category == "packages",
		       "failed package phase rolls back including the failing callback" );
		CHECK( packages.activate( "rules.first" ) == PackageActivationError::BootstrapInProgress &&
		       !packages.deactivate( "rules.first" ),
		       "active package set is frozen while bootstrap resources exist" );
		packages.shutdownBootstrap();
		CHECK( packages.completedBootstrapPhases() == 0 &&
		       first.shutdownCalls.back() == 0 && failing.shutdownCalls.back() == 0,
		       "package shutdown unwinds completed phases for every active package" );
		throwing.throwPhase = static_cast<int>(PackageBootstrapPhase::Configure);
		CHECK( packages.registerPackage( throwing ) == PackageRegistrationError::None &&
		       packages.activate( "rules.throwing" ) == PackageActivationError::None &&
		       packages.bootstrap( PackageBootstrapPhase::Configure ) ==
		       PackageBootstrapError::CallbackFailed &&
		       throwing.shutdownCalls == std::vector<int>{ 0 } &&
		       logSink.records().size() == 2 &&
		       logSink.records().back().message == "Bootstrap callback threw: rules.throwing",
		       "package exceptions become deterministic rollback failures at the engine boundary" );
		const bool throwingDeactivated = packages.deactivate( "rules.throwing" );
		const bool failingDeactivated = packages.deactivate( "rules.failing" );
		const bool firstDeactivated = packages.deactivate( "rules.first" );
		CHECK( packages.completedBootstrapPhases() == 0 && throwingDeactivated &&
		       failingDeactivated && firstDeactivated && packages.activationOrder().empty(),
		       "package operation guard recovers after exceptions and permits clean teardown" );
	}

	{
		vfs::PropertyContainer emptyProperties;
		char executable[] = "ja2";
		char* emptyArguments[] = { executable };
		const PackageStartupOptions disabled =
			ReadPackageStartupOptions( emptyProperties, 1, emptyArguments );
		CHECK( !disabled.enabled && disabled.roots.empty() && disabled.selected.empty(),
		       "data packages remain disabled when configuration and command line are empty" );
		PackageHost disabledHost;
		EngineRuntime<unsigned> disabledRuntime;
		RecordingPackageAssetMounter disabledMounter;
		CHECK( disabledHost.initialize(
		       disabledRuntime.packages(), disabled, disabledMounter ) &&
		       !disabledHost.attempted() && disabledMounter.preflighted.empty() &&
		       disabledMounter.mounted.empty(),
		       "disabled package-host initialization is a state-free no-op" );

		vfs::PropertyContainer configuredProperties;
		configuredProperties.setStringProperty(
			L"Ja2 Settings", L"PACKAGE_ROOTS", L"ignored/ini/root" );
		configuredProperties.setStringProperty(
			L"Ja2 Settings", L"PACKAGE_SELECTION", L"ignored.ini.package" );
		const std::filesystem::path absoluteRoot =
			std::filesystem::absolute( "package-root-with-slashes" );
		std::vector<std::string> arguments = {
			"ja2",
			"--package=fixture.consumer,fixture.tools",
			"--package", "fixture.patch",
			"--package-root", absoluteRoot.generic_u8string(),
			"--package-root=relative/second-root"
		};
		std::vector<char*> argumentPointers;
		for ( std::string& argument : arguments ) argumentPointers.push_back( &argument[0] );
		const PackageStartupOptions overridden = ReadPackageStartupOptions(
			configuredProperties, static_cast<int>( argumentPointers.size() ),
			argumentPointers.data() );
		CHECK( overridden.enabled && overridden.selected == std::vector<std::string>({
		       "fixture.consumer", "fixture.tools", "fixture.patch" }) &&
		       overridden.roots == std::vector<std::filesystem::path>({
		       absoluteRoot, std::filesystem::path( "relative/second-root" ) }),
		       "repeated package CLI options override INI lists and preserve absolute slash paths" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime(
			EngineServices::defaults(), ContentApiVersion{ 1, 1 } );
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady =
			AddPackageFixture( fixture, "a-base", "fixture.registration-base" ) &&
			AddPackageFixture( fixture, "b-consumer", "fixture.registration-consumer",
			                   "fixture.registration-base" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.registration-consumer" };
		PackageHostResult result;
		if ( fixtureReady ) result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::RegistrationFailed &&
		       result.packageId == "fixture.registration-consumer" &&
		       runtime.packages().find( "fixture.registration-base" ) == nullptr &&
		       runtime.packages().find( "fixture.registration-consumer" ) == nullptr &&
		       runtime.packageCatalog().packages.empty() && result.rollbackFailures.empty() &&
		       mounter.preflighted.empty() && mounter.mounted.empty(),
		       "late registration rejection removes every package registered by the attempt" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		// Create in reverse order to prove discovery does not inherit directory
		// iteration order. Package directory names provide the stable order.
		const bool fixtureReady =
			AddPackageFixture( fixture, "z-consumer", "fixture.consumer", "fixture.base" ) &&
			fixture.write( "z-consumer/Data/rULES/aCTUALcASE.bin", "consumer" ) &&
			AddPackageFixture( fixture, "a-base", "fixture.base" ) &&
			fixture.write( "a-base/Data/Rules/ActualCase.BIN", "base" ) &&
			fixture.write( "a-base/Data/Mixed/OnlyHere.Dat", "base-only" ) &&
			fixture.makeDirectory( "m-not-a-package" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.consumer" };
		PackageHostResult result;
		if ( fixtureReady ) result = host.initialize( runtime.packages(), options, mounter );
		AssetData overriddenAsset;
		AssetData baseAsset;
		const AssetReadResult overrideRead = runtime.services().assets.read(
			"RULES/actualcase.BIN", overriddenAsset );
		const AssetReadResult baseRead = runtime.services().assets.read(
			"MIXED/onlyhere.dat", baseAsset );
		CHECK( fixtureReady && result &&
		       result.discovered == std::vector<std::string>({
		       "fixture.base", "fixture.consumer" }) &&
		       host.discoveredPackageIds() == result.discovered,
		       "package discovery is deterministic and ignores directories without manifests" );
		CHECK( result.activated == std::vector<std::string>({
		       "fixture.base", "fixture.consumer" }) &&
		       mounter.preflighted == result.activated && mounter.mounted == result.activated &&
		       runtime.packages().activationOrder() == result.activated,
		       "package dependencies activate and mount before their consumers" );
		CHECK( overrideRead == AssetReadResult::Success &&
		       std::string( overriddenAsset.bytes.begin(), overriddenAsset.bytes.end() ) ==
		       "consumer" && overriddenAsset.provenance == "fixture.consumer" &&
		       baseRead == AssetReadResult::Success &&
		       std::string( baseAsset.bytes.begin(), baseAsset.bytes.end() ) == "base-only" &&
		       baseAsset.provenance == "fixture.base",
		       "directory packages preserve actual file casing while consumer assets override dependencies" );
		const PackageHostResult repeated = host.initialize( runtime.packages(), options, mounter );
		CHECK( repeated.error == PackageHostError::AlreadyInitialized && host.attempted() &&
		       mounter.mounted == std::vector<std::string>({
		       "fixture.base", "fixture.consumer" }),
		       "a package host attempts startup exactly once" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady =
			AddPackageFixture( fixture, "a-base", "fixture.policy-base" ) &&
			AddPackageFixture( fixture, "b-peer", "fixture.policy-peer" ) &&
			fixture.makeDirectory( "c-consumer/Data" ) &&
			fixture.write( "c-consumer/package.ini", PackageFixtureManifest(
				"fixture.policy-consumer", {}, "Data", "fixture.policy-base@1.0.0",
				{}, "fixture.policy-peer", "feature.dynamic-weather, feature.new-ai" ) );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.policy-consumer", "fixture.policy-peer" };
		PackageHostResult result;
		if ( fixtureReady ) result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result && result.activated == std::vector<std::string>({
		       "fixture.policy-base", "fixture.policy-peer", "fixture.policy-consumer" }) &&
		       mounter.preflighted == result.activated && mounter.mounted == result.activated,
		       "Data Package v2 parses optional dependencies and deterministic LOAD_AFTER policy" );
		CHECK( runtime.hasCapability( "feature.dynamic-weather" ) &&
		       runtime.hasCapability( "feature.new-ai" ) &&
		       runtime.runtimeCapabilities().ids() == std::vector<std::string>({
		       "feature.dynamic-weather", "feature.new-ai" }),
		       "active data packages contribute portable runtime capabilities" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady = fixture.makeDirectory( "old-policy/Data" ) &&
			fixture.write( "old-policy/package.ini",
				"[Package]\n"
				"MANIFEST_VERSION = 1\n"
				"ID = fixture.old-policy\n"
				"VERSION = 1.0.0\n"
				"CONTENT_API = 1.3\n"
				"TYPE = extension\n"
				"ASSET_ROOT = Data\n"
				"CONFLICTS = fixture.other\n" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.old-policy" };
		PackageHostResult result;
		if ( fixtureReady ) result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::InvalidManifest &&
		       result.packageId == "fixture.old-policy" && mounter.mounted.empty(),
		       "v1 manifests cannot silently opt into v2 dependency policy" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady = fixture.makeDirectory( "bad/Data" ) &&
			fixture.write( "bad/package.ini",
			               PackageFixtureManifest( "fixture.traversal", {}, "../Data" ) );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.traversal" };
		PackageHostResult result;
		if ( fixtureReady ) result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::InvalidManifest &&
		       result.packageId == "fixture.traversal" &&
		       runtime.packages().find( "fixture.traversal" ) == nullptr &&
		       mounter.preflighted.empty() && mounter.mounted.empty(),
		       "package manifests reject asset-root traversal before registration" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady = fixture.makeDirectory( "malformed/Data" ) &&
			fixture.write( "malformed/package.ini",
				"[Package]\n"
				"MANIFEST_VERSION = 1\n"
				"ID = fixture.malformed\n"
				"VERSION = 1.0.0\n"
				"CONTENT_API = 1.1\n"
				"ASSET_ROOT = Data\n" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.malformed" };
		PackageHostResult result;
		if ( fixtureReady ) result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::InvalidManifest &&
		       runtime.packages().find( "fixture.malformed" ) == nullptr,
		       "package discovery rejects incomplete manifests transactionally" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady = AddPackageFixture(
			fixture, "bad-requirement", "fixture.bad-requirement", "fixture.base@" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.bad-requirement" };
		PackageHostResult result;
		if ( fixtureReady ) result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::InvalidManifest &&
		       result.packageId == "fixture.bad-requirement" &&
		       runtime.packages().find( "fixture.bad-requirement" ) == nullptr,
		       "package manifests reject a dependency with an empty @ version" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady = fixture.makeDirectory( "backslash/Data/Nested" ) &&
			fixture.write( "backslash/package.ini",
			               PackageFixtureManifest(
				               "fixture.backslash", {}, "Data\\Nested" ) );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.backslash" };
		PackageHostResult result;
		if ( fixtureReady ) result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::InvalidManifest &&
		       result.packageId == "fixture.backslash" &&
		       runtime.packages().find( "fixture.backslash" ) == nullptr,
		       "package manifests reject platform-ambiguous backslash asset roots" );
	}

	{
		ScopedPackageFixture fixture;
		const bool fixtureReady = fixture.makeDirectory( "symlink/RealData" ) &&
			fixture.write( "symlink/package.ini",
			               PackageFixtureManifest( "fixture.symlink" ) );
		std::error_code symlinkError;
		if ( fixtureReady )
			std::filesystem::create_directory_symlink(
				fixture.root() / "symlink/RealData",
				fixture.root() / "symlink/Data", symlinkError );
		if ( fixtureReady && !symlinkError )
		{
			PackageHost host;
			EngineRuntime<unsigned> runtime;
			RecordingPackageAssetMounter mounter;
			PackageStartupOptions options;
			options.enabled = true;
			options.roots = { fixture.root() };
			options.selected = { "fixture.symlink" };
			const PackageHostResult result =
				host.initialize( runtime.packages(), options, mounter );
			CHECK( result.error == PackageHostError::InvalidManifest &&
			       result.packageId == "fixture.symlink" &&
			       runtime.packages().find( "fixture.symlink" ) == nullptr,
			       "package manifests reject a symbolic-link asset root" );
		}
		else
		{
			std::printf( "skip  package symbolic-link test (host disallows symlink creation)\n" );
		}
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady = AddPackageFixture(
			fixture, "needs-missing", "fixture.needs-missing", "fixture.absent" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.needs-missing" };
		PackageHostResult result;
		if ( fixtureReady ) result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::ResolutionFailed &&
		       result.packageId == "fixture.absent" &&
		       result.diagnosticPath == std::vector<std::string>({
		       "fixture.needs-missing", "fixture.absent" }) &&
		       !runtime.packages().isActive( "fixture.needs-missing" ) &&
		       runtime.packages().find( "fixture.needs-missing" ) == nullptr &&
		       runtime.packageCatalog().packages.empty() && result.rollbackFailures.empty() &&
		       runtime.packages().activationOrder().empty() &&
		       mounter.preflighted.empty() && mounter.mounted.empty(),
		       "missing package dependencies fail with a diagnostic path before mount preflight" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		mounter.failPreflightId = "fixture.preflight-consumer";
		const bool fixtureReady =
			AddPackageFixture( fixture, "a-base", "fixture.preflight-base" ) &&
			AddPackageFixture( fixture, "b-consumer", "fixture.preflight-consumer",
			                   "fixture.preflight-base" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.preflight-consumer" };
		PackageHostResult result;
		if ( fixtureReady ) result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::MountPreflightFailed &&
		       result.packageId == "fixture.preflight-consumer" &&
		       mounter.preflighted == std::vector<std::string>({
		       "fixture.preflight-base", "fixture.preflight-consumer" }) &&
		       mounter.mounted.empty() && runtime.packages().activationOrder().empty() &&
		       !runtime.packages().isActive( "fixture.preflight-base" ) &&
		       !runtime.packages().isActive( "fixture.preflight-consumer" ) &&
		       runtime.packages().find( "fixture.preflight-base" ) == nullptr &&
		       runtime.packages().find( "fixture.preflight-consumer" ) == nullptr &&
		       result.rollbackFailures.empty(),
		       "mount preflight completes before invoking any package activation" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		mounter.failMountId = "fixture.mount-consumer";
		const bool fixtureReady =
			AddPackageFixture( fixture, "a-base", "fixture.mount-base" ) &&
			fixture.write( "a-base/Data/rollback.bin", "base" ) &&
			AddPackageFixture( fixture, "b-consumer", "fixture.mount-consumer",
			                   "fixture.mount-base" ) &&
			fixture.write( "b-consumer/Data/rollback.bin", "consumer" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.mount-consumer" };
		PackageHostResult result;
		if ( fixtureReady ) result = host.initialize( runtime.packages(), options, mounter );
		AssetData rolledBackAsset;
		const AssetReadResult rolledBackRead =
			runtime.services().assets.read( "rollback.bin", rolledBackAsset );
		CHECK( fixtureReady && result.error == PackageHostError::MountFailed &&
		       result.packageId == "fixture.mount-consumer" &&
		       mounter.preflighted == std::vector<std::string>({
		       "fixture.mount-base", "fixture.mount-consumer" }) &&
		       mounter.mounted == mounter.preflighted &&
		       mounter.unmounted == std::vector<std::string>({
		       "fixture.mount-consumer", "fixture.mount-base" }) &&
		       mounter.activeMounts.empty() && result.rollbackFailures.empty() &&
		       runtime.packages().activationOrder().empty() &&
		       !runtime.packages().isActive( "fixture.mount-base" ) &&
		       !runtime.packages().isActive( "fixture.mount-consumer" ) &&
		       runtime.packages().find( "fixture.mount-base" ) == nullptr &&
		       runtime.packages().find( "fixture.mount-consumer" ) == nullptr &&
		       rolledBackRead == AssetReadResult::NotFound,
		       "a partial late mount failure rolls back VFS mounts, activation, registration, and assets" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		mounter.failMountId = "fixture.rollback-report-consumer";
		mounter.failUnmountId = "fixture.rollback-report-base";
		const bool fixtureReady =
			AddPackageFixture( fixture, "a-base", "fixture.rollback-report-base" ) &&
			AddPackageFixture( fixture, "b-consumer", "fixture.rollback-report-consumer",
			                   "fixture.rollback-report-base" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.rollback-report-consumer" };
		PackageHostResult result;
		if ( fixtureReady ) result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::MountFailed &&
		       result.rollbackFailures == std::vector<std::string>({
		       "unmount fixture.rollback-report-base: injected unmount failure" }) &&
		       result.message.find( "rollback incomplete" ) != std::string::npos &&
		       runtime.packages().activationOrder().empty() &&
		       runtime.packageCatalog().packages.empty(),
		       "package startup reports external rollback failures while continuing engine cleanup" );
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
		vitals.maximumHealth() = 80;
		vitals.applyLifeDeduction( 20 );
		CHECK( vitals.health() == 55,
		       "soldier vitals component applies production life deduction" );
		vitals.applyLifeDeduction( -50 );
		CHECK( vitals.health() == 80,
		       "soldier vitals component caps negative damage at maximum health" );
		vitals.applyLifeDeduction( 100 );
		CHECK( !vitals.alive() && vitals.health() == 0,
		       "soldier vitals component clamps lethal damage to zero" );
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
	ScopedPackageFixture vfsPrecedenceFixture;
	const std::string vfsPriorityToken =
		std::to_string( static_cast<unsigned long long>( SDL_GetTicksNS() ) );
	const std::string packageOverlayPath =
		"package-vfs-overlay-" + vfsPriorityToken + ".txt";
	const std::string writableOverlayPath =
		"package-vfs-writable-" + vfsPriorityToken + ".txt";
	const bool vfsPrecedenceFixtureReady =
		vfsPrecedenceFixture.write( "legacy/" + packageOverlayPath, "legacy" ) &&
		vfsPrecedenceFixture.write( "package/" + packageOverlayPath, "package" ) &&
		vfsPrecedenceFixture.write( "legacy/" + writableOverlayPath, "legacy" ) &&
		vfsPrecedenceFixture.write( "package/" + writableOverlayPath, "package" );
	{
		std::ofstream writableOverlay( writableOverlayPath,
		                                std::ios::binary | std::ios::trunc );
		writableOverlay << "writable";
	}
	vfs_init::VfsConfig vfsConfig;
	vfs_init::Profile* testProfile = new vfs_init::Profile();
	testProfile->m_name = L"headless-tests";
	testProfile->m_root = L".";
	testProfile->m_writable = true;
	vfsConfig.addProfile( testProfile, true );
	CHECK( vfs_init::initVirtualFileSystem( vfsConfig ), "initialize writable headless VFS profile" );
	CHECK( InitializeFileManager( NULL ), "InitializeFileManager(NULL)" );

	{
		auto mountReadOnlyDirectory = []( const std::string& profileName,
		                                  const std::filesystem::path& root )
		{
			vfs_init::VfsConfig config;
			vfs_init::Profile* profile = new vfs_init::Profile();
			profile->m_name = vfs::String( profileName.c_str() );
			profile->m_root = vfs::Path( root.generic_u8string() );
			profile->m_writable = false;
			vfs_init::Location* location = new vfs_init::Location();
			location->m_optional = false;
			location->m_type = L"DIRECTORY";
			profile->addLocation( location, true );
			config.addProfile( profile, true );
			return vfs_init::initVirtualFileSystem( config, false );
		};

		const std::string legacyProfileName = "headless-legacy-" + vfsPriorityToken;
		const std::string packageProfileName = "headless-package-" + vfsPriorityToken;
		const bool legacyMounted = vfsPrecedenceFixtureReady &&
			mountReadOnlyDirectory(
				legacyProfileName,
				vfsPrecedenceFixture.root() / "legacy" );
		const bool packageMounted = legacyMounted &&
			mountReadOnlyDirectory(
				packageProfileName,
				vfsPrecedenceFixture.root() / "package" );
		std::string packageOverlay;
		std::string writableOverlay;
		const bool packageRead =
			ReadFileManagerText( packageOverlayPath, packageOverlay );
		const bool writableRead =
			ReadFileManagerText( writableOverlayPath, writableOverlay );
		CHECK( packageMounted && packageRead && packageOverlay == "package",
		       "late package VFS mounts override earlier read-only legacy content" );
		CHECK( packageMounted && writableRead && writableOverlay == "writable",
		       "late read-only package VFS mounts remain below writable user content" );
		const bool packageUnmounted = packageMounted &&
			getVFS()->getProfileStack()->removeProfile(
				vfs::String( packageProfileName.c_str() ) );
		packageOverlay.clear();
		const bool legacyRead = packageUnmounted &&
			ReadFileManagerText( packageOverlayPath, packageOverlay );
		CHECK( packageUnmounted && legacyRead && packageOverlay == "legacy" &&
		       getVFS()->getProfileStack()->getProfile( L"headless-tests" ) != nullptr,
		       "named VFS removal restores a lower overlay without popping writable user data" );
		std::error_code ignored;
		std::filesystem::remove( writableOverlayPath, ignored );
	}

	{
		MemoryByteStorage memoryStorage;
		PersistenceService memoryPersistence( memoryStorage );
		const std::vector<std::uint8_t> legacyBytes = { 1, 2, 3, 4 };
		std::vector<std::uint8_t> loadedLegacyBytes;
		CHECK( memoryPersistence.saveRaw( "legacy", legacyBytes ) &&
		       memoryPersistence.loadRaw( "legacy", loadedLegacyBytes ) &&
		       loadedLegacyBytes == legacyBytes,
		       "persistence service preserves established raw formats byte-for-byte" );
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
		const std::vector<std::uint8_t> envelopePayload = { 5, 8, 13, 21 };
		CHECK( memoryPersistence.saveEnvelope(
		       "envelope", PersistenceHeader{ 0x454E4732u, 3 }, envelopePayload ) ==
		       PersistenceSaveResult::Success &&
		       memoryPersistence.loadEnvelope(
		       "envelope", 0x454E4732u, 2, 3, emptyHeader, emptyPayload ) ==
		       PersistenceLoadResult::Success && emptyHeader.version == 3 &&
		       emptyPayload == envelopePayload,
		       "runtime persistence supports bounded integrity-checked envelopes" );

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
		EngineInputEvent engineAtom = {};
		InputAtom atom = {};
		CHECK( GetPlatformInputSource().poll( engineAtom ) && engineAtom.type == KEY_DOWN &&
		       engineAtom.primary == F1 && engineAtom.sequence == 1 &&
		       engineAtom.droppedBefore == 0,
		       "platform input mirrors SDL events for engine consumers" );
		CHECK( DequeueEvent( &atom ) && atom.usEvent == KEY_DOWN && atom.usParam == F1,
		       "engine input polling does not steal the legacy UI event" );
		keyEvent.type = SDL_EVENT_KEY_UP;
		SgpHandleSDLEvent( &keyEvent );
		CHECK( GetPlatformInputSource().poll( engineAtom ) && engineAtom.type == KEY_UP,
		       "platform input preserves mirrored key-up transitions" );
		DequeueEvent( &atom );
		ShutdownInputManager();
		CHECK( !GetPlatformInputSource().poll( engineAtom ),
		       "platform input discards stale events across manager lifecycles" );
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
