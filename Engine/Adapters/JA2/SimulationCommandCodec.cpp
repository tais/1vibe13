#include <Engine/Adapters/JA2/SimulationCommandCodec.h>

#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

#include <Engine/Core/BinaryArchive.h>

namespace
{
constexpr std::uint32_t CommandJournalMagic = 0x31434d53u; // "SMC1"
constexpr std::uint32_t MaximumJournalRecords = 1'000'000;
constexpr std::size_t MinimumEncodedRecordBytes = 20;

enum class CommandTag : std::uint8_t
{
	EndTurn = 1,
	ChangeStance = 2,
	BeginFireWeapon = 3,
	MoveToGrid = 4,
	SetFacing = 5,
	SetStealthMode = 6,
	StopMovement = 7,
	CycleWeaponMode = 8,
	CycleScopeMode = 9,
	ReloadWeapon = 10,
	TraverseObstacle = 11,
	ActivateWorldObject = 12,
	ApproachWorldObject = 13,
	StartConversation = 14,
	ApproachConversation = 15,
	EnterVehicle = 16,
	ApproachVehicle = 17,
	PickupWorldItem = 18,
	StealFromActor = 19,
	ExchangePositions = 20,
	SetWeaponReady = 21,
	CancelDrag = 22,
	SynchronizeActorPath = 23,
	SynchronizeActorFire = 24,
	SynchronizeActorStop = 25,
	SynchronizeTurn = 26,
	BeginSelectedFireWeapon = 27,
	BulkReloadWeapons = 28,
	ApplyWeaponConfiguration = 29,
	SystemWorldObjectInteraction = 30,
	SynchronizeActorVitals = 31,
	AimedFirearmAttack = 32,
	AuthoritativeDoorOpenClose = 33,
	PassInterrupt = 34
};

constexpr std::uint8_t MoveReverseFlag = 0x01u;
constexpr std::uint8_t MoveForceRestartFlag = 0x02u;
constexpr std::uint8_t MoveKnownFlags =
	MoveReverseFlag | MoveForceRestartFlag;
constexpr std::uint8_t WeaponReadyFlag = 0x01u;
constexpr std::uint8_t WeaponAlternativeHoldFlag = 0x02u;
constexpr std::uint8_t WeaponReadyKnownFlags =
	WeaponReadyFlag | WeaponAlternativeHoldFlag;
constexpr std::uint8_t StopMovementFlag = 0x01u;
constexpr std::uint8_t SynchronizeTurnEnterCombatFlag = 0x01u;
constexpr std::uint8_t SynchronizeTurnEndClientTurnFlag = 0x02u;
constexpr std::uint8_t SynchronizeTurnKnownFlags =
	SynchronizeTurnEnterCombatFlag | SynchronizeTurnEndClientTurnFlag;
constexpr std::uint8_t WeaponConfigurationGrenadeDelayFlag = 0x01u;
constexpr std::uint8_t WeaponConfigurationResetAutofireFlag = 0x02u;
constexpr std::uint8_t WeaponConfigurationKnownFlags =
	WeaponConfigurationGrenadeDelayFlag |
	WeaponConfigurationResetAutofireFlag;

bool IsValidSource(std::uint8_t value)
{
	return IsValidSimulationCommandSource(
		static_cast<SimulationCommandSource>(value));
}

bool IsValidAuthorityPolicy(std::uint8_t value)
{
	return IsValidTacticalCommandAuthorityPolicy(
		static_cast<TacticalCommandAuthorityPolicy>(value));
}

bool IsValidStatus(std::uint8_t value)
{
	switch (value)
	{
		case 0:
		case 1:
		case 2:
		case 3:
			return true;
	}
	return false;
}

bool IsValidMoveOrigin(std::uint8_t value)
{
	return IsValidTacticalMoveOrigin(static_cast<TacticalMoveOrigin>(value));
}

bool IsValidPendingActionPolicy(std::uint8_t value)
{
	return IsValidTacticalPendingActionPolicy(
		static_cast<TacticalPendingActionPolicy>(value));
}

bool IsValidEventPolicy(std::uint8_t value)
{
	return IsValidTacticalEventPolicy(
		static_cast<TacticalEventPolicy>(value));
}

bool IsValidTraversalKind(std::uint8_t value)
{
	return IsValidTacticalTraversalKind(
		static_cast<TacticalTraversalKind>(value));
}

bool IsValidTraversalOrigin(std::uint8_t value)
{
	return IsValidTacticalTraversalOrigin(
		static_cast<TacticalTraversalOrigin>(value));
}

bool IsValidTraversalContinuation(std::uint8_t value)
{
	return IsValidTacticalTraversalContinuation(
		static_cast<TacticalTraversalContinuation>(value));
}

bool IsValidWorldObjectOperation(std::uint8_t value)
{
	return IsValidTacticalWorldObjectOperation(
		static_cast<TacticalWorldObjectOperation>(value));
}

bool IsValidWorldObjectOrigin(std::uint8_t value)
{
	return IsValidTacticalWorldObjectOrigin(
		static_cast<TacticalWorldObjectOrigin>(value));
}

bool IsValidWorldObjectContinuation(std::uint8_t value)
{
	return IsValidTacticalWorldObjectContinuation(
		static_cast<TacticalWorldObjectContinuation>(value));
}

bool IsValidWeaponConfigurationCause(std::uint8_t value)
{
	return IsValidTacticalWeaponConfigurationCause(
		static_cast<TacticalWeaponConfigurationCause>(value));
}

bool IsValidWeaponConfigurationPostApplyPolicy(std::uint8_t value)
{
	return IsValidTacticalWeaponConfigurationPostApplyPolicy(
		static_cast<TacticalWeaponConfigurationPostApplyPolicy>(value));
}

bool IsValidWeaponConfigurationContinuation(std::uint8_t value)
{
	return IsValidTacticalWeaponConfigurationContinuation(
		static_cast<TacticalWeaponConfigurationContinuation>(value));
}

void WriteI16(BinaryWriter& writer, std::int16_t value)
{
	writer.writeU16(static_cast<std::uint16_t>(value));
}

bool ReadI16(BinaryReader& reader, std::int16_t& value)
{
	std::uint16_t encoded = 0;
	if (!reader.readU16(encoded)) return false;
	value = encoded <= 0x7fffu
		? static_cast<std::int16_t>(encoded)
		: static_cast<std::int16_t>(
			-1 - static_cast<std::int32_t>(0xffffu - encoded));
	return true;
}

void WriteCommand(BinaryWriter& writer, const SimulationCommand& command)
{
	std::visit([&writer](const auto& value) {
		using Command = typename std::decay<decltype(value)>::type;
		if constexpr (std::is_same<Command, EndTurnCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::EndTurn));
			writer.writeU8(value.nextTeam);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
			writer.writeU8(static_cast<std::uint8_t>(value.authority));
		}
		else if constexpr (std::is_same<Command, ChangeStanceCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::ChangeStance));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(value.stance);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
			writer.writeU8(static_cast<std::uint8_t>(value.eventPolicy));
			writer.writeU8(static_cast<std::uint8_t>(value.authority));
		}
		else if constexpr (std::is_same<Command, BeginFireWeaponCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::BeginFireWeapon));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.targetGrid);
			writer.writeI8(value.targetLevel);
			writer.writeI8(value.targetCubeLevel);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, BeginSelectedFireWeaponCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(
					CommandTag::BeginSelectedFireWeapon));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.targetGrid);
			writer.writeI8(value.targetLevel);
			writer.writeI8(value.targetCubeLevel);
			writer.writeU8(value.attackingHand);
			writer.writeU32(value.attackingWeapon);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, AimedFirearmAttackCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::AimedFirearmAttack));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU16(value.target.slot);
			writer.writeU32(value.target.incarnation);
			writer.writeI32(value.expectedTargetGrid);
			writer.writeI8(value.expectedTargetLevel);
			writer.writeU8(value.aimTime);
			writer.writeU32(value.expectedHandItem);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command,
				AuthoritativeDoorOpenCloseCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(
				CommandTag::AuthoritativeDoorOpenClose));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.object.grid);
			writer.writeU16(value.object.structureId);
			writer.writeU8(static_cast<std::uint8_t>(value.operation));
			writer.writeU8(value.direction);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
			writer.writeU8(static_cast<std::uint8_t>(value.authority));
			writer.writeU64(value.expectedWorldGeneration);
			writer.writeU64(value.expectedTurnSerial);
			writer.writeI32(value.expectedActorGrid);
			writer.writeI8(value.expectedActorLevel);
			writer.writeU16(value.expectedAnimationState);
			writer.writeU64(value.expectedActorStateFingerprint);
			writer.writeU64(value.expectedObjectFingerprint);
			WriteI16(writer, value.expectedActionPointCost);
			WriteI16(writer, value.expectedBreathPointCost);
		}
		else if constexpr (
			std::is_same<Command, PassInterruptCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::PassInterrupt));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU64(value.expectedWorldGeneration);
			writer.writeU64(value.expectedInterruptSerial);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
			writer.writeU8(static_cast<std::uint8_t>(value.authority));
		}
		else if constexpr (std::is_same<Command, MoveToGridCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::MoveToGrid));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.destinationGrid);
			writer.writeU16(value.movementMode);
			writer.writeU8(
				(value.reverse ? MoveReverseFlag : 0u) |
				(value.forceRestart ? MoveForceRestartFlag : 0u));
			writer.writeU8(static_cast<std::uint8_t>(value.source));
			writer.writeU8(static_cast<std::uint8_t>(value.origin));
			writer.writeU8(static_cast<std::uint8_t>(value.pendingAction));
			writer.writeU8(static_cast<std::uint8_t>(value.authority));
		}
		else if constexpr (std::is_same<Command, SetFacingCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::SetFacing));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(value.direction);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
			writer.writeU8(static_cast<std::uint8_t>(value.eventPolicy));
			writer.writeU8(static_cast<std::uint8_t>(value.authority));
		}
		else if constexpr (std::is_same<Command, SetStealthModeCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::SetStealthMode));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(value.enabled ? 1u : 0u);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (std::is_same<Command, StopMovementCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::StopMovement));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
			writer.writeU8(static_cast<std::uint8_t>(value.authority));
		}
		else if constexpr (std::is_same<Command, CancelDragCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::CancelDrag));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (std::is_same<Command, CycleWeaponModeCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::CycleWeaponMode));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (std::is_same<Command, CycleScopeModeCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::CycleScopeMode));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.targetGrid);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (std::is_same<Command, ReloadWeaponCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::ReloadWeapon));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(value.reloadEvenIfNotEmpty ? 1u : 0u);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
			writer.writeU8(static_cast<std::uint8_t>(value.authority));
		}
		else if constexpr (
			std::is_same<Command, BulkReloadWeaponsCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::BulkReloadWeapons));
			writer.writeU16(value.soldierCount);
			writer.writeU8(value.squad);
			writer.writeU8(static_cast<std::uint8_t>(value.mode));
			for (std::size_t index = 0; index < value.soldierCount; ++index)
			{
				writer.writeU16(value.soldiers[index].slot);
				writer.writeU32(value.soldiers[index].incarnation);
			}
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, ApplyWeaponConfigurationCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(
					CommandTag::ApplyWeaponConfiguration));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI8(value.result.weaponMode);
			writer.writeI8(value.result.scopeMode);
			writer.writeI8(value.result.burstCounter);
			writer.writeU8(value.result.autofireShots);
			writer.writeU8(value.result.barrelMode);
			writer.writeI8(value.result.shownAimTime);
			writer.writeU8(
				(value.result.grenadeLauncherDelay
					? WeaponConfigurationGrenadeDelayFlag : 0u) |
				(value.result.resetAutofireBulletInitialization
					? WeaponConfigurationResetAutofireFlag : 0u));
			writer.writeU8(static_cast<std::uint8_t>(value.cause));
			writer.writeU8(
				static_cast<std::uint8_t>(value.postApplyPolicy));
			writer.writeU8(
				static_cast<std::uint8_t>(value.continuation));
			writer.writeU8(static_cast<std::uint8_t>(value.eventPolicy));
			writer.writeU16(value.target.slot);
			writer.writeU32(value.target.incarnation);
			writer.writeI32(value.targetGrid);
			writer.writeI8(value.targetLevel);
			writer.writeU32(value.handItem);
			writer.writeU32(value.previousItem);
			writer.writeU32(value.changedItem);
			writer.writeU32(value.inventoryPosition);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (std::is_same<Command, SetWeaponReadyCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::SetWeaponReady));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(value.direction);
			writer.writeU8(
				(value.ready ? WeaponReadyFlag : 0u) |
				(value.alternativeHold ? WeaponAlternativeHoldFlag : 0u));
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (std::is_same<Command, TraverseObstacleCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::TraverseObstacle));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(static_cast<std::uint8_t>(value.kind));
			writer.writeU8(static_cast<std::uint8_t>(value.source));
			writer.writeU8(static_cast<std::uint8_t>(value.origin));
			writer.writeU8(static_cast<std::uint8_t>(value.continuation));
			writer.writeU8(static_cast<std::uint8_t>(value.eventPolicy));
			writer.writeI32(value.expectedGrid);
			writer.writeI32(value.expectedFinalDestination);
			writer.writeI8(value.expectedLevel);
			writer.writeU8(value.expectedDirection);
			writer.writeU16(value.expectedAnimationState);
			writer.writeU16(value.movementAnimationState);
			writer.writeU16(value.expectedPathIndex);
			writer.writeU16(value.expectedPathSize);
			writer.writeU8(value.expectedPathDirection);
			writer.writeU8(value.expectedNextPathDirection);
			writer.writeU64(value.expectedStateFingerprint);
			WriteI16(writer, value.expectedActionPointCost);
			WriteI16(writer, value.expectedBreathPointCost);
		}
		else if constexpr (
			std::is_same<Command, ActivateWorldObjectCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::ActivateWorldObject));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.object.grid);
			writer.writeU16(value.object.structureId);
			writer.writeU8(value.direction);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, ApproachWorldObjectCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::ApproachWorldObject));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.object.grid);
			writer.writeU16(value.object.structureId);
			writer.writeU8(value.direction);
			writer.writeI32(value.destinationGrid);
			writer.writeU16(value.movementMode);
			writer.writeU8(
				(value.reverse ? MoveReverseFlag : 0u) |
				(value.forceRestart ? MoveForceRestartFlag : 0u));
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command,
				SystemWorldObjectInteractionCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(
				CommandTag::SystemWorldObjectInteraction));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.object.grid);
			writer.writeU16(value.object.structureId);
			writer.writeU8(value.direction);
			writer.writeU8(static_cast<std::uint8_t>(value.operation));
			writer.writeU8(static_cast<std::uint8_t>(value.source));
			writer.writeU8(static_cast<std::uint8_t>(value.origin));
			writer.writeU8(static_cast<std::uint8_t>(value.continuation));
			writer.writeU8(static_cast<std::uint8_t>(value.eventPolicy));
			writer.writeU8(value.unlockBeforeInteraction ? 1u : 0u);
			writer.writeI32(value.expectedGrid);
			writer.writeI32(value.expectedDestinationGrid);
			writer.writeI8(value.expectedLevel);
			writer.writeU16(value.expectedAnimationState);
			writer.writeU16(value.movementMode);
			writer.writeU16(value.expectedPathIndex);
			writer.writeU16(value.expectedPathSize);
			writer.writeU8(value.expectedPathDirection);
			writer.writeU64(value.expectedStateFingerprint);
			writer.writeU64(value.expectedObjectFingerprint);
			WriteI16(writer, value.expectedActionPointCost);
			WriteI16(writer, value.expectedBreathPointCost);
		}
		else if constexpr (
			std::is_same<Command, StartConversationCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::StartConversation));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU16(value.target.slot);
			writer.writeU32(value.target.incarnation);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, ApproachConversationCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::ApproachConversation));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU16(value.target.slot);
			writer.writeU32(value.target.incarnation);
			writer.writeI32(value.destinationGrid);
			writer.writeU16(value.movementMode);
			writer.writeU8(value.forceRestart ? 1u : 0u);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, EnterVehicleCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::EnterVehicle));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU16(value.vehicle.slot);
			writer.writeU32(value.vehicle.incarnation);
			writer.writeU8(value.direction);
			writer.writeU8(value.seatIndex);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, ApproachVehicleCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::ApproachVehicle));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU16(value.vehicle.slot);
			writer.writeU32(value.vehicle.incarnation);
			writer.writeU8(value.direction);
			writer.writeU8(value.seatIndex);
			writer.writeI32(value.destinationGrid);
			writer.writeU16(value.movementMode);
			writer.writeU8(value.forceRestart ? 1u : 0u);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, PickupWorldItemCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::PickupWorldItem));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU32(value.item.slot);
			writer.writeU32(value.item.incarnation);
			writer.writeI32(value.grid);
			writer.writeI8(value.renderHeight);
			writer.writeU8(static_cast<std::uint8_t>(value.kind));
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, StealFromActorCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::StealFromActor));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU16(value.target.slot);
			writer.writeU32(value.target.incarnation);
			writer.writeI32(value.targetGrid);
			writer.writeI8(value.targetLevel);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, ExchangePositionsCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::ExchangePositions));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU16(value.target.slot);
			writer.writeU32(value.target.incarnation);
			writer.writeI32(value.soldierGrid);
			writer.writeI32(value.targetGrid);
			writer.writeI8(value.level);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, SynchronizeActorPathCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::SynchronizeActorPath));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.reportedGrid);
			writer.writeI32(value.destinationGrid);
			writer.writeU16(value.movementState);
			writer.writeU16(value.currentPathIndex);
			writer.writeU16(value.pathSize);
			for (const std::uint16_t step : value.path)
				writer.writeU16(step);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, SynchronizeActorFireCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::SynchronizeActorFire));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.targetGrid);
			writer.writeI8(value.targetLevel);
			writer.writeI8(value.targetCubeLevel);
			writer.writeU32(value.attackingWeapon);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, SynchronizeActorStopCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::SynchronizeActorStop));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.reportedGrid);
			WriteI16(writer, value.positionX);
			WriteI16(writer, value.positionY);
			writer.writeU8(value.direction);
			writer.writeU8(value.stop ? StopMovementFlag : 0u);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, SynchronizeActorVitalsCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::SynchronizeActorVitals));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI8(value.health);
			writer.writeI8(value.bleeding);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (
			std::is_same<Command, SynchronizeTurnCommand>::value)
		{
			writer.writeU8(
				static_cast<std::uint8_t>(CommandTag::SynchronizeTurn));
			writer.writeU8(value.nextTeam);
			writer.writeU8(
				(value.enterCombat ? SynchronizeTurnEnterCombatFlag : 0u) |
				(value.endClientTurn
					? SynchronizeTurnEndClientTurnFlag : 0u));
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
	}, command);
}

