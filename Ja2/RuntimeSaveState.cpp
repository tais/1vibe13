#include "RuntimeSaveState.h"

#include "GameContext.h"
#include "random.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
constexpr std::uint32_t RuntimeCheckpointSection = 0x504b4843u; // "CHKP"
constexpr std::uint32_t PackageStateSection = 0x54534750u; // "PGST"

bool RemoveIncompleteSave(GameContext& context, const std::string& path) noexcept
{
	try { return context.persistence().storage().remove(path); }
	catch (...) { return false; }
}

SimulationRandom* CanonicalSimulationRandom(GameContext& context) noexcept
{
	// This gate is intentionally closed by today's legacy global source. The
	// dedicated startup may replace it only with the manifest-seeded canonical
	// SimulationRandom after the checkpoint-eligibility collector is wired; the
	// execution guard and non-rewindable package transaction below already own
	// every strict serializer/loader exit.
	RandomSource& serviceRandom = context.services().random;
	RandomSource& legacyRandom = GetGameRandomSource();
	if (&serviceRandom != &legacyRandom) return nullptr;
	return dynamic_cast<SimulationRandom*>(&legacyRandom);
}

const SimulationRandom* CanonicalSimulationRandom(
	const GameContext& context) noexcept
{
	const RandomSource& serviceRandom = context.services().random;
	const RandomSource& legacyRandom = GetGameRandomSource();
	if (&serviceRandom != &legacyRandom) return nullptr;
	return dynamic_cast<const SimulationRandom*>(&legacyRandom);
}

bool SamePackages(const std::vector<RuntimeCheckpointPackage>& first,
	const std::vector<RuntimeCheckpointPackage>& second) noexcept
{
	if (first.size() != second.size()) return false;
	for (std::size_t index = 0; index < first.size(); ++index)
	{
		if (first[index].id != second[index].id ||
			first[index].version != second[index].version)
			return false;
	}
	return true;
}

bool SameRuntimeBoundary(const RuntimeCheckpoint& first,
	const RuntimeCheckpoint& second) noexcept
{
	return first.compatibility == second.compatibility &&
		first.completedFrames == second.completedFrames &&
		first.completedSimulationTicks == second.completedSimulationTicks &&
		first.frameBoundary == second.frameBoundary &&
		first.simulationTickBoundary == second.simulationTickBoundary &&
		SamePackages(first.activePackages, second.activePackages);
}

bool UsesCurrentPackageRandomSchema(
	const PackageSaveStateSnapshot& snapshot) noexcept
{
	for (const PackageEngineSaveStateRecord& record : snapshot.engineRecords)
	{
		if (record.random.schema != PackageRandomCheckpoint::CurrentSchema)
			return false;
	}
	return true;
}

bool StrictRuntimeStillMatches(GameContext& context,
	const RuntimeCheckpoint& checkpoint,
	const RuntimeRandomCheckpoint& randomCheckpoint,
	RuntimeSavePolicyError& policyError) noexcept
{
	SimulationRandom* random = CanonicalSimulationRandom(context);
	if (random == nullptr)
	{
		policyError =
			RuntimeSavePolicyError::CanonicalSimulationRandomRequired;
		return false;
	}
	if (!random->healthy())
	{
		policyError = RuntimeSavePolicyError::SimulationRandomUnhealthy;
		return false;
	}
	if (!(random->checkpoint() == randomCheckpoint.simulationRandom))
	{
		policyError = RuntimeSavePolicyError::SimulationRandomChanged;
		return false;
	}
	try
	{
		if (context.runtime().compatibilityFingerprint() !=
			randomCheckpoint.compatibility)
		{
			policyError = RuntimeSavePolicyError::RuntimeCompatibilityChanged;
			return false;
		}
		if (context.runtime().packageRandomHostSeed() !=
			randomCheckpoint.packageRandomHostSeed)
		{
			policyError = RuntimeSavePolicyError::PackageRandomHostSeedChanged;
			return false;
		}
		const RuntimeCheckpointCaptureResult current =
			context.runtime().captureRuntimeCheckpoint();
		if (!current || !SameRuntimeBoundary(current.checkpoint, checkpoint))
		{
			policyError = RuntimeSavePolicyError::DeterministicBoundaryChanged;
			return false;
		}
		return true;
	}
	catch (...)
	{
		policyError = RuntimeSavePolicyError::RuntimeCompatibilityChanged;
		return false;
	}
}

PackageSaveStateLoadResult StrictRestoreFailure(
	PreparedRuntimeLoad& prepared, RuntimeSavePolicyError policyError,
	PackageSaveStateError packageError =
		PackageSaveStateError::RuntimeNotReady) noexcept
{
	prepared.policyError = policyError;
	prepared.packageContractError = packageError;
	return {packageError, {}, 0, 0};
}

RuntimeExecutionGuardResult GuardPolicyFailure(
	RuntimeSavePolicyError error) noexcept
{
	return {error, PackageRandomTransactionError::None};
}

RuntimeExecutionGuardResult GuardPackageFailure(
	PackageRandomTransactionError error) noexcept
{
	return {RuntimeSavePolicyError::PackageRandomTransactionFailed, error};
}

PackageSaveStateError PackageGuardSaveStateError(
	PackageRandomTransactionError error) noexcept
{
	return error == PackageRandomTransactionError::RandomConsumed
		? PackageSaveStateError::RandomConsumed
		: PackageSaveStateError::EngineStateMismatch;
}

RuntimeSavePolicyError UnguardedStrictExecutionError(
	const GameContext& context) noexcept
{
	const SimulationRandom* const simulationRandom =
		CanonicalSimulationRandom(context);
	if (simulationRandom == nullptr)
		return RuntimeSavePolicyError::CanonicalSimulationRandomRequired;
	if (!simulationRandom->healthy())
		return RuntimeSavePolicyError::SimulationRandomUnhealthy;
	if (context.hasActiveStatefulPackageSaveState())
		return RuntimeSavePolicyError::SemanticPackagePreflightRequired;
	return RuntimeSavePolicyError::ExecutionGuardRequired;
}

static_assert(std::is_move_constructible<RuntimeSaveExecutionGuard>::value,
	"save execution guards must be move constructible");
static_assert(!std::is_move_assignable<RuntimeSaveExecutionGuard>::value,
	"armed save execution guards must not be overwritten");
static_assert(std::is_move_constructible<RuntimeLoadExecutionGuard>::value,
	"load execution guards must be move constructible");
static_assert(!std::is_move_assignable<RuntimeLoadExecutionGuard>::value,
	"armed load execution guards must not be overwritten");
}

RuntimeSaveExecutionGuard::RuntimeSaveExecutionGuard() noexcept = default;

RuntimeSaveExecutionGuard::RuntimeSaveExecutionGuard(
	GameContext& context, RuntimeSavePolicy policy) noexcept
	: context_(&context), policy_(policy)
{
}

