#include "SaveCompatibility.h"

#include "GameContext.h"

#include <vfs/Tools/vfs_property_container.h>

#include <cctype>
#include <utility>

namespace
{
constexpr const char* CheckpointSuffix = ".engine-checkpoint";
constexpr const char* PackageStateSuffix = ".engine-packages";

SaveCompatibilityPolicy& ConfiguredPolicy()
{
	static SaveCompatibilityPolicy policy = SaveCompatibilityPolicy::Warn;
	return policy;
}

bool IsSeparateOptionValue(const char* value)
{
	if (!value || value[0] == '\0' || value[0] == '-') return false;
#ifdef _WIN32
	if (value[0] == '/') return false;
#endif
	return true;
}

std::string LowerAscii(std::string value)
{
	for (char& character : value)
		if (character >= 'A' && character <= 'Z')
			character = static_cast<char>(character - 'A' + 'a');
	return value;
}
}

SaveCompatibilityPolicy ParseSaveCompatibilityPolicy(
	const std::string& value, SaveCompatibilityPolicy fallback) noexcept
{
	try
	{
		std::string normalized;
		normalized.reserve(value.size());
		for (unsigned char character : value)
		{
			if (std::isspace(character)) continue;
			normalized.push_back(static_cast<char>(std::tolower(character)));
		}
		if (normalized == "ignore" || normalized == "off")
			return SaveCompatibilityPolicy::Ignore;
		if (normalized == "warn") return SaveCompatibilityPolicy::Warn;
		if (normalized == "enforce-known" || normalized == "enforce_known" ||
			normalized == "enforce")
			return SaveCompatibilityPolicy::EnforceKnown;
		if (normalized == "require-metadata" || normalized == "require_metadata" ||
			normalized == "require")
			return SaveCompatibilityPolicy::RequireMetadata;
	}
	catch (...) {}
	return fallback;
}

const char* SaveCompatibilityPolicyName(SaveCompatibilityPolicy policy) noexcept
{
	switch (policy)
	{
		case SaveCompatibilityPolicy::Ignore: return "ignore";
		case SaveCompatibilityPolicy::Warn: return "warn";
		case SaveCompatibilityPolicy::EnforceKnown: return "enforce-known";
		case SaveCompatibilityPolicy::RequireMetadata: return "require-metadata";
	}
	return "warn";
}

const char* SaveCompatibilityStateName(SaveCompatibilityState state) noexcept
{
	switch (state)
	{
		case SaveCompatibilityState::Compatible: return "compatible";
		case SaveCompatibilityState::LegacyWithoutMetadata: return "legacy-without-metadata";
		case SaveCompatibilityState::IncompatibleRuntime: return "incompatible-runtime";
		case SaveCompatibilityState::InvalidMetadata: return "invalid-metadata";
		case SaveCompatibilityState::StorageError: return "storage-error";
	}
	return "invalid-metadata";
}

const char* PackageSaveMetadataStateName(PackageSaveMetadataState state) noexcept
{
	switch (state)
	{
		case PackageSaveMetadataState::Ready: return "ready";
		case PackageSaveMetadataState::NotRequired: return "not-required";
		case PackageSaveMetadataState::LegacyWithoutMetadata:
			return "legacy-without-package-state";
		case PackageSaveMetadataState::IncompatibleRuntime: return "incompatible-runtime";
		case PackageSaveMetadataState::InvalidMetadata: return "invalid-metadata";
		case PackageSaveMetadataState::PackageContractMismatch:
			return "package-contract-mismatch";
		case PackageSaveMetadataState::StorageError: return "storage-error";
	}
	return "invalid-metadata";
}

