#include "Simulation Commands.h"
#include "TacticalWorldAdapter.h"

#include <array>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include <Engine/Adapters/JA2/SimulationCommandExecutor.h>

#include "Animation Control.h"
#include "GameContext.h"
#include "Handle UI.h"
#include "Handle Items.h"
#include "Interactive Tiles.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Map Information.h"
#include "Overhead.h"
#include "Points.h"
#include "Soldier Control.h"
#include "Soldier Functions.h"
#include "Soldier macros.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldItemHost.h"
#include "TeamTurns.h"
#include "Vehicles.h"
#include "Weapons.h"
#include "connect.h"
#include "opplist.h"
#include "soldier tile.h"
#include "structure.h"
#include "worldman.h"

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
		return IsJa2TacticalWorldLoaded() &&
			IsJa2TacticalTurnBasedCombat() &&
			GetJa2TacticalCurrentTeam() < MAXTEAMS;
	}

	SOLDIERTYPE* ResolveLiveCommandActor(TacticalEntityId actor) noexcept
	{
		if (!IsJa2TacticalWorldLoaded()) return nullptr;
		SOLDIERTYPE* soldier = ResolveJa2TacticalEntity(actor);
		if (!soldier || !soldier->bInSector) return nullptr;
		return soldier;
	}

	bool CaptureCommandActor(
		const SOLDIERTYPE& soldier, TacticalEntityId& actor) noexcept
	{
		actor = GetJa2TacticalEntityId(
			static_cast<std::uint16_t>(soldier.ubID));
		return actor.valid() && ResolveJa2TacticalEntity(actor) == &soldier;
	}

	SimulationCommandDispatchResult InvalidCommandActorResult() noexcept
	{
		SimulationCommandDispatchResult result;
		result.status = SimulationCommandDispatchStatus::InvalidActor;
		result.tick =
			GetGameContext().runtime().simulationTicks().completedTickSequence();
		return result;
	}

	template <typename Builder>
	SimulationCommandDispatchResult DispatchActorCommand(
		SOLDIERTYPE& soldier, Builder&& builder) noexcept
	{
		TacticalEntityId actor;
		if (!CaptureCommandActor(soldier, actor))
			return InvalidCommandActorResult();
		return TryDispatchSimulationCommandNow(
			SimulationCommand{builder(actor)});
	}

	template <typename Builder>
	SimulationCommandDispatchResult DispatchNetworkActorCommand(
		SOLDIERTYPE& soldier, Builder&& builder) noexcept
	{
		TacticalEntityId actor;
		if (!CaptureCommandActor(soldier, actor))
			return InvalidCommandActorResult();
		return TryDispatchNetworkSimulationCommand(
			SimulationCommand{builder(actor)});
	}

	template <typename Builder>
	SimulationCommandDispatchResult DispatchSystemActorCommand(
		SOLDIERTYPE& soldier, Builder&& builder) noexcept
	{
		TacticalEntityId actor;
		if (!CaptureCommandActor(soldier, actor))
			return InvalidCommandActorResult();
		return TryDispatchSystemSimulationCommand(
			SimulationCommand{builder(actor)});
	}

	template <typename Builder>
	SimulationCommandDispatchResult DispatchActorPairCommand(
		SOLDIERTYPE& soldier,
		SOLDIERTYPE& target,
		Builder&& builder) noexcept
	{
		TacticalEntityId actor;
		TacticalEntityId targetActor;
		if (!CaptureCommandActor(soldier, actor) ||
			!CaptureCommandActor(target, targetActor))
			return InvalidCommandActorResult();
		return TryDispatchSimulationCommandNow(
			SimulationCommand{builder(actor, targetActor)});
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

	bool IsAtIssuedTacticalPosition(
		const SOLDIERTYPE& soldier,
		std::int32_t grid,
		std::int8_t level) noexcept
	{
		return soldier.sGridNo == grid && soldier.pathing.bLevel == level;
	}

	bool CanExecutePositionExchange(
		SOLDIERTYPE& soldier,
		SOLDIERTYPE& target,
		const ExchangePositionsCommand& command) noexcept
	{
		if (!IsAtIssuedTacticalPosition(
				soldier, command.soldierGrid, command.level) ||
			!IsAtIssuedTacticalPosition(
				target, command.targetGrid, command.level) ||
			soldier.vitals().health() < OKLIFE ||
			target.vitals().health() < OKLIFE ||
			PythSpacesAway(soldier.sGridNo, target.sGridNo) != 1 ||
			(!target.aiData.bNeutral && target.bSide != gbPlayerNum))
			return false;
		return CanExchangePlaces(&soldier, &target, FALSE) == TRUE;
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
		soldier.runtime.pendingAction.targetIncarnation = 0;
		UnSetUIBusy(soldier.ubID);
	}

	void ClearPendingSteal(SOLDIERTYPE& soldier) noexcept
	{
		soldier.aiData.ubPendingAction = NO_PENDING_ACTION;
		soldier.aiData.uiPendingActionData1 = 0;
		soldier.aiData.sPendingActionData2 = 0;
		soldier.aiData.bPendingActionData3 = 0;
		soldier.aiData.uiPendingActionData4 = 0;
		soldier.runtime.pendingAction.targetIncarnation = 0;
		UnSetUIBusy(soldier.ubID);
	}

	SOLDIERTYPE* ResolveStablePendingStealTarget(
		const SOLDIERTYPE& soldier) noexcept
	{
		const UINT32 rawSlot = soldier.aiData.uiPendingActionData1;
		if (rawSlot >= TOTAL_SOLDIERS ||
			soldier.runtime.pendingAction.targetIncarnation == 0)
			return nullptr;
		return ResolveLiveCommandActor(TacticalEntityId{
			static_cast<std::uint16_t>(rawSlot),
			soldier.runtime.pendingAction.targetIncarnation});
	}

	bool PendingWorldItemMatches(
		const SOLDIERTYPE& soldier,
		std::int32_t itemIndex,
		std::int32_t grid,
		std::int8_t level) noexcept
	{
		if (soldier.runtime.pendingAction.targetIncarnation == 0)
			return true;
		const UINT32 rawSlot = soldier.aiData.uiPendingActionData1;
		const TacticalWorldItemId itemId{
			rawSlot, soldier.runtime.pendingAction.targetIncarnation};
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
			else if constexpr (
				std::is_same<Command, SynchronizeTurnCommand>::value)
			{
				if (!IsJa2TacticalWorldLoaded() ||
					value.nextTeam >= MAXTEAMS ||
					!IsSimulationSynchronizationSource(value.source))
					return CommandDisposition::Discard;
				if (value.enterCombat)
					EnterCombatMode(0);
				if (value.endClientTurn)
				{
					EndTurnEvents();
					EndTurn(value.nextTeam);
				}
				BeginTeamTurn(value.nextTeam);
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, ChangeStanceCommand>::value)
			{
				if (SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier))
				{
					if (value.eventPolicy == TacticalEventPolicy::LocalOnly)
					{
						// Some received and scripted actions deliberately bypass
						// outbound replication while retaining the same local
						// stance transition.
						soldier->ChangeSoldierStance(value.stance);
						return CommandDisposition::Applied;
					}
					const bool realtimeStanceChange =
						(gTacticalStatus.uiFlags & REALTIME) != 0 ||
						(IsJa2TacticalCombatActive()) == 0;
					if (realtimeStanceChange &&
						(gAnimControl[soldier->usAnimState].uiFlags &
							ANIM_STATIONARY) == 0)
					{
						soldier->usUIMovementMode =
							soldier->GetMoveStateBasedOnStance(value.stance);
						soldier->ubDesiredHeight = NO_DESIRED_HEIGHT;
						soldier->usDontUpdateNewGridNoOnMoveAnimChange = 1;
						soldier->ChangeSoldierState(
							soldier->usUIMovementMode, 0, FALSE);
					}
					else
					{
						SendChangeSoldierStanceEvent(soldier, value.stance);
					}
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
			else if constexpr (
				std::is_same<Command, BeginSelectedFireWeaponCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier ||
					!IsSimulationSystemSource(value.source) ||
					value.attackingHand >= NUM_INV_SLOTS ||
					value.attackingWeapon >= MAXITEMS)
					return CommandDisposition::Discard;
				soldier->ubAttackingHand = value.attackingHand;
				soldier->usAttackingWeapon =
					static_cast<UINT16>(value.attackingWeapon);
				soldier->sTargetGridNo = value.targetGrid;
				soldier->bTargetLevel = value.targetLevel;
				soldier->bTargetCubeLevel = value.targetCubeLevel;
				SendBeginFireWeaponEvent(soldier, value.targetGrid);
				if (value.source == SimulationCommandSource::System &&
					(is_server ||
						(is_client && soldier->ubID < 20)))
					send_fire(soldier, value.targetGrid);
				return CommandDisposition::Applied;
			}
			else if constexpr (
				std::is_same<Command, SynchronizeActorFireCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier ||
					!IsSimulationSynchronizationSource(value.source) ||
					value.attackingWeapon >= MAXITEMS)
					return CommandDisposition::Discard;
				soldier->sTargetGridNo = value.targetGrid;
				soldier->bTargetLevel = value.targetLevel;
				soldier->bTargetCubeLevel = value.targetCubeLevel;
				soldier->usAttackingWeapon =
					static_cast<UINT16>(value.attackingWeapon);
				SendBeginFireWeaponEvent(soldier, value.targetGrid);
				return CommandDisposition::Applied;
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
					if (value.eventPolicy == TacticalEventPolicy::LocalOnly)
						soldier->EVENT_SetSoldierDesiredDirection(value.direction);
					else
						SendSoldierSetDesiredDirectionEvent(
							soldier, value.direction);
					return CommandDisposition::Applied;
				}
				return CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, SynchronizeActorPathCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier ||
					!IsSimulationSynchronizationSource(value.source))
					return CommandDisposition::Discard;

				for (std::size_t index = 0;
					index < TacticalReplicatedPathCapacity; ++index)
					soldier->pathing.usPathingData[index] = value.path[index];
				soldier->pathing.sDestination = value.destinationGrid;
				soldier->pathing.sFinalDestination = value.destinationGrid;
				soldier->pathing.usPathIndex = value.currentPathIndex;
				soldier->pathing.usPathDataSize = value.pathSize;

				SendGetNewSoldierPathEvent(
					soldier, value.destinationGrid, value.movementState);

				INT16 positionX = 0;
				INT16 positionY = 0;
				ConvertGridNoToCenterCellXY(
					value.reportedGrid, &positionX, &positionY);
				if ((gAnimControl[soldier->usAnimState].uiFlags &
						(ANIM_MOVING | ANIM_SPECIALMOVE)) == 0 ||
					soldier->flags.fNoAPToFinishMove)
					soldier->EVENT_InternalSetSoldierPosition(
						positionX, positionY, FALSE, FALSE, FALSE);
				soldier->EVENT_InitNewSoldierAnim(
					value.movementState, 0, FALSE);
				return CommandDisposition::Applied;
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
			else if constexpr (
				std::is_same<Command, SynchronizeActorStopCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier ||
					!IsSimulationSynchronizationSource(value.source))
					return CommandDisposition::Discard;
				soldier->EVENT_InternalSetSoldierPosition(
					value.positionX, value.positionY, FALSE, FALSE, FALSE);
				soldier->EVENT_SetSoldierDirection(value.direction);
				if (value.stop && soldier->bTeam >= LAN_TEAM_ONE &&
					soldier->sGridNo >= 0 &&
					soldier->sGridNo < WORLD_MAX &&
					(gAnimControl[soldier->usAnimState].uiFlags &
						ANIM_MOVING) != 0)
					soldier->EVENT_StopMerc(
						soldier->sGridNo, soldier->ubDirection);
				soldier->AdjustNoAPToFinishMove(
					value.stop ? TRUE : FALSE);
				soldier->flags.bTurningFromPronePosition = FALSE;
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, CancelDragCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier || !soldier->IsDragging())
					return CommandDisposition::Discard;
				soldier->CancelDrag();
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
			else if constexpr (
				std::is_same<Command, SetWeaponReadyCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier) return CommandDisposition::Discard;
				return soldier->InternalSoldierReadyWeapon(
					value.direction,
					value.ready ? FALSE : TRUE,
					value.alternativeHold ? TRUE : FALSE)
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
					soldier->runtime.pendingAction.targetIncarnation;
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
				soldier->runtime.pendingAction.targetIncarnation =
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
				soldier->runtime.pendingAction.targetIncarnation =
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
					soldier->runtime.pendingAction.targetIncarnation;
				const UINT8 previousAnimCount =
					soldier->aiData.ubPendingActionAnimCount;
				const UINT16 previousMovementMode =
					soldier->usUIMovementMode;

				soldier->usUIMovementMode = value.movementMode;
				soldier->aiData.ubPendingAction = MERC_ENTER_VEHICLE;
				soldier->aiData.uiPendingActionData1 = 0;
				soldier->runtime.pendingAction.targetIncarnation =
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
				soldier->runtime.pendingAction.targetIncarnation =
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
			else if constexpr (
				std::is_same<Command, StealFromActorCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				SOLDIERTYPE* target = ResolveLiveCommandActor(value.target);
				if (!soldier || !target ||
					!IsAtIssuedTacticalPosition(
						*target, value.targetGrid, value.targetLevel))
					return CommandDisposition::Discard;
				return MercStealFromMerc(soldier, target)
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, ExchangePositionsCommand>::value)
			{
				SOLDIERTYPE* soldier = ResolveLiveCommandActor(value.soldier);
				SOLDIERTYPE* target = ResolveLiveCommandActor(value.target);
				if (!soldier || !target ||
					!CanExecutePositionExchange(*soldier, *target, value) ||
					!SwapMercPositions(soldier, target))
					return CommandDisposition::Discard;
				DeductPoints(
					soldier, APBPConstants[AP_EXCHANGE_PLACES], 0);
				DeductPoints(
					target, APBPConstants[AP_EXCHANGE_PLACES], 0);
				return CommandDisposition::Applied;
			}
			else
			{
				return CommandDisposition::Discard;
			}
		}, command);
	}

	void SynchronizeExecutedCommandActors(
		const SimulationCommand& command) noexcept
	{
		if (command.valueless_by_exception()) return;
		std::visit([](const auto& value) noexcept {
			using Command = typename std::decay<decltype(value)>::type;
			if constexpr (
				!std::is_same<Command, EndTurnCommand>::value &&
				!std::is_same<Command, SynchronizeTurnCommand>::value)
			{
				if (SOLDIERTYPE* soldier =
					ResolveLiveCommandActor(value.soldier))
					(void)SynchronizeJa2TacticalEntityState(*soldier);

				if constexpr (
					std::is_same<Command, StartConversationCommand>::value ||
					std::is_same<Command, ApproachConversationCommand>::value ||
					std::is_same<Command, StealFromActorCommand>::value ||
					std::is_same<Command, ExchangePositionsCommand>::value)
				{
					if (SOLDIERTYPE* target =
						ResolveLiveCommandActor(value.target))
						(void)SynchronizeJa2TacticalEntityState(*target);
				}
				else if constexpr (
					std::is_same<Command, EnterVehicleCommand>::value ||
					std::is_same<Command, ApproachVehicleCommand>::value)
				{
					if (SOLDIERTYPE* vehicle =
						ResolveLiveCommandActor(value.vehicle))
						(void)SynchronizeJa2TacticalEntityState(*vehicle);
				}
			}
		}, command);
	}

	class Ja2SimulationCommandExecutor final
		: public SimulationCommandExecutor
	{
	public:
		CommandDisposition execute(
			const SimulationCommand& command,
			std::uint64_t,
			std::uint64_t) override
		{
			const CommandDisposition disposition =
				ExecuteSimulationCommand(command);
			SynchronizeExecutedCommandActors(command);
			return disposition;
		}
	};

	SimulationCommandExecutor& ApplicationSimulationCommandExecutor()
	{
		static Ja2SimulationCommandExecutor executor;
		return executor;
	}

	class FanOutSimulationCommandExecutionSink final
		: public SimulationCommandExecutionSink
	{
	public:
		FanOutSimulationCommandExecutionSink(
			SimulationCommandExecutionSink* first,
			SimulationCommandExecutionSink* second) noexcept
			: first_(first),
			  second_(second != first ? second : nullptr)
		{
		}

		explicit operator bool() const noexcept
		{
			return first_ || second_;
		}

		void commandProcessed(
			const SimulationCommand& command,
			std::uint64_t tick,
			std::uint64_t sequence,
			CommandDisposition disposition) noexcept override
		{
			if (first_)
			{
				try
				{
					first_->commandProcessed(
						command, tick, sequence, disposition);
				}
				catch (...)
				{
				}
			}
			if (second_)
			{
				try
				{
					second_->commandProcessed(
						command, tick, sequence, disposition);
				}
				catch (...)
				{
				}
			}
		}

	private:
		SimulationCommandExecutionSink* first_;
		SimulationCommandExecutionSink* second_;
	};

	template<typename Process>
	auto ExecuteSimulationCommands(
		Process&& process, SimulationCommandExecutionSink* sink = nullptr)
	{
		GameContext& game = GetGameContext();
		FanOutSimulationCommandExecutionSink observer{
			ApplicationExecutionSink(), sink};
		return process(game.runtime(), observer ? &observer : nullptr);
	}

	ExpectedCommandProcessingResult ExecuteExpectedSimulationCommand(
		std::uint64_t tick, std::uint64_t sequence)
	{
		return ExecuteSimulationCommands(
			[tick, sequence](
				auto& runtime,
				SimulationCommandExecutionSink* observer) {
				return runtime.executeExpectedCommandThrough(
					tick, sequence, observer);
			});
	}
}

