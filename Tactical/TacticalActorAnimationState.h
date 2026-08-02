#pragma once

// Sentinels stored by SoldierAnimationIntentComponent. Keep these values
// stable because they are also consumed by legacy animation code.
enum
{
	NO_PENDING_ANIMATION = 32001,
	NO_PENDING_DIRECTION = 253,
	NO_PENDING_STANCE = 254,
	NO_DESIRED_HEIGHT = 255,
};

// SoldierMovementComponent grid-update suppression policy used while a
// moving actor transitions to or from prone.
enum
{
	LOCKED_NO_NEWGRIDNO = 2,
};

// SoldierAnimationActivityComponent::turningFromProneMode values.
enum
{
	TURNING_FROM_PRONE_OFF = 0,
	TURNING_FROM_PRONE_ON,
	TURNING_FROM_PRONE_START_UP_FROM_MOVE,
	TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE,
	TURNING_FROM_PRONE_FOR_PUNCH_OR_STAB,
};

// Stance selected after a hit reaction completes.
enum
{
	NO_SPEC_STANCE_AFTER_HIT = 0,
	GO_TO_AIM_AFTER_HIT,
	GO_TO_ALTERNATIVE_AIM_AFTER_HIT,
	GO_TO_HTH_BREATH_AFTER_HIT,
	GO_TO_COWERING_AFTER_HIT,
};
