#include "Simulation Commands.h"

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include <Engine/Core/CommandProcessor.h>

#include "Animation Control.h"
#include "GameContext.h"
#include "Handle UI.h"
#include "Handle Items.h"
#include "Interactive Tiles.h"
#include "Items.h"
#include "Map Information.h"
#include "Overhead.h"
#include "Soldier Control.h"
#include "Soldier Functions.h"
#include "Soldier macros.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldItemHost.h"
#include "Vehicles.h"
#include "Weapons.h"
#include "opplist.h"
#include "structure.h"

namespace
{
	static_assert(
		TacticalMaximumVehicleSeats == MAXPASSENGERS,
		"public vehicle command seat bound must match legacy passenger storage");

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
		return IsJa2TacticalWorldLoaded() &&
			(gTacticalStatus.uiFlags & RequiredFlags) == RequiredFlags &&
			gTacticalStatus.ubCurrentTeam < MAXTEAMS;
	}

	SOLDIERTYPE* ResolveLiveCommandActor(TacticalEntityId actor) noexcept
	{
		if (!IsJa2TacticalWorldLoaded()) return nullptr;
		SOLDIERTYPE* soldier = ResolveJa2TacticalEntity(actor);
		if (!soldier || !soldier->bInSector) return nullptr;
		return soldier;
	}

	STRUCTURE* ResolveLiveWorldObject(TacticalWorldObjectId object) noexcept
	{
		if (!IsJa2TacticalWorldLoaded() ||
			object.grid < 0 || object.grid >= WORLD_MAX) return nullptr;
		return FindStructureByID(object.grid, object.structureId);
	}

	bool CanBeginWorldObjectInteraction(const SOLDIERTYPE& soldier) noexcept
	{
		return soldier.usAnimState != OPEN_STRUCT &&
			soldier.usAnimState != OPEN_STRUCT_CROUCHED &&
			soldier.usAnimState != BEGIN_OPENSTRUCT &&
			soldier.usAnimState != BEGIN_OPENSTRUCT_CROUCHED;
	}

	bool IsValidConversationPair(
		const SOLDIERTYPE& soldier, const SOLDIERTYPE& target) noexcept
	{
		return soldier.ubID != target.ubID && target.bActive && target.bInSector;
	}

	bool HasValidVehicleSeat(
		const SOLDIERTYPE& vehicle, std::uint8_t seatIndex) noexcept
	{
		const INT32 capacity =
			GetVehicleSeatingCapacity(vehicle.bVehicleID);
		return capacity > 0 && seatIndex < capacity;
	}

	bool CanEnterCommandVehicle(
		SOLDIERTYPE& soldier, SOLDIERTYPE& vehicle,
		std::uint8_t seatIndex) noexcept
	{
		if (soldier.ubID == vehicle.ubID ||
			(soldier.flags.uiStatusFlags &
				(SOLDIER_DRIVER | SOLDIER_PASSENGER | SOLDIER_VEHICLE)) != 0 ||
			!OK_ENTERABLE_VEHICLE((&vehicle)) ||
			vehicle.bVisible == -1 ||
			!OKUseVehicle(vehicle.ubProfile) ||
			!IsThisVehicleAccessibleToSoldier(
				&soldier, vehicle.bVehicleID) ||
			!HasValidVehicleSeat(vehicle, seatIndex))
			return false;
		return IsEnoughSpaceInVehicle(vehicle.bVehicleID) == TRUE;
	}

	void ClearPendingWorldItemPickup(SOLDIERTYPE& soldier) noexcept
	{
		soldier.aiData.ubPendingAction = NO_PENDING_ACTION;
		soldier.aiData.uiPendingActionData1 = 0;
		soldier.aiData.sPendingActionData2 = 0;
		soldier.aiData.bPendingActionData3 = 0;
		soldier.aiData.uiPendingActionData4 = 0;
		soldier.uiPendingActionTargetIncarnation = 0;
		UnSetUIBusy(soldier.ubID);
	}

	bool PendingWorldItemMatches(
		const SOLDIERTYPE& soldier,
		std::int32_t itemIndex,
		std::int32_t grid,
		std::int8_t level) noexcept
	{
		if (soldier.uiPendingActionTargetIncarnation == 0)
			return true;
		const UINT32 rawSlot = soldier.aiData.uiPendingActionData1;
		const TacticalWorldItemId itemId{
			rawSlot, soldier.uiPendingActionTargetIncarnation};
		WORLDITEM* item = ResolveJa2TacticalWorldItem(itemId);
		return item &&
			itemIndex >= 0 &&
			static_cast<UINT32>(itemIndex) == rawSlot &&
			grid == soldier.aiData.uiPendingActionData4 &&
			level == soldier.aiData.bPendingActionData3 &&
			item->sGridNo == grid &&
			item->ubLevel == soldier.pathing.bLevel &&
			(level == ITEM_IGNORE_Z_LEVEL ||
				item->bRenderZHeightAboveLevel == level);
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
			else if constexpr (std::is_same<Command, CycleWeaponModeCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier || !soldier->inv[HANDPOS].exists() ||
					(gAnimControl[soldier->usAnimState].uiFlags & ANIM_FIRE) != 0)
					return CommandDisposition::Discard;
				ChangeWeaponMode(soldier);
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, CycleScopeModeCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier || !soldier->inv[HANDPOS].exists() ||
					(gAnimControl[soldier->usAnimState].uiFlags & ANIM_FIRE) != 0)
					return CommandDisposition::Discard;
				ChangeScopeMode(soldier, value.targetGrid);
				ManLooksForOtherTeams(soldier);
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, ReloadWeaponCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier || !soldier->inv[HANDPOS].exists())
					return CommandDisposition::Discard;
				return AutoReload(soldier, value.reloadEvenIfNotEmpty)
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (std::is_same<Command, TraverseObstacleCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier) return CommandDisposition::Discard;
				switch (value.kind)
				{
					case TacticalTraversalKind::ClimbUpRoof:
						soldier->BeginSoldierClimbUpRoof();
						break;
					case TacticalTraversalKind::ClimbDownRoof:
						soldier->BeginSoldierClimbDownRoof();
						break;
					case TacticalTraversalKind::JumpFence:
						soldier->BeginSoldierClimbFence();
						break;
					case TacticalTraversalKind::ClimbWall:
						soldier->BeginSoldierClimbWall();
						break;
					case TacticalTraversalKind::JumpWindow:
						soldier->BeginSoldierClimbWindow();
						break;
				}
				return CommandDisposition::Applied;
			}
			else if constexpr (
				std::is_same<Command, ActivateWorldObjectCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier || !CanBeginWorldObjectInteraction(*soldier))
					return CommandDisposition::Discard;
				STRUCTURE* structure = ResolveLiveWorldObject(value.object);
				if (!structure) return CommandDisposition::Discard;
				if (!StartInteractiveObject(
						value.object.grid, value.object.structureId,
						soldier, value.direction))
					return CommandDisposition::Discard;
				return InteractWithInteractiveObject(
					soldier, structure, value.direction)
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, ApproachWorldObjectCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier || !CanBeginWorldObjectInteraction(*soldier) ||
					!IsValidMovementMode(soldier, value.movementMode))
					return CommandDisposition::Discard;
				STRUCTURE* structure = ResolveLiveWorldObject(value.object);
				if (!structure) return CommandDisposition::Discard;

				soldier->usUIMovementMode = value.movementMode;
				soldier->bReverse = value.reverse ? TRUE : FALSE;
				soldier->aiData.ubPendingAction = NO_PENDING_ACTION;
				if (!soldier->EVENT_InternalGetNewSoldierPath(
						value.destinationGrid, value.movementMode,
						static_cast<BOOLEAN>(TacticalMoveOrigin::PlayerUi),
						value.forceRestart ? TRUE : FALSE))
					return CommandDisposition::Discard;
				return StartInteractiveObject(
					value.object.grid, value.object.structureId,
					soldier, value.direction)
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, StartConversationCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				SOLDIERTYPE* target = ResolveLiveCommandActor(value.target);
				if (!soldier || !target ||
					!IsValidConversationPair(*soldier, *target))
					return CommandDisposition::Discard;
				(void)soldier->PlayerSoldierStartTalking(target->ubID, FALSE);
				return CommandDisposition::Applied;
			}
			else if constexpr (
				std::is_same<Command, ApproachConversationCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				SOLDIERTYPE* target = ResolveLiveCommandActor(value.target);
				if (!soldier || !target ||
					!IsValidConversationPair(*soldier, *target) ||
					!IsValidMovementMode(soldier, value.movementMode))
					return CommandDisposition::Discard;

				const UINT8 previousAction = soldier->aiData.ubPendingAction;
				const UINT32 previousData1 =
					soldier->aiData.uiPendingActionData1;
				const INT32 previousData2 =
					soldier->aiData.sPendingActionData2;
				const INT8 previousData3 =
					soldier->aiData.bPendingActionData3;
				const UINT32 previousData4 =
					soldier->aiData.uiPendingActionData4;
				const UINT32 previousTargetIncarnation =
					soldier->uiPendingActionTargetIncarnation;
				const UINT8 previousAnimCount =
					soldier->aiData.ubPendingActionAnimCount;
				const UINT16 previousMovementMode =
					soldier->usUIMovementMode;

				soldier->usUIMovementMode = value.movementMode;
				soldier->aiData.ubPendingAction = MERC_TALK;
				soldier->aiData.uiPendingActionData1 = value.target.slot;
				soldier->aiData.sPendingActionData2 = 0;
				soldier->aiData.bPendingActionData3 = 0;
				soldier->aiData.uiPendingActionData4 = 0;
				soldier->uiPendingActionTargetIncarnation =
					value.target.incarnation;
				soldier->aiData.ubPendingActionAnimCount = 0;
				if (soldier->EVENT_InternalGetNewSoldierPath(
						value.destinationGrid, value.movementMode,
						static_cast<BOOLEAN>(TacticalMoveOrigin::PlayerUi),
						value.forceRestart ? TRUE : FALSE))
					return CommandDisposition::Applied;

				soldier->aiData.ubPendingAction = previousAction;
				soldier->aiData.uiPendingActionData1 = previousData1;
				soldier->aiData.sPendingActionData2 = previousData2;
				soldier->aiData.bPendingActionData3 = previousData3;
				soldier->aiData.uiPendingActionData4 = previousData4;
				soldier->uiPendingActionTargetIncarnation =
					previousTargetIncarnation;
				soldier->aiData.ubPendingActionAnimCount =
					previousAnimCount;
				soldier->usUIMovementMode = previousMovementMode;
				return CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, EnterVehicleCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				SOLDIERTYPE* vehicle = ResolveLiveCommandActor(value.vehicle);
				if (!soldier || !vehicle ||
					!CanEnterCommandVehicle(
						*soldier, *vehicle, value.seatIndex))
					return CommandDisposition::Discard;
				const BOOLEAN entered =
					EnterVehicle(vehicle, soldier, value.seatIndex);
				UnSetUIBusy(soldier->ubID);
				return entered
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, ApproachVehicleCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				SOLDIERTYPE* vehicle = ResolveLiveCommandActor(value.vehicle);
				if (!soldier || !vehicle ||
					!CanEnterCommandVehicle(
						*soldier, *vehicle, value.seatIndex) ||
					!IsValidMovementMode(soldier, value.movementMode))
					return CommandDisposition::Discard;

				const UINT8 previousAction = soldier->aiData.ubPendingAction;
				const UINT32 previousData1 =
					soldier->aiData.uiPendingActionData1;
				const INT32 previousData2 =
					soldier->aiData.sPendingActionData2;
				const INT8 previousData3 =
					soldier->aiData.bPendingActionData3;
				const UINT32 previousData4 =
					soldier->aiData.uiPendingActionData4;
				const UINT32 previousTargetIncarnation =
					soldier->uiPendingActionTargetIncarnation;
				const UINT8 previousAnimCount =
					soldier->aiData.ubPendingActionAnimCount;
				const UINT16 previousMovementMode =
					soldier->usUIMovementMode;

				soldier->usUIMovementMode = value.movementMode;
				soldier->aiData.ubPendingAction = MERC_ENTER_VEHICLE;
				soldier->aiData.uiPendingActionData1 = 0;
				soldier->uiPendingActionTargetIncarnation =
					value.vehicle.incarnation;
				// The old field held a grid. All production scheduling now
				// stores the vehicle slot so completion can resolve the exact
				// incarnation even if the target moves.
				soldier->aiData.sPendingActionData2 = value.vehicle.slot;
				soldier->aiData.bPendingActionData3 = value.direction;
				soldier->aiData.uiPendingActionData4 = value.seatIndex;
				soldier->aiData.ubPendingActionAnimCount = 0;
				if (soldier->EVENT_InternalGetNewSoldierPath(
						value.destinationGrid, value.movementMode,
						static_cast<BOOLEAN>(TacticalMoveOrigin::TeamAwareUi),
						value.forceRestart ? TRUE : FALSE))
					return CommandDisposition::Applied;

				soldier->aiData.ubPendingAction = previousAction;
				soldier->aiData.uiPendingActionData1 = previousData1;
				soldier->aiData.sPendingActionData2 = previousData2;
				soldier->aiData.bPendingActionData3 = previousData3;
				soldier->aiData.uiPendingActionData4 = previousData4;
				soldier->uiPendingActionTargetIncarnation =
					previousTargetIncarnation;
				soldier->aiData.ubPendingActionAnimCount =
					previousAnimCount;
				soldier->usUIMovementMode = previousMovementMode;
				return CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, PickupWorldItemCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier) return CommandDisposition::Discard;

				INT32 itemIndex = NOTHING;
				std::uint32_t targetIncarnation = 0;
				if (value.kind ==
					TacticalWorldItemPickupKind::SpecificItem)
				{
					WORLDITEM* item =
						ResolveJa2TacticalWorldItem(value.item);
					if (!item || item->sGridNo != value.grid ||
						item->ubLevel != soldier->pathing.bLevel ||
						item->bRenderZHeightAboveLevel !=
							value.renderHeight)
						return CommandDisposition::Discard;
					itemIndex = static_cast<INT32>(value.item.slot);
					targetIncarnation = value.item.incarnation;
				}

				SoldierPickupItem(
					soldier, itemIndex, value.grid, value.renderHeight,
					targetIncarnation);
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
				std::is_same<Command, StopMovementCommand>::value ||
				std::is_same<Command, CycleWeaponModeCommand>::value ||
				std::is_same<Command, ReloadWeaponCommand>::value)
			{
				return SimulationCommandDomainError::None;
			}
			else if constexpr (std::is_same<Command, CycleScopeModeCommand>::value)
			{
				return value.targetGrid == TacticalNoTargetGrid ||
					(value.targetGrid >= 0 && value.targetGrid < WORLD_MAX)
					? SimulationCommandDomainError::None
					: SimulationCommandDomainError::InvalidTargetGrid;
			}
			else if constexpr (std::is_same<Command, TraverseObstacleCommand>::value)
			{
				return IsValidTacticalTraversalKind(value.kind)
					? SimulationCommandDomainError::None
					: SimulationCommandDomainError::InvalidTraversalKind;
			}
			else if constexpr (
				std::is_same<Command, ActivateWorldObjectCommand>::value ||
				std::is_same<Command, ApproachWorldObjectCommand>::value)
			{
				if (value.object.grid < 0 || value.object.grid >= WORLD_MAX)
					return SimulationCommandDomainError::InvalidObjectGrid;
				if (!IsValidTacticalDirection(value.direction))
					return SimulationCommandDomainError::InvalidDirection;
				if constexpr (
					std::is_same<Command, ApproachWorldObjectCommand>::value)
				{
					if (value.destinationGrid < 0 ||
						value.destinationGrid >= WORLD_MAX)
						return SimulationCommandDomainError::InvalidDestinationGrid;
					if (value.movementMode >= NUMANIMATIONSTATES ||
						(gAnimControl[value.movementMode].uiFlags &
							ANIM_MOVING) == 0)
						return SimulationCommandDomainError::InvalidMovementMode;
				}
				return SimulationCommandDomainError::None;
			}
			else if constexpr (
				std::is_same<Command, StartConversationCommand>::value ||
				std::is_same<Command, ApproachConversationCommand>::value)
			{
				if (!value.target.valid() ||
					value.target.slot >= TOTAL_SOLDIERS ||
					value.target == value.soldier)
					return SimulationCommandDomainError::InvalidTargetActor;
				if constexpr (
					std::is_same<Command, ApproachConversationCommand>::value)
				{
					if (value.destinationGrid < 0 ||
						value.destinationGrid >= WORLD_MAX)
						return SimulationCommandDomainError::InvalidDestinationGrid;
					if (value.movementMode >= NUMANIMATIONSTATES ||
						(gAnimControl[value.movementMode].uiFlags &
							ANIM_MOVING) == 0)
						return SimulationCommandDomainError::InvalidMovementMode;
				}
				return SimulationCommandDomainError::None;
			}
			else if constexpr (
				std::is_same<Command, EnterVehicleCommand>::value ||
				std::is_same<Command, ApproachVehicleCommand>::value)
			{
				if (!value.vehicle.valid() ||
					value.vehicle.slot >= TOTAL_SOLDIERS ||
					value.vehicle == value.soldier)
					return SimulationCommandDomainError::InvalidTargetActor;
				if (!IsValidTacticalDirection(value.direction))
					return SimulationCommandDomainError::InvalidDirection;
				if (value.seatIndex >= TacticalMaximumVehicleSeats)
					return SimulationCommandDomainError::InvalidVehicleSeat;
				if constexpr (
					std::is_same<Command, ApproachVehicleCommand>::value)
				{
					if (value.destinationGrid < 0 ||
						value.destinationGrid >= WORLD_MAX)
						return SimulationCommandDomainError::InvalidDestinationGrid;
					if (value.movementMode >= NUMANIMATIONSTATES ||
						(gAnimControl[value.movementMode].uiFlags &
							ANIM_MOVING) == 0)
						return SimulationCommandDomainError::InvalidMovementMode;
				}
				return SimulationCommandDomainError::None;
			}
			else if constexpr (
				std::is_same<Command, PickupWorldItemCommand>::value)
			{
				if (!IsValidTacticalWorldItemPickupKind(value.kind))
					return SimulationCommandDomainError::InvalidWorldItemPickupKind;
				if (value.grid < 0 || value.grid >= WORLD_MAX)
					return SimulationCommandDomainError::InvalidObjectGrid;
				if (value.renderHeight < 0)
					return SimulationCommandDomainError::
						InvalidWorldItemRenderHeight;
				if (value.kind ==
						TacticalWorldItemPickupKind::SpecificItem
					? !value.item.valid() ||
						value.item.slot > TacticalMaximumWorldItemSlot
					: value.item != TacticalWorldItemId{})
					return SimulationCommandDomainError::InvalidWorldItem;
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

SimulationCommandDispatchResult TryDispatchCycleWeaponModeCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{CycleWeaponModeCommand{
			TacticalEntityId{soldierId, uniqueSoldierId}, source}});
}

