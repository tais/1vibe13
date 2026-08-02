#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorEvents.h"
#include "TacticalActorPendingActionTypes.h"
#include "TacticalActorStateFlags.h"
#include "TacticalActorOrientation.h"
#include "TacticalActorRouteExecution.h"
#include "TacticalActorWorldPlacement.h"
#include "TacticalActorMobility.h"
#include "Simulation Commands.h"
#include "Simulation Command Legacy.h"
#include "SoldierRepository.h"
#include "TacticalActorDragging.h"
#include "TacticalActorInteractions.h"
#include "TacticalActorRangedActions.h"
#include "TacticalActorTraversal.h"
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
#include "TacticalActor.h"
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

	TacticalActor* ResolveLiveCommandActor(TacticalEntityId actor) noexcept
	{
		if (!IsJa2TacticalWorldLoaded()) return nullptr;
		TacticalActor* soldier = ResolveJa2TacticalEntity(actor);
		if (!soldier || !soldier->roster().inSector()) return nullptr;
		return soldier;
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
		TacticalEntityId actor, Builder&& builder) noexcept
	{
		if (!actor.valid()) return InvalidCommandActorResult();
		return TryDispatchSimulationCommandNow(
			SimulationCommand{builder(actor)});
	}

	template <typename Builder>
	SimulationCommandDispatchResult DispatchNetworkActorCommand(
		TacticalEntityId actor, Builder&& builder) noexcept
	{
		if (!actor.valid()) return InvalidCommandActorResult();
		return TryDispatchNetworkSimulationCommand(
			SimulationCommand{builder(actor)});
	}

	template <typename Builder>
	SimulationCommandDispatchResult DispatchSystemActorCommand(
		TacticalEntityId actor, Builder&& builder) noexcept
	{
		if (!actor.valid()) return InvalidCommandActorResult();
		return TryDispatchSystemSimulationCommand(
			SimulationCommand{builder(actor)});
	}

	template <typename Builder>
	SimulationCommandDispatchResult DispatchActorPairCommand(
		TacticalEntityId actor,
		TacticalEntityId target,
		Builder&& builder) noexcept
	{
		if (!actor.valid() || !target.valid())
			return InvalidCommandActorResult();
		return TryDispatchSimulationCommandNow(
			SimulationCommand{builder(actor, target)});
	}

	STRUCTURE* ResolveLiveWorldObject(TacticalWorldObjectId object) noexcept
	{
		if (!IsJa2TacticalWorldLoaded() ||
			object.grid < 0 || object.grid >= WORLD_MAX) return nullptr;
		return FindStructureByID(object.grid, object.structureId);
	}

	bool CanBeginWorldObjectInteraction(const TacticalActor& soldier) noexcept
	{
		return soldier.animationPlayback().state() != OPEN_STRUCT &&
			soldier.animationPlayback().state() != OPEN_STRUCT_CROUCHED &&
			soldier.animationPlayback().state() != BEGIN_OPENSTRUCT &&
			soldier.animationPlayback().state() != BEGIN_OPENSTRUCT_CROUCHED;
	}

	bool IsValidConversationPair(
		const TacticalActor& soldier, const TacticalActor& target) noexcept
	{
		return soldier.identity().id() != target.identity().id() && target.roster().active() && target.roster().inSector();
	}

	bool IsAtIssuedTacticalPosition(
		const TacticalActor& soldier,
		std::int32_t grid,
		std::int8_t level) noexcept
	{
		return soldier.position().gridNo() == grid && soldier.position().level() == level;
	}

	bool CanExecutePositionExchange(
		TacticalActor& soldier,
		TacticalActor& target,
		const ExchangePositionsCommand& command) noexcept
	{
		if (!IsAtIssuedTacticalPosition(
				soldier, command.soldierGrid, command.level) ||
			!IsAtIssuedTacticalPosition(
				target, command.targetGrid, command.level) ||
			soldier.vitals().health() < OKLIFE ||
			target.vitals().health() < OKLIFE ||
			PythSpacesAway(soldier.position().gridNo(), target.position().gridNo()) != 1 ||
			(!target.aiBehavior().neutral() && target.roster().side() != gbPlayerNum))
			return false;
		return CanExchangePlaces(&soldier, &target, FALSE) == TRUE;
	}

	bool HasValidVehicleSeat(
		const TacticalActor& vehicle, std::uint8_t seatIndex) noexcept
	{
		const INT32 capacity =
			GetVehicleSeatingCapacity(vehicle.vehicleState().tacticalVehicleId());
		return capacity > 0 && seatIndex < capacity;
	}

	bool CanEnterCommandVehicle(
		TacticalActor& soldier, TacticalActor& vehicle,
		std::uint8_t seatIndex) noexcept
	{
		if (soldier.identity().id() == vehicle.identity().id() ||
			(soldier.status().flags() &
				(SOLDIER_DRIVER | SOLDIER_PASSENGER | SOLDIER_VEHICLE)) != 0 ||
			!OK_ENTERABLE_VEHICLE((&vehicle)) ||
			vehicle.awareness().visibility() == -1 ||
			!OKUseVehicle(vehicle.identity().profile()) ||
			!IsThisVehicleAccessibleToSoldier(
				&soldier, vehicle.vehicleState().tacticalVehicleId()) ||
			!HasValidVehicleSeat(vehicle, seatIndex))
			return false;
		return IsEnoughSpaceInVehicle(vehicle.vehicleState().tacticalVehicleId()) == TRUE;
	}

	void ClearPendingWorldItemPickup(TacticalActor& soldier) noexcept
	{
		soldier.pendingAction().clearAction();
		soldier.pendingAction().primaryData() = 0;
		soldier.pendingAction().secondaryData() = 0;
		soldier.pendingAction().tertiaryData() = 0;
		soldier.pendingAction().quaternaryData() = 0;
		soldier.runtime().pendingAction.targetIncarnation = 0;
		UnSetUIBusy(soldier.identity().id());
	}

	void ClearPendingSteal(TacticalActor& soldier) noexcept
	{
		soldier.pendingAction().clearAction();
		soldier.pendingAction().primaryData() = 0;
		soldier.pendingAction().secondaryData() = 0;
		soldier.pendingAction().tertiaryData() = 0;
		soldier.pendingAction().quaternaryData() = 0;
		soldier.runtime().pendingAction.targetIncarnation = 0;
		UnSetUIBusy(soldier.identity().id());
	}

	TacticalActor* ResolveStablePendingStealTarget(
		const TacticalActor& soldier) noexcept
	{
		const UINT32 rawSlot = soldier.pendingAction().primaryData();
		if (rawSlot >= TOTAL_SOLDIERS ||
			soldier.runtime().pendingAction.targetIncarnation == 0)
			return nullptr;
		return ResolveLiveCommandActor(TacticalEntityId{
			static_cast<std::uint16_t>(rawSlot),
			soldier.runtime().pendingAction.targetIncarnation});
	}

	bool PendingWorldItemMatches(
		const TacticalActor& soldier,
		std::int32_t itemIndex,
		std::int32_t grid,
		std::int8_t level) noexcept
	{
		if (soldier.runtime().pendingAction.targetIncarnation == 0)
			return true;
		const UINT32 rawSlot = soldier.pendingAction().primaryData();
		const TacticalWorldItemId itemId{
			rawSlot, soldier.runtime().pendingAction.targetIncarnation};
		WORLDITEM* item = ResolveJa2TacticalWorldItem(itemId);
		return item &&
			itemIndex >= 0 &&
			static_cast<UINT32>(itemIndex) == rawSlot &&
			grid == soldier.pendingAction().quaternaryData() &&
			level == soldier.pendingAction().tertiaryData() &&
			item->sGridNo == grid &&
			item->ubLevel == soldier.position().level() &&
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
				if (TacticalActor* soldier = ResolveLiveCommandActor(value.soldier))
				{
					if (value.eventPolicy == TacticalEventPolicy::LocalOnly)
					{
						// Some received and scripted actions deliberately bypass
						// outbound replication while retaining the same local
						// stance transition.
						(void)TacticalActorOrientation::changeStance(*soldier, value.stance);
						return CommandDisposition::Applied;
					}
					const bool realtimeStanceChange =
						(gTacticalStatus.uiFlags & REALTIME) != 0 ||
						(IsJa2TacticalCombatActive()) == 0;
					if (realtimeStanceChange &&
						(gAnimControl[soldier->animationPlayback().state()].uiFlags &
							ANIM_STATIONARY) == 0)
					{
						soldier->movement().mode() =
							TacticalActorMobility::movementStateForStance(*soldier, value.stance);
						soldier->animationIntent().clearDesiredHeight();
						soldier->movement().requestGridUpdateSuppression();
						TacticalActorAnimationTransitions::changeState(*soldier,
							soldier->movement().mode(), 0, FALSE);
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
				if (TacticalActor* soldier = ResolveLiveCommandActor(value.soldier))
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
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier ||
					!IsSimulationSystemSource(value.source) ||
					value.attackingHand >= NUM_INV_SLOTS ||
					value.attackingWeapon >= MAXITEMS)
					return CommandDisposition::Discard;
				soldier->attackSelection().selectWeapon(
					value.attackingHand,
					static_cast<UINT16>(value.attackingWeapon));
				soldier->targeting().gridNo() = value.targetGrid;
				soldier->targeting().level() = value.targetLevel;
				soldier->targeting().cubeLevel() = value.targetCubeLevel;
				SendBeginFireWeaponEvent(soldier, value.targetGrid);
				if (value.source == SimulationCommandSource::System &&
					(is_server ||
						(is_client && soldier->identity().id() < 20)))
					send_fire(soldier, value.targetGrid);
				return CommandDisposition::Applied;
			}
			else if constexpr (
				std::is_same<Command, SynchronizeActorFireCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier ||
					!IsSimulationSynchronizationSource(value.source) ||
					value.attackingWeapon >= MAXITEMS)
					return CommandDisposition::Discard;
				soldier->targeting().gridNo() = value.targetGrid;
				soldier->targeting().level() = value.targetLevel;
				soldier->targeting().cubeLevel() = value.targetCubeLevel;
				soldier->attackSelection().weapon() =
					static_cast<UINT16>(value.attackingWeapon);
				SendBeginFireWeaponEvent(soldier, value.targetGrid);
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, MoveToGridCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier ||
					!TacticalActorMobility::isValidMovementMode(*soldier, value.movementMode))
					return CommandDisposition::Discard;

				soldier->movement().mode() = value.movementMode;
				soldier->movement().setReverse(value.reverse);
				if (value.pendingAction == TacticalPendingActionPolicy::Clear)
					soldier->pendingAction().clearAction();
				return TacticalActorRouteExecution::requestPath(*soldier,
					value.destinationGrid, value.movementMode,
					static_cast<TacticalActorRouteExecution::PathOrigin>(
						value.origin),
					value.forceRestart)
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (std::is_same<Command, SetFacingCommand>::value)
			{
				if (TacticalActor* soldier = ResolveLiveCommandActor(value.soldier))
				{
					if (value.eventPolicy == TacticalEventPolicy::LocalOnly)
						(void)TacticalActorOrientation::setDesiredDirection(*soldier, value.direction);
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
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier ||
					!IsSimulationSynchronizationSource(value.source))
					return CommandDisposition::Discard;

				for (std::size_t index = 0;
					index < TacticalReplicatedPathCapacity; ++index)
					soldier->pathing().path()[index] = value.path[index];
				soldier->pathing().destinationGrid() = value.destinationGrid;
				soldier->pathing().finalDestinationGrid() = value.destinationGrid;
				soldier->pathing().pathIndex() = value.currentPathIndex;
				soldier->pathing().pathSize() = value.pathSize;

				SendGetNewSoldierPathEvent(
					soldier, value.destinationGrid, value.movementState);

				INT16 positionX = 0;
				INT16 positionY = 0;
				ConvertGridNoToCenterCellXY(
					value.reportedGrid, &positionX, &positionY);
				if ((gAnimControl[soldier->animationPlayback().state()].uiFlags &
						(ANIM_MOVING | ANIM_SPECIALMOVE)) == 0 ||
					soldier->movement().outOfActionPoints())
					(void)TacticalActorWorldPlacement::setPosition(*soldier,
						positionX, positionY, FALSE, FALSE, FALSE);
				TacticalActorAnimationTransitions::initializeAnimation(*soldier,
					value.movementState, 0, FALSE);
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, SetStealthModeCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier ||
					(soldier->status().flags() & SOLDIER_VEHICLE) != 0)
					return CommandDisposition::Discard;
				soldier->movement().setStealth(value.enabled);
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, StopMovementCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier) return CommandDisposition::Discard;
				soldier->movement().clearDelay();
				soldier->pathing().finalDestinationGrid() = soldier->position().gridNo();
				(void)TacticalActorRouteExecution::stop(*soldier);
				return CommandDisposition::Applied;
			}
			else if constexpr (
				std::is_same<Command, SynchronizeActorStopCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier ||
					!IsSimulationSynchronizationSource(value.source))
					return CommandDisposition::Discard;
				(void)TacticalActorWorldPlacement::setPosition(*soldier,
					value.positionX, value.positionY, FALSE, FALSE, FALSE);
				(void)TacticalActorOrientation::setDirection(*soldier, value.direction);
				if (value.stop && soldier->roster().team() >= LAN_TEAM_ONE &&
					soldier->position().gridNo() >= 0 &&
					soldier->position().gridNo() < WORLD_MAX &&
					(gAnimControl[soldier->animationPlayback().state()].uiFlags &
						ANIM_MOVING) != 0)
					(void)TacticalActorRouteExecution::stopAt(*soldier,
						soldier->position().gridNo(), soldier->position().direction());
				(void)TacticalActorRouteExecution::setOutOfActionPoints(*soldier,
					value.stop ? TRUE : FALSE);
				soldier->animationActivity().turningFromProneMode() = FALSE;
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, CancelDragCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier || !TacticalActorDragging::isDragging(*soldier))
					return CommandDisposition::Discard;
				TacticalActorDragging::cancel(*soldier);
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, CycleWeaponModeCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier || !soldier->inventory()[HANDPOS].exists() ||
					(gAnimControl[soldier->animationPlayback().state()].uiFlags & ANIM_FIRE) != 0)
					return CommandDisposition::Discard;
				ChangeWeaponMode(soldier);
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, CycleScopeModeCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier || !soldier->inventory()[HANDPOS].exists() ||
					(gAnimControl[soldier->animationPlayback().state()].uiFlags & ANIM_FIRE) != 0)
					return CommandDisposition::Discard;
				ChangeScopeMode(soldier, value.targetGrid);
				ManLooksForOtherTeams(soldier);
				return CommandDisposition::Applied;
			}
			else if constexpr (std::is_same<Command, ReloadWeaponCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier || !soldier->inventory()[HANDPOS].exists())
					return CommandDisposition::Discard;
				return AutoReload(soldier, value.reloadEvenIfNotEmpty)
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, SetWeaponReadyCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier) return CommandDisposition::Discard;
				return TacticalActorRangedActions::readyFacing(
					*soldier,
					value.direction,
					!value.ready,
					value.alternativeHold)
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (std::is_same<Command, TraverseObstacleCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier) return CommandDisposition::Discard;
				bool started = false;
				switch (value.kind)
				{
					case TacticalTraversalKind::ClimbUpRoof:
						started = TacticalActorTraversal::
							beginRoofClimb(*soldier);
						break;
					case TacticalTraversalKind::ClimbDownRoof:
						started = TacticalActorTraversal::
							beginRoofDescent(*soldier);
						break;
					case TacticalTraversalKind::JumpFence:
						started = TacticalActorTraversal::
							beginFenceJump(*soldier);
						break;
					case TacticalTraversalKind::ClimbWall:
						started = TacticalActorTraversal::
							beginWallClimb(*soldier);
						break;
					case TacticalTraversalKind::JumpWindow:
						started = TacticalActorTraversal::
							beginWindowJump(*soldier);
						break;
				}
				return started
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, ActivateWorldObjectCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
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
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier || !CanBeginWorldObjectInteraction(*soldier) ||
					!TacticalActorMobility::isValidMovementMode(*soldier, value.movementMode))
					return CommandDisposition::Discard;
				STRUCTURE* structure = ResolveLiveWorldObject(value.object);
				if (!structure) return CommandDisposition::Discard;

				soldier->movement().mode() = value.movementMode;
				soldier->movement().setReverse(value.reverse);
				soldier->pendingAction().clearAction();
				if (!TacticalActorRouteExecution::requestPath(*soldier,
						value.destinationGrid, value.movementMode,
						TacticalActorRouteExecution::PathOrigin::PlayerUi,
						value.forceRestart))
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
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				TacticalActor* target = ResolveLiveCommandActor(value.target);
				if (!soldier || !target ||
					!IsValidConversationPair(*soldier, *target))
					return CommandDisposition::Discard;
				(void)TacticalActorInteractions::startConversation(
					*soldier,
					*target,
					false);
				return CommandDisposition::Applied;
			}
			else if constexpr (
				std::is_same<Command, ApproachConversationCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				TacticalActor* target = ResolveLiveCommandActor(value.target);
				if (!soldier || !target ||
					!IsValidConversationPair(*soldier, *target) ||
					!TacticalActorMobility::isValidMovementMode(*soldier, value.movementMode))
					return CommandDisposition::Discard;

				const UINT8 previousAction = soldier->pendingAction().action();
				const UINT32 previousData1 =
					soldier->pendingAction().primaryData();
				const INT32 previousData2 =
					soldier->pendingAction().secondaryData();
				const INT8 previousData3 =
					soldier->pendingAction().tertiaryData();
				const UINT32 previousData4 =
					soldier->pendingAction().quaternaryData();
				const UINT32 previousTargetIncarnation =
					soldier->runtime().pendingAction.targetIncarnation;
				const UINT8 previousAnimCount =
					soldier->pendingAction().animationCount();
				const UINT16 previousMovementMode =
					soldier->movement().mode();

				soldier->movement().mode() = value.movementMode;
				soldier->pendingAction().begin(MERC_TALK);
				soldier->pendingAction().primaryData() = value.target.slot;
				soldier->pendingAction().secondaryData() = 0;
				soldier->pendingAction().tertiaryData() = 0;
				soldier->pendingAction().quaternaryData() = 0;
				soldier->runtime().pendingAction.targetIncarnation =
					value.target.incarnation;
				soldier->pendingAction().resetAnimationCount();
				if (TacticalActorRouteExecution::requestPath(*soldier,
						value.destinationGrid, value.movementMode,
						TacticalActorRouteExecution::PathOrigin::PlayerUi,
						value.forceRestart))
					return CommandDisposition::Applied;

				soldier->pendingAction().action() = previousAction;
				soldier->pendingAction().primaryData() = previousData1;
				soldier->pendingAction().secondaryData() = previousData2;
				soldier->pendingAction().tertiaryData() = previousData3;
				soldier->pendingAction().quaternaryData() = previousData4;
				soldier->runtime().pendingAction.targetIncarnation =
					previousTargetIncarnation;
				soldier->pendingAction().animationCount() =
					previousAnimCount;
				soldier->movement().mode() = previousMovementMode;
				return CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, EnterVehicleCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				TacticalActor* vehicle = ResolveLiveCommandActor(value.vehicle);
				if (!soldier || !vehicle ||
					!CanEnterCommandVehicle(
						*soldier, *vehicle, value.seatIndex))
					return CommandDisposition::Discard;
				const BOOLEAN entered =
					EnterVehicle(vehicle, soldier, value.seatIndex);
				UnSetUIBusy(soldier->identity().id());
				return entered
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, ApproachVehicleCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				TacticalActor* vehicle = ResolveLiveCommandActor(value.vehicle);
				if (!soldier || !vehicle ||
					!CanEnterCommandVehicle(
						*soldier, *vehicle, value.seatIndex) ||
					!TacticalActorMobility::isValidMovementMode(*soldier, value.movementMode))
					return CommandDisposition::Discard;

				const UINT8 previousAction = soldier->pendingAction().action();
				const UINT32 previousData1 =
					soldier->pendingAction().primaryData();
				const INT32 previousData2 =
					soldier->pendingAction().secondaryData();
				const INT8 previousData3 =
					soldier->pendingAction().tertiaryData();
				const UINT32 previousData4 =
					soldier->pendingAction().quaternaryData();
				const UINT32 previousTargetIncarnation =
					soldier->runtime().pendingAction.targetIncarnation;
				const UINT8 previousAnimCount =
					soldier->pendingAction().animationCount();
				const UINT16 previousMovementMode =
					soldier->movement().mode();

				soldier->movement().mode() = value.movementMode;
				soldier->pendingAction().begin(MERC_ENTER_VEHICLE);
				soldier->pendingAction().primaryData() = 0;
				soldier->runtime().pendingAction.targetIncarnation =
					value.vehicle.incarnation;
				// The old field held a grid. All production scheduling now
				// stores the vehicle slot so completion can resolve the exact
				// incarnation even if the target moves.
				soldier->pendingAction().secondaryData() = value.vehicle.slot;
				soldier->pendingAction().tertiaryData() = value.direction;
				soldier->pendingAction().quaternaryData() = value.seatIndex;
				soldier->pendingAction().resetAnimationCount();
				if (TacticalActorRouteExecution::requestPath(*soldier,
						value.destinationGrid, value.movementMode,
						TacticalActorRouteExecution::PathOrigin::TeamAwareUi,
						value.forceRestart))
					return CommandDisposition::Applied;

				soldier->pendingAction().action() = previousAction;
				soldier->pendingAction().primaryData() = previousData1;
				soldier->pendingAction().secondaryData() = previousData2;
				soldier->pendingAction().tertiaryData() = previousData3;
				soldier->pendingAction().quaternaryData() = previousData4;
				soldier->runtime().pendingAction.targetIncarnation =
					previousTargetIncarnation;
				soldier->pendingAction().animationCount() =
					previousAnimCount;
				soldier->movement().mode() = previousMovementMode;
				return CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, PickupWorldItemCommand>::value)
			{
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				if (!soldier) return CommandDisposition::Discard;

				INT32 itemIndex = NOTHING;
				std::uint32_t targetIncarnation = 0;
				if (value.kind ==
					TacticalWorldItemPickupKind::SpecificItem)
				{
					WORLDITEM* item =
						ResolveJa2TacticalWorldItem(value.item);
					if (!item || item->sGridNo != value.grid ||
						item->ubLevel != soldier->position().level() ||
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
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				TacticalActor* target = ResolveLiveCommandActor(value.target);
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
				TacticalActor* soldier = ResolveLiveCommandActor(value.soldier);
				TacticalActor* target = ResolveLiveCommandActor(value.target);
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
				if (TacticalActor* soldier =
					ResolveLiveCommandActor(value.soldier))
					(void)SynchronizeJa2TacticalEntityState(*soldier);

				if constexpr (
					std::is_same<Command, StartConversationCommand>::value ||
					std::is_same<Command, ApproachConversationCommand>::value ||
					std::is_same<Command, StealFromActorCommand>::value ||
					std::is_same<Command, ExchangePositionsCommand>::value)
				{
					if (TacticalActor* target =
						ResolveLiveCommandActor(value.target))
						(void)SynchronizeJa2TacticalEntityState(*target);
				}
				else if constexpr (
					std::is_same<Command, EnterVehicleCommand>::value ||
					std::is_same<Command, ApproachVehicleCommand>::value)
				{
					if (TacticalActor* vehicle =
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
	TacticalEntityId actor, std::uint8_t stance) noexcept
{
	return DispatchNetworkActorCommand(
		actor, [stance](TacticalEntityId actor) {
			return ChangeStanceCommand{
				actor, stance, SimulationCommandSource::NetworkPeer,
				TacticalEventPolicy::LocalOnly};
		});
}

SimulationCommandDispatchResult TryDispatchNetworkSetFacingCommand(
	TacticalEntityId actor, std::uint8_t direction) noexcept
{
	return DispatchNetworkActorCommand(
		actor, [direction](TacticalEntityId actor) {
			return SetFacingCommand{
				actor, direction, SimulationCommandSource::NetworkPeer,
				TacticalEventPolicy::LocalOnly};
		});
}

SimulationCommandDispatchResult TryDispatchNetworkActorPathCommand(
	TacticalEntityId actor,
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
		actor,
		[reportedGrid, destinationGrid, movementState, currentPathIndex,
			pathSize, captured](TacticalEntityId actor) {
			return SynchronizeActorPathCommand{
				actor, reportedGrid, destinationGrid, movementState,
				currentPathIndex, pathSize, captured,
				SimulationCommandSource::NetworkPeer};
		});
}

SimulationCommandDispatchResult TryDispatchNetworkActorFireCommand(
	TacticalEntityId actor,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	std::uint32_t attackingWeapon) noexcept
{
	return DispatchNetworkActorCommand(
		actor,
		[targetGrid, targetLevel, targetCubeLevel, attackingWeapon](
			TacticalEntityId actor) {
			return SynchronizeActorFireCommand{
				actor, targetGrid, targetLevel, targetCubeLevel,
				attackingWeapon, SimulationCommandSource::NetworkPeer};
		});
}

SimulationCommandDispatchResult TryDispatchNetworkActorStopCommand(
	TacticalEntityId actor,
	std::int32_t reportedGrid,
	std::int16_t positionX,
	std::int16_t positionY,
	std::uint8_t direction,
	bool stop) noexcept
{
	return DispatchNetworkActorCommand(
		actor,
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
	TacticalEntityId actor,
	std::uint8_t stance,
	TacticalEventPolicy eventPolicy) noexcept
{
	return DispatchSystemActorCommand(
		actor, [stance, eventPolicy](TacticalEntityId actor) {
			return ChangeStanceCommand{
				actor, stance, SimulationCommandSource::System, eventPolicy};
		});
}

SimulationCommandDispatchResult TryDispatchSystemSetFacingCommand(
	TacticalEntityId actor,
	std::uint8_t direction,
	TacticalEventPolicy eventPolicy) noexcept
{
	return DispatchSystemActorCommand(
		actor, [direction, eventPolicy](TacticalEntityId actor) {
			return SetFacingCommand{
				actor, direction, SimulationCommandSource::System, eventPolicy};
		});
}

SimulationCommandDispatchResult TryDispatchSystemMoveToGridCommand(
	TacticalEntityId actor,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart) noexcept
{
	return DispatchSystemActorCommand(
		actor,
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
	TacticalEntityId actor,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	std::uint8_t attackingHand,
	std::uint32_t attackingWeapon) noexcept
{
	return DispatchSystemActorCommand(
		actor,
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
	TacticalEntityId actor,
	std::uint8_t stance,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor, [stance, source](TacticalEntityId actor) {
			return ChangeStanceCommand{actor, stance, source};
		});
}

SimulationCommandDispatchResult TryDispatchBeginFireWeaponCommandNow(
	TacticalEntityId actor,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor,
		[targetGrid, targetLevel, targetCubeLevel, source](
			TacticalEntityId actor) {
			return BeginFireWeaponCommand{
				actor, targetGrid, targetLevel, targetCubeLevel, source};
		});
}

SimulationCommandDispatchResult TryDispatchMoveToGridCommandNow(
	TacticalEntityId actor,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor,
		[destinationGrid, movementMode, reverse, forceRestart, source](
			TacticalEntityId actor) {
			return MoveToGridCommand{
				actor, destinationGrid, movementMode,
				reverse, forceRestart, source};
		});
}

SimulationCommandDispatchResult TryDispatchSetFacingCommandNow(
	TacticalEntityId actor,
	std::uint8_t direction,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor, [direction, source](TacticalEntityId actor) {
			return SetFacingCommand{actor, direction, source};
		});
}

SimulationCommandDispatchResult TryDispatchSetStealthModeCommandNow(
	TacticalEntityId actor,
	bool enabled,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor, [enabled, source](TacticalEntityId actor) {
			return SetStealthModeCommand{actor, enabled, source};
		});
}

SimulationCommandDispatchResult TryDispatchStopMovementCommandNow(
	TacticalEntityId actor,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor, [source](TacticalEntityId actor) {
			return StopMovementCommand{actor, source};
		});
}

