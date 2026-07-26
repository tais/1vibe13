#include "Soldier Components.h"
#include "Soldier Control.h"

#include <algorithm>

bool SoldierVitalsComponent::alive() const noexcept { return health() > 0; }

void SoldierVitalsComponent::applyLifeDeduction(INT16 lifeDeduction)
{
	if (lifeDeduction > health())
	{
		health() = 0;
		return;
	}

	health() -= lifeDeduction;
	health() = std::min(health(), maximumHealth());
}

void SoldierVitalsComponent::reset() noexcept
{
	health_ = 0;
	maximumHealth_ = 0;
	breath_ = 0;
	maximumBreath_ = 0;
	bleeding_ = 0;
}

INT32& SoldierPositionComponent::gridNo() { return soldier_.sGridNo; }
const INT32& SoldierPositionComponent::gridNo() const { return soldier_.sGridNo; }
INT8& SoldierPositionComponent::level() { return soldier_.pathing.bLevel; }
const INT8& SoldierPositionComponent::level() const { return soldier_.pathing.bLevel; }
UINT8& SoldierPositionComponent::direction() { return soldier_.ubDirection; }
const UINT8& SoldierPositionComponent::direction() const { return soldier_.ubDirection; }