bool BindJa2SimulationCommandExecutor(GameContext& game) noexcept
{
	return game.runtime().bindSimulationCommandExecutor(
		ApplicationSimulationCommandExecutor());
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
		if constexpr (
			std::is_same<Command, EndTurnCommand>::value ||
			std::is_same<Command, SynchronizeTurnCommand>::value)
		{
			if (value.nextTeam >= MAXTEAMS)
				return SimulationCommandDomainError::InvalidTeam;
			if constexpr (
				std::is_same<Command, SynchronizeTurnCommand>::value)
				if (!IsSimulationSynchronizationSource(value.source))
					return SimulationCommandDomainError::InvalidSource;
			return SimulationCommandDomainError::None;
		}
		else
		{
			if (!value.soldier.valid() || value.soldier.slot >= TOTAL_SOLDIERS)
				return SimulationCommandDomainError::InvalidActor;
			if constexpr (std::is_same<Command, ChangeStanceCommand>::value)
			{
				if (!IsValidTacticalEventPolicy(value.eventPolicy))
					return SimulationCommandDomainError::InvalidEventPolicy;
				if (value.source == SimulationCommandSource::NetworkPeer &&
					value.eventPolicy != TacticalEventPolicy::LocalOnly)
					return SimulationCommandDomainError::InvalidEventPolicy;
				return value.stance == ANIM_STAND ||
					value.stance == ANIM_CROUCH ||
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
			else if constexpr (
				std::is_same<Command, BeginSelectedFireWeaponCommand>::value)
			{
				if (!IsSimulationSystemSource(value.source))
					return SimulationCommandDomainError::InvalidSource;
				if (value.targetGrid < 0 || value.targetGrid >= WORLD_MAX)
					return SimulationCommandDomainError::InvalidTargetGrid;
				if (value.targetLevel != FIRST_LEVEL &&
					value.targetLevel != SECOND_LEVEL)
					return SimulationCommandDomainError::InvalidTargetLevel;
				if (value.targetCubeLevel < 0 ||
					value.targetCubeLevel > PROFILE_Z_SIZE)
					return SimulationCommandDomainError::InvalidTargetCubeLevel;
				if (value.attackingHand >= NUM_INV_SLOTS)
					return SimulationCommandDomainError::InvalidAttackingHand;
				if (value.attackingWeapon >= MAXITEMS)
					return SimulationCommandDomainError::InvalidAttackingWeapon;
				return SimulationCommandDomainError::None;
			}
			else if constexpr (
				std::is_same<Command, SynchronizeActorFireCommand>::value)
			{
				if (!IsSimulationSynchronizationSource(value.source))
					return SimulationCommandDomainError::InvalidSource;
				if (value.targetGrid < 0 || value.targetGrid >= WORLD_MAX)
					return SimulationCommandDomainError::InvalidTargetGrid;
				if (value.targetLevel != FIRST_LEVEL &&
					value.targetLevel != SECOND_LEVEL)
					return SimulationCommandDomainError::InvalidTargetLevel;
				if (value.targetCubeLevel < 0 ||
					value.targetCubeLevel > PROFILE_Z_SIZE)
					return SimulationCommandDomainError::InvalidTargetCubeLevel;
				if (value.attackingWeapon >= MAXITEMS)
					return SimulationCommandDomainError::InvalidAttackingWeapon;
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
			else if constexpr (
				std::is_same<Command, SynchronizeActorPathCommand>::value)
			{
				if (!IsSimulationSynchronizationSource(value.source))
					return SimulationCommandDomainError::InvalidSource;
				if (value.reportedGrid < 0 ||
					value.reportedGrid >= WORLD_MAX)
					return SimulationCommandDomainError::InvalidReportedGrid;
				if (value.destinationGrid < 0 ||
					value.destinationGrid >= WORLD_MAX)
					return SimulationCommandDomainError::InvalidDestinationGrid;
				if (value.movementState >= NUMANIMATIONSTATES)
					return SimulationCommandDomainError::InvalidMovementMode;
				if (value.pathSize > TacticalReplicatedPathCapacity ||
					value.currentPathIndex > value.pathSize)
					return SimulationCommandDomainError::InvalidReplicatedPath;
				return SimulationCommandDomainError::None;
			}
			else if constexpr (std::is_same<Command, SetFacingCommand>::value)
			{
				if (!IsValidTacticalEventPolicy(value.eventPolicy))
					return SimulationCommandDomainError::InvalidEventPolicy;
				if (value.source == SimulationCommandSource::NetworkPeer &&
					value.eventPolicy != TacticalEventPolicy::LocalOnly)
					return SimulationCommandDomainError::InvalidEventPolicy;
				return IsValidTacticalDirection(value.direction)
					? SimulationCommandDomainError::None
					: SimulationCommandDomainError::InvalidDirection;
			}
			else if constexpr (
				std::is_same<Command, SetStealthModeCommand>::value ||
				std::is_same<Command, StopMovementCommand>::value ||
				std::is_same<Command, CancelDragCommand>::value ||
				std::is_same<Command, CycleWeaponModeCommand>::value ||
				std::is_same<Command, ReloadWeaponCommand>::value)
			{
				return SimulationCommandDomainError::None;
			}
			else if constexpr (
				std::is_same<Command, SynchronizeActorStopCommand>::value)
			{
				if (!IsSimulationSynchronizationSource(value.source))
					return SimulationCommandDomainError::InvalidSource;
				if (value.reportedGrid < 0 ||
					value.reportedGrid >= WORLD_MAX)
					return SimulationCommandDomainError::InvalidReportedGrid;
				if (value.positionX < 0 || value.positionY < 0 ||
					value.positionX >= WORLD_COORD_COLS ||
					value.positionY >= WORLD_COORD_ROWS)
					return SimulationCommandDomainError::
						InvalidReplicatedPosition;
				return IsValidTacticalDirection(value.direction)
					? SimulationCommandDomainError::None
					: SimulationCommandDomainError::InvalidDirection;
			}
			else if constexpr (
				std::is_same<Command, SetWeaponReadyCommand>::value)
			{
				return IsValidTacticalDirection(value.direction)
					? SimulationCommandDomainError::None
					: SimulationCommandDomainError::InvalidDirection;
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
			else if constexpr (
				std::is_same<Command, StealFromActorCommand>::value)
			{
				if (!value.target.valid() ||
					value.target.slot >= TOTAL_SOLDIERS ||
					value.target == value.soldier)
					return SimulationCommandDomainError::InvalidTargetActor;
				if (value.targetGrid < 0 || value.targetGrid >= WORLD_MAX)
					return SimulationCommandDomainError::InvalidTargetGrid;
				if (value.targetLevel != FIRST_LEVEL &&
					value.targetLevel != SECOND_LEVEL)
					return SimulationCommandDomainError::InvalidTargetLevel;
				return SimulationCommandDomainError::None;
			}
			else if constexpr (
				std::is_same<Command, ExchangePositionsCommand>::value)
			{
				if (!value.target.valid() ||
					value.target.slot >= TOTAL_SOLDIERS ||
					value.target == value.soldier)
					return SimulationCommandDomainError::InvalidTargetActor;
				if (value.soldierGrid < 0 || value.soldierGrid >= WORLD_MAX)
					return SimulationCommandDomainError::InvalidActorGrid;
				if (value.targetGrid < 0 || value.targetGrid >= WORLD_MAX)
					return SimulationCommandDomainError::InvalidTargetGrid;
				if (value.level != FIRST_LEVEL &&
					value.level != SECOND_LEVEL)
					return SimulationCommandDomainError::InvalidActorLevel;
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
		[tick](
			auto& runtime,
			SimulationCommandExecutionSink* observer) {
			return runtime.executeCommandsThrough(tick, observer);
		});
}

CommandProcessingResult ExecuteSimulationCommandsThrough(
	std::uint64_t tick, std::size_t maximumCommands)
{
	return ExecuteSimulationCommands(
		[tick, maximumCommands](
			auto& runtime,
			SimulationCommandExecutionSink* observer) {
			return runtime.executeCommandsThrough(
				tick, maximumCommands, observer);
		});
}

CommandProcessingResult ExecuteSimulationCommandsThrough(
	std::uint64_t tick, std::size_t maximumCommands,
	SimulationCommandExecutionSink& sink)
{
	return ExecuteSimulationCommands(
		[tick, maximumCommands](
			auto& runtime,
			SimulationCommandExecutionSink* observer) {
			return runtime.executeCommandsThrough(
				tick, maximumCommands, observer);
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

namespace
{
	SimulationCommandDispatchResult TryDispatchRetainedSimulationCommand(
		SimulationCommand command,
		SimulationCommandSource requiredSource) noexcept
	{
		SimulationCommandDispatchResult invalid;
		invalid.tick =
			GetGameContext().runtime().simulationTicks().completedTickSequence();
		const bool ownedByRequiredSource =
			!command.valueless_by_exception() &&
			std::visit([requiredSource](const auto& value) noexcept {
				return value.source == requiredSource;
			}, command);
		if (!ownedByRequiredSource ||
			ValidateSimulationCommandDomain(command) !=
				SimulationCommandDomainError::None)
		{
			invalid.status = SimulationCommandDispatchStatus::InvalidDomain;
			return invalid;
		}

		SimulationCommandDispatchResult result =
			TryDispatchSimulationCommandNow(command);
		if (result.submitted ||
			(result.status !=
					SimulationCommandDispatchStatus::AuthoritativeBackpressure &&
				result.status !=
					SimulationCommandDispatchStatus::FrameBudgetExhausted))
			return result;

		GameContext& game = GetGameContext();
		if (game.commands().sequenceExhausted())
		{
			result.status = SimulationCommandDispatchStatus::SequenceExhausted;
			return result;
		}
		try
		{
			result.sequence =
				game.submitCommand(result.tick, std::move(command));
			result.submitted = true;
			result.status = SimulationCommandDispatchStatus::RetryDeferred;
		}
		catch (const std::overflow_error&)
		{
			result.status = SimulationCommandDispatchStatus::SequenceExhausted;
		}
		catch (...)
		{
			result.status = SimulationCommandDispatchStatus::SubmissionFailure;
		}
		return result;
	}
}

SimulationCommandDispatchResult TryDispatchNetworkSimulationCommand(
	SimulationCommand command) noexcept
{
	return TryDispatchRetainedSimulationCommand(
		std::move(command), SimulationCommandSource::NetworkPeer);
}

SimulationCommandDispatchResult TryDispatchSystemSimulationCommand(
	SimulationCommand command) noexcept
{
	return TryDispatchRetainedSimulationCommand(
		std::move(command), SimulationCommandSource::System);
}

SimulationCommandDispatchResult TryDispatchNetworkChangeStanceCommand(
	SOLDIERTYPE& soldier, std::uint8_t stance) noexcept
{
	return DispatchNetworkActorCommand(
		soldier, [stance](TacticalEntityId actor) {
			return ChangeStanceCommand{
				actor, stance, SimulationCommandSource::NetworkPeer,
				TacticalEventPolicy::LocalOnly};
		});
}

SimulationCommandDispatchResult TryDispatchNetworkSetFacingCommand(
	SOLDIERTYPE& soldier, std::uint8_t direction) noexcept
{
	return DispatchNetworkActorCommand(
		soldier, [direction](TacticalEntityId actor) {
			return SetFacingCommand{
				actor, direction, SimulationCommandSource::NetworkPeer,
				TacticalEventPolicy::LocalOnly};
		});
}

SimulationCommandDispatchResult TryDispatchNetworkActorPathCommand(
	SOLDIERTYPE& soldier,
	std::int32_t reportedGrid,
	std::int32_t destinationGrid,
	std::uint16_t movementState,
	std::uint16_t currentPathIndex,
	const std::uint16_t* path,
	std::uint16_t pathSize) noexcept
{
	if (pathSize > TacticalReplicatedPathCapacity ||
		currentPathIndex > pathSize || (!path && pathSize != 0))
	{
		SimulationCommandDispatchResult result;
		result.status = SimulationCommandDispatchStatus::InvalidDomain;
		result.tick =
			GetGameContext().runtime().simulationTicks().completedTickSequence();
		return result;
	}
	std::array<std::uint16_t, TacticalReplicatedPathCapacity> captured{};
	for (std::size_t index = 0; index < pathSize; ++index)
		captured[index] = path[index];
	return DispatchNetworkActorCommand(
		soldier,
		[reportedGrid, destinationGrid, movementState, currentPathIndex,
			pathSize, captured](TacticalEntityId actor) {
			return SynchronizeActorPathCommand{
				actor, reportedGrid, destinationGrid, movementState,
				currentPathIndex, pathSize, captured,
				SimulationCommandSource::NetworkPeer};
		});
}

SimulationCommandDispatchResult TryDispatchNetworkActorFireCommand(
	SOLDIERTYPE& soldier,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	std::uint32_t attackingWeapon) noexcept
{
	return DispatchNetworkActorCommand(
		soldier,
		[targetGrid, targetLevel, targetCubeLevel, attackingWeapon](
			TacticalEntityId actor) {
			return SynchronizeActorFireCommand{
				actor, targetGrid, targetLevel, targetCubeLevel,
				attackingWeapon, SimulationCommandSource::NetworkPeer};
		});
}

SimulationCommandDispatchResult TryDispatchNetworkActorStopCommand(
	SOLDIERTYPE& soldier,
	std::int32_t reportedGrid,
	std::int16_t positionX,
	std::int16_t positionY,
	std::uint8_t direction,
	bool stop) noexcept
{
	return DispatchNetworkActorCommand(
		soldier,
		[reportedGrid, positionX, positionY, direction, stop](
			TacticalEntityId actor) {
			return SynchronizeActorStopCommand{
				actor, reportedGrid, positionX, positionY, direction, stop,
				SimulationCommandSource::NetworkPeer};
		});
}

SimulationCommandDispatchResult TryDispatchNetworkTurnCommand(
	std::uint8_t nextTeam,
	bool enterCombat,
	bool endClientTurn) noexcept
{
	return TryDispatchNetworkSimulationCommand(
		SimulationCommand{SynchronizeTurnCommand{
			nextTeam, enterCombat, endClientTurn,
			SimulationCommandSource::NetworkPeer}});
}

SimulationCommandDispatchResult TryDispatchSystemChangeStanceCommand(
	SOLDIERTYPE& soldier,
	std::uint8_t stance,
	TacticalEventPolicy eventPolicy) noexcept
{
	return DispatchSystemActorCommand(
		soldier, [stance, eventPolicy](TacticalEntityId actor) {
			return ChangeStanceCommand{
				actor, stance, SimulationCommandSource::System, eventPolicy};
		});
}

SimulationCommandDispatchResult TryDispatchSystemSetFacingCommand(
	SOLDIERTYPE& soldier,
	std::uint8_t direction,
	TacticalEventPolicy eventPolicy) noexcept
{
	return DispatchSystemActorCommand(
		soldier, [direction, eventPolicy](TacticalEntityId actor) {
			return SetFacingCommand{
				actor, direction, SimulationCommandSource::System, eventPolicy};
		});
}

SimulationCommandDispatchResult TryDispatchSystemMoveToGridCommand(
	SOLDIERTYPE& soldier,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart) noexcept
{
	return DispatchSystemActorCommand(
		soldier,
		[destinationGrid, movementMode, reverse, forceRestart](
			TacticalEntityId actor) {
			return MoveToGridCommand{
				actor, destinationGrid, movementMode, reverse, forceRestart,
				SimulationCommandSource::System,
				TacticalMoveOrigin::System,
				TacticalPendingActionPolicy::Preserve};
		});
}

SimulationCommandDispatchResult
TryDispatchSystemBeginSelectedFireWeaponCommand(
	SOLDIERTYPE& soldier,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	std::uint8_t attackingHand,
	std::uint32_t attackingWeapon) noexcept
{
	return DispatchSystemActorCommand(
		soldier,
		[targetGrid, targetLevel, targetCubeLevel, attackingHand,
			attackingWeapon](TacticalEntityId actor) {
			return BeginSelectedFireWeaponCommand{
				actor, targetGrid, targetLevel, targetCubeLevel,
				attackingHand, attackingWeapon,
				SimulationCommandSource::System};
		});
}

SimulationCommandDispatchResult TryDispatchEndTurnCommandNow(
	std::uint8_t nextTeam, SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{EndTurnCommand{nextTeam, source}});
}

SimulationCommandDispatchResult TryDispatchChangeStanceCommandNow(
	SOLDIERTYPE& soldier,
	std::uint8_t stance,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier, [stance, source](TacticalEntityId actor) {
			return ChangeStanceCommand{actor, stance, source};
		});
}

SimulationCommandDispatchResult TryDispatchBeginFireWeaponCommandNow(
	SOLDIERTYPE& soldier,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier,
		[targetGrid, targetLevel, targetCubeLevel, source](
			TacticalEntityId actor) {
			return BeginFireWeaponCommand{
				actor, targetGrid, targetLevel, targetCubeLevel, source};
		});
}

SimulationCommandDispatchResult TryDispatchMoveToGridCommandNow(
	SOLDIERTYPE& soldier,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier,
		[destinationGrid, movementMode, reverse, forceRestart, source](
			TacticalEntityId actor) {
			return MoveToGridCommand{
				actor, destinationGrid, movementMode,
				reverse, forceRestart, source};
		});
}

SimulationCommandDispatchResult TryDispatchSetFacingCommandNow(
	SOLDIERTYPE& soldier,
	std::uint8_t direction,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier, [direction, source](TacticalEntityId actor) {
			return SetFacingCommand{actor, direction, source};
		});
}

SimulationCommandDispatchResult TryDispatchSetStealthModeCommandNow(
	SOLDIERTYPE& soldier,
	bool enabled,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier, [enabled, source](TacticalEntityId actor) {
			return SetStealthModeCommand{actor, enabled, source};
		});
}

SimulationCommandDispatchResult TryDispatchStopMovementCommandNow(
	SOLDIERTYPE& soldier,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier, [source](TacticalEntityId actor) {
			return StopMovementCommand{actor, source};
		});
}

