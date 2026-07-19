#ifndef ENGINE_CORE_CONTENT_API_H
#define ENGINE_CORE_CONTENT_API_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct ContentApiVersion
{
	std::uint16_t major;
	std::uint16_t minor;
};

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
		if (manifest.id.empty() || manifest.version.empty()) return ContentRegistrationError::InvalidManifest;
		if (manifest.requiredApi.major != supportedApi_.major || manifest.requiredApi.minor > supportedApi_.minor)
			return ContentRegistrationError::IncompatibleApi;
		if (byId_.find(manifest.id) != byId_.end()) return ContentRegistrationError::DuplicateId;
		const std::size_t index = manifests_.size();
		byId_.emplace(manifest.id, index);
		manifests_.push_back(std::move(manifest));
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