SimulationCommandDispatchResult TryDispatchCycleScopeModeCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t targetGrid,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{CycleScopeModeCommand{
			TacticalEntityId{soldierId, uniqueSoldierId}, targetGrid, source}});
}

SimulationCommandDispatchResult TryDispatchReloadWeaponCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	bool reloadEvenIfNotEmpty,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{ReloadWeaponCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			reloadEvenIfNotEmpty, source}});
}

SimulationCommandDispatchResult TryDispatchTraverseObstacleCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	TacticalTraversalKind kind,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{TraverseObstacleCommand{
			TacticalEntityId{soldierId, uniqueSoldierId}, kind, source}});
}

SimulationCommandDispatchResult TryDispatchActivateWorldObjectCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{ActivateWorldObjectCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			TacticalWorldObjectId{objectGrid, structureId},
			direction, source}});
}

SimulationCommandDispatchResult TryDispatchApproachWorldObjectCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{ApproachWorldObjectCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			TacticalWorldObjectId{objectGrid, structureId},
			direction, destinationGrid, movementMode,
			reverse, forceRestart, source}});
}

SimulationCommandDispatchResult TryDispatchStartConversationCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint16_t targetId,
	std::uint32_t targetUniqueSoldierId,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{StartConversationCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			TacticalEntityId{targetId, targetUniqueSoldierId},
			source}});
}

