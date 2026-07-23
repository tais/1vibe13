#ifndef ENGINE_CORE_SUBSYSTEM_RUNTIME_H
#define ENGINE_CORE_SUBSYSTEM_RUNTIME_H

#include <cstddef>
#include <exception>
#include <functional>
#include <limits>
#include <string>
#include <vector>

enum class SubsystemRuntimeState
{
	Stopped,
	Starting,
	Running,
	Stopping
};

enum class SubsystemStartError
{
	None,
	TransitionInProgress,
	Rejected,
	CallbackException
};

enum class SubsystemStopError
{
	None,
	TransitionInProgress,
	CallbackException
};

constexpr std::size_t NoSubsystem =
	std::numeric_limits<std::size_t>::max();

// A subsystem must either initialize completely or leave no state behind when
// initialize returns false or throws. Once initialization succeeds, shutdown
// must tolerate every later subsystem failing and must not throw deliberately.
// The coordinator still catches shutdown exceptions so one broken boundary
// cannot strand the remaining resources.
struct SubsystemDefinition
{
	std::string name;
	std::function<bool()> initialize;
	std::function<void()> shutdown;

	// Lower values shut down first. Equal values use reverse initialization
	// order, which makes an all-zero definition list a conventional stack.
	std::size_t shutdownOrder = 0;
};

struct SubsystemStopResult
{
	SubsystemStopError error = SubsystemStopError::None;
	std::size_t stopped = 0;
	std::size_t callbackFailures = 0;
	std::size_t firstFailedSubsystem = NoSubsystem;

	explicit operator bool() const { return error == SubsystemStopError::None; }
};

struct SubsystemStartResult
{
	SubsystemStartError error = SubsystemStartError::None;
	std::size_t started = 0;
	std::size_t failedSubsystem = NoSubsystem;
	bool alreadyRunning = false;
	std::exception_ptr callbackException;
	SubsystemStopResult rollback;

	explicit operator bool() const { return error == SubsystemStartError::None; }
};

// Coordinates a fixed process/runtime composition without owning any of its
// subsystems. Startup is a transaction: a rejected or throwing callback rolls
// back every successfully initialized predecessor in deterministic shutdown
// order. Shutdown is best-effort, exact-once, and safe to call repeatedly.
class SubsystemRuntime
{
public:
	explicit SubsystemRuntime(std::vector<SubsystemDefinition> definitions);

	SubsystemRuntime(const SubsystemRuntime&) = delete;
	SubsystemRuntime& operator=(const SubsystemRuntime&) = delete;
	SubsystemRuntime(SubsystemRuntime&&) = delete;
	SubsystemRuntime& operator=(SubsystemRuntime&&) = delete;

	SubsystemStartResult start() noexcept;
	SubsystemStopResult stop() noexcept;

	SubsystemRuntimeState state() const { return state_; }
	std::size_t size() const { return definitions_.size(); }
	std::size_t activeSubsystems() const;
	const std::string& subsystemName(std::size_t index) const;

private:
	SubsystemStopResult stopActive() noexcept;

	std::vector<SubsystemDefinition> definitions_;
	std::vector<std::size_t> shutdownOrder_;
	std::vector<bool> active_;
	SubsystemRuntimeState state_ = SubsystemRuntimeState::Stopped;
};

#endif
