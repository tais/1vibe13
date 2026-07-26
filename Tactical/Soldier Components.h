#ifndef TACTICAL_SOLDIER_COMPONENTS_H
#define TACTICAL_SOLDIER_COMPONENTS_H

#include "types.h"

#include <functional>

class SOLDIERTYPE;

// Focused views provide domain seams for serialized legacy fields without
// changing their representation yet. Runtime-only state below is already
// stored in owned components instead of extending SOLDIERTYPE's flat tail.
class SoldierVitalsComponent
{
public:
	explicit SoldierVitalsComponent(SOLDIERTYPE& soldier) : soldier_(soldier) {}

	INT8& health();
	const INT8& health() const;
	INT8& maximumHealth();
	const INT8& maximumHealth() const;
	INT8& breath();
	const INT8& breath() const;
	INT8& maximumBreath();
	const INT8& maximumBreath() const;
	INT8& bleeding();
	const INT8& bleeding() const;
	bool alive() const;
	void applyLifeDeduction(INT16 lifeDeduction);

private:
	SOLDIERTYPE& soldier_;
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

	void reset() noexcept
	{
		pendingAction.reset();
		combatFeedback.reset();
		quickItem.reset();
	}
};

#endif
