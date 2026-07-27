#ifndef TACTICAL_SOLDIER_COMPONENTS_H
#define TACTICAL_SOLDIER_COMPONENTS_H

#include "Overhead Types.h"
#include "types.h"

#include <functional>

// Canonical soldier vitals storage. Reference accessors keep legacy mutation
// sites zero-cost while the state itself has one owner and reset boundary.
class SoldierVitalsComponent
{
public:
	INT8& health() noexcept { return health_; }
	const INT8& health() const noexcept { return health_; }
	INT8& maximumHealth() noexcept { return maximumHealth_; }
	const INT8& maximumHealth() const noexcept { return maximumHealth_; }
	INT8& breath() noexcept { return breath_; }
	const INT8& breath() const noexcept { return breath_; }
	INT8& maximumBreath() noexcept { return maximumBreath_; }
	const INT8& maximumBreath() const noexcept { return maximumBreath_; }
	INT8& bleeding() noexcept { return bleeding_; }
	const INT8& bleeding() const noexcept { return bleeding_; }

	bool alive() const noexcept;
	void applyLifeDeduction(INT16 lifeDeduction);
	void reset() noexcept;

private:
	INT8 health_ = 0;
	INT8 maximumHealth_ = 0;
	INT8 breath_ = 0;
	INT8 maximumBreath_ = 0;
	INT8 bleeding_ = 0;
};

// Canonical current tactical location storage. Persistent adapters serialize
// these values at their established schema positions; the component itself is
// independent of the legacy SOLDIERTYPE declaration.
class SoldierPositionComponent
{
public:
	INT32& gridNo() noexcept { return gridNo_; }
	const INT32& gridNo() const noexcept { return gridNo_; }
	INT8& level() noexcept { return level_; }
	const INT8& level() const noexcept { return level_; }
	UINT8& direction() noexcept { return direction_; }
	const UINT8& direction() const noexcept { return direction_; }

	void reset() noexcept;

private:
	INT32 gridNo_ = 0;
	INT8 level_ = 0;
	UINT8 direction_ = 0;
};

// Canonical tactical route ownership. The fixed-capacity path and its cursor
// deliberately retain the established JA2 representation, while private
// storage prevents unrelated SOLDIERTYPE fields from becoming a second route
// authority.
class SoldierPathingComponent
{
public:
	using Path = UINT16[MAX_PATH_LIST_SIZE];

	INT8& desiredDirection() noexcept { return desiredDirection_; }
	const INT8& desiredDirection() const noexcept { return desiredDirection_; }
	INT16& destinationX() noexcept { return destinationX_; }
	const INT16& destinationX() const noexcept { return destinationX_; }
	INT16& destinationY() noexcept { return destinationY_; }
	const INT16& destinationY() const noexcept { return destinationY_; }
	INT32& destinationGrid() noexcept { return destinationGrid_; }
	const INT32& destinationGrid() const noexcept { return destinationGrid_; }
	INT32& finalDestinationGrid() noexcept { return finalDestinationGrid_; }
	const INT32& finalDestinationGrid() const noexcept { return finalDestinationGrid_; }
	INT8& stopped() noexcept { return stopped_; }
	const INT8& stopped() const noexcept { return stopped_; }
	INT8& needsLook() noexcept { return needsLook_; }
	const INT8& needsLook() const noexcept { return needsLook_; }
	Path& path() noexcept { return path_; }
	const Path& path() const noexcept { return path_; }
	UINT16& pathSize() noexcept { return pathSize_; }
	const UINT16& pathSize() const noexcept { return pathSize_; }
	UINT16& pathIndex() noexcept { return pathIndex_; }
	const UINT16& pathIndex() const noexcept { return pathIndex_; }
	INT32& blackListGrid() noexcept { return blackListGrid_; }
	const INT32& blackListGrid() const noexcept { return blackListGrid_; }
	INT8& stored() noexcept { return stored_; }
	const INT8& stored() const noexcept { return stored_; }

	bool empty() const noexcept { return pathSize_ == 0; }
	bool complete() const noexcept { return pathIndex_ >= pathSize_; }
	void clearRoute() noexcept;
	void reset() noexcept;

private:
	INT8 desiredDirection_ = 0;
	INT16 destinationX_ = 0;
	INT16 destinationY_ = 0;
	INT32 destinationGrid_ = 0;
	INT32 finalDestinationGrid_ = 0;
	INT8 stopped_ = 0;
	INT8 needsLook_ = 0;
	Path path_{};
	UINT16 pathSize_ = 0;
	UINT16 pathIndex_ = 0;
	INT32 blackListGrid_ = 0;
	INT8 stored_ = 0;
};

