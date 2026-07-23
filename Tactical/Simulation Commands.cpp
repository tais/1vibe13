#include "Simulation Commands.h"

#include <stdexcept>
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
#include "TacticalEntityHost.h"

namespace
{
	struct SimulationCommandFrameBudget
	{
		std::uint64_t frameSequence = 0;
		std::size_t maximumCommands = 64;
		std::size_t consumedCommands = 0;
		bool initialized = false;
	};

	SimulationCommandFrameBudget& FrameBudget() noexcept
	{
		static SimulationCommandFrameBudget budget;
		return budget;
	}

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
		if (!gfWorldLoaded) return nullptr;
		SOLDIERTYPE* soldier = ResolveJa2TacticalEntity(actor);
		if (!soldier || !soldier->bInSector) return nullptr;
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
				if (value.pendingAction == TacticalPendingActionPolicy::Clear)
					soldier->aiData.ubPendingAction = NO_PENDING_ACTION;
				return soldier->EVENT_InternalGetNewSoldierPath(
					value.destinationGrid, value.movementMode,
					static_cast<BOOLEAN>(value.origin),
					value.forceRestart ? TRUE : FALSE)
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (std::is_same<Command, SetFacingCommand>::value)
			{
				if (SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier))
				{
					SendSoldierSetDesiredDirectionEvent(soldier, value.direction);
					return CommandDisposition::Applied;
				}
				return CommandDisposition::Discard;
			}
			else if constexpr (std::is_same<Command, SetStealthModeCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier ||
					(soldier->flags.uiStatusFlags & SOLDIER_VEHICLE) != 0)
					return CommandDisposition::Discard;
				soldier->bStealthMode = value.enabled ? TRUE : FALSE;
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, StopMovementCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier) return CommandDisposition::Discard;
				soldier->flags.fDelayedMovement = FALSE;
				soldier->pathing.sFinalDestination = soldier->sGridNo;
				soldier->StopSoldier();
				return CommandDisposition::Applied;
			}
			else
			{
				return CommandDisposition::Discard;
			}
		}, command);
	}

	template<typename Process>
	auto ExecuteSimulationCommands(
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

	ExpectedCommandProcessingResult ExecuteExpectedSimulationCommand(
		std::uint64_t tick, std::uint64_t sequence)
	{
		return ExecuteSimulationCommands(
			[tick, sequence](auto& queue, auto&& handler, auto&& observer) {
				return ProcessExpectedNextCommandThrough(
					queue, tick, sequence,
					std::forward<decltype(handler)>(handler),
					std::forward<decltype(observer)>(observer));
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
				if (!IsValidTacticalMoveOrigin(value.origin))
					return SimulationCommandDomainError::InvalidMoveOrigin;
				if (!IsValidTacticalPendingActionPolicy(value.pendingAction))
					return SimulationCommandDomainError::InvalidPendingActionPolicy;
				return SimulationCommandDomainError::None;
			}
			else if constexpr (std::is_same<Command, SetFacingCommand>::value)
			{
				return IsValidTacticalDirection(value.direction)
					? SimulationCommandDomainError::None
					: SimulationCommandDomainError::InvalidDirection;
			}
			else if constexpr (
				std::is_same<Command, SetStealthModeCommand>::value ||
				std::is_same<Command, StopMovementCommand>::value)
			{
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

void BeginSimulationCommandFrameBudget(
	std::uint64_t frameSequence, std::size_t maximumCommands) noexcept
{
	SimulationCommandFrameBudget& budget = FrameBudget();
	if (budget.initialized && budget.frameSequence == frameSequence) return;
	budget.frameSequence = frameSequence;
	budget.maximumCommands = maximumCommands;
	budget.consumedCommands = 0;
	budget.initialized = true;
}

std::size_t RemainingSimulationCommandFrameBudget(
	std::size_t requestedMaximum) noexcept
{
	const SimulationCommandFrameBudget& budget = FrameBudget();
	const std::size_t remaining = budget.consumedCommands < budget.maximumCommands
		? budget.maximumCommands - budget.consumedCommands : 0;
	return requestedMaximum < remaining ? requestedMaximum : remaining;
}

void ConsumeSimulationCommandFrameBudget(std::size_t commands) noexcept
{
	SimulationCommandFrameBudget& budget = FrameBudget();
	const std::size_t remaining = budget.consumedCommands < budget.maximumCommands
		? budget.maximumCommands - budget.consumedCommands : 0;
	budget.consumedCommands += commands < remaining ? commands : remaining;
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

SimulationCommandDispatchResult TryDispatchSimulationCommandNow(
	SimulationCommand command) noexcept
{
	GameContext& game = GetGameContext();
	SimulationCommandDispatchResult result;
	result.tick = game.runtime().simulationTicks().completedTickSequence();
	if (RemainingSimulationCommandFrameBudget(1) == 0)
	{
		result.status = SimulationCommandDispatchStatus::FrameBudgetExhausted;
		return result;
	}
	if (game.commands().hasReadyThrough(result.tick))
	{
		result.status =
			SimulationCommandDispatchStatus::AuthoritativeBackpressure;
		return result;
	}
	if (game.commands().sequenceExhausted())
	{
		result.status = SimulationCommandDispatchStatus::SequenceExhausted;
		return result;
	}

	try
	{
		result.sequence = game.submitCommand(result.tick, std::move(command));
		result.submitted = true;
	}
	catch (const std::overflow_error&)
	{
		result.status = SimulationCommandDispatchStatus::SequenceExhausted;
		return result;
	}
	catch (...)
	{
		result.status = SimulationCommandDispatchStatus::SubmissionFailure;
		return result;
	}

	ConsumeSimulationCommandFrameBudget(1);
	try
	{
		const ExpectedCommandProcessingResult processed =
			ExecuteExpectedSimulationCommand(result.tick, result.sequence);
		switch (processed.status)
		{
			case ExpectedCommandProcessStatus::Processed:
				result.status = processed.processing.applied == 1
					? SimulationCommandDispatchStatus::Applied
					: SimulationCommandDispatchStatus::Discarded;
				break;
			case ExpectedCommandProcessStatus::Retry:
				result.status = SimulationCommandDispatchStatus::RetryDeferred;
				break;
			case ExpectedCommandProcessStatus::NoCommandReady:
			case ExpectedCommandProcessStatus::DifferentCommandReady:
			case ExpectedCommandProcessStatus::QueueChanged:
				result.status = SimulationCommandDispatchStatus::QueueChanged;
				break;
		}
	}
	catch (...)
	{
		result.status = SimulationCommandDispatchStatus::RetryDeferred;
	}
	return result;
}

SimulationCommandDispatchResult TryDispatchEndTurnCommandNow(
	std::uint8_t nextTeam, SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{EndTurnCommand{nextTeam, source}});
}

SimulationCommandDispatchResult TryDispatchChangeStanceCommandNow(
	std::uint16_t soldierId, std::uint8_t stance,
	SimulationCommandSource source) noexcept
{
	const TacticalEntityId soldier = GetJa2TacticalEntityId(soldierId);
	return TryDispatchSimulationCommandNow(
		SimulationCommand{ChangeStanceCommand{soldier, stance, source}});
}

SimulationCommandDispatchResult TryDispatchBeginFireWeaponCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{BeginFireWeaponCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			targetGrid, targetLevel, targetCubeLevel, source}});
}

SimulationCommandDispatchResult TryDispatchMoveToGridCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{MoveToGridCommand{
			TacticalEntityId{soldierId, uniqueSoldierId}, destinationGrid,
			movementMode, reverse, forceRestart, source}});
}

