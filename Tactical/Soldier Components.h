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

// Canonical tactical target selection. Attack execution, UI, AI, and network
// adapters all observe this same target geometry and identity; keeping it
// private prevents the legacy SOLDIERTYPE field list from becoming a second
// mutable authority.
class SoldierTargetingComponent
{
public:
	INT32& gridNo() noexcept { return gridNo_; }
	const INT32& gridNo() const noexcept { return gridNo_; }
	INT8& level() noexcept { return level_; }
	const INT8& level() const noexcept { return level_; }
	INT8& cubeLevel() noexcept { return cubeLevel_; }
	const INT8& cubeLevel() const noexcept { return cubeLevel_; }
	INT32& lastGridNo() noexcept { return lastGridNo_; }
	const INT32& lastGridNo() const noexcept { return lastGridNo_; }
	SoldierID& targetId() noexcept { return targetId_; }
	const SoldierID& targetId() const noexcept { return targetId_; }

	bool hasTargetSoldier() const noexcept { return targetId_ != NOBODY; }
	void selectLocation(INT32 gridNo, INT8 level, INT8 cubeLevel = 0) noexcept;
	void selectSoldier(SoldierID target) noexcept { targetId_ = target; }
	void clearTargetSoldier() noexcept { targetId_ = NOBODY; }
	void reset() noexcept;

private:
	INT32 gridNo_ = 0;
	INT8 level_ = 0;
	INT8 cubeLevel_ = 0;
	INT32 lastGridNo_ = 0;
	SoldierID targetId_ = NOBODY;
};

// Canonical weapon and aim selection for one tactical actor. Target geometry
// belongs to SoldierTargetingComponent; this component owns how that target is
// attacked so UI, AI, replay, and network adapters cannot diverge through
// independent flat SOLDIERTYPE fields.
class SoldierAttackSelectionComponent
{
public:
	UINT8& hand() noexcept { return hand_; }
	const UINT8& hand() const noexcept { return hand_; }
	UINT16& weapon() noexcept { return weapon_; }
	const UINT16& weapon() const noexcept { return weapon_; }
	INT8& weaponMode() noexcept { return weaponMode_; }
	const INT8& weaponMode() const noexcept { return weaponMode_; }
	INT8& scopeMode() noexcept { return scopeMode_; }
	const INT8& scopeMode() const noexcept { return scopeMode_; }
	UINT8& shotLocation() noexcept { return shotLocation_; }
	const UINT8& shotLocation() const noexcept { return shotLocation_; }
	UINT8& meleeLocation() noexcept { return meleeLocation_; }
	const UINT8& meleeLocation() const noexcept { return meleeLocation_; }

	void selectWeapon(UINT8 hand, UINT16 weapon) noexcept;
	void reset() noexcept;

private:
	UINT8 hand_ = 0;
	UINT16 weapon_ = 0;
	INT8 weaponMode_ = 0;
	INT8 scopeMode_ = 0;
	UINT8 shotLocation_ = 0;
	UINT8 meleeLocation_ = 0;
};

// Canonical state for selecting and executing one firing volley. Weapon and
// aim choice belong to SoldierAttackSelectionComponent; this component owns
// burst/autofire progress, spread targets, recoil history, and multi-barrel
// bookkeeping that must evolve together once firing begins.
class SoldierFireControlComponent
{
public:
	static constexpr UINT8 SpreadTargetCapacity = 6;
	using OffsetHistory = FLOAT[2];
	using SpreadLocations = INT32[SpreadTargetCapacity];

