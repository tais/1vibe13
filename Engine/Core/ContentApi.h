#ifndef ENGINE_CORE_CONTENT_API_H
#define ENGINE_CORE_CONTENT_API_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Engine/Core/Identifier.h>

struct ContentApiVersion
{
	std::uint16_t major;
	std::uint16_t minor;
};

// 1.1 adds package-owned AssetSource overlays. Version 1.0 manifests remain
// compatible when they do not depend on that optional lifecycle contract.
constexpr ContentApiVersion CurrentContentApiVersion{1, 1};

struct ContentManifest
{
	std::string id;
	std::string version;
	ContentApiVersion requiredApi;
};

enum class ContentRegistrationError
{
	None,
	InvalidManifest,
	IncompatibleApi,
	DuplicateId
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
