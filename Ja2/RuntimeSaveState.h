#ifndef JA2_RUNTIME_SAVE_STATE_H
#define JA2_RUNTIME_SAVE_STATE_H

#include <cstdint>
#include <string>

#include <Engine/Core/PackageSaveArchive.h>
#include <Engine/Core/RuntimeCheckpoint.h>
#include <Engine/Core/RuntimeSaveContainer.h>

class GameContext;

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
	RuntimeCheckpointSaveError checkpointError =
		RuntimeCheckpointSaveError::InvalidCheckpoint;
	PackageSaveStateError packageCaptureError =
		PackageSaveStateError::RuntimeNotReady;
	std::string packageId;

	explicit operator bool() const
	{
		return checkpointError == RuntimeCheckpointSaveError::None &&
			packageCaptureError == PackageSaveStateError::None;
	}
};

struct RuntimeSaveCommitResult
{
	RuntimeCheckpointSaveError checkpointError =
		RuntimeCheckpointSaveError::InvalidCheckpoint;
	PackageSaveStateError packageCaptureError =
		PackageSaveStateError::RuntimeNotReady;
	PackageSaveArchiveSaveError packageArchiveError =
		PackageSaveArchiveSaveError::None;
	RuntimeSaveContainerSaveError containerError =
		RuntimeSaveContainerSaveError::None;
	std::string packageId;

	explicit operator bool() const
	{
		return checkpointError == RuntimeCheckpointSaveError::None &&
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
		return containerError == RuntimeSaveContainerLoadError::None &&
			checkpointError == RuntimeCheckpointLoadError::None &&
			packageArchiveError == PackageSaveArchiveLoadError::None &&
			packageContractError == PackageSaveStateError::None;
	}
	bool restorePending() const { return packageRestorePending_; }

private:
	bool packageRestorePending_ = false;

	friend PreparedRuntimeLoad PrepareRuntimeLoad(
		const GameContext&, const std::string&) noexcept;
	friend PackageSaveStateLoadResult RestorePreparedRuntimeSave(
		GameContext&, PreparedRuntimeLoad&) noexcept;
};

PreparedRuntimeSave PrepareRuntimeSave(GameContext& context) noexcept;
RuntimeSaveCommitResult CommitRuntimeSave(GameContext& context,
	const std::string& savePath, PreparedRuntimeSave prepared) noexcept;

// Strict preflight for the one-file runtime format. A missing container,
// corrupt domain prefix, incompatible runtime, or invalid package contract is
// rejected before the live tactical/strategic state is dismantled.
PreparedRuntimeLoad PrepareRuntimeLoad(
	const GameContext& context, const std::string& savePath) noexcept;
PackageSaveStateLoadResult RestorePreparedRuntimeSave(
	GameContext& context, PreparedRuntimeLoad& prepared) noexcept;

const char* RuntimeSaveContainerLoadErrorName(
	RuntimeSaveContainerLoadError error) noexcept;
const char* RuntimeCheckpointLoadErrorName(
	RuntimeCheckpointLoadError error) noexcept;
const char* PackageSaveArchiveLoadErrorName(
	PackageSaveArchiveLoadError error) noexcept;

#endif
