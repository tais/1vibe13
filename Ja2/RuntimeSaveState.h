#ifndef JA2_RUNTIME_SAVE_STATE_H
#define JA2_RUNTIME_SAVE_STATE_H

#include <cstdint>
#include <string>

#include <Engine/Core/PackageSaveArchive.h>
#include <Engine/Core/RuntimeCheckpoint.h>
#include <Engine/Core/RuntimeRandomCheckpoint.h>
#include <Engine/Core/RuntimeSaveContainer.h>

class GameContext;

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
	BoundaryRestoreFailed
};

struct RuntimeSaveCommitResult;

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
	std::string packageId;

	explicit operator bool() const
	{
		return policyError == RuntimeSavePolicyError::None &&
			checkpointError == RuntimeCheckpointSaveError::None &&
			packageCaptureError == PackageSaveStateError::None;
	}
	RuntimeSavePolicy policy() const noexcept { return policy_; }

private:
	RuntimeSavePolicy policy_ = RuntimeSavePolicy::Interactive;

	friend PreparedRuntimeSave PrepareRuntimeSave(
		GameContext&, RuntimeSavePolicy) noexcept;
	friend RuntimeSaveCommitResult CommitRuntimeSave(GameContext&,
		const std::string&, PreparedRuntimeSave) noexcept;
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
			packageContractError == PackageSaveStateError::None;
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
	bool hasLiveSimulationSentinel_ = false;
	SimulationRandomCheckpoint liveSimulationSentinel_;
	RuntimeRandomCheckpoint randomCheckpoint_;

	friend PreparedRuntimeLoad PrepareRuntimeLoad(
		const GameContext&, const std::string&, RuntimeSavePolicy) noexcept;
	friend PackageSaveStateLoadResult RestorePreparedRuntimeSave(
		GameContext&, PreparedRuntimeLoad&) noexcept;
};

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
PackageSaveStateLoadResult RestorePreparedRuntimeSave(
	GameContext& context, PreparedRuntimeLoad& prepared) noexcept;

const char* RuntimeSaveContainerLoadErrorName(
	RuntimeSaveContainerLoadError error) noexcept;
const char* RuntimeCheckpointLoadErrorName(
	RuntimeCheckpointLoadError error) noexcept;
const char* PackageSaveArchiveLoadErrorName(
	PackageSaveArchiveLoadError error) noexcept;

#endif