RuntimeSaveExecutionGuard::~RuntimeSaveExecutionGuard()
{
	(void)rollback();
}

RuntimeSaveExecutionGuard::RuntimeSaveExecutionGuard(
	RuntimeSaveExecutionGuard&& other) noexcept
	: context_(other.context_), simulationRandom_(other.simulationRandom_),
	  simulationBaseline_(other.simulationBaseline_),
	  simulationEpoch_(other.simulationEpoch_),
	  packageTransaction_(std::move(other.packageTransaction_)),
	  beginResult_(other.beginResult_), finalResult_(other.finalResult_),
	  policy_(other.policy_), armed_(other.armed_), finished_(other.finished_)
{
	other.context_ = nullptr;
	other.simulationRandom_ = nullptr;
	other.armed_ = false;
	other.finished_ = true;
}

RuntimeSaveExecutionGuard::operator bool() const noexcept
{
	return static_cast<bool>(beginResult_) && !finished_ &&
		(policy_ == RuntimeSavePolicy::Interactive || armed_);
}

RuntimeExecutionGuardResult
RuntimeSaveExecutionGuard::verifyUnchanged() const noexcept
{
	if (policy_ == RuntimeSavePolicy::Interactive) return {};
	if (!armed_ || simulationRandom_ == nullptr || !packageTransaction_)
		return GuardPolicyFailure(RuntimeSavePolicyError::ExecutionGuardRequired);
	if (simulationRandom_->consumptionEpoch() != simulationEpoch_)
		return GuardPolicyFailure(
			RuntimeSavePolicyError::SimulationRandomConsumed);
	if (!simulationRandom_->healthy())
		return GuardPolicyFailure(
			RuntimeSavePolicyError::SimulationRandomUnhealthy);
	if (!(simulationRandom_->checkpoint() == simulationBaseline_))
		return GuardPolicyFailure(
			RuntimeSavePolicyError::SimulationRandomChanged);
	return {};
}

RuntimeExecutionGuardResult RuntimeSaveExecutionGuard::rollback() noexcept
{
	if (finished_) return finalResult_;
	if (!static_cast<bool>(beginResult_) && !armed_)
	{
		finalResult_ = {};
		finished_ = true;
		context_ = nullptr;
		return finalResult_;
	}
	if (policy_ == RuntimeSavePolicy::Interactive)
	{
		finalResult_ = {};
		finished_ = true;
		context_ = nullptr;
		return finalResult_;
	}

	RuntimeExecutionGuardResult result;
	if (simulationRandom_ == nullptr)
	{
		result = GuardPolicyFailure(
			RuntimeSavePolicyError::CanonicalSimulationRandomRequired);
	}
	else
	{
		const bool changed = !simulationRandom_->healthy() ||
			!(simulationRandom_->checkpoint() == simulationBaseline_);
		if (changed)
		{
			if (simulationRandom_->validateCheckpoint(simulationBaseline_) !=
					SimulationRandomCheckpointError::None ||
				simulationRandom_->restoreCheckpoint(simulationBaseline_) !=
					SimulationRandomCheckpointError::None)
				result = GuardPolicyFailure(
					RuntimeSavePolicyError::SimulationRandomRestoreFailed);
		}
	}

	if (armed_)
	{
		const PackageRandomTransactionResult package =
			packageTransaction_.rollback();
		if (!package)
		{
			result.packageRandomTransactionError = package.error;
			if (result.policyError == RuntimeSavePolicyError::None)
				result.policyError = RuntimeSavePolicyError::
					PackageRandomTransactionFailed;
		}
	}
	armed_ = false;
	finished_ = true;
	context_ = nullptr;
	simulationRandom_ = nullptr;
	finalResult_ = result;
	return finalResult_;
}

RuntimeExecutionGuardResult
RuntimeSaveExecutionGuard::commitUnchanged() noexcept
{
	RuntimeExecutionGuardResult result = verifyUnchanged();
	if (!result) return result;
	if (policy_ == RuntimeSavePolicy::Interactive)
	{
		finished_ = true;
		context_ = nullptr;
		finalResult_ = {};
		return finalResult_;
	}

	const PackageRandomTransactionResult package =
		packageTransaction_.commitUnchanged();
	if (!package)
	{
		result = GuardPackageFailure(package.error);
		return result;
	}
	armed_ = false;
	finished_ = true;
	context_ = nullptr;
	simulationRandom_ = nullptr;
	finalResult_ = {};
	return finalResult_;
}

RuntimeLoadExecutionGuard::RuntimeLoadExecutionGuard() noexcept = default;

RuntimeLoadExecutionGuard::RuntimeLoadExecutionGuard(
	GameContext& context, RuntimeSavePolicy policy) noexcept
	: context_(&context), policy_(policy)
{
}

RuntimeLoadExecutionGuard::~RuntimeLoadExecutionGuard()
{
	(void)rollback();
}

RuntimeLoadExecutionGuard::RuntimeLoadExecutionGuard(
	RuntimeLoadExecutionGuard&& other) noexcept
	: context_(other.context_), simulationRandom_(other.simulationRandom_),
	  simulationBaseline_(other.simulationBaseline_),
	  simulationEpoch_(other.simulationEpoch_),
	  packageTransaction_(std::move(other.packageTransaction_)),
	  beginResult_(other.beginResult_), finalResult_(other.finalResult_),
	  policy_(other.policy_), armed_(other.armed_), finished_(other.finished_)
{
	other.context_ = nullptr;
	other.simulationRandom_ = nullptr;
	other.armed_ = false;
	other.finished_ = true;
}

RuntimeLoadExecutionGuard::operator bool() const noexcept
{
	return static_cast<bool>(beginResult_) && !finished_ &&
		(policy_ == RuntimeSavePolicy::Interactive || armed_);
}

RuntimeExecutionGuardResult
RuntimeLoadExecutionGuard::verifyBaseline() const noexcept
{
	if (policy_ == RuntimeSavePolicy::Interactive) return {};
	if (!armed_ || simulationRandom_ == nullptr || !packageTransaction_)
		return GuardPolicyFailure(RuntimeSavePolicyError::ExecutionGuardRequired);
	if (simulationRandom_->consumptionEpoch() != simulationEpoch_)
		return GuardPolicyFailure(
			RuntimeSavePolicyError::SimulationRandomConsumed);
	if (!simulationRandom_->healthy())
		return GuardPolicyFailure(
			RuntimeSavePolicyError::SimulationRandomUnhealthy);
	if (!(simulationRandom_->checkpoint() == simulationBaseline_))
		return GuardPolicyFailure(
			RuntimeSavePolicyError::SimulationRandomChanged);
	return {};
}

