#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/ByteStorage.h>
#include <Engine/Core/ContentApi.h>
#include <Engine/Core/PackageApi.h>
#include <Engine/Core/PackageSaveArchive.h>
#include <Engine/Core/PersistenceService.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr std::uint32_t PackageSaveArchiveMagic = 0x54534750u;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::fprintf(stderr, "FAIL: %s\n", message);
	std::exit(1);
}

enum class CallbackRandomAction
{
	None,
	Draw,
	DrawAndRestore,
	DrawFromCopy,
	DrawFromAssignedCopy,
	InspectCheckpoint,
	InspectCheckpointAndDrawFromCopy,
	AssignThenDrawAndRestore,
	MoveAssignThenDrawAndRestore,
	MoveConstructFromLiveAndDraw,
	MoveAssignFromLiveAndDraw,
	MismatchedCopyAssignWithoutDraw,
	MismatchedMoveAssignThenDraw,
	MoveConstructFromLiveWithoutDraw,
	RetainCopyWithoutDraw,
	DrawFromRetainedCopy,
	InvalidDraws,
	ThrowWithoutDraw,
	ThrowAfterDraw
};

class PolicyPackage final : public EnginePackage
{
public:
	explicit PolicyPackage(std::string id)
	{
		descriptor_.content.id = std::move(id);
		descriptor_.content.version = "1.0";
		descriptor_.content.requiredApi = ContentApiVersion{1, 0};
		descriptor_.kind = PackageKind::Rules;
		descriptor_.saveStateSchemaVersion = 1;
	}

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override { active_ = true; return true; }
	void deactivate() noexcept override { active_ = false; }
	void simulate(PackageBootstrapContext& context,
		const SimulationTickContext&) override
	{
		if (retainRandomOnSimulation)
		{
			retainedRandom_ =
				std::make_unique<PackageRandomSource>(context.random);
			retainRandomOnSimulation = false;
		}
		RequireDraw(context.random.next("simulation", 1000000));
	}

	bool saveState(PackageBootstrapContext& context,
		std::vector<std::uint8_t>& state) override
	{
		++saveCalls;
		exercise(context.random, saveAction);
		state = {domainState};
		return true;
	}

	bool validateState(PackageBootstrapContext& context, std::uint32_t schema,
		const std::vector<std::uint8_t>& state) override
	{
		++validateCalls;
		exercise(context.random, validateAction);
		return schema == 1 && state.size() == 1;
	}

	bool loadState(PackageBootstrapContext& context, std::uint32_t schema,
		const std::vector<std::uint8_t>& state) override
	{
		++loadCalls;
		exercise(context.random, loadAction);
		if (schema != 1 || state.size() != 1) return false;
		domainState = state.front();
		return true;
	}

	CallbackRandomAction saveAction = CallbackRandomAction::None;
	CallbackRandomAction validateAction = CallbackRandomAction::None;
	CallbackRandomAction loadAction = CallbackRandomAction::None;
	std::uint8_t domainState = 7;
	int saveCalls = 0;
	int validateCalls = 0;
	int loadCalls = 0;
	bool retainRandomOnSimulation = false;
	std::vector<PackageRandomCheckpoint> observedRandom;

	void clearObservations() { observedRandom.clear(); }
	void drawRetainedCopy()
	{
		Check(retainedRandom_ != nullptr,
			"callback retained a live-derived RNG copy");
		RequireDraw(retainedRandom_->next("retained", 1000000));
	}

private:
	static void RequireDraw(PackageRandomResult result)
	{
		if (!result) throw 17;
	}

