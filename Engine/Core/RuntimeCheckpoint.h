#ifndef ENGINE_CORE_RUNTIME_CHECKPOINT_H
#define ENGINE_CORE_RUNTIME_CHECKPOINT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Core/FrameDriver.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeFingerprint.h>
#include <Engine/Core/SimulationTick.h>

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

	// Version 2 adds the complete deterministic engine boundary while retaining
	// the original counters above as inexpensive metadata. The duplicated values
	// are checked on both encode and decode instead of being independently trusted.
	FrameDriverBoundaryState frameBoundary;
	SimulationTickBoundaryState simulationTickBoundary;
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
	std::uint16_t storedVersion = 0;
	bool hasDeterministicBoundary = false;

	explicit operator bool() const { return error == RuntimeCheckpointLoadError::None; }
};

enum class RuntimeCheckpointBoundaryError : std::uint8_t
{
	None,
	MissingDeterministicBoundary,
	CompletedFrameMismatch,
	CompletedSimulationTickMismatch,
	FrameStateRejected,
	SimulationTickStateRejected,
	AllocationFailure
};

// The component errors remain available so a caller can distinguish an active
// operation from malformed or incompatible state without weakening the single
// aggregate success contract.
struct RuntimeCheckpointBoundaryResult
{
	RuntimeCheckpointBoundaryError error =
		RuntimeCheckpointBoundaryError::MissingDeterministicBoundary;
	FrameDriverBoundaryStateError frameError =
		FrameDriverBoundaryStateError::InvalidNextFrameSequence;
	SimulationTickBoundaryStateError simulationTickError =
		SimulationTickBoundaryStateError::InvalidConfiguration;

	explicit operator bool() const noexcept
	{
		return error == RuntimeCheckpointBoundaryError::None;
	}
};

struct RuntimeCheckpointCaptureResult
{
	RuntimeCheckpointBoundaryResult boundary;
	RuntimeCheckpoint checkpoint;

	explicit operator bool() const noexcept { return static_cast<bool>(boundary); }
};

// Persists only the portable runtime identity and progress boundary. Domain
// state remains in versioned game/package serializers, which can reject an
// incompatible manifest before decoding legacy layouts.
class RuntimeCheckpointService
{
public:
	static constexpr std::uint16_t LegacyVersion = 1;
	static constexpr std::uint16_t CurrentVersion = 2;

	explicit RuntimeCheckpointService(
		PersistenceService& persistence, std::size_t maximumPackages = 4096)
		: persistence_(persistence), maximumPackages_(maximumPackages) {}

	RuntimeCheckpointSaveError save(
		const std::string& path, const RuntimeCheckpoint& checkpoint) const noexcept;
	RuntimeCheckpointSaveError encode(const RuntimeCheckpoint& checkpoint,
		std::vector<std::uint8_t>& encoded) const noexcept;
	// Interactive legacy saves are initiated synchronously from inside the
	// application frame and therefore cannot claim a committed restorable
	// boundary. Preserve their CHKP v1 metadata record explicitly; dedicated
	// deterministic checkpoints must use encode() and CHKP v2.
	RuntimeCheckpointSaveError encodeLegacyMetadata(
		const RuntimeCheckpoint& checkpoint,
		std::vector<std::uint8_t>& encoded) const noexcept;
	RuntimeCheckpointLoadResult load(const std::string& path,
		RuntimeCompatibilityFingerprint expectedCompatibility,
		RuntimeCheckpoint& checkpoint) const noexcept;
	RuntimeCheckpointLoadResult decode(const std::vector<std::uint8_t>& encoded,
		RuntimeCompatibilityFingerprint expectedCompatibility,
		RuntimeCheckpoint& checkpoint) const noexcept;

	std::size_t maximumPackages() const { return maximumPackages_; }

private:
	PersistenceService& persistence_;
	std::size_t maximumPackages_;
};

#endif
