#include "Simulation Commands.h"

#include <type_traits>
#include <utility>
#include <variant>

#include <Engine/Core/CommandProcessor.h>

#include "GameContext.h"
#include "Map Information.h"
#include "Overhead.h"
#include "Soldier Control.h"

namespace
{
	constexpr std::uint64_t ImmediateCommandTick = 0;

	bool HasEndTurnExecutionContext() noexcept
	{
		constexpr UINT32 RequiredFlags = TURNBASED | INCOMBAT;
		return gfWorldLoaded &&
			(gTacticalStatus.uiFlags & RequiredFlags) == RequiredFlags &&
			gTacticalStatus.ubCurrentTeam < MAXTEAMS;
	}

	SOLDIERTYPE* ResolveLiveCommandActor(TacticalEntityId actor) noexcept
	{
		if (!gfWorldLoaded || !actor.valid() || actor.slot >= TOTAL_SOLDIERS)
			return nullptr;
		SOLDIERTYPE* soldier = MercPtrs[actor.slot];
		if (!soldier || !soldier->bActive || !soldier->bInSector ||
			static_cast<std::uint16_t>(soldier->ubID) != actor.slot ||
			soldier->uiUniqueSoldierIdValue != actor.incarnation)
			return nullptr;
		return soldier;
	}

	CommandDisposition ExecuteSimulationCommand(const SimulationCommand& command)
	{
		return std::visit([](const auto& value) {
			using Command = typename std::decay<decltype(value)>::type;
			if constexpr (std::is_same<Command, EndTurnCommand>::value)
			{
				if (value.nextTeam >= MAXTEAMS || !HasEndTurnExecutionContext())
					return CommandDisposition::Discard;
				EndTurn(value.nextTeam);
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, ChangeStanceCommand>::value)
			{
				// Version-1 replay entries have no incarnation. Reject those
				// legacy-unresolved references rather than guessing after slot reuse.
				if (SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier))
				{
					SendChangeSoldierStanceEvent(soldier, value.stance);
					return CommandDisposition::Applied;
				}
				return CommandDisposition::Discard;
			}
			else if constexpr (std::is_same<Command, BeginFireWeaponCommand>::value)
			{
				if (SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier))
				{
					SendBeginFireWeaponEvent(
						soldier, value.targetGrid,
						value.targetLevel, value.targetCubeLevel);
					return CommandDisposition::Applied;
				}
				return CommandDisposition::Discard;
			}
			else
			{
				// New value commands fail closed until their legacy executor is
				// deliberately installed by the application adapter.
				return CommandDisposition::Discard;
			}
		}, command);
	}

	template<typename Process>
	CommandProcessingResult ExecuteSimulationCommands(
		Process&& process, SimulationCommandExecutionSink* sink = nullptr)
	{
		GameContext& game = GetGameContext();
		return process(
			game.commands(),
			[](const SimulationCommand& command, std::uint64_t, std::uint64_t) {
				return ExecuteSimulationCommand(command);
			},
			[&game, sink](const SimulationCommand& command, std::uint64_t tick,
				std::uint64_t sequence,
				CommandDisposition disposition) {
				game.commandJournal().recordDisposition(sequence, disposition);
				if (sink)
					sink->commandProcessed(command, tick, sequence, disposition);
			});
	}
}

CommandProcessingResult ExecuteSimulationCommandsThrough(std::uint64_t tick)
{
	return ExecuteSimulationCommands(
		[tick](auto& queue, auto&& handler, auto&& observer) {
			return ProcessCommandsThrough(
				queue, tick, std::forward<decltype(handler)>(handler),
				std::forward<decltype(observer)>(observer));
		});
}

CommandProcessingResult ExecuteSimulationCommandsThrough(
	std::uint64_t tick, std::size_t maximumCommands)
{
	return ExecuteSimulationCommands(
		[tick, maximumCommands](auto& queue, auto&& handler, auto&& observer) {
			return ProcessCommandsThrough(
				queue, tick, maximumCommands,
				std::forward<decltype(handler)>(handler),
				std::forward<decltype(observer)>(observer));
		});
}

CommandProcessingResult ExecuteSimulationCommandsThrough(
	std::uint64_t tick, std::size_t maximumCommands,
	SimulationCommandExecutionSink& sink)
{
	return ExecuteSimulationCommands(
		[tick, maximumCommands](auto& queue, auto&& handler, auto&& observer) {
			return ProcessCommandsThrough(
				queue, tick, maximumCommands,
				std::forward<decltype(handler)>(handler),
				std::forward<decltype(observer)>(observer));
		}, &sink);
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