	void exercise(PackageRandomSource& random, CallbackRandomAction action)
	{
		switch (action)
		{
			case CallbackRandomAction::None:
				return;
			case CallbackRandomAction::Draw:
				RequireDraw(random.next("callback", 1000000));
				return;
			case CallbackRandomAction::DrawAndRestore:
			{
				const PackageRandomCheckpoint before = random.checkpoint();
				RequireDraw(random.next("callback", 1000000));
				if (random.restoreCheckpoint(before) !=
					PackageRandomCheckpointError::None) throw 18;
				return;
			}
			case CallbackRandomAction::DrawFromCopy:
			{
				PackageRandomSource copy(random);
				RequireDraw(copy.next("copy-only", 1000000));
				return;
			}
			case CallbackRandomAction::DrawFromAssignedCopy:
			{
				PackageRandomSource copy(random.packageId(), 0,
					random.maximumStreams());
				copy = random;
				RequireDraw(copy.next("copy-only", 1000000));
				return;
			}
			case CallbackRandomAction::InspectCheckpoint:
				observedRandom.push_back(random.checkpoint());
				return;
			case CallbackRandomAction::InspectCheckpointAndDrawFromCopy:
			{
				observedRandom.push_back(random.checkpoint());
				PackageRandomSource copy(random);
				RequireDraw(copy.next("copy-only", 1000000));
				return;
			}
			case CallbackRandomAction::AssignThenDrawAndRestore:
			{
				const PackageRandomCheckpoint before = random.checkpoint();
				PackageRandomSource replacement(random);
				random = replacement;
				RequireDraw(random.next("callback", 1000000));
				if (random.restoreCheckpoint(before) !=
					PackageRandomCheckpointError::None) throw 19;
				return;
			}
			case CallbackRandomAction::MoveAssignThenDrawAndRestore:
			{
				const PackageRandomCheckpoint before = random.checkpoint();
				PackageRandomSource replacement(random);
				random = std::move(replacement);
				RequireDraw(random.next("callback", 1000000));
				if (random.restoreCheckpoint(before) !=
					PackageRandomCheckpointError::None) throw 23;
				return;
			}
			case CallbackRandomAction::MoveConstructFromLiveAndDraw:
			{
				PackageRandomSource moved(std::move(random));
				RequireDraw(moved.next("moved-copy", 1000000));
				return;
			}
			case CallbackRandomAction::MoveAssignFromLiveAndDraw:
			{
				PackageRandomSource sink(random.packageId(), 0,
					random.maximumStreams());
				sink = std::move(random);
				RequireDraw(sink.next("move-assigned-copy", 1000000));
				return;
			}
			case CallbackRandomAction::MismatchedCopyAssignWithoutDraw:
			{
				PackageRandomSource replacement(random.packageId(), 0,
					random.maximumStreams() + 1);
				random = replacement;
				return;
			}
			case CallbackRandomAction::MismatchedMoveAssignThenDraw:
			{
				PackageRandomSource replacement("rules.assignment-mismatch", 0,
					random.maximumStreams());
				random = std::move(replacement);
				RequireDraw(random.next("callback", 1000000));
				return;
			}
			case CallbackRandomAction::MoveConstructFromLiveWithoutDraw:
			{
				PackageRandomSource moved(std::move(random));
				if (moved.packageId().empty()) throw 24;
				return;
			}
			case CallbackRandomAction::RetainCopyWithoutDraw:
				retainedRandom_ = std::make_unique<PackageRandomSource>(random);
				return;
			case CallbackRandomAction::DrawFromRetainedCopy:
				if (!retainedRandom_) throw 25;
				RequireDraw(retainedRandom_->next("retained", 1000000));
				return;
			case CallbackRandomAction::InvalidDraws:
				if (random.next("invalid/stream", 10).error !=
						PackageRandomError::InvalidStream ||
					random.next("callback", 0).error !=
						PackageRandomError::InvalidUpperBound) throw 20;
				return;
			case CallbackRandomAction::ThrowWithoutDraw:
				throw 21;
			case CallbackRandomAction::ThrowAfterDraw:
				RequireDraw(random.next("callback", 1000000));
				throw 22;
		}
	}

	PackageDescriptor descriptor_{};
	bool active_ = false;
	std::unique_ptr<PackageRandomSource> retainedRandom_;
};

struct RegistryFixture
{
	explicit RegistryFixture(std::uint64_t hostSeed = 0x1122334455667788ULL)
		: package("rules.policy"), content(CurrentContentApiVersion),
		  registry(content, EngineServices::defaults(),
			  NullPackageEventSink::instance(), RuntimeMessageBus::disabled(),
			  ServiceCatalog::disabled(), RuntimeConfiguration::disabled(),
			  hostSeed, 4)
	{
		Check(registry.registerPackage(package) == PackageRegistrationError::None,
			"policy test package registers");
		Check(registry.activate("rules.policy") == PackageActivationError::None,
			"policy test package activates");
		Check(registry.bootstrap(PackageBootstrapPhase::Configure) ==
				PackageBootstrapError::None &&
			registry.bootstrap(PackageBootstrapPhase::LoadContent) ==
				PackageBootstrapError::None &&
			registry.bootstrap(PackageBootstrapPhase::StartRuntime) ==
				PackageBootstrapError::None,
			"policy test registry reaches the save-ready boundary");
	}

	PolicyPackage package;
	ContentRegistry content;
	PackageRegistry registry;
};

PackageSaveStateSnapshot CaptureClean(RegistryFixture& fixture,
	PackageSaveRandomPolicy policy = PackageSaveRandomPolicy::RequireUnconsumed)
{
	fixture.package.saveAction = CallbackRandomAction::None;
	PackageSaveStateCaptureResult captured = fixture.registry.captureSaveState(policy);
	Check(static_cast<bool>(captured) && captured.snapshot.records.size() == 1 &&
		captured.snapshot.engineRecords.size() == 1,
		"clean package state capture succeeds");
	return std::move(captured.snapshot);
}

const PackageRandomCheckpoint& OnlyRandom(
	const PackageSaveStateSnapshot& snapshot)
{
	Check(snapshot.engineRecords.size() == 1,
		"test snapshot contains one engine RNG record");
	return snapshot.engineRecords.front().random;
}

