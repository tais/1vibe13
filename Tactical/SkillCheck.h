#ifndef __SKILL_CHECK_H
#define __SKILL_CHECK_H

#include "types.h"
class TacticalActor;

void ReducePointsForFatigue( TacticalActor *pSoldier, UINT32 *pusPoints );
void ReducePointsForFatigue( TacticalActor *pSoldier, INT32 *psPoints );
INT32 GetSkillCheckPenaltyForFatigue( TacticalActor *pSoldier, INT32 iSkill );
INT32 SkillCheck( TacticalActor *pSoldier, INT8 bReason, INT8 bDifficulty );
INT16 CalcTrapDetectLevel( TacticalActor *pSoldier, BOOLEAN fExamining );

INT16 EffectiveStrength( TacticalActor *pSoldier, BOOLEAN fTrainer );
INT16 EffectiveWisdom( TacticalActor *pSoldier );
INT16 EffectiveAgility( TacticalActor *pSoldier, BOOLEAN fTrainer );
INT8 EffectiveMechanical( TacticalActor *pSoldier );
INT8 EffectiveExplosive( TacticalActor *pSoldier );
INT8 EffectiveLeadership( TacticalActor *pSoldier );
INT8 EffectiveMarksmanship( TacticalActor *pSoldier );
INT16 EffectiveDexterity( TacticalActor *pSoldier, BOOLEAN fTrainer );
INT8 EffectiveExpLevel( TacticalActor *pSoldier, BOOLEAN fTactical = TRUE );
INT8 EffectiveMedical( TacticalActor *pSoldier );

enum SkillChecks
{
	NO_CHECK = 0,
	LOCKPICKING_CHECK,
	ELECTRONIC_LOCKPICKING_CHECK,
	ATTACHING_DETONATOR_CHECK,
	ATTACHING_REMOTE_DETONATOR_CHECK,
	PLANTING_BOMB_CHECK,
	PLANTING_REMOTE_BOMB_CHECK,
	OPEN_WITH_CROWBAR,
	SMASH_DOOR_CHECK,
	DISARM_TRAP_CHECK,
	UNJAM_GUN_CHECK,
	NOTICE_DART_CHECK,
	LIE_TO_QUEEN_CHECK,
	ATTACHING_SPECIAL_ITEM_CHECK,
	ATTACHING_SPECIAL_ELECTRONIC_ITEM_CHECK,
	DISARM_ELECTRONIC_TRAP_CHECK,
	ATTACH_POWER_PACK,					// Flugente: attach a power pack to power armor
	PLANTING_MECHANICAL_BOMB_CHECK,
	DISARM_MECHANICAL_TRAP_CHECK,
} ;



#endif