	INT8& burstCounter() noexcept { return burstCounter_; }
	const INT8& burstCounter() const noexcept { return burstCounter_; }
	UINT8& autofireShots() noexcept { return autofireShots_; }
	const UINT8& autofireShots() const noexcept { return autofireShots_; }
	INT8& bulletsLeft() noexcept { return bulletsLeft_; }
	const INT8& bulletsLeft() const noexcept { return bulletsLeft_; }
	BOOLEAN& spreadIndex() noexcept { return spreadIndex_; }
	const BOOLEAN& spreadIndex() const noexcept { return spreadIndex_; }
	BOOLEAN& autofireLastStep() noexcept { return autofireLastStep_; }
	const BOOLEAN& autofireLastStep() const noexcept { return autofireLastStep_; }
	SpreadLocations& spreadLocations() noexcept { return spreadLocations_; }
	const SpreadLocations& spreadLocations() const noexcept { return spreadLocations_; }
	OffsetHistory& previousMuzzleOffsetX() noexcept { return previousMuzzleOffsetX_; }
	const OffsetHistory& previousMuzzleOffsetX() const noexcept { return previousMuzzleOffsetX_; }
	OffsetHistory& previousMuzzleOffsetY() noexcept { return previousMuzzleOffsetY_; }
	const OffsetHistory& previousMuzzleOffsetY() const noexcept { return previousMuzzleOffsetY_; }
	OffsetHistory& previousCounterForceX() noexcept { return previousCounterForceX_; }
	const OffsetHistory& previousCounterForceX() const noexcept { return previousCounterForceX_; }
	OffsetHistory& previousCounterForceY() noexcept { return previousCounterForceY_; }
	const OffsetHistory& previousCounterForceY() const noexcept { return previousCounterForceY_; }
	FLOAT& initialMuzzleOffsetX() noexcept { return initialMuzzleOffsetX_; }
	const FLOAT& initialMuzzleOffsetX() const noexcept { return initialMuzzleOffsetX_; }
	FLOAT& initialMuzzleOffsetY() noexcept { return initialMuzzleOffsetY_; }
	const FLOAT& initialMuzzleOffsetY() const noexcept { return initialMuzzleOffsetY_; }
	UINT8& barrelCounter() noexcept { return barrelCounter_; }
	const UINT8& barrelCounter() const noexcept { return barrelCounter_; }

	bool bursting() const noexcept { return burstCounter_ != 0; }
	bool autofiring() const noexcept { return autofireShots_ != 0; }
	static constexpr UINT8 clampSpreadTargetCount(UINT16 requested) noexcept
	{
		return requested > SpreadTargetCapacity
			? SpreadTargetCapacity
			: static_cast<UINT8>(requested);
	}
	void selectSingleShot() noexcept;
	void selectBurst() noexcept;
	void selectAutofire(UINT8 shots = 1) noexcept;
	void clearSpreadTargets() noexcept;
	void reset() noexcept;

private:
	INT8 burstCounter_ = 0;
	UINT8 autofireShots_ = 0;
	INT8 bulletsLeft_ = 0;
	BOOLEAN spreadIndex_ = FALSE;
	BOOLEAN autofireLastStep_ = FALSE;
	SpreadLocations spreadLocations_{};
	OffsetHistory previousMuzzleOffsetX_{};
	OffsetHistory previousMuzzleOffsetY_{};
	OffsetHistory previousCounterForceX_{};
	OffsetHistory previousCounterForceY_{};
	FLOAT initialMuzzleOffsetX_ = 0.0f;
	FLOAT initialMuzzleOffsetY_ = 0.0f;
	UINT8 barrelCounter_ = 0;
};

// Canonical result of incoming combat. This keeps the current/previous
// attacker chain and hit metadata together without mixing it with outgoing
// target selection or damage-number presentation.
class SoldierCombatResultComponent
{
public:
	SoldierID& currentAttacker() noexcept { return currentAttacker_; }
	const SoldierID& currentAttacker() const noexcept { return currentAttacker_; }
	SoldierID& previousAttacker() noexcept { return previousAttacker_; }
	const SoldierID& previousAttacker() const noexcept { return previousAttacker_; }
	SoldierID& earlierAttacker() noexcept { return earlierAttacker_; }
	const SoldierID& earlierAttacker() const noexcept { return earlierAttacker_; }
	UINT8& hitLocation() noexcept { return hitLocation_; }
	const UINT8& hitLocation() const noexcept { return hitLocation_; }
	UINT8& lastDamageReason() noexcept { return lastDamageReason_; }
	const UINT8& lastDamageReason() const noexcept { return lastDamageReason_; }
	INT8& hitsThisTurn() noexcept { return hitsThisTurn_; }
	const INT8& hitsThisTurn() const noexcept { return hitsThisTurn_; }
	INT8& pelletsHitBy() noexcept { return pelletsHitBy_; }
	const INT8& pelletsHitBy() const noexcept { return pelletsHitBy_; }
	INT16& accumulatedDamage() noexcept { return accumulatedDamage_; }
	const INT16& accumulatedDamage() const noexcept { return accumulatedDamage_; }

