#ifndef ENGINE_CORE_STATE_TRANSITION_H
#define ENGINE_CORE_STATE_TRANSITION_H

#include <utility>

#include <Engine/Core/StateStack.h>

enum class StateTransitionResult
{
	Initialized,
	Unchanged,
	Replaced,
	OverlayPushed,
	OverlayPopped
};

template<typename State, typename IsOverlay>
StateTransitionResult ApplyStateTransition(
	StateStack<State>& states, State next, IsOverlay&& isOverlay)
{
	if (states.empty())
	{
		states.reset(std::move(next));
		return StateTransitionResult::Initialized;
	}
	const auto* current = states.current();
	if (current->state == next) return StateTransitionResult::Unchanged;
	const auto* underlay = states.underlay();
	if (current->overlay && underlay && underlay->state == next)
	{
		states.popOverlay();
		return StateTransitionResult::OverlayPopped;
	}
	if (std::forward<IsOverlay>(isOverlay)(next))
	{
		states.pushOverlay(std::move(next));
		return StateTransitionResult::OverlayPushed;
	}
	if (current->overlay) states.popOverlay();
	states.replace(std::move(next));
	return StateTransitionResult::Replaced;
}

#endif
