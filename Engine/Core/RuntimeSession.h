#ifndef ENGINE_CORE_RUNTIME_SESSION_H
#define ENGINE_CORE_RUNTIME_SESSION_H

#include <Engine/Core/PackageLifecycle.h>

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
	PackageShutdownFailed
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

// Owns the application lifecycle state and is the single gateway between it
// and package bootstrap. Hosts may still advance package phases at established
// loading boundaries, but shutdown can no longer accidentally bypass the
// package lifecycle transaction. The coordinator owns no package objects.
class RuntimeSession
{
public:
	explicit RuntimeSession(PackageLifecycle& packages) : packages_(packages) {}

	RuntimeSession(const RuntimeSession&) = delete;
	RuntimeSession& operator=(const RuntimeSession&) = delete;
	RuntimeSession(RuntimeSession&&) = delete;
	RuntimeSession& operator=(RuntimeSession&&) = delete;

	EngineLifecycle lifecycle() const { return lifecycle_; }

	RuntimeSessionAdvanceResult advancePackagesTo(PackageBootstrapPhase phase)
	{
		if (lifecycle_ == EngineLifecycle::ShuttingDown)
			return RuntimeSessionAdvanceResult{RuntimeSessionError::InvalidState, {}};
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
		return RuntimeSessionShutdownResult{
			result ? RuntimeSessionError::None
			       : RuntimeSessionError::PackageShutdownFailed,
			result};
	}

	bool beginInitialization()
	{
		if (lifecycle_ != EngineLifecycle::Stopped) return false;
		lifecycle_ = EngineLifecycle::Initializing;
		return true;
	}

	bool cancelInitialization()
	{
		if (lifecycle_ != EngineLifecycle::Initializing) return false;
		lifecycle_ = EngineLifecycle::Stopped;
		return true;
	}

	bool markRunning()
	{
		if (lifecycle_ != EngineLifecycle::Initializing) return false;
		lifecycle_ = EngineLifecycle::Running;
		return true;
	}

	bool beginShutdown()
	{
		if (lifecycle_ != EngineLifecycle::Running &&
			lifecycle_ != EngineLifecycle::Initializing) return false;
		lifecycle_ = EngineLifecycle::ShuttingDown;
		return true;
	}

	bool markStopped()
	{
		if (lifecycle_ != EngineLifecycle::ShuttingDown) return false;
		lifecycle_ = EngineLifecycle::Stopped;
		return true;
	}

private:
	PackageLifecycle& packages_;
	EngineLifecycle lifecycle_ = EngineLifecycle::Stopped;
};

#endif