	bool hasCurrentAttacker() const noexcept { return currentAttacker_ != NOBODY; }
	void recordHit(SoldierID attacker, UINT8 location) noexcept;
	void advanceAttackerHistory(bool retainCurrent) noexcept;
	void restorePreviousAttacker() noexcept;
	void clearAttackers() noexcept;
	void reset() noexcept;

private:
	SoldierID currentAttacker_ = NOBODY;
	SoldierID previousAttacker_ = NOBODY;
	SoldierID earlierAttacker_ = NOBODY;
	UINT8 hitLocation_ = 0;
	UINT8 lastDamageReason_ = 0;
	INT8 hitsThisTurn_ = 0;
	INT8 pelletsHitBy_ = 0;
	INT16 accumulatedDamage_ = 0;
};

// Canonical reaction to hostile fire. This state is consumed by both combat
// rules and AI decisions, so it is independent of generic AI scratch and
// presentation-only feedback from the most recent attack.
class SoldierSuppressionComponent
{
public:
	INT8& underFire() noexcept { return underFire_; }
	const INT8& underFire() const noexcept { return underFire_; }
	INT8& shock() noexcept { return shock_; }
	const INT8& shock() const noexcept { return shock_; }
	UINT8& points() noexcept { return points_; }
	const UINT8& points() const noexcept { return points_; }
	UINT8& actionPointsLost() noexcept { return actionPointsLost_; }
	const UINT8& actionPointsLost() const noexcept { return actionPointsLost_; }
	SoldierID& suppressor() noexcept { return suppressor_; }
	const SoldierID& suppressor() const noexcept { return suppressor_; }
	INT8& closeCall() noexcept { return closeCall_; }
	const INT8& closeCall() const noexcept { return closeCall_; }

	bool active() const noexcept { return points_ != 0; }
	bool hasSuppressor() const noexcept { return suppressor_ != NOBODY; }
	void addPoints(UINT16 amount) noexcept;
	void recordBullet(SoldierID suppressor) noexcept;
	void addActionPointLoss(UINT16 amount) noexcept;
	void markCloseCall() noexcept { closeCall_ = TRUE; }
	void clearCloseCall() noexcept { closeCall_ = FALSE; }
	void clearAttackPoints() noexcept { points_ = 0; }
	void beginTurn() noexcept;
	void reset() noexcept;

private:
	INT8 underFire_ = 0;
	INT8 shock_ = 0;
	UINT8 points_ = 0;
	UINT8 actionPointsLost_ = 0;
	SoldierID suppressor_ = NOBODY;
	INT8 closeCall_ = FALSE;
};

// Presentation payload for the floating tactical damage number. Simulation
// damage and attribution stay in SoldierCombatResultComponent; this state only
// tracks the animation cursor, screen offset, and display direction.
class SoldierDamageDisplayComponent
{
public:
	INT8& displayFlag() noexcept { return displayFlag_; }
	const INT8& displayFlag() const noexcept { return displayFlag_; }
	INT8& counter() noexcept { return counter_; }
	const INT8& counter() const noexcept { return counter_; }
	INT16& offsetX() noexcept { return offsetX_; }
	const INT16& offsetX() const noexcept { return offsetX_; }
	INT16& offsetY() noexcept { return offsetY_; }
	const INT16& offsetY() const noexcept { return offsetY_; }
	INT8& direction() noexcept { return direction_; }
	const INT8& direction() const noexcept { return direction_; }

