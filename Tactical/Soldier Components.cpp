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

void SoldierActionPointComponent::beginTurn(INT16 points) noexcept
{
	current_ = points;
	initial_ = points;
}

void SoldierActionPointComponent::clear() noexcept
{
	current_ = 0;
	initial_ = 0;
}

void SoldierActionPointComponent::reset() noexcept
{
	*this = SoldierActionPointComponent{};
}

void SoldierCollapseComponent::recover() noexcept
{
	tactical_ = FALSE;
	turns_ = 0;
}

void SoldierCollapseComponent::reset() noexcept
{
	*this = SoldierCollapseComponent{};
}

bool SoldierPerceptionComponent::hasHeardMovementFrom(UINT8 direction) const noexcept
{
	return direction < 8 &&
		(movementNoiseDirections_ & (1u << direction)) != 0;
}

void SoldierPerceptionComponent::rememberMovementFrom(UINT8 direction) noexcept
{
	if (direction < 8)
	{
		movementNoiseDirections_ |= static_cast<UINT8>(1u << direction);
	}
}

bool SoldierPerceptionComponent::extendBlindnessToAtLeast(INT32 turns) noexcept
{
	if (blindnessTurns_ < turns)
	{
		blindnessTurns_ = static_cast<INT8>(turns);
		return true;
	}

	return false;
}

bool SoldierPerceptionComponent::ageBlindness() noexcept
{
	if (blindnessTurns_ <= 0)
	{
		return false;
	}

	--blindnessTurns_;
	return blindnessTurns_ == 0;
}

void SoldierPerceptionComponent::ageDeafness() noexcept
{
	if (deafnessTurns_ > 0)
	{
		--deafnessTurns_;
	}
}

void SoldierPerceptionComponent::reset() noexcept
{
	*this = SoldierPerceptionComponent{};
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

void SoldierSuppressionComponent::addPoints(UINT16 amount) noexcept
{
	// Preserve the established UINT8 accumulation semantics.
	points_ += amount;
}

void SoldierSuppressionComponent::recordBullet(SoldierID suppressor) noexcept
{
	addPoints(1);
	suppressor_ = suppressor;
}

void SoldierSuppressionComponent::addActionPointLoss(UINT16 amount) noexcept
{
	actionPointsLost_ = static_cast<UINT8>(
		std::min<UINT32>(
			255,
			static_cast<UINT32>(actionPointsLost_) + amount));
}

void SoldierSuppressionComponent::beginTurn() noexcept
{
	points_ = 0;
	actionPointsLost_ = 0;
	closeCall_ = FALSE;
}

void SoldierSuppressionComponent::reset() noexcept
{
	*this = SoldierSuppressionComponent{};
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
