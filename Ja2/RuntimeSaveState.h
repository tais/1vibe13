#ifndef JA2_RUNTIME_SAVE_STATE_H
#define JA2_RUNTIME_SAVE_STATE_H

#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Core/PackageApi.h>
#include <Engine/Core/PackageSaveArchive.h>
#include <Engine/Core/RuntimeCheckpoint.h>
#include <Engine/Core/RuntimeRandomCheckpoint.h>
#include <Engine/Core/RuntimeSaveContainer.h>

class GameContext;
struct PreparedRuntimeSave;
struct RuntimeSaveCommitResult;
struct PreparedRuntimeLoad;

// Interactive preserves the legacy UI save/load contract. The dedicated
// policy is an opt-in deterministic contract: no caller can obtain it by
// merely loading an older interactive save.
enum class RuntimeSavePolicy : std::uint8_t
{
	Interactive,
	DedicatedDeterministic
};

enum class RuntimeSavePolicyError : std::uint8_t
{
	None,
	CanonicalSimulationRandomRequired,
	SimulationRandomUnhealthy,
	SimulationRandomChanged,
	SimulationRandomRestoreFailed,
	DeterministicBoundaryRequired,
	DeterministicBoundaryChanged,
	RuntimeCompatibilityChanged,
	PackageRandomHostSeedChanged,
	UnsupportedCheckpointVersion,
	UnsupportedPackageArchiveVersion,
	UnsupportedPackageRandomSchema,
	MissingRandomCheckpointSection,
	SemanticPackagePreflightRequired,
	BoundaryRestoreFailed,
	ExecutionGuardRequired,
	SimulationRandomConsumed,
	PackageRandomTransactionFailed
};

struct RuntimeExecutionGuardResult
{
	RuntimeSavePolicyError policyError = RuntimeSavePolicyError::None;
	PackageRandomTransactionError packageRandomTransactionError =
		PackageRandomTransactionError::None;

	explicit operator bool() const noexcept
	{
		return policyError == RuntimeSavePolicyError::None &&
			packageRandomTransactionError ==
				PackageRandomTransactionError::None;
	}
};

// Execution guards are separate from Prepared values so decoded/captured data
// remains inert. A strict guard spans the legacy domain serializer/loader and
// owns the process-local SimulationRandom and package RNG rollback transaction.
// Interactive guards are context-bound inert successes. Guards are
// coordinator-thread objects and must be finalized or destroyed before their
// bound GameContext.
class RuntimeSaveExecutionGuard
{
public:
	RuntimeSaveExecutionGuard() noexcept;
	~RuntimeSaveExecutionGuard();
	RuntimeSaveExecutionGuard(const RuntimeSaveExecutionGuard&) = delete;
	RuntimeSaveExecutionGuard& operator=(const RuntimeSaveExecutionGuard&) = delete;
	RuntimeSaveExecutionGuard(RuntimeSaveExecutionGuard&& other) noexcept;
	RuntimeSaveExecutionGuard& operator=(
		RuntimeSaveExecutionGuard&& other) = delete;

	explicit operator bool() const noexcept;
	const RuntimeExecutionGuardResult& beginResult() const noexcept
	{
		return beginResult_;
	}
	RuntimeSavePolicy policy() const noexcept { return policy_; }
	RuntimeExecutionGuardResult rollback() noexcept;

private:
	RuntimeSaveExecutionGuard(GameContext& context,
		RuntimeSavePolicy policy) noexcept;
	RuntimeExecutionGuardResult verifyUnchanged() const noexcept;
	RuntimeExecutionGuardResult commitUnchanged() noexcept;

	GameContext* context_ = nullptr;
	SimulationRandom* simulationRandom_ = nullptr;
	SimulationRandomCheckpoint simulationBaseline_;
	std::uint64_t simulationEpoch_ = 0;
	PackageRandomTransaction packageTransaction_;
	RuntimeExecutionGuardResult beginResult_{
		RuntimeSavePolicyError::ExecutionGuardRequired,
		PackageRandomTransactionError::None};
	RuntimeExecutionGuardResult finalResult_;
	RuntimeSavePolicy policy_ = RuntimeSavePolicy::Interactive;
	bool armed_ = false;
	bool finished_ = false;

