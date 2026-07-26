#include "Soldier Components.h"

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

void SoldierPositionComponent::reset() noexcept
{
	gridNo_ = 0;
	level_ = 0;
	direction_ = 0;
}
