#include <Engine/Adapters/JA2/CommandReplay.h>
#include <Engine/Adapters/JA2/EngineRuntime.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Adapters/JA2/TacticalCommandService.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaCodec.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaPublisher.h>
#include <Engine/Adapters/JA2/TacticalWorldObserver.h>
#include <Engine/Core/EngineHost.h>
#include <Engine/Core/EngineHostOptions.h>
#include <Engine/Core/EngineServiceContracts.h>
#include <Engine/Core/ServiceCatalog.h>

#include <cstddef>
#include <cstdint>
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

	MemoryByteStorage storage;
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
		!packagesStarted || !package.issuedIdentity() ||
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
	if (!std::holds_alternative<SetFacingCommand>(externalFacing) ||
		!std::holds_alternative<SetStealthModeCommand>(externalStealth) ||
		!std::holds_alternative<StopMovementCommand>(externalStop)) return 45;
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
