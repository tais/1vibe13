#ifndef ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_H
#define ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <variant>

#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <Engine/Adapters/JA2/TacticalWorldItem.h>

enum class SimulationCommandSource : std::uint8_t
{
	LocalPlayer,
	NetworkPeer,
	System,
	Replay
};

// Keep source validation beside the public wire vocabulary so package
// ingress, codecs, and application executors cannot silently disagree when a
// new producer is added.
constexpr bool IsValidSimulationCommandSource(
	SimulationCommandSource source) noexcept
{
	switch (source)
	{
		case SimulationCommandSource::LocalPlayer:
		case SimulationCommandSource::NetworkPeer:
		case SimulationCommandSource::System:
		case SimulationCommandSource::Replay:
			return true;
	}
	return false;
}

struct EndTurnCommand
{
	std::uint8_t nextTeam;
	SimulationCommandSource source;
};

// Whether the JA2 compatibility executor should use the established
// multiplayer-aware event wrapper or apply the same local action without
// outbound replication. Keeping this explicit lets AI, dialogue scripts,
// network ingress, and replay preserve their original behavior independently
// from command provenance.
enum class TacticalEventPolicy : std::uint8_t
{
	Replicated,
	LocalOnly
};

constexpr bool IsValidTacticalEventPolicy(
	TacticalEventPolicy policy) noexcept
{
	switch (policy)
	{
		case TacticalEventPolicy::Replicated:
		case TacticalEventPolicy::LocalOnly:
			return true;
	}
	return false;
}

struct ChangeStanceCommand
{
	TacticalEntityId soldier;
	std::uint8_t stance;
	SimulationCommandSource source;
	TacticalEventPolicy eventPolicy = TacticalEventPolicy::Replicated;
};

struct BeginFireWeaponCommand
{
	TacticalEntityId soldier;
	std::int32_t targetGrid;
	std::int8_t targetLevel;
	std::int8_t targetCubeLevel;
	SimulationCommandSource source;
};

// AI item handling selects an attacking hand/weapon before it reaches the
// final fire event. Capture that selection with the target so retained System
// ingress cannot later fire using unrelated mutable actor state.
struct BeginSelectedFireWeaponCommand
{
	TacticalEntityId soldier;
	std::int32_t targetGrid;
	std::int8_t targetLevel;
	std::int8_t targetCubeLevel;
	std::uint8_t attackingHand;
	std::uint32_t attackingWeapon;
	SimulationCommandSource source;
};

// Preserve the legacy fFromUI modes as explicit replay/network vocabulary.
// Values are intentionally identical to TacticalActorRouteExecution's
// established 0/1/2/3 ingress policy.
enum class TacticalMoveOrigin : std::uint8_t
{
	System = 0,
	PlayerUi = 1,
	ContinueMovement = 2,
	TeamAwareUi = 3
};

constexpr bool IsValidTacticalMoveOrigin(TacticalMoveOrigin origin) noexcept
{
	switch (origin)
	{
		case TacticalMoveOrigin::System:
		case TacticalMoveOrigin::PlayerUi:
		case TacticalMoveOrigin::ContinueMovement:
		case TacticalMoveOrigin::TeamAwareUi:
			return true;
	}
	return false;
}

enum class TacticalPendingActionPolicy : std::uint8_t
{
	Preserve,
	Clear
};

constexpr bool IsValidTacticalPendingActionPolicy(
	TacticalPendingActionPolicy policy) noexcept
{
	switch (policy)
	{
		case TacticalPendingActionPolicy::Preserve:
		case TacticalPendingActionPolicy::Clear:
			return true;
	}
	return false;
}

struct MoveToGridCommand
{
	TacticalEntityId soldier;
	std::int32_t destinationGrid;
	std::uint16_t movementMode;
	bool reverse;
	bool forceRestart;
	SimulationCommandSource source;
	// Appended defaults retain source compatibility for existing aggregate
	// initializers and preserve the original synchronous UI behavior.
	TacticalMoveOrigin origin = TacticalMoveOrigin::PlayerUi;
	TacticalPendingActionPolicy pendingAction =
		TacticalPendingActionPolicy::Clear;
};

struct SetFacingCommand
{
	TacticalEntityId soldier;
	std::uint8_t direction;
	SimulationCommandSource source;
	TacticalEventPolicy eventPolicy = TacticalEventPolicy::Replicated;
};

inline constexpr std::uint8_t TacticalDirectionCount = 8;

constexpr bool IsValidTacticalDirection(std::uint8_t direction) noexcept
{
	return direction < TacticalDirectionCount;
}

struct SetStealthModeCommand
{
	TacticalEntityId soldier;
	bool enabled;
	SimulationCommandSource source;
};

struct StopMovementCommand
{
	TacticalEntityId soldier;
	SimulationCommandSource source;
};

struct CancelDragCommand
{
	TacticalEntityId soldier;
	SimulationCommandSource source;
};

struct CycleWeaponModeCommand
{
	TacticalEntityId soldier;
	SimulationCommandSource source;
};

// A target grid lets the compatibility executor retain the merc's current aim
// level where possible. -1 is the stable value-only spelling of "no target".
inline constexpr std::int32_t TacticalNoTargetGrid = -1;

struct CycleScopeModeCommand
{
	TacticalEntityId soldier;
	std::int32_t targetGrid;
	SimulationCommandSource source;
};

struct ReloadWeaponCommand
{
	TacticalEntityId soldier;
	bool reloadEvenIfNotEmpty;
	SimulationCommandSource source;
};

// The configurable player-team storage contains at most 254 mercenary and
// six vehicle slots. Bulk reload excludes vehicles, but retaining the complete
// compatibility range keeps the command independent from the current runtime
// team-size option.
inline constexpr std::size_t TacticalBulkReloadActorCapacity = 260;

enum class TacticalBulkReloadMode : std::uint8_t
{
	PeacefulSector = 0,
	HostileTurnBased = 1,
	HostileRealTime = 2
};

constexpr bool IsValidTacticalBulkReloadMode(
	TacticalBulkReloadMode mode) noexcept
{
	switch (mode)
	{
		case TacticalBulkReloadMode::PeacefulSector:
		case TacticalBulkReloadMode::HostileTurnBased:
		case TacticalBulkReloadMode::HostileRealTime:
			return true;
	}
	return false;
}