SimulationCommandDispatchResult TryDispatchCancelDragCommandNow(
	TacticalEntityId actor,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor, [source](TacticalEntityId actor) {
			return CancelDragCommand{actor, source};
		});
}

SimulationCommandDispatchResult TryDispatchCycleWeaponModeCommandNow(
	TacticalEntityId actor,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor, [source](TacticalEntityId actor) {
			return CycleWeaponModeCommand{actor, source};
		});
}

SimulationCommandDispatchResult TryDispatchCycleScopeModeCommandNow(
	TacticalEntityId actor,
	std::int32_t targetGrid,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor, [targetGrid, source](TacticalEntityId actor) {
			return CycleScopeModeCommand{actor, targetGrid, source};
		});
}

SimulationCommandDispatchResult TryDispatchReloadWeaponCommandNow(
	TacticalEntityId actor,
	bool reloadEvenIfNotEmpty,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor, [reloadEvenIfNotEmpty, source](TacticalEntityId actor) {
			return ReloadWeaponCommand{
				actor, reloadEvenIfNotEmpty, source};
		});
}

SimulationCommandDispatchResult TryDispatchSetWeaponReadyCommandNow(
	TacticalEntityId actor,
	std::uint8_t direction,
	bool ready,
	bool alternativeHold,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor,
		[direction, ready, alternativeHold, source](
			TacticalEntityId actor) {
			return SetWeaponReadyCommand{
				actor, direction, ready, alternativeHold, source};
		});
}

