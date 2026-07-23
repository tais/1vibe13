#include <Engine/Core/SubsystemRuntime.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

SubsystemRuntime::SubsystemRuntime(
	std::vector<SubsystemDefinition> definitions)
	: definitions_(std::move(definitions)),
	  shutdownOrder_(definitions_.size()),
	  active_(definitions_.size(), false)
{
	for (std::size_t index = 0; index < definitions_.size(); ++index)
	{
		const SubsystemDefinition& definition = definitions_[index];
		if (definition.name.empty())
			throw std::invalid_argument("subsystem name cannot be empty");
		if (!definition.initialize)
			throw std::invalid_argument(
				"subsystem initialize callback cannot be empty: " +
				definition.name);
		if (!definition.shutdown)
			throw std::invalid_argument(
				"subsystem shutdown callback cannot be empty: " +
				definition.name);
		shutdownOrder_[index] = index;
	}
	std::sort(shutdownOrder_.begin(), shutdownOrder_.end(),
		[this](std::size_t left, std::size_t right)
		{
			const std::size_t leftOrder = definitions_[left].shutdownOrder;
			const std::size_t rightOrder = definitions_[right].shutdownOrder;
			if (leftOrder != rightOrder) return leftOrder < rightOrder;
			return left > right;
		});
}

SubsystemStartResult SubsystemRuntime::start() noexcept
{
	if (state_ == SubsystemRuntimeState::Running)
	{
		SubsystemStartResult result;
		result.alreadyRunning = true;
		return result;
	}
	if (state_ != SubsystemRuntimeState::Stopped)
	{
		SubsystemStartResult result;
		result.error = SubsystemStartError::TransitionInProgress;
		return result;
	}

	state_ = SubsystemRuntimeState::Starting;
	for (std::size_t index = 0; index < definitions_.size(); ++index)
	{
		bool initialized = false;
		try
		{
			initialized = definitions_[index].initialize();
		}
		catch (...)
		{
			SubsystemStartResult result;
			result.error = SubsystemStartError::CallbackException;
			result.started = index;
			result.failedSubsystem = index;
			result.callbackException = std::current_exception();
			result.rollback = stopActive();
			return result;
		}
		if (!initialized)
		{
			SubsystemStartResult result;
			result.error = SubsystemStartError::Rejected;
			result.started = index;
			result.failedSubsystem = index;
			result.rollback = stopActive();
			return result;
		}
		active_[index] = true;
	}
	state_ = SubsystemRuntimeState::Running;

	SubsystemStartResult result;
	result.started = definitions_.size();
	return result;
}

SubsystemStopResult SubsystemRuntime::stop() noexcept
{
	if (state_ == SubsystemRuntimeState::Stopped)
		return SubsystemStopResult{};
	if (state_ != SubsystemRuntimeState::Running)
	{
		SubsystemStopResult result;
		result.error = SubsystemStopError::TransitionInProgress;
		return result;
	}
	return stopActive();
}

std::size_t SubsystemRuntime::activeSubsystems() const
{
	return static_cast<std::size_t>(
		std::count(active_.begin(), active_.end(), true));
}

const std::string& SubsystemRuntime::subsystemName(std::size_t index) const
{
	if (index >= definitions_.size())
		throw std::out_of_range("subsystem index is out of range");
	return definitions_[index].name;
}

SubsystemStopResult SubsystemRuntime::stopActive() noexcept
{
	state_ = SubsystemRuntimeState::Stopping;
	SubsystemStopResult result;
	for (const std::size_t index : shutdownOrder_)
	{
		if (!active_[index]) continue;

		// Retire ownership before crossing the callback. A reentrant stop cannot
		// run this boundary twice even if the callback itself asks to stop.
		active_[index] = false;
		++result.stopped;
		try
		{
			definitions_[index].shutdown();
		}
		catch (...)
		{
			if (result.callbackFailures == 0)
				result.firstFailedSubsystem = index;
			++result.callbackFailures;
			result.error = SubsystemStopError::CallbackException;
		}
	}
	state_ = SubsystemRuntimeState::Stopped;
	return result;
}