SimulationCommandDispatchResult TryDispatchCancelDragCommandNow(
	SOLDIERTYPE& soldier,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier, [source](TacticalEntityId actor) {
			return CancelDragCommand{actor, source};
		});
}

SimulationCommandDispatchResult TryDispatchCycleWeaponModeCommandNow(
	SOLDIERTYPE& soldier,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier, [source](TacticalEntityId actor) {
			return CycleWeaponModeCommand{actor, source};
		});
}

SimulationCommandDispatchResult TryDispatchCycleScopeModeCommandNow(
	SOLDIERTYPE& soldier,
	std::int32_t targetGrid,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier, [targetGrid, source](TacticalEntityId actor) {
			return CycleScopeModeCommand{actor, targetGrid, source};
		});
}

SimulationCommandDispatchResult TryDispatchReloadWeaponCommandNow(
	SOLDIERTYPE& soldier,
	bool reloadEvenIfNotEmpty,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier, [reloadEvenIfNotEmpty, source](TacticalEntityId actor) {
			return ReloadWeaponCommand{
				actor, reloadEvenIfNotEmpty, source};
		});
}

SimulationCommandDispatchResult TryDispatchSetWeaponReadyCommandNow(
	SOLDIERTYPE& soldier,
	std::uint8_t direction,
	bool ready,
	bool alternativeHold,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier,
		[direction, ready, alternativeHold, source](
			TacticalEntityId actor) {
			return SetWeaponReadyCommand{
				actor, direction, ready, alternativeHold, source};
		});
}

