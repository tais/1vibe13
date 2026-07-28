#include "Soldier Components.h"

#include <algorithm>
#include <limits>

bool SoldierVitalsComponent::alive() const noexcept { return health() > 0; }

void SoldierVitalsComponent::clearCriticalStatDamage() noexcept
{
	for (UINT8& damage : criticalStatDamage_)
	{
		damage = 0;
	}
}

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
	*this = SoldierVitalsComponent{};
}

void SoldierServiceComponent::addProvider() noexcept
{
	if (providerCount_ < std::numeric_limits<UINT8>::max())
	{
		++providerCount_;
	}
}

void SoldierServiceComponent::removeProvider() noexcept
{
	if (providerCount_ > 0)
	{
		--providerCount_;
	}
}

void SoldierServiceComponent::reset() noexcept
{
	*this = SoldierServiceComponent{};
}

void SoldierDialogueComponent::reset() noexcept
{
	*this = SoldierDialogueComponent{};
}

void SoldierAudioComponent::reset() noexcept
{
	*this = SoldierAudioComponent{};
}

void SoldierReplicationComponent::reset() noexcept
{
	*this = SoldierReplicationComponent{};
}

void SoldierMovementMetricsComponent::recordTileMovement(
	bool running, bool realtime, UINT16 animation) noexcept
{
	const INT16 distance = running ? 2 : 1;
	const INT16 updatedDistance = static_cast<INT16>(tilesMoved_) + distance;
	tilesMoved_ = static_cast<INT8>(
		updatedDistance > MaximumTurnTiles ? MaximumTurnTiles : updatedDistance);

	if (realtime)
	{
		if (realtimeBreathTiles_ < MaximumRealtimeBreathTiles)
		{
			++realtimeBreathTiles_;
		}
		lastRealtimeMovementAnimation_ = animation;
	}
}

void SoldierMovementMetricsComponent::reset() noexcept
{
	*this = SoldierMovementMetricsComponent{};
}

void SoldierAiPlanningComponent::recordFlankStep(
	INT32 anchorGrid, INT16 originDirection) noexcept
{
	if (flankAnchorGrid_ != anchorGrid)
	{
		clearFlank();
	}
	flankAnchorGrid_ = anchorGrid;
	flankOriginDirection_ = originDirection;
	advanceFlank();
}

void SoldierAiPlanningComponent::advanceFlank() noexcept
{
	if (flankCount_ < MaximumFlankCount)
	{
		++flankCount_;
	}
}

INT16 SoldierAiPlanningComponent::ensurePlanIndex(INT16 fallback) noexcept
{
	if (!hasPlanIndex())
	{
		planIndex_ = fallback;
	}
	return planIndex_;
}

void SoldierAiPlanningComponent::reset() noexcept
{
	*this = SoldierAiPlanningComponent{};
}

void SoldierSkillStateComponent::ageTurnCounters() noexcept
{
	for (UINT8 index = 0; index < SOLDIER_COUNTER_MAX; ++index)
	{
		if (index == SOLDIER_COUNTER_ROLE_OBSERVED)
		{
			continue;
		}

		if (index == SOLDIER_COUNTER_SPOTTER && counters_[index] > 0)
		{
			counters_[index] = static_cast<UINT16>(
				std::min<UINT32>(255, static_cast<UINT32>(counters_[index]) + 1));
		}
		else if (counters_[index] > 0)
		{
			--counters_[index];
		}
	}
}

void SoldierSkillStateComponent::reset() noexcept
{
	*this = SoldierSkillStateComponent{};
}

bool SoldierConditionComponent::hasExtraStats() const noexcept
{
	return extraStrength_ != 0 ||
	       extraDexterity_ != 0 ||
	       extraAgility_ != 0 ||
	       extraWisdom_ != 0 ||
	       extraExperienceLevel_ != 0;
}

bool SoldierConditionComponent::hasStarvationDamage() const noexcept
{
	return starvationHealthDamage_ != 0 || starvationStrengthDamage_ != 0;
}

