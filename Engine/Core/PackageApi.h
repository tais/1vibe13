#ifndef ENGINE_CORE_PACKAGE_API_H
#define ENGINE_CORE_PACKAGE_API_H

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Engine/Core/PackageContract.h>
#include <Engine/Core/PackageResults.h>

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
		const auto inserted = packages_.emplace(id, RegisteredPackage{&package, descriptor.kind,
			descriptor.content.version, descriptor.content.requirements, false, false});
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

	PackageActivationPlan resolveActivation(const std::string& id) const
	{
		return resolveActivation(std::vector<std::string>{id});
	}

	PackageActivationPlan resolveActivation(const std::vector<std::string>& requested) const
	{
		if (operationInProgress_)
		{
			PackageActivationPlan plan;
			plan.error = PackageResolutionError::OperationInProgress;
			return plan;
		}
		return resolveActivationUnchecked(requested);
	}

	PackageActivationPlan resolveActivation(
		std::initializer_list<std::string> requested) const
	{
		return resolveActivation(std::vector<std::string>(requested));
	}

	PackageActivationResult activateAll(const std::vector<std::string>& requested)
	{
		if (operationInProgress_)
			return PackageActivationResult{PackageActivationError::OperationInProgress, {}, {}, {}};
		OperationGuard operation(operationInProgress_);
		if (completedBootstrapPhases_ != 0)
			return PackageActivationResult{PackageActivationError::BootstrapInProgress, {}, {}, {}};

		PackageActivationPlan plan = resolveActivationUnchecked(requested);
		if (!plan)
			return PackageActivationResult{
				translateResolution(plan.error), plan.packageId, std::move(plan.diagnosticPath), {}};

		std::size_t activatedCount = 0;
		try
		{
			for (; activatedCount < plan.order.size(); ++activatedCount)
			{
				const PackageActivationError error = activateOne(plan.order[activatedCount]);
				if (error == PackageActivationError::None) continue;
				const std::string failedPackage = plan.order[activatedCount];
				while (activatedCount > 0)
					deactivateOne(plan.order[--activatedCount]);
				return PackageActivationResult{error, failedPackage, {}, {}};
			}
		}
		catch (...)
		{
			while (activatedCount > 0) deactivateOne(plan.order[--activatedCount]);
			throw;
		}
		return PackageActivationResult{
			PackageActivationError::None, {}, {}, std::move(plan.order)};
	}

	PackageActivationError activate(const std::string& id)
	{
		if (operationInProgress_) return PackageActivationError::OperationInProgress;
		if (completedBootstrapPhases_ != 0) return PackageActivationError::BootstrapInProgress;
		const auto found = packages_.find(id);
		if (found == packages_.end()) return PackageActivationError::NotFound;
		if (found->second.active) return PackageActivationError::AlreadyActive;
		return activateAll(std::vector<std::string>{found->first}).error;
	}

	PackageDeactivationResult deactivateDetailed(const std::string& id)
	{
		if (operationInProgress_)
			return PackageDeactivationResult{PackageDeactivationError::OperationInProgress, {}, {}};
		OperationGuard operation(operationInProgress_);
		if (completedBootstrapPhases_ != 0)
			return PackageDeactivationResult{PackageDeactivationError::BootstrapInProgress, id, {}};
		const auto found = packages_.find(id);
		if (found == packages_.end())
			return PackageDeactivationResult{PackageDeactivationError::NotFound, id, {}};
		if (!found->second.active)
			return PackageDeactivationResult{PackageDeactivationError::NotActive, found->first, {}};

		for (const std::string& activeId : active_)
		{
			const RegisteredPackage& activePackage = packages_.at(activeId);
			for (const ContentRequirement& requirement : activePackage.requirements)
			{
				if (requirement.id != found->first) continue;
				return PackageDeactivationResult{
					PackageDeactivationError::RequiredByActivePackage, found->first, activeId};
			}
		}

		// Allocate diagnostics before mutating lifecycle state so an allocation
		// failure cannot obscure a successful deactivation.
		PackageDeactivationResult result{PackageDeactivationError::None, found->first, {}};
		result.error = deactivateOne(found->first);
		return result;
	}

	bool deactivate(const std::string& id)
	{
		if (operationInProgress_ || completedBootstrapPhases_ != 0) return false;
		return static_cast<bool>(deactivateDetailed(id));
	}

