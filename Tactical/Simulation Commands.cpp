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
				// Version-1 replay entries have no incarnation. Reject those
				// legacy-unresolved references rather than guessing after slot reuse.
				if (value.soldier.valid() && value.soldier.slot < TOTAL_SOLDIERS)
				{
					SOLDIERTYPE* soldier = MercPtrs[value.soldier.slot];
					if (soldier != nullptr &&
						soldier->uiUniqueSoldierIdValue == value.soldier.incarnation)
					{
						SendChangeSoldierStanceEvent(soldier, value.stance);
						return CommandDisposition::Applied;
					}
				}
				return CommandDisposition::Discard;
			}
			else if constexpr (std::is_same<Command, BeginFireWeaponCommand>::value)
			{
				if (value.soldier.slot < TOTAL_SOLDIERS)
				{
					SOLDIERTYPE* soldier = MercPtrs[value.soldier.slot];
					if (soldier != nullptr &&
						soldier->uiUniqueSoldierIdValue == value.soldier.incarnation)
					{
						SendBeginFireWeaponEvent(
							soldier, value.targetGrid,
							value.targetLevel, value.targetCubeLevel);
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
	GameContext& game = GetGameContext();
	return ProcessCommandsThrough(
		game.commands(), tick,
		[](const SimulationCommand& command, std::uint64_t, std::uint64_t) {
			return ExecuteSimulationCommand(command);
		},
		[&game](const SimulationCommand&, std::uint64_t, std::uint64_t sequence,
			CommandDisposition disposition) {
			game.commandJournal().recordDisposition(sequence, disposition);
		});
}

std::uint64_t DispatchEndTurnCommandNow(
	std::uint8_t nextTeam, SimulationCommandSource source)
{
	GameContext& game = GetGameContext();
	const std::uint64_t sequence = game.submitCommand(
		ImmediateCommandTick, SimulationCommand{EndTurnCommand{nextTeam, source}});
	ExecuteSimulationCommandsThrough(ImmediateCommandTick);
	return sequence;
}

std::uint64_t DispatchChangeStanceCommandNow(
	std::uint16_t soldierId, std::uint8_t stance, SimulationCommandSource source)
{
	TacticalEntityId soldier;
	if (soldierId < TOTAL_SOLDIERS && MercPtrs[soldierId] != nullptr)
	{
		soldier = TacticalEntityId{
			soldierId, MercPtrs[soldierId]->uiUniqueSoldierIdValue};
	}
	GameContext& game = GetGameContext();
	const std::uint64_t sequence = game.submitCommand(
		ImmediateCommandTick,
		SimulationCommand{ChangeStanceCommand{soldier, stance, source}});
	ExecuteSimulationCommandsThrough(ImmediateCommandTick);
	return sequence;
}

std::uint64_t DispatchBeginFireWeaponCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	SimulationCommandSource source)
{
	GameContext& game = GetGameContext();
	const std::uint64_t sequence = game.submitCommand(
		ImmediateCommandTick,
		SimulationCommand{BeginFireWeaponCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			targetGrid, targetLevel, targetCubeLevel, source}});
	ExecuteSimulationCommandsThrough(ImmediateCommandTick);
	return sequence;
}