bool SoldierConditionComponent::hasDisability(UINT8 disability) const noexcept
{
	if (disability == 0 || disability > DisabilityBitCount)
	{
		return false;
	}

	return (disabilityFlags_ & (UINT32{1} << (disability - 1))) != 0;
}

void SoldierConditionComponent::addDisability(UINT8 disability) noexcept
{
	if (disability == 0 || disability > DisabilityBitCount)
	{
		return;
	}

	disabilityFlags_ |= UINT32{1} << (disability - 1);
}

void SoldierConditionComponent::clearExtraStats() noexcept
{
	extraStrength_ = 0;
	extraDexterity_ = 0;
	extraAgility_ = 0;
	extraWisdom_ = 0;
	extraExperienceLevel_ = 0;
}

void SoldierConditionComponent::reset() noexcept
{
	*this = SoldierConditionComponent{};
}

void SoldierLongActionComponent::begin(
	UINT8 action, INT32 contextGrid, INT16 actionPoints) noexcept
{
	action_ = action;
	contextGrid_ = contextGrid;
	remainingActionPoints_ = std::max<INT16>(0, actionPoints);
}

void SoldierLongActionComponent::consumeActionPoints(INT16 actionPoints) noexcept
{
	if (actionPoints <= 0)
	{
		return;
	}

	remainingActionPoints_ =
		std::max<INT16>(0, remainingActionPoints_ - actionPoints);
}

void SoldierLongActionComponent::clear() noexcept
{
	remainingActionPoints_ = 0;
	contextGrid_ = NoContextGrid;
	action_ = 0;
}

void SoldierLongActionComponent::reset() noexcept
{
	*this = SoldierLongActionComponent{};
}

void SoldierInteractionComponent::dragPerson(SoldierID soldier) noexcept
{
	clearDrag();
	draggedPerson_ = soldier;
}

void SoldierInteractionComponent::dragCorpse(INT16 corpse) noexcept
{
	clearDrag();
	if (corpse >= 0)
	{
		draggedCorpse_ = corpse;
	}
}

void SoldierInteractionComponent::dragStructure(INT32 grid) noexcept
{
	clearDrag();
	if (grid >= 0)
	{
		draggedStructureGrid_ = grid;
	}
}

void SoldierInteractionComponent::copyDragFrom(
	const SoldierInteractionComponent& source) noexcept
{
	draggedPerson_ = source.draggedPerson_;
	draggedCorpse_ = source.draggedCorpse_;
	draggedStructureGrid_ = source.draggedStructureGrid_;
}

void SoldierInteractionComponent::clearDrag() noexcept
{
	draggedPerson_ = NOBODY;
	draggedCorpse_ = -1;
	draggedStructureGrid_ = NoGrid;
}

void SoldierInteractionComponent::reset() noexcept
{
	*this = SoldierInteractionComponent{};
}

void SoldierPendingActionComponent::clearPayload() noexcept
{
	primaryData_ = 0;
	secondaryData_ = 0;
	tertiaryData_ = 0;
	doorHandleCode_ = 0;
	quaternaryData_ = 0;
	inventorySlot_ = 0;
}

void SoldierPendingActionComponent::recordAnimationTransition() noexcept
{
	if (animationCount_ < std::numeric_limits<UINT8>::max())
	{
		++animationCount_;
	}
}