SimulationCommandDispatchResult TryDispatchTraverseObstacleCommandNow(
	TacticalEntityId actor,
	TacticalTraversalKind kind,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor, [kind, source](TacticalEntityId actor) {
			return TraverseObstacleCommand{actor, kind, source};
		});
}

SimulationCommandDispatchResult TryDispatchActivateWorldObjectCommandNow(
	TacticalEntityId actor,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor,
		[objectGrid, structureId, direction, source](
			TacticalEntityId actor) {
			return ActivateWorldObjectCommand{
				actor, TacticalWorldObjectId{objectGrid, structureId},
				direction, source};
		});
}

SimulationCommandDispatchResult TryDispatchApproachWorldObjectCommandNow(
	TacticalEntityId actor,
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
		actor,
		[objectGrid, structureId, direction, destinationGrid, movementMode,
		 reverse, forceRestart, source](TacticalEntityId actor) {
			return ApproachWorldObjectCommand{
				actor, TacticalWorldObjectId{objectGrid, structureId},
				direction, destinationGrid, movementMode,
				reverse, forceRestart, source};
		});
}

SimulationCommandDispatchResult TryDispatchStartConversationCommandNow(
	TacticalEntityId actor,
	TacticalEntityId target,
	SimulationCommandSource source) noexcept
{
	return DispatchActorPairCommand(
		actor, target,
		[source](TacticalEntityId commandActor, TacticalEntityId commandTarget) {
			return StartConversationCommand{commandActor, commandTarget, source};
		});
}