SaveCompatibilityState PackageSaveMetadataResult::compatibilityState() const noexcept
{
	switch (state)
	{
		case PackageSaveMetadataState::Ready:
		case PackageSaveMetadataState::NotRequired:
			return SaveCompatibilityState::Compatible;
		case PackageSaveMetadataState::LegacyWithoutMetadata:
			return SaveCompatibilityState::LegacyWithoutMetadata;
		case PackageSaveMetadataState::IncompatibleRuntime:
			return SaveCompatibilityState::IncompatibleRuntime;
		case PackageSaveMetadataState::InvalidMetadata:
		case PackageSaveMetadataState::PackageContractMismatch:
			return SaveCompatibilityState::InvalidMetadata;
		case PackageSaveMetadataState::StorageError:
			return SaveCompatibilityState::StorageError;
	}
	return SaveCompatibilityState::InvalidMetadata;
}

SaveCompatibilityPolicy ReadSaveCompatibilityPolicy(
	vfs::PropertyContainer& properties, int argc, char* const* argv)
{
	SaveCompatibilityPolicy policy = SaveCompatibilityPolicy::Warn;
	vfs::String configured;
	if (properties.getStringProperty(
		L"Ja2 Settings", L"SAVE_COMPATIBILITY_POLICY", configured))
		policy = ParseSaveCompatibilityPolicy(configured.utf8(), policy);
	for (int index = 1; index < argc; ++index)
	{
		if (!argv || !argv[index]) continue;
		std::string option = argv[index];
		std::size_t prefix = 0;
		if (option.compare(0, 2, "--") == 0) prefix = 2;
		else if (!option.empty() && (option[0] == '-' || option[0] == '/')) prefix = 1;
		else continue;
		option.erase(0, prefix);
		const std::size_t separator = option.find_first_of("=:");
		const std::string key = LowerAscii(option.substr(0, separator));
		if (key == "no-save-compatibility")
		{
			policy = SaveCompatibilityPolicy::Ignore;
			continue;
		}
		if (key != "save-compatibility") continue;
		std::string value = separator == std::string::npos
			? std::string{} : option.substr(separator + 1);
		if (value.empty() && index + 1 < argc && IsSeparateOptionValue(argv[index + 1]))
			value = argv[++index];
		policy = ParseSaveCompatibilityPolicy(value, policy);
	}
	return policy;
}

void ConfigureSaveCompatibilityPolicy(SaveCompatibilityPolicy policy) noexcept
{
	ConfiguredPolicy() = policy;
}

SaveCompatibilityPolicy GetSaveCompatibilityPolicy() noexcept
{
	return ConfiguredPolicy();
}

SaveCompatibilityLoadAction EvaluateSaveCompatibility(
	SaveCompatibilityState state, SaveCompatibilityPolicy policy) noexcept
{
	if (policy == SaveCompatibilityPolicy::Ignore ||
		state == SaveCompatibilityState::Compatible)
		return SaveCompatibilityLoadAction::Allow;
	if (state == SaveCompatibilityState::LegacyWithoutMetadata)
		return policy == SaveCompatibilityPolicy::RequireMetadata
			? SaveCompatibilityLoadAction::Reject
			: SaveCompatibilityLoadAction::Allow;
	if (policy == SaveCompatibilityPolicy::Warn)
		return SaveCompatibilityLoadAction::AllowWithWarning;
	if (state == SaveCompatibilityState::StorageError &&
		policy == SaveCompatibilityPolicy::EnforceKnown)
		return SaveCompatibilityLoadAction::AllowWithWarning;
	return SaveCompatibilityLoadAction::Reject;
}

SaveCompatibilityLoadAction EvaluatePackageSaveMetadata(
	PackageSaveMetadataState state, SaveCompatibilityPolicy policy) noexcept
{
	if (policy == SaveCompatibilityPolicy::Ignore ||
		state == PackageSaveMetadataState::Ready ||
		state == PackageSaveMetadataState::NotRequired)
		return SaveCompatibilityLoadAction::Allow;
	if (state == PackageSaveMetadataState::LegacyWithoutMetadata)
		return policy == SaveCompatibilityPolicy::RequireMetadata
			? SaveCompatibilityLoadAction::Reject
			: SaveCompatibilityLoadAction::AllowWithWarning;
	if (policy == SaveCompatibilityPolicy::Warn ||
		(state == PackageSaveMetadataState::StorageError &&
		 policy == SaveCompatibilityPolicy::EnforceKnown))
		return SaveCompatibilityLoadAction::AllowWithWarning;
	return SaveCompatibilityLoadAction::Reject;
}

