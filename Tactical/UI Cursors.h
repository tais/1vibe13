#ifndef __UI_CURSORS_H
#define __UI_CURSORS_H


#define REFINE_PUNCH_1				0
#define REFINE_PUNCH_2				(gGameExternalOptions.fEnhancedCloseCombatSystem ? gSkillTraitValues.ubModifierForAPsAddedOnAimedPunches*2 : 6)

#define REFINE_KNIFE_1				0
#define REFINE_KNIFE_2				(gGameExternalOptions.fEnhancedCloseCombatSystem ? gSkillTraitValues.ubModifierForAPsAddedOnAimedBladedAttackes*2 : 6)


UINT8 GetProperItemCursor( SoldierID ubSoldierID, UINT16 ubItemIndex, INT32 usMapPos, BOOLEAN fActivated );
void DetermineCursorBodyLocation( SoldierID ubSoldierID, BOOLEAN fDisplay, BOOLEAN fRecalc );

void HandleLeftClickCursor( TacticalActor *pSoldier );
void HandleRightClickAdjustCursor( TacticalActor *pSoldier, INT32 usMapPos );
void HandleWheelAdjustCursor( TacticalActor *pSoldier, INT32 usMapPos, INT32 sDelta, INT16 brstmode );
void HandleWheelAdjustCursorWOAB( TacticalActor *pSoldier, INT32 sMapPos, INT32 sDelta );

UINT8 GetActionModeCursor( TacticalActor *pSoldier );

extern BOOLEAN gfCannotGetThrough;

void HandleUICursorRTFeedback( TacticalActor *pSoldier );
void HandleEndConfirmCursor( TacticalActor *pSoldier );

BOOLEAN GetMouseRecalcAndShowAPFlags( UINT32 *puiCursorFlags, BOOLEAN *pfShowAPs );

// HEADROCK HAM B2.7: This function calculates the nearest value (display purposes only) 
// based on how trained the shooter is.
UINT32 ChanceToHitApproximation( TacticalActor * pSoldier, UINT32 uiChance );

#endif