SimulationCommandDispatchResult TryDispatchTraverseObstacleCommandNow(
	SOLDIERTYPE& soldier,
	TacticalTraversalKind kind,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier, [kind, source](TacticalEntityId actor) {
			return TraverseObstacleCommand{actor, kind, source};
		});
}

SimulationCommandDispatchResult TryDispatchActivateWorldObjectCommandNow(
	SOLDIERTYPE& soldier,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier,
		[objectGrid, structureId, direction, source](
			TacticalEntityId actor) {
			return ActivateWorldObjectCommand{
				actor, TacticalWorldObjectId{objectGrid, structureId},
				direction, source};
		});
}

SimulationCommandDispatchResult TryDispatchApproachWorldObjectCommandNow(
	SOLDIERTYPE& soldier,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier,
		[objectGrid, structureId, direction, destinationGrid, movementMode,
		 reverse, forceRestart, source](TacticalEntityId actor) {
			return ApproachWorldObjectCommand{
				actor, TacticalWorldObjectId{objectGrid, structureId},
				direction, destinationGrid, movementMode,
				reverse, forceRestart, source};
		});
}

SimulationCommandDispatchResult TryDispatchStartConversationCommandNow(
	SOLDIERTYPE& soldier,
	SOLDIERTYPE& target,
	SimulationCommandSource source) noexcept
{
	return DispatchActorPairCommand(
		soldier, target,
		[source](TacticalEntityId actor, TacticalEntityId targetActor) {
			return StartConversationCommand{actor, targetActor, source};
		});
}