// Reload-all is one ordered inventory transaction: peaceful sectors first
// consume ground ammunition for every chosen merc and only then refill loose
// magazines. Capturing the complete, slot-ordered squad roster prevents a
// deferred or replayed command from being redirected to reused soldier slots.
struct BulkReloadWeaponsCommand
{
	std::array<TacticalEntityId, TacticalBulkReloadActorCapacity> soldiers{};
	std::uint16_t soldierCount = 0;
	std::uint8_t squad = 0;
	TacticalBulkReloadMode mode = TacticalBulkReloadMode::PeacefulSector;
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer;
};

// Equipment changes and system reactions choose an exact configuration before
// entering the deterministic stream. They are results, not requests to cycle
// mutable player state: a retained command therefore cannot select a different
// mode after another attachment or weapon has changed.
inline constexpr std::int8_t TacticalWeaponModeCount = 10;
inline constexpr std::int8_t TacticalMinimumScopeMode = -1;
inline constexpr std::int8_t TacticalScopeModeCount = 10;
inline constexpr std::uint32_t TacticalNoInventoryPosition =
	std::numeric_limits<std::uint32_t>::max();

struct TacticalWeaponConfigurationResult
{
	std::int8_t weaponMode = 0;
	std::int8_t scopeMode = 0;
	std::int8_t burstCounter = 0;
	std::uint8_t autofireShots = 0;
	std::uint8_t barrelMode = 0;
	std::int8_t shownAimTime = 0;
	bool grenadeLauncherDelay = false;
	bool resetAutofireBulletInitialization = false;
};

constexpr bool operator==(
	const TacticalWeaponConfigurationResult& left,
	const TacticalWeaponConfigurationResult& right) noexcept
{
	return left.weaponMode == right.weaponMode &&
		left.scopeMode == right.scopeMode &&
		left.burstCounter == right.burstCounter &&
		left.autofireShots == right.autofireShots &&
		left.barrelMode == right.barrelMode &&
		left.shownAimTime == right.shownAimTime &&
		left.grenadeLauncherDelay == right.grenadeLauncherDelay &&
		left.resetAutofireBulletInitialization ==
			right.resetAutofireBulletInitialization;
}

constexpr bool operator!=(
	const TacticalWeaponConfigurationResult& left,
	const TacticalWeaponConfigurationResult& right) noexcept
{
	return !(left == right);
}

constexpr bool IsValidTacticalWeaponConfigurationResult(
	const TacticalWeaponConfigurationResult& result) noexcept
{
	if (result.weaponMode < 0 ||
		result.weaponMode >= TacticalWeaponModeCount ||
		result.scopeMode < TacticalMinimumScopeMode ||
		result.scopeMode >= TacticalScopeModeCount)
		return false;
	// Fire progress is carried exactly because legacy attachment insertion can
	// change the selected weapon mode without resetting an in-progress counter.
	// Barrel mode is likewise an item-table-dependent component value, not a
	// public enum. The one impossible representation is autofire without burst
	// progression. JA2's executor supplies semantic validation by re-running the
	// cause-specific resolver and comparing every result field before mutation.
	return result.burstCounter >= 0 &&
		(result.autofireShots == 0 || result.burstCounter != 0) &&
		(!result.resetAutofireBulletInitialization ||
			result.weaponMode == 2 || result.weaponMode == 5 ||
			result.weaponMode == 8);
}

// Cause is stable replay/diagnostic vocabulary and constrains the legal
// presentation/continuation combinations. Keeping it explicit makes an
// equipment correction distinguishable from a friendly retaliation even when
// both happen to select the same resulting mode.
enum class TacticalWeaponConfigurationCause : std::uint8_t
{
	EquipmentChanged = 0,
	LauncherUnavailable = 1,
	ScopeAttachmentChanged = 2,
	FriendlyRetaliation = 3,
	AttachedLauncherShotCompleted = 4
};

constexpr bool IsValidTacticalWeaponConfigurationCause(
	TacticalWeaponConfigurationCause cause) noexcept
{
	switch (cause)
	{
		case TacticalWeaponConfigurationCause::EquipmentChanged:
		case TacticalWeaponConfigurationCause::LauncherUnavailable:
		case TacticalWeaponConfigurationCause::ScopeAttachmentChanged:
		case TacticalWeaponConfigurationCause::FriendlyRetaliation:
		case TacticalWeaponConfigurationCause::AttachedLauncherShotCompleted:
			return true;
	}
	return false;
}

// Presentation work is independent from event replication and gameplay
// continuation. Only these established combinations are public vocabulary.
enum class TacticalWeaponConfigurationPostApplyPolicy : std::uint8_t
{
	None = 0,
	DirtyMercPanel = 1,
	DirtyMercPanelAndCursor = 2,
	DirtyMercPanelCursorAndSight = 3
};

constexpr bool IsValidTacticalWeaponConfigurationPostApplyPolicy(
	TacticalWeaponConfigurationPostApplyPolicy policy) noexcept
{
	switch (policy)
	{
		case TacticalWeaponConfigurationPostApplyPolicy::None:
		case TacticalWeaponConfigurationPostApplyPolicy::DirtyMercPanel:
		case TacticalWeaponConfigurationPostApplyPolicy::DirtyMercPanelAndCursor:
		case TacticalWeaponConfigurationPostApplyPolicy::DirtyMercPanelCursorAndSight:
			return true;
	}
	return false;
}

enum class TacticalWeaponConfigurationContinuation : std::uint8_t
{
	None = 0,
	CompleteHandItemChange = 1,
	BeginFriendlyRetaliation = 2,
	CompleteEquipmentChange = 3
};

constexpr bool IsValidTacticalWeaponConfigurationContinuation(
	TacticalWeaponConfigurationContinuation continuation) noexcept
{
	switch (continuation)
	{
		case TacticalWeaponConfigurationContinuation::None:
		case TacticalWeaponConfigurationContinuation::CompleteHandItemChange:
		case TacticalWeaponConfigurationContinuation::BeginFriendlyRetaliation:
		case TacticalWeaponConfigurationContinuation::CompleteEquipmentChange:
			return true;
	}
	return false;
}

