#ifndef ENGINE_CORE_PACKAGE_SAVE_STATE_H
#define ENGINE_CORE_PACKAGE_SAVE_STATE_H

#include <cstdint>
#include <string>
#include <vector>

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

struct PackageSaveStateSnapshot
{
	std::vector<PackageSaveStateRecord> records;

	const PackageSaveStateRecord* find(const std::string& packageId) const
	{
		for (const PackageSaveStateRecord& record : records)
			if (record.packageId == packageId) return &record;
		return nullptr;
	}
};

#endif
