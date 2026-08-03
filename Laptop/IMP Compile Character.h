#ifndef __IMP_COMPILE_H
#define __IMP_COMPILE_H

#define PLAYER_GENERATED_CHARACTER_ID 51
#define ATTITUDE_LIST_SIZE 30
#define NUMBER_OF_PLAYER_PORTRAITS 50 //16

void AddAnAttitudeToAttitudeList( INT8 bAttitude );
void CreatePlayerAttitude( void );
BOOLEAN CreateACharacterFromPlayerEnteredStats(INT32 profileId);
void CreatePlayerSkills( void );
void CreatePlayersPersonalitySkillsAndAttitude( void );
void AddAPersonalityToPersonalityList( INT8 bPersonlity );
void CreatePlayerPersonality( void );
void AddSkillToSkillList( INT8 bSkill );
void ResetSkillsAttributesAndPersonality( void );
void ResetIncrementCharacterAttributes( void );
void HandleMercStatsForChangesInFace( void );
void ClearAllSkillsList( void );

#endif
