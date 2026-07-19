#ifndef ENGINE_CORE_PACKAGE_API_H
#define ENGINE_CORE_PACKAGE_API_H

#include <string>
#include <unordered_map>
#include <vector>

#include <Engine/Core/ContentApi.h>

enum class PackageKind
{
	Campaign,
	Rules,
	Extension,
	Tool
};

struct PackageDescriptor
{
	ContentManifest content;
	PackageKind kind;
};

// Packages are owned by the application and must outlive the registry. The
// callbacks are deliberately engine-only lifecycle hooks: platform and
// campaign-specific services are supplied by adapters outside Engine/Core.
class EnginePackage
{
public:
	virtual ~EnginePackage() = default;
	virtual const PackageDescriptor& descriptor() const = 0;
	virtual bool activate() = 0;
	virtual void deactivate() = 0;
};

enum class PackageRegistrationError
{
	None,
	InvalidManifest,
	IncompatibleApi,
	DuplicateId
};

enum class PackageActivationError
{
	None,
	NotFound,
	AlreadyActive,
	CampaignAlreadyActive,
	ActivationFailed
};

class PackageRegistry
{
public:
	explicit PackageRegistry(ContentRegistry& content) : content_(content) {}

	PackageRegistrationError registerPackage(EnginePackage& package)
	{
		const std::string& id = package.descriptor().content.id;
		if (packages_.find(id) != packages_.end()) return PackageRegistrationError::DuplicateId;
		const ContentRegistrationError result = content_.registerContent(package.descriptor().content);
		if (result != ContentRegistrationError::None) return translate(result);
		packages_.emplace(id, &package);
		return PackageRegistrationError::None;
	}

	PackageActivationError activate(const std::string& id)
	{
		const auto found = packages_.find(id);
		if (found == packages_.end()) return PackageActivationError::NotFound;
		if (isActive(id)) return PackageActivationError::AlreadyActive;
		EnginePackage& package = *found->second;
		if (package.descriptor().kind == PackageKind::Campaign && !activeCampaign_.empty())
			return PackageActivationError::CampaignAlreadyActive;
		if (!package.activate()) return PackageActivationError::ActivationFailed;
		active_.push_back(id);
		if (package.descriptor().kind == PackageKind::Campaign) activeCampaign_ = id;
		return PackageActivationError::None;
	}

	bool deactivate(const std::string& id)
	{
		for (auto it = active_.begin(); it != active_.end(); ++it)
		{
			if (*it != id) continue;
			packages_.at(id)->deactivate();
			active_.erase(it);
			if (activeCampaign_ == id) activeCampaign_.clear();
			return true;
		}
		return false;
	}

	const EnginePackage* find(const std::string& id) const
	{
		const auto found = packages_.find(id);
		return found == packages_.end() ? nullptr : found->second;
	}

	bool isActive(const std::string& id) const
	{
		for (const std::string& activeId : active_)
			if (activeId == id) return true;
		return false;
	}

	const std::string& activeCampaign() const { return activeCampaign_; }
	const std::vector<std::string>& activationOrder() const { return active_; }

private:
	static PackageRegistrationError translate(ContentRegistrationError error)
	{
		switch (error)
		{
			case ContentRegistrationError::InvalidManifest: return PackageRegistrationError::InvalidManifest;
			case ContentRegistrationError::IncompatibleApi: return PackageRegistrationError::IncompatibleApi;
			case ContentRegistrationError::DuplicateId: return PackageRegistrationError::DuplicateId;
			case ContentRegistrationError::None: break;
		}
		return PackageRegistrationError::None;
	}

	ContentRegistry& content_;
	std::unordered_map<std::string, EnginePackage*> packages_;
	std::vector<std::string> active_;
	std::string activeCampaign_;
};

#endif