void TestSaveCallbackPolicyAndStickyProbe()
{
	RegistryFixture fixture(0xaabbccddeeff0011ULL);
	Check(fixture.registry.packageRandomHostSeed() == 0xaabbccddeeff0011ULL &&
		fixture.registry.packageRandomStreamLimit() == 4,
		"registry exposes immutable package RNG host configuration");
	const PackageSaveStateSnapshot before = CaptureClean(fixture);

	fixture.package.saveAction = CallbackRandomAction::DrawAndRestore;
	const PackageSaveStateCaptureResult rejected = fixture.registry.captureSaveState(
		PackageSaveRandomPolicy::RequireUnconsumed);
	Check(rejected.error == PackageSaveStateError::RandomConsumed &&
		rejected.packageId == "rules.policy" &&
		rejected.snapshot.records.empty() && rejected.snapshot.engineRecords.empty(),
		"strict capture catches a successful draw even when the callback rewinds RNG");
	const PackageSaveStateSnapshot afterRejected = CaptureClean(fixture);
	Check(OnlyRandom(afterRejected) == OnlyRandom(before),
		"rejected strict capture rolls back RNG and publishes no partial snapshot");

	fixture.package.saveAction = CallbackRandomAction::Draw;
	const PackageSaveStateCaptureResult interactive = fixture.registry.captureSaveState();
	Check(static_cast<bool>(interactive) &&
		OnlyRandom(interactive.snapshot) == OnlyRandom(before),
		"no-argument interactive capture still permits and rolls back callback draws");

	fixture.package.saveAction = CallbackRandomAction::DrawFromCopy;
	const PackageSaveStateCaptureResult copied = fixture.registry.captureSaveState(
		PackageSaveRandomPolicy::RequireUnconsumed);
	Check(copied.error == PackageSaveStateError::RandomConsumed &&
		copied.packageId == "rules.policy",
		"strict capture rejects a draw from a live-derived copied RNG source");
	fixture.package.saveAction = CallbackRandomAction::DrawFromAssignedCopy;
	const PackageSaveStateCaptureResult assignedCopy =
		fixture.registry.captureSaveState(
			PackageSaveRandomPolicy::RequireUnconsumed);
	Check(assignedCopy.error == PackageSaveStateError::RandomConsumed &&
		assignedCopy.packageId == "rules.policy",
		"strict capture rejects a draw from a copy assigned from the live source");
	fixture.package.saveAction = CallbackRandomAction::DrawFromCopy;
	Check(static_cast<bool>(fixture.registry.captureSaveState()) &&
		OnlyRandom(CaptureClean(fixture)) == OnlyRandom(before),
		"interactive capture still permits a draw from a copied RNG value");
	fixture.package.saveAction = CallbackRandomAction::DrawFromAssignedCopy;
	Check(static_cast<bool>(fixture.registry.captureSaveState()) &&
		OnlyRandom(CaptureClean(fixture)) == OnlyRandom(before),
		"interactive capture still permits a draw from an assigned RNG copy");

	fixture.package.saveAction = CallbackRandomAction::AssignThenDrawAndRestore;
	const PackageSaveStateCaptureResult assigned = fixture.registry.captureSaveState(
		PackageSaveRandomPolicy::RequireUnconsumed);
	Check(assigned.error == PackageSaveStateError::RandomConsumed &&
		assigned.packageId == "rules.policy",
		"copy assignment preserves the destination's live probe");
	fixture.package.saveAction = CallbackRandomAction::MoveAssignThenDrawAndRestore;
	const PackageSaveStateCaptureResult moveAssigned =
		fixture.registry.captureSaveState(
			PackageSaveRandomPolicy::RequireUnconsumed);
	Check(moveAssigned.error == PackageSaveStateError::RandomConsumed &&
		moveAssigned.packageId == "rules.policy",
		"move assignment also preserves the destination's live probe");

	fixture.package.saveAction =
		CallbackRandomAction::MismatchedCopyAssignWithoutDraw;
	const PackageSaveStateCaptureResult strictMismatchedCopy =
		fixture.registry.captureSaveState(
			PackageSaveRandomPolicy::RequireUnconsumed);
	Check(static_cast<bool>(strictMismatchedCopy) &&
		OnlyRandom(strictMismatchedCopy.snapshot) == OnlyRandom(before),
		"strict capture treats mismatched copy assignment as a no-op");
	Check(OnlyRandom(CaptureClean(fixture)) == OnlyRandom(before),
		"mismatched copy assignment cannot corrupt live RNG identity or capacity");
	fixture.package.saveAction =
		CallbackRandomAction::MismatchedCopyAssignWithoutDraw;
	const PackageSaveStateCaptureResult interactiveMismatchedCopy =
		fixture.registry.captureSaveState();
	Check(static_cast<bool>(interactiveMismatchedCopy) &&
		OnlyRandom(CaptureClean(fixture)) == OnlyRandom(before),
		"interactive capture also rolls through a mismatched copy no-op safely");

	fixture.package.saveAction = CallbackRandomAction::MismatchedMoveAssignThenDraw;
	const PackageSaveStateCaptureResult strictMismatchedMove =
		fixture.registry.captureSaveState(
			PackageSaveRandomPolicy::RequireUnconsumed);
	Check(strictMismatchedMove.error == PackageSaveStateError::RandomConsumed &&
		strictMismatchedMove.packageId == "rules.policy" &&
		OnlyRandom(CaptureClean(fixture)) == OnlyRandom(before),
		"strict mismatched move is a no-op and the following live draw rolls back");
	fixture.package.saveAction = CallbackRandomAction::MismatchedMoveAssignThenDraw;
	const PackageSaveStateCaptureResult interactiveMismatchedMove =
		fixture.registry.captureSaveState();
	Check(static_cast<bool>(interactiveMismatchedMove) &&
		OnlyRandom(interactiveMismatchedMove.snapshot) == OnlyRandom(before) &&
		OnlyRandom(CaptureClean(fixture)) == OnlyRandom(before),
		"interactive mismatched move plus draw preserves the bound live RNG state");

	fixture.package.saveAction =
		CallbackRandomAction::MoveConstructFromLiveWithoutDraw;
	const PackageSaveStateCaptureResult strictMoveConstruction =
		fixture.registry.captureSaveState(
			PackageSaveRandomPolicy::RequireUnconsumed);
	Check(static_cast<bool>(strictMoveConstruction) &&
		OnlyRandom(CaptureClean(fixture)) == OnlyRandom(before),
		"strict move construction cannot hollow out a registry-bound live source");
	fixture.package.saveAction =
		CallbackRandomAction::MoveConstructFromLiveWithoutDraw;
	const PackageSaveStateCaptureResult interactiveMoveConstruction =
		fixture.registry.captureSaveState();
	Check(static_cast<bool>(interactiveMoveConstruction) &&
		OnlyRandom(CaptureClean(fixture)) == OnlyRandom(before),
		"interactive move construction also preserves live identity and rollback state");

	fixture.package.saveAction =
		CallbackRandomAction::MoveConstructFromLiveAndDraw;
	const PackageSaveStateCaptureResult movedCopyDraw =
		fixture.registry.captureSaveState(
			PackageSaveRandomPolicy::RequireUnconsumed);
	Check(movedCopyDraw.error == PackageSaveStateError::RandomConsumed &&
		movedCopyDraw.packageId == "rules.policy" &&
		OnlyRandom(CaptureClean(fixture)) == OnlyRandom(before),
		"move construction propagates the strict probe without hollowing live RNG");
	fixture.package.saveAction = CallbackRandomAction::MoveAssignFromLiveAndDraw;
	const PackageSaveStateCaptureResult movedAssignedCopyDraw =
		fixture.registry.captureSaveState(
			PackageSaveRandomPolicy::RequireUnconsumed);
	Check(movedAssignedCopyDraw.error == PackageSaveStateError::RandomConsumed &&
		movedAssignedCopyDraw.packageId == "rules.policy" &&
		OnlyRandom(CaptureClean(fixture)) == OnlyRandom(before),
		"move assignment from live propagates the probe and preserves its source");
	fixture.package.saveAction = CallbackRandomAction::MoveAssignFromLiveAndDraw;
	Check(static_cast<bool>(fixture.registry.captureSaveState()) &&
		OnlyRandom(CaptureClean(fixture)) == OnlyRandom(before),
		"interactive move assignment from live remains a rollback-only copy");

	fixture.package.saveAction = CallbackRandomAction::RetainCopyWithoutDraw;
	Check(static_cast<bool>(fixture.registry.captureSaveState(
			PackageSaveRandomPolicy::RequireUnconsumed)),
		"retaining a live-derived copy without drawing is allowed");
	fixture.package.drawRetainedCopy();
	Check(OnlyRandom(CaptureClean(fixture)) == OnlyRandom(before),
		"a retained copy safely outlives its detached callback probe");

	fixture.package.saveAction = CallbackRandomAction::InvalidDraws;
	Check(static_cast<bool>(fixture.registry.captureSaveState(
			PackageSaveRandomPolicy::RequireUnconsumed)),
		"rejected next() requests do not count as random consumption");

	fixture.package.saveAction = CallbackRandomAction::ThrowAfterDraw;
	const PackageSaveStateCaptureResult threwAfterDraw =
		fixture.registry.captureSaveState(
			PackageSaveRandomPolicy::RequireUnconsumed);
	Check(threwAfterDraw.error == PackageSaveStateError::RandomConsumed &&
		threwAfterDraw.packageId == "rules.policy",
		"random consumption remains the exact strict error when a callback then throws");
	fixture.package.saveAction = CallbackRandomAction::ThrowWithoutDraw;
	const PackageSaveStateCaptureResult threwClean = fixture.registry.captureSaveState(
		PackageSaveRandomPolicy::RequireUnconsumed);
	Check(threwClean.error == PackageSaveStateError::CallbackFailed,
		"a throwing callback without a draw keeps the established callback error");
	Check(!CaptureClean(fixture).records.empty(),
		"callback probes detach after every failure and exception exit");
}

