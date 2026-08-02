#ifndef __SOLDER_CONTROL_H
#define __SOLDER_CONTROL_H

#include "Animation Cache.h"
#include "Timer Control.h"
#include "vobject.h"
#include "Overhead Types.h"
#include "Item Types.h"
#include "worlddef.h"
#include <vector>
#include <iterator>
#include "GameSettings.h"	// added by Flugente
#include "Disease.h"		// added by Flugente
#include "Grid Direction.h"
#include "Soldier Background Types.h"
#include "Soldier Class.h"
#include "Soldier Palette.h"
#include "Soldier Patrol Types.h"
#include "Soldier Profile Constants.h"
#include "Soldier Profile.h"
#include "Strategic Path Types.h"
#include "TacticalActor.h"
#include "TacticalActorAnimationState.h"
#include "TacticalActorBattleSounds.h"
#include "TacticalActorBloodState.h"
#include "TacticalActorConsumables.h"
#include "TacticalActorDamageFeedback.h"
#include "TacticalActorDamageResolution.h"
#include "TacticalActorEmploymentTypes.h"
#include "TacticalActorEvents.h"
#include "TacticalActorLighting.h"
#include "TacticalActorLongActions.h"
#include "TacticalActorInterrupts.h"
#include "TacticalActorMovementState.h"
#include "TacticalActorPendingActionTypes.h"
#include "TacticalActorQuoteFlags.h"
#include "TacticalActorSkills.h"
#include "TacticalActorStateFlags.h"

static_assert(MAXPATROLGRIDS == SOLDIER_PATROL_GRID_COUNT,
	"Soldier patrol storage must retain the established save-schema capacity");

#define PTR_CIVILIAN	(pSoldier->roster().team() == CIV_TEAM)
#define PTR_CROUCHED	(gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight == ANIM_CROUCH)
#define PTR_STANDING	(gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight == ANIM_STAND)
#define PTR_PRONE	 (gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight == ANIM_PRONE)

#define DRUG_TYPE_MAX	32

// TEMP VALUES FOR NAMES
#define MAXCIVLASTNAMES		30
extern UINT16 CivLastNames[MAXCIVLASTNAMES][10];

// ANDREW: these are defines for OKDestanation usage - please move to approprite file
#define IGNOREPEOPLE	0
#define PEOPLETOO		1
#define ALLPEOPLE		2
#define FALLINGTEST	 3

#define SOLDIER_UNBLIT_SIZE			(75*75*2)

/*
#define	SOLDIER_TRAIT_LOCKPICKING		0x0001
#define	SOLDIER_TRAIT_HANDTOHAND		0x0002
#define	SOLDIER_TRAIT_ELECTRONICS		0x0004
#define	SOLDIER_TRAIT_NIGHTOPS			0x0008
#define	SOLDIER_TRAIT_THROWING			0x0010
#define	SOLDIER_TRAIT_TEACHING			0x0020
#define	SOLDIER_TRAIT_HEAVY_WEAPS		0x0040
#define	SOLDIER_TRAIT_AUTO_WEAPS		0x0080
#define	SOLDIER_TRAIT_STEALTHY			0x0100
#define	SOLDIER_TRAIT_AMBIDEXT			0x0200
#define	SOLDIER_TRAIT_THIEF					0x0400
#define	SOLDIER_TRAIT_MARTIALARTS		0x0800
#define	SOLDIER_TRAIT_KNIFING				0x1000
*/
#define SOLDIER_MISC_HEARD_GUNSHOT										0x01
// make sure soldiers (esp tanks) are not hurt multiple times by explosions
#define SOLDIER_MISC_HURT_BY_EXPLOSION								0x02
// should be revealed due to xrays
#define SOLDIER_MISC_XRAYED														0x04

#define BANDAGED( s ) (s->vitals().maximumHealth() - s->vitals().health() - s->vitals().bleeding())

// amount of time a stats is to be displayed differently, due to change
#define CHANGE_STAT_RECENTLY_DURATION		60000

// MACROS
// #######################################################

#define NO_PENDING_ACTION			SoldierPendingActionComponent::NoAction

#define MAX_FULLTILE_DIRECTIONS 3
static_assert(
	SoldierFrontArcComponent::DirectionCount == MAX_FULLTILE_DIRECTIONS,
	"front-arc component capacity must retain the established soldier schema");

// DIGICRAB: Burst UnCap. Keep the legacy spelling as a source-compatible
// alias; persistent capacity is now owned by SoldierFireControlComponent.
#define MAX_BURST_SPREAD_TARGETS SoldierFireControlComponent::SpreadTargetCapacity

// anv: externalised taunts
// taunt properties
// attitudes
#define TAUNT_A_CUNNING_SOLO						0x0000000000000001	//1
#define TAUNT_A_CUNNING_AID							0x0000000000000002	//2
#define TAUNT_A_BRAVE_SOLO							0x0000000000000004	//4
#define TAUNT_A_BRAVE_AID							0x0000000000000008	//8

