#ifndef JA2_SAVE_COMPATIBILITY_H
#define JA2_SAVE_COMPATIBILITY_H

#include <string>

#include <Engine/Core/RuntimeCheckpoint.h>
#include <Engine/Core/PackageSaveArchive.h>

namespace vfs
{
class PropertyContainer;
}

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

enum class PackageSaveMetadataState
{
	Ready,
	NotRequired,
	LegacyWithoutMetadata,
	IncompatibleRuntime,
	InvalidMetadata,
	PackageContractMismatch,
	StorageError
};

struct PackageSaveMetadataWriteResult
{
	bool stateful = false;
	PackageSaveStateError captureError = PackageSaveStateError::None;
	PackageSaveArchiveSaveError archiveError = PackageSaveArchiveSaveError::None;

	explicit operator bool() const
	{
		return captureError == PackageSaveStateError::None &&
			archiveError == PackageSaveArchiveSaveError::None;
	}
};

struct PackageSaveMetadataResult
{
	PackageSaveMetadataState state = PackageSaveMetadataState::Ready;
	std::string sidecarPath;
	PackageSaveArchive archive;
	PackageSaveArchiveLoadError archiveError = PackageSaveArchiveLoadError::None;
	PackageSaveStateError contractError = PackageSaveStateError::None;

	SaveCompatibilityState compatibilityState() const noexcept;
};

// Unknown values retain the caller-supplied fallback. Canonical values are
// ignore, warn, enforce-known, and require-metadata.
SaveCompatibilityPolicy ParseSaveCompatibilityPolicy(
	const std::string& value,
	SaveCompatibilityPolicy fallback = SaveCompatibilityPolicy::Warn) noexcept;
const char* SaveCompatibilityPolicyName(SaveCompatibilityPolicy policy) noexcept;
const char* SaveCompatibilityStateName(SaveCompatibilityState state) noexcept;
const char* PackageSaveMetadataStateName(PackageSaveMetadataState state) noexcept;

SaveCompatibilityPolicy ReadSaveCompatibilityPolicy(
	vfs::PropertyContainer& properties, int argc, char* const* argv);
void ConfigureSaveCompatibilityPolicy(SaveCompatibilityPolicy policy) noexcept;
SaveCompatibilityPolicy GetSaveCompatibilityPolicy() noexcept;

// EnforceKnown is the compatibility-preserving strict mode: it rejects
// metadata that is known to be incompatible or invalid, but old saves without
// metadata continue to load. RequireMetadata is a separate explicit opt-in.
SaveCompatibilityLoadAction EvaluateSaveCompatibility(
	SaveCompatibilityState state, SaveCompatibilityPolicy policy) noexcept;
SaveCompatibilityLoadAction EvaluatePackageSaveMetadata(
	PackageSaveMetadataState state, SaveCompatibilityPolicy policy) noexcept;

// The legacy .sav bytes remain untouched. New engine identity/progress metadata
// lives beside them under a deterministic suffix and can be ignored by older
// builds without changing their save-slot discovery.
std::string RuntimeCheckpointSidecarPath(const std::string& savePath);
std::string PackageSaveStateSidecarPath(const std::string& savePath);

RuntimeCheckpointSaveError WriteSaveCompatibilityMetadata(
	const GameContext& context, const std::string& savePath) noexcept;

SaveCompatibilityResult InspectSaveCompatibilityMetadata(
	const GameContext& context, const std::string& savePath) noexcept;

PackageSaveMetadataWriteResult WritePackageSaveStateMetadata(
	GameContext& context, const std::string& savePath) noexcept;
PackageSaveMetadataResult InspectPackageSaveStateMetadata(
	const GameContext& context, const std::string& savePath) noexcept;

// Idempotently removes metadata when its owning legacy save is deleted.
bool RemoveSaveCompatibilityMetadata(
	GameContext& context, const std::string& savePath) noexcept;

#endif