SimulationCommandDispatchResult TryDispatchApproachConversationCommandNow(
	TacticalEntityId actor,
	TacticalEntityId target,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool forceRestart,
	SimulationCommandSource source) noexcept
{
	return DispatchActorPairCommand(
		actor, target,
		[destinationGrid, movementMode, forceRestart, source](
			TacticalEntityId actor, TacticalEntityId targetActor) {
			return ApproachConversationCommand{
				actor, targetActor, destinationGrid, movementMode,
				forceRestart, source};
		});
}

SimulationCommandDispatchResult TryDispatchEnterVehicleCommandNow(
	TacticalEntityId actor,
	TacticalEntityId vehicle,
	std::uint8_t direction,
	std::uint8_t seatIndex,
	SimulationCommandSource source) noexcept
{
	return DispatchActorPairCommand(
		actor, vehicle,
		[direction, seatIndex, source](
			TacticalEntityId actor, TacticalEntityId vehicleActor) {
			return EnterVehicleCommand{
				actor, vehicleActor, direction, seatIndex, source};
		});
}

SimulationCommandDispatchResult TryDispatchApproachVehicleCommandNow(
	TacticalEntityId actor,
	TacticalEntityId vehicle,
	std::uint8_t direction,
	std::uint8_t seatIndex,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool forceRestart,
	SimulationCommandSource source) noexcept
{
	return DispatchActorPairCommand(
		actor, vehicle,
		[direction, seatIndex, destinationGrid, movementMode,
		 forceRestart, source](
			TacticalEntityId actor, TacticalEntityId vehicleActor) {
			return ApproachVehicleCommand{
				actor, vehicleActor, direction, seatIndex,
				destinationGrid, movementMode, forceRestart, source};
		});
}

