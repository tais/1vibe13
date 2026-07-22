#include <Engine/Adapters/JA2/CommandReplay.h>
#include <Engine/Adapters/JA2/EngineRuntime.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Adapters/JA2/TacticalCommandService.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaCodec.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaPublisher.h>
#include <Engine/Adapters/JA2/TacticalWorldObserver.h>
#include <Engine/Core/EngineHost.h>

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
			issuedIdentity_ = context.identity;
		return true;
	}
	const PackageIdentity& issuedIdentity() const { return issuedIdentity_; }
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
	bool active_ = false;
};

int main()
{
	MemoryByteStorage storage;
	EngineServices services{
		ZeroTimeSource::instance(), ZeroRandomSource::instance(), storage};
	RuntimeCapabilities hostCapabilities;
	if (!hostCapabilities.add("host.external-consumer")) return 1;
	EngineHost<> host(services, CurrentContentApiVersion,
		NullPackageEventSink::instance(), std::move(hostCapabilities));
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
	if (!host.screens().current() || host.screens().current()->state != 7 ||
		!host.beginInitialization() ||
		!host.runtimeSession().advancePackagesTo(PackageBootstrapPhase::StartRuntime) ||
		!package.issuedIdentity() || package.issuedIdentity().id() != "external.rules" ||
		!host.markRunning()) return 5;
	const PackageSaveStateCaptureResult capturedExternalState =
		host.capturePackageSaveState();
	if (!capturedExternalState || capturedExternalState.snapshot.records.size() != 1 ||
		capturedExternalState.snapshot.records[0].payload !=
			std::vector<std::uint8_t>({1, 2, 3}) ||
		!host.validatePackageSaveState(capturedExternalState.snapshot) ||
		!host.restorePackageSaveState(capturedExternalState.snapshot)) return 14;
	if (!host.beginShutdown() || !host.runtimeSession().shutdownPackages() ||
		!host.markStopped()) return 5;
	const EngineServiceLookupResult<unsigned> resolved =
		host.serviceCatalog().resolve<unsigned>(
			"external.test-service", EngineServiceVersion{1, 1});
	if (!resolved || resolved.service != &externalService ||
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
	const TacticalWorldDeltaPublishResult published = publisher.publish(*publication.delta);
	if (!published || published.sequence != 1 || published.payloadBytes == 0 ||
		messages.queued() != 1) return 25;
	const RuntimeMessageDispatchResult dispatched = messages.dispatchPending();
	if (dispatched.messages != 1 || dispatched.delivered != 1 ||
		dispatched.callbackFailures != 0 || sink.received != 1 || sink.sequence != 1 ||
		sink.topic != TacticalWorldDeltaMessageTopic ||
		sink.source != TacticalWorldDeltaMessageSource ||
		sink.decodeResult != TacticalWorldDeltaDecodeResult::Success ||
		sink.delta.events.size() != 3) return 26;

	EngineRuntime<> commandRuntime(services);
	const std::uint64_t commandSequence = commandRuntime.submitCommand(
		37, BeginFireWeaponCommand{
			actorId, 1300, 0, 1, SimulationCommandSource::LocalPlayer});
	const std::vector<RecordedSimulationCommand> recordedCommands =
		commandRuntime.commandJournal().snapshot();
	if (commandSequence != 0 || recordedCommands.size() != 1 ||
		recordedCommands[0].tick != 37 || recordedCommands[0].sequence != 0 ||
		!std::holds_alternative<BeginFireWeaponCommand>(
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
		std::get<BeginFireWeaponCommand>(decodedCommands[0].command).targetGrid != 1300)
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
		!std::holds_alternative<BeginFireWeaponCommand>(
			replayedCommands[0].command) ||
		std::get<BeginFireWeaponCommand>(
			replayedCommands[0].command).soldier != actorId)
		return 32;

	TacticalCommandInbox commandInbox(
		TacticalCommandInboxLimits{2, 1, 1, 64, 10});
	ServiceCatalog commandServices;
	if (RegisterTacticalCommandService(commandServices, commandInbox) !=
		EngineServiceRegistrationError::None) return 33;
	const auto commandIngress = commandServices.resolve(TacticalCommandServiceContract);
	const TacticalCommandSubmissionResult commandRequest = commandIngress
		? commandIngress.service->submit(
			"external.rules", SimulationCommand{ChangeStanceCommand{
				actorId, 3, SimulationCommandSource::System}})
		: TacticalCommandSubmissionResult{
			TacticalCommandSubmissionError::InvalidCommand, 0};
	std::uint64_t drainedRequest = 0;
	const TacticalCommandDrainResult commandDrain = commandInbox.drain(
		[&drainedRequest](const TacticalCommandRequest& request) {
			drainedRequest = request.requestId;
			return TacticalCommandDisposition::Accept;
		});
	if (!commandIngress || !commandRequest || commandRequest.requestId != 1 ||
		commandDrain.accepted != 1 || drainedRequest != commandRequest.requestId ||
		!commandInbox.empty()) return 34;
	return 0;
}
