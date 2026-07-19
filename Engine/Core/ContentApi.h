#ifndef ENGINE_CORE_CONTENT_API_H
#define ENGINE_CORE_CONTENT_API_H

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
// package requirements. Older manifests remain valid when they do not depend
// on those optional contracts.
constexpr ContentApiVersion CurrentContentApiVersion{1, 2};
constexpr ContentApiVersion PackageRequirementsContentApiVersion{1, 2};

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
};

enum class ContentRegistrationError
{
	None,
	InvalidManifest,
	IncompatibleApi,
	DuplicateId,
	InvalidRequirement
};

class ContentRegistry
{
public:
	explicit ContentRegistry(ContentApiVersion supportedApi) : supportedApi_(supportedApi) {}

	ContentRegistrationError registerContent(ContentManifest manifest)
	{
		if (!IsValidEngineIdentifier(manifest.id) || manifest.version.empty())
			return ContentRegistrationError::InvalidManifest;
		if (manifest.requiredApi.major != supportedApi_.major || manifest.requiredApi.minor > supportedApi_.minor)
			return ContentRegistrationError::IncompatibleApi;
		if (!manifest.requirements.empty() &&
			(manifest.requiredApi.major != PackageRequirementsContentApiVersion.major ||
			 manifest.requiredApi.minor < PackageRequirementsContentApiVersion.minor))
			return ContentRegistrationError::InvalidRequirement;
		std::unordered_set<std::string> requirementIds;
		requirementIds.reserve(manifest.requirements.size());
		for (const ContentRequirement& requirement : manifest.requirements)
		{
			if (!IsValidEngineIdentifier(requirement.id) || requirement.id == manifest.id ||
				!requirementIds.insert(requirement.id).second)
				return ContentRegistrationError::InvalidRequirement;
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