RuntimeExecutionGuardResult RuntimeLoadExecutionGuard::rollback() noexcept
{
	if (finished_) return finalResult_;
	if (!static_cast<bool>(beginResult_) && !armed_)
	{
		finalResult_ = {};
		finished_ = true;
		context_ = nullptr;
		return finalResult_;
	}
	if (policy_ == RuntimeSavePolicy::Interactive)
	{
		finalResult_ = {};
		finished_ = true;
		context_ = nullptr;
		return finalResult_;
	}

	RuntimeExecutionGuardResult result;
	if (simulationRandom_ == nullptr)
	{
		result = GuardPolicyFailure(
			RuntimeSavePolicyError::CanonicalSimulationRandomRequired);
	}
	else
	{
		const bool changed = !simulationRandom_->healthy() ||
			!(simulationRandom_->checkpoint() == simulationBaseline_);
		if (changed)
		{
			if (simulationRandom_->validateCheckpoint(simulationBaseline_) !=
					SimulationRandomCheckpointError::None ||
				simulationRandom_->restoreCheckpoint(simulationBaseline_) !=
					SimulationRandomCheckpointError::None)
				result = GuardPolicyFailure(
					RuntimeSavePolicyError::SimulationRandomRestoreFailed);
		}
	}

	if (armed_)
	{
		const PackageRandomTransactionResult package =
			packageTransaction_.rollback();
		if (!package)
		{
			result.packageRandomTransactionError = package.error;
			if (result.policyError == RuntimeSavePolicyError::None)
				result.policyError = RuntimeSavePolicyError::
					PackageRandomTransactionFailed;
		}
	}
	armed_ = false;
	finished_ = true;
	context_ = nullptr;
	simulationRandom_ = nullptr;
	finalResult_ = result;
	return finalResult_;
}

RuntimeExecutionGuardResult RuntimeLoadExecutionGuard::commitTarget(
	const std::vector<PackageEngineSaveStateRecord>& target) noexcept
{
	if (context_ == nullptr || finished_ || !beginResult_ ||
		(policy_ == RuntimeSavePolicy::DedicatedDeterministic &&
			(!armed_ || !packageTransaction_)))
		return GuardPolicyFailure(RuntimeSavePolicyError::ExecutionGuardRequired);
	if (policy_ == RuntimeSavePolicy::Interactive)
	{
		finished_ = true;
		context_ = nullptr;
		finalResult_ = {};
		return finalResult_;
	}
	if (simulationRandom_ == nullptr || !simulationRandom_->healthy())
		return GuardPolicyFailure(
			RuntimeSavePolicyError::SimulationRandomUnhealthy);
	if (simulationRandom_->consumptionEpoch() != simulationEpoch_)
		return GuardPolicyFailure(
			RuntimeSavePolicyError::SimulationRandomConsumed);

	const PackageRandomTransactionResult package =
		packageTransaction_.commitTarget(target);
	if (!package)
	{
		const RuntimeExecutionGuardResult result =
			GuardPackageFailure(package.error);
		return result;
	}
	armed_ = false;
	finished_ = true;
	context_ = nullptr;
	simulationRandom_ = nullptr;
	finalResult_ = {};
	return finalResult_;
}

RuntimeSaveExecutionGuard BeginRuntimeSaveExecution(
	GameContext& context, RuntimeSavePolicy policy) noexcept
{
	RuntimeSaveExecutionGuard guard(context, policy);
	if (policy == RuntimeSavePolicy::Interactive)
	{
		guard.beginResult_ = {};
		return guard;
	}

	guard.simulationRandom_ = CanonicalSimulationRandom(context);
	if (guard.simulationRandom_ == nullptr)
	{
		guard.beginResult_ = GuardPolicyFailure(
			RuntimeSavePolicyError::CanonicalSimulationRandomRequired);
		return guard;
	}
	if (!guard.simulationRandom_->healthy())
	{
		guard.beginResult_ = GuardPolicyFailure(
			RuntimeSavePolicyError::SimulationRandomUnhealthy);
		return guard;
	}
	if (context.hasActiveStatefulPackageSaveState())
	{
		guard.beginResult_ = GuardPolicyFailure(
			RuntimeSavePolicyError::SemanticPackagePreflightRequired);
		return guard;
	}
	guard.simulationBaseline_ = guard.simulationRandom_->checkpoint();
	guard.simulationEpoch_ = guard.simulationRandom_->consumptionEpoch();
	PackageRandomTransaction packages =
		context.beginPackageRandomTransaction();
	if (!packages)
	{
		guard.beginResult_ = GuardPackageFailure(packages.beginResult().error);
		return guard;
	}
	guard.packageTransaction_ = std::move(packages);
	guard.armed_ = true;
	guard.beginResult_ = guard.verifyUnchanged();
	if (!guard.beginResult_)
	{
		const RuntimeExecutionGuardResult failure = guard.beginResult_;
		(void)guard.rollback();
		guard.beginResult_ = failure;
	}
	return guard;
}

RuntimeLoadExecutionGuard BeginRuntimeLoadExecution(
	GameContext& context, RuntimeSavePolicy policy) noexcept
{
	RuntimeLoadExecutionGuard guard(context, policy);
	if (policy == RuntimeSavePolicy::Interactive)
	{
		guard.beginResult_ = {};
		return guard;
	}

	guard.simulationRandom_ = CanonicalSimulationRandom(context);
	if (guard.simulationRandom_ == nullptr)
	{
		guard.beginResult_ = GuardPolicyFailure(
			RuntimeSavePolicyError::CanonicalSimulationRandomRequired);
		return guard;
	}
	if (!guard.simulationRandom_->healthy())
	{
		guard.beginResult_ = GuardPolicyFailure(
			RuntimeSavePolicyError::SimulationRandomUnhealthy);
		return guard;
	}
	if (context.hasActiveStatefulPackageSaveState())
	{
		guard.beginResult_ = GuardPolicyFailure(
			RuntimeSavePolicyError::SemanticPackagePreflightRequired);
		return guard;
	}
	guard.simulationBaseline_ = guard.simulationRandom_->checkpoint();
	guard.simulationEpoch_ = guard.simulationRandom_->consumptionEpoch();
	PackageRandomTransaction packages =
		context.beginPackageRandomTransaction();
	if (!packages)
	{
		guard.beginResult_ = GuardPackageFailure(packages.beginResult().error);
		return guard;
	}
	guard.packageTransaction_ = std::move(packages);
	guard.armed_ = true;
	guard.beginResult_ = guard.verifyBaseline();
	if (!guard.beginResult_)
	{
		const RuntimeExecutionGuardResult failure = guard.beginResult_;
		(void)guard.rollback();
		guard.beginResult_ = failure;
	}
	return guard;
}