#define TAUNT_A_AGGRESSIVE							0x0000000000000010	//16
#define TAUNT_A_DEFENSIVE							0x0000000000000020	//32

// situations
// actions
#define TAUNT_S_FIRE_GUN		   					0x0000000000000040	//64
#define TAUNT_S_FIRE_LAUNCHER						0x0000000000000080	//128
#define TAUNT_S_ATTACK_BLADE						0x0000000000000100	//256
#define TAUNT_S_ATTACK_HTH							0x0000000000000200	//512

#define TAUNT_S_THROW_KNIFE							0x0000000000000400	//1024
#define TAUNT_S_THROW_GRENADE						0x0000000000000800	//2048

#define TAUNT_S_OUT_OF_AMMO							0x0000000000001000	//4096
#define TAUNT_S_RELOAD								0x0000000000002000	//8192

#define TAUNT_S_STEAL								0x0000000000004000	//16384

// AI routines
#define TAUNT_S_CHARGE_BLADE						0x0000000000008000	//32768
#define TAUNT_S_CHARGE_HTH							0x0000000000010000	//65536
#define TAUNT_S_RUN_AWAY							0x0000000000020000	//131072
#define TAUNT_S_SEEK_NOISE							0x0000000000040000	//262144
#define TAUNT_S_ALERT								0x0000000000080000	//...
#define TAUNT_S_SUSPICIOUS							0x0000000000100000
#define TAUNT_S_NOTICED_UNSEEN						0x0000000000200000	//
#define TAUNT_S_SAY_HI								0x0000000000400000	//
#define	TAUNT_S_INFORM_ABOUT						0x0000000000800000

// got_hit_xxx
#define TAUNT_S_GOT_HIT								0x0000000001000000	//
#define TAUNT_S_GOT_HIT_GUNFIRE						0x0000000002000000	//
#define TAUNT_S_GOT_HIT_BLADE						0x0000000004000000	//
#define TAUNT_S_GOT_HIT_HTH							0x0000000008000000	//
#define TAUNT_S_GOT_HIT_FALLROOF					0x0000000010000000	//
#define TAUNT_S_GOT_HIT_BLOODLOSS					0x0000000020000000	//
#define TAUNT_S_GOT_HIT_EXPLOSION					0x0000000040000000	//
#define TAUNT_S_GOT_HIT_GAS							0x0000000080000000	//
#define TAUNT_S_GOT_HIT_TENTACLES					0x0000000100000000	//
#define TAUNT_S_GOT_HIT_STRUCTURE_EXPLOSION			0x0000000200000000	//
#define TAUNT_S_GOT_HIT_OBJECT						0x0000000400000000	//
#define TAUNT_S_GOT_HIT_THROWING_KNIFE				0x0000000800000000	//

#define TAUNT_S_GOT_DEAFENED						0x0000001000000000	//
#define TAUNT_S_GOT_BLINDED							0x0000002000000000	//

#define TAUNT_S_GOT_ROBBED							0x0000004000000000	//

// got_missed_xxx
#define TAUNT_S_GOT_MISSED							0x0000008000000000	//
#define TAUNT_S_GOT_MISSED_GUNFIRE					0x0000010000000000	//
#define TAUNT_S_GOT_MISSED_BLADE					0x0000020000000000	//
#define TAUNT_S_GOT_MISSED_HTH						0x0000040000000000	//
#define TAUNT_S_GOT_MISSED_THROWING_KNIFE			0x0000080000000000	//

// hit_xxx
#define TAUNT_S_HIT									0x0000100000000000	//
#define TAUNT_S_HIT_GUNFIRE							0x0000200000000000	//
#define TAUNT_S_HIT_BLADE							0x0000400000000000	//
#define TAUNT_S_HIT_HTH								0x0000800000000000	//
#define TAUNT_S_HIT_EXPLOSION						0x0001000000000000	//
#define TAUNT_S_HIT_THROWING_KNIFE					0x0002000000000000	//

// kill_xxx
#define TAUNT_S_KILL								0x0004000000000000	//
#define TAUNT_S_KILL_GUNFIRE						0x0008000000000000	//
#define TAUNT_S_KILL_BLADE							0x0010000000000000	//
#define TAUNT_S_KILL_HTH							0x0020000000000000	//
#define TAUNT_S_KILL_THROWING_KNIFE					0x0040000000000000	//
#define TAUNT_S_HEAD_POP							0x0080000000000000	//

// miss_xxx
#define TAUNT_S_MISS								0x0100000000000000	//
#define TAUNT_S_MISS_GUNFIRE						0x0200000000000000	//
#define TAUNT_S_MISS_BLADE							0x0400000000000000	//
#define TAUNT_S_MISS_HTH							0x0800000000000000	//
#define TAUNT_S_MISS_THROWING_KNIFE					0x1000000000000000	//

