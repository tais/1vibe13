#ifndef ENGINE_CORE_RUNTIME_SESSION_H
#define ENGINE_CORE_RUNTIME_SESSION_H

#include <cstddef>

#include <Engine/Core/PackageLifecycle.h>
#include <Engine/Core/RuntimeConfiguration.h>
#include <Engine/Core/ServiceCatalog.h>

enum class EngineLifecycle
{
	Stopped,
	Initializing,
	Running,
	ShuttingDown
};

enum class RuntimeSessionError
{
	None,
	InvalidState,
	PackageBootstrapFailed,
	PackageShutdownFailed,
	PackageBootstrapIncomplete,
	PackageRollbackFailed,
	PackageShutdownIncomplete
};

struct RuntimeSessionAdvanceResult
{
	RuntimeSessionError error = RuntimeSessionError::None;
	PackageLifecycleAdvanceResult packages;

	explicit operator bool() const { return error == RuntimeSessionError::None; }
};

struct RuntimeSessionShutdownResult
{
	RuntimeSessionError error = RuntimeSessionError::None;
	PackageLifecycleShutdownResult packages;

	explicit operator bool() const { return error == RuntimeSessionError::None; }
};

struct RuntimeSessionTransitionResult
{
	RuntimeSessionError error = RuntimeSessionError::None;
	EngineLifecycle lifecycle = EngineLifecycle::Stopped;
	std::size_t completedPackagePhases = 0;
	PackageLifecycleRollbackResult rollback;

	explicit operator bool() const { return error == RuntimeSessionError::None; }
};

// Owns the application lifecycle state and is the single gateway between it
// and package bootstrap. Hosts may still advance package phases at established
// loading boundaries, but shutdown can no longer accidentally bypass the
// package lifecycle transaction. The coordinator owns no package objects.
class RuntimeSession
{
public:
	RuntimeSession(PackageLifecycle& packages, ServiceCatalog& extensionServices,
		RuntimeConfiguration& configuration)
		: packages_(packages), extensionServices_(extensionServices),
		  configuration_(configuration) {}

	RuntimeSession(const RuntimeSession&) = delete;
	RuntimeSession& operator=(const RuntimeSession&) = delete;
	RuntimeSession(RuntimeSession&&) = delete;
	RuntimeSession& operator=(RuntimeSession&&) = delete;

	EngineLifecycle lifecycle() const { return lifecycle_; }

	RuntimeSessionAdvanceResult advancePackagesTo(PackageBootstrapPhase phase)
	{
		if (lifecycle_ == EngineLifecycle::ShuttingDown)
			return RuntimeSessionAdvanceResult{RuntimeSessionError::InvalidState, {}};
		extensionServices_.seal();
		configuration_.seal();
		PackageLifecycleAdvanceResult result = packages_.advanceTo(phase);
		return RuntimeSessionAdvanceResult{
			result ? RuntimeSessionError::None
			       : RuntimeSessionError::PackageBootstrapFailed,
			result};
	}

	RuntimeSessionShutdownResult shutdownPackages()
	{
		if (lifecycle_ != EngineLifecycle::ShuttingDown)
			return RuntimeSessionShutdownResult{RuntimeSessionError::InvalidState, {}};
		PackageLifecycleShutdownResult result = packages_.shutdown();
		if (!result.bootstrap) shutdownRollbackFailed_ = true;
		const bool completed = static_cast<bool>(result) && !shutdownRollbackFailed_;
		if (completed) shutdownCompleted_ = true;
		return RuntimeSessionShutdownResult{
			completed ? RuntimeSessionError::None
			          : RuntimeSessionError::PackageShutdownFailed,
			result};
	}

	RuntimeSessionTransitionResult tryBeginInitialization()
	{
		if (lifecycle_ != EngineLifecycle::Stopped)
			return transitionError(RuntimeSessionError::InvalidState);
		extensionServices_.seal();
		configuration_.seal();
		shutdownCompleted_ = false;
		shutdownRollbackFailed_ = false;
		lifecycle_ = EngineLifecycle::Initializing;
		return transitionSuccess();
	}

	RuntimeSessionTransitionResult tryCancelInitialization()
	{
		if (lifecycle_ != EngineLifecycle::Initializing)
			return transitionError(RuntimeSessionError::InvalidState);
		const PackageLifecycleRollbackResult rollback = packages_.rollback();
		if (rollback.packages.error ==
			PackageBootstrapShutdownError::OperationInProgress)
		{
			return RuntimeSessionTransitionResult{
				RuntimeSessionError::PackageRollbackFailed, lifecycle_,
				packages_.completedPhases(), rollback};
		}
		shutdownCompleted_ = false;
		shutdownRollbackFailed_ = false;
		lifecycle_ = EngineLifecycle::Stopped;
		return RuntimeSessionTransitionResult{
			rollback ? RuntimeSessionError::None
			         : RuntimeSessionError::PackageRollbackFailed,
			lifecycle_, packages_.completedPhases(), rollback};
	}

	RuntimeSessionTransitionResult tryMarkRunning()
	{
		if (lifecycle_ != EngineLifecycle::Initializing)
			return transitionError(RuntimeSessionError::InvalidState);
		if (!packages_.readyToRun())
			return transitionError(RuntimeSessionError::PackageBootstrapIncomplete);
		lifecycle_ = EngineLifecycle::Running;
		return transitionSuccess();
	}

	RuntimeSessionTransitionResult tryBeginShutdown()
	{
		if (lifecycle_ != EngineLifecycle::Running &&
			lifecycle_ != EngineLifecycle::Initializing)
			return transitionError(RuntimeSessionError::InvalidState);
		shutdownCompleted_ = false;
		shutdownRollbackFailed_ = false;
		lifecycle_ = EngineLifecycle::ShuttingDown;
		return transitionSuccess();
	}

	RuntimeSessionTransitionResult tryMarkStopped()
	{
		if (lifecycle_ != EngineLifecycle::ShuttingDown)
			return transitionError(RuntimeSessionError::InvalidState);
		if (!shutdownCompleted_)
			return transitionError(RuntimeSessionError::PackageShutdownIncomplete);
		lifecycle_ = EngineLifecycle::Stopped;
		return transitionSuccess();
	}

	// Source-compatible convenience wrappers for established hosts. New code
	// can use the try* forms when it needs the structured failure reason and
	// rollback diagnostics.
	bool beginInitialization() { return static_cast<bool>(tryBeginInitialization()); }
	bool cancelInitialization() { return static_cast<bool>(tryCancelInitialization()); }
	bool markRunning() { return static_cast<bool>(tryMarkRunning()); }
	bool beginShutdown() { return static_cast<bool>(tryBeginShutdown()); }
	bool markStopped() { return static_cast<bool>(tryMarkStopped()); }

private:
	RuntimeSessionTransitionResult transitionSuccess() const
	{
		return RuntimeSessionTransitionResult{
			RuntimeSessionError::None, lifecycle_, packages_.completedPhases(), {}};
	}
	RuntimeSessionTransitionResult transitionError(RuntimeSessionError error) const
	{
		return RuntimeSessionTransitionResult{
			error, lifecycle_, packages_.completedPhases(), {}};
	}

	PackageLifecycle& packages_;
	ServiceCatalog& extensionServices_;
	RuntimeConfiguration& configuration_;
	EngineLifecycle lifecycle_ = EngineLifecycle::Stopped;
	bool shutdownCompleted_ = false;
	bool shutdownRollbackFailed_ = false;
};

#endif
