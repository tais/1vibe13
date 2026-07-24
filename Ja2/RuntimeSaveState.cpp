#include "RuntimeSaveState.h"

#include "GameContext.h"

#include <utility>
#include <vector>

namespace
{
constexpr std::uint32_t RuntimeCheckpointSection = 0x504b4843u; // "CHKP"
constexpr std::uint32_t PackageStateSection = 0x54534750u; // "PGST"

void RemoveIncompleteSave(GameContext& context, const std::string& path) noexcept
{
	try { context.persistence().storage().remove(path); } catch (...) {}
}
}

PreparedRuntimeSave PrepareRuntimeSave(GameContext& context) noexcept
{
	PreparedRuntimeSave prepared;
	try
	{
		prepared.checkpoint = context.runtime().makeRuntimeCheckpoint();
		prepared.checkpointError = RuntimeCheckpointSaveError::None;
		PackageSaveStateCaptureResult captured = context.capturePackageSaveState();
		prepared.packageCaptureError = captured.error;
		prepared.packageId = std::move(captured.packageId);
		if (captured)
			prepared.packageState = std::move(captured.snapshot);
	}
	catch (...)
	{
		if (prepared.checkpointError != RuntimeCheckpointSaveError::None)
			prepared.checkpointError = RuntimeCheckpointSaveError::StorageError;
		else if (prepared.packageCaptureError != PackageSaveStateError::None)
			prepared.packageCaptureError = PackageSaveStateError::AllocationFailure;
	}
	return prepared;
}

RuntimeSaveCommitResult CommitRuntimeSave(GameContext& context,
	const std::string& savePath, PreparedRuntimeSave prepared) noexcept
{
	RuntimeSaveCommitResult result;
	result.checkpointError = prepared.checkpointError;
	result.packageCaptureError = prepared.packageCaptureError;
	result.packageId = std::move(prepared.packageId);
	if (savePath.empty())
	{
		result.containerError = RuntimeSaveContainerSaveError::InvalidRequest;
		return result;
	}
	if (!prepared)
	{
		RemoveIncompleteSave(context, savePath);
		return result;
	}
	try
	{
		std::vector<std::uint8_t> checkpointBytes;
		result.checkpointError = context.runtime().runtimeCheckpoints().encode(
			prepared.checkpoint, checkpointBytes);
		if (result.checkpointError != RuntimeCheckpointSaveError::None)
		{
			RemoveIncompleteSave(context, savePath);
			return result;
		}

		std::vector<std::uint8_t> packageBytes;
		result.packageArchiveError = context.packageSaveArchives().encode(
			PackageSaveArchive{
				prepared.checkpoint.compatibility,
				std::move(prepared.packageState)},
			packageBytes);
		if (result.packageArchiveError != PackageSaveArchiveSaveError::None)
		{
			RemoveIncompleteSave(context, savePath);
			return result;
		}

		std::vector<RuntimeSaveSection> sections;
		sections.reserve(2);
		sections.push_back(
			RuntimeSaveSection{RuntimeCheckpointSection, std::move(checkpointBytes)});
		sections.push_back(
			RuntimeSaveSection{PackageStateSection, std::move(packageBytes)});
		result.containerError =
			context.runtimeSaveContainers().seal(savePath, sections);
		if (result.containerError != RuntimeSaveContainerSaveError::None)
			RemoveIncompleteSave(context, savePath);
		return result;
	}
	catch (...)
	{
		result.containerError = RuntimeSaveContainerSaveError::StorageError;
		RemoveIncompleteSave(context, savePath);
		return result;
	}
}

PreparedRuntimeLoad PrepareRuntimeLoad(
	const GameContext& context, const std::string& savePath) noexcept
{
	PreparedRuntimeLoad prepared;
	try
	{
		RuntimeSaveContainer container;
		const RuntimeSaveContainerLoadResult loaded =
			context.runtimeSaveContainers().inspect(savePath, container);
		prepared.containerError = loaded.error;
		if (!loaded) return prepared;
		prepared.domainBytes = container.domainBytes;

		const RuntimeSaveSection* checkpoint =
			container.find(RuntimeCheckpointSection);
		const RuntimeSaveSection* packages =
			container.find(PackageStateSection);
		if (!checkpoint || !packages)
		{
			prepared.containerError =
				RuntimeSaveContainerLoadError::MalformedContainer;
			return prepared;
		}

		const RuntimeCompatibilityFingerprint compatibility =
			context.runtime().compatibilityFingerprint();
		const RuntimeCheckpointLoadResult checkpointResult =
			context.runtime().runtimeCheckpoints().decode(
				checkpoint->payload, compatibility, prepared.checkpoint);
		prepared.checkpointError = checkpointResult.error;
		if (!checkpointResult) return prepared;

		const PackageSaveArchiveLoadResult packageResult =
			context.packageSaveArchives().decode(
				packages->payload, compatibility, prepared.packages);
		prepared.packageArchiveError = packageResult.error;
		if (!packageResult) return prepared;

		const PackageSaveStateLoadResult contract =
			context.validatePackageSaveState(prepared.packages.state);
		prepared.packageContractError = contract.error;
		prepared.packageId = std::move(contract.packageId);
		if (!contract) return prepared;
		prepared.packageRestorePending_ = true;
		return prepared;
	}
	catch (...)
	{
		prepared.containerError = RuntimeSaveContainerLoadError::StorageError;
		return prepared;
	}
}

PackageSaveStateLoadResult RestorePreparedRuntimeSave(
	GameContext& context, PreparedRuntimeLoad& prepared) noexcept
{
	if (!prepared.packageRestorePending_) return {};
	prepared.packageRestorePending_ = false;
	PackageSaveStateSnapshot snapshot = std::move(prepared.packages.state);
	return context.restorePackageSaveState(snapshot);
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
