#include "Simulation Commands.h"

#include <type_traits>
#include <variant>

#include <Engine/Core/CommandProcessor.h>

#include "GameContext.h"
#include "Overhead.h"
#include "Soldier Control.h"

namespace
{
	constexpr std::uint64_t ImmediateCommandTick = 0;

	CommandDisposition ExecuteSimulationCommand(const SimulationCommand& command)
	{
		return std::visit([](const auto& value) {
			using Command = typename std::decay<decltype(value)>::type;
			if constexpr (std::is_same<Command, EndTurnCommand>::value)
			{
				EndTurn(value.nextTeam);
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, ChangeStanceCommand>::value)
			{
				if (value.soldierId < TOTAL_SOLDIERS)
				{
					SOLDIERTYPE* soldier = MercPtrs[value.soldierId];
					if (soldier != nullptr)
					{
						SendChangeSoldierStanceEvent(soldier, value.stance);
						return CommandDisposition::Applied;
					}
				}
				return CommandDisposition::Discard;
			}
		}, command);
	}
}

CommandProcessingResult ExecuteSimulationCommandsThrough(std::uint64_t tick)
{
	return ProcessCommandsThrough(
		GetGameContext().commands(), tick,
		[](const SimulationCommand& command, std::uint64_t, std::uint64_t) {
			return ExecuteSimulationCommand(command);
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