	bool displaying() const noexcept { return displayFlag_ != 0; }
	bool expired() const noexcept { return counter_ >= 8; }
	void restart() noexcept;
	void activateAt(INT16 offsetX, INT16 offsetY) noexcept;
	void advance() noexcept;
	void clear() noexcept;
	void reset() noexcept;

private:
	INT8 displayFlag_ = 0;
	INT8 counter_ = 0;
	INT16 offsetX_ = 0;
	INT16 offsetY_ = 0;
	INT8 direction_ = 0;
};

// Canonical requests that bridge tactical decisions into animation playback.
// The playback state itself remains separate: this component owns only queued
// animations, stance/facing intent, and the movement continuation policy that
// must survive until the requested transition completes.
class SoldierAnimationIntentComponent
{
public:
	UINT8& desiredHeight() noexcept { return desiredHeight_; }
	const UINT8& desiredHeight() const noexcept { return desiredHeight_; }
	UINT16& pendingAnimation() noexcept { return pendingAnimation_; }
	const UINT16& pendingAnimation() const noexcept { return pendingAnimation_; }
	UINT8& pendingStance() noexcept { return pendingStance_; }
	const UINT8& pendingStance() const noexcept { return pendingStance_; }
	UINT16& secondaryPendingAnimation() noexcept { return secondaryPendingAnimation_; }
	const UINT16& secondaryPendingAnimation() const noexcept { return secondaryPendingAnimation_; }
	UINT8& pendingDirection() noexcept { return pendingDirection_; }
	const UINT8& pendingDirection() const noexcept { return pendingDirection_; }
	INT8& turningFromUi() noexcept { return turningFromUi_; }
	const INT8& turningFromUi() const noexcept { return turningFromUi_; }
	BOOLEAN& stopPendingNextTile() noexcept { return stopPendingNextTile_; }
	const BOOLEAN& stopPendingNextTile() const noexcept { return stopPendingNextTile_; }
	UINT8& continuationMode() noexcept { return continuationMode_; }
	const UINT8& continuationMode() const noexcept { return continuationMode_; }

	bool hasDesiredHeight() const noexcept { return desiredHeight_ != NoDesiredHeight; }
	bool hasPendingAnimation() const noexcept { return pendingAnimation_ != NoPendingAnimation; }
	bool hasPendingStance() const noexcept { return pendingStance_ != NoPendingStance; }
	bool hasSecondaryPendingAnimation() const noexcept { return secondaryPendingAnimation_ != NoPendingAnimation; }
	bool hasPendingDirection() const noexcept { return pendingDirection_ != NoPendingDirection; }
	bool continuesAfterStance() const noexcept { return continuationMode_ != 0; }

	void requestHeight(UINT8 height) noexcept { desiredHeight_ = height; }
	void clearDesiredHeight() noexcept { desiredHeight_ = NoDesiredHeight; }
	void queueAnimation(UINT16 animation) noexcept { pendingAnimation_ = animation; }
	void clearPendingAnimation() noexcept { pendingAnimation_ = NoPendingAnimation; }
	void queueStance(UINT8 stance) noexcept { pendingStance_ = stance; }
	void clearPendingStance() noexcept { pendingStance_ = NoPendingStance; }
	void queueSecondaryAnimation(UINT16 animation) noexcept { secondaryPendingAnimation_ = animation; }
	void clearSecondaryPendingAnimation() noexcept { secondaryPendingAnimation_ = NoPendingAnimation; }
	void queueDirection(UINT8 direction) noexcept { pendingDirection_ = direction; }
	void clearPendingDirection() noexcept { pendingDirection_ = NoPendingDirection; }
	void queueFacingAnimation(UINT16 animation, UINT8 direction) noexcept;
	void clearFacingAnimation() noexcept;
	void markTurningFromUi() noexcept { turningFromUi_ = TRUE; }
	void clearTurningFromUi() noexcept { turningFromUi_ = FALSE; }
	void requestStopAtNextTile() noexcept { stopPendingNextTile_ = TRUE; }
	void clearStopAtNextTile() noexcept { stopPendingNextTile_ = FALSE; }
	void continueAfterStance(UINT8 mode = 1) noexcept { continuationMode_ = mode; }
	void clearContinuation() noexcept { continuationMode_ = 0; }
	void clearPendingAnimations() noexcept;
	void reset() noexcept;

private:
	static constexpr UINT8 NoDesiredHeight = 255;
	static constexpr UINT16 NoPendingAnimation = 32001;
	static constexpr UINT8 NoPendingStance = 254;
	static constexpr UINT8 NoPendingDirection = 253;