SimulationCommandDispatchResult TryDispatchSetFacingCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint8_t direction,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{SetFacingCommand{
			TacticalEntityId{soldierId, uniqueSoldierId}, direction, source}});
}

SimulationCommandDispatchResult TryDispatchSetStealthModeCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	bool enabled,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{SetStealthModeCommand{
			TacticalEntityId{soldierId, uniqueSoldierId}, enabled, source}});
}

SimulationCommandDispatchResult TryDispatchStopMovementCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{StopMovementCommand{
			TacticalEntityId{soldierId, uniqueSoldierId}, source}});
}

std::uint64_t DispatchEndTurnCommandNow(
	std::uint8_t nextTeam, SimulationCommandSource source)
{
	return TryDispatchEndTurnCommandNow(nextTeam, source).sequence;
}

std::uint64_t DispatchChangeStanceCommandNow(
	std::uint16_t soldierId, std::uint8_t stance, SimulationCommandSource source)
{
	return TryDispatchChangeStanceCommandNow(soldierId, stance, source).sequence;
}

std::uint64_t DispatchBeginFireWeaponCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	SimulationCommandSource source)
{
	return TryDispatchBeginFireWeaponCommandNow(
		soldierId, uniqueSoldierId, targetGrid, targetLevel,
		targetCubeLevel, source).sequence;
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
	return TryDispatchMoveToGridCommandNow(
		soldierId, uniqueSoldierId, destinationGrid, movementMode,
		reverse, forceRestart, source).sequence;
}