constexpr bool IsValidTacticalWeaponConfigurationPolicyForCause(
	TacticalWeaponConfigurationCause cause,
	TacticalWeaponConfigurationPostApplyPolicy postApplyPolicy,
	TacticalWeaponConfigurationContinuation continuation) noexcept
{
	switch (cause)
	{
		case TacticalWeaponConfigurationCause::EquipmentChanged:
			return postApplyPolicy ==
					TacticalWeaponConfigurationPostApplyPolicy::None &&
				(continuation ==
						TacticalWeaponConfigurationContinuation::
							CompleteEquipmentChange ||
					continuation ==
						TacticalWeaponConfigurationContinuation::
							CompleteHandItemChange);
		case TacticalWeaponConfigurationCause::LauncherUnavailable:
			return postApplyPolicy ==
					TacticalWeaponConfigurationPostApplyPolicy::
						DirtyMercPanelAndCursor &&
				continuation ==
					TacticalWeaponConfigurationContinuation::None;
		case TacticalWeaponConfigurationCause::ScopeAttachmentChanged:
			return postApplyPolicy ==
					TacticalWeaponConfigurationPostApplyPolicy::
						DirtyMercPanelCursorAndSight &&
				continuation ==
					TacticalWeaponConfigurationContinuation::None;
		case TacticalWeaponConfigurationCause::FriendlyRetaliation:
			return postApplyPolicy ==
					TacticalWeaponConfigurationPostApplyPolicy::
						DirtyMercPanelAndCursor &&
				continuation ==
					TacticalWeaponConfigurationContinuation::
						BeginFriendlyRetaliation;
		case TacticalWeaponConfigurationCause::
				AttachedLauncherShotCompleted:
			return postApplyPolicy ==
					TacticalWeaponConfigurationPostApplyPolicy::
						DirtyMercPanel &&
				continuation ==
					TacticalWeaponConfigurationContinuation::None;
	}
	return false;
}

struct ApplyWeaponConfigurationCommand
{
	TacticalEntityId soldier;
	TacticalWeaponConfigurationResult result;
	TacticalWeaponConfigurationCause cause =
		TacticalWeaponConfigurationCause::EquipmentChanged;
	TacticalWeaponConfigurationPostApplyPolicy postApplyPolicy =
		TacticalWeaponConfigurationPostApplyPolicy::None;
	TacticalWeaponConfigurationContinuation continuation =
		TacticalWeaponConfigurationContinuation::None;
	TacticalEventPolicy eventPolicy = TacticalEventPolicy::LocalOnly;
	// The expected hand item is populated for every command and rejects a
	// correction retained across a weapon swap. Target identity/location belong
	// only to BeginFriendlyRetaliation; inventory position and previous/changed
	// item belong only to equipment continuations. The executor re-resolves the
	// complete result from this context before any mutation, so a retained
	// command cannot apply a result made stale by an in-place attachment or
	// configuration change. Exact target incarnation and issued location prevent
	// deferred retaliation from following a reused soldier slot.
	TacticalEntityId target;
	std::int32_t targetGrid = TacticalNoTargetGrid;
	std::int8_t targetLevel = 0;
	std::uint32_t handItem = 0;
	std::uint32_t previousItem = 0;
	std::uint32_t changedItem = 0;
	std::uint32_t inventoryPosition = TacticalNoInventoryPosition;
	SimulationCommandSource source = SimulationCommandSource::System;
};

// Ready/lower intent records the resolved eight-way direction rather than a
// mouse coordinate. The compatibility executor may still select JA2's exact
// animation, while replay and network producers retain the player's choice.
struct SetWeaponReadyCommand
{
	TacticalEntityId soldier;
	std::uint8_t direction;
	bool ready;
	bool alternativeHold;
	SimulationCommandSource source;
};

// Explicit values keep the current replay/network vocabulary independent from
// legacy animation constants.
enum class TacticalTraversalKind : std::uint8_t
{
	ClimbUpRoof = 0,
	ClimbDownRoof = 1,
	JumpFence = 2,
	ClimbWall = 3,
	JumpWindow = 4
};

constexpr bool IsValidTacticalTraversalKind(
	TacticalTraversalKind kind) noexcept
{
	switch (kind)
	{
		case TacticalTraversalKind::ClimbUpRoof:
		case TacticalTraversalKind::ClimbDownRoof:
		case TacticalTraversalKind::JumpFence:
		case TacticalTraversalKind::ClimbWall:
		case TacticalTraversalKind::JumpWindow:
			return true;
	}
	return false;
}

// Traversal has three distinct producers. Player intent is synchronous and
// carries no mutable compatibility state. AI actions and path completion can
// be retained behind another authoritative command, so they capture the
// exact state against which the legacy continuation was selected.
enum class TacticalTraversalOrigin : std::uint8_t
{
	PlayerIntent = 0,
	AiAction = 1,
	PathCompletion = 2
};

constexpr bool IsValidTacticalTraversalOrigin(
	TacticalTraversalOrigin origin) noexcept
{
	switch (origin)
	{
		case TacticalTraversalOrigin::PlayerIntent:
		case TacticalTraversalOrigin::AiAction:
		case TacticalTraversalOrigin::PathCompletion:
			return true;
	}
	return false;
}

enum class TacticalTraversalContinuation : std::uint8_t
{
	None = 0,
	CompleteAiAction = 1,
	ContinuePathAfterStance = 2
};

constexpr bool IsValidTacticalTraversalContinuation(
	TacticalTraversalContinuation continuation) noexcept
{
	switch (continuation)
	{
		case TacticalTraversalContinuation::None:
		case TacticalTraversalContinuation::CompleteAiAction:
		case TacticalTraversalContinuation::ContinuePathAfterStance:
			return true;
	}
	return false;
}

inline constexpr std::int8_t TacticalTraversalNoExpectedLevel = -1;
inline constexpr std::uint8_t TacticalTraversalNoExpectedDirection =
	TacticalDirectionCount;
inline constexpr std::uint16_t TacticalTraversalNoExpectedAnimation =
	std::numeric_limits<std::uint16_t>::max();
inline constexpr std::uint16_t TacticalTraversalNoExpectedPathValue =
	std::numeric_limits<std::uint16_t>::max();
inline constexpr std::uint64_t TacticalTraversalNoExpectedStateFingerprint =
	std::numeric_limits<std::uint64_t>::max();
inline constexpr std::int16_t TacticalTraversalNoExpectedPointCost =
	std::numeric_limits<std::int16_t>::min();
inline constexpr std::uint16_t TacticalTraversalPathCapacity = 30;

