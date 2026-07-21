#ifndef JA2_SAVE_COMPATIBILITY_H
#define JA2_SAVE_COMPATIBILITY_H

#include <string>

#include <Engine/Core/RuntimeCheckpoint.h>

class GameContext;

enum class SaveCompatibilityState
{
	Compatible,
	LegacyWithoutMetadata,
	IncompatibleRuntime,
	InvalidMetadata,
	StorageError
};

enum class SaveCompatibilityPolicy
{
	Ignore,
	Warn,
	EnforceKnown,
	RequireMetadata
};

enum class SaveCompatibilityLoadAction
{
	Allow,
	AllowWithWarning,
	Reject
};

struct SaveCompatibilityResult
{
	SaveCompatibilityState state = SaveCompatibilityState::Compatible;
	std::string sidecarPath;
	RuntimeCheckpoint checkpoint;
	RuntimeCompatibilityFingerprint storedCompatibility;

	bool permitsCompatibleLoad() const
	{
		return state == SaveCompatibilityState::Compatible ||
			state == SaveCompatibilityState::LegacyWithoutMetadata;
	}
};

// Unknown values retain the caller-supplied fallback. Canonical values are
// ignore, warn, enforce-known, and require-metadata.
SaveCompatibilityPolicy ParseSaveCompatibilityPolicy(
	const std::string& value,
	SaveCompatibilityPolicy fallback = SaveCompatibilityPolicy::Warn) noexcept;
const char* SaveCompatibilityPolicyName(SaveCompatibilityPolicy policy) noexcept;

// EnforceKnown is the compatibility-preserving strict mode: it rejects
// metadata that is known to be incompatible or invalid, but old saves without
// metadata continue to load. RequireMetadata is a separate explicit opt-in.
SaveCompatibilityLoadAction EvaluateSaveCompatibility(
	SaveCompatibilityState state, SaveCompatibilityPolicy policy) noexcept;

// The legacy .sav bytes remain untouched. New engine identity/progress metadata
// lives beside them under a deterministic suffix and can be ignored by older
// builds without changing their save-slot discovery.
std::string RuntimeCheckpointSidecarPath(const std::string& savePath);

RuntimeCheckpointSaveError WriteSaveCompatibilityMetadata(
	const GameContext& context, const std::string& savePath) noexcept;

SaveCompatibilityResult InspectSaveCompatibilityMetadata(
	const GameContext& context, const std::string& savePath) noexcept;

// Idempotently removes metadata when its owning legacy save is deleted.
bool RemoveSaveCompatibilityMetadata(
	GameContext& context, const std::string& savePath) noexcept;

#endif