void SoldierPendingActionComponent::reset() noexcept
{
	*this = SoldierPendingActionComponent{};
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

void SoldierAwarenessComponent::reset() noexcept
{
	*this = SoldierAwarenessComponent{};
}

INT8 SoldierCamouflageComponent::total(Terrain terrain) const noexcept
{
	INT16 value = 0;
	switch (terrain)
	{
		case Terrain::Jungle:
			value = jungleApplied_ + jungleWorn_;
			break;
		case Terrain::Urban:
			value = urbanApplied_ + urbanWorn_;
			break;
		case Terrain::Desert:
			value = desertApplied_ + desertWorn_;
			break;
		case Terrain::Snow:
			value = snowApplied_ + snowWorn_;
			break;
	}

	return static_cast<INT8>(std::clamp<INT16>(value, -100, 100));
}

INT8 SoldierCamouflageComponent::strongestTotal() const noexcept
{
	return std::max<INT8>(
		0,
		std::max(
			std::max(total(Terrain::Jungle), total(Terrain::Urban)),
			std::max(total(Terrain::Desert), total(Terrain::Snow))));
}

INT16 SoldierCamouflageComponent::appliedTotal() const noexcept
{
	return jungleApplied_ + urbanApplied_ + desertApplied_ + snowApplied_;
}

void SoldierCamouflageComponent::reset() noexcept
{
	*this = SoldierCamouflageComponent{};
}

void SoldierEmploymentComponent::reset() noexcept
{
	*this = SoldierEmploymentComponent{};
}

void SoldierAssignmentComponent::reset() noexcept
{
	*this = SoldierAssignmentComponent{};
}

void SoldierDeploymentComponent::reset() noexcept
{
	*this = SoldierDeploymentComponent{};
}

void SoldierScheduleComponent::advanceProgress() noexcept
{
	if (progress_ < std::numeric_limits<INT8>::max())
	{
		++progress_;
	}
}

void SoldierScheduleComponent::beginDoorContinuation(INT32 gridNo) noexcept
{
	doorOpenPhase_ = 1;
	doorGrid_ = gridNo;
}

void SoldierScheduleComponent::completeDoorAnimation() noexcept
{
	if (doorOpenPhase_ == 1)
	{
		doorOpenPhase_ = 2;
	}
}

INT32 SoldierScheduleComponent::consumeDoorGrid() noexcept
{
	doorOpenPhase_ = 0;
	return doorGrid_;
}

void SoldierScheduleComponent::reset() noexcept
{
	*this = SoldierScheduleComponent{};
}

void SoldierPositionComponent::setWorldCoordinates(FLOAT x, FLOAT y) noexcept
{
	worldX_ = x;
	worldY_ = y;
	worldXInt_ = static_cast<INT16>(x);
	worldYInt_ = static_cast<INT16>(y);
}

void SoldierPositionComponent::recordTurnStart(INT16 x, INT16 y) noexcept
{
	turnStartX_ = x;
	turnStartY_ = y;
}

void SoldierPositionComponent::enterTerrain(INT8 terrainType) noexcept
{
	previousTerrainType_ = terrainType_;
	terrainType_ = terrainType;
}

void SoldierPositionComponent::reset() noexcept
{
	*this = SoldierPositionComponent{};
}

void SoldierMovementHistoryComponent::resetAiLoop() noexcept
{
	recentLocations_[0] = NoGrid;
	recentLocations_[1] = NoGrid;
}

bool SoldierMovementHistoryComponent::observeAiMovement(
	INT32 currentGrid, INT32 destinationGrid, INT32 gridCount) noexcept
{
	const auto isOutOfBounds = [gridCount](INT32 gridNo) noexcept
	{
		return gridNo < 0 || gridNo >= gridCount;
	};

	if (isOutOfBounds(recentLocations_[0]))
	{
		recentLocations_[0] = currentGrid;
		return false;
	}
	if (isOutOfBounds(recentLocations_[1]))
	{
		recentLocations_[1] = currentGrid;
		return false;
	}
	if (destinationGrid == recentLocations_[1] &&
		currentGrid == recentLocations_[0])
	{
		return true;
	}

	recentLocations_[0] = recentLocations_[1];
	recentLocations_[1] = currentGrid;
	return false;
}

void SoldierMovementHistoryComponent::reset() noexcept
{
	*this = SoldierMovementHistoryComponent{};
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

void SoldierCombatContributionComponent::recordMilitiaKill() noexcept
{
	if (militiaKills_ < std::numeric_limits<UINT8>::max())
	{
		++militiaKills_;
	}
}

void SoldierCombatContributionComponent::recordMilitiaAssist() noexcept
{
	if (militiaAssists_ < std::numeric_limits<UINT8>::max())
	{
		++militiaAssists_;
	}
}

void SoldierCombatContributionComponent::clearMilitiaCredit() noexcept
{
	militiaKills_ = 0;
	militiaAssists_ = 0;
}

void SoldierCombatContributionComponent::reset() noexcept
{
	*this = SoldierCombatContributionComponent{};
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