struct TraverseObstacleCommand
{
	TacticalEntityId soldier;
	TacticalTraversalKind kind;
	SimulationCommandSource source;
	// Appended fields preserve the original aggregate prefix and make only the
	// retained System/Replay forms stateful. Sentinel-only player commands keep
	// their established request semantics.
	TacticalTraversalOrigin origin =
		TacticalTraversalOrigin::PlayerIntent;
	TacticalTraversalContinuation continuation =
		TacticalTraversalContinuation::None;
	TacticalEventPolicy eventPolicy = TacticalEventPolicy::LocalOnly;
	std::int32_t expectedGrid = TacticalNoTargetGrid;
	std::int32_t expectedFinalDestination = TacticalNoTargetGrid;
	std::int8_t expectedLevel = TacticalTraversalNoExpectedLevel;
	std::uint8_t expectedDirection = TacticalTraversalNoExpectedDirection;
	std::uint16_t expectedAnimationState =
		TacticalTraversalNoExpectedAnimation;
	std::uint16_t movementAnimationState =
		TacticalTraversalNoExpectedAnimation;
	std::uint16_t expectedPathIndex =
		TacticalTraversalNoExpectedPathValue;
	std::uint16_t expectedPathSize =
		TacticalTraversalNoExpectedPathValue;
	std::uint8_t expectedPathDirection =
		TacticalTraversalNoExpectedDirection;
	std::uint8_t expectedNextPathDirection =
		TacticalTraversalNoExpectedDirection;
	std::uint64_t expectedStateFingerprint =
		TacticalTraversalNoExpectedStateFingerprint;
	std::int16_t expectedActionPointCost =
		TacticalTraversalNoExpectedPointCost;
	std::int16_t expectedBreathPointCost =
		TacticalTraversalNoExpectedPointCost;
};

constexpr bool HasNoTacticalTraversalExpectation(
	const TraverseObstacleCommand& command) noexcept
{
	return command.expectedGrid == TacticalNoTargetGrid &&
		command.expectedFinalDestination == TacticalNoTargetGrid &&
		command.expectedLevel == TacticalTraversalNoExpectedLevel &&
		command.expectedDirection == TacticalTraversalNoExpectedDirection &&
		command.expectedAnimationState ==
			TacticalTraversalNoExpectedAnimation &&
		command.movementAnimationState ==
			TacticalTraversalNoExpectedAnimation &&
		command.expectedPathIndex == TacticalTraversalNoExpectedPathValue &&
		command.expectedPathSize == TacticalTraversalNoExpectedPathValue &&
		command.expectedPathDirection ==
			TacticalTraversalNoExpectedDirection &&
		command.expectedNextPathDirection ==
			TacticalTraversalNoExpectedDirection &&
		command.expectedStateFingerprint ==
			TacticalTraversalNoExpectedStateFingerprint &&
		command.expectedActionPointCost ==
			TacticalTraversalNoExpectedPointCost &&
		command.expectedBreathPointCost ==
			TacticalTraversalNoExpectedPointCost;
}

constexpr bool IsStructurallyValidTacticalTraversalCommand(
	const TraverseObstacleCommand& command) noexcept
{
	if (!IsValidTacticalTraversalKind(command.kind) ||
		!IsValidTacticalTraversalOrigin(command.origin) ||
		!IsValidTacticalTraversalContinuation(command.continuation) ||
		!IsValidTacticalEventPolicy(command.eventPolicy))
		return false;

	if (command.origin == TacticalTraversalOrigin::PlayerIntent)
		return command.continuation ==
				TacticalTraversalContinuation::None &&
			command.eventPolicy == TacticalEventPolicy::LocalOnly &&
			HasNoTacticalTraversalExpectation(command);

	if ((command.source != SimulationCommandSource::System &&
		 command.source != SimulationCommandSource::Replay) ||
		command.expectedGrid < 0 ||
		(command.expectedLevel != 0 && command.expectedLevel != 1) ||
		!IsValidTacticalDirection(command.expectedDirection) ||
		command.expectedAnimationState ==
			TacticalTraversalNoExpectedAnimation)
		return false;

	if (command.origin == TacticalTraversalOrigin::AiAction)
	{
		const bool roofAction =
			command.kind == TacticalTraversalKind::ClimbUpRoof ||
			command.kind == TacticalTraversalKind::ClimbDownRoof;
		const bool windowAction =
			command.kind == TacticalTraversalKind::JumpWindow;
		const bool commonAiShape =
			command.expectedFinalDestination == TacticalNoTargetGrid &&
			command.movementAnimationState ==
				TacticalTraversalNoExpectedAnimation &&
			command.expectedNextPathDirection ==
				TacticalTraversalNoExpectedDirection;
		if (!commonAiShape) return false;
		if (roofAction)
			return command.eventPolicy == TacticalEventPolicy::Replicated &&
				(command.kind == TacticalTraversalKind::ClimbUpRoof
					? command.expectedLevel == 0
					: command.expectedLevel == 1) &&
				command.continuation ==
					TacticalTraversalContinuation::None &&
				command.expectedPathIndex ==
					TacticalTraversalNoExpectedPathValue &&
				command.expectedPathSize ==
					TacticalTraversalNoExpectedPathValue &&
				command.expectedPathDirection ==
					TacticalTraversalNoExpectedDirection &&
				command.expectedStateFingerprint !=
					TacticalTraversalNoExpectedStateFingerprint &&
				command.expectedActionPointCost !=
					TacticalTraversalNoExpectedPointCost &&
				command.expectedBreathPointCost == 0;
		if (!windowAction ||
			command.eventPolicy != TacticalEventPolicy::Replicated ||
			command.continuation !=
				TacticalTraversalContinuation::CompleteAiAction ||
			command.expectedStateFingerprint ==
				TacticalTraversalNoExpectedStateFingerprint ||
			command.expectedActionPointCost !=
				TacticalTraversalNoExpectedPointCost ||
			command.expectedBreathPointCost !=
				TacticalTraversalNoExpectedPointCost ||
			command.expectedPathSize > TacticalTraversalPathCapacity ||
			command.expectedPathIndex > command.expectedPathSize)
			return false;
		return command.expectedPathIndex < command.expectedPathSize
			? IsValidTacticalDirection(command.expectedPathDirection)
			: command.expectedPathDirection ==
				TacticalTraversalNoExpectedDirection;
	}

	return command.kind == TacticalTraversalKind::JumpFence &&
		command.continuation ==
			TacticalTraversalContinuation::ContinuePathAfterStance &&
		command.eventPolicy == TacticalEventPolicy::Replicated &&
		command.expectedFinalDestination >= 0 &&
		command.movementAnimationState !=
			TacticalTraversalNoExpectedAnimation &&
		command.expectedStateFingerprint !=
			TacticalTraversalNoExpectedStateFingerprint &&
		command.expectedActionPointCost !=
			TacticalTraversalNoExpectedPointCost &&
		command.expectedBreathPointCost !=
			TacticalTraversalNoExpectedPointCost &&
		command.expectedPathSize >= 2 &&
		command.expectedPathSize <= TacticalTraversalPathCapacity &&
		command.expectedPathIndex < command.expectedPathSize &&
		command.expectedPathIndex + 1 < command.expectedPathSize &&
		IsValidTacticalDirection(command.expectedPathDirection) &&
		IsValidTacticalDirection(command.expectedNextPathDirection);
}