void TestValidateAndLoadCallbackPolicy()
{
	RegistryFixture fixture;
	const PackageSaveStateSnapshot saved = CaptureClean(fixture);
	fixture.registry.simulate(SimulationTickContext{1, 1, 1});
	fixture.registry.simulate(SimulationTickContext{2, 1, 2});
	const PackageSaveStateSnapshot preRestore = CaptureClean(fixture);
	Check(!(OnlyRandom(preRestore) == OnlyRandom(saved)),
		"restore policy fixture has destination RNG state newer than its checkpoint");

	fixture.package.validateAction = CallbackRandomAction::DrawAndRestore;
	fixture.package.loadAction = CallbackRandomAction::None;
	const int loadsBeforeValidationFailure = fixture.package.loadCalls;
	const PackageSaveStateLoadResult validateRejected = fixture.registry.restoreSaveState(
		saved, PackageSaveRandomPolicy::RequireUnconsumed);
	Check(validateRejected.error == PackageSaveStateError::RandomConsumed &&
		validateRejected.packageId == "rules.policy" &&
		validateRejected.restored == 0 &&
		fixture.package.loadCalls == loadsBeforeValidationFailure,
		"strict restore catches validateState draw-and-rewind before any load callback");
	fixture.package.validateAction = CallbackRandomAction::None;
	const PackageSaveStateSnapshot afterValidateFailure = CaptureClean(fixture);
	Check(OnlyRandom(afterValidateFailure) == OnlyRandom(preRestore),
		"validateState random rejection restores differing destination RNG state");

	for (const CallbackRandomAction action : {
			CallbackRandomAction::DrawFromCopy,
			CallbackRandomAction::DrawFromAssignedCopy})
	{
		fixture.package.validateAction = action;
		const int loadsBeforeCopyFailure = fixture.package.loadCalls;
		const PackageSaveStateLoadResult copyRejected =
			fixture.registry.restoreSaveState(
				saved, PackageSaveRandomPolicy::RequireUnconsumed);
		Check(copyRejected.error == PackageSaveStateError::RandomConsumed &&
			copyRejected.packageId == "rules.policy" &&
			copyRejected.restored == 0 &&
			fixture.package.loadCalls == loadsBeforeCopyFailure,
			"strict validation rejects draws from copied and assigned RNG values");
		fixture.package.validateAction = CallbackRandomAction::None;
		Check(OnlyRandom(CaptureClean(fixture)) == OnlyRandom(preRestore),
			"copied validation draw rejection rolls back to pre-restore RNG state");
	}

	fixture.package.loadAction = CallbackRandomAction::DrawAndRestore;
	const PackageSaveStateLoadResult loadRejected = fixture.registry.restoreSaveState(
		saved, PackageSaveRandomPolicy::RequireUnconsumed);
	Check(loadRejected.error == PackageSaveStateError::RandomConsumed &&
		loadRejected.packageId == "rules.policy" && loadRejected.restored == 0,
		"strict restore catches loadState draw-and-rewind before RNG publication");
	fixture.package.loadAction = CallbackRandomAction::None;
	const PackageSaveStateSnapshot afterLoadFailure = CaptureClean(fixture);
	Check(OnlyRandom(afterLoadFailure) == OnlyRandom(preRestore),
		"loadState random rejection restores differing destination RNG state");

	for (const CallbackRandomAction action : {
			CallbackRandomAction::DrawFromCopy,
			CallbackRandomAction::DrawFromAssignedCopy})
	{
		fixture.package.loadAction = action;
		const PackageSaveStateLoadResult copyRejected =
			fixture.registry.restoreSaveState(
				saved, PackageSaveRandomPolicy::RequireUnconsumed);
		Check(copyRejected.error == PackageSaveStateError::RandomConsumed &&
			copyRejected.packageId == "rules.policy" && copyRejected.restored == 0,
			"strict load rejects draws from copied and assigned RNG values");
		fixture.package.loadAction = CallbackRandomAction::None;
		Check(OnlyRandom(CaptureClean(fixture)) == OnlyRandom(preRestore),
			"copied load draw rejection rolls back to pre-restore RNG state");
	}

	fixture.package.validateAction = CallbackRandomAction::Draw;
	fixture.package.loadAction = CallbackRandomAction::Draw;
	const PackageSaveStateLoadResult interactive = fixture.registry.restoreSaveState(saved);
	Check(static_cast<bool>(interactive) && interactive.restored == 1 &&
		interactive.engineRecordsRestored == 1,
		"no-argument interactive restore still permits callback RNG use");
	fixture.package.validateAction = CallbackRandomAction::None;
	fixture.package.loadAction = CallbackRandomAction::None;
	const PackageSaveStateSnapshot afterInteractive = CaptureClean(fixture);
	Check(OnlyRandom(afterInteractive) == OnlyRandom(saved),
		"interactive validate/load callback draws remain rollback-only");
}