SimulationCommandDispatchResult TryDispatchApproachConversationCommandNow(
	SOLDIERTYPE& soldier,
	SOLDIERTYPE& target,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool forceRestart,
	SimulationCommandSource source) noexcept
{
	return DispatchActorPairCommand(
		soldier, target,
		[destinationGrid, movementMode, forceRestart, source](
			TacticalEntityId actor, TacticalEntityId targetActor) {
			return ApproachConversationCommand{
				actor, targetActor, destinationGrid, movementMode,
				forceRestart, source};
		});
}

SimulationCommandDispatchResult TryDispatchEnterVehicleCommandNow(
	SOLDIERTYPE& soldier,
	SOLDIERTYPE& vehicle,
	std::uint8_t direction,
	std::uint8_t seatIndex,
	SimulationCommandSource source) noexcept
{
	return DispatchActorPairCommand(
		soldier, vehicle,
		[direction, seatIndex, source](
			TacticalEntityId actor, TacticalEntityId vehicleActor) {
			return EnterVehicleCommand{
				actor, vehicleActor, direction, seatIndex, source};
		});
}

SimulationCommandDispatchResult TryDispatchApproachVehicleCommandNow(
	SOLDIERTYPE& soldier,
	SOLDIERTYPE& vehicle,
	std::uint8_t direction,
	std::uint8_t seatIndex,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool forceRestart,
	SimulationCommandSource source) noexcept
{
	return DispatchActorPairCommand(
		soldier, vehicle,
		[direction, seatIndex, destinationGrid, movementMode,
		 forceRestart, source](
			TacticalEntityId actor, TacticalEntityId vehicleActor) {
			return ApproachVehicleCommand{
				actor, vehicleActor, direction, seatIndex,
				destinationGrid, movementMode, forceRestart, source};
		});
}

