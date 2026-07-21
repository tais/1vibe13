#ifndef ENGINE_CORE_PACKAGE_RESOURCE_USAGE_H
#define ENGINE_CORE_PACKAGE_RESOURCE_USAGE_H

#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Core/AudioGroupService.h>
#include <Engine/Core/DefinitionCatalog.h>
#include <Engine/Core/EntityRegistry.h>
#include <Engine/Core/LocalizationCatalog.h>
#include <Engine/Core/PackageCatalog.h>
#include <Engine/Core/PackageTaskQueue.h>

struct PackageRandomUsageSnapshot
{
	std::string packageId;
	std::uint64_t streams = 0;
	std::uint64_t valuesGenerated = 0;
};

struct PackageResourceUsage
{
	std::string packageId;
	bool active = false;
	std::uint64_t localizationEntries = 0;
	std::uint64_t localizationTextBytes = 0;
	std::uint64_t definitionEntries = 0;
	std::uint64_t definitionPayloadBytes = 0;
	std::uint64_t entities = 0;
	std::uint64_t audioPlaybacks = 0;
	std::uint64_t deferredTasks = 0;
	std::uint64_t randomStreams = 0;
	std::uint64_t randomValuesGenerated = 0;
};

struct PackageResourceUsageSnapshot
{
	std::vector<PackageResourceUsage> packages;
	PackageResourceUsage total;
	std::uint64_t unattributedRecords = 0;

	const PackageResourceUsage* find(const std::string& packageId) const
	{
		for (const PackageResourceUsage& usage : packages)
			if (usage.packageId == packageId) return &usage;
		return nullptr;
	}
};

PackageResourceUsageSnapshot BuildPackageResourceUsage(
	const PackageCatalogSnapshot& packages,
	const std::vector<LocalizationEntry>& localization,
	const std::vector<DefinitionRecord>& definitions,
	const std::vector<EntityRecordSnapshot>& entities,
	const std::vector<PackageAudioPlaybackSnapshot>& audio,
	const PackageTaskQueueSnapshot& tasks,
	const std::vector<PackageRandomUsageSnapshot>& random);

#endif
