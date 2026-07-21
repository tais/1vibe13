#include "SaveCompatibility.h"

#include "GameContext.h"

#include <vfs/Tools/vfs_property_container.h>

#include <cctype>
#include <utility>

namespace
{
constexpr const char* CheckpointSuffix = ".engine-checkpoint";

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