SimulationCommandDispatchResult TryDispatchPickupWorldItemCommandNow(
	SOLDIERTYPE& soldier,
	TacticalWorldItemId item,
	std::int32_t grid,
	std::int8_t renderHeight,
	TacticalWorldItemPickupKind kind,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		soldier, [item, grid, renderHeight, kind, source](
			TacticalEntityId actor) {
			return PickupWorldItemCommand{
				actor, item, grid, renderHeight, kind, source};
		});
}

SimulationCommandDispatchResult TryDispatchStealFromActorCommandNow(
	SOLDIERTYPE& soldier,
	SOLDIERTYPE& target,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	SimulationCommandSource source) noexcept
{
	return DispatchActorPairCommand(
		soldier, target,
		[targetGrid, targetLevel, source](
			TacticalEntityId actor, TacticalEntityId targetActor) {
			return StealFromActorCommand{
				actor, targetActor, targetGrid, targetLevel, source};
		});
}

SimulationCommandDispatchResult TryDispatchExchangePositionsCommandNow(
	SOLDIERTYPE& soldier,
	SOLDIERTYPE& target,
	std::int32_t soldierGrid,
	std::int32_t targetGrid,
	std::int8_t level,
	SimulationCommandSource source) noexcept
{
	return DispatchActorPairCommand(
		soldier, target,
		[soldierGrid, targetGrid, level, source](
			TacticalEntityId actor, TacticalEntityId targetActor) {
			return ExchangePositionsCommand{
				actor, targetActor, soldierGrid, targetGrid, level, source};
		});
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

SimulationCommandDispatchResult TryDispatchCancelDragCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{CancelDragCommand{
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

SimulationCommandDispatchResult TryDispatchSetWeaponReadyCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint8_t direction,
	bool ready,
	bool alternativeHold,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{SetWeaponReadyCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			direction, ready, alternativeHold, source}});
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

SimulationCommandDispatchResult TryDispatchStealFromActorCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint16_t targetId,
	std::uint32_t targetUniqueSoldierId,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{StealFromActorCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			TacticalEntityId{targetId, targetUniqueSoldierId},
			targetGrid, targetLevel, source}});
}

