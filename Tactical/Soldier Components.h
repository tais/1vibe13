#ifndef TACTICAL_SOLDIER_COMPONENTS_H
#define TACTICAL_SOLDIER_COMPONENTS_H

#include "types.h"

#include <functional>

class SOLDIERTYPE;

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

class SoldierPositionComponent
{
public:
	explicit SoldierPositionComponent(SOLDIERTYPE& soldier) : soldier_(soldier) {}

	INT32& gridNo();
	const INT32& gridNo() const;
	INT8& level();
	const INT8& level() const;
	UINT8& direction();
	const UINT8& direction() const;

private:
	SOLDIERTYPE& soldier_;
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