private:
	PackageActivationPlan resolveActivationUnchecked(
		const std::vector<std::string>& requested) const
	{
		PackageActivationPlan plan;
		if (requested.empty())
		{
			plan.error = PackageResolutionError::EmptyRequest;
			return plan;
		}

		enum class VisitState { Visiting, Visited };
		struct Frame
		{
			const std::string* id;
			const RegisteredPackage* package;
			std::size_t nextRequirement;
		};
		std::unordered_map<std::string, VisitState> states;
		std::vector<Frame> stack;
		states.reserve(packages_.size());
		stack.reserve(packages_.size());
		plan.order.reserve(packages_.size());

		auto setPath = [&plan, &stack](const std::string& finalId, std::size_t first)
		{
			plan.diagnosticPath.clear();
			plan.diagnosticPath.reserve(stack.size() - first + 1);
			for (std::size_t index = first; index < stack.size(); ++index)
				plan.diagnosticPath.push_back(*stack[index].id);
			plan.diagnosticPath.push_back(finalId);
		};

		for (const std::string& requestedId : requested)
		{
			const auto root = packages_.find(requestedId);
			if (root == packages_.end())
			{
				plan.error = PackageResolutionError::NotFound;
				plan.packageId = requestedId;
				plan.diagnosticPath = {requestedId};
				plan.order.clear();
				return plan;
			}
			if (root->second.active) continue;
			const auto rootState = states.find(root->first);
			if (rootState != states.end() && rootState->second == VisitState::Visited) continue;

			states.emplace(root->first, VisitState::Visiting);
			stack.push_back(Frame{&root->first, &root->second, 0});
			while (!stack.empty())
			{
				Frame& frame = stack.back();
				if (frame.nextRequirement >= frame.package->requirements.size())
				{
					states[*frame.id] = VisitState::Visited;
					plan.order.push_back(*frame.id);
					stack.pop_back();
					continue;
				}

				const ContentRequirement& requirement =
					frame.package->requirements[frame.nextRequirement++];
				const auto dependency = packages_.find(requirement.id);
				if (dependency == packages_.end())
				{
					plan.error = PackageResolutionError::MissingRequirement;
					plan.packageId = requirement.id;
					setPath(requirement.id, 0);
					plan.order.clear();
					return plan;
				}
				if (!requirement.exactVersion.empty() &&
					dependency->second.version != requirement.exactVersion)
				{
					plan.error = PackageResolutionError::VersionMismatch;
					plan.packageId = requirement.id;
					setPath(requirement.id, 0);
					plan.order.clear();
					return plan;
				}
				if (dependency->second.active) continue;

				const auto dependencyState = states.find(dependency->first);
				if (dependencyState != states.end())
				{
					if (dependencyState->second == VisitState::Visiting)
					{
						std::size_t cycleStart = 0;
						while (cycleStart < stack.size() &&
							*stack[cycleStart].id != dependency->first) ++cycleStart;
						plan.error = PackageResolutionError::DependencyCycle;
						plan.packageId = dependency->first;
						setPath(dependency->first, cycleStart);
						plan.order.clear();
						return plan;
					}
					continue;
				}

				states.emplace(dependency->first, VisitState::Visiting);
				stack.push_back(Frame{&dependency->first, &dependency->second, 0});
			}
		}

		std::string plannedCampaign = activeCampaign_;
		for (const std::string& packageId : plan.order)
		{
			if (packages_.at(packageId).kind != PackageKind::Campaign) continue;
			if (!plannedCampaign.empty() && plannedCampaign != packageId)
			{
				plan.error = PackageResolutionError::CampaignConflict;
				plan.packageId = packageId;
				plan.diagnosticPath = {plannedCampaign, packageId};
				plan.order.clear();
				return plan;
			}
			plannedCampaign = packageId;
		}
		return plan;
	}

public:
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
		const auto found = packages_.find(id);
		return found != packages_.end() && found->second.active;
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
		std::string version;
		std::vector<ContentRequirement> requirements;
		bool assetsMounted;
		bool active;
	};

	PackageActivationError activateOne(const std::string& id)
	{
		const auto found = packages_.find(id);
		if (found == packages_.end()) return PackageActivationError::NotFound;
		RegisteredPackage& registered = found->second;
		if (registered.active) return PackageActivationError::AlreadyActive;
		const std::string& packageId = found->first;
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
		registered.active = true;
		return PackageActivationError::None;
	}

	PackageDeactivationError deactivateOne(const std::string& id)
	{
		const auto found = packages_.find(id);
		if (found == packages_.end()) return PackageDeactivationError::NotFound;
		RegisteredPackage& registered = found->second;
		if (!registered.active) return PackageDeactivationError::NotActive;
		const std::string& packageId = found->first;
		const auto activePosition = std::find(active_.begin(), active_.end(), packageId);
		if (activePosition == active_.end()) return PackageDeactivationError::NotActive;
		if (registered.assetsMounted)
		{
			// Remove the non-owning source before the callback is allowed to
			// release its storage.
			if (!assets_.unmount(packageId))
			{
				logError("Active asset mount was missing: ", packageId);
				return PackageDeactivationError::AssetUnmountFailed;
			}
			registered.assetsMounted = false;
		}
		const bool wasCampaign = activeCampaign_ == packageId;
		registered.package->deactivate();
		active_.erase(activePosition);
		registered.active = false;
		if (wasCampaign) activeCampaign_.clear();
		return PackageDeactivationError::None;
	}

	static PackageActivationError translateResolution(PackageResolutionError error)
	{
		switch (error)
		{
			case PackageResolutionError::OperationInProgress: return PackageActivationError::OperationInProgress;
			case PackageResolutionError::EmptyRequest: return PackageActivationError::InvalidRequest;
			case PackageResolutionError::NotFound: return PackageActivationError::NotFound;
			case PackageResolutionError::MissingRequirement: return PackageActivationError::MissingRequirement;
			case PackageResolutionError::VersionMismatch: return PackageActivationError::RequirementVersionMismatch;
			case PackageResolutionError::DependencyCycle: return PackageActivationError::DependencyCycle;
			case PackageResolutionError::CampaignConflict: return PackageActivationError::CampaignAlreadyActive;
			case PackageResolutionError::None: break;
		}
		return PackageActivationError::None;
	}

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
			case ContentRegistrationError::InvalidRequirement: return PackageRegistrationError::InvalidRequirement;
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
