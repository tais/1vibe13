#ifndef ENGINE_CORE_PACKAGE_LIFECYCLE_H
#define ENGINE_CORE_PACKAGE_LIFECYCLE_H

#include <cstddef>
#include <exception>
#include <limits>

#include <Engine/Core/PackageApi.h>

struct PackageLifecycleRollbackResult
{
	PackageBootstrapShutdownResult packages;

	explicit operator bool() const { return static_cast<bool>(packages); }
};

struct PackageLifecycleAdvanceResult
{
	PackageBootstrapError error = PackageBootstrapError::None;
	PackageBootstrapPhase phase = PackageBootstrapPhase::Configure;
	std::size_t completedPhases = 0;
	bool rolledBack = false;
	PackageLifecycleRollbackResult rollback;
	std::exception_ptr callbackException;

	explicit operator bool() const { return error == PackageBootstrapError::None; }
};

struct PackageLifecycleShutdownResult
{
	std::size_t shutdownPhases = 0;
	PackageDeactivationBatchResult deactivation;
	PackageLifecycleRollbackResult bootstrap;

	explicit operator bool() const
	{
		return static_cast<bool>(bootstrap) && static_cast<bool>(deactivation);
	}
};

// Coordinates the complete package bootstrap transaction above the lower-level
// registry. Applications may advance one phase at their established loading
// boundaries or directly to a target phase. A failure unwinds every previously
// completed phase, leaving no half-configured package runtime behind.
class PackageLifecycle
{
public:
	explicit PackageLifecycle(PackageRegistry& packages) : packages_(packages) {}

	PackageLifecycle(const PackageLifecycle&) = delete;
	PackageLifecycle& operator=(const PackageLifecycle&) = delete;
	PackageLifecycle(PackageLifecycle&&) = delete;
	PackageLifecycle& operator=(PackageLifecycle&&) = delete;

	PackageLifecycleAdvanceResult advanceTo(PackageBootstrapPhase target)
	{
		const std::size_t targetIndex = static_cast<std::size_t>(target);
		std::size_t completed = packages_.completedBootstrapPhases();
		if (completed > targetIndex)
			return PackageLifecycleAdvanceResult{
				PackageBootstrapError::None, target, completed, false};

		while (completed <= targetIndex)
		{
			const PackageBootstrapPhase phase =
				static_cast<PackageBootstrapPhase>(completed);
			const PackageBootstrapResult bootstrap = packages_.bootstrapDetailed(phase);
			if (!bootstrap)
			{
				PackageLifecycleRollbackResult rollback = this->rollback();
				mergeRollback(rollback.packages, bootstrap.failedPhaseRollback);
				return PackageLifecycleAdvanceResult{
					bootstrap.error, phase, packages_.completedBootstrapPhases(), true,
					rollback, bootstrap.callbackException};
			}
			completed = packages_.completedBootstrapPhases();
		}
		return PackageLifecycleAdvanceResult{
			PackageBootstrapError::None, target, completed, false};
	}

	PackageLifecycleRollbackResult rollback()
	{
		return PackageLifecycleRollbackResult{packages_.shutdownBootstrap()};
	}

	PackageLifecycleShutdownResult shutdown()
	{
		const PackageLifecycleRollbackResult bootstrap = rollback();
		return PackageLifecycleShutdownResult{
			bootstrap.packages.shutdownPhases, packages_.deactivateAll(), bootstrap};
	}

	std::size_t completedPhases() const
	{
		return packages_.completedBootstrapPhases();
	}
	bool readyToRun() const
	{
		return completedPhases() ==
			static_cast<std::size_t>(PackageBootstrapPhase::StartRuntime) + 1;
	}

private:
	static std::size_t saturatingAdd(std::size_t left, std::size_t right) noexcept
	{
		const std::size_t maximum = std::numeric_limits<std::size_t>::max();
		return right > maximum - left ? maximum : left + right;
	}

	static PackageBootstrapShutdownError mergeRollbackError(
		PackageBootstrapShutdownError left,
		PackageBootstrapShutdownError right) noexcept
	{
		if (left == PackageBootstrapShutdownError::CallbackFailed ||
			right == PackageBootstrapShutdownError::CallbackFailed)
			return PackageBootstrapShutdownError::CallbackFailed;
		if (left == PackageBootstrapShutdownError::OperationInProgress ||
			right == PackageBootstrapShutdownError::OperationInProgress)
			return PackageBootstrapShutdownError::OperationInProgress;
		return PackageBootstrapShutdownError::None;
	}

	static void mergeRollback(
		PackageBootstrapShutdownResult& completedPhases,
		const PackageBootstrapShutdownResult& failedPhase) noexcept
	{
		completedPhases.error =
			mergeRollbackError(completedPhases.error, failedPhase.error);
		completedPhases.shutdownPhases = saturatingAdd(
			completedPhases.shutdownPhases, failedPhase.shutdownPhases);
		completedPhases.callbacks = saturatingAdd(
			completedPhases.callbacks, failedPhase.callbacks);
		completedPhases.callbackFailures = saturatingAdd(
			completedPhases.callbackFailures, failedPhase.callbackFailures);
	}

	PackageRegistry& packages_;
};

#endif