SimulationCommandDispatchResult TryDispatchApproachConversationCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint16_t targetId,
	std::uint32_t targetUniqueSoldierId,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool forceRestart,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{ApproachConversationCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			TacticalEntityId{targetId, targetUniqueSoldierId},
			destinationGrid, movementMode, forceRestart, source}});
}

SimulationCommandDispatchResult TryDispatchEnterVehicleCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint16_t vehicleId,
	std::uint32_t vehicleUniqueSoldierId,
	std::uint8_t direction,
	std::uint8_t seatIndex,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{EnterVehicleCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			TacticalEntityId{vehicleId, vehicleUniqueSoldierId},
			direction, seatIndex, source}});
}

SimulationCommandDispatchResult TryDispatchApproachVehicleCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint16_t vehicleId,
	std::uint32_t vehicleUniqueSoldierId,
	std::uint8_t direction,
	std::uint8_t seatIndex,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool forceRestart,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{ApproachVehicleCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			TacticalEntityId{vehicleId, vehicleUniqueSoldierId},
			direction, seatIndex, destinationGrid, movementMode,
			forceRestart, source}});
}

SimulationCommandDispatchResult TryDispatchPickupWorldItemCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	TacticalWorldItemId item,
	std::int32_t grid,
	std::int8_t renderHeight,
	TacticalWorldItemPickupKind kind,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{PickupWorldItemCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			item, grid, renderHeight, kind, source}});
}

