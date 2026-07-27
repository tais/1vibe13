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

void SoldierTargetingComponent::selectLocation(
	INT32 gridNo, INT8 level, INT8 cubeLevel) noexcept
{
	gridNo_ = gridNo;
	level_ = level;
	cubeLevel_ = cubeLevel;
}

void SoldierTargetingComponent::reset() noexcept
{
	*this = SoldierTargetingComponent{};
}

void SoldierAttackSelectionComponent::selectWeapon(
	UINT8 hand, UINT16 weapon) noexcept
{
	hand_ = hand;
	weapon_ = weapon;
}

void SoldierAttackSelectionComponent::reset() noexcept
{
	*this = SoldierAttackSelectionComponent{};
}

void SoldierFireControlComponent::selectSingleShot() noexcept
{
	burstCounter_ = 0;
	autofireShots_ = 0;
}

void SoldierFireControlComponent::selectBurst() noexcept
{
	burstCounter_ = 1;
	autofireShots_ = 0;
}

void SoldierFireControlComponent::selectAutofire(UINT8 shots) noexcept
{
	burstCounter_ = 1;
	autofireShots_ = shots;
}

void SoldierFireControlComponent::clearSpreadTargets() noexcept
{
	std::fill_n(spreadLocations_, SpreadTargetCapacity, 0);
	spreadIndex_ = FALSE;
}

void SoldierFireControlComponent::reset() noexcept
{
	*this = SoldierFireControlComponent{};
}

void SoldierCombatResultComponent::recordHit(
	SoldierID attacker,
	UINT8 location) noexcept
{
	currentAttacker_ = attacker;
	hitLocation_ = location;
}

void SoldierCombatResultComponent::advanceAttackerHistory(
	bool retainCurrent) noexcept
{
	if (!hasCurrentAttacker())
		return;

	if (previousAttacker_ != currentAttacker_)
		earlierAttacker_ = previousAttacker_;

	previousAttacker_ = currentAttacker_;
	if (!retainCurrent)
		currentAttacker_ = NOBODY;
}

void SoldierCombatResultComponent::restorePreviousAttacker() noexcept
{
	if (previousAttacker_ != NOBODY)
		currentAttacker_ = previousAttacker_;
}

void SoldierCombatResultComponent::clearAttackers() noexcept
{
	currentAttacker_ = NOBODY;
	previousAttacker_ = NOBODY;
	earlierAttacker_ = NOBODY;
}

void SoldierCombatResultComponent::reset() noexcept
{
	*this = SoldierCombatResultComponent{};
}

void SoldierDamageDisplayComponent::restart() noexcept
{
	displayFlag_ = TRUE;
	counter_ = 0;
}

void SoldierDamageDisplayComponent::activateAt(
	INT16 offsetX,
	INT16 offsetY) noexcept
{
	restart();
	offsetX_ = offsetX;
	offsetY_ = offsetY;
}

void SoldierDamageDisplayComponent::advance() noexcept
{
	++counter_;
	++offsetX_;
	--offsetY_;
}

void SoldierDamageDisplayComponent::clear() noexcept
{
	displayFlag_ = FALSE;
	counter_ = 0;
}

void SoldierDamageDisplayComponent::reset() noexcept
{
	*this = SoldierDamageDisplayComponent{};
}

void SoldierAnimationIntentComponent::clearPendingAnimations() noexcept
{
	clearPendingAnimation();
	clearSecondaryPendingAnimation();
}

void SoldierAnimationIntentComponent::queueFacingAnimation(UINT16 animation, UINT8 direction) noexcept
{
	queueAnimation(animation);
	queueDirection(direction);
}

void SoldierAnimationIntentComponent::clearFacingAnimation() noexcept
{
	clearPendingAnimation();
	clearPendingDirection();
}

void SoldierAnimationIntentComponent::reset() noexcept
{
	*this = SoldierAnimationIntentComponent{};
}

void SoldierAnimationPlaybackComponent::reset() noexcept
{
	*this = SoldierAnimationPlaybackComponent{};
}

void SoldierAnimationActivityComponent::setInterruptibility(
	BOOLEAN nonInterruptible, BOOLEAN realtimeNonInterruptible) noexcept
{
	nonInterruptible_ = nonInterruptible;
	realtimeNonInterruptible_ = realtimeNonInterruptible;
}

void SoldierAnimationActivityComponent::clearInterruptibility() noexcept
{
	setInterruptibility(FALSE, FALSE);
}

void SoldierAnimationActivityComponent::beginFall(INT8 direction) noexcept
{
	fallDirection_ = direction;
	tryingToFall_ = TRUE;
}

void SoldierAnimationActivityComponent::reset() noexcept
{
	*this = SoldierAnimationActivityComponent{};
}
