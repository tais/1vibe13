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
#include "TacticalWorldAdapter.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
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
#include <Engine/Adapters/JA2/CampaignEventService.h>
#include <Engine/Adapters/JA2/EngineRuntime.h>
#include <Engine/Adapters/JA2/MemoryTacticalSimulation.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Adapters/JA2/TacticalCommandResultCodec.h>
#include <Engine/Adapters/JA2/TacticalCommandService.h>
#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaPublisher.h>
#include <Engine/Adapters/JA2/TacticalWorldObserver.h>
#include <Engine/Adapters/JA2/TacticalWorldService.h>
#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/StateStack.h>
#include <Engine/Core/StateTransition.h>
#include <Engine/Core/ContentApi.h>
#include <Engine/Core/TimeSource.h>
#include <Engine/Core/RandomSource.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Adapters/Legacy/PlatformFileSystem.h>
#include <Engine/Adapters/Legacy/LegacyFrameInvalidationGateway.h>
#include <Engine/Adapters/Legacy/LegacyFrameGateway.h>
#include <Engine/Adapters/Legacy/LegacyRenderCommandGateway.h>
#include <Engine/Adapters/Legacy/LegacyRenderSurfaceGateway.h>
#include <Engine/Adapters/Legacy/PlatformFrameInvalidator.h>
#include <Engine/Adapters/Legacy/PlatformFramePresenter.h>
#include <Engine/Adapters/Legacy/PlatformRenderCommands.h>
#include <Engine/Adapters/Legacy/PlatformRenderSurfaceAccess.h>
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
#include "GameVersion.h"
#include "gameloop.h"
#include "SaveLoadGame.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "CampaignPackage.h"
#include "RulesPackage.h"
#include "PackageHost.h"
#include "RuntimeReportHost.h"
#include "RuntimeSaveState.h"
#include "Simulation Commands.h"
#include "StrategicGroupHost.h"
#include "TacticalCommandHost.h"
#include "TacticalEntityHost.h"
#include "TacticalInventoryUiHost.h"
#include "TacticalWorldItemHost.h"
#include "TacticalWorldObserverHost.h"
#include "interface Dialogue.h"
#include "Assignments.h"
#include "Merc Contract.h"
#include "PreBattle Interface.h"
#include "Game Clock.h"
#include "Game Events.h"
#include "popup_class.h"
#include "Soldier Control.h"
#include "Animation Control.h"
#include "Map Information.h"
#include "Overhead.h"
#include "ai.h"
#include "Vehicles.h"
#include "World Items.h"
#include "Strategic Movement.h"
#include "strategicmap.h"
#include "strategic.h"
#include "MovementDestinationPolicy.h"
#include "Rain.h"
#include "Render Dirty.h"
#include "Timer Control.h"
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
static UINT64 gInjectedLegacyClockTime = 0;
static BOOLEAN gInjectedFastForwardKeyDown = FALSE;

extern VIDEO_OVERLAY gVideoOverlays[];
extern UINT32 guiNumVideoOverlays;

static_assert(
	std::is_same<decltype( gWorldSectorX ), const INT16&>::value &&
		std::is_same<decltype( gWorldSectorY ), const INT16&>::value &&
		std::is_same<decltype( gbWorldSectorZ ), const INT8&>::value,
	"legacy tactical-sector names must remain compiler-enforced read-only projections" );

static UINT64 InjectedLegacyClockTime()
{
	return gInjectedLegacyClockTime;
}

static BOOLEAN InjectedLegacyClockKeyState( INT32 key )
{
	return key == 42 && gInjectedFastForwardKeyDown;
}

#define CHECK( cond, msg ) \
	do { if ( !( cond ) ) { ++g_failures; std::printf( "FAIL  %s\n", msg ); } \
	     else std::printf( "ok    %s\n", msg ); } while ( 0 )

class HeadlessRuntimeMessageSink final : public RuntimeMessageSink
{
public:
	void receiveMessage( const RuntimeMessage& message ) override
	{
		messages.push_back( message );
	}

	std::vector<RuntimeMessage> messages;
};

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
	const std::string& capabilities = {}, const std::string& requiredCapabilities = {} )
{
	const bool policyV2 = !optionalRequirements.empty() || !conflicts.empty() ||
		!loadAfter.empty() || !capabilities.empty() || !requiredCapabilities.empty();
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
	if ( !requiredCapabilities.empty() )
		manifest += "REQUIRED_CAPABILITIES = " + requiredCapabilities + "\n";
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
		observedMessages = &context.messages;
		observedIdentity = context.identity;
		const EngineServiceLookupResult<FrameTelemetry> telemetry =
			context.extensionServices.resolve( FrameTelemetryServiceContract );
		observedTelemetry = telemetry.service;
		const std::int64_t* messageCapacity =
			context.configuration.find<std::int64_t>( "engine.messages.queue-capacity" );
		observedMessageCapacity = messageCapacity ? *messageCapacity : -1;
		if (persistOnConfigure && phase == PackageBootstrapPhase::Configure)
		{
			observedStoragePackageId = context.storage.packageId();
			packageSaveResult = context.storage.saveEnvelope(
				"test-state", PersistenceHeader{ 0x504B4754u, 1 }, { 8, 9 } );
			invalidPackageSaveResult = context.storage.saveEnvelope(
				"../invalid", PersistenceHeader{ 0x504B4754u, 1 }, {} );
		}
		if (publishOnConfigure && phase == PackageBootstrapPhase::Configure)
		{
			observedPublisherPackageId = context.messagePublisher.packageId();
			packagePublishResult = context.messagePublisher.publish(
				"package.ready", { 6, 1 } );
			invalidPackagePublishResult = context.messagePublisher.publish(
				"../invalid", {} );
		}
		if (usePackageRandomOnConfigure && phase == PackageBootstrapPhase::Configure)
		{
			observedRandomPackageId = context.random.packageId();
			packageRandomResult = context.random.next( "bootstrap", 100 );
		}
		if (localizeOnConfigure && phase == PackageBootstrapPhase::Configure)
		{
			observedLocalizationPackageId = context.localization.packageId();
			localizationSetResult = context.localization.set(
				"en", "ui.package-ready", "Package ready" );
			invalidLocalizationSetResult = context.localization.set(
				"invalid/locale", "ui.package-ready", "Invalid" );
		}
		if (defineOnConfigure && phase == PackageBootstrapPhase::Configure)
		{
			observedDefinitionPackageId = context.definitions.packageId();
			definitionSetResult = context.definitions.set(
				"rules", "package-ready", 1, { 4, 2 } );
			invalidDefinitionSetResult = context.definitions.set(
				"invalid/type", "package-ready", 1, {} );
		}
		if (createEntityOnConfigure && phase == PackageBootstrapPhase::Configure)
		{
			observedEntityPackageId = context.entities.packageId();
			packageEntityCreateResult = context.entities.create( "test-entity" );
		}
		if (playAudioOnConfigure && phase == PackageBootstrapPhase::Configure)
		{
			observedAudioPackageId = context.audio.packageId();
			packageAudioPlayResult = context.audio.play(
				"ui", AudioPlaybackRequest{ "Audio/Package.wav", 11025, 90, 64, 1, false } );
			invalidPackageAudioPlayResult = context.audio.play(
				"invalid/group", AudioPlaybackRequest{ "Audio/Invalid.wav" } );
		}
		if (deferTasksOnConfigure && phase == PackageBootstrapPhase::Configure)
		{
			observedTasksPackageId = context.tasks.packageId();
			packageTaskResult = context.tasks.defer([this] { ++deferredTaskRuns; });
			throwingPackageTaskResult = context.tasks.defer([] { throw "test deferred task"; });
		}
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
		if (initializationGuardDuringBootstrap)
		{
			initializationGuardCancelResult =
				initializationGuardDuringBootstrap->cancel();
			initializationGuardDuringBootstrap = nullptr;
		}
		bootstrapCalls.push_back(static_cast<int>(phase));
		if (static_cast<int>(phase) == throwPhase) throw "test package bootstrap exception";
		if (static_cast<int>(phase) == failOncePhase)
		{
			failOncePhase = -1;
			return false;
		}
		return static_cast<int>(phase) != failPhase_;
	}
	void shutdown(PackageBootstrapContext&, PackageBootstrapPhase phase) override
	{
		if (lifecycleTrace)
			lifecycleTrace->push_back("shutdown:" + descriptor_.content.id);
		shutdownCalls.push_back(static_cast<int>(phase));
	}
	void receiveInput(PackageBootstrapContext&, const EngineInputEvent& event) override
	{
		inputEvents.push_back(event);
		if (throwOnInput) throw "test package input exception";
	}
	void updateRuntime(PackageBootstrapContext&, const RuntimeUpdateContext& update) override
	{
		runtimeUpdates.push_back(update);
		if (throwOnRuntimeUpdate) throw "test package runtime update exception";
	}
	void receiveMessage(PackageBootstrapContext&, const RuntimeMessage& message) override
	{
		receivedMessages.push_back(message);
		if (throwOnMessage) throw "test package message exception";
	}
	void simulate(PackageBootstrapContext&, const SimulationTickContext& tick) override
	{
		simulationTicks.push_back(tick);
		if (throwOnSimulationTick) throw "test package simulation exception";
	}
	bool saveState(PackageBootstrapContext&, std::vector<std::uint8_t>& state) override
	{
		++saveStateCalls;
		if (throwOnSaveState) throw "test package save-state exception";
		state = saveStatePayload;
		return saveStateSucceeds;
	}
	bool validateState(PackageBootstrapContext&, std::uint32_t schema,
	                   const std::vector<std::uint8_t>& state) override
	{
		++validateStateCalls;
		return validateStateSucceeds && schema == descriptor_.saveStateSchemaVersion &&
			state == saveStatePayload;
	}
	bool loadState(PackageBootstrapContext&, std::uint32_t schema,
	               const std::vector<std::uint8_t>& state) override
	{
		++loadStateCalls;
		if (!loadStateSucceeds || schema != descriptor_.saveStateSchemaVersion) return false;
		loadedStatePayload = state;
		return true;
	}

	std::vector<int> bootstrapCalls;
	std::vector<int> shutdownCalls;
	std::vector<EngineInputEvent> inputEvents;
	std::vector<RuntimeUpdateContext> runtimeUpdates;
	std::vector<RuntimeMessage> receivedMessages;
	std::vector<SimulationTickContext> simulationTicks;
	ContentApiVersion observedContentApi{};
	std::uint64_t observedTime = 0;
	std::uint32_t observedRandom = 0;
	std::string observedAssetProvenance;
	EngineServices* observedServices = nullptr;
	RuntimeMessageBus* observedMessages = nullptr;
	PackageIdentity observedIdentity;
	FrameTelemetry* observedTelemetry = nullptr;
	std::int64_t observedMessageCapacity = -1;
	std::string observedStoragePackageId;
	PersistenceSaveResult packageSaveResult = PersistenceSaveResult::StorageError;
	PersistenceSaveResult invalidPackageSaveResult = PersistenceSaveResult::StorageError;
	std::string observedPublisherPackageId;
	RuntimeMessagePublishResult packagePublishResult;
	RuntimeMessagePublishResult invalidPackagePublishResult;
	std::string observedRandomPackageId;
	PackageRandomResult packageRandomResult;
	std::string observedLocalizationPackageId;
	LocalizationSetError localizationSetResult = LocalizationSetError::AllocationFailure;
	LocalizationSetError invalidLocalizationSetResult = LocalizationSetError::AllocationFailure;
	std::string observedDefinitionPackageId;
	DefinitionSetError definitionSetResult = DefinitionSetError::AllocationFailure;
	DefinitionSetError invalidDefinitionSetResult = DefinitionSetError::AllocationFailure;
	std::string observedEntityPackageId;
	EntityCreateResult packageEntityCreateResult;
	std::string observedAudioPackageId;
	PackageAudioPlayResult packageAudioPlayResult;
	PackageAudioPlayResult invalidPackageAudioPlayResult;
	std::string observedTasksPackageId;
	PackageTaskScheduleResult packageTaskResult;
	PackageTaskScheduleResult throwingPackageTaskResult;
	unsigned deferredTaskRuns = 0;
	int activateCalls = 0;
	int deactivateCalls = 0;
	bool activationSucceeds = true;
	int throwPhase = -1;
	int failOncePhase = -1;
	bool throwOnInput = false;
	bool throwOnRuntimeUpdate = false;
	bool throwOnMessage = false;
	bool throwOnSimulationTick = false;
	bool throwOnSaveState = false;
	bool saveStateSucceeds = true;
	bool validateStateSucceeds = true;
	bool loadStateSucceeds = true;
	int saveStateCalls = 0;
	int validateStateCalls = 0;
	int loadStateCalls = 0;
	std::vector<std::uint8_t> saveStatePayload{ 4, 2, 1 };
	std::vector<std::uint8_t> loadedStatePayload;
	bool persistOnConfigure = false;
	bool publishOnConfigure = false;
	bool usePackageRandomOnConfigure = false;
	bool localizeOnConfigure = false;
	bool defineOnConfigure = false;
	bool createEntityOnConfigure = false;
	bool playAudioOnConfigure = false;
	bool deferTasksOnConfigure = false;
	PackageRegistry* registryDuringBootstrap = nullptr;
	std::string activateDuringBootstrap;
	std::string deactivateDuringBootstrap;
	PackageActivationError nestedActivationResult = PackageActivationError::None;
	PackageActivationPlan nestedResolutionResult;
	bool nestedBootstrapDeactivationResult = true;
	GameInitializationGuard* initializationGuardDuringBootstrap = nullptr;
	RuntimeSessionTransitionResult initializationGuardCancelResult;
	PackageRegistry* registryDuringDeactivate = nullptr;
	std::string deactivateDuringDeactivate;
	bool nestedDeactivateResult = true;
	std::vector<std::string>* lifecycleTrace = nullptr;
	std::vector<std::string>* deactivationTrace = nullptr;
	void setMessageTopics(std::vector<std::string> topics)
	{
		descriptor_.messageTopics = std::move(topics);
	}
	void setRequiredServices(std::vector<EngineServiceRequirement> requirements)
	{
		descriptor_.requiredServices = std::move(requirements);
	}
	void setRequiredCapabilities(std::vector<std::string> requirements)
	{
		descriptor_.requiredCapabilities = std::move(requirements);
	}
	void setSaveStateSchema(std::uint32_t schema)
	{
		descriptor_.saveStateSchemaVersion = schema;
	}
	bool active() const { return active_; }

private:
	PackageDescriptor descriptor_;
	int failPhase_;
	AssetSource* assets_;
	bool active_ = false;
};

class TestGameplayBootstrapHooks final
	: public RulesContentBootstrapHost,
	  public CampaignRuntimeBootstrapHost
{
public:
	bool loadRulesContent(const GameCapabilities& capabilities) override
	{
		contentCampaigns.push_back(capabilities.campaign);
		if (trace)
			trace->push_back(capabilities.isUnfinishedBusiness()
				? "campaign:load-content:ub" : "campaign:load-content:ja2");
		if (throwOnLoadContent)
			throw std::runtime_error("test campaign content exception");
		return loadContentSucceeds;
	}

	bool startCampaignRuntime(const GameCapabilities& capabilities) override
	{
		runtimeCampaigns.push_back(capabilities.campaign);
		if (trace)
			trace->push_back(capabilities.isUnfinishedBusiness()
				? "campaign:start-runtime:ub" : "campaign:start-runtime:ja2");
		if (throwOnStartRuntime)
			throw std::runtime_error("test campaign runtime exception");
		return startRuntimeSucceeds;
	}

	std::vector<GameCampaign> contentCampaigns;
	std::vector<GameCampaign> runtimeCampaigns;
	std::vector<std::string>* trace = nullptr;
	bool loadContentSucceeds = true;
	bool startRuntimeSucceeds = true;
	bool throwOnLoadContent = false;
	bool throwOnStartRuntime = false;
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

class RecordingSimulationCommandExecutionSink final
	: public SimulationCommandExecutionSink
{
public:
	void commandProcessed(
		const SimulationCommand&, std::uint64_t tick,
		std::uint64_t sequence,
		CommandDisposition disposition) noexcept override
	{
		observed = true;
		lastTick = tick;
		lastSequence = sequence;
		lastDisposition = disposition;
	}

	bool observed = false;
	std::uint64_t lastTick = 0;
	std::uint64_t lastSequence = 0;
	CommandDisposition lastDisposition = CommandDisposition::Retry;
};

struct HeadlessTacticalCommandObservation
{
	std::uint64_t tick = 0;
	std::uint64_t sequence = 0;
	std::size_t commandIndex = 0;
	CommandDisposition disposition = CommandDisposition::Discard;

	friend bool operator==(
		const HeadlessTacticalCommandObservation& left,
		const HeadlessTacticalCommandObservation& right)
	{
		return left.tick == right.tick &&
			left.sequence == right.sequence &&
			left.commandIndex == right.commandIndex &&
			left.disposition == right.disposition;
	}
};

struct HeadlessTacticalTurnRun
{
	TacticalSimulationSnapshot snapshot;
	std::vector<HeadlessTacticalCommandObservation> observations;
	bool executorBound = false;
	bool observationFailure = false;
	bool completed = false;
	bool sawBudgetExhaustion = false;
	bool sawRetry = false;
	bool queueChanged = false;
	TacticalSimulationResetError resetError =
		TacticalSimulationResetError::None;
};

class RetrySelectedFireOnceExecutor final
	: public SimulationCommandExecutor
{
public:
	explicit RetrySelectedFireOnceExecutor(
		SimulationCommandExecutor& next) noexcept
		: next_(next)
	{
	}

	CommandDisposition execute(
		const SimulationCommand& command,
		std::uint64_t tick,
		std::uint64_t sequence) override
	{
		// Exercise queue retention without hiding retry policy inside the
		// reusable executor. The second attempt applies the exact same captured
		// command through the public execution contract.
		if (!selectedFireRetried_ &&
			std::holds_alternative<
				BeginSelectedFireWeaponCommand>(command))
		{
			selectedFireRetried_ = true;
			return CommandDisposition::Retry;
		}
		return next_.execute(command, tick, sequence);
	}

private:
	SimulationCommandExecutor& next_;
	bool selectedFireRetried_ = false;
};

class HeadlessTacticalTurnExecutionSink final
	: public SimulationCommandExecutionSink
{
public:
	explicit HeadlessTacticalTurnExecutionSink(
		HeadlessTacticalTurnRun& run) noexcept
		: run_(run)
	{
	}

	void commandProcessed(
		const SimulationCommand& command,
		std::uint64_t tick,
		std::uint64_t sequence,
		CommandDisposition disposition) noexcept override
	{
		try
		{
			run_.observations.push_back(
				HeadlessTacticalCommandObservation{
					tick, sequence, command.index(), disposition});
		}
		catch (...)
		{
			run_.observationFailure = true;
		}
	}

private:
	HeadlessTacticalTurnRun& run_;
};

static HeadlessTacticalTurnRun RunHeadlessTacticalTurn(
	EngineRuntime<unsigned>& runtime,
	MemoryTacticalSimulation& model,
	SimulationCommandExecutor& executor,
	TacticalSimulationSnapshot baseline)
{
	HeadlessTacticalTurnRun run;
	run.resetError = model.reset(std::move(baseline));
	if (run.resetError != TacticalSimulationResetError::None)
		return run;
	run.executorBound =
		runtime.bindSimulationCommandExecutor(executor);
	if (!run.executorBound) return run;
	HeadlessTacticalTurnExecutionSink executionSink{run};
	for (std::size_t pass = 0;
		pass < 32 && !runtime.commands().empty(); ++pass)
	{
		const CommandProcessingResult processing =
			runtime.executeCommandsThrough(3, 2, &executionSink);
		run.sawBudgetExhaustion =
			run.sawBudgetExhaustion ||
			processing.status == CommandProcessStatus::BudgetExhausted;
		run.sawRetry =
			run.sawRetry ||
			processing.status == CommandProcessStatus::Blocked;
		run.queueChanged =
			run.queueChanged ||
			processing.status == CommandProcessStatus::QueueChanged;
		if (run.queueChanged) break;
	}
	run.completed = runtime.commands().empty();
	run.snapshot = model.snapshot();
	return run;
}

static TacticalSimulationSnapshot MakeHeadlessTacticalTurnBaseline(
	TacticalEntityId player,
	TacticalEntityId opponent)
{
	TacticalSimulationSnapshot snapshot;
	snapshot.actors.push_back(TacticalSimulationActorState{
		player, 100, 10, 10, ANIM_STAND, 0, false, true});
	snapshot.actors.push_back(TacticalSimulationActorState{
		opponent, 400, 40, 40, ANIM_STAND, 4, false, true});
	return snapshot;
}

static std::vector<std::uint64_t> SubmitHeadlessTacticalTurn(
	EngineRuntime<unsigned>& runtime,
	TacticalEntityId player,
	TacticalEntityId opponent)
{
	std::vector<std::uint64_t> sequences;
	sequences.push_back(runtime.submitCommand(
		3, SimulationCommand{ChangeStanceCommand{
			player, ANIM_CROUCH, SimulationCommandSource::System,
			TacticalEventPolicy::Replicated}}));
	sequences.push_back(runtime.submitCommand(
		1, SimulationCommand{MoveToGridCommand{
			player, 220, WALKING, false, true,
			SimulationCommandSource::System,
			TacticalMoveOrigin::System,
			TacticalPendingActionPolicy::Preserve}}));
	sequences.push_back(runtime.submitCommand(
		2, SimulationCommand{SetFacingCommand{
			player, 2, SimulationCommandSource::System,
			TacticalEventPolicy::LocalOnly}}));
	sequences.push_back(runtime.submitCommand(
		2, SimulationCommand{BeginSelectedFireWeaponCommand{
			player, 390, FIRST_LEVEL, 0, HANDPOS, 17,
			SimulationCommandSource::System}}));
	sequences.push_back(runtime.submitCommand(
		1, SimulationCommand{SynchronizeActorStopCommand{
			opponent, 390, 20, 30, 6, true,
			SimulationCommandSource::NetworkPeer}}));
	sequences.push_back(runtime.submitCommand(
		2, SimulationCommand{SetStealthModeCommand{
			player, true, SimulationCommandSource::LocalPlayer}}));
	sequences.push_back(runtime.submitCommand(
		3, SimulationCommand{StopMovementCommand{
			player, SimulationCommandSource::System}}));
	sequences.push_back(runtime.submitCommand(
		3, SimulationCommand{SynchronizeTurnCommand{
			1, true, false, SimulationCommandSource::NetworkPeer}}));
	return sequences;
}

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
		const UINT32 savedOverlayCount = guiNumVideoOverlays;
		const VIDEO_OVERLAY savedOverlays[2] = {
			gVideoOverlays[0], gVideoOverlays[1] };
		gVideoOverlays[0] = VIDEO_OVERLAY{};
		gVideoOverlays[1] = VIDEO_OVERLAY{};
		gVideoOverlays[1].fAllocated = TRUE;
		gVideoOverlays[1].fDisabled = TRUE;
		guiNumVideoOverlays = 2;
		RainClipVideoOverlay();
		CHECK( !gVideoOverlays[0].fAllocated &&
		       !gVideoOverlays[0].pBackground &&
		       gVideoOverlays[1].fAllocated &&
		       !gVideoOverlays[1].pBackground,
		       "rain clipping skips sparse and disabled overlay slots without dereferencing null backgrounds" );
		gVideoOverlays[0] = savedOverlays[0];
		gVideoOverlays[1] = savedOverlays[1];
		guiNumVideoOverlays = savedOverlayCount;
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
		static_assert( !std::is_constructible<PackageIdentity, std::string>::value,
		               "package identities may only be issued by the package registry" );
		EngineHost<unsigned> host;
		host.screenController().reset( 7 );
		CHECK( host.screens().current() && host.screens().current()->state == 7,
		       "command-agnostic engine host owns screen state" );
		CHECK( host.beginInitialization() &&
		       host.runtimeSession().advancePackagesTo(
		           PackageBootstrapPhase::StartRuntime ) &&
		       host.markRunning() && host.beginShutdown() &&
		       host.runtimeSession().shutdownPackages() && host.markStopped(),
		       "command-agnostic engine host owns lifecycle" );
	}

	{
		ManualTimeSource time;
		MemoryInputSource input;
		MemoryByteStorage storage;
		RecordingAudioOutput audio;
		EngineServices services{
			time, ZeroRandomSource::instance(),
			storage, NullLogSink::instance(), input,
			audio, NullFramePresenter::instance(),
			NullAssetSource::instance()};
		RuntimeCapabilities hostCapabilities;
		hostCapabilities.add( "host.headless" );
		EngineHost<unsigned> host( services, CurrentContentApiVersion,
			NullPackageEventSink::instance(), hostCapabilities );
		TestLifecyclePackage package( "lifecycle.complete", PackageKind::Rules );
		package.persistOnConfigure = true;
		package.publishOnConfigure = true;
		package.usePackageRandomOnConfigure = true;
		package.localizeOnConfigure = true;
		package.defineOnConfigure = true;
		package.createEntityOnConfigure = true;
		package.playAudioOnConfigure = true;
		package.deferTasksOnConfigure = true;
		package.setRequiredServices({
			EngineServiceRequirement{ "engine.persistence", { 1, 0 } },
			EngineServiceRequirement{ "engine.runtime-messages", { 1, 0 } } });
		package.setRequiredCapabilities({ "host.headless" });
		package.setSaveStateSchema( 3 );
		CHECK( host.packages().registerPackage( package ) == PackageRegistrationError::None &&
		       host.packages().activate( "lifecycle.complete" ) == PackageActivationError::None,
		       "engine host prepares a package for coordinated lifecycle startup" );
		const RuntimeSessionAdvanceResult started =
			host.runtimeSession().advancePackagesTo( PackageBootstrapPhase::StartRuntime );
		const RuntimeSessionAdvanceResult repeated =
			host.runtimeSession().advancePackagesTo( PackageBootstrapPhase::Configure );
		const PackageCatalogSnapshot catalog = host.packageCatalog();
		const PackageCatalogEntry* catalogPackage = catalog.find( "lifecycle.complete" );
		const LocalizedTextView packageText =
			host.localization().resolve( "en", "ui.package-ready" );
		const DefinitionView packageDefinition =
			host.definitions().resolve( "rules", "package-ready", 1, 1 );
		CHECK( started && started.packages.completedPhases == 3 &&
		       !started.packages.rolledBack &&
		       repeated && package.bootstrapCalls == std::vector<int>({ 0, 1, 2 }) &&
		       package.observedIdentity &&
		       package.observedIdentity.id() == "lifecycle.complete" &&
		       package.observedTelemetry == &host.frameTelemetry() &&
		       package.observedMessageCapacity == 1024 &&
		       package.observedStoragePackageId == "lifecycle.complete" &&
		       package.packageSaveResult == PersistenceSaveResult::Success &&
		       package.invalidPackageSaveResult == PersistenceSaveResult::InvalidRequest &&
		       package.observedPublisherPackageId == "lifecycle.complete" &&
		       package.packagePublishResult &&
		       package.invalidPackagePublishResult.error ==
		           RuntimeMessagePublishError::InvalidTopic &&
		       package.observedRandomPackageId == "lifecycle.complete" &&
		       package.packageRandomResult && package.packageRandomResult.value < 100 &&
		       package.observedLocalizationPackageId == "lifecycle.complete" &&
		       package.localizationSetResult == LocalizationSetError::None &&
		       package.invalidLocalizationSetResult == LocalizationSetError::InvalidLocale &&
		       packageText && *packageText.text == "Package ready" &&
		       package.observedDefinitionPackageId == "lifecycle.complete" &&
		       package.definitionSetResult == DefinitionSetError::None &&
		       package.invalidDefinitionSetResult == DefinitionSetError::InvalidType &&
		       packageDefinition &&
		       *packageDefinition.payload == std::vector<std::uint8_t>({ 4, 2 }) &&
		       package.observedEntityPackageId == "lifecycle.complete" &&
		       package.packageEntityCreateResult &&
		       host.entities().alive( package.packageEntityCreateResult.id ) &&
		       package.observedAudioPackageId == "lifecycle.complete" &&
		       package.packageAudioPlayResult &&
		       package.invalidPackageAudioPlayResult.error ==
		           PackageAudioPlayError::InvalidGroup &&
		       audio.isPlaying( package.packageAudioPlayResult.playback ) &&
		       host.packageAudio().size() == 1 &&
		       package.observedTasksPackageId == "lifecycle.complete" &&
		       package.packageTaskResult && package.throwingPackageTaskResult &&
		       package.deferredTaskRuns == 0 && host.packageTasks().size() == 2 &&
		       catalogPackage && catalogPackage->descriptor.requiredServices.size() == 2 &&
		       catalogPackage->descriptor.requiredCapabilities ==
		           std::vector<std::string>({ "host.headless" }) &&
		       host.serviceCatalog().sealed(),
		       "package lifecycle advances missing phases once and treats completed targets idempotently" );
		const PackageSaveStateCaptureResult capturedState = host.capturePackageSaveState();
		const PackageSaveStateLoadResult validatedState =
			host.validatePackageSaveState( capturedState.snapshot );
		const PackageSaveStateLoadResult restoredState =
			host.restorePackageSaveState( capturedState.snapshot );
		PackageSaveStateSnapshot wrongSchema = capturedState.snapshot;
		wrongSchema.records[0].schemaVersion = 4;
		const PackageSaveStateLoadResult rejectedSchema =
			host.restorePackageSaveState( wrongSchema );
		CHECK( capturedState && capturedState.snapshot.records.size() == 1 &&
		       capturedState.snapshot.records[0].packageId == "lifecycle.complete" &&
		       capturedState.snapshot.records[0].packageVersion == "1.0" &&
		       capturedState.snapshot.records[0].schemaVersion == 3 &&
		       capturedState.snapshot.records[0].payload == package.saveStatePayload &&
		       validatedState && restoredState && restoredState.restored == 1 &&
		       package.saveStateCalls == 1 && package.validateStateCalls == 1 &&
		       package.loadStateCalls == 1 &&
		       package.loadedStatePayload == package.saveStatePayload &&
		       rejectedSchema.error == PackageSaveStateError::SchemaMismatch &&
		       package.validateStateCalls == 1 && package.loadStateCalls == 1,
		       "package registry captures, preflights, and restores bounded state in activation order" );
		const PackageResourceUsageSnapshot resources = host.packageResourceUsage();
		const PackageResourceUsage* packageResources = resources.find( "lifecycle.complete" );
		CHECK( packageResources && packageResources->active &&
		       packageResources->localizationEntries == 1 &&
		       packageResources->definitionEntries == 1 && packageResources->entities == 1 &&
		       packageResources->audioPlaybacks == 1 && packageResources->deferredTasks == 2 &&
		       packageResources->randomStreams == 1 &&
		       packageResources->randomValuesGenerated == 1 &&
		       resources.unattributedRecords == 0,
		       "live diagnostics attribute framework resource use to its owning package" );
		PersistenceHeader packageHeader{};
		std::vector<std::uint8_t> packagePayload;
		PackageStorage otherPackageStorage( "other.package", host.persistence() );
		CHECK( host.persistence().loadEnvelope(
		           PackageStorage::recordPath( "lifecycle.complete", "test-state" ),
		           0x504B4754u, 1, 1, packageHeader, packagePayload ) ==
		           PersistenceLoadResult::Success &&
		       packagePayload == std::vector<std::uint8_t>({ 8, 9 }) &&
		       otherPackageStorage.loadEnvelope(
		           "test-state", 0x504B4754u, 1, 1, packageHeader, packagePayload ) ==
		           PersistenceLoadResult::NotFound,
		       "package persistence writes bounded envelopes into isolated namespaces" );
		input.push( EngineInputEvent{ 10, 2, 7, 65, 0, 1, 0 } );
		const RuntimeMessagePublishResult published = host.runtimeMessages().publish(
			RuntimeMessageRequest{ "engine.test", "host.headless", { 4, 2 } } );
		const FrameRunResult frame = host.frameDriver().runFrame(
			[] { return FramePlan{ false, FramePresentMode::Paced }; }, [] {} );
		CHECK( frame.input.polled == 1 && package.inputEvents.size() == 1 &&
		       package.inputEvents[0].modifiers == 2 && package.inputEvents[0].primary == 65,
		       "runtime-started packages receive live mirrored input before the application frame" );
		CHECK( published && frame.messages.messages == 2 &&
		       package.receivedMessages.size() == 2 &&
		       package.receivedMessages[0].topic == "package.ready" &&
		       package.receivedMessages[0].source == "lifecycle.complete" &&
		       package.receivedMessages[0].payload == std::vector<std::uint8_t>({ 6, 1 }) &&
		       package.receivedMessages[1].payload == std::vector<std::uint8_t>({ 4, 2 }) &&
		       package.observedMessages == &host.runtimeMessages(),
		       "runtime-started packages exchange source-bound messages at the frame boundary" );
		CHECK( frame.runtimeUpdates.delivered == 1 && package.runtimeUpdates.size() == 1 &&
		       package.runtimeUpdates[0].frameSequence == 1 &&
		       package.runtimeUpdates[0].elapsedSincePreviousFrameMicroseconds == 0,
		       "runtime-started packages receive deterministic per-frame engine updates" );
		const PackageTaskQueueSnapshot taskSnapshot = host.packageTasks().snapshot();
		CHECK( package.deferredTaskRuns == 1 && taskSnapshot.queued.empty() &&
		       taskSnapshot.summary.executed == 1 && taskSnapshot.summary.failed == 1 &&
		       host.runtimeFaults().snapshot().records.back().kind ==
		           RuntimeFaultKind::DeferredTask,
		       "package deferred work runs at the frame boundary and contains failures" );
		const FrameTelemetrySnapshot telemetry = host.frameTelemetry().snapshot();
		CHECK( telemetry.summary.completedFrames == 1 && telemetry.samples.size() == 1 &&
		       telemetry.samples[0].sequence == frame.sequence &&
		       !telemetry.samples[0].presented,
		       "live engine host retains bounded value-only frame telemetry" );
		time.advanceMicroseconds( 40000 );
		const FrameRunResult simulationFrame = host.frameDriver().runFrame(
			[] { return FramePlan{ false, FramePresentMode::Paced }; }, [] {} );
		CHECK( simulationFrame.simulationTicks.scheduled == 2 &&
		       simulationFrame.simulationTicks.executed == 2 &&
		       simulationFrame.simulationTicks.dropped == 0 &&
		       package.simulationTicks.size() == 2 &&
		       package.simulationTicks[0].sequence == 1 &&
		       package.simulationTicks[1].simulatedTimeMicroseconds == 33334,
		       "live packages receive fixed-step ticks independently of render updates" );
		const RuntimeCheckpoint expectedCheckpoint = host.makeRuntimeCheckpoint();
		RuntimeCheckpoint loadedCheckpoint;
		const RuntimeCheckpointSaveError savedCheckpoint =
			host.saveRuntimeCheckpoint( "runtime/headless-checkpoint" );
		const RuntimeCheckpointLoadResult loadedCheckpointResult =
			host.loadRuntimeCheckpoint( "runtime/headless-checkpoint", loadedCheckpoint );
		CHECK( savedCheckpoint == RuntimeCheckpointSaveError::None &&
		       loadedCheckpointResult &&
		       loadedCheckpoint.compatibility == expectedCheckpoint.compatibility &&
		       loadedCheckpoint.completedFrames == 2 &&
		       loadedCheckpoint.completedSimulationTicks == 2 &&
		       loadedCheckpoint.activePackages.size() == 1 &&
		       loadedCheckpoint.activePackages[0].id == "lifecycle.complete" &&
		       loadedCheckpoint.activePackages[0].version == "1.0",
		       "live host persists a compatibility-gated package and progress checkpoint" );
		CHECK( host.beginInitialization() && host.markRunning() && host.beginShutdown(),
		       "runtime package test enters an orderly engine shutdown" );
		const RuntimeSessionShutdownResult stopped = host.runtimeSession().shutdownPackages();
		CHECK( stopped && stopped.packages.shutdownPhases == 3 &&
		       package.shutdownCalls == std::vector<int>({ 2, 1, 0 }) &&
		       package.deactivateCalls == 1 && host.packages().activationOrder().empty() &&
		       !host.localization().resolve( "en", "ui.package-ready" ) &&
		       !host.definitions().resolve( "rules", "package-ready", 1, 1 ) &&
		       !host.entities().alive( package.packageEntityCreateResult.id ) &&
		       !audio.isPlaying( package.packageAudioPlayResult.playback ) &&
		       host.packageAudio().size() == 0 &&
		       host.markStopped(),
		       "package lifecycle shuts down phases and active packages in reverse order" );
	}

	{
		EngineHost<unsigned> capabilityHost;
		TestLifecyclePackage missing( "capabilities.missing", PackageKind::Rules );
		missing.setRequiredCapabilities({ "host.not-installed" });
		TestLifecyclePackage invalid( "capabilities.invalid", PackageKind::Rules );
		invalid.setRequiredCapabilities({ "host.duplicate", "host.duplicate" });
		CHECK( capabilityHost.packages().registerPackage( invalid ) ==
		           PackageRegistrationError::InvalidManifest &&
		       capabilityHost.packages().registerPackage( missing ) ==
		           PackageRegistrationError::None &&
		       capabilityHost.packages().activate( "capabilities.missing" ) ==
		           PackageActivationError::None,
		       "capability contracts validate portable unique requirement lists" );
		const RuntimeSessionAdvanceResult result =
			capabilityHost.runtimeSession().advancePackagesTo( PackageBootstrapPhase::Configure );
		const PackageCapabilityContractFailure& failure =
			capabilityHost.packages().lastCapabilityContractFailure();
		const RuntimeFaultSnapshot faults = capabilityHost.runtimeFaults().snapshot();
		CHECK( !result && result.packages.error == PackageBootstrapError::MissingCapability &&
		       missing.bootstrapCalls.empty() && failure &&
		       failure.packageId == "capabilities.missing" &&
		       failure.capabilityId == "host.not-installed" &&
		       faults.records.size() == 1 &&
		       faults.records[0].kind == RuntimeFaultKind::CapabilityContract,
		       "missing runtime capabilities fail before package code with diagnostics" );
	}

	{
		EngineHost<unsigned> missingHost;
		TestLifecyclePackage missing( "services.missing", PackageKind::Rules );
		missing.setRequiredServices({
			EngineServiceRequirement{ "engine.not-installed", { 1, 0 } } });
		CHECK( missingHost.packages().registerPackage( missing ) == PackageRegistrationError::None &&
		       missingHost.packages().activate( "services.missing" ) == PackageActivationError::None,
		       "valid package service contracts register before host composition is sealed" );
		const RuntimeSessionAdvanceResult result =
			missingHost.runtimeSession().advancePackagesTo( PackageBootstrapPhase::Configure );
		const PackageServiceContractFailure& failure =
			missingHost.packages().lastServiceContractFailure();
		CHECK( !result && result.packages.error == PackageBootstrapError::MissingService &&
		       missing.bootstrapCalls.empty() && failure &&
		       failure.packageId == "services.missing" &&
		       failure.serviceId == "engine.not-installed" &&
		       failure.requiredVersion.major == 1,
		       "missing required services fail before the first package callback with diagnostics" );
	}

	{
		EngineHost<unsigned> versionHost;
		TestLifecyclePackage incompatible( "services.version", PackageKind::Rules );
		incompatible.setRequiredServices({
			EngineServiceRequirement{ "engine.persistence", { 2, 0 } } });
		TestLifecyclePackage invalid( "services.invalid", PackageKind::Rules );
		invalid.setRequiredServices({
			EngineServiceRequirement{ "engine.persistence", { 1, 0 } },
			EngineServiceRequirement{ "engine.persistence", { 1, 1 } } });
		CHECK( versionHost.packages().registerPackage( invalid ) ==
		           PackageRegistrationError::InvalidManifest &&
		       versionHost.packages().registerPackage( incompatible ) == PackageRegistrationError::None &&
		       versionHost.packages().activate( "services.version" ) == PackageActivationError::None,
		       "duplicate service requirements are rejected while valid contracts register" );
		const RuntimeSessionAdvanceResult result =
			versionHost.runtimeSession().advancePackagesTo( PackageBootstrapPhase::Configure );
		const PackageServiceContractFailure& failure =
			versionHost.packages().lastServiceContractFailure();
		CHECK( !result &&
		       result.packages.error == PackageBootstrapError::ServiceVersionMismatch &&
		       incompatible.bootstrapCalls.empty() && failure &&
		       failure.serviceId == "engine.persistence" &&
		       failure.requiredVersion.major == 2 &&
		       failure.availableVersion.major == 1,
		       "incompatible service versions fail before package code observes the host" );
	}

	{
		EngineHost<unsigned> host;
		TestLifecyclePackage package(
			"lifecycle.rollback", PackageKind::Rules,
			static_cast<int>(PackageBootstrapPhase::LoadContent) );
		package.deferTasksOnConfigure = true;
		host.packages().registerPackage( package );
		host.packages().activate( "lifecycle.rollback" );
		const PackageLifecycleAdvanceResult failed =
			host.packageLifecycle().advanceTo( PackageBootstrapPhase::StartRuntime );
		CHECK( !failed && failed.error == PackageBootstrapError::CallbackFailed &&
		       failed.phase == PackageBootstrapPhase::LoadContent && failed.rolledBack &&
		       failed.completedPhases == 0 &&
		       package.shutdownCalls == std::vector<int>({ 1, 0 }) &&
		       package.deferredTaskRuns == 0 && host.packageTasks().size() == 0,
		       "package lifecycle unwinds all earlier phases after a later startup failure" );
		const PackageLifecycleShutdownResult stopped = host.packageLifecycle().shutdown();
		CHECK( stopped && stopped.shutdownPhases == 0 && package.deactivateCalls == 1,
		       "rolled-back package lifecycle remains safe to deactivate during host shutdown" );
	}

	{
		MemoryInputSource input;
		MemoryLogSink log;
		EngineServices services{
			ZeroTimeSource::instance(), ZeroRandomSource::instance(),
			NullByteStorage::instance(), log, input};
		EngineHost<unsigned> host( services );
		TestLifecyclePackage invalidTopics( "runtime.invalid-topics", PackageKind::Extension );
		invalidTopics.setMessageTopics( { "invalid/topic" } );
		CHECK( host.packages().registerPackage( invalidTopics ) ==
		       PackageRegistrationError::InvalidManifest,
		       "package registration rejects invalid runtime message topics" );
		TestLifecyclePackage package( "runtime.unhealthy", PackageKind::Extension );
		package.setMessageTopics( { "engine.allowed" } );
		package.throwOnInput = true;
		package.throwOnRuntimeUpdate = true;
		host.packages().registerPackage( package );
		host.packages().activate( "runtime.unhealthy" );
		host.runtimeSession().advancePackagesTo( PackageBootstrapPhase::StartRuntime );
		host.runtimeMessages().publish(
			RuntimeMessageRequest{ "engine.denied", "host.headless", {} } );
		host.runtimeMessages().publish(
			RuntimeMessageRequest{ "engine.allowed", "host.headless", { 7 } } );
		for ( std::uint64_t sequence = 1; sequence <= 10; ++sequence )
		{
			input.push( EngineInputEvent{ sequence, 0, 1, 0, 0, sequence, 0 } );
			host.frameDriver().runFrame(
				[] { return FramePlan{ false, FramePresentMode::Paced }; }, [] {} );
		}
		const PackageCatalogSnapshot catalog = host.packageCatalog();
		const PackageCatalogEntry* entry = catalog.find( "runtime.unhealthy" );
		const RuntimeFaultSnapshot faults = host.runtimeFaults().snapshot();
		CHECK( entry && entry->runtimeHealth.inputCallbacks == 10 &&
		       entry->runtimeHealth.inputFailures == 10 &&
		       entry->runtimeHealth.runtimeUpdateCallbacks == 10 &&
		       entry->runtimeHealth.runtimeUpdateFailures == 10 &&
		       entry->runtimeHealth.messageCallbacks == 1 &&
		       entry->runtimeHealth.filteredMessages == 1 &&
		       entry->runtimeHealth.suppressedFailureLogs == 10,
		       "package catalog snapshots retain per-package runtime callback health" );
		CHECK( faults.summary.observed == 20 && faults.summary.retained == 20 &&
		       faults.records.size() == 20 && faults.records.front().sequence == 1 &&
		       faults.records.front().kind == RuntimeFaultKind::Input &&
		       faults.records.front().packageId == "runtime.unhealthy" &&
		       faults.records.back().kind == RuntimeFaultKind::RuntimeUpdate &&
		       faults.records.back().occurrence == 10,
		       "bounded fault journal retains every package failure despite log suppression" );
		CHECK( log.records().size() == 10,
		       "repeated package callback exceptions use bounded logarithmic logging" );
		CHECK( package.receivedMessages.size() == 1 &&
		       package.receivedMessages[0].topic == "engine.allowed" &&
		       entry && entry->descriptor.messageTopics ==
		           std::vector<std::string>({ "engine.allowed" }),
		       "declared package message topics filter traffic before entering mod code" );
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
		CHECK( !context.campaignSimulationEnabled() &&
		       context.runtime().simulationTicks().sinkCount() == 1,
		       "isolated game contexts do not route fixed ticks into process-global campaign state" );
		GameCapabilities editorCapabilities;
		editorCapabilities.editor = true;
		CHECK( context.setCapabilities( editorCapabilities ) && context.capabilities().isEditor() &&
		       context.hasCapability( GameCapability::ApplicationMapEditor ),
		       "game context accepts runtime capabilities before initialization" );
		CHECK( context.beginInitialization() &&
		       context.advancePackagesTo( PackageBootstrapPhase::StartRuntime ) &&
		       context.markRunning(),
		       "game context enters running lifecycle" );
		CHECK( !context.setCapabilities( GameCapabilities{} ),
		       "game context freezes runtime capabilities while running" );
		CHECK( !context.beginInitialization(), "game context rejects duplicate initialization" );
		CHECK( context.beginShutdown() && context.shutdownPackages() &&
		       context.markStopped(),
		       "game context completes shutdown lifecycle" );
		TestLifecyclePackage guardedPackage(
			"lifecycle.initialization-guard", PackageKind::Rules );
		CHECK( context.packages().registerPackage( guardedPackage ) ==
		           PackageRegistrationError::None &&
		       context.packages().activate( "lifecycle.initialization-guard" ) ==
		           PackageActivationError::None,
		       "game initialization guard test activates a retryable package" );
		{
			GameInitializationGuard initialization( context );
			const RuntimeSessionAdvanceResult loaded = context.advancePackagesTo(
				PackageBootstrapPhase::LoadContent );
			CHECK( initialization && loaded,
			       "game initialization guard starts and reaches partial package bootstrap" );
		}
		CHECK( context.lifecycle() == GameLifecycle::Stopped && guardedPackage.active() &&
		       guardedPackage.deactivateCalls == 0 &&
		       guardedPackage.shutdownCalls == std::vector<int>({ 1, 0 }) &&
		       context.packages().completedBootstrapPhases() == 0,
		       "game initialization guard rolls back incomplete package initialization" );
		{
			GameInitializationGuard retry( context );
			const RuntimeSessionAdvanceResult started = context.advancePackagesTo(
				PackageBootstrapPhase::StartRuntime );
			const RuntimeSessionTransitionResult running = retry.tryMarkRunning();
			CHECK( retry.beginResult() && started && running,
			       "game initialization guard permits a clean retry after rollback" );
		}
		CHECK( context.beginShutdown() && context.shutdownPackages() &&
		       context.markStopped() && guardedPackage.deactivateCalls == 1 &&
		       guardedPackage.shutdownCalls == std::vector<int>({ 1, 0, 2, 1, 0 }),
		       "retried guarded initialization shuts packages down exactly once" );

		TestLifecyclePackage reentrantGuardedPackage(
			"lifecycle.reentrant-initialization-guard", PackageKind::Rules );
		CHECK( context.packages().registerPackage( reentrantGuardedPackage ) ==
		           PackageRegistrationError::None &&
		       context.packages().activate(
		           "lifecycle.reentrant-initialization-guard" ) ==
		           PackageActivationError::None,
		       "reentrant initialization guard test activates its package" );
		{
			GameInitializationGuard initialization( context );
			reentrantGuardedPackage.initializationGuardDuringBootstrap =
				&initialization;
			const RuntimeSessionAdvanceResult configured = context.advancePackagesTo(
				PackageBootstrapPhase::Configure );
			CHECK( initialization.beginResult() && configured && initialization &&
			       reentrantGuardedPackage.initializationGuardCancelResult.error ==
			           RuntimeSessionError::PackageRollbackFailed &&
			       reentrantGuardedPackage.initializationGuardCancelResult.lifecycle ==
			           EngineLifecycle::Initializing &&
			       reentrantGuardedPackage.initializationGuardCancelResult.rollback.packages.error ==
			           PackageBootstrapShutdownError::OperationInProgress,
			       "initialization guard remains armed after reentrant rollback contention" );
		}
		const bool reentrantGuardPackageRemoved =
			context.lifecycle() == GameLifecycle::Stopped &&
			context.packages().completedBootstrapPhases() == 0 &&
			reentrantGuardedPackage.shutdownCalls == std::vector<int>({ 0 }) &&
			context.packages().deactivate(
				"lifecycle.reentrant-initialization-guard" ) &&
			context.packages().unregisterPackage(
				"lifecycle.reentrant-initialization-guard" );
		CHECK( reentrantGuardPackageRemoved,
		       "armed initialization guard retries rollback from its destructor" );
	}

	{
		GAME_SETTINGS settings = {};
		GAME_OPTIONS options = {};
		MemoryByteStorage storage;
		MemoryLogSink logs;
		EngineServices services{
			ZeroTimeSource::instance(), ZeroRandomSource::instance(), storage, logs };
		GameContext context( settings, options, GameCapabilities{}, services );
		RuntimeReportOptions reportOptions;
		reportOptions.enabled = true;
		reportOptions.path = "reports/live-runtime.json";
		ConfigureRuntimeReports( reportOptions );
		const RuntimeReportWriteResult written = WriteConfiguredRuntimeReport(
			context, RuntimeReportMoment::Startup );
		std::vector<std::uint8_t> reportBytes;
		const bool reportReadable = storage.readAll( reportOptions.path, reportBytes );
		ConfigureRuntimeReports( RuntimeReportOptions{} );
		const RuntimeReportWriteResult skipped = WriteConfiguredRuntimeReport(
			context, RuntimeReportMoment::Shutdown );
		CHECK( written.attempted && written && reportReadable &&
		       !reportBytes.empty() && reportBytes.front() == '{' &&
		       reportBytes.back() == '\n' && logs.records().size() == 1 &&
		       logs.records()[0].severity == LogSeverity::Info &&
		       logs.records()[0].message.find( reportOptions.path ) != std::string::npos &&
		       !skipped.attempted && skipped,
		       "configured application hook writes a live report without making it mandatory" );
	}

	{
		GAME_SETTINGS firstSettings = {};
		GAME_OPTIONS firstOptions = {};
		GAME_SETTINGS secondSettings = {};
		GAME_OPTIONS secondOptions = {};
		MemoryByteStorage storage;
		EngineServices firstServices{
			ZeroTimeSource::instance(), ZeroRandomSource::instance(), storage };
		EngineServices secondServices{
			ZeroTimeSource::instance(), ZeroRandomSource::instance(), storage };
		GameContext first( firstSettings, firstOptions, GameCapabilities{}, firstServices );
		GameContext second( secondSettings, secondOptions, GameCapabilities{}, secondServices );
		first.configuration().set( "test.save-profile", std::string( "first" ) );
		second.configuration().set( "test.save-profile", std::string( "second" ) );
		const bool firstReady = first.beginInitialization() &&
			first.advancePackagesTo( PackageBootstrapPhase::StartRuntime ) &&
			first.markRunning();
		const bool secondReady = second.beginInitialization() &&
			second.advancePackagesTo( PackageBootstrapPhase::StartRuntime ) &&
			second.markRunning();
		const std::string savePath = "SavedGames/SaveGame01.sav";
		const std::vector<std::uint8_t> domain = { 0x4a, 0x41, 0x32, 0x01 };
		storage.writeAll( savePath, domain );
		PreparedRuntimeSave prepared = PrepareRuntimeSave( first );
		std::vector<std::uint8_t> domainAfterCapture;
		const bool captureDidNotWrite =
			storage.readAll( savePath, domainAfterCapture ) &&
			domainAfterCapture == domain;
		const RuntimeSaveCommitResult committed =
			CommitRuntimeSave( first, savePath, std::move( prepared ) );
		RuntimeSaveContainer inspected;
		const RuntimeSaveContainerLoadResult containerLoaded =
			first.runtimeSaveContainers().inspect( savePath, inspected );
		const PreparedRuntimeLoad compatible =
			PrepareRuntimeLoad( first, savePath );
		const PreparedRuntimeLoad incompatible =
			PrepareRuntimeLoad( second, savePath );
		std::vector<std::uint8_t> corrupted;
		storage.readAll( savePath, corrupted );
		if ( !corrupted.empty() ) corrupted.front() ^= 0xffu;
		storage.writeAll( savePath, corrupted );
		const PreparedRuntimeLoad invalid =
			PrepareRuntimeLoad( first, savePath );
		const std::string plainPath = "SavedGames/SaveGamePlain.sav";
		storage.writeAll( plainPath, domain );
		const PreparedRuntimeLoad plain =
			PrepareRuntimeLoad( first, plainPath );
		CHECK( firstReady && secondReady && captureDidNotWrite && committed &&
		       containerLoaded && inspected.domainBytes == domain.size() &&
		       inspected.sections.size() == 2 &&
		       compatible.domainBytes == domain.size() && compatible &&
		       compatible.checkpoint.compatibility ==
		           first.runtime().compatibilityFingerprint() &&
		       !incompatible &&
		       incompatible.checkpointError ==
		           RuntimeCheckpointLoadError::IncompatibleRuntime &&
		       !invalid &&
		       invalid.containerError ==
		           RuntimeSaveContainerLoadError::IntegrityFailure &&
		       !plain &&
		       plain.containerError ==
		           RuntimeSaveContainerLoadError::InvalidOrUnsupported,
		       "one-file runtime saves require intact domain, checkpoint, and package sections" );
		CHECK(
		       std::string( RuntimeSaveContainerLoadErrorName(
		           invalid.containerError ) ) == "integrity-failure" &&
		       std::string( RuntimeCheckpointLoadErrorName(
		           incompatible.checkpointError ) ) == "incompatible-runtime",
		       "runtime save preflight exposes stable diagnostic classifications" );
	}

	{
		TestLifecyclePackage statePackage( "rules.saved-state", PackageKind::Rules );
		statePackage.setSaveStateSchema( 5 );
		GAME_SETTINGS settings = {};
		GAME_OPTIONS options = {};
		MemoryByteStorage storage;
		EngineServices services{
			ZeroTimeSource::instance(), ZeroRandomSource::instance(), storage };
		GameContext context( settings, options, GameCapabilities{}, services );
		const bool registered = context.packages().registerPackage( statePackage ) ==
			PackageRegistrationError::None;
		const bool activated = context.packages().activate( "rules.saved-state" ) ==
			PackageActivationError::None;
		const bool initializing = context.beginInitialization();
		const RuntimeSessionAdvanceResult advanced =
			context.advancePackagesTo( PackageBootstrapPhase::StartRuntime );
		const bool running = context.markRunning();
		const std::string savePath = "SavedGames/SaveGame02.sav";
		const std::vector<std::uint8_t> domain = { 7, 8, 9 };
		storage.writeAll( savePath, domain );
		PreparedRuntimeSave captured = PrepareRuntimeSave( context );
		const RuntimeSaveCommitResult written =
			CommitRuntimeSave( context, savePath, std::move( captured ) );
		PreparedRuntimeLoad ready = PrepareRuntimeLoad( context, savePath );
		PackageSaveArchive wrongSchema = ready.packages;
		wrongSchema.state.records[0].schemaVersion = 6;
		std::vector<std::uint8_t> checkpointBytes;
		const RuntimeCheckpointSaveError checkpointEncoded =
			context.runtime().runtimeCheckpoints().encode(
				ready.checkpoint, checkpointBytes );
		std::vector<std::uint8_t> packageBytes;
		const PackageSaveArchiveSaveError packageEncoded =
			context.packageSaveArchives().encode( wrongSchema, packageBytes );
		const std::string wrongPath = "SavedGames/SaveGameWrongSchema.sav";
		storage.writeAll( wrongPath, domain );
		const RuntimeSaveContainerSaveError wrongSealed =
			context.runtimeSaveContainers().seal( wrongPath, {
				{ 0x504b4843u, checkpointBytes },
				{ 0x54534750u, packageBytes } } );
		const PreparedRuntimeLoad contractMismatch =
			PrepareRuntimeLoad( context, wrongPath );
		CHECK( registered && activated && initializing && advanced && running && written &&
		       ready && ready.packages.state.records.size() == 1 &&
		       ready.packages.state.records[0].payload ==
		           statePackage.saveStatePayload &&
		       checkpointEncoded == RuntimeCheckpointSaveError::None &&
		       packageEncoded == PackageSaveArchiveSaveError::None &&
		       wrongSealed == RuntimeSaveContainerSaveError::None &&
		       !contractMismatch &&
		       contractMismatch.packageContractError ==
		           PackageSaveStateError::SchemaMismatch,
		       "embedded package state is mandatory and contract-checked before domain load" );
	}

	{
		TestLifecyclePackage stagedPackage(
			"rules.staged-save-boundary", PackageKind::Rules );
		stagedPackage.setSaveStateSchema( 3 );
		GAME_SETTINGS settings = {};
		GAME_OPTIONS options = {};
		MemoryByteStorage storage;
		EngineServices services{
			ZeroTimeSource::instance(), ZeroRandomSource::instance(), storage };
		GameContext context( settings, options, GameCapabilities{}, services );
		const bool ready =
			context.packages().registerPackage( stagedPackage ) ==
				PackageRegistrationError::None &&
			context.packages().activate( "rules.staged-save-boundary" ) ==
				PackageActivationError::None &&
			context.beginInitialization() &&
			context.advancePackagesTo( PackageBootstrapPhase::StartRuntime ) &&
			context.markRunning();
		const std::string savePath = "SavedGames/SaveGamePrepared.sav";
		const std::vector<std::uint8_t> domain = { 3, 1, 4, 1, 5 };
		storage.writeAll( savePath, domain );
		const std::vector<std::uint8_t> capturedPayload =
			stagedPackage.saveStatePayload;
		PreparedRuntimeSave prepared = PrepareRuntimeSave( context );
		std::vector<std::uint8_t> domainAfterPrepare;
		const bool prepareDidNotWrite =
			storage.readAll( savePath, domainAfterPrepare ) &&
			domainAfterPrepare == domain;
		stagedPackage.saveStatePayload = { 9, 8, 7, 6 };
		const RuntimeSaveCommitResult committed =
			CommitRuntimeSave( context, savePath, std::move( prepared ) );
		stagedPackage.saveStatePayload = capturedPayload;
		PreparedRuntimeLoad stagedLoad = PrepareRuntimeLoad( context, savePath );
		const bool stagedPayloadCaptured = stagedLoad &&
			stagedLoad.packages.state.records.size() == 1 &&
			stagedLoad.packages.state.records[0].payload == capturedPayload;
		std::vector<std::uint8_t> corruptedBytes;
		storage.readAll( savePath, corruptedBytes );
		if ( !corruptedBytes.empty() ) corruptedBytes.back() ^= 0x80u;
		storage.writeAll( savePath, corruptedBytes );
		const PackageSaveStateLoadResult restored =
			RestorePreparedRuntimeSave( context, stagedLoad );
		const PackageSaveStateLoadResult restoredAgain =
			RestorePreparedRuntimeSave( context, stagedLoad );
		const PreparedRuntimeLoad corrupted =
			PrepareRuntimeLoad( context, savePath );
		CHECK( ready && prepareDidNotWrite && committed &&
		       stagedPackage.saveStateCalls == 1 &&
		       stagedPayloadCaptured,
		       "prepared runtime save seals the one captured paused-game snapshot" );
		CHECK(
		       restored && restoredAgain && stagedPackage.validateStateCalls == 1 &&
		       stagedPackage.loadedStatePayload == capturedPayload &&
		       stagedPackage.loadStateCalls == 1,
		       "prepared runtime load survives backing-file changes and restores once" );
		CHECK(
		       !corrupted &&
		       corrupted.containerError ==
		           RuntimeSaveContainerLoadError::IntegrityFailure,
		       "the destructive-load gate rejects a corrupted in-file runtime trailer" );

		stagedPackage.saveStateSucceeds = false;
		const std::string failedPath = "SavedGames/SaveGameFailed.sav";
		storage.writeAll( failedPath, domain );
		PreparedRuntimeSave failedPreparation = PrepareRuntimeSave( context );
		const RuntimeSaveCommitResult failedCommit =
			CommitRuntimeSave(
				context, failedPath, std::move( failedPreparation ) );
		CHECK( !failedCommit &&
		       failedCommit.packageCaptureError ==
		           PackageSaveStateError::CallbackFailed &&
		       !storage.exists( failedPath ),
		       "a failed runtime-state capture cannot leave a partial save container" );
	}

	{
		TestLifecyclePackage randomOnlyPackage(
			"rules.random-only-save", PackageKind::Rules );
		randomOnlyPackage.usePackageRandomOnConfigure = true;
		GAME_SETTINGS settings = {};
		GAME_OPTIONS options = {};
		MemoryByteStorage storage;
		EngineServices services{
			ZeroTimeSource::instance(), ZeroRandomSource::instance(), storage };
		GameContext context( settings, options, GameCapabilities{}, services );
		const bool ready =
			context.packages().registerPackage( randomOnlyPackage ) ==
				PackageRegistrationError::None &&
			context.packages().activate( "rules.random-only-save" ) ==
				PackageActivationError::None &&
			context.beginInitialization() &&
			context.advancePackagesTo( PackageBootstrapPhase::StartRuntime ) &&
			context.markRunning();
		const std::string savePath = "SavedGames/SaveGame03.sav";
		storage.writeAll( savePath, { 4, 2 } );
		PreparedRuntimeSave prepared = PrepareRuntimeSave( context );
		const RuntimeSaveCommitResult written =
			CommitRuntimeSave( context, savePath, std::move( prepared ) );
		const PreparedRuntimeLoad inspected =
			PrepareRuntimeLoad( context, savePath );
		CHECK( ready && written && inspected &&
		       inspected.packages.state.records.empty() &&
		       inspected.packages.state.engineRecords.size() == 1 &&
		       inspected.packages.state.engineRecords[0].packageId ==
		           "rules.random-only-save" &&
		       inspected.packages.state.engineRecords[0].random.streams.size() == 1,
		       "random-only engine state is mandatory inside the one-file runtime save" );
	}

	{
		GAME_SETTINGS settings = {};
		GAME_OPTIONS options = {};
		MemoryByteStorage storage;
		EngineServices services{
			ZeroTimeSource::instance(), ZeroRandomSource::instance(), storage };
		GameContext context( settings, options, GameCapabilities{}, services );
		const bool ready = context.beginInitialization() &&
			context.advancePackagesTo( PackageBootstrapPhase::StartRuntime ) &&
			context.markRunning();
		const std::string savePath = "SavedGames/SaveGame04.sav";
		storage.writeAll( savePath, { 1, 2, 3 } );
		PreparedRuntimeSave prepared = PrepareRuntimeSave( context );
		const RuntimeSaveCommitResult written =
			CommitRuntimeSave( context, savePath, std::move( prepared ) );
		const PreparedRuntimeLoad inspected =
			PrepareRuntimeLoad( context, savePath );
		CHECK( ready && written && inspected &&
		       inspected.packages.state.records.empty() &&
		       inspected.packages.state.engineRecords.empty(),
		       "an empty package runtime remains valid in its explicit in-file state section" );
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
		CHECK( bounded && bounded.status == CommandProcessStatus::Completed &&
		       bounded.scheduled == 2 && bounded.applied == 1 &&
		       bounded.discarded == 1 && deferred.size() == 1 && deferred[0].command == 30,
		       "command passes count discards and defer commands produced by their handlers" );
	}

	{
		DeterministicCommandQueue<int> commands;
		commands.enqueue( 1, 10 );
		commands.enqueue( 1, 20 );
		commands.enqueue( 1, 30 );
		std::vector<int> handled;
		const CommandProcessingResult first = ProcessCommandsThrough(
			commands, 1, 2,
			[&handled]( int command, std::uint64_t, std::uint64_t ) {
				handled.push_back( command );
				return CommandDisposition::Applied;
			} );
		bool moreReady = false;
		const auto retained = commands.snapshotThrough( 1, 1, moreReady );
		const CommandProcessingResult second = ProcessCommandsThrough(
			commands, 1, 2,
			[&handled]( int command, std::uint64_t, std::uint64_t ) {
				handled.push_back( command );
				return CommandDisposition::Applied;
			} );
		CHECK( first.status == CommandProcessStatus::BudgetExhausted &&
		       first.scheduled == 2 && first.applied == 2 &&
		       retained.size() == 1 && retained[0].command == 30 && !moreReady &&
		       second.status == CommandProcessStatus::Completed &&
		       second.scheduled == 1 && second.applied == 1 && commands.empty() &&
		       handled == std::vector<int>({ 10, 20, 30 }),
		       "bounded command processing reports and resumes an exact FIFO budget" );

		commands.enqueue( 2, 40 );
		bool invoked = false;
		const CommandProcessingResult zeroBudget = ProcessCommandsThrough(
			commands, 2, 0,
			[&invoked]( int, std::uint64_t, std::uint64_t ) {
				invoked = true;
				return CommandDisposition::Applied;
			} );
		CHECK( zeroBudget.status == CommandProcessStatus::BudgetExhausted &&
		       zeroBudget.scheduled == 0 && !invoked && commands.size() == 1,
		       "a zero command budget exposes ready backlog without invoking handlers" );
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
		const TacticalEntityId player{ 7, 7007 };
		const TacticalEntityId opponent{ 31, 31031 };
		const TacticalSimulationSnapshot baseline =
			MakeHeadlessTacticalTurnBaseline( player, opponent );

		MemoryTacticalSimulation captureModel;
		RetrySelectedFireOnceExecutor captureExecutor{ captureModel };
		EngineRuntime<unsigned> captureRuntime;
		const std::vector<std::uint64_t> submitted =
			SubmitHeadlessTacticalTurn(
				captureRuntime, player, opponent );
		const HeadlessTacticalTurnRun captured =
			RunHeadlessTacticalTurn(
				captureRuntime, captureModel, captureExecutor, baseline );
		const std::vector<RecordedSimulationCommand> capturedJournal =
			captureRuntime.commandJournal().snapshot();

		std::vector<std::uint8_t> capturedBytes;
		const bool captureEncoded = EncodeSimulationCommandJournal(
			capturedJournal,
			captureRuntime.commandJournal().droppedCount(),
			capturedBytes );
		std::vector<RecordedSimulationCommand> decodedJournal;
		std::uint64_t decodedDropped = 1;
		const SimulationCommandJournalDecodeResult decoded =
			DecodeSimulationCommandJournal(
				capturedBytes, decodedJournal, decodedDropped );

		MemoryTacticalSimulation replayModel;
		RetrySelectedFireOnceExecutor replayExecutor{ replayModel };
		EngineRuntime<unsigned> replayRuntime;
		const CommandReplayStageResult staged =
			replayRuntime.stageCommandReplay( SimulationCommandReplay{
				decodedJournal, decodedDropped } );
		const HeadlessTacticalTurnRun replayed =
			RunHeadlessTacticalTurn(
				replayRuntime, replayModel, replayExecutor, baseline );
		const std::vector<RecordedSimulationCommand> replayedJournal =
			replayRuntime.commandJournal().snapshot();
		std::vector<std::uint8_t> replayedBytes;
		const bool replayEncoded = EncodeSimulationCommandJournal(
			replayedJournal,
			replayRuntime.commandJournal().droppedCount(),
			replayedBytes );

		std::vector<std::uint64_t> appliedOrder;
		for ( const HeadlessTacticalCommandObservation& observation
			: captured.observations )
			if ( observation.disposition == CommandDisposition::Applied )
				appliedOrder.push_back( observation.sequence );
		const std::vector<std::uint64_t> expectedOrder{
			submitted[1], submitted[4], submitted[2], submitted[3],
			submitted[5], submitted[0], submitted[6], submitted[7] };

		TacticalSimulationSnapshot expected = baseline;
		expected.currentTeam = 1;
		expected.inCombat = true;
		expected.completedTurns = 1;
		expected.actors[0] = TacticalSimulationActorState{
			player, 220, 10, 10, ANIM_CROUCH, 2, true, true };
		expected.actors[1] = TacticalSimulationActorState{
			opponent, 390, 20, 30, ANIM_STAND, 6, false, true };
		expected.shots.push_back( TacticalSimulationShot{
			player, 390, FIRST_LEVEL, 0, HANDPOS, 17,
			TacticalSimulationFireKind::CapturedSelection,
			2, submitted[3] } );

		const bool everyCaptureRecordApplied = std::all_of(
			capturedJournal.begin(), capturedJournal.end(),
			[]( const RecordedSimulationCommand& record ) {
				return record.status == CommandJournalStatus::Applied;
			} );
		CHECK(
			captured.resetError == TacticalSimulationResetError::None &&
			replayed.resetError == TacticalSimulationResetError::None &&
			captured.executorBound && replayed.executorBound &&
			!captured.observationFailure &&
			!replayed.observationFailure &&
			captured.completed && replayed.completed &&
			!captured.queueChanged && !replayed.queueChanged &&
			captured.sawBudgetExhaustion &&
			replayed.sawBudgetExhaustion &&
			captured.sawRetry && replayed.sawRetry &&
			captured.snapshot == expected &&
			replayed.snapshot == expected &&
			captured.snapshot == replayed.snapshot &&
			captured.observations == replayed.observations &&
			appliedOrder == expectedOrder &&
			capturedJournal.size() == submitted.size() &&
			everyCaptureRecordApplied &&
			captureEncoded &&
			decoded ==
				SimulationCommandJournalDecodeResult::Success &&
			decodedDropped == 0 &&
			staged == CommandReplayStageResult::Success &&
			replayEncoded &&
			replayedBytes == capturedBytes,
			"data-free tactical turn replay reproduces bounded ordering, retries, actor/world state, and final journal bytes" );
	}

	{
		const TacticalEntityId actor{ 1, 101 };
		const TacticalEntityId target{ 2, 202 };
		const SimulationCommand validMove{ MoveToGridCommand{
			actor, 100, WALKING, false, false,
			SimulationCommandSource::LocalPlayer } };
		CHECK(
			ValidateSimulationCommandDomain( validMove ) ==
				SimulationCommandDomainError::None &&
			ValidateSimulationCommandDomain( SimulationCommand{ EndTurnCommand{
				0xffu, SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidTeam &&
			ValidateSimulationCommandDomain( SimulationCommand{ ChangeStanceCommand{
				actor, 0xffu, SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidStance &&
			ValidateSimulationCommandDomain( SimulationCommand{ ChangeStanceCommand{
				actor, ANIM_STAND, SimulationCommandSource::System,
				static_cast<TacticalEventPolicy>( 0xffu ) } } ) ==
				SimulationCommandDomainError::InvalidEventPolicy &&
			ValidateSimulationCommandDomain( SimulationCommand{ BeginFireWeaponCommand{
				actor, -1, FIRST_LEVEL, 0,
				SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidTargetGrid &&
			ValidateSimulationCommandDomain( SimulationCommand{ BeginFireWeaponCommand{
				actor, 100, 7, 0,
				SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidTargetLevel &&
			ValidateSimulationCommandDomain( SimulationCommand{ BeginFireWeaponCommand{
				actor, 100, FIRST_LEVEL, PROFILE_Z_SIZE + 1,
				SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidTargetCubeLevel &&
			ValidateSimulationCommandDomain( SimulationCommand{
				BeginSelectedFireWeaponCommand{
					actor, 100, FIRST_LEVEL, 0, NUM_INV_SLOTS, 1,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidAttackingHand &&
			ValidateSimulationCommandDomain( SimulationCommand{
				BeginSelectedFireWeaponCommand{
					actor, 100, FIRST_LEVEL, 0, HANDPOS, MAXITEMS,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidAttackingWeapon &&
			ValidateSimulationCommandDomain( SimulationCommand{
				BeginSelectedFireWeaponCommand{
					actor, 100, FIRST_LEVEL, 0, HANDPOS, 1,
					SimulationCommandSource::LocalPlayer } } ) ==
				SimulationCommandDomainError::InvalidSource &&
			ValidateSimulationCommandDomain( SimulationCommand{ MoveToGridCommand{
				actor, WORLD_MAX, WALKING, false, false,
				SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidDestinationGrid &&
			ValidateSimulationCommandDomain( SimulationCommand{ MoveToGridCommand{
				actor, 100, WALKING, false, false,
				SimulationCommandSource::System,
				static_cast<TacticalMoveOrigin>( 0xff ),
				TacticalPendingActionPolicy::Clear } } ) ==
				SimulationCommandDomainError::InvalidMoveOrigin &&
			ValidateSimulationCommandDomain( SimulationCommand{ MoveToGridCommand{
				actor, 100, WALKING, false, false,
				SimulationCommandSource::System,
				TacticalMoveOrigin::System,
				static_cast<TacticalPendingActionPolicy>( 0xff ) } } ) ==
				SimulationCommandDomainError::InvalidPendingActionPolicy &&
			ValidateSimulationCommandDomain( SimulationCommand{ SetFacingCommand{
				actor, TacticalDirectionCount,
				SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidDirection &&
			ValidateSimulationCommandDomain( SimulationCommand{
				CycleScopeModeCommand{
					actor, TacticalNoTargetGrid,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::None &&
			ValidateSimulationCommandDomain( SimulationCommand{
				CycleScopeModeCommand{
					actor, WORLD_MAX,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidTargetGrid &&
			ValidateSimulationCommandDomain( SimulationCommand{
				CycleWeaponModeCommand{
					actor, SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::None &&
			ValidateSimulationCommandDomain( SimulationCommand{
				CancelDragCommand{
					actor, SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::None &&
			ValidateSimulationCommandDomain( SimulationCommand{
				ReloadWeaponCommand{
					actor, false, SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::None &&
			ValidateSimulationCommandDomain( SimulationCommand{
				SetWeaponReadyCommand{
					actor, 3, true, false,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::None &&
			ValidateSimulationCommandDomain( SimulationCommand{
				SetWeaponReadyCommand{
					actor, TacticalDirectionCount, false, false,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidDirection &&
			ValidateSimulationCommandDomain( SimulationCommand{
				TraverseObstacleCommand{
					actor, TacticalTraversalKind::JumpFence,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::None &&
			ValidateSimulationCommandDomain( SimulationCommand{
				TraverseObstacleCommand{
					actor, static_cast<TacticalTraversalKind>( 0xff ),
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidTraversalKind &&
			ValidateSimulationCommandDomain( SimulationCommand{
				ActivateWorldObjectCommand{
					actor, TacticalWorldObjectId{ 100, 7 }, 3,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::None &&
			ValidateSimulationCommandDomain( SimulationCommand{
				ActivateWorldObjectCommand{
					actor, TacticalWorldObjectId{ WORLD_MAX, 7 }, 3,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidObjectGrid &&
			ValidateSimulationCommandDomain( SimulationCommand{
				ActivateWorldObjectCommand{
					actor, TacticalWorldObjectId{ 100, 7 },
					TacticalDirectionCount,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidDirection &&
			ValidateSimulationCommandDomain( SimulationCommand{
				ApproachWorldObjectCommand{
					actor, TacticalWorldObjectId{ 100, 7 }, 3,
					101, WALKING, false, false,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::None &&
			ValidateSimulationCommandDomain( SimulationCommand{
				ApproachWorldObjectCommand{
					actor, TacticalWorldObjectId{ 100, 7 }, 3,
					WORLD_MAX, WALKING, false, false,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidDestinationGrid &&
			ValidateSimulationCommandDomain( SimulationCommand{
				ApproachWorldObjectCommand{
					actor, TacticalWorldObjectId{ 100, 7 }, 3,
					101, NUMANIMATIONSTATES, false, false,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidMovementMode &&
			ValidateSimulationCommandDomain( SimulationCommand{
				StartConversationCommand{
					actor, target, SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::None &&
			ValidateSimulationCommandDomain( SimulationCommand{
				StartConversationCommand{
					actor, actor, SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidTargetActor &&
			ValidateSimulationCommandDomain( SimulationCommand{
				ApproachConversationCommand{
					actor, target, WORLD_MAX, WALKING, false,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidDestinationGrid &&
			ValidateSimulationCommandDomain( SimulationCommand{
				EnterVehicleCommand{
					actor, target, TacticalDirectionCount, 0,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidDirection &&
			ValidateSimulationCommandDomain( SimulationCommand{
				EnterVehicleCommand{
					actor, target, 3, TacticalMaximumVehicleSeats,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidVehicleSeat &&
			ValidateSimulationCommandDomain( SimulationCommand{
				ApproachVehicleCommand{
					actor, target, 3, 0, 101, NUMANIMATIONSTATES, false,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidMovementMode &&
			ValidateSimulationCommandDomain( SimulationCommand{
				PickupWorldItemCommand{
					actor, TacticalWorldItemId{ 7, 707 }, 100, 0,
					TacticalWorldItemPickupKind::SpecificItem,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::None &&
			ValidateSimulationCommandDomain( SimulationCommand{
				PickupWorldItemCommand{
					actor, TacticalWorldItemId{ 7, 707 }, 100, -1,
					TacticalWorldItemPickupKind::SpecificItem,
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidWorldItemRenderHeight &&
			ValidateSimulationCommandDomain( SimulationCommand{
				PickupWorldItemCommand{
					actor, TacticalWorldItemId{ 7, 707 }, 100, 0,
					static_cast<TacticalWorldItemPickupKind>( 0xff ),
					SimulationCommandSource::System } } ) ==
				SimulationCommandDomainError::InvalidWorldItemPickupKind &&
				ValidateSimulationCommandDomain( SimulationCommand{
					PickupWorldItemCommand{
						actor, TacticalWorldItemId{ 7, 707 }, 100, 0,
						TacticalWorldItemPickupKind::SearchGrid,
						SimulationCommandSource::System } } ) ==
					SimulationCommandDomainError::InvalidWorldItem &&
				ValidateSimulationCommandDomain( SimulationCommand{
					StealFromActorCommand{
						actor, target, 101, FIRST_LEVEL,
						SimulationCommandSource::System } } ) ==
					SimulationCommandDomainError::None &&
				ValidateSimulationCommandDomain( SimulationCommand{
					StealFromActorCommand{
						actor, actor, 101, FIRST_LEVEL,
						SimulationCommandSource::System } } ) ==
					SimulationCommandDomainError::InvalidTargetActor &&
				ValidateSimulationCommandDomain( SimulationCommand{
					StealFromActorCommand{
						actor, target, WORLD_MAX, FIRST_LEVEL,
						SimulationCommandSource::System } } ) ==
					SimulationCommandDomainError::InvalidTargetGrid &&
				ValidateSimulationCommandDomain( SimulationCommand{
					StealFromActorCommand{
						actor, target, 101, 7,
						SimulationCommandSource::System } } ) ==
					SimulationCommandDomainError::InvalidTargetLevel &&
				ValidateSimulationCommandDomain( SimulationCommand{
					ExchangePositionsCommand{
						actor, target, 100, 101, FIRST_LEVEL,
						SimulationCommandSource::System } } ) ==
					SimulationCommandDomainError::None &&
				ValidateSimulationCommandDomain( SimulationCommand{
					ExchangePositionsCommand{
						actor, target, WORLD_MAX, 101, FIRST_LEVEL,
						SimulationCommandSource::System } } ) ==
					SimulationCommandDomainError::InvalidActorGrid &&
				ValidateSimulationCommandDomain( SimulationCommand{
					ExchangePositionsCommand{
						actor, target, 100, 101, 7,
						SimulationCommandSource::System } } ) ==
					SimulationCommandDomainError::InvalidActorLevel &&
				ValidateSimulationCommandDomain( SimulationCommand{ EndTurnCommand{
				1, static_cast<SimulationCommandSource>( 0xff ) } } ) ==
				SimulationCommandDomainError::InvalidSource,
				"all tactical execution paths share complete value-domain validation" );
	}

	{
		VEHICLETYPE* const previousVehicleList = pVehicleList;
		const UINT8 previousVehicleCount = ubNumberOfVehicles;
		const INT32 previousCapacity =
			gNewVehicle[0].iNewSeatingCapacities;
		VEHICLETYPE vehicleRecord = {};
		vehicleRecord.fValid = TRUE;
		vehicleRecord.ubVehicleType = 0;
		pVehicleList = &vehicleRecord;
		ubNumberOfVehicles = 1;
		gNewVehicle[0].iNewSeatingCapacities = 2;
		SOLDIERTYPE vehicle;
		vehicle.flags.uiStatusFlags |= SOLDIER_VEHICLE;
		vehicle.bVehicleID = 0;
		SOLDIERTYPE passenger;
		const bool acceptedBoundedCapacity =
			GetVehicleSeatingCapacity( 0 ) == 2;
		const bool rejectedLegacyEntrySeat =
			EnterVehicle(
				&vehicle, &passenger, TacticalMaximumVehicleSeats ) == FALSE;
		const bool rejectedLegacyAddSeat =
			AddSoldierToVehicle(
				&passenger, 0, TacticalMaximumVehicleSeats ) == FALSE;
		gNewVehicle[0].iNewSeatingCapacities = MAXPASSENGERS + 1;
		const bool rejectedMalformedCapacity =
			GetVehicleSeatingCapacity( 0 ) == 0 &&
			GetNumberInVehicle( 0 ) == 0 &&
			EnterVehicle( &vehicle, &passenger, 0 ) == FALSE;
		pVehicleList = previousVehicleList;
		ubNumberOfVehicles = previousVehicleCount;
		gNewVehicle[0].iNewSeatingCapacities = previousCapacity;
		CHECK( acceptedBoundedCapacity && rejectedLegacyEntrySeat &&
		       rejectedLegacyAddSeat && rejectedMalformedCapacity,
		       "legacy vehicle entry validates seat capacity before indexing passenger storage" );
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
			8, SimulationCommand{ChangeStanceCommand{
			    TacticalEntityId{17, 701}, 2, SimulationCommandSource::LocalPlayer}} );
		context.submitCommand(
			8, SimulationCommand{MoveToGridCommand{
			    TacticalEntityId{17, 701}, 1400, 6, true, false,
			    SimulationCommandSource::LocalPlayer}} );
		const auto stanceReady = context.commands().drainThrough( 8 );
		const auto& stance = std::get<ChangeStanceCommand>( stanceReady[0].command );
		const auto& move = std::get<MoveToGridCommand>( stanceReady[1].command );
		CHECK( stanceReady.size() == 2 &&
		       (stance.soldier == TacticalEntityId{17, 701}) &&
		       stance.stance == 2 &&
		       stance.source == SimulationCommandSource::LocalPlayer &&
		       move.soldier == stance.soldier && move.destinationGrid == 1400 &&
		       move.movementMode == 6 && move.reverse && !move.forceRestart,
		       "engine runtime carries generational stance and movement commands" );
		CHECK( context.submitRecordedCommand(
		           9, 500, SimulationCommand{BeginFireWeaponCommand{
		               TacticalEntityId{17, 700}, 1234, 0, 2,
		               SimulationCommandSource::Replay}} ) &&
		       !context.submitRecordedCommand(
		           9, 500, SimulationCommand{BeginFireWeaponCommand{
		               TacticalEntityId{17, 700}, 1234, 0, 2,
		               SimulationCommandSource::Replay}} ),
		       "engine runtime admits uniquely sequenced replay commands through the same journal" );
	}

	{
		TacticalCommandInbox inbox( TacticalCommandInboxLimits{ 3, 1, 2, 32, 5 } );
		ServiceCatalog services;
		CHECK( RegisterTacticalCommandService( services, inbox ) ==
		           EngineServiceRegistrationError::None,
		       "headless hosts can explicitly register package tactical command ingress" );
		const auto commandService = services.resolve( TacticalCommandServiceContract );
		const auto incompatibleService = services.resolve<TacticalCommandService>(
			TacticalCommandServiceId, EngineServiceVersion{ 1, 1 } );
		const SimulationCommand stanceCommand{ ChangeStanceCommand{
			TacticalEntityId{ 17, 701 }, 2, SimulationCommandSource::Replay } };
		TacticalCommandSubmissionResult submitted{
			TacticalCommandSubmissionError::InvalidCommand, 0 };
		if ( commandService )
			submitted = commandService.service->submit( "fixture.package", stanceCommand );
		std::uint64_t handledRequest = 0;
		TacticalEntityId handledSoldier;
		const TacticalCommandDrainResult drained = inbox.drain(
			[&]( const TacticalCommandRequest& request ) {
				handledRequest = request.requestId;
				handledSoldier = std::get<ChangeStanceCommand>( request.command ).soldier;
				return TacticalCommandDisposition::Accept;
			} );
		TacticalCommandInboxSnapshot snapshot;
		CHECK( commandService &&
		       incompatibleService.error == EngineServiceLookupError::IncompatibleVersion &&
		       submitted.requestId == 1 && drained.accepted == 1 &&
		       handledRequest == submitted.requestId &&
		       (handledSoldier == TacticalEntityId{ 17, 701 }) && inbox.empty() &&
		       commandService.service->snapshot( snapshot ) ==
		           TacticalCommandSnapshotError::None &&
		       snapshot.summary.accepted == 1 && snapshot.pending.empty(),
		       "package command ingress stays value-only and host-drained in the headless runtime" );
		CHECK( NullTacticalCommandService::instance()
		               .submit( "fixture.package", stanceCommand )
		               .error == TacticalCommandSubmissionError::CapacityReached,
		       "headless hosts have an explicit disabled tactical command service" );
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
		CHECK( screens.transitionTo( 100, overlay ) == StateTransitionResult::OverlayPushed &&
		       screens.current() && *screens.current() == 100 &&
		       screens.previous() && *screens.previous() == 1,
		       "state controller prepares overlay history for a scoped override" );
		{
			[[maybe_unused]] auto override =
				screens.scopedCurrentOverride( 7 );
			CHECK( screens.current() && *screens.current() == 7 &&
			       screens.stack().current() &&
			       screens.stack().current()->state == 100 &&
			       screens.stack().underlay() &&
			       screens.stack().underlay()->state == 1 &&
			       screens.previous() && *screens.previous() == 1,
			       "scoped state override changes the visible state without changing transition history" );
		}
		CHECK( screens.current() && *screens.current() == 100 &&
		       screens.stack().underlay() &&
		       screens.stack().underlay()->state == 1 &&
		       screens.previous() && *screens.previous() == 1,
		       "scoped state override restores the complete overlay stack" );
		{
			[[maybe_unused]] auto override =
				screens.scopedCurrentOverride( 7 );
			CHECK( screens.transitionTo( 2, overlay ) ==
			           StateTransitionResult::Replaced &&
			       screens.current() && *screens.current() == 7 &&
			       screens.stack().current() &&
			       screens.stack().current()->state == 2 &&
			       screens.previous() && *screens.previous() == 100,
			       "scoped state override keeps underlying transitions live" );
		}
		CHECK( screens.current() && *screens.current() == 2 &&
		       screens.previous() && *screens.previous() == 100,
		       "scoped state override reveals transitions completed inside its lifetime" );
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
		AssetMetadata layeredMetadata;
		CHECK( layeredAssets.metadata( "DATA/ITEMS.XML", layeredMetadata ) ==
		           AssetMetadataResult::Success &&
		       layeredMetadata.logicalPath == "data/items.xml" &&
		       layeredMetadata.provenance == "mod.example" &&
		       layeredMetadata.byteSize == 1,
		       "layered asset metadata resolves winning provenance without reading bytes" );
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
		ContentRegistry content( CurrentContentApiVersion );
		TestLifecyclePackage rules(
			GamePackage::Rules113, PackageKind::Rules, -1, nullptr, {},
			GamePackage::Rules113Version, { GameCapability::Rules113 } );
		TestGameplayBootstrapHooks arulcoHooks;
		LegacyCampaignPackage arulco( GameCapabilities{}, arulcoHooks );
		GameCapabilities ubCapabilities;
		ubCapabilities.campaign = GameCampaign::UnfinishedBusiness;
		TestGameplayBootstrapHooks unfinishedBusinessHooks;
		LegacyCampaignPackage unfinishedBusiness(
			ubCapabilities, unfinishedBusinessHooks );
		PackageRegistry packages( content );
		CHECK( packages.registerPackage( rules ) == PackageRegistrationError::None &&
		       packages.registerPackage( arulco ) == PackageRegistrationError::None &&
		       packages.registerPackage( unfinishedBusiness ) == PackageRegistrationError::None,
		       "campaign packages register through the versioned engine API" );
		CHECK( packages.activate( "ja2.arulco" ) == PackageActivationError::None && arulco.active() &&
		       rules.active() && packages.hasCapability( GameCapability::Rules113 ) &&
		       packages.hasCapability( GameCapability::CampaignArulco ) &&
		       !packages.hasCapability( GameCapability::CampaignUnfinishedBusiness ),
		       "campaign package activation selects its rules dependency at runtime" );
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
		TestGameplayBootstrapHooks campaignHooks;
		LegacyCampaignPackage campaign( GameCapabilities{}, campaignHooks );
		LegacyRulesPackage rules( GameCapabilities{}, campaignHooks );
		TestLifecyclePackage extension(
			"extension.campaign-bootstrap", PackageKind::Extension, -1, nullptr,
			{{ "ja2.arulco", "1.13" }} );
		std::vector<std::string> lifecycleTrace;
		campaignHooks.trace = &lifecycleTrace;
		extension.lifecycleTrace = &lifecycleTrace;
		ContentRegistry content( CurrentContentApiVersion );
		PackageRegistry packages( content );
		const bool activated =
			packages.registerPackage( extension ) == PackageRegistrationError::None &&
			packages.registerPackage( rules ) == PackageRegistrationError::None &&
			packages.registerPackage( campaign ) == PackageRegistrationError::None &&
			packages.activate( "extension.campaign-bootstrap" ) ==
				PackageActivationError::None;
		const PackageDescriptor* descriptorIdentity = &campaign.descriptor();
		const PackageCatalogSnapshot catalogBeforeBootstrap = packages.catalog();
		const RuntimeCompatibilityFingerprint fingerprintBeforeBootstrap =
			BuildRuntimeCompatibilityFingerprint(
				catalogBeforeBootstrap, std::vector<EngineServiceDescriptor>{},
				std::vector<RuntimeConfigurationEntry>{},
				catalogBeforeBootstrap.activeCapabilities,
				std::vector<DefinitionRecord>{} );
		const bool configured = packages.bootstrap( PackageBootstrapPhase::Configure ) ==
			PackageBootstrapError::None;
		const bool configureOrder = lifecycleTrace == std::vector<std::string>({
			"bootstrap:extension.campaign-bootstrap" });
		lifecycleTrace.clear();
		const bool contentLoaded = packages.bootstrap(
			PackageBootstrapPhase::LoadContent ) == PackageBootstrapError::None;
		const bool contentOrder = lifecycleTrace == std::vector<std::string>({
			"campaign:load-content:ja2",
			"bootstrap:extension.campaign-bootstrap" });
		lifecycleTrace.clear();
		const bool runtimeStarted = packages.bootstrap(
			PackageBootstrapPhase::StartRuntime ) == PackageBootstrapError::None;
		const bool runtimeOrder = lifecycleTrace == std::vector<std::string>({
			"campaign:start-runtime:ja2",
			"bootstrap:extension.campaign-bootstrap" });
		const PackageCatalogSnapshot catalogAfterBootstrap = packages.catalog();
		const RuntimeCompatibilityFingerprint fingerprintAfterBootstrap =
			BuildRuntimeCompatibilityFingerprint(
				catalogAfterBootstrap, std::vector<EngineServiceDescriptor>{},
				std::vector<RuntimeConfigurationEntry>{},
				catalogAfterBootstrap.activeCapabilities,
				std::vector<DefinitionRecord>{} );
		CHECK( activated && configured && configureOrder &&
		       contentLoaded && runtimeStarted &&
		       contentOrder && runtimeOrder &&
		       packages.activationOrder() == std::vector<std::string>({
		           GamePackage::Rules113, "ja2.arulco",
		           "extension.campaign-bootstrap" }) &&
		       descriptorIdentity == &campaign.descriptor() &&
		       campaign.descriptor().content.id == "ja2.arulco" &&
		       campaign.descriptor().content.version == "1.13" &&
		       fingerprintBeforeBootstrap == fingerprintAfterBootstrap &&
		       campaignHooks.contentCampaigns ==
		           std::vector<GameCampaign>({ GameCampaign::Arulco }) &&
		       campaignHooks.runtimeCampaigns ==
		           std::vector<GameCampaign>({ GameCampaign::Arulco }),
		       "compiled rules and campaign hooks own exact phase order without changing package identity or compatibility" );
		const PackageBootstrapShutdownResult shutdown = packages.shutdownBootstrap();
		CHECK( shutdown && shutdown.shutdownPhases == 3 &&
		       extension.shutdownCalls == std::vector<int>({ 2, 1, 0 }) &&
		       campaignHooks.contentCampaigns.size() == 1 &&
		       campaignHooks.runtimeCampaigns.size() == 1 &&
		       packages.isActive( GamePackage::Rules113 ) &&
		       packages.isActive( "ja2.arulco" ),
		       "rules and campaign lifecycle shutdown preserve intentionally process-lifetime legacy state" );
	}

	{
		TestGameplayBootstrapHooks failingHooks;
		failingHooks.throwOnLoadContent = true;
		LegacyCampaignPackage campaign( GameCapabilities{}, failingHooks );
		LegacyRulesPackage rules( GameCapabilities{}, failingHooks );
		TestLifecyclePackage extension(
			"extension.after-campaign-failure", PackageKind::Extension, -1, nullptr,
			{{ "ja2.arulco", "1.13" }} );
		std::vector<std::string> lifecycleTrace;
		failingHooks.trace = &lifecycleTrace;
		extension.lifecycleTrace = &lifecycleTrace;
		ContentRegistry content( CurrentContentApiVersion );
		PackageRegistry packages( content );
		const bool ready =
			packages.registerPackage( rules ) == PackageRegistrationError::None &&
			packages.registerPackage( campaign ) == PackageRegistrationError::None &&
			packages.registerPackage( extension ) == PackageRegistrationError::None &&
			packages.activate( "extension.after-campaign-failure" ) ==
				PackageActivationError::None &&
			packages.bootstrap( PackageBootstrapPhase::Configure ) ==
				PackageBootstrapError::None;
		lifecycleTrace.clear();
		const PackageBootstrapResult failed = packages.bootstrapDetailed(
			PackageBootstrapPhase::LoadContent );
		bool detailedFailurePreserved = false;
		try
		{
			if (failed.callbackException)
				std::rethrow_exception(failed.callbackException);
		}
		catch ( const std::runtime_error& error )
		{
			detailedFailurePreserved =
				std::string( error.what() ) == "test campaign content exception";
		}
		CHECK( ready && failed.error == PackageBootstrapError::CallbackFailed &&
		       failed.failedPhaseRollback.shutdownPhases == 1 &&
		       failed.failedPhaseRollback.callbacks == 1 &&
		       failed.failedPhaseRollback.callbackFailures == 0 &&
		       packages.completedBootstrapPhases() == 1 &&
		       extension.bootstrapCalls == std::vector<int>({ 0 }) &&
		       detailedFailurePreserved &&
		       lifecycleTrace == std::vector<std::string>({
		           "campaign:load-content:ja2" }),
		       "rules content failure preserves diagnostics and rolls back before campaigns or extensions observe the phase" );
		failingHooks.throwOnLoadContent = false;
		const PackageBootstrapResult unsafeRetry = packages.bootstrapDetailed(
			PackageBootstrapPhase::LoadContent );
		CHECK( !unsafeRetry &&
		       failingHooks.contentCampaigns ==
		           std::vector<GameCampaign>({ GameCampaign::Arulco }),
		       "a partially failed process-lifetime campaign phase is never replayed in-process" );
		packages.shutdownBootstrap();
	}

	{
		TestGameplayBootstrapHooks failingHooks;
		failingHooks.throwOnStartRuntime = true;
		LegacyCampaignPackage campaign( GameCapabilities{}, failingHooks );
		LegacyRulesPackage rules( GameCapabilities{}, failingHooks );
		ContentRegistry content( CurrentContentApiVersion );
		PackageRegistry packages( content );
		PackageLifecycle lifecycle( packages );
		const bool ready =
			packages.registerPackage( rules ) == PackageRegistrationError::None &&
			packages.registerPackage( campaign ) == PackageRegistrationError::None &&
			packages.activate( "ja2.arulco" ) == PackageActivationError::None;
		const PackageLifecycleAdvanceResult failed =
			lifecycle.advanceTo( PackageBootstrapPhase::StartRuntime );
		bool detailedFailurePreserved = false;
		try
		{
			if (failed.callbackException)
				std::rethrow_exception(failed.callbackException);
		}
		catch ( const std::runtime_error& error )
		{
			detailedFailurePreserved =
				std::string( error.what() ) == "test campaign runtime exception";
		}
		failingHooks.throwOnStartRuntime = false;
		const PackageLifecycleAdvanceResult unsafeRetry =
			lifecycle.advanceTo( PackageBootstrapPhase::StartRuntime );
		CHECK( ready && !failed &&
		       failed.error == PackageBootstrapError::CallbackFailed &&
		       failed.phase == PackageBootstrapPhase::StartRuntime &&
		       failed.rolledBack && failed.completedPhases == 0 &&
		       detailedFailurePreserved && !unsafeRetry &&
		       failingHooks.contentCampaigns ==
		           std::vector<GameCampaign>({ GameCampaign::Arulco }) &&
		       failingHooks.runtimeCampaigns ==
		           std::vector<GameCampaign>({ GameCampaign::Arulco }),
		       "campaign-owned runtime startup preserves diagnostics and cannot replay a partial process-lifetime attempt" );
		lifecycle.rollback();
	}

	{
		TestGameplayBootstrapHooks campaignHooks;
		LegacyCampaignPackage campaign( GameCapabilities{}, campaignHooks );
		LegacyRulesPackage rules( GameCapabilities{}, campaignHooks );
		TestLifecyclePackage extension(
			"extension.retry-after-campaign", PackageKind::Extension, -1, nullptr,
			{{ "ja2.arulco", "1.13" }} );
		extension.failOncePhase =
			static_cast<int>( PackageBootstrapPhase::LoadContent );
		ContentRegistry content( CurrentContentApiVersion );
		PackageRegistry packages( content );
		PackageLifecycle lifecycle( packages );
		const bool ready =
			packages.registerPackage( rules ) == PackageRegistrationError::None &&
			packages.registerPackage( campaign ) == PackageRegistrationError::None &&
			packages.registerPackage( extension ) == PackageRegistrationError::None &&
			packages.activate( "extension.retry-after-campaign" ) ==
				PackageActivationError::None;
		const PackageLifecycleAdvanceResult first =
			lifecycle.advanceTo( PackageBootstrapPhase::LoadContent );
		const PackageLifecycleAdvanceResult retry =
			lifecycle.advanceTo( PackageBootstrapPhase::LoadContent );
		CHECK( ready && !first && first.rolledBack &&
		       first.phase == PackageBootstrapPhase::LoadContent &&
		       first.completedPhases == 0 && retry &&
		       campaignHooks.contentCampaigns ==
		           std::vector<GameCampaign>({ GameCampaign::Arulco }) &&
		       extension.bootstrapCalls == std::vector<int>({ 0, 1, 0, 1 }),
		       "a later extension rollback can retry without reloading process-lifetime rules tables" );
		lifecycle.rollback();
	}

	{
		TestGameplayBootstrapHooks unfinishedBusinessHooks;
		GameCapabilities unfinishedBusinessCapabilities;
		unfinishedBusinessCapabilities.campaign = GameCampaign::UnfinishedBusiness;
		LegacyCampaignPackage unfinishedBusiness(
			unfinishedBusinessCapabilities, unfinishedBusinessHooks );
		LegacyRulesPackage rules(
			unfinishedBusinessCapabilities, unfinishedBusinessHooks );
		ContentRegistry content( CurrentContentApiVersion );
		PackageRegistry packages( content );
		const bool started =
			packages.registerPackage( rules ) == PackageRegistrationError::None &&
			packages.registerPackage( unfinishedBusiness ) ==
				PackageRegistrationError::None &&
			packages.activate( "ja2.unfinished-business" ) ==
				PackageActivationError::None &&
			packages.bootstrap( PackageBootstrapPhase::Configure ) ==
				PackageBootstrapError::None &&
			packages.bootstrap( PackageBootstrapPhase::LoadContent ) ==
				PackageBootstrapError::None &&
			packages.bootstrap( PackageBootstrapPhase::StartRuntime ) ==
				PackageBootstrapError::None;
		CHECK( started && unfinishedBusinessHooks.contentCampaigns ==
		           std::vector<GameCampaign>({ GameCampaign::UnfinishedBusiness }) &&
		       unfinishedBusinessHooks.runtimeCampaigns ==
		           std::vector<GameCampaign>({ GameCampaign::UnfinishedBusiness }) &&
		       unfinishedBusiness.descriptor().content.id ==
		           "ja2.unfinished-business" &&
		       packages.hasCapability(
		           GameCapability::CampaignUnfinishedBusiness ),
		       "rules and campaign bootstrap select the JA2 or Unfinished Business legacy hooks by package capability" );
		packages.shutdownBootstrap();
	}

	{
		GameContext& compiledContext = GetGameContext();
		Ja2SoldierRepository& soldierRepository = compiledContext.soldiers();
		soldierRepository.initializeSlots();
		CHECK( compiledContext.runtime().hasSimulationCommandExecutor() &&
		       &compiledContext.runtime().simulationCommandExecutor() !=
		           &NullSimulationCommandExecutor::instance(),
		       "production composition binds one runtime-owned tactical command executor" );
		CHECK( &compiledContext.soldiers() == &GetJa2SoldierRepository() &&
		       compiledContext.soldiers().capacity() == TOTAL_SOLDIERS &&
		       soldierRepository.resolve( 0 ) ==
		           &soldierRepository.record( 0 ),
		       "application composition owns the bound JA2 soldier repository" );
		compiledContext.screenController().reset( 7 );
		compiledContext.screenController().transitionTo(
			9, []( UINT32 ) { return false; } );
		const bool previousScreenOwnedByController =
			GetPreviousScreen() == 7;
		compiledContext.screenController().reset( 9 );
		bool currentScreenOwnedByController = false;
		{
			[[maybe_unused]] auto override =
				OverrideCurrentScreen( 14 );
			currentScreenOwnedByController =
				GetCurrentScreen() == 14 &&
				compiledContext.screenController().current() &&
				*compiledContext.screenController().current() == 14;
		}
		currentScreenOwnedByController =
			currentScreenOwnedByController &&
			GetCurrentScreen() == 9;
		SetPendingNewScreen( 11 );
		const bool pendingScreenOwnedByController =
			GetPendingNewScreen() == 11 &&
			compiledContext.screenController().pending() &&
			*compiledContext.screenController().pending() == 11;
		SetPendingNewScreen( NO_PENDING_SCREEN );
		CHECK( currentScreenOwnedByController &&
		       previousScreenOwnedByController &&
		       GetPreviousScreen() == NO_PENDING_SCREEN &&
		       pendingScreenOwnedByController &&
		       GetPendingNewScreen() == NO_PENDING_SCREEN &&
		       !compiledContext.screenController().hasPending(),
		       "current, pending, and previous application screen state have one controller-owned representation" );
		LegacyCampaignPackage& compiledPackage = GetCompiledCampaignPackage();
		LegacyRulesPackage& compiledRules = GetCompiledRulesPackage();
		const std::string& packageId = compiledPackage.descriptor().content.id;
		const bool fallbackSelectionDeferred =
			compiledContext.packages().activeCampaign().empty() &&
			compiledContext.packages().find( packageId ) != nullptr &&
			compiledContext.packages().find( GamePackage::Rules113 ) != nullptr &&
			!compiledPackage.active() && !compiledRules.active();
		const PackageActivationError fallbackActivated =
			compiledContext.packages().activate( packageId );
		CHECK( fallbackSelectionDeferred &&
		       fallbackActivated == PackageActivationError::None &&
		       compiledContext.packages().activeCampaign() == packageId &&
		       compiledContext.packages().isActive( packageId ) && compiledPackage.active() &&
		       compiledContext.packages().isActive( GamePackage::Rules113 ) &&
		       compiledRules.active() &&
		       compiledContext.hasCapability( GameCapability::Rules113 ) &&
		       compiledContext.hasCapability(
		           compiledContext.capabilities().isUnfinishedBusiness()
		               ? GameCapability::CampaignUnfinishedBusiness
		               : GameCapability::CampaignArulco ),
		       "the registered legacy rules and campaign remain a deferred startup fallback graph" );
		CHECK( compiledPackage.capabilities().campaign == compiledContext.capabilities().campaign,
		       "campaign adapter preserves the compiled JA2 or UB compatibility default" );
		CHECK( compiledContext.stateRegistry().size() == MAX_SCREENS &&
		       compiledContext.stateRegistry().contains( GAME_SCREEN ) &&
		       compiledContext.stateRegistry().contains( MAP_SCREEN ),
		       "live JA2 screens are bound through the generic runtime state registry" );
		CHECK( &compiledContext.log() == &GetPlatformLogSink(),
		       "application composition root binds the SDL logging adapter" );
		CHECK( compiledContext.campaignSimulationEnabled() &&
		       compiledContext.runtime().simulationTicks().sinkCount() == 2,
		       "application composition registers engine-owned campaign pacing before package ticks" );
		CHECK( &compiledContext.services().time == &GetPlatformTimeSource() &&
		       &compiledContext.services().random == &GetGameRandomSource() &&
		       &compiledContext.services().storage == &GetPlatformByteStorage() &&
		       &compiledContext.services().input == &GetPlatformInputSource() &&
		       &compiledContext.services().audio == &GetPlatformAudioOutput() &&
		       &compiledContext.services().frames == &GetPlatformFramePresenter() &&
		       &compiledContext.services().frames == &GetLegacyFramePresenter() &&
		       &compiledContext.services().frameInvalidation ==
		           &GetPlatformFrameInvalidator() &&
		       &compiledContext.services().frameInvalidation ==
		           &GetLegacyFrameInvalidator() &&
		       &compiledContext.services().renderSurfaces ==
		           &GetPlatformRenderSurfaceAccess() &&
		       &compiledContext.services().renderSurfaces ==
		           &GetLegacyRenderSurfaceAccess() &&
		       &compiledContext.services().renderCommands ==
		           &GetPlatformRenderCommands() &&
		       &compiledContext.services().renderCommands ==
		           &GetLegacyRenderCommands() &&
		       &compiledContext.services().assets == &compiledContext.packages().assets() &&
		       &compiledContext.persistence().storage() == &GetPlatformByteStorage() &&
		       compiledContext.services().assets.containsSource( &GetPlatformAssetSource() ),
		       "application composition root binds platform service adapters" );

		const std::uint32_t previousEntityIncarnation =
			NextJa2TacticalEntityIncarnation();
		const std::uint32_t issuedEntityIncarnation =
			IssueJa2TacticalEntityIncarnation();
		const std::uint32_t nextEntityIncarnation =
			NextJa2TacticalEntityIncarnation();
		RestoreJa2TacticalEntityIncarnationSequence(
			previousEntityIncarnation );
		CHECK( issuedEntityIncarnation == previousEntityIncarnation &&
		       nextEntityIncarnation == previousEntityIncarnation + 1 &&
		       NextJa2TacticalEntityIncarnation() ==
		           previousEntityIncarnation,
		       "tactical entity directory exclusively owns its incarnation sequence" );

		const CampaignClockSession::Snapshot previousCampaignClock =
			CaptureJa2CampaignClock();
		InitializeJa2CampaignClock( 90061 );
		AdvanceJa2CampaignClockUncommitted( 60 );
		const CampaignClockSession::Snapshot uncommittedCampaignClock =
			CaptureJa2CampaignClock();
		const CampaignClockSession::AdvanceCommit campaignClockCommit =
			CommitJa2CampaignClockAdvance();
		const CampaignClockSession::Snapshot committedCampaignClock =
			CaptureJa2CampaignClock();
		const CampaignClockSession::Snapshot expectedUncommittedCampaignClock{
			90121, 90061, 1, 1, 1 };
		const CampaignClockSession::Snapshot expectedCommittedCampaignClock{
			90121, 90121, 1, 1, 2 };
		CHECK( &compiledContext.runtime().campaignClockSession().snapshot() ==
		           &CaptureJa2CampaignClock() &&
		       uncommittedCampaignClock == expectedUncommittedCampaignClock &&
		       !campaignClockCommit.movedBackward &&
		       committedCampaignClock == expectedCommittedCampaignClock &&
		       GetWorldTotalSeconds() == committedCampaignClock.totalSeconds &&
		       GetWorldDay() == committedCampaignClock.day &&
		       GetWorldHour() == committedCampaignClock.hour &&
		       GetWorldMinutes() == committedCampaignClock.minute,
		       "application campaign-clock gateway is the sole owner behind established clock accessors" );
		const auto campaignClockService =
			compiledContext.serviceCatalog().resolve( CampaignClockServiceContract );
		CampaignClockSession::Snapshot packageCampaignClock;
		CHECK( campaignClockService &&
		       campaignClockService.service ==
		           &compiledContext.runtime().campaignClockService() &&
		       campaignClockService.service->capture( packageCampaignClock ) ==
		           CampaignClockCaptureResult::Success &&
		       packageCampaignClock == committedCampaignClock,
		       "application registers its live campaign clock as a read-only package service" );
		RestoreJa2CampaignClockSession( previousCampaignClock );

		const auto campaignEventService =
			compiledContext.serviceCatalog().resolve( CampaignEventServiceContract );
		CampaignEventQueue& liveCampaignEventQueue =
			compiledContext.runtime().campaignEventQueue();
		std::vector<CampaignEventSnapshot> previousCampaignEvents;
		const bool previousCampaignEventsCaptured =
			liveCampaignEventQueue.capture( previousCampaignEvents );
		const CampaignEventQueueError campaignEventFixtureInstalled =
			liveCampaignEventQueue.replace( {
				{ 190800, 71, 0, ONETIME_EVENT, 17, SEF_PREVENT_DELETION },
				{ 190800, 72, 3600, PERIODIC_EVENT, 18,
					SEF_DELETION_PENDING } } );
		STRATEGICEVENT* const firstCampaignEvent = liveCampaignEventQueue.head();
		STRATEGICEVENT* const secondCampaignEvent =
			firstCampaignEvent ? firstCampaignEvent->next : nullptr;
		STRATEGICEVENT* const gatewayCampaignEvent =
			AddAdvancedStrategicEvent( ONETIME_EVENT, 19, 190900, 73 );
		const bool campaignEventGatewayOwned =
			gatewayCampaignEvent != nullptr &&
			GetStrategicEventListHead() == liveCampaignEventQueue.head() &&
			DeleteStrategicEvent( 19, 73 ) == TRUE &&
			liveCampaignEventQueue.size() == 2;
		CampaignEventQueueSnapshot liveCampaignEvents;
		const CampaignEventCaptureResult liveCampaignEventCapture =
			campaignEventService
				? campaignEventService.service->capture( liveCampaignEvents )
				: CampaignEventCaptureResult::Unavailable;
		const std::size_t liveCampaignEventCapacity =
			liveCampaignEvents.events().capacity();
		const CampaignEventCaptureResult repeatedCampaignEventCapture =
			campaignEventService
				? campaignEventService.service->capture( liveCampaignEvents )
				: CampaignEventCaptureResult::Unavailable;
		Ja2CampaignEventAdapter boundedCampaignEvents( 1 );
		const CampaignEventCaptureResult boundedCampaignEventCapture =
			boundedCampaignEvents.capture( liveCampaignEvents );
		if( secondCampaignEvent )
			secondCampaignEvent->next = firstCampaignEvent;
		const CampaignEventCaptureResult cyclicCampaignEventCapture =
			campaignEventService
				? campaignEventService.service->capture( liveCampaignEvents )
				: CampaignEventCaptureResult::Unavailable;
		if( secondCampaignEvent )
			secondCampaignEvent->next = nullptr;
		const CampaignEventQueueError previousCampaignEventsRestored =
			liveCampaignEventQueue.replace( previousCampaignEvents );
		CHECK( previousCampaignEventsCaptured &&
		       campaignEventFixtureInstalled == CampaignEventQueueError::None &&
		       campaignEventGatewayOwned &&
		       previousCampaignEventsRestored == CampaignEventQueueError::None &&
		       GetStrategicEventListHead() == liveCampaignEventQueue.head() &&
		       campaignEventService &&
		       campaignEventService.service == &GetJa2CampaignEventAdapter() &&
		       liveCampaignEventCapture == CampaignEventCaptureResult::Success &&
		       repeatedCampaignEventCapture ==
		           CampaignEventCaptureResult::Success &&
		       liveCampaignEvents.size() == 2 &&
		       liveCampaignEvents.events().capacity() ==
		           liveCampaignEventCapacity &&
		       liveCampaignEvents.events()[0] ==
		           ( CampaignEventSnapshot{
			           190800, 71, 0, ONETIME_EVENT, 17,
			           SEF_PREVENT_DELETION } ) &&
		       liveCampaignEvents.events()[1] ==
		           ( CampaignEventSnapshot{
			           190800, 72, 3600, PERIODIC_EVENT, 18,
			           SEF_DELETION_PENDING } ) &&
		       boundedCampaignEventCapture ==
		           CampaignEventCaptureResult::CapacityReached &&
		       cyclicCampaignEventCapture ==
		           CampaignEventCaptureResult::AdapterFailure,
		       "application owns campaign events in its runtime and exposes bounded FIFO captures" );

		const auto tacticalCommands =
			compiledContext.serviceCatalog().resolve( TacticalCommandServiceContract );
		RuntimeMessageBus& liveRuntimeMessages = compiledContext.runtimeMessages();
		HeadlessRuntimeMessageSink commandResultSink;
		const RuntimeMessageSinkRegistrationError commandResultSinkRegistered =
			liveRuntimeMessages.addSink( commandResultSink );
		const TacticalCommandInboxLimits productionCommandLimits = tacticalCommands
			? tacticalCommands.service->limits() : TacticalCommandInboxLimits{};
		std::uint64_t commandTestFrameSequence = UINT64_C( 0x8000000000000000 );
		const auto beginCommandTestFrame = [&] {
			BeginSimulationCommandFrameBudget(
				++commandTestFrameSequence,
				productionCommandLimits.maximumPerDrain );
		};
		const TacticalEntityId staleActor{ 0, 0xfedcba98u };
		const SimulationCommand staleStance{ ChangeStanceCommand{
			staleActor, ANIM_CROUCH, SimulationCommandSource::System } };
		const SimulationCommand staleMove{ MoveToGridCommand{
			staleActor, 100, WALKING, true, true,
			SimulationCommandSource::System } };
		const TacticalWorldSession::Snapshot previousCommandWorldSession =
			compiledContext.runtime().tacticalWorldSession().snapshot();
		const SOLDIERTYPE previousCommandActor = soldierRepository.record( 0 );
		SOLDIERTYPE& commandHostActor = soldierRepository.record( 0 );
		const TacticalEntityId previousCommandEntity =
			GetJa2TacticalEntityId( 0 );
		SOLDIERTYPE commandHostActorFixture;
		commandHostActorFixture.ubID = SoldierID{ static_cast<UINT16>( 0 ) };
		commandHostActorFixture.uiUniqueSoldierIdValue = 0x12345678u;
		commandHostActorFixture.bActive = TRUE;
		commandHostActorFixture.bInSector = TRUE;
		NotifyJa2TacticalWorldUnloaded();
		const Ja2TacticalCommandHostDiagnostics commandHostInitially =
			GetJa2TacticalCommandHostDiagnostics();
		const TacticalCommandSubmissionResult teardownPending = tacticalCommands
			? tacticalCommands.service->submit( "fixture.torn-down", staleStance )
			: TacticalCommandSubmissionResult{
				TacticalCommandSubmissionError::InvalidCommand, 0 };
		GetJa2TacticalCommandPackageEventSink().publish( PackageEvent{
			PackageEventKind::Deactivated, "fixture.torn-down" } );
		TacticalCommandInboxSnapshot afterPendingCancellation;
		const TacticalCommandSnapshotError pendingCancellationSnapshot = tacticalCommands
			? tacticalCommands.service->snapshot( afterPendingCancellation )
			: TacticalCommandSnapshotError::AllocationFailure;
		const Ja2TacticalCommandHostDiagnostics commandHostAfterPendingCancellation =
			GetJa2TacticalCommandHostDiagnostics();
		CHECK( tacticalCommands && compiledContext.packages().isActive( packageId ) &&
		       productionCommandLimits.maximumPerDrain > 0 && teardownPending &&
		       pendingCancellationSnapshot == TacticalCommandSnapshotError::None &&
		       afterPendingCancellation.summary.pending == 0 &&
		       commandHostAfterPendingCancellation.lifecycleCancellationEvents ==
		           commandHostInitially.lifecycleCancellationEvents + 1 &&
		       commandHostAfterPendingCancellation.cancelledRequests ==
		           commandHostInitially.cancelledRequests + 1,
		       "production command service exists before active-package bootstrap and cancels pending teardown work" );

		const std::vector<RecordedSimulationCommand> journalBeforeCommandHost =
			compiledContext.commandJournal().snapshot();
		const Ja2TacticalCommandHostDiagnostics commandHostBeforeValidation =
			GetJa2TacticalCommandHostDiagnostics();
		const TacticalCommandSubmissionResult inactiveOwner =
			tacticalCommands.service->submit( "fixture.inactive", staleStance );
		const TacticalCommandSubmissionResult invalidTeam =
			tacticalCommands.service->submit( packageId, SimulationCommand{ EndTurnCommand{
				0xffu, SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidStance =
			tacticalCommands.service->submit( packageId, SimulationCommand{ ChangeStanceCommand{
				staleActor, 0xffu, SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidFire =
			tacticalCommands.service->submit( packageId, SimulationCommand{ BeginFireWeaponCommand{
				staleActor, -1, FIRST_LEVEL, 0, SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidMoveGrid =
			tacticalCommands.service->submit( packageId, SimulationCommand{ MoveToGridCommand{
				staleActor, -1, WALKING, false, false,
				SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidMoveMode =
			tacticalCommands.service->submit( packageId, SimulationCommand{ MoveToGridCommand{
				staleActor, 100, NUMANIMATIONSTATES, false, false,
				SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidScopeGrid =
			tacticalCommands.service->submit(
				packageId, SimulationCommand{ CycleScopeModeCommand{
					staleActor, WORLD_MAX, SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidObjectGrid =
			tacticalCommands.service->submit(
				packageId, SimulationCommand{ ActivateWorldObjectCommand{
					staleActor, TacticalWorldObjectId{ WORLD_MAX, 7 }, 3,
					SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidApproachMode =
			tacticalCommands.service->submit(
				packageId, SimulationCommand{ ApproachWorldObjectCommand{
					staleActor, TacticalWorldObjectId{ 100, 7 }, 3,
					101, NUMANIMATIONSTATES, false, false,
					SimulationCommandSource::System } } );
		const TacticalEntityId outOfRangeTarget{
			static_cast<std::uint16_t>( TOTAL_SOLDIERS ), 1 };
		const TacticalCommandSubmissionResult invalidConversationTarget =
			tacticalCommands.service->submit(
				packageId, SimulationCommand{ StartConversationCommand{
					staleActor, outOfRangeTarget,
					SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidConversationApproach =
			tacticalCommands.service->submit(
				packageId, SimulationCommand{ ApproachConversationCommand{
					staleActor, TacticalEntityId{ 1, 1 }, -1, WALKING, false,
					SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidVehicleTarget =
			tacticalCommands.service->submit(
				packageId, SimulationCommand{ EnterVehicleCommand{
					staleActor, outOfRangeTarget, 3, 0,
					SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidVehicleApproach =
			tacticalCommands.service->submit(
				packageId, SimulationCommand{ ApproachVehicleCommand{
					staleActor, TacticalEntityId{ 1, 1 }, 3, 0, 101,
					NUMANIMATIONSTATES, false,
					SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidWorldItemLevel =
			tacticalCommands.service->submit(
				packageId, SimulationCommand{ PickupWorldItemCommand{
					staleActor, TacticalWorldItemId{ 0, 1 }, 100, -1,
					TacticalWorldItemPickupKind::SpecificItem,
					SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidWorldItemGrid =
			tacticalCommands.service->submit(
				packageId, SimulationCommand{ PickupWorldItemCommand{
					staleActor, TacticalWorldItemId{}, WORLD_MAX, 0,
					TacticalWorldItemPickupKind::SearchGrid,
					SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidStealGrid =
			tacticalCommands.service->submit(
				packageId, SimulationCommand{ StealFromActorCommand{
					staleActor, TacticalEntityId{ 1, 1 }, WORLD_MAX,
					FIRST_LEVEL, SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult invalidExchangeLevel =
			tacticalCommands.service->submit(
				packageId, SimulationCommand{ ExchangePositionsCommand{
					staleActor, TacticalEntityId{ 1, 1 }, 100, 101, 7,
					SimulationCommandSource::System } } );
		const TacticalCommandSubmissionResult unloadedContext =
			tacticalCommands.service->submit( packageId, staleMove );
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		const Ja2TacticalCommandHostDiagnostics commandHostAfterInvalidContext =
			GetJa2TacticalCommandHostDiagnostics();
		NotifyJa2TacticalWorldLoaded(
			previousCommandWorldSession.worldGeneration != 0
				? previousCommandWorldSession.worldGeneration : 1 );
		const bool commandHostActorInstalled =
			soldierRepository.replace( 0, commandHostActorFixture ) ==
				&commandHostActor;
		const bool commandHostActorAdopted =
			commandHostActorInstalled &&
			AdoptJa2TacticalEntity( commandHostActor );
		const TacticalActorSnapshot* commandHostInitialState =
			compiledContext.runtime().tacticalEntityDirectory().state(
				TacticalEntityId{ 0, 0x12345678u } );
		CHECK( commandHostActorInstalled && commandHostActorAdopted &&
		       GetJa2TacticalEntityId( 0 ) == ( TacticalEntityId{ 0, 0x12345678u } ) &&
		       ResolveJa2TacticalEntity( TacticalEntityId{ 0, 0x12345678u } ) ==
		           &commandHostActor &&
		       ResolveJa2TacticalEntity( staleActor ) == nullptr &&
		       commandHostInitialState && commandHostInitialState->active &&
		       commandHostInitialState->inSector,
		       "runtime entity directory rejects stale incarnations and owns the adopted actor projection" );
		const TacticalCommandSubmissionResult staleRequest =
			tacticalCommands.service->submit( packageId, staleMove );
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		const Ja2TacticalCommandHostDiagnostics commandHostAfterValidation =
			GetJa2TacticalCommandHostDiagnostics();
		const std::vector<RecordedSimulationCommand> journalAfterCommandHost =
			compiledContext.commandJournal().snapshot();
		const RecordedSimulationCommand* staleRecord =
			journalAfterCommandHost.size() == journalBeforeCommandHost.size() + 1
				? &journalAfterCommandHost.back() : nullptr;
		CHECK( inactiveOwner && invalidTeam && invalidStance && invalidFire &&
		       invalidMoveGrid && invalidMoveMode && invalidScopeGrid &&
		       invalidObjectGrid && invalidApproachMode &&
		       invalidConversationTarget && invalidConversationApproach &&
		       invalidVehicleTarget && invalidVehicleApproach &&
		       invalidWorldItemLevel && invalidWorldItemGrid &&
		       invalidStealGrid && invalidExchangeLevel &&
		       unloadedContext && staleRequest &&
		       commandHostAfterInvalidContext.lastDrain.accepted == 0 &&
		       commandHostAfterInvalidContext.lastDrain.rejected == 18 &&
		       commandHostAfterValidation.lastDrain.accepted == 1 &&
		       commandHostAfterValidation.lastDrain.rejected == 0 &&
		       commandHostAfterValidation.inactiveOwnerRejections ==
		           commandHostBeforeValidation.inactiveOwnerRejections + 1 &&
		       commandHostAfterValidation.semanticRejections ==
		           commandHostBeforeValidation.semanticRejections + 16 &&
		       commandHostAfterValidation.contextRejections ==
		           commandHostBeforeValidation.contextRejections + 1 &&
		       commandHostAfterValidation.lastProcessing.status ==
		           CommandProcessStatus::Completed &&
		       commandHostAfterValidation.lastProcessing.scheduled == 1 &&
		       commandHostAfterValidation.lastProcessing.discarded == 1 &&
		       staleRecord && staleRecord->status == CommandJournalStatus::Discarded &&
		       std::get<MoveToGridCommand>( staleRecord->command ).soldier == staleActor &&
		       compiledContext.commands().empty(),
		       "safe-frame command host validates movement domains and journals stale move identities as discarded" );

		SOLDIERTYPE detachedCommandActor;
		detachedCommandActor.ubID = commandHostActor.ubID;
		detachedCommandActor.uiUniqueSoldierIdValue =
			commandHostActor.uiUniqueSoldierIdValue;
		detachedCommandActor.bActive = TRUE;
		detachedCommandActor.bInSector = TRUE;
		const std::size_t journalBeforeDetachedActor =
			compiledContext.commandJournal().size();
		beginCommandTestFrame();
		const SimulationCommandDispatchResult detachedActorRejected =
			TryDispatchSetStealthModeCommandNow(
				detachedCommandActor, true, SimulationCommandSource::System );
		const SimulationCommandDispatchResult detachedTargetRejected =
			TryDispatchStartConversationCommandNow(
				commandHostActor, detachedCommandActor,
				SimulationCommandSource::System );
		CHECK(
			detachedActorRejected.status ==
				SimulationCommandDispatchStatus::InvalidActor &&
			!detachedActorRejected.submitted &&
			detachedTargetRejected.status ==
				SimulationCommandDispatchStatus::InvalidActor &&
			!detachedTargetRejected.submitted &&
			compiledContext.commandJournal().size() ==
				journalBeforeDetachedActor &&
			compiledContext.commands().empty(),
			"actor-reference ingress rejects a detached object even when its slot and incarnation fields match a live merc" );

		commandHostActor.movement().mode() = WALKING;
		commandHostActor.movement().setReverse(false);
		commandHostActor.pendingAction().action() = 7;
		const SimulationCommandDispatchResult invalidImmediateMove =
			TryDispatchMoveToGridCommandNow(
				commandHostActor, -1, RUNNING,
				true, true, SimulationCommandSource::System );
		const std::uint64_t invalidImmediateMoveSequence =
			invalidImmediateMove.sequence;
		const std::vector<RecordedSimulationCommand> journalAfterInvalidImmediateMove =
			compiledContext.commandJournal().snapshot();
		const RecordedSimulationCommand* invalidImmediateMoveRecord =
			!journalAfterInvalidImmediateMove.empty()
				? &journalAfterInvalidImmediateMove.back() : nullptr;
		CHECK( invalidImmediateMove.submitted &&
		       invalidImmediateMove.status ==
		           SimulationCommandDispatchStatus::Discarded &&
		       invalidImmediateMove.tick ==
		           compiledContext.runtime().simulationTicks().completedTickSequence() &&
		       invalidImmediateMoveRecord &&
		       invalidImmediateMoveRecord->sequence == invalidImmediateMoveSequence &&
		       invalidImmediateMoveRecord->status == CommandJournalStatus::Discarded &&
		       std::holds_alternative<MoveToGridCommand>(
		           invalidImmediateMoveRecord->command ) &&
		       commandHostActor.movement().mode() == WALKING &&
		       !commandHostActor.movement().reversing() &&
		       commandHostActor.pendingAction().action() == 7,
		       "immediate movement execution rejects invalid destinations before mutating the live actor" );

		beginCommandTestFrame();
		commandHostActor.movement().setStealth(false);
		const SimulationCommandDispatchResult stealthEnabled =
			TryDispatchSetStealthModeCommandNow(
				commandHostActor, true,
				SimulationCommandSource::System );
		beginCommandTestFrame();
		commandHostActor.position().gridNo() = 77;
		commandHostActor.pathing().finalDestinationGrid() = 99;
		commandHostActor.movement().delayCounter() = TRUE;
		commandHostActor.animationPlayback().state() = STANDING;
		const SimulationCommandDispatchResult movementStopped =
			TryDispatchStopMovementCommandNow(
				commandHostActor, SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult facingQueued =
			TryDispatchSetFacingCommandNow(
				commandHostActor, 3,
				SimulationCommandSource::System );
		const TacticalActorSnapshot* commandHostExecutedState =
			compiledContext.runtime().tacticalEntityDirectory().state(
				TacticalEntityId{ 0, 0x12345678u } );
		CHECK( stealthEnabled.status == SimulationCommandDispatchStatus::Applied &&
		       commandHostActor.movement().stealthy() &&
		       movementStopped.status == SimulationCommandDispatchStatus::Applied &&
		       !commandHostActor.movement().delayed() &&
		       commandHostActor.pathing().finalDestinationGrid() == commandHostActor.position().gridNo() &&
		       facingQueued.status == SimulationCommandDispatchStatus::Applied &&
		       commandHostExecutedState &&
		       commandHostExecutedState->grid == commandHostActor.position().gridNo() &&
		       commandHostExecutedState->direction == commandHostActor.position().direction() &&
		       commandHostExecutedState->animation == commandHostActor.animationPlayback().state(),
		       "structured commands execute and commit the resulting public actor state through one path" );

		const UINT32 stanceFlags = CaptureJa2TacticalStatusFlags();
		const UINT8 stanceTeam = GetJa2TacticalCurrentTeam();
		RestoreJa2TacticalTurnState(ACTIVE | REALTIME, stanceTeam);
		commandHostActor.animationPlayback().state() = WALKING;
		commandHostActor.animationIntent().desiredHeight() = ANIM_STAND;
		commandHostActor.movement().clearGridUpdatePolicy();
		const UINT16 expectedMovingStance =
			commandHostActor.GetMoveStateBasedOnStance(ANIM_CROUCH);
		const UINT16 movingStanceSurface =
			DetermineSoldierAnimationSurface(
				&commandHostActor, expectedMovingStance);
		AnimationSurfaceType& movingStanceSurfaceState =
			gAnimSurfaceDatabase[movingStanceSurface];
		const AnimationSurfaceType previousMovingStanceSurfaceState =
			movingStanceSurfaceState;
		ETRLEObject movingStanceFrames[8]{};
		for (ETRLEObject& frame : movingStanceFrames)
		{
			frame.usHeight = 1;
			frame.usWidth = 1;
		}
		SGPVObject movingStanceVideoObject{};
		movingStanceVideoObject.pETRLEObject = movingStanceFrames;
		movingStanceVideoObject.usNumberOfObjects = 8;
		movingStanceSurfaceState.ubFlags = 0;
		movingStanceSurfaceState.uiNumDirections = 8;
		movingStanceSurfaceState.uiNumFramesPerDir = 1;
		movingStanceSurfaceState.hVideoObject = &movingStanceVideoObject;
		movingStanceSurfaceState.bProfile = -1;
		const UINT16 previousAnimationSurface =
			commandHostActor.animationPlayback().surface();
		// The data-free host has no animation assets. Seed one inert
		// eight-direction surface so the production cache and transition remain
		// under test without entering font-backed missing-asset diagnostics.
		commandHostActor.animationCache().reset();
		beginCommandTestFrame();
		const SimulationCommandDispatchResult movingStanceChanged =
			TryDispatchChangeStanceCommandNow(
				commandHostActor, ANIM_CROUCH,
				SimulationCommandSource::System );
		const bool animationCacheHit =
			commandHostActor.animationCache().contains(
				movingStanceSurface ) &&
			commandHostActor.animationCache().acquire(
				commandHostActor.ubID, movingStanceSurface,
				commandHostActor.animationPlayback().state() ) &&
			commandHostActor.animationCache().hitCount(
				movingStanceSurface ) == 1;
		const SOLDIERTYPE copiedCacheOwner = commandHostActor;
		const bool copiedCacheStartsEmpty =
			copiedCacheOwner.animationCache().empty();
		const bool repositoryRetainsSlotCache =
			soldierRepository.replace( 0, commandHostActor ) ==
				&commandHostActor &&
			commandHostActor.animationCache().contains(
				movingStanceSurface );
		const bool movingStanceOwnedByExecutor =
			movingStanceChanged.status ==
				SimulationCommandDispatchStatus::Applied &&
			commandHostActor.movement().mode() == expectedMovingStance &&
			commandHostActor.animationIntent().desiredHeight() == ANIM_CROUCH &&
			commandHostActor.animationPlayback().state() == START_SWAT;
		commandHostActor.animationCache().reset();
		ClearAnimationSurfacesUsageHistory( commandHostActor.ubID );
		commandHostActor.animationPlayback().surface() = previousAnimationSurface;
		movingStanceSurfaceState = previousMovingStanceSurfaceState;
		RestoreJa2TacticalTurnState(stanceFlags, stanceTeam);
		commandHostActor.animationPlayback().state() = STANDING;
		commandHostActor.animationIntent().clearDesiredHeight();
		commandHostActor.movement().clearGridUpdatePolicy();
		CHECK( animationCacheHit,
		       "inline soldier animation cache acquires surfaces and records bounded hits" );
		CHECK( copiedCacheStartsEmpty,
		       "copied soldiers do not alias or inherit runtime animation surface ownership" );
		CHECK( repositoryRetainsSlotCache,
		       "whole-record replacement retains animation surface ownership with its canonical slot" );
		CHECK( movingStanceOwnedByExecutor,
		       "stance command executor owns real-time moving animation transitions" );

		beginCommandTestFrame();
		const SimulationCommandDispatchResult cancelWithoutDrag =
			TryDispatchCancelDragCommandNow(
				commandHostActor, SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult weaponModeWithoutWeapon =
			TryDispatchCycleWeaponModeCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue,
				SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult scopeModeWithoutWeapon =
			TryDispatchCycleScopeModeCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue,
				TacticalNoTargetGrid, SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult reloadWithoutWeapon =
			TryDispatchReloadWeaponCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue,
				false, SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult readyWithoutWeapon =
			TryDispatchSetWeaponReadyCommandNow(
				commandHostActor, 3, true, false,
				SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult staleTraversal =
			TryDispatchTraverseObstacleCommandNow(
				staleActor.slot, staleActor.incarnation,
				TacticalTraversalKind::JumpFence,
				SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult staleActivation =
			TryDispatchActivateWorldObjectCommandNow(
				staleActor.slot, staleActor.incarnation,
				100, 7, 3, SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult staleApproach =
			TryDispatchApproachWorldObjectCommandNow(
				staleActor.slot, staleActor.incarnation,
				100, 7, 3, 101, WALKING, false, false,
				SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult staleConversationTarget =
			TryDispatchStartConversationCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue,
				staleActor.slot, staleActor.incarnation,
				SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult staleConversationApproachTarget =
			TryDispatchApproachConversationCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue,
				staleActor.slot, staleActor.incarnation,
				101, WALKING, false, SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult staleVehicleTarget =
			TryDispatchEnterVehicleCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue,
				staleActor.slot, staleActor.incarnation,
				3, 0, SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult staleVehicleApproachTarget =
			TryDispatchApproachVehicleCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue,
				staleActor.slot, staleActor.incarnation,
				3, 0, 101, WALKING, false,
				SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult staleStealTarget =
			TryDispatchStealFromActorCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue,
				staleActor.slot, staleActor.incarnation,
				100, FIRST_LEVEL, SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult staleExchangeTarget =
			TryDispatchExchangePositionsCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue,
				staleActor.slot, staleActor.incarnation,
				99, 100, FIRST_LEVEL,
				SimulationCommandSource::System );
		commandHostActor.pendingAction().action() = MERC_TALK;
		commandHostActor.pendingAction().primaryData() = staleActor.slot;
		commandHostActor.pendingAction().quaternaryData() = 0;
		commandHostActor.runtime.pendingAction.targetIncarnation =
			staleActor.incarnation;
		const bool stalePendingConversationCompleted =
			TryCompletePendingConversationCommand( commandHostActor );
		const bool stalePendingConversationCleared =
			commandHostActor.pendingAction().action() == NO_PENDING_ACTION;
		commandHostActor.pendingAction().action() = MERC_ENTER_VEHICLE;
		commandHostActor.pendingAction().primaryData() = 0;
		commandHostActor.pendingAction().secondaryData() = staleActor.slot;
		commandHostActor.pendingAction().tertiaryData() = 3;
		commandHostActor.pendingAction().quaternaryData() = 0;
		commandHostActor.runtime.pendingAction.targetIncarnation =
			staleActor.incarnation;
		const bool stalePendingVehicleCompleted =
			TryCompletePendingVehicleCommand( commandHostActor );
		const bool stalePendingVehicleCleared =
			commandHostActor.pendingAction().action() == NO_PENDING_ACTION;
		commandHostActor.pendingAction().action() = MERC_STEAL;
		commandHostActor.pendingAction().primaryData() = staleActor.slot;
		commandHostActor.pendingAction().secondaryData() = 100;
		commandHostActor.pendingAction().tertiaryData() = 3;
		commandHostActor.pendingAction().quaternaryData() = 0;
		commandHostActor.targeting().level() = FIRST_LEVEL;
		commandHostActor.runtime.pendingAction.targetIncarnation =
			staleActor.incarnation;
		const bool stalePendingStealCompleted =
			TryCompletePendingStealCommand( commandHostActor );
		const bool stalePendingStealCleared =
			commandHostActor.pendingAction().action() == NO_PENDING_ACTION &&
			commandHostActor.runtime.pendingAction.targetIncarnation == 0;

		std::vector<WORLDITEM> previousWorldItems = std::move( gWorldItems );
		const UINT32 previousWorldItemCount = guiNumWorldItems;
		gWorldItems.clear();
		gWorldItems.resize( 1 );
		guiNumWorldItems = 1;
		ResetJa2TacticalWorldItemDirectory();
		gWorldItems[0].fExists = TRUE;
		gWorldItems[0].sGridNo = 123;
		gWorldItems[0].ubLevel = 0;
		gWorldItems[0].bRenderZHeightAboveLevel = 5;
		const bool firstWorldItemAssigned =
			AssignJa2TacticalWorldItemIdentity( 0 );
		const TacticalWorldItemId firstWorldItem =
			GetJa2TacticalWorldItemId( 0 );
		const std::size_t firstWorldItemActiveCount =
			GetJa2TacticalWorldItemDirectory().activeCount();
		Ja2TacticalWorldItemReference liveWorldItemReference;
		const bool liveWorldItemReferenceCaptured =
			liveWorldItemReference.capture( 0 ) &&
			liveWorldItemReference.identity() == firstWorldItem &&
			liveWorldItemReference.resolve() == &gWorldItems[0];
		WORLDITEM* consumedWorldItemReference =
			liveWorldItemReference.consume();
		const bool consumedWorldItemReferenceMatched =
			consumedWorldItemReference == &gWorldItems[0];
		Ja2TacticalWorldItemReference removedWorldItemReference;
		const bool removedWorldItemReferenceCaptured =
			removedWorldItemReference.capture( 0 );
		commandHostActor.position().level() = 0;
		commandHostActor.pendingAction().action() = MERC_PICKUPITEM;
		commandHostActor.pendingAction().primaryData() =
			firstWorldItem.slot;
		commandHostActor.pendingAction().secondaryData() = 123;
		commandHostActor.pendingAction().tertiaryData() = 5;
		commandHostActor.pendingAction().quaternaryData() = 123;
		commandHostActor.runtime.pendingAction.targetIncarnation =
			firstWorldItem.incarnation;
		const bool livePendingWorldItemAccepted =
			TryValidatePendingWorldItemPickup( commandHostActor ) &&
			TryConsumePendingWorldItemPickup(
				commandHostActor, 0, 123, 5 ) &&
			commandHostActor.runtime.pendingAction.targetIncarnation == 0;
		commandHostActor.pendingAction().action() = NO_PENDING_ACTION;
		WORLDITEM copiedWorldItem = gWorldItems[0];
		RemoveItemFromWorld( -1 );
		RemoveItemFromWorld( 1 );
		const bool invalidWorldItemRemovalPreservedLiveItem =
			ResolveJa2TacticalWorldItem( firstWorldItem ) ==
				&gWorldItems[0] &&
			GetJa2TacticalWorldItemDirectory().activeCount() ==
				firstWorldItemActiveCount;
		RemoveItemFromWorld( 0 );
		const bool firstWorldItemRetired =
			ResolveJa2TacticalWorldItem( firstWorldItem ) == nullptr &&
			gWorldItems[0].uiUniqueWorldItemIdValue == 0;
		const bool removedWorldItemReferenceRejected =
			removedWorldItemReference.resolve() == nullptr;
		gWorldItems[0].fExists = TRUE;
		gWorldItems[0].sGridNo = 123;
		gWorldItems[0].ubLevel = 0;
		const bool replacementWorldItemAssigned =
			AssignJa2TacticalWorldItemIdentity( 0 );
		const TacticalWorldItemId replacementWorldItem =
			GetJa2TacticalWorldItemId( 0 );
		const bool replacementWorldItemResolved =
			ResolveJa2TacticalWorldItem( replacementWorldItem ) ==
				&gWorldItems[0];
		const bool removedWorldItemReferenceRejectedReplacement =
			removedWorldItemReference.resolve() == nullptr;
		gWorldItems.resize( 2 );
		guiNumWorldItems = 2;
		gWorldItems[1].fExists = TRUE;
		gWorldItems[1].sGridNo = 124;
		gWorldItems[1].ubLevel = 0;
		const bool movableWorldItemAssigned =
			AssignJa2TacticalWorldItemIdentity( 1 );
		const TacticalWorldItemId movableWorldItem =
			GetJa2TacticalWorldItemId( 1 );
		std::vector<WORLDITEM> compactedWorldItems{
			gWorldItems[1] };
		gWorldItems = std::move( compactedWorldItems );
		guiNumWorldItems = 1;
		RebuildJa2TacticalWorldItemDirectory();
		const TacticalWorldItemId movedWorldItem =
			GetJa2TacticalWorldItemId( 0 );
		const bool worldItemReplacementRebuilt =
			movableWorldItemAssigned && movableWorldItem.valid() &&
			movedWorldItem ==
				( TacticalWorldItemId{
					0, movableWorldItem.incarnation } ) &&
			ResolveJa2TacticalWorldItem( movableWorldItem ) == nullptr &&
			ResolveJa2TacticalWorldItem( replacementWorldItem ) == nullptr &&
			ResolveJa2TacticalWorldItem( movedWorldItem ) ==
				&gWorldItems[0];
		const TacticalWorldItemId forgedWorldItemMirror{
			0, movedWorldItem.incarnation == 1 ? 2u : 1u };
		gWorldItems[0].uiUniqueWorldItemIdValue =
			forgedWorldItemMirror.incarnation;
		const TacticalWorldItemId repairedWorldItem =
			GetJa2TacticalWorldItemId( 0 );
		const bool splitWorldItemIdentityFailedClosed =
			repairedWorldItem.valid() &&
			repairedWorldItem != movedWorldItem &&
			repairedWorldItem != forgedWorldItemMirror &&
			ResolveJa2TacticalWorldItem( movedWorldItem ) == nullptr &&
			ResolveJa2TacticalWorldItem( forgedWorldItemMirror ) == nullptr &&
			ResolveJa2TacticalWorldItem( repairedWorldItem ) ==
				&gWorldItems[0];
		beginCommandTestFrame();
		const SimulationCommandDispatchResult staleWorldItemPickup =
			TryDispatchPickupWorldItemCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue,
				firstWorldItem, 123, 0,
				TacticalWorldItemPickupKind::SpecificItem,
				SimulationCommandSource::System );
		commandHostActor.pendingAction().action() = MERC_PICKUPITEM;
		commandHostActor.pendingAction().primaryData() =
			firstWorldItem.slot;
		commandHostActor.pendingAction().secondaryData() = 123;
		commandHostActor.pendingAction().tertiaryData() = 0;
		commandHostActor.pendingAction().quaternaryData() = 123;
		commandHostActor.runtime.pendingAction.targetIncarnation =
			firstWorldItem.incarnation;
		const bool stalePendingWorldItemRejected =
			!TryValidatePendingWorldItemPickup( commandHostActor );
		const bool stalePendingWorldItemCleared =
			commandHostActor.pendingAction().action() == NO_PENDING_ACTION &&
			commandHostActor.runtime.pendingAction.targetIncarnation == 0;
		const bool worldItemIdentityLifecycle =
			firstWorldItemAssigned && firstWorldItem.valid() &&
			liveWorldItemReferenceCaptured &&
			consumedWorldItemReferenceMatched &&
			!liveWorldItemReference.valid() &&
			removedWorldItemReferenceCaptured &&
			removedWorldItemReferenceRejected &&
			removedWorldItemReferenceRejectedReplacement &&
			copiedWorldItem.uiUniqueWorldItemIdValue ==
				firstWorldItem.incarnation &&
			livePendingWorldItemAccepted &&
			invalidWorldItemRemovalPreservedLiveItem &&
			firstWorldItemRetired &&
			replacementWorldItemAssigned &&
			replacementWorldItem.valid() &&
			replacementWorldItem != firstWorldItem &&
			replacementWorldItemResolved &&
			worldItemReplacementRebuilt &&
			splitWorldItemIdentityFailedClosed &&
			ResolveJa2TacticalWorldItem( firstWorldItem ) == nullptr;
		ResetJa2TacticalWorldItemDirectory();
		gWorldItems = std::move( previousWorldItems );
		guiNumWorldItems = previousWorldItemCount;
		RebuildJa2TacticalWorldItemDirectory();
		CHECK(
			cancelWithoutDrag.status ==
				SimulationCommandDispatchStatus::Discarded &&
			weaponModeWithoutWeapon.status ==
				SimulationCommandDispatchStatus::Discarded &&
			scopeModeWithoutWeapon.status ==
				SimulationCommandDispatchStatus::Discarded &&
			reloadWithoutWeapon.status ==
				SimulationCommandDispatchStatus::Discarded &&
			readyWithoutWeapon.status ==
				SimulationCommandDispatchStatus::Discarded &&
			staleTraversal.status ==
				SimulationCommandDispatchStatus::Discarded &&
			staleActivation.status ==
				SimulationCommandDispatchStatus::Discarded &&
			staleApproach.status ==
				SimulationCommandDispatchStatus::Discarded &&
			staleConversationTarget.status ==
				SimulationCommandDispatchStatus::Discarded &&
			staleConversationApproachTarget.status ==
				SimulationCommandDispatchStatus::Discarded &&
			staleVehicleTarget.status ==
				SimulationCommandDispatchStatus::Discarded &&
			staleVehicleApproachTarget.status ==
				SimulationCommandDispatchStatus::Discarded &&
			staleStealTarget.status ==
				SimulationCommandDispatchStatus::Discarded &&
			staleExchangeTarget.status ==
				SimulationCommandDispatchStatus::Discarded &&
			!stalePendingConversationCompleted &&
			stalePendingConversationCleared &&
			!stalePendingVehicleCompleted &&
			stalePendingVehicleCleared &&
			!stalePendingStealCompleted &&
			stalePendingStealCleared &&
			worldItemIdentityLifecycle &&
			staleWorldItemPickup.status ==
				SimulationCommandDispatchStatus::Discarded &&
			stalePendingWorldItemRejected &&
			stalePendingWorldItemCleared,
			"equipment and interaction commands reject stale actor, world-item, and delayed target incarnations" );

		const std::uint64_t oneCommandFrame = ++commandTestFrameSequence;
		BeginSimulationCommandFrameBudget( oneCommandFrame, 1 );
		const SimulationCommandDispatchResult firstBudgetedImmediate =
			TryDispatchMoveToGridCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue, -1, WALKING,
				false, false, SimulationCommandSource::System );
		BeginSimulationCommandFrameBudget( oneCommandFrame, 1 );
		const SimulationCommandDispatchResult sameFrameBudgetExhausted =
			TryDispatchMoveToGridCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue, -1, WALKING,
				false, false, SimulationCommandSource::System );
		BeginSimulationCommandFrameBudget( ++commandTestFrameSequence, 1 );
		const SimulationCommandDispatchResult nextFrameBudgetReset =
			TryDispatchMoveToGridCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue, -1, WALKING,
				false, false, SimulationCommandSource::System );
		CHECK( firstBudgetedImmediate.processed() &&
		       sameFrameBudgetExhausted.status ==
		           SimulationCommandDispatchStatus::FrameBudgetExhausted &&
		       !sameFrameBudgetExhausted.submitted &&
		       nextFrameBudgetReset.processed(),
		       "command frame budget resets once per frame identity and not on duplicate begin" );

		BeginSimulationCommandFrameBudget( ++commandTestFrameSequence, 1 );
		const SimulationCommandDispatchResult inboxBudgetConsumer =
			TryDispatchMoveToGridCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue, -1, WALKING,
				false, false, SimulationCommandSource::System );
		const TacticalCommandSubmissionResult heldByExhaustedFrame =
			tacticalCommands.service->submit( packageId, staleStance );
		const Ja2TacticalCommandHostDiagnostics beforeExhaustedFrameDrain =
			GetJa2TacticalCommandHostDiagnostics();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		TacticalCommandInboxSnapshot exhaustedFrameInbox;
		tacticalCommands.service->snapshot( exhaustedFrameInbox );
		const Ja2TacticalCommandHostDiagnostics afterExhaustedFrameDrain =
			GetJa2TacticalCommandHostDiagnostics();
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		TacticalCommandInboxSnapshot resumedFrameInbox;
		tacticalCommands.service->snapshot( resumedFrameInbox );
		CHECK( inboxBudgetConsumer.processed() && heldByExhaustedFrame &&
		       exhaustedFrameInbox.summary.pending == 1 &&
		       afterExhaustedFrameDrain.lastDrain.eligible == 0 &&
		       afterExhaustedFrameDrain.trackedCommands ==
		           beforeExhaustedFrameDrain.trackedCommands &&
		       resumedFrameInbox.summary.pending == 0,
		       "an exhausted production frame leaves package inbox admission for the next frame" );

		const TacticalCommandSubmissionResult immediateMoveDrainedTracked =
			tacticalCommands.service->submit( packageId, staleMove );
		const Ja2TacticalCommandHostDiagnostics beforeImmediateMoveDrain =
			GetJa2TacticalCommandHostDiagnostics();
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext, 0 );
		const Ja2TacticalCommandHostDiagnostics retainedBeforeImmediateMove =
			GetJa2TacticalCommandHostDiagnostics();
		const SimulationCommandDispatchResult backpressuredImmediateMove =
			TryDispatchMoveToGridCommandNow(
			0, commandHostActor.uiUniqueSoldierIdValue, -1, RUNNING,
			false, false, SimulationCommandSource::System );
		const SimulationCommandDispatchResult retainedNetworkPacket =
			TryDispatchNetworkSimulationCommand(
				SimulationCommand{ ChangeStanceCommand{
					staleActor, ANIM_STAND,
					SimulationCommandSource::NetworkPeer,
					TacticalEventPolicy::LocalOnly } } );
		const SimulationCommandDispatchResult retainedSystemAction =
			TryDispatchSystemSimulationCommand(
				SimulationCommand{ BeginSelectedFireWeaponCommand{
					staleActor, 100, FIRST_LEVEL, 0, HANDPOS, 1,
					SimulationCommandSource::System } } );
		const Ja2TacticalCommandHostDiagnostics afterImmediateMoveDrain =
			GetJa2TacticalCommandHostDiagnostics();
		CHECK( immediateMoveDrainedTracked &&
		       retainedBeforeImmediateMove.trackedCommands ==
		           beforeImmediateMoveDrain.trackedCommands + 1 &&
		       retainedBeforeImmediateMove.authoritativeBackpressure &&
		       afterImmediateMoveDrain.trackedCommands ==
		           retainedBeforeImmediateMove.trackedCommands &&
		       backpressuredImmediateMove.status ==
		           SimulationCommandDispatchStatus::AuthoritativeBackpressure &&
		       !backpressuredImmediateMove.submitted &&
		       retainedNetworkPacket.status ==
		           SimulationCommandDispatchStatus::RetryDeferred &&
		       retainedNetworkPacket.submitted &&
		       retainedSystemAction.status ==
		           SimulationCommandDispatchStatus::RetryDeferred &&
		       retainedSystemAction.submitted &&
		       retainedSystemAction.sequence ==
		           retainedNetworkPacket.sequence + 1 &&
		       afterImmediateMoveDrain.receiptsQueued ==
		           retainedBeforeImmediateMove.receiptsQueued,
		       "local dispatch preserves earlier work while reliable network and System ingress queue behind it" );
		// Consume the retained host backpressure frame and publish the receipt
		// before starting the independent bounded-admission fixture below.
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		const std::vector<RecordedSimulationCommand>
			journalAfterRetainedNetworkPacket =
				compiledContext.commandJournal().snapshot();
		const RecordedSimulationCommand* retainedNetworkRecord =
			journalAfterRetainedNetworkPacket.size() >= 2
				? &journalAfterRetainedNetworkPacket[
					journalAfterRetainedNetworkPacket.size() - 2]
				: nullptr;
		const RecordedSimulationCommand* retainedSystemRecord =
			!journalAfterRetainedNetworkPacket.empty()
				? &journalAfterRetainedNetworkPacket.back() : nullptr;
		CHECK(
			retainedNetworkRecord &&
			retainedNetworkRecord->sequence ==
				retainedNetworkPacket.sequence &&
			retainedNetworkRecord->status ==
				CommandJournalStatus::Discarded &&
			std::holds_alternative<ChangeStanceCommand>(
				retainedNetworkRecord->command ) &&
			std::get<ChangeStanceCommand>(
				retainedNetworkRecord->command ).source ==
				SimulationCommandSource::NetworkPeer &&
			retainedSystemRecord &&
			retainedSystemRecord->sequence ==
				retainedSystemAction.sequence &&
			retainedSystemRecord->status ==
				CommandJournalStatus::Discarded &&
			std::holds_alternative<BeginSelectedFireWeaponCommand>(
				retainedSystemRecord->command ) &&
			std::get<BeginSelectedFireWeaponCommand>(
				retainedSystemRecord->command ).attackingHand ==
				HANDPOS &&
			std::get<BeginSelectedFireWeaponCommand>(
				retainedSystemRecord->command ).attackingWeapon == 1 &&
			std::get<BeginSelectedFireWeaponCommand>(
				retainedSystemRecord->command ).source ==
				SimulationCommandSource::System &&
			compiledContext.commands().empty(),
			"reliable network and System ingress retain captured state and ordering through the safe-frame drain" );

		std::vector<std::uint64_t> boundedRequestIds;
		boundedRequestIds.reserve( productionCommandLimits.maximumPerDrain + 1 );
		bool boundedRequestsSubmitted = true;
		for ( std::size_t index = 0;
		      index < productionCommandLimits.maximumPerDrain + 1; ++index )
		{
			const TacticalCommandSubmissionResult submitted =
				tacticalCommands.service->submit( packageId, staleStance );
			boundedRequestsSubmitted = boundedRequestsSubmitted && static_cast<bool>( submitted );
			boundedRequestIds.push_back( submitted.requestId );
		}
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		TacticalCommandInboxSnapshot boundedRemainder;
		tacticalCommands.service->snapshot( boundedRemainder );
		const Ja2TacticalCommandHostDiagnostics boundedFirstFrame =
			GetJa2TacticalCommandHostDiagnostics();
		const bool retainedFifoTail = boundedRemainder.pending.size() == 1 &&
			boundedRemainder.pending[0].requestId == boundedRequestIds.back();
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		TacticalCommandInboxSnapshot boundedFinished;
		tacticalCommands.service->snapshot( boundedFinished );
		CHECK( boundedRequestsSubmitted && retainedFifoTail &&
		       boundedFirstFrame.lastDrain.eligible ==
		           productionCommandLimits.maximumPerDrain &&
		       boundedFirstFrame.lastDrain.accepted ==
		           productionCommandLimits.maximumPerDrain &&
		       boundedFirstFrame.lastProcessing.scheduled ==
		           productionCommandLimits.maximumPerDrain &&
		       boundedFirstFrame.lastProcessing.discarded ==
		           productionCommandLimits.maximumPerDrain &&
		       boundedFinished.summary.pending == 0 && compiledContext.commands().empty(),
		       "production admission moves only one configured FIFO prefix per safe frame" );

		const std::uint64_t commandTick =
			compiledContext.runtime().simulationTicks().completedTickSequence();
		const std::uint64_t futureCommandTick = commandTick + 1;
		const std::uint64_t futureCommandSequence =
			compiledContext.submitCommand( futureCommandTick, staleStance );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult currentAheadOfFuture =
			TryDispatchMoveToGridCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue, -1, RUNNING,
				false, false, SimulationCommandSource::System );
		const bool futureRetainedAfterCurrent =
			compiledContext.commands().containsSequence( futureCommandSequence );
		const TacticalCommandSubmissionResult heldBehindFutureReplay =
			tacticalCommands.service->submit( packageId, staleStance );
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		TacticalCommandInboxSnapshot futureReplayGatedSnapshot;
		tacticalCommands.service->snapshot( futureReplayGatedSnapshot );
		const Ja2TacticalCommandHostDiagnostics futureReplayGatedFrame =
			GetJa2TacticalCommandHostDiagnostics();
		RecordingSimulationCommandExecutionSink futureReplaySink;
		const CommandProcessingResult futureReplayProcessed =
			ExecuteSimulationCommandsThrough(
				futureCommandTick, 1, futureReplaySink );
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		TacticalCommandInboxSnapshot futureReplayResumedSnapshot;
		tacticalCommands.service->snapshot( futureReplayResumedSnapshot );
		const std::vector<RecordedSimulationCommand> journalAfterFutureReplay =
			compiledContext.commandJournal().snapshot();
		const bool futureReplayWasDiscarded = std::any_of(
			journalAfterFutureReplay.begin(), journalAfterFutureReplay.end(),
			[futureCommandSequence]( const RecordedSimulationCommand& record ) {
				return record.sequence == futureCommandSequence &&
					record.status == CommandJournalStatus::Discarded;
			} );
		CHECK( currentAheadOfFuture.submitted &&
		       currentAheadOfFuture.status ==
		           SimulationCommandDispatchStatus::Discarded &&
		       futureRetainedAfterCurrent &&
		       heldBehindFutureReplay &&
		       futureReplayGatedSnapshot.summary.pending == 1 &&
		       futureReplayGatedFrame.lastProcessing.status ==
		           CommandProcessStatus::Completed &&
		       futureReplayGatedFrame.lastProcessing.scheduled == 0 &&
		       futureReplayWasDiscarded &&
		       futureReplayProcessed.discarded == 1 &&
		       futureReplaySink.observed &&
		       futureReplaySink.lastTick == futureCommandTick &&
		       futureReplaySink.lastSequence == futureCommandSequence &&
		       futureReplaySink.lastDisposition == CommandDisposition::Discard &&
		       futureReplayResumedSnapshot.summary.pending == 0 &&
		       compiledContext.commands().empty(),
		       "future replay backlog pauses live package ingress until the authoritative stream clears" );

		for ( std::size_t index = 0;
		      index < productionCommandLimits.maximumPerDrain + 1; ++index )
			compiledContext.submitCommand( commandTick, staleStance );
		const TacticalCommandSubmissionResult gatedRequest =
			tacticalCommands.service->submit( packageId, staleStance );
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		TacticalCommandInboxSnapshot gatedSnapshot;
		tacticalCommands.service->snapshot( gatedSnapshot );
		const Ja2TacticalCommandHostDiagnostics gatedFrame =
			GetJa2TacticalCommandHostDiagnostics();
		const TacticalCommandSubmissionResult heldDuringRetry =
			tacticalCommands.service->submit( packageId, staleStance );
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		TacticalCommandInboxSnapshot retrySnapshot;
		tacticalCommands.service->snapshot( retrySnapshot );
		const Ja2TacticalCommandHostDiagnostics retryFrame =
			GetJa2TacticalCommandHostDiagnostics();
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		TacticalCommandInboxSnapshot resumedSnapshot;
		tacticalCommands.service->snapshot( resumedSnapshot );
		CHECK( gatedRequest && heldDuringRetry && gatedFrame.authoritativeBackpressure &&
		       gatedFrame.lastProcessing.status == CommandProcessStatus::BudgetExhausted &&
		       gatedFrame.lastProcessing.scheduled ==
		           productionCommandLimits.maximumPerDrain &&
		       gatedSnapshot.summary.pending == 1 &&
		       retryFrame.lastProcessing.status == CommandProcessStatus::Completed &&
		       !retryFrame.authoritativeBackpressure && retrySnapshot.summary.pending == 2 &&
		       resumedSnapshot.summary.pending == 0 && compiledContext.commands().empty(),
		       "ready authoritative backlog gates admission and consumes bounded retry frames" );

		const std::vector<RecordedSimulationCommand> journalBeforeRetained =
			compiledContext.commandJournal().snapshot();
		const TacticalCommandSubmissionResult retainedForTeardown =
			tacticalCommands.service->submit( packageId, staleStance );
		const Ja2TacticalCommandHostDiagnostics beforeRetainedCancellation =
			GetJa2TacticalCommandHostDiagnostics();
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext, 0 );
		const std::vector<RecordedSimulationCommand> beforeAuthoritativeCancellation =
			compiledContext.commandJournal().snapshot();
		GetJa2TacticalCommandPackageEventSink().publish( PackageEvent{
			PackageEventKind::ShutdownFailed, packageId } );
		const Ja2TacticalCommandHostDiagnostics afterRetainedCancellation =
			GetJa2TacticalCommandHostDiagnostics();
		const std::vector<RecordedSimulationCommand> afterAuthoritativeCancellation =
			compiledContext.commandJournal().snapshot();
		const RecordedSimulationCommand* cancelledRecord =
			!afterAuthoritativeCancellation.empty()
				? &afterAuthoritativeCancellation.back() : nullptr;
		CHECK( retainedForTeardown &&
		       beforeAuthoritativeCancellation.size() ==
		           journalBeforeRetained.size() + 1 &&
		       afterRetainedCancellation.cancelledAuthoritativeCommands ==
		           beforeRetainedCancellation.cancelledAuthoritativeCommands + 1 &&
		       cancelledRecord && cancelledRecord->status == CommandJournalStatus::Discarded &&
		       compiledContext.commands().empty(),
		       "lifecycle teardown cancels a bounded accepted command retained by execution backpressure" );
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		const Ja2TacticalCommandHostDiagnostics commandReceiptDiagnostics =
			GetJa2TacticalCommandHostDiagnostics();
		const RuntimeMessageDispatchResult commandReceiptDispatch =
			liveRuntimeMessages.dispatchPending();
		std::vector<TacticalCommandResult> commandResults;
		bool commandResultsDecoded = true;
		for ( const RuntimeMessage& message : commandResultSink.messages )
		{
			if ( message.topic != TacticalCommandResultMessageTopic ) continue;
			TacticalCommandResult result;
			if ( DecodeTacticalCommandResult( message.payload, result ) !=
			     TacticalCommandResultDecodeError::None )
			{
				commandResultsDecoded = false;
				continue;
			}
			commandResults.push_back( std::move( result ) );
		}
		const auto findCommandResult = [&]( std::uint64_t requestId ) {
			return std::find_if(
				commandResults.begin(), commandResults.end(),
				[requestId]( const TacticalCommandResult& result ) {
					return result.requestId == requestId;
				} );
		};
		const auto pendingCancellationResult = findCommandResult( teardownPending.requestId );
		const auto inactiveOwnerResult = findCommandResult( inactiveOwner.requestId );
		const auto unavailableContextResult = findCommandResult( unloadedContext.requestId );
		const auto staleCommandResult = findCommandResult( staleRequest.requestId );
		const auto immediateMoveDrainedResult =
			findCommandResult( immediateMoveDrainedTracked.requestId );
		const auto retainedCancellationResult =
			findCommandResult( retainedForTeardown.requestId );
		CHECK( commandResultSinkRegistered == RuntimeMessageSinkRegistrationError::None &&
		       commandReceiptDispatch.messages == commandResultSink.messages.size() &&
		       commandResultsDecoded &&
		       commandReceiptDiagnostics.pendingReceipts == 0 &&
		       commandReceiptDiagnostics.receiptsPublished == commandResults.size() &&
		       commandReceiptDiagnostics.receiptDrops == 0 &&
		       pendingCancellationResult != commandResults.end() &&
		       pendingCancellationResult->packageId == "fixture.torn-down" &&
		       pendingCancellationResult->status == TacticalCommandTerminalStatus::Cancelled &&
		       pendingCancellationResult->reason ==
		           TacticalCommandTerminalReason::PackageTeardown &&
		       inactiveOwnerResult != commandResults.end() &&
		       inactiveOwnerResult->status == TacticalCommandTerminalStatus::Rejected &&
		       inactiveOwnerResult->reason == TacticalCommandTerminalReason::InactiveOwner &&
		       unavailableContextResult != commandResults.end() &&
		       unavailableContextResult->reason ==
		           TacticalCommandTerminalReason::UnavailableContext &&
		       staleCommandResult != commandResults.end() && staleRecord &&
		       staleCommandResult->authoritativeSequence == staleRecord->sequence &&
		       staleCommandResult->status == TacticalCommandTerminalStatus::Discarded &&
		       staleCommandResult->reason ==
		           TacticalCommandTerminalReason::AuthoritativeDiscard &&
		       immediateMoveDrainedResult != commandResults.end() &&
		       immediateMoveDrainedResult->status ==
		           TacticalCommandTerminalStatus::Discarded &&
		       immediateMoveDrainedResult->reason ==
		           TacticalCommandTerminalReason::AuthoritativeDiscard &&
		       retainedCancellationResult != commandResults.end() &&
		       retainedCancellationResult->status ==
		           TacticalCommandTerminalStatus::Cancelled &&
		       retainedCancellationResult->reason ==
		           TacticalCommandTerminalReason::PackageTeardown,
		       "production command receipts report rejection, authoritative discard, and teardown cancellation" );
		CHECK( liveRuntimeMessages.removeSink( commandResultSink ) ==
		           RuntimeMessageSinkRegistrationError::None,
		       "production command receipt test removes its non-owning runtime sink" );

		const Ja2TacticalCommandHostDiagnostics beforeReceiptReserve =
			GetJa2TacticalCommandHostDiagnostics();
		bool receiptBusFilled = liveRuntimeMessages.queued() == 0;
		for ( std::size_t index = 0;
		      index < liveRuntimeMessages.maxQueuedMessages(); ++index )
		{
			receiptBusFilled = receiptBusFilled && static_cast<bool>(
				liveRuntimeMessages.publish( RuntimeMessageRequest{
					"test.receipt-fill", "test.headless", {} } ) );
		}
		const std::size_t admittedReceiptObligations =
			productionCommandLimits.maximumPending +
			productionCommandLimits.maximumPerDrain;
		bool receiptObligationsSubmitted =
			productionCommandLimits.maximumPerDrain != 0;
		std::size_t admittedReceiptCount = 0;
		while ( receiptObligationsSubmitted &&
		        admittedReceiptCount < admittedReceiptObligations )
		{
			const std::size_t batch = std::min(
				productionCommandLimits.maximumPerDrain,
				admittedReceiptObligations - admittedReceiptCount );
			for ( std::size_t index = 0; index < batch; ++index )
				receiptObligationsSubmitted = receiptObligationsSubmitted &&
					static_cast<bool>( tacticalCommands.service->submit(
						packageId, staleStance ) );
			beginCommandTestFrame();
			DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
			admittedReceiptCount += batch;
		}
		const TacticalCommandSubmissionResult receiptCapacityFront =
			tacticalCommands.service->submit( packageId, staleStance );
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		const Ja2TacticalCommandHostDiagnostics atReceiptAdmissionLimit =
			GetJa2TacticalCommandHostDiagnostics();
		TacticalCommandInboxSummary receiptReserveInbox =
			tacticalCommands.service->summary();
		bool receiptReserveInboxFilled = receiptReserveInbox.pending == 1;
		while ( receiptReserveInboxFilled &&
		        receiptReserveInbox.pending < productionCommandLimits.maximumPending )
		{
			receiptReserveInboxFilled = static_cast<bool>(
				tacticalCommands.service->submit( packageId, staleStance ) );
			receiptReserveInbox = tacticalCommands.service->summary();
		}
		const TacticalCommandSubmissionResult receiptReserveOverflow =
			tacticalCommands.service->submit( packageId, staleStance );
		const Ja2TacticalCommandHostDiagnostics beforeReceiptReserveCancellation =
			GetJa2TacticalCommandHostDiagnostics();
		GetJa2TacticalCommandPackageEventSink().publish( PackageEvent{
			PackageEventKind::Deactivated, packageId } );
		const Ja2TacticalCommandHostDiagnostics afterReceiptReserveCancellation =
			GetJa2TacticalCommandHostDiagnostics();
		CHECK( receiptBusFilled &&
		       liveRuntimeMessages.queued() == liveRuntimeMessages.maxQueuedMessages() &&
		       receiptObligationsSubmitted && receiptCapacityFront &&
		       atReceiptAdmissionLimit.pendingReceipts == admittedReceiptObligations &&
		       atReceiptAdmissionLimit.receiptCapacityDeferrals ==
		           beforeReceiptReserve.receiptCapacityDeferrals + 1 &&
		       receiptReserveInboxFilled &&
		       receiptReserveInbox.pending == productionCommandLimits.maximumPending &&
		       receiptReserveOverflow.error ==
		           TacticalCommandSubmissionError::CapacityReached &&
		       afterReceiptReserveCancellation.lastCancellation.cancelled ==
		           productionCommandLimits.maximumPending &&
		       afterReceiptReserveCancellation.cancelledRequests ==
		           beforeReceiptReserveCancellation.cancelledRequests +
		               productionCommandLimits.maximumPending &&
		       afterReceiptReserveCancellation.pendingReceipts ==
		           admittedReceiptObligations +
		               productionCommandLimits.maximumPending &&
		       afterReceiptReserveCancellation.receiptDrops ==
		           beforeReceiptReserveCancellation.receiptDrops &&
		       tacticalCommands.service->summary().pending == 0,
		       "receipt admission preserves enough storage to terminally cancel a full package inbox" );

		liveRuntimeMessages.dispatchPending();
		for ( std::size_t pass = 0;
		      pass < 3 && GetJa2TacticalCommandHostDiagnostics().pendingReceipts != 0;
		      ++pass )
		{
			beginCommandTestFrame();
			DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
			liveRuntimeMessages.dispatchPending();
		}
		CHECK( GetJa2TacticalCommandHostDiagnostics().pendingReceipts == 0 &&
		       liveRuntimeMessages.queued() == 0,
		       "receipt reserve saturation fixture drains all retained terminal results" );
		(void)ReleaseJa2TacticalEntity( commandHostActor );
		SOLDIERTYPE* const restoredCommandActor =
			soldierRepository.replace( 0, previousCommandActor );
		if ( previousCommandEntity.valid() && restoredCommandActor )
			(void)AdoptJa2TacticalEntity( *restoredCommandActor );
		RestoreJa2TacticalWorldSession( previousCommandWorldSession );

		const auto tacticalWorld =
			compiledContext.serviceCatalog().resolve( TacticalWorldServiceContract );
		TacticalWorldSnapshot unavailableWorld;
		CHECK( tacticalWorld &&
		       tacticalWorld.service->capture( unavailableWorld ) ==
		           TacticalWorldCaptureResult::Unavailable,
		       "application composition root registers the live tactical world service" );
		const auto tacticalWorldObserver =
			compiledContext.serviceCatalog().resolve( TacticalWorldObserverServiceContract );
		const Ja2TacticalCommandHostDiagnostics commandsBeforeOrdering =
			GetJa2TacticalCommandHostDiagnostics();
		const Ja2TacticalWorldObserverDiagnostics observerBeforeOrdering =
			GetJa2TacticalWorldObserverDiagnostics();
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		const Ja2TacticalCommandHostDiagnostics commandsAfterOrdering =
			GetJa2TacticalCommandHostDiagnostics();
		UpdateJa2TacticalWorldObserverAtSafeFrame( liveRuntimeMessages );
		const Ja2TacticalWorldObserverDiagnostics observerBeforeWorld =
			GetJa2TacticalWorldObserverDiagnostics();
		CHECK( commandsAfterOrdering.safeFrameCalls ==
		           commandsBeforeOrdering.safeFrameCalls + 1 &&
		       commandsAfterOrdering.simulationTick ==
		           compiledContext.runtime().simulationTicks().completedTickSequence() &&
		       observerBeforeWorld.safeFrameUpdates ==
		           observerBeforeOrdering.safeFrameUpdates + 1 &&
		       observerBeforeWorld.lastUpdate ==
		           TacticalWorldObserverUpdateResult::SourceUnavailable,
		       "production safe-frame order drains completed-tick commands before observing tactical state" );
		const std::size_t queuedBeforeWorld = liveRuntimeMessages.queued();
		UpdateJa2TacticalWorldObserverAtSafeFrame( liveRuntimeMessages );
		const Ja2TacticalWorldObserverDiagnostics unavailableObservation =
			GetJa2TacticalWorldObserverDiagnostics();
		CHECK( tacticalWorldObserver && !tacticalWorldObserver.service->latest() &&
		       unavailableObservation.lastUpdate ==
		           TacticalWorldObserverUpdateResult::SourceUnavailable &&
		       unavailableObservation.publicationSerial ==
		           observerBeforeWorld.publicationSerial &&
		       unavailableObservation.safeFrameUpdates ==
		           observerBeforeWorld.safeFrameUpdates + 1 &&
		       unavailableObservation.bridgeResult ==
		           Ja2TacticalWorldDeltaBridgeResult::ObservationSuppressed &&
		       unavailableObservation.worldGeneration == 0 &&
		       unavailableObservation.turnSerial == 0 &&
		       unavailableObservation.worldTransitions == 0 &&
		       unavailableObservation.observerResets == 0 &&
		       unavailableObservation.publishAttempts == 0 &&
		       liveRuntimeMessages.queued() == queuedBeforeWorld,
		       "pre-world safe frames harmlessly retain an unavailable observer service" );

		Ja2TacticalWorldAdapter worldBindingFixture;
		worldBindingFixture.session().setSector( { 8, 9, 1 } );
		const std::uint64_t bindingFixtureGeneration =
			worldBindingFixture.session().commitLoad();
		worldBindingFixture.session().setTurnState(
			{ true, true, 3 } );
		TacticalWorldSession boundWorldFixture;
		worldBindingFixture.bindSession( boundWorldFixture );
		const TacticalWorldSession::Snapshot& boundWorldState =
			boundWorldFixture.snapshot();
		CHECK( &worldBindingFixture.session() == &boundWorldFixture &&
		       boundWorldState.sector ==
		           ( TacticalWorldSession::Sector{ 8, 9, 1 } ) &&
		       boundWorldState.loaded &&
		       boundWorldState.worldGeneration == bindingFixtureGeneration &&
		       boundWorldState.turn ==
		           ( TacticalWorldSession::Snapshot::Turn{ true, true, 3 } ),
		       "tactical world composition transfers pre-runtime state without a generation mirror" );

		const SOLDIERTYPE previousWorldActor = soldierRepository.record( 0 );
		SOLDIERTYPE& worldActor = soldierRepository.record( 0 );
		const TacticalEntityId previousWorldEntity =
			GetJa2TacticalEntityId( 0 );
		const TacticalWorldSession::Snapshot previousWorldSession =
			compiledContext.runtime().tacticalWorldSession().snapshot();
		const UINT32 previousTacticalProjectionFlags =
			CaptureJa2TacticalStatusFlags();
		const UINT8 previousTacticalProjectionTeam = GetJa2TacticalCurrentTeam();
		worldActor.ubID = SoldierID{ static_cast<UINT16>( 0 ) };
		worldActor.uiUniqueSoldierIdValue = 701;
		worldActor.bActive = TRUE;
		worldActor.bInSector = TRUE;
		worldActor.bTeam = 1;
		worldActor.ubProfile = 12;
		worldActor.position().gridNo() = 345;
		worldActor.position().level() = 1;
		worldActor.position().direction() = 3;
		worldActor.animationPlayback().state() = STANDING;
		worldActor.actionPoints().current() = 72;
		worldActor.vitals().health() = 76;
		worldActor.vitals().maximumHealth() = 80;
		worldActor.vitals().breath() = 64;
		worldActor.vitals().maximumBreath() = 90;
		const bool worldActorAdopted = AdoptJa2TacticalEntity( worldActor );
		const TacticalActorSnapshot* adoptedWorldActorState =
			compiledContext.runtime().tacticalEntityDirectory().state(
				TacticalEntityId{ 0, 701 } );
		CHECK( worldActorAdopted &&
		       compiledContext.runtime().tacticalEntityDirectory().identity( 0 ) ==
		           ( TacticalEntityId{ 0, 701 } ) &&
		       adoptedWorldActorState &&
		       adoptedWorldActorState->grid == 345 &&
		       adoptedWorldActorState->life == 76 &&
		       adoptedWorldActorState->stance == TacticalStance::Standing,
		       "legacy pool actors publish liveness and public state through the runtime-owned directory" );
		Ja2TacticalEntityReference liveCallbackActor;
		const bool callbackActorCaptured =
			liveCallbackActor.capture( &worldActor ) &&
			liveCallbackActor.identity() ==
				( TacticalEntityId{ 0, 701 } ) &&
			liveCallbackActor.resolve() == &worldActor;
		const bool dialogueDestinationCaptured =
			SetDialogueDestinationSoldier( &worldActor ) &&
			GetDialogueDestinationSoldier() == &worldActor;
		const bool contractRehireCaptured =
			SetContractRehireSoldier( &worldActor ) &&
			GetContractRehireSoldier() == &worldActor;
		const bool traversalActorCaptured =
			CaptureTacticalTraversalChosenSoldier( &worldActor ) &&
			ResolveTacticalTraversalChosenSoldier() == &worldActor;
		SOLDIERTYPE* consumedCallbackActor =
			liveCallbackActor.consume();
		Ja2TacticalEntityReference releasedCallbackActor;
		const bool releasedCallbackCaptured =
			releasedCallbackActor.capture( &worldActor );
		ResetTacticalInventoryUiActorContexts();
		const bool inventoryUiActorsCaptured =
			SetSMCurrentMerc( &worldActor ) &&
			SetItemPointerSoldier( &worldActor ) &&
			SetItemDescSoldier( &worldActor ) &&
			SetAttachSoldier( &worldActor ) &&
			SetItemPopupSoldier( &worldActor ) &&
			SetItemPickupActor( &worldActor ) &&
			SetItemPickupOpponent( &worldActor ) &&
			GetJa2TacticalInventoryUiSession().actorContextCount() == 7;
		const bool callbackActorReleased =
			ReleaseJa2TacticalEntity( worldActor );
		const bool releasedCallbackRejected =
			releasedCallbackActor.resolve() == nullptr;
		const bool releasedDialogueDestinationRejected =
			GetDialogueDestinationSoldier() == nullptr;
		const bool releasedContractRehireRejected =
			GetContractRehireSoldier() == nullptr;
		const bool releasedTraversalActorRejected =
			ResolveTacticalTraversalChosenSoldier() == nullptr;
		const bool releasedInventoryUiActorsRejected =
			HasJa2TacticalInventoryActorContext(
				TacticalInventoryActorRole::SelectedMerc) &&
			HasJa2TacticalInventoryActorContext(
				TacticalInventoryActorRole::PickupActor) &&
			!GetSMCurrentMerc() &&
			!GetItemPointerSoldier() &&
			!GetItemDescSoldier() &&
			!GetAttachSoldier() &&
			!GetItemPopupSoldier() &&
			!GetItemPickupActor() &&
			!GetItemPickupOpponent();
		worldActor.uiUniqueSoldierIdValue = 703;
		const bool replacementInventoryActorAdopted =
			AdoptJa2TacticalEntity( worldActor );
		const bool replacementInventoryActorRejected =
			!GetSMCurrentMerc() &&
			!GetItemPointerSoldier() &&
			!GetItemDescSoldier() &&
			!GetAttachSoldier() &&
			!GetItemPopupSoldier() &&
			!GetItemPickupActor() &&
			!GetItemPickupOpponent();
		const bool replacementInventoryActorReleased =
			ReleaseJa2TacticalEntity( worldActor );
		ResetTacticalInventoryUiActorContexts();
		worldActor.uiUniqueSoldierIdValue = 701;
		ResetMercContractActorContexts();
		ResetTacticalTraversalContext();
		const bool callbackActorReadopted =
			AdoptJa2TacticalEntity( worldActor );
		CHECK( callbackActorCaptured &&
		       dialogueDestinationCaptured &&
		       contractRehireCaptured &&
		       consumedCallbackActor == &worldActor &&
		       !liveCallbackActor.valid() &&
		       releasedCallbackCaptured && callbackActorReleased &&
		       releasedCallbackRejected &&
		       releasedDialogueDestinationRejected &&
		       releasedContractRehireRejected &&
		       traversalActorCaptured &&
		       releasedTraversalActorRejected &&
		       inventoryUiActorsCaptured &&
		       releasedInventoryUiActorsRejected &&
		       replacementInventoryActorAdopted &&
		       replacementInventoryActorRejected &&
		       replacementInventoryActorReleased &&
		       callbackActorReadopted,
		       "delayed callbacks and inventory UI roles reject released and reused actor incarnations" );
		SOLDIERTYPE& swapTarget = soldierRepository.record( 1 );
		const SOLDIERTYPE previousSwapTarget = swapTarget;
		const bool swapTargetInstalled =
			soldierRepository.replace( 1, worldActor ) == &swapTarget;
		swapTarget.ubID = SoldierID{ static_cast<UINT16>( 1 ) };
		swapTarget.uiUniqueSoldierIdValue = 702;
		swapTarget.position().gridNo() = 678;
		const bool swapTargetAdopted =
			swapTargetInstalled && AdoptJa2TacticalEntity( swapTarget );
		const bool entitySlotsSwapped = SwapJa2TacticalEntitySlots( 0, 1 );
		const bool swappedEntitiesResolvable =
			GetJa2TacticalEntityId( 0 ) == ( TacticalEntityId{ 0, 702 } ) &&
			GetJa2TacticalEntityId( 1 ) == ( TacticalEntityId{ 1, 701 } ) &&
			ResolveJa2TacticalEntity( TacticalEntityId{ 0, 702 } ) == &worldActor &&
			ResolveJa2TacticalEntity( TacticalEntityId{ 1, 701 } ) == &swapTarget &&
			worldActor.position().gridNo() == 678 && swapTarget.position().gridNo() == 345;
		const bool entitySlotsRestored = SwapJa2TacticalEntitySlots( 0, 1 );
		CHECK( swapTargetInstalled && swapTargetAdopted &&
		       entitySlotsSwapped && swappedEntitiesResolvable &&
		       entitySlotsRestored &&
		       GetJa2TacticalEntityId( 0 ) == ( TacticalEntityId{ 0, 701 } ) &&
		       !SwapJa2TacticalEntitySlots( 0, 0 ) &&
		       !SwapJa2TacticalEntitySlots( TOTAL_SOLDIERS, 0 ),
		       "whole-record portrait swaps rebuild authoritative tactical entity identities" );
		(void)soldierRepository.replace( 1, previousSwapTarget );
		RebuildJa2TacticalEntityDirectory();

		RebuildJa2StrategicGroupDirectory();
		UINT16 strategicGroupSlot = 255;
		while (strategicGroupSlot > 0 &&
			GetGroup(static_cast<UINT8>(strategicGroupSlot)))
			--strategicGroupSlot;
		CHECK(strategicGroupSlot > 0,
			"headless strategic-group fixture finds one unused legacy ID");
		GROUP strategicGroupFixture{};
		strategicGroupFixture.ubGroupID =
			static_cast<UINT8>(strategicGroupSlot);
		GROUP* const previousStrategicGroupList = gpGroupList;
		strategicGroupFixture.next = previousStrategicGroupList;
		gpGroupList = &strategicGroupFixture;
		const bool strategicGroupAdopted =
			AdoptJa2StrategicGroup(strategicGroupFixture);
		Ja2StrategicGroupReference delayedStrategicGroup;
		const bool delayedStrategicGroupCaptured =
			delayedStrategicGroup.capture(&strategicGroupFixture);
		const bool preBattleGroupCaptured =
			SetPreBattleGroup(&strategicGroupFixture) &&
			ResolvePreBattleGroup() == &strategicGroupFixture;
		const bool traversalGroupCaptured =
			CaptureTacticalTraversalGroup(&strategicGroupFixture) &&
			ResolveTacticalTraversalGroup() == &strategicGroupFixture;
		const StrategicGroupId firstStrategicIdentity =
			GetJa2StrategicGroupId(strategicGroupFixture.ubGroupID);
		const bool strategicGroupReleased =
			ReleaseJa2StrategicGroup(strategicGroupFixture);
		const bool staleStrategicContextsRejected =
			!delayedStrategicGroup.resolve() &&
			!ResolvePreBattleGroup() &&
			!ResolveTacticalTraversalGroup();
		const bool strategicGroupReadopted =
			AdoptJa2StrategicGroup(strategicGroupFixture);
		const StrategicGroupId replacementStrategicIdentity =
			GetJa2StrategicGroupId(strategicGroupFixture.ubGroupID);
		CHECK(strategicGroupAdopted && delayedStrategicGroupCaptured &&
			preBattleGroupCaptured && traversalGroupCaptured &&
			firstStrategicIdentity.valid() && strategicGroupReleased &&
			staleStrategicContextsRejected && strategicGroupReadopted &&
			replacementStrategicIdentity.valid() &&
			firstStrategicIdentity != replacementStrategicIdentity &&
			!delayedStrategicGroup.resolve(),
			"battle, traversal, and delayed strategic contexts reject reused group IDs");
		ResetPreBattleGroup();
		ResetTacticalTraversalContext();
		(void)ReleaseJa2StrategicGroup(strategicGroupFixture);
		gpGroupList = previousStrategicGroupList;
		RebuildJa2StrategicGroupDirectory();

		SetJa2TacticalWorldSector( 4, 5, 2 );
		CHECK( CaptureJa2TacticalWorld().sector ==
		           ( TacticalWorldSession::Sector{ 4, 5, 2 } ) &&
		       gWorldSectorX == 4 && gWorldSectorY == 5 && gbWorldSectorZ == 2,
		       "sector transitions atomically publish read-only coordinate projections" );
		SetJa2TacticalWorldDepth( 1 );
		CHECK( CaptureJa2TacticalWorld().sector ==
		           ( TacticalWorldSession::Sector{ 4, 5, 1 } ) &&
		       gWorldSectorX == 4 && gWorldSectorY == 5 && gbWorldSectorZ == 1,
		       "depth-only transitions cannot split the tactical session and projections" );
		ClearJa2TacticalWorldSector();
		CHECK( CaptureJa2TacticalWorld().sector ==
		           TacticalWorldSession::Sector{} &&
		       gWorldSectorX == 0 && gWorldSectorY == 0 && gbWorldSectorZ == -1,
		       "clearing a tactical sector publishes the canonical empty projection" );

		SetJa2TacticalWorldSector( 13, 4, 2 );
		const std::uint64_t projectionLoadGeneration =
			CommitJa2TacticalWorldLoad();
		CHECK( projectionLoadGeneration != 0 &&
		       CaptureJa2TacticalWorld().loaded &&
		       CaptureJa2TacticalWorld().worldGeneration ==
		           projectionLoadGeneration &&
		       CaptureJa2TacticalWorld().turnSerial == 1 &&
		       gWorldSectorX == 13 && gWorldSectorY == 4 && gbWorldSectorZ == 2,
		       "committing a world load retains one session-owned identity and coordinate view" );

		RestoreJa2TacticalTurnState( ACTIVE | TURNBASED, 4, 0 );
		CHECK( CaptureJa2TacticalWorld().turn ==
		           ( TacticalWorldSession::Snapshot::Turn{ true, false, 4 } ),
		       "tactical-turn restore initializes the runtime-owned state" );
		ResetJa2TacticalCombatActions();
		const bool firstCombatActionAccepted =
			BeginJa2TacticalCombatAction();
		const bool secondCombatActionAccepted =
			BeginJa2TacticalCombatAction();
		const bool combatActionCompleted =
			CompleteJa2TacticalCombatAction();
		CHECK( firstCombatActionAccepted && secondCombatActionAccepted &&
		       combatActionCompleted &&
		       CaptureJa2TacticalWorld().turn.pendingCombatActions == 1 &&
		       GetJa2PendingTacticalCombatActions() == 1,
		       "combat-action gateways update the runtime owner" );
		bool wideCombatActionCountAccepted = true;
		for ( std::uint32_t pending = 1; pending < 256; ++pending )
			wideCombatActionCountAccepted =
				BeginJa2TacticalCombatAction() &&
				wideCombatActionCountAccepted;
		CHECK( wideCombatActionCountAccepted &&
		       GetJa2PendingTacticalCombatActions() == 256 &&
		       CaptureJa2SerializedPendingCombatActions() == 255,
		       "the engine retains overlapping combat work beyond the serialized byte" );
		ResetJa2TacticalCombatActions();
		CHECK( BeginJa2TacticalCombatAction() &&
		       GetJa2PendingTacticalCombatActions() == 1,
		       "combat-action reset republishes one coherent idle boundary" );
		SetJa2TacticalCombatMode( true );
		SetJa2TacticalTurnBasedMode( false );
		SetJa2TacticalCurrentTeam( 5 );
		AdvanceJa2TacticalCurrentTeam();
		CHECK( CaptureJa2TacticalWorld().turn ==
		           ( TacticalWorldSession::Snapshot::Turn{ false, true, 6, 1 } ) &&
		       ( gTacticalStatus.uiFlags & ACTIVE ) != 0 &&
		       ( gTacticalStatus.uiFlags & (TURNBASED | INCOMBAT) ) == 0 &&
		       !IsJa2TacticalTurnBased() &&
		       IsJa2TacticalCombatActive() &&
		       GetJa2TacticalCurrentTeam() == 6 &&
		       GetJa2PendingTacticalCombatActions() == 1,
		       "turn gateways preserve unrelated tactical flags without live mirrors" );

		RestoreJa2TacticalTurnState(
			ACTIVE | TURNBASED | INCOMBAT, 2, 7 );
		CHECK( CaptureJa2TacticalWorld().turn ==
		           ( TacticalWorldSession::Snapshot::Turn{ true, true, 2, 7 } ) &&
		       ( CaptureJa2TacticalStatusFlags() &
		           (TURNBASED | INCOMBAT) ) ==
		           (TURNBASED | INCOMBAT) &&
		       ( gTacticalStatus.uiFlags & (TURNBASED | INCOMBAT) ) == 0,
		       "save/bootstrap restoration publishes tactical battle values into the session" );

		TacticalWorldSession::Snapshot restoredProjectionState;
		restoredProjectionState.sector = { 8, 7, 1 };
		restoredProjectionState.loaded = true;
		restoredProjectionState.worldGeneration = 77;
		restoredProjectionState.turnSerial = 11;
		restoredProjectionState.turn = { true, false, 3, 4 };
		RestoreJa2TacticalWorldSession( restoredProjectionState );
		CHECK( CaptureJa2TacticalWorld().sector ==
		           restoredProjectionState.sector &&
		       CaptureJa2TacticalWorld().loaded &&
		       CaptureJa2TacticalWorld().worldGeneration == 77 &&
		       CaptureJa2TacticalWorld().turnSerial == 11 &&
		       CaptureJa2TacticalWorld().turn == restoredProjectionState.turn &&
		       gWorldSectorX == 8 && gWorldSectorY == 7 && gbWorldSectorZ == 1 &&
		       ( gTacticalStatus.uiFlags & ACTIVE ) != 0 &&
		       IsJa2TacticalTurnBased() &&
		       !IsJa2TacticalCombatActive() &&
		       GetJa2TacticalCurrentTeam() == 3 &&
		       GetJa2PendingTacticalCombatActions() == 4,
		       "session restore republishes coordinate and battle state as one boundary" );
		NotifyJa2TacticalTeamTurnBegan( 77 );
		CHECK( CaptureJa2TacticalWorld().turnSerial == 12 &&
		       gWorldSectorX == 8 && gWorldSectorY == 7 && gbWorldSectorZ == 1,
		       "team-turn publication advances identity without disturbing sector projections" );
		NotifyJa2TacticalWorldLoaded( 78 );
		CHECK( CaptureJa2TacticalWorld().loaded &&
		       CaptureJa2TacticalWorld().worldGeneration == 78 &&
		       CaptureJa2TacticalWorld().turnSerial == 1 &&
		       CaptureJa2TacticalWorld().turn.pendingCombatActions == 0 &&
		       GetJa2PendingTacticalCombatActions() == 0 &&
		       gWorldSectorX == 8 && gWorldSectorY == 7 && gbWorldSectorZ == 1,
		       "world lifecycle publication resets turn identity and pending work without splitting coordinates" );
		NotifyJa2TacticalWorldUnloaded();
		CHECK( !CaptureJa2TacticalWorld().loaded &&
		       CaptureJa2TacticalWorld().turnSerial == 0 &&
		       gWorldSectorX == 8 && gWorldSectorY == 7 && gbWorldSectorZ == 1,
		       "world unload retires identity while retaining the selected-sector projection" );

		RestoreJa2TacticalWorldSession( previousWorldSession );
		RestoreJa2TacticalTurnState(
			previousTacticalProjectionFlags, previousTacticalProjectionTeam );
		CHECK( CaptureJa2TacticalWorld().sector == previousWorldSession.sector &&
		       CaptureJa2TacticalWorld().loaded == previousWorldSession.loaded &&
		       gWorldSectorX == previousWorldSession.sector.x &&
		       gWorldSectorY == previousWorldSession.sector.y &&
		       gbWorldSectorZ == previousWorldSession.sector.z,
		       "projection invariant fixture restores the pre-test tactical session" );

		SetJa2TacticalWorldSector( 9, 1, 0 );
		NotifyJa2TacticalWorldLoaded( 23 );
		CHECK( IsJa2TacticalWorldLoaded() &&
		       CaptureJa2TacticalWorld().loaded,
		       "world-loaded state is read from the runtime-owned tactical session" );
		SetJa2TacticalTurnBasedMode( true );
		SetJa2TacticalCombatMode( true );
		SetJa2TacticalCurrentTeam( 1 );
		const TacticalWorldSession::Snapshot::Turn ownedTacticalTurn =
			compiledContext.runtime().tacticalWorldSession().snapshot().turn;
		CHECK( ownedTacticalTurn ==
		           ( TacticalWorldSession::Snapshot::Turn{ true, true, 1 } ) &&
		       IsJa2TacticalTurnBasedCombat() &&
		       GetJa2TacticalCurrentTeam() == 1,
		       "tactical mode and current team are read from the runtime owner" );
		Ja2TacticalWorldAdapter turnIdentityFixture( 0 );
		turnIdentityFixture.onWorldLoaded( 23 );
		const Ja2TacticalTurnIdentity loadedTurnIdentity =
			turnIdentityFixture.turnIdentity();
		turnIdentityFixture.onTeamTurnBegan( 23 );
		const Ja2TacticalTurnIdentity advancedTurnIdentity =
			turnIdentityFixture.turnIdentity();
		turnIdentityFixture.onWorldUnloaded();
		const Ja2TacticalTurnIdentity unloadedTurnIdentity =
			turnIdentityFixture.turnIdentity();
		CHECK( loadedTurnIdentity.worldGeneration == 23 &&
		       loadedTurnIdentity.serial == 1 && advancedTurnIdentity.worldGeneration == 23 &&
		       advancedTurnIdentity.serial == 2 && !unloadedTurnIdentity,
		       "live tactical turn identity is nonzero, advances, and resets with its world" );
		NotifyJa2TacticalWorldLoaded(
			CaptureJa2TacticalWorld().worldGeneration );
		NotifyJa2TacticalTeamTurnBegan(
			CaptureJa2TacticalWorld().worldGeneration );
		TacticalWorldSnapshot liveWorld;
		const TacticalWorldCaptureResult liveCapture =
			tacticalWorld.service->capture( liveWorld );
		const TacticalActorSnapshot* liveActor =
			liveWorld.find( TacticalEntityId{ 0, 701 } );
		CHECK( liveCapture == TacticalWorldCaptureResult::Success &&
		       liveWorld.epoch() == 23 && liveWorld.sector().x == 9 &&
		       liveWorld.turn().serial == 2 && liveWorld.turn().turnBased &&
		       liveWorld.turn().inCombat && liveWorld.turn().activeTeam == 1 &&
		       liveActor && liveActor->grid == 345 && liveActor->level == 1 &&
		       liveActor->stance == TacticalStance::Standing && liveActor->life == 76,
		       "live tactical service captures stable pointer-free legacy soldier state" );
		const std::size_t liveActorCapacity = liveWorld.actors().capacity();
		const TacticalWorldCaptureResult repeatedLiveCapture =
			tacticalWorld.service->capture( liveWorld );
		liveActor = liveWorld.find( TacticalEntityId{ 0, 701 } );
		CHECK( repeatedLiveCapture == TacticalWorldCaptureResult::Success && liveActor &&
		       liveWorld.actors().capacity() == liveActorCapacity &&
		       liveActorCapacity >= TOTAL_SOLDIERS,
		       "live tactical capture reuses bounded actor storage without shrinking" );

		const PackageSaveStateCaptureResult packageSaveBeforeObservation =
			compiledContext.capturePackageSaveState();
		const std::vector<RecordedSimulationCommand> replayBeforeObservation =
			compiledContext.commandJournal().snapshot();
		const std::uint64_t replayDroppedBeforeObservation =
			compiledContext.commandJournal().droppedCount();
		HeadlessRuntimeMessageSink liveTacticalDeltaSink;
		const bool liveTacticalDeltaSinkAdded =
			liveRuntimeMessages.addSink( liveTacticalDeltaSink ) ==
				RuntimeMessageSinkRegistrationError::None;
		UpdateJa2TacticalWorldObserverAtSafeFrame( liveRuntimeMessages );
		Ja2TacticalWorldObserverDiagnostics observerDiagnostics =
			GetJa2TacticalWorldObserverDiagnostics();
		TacticalWorldPublicationView observedPublication =
			tacticalWorldObserver.service->latest();
		const TacticalActorSnapshot* observedActor = observedPublication
			? observedPublication.snapshot->find( TacticalEntityId{ 0, 701 } ) : nullptr;
		CHECK( observerDiagnostics.lastUpdate ==
		           TacticalWorldObserverUpdateResult::PublishedBaseline &&
		       observerDiagnostics.publicationSerial == 1 && observedPublication &&
		       observedPublication.status == TacticalWorldPublicationStatus::Baseline &&
		       observedPublication.delta->events.empty() && observedActor &&
		       observedActor->grid == 345 && observedActor->life == 76 &&
		       observerDiagnostics.bridgeResult ==
		           Ja2TacticalWorldDeltaBridgeResult::BaselineSuppressed &&
		       observerDiagnostics.worldGeneration == 23 &&
		       observerDiagnostics.turnSerial == 2 &&
		       observerDiagnostics.worldTransitions == 1 &&
		       observerDiagnostics.observerResets == 0 &&
		       observerDiagnostics.publishAttempts == 0 &&
		       observerDiagnostics.preparationAttempts == 0 && liveTacticalDeltaSinkAdded &&
		       liveRuntimeMessages.queued() == 0 && liveTacticalDeltaSink.messages.empty(),
		       "production baseline observation is retained without publishing a message" );
		const TacticalWorldSnapshot* baselinePublicationStorage = observedPublication.snapshot;
		const TacticalWorldDelta* baselineDeltaStorage = observedPublication.delta;

		NotifyJa2TacticalTeamTurnBegan(
			CaptureJa2TacticalWorld().worldGeneration );
		worldActor.position().gridNo() = 346;
		worldActor.vitals().health() = 75;
		UpdateJa2TacticalWorldObserverAtSafeFrame( liveRuntimeMessages );
		observerDiagnostics = GetJa2TacticalWorldObserverDiagnostics();
		observedPublication = tacticalWorldObserver.service->latest();
		observedActor = observedPublication
			? observedPublication.snapshot->find( TacticalEntityId{ 0, 701 } ) : nullptr;
		const TacticalActorSnapshot* committedObservedActor =
			compiledContext.runtime().tacticalEntityDirectory().state(
				TacticalEntityId{ 0, 701 } );
		CHECK( observerDiagnostics.lastUpdate ==
		           TacticalWorldObserverUpdateResult::PublishedDelta &&
		       observerDiagnostics.publicationSerial == 2 && observedPublication &&
		       observedPublication.status == TacticalWorldPublicationStatus::Delta &&
		       observedPublication.delta->events.size() == 3 &&
		       std::holds_alternative<TacticalTurnChangedEvent>(
		           observedPublication.delta->events[0] ) &&
		       std::holds_alternative<TacticalActorMovedEvent>(
		           observedPublication.delta->events[1] ) &&
		       std::holds_alternative<TacticalActorVitalsChangedEvent>(
		           observedPublication.delta->events[2] ) &&
		       observedActor && observedActor->grid == 346 && observedActor->life == 75 &&
		       committedObservedActor &&
		       committedObservedActor->grid == observedActor->grid &&
		       committedObservedActor->life == observedActor->life &&
		       observerDiagnostics.bridgeResult ==
		           Ja2TacticalWorldDeltaBridgeResult::Published &&
		       observerDiagnostics.lastPublishError == TacticalWorldDeltaPublishError::None &&
		       observerDiagnostics.handledDeltaSerial == 2 &&
		       observerDiagnostics.publishedDeltaSerial == 2 &&
		       observerDiagnostics.turnSerial == 3 &&
		       observerDiagnostics.messageSequence != 0 &&
		       observerDiagnostics.publishAttempts == 1 &&
		       observerDiagnostics.preparationAttempts == 1 &&
		       observerDiagnostics.publishedMessages == 1 &&
		       liveRuntimeMessages.queued() == 1 && liveTacticalDeltaSink.messages.empty(),
		       "production safe-frame bridge queues one message for a new non-empty delta" );
		const TacticalWorldSnapshot* deltaPublicationStorage = observedPublication.snapshot;
		const TacticalWorldDelta* deltaStorage = observedPublication.delta;
		CHECK( deltaPublicationStorage != baselinePublicationStorage &&
		       deltaStorage != baselineDeltaStorage,
		       "production observation publishes through an independent second buffer" );

		const FrameRunResult tacticalDeltaDeliveryFrame =
			compiledContext.frameDriver().runFrame(
				[] { return FramePlan{ false, FramePresentMode::Paced }; }, [] {} );
		TacticalWorldDelta deliveredLiveDelta;
		const RuntimeMessage* deliveredLiveMessage =
			liveTacticalDeltaSink.messages.size() == 1
				? &liveTacticalDeltaSink.messages[0] : nullptr;
		const bool deliveredLiveDeltaDecoded = deliveredLiveMessage &&
			DecodeTacticalWorldDelta(
				deliveredLiveMessage->payload, deliveredLiveDelta ) ==
					TacticalWorldDeltaDecodeResult::Success;
		CHECK( tacticalDeltaDeliveryFrame.messages.messages == 1 &&
		       liveRuntimeMessages.queued() == 0 && deliveredLiveDeltaDecoded &&
		       deliveredLiveMessage->topic == TacticalWorldDeltaMessageTopic &&
		       deliveredLiveMessage->source == TacticalWorldDeltaMessageSource &&
		       deliveredLiveDelta.events.size() == 3 &&
		       std::holds_alternative<TacticalTurnChangedEvent>(
		           deliveredLiveDelta.events[0] ) &&
		       std::holds_alternative<TacticalActorMovedEvent>(
		           deliveredLiveDelta.events[1] ) &&
		       std::holds_alternative<TacticalActorVitalsChangedEvent>(
		           deliveredLiveDelta.events[2] ) &&
		       std::get<TacticalTurnChangedEvent>(
		           deliveredLiveDelta.events[0] ).current.serial == 3 &&
		       std::get<TacticalActorMovedEvent>( deliveredLiveDelta.events[1] ).currentGrid ==
		           346 &&
		       std::get<TacticalActorVitalsChangedEvent>(
		           deliveredLiveDelta.events[2] ).currentLife == 75,
		       "queued tactical delta reaches package sinks and decodes on the next frame" );

		worldActor.uiUniqueSoldierIdValue = 0;
		UpdateJa2TacticalWorldObserverAtSafeFrame( liveRuntimeMessages );
		observerDiagnostics = GetJa2TacticalWorldObserverDiagnostics();
		observedPublication = tacticalWorldObserver.service->latest();
		observedActor = observedPublication
			? observedPublication.snapshot->find( TacticalEntityId{ 0, 701 } ) : nullptr;
		CHECK( observerDiagnostics.lastUpdate ==
		           TacticalWorldObserverUpdateResult::SourceAdapterFailure &&
		       observerDiagnostics.publicationSerial == 2 && observedPublication &&
		       observedPublication.snapshot == deltaPublicationStorage &&
		       observedPublication.delta == deltaStorage &&
		       observedPublication.delta->events.size() == 3 && observedActor &&
		       observedActor->grid == 346 && observedActor->life == 75 &&
		       observerDiagnostics.bridgeResult ==
		           Ja2TacticalWorldDeltaBridgeResult::ObservationSuppressed &&
		       observerDiagnostics.publishAttempts == 1 &&
		       observerDiagnostics.preparationAttempts == 1 &&
		       observerDiagnostics.publishedMessages == 1 &&
		       liveRuntimeMessages.queued() == 0,
		       "live adapter failure preserves the last complete observer publication" );

		worldActor.uiUniqueSoldierIdValue = 701;
		UpdateJa2TacticalWorldObserverAtSafeFrame( liveRuntimeMessages );
		observerDiagnostics = GetJa2TacticalWorldObserverDiagnostics();
		observedPublication = tacticalWorldObserver.service->latest();
		CHECK( observerDiagnostics.lastUpdate ==
		           TacticalWorldObserverUpdateResult::Unchanged &&
		       observerDiagnostics.publicationSerial == 2 &&
		       observedPublication.snapshot == deltaPublicationStorage &&
		       observedPublication.delta == deltaStorage &&
		       observerDiagnostics.bridgeResult ==
		           Ja2TacticalWorldDeltaBridgeResult::EmptyDeltaSuppressed &&
		       observerDiagnostics.handledDeltaSerial == 2 &&
		       observerDiagnostics.publishedDeltaSerial == 2 &&
		       observerDiagnostics.publishAttempts == 1 &&
		       observerDiagnostics.preparationAttempts == 1 &&
		       observerDiagnostics.publishedMessages == 1 &&
		       liveRuntimeMessages.queued() == 0,
		       "unchanged live captures suppress empty and duplicate delta messages" );

		RuntimeMessageBus saturatedTacticalMessages(
			1, liveRuntimeMessages.maxPayloadBytes() );
		const RuntimeMessagePublishResult saturatedFiller =
			saturatedTacticalMessages.publish(
				RuntimeMessageRequest{ "test.queue-fill", "test.headless", {} } );
		worldActor.position().gridNo() = 347;
		UpdateJa2TacticalWorldObserverAtSafeFrame( saturatedTacticalMessages );
		observerDiagnostics = GetJa2TacticalWorldObserverDiagnostics();
		observedPublication = tacticalWorldObserver.service->latest();
		observedActor = observedPublication
			? observedPublication.snapshot->find( TacticalEntityId{ 0, 701 } ) : nullptr;
		CHECK( saturatedFiller && saturatedTacticalMessages.queued() == 1 &&
		       liveRuntimeMessages.queued() == 0 &&
		       observerDiagnostics.lastUpdate ==
		           TacticalWorldObserverUpdateResult::PublishedDelta &&
		       observerDiagnostics.publicationSerial == 3 &&
		       observerDiagnostics.bridgeResult ==
		           Ja2TacticalWorldDeltaBridgeResult::PublishFailed &&
		       observerDiagnostics.lastPublishError == TacticalWorldDeltaPublishError::QueueFull &&
		       observerDiagnostics.handledDeltaSerial == 3 &&
		       observerDiagnostics.pendingDeltaSerial == 3 &&
		       observerDiagnostics.pendingTransferId == 2 &&
		       observerDiagnostics.nextTransferId == 3 &&
		       observerDiagnostics.publishedDeltaSerial == 2 &&
		       observerDiagnostics.publishAttempts == 2 &&
		       observerDiagnostics.preparationAttempts == 2 &&
		       observerDiagnostics.publicationFailures == 1 && observedActor &&
		       observedActor->grid == 347 && observedPublication.delta->events.size() == 1,
		       "queue-full publication retains one bounded observer delta for retry" );

		UpdateJa2TacticalWorldObserverAtSafeFrame( saturatedTacticalMessages );
		observerDiagnostics = GetJa2TacticalWorldObserverDiagnostics();
		CHECK( saturatedTacticalMessages.queued() == 1 &&
		       liveRuntimeMessages.queued() == 0 &&
		       observerDiagnostics.lastUpdate ==
		           TacticalWorldObserverUpdateResult::PublishedDelta &&
		       observerDiagnostics.publicationSerial == 3 &&
		       observerDiagnostics.bridgeResult ==
		           Ja2TacticalWorldDeltaBridgeResult::PublishFailed &&
		       observerDiagnostics.lastPublishError == TacticalWorldDeltaPublishError::QueueFull &&
		       observerDiagnostics.handledDeltaSerial == 3 &&
		       observerDiagnostics.pendingDeltaSerial == 3 &&
		       observerDiagnostics.pendingTransferId == 2 &&
		       observerDiagnostics.nextTransferId == 3 &&
		       observerDiagnostics.publishedDeltaSerial == 2 &&
		       observerDiagnostics.publishAttempts == 3 &&
		       observerDiagnostics.preparationAttempts == 2 &&
		       observerDiagnostics.publishedMessages == 1 &&
		       observerDiagnostics.publicationFailures == 2,
		       "a failed retry backpressures observation and retains the same delta serial" );

		UpdateJa2TacticalWorldObserverAtSafeFrame( liveRuntimeMessages );
		observerDiagnostics = GetJa2TacticalWorldObserverDiagnostics();
		CHECK( liveRuntimeMessages.queued() == 1 &&
		       observerDiagnostics.lastUpdate ==
		           TacticalWorldObserverUpdateResult::PublishedDelta &&
		       observerDiagnostics.publicationSerial == 3 &&
		       observerDiagnostics.bridgeResult ==
		           Ja2TacticalWorldDeltaBridgeResult::Published &&
		       observerDiagnostics.lastPublishError == TacticalWorldDeltaPublishError::None &&
		       observerDiagnostics.handledDeltaSerial == 3 &&
		       observerDiagnostics.pendingDeltaSerial == 0 &&
		       observerDiagnostics.pendingTransferId == 0 &&
		       observerDiagnostics.nextTransferId == 3 &&
		       observerDiagnostics.publishedDeltaSerial == 3 &&
		       observerDiagnostics.publishAttempts == 4 &&
		       observerDiagnostics.preparationAttempts == 2 &&
		       observerDiagnostics.publishedMessages == 2 &&
		       observerDiagnostics.publicationFailures == 2,
		       "the next safe frame retries the retained serial before taking another observation" );

		const FrameRunResult retriedDeltaDeliveryFrame =
			compiledContext.frameDriver().runFrame(
				[] { return FramePlan{ false, FramePresentMode::Paced }; }, [] {} );
		TacticalWorldDelta retriedLiveDelta;
		const RuntimeMessage* retriedLiveMessage =
			liveTacticalDeltaSink.messages.size() == 2
				? &liveTacticalDeltaSink.messages[1] : nullptr;
		const bool retriedLiveDeltaDecoded = retriedLiveMessage &&
			DecodeTacticalWorldDelta(
				retriedLiveMessage->payload, retriedLiveDelta ) ==
					TacticalWorldDeltaDecodeResult::Success;
		const bool liveTacticalDeltaSinkRemoved =
			liveRuntimeMessages.removeSink( liveTacticalDeltaSink ) ==
				RuntimeMessageSinkRegistrationError::None;
		CHECK( retriedDeltaDeliveryFrame.messages.messages == 1 &&
		       liveRuntimeMessages.queued() == 0 && retriedLiveDeltaDecoded &&
		       retriedLiveMessage->topic == TacticalWorldDeltaMessageTopic &&
		       retriedLiveMessage->source == TacticalWorldDeltaMessageSource &&
		       retriedLiveDelta.events.size() == 1 &&
		       std::holds_alternative<TacticalActorMovedEvent>(
		           retriedLiveDelta.events[0] ) &&
		       std::get<TacticalActorMovedEvent>(
		           retriedLiveDelta.events[0] ).currentGrid == 347 &&
		       liveTacticalDeltaSinkRemoved,
		       "the retained tactical delta is delivered and decodes on the next frame" );

		UpdateJa2TacticalWorldObserverAtSafeFrame( liveRuntimeMessages );
		observerDiagnostics = GetJa2TacticalWorldObserverDiagnostics();
		CHECK( liveRuntimeMessages.queued() == 0 &&
		       observerDiagnostics.lastUpdate ==
		           TacticalWorldObserverUpdateResult::Unchanged &&
		       observerDiagnostics.publicationSerial == 3 &&
		       observerDiagnostics.bridgeResult ==
		           Ja2TacticalWorldDeltaBridgeResult::EmptyDeltaSuppressed &&
		       observerDiagnostics.lastPublishError == TacticalWorldDeltaPublishError::None &&
		       observerDiagnostics.handledDeltaSerial == 3 &&
		       observerDiagnostics.pendingDeltaSerial == 0 &&
		       observerDiagnostics.publishedDeltaSerial == 3 &&
		       observerDiagnostics.publishAttempts == 4 &&
		       observerDiagnostics.preparationAttempts == 2 &&
		       observerDiagnostics.publishedMessages == 2 &&
		       observerDiagnostics.publicationFailures == 2,
		       "observation resumes after retry and suppresses the unchanged frame" );

		RuntimeMessageBus saturatedChunkMessages(
			1, TacticalWorldDeltaChunkHeaderBytes + 4 );
		worldActor.position().gridNo() = 348;
		UpdateJa2TacticalWorldObserverAtSafeFrame( saturatedChunkMessages );
		observerDiagnostics = GetJa2TacticalWorldObserverDiagnostics();
		CHECK( saturatedChunkMessages.queued() == 1 && observerDiagnostics.lastUpdate ==
		           TacticalWorldObserverUpdateResult::PublishedDelta &&
		       observerDiagnostics.publicationSerial == 4 &&
		       observerDiagnostics.bridgeResult ==
		           Ja2TacticalWorldDeltaBridgeResult::PublishFailed &&
		       observerDiagnostics.lastPublishError ==
		           TacticalWorldDeltaPublishError::QueueFull &&
		       observerDiagnostics.pendingDeltaSerial == 4 &&
		       observerDiagnostics.pendingTransferId == 3 &&
		       observerDiagnostics.nextTransferId == 4 &&
		       observerDiagnostics.publishAttempts == 5 &&
		       observerDiagnostics.preparationAttempts == 3 &&
		       observerDiagnostics.pendingBatchMessages > 1 &&
		       observerDiagnostics.pendingBatchCursor == 1 &&
		       observerDiagnostics.chunkedDeltasPrepared == 1 &&
		       observerDiagnostics.physicalMessagesPublished == 3 &&
		       observerDiagnostics.publicationFailures == 3 &&
		       observerDiagnostics.discardedPendingDeltas == 0,
		       "a partially published chunk batch remains prepared on its original bus" );

		UpdateJa2TacticalWorldObserverAtSafeFrame( liveRuntimeMessages );
		observerDiagnostics = GetJa2TacticalWorldObserverDiagnostics();
		CHECK( liveRuntimeMessages.queued() == 0 &&
		       saturatedChunkMessages.queued() == 1 &&
		       observerDiagnostics.bridgeResult ==
		           Ja2TacticalWorldDeltaBridgeResult::PublishFailed &&
		       observerDiagnostics.lastPublishError ==
		           TacticalWorldDeltaPublishError::MessageBusChanged &&
		       observerDiagnostics.pendingDeltaSerial == 4 &&
		       observerDiagnostics.pendingTransferId == 3 &&
		       observerDiagnostics.pendingBatchCursor == 1 &&
		       observerDiagnostics.publishAttempts == 6 &&
		       observerDiagnostics.preparationAttempts == 3 &&
		       observerDiagnostics.physicalMessagesPublished == 3 &&
		       observerDiagnostics.publicationFailures == 4,
		       "a partial transfer cannot be split across runtime message buses" );

		NotifyJa2TacticalWorldUnloaded();
		UpdateJa2TacticalWorldObserverAtSafeFrame( liveRuntimeMessages );
		observerDiagnostics = GetJa2TacticalWorldObserverDiagnostics();
		const PackageSaveStateCaptureResult packageSaveAfterObservation =
			compiledContext.capturePackageSaveState();
		const std::vector<RecordedSimulationCommand> replayAfterObservation =
			compiledContext.commandJournal().snapshot();
		CHECK( !IsJa2TacticalWorldLoaded() &&
		       observerDiagnostics.lastUpdate ==
		           TacticalWorldObserverUpdateResult::SourceUnavailable &&
		       observerDiagnostics.publicationSerial == 0 &&
		       tacticalWorldObserver && !tacticalWorldObserver.service->latest() &&
		       observerDiagnostics.safeFrameUpdates ==
		           observerBeforeWorld.safeFrameUpdates + 12 &&
		       observerDiagnostics.bridgeResult ==
		           Ja2TacticalWorldDeltaBridgeResult::WorldUnavailableReset &&
		       observerDiagnostics.lastPublishError == TacticalWorldDeltaPublishError::None &&
		       observerDiagnostics.pendingDeltaSerial == 0 &&
		       observerDiagnostics.pendingTransferId == 0 &&
		       observerDiagnostics.nextTransferId == 4 &&
		       observerDiagnostics.pendingBatchMessages == 0 &&
		       observerDiagnostics.pendingBatchCursor == 0 &&
		       observerDiagnostics.handledDeltaSerial == 0 &&
		       observerDiagnostics.publishedDeltaSerial == 0 &&
		       observerDiagnostics.worldGeneration == 0 &&
		       observerDiagnostics.turnSerial == 0 &&
		       observerDiagnostics.worldTransitions == 2 &&
		       observerDiagnostics.observerResets == 1 &&
		       observerDiagnostics.discardedPendingDeltas == 1 &&
		       observerDiagnostics.publishAttempts == 6 &&
		       observerDiagnostics.preparationAttempts == 3 &&
		       observerDiagnostics.chunkedDeltasPrepared == 1 &&
		       observerDiagnostics.physicalMessagesPublished == 3 &&
		       observerDiagnostics.publishedMessages == 2 &&
		       observerDiagnostics.publicationFailures == 4 &&
		       packageSaveAfterObservation.error == packageSaveBeforeObservation.error &&
		       packageSaveAfterObservation.packageId == packageSaveBeforeObservation.packageId &&
		       packageSaveAfterObservation.snapshot.records.size() ==
		           packageSaveBeforeObservation.snapshot.records.size() &&
		       replayAfterObservation.size() == replayBeforeObservation.size() &&
		       compiledContext.commandJournal().droppedCount() ==
		           replayDroppedBeforeObservation &&
		       soldierRepository.resolve( 0 ) == &worldActor &&
		       worldActor.ubID == SoldierID{ static_cast<UINT16>( 0 ) } &&
		       worldActor.uiUniqueSoldierIdValue == 701 && worldActor.position().gridNo() == 348 &&
		       worldActor.vitals().health() == 75,
		       "world unload invalidates publication and stale retry without mutating legacy state" );
		(void)ReleaseJa2TacticalEntity( worldActor );
		SOLDIERTYPE* const restoredWorldActor =
			soldierRepository.replace( 0, previousWorldActor );
		if ( previousWorldEntity.valid() && restoredWorldActor )
			(void)AdoptJa2TacticalEntity( *restoredWorldActor );
		RestoreJa2TacticalWorldSession( previousWorldSession );

		RuntimeMessageBus tacticalDeltaMessages( 1, 256 );
		HeadlessRuntimeMessageSink tacticalDeltaSink;
		TacticalWorldDeltaPublisher tacticalDeltaPublisher(
			tacticalDeltaMessages, TacticalWorldDeltaPublishLimits{ 1, 256 } );
		TacticalWorldDelta headlessDelta;
		headlessDelta.previousEpoch = 23;
		headlessDelta.currentEpoch = 23;
		headlessDelta.events.push_back( TacticalActorMovedEvent{
			TacticalEntityId{ 0, 701 }, 344, 345, 0, 1, 2, 3 } );
		const bool tacticalDeltaSinkAdded =
			tacticalDeltaMessages.addSink( tacticalDeltaSink ) ==
				RuntimeMessageSinkRegistrationError::None;
		const TacticalWorldDeltaPublishResult headlessDeltaPublished =
			tacticalDeltaPublisher.publish( headlessDelta );
		const RuntimeMessageDispatchResult headlessDeltaDispatch =
			tacticalDeltaMessages.dispatchPending();
		TacticalWorldDelta receivedHeadlessDelta;
		const bool headlessDeltaDecoded = tacticalDeltaSink.messages.size() == 1 &&
			DecodeTacticalWorldDelta(
				tacticalDeltaSink.messages[0].payload, receivedHeadlessDelta ) ==
				TacticalWorldDeltaDecodeResult::Success;
		CHECK( tacticalDeltaSinkAdded && headlessDeltaPublished &&
		       headlessDeltaPublished.sequence == 1 && headlessDeltaDispatch.messages == 1 &&
		       headlessDeltaDispatch.delivered == 1 && headlessDeltaDecoded &&
		       tacticalDeltaSink.messages[0].topic == TacticalWorldDeltaMessageTopic &&
		       tacticalDeltaSink.messages[0].source == TacticalWorldDeltaMessageSource &&
		       receivedHeadlessDelta.events.size() == 1 &&
		       std::get<TacticalActorMovedEvent>( receivedHeadlessDelta.events[0] ).currentGrid == 345,
		       "headless package messaging delivers standalone encoded tactical deltas" );
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
		MemoryRenderSurfaceAccess packageSurfaces;
		MappedRenderCommandSink packageCommands(packageSurfaces);
		MemoryAssetSource packageAssets( "rules.test" );
		EngineServices services{packageTime, packageRandom, packageStorage, logSink, packageInput,
		                        packageAudio, packageFrames, packageAssets,
		                        NullFrameInvalidator::instance(), packageSurfaces,
		                        packageCommands};
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
		       first.observedAssetProvenance == "rules.test" &&
		       first.observedServices &&
		       &first.observedServices->renderSurfaces == &packageSurfaces &&
		       &first.observedServices->renderCommands == &packageCommands,
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

		const RuntimeReportOptions disabledReports =
			ReadRuntimeReportOptions( emptyProperties, 1, emptyArguments );
		vfs::PropertyContainer reportProperties;
		reportProperties.setStringProperty(
			L"Ja2 Settings", L"ENGINE_REPORT_PATH", L"reports/from-ini.json" );
		reportProperties.setStringProperty(
			L"Ja2 Settings", L"ENGINE_REPORT_ON_STARTUP", L"false" );
		std::vector<std::string> reportArguments = {
			"ja2", "--engine-report=reports/from-cli.json" };
		std::vector<char*> reportArgumentPointers;
		for ( std::string& argument : reportArguments )
			reportArgumentPointers.push_back( &argument[0] );
		const RuntimeReportOptions configuredReports = ReadRuntimeReportOptions(
			reportProperties, static_cast<int>( reportArgumentPointers.size() ),
			reportArgumentPointers.data() );
		char disableReport[] = "--no-engine-report";
		char* disableReportArguments[] = { executable, disableReport };
		const RuntimeReportOptions explicitlyDisabledReports = ReadRuntimeReportOptions(
			reportProperties, 2, disableReportArguments );
		ConfigureRuntimeReports( configuredReports );
		CHECK( !disabledReports.enabled && configuredReports.enabled &&
		       configuredReports.path == "reports/from-cli.json" &&
		       !configuredReports.shouldWrite( RuntimeReportMoment::Startup ) &&
		       configuredReports.shouldWrite( RuntimeReportMoment::Shutdown ) &&
		       !explicitlyDisabledReports.enabled &&
		       GetRuntimeReportOptions().path == configuredReports.path,
		       "runtime report options support opt-in INI settings and CLI override/disable" );

	}

	{
		TestLifecyclePackage fallbackCampaign(
			"fixture.default-campaign", PackageKind::Campaign, -1, nullptr, {},
			"1.0", { GameCapability::CampaignArulco } );
		EngineRuntime<unsigned> runtime;
		PackageHost host;
		RecordingPackageAssetMounter mounter;
		PackageStartupOptions disabled;
		const bool registered =
			runtime.packages().registerPackage( fallbackCampaign ) ==
				PackageRegistrationError::None;
		const PackageHostResult selected = host.initialize(
			runtime.packages(), disabled, mounter,
			fallbackCampaign.descriptor().content.id );
		CHECK( registered && selected &&
		       selected.activated == std::vector<std::string>({
		           "fixture.default-campaign" }) &&
		       runtime.packages().activeCampaign() == "fixture.default-campaign" &&
		       !host.attempted() && mounter.preflighted.empty() &&
		       mounter.mounted.empty(),
		       "disabled discovery still selects the registered built-in campaign fallback" );
		runtime.packages().deactivate( "fixture.default-campaign" );
	}

	{
		ScopedPackageFixture fixture;
		TestLifecyclePackage fallbackCampaign(
			"fixture.default-campaign", PackageKind::Campaign, -1, nullptr, {},
			"1.0", { GameCapability::CampaignArulco } );
		EngineRuntime<unsigned> runtime;
		PackageHost host;
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady =
			AddPackageFixture( fixture, "extension", "fixture.with-default" ) &&
			runtime.packages().registerPackage( fallbackCampaign ) ==
				PackageRegistrationError::None;
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.with-default" };
		PackageHostResult result;
		if ( fixtureReady )
			result = host.initialize(
				runtime.packages(), options, mounter,
				fallbackCampaign.descriptor().content.id );
		CHECK( fixtureReady && result &&
		       result.activated == std::vector<std::string>({
		           "fixture.default-campaign", "fixture.with-default" }) &&
		       runtime.packages().activeCampaign() == "fixture.default-campaign" &&
		       mounter.preflighted == std::vector<std::string>({
		           "fixture.with-default" }) &&
		       mounter.mounted == mounter.preflighted,
		       "an extension-only selection composes over the deferred built-in campaign" );
		const PackageHostShutdownResult shutdown =
			host.shutdown( runtime.packages(), mounter );
		CHECK( shutdown && runtime.packages().find( "fixture.with-default" ) == nullptr &&
		       runtime.packages().find( "fixture.default-campaign" ) != nullptr &&
		       runtime.packages().activeCampaign().empty(),
		       "package-host teardown removes external packages but preserves the registered fallback" );
	}

	{
		ScopedPackageFixture fixture;
		TestGameplayBootstrapHooks hooks;
		LegacyRulesPackage rulesPackage( GameCapabilities{}, hooks );
		PackageHost host( &hooks );
		TestLifecyclePackage campaignSupport(
			"fixture.campaign-support", PackageKind::Rules, -1, nullptr, {},
			"1.0.0" );
		TestLifecyclePackage fallbackCampaign(
			"fixture.default-campaign", PackageKind::Campaign, -1, nullptr, {},
			"1.0", { GameCapability::CampaignArulco } );
		std::vector<std::string> lifecycleTrace;
		hooks.trace = &lifecycleTrace;
		campaignSupport.lifecycleTrace = &lifecycleTrace;
		EngineHostOptions hostOptions;
		hostOptions.hostCapabilities.add( GameCapability::HostCampaignArulco );
		EngineRuntime<unsigned> runtime( hostOptions );
		RecordingPackageAssetMounter mounter;
		const std::string manifest =
			"[Package]\n"
			"MANIFEST_VERSION = 4\n"
			"ID = fixture.total-conversion\n"
			"VERSION = 1.0.0\n"
			"CONTENT_API = 1.5\n"
			"TYPE = campaign\n"
			"CAMPAIGN_FAMILY = ja2\n"
			"ASSET_ROOT = Data\n"
			"REQUIRES = fixture.campaign-support@1.0.0\n"
			"LOCALIZATION = en@Localization/campaign.lang\n";
		const bool fixtureReady =
			fixture.write( "campaign/package.ini", manifest ) &&
			fixture.write( "campaign/Data/Localization/campaign.lang",
				"JA2-LOCALIZATION 1\ncampaign.name = Total Conversion\n" ) &&
			runtime.packages().registerPackage( rulesPackage ) ==
				PackageRegistrationError::None &&
			runtime.packages().registerPackage( campaignSupport ) ==
				PackageRegistrationError::None &&
			runtime.packages().registerPackage( fallbackCampaign ) ==
				PackageRegistrationError::None;
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.total-conversion" };
		PackageHostResult result;
		if ( fixtureReady )
			result = host.initialize(
				runtime.packages(), options, mounter,
				fallbackCampaign.descriptor().content.id );
		const RuntimeSessionAdvanceResult started =
			result ? runtime.runtimeSession().advancePackagesTo(
				PackageBootstrapPhase::StartRuntime ) : RuntimeSessionAdvanceResult{};
		const PackageCatalogSnapshot campaignCatalog = runtime.packageCatalog();
		const PackageCatalogEntry* campaign =
			campaignCatalog.find( "fixture.total-conversion" );
		const LocalizedTextView campaignName =
			runtime.localization().resolve( "en", "campaign.name" );
		CHECK( fixtureReady && result && started &&
		       result.activated == std::vector<std::string>({
		           GamePackage::Rules113, "fixture.campaign-support",
		           "fixture.total-conversion" }) &&
		       runtime.packages().activeCampaign() == "fixture.total-conversion" &&
		       !fallbackCampaign.active() && rulesPackage.active() &&
		       mounter.preflighted == std::vector<std::string>({
		           "fixture.total-conversion" }) &&
		       mounter.mounted == mounter.preflighted && campaign &&
		       campaign->descriptor.kind == PackageKind::Campaign &&
		       campaign->descriptor.content.requiredApi.minor == 5 &&
		       campaign->descriptor.content.requirements.size() == 2 &&
		       campaign->descriptor.content.requirements[0].id ==
		           GamePackage::Rules113 &&
		       campaign->descriptor.content.requirements[0].exactVersion ==
		           GamePackage::Rules113Version &&
		       campaign->descriptor.content.requirements[1].id ==
		           "fixture.campaign-support" &&
		       campaign->descriptor.content.requirements[1].exactVersion ==
		           "1.0.0" &&
		       campaign->descriptor.capabilities == std::vector<std::string>({
		           GameCapability::CampaignArulco }) &&
		       campaign->descriptor.requiredCapabilities == std::vector<std::string>({
		           GameCapability::HostCampaignArulco }) &&
		       hooks.contentCampaigns == std::vector<GameCampaign>({
		           GameCampaign::Arulco }) &&
		       hooks.runtimeCampaigns == std::vector<GameCampaign>({
		           GameCampaign::Arulco }) &&
		       lifecycleTrace == std::vector<std::string>({
		           "bootstrap:fixture.campaign-support",
		           "campaign:load-content:ja2",
		           "bootstrap:fixture.campaign-support",
		           "bootstrap:fixture.campaign-support",
		           "campaign:start-runtime:ja2" }) &&
		       campaignName && *campaignName.text == "Total Conversion",
		       "Data Package v4 selects and boots a first-class external campaign" );
		runtime.packageLifecycle().shutdown();
		const PackageHostShutdownResult shutdown =
			host.shutdown( runtime.packages(), mounter );
		CHECK( shutdown && runtime.packages().find( "fixture.total-conversion" ) == nullptr &&
		       runtime.packages().find( "fixture.default-campaign" ) != nullptr &&
		       runtime.localization().size() == 0,
		       "external campaign teardown preserves fallback ownership and removes declared content" );
	}

	{
		ScopedPackageFixture fixture;
		TestGameplayBootstrapHooks hooks;
		LegacyRulesPackage rulesPackage( GameCapabilities{}, hooks );
		PackageHost host( &hooks );
		EngineHostOptions hostOptions;
		hostOptions.hostCapabilities.add( GameCapability::HostCampaignArulco );
		EngineRuntime<unsigned> runtime( hostOptions );
		RecordingPackageAssetMounter mounter;
		const std::string manifest =
			"[Package]\n"
			"MANIFEST_VERSION = 4\n"
			"ID = fixture.wrong-family\n"
			"VERSION = 1.0.0\n"
			"CONTENT_API = 1.5\n"
			"TYPE = campaign\n"
			"CAMPAIGN_FAMILY = unfinished-business\n"
			"ASSET_ROOT = Data\n";
		const bool fixtureReady =
			fixture.write( "wrong-family/package.ini", manifest ) &&
			fixture.makeDirectory( "wrong-family/Data" ) &&
			runtime.packages().registerPackage( rulesPackage ) ==
				PackageRegistrationError::None;
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.wrong-family" };
		PackageHostResult result;
		if ( fixtureReady )
			result = host.initialize( runtime.packages(), options, mounter );
		const RuntimeSessionAdvanceResult configured =
			result ? runtime.runtimeSession().advancePackagesTo(
				PackageBootstrapPhase::Configure ) : RuntimeSessionAdvanceResult{};
		CHECK( fixtureReady && result && !configured &&
		       configured.error == RuntimeSessionError::PackageBootstrapFailed &&
		       hooks.contentCampaigns.empty() && hooks.runtimeCampaigns.empty() &&
		       runtime.packages().completedBootstrapPhases() == 0,
		       "a campaign for another binary family fails before legacy bootstrap" );
		host.shutdown( runtime.packages(), mounter );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady =
			fixture.makeDirectory( "host-impersonation/Data" ) &&
			fixture.write( "host-impersonation/package.ini",
				"[Package]\n"
				"MANIFEST_VERSION = 2\n"
				"ID = fixture.host-impersonation\n"
				"VERSION = 1.0.0\n"
				"CONTENT_API = 1.3\n"
				"TYPE = extension\n"
				"ASSET_ROOT = Data\n"
				"CAPABILITIES = host.campaign-family.unfinished-business\n" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.host-impersonation" };
		PackageHostResult result;
		if ( fixtureReady )
			result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::InvalidManifest &&
		       result.packageId == "fixture.host-impersonation" &&
		       runtime.packageCatalog().packages.empty() &&
		       mounter.preflighted.empty() && mounter.mounted.empty(),
		       "data packages cannot impersonate host-owned capability namespaces" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady =
			fixture.makeDirectory( "reserved-rules-policy/Data" ) &&
			fixture.write( "reserved-rules-policy/package.ini",
				"[Package]\n"
				"MANIFEST_VERSION = 4\n"
				"ID = fixture.reserved-rules-policy\n"
				"VERSION = 1.0.0\n"
				"CONTENT_API = 1.5\n"
				"TYPE = campaign\n"
				"CAMPAIGN_FAMILY = ja2\n"
				"ASSET_ROOT = Data\n"
				"CONFLICTS = ja2.1.13\n" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.reserved-rules-policy" };
		PackageHostResult result;
		if ( fixtureReady )
			result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::InvalidManifest &&
		       result.packageId == "fixture.reserved-rules-policy" &&
		       runtime.packageCatalog().packages.empty(),
		       "campaign dependency policy cannot contradict its host-managed rules layer" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		std::string requirements;
		for ( std::size_t index = 0; index < 128; ++index )
		{
			if ( !requirements.empty() ) requirements += ", ";
			requirements += "fixture.dependency-" + std::to_string( index );
		}
		const bool fixtureReady =
			fixture.makeDirectory( "campaign-relationship-limit/Data" ) &&
			fixture.write( "campaign-relationship-limit/package.ini",
				"[Package]\n"
				"MANIFEST_VERSION = 4\n"
				"ID = fixture.campaign-relationship-limit\n"
				"VERSION = 1.0.0\n"
				"CONTENT_API = 1.5\n"
				"TYPE = campaign\n"
				"CAMPAIGN_FAMILY = ja2\n"
				"ASSET_ROOT = Data\n"
				"REQUIRES = " + requirements + "\n" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.campaign-relationship-limit" };
		PackageHostResult result;
		if ( fixtureReady )
			result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::InvalidManifest &&
		       result.packageId == "fixture.campaign-relationship-limit" &&
		       runtime.packageCatalog().packages.empty(),
		       "campaign manifests reserve one bounded relationship for the compiled rules layer" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady =
			fixture.makeDirectory( "old-campaign/Data" ) &&
			fixture.write( "old-campaign/package.ini",
				"[Package]\n"
				"MANIFEST_VERSION = 3\n"
				"ID = fixture.old-campaign\n"
				"VERSION = 1.0.0\n"
				"CONTENT_API = 1.4\n"
				"TYPE = campaign\n"
				"CAMPAIGN_FAMILY = ja2\n"
				"ASSET_ROOT = Data\n" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.old-campaign" };
		PackageHostResult result;
		if ( fixtureReady )
			result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::InvalidManifest &&
		       result.packageId == "fixture.old-campaign" &&
		       runtime.packageCatalog().packages.empty(),
		       "older manifests cannot silently opt into selectable campaigns" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		TestLifecyclePackage rulesPackage(
			GamePackage::Rules113, PackageKind::Rules, -1, nullptr, {},
			GamePackage::Rules113Version, { GameCapability::Rules113 } );
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady =
			fixture.makeDirectory( "campaign-without-host/Data" ) &&
			fixture.write( "campaign-without-host/package.ini",
				"[Package]\n"
				"MANIFEST_VERSION = 4\n"
				"ID = fixture.campaign-without-host\n"
				"VERSION = 1.0.0\n"
				"CONTENT_API = 1.5\n"
				"TYPE = campaign\n"
				"CAMPAIGN_FAMILY = ja2\n"
				"ASSET_ROOT = Data\n" ) &&
			runtime.packages().registerPackage( rulesPackage ) ==
				PackageRegistrationError::None;
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.campaign-without-host" };
		PackageHostResult result;
		if ( fixtureReady )
			result = host.initialize( runtime.packages(), options, mounter );
		CHECK( fixtureReady && result.error == PackageHostError::ActivationFailed &&
		       result.packageId == "fixture.campaign-without-host" &&
		       mounter.preflighted.empty() && mounter.mounted.empty() &&
		       runtime.packages().find( "fixture.campaign-without-host" ) == nullptr &&
		       runtime.packages().find( GamePackage::Rules113 ) != nullptr &&
		       !rulesPackage.active(),
		       "a generic data host rejects campaigns before activation without a runtime adapter" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const std::string manifest =
			"[Package]\n"
			"MANIFEST_VERSION = 3\n"
			"ID = fixture.localized\n"
			"VERSION = 1.0.0\n"
			"CONTENT_API = 1.4\n"
			"TYPE = extension\n"
			"ASSET_ROOT = Data\n"
			"LOCALIZATION = en@Localization/en.lang, nl@Localization/nl.lang\n"
			"DEFINITIONS = item:field-kit@2=Definitions/field-kit.json\n";
		const bool fixtureReady =
			fixture.write( "localized/package.ini", manifest ) &&
			fixture.write( "localized/Data/Localization/en.lang",
			               "JA2-LOCALIZATION 1\nui.package-ready = Package ready\n" ) &&
			fixture.write( "localized/Data/Localization/nl.lang",
			               "JA2-LOCALIZATION 1\nui.package-ready = Pakket gereed\n" ) &&
			fixture.write( "localized/Data/Definitions/field-kit.json",
			               "{\"healing\": 25}" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.localized" };
		PackageHostResult initialized;
		if ( fixtureReady ) initialized = host.initialize( runtime.packages(), options, mounter );
		const RuntimeSessionAdvanceResult loaded =
			initialized ? runtime.runtimeSession().advancePackagesTo(
				PackageBootstrapPhase::LoadContent ) : RuntimeSessionAdvanceResult{};
		const LocalizedTextView localized =
			runtime.localization().resolve( "nl", "ui.package-ready" );
		const PackageCatalogSnapshot catalog = runtime.packageCatalog();
		const PackageCatalogEntry* package = catalog.find( "fixture.localized" );
		const DefinitionView definition =
			runtime.definitions().resolve( "item", "field-kit", 2, 2 );
		CHECK( fixtureReady && initialized && loaded && localized &&
		       *localized.text == "Pakket gereed" &&
		       *localized.packageId == "fixture.localized" && package &&
		       package->descriptor.localizationSources.size() == 2 &&
		       package->descriptor.definitionSources.size() == 1 && definition &&
		       std::string( definition.payload->begin(), definition.payload->end() ) ==
		           "{\"healing\": 25}" &&
		       package->descriptor.content.requiredApi.minor == 4,
		       "Data Package v3 imports declared localization and definition assets" );
		runtime.packageLifecycle().shutdown();
		CHECK( runtime.localization().size() == 0 && runtime.definitions().size() == 0,
		       "package content imports are removed during lifecycle teardown" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const std::string manifest =
			"[Package]\n"
			"MANIFEST_VERSION = 3\n"
			"ID = fixture.bad-localization\n"
			"VERSION = 1.0.0\n"
			"CONTENT_API = 1.4\n"
			"TYPE = extension\n"
			"ASSET_ROOT = Data\n"
			"LOCALIZATION = en@Localization/en.lang\n";
		const bool fixtureReady =
			fixture.write( "bad-localization/package.ini", manifest ) &&
			fixture.write( "bad-localization/Data/Localization/en.lang",
			               "JA2-LOCALIZATION 1\nui.same = First\nui.same = Second\n" );
		PackageStartupOptions options;
		options.enabled = true;
		options.roots = { fixture.root() };
		options.selected = { "fixture.bad-localization" };
		PackageHostResult initialized;
		if ( fixtureReady ) initialized = host.initialize( runtime.packages(), options, mounter );
		const RuntimeSessionAdvanceResult loaded =
			initialized ? runtime.runtimeSession().advancePackagesTo(
				PackageBootstrapPhase::LoadContent ) : RuntimeSessionAdvanceResult{};
		CHECK( fixtureReady && initialized && !loaded &&
		       loaded.error == RuntimeSessionError::PackageBootstrapFailed &&
		       runtime.localization().size() == 0 &&
		       runtime.packages().completedBootstrapPhases() == 0,
		       "invalid package localization rolls back the complete bootstrap transaction" );
		runtime.packageLifecycle().shutdown();
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
		const PackageHostShutdownResult shutdown =
			host.shutdown( runtime.packages(), mounter );
		const PackageHostShutdownResult repeatedShutdown =
			host.shutdown( runtime.packages(), mounter );
		CHECK( shutdown && shutdown.unmounted == 2 && shutdown.deactivated == 2 &&
		       shutdown.unregistered == 2 &&
		       mounter.unmounted == std::vector<std::string>({
		       "fixture.consumer", "fixture.base" }) &&
		       runtime.packages().activationOrder().empty() &&
		       runtime.packageCatalog().packages.empty() && !host.attempted() &&
		       repeatedShutdown && repeatedShutdown.unmounted == 0 &&
		       repeatedShutdown.deactivated == 0 &&
		       repeatedShutdown.unregistered == 0,
		       "package host shutdown reverses mounts and ownership exactly once" );
		const PackageHostResult restarted =
			host.initialize( runtime.packages(), options, mounter );
		const PackageHostShutdownResult restartedShutdown =
			host.shutdown( runtime.packages(), mounter );
		CHECK( restarted && restarted.activated == result.activated &&
		       restartedShutdown && !host.attempted() &&
		       runtime.packageCatalog().packages.empty(),
		       "a fully stopped package host can start a fresh runtime session" );
	}

	{
		ScopedPackageFixture fixture;
		PackageHost host;
		EngineRuntime<unsigned> runtime;
		RecordingPackageAssetMounter mounter;
		const bool fixtureReady =
			AddPackageFixture( fixture, "a-base", "fixture.policy-base" ) &&
			fixture.makeDirectory( "b-peer/Data" ) &&
			fixture.write( "b-peer/package.ini", PackageFixtureManifest(
				"fixture.policy-peer", {}, "Data", {}, {}, {}, "feature.peer" ) ) &&
			fixture.makeDirectory( "c-consumer/Data" ) &&
			fixture.write( "c-consumer/package.ini", PackageFixtureManifest(
				"fixture.policy-consumer", {}, "Data", "fixture.policy-base@1.0.0",
				{}, "fixture.policy-peer", "feature.dynamic-weather, feature.new-ai",
				"feature.peer" ) );
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
		CHECK( runtime.hasCapability( "feature.peer" ) &&
		       runtime.hasCapability( "feature.dynamic-weather" ) &&
		       runtime.hasCapability( "feature.new-ai" ) &&
		       runtime.runtimeCapabilities().ids() == std::vector<std::string>({
		       "feature.peer", "feature.dynamic-weather", "feature.new-ai" }),
		       "active data packages contribute portable runtime capabilities" );
		const PackageCatalogSnapshot policyCatalog = runtime.packageCatalog();
		const PackageCatalogEntry* consumer =
			policyCatalog.find( "fixture.policy-consumer" );
		CHECK( consumer && consumer->descriptor.requiredCapabilities ==
		           std::vector<std::string>({ "feature.peer" }),
		       "Data Package v2 retains required capability contracts for bootstrap preflight" );
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
		mounter.failPreflightId.clear();
		const PackageHostResult retry =
			host.initialize( runtime.packages(), options, mounter );
		const PackageHostShutdownResult retryShutdown =
			host.shutdown( runtime.packages(), mounter );
		CHECK( retry && retryShutdown && !host.attempted() &&
		       runtime.packageCatalog().packages.empty(),
		       "a fully rolled-back package startup failure is immediately retryable" );
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
		const UINT32 savedClock = guiBaseJA2Clock;
		const UINT32 savedNoPauseClock = guiBaseJA2NoPauseClock;
		const BOOLEAN savedHiSpeed = IsHiSpeedClockMode();
		const BOOLEAN savedFastForward = IsFastForwardModeEnabled();
		const BOOLEAN savedPaused = IsJA2ClockPaused();
		const UINT32 savedFastForwardPeriod = GetFastForwardPeriod();
		const INT32 savedFastForwardKey = GetFastForwardKey();
		const FLOAT savedClockSpeed = GetClockSpeedPercent();
		const std::vector<INT32> savedTimerCounters(
			giTimerCounters, giTimerCounters + NUMTIMERS );

		gInjectedLegacyClockTime = 1000000;
		gInjectedFastForwardKeyDown = FALSE;
		CHECK( SetJA2ClockTestTimeSource( InjectedLegacyClockTime ) &&
		       SetJA2ClockTestKeyStateSource( InjectedLegacyClockKeyState ),
		       "legacy clock accepts deterministic sources before initialization" );
		SetFastForwardKey( 0 );
		SetFastForwardPeriod( 500 );
		SetHiSpeedClockMode( FALSE );
		SetFastForwardMode( FALSE );
		SetClockSpeedPercent( 100 );
		PauseTime( FALSE );
		guiBaseJA2Clock = 0;
		guiBaseJA2NoPauseClock = 0;
		const UINT64 blockingTransitionStart = GetJA2MonotonicMilliseconds();
		gInjectedLegacyClockTime += 250000;
		CHECK( GetJA2MonotonicMilliseconds() == blockingTransitionStart + 250 &&
		       guiBaseJA2Clock == 0 && guiBaseJA2NoPauseClock == 0,
		       "blocking transition time advances without pumping legacy game state" );
		gInjectedLegacyClockTime = 1000000;
		CHECK( InitializeJA2Clock() && IsJA2TimerThread(),
		       "legacy clock assigns timer ownership to its initializing game thread" );

		const UINT64 normalStart = gInjectedLegacyClockTime;
		CHECK( PumpJA2ClockAt( normalStart ) == 0 &&
		       PumpJA2ClockAt( normalStart + 1 ) == 1 &&
		       PumpJA2ClockAt( normalStart + 9999 ) == 0 &&
		       PumpJA2ClockAt( normalStart + 10001 ) == 1 &&
		       guiBaseJA2Clock == 20 && guiBaseJA2NoPauseClock == 20,
		       "normal non-high-speed clock preserves its ten-millisecond cadence" );

		gInjectedLegacyClockTime = normalStart + 25001;
		PauseTime( TRUE );
		const INT32 counterWhilePaused = giTimerCounters[ ANIMATETILES ];
		CHECK( IsJA2ClockPaused() && guiBaseJA2Clock == 30 &&
		       guiBaseJA2NoPauseClock == 30 && counterWhilePaused == 170,
		       "pausing settles elapsed work under the preceding unpaused segment" );
		gInjectedLegacyClockTime = normalStart + 35002;
		PumpJA2Clock();
		CHECK( guiBaseJA2Clock == 30 && guiBaseJA2NoPauseClock == 40 &&
		       giTimerCounters[ ANIMATETILES ] == counterWhilePaused,
		       "a paused schedule advances only the no-pause clock" );
		gInjectedLegacyClockTime = normalStart + 45002;
		PauseTime( FALSE );
		CHECK( !IsJA2ClockPaused() && guiBaseJA2Clock == 30 &&
		       guiBaseJA2NoPauseClock == 50 &&
		       giTimerCounters[ ANIMATETILES ] == counterWhilePaused,
		       "unpausing settles elapsed work under the preceding paused segment" );
		gInjectedLegacyClockTime = normalStart + 55003;
		PumpJA2Clock();
		CHECK( guiBaseJA2Clock == 40 && guiBaseJA2NoPauseClock == 60 &&
		       giTimerCounters[ ANIMATETILES ] == counterWhilePaused - 10,
		       "the new unpaused schedule starts from its transition timestamp" );

		gInjectedLegacyClockTime = normalStart + 70003;
		SetFastForwardMode( TRUE );
		CHECK( guiBaseJA2Clock == 50 && guiBaseJA2NoPauseClock == 70,
		       "fast-forward activation drains only the old normal-speed segment" );
		gInjectedLegacyClockTime = normalStart + 75004;
		CHECK( PumpJA2ClockAt( gInjectedLegacyClockTime ) == 5 &&
		       guiBaseJA2Clock == 100 && guiBaseJA2NoPauseClock == 120,
		       "non-high-speed fast-forward retains its one-millisecond cadence" );

		gInjectedLegacyClockTime = normalStart + 80004;
		SetHiSpeedClockMode( TRUE );
		CHECK( guiBaseJA2Clock == 150 && guiBaseJA2NoPauseClock == 170,
		       "high-speed transition drains the preceding fast-forward schedule" );
		gInjectedLegacyClockTime = normalStart + 85005;
		SetFastForwardPeriod( 2000 );
		CHECK( guiBaseJA2Clock == 200 && guiBaseJA2NoPauseClock == 220,
		       "period changes settle fast-forward work using the old period" );
		gInjectedLegacyClockTime = normalStart + 91006;
		CHECK( PumpJA2ClockAt( gInjectedLegacyClockTime ) == 3 &&
		       guiBaseJA2Clock == 230 && guiBaseJA2NoPauseClock == 250,
		       "high-speed fast-forward honors configured periods above its floor" );

		SetFastForwardMode( FALSE );
		gInjectedLegacyClockTime = normalStart + 106007;
		SetClockSpeedPercent( 200 );
		CHECK( guiBaseJA2Clock == 240 && guiBaseJA2NoPauseClock == 260,
		       "clock-speed changes drain elapsed work using the old speed" );
		gInjectedLegacyClockTime = normalStart + 116008;
		CHECK( PumpJA2ClockAt( gInjectedLegacyClockTime ) == 2 &&
		       guiBaseJA2Clock == 260 && guiBaseJA2NoPauseClock == 280,
		       "the new high-speed clock percentage owns its fresh segment" );
		SetClockSpeedPercent( 150 );
		gInjectedLegacyClockTime = normalStart + 128000;
		SetHiSpeedClockMode( FALSE );
		CHECK( guiBaseJA2Clock == 270 && guiBaseJA2NoPauseClock == 290,
		       "high-speed shutdown settles its microsecond-period segment first" );
		gInjectedLegacyClockTime = normalStart + 140001;
		CHECK( PumpJA2ClockAt( gInjectedLegacyClockTime ) == 2 &&
		       guiBaseJA2Clock == 290 && guiBaseJA2NoPauseClock == 310,
		       "normal mode resumes with its legacy millisecond-truncated period" );

		SetFastForwardKey( 42 );
		gInjectedFastForwardKeyDown = TRUE;
		gInjectedLegacyClockTime = normalStart + 147002;
		CHECK( PumpJA2ClockAt( gInjectedLegacyClockTime ) == 1 &&
		       guiBaseJA2Clock == 300 && guiBaseJA2NoPauseClock == 320,
		       "a key-driven fast-forward edge first drains the old normal segment" );
		gInjectedLegacyClockTime = normalStart + 152003;
		CHECK( PumpJA2ClockAt( gInjectedLegacyClockTime ) == 5 &&
		       guiBaseJA2Clock == 350 && guiBaseJA2NoPauseClock == 370,
		       "key-driven fast-forward owns the following schedule segment" );
		gInjectedFastForwardKeyDown = FALSE;
		gInjectedLegacyClockTime = normalStart + 155004;
		CHECK( PumpJA2ClockAt( gInjectedLegacyClockTime ) == 3 &&
		       guiBaseJA2Clock == 380 && guiBaseJA2NoPauseClock == 400,
		       "a key release drains old fast-forward debt before returning to normal" );
		gInjectedLegacyClockTime = normalStart + 161005;
		CHECK( PumpJA2ClockAt( gInjectedLegacyClockTime ) == 1 &&
		       guiBaseJA2Clock == 390 && guiBaseJA2NoPauseClock == 410,
		       "key release anchors a fresh normal-speed segment" );

		SetFastForwardMode( TRUE );
		const UINT32 beforeBoundedCatchUp = guiBaseJA2Clock;
		const UINT32 beforeBoundedCatchUpNoPause = guiBaseJA2NoPauseClock;
		gInjectedLegacyClockTime += 250001;
		PauseTime( TRUE );
		CHECK( guiBaseJA2Clock == beforeBoundedCatchUp + 1000 &&
		       guiBaseJA2NoPauseClock == beforeBoundedCatchUpNoPause + 1000 &&
		       PumpJA2ClockAt( gInjectedLegacyClockTime ) == 100 &&
		       PumpJA2ClockAt( gInjectedLegacyClockTime ) == 50 &&
		       guiBaseJA2Clock == beforeBoundedCatchUp + 2500 &&
		       guiBaseJA2NoPauseClock == beforeBoundedCatchUpNoPause + 2500,
		       "capped pre-transition work retains its old unpaused schedule segment" );
		SetFastForwardMode( FALSE );
		PauseTime( FALSE );

		const UINT32 beforeForwardDiscontinuityClock = guiBaseJA2Clock;
		const UINT32 beforeForwardDiscontinuityNoPause = guiBaseJA2NoPauseClock;
		gInjectedLegacyClockTime += 1000001;
		CHECK( PumpJA2ClockAt( gInjectedLegacyClockTime ) == 1 &&
		       PumpJA2ClockAt( gInjectedLegacyClockTime ) == 0 &&
		       guiBaseJA2Clock == beforeForwardDiscontinuityClock + 10 &&
		       guiBaseJA2NoPauseClock == beforeForwardDiscontinuityNoPause + 10,
		       "a forward discontinuity retains at most one old-state step" );
		const UINT32 afterForwardDiscontinuityClock = guiBaseJA2Clock;
		const UINT32 afterForwardDiscontinuityNoPause = guiBaseJA2NoPauseClock;
		gInjectedLegacyClockTime += 6001;
		CHECK( PumpJA2ClockAt( gInjectedLegacyClockTime ) == 1 &&
		       guiBaseJA2Clock == afterForwardDiscontinuityClock + 10 &&
		       guiBaseJA2NoPauseClock == afterForwardDiscontinuityNoPause + 10,
		       "forward discontinuity re-anchors the current configuration at now" );

		const UINT32 beforeBackwardClock = guiBaseJA2Clock;
		const UINT32 beforeBackwardNoPause = guiBaseJA2NoPauseClock;
		gInjectedLegacyClockTime -= 500000;
		const UINT64 backwardAnchor = gInjectedLegacyClockTime;
		CHECK( PumpJA2ClockAt( backwardAnchor ) == 0 &&
		       PumpJA2ClockAt( backwardAnchor + 6001 ) == 1 &&
		       guiBaseJA2Clock == beforeBackwardClock + 10 &&
		       guiBaseJA2NoPauseClock == beforeBackwardNoPause + 10,
		       "a backward time discontinuity rebases without unsigned clock debt" );
		gInjectedLegacyClockTime = backwardAnchor + 6001;

		giTimerCounters[ ANIMATETILES ] = 1;
		CHECK( InitializeJA2Clock() && IsJA2TimerThread() &&
		       giTimerCounters[ ANIMATETILES ] == giTimerIntervals[ ANIMATETILES ] &&
		       PumpJA2ClockAt( gInjectedLegacyClockTime ) == 0 &&
		       PumpJA2ClockAt( gInjectedLegacyClockTime + 1 ) == 1,
		       "owner-thread reinitialization starts a clean immediate schedule" );
		gInjectedLegacyClockTime += 1;

		BOOLEAN workerClaimsOwnership = TRUE;
		UINT32 workerSteps = 1;
		std::thread worker( [&] {
			workerClaimsOwnership = IsJA2TimerThread();
			PauseTime( TRUE );
			SetFastForwardMode( TRUE );
			ShutdownJA2Clock();
			workerSteps = PumpJA2ClockAt( gInjectedLegacyClockTime + 10000 );
		} );
		worker.join();
		CHECK( !workerClaimsOwnership && workerSteps == 0 && IsJA2TimerThread() &&
		       !IsJA2ClockPaused() && !IsFastForwardModeEnabled(),
		       "non-owner threads cannot pump, configure, or shut down the clock" );
		CHECK( !SetJA2ClockTestTimeSource( NULL ) &&
		       !SetJA2ClockTestKeyStateSource( NULL ),
		       "clock test sources remain immutable throughout the owner lifecycle" );

		ShutdownJA2Clock();
		CHECK( !IsJA2TimerThread() &&
		       PumpJA2ClockAt( gInjectedLegacyClockTime + 10001 ) == 0,
		       "clock shutdown rejects further frame pumps" );

		SetFastForwardKey( savedFastForwardKey );
		SetFastForwardPeriod( savedFastForwardPeriod );
		SetClockSpeedPercent( savedClockSpeed );
		SetFastForwardMode( savedFastForward );
		SetHiSpeedClockMode( savedHiSpeed );
		PauseTime( savedPaused );
		guiBaseJA2Clock = savedClock;
		guiBaseJA2NoPauseClock = savedNoPauseClock;
		std::copy( savedTimerCounters.begin(), savedTimerCounters.end(),
		           giTimerCounters );
		gInjectedFastForwardKeyDown = FALSE;
		gInjectedLegacyClockTime = 0;
		CHECK( SetJA2ClockTestTimeSource( NULL ) &&
		       SetJA2ClockTestKeyStateSource( NULL ),
		       "legacy clock test sources restore cleanly after shutdown" );
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
		SOLDIERTYPE records[2];
		SOLDIERTYPE* slots[2] = { nullptr, nullptr };
		Ja2SoldierRepository repository(records, slots, 2);
		CHECK( repository.capacity() == 2 &&
		       repository.resolve(0) == nullptr &&
		       repository.resolve(2) == nullptr,
		       "soldier repository bounds unresolved legacy slots" );

		repository.initializeSlots();
		CHECK( repository.resolve(0) == &records[0] &&
		       repository.resolve(1) == &records[1] &&
		       !records[0].bActive && !records[1].bActive &&
		       repository.contains(0, records[0]) &&
		       !repository.contains(0, records[1]),
		       "soldier repository establishes canonical fixed-slot bindings" );

		Ja2SoldierRepository& productionRepository =
			GetJa2SoldierRepository();
		BindJa2SoldierRepository(repository);
		const bool isolatedRepositoryBound =
			&GetJa2SoldierRepository() == &repository;
		BindJa2SoldierRepository(productionRepository);
		CHECK( isolatedRepositoryBound &&
		       &GetJa2SoldierRepository() == &productionRepository,
		       "soldier repository binding is independent and restores the composed application owner" );

		SOLDIERTYPE source;
		source.ubID = SoldierID{ static_cast<UINT16>( 1 ) };
		source.bActive = TRUE;
		source.position().gridNo() = 4321;
		source.vitals().health() = 73;
		source.runtime.pendingAction.pathSearchSourceGrid = 99;
		SOLDIERTYPE* replaced = repository.replace(1, source);
		CHECK( replaced == &records[1] &&
		       records[1].position().gridNo() == 4321 &&
		       records[1].vitals().health() == 73 &&
		       records[1].runtime.pendingAction.pathSearchSourceGrid == 0 &&
		       repository.replace(2, source) == nullptr,
		       "soldier repository owns bounded whole-record replacement and clone reset semantics" );

		slots[1] = &records[0];
		CHECK( repository.replace(1, source) == nullptr,
		       "soldier repository rejects a noncanonical compatibility slot" );
		repository.initializeSlots();
		records[0].ubID = SoldierID{ static_cast<UINT16>( 0 ) };
		records[0].position().gridNo() = 100;
		records[1].ubID = SoldierID{ static_cast<UINT16>( 1 ) };
		records[1].position().gridNo() = 200;
		const bool swapped = repository.swapRecords(0, 1);
		CHECK( swapped &&
		       records[0].ubID == SoldierID{ static_cast<UINT16>( 0 ) } &&
		       records[1].ubID == SoldierID{ static_cast<UINT16>( 1 ) } &&
		       records[0].position().gridNo() == 200 &&
		       records[1].position().gridNo() == 100 &&
		       !repository.swapRecords(0, 0) &&
		       !repository.swapRecords(0, 2),
		       "soldier repository relocates complete records while preserving slot identities" );
	}

	{
		SOLDIERTYPE soldier;
		SoldierVitalsComponent& vitals = soldier.vitals();
		vitals.maximumHealth() = 90;
		vitals.health() = 75;
		vitals.breath() = 60;
		vitals.maximumBreath() = 95;
		vitals.bleeding() = 8;
		vitals.previousHealth() = 74;
		vitals.fractionalHealth() = 37;
		vitals.breathReduction() = 420;
		vitals.healableInjury() = 1600;
		vitals.beginSurgery();
		vitals.unregainableBreath() = 222;
		vitals.criticalStatDamage()[DAMAGED_STAT_HEALTH] = 4;
		vitals.criticalStatDamage()[DAMAGED_STAT_AGILITY] = 6;
		vitals.nextBleedAt() = 2.5f;
		vitals.regenerationCounter() = -1;
		vitals.regenerationBoostersUsedToday() = 2;
		vitals.lastBleedGruntAt() = 12340;
		SoldierServiceComponent& service = soldier.service();
		service.activity() = 2;
		service.addProvider();
		service.addProvider();
		service.beginProvidingTo( SoldierID{ 7 } );
		service.assignAutoBandagingMedic( SoldierID{ 8 } );
		SoldierDialogueComponent& dialogue = soldier.dialogue();
		dialogue.quoteRecord() = 13;
		dialogue.quoteActionId() = QUOTE_ACTION_ID_CHECKFORDEST;
		dialogue.battleSoundSet() = 4;
		dialogue.markSaid(SOLDIER_QUOTE_SAID_LOW_BREATH);
		dialogue.vocalVolume() = 87;
		dialogue.recordBattleSound(BATTLE_SOUND_ATTN1, 12342);
		dialogue.startHeardNoiseCooldown(5);
		dialogue.markSaidExtended(SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL);
		dialogue.activeBattleSound() = 77;
		dialogue.currentCivilianQuote() = -2;
		dialogue.civilianQuoteDelta() = 2;
		dialogue.recordSpokeAt(12343);
		dialogue.corpseQuoteTolerance() = 3;
		SoldierAudioComponent& audio = soldier.audio();
		audio.recordFootstepVariant(2);
		audio.recordDoorOpeningNoise(17);
		audio.startBurstSound(301);
		audio.startPositionSound(302);
		audio.startTurningSound(303);
		SoldierReplicationComponent& replication = soldier.replication();
		replication.movementStartedAt() = 12344;
		replication.optimumMovementTime() = 12345;
		replication.recordUpdate(12346);
		replication.updateSequence() = 304;
		replication.updateType() = 7;
		replication.scheduleStop(1235);
		replication.recordChecksum(305);
		SoldierMovementMetricsComponent& movementMetrics = soldier.movementMetrics();
		movementMetrics.carriedWeightAtTurnStart() = 135;
		movementMetrics.tilesMoved() = 7;
		movementMetrics.realtimeBreathTiles() = 4;
		movementMetrics.lastRealtimeMovementAnimation() = RUNNING;
		SoldierAiPlanningComponent& aiPlanning = soldier.aiPlanning();
		aiPlanning.flankCount() = 3;
		aiPlanning.flankAnchorGrid() = 1236;
		aiPlanning.raiseSniperPosture();
		aiPlanning.flankOriginDirection() = 5;
		aiPlanning.planIndex() = 4;
		SoldierSkillStateComponent& skillState = soldier.skillState();
		skillState.beginCheck(-7, 1234);
		skillState.recordCheckAttempt();
		skillState.selectedAiSkill() = SKILLS_FOCUS;
		skillState.counter(SOLDIER_COUNTER_RADIO_ARTILLERY) = 3;
		skillState.counter(SOLDIER_COUNTER_SPOTTER) = 4;
		skillState.counter(SOLDIER_COUNTER_ROLE_OBSERVED) = 5;
		skillState.counter(SOLDIER_COUNTER_RETREAT) = 6;
		skillState.counter(SOLDIER_COUNTER_MAX - 1) = 19;
		skillState.cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_SECONDS) = 12344;
		skillState.cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_APS) = 12;
		skillState.cooldown(SOLDIER_COOLDOWN_CRYO) = 2;
		skillState.cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) = 33;
		skillState.cooldown(SOLDIER_COOLDOWN_DRUGUSER_COMBAT) = 4;
		skillState.cooldown(SOLDIER_COOLDOWN_ROBOT_XRAY) = 5;
		skillState.cooldown(SOLDIER_COOLDOWN_MAX - 1) = 190;
		skillState.focusOn(1290);
		SoldierConditionComponent& condition = soldier.condition();
		condition.extraStrength() = 11;
		condition.extraDexterity() = -12;
		condition.extraAgility() = 13;
		condition.extraWisdom() = -14;
		condition.extraExperienceLevel() = 2;
		condition.foodLevel() = 54000;
		condition.drinkLevel() = -24000;
		condition.starvationHealthDamage() = 7;
		condition.starvationStrengthDamage() = 8;
		condition.diseasePoints(0) = 101;
		condition.diseaseFlags(0) = 0x03;
		condition.diseasePoints(6) = -202;
		condition.diseaseFlags(6) = 0x40;
		condition.diseasePoints(NUM_DISEASES - 1) = 303;
		condition.diseaseFlags(NUM_DISEASES - 1) = 0x80;
		condition.addDisability(2);
		condition.addDisability(SoldierConditionComponent::DisabilityBitCount);
		SoldierLongActionComponent& longAction = soldier.longAction();
		longAction.begin(MTA_FORTIFY, 1300, 37);
		SoldierInteractionComponent& interaction = soldier.interaction();
		interaction.nonNpcTraderId() = 7;
		interaction.dragPerson(SoldierID{ 17 });
		interaction.beginChatWith(SoldierID{ 18 });
		SoldierPendingActionComponent& pendingAction = soldier.pendingAction();
		pendingAction.begin(MERC_GIVEITEM);
		pendingAction.animationCount() = 9;
		pendingAction.primaryData() = 1301;
		pendingAction.secondaryData() = -1302;
		pendingAction.tertiaryData() = -3;
		pendingAction.doorHandleCode() = 4;
		pendingAction.quaternaryData() = 1303;
		pendingAction.nextSpecialData() = -1304;
		pendingAction.interruptionMarker() = 5;
		pendingAction.inventorySlot() = -6;
		SoldierActionPointComponent& actionPoints = soldier.actionPoints();
		actionPoints.beginTurn(78);
		actionPoints.current() = 43;
		SoldierCollapseComponent& collapseState = soldier.collapseState();
		collapseState.collapse();
		collapseState.markBreathCollapse();
		collapseState.turns() = 2;
		collapseState.sleepDrugCounter() = 7;
		collapseState.markFatigueCollapse();
		SoldierPerceptionComponent& perception = soldier.perception();
		perception.rememberMovementFrom(2);
		perception.rememberMovementFrom(6);
		perception.viewRange() = 14;
		perception.setBlindness(5);
		perception.heardNoiseLevel() = 1;
		perception.activateXrayAt(12345);
		perception.setDeafness(3);
		SoldierAwarenessComponent& awareness = soldier.awareness();
		awareness.markVisible();
		awareness.lastRenderedVisibility() = -1;
		awareness.recordNewOpponent();
		awareness.recordNewOpponent();
		awareness.tilesSinceForget() = 201;
		SoldierCamouflageComponent& camouflage = soldier.camouflage();
		camouflage.jungleApplied() = 25;
		camouflage.jungleWorn() = 30;
		camouflage.urbanApplied() = 20;
		camouflage.urbanWorn() = 45;
		camouflage.desertApplied() = -20;
		camouflage.desertWorn() = -90;
		camouflage.snowApplied() = 15;
		camouflage.snowWorn() = 10;
		SoldierEmploymentComponent& employment = soldier.employment();
		employment.endTime() = 12000;
		employment.startTime() = 17;
		employment.totalLength() = 14;
		employment.mercenaryType() = MERC_TYPE__AIM_MERC;
		employment.medicalDeposit() = 600;
		employment.lifeInsurance() = 1;
		employment.insuranceStartDay() = 4;
		employment.insuranceLengthDays() = 8;
		employment.lastContractUpdateTime() = 12001;
		employment.lastContractType() = CONTRACT_EXTEND_2_WEEK;
		employment.justFired() = 1;
		employment.renewalQuoteCode() = SOLDIER_CONTRACT_RENEW_QUOTE_89_USED;
		employment.timeCanSignElsewhere() = 13000;
		employment.hospitalPriceModifier() = -2;
		employment.insuranceStartTime() = 11000;
		SoldierAssignmentComponent& assignment = soldier.assignment();
		assignment.current() = TRAIN_TEAMMATE;
		assignment.previous() = ON_DUTY;
		assignment.trainingStat() = STRENGTH;
		assignment.lastChangeMinute() = 11900;
		assignment.desiredSquad() = 3;
		assignment.mergeTraversalAllowance() = 4;
		assignment.hours() = 6;
		assignment.repairVehicleId() = 2;
		assignment.facilityType() = 5;
		assignment.itemMoveSectorId() = 47;
		assignment.miniEventHoursRemaining() = 12;
		SoldierDeploymentComponent& deployment = soldier.deployment();
		deployment.insertionDirection() = -3;
		deployment.groupId() = 42;
		deployment.insertionGrid() = 2200;
		deployment.setStrategicInsertion(INSERTION_CODE_GRIDNO, 2201);
		deployment.setSector(9, 4, 1);
		deployment.vehicleId() = 7;
		deployment.offWorldGrid() = 2202;
		deployment.setTraversalOrigin(31, 2203);
		deployment.useExitGridForReentryDirection() = 1;
		deployment.scheduleArrival(14000, 6);
		deployment.beginArrivalGetup();
		deployment.arrivalGetupCounter() = 15000;
		SoldierScheduleComponent& schedule = soldier.schedule();
		schedule.id() = 37;
		schedule.progress() = 2;
		schedule.beginDoorContinuation(2204);
		schedule.completeDoorAnimation();
		SoldierPositionComponent& position = soldier.position();
		position.setWorldCoordinates(123.75f, 456.25f);
		position.recordTurnStart(120, 450);
		position.initialGrid() = 1200;
		position.gridNo() = 1234;
		position.level() = 1;
		position.direction() = 6;
		position.heightAdjustment() = 17;
		position.desiredHeight() = 25;
		position.temporaryGrid() = 1236;
		position.roomNo() = 8;
		position.terrainType() = PAVED_ROAD;
		position.enterTerrain(LOW_GRASS);
		SoldierMovementHistoryComponent& movementHistory = soldier.movementHistory();
		movementHistory.recordDeparture(1233);
		movementHistory.recentLocations()[0] = 1220;
		movementHistory.recentLocations()[1] = 1221;
		SoldierPathingComponent& pathing = soldier.pathing();
		pathing.desiredDirection() = 7;
		pathing.destinationX() = 14;
		pathing.destinationY() = 28;
		pathing.destinationGrid() = 1235;
		pathing.finalDestinationGrid() = 1240;
		pathing.stopped() = 1;
		pathing.needsLook() = 1;
		pathing.path()[0] = 2;
		pathing.path()[1] = 3;
		pathing.pathSize() = 2;
		pathing.pathIndex() = 1;
		pathing.blackListGrid() = 1300;
		pathing.stored() = 1;
		SoldierMovementComponent& movement = soldier.movement();
		movement.mode() = SWATTING;
		movement.setStealth(true);
		movement.setReverse(true);
		movement.setHighResolutionFacing(11, 13);
		movement.animationDirection() = 5;
		movement.requestGridUpdateSuppression();
		movement.waitForGrid(1250, 9);
		movement.reservedGrid() = 1251;
		movement.blockInDirection(3);
		movement.absoluteDestination() = 1260;
		movement.setContinuedPath(1270);
		movement.delayedFlags() = 5;
		movement.stopReason() = 2;
		movement.overrideMoveSpeedWith(SoldierID{ 4 });
		movement.beginTurn();
		movement.rememberWaterState(true);
		movement.setUiMovementFast(2);
		movement.setOutOfActionPoints(true);
		movement.pauseMovement();
		movement.startMovementClock();
		movement.setNetworkDelayed(true);
		movement.syncPresentationMotion(true);
		movement.markPastXDestination();
		movement.markPastYDestination();
		soldier.interruptSnapshot().captureMoved(1);
		SoldierTargetingComponent& targeting = soldier.targeting();
		targeting.selectLocation(1280, 1, 3);
		targeting.lastGridNo() = 1279;
		targeting.selectSoldier(SoldierID{ 5 });
		SoldierAttackSelectionComponent& attackSelection = soldier.attackSelection();
		attackSelection.selectWeapon(SECONDHANDPOS, 321);
		attackSelection.weaponMode() = WM_ATTACHED_UB_AUTO;
		attackSelection.scopeMode() = USE_ALT_WEAPON_HOLD;
		attackSelection.shotLocation() = AIM_SHOT_HEAD;
		attackSelection.meleeLocation() = AIM_SHOT_LEGS;
		SoldierMeleeApproachComponent& meleeApproach = soldier.meleeApproach();
		meleeApproach.recordPath(SWATTING, 11, 6);
		meleeApproach.rememberGrid(1275);
		SoldierFireControlComponent& fireControl = soldier.fireControl();
		fireControl.selectAutofire(7);
		fireControl.bulletsLeft() = 3;
		fireControl.spreadIndex() = 2;
		fireControl.autofireLastStep() = TRUE;
		for (UINT8 i = 0; i < SoldierFireControlComponent::SpreadTargetCapacity; ++i)
			fireControl.spreadLocations()[i] = 1281 + i;
		fireControl.previousMuzzleOffsetX()[0] = 1.25f;
		fireControl.previousMuzzleOffsetX()[1] = 2.5f;
		fireControl.previousMuzzleOffsetY()[0] = -1.5f;
		fireControl.previousMuzzleOffsetY()[1] = -3.0f;
		fireControl.previousCounterForceX()[0] = 0.5f;
		fireControl.previousCounterForceX()[1] = 1.0f;
		fireControl.previousCounterForceY()[0] = -0.75f;
		fireControl.previousCounterForceY()[1] = -1.25f;
		fireControl.initialMuzzleOffsetX() = 4.0f;
		fireControl.initialMuzzleOffsetY() = -5.0f;
		fireControl.barrelCounter() = 2;
		fireControl.beginSpreadDrag(1287);
		fireControl.updateSpreadDrag(1288);
		SoldierCombatResultComponent& combatResult = soldier.combatResult();
		combatResult.recordHit(SoldierID{ 6 }, AIM_SHOT_HEAD);
		combatResult.previousAttacker() = SoldierID{ 5 };
		combatResult.earlierAttacker() = SoldierID{ 4 };
		combatResult.lastDamageReason() = 7;
		combatResult.hitsThisTurn() = 3;
		combatResult.pelletsHitBy() = 4;
		combatResult.accumulatedDamage() = 27;
		SoldierCombatContributionComponent& combatContribution =
			soldier.combatContribution();
		combatContribution.recordMilitiaKill();
		combatContribution.recordMilitiaKill();
		combatContribution.recordMilitiaAssist();
		combatContribution.damageByTeam()[0] = 41;
		combatContribution.damageByTeam()[NUM_ASSIST_SLOTS - 1] = 42;
		SoldierDamageDisplayComponent& damageDisplay = soldier.damageDisplay();
		damageDisplay.activateAt(13, -9);
		damageDisplay.counter() = 2;
		damageDisplay.direction() = -1;
		SoldierRenderStateComponent& renderState = soldier.renderState();
		std::strcpy(renderState.headPalette(), "BROWNHEAD");
		std::strcpy(renderState.pantsPalette(), "GREENPANTS");
		std::strcpy(renderState.vestPalette(), "REDVEST");
		std::strcpy(renderState.skinPalette(), "PINKSKIN");
		std::strcpy(renderState.miscPalette(), "MISC");
		renderState.beginFade(2, 17, 1301);
		renderState.forceRenderColor() = TRUE;
		renderState.forceNoPaletteCycle() = TRUE;
		renderState.enableForceShade();
		renderState.showMuzzleFlash();
		renderState.setUnblitRect(11, 12, 13, 14);
		renderState.lightSprite() = 501;
		renderState.startMuzzleFlashSprite(502);
		renderState.muzzleFlashFrame() = 3;
		renderState.setBoundingBox(41, 42, -3, -4);
		SoldierUiPresentationComponent& uiPresentation = soldier.uiPresentation();
		uiPresentation.portraitFlashFrame() = 3;
		uiPresentation.startLocator(5);
		uiPresentation.locatorFrame() = 2;
		uiPresentation.setLocatorOffset(7, -8);
		uiPresentation.interfaceLevel() = 1;
		uiPresentation.closePanelFrame() = 2;
		uiPresentation.deadPanelFrame() = 3;
		uiPresentation.openPanelFrame() = 4;
		uiPresentation.setPanelFacePosition(91, 92);
		uiPresentation.setPlannedTarget(500, 600, 12);
		uiPresentation.lastEnemyCycled() = SoldierID{ 9 };
		SoldierSuppressionComponent& suppression = soldier.suppression();
		suppression.underFire() = 2;
		suppression.shock() = 9;
		suppression.addPoints(4);
		suppression.addActionPointLoss(6);
		suppression.suppressor() = SoldierID{ 8 };
		suppression.markCloseCall();
		SoldierAnimationIntentComponent& animationIntent = soldier.animationIntent();
		animationIntent.requestHeight( ANIM_CROUCH );
		animationIntent.queueFacingAnimation( WALKING, 4 );
		animationIntent.queueStance( ANIM_PRONE );
		animationIntent.queueSecondaryAnimation( RUNNING );
		animationIntent.markTurningFromUi();
		animationIntent.requestStopAtNextTile();
		animationIntent.continueAfterStance( 2 );
		SoldierAnimationPlaybackComponent& animationPlayback = soldier.animationPlayback();
		animationPlayback.state() = RUNNING;
		animationPlayback.code() = 17;
		animationPlayback.frame() = 23;
		animationPlayback.delay() = -7;
		animationPlayback.previousState() = WALKING;
		animationPlayback.previousCode() = 11;
		animationPlayback.surface() = 321;
		animationPlayback.zLevel() = 654;
		animationPlayback.subFlags() = 0x10203040;
		SoldierAnimationActivityComponent& animationActivity = soldier.animationActivity();
		animationActivity.turningFromProneMode() = TURNING_FROM_PRONE_ON;
		animationActivity.readyCostWaived() = TRUE;
		animationActivity.postHitStance() = GO_TO_AIM_AFTER_HIT;
		animationActivity.pause();
		animationActivity.holdAttackerUntilDone() = TRUE;
		animationActivity.turningToShoot() = TRUE;
		animationActivity.turningToFall() = TRUE;
		animationActivity.turningUntilDone() = TRUE;
		animationActivity.beginHit();
		animationActivity.advanceHit();
		animationActivity.setInterruptibility( TRUE, TRUE );
		animationActivity.turningCostWaived() = TRUE;
		animationActivity.suppressionStanceChange() = TRUE;
		animationActivity.stanceCostWaived() = TRUE;
		animationActivity.beginFall( 6 );
		animationActivity.fallClockwise() = TRUE;
		animationActivity.turningIncrement() = -1;
		animationActivity.forecastTraversalAt(1290);
		animationActivity.setRenderZOverride(999);
		animationActivity.randomActionCheckCounter() = 42;
		animationActivity.lastRandomAnimation() = 123;
		CHECK( vitals.alive() &&
		       vitals.hasHealableInjury() &&
		       vitals.isUndergoingSurgery() &&
		       soldier.vitals().health() == 75 &&
		       soldier.vitals().breath() == 60,
		       "soldier vitals component owns health, breath, and recovery state" );
		const SOLDIERTYPE& constSoldier = soldier;
		CHECK( constSoldier.vitals().health() == 75 &&
		       constSoldier.vitals().maximumHealth() == 90 &&
		       constSoldier.vitals().breath() == 60 &&
		       constSoldier.vitals().maximumBreath() == 95 &&
		       constSoldier.vitals().bleeding() == 8 &&
		       constSoldier.vitals().previousHealth() == 74 &&
		       constSoldier.vitals().fractionalHealth() == 37 &&
		       constSoldier.vitals().breathReduction() == 420 &&
		       constSoldier.vitals().healableInjury() == 1600 &&
		       constSoldier.vitals().isUndergoingSurgery() &&
		       constSoldier.vitals().unregainableBreath() == 222 &&
		       constSoldier.vitals().criticalStatDamage()[DAMAGED_STAT_HEALTH] == 4 &&
		       constSoldier.vitals().criticalStatDamage()[DAMAGED_STAT_AGILITY] == 6 &&
		       constSoldier.vitals().nextBleedAt() == 2.5f &&
		       constSoldier.vitals().regenerationCounter() == -1 &&
		       constSoldier.vitals().regenerationBoostersUsedToday() == 2 &&
		       constSoldier.vitals().lastBleedGruntAt() == 12340,
		       "const soldier access reads the complete canonical vitals component" );
		CHECK( constSoldier.service().active() &&
		       constSoldier.service().providerCount() == 2 &&
		       constSoldier.service().hasProviders() &&
		       constSoldier.service().hasPartner() &&
		       constSoldier.service().partner() == SoldierID{ 7 } &&
		       constSoldier.service().hasAutoBandagingMedic() &&
		       constSoldier.service().autoBandagingMedic() == SoldierID{ 8 },
		       "soldier service component owns service activity, providers, partner, and automatic-bandage reservation" );
		CHECK( constSoldier.dialogue().hasQuoteRecord() &&
		       constSoldier.dialogue().quoteRecord() == 13 &&
		       constSoldier.dialogue().hasQuoteAction() &&
		       constSoldier.dialogue().quoteActionId() == QUOTE_ACTION_ID_CHECKFORDEST &&
		       constSoldier.dialogue().battleSoundSet() == 4 &&
		       constSoldier.dialogue().hasSaid(SOLDIER_QUOTE_SAID_LOW_BREATH) &&
		       constSoldier.dialogue().vocalVolume() == 87 &&
		       constSoldier.dialogue().previousBattleSound() == BATTLE_SOUND_ATTN1 &&
		       constSoldier.dialogue().repeatedBattleSoundAt() == 12342 &&
		       constSoldier.dialogue().heardNoiseCooldownTurns() == 5 &&
		       constSoldier.dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL) &&
		       constSoldier.dialogue().activeBattleSound() == 77 &&
		       constSoldier.dialogue().currentCivilianQuote() == -2 &&
		       constSoldier.dialogue().civilianQuoteDelta() == 2 &&
		       constSoldier.dialogue().lastSpokeAt() == 12343 &&
		       constSoldier.dialogue().corpseQuoteTolerance() == 3,
		       "soldier dialogue component owns quote planning, spoken history, voice playback, and cooldown state" );
		CHECK( constSoldier.audio().lastFootstepVariant() == 2 &&
		       constSoldier.audio().hasDoorOpeningNoise() &&
		       constSoldier.audio().doorOpeningNoise() == 17 &&
		       constSoldier.audio().hasBurstSound() &&
		       constSoldier.audio().burstSoundId() == 301 &&
		       constSoldier.audio().hasPositionSound() &&
		       constSoldier.audio().positionSoundId() == 302 &&
		       constSoldier.audio().hasTurningSound() &&
		       constSoldier.audio().turningSoundId() == 303,
		       "soldier audio component owns footstep/noise state and non-dialogue sound handles" );
		CHECK( constSoldier.replication().movementStartedAt() == 12344 &&
		       constSoldier.replication().optimumMovementTime() == 12345 &&
		       constSoldier.replication().hasLastUpdate() &&
		       constSoldier.replication().lastUpdateAt() == 12346 &&
		       constSoldier.replication().updateSequence() == 304 &&
		       constSoldier.replication().updateType() == 7 &&
		       constSoldier.replication().scheduledStopGrid() == 1235 &&
		       constSoldier.replication().checksum() == 305,
		       "soldier replication component owns transport timing, update metadata, stop scheduling, and integrity state" );
		CHECK( constSoldier.movementMetrics().carriedWeightAtTurnStart() == 135 &&
		       constSoldier.movementMetrics().movedThisTurn() &&
		       constSoldier.movementMetrics().tilesMoved() == 7 &&
		       constSoldier.movementMetrics().hasRealtimeBreathMovement() &&
		       constSoldier.movementMetrics().realtimeBreathTiles() == 4 &&
		       constSoldier.movementMetrics().lastRealtimeMovementAnimation() == RUNNING,
		       "soldier movement metrics own turn distance, carried-weight snapshot, and realtime breath cadence" );
		CHECK( constSoldier.aiPlanning().flanking(6) &&
		       constSoldier.aiPlanning().flankCount() == 3 &&
		       constSoldier.aiPlanning().flankAnchorGrid() == 1236 &&
		       constSoldier.aiPlanning().sniperPostureActive() &&
		       constSoldier.aiPlanning().flankOriginDirection() == 5 &&
		       constSoldier.aiPlanning().planIndex() == 4,
		       "soldier AI planning owns flanking, sniper posture, and modular plan selection" );
		CHECK( constSoldier.skillState().isRepeatedCheck(-7, 1234) &&
		       constSoldier.skillState().checkAttempts() == 2 &&
		       constSoldier.skillState().selectedAiSkill() == SKILLS_FOCUS &&
		       constSoldier.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY) == 3 &&
		       constSoldier.skillState().counter(SOLDIER_COUNTER_SPOTTER) == 4 &&
		       constSoldier.skillState().counter(SOLDIER_COUNTER_ROLE_OBSERVED) == 5 &&
		       constSoldier.skillState().counter(SOLDIER_COUNTER_RETREAT) == 6 &&
		       constSoldier.skillState().counter(SOLDIER_COUNTER_MAX - 1) == 19 &&
		       constSoldier.skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_SECONDS) == 12344 &&
		       constSoldier.skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_APS) == 12 &&
		       constSoldier.skillState().hasCooldown(SOLDIER_COOLDOWN_CRYO) &&
		       constSoldier.skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) == 33 &&
		       constSoldier.skillState().cooldown(SOLDIER_COOLDOWN_DRUGUSER_COMBAT) == 4 &&
		       constSoldier.skillState().cooldown(SOLDIER_COOLDOWN_ROBOT_XRAY) == 5 &&
		       constSoldier.skillState().cooldown(SOLDIER_COOLDOWN_MAX - 1) == 190 &&
		       constSoldier.skillState().focusGrid() == 1290,
		       "soldier skill-state component owns checks, AI selection, counters, cooldowns, and focus" );
		CHECK( constSoldier.condition().hasExtraStats() &&
		       constSoldier.condition().extraStrength() == 11 &&
		       constSoldier.condition().extraDexterity() == -12 &&
		       constSoldier.condition().extraAgility() == 13 &&
		       constSoldier.condition().extraWisdom() == -14 &&
		       constSoldier.condition().extraExperienceLevel() == 2 &&
		       constSoldier.condition().foodLevel() == 54000 &&
		       constSoldier.condition().drinkLevel() == -24000 &&
		       constSoldier.condition().hasStarvationDamage() &&
		       constSoldier.condition().starvationHealthDamage() == 7 &&
		       constSoldier.condition().starvationStrengthDamage() == 8 &&
		       constSoldier.condition().infected(0) &&
		       constSoldier.condition().hasDiseaseFlag(0, 0x02) &&
		       constSoldier.condition().diseasePoints(6) == -202 &&
		       constSoldier.condition().diseaseFlags(6) == 0x40 &&
		       constSoldier.condition().diseasePoints(NUM_DISEASES - 1) == 303 &&
		       constSoldier.condition().diseaseFlags(NUM_DISEASES - 1) == 0x80 &&
		       constSoldier.condition().hasDisability(2) &&
		       constSoldier.condition().hasDisability(SoldierConditionComponent::DisabilityBitCount),
		       "soldier condition component owns temporary stats, nutrition, starvation, disease, and acquired disabilities" );
		CHECK( constSoldier.longAction().active() &&
		       constSoldier.longAction().action() == MTA_FORTIFY &&
		       constSoldier.longAction().contextGrid() == 1300 &&
		       constSoldier.longAction().remainingActionPoints() == 37,
		       "soldier long-action component owns the action, retained grid, and remaining AP cost" );
		CHECK( constSoldier.interaction().isNonNpcTrader() &&
		       constSoldier.interaction().nonNpcTraderId() == 7 &&
		       constSoldier.interaction().draggingPerson() &&
		       constSoldier.interaction().draggedPerson() == SoldierID{ 17 } &&
		       !constSoldier.interaction().draggingCorpse() &&
		       !constSoldier.interaction().draggingStructure() &&
		       constSoldier.interaction().chatting() &&
		       constSoldier.interaction().chatPartner() == SoldierID{ 18 },
		       "soldier interaction component owns merchant, drag, and reciprocal chat state" );
		CHECK( constSoldier.pendingAction().active() &&
		       constSoldier.pendingAction().action() == MERC_GIVEITEM &&
		       constSoldier.pendingAction().animationCount() == 9 &&
		       constSoldier.pendingAction().primaryData() == 1301 &&
		       constSoldier.pendingAction().secondaryData() == -1302 &&
		       constSoldier.pendingAction().tertiaryData() == -3 &&
		       constSoldier.pendingAction().doorHandleCode() == 4 &&
		       constSoldier.pendingAction().quaternaryData() == 1303 &&
		       constSoldier.pendingAction().nextSpecialData() == -1304 &&
		       constSoldier.pendingAction().interruptionMarker() == 5 &&
		       constSoldier.pendingAction().inventorySlot() == -6,
		       "soldier pending-action component owns persistent current, queued, and payload state" );
		CHECK( constSoldier.actionPoints().hasAny() &&
		       constSoldier.actionPoints().current() == 43 &&
		       constSoldier.actionPoints().initial() == 78,
		       "soldier action-point component owns current and turn-start budgets" );
		CHECK( constSoldier.collapseState().collapsed() &&
		       constSoldier.collapseState().breathCollapsed() &&
		       constSoldier.collapseState().turns() == 2 &&
		       constSoldier.collapseState().sleepDrugCounter() == 7 &&
		       constSoldier.collapseState().fatigueCollapsed(),
		       "soldier collapse component owns tactical and strategic incapacitation state" );
		CHECK( constSoldier.perception().hasHeardMovementFrom(2) &&
		       constSoldier.perception().hasHeardMovementFrom(6) &&
		       constSoldier.perception().viewRange() == 14 &&
		       constSoldier.perception().blindnessTurns() == 5 &&
		       constSoldier.perception().heardNoiseLevel() == 1 &&
		       constSoldier.perception().xrayActivatedAt() == 12345 &&
		       constSoldier.perception().deafnessTurns() == 3,
		       "soldier perception component owns sensory range, memory, effects, and X-ray lifetime" );
		CHECK( constSoldier.awareness().visibleNow() &&
		       constSoldier.awareness().lastRenderedVisibility() == -1 &&
		       constSoldier.awareness().renderVisibilityChanged() &&
		       constSoldier.awareness().newOpponentCount() == 2 &&
		       constSoldier.awareness().hasNewOpponents() &&
		       constSoldier.awareness().tilesSinceForget() == 201,
		       "soldier awareness component owns visibility, render synchronization, discovery, and stale-memory distance" );
		CHECK( constSoldier.camouflage().total(SoldierCamouflageComponent::Terrain::Jungle) == 55 &&
		       constSoldier.camouflage().total(SoldierCamouflageComponent::Terrain::Urban) == 65 &&
		       constSoldier.camouflage().total(SoldierCamouflageComponent::Terrain::Desert) == -100 &&
		       constSoldier.camouflage().total(SoldierCamouflageComponent::Terrain::Snow) == 25 &&
		       constSoldier.camouflage().strongestTotal() == 65 &&
		       constSoldier.camouflage().appliedTotal() == 40,
		       "soldier camouflage component owns applied and equipment totals for every terrain family" );
		CHECK( constSoldier.employment().endTime() == 12000 &&
		       constSoldier.employment().startTime() == 17 &&
		       constSoldier.employment().totalLength() == 14 &&
		       constSoldier.employment().isMercenaryType(MERC_TYPE__AIM_MERC) &&
		       constSoldier.employment().medicalDeposit() == 600 &&
		       constSoldier.employment().hasMedicalDeposit() &&
		       constSoldier.employment().hasLifeInsurance() &&
		       constSoldier.employment().insuranceStartDay() == 4 &&
		       constSoldier.employment().insuranceLengthDays() == 8 &&
		       constSoldier.employment().lastContractUpdateTime() == 12001 &&
		       constSoldier.employment().lastContractType() == CONTRACT_EXTEND_2_WEEK &&
		       constSoldier.employment().wasJustFired() &&
		       constSoldier.employment().renewalQuoteCode() == SOLDIER_CONTRACT_RENEW_QUOTE_89_USED &&
		       constSoldier.employment().timeCanSignElsewhere() == 13000 &&
		       constSoldier.employment().hospitalPriceModifier() == -2 &&
		       constSoldier.employment().insuranceStartTime() == 11000,
		       "soldier employment component owns contract, classification, deposit, insurance, and renewal state" );
		CHECK( constSoldier.assignment().isAssignedTo(TRAIN_TEAMMATE) &&
		       constSoldier.assignment().previous() == ON_DUTY &&
		       constSoldier.assignment().trainingStat() == STRENGTH &&
		       constSoldier.assignment().lastChangeMinute() == 11900 &&
		       constSoldier.assignment().desiredSquad() == 3 &&
		       constSoldier.assignment().mergeTraversalAllowance() == 4 &&
		       constSoldier.assignment().hours() == 6 &&
		       constSoldier.assignment().hasAssignmentHours() &&
		       constSoldier.assignment().repairVehicleId() == 2 &&
		       constSoldier.assignment().facilityType() == 5 &&
		       constSoldier.assignment().itemMoveSectorId() == 47 &&
		       constSoldier.assignment().miniEventHoursRemaining() == 12 &&
		       constSoldier.assignment().hasMiniEventTime(),
		       "soldier assignment component owns strategic duty and its subsidiary context" );
		CHECK( constSoldier.deployment().insertionDirection() == -3 &&
		       constSoldier.deployment().groupId() == 42 &&
		       constSoldier.deployment().insertionGrid() == 2200 &&
		       constSoldier.deployment().strategicInsertionCode() == INSERTION_CODE_GRIDNO &&
		       constSoldier.deployment().strategicInsertionData() == 2201 &&
		       constSoldier.deployment().isInSector(9, 4, 1) &&
		       constSoldier.deployment().vehicleId() == 7 &&
		       constSoldier.deployment().hasVehicle() &&
		       constSoldier.deployment().offWorldGrid() == 2202 &&
		       constSoldier.deployment().previousSectorId() == 31 &&
		       constSoldier.deployment().useExitGridForReentryDirection() == 1 &&
		       constSoldier.deployment().preTraversalGrid() == 2203 &&
		       constSoldier.deployment().leaveHistoryCode() == 6 &&
		       constSoldier.deployment().arrivalTime() == 14000 &&
		       constSoldier.deployment().arrivalGetupPending() &&
		       constSoldier.deployment().ignoreCollapseGetupCheck() &&
		       constSoldier.deployment().arrivalGetupCounter() == 15000,
		       "soldier deployment component owns strategic placement, insertion, traversal, and arrival lifecycle" );
		CHECK( constSoldier.schedule().assigned() &&
		       constSoldier.schedule().id() == 37 &&
		       constSoldier.schedule().progress() == 2 &&
		       constSoldier.schedule().doorAnimationComplete() &&
		       constSoldier.schedule().doorGrid() == 2204,
		       "soldier schedule component owns NPC schedule execution and door continuation state" );
		CHECK( constSoldier.position().worldX() == 123.75f &&
		       constSoldier.position().worldY() == 456.25f &&
		       constSoldier.position().worldXInt() == 123 &&
		       constSoldier.position().worldYInt() == 456 &&
		       constSoldier.position().hasTurnStart() &&
		       constSoldier.position().turnStartX() == 120 &&
		       constSoldier.position().turnStartY() == 450 &&
		       constSoldier.position().initialGrid() == 1200 &&
		       constSoldier.position().gridNo() == 1234 &&
		       constSoldier.position().level() == 1 &&
		       constSoldier.position().direction() == 6 &&
		       constSoldier.position().heightAdjustment() == 17 &&
		       constSoldier.position().desiredHeight() == 25 &&
		       constSoldier.position().temporaryGrid() == 1236 &&
		       constSoldier.position().roomNo() == 8 &&
		       constSoldier.position().terrainType() == LOW_GRASS &&
		       constSoldier.position().previousTerrainType() == PAVED_ROAD,
		       "soldier position component owns precise, projected, historical, vertical, room, and terrain placement" );
		CHECK( constSoldier.movementHistory().previousGrid() == 1233 &&
		       constSoldier.movementHistory().recentLocations()[0] == 1220 &&
		       constSoldier.movementHistory().recentLocations()[1] == 1221,
		       "soldier movement-history component owns departed-grid and bounded AI location memory" );
		CHECK( constSoldier.pathing().desiredDirection() == 7 &&
		       constSoldier.pathing().destinationGrid() == 1235 &&
		       constSoldier.pathing().finalDestinationGrid() == 1240 &&
		       constSoldier.pathing().path()[0] == 2 &&
		       constSoldier.pathing().pathSize() == 2 &&
		       constSoldier.pathing().pathIndex() == 1 &&
		       !constSoldier.pathing().empty() &&
		       !constSoldier.pathing().complete(),
		       "soldier pathing component owns the canonical tactical route" );
		CHECK( constSoldier.movement().mode() == SWATTING &&
		       constSoldier.movement().stealthy() &&
		       constSoldier.movement().reversing() &&
		       constSoldier.movement().highResolutionDirection() == 11 &&
		       constSoldier.movement().highResolutionDesiredDirection() == 13 &&
		       constSoldier.movement().animationDirection() == 5 &&
		       constSoldier.movement().gridUpdatePolicy() == 1 &&
		       constSoldier.movement().delayed() &&
		       constSoldier.movement().delayCounter() == 9 &&
		       constSoldier.movement().delayedCauseGrid() == 1250 &&
		       constSoldier.movement().reservedGrid() == 1251 &&
		       constSoldier.movement().blockedByAnotherMerc() &&
		       constSoldier.movement().blockedDirection() == 3 &&
		       constSoldier.movement().absoluteDestination() == 1260 &&
		       constSoldier.movement().continuedPathValid() &&
		       constSoldier.movement().continuedPathGrid() == 1270 &&
		       constSoldier.movement().delayedFlags() == 5 &&
		       constSoldier.movement().stopReason() == 2 &&
		       constSoldier.movement().usesMoveSpeedOverride() &&
		       constSoldier.movement().moveSpeedOverride() == SoldierID{ 4 } &&
		       constSoldier.movement().turnActive() &&
		       constSoldier.movement().wasInWater() &&
		       constSoldier.movement().uiMovementFast() == 2 &&
		       constSoldier.movement().outOfActionPoints() &&
		       constSoldier.movement().movementPaused() &&
		       constSoldier.movement().recordingMovement() &&
		       constSoldier.movement().delayedByNetwork() &&
		       constSoldier.movement().wasMoving() &&
		       constSoldier.movement().crossedDestinationCenter(),
		       "soldier movement component owns tactical intent, contention, and activity state" );
		CHECK( constSoldier.interruptSnapshot().movedBeforeInterrupt() == 1,
		       "soldier interrupt snapshot owns the pre-interrupt scheduler state" );
		CHECK( constSoldier.targeting().gridNo() == 1280 &&
		       constSoldier.targeting().level() == 1 &&
		       constSoldier.targeting().cubeLevel() == 3 &&
		       constSoldier.targeting().lastGridNo() == 1279 &&
		       constSoldier.targeting().hasTargetSoldier() &&
		       constSoldier.targeting().targetId() == SoldierID{ 5 },
		       "soldier targeting component owns current target geometry and identity" );
		CHECK( constSoldier.attackSelection().hand() == SECONDHANDPOS &&
		       constSoldier.attackSelection().weapon() == 321 &&
		       constSoldier.attackSelection().weaponMode() == WM_ATTACHED_UB_AUTO &&
		       constSoldier.attackSelection().scopeMode() == USE_ALT_WEAPON_HOLD &&
		       constSoldier.attackSelection().shotLocation() == AIM_SHOT_HEAD &&
		       constSoldier.attackSelection().meleeLocation() == AIM_SHOT_LEGS,
		       "soldier attack-selection component owns weapon and aim choices" );
		CHECK( constSoldier.meleeApproach().matches(1275, SWATTING) &&
		       constSoldier.meleeApproach().cost() == 11 &&
		       constSoldier.meleeApproach().endDirection() == 6,
		       "soldier melee-approach component owns the reusable path-cost result" );
		CHECK( constSoldier.fireControl().bursting() &&
		       constSoldier.fireControl().burstCounter() == 1 &&
		       constSoldier.fireControl().autofiring() &&
		       constSoldier.fireControl().autofireShots() == 7 &&
		       constSoldier.fireControl().bulletsLeft() == 3 &&
		       constSoldier.fireControl().spreadIndex() == 2 &&
		       constSoldier.fireControl().autofireLastStep() &&
		       constSoldier.fireControl().spreadLocations()[0] == 1281 &&
		       constSoldier.fireControl().spreadLocations()[5] == 1286 &&
		       constSoldier.fireControl().previousMuzzleOffsetX()[1] == 2.5f &&
		       constSoldier.fireControl().previousCounterForceY()[1] == -1.25f &&
		       constSoldier.fireControl().initialMuzzleOffsetX() == 4.0f &&
		       constSoldier.fireControl().initialMuzzleOffsetY() == -5.0f &&
		       constSoldier.fireControl().barrelCounter() == 2 &&
		       constSoldier.fireControl().spreadDragStartGrid() == 1287 &&
		       constSoldier.fireControl().spreadDragEndGrid() == 1288 &&
		       constSoldier.fireControl().spreadDragMoved(),
		       "soldier fire-control component owns volley selection and execution state" );
		CHECK(
			SoldierFireControlComponent::clampSpreadTargetCount(4) == 4 &&
			SoldierFireControlComponent::clampSpreadTargetCount(12) ==
				SoldierFireControlComponent::SpreadTargetCapacity,
			"soldier fire control caps dual-wield spread targets at persistent capacity" );
		CHECK( constSoldier.combatResult().hasCurrentAttacker() &&
		       constSoldier.combatResult().currentAttacker() == SoldierID{ 6 } &&
		       constSoldier.combatResult().previousAttacker() == SoldierID{ 5 } &&
		       constSoldier.combatResult().earlierAttacker() == SoldierID{ 4 } &&
		       constSoldier.combatResult().hitLocation() == AIM_SHOT_HEAD &&
		       constSoldier.combatResult().lastDamageReason() == 7 &&
		       constSoldier.combatResult().hitsThisTurn() == 3 &&
		       constSoldier.combatResult().pelletsHitBy() == 4 &&
		       constSoldier.combatResult().accumulatedDamage() == 27,
		       "soldier combat-result component owns incoming hit attribution and outcome state" );
		CHECK( constSoldier.combatContribution().hasMilitiaKills() &&
		       constSoldier.combatContribution().hasMilitiaCredit() &&
		       constSoldier.combatContribution().militiaKills() == 2 &&
		       constSoldier.combatContribution().militiaAssists() == 1 &&
		       constSoldier.combatContribution().militiaPromotionPoints() == 5 &&
		       constSoldier.combatContribution().damageByTeam()[0] == 41 &&
		       constSoldier.combatContribution().damageByTeam()[NUM_ASSIST_SLOTS - 1] == 42,
		       "soldier combat-contribution component owns militia credit and fixed assist attribution" );
		CHECK( constSoldier.damageDisplay().displaying() &&
		       constSoldier.damageDisplay().counter() == 2 &&
		       constSoldier.damageDisplay().offsetX() == 13 &&
		       constSoldier.damageDisplay().offsetY() == -9 &&
		       constSoldier.damageDisplay().direction() == -1,
		       "soldier damage-display component owns floating-number presentation state" );
		CHECK( std::strcmp(constSoldier.renderState().headPalette(), "BROWNHEAD") == 0 &&
		       std::strcmp(constSoldier.renderState().pantsPalette(), "GREENPANTS") == 0 &&
		       std::strcmp(constSoldier.renderState().vestPalette(), "REDVEST") == 0 &&
		       std::strcmp(constSoldier.renderState().skinPalette(), "PINKSKIN") == 0 &&
		       std::strcmp(constSoldier.renderState().miscPalette(), "MISC") == 0 &&
		       constSoldier.renderState().fadeMode() == 2 &&
		       constSoldier.renderState().fadeLevel() == 17 &&
		       constSoldier.renderState().fadeOriginGrid() == 1301 &&
		       constSoldier.renderState().forceRenderColor() &&
		       constSoldier.renderState().forceNoPaletteCycle() &&
		       constSoldier.renderState().forceShade() &&
		       constSoldier.renderState().muzzleFlashVisible() &&
		       constSoldier.renderState().unblitX() == 11 &&
		       constSoldier.renderState().unblitY() == 12 &&
		       constSoldier.renderState().unblitWidth() == 13 &&
		       constSoldier.renderState().unblitHeight() == 14 &&
		       constSoldier.renderState().lightSprite() == 501 &&
		       constSoldier.renderState().muzzleFlashSprite() == 502 &&
		       constSoldier.renderState().muzzleFlashFrame() == 3 &&
		       constSoldier.renderState().boundingBoxWidth() == 41 &&
		       constSoldier.renderState().boundingBoxHeight() == 42 &&
		       constSoldier.renderState().boundingBoxOffsetX() == -3 &&
		       constSoldier.renderState().boundingBoxOffsetY() == -4,
		       "soldier render-state component owns palette identity, fade, light, redraw, and geometry values" );
		CHECK( constSoldier.uiPresentation().portraitFlashFrame() == 3 &&
		       constSoldier.uiPresentation().locatorFrame() == 2 &&
		       constSoldier.uiPresentation().locatorOffsetX() == 7 &&
		       constSoldier.uiPresentation().locatorOffsetY() == -8 &&
		       constSoldier.uiPresentation().interfaceLevel() == 1 &&
		       constSoldier.uiPresentation().closePanelFrame() == 2 &&
		       constSoldier.uiPresentation().deadPanelFrame() == 3 &&
		       constSoldier.uiPresentation().openPanelFrame() == 4 &&
		       constSoldier.uiPresentation().panelFaceX() == 91 &&
		       constSoldier.uiPresentation().panelFaceY() == 92 &&
		       constSoldier.uiPresentation().hasPlannedTarget() &&
		       constSoldier.uiPresentation().plannedActionPointCost() == 12 &&
		       constSoldier.uiPresentation().plannedTargetX() == 500 &&
		       constSoldier.uiPresentation().plannedTargetY() == 600 &&
		       constSoldier.uiPresentation().lastEnemyCycled() == SoldierID{ 9 } &&
		       constSoldier.uiPresentation().locateCycles() == 5,
		       "soldier UI presentation component owns locator, panel, planning, and enemy-cycle view state" );
		CHECK( constSoldier.suppression().active() &&
		       constSoldier.suppression().underFire() == 2 &&
		       constSoldier.suppression().shock() == 9 &&
		       constSoldier.suppression().points() == 4 &&
		       constSoldier.suppression().actionPointsLost() == 6 &&
		       constSoldier.suppression().hasSuppressor() &&
		       constSoldier.suppression().suppressor() == SoldierID{ 8 } &&
		       constSoldier.suppression().closeCall(),
		       "soldier suppression component owns hostile-fire reaction state" );
		CHECK( constSoldier.animationIntent().hasDesiredHeight() &&
		       constSoldier.animationIntent().desiredHeight() == ANIM_CROUCH &&
		       constSoldier.animationIntent().hasPendingAnimation() &&
		       constSoldier.animationIntent().pendingAnimation() == WALKING &&
		       constSoldier.animationIntent().hasPendingStance() &&
		       constSoldier.animationIntent().pendingStance() == ANIM_PRONE &&
		       constSoldier.animationIntent().hasSecondaryPendingAnimation() &&
		       constSoldier.animationIntent().secondaryPendingAnimation() == RUNNING &&
		       constSoldier.animationIntent().hasPendingDirection() &&
		       constSoldier.animationIntent().pendingDirection() == 4 &&
		       constSoldier.animationIntent().turningFromUi() &&
		       constSoldier.animationIntent().stopPendingNextTile() &&
		       constSoldier.animationIntent().continuesAfterStance() &&
		       constSoldier.animationIntent().continuationMode() == 2,
		       "soldier animation-intent component owns queued transitions and continuation policy" );
		CHECK( constSoldier.animationPlayback().isPlaying( RUNNING ) &&
		       constSoldier.animationPlayback().code() == 17 &&
		       constSoldier.animationPlayback().frame() == 23 &&
		       constSoldier.animationPlayback().delay() == -7 &&
		       constSoldier.animationPlayback().previousState() == WALKING &&
		       constSoldier.animationPlayback().previousCode() == 11 &&
		       constSoldier.animationPlayback().surface() == 321 &&
		       constSoldier.animationPlayback().zLevel() == 654 &&
		       constSoldier.animationPlayback().subFlags() == 0x10203040,
		       "soldier animation-playback component owns accepted frame, timing, and render state" );
		CHECK( constSoldier.animationActivity().turningFromProneMode() == TURNING_FROM_PRONE_ON &&
		       constSoldier.animationActivity().readyCostWaived() &&
		       constSoldier.animationActivity().postHitStance() == GO_TO_AIM_AFTER_HIT &&
		       constSoldier.animationActivity().paused() &&
		       constSoldier.animationActivity().holdAttackerUntilDone() &&
		       constSoldier.animationActivity().turningToShoot() &&
		       constSoldier.animationActivity().turningToFall() &&
		       constSoldier.animationActivity().turningUntilDone() &&
		       constSoldier.animationActivity().gettingHit() &&
		       constSoldier.animationActivity().hitPhase() == 2 &&
		       constSoldier.animationActivity().nonInterruptible() &&
		       constSoldier.animationActivity().turningCostWaived() &&
		       constSoldier.animationActivity().suppressionStanceChange() &&
		       constSoldier.animationActivity().stanceCostWaived() &&
		       constSoldier.animationActivity().realtimeNonInterruptible() &&
		       constSoldier.animationActivity().tryingToFall() &&
		       constSoldier.animationActivity().fallClockwise() &&
		       constSoldier.animationActivity().fallDirection() == 6 &&
		       constSoldier.animationActivity().turningIncrement() == -1 &&
		       constSoldier.animationActivity().traversalForecastGrid() == 1290 &&
		       constSoldier.animationActivity().hasRenderZOverride() &&
		       constSoldier.animationActivity().renderZOverride() == 999 &&
		       constSoldier.animationActivity().randomActionCheckDue(41) &&
		       constSoldier.animationActivity().randomActionCheckCounter() == 42 &&
		       constSoldier.animationActivity().lastRandomAnimation() == 123,
		       "soldier animation-activity component owns coordinated turn, hit, fall, random-action, and AP lifecycle state" );
		SoldierAnimationCacheComponent invalidOwnerCache;
		invalidOwnerCache.initialize( NOBODY );
		CHECK( invalidOwnerCache.empty() &&
		       !invalidOwnerCache.acquire( NOBODY, 0, STANDING ),
		       "animation cache rejects the invalid sentinel owner without indexing global soldier history" );
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
		vitals.health() = 42;
		vitals.snapshotHealth();
		vitals.health() = 41;
		vitals.finishSurgery();
		vitals.clearCriticalStatDamage();
		CHECK( vitals.previousHealth() == 42 &&
		       !vitals.isUndergoingSurgery() &&
		       vitals.criticalStatDamage()[DAMAGED_STAT_HEALTH] == 0 &&
		       vitals.criticalStatDamage()[DAMAGED_STAT_AGILITY] == 0,
		       "soldier vitals component coordinates turn snapshots, surgery, and critical-damage recovery" );
		SoldierServiceComponent serviceLifecycle;
		serviceLifecycle.removeProvider();
		serviceLifecycle.addProvider();
		serviceLifecycle.addProvider();
		serviceLifecycle.removeProvider();
		serviceLifecycle.beginProvidingTo( SoldierID{ 9 } );
		serviceLifecycle.assignAutoBandagingMedic( SoldierID{ 10 } );
		CHECK( serviceLifecycle.providerCount() == 1 &&
		       serviceLifecycle.hasPartner() &&
		       serviceLifecycle.partner() == SoldierID{ 9 } &&
		       serviceLifecycle.hasAutoBandagingMedic(),
		       "soldier service component coordinates provider, partner, and automatic-bandage lifecycles" );
		serviceLifecycle.clearProviders();
		serviceLifecycle.finishProviding();
		serviceLifecycle.clearAutoBandagingMedic();
		serviceLifecycle.removeProvider();
		CHECK( !serviceLifecycle.hasProviders() &&
		       !serviceLifecycle.hasPartner() &&
		       !serviceLifecycle.hasAutoBandagingMedic(),
		       "soldier service component clears relationships without underflowing its provider count" );
		SoldierDialogueComponent dialogueLifecycle;
		dialogueLifecycle.quoteRecord() = 4;
		dialogueLifecycle.quoteActionId() = QUOTE_ACTION_ID_TURNTOWARDSPLAYER;
		dialogueLifecycle.markSaid(SOLDIER_QUOTE_SAID_PERSONALITY);
		dialogueLifecycle.markSaidExtended(SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL);
		dialogueLifecycle.recordBattleSound(BATTLE_SOUND_OK1, 4000);
		dialogueLifecycle.startHeardNoiseCooldown(2);
		dialogueLifecycle.ageHeardNoiseCooldown();
		dialogueLifecycle.currentCivilianQuote() = 6;
		dialogueLifecycle.civilianQuoteDelta() = 1;
		dialogueLifecycle.recordSpokeAt(5000);
		CHECK( dialogueLifecycle.hasQuoteRecord() &&
		       dialogueLifecycle.hasQuoteAction() &&
		       dialogueLifecycle.hasSaid(SOLDIER_QUOTE_SAID_PERSONALITY) &&
		       dialogueLifecycle.hasSaidExtended(SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL) &&
		       dialogueLifecycle.previousBattleSound() == BATTLE_SOUND_OK1 &&
		       dialogueLifecycle.repeatedBattleSoundAt() == 4000 &&
		       dialogueLifecycle.heardNoiseCooldownTurns() == 1 &&
		       dialogueLifecycle.lastSpokeAt() == 5000,
		       "soldier dialogue component coordinates quote, history, playback, and cooldown transitions" );
		dialogueLifecycle.clearQuotePlan();
		dialogueLifecycle.clearSaid(SOLDIER_QUOTE_SAID_PERSONALITY);
		dialogueLifecycle.clearSaidExtended(SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL);
		dialogueLifecycle.clearCivilianQuote();
		CHECK( !dialogueLifecycle.hasQuoteRecord() &&
		       !dialogueLifecycle.hasQuoteAction() &&
		       !dialogueLifecycle.hasSaid(SOLDIER_QUOTE_SAID_PERSONALITY) &&
		       !dialogueLifecycle.hasSaidExtended(SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL) &&
		       dialogueLifecycle.currentCivilianQuote() == -1 &&
		       dialogueLifecycle.civilianQuoteDelta() == 0,
		       "soldier dialogue component clears coordinated quote plans, history bits, and civilian progression" );
		dialogueLifecycle.reset();
		CHECK( dialogueLifecycle.quoteRecord() == 0 &&
		       dialogueLifecycle.quoteActionId() == 0 &&
		       dialogueLifecycle.battleSoundSet() == 0 &&
		       dialogueLifecycle.saidFlags() == 0 &&
		       dialogueLifecycle.vocalVolume() == 0 &&
		       dialogueLifecycle.repeatedBattleSoundAt() == 0 &&
		       dialogueLifecycle.previousBattleSound() == 0 &&
		       dialogueLifecycle.heardNoiseCooldownTurns() == 0 &&
		       dialogueLifecycle.saidExtendedFlags() == 0 &&
		       dialogueLifecycle.activeBattleSound() == 0 &&
		       dialogueLifecycle.currentCivilianQuote() == 0 &&
		       dialogueLifecycle.civilianQuoteDelta() == 0 &&
		       dialogueLifecycle.lastSpokeAt() == 0 &&
		       dialogueLifecycle.corpseQuoteTolerance() == 0,
		       "soldier dialogue reset clears the complete spoken-state domain" );

		SoldierAudioComponent audioLifecycle;
		CHECK( !audioLifecycle.hasDoorOpeningNoise() &&
		       !audioLifecycle.hasBurstSound() &&
		       !audioLifecycle.hasPositionSound() &&
		       !audioLifecycle.hasTurningSound(),
		       "soldier audio handles begin at the explicit no-sample sentinel" );
		audioLifecycle.recordFootstepVariant(3);
		audioLifecycle.recordDoorOpeningNoise(18);
		audioLifecycle.startBurstSound(401);
		audioLifecycle.startPositionSound(402);
		audioLifecycle.startTurningSound(403);
		CHECK( audioLifecycle.lastFootstepVariant() == 3 &&
		       audioLifecycle.doorOpeningNoise() == 18 &&
		       audioLifecycle.burstSoundId() == 401 &&
		       audioLifecycle.positionSoundId() == 402 &&
		       audioLifecycle.turningSoundId() == 403,
		       "soldier audio records gameplay noise and opaque adapter handles together" );
		audioLifecycle.clearDoorOpeningNoise();
		audioLifecycle.clearBurstSound();
		audioLifecycle.clearPositionSound();
		audioLifecycle.clearTurningSound();
		CHECK( !audioLifecycle.hasDoorOpeningNoise() &&
		       !audioLifecycle.hasBurstSound() &&
		       !audioLifecycle.hasPositionSound() &&
		       !audioLifecycle.hasTurningSound(),
		       "soldier audio closes every independent sound lifecycle through named transitions" );
		audioLifecycle.reset();
		CHECK( audioLifecycle.lastFootstepVariant() == 0 &&
		       audioLifecycle.doorOpeningNoise() == 0 &&
		       audioLifecycle.burstSoundId() == SoldierAudioComponent::NoSample &&
		       audioLifecycle.positionSoundId() == SoldierAudioComponent::NoSample &&
		       audioLifecycle.turningSoundId() == SoldierAudioComponent::NoSample,
		       "soldier audio reset restores gameplay values and handle sentinels" );
		SoldierReplicationComponent replicationLifecycle;
		CHECK( !replicationLifecycle.hasLastUpdate() &&
		       !replicationLifecycle.updateTimedOut(5000, 2000),
		       "soldier replication begins without an observed update timestamp" );
		replicationLifecycle.movementStartedAt() = 410;
		replicationLifecycle.optimumMovementTime() = 420;
		replicationLifecycle.recordUpdate(1000);
		replicationLifecycle.updateSequence() = 430;
		replicationLifecycle.updateType() = 4;
		replicationLifecycle.scheduleStop(440);
		replicationLifecycle.recordChecksum(450);
		CHECK( !replicationLifecycle.updateTimedOut(3000, 2000) &&
		       replicationLifecycle.updateTimedOut(3001, 2000) &&
		       replicationLifecycle.scheduledStopGrid() == 440 &&
		       replicationLifecycle.checksum() == 450,
		       "soldier replication applies an explicit timeout and records synchronization metadata" );
		replicationLifecycle.clearScheduledStop();
		CHECK( replicationLifecycle.scheduledStopGrid() == 0,
		       "soldier replication closes a scheduled synchronization stop through a named transition" );
		replicationLifecycle.reset();
		CHECK( replicationLifecycle.movementStartedAt() == 0 &&
		       replicationLifecycle.optimumMovementTime() == 0 &&
		       !replicationLifecycle.hasLastUpdate() &&
		       replicationLifecycle.updateSequence() == 0 &&
		       replicationLifecycle.updateType() == 0 &&
		       replicationLifecycle.scheduledStopGrid() == 0 &&
		       replicationLifecycle.checksum() == 0,
		       "soldier replication reset clears the complete transport bookkeeping domain" );
		SoldierMovementMetricsComponent movementMetricsLifecycle;
		movementMetricsLifecycle.recordCarriedWeightAtTurnStart(140);
		movementMetricsLifecycle.tilesMoved() =
			SoldierMovementMetricsComponent::MaximumTurnTiles - 1;
		movementMetricsLifecycle.realtimeBreathTiles() =
			SoldierMovementMetricsComponent::MaximumRealtimeBreathTiles - 1;
		movementMetricsLifecycle.recordTileMovement(true, true, RUNNING);
		CHECK( movementMetricsLifecycle.carriedWeightAtTurnStart() == 140 &&
		       movementMetricsLifecycle.tilesMoved() ==
			       SoldierMovementMetricsComponent::MaximumTurnTiles &&
		       movementMetricsLifecycle.realtimeBreathTiles() ==
			       SoldierMovementMetricsComponent::MaximumRealtimeBreathTiles &&
		       movementMetricsLifecycle.lastRealtimeMovementAnimation() == RUNNING,
		       "soldier movement recording saturates both narrow persisted distance counters" );
		movementMetricsLifecycle.recordTileMovement(false, true, WALKING);
		CHECK( movementMetricsLifecycle.tilesMoved() ==
			       SoldierMovementMetricsComponent::MaximumTurnTiles &&
		       movementMetricsLifecycle.realtimeBreathTiles() ==
			       SoldierMovementMetricsComponent::MaximumRealtimeBreathTiles &&
		       movementMetricsLifecycle.lastRealtimeMovementAnimation() == WALKING,
		       "soldier movement saturation does not wrap and still observes the latest realtime animation" );
		movementMetricsLifecycle.clearTurnDistance();
		movementMetricsLifecycle.clearRealtimeBreathMovement();
		CHECK( !movementMetricsLifecycle.movedThisTurn() &&
		       !movementMetricsLifecycle.hasRealtimeBreathMovement(),
		       "soldier movement metrics close turn and realtime cadence windows independently" );
		movementMetricsLifecycle.reset();
		CHECK( movementMetricsLifecycle.carriedWeightAtTurnStart() == 0 &&
		       movementMetricsLifecycle.tilesMoved() == 0 &&
		       movementMetricsLifecycle.realtimeBreathTiles() == 0 &&
		       movementMetricsLifecycle.lastRealtimeMovementAnimation() == 0,
		       "soldier movement metrics reset clears the complete telemetry domain" );
		SoldierAiPlanningComponent aiPlanningLifecycle;
		aiPlanningLifecycle.recordFlankStep(500, 2);
		aiPlanningLifecycle.recordFlankStep(500, 3);
		CHECK( aiPlanningLifecycle.flanking(6) &&
		       aiPlanningLifecycle.flankCount() == 2 &&
		       aiPlanningLifecycle.flankAnchorGrid() == 500 &&
		       aiPlanningLifecycle.flankOriginDirection() == 3,
		       "soldier AI planning advances repeated steps toward one flank anchor" );
		aiPlanningLifecycle.recordFlankStep(501, 4);
		CHECK( aiPlanningLifecycle.flankCount() == 1 &&
		       aiPlanningLifecycle.flankAnchorGrid() == 501 &&
		       aiPlanningLifecycle.flankOriginDirection() == 4,
		       "soldier AI planning starts a new flank when its tactical anchor changes" );
		aiPlanningLifecycle.flankCount() =
			SoldierAiPlanningComponent::MaximumFlankCount - 1;
		aiPlanningLifecycle.advanceFlank();
		aiPlanningLifecycle.advanceFlank();
		CHECK( aiPlanningLifecycle.flankCount() ==
			       SoldierAiPlanningComponent::MaximumFlankCount,
		       "soldier AI planning saturates its narrow persisted flank counter" );
		aiPlanningLifecycle.finishFlank(6);
		aiPlanningLifecycle.raiseSniperPosture();
		CHECK( !aiPlanningLifecycle.flanking(6) &&
		       aiPlanningLifecycle.sniperPostureActive() &&
		       aiPlanningLifecycle.ensurePlanIndex(7) == 7 &&
		       aiPlanningLifecycle.ensurePlanIndex(8) == 7,
		       "soldier AI planning closes flank progress and selects a stable modular plan" );
		aiPlanningLifecycle.clearFlank();
		aiPlanningLifecycle.lowerSniperPosture();
		aiPlanningLifecycle.reset();
		CHECK( aiPlanningLifecycle.flankCount() == 0 &&
		       aiPlanningLifecycle.flankAnchorGrid() == 0 &&
		       !aiPlanningLifecycle.sniperPostureActive() &&
		       aiPlanningLifecycle.flankOriginDirection() == 0 &&
		       !aiPlanningLifecycle.hasPlanIndex(),
		       "soldier AI planning reset clears the complete execution domain" );
		SoldierSkillStateComponent skillStateLifecycle;
		skillStateLifecycle.beginCheck(-5, 700);
		skillStateLifecycle.recordCheckAttempt();
		skillStateLifecycle.selectedAiSkill() = SKILLS_RADIO_JAM;
		skillStateLifecycle.counter(SOLDIER_COUNTER_RADIO_ARTILLERY) = 2;
		skillStateLifecycle.counter(SOLDIER_COUNTER_SPOTTER) = 254;
		skillStateLifecycle.counter(SOLDIER_COUNTER_ROLE_OBSERVED) = 9;
		skillStateLifecycle.cooldown(SOLDIER_COOLDOWN_CRYO) = 2;
		skillStateLifecycle.focusOn(701);
		skillStateLifecycle.ageTurnCounters();
		skillStateLifecycle.decrementCooldown(SOLDIER_COOLDOWN_CRYO);
		CHECK( skillStateLifecycle.isRepeatedCheck(-5, 700) &&
		       skillStateLifecycle.checkAttempts() == 2 &&
		       skillStateLifecycle.selectedAiSkill() == SKILLS_RADIO_JAM &&
		       skillStateLifecycle.counter(SOLDIER_COUNTER_RADIO_ARTILLERY) == 1 &&
		       skillStateLifecycle.counter(SOLDIER_COUNTER_SPOTTER) == 255 &&
		       skillStateLifecycle.counter(SOLDIER_COUNTER_ROLE_OBSERVED) == 9 &&
		       skillStateLifecycle.cooldown(SOLDIER_COOLDOWN_CRYO) == 1 &&
		       skillStateLifecycle.focusGrid() == 701,
		       "soldier skill-state component coordinates repeated checks, counter aging, cooldowns, and focus" );
		skillStateLifecycle.counter(SOLDIER_COUNTER_SPOTTER) = 65535;
		skillStateLifecycle.ageTurnCounters();
		CHECK( skillStateLifecycle.counter(SOLDIER_COUNTER_SPOTTER) == 255,
		       "soldier skill-state spotter aging saturates safely even when persisted input is out of range" );
		skillStateLifecycle.checkAttempts() = SoldierSkillStateComponent::MaximumCheckAttempts;
		skillStateLifecycle.recordCheckAttempt();
		CHECK( skillStateLifecycle.checkAttempts() == SoldierSkillStateComponent::MaximumCheckAttempts,
		       "soldier skill-state repeated-check attempts saturate before signed overflow" );
		skillStateLifecycle.clearCounter(SOLDIER_COUNTER_RADIO_ARTILLERY);
		skillStateLifecycle.clearCooldown(SOLDIER_COOLDOWN_CRYO);
		skillStateLifecycle.clearFocus();
		CHECK( !skillStateLifecycle.hasCounter(SOLDIER_COUNTER_RADIO_ARTILLERY) &&
		       !skillStateLifecycle.hasCooldown(SOLDIER_COOLDOWN_CRYO) &&
		       skillStateLifecycle.focusGrid() == -1,
		       "soldier skill-state component clears independent counters, cooldowns, and focus targets" );
		skillStateLifecycle.counter(SOLDIER_COUNTER_MAX - 1) = 19;
		skillStateLifecycle.cooldown(SOLDIER_COOLDOWN_MAX - 1) = 190;
		skillStateLifecycle.reset();
		CHECK( skillStateLifecycle.lastCheckReason() == 0 &&
		       skillStateLifecycle.checkAttempts() == 0 &&
		       skillStateLifecycle.checkGrid() == 0 &&
		       skillStateLifecycle.selectedAiSkill() == 0 &&
		       skillStateLifecycle.counter(SOLDIER_COUNTER_SPOTTER) == 0 &&
		       skillStateLifecycle.counter(SOLDIER_COUNTER_ROLE_OBSERVED) == 0 &&
		       skillStateLifecycle.counter(SOLDIER_COUNTER_MAX - 1) == 0 &&
		       skillStateLifecycle.cooldown(SOLDIER_COOLDOWN_MAX - 1) == 0 &&
		       skillStateLifecycle.focusGrid() == 0,
		       "soldier skill-state reset clears every persisted field and the full fixed-capacity arrays" );
		SoldierConditionComponent conditionLifecycle;
		conditionLifecycle.extraStrength() = 4;
		conditionLifecycle.extraExperienceLevel() = -1;
		conditionLifecycle.foodLevel() = 50000;
		conditionLifecycle.drinkLevel() = 40000;
		conditionLifecycle.starvationHealthDamage() = 2;
		conditionLifecycle.diseasePoints(0) = 10;
		conditionLifecycle.markDiseaseFlag(0, 0x01);
		conditionLifecycle.markDiseaseFlag(0, 0x02);
		conditionLifecycle.addDisability(0);
		conditionLifecycle.addDisability(1);
		conditionLifecycle.addDisability(SoldierConditionComponent::DisabilityBitCount);
		conditionLifecycle.addDisability(SoldierConditionComponent::DisabilityBitCount + 1);
		CHECK( conditionLifecycle.hasExtraStats() &&
		       conditionLifecycle.hasStarvationDamage() &&
		       conditionLifecycle.infected(0) &&
		       conditionLifecycle.hasDiseaseFlag(0, 0x03) &&
		       conditionLifecycle.hasDisability(1) &&
		       conditionLifecycle.hasDisability(SoldierConditionComponent::DisabilityBitCount) &&
		       !conditionLifecycle.hasDisability(0) &&
		       !conditionLifecycle.hasDisability(SoldierConditionComponent::DisabilityBitCount + 1),
		       "soldier condition component coordinates effect, disease, and validated disability transitions" );
		conditionLifecycle.clearExtraStats();
		conditionLifecycle.clearDiseaseFlags(0, 0x01);
		CHECK( !conditionLifecycle.hasExtraStats() &&
		       !conditionLifecycle.hasDiseaseFlag(0, 0x01) &&
		       conditionLifecycle.hasDiseaseFlag(0, 0x02),
		       "soldier condition component clears temporary stats and selected disease flags without disturbing adjacent state" );
		conditionLifecycle.diseasePoints(NUM_DISEASES - 1) = 20;
		conditionLifecycle.diseaseFlags(NUM_DISEASES - 1) = 0x80;
		conditionLifecycle.reset();
		CHECK( conditionLifecycle.extraStrength() == 0 &&
		       conditionLifecycle.extraExperienceLevel() == 0 &&
		       conditionLifecycle.foodLevel() == 0 &&
		       conditionLifecycle.drinkLevel() == 0 &&
		       !conditionLifecycle.hasStarvationDamage() &&
		       conditionLifecycle.diseasePoints(0) == 0 &&
		       conditionLifecycle.diseaseFlags(0) == 0 &&
		       conditionLifecycle.diseasePoints(NUM_DISEASES - 1) == 0 &&
		       conditionLifecycle.diseaseFlags(NUM_DISEASES - 1) == 0 &&
		       conditionLifecycle.disabilityFlags() == 0,
		       "soldier condition reset clears scalar state and the complete fixed disease capacity" );
		SoldierLongActionComponent longActionLifecycle;
		longActionLifecycle.begin(MTA_HACK, 1700, -5);
		CHECK( longActionLifecycle.active() &&
		       longActionLifecycle.action() == MTA_HACK &&
		       longActionLifecycle.contextGrid() == 1700 &&
		       longActionLifecycle.remainingActionPoints() == 0,
		       "soldier long-action begin keeps its state coherent and clamps invalid negative costs" );
		longActionLifecycle.begin(MTA_REMOVE_FORTIFY, 1701, 40);
		longActionLifecycle.consumeActionPoints(13);
		longActionLifecycle.consumeActionPoints(-4);
		CHECK( longActionLifecycle.remainingActionPoints() == 27,
		       "soldier long-action AP consumption ignores invalid negative input" );
		longActionLifecycle.consumeActionPoints(100);
		longActionLifecycle.rememberContextGrid(1702);
		CHECK( longActionLifecycle.active() &&
		       longActionLifecycle.remainingActionPoints() == 0 &&
		       longActionLifecycle.contextGrid() == 1702,
		       "soldier long-action AP consumption saturates without discarding retained context" );
		longActionLifecycle.clear();
		CHECK( !longActionLifecycle.active() &&
		       longActionLifecycle.action() == MTA_NONE &&
		       longActionLifecycle.contextGrid() == -1 &&
		       longActionLifecycle.remainingActionPoints() == 0,
		       "soldier long-action clear releases action, grid, and AP state atomically" );
		SoldierInteractionComponent interactionLifecycle;
		CHECK( !interactionLifecycle.isNonNpcTrader() &&
		       !interactionLifecycle.dragging() &&
		       interactionLifecycle.draggedPerson() == NOBODY &&
		       interactionLifecycle.draggedCorpse() == -1 &&
		       interactionLifecycle.draggedStructureGrid() == -1 &&
		       !interactionLifecycle.chatting(),
		       "soldier interaction defaults cannot mistake corpse zero for an active drag" );
		interactionLifecycle.dragPerson(SoldierID{ 20 });
		CHECK( interactionLifecycle.draggingPerson() &&
		       interactionLifecycle.draggedPerson() == SoldierID{ 20 } &&
		       !interactionLifecycle.draggingCorpse() &&
		       !interactionLifecycle.draggingStructure(),
		       "soldier interaction begins an exclusive person drag" );
		interactionLifecycle.dragCorpse(21);
		CHECK( !interactionLifecycle.draggingPerson() &&
		       interactionLifecycle.draggingCorpse() &&
		       interactionLifecycle.draggedCorpse() == 21 &&
		       !interactionLifecycle.draggingStructure(),
		       "soldier interaction replaces person dragging with an exclusive corpse drag" );
		interactionLifecycle.dragStructure(0);
		CHECK( !interactionLifecycle.draggingPerson() &&
		       !interactionLifecycle.draggingCorpse() &&
		       interactionLifecycle.draggingStructure() &&
		       interactionLifecycle.draggedStructureGrid() == 0,
		       "soldier interaction treats grid zero as an active exclusive structure drag" );
		interactionLifecycle.dragStructure(1720);
		SoldierInteractionComponent copiedDragLifecycle;
		copiedDragLifecycle.nonNpcTraderId() = 6;
		copiedDragLifecycle.beginChatWith(SoldierID{ 22 });
		copiedDragLifecycle.copyDragFrom(interactionLifecycle);
		CHECK( copiedDragLifecycle.nonNpcTraderId() == 6 &&
		       copiedDragLifecycle.chatPartner() == SoldierID{ 22 } &&
		       copiedDragLifecycle.draggingStructure() &&
		       copiedDragLifecycle.draggedStructureGrid() == 1720,
		       "copying drag context leaves independent merchant and chat relationships intact" );
		interactionLifecycle.beginChatWith(SoldierID{ 23 });
		interactionLifecycle.endChat();
		interactionLifecycle.clearDrag();
		CHECK( !interactionLifecycle.chatting() && !interactionLifecycle.dragging(),
		       "soldier interaction clears chat and drag lifecycles independently" );
		interactionLifecycle.nonNpcTraderId() = 9;
		interactionLifecycle.dragCorpse(24);
		interactionLifecycle.beginChatWith(SoldierID{ 25 });
		interactionLifecycle.reset();
		CHECK( interactionLifecycle.nonNpcTraderId() == 0 &&
		       !interactionLifecycle.dragging() &&
		       interactionLifecycle.draggedCorpse() == -1 &&
		       !interactionLifecycle.chatting(),
		       "soldier interaction reset clears every relationship and restores invalid drag sentinels" );
		SoldierMovementHistoryComponent movementHistoryLifecycle;
		movementHistoryLifecycle.recordDeparture(1719);
		movementHistoryLifecycle.resetAiLoop();
		CHECK( movementHistoryLifecycle.previousGrid() == 1719 &&
		       movementHistoryLifecycle.recentLocations()[0] == -1 &&
		       movementHistoryLifecycle.recentLocations()[1] == -1,
		       "soldier movement-history AI reset preserves departed-grid context and clears both loop slots" );
		CHECK( !movementHistoryLifecycle.observeAiMovement(1720, 1721, 2000) &&
		       movementHistoryLifecycle.recentLocations()[0] == 1720 &&
		       movementHistoryLifecycle.recentLocations()[1] == -1,
		       "soldier movement-history seeds the first valid AI location" );
		CHECK( !movementHistoryLifecycle.observeAiMovement(1721, 1720, 2000) &&
		       movementHistoryLifecycle.recentLocations()[0] == 1720 &&
		       movementHistoryLifecycle.recentLocations()[1] == 1721,
		       "soldier movement-history seeds the second valid AI location" );
		CHECK( movementHistoryLifecycle.observeAiMovement(1720, 1721, 2000) &&
		       movementHistoryLifecycle.recentLocations()[0] == 1720 &&
		       movementHistoryLifecycle.recentLocations()[1] == 1721,
		       "soldier movement-history detects a two-location oscillation without advancing it" );
		CHECK( !movementHistoryLifecycle.observeAiMovement(1722, 1800, 2000) &&
		       movementHistoryLifecycle.recentLocations()[0] == 1721 &&
		       movementHistoryLifecycle.recentLocations()[1] == 1722,
		       "soldier movement-history advances its bounded window for a non-looping move" );
		movementHistoryLifecycle.recentLocations()[0] = 2000;
		movementHistoryLifecycle.recentLocations()[1] = -2;
		CHECK( !movementHistoryLifecycle.observeAiMovement(1730, 1731, 2000) &&
		       movementHistoryLifecycle.recentLocations()[0] == 1730 &&
		       movementHistoryLifecycle.recentLocations()[1] == -2,
		       "soldier movement-history replaces out-of-world legacy values using the supplied world bound" );
		movementHistoryLifecycle.reset();
		CHECK( movementHistoryLifecycle.previousGrid() == 0 &&
		       movementHistoryLifecycle.recentLocations()[0] == 0 &&
		       movementHistoryLifecycle.recentLocations()[1] == 0,
		       "soldier movement-history reset restores the established zero-initialized persistent state" );
		SoldierPendingActionComponent pendingActionLifecycle;
		CHECK( !pendingActionLifecycle.active() &&
		       pendingActionLifecycle.action() == SoldierPendingActionComponent::NoAction &&
		       pendingActionLifecycle.animationCount() == 0,
		       "soldier pending-action state defaults to no selected action" );
		pendingActionLifecycle.begin(MERC_PICKUPITEM);
		pendingActionLifecycle.primaryData() = 41;
		pendingActionLifecycle.secondaryData() = -42;
		pendingActionLifecycle.tertiaryData() = -3;
		pendingActionLifecycle.doorHandleCode() = 4;
		pendingActionLifecycle.quaternaryData() = 43;
		pendingActionLifecycle.nextSpecialData() = -44;
		pendingActionLifecycle.interruptionMarker() = 5;
		pendingActionLifecycle.inventorySlot() = -6;
		pendingActionLifecycle.animationCount() = 255;
		pendingActionLifecycle.recordAnimationTransition();
		CHECK( pendingActionLifecycle.active() &&
		       pendingActionLifecycle.animationCount() == 255,
		       "soldier pending-action transition count saturates instead of wrapping" );
		pendingActionLifecycle.clearAction();
		CHECK( !pendingActionLifecycle.active() &&
		       pendingActionLifecycle.primaryData() == 41 &&
		       pendingActionLifecycle.quaternaryData() == 43,
		       "cancelling a pending action preserves payloads still consumed by completion cleanup" );
		pendingActionLifecycle.clearPayload();
		CHECK( pendingActionLifecycle.primaryData() == 0 &&
		       pendingActionLifecycle.secondaryData() == 0 &&
		       pendingActionLifecycle.tertiaryData() == 0 &&
		       pendingActionLifecycle.doorHandleCode() == 0 &&
		       pendingActionLifecycle.quaternaryData() == 0 &&
		       pendingActionLifecycle.inventorySlot() == 0 &&
		       pendingActionLifecycle.nextSpecialData() == -44 &&
		       pendingActionLifecycle.interruptionMarker() == 5,
		       "clearing the current payload leaves independent queued and interruption state intact" );
		pendingActionLifecycle.reset();
		CHECK( !pendingActionLifecycle.active() &&
		       pendingActionLifecycle.nextSpecialData() == 0 &&
		       pendingActionLifecycle.interruptionMarker() == 0,
		       "soldier pending-action reset clears the complete persistent domain" );
		SoldierCombatContributionComponent contributionLifecycle;
		CHECK( !contributionLifecycle.hasMilitiaCredit() &&
		       contributionLifecycle.militiaPromotionPoints() == 0 &&
		       contributionLifecycle.damageByTeam()[0] == 0 &&
		       contributionLifecycle.damageByTeam()[NUM_ASSIST_SLOTS - 1] == 0,
		       "soldier combat contribution defaults every credit and attribution slot" );
		contributionLifecycle.recordMilitiaKill();
		contributionLifecycle.recordMilitiaAssist();
		contributionLifecycle.damageByTeam()[0] = 43;
		CHECK( contributionLifecycle.militiaKills() == 1 &&
		       contributionLifecycle.militiaAssists() == 1 &&
		       contributionLifecycle.militiaPromotionPoints() == 3,
		       "soldier combat contribution accrues named promotion credit" );
		contributionLifecycle.militiaKills() = 255;
		contributionLifecycle.militiaAssists() = 255;
		contributionLifecycle.recordMilitiaKill();
		contributionLifecycle.recordMilitiaAssist();
		CHECK( contributionLifecycle.militiaKills() == 255 &&
		       contributionLifecycle.militiaAssists() == 255,
		       "soldier combat contribution saturates instead of wrapping long battles" );
		contributionLifecycle.clearMilitiaCredit();
		CHECK( !contributionLifecycle.hasMilitiaCredit() &&
		       contributionLifecycle.damageByTeam()[0] == 43,
		       "transferring militia credit leaves independent assist attribution intact" );
		contributionLifecycle.reset();
		CHECK( !contributionLifecycle.hasMilitiaCredit() &&
		       contributionLifecycle.damageByTeam()[0] == 0,
		       "soldier combat-contribution reset clears the complete persistent domain" );
		vitals.health() = 42;
		vitals.maximumHealth() = 84;
		vitals.breath() = 63;
		vitals.maximumBreath() = 91;
		vitals.bleeding() = 7;
		vitals.previousHealth() = 41;
		vitals.fractionalHealth() = 38;
		vitals.breathReduction() = 421;
		vitals.healableInjury() = 1700;
		vitals.beginSurgery();
		vitals.unregainableBreath() = 223;
		vitals.criticalStatDamage()[DAMAGED_STAT_DEXTERITY] = 5;
		vitals.criticalStatDamage()[DAMAGED_STAT_STRENGTH] = 7;
		vitals.nextBleedAt() = 3.5f;
		vitals.regenerationCounter() = -2;
		vitals.regenerationBoostersUsedToday() = 3;
		vitals.lastBleedGruntAt() = 12341;
		SOLDIERTYPE copiedSoldier = soldier;
		CHECK( copiedSoldier.vitals().health() == 42 &&
		       copiedSoldier.vitals().maximumHealth() == 84 &&
		       copiedSoldier.vitals().breath() == 63 &&
		       copiedSoldier.vitals().maximumBreath() == 91 &&
		       copiedSoldier.vitals().bleeding() == 7 &&
		       copiedSoldier.vitals().previousHealth() == 41 &&
		       copiedSoldier.vitals().fractionalHealth() == 38 &&
		       copiedSoldier.vitals().breathReduction() == 421 &&
		       copiedSoldier.vitals().healableInjury() == 1700 &&
		       copiedSoldier.vitals().isUndergoingSurgery() &&
		       copiedSoldier.vitals().unregainableBreath() == 223 &&
		       copiedSoldier.vitals().criticalStatDamage()[DAMAGED_STAT_DEXTERITY] == 5 &&
		       copiedSoldier.vitals().criticalStatDamage()[DAMAGED_STAT_STRENGTH] == 7 &&
		       copiedSoldier.vitals().nextBleedAt() == 3.5f &&
		       copiedSoldier.vitals().regenerationCounter() == -2 &&
		       copiedSoldier.vitals().regenerationBoostersUsedToday() == 3 &&
		       copiedSoldier.vitals().lastBleedGruntAt() == 12341,
		       "soldier copies retain their owned persistent vitals" );
		CHECK( copiedSoldier.service().activity() == 2 &&
		       copiedSoldier.service().providerCount() == 2 &&
		       copiedSoldier.service().partner() == SoldierID{ 7 } &&
		       copiedSoldier.service().autoBandagingMedic() == SoldierID{ 8 },
		       "soldier copies retain their owned persistent service relationships" );
		CHECK( copiedSoldier.dialogue().quoteRecord() == 13 &&
		       copiedSoldier.dialogue().quoteActionId() == QUOTE_ACTION_ID_CHECKFORDEST &&
		       copiedSoldier.dialogue().battleSoundSet() == 4 &&
		       copiedSoldier.dialogue().hasSaid(SOLDIER_QUOTE_SAID_LOW_BREATH) &&
		       copiedSoldier.dialogue().vocalVolume() == 87 &&
		       copiedSoldier.dialogue().repeatedBattleSoundAt() == 12342 &&
		       copiedSoldier.dialogue().previousBattleSound() == BATTLE_SOUND_ATTN1 &&
		       copiedSoldier.dialogue().heardNoiseCooldownTurns() == 5 &&
		       copiedSoldier.dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL) &&
		       copiedSoldier.dialogue().activeBattleSound() == 77 &&
		       copiedSoldier.dialogue().currentCivilianQuote() == -2 &&
		       copiedSoldier.dialogue().civilianQuoteDelta() == 2 &&
		       copiedSoldier.dialogue().lastSpokeAt() == 12343 &&
		       copiedSoldier.dialogue().corpseQuoteTolerance() == 3,
		       "soldier copies retain their owned persistent dialogue state" );
		CHECK( copiedSoldier.audio().lastFootstepVariant() == 2 &&
		       copiedSoldier.audio().doorOpeningNoise() == 17 &&
		       copiedSoldier.audio().burstSoundId() == 301 &&
		       copiedSoldier.audio().positionSoundId() == 302 &&
		       copiedSoldier.audio().turningSoundId() == 303,
		       "soldier copies retain their owned audio state" );
		CHECK( copiedSoldier.replication().movementStartedAt() == 12344 &&
		       copiedSoldier.replication().optimumMovementTime() == 12345 &&
		       copiedSoldier.replication().lastUpdateAt() == 12346 &&
		       copiedSoldier.replication().updateSequence() == 304 &&
		       copiedSoldier.replication().updateType() == 7 &&
		       copiedSoldier.replication().scheduledStopGrid() == 1235 &&
		       copiedSoldier.replication().checksum() == 305,
		       "soldier copies retain their owned replication state" );
		CHECK( copiedSoldier.movementMetrics().carriedWeightAtTurnStart() == 135 &&
		       copiedSoldier.movementMetrics().tilesMoved() == 7 &&
		       copiedSoldier.movementMetrics().realtimeBreathTiles() == 4 &&
		       copiedSoldier.movementMetrics().lastRealtimeMovementAnimation() == RUNNING,
		       "soldier copies retain their owned movement metrics" );
		CHECK( copiedSoldier.aiPlanning().flankCount() == 3 &&
		       copiedSoldier.aiPlanning().flankAnchorGrid() == 1236 &&
		       copiedSoldier.aiPlanning().sniperPostureActive() &&
		       copiedSoldier.aiPlanning().flankOriginDirection() == 5 &&
		       copiedSoldier.aiPlanning().planIndex() == 4,
		       "soldier copies retain their owned AI planning state" );
		CHECK( copiedSoldier.skillState().lastCheckReason() == -7 &&
		       copiedSoldier.skillState().checkAttempts() == 2 &&
		       copiedSoldier.skillState().checkGrid() == 1234 &&
		       copiedSoldier.skillState().selectedAiSkill() == SKILLS_FOCUS &&
		       copiedSoldier.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY) == 3 &&
		       copiedSoldier.skillState().counter(SOLDIER_COUNTER_MAX - 1) == 19 &&
		       copiedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_CRYO) == 2 &&
		       copiedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_MAX - 1) == 190 &&
		       copiedSoldier.skillState().focusGrid() == 1290,
		       "soldier copies retain their owned persistent skill state" );
		CHECK( copiedSoldier.condition().extraStrength() == 11 &&
		       copiedSoldier.condition().extraDexterity() == -12 &&
		       copiedSoldier.condition().extraAgility() == 13 &&
		       copiedSoldier.condition().extraWisdom() == -14 &&
		       copiedSoldier.condition().extraExperienceLevel() == 2 &&
		       copiedSoldier.condition().foodLevel() == 54000 &&
		       copiedSoldier.condition().drinkLevel() == -24000 &&
		       copiedSoldier.condition().starvationHealthDamage() == 7 &&
		       copiedSoldier.condition().starvationStrengthDamage() == 8 &&
		       copiedSoldier.condition().diseasePoints(0) == 101 &&
		       copiedSoldier.condition().diseaseFlags(0) == 0x03 &&
		       copiedSoldier.condition().diseasePoints(NUM_DISEASES - 1) == 303 &&
		       copiedSoldier.condition().diseaseFlags(NUM_DISEASES - 1) == 0x80 &&
		       copiedSoldier.condition().hasDisability(2) &&
		       copiedSoldier.condition().hasDisability(SoldierConditionComponent::DisabilityBitCount),
		       "soldier copies retain their owned persistent condition state" );
		CHECK( copiedSoldier.longAction().active() &&
		       copiedSoldier.longAction().action() == MTA_FORTIFY &&
		       copiedSoldier.longAction().contextGrid() == 1300 &&
		       copiedSoldier.longAction().remainingActionPoints() == 37,
		       "soldier copies retain their owned persistent long-action state" );
		CHECK( copiedSoldier.interaction().nonNpcTraderId() == 7 &&
		       copiedSoldier.interaction().draggingPerson() &&
		       copiedSoldier.interaction().draggedPerson() == SoldierID{ 17 } &&
		       copiedSoldier.interaction().chatPartner() == SoldierID{ 18 },
		       "soldier copies retain their owned persistent interaction state" );
		CHECK( copiedSoldier.pendingAction().action() == MERC_GIVEITEM &&
		       copiedSoldier.pendingAction().animationCount() == 9 &&
		       copiedSoldier.pendingAction().primaryData() == 1301 &&
		       copiedSoldier.pendingAction().secondaryData() == -1302 &&
		       copiedSoldier.pendingAction().tertiaryData() == -3 &&
		       copiedSoldier.pendingAction().doorHandleCode() == 4 &&
		       copiedSoldier.pendingAction().quaternaryData() == 1303 &&
		       copiedSoldier.pendingAction().nextSpecialData() == -1304 &&
		       copiedSoldier.pendingAction().interruptionMarker() == 5 &&
		       copiedSoldier.pendingAction().inventorySlot() == -6,
		       "soldier copies retain their owned persistent pending-action state" );
		CHECK( copiedSoldier.actionPoints().current() == 43 &&
		       copiedSoldier.actionPoints().initial() == 78,
		       "soldier copies retain their owned persistent action-point budget" );
		CHECK( copiedSoldier.collapseState().collapsed() &&
		       copiedSoldier.collapseState().breathCollapsed() &&
		       copiedSoldier.collapseState().turns() == 2 &&
		       copiedSoldier.collapseState().sleepDrugCounter() == 7 &&
		       copiedSoldier.collapseState().fatigueCollapsed(),
		       "soldier copies retain their owned persistent collapse state" );
		CHECK( copiedSoldier.perception().hasHeardMovementFrom(2) &&
		       copiedSoldier.perception().hasHeardMovementFrom(6) &&
		       copiedSoldier.perception().viewRange() == 14 &&
		       copiedSoldier.perception().blindnessTurns() == 5 &&
		       copiedSoldier.perception().heardNoiseLevel() == 1 &&
		       copiedSoldier.perception().xrayActivatedAt() == 12345 &&
		       copiedSoldier.perception().deafnessTurns() == 3,
		       "soldier copies retain their owned persistent perception state" );
		CHECK( copiedSoldier.awareness().visibleNow() &&
		       copiedSoldier.awareness().lastRenderedVisibility() == -1 &&
		       copiedSoldier.awareness().newOpponentCount() == 2 &&
		       copiedSoldier.awareness().tilesSinceForget() == 201,
		       "soldier copies retain their owned persistent awareness state" );
		CHECK( copiedSoldier.camouflage().jungleApplied() == 25 &&
		       copiedSoldier.camouflage().jungleWorn() == 30 &&
		       copiedSoldier.camouflage().urbanApplied() == 20 &&
		       copiedSoldier.camouflage().urbanWorn() == 45 &&
		       copiedSoldier.camouflage().desertApplied() == -20 &&
		       copiedSoldier.camouflage().desertWorn() == -90 &&
		       copiedSoldier.camouflage().snowApplied() == 15 &&
		       copiedSoldier.camouflage().snowWorn() == 10,
		       "soldier copies retain their owned persistent camouflage state" );
		CHECK( copiedSoldier.employment().endTime() == 12000 &&
		       copiedSoldier.employment().startTime() == 17 &&
		       copiedSoldier.employment().totalLength() == 14 &&
		       copiedSoldier.employment().mercenaryType() == MERC_TYPE__AIM_MERC &&
		       copiedSoldier.employment().medicalDeposit() == 600 &&
		       copiedSoldier.employment().lifeInsurance() == 1 &&
		       copiedSoldier.employment().insuranceStartDay() == 4 &&
		       copiedSoldier.employment().insuranceLengthDays() == 8 &&
		       copiedSoldier.employment().lastContractUpdateTime() == 12001 &&
		       copiedSoldier.employment().lastContractType() == CONTRACT_EXTEND_2_WEEK &&
		       copiedSoldier.employment().justFired() == 1 &&
		       copiedSoldier.employment().renewalQuoteCode() == SOLDIER_CONTRACT_RENEW_QUOTE_89_USED &&
		       copiedSoldier.employment().timeCanSignElsewhere() == 13000 &&
		       copiedSoldier.employment().hospitalPriceModifier() == -2 &&
		       copiedSoldier.employment().insuranceStartTime() == 11000,
		       "soldier copies retain their owned persistent employment state" );
		CHECK( copiedSoldier.assignment().current() == TRAIN_TEAMMATE &&
		       copiedSoldier.assignment().previous() == ON_DUTY &&
		       copiedSoldier.assignment().trainingStat() == STRENGTH &&
		       copiedSoldier.assignment().lastChangeMinute() == 11900 &&
		       copiedSoldier.assignment().desiredSquad() == 3 &&
		       copiedSoldier.assignment().mergeTraversalAllowance() == 4 &&
		       copiedSoldier.assignment().hours() == 6 &&
		       copiedSoldier.assignment().repairVehicleId() == 2 &&
		       copiedSoldier.assignment().facilityType() == 5 &&
		       copiedSoldier.assignment().itemMoveSectorId() == 47 &&
		       copiedSoldier.assignment().miniEventHoursRemaining() == 12,
		       "soldier copies retain their owned persistent assignment state" );
		CHECK( copiedSoldier.deployment().insertionDirection() == -3 &&
		       copiedSoldier.deployment().groupId() == 42 &&
		       copiedSoldier.deployment().insertionGrid() == 2200 &&
		       copiedSoldier.deployment().strategicInsertionCode() == INSERTION_CODE_GRIDNO &&
		       copiedSoldier.deployment().strategicInsertionData() == 2201 &&
		       copiedSoldier.deployment().isInSector(9, 4, 1) &&
		       copiedSoldier.deployment().vehicleId() == 7 &&
		       copiedSoldier.deployment().offWorldGrid() == 2202 &&
		       copiedSoldier.deployment().previousSectorId() == 31 &&
		       copiedSoldier.deployment().useExitGridForReentryDirection() == 1 &&
		       copiedSoldier.deployment().preTraversalGrid() == 2203 &&
		       copiedSoldier.deployment().leaveHistoryCode() == 6 &&
		       copiedSoldier.deployment().arrivalTime() == 14000 &&
		       copiedSoldier.deployment().arrivalGetupPending() &&
		       copiedSoldier.deployment().ignoreCollapseGetupCheck() &&
		       copiedSoldier.deployment().arrivalGetupCounter() == 15000,
		       "soldier copies retain their owned persistent deployment state" );
		CHECK( copiedSoldier.schedule().id() == 37 &&
		       copiedSoldier.schedule().progress() == 2 &&
		       copiedSoldier.schedule().doorAnimationComplete() &&
		       copiedSoldier.schedule().doorGrid() == 2204,
		       "soldier copies retain their owned persistent schedule state" );
		CHECK( copiedSoldier.position().worldX() == 123.75f &&
		       copiedSoldier.position().worldY() == 456.25f &&
		       copiedSoldier.position().worldXInt() == 123 &&
		       copiedSoldier.position().worldYInt() == 456 &&
		       copiedSoldier.position().turnStartX() == 120 &&
		       copiedSoldier.position().turnStartY() == 450 &&
		       copiedSoldier.position().initialGrid() == 1200 &&
		       copiedSoldier.position().gridNo() == 1234 &&
		       copiedSoldier.position().level() == 1 &&
		       copiedSoldier.position().direction() == 6 &&
		       copiedSoldier.position().heightAdjustment() == 17 &&
		       copiedSoldier.position().desiredHeight() == 25 &&
		       copiedSoldier.position().temporaryGrid() == 1236 &&
		       copiedSoldier.position().roomNo() == 8 &&
		       copiedSoldier.position().terrainType() == LOW_GRASS &&
		       copiedSoldier.position().previousTerrainType() == PAVED_ROAD,
		       "soldier copies retain their complete owned persistent position" );
		CHECK( copiedSoldier.movementHistory().previousGrid() == 1233 &&
		       copiedSoldier.movementHistory().recentLocations()[0] == 1220 &&
		       copiedSoldier.movementHistory().recentLocations()[1] == 1221,
		       "soldier copies retain their owned persistent movement history" );
		CHECK( copiedSoldier.pathing().desiredDirection() == 7 &&
		       copiedSoldier.pathing().destinationX() == 14 &&
		       copiedSoldier.pathing().destinationY() == 28 &&
		       copiedSoldier.pathing().destinationGrid() == 1235 &&
		       copiedSoldier.pathing().finalDestinationGrid() == 1240 &&
		       copiedSoldier.pathing().stopped() == 1 &&
		       copiedSoldier.pathing().needsLook() == 1 &&
		       copiedSoldier.pathing().path()[1] == 3 &&
		       copiedSoldier.pathing().pathSize() == 2 &&
		       copiedSoldier.pathing().pathIndex() == 1 &&
		       copiedSoldier.pathing().blackListGrid() == 1300 &&
		       copiedSoldier.pathing().stored() == 1,
		       "soldier copies retain their owned persistent pathing state" );
		CHECK( copiedSoldier.movement().mode() == SWATTING &&
		       copiedSoldier.movement().stealthy() &&
		       copiedSoldier.movement().reversing() &&
		       copiedSoldier.movement().highResolutionDirection() == 11 &&
		       copiedSoldier.movement().highResolutionDesiredDirection() == 13 &&
		       copiedSoldier.movement().animationDirection() == 5 &&
		       copiedSoldier.movement().gridUpdatePolicy() == 1 &&
		       copiedSoldier.movement().delayCounter() == 9 &&
		       copiedSoldier.movement().delayedCauseGrid() == 1250 &&
		       copiedSoldier.movement().reservedGrid() == 1251 &&
		       copiedSoldier.movement().blockedByAnotherMerc() &&
		       copiedSoldier.movement().blockedDirection() == 3 &&
		       copiedSoldier.movement().absoluteDestination() == 1260 &&
		       copiedSoldier.movement().continuedPathValid() &&
		       copiedSoldier.movement().continuedPathGrid() == 1270 &&
		       copiedSoldier.movement().delayedFlags() == 5 &&
		       copiedSoldier.movement().stopReason() == 2 &&
		       copiedSoldier.movement().usesMoveSpeedOverride() &&
		       copiedSoldier.movement().moveSpeedOverride() == SoldierID{ 4 } &&
		       copiedSoldier.movement().turnActive() &&
		       copiedSoldier.movement().wasInWater() &&
		       copiedSoldier.movement().uiMovementFast() == 2 &&
		       copiedSoldier.movement().outOfActionPoints() &&
		       copiedSoldier.movement().movementPaused() &&
		       copiedSoldier.movement().recordingMovement() &&
		       copiedSoldier.movement().delayedByNetwork() &&
		       copiedSoldier.movement().wasMoving() &&
		       copiedSoldier.movement().crossedDestinationCenter(),
		       "soldier copies retain their owned persistent movement state" );
		CHECK( copiedSoldier.interruptSnapshot().movedBeforeInterrupt() == 1,
		       "soldier copies retain their owned interrupt snapshot" );
		CHECK( copiedSoldier.targeting().gridNo() == 1280 &&
		       copiedSoldier.targeting().level() == 1 &&
		       copiedSoldier.targeting().cubeLevel() == 3 &&
		       copiedSoldier.targeting().lastGridNo() == 1279 &&
		       copiedSoldier.targeting().targetId() == SoldierID{ 5 },
		       "soldier copies retain their owned persistent targeting state" );
		CHECK( copiedSoldier.attackSelection().hand() == SECONDHANDPOS &&
		       copiedSoldier.attackSelection().weapon() == 321 &&
		       copiedSoldier.attackSelection().weaponMode() == WM_ATTACHED_UB_AUTO &&
		       copiedSoldier.attackSelection().scopeMode() == USE_ALT_WEAPON_HOLD &&
		       copiedSoldier.attackSelection().shotLocation() == AIM_SHOT_HEAD &&
		       copiedSoldier.attackSelection().meleeLocation() == AIM_SHOT_LEGS,
		       "soldier copies retain their owned persistent attack selection" );
		CHECK( copiedSoldier.meleeApproach().matches(1275, SWATTING) &&
		       copiedSoldier.meleeApproach().cost() == 11 &&
		       copiedSoldier.meleeApproach().endDirection() == 6,
		       "soldier copies retain their owned melee-approach cache" );
		CHECK( copiedSoldier.fireControl().burstCounter() == 1 &&
		       copiedSoldier.fireControl().autofireShots() == 7 &&
		       copiedSoldier.fireControl().bulletsLeft() == 3 &&
		       copiedSoldier.fireControl().spreadIndex() == 2 &&
		       copiedSoldier.fireControl().autofireLastStep() &&
		       copiedSoldier.fireControl().spreadLocations()[0] == 1281 &&
		       copiedSoldier.fireControl().spreadLocations()[5] == 1286 &&
		       copiedSoldier.fireControl().previousMuzzleOffsetY()[1] == -3.0f &&
		       copiedSoldier.fireControl().previousCounterForceX()[1] == 1.0f &&
		       copiedSoldier.fireControl().initialMuzzleOffsetX() == 4.0f &&
		       copiedSoldier.fireControl().barrelCounter() == 2 &&
		       copiedSoldier.fireControl().spreadDragStartGrid() == 1287 &&
		       copiedSoldier.fireControl().spreadDragEndGrid() == 1288,
		       "soldier copies retain their owned persistent fire-control state" );
		CHECK( copiedSoldier.combatResult().currentAttacker() == SoldierID{ 6 } &&
		       copiedSoldier.combatResult().previousAttacker() == SoldierID{ 5 } &&
		       copiedSoldier.combatResult().earlierAttacker() == SoldierID{ 4 } &&
		       copiedSoldier.combatResult().hitLocation() == AIM_SHOT_HEAD &&
		       copiedSoldier.combatResult().lastDamageReason() == 7 &&
		       copiedSoldier.combatResult().hitsThisTurn() == 3 &&
		       copiedSoldier.combatResult().pelletsHitBy() == 4 &&
		       copiedSoldier.combatResult().accumulatedDamage() == 27,
		       "soldier copies retain their owned persistent combat-result state" );
		CHECK( copiedSoldier.combatContribution().militiaKills() == 2 &&
		       copiedSoldier.combatContribution().militiaAssists() == 1 &&
		       copiedSoldier.combatContribution().damageByTeam()[0] == 41 &&
		       copiedSoldier.combatContribution().damageByTeam()[NUM_ASSIST_SLOTS - 1] == 42,
		       "soldier copies retain their owned persistent combat-contribution state" );
		CHECK( copiedSoldier.damageDisplay().displaying() &&
		       copiedSoldier.damageDisplay().counter() == 2 &&
		       copiedSoldier.damageDisplay().offsetX() == 13 &&
		       copiedSoldier.damageDisplay().offsetY() == -9 &&
		       copiedSoldier.damageDisplay().direction() == -1,
		       "soldier copies retain their owned persistent damage-display state" );
		CHECK( std::strcmp(copiedSoldier.renderState().headPalette(), "BROWNHEAD") == 0 &&
		       copiedSoldier.renderState().fadeMode() == 2 &&
		       copiedSoldier.renderState().fadeLevel() == 17 &&
		       copiedSoldier.renderState().fadeOriginGrid() == 1301 &&
		       copiedSoldier.renderState().forceShade() &&
		       copiedSoldier.renderState().muzzleFlashVisible() &&
		       copiedSoldier.renderState().unblitWidth() == 13 &&
		       copiedSoldier.renderState().lightSprite() == 501 &&
		       copiedSoldier.renderState().muzzleFlashSprite() == 502 &&
		       copiedSoldier.renderState().muzzleFlashFrame() == 3 &&
		       copiedSoldier.renderState().boundingBoxOffsetY() == -4,
		       "soldier copies retain their owned persistent render state" );
		CHECK( copiedSoldier.uiPresentation().portraitFlashFrame() == 3 &&
		       copiedSoldier.uiPresentation().locatorFrame() == 2 &&
		       copiedSoldier.uiPresentation().locatorOffsetX() == 7 &&
		       copiedSoldier.uiPresentation().locatorOffsetY() == -8 &&
		       copiedSoldier.uiPresentation().interfaceLevel() == 1 &&
		       copiedSoldier.uiPresentation().closePanelFrame() == 2 &&
		       copiedSoldier.uiPresentation().deadPanelFrame() == 3 &&
		       copiedSoldier.uiPresentation().openPanelFrame() == 4 &&
		       copiedSoldier.uiPresentation().panelFaceX() == 91 &&
		       copiedSoldier.uiPresentation().panelFaceY() == 92 &&
		       copiedSoldier.uiPresentation().plannedActionPointCost() == 12 &&
		       copiedSoldier.uiPresentation().plannedTargetX() == 500 &&
		       copiedSoldier.uiPresentation().plannedTargetY() == 600 &&
		       copiedSoldier.uiPresentation().lastEnemyCycled() == SoldierID{ 9 } &&
		       copiedSoldier.uiPresentation().locateCycles() == 5,
		       "soldier copies retain their owned UI presentation state" );
		CHECK( copiedSoldier.suppression().underFire() == 2 &&
		       copiedSoldier.suppression().shock() == 9 &&
		       copiedSoldier.suppression().points() == 4 &&
		       copiedSoldier.suppression().actionPointsLost() == 6 &&
		       copiedSoldier.suppression().suppressor() == SoldierID{ 8 } &&
		       copiedSoldier.suppression().closeCall(),
		       "soldier copies retain their owned persistent suppression state" );
		CHECK( copiedSoldier.animationIntent().desiredHeight() == ANIM_CROUCH &&
		       copiedSoldier.animationIntent().pendingAnimation() == WALKING &&
		       copiedSoldier.animationIntent().pendingStance() == ANIM_PRONE &&
		       copiedSoldier.animationIntent().secondaryPendingAnimation() == RUNNING &&
		       copiedSoldier.animationIntent().pendingDirection() == 4 &&
		       copiedSoldier.animationIntent().turningFromUi() &&
		       copiedSoldier.animationIntent().stopPendingNextTile() &&
		       copiedSoldier.animationIntent().continuationMode() == 2,
		       "soldier copies retain their owned persistent animation intent" );
		CHECK( copiedSoldier.animationPlayback().state() == RUNNING &&
		       copiedSoldier.animationPlayback().code() == 17 &&
		       copiedSoldier.animationPlayback().frame() == 23 &&
		       copiedSoldier.animationPlayback().delay() == -7 &&
		       copiedSoldier.animationPlayback().previousState() == WALKING &&
		       copiedSoldier.animationPlayback().previousCode() == 11 &&
		       copiedSoldier.animationPlayback().surface() == 321 &&
		       copiedSoldier.animationPlayback().zLevel() == 654 &&
		       copiedSoldier.animationPlayback().subFlags() == 0x10203040,
		       "soldier copies retain their owned persistent animation playback" );
		CHECK( copiedSoldier.animationActivity().turningFromProneMode() == TURNING_FROM_PRONE_ON &&
		       copiedSoldier.animationActivity().readyCostWaived() &&
		       copiedSoldier.animationActivity().postHitStance() == GO_TO_AIM_AFTER_HIT &&
		       copiedSoldier.animationActivity().paused() &&
		       copiedSoldier.animationActivity().holdAttackerUntilDone() &&
		       copiedSoldier.animationActivity().turningToShoot() &&
		       copiedSoldier.animationActivity().turningToFall() &&
		       copiedSoldier.animationActivity().turningUntilDone() &&
		       copiedSoldier.animationActivity().hitPhase() == 2 &&
		       copiedSoldier.animationActivity().nonInterruptible() &&
		       copiedSoldier.animationActivity().turningCostWaived() &&
		       copiedSoldier.animationActivity().suppressionStanceChange() &&
		       copiedSoldier.animationActivity().stanceCostWaived() &&
		       copiedSoldier.animationActivity().realtimeNonInterruptible() &&
		       copiedSoldier.animationActivity().tryingToFall() &&
		       copiedSoldier.animationActivity().fallClockwise() &&
		       copiedSoldier.animationActivity().fallDirection() == 6 &&
		       copiedSoldier.animationActivity().turningIncrement() == -1 &&
		       copiedSoldier.animationActivity().traversalForecastGrid() == 1290 &&
		       copiedSoldier.animationActivity().renderZOverride() == 999 &&
		       copiedSoldier.animationActivity().randomActionCheckCounter() == 42 &&
		       copiedSoldier.animationActivity().lastRandomAnimation() == 123,
		       "soldier copies retain their owned persistent animation activity" );
		copiedSoldier.pathing().clearRoute();
		CHECK( copiedSoldier.pathing().empty() &&
		       copiedSoldier.pathing().complete() &&
		       copiedSoldier.pathing().pathIndex() == 0 &&
		       copiedSoldier.pathing().stored() == 0,
		       "soldier pathing component clears route cursors and cached-route state together" );
		copiedSoldier.movement().clearDelay();
		copiedSoldier.movement().clearBlock();
		copiedSoldier.movement().clearContinuedPath();
		copiedSoldier.movement().clearMoveSpeedOverride();
		copiedSoldier.movement().setStealth(false);
		copiedSoldier.movement().setReverse(false);
		copiedSoldier.movement().setHighResolutionFacing(2, 3);
		copiedSoldier.movement().clearGridUpdatePolicy();
		copiedSoldier.movement().finishTurn();
		copiedSoldier.movement().rememberWaterState(false);
		copiedSoldier.movement().clearUiMovementFast();
		copiedSoldier.movement().setOutOfActionPoints(false);
		copiedSoldier.movement().resumeMovement();
		copiedSoldier.movement().stopMovementClock();
		copiedSoldier.movement().setNetworkDelayed(false);
		copiedSoldier.movement().syncPresentationMotion(false);
		copiedSoldier.movement().clearPastDestination();
		copiedSoldier.animationIntent().clearDesiredHeight();
		copiedSoldier.animationIntent().clearFacingAnimation();
		copiedSoldier.animationIntent().queueAnimation( WALKING );
		copiedSoldier.animationIntent().queueSecondaryAnimation( RUNNING );
		copiedSoldier.animationIntent().clearPendingAnimations();
		copiedSoldier.animationIntent().clearPendingStance();
		copiedSoldier.animationIntent().clearTurningFromUi();
		copiedSoldier.animationIntent().clearStopAtNextTile();
		copiedSoldier.animationIntent().clearContinuation();
		copiedSoldier.animationActivity().resume();
		copiedSoldier.animationActivity().clearHit();
		copiedSoldier.animationActivity().clearInterruptibility();
		copiedSoldier.animationActivity().clearFall();
		CHECK( !copiedSoldier.movement().delayed() &&
		       !copiedSoldier.movement().blockedByAnotherMerc() &&
		       !copiedSoldier.movement().continuedPathValid() &&
		       !copiedSoldier.movement().usesMoveSpeedOverride() &&
		       !copiedSoldier.movement().stealthy() &&
		       !copiedSoldier.movement().reversing() &&
		       copiedSoldier.movement().highResolutionDirection() == 2 &&
		       copiedSoldier.movement().highResolutionDesiredDirection() == 3 &&
		       copiedSoldier.movement().gridUpdatePolicy() == 0 &&
		       !copiedSoldier.movement().turnActive() &&
		       !copiedSoldier.movement().wasInWater() &&
		       !copiedSoldier.movement().fastUiMovement() &&
		       !copiedSoldier.movement().outOfActionPoints() &&
		       !copiedSoldier.movement().movementPaused() &&
		       !copiedSoldier.movement().recordingMovement() &&
		       !copiedSoldier.movement().delayedByNetwork() &&
		       !copiedSoldier.movement().wasMoving() &&
		       !copiedSoldier.movement().crossedDestinationCenter(),
		       "soldier movement component clears coordinated movement activity through named transitions" );
		CHECK( !copiedSoldier.animationIntent().hasDesiredHeight() &&
		       !copiedSoldier.animationIntent().hasPendingAnimation() &&
		       !copiedSoldier.animationIntent().hasSecondaryPendingAnimation() &&
		       !copiedSoldier.animationIntent().hasPendingStance() &&
		       !copiedSoldier.animationIntent().hasPendingDirection() &&
		       !copiedSoldier.animationIntent().turningFromUi() &&
		       !copiedSoldier.animationIntent().stopPendingNextTile() &&
		       !copiedSoldier.animationIntent().continuesAfterStance(),
		       "soldier animation-intent component clears coordinated transition modes through named operations" );
		CHECK( !copiedSoldier.animationActivity().paused() &&
		       !copiedSoldier.animationActivity().gettingHit() &&
		       !copiedSoldier.animationActivity().nonInterruptible() &&
		       !copiedSoldier.animationActivity().realtimeNonInterruptible() &&
		       !copiedSoldier.animationActivity().tryingToFall(),
		       "soldier animation-activity component clears coordinated lifecycle modes through named operations" );
		SoldierCombatResultComponent attackerHistory;
		attackerHistory.previousAttacker() = SoldierID{ 6 };
		attackerHistory.earlierAttacker() = SoldierID{ 5 };
		attackerHistory.recordHit(SoldierID{ 7 }, AIM_SHOT_LEGS);
		attackerHistory.advanceAttackerHistory(false);
		CHECK( attackerHistory.currentAttacker() == NOBODY &&
		       attackerHistory.previousAttacker() == SoldierID{ 7 } &&
		       attackerHistory.earlierAttacker() == SoldierID{ 6 } &&
		       attackerHistory.hitLocation() == AIM_SHOT_LEGS,
		       "combat-result history advances attribution and optionally releases the current attacker atomically" );
		attackerHistory.restorePreviousAttacker();
		attackerHistory.advanceAttackerHistory(false);
		CHECK( attackerHistory.currentAttacker() == NOBODY &&
		       attackerHistory.previousAttacker() == SoldierID{ 7 } &&
		       attackerHistory.earlierAttacker() == SoldierID{ 6 },
		       "repeated hits by one attacker preserve the distinct earlier assister" );
		attackerHistory.restorePreviousAttacker();
		CHECK( attackerHistory.currentAttacker() == SoldierID{ 7 },
		       "combat-result history can restore the killer after a delayed death" );

		SoldierDamageDisplayComponent displayLifecycle;
		displayLifecycle.activateAt(20, -3);
		for (INT8 i = 0; i < 8; ++i)
			displayLifecycle.advance();
		CHECK( displayLifecycle.expired() &&
		       displayLifecycle.counter() == 8 &&
		       displayLifecycle.offsetX() == 28 &&
		       displayLifecycle.offsetY() == -11,
		       "damage-display lifecycle advances its counter and screen offsets together" );
		displayLifecycle.clear();
		CHECK( !displayLifecycle.displaying() && displayLifecycle.counter() == 0,
		       "damage-display lifecycle clears its active cursor atomically" );

		SoldierRenderStateComponent renderLifecycle;
		renderLifecycle.beginFade(2, 18, 1401);
		renderLifecycle.setUnblitRect(21, 22, 23, 24);
		renderLifecycle.setBoundingBox(51, 52, -5, -6);
		renderLifecycle.lightSprite() = 601;
		renderLifecycle.startMuzzleFlashSprite(602);
		renderLifecycle.advanceMuzzleFlashFrame();
		renderLifecycle.showMuzzleFlash();
		renderLifecycle.enableForceShade();
		CHECK( renderLifecycle.fading() &&
		       renderLifecycle.fadeMode() == 2 &&
		       renderLifecycle.fadeLevel() == 18 &&
		       renderLifecycle.fadeOriginGrid() == 1401 &&
		       renderLifecycle.hasLightSprite() &&
		       renderLifecycle.hasMuzzleFlashSprite() &&
		       renderLifecycle.muzzleFlashFrame() == 2 &&
		       renderLifecycle.muzzleFlashExpired(1) &&
		       renderLifecycle.muzzleFlashVisible() &&
		       renderLifecycle.forceShade() &&
		       renderLifecycle.unblitHeight() == 24 &&
		       renderLifecycle.boundingBoxOffsetY() == -6,
		       "render-state transitions coordinate fade, light, redraw, and screen geometry" );
		renderLifecycle.finishFade();
		renderLifecycle.clearMuzzleFlashSprite();
		renderLifecycle.clearLightSprite();
		renderLifecycle.hideMuzzleFlash();
		renderLifecycle.disableForceShade();
		CHECK( !renderLifecycle.fading() &&
		       !renderLifecycle.hasLightSprite() &&
		       !renderLifecycle.hasMuzzleFlashSprite() &&
		       renderLifecycle.muzzleFlashFrame() == 0 &&
		       !renderLifecycle.muzzleFlashVisible() &&
		       !renderLifecycle.forceShade(),
		       "render-state transitions clear completed fade and light lifecycles atomically" );
		std::strcpy(renderLifecycle.headPalette(), "STALE");
		renderLifecycle.forceRenderColor() = TRUE;
		renderLifecycle.reset();
		CHECK( renderLifecycle.headPalette()[0] == '\0' &&
		       !renderLifecycle.forceRenderColor() &&
		       !renderLifecycle.hasLightSprite() &&
		       !renderLifecycle.hasMuzzleFlashSprite(),
		       "render-state reset restores empty palettes and explicit no-light sentinels" );

		SoldierUiPresentationComponent uiPresentationLifecycle;
		uiPresentationLifecycle.startLocator(7);
		uiPresentationLifecycle.setLocatorOffset(10, -11);
		uiPresentationLifecycle.setPanelFacePosition(120, 121);
		uiPresentationLifecycle.setPlannedTarget(700, 701, 22);
		uiPresentationLifecycle.clearPlannedTarget();
		CHECK( uiPresentationLifecycle.locatorFrame() == 0 &&
		       uiPresentationLifecycle.locateCycles() == 7 &&
		       uiPresentationLifecycle.locatorOffsetX() == 10 &&
		       uiPresentationLifecycle.locatorOffsetY() == -11 &&
		       uiPresentationLifecycle.panelFaceX() == 120 &&
		       uiPresentationLifecycle.panelFaceY() == 121 &&
		       !uiPresentationLifecycle.hasPlannedTarget() &&
		       uiPresentationLifecycle.plannedTargetX() == -1 &&
		       uiPresentationLifecycle.plannedTargetY() == -1 &&
		       uiPresentationLifecycle.plannedActionPointCost() == 22,
		       "UI-presentation transitions coordinate locator, panel, and planned-target state" );
		uiPresentationLifecycle.reset();
		CHECK( uiPresentationLifecycle.locatorFrame() == 0 &&
		       uiPresentationLifecycle.locateCycles() == 0 &&
		       uiPresentationLifecycle.plannedTargetX() == 0 &&
		       uiPresentationLifecycle.plannedTargetY() == 0,
		       "UI-presentation reset restores the established fresh-soldier defaults" );

		SoldierMeleeApproachComponent meleeApproachLifecycle;
		meleeApproachLifecycle.recordPath(SWATTING, 13, 5);
		meleeApproachLifecycle.rememberGrid(1400);
		CHECK( meleeApproachLifecycle.matches(1400, SWATTING) &&
		       meleeApproachLifecycle.cost() == 13 &&
		       meleeApproachLifecycle.endDirection() == 5,
		       "melee-approach cache coordinates its path key and calculated result" );
		meleeApproachLifecycle.clearCost();
		meleeApproachLifecycle.invalidate();
		CHECK( !meleeApproachLifecycle.matches(1400, SWATTING) &&
		       meleeApproachLifecycle.cost() == 0 &&
		       meleeApproachLifecycle.movementMode() == SWATTING &&
		       meleeApproachLifecycle.endDirection() == 5,
		       "melee-approach invalidation preserves the historical partial-cache semantics" );
		meleeApproachLifecycle.reset();
		CHECK( meleeApproachLifecycle.grid() == 0 &&
		       meleeApproachLifecycle.movementMode() == 0 &&
		       meleeApproachLifecycle.cost() == 0 &&
		       meleeApproachLifecycle.endDirection() == 0,
		       "melee-approach reset restores established fresh-soldier defaults" );

		SoldierFireControlComponent spreadDragLifecycle;
		spreadDragLifecycle.beginSpreadDrag(1500);
		spreadDragLifecycle.updateSpreadDrag(1500);
		CHECK( !spreadDragLifecycle.spreadDragMoved(),
		       "burst-spread gesture distinguishes a stationary click" );
		spreadDragLifecycle.updateSpreadDrag(1501);
		CHECK( spreadDragLifecycle.spreadDragMoved(),
		       "burst-spread gesture detects movement from its captured start grid" );

		SoldierAnimationActivityComponent traversalLifecycle;
		traversalLifecycle.forecastTraversalAt(1600);
		traversalLifecycle.setRenderZOverride(777);
		CHECK( traversalLifecycle.traversalForecastGrid() == 1600 &&
		       traversalLifecycle.hasRenderZOverride() &&
		       traversalLifecycle.renderZOverride() == 777,
		       "animation activity coordinates traversal forecast and render-depth override" );
		traversalLifecycle.clearRenderZOverride();
		CHECK( !traversalLifecycle.hasRenderZOverride(),
		       "animation activity clears traversal render-depth override explicitly" );
		traversalLifecycle.advanceRandomActionCheck();
		traversalLifecycle.lastRandomAnimation() = 17;
		CHECK( traversalLifecycle.randomActionCheckDue(0) &&
		       traversalLifecycle.randomActionCheckCounter() == 1 &&
		       traversalLifecycle.lastRandomAnimation() == 17,
		       "animation activity advances the random-animation cadence through a named transition" );
		traversalLifecycle.resetRandomActionCheck();
		CHECK( !traversalLifecycle.randomActionCheckDue(0) &&
		       traversalLifecycle.randomActionCheckCounter() == 0,
		       "animation activity resets only the random-animation cadence when a check is consumed" );

		SoldierInterruptSnapshotComponent interruptSnapshotLifecycle;
		interruptSnapshotLifecycle.captureMoved(1);
		CHECK( interruptSnapshotLifecycle.movedBeforeInterrupt() == 1,
		       "interrupt snapshot captures scheduler movement state" );
		interruptSnapshotLifecycle.reset();
		CHECK( interruptSnapshotLifecycle.movedBeforeInterrupt() == 0,
		       "interrupt snapshot reset restores the established default" );

		SoldierSuppressionComponent suppressionLifecycle;
		suppressionLifecycle.underFire() = 3;
		suppressionLifecycle.shock() = 7;
		suppressionLifecycle.recordBullet(SoldierID{ 7 });
		suppressionLifecycle.addPoints(4);
		suppressionLifecycle.addActionPointLoss(250);
		suppressionLifecycle.addActionPointLoss(10);
		suppressionLifecycle.markCloseCall();
		CHECK( suppressionLifecycle.points() == 5 &&
		       suppressionLifecycle.actionPointsLost() == 255 &&
		       suppressionLifecycle.suppressor() == SoldierID{ 7 } &&
		       suppressionLifecycle.closeCall(),
		       "suppression transitions coordinate bullet attribution and clamp accumulated AP loss" );
		suppressionLifecycle.clearCloseCall();
		CHECK( !suppressionLifecycle.closeCall(),
		       "suppression close-call feedback has an explicit clear transition" );
		suppressionLifecycle.markCloseCall();
		suppressionLifecycle.beginTurn();
		CHECK( suppressionLifecycle.underFire() == 3 &&
		       suppressionLifecycle.shock() == 7 &&
		       suppressionLifecycle.points() == 0 &&
		       suppressionLifecycle.actionPointsLost() == 0 &&
		       suppressionLifecycle.suppressor() == SoldierID{ 7 } &&
		       !suppressionLifecycle.closeCall(),
		       "beginning a turn clears per-attack suppression without discarding aged reaction state or attribution" );
		suppressionLifecycle.reset();
		CHECK( suppressionLifecycle.underFire() == 0 &&
		       suppressionLifecycle.shock() == 0 &&
		       suppressionLifecycle.points() == 0 &&
		       suppressionLifecycle.actionPointsLost() == 0 &&
		       suppressionLifecycle.suppressor() == NOBODY &&
		       !suppressionLifecycle.closeCall(),
		       "suppression reset clears the complete hostile-fire reaction domain" );

		SoldierActionPointComponent actionPointLifecycle;
		actionPointLifecycle.beginTurn(80);
		actionPointLifecycle.current() = 35;
		CHECK( actionPointLifecycle.current() == 35 &&
		       actionPointLifecycle.initial() == 80 &&
		       actionPointLifecycle.hasAny(),
		       "action-point turn setup records one current and initial budget" );
		actionPointLifecycle.snapshotTurnStart();
		CHECK( actionPointLifecycle.current() == 35 &&
		       actionPointLifecycle.initial() == 35,
		       "action-point turn snapshot advances both consumers to the current budget" );
		actionPointLifecycle.clear();
		CHECK( actionPointLifecycle.current() == 0 &&
		       actionPointLifecycle.initial() == 0 &&
		       !actionPointLifecycle.hasAny(),
		       "action-point clear transition removes both current and turn-start budgets" );

		SoldierCollapseComponent collapseLifecycle;
		collapseLifecycle.collapse();
		collapseLifecycle.markBreathCollapse();
		collapseLifecycle.turns() = 3;
		collapseLifecycle.sleepDrugCounter() = 4;
		collapseLifecycle.markFatigueCollapse();
		CHECK( collapseLifecycle.collapsed() &&
		       collapseLifecycle.breathCollapsed() &&
		       collapseLifecycle.turns() == 3 &&
		       collapseLifecycle.sleepDrugCounter() == 4 &&
		       collapseLifecycle.fatigueCollapsed(),
		       "collapse lifecycle distinguishes tactical, breath, drug, and strategic fatigue state" );
		collapseLifecycle.clearBreathCollapse();
		collapseLifecycle.clearFatigueCollapse();
		collapseLifecycle.recover();
		CHECK( !collapseLifecycle.collapsed() &&
		       !collapseLifecycle.breathCollapsed() &&
		       collapseLifecycle.turns() == 0 &&
		       collapseLifecycle.sleepDrugCounter() == 4 &&
		       !collapseLifecycle.fatigueCollapsed(),
		       "collapse recovery clears tactical duration without discarding the independent sleep-drug timer" );
		collapseLifecycle.reset();
		CHECK( !collapseLifecycle.collapsed() &&
		       !collapseLifecycle.breathCollapsed() &&
		       collapseLifecycle.turns() == 0 &&
		       collapseLifecycle.sleepDrugCounter() == 0 &&
		       !collapseLifecycle.fatigueCollapsed(),
		       "collapse reset clears the complete incapacitation domain" );

		SoldierPerceptionComponent perceptionLifecycle;
		perceptionLifecycle.rememberMovementFrom(1);
		perceptionLifecycle.rememberMovementFrom(5);
		perceptionLifecycle.rememberMovementFrom(8);
		perceptionLifecycle.viewRange() = 16;
		perceptionLifecycle.setBlindness(2);
		perceptionLifecycle.addBlindness(2);
		const bool ignoredShorterBlindness =
			perceptionLifecycle.extendBlindnessToAtLeast(3);
		const bool acceptedLongerBlindness =
			perceptionLifecycle.extendBlindnessToAtLeast(6);
		perceptionLifecycle.setDeafness(4);
		perceptionLifecycle.halveDeafness();
		perceptionLifecycle.heardNoiseLevel() = 1;
		perceptionLifecycle.activateXrayAt(9876);
		CHECK( perceptionLifecycle.hasHeardMovementFrom(1) &&
		       perceptionLifecycle.hasHeardMovementFrom(5) &&
		       !perceptionLifecycle.hasHeardMovementFrom(8) &&
		       perceptionLifecycle.viewRange() == 16 &&
		       perceptionLifecycle.blindnessTurns() == 6 &&
		       !ignoredShorterBlindness &&
		       acceptedLongerBlindness &&
		       perceptionLifecycle.deafnessTurns() == 2 &&
		       perceptionLifecycle.isDeafened() &&
		       perceptionLifecycle.heardNoiseLevel() == 1 &&
		       perceptionLifecycle.xrayActive(),
		       "perception lifecycle coordinates directional memory and bounded sensory effects" );
		CHECK( !perceptionLifecycle.ageBlindness() &&
		       perceptionLifecycle.blindnessTurns() == 5,
		       "perception blindness aging reports only the recovery transition" );
		perceptionLifecycle.setBlindness(1);
		CHECK( perceptionLifecycle.ageBlindness() &&
		       !perceptionLifecycle.isBlinded(),
		       "perception blindness aging reports the exact sight-recovery edge" );
		perceptionLifecycle.ageDeafness();
		perceptionLifecycle.clearMovementDirections();
		perceptionLifecycle.deactivateXray();
		CHECK( perceptionLifecycle.deafnessTurns() == 1 &&
		       perceptionLifecycle.movementNoiseDirections() == 0 &&
		       !perceptionLifecycle.xrayActive(),
		       "perception turn operations age and clear independent sensory state" );
		perceptionLifecycle.reset();
		CHECK( perceptionLifecycle.movementNoiseDirections() == 0 &&
		       perceptionLifecycle.viewRange() == 0 &&
		       !perceptionLifecycle.isBlinded() &&
		       perceptionLifecycle.heardNoiseLevel() == 0 &&
		       !perceptionLifecycle.xrayActive() &&
		       !perceptionLifecycle.isDeafened(),
		       "perception reset clears the complete sensory domain" );

		SoldierAwarenessComponent awarenessLifecycle;
		CHECK( awarenessLifecycle.locationKnown() &&
		       !awarenessLifecycle.visibleNow() &&
		       !awarenessLifecycle.fullyHidden() &&
		       !awarenessLifecycle.fadingOut() &&
		       !awarenessLifecycle.renderVisibilityChanged() &&
		       !awarenessLifecycle.hasNewOpponents() &&
		       awarenessLifecycle.tilesSinceForget() == 0,
		       "awareness defaults to synchronized indeterminate visibility with no discoveries or stale movement" );
		awarenessLifecycle.markHidden();
		CHECK( awarenessLifecycle.fullyHidden() &&
		       !awarenessLifecycle.locationKnown() &&
		       awarenessLifecycle.renderVisibilityChanged(),
		       "awareness distinguishes fully hidden soldiers from the synchronized render state" );
		awarenessLifecycle.syncRenderedVisibility();
		CHECK( !awarenessLifecycle.renderVisibilityChanged(),
		       "awareness explicitly acknowledges visibility consumed by rendering" );
		awarenessLifecycle.markVisible();
		awarenessLifecycle.recordNewOpponent();
		awarenessLifecycle.recordNewOpponent();
		awarenessLifecycle.tilesSinceForget() = 254;
		awarenessLifecycle.recordTileForMemory();
		awarenessLifecycle.recordTileForMemory();
		CHECK( awarenessLifecycle.visibleNow() &&
		       awarenessLifecycle.newOpponentCount() == 2 &&
		       awarenessLifecycle.tilesSinceForget() == 255,
		       "awareness coordinates visibility and saturates stale-memory movement distance" );
		awarenessLifecycle.newOpponentCount() = 127;
		awarenessLifecycle.recordNewOpponent();
		CHECK( awarenessLifecycle.newOpponentCount() == 127,
		       "awareness discovery count saturates instead of overflowing signed storage" );
		awarenessLifecycle.clearNewOpponents();
		awarenessLifecycle.resetForgetDistance();
		awarenessLifecycle.markHidden();
		awarenessLifecycle.beginFadeOut();
		CHECK( awarenessLifecycle.fadingOut() &&
		       !awarenessLifecycle.locationKnown() &&
		       !awarenessLifecycle.hasNewOpponents() &&
		       awarenessLifecycle.tilesSinceForget() == 0,
		       "awareness exposes the render fade-out sentinel as a named transition" );
		awarenessLifecycle.setVisibilityAndRendered(0);
		CHECK( awarenessLifecycle.locationKnown() &&
		       !awarenessLifecycle.renderVisibilityChanged(),
		       "awareness can synchronize editor visibility without an artificial render transition" );
		awarenessLifecycle.reset();
		CHECK( awarenessLifecycle.visibility() == 0 &&
		       awarenessLifecycle.lastRenderedVisibility() == 0 &&
		       awarenessLifecycle.newOpponentCount() == 0 &&
		       awarenessLifecycle.tilesSinceForget() == 0,
		       "awareness reset clears the complete player-knowledge domain" );

		SoldierCamouflageComponent camouflageLifecycle;
		camouflageLifecycle.jungleApplied() = 90;
		camouflageLifecycle.jungleWorn() = 40;
		camouflageLifecycle.urbanApplied() = 45;
		camouflageLifecycle.urbanWorn() = 35;
		camouflageLifecycle.desertApplied() = -80;
		camouflageLifecycle.desertWorn() = -50;
		CHECK( camouflageLifecycle.total(SoldierCamouflageComponent::Terrain::Jungle) == 100 &&
		       camouflageLifecycle.total(SoldierCamouflageComponent::Terrain::Urban) == 80 &&
		       camouflageLifecycle.total(SoldierCamouflageComponent::Terrain::Desert) == -100 &&
		       camouflageLifecycle.strongestTotal() == 100 &&
		       camouflageLifecycle.appliedTotal() == 55,
		       "camouflage totals preserve signed effects while bounding terrain and display values" );
		camouflageLifecycle.reset();
		CHECK( camouflageLifecycle.jungleApplied() == 0 &&
		       camouflageLifecycle.jungleWorn() == 0 &&
		       camouflageLifecycle.urbanApplied() == 0 &&
		       camouflageLifecycle.urbanWorn() == 0 &&
		       camouflageLifecycle.desertApplied() == 0 &&
		       camouflageLifecycle.desertWorn() == 0 &&
		       camouflageLifecycle.snowApplied() == 0 &&
		       camouflageLifecycle.snowWorn() == 0,
		       "camouflage reset clears applied and equipment-derived state for every terrain family" );

		SoldierEmploymentComponent employmentLifecycle;
		employmentLifecycle.mercenaryType() = MERC_TYPE__MERC;
		employmentLifecycle.medicalDeposit() = 1;
		employmentLifecycle.lifeInsurance() = 1;
		employmentLifecycle.justFired() = 1;
		CHECK( employmentLifecycle.isMercenaryType(MERC_TYPE__MERC) &&
		       employmentLifecycle.hasMedicalDeposit() &&
		       employmentLifecycle.hasLifeInsurance() &&
		       employmentLifecycle.wasJustFired(),
		       "employment exposes named classification, deposit, insurance, and dismissal queries" );
		employmentLifecycle.reset();
		CHECK( employmentLifecycle.endTime() == 0 &&
		       employmentLifecycle.startTime() == 0 &&
		       employmentLifecycle.totalLength() == 0 &&
		       employmentLifecycle.mercenaryType() == 0 &&
		       employmentLifecycle.medicalDeposit() == 0 &&
		       employmentLifecycle.lifeInsurance() == 0 &&
		       employmentLifecycle.insuranceStartDay() == 0 &&
		       employmentLifecycle.insuranceLengthDays() == 0 &&
		       employmentLifecycle.lastContractUpdateTime() == 0 &&
		       employmentLifecycle.lastContractType() == 0 &&
		       employmentLifecycle.justFired() == 0 &&
		       employmentLifecycle.renewalQuoteCode() == 0 &&
		       employmentLifecycle.timeCanSignElsewhere() == 0 &&
		       employmentLifecycle.hospitalPriceModifier() == 0 &&
		       employmentLifecycle.insuranceStartTime() == 0,
		       "employment reset clears the complete strategic contract lifecycle" );

		SoldierAssignmentComponent assignmentLifecycle;
		assignmentLifecycle.current() = TRAIN_BY_OTHER;
		assignmentLifecycle.hours() = 2;
		assignmentLifecycle.miniEventHoursRemaining() = 3;
		assignmentLifecycle.repairVehicleId() = 4;
		assignmentLifecycle.facilityType() = 5;
		CHECK( assignmentLifecycle.isAssignedTo(TRAIN_BY_OTHER) &&
		       assignmentLifecycle.hasAssignmentHours() &&
		       assignmentLifecycle.hasMiniEventTime(),
		       "assignment exposes named duty and elapsed-time queries" );
		assignmentLifecycle.clearRepairVehicle();
		assignmentLifecycle.clearFacility();
		CHECK( assignmentLifecycle.repairVehicleId() == -1 &&
		       assignmentLifecycle.facilityType() == -1,
		       "assignment clears subsidiary repair and facility context through named transitions" );
		assignmentLifecycle.reset();
		CHECK( assignmentLifecycle.current() == 0 &&
		       assignmentLifecycle.previous() == 0 &&
		       assignmentLifecycle.trainingStat() == 0 &&
		       assignmentLifecycle.lastChangeMinute() == 0 &&
		       assignmentLifecycle.desiredSquad() == 0 &&
		       assignmentLifecycle.mergeTraversalAllowance() == 0 &&
		       assignmentLifecycle.hours() == 0 &&
		       assignmentLifecycle.repairVehicleId() == 0 &&
		       assignmentLifecycle.facilityType() == 0 &&
		       assignmentLifecycle.itemMoveSectorId() == 0 &&
		       assignmentLifecycle.miniEventHoursRemaining() == 0,
		       "assignment reset clears the complete strategic duty lifecycle" );

		SoldierDeploymentComponent deploymentLifecycle;
		deploymentLifecycle.insertionDirection() = 5;
		deploymentLifecycle.groupId() = 9;
		deploymentLifecycle.insertionGrid() = 3300;
		deploymentLifecycle.setStrategicInsertion(INSERTION_CODE_GRIDNO, 3301);
		deploymentLifecycle.setSector(11, 5, 2);
		deploymentLifecycle.vehicleId() = 8;
		deploymentLifecycle.offWorldGrid() = 3302;
		deploymentLifecycle.setTraversalOrigin(55, 3303);
		deploymentLifecycle.useExitGridForReentryDirection() = 1;
		deploymentLifecycle.scheduleArrival(4400, 7);
		deploymentLifecycle.beginArrivalGetup();
		deploymentLifecycle.arrivalGetupCounter() = 4500;
		CHECK( deploymentLifecycle.isInSector(11, 5, 2) &&
		       deploymentLifecycle.hasVehicle() &&
		       deploymentLifecycle.strategicInsertionData() == 3301 &&
		       deploymentLifecycle.previousSectorId() == 55 &&
		       deploymentLifecycle.preTraversalGrid() == 3303 &&
		       deploymentLifecycle.arrivalTime() == 4400 &&
		       deploymentLifecycle.leaveHistoryCode() == 7 &&
		       deploymentLifecycle.arrivalGetupPending() &&
		       deploymentLifecycle.ignoreCollapseGetupCheck() &&
		       deploymentLifecycle.arrivalGetupCounter() == 4500,
		       "deployment exposes named sector, insertion, traversal, vehicle, and arrival transitions" );
		deploymentLifecycle.completeArrivalGetup();
		deploymentLifecycle.clearCollapseGetupOverride();
		CHECK( !deploymentLifecycle.arrivalGetupPending() &&
		       !deploymentLifecycle.ignoreCollapseGetupCheck() &&
		       deploymentLifecycle.arrivalGetupCounter() == 4500,
		       "deployment completes arrival get-up while retaining the historical timer value" );
		deploymentLifecycle.clearVehicle();
		CHECK( !deploymentLifecycle.hasVehicle() && deploymentLifecycle.vehicleId() == -1,
		       "deployment clears vehicle membership through a named transition" );
		deploymentLifecycle.reset();
		CHECK( deploymentLifecycle.insertionDirection() == 0 &&
		       deploymentLifecycle.groupId() == 0 &&
		       deploymentLifecycle.insertionGrid() == 0 &&
		       deploymentLifecycle.strategicInsertionCode() == 0 &&
		       deploymentLifecycle.strategicInsertionData() == 0 &&
		       deploymentLifecycle.isInSector(0, 0, 0) &&
		       deploymentLifecycle.vehicleId() == -1 &&
		       deploymentLifecycle.offWorldGrid() == 0 &&
		       deploymentLifecycle.previousSectorId() == 0 &&
		       deploymentLifecycle.useExitGridForReentryDirection() == 0 &&
		       deploymentLifecycle.preTraversalGrid() == 0 &&
		       deploymentLifecycle.leaveHistoryCode() == 0 &&
		       deploymentLifecycle.arrivalTime() == 0 &&
		       !deploymentLifecycle.arrivalGetupPending() &&
		       !deploymentLifecycle.ignoreCollapseGetupCheck() &&
		       deploymentLifecycle.arrivalGetupCounter() == 0,
		       "deployment reset clears the complete strategic placement lifecycle" );

		SoldierScheduleComponent scheduleLifecycle;
		scheduleLifecycle.id() = 41;
		scheduleLifecycle.progress() = 1;
		scheduleLifecycle.advanceProgress();
		scheduleLifecycle.beginDoorContinuation(3400);
		CHECK( scheduleLifecycle.assigned() &&
		       scheduleLifecycle.progress() == 2 &&
		       scheduleLifecycle.doorAnimationStarted() &&
		       scheduleLifecycle.doorGrid() == 3400,
		       "schedule begins door continuation with one atomic phase-and-grid transition" );
		scheduleLifecycle.completeDoorAnimation();
		CHECK( scheduleLifecycle.doorAnimationComplete() &&
		       scheduleLifecycle.consumeDoorGrid() == 3400 &&
		       !scheduleLifecycle.doorContinuationPending(),
		       "schedule completes and consumes door continuation through named transitions" );
		scheduleLifecycle.progress() = std::numeric_limits<INT8>::max();
		scheduleLifecycle.advanceProgress();
		CHECK( scheduleLifecycle.progress() == std::numeric_limits<INT8>::max(),
		       "schedule progress saturates instead of overflowing a corrupted or extended schedule" );
		scheduleLifecycle.beginDoorContinuation(3401);
		scheduleLifecycle.cancelDoorContinuation();
		CHECK( !scheduleLifecycle.doorContinuationPending() &&
		       scheduleLifecycle.doorGrid() == 3401,
		       "cancelling a door continuation preserves the historical retained-grid behavior" );
		scheduleLifecycle.reset();
		CHECK( !scheduleLifecycle.assigned() &&
		       scheduleLifecycle.progress() == 0 &&
		       !scheduleLifecycle.doorContinuationPending() &&
		       scheduleLifecycle.doorGrid() == 0,
		       "schedule reset clears identity, progress, and door continuation state" );
		copiedSoldier.initialize();
		CHECK( copiedSoldier.vitals().health() == 0 &&
		       copiedSoldier.vitals().maximumHealth() == 0 &&
		       copiedSoldier.vitals().breath() == 0 &&
		       copiedSoldier.vitals().maximumBreath() == 0 &&
		       copiedSoldier.vitals().bleeding() == 0 &&
		       copiedSoldier.vitals().previousHealth() == 0 &&
		       copiedSoldier.vitals().fractionalHealth() == 0 &&
		       copiedSoldier.vitals().breathReduction() == 0 &&
		       copiedSoldier.vitals().healableInjury() == 0 &&
		       !copiedSoldier.vitals().isUndergoingSurgery() &&
		       copiedSoldier.vitals().unregainableBreath() == 0 &&
		       copiedSoldier.vitals().criticalStatDamage()[DAMAGED_STAT_DEXTERITY] == 0 &&
		       copiedSoldier.vitals().criticalStatDamage()[DAMAGED_STAT_STRENGTH] == 0 &&
		       copiedSoldier.vitals().nextBleedAt() == 0.0f &&
		       copiedSoldier.vitals().regenerationCounter() == 0 &&
		       copiedSoldier.vitals().regenerationBoostersUsedToday() == 0 &&
		       copiedSoldier.vitals().lastBleedGruntAt() == 0,
		       "soldier initialization resets the complete vitals domain" );
		CHECK( copiedSoldier.service().activity() == 0 &&
		       !copiedSoldier.service().hasProviders() &&
		       !copiedSoldier.service().hasPartner() &&
		       !copiedSoldier.service().hasAutoBandagingMedic(),
		       "soldier initialization resets the complete tactical service domain" );
		CHECK( copiedSoldier.dialogue().quoteRecord() == 0 &&
		       copiedSoldier.dialogue().quoteActionId() == 0 &&
		       copiedSoldier.dialogue().battleSoundSet() == 0 &&
		       copiedSoldier.dialogue().saidFlags() == 0 &&
		       copiedSoldier.dialogue().vocalVolume() == 0 &&
		       copiedSoldier.dialogue().repeatedBattleSoundAt() == 0 &&
		       copiedSoldier.dialogue().previousBattleSound() == 0 &&
		       copiedSoldier.dialogue().heardNoiseCooldownTurns() == 0 &&
		       copiedSoldier.dialogue().saidExtendedFlags() == 0 &&
		       copiedSoldier.dialogue().activeBattleSound() == 0 &&
		       copiedSoldier.dialogue().currentCivilianQuote() == 0 &&
		       copiedSoldier.dialogue().civilianQuoteDelta() == 0 &&
		       copiedSoldier.dialogue().lastSpokeAt() == 0 &&
		       copiedSoldier.dialogue().corpseQuoteTolerance() == 0,
		       "soldier initialization resets the complete dialogue domain" );
		CHECK( copiedSoldier.audio().lastFootstepVariant() == 0 &&
		       copiedSoldier.audio().doorOpeningNoise() == 0 &&
		       !copiedSoldier.audio().hasBurstSound() &&
		       !copiedSoldier.audio().hasPositionSound() &&
		       !copiedSoldier.audio().hasTurningSound(),
		       "soldier initialization resets non-dialogue audio with valid inactive sentinels" );
		CHECK( copiedSoldier.replication().movementStartedAt() == 0 &&
		       copiedSoldier.replication().optimumMovementTime() == 0 &&
		       !copiedSoldier.replication().hasLastUpdate() &&
		       copiedSoldier.replication().updateSequence() == 0 &&
		       copiedSoldier.replication().updateType() == 0 &&
		       copiedSoldier.replication().scheduledStopGrid() == 0 &&
		       copiedSoldier.replication().checksum() == 0,
		       "soldier initialization resets the complete replication domain" );
		CHECK( copiedSoldier.movementMetrics().carriedWeightAtTurnStart() == 0 &&
		       !copiedSoldier.movementMetrics().movedThisTurn() &&
		       !copiedSoldier.movementMetrics().hasRealtimeBreathMovement() &&
		       copiedSoldier.movementMetrics().lastRealtimeMovementAnimation() == 0,
		       "soldier initialization resets the complete movement-metrics domain" );
		CHECK( copiedSoldier.aiPlanning().flankCount() == 0 &&
		       copiedSoldier.aiPlanning().flankAnchorGrid() == 0 &&
		       !copiedSoldier.aiPlanning().sniperPostureActive() &&
		       copiedSoldier.aiPlanning().flankOriginDirection() == 0 &&
		       !copiedSoldier.aiPlanning().hasPlanIndex(),
		       "soldier initialization resets the complete AI-planning domain" );
		CHECK( copiedSoldier.skillState().lastCheckReason() == 0 &&
		       copiedSoldier.skillState().checkAttempts() == 0 &&
		       copiedSoldier.skillState().checkGrid() == 0 &&
		       copiedSoldier.skillState().selectedAiSkill() == 0 &&
		       copiedSoldier.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY) == 0 &&
		       copiedSoldier.skillState().counter(SOLDIER_COUNTER_MAX - 1) == 0 &&
		       copiedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_CRYO) == 0 &&
		       copiedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_MAX - 1) == 0 &&
		       copiedSoldier.skillState().focusGrid() == 0,
		       "soldier initialization resets the complete skill-state domain" );
		CHECK( copiedSoldier.condition().extraStrength() == 0 &&
		       copiedSoldier.condition().extraDexterity() == 0 &&
		       copiedSoldier.condition().extraAgility() == 0 &&
		       copiedSoldier.condition().extraWisdom() == 0 &&
		       copiedSoldier.condition().extraExperienceLevel() == 0 &&
		       copiedSoldier.condition().foodLevel() == 0 &&
		       copiedSoldier.condition().drinkLevel() == 0 &&
		       copiedSoldier.condition().starvationHealthDamage() == 0 &&
		       copiedSoldier.condition().starvationStrengthDamage() == 0 &&
		       copiedSoldier.condition().diseasePoints(0) == 0 &&
		       copiedSoldier.condition().diseaseFlags(0) == 0 &&
		       copiedSoldier.condition().diseasePoints(NUM_DISEASES - 1) == 0 &&
		       copiedSoldier.condition().diseaseFlags(NUM_DISEASES - 1) == 0 &&
		       copiedSoldier.condition().disabilityFlags() == 0,
		       "soldier initialization resets the complete condition domain" );
		CHECK( !copiedSoldier.longAction().active() &&
		       copiedSoldier.longAction().action() == MTA_NONE &&
		       copiedSoldier.longAction().contextGrid() == -1 &&
		       copiedSoldier.longAction().remainingActionPoints() == 0,
		       "soldier initialization resets the complete long-action domain" );
		CHECK( copiedSoldier.interaction().nonNpcTraderId() == 0 &&
		       !copiedSoldier.interaction().dragging() &&
		       copiedSoldier.interaction().draggedPerson() == NOBODY &&
		       copiedSoldier.interaction().draggedCorpse() == -1 &&
		       copiedSoldier.interaction().draggedStructureGrid() == -1 &&
		       copiedSoldier.interaction().chatPartner() == NOBODY,
		       "soldier initialization resets the complete interaction domain" );
		CHECK( !copiedSoldier.pendingAction().active() &&
		       copiedSoldier.pendingAction().action() == SoldierPendingActionComponent::NoAction &&
		       copiedSoldier.pendingAction().animationCount() == 0 &&
		       copiedSoldier.pendingAction().primaryData() == 0 &&
		       copiedSoldier.pendingAction().secondaryData() == 0 &&
		       copiedSoldier.pendingAction().tertiaryData() == 0 &&
		       copiedSoldier.pendingAction().doorHandleCode() == 0 &&
		       copiedSoldier.pendingAction().quaternaryData() == 0 &&
		       copiedSoldier.pendingAction().nextSpecialData() == 0 &&
		       copiedSoldier.pendingAction().interruptionMarker() == 0 &&
		       copiedSoldier.pendingAction().inventorySlot() == 0,
		       "soldier initialization resets the complete pending-action domain" );
		CHECK( copiedSoldier.actionPoints().current() == 0 &&
		       copiedSoldier.actionPoints().initial() == 0 &&
		       !copiedSoldier.actionPoints().hasAny(),
		       "soldier initialization resets the complete action-point domain" );
		CHECK( !copiedSoldier.collapseState().collapsed() &&
		       !copiedSoldier.collapseState().breathCollapsed() &&
		       copiedSoldier.collapseState().turns() == 0 &&
		       copiedSoldier.collapseState().sleepDrugCounter() == 0 &&
		       !copiedSoldier.collapseState().fatigueCollapsed(),
		       "soldier initialization resets the complete collapse domain" );
		CHECK( copiedSoldier.perception().movementNoiseDirections() == 0 &&
		       copiedSoldier.perception().viewRange() == 0 &&
		       !copiedSoldier.perception().isBlinded() &&
		       copiedSoldier.perception().heardNoiseLevel() == 0 &&
		       !copiedSoldier.perception().xrayActive() &&
		       !copiedSoldier.perception().isDeafened(),
		       "soldier initialization resets the complete perception domain" );
		CHECK( copiedSoldier.awareness().visibility() == 0 &&
		       copiedSoldier.awareness().lastRenderedVisibility() == 0 &&
		       copiedSoldier.awareness().newOpponentCount() == 0 &&
		       copiedSoldier.awareness().tilesSinceForget() == 0 &&
		       !copiedSoldier.awareness().hasNewOpponents() &&
		       !copiedSoldier.awareness().renderVisibilityChanged(),
		       "soldier initialization resets the complete awareness domain" );
		CHECK( copiedSoldier.camouflage().jungleApplied() == 0 &&
		       copiedSoldier.camouflage().jungleWorn() == 0 &&
		       copiedSoldier.camouflage().urbanApplied() == 0 &&
		       copiedSoldier.camouflage().urbanWorn() == 0 &&
		       copiedSoldier.camouflage().desertApplied() == 0 &&
		       copiedSoldier.camouflage().desertWorn() == 0 &&
		       copiedSoldier.camouflage().snowApplied() == 0 &&
		       copiedSoldier.camouflage().snowWorn() == 0 &&
		       copiedSoldier.camouflage().strongestTotal() == 0 &&
		       copiedSoldier.camouflage().appliedTotal() == 0,
		       "soldier initialization resets the complete camouflage domain" );
		CHECK( copiedSoldier.employment().endTime() == 0 &&
		       copiedSoldier.employment().startTime() == 0 &&
		       copiedSoldier.employment().totalLength() == 0 &&
		       copiedSoldier.employment().mercenaryType() == 0 &&
		       copiedSoldier.employment().medicalDeposit() == 0 &&
		       copiedSoldier.employment().lifeInsurance() == 0 &&
		       copiedSoldier.employment().insuranceStartDay() == 0 &&
		       copiedSoldier.employment().insuranceLengthDays() == 0 &&
		       copiedSoldier.employment().lastContractUpdateTime() == 0 &&
		       copiedSoldier.employment().lastContractType() == 0 &&
		       copiedSoldier.employment().justFired() == 0 &&
		       copiedSoldier.employment().renewalQuoteCode() == 0 &&
		       copiedSoldier.employment().timeCanSignElsewhere() == 0 &&
		       copiedSoldier.employment().hospitalPriceModifier() == 0 &&
		       copiedSoldier.employment().insuranceStartTime() == 0,
		       "soldier initialization resets the complete employment domain" );
		CHECK( copiedSoldier.assignment().current() == 0 &&
		       copiedSoldier.assignment().previous() == 0 &&
		       copiedSoldier.assignment().trainingStat() == 0 &&
		       copiedSoldier.assignment().lastChangeMinute() == 0 &&
		       copiedSoldier.assignment().desiredSquad() == 0 &&
		       copiedSoldier.assignment().mergeTraversalAllowance() == 0 &&
		       copiedSoldier.assignment().hours() == 0 &&
		       copiedSoldier.assignment().repairVehicleId() == 0 &&
		       copiedSoldier.assignment().facilityType() == 0 &&
		       copiedSoldier.assignment().itemMoveSectorId() == 0 &&
		       copiedSoldier.assignment().miniEventHoursRemaining() == 0,
		       "soldier initialization resets the complete assignment domain" );
		CHECK( copiedSoldier.deployment().insertionDirection() == 0 &&
		       copiedSoldier.deployment().groupId() == 0 &&
		       copiedSoldier.deployment().insertionGrid() == 0 &&
		       copiedSoldier.deployment().strategicInsertionCode() == 0 &&
		       copiedSoldier.deployment().strategicInsertionData() == 0 &&
		       copiedSoldier.deployment().isInSector(0, 0, 0) &&
		       copiedSoldier.deployment().vehicleId() == -1 &&
		       copiedSoldier.deployment().offWorldGrid() == 0 &&
		       copiedSoldier.deployment().previousSectorId() == 0 &&
		       copiedSoldier.deployment().useExitGridForReentryDirection() == 0 &&
		       copiedSoldier.deployment().preTraversalGrid() == 0 &&
		       copiedSoldier.deployment().leaveHistoryCode() == 0 &&
		       copiedSoldier.deployment().arrivalTime() == 0 &&
		       !copiedSoldier.deployment().arrivalGetupPending() &&
		       !copiedSoldier.deployment().ignoreCollapseGetupCheck() &&
		       copiedSoldier.deployment().arrivalGetupCounter() == 0,
		       "soldier initialization resets the complete deployment domain" );
		CHECK( !copiedSoldier.schedule().assigned() &&
		       copiedSoldier.schedule().progress() == 0 &&
		       !copiedSoldier.schedule().doorContinuationPending() &&
		       copiedSoldier.schedule().doorGrid() == 0,
		       "soldier initialization resets the complete NPC schedule domain" );
		CHECK( copiedSoldier.position().worldX() == 0 &&
		       copiedSoldier.position().worldY() == 0 &&
		       copiedSoldier.position().worldXInt() == 0 &&
		       copiedSoldier.position().worldYInt() == 0 &&
		       !copiedSoldier.position().hasTurnStart() &&
		       copiedSoldier.position().turnStartX() == 0 &&
		       copiedSoldier.position().turnStartY() == 0 &&
		       copiedSoldier.position().initialGrid() == 0 &&
		       copiedSoldier.position().gridNo() == 0 &&
		       copiedSoldier.position().level() == 0 &&
		       copiedSoldier.position().direction() == 0 &&
		       copiedSoldier.position().heightAdjustment() == 0 &&
		       copiedSoldier.position().desiredHeight() == 0 &&
		       copiedSoldier.position().temporaryGrid() == 0 &&
		       copiedSoldier.position().roomNo() == 0 &&
		       copiedSoldier.position().terrainType() == 0 &&
		       copiedSoldier.position().previousTerrainType() == 0,
		       "soldier initialization resets the complete position domain" );
		CHECK( copiedSoldier.movementHistory().previousGrid() == 0 &&
		       copiedSoldier.movementHistory().recentLocations()[0] == 0 &&
		       copiedSoldier.movementHistory().recentLocations()[1] == 0,
		       "soldier initialization resets the complete movement-history domain" );
		CHECK( copiedSoldier.pathing().desiredDirection() == 0 &&
		       copiedSoldier.pathing().destinationX() == 0 &&
		       copiedSoldier.pathing().destinationY() == 0 &&
		       copiedSoldier.pathing().destinationGrid() == 0 &&
		       copiedSoldier.pathing().finalDestinationGrid() == 0 &&
		       copiedSoldier.pathing().stopped() == 0 &&
		       copiedSoldier.pathing().needsLook() == 0 &&
		       copiedSoldier.pathing().path()[0] == 0 &&
		       copiedSoldier.pathing().pathSize() == 0 &&
		       copiedSoldier.pathing().pathIndex() == 0 &&
		       copiedSoldier.pathing().blackListGrid() == 0 &&
		       copiedSoldier.pathing().stored() == 0,
		       "soldier initialization resets the complete pathing domain" );
		CHECK( copiedSoldier.movement().mode() == 0 &&
		       !copiedSoldier.movement().stealthy() &&
		       !copiedSoldier.movement().reversing() &&
		       copiedSoldier.movement().highResolutionDirection() == 0 &&
		       copiedSoldier.movement().highResolutionDesiredDirection() == 0 &&
		       copiedSoldier.movement().animationDirection() == 0 &&
		       copiedSoldier.movement().gridUpdatePolicy() == 0 &&
		       copiedSoldier.movement().delayCounter() == 0 &&
		       copiedSoldier.movement().delayedCauseGrid() == 0 &&
		       copiedSoldier.movement().reservedGrid() == 0 &&
		       !copiedSoldier.movement().blockedByAnotherMerc() &&
		       copiedSoldier.movement().blockedDirection() == 0 &&
		       copiedSoldier.movement().absoluteDestination() == 0 &&
		       copiedSoldier.movement().continuedPathGrid() == 0 &&
		       !copiedSoldier.movement().continuedPathValid() &&
		       copiedSoldier.movement().delayedFlags() == 0 &&
		       copiedSoldier.movement().stopReason() == 0 &&
		       copiedSoldier.movement().moveSpeedOverride() == NOBODY &&
		       !copiedSoldier.movement().usesMoveSpeedOverride() &&
		       !copiedSoldier.movement().turnActive() &&
		       !copiedSoldier.movement().wasInWater() &&
		       !copiedSoldier.movement().fastUiMovement() &&
		       !copiedSoldier.movement().outOfActionPoints() &&
		       !copiedSoldier.movement().movementPaused() &&
		       !copiedSoldier.movement().recordingMovement() &&
		       !copiedSoldier.movement().delayedByNetwork() &&
		       !copiedSoldier.movement().wasMoving() &&
		       !copiedSoldier.movement().crossedDestinationCenter(),
		       "soldier initialization resets the complete movement domain" );
		CHECK( copiedSoldier.interruptSnapshot().movedBeforeInterrupt() == 0,
		       "soldier initialization resets the interrupt snapshot domain" );
		CHECK( copiedSoldier.targeting().gridNo() == 0 &&
		       copiedSoldier.targeting().level() == 0 &&
		       copiedSoldier.targeting().cubeLevel() == 0 &&
		       copiedSoldier.targeting().lastGridNo() == 0 &&
		       !copiedSoldier.targeting().hasTargetSoldier() &&
		       copiedSoldier.targeting().targetId() == NOBODY,
		       "soldier initialization resets the complete targeting domain" );
		CHECK( copiedSoldier.attackSelection().hand() == 0 &&
		       copiedSoldier.attackSelection().weapon() == 0 &&
		       copiedSoldier.attackSelection().weaponMode() == 0 &&
		       copiedSoldier.attackSelection().scopeMode() == 0 &&
		       copiedSoldier.attackSelection().shotLocation() == 0 &&
		       copiedSoldier.attackSelection().meleeLocation() == 0,
		       "soldier initialization resets the complete attack-selection domain" );
		CHECK( copiedSoldier.meleeApproach().movementMode() == 0 &&
		       copiedSoldier.meleeApproach().grid() == 0 &&
		       copiedSoldier.meleeApproach().cost() == 0 &&
		       copiedSoldier.meleeApproach().endDirection() == 0,
		       "soldier initialization resets the complete melee-approach cache" );
		CHECK( copiedSoldier.fireControl().burstCounter() == 0 &&
		       copiedSoldier.fireControl().autofireShots() == 0 &&
		       copiedSoldier.fireControl().bulletsLeft() == 0 &&
		       copiedSoldier.fireControl().spreadIndex() == 0 &&
		       !copiedSoldier.fireControl().autofireLastStep() &&
		       copiedSoldier.fireControl().spreadLocations()[0] == 0 &&
		       copiedSoldier.fireControl().spreadLocations()[5] == 0 &&
		       copiedSoldier.fireControl().previousMuzzleOffsetX()[1] == 0.0f &&
		       copiedSoldier.fireControl().previousCounterForceY()[1] == 0.0f &&
		       copiedSoldier.fireControl().initialMuzzleOffsetX() == 0.0f &&
		       copiedSoldier.fireControl().initialMuzzleOffsetY() == 0.0f &&
		       copiedSoldier.fireControl().barrelCounter() == 0 &&
		       copiedSoldier.fireControl().spreadDragStartGrid() == 0 &&
		       copiedSoldier.fireControl().spreadDragEndGrid() == 0,
		       "soldier initialization resets the complete fire-control domain" );
		CHECK( copiedSoldier.combatResult().currentAttacker() == NOBODY &&
		       copiedSoldier.combatResult().previousAttacker() == NOBODY &&
		       copiedSoldier.combatResult().earlierAttacker() == NOBODY &&
		       copiedSoldier.combatResult().hitLocation() == 0 &&
		       copiedSoldier.combatResult().lastDamageReason() == 0 &&
		       copiedSoldier.combatResult().hitsThisTurn() == 0 &&
		       copiedSoldier.combatResult().pelletsHitBy() == 0 &&
		       copiedSoldier.combatResult().accumulatedDamage() == 0,
		       "soldier initialization resets the complete combat-result domain" );
		CHECK( !copiedSoldier.combatContribution().hasMilitiaCredit() &&
		       copiedSoldier.combatContribution().damageByTeam()[0] == 0 &&
		       copiedSoldier.combatContribution().damageByTeam()[NUM_ASSIST_SLOTS - 1] == 0,
		       "soldier initialization resets the complete combat-contribution domain" );
		CHECK( !copiedSoldier.damageDisplay().displaying() &&
		       copiedSoldier.damageDisplay().counter() == 0 &&
		       copiedSoldier.damageDisplay().offsetX() == 0 &&
		       copiedSoldier.damageDisplay().offsetY() == 0 &&
		       copiedSoldier.damageDisplay().direction() == 0,
		       "soldier initialization resets the complete damage-display domain" );
		CHECK( copiedSoldier.renderState().headPalette()[0] == '\0' &&
		       copiedSoldier.renderState().pantsPalette()[0] == '\0' &&
		       !copiedSoldier.renderState().fading() &&
		       copiedSoldier.renderState().fadeLevel() == 0 &&
		       copiedSoldier.renderState().fadeOriginGrid() == 0 &&
		       !copiedSoldier.renderState().forceRenderColor() &&
		       !copiedSoldier.renderState().forceNoPaletteCycle() &&
		       !copiedSoldier.renderState().forceShade() &&
		       !copiedSoldier.renderState().muzzleFlashVisible() &&
		       copiedSoldier.renderState().unblitWidth() == 0 &&
		       copiedSoldier.renderState().unblitHeight() == 0 &&
		       !copiedSoldier.renderState().hasLightSprite() &&
		       !copiedSoldier.renderState().hasMuzzleFlashSprite() &&
		       copiedSoldier.renderState().muzzleFlashFrame() == 0 &&
		       copiedSoldier.renderState().boundingBoxWidth() == 0 &&
		       copiedSoldier.renderState().boundingBoxHeight() == 0,
		       "soldier initialization resets the complete render-state domain" );
		CHECK( copiedSoldier.uiPresentation().portraitFlashFrame() == 0 &&
		       copiedSoldier.uiPresentation().locatorFrame() == 0 &&
		       copiedSoldier.uiPresentation().locatorOffsetX() == 0 &&
		       copiedSoldier.uiPresentation().locatorOffsetY() == 0 &&
		       copiedSoldier.uiPresentation().interfaceLevel() == 0 &&
		       copiedSoldier.uiPresentation().closePanelFrame() == 0 &&
		       copiedSoldier.uiPresentation().deadPanelFrame() == 0 &&
		       copiedSoldier.uiPresentation().openPanelFrame() == 0 &&
		       copiedSoldier.uiPresentation().panelFaceX() == 0 &&
		       copiedSoldier.uiPresentation().panelFaceY() == 0 &&
		       copiedSoldier.uiPresentation().plannedActionPointCost() == 0 &&
		       copiedSoldier.uiPresentation().plannedTargetX() == 0 &&
		       copiedSoldier.uiPresentation().plannedTargetY() == 0 &&
		       copiedSoldier.uiPresentation().lastEnemyCycled() == SoldierID{} &&
		       copiedSoldier.uiPresentation().locateCycles() == 0,
		       "soldier initialization resets the complete UI-presentation domain" );
		CHECK( copiedSoldier.suppression().underFire() == 0 &&
		       copiedSoldier.suppression().shock() == 0 &&
		       copiedSoldier.suppression().points() == 0 &&
		       copiedSoldier.suppression().actionPointsLost() == 0 &&
		       copiedSoldier.suppression().suppressor() == NOBODY &&
		       !copiedSoldier.suppression().closeCall(),
		       "soldier initialization resets the complete suppression domain" );
		CHECK( copiedSoldier.animationIntent().desiredHeight() == NO_DESIRED_HEIGHT &&
		       copiedSoldier.animationIntent().pendingAnimation() == NO_PENDING_ANIMATION &&
		       copiedSoldier.animationIntent().pendingStance() == NO_PENDING_STANCE &&
		       copiedSoldier.animationIntent().secondaryPendingAnimation() == NO_PENDING_ANIMATION &&
		       copiedSoldier.animationIntent().pendingDirection() == NO_PENDING_DIRECTION &&
		       !copiedSoldier.animationIntent().turningFromUi() &&
		       !copiedSoldier.animationIntent().stopPendingNextTile() &&
		       copiedSoldier.animationIntent().continuationMode() == 0,
		       "soldier initialization resets the complete animation-intent domain" );
		CHECK( copiedSoldier.animationPlayback().state() == 0 &&
		       copiedSoldier.animationPlayback().code() == 0 &&
		       copiedSoldier.animationPlayback().frame() == 0 &&
		       copiedSoldier.animationPlayback().delay() == 0 &&
		       copiedSoldier.animationPlayback().previousState() == 0 &&
		       copiedSoldier.animationPlayback().previousCode() == 0 &&
		       copiedSoldier.animationPlayback().surface() == 0 &&
		       copiedSoldier.animationPlayback().zLevel() == 0 &&
		       copiedSoldier.animationPlayback().subFlags() == 0,
		       "soldier initialization resets the complete animation-playback domain" );
		CHECK( copiedSoldier.animationActivity().turningFromProneMode() == 0 &&
		       !copiedSoldier.animationActivity().readyCostWaived() &&
		       copiedSoldier.animationActivity().postHitStance() == 0 &&
		       !copiedSoldier.animationActivity().paused() &&
		       !copiedSoldier.animationActivity().holdAttackerUntilDone() &&
		       !copiedSoldier.animationActivity().turningToShoot() &&
		       !copiedSoldier.animationActivity().turningToFall() &&
		       !copiedSoldier.animationActivity().turningUntilDone() &&
		       copiedSoldier.animationActivity().hitPhase() == 0 &&
		       !copiedSoldier.animationActivity().nonInterruptible() &&
		       !copiedSoldier.animationActivity().turningCostWaived() &&
		       !copiedSoldier.animationActivity().suppressionStanceChange() &&
		       !copiedSoldier.animationActivity().stanceCostWaived() &&
		       !copiedSoldier.animationActivity().realtimeNonInterruptible() &&
		       !copiedSoldier.animationActivity().tryingToFall() &&
		       !copiedSoldier.animationActivity().fallClockwise() &&
		       copiedSoldier.animationActivity().fallDirection() == 0 &&
		       copiedSoldier.animationActivity().turningIncrement() == 0 &&
		       copiedSoldier.animationActivity().traversalForecastGrid() == 0 &&
		       copiedSoldier.animationActivity().hasRenderZOverride() &&
		       copiedSoldier.animationActivity().renderZOverride() == 0 &&
		       copiedSoldier.animationActivity().randomActionCheckCounter() == 0 &&
		       copiedSoldier.animationActivity().lastRandomAnimation() == 0,
		       "soldier initialization resets the complete animation-activity domain" );
	}

	{
		auto legacySoldier = std::make_unique<OLDSOLDIERTYPE_101>();
		legacySoldier->fDoSpread = TRUE;
		legacySoldier->autofireLastStep = TRUE;
		legacySoldier->bBulletsLeft = 3;
		legacySoldier->bDoBurst = 4;
		legacySoldier->bDoAutofire = 8;
		legacySoldier->sWalkToAttackGridNo = 1700;
		legacySoldier->sWalkToAttackWalkToCost = 23;
		legacySoldier->sStartGridNo = 1701;
		legacySoldier->sEndGridNo = 1702;
		legacySoldier->sForcastGridNo = 1703;
		legacySoldier->sZLevelOverride = 701;
		legacySoldier->uiTimeOfLastRandomAction = 73;
		legacySoldier->usLastRandomAnim = 702;
		legacySoldier->fIgnoreGetupFromCollapseCheck = TRUE;
		legacySoldier->GetupFromJA25StartCounter = 1704;
		legacySoldier->fWaitingToGetupFromJA25Start = TRUE;
		std::strcpy(legacySoldier->HeadPal, "BLACKHEAD");
		std::strcpy(legacySoldier->PantsPal, "BLUEPANTS");
		std::strcpy(legacySoldier->VestPal, "BLUEVEST");
		std::strcpy(legacySoldier->SkinPal, "TANSKIN");
		std::strcpy(legacySoldier->MiscPal, "OLDMISC");
		legacySoldier->fBeginFade = 2;
		legacySoldier->ubFadeLevel = 19;
		legacySoldier->fForceRenderColor = TRUE;
		legacySoldier->fForceNoRenderPaletteCycle = TRUE;
		legacySoldier->fForceShade = TRUE;
		legacySoldier->fMuzzleFlash = TRUE;
		legacySoldier->usUnblitX = 31;
		legacySoldier->usUnblitY = 32;
		legacySoldier->usUnblitWidth = 33;
		legacySoldier->usUnblitHeight = 34;
		legacySoldier->iLight = 701;
		legacySoldier->iMuzFlash = 702;
		legacySoldier->bMuzFlashCount = 4;
		legacySoldier->sBoundingBoxWidth = 61;
		legacySoldier->sBoundingBoxHeight = 62;
		legacySoldier->sBoundingBoxOffsetX = -7;
		legacySoldier->sBoundingBoxOffsetY = -8;
		legacySoldier->sLocationOfFadeStart = 1705;
		legacySoldier->fTurnInProgress = TRUE;
		legacySoldier->fPrevInWater = TRUE;
		legacySoldier->fUIMovementFast = 2;
		legacySoldier->fNoAPToFinishMove = TRUE;
		legacySoldier->fPausedMove = TRUE;
		legacySoldier->fIsSoldierMoving = TRUE;
		legacySoldier->fIsSoldierDelayed = TRUE;
		legacySoldier->fSoldierWasMoving = TRUE;
		legacySoldier->fPastXDest = -2;
		legacySoldier->fPastYDest = 3;
		legacySoldier->bMovedPriorToInterrupt = 1;
		legacySoldier->bActionPoints = 43;
		legacySoldier->bInitialActionPoints = 78;
		legacySoldier->bOldLife = 72;
		legacySoldier->sFractLife = 35;
		legacySoldier->bLife = 70;
		legacySoldier->bLifeMax = 88;
		legacySoldier->bBleeding = 9;
		legacySoldier->bBreath = 61;
		legacySoldier->bBreathMax = 93;
		legacySoldier->sBreathRed = 444;
		legacySoldier->dNextBleed = 12.5f;
		legacySoldier->bRegenerationCounter = -2;
		legacySoldier->bRegenBoostersUsedToday = 3;
		legacySoldier->uiTimeSinceLastBleedGrunt = 12348;
		legacySoldier->bService = 2;
		legacySoldier->ubServiceCount = 3;
		legacySoldier->ubServicePartner = 7;
		legacySoldier->ubAutoBandagingMedic = 8;
		legacySoldier->ubQuoteRecord = 14;
		legacySoldier->ubQuoteActionID = QUOTE_ACTION_ID_CHECKFORDEST;
		legacySoldier->ubBattleSoundID = 5;
		legacySoldier->usQuoteSaidFlags = SOLDIER_QUOTE_SAID_LOW_BREATH;
		legacySoldier->bVocalVolume = 86;
		legacySoldier->uiTimeSameBattleSndDone = 12350;
		legacySoldier->bOldBattleSnd = BATTLE_SOUND_DIE1;
		legacySoldier->ubTurnsUntilCanSayHeardNoise = 4;
		legacySoldier->usQuoteSaidExtFlags = SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL;
		legacySoldier->uiBattleSoundID = 78;
		legacySoldier->bCurrentCivQuote = -3;
		legacySoldier->bCurrentCivQuoteDelta = 1;
		legacySoldier->uiTimeSinceLastSpoke = 12351;
		legacySoldier->bCorpseQuoteTolerance = 2;
		legacySoldier->bFlashPortraitFrame = -3;
		legacySoldier->sLocatorFrame = 4;
		legacySoldier->sLocatorOffX = 12;
		legacySoldier->sLocatorOffY = -13;
		legacySoldier->bUIInterfaceLevel = 1;
		legacySoldier->ubClosePanelFrame = 2;
		legacySoldier->ubDeadPanelFrame = 3;
		legacySoldier->bOpenPanelFrame = -4;
		legacySoldier->sPanelFaceX = 100;
		legacySoldier->sPanelFaceY = 101;
		legacySoldier->ubPlannedUIAPCost = 14;
		legacySoldier->sPlannedTargetX = 500;
		legacySoldier->sPlannedTargetY = 501;
		legacySoldier->ubLastEnemyCycledID = 15;
		legacySoldier->ubNumLocateCycles = 6;
		legacySoldier->bLastSkillCheck = -6;
		legacySoldier->ubSkillCheckAttempts = 3;
		legacySoldier->sSkillCheckGridNo = 1410;
		legacySoldier->ubPendingAction = MERC_GIVEITEM;
		legacySoldier->ubPendingActionAnimCount = 11;
		legacySoldier->uiPendingActionData1 = 1411;
		legacySoldier->sPendingActionData2 = -1412;
		legacySoldier->bPendingActionData3 = -4;
		legacySoldier->ubDoorHandleCode = 5;
		legacySoldier->uiPendingActionData4 = 1413;
		legacySoldier->iNextActionSpecialData = -1414;
		legacySoldier->ubPendingActionInterrupted = 6;
		legacySoldier->bPendingActionData5 = -7;
		legacySoldier->bCollapsed = TRUE;
		legacySoldier->bBreathCollapsed = TRUE;
		legacySoldier->bTurnsCollapsed = 3;
		legacySoldier->bSleepDrugCounter = 6;
		legacySoldier->fMercCollapsedFlag = TRUE;
		legacySoldier->ubMovementNoiseHeard = (1u << 2) | (1u << 6);
		legacySoldier->bViewRange = 15;
		legacySoldier->bBlindedCounter = 5;
		legacySoldier->bNoiseLevel = 1;
		legacySoldier->uiXRayActivatedTime = 12346;
		legacySoldier->bDeafenedCounter = 4;
		legacySoldier->bVisible = 1;
		legacySoldier->bLastRenderVisibleValue = -1;
		legacySoldier->bNewOppCnt = 3;
		legacySoldier->ubNumTilesMovesSinceLastForget = 202;
		legacySoldier->bCamo = -1;
		legacySoldier->wornCamo = -2;
		legacySoldier->urbanCamo = -3;
		legacySoldier->wornUrbanCamo = -4;
		legacySoldier->desertCamo = -5;
		legacySoldier->wornDesertCamo = -6;
		legacySoldier->snowCamo = -7;
		legacySoldier->wornSnowCamo = -8;
		legacySoldier->iEndofContractTime = -2;
		legacySoldier->iStartContractTime = 18;
		legacySoldier->iTotalContractLength = 21;
		legacySoldier->ubWhatKindOfMercAmI = MERC_TYPE__MERC;
		legacySoldier->usMedicalDeposit = 700;
		legacySoldier->usLifeInsurance = 2;
		legacySoldier->iStartOfInsuranceContract = 5;
		legacySoldier->iTotalLengthOfInsuranceContract = 9;
		legacySoldier->uiTimeOfLastContractUpdate = 12002;
		legacySoldier->bTypeOfLastContract = CONTRACT_EXTEND_1_WEEK;
		legacySoldier->ubMercJustFired = 1;
		legacySoldier->ubContractRenewalQuoteCode = SOLDIER_CONTRACT_RENEW_QUOTE_115_USED;
		legacySoldier->iTimeCanSignElsewhere = 13001;
		legacySoldier->bHospitalPriceModifier = -3;
		legacySoldier->uiStartTimeOfInsuranceContract = 11001;
		legacySoldier->bAssignment = TRAIN_BY_OTHER;
		legacySoldier->bOldAssignment = ON_DUTY;
		legacySoldier->bTrainStat = STRENGTH;
		legacySoldier->uiLastAssignmentChangeMin = 11901;
		legacySoldier->ubDesiredSquadAssignment = 4;
		legacySoldier->ubNumTraversalsAllowedToMerge = 5;
		legacySoldier->ubHoursOnAssignment = 7;
		legacySoldier->bVehicleUnderRepairID = -1;
		legacySoldier->ubInsertionDirection = 4;
		legacySoldier->ubGroupID = 43;
		legacySoldier->sInsertionGridNo = 2300;
		legacySoldier->ubStrategicInsertionCode = INSERTION_CODE_GRIDNO;
		legacySoldier->usStrategicInsertionData = 2301;
		legacySoldier->sSectorX = 10;
		legacySoldier->sSectorY = 6;
		legacySoldier->bSectorZ = 2;
		legacySoldier->iVehicleId = 9;
		legacySoldier->sOffWorldGridNo = 2302;
		legacySoldier->ubPrevSectorID = 32;
		legacySoldier->bUseExitGridForReentryDirection = 1;
		legacySoldier->sPreTraversalGridNo = 2303;
		legacySoldier->ubLeaveHistoryCode = 8;
		legacySoldier->uiTimeSoldierWillArrive = 15000;
		legacySoldier->bEndDoorOpenCode = 2;
		legacySoldier->ubScheduleID = 42;
		legacySoldier->sEndDoorOpenCodeData = 2310;
		legacySoldier->bAIScheduleProgress = 3;
		legacySoldier->ubLastFootPrintSound = 4;
		legacySoldier->ubHiResDirection = 11;
		legacySoldier->ubHiResDesiredDirection = 13;
		legacySoldier->ubDoorOpeningNoise = 19;
		legacySoldier->iBurstSoundID = 501;
		legacySoldier->iPositionSndID = 502;
		legacySoldier->iTuringSoundID = 503;
		legacySoldier->uiStartMovementTime = 16001;
		legacySoldier->uiOptimumMovementTime = 16002;
		legacySoldier->usLastUpdateTime = 16003;
		legacySoldier->uiSoldierUpdateNumber = 504;
		legacySoldier->ubSoldierUpdateType = 8;
		legacySoldier->uiMercChecksum = 505;
		legacySoldier->sWeightCarriedAtTurnStart = 145;
		legacySoldier->bTilesMoved = 8;
		legacySoldier->ubTilesMovedPerRTBreathUpdate = 5;
		legacySoldier->usLastMovementAnimPerRTBreathUpdate = WALKING;
		legacySoldier->usUIMovementMode = SWATTING;
		legacySoldier->bStealthMode = TRUE;
		legacySoldier->bReverse = TRUE;
		legacySoldier->bMovementDirection = 5;
		legacySoldier->usDontUpdateNewGridNoOnMoveAnimChange =
			LOCKED_NO_NEWGRIDNO;
		legacySoldier->numFlanks = 4;
		legacySoldier->lastFlankSpot = 16004;
		legacySoldier->sniper = 1;
		legacySoldier->origDir = 6;
		legacySoldier->dXPos = 321.5f;
		legacySoldier->dYPos = 654.25f;
		legacySoldier->sX = 319;
		legacySoldier->sY = 649;
		legacySoldier->sOldXPos = 300;
		legacySoldier->sOldYPos = 600;
		legacySoldier->sInitialGridNo = 2304;
		legacySoldier->sGridNo = 2305;
		legacySoldier->bLevel = 1;
		legacySoldier->ubDirection = 6;
		legacySoldier->sHeightAdjustment = 21;
		legacySoldier->sDesiredHeight = 29;
		legacySoldier->sTempNewGridNo = 2306;
		legacySoldier->sRoomNo = 13;
		legacySoldier->bOverTerrainType = HIGH_GRASS;
		legacySoldier->bOldOverTerrainType = DIRT_ROAD;
		legacySoldier->sOldGridNo = 2307;
		legacySoldier->sLastTwoLocations[0] = 2308;
		legacySoldier->sLastTwoLocations[1] = 2309;
		legacySoldier->ubAttackerID = 6;
		legacySoldier->ubPreviousAttackerID = 5;
		legacySoldier->ubNextToPreviousAttackerID = 4;
		legacySoldier->ubHitLocation = AIM_SHOT_HEAD;
		legacySoldier->ubLastDamageReason = 7;
		legacySoldier->bNumHitsThisTurn = 3;
		legacySoldier->bNumPelletsHitBy = 2;
		legacySoldier->sDamage = 26;
		legacySoldier->ubMilitiaKills = 6;
		legacySoldier->ubPercentDamageInflictedByTeam[0] = 71;
		legacySoldier->ubPercentDamageInflictedByTeam[NUM_ASSIST_SLOTS - 1] = 72;
		legacySoldier->fDisplayDamage = 2;
		legacySoldier->bDisplayDamageCount = 3;
		legacySoldier->sDamageX = 15;
		legacySoldier->sDamageY = -8;
		legacySoldier->bDamageDir = -1;
		legacySoldier->bUnderFire = 2;
		legacySoldier->bShock = 9;
		legacySoldier->ubSuppressionPoints = 5;
		legacySoldier->ubAPsLostToSuppression = 12;
		legacySoldier->ubSuppressorID = 8;
		legacySoldier->fCloseCall = TRUE;
		for (UINT8 i = 0; i < SoldierFireControlComponent::SpreadTargetCapacity; ++i)
			legacySoldier->sSpreadLocations[i] = 22001 + i;

		SOLDIERTYPE convertedSoldier;
		convertedSoldier.vitals().healableInjury() = 900;
		convertedSoldier.vitals().beginSurgery();
		convertedSoldier.vitals().unregainableBreath() = 333;
		convertedSoldier.vitals().criticalStatDamage()[DAMAGED_STAT_MEDICAL] = 8;
		convertedSoldier.assignment().facilityType() = 9;
		convertedSoldier.assignment().itemMoveSectorId() = 48;
		convertedSoldier.assignment().miniEventHoursRemaining() = 13;
		convertedSoldier.schedule().id() = 99;
		convertedSoldier.schedule().progress() = 98;
		convertedSoldier.schedule().beginDoorContinuation(9999);
		convertedSoldier.schedule().completeDoorAnimation();
		convertedSoldier.audio().recordFootstepVariant(98);
		convertedSoldier.audio().recordDoorOpeningNoise(97);
		convertedSoldier.audio().startBurstSound(9996);
		convertedSoldier.audio().startPositionSound(9997);
		convertedSoldier.audio().startTurningSound(9998);
		convertedSoldier.replication().movementStartedAt() = 9990;
		convertedSoldier.replication().optimumMovementTime() = 9991;
		convertedSoldier.replication().recordUpdate(9992);
		convertedSoldier.replication().updateSequence() = 9993;
		convertedSoldier.replication().updateType() = 99;
		convertedSoldier.replication().scheduleStop(9994);
		convertedSoldier.replication().recordChecksum(9995);
		convertedSoldier.uiPresentation().portraitFlashFrame() = 99;
		convertedSoldier.uiPresentation().startLocator(98);
		convertedSoldier.uiPresentation().setPlannedTarget(999, 998, 97);
		convertedSoldier.uiPresentation().lastEnemyCycled() = SoldierID{ 96 };
		std::strcpy(convertedSoldier.renderState().headPalette(), "STALE");
		convertedSoldier.renderState().beginFade(1, 99, 9999);
		convertedSoldier.renderState().forceRenderColor() = FALSE;
		convertedSoldier.renderState().forceNoPaletteCycle() = FALSE;
		convertedSoldier.renderState().disableForceShade();
		convertedSoldier.renderState().hideMuzzleFlash();
		convertedSoldier.renderState().setUnblitRect(91, 92, 93, 94);
		convertedSoldier.renderState().lightSprite() = 991;
		convertedSoldier.renderState().startMuzzleFlashSprite(992);
		convertedSoldier.renderState().setBoundingBox(91, 92, 93, 94);
		convertedSoldier.movementMetrics().carriedWeightAtTurnStart() = 190;
		convertedSoldier.movementMetrics().tilesMoved() = 90;
		convertedSoldier.movementMetrics().realtimeBreathTiles() = 91;
		convertedSoldier.movementMetrics().lastRealtimeMovementAnimation() = RUNNING;
		convertedSoldier.movement().mode() = RUNNING;
		convertedSoldier.movement().setStealth(false);
		convertedSoldier.movement().setReverse(false);
		convertedSoldier.movement().setHighResolutionFacing(1, 2);
		convertedSoldier.movement().animationDirection() = 3;
		convertedSoldier.movement().requestGridUpdateSuppression();
		convertedSoldier.movement().clearUiMovementFast();
		convertedSoldier.movement().clearPastDestination();
		convertedSoldier.interruptSnapshot().captureMoved(9);
		convertedSoldier.meleeApproach().recordPath(RUNNING, 99, 7);
		convertedSoldier.meleeApproach().rememberGrid(9997);
		convertedSoldier.fireControl().beginSpreadDrag(9998);
		convertedSoldier.fireControl().updateSpreadDrag(9999);
		convertedSoldier.animationActivity().forecastTraversalAt(10000);
		convertedSoldier.animationActivity().setRenderZOverride(1001);
		convertedSoldier.animationActivity().randomActionCheckCounter() = 99;
		convertedSoldier.animationActivity().lastRandomAnimation() = 1002;
		convertedSoldier.deployment().beginArrivalGetup();
		convertedSoldier.deployment().arrivalGetupCounter() = 10003;
		convertedSoldier.aiPlanning().flankCount() = 90;
		convertedSoldier.aiPlanning().flankAnchorGrid() = 9996;
		convertedSoldier.aiPlanning().raiseSniperPosture();
		convertedSoldier.aiPlanning().flankOriginDirection() = 7;
		convertedSoldier.aiPlanning().planIndex() = 99;
		convertedSoldier.skillState().selectedAiSkill() = SKILLS_FOCUS;
		convertedSoldier.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY) = 8;
		convertedSoldier.skillState().counter(SOLDIER_COUNTER_MAX - 1) = 18;
		convertedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_CRYO) = 7;
		convertedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_MAX - 1) = 17;
		convertedSoldier.skillState().focusOn(1411);
		convertedSoldier.condition().extraStrength() = 15;
		convertedSoldier.condition().extraExperienceLevel() = 3;
		convertedSoldier.condition().foodLevel() = 51000;
		convertedSoldier.condition().drinkLevel() = 41000;
		convertedSoldier.condition().starvationHealthDamage() = 5;
		convertedSoldier.condition().starvationStrengthDamage() = 6;
		convertedSoldier.condition().diseasePoints(0) = 111;
		convertedSoldier.condition().diseaseFlags(0) = 0x03;
		convertedSoldier.condition().diseasePoints(NUM_DISEASES - 1) = 222;
		convertedSoldier.condition().diseaseFlags(NUM_DISEASES - 1) = 0x80;
		convertedSoldier.condition().addDisability(2);
		convertedSoldier.longAction().begin(MTA_HACK, 1412, 31);
		convertedSoldier.interaction().nonNpcTraderId() = 10;
		convertedSoldier.interaction().draggedPerson() = SoldierID{ 26 };
		convertedSoldier.interaction().draggedCorpse() = 27;
		convertedSoldier.interaction().chatPartner() = SoldierID{ 28 };
		convertedSoldier.interaction().draggedStructureGrid() = 1413;
		convertedSoldier.pendingAction().begin(MERC_TALK);
		convertedSoldier.pendingAction().primaryData() = 99;
		convertedSoldier.pendingAction().nextSpecialData() = 98;
		convertedSoldier.combatContribution().militiaKills() = 9;
		convertedSoldier.combatContribution().militiaAssists() = 8;
		convertedSoldier.combatContribution().damageByTeam()[0] = 70;
		convertedSoldier = *legacySoldier;
		CHECK( convertedSoldier.vitals().previousHealth() == 72 &&
		       convertedSoldier.vitals().fractionalHealth() == 35 &&
		       convertedSoldier.vitals().health() == 70 &&
		       convertedSoldier.vitals().maximumHealth() == 88 &&
		       convertedSoldier.vitals().bleeding() == 9 &&
		       convertedSoldier.vitals().breath() == 61 &&
		       convertedSoldier.vitals().maximumBreath() == 93 &&
		       convertedSoldier.vitals().breathReduction() == 444 &&
		       convertedSoldier.vitals().nextBleedAt() == 12.5f &&
		       convertedSoldier.vitals().regenerationCounter() == -2 &&
		       convertedSoldier.vitals().regenerationBoostersUsedToday() == 3 &&
		       convertedSoldier.vitals().lastBleedGruntAt() == 12348 &&
		       convertedSoldier.vitals().healableInjury() == 0 &&
		       !convertedSoldier.vitals().isUndergoingSurgery() &&
		       convertedSoldier.vitals().unregainableBreath() == 0 &&
		       convertedSoldier.vitals().criticalStatDamage()[DAMAGED_STAT_MEDICAL] == 0,
		       "v101 soldier conversion maps established vitals and clears fields absent from that schema" );
		CHECK( convertedSoldier.service().activity() == 2 &&
		       convertedSoldier.service().providerCount() == 3 &&
		       convertedSoldier.service().partner() == SoldierID{ 7 } &&
		       convertedSoldier.service().autoBandagingMedic() == SoldierID{ 8 },
		       "v101 soldier conversion retains the complete tactical service relationship" );
		CHECK( convertedSoldier.dialogue().quoteRecord() == 14 &&
		       convertedSoldier.dialogue().quoteActionId() == QUOTE_ACTION_ID_CHECKFORDEST &&
		       convertedSoldier.dialogue().battleSoundSet() == 5 &&
		       convertedSoldier.dialogue().saidFlags() == SOLDIER_QUOTE_SAID_LOW_BREATH &&
		       convertedSoldier.dialogue().vocalVolume() == 86 &&
		       convertedSoldier.dialogue().repeatedBattleSoundAt() == 12350 &&
		       convertedSoldier.dialogue().previousBattleSound() == BATTLE_SOUND_DIE1 &&
		       convertedSoldier.dialogue().heardNoiseCooldownTurns() == 4 &&
		       convertedSoldier.dialogue().saidExtendedFlags() == SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL &&
		       convertedSoldier.dialogue().activeBattleSound() == 78 &&
		       convertedSoldier.dialogue().currentCivilianQuote() == -3 &&
		       convertedSoldier.dialogue().civilianQuoteDelta() == 1 &&
		       convertedSoldier.dialogue().lastSpokeAt() == 12351 &&
		       convertedSoldier.dialogue().corpseQuoteTolerance() == 2,
		       "v101 soldier conversion retains the complete spoken-dialogue domain" );
		CHECK( convertedSoldier.audio().lastFootstepVariant() == 4 &&
		       convertedSoldier.audio().doorOpeningNoise() == 19 &&
		       convertedSoldier.audio().burstSoundId() == 501 &&
		       convertedSoldier.audio().positionSoundId() == 502 &&
		       convertedSoldier.audio().turningSoundId() == 503,
		       "v101 soldier conversion retains the complete non-dialogue audio domain" );
		CHECK( convertedSoldier.replication().movementStartedAt() == 16001 &&
		       convertedSoldier.replication().optimumMovementTime() == 16002 &&
		       convertedSoldier.replication().lastUpdateAt() == 16003 &&
		       convertedSoldier.replication().updateSequence() == 504 &&
		       convertedSoldier.replication().updateType() == 8 &&
		       convertedSoldier.replication().scheduledStopGrid() == 0 &&
		       convertedSoldier.replication().checksum() == 505,
		       "v101 soldier conversion maps established replication metadata and clears the later scheduled-stop field" );
		CHECK( convertedSoldier.movementMetrics().carriedWeightAtTurnStart() == 145 &&
		       convertedSoldier.movementMetrics().tilesMoved() == 8 &&
		       convertedSoldier.movementMetrics().realtimeBreathTiles() == 5 &&
		       convertedSoldier.movementMetrics().lastRealtimeMovementAnimation() == WALKING,
		       "v101 soldier conversion retains the complete movement telemetry domain" );
		CHECK( convertedSoldier.aiPlanning().flankCount() == 4 &&
		       convertedSoldier.aiPlanning().flankAnchorGrid() == 16004 &&
		       convertedSoldier.aiPlanning().sniperPostureActive() &&
		       convertedSoldier.aiPlanning().flankOriginDirection() == 6 &&
		       !convertedSoldier.aiPlanning().hasPlanIndex(),
		       "v101 soldier conversion maps established AI planning and clears the later modular plan index" );
		CHECK( convertedSoldier.uiPresentation().portraitFlashFrame() == -3 &&
		       convertedSoldier.uiPresentation().locatorFrame() == 4 &&
		       convertedSoldier.uiPresentation().locatorOffsetX() == 12 &&
		       convertedSoldier.uiPresentation().locatorOffsetY() == -13 &&
		       convertedSoldier.uiPresentation().interfaceLevel() == 1 &&
		       convertedSoldier.uiPresentation().closePanelFrame() == 2 &&
		       convertedSoldier.uiPresentation().deadPanelFrame() == 3 &&
		       convertedSoldier.uiPresentation().openPanelFrame() == -4 &&
		       convertedSoldier.uiPresentation().panelFaceX() == 100 &&
		       convertedSoldier.uiPresentation().panelFaceY() == 101 &&
		       convertedSoldier.uiPresentation().plannedActionPointCost() == 14 &&
		       convertedSoldier.uiPresentation().plannedTargetX() == 500 &&
		       convertedSoldier.uiPresentation().plannedTargetY() == 501 &&
		       convertedSoldier.uiPresentation().lastEnemyCycled() == SoldierID{ 15 } &&
		       convertedSoldier.uiPresentation().locateCycles() == 6,
		       "v101 soldier conversion retains the complete UI-presentation domain" );
		CHECK( convertedSoldier.skillState().lastCheckReason() == -6 &&
		       convertedSoldier.skillState().checkAttempts() == 3 &&
		       convertedSoldier.skillState().checkGrid() == 1410 &&
		       convertedSoldier.skillState().selectedAiSkill() == 0 &&
		       convertedSoldier.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY) == 0 &&
		       convertedSoldier.skillState().counter(SOLDIER_COUNTER_MAX - 1) == 0 &&
		       convertedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_CRYO) == 0 &&
		       convertedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_MAX - 1) == 0 &&
		       convertedSoldier.skillState().focusGrid() == 0,
		       "v101 soldier conversion maps established skill checks and clears skill state absent from that schema" );
		CHECK( convertedSoldier.condition().extraStrength() == 0 &&
		       convertedSoldier.condition().extraDexterity() == 0 &&
		       convertedSoldier.condition().extraAgility() == 0 &&
		       convertedSoldier.condition().extraWisdom() == 0 &&
		       convertedSoldier.condition().extraExperienceLevel() == 0 &&
		       convertedSoldier.condition().foodLevel() == 0 &&
		       convertedSoldier.condition().drinkLevel() == 0 &&
		       convertedSoldier.condition().starvationHealthDamage() == 0 &&
		       convertedSoldier.condition().starvationStrengthDamage() == 0 &&
		       convertedSoldier.condition().diseasePoints(0) == 0 &&
		       convertedSoldier.condition().diseaseFlags(0) == 0 &&
		       convertedSoldier.condition().diseasePoints(NUM_DISEASES - 1) == 0 &&
		       convertedSoldier.condition().diseaseFlags(NUM_DISEASES - 1) == 0 &&
		       convertedSoldier.condition().disabilityFlags() == 0,
		       "v101 soldier conversion clears condition state absent from that schema" );
		CHECK( !convertedSoldier.longAction().active() &&
		       convertedSoldier.longAction().action() == MTA_NONE &&
		       convertedSoldier.longAction().contextGrid() == -1 &&
		       convertedSoldier.longAction().remainingActionPoints() == 0,
		       "v101 soldier conversion clears long-action state absent from that schema" );
		CHECK( convertedSoldier.interaction().nonNpcTraderId() == 0 &&
		       convertedSoldier.interaction().draggedPerson() == NOBODY &&
		       convertedSoldier.interaction().draggedCorpse() == -1 &&
		       convertedSoldier.interaction().chatPartner() == NOBODY &&
		       convertedSoldier.interaction().draggedStructureGrid() == -1 &&
		       !convertedSoldier.interaction().dragging() &&
		       !convertedSoldier.interaction().chatting(),
		       "v101 soldier conversion clears interaction state absent from that schema" );
		CHECK( convertedSoldier.pendingAction().active() &&
		       convertedSoldier.pendingAction().action() == MERC_GIVEITEM &&
		       convertedSoldier.pendingAction().animationCount() == 11 &&
		       convertedSoldier.pendingAction().primaryData() == 1411 &&
		       convertedSoldier.pendingAction().secondaryData() == -1412 &&
		       convertedSoldier.pendingAction().tertiaryData() == -4 &&
		       convertedSoldier.pendingAction().doorHandleCode() == 5 &&
		       convertedSoldier.pendingAction().quaternaryData() == 1413 &&
		       convertedSoldier.pendingAction().nextSpecialData() == -1414 &&
		       convertedSoldier.pendingAction().interruptionMarker() == 6 &&
		       convertedSoldier.pendingAction().inventorySlot() == -7,
		       "v101 soldier conversion retains the complete persistent pending-action domain" );
		CHECK( convertedSoldier.actionPoints().current() == 43 &&
		       convertedSoldier.actionPoints().initial() == 78,
		       "v101 soldier conversion retains current and turn-start action-point budgets" );
		CHECK( convertedSoldier.collapseState().collapsed() &&
		       convertedSoldier.collapseState().breathCollapsed() &&
		       convertedSoldier.collapseState().turns() == 3 &&
		       convertedSoldier.collapseState().sleepDrugCounter() == 6 &&
		       convertedSoldier.collapseState().fatigueCollapsed(),
		       "v101 soldier conversion retains tactical and strategic collapse state" );
		CHECK( convertedSoldier.perception().hasHeardMovementFrom(2) &&
		       convertedSoldier.perception().hasHeardMovementFrom(6) &&
		       convertedSoldier.perception().viewRange() == 15 &&
		       convertedSoldier.perception().blindnessTurns() == 5 &&
		       convertedSoldier.perception().heardNoiseLevel() == 1 &&
		       convertedSoldier.perception().xrayActivatedAt() == 12346 &&
		       convertedSoldier.perception().deafnessTurns() == 4,
		       "v101 soldier conversion retains sensory range, memory, effects, and X-ray lifetime" );
		CHECK( convertedSoldier.awareness().visibility() == 1 &&
		       convertedSoldier.awareness().lastRenderedVisibility() == -1 &&
		       convertedSoldier.awareness().newOpponentCount() == 3 &&
		       convertedSoldier.awareness().tilesSinceForget() == 202,
		       "v101 soldier conversion retains visibility, render synchronization, discovery, and stale-memory distance" );
		CHECK( convertedSoldier.camouflage().jungleApplied() == -1 &&
		       convertedSoldier.camouflage().jungleWorn() == -2 &&
		       convertedSoldier.camouflage().urbanApplied() == -3 &&
		       convertedSoldier.camouflage().urbanWorn() == -4 &&
		       convertedSoldier.camouflage().desertApplied() == -5 &&
		       convertedSoldier.camouflage().desertWorn() == -6 &&
		       convertedSoldier.camouflage().snowApplied() == -7 &&
		       convertedSoldier.camouflage().snowWorn() == -8,
		       "v101 soldier conversion retains all applied and equipment-derived camouflage values" );
		CHECK( convertedSoldier.employment().endTime() == -2 &&
		       convertedSoldier.employment().startTime() == 18 &&
		       convertedSoldier.employment().totalLength() == 21 &&
		       convertedSoldier.employment().mercenaryType() == MERC_TYPE__MERC &&
		       convertedSoldier.employment().medicalDeposit() == 700 &&
		       convertedSoldier.employment().lifeInsurance() == 2 &&
		       convertedSoldier.employment().insuranceStartDay() == 5 &&
		       convertedSoldier.employment().insuranceLengthDays() == 9 &&
		       convertedSoldier.employment().lastContractUpdateTime() == 12002 &&
		       convertedSoldier.employment().lastContractType() == CONTRACT_EXTEND_1_WEEK &&
		       convertedSoldier.employment().justFired() == 1 &&
		       convertedSoldier.employment().renewalQuoteCode() == SOLDIER_CONTRACT_RENEW_QUOTE_115_USED &&
		       convertedSoldier.employment().timeCanSignElsewhere() == 13001 &&
		       convertedSoldier.employment().hospitalPriceModifier() == -3 &&
		       convertedSoldier.employment().insuranceStartTime() == 11001,
		       "v101 soldier conversion retains the complete employment and insurance lifecycle" );
		CHECK( convertedSoldier.assignment().current() == TRAIN_BY_OTHER &&
		       convertedSoldier.assignment().previous() == ON_DUTY &&
		       convertedSoldier.assignment().trainingStat() == STRENGTH &&
		       convertedSoldier.assignment().lastChangeMinute() == 11901 &&
		       convertedSoldier.assignment().desiredSquad() == 4 &&
		       convertedSoldier.assignment().mergeTraversalAllowance() == 5 &&
		       convertedSoldier.assignment().hours() == 7 &&
		       convertedSoldier.assignment().repairVehicleId() == -1 &&
		       convertedSoldier.assignment().facilityType() == 0 &&
		       convertedSoldier.assignment().itemMoveSectorId() == 0 &&
		       convertedSoldier.assignment().miniEventHoursRemaining() == 0,
		       "v101 soldier conversion retains legacy assignment state and clears fields absent from v101" );
		CHECK( convertedSoldier.deployment().insertionDirection() == 4 &&
		       convertedSoldier.deployment().groupId() == 43 &&
		       convertedSoldier.deployment().insertionGrid() == 2300 &&
		       convertedSoldier.deployment().strategicInsertionCode() == INSERTION_CODE_GRIDNO &&
		       convertedSoldier.deployment().strategicInsertionData() == 2301 &&
		       convertedSoldier.deployment().isInSector(10, 6, 2) &&
		       convertedSoldier.deployment().vehicleId() == 9 &&
		       convertedSoldier.deployment().offWorldGrid() == 2302 &&
		       convertedSoldier.deployment().previousSectorId() == 32 &&
		       convertedSoldier.deployment().useExitGridForReentryDirection() == 1 &&
		       convertedSoldier.deployment().preTraversalGrid() == 2303 &&
		       convertedSoldier.deployment().leaveHistoryCode() == 8 &&
		       convertedSoldier.deployment().arrivalTime() == 15000 &&
		       !convertedSoldier.deployment().arrivalGetupPending() &&
		       !convertedSoldier.deployment().ignoreCollapseGetupCheck() &&
		       convertedSoldier.deployment().arrivalGetupCounter() == 0,
		       "v101 soldier conversion retains deployment while clearing historically ignored arrival get-up state" );
		CHECK( convertedSoldier.schedule().id() == 42 &&
		       convertedSoldier.schedule().progress() == 3 &&
		       !convertedSoldier.schedule().doorContinuationPending() &&
		       convertedSoldier.schedule().doorGrid() == 2310,
		       "v101 soldier conversion maps schedule data while clearing the historically ignored door phase" );
		CHECK( convertedSoldier.position().worldX() == 321.5f &&
		       convertedSoldier.position().worldY() == 654.25f &&
		       convertedSoldier.position().worldXInt() == 319 &&
		       convertedSoldier.position().worldYInt() == 649 &&
		       convertedSoldier.position().turnStartX() == 300 &&
		       convertedSoldier.position().turnStartY() == 600 &&
		       convertedSoldier.position().initialGrid() == 2304 &&
		       convertedSoldier.position().gridNo() == 2305 &&
		       convertedSoldier.position().level() == 1 &&
		       convertedSoldier.position().direction() == 6 &&
		       convertedSoldier.position().heightAdjustment() == 21 &&
		       convertedSoldier.position().desiredHeight() == 29 &&
		       convertedSoldier.position().temporaryGrid() == 2306 &&
		       convertedSoldier.position().roomNo() == 13 &&
		       convertedSoldier.position().terrainType() == HIGH_GRASS &&
		       convertedSoldier.position().previousTerrainType() == DIRT_ROAD,
		       "v101 soldier conversion retains every historical tactical world-placement value" );
		CHECK( convertedSoldier.movementHistory().previousGrid() == 2307 &&
		       convertedSoldier.movementHistory().recentLocations()[0] == 2308 &&
		       convertedSoldier.movementHistory().recentLocations()[1] == 2309,
		       "v101 soldier conversion retains every historical tactical movement-history value" );
		CHECK( convertedSoldier.movement().mode() == SWATTING &&
		       convertedSoldier.movement().stealthy() &&
		       convertedSoldier.movement().reversing() &&
		       convertedSoldier.movement().highResolutionDirection() == 11 &&
		       convertedSoldier.movement().highResolutionDesiredDirection() == 13 &&
		       convertedSoldier.movement().animationDirection() == 5 &&
		       convertedSoldier.movement().gridUpdatePolicy() ==
		           LOCKED_NO_NEWGRIDNO &&
		       convertedSoldier.movement().turnActive() &&
		       convertedSoldier.movement().wasInWater() &&
		       convertedSoldier.movement().uiMovementFast() == 2 &&
		       convertedSoldier.movement().outOfActionPoints() &&
		       convertedSoldier.movement().movementPaused() &&
		       convertedSoldier.movement().recordingMovement() &&
		       convertedSoldier.movement().delayedByNetwork() &&
		       convertedSoldier.movement().wasMoving() &&
		       convertedSoldier.movement().pastXDestination() == -2 &&
		       convertedSoldier.movement().pastYDestination() == 3,
		       "v101 soldier conversion retains movement intent, facing, and complete activity state" );
		CHECK( convertedSoldier.meleeApproach().movementMode() == 0 &&
		       convertedSoldier.meleeApproach().grid() == 1700 &&
		       convertedSoldier.meleeApproach().cost() == 23 &&
		       convertedSoldier.meleeApproach().endDirection() == 0,
		       "v101 soldier conversion maps the historical melee cache and clears later key fields" );
		CHECK( convertedSoldier.interruptSnapshot().movedBeforeInterrupt() == 1,
		       "v101 soldier conversion retains the pre-interrupt moved snapshot" );
		CHECK( convertedSoldier.animationActivity().traversalForecastGrid() == 1703 &&
		       convertedSoldier.animationActivity().hasRenderZOverride() &&
		       convertedSoldier.animationActivity().renderZOverride() == 701 &&
		       convertedSoldier.animationActivity().randomActionCheckCounter() == 73 &&
		       convertedSoldier.animationActivity().lastRandomAnimation() == 702,
		       "v101 soldier conversion retains traversal, render-depth, and random-animation state" );
		CHECK( convertedSoldier.fireControl().spreadIndex() == TRUE &&
		       convertedSoldier.fireControl().autofireLastStep() &&
		       convertedSoldier.fireControl().bulletsLeft() == 3 &&
		       convertedSoldier.fireControl().burstCounter() == 4 &&
		       convertedSoldier.fireControl().autofireShots() == 8 &&
		       convertedSoldier.fireControl().spreadLocations()[0] == 22001 &&
		       convertedSoldier.fireControl().spreadLocations()[1] == 22002 &&
		       convertedSoldier.fireControl().spreadLocations()[2] == 22003 &&
		       convertedSoldier.fireControl().spreadLocations()[3] == 22004 &&
		       convertedSoldier.fireControl().spreadLocations()[4] == 22005 &&
		       convertedSoldier.fireControl().spreadLocations()[5] == 22006 &&
		       convertedSoldier.fireControl().spreadDragStartGrid() == 1701 &&
		       convertedSoldier.fireControl().spreadDragEndGrid() == 1702 &&
		       convertedSoldier.fireControl().spreadDragMoved(),
		       "v101 soldier conversion retains the complete fire-control spread array" );
		CHECK( convertedSoldier.combatResult().currentAttacker() == SoldierID{ 6 } &&
		       convertedSoldier.combatResult().previousAttacker() == SoldierID{ 5 } &&
		       convertedSoldier.combatResult().earlierAttacker() == SoldierID{ 4 } &&
		       convertedSoldier.combatResult().hitLocation() == AIM_SHOT_HEAD &&
		       convertedSoldier.combatResult().lastDamageReason() == 7 &&
		       convertedSoldier.combatResult().hitsThisTurn() == 3 &&
		       convertedSoldier.combatResult().pelletsHitBy() == 2 &&
		       convertedSoldier.combatResult().accumulatedDamage() == 26,
		       "v101 soldier conversion retains incoming combat attribution and outcome state" );
		CHECK( convertedSoldier.combatContribution().militiaKills() == 6 &&
		       convertedSoldier.combatContribution().militiaAssists() == 0 &&
		       convertedSoldier.combatContribution().damageByTeam()[0] == 71 &&
		       convertedSoldier.combatContribution().damageByTeam()[NUM_ASSIST_SLOTS - 1] == 72,
		       "v101 soldier conversion retains militia kills and every established assist-attribution slot" );
		CHECK( convertedSoldier.damageDisplay().displayFlag() == 2 &&
		       convertedSoldier.damageDisplay().counter() == 3 &&
		       convertedSoldier.damageDisplay().offsetX() == 15 &&
		       convertedSoldier.damageDisplay().offsetY() == -8 &&
		       convertedSoldier.damageDisplay().direction() == -1,
		       "v101 soldier conversion retains floating damage-display presentation state" );
		CHECK( std::strcmp(convertedSoldier.renderState().headPalette(), "BLACKHEAD") == 0 &&
		       std::strcmp(convertedSoldier.renderState().pantsPalette(), "BLUEPANTS") == 0 &&
		       std::strcmp(convertedSoldier.renderState().vestPalette(), "BLUEVEST") == 0 &&
		       std::strcmp(convertedSoldier.renderState().skinPalette(), "TANSKIN") == 0 &&
		       std::strcmp(convertedSoldier.renderState().miscPalette(), "OLDMISC") == 0 &&
		       convertedSoldier.renderState().fadeMode() == 2 &&
		       convertedSoldier.renderState().fadeLevel() == 19 &&
		       convertedSoldier.renderState().fadeOriginGrid() == 1705 &&
		       convertedSoldier.renderState().forceRenderColor() &&
		       convertedSoldier.renderState().forceNoPaletteCycle() &&
		       convertedSoldier.renderState().forceShade() &&
		       convertedSoldier.renderState().muzzleFlashVisible() &&
		       convertedSoldier.renderState().unblitX() == 31 &&
		       convertedSoldier.renderState().unblitY() == 32 &&
		       convertedSoldier.renderState().unblitWidth() == 33 &&
		       convertedSoldier.renderState().unblitHeight() == 34 &&
		       convertedSoldier.renderState().lightSprite() == 701 &&
		       convertedSoldier.renderState().muzzleFlashSprite() == 702 &&
		       convertedSoldier.renderState().muzzleFlashFrame() == 4 &&
		       convertedSoldier.renderState().boundingBoxWidth() == 61 &&
		       convertedSoldier.renderState().boundingBoxHeight() == 62 &&
		       convertedSoldier.renderState().boundingBoxOffsetX() == -7 &&
		       convertedSoldier.renderState().boundingBoxOffsetY() == -8,
		       "v101 soldier conversion retains palette, fade, light, redraw, and geometry render state" );
		CHECK( convertedSoldier.suppression().underFire() == 2 &&
		       convertedSoldier.suppression().shock() == 9 &&
		       convertedSoldier.suppression().points() == 5 &&
		       convertedSoldier.suppression().actionPointsLost() == 12 &&
		       convertedSoldier.suppression().suppressor() == SoldierID{ 8 } &&
		       convertedSoldier.suppression().closeCall(),
		       "v101 soldier conversion retains suppression and hostile-fire reaction state" );
	}

	{
		SoldierRuntimeComponents runtime;
		runtime.pendingAction.pathSearchSourceGrid = 1234;
		runtime.pendingAction.targetIncarnation = 99;
		runtime.pendingAction.grenadeItem = 42;
		runtime.pendingAction.delayedDamage = [] {};
		runtime.combatFeedback.lastShock = 7;
		runtime.combatFeedback.lastSuppression = 8;
		runtime.combatFeedback.lastActionPoints = 9;
		runtime.combatFeedback.lastMorale = 10;
		runtime.combatFeedback.lastShockFromHit = 11;
		runtime.combatFeedback.lastActionPointsFromHit = 12;
		runtime.combatFeedback.lastMoraleFromHit = 13;
		runtime.combatFeedback.lastBulletImpact = 14;
		runtime.combatFeedback.lastArmourProtection = 15;
		runtime.quickItem.itemId = 16;
		runtime.quickItem.slot = 17;

		const SoldierRuntimeComponents copiedRuntime = runtime;
		SoldierRuntimeComponents assignedRuntime;
		assignedRuntime = runtime;
		CHECK( copiedRuntime.pendingAction.pathSearchSourceGrid == 0 &&
		       copiedRuntime.pendingAction.targetIncarnation == 0 &&
		       !copiedRuntime.pendingAction.delayedDamage &&
		       copiedRuntime.combatFeedback.lastShock == 0 &&
		       copiedRuntime.quickItem.itemId == 0 &&
		       assignedRuntime.pendingAction.grenadeItem == 0 &&
		       !assignedRuntime.pendingAction.delayedDamage &&
		       assignedRuntime.combatFeedback.lastArmourProtection == 0 &&
		       assignedRuntime.quickItem.slot == 0,
		       "soldier clones do not inherit transient callbacks or UI state" );

		runtime.reset();
		CHECK( runtime.pendingAction.pathSearchSourceGrid == 0 &&
		       runtime.pendingAction.targetIncarnation == 0 &&
		       runtime.pendingAction.grenadeItem == 0 &&
		       !runtime.pendingAction.delayedDamage,
		       "soldier pending-action runtime component resets transient work" );
		CHECK( runtime.combatFeedback.lastShock == 0 &&
		       runtime.combatFeedback.lastSuppression == 0 &&
		       runtime.combatFeedback.lastActionPoints == 0 &&
		       runtime.combatFeedback.lastMorale == 0 &&
		       runtime.combatFeedback.lastShockFromHit == 0 &&
		       runtime.combatFeedback.lastActionPointsFromHit == 0 &&
		       runtime.combatFeedback.lastMoraleFromHit == 0 &&
		       runtime.combatFeedback.lastBulletImpact == 0 &&
		       runtime.combatFeedback.lastArmourProtection == 0,
		       "soldier combat-feedback runtime component resets as one domain" );
		CHECK( runtime.quickItem.itemId == 0 && runtime.quickItem.slot == 0,
		       "soldier quick-item runtime component resets retained UI state" );
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
		const std::string path = "soldier_vitals_roundtrip_test.bin";
		const UINT32 previousSaveVersion = guiCurrentSaveGameVersion;
		guiCurrentSaveGameVersion = SAVE_GAME_VERSION;

		SOLDIERTYPE savedSoldier;
		savedSoldier.stats.bExpLevel = 6;
		savedSoldier.stats.bStrength = 77;
		savedSoldier.vitals().health() = 71;
		savedSoldier.vitals().maximumHealth() = 89;
		savedSoldier.vitals().breath() = 62;
		savedSoldier.vitals().maximumBreath() = 94;
		savedSoldier.vitals().bleeding() = 11;
		savedSoldier.vitals().previousHealth() = 69;
		savedSoldier.vitals().fractionalHealth() = 43;
		savedSoldier.vitals().breathReduction() = 445;
		savedSoldier.vitals().healableInjury() = 1800;
		savedSoldier.vitals().beginSurgery();
		savedSoldier.vitals().unregainableBreath() = 334;
		savedSoldier.vitals().criticalStatDamage()[DAMAGED_STAT_WISDOM] = 6;
		savedSoldier.vitals().criticalStatDamage()[DAMAGED_STAT_MEDICAL] = 9;
		savedSoldier.vitals().nextBleedAt() = 13.5f;
		savedSoldier.vitals().regenerationCounter() = -3;
		savedSoldier.vitals().regenerationBoostersUsedToday() = 4;
		savedSoldier.vitals().lastBleedGruntAt() = 12349;
		savedSoldier.service().activity() = 3;
		savedSoldier.service().addProvider();
		savedSoldier.service().addProvider();
		savedSoldier.service().addProvider();
		savedSoldier.service().beginProvidingTo( SoldierID{ 15 } );
		savedSoldier.service().assignAutoBandagingMedic( SoldierID{ 16 } );
		savedSoldier.dialogue().quoteRecord() = 15;
		savedSoldier.dialogue().quoteActionId() = QUOTE_ACTION_ID_TURNTOWARDSPLAYER;
		savedSoldier.dialogue().battleSoundSet() = 6;
		savedSoldier.dialogue().saidFlags() = SOLDIER_QUOTE_SAID_PERSONALITY;
		savedSoldier.dialogue().vocalVolume() = 85;
		savedSoldier.dialogue().recordBattleSound(BATTLE_SOUND_OK1, 12352);
		savedSoldier.dialogue().startHeardNoiseCooldown(3);
		savedSoldier.dialogue().saidExtendedFlags() = SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL;
		savedSoldier.dialogue().activeBattleSound() = 79;
		savedSoldier.dialogue().currentCivilianQuote() = -4;
		savedSoldier.dialogue().civilianQuoteDelta() = 3;
		savedSoldier.dialogue().recordSpokeAt(12353);
		savedSoldier.dialogue().corpseQuoteTolerance() = 4;
		savedSoldier.audio().recordFootstepVariant(5);
		savedSoldier.audio().recordDoorOpeningNoise(20);
		savedSoldier.audio().startBurstSound(601);
		savedSoldier.audio().startPositionSound(602);
		savedSoldier.audio().startTurningSound(603);
		std::strcpy(savedSoldier.renderState().headPalette(), "WHITEHEAD");
		std::strcpy(savedSoldier.renderState().pantsPalette(), "BLACKPANTS");
		std::strcpy(savedSoldier.renderState().vestPalette(), "BLACKSHIRT");
		std::strcpy(savedSoldier.renderState().skinPalette(), "DARKSKIN");
		std::strcpy(savedSoldier.renderState().miscPalette(), "SAVEMISC");
		savedSoldier.renderState().beginFade(2, 20, 2500);
		savedSoldier.renderState().forceRenderColor() = TRUE;
		savedSoldier.renderState().forceNoPaletteCycle() = TRUE;
		savedSoldier.renderState().enableForceShade();
		savedSoldier.renderState().showMuzzleFlash();
		savedSoldier.renderState().setUnblitRect(41, 42, 43, 44);
		savedSoldier.renderState().lightSprite() = 801;
		savedSoldier.renderState().startMuzzleFlashSprite(802);
		savedSoldier.renderState().muzzleFlashFrame() = 5;
		savedSoldier.renderState().setBoundingBox(71, 72, -9, -10);
		savedSoldier.uiPresentation().portraitFlashFrame() = -5;
		savedSoldier.uiPresentation().startLocator(8);
		savedSoldier.uiPresentation().locatorFrame() = 6;
		savedSoldier.uiPresentation().setLocatorOffset(14, -15);
		savedSoldier.uiPresentation().interfaceLevel() = 1;
		savedSoldier.uiPresentation().closePanelFrame() = 4;
		savedSoldier.uiPresentation().deadPanelFrame() = 5;
		savedSoldier.uiPresentation().openPanelFrame() = -6;
		savedSoldier.uiPresentation().setPanelFacePosition(110, 111);
		savedSoldier.uiPresentation().setPlannedTarget(600, 601, 16);
		savedSoldier.uiPresentation().lastEnemyCycled() = SoldierID{ 17 };
		savedSoldier.replication().movementStartedAt() = 26001;
		savedSoldier.replication().optimumMovementTime() = 26002;
		savedSoldier.replication().recordUpdate(26003);
		savedSoldier.replication().updateSequence() = 604;
		savedSoldier.replication().updateType() = 9;
		savedSoldier.replication().scheduleStop(26004);
		savedSoldier.movementMetrics().carriedWeightAtTurnStart() = 155;
		savedSoldier.movementMetrics().tilesMoved() = 9;
		savedSoldier.movementMetrics().realtimeBreathTiles() = 6;
		savedSoldier.movementMetrics().lastRealtimeMovementAnimation() = RUNNING;
		savedSoldier.aiPlanning().flankCount() = 5;
		savedSoldier.aiPlanning().flankAnchorGrid() = 26005;
		savedSoldier.aiPlanning().raiseSniperPosture();
		savedSoldier.aiPlanning().flankOriginDirection() = 7;
		savedSoldier.aiPlanning().planIndex() = 8;
		savedSoldier.skillState().beginCheck(-8, 1500);
		savedSoldier.skillState().recordCheckAttempt();
		savedSoldier.skillState().recordCheckAttempt();
		savedSoldier.skillState().selectedAiSkill() = SKILLS_RADIO_JAM;
		savedSoldier.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY) = 21;
		savedSoldier.skillState().counter(SOLDIER_COUNTER_SPOTTER) = 22;
		savedSoldier.skillState().counter(SOLDIER_COUNTER_ROLE_OBSERVED) = 23;
		savedSoldier.skillState().counter(SOLDIER_COUNTER_RETREAT) = 24;
		savedSoldier.skillState().counter(SOLDIER_COUNTER_MAX - 1) = 219;
		savedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_SECONDS) = 12354;
		savedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_APS) = 25;
		savedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_CRYO) = 26;
		savedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) = 27;
		savedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_DRUGUSER_COMBAT) = 28;
		savedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_ROBOT_XRAY) = 29;
		savedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_MAX - 1) = 229;
		savedSoldier.skillState().focusOn(1510);
		savedSoldier.condition().extraStrength() = 16;
		savedSoldier.condition().extraDexterity() = -17;
		savedSoldier.condition().extraAgility() = 18;
		savedSoldier.condition().extraWisdom() = -19;
		savedSoldier.condition().extraExperienceLevel() = 3;
		savedSoldier.condition().foodLevel() = 52000;
		savedSoldier.condition().drinkLevel() = -42000;
		savedSoldier.condition().starvationHealthDamage() = 9;
		savedSoldier.condition().starvationStrengthDamage() = 10;
		savedSoldier.condition().diseasePoints(0) = 120;
		savedSoldier.condition().diseaseFlags(0) = 0x05;
		savedSoldier.condition().diseasePoints(7) = -230;
		savedSoldier.condition().diseaseFlags(7) = 0x40;
		savedSoldier.condition().diseasePoints(NUM_DISEASES - 1) = 340;
		savedSoldier.condition().diseaseFlags(NUM_DISEASES - 1) = 0x80;
		savedSoldier.condition().addDisability(5);
		savedSoldier.condition().addDisability(SoldierConditionComponent::DisabilityBitCount);
		savedSoldier.longAction().begin(MTA_REMOVE_FORTIFY, 1520, 34);
		savedSoldier.interaction().nonNpcTraderId() = 11;
		savedSoldier.interaction().draggedPerson() = SoldierID{ 29 };
		savedSoldier.interaction().draggedCorpse() = 30;
		savedSoldier.interaction().chatPartner() = SoldierID{ 31 };
		savedSoldier.interaction().draggedStructureGrid() = 1521;
		savedSoldier.pendingAction().begin(MERC_APPLYITEM);
		savedSoldier.pendingAction().animationCount() = 12;
		savedSoldier.pendingAction().primaryData() = 1522;
		savedSoldier.pendingAction().secondaryData() = -1523;
		savedSoldier.pendingAction().tertiaryData() = -5;
		savedSoldier.pendingAction().doorHandleCode() = 6;
		savedSoldier.pendingAction().quaternaryData() = 1524;
		savedSoldier.pendingAction().nextSpecialData() = -1525;
		savedSoldier.pendingAction().interruptionMarker() = 7;
		savedSoldier.pendingAction().inventorySlot() = -8;
		savedSoldier.actionPoints().beginTurn(76);
		savedSoldier.actionPoints().current() = 41;
		savedSoldier.collapseState().collapse();
		savedSoldier.collapseState().markBreathCollapse();
		savedSoldier.collapseState().turns() = 4;
		savedSoldier.collapseState().sleepDrugCounter() = 8;
		savedSoldier.collapseState().markFatigueCollapse();
		savedSoldier.perception().rememberMovementFrom(3);
		savedSoldier.perception().rememberMovementFrom(7);
		savedSoldier.perception().viewRange() = 17;
		savedSoldier.perception().setBlindness(6);
		savedSoldier.perception().heardNoiseLevel() = 1;
		savedSoldier.perception().activateXrayAt(12347);
		savedSoldier.perception().setDeafness(5);
		savedSoldier.awareness().visibility() = -2;
		savedSoldier.awareness().lastRenderedVisibility() = -1;
		savedSoldier.awareness().newOpponentCount() = 4;
		savedSoldier.awareness().tilesSinceForget() = 203;
		savedSoldier.camouflage().jungleApplied() = 11;
		savedSoldier.camouflage().jungleWorn() = 12;
		savedSoldier.camouflage().urbanApplied() = -13;
		savedSoldier.camouflage().urbanWorn() = 14;
		savedSoldier.camouflage().desertApplied() = 15;
		savedSoldier.camouflage().desertWorn() = -16;
		savedSoldier.camouflage().snowApplied() = 17;
		savedSoldier.camouflage().snowWorn() = 18;
		savedSoldier.employment().endTime() = 22000;
		savedSoldier.employment().startTime() = 27;
		savedSoldier.employment().totalLength() = 28;
		savedSoldier.employment().mercenaryType() = MERC_TYPE__NPC;
		savedSoldier.employment().medicalDeposit() = 800;
		savedSoldier.employment().lifeInsurance() = 3;
		savedSoldier.employment().insuranceStartDay() = 6;
		savedSoldier.employment().insuranceLengthDays() = 10;
		savedSoldier.employment().lastContractUpdateTime() = 22001;
		savedSoldier.employment().lastContractType() = CONTRACT_EXTEND_1_DAY;
		savedSoldier.employment().justFired() = 1;
		savedSoldier.employment().renewalQuoteCode() = SOLDIER_CONTRACT_RENEW_QUOTE_89_USED;
		savedSoldier.employment().timeCanSignElsewhere() = 23000;
		savedSoldier.employment().hospitalPriceModifier() = -4;
		savedSoldier.employment().insuranceStartTime() = 21000;
		savedSoldier.assignment().current() = TRAIN_SELF;
		savedSoldier.assignment().previous() = TRAIN_TEAMMATE;
		savedSoldier.assignment().trainingStat() = STRENGTH;
		savedSoldier.assignment().lastChangeMinute() = 21900;
		savedSoldier.assignment().desiredSquad() = 5;
		savedSoldier.assignment().mergeTraversalAllowance() = 6;
		savedSoldier.assignment().hours() = 8;
		savedSoldier.assignment().repairVehicleId() = 3;
		savedSoldier.assignment().facilityType() = 7;
		savedSoldier.assignment().itemMoveSectorId() = 48;
		savedSoldier.assignment().miniEventHoursRemaining() = 14;
		savedSoldier.deployment().insertionDirection() = -4;
		savedSoldier.deployment().groupId() = 44;
		savedSoldier.deployment().insertionGrid() = 2400;
		savedSoldier.deployment().setStrategicInsertion(INSERTION_CODE_GRIDNO, 2401);
		savedSoldier.deployment().setSector(12, 7, 3);
		savedSoldier.deployment().vehicleId() = 10;
		savedSoldier.deployment().offWorldGrid() = 2402;
		savedSoldier.deployment().setTraversalOrigin(33, 2403);
		savedSoldier.deployment().useExitGridForReentryDirection() = 1;
		savedSoldier.deployment().scheduleArrival(16000, 9);
		savedSoldier.deployment().beginArrivalGetup();
		savedSoldier.deployment().arrivalGetupCounter() = 17000;
		savedSoldier.schedule().id() = 43;
		savedSoldier.schedule().progress() = 4;
		savedSoldier.schedule().beginDoorContinuation(2404);
		savedSoldier.schedule().completeDoorAnimation();
		savedSoldier.position().setWorldCoordinates(242.5f, 668.75f);
		savedSoldier.position().worldXInt() = 241;
		savedSoldier.position().worldYInt() = 667;
		savedSoldier.position().recordTurnStart(230, 650);
		savedSoldier.position().initialGrid() = 1400;
		savedSoldier.position().gridNo() = 1427;
		savedSoldier.position().level() = 1;
		savedSoldier.position().direction() = 5;
		savedSoldier.position().heightAdjustment() = 19;
		savedSoldier.position().desiredHeight() = 27;
		savedSoldier.position().temporaryGrid() = 1428;
		savedSoldier.position().roomNo() = 11;
		savedSoldier.position().terrainType() = HIGH_GRASS;
		savedSoldier.position().previousTerrainType() = DIRT_ROAD;
		savedSoldier.movementHistory().recordDeparture(1426);
		savedSoldier.movementHistory().recentLocations()[0] = 1410;
		savedSoldier.movementHistory().recentLocations()[1] = 1411;
		savedSoldier.pathing().desiredDirection() = 4;
		savedSoldier.pathing().destinationX() = 321;
		savedSoldier.pathing().destinationY() = 654;
		savedSoldier.pathing().destinationGrid() = 1430;
		savedSoldier.pathing().finalDestinationGrid() = 1450;
		savedSoldier.pathing().stopped() = 1;
		savedSoldier.pathing().needsLook() = 1;
		savedSoldier.pathing().path()[0] = 6;
		savedSoldier.pathing().path()[1] = 7;
		savedSoldier.pathing().pathSize() = 2;
		savedSoldier.pathing().pathIndex() = 1;
		savedSoldier.pathing().blackListGrid() = 1444;
		savedSoldier.pathing().stored() = 1;
		savedSoldier.movement().mode() = WALKING_WEAPON_RDY;
		savedSoldier.movement().setStealth(true);
		savedSoldier.movement().setReverse(true);
		savedSoldier.movement().setHighResolutionFacing(15, 17);
		savedSoldier.movement().animationDirection() = 6;
		savedSoldier.movement().gridUpdatePolicy() = LOCKED_NO_NEWGRIDNO;
		savedSoldier.movement().waitForGrid(1451, 12);
		savedSoldier.movement().reservedGrid() = 1452;
		savedSoldier.movement().blockInDirection(6);
		savedSoldier.movement().absoluteDestination() = 1460;
		savedSoldier.movement().setContinuedPath(1470);
		savedSoldier.movement().delayedFlags() = 3;
		savedSoldier.movement().stopReason() = 4;
		savedSoldier.movement().overrideMoveSpeedWith(SoldierID{ 8 });
		savedSoldier.movement().beginTurn();
		savedSoldier.movement().rememberWaterState(true);
		savedSoldier.movement().setUiMovementFast(TRUE);
		savedSoldier.movement().setOutOfActionPoints(true);
		savedSoldier.movement().pauseMovement();
		savedSoldier.movement().startMovementClock();
		savedSoldier.movement().setNetworkDelayed(true);
		savedSoldier.movement().syncPresentationMotion(true);
		savedSoldier.movement().pastXDestination() = -3;
		savedSoldier.movement().pastYDestination() = 4;
		savedSoldier.interruptSnapshot().captureMoved(1);
		savedSoldier.targeting().selectLocation(1480, 1, 4);
		savedSoldier.targeting().lastGridNo() = 1479;
		savedSoldier.targeting().selectSoldier(SoldierID{ 9 });
		savedSoldier.attackSelection().selectWeapon(SECONDHANDPOS, 444);
		savedSoldier.attackSelection().weaponMode() = WM_ATTACHED_GL_AUTO;
		savedSoldier.attackSelection().scopeMode() = USE_SCOPE_3;
		savedSoldier.attackSelection().shotLocation() = AIM_SHOT_HEAD;
		savedSoldier.attackSelection().meleeLocation() = AIM_SHOT_LEGS;
		savedSoldier.meleeApproach().recordPath(WALKING_WEAPON_RDY, 25, 6);
		savedSoldier.meleeApproach().rememberGrid(1490);
		savedSoldier.fireControl().selectAutofire(9);
		savedSoldier.fireControl().bulletsLeft() = 4;
		savedSoldier.fireControl().spreadIndex() = TRUE;
		savedSoldier.fireControl().autofireLastStep() = TRUE;
		for (UINT8 i = 0; i < SoldierFireControlComponent::SpreadTargetCapacity; ++i)
			savedSoldier.fireControl().spreadLocations()[i] = 1501 + i;
		savedSoldier.fireControl().previousMuzzleOffsetX()[0] = 1.5f;
		savedSoldier.fireControl().previousMuzzleOffsetX()[1] = 2.75f;
		savedSoldier.fireControl().previousMuzzleOffsetY()[0] = -1.25f;
		savedSoldier.fireControl().previousMuzzleOffsetY()[1] = -2.5f;
		savedSoldier.fireControl().previousCounterForceX()[0] = 0.25f;
		savedSoldier.fireControl().previousCounterForceX()[1] = 0.75f;
		savedSoldier.fireControl().previousCounterForceY()[0] = -0.5f;
		savedSoldier.fireControl().previousCounterForceY()[1] = -1.0f;
		savedSoldier.fireControl().initialMuzzleOffsetX() = 3.5f;
		savedSoldier.fireControl().initialMuzzleOffsetY() = -4.5f;
		savedSoldier.fireControl().barrelCounter() = 3;
		savedSoldier.fireControl().beginSpreadDrag(1510);
		savedSoldier.fireControl().updateSpreadDrag(1512);
		savedSoldier.combatResult().recordHit(SoldierID{ 12 }, AIM_SHOT_HEAD);
		savedSoldier.combatResult().previousAttacker() = SoldierID{ 11 };
		savedSoldier.combatResult().earlierAttacker() = SoldierID{ 10 };
		savedSoldier.combatResult().lastDamageReason() = 8;
		savedSoldier.combatResult().hitsThisTurn() = 4;
		savedSoldier.combatResult().pelletsHitBy() = 5;
		savedSoldier.combatResult().accumulatedDamage() = 29;
		savedSoldier.combatContribution().militiaKills() = 7;
		savedSoldier.combatContribution().militiaAssists() = 8;
		savedSoldier.combatContribution().damageByTeam()[0] = 81;
		savedSoldier.combatContribution().damageByTeam()[NUM_ASSIST_SLOTS - 1] = 82;
		savedSoldier.damageDisplay().activateAt(17, -12);
		savedSoldier.damageDisplay().displayFlag() = 2;
		savedSoldier.damageDisplay().counter() = 3;
		savedSoldier.damageDisplay().direction() = -1;
		savedSoldier.suppression().underFire() = 2;
		savedSoldier.suppression().shock() = 10;
		savedSoldier.suppression().addPoints(6);
		savedSoldier.suppression().addActionPointLoss(13);
		savedSoldier.suppression().suppressor() = SoldierID{ 14 };
		savedSoldier.suppression().markCloseCall();
		savedSoldier.animationIntent().requestHeight( ANIM_PRONE );
		savedSoldier.animationIntent().queueFacingAnimation( SWATTING, 7 );
		savedSoldier.animationIntent().queueStance( ANIM_CROUCH );
		savedSoldier.animationIntent().queueSecondaryAnimation( RUNNING );
		savedSoldier.animationIntent().markTurningFromUi();
		savedSoldier.animationIntent().requestStopAtNextTile();
		savedSoldier.animationIntent().continueAfterStance( 2 );
		savedSoldier.animationPlayback().state() = RUNNING;
		savedSoldier.animationPlayback().code() = 27;
		savedSoldier.animationPlayback().frame() = 45;
		savedSoldier.animationPlayback().delay() = -13;
		savedSoldier.animationPlayback().previousState() = STANDING;
		savedSoldier.animationPlayback().previousCode() = 9;
		savedSoldier.animationPlayback().surface() = 123;
		savedSoldier.animationPlayback().zLevel() = 456;
		savedSoldier.animationPlayback().subFlags() = 0xA5A55A5A;
		savedSoldier.animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE;
		savedSoldier.animationActivity().readyCostWaived() = TRUE;
		savedSoldier.animationActivity().postHitStance() = GO_TO_AIM_AFTER_HIT;
		savedSoldier.animationActivity().pause();
		savedSoldier.animationActivity().holdAttackerUntilDone() = TRUE;
		savedSoldier.animationActivity().turningToShoot() = TRUE;
		savedSoldier.animationActivity().turningToFall() = TRUE;
		savedSoldier.animationActivity().turningUntilDone() = TRUE;
		savedSoldier.animationActivity().beginHit();
		savedSoldier.animationActivity().advanceHit();
		savedSoldier.animationActivity().setInterruptibility( TRUE, TRUE );
		savedSoldier.animationActivity().turningCostWaived() = TRUE;
		savedSoldier.animationActivity().suppressionStanceChange() = TRUE;
		savedSoldier.animationActivity().stanceCostWaived() = TRUE;
		savedSoldier.animationActivity().beginFall( 4 );
		savedSoldier.animationActivity().fallClockwise() = TRUE;
		savedSoldier.animationActivity().turningIncrement() = -1;
		savedSoldier.animationActivity().forecastTraversalAt(1520);
		savedSoldier.animationActivity().setRenderZOverride(888);
		savedSoldier.animationActivity().randomActionCheckCounter() = 81;
		savedSoldier.animationActivity().lastRandomAnimation() = 124;

		HWFILE output = FileOpen( const_cast<char*>( path.c_str() ),
		                          FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS );
		const bool saved = output && savedSoldier.Save( output );
		if ( output ) FileClose( output );

		SOLDIERTYPE loadedSoldier;
		HWFILE input = FileOpen( const_cast<char*>( path.c_str() ),
		                         FILE_ACCESS_READ | FILE_OPEN_EXISTING );
		const bool loaded = input && loadedSoldier.Load( input );
		if ( input ) FileClose( input );
		FileDelete( const_cast<char*>( path.c_str() ) );
		guiCurrentSaveGameVersion = previousSaveVersion;

		CHECK( saved && loaded &&
		       loadedSoldier.stats.bExpLevel == 6 &&
		       loadedSoldier.stats.bStrength == 77 &&
		       loadedSoldier.vitals().health() == 71 &&
		       loadedSoldier.vitals().maximumHealth() == 89 &&
		       loadedSoldier.vitals().breath() == 62 &&
		       loadedSoldier.vitals().maximumBreath() == 94 &&
		       loadedSoldier.vitals().bleeding() == 11 &&
		       loadedSoldier.vitals().previousHealth() == 69 &&
		       loadedSoldier.vitals().fractionalHealth() == 43 &&
		       loadedSoldier.vitals().breathReduction() == 445 &&
		       loadedSoldier.vitals().healableInjury() == 1800 &&
		       loadedSoldier.vitals().isUndergoingSurgery() &&
		       loadedSoldier.vitals().unregainableBreath() == 334 &&
		       loadedSoldier.vitals().criticalStatDamage()[DAMAGED_STAT_WISDOM] == 6 &&
		       loadedSoldier.vitals().criticalStatDamage()[DAMAGED_STAT_MEDICAL] == 9 &&
		       loadedSoldier.vitals().nextBleedAt() == 13.5f &&
		       loadedSoldier.vitals().regenerationCounter() == -3 &&
		       loadedSoldier.vitals().regenerationBoostersUsedToday() == 4 &&
		       loadedSoldier.vitals().lastBleedGruntAt() == 12349,
		       "soldier save/load round-trips vitals state at established schema positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.service().activity() == 3 &&
		       loadedSoldier.service().providerCount() == 3 &&
		       loadedSoldier.service().partner() == SoldierID{ 15 } &&
		       loadedSoldier.service().autoBandagingMedic() == SoldierID{ 16 },
		       "soldier save/load round-trips tactical service state at established schema positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.dialogue().quoteRecord() == 15 &&
		       loadedSoldier.dialogue().quoteActionId() == QUOTE_ACTION_ID_TURNTOWARDSPLAYER &&
		       loadedSoldier.dialogue().battleSoundSet() == 6 &&
		       loadedSoldier.dialogue().saidFlags() == SOLDIER_QUOTE_SAID_PERSONALITY &&
		       loadedSoldier.dialogue().vocalVolume() == 85 &&
		       loadedSoldier.dialogue().repeatedBattleSoundAt() == 12352 &&
		       loadedSoldier.dialogue().previousBattleSound() == BATTLE_SOUND_OK1 &&
		       loadedSoldier.dialogue().heardNoiseCooldownTurns() == 3 &&
		       loadedSoldier.dialogue().saidExtendedFlags() == SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL &&
		       loadedSoldier.dialogue().activeBattleSound() == 79 &&
		       loadedSoldier.dialogue().currentCivilianQuote() == -4 &&
		       loadedSoldier.dialogue().civilianQuoteDelta() == 3 &&
		       loadedSoldier.dialogue().lastSpokeAt() == 12353 &&
		       loadedSoldier.dialogue().corpseQuoteTolerance() == 4,
		       "soldier save/load round-trips dialogue state at every established schema position" );
		CHECK( saved && loaded &&
		       loadedSoldier.audio().lastFootstepVariant() == 5 &&
		       loadedSoldier.audio().doorOpeningNoise() == 20 &&
		       loadedSoldier.audio().burstSoundId() == 601 &&
		       loadedSoldier.audio().positionSoundId() == 602 &&
		       loadedSoldier.audio().turningSoundId() == 603,
		       "soldier save/load round-trips non-dialogue audio state at every established schema position" );
		CHECK( saved && loaded &&
		       loadedSoldier.uiPresentation().portraitFlashFrame() == -5 &&
		       loadedSoldier.uiPresentation().locatorFrame() == 6 &&
		       loadedSoldier.uiPresentation().locatorOffsetX() == 14 &&
		       loadedSoldier.uiPresentation().locatorOffsetY() == -15 &&
		       loadedSoldier.uiPresentation().interfaceLevel() == 1 &&
		       loadedSoldier.uiPresentation().closePanelFrame() == 4 &&
		       loadedSoldier.uiPresentation().deadPanelFrame() == 5 &&
		       loadedSoldier.uiPresentation().openPanelFrame() == -6 &&
		       loadedSoldier.uiPresentation().panelFaceX() == 110 &&
		       loadedSoldier.uiPresentation().panelFaceY() == 111 &&
		       loadedSoldier.uiPresentation().plannedActionPointCost() == 16 &&
		       loadedSoldier.uiPresentation().plannedTargetX() == 600 &&
		       loadedSoldier.uiPresentation().plannedTargetY() == 601 &&
		       loadedSoldier.uiPresentation().lastEnemyCycled() == SoldierID{ 17 } &&
		       loadedSoldier.uiPresentation().locateCycles() == 8,
		       "soldier save/load round-trips UI presentation at every established schema position" );
		CHECK( saved && loaded &&
		       loadedSoldier.replication().movementStartedAt() == 26001 &&
		       loadedSoldier.replication().optimumMovementTime() == 26002 &&
		       loadedSoldier.replication().lastUpdateAt() == 26003 &&
		       loadedSoldier.replication().updateSequence() == 604 &&
		       loadedSoldier.replication().updateType() == 9 &&
		       loadedSoldier.replication().scheduledStopGrid() == 26004 &&
		       loadedSoldier.replication().checksum() == savedSoldier.GetChecksum() &&
		       savedSoldier.replication().checksum() == savedSoldier.GetChecksum(),
		       "soldier save/load round-trips replication state and records the current integrity checksum" );
		CHECK( saved && loaded &&
		       loadedSoldier.movementMetrics().carriedWeightAtTurnStart() == 155 &&
		       loadedSoldier.movementMetrics().tilesMoved() == 9 &&
		       loadedSoldier.movementMetrics().realtimeBreathTiles() == 6 &&
		       loadedSoldier.movementMetrics().lastRealtimeMovementAnimation() == RUNNING,
		       "soldier save/load round-trips movement telemetry at every established schema position" );
		CHECK( saved && loaded &&
		       loadedSoldier.aiPlanning().flankCount() == 5 &&
		       loadedSoldier.aiPlanning().flankAnchorGrid() == 26005 &&
		       loadedSoldier.aiPlanning().sniperPostureActive() &&
		       loadedSoldier.aiPlanning().flankOriginDirection() == 7 &&
		       loadedSoldier.aiPlanning().planIndex() == 8,
		       "soldier save/load round-trips AI planning at every established schema position" );
		CHECK( saved && loaded &&
		       loadedSoldier.skillState().lastCheckReason() == -8 &&
		       loadedSoldier.skillState().checkAttempts() == 3 &&
		       loadedSoldier.skillState().checkGrid() == 1500 &&
		       loadedSoldier.skillState().selectedAiSkill() == SKILLS_RADIO_JAM &&
		       loadedSoldier.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY) == 21 &&
		       loadedSoldier.skillState().counter(SOLDIER_COUNTER_SPOTTER) == 22 &&
		       loadedSoldier.skillState().counter(SOLDIER_COUNTER_ROLE_OBSERVED) == 23 &&
		       loadedSoldier.skillState().counter(SOLDIER_COUNTER_RETREAT) == 24 &&
		       loadedSoldier.skillState().counter(SOLDIER_COUNTER_MAX - 1) == 219 &&
		       loadedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_SECONDS) == 12354 &&
		       loadedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_APS) == 25 &&
		       loadedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_CRYO) == 26 &&
		       loadedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) == 27 &&
		       loadedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_DRUGUSER_COMBAT) == 28 &&
		       loadedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_ROBOT_XRAY) == 29 &&
		       loadedSoldier.skillState().cooldown(SOLDIER_COOLDOWN_MAX - 1) == 229 &&
		       loadedSoldier.skillState().focusGrid() == 1510,
		       "soldier save/load round-trips skill state at every established schema position" );
		CHECK( saved && loaded &&
		       loadedSoldier.condition().extraStrength() == 16 &&
		       loadedSoldier.condition().extraDexterity() == -17 &&
		       loadedSoldier.condition().extraAgility() == 18 &&
		       loadedSoldier.condition().extraWisdom() == -19 &&
		       loadedSoldier.condition().extraExperienceLevel() == 3 &&
		       loadedSoldier.condition().foodLevel() == 52000 &&
		       loadedSoldier.condition().drinkLevel() == -42000 &&
		       loadedSoldier.condition().starvationHealthDamage() == 9 &&
		       loadedSoldier.condition().starvationStrengthDamage() == 10 &&
		       loadedSoldier.condition().diseasePoints(0) == 120 &&
		       loadedSoldier.condition().diseaseFlags(0) == 0x05 &&
		       loadedSoldier.condition().diseasePoints(7) == -230 &&
		       loadedSoldier.condition().diseaseFlags(7) == 0x40 &&
		       loadedSoldier.condition().diseasePoints(NUM_DISEASES - 1) == 340 &&
		       loadedSoldier.condition().diseaseFlags(NUM_DISEASES - 1) == 0x80 &&
		       loadedSoldier.condition().hasDisability(5) &&
		       loadedSoldier.condition().hasDisability(SoldierConditionComponent::DisabilityBitCount),
		       "soldier save/load round-trips condition state at every established schema position and fixed-capacity edge" );
		CHECK( saved && loaded &&
		       loadedSoldier.longAction().active() &&
		       loadedSoldier.longAction().action() == MTA_REMOVE_FORTIFY &&
		       loadedSoldier.longAction().contextGrid() == 1520 &&
		       loadedSoldier.longAction().remainingActionPoints() == 34,
		       "soldier save/load round-trips long-action state at every established schema position" );
		CHECK( saved && loaded &&
		       loadedSoldier.interaction().nonNpcTraderId() == 11 &&
		       loadedSoldier.interaction().draggedPerson() == SoldierID{ 29 } &&
		       loadedSoldier.interaction().draggedCorpse() == 30 &&
		       loadedSoldier.interaction().chatPartner() == SoldierID{ 31 } &&
		       loadedSoldier.interaction().draggedStructureGrid() == 1521,
		       "soldier save/load round-trips every interaction field at its established schema position" );
		CHECK( saved && loaded &&
		       loadedSoldier.pendingAction().action() == MERC_APPLYITEM &&
		       loadedSoldier.pendingAction().animationCount() == 12 &&
		       loadedSoldier.pendingAction().primaryData() == 1522 &&
		       loadedSoldier.pendingAction().secondaryData() == -1523 &&
		       loadedSoldier.pendingAction().tertiaryData() == -5 &&
		       loadedSoldier.pendingAction().doorHandleCode() == 6 &&
		       loadedSoldier.pendingAction().quaternaryData() == 1524 &&
		       loadedSoldier.pendingAction().nextSpecialData() == -1525 &&
		       loadedSoldier.pendingAction().interruptionMarker() == 7 &&
		       loadedSoldier.pendingAction().inventorySlot() == -8,
		       "soldier save/load round-trips pending-action state at every established schema position" );
		CHECK( saved && loaded &&
		       loadedSoldier.actionPoints().current() == 41 &&
		       loadedSoldier.actionPoints().initial() == 76,
		       "soldier save/load round-trips action-point state at established schema positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.collapseState().collapsed() &&
		       loadedSoldier.collapseState().breathCollapsed() &&
		       loadedSoldier.collapseState().turns() == 4 &&
		       loadedSoldier.collapseState().sleepDrugCounter() == 8 &&
		       loadedSoldier.collapseState().fatigueCollapsed(),
		       "soldier save/load round-trips collapse state at established flags and POD positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.perception().hasHeardMovementFrom(3) &&
		       loadedSoldier.perception().hasHeardMovementFrom(7) &&
		       loadedSoldier.perception().viewRange() == 17 &&
		       loadedSoldier.perception().blindnessTurns() == 6 &&
		       loadedSoldier.perception().heardNoiseLevel() == 1 &&
		       loadedSoldier.perception().xrayActivatedAt() == 12347 &&
		       loadedSoldier.perception().deafnessTurns() == 5,
		       "soldier save/load round-trips perception state at established POD positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.awareness().visibility() == -2 &&
		       loadedSoldier.awareness().lastRenderedVisibility() == -1 &&
		       loadedSoldier.awareness().newOpponentCount() == 4 &&
		       loadedSoldier.awareness().tilesSinceForget() == 203,
		       "soldier save/load round-trips awareness state at established POD positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.camouflage().jungleApplied() == 11 &&
		       loadedSoldier.camouflage().jungleWorn() == 12 &&
		       loadedSoldier.camouflage().urbanApplied() == -13 &&
		       loadedSoldier.camouflage().urbanWorn() == 14 &&
		       loadedSoldier.camouflage().desertApplied() == 15 &&
		       loadedSoldier.camouflage().desertWorn() == -16 &&
		       loadedSoldier.camouflage().snowApplied() == 17 &&
		       loadedSoldier.camouflage().snowWorn() == 18,
		       "soldier save/load round-trips camouflage state at established POD positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.employment().endTime() == 22000 &&
		       loadedSoldier.employment().startTime() == 27 &&
		       loadedSoldier.employment().totalLength() == 28 &&
		       loadedSoldier.employment().mercenaryType() == MERC_TYPE__NPC &&
		       loadedSoldier.employment().medicalDeposit() == 800 &&
		       loadedSoldier.employment().lifeInsurance() == 3 &&
		       loadedSoldier.employment().insuranceStartDay() == 6 &&
		       loadedSoldier.employment().insuranceLengthDays() == 10 &&
		       loadedSoldier.employment().lastContractUpdateTime() == 22001 &&
		       loadedSoldier.employment().lastContractType() == CONTRACT_EXTEND_1_DAY &&
		       loadedSoldier.employment().justFired() == 1 &&
		       loadedSoldier.employment().renewalQuoteCode() == SOLDIER_CONTRACT_RENEW_QUOTE_89_USED &&
		       loadedSoldier.employment().timeCanSignElsewhere() == 23000 &&
		       loadedSoldier.employment().hospitalPriceModifier() == -4 &&
		       loadedSoldier.employment().insuranceStartTime() == 21000,
		       "soldier save/load round-trips employment state at every established POD position" );
		CHECK( saved && loaded &&
		       loadedSoldier.assignment().current() == TRAIN_SELF &&
		       loadedSoldier.assignment().previous() == TRAIN_TEAMMATE &&
		       loadedSoldier.assignment().trainingStat() == STRENGTH &&
		       loadedSoldier.assignment().lastChangeMinute() == 21900 &&
		       loadedSoldier.assignment().desiredSquad() == 5 &&
		       loadedSoldier.assignment().mergeTraversalAllowance() == 6 &&
		       loadedSoldier.assignment().hours() == 8 &&
		       loadedSoldier.assignment().repairVehicleId() == 3 &&
		       loadedSoldier.assignment().facilityType() == 7 &&
		       loadedSoldier.assignment().itemMoveSectorId() == 48 &&
		       loadedSoldier.assignment().miniEventHoursRemaining() == 14,
		       "soldier save/load round-trips assignment state at every established POD position" );
		CHECK( saved && loaded &&
		       loadedSoldier.deployment().insertionDirection() == -4 &&
		       loadedSoldier.deployment().groupId() == 44 &&
		       loadedSoldier.deployment().insertionGrid() == 2400 &&
		       loadedSoldier.deployment().strategicInsertionCode() == INSERTION_CODE_GRIDNO &&
		       loadedSoldier.deployment().strategicInsertionData() == 2401 &&
		       loadedSoldier.deployment().isInSector(12, 7, 3) &&
		       loadedSoldier.deployment().vehicleId() == 10 &&
		       loadedSoldier.deployment().offWorldGrid() == 2402 &&
		       loadedSoldier.deployment().previousSectorId() == 33 &&
		       loadedSoldier.deployment().useExitGridForReentryDirection() == 1 &&
		       loadedSoldier.deployment().preTraversalGrid() == 2403 &&
		       loadedSoldier.deployment().leaveHistoryCode() == 9 &&
		       loadedSoldier.deployment().arrivalTime() == 16000 &&
		       loadedSoldier.deployment().arrivalGetupPending() &&
		       loadedSoldier.deployment().ignoreCollapseGetupCheck() &&
		       loadedSoldier.deployment().arrivalGetupCounter() == 17000,
		       "soldier save/load round-trips deployment state at every established POD position" );
		CHECK( saved && loaded &&
		       loadedSoldier.schedule().id() == 43 &&
		       loadedSoldier.schedule().progress() == 4 &&
		       loadedSoldier.schedule().doorAnimationComplete() &&
		       loadedSoldier.schedule().doorGrid() == 2404,
		       "soldier save/load round-trips schedule execution at every established POD position" );
		CHECK( saved && loaded &&
		       loadedSoldier.position().worldX() == 242.5f &&
		       loadedSoldier.position().worldY() == 668.75f &&
		       loadedSoldier.position().worldXInt() == 241 &&
		       loadedSoldier.position().worldYInt() == 667 &&
		       loadedSoldier.position().turnStartX() == 230 &&
		       loadedSoldier.position().turnStartY() == 650 &&
		       loadedSoldier.position().initialGrid() == 1400 &&
		       loadedSoldier.position().gridNo() == 1427 &&
		       loadedSoldier.position().level() == 1 &&
		       loadedSoldier.position().direction() == 5 &&
		       loadedSoldier.position().heightAdjustment() == 19 &&
		       loadedSoldier.position().desiredHeight() == 27 &&
		       loadedSoldier.position().temporaryGrid() == 1428 &&
		       loadedSoldier.position().roomNo() == 11 &&
		       loadedSoldier.position().terrainType() == HIGH_GRASS &&
		       loadedSoldier.position().previousTerrainType() == DIRT_ROAD &&
		       loadedSoldier.movementHistory().previousGrid() == 1426 &&
		       loadedSoldier.movementHistory().recentLocations()[0] == 1410 &&
		       loadedSoldier.movementHistory().recentLocations()[1] == 1411 &&
		       loadedSoldier.pathing().desiredDirection() == 4 &&
		       loadedSoldier.pathing().destinationX() == 321 &&
		       loadedSoldier.pathing().destinationY() == 654 &&
		       loadedSoldier.pathing().destinationGrid() == 1430 &&
		       loadedSoldier.pathing().finalDestinationGrid() == 1450 &&
		       loadedSoldier.pathing().stopped() == 1 &&
		       loadedSoldier.pathing().needsLook() == 1 &&
		       loadedSoldier.pathing().path()[0] == 6 &&
		       loadedSoldier.pathing().path()[1] == 7 &&
		       loadedSoldier.pathing().pathSize() == 2 &&
		       loadedSoldier.pathing().pathIndex() == 1 &&
		       loadedSoldier.pathing().blackListGrid() == 1444 &&
		       loadedSoldier.pathing().stored() == 1,
		       "soldier save/load round-trips complete position, movement-history, and pathing state through every established schema site" );
		CHECK( saved && loaded &&
		       loadedSoldier.movement().mode() == WALKING_WEAPON_RDY &&
		       loadedSoldier.movement().stealthy() &&
		       loadedSoldier.movement().reversing() &&
		       loadedSoldier.movement().highResolutionDirection() == 15 &&
		       loadedSoldier.movement().highResolutionDesiredDirection() == 17 &&
		       loadedSoldier.movement().animationDirection() == 6 &&
		       loadedSoldier.movement().gridUpdatePolicy() ==
		           LOCKED_NO_NEWGRIDNO &&
		       loadedSoldier.movement().delayCounter() == 12 &&
		       loadedSoldier.movement().turnActive() &&
		       loadedSoldier.movement().wasInWater() &&
		       loadedSoldier.movement().fastUiMovement() &&
		       loadedSoldier.movement().outOfActionPoints() &&
		       loadedSoldier.movement().movementPaused() &&
		       loadedSoldier.movement().recordingMovement() &&
		       loadedSoldier.movement().delayedByNetwork() &&
		       loadedSoldier.movement().wasMoving() &&
		       loadedSoldier.movement().pastXDestination() == -3 &&
		       loadedSoldier.movement().pastYDestination() == 4,
		       "soldier save/load round-trips component-owned movement intent, facing, and activity state" );
		CHECK( saved && loaded &&
		       loadedSoldier.movement().delayedCauseGrid() == 1451,
		       "soldier save/load round-trips the component-owned movement delay cause" );
		CHECK( saved && loaded &&
		       loadedSoldier.movement().reservedGrid() == 1452,
		       "soldier save/load round-trips the component-owned movement reservation" );
		CHECK( saved && loaded &&
		       loadedSoldier.movement().blockedByAnotherMerc() &&
		       loadedSoldier.movement().blockedDirection() == 6,
		       "soldier save/load round-trips component-owned movement contention state" );
		CHECK( saved && loaded &&
		       loadedSoldier.movement().absoluteDestination() == 1460 &&
		       loadedSoldier.movement().continuedPathValid() &&
		       loadedSoldier.movement().continuedPathGrid() == 1470,
		       "soldier save/load round-trips component-owned movement destination state" );
		CHECK( saved && loaded &&
		       loadedSoldier.movement().delayedFlags() == 3 &&
		       loadedSoldier.movement().stopReason() == 4,
		       "soldier save/load round-trips component-owned movement result state" );
		CHECK( saved && loaded &&
		       loadedSoldier.movement().usesMoveSpeedOverride() &&
		       loadedSoldier.movement().moveSpeedOverride() == SoldierID{ 8 },
		       "soldier save/load round-trips component-owned movement speed state" );
		CHECK( saved && loaded &&
		       loadedSoldier.interruptSnapshot().movedBeforeInterrupt() == 1,
		       "soldier save/load round-trips the interrupt scheduler snapshot" );
		CHECK( saved && loaded &&
		       loadedSoldier.targeting().gridNo() == 1480 &&
		       loadedSoldier.targeting().level() == 1 &&
		       loadedSoldier.targeting().cubeLevel() == 4 &&
		       loadedSoldier.targeting().lastGridNo() == 1479 &&
		       loadedSoldier.targeting().targetId() == SoldierID{ 9 },
		       "soldier save/load round-trips component-owned targeting state at established schema positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.attackSelection().hand() == SECONDHANDPOS &&
		       loadedSoldier.attackSelection().weapon() == 444 &&
		       loadedSoldier.attackSelection().weaponMode() == WM_ATTACHED_GL_AUTO &&
		       loadedSoldier.attackSelection().scopeMode() == USE_SCOPE_3 &&
		       loadedSoldier.attackSelection().shotLocation() == AIM_SHOT_HEAD &&
		       loadedSoldier.attackSelection().meleeLocation() == AIM_SHOT_LEGS,
		       "soldier save/load round-trips component-owned attack selection at established schema positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.meleeApproach().matches(1490, WALKING_WEAPON_RDY) &&
		       loadedSoldier.meleeApproach().cost() == 25 &&
		       loadedSoldier.meleeApproach().endDirection() == 6,
		       "soldier save/load round-trips the melee-approach cache at established schema positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.fireControl().burstCounter() == 1 &&
		       loadedSoldier.fireControl().autofireShots() == 9 &&
		       loadedSoldier.fireControl().bulletsLeft() == 4 &&
		       loadedSoldier.fireControl().spreadIndex() == TRUE &&
		       loadedSoldier.fireControl().autofireLastStep() &&
		       loadedSoldier.fireControl().spreadLocations()[0] == 1501 &&
		       loadedSoldier.fireControl().spreadLocations()[5] == 1506 &&
		       loadedSoldier.fireControl().previousMuzzleOffsetX()[1] == 2.75f &&
		       loadedSoldier.fireControl().previousMuzzleOffsetY()[1] == -2.5f &&
		       loadedSoldier.fireControl().previousCounterForceX()[1] == 0.75f &&
		       loadedSoldier.fireControl().previousCounterForceY()[1] == -1.0f &&
		       loadedSoldier.fireControl().initialMuzzleOffsetX() == 3.5f &&
		       loadedSoldier.fireControl().initialMuzzleOffsetY() == -4.5f &&
		       loadedSoldier.fireControl().barrelCounter() == 3 &&
		       loadedSoldier.fireControl().spreadDragStartGrid() == 1510 &&
		       loadedSoldier.fireControl().spreadDragEndGrid() == 1512 &&
		       loadedSoldier.fireControl().spreadDragMoved(),
		       "soldier save/load round-trips fire-control state at established schema positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.combatResult().currentAttacker() == SoldierID{ 12 } &&
		       loadedSoldier.combatResult().previousAttacker() == SoldierID{ 11 } &&
		       loadedSoldier.combatResult().earlierAttacker() == SoldierID{ 10 } &&
		       loadedSoldier.combatResult().hitLocation() == AIM_SHOT_HEAD &&
		       loadedSoldier.combatResult().lastDamageReason() == 8 &&
		       loadedSoldier.combatResult().hitsThisTurn() == 4 &&
		       loadedSoldier.combatResult().pelletsHitBy() == 5 &&
		       loadedSoldier.combatResult().accumulatedDamage() == 29,
		       "soldier save/load round-trips combat-result state at established schema positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.combatContribution().militiaKills() == 7 &&
		       loadedSoldier.combatContribution().militiaAssists() == 8 &&
		       loadedSoldier.combatContribution().damageByTeam()[0] == 81 &&
		       loadedSoldier.combatContribution().damageByTeam()[NUM_ASSIST_SLOTS - 1] == 82,
		       "soldier save/load round-trips militia credit and every fixed assist-attribution slot" );
		CHECK( saved && loaded &&
		       loadedSoldier.damageDisplay().displayFlag() == 2 &&
		       loadedSoldier.damageDisplay().counter() == 3 &&
		       loadedSoldier.damageDisplay().offsetX() == 17 &&
		       loadedSoldier.damageDisplay().offsetY() == -12 &&
		       loadedSoldier.damageDisplay().direction() == -1,
		       "soldier save/load round-trips damage-display state at established schema positions" );
		CHECK( saved && loaded &&
		       std::strcmp(loadedSoldier.renderState().headPalette(), "WHITEHEAD") == 0 &&
		       std::strcmp(loadedSoldier.renderState().pantsPalette(), "BLACKPANTS") == 0 &&
		       std::strcmp(loadedSoldier.renderState().vestPalette(), "BLACKSHIRT") == 0 &&
		       std::strcmp(loadedSoldier.renderState().skinPalette(), "DARKSKIN") == 0 &&
		       std::strcmp(loadedSoldier.renderState().miscPalette(), "SAVEMISC") == 0 &&
		       loadedSoldier.renderState().fadeMode() == 2 &&
		       loadedSoldier.renderState().fadeLevel() == 20 &&
		       loadedSoldier.renderState().fadeOriginGrid() == 2500 &&
		       loadedSoldier.renderState().forceRenderColor() &&
		       loadedSoldier.renderState().forceNoPaletteCycle() &&
		       loadedSoldier.renderState().forceShade() &&
		       loadedSoldier.renderState().muzzleFlashVisible() &&
		       loadedSoldier.renderState().unblitX() == 41 &&
		       loadedSoldier.renderState().unblitY() == 42 &&
		       loadedSoldier.renderState().unblitWidth() == 43 &&
		       loadedSoldier.renderState().unblitHeight() == 44 &&
		       loadedSoldier.renderState().lightSprite() == 801 &&
		       loadedSoldier.renderState().muzzleFlashSprite() == 802 &&
		       loadedSoldier.renderState().muzzleFlashFrame() == 5 &&
		       loadedSoldier.renderState().boundingBoxWidth() == 71 &&
		       loadedSoldier.renderState().boundingBoxHeight() == 72 &&
		       loadedSoldier.renderState().boundingBoxOffsetX() == -9 &&
		       loadedSoldier.renderState().boundingBoxOffsetY() == -10,
		       "soldier save/load round-trips render state while preserving fade mode 2" );
		CHECK( saved && loaded &&
		       loadedSoldier.suppression().underFire() == 2 &&
		       loadedSoldier.suppression().shock() == 10 &&
		       loadedSoldier.suppression().points() == 6 &&
		       loadedSoldier.suppression().actionPointsLost() == 13 &&
		       loadedSoldier.suppression().suppressor() == SoldierID{ 14 } &&
		       loadedSoldier.suppression().closeCall(),
		       "soldier save/load round-trips suppression state at established AI, flag, and POD positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.animationIntent().desiredHeight() == ANIM_PRONE &&
		       loadedSoldier.animationIntent().pendingAnimation() == SWATTING &&
		       loadedSoldier.animationIntent().pendingStance() == ANIM_CROUCH &&
		       loadedSoldier.animationIntent().secondaryPendingAnimation() == RUNNING &&
		       loadedSoldier.animationIntent().pendingDirection() == 7,
		       "soldier save/load round-trips component-owned queued animation transitions" );
		CHECK( saved && loaded &&
		       loadedSoldier.animationIntent().turningFromUi() &&
		       loadedSoldier.animationIntent().stopPendingNextTile() &&
		       loadedSoldier.animationIntent().continuationMode() == 2,
		       "soldier save/load preserves animation continuation modes without boolean normalization" );
		CHECK( saved && loaded &&
		       loadedSoldier.animationPlayback().state() == RUNNING &&
		       loadedSoldier.animationPlayback().code() == 27 &&
		       loadedSoldier.animationPlayback().frame() == 45 &&
		       loadedSoldier.animationPlayback().delay() == -13 &&
		       loadedSoldier.animationPlayback().previousState() == STANDING &&
		       loadedSoldier.animationPlayback().previousCode() == 9 &&
		       loadedSoldier.animationPlayback().surface() == 123 &&
		       loadedSoldier.animationPlayback().zLevel() == 456 &&
		       loadedSoldier.animationPlayback().subFlags() == 0xA5A55A5A,
		       "soldier save/load round-trips component-owned animation playback at established schema positions" );
		CHECK( saved && loaded &&
		       loadedSoldier.animationActivity().turningFromProneMode() == TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE &&
		       loadedSoldier.animationActivity().readyCostWaived() &&
		       loadedSoldier.animationActivity().postHitStance() == GO_TO_AIM_AFTER_HIT &&
		       loadedSoldier.animationActivity().paused() &&
		       loadedSoldier.animationActivity().holdAttackerUntilDone() &&
		       loadedSoldier.animationActivity().turningToShoot() &&
		       loadedSoldier.animationActivity().turningToFall() &&
		       loadedSoldier.animationActivity().turningUntilDone() &&
		       loadedSoldier.animationActivity().hitPhase() == 2 &&
		       loadedSoldier.animationActivity().nonInterruptible() &&
		       loadedSoldier.animationActivity().turningCostWaived() &&
		       loadedSoldier.animationActivity().suppressionStanceChange() &&
		       loadedSoldier.animationActivity().stanceCostWaived() &&
		       loadedSoldier.animationActivity().realtimeNonInterruptible() &&
		       loadedSoldier.animationActivity().tryingToFall() &&
		       loadedSoldier.animationActivity().fallClockwise() &&
		       loadedSoldier.animationActivity().fallDirection() == 4 &&
		       loadedSoldier.animationActivity().turningIncrement() == -1 &&
		       loadedSoldier.animationActivity().traversalForecastGrid() == 1520 &&
		       loadedSoldier.animationActivity().hasRenderZOverride() &&
		       loadedSoldier.animationActivity().renderZOverride() == 888 &&
		       loadedSoldier.animationActivity().randomActionCheckCounter() == 81 &&
		       loadedSoldier.animationActivity().lastRandomAnimation() == 124,
		       "soldier save/load round-trips animation activity and random cadence without normalizing hit phase 2 to boolean 1" );
	}

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

		const std::string oversizedPath = "engine_persistence_oversized_test.bin";
		const std::vector<std::uint8_t> oversizedBytes( 32, 0x5a );
		ByteStorage& platformStorage = GetPlatformByteStorage();
		std::vector<std::uint8_t> unchangedBytes = { 9 };
		PersistenceService tinyPersistence( platformStorage, 4 );
		PersistenceHeader unchangedHeader{ 77, 88 };
		CHECK( platformStorage.writeAll( oversizedPath, oversizedBytes ) &&
		       platformStorage.readAllBounded( oversizedPath, 4, unchangedBytes ) ==
		           ByteStorageReadResult::TooLarge && unchangedBytes == std::vector<std::uint8_t>({ 9 }) &&
		       tinyPersistence.loadEnvelope( oversizedPath, 1, 1, 1,
		           unchangedHeader, unchangedBytes ) == PersistenceLoadResult::TooLarge &&
		       unchangedHeader.magic == 77 && unchangedHeader.version == 88 &&
		       unchangedBytes == std::vector<std::uint8_t>({ 9 }),
		       "platform persistence rejects oversized files before publishing or decoding them" );
		CHECK( platformStorage.readAllBounded(
		           oversizedPath, oversizedBytes.size(), unchangedBytes ) ==
		           ByteStorageReadResult::Success && unchangedBytes == oversizedBytes,
		       "platform bounded reads accept an exact file-size limit" );
		FileDelete( const_cast<char*>(oversizedPath.c_str()) );
		unchangedBytes = { 9 };
		CHECK( platformStorage.readAllBounded(
		           oversizedPath, 4, unchangedBytes ) == ByteStorageReadResult::NotFound &&
		       unchangedBytes == std::vector<std::uint8_t>({ 9 }),
		       "platform bounded reads distinguish missing files without mutating output" );
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

	{
		CHECK( InitializeInputManager(), "InitializeInputManager() for saturation coverage" );
		for ( UINT32 index = 0; index < 256; ++index )
			QueueEvent( KEY_REPEAT, index, 0 );
		QueueEvent( KEY_REPEAT, 999, 0 );
		QueueEvent( KEY_UP, 'A', 0 );
		QueueEvent( LEFT_BUTTON_UP, 0, 0 );
		const InputQueueStatistics saturated = GetInputQueueStatistics();
		InputAtom atom = {};
		std::vector<UINT16> releases;
		UINT32 repeats = 0;
		while ( DequeueEvent( &atom ) )
		{
			if ( atom.usEvent == KEY_REPEAT ) ++repeats;
			if ( atom.usEvent == KEY_UP || atom.usEvent == LEFT_BUTTON_UP )
				releases.push_back( atom.usEvent );
		}
		CHECK( saturated.accepted == 258 && saturated.dropped == 1 &&
		       saturated.evictedForRelease == 2 && saturated.queued == 256 &&
		       repeats == 254 && releases == std::vector<UINT16>({ KEY_UP, LEFT_BUTTON_UP }),
		       "saturated input preserves ordered key and mouse releases by evicting repeats" );
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
