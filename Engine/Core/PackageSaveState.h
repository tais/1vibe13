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
};

struct PackageSaveStateSnapshot
{
	std::vector<PackageSaveStateRecord> records;
	std::vector<PackageEngineSaveStateRecord> engineRecords;
	// False identifies legacy archive payloads that predate engine-owned state.
	// An explicitly present empty section is distinct and valid for no packages.
	bool engineStatePresent = false;

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
	EngineStateMismatch
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
