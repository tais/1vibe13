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
	// Activation and teardown cross the package boundary and must not throw.
	// Fail activation through the return value so the registry remains able to
	// preserve a coherent active set.
	virtual bool activate() noexcept = 0;
	virtual void deactivate() noexcept = 0;
	virtual bool bootstrap(PackageBootstrapContext&, PackageBootstrapPhase) { return true; }
	virtual void shutdown(PackageBootstrapContext&, PackageBootstrapPhase) {}
	// Optional read-only package content. This virtual is appended to preserve
	// the positions of the original lifecycle hooks. Activation may construct
	// the source; once returned, its identity and lifetime must remain stable
	// until the registry unmounts it immediately before deactivate().
	virtual const AssetSource* assetSource() const noexcept { return nullptr; }
};

enum class PackageRegistrationError
{
	None,
	InvalidManifest,
	IncompatibleApi,
	DuplicateId,
	OperationInProgress
};

enum class PackageActivationError
{
	None,
	NotFound,
	AlreadyActive,
	CampaignAlreadyActive,
	BootstrapInProgress,
	ActivationFailed,
	AssetMountFailed,
	OperationInProgress
};

enum class PackageBootstrapError
{
	None,
	OutOfOrder,
	CallbackFailed,
	OperationInProgress
};

class PackageRegistry
{
public:
	explicit PackageRegistry(ContentRegistry& content, EngineServices services = EngineServices::defaults())
		: content_(content), assets_(services.assets), services_(withAssets(services, assets_)) {}

	// Registry entries and bootstrap state are tied to the referenced content
	// registry and application-owned package objects. Preserve that identity;
	// copying or moving would create a second registry with unsafe aliases.
	PackageRegistry(const PackageRegistry&) = delete;
	PackageRegistry& operator=(const PackageRegistry&) = delete;
	PackageRegistry(PackageRegistry&&) = delete;
	PackageRegistry& operator=(PackageRegistry&&) = delete;

	PackageRegistrationError registerPackage(EnginePackage& package)
	{
		if (operationInProgress_) return PackageRegistrationError::OperationInProgress;
		OperationGuard operation(operationInProgress_);
		const PackageDescriptor& descriptor = package.descriptor();
		const std::string& id = descriptor.content.id;
		if (packages_.find(id) != packages_.end()) return PackageRegistrationError::DuplicateId;
		const auto inserted =
			packages_.emplace(id, RegisteredPackage{&package, descriptor.kind, false});
		if (!inserted.second) return PackageRegistrationError::DuplicateId;
		ContentRegistrationError result = ContentRegistrationError::None;
		try
		{
			result = content_.registerContent(descriptor.content);
		}
		catch (...)
		{
			packages_.erase(inserted.first);
			throw;
		}
		if (result != ContentRegistrationError::None)
		{
			packages_.erase(inserted.first);
			return translate(result);
		}
		return PackageRegistrationError::None;
	}

	PackageActivationError activate(const std::string& id)
	{
		if (operationInProgress_) return PackageActivationError::OperationInProgress;
		OperationGuard operation(operationInProgress_);
		if (completedBootstrapPhases_ != 0) return PackageActivationError::BootstrapInProgress;
		const auto found = packages_.find(id);
		if (found == packages_.end()) return PackageActivationError::NotFound;
		if (isActive(id)) return PackageActivationError::AlreadyActive;
		const std::string packageId = found->first;
		RegisteredPackage& registered = found->second;
		EnginePackage& package = *registered.package;
		if (registered.kind == PackageKind::Campaign && !activeCampaign_.empty())
			return PackageActivationError::CampaignAlreadyActive;
		if (!package.activate()) return PackageActivationError::ActivationFailed;

		const AssetSource* packageAssets = nullptr;
		bool assetsMounted = false;
		try
		{
			packageAssets = package.assetSource();
			assetsMounted = packageAssets && assets_.mount(packageId, *packageAssets);
		}
		catch (...)
		{
			package.deactivate();
			throw;
		}
		if (packageAssets && !assetsMounted)
		{
			package.deactivate();
			logError("Asset mount failed: ", packageId);
			return PackageActivationError::AssetMountFailed;
		}
		try
		{
			active_.push_back(packageId);
			if (registered.kind == PackageKind::Campaign) activeCampaign_ = packageId;
		}
		catch (...)
		{
			if (!active_.empty() && active_.back() == packageId) active_.pop_back();
			if (registered.kind == PackageKind::Campaign) activeCampaign_.clear();
			if (assetsMounted) assets_.unmount(packageId);
			package.deactivate();
			throw;
		}
		registered.assetsMounted = assetsMounted;
		return PackageActivationError::None;
	}