	friend RuntimeSaveExecutionGuard BeginRuntimeSaveExecution(
		GameContext&, RuntimeSavePolicy) noexcept;
	friend PreparedRuntimeSave PrepareRuntimeSave(
		GameContext&, RuntimeSaveExecutionGuard&) noexcept;
	friend RuntimeSaveCommitResult CommitRuntimeSave(GameContext&,
		const std::string&, PreparedRuntimeSave,
		RuntimeSaveExecutionGuard&) noexcept;
};

class RuntimeLoadExecutionGuard
{
public:
	RuntimeLoadExecutionGuard() noexcept;
	~RuntimeLoadExecutionGuard();
	RuntimeLoadExecutionGuard(const RuntimeLoadExecutionGuard&) = delete;
	RuntimeLoadExecutionGuard& operator=(const RuntimeLoadExecutionGuard&) = delete;
	RuntimeLoadExecutionGuard(RuntimeLoadExecutionGuard&& other) noexcept;
	RuntimeLoadExecutionGuard& operator=(
		RuntimeLoadExecutionGuard&& other) = delete;

	explicit operator bool() const noexcept;
	const RuntimeExecutionGuardResult& beginResult() const noexcept
	{
		return beginResult_;
	}
	RuntimeSavePolicy policy() const noexcept { return policy_; }
	RuntimeExecutionGuardResult rollback() noexcept;

private:
	RuntimeLoadExecutionGuard(GameContext& context,
		RuntimeSavePolicy policy) noexcept;
	RuntimeExecutionGuardResult verifyBaseline() const noexcept;
	RuntimeExecutionGuardResult commitTarget(
		const std::vector<PackageEngineSaveStateRecord>& target) noexcept;

	GameContext* context_ = nullptr;
	SimulationRandom* simulationRandom_ = nullptr;
	SimulationRandomCheckpoint simulationBaseline_;
	std::uint64_t simulationEpoch_ = 0;
	PackageRandomTransaction packageTransaction_;
	RuntimeExecutionGuardResult beginResult_{
		RuntimeSavePolicyError::ExecutionGuardRequired,
		PackageRandomTransactionError::None};
	RuntimeExecutionGuardResult finalResult_;
	RuntimeSavePolicy policy_ = RuntimeSavePolicy::Interactive;
	bool armed_ = false;
	bool finished_ = false;

	friend RuntimeLoadExecutionGuard BeginRuntimeLoadExecution(
		GameContext&, RuntimeSavePolicy) noexcept;
	friend PreparedRuntimeLoad PrepareRuntimeLoad(const GameContext&,
		const std::string&, RuntimeLoadExecutionGuard&) noexcept;
	friend PackageSaveStateLoadResult RestorePreparedRuntimeSave(
		GameContext&, PreparedRuntimeLoad&, RuntimeLoadExecutionGuard&) noexcept;
};

// Captured once at the paused game boundary, before the domain serializer
// starts. The immutable checkpoint and package state are sealed into the same
// .sav only after the domain writer closes successfully.
struct PreparedRuntimeSave
{
	PreparedRuntimeSave() = default;
	PreparedRuntimeSave(const PreparedRuntimeSave&) = delete;
	PreparedRuntimeSave& operator=(const PreparedRuntimeSave&) = delete;
	PreparedRuntimeSave(PreparedRuntimeSave&&) = default;
	PreparedRuntimeSave& operator=(PreparedRuntimeSave&&) = default;

	RuntimeCheckpoint checkpoint;
	PackageSaveStateSnapshot packageState;
	RuntimeRandomCheckpoint randomCheckpoint;
	RuntimeSavePolicyError policyError = RuntimeSavePolicyError::None;
	RuntimeCheckpointSaveError checkpointError =
		RuntimeCheckpointSaveError::InvalidCheckpoint;
	PackageSaveStateError packageCaptureError =
		PackageSaveStateError::RuntimeNotReady;
	PackageRandomTransactionError packageRandomTransactionError =
		PackageRandomTransactionError::None;
	std::string packageId;

