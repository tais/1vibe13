#ifndef ENGINE_CORE_PACKAGE_LIFECYCLE_H
#define ENGINE_CORE_PACKAGE_LIFECYCLE_H

#include <cstddef>

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
			const PackageBootstrapError error = packages_.bootstrap(phase);
			if (error != PackageBootstrapError::None)
			{
				const PackageLifecycleRollbackResult rollback = this->rollback();
				return PackageLifecycleAdvanceResult{
					error, phase, packages_.completedBootstrapPhases(), true, rollback};
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
	PackageRegistry& packages_;
};

#endif