// Traversal continuation may emit the established stance/stop traffic only
// for a newly produced System action. Captured replay applies the same local
// stance, no-AP halt, and AI completion without reflecting traffic outward.
constexpr bool ShouldReplicateTraversalContinuation(
	SimulationCommandSource source,
	TacticalEventPolicy eventPolicy) noexcept
{
	return source == SimulationCommandSource::System &&
		eventPolicy == TacticalEventPolicy::Replicated;
}

// A map-local, pointer-free identity for an interactive structure. The grid
// and structure ID are resolved against the live tactical world only by the
// JA2 compatibility executor.
struct TacticalWorldObjectId
{
	std::int32_t grid;
	std::uint16_t structureId;
};

struct ActivateWorldObjectCommand
{
	TacticalEntityId soldier;
	TacticalWorldObjectId object;
	std::uint8_t direction;
	SimulationCommandSource source;
};

// Approaching an object must be one authoritative operation: accepting a
// movement command without its pending door/structure action would leave the
// actor at the destination with the player's interaction silently lost.
struct ApproachWorldObjectCommand
{
	TacticalEntityId soldier;
	TacticalWorldObjectId object;
	std::uint8_t direction;
	std::int32_t destinationGrid;
	std::uint16_t movementMode;
	bool reverse;
	bool forceRestart;
	SimulationCommandSource source;
};

// Automatic structure handling is not player intent. It can be retained by
// the authoritative stream, so the selected operation and every live-world
// precondition needed by its continuation travel as values instead of being
// rediscovered after backpressure.
enum class TacticalWorldObjectOperation : std::uint8_t
{
	Open = 0,
	Close = 1,
	Unlock = 2,
	Lock = 3
};

constexpr bool IsValidTacticalWorldObjectOperation(
	TacticalWorldObjectOperation operation) noexcept
{
	switch (operation)
	{
		case TacticalWorldObjectOperation::Open:
		case TacticalWorldObjectOperation::Close:
		case TacticalWorldObjectOperation::Unlock:
		case TacticalWorldObjectOperation::Lock:
			return true;
	}
	return false;
}

enum class TacticalWorldObjectOrigin : std::uint8_t
{
	AiAction = 0,
	PathTraversal = 1,
	PendingAction = 2,
	Dialogue = 3
};

constexpr bool IsValidTacticalWorldObjectOrigin(
	TacticalWorldObjectOrigin origin) noexcept
{
	switch (origin)
	{
		case TacticalWorldObjectOrigin::AiAction:
		case TacticalWorldObjectOrigin::PathTraversal:
		case TacticalWorldObjectOrigin::PendingAction:
		case TacticalWorldObjectOrigin::Dialogue:
			return true;
	}
	return false;
}

enum class TacticalWorldObjectContinuation : std::uint8_t
{
	None = 0,
	ResumePathAndCloseDoor = 1,
	MarkDialogueActionPending = 2,
	MarkDialogueApproachPending = 3
};

constexpr bool IsValidTacticalWorldObjectContinuation(
	TacticalWorldObjectContinuation continuation) noexcept
{
	switch (continuation)
	{
		case TacticalWorldObjectContinuation::None:
		case TacticalWorldObjectContinuation::ResumePathAndCloseDoor:
		case TacticalWorldObjectContinuation::MarkDialogueActionPending:
		case TacticalWorldObjectContinuation::MarkDialogueApproachPending:
			return true;
	}
	return false;
}

inline constexpr std::int32_t TacticalWorldObjectNoExpectedGrid = -1;
inline constexpr std::int8_t TacticalWorldObjectNoExpectedLevel = -1;
inline constexpr std::uint16_t TacticalWorldObjectNoExpectedAnimation =
	std::numeric_limits<std::uint16_t>::max();
inline constexpr std::uint16_t TacticalWorldObjectNoExpectedPathValue =
	std::numeric_limits<std::uint16_t>::max();
inline constexpr std::uint64_t TacticalWorldObjectNoExpectedFingerprint =
	std::numeric_limits<std::uint64_t>::max();
inline constexpr std::int16_t TacticalWorldObjectNoExpectedPointCost =
	std::numeric_limits<std::int16_t>::min();

struct SystemWorldObjectInteractionCommand
{
	TacticalEntityId soldier;
	TacticalWorldObjectId object;
	std::uint8_t direction;
	TacticalWorldObjectOperation operation;
	SimulationCommandSource source;
	TacticalWorldObjectOrigin origin;
	TacticalWorldObjectContinuation continuation;
	TacticalEventPolicy eventPolicy;
	bool unlockBeforeInteraction = false;
	std::int32_t expectedGrid;
	std::int32_t expectedDestinationGrid =
		TacticalWorldObjectNoExpectedGrid;
	std::int8_t expectedLevel;
	std::uint16_t expectedAnimationState;
	std::uint16_t movementMode =
		TacticalWorldObjectNoExpectedAnimation;
	std::uint16_t expectedPathIndex =
		TacticalWorldObjectNoExpectedPathValue;
	std::uint16_t expectedPathSize =
		TacticalWorldObjectNoExpectedPathValue;
	std::uint8_t expectedPathDirection = TacticalDirectionCount;
	std::uint64_t expectedStateFingerprint;
	std::uint64_t expectedObjectFingerprint;
	std::int16_t expectedActionPointCost =
		TacticalWorldObjectNoExpectedPointCost;
	std::int16_t expectedBreathPointCost =
		TacticalWorldObjectNoExpectedPointCost;
};

static_assert(
	std::is_trivially_copyable<SystemWorldObjectInteractionCommand>::value,
	"automatic world-object commands must remain pointer-free values");

