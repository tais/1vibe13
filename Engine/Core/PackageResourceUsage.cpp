#include <Engine/Core/PackageResourceUsage.h>

#include <limits>
#include <unordered_map>

namespace
{
void AddSaturating(std::uint64_t& destination, std::uint64_t value)
{
	const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
	destination = value > maximum - destination ? maximum : destination + value;
}

void AddUsage(PackageResourceUsage& destination, const PackageResourceUsage& source)
{
	AddSaturating(destination.localizationEntries, source.localizationEntries);
	AddSaturating(destination.localizationTextBytes, source.localizationTextBytes);
	AddSaturating(destination.definitionEntries, source.definitionEntries);
	AddSaturating(destination.definitionPayloadBytes, source.definitionPayloadBytes);
	AddSaturating(destination.entities, source.entities);
	AddSaturating(destination.audioPlaybacks, source.audioPlaybacks);
	AddSaturating(destination.deferredTasks, source.deferredTasks);
	AddSaturating(destination.randomStreams, source.randomStreams);
	AddSaturating(destination.randomValuesGenerated, source.randomValuesGenerated);
}
}

PackageResourceUsageSnapshot BuildPackageResourceUsage(
	const PackageCatalogSnapshot& packages,
	const std::vector<LocalizationEntry>& localization,
	const std::vector<DefinitionRecord>& definitions,
	const std::vector<EntityRecordSnapshot>& entities,
	const std::vector<PackageAudioPlaybackSnapshot>& audio,
	const PackageTaskQueueSnapshot& tasks,
	const std::vector<PackageRandomUsageSnapshot>& random)
{
	PackageResourceUsageSnapshot result;
	result.packages.reserve(packages.packages.size());
	std::unordered_map<std::string, std::size_t> byId;
	byId.reserve(packages.packages.size());
	for (const PackageCatalogEntry& package : packages.packages)
	{
		const std::string& packageId = package.descriptor.content.id;
		byId.emplace(packageId, result.packages.size());
		result.packages.push_back(PackageResourceUsage{packageId, package.active()});
	}

	auto find = [&result, &byId](const std::string& packageId) -> PackageResourceUsage*
	{
		const auto found = byId.find(packageId);
		return found == byId.end() ? nullptr : &result.packages[found->second];
	};
	for (const LocalizationEntry& entry : localization)
	{
		PackageResourceUsage* usage = find(entry.packageId);
		if (!usage) { ++result.unattributedRecords; continue; }
		++usage->localizationEntries;
		AddSaturating(usage->localizationTextBytes, entry.text.size());
	}
	for (const DefinitionRecord& definition : definitions)
	{
		PackageResourceUsage* usage = find(definition.packageId);
		if (!usage) { ++result.unattributedRecords; continue; }
		++usage->definitionEntries;
		AddSaturating(usage->definitionPayloadBytes, definition.payload.size());
	}
	for (const EntityRecordSnapshot& entity : entities)
	{
		PackageResourceUsage* usage = find(entity.ownerPackageId);
		if (usage) ++usage->entities;
		else ++result.unattributedRecords;
	}
	for (const PackageAudioPlaybackSnapshot& playback : audio)
	{
		PackageResourceUsage* usage = find(playback.packageId);
		if (usage) ++usage->audioPlaybacks;
		else ++result.unattributedRecords;
	}
	for (const PackageTaskRecord& task : tasks.queued)
	{
		PackageResourceUsage* usage = find(task.packageId);
		if (usage) ++usage->deferredTasks;
		else ++result.unattributedRecords;
	}
	for (const PackageRandomUsageSnapshot& packageRandom : random)
	{
		PackageResourceUsage* usage = find(packageRandom.packageId);
		if (!usage) { ++result.unattributedRecords; continue; }
		usage->randomStreams = packageRandom.streams;
		usage->randomValuesGenerated = packageRandom.valuesGenerated;
	}
	for (const PackageResourceUsage& usage : result.packages)
		AddUsage(result.total, usage);
	return result;
}