	UINT8 desiredHeight_ = NoDesiredHeight;
	UINT16 pendingAnimation_ = NoPendingAnimation;
	UINT8 pendingStance_ = NoPendingStance;
	UINT16 secondaryPendingAnimation_ = NoPendingAnimation;
	UINT8 pendingDirection_ = NoPendingDirection;
	INT8 turningFromUi_ = FALSE;
	BOOLEAN stopPendingNextTile_ = FALSE;
	UINT8 continuationMode_ = 0;
};

// Canonical state of the animation currently being played. Transition
// requests belong to SoldierAnimationIntentComponent; this component owns the
// frame cursor, timing, previous-state bookkeeping, and render selection that
// advance after a request has been accepted.
class SoldierAnimationPlaybackComponent
{
public:
	UINT16& state() noexcept { return state_; }
	const UINT16& state() const noexcept { return state_; }
	UINT16& code() noexcept { return code_; }
	const UINT16& code() const noexcept { return code_; }
	UINT16& frame() noexcept { return frame_; }
	const UINT16& frame() const noexcept { return frame_; }
	INT16& delay() noexcept { return delay_; }
	const INT16& delay() const noexcept { return delay_; }
	UINT16& previousState() noexcept { return previousState_; }
	const UINT16& previousState() const noexcept { return previousState_; }
	INT16& previousCode() noexcept { return previousCode_; }
	const INT16& previousCode() const noexcept { return previousCode_; }
	UINT16& surface() noexcept { return surface_; }
	const UINT16& surface() const noexcept { return surface_; }
	UINT16& zLevel() noexcept { return zLevel_; }
	const UINT16& zLevel() const noexcept { return zLevel_; }
	UINT32& subFlags() noexcept { return subFlags_; }
	const UINT32& subFlags() const noexcept { return subFlags_; }

	bool isPlaying(UINT16 animation) const noexcept { return state_ == animation; }
	void reset() noexcept;

private:
	UINT16 state_ = 0;
	UINT16 code_ = 0;
	UINT16 frame_ = 0;
	INT16 delay_ = 0;
	UINT16 previousState_ = 0;
	INT16 previousCode_ = 0;
	UINT16 surface_ = 0;
	UINT16 zLevel_ = 0;
	UINT32 subFlags_ = 0;
};