constexpr bool IsStructurallyValidSystemWorldObjectInteractionCommand(
	const SystemWorldObjectInteractionCommand& command) noexcept
{
	if ((command.source != SimulationCommandSource::System &&
		 command.source != SimulationCommandSource::Replay) ||
		command.object.grid < 0 ||
		!IsValidTacticalDirection(command.direction) ||
		!IsValidTacticalWorldObjectOperation(command.operation) ||
		!IsValidTacticalWorldObjectOrigin(command.origin) ||
		!IsValidTacticalWorldObjectContinuation(command.continuation) ||
		!IsValidTacticalEventPolicy(command.eventPolicy) ||
		command.eventPolicy != TacticalEventPolicy::Replicated ||
		command.expectedGrid < 0 ||
		(command.expectedLevel != 0 && command.expectedLevel != 1) ||
		command.expectedAnimationState ==
			TacticalWorldObjectNoExpectedAnimation ||
		command.expectedStateFingerprint ==
			TacticalWorldObjectNoExpectedFingerprint ||
		command.expectedObjectFingerprint ==
			TacticalWorldObjectNoExpectedFingerprint)
		return false;

	const bool noPath =
		command.expectedPathIndex == TacticalWorldObjectNoExpectedPathValue &&
		command.expectedPathSize == TacticalWorldObjectNoExpectedPathValue &&
		command.expectedPathDirection == TacticalDirectionCount;
	const bool noPoints =
		command.expectedActionPointCost ==
			TacticalWorldObjectNoExpectedPointCost &&
		command.expectedBreathPointCost ==
			TacticalWorldObjectNoExpectedPointCost;
	const bool noDestination =
		command.expectedDestinationGrid ==
			TacticalWorldObjectNoExpectedGrid &&
		command.movementMode == TacticalWorldObjectNoExpectedAnimation;
	const bool opensOrCloses =
		command.operation == TacticalWorldObjectOperation::Open ||
		command.operation == TacticalWorldObjectOperation::Close;

	switch (command.origin)
	{
		case TacticalWorldObjectOrigin::AiAction:
			return command.continuation ==
					TacticalWorldObjectContinuation::None &&
				!command.unlockBeforeInteraction && noPath && noPoints &&
				noDestination;
		case TacticalWorldObjectOrigin::PathTraversal:
			return opensOrCloses && !command.unlockBeforeInteraction &&
				noPoints &&
				(command.continuation ==
					TacticalWorldObjectContinuation::None ||
				 command.continuation ==
					TacticalWorldObjectContinuation::
						ResumePathAndCloseDoor) &&
				command.expectedDestinationGrid >= 0 &&
				command.movementMode !=
					TacticalWorldObjectNoExpectedAnimation &&
				command.expectedPathSize > 0 &&
				command.expectedPathSize <= TacticalTraversalPathCapacity &&
				command.expectedPathIndex < command.expectedPathSize &&
				IsValidTacticalDirection(command.expectedPathDirection) &&
				(command.direction & 1u) == 0 &&
				command.expectedPathDirection == command.direction;
		case TacticalWorldObjectOrigin::PendingAction:
			return opensOrCloses &&
				!command.unlockBeforeInteraction &&
				command.continuation ==
					TacticalWorldObjectContinuation::None &&
				noPath && noDestination &&
				command.expectedActionPointCost !=
					TacticalWorldObjectNoExpectedPointCost &&
				command.expectedBreathPointCost !=
					TacticalWorldObjectNoExpectedPointCost;
		case TacticalWorldObjectOrigin::Dialogue:
			if (command.operation != TacticalWorldObjectOperation::Open ||
				!noPath || !noPoints ||
				command.expectedDestinationGrid < 0)
				return false;
			if (command.continuation ==
				TacticalWorldObjectContinuation::
					MarkDialogueActionPending)
				return command.movementMode ==
						TacticalWorldObjectNoExpectedAnimation &&
					command.expectedDestinationGrid == command.expectedGrid;
			return command.continuation ==
					TacticalWorldObjectContinuation::
						MarkDialogueApproachPending &&
				command.movementMode !=
					TacticalWorldObjectNoExpectedAnimation &&
				command.expectedDestinationGrid != command.expectedGrid;
	}
	return false;
}

// Door completion can outlive the executor scope. Newly produced local/System
// work may cross the established peer boundary; replay and peer-originated work
// perform the same local animation without reflection.
constexpr bool ShouldReplicateWorldObjectCompletion(
	SimulationCommandSource source,
	TacticalEventPolicy eventPolicy) noexcept
{
	return eventPolicy == TacticalEventPolicy::Replicated &&
		(source == SimulationCommandSource::LocalPlayer ||
		 source == SimulationCommandSource::System);
}

// Entity-to-entity player intent always carries both incarnations. Soldier
// pool slots are reused, including while an actor is walking toward a delayed
// interaction, so a slot alone is not an authoritative target identity.
struct StartConversationCommand
{
	TacticalEntityId soldier;
	TacticalEntityId target;
	SimulationCommandSource source;
};

struct ApproachConversationCommand
{
	TacticalEntityId soldier;
	TacticalEntityId target;
	std::int32_t destinationGrid;
	std::uint16_t movementMode;
	bool forceRestart;
	SimulationCommandSource source;
};

inline constexpr std::uint8_t TacticalMaximumVehicleSeats = 10;

struct EnterVehicleCommand
{
	TacticalEntityId soldier;
	TacticalEntityId vehicle;
	std::uint8_t direction;
	std::uint8_t seatIndex;
	SimulationCommandSource source;
};

struct ApproachVehicleCommand
{
	TacticalEntityId soldier;
	TacticalEntityId vehicle;
	std::uint8_t direction;
	std::uint8_t seatIndex;
	std::int32_t destinationGrid;
	std::uint16_t movementMode;
	bool forceRestart;
	SimulationCommandSource source;
};

enum class TacticalWorldItemPickupKind : std::uint8_t
{
	SpecificItem = 0,
	SearchGrid = 1
};

constexpr bool IsValidTacticalWorldItemPickupKind(
	TacticalWorldItemPickupKind kind) noexcept
{
	switch (kind)
	{
		case TacticalWorldItemPickupKind::SpecificItem:
		case TacticalWorldItemPickupKind::SearchGrid:
			return true;
	}
	return false;
}

inline constexpr std::uint32_t TacticalMaximumWorldItemSlot =
	static_cast<std::uint32_t>(
		std::numeric_limits<std::int32_t>::max());

struct PickupWorldItemCommand
{
	TacticalEntityId soldier;
	TacticalWorldItemId item;
	std::int32_t grid;
	// Item-pool display height above its tactical floor, not the actor's
	// ground/roof level. The JA2 executor validates that floor separately.
	std::int8_t renderHeight;
	TacticalWorldItemPickupKind kind;
	SimulationCommandSource source;
};

// Stealing is a delayed actor-to-actor interaction. Capture both the target
// incarnation and its issued location so walking to the target cannot silently
// retarget a replacement slot occupant or follow an actor that has moved.
struct StealFromActorCommand
{
	TacticalEntityId soldier;
	TacticalEntityId target;
	std::int32_t targetGrid;
	std::int8_t targetLevel;
	SimulationCommandSource source;
};