void TestRetainedSourcesCannotBypassStrictCallbacks()
{
	RegistryFixture fixture;
	const PackageSaveStateSnapshot saved = CaptureClean(fixture);
	fixture.package.retainRandomOnSimulation = true;
	fixture.registry.simulate(SimulationTickContext{1, 1, 1});
	const PackageSaveStateSnapshot preRestore = CaptureClean(fixture);
	Check(!(OnlyRandom(preRestore) == OnlyRandom(saved)),
		"retained-source fixture advances live RNG after cloning it");

	fixture.package.saveAction = CallbackRandomAction::DrawFromRetainedCopy;
	const PackageSaveStateCaptureResult saveRejected =
		fixture.registry.captureSaveState(
			PackageSaveRandomPolicy::RequireUnconsumed);
	Check(saveRejected.error == PackageSaveStateError::RandomConsumed &&
		saveRejected.packageId == "rules.policy" &&
		OnlyRandom(CaptureClean(fixture)) == OnlyRandom(preRestore),
		"strict save catches a draw from a clone retained before the callback");

	fixture.package.validateAction = CallbackRandomAction::DrawFromRetainedCopy;
	const int loadsBeforeValidationFailure = fixture.package.loadCalls;
	const PackageSaveStateLoadResult validationRejected =
		fixture.registry.restoreSaveState(
			saved, PackageSaveRandomPolicy::RequireUnconsumed);
	Check(validationRejected.error == PackageSaveStateError::RandomConsumed &&
		validationRejected.packageId == "rules.policy" &&
		fixture.package.loadCalls == loadsBeforeValidationFailure,
		"strict validation catches a draw from a pre-existing retained clone");
	fixture.package.validateAction = CallbackRandomAction::None;
	Check(OnlyRandom(CaptureClean(fixture)) == OnlyRandom(preRestore),
		"retained validation rejection rolls live RNG back transactionally");

	fixture.package.loadAction = CallbackRandomAction::DrawFromRetainedCopy;
	const PackageSaveStateLoadResult loadRejected = fixture.registry.restoreSaveState(
		saved, PackageSaveRandomPolicy::RequireUnconsumed);
	Check(loadRejected.error == PackageSaveStateError::RandomConsumed &&
		loadRejected.packageId == "rules.policy" && loadRejected.restored == 0,
		"strict load catches a draw from a pre-existing retained clone");
	fixture.package.loadAction = CallbackRandomAction::None;
	Check(OnlyRandom(CaptureClean(fixture)) == OnlyRandom(preRestore),
		"retained load rejection rolls live RNG back transactionally");

	fixture.package.validateAction = CallbackRandomAction::RetainCopyWithoutDraw;
	fixture.package.loadAction = CallbackRandomAction::DrawFromRetainedCopy;
	const PackageSaveStateLoadResult crossPhaseRejected =
		fixture.registry.restoreSaveState(
			saved, PackageSaveRandomPolicy::RequireUnconsumed);
	Check(crossPhaseRejected.error == PackageSaveStateError::RandomConsumed &&
		crossPhaseRejected.packageId == "rules.policy" &&
		crossPhaseRejected.restored == 0,
		"load scope catches a draw from a clone retained during validation");
	fixture.package.validateAction = CallbackRandomAction::None;
	fixture.package.loadAction = CallbackRandomAction::None;
	Check(OnlyRandom(CaptureClean(fixture)) == OnlyRandom(preRestore),
		"cross-phase retained draw rejection preserves pre-restore RNG state");

	fixture.package.validateAction = CallbackRandomAction::RetainCopyWithoutDraw;
	fixture.package.loadAction = CallbackRandomAction::DrawFromRetainedCopy;
	const PackageSaveStateLoadResult interactive =
		fixture.registry.restoreSaveState(saved);
	Check(static_cast<bool>(interactive) && interactive.restored == 1,
		"interactive restore still permits draws from retained RNG values");
	fixture.package.validateAction = CallbackRandomAction::None;
	fixture.package.loadAction = CallbackRandomAction::None;
	Check(OnlyRandom(CaptureClean(fixture)) == OnlyRandom(saved),
		"interactive retained draws do not alter the published persisted RNG");
}

