#include "SaveCompatibility.h"

#include "GameContext.h"

#include <utility>

namespace
{
constexpr const char* CheckpointSuffix = ".engine-checkpoint";
}

std::string RuntimeCheckpointSidecarPath(const std::string& savePath)
{
	return savePath.empty() ? std::string{} : savePath + CheckpointSuffix;
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

bool RemoveSaveCompatibilityMetadata(
	GameContext& context, const std::string& savePath) noexcept
{
	try
	{
		const std::string sidecar = RuntimeCheckpointSidecarPath(savePath);
		return !sidecar.empty() && context.persistence().storage().remove(sidecar);
	}
	catch (...)
	{
		return false;
	}
}