// Canonical tactical movement intent and contention state. Route geometry
// belongs to SoldierPathingComponent; this component owns the mutable state
// used while executing that route around reservations and other soldiers.
class SoldierMovementComponent
{
public:
	UINT8& delayCounter() noexcept { return delayCounter_; }
	const UINT8& delayCounter() const noexcept { return delayCounter_; }
	INT32& delayedCauseGrid() noexcept { return delayedCauseGrid_; }
	const INT32& delayedCauseGrid() const noexcept { return delayedCauseGrid_; }
	INT32& reservedGrid() noexcept { return reservedGrid_; }
	const INT32& reservedGrid() const noexcept { return reservedGrid_; }
	BOOLEAN& blockedByAnotherMerc() noexcept { return blockedByAnotherMerc_; }
	const BOOLEAN& blockedByAnotherMerc() const noexcept { return blockedByAnotherMerc_; }
	INT8& blockedDirection() noexcept { return blockedDirection_; }
	const INT8& blockedDirection() const noexcept { return blockedDirection_; }
	INT32& absoluteDestination() noexcept { return absoluteDestination_; }
	const INT32& absoluteDestination() const noexcept { return absoluteDestination_; }
	INT32& continuedPathGrid() noexcept { return continuedPathGrid_; }
	const INT32& continuedPathGrid() const noexcept { return continuedPathGrid_; }
	INT8& continuedPathValid() noexcept { return continuedPathValid_; }
	const INT8& continuedPathValid() const noexcept { return continuedPathValid_; }
	UINT8& delayedFlags() noexcept { return delayedFlags_; }
	const UINT8& delayedFlags() const noexcept { return delayedFlags_; }
	UINT8& stopReason() noexcept { return stopReason_; }
	const UINT8& stopReason() const noexcept { return stopReason_; }
	SoldierID& moveSpeedOverride() noexcept { return moveSpeedOverride_; }
	const SoldierID& moveSpeedOverride() const noexcept { return moveSpeedOverride_; }
	BOOLEAN& usesMoveSpeedOverride() noexcept { return usesMoveSpeedOverride_; }
	const BOOLEAN& usesMoveSpeedOverride() const noexcept { return usesMoveSpeedOverride_; }

	bool delayed() const noexcept { return delayCounter_ != 0; }
	void waitForGrid(INT32 gridNo, UINT8 counter) noexcept;
	void clearDelay() noexcept { delayCounter_ = 0; }
	void blockInDirection(INT8 direction) noexcept;
	void clearBlock() noexcept { blockedByAnotherMerc_ = FALSE; }
	void setContinuedPath(INT32 gridNo) noexcept;
	void clearContinuedPath() noexcept { continuedPathValid_ = FALSE; }
	void overrideMoveSpeedWith(SoldierID soldier) noexcept;
	void clearMoveSpeedOverride() noexcept { usesMoveSpeedOverride_ = FALSE; }
	void reset() noexcept;

private:
	UINT8 delayCounter_ = 0;
	INT32 delayedCauseGrid_ = 0;
	INT32 reservedGrid_ = 0;
	BOOLEAN blockedByAnotherMerc_ = FALSE;
	INT8 blockedDirection_ = 0;
	INT32 absoluteDestination_ = 0;
	INT32 continuedPathGrid_ = 0;
	INT8 continuedPathValid_ = FALSE;
	UINT8 delayedFlags_ = 0;
	UINT8 stopReason_ = 0;
	SoldierID moveSpeedOverride_{};
	BOOLEAN usesMoveSpeedOverride_ = FALSE;
};

struct SoldierPendingActionRuntimeState
{
	// Debug/path scratch retained across the path-cost operation only.
	INT32 pathSearchSourceGrid = 0;

	// Incarnation paired with a legacy pending target slot/grid. Delayed
	// completion must not follow a slot after it has been reused.
	UINT32 targetIncarnation = 0;

	// Transient launcher selection and deferred damage work.
	UINT16 grenadeItem = 0;
	std::function<void()> delayedDamage;

	void reset() noexcept
	{
		pathSearchSourceGrid = 0;
		targetIncarnation = 0;
		grenadeItem = 0;
		delayedDamage = nullptr;
	}
};

struct SoldierCombatFeedbackState
{
	// Presentation counters for the most recent attack. They are intentionally
	// runtime-only and are never part of a soldier save payload.
	UINT8 lastShock = 0;
	UINT8 lastSuppression = 0;
	UINT8 lastActionPoints = 0;
	UINT8 lastMorale = 0;
	UINT8 lastShockFromHit = 0;
	UINT8 lastActionPointsFromHit = 0;
	UINT8 lastMoraleFromHit = 0;
	UINT8 lastBulletImpact = 0;
	UINT8 lastArmourProtection = 0;

	void reset() noexcept
	{
		*this = SoldierCombatFeedbackState{};
	}
};

struct SoldierQuickItemRuntimeState
{
	UINT16 itemId = 0;
	UINT8 slot = 0;

	void reset() noexcept
	{
		*this = SoldierQuickItemRuntimeState{};
	}
};

struct SoldierRuntimeComponents
{
	SoldierPendingActionRuntimeState pendingAction;
	SoldierCombatFeedbackState combatFeedback;
	SoldierQuickItemRuntimeState quickItem;

	SoldierRuntimeComponents() = default;

	// A SOLDIERTYPE clone represents a new runtime object. Never copy deferred
	// callbacks that capture the source soldier, stale target incarnations, or
	// presentation/UI scratch into that clone.
	SoldierRuntimeComponents(const SoldierRuntimeComponents&) noexcept {}

	SoldierRuntimeComponents& operator=(const SoldierRuntimeComponents&) noexcept
	{
		reset();
		return *this;
	}

	void reset() noexcept
	{
		pendingAction.reset();
		combatFeedback.reset();
		quickItem.reset();
	}
};

#endif