PreparedRuntimeSave PrepareRuntimeSave(
	GameContext& context, RuntimeSaveExecutionGuard& guard) noexcept
{
	PreparedRuntimeSave prepared;
	const RuntimeSavePolicy policy = guard.policy_;
	prepared.policy_ = policy;
	if (guard.context_ != &context || guard.finished_)
	{
		prepared.policyError = RuntimeSavePolicyError::ExecutionGuardRequired;
		return prepared;
	}
	if (!guard.beginResult_)
	{
		prepared.policyError = guard.beginResult_.policyError;
		prepared.packageRandomTransactionError =
			guard.beginResult_.packageRandomTransactionError;
		return prepared;
	}
	if (policy == RuntimeSavePolicy::DedicatedDeterministic && !guard.armed_)
	{
		prepared.policyError = RuntimeSavePolicyError::ExecutionGuardRequired;
		return prepared;
	}
	auto rollbackPrepared = [&]() noexcept -> PreparedRuntimeSave
	{
		const RuntimeExecutionGuardResult cleanup = guard.rollback();
		if (!cleanup)
		{
			if (prepared.policyError == RuntimeSavePolicyError::None)
				prepared.policyError = cleanup.policyError;
			if (prepared.packageRandomTransactionError ==
				PackageRandomTransactionError::None)
				prepared.packageRandomTransactionError =
					cleanup.packageRandomTransactionError;
		}
		return std::move(prepared);
	};
	SimulationRandom* simulationRandom = nullptr;
	SimulationRandomCheckpoint simulationSentinel;
	try
	{
		if (policy == RuntimeSavePolicy::DedicatedDeterministic)
		{
			simulationRandom = guard.simulationRandom_;
			if (simulationRandom == nullptr)
			{
				prepared.policyError = RuntimeSavePolicyError::
					CanonicalSimulationRandomRequired;
				return rollbackPrepared();
			}
			if (!simulationRandom->healthy())
			{
				prepared.policyError =
					RuntimeSavePolicyError::SimulationRandomUnhealthy;
				return rollbackPrepared();
			}
			simulationSentinel = guard.simulationBaseline_;

			const RuntimeCheckpointCaptureResult captured =
				context.runtime().captureRuntimeCheckpoint();
			if (!captured)
			{
				prepared.policyError =
					RuntimeSavePolicyError::DeterministicBoundaryRequired;
				return rollbackPrepared();
			}
			prepared.checkpoint = captured.checkpoint;
		}
		else
		{
			prepared.checkpoint = context.runtime().makeRuntimeCheckpoint();
		}
		prepared.checkpointError = RuntimeCheckpointSaveError::None;

		PackageSaveStateCaptureResult captured = context.capturePackageSaveState(
			policy == RuntimeSavePolicy::DedicatedDeterministic
				? PackageSaveRandomPolicy::RequireUnconsumed
				: PackageSaveRandomPolicy::AllowAndRollback);
		prepared.packageCaptureError = captured.error;
		prepared.packageId = std::move(captured.packageId);
		if (captured)
			prepared.packageState = std::move(captured.snapshot);

		if (policy == RuntimeSavePolicy::DedicatedDeterministic)
		{
			const RuntimeExecutionGuardResult unchangedAfterCapture =
				guard.verifyUnchanged();
			if (!unchangedAfterCapture)
			{
				prepared.policyError = unchangedAfterCapture.policyError;
				prepared.packageRandomTransactionError =
					unchangedAfterCapture.packageRandomTransactionError;
				return rollbackPrepared();
			}
			if (!captured) return rollbackPrepared();
			if (prepared.packageState.engineRecords !=
				guard.packageTransaction_.baseline().engineRecords)
			{
				prepared.policyError = RuntimeSavePolicyError::
					PackageRandomTransactionFailed;
				prepared.packageRandomTransactionError =
					PackageRandomTransactionError::StateChanged;
				return rollbackPrepared();
			}
			// The current package API performs semantic validateState callbacks
			// only during restore, after the legacy domain loader has already
			// dismantled live state. Never publish a strict checkpoint that this
			// coordinator cannot preflight before that destructive boundary.
			if (!prepared.packageState.records.empty())
			{
				prepared.policyError = RuntimeSavePolicyError::
					SemanticPackagePreflightRequired;
				return rollbackPrepared();
			}
			if (!UsesCurrentPackageRandomSchema(prepared.packageState))
			{
				prepared.policyError = RuntimeSavePolicyError::
					UnsupportedPackageRandomSchema;
				return rollbackPrepared();
			}
			prepared.randomCheckpoint.compatibility =
				prepared.checkpoint.compatibility;
			prepared.randomCheckpoint.simulationRandom = simulationSentinel;
			prepared.randomCheckpoint.packageRandomHostSeed =
				context.runtime().packageRandomHostSeed();
			if (!StrictRuntimeStillMatches(context, prepared.checkpoint,
					prepared.randomCheckpoint, prepared.policyError))
				return rollbackPrepared();
			const RuntimeExecutionGuardResult unchanged =
				guard.verifyUnchanged();
			if (!unchanged)
			{
				prepared.policyError = unchanged.policyError;
				prepared.packageRandomTransactionError =
					unchanged.packageRandomTransactionError;
				return rollbackPrepared();
			}
		}
	}
	catch (...)
	{
		if (prepared.checkpointError != RuntimeCheckpointSaveError::None)
			prepared.checkpointError = RuntimeCheckpointSaveError::StorageError;
		else if (prepared.packageCaptureError != PackageSaveStateError::None)
			prepared.packageCaptureError = PackageSaveStateError::AllocationFailure;
		else
			prepared.checkpointError = RuntimeCheckpointSaveError::StorageError;
		return rollbackPrepared();
	}
	return prepared;
}

