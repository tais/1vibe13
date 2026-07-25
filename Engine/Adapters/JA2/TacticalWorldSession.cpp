#include <Engine/Adapters/JA2/TacticalWorldSession.h>

#include <limits>

namespace
{
std::uint64_t NextNonZero(std::uint64_t value) noexcept
{
	++value;
	return value == 0 ? 1 : value;
}

std::uint64_t IncrementSaturated(std::uint64_t value) noexcept
{
	return value == std::numeric_limits<std::uint64_t>::max() ? value : value + 1;
}
}

std::uint64_t TacticalWorldSession::commitLoad() noexcept
{
	state_.worldGeneration = NextNonZero(state_.worldGeneration);
	state_.turnSerial = 1;
	state_.turn.pendingCombatActions = 0;
	state_.loaded = true;
	return state_.worldGeneration;
}

void TacticalWorldSession::unload() noexcept
{
	state_.loaded = false;
	state_.turnSerial = 0;
	state_.turn.pendingCombatActions = 0;
}

void TacticalWorldSession::beginTeamTurn() noexcept
{
	if (!state_.loaded || state_.worldGeneration == 0) return;
	if (state_.turnSerial == 0) state_.turnSerial = 1;
	else state_.turnSerial = IncrementSaturated(state_.turnSerial);
}

bool TacticalWorldSession::beginCombatAction() noexcept
{
	if (state_.turn.pendingCombatActions ==
		std::numeric_limits<std::uint32_t>::max())
		return false;
	++state_.turn.pendingCombatActions;
	return true;
}

bool TacticalWorldSession::completeCombatAction() noexcept
{
	if (state_.turn.pendingCombatActions == 0) return false;
	--state_.turn.pendingCombatActions;
	return true;
}

void TacticalWorldSession::restore(Snapshot state) noexcept
{
	if (!state.loaded || state.worldGeneration == 0)
	{
		state.loaded = false;
		state.turnSerial = 0;
		state.turn.pendingCombatActions = 0;
	}
	else if (state.turnSerial == 0)
	{
		state.turnSerial = 1;
	}
	state_ = state;
}