	explicit operator bool() const
	{
		return policyError == RuntimeSavePolicyError::None &&
			checkpointError == RuntimeCheckpointSaveError::None &&
			packageCaptureError == PackageSaveStateError::None &&
			packageRandomTransactionError ==
				PackageRandomTransactionError::None;
	}
	RuntimeSavePolicy policy() const noexcept { return policy_; }

private:
	RuntimeSavePolicy policy_ = RuntimeSavePolicy::Interactive;

	friend PreparedRuntimeSave PrepareRuntimeSave(
		GameContext&, RuntimeSavePolicy) noexcept;
	friend PreparedRuntimeSave PrepareRuntimeSave(
		GameContext&, RuntimeSaveExecutionGuard&) noexcept;
	friend RuntimeSaveCommitResult CommitRuntimeSave(GameContext&,
		const std::string&, PreparedRuntimeSave) noexcept;
	friend RuntimeSaveCommitResult CommitRuntimeSave(GameContext&,
		const std::string&, PreparedRuntimeSave,
		RuntimeSaveExecutionGuard&) noexcept;
};

struct RuntimeSaveCommitResult
{
	RuntimeSavePolicy policy = RuntimeSavePolicy::Interactive;
	RuntimeCheckpointSaveError checkpointError =
		RuntimeCheckpointSaveError::InvalidCheckpoint;
	RuntimeRandomCheckpointSaveError randomCheckpointError =
		RuntimeRandomCheckpointSaveError::None;
	RuntimeSavePolicyError policyError = RuntimeSavePolicyError::None;
	PackageSaveStateError packageCaptureError =
		PackageSaveStateError::RuntimeNotReady;
	PackageRandomTransactionError packageRandomTransactionError =
		PackageRandomTransactionError::None;
	PackageSaveArchiveSaveError packageArchiveError =
		PackageSaveArchiveSaveError::None;
	RuntimeSaveContainerSaveError containerError =
		RuntimeSaveContainerSaveError::None;
	std::string packageId;

	explicit operator bool() const
	{
		return policyError == RuntimeSavePolicyError::None &&
			checkpointError == RuntimeCheckpointSaveError::None &&
			randomCheckpointError == RuntimeRandomCheckpointSaveError::None &&
			packageCaptureError == PackageSaveStateError::None &&
			packageRandomTransactionError ==
				PackageRandomTransactionError::None &&
			packageArchiveError == PackageSaveArchiveSaveError::None &&
			containerError == RuntimeSaveContainerSaveError::None;
	}
};

struct PreparedRuntimeLoad
{
	PreparedRuntimeLoad() = default;
	PreparedRuntimeLoad(const PreparedRuntimeLoad&) = delete;
	PreparedRuntimeLoad& operator=(const PreparedRuntimeLoad&) = delete;
	PreparedRuntimeLoad(PreparedRuntimeLoad&&) = default;
	PreparedRuntimeLoad& operator=(PreparedRuntimeLoad&&) = default;

	RuntimeSaveContainerLoadError containerError =
		RuntimeSaveContainerLoadError::InvalidOrUnsupported;
	RuntimeCheckpointLoadError checkpointError =
		RuntimeCheckpointLoadError::InvalidOrUnsupported;
	RuntimeRandomCheckpointLoadError randomCheckpointError =
		RuntimeRandomCheckpointLoadError::None;
	RuntimeSavePolicyError policyError = RuntimeSavePolicyError::None;
	RuntimeCheckpointBoundaryResult checkpointBoundary;
	PackageSaveArchiveLoadError packageArchiveError =
		PackageSaveArchiveLoadError::InvalidOrUnsupported;
	PackageSaveStateError packageContractError =
		PackageSaveStateError::RuntimeNotReady;
	PackageRandomTransactionError packageRandomTransactionError =
		PackageRandomTransactionError::None;
	std::uint64_t domainBytes = 0;
	RuntimeCheckpoint checkpoint;
	PackageSaveArchive packages;
	std::string packageId;