RuntimeSaveCommitResult CommitRuntimeSave(GameContext& context,
	const std::string& savePath, PreparedRuntimeSave prepared,
	RuntimeSaveExecutionGuard& guard) noexcept
{
	RuntimeSaveCommitResult result;
	result.policy = prepared.policy_;
	result.policyError = prepared.policyError;
	result.checkpointError = prepared.checkpointError;
	result.packageCaptureError = prepared.packageCaptureError;
	result.packageRandomTransactionError =
		prepared.packageRandomTransactionError;
	result.packageId = std::move(prepared.packageId);
	if (guard.context_ != &context || guard.policy_ != prepared.policy_ ||
		guard.finished_)
	{
		result.policyError = RuntimeSavePolicyError::ExecutionGuardRequired;
		result.packageRandomTransactionError =
			PackageRandomTransactionError::None;
		return result;
	}
	if (!guard.beginResult_)
	{
		result.policyError = guard.beginResult_.policyError;
		result.packageRandomTransactionError =
			guard.beginResult_.packageRandomTransactionError;
		return result;
	}
	if (!guard)
	{
		result.policyError = RuntimeSavePolicyError::ExecutionGuardRequired;
		result.packageRandomTransactionError =
			PackageRandomTransactionError::None;
		return result;
	}
	auto rollbackResult = [&]() noexcept -> RuntimeSaveCommitResult
	{
		// Keep both RNG domains armed while storage cleanup callbacks run.
		if (!savePath.empty() && !RemoveIncompleteSave(context, savePath))
			result.containerError = RuntimeSaveContainerSaveError::StorageError;
		const RuntimeExecutionGuardResult cleanup = guard.rollback();
		if (!cleanup)
		{
			if (result.policyError == RuntimeSavePolicyError::None)
				result.policyError = cleanup.policyError;
			if (result.packageRandomTransactionError ==
				PackageRandomTransactionError::None)
				result.packageRandomTransactionError =
					cleanup.packageRandomTransactionError;
		}
		return result;
	};
	if (savePath.empty())
	{
		result.containerError = RuntimeSaveContainerSaveError::InvalidRequest;
		return rollbackResult();
	}
	if (!prepared)
		return rollbackResult();
	try
	{
		const bool strict =
			prepared.policy_ == RuntimeSavePolicy::DedicatedDeterministic;
		if (strict && !StrictRuntimeStillMatches(context, prepared.checkpoint,
				prepared.randomCheckpoint, result.policyError))
			return rollbackResult();

		std::vector<std::uint8_t> checkpointBytes;
		if (strict)
		{
			result.checkpointError =
				context.runtime().runtimeCheckpoints().encode(
					prepared.checkpoint, checkpointBytes);
		}
		else
		{
			// Interactive saves are still initiated synchronously by the legacy
			// screen handler from inside FrameDriver::runFrame. They retain CHKP
			// v1 metadata and never claim a committed restorable boundary.
			result.checkpointError =
				context.runtime().runtimeCheckpoints().encodeLegacyMetadata(
					prepared.checkpoint, checkpointBytes);
		}
		if (result.checkpointError != RuntimeCheckpointSaveError::None)
			return rollbackResult();

		std::vector<std::uint8_t> packageBytes;
		result.packageArchiveError = context.packageSaveArchives().encode(
			PackageSaveArchive{
				prepared.checkpoint.compatibility,
				std::move(prepared.packageState)},
			packageBytes);
		if (result.packageArchiveError != PackageSaveArchiveSaveError::None)
			return rollbackResult();

		std::vector<std::uint8_t> randomBytes;
		if (strict)
		{
			RuntimeRandomCheckpointService randomCheckpoints(
				context.persistence());
			result.randomCheckpointError = randomCheckpoints.encode(
				prepared.randomCheckpoint, randomBytes);
			if (result.randomCheckpointError !=
					RuntimeRandomCheckpointSaveError::None)
				return rollbackResult();
			if (!StrictRuntimeStillMatches(context, prepared.checkpoint,
					prepared.randomCheckpoint, result.policyError))
				return rollbackResult();
		}

		std::vector<RuntimeSaveSection> sections;
		sections.reserve(strict ? 3 : 2);
		sections.push_back(
			RuntimeSaveSection{RuntimeCheckpointSection, std::move(checkpointBytes)});
		sections.push_back(
			RuntimeSaveSection{PackageStateSection, std::move(packageBytes)});
		if (strict)
			sections.push_back(RuntimeSaveSection{
				RuntimeRandomCheckpointSectionType, std::move(randomBytes)});
		result.containerError =
			context.runtimeSaveContainers().seal(savePath, sections);
		if (result.containerError != RuntimeSaveContainerSaveError::None)
			return rollbackResult();

		// Storage can execute user-provided callbacks. Recheck every strict
		// runtime invariant after sealing and before publishing the RNG state.
		if (strict && !StrictRuntimeStillMatches(context, prepared.checkpoint,
				prepared.randomCheckpoint, result.policyError))
			return rollbackResult();

		// Package commit is deliberately the final fallible publication gate.
		const RuntimeExecutionGuardResult committed =
			guard.commitUnchanged();
		if (!committed)
		{
			result.policyError = committed.policyError;
			result.packageRandomTransactionError =
				committed.packageRandomTransactionError;
			return rollbackResult();
		}
		return result;
	}
	catch (...)
	{
		result.containerError = RuntimeSaveContainerSaveError::StorageError;
		return rollbackResult();
	}
}

