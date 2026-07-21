#ifndef ENGINE_CORE_PACKAGE_LIFECYCLE_H
#define ENGINE_CORE_PACKAGE_LIFECYCLE_H

#include <cstddef>

#include <Engine/Core/PackageApi.h>

struct PackageLifecycleAdvanceResult
{
	PackageBootstrapError error = PackageBootstrapError::None;
	PackageBootstrapPhase phase = PackageBootstrapPhase::Configure;
	std::size_t completedPhases = 0;
	bool rolledBack = false;

	explicit operator bool() const { return error == PackageBootstrapError::None; }
};

struct PackageLifecycleShutdownResult
{
	std::size_t shutdownPhases = 0;
	PackageDeactivationBatchResult deactivation;

	explicit operator bool() const { return static_cast<bool>(deactivation); }
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
				packages_.shutdownBootstrap();
				return PackageLifecycleAdvanceResult{
					error, phase, packages_.completedBootstrapPhases(), true};
			}
			completed = packages_.completedBootstrapPhases();
		}
		return PackageLifecycleAdvanceResult{
			PackageBootstrapError::None, target, completed, false};
	}

	PackageLifecycleShutdownResult shutdown()
	{
		const std::size_t phases = packages_.completedBootstrapPhases();
		packages_.shutdownBootstrap();
		return PackageLifecycleShutdownResult{phases, packages_.deactivateAll()};
	}

private:
	PackageRegistry& packages_;
};

#endif
