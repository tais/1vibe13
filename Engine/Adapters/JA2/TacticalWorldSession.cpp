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

bool TacticalWorldSession::addTeamMember(std::size_t team) noexcept
{
	if (team >= state_.teamPopulations.size()) return false;
	Snapshot::TeamPopulation& population = state_.teamPopulations[team];
	if (population.menInSector == 0) population.active = 1;
	if (population.menInSector ==
		std::numeric_limits<std::int16_t>::max())
		return false;
	++population.menInSector;
	return true;
}

bool TacticalWorldSession::removeTeamMember(
	std::size_t team,
	bool& underflow,
	std::int16_t& observedCount) noexcept
{
	underflow = false;
	observedCount = 0;
	if (team >= state_.teamPopulations.size()) return false;
	Snapshot::TeamPopulation& population = state_.teamPopulations[team];
	if (population.menInSector ==
		std::numeric_limits<std::int16_t>::min())
	{
		observedCount = std::numeric_limits<std::int16_t>::min();
		population.menInSector = 0;
		underflow = true;
		return true;
	}
	--population.menInSector;
	observedCount = population.menInSector;
	if (population.menInSector == 0)
	{
		population.active = 0;
	}
	else if (population.menInSector < 0)
	{
		population.menInSector = 0;
		underflow = true;
	}
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