PreparedRuntimeLoad PrepareRuntimeLoad(const GameContext& context,
	const std::string& savePath, RuntimeLoadExecutionGuard& guard) noexcept
{
	PreparedRuntimeLoad prepared;
	const RuntimeSavePolicy policy = guard.policy_;
	prepared.policy_ = policy;
	if (guard.context_ != &context || guard.finished_)
	{
		prepared.policyError = RuntimeSavePolicyError::ExecutionGuardRequired;
		return prepared;
	}
	if (!guard.beginResult_)
	{
		prepared.policyError = guard.beginResult_.policyError;
		prepared.packageRandomTransactionError =
			guard.beginResult_.packageRandomTransactionError;
		return prepared;
	}
	if (policy == RuntimeSavePolicy::DedicatedDeterministic && !guard.armed_)
	{
		prepared.policyError = RuntimeSavePolicyError::ExecutionGuardRequired;
		return prepared;
	}
	auto rollbackPrepared = [&]() noexcept -> PreparedRuntimeLoad
	{
		const RuntimeExecutionGuardResult cleanup = guard.rollback();
		if (!cleanup)
		{
			if (prepared.policyError == RuntimeSavePolicyError::None)
				prepared.policyError = cleanup.policyError;
			if (prepared.packageRandomTransactionError ==
				PackageRandomTransactionError::None)
				prepared.packageRandomTransactionError =
					cleanup.packageRandomTransactionError;
		}
		return std::move(prepared);
	};

	const SimulationRandom* const simulationRandom = guard.simulationRandom_;
	try
	{
		RuntimeSaveContainer container;
		const RuntimeSaveContainerLoadResult loaded =
			context.runtimeSaveContainers().inspect(savePath, container);
		prepared.containerError = loaded.error;
		if (!loaded) return rollbackPrepared();
		prepared.domainBytes = container.domainBytes;
		if (policy == RuntimeSavePolicy::DedicatedDeterministic &&
			container.sections.size() != 3)
		{
			prepared.containerError =
				RuntimeSaveContainerLoadError::MalformedContainer;
			return rollbackPrepared();
		}

		const RuntimeSaveSection* checkpoint =
			container.find(RuntimeCheckpointSection);
		const RuntimeSaveSection* packages =
			container.find(PackageStateSection);
		const RuntimeSaveSection* random =
			container.find(RuntimeRandomCheckpointSectionType);
		if (!checkpoint || !packages)
		{
			prepared.containerError =
				RuntimeSaveContainerLoadError::MalformedContainer;
			return rollbackPrepared();
		}
		if (policy == RuntimeSavePolicy::DedicatedDeterministic && !random)
		{
			prepared.policyError =
				RuntimeSavePolicyError::MissingRandomCheckpointSection;
			prepared.randomCheckpointError =
				RuntimeRandomCheckpointLoadError::InvalidOrUnsupported;
			return rollbackPrepared();
		}

		const RuntimeCompatibilityFingerprint compatibility =
			context.runtime().compatibilityFingerprint();
		const RuntimeCheckpointLoadResult checkpointResult =
			context.runtime().runtimeCheckpoints().decode(
				checkpoint->payload, compatibility, prepared.checkpoint);
		prepared.checkpointError = checkpointResult.error;
		if (!checkpointResult) return rollbackPrepared();
		if (policy == RuntimeSavePolicy::DedicatedDeterministic)
		{
			if (checkpointResult.storedVersion !=
					RuntimeCheckpointService::CurrentVersion ||
				!checkpointResult.hasDeterministicBoundary)
			{
				prepared.policyError = RuntimeSavePolicyError::
					UnsupportedCheckpointVersion;
				return rollbackPrepared();
			}
			prepared.checkpointBoundary =
				context.runtime().validateRuntimeCheckpointBoundary(
					prepared.checkpoint);
			if (!prepared.checkpointBoundary)
			{
				prepared.policyError = RuntimeSavePolicyError::
					DeterministicBoundaryRequired;
				return rollbackPrepared();
			}
		}

		const PackageSaveArchiveLoadResult packageResult =
			context.packageSaveArchives().decode(
				packages->payload, compatibility, prepared.packages);
		prepared.packageArchiveError = packageResult.error;
		if (!packageResult) return rollbackPrepared();
		if (policy == RuntimeSavePolicy::DedicatedDeterministic)
		{
			if (packageResult.storedVersion !=
					PackageSaveArchiveService::CurrentVersion)
			{
				prepared.policyError = RuntimeSavePolicyError::
					UnsupportedPackageArchiveVersion;
				return rollbackPrepared();
			}
			if (!UsesCurrentPackageRandomSchema(prepared.packages.state))
			{
				prepared.policyError = RuntimeSavePolicyError::
					UnsupportedPackageRandomSchema;
				return rollbackPrepared();
			}
			if (!prepared.packages.state.records.empty())
			{
				prepared.policyError = RuntimeSavePolicyError::
					SemanticPackagePreflightRequired;
				return rollbackPrepared();
			}
		}

		if (policy == RuntimeSavePolicy::DedicatedDeterministic)
		{
			if (simulationRandom == nullptr)
			{
				prepared.policyError = RuntimeSavePolicyError::
					CanonicalSimulationRandomRequired;
				return rollbackPrepared();
			}
			RuntimeRandomCheckpointService randomCheckpoints(
				context.persistence());
			const RuntimeRandomCheckpointLoadResult randomResult =
				randomCheckpoints.decode(random->payload, compatibility,
					simulationRandom->campaignSeed(),
					context.runtime().packageRandomHostSeed(),
					prepared.randomCheckpoint_);
			prepared.randomCheckpointError = randomResult.error;
			if (!randomResult) return rollbackPrepared();
			if (randomResult.storedVersion !=
					RuntimeRandomCheckpointVersion)
			{
				prepared.randomCheckpointError =
					RuntimeRandomCheckpointLoadError::InvalidOrUnsupported;
				return rollbackPrepared();
			}
		}

		const PackageSaveStateLoadResult contract =
			context.validatePackageSaveState(prepared.packages.state,
				policy == RuntimeSavePolicy::DedicatedDeterministic
					? PackageSaveRandomPolicy::RequireUnconsumed
					: PackageSaveRandomPolicy::AllowAndRollback);
		prepared.packageContractError = contract.error;
		prepared.packageId = contract.packageId;
		if (!contract) return rollbackPrepared();

		if (policy == RuntimeSavePolicy::DedicatedDeterministic)
		{
			if (CanonicalSimulationRandom(context) != simulationRandom)
			{
				prepared.policyError = RuntimeSavePolicyError::
					CanonicalSimulationRandomRequired;
				return rollbackPrepared();
			}
			if (context.runtime().compatibilityFingerprint() !=
				prepared.randomCheckpoint_.compatibility)
			{
				prepared.policyError = RuntimeSavePolicyError::
					RuntimeCompatibilityChanged;
				return rollbackPrepared();
			}
			if (context.runtime().packageRandomHostSeed() !=
				prepared.randomCheckpoint_.packageRandomHostSeed)
			{
				prepared.policyError = RuntimeSavePolicyError::
					PackageRandomHostSeedChanged;
				return rollbackPrepared();
			}
			prepared.checkpointBoundary =
				context.runtime().validateRuntimeCheckpointBoundary(
					prepared.checkpoint);
			if (!prepared.checkpointBoundary)
			{
				prepared.policyError = RuntimeSavePolicyError::
					DeterministicBoundaryRequired;
				return rollbackPrepared();
			}
			if (simulationRandom->validateCheckpoint(
					prepared.randomCheckpoint_.simulationRandom) !=
				SimulationRandomCheckpointError::None)
			{
				prepared.policyError =
					RuntimeSavePolicyError::SimulationRandomChanged;
				return rollbackPrepared();
			}
			const RuntimeExecutionGuardResult unchanged =
				guard.verifyBaseline();
			if (!unchanged)
			{
				prepared.policyError = unchanged.policyError;
				prepared.packageRandomTransactionError =
					unchanged.packageRandomTransactionError;
				return rollbackPrepared();
			}
		}
		prepared.packageRestorePending_ = true;
		return prepared;
	}
	catch (...)
	{
		prepared.containerError = RuntimeSaveContainerLoadError::StorageError;
		return rollbackPrepared();
	}
}

