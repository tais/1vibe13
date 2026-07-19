#ifndef TACTICAL_MOVEMENT_DESTINATION_POLICY_H
#define TACTICAL_MOVEMENT_DESTINATION_POLICY_H

#include "Animation Control.h"

// The caller combines the legacy option's runtime eligibility (turn-based
// combat, player merc, and conscious state) into fPolicyEnabled. Animation
// metadata is explicit so this destination decision remains side-effect-free.
inline BOOLEAN ShouldRetainMovementAnimationAtDestination(
	BOOLEAN fPolicyEnabled, UINT16 usAnimState, const ANIMCONTROLTYPE& animation)
{
	if ( !fPolicyEnabled )
	{
		return FALSE;
	}

	BOOLEAN fEligibleAnimation = FALSE;
	switch ( usAnimState )
	{
		case WALKING:
		case CROUCHING:
		case SWATTING:
		case RUNNING:
		case CRAWLING:
		case END_HURT_WALKING:
		case SWAT_BACKWARDS:
		case SWATTING_WK:
		case SWAT_BACKWARDS_WK:
		case SWAT_BACKWARDS_NOTHING:
		case RUNNING_W_PISTOL:
		case SIDE_STEP_WEAPON_RDY:
		case SIDE_STEP_DUAL_RDY:
		case SIDE_STEP_CROUCH_RIFLE:
		case SIDE_STEP_CROUCH_PISTOL:
		case SIDE_STEP_CROUCH_DUAL:
		case WALKING_WEAPON_RDY:
		case WALKING_DUAL_RDY:
		case WALKING_ALTERNATIVE_RDY:
		case SIDE_STEP_ALTERNATIVE_RDY:
		case CROUCHEDMOVE_RIFLE_READY:
		case CROUCHEDMOVE_PISTOL_READY:
		case CROUCHEDMOVE_DUAL_READY:
			fEligibleAnimation = TRUE;
			break;
	}

	if ( !fEligibleAnimation )
	{
		return FALSE;
	}

	// Retaining crouched locomotion freezes the merc on an arbitrary stride
	// frame. Unlike running, crouched movement has no restart AP surcharge, so
	// let it transition into its stationary crouch (through END_SWAT where
	// applicable) instead.
	return !( ( animation.uiFlags & ANIM_MOVING ) && animation.ubEndHeight == ANIM_CROUCH );
}

#endif
