#ifndef ENGINE_CORE_STATE_CONTROLLER_H
#define ENGINE_CORE_STATE_CONTROLLER_H

#include <optional>
#include <utility>

#include <Engine/Core/StateStack.h>
#include <Engine/Core/StateTransition.h>

// Owns current, previous, and pending application state while StateStack keeps
// overlay history. Hosts may retain legacy scalar mirrors during migration;
// all actual transition policy belongs here.
template<typename State>
class StateController
{
public:
	StateStack<State>& stack() { return states_; }
	const StateStack<State>& stack() const { return states_; }

	const State* current() const
	{
		const auto* entry = states_.current();
		return entry ? &entry->state : nullptr;
	}

	const State* previous() const { return previous_ ? &*previous_ : nullptr; }
	const State* pending() const { return pending_ ? &*pending_ : nullptr; }
	bool hasPending() const { return static_cast<bool>(pending_); }

	void reset(State state)
	{
		states_.reset(std::move(state));
		previous_.reset();
		pending_.reset();
	}

	bool request(State state)
	{
		const bool changed = !pending_ || !(*pending_ == state);
		pending_ = std::move(state);
		return changed;
	}

	bool cancelPending()
	{
		if (!pending_) return false;
		pending_.reset();
		return true;
	}

	template<typename IsOverlay>
	StateTransitionResult commitPending(IsOverlay&& isOverlay)
	{
		if (!pending_) return StateTransitionResult::Unchanged;
		State next = std::move(*pending_);
		pending_.reset();
		return transitionTo(std::move(next), std::forward<IsOverlay>(isOverlay));
	}

	template<typename IsOverlay>
	StateTransitionResult transitionTo(State next, IsOverlay&& isOverlay)
	{
		const State* currentState = current();
		std::optional<State> old;
		if (currentState) old = *currentState;
		const StateTransitionResult result = ApplyStateTransition(
			states_, std::move(next), std::forward<IsOverlay>(isOverlay));
		if (result == StateTransitionResult::Initialized)
		{
			previous_.reset();
		}
		else if (result != StateTransitionResult::Unchanged && old)
		{
			previous_ = std::move(*old);
		}
		return result;
	}

private:
	StateStack<State> states_;
	std::optional<State> previous_;
	std::optional<State> pending_;
};

#endif
