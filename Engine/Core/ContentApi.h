#ifndef ENGINE_CORE_CONTENT_API_H
#define ENGINE_CORE_CONTENT_API_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Engine/Core/Identifier.h>

struct ContentApiVersion
{
	std::uint16_t major;
	std::uint16_t minor;
};

// 1.1 adds package-owned AssetSource overlays; 1.2 adds exact or unversioned
// package requirements; 1.3 adds optional dependencies, conflicts, and weak
// ordering relationships; 1.4 adds declarative localization and definition
// sources. Older manifests remain valid when they do not use newer contracts.
constexpr ContentApiVersion CurrentContentApiVersion{1, 4};
constexpr ContentApiVersion PackageRequirementsContentApiVersion{1, 2};
constexpr ContentApiVersion PackagePolicyContentApiVersion{1, 3};
constexpr ContentApiVersion PackageDeclaredContentApiVersion{1, 4};

struct ContentRequirement
{
	std::string id;
	// Versions are deliberately opaque and case-sensitive for now. Empty means
	// any registered version; non-empty means exact equality, not SemVer.
	std::string exactVersion;
};

struct ContentManifest
{
	std::string id;
	std::string version;
	ContentApiVersion requiredApi;
	std::vector<ContentRequirement> requirements;
	// Optional requirements participate in the activation closure only when the
	// target is registered. If present, their version and lifecycle guarantees
	// are identical to mandatory requirements.
	std::vector<ContentRequirement> optionalRequirements;
	// Conflicts are symmetric during resolution even when only one side declares
	// the relationship. Missing targets are harmless.
	std::vector<std::string> conflicts;
	// Weak ordering edges never select another package. When both packages are
	// in an activation plan, the named package must activate first.
	std::vector<std::string> loadAfter;
};

enum class ContentRegistrationError
{
	None,
	InvalidManifest,
	IncompatibleApi,
	DuplicateId,
	InvalidRequirement,
	InvalidRelationship
};

enum class ContentUnregistrationError
{
	None,
	NotFound
};

class ContentRegistry
{
public:
	explicit ContentRegistry(ContentApiVersion supportedApi) : supportedApi_(supportedApi) {}

	ContentRegistrationError registerContent(ContentManifest manifest)
	{
		if (!IsValidEngineIdentifier(manifest.id) || manifest.version.empty() ||
			manifest.version.size() > MaximumEngineVersionBytes)
			return ContentRegistrationError::InvalidManifest;
		if (manifest.requiredApi.major != supportedApi_.major || manifest.requiredApi.minor > supportedApi_.minor)
			return ContentRegistrationError::IncompatibleApi;
		if (!manifest.requirements.empty() &&
			(manifest.requiredApi.major != PackageRequirementsContentApiVersion.major ||
			 manifest.requiredApi.minor < PackageRequirementsContentApiVersion.minor))
			return ContentRegistrationError::InvalidRequirement;
		const bool hasPolicy = !manifest.optionalRequirements.empty() ||
			!manifest.conflicts.empty() || !manifest.loadAfter.empty();
		if (hasPolicy &&
			(manifest.requiredApi.major != PackagePolicyContentApiVersion.major ||
			 manifest.requiredApi.minor < PackagePolicyContentApiVersion.minor))
			return ContentRegistrationError::InvalidRelationship;
		std::unordered_set<std::string> relationshipIds;
		relationshipIds.reserve(manifest.requirements.size() +
			manifest.optionalRequirements.size() + manifest.conflicts.size() +
			manifest.loadAfter.size());
		for (const ContentRequirement& requirement : manifest.requirements)
		{
			if (!IsValidEngineIdentifier(requirement.id) || requirement.id == manifest.id ||
				requirement.exactVersion.size() > MaximumEngineVersionBytes ||
				!relationshipIds.insert(requirement.id).second)
				return ContentRegistrationError::InvalidRequirement;
		}
		for (const ContentRequirement& requirement : manifest.optionalRequirements)
		{
			if (!IsValidEngineIdentifier(requirement.id) || requirement.id == manifest.id ||
				requirement.exactVersion.size() > MaximumEngineVersionBytes ||
				!relationshipIds.insert(requirement.id).second)
				return ContentRegistrationError::InvalidRelationship;
		}
		for (const std::string& conflict : manifest.conflicts)
		{
			if (!IsValidEngineIdentifier(conflict) || conflict == manifest.id ||
				!relationshipIds.insert(conflict).second)
				return ContentRegistrationError::InvalidRelationship;
		}
		for (const std::string& predecessor : manifest.loadAfter)
		{
			if (!IsValidEngineIdentifier(predecessor) || predecessor == manifest.id ||
				!relationshipIds.insert(predecessor).second)
				return ContentRegistrationError::InvalidRelationship;
		}
		if (byId_.find(manifest.id) != byId_.end()) return ContentRegistrationError::DuplicateId;
		const std::size_t index = manifests_.size();
		manifests_.push_back(std::move(manifest));
		try
		{
			const auto inserted = byId_.emplace(manifests_.back().id, index);
			if (!inserted.second)
			{
				manifests_.pop_back();
				return ContentRegistrationError::DuplicateId;
			}
		}
		catch (...)
		{
			manifests_.pop_back();
			throw;
		}
		return ContentRegistrationError::None;
	}

	ContentUnregistrationError unregisterContent(const std::string& id)
	{
		const auto found = byId_.find(id);
		if (found == byId_.end()) return ContentUnregistrationError::NotFound;
		const std::size_t removedIndex = found->second;
		byId_.erase(found);
		manifests_.erase(manifests_.begin() + static_cast<std::ptrdiff_t>(removedIndex));
		// Preserve discovery order for hosts that expose manifests to tools while
		// repairing the compact-vector index after the erased entry.
		for (std::size_t index = removedIndex; index < manifests_.size(); ++index)
			byId_.at(manifests_[index].id) = index;
		return ContentUnregistrationError::None;
	}

	const ContentManifest* find(const std::string& id) const
	{
		const auto found = byId_.find(id);
		return found == byId_.end() ? nullptr : &manifests_[found->second];
	}

	ContentApiVersion supportedApi() const { return supportedApi_; }
	const std::vector<ContentManifest>& manifests() const { return manifests_; }

private:
	ContentApiVersion supportedApi_;
	std::vector<ContentManifest> manifests_;
	std::unordered_map<std::string, std::size_t> byId_;
};

#endif
