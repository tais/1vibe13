#ifndef ENGINE_CORE_STATE_REGISTRY_H
#define ENGINE_CORE_STATE_REGISTRY_H

#include <cstddef>
#include <exception>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

enum class StateRegistrationError
{
	None,
	DuplicateId,
	InvalidCallbacks,
	OperationInProgress
};

enum class StateInitializationError
{
	None,
	NotRegistered,
	AlreadyInitialized,
	Rejected,
	CallbackException,
	OperationInProgress
};

enum class StateHandleError
{
	None,
	NotRegistered,
	NotInitialized,
	CallbackException,
	OperationInProgress
};

enum class StateShutdownError
{
	None,
	NotRegistered,
	NotInitialized,
	CallbackException,
	OperationInProgress
};

template<typename StateId>
struct StateCallbacks
{
	std::function<bool()> initialize;
	std::function<StateId()> handle;
	std::function<void()> shutdown;
};

template<typename StateId>
struct StateHandleResult
{
	StateHandleError error = StateHandleError::None;
	std::optional<StateId> nextState;
	std::exception_ptr callbackException;

	explicit operator bool() const
	{
		return error == StateHandleError::None && nextState.has_value();
	}
};

// Game-agnostic registry for application states/screens. The registry owns
// callback values but not anything captured by them; captured application
// objects must outlive their registration. Lifecycle mutation and dispatch are
// serialized by the host. Reentrant lifecycle mutation is rejected before a
// callback can invalidate the entry that owns it.
template<typename StateId>
class StateRegistry
{
public:
	StateRegistrationError registerState(
		StateId id, StateCallbacks<StateId> callbacks)
	{
		if (operationInProgress_) return StateRegistrationError::OperationInProgress;
		if (!callbacks.initialize || !callbacks.handle || !callbacks.shutdown)
			return StateRegistrationError::InvalidCallbacks;
		if (findEntry(id) != entries_.end())
			return StateRegistrationError::DuplicateId;
		entries_.push_back(Entry{id, std::move(callbacks), false});
		return StateRegistrationError::None;
	}

	bool contains(StateId id) const { return findEntry(id) != entries_.end(); }
	bool isInitialized(StateId id) const
	{
		const auto found = findEntry(id);
		return found != entries_.end() && found->initialized;
	}
	std::size_t size() const { return entries_.size(); }
	std::size_t initializedCount() const
	{
		std::size_t count = 0;
		for (const Entry& entry : entries_)
			if (entry.initialized) ++count;
		return count;
	}

	StateInitializationError initialize(StateId id)
	{
		if (operationInProgress_) return StateInitializationError::OperationInProgress;
		auto found = findEntry(id);
		if (found == entries_.end())
			return StateInitializationError::NotRegistered;
		if (found->initialized)
			return StateInitializationError::AlreadyInitialized;
		OperationGuard operation(operationInProgress_);
		try
		{
			if (!found->callbacks.initialize())
				return StateInitializationError::Rejected;
		}
		catch (...)
		{
			return StateInitializationError::CallbackException;
		}
		found->initialized = true;
		return StateInitializationError::None;
	}

	StateHandleResult<StateId> handle(StateId id)
	{
		if (operationInProgress_)
			return {StateHandleError::OperationInProgress, std::nullopt};
		auto found = findEntry(id);
		if (found == entries_.end())
			return {StateHandleError::NotRegistered, std::nullopt};
		if (!found->initialized)
			return {StateHandleError::NotInitialized, std::nullopt};
		OperationGuard operation(operationInProgress_);
		try
		{
			return {StateHandleError::None, found->callbacks.handle()};
		}
		catch (...)
		{
			return {StateHandleError::CallbackException, std::nullopt,
				std::current_exception()};
		}
	}

	StateShutdownError shutdown(StateId id)
	{
		if (operationInProgress_) return StateShutdownError::OperationInProgress;
		auto found = findEntry(id);
		if (found == entries_.end())
			return StateShutdownError::NotRegistered;
		if (!found->initialized)
			return StateShutdownError::NotInitialized;
		OperationGuard operation(operationInProgress_);
		try
		{
			found->callbacks.shutdown();
		}
		catch (...)
		{
			found->initialized = false;
			return StateShutdownError::CallbackException;
		}
		found->initialized = false;
		return StateShutdownError::None;
	}

private:
	class OperationGuard
	{
	public:
		explicit OperationGuard(bool& active) : active_(active) { active_ = true; }
		~OperationGuard() { active_ = false; }
	private:
		bool& active_;
	};

	struct Entry
	{
		StateId id;
		StateCallbacks<StateId> callbacks;
		bool initialized;
	};

	using Entries = std::vector<Entry>;
	typename Entries::iterator findEntry(StateId id)
	{
		for (auto entry = entries_.begin(); entry != entries_.end(); ++entry)
			if (entry->id == id) return entry;
		return entries_.end();
	}
	typename Entries::const_iterator findEntry(StateId id) const
	{
		for (auto entry = entries_.begin(); entry != entries_.end(); ++entry)
			if (entry->id == id) return entry;
		return entries_.end();
	}

	Entries entries_;
	bool operationInProgress_ = false;
};

#endif
