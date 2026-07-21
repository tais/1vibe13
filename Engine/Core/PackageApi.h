#ifndef ENGINE_CORE_PACKAGE_API_H
#define ENGINE_CORE_PACKAGE_API_H

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Engine/Core/PackageContract.h>
#include <Engine/Core/PackageCatalog.h>
#include <Engine/Core/PackageEventSink.h>
#include <Engine/Core/PackageResults.h>
#include <Engine/Core/InputDispatcher.h>
#include <Engine/Core/RuntimeUpdate.h>
#include <Engine/Core/RuntimeCapabilities.h>

class PackageRegistry : public InputEventSink, public RuntimeUpdateSink,
	public RuntimeMessageSink
{
public:
	explicit PackageRegistry(ContentRegistry& content,
		EngineServices services = EngineServices::defaults(),
		PackageEventSink& events = NullPackageEventSink::instance(),
		RuntimeMessageBus& messages = RuntimeMessageBus::disabled(),
		ServiceCatalog& extensionServices = ServiceCatalog::disabled())
		: content_(content), assets_(services.assets), services_(withAssets(services, assets_)),
		  events_(events), messages_(messages), extensionServices_(extensionServices) {}

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
		if (!RuntimeCapabilities::isValidList(descriptor.capabilities) ||
			!RuntimeCapabilities::isValidList(descriptor.messageTopics))
			return PackageRegistrationError::InvalidManifest;
		if (packages_.find(id) != packages_.end()) return PackageRegistrationError::DuplicateId;
		const auto inserted = packages_.emplace(id, RegisteredPackage{&package, descriptor.kind,
			descriptor.content.version, descriptor.content.requirements,
			descriptor.content.optionalRequirements, descriptor.content.conflicts,
			descriptor.content.loadAfter, descriptor.capabilities,
			descriptor.messageTopics, false, false});
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
		emit(PackageEventKind::Registered, id);
		return PackageRegistrationError::None;
	}

	PackageUnregistrationResult unregisterPackage(const std::string& id)
	{
		const PackageUnregistrationBatchResult batch =
			unregisterPackages(std::vector<std::string>{id});
		return PackageUnregistrationResult{batch.error, batch.packageId, batch.dependentId};
	}

	PackageUnregistrationBatchResult unregisterPackages(
		const std::vector<std::string>& requested)
	{
		if (operationInProgress_)
			return PackageUnregistrationBatchResult{
				PackageUnregistrationError::OperationInProgress, {}, {}, {}};
		OperationGuard operation(operationInProgress_);
		if (completedBootstrapPhases_ != 0)
			return PackageUnregistrationBatchResult{
				PackageUnregistrationError::BootstrapInProgress, {}, {}, {}};
		if (requested.empty()) return PackageUnregistrationBatchResult{};

		std::unordered_set<std::string> removalSet;
		removalSet.reserve(requested.size());
		for (const std::string& id : requested)
		{
			if (!removalSet.insert(id).second)
				return PackageUnregistrationBatchResult{
					PackageUnregistrationError::InvalidRequest, id, {}, {}};
			const auto found = packages_.find(id);
			if (found == packages_.end())
				return PackageUnregistrationBatchResult{
					PackageUnregistrationError::NotFound, id, {}, {}};
			if (found->second.active)
				return PackageUnregistrationBatchResult{
					PackageUnregistrationError::Active, found->first, {}, {}};
			if (!content_.find(found->first))
				return PackageUnregistrationBatchResult{
					PackageUnregistrationError::ContentMissing, found->first, {}, {}};
		}

		// Internal edges are removed as one transaction, including cycles. Only
		// dependents outside the requested set are blockers.
		for (const auto& registered : packages_)
		{
			if (removalSet.find(registered.first) != removalSet.end()) continue;
			for (const ContentRequirement& requirement : registered.second.requirements)
			{
				if (removalSet.find(requirement.id) == removalSet.end()) continue;
				return PackageUnregistrationBatchResult{
					PackageUnregistrationError::RequiredByRegisteredPackage,
					requirement.id, registered.first, {}};
			}
		}

		// Copy every diagnostic string before changing either registry. The
		// mutation path then performs no allocations and cannot expose a package
		// whose content entry was only partly removed.
		PackageUnregistrationBatchResult result;
		result.unregistered = requested;
		for (std::size_t index = 0; index < result.unregistered.size(); ++index)
		{
			const std::string& id = result.unregistered[index];
			if (content_.unregisterContent(id) != ContentUnregistrationError::None)
			{
				result.error = PackageUnregistrationError::ContentMissing;
				result.packageId = id;
				result.unregistered.resize(index);
				return result;
			}
			packages_.erase(id);
			emit(PackageEventKind::Unregistered, id);
		}
		return result;
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
			for (const ContentRequirement& requirement : activePackage.optionalRequirements)
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

	PackageDeactivationBatchResult deactivateAll()
	{
		if (operationInProgress_)
			return PackageDeactivationBatchResult{
				PackageDeactivationError::OperationInProgress, {}, {}};
		OperationGuard operation(operationInProgress_);
		if (completedBootstrapPhases_ != 0)
			return PackageDeactivationBatchResult{
				PackageDeactivationError::BootstrapInProgress, {}, {}};
		PackageDeactivationBatchResult result;
		result.deactivated.reserve(active_.size());
		while (!active_.empty())
		{
			// Copy before mutation: deactivateOne removes the corresponding
			// activation-order element on success.
			std::string packageId = active_.back();
			const PackageDeactivationError error = deactivateOne(packageId);
			if (error != PackageDeactivationError::None)
			{
				result.error = error;
				result.packageId = std::move(packageId);
				return result;
			}
			result.deactivated.push_back(std::move(packageId));
		}
		return result;
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
			std::size_t nextDependency;
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
				const std::size_t mandatoryCount = frame.package->requirements.size();
				const std::size_t dependencyCount = mandatoryCount +
					frame.package->optionalRequirements.size();
				if (frame.nextDependency >= dependencyCount)
				{
					states[*frame.id] = VisitState::Visited;
					plan.order.push_back(*frame.id);
					stack.pop_back();
					continue;
				}

				const bool optional = frame.nextDependency >= mandatoryCount;
				const ContentRequirement& requirement = optional
					? frame.package->optionalRequirements[frame.nextDependency++ - mandatoryCount]
					: frame.package->requirements[frame.nextDependency++];
				const auto dependency = packages_.find(requirement.id);
				if (dependency == packages_.end())
				{
					if (optional) continue;
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

		std::unordered_set<std::string> plannedIds(plan.order.begin(), plan.order.end());
		for (const std::string& packageId : plan.order)
		{
			const RegisteredPackage& package = packages_.at(packageId);
			for (const std::string& conflictId : package.conflicts)
			{
				const auto conflict = packages_.find(conflictId);
				if (conflict == packages_.end() ||
					(!conflict->second.active && plannedIds.find(conflictId) == plannedIds.end()))
					continue;
				plan.error = PackageResolutionError::PackageConflict;
				plan.packageId = packageId;
				plan.diagnosticPath = {packageId, conflictId};
				plan.order.clear();
				return plan;
			}
		}
		// Conflicts are symmetric at resolution time: an already-active package
		// can reject a newly planned package without requiring the latter to
		// repeat the declaration.
		for (const std::string& activeId : active_)
		{
			for (const std::string& conflictId : packages_.at(activeId).conflicts)
			{
				if (plannedIds.find(conflictId) == plannedIds.end()) continue;
				plan.error = PackageResolutionError::PackageConflict;
				plan.packageId = conflictId;
				plan.diagnosticPath = {activeId, conflictId};
				plan.order.clear();
				return plan;
			}
		}

		// The dependency DFS above discovers a stable closure and reports strong
		// dependency cycles with their exact path. A second stable topological
		// pass applies weak LOAD_AFTER edges only among members of that closure;
		// absent or unselected predecessors never cause implicit activation.
		if (plan.order.size() > 1)
		{
			const std::vector<std::string> discovered = plan.order;
			std::unordered_map<std::string, std::size_t> indices;
			indices.reserve(discovered.size());
			for (std::size_t index = 0; index < discovered.size(); ++index)
				indices.emplace(discovered[index], index);
			std::vector<std::vector<std::size_t>> consumers(discovered.size());
			std::vector<std::size_t> incoming(discovered.size(), 0);
			auto addEdge = [&indices, &consumers, &incoming](
				const std::string& predecessor, std::size_t consumer)
			{
				const auto found = indices.find(predecessor);
				if (found == indices.end()) return;
				std::vector<std::size_t>& edges = consumers[found->second];
				if (std::find(edges.begin(), edges.end(), consumer) != edges.end()) return;
				edges.push_back(consumer);
				++incoming[consumer];
			};
			for (std::size_t consumer = 0; consumer < discovered.size(); ++consumer)
			{
				const RegisteredPackage& package = packages_.at(discovered[consumer]);
				for (const ContentRequirement& dependency : package.requirements)
					addEdge(dependency.id, consumer);
				for (const ContentRequirement& dependency : package.optionalRequirements)
					addEdge(dependency.id, consumer);
				for (const std::string& predecessor : package.loadAfter)
					addEdge(predecessor, consumer);
			}

			std::vector<bool> emitted(discovered.size(), false);
			std::vector<std::string> ordered;
			ordered.reserve(discovered.size());
			while (ordered.size() < discovered.size())
			{
				std::size_t next = discovered.size();
				for (std::size_t candidate = 0; candidate < discovered.size(); ++candidate)
				{
					if (!emitted[candidate] && incoming[candidate] == 0)
					{
						next = candidate;
						break;
					}
				}
				if (next == discovered.size())
				{
					plan.error = PackageResolutionError::OrderingCycle;
					for (std::size_t index = 0; index < discovered.size(); ++index)
					{
						if (emitted[index]) continue;
						if (plan.packageId.empty()) plan.packageId = discovered[index];
						plan.diagnosticPath.push_back(discovered[index]);
					}
					plan.order.clear();
					return plan;
				}
				emitted[next] = true;
				ordered.push_back(discovered[next]);
				for (const std::size_t consumer : consumers[next]) --incoming[consumer];
			}
			plan.order = std::move(ordered);
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

		PackageBootstrapContext context{content_, services_, messages_, extensionServices_};
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
			if (succeeded)
			{
				emit(PackageEventKind::BootstrapCompleted, active_[index], phaseIndex);
				continue;
			}
			emit(PackageEventKind::BootstrapFailed, active_[index], phaseIndex);
			logError(threw ? "Bootstrap callback threw: " : "Bootstrap callback failed: ",
				active_[index]);
			// The failing callback may have acquired part of its phase resources,
			// so include it in the reverse rollback contract.
			for (std::size_t rollback = index + 1; rollback > 0; --rollback)
			{
				bool rolledBack = false;
				try
				{
					packages_.at(active_[rollback - 1]).package->shutdown(context, phase);
					rolledBack = true;
				}
				catch (...)
				{
					logError("Bootstrap rollback threw: ", active_[rollback - 1]);
				}
				emit(rolledBack ? PackageEventKind::BootstrapRollbackCompleted
				                : PackageEventKind::BootstrapRollbackFailed,
					active_[rollback - 1], phaseIndex);
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
		PackageBootstrapContext context{content_, services_, messages_, extensionServices_};
		while (completedBootstrapPhases_ > 0)
		{
			const PackageBootstrapPhase phase =
				static_cast<PackageBootstrapPhase>(completedBootstrapPhases_ - 1);
			for (auto package = active_.rbegin(); package != active_.rend(); ++package)
			{
				bool shutDown = false;
				try
				{
					packages_.at(*package).package->shutdown(context, phase);
					shutDown = true;
				}
				catch (...)
				{
					logError("Package shutdown threw: ", *package);
				}
				emit(shutDown ? PackageEventKind::ShutdownCompleted
				              : PackageEventKind::ShutdownFailed,
					*package, static_cast<std::size_t>(phase));
			}
			--completedBootstrapPhases_;
		}
	}

	void receiveInput(const EngineInputEvent& event) override
	{
		if (operationInProgress_ || completedBootstrapPhases_ != bootstrapPhaseCount_)
			return;
		OperationGuard operation(operationInProgress_);
		PackageBootstrapContext context{content_, services_, messages_, extensionServices_};
		for (const std::string& packageId : active_)
		{
			RegisteredPackage& registered = packages_.at(packageId);
			++registered.runtimeHealth.inputCallbacks;
			try
			{
				registered.package->receiveInput(context, event);
			}
			catch (...)
			{
				const std::uint64_t failure = ++registered.runtimeHealth.inputFailures;
				logRuntimeFailure("input", packageId, failure,
					registered.runtimeHealth.suppressedFailureLogs);
			}
		}
	}

	void updateRuntime(const RuntimeUpdateContext& update) override
	{
		if (operationInProgress_ || completedBootstrapPhases_ != bootstrapPhaseCount_)
			return;
		OperationGuard operation(operationInProgress_);
		PackageBootstrapContext context{content_, services_, messages_, extensionServices_};
		for (const std::string& packageId : active_)
		{
			RegisteredPackage& registered = packages_.at(packageId);
			++registered.runtimeHealth.runtimeUpdateCallbacks;
			try
			{
				registered.package->updateRuntime(context, update);
			}
			catch (...)
			{
				const std::uint64_t failure =
					++registered.runtimeHealth.runtimeUpdateFailures;
				logRuntimeFailure("runtime update", packageId, failure,
					registered.runtimeHealth.suppressedFailureLogs);
			}
		}
	}

	void receiveMessage(const RuntimeMessage& message) override
	{
		if (operationInProgress_ || completedBootstrapPhases_ != bootstrapPhaseCount_)
			return;
		OperationGuard operation(operationInProgress_);
		PackageBootstrapContext context{content_, services_, messages_, extensionServices_};
		for (const std::string& packageId : active_)
		{
			RegisteredPackage& registered = packages_.at(packageId);
			if (!registered.messageTopics.empty() &&
				std::find(registered.messageTopics.begin(), registered.messageTopics.end(),
					message.topic) == registered.messageTopics.end())
			{
				++registered.runtimeHealth.filteredMessages;
				continue;
			}
			++registered.runtimeHealth.messageCallbacks;
			try
			{
				registered.package->receiveMessage(context, message);
			}
			catch (...)
			{
				const std::uint64_t failure = ++registered.runtimeHealth.messageFailures;
				logRuntimeFailure("message", packageId, failure,
					registered.runtimeHealth.suppressedFailureLogs);
			}
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

	bool hasCapability(const std::string& capability) const
	{
		for (const std::string& packageId : active_)
		{
			const std::vector<std::string>& capabilities =
				packages_.at(packageId).capabilities;
			if (std::find(capabilities.begin(), capabilities.end(), capability) !=
				capabilities.end()) return true;
		}
		return false;
	}

	RuntimeCapabilities activeCapabilities() const
	{
		RuntimeCapabilities capabilities;
		for (const std::string& packageId : active_)
			capabilities.addAll(packages_.at(packageId).capabilities);
		return capabilities;
	}

	const std::string& activeCampaign() const { return activeCampaign_; }
	const std::vector<std::string>& activationOrder() const { return active_; }
	std::size_t completedBootstrapPhases() const { return completedBootstrapPhases_; }
	PackageCatalogSnapshot catalog() const
	{
		PackageCatalogSnapshot snapshot;
		snapshot.supportedApi = content_.supportedApi();
		snapshot.activationOrder = active_;
		snapshot.activeCampaign = activeCampaign_;
		snapshot.completedBootstrapPhases = completedBootstrapPhases_;
		snapshot.activeCapabilities = activeCapabilities();
		snapshot.packages.reserve(content_.manifests().size());

		// ContentRegistry preserves host discovery order. Building the catalog
		// from it prevents unordered registry storage from leaking nondeterminism
		// into launcher, editor, replay, or diagnostic output.
		for (const ContentManifest& manifest : content_.manifests())
		{
			const auto registered = packages_.find(manifest.id);
			if (registered == packages_.end()) continue;
			const auto active = std::find(active_.begin(), active_.end(), manifest.id);
			PackageCatalogEntry entry{
				PackageDescriptor{manifest, registered->second.kind,
					registered->second.capabilities, registered->second.messageTopics},
				registered->second.active ? PackageLifecycleState::Active
				                          : PackageLifecycleState::Registered,
				registered->second.assetsMounted,
				active == active_.end()
					? PackageCatalogEntry::NotActive
					: static_cast<std::size_t>(active - active_.begin()),
				{}, registered->second.runtimeHealth};
			for (const ContentManifest& consumer : content_.manifests())
			{
				for (const ContentRequirement& requirement : consumer.requirements)
				{
					if (requirement.id != manifest.id) continue;
					entry.dependents.push_back(consumer.id);
					break;
				}
				for (const ContentRequirement& requirement : consumer.optionalRequirements)
				{
					if (requirement.id != manifest.id) continue;
					entry.dependents.push_back(consumer.id);
					break;
				}
			}
			snapshot.packages.push_back(std::move(entry));
		}
		return snapshot;
	}
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
		std::vector<ContentRequirement> optionalRequirements;
		std::vector<std::string> conflicts;
		std::vector<std::string> loadAfter;
		std::vector<std::string> capabilities;
		std::vector<std::string> messageTopics;
		bool assetsMounted;
		bool active;
		PackageRuntimeHealth runtimeHealth;
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
		registered.runtimeHealth = PackageRuntimeHealth{};
		emit(PackageEventKind::Activated, packageId);
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
		emit(PackageEventKind::Deactivated, packageId);
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
			case PackageResolutionError::PackageConflict: return PackageActivationError::PackageConflict;
			case PackageResolutionError::OrderingCycle: return PackageActivationError::OrderingCycle;
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

	void logRuntimeFailure(const char* callback, const std::string& packageId,
		std::uint64_t failure, std::uint64_t& suppressed) noexcept
	{
		// A broken per-frame callback must not turn into an unbounded logging
		// workload. Preserve the first four diagnostics, then only powers of two.
		const bool shouldLog = failure <= 4 || (failure & (failure - 1)) == 0;
		if (!shouldLog)
		{
			++suppressed;
			return;
		}
		try
		{
			services_.log.write(LogRecord{
				LogSeverity::Error, "packages",
				"Package " + std::string(callback) + " callback threw: " + packageId +
					" (failure " + std::to_string(failure) +
					", suppressed " + std::to_string(suppressed) + ")"});
		}
		catch (...)
		{
			// Diagnostics must never alter the runtime callback path.
		}
	}

	void emit(PackageEventKind kind, const std::string& packageId,
		std::size_t phase = PackageEvent::NoBootstrapPhase) noexcept
	{
		try
		{
			events_.publish(PackageEvent{kind, packageId, phase});
		}
		catch (...)
		{
			logError("Package event sink threw: ", packageId);
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
			case ContentRegistrationError::InvalidRelationship: return PackageRegistrationError::InvalidRelationship;
			case ContentRegistrationError::None: break;
		}
		return PackageRegistrationError::None;
	}

	ContentRegistry& content_;
	CompositeAssetSource assets_;
	EngineServices services_;
	PackageEventSink& events_;
	RuntimeMessageBus& messages_;
	ServiceCatalog& extensionServices_;
	std::unordered_map<std::string, RegisteredPackage> packages_;
	std::vector<std::string> active_;
	std::string activeCampaign_;
	std::size_t completedBootstrapPhases_ = 0;
	bool operationInProgress_ = false;
	static constexpr std::size_t bootstrapPhaseCount_ = 3;
};

#endif
