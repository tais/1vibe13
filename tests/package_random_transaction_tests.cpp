#include <Engine/Core/ContentApi.h>
#include <Engine/Core/PackageApi.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
bool RejectAllocations = false;
}

void* operator new(std::size_t size)
{
	if (RejectAllocations) throw std::bad_alloc();
	if (void* allocation = std::malloc(size)) return allocation;
	throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
	return ::operator new(size);
}

void operator delete(void* allocation) noexcept
{
	std::free(allocation);
}

void operator delete[](void* allocation) noexcept
{
	std::free(allocation);
}

void operator delete(void* allocation, std::size_t) noexcept
{
	std::free(allocation);
}

void operator delete[](void* allocation, std::size_t) noexcept
{
	std::free(allocation);
}

namespace
{
constexpr std::uint64_t HostSeed = 0x123456789abcdef0ULL;
constexpr std::size_t StreamLimit = 8;

template<typename Condition>
void Check(const Condition& condition, const char* message)
{
	if (static_cast<bool>(condition)) return;
	std::fprintf(stderr, "FAIL: %s\n", message);
	std::exit(1);
}

class TransactionPackage final : public EnginePackage
{
public:
	TransactionPackage(std::string id, std::uint32_t saveSchema)
	{
		descriptor_.content.id = std::move(id);
		descriptor_.content.version = "1.0";
		descriptor_.content.requiredApi = ContentApiVersion{1, 0};
		descriptor_.kind = PackageKind::Rules;
		descriptor_.saveStateSchemaVersion = saveSchema;
	}

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override { return true; }
	void deactivate() noexcept override {}

	bool bootstrap(PackageBootstrapContext& context,
		PackageBootstrapPhase phase) override
	{
		if (phase == PackageBootstrapPhase::StartRuntime)
		{
			liveRandom = &context.random;
			retainedRandom = std::make_unique<PackageRandomSource>(context.random);
		}
		return true;
	}

	void simulate(PackageBootstrapContext& context,
		const SimulationTickContext&) override
	{
		++simulationCalls;
		if (!context.random.next("simulation", 1000000)) throw 1;
	}

	bool saveState(PackageBootstrapContext&,
		std::vector<std::uint8_t>& state) override
	{
		++saveCalls;
		if (registryDuringSave != nullptr)
		{
			PackageRandomTransaction nested =
				registryDuringSave->beginRandomTransaction();
			nestedBeginError = nested.beginResult().error;
		}
		state = {domainState};
		return true;
	}

	bool validateState(PackageBootstrapContext&, std::uint32_t schema,
		const std::vector<std::uint8_t>& state) override
	{
		return schema == descriptor_.saveStateSchemaVersion && state.size() == 1;
	}

	bool loadState(PackageBootstrapContext&, std::uint32_t schema,
		const std::vector<std::uint8_t>& state) override
	{
		if (schema != descriptor_.saveStateSchemaVersion || state.size() != 1)
			return false;
		domainState = state.front();
		return true;
	}

	PackageRandomSource* liveRandom = nullptr;
	std::unique_ptr<PackageRandomSource> retainedRandom;
	PackageRegistry* registryDuringSave = nullptr;
	PackageRandomTransactionError nestedBeginError =
		PackageRandomTransactionError::None;
	std::uint8_t domainState = 7;
	std::size_t saveCalls = 0;
	std::size_t simulationCalls = 0;

private:
	PackageDescriptor descriptor_{};
};

struct RegistryFixture
{
	explicit RegistryFixture(bool secondStateful = false,
		bool completeBootstrap = true)
		: content(CurrentContentApiVersion),
		  first("rules.first", 1),
		  second("rules.second", secondStateful ? 1u : 0u),
		  late("rules.late", 0),
		  registry(content, EngineServices::defaults(),
			  NullPackageEventSink::instance(), RuntimeMessageBus::disabled(),
			  ServiceCatalog::disabled(), RuntimeConfiguration::disabled(),
			  HostSeed, StreamLimit)
	{
		Check(registry.registerPackage(first) == PackageRegistrationError::None &&
			registry.registerPackage(second) == PackageRegistrationError::None,
			"transaction packages register");
		const PackageActivationResult activated = registry.activateAll(
			std::vector<std::string>{"rules.first", "rules.second"});
		Check(static_cast<bool>(activated) && activated.activated.size() == 2,
			"transaction packages activate in requested order");
		if (!completeBootstrap) return;
		Check(registry.bootstrap(PackageBootstrapPhase::Configure) ==
				PackageBootstrapError::None &&
			registry.bootstrap(PackageBootstrapPhase::LoadContent) ==
				PackageBootstrapError::None &&
			registry.bootstrap(PackageBootstrapPhase::StartRuntime) ==
				PackageBootstrapError::None,
			"transaction registry reaches runtime-ready boundary");
		Check(first.liveRandom != nullptr && second.liveRandom != nullptr &&
			first.retainedRandom && second.retainedRandom,
			"bootstrap exposes live and directly derived RNG sources");
	}

