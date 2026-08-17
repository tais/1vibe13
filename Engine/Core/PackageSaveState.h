#ifndef ENGINE_CORE_PACKAGE_SAVE_STATE_H
#define ENGINE_CORE_PACKAGE_SAVE_STATE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Core/PackageRandomSource.h>

// Value-only state captured from an active package for one game save. Package
// code owns the payload schema; the engine owns identity, bounds, ordering,
// transport, and compatibility checks around it.
struct PackageSaveStateRecord
{
	std::string packageId;
	std::string packageVersion;
	std::uint32_t schemaVersion = 0;
	std::vector<std::uint8_t> payload;
};

// Engine-owned package state is kept separate from the package-defined opaque
// payload. New engine services can extend this record without forcing every
// package to rev its domain save schema or callback implementation.
struct PackageEngineSaveStateRecord
{
	std::string packageId;
	std::string packageVersion;
	PackageRandomCheckpoint random;

	bool operator==(const PackageEngineSaveStateRecord& other) const
	{
		return packageId == other.packageId &&
			packageVersion == other.packageVersion && random == other.random;
	}

	bool operator!=(const PackageEngineSaveStateRecord& other) const
	{
		return !(*this == other);
	}
};

// Process-local evidence captured around a strict runtime save/load boundary.
// The activation-ordered engine records are deterministic state; the epoch is
// deliberately non-serializable and only proves that no registry-derived RNG
// source consumed randomness while a transaction was armed.
struct PackageRandomTransactionStamp
{
	std::uint64_t consumptionEpoch = 0;
	std::vector<PackageEngineSaveStateRecord> engineRecords;

	bool operator==(const PackageRandomTransactionStamp& other) const
	{
		return consumptionEpoch == other.consumptionEpoch &&
			engineRecords == other.engineRecords;
	}

	bool operator!=(const PackageRandomTransactionStamp& other) const
	{
		return !(*this == other);
	}
};

enum class PackageRandomTransactionError
{
	None,
	RuntimeNotReady,
	OperationInProgress,
	TransactionAlreadyActive,
	TooManyPackages,
	IdentityMismatch,
	VersionMismatch,
	InvalidCheckpoint,
	StateChanged,
	RandomConsumed,
	AllocationFailure,
	InvariantViolation
};

struct PackageRandomTransactionResult
{
	PackageRandomTransactionError error = PackageRandomTransactionError::None;
	std::string packageId;

	explicit operator bool() const
	{
		return error == PackageRandomTransactionError::None;
	}
};

struct PackageSaveStateSnapshot
{
	std::vector<PackageSaveStateRecord> records;
	std::vector<PackageEngineSaveStateRecord> engineRecords;

	const PackageSaveStateRecord* find(const std::string& packageId) const
	{
		for (const PackageSaveStateRecord& record : records)
			if (record.packageId == packageId) return &record;
		return nullptr;
	}

	const PackageEngineSaveStateRecord* findEngine(
		const std::string& packageId) const
	{
		for (const PackageEngineSaveStateRecord& record : engineRecords)
			if (record.packageId == packageId) return &record;
		return nullptr;
	}
};

// Interactive saves preserve the established behavior: package callback RNG
// draws are rolled back with the capture/load transaction. Deterministic
// dedicated saves additionally reject successful PackageRandomSource::next()
// draws made on the synchronous callback thread, including through retained
// or directly copy/move-derived values. Read-only checkpoint inspection
// remains allowed; other random services require their own coordinator-level
// guard.
enum class PackageSaveRandomPolicy
{
	AllowAndRollback,
	RequireUnconsumed
};

enum class PackageSaveStateError
{
	None,
	RuntimeNotReady,
	OperationInProgress,
	TooManyRecords,
	IdentityMismatch,
	VersionMismatch,
	SchemaMismatch,
	PayloadTooLarge,
	TotalTooLarge,
	ValidationFailed,
	CallbackFailed,
	AllocationFailure,
	EngineStateMismatch,
	RandomConsumed
};

struct PackageSaveStateCaptureResult
{
	PackageSaveStateError error = PackageSaveStateError::None;
	std::string packageId;
	PackageSaveStateSnapshot snapshot;

	explicit operator bool() const { return error == PackageSaveStateError::None; }
};

struct PackageSaveStateLoadResult
{
	PackageSaveStateError error = PackageSaveStateError::None;
	std::string packageId;
	std::size_t restored = 0;
	std::size_t engineRecordsRestored = 0;

	explicit operator bool() const { return error == PackageSaveStateError::None; }
};

#endif
