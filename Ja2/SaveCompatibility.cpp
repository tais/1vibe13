#include "SaveCompatibility.h"

#include "GameContext.h"

#include <cctype>
#include <utility>

namespace
{
constexpr const char* CheckpointSuffix = ".engine-checkpoint";
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