bool ReadSource(BinaryReader& reader, SimulationCommandSource& source)
{
	std::uint8_t value = 0;
	if (!reader.readU8(value) || !IsValidSource(value)) return false;
	source = static_cast<SimulationCommandSource>(value);
	return true;
}

bool ReadAuthorityPolicy(
	BinaryReader& reader,
	TacticalCommandAuthorityPolicy& policy)
{
	std::uint8_t value = 0;
	if (!reader.readU8(value) || !IsValidAuthorityPolicy(value)) return false;
	policy = static_cast<TacticalCommandAuthorityPolicy>(value);
	return true;
}

bool ReadCommand(BinaryReader& reader, SimulationCommand& command)
{
	std::uint8_t rawTag = 0;
	if (!reader.readU8(rawTag)) return false;
	switch (static_cast<CommandTag>(rawTag))
	{
		case CommandTag::EndTurn:
		{
			EndTurnCommand value{};
			if (!reader.readU8(value.nextTeam) ||
				!ReadSource(reader, value.source) ||
				!ReadAuthorityPolicy(reader, value.authority))
				return false;
			command = value;
			return true;
		}
		case CommandTag::ChangeStance:
		{
			ChangeStanceCommand value{};
			std::uint8_t eventPolicy = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid()) return false;
			if (!reader.readU8(value.stance) ||
				!ReadSource(reader, value.source) ||
				!reader.readU8(eventPolicy) ||
				!IsValidEventPolicy(eventPolicy) ||
				!ReadAuthorityPolicy(reader, value.authority))
				return false;
			value.eventPolicy =
				static_cast<TacticalEventPolicy>(eventPolicy);
			command = value;
			return true;
		}
		case CommandTag::BeginFireWeapon:
		{
			BeginFireWeaponCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI32(value.targetGrid) ||
				!reader.readI8(value.targetLevel) ||
				!reader.readI8(value.targetCubeLevel) ||
				!ReadSource(reader, value.source)) return false;
			command = value;
			return true;
		}
		case CommandTag::BeginSelectedFireWeapon:
		{
			BeginSelectedFireWeaponCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI32(value.targetGrid) ||
				!reader.readI8(value.targetLevel) ||
				!reader.readI8(value.targetCubeLevel) ||
				!reader.readU8(value.attackingHand) ||
				!reader.readU32(value.attackingWeapon) ||
				!ReadSource(reader, value.source) ||
				!IsSimulationSystemSource(value.source))
				return false;
			command = value;
			return true;
		}
		case CommandTag::AimedFirearmAttack:
		{
			AimedFirearmAttackCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!reader.readU16(value.target.slot) ||
				!reader.readU32(value.target.incarnation) ||
				!reader.readI32(value.expectedTargetGrid) ||
				!reader.readI8(value.expectedTargetLevel) ||
				!reader.readU8(value.aimTime) ||
				!reader.readU32(value.expectedHandItem) ||
				!ReadSource(reader, value.source) ||
				!IsStructurallyValidSimulationCommand(
					SimulationCommand{value}))
				return false;
			command = value;
			return true;
		}
		case CommandTag::AuthoritativeDoorOpenClose:
		{
			AuthoritativeDoorOpenCloseCommand value{};
			std::uint8_t operation = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!reader.readI32(value.object.grid) ||
				!reader.readU16(value.object.structureId) ||
				!reader.readU8(operation) ||
				!IsValidWorldObjectOperation(operation) ||
				!reader.readU8(value.direction) ||
				!IsValidTacticalDirection(value.direction) ||
				!ReadSource(reader, value.source) ||
				!ReadAuthorityPolicy(reader, value.authority) ||
				!reader.readU64(value.expectedWorldGeneration) ||
				!reader.readU64(value.expectedTurnSerial) ||
				!reader.readI32(value.expectedActorGrid) ||
				!reader.readI8(value.expectedActorLevel) ||
				!reader.readU16(value.expectedAnimationState) ||
				!reader.readU64(value.expectedActorStateFingerprint) ||
				!reader.readU64(value.expectedObjectFingerprint) ||
				!ReadI16(reader, value.expectedActionPointCost) ||
				!ReadI16(reader, value.expectedBreathPointCost))
				return false;
			value.operation =
				static_cast<TacticalWorldObjectOperation>(operation);
			if (!IsStructurallyValidSimulationCommand(
					SimulationCommand{value}))
				return false;
			command = value;
			return true;
		}
		case CommandTag::PassInterrupt:
		{
			PassInterruptCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!reader.readU64(value.expectedWorldGeneration) ||
				!reader.readU64(value.expectedInterruptSerial) ||
				!ReadSource(reader, value.source) ||
				!ReadAuthorityPolicy(reader, value.authority) ||
				!IsStructurallyValidPassInterruptCommand(value))
				return false;
			command = value;
			return true;
		}
		case CommandTag::MoveToGrid:
		{
			MoveToGridCommand value{};
			std::uint8_t flags = 0;
			std::uint8_t origin = 0;
			std::uint8_t pendingAction = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI32(value.destinationGrid) ||
				!reader.readU16(value.movementMode) ||
				!reader.readU8(flags) || (flags & ~MoveKnownFlags) != 0 ||
				!ReadSource(reader, value.source) ||
				!reader.readU8(origin) || !IsValidMoveOrigin(origin) ||
				!reader.readU8(pendingAction) ||
				!IsValidPendingActionPolicy(pendingAction) ||
				!ReadAuthorityPolicy(reader, value.authority)) return false;
			value.reverse = (flags & MoveReverseFlag) != 0;
			value.forceRestart = (flags & MoveForceRestartFlag) != 0;
			value.origin = static_cast<TacticalMoveOrigin>(origin);
			value.pendingAction =
				static_cast<TacticalPendingActionPolicy>(pendingAction);
			command = value;
			return true;
		}
		case CommandTag::SetFacing:
		{
			SetFacingCommand value{};
			std::uint8_t eventPolicy = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readU8(value.direction) ||
				!IsValidTacticalDirection(value.direction) ||
				!ReadSource(reader, value.source) ||
				!reader.readU8(eventPolicy) ||
				!IsValidEventPolicy(eventPolicy) ||
				!ReadAuthorityPolicy(reader, value.authority)) return false;
			value.eventPolicy =
				static_cast<TacticalEventPolicy>(eventPolicy);
			command = value;
			return true;
		}
		case CommandTag::SetStealthMode:
		{
			SetStealthModeCommand value{};
			std::uint8_t enabled = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() || !reader.readU8(enabled) || enabled > 1 ||
				!ReadSource(reader, value.source)) return false;
			value.enabled = enabled != 0;
			command = value;
			return true;
		}
		case CommandTag::StopMovement:
		{
			StopMovementCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() || !ReadSource(reader, value.source) ||
				!ReadAuthorityPolicy(reader, value.authority))
				return false;
			command = value;
			return true;
		}
		case CommandTag::CancelDrag:
		{
			CancelDragCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() || !ReadSource(reader, value.source))
				return false;
			command = value;
			return true;
		}
		case CommandTag::CycleWeaponMode:
		{
			CycleWeaponModeCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() || !ReadSource(reader, value.source))
				return false;
			command = value;
			return true;
		}
		case CommandTag::CycleScopeMode:
		{
			CycleScopeModeCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI32(value.targetGrid) ||
				!ReadSource(reader, value.source)) return false;
			command = value;
			return true;
		}
		case CommandTag::ReloadWeapon:
		{
			ReloadWeaponCommand value{};
			std::uint8_t reloadEvenIfNotEmpty = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readU8(reloadEvenIfNotEmpty) ||
				reloadEvenIfNotEmpty > 1 ||
				!ReadSource(reader, value.source) ||
				!ReadAuthorityPolicy(reader, value.authority)) return false;
			value.reloadEvenIfNotEmpty = reloadEvenIfNotEmpty != 0;
			command = value;
			return true;
		}
		case CommandTag::BulkReloadWeapons:
		{
			BulkReloadWeaponsCommand value{};
			std::uint8_t mode = 0;
			if (!reader.readU16(value.soldierCount) ||
				value.soldierCount == 0 ||
				value.soldierCount > TacticalBulkReloadActorCapacity ||
				!reader.readU8(value.squad) || !reader.readU8(mode) ||
				!IsValidTacticalBulkReloadMode(
					static_cast<TacticalBulkReloadMode>(mode)))
				return false;
			value.mode = static_cast<TacticalBulkReloadMode>(mode);
			for (std::size_t index = 0; index < value.soldierCount; ++index)
			{
				if (!reader.readU16(value.soldiers[index].slot) ||
					!reader.readU32(value.soldiers[index].incarnation) ||
					!value.soldiers[index].valid() ||
					(index != 0 &&
						value.soldiers[index - 1].slot >=
							value.soldiers[index].slot))
					return false;
			}
			if (!ReadSource(reader, value.source) ||
				(value.source != SimulationCommandSource::LocalPlayer &&
					value.source != SimulationCommandSource::Replay))
				return false;
			command = value;
			return true;
		}
		case CommandTag::ApplyWeaponConfiguration:
		{
			ApplyWeaponConfigurationCommand value{};
			std::uint8_t flags = 0;
			std::uint8_t cause = 0;
			std::uint8_t postApplyPolicy = 0;
			std::uint8_t continuation = 0;
			std::uint8_t eventPolicy = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI8(value.result.weaponMode) ||
				!reader.readI8(value.result.scopeMode) ||
				!reader.readI8(value.result.burstCounter) ||
				!reader.readU8(value.result.autofireShots) ||
				!reader.readU8(value.result.barrelMode) ||
				!reader.readI8(value.result.shownAimTime) ||
				!reader.readU8(flags) ||
				(flags & ~WeaponConfigurationKnownFlags) != 0 ||
				!reader.readU8(cause) ||
				!IsValidWeaponConfigurationCause(cause) ||
				!reader.readU8(postApplyPolicy) ||
				!IsValidWeaponConfigurationPostApplyPolicy(
					postApplyPolicy) ||
				!reader.readU8(continuation) ||
				!IsValidWeaponConfigurationContinuation(continuation) ||
				!reader.readU8(eventPolicy) ||
				!IsValidEventPolicy(eventPolicy) ||
				!reader.readU16(value.target.slot) ||
				!reader.readU32(value.target.incarnation) ||
				!reader.readI32(value.targetGrid) ||
				!reader.readI8(value.targetLevel) ||
				!reader.readU32(value.handItem) ||
				!reader.readU32(value.previousItem) ||
				!reader.readU32(value.changedItem) ||
				!reader.readU32(value.inventoryPosition) ||
				!ReadSource(reader, value.source) ||
				!IsSimulationSystemSource(value.source))
				return false;
			value.result.grenadeLauncherDelay =
				(flags & WeaponConfigurationGrenadeDelayFlag) != 0;
			value.result.resetAutofireBulletInitialization =
				(flags & WeaponConfigurationResetAutofireFlag) != 0;
			value.cause =
				static_cast<TacticalWeaponConfigurationCause>(cause);
			value.postApplyPolicy =
				static_cast<TacticalWeaponConfigurationPostApplyPolicy>(
					postApplyPolicy);
			value.continuation =
				static_cast<TacticalWeaponConfigurationContinuation>(
					continuation);
			value.eventPolicy =
				static_cast<TacticalEventPolicy>(eventPolicy);
			if (!IsStructurallyValidSimulationCommand(
					SimulationCommand{value}))
				return false;
			command = value;
			return true;
		}
		case CommandTag::SetWeaponReady:
		{
			SetWeaponReadyCommand value{};
			std::uint8_t flags = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readU8(value.direction) ||
				!IsValidTacticalDirection(value.direction) ||
				!reader.readU8(flags) ||
				(flags & ~WeaponReadyKnownFlags) != 0 ||
				!ReadSource(reader, value.source)) return false;
			value.ready = (flags & WeaponReadyFlag) != 0;
			value.alternativeHold =
				(flags & WeaponAlternativeHoldFlag) != 0;
			command = value;
			return true;
		}
		case CommandTag::TraverseObstacle:
		{
			TraverseObstacleCommand value{};
			std::uint8_t kind = 0;
			std::uint8_t origin = 0;
			std::uint8_t continuation = 0;
			std::uint8_t eventPolicy = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readU8(kind) || !IsValidTraversalKind(kind) ||
				!ReadSource(reader, value.source) ||
				!reader.readU8(origin) || !IsValidTraversalOrigin(origin) ||
				!reader.readU8(continuation) ||
				!IsValidTraversalContinuation(continuation) ||
				!reader.readU8(eventPolicy) ||
				!IsValidEventPolicy(eventPolicy) ||
				!reader.readI32(value.expectedGrid) ||
				!reader.readI32(value.expectedFinalDestination) ||
				!reader.readI8(value.expectedLevel) ||
				!reader.readU8(value.expectedDirection) ||
				!reader.readU16(value.expectedAnimationState) ||
				!reader.readU16(value.movementAnimationState) ||
				!reader.readU16(value.expectedPathIndex) ||
				!reader.readU16(value.expectedPathSize) ||
				!reader.readU8(value.expectedPathDirection) ||
				!reader.readU8(value.expectedNextPathDirection) ||
				!reader.readU64(value.expectedStateFingerprint) ||
				!ReadI16(reader, value.expectedActionPointCost) ||
				!ReadI16(reader, value.expectedBreathPointCost)) return false;
			value.kind = static_cast<TacticalTraversalKind>(kind);
			value.origin = static_cast<TacticalTraversalOrigin>(origin);
			value.continuation =
				static_cast<TacticalTraversalContinuation>(continuation);
			value.eventPolicy =
				static_cast<TacticalEventPolicy>(eventPolicy);
			if (!IsStructurallyValidSimulationCommand(
					SimulationCommand{value})) return false;
			command = value;
			return true;
		}
		case CommandTag::ActivateWorldObject:
		{
			ActivateWorldObjectCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI32(value.object.grid) ||
				!reader.readU16(value.object.structureId) ||
				!reader.readU8(value.direction) ||
				!IsValidTacticalDirection(value.direction) ||
				!ReadSource(reader, value.source)) return false;
			command = value;
			return true;
		}
		case CommandTag::ApproachWorldObject:
		{
			ApproachWorldObjectCommand value{};
			std::uint8_t flags = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI32(value.object.grid) ||
				!reader.readU16(value.object.structureId) ||
				!reader.readU8(value.direction) ||
				!IsValidTacticalDirection(value.direction) ||
				!reader.readI32(value.destinationGrid) ||
				!reader.readU16(value.movementMode) ||
				!reader.readU8(flags) || (flags & ~MoveKnownFlags) != 0 ||
				!ReadSource(reader, value.source)) return false;
			value.reverse = (flags & MoveReverseFlag) != 0;
			value.forceRestart = (flags & MoveForceRestartFlag) != 0;
			command = value;
			return true;
		}
		case CommandTag::SystemWorldObjectInteraction:
		{
			SystemWorldObjectInteractionCommand value{};
			std::uint8_t operation = 0;
			std::uint8_t origin = 0;
			std::uint8_t continuation = 0;
			std::uint8_t eventPolicy = 0;
			std::uint8_t unlockBeforeInteraction = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI32(value.object.grid) ||
				!reader.readU16(value.object.structureId) ||
				!reader.readU8(value.direction) ||
				!IsValidTacticalDirection(value.direction) ||
				!reader.readU8(operation) ||
				!IsValidWorldObjectOperation(operation) ||
				!ReadSource(reader, value.source) ||
				!reader.readU8(origin) ||
				!IsValidWorldObjectOrigin(origin) ||
				!reader.readU8(continuation) ||
				!IsValidWorldObjectContinuation(continuation) ||
				!reader.readU8(eventPolicy) ||
				!IsValidEventPolicy(eventPolicy) ||
				!reader.readU8(unlockBeforeInteraction) ||
				unlockBeforeInteraction > 1 ||
				!reader.readI32(value.expectedGrid) ||
				!reader.readI32(value.expectedDestinationGrid) ||
				!reader.readI8(value.expectedLevel) ||
				!reader.readU16(value.expectedAnimationState) ||
				!reader.readU16(value.movementMode) ||
				!reader.readU16(value.expectedPathIndex) ||
				!reader.readU16(value.expectedPathSize) ||
				!reader.readU8(value.expectedPathDirection) ||
				!reader.readU64(value.expectedStateFingerprint) ||
				!reader.readU64(value.expectedObjectFingerprint) ||
				!ReadI16(reader, value.expectedActionPointCost) ||
				!ReadI16(reader, value.expectedBreathPointCost)) return false;
			value.operation =
				static_cast<TacticalWorldObjectOperation>(operation);
			value.origin = static_cast<TacticalWorldObjectOrigin>(origin);
			value.continuation =
				static_cast<TacticalWorldObjectContinuation>(continuation);
			value.eventPolicy =
				static_cast<TacticalEventPolicy>(eventPolicy);
			value.unlockBeforeInteraction =
				unlockBeforeInteraction != 0;
			if (!IsStructurallyValidSimulationCommand(
					SimulationCommand{value})) return false;
			command = value;
			return true;
		}
		case CommandTag::StartConversation:
		{
			StartConversationCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readU16(value.target.slot) ||
				!reader.readU32(value.target.incarnation) ||
				!value.target.valid() || value.target == value.soldier ||
				!ReadSource(reader, value.source)) return false;
			command = value;
			return true;
		}
		case CommandTag::ApproachConversation:
		{
			ApproachConversationCommand value{};
			std::uint8_t forceRestart = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readU16(value.target.slot) ||
				!reader.readU32(value.target.incarnation) ||
				!value.target.valid() || value.target == value.soldier ||
				!reader.readI32(value.destinationGrid) ||
				!reader.readU16(value.movementMode) ||
				!reader.readU8(forceRestart) || forceRestart > 1 ||
				!ReadSource(reader, value.source)) return false;
			value.forceRestart = forceRestart != 0;
			command = value;
			return true;
		}
		case CommandTag::EnterVehicle:
		{
			EnterVehicleCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readU16(value.vehicle.slot) ||
				!reader.readU32(value.vehicle.incarnation) ||
				!value.vehicle.valid() || value.vehicle == value.soldier ||
				!reader.readU8(value.direction) ||
				!IsValidTacticalDirection(value.direction) ||
				!reader.readU8(value.seatIndex) ||
				value.seatIndex >= TacticalMaximumVehicleSeats ||
				!ReadSource(reader, value.source)) return false;
			command = value;
			return true;
		}
		case CommandTag::ApproachVehicle:
		{
			ApproachVehicleCommand value{};
			std::uint8_t forceRestart = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readU16(value.vehicle.slot) ||
				!reader.readU32(value.vehicle.incarnation) ||
				!value.vehicle.valid() || value.vehicle == value.soldier ||
				!reader.readU8(value.direction) ||
				!IsValidTacticalDirection(value.direction) ||
				!reader.readU8(value.seatIndex) ||
				value.seatIndex >= TacticalMaximumVehicleSeats ||
				!reader.readI32(value.destinationGrid) ||
				!reader.readU16(value.movementMode) ||
				!reader.readU8(forceRestart) || forceRestart > 1 ||
				!ReadSource(reader, value.source)) return false;
			value.forceRestart = forceRestart != 0;
			command = value;
			return true;
		}
		case CommandTag::PickupWorldItem:
		{
			PickupWorldItemCommand value{};
			std::uint8_t kind = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readU32(value.item.slot) ||
				!reader.readU32(value.item.incarnation) ||
				!reader.readI32(value.grid) ||
				!reader.readI8(value.renderHeight) ||
				!reader.readU8(kind) ||
				!IsValidTacticalWorldItemPickupKind(
					static_cast<TacticalWorldItemPickupKind>(kind)) ||
				!ReadSource(reader, value.source))
				return false;
			value.kind = static_cast<TacticalWorldItemPickupKind>(kind);
			if (value.kind ==
					TacticalWorldItemPickupKind::SpecificItem
				? !value.item.valid() ||
					value.item.slot > TacticalMaximumWorldItemSlot
				: value.item != TacticalWorldItemId{})
				return false;
			command = value;
			return true;
		}
		case CommandTag::StealFromActor:
		{
			StealFromActorCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readU16(value.target.slot) ||
				!reader.readU32(value.target.incarnation) ||
				!value.target.valid() || value.target == value.soldier ||
				!reader.readI32(value.targetGrid) ||
				!reader.readI8(value.targetLevel) ||
				!ReadSource(reader, value.source))
				return false;
			command = value;
			return true;
		}
		case CommandTag::ExchangePositions:
		{
			ExchangePositionsCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readU16(value.target.slot) ||
				!reader.readU32(value.target.incarnation) ||
				!value.target.valid() || value.target == value.soldier ||
				!reader.readI32(value.soldierGrid) ||
				!reader.readI32(value.targetGrid) ||
				!reader.readI8(value.level) ||
				!ReadSource(reader, value.source))
				return false;
			command = value;
			return true;
		}
		case CommandTag::SynchronizeActorPath:
		{
			SynchronizeActorPathCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI32(value.reportedGrid) ||
				!reader.readI32(value.destinationGrid) ||
				!reader.readU16(value.movementState) ||
				!reader.readU16(value.currentPathIndex) ||
				!reader.readU16(value.pathSize) ||
				value.pathSize > TacticalReplicatedPathCapacity ||
				value.currentPathIndex > value.pathSize)
				return false;
			for (std::uint16_t& step : value.path)
				if (!reader.readU16(step)) return false;
			if (!ReadSource(reader, value.source) ||
				!IsSimulationSynchronizationSource(value.source))
				return false;
			command = value;
			return true;
		}
		case CommandTag::SynchronizeActorFire:
		{
			SynchronizeActorFireCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI32(value.targetGrid) ||
				!reader.readI8(value.targetLevel) ||
				!reader.readI8(value.targetCubeLevel) ||
				!reader.readU32(value.attackingWeapon) ||
				!ReadSource(reader, value.source) ||
				!IsSimulationSynchronizationSource(value.source))
				return false;
			command = value;
			return true;
		}
		case CommandTag::SynchronizeActorStop:
		{
			SynchronizeActorStopCommand value{};
			std::uint8_t flags = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI32(value.reportedGrid) ||
				!ReadI16(reader, value.positionX) ||
				!ReadI16(reader, value.positionY) ||
				!reader.readU8(value.direction) ||
				!IsValidTacticalDirection(value.direction) ||
				!reader.readU8(flags) ||
				(flags & ~StopMovementFlag) != 0 ||
				!ReadSource(reader, value.source) ||
				!IsSimulationSynchronizationSource(value.source))
				return false;
			value.stop = (flags & StopMovementFlag) != 0;
			command = value;
			return true;
		}
		case CommandTag::SynchronizeActorVitals:
		{
			SynchronizeActorVitalsCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI8(value.health) ||
				!reader.readI8(value.bleeding) ||
				!ReadSource(reader, value.source) ||
				!IsSimulationSynchronizationSource(value.source))
				return false;
			command = value;
			return true;
		}
		case CommandTag::SynchronizeTurn:
		{
			SynchronizeTurnCommand value{};
			std::uint8_t flags = 0;
			if (!reader.readU8(value.nextTeam) ||
				!reader.readU8(flags) ||
				(flags & ~SynchronizeTurnKnownFlags) != 0 ||
				!ReadSource(reader, value.source) ||
				!IsSimulationSynchronizationSource(value.source))
				return false;
			value.enterCombat =
				(flags & SynchronizeTurnEnterCombatFlag) != 0;
			value.endClientTurn =
				(flags & SynchronizeTurnEndClientTurnFlag) != 0;
			command = value;
			return true;
		}
	}
	return false;
}
}