SimulationCommandDispatchResult TryDispatchPickupWorldItemCommandNow(
	TacticalEntityId actor,
	TacticalWorldItemId item,
	std::int32_t grid,
	std::int8_t renderHeight,
	TacticalWorldItemPickupKind kind,
	SimulationCommandSource source) noexcept
{
	return DispatchActorCommand(
		actor, [item, grid, renderHeight, kind, source](
			TacticalEntityId actor) {
			return PickupWorldItemCommand{
				actor, item, grid, renderHeight, kind, source};
		});
}

SimulationCommandDispatchResult TryDispatchStealFromActorCommandNow(
	TacticalEntityId actor,
	TacticalEntityId target,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	SimulationCommandSource source) noexcept
{
	return DispatchActorPairCommand(
		actor, target,
		[targetGrid, targetLevel, source](
			TacticalEntityId actor, TacticalEntityId targetActor) {
			return StealFromActorCommand{
				actor, targetActor, targetGrid, targetLevel, source};
		});
}

SimulationCommandDispatchResult TryDispatchExchangePositionsCommandNow(
	TacticalEntityId actor,
	TacticalEntityId target,
	std::int32_t soldierGrid,
	std::int32_t targetGrid,
	std::int8_t level,
	SimulationCommandSource source) noexcept
{
	return DispatchActorPairCommand(
		actor, target,
		[soldierGrid, targetGrid, level, source](
			TacticalEntityId actor, TacticalEntityId targetActor) {
			return ExchangePositionsCommand{
				actor, targetActor, soldierGrid, targetGrid, level, source};
		});
}

