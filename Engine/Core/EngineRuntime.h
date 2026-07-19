#ifndef ENGINE_CORE_ENGINE_RUNTIME_H
#define ENGINE_CORE_ENGINE_RUNTIME_H

#include <cstdint>

#include <Engine/Core/ContentApi.h>
#include <Engine/Core/EngineServices.h>
#include <Engine/Core/PackageApi.h>
#include <Engine/Core/StateStack.h>

enum class EngineLifecycle
{
	Stopped,
	Initializing,
	Running,
	ShuttingDown
};

// Campaign-agnostic composition root for reusable engine state. The
// application owns platform adapters and package objects; both must outlive
// this runtime and its non-owning service/package references.
template<typename ScreenId = std::uint32_t>
class EngineRuntime
{
public:
	explicit EngineRuntime(
		EngineServices services = EngineServices::defaults(),
		ContentApiVersion supportedContentApi = CurrentContentApiVersion)
		: content_(supportedContentApi), packages_(content_, services)
	{
	}

	// PackageRegistry keeps references to this runtime's ContentRegistry.
	// Copying or moving the aggregate would leave those references pointing at
	// the source runtime (and eventually dangling), so runtime identity is
	// deliberately stable for its entire lifetime.
	EngineRuntime(const EngineRuntime&) = delete;
	EngineRuntime& operator=(const EngineRuntime&) = delete;
	EngineRuntime(EngineRuntime&&) = delete;
	EngineRuntime& operator=(EngineRuntime&&) = delete;

	EngineServices& services() { return packages_.services(); }
	const EngineServices& services() const { return packages_.services(); }
	LogSink& log() { return services().log; }
	StateStack<ScreenId>& screens() { return screens_; }
	const StateStack<ScreenId>& screens() const { return screens_; }
	ContentRegistry& content() { return content_; }
	const ContentRegistry& content() const { return content_; }
	PackageRegistry& packages() { return packages_; }
	const PackageRegistry& packages() const { return packages_; }

	EngineLifecycle lifecycle() const { return lifecycle_; }

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
	StateStack<ScreenId> screens_;
	ContentRegistry content_;
	PackageRegistry packages_;
	EngineLifecycle lifecycle_ = EngineLifecycle::Stopped;
};

#endif