bool EncodeSimulationCommandJournal(
	const std::vector<RecordedSimulationCommand>& records,
	std::uint64_t droppedCount,
	std::vector<std::uint8_t>& bytes)
{
	if (records.size() > MaximumJournalRecords) return false;
	BinaryWriter writer;
	WritePersistenceHeader(
		writer, PersistenceHeader{
			CommandJournalMagic, SimulationCommandJournalWireVersion});
	writer.writeU64(droppedCount);
	writer.writeU32(static_cast<std::uint32_t>(records.size()));
	for (const RecordedSimulationCommand& record : records)
	{
		if (!IsValidStatus(static_cast<std::uint8_t>(record.status)) ||
			!IsStructurallyValidSimulationCommand(record.command))
		{
			return false;
		}
		writer.writeU64(record.tick);
		writer.writeU64(record.sequence);
		writer.writeU8(static_cast<std::uint8_t>(record.status));
		WriteCommand(writer, record.command);
	}
	bytes = writer.take();
	return true;
}

SimulationCommandJournalDecodeResult DecodeSimulationCommandJournal(
	const std::vector<std::uint8_t>& bytes,
	std::vector<RecordedSimulationCommand>& records,
	std::uint64_t& droppedCount)
{
	BinaryReader reader(bytes);
	PersistenceHeader header{};
	if (!reader.readU32(header.magic) || !reader.readU16(header.version) ||
		header.magic != CommandJournalMagic)
		return SimulationCommandJournalDecodeResult::Invalid;
	if (header.version != SimulationCommandJournalWireVersion)
		return SimulationCommandJournalDecodeResult::UnsupportedVersion;

	std::uint64_t decodedDroppedCount = 0;
	std::uint32_t count = 0;
	if (!reader.readU64(decodedDroppedCount) || !reader.readU32(count))
		return SimulationCommandJournalDecodeResult::Invalid;
	if (count > MaximumJournalRecords)
		return SimulationCommandJournalDecodeResult::TooManyRecords;
	if (count > reader.remaining() / MinimumEncodedRecordBytes)
		return SimulationCommandJournalDecodeResult::Invalid;

	std::vector<RecordedSimulationCommand> decoded;
	decoded.reserve(count);
	for (std::uint32_t index = 0; index < count; ++index)
	{
		RecordedSimulationCommand record{};
		std::uint8_t status = 0;
		if (!reader.readU64(record.tick) || !reader.readU64(record.sequence) ||
			!reader.readU8(status) || !IsValidStatus(status) ||
			!ReadCommand(reader, record.command) ||
			!IsStructurallyValidSimulationCommand(record.command))
			return SimulationCommandJournalDecodeResult::Invalid;
		record.status = static_cast<CommandJournalStatus>(status);
		decoded.push_back(std::move(record));
	}
	if (reader.remaining() != 0)
		return SimulationCommandJournalDecodeResult::Invalid;
	records = std::move(decoded);
	droppedCount = decodedDroppedCount;
	return SimulationCommandJournalDecodeResult::Success;
}