void TestRestoreCallbacksObservePersistedRandomState()
{
	constexpr std::uint64_t hostSeed = 0x33445566778899aaULL;
	RegistryFixture first(hostSeed);
	RegistryFixture second(hostSeed);
	const PackageSaveStateSnapshot saved = CaptureClean(first);
	Check(OnlyRandom(CaptureClean(second)) == OnlyRandom(saved),
		"two restore destinations begin with the same package RNG root");

	first.registry.simulate(SimulationTickContext{1, 1, 1});
	second.registry.simulate(SimulationTickContext{1, 1, 1});
	second.registry.simulate(SimulationTickContext{2, 1, 2});
	const PackageSaveStateSnapshot firstBeforeRestore = CaptureClean(first);
	const PackageSaveStateSnapshot secondBeforeRestore = CaptureClean(second);
	Check(!(OnlyRandom(firstBeforeRestore) == OnlyRandom(secondBeforeRestore)) &&
		!(OnlyRandom(firstBeforeRestore) == OnlyRandom(saved)) &&
		!(OnlyRandom(secondBeforeRestore) == OnlyRandom(saved)),
		"restore destinations have deliberately different newer RNG positions");

	first.package.clearObservations();
	second.package.clearObservations();
	first.package.validateAction =
		CallbackRandomAction::InspectCheckpointAndDrawFromCopy;
	second.package.validateAction =
		CallbackRandomAction::InspectCheckpointAndDrawFromCopy;
	const PackageSaveStateLoadResult firstValidateRejected =
		first.registry.restoreSaveState(
			saved, PackageSaveRandomPolicy::RequireUnconsumed);
	const PackageSaveStateLoadResult secondValidateRejected =
		second.registry.restoreSaveState(
			saved, PackageSaveRandomPolicy::RequireUnconsumed);
	Check(firstValidateRejected.error == PackageSaveStateError::RandomConsumed &&
		secondValidateRejected.error == PackageSaveStateError::RandomConsumed &&
		first.package.observedRandom.size() == 1 &&
		second.package.observedRandom.size() == 1 &&
		first.package.observedRandom.front() == OnlyRandom(saved) &&
		second.package.observedRandom.front() == OnlyRandom(saved),
		"strict validation sees identical persisted RNG and rejects copied draws");
	first.package.validateAction = CallbackRandomAction::None;
	second.package.validateAction = CallbackRandomAction::None;
	Check(OnlyRandom(CaptureClean(first)) == OnlyRandom(firstBeforeRestore) &&
		OnlyRandom(CaptureClean(second)) == OnlyRandom(secondBeforeRestore),
		"rejected staged validation restores each destination's prior RNG state");

	first.package.clearObservations();
	second.package.clearObservations();
	first.package.validateAction = CallbackRandomAction::InspectCheckpoint;
	second.package.validateAction = CallbackRandomAction::InspectCheckpoint;
	first.package.loadAction =
		CallbackRandomAction::InspectCheckpointAndDrawFromCopy;
	second.package.loadAction =
		CallbackRandomAction::InspectCheckpointAndDrawFromCopy;
	const PackageSaveStateLoadResult firstLoadRejected =
		first.registry.restoreSaveState(
			saved, PackageSaveRandomPolicy::RequireUnconsumed);
	const PackageSaveStateLoadResult secondLoadRejected =
		second.registry.restoreSaveState(
			saved, PackageSaveRandomPolicy::RequireUnconsumed);
	Check(firstLoadRejected.error == PackageSaveStateError::RandomConsumed &&
		secondLoadRejected.error == PackageSaveStateError::RandomConsumed &&
		first.package.observedRandom.size() == 2 &&
		second.package.observedRandom.size() == 2 &&
		first.package.observedRandom == second.package.observedRandom &&
		first.package.observedRandom[0] == OnlyRandom(saved) &&
		first.package.observedRandom[1] == OnlyRandom(saved),
		"strict validate and load phases independently see the persisted RNG state");
	first.package.validateAction = CallbackRandomAction::None;
	second.package.validateAction = CallbackRandomAction::None;
	first.package.loadAction = CallbackRandomAction::None;
	second.package.loadAction = CallbackRandomAction::None;
	Check(OnlyRandom(CaptureClean(first)) == OnlyRandom(firstBeforeRestore) &&
		OnlyRandom(CaptureClean(second)) == OnlyRandom(secondBeforeRestore),
		"rejected staged load restores each destination's prior RNG state");

	first.package.clearObservations();
	second.package.clearObservations();
	first.package.validateAction = CallbackRandomAction::InspectCheckpoint;
	second.package.validateAction = CallbackRandomAction::InspectCheckpoint;
	first.package.loadAction =
		CallbackRandomAction::InspectCheckpointAndDrawFromCopy;
	second.package.loadAction =
		CallbackRandomAction::InspectCheckpointAndDrawFromCopy;
	const PackageSaveStateLoadResult firstInteractive =
		first.registry.restoreSaveState(saved);
	const PackageSaveStateLoadResult secondInteractive =
		second.registry.restoreSaveState(saved);
	Check(static_cast<bool>(firstInteractive) &&
		static_cast<bool>(secondInteractive) &&
		first.package.observedRandom.size() == 2 &&
		first.package.observedRandom == second.package.observedRandom &&
		first.package.observedRandom[0] == OnlyRandom(saved) &&
		first.package.observedRandom[1] == OnlyRandom(saved),
		"interactive callbacks also observe identical pristine persisted RNG state");
	first.package.validateAction = CallbackRandomAction::None;
	second.package.validateAction = CallbackRandomAction::None;
	first.package.loadAction = CallbackRandomAction::None;
	second.package.loadAction = CallbackRandomAction::None;
	Check(OnlyRandom(CaptureClean(first)) == OnlyRandom(saved) &&
		OnlyRandom(CaptureClean(second)) == OnlyRandom(saved),
		"successful interactive restore publishes pristine persisted RNG state");
}

