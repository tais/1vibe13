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

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <list>
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
#include "gameloop.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "CampaignPackage.h"
#include "RulesPackage.h"
#include "PackageHost.h"
#include "RuntimeReportHost.h"
#include "RuntimeSaveState.h"
#include "Simulation Commands.h"
#include "TacticalCommandHost.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "TacticalWorldObserverHost.h"
#include "Game Clock.h"
#include "Game Events.h"
#include "popup_class.h"
#include "Soldier Control.h"
#include "Animation Control.h"
#include "Map Information.h"
#include "Overhead.h"
#include "strategicmap.h"
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
		const TacticalEntityId actor{ 1, 101 };
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
			ValidateSimulationCommandDomain( SimulationCommand{ EndTurnCommand{
				1, static_cast<SimulationCommandSource>( 0xff ) } } ) ==
				SimulationCommandDomainError::InvalidSource,
			"all tactical execution paths share complete value-domain validation" );
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
		compiledContext.screenController().reset( 7 );
		compiledContext.screenController().transitionTo(
			9, []( UINT32 ) { return false; } );
		const bool previousScreenOwnedByController =
			GetPreviousScreen() == 7;
		compiledContext.screenController().reset( 9 );
		SetPendingNewScreen( 11 );
		const bool pendingScreenOwnedByController =
			GetPendingNewScreen() == 11 &&
			compiledContext.screenController().pending() &&
			*compiledContext.screenController().pending() == 11;
		SetPendingNewScreen( NO_PENDING_SCREEN );
		CHECK( previousScreenOwnedByController &&
		       GetPreviousScreen() == NO_PENDING_SCREEN &&
		       pendingScreenOwnedByController &&
		       GetPendingNewScreen() == NO_PENDING_SCREEN &&
		       !compiledContext.screenController().hasPending(),
		       "pending and previous application screen state have one controller-owned representation" );
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
		SOLDIERTYPE* const previousCommandActor = MercPtrs[0];
		const TacticalEntityId previousCommandEntity =
			GetJa2TacticalEntityId( 0 );
		SOLDIERTYPE commandHostActor;
		commandHostActor.ubID = SoldierID{ static_cast<UINT16>( 0 ) };
		commandHostActor.uiUniqueSoldierIdValue = 0x12345678u;
		commandHostActor.bActive = TRUE;
		commandHostActor.bInSector = TRUE;
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
		const TacticalCommandSubmissionResult unloadedContext =
			tacticalCommands.service->submit( packageId, staleMove );
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );
		const Ja2TacticalCommandHostDiagnostics commandHostAfterInvalidContext =
			GetJa2TacticalCommandHostDiagnostics();
		NotifyJa2TacticalWorldLoaded(
			previousCommandWorldSession.worldGeneration != 0
				? previousCommandWorldSession.worldGeneration : 1 );
		MercPtrs[0] = &commandHostActor;
		const bool commandHostActorAdopted =
			AdoptJa2TacticalEntity( commandHostActor );
		CHECK( commandHostActorAdopted &&
		       GetJa2TacticalEntityId( 0 ) == ( TacticalEntityId{ 0, 0x12345678u } ) &&
		       ResolveJa2TacticalEntity( TacticalEntityId{ 0, 0x12345678u } ) ==
		           &commandHostActor &&
		       ResolveJa2TacticalEntity( staleActor ) == nullptr,
		       "runtime entity directory rejects a stale incarnation for a reused pool slot" );
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
		       invalidMoveGrid && invalidMoveMode &&
		       unloadedContext && staleRequest &&
		       commandHostAfterInvalidContext.lastDrain.accepted == 0 &&
		       commandHostAfterInvalidContext.lastDrain.rejected == 7 &&
		       commandHostAfterValidation.lastDrain.accepted == 1 &&
		       commandHostAfterValidation.lastDrain.rejected == 0 &&
		       commandHostAfterValidation.inactiveOwnerRejections ==
		           commandHostBeforeValidation.inactiveOwnerRejections + 1 &&
		       commandHostAfterValidation.semanticRejections ==
		           commandHostBeforeValidation.semanticRejections + 5 &&
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

		commandHostActor.usUIMovementMode = WALKING;
		commandHostActor.bReverse = FALSE;
		commandHostActor.aiData.ubPendingAction = 7;
		const SimulationCommandDispatchResult invalidImmediateMove =
			TryDispatchMoveToGridCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue, -1, RUNNING,
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
		       commandHostActor.usUIMovementMode == WALKING &&
		       commandHostActor.bReverse == FALSE &&
		       commandHostActor.aiData.ubPendingAction == 7,
		       "immediate movement execution rejects invalid destinations before mutating the live actor" );

		beginCommandTestFrame();
		commandHostActor.bStealthMode = FALSE;
		const SimulationCommandDispatchResult stealthEnabled =
			TryDispatchSetStealthModeCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue, true,
				SimulationCommandSource::System );
		beginCommandTestFrame();
		commandHostActor.sGridNo = 77;
		commandHostActor.pathing.sFinalDestination = 99;
		commandHostActor.flags.fDelayedMovement = TRUE;
		commandHostActor.usAnimState = STANDING;
		const SimulationCommandDispatchResult movementStopped =
			TryDispatchStopMovementCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue,
				SimulationCommandSource::System );
		beginCommandTestFrame();
		const SimulationCommandDispatchResult facingQueued =
			TryDispatchSetFacingCommandNow(
				0, commandHostActor.uiUniqueSoldierIdValue, 3,
				SimulationCommandSource::System );
		CHECK( stealthEnabled.status == SimulationCommandDispatchStatus::Applied &&
		       commandHostActor.bStealthMode == TRUE &&
		       movementStopped.status == SimulationCommandDispatchStatus::Applied &&
		       commandHostActor.flags.fDelayedMovement == FALSE &&
		       commandHostActor.pathing.sFinalDestination == commandHostActor.sGridNo &&
		       facingQueued.status == SimulationCommandDispatchStatus::Applied,
		       "structured facing, stealth, and stop commands execute through the authoritative path" );

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
		       afterImmediateMoveDrain.receiptsQueued ==
		           retainedBeforeImmediateMove.receiptsQueued,
		       "immediate movement dispatch preserves earlier authoritative package work" );
		// Consume the retained host backpressure frame and publish the receipt
		// before starting the independent bounded-admission fixture below.
		beginCommandTestFrame();
		DrainJa2TacticalCommandsAtSafeFrame( compiledContext );

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
		MercPtrs[0] = previousCommandActor;
		if ( previousCommandEntity.valid() && previousCommandActor )
			(void)AdoptJa2TacticalEntity( *previousCommandActor );
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

		SOLDIERTYPE* previousSlot = MercPtrs[0];
		const TacticalEntityId previousWorldEntity =
			GetJa2TacticalEntityId( 0 );
		const INT8 previousActive = Menptr[0].bActive;
		const INT8 previousInSector = Menptr[0].bInSector;
		const SoldierID previousId = Menptr[0].ubID;
		const UINT32 previousIncarnation = Menptr[0].uiUniqueSoldierIdValue;
		const INT8 previousTeam = Menptr[0].bTeam;
		const UINT8 previousProfile = Menptr[0].ubProfile;
		const INT32 previousGrid = Menptr[0].sGridNo;
		const INT8 previousLevel = Menptr[0].pathing.bLevel;
		const UINT8 previousDirection = Menptr[0].ubDirection;
		const UINT16 previousAnimation = Menptr[0].usAnimState;
		const INT16 previousActionPoints = Menptr[0].bActionPoints;
		const INT8 previousLife = Menptr[0].stats.bLife;
		const INT8 previousMaximumLife = Menptr[0].stats.bLifeMax;
		const INT8 previousBreath = Menptr[0].bBreath;
		const INT8 previousMaximumBreath = Menptr[0].bBreathMax;
		const TacticalWorldSession::Snapshot previousWorldSession =
			compiledContext.runtime().tacticalWorldSession().snapshot();
		MercPtrs[0] = &Menptr[0];
		Menptr[0].ubID = SoldierID{ static_cast<UINT16>( 0 ) };
		Menptr[0].uiUniqueSoldierIdValue = 701;
		Menptr[0].bActive = TRUE;
		Menptr[0].bInSector = TRUE;
		Menptr[0].bTeam = 1;
		Menptr[0].ubProfile = 12;
		Menptr[0].sGridNo = 345;
		Menptr[0].pathing.bLevel = 1;
		Menptr[0].ubDirection = 3;
		Menptr[0].usAnimState = STANDING;
		Menptr[0].bActionPoints = 72;
		Menptr[0].stats.bLife = 76;
		Menptr[0].stats.bLifeMax = 80;
		Menptr[0].bBreath = 64;
		Menptr[0].bBreathMax = 90;
		const bool worldActorAdopted = AdoptJa2TacticalEntity( Menptr[0] );
		CHECK( worldActorAdopted &&
		       compiledContext.runtime().tacticalEntityDirectory().identity( 0 ) ==
		           ( TacticalEntityId{ 0, 701 } ),
		       "legacy pool actors publish liveness through the runtime-owned directory" );
		SOLDIERTYPE* const previousSwapTargetPointer = MercPtrs[1];
		const SOLDIERTYPE previousSwapTarget = Menptr[1];
		Menptr[1] = Menptr[0];
		MercPtrs[1] = &Menptr[1];
		Menptr[1].ubID = SoldierID{ static_cast<UINT16>( 1 ) };
		Menptr[1].uiUniqueSoldierIdValue = 702;
		Menptr[1].sGridNo = 678;
		const bool swapTargetAdopted = AdoptJa2TacticalEntity( Menptr[1] );
		const bool entitySlotsSwapped = SwapJa2TacticalEntitySlots( 0, 1 );
		const bool swappedEntitiesResolvable =
			GetJa2TacticalEntityId( 0 ) == ( TacticalEntityId{ 0, 702 } ) &&
			GetJa2TacticalEntityId( 1 ) == ( TacticalEntityId{ 1, 701 } ) &&
			ResolveJa2TacticalEntity( TacticalEntityId{ 0, 702 } ) == &Menptr[0] &&
			ResolveJa2TacticalEntity( TacticalEntityId{ 1, 701 } ) == &Menptr[1] &&
			Menptr[0].sGridNo == 678 && Menptr[1].sGridNo == 345;
		const bool entitySlotsRestored = SwapJa2TacticalEntitySlots( 0, 1 );
		CHECK( swapTargetAdopted && entitySlotsSwapped && swappedEntitiesResolvable &&
		       entitySlotsRestored &&
		       GetJa2TacticalEntityId( 0 ) == ( TacticalEntityId{ 0, 701 } ) &&
		       !SwapJa2TacticalEntitySlots( 0, 0 ) &&
		       !SwapJa2TacticalEntitySlots( TOTAL_SOLDIERS, 0 ),
		       "whole-record portrait swaps rebuild authoritative tactical entity identities" );
		Menptr[1] = previousSwapTarget;
		MercPtrs[1] = previousSwapTargetPointer;
		RebuildJa2TacticalEntityDirectory();
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
		       ( gTacticalStatus.uiFlags & TURNBASED ) != 0 &&
		       ( gTacticalStatus.uiFlags & INCOMBAT ) != 0 &&
		       gTacticalStatus.ubCurrentTeam == 1,
		       "tactical mode and current team are runtime-owned with exact legacy mirrors" );
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
		Menptr[0].sGridNo = 346;
		Menptr[0].stats.bLife = 75;
		UpdateJa2TacticalWorldObserverAtSafeFrame( liveRuntimeMessages );
		observerDiagnostics = GetJa2TacticalWorldObserverDiagnostics();
		observedPublication = tacticalWorldObserver.service->latest();
		observedActor = observedPublication
			? observedPublication.snapshot->find( TacticalEntityId{ 0, 701 } ) : nullptr;
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

		Menptr[0].uiUniqueSoldierIdValue = 0;
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

		Menptr[0].uiUniqueSoldierIdValue = 701;
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
		Menptr[0].sGridNo = 347;
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
		Menptr[0].sGridNo = 348;
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
		       MercPtrs[0] == &Menptr[0] &&
		       Menptr[0].ubID == SoldierID{ static_cast<UINT16>( 0 ) } &&
		       Menptr[0].uiUniqueSoldierIdValue == 701 && Menptr[0].sGridNo == 348 &&
		       Menptr[0].stats.bLife == 75,
		       "world unload invalidates publication and stale retry without mutating legacy state" );
		(void)ReleaseJa2TacticalEntity( Menptr[0] );
		Menptr[0].bActive = previousActive;
		Menptr[0].bInSector = previousInSector;
		Menptr[0].ubID = previousId;
		Menptr[0].uiUniqueSoldierIdValue = previousIncarnation;
		Menptr[0].bTeam = previousTeam;
		Menptr[0].ubProfile = previousProfile;
		Menptr[0].sGridNo = previousGrid;
		Menptr[0].pathing.bLevel = previousLevel;
		Menptr[0].ubDirection = previousDirection;
		Menptr[0].usAnimState = previousAnimation;
		Menptr[0].bActionPoints = previousActionPoints;
		Menptr[0].stats.bLife = previousLife;
		Menptr[0].stats.bLifeMax = previousMaximumLife;
		Menptr[0].bBreath = previousBreath;
		Menptr[0].bBreathMax = previousMaximumBreath;
		MercPtrs[0] = previousSlot;
		if ( previousWorldEntity.valid() && previousSlot )
			(void)AdoptJa2TacticalEntity( *previousSlot );
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