SimulationCommandDispatchResult TryDispatchExchangePositionsCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint16_t targetId,
	std::uint32_t targetUniqueSoldierId,
	std::int32_t soldierGrid,
	std::int32_t targetGrid,
	std::int8_t level,
	SimulationCommandSource source) noexcept
{
	return TryDispatchSimulationCommandNow(
		SimulationCommand{ExchangePositionsCommand{
			TacticalEntityId{soldierId, uniqueSoldierId},
			TacticalEntityId{targetId, targetUniqueSoldierId},
			soldierGrid, targetGrid, level, source}});
}

bool TryCompletePendingConversationCommand(SOLDIERTYPE& soldier) noexcept
{
	if (soldier.aiData.ubPendingAction != MERC_TALK) return false;
	const UINT32 targetSlot = soldier.aiData.uiPendingActionData1;
	const TacticalEntityId targetId{
		targetSlot < TOTAL_SOLDIERS
			? static_cast<std::uint16_t>(targetSlot)
			: static_cast<std::uint16_t>(TOTAL_SOLDIERS),
		soldier.runtime.pendingAction.targetIncarnation};

	soldier.aiData.ubPendingAction = NO_PENDING_ACTION;
	soldier.aiData.uiPendingActionData1 = 0;
	soldier.aiData.uiPendingActionData4 = 0;
	soldier.runtime.pendingAction.targetIncarnation = 0;

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
		soldier.runtime.pendingAction.targetIncarnation};

	soldier.aiData.ubPendingAction = NO_PENDING_ACTION;
	soldier.aiData.uiPendingActionData1 = 0;
	soldier.aiData.sPendingActionData2 = 0;
	soldier.aiData.bPendingActionData3 = 0;
	soldier.aiData.uiPendingActionData4 = 0;
	soldier.runtime.pendingAction.targetIncarnation = 0;

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

