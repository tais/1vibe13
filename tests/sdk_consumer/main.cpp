#include <Engine/Adapters/JA2/CommandReplay.h>
#include <Engine/Adapters/JA2/CampaignClockScheduler.h>
#include <Engine/Adapters/JA2/CampaignClockService.h>
#include <Engine/Adapters/JA2/CampaignClockSession.h>
#include <Engine/Adapters/JA2/CampaignEventQueue.h>
#include <Engine/Adapters/JA2/CampaignEventService.h>
#include <Engine/Adapters/JA2/EngineRuntime.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Adapters/JA2/TacticalCommandService.h>
#include <Engine/Adapters/JA2/TacticalInventoryUiSession.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaCodec.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaPublisher.h>
#include <Engine/Adapters/JA2/TacticalWorldObserver.h>
#include <Engine/Core/EngineHost.h>
#include <Engine/Core/EngineHostOptions.h>
#include <Engine/Core/EngineServiceContracts.h>
#include <Engine/Core/RenderCommands.h>
#include <Engine/Core/RenderSurfaceAccess.h>
#include <Engine/Core/ServiceCatalog.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{
TacticalActorSnapshot MakeExternalActor(
	TacticalEntityId id,
	std::int32_t grid,
	TacticalStance stance,
	std::int16_t actionPoints,
	std::int16_t life)
{
	TacticalActorSnapshot actor;
	actor.id = id;
	actor.team = 0;
	actor.profile = 17;
	actor.grid = grid;
	actor.level = 0;
	actor.direction = 2;
	actor.animation = stance == TacticalStance::Standing ? 100 : 200;
	actor.stance = stance;
	actor.actionPoints = actionPoints;
	actor.life = life;
	actor.maximumLife = 85;
	actor.breath = 70;
	actor.maximumBreath = 90;
	actor.active = true;
	actor.inSector = true;
	return actor;
}

bool MakeExternalSnapshot(
	std::uint64_t epoch,
	std::vector<TacticalActorSnapshot> actors,
	TacticalWorldSnapshot& snapshot)
{
	return TacticalWorldSnapshot::create(
		epoch,
		TacticalSectorSnapshot{9, 1, 0, true},
		TacticalTurnSnapshot{true, true, 0, 12},
		std::move(actors), snapshot) == TacticalSnapshotCreateError::None;
}

class ExternalTacticalDeltaSink final : public RuntimeMessageSink
{
public:
	void receiveMessage(const RuntimeMessage& message) override
	{
		++received;
		topic = message.topic;
		source = message.source;
		sequence = message.sequence;
		decodeResult = DecodeTacticalWorldDelta(message.payload, delta);
	}

	std::size_t received = 0;
	std::string topic;
	std::string source;
	std::uint64_t sequence = 0;
	TacticalWorldDeltaDecodeResult decodeResult =
		TacticalWorldDeltaDecodeResult::Invalid;
	TacticalWorldDelta delta;
};

class ExternalChunkedDeltaSink final : public RuntimeMessageSink
{
public:
	ExternalChunkedDeltaSink()
		: reassembler(TacticalWorldDeltaReassemblyLimits{4096, 128, 3}) {}

	void receiveMessage(const RuntimeMessage& message) override
	{
		lastResult = reassembler.accept(message, delta);
		if (lastResult == TacticalWorldDeltaReassemblyResult::Completed)
			++completed;
	}

	TacticalWorldDeltaReassembler reassembler;
	TacticalWorldDeltaReassemblyResult lastResult =
		TacticalWorldDeltaReassemblyResult::InvalidMessage;
	TacticalWorldDelta delta;
	std::size_t completed = 0;
};
}

class ExternalRulesPackage final : public EnginePackage
{
public:
	ExternalRulesPackage()
		: descriptor_{
			ContentManifest{"external.rules", "1.0.0", ContentApiVersion{1, 0}},
			PackageKind::Rules,
			{"rules.external-consumer"}}
	{
		descriptor_.saveStateSchemaVersion = 1;
	}

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override
	{
		if (active_) return false;
		active_ = true;
		return true;
	}
	void deactivate() noexcept override { active_ = false; }
	bool bootstrap(PackageBootstrapContext& context, PackageBootstrapPhase phase) override
	{
		if (phase == PackageBootstrapPhase::Configure)
		{
			issuedIdentity_ = context.identity;
			commandBinding_ = BindTacticalCommandClient(
				context.extensionServices, context.identity);
		}
		return phase != PackageBootstrapPhase::Configure || commandBinding_;
	}
	const PackageIdentity& issuedIdentity() const { return issuedIdentity_; }
	const TacticalCommandClientBindingResult& commandBinding() const
	{
		return commandBinding_;
	}
	bool saveState(PackageBootstrapContext&, std::vector<std::uint8_t>& state) override
	{
		state = {1, 2, 3};
		return true;
	}
	bool loadState(PackageBootstrapContext&, std::uint32_t schema,
		const std::vector<std::uint8_t>& state) override
	{
		return schema == 1 && state == std::vector<std::uint8_t>({1, 2, 3});
	}

private:
	PackageDescriptor descriptor_;
	PackageIdentity issuedIdentity_;
	TacticalCommandClientBindingResult commandBinding_;
	bool active_ = false;
};

class ExternalTypedService
{
public:
	virtual ~ExternalTypedService() = default;
	virtual unsigned value() const = 0;
};

class ExternalTypedImplementation final : public ExternalTypedService
{
public:
	unsigned value() const override { return 29; }
};

inline constexpr EngineServiceContract<ExternalTypedService>
	ExternalTypedServiceContract{"external.typed-service", {1, 0}};