PackageSaveStateLoadResult RestorePreparedRuntimeSave(
	GameContext& context, PreparedRuntimeLoad& prepared,
	RuntimeLoadExecutionGuard& guard) noexcept
{
	if (!prepared.packageRestorePending_) return {};
	auto rejectGuard = [&](const RuntimeExecutionGuardResult& failure) noexcept
		-> PackageSaveStateLoadResult
	{
		prepared.policyError = failure.policyError;
		prepared.packageRandomTransactionError =
			failure.packageRandomTransactionError;
		return {failure.packageRandomTransactionError ==
				PackageRandomTransactionError::None
				? PackageSaveStateError::EngineStateMismatch
				: PackageGuardSaveStateError(
					failure.packageRandomTransactionError),
			{}, 0, 0};
	};
	if (guard.context_ != &context || guard.policy_ != prepared.policy_ ||
		guard.finished_)
		return rejectGuard(GuardPolicyFailure(
			RuntimeSavePolicyError::ExecutionGuardRequired));
	if (!guard.beginResult_)
		return rejectGuard(guard.beginResult_);
	if (!guard)
		return rejectGuard(GuardPolicyFailure(
			RuntimeSavePolicyError::ExecutionGuardRequired));

	// A pure guard-mismatch rejection leaves the staged data retryable. Clear
	// only those diagnostics after a valid owner resumes the pending restore.
	prepared.policyError = RuntimeSavePolicyError::None;
	prepared.packageRandomTransactionError =
		PackageRandomTransactionError::None;
	prepared.packageRestorePending_ = false;
	auto rollbackResult = [&](PackageSaveStateLoadResult result) noexcept
		-> PackageSaveStateLoadResult
	{
		const RuntimeExecutionGuardResult cleanup = guard.rollback();
		if (!cleanup)
		{
			if (prepared.policyError == RuntimeSavePolicyError::None)
				prepared.policyError = cleanup.policyError;
			if (prepared.packageRandomTransactionError ==
				PackageRandomTransactionError::None)
				prepared.packageRandomTransactionError =
					cleanup.packageRandomTransactionError;
			if (result.error == PackageSaveStateError::None)
				result.error = cleanup.packageRandomTransactionError ==
						PackageRandomTransactionError::None
					? PackageSaveStateError::EngineStateMismatch
					: PackageGuardSaveStateError(
						cleanup.packageRandomTransactionError);
		}
		return result;
	};
	auto strictFailure = [&](RuntimeSavePolicyError policyError,
		PackageSaveStateError packageError =
			PackageSaveStateError::EngineStateMismatch) noexcept
		-> PackageSaveStateLoadResult
	{
		return rollbackResult(
			StrictRestoreFailure(prepared, policyError, packageError));
	};
	auto guardFailure = [&](const RuntimeExecutionGuardResult& failure) noexcept
		-> PackageSaveStateLoadResult
	{
		prepared.packageRandomTransactionError =
			failure.packageRandomTransactionError;
		return strictFailure(failure.policyError,
			failure.packageRandomTransactionError ==
				PackageRandomTransactionError::None
				? (failure.policyError ==
						RuntimeSavePolicyError::SimulationRandomConsumed
					? PackageSaveStateError::RandomConsumed
					: PackageSaveStateError::EngineStateMismatch)
				: PackageGuardSaveStateError(
					failure.packageRandomTransactionError));
	};
	try
	{
		if (prepared.policy_ == RuntimeSavePolicy::Interactive)
		{
			PackageSaveStateLoadResult restored =
				context.restorePackageSaveState(prepared.packages.state);
			if (!restored)
			{
				prepared.packageContractError = restored.error;
				return rollbackResult(std::move(restored));
			}
			const RuntimeExecutionGuardResult committed =
				guard.commitTarget(prepared.packages.state.engineRecords);
			if (!committed) return guardFailure(committed);
			prepared.packageContractError = PackageSaveStateError::None;
			return restored;
		}

		const RuntimeExecutionGuardResult baseline = guard.verifyBaseline();
		if (!baseline) return guardFailure(baseline);
		SimulationRandom* const simulationRandom = guard.simulationRandom_;
		if (simulationRandom == nullptr ||
			CanonicalSimulationRandom(context) != simulationRandom)
			return strictFailure(RuntimeSavePolicyError::
				CanonicalSimulationRandomRequired);
		if (context.runtime().compatibilityFingerprint() !=
			prepared.randomCheckpoint_.compatibility)
			return strictFailure(
				RuntimeSavePolicyError::RuntimeCompatibilityChanged);
		if (context.runtime().packageRandomHostSeed() !=
			prepared.randomCheckpoint_.packageRandomHostSeed)
			return strictFailure(
				RuntimeSavePolicyError::PackageRandomHostSeedChanged);
		if (!prepared.packages.state.records.empty())
			return strictFailure(
				RuntimeSavePolicyError::SemanticPackagePreflightRequired);
		prepared.checkpointBoundary =
			context.runtime().validateRuntimeCheckpointBoundary(prepared.checkpoint);
		if (!prepared.checkpointBoundary)
			return strictFailure(
				RuntimeSavePolicyError::DeterministicBoundaryRequired);
		if (simulationRandom->validateCheckpoint(
				prepared.randomCheckpoint_.simulationRandom) !=
			SimulationRandomCheckpointError::None)
			return strictFailure(
				RuntimeSavePolicyError::SimulationRandomChanged);

		// Strict restore order is deliberate: package engine state first, the
		// committed frame/tick boundary second, and the authoritative simulation
		// stream last. Package publication remains guarded until every other
		// fallible validation and restoration operation has completed.
		PackageSaveStateLoadResult restored = context.restorePackageSaveState(
			prepared.packages.state, PackageSaveRandomPolicy::RequireUnconsumed);
		const RuntimeExecutionGuardResult afterPackage = guard.verifyBaseline();
		if (!afterPackage) return guardFailure(afterPackage);
		if (!restored)
		{
			prepared.packageContractError = restored.error;
			return rollbackResult(std::move(restored));
		}

		prepared.checkpointBoundary =
			context.runtime().restoreRuntimeCheckpointBoundary(prepared.checkpoint);
		if (!prepared.checkpointBoundary)
			return strictFailure(RuntimeSavePolicyError::BoundaryRestoreFailed);
		if (simulationRandom->restoreCheckpoint(
				prepared.randomCheckpoint_.simulationRandom) !=
			SimulationRandomCheckpointError::None)
			return strictFailure(
				RuntimeSavePolicyError::SimulationRandomRestoreFailed);

		if (CanonicalSimulationRandom(context) != simulationRandom)
			return strictFailure(RuntimeSavePolicyError::
				CanonicalSimulationRandomRequired);
		if (context.runtime().compatibilityFingerprint() !=
			prepared.randomCheckpoint_.compatibility)
			return strictFailure(
				RuntimeSavePolicyError::RuntimeCompatibilityChanged);
		if (context.runtime().packageRandomHostSeed() !=
			prepared.randomCheckpoint_.packageRandomHostSeed)
			return strictFailure(
				RuntimeSavePolicyError::PackageRandomHostSeedChanged);
		const RuntimeCheckpointCaptureResult current =
			context.runtime().captureRuntimeCheckpoint();
		if (!current ||
			!SameRuntimeBoundary(current.checkpoint, prepared.checkpoint))
			return strictFailure(
				RuntimeSavePolicyError::DeterministicBoundaryChanged);
		if (!simulationRandom->healthy())
			return strictFailure(
				RuntimeSavePolicyError::SimulationRandomUnhealthy);
		if (simulationRandom->consumptionEpoch() != guard.simulationEpoch_)
			return guardFailure(GuardPolicyFailure(
				RuntimeSavePolicyError::SimulationRandomConsumed));
		if (!(simulationRandom->checkpoint() ==
			prepared.randomCheckpoint_.simulationRandom))
			return strictFailure(
				RuntimeSavePolicyError::SimulationRandomChanged);

		// Exact target observation immediately precedes the package transaction,
		// which is the final fallible publication boundary.
		const RuntimeExecutionGuardResult committed = guard.commitTarget(
			prepared.packages.state.engineRecords);
		if (!committed) return guardFailure(committed);
		prepared.packageContractError = PackageSaveStateError::None;
		return restored;
	}
	catch (...)
	{
		return rollbackResult(StrictRestoreFailure(prepared,
			RuntimeSavePolicyError::RuntimeCompatibilityChanged,
			PackageSaveStateError::AllocationFailure));
	}
}

