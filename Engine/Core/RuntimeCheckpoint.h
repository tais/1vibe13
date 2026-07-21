#ifndef ENGINE_CORE_RUNTIME_CHECKPOINT_H
#define ENGINE_CORE_RUNTIME_CHECKPOINT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeFingerprint.h>

struct RuntimeCheckpointPackage
{
	std::string id;
	std::string version;
};

struct RuntimeCheckpoint
{
	RuntimeCompatibilityFingerprint compatibility;
	std::uint64_t completedFrames = 0;
	std::uint64_t completedSimulationTicks = 0;
	std::vector<RuntimeCheckpointPackage> activePackages;
};

enum class RuntimeCheckpointSaveError
{
	None,
	InvalidCheckpoint,
	TooManyPackages,
	TooLarge,
	StorageError
};

enum class RuntimeCheckpointLoadError
{
	None,
	NotFound,
	InvalidOrUnsupported,
	TooLarge,
	IntegrityFailure,
	StorageError,
	MalformedPayload,
	TooManyPackages,
	IncompatibleRuntime
};

struct RuntimeCheckpointLoadResult
{
	RuntimeCheckpointLoadError error = RuntimeCheckpointLoadError::None;
	RuntimeCompatibilityFingerprint storedCompatibility;

	explicit operator bool() const { return error == RuntimeCheckpointLoadError::None; }
};

// Persists only the portable runtime identity and progress boundary. Domain
// state remains in versioned game/package serializers, which can reject an
// incompatible manifest before decoding legacy layouts.
class RuntimeCheckpointService
{
public:
	explicit RuntimeCheckpointService(
		PersistenceService& persistence, std::size_t maximumPackages = 4096)
		: persistence_(persistence), maximumPackages_(maximumPackages) {}

	RuntimeCheckpointSaveError save(
		const std::string& path, const RuntimeCheckpoint& checkpoint) const noexcept;
	RuntimeCheckpointLoadResult load(const std::string& path,
		RuntimeCompatibilityFingerprint expectedCompatibility,
		RuntimeCheckpoint& checkpoint) const noexcept;

	std::size_t maximumPackages() const { return maximumPackages_; }

private:
	PersistenceService& persistence_;
	std::size_t maximumPackages_;
};

#endif