bool TryCompletePendingConversationCommand(SOLDIERTYPE& soldier) noexcept
{
	if (soldier.aiData.ubPendingAction != MERC_TALK) return false;
	const UINT32 targetSlot = soldier.aiData.uiPendingActionData1;
	const TacticalEntityId targetId{
		targetSlot < TOTAL_SOLDIERS
			? static_cast<std::uint16_t>(targetSlot)
			: static_cast<std::uint16_t>(TOTAL_SOLDIERS),
		soldier.uiPendingActionTargetIncarnation};

	soldier.aiData.ubPendingAction = NO_PENDING_ACTION;
	soldier.aiData.uiPendingActionData1 = 0;
	soldier.aiData.uiPendingActionData4 = 0;
	soldier.uiPendingActionTargetIncarnation = 0;

	SOLDIERTYPE* target = ResolveLiveCommandActor(targetId);
	if (!target || !IsValidConversationPair(soldier, *target)) return false;
	(void)soldier.PlayerSoldierStartTalking(target->ubID, TRUE);
	return true;
}

bool TryCompletePendingVehicleCommand(SOLDIERTYPE& soldier) noexcept
{
	if (soldier.aiData.ubPendingAction != MERC_ENTER_VEHICLE) return false;
	const INT32 targetSlot = soldier.aiData.sPendingActionData2;
	const INT32 rawDirection = soldier.aiData.bPendingActionData3;
	const UINT32 rawSeatIndex = soldier.aiData.uiPendingActionData4;
	const TacticalEntityId vehicleId{
		targetSlot >= 0 && targetSlot < TOTAL_SOLDIERS
			? static_cast<std::uint16_t>(targetSlot)
			: static_cast<std::uint16_t>(TOTAL_SOLDIERS),
		soldier.uiPendingActionTargetIncarnation};

	soldier.aiData.ubPendingAction = NO_PENDING_ACTION;
	soldier.aiData.uiPendingActionData1 = 0;
	soldier.aiData.sPendingActionData2 = 0;
	soldier.aiData.bPendingActionData3 = 0;
	soldier.aiData.uiPendingActionData4 = 0;
	soldier.uiPendingActionTargetIncarnation = 0;

	SOLDIERTYPE* vehicle = ResolveLiveCommandActor(vehicleId);
	if (!vehicle || rawDirection < 0 ||
		!IsValidTacticalDirection(static_cast<std::uint8_t>(rawDirection)) ||
		rawSeatIndex >= TacticalMaximumVehicleSeats ||
		!CanEnterCommandVehicle(
			soldier, *vehicle, static_cast<std::uint8_t>(rawSeatIndex)))
	{
		UnSetUIBusy(soldier.ubID);
		return false;
	}

	const BOOLEAN entered = EnterVehicle(
		vehicle, &soldier, static_cast<std::uint8_t>(rawSeatIndex));
	UnSetUIBusy(soldier.ubID);
	return entered == TRUE;
}

bool TryValidatePendingWorldItemPickup(SOLDIERTYPE& soldier) noexcept
{
	if (soldier.aiData.ubPendingAction != MERC_PICKUPITEM)
		return true;
	if (PendingWorldItemMatches(
			soldier,
			static_cast<INT32>(soldier.aiData.uiPendingActionData1),
			static_cast<INT32>(soldier.aiData.uiPendingActionData4),
			soldier.aiData.bPendingActionData3))
		return true;
	ClearPendingWorldItemPickup(soldier);
	return false;
}

bool TryConsumePendingWorldItemPickup(
	SOLDIERTYPE& soldier,
	std::int32_t itemIndex,
	std::int32_t grid,
	std::int8_t level) noexcept
{
	if (soldier.aiData.ubPendingAction != MERC_PICKUPITEM)
	{
		soldier.uiPendingActionTargetIncarnation = 0;
		return true;
	}
	if (!PendingWorldItemMatches(soldier, itemIndex, grid, level))
	{
		ClearPendingWorldItemPickup(soldier);
		return false;
	}
	soldier.uiPendingActionTargetIncarnation = 0;
	return true;
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