std::string RuntimeCheckpointSidecarPath(const std::string& savePath)
{
	return savePath.empty() ? std::string{} : savePath + CheckpointSuffix;
}

std::string PackageSaveStateSidecarPath(const std::string& savePath)
{
	return savePath.empty() ? std::string{} : savePath + PackageStateSuffix;
}

PreparedSaveMetadata PrepareSaveMetadata(GameContext& context) noexcept
{
	PreparedSaveMetadata prepared;
	try
	{
		prepared.checkpoint = context.runtime().makeRuntimeCheckpoint();
		prepared.checkpointError = RuntimeCheckpointSaveError::None;
		PackageSaveStateCaptureResult captured = context.capturePackageSaveState();
		prepared.packageCaptureError = captured.error;
		prepared.packageId = std::move(captured.packageId);
		if (captured)
		{
			prepared.stateful = !captured.snapshot.records.empty() ||
				!captured.snapshot.engineRecords.empty();
			prepared.packageState = std::move(captured.snapshot);
		}
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

PreparedSaveMetadataCommitResult CommitPreparedSaveMetadata(
	GameContext& context, const std::string& savePath,
	PreparedSaveMetadata prepared) noexcept
{
	PreparedSaveMetadataCommitResult result;
	result.checkpointError = prepared.checkpointError;
	result.packages.stateful = prepared.stateful;
	result.packages.captureError = prepared.packageCaptureError;
	result.packageId = std::move(prepared.packageId);
	try
	{
		const std::string checkpointPath = RuntimeCheckpointSidecarPath(savePath);
		const std::string packagePath = PackageSaveStateSidecarPath(savePath);
		if (checkpointPath.empty() || packagePath.empty())
		{
			result.checkpointError = RuntimeCheckpointSaveError::InvalidCheckpoint;
			result.packages.archiveError = PackageSaveArchiveSaveError::InvalidArchive;
			return result;
		}
		if (!prepared)
		{
			RemoveSaveCompatibilityMetadata(context, savePath);
			return result;
		}

		result.checkpointError = context.runtime().runtimeCheckpoints().save(
			checkpointPath, prepared.checkpoint);
		if (result.checkpointError != RuntimeCheckpointSaveError::None)
		{
			RemoveSaveCompatibilityMetadata(context, savePath);
			return result;
		}

		if (prepared.stateful)
		{
			result.packages.archiveError = context.packageSaveArchives().save(
				packagePath, PackageSaveArchive{
					prepared.checkpoint.compatibility,
					std::move(prepared.packageState)});
		}
		else if (!context.persistence().storage().remove(packagePath))
			result.packages.archiveError = PackageSaveArchiveSaveError::StorageError;

		if (result.packages.archiveError != PackageSaveArchiveSaveError::None)
			RemoveSaveCompatibilityMetadata(context, savePath);
		return result;
	}
	catch (...)
	{
		result.packages.archiveError = PackageSaveArchiveSaveError::StorageError;
		RemoveSaveCompatibilityMetadata(context, savePath);
		return result;
	}
}

PreparedLoadMetadata PrepareLoadMetadata(const GameContext& context,
	const std::string& savePath, SaveCompatibilityPolicy policy) noexcept
{
	PreparedLoadMetadata prepared;
	prepared.policy = policy;
	if (policy == SaveCompatibilityPolicy::Ignore)
	{
		prepared.compatibility.state = SaveCompatibilityState::Compatible;
		prepared.packages.state = PackageSaveMetadataState::NotRequired;
		return prepared;
	}
	prepared.compatibility = InspectSaveCompatibilityMetadata(context, savePath);
	prepared.compatibilityAction = EvaluateSaveCompatibility(
		prepared.compatibility.state, policy);
	prepared.packages = InspectPackageSaveStateMetadata(context, savePath);
	prepared.packageAction = EvaluatePackageSaveMetadata(
		prepared.packages.state, policy);
	prepared.packageRestorePending =
		prepared.packages.state == PackageSaveMetadataState::Ready &&
		prepared.packageAction != SaveCompatibilityLoadAction::Reject &&
		prepared.compatibilityAction != SaveCompatibilityLoadAction::Reject;
	return prepared;
}

PackageSaveStateLoadResult RestorePreparedPackageSaveState(
	GameContext& context, PreparedLoadMetadata& prepared) noexcept
{
	if (!prepared.packageRestorePending)
		return {};
	prepared.packageRestorePending = false;
	PackageSaveStateSnapshot snapshot = std::move(prepared.packages.archive.state);
	return context.restorePackageSaveState(snapshot);
}

RuntimeCheckpointSaveError WriteSaveCompatibilityMetadata(
	const GameContext& context, const std::string& savePath) noexcept
{
	try
	{
		const std::string sidecar = RuntimeCheckpointSidecarPath(savePath);
		return sidecar.empty()
			? RuntimeCheckpointSaveError::InvalidCheckpoint
			: context.runtime().saveRuntimeCheckpoint(sidecar);
	}
	catch (...)
	{
		return RuntimeCheckpointSaveError::StorageError;
	}
}

SaveCompatibilityResult InspectSaveCompatibilityMetadata(
	const GameContext& context, const std::string& savePath) noexcept
{
	SaveCompatibilityResult result;
	try
	{
		result.sidecarPath = RuntimeCheckpointSidecarPath(savePath);
		if (result.sidecarPath.empty())
		{
			result.state = SaveCompatibilityState::InvalidMetadata;
			return result;
		}
		RuntimeCheckpoint checkpoint;
		const RuntimeCheckpointLoadResult loaded =
			context.runtime().loadRuntimeCheckpoint(result.sidecarPath, checkpoint);
		result.storedCompatibility = loaded.storedCompatibility;
		switch (loaded.error)
		{
			case RuntimeCheckpointLoadError::None:
				result.state = SaveCompatibilityState::Compatible;
				result.checkpoint = std::move(checkpoint);
				break;
			case RuntimeCheckpointLoadError::NotFound:
				result.state = SaveCompatibilityState::LegacyWithoutMetadata;
				break;
			case RuntimeCheckpointLoadError::IncompatibleRuntime:
				result.state = SaveCompatibilityState::IncompatibleRuntime;
				break;
			case RuntimeCheckpointLoadError::StorageError:
				result.state = SaveCompatibilityState::StorageError;
				break;
			case RuntimeCheckpointLoadError::InvalidOrUnsupported:
			case RuntimeCheckpointLoadError::TooLarge:
			case RuntimeCheckpointLoadError::IntegrityFailure:
			case RuntimeCheckpointLoadError::MalformedPayload:
			case RuntimeCheckpointLoadError::TooManyPackages:
				result.state = SaveCompatibilityState::InvalidMetadata;
				break;
		}
		return result;
	}
	catch (...)
	{
		result.state = SaveCompatibilityState::StorageError;
		return result;
	}
}

PackageSaveMetadataWriteResult WritePackageSaveStateMetadata(
	GameContext& context, const std::string& savePath) noexcept
{
	try
	{
		const std::string sidecar = PackageSaveStateSidecarPath(savePath);
		if (sidecar.empty())
			return {false, PackageSaveStateError::None,
				PackageSaveArchiveSaveError::InvalidArchive};
		PackageSaveStateCaptureResult captured = context.capturePackageSaveState();
		if (!captured)
			return {false, captured.error, PackageSaveArchiveSaveError::None};
		if (captured.snapshot.records.empty() &&
			captured.snapshot.engineRecords.empty())
		{
			const bool removed = context.persistence().storage().remove(sidecar);
			return {false, PackageSaveStateError::None,
				removed ? PackageSaveArchiveSaveError::None
				        : PackageSaveArchiveSaveError::StorageError};
		}
		const PackageSaveArchiveSaveError saved = context.packageSaveArchives().save(
			sidecar, PackageSaveArchive{
				context.runtime().compatibilityFingerprint(), std::move(captured.snapshot)});
		if (saved != PackageSaveArchiveSaveError::None)
			context.persistence().storage().remove(sidecar);
		return {true, PackageSaveStateError::None, saved};
	}
	catch (...)
	{
		return {false, PackageSaveStateError::AllocationFailure,
			PackageSaveArchiveSaveError::StorageError};
	}
}

PackageSaveMetadataResult InspectPackageSaveStateMetadata(
	const GameContext& context, const std::string& savePath) noexcept
{
	PackageSaveMetadataResult result;
	try
	{
		result.sidecarPath = PackageSaveStateSidecarPath(savePath);
		if (result.sidecarPath.empty())
		{
			result.state = PackageSaveMetadataState::InvalidMetadata;
			return result;
		}
		PackageSaveArchive archive;
		const PackageSaveArchiveLoadResult loaded = context.packageSaveArchives().load(
			result.sidecarPath, context.runtime().compatibilityFingerprint(),
			context.runtime().preAggregateCatalogCompatibilityFingerprint(), archive);
		result.archiveError = loaded.error;
		if (loaded.error == PackageSaveArchiveLoadError::NotFound)
		{
			if (context.requiresPackageEngineSaveState())
			{
				result.state = PackageSaveMetadataState::LegacyWithoutMetadata;
				return result;
			}
			const PackageSaveStateLoadResult emptyContract =
				context.validatePackageSaveState(PackageSaveStateSnapshot{});
			result.contractError = emptyContract.error;
			result.state = emptyContract
				? PackageSaveMetadataState::NotRequired
				: PackageSaveMetadataState::LegacyWithoutMetadata;
			return result;
		}
		if (loaded.error == PackageSaveArchiveLoadError::IncompatibleRuntime)
		{
			result.state = PackageSaveMetadataState::IncompatibleRuntime;
			return result;
		}
		if (loaded.error == PackageSaveArchiveLoadError::StorageError)
		{
			result.state = PackageSaveMetadataState::StorageError;
			return result;
		}
		if (!loaded)
		{
			result.state = PackageSaveMetadataState::InvalidMetadata;
			return result;
		}
		const PackageSaveStateLoadResult contract =
			context.validatePackageSaveState(archive.state);
		result.contractError = contract.error;
		if (!contract)
		{
			result.state = PackageSaveMetadataState::PackageContractMismatch;
			return result;
		}
		result.state = PackageSaveMetadataState::Ready;
		result.archive = std::move(archive);
		return result;
	}
	catch (...)
	{
		result.state = PackageSaveMetadataState::StorageError;
		return result;
	}
}

bool RemoveSaveCompatibilityMetadata(
	GameContext& context, const std::string& savePath) noexcept
{
	try
	{
		const std::string sidecar = RuntimeCheckpointSidecarPath(savePath);
		const std::string packageState = PackageSaveStateSidecarPath(savePath);
		if (sidecar.empty() || packageState.empty())
			return false;
		const bool checkpointRemoved = context.persistence().storage().remove(sidecar);
		const bool packageStateRemoved = context.persistence().storage().remove(packageState);
		return checkpointRemoved && packageStateRemoved;
	}
	catch (...)
	{
		return false;
	}
}