// NEW FLAGS, starting from the beginning (UINT128 is redundant? yeah, right)

// class
#define TAUNT_C_ADMIN		   						0x0000000000000004	//4
#define TAUNT_C_ARMY		   						0x0000000000000008	//8
#define TAUNT_C_ELITE								0x0000000000000010	//16
#define TAUNT_C_GREEN								0x0000000000000020	//32
#define TAUNT_C_REGULAR		   						0x0000000000000040	//64
#define TAUNT_C_VETERAN								0x0000000000000080	//128

// sex
#define TAUNT_G_MALE								0x0000000000000100	//256
#define TAUNT_G_FEMALE								0x0000000000000200	//512

// target
#define TAUNT_T_MALE								0x0000000000000400	//1024
#define TAUNT_T_FEMALE								0x0000000000000800	//2048

#define TAUNT_T_ZOMBIE								0x0000000000001000	//4096

#define TAUNT_FLAG_1_MAX	64
#define TAUNT_FLAG_2_MAX	13
#define TAUNT_FLAG_MAX	TAUNT_FLAG_1_MAX + TAUNT_FLAG_2_MAX

// Flugente: a structure for clothing items
typedef struct
{
	UINT16			uiIndex;
	CHAR16			szName[80];				// name of these clothes
	PaletteRepID	vest;
	PaletteRepID	pants;
} CLOTHES_STRUCT;

#define CLOTHES_MAX	50

extern CLOTHES_STRUCT Clothes[CLOTHES_MAX];

// This macro should be used whenever we want to see if someone is neutral
// IF WE ARE CONSIDERING ATTACKING THEM.	Creatures & bloodcats will attack neutrals
// but they can't attack empty vehicles!!
// the_bob: also, creatures won't attack crows, because it seems to confuse the AI and cause freezes
#define CONSIDERED_NEUTRAL( me, them )  (\
										(them->aiBehavior().neutral() || them->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV|SOLDIER_COVERT_SOLDIER|SOLDIER_POW)) \
										&& (me->roster().team() != CREATURE_TEAM || (them->status().flags() & SOLDIER_VEHICLE) || (them->identity().bodyType() == CROW)) \
										&& !(me->status().flags() & SOLDIER_BOXER && them->status().flags() & SOLDIER_BOXER) \
										)

enum
{
	HIT_BY_TEARGAS = 0x01,
	HIT_BY_MUSTARDGAS = 0x02,
	HIT_BY_CREATUREGAS = 0x04,
	HIT_BY_BURNABLEGAS = 0x08,
	HIT_BY_SMOKEGAS = 0x10,//dnl ch40 200909
};


//ADB makes the code clearer, used like "thisSoldier->foo();"
//CHRISL: Not sure if it make the code easier to read or not, but it does make it harder to debug
//#define thisSoldier this

enum class BackgroundVectorTypes;

#define HEALTH_INCREASE			0x0001
#define STRENGTH_INCREASE		0x0002
#define	DEX_INCREASE				0x0004
#define AGIL_INCREASE				0x0008
#define WIS_INCREASE				0x0010
#define LDR_INCREASE				0x0020

#define MRK_INCREASE				0x0040
#define MED_INCREASE				0x0080
#define EXP_INCREASE				0x0100
#define MECH_INCREASE				0x0200

#define LVL_INCREASE				0x0400




// Moved to weapons.h by ADB, rev 1513
/*enum WeaponModes
{
	WM_NORMAL = 0,
	WM_BURST,
	WM_AUTOFIRE,
	WM_ATTACHED_GL,
	WM_ATTACHED_GL_BURST,
	WM_ATTACHED_GL_AUTO,
	NUM_WEAPON_MODES
} ;
*/
// Globals
//////////

extern UINT8					bHealthStrRanges[];


// Functions
////////////

// Soldier Management functions called by Overhead.c
void RevivePlayerTeam( );


// UTILITY FUNCTUIONS
void MoveMercFacingDirection( TacticalActor *pSoldier, BOOLEAN fReverse, FLOAT dMovementDist );
// This function is now obsolete.	Call ReduceAttackBusyCount instead.
// void ReleaseSoldiersAttacker( TacticalActor *pSoldier );



BOOLEAN PreloadSoldierBattleSounds( TacticalActor *pSoldier, BOOLEAN fRemove );
void CrowsFlyAway( UINT8 ubTeam );
void DebugValidateSoldierData( );
// added by Flugente
BOOLEAN MajorTrait( UINT8 uiSkillTraitNumber );							// determine if this is a major trait

//typedef struct


// SANDRO - This whole procedure was merged with the surgery ability of the doctor trait

#endif
