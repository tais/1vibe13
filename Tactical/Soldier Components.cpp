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

void SoldierPathingComponent::clearRoute() noexcept
{
	pathSize_ = 0;
	pathIndex_ = 0;
	stored_ = 0;
}

void SoldierPathingComponent::reset() noexcept
{
	*this = SoldierPathingComponent{};
}

void SoldierMovementComponent::waitForGrid(INT32 gridNo, UINT8 counter) noexcept
{
	delayedCauseGrid_ = gridNo;
	delayCounter_ = counter;
}

void SoldierMovementComponent::blockInDirection(INT8 direction) noexcept
{
	blockedByAnotherMerc_ = TRUE;
	blockedDirection_ = direction;
}

void SoldierMovementComponent::setContinuedPath(INT32 gridNo) noexcept
{
	continuedPathGrid_ = gridNo;
	continuedPathValid_ = TRUE;
}

void SoldierMovementComponent::overrideMoveSpeedWith(SoldierID soldier) noexcept
{
	moveSpeedOverride_ = soldier;
	usesMoveSpeedOverride_ = TRUE;
}

void SoldierMovementComponent::reset() noexcept
{
	*this = SoldierMovementComponent{};
}