void TestStrictLiveRootContract()
{
	RegistryFixture fixture;
	const PackageSaveStateSnapshot saved = CaptureClean(fixture);
	Check(OnlyRandom(saved).schema == PackageRandomCheckpoint::CurrentSchema,
		"strict root fixture uses a complete schema-2 checkpoint");

	PackageSaveStateSnapshot mismatched = saved;
	mismatched.engineRecords.front().random.rootSeed ^= 0x8000000000000000ULL;
	const PackageSaveStateLoadResult strictValidation =
		fixture.registry.validateSaveState(
			mismatched, PackageSaveRandomPolicy::RequireUnconsumed);
	Check(strictValidation.error == PackageSaveStateError::EngineStateMismatch &&
		strictValidation.packageId == "rules.policy",
		"strict structural validation rejects a schema-2 live-root mismatch");
	Check(static_cast<bool>(fixture.registry.validateSaveState(mismatched)),
		"interactive structural validation retains schema-2 root adoption semantics");

	PackageSaveStateSnapshot legacy = saved;
	legacy.engineRecords.front().random.schema =
		PackageRandomCheckpoint::LegacySchema;
	legacy.engineRecords.front().random.rootSeed = 0;
	legacy.engineRecords.front().random.maximumStreams = 0;
	Check(static_cast<bool>(fixture.registry.validateSaveState(
			legacy, PackageSaveRandomPolicy::RequireUnconsumed)),
		"strict callback policy accepts schema-1 state that retains the matched live root");

	PackageRandomSource live("rules.live-contract", 77, 3);
	const PackageRandomCheckpoint matching = live.checkpoint();
	PackageRandomCheckpoint wrongRoot = matching;
	wrongRoot.rootSeed ^= 1;
	Check(live.matchesLiveRootAndConfiguration(matching) &&
		!live.matchesLiveRootAndConfiguration(wrongRoot),
		"read-only package RNG API compares schema, identity, root, and stream config");
}

