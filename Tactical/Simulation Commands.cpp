#include "Simulation Commands.h"

#include <type_traits>
#include <variant>

#include <Engine/Core/CommandDispatch.h>

#include "GameContext.h"
#include "Overhead.h"
#include "Soldier Control.h"

namespace
{
	constexpr std::uint64_t ImmediateCommandTick = 0;

	void ExecuteSimulationCommand(const SimulationCommand& command)
	{
		std::visit([](const auto& value) {
			using Command = typename std::decay<decltype(value)>::type;
			if constexpr (std::is_same<Command, EndTurnCommand>::value)
			{
				EndTurn(value.nextTeam);
			}
			else if constexpr (std::is_same<Command, ChangeStanceCommand>::value)
			{
				if (value.soldierId < TOTAL_SOLDIERS)
				{
					SOLDIERTYPE* soldier = MercPtrs[value.soldierId];
					if (soldier != nullptr)
					{
						SendChangeSoldierStanceEvent(soldier, value.stance);
					}
				}
			}
		}, command);
	}
}

void ExecuteSimulationCommandsThrough(std::uint64_t tick)
{
	DispatchCommandsThrough(
		GetGameContext().commands(), tick,
		[](const SimulationCommand& command, std::uint64_t, std::uint64_t) {
			ExecuteSimulationCommand(command);
		});
}

std::uint64_t DispatchEndTurnCommandNow(
	std::uint8_t nextTeam, SimulationCommandSource source)
{
	auto& commands = GetGameContext().commands();
	const std::uint64_t sequence = commands.enqueue(
		ImmediateCommandTick, SimulationCommand{EndTurnCommand{nextTeam, source}});
	ExecuteSimulationCommandsThrough(ImmediateCommandTick);
	return sequence;
}

std::uint64_t DispatchChangeStanceCommandNow(
	std::uint16_t soldierId, std::uint8_t stance, SimulationCommandSource source)
{
	auto& commands = GetGameContext().commands();
	const std::uint64_t sequence = commands.enqueue(
		ImmediateCommandTick,
		SimulationCommand{ChangeStanceCommand{soldierId, stance, source}});
	ExecuteSimulationCommandsThrough(ImmediateCommandTick);
	return sequence;
}
