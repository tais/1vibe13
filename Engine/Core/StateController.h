#ifndef ENGINE_CORE_STATE_CONTROLLER_H
#define ENGINE_CORE_STATE_CONTROLLER_H

#include <optional>
#include <utility>
#include <vector>

#include <Engine/Core/StateStack.h>
#include <Engine/Core/StateTransition.h>

// Owns current, previous, and pending application state while StateStack keeps
// overlay history. Scoped visible-state overrides support operations whose
// domain behavior depends on a temporary state without creating false history.
template<typename State>
class StateController
{
public:
	class ScopedCurrentOverride
	{
	public:
		ScopedCurrentOverride(const ScopedCurrentOverride&) = delete;
		ScopedCurrentOverride& operator=(const ScopedCurrentOverride&) = delete;
		ScopedCurrentOverride(ScopedCurrentOverride&&) = delete;
		ScopedCurrentOverride& operator=(ScopedCurrentOverride&&) = delete;

		~ScopedCurrentOverride()
		{
			if (owner_)
				owner_->currentOverrides_.pop_back();
		}

	private:
		friend class StateController;

		ScopedCurrentOverride(StateController& owner, State state)
			: owner_(&owner)
		{
			owner.currentOverrides_.push_back(std::move(state));
		}

		StateController* owner_;
	};

	StateStack<State>& stack() { return states_; }
	const StateStack<State>& stack() const { return states_; }

	const State* current() const
	{
		if (!currentOverrides_.empty())
			return &currentOverrides_.back();
		const auto* entry = states_.current();
		return entry ? &entry->state : nullptr;
	}

	const State* previous() const { return previous_ ? &*previous_ : nullptr; }
	const State* pending() const { return pending_ ? &*pending_ : nullptr; }
	bool hasPending() const { return static_cast<bool>(pending_); }

	// Temporarily changes the state visible to legacy/domain operations without
	// recording a navigation transition. The underlying transition and overlay
	// state remains live and is revealed when the guard leaves scope.
	[[nodiscard]] ScopedCurrentOverride scopedCurrentOverride(State state)
	{
		return ScopedCurrentOverride(*this, std::move(state));
	}

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
		const auto* currentEntry = states_.current();
		std::optional<State> old;
		if (currentEntry) old = currentEntry->state;
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
	std::vector<State> currentOverrides_;
	std::optional<State> previous_;
	std::optional<State> pending_;
};

#endif