PreparedRuntimeSave PrepareRuntimeSave(
	GameContext& context, RuntimeSavePolicy policy) noexcept
{
	if (policy == RuntimeSavePolicy::DedicatedDeterministic)
	{
		PreparedRuntimeSave prepared;
		prepared.policy_ = policy;
		prepared.policyError = UnguardedStrictExecutionError(context);
		return prepared;
	}

	RuntimeSaveExecutionGuard guard =
		BeginRuntimeSaveExecution(context, policy);
	return PrepareRuntimeSave(context, guard);
}

RuntimeSaveCommitResult CommitRuntimeSave(GameContext& context,
	const std::string& savePath, PreparedRuntimeSave prepared) noexcept
{
	if (prepared.policy_ == RuntimeSavePolicy::DedicatedDeterministic)
	{
		RuntimeSaveCommitResult result;
		result.policy = prepared.policy_;
		result.policyError = UnguardedStrictExecutionError(context);
		result.checkpointError = prepared.checkpointError;
		result.packageCaptureError = prepared.packageCaptureError;
		result.packageRandomTransactionError =
			prepared.packageRandomTransactionError;
		result.packageId = std::move(prepared.packageId);
		return result;
	}

	RuntimeSaveExecutionGuard guard =
		BeginRuntimeSaveExecution(context, prepared.policy_);
	return CommitRuntimeSave(
		context, savePath, std::move(prepared), guard);
}

PreparedRuntimeLoad PrepareRuntimeLoad(const GameContext& context,
	const std::string& savePath, RuntimeSavePolicy policy) noexcept
{
	if (policy == RuntimeSavePolicy::DedicatedDeterministic)
	{
		PreparedRuntimeLoad prepared;
		prepared.policy_ = policy;
		prepared.policyError = UnguardedStrictExecutionError(context);
		return prepared;
	}

	// Interactive execution guards never mutate the context. Preserve the
	// historical const inspection entry point while routing it through the same
	// context-bound implementation.
	GameContext& mutableContext = const_cast<GameContext&>(context);
	RuntimeLoadExecutionGuard guard =
		BeginRuntimeLoadExecution(mutableContext, policy);
	return PrepareRuntimeLoad(context, savePath, guard);
}

PackageSaveStateLoadResult RestorePreparedRuntimeSave(
	GameContext& context, PreparedRuntimeLoad& prepared) noexcept
{
	if (!prepared.packageRestorePending_) return {};
	if (prepared.policy_ == RuntimeSavePolicy::DedicatedDeterministic)
	{
		prepared.policyError = UnguardedStrictExecutionError(context);
		prepared.packageRandomTransactionError =
			PackageRandomTransactionError::None;
		return {PackageSaveStateError::EngineStateMismatch, {}, 0, 0};
	}

	RuntimeLoadExecutionGuard guard =
		BeginRuntimeLoadExecution(context, prepared.policy_);
	return RestorePreparedRuntimeSave(context, prepared, guard);
}

const char* RuntimeSaveContainerLoadErrorName(
	RuntimeSaveContainerLoadError error) noexcept
{
	switch (error)
	{
		case RuntimeSaveContainerLoadError::None: return "none";
		case RuntimeSaveContainerLoadError::NotFound: return "not-found";
		case RuntimeSaveContainerLoadError::InvalidOrUnsupported:
			return "invalid-or-unsupported";
		case RuntimeSaveContainerLoadError::TooLarge: return "too-large";
		case RuntimeSaveContainerLoadError::IntegrityFailure:
			return "integrity-failure";
		case RuntimeSaveContainerLoadError::StorageError: return "storage-error";
		case RuntimeSaveContainerLoadError::MalformedContainer:
			return "malformed-container";
		case RuntimeSaveContainerLoadError::TooManySections:
			return "too-many-sections";
		case RuntimeSaveContainerLoadError::DuplicateSection:
			return "duplicate-section";
	}
	return "invalid-or-unsupported";
}

const char* RuntimeCheckpointLoadErrorName(
	RuntimeCheckpointLoadError error) noexcept
{
	switch (error)
	{
		case RuntimeCheckpointLoadError::None: return "none";
		case RuntimeCheckpointLoadError::NotFound: return "not-found";
		case RuntimeCheckpointLoadError::InvalidOrUnsupported:
			return "invalid-or-unsupported";
		case RuntimeCheckpointLoadError::TooLarge: return "too-large";
		case RuntimeCheckpointLoadError::IntegrityFailure:
			return "integrity-failure";
		case RuntimeCheckpointLoadError::StorageError: return "storage-error";
		case RuntimeCheckpointLoadError::MalformedPayload:
			return "malformed-payload";
		case RuntimeCheckpointLoadError::TooManyPackages:
			return "too-many-packages";
		case RuntimeCheckpointLoadError::IncompatibleRuntime:
			return "incompatible-runtime";
	}
	return "invalid-or-unsupported";
}

const char* PackageSaveArchiveLoadErrorName(
	PackageSaveArchiveLoadError error) noexcept
{
	switch (error)
	{
		case PackageSaveArchiveLoadError::None: return "none";
		case PackageSaveArchiveLoadError::NotFound: return "not-found";
		case PackageSaveArchiveLoadError::InvalidOrUnsupported:
			return "invalid-or-unsupported";
		case PackageSaveArchiveLoadError::TooLarge: return "too-large";
		case PackageSaveArchiveLoadError::IntegrityFailure:
			return "integrity-failure";
		case PackageSaveArchiveLoadError::StorageError: return "storage-error";
		case PackageSaveArchiveLoadError::MalformedPayload:
			return "malformed-payload";
		case PackageSaveArchiveLoadError::TooManyRecords:
			return "too-many-records";
		case PackageSaveArchiveLoadError::PayloadTooLarge:
			return "payload-too-large";
		case PackageSaveArchiveLoadError::TotalTooLarge:
			return "total-too-large";
		case PackageSaveArchiveLoadError::DuplicatePackage:
			return "duplicate-package";
		case PackageSaveArchiveLoadError::IncompatibleRuntime:
			return "incompatible-runtime";
		case PackageSaveArchiveLoadError::TooManyRandomStreams:
			return "too-many-random-streams";
		case PackageSaveArchiveLoadError::DuplicateRandomStream:
			return "duplicate-random-stream";
	}
	return "invalid-or-unsupported";
}