int main()
{
	EngineHost<> legacyBraceHost({});
	EngineRuntime<> legacyBraceRuntime({});
	if (legacyBraceHost.serviceCatalog().size() != 14 ||
		legacyBraceRuntime.serviceCatalog().size() != 14) return 42;
	legacyBraceRuntime.tacticalWorldSession().setTurnState({true, true, 2});
	if (legacyBraceRuntime.tacticalWorldSession().snapshot().turn !=
		TacticalWorldSession::Snapshot::Turn{true, true, 2}) return 53;
	const StrategicGroupId externalStrategicGroup =
		legacyBraceRuntime.strategicGroupDirectory().adopt(7);
	if (externalStrategicGroup != (StrategicGroupId{7, 1}) ||
		!legacyBraceRuntime.strategicGroupDirectory().contains(
			externalStrategicGroup) ||
		!legacyBraceRuntime.strategicGroupDirectory().release(
			externalStrategicGroup))
		return 58;
	const TacticalEntityId externalInventoryActor{3, 0x01020304u};
	if (!legacyBraceRuntime.tacticalInventoryUiSession().setActor(
			TacticalInventoryActorRole::SelectedMerc,
			externalInventoryActor) ||
		legacyBraceRuntime.tacticalInventoryUiSession().actor(
			TacticalInventoryActorRole::SelectedMerc) !=
			externalInventoryActor ||
		legacyBraceRuntime.tacticalInventoryUiSession().actorContextCount() != 1)
		return 59;
	legacyBraceRuntime.tacticalInventoryUiSession().reset();
	if (legacyBraceRuntime.tacticalInventoryUiSession().actorContextCount() != 0)
		return 59;
	CampaignClockSession externalCampaignClock;
	externalCampaignClock.initialize(90061);
	externalCampaignClock.advanceUncommitted(60);
	if (externalCampaignClock.commitAdvance().movedBackward ||
		externalCampaignClock.snapshot() != CampaignClockSession::Snapshot{
			90121, 90121, 1, 1, 2} ||
		&legacyBraceRuntime.campaignClockSession() !=
			&legacyBraceRuntime.campaignClockSession()) return 49;
	legacyBraceRuntime.campaignClockSession().restore(
		externalCampaignClock.snapshot());
	const CampaignClockScheduleResult externalCampaignSchedule =
		legacyBraceRuntime.campaignClockScheduler().schedule(1000000, 1, 1);
	if (!externalCampaignSchedule ||
		externalCampaignSchedule.advanceSeconds != 1 ||
		externalCampaignSchedule.completedRealSeconds != 1) return 54;
	if (RegisterCampaignClockService(
			legacyBraceRuntime.serviceCatalog(),
			legacyBraceRuntime.campaignClockService()) !=
			EngineServiceRegistrationError::None) return 50;
	const auto externalCampaignClockService =
		legacyBraceRuntime.serviceCatalog().resolve(CampaignClockServiceContract);
	CampaignClockSession::Snapshot capturedCampaignClock;
	if (!externalCampaignClockService ||
		externalCampaignClockService.service->capture(capturedCampaignClock) !=
			CampaignClockCaptureResult::Success ||
		capturedCampaignClock != externalCampaignClock.snapshot()) return 50;
	CampaignEventQueueSnapshot externalCampaignEvents;
	if (CampaignEventQueueSnapshot::create(
			{{90121, 7, 0, 0, 17, 0}, {90121, 8, 3600, 4, 18, 1}},
			externalCampaignEvents) != CampaignEventSnapshotCreateError::None)
		return 51;
	MemoryCampaignEventService externalCampaignEventService;
	externalCampaignEventService.publish(externalCampaignEvents);
	if (RegisterCampaignEventService(
			legacyBraceRuntime.serviceCatalog(),
			externalCampaignEventService) !=
			EngineServiceRegistrationError::None) return 51;
	const auto resolvedExternalCampaignEvents =
		legacyBraceRuntime.serviceCatalog().resolve(CampaignEventServiceContract);
	CampaignEventQueueSnapshot capturedExternalCampaignEvents;
	if (!resolvedExternalCampaignEvents ||
		resolvedExternalCampaignEvents.service->capture(
			capturedExternalCampaignEvents) !=
			CampaignEventCaptureResult::Success ||
		capturedExternalCampaignEvents.events() !=
			externalCampaignEvents.events()) return 51;
	const CampaignEventScheduleResult runtimeOwnedCampaignEvent =
		legacyBraceRuntime.campaignEventQueue().schedule(
			{90122, 9, 0, 0, 19, 0});
	std::vector<CampaignEventSnapshot> runtimeOwnedCampaignEvents;
	if (!runtimeOwnedCampaignEvent ||
		!legacyBraceRuntime.campaignEventQueue().capture(
			runtimeOwnedCampaignEvents) ||
		runtimeOwnedCampaignEvents !=
			std::vector<CampaignEventSnapshot>{{90122, 9, 0, 0, 19, 0}})
		return 52;

	MemoryByteStorage storage;
	MemoryRenderSurfaceAccess renderSurfaces(1024);
	if (!renderSurfaces.defineSurface(
			1, RenderSurfaceDescription{
				4, 4, RenderPixelFormat::Argb8888, 32}) ||
		!renderSurfaces.defineSurface(
			2, RenderSurfaceDescription{
				2, 1, RenderPixelFormat::Argb8888, 32}) ||
		!renderSurfaces.defineSurface(
			3, RenderSurfaceDescription{
				3, 2, RenderPixelFormat::Depth16, 16}) ||
		!renderSurfaces.setSurfaceFor(RenderSurfaceRole::FrameBuffer, 1) ||
		!renderSurfaces.setSurfaceFor(RenderSurfaceRole::DepthBuffer, 3))
		return 48;
	MutableRenderSurface externalSurface;
	if (!renderSurfaces.map(1, externalSurface) ||
		externalSurface.pitchBytes != 16 || externalSurface.sizeBytes != 64)
		return 48;
	externalSurface.pixels[0] = std::byte{0x2a};
	renderSurfaces.unmap(1);
	if (renderSurfaces.mappingCount(1) != 0) return 48;
	MappedRenderCommandSink renderCommands(renderSurfaces);
	if (!renderCommands.fillSurface(RenderSurfaceFillCommand{
			1, RenderSurfaceRegion{1, 1, 3, 3},
			RenderColor{0x12, 0x34, 0x56, 0xff}}) ||
		!renderSurfaces.map(1, externalSurface))
		return 48;
	std::uint32_t externalPixel = 0;
	std::memcpy(
		&externalPixel,
		externalSurface.pixels + externalSurface.pitchBytes + 4,
		sizeof(externalPixel));
	renderSurfaces.unmap(1);
	if (externalPixel != 0xff123456u ||
		renderSurfaces.mappingCount(1) != 0)
		return 48;
	MutableRenderSurface copySource;
	if (!renderSurfaces.map(2, copySource)) return 48;
	const std::uint32_t externalCopyPixel = 0xffabcdefu;
	std::memcpy(
		copySource.pixels + sizeof(externalCopyPixel),
		&externalCopyPixel, sizeof(externalCopyPixel));
	renderSurfaces.unmap(2);
	if (!renderCommands.copySurface(RenderSurfaceCopyCommand{
			2, 1, RenderSurfaceRegion{1, 0, 2, 1},
			RenderSurfacePoint{3, 3}, RenderSurfaceCopyMode::Opaque, {}}) ||
		!renderSurfaces.map(1, externalSurface))
		return 48;
	externalPixel = 0;
	std::memcpy(
		&externalPixel,
		externalSurface.pixels + 3 * externalSurface.pitchBytes +
			3 * sizeof(externalPixel),
		sizeof(externalPixel));
	renderSurfaces.unmap(1);
	if (externalPixel != externalCopyPixel ||
		renderSurfaces.mappingCount(1) != 0 ||
		renderSurfaces.mappingCount(2) != 0)
		return 48;
	if (!renderCommands.stretchSurface(RenderSurfaceStretchCommand{
			2, 1, RenderSurfaceRegion{0, 0, 2, 1},
			RenderSurfaceRegion{0, 0, 4, 1},
			RenderSurfaceCopyMode::Opaque, {}}) ||
		!renderCommands.shadeSurface(RenderSurfaceShadeCommand{
			1, RenderSurfaceRegion{2, 0, 4, 1}, 1, 2}) ||
		!renderSurfaces.map(1, externalSurface))
		return 48;
	externalPixel = 0;
	std::memcpy(
		&externalPixel,
		externalSurface.pixels + 2 * sizeof(externalPixel),
		sizeof(externalPixel));
	renderSurfaces.unmap(1);
	if (externalPixel != 0xff556677u ||
		renderSurfaces.mappingCount(1) != 0 ||
		renderSurfaces.mappingCount(2) != 0)
		return 48;
	const RenderDepthFillCommand externalDepthCommand{
		3, RenderSurfaceRegion{1, -1, 4, 1}, 0x4321};
	if (!renderCommands.fillDepth(externalDepthCommand) ||
		!renderCommands.fillDepth(RenderDepthFillCommand{
			3, RenderSurfaceRegion{9, 9, 10, 10}, 1}) ||
		renderCommands.fillSurface(RenderSurfaceFillCommand{
			3, RenderSurfaceRegion{0, 0, 3, 2}, {}}) ||
		renderSurfaces.surfaceFor(RenderSurfaceRole::DepthBuffer) != 3 ||
		!renderSurfaces.map(3, externalSurface))
		return 48;
	bool externalDepthMatches = true;
	for (std::uint32_t y = 0; externalDepthMatches && y < 2; ++y)
	{
		for (std::uint32_t x = 0; x < 3; ++x)
		{
			std::uint16_t depth = 0;
			std::memcpy(
				&depth,
				externalSurface.pixels + y * externalSurface.pitchBytes +
					x * sizeof(depth),
				sizeof(depth));
			const std::uint16_t expected =
				y == 0 && x >= 1 ? 0x4321 : 0;
			if (depth != expected) externalDepthMatches = false;
		}
	}
	renderSurfaces.unmap(3);
	if (!externalDepthMatches || renderSurfaces.mappingCount(3) != 0)
		return 48;
	RecordingRenderCommandSink recordedImageCommands;
	const RenderImageDrawCommand externalImageCommand{
		1, 44, 3, RenderSurfacePoint{-2, 5},
		RenderSurfaceRegion{0, 0, 4, 4},
		RenderImageCompositeMode::Intensity};
	const RenderImageDrawCommand externalPaletteImageCommand{
		1, 48, 7, RenderSurfacePoint{2, -5},
		RenderSurfaceRegion{0, 0, 4, 4},
		RenderImageCompositeMode::PaletteWithShadowMarker,
		(RenderPaletteId{1} << 32) + 1, 49, true};
	const RenderImageDrawCommand externalClearImageCommand{
		1, 55, 11, RenderSurfacePoint{-4, 6},
		RenderSurfaceRegion{0, 0, 4, 4},
		RenderImageCompositeMode::ClearDestination};
	if (!recordedImageCommands.drawImage(externalImageCommand) ||
		!recordedImageCommands.drawImage(
			externalPaletteImageCommand) ||
		!recordedImageCommands.drawImage(
			externalClearImageCommand) ||
		recordedImageCommands.imageCommands() !=
			std::vector<RenderImageDrawCommand>{
				externalImageCommand, externalPaletteImageCommand,
				externalClearImageCommand})
		return 48;
	const RenderImageDepthDrawCommand externalDepthImageCommand{
		1, 3, 46, 5, RenderSurfacePoint{-1, 2},
		RenderSurfaceRegion{0, 0, 4, 4}, 0x2222,
		RenderDepthCompareMode::Greater,
		RenderDepthWriteMode::ReplaceOnDraw,
		RenderImageDepthEffect::PixelateObscuredSourcePalette};
	if (!recordedImageCommands.drawImageDepth(
			externalDepthImageCommand) ||
		recordedImageCommands.imageDepthCommands() !=
			std::vector<RenderImageDepthDrawCommand>{
				externalDepthImageCommand})
		return 48;
	const RenderImageDepthDrawCommand
		externalPaletteDepthImageCommand{
			1, 3, 50, 8, RenderSurfacePoint{1, -2},
			RenderSurfaceRegion{0, 0, 4, 4}, 0x4444,
			RenderDepthCompareMode::GreaterOrEqual,
			RenderDepthWriteMode::Preserve,
			RenderImageDepthEffect::PaletteWithShadowMarker,
			(RenderPaletteId{1} << 32) + 2, 51, true};
	if (!recordedImageCommands.drawImageDepth(
			externalPaletteDepthImageCommand) ||
		recordedImageCommands.imageDepthCommands() !=
			std::vector<RenderImageDepthDrawCommand>{
				externalDepthImageCommand,
				externalPaletteDepthImageCommand})
		return 48;
	const RenderImageDepthDrawCommand
		externalObscuredPaletteDepthImageCommand{
			1, 3, 52, 9, RenderSurfacePoint{-2, 3},
			RenderSurfaceRegion{0, 0, 4, 4}, 0x5555,
			RenderDepthCompareMode::GreaterOrEqual,
			RenderDepthWriteMode::Preserve,
			RenderImageDepthEffect::
				PaletteWithShadowMarkerPixelateObscured,
			(RenderPaletteId{1} << 32) + 3, 53, false};
	if (!recordedImageCommands.drawImageDepth(
			externalObscuredPaletteDepthImageCommand) ||
		recordedImageCommands.imageDepthCommands() !=
			std::vector<RenderImageDepthDrawCommand>{
				externalDepthImageCommand,
				externalPaletteDepthImageCommand,
				externalObscuredPaletteDepthImageCommand})
		return 48;
	const RenderImageDepthDrawCommand externalStripDepthImageCommand{
		1, 3, 54, 10, RenderSurfacePoint{-3, 4},
		RenderSurfaceRegion{0, 0, 4, 4}, 0x6666,
		RenderDepthCompareMode::GreaterOrEqual,
		RenderDepthWriteMode::ReplaceOnPass,
		RenderImageDepthEffect::StripDepthSourcePalette,
		0, 0, false, 2};
	if (!recordedImageCommands.drawImageDepth(
			externalStripDepthImageCommand) ||
		recordedImageCommands.imageDepthCommands() !=
			std::vector<RenderImageDepthDrawCommand>{
				externalDepthImageCommand,
				externalPaletteDepthImageCommand,
				externalObscuredPaletteDepthImageCommand,
				externalStripDepthImageCommand})
		return 48;
	const RenderImageDepthVisibilityQuery
		externalDepthVisibilityQuery{
			3, 56, 12, RenderSurfacePoint{-5, 7},
			RenderSurfaceRegion{0, 0, 4, 4}, -321};
	recordedImageCommands.setImageDepthVisibilityResult(
		RenderImageDepthVisibility::FullyOccluded);
	if (recordedImageCommands.queryImageDepthVisibility(
			externalDepthVisibilityQuery) !=
			RenderImageDepthVisibility::FullyOccluded ||
		recordedImageCommands.imageDepthVisibilityQueries() !=
			std::vector<RenderImageDepthVisibilityQuery>{
				externalDepthVisibilityQuery})
		return 48;
	const RenderImageOutlineCommand externalOutlineCommand{
		1, 45, 4, RenderSurfacePoint{3, -1},
		RenderSurfaceRegion{0, 0, 4, 4},
		RenderImageOutlineMode::Color,
		RenderColor{10, 20, 30, 255}, true};
	if (!recordedImageCommands.drawImageOutline(externalOutlineCommand) ||
		recordedImageCommands.imageOutlineCommands() !=
			std::vector<RenderImageOutlineCommand>{
				externalOutlineCommand})
		return 48;
	const RenderImageDepthOutlineCommand externalDepthOutlineCommand{
		1, 3, 47, 6, RenderSurfacePoint{-3, 2},
		RenderSurfaceRegion{0, 0, 4, 4}, 0x3333,
		RenderDepthCompareMode::Greater,
		RenderDepthWriteMode::ReplaceOnPass,
		RenderImageDepthOutlineVisibility::PixelateWhenObscured,
		RenderColor{20, 30, 40, 255}, true};
	if (!recordedImageCommands.drawImageDepthOutline(
			externalDepthOutlineCommand) ||
		recordedImageCommands.imageDepthOutlineCommands() !=
			std::vector<RenderImageDepthOutlineCommand>{
				externalDepthOutlineCommand})
		return 48;
	if (!recordedImageCommands.fillDepth(externalDepthCommand) ||
		recordedImageCommands.depthFillCommands() !=
			std::vector<RenderDepthFillCommand>{externalDepthCommand})
		return 48;
	EngineServices services{
		ZeroTimeSource::instance(), ZeroRandomSource::instance(), storage};
	EngineHostOptions hostOptions;
	if (!hostOptions.hostCapabilities.add("host.external-consumer")) return 1;
	EngineHost<> host(std::move(hostOptions), services);
	if (host.runtimeMessages().maxQueuedMessages() != 1024 ||
		host.inputDispatcher().maxEventsPerDispatch() != 256 ||
		host.persistence().maximumPayloadBytes() !=
			PersistenceService::DefaultMaximumPayloadBytes) return 1;
	TacticalCommandInbox commandInbox(
		TacticalCommandInboxLimits{2, 1, 1, 64, 10});
	if (RegisterTacticalCommandService(host.serviceCatalog(), commandInbox) !=
		EngineServiceRegistrationError::None) return 33;
	ExternalTypedImplementation typedServiceImplementation;
	EngineServiceContract<ExternalTypedService> invalidTypedServiceContract;
	if (host.serviceCatalog().registerService(
			ExternalTypedServiceContract, typedServiceImplementation) !=
			EngineServiceRegistrationError::None ||
		host.serviceCatalog().registerService(
			invalidTypedServiceContract, typedServiceImplementation) !=
			EngineServiceRegistrationError::InvalidDescriptor ||
		host.serviceCatalog().resolve(invalidTypedServiceContract).error !=
			EngineServiceLookupError::InvalidDescriptor) return 43;
	unsigned externalService = 17;
	if (host.serviceCatalog().registerService(
		"external.test-service", EngineServiceVersion{1, 2}, externalService) !=
		EngineServiceRegistrationError::None) return 1;
	if (host.configuration().set(
		"external.test-value", std::int64_t{23}) !=
		RuntimeConfigurationSetError::None) return 1;

	ExternalRulesPackage package;
	if (host.packages().registerPackage(package) != PackageRegistrationError::None ||
		host.packages().activate("external.rules") != PackageActivationError::None ||
		!host.hasCapability("host.external-consumer") ||
		!host.hasCapability("rules.external-consumer")) return 2;
	const RuntimeCompatibilityFingerprint fingerprint = host.compatibilityFingerprint();
	if (fingerprint.hex().size() != 40 ||
		fingerprint != host.diagnostics().compatibility) return 8;
	const PackageResourceUsageSnapshot resources = host.packageResourceUsage();
	const PackageResourceUsage* packageResources = resources.find("external.rules");
	if (!packageResources || !packageResources->active ||
		resources.unattributedRecords != 0) return 9;
	if (host.saveRuntimeCheckpoint("external.checkpoint") !=
		RuntimeCheckpointSaveError::None) return 10;
	RuntimeCheckpoint checkpoint;
	if (!host.loadRuntimeCheckpoint("external.checkpoint", checkpoint) ||
		checkpoint.activePackages.size() != 1 ||
		checkpoint.activePackages[0].id != "external.rules") return 11;
	const PackageSaveArchive externalPackageState{fingerprint,
		PackageSaveStateSnapshot{{
			PackageSaveStateRecord{"external.rules", "1.0.0", 1, {1, 2, 3}}}}};
	if (host.packageSaveArchives().save("external.package-state", externalPackageState) !=
		PackageSaveArchiveSaveError::None) return 12;
	PackageSaveArchive loadedExternalPackageState;
	if (!host.packageSaveArchives().load(
		"external.package-state", fingerprint, loadedExternalPackageState) ||
		loadedExternalPackageState.state.records.size() != 1 ||
		loadedExternalPackageState.state.records[0].payload !=
			std::vector<std::uint8_t>({1, 2, 3})) return 13;

	const std::vector<std::uint8_t> saved{2, 3, 5, 7};
	if (host.persistence().saveEnvelope(
		"external.record", PersistenceHeader{0x54534f48u, 1}, saved) !=
		PersistenceSaveResult::Success) return 3;
	PersistenceHeader header{};
	std::vector<std::uint8_t> loaded;
	if (host.persistence().loadEnvelope(
		"external.record", 0x54534f48u, 1, 1, header, loaded) !=
			PersistenceLoadResult::Success ||
		header.version != 1 || loaded != saved) return 4;

	host.screenController().reset(7);
	const RuntimeSessionTransitionResult initializing = host.tryBeginInitialization();
	const RuntimeSessionTransitionResult prematureRunning = host.tryMarkRunning();
	const RuntimeSessionAdvanceResult packagesStarted =
		host.runtimeSession().advancePackagesTo(PackageBootstrapPhase::StartRuntime);
	const RuntimeSessionTransitionResult running = host.tryMarkRunning();
	if (!host.screens().current() || host.screens().current()->state != 7 ||
		!initializing ||
		prematureRunning.error != RuntimeSessionError::PackageBootstrapIncomplete ||
		!packagesStarted || packagesStarted.packages.callbackException ||
		!package.issuedIdentity() ||
		package.issuedIdentity().id() != "external.rules" || !running) return 5;
	const PackageSaveStateCaptureResult capturedExternalState =
		host.capturePackageSaveState();
	if (!capturedExternalState || capturedExternalState.snapshot.records.size() != 1 ||
		capturedExternalState.snapshot.records[0].payload !=
			std::vector<std::uint8_t>({1, 2, 3}) ||
		!host.validatePackageSaveState(capturedExternalState.snapshot) ||
		!host.restorePackageSaveState(capturedExternalState.snapshot)) return 14;
	const TacticalCommandSubmissionResult commandRequest =
		package.commandBinding()
			? package.commandBinding().client.submit(
				SimulationCommand{MoveToGridCommand{
					TacticalEntityId{7, 3}, 1311, 0, false, true,
					SimulationCommandSource::System,
					TacticalMoveOrigin::System,
					TacticalPendingActionPolicy::Preserve}})
			: TacticalCommandSubmissionResult{
				TacticalCommandSubmissionError::InvalidOwner, 0};
	std::uint64_t drainedRequest = 0;
	bool drainedMoveToGrid = false;
	bool drainedMovePolicy = false;
	const TacticalCommandDrainResult commandDrain = commandInbox.drain(
		[&drainedRequest, &drainedMoveToGrid, &drainedMovePolicy](
			const TacticalCommandRequest& request) {
			drainedRequest = request.requestId;
			drainedMoveToGrid = std::holds_alternative<MoveToGridCommand>(
				request.command);
			if (drainedMoveToGrid)
			{
				const MoveToGridCommand& move =
					std::get<MoveToGridCommand>(request.command);
				drainedMovePolicy = move.origin == TacticalMoveOrigin::System &&
					move.pendingAction ==
						TacticalPendingActionPolicy::Preserve;
			}
			return TacticalCommandDisposition::Accept;
		});
	if (!commandRequest || commandRequest.requestId != 1 ||
		package.commandBinding().client.packageId() != "external.rules" ||
		commandDrain.accepted != 1 || !drainedMoveToGrid || !drainedMovePolicy ||
		drainedRequest != commandRequest.requestId ||
		!commandInbox.empty()) return 34;
	const RuntimeSessionTransitionResult shuttingDown = host.tryBeginShutdown();
	const RuntimeSessionTransitionResult prematureStopped = host.tryMarkStopped();
	const RuntimeSessionShutdownResult packagesStopped =
		host.runtimeSession().shutdownPackages();
	const RuntimeSessionTransitionResult stopped = host.tryMarkStopped();
	if (!shuttingDown ||
		prematureStopped.error != RuntimeSessionError::PackageShutdownIncomplete ||
		!packagesStopped || !stopped) return 5;
	const EngineServiceLookupResult<unsigned> resolved =
		host.serviceCatalog().resolve<unsigned>(
			"external.test-service", EngineServiceVersion{1, 1});
	const EngineServiceLookupResult<ExternalTypedService> typedService =
		host.serviceCatalog().resolve(ExternalTypedServiceContract);
	const EngineServiceLookupResult<FrameTelemetry> telemetryService =
		host.serviceCatalog().resolve(FrameTelemetryServiceContract);
	if (!resolved || resolved.service != &externalService ||
		!typedService || typedService.service->value() != 29 ||
		!telemetryService || telemetryService.service != &host.frameTelemetry() ||
		!host.serviceCatalog().sealed()) return 6;
	const std::int64_t* configured =
		host.configuration().find<std::int64_t>("external.test-value");
	if (!configured || *configured != 23 || !host.configuration().sealed()) return 7;

	// Everything below is supplied by the installed RuntimeAdapter archive and
	// headers. The external consumer deliberately supplies its own value-only
	// tactical data rather than including or binding any JA2 application state.
	const TacticalEntityId actorId{7, 3};
	const TacticalEntityId earlierId{2, 9};
	if (!actorId.valid() || !earlierId.valid() || actorId == TacticalEntityId{7, 2} ||
		!(earlierId < actorId)) return 15;

	TacticalActorSnapshot previousActor = MakeExternalActor(
		actorId, 1200, TacticalStance::Standing, 20, 85);
	TacticalActorSnapshot previousEarlierActor = MakeExternalActor(
		earlierId, 900, TacticalStance::Crouched, 18, 80);
	TacticalWorldSnapshot previous;
	if (!MakeExternalSnapshot(41,
		{previousActor, previousEarlierActor}, previous) ||
		previous.actors().size() != 2 || previous.actors()[0].id != earlierId ||
		!previous.find(actorId) || previous.find(TacticalEntityId{7, 2})) return 16;

	TacticalActorSnapshot currentActor = previousActor;
	currentActor.grid = 1211;
	currentActor.direction = 3;
	currentActor.stance = TacticalStance::Crouched;
	currentActor.animation = 201;
	currentActor.actionPoints = 14;
	currentActor.life = 79;
	TacticalWorldSnapshot current;
	if (!MakeExternalSnapshot(41,
		{currentActor, previousEarlierActor}, current)) return 17;

	TacticalWorldDelta delta;
	if (DiffTacticalWorldSnapshots(previous, current, 3, delta) !=
		TacticalWorldDiffResult::Success ||
		delta.previousEpoch != 41 || delta.currentEpoch != 41 ||
		delta.events.size() != 3 ||
		!std::holds_alternative<TacticalActorMovedEvent>(delta.events[0]) ||
		!std::holds_alternative<TacticalActorStanceChangedEvent>(delta.events[1]) ||
		!std::holds_alternative<TacticalActorVitalsChangedEvent>(delta.events[2])) return 18;

	std::vector<std::uint8_t> encodedDelta;
	TacticalWorldDelta decodedDelta;
	if (EncodeTacticalWorldDelta(delta, encodedDelta) !=
		TacticalWorldDeltaEncodeResult::Success || encodedDelta.empty() ||
		DecodeTacticalWorldDelta(encodedDelta, decodedDelta) !=
		TacticalWorldDeltaDecodeResult::Success ||
		decodedDelta.events.size() != delta.events.size() ||
		std::get<TacticalActorMovedEvent>(decodedDelta.events[0]).currentGrid != 1211 ||
		std::get<TacticalActorVitalsChangedEvent>(decodedDelta.events[2]).currentLife != 79)
		return 19;

	MemoryTacticalWorldService tacticalSource;
	TacticalWorldObserver observer(tacticalSource, TacticalWorldObserverLimits{2, 3});
	tacticalSource.publish(previous);
	if (observer.update() != TacticalWorldObserverUpdateResult::PublishedBaseline)
		return 20;
	const TacticalWorldPublicationView baseline = observer.latest();
	if (!baseline || baseline.status != TacticalWorldPublicationStatus::Baseline ||
		baseline.serial != 1 || !baseline.snapshot->find(actorId) ||
		!baseline.delta->events.empty()) return 21;

	tacticalSource.publish(current);
	if (observer.update() != TacticalWorldObserverUpdateResult::PublishedDelta)
		return 22;
	const TacticalWorldPublicationView publication = observer.latest();
	if (!publication || publication.status != TacticalWorldPublicationStatus::Delta ||
		publication.serial != 2 || publication.delta->events.size() != 3)
		return 23;

	RuntimeMessageBus messages(2, 4096);
	ExternalTacticalDeltaSink sink;
	if (messages.addSink(sink) != RuntimeMessageSinkRegistrationError::None)
		return 24;
	TacticalWorldDeltaPublisher publisher(
		messages, TacticalWorldDeltaPublishLimits{3, 4096});
	PreparedTacticalWorldDeltaMessage preparedPublication;
	if (publisher.prepare(*publication.delta, preparedPublication) !=
		TacticalWorldDeltaPublishError::None) return 25;
	const TacticalWorldDeltaPublishResult published =
		publisher.publishPrepared(preparedPublication);
	if (!published || published.sequence != 1 || published.payloadBytes == 0 ||
		messages.queued() != 1) return 25;
	const RuntimeMessageDispatchResult dispatched = messages.dispatchPending();
	if (dispatched.messages != 1 || dispatched.delivered != 1 ||
		dispatched.callbackFailures != 0 || sink.received != 1 || sink.sequence != 1 ||
		sink.topic != TacticalWorldDeltaMessageTopic ||
		sink.source != TacticalWorldDeltaMessageSource ||
		sink.decodeResult != TacticalWorldDeltaDecodeResult::Success ||
		sink.delta.events.size() != 3) return 26;

	RuntimeMessageBus chunkMessages(
		2, TacticalWorldDeltaChunkHeaderBytes + 8);
	ExternalChunkedDeltaSink chunkSink;
	if (chunkMessages.addSink(chunkSink) !=
		RuntimeMessageSinkRegistrationError::None) return 47;
	TacticalWorldDeltaPublisher chunkPublisher(
		chunkMessages,
		TacticalWorldDeltaPublishLimits{
			3, TacticalWorldDeltaChunkHeaderBytes + 8, 4096, 128});
	PreparedTacticalWorldDeltaBatch chunkBatch;
	if (chunkPublisher.prepareBatch(*publication.delta, 1, chunkBatch) !=
			TacticalWorldDeltaPublishError::None || !chunkBatch.chunked)
		return 47;
	while (!chunkBatch.complete())
	{
		const TacticalWorldDeltaBatchPublishResult pass =
			chunkPublisher.publishPreparedBatch(chunkBatch);
		if (pass.error != TacticalWorldDeltaPublishError::None &&
			pass.error != TacticalWorldDeltaPublishError::QueueFull)
			return 47;
		chunkMessages.dispatchPending();
	}
	if (chunkSink.completed != 1 ||
		chunkSink.lastResult != TacticalWorldDeltaReassemblyResult::Completed ||
		chunkSink.delta.events.size() != publication.delta->events.size())
		return 47;
	const TacticalWorldPublicationView meaningfulPublication = observer.latest();
	if (observer.update() != TacticalWorldObserverUpdateResult::Unchanged ||
		observer.latest().serial != 2 ||
		observer.latest().snapshot != meaningfulPublication.snapshot ||
		observer.latest().delta != meaningfulPublication.delta) return 46;
	observer.reset();
	if (observer.latest() ||
		observer.update() != TacticalWorldObserverUpdateResult::PublishedBaseline ||
		observer.latest().serial != 1) return 41;

	EngineRuntime<> commandRuntime(EngineHostOptions{}, services);
	const SimulationCommand externalFacing{SetFacingCommand{
		actorId, 2, SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalStealth{SetStealthModeCommand{
		actorId, true, SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalStop{StopMovementCommand{
		actorId, SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalWeaponMode{CycleWeaponModeCommand{
		actorId, SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalScopeMode{CycleScopeModeCommand{
		actorId, TacticalNoTargetGrid, SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalReload{ReloadWeaponCommand{
		actorId, false, SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalTraversal{TraverseObstacleCommand{
		actorId, TacticalTraversalKind::JumpFence,
		SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalActivation{ActivateWorldObjectCommand{
		actorId, TacticalWorldObjectId{1301, 17}, 2,
		SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalApproach{ApproachWorldObjectCommand{
		actorId, TacticalWorldObjectId{1301, 17}, 2,
		1300, 6, true, false, SimulationCommandSource::LocalPlayer}};
	const TacticalEntityId targetId{8, 0x01020305u};
	const SimulationCommand externalConversation{StartConversationCommand{
		actorId, targetId, SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalConversationApproach{
		ApproachConversationCommand{
			actorId, targetId, 1300, 6, false,
			SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalVehicle{EnterVehicleCommand{
		actorId, targetId, 2, 0, SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalVehicleApproach{ApproachVehicleCommand{
		actorId, targetId, 2, 0, 1300, 6, true,
		SimulationCommandSource::LocalPlayer}};
	const TacticalWorldItemId worldItemId{17, 0x01020306u};
	const SimulationCommand externalWorldItemPickup{PickupWorldItemCommand{
		actorId, worldItemId, 1301, 0,
		TacticalWorldItemPickupKind::SpecificItem,
		SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalSteal{StealFromActorCommand{
		actorId, targetId, 1301, 0,
		SimulationCommandSource::LocalPlayer}};
	const SimulationCommand externalExchange{ExchangePositionsCommand{
		actorId, targetId, 1300, 1301, 0,
		SimulationCommandSource::LocalPlayer}};
	if (!std::holds_alternative<SetFacingCommand>(externalFacing) ||
		!std::holds_alternative<SetStealthModeCommand>(externalStealth) ||
		!std::holds_alternative<StopMovementCommand>(externalStop) ||
		!std::holds_alternative<CycleWeaponModeCommand>(externalWeaponMode) ||
		!std::holds_alternative<CycleScopeModeCommand>(externalScopeMode) ||
		!std::holds_alternative<ReloadWeaponCommand>(externalReload) ||
		!std::holds_alternative<TraverseObstacleCommand>(externalTraversal) ||
		!std::holds_alternative<ActivateWorldObjectCommand>(
			externalActivation) ||
		!std::holds_alternative<ApproachWorldObjectCommand>(
			externalApproach) ||
		!std::holds_alternative<StartConversationCommand>(
			externalConversation) ||
		!std::holds_alternative<ApproachConversationCommand>(
			externalConversationApproach) ||
		!std::holds_alternative<EnterVehicleCommand>(externalVehicle) ||
		!std::holds_alternative<ApproachVehicleCommand>(
			externalVehicleApproach) ||
		!std::holds_alternative<PickupWorldItemCommand>(
			externalWorldItemPickup) ||
		!std::holds_alternative<StealFromActorCommand>(externalSteal) ||
		!std::holds_alternative<ExchangePositionsCommand>(externalExchange) ||
		!worldItemId.valid())
		return 45;
	TacticalWorldItemDirectory& externalWorldItems =
		commandRuntime.tacticalWorldItemDirectory();
	const TacticalWorldItemId ownedWorldItem{
		17, externalWorldItems.issueIncarnation()};
	if (!externalWorldItems.activate(ownedWorldItem) ||
		!externalWorldItems.contains(ownedWorldItem) ||
		externalWorldItems.identity(17) != ownedWorldItem ||
		!externalWorldItems.release(ownedWorldItem))
		return 55;
	const std::uint64_t commandSequence = commandRuntime.submitCommand(
		37, MoveToGridCommand{
			actorId, 1300, 6, true, false,
			SimulationCommandSource::LocalPlayer});
	const std::vector<RecordedSimulationCommand> recordedCommands =
		commandRuntime.commandJournal().snapshot();
	if (commandSequence != 0 || recordedCommands.size() != 1 ||
		recordedCommands[0].tick != 37 || recordedCommands[0].sequence != 0 ||
		!std::holds_alternative<MoveToGridCommand>(
			recordedCommands[0].command)) return 27;

	std::vector<std::uint8_t> encodedCommands;
	std::vector<RecordedSimulationCommand> decodedCommands;
	std::uint64_t decodedDroppedCount = 1;
	if (!EncodeSimulationCommandJournal(recordedCommands, 0, encodedCommands) ||
		encodedCommands.empty() ||
		DecodeSimulationCommandJournal(
			encodedCommands, decodedCommands, decodedDroppedCount) !=
				SimulationCommandJournalDecodeResult::Success ||
		decodedDroppedCount != 0 || decodedCommands.size() != 1 ||
		std::get<MoveToGridCommand>(decodedCommands[0].command).destinationGrid != 1300 ||
		!std::get<MoveToGridCommand>(decodedCommands[0].command).reverse ||
		std::get<MoveToGridCommand>(decodedCommands[0].command).origin !=
			TacticalMoveOrigin::PlayerUi ||
		std::get<MoveToGridCommand>(decodedCommands[0].command).pendingAction !=
			TacticalPendingActionPolicy::Clear)
		return 28;

	if (commandRuntime.saveCommandReplay("external.command-replay") !=
		CommandReplaySaveResult::Success) return 29;
	SimulationCommandReplay loadedReplay;
	if (commandRuntime.loadCommandReplay(
		"external.command-replay", loadedReplay) != CommandReplayLoadResult::Success ||
		loadedReplay.droppedCount != 0 || loadedReplay.records.size() != 1)
		return 30;

	EngineRuntime<> replayRuntime(services);
	if (replayRuntime.stageCommandReplay(loadedReplay) !=
		CommandReplayStageResult::Success) return 31;
	const std::vector<ScheduledCommand<SimulationCommand>> replayedCommands =
		replayRuntime.commands().drainThrough(37);
	if (replayedCommands.size() != 1 || replayedCommands[0].sequence != 0 ||
		!std::holds_alternative<MoveToGridCommand>(
			replayedCommands[0].command) ||
		std::get<MoveToGridCommand>(
			replayedCommands[0].command).soldier != actorId)
		return 32;
	return 0;
}