bool TryCompletePendingConversationCommand(TacticalActor& soldier) noexcept
{
	if (soldier.pendingAction().action() != MERC_TALK) return false;
	const UINT32 targetSlot = soldier.pendingAction().primaryData();
	const TacticalEntityId targetId{
		targetSlot < TOTAL_SOLDIERS
			? static_cast<std::uint16_t>(targetSlot)
			: static_cast<std::uint16_t>(TOTAL_SOLDIERS),
		soldier.runtime().pendingAction.targetIncarnation};

	soldier.pendingAction().clearAction();
	soldier.pendingAction().primaryData() = 0;
	soldier.pendingAction().quaternaryData() = 0;
	soldier.runtime().pendingAction.targetIncarnation = 0;

	TacticalActor* target = ResolveLiveCommandActor(targetId);
	if (!target || !IsValidConversationPair(soldier, *target)) return false;
	(void)TacticalActorInteractions::startConversation(
		soldier,
		*target,
		true);
	return true;
}

bool TryCompletePendingVehicleCommand(TacticalActor& soldier) noexcept
{
	if (soldier.pendingAction().action() != MERC_ENTER_VEHICLE) return false;
	const INT32 targetSlot = soldier.pendingAction().secondaryData();
	const INT32 rawDirection = soldier.pendingAction().tertiaryData();
	const UINT32 rawSeatIndex = soldier.pendingAction().quaternaryData();
	const TacticalEntityId vehicleId{
		targetSlot >= 0 && targetSlot < TOTAL_SOLDIERS
			? static_cast<std::uint16_t>(targetSlot)
			: static_cast<std::uint16_t>(TOTAL_SOLDIERS),
		soldier.runtime().pendingAction.targetIncarnation};

	soldier.pendingAction().clearAction();
	soldier.pendingAction().primaryData() = 0;
	soldier.pendingAction().secondaryData() = 0;
	soldier.pendingAction().tertiaryData() = 0;
	soldier.pendingAction().quaternaryData() = 0;
	soldier.runtime().pendingAction.targetIncarnation = 0;

	TacticalActor* vehicle = ResolveLiveCommandActor(vehicleId);
	if (!vehicle || rawDirection < 0 ||
		!IsValidTacticalDirection(static_cast<std::uint8_t>(rawDirection)) ||
		rawSeatIndex >= TacticalMaximumVehicleSeats ||
		!CanEnterCommandVehicle(
			soldier, *vehicle, static_cast<std::uint8_t>(rawSeatIndex)))
	{
		UnSetUIBusy(soldier.identity().id());
		return false;
	}

	const BOOLEAN entered = EnterVehicle(
		vehicle, &soldier, static_cast<std::uint8_t>(rawSeatIndex));
	UnSetUIBusy(soldier.identity().id());
	return entered == TRUE;
}

