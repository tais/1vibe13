#include "Strategic/Strategic Path Types.h"
#include "Tactical/Animation Data.h"
#include "Tactical/Soldier Drug Types.h"
#include "Tactical/Soldier Background Types.h"
#include "Tactical/Soldier Palette.h"
#include "Tactical/Soldier Patrol Types.h"
#include "Tactical/Soldier Profile Constants.h"
#include "Tactical/TacticalActorAnimationState.h"
#include "Tactical/TacticalActorBattleSounds.h"
#include "Tactical/TacticalActorBloodState.h"
#include "Tactical/TacticalActorDamageResolution.h"
#include "Tactical/TacticalActorEmploymentTypes.h"
#include "Tactical/TacticalActorLongActions.h"
#include "Tactical/TacticalActorMovementState.h"
#include "Tactical/TacticalActorPendingActionTypes.h"
#include "Tactical/TacticalActorQuoteFlags.h"
#include "Tactical/TacticalActorSkills.h"
#include "Tactical/TacticalActorStateFlags.h"

#include <cstddef>
#include <type_traits>

static_assert(std::is_standard_layout_v<PathSt>);
static_assert(std::is_same_v<decltype(PathSt::uiSectorId), UINT32>);
static_assert(std::is_same_v<decltype(PathSt::uiEta), UINT32>);
static_assert(std::is_same_v<decltype(PathSt::fSpeed), BOOLEAN>);
static_assert(std::is_same_v<decltype(PathSt::pNext), path*>);
static_assert(std::is_same_v<decltype(PathSt::pPrev), path*>);
static_assert(OLD_MAXPATROLGRIDS == 10);
static_assert(MAXPATROLGRIDS == SOLDIER_PATROL_GRID_COUNT);
static_assert(NO_PROFILE == 200);
static_assert(DRUG_EFFECT_HP == 0);
static_assert(DRUG_EFFECT_WIS == 8);
static_assert(DRUG_EFFECT_MAX == 20);
static_assert(NUM_MERC_BATTLE_SOUNDS == 16);
static_assert(BATTLE_SND_LOWER_VOLUME == 1);
static_assert(BACKGROUND_FLAG_MAX == 12);
static_assert(BACKGROUND_XENOPHOBIC == 0x2);
static_assert(UNIFORM_ENEMY_ADMIN == 0);
static_assert(UNIFORM_MILITIA_ELITE == 5);
static_assert(NUM_UNIFORMS == 6);
static_assert(std::is_same_v<decltype(UNIFORMCOLORS::vest), PaletteRepID>);
static_assert(std::is_same_v<decltype(UNIFORMCOLORS::pants), PaletteRepID>);
static_assert(std::is_standard_layout_v<ANIM_PROF_TILE>);
static_assert(std::is_same_v<decltype(ANIM_PROF_DIR::pTiles), ANIM_PROF_TILE*>);
static_assert(std::extent_v<decltype(ANIM_PROF::Dirs)> == 8);
static_assert(MERC_TYPE__PLAYER_CHARACTER == 0);
static_assert(MERC_TYPE__VEHICLE == 6);
static_assert(SOLDIER_CONTRACT_RENEW_QUOTE_115_USED == 2);
static_assert(NO_PENDING_ANIMATION == 32001);
static_assert(NO_PENDING_DIRECTION == 253);
static_assert(NO_PENDING_STANCE == 254);
static_assert(NO_DESIRED_HEIGHT == 255);
static_assert(LOCKED_NO_NEWGRIDNO == 2);
static_assert(TURNING_FROM_PRONE_OFF == 0);
static_assert(TURNING_FROM_PRONE_FOR_PUNCH_OR_STAB == 4);
static_assert(NO_SPEC_STANCE_AFTER_HIT == 0);
static_assert(GO_TO_COWERING_AFTER_HIT == 4);
static_assert(MAXBLOOD == 40);
static_assert(NOBLOOD == MAXBLOOD);
static_assert(BLOODTIME == 5);
static_assert(FOOTPRINTTIME == 2);
static_assert(MIN_BLEEDING_THRESHOLD == 12);
static_assert(TAKE_DAMAGE_GUNFIRE == 1);
static_assert(TAKE_DAMAGE_GAS_NOTFIRE == 13);
static_assert(MTA_NONE == 0);
static_assert(MTA_HACK == 3);
static_assert(NUM_MTA == 4);
static_assert(DELAYED_MOVEMENT_FLAG_PATH_THROUGH_PEOPLE == 0x01);
static_assert(REASON_STOPPED_NO_APS == 0);
static_assert(REASON_STOPPED_SIGHT == 1);
static_assert(MERC_OPENDOOR == 0);
static_assert(MERC_GIVEITEM == 6);
static_assert(MERC_MEDICALSPLINT == 23);
static_assert(NO_THROW_ACTION == 0);
static_assert(THROW_ARM_ITEM == 1);
static_assert(THROW_TARGET_MERC_CATCH == 2);
static_assert(SOLDIER_QUOTE_SAID_IN_SHIT == 0x0001);
static_assert(SOLDIER_QUOTE_SAID_FOUND_SOMETHING_NICE == 0x8000);
static_assert(SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL == 0x0008);
static_assert(SOLDIER_QUOTE_SAID_EXT_MORRIS == SOLDIER_QUOTE_SAID_EXT_MIKE);
static_assert(SOLDIER_QUOTE_SAID_BUDDY_6_WITNESSED == 0x1000);
static_assert(SKILLS_RADIO_ARTILLERY == 0);
static_assert(SKILLS_INTEL_FIRST == 7);
static_assert(SKILLS_DISGUISE_FIRST == 12);
static_assert(SKILLS_VARIOUS_FIRST == 16);
static_assert(SKILLS_FILL_CANTEENS == 19);
static_assert(SKILLS_MAX == 20);
static_assert(SOLDIER_PCUNDERAICONTROL == 0x20);
static_assert(SOLDIER_ROBOT == 0x1000);
static_assert(SOLDIER_VEHICLE == 0x8000);
static_assert(SOLDIER_COVERT_CIV == 0x4);
static_assert(SOLDIER_COVERT_NPC_SPECIAL == 0x20);

int main()
{
	return 0;
}