	explicit operator bool() const
	{
		return policyError == RuntimeSavePolicyError::None &&
			containerError == RuntimeSaveContainerLoadError::None &&
			checkpointError == RuntimeCheckpointLoadError::None &&
			randomCheckpointError == RuntimeRandomCheckpointLoadError::None &&
			packageArchiveError == PackageSaveArchiveLoadError::None &&
			packageContractError == PackageSaveStateError::None &&
			packageRandomTransactionError ==
				PackageRandomTransactionError::None;
	}
	bool restorePending() const { return packageRestorePending_; }
	RuntimeSavePolicy policy() const noexcept { return policy_; }
	const RuntimeRandomCheckpoint& runtimeRandomCheckpoint() const noexcept
	{
		return randomCheckpoint_;
	}

private:
	RuntimeSavePolicy policy_ = RuntimeSavePolicy::Interactive;
	bool packageRestorePending_ = false;
	RuntimeRandomCheckpoint randomCheckpoint_;

	friend PreparedRuntimeLoad PrepareRuntimeLoad(
		const GameContext&, const std::string&, RuntimeSavePolicy) noexcept;
	friend PreparedRuntimeLoad PrepareRuntimeLoad(const GameContext&,
		const std::string&, RuntimeLoadExecutionGuard&) noexcept;
	friend PackageSaveStateLoadResult RestorePreparedRuntimeSave(
		GameContext&, PreparedRuntimeLoad&) noexcept;
	friend PackageSaveStateLoadResult RestorePreparedRuntimeSave(
		GameContext&, PreparedRuntimeLoad&, RuntimeLoadExecutionGuard&) noexcept;
};

RuntimeSaveExecutionGuard BeginRuntimeSaveExecution(GameContext& context,
	RuntimeSavePolicy policy = RuntimeSavePolicy::Interactive) noexcept;
PreparedRuntimeSave PrepareRuntimeSave(GameContext& context,
	RuntimeSaveExecutionGuard& guard) noexcept;
RuntimeSaveCommitResult CommitRuntimeSave(GameContext& context,
	const std::string& savePath, PreparedRuntimeSave prepared,
	RuntimeSaveExecutionGuard& guard) noexcept;

// Compatibility entry points keep interactive behavior. Strict callers must
// hold the explicit execution guard across the legacy domain serializer.
PreparedRuntimeSave PrepareRuntimeSave(GameContext& context,
	RuntimeSavePolicy policy = RuntimeSavePolicy::Interactive) noexcept;
RuntimeSaveCommitResult CommitRuntimeSave(GameContext& context,
	const std::string& savePath, PreparedRuntimeSave prepared) noexcept;

// Strict preflight for the one-file runtime format. A missing container,
// corrupt domain prefix, incompatible runtime, or invalid package contract is
// rejected before the live tactical/strategic state is dismantled.
PreparedRuntimeLoad PrepareRuntimeLoad(
	const GameContext& context, const std::string& savePath,
	RuntimeSavePolicy policy = RuntimeSavePolicy::Interactive) noexcept;
RuntimeLoadExecutionGuard BeginRuntimeLoadExecution(GameContext& context,
	RuntimeSavePolicy policy = RuntimeSavePolicy::Interactive) noexcept;
PreparedRuntimeLoad PrepareRuntimeLoad(
	const GameContext& context, const std::string& savePath,
	RuntimeLoadExecutionGuard& guard) noexcept;
PackageSaveStateLoadResult RestorePreparedRuntimeSave(
	GameContext& context, PreparedRuntimeLoad& prepared) noexcept;
PackageSaveStateLoadResult RestorePreparedRuntimeSave(
	GameContext& context, PreparedRuntimeLoad& prepared,
	RuntimeLoadExecutionGuard& guard) noexcept;

const char* RuntimeSaveContainerLoadErrorName(
	RuntimeSaveContainerLoadError error) noexcept;
const char* RuntimeCheckpointLoadErrorName(
	RuntimeCheckpointLoadError error) noexcept;
const char* PackageSaveArchiveLoadErrorName(
	PackageSaveArchiveLoadError error) noexcept;

#endif
