#include "Soldier Components.h"
#include "Soldier Control.h"

#include <algorithm>

INT8& SoldierVitalsComponent::health() { return soldier_.stats.bLife; }
const INT8& SoldierVitalsComponent::health() const { return soldier_.stats.bLife; }
INT8& SoldierVitalsComponent::maximumHealth() { return soldier_.stats.bLifeMax; }
const INT8& SoldierVitalsComponent::maximumHealth() const { return soldier_.stats.bLifeMax; }
INT8& SoldierVitalsComponent::breath() { return soldier_.bBreath; }
const INT8& SoldierVitalsComponent::breath() const { return soldier_.bBreath; }
INT8& SoldierVitalsComponent::maximumBreath() { return soldier_.bBreathMax; }
const INT8& SoldierVitalsComponent::maximumBreath() const { return soldier_.bBreathMax; }
INT8& SoldierVitalsComponent::bleeding() { return soldier_.bBleeding; }
const INT8& SoldierVitalsComponent::bleeding() const { return soldier_.bBleeding; }
bool SoldierVitalsComponent::alive() const { return health() > 0; }

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

INT32& SoldierPositionComponent::gridNo() { return soldier_.sGridNo; }
const INT32& SoldierPositionComponent::gridNo() const { return soldier_.sGridNo; }
INT8& SoldierPositionComponent::level() { return soldier_.pathing.bLevel; }
const INT8& SoldierPositionComponent::level() const { return soldier_.pathing.bLevel; }
UINT8& SoldierPositionComponent::direction() { return soldier_.ubDirection; }
const UINT8& SoldierPositionComponent::direction() const { return soldier_.ubDirection; }