// Runtime lifecycle surrounding accepted animation playback. Modes that must
// coordinate turning, hit/fall completion, interruption, and one-shot AP
// charging live here rather than in the generic soldier flag bucket.
class SoldierAnimationActivityComponent
{
public:
	INT8& turningFromProneMode() noexcept { return turningFromProneMode_; }
	const INT8& turningFromProneMode() const noexcept { return turningFromProneMode_; }
	BOOLEAN& readyCostWaived() noexcept { return readyCostWaived_; }
	const BOOLEAN& readyCostWaived() const noexcept { return readyCostWaived_; }
	INT8& postHitStance() noexcept { return postHitStance_; }
	const INT8& postHitStance() const noexcept { return postHitStance_; }
	BOOLEAN& paused() noexcept { return paused_; }
	const BOOLEAN& paused() const noexcept { return paused_; }
	BOOLEAN& holdAttackerUntilDone() noexcept { return holdAttackerUntilDone_; }
	const BOOLEAN& holdAttackerUntilDone() const noexcept { return holdAttackerUntilDone_; }
	BOOLEAN& turningToShoot() noexcept { return turningToShoot_; }
	const BOOLEAN& turningToShoot() const noexcept { return turningToShoot_; }
	BOOLEAN& turningToFall() noexcept { return turningToFall_; }
	const BOOLEAN& turningToFall() const noexcept { return turningToFall_; }
	BOOLEAN& turningUntilDone() noexcept { return turningUntilDone_; }
	const BOOLEAN& turningUntilDone() const noexcept { return turningUntilDone_; }
	UINT8& hitPhase() noexcept { return hitPhase_; }
	const UINT8& hitPhase() const noexcept { return hitPhase_; }
	BOOLEAN& nonInterruptible() noexcept { return nonInterruptible_; }
	const BOOLEAN& nonInterruptible() const noexcept { return nonInterruptible_; }
	BOOLEAN& turningCostWaived() noexcept { return turningCostWaived_; }
	const BOOLEAN& turningCostWaived() const noexcept { return turningCostWaived_; }
	BOOLEAN& suppressionStanceChange() noexcept { return suppressionStanceChange_; }
	const BOOLEAN& suppressionStanceChange() const noexcept { return suppressionStanceChange_; }
	BOOLEAN& stanceCostWaived() noexcept { return stanceCostWaived_; }
	const BOOLEAN& stanceCostWaived() const noexcept { return stanceCostWaived_; }
	BOOLEAN& realtimeNonInterruptible() noexcept { return realtimeNonInterruptible_; }
	const BOOLEAN& realtimeNonInterruptible() const noexcept { return realtimeNonInterruptible_; }
	INT8& tryingToFall() noexcept { return tryingToFall_; }
	const INT8& tryingToFall() const noexcept { return tryingToFall_; }
	BOOLEAN& fallClockwise() noexcept { return fallClockwise_; }
	const BOOLEAN& fallClockwise() const noexcept { return fallClockwise_; }
	INT8& fallDirection() noexcept { return fallDirection_; }
	const INT8& fallDirection() const noexcept { return fallDirection_; }
	INT8& turningIncrement() noexcept { return turningIncrement_; }
	const INT8& turningIncrement() const noexcept { return turningIncrement_; }

	bool gettingHit() const noexcept { return hitPhase_ != 0; }
	void beginHit() noexcept { hitPhase_ = 1; }
	void advanceHit() noexcept { hitPhase_ = 2; }
	void clearHit() noexcept { hitPhase_ = 0; }
	void pause() noexcept { paused_ = TRUE; }
	void resume() noexcept { paused_ = FALSE; }
	void setInterruptibility(BOOLEAN nonInterruptible, BOOLEAN realtimeNonInterruptible) noexcept;
	void clearInterruptibility() noexcept;
	void beginFall(INT8 direction) noexcept;
	void clearFall() noexcept { tryingToFall_ = FALSE; }
	void reset() noexcept;

private:
	INT8 turningFromProneMode_ = 0;
	BOOLEAN readyCostWaived_ = FALSE;
	INT8 postHitStance_ = 0;
	BOOLEAN paused_ = FALSE;
	BOOLEAN holdAttackerUntilDone_ = FALSE;
	BOOLEAN turningToShoot_ = FALSE;
	BOOLEAN turningToFall_ = FALSE;
	BOOLEAN turningUntilDone_ = FALSE;
	UINT8 hitPhase_ = 0;
	BOOLEAN nonInterruptible_ = FALSE;
	BOOLEAN turningCostWaived_ = FALSE;
	BOOLEAN suppressionStanceChange_ = FALSE;
	BOOLEAN stanceCostWaived_ = FALSE;
	BOOLEAN realtimeNonInterruptible_ = FALSE;
	INT8 tryingToFall_ = FALSE;
	BOOLEAN fallClockwise_ = FALSE;
	INT8 fallDirection_ = 0;
	INT8 turningIncrement_ = 0;
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