std::vector<std::uint8_t> EncodeLegacyArchive(
	PersistenceService& persistence,
	RuntimeCompatibilityFingerprint compatibility)
{
	BinaryWriter payload;
	payload.writeU32(compatibility.schema);
	payload.writeU64(compatibility.high);
	payload.writeU64(compatibility.low);
	payload.writeU32(0);
	payload.writeU32(1);
	payload.writeString("rules.archive");
	payload.writeString("1.0");
	payload.writeU32(PackageRandomCheckpoint::LegacySchema);
	payload.writeU32(0);
	std::vector<std::uint8_t> encoded;
	Check(persistence.encodeEnvelope(
		PersistenceHeader{PackageSaveArchiveMagic, 3}, payload.bytes(), encoded) ==
		PersistenceSaveResult::Success,
		"PGST v3 stored-version fixture encodes");
	return encoded;
}

void TestArchiveReportsStoredVersion()
{
	MemoryByteStorage storage;
	PersistenceService persistence(storage, 4096);
	PackageSaveArchiveService archives(persistence, 2, 1024, 4096, 4);
	const RuntimeCompatibilityFingerprint compatibility{1, 7, 9};
	PackageRandomSource random("rules.archive", 55, 4);
	PackageSaveArchive archive{compatibility, {}};
	archive.state.engineRecords.push_back(
		{"rules.archive", "1.0", random.checkpoint()});
	std::vector<std::uint8_t> encoded;
	Check(archives.encode(archive, encoded) == PackageSaveArchiveSaveError::None,
		"PGST v4 stored-version fixture encodes");
	PackageSaveArchive decoded;
	const PackageSaveArchiveLoadResult current =
		archives.decode(encoded, compatibility, decoded);
	Check(static_cast<bool>(current) && current.storedVersion == 4,
		"successful PGST v4 decode reports stored version 4");
	const PackageSaveArchiveLoadResult incompatible = archives.decode(
		encoded, RuntimeCompatibilityFingerprint{1, 8, 9}, decoded);
	Check(incompatible.error == PackageSaveArchiveLoadError::IncompatibleRuntime &&
		incompatible.storedVersion == 4,
		"semantic PGST rejection still reports its integrity-checked stored version");

	const std::vector<std::uint8_t> encodedLegacy =
		EncodeLegacyArchive(persistence, compatibility);
	PackageSaveArchive decodedLegacy;
	const PackageSaveArchiveLoadResult legacy =
		archives.decode(encodedLegacy, compatibility, decodedLegacy);
	Check(static_cast<bool>(legacy) && legacy.storedVersion == 3 &&
		decodedLegacy.state.engineRecords.front().random.schema ==
			PackageRandomCheckpoint::LegacySchema,
		"successful compatibility decode reports stored PGST version 3");
}
}

int main()
{
	TestSaveCallbackPolicyAndStickyProbe();
	TestValidateAndLoadCallbackPolicy();
	TestRetainedSourcesCannotBypassStrictCallbacks();
	TestRestoreCallbacksObservePersistedRandomState();
	TestStrictLiveRootContract();
	TestArchiveReportsStoredVersion();
	std::puts("package save RNG policy tests passed");
	return 0;
}