// Position exchange mutates two actors atomically. Capturing both issued grids
// and their common level makes a queued/replayed command reject changed world
// state instead of swapping actors from unrelated later positions.
struct ExchangePositionsCommand
{
	TacticalEntityId soldier;
	TacticalEntityId target;
	std::int32_t soldierGrid;
	std::int32_t targetGrid;
	std::int8_t level;
	SimulationCommandSource source;
};

// Multiplayer movement packets are authoritative path snapshots, not local
// pathfinding intent. Keeping the fixed legacy capacity in a value command
// lets network and replay ingress preserve the exact remote path without
// exposing TacticalActor storage or re-running pathfinding on the receiver.
inline constexpr std::size_t TacticalReplicatedPathCapacity = 30;

struct SynchronizeActorPathCommand
{
	TacticalEntityId soldier;
	std::int32_t reportedGrid;
	std::int32_t destinationGrid;
	std::uint16_t movementState;
	std::uint16_t currentPathIndex;
	std::uint16_t pathSize;
	std::array<std::uint16_t, TacticalReplicatedPathCapacity> path{};
	SimulationCommandSource source;
};

// The established multiplayer fire packet carries the selected weapon in a
// field that is separate from the normal fire event. Capture it as part of the
// same authoritative operation so a queued packet cannot use later actor
// state.
struct SynchronizeActorFireCommand
{
	TacticalEntityId soldier;
	std::int32_t targetGrid;
	std::int8_t targetLevel;
	std::int8_t targetCubeLevel;
	std::uint32_t attackingWeapon;
	SimulationCommandSource source;
};

// A remote stop is also a small reconciliation snapshot. The receiver first
// restores the owner's reported cell/direction and then applies the legacy
// halt/no-AP policy as one ordered command.
struct SynchronizeActorStopCommand
{
	TacticalEntityId soldier;
	std::int32_t reportedGrid;
	std::int16_t positionX;
	std::int16_t positionY;
	std::uint8_t direction;
	bool stop;
	SimulationCommandSource source;
};

// Network turn synchronization has two host-dependent compatibility actions:
// clients may need to enter combat and close their current turn before the
// common BeginTeamTurn boundary. Recording those decisions makes replay
// execution independent from whatever network role exists later.
struct SynchronizeTurnCommand
{
	std::uint8_t nextTeam;
	bool enterCombat;
	bool endClientTurn;
	SimulationCommandSource source;
};

constexpr bool IsSimulationSynchronizationSource(
	SimulationCommandSource source) noexcept
{
	return source == SimulationCommandSource::NetworkPeer ||
		source == SimulationCommandSource::Replay;
}

constexpr bool IsSimulationSystemSource(
	SimulationCommandSource source) noexcept
{
	return source == SimulationCommandSource::System ||
		source == SimulationCommandSource::Replay;
}

// Only a freshly produced authoritative System continuation may cross the
// legacy multiplayer boundary. Replay is executable System provenance, but it
// must never re-emit captured outbound traffic.
constexpr bool ShouldReplicateWeaponConfigurationFire(
	SimulationCommandSource source,
	TacticalEventPolicy eventPolicy) noexcept
{
	return source == SimulationCommandSource::System &&
		eventPolicy == TacticalEventPolicy::Replicated;
}

// A closed, value-only command set keeps the deterministic queue independent
// from JA2 globals and pointers. New commands extend this variant while their
// legacy executors remain in the compatibility layer during migration.
using SimulationCommand = std::variant<
	EndTurnCommand,
	ChangeStanceCommand,
	BeginFireWeaponCommand,
	BeginSelectedFireWeaponCommand,
	MoveToGridCommand,
	SetFacingCommand,
	SetStealthModeCommand,
	StopMovementCommand,
	CycleWeaponModeCommand,
	CycleScopeModeCommand,
	ReloadWeaponCommand,
	TraverseObstacleCommand,
	ActivateWorldObjectCommand,
	ApproachWorldObjectCommand,
	StartConversationCommand,
	ApproachConversationCommand,
	EnterVehicleCommand,
	ApproachVehicleCommand,
	PickupWorldItemCommand,
	StealFromActorCommand,
	ExchangePositionsCommand,
	SetWeaponReadyCommand,
	CancelDragCommand,
	SynchronizeActorPathCommand,
	SynchronizeActorFireCommand,
	SynchronizeActorStopCommand,
	SynchronizeTurnCommand,
	BulkReloadWeaponsCommand,
	ApplyWeaponConfigurationCommand,
	SystemWorldObjectInteractionCommand>;

// EngineRuntime fixes this policy into its CommandStream type. Playback gets a
// distinct execution origin for every variant while the stream journals the
// original captured command; callers cannot supply a mismatched execution
// batch and diagnostic batch.
struct SimulationCommandPlaybackPolicy
{
	static SimulationCommand executionCommand(SimulationCommand command)
	{
		std::visit([](auto& value) noexcept {
			value.source = SimulationCommandSource::Replay;
		}, command);
		return command;
	}
};

