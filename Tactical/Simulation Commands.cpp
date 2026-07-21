#include "Simulation Commands.h"

#include <type_traits>
#include <variant>

#include "GameContext.h"
#include "Overhead.h"

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
		}, command);
	}
}

void ExecuteSimulationCommandsThrough(std::uint64_t tick)
{
	auto ready = GetGameContext().commands().drainThrough(tick);
	for (const auto& entry : ready)
	{
		ExecuteSimulationCommand(entry.command);
	}
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
