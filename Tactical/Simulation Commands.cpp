#include "Simulation Commands.h"

#include <type_traits>
#include <utility>
#include <variant>

#include <Engine/Core/CommandProcessor.h>

#include "Animation Control.h"
#include "GameContext.h"
#include "Map Information.h"
#include "Overhead.h"
#include "Soldier Control.h"
#include "Soldier Functions.h"

namespace
{
	constexpr std::uint64_t ImmediateCommandTick = 0;

	SimulationCommandExecutionSink*& ApplicationExecutionSink() noexcept
	{
		static SimulationCommandExecutionSink* sink = nullptr;
		return sink;
	}

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
		if (ValidateSimulationCommandDomain(command) !=
			SimulationCommandDomainError::None)
			return CommandDisposition::Discard;
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
			else if constexpr (std::is_same<Command, MoveToGridCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier ||
					!IsValidMovementMode(soldier, value.movementMode))
					return CommandDisposition::Discard;

				soldier->usUIMovementMode = value.movementMode;
				soldier->bReverse = value.reverse ? TRUE : FALSE;
				soldier->aiData.ubPendingAction = NO_PENDING_ACTION;
				return soldier->EVENT_InternalGetNewSoldierPath(
					value.destinationGrid, value.movementMode, TRUE,
					value.forceRestart ? TRUE : FALSE)
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else
			{
				return CommandDisposition::Discard;
			}
		}, command);
	}

	template<typename Process>
	CommandProcessingResult ExecuteSimulationCommands(
		Process&& process, SimulationCommandExecutionSink* sink = nullptr)
	{
		GameContext& game = GetGameContext();
		SimulationCommandExecutionSink* const applicationSink =
			ApplicationExecutionSink();
		return process(
			game.commands(),
			[](const SimulationCommand& command, std::uint64_t, std::uint64_t) {
				return ExecuteSimulationCommand(command);
			},
			[&game, applicationSink, sink](const SimulationCommand& command, std::uint64_t tick,
				std::uint64_t sequence,
				CommandDisposition disposition) {
				game.commandJournal().recordDisposition(sequence, disposition);
				if (applicationSink)
					applicationSink->commandProcessed(
						command, tick, sequence, disposition);
				if (sink && sink != applicationSink)
					sink->commandProcessed(command, tick, sequence, disposition);
			});
	}
}

SimulationCommandDomainError ValidateSimulationCommandDomain(
	const SimulationCommand& command) noexcept
{
	if (command.valueless_by_exception())
		return SimulationCommandDomainError::ValuelessCommand;
	return std::visit([](const auto& value) noexcept {
		using Command = typename std::decay<decltype(value)>::type;
		if (!IsValidSimulationCommandSource(value.source))
			return SimulationCommandDomainError::InvalidSource;
		if constexpr (std::is_same<Command, EndTurnCommand>::value)
		{
			return value.nextTeam < MAXTEAMS
				? SimulationCommandDomainError::None
				: SimulationCommandDomainError::InvalidTeam;
		}
		else
		{
			if (!value.soldier.valid() || value.soldier.slot >= TOTAL_SOLDIERS)
				return SimulationCommandDomainError::InvalidActor;
			if constexpr (std::is_same<Command, ChangeStanceCommand>::value)
			{
				return value.stance == ANIM_STAND || value.stance == ANIM_CROUCH ||
					value.stance == ANIM_PRONE
					? SimulationCommandDomainError::None
					: SimulationCommandDomainError::InvalidStance;
			}
			else if constexpr (std::is_same<Command, BeginFireWeaponCommand>::value)
			{
				if (value.targetGrid < 0 || value.targetGrid >= WORLD_MAX)
					return SimulationCommandDomainError::InvalidTargetGrid;
				if (value.targetLevel != FIRST_LEVEL &&
					value.targetLevel != SECOND_LEVEL)
					return SimulationCommandDomainError::InvalidTargetLevel;
				if (value.targetCubeLevel < 0 ||
					value.targetCubeLevel > PROFILE_Z_SIZE)
					return SimulationCommandDomainError::InvalidTargetCubeLevel;
				return SimulationCommandDomainError::None;
			}
			else if constexpr (std::is_same<Command, MoveToGridCommand>::value)
			{
				if (value.destinationGrid < 0 || value.destinationGrid >= WORLD_MAX)
					return SimulationCommandDomainError::InvalidDestinationGrid;
				if (value.movementMode >= NUMANIMATIONSTATES ||
					(gAnimControl[value.movementMode].uiFlags & ANIM_MOVING) == 0)
					return SimulationCommandDomainError::InvalidMovementMode;
				return SimulationCommandDomainError::None;
			}
		}
		return SimulationCommandDomainError::ValuelessCommand;
	}, command);
}

bool BindSimulationCommandExecutionSink(
	SimulationCommandExecutionSink& sink) noexcept
{
	SimulationCommandExecutionSink*& bound = ApplicationExecutionSink();
	if (bound && bound != &sink) return false;
	bound = &sink;
	return true;
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

std::uint64_t DispatchMoveToGridCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source)
{
	GameContext& game = GetGameContext();
	const std::uint64_t sequence = game.submitCommand(
		ImmediateCommandTick,
		SimulationCommand{MoveToGridCommand{
			TacticalEntityId{soldierId, uniqueSoldierId}, destinationGrid,
			movementMode, reverse, forceRestart, source}});
	ExecuteSimulationCommandsThrough(ImmediateCommandTick);
	return sequence;
}