bool TryCompletePendingStealCommand(TacticalActor& soldier) noexcept
{
	if (soldier.pendingAction().action() != MERC_STEAL) return false;

	TacticalActor* target = nullptr;
	if (soldier.runtime().pendingAction.targetIncarnation != 0)
	{
		target = ResolveStablePendingStealTarget(soldier);
	}
	else
	{
		SoldierID targetId = WhoIsThere2(
			soldier.pendingAction().secondaryData(),
			soldier.targeting().level());
		if (targetId != NOBODY)
		{
			target = GetJa2SoldierRepository().resolve(targetId.i);
		}
	}

	const INT32 rawDirection = soldier.pendingAction().tertiaryData();
	if (!target ||
		target->position().gridNo() != soldier.pendingAction().secondaryData() ||
		target->position().level() != soldier.targeting().level() ||
		soldier.position().level() != target->position().level() ||
		PythSpacesAway(soldier.position().gridNo(), target->position().gridNo()) != 1 ||
		rawDirection < 0 ||
		!IsValidTacticalDirection(
			static_cast<std::uint8_t>(rawDirection)))
	{
		ClearPendingSteal(soldier);
		return false;
	}

	(void)TacticalActorOrientation::setDesiredDirection(soldier,
		static_cast<UINT8>(rawDirection));
	if (gAnimControl[soldier.animationPlayback().state()].ubEndHeight == ANIM_PRONE ||
		gAnimControl[soldier.animationPlayback().state()].ubEndHeight == ANIM_CROUCH ||
		gAnimControl[target->animationPlayback().state()].ubEndHeight == ANIM_PRONE)
	{
		TacticalActorAnimationTransitions::initializeAnimation(soldier,
			STEAL_ITEM_CROUCHED, 0, FALSE);
	}
	else
	{
		TacticalActorAnimationTransitions::initializeAnimation(soldier, STEAL_ITEM, 0, FALSE);
	}
	soldier.pendingAction().clearAction();
	return true;
}