bool TryCompletePendingStealCommand(SOLDIERTYPE& soldier) noexcept
{
	if (soldier.aiData.ubPendingAction != MERC_STEAL) return false;

	SOLDIERTYPE* target = nullptr;
	if (soldier.runtime.pendingAction.targetIncarnation != 0)
	{
		target = ResolveStablePendingStealTarget(soldier);
	}
	else
	{
		SoldierID targetId = WhoIsThere2(
			soldier.aiData.sPendingActionData2,
			soldier.bTargetLevel);
		if (targetId != NOBODY) target = targetId;
	}

	const INT32 rawDirection = soldier.aiData.bPendingActionData3;
	if (!target ||
		target->sGridNo != soldier.aiData.sPendingActionData2 ||
		target->pathing.bLevel != soldier.bTargetLevel ||
		soldier.pathing.bLevel != target->pathing.bLevel ||
		PythSpacesAway(soldier.sGridNo, target->sGridNo) != 1 ||
		rawDirection < 0 ||
		!IsValidTacticalDirection(
			static_cast<std::uint8_t>(rawDirection)))
	{
		ClearPendingSteal(soldier);
		return false;
	}

	soldier.EVENT_SetSoldierDesiredDirection(
		static_cast<UINT8>(rawDirection));
	if (gAnimControl[soldier.usAnimState].ubEndHeight == ANIM_PRONE ||
		gAnimControl[soldier.usAnimState].ubEndHeight == ANIM_CROUCH ||
		gAnimControl[target->usAnimState].ubEndHeight == ANIM_PRONE)
	{
		soldier.EVENT_InitNewSoldierAnim(
			STEAL_ITEM_CROUCHED, 0, FALSE);
	}
	else
	{
		soldier.EVENT_InitNewSoldierAnim(STEAL_ITEM, 0, FALSE);
	}
	soldier.aiData.ubPendingAction = NO_PENDING_ACTION;
	return true;
}

SOLDIERTYPE* ResolveAndConsumePendingStealTarget(
	SOLDIERTYPE& soldier,
	std::int32_t targetGrid,
	std::int8_t targetLevel) noexcept
{
	SOLDIERTYPE* target = nullptr;
	if (soldier.runtime.pendingAction.targetIncarnation != 0)
	{
		target = ResolveStablePendingStealTarget(soldier);
	}
	else
	{
		target = SimpleFindSoldier(targetGrid, targetLevel);
	}

	soldier.aiData.uiPendingActionData1 = 0;
	soldier.aiData.sPendingActionData2 = 0;
	soldier.aiData.bPendingActionData3 = 0;
	soldier.aiData.uiPendingActionData4 = 0;
	soldier.runtime.pendingAction.targetIncarnation = 0;

	if (!target || target->sGridNo != targetGrid ||
		target->pathing.bLevel != targetLevel ||
		soldier.pathing.bLevel != target->pathing.bLevel ||
		PythSpacesAway(soldier.sGridNo, target->sGridNo) != 1)
	{
		UnSetUIBusy(soldier.ubID);
		return nullptr;
	}
	return target;
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
		soldier.runtime.pendingAction.targetIncarnation = 0;
		return true;
	}
	if (!PendingWorldItemMatches(soldier, itemIndex, grid, level))
	{
		ClearPendingWorldItemPickup(soldier);
		return false;
	}
	soldier.runtime.pendingAction.targetIncarnation = 0;
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