// Shared transport/admission validation deliberately covers only the public
// value shape. Application-specific ranges and live-world policy belong to the
// JA2 executor. Keeping this visitor here prevents codecs and package ingress
// from acquiring subtly different command allowlists as the vocabulary grows.
inline bool IsStructurallyValidSimulationCommand(
	const SimulationCommand& command) noexcept
{
	if (command.valueless_by_exception()) return false;
	return std::visit([](const auto& value) noexcept {
		using Command = typename std::decay<decltype(value)>::type;
		if (!IsValidSimulationCommandSource(value.source)) return false;
		if constexpr (
			std::is_same<Command, EndTurnCommand>::value ||
			std::is_same<Command, SynchronizeTurnCommand>::value)
		{
			if constexpr (
				std::is_same<Command, SynchronizeTurnCommand>::value)
				return IsSimulationSynchronizationSource(value.source);
			return true;
		}
		else if constexpr (
			std::is_same<Command, BulkReloadWeaponsCommand>::value)
		{
			if ((value.source != SimulationCommandSource::LocalPlayer &&
					value.source != SimulationCommandSource::Replay) ||
				!IsValidTacticalBulkReloadMode(value.mode) ||
				value.soldierCount == 0 ||
				value.soldierCount > TacticalBulkReloadActorCapacity)
				return false;
			for (std::size_t index = 0; index < value.soldierCount; ++index)
			{
				if (!value.soldiers[index].valid() ||
					(index != 0 &&
						value.soldiers[index - 1].slot >=
							value.soldiers[index].slot))
					return false;
			}
			for (std::size_t index = value.soldierCount;
				index < TacticalBulkReloadActorCapacity; ++index)
				if (value.soldiers[index] != TacticalEntityId{}) return false;
			return true;
		}
		else if constexpr (
			std::is_same<Command, ApplyWeaponConfigurationCommand>::value)
		{
			if (!IsSimulationSystemSource(value.source) ||
				!value.soldier.valid() ||
				!IsValidTacticalWeaponConfigurationResult(value.result) ||
				!IsValidTacticalWeaponConfigurationCause(value.cause) ||
				!IsValidTacticalWeaponConfigurationPostApplyPolicy(
					value.postApplyPolicy) ||
				!IsValidTacticalWeaponConfigurationContinuation(
					value.continuation) ||
				!IsValidTacticalEventPolicy(value.eventPolicy) ||
				!IsValidTacticalWeaponConfigurationPolicyForCause(
					value.cause, value.postApplyPolicy,
					value.continuation))
				return false;
			const bool continuesRetaliation =
				value.continuation ==
					TacticalWeaponConfigurationContinuation::
						BeginFriendlyRetaliation;
			if (continuesRetaliation)
				return value.cause ==
						TacticalWeaponConfigurationCause::FriendlyRetaliation &&
					value.eventPolicy == TacticalEventPolicy::Replicated &&
					value.postApplyPolicy ==
						TacticalWeaponConfigurationPostApplyPolicy::
							DirtyMercPanelAndCursor &&
					value.target.valid() && value.target != value.soldier &&
					value.targetGrid >= 0 &&
					value.handItem <=
						std::numeric_limits<std::uint16_t>::max() &&
					value.previousItem == 0 && value.changedItem == 0 &&
					value.inventoryPosition == TacticalNoInventoryPosition;
			const bool completesHandItemChange =
				value.continuation ==
					TacticalWeaponConfigurationContinuation::
						CompleteHandItemChange;
			const bool completesEquipmentChange =
				value.continuation ==
					TacticalWeaponConfigurationContinuation::
						CompleteEquipmentChange;
			if (completesHandItemChange || completesEquipmentChange)
				return value.cause ==
						TacticalWeaponConfigurationCause::EquipmentChanged &&
					value.eventPolicy == TacticalEventPolicy::LocalOnly &&
					value.handItem <=
						std::numeric_limits<std::uint16_t>::max() &&
					value.previousItem <=
						std::numeric_limits<std::uint16_t>::max() &&
					value.changedItem <=
						std::numeric_limits<std::uint16_t>::max() &&
					value.inventoryPosition <=
						std::numeric_limits<std::uint16_t>::max() &&
					value.target == TacticalEntityId{} &&
					value.targetGrid == TacticalNoTargetGrid &&
					value.targetLevel == 0;
			return value.cause !=
					TacticalWeaponConfigurationCause::FriendlyRetaliation &&
				value.eventPolicy == TacticalEventPolicy::LocalOnly &&
				value.handItem <=
					std::numeric_limits<std::uint16_t>::max() &&
				value.target == TacticalEntityId{} &&
				value.targetGrid == TacticalNoTargetGrid &&
				value.targetLevel == 0 && value.previousItem == 0 &&
				value.changedItem == 0 &&
				value.inventoryPosition == TacticalNoInventoryPosition;
		}
		else if constexpr (
			std::is_same<Command,
				SystemWorldObjectInteractionCommand>::value)
		{
			return value.soldier.valid() &&
				IsStructurallyValidSystemWorldObjectInteractionCommand(value);
		}
		else
		{
			if (!value.soldier.valid()) return false;
			if constexpr (
				std::is_same<Command, SynchronizeActorPathCommand>::value)
				return IsSimulationSynchronizationSource(value.source) &&
					value.pathSize <= TacticalReplicatedPathCapacity &&
					value.currentPathIndex <= value.pathSize;
			if constexpr (
				std::is_same<Command, BeginSelectedFireWeaponCommand>::value)
				return IsSimulationSystemSource(value.source);
			if constexpr (
				std::is_same<Command, SynchronizeActorFireCommand>::value)
				return IsSimulationSynchronizationSource(value.source);
			if constexpr (
				std::is_same<Command, SynchronizeActorStopCommand>::value)
				return IsSimulationSynchronizationSource(value.source) &&
					IsValidTacticalDirection(value.direction);
			if constexpr (std::is_same<Command, MoveToGridCommand>::value)
				return IsValidTacticalMoveOrigin(value.origin) &&
					IsValidTacticalPendingActionPolicy(value.pendingAction);
			if constexpr (std::is_same<Command, ChangeStanceCommand>::value)
				return IsValidTacticalEventPolicy(value.eventPolicy) &&
					(value.source != SimulationCommandSource::NetworkPeer ||
						value.eventPolicy == TacticalEventPolicy::LocalOnly);
			if constexpr (std::is_same<Command, SetFacingCommand>::value)
				return IsValidTacticalDirection(value.direction) &&
					IsValidTacticalEventPolicy(value.eventPolicy) &&
					(value.source != SimulationCommandSource::NetworkPeer ||
						value.eventPolicy == TacticalEventPolicy::LocalOnly);
			if constexpr (std::is_same<Command, SetWeaponReadyCommand>::value)
				return IsValidTacticalDirection(value.direction);
			if constexpr (std::is_same<Command, TraverseObstacleCommand>::value)
				return IsStructurallyValidTacticalTraversalCommand(value);
			if constexpr (
				std::is_same<Command, ActivateWorldObjectCommand>::value ||
				std::is_same<Command, ApproachWorldObjectCommand>::value)
				return IsValidTacticalDirection(value.direction);
			if constexpr (
				std::is_same<Command, StartConversationCommand>::value ||
				std::is_same<Command, ApproachConversationCommand>::value ||
				std::is_same<Command, StealFromActorCommand>::value ||
				std::is_same<Command, ExchangePositionsCommand>::value)
				return value.target.valid() && value.target != value.soldier;
			if constexpr (
				std::is_same<Command, EnterVehicleCommand>::value ||
				std::is_same<Command, ApproachVehicleCommand>::value)
				return value.vehicle.valid() &&
					value.vehicle != value.soldier &&
					IsValidTacticalDirection(value.direction) &&
					value.seatIndex < TacticalMaximumVehicleSeats;
			if constexpr (
				std::is_same<Command, PickupWorldItemCommand>::value)
				return IsValidTacticalWorldItemPickupKind(value.kind) &&
					(value.kind ==
						TacticalWorldItemPickupKind::SpecificItem
						? value.item.valid() &&
							value.item.slot <= TacticalMaximumWorldItemSlot
						: value.item == TacticalWorldItemId{});
			return true;
		}
	}, command);
}

#endif
