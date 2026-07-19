#ifndef ENGINE_CORE_PACKAGE_API_H
#define ENGINE_CORE_PACKAGE_API_H

#include <string>
#include <unordered_map>
#include <vector>

#include <Engine/Core/ContentApi.h>
#include <Engine/Core/EngineServices.h>

enum class PackageKind
{
	Campaign,
	Rules,
	Extension,
	Tool
};

enum class PackageBootstrapPhase
{
	Configure,
	LoadContent,
	StartRuntime
};

// Engine-owned surface passed to package hooks. New services are added here
// as interfaces, never as SDL/SGP or campaign types, so packages do not need
// legacy globals to participate in bootstrap.
struct PackageBootstrapContext
{
	ContentRegistry& content;
	EngineServices& services;
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
	virtual bool bootstrap(PackageBootstrapContext&, PackageBootstrapPhase) { return true; }
	virtual void shutdown(PackageBootstrapContext&, PackageBootstrapPhase) {}
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
	BootstrapInProgress,
	ActivationFailed
};

enum class PackageBootstrapError
{
	None,
	OutOfOrder,
	CallbackFailed
};

class PackageRegistry
{
public:
	explicit PackageRegistry(ContentRegistry& content, EngineServices services = EngineServices::defaults())
		: content_(content), services_(services) {}

	// Registry entries and bootstrap state are tied to the referenced content
	// registry and application-owned package objects. Preserve that identity;
	// copying or moving would create a second registry with unsafe aliases.
	PackageRegistry(const PackageRegistry&) = delete;
	PackageRegistry& operator=(const PackageRegistry&) = delete;
	PackageRegistry(PackageRegistry&&) = delete;
	PackageRegistry& operator=(PackageRegistry&&) = delete;

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
		if (completedBootstrapPhases_ != 0) return PackageActivationError::BootstrapInProgress;
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
		if (completedBootstrapPhases_ != 0) return false;
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

	PackageBootstrapError bootstrap(PackageBootstrapPhase phase)
	{
		const std::size_t phaseIndex = static_cast<std::size_t>(phase);
		if (phaseIndex >= bootstrapPhaseCount_ || phaseIndex != completedBootstrapPhases_)
			return PackageBootstrapError::OutOfOrder;

		PackageBootstrapContext context{content_, services_};
		for (std::size_t index = 0; index < active_.size(); ++index)
		{
			if (packages_.at(active_[index])->bootstrap(context, phase)) continue;
			services_.log.write(LogRecord{LogSeverity::Error, "packages",
				"Bootstrap callback failed: " + active_[index]});
			// The failing callback may have acquired part of its phase resources,
			// so include it in the reverse rollback contract.
			for (std::size_t rollback = index + 1; rollback > 0; --rollback)
				packages_.at(active_[rollback - 1])->shutdown(context, phase);
			return PackageBootstrapError::CallbackFailed;
		}
		++completedBootstrapPhases_;
		return PackageBootstrapError::None;
	}

	void shutdownBootstrap()
	{
		PackageBootstrapContext context{content_, services_};
		while (completedBootstrapPhases_ > 0)
		{
			const PackageBootstrapPhase phase =
				static_cast<PackageBootstrapPhase>(completedBootstrapPhases_ - 1);
			for (auto package = active_.rbegin(); package != active_.rend(); ++package)
				packages_.at(*package)->shutdown(context, phase);
			--completedBootstrapPhases_;
		}
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
	std::size_t completedBootstrapPhases() const { return completedBootstrapPhases_; }

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
	EngineServices services_;
	std::unordered_map<std::string, EnginePackage*> packages_;
	std::vector<std::string> active_;
	std::string activeCampaign_;
	std::size_t completedBootstrapPhases_ = 0;
	static constexpr std::size_t bootstrapPhaseCount_ = 3;
};

#endif