TacticalActor* ResolveAndConsumePendingStealTarget(
	TacticalActor& soldier,
	std::int32_t targetGrid,
	std::int8_t targetLevel) noexcept
{
	TacticalActor* target = nullptr;
	if (soldier.runtime().pendingAction.targetIncarnation != 0)
	{
		target = ResolveStablePendingStealTarget(soldier);
	}
	else
	{
		target = SimpleFindSoldier(targetGrid, targetLevel);
	}

	soldier.pendingAction().primaryData() = 0;
	soldier.pendingAction().secondaryData() = 0;
	soldier.pendingAction().tertiaryData() = 0;
	soldier.pendingAction().quaternaryData() = 0;
	soldier.runtime().pendingAction.targetIncarnation = 0;

	if (!target || target->position().gridNo() != targetGrid ||
		target->position().level() != targetLevel ||
		soldier.position().level() != target->position().level() ||
		PythSpacesAway(soldier.position().gridNo(), target->position().gridNo()) != 1)
	{
		UnSetUIBusy(soldier.identity().id());
		return nullptr;
	}
	return target;
}

bool TryValidatePendingWorldItemPickup(TacticalActor& soldier) noexcept
{
	if (soldier.pendingAction().action() != MERC_PICKUPITEM)
		return true;
	if (PendingWorldItemMatches(
			soldier,
			static_cast<INT32>(soldier.pendingAction().primaryData()),
			static_cast<INT32>(soldier.pendingAction().quaternaryData()),
			soldier.pendingAction().tertiaryData()))
		return true;
	ClearPendingWorldItemPickup(soldier);
	return false;
}

bool TryConsumePendingWorldItemPickup(
	TacticalActor& soldier,
	std::int32_t itemIndex,
	std::int32_t grid,
	std::int8_t level) noexcept
{
	if (soldier.pendingAction().action() != MERC_PICKUPITEM)
	{
		soldier.runtime().pendingAction.targetIncarnation = 0;
		return true;
	}
	if (!PendingWorldItemMatches(soldier, itemIndex, grid, level))
	{
		ClearPendingWorldItemPickup(soldier);
		return false;
	}
	soldier.runtime().pendingAction.targetIncarnation = 0;
	return true;
}

std::uint64_t DispatchEndTurnCommandNow(
	std::uint8_t nextTeam, SimulationCommandSource source)
{
	return TryDispatchEndTurnCommandNow(nextTeam, source).sequence;
}

std::uint64_t DispatchChangeStanceCommandNow(
	TacticalEntityId actor, std::uint8_t stance, SimulationCommandSource source)
{
	return TryDispatchChangeStanceCommandNow(actor, stance, source).sequence;
}

std::uint64_t DispatchBeginFireWeaponCommandNow(
	TacticalEntityId actor,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	SimulationCommandSource source)
{
	return TryDispatchBeginFireWeaponCommandNow(
		actor, targetGrid, targetLevel, targetCubeLevel, source).sequence;
}

std::uint64_t DispatchMoveToGridCommandNow(
	TacticalEntityId actor,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source)
{
	return TryDispatchMoveToGridCommandNow(
		actor, destinationGrid, movementMode,
		reverse, forceRestart, source).sequence;
}