	ContentRegistry content;
	TransactionPackage first;
	TransactionPackage second;
	TransactionPackage late;
	PackageRegistry registry;
};

void TestBeginContractAndStatefulQuery()
{
	static_assert(!std::is_copy_constructible<PackageRandomTransaction>::value &&
		!std::is_copy_assignable<PackageRandomTransaction>::value,
		"package RNG transactions are uniquely owned");
	static_assert(std::is_nothrow_move_constructible<
		PackageRandomTransaction>::value &&
		std::is_nothrow_move_assignable<PackageRandomTransaction>::value,
		"package RNG transaction ownership transfers without throwing");

	RegistryFixture unready(false, false);
	PackageRandomTransaction rejected =
		unready.registry.beginRandomTransaction();
	Check(!rejected && rejected.beginResult().error ==
		PackageRandomTransactionError::RuntimeNotReady,
		"transaction begin rejects a registry before full bootstrap");

	RegistryFixture fixture;
	const std::size_t savesBefore = fixture.first.saveCalls;
	Check(fixture.registry.hasActiveStatefulSaveState() &&
		fixture.first.saveCalls == savesBefore,
		"stateful package query is callback-free and fail-closed");

	PackageRandomTransaction transaction =
		fixture.registry.beginRandomTransaction();
	Check(static_cast<bool>(transaction) && transaction.beginResult() &&
		transaction.baseline().engineRecords.size() == 2 &&
		transaction.baseline().engineRecords[0].packageId == "rules.first" &&
		transaction.baseline().engineRecords[1].packageId == "rules.second" &&
		transaction.baseline().engineRecords[0].packageVersion == "1.0",
		"begin captures an exact activation-order engine stamp");

	PackageRandomTransaction nested =
		fixture.registry.beginRandomTransaction();
	Check(!nested && nested.beginResult().error ==
		PackageRandomTransactionError::TransactionAlreadyActive,
		"a registry admits only one package RNG transaction");
	Check(transaction.rollback(), "explicit rollback closes a clean transaction");

	// A fresh registry verifies the all-stateless fast path.
	ContentRegistry content(CurrentContentApiVersion);
	TransactionPackage one("rules.stateless-one", 0);
	TransactionPackage two("rules.stateless-two", 0);
	PackageRegistry registry(content);
	Check(registry.registerPackage(one) == PackageRegistrationError::None &&
		registry.registerPackage(two) == PackageRegistrationError::None &&
		registry.activateAll(std::vector<std::string>{
			"rules.stateless-one", "rules.stateless-two"}),
		"stateless query fixture activates");
	Check(!registry.hasActiveStatefulSaveState(),
		"all-stateless active set is reported without callbacks");
}

void TestOwnerOperationsAndDispatchFreeze()
{
	RegistryFixture fixture;
	fixture.first.registryDuringSave = &fixture.registry;
	const PackageSaveStateCaptureResult before = fixture.registry.captureSaveState(
		PackageSaveRandomPolicy::RequireUnconsumed);
	Check(before && fixture.first.nestedBeginError ==
		PackageRandomTransactionError::OperationInProgress,
		"begin reports an in-progress package callback transaction");
	fixture.first.registryDuringSave = nullptr;

	PackageRandomTransaction transaction =
		fixture.registry.beginRandomTransaction();
	const std::size_t simulationsBefore = fixture.first.simulationCalls;
	fixture.registry.simulate(SimulationTickContext{1, 1, 1});
	Check(fixture.first.simulationCalls == simulationsBefore,
		"runtime dispatch is frozen while a random transaction is armed");
	Check(fixture.registry.registerPackage(fixture.late) ==
		PackageRegistrationError::OperationInProgress,
		"package registration is frozen while a random transaction is armed");
	Check(fixture.registry.shutdownBootstrap().error ==
		PackageBootstrapShutdownError::OperationInProgress,
		"bootstrap lifecycle mutation is frozen while a transaction is armed");

	const PackageSaveStateCaptureResult captured = fixture.registry.captureSaveState(
		PackageSaveRandomPolicy::RequireUnconsumed);
	Check(captured && captured.snapshot.engineRecords ==
		transaction.baseline().engineRecords,
		"owning save capture remains available inside the random transaction");
	Check(transaction.commitUnchanged(),
		"unchanged save capture commits and unfreezes the registry");
	Check(fixture.registry.registerPackage(fixture.late) ==
		PackageRegistrationError::None,
		"lifecycle mutation resumes after commit");
	fixture.registry.simulate(SimulationTickContext{2, 1, 2});
	Check(fixture.first.simulationCalls == simulationsBefore + 1,
		"runtime dispatch resumes after commit");
}

void TestEpochDetectsDerivedDrawAndRollbackNeverRewindsIt()
{
	RegistryFixture fixture;
	PackageRandomTransaction transaction =
		fixture.registry.beginRandomTransaction();
	const PackageRandomCheckpoint liveBefore =
		transaction.baseline().engineRecords.front().random;
	const PackageRandomCheckpoint retainedBefore =
		fixture.first.retainedRandom->checkpoint();
	Check(fixture.first.retainedRandom->next("retained", 1000000),
		"live-derived retained source draws successfully");
	Check(fixture.first.retainedRandom->restoreCheckpoint(retainedBefore) ==
		PackageRandomCheckpointError::None,
		"retained source can rewind deterministic state");

	const PackageRandomTransactionResult rejected =
		transaction.commitUnchanged();
	Check(rejected.error == PackageRandomTransactionError::RandomConsumed,
		"non-rewindable aggregate epoch detects draw-and-restore on a derived source");
	PackageRandomTransaction stillActive =
		fixture.registry.beginRandomTransaction();
	Check(stillActive.beginResult().error ==
		PackageRandomTransactionError::TransactionAlreadyActive,
		"failed commit leaves rollback ownership armed");
	const std::uint64_t consumedEpoch =
		transaction.baseline().consumptionEpoch + 1;
	Check(transaction.rollback(), "failed transaction rolls live sources back");
	Check(fixture.first.liveRandom->checkpoint() == liveBefore,
		"rollback restores the exact deterministic live checkpoint");

	PackageRandomTransaction after = fixture.registry.beginRandomTransaction();
	Check(after && after.baseline().consumptionEpoch == consumedEpoch &&
		after.baseline().engineRecords.front().random == liveBefore,
		"rollback restores stream state but never rewinds consumption evidence");
	Check(after.commitUnchanged(), "post-rollback baseline commits normally");
}

void TestCommitRechecksEpochAtPublication()
{
	RegistryFixture fixture;
	PackageRandomTransaction transaction =
		fixture.registry.beginRandomTransaction();
	const PackageRandomCheckpoint retainedBefore =
		fixture.first.retainedRandom->checkpoint();
	Check(fixture.first.retainedRandom->next("publication-race", 1000000) &&
		fixture.first.retainedRandom->restoreCheckpoint(retainedBefore) ==
			PackageRandomCheckpointError::None,
		"publication fixture changes only the shared epoch");
	Check(transaction.commitUnchanged().error ==
		PackageRandomTransactionError::RandomConsumed,
		"commit publication rejects retained consumption despite exact live state");
	Check(transaction.rollback(),
		"publication-epoch rejection remains explicitly rollbackable");
}

void TestInvalidAndUnboundDrawsDoNotTripRegistryEvidence()
{
	RegistryFixture fixture;
	PackageRandomTransaction invalid = fixture.registry.beginRandomTransaction();
	Check(fixture.first.liveRandom->next("invalid/stream", 10).error ==
			PackageRandomError::InvalidStream &&
		fixture.first.liveRandom->next("valid", 0).error ==
			PackageRandomError::InvalidUpperBound,
		"invalid live requests are rejected");
	Check(invalid.commitUnchanged(),
		"invalid requests do not advance the transaction epoch");

	PackageRandomTransaction unbound = fixture.registry.beginRandomTransaction();
	PackageRandomSource standalone("rules.first", HostSeed, StreamLimit);
	Check(standalone.next("standalone", 1000000),
		"independently constructed package RNG draws");
	Check(unbound.commitUnchanged(),
		"an unbound standalone source does not forge registry evidence");

	PackageRandomTransaction assigned = fixture.registry.beginRandomTransaction();
	standalone = *fixture.first.liveRandom;
	const PackageRandomCheckpoint before = standalone.checkpoint();
	Check(standalone.next("assigned", 1000000) &&
		standalone.restoreCheckpoint(before) == PackageRandomCheckpointError::None,
		"an unbound destination adopts a live source and draws then rewinds");
	Check(assigned.commitUnchanged().error ==
		PackageRandomTransactionError::RandomConsumed,
		"assignment from a live source preserves registry transaction provenance");
	Check(assigned.rollback(), "assigned-copy rejection remains rollbackable");
}

void TestOwnerRestoreAndTargetCommit()
{
	RegistryFixture fixture;
	const PackageSaveStateCaptureResult saved = fixture.registry.captureSaveState(
		PackageSaveRandomPolicy::RequireUnconsumed);
	Check(saved && saved.snapshot.engineRecords.size() == 2,
		"owner restore fixture captures an activation-ordered target");
	fixture.registry.simulate(SimulationTickContext{1, 1, 1});
	Check(!(fixture.first.liveRandom->checkpoint() ==
		saved.snapshot.engineRecords.front().random),
		"owner restore fixture advances beyond the saved target");

	PackageRandomTransaction transaction = fixture.registry.beginRandomTransaction();
	const PackageSaveStateLoadResult restored = fixture.registry.restoreSaveState(
		saved.snapshot, PackageSaveRandomPolicy::RequireUnconsumed);
	Check(restored && restored.engineRecordsRestored == 2,
		"owning restore remains available inside the random transaction");
	Check(transaction.commitTarget(saved.snapshot.engineRecords),
		"transaction commits the exact package RNG state published by restore");

	PackageRandomTransaction after = fixture.registry.beginRandomTransaction();
	Check(after.baseline().engineRecords == saved.snapshot.engineRecords,
		"restored package RNG target persists after commit");
	Check(after.commitUnchanged(),
		"registry remains usable after owner restore target commit");
}

PackageRandomCheckpoint AdvancedCheckpoint(
	const PackageRandomCheckpoint& baseline, const std::string& packageId)
{
	PackageRandomSource source(packageId, HostSeed, StreamLimit);
	Check(source.restoreCheckpoint(baseline) ==
			PackageRandomCheckpointError::None &&
		source.next("target", 1000000),
		"target checkpoint fixture advances a compatible standalone source");
	return source.checkpoint();
}

void TestStateComparisonTargetCommitAndMoveOwnership()
{
	RegistryFixture fixture;
	PackageRandomTransaction original = fixture.registry.beginRandomTransaction();
	PackageRandomTransaction transaction(std::move(original));
	Check(!original && transaction,
		"move construction transfers unique rollback ownership");

	std::vector<PackageEngineSaveStateRecord> target =
		transaction.baseline().engineRecords;
	target.front().random = AdvancedCheckpoint(
		target.front().random, target.front().packageId);
	Check(fixture.first.liveRandom->restoreCheckpoint(target.front().random) ==
		PackageRandomCheckpointError::None,
		"load path publishes the preflighted target without consuming RNG");
	Check(transaction.commitUnchanged().error ==
		PackageRandomTransactionError::StateChanged,
		"unchanged commit rejects a deterministic state replacement");
	Check(transaction.commitTarget(target),
		"target commit accepts the exact activation-order published state");

	PackageRandomTransaction persisted = fixture.registry.beginRandomTransaction();
	Check(persisted.baseline().engineRecords == target,
		"successful target commit preserves the published RNG state");
	Check(persisted.commitUnchanged(), "published target becomes the next baseline");

	PackageRandomTransaction mismatch = fixture.registry.beginRandomTransaction();
	std::vector<PackageEngineSaveStateRecord> wrong =
		mismatch.baseline().engineRecords;
	wrong.front().packageVersion = "2.0";
	Check(mismatch.commitTarget(wrong).error ==
		PackageRandomTransactionError::VersionMismatch,
		"target commit distinguishes package version mismatch");
	wrong = mismatch.baseline().engineRecords;
	wrong.front().packageId = "rules.other";
	Check(mismatch.commitTarget(wrong).error ==
		PackageRandomTransactionError::IdentityMismatch,
		"target commit distinguishes activation identity mismatch");
	wrong = mismatch.baseline().engineRecords;
	wrong.front().random.schema = 999;
	Check(mismatch.commitTarget(wrong).error ==
		PackageRandomTransactionError::InvalidCheckpoint,
		"target commit distinguishes invalid checkpoint structure");
	wrong = mismatch.baseline().engineRecords;
	wrong.front().random.schema = PackageRandomCheckpoint::LegacySchema;
	wrong.front().random.rootSeed = 0;
	wrong.front().random.maximumStreams = 0;
	Check(mismatch.commitTarget(wrong).error ==
		PackageRandomTransactionError::InvalidCheckpoint,
		"target commit cannot publish an incomplete legacy checkpoint");
	wrong = mismatch.baseline().engineRecords;
	++wrong.front().random.maximumStreams;
	Check(mismatch.commitTarget(wrong).error ==
		PackageRandomTransactionError::InvalidCheckpoint,
		"target commit cannot change the package stream limit");
	wrong = mismatch.baseline().engineRecords;
	wrong.front().random.rootSeed ^= 0x8000000000000000ULL;
	Check(fixture.first.liveRandom->validateCheckpoint(wrong.front().random) ==
			PackageRandomCheckpointError::None &&
		fixture.first.liveRandom->restoreCheckpoint(wrong.front().random) ==
			PackageRandomCheckpointError::None,
		"different-root checkpoint is structurally valid and directly adoptable");
	Check(mismatch.commitTarget(wrong).error ==
		PackageRandomTransactionError::InvalidCheckpoint,
		"target commit independently rejects host-root adoption bypass");
	Check(mismatch.rollback(), "target diagnostic failures remain rollbackable");
	Check(fixture.first.liveRandom->checkpoint() ==
		mismatch.baseline().engineRecords.front().random,
		"root-adoption rejection rolls the live source back to its baseline root");
}

void TestAllocationFailureRollbackAndEscapedLifetime()
{
	RegistryFixture fixture;
	RejectAllocations = true;
	PackageRandomTransaction failed = fixture.registry.beginRandomTransaction();
	RejectAllocations = false;
	Check(failed.beginResult().error ==
		PackageRandomTransactionError::AllocationFailure,
		"begin translates snapshot allocation failure without arming the registry");

	PackageRandomTransaction transaction = fixture.registry.beginRandomTransaction();
	const PackageRandomTransactionStamp rollbackBaseline = transaction.baseline();
	Check(fixture.first.liveRandom->next("changed", 1000000),
		"rollback allocation fixture changes live RNG state");
	RejectAllocations = true;
	const PackageRandomTransactionResult rolledBack = transaction.rollback();
	RejectAllocations = false;
	Check(rolledBack &&
		fixture.first.liveRandom->checkpoint() ==
			rollbackBaseline.engineRecords[0].random &&
		fixture.second.liveRandom->checkpoint() ==
			rollbackBaseline.engineRecords[1].random,
		"preallocated rollback completes while all allocations are rejected");
	PackageRandomTransaction afterRollback =
		fixture.registry.beginRandomTransaction();
	Check(afterRollback &&
		afterRollback.baseline().consumptionEpoch ==
			rollbackBaseline.consumptionEpoch + 1 &&
		afterRollback.baseline().engineRecords == rollbackBaseline.engineRecords,
		"allocation-free rollback restores both live streams but never the epoch");
	Check(afterRollback.commitUnchanged(),
		"registry remains usable after allocation-free rollback");

	PackageRandomTransaction escaped;
	{
		auto content = std::make_unique<ContentRegistry>(CurrentContentApiVersion);
		auto package = std::make_unique<TransactionPackage>("rules.escape", 0);
		auto registry = std::make_unique<PackageRegistry>(*content,
			EngineServices::defaults(), NullPackageEventSink::instance(),
			RuntimeMessageBus::disabled(), ServiceCatalog::disabled(),
			RuntimeConfiguration::disabled(), HostSeed, StreamLimit);
		Check(registry->registerPackage(*package) == PackageRegistrationError::None &&
			registry->activate("rules.escape") == PackageActivationError::None &&
			registry->bootstrap(PackageBootstrapPhase::Configure) ==
				PackageBootstrapError::None &&
			registry->bootstrap(PackageBootstrapPhase::LoadContent) ==
				PackageBootstrapError::None &&
			registry->bootstrap(PackageBootstrapPhase::StartRuntime) ==
				PackageBootstrapError::None,
			"escaped lifetime fixture reaches runtime-ready state");
		escaped = registry->beginRandomTransaction();
		Check(escaped, "escaped transaction begins");
		registry.reset();
		Check(!escaped,
			"destroying the registry invalidates escaped transaction ownership");
	}
	Check(escaped.rollback().error ==
		PackageRandomTransactionError::InvariantViolation,
		"escaped transaction rollback never dereferences the destroyed registry");

	{
		PackageRandomTransaction destructorOnly;
		{
			ContentRegistry content(CurrentContentApiVersion);
			TransactionPackage package("rules.destructor-escape", 0);
			PackageRegistry registry(content, EngineServices::defaults(),
				NullPackageEventSink::instance(), RuntimeMessageBus::disabled(),
				ServiceCatalog::disabled(), RuntimeConfiguration::disabled(),
				HostSeed, StreamLimit);
			Check(registry.registerPackage(package) ==
					PackageRegistrationError::None &&
				registry.activate("rules.destructor-escape") ==
					PackageActivationError::None &&
				registry.bootstrap(PackageBootstrapPhase::Configure) ==
					PackageBootstrapError::None &&
				registry.bootstrap(PackageBootstrapPhase::LoadContent) ==
					PackageBootstrapError::None &&
				registry.bootstrap(PackageBootstrapPhase::StartRuntime) ==
					PackageBootstrapError::None,
				"destructor-only escaped lifetime fixture is ready");
			destructorOnly = registry.beginRandomTransaction();
			Check(destructorOnly,
				"destructor-only escaped transaction begins");
		}
		// destructorOnly leaves this scope after its registry. ASan verifies that
		// implicit rollback observes the invalidated token and does not dereference
		// the destroyed PackageRegistry.
	}
}
}

int main()
{
	TestBeginContractAndStatefulQuery();
	TestOwnerOperationsAndDispatchFreeze();
	TestEpochDetectsDerivedDrawAndRollbackNeverRewindsIt();
	TestCommitRechecksEpochAtPublication();
	TestInvalidAndUnboundDrawsDoNotTripRegistryEvidence();
	TestOwnerRestoreAndTargetCommit();
	TestStateComparisonTargetCommitAndMoveOwnership();
	TestAllocationFailureRollbackAndEscapedLifetime();
	std::puts("package random transaction tests passed");
	return 0;
}