	bool deactivate(const std::string& id)
	{
		if (operationInProgress_) return false;
		OperationGuard operation(operationInProgress_);
		if (completedBootstrapPhases_ != 0) return false;
		for (auto it = active_.begin(); it != active_.end(); ++it)
		{
			if (*it != id) continue;
			RegisteredPackage& registered = packages_.at(*it);
			if (registered.assetsMounted)
			{
				// Remove the non-owning source before the callback is allowed to
				// release its storage.
				if (!assets_.unmount(*it))
				{
					logError("Active asset mount was missing: ", *it);
					return false;
				}
				registered.assetsMounted = false;
			}
			const bool wasCampaign = activeCampaign_ == *it;
			registered.package->deactivate();
			active_.erase(it);
			if (wasCampaign) activeCampaign_.clear();
			return true;
		}
		return false;
	}

	PackageBootstrapError bootstrap(PackageBootstrapPhase phase)
	{
		if (operationInProgress_) return PackageBootstrapError::OperationInProgress;
		OperationGuard operation(operationInProgress_);
		const std::size_t phaseIndex = static_cast<std::size_t>(phase);
		if (phaseIndex >= bootstrapPhaseCount_ || phaseIndex != completedBootstrapPhases_)
			return PackageBootstrapError::OutOfOrder;

		PackageBootstrapContext context{content_, services_};
		for (std::size_t index = 0; index < active_.size(); ++index)
		{
			bool succeeded = false;
			bool threw = false;
			try
			{
				succeeded = packages_.at(active_[index]).package->bootstrap(context, phase);
			}
			catch (...)
			{
				threw = true;
			}
			if (succeeded) continue;
			logError(threw ? "Bootstrap callback threw: " : "Bootstrap callback failed: ",
				active_[index]);
			// The failing callback may have acquired part of its phase resources,
			// so include it in the reverse rollback contract.
			for (std::size_t rollback = index + 1; rollback > 0; --rollback)
			{
				try
				{
					packages_.at(active_[rollback - 1]).package->shutdown(context, phase);
				}
				catch (...)
				{
					logError("Bootstrap rollback threw: ", active_[rollback - 1]);
				}
			}
			return PackageBootstrapError::CallbackFailed;
		}
		++completedBootstrapPhases_;
		return PackageBootstrapError::None;
	}

	void shutdownBootstrap()
	{
		if (operationInProgress_) return;
		OperationGuard operation(operationInProgress_);
		PackageBootstrapContext context{content_, services_};
		while (completedBootstrapPhases_ > 0)
		{
			const PackageBootstrapPhase phase =
				static_cast<PackageBootstrapPhase>(completedBootstrapPhases_ - 1);
			for (auto package = active_.rbegin(); package != active_.rend(); ++package)
			{
				try
				{
					packages_.at(*package).package->shutdown(context, phase);
				}
				catch (...)
				{
					logError("Package shutdown threw: ", *package);
				}
			}
			--completedBootstrapPhases_;
		}
	}

	const EnginePackage* find(const std::string& id) const
	{
		const auto found = packages_.find(id);
		return found == packages_.end() ? nullptr : found->second.package;
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
	EngineServices& services() { return services_; }
	const EngineServices& services() const { return services_; }
	const AssetSource& assets() const { return assets_; }

private:
	class OperationGuard
	{
	public:
		explicit OperationGuard(bool& active) : active_(active) { active_ = true; }
		~OperationGuard() { active_ = false; }
		OperationGuard(const OperationGuard&) = delete;
		OperationGuard& operator=(const OperationGuard&) = delete;
	private:
		bool& active_;
	};

	struct RegisteredPackage
	{
		EnginePackage* package;
		PackageKind kind;
		bool assetsMounted;
	};

	static EngineServices withAssets(EngineServices services, const AssetSource& assets)
	{
		return EngineServices{services.time, services.random, services.storage, services.log,
			services.input, services.audio, services.frames, assets};
	}

	void logError(const char* prefix, const std::string& packageId) noexcept
	{
		try
		{
			services_.log.write(LogRecord{LogSeverity::Error, "packages",
				std::string(prefix) + packageId});
		}
		catch (...)
		{
			// Diagnostics must never corrupt package lifecycle state.
		}
	}

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
	CompositeAssetSource assets_;
	EngineServices services_;
	std::unordered_map<std::string, RegisteredPackage> packages_;
	std::vector<std::string> active_;
	std::string activeCampaign_;
	std::size_t completedBootstrapPhases_ = 0;
	bool operationInProgress_ = false;
	static constexpr std::size_t bootstrapPhaseCount_ = 3;
};

#endif
