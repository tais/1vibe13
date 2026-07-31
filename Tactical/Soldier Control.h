#ifndef __SOLDER_CONTROL_H
#define __SOLDER_CONTROL_H

//dnl ch33 200909
// In the future MAXPATROLDGRIDS could be externalized but his value must always be >= OLD_MAXPATROLGRIDS
#define OLD_MAXPATROLGRIDS	10
#define MAXPATROLGRIDS		OLD_MAXPATROLGRIDS

// WANNE: Yes I know, we support up to 254 profiles, but because of compatibility, profile Id = 200
// is not a valid profil. We in MercProfiles.xml, the profile id = 200 should not be used!
#define	NO_PROFILE			200

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
#include "Render Palette Bank.h"
#include "Soldier Components.h"
#include "Soldier Inventory.h"

static_assert(MAXPATROLGRIDS == SOLDIER_PATROL_GRID_COUNT,
	"Soldier patrol storage must retain the established save-schema capacity");

#define PTR_CIVILIAN	(pSoldier->roster().team() == CIV_TEAM)
#define PTR_CROUCHED	(gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight == ANIM_CROUCH)
#define PTR_STANDING	(gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight == ANIM_STAND)
#define PTR_PRONE	 (gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight == ANIM_PRONE)

#define DRUG_TYPE_MAX	32
#define FOOD_TYPE_MAX	500

// TEMP VALUES FOR NAMES
#define MAXCIVLASTNAMES		30
extern UINT16 CivLastNames[MAXCIVLASTNAMES][10];

// ANDREW: these are defines for OKDestanation usage - please move to approprite file
#define IGNOREPEOPLE	0
#define PEOPLETOO		1
#define ALLPEOPLE		2
#define FALLINGTEST	 3

#define	LOCKED_NO_NEWGRIDNO			2

#define	BATTLE_SND_LOWER_VOLUME		1

#define	TAKE_DAMAGE_GUNFIRE				1
#define	TAKE_DAMAGE_BLADE					2
#define	TAKE_DAMAGE_HANDTOHAND		3
#define TAKE_DAMAGE_FALLROOF			4
#define TAKE_DAMAGE_BLOODLOSS			5
#define TAKE_DAMAGE_EXPLOSION			6
#define TAKE_DAMAGE_ELECTRICITY		7
#define TAKE_DAMAGE_GAS_FIRE			8
#define TAKE_DAMAGE_TENTACLES			9
#define TAKE_DAMAGE_STRUCTURE_EXPLOSION 10
#define TAKE_DAMAGE_OBJECT		11
#define TAKE_DAMAGE_VEHICLE_TRAUMA		12
#define TAKE_DAMAGE_GAS_NOTFIRE			13


#define SOLDIER_UNBLIT_SIZE			(75*75*2)

#define	SOLDIER_IS_TACTICALLY_VALID					0x00000001
#define SOLDIER_SHOULD_BE_TACTICALLY_VALID	0x00000002
#define SOLDIER_MULTI_SELECTED							0x00000004
#define SOLDIER_PC													0x00000008
#define SOLDIER_ATTACK_NOTICED							0x00000010
#define SOLDIER_PCUNDERAICONTROL						0x00000020
#define SOLDIER_UNDERAICONTROL							0x00000040
#define SOLDIER_DEAD												0x00000080
#define SOLDIER_GREEN_RAY										0x00000100
#define SOLDIER_LOOKFOR_ITEMS								0x00000200
#define SOLDIER_ENEMY												0x00000400
#define SOLDIER_ENGAGEDINACTION							0x00000800
#define SOLDIER_ROBOT												0x00001000
#define SOLDIER_MONSTER											0x00002000
#define SOLDIER_ANIMAL											0x00004000
#define SOLDIER_VEHICLE											0x00008000
#define SOLDIER_MULTITILE_NZ								0x00010000
#define SOLDIER_Z								0x00010000
#define SOLDIER_MULTITILE_Z									0x00020000
#define SOLDIER_MULTITILE										( SOLDIER_MULTITILE_Z | SOLDIER_MULTITILE_NZ )
#define SOLDIER_RECHECKLIGHT								0x00040000
#define SOLDIER_TURNINGFROMHIT							0x00080000
#define SOLDIER_BOXER												0x00100000
#define SOLDIER_LOCKPENDINGACTIONCOUNTER		0x00200000
#define SOLDIER_COWERING										0x00400000
#define SOLDIER_MUTE												0x00800000
#define SOLDIER_GASSED											0x01000000
#define SOLDIER_OFF_MAP											0x02000000
#define SOLDIER_PAUSEANIMOVE								0x04000000
#define SOLDIER_DRIVER											0x08000000
#define SOLDIER_PASSENGER										0x10000000
#define SOLDIER_NPC_DOING_PUNCH							0x20000000
#define SOLDIER_NPC_SHOOTING								0x40000000
#define SOLDIER_LOOK_NEXT_TURNSOLDIER				0x80000000


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
// SANDRO was here, messed this..
//#define HAS_SKILL_TRAIT( s, t ) (s->stats.ubSkillTrait1 == t || s->stats.ubSkillTrait2 == t)
//#define NUM_SKILL_TRAITS( s, t ) ( (s->stats.ubSkillTrait1 == t) ? ( (s->stats.ubSkillTrait2 == t) ? 2 : 1 ) : ( (s->stats.ubSkillTrait2 == t) ? 1 : 0 ) )
BOOLEAN HAS_SKILL_TRAIT( TacticalActor * pSoldier, UINT8 uiSkillTraitNumber );
INT8 NUM_SKILL_TRAITS( TacticalActor * pSoldier, UINT8 uiSkillTraitNumber );

#define	SOLDIER_QUOTE_SAID_IN_SHIT										0x0001
#define	SOLDIER_QUOTE_SAID_LOW_BREATH									0x0002
#define	SOLDIER_QUOTE_SAID_BEING_PUMMELED							0x0004
#define	SOLDIER_QUOTE_SAID_NEED_SLEEP									0x0008
#define	SOLDIER_QUOTE_SAID_LOW_MORAL									0x0010
#define	SOLDIER_QUOTE_SAID_MULTIPLE_CREATURES					0x0020
#define SOLDIER_QUOTE_SAID_ANNOYING_MERC							0x0040
#define SOLDIER_QUOTE_SAID_LIKESGUN										0x0080
#define SOLDIER_QUOTE_SAID_DROWNING										0x0100
#define SOLDIER_QUOTE_SAID_ROTTINGCORPSE							0x0200
#define SOLDIER_QUOTE_SAID_SPOTTING_CREATURE_ATTACK		0x0400
#define SOLDIER_QUOTE_SAID_SMELLED_CREATURE						0x0800
#define SOLDIER_QUOTE_SAID_ANTICIPATING_DANGER				0x1000
#define SOLDIER_QUOTE_SAID_WORRIED_ABOUT_CREATURES		0x2000
#define SOLDIER_QUOTE_SAID_PERSONALITY								0x4000
#define SOLDIER_QUOTE_SAID_FOUND_SOMETHING_NICE				0x8000

#define SOLDIER_QUOTE_SAID_EXT_HEARD_SOMETHING				0x0001
#define SOLDIER_QUOTE_SAID_EXT_SEEN_CREATURE_ATTACK		0x0002
#define SOLDIER_QUOTE_SAID_EXT_USED_BATTLESOUND_HIT		0x0004
#define SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL							0x0008

#define SOLDIER_QUOTE_SAID_EXT_MORRIS									0x0010 //Ja25 UB
#define SOLDIER_QUOTE_SAID_EXT_MIKE										0x0010
static_assert(
	SOLDIER_QUOTE_SAID_EXT_MORRIS ==
	SOLDIER_QUOTE_SAID_EXT_MIKE);

#define SOLDIER_QUOTE_SAID_DONE_ASSIGNMENT						0x0020
#define SOLDIER_QUOTE_SAID_BUDDY_1_WITNESSED					0x0040
#define SOLDIER_QUOTE_SAID_BUDDY_2_WITNESSED					0x0080
#define SOLDIER_QUOTE_SAID_BUDDY_3_WITNESSED					0x0100
#define SOLDIER_QUOTE_SAID_BUDDY_4_WITNESSED					0x0400
#define SOLDIER_QUOTE_SAID_BUDDY_5_WITNESSED					0x0800
#define SOLDIER_QUOTE_SAID_BUDDY_6_WITNESSED					0x1000

#define	SOLDIER_QUOTE_SAID_THOUGHT_KILLED_YOU					0x0200


#define	SOLDIER_CONTRACT_RENEW_QUOTE_NOT_USED					0
#define	SOLDIER_CONTRACT_RENEW_QUOTE_89_USED					1
#define	SOLDIER_CONTRACT_RENEW_QUOTE_115_USED					2


#define SOLDIER_MISC_HEARD_GUNSHOT										0x01
// make sure soldiers (esp tanks) are not hurt multiple times by explosions
#define SOLDIER_MISC_HURT_BY_EXPLOSION								0x02
// should be revealed due to xrays
#define SOLDIER_MISC_XRAYED														0x04

#define MAXBLOOD										40
#define NOBLOOD											MAXBLOOD
#define BLOODTIME										5
#define FOOTPRINTTIME								2
#define MIN_BLEEDING_THRESHOLD			12		// you're OK while <4 Yellow life bars

#define BANDAGED( s ) (s->vitals().maximumHealth() - s->vitals().health() - s->vitals().bleeding())

// amount of time a stats is to be displayed differently, due to change
#define CHANGE_STAT_RECENTLY_DURATION		60000

// MACROS
// #######################################################

#define NO_PENDING_ACTION			SoldierPendingActionComponent::NoAction
#define NO_PENDING_ANIMATION	32001
#define NO_PENDING_DIRECTION	253
#define NO_PENDING_STANCE			254
#define NO_DESIRED_HEIGHT			255

#define MAX_FULLTILE_DIRECTIONS 3
static_assert(
	SoldierFrontArcComponent::DirectionCount == MAX_FULLTILE_DIRECTIONS,
	"front-arc component capacity must retain the established soldier schema");

// DIGICRAB: Burst UnCap. Keep the legacy spelling as a source-compatible
// alias; persistent capacity is now owned by SoldierFireControlComponent.
#define MAX_BURST_SPREAD_TARGETS SoldierFireControlComponent::SpreadTargetCapacity

#define		TURNING_FROM_PRONE_OFF						0
#define		TURNING_FROM_PRONE_ON						1	
#define		TURNING_FROM_PRONE_START_UP_FROM_MOVE		2
#define		TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE		3
#define		TURNING_FROM_PRONE_FOR_PUNCH_OR_STAB		4//dnl ch73 290913

//ENUMERATIONS FOR ACTIONS
enum
{
	MERC_OPENDOOR,
	MERC_OPENSTRUCT,
	MERC_PICKUPITEM,
	MERC_PUNCH,
	MERC_KNIFEATTACK,
	MERC_GIVEAID,
	MERC_GIVEITEM,
	MERC_WAITFOROTHERSTOTRIGGER,
	MERC_CUTFFENCE,
	MERC_DROPBOMB,
	MERC_STEAL,
	MERC_TALK,
	MERC_ENTER_VEHICLE,
	MERC_REPAIR,
	MERC_RELOADROBOT,
	MERC_TAKEBLOOD,
	MERC_ATTACH_CAN,
	MERC_FUEL_VEHICLE,
	MERC_BUILD_FORTIFICATION,
	MERC_HANDCUFF_PERSON,
	MERC_APPLYITEM,
	MERC_INTERACTIVEACTION,
	MERC_FILLBLOODBAG,
	MERC_MEDICALSPLINT,
};

// ENUMERATIONS FOR THROW ACTIONS
enum
{
	NO_THROW_ACTION,
	THROW_ARM_ITEM,
	THROW_TARGET_MERC_CATCH,
};

// An enumeration for playing battle sounds
enum
{
	BATTLE_SOUND_OK1,
	BATTLE_SOUND_COOL1,
	BATTLE_SOUND_CURSE1,
	BATTLE_SOUND_HIT1,
	BATTLE_SOUND_LAUGH1,
	BATTLE_SOUND_ATTN1,
	BATTLE_SOUND_DIE1,
	BATTLE_SOUND_HUMM,
	BATTLE_SOUND_NOTHING,
	BATTLE_SOUND_GOTIT,
	BATTLE_SOUND_LOWMARALE_OK1,
	BATTLE_SOUND_LOWMARALE_ATTN1,
	BATTLE_SOUND_LOCKED,
	BATTLE_SOUND_ENEMY,
	BATTLE_SOUND_PUNCH,				// Flugente: attacking with punch attack
	BATTLE_SOUND_KNIFE,				// Flugente: attacking with knife attack
	NUM_MERC_BATTLE_SOUNDS
};


//different kinds of merc
enum
{
	MERC_TYPE__PLAYER_CHARACTER,
	MERC_TYPE__AIM_MERC,
	MERC_TYPE__MERC,
	MERC_TYPE__NPC,
	MERC_TYPE__EPC,
	MERC_TYPE__NPC_WITH_UNEXTENDABLE_CONTRACT,
	MERC_TYPE__VEHICLE,
};

// SANDRO - this is for determining what stance to go back after being hit
enum
{
	NO_SPEC_STANCE_AFTER_HIT,
	GO_TO_AIM_AFTER_HIT,
	GO_TO_ALTERNATIVE_AIM_AFTER_HIT,
	GO_TO_HTH_BREATH_AFTER_HIT,
	GO_TO_COWERING_AFTER_HIT,
};

// vehicle/human path structure
struct path
{
	UINT32 uiSectorId;
	UINT32 uiEta;
	BOOLEAN fSpeed;
	struct path *pNext;
	struct path *pPrev;
};



typedef struct path PathSt;
typedef PathSt *PathStPtr;

//used for color codes, but also shows the enemy type for debugging purposes
enum
{
	SOLDIER_CLASS_NONE,
	SOLDIER_CLASS_ADMINISTRATOR,
	SOLDIER_CLASS_ELITE,
	SOLDIER_CLASS_ARMY,
	SOLDIER_CLASS_GREEN_MILITIA,
	SOLDIER_CLASS_REG_MILITIA,
	SOLDIER_CLASS_ELITE_MILITIA,
	SOLDIER_CLASS_CREATURE,
	SOLDIER_CLASS_MINER,
	SOLDIER_CLASS_ZOMBIE,
	SOLDIER_CLASS_TANK,
	SOLDIER_CLASS_JEEP,
	SOLDIER_CLASS_BANDIT,
	SOLDIER_CLASS_ROBOT,
	SOLDIER_CLASS_MAX,
};

// Flugente: there are now separate gun choices, depending on a soldier's class
#define SOLDIER_GUN_CHOICE_SELECTIONS SOLDIER_CLASS_CREATURE

#define SOLDIER_CLASS_ENEMY( bSoldierClass )		( ( bSoldierClass >= SOLDIER_CLASS_ADMINISTRATOR ) && ( bSoldierClass <= SOLDIER_CLASS_ARMY ) )
#define SOLDIER_CLASS_MILITIA( bSoldierClass )	( ( bSoldierClass >= SOLDIER_CLASS_GREEN_MILITIA ) && ( bSoldierClass <= SOLDIER_CLASS_ELITE_MILITIA ) )

// Types of uniforms available
enum
{
	UNIFORM_ENEMY_ADMIN = 0,
	UNIFORM_ENEMY_TROOP,
	UNIFORM_ENEMY_ELITE,
	UNIFORM_MILITIA_ROOKIE,
	UNIFORM_MILITIA_REGULAR,
	UNIFORM_MILITIA_ELITE,
	NUM_UNIFORMS,
};

// -------- added by Flugente: various flags for soldiers --------
// easier than adding 32 differently named variables. DO NOT CHANGE THEM, UNLESS YOU KNOW WHAT YOU ARE DOING!!!
#define SOLDIER_DRUGGED						0x00000001	//1			// Soldier is on (non-alcoholic) drugs
#define SOLDIER_NO_AP						0x00000002	//2			// Soldier has no APs this turn (fix for reinforcement bug)
#define SOLDIER_COVERT_CIV					0x00000004	//4			// Soldier is currently disguised as a civilian
#define SOLDIER_COVERT_SOLDIER				0x00000008	//8			// Soldier is currently disguised as an enemy soldier

#define SOLDIER_DAMAGED_VEST				0x00000010	//16		// Soldier's vest is damaged (and thus can't be taken off)
#define SOLDIER_COVERT_NPC_SPECIAL			0x00000020	//32		// Special flag for NPCs when recruited (used for covert stuff)
#define SOLDIER_NEW_VEST   					0x00000040	//64		// Soldier is wearing new vest. if having both vest and pants, he can disguise
#define SOLDIER_NEW_PANTS					0x00000080	//128		// Soldier is wearing new pants

#define SOLDIER_DAMAGED_PANTS				0x00000100	//256		// Soldier's vest is damaged (and thus can't be taken off)
#define SOLDIER_HEADSHOT					0x00000200	//512		// last hit received was a headshot (attack to the head, so knifes/punches also work)
#define SOLDIER_POW							0x00000400	//1024		// we are a prisoner of war
#define SOLDIER_ASSASSIN					0x00000800	//2048		// we are an enemy assassin, and thus we will behave very different from normal enemies (not set on Kingpin's assassins intentionally)

#define SOLDIER_POW_PRISON					0x00001000	//4096		// this guy is a prisoner of war in a prison sector. SOLDIER_POW refers to people we capture, this refers to people we hold captive
#define SOLDIER_EQUIPMENT_DROPPED			0x00002000	//8192		// under certain circumstances, militia can be ordered to drop their gear twice. Thus we set a marker to avoid that.
#define SOLDIER_ACCESSTEAMMEMBER			0x00004000	//16384		// this merc is accessing another team member'S inventory (via abusing the stealing mechanic)
#define SOLDIER_REDOFLASHLIGHT				0x00008000	//32768		// this flag signifies that we somehow interacted with the items in our hands. Thus we have to possible redo lighting from flashlights

#define SOLDIER_LIGHT_OWNER					0x00010000	//65536		// we 'own' at least one light source (via flashlights)
#define SOLDIER_AIRDROP_TURN				0x00020000	//131072	// we are entering a sector via airdrop this turn
#define SOLDIER_ASSAULT_BONUS				0x00040000	//262144	// backgrounds: our first turn in an assault
#define SOLDIER_RADIO_OPERATOR_LISTENING	0x00080000	//524288	// radio operator is listening with his set

#define SOLDIER_RADIO_OPERATOR_JAMMING		0x00100000	//1048576	// radio operator is jamming frequencies
#define SOLDIER_RADIO_OPERATOR_SCANNING		0x00200000	//2097152	// radio operator is scanning for jammers
#define SOLDIER_AIRDROP						0x00400000	//4194304	// soldier is entering the sector via airdrop from a helicopter. Slightly different from SOLDIER_AIRDROP_TURN
#define SOLDIER_FRESHWOUND					0x00800000	//8388608	// campaign stats: soldier was wounded in this battle

#define SOLDIER_BATTLE_PARTICIPATION		0x01000000	//16777216	// campaign stats: soldier took part in this battle
#define SOLDIER_RAISED_REDALERT				0x02000000	//33554432	// this (AI) soldier has raised red alert. Don't allow him to do so again this turn - either it already worked, or the signal is blocked
#define SOLDIER_ENEMY_OFFICER				0x04000000	//67108864	// soldier is an enemy officer
#define SOLDIER_ENEMY_OBSERVEDTHISTURN		0x08000000	//134217728 // enemy soldier was seen by the player this turn

#define SOLDIER_VIP							0x10000000	//268435456	// soldier is a VIP - the player will likely try to assassinate him
#define SOLDIER_BODYGUARD					0x20000000	//536870912 // soldier is a bodyguard for a VIP
#define SOLDIER_COVERT_TEMPORARY_OVERT		0x40000000	//1073741824	// we are covert, but just performed a obviously suspicious task. For a short time, we can be uncovered more easily
#define SOLDIER_MOVEITEM_RESTRICTED			0x80000000	//2147483648	// when moving item, this soldier will not pick up equipment the militia might use
// ----------------------------------------------------------------

// ------------------- more flags for soldiers --------------------
#define SOLDIER_SNITCHING_OFF				0x00000001	//1				// isn't allowed to snitch
#define SOLDIER_PREVENT_MISBEHAVIOUR_OFF	0x00000002	//2				// isn't allowed to prevent misbehaviour
#define SOLDIER_RAM_THROUGH_OBSTACLES		0x00000004	//4				// vehicle
#define SOLDIER_INTERROGATE_ADMIN			0x00000008	//8				// interrogate admins. Flags might not be the best solution, but I won't add an extra variable for this

#define SOLDIER_INTERROGATE_TROOP			0x00000010	//16			// interrogate troops
#define SOLDIER_INTERROGATE_ELITE			0x00000020	//32			// interrogate elites
#define SOLDIER_INTERROGATE_OFFICER			0x00000040	//64			// interrogate officers
#define SOLDIER_INTERROGATE_GENERAL			0x00000080	//128			// interrogate generals

#define SOLDIER_INTERROGATE_CIVILIAN		0x00000100	//256			// interrogate civilian
#define SOLDIER_POTENTIAL_VOLUNTEER			0x00000200	//512			// this civilian _might_ join us as a volunteer if conditions are right
#define SOLDIER_HUNGOVER					0x00000400	//1024			// we drank alcohol recently, and are now hungover
#define SOLDIER_TAKEN_LARGE_HIT				0x00000800					// we recently received a lot of damage in a single hit

#define SOLDIER_COVERT_NOREDISGUISE			0x00001000					// this soldier does not want to be redisguised
#define SOLDIER_TRAIT_FOCUS					0x00002000					// 'focus' skill is active
#define SOLDIER_BAYONET_RUNBONUS			0x00004000					// we are performing a bayonet attack after transitioning from running, giving our attack extra force
#define SOLDIER_CONCEALINSERTION			0x00008000					// we enteri a sector by transition from concealed state (which causes us to spawn at the location we left the sector in)

#define SOLDIER_CONCEALINSERTION_DISCOVERED	0x00010000					// we enter a sector by transition from concealed state, but as we were 'discovered', set red alert
#define SOLDIER_MERC_POW_LOCATIONKNOWN		0x00020000					// we are a POW, but the player has discovered our location
#define SOLDIER_SURGERY_BOOSTED				0x00040000					// we are a boosted performing surgery (e.g. by using up a blood bag)

#define SOLDIER_DRAG_SOUND					0x00080000					// played sound when started dragging
#define SOLDIER_SPENT_AP					0x00100000					// soldier has spent some AP this turn (including realtime)
#define SOLDIER_TURNCOAT					0x00200000					// this enemy soldier will switch to the militia team if ordered to
#define SOLDIER_BACK_ATTACK					0x00400000					// soldier was attacked from the back
#define SOLDIER_SNEAK_ATTACK				0x00800000					// soldier was attacked by unseen enemy

#define SOLDIER_INTERROGATE_ALL				0x000001F8					// all interrogation flags
// ----------------------------------------------------------------

// -------- added by Flugente: background property flags --------
// easier than adding 32 differently named variables. DO NOT CHANGE THEM, UNLESS YOU KNOW WHAT YOU ARE DOING!!!
// a merc's background info reveals data about his previous life, like former regiments. These backgrounds add small abilities/disabilities. Nothing substantial, just small bits do
// diversify your mercs and add more personality
#define BACKGROUND_DRUGUSE						0x0000000000000001	//1				// might use drugs on his own (the 'Larry'-effect)
#define BACKGROUND_XENOPHOBIC					0x0000000000000002	//2				// arrogant towards others without this background
#define BACKGROUND_EXP_UNDERGROUND				0x0000000000000004	//4				// extra level in underground sectors
#define BACKGROUND_SCROUNGING					0x0000000000000008	//8				// might pick up valuable items on his own

#define BACKGROUND_TRAPLEVEL					0x0000000000000010	//16			// trap level +1
#define BACKGROUND_CORRUPTIONSPREAD				0x0000000000000020	//32			// spreads corruption to others	- not used in trunk!
#define BACKGROUND_NO_MALE   					0x0000000000000040	//64			// background cannot be selected by males (IMP creation)
#define BACKGROUND_NO_FEMALE					0x0000000000000080	//128			// background cannot be selected by females (IMP creation)

#define BACKGROUND_GLOBALOYALITYLOSSONDEATH		0x0000000000000100	//256			// if character dies, huge loyalty loss in entire country

#define BACKGROUND_ANIMALFRIEND					0x0000000000000200	//512			// refuses to attack animals
#define BACKGROUND_CIVGROUPLOYAL				0x0000000000000400	//1024			// refuses to attack members of the same civgroup
#define BACKGROUND_ALT_IMP_CREATION				0x0000000000000800	//2048			// BG can only be used when ALT_IMP_CREATION is TRUE in ja2options.ini (IMP creation)

#define BACKGROUND_FLAG_MAX	12					// number of flagged backgrounds - keep this updated, or properties will get lost!

// some properties are hidden (forbid background in MP creation)
// corruption property is not relevant in 1.13
#define BACKGROUND_HIDDEN_FLAGS					(BACKGROUND_NO_MALE|BACKGROUND_NO_FEMALE|BACKGROUND_CORRUPTIONSPREAD|BACKGROUND_ALT_IMP_CREATION)

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

// Flugente: types of multi-turn actions
enum
{
	MTA_NONE = 0,
	MTA_FORTIFY,
	MTA_REMOVE_FORTIFY,
	MTA_HACK,
	NUM_MTA,
};

//Flugente skills from traits and other sources
enum{
	// first skill
	SKILLS_FIRST = 0,
	
	// radio operator
	SKILLS_RADIO_FIRST = SKILLS_FIRST,
	SKILLS_RADIO_ARTILLERY = SKILLS_RADIO_FIRST,
	SKILLS_RADIO_JAM,
	SKILLS_RADIO_SCAN_FOR_JAM,
	SKILLS_RADIO_LISTEN,
	SKILLS_RADIO_CALLREINFORCEMENTS,
	SKILLS_RADIO_TURNOFF,
	SKILLS_RADIO_ACTIVATE_TURNCOATS_ALL,		// order all enemy turncoats to turn into militia right now (in case this requires a radio)
	SKILLS_RADIO_LAST = SKILLS_RADIO_ACTIVATE_TURNCOATS_ALL,

	// spy
	SKILLS_INTEL_FIRST,
	SKILLS_INTEL_CONCEAL = SKILLS_INTEL_FIRST,	// assignment: spy hides among the population
	SKILLS_INTEL_GATHERINTEL,					// assignment: spy gathers information while disguised
	SKILLS_CREATE_TURNCOAT,
	SKILLS_ACTIVATE_TURNCOATS,					// order enemy turncoat to turn into militia right now
	SKILLS_ACTIVATE_TURNCOATS_ALL,				// order all enemy turncoats to turn into militia right now
	SKILLS_INTEL_LAST = SKILLS_ACTIVATE_TURNCOATS_ALL,

	// disguise
	SKILLS_DISGUISE_FIRST,
	SKILLS_DISGUISE_APPLY_DISGUISE = SKILLS_DISGUISE_FIRST,
	SKILLS_DISGUISE_REMOVE_DISGUISE,
	SKILLS_DISGUISE_TEST_DISGUISE,
	SKILLS_DISGUISE_REMOVE_CLOTHES,
	SKILLS_DISGUISE_LAST = SKILLS_DISGUISE_REMOVE_CLOTHES,

	// various
	SKILLS_VARIOUS_FIRST,
	SKILLS_SPOTTER = SKILLS_VARIOUS_FIRST,
	SKILLS_FOCUS,
	SKILLS_DRAG,
	SKILLS_FILL_CANTEENS,
	SKILLS_VARIOUS_LAST = SKILLS_FILL_CANTEENS,

	SKILLS_MAX,
};

// enum of uniform pieces
typedef struct
{
	PaletteRepID vest;
	PaletteRepID pants;
}UNIFORMCOLORS;

// HEADROCK HAM 3.6: Uniform colors for the different soldier classes
extern UNIFORMCOLORS gUniformColors[NUM_UNIFORMS];

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

#define DELAYED_MOVEMENT_FLAG_PATH_THROUGH_PEOPLE 0x01

// reasons for being unable to continue movement
enum
{
	REASON_STOPPED_NO_APS,
	REASON_STOPPED_SIGHT,
};


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

class TacticalActor
{
public:
	// Constructor
	TacticalActor();
	// Destructor
	~TacticalActor();

	// Reset every owned component. The constructor does this automatically.
	void initialize();
	SoldierIdentityComponent& identity() noexcept { return identity_; }
	const SoldierIdentityComponent& identity() const noexcept { return identity_; }
	SoldierRosterComponent& roster() noexcept { return roster_; }
	const SoldierRosterComponent& roster() const noexcept { return roster_; }
	SoldierVitalsComponent& vitals() noexcept { return vitals_; }
	const SoldierVitalsComponent& vitals() const noexcept { return vitals_; }
	SoldierStatisticsComponent& statistics() noexcept { return statistics_; }
	const SoldierStatisticsComponent& statistics() const noexcept { return statistics_; }
	SoldierStatusComponent& status() noexcept { return status_; }
	const SoldierStatusComponent& status() const noexcept { return status_; }
	SoldierFeatureFlagsComponent& featureFlags() noexcept { return featureFlags_; }
	const SoldierFeatureFlagsComponent& featureFlags() const noexcept { return featureFlags_; }
	SoldierInventory& inventory() noexcept { return inventory_; }
	const SoldierInventory& inventory() const noexcept { return inventory_; }
	SoldierKeyRingComponent& keyRing() noexcept { return keyRing_; }
	const SoldierKeyRingComponent& keyRing() const noexcept { return keyRing_; }
	SoldierPendingItemComponent& pendingItem() noexcept { return pendingItem_; }
	const SoldierPendingItemComponent& pendingItem() const noexcept { return pendingItem_; }
	SoldierServiceComponent& service() noexcept { return service_; }
	const SoldierServiceComponent& service() const noexcept { return service_; }
	SoldierDialogueComponent& dialogue() noexcept { return dialogue_; }
	const SoldierDialogueComponent& dialogue() const noexcept { return dialogue_; }
	SoldierAudioComponent& audio() noexcept { return audio_; }
	const SoldierAudioComponent& audio() const noexcept { return audio_; }
	SoldierReplicationComponent& replication() noexcept { return replication_; }
	const SoldierReplicationComponent& replication() const noexcept { return replication_; }
	SoldierMovementMetricsComponent& movementMetrics() noexcept { return movementMetrics_; }
	const SoldierMovementMetricsComponent& movementMetrics() const noexcept { return movementMetrics_; }
	SoldierAiPlanningComponent& aiPlanning() noexcept { return aiPlanning_; }
	const SoldierAiPlanningComponent& aiPlanning() const noexcept { return aiPlanning_; }
	SoldierAiPlanComponent& aiPlan() noexcept { return aiPlan_; }
	const SoldierAiPlanComponent& aiPlan() const noexcept { return aiPlan_; }
	SoldierAiBehaviorComponent& aiBehavior() noexcept { return aiBehavior_; }
	const SoldierAiBehaviorComponent& aiBehavior() const noexcept { return aiBehavior_; }
	SoldierAiCommunicationComponent& aiCommunication() noexcept { return aiCommunication_; }
	const SoldierAiCommunicationComponent& aiCommunication() const noexcept { return aiCommunication_; }
	SoldierMoraleComponent& morale() noexcept { return morale_; }
	const SoldierMoraleComponent& morale() const noexcept { return morale_; }
	SoldierSkillStateComponent& skillState() noexcept { return skillState_; }
	const SoldierSkillStateComponent& skillState() const noexcept { return skillState_; }
	SoldierConditionComponent& condition() noexcept { return condition_; }
	const SoldierConditionComponent& condition() const noexcept { return condition_; }
	SoldierDrugStateComponent& drugState() noexcept { return drugState_; }
	const SoldierDrugStateComponent& drugState() const noexcept { return drugState_; }
	SoldierStatProgressComponent& statProgress() noexcept { return statProgress_; }
	const SoldierStatProgressComponent& statProgress() const noexcept { return statProgress_; }
	SoldierTimingComponent& timing() noexcept { return timing_; }
	const SoldierTimingComponent& timing() const noexcept { return timing_; }
	SoldierLongActionComponent& longAction() noexcept { return longAction_; }
	const SoldierLongActionComponent& longAction() const noexcept { return longAction_; }
	SoldierInteractionComponent& interaction() noexcept { return interaction_; }
	const SoldierInteractionComponent& interaction() const noexcept { return interaction_; }
	SoldierPendingActionComponent& pendingAction() noexcept { return pendingAction_; }
	const SoldierPendingActionComponent& pendingAction() const noexcept { return pendingAction_; }
	SoldierActionPointComponent& actionPoints() noexcept { return actionPoints_; }
	const SoldierActionPointComponent& actionPoints() const noexcept { return actionPoints_; }
	SoldierCollapseComponent& collapseState() noexcept { return collapseState_; }
	const SoldierCollapseComponent& collapseState() const noexcept { return collapseState_; }
	SoldierPerceptionComponent& perception() noexcept { return perception_; }
	const SoldierPerceptionComponent& perception() const noexcept { return perception_; }
	SoldierAwarenessComponent& awareness() noexcept { return awareness_; }
	const SoldierAwarenessComponent& awareness() const noexcept { return awareness_; }
	SoldierCamouflageComponent& camouflage() noexcept { return camouflage_; }
	const SoldierCamouflageComponent& camouflage() const noexcept { return camouflage_; }
	SoldierEmploymentComponent& employment() noexcept { return employment_; }
	const SoldierEmploymentComponent& employment() const noexcept { return employment_; }
	SoldierAssignmentComponent& assignment() noexcept { return assignment_; }
	const SoldierAssignmentComponent& assignment() const noexcept { return assignment_; }
	SoldierDeploymentComponent& deployment() noexcept { return deployment_; }
	const SoldierDeploymentComponent& deployment() const noexcept { return deployment_; }
	SoldierStrategicPathComponent& strategicPath() noexcept { return strategicPath_; }
	const SoldierStrategicPathComponent& strategicPath() const noexcept { return strategicPath_; }
	SoldierVehicleStateComponent& vehicleState() noexcept { return vehicleState_; }
	const SoldierVehicleStateComponent& vehicleState() const noexcept { return vehicleState_; }
	SoldierScheduleComponent& schedule() noexcept { return schedule_; }
	const SoldierScheduleComponent& schedule() const noexcept { return schedule_; }
	SoldierPositionComponent& position() noexcept { return position_; }
	const SoldierPositionComponent& position() const noexcept { return position_; }
	SoldierFrontArcComponent& frontArc() noexcept { return frontArc_; }
	const SoldierFrontArcComponent& frontArc() const noexcept { return frontArc_; }
	SoldierMovementHistoryComponent& movementHistory() noexcept { return movementHistory_; }
	const SoldierMovementHistoryComponent& movementHistory() const noexcept { return movementHistory_; }
	SoldierPathingComponent& pathing() noexcept { return pathing_; }
	const SoldierPathingComponent& pathing() const noexcept { return pathing_; }
	SoldierMovementComponent& movement() noexcept { return movement_; }
	const SoldierMovementComponent& movement() const noexcept { return movement_; }
	SoldierTurnStateComponent& turnState() noexcept { return turnState_; }
	const SoldierTurnStateComponent& turnState() const noexcept { return turnState_; }
	SoldierTargetingComponent& targeting() noexcept { return targeting_; }
	const SoldierTargetingComponent& targeting() const noexcept { return targeting_; }
	SoldierAttackSelectionComponent& attackSelection() noexcept { return attackSelection_; }
	const SoldierAttackSelectionComponent& attackSelection() const noexcept { return attackSelection_; }
	SoldierMeleeApproachComponent& meleeApproach() noexcept { return meleeApproach_; }
	const SoldierMeleeApproachComponent& meleeApproach() const noexcept { return meleeApproach_; }
	SoldierFireControlComponent& fireControl() noexcept { return fireControl_; }
	const SoldierFireControlComponent& fireControl() const noexcept { return fireControl_; }
	SoldierCombatResultComponent& combatResult() noexcept { return combatResult_; }
	const SoldierCombatResultComponent& combatResult() const noexcept { return combatResult_; }
	SoldierCombatContributionComponent& combatContribution() noexcept { return combatContribution_; }
	const SoldierCombatContributionComponent& combatContribution() const noexcept { return combatContribution_; }
	SoldierSuppressionComponent& suppression() noexcept { return suppression_; }
	const SoldierSuppressionComponent& suppression() const noexcept { return suppression_; }
	SoldierDamageDisplayComponent& damageDisplay() noexcept { return damageDisplay_; }
	const SoldierDamageDisplayComponent& damageDisplay() const noexcept { return damageDisplay_; }
	RenderPaletteBank& palette() noexcept { return palette_; }
	const RenderPaletteBank& palette() const noexcept { return palette_; }
	SoldierRenderStateComponent& renderState() noexcept { return renderState_; }
	const SoldierRenderStateComponent& renderState() const noexcept { return renderState_; }
	SoldierUiPresentationComponent& uiPresentation() noexcept { return uiPresentation_; }
	const SoldierUiPresentationComponent& uiPresentation() const noexcept { return uiPresentation_; }
	SoldierAnimationIntentComponent& animationIntent() noexcept { return animationIntent_; }
	const SoldierAnimationIntentComponent& animationIntent() const noexcept { return animationIntent_; }
	SoldierAnimationPlaybackComponent& animationPlayback() noexcept { return animationPlayback_; }
	const SoldierAnimationPlaybackComponent& animationPlayback() const noexcept { return animationPlayback_; }
	SoldierAnimationActivityComponent& animationActivity() noexcept { return animationActivity_; }
	const SoldierAnimationActivityComponent& animationActivity() const noexcept { return animationActivity_; }
	SoldierAnimationCacheComponent& animationCache() noexcept { return animationCache_; }
	const SoldierAnimationCacheComponent& animationCache() const noexcept { return animationCache_; }
	SoldierRenderBindingsComponent& renderBindings() noexcept { return renderBindings_; }
	const SoldierRenderBindingsComponent& renderBindings() const noexcept { return renderBindings_; }
	SoldierRuntimeComponents& runtime() noexcept { return runtime_; }
	const SoldierRuntimeComponents& runtime() const noexcept { return runtime_; }

	INT16	GetMaxDistanceVisible(INT32 sGridNo = -1, INT8 bLevel = -1, int calcAsType = -1, TacticalActor *pKnownSubject = NULL);

private:
	SoldierIdentityComponent	identity_;
	SoldierRosterComponent	roster_;
	SoldierVitalsComponent	vitals_;
	SoldierStatisticsComponent	statistics_;
	SoldierStatusComponent	status_;
	SoldierFeatureFlagsComponent	featureFlags_;
	SoldierInventory	inventory_;
	SoldierKeyRingComponent	keyRing_;
	SoldierPendingItemComponent	pendingItem_;
	SoldierServiceComponent	service_;
	SoldierDialogueComponent	dialogue_;
	SoldierAudioComponent	audio_;
	SoldierReplicationComponent	replication_;
	SoldierMovementMetricsComponent	movementMetrics_;
	SoldierAiPlanningComponent	aiPlanning_;
	SoldierAiPlanComponent	aiPlan_;
	SoldierAiBehaviorComponent	aiBehavior_;
	SoldierAiCommunicationComponent	aiCommunication_;
	SoldierMoraleComponent	morale_;
	SoldierSkillStateComponent	skillState_;
	SoldierConditionComponent	condition_;
	SoldierDrugStateComponent	drugState_;
	SoldierStatProgressComponent	statProgress_;
	SoldierTimingComponent	timing_;
	SoldierLongActionComponent	longAction_;
	SoldierInteractionComponent	interaction_;
	SoldierPendingActionComponent	pendingAction_;
	SoldierActionPointComponent	actionPoints_;
	SoldierCollapseComponent	collapseState_;
	SoldierPerceptionComponent	perception_;
	SoldierAwarenessComponent	awareness_;
	SoldierCamouflageComponent	camouflage_;
	SoldierEmploymentComponent	employment_;
	SoldierAssignmentComponent	assignment_;
	SoldierDeploymentComponent	deployment_;
	SoldierStrategicPathComponent	strategicPath_;
	SoldierVehicleStateComponent	vehicleState_;
	SoldierScheduleComponent	schedule_;
	SoldierPositionComponent	position_;
	SoldierFrontArcComponent	frontArc_;
	SoldierMovementHistoryComponent	movementHistory_;
	SoldierPathingComponent	pathing_;
	SoldierMovementComponent	movement_;
	SoldierTurnStateComponent	turnState_;
	SoldierTargetingComponent	targeting_;
	SoldierAttackSelectionComponent	attackSelection_;
	SoldierMeleeApproachComponent	meleeApproach_;
	SoldierFireControlComponent	fireControl_;
	SoldierCombatResultComponent	combatResult_;
	SoldierCombatContributionComponent	combatContribution_;
	SoldierSuppressionComponent	suppression_;
	SoldierDamageDisplayComponent	damageDisplay_;
	RenderPaletteBank	palette_;
	SoldierRenderStateComponent	renderState_;
	SoldierUiPresentationComponent	uiPresentation_;
	SoldierAnimationIntentComponent	animationIntent_;
	SoldierAnimationPlaybackComponent	animationPlayback_;
	SoldierAnimationActivityComponent	animationActivity_;
	SoldierAnimationCacheComponent	animationCache_;
	SoldierRenderBindingsComponent	renderBindings_;
	// Runtime-only state is grouped by behavior and reset as one boundary. It is
	// deliberately outside the serialized POD and sub-structure field lists.
	SoldierRuntimeComponents	runtime_;

public:
	// CREATION FUNCTIONS
	BOOLEAN DeleteSoldier( void );

	BOOLEAN CreateSoldierCommon( UINT8 ubBodyType, SoldierID usSoldierID, UINT16 usState );


	// Soldier Management functions, called by Event Pump.c
	BOOLEAN EVENT_InitNewSoldierAnim( UINT16 usNewState, UINT16 usStartingAniCode, BOOLEAN fForce );

	BOOLEAN ChangeSoldierState( UINT16 usNewState, UINT16 usStartingAniCode, BOOLEAN fForce );
	void EVENT_SetSoldierPosition( FLOAT dNewXPos, FLOAT dNewYPos );
	void EVENT_SetSoldierDestination( UINT8	ubNewDirection );
	void EVENT_GetNewSoldierPath( INT32 sDestGridNo, UINT16 usMovementAnim );

	void EVENT_SetSoldierDirection( UINT16	usNewDirection );
	void EVENT_SetSoldierDesiredDirection( UINT16	usNewDirection );
	void EVENT_FireSoldierWeapon( INT32 sTargetGridNo );
	void EVENT_SoldierGotHit( UINT16 usWeaponIndex, INT16 ubDamage, INT16 sBreathLoss, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation, INT16 sSubsequent, INT32 sLocationGridNo );
	void EVENT_StopMerc( INT32 sGridNo, INT8 bDirection );
	void EVENT_SetSoldierPositionForceDelete( FLOAT dNewXPos, FLOAT dNewYPos );
	void EVENT_BeginMercTurn( BOOLEAN fFromRealTime, INT32 iRealTimeCounter );

	BOOLEAN EVENT_InternalGetNewSoldierPath( INT32 sDestGridNo, UINT16 usMovementAnim, BOOLEAN fFromUI, BOOLEAN fForceRestart );
	void EVENT_InternalSetSoldierDestination( UINT16	usNewDirection, BOOLEAN fFromMove, UINT16 usAnimState );
	void EVENT_InternalSetSoldierPosition( FLOAT dNewXPos, FLOAT dNewYPos ,BOOLEAN fUpdateDest, BOOLEAN fUpdateFinalDest, BOOLEAN fForceDelete );


	// Soldier Management functions called by Overhead.c
	// Flugente: frozen soldiers do not move. We simulate this by using fixed animation frames, which we determine here 
	UINT16 CryoAniFrame();
	BOOLEAN ConvertAniCodeToAniFrame( UINT16 usAniFrame );
	// Convert this soldier's world direction into the sprite direction for the given animation surface
	UINT8 SpriteDirForSurface( UINT16 usAnimSurface );
	void TurnSoldier( void );
	void ChangeSoldierStance( UINT8 ubDesiredStance );
	void StopSoldier( void );
	void ReviveSoldier( void );
	UINT8 SoldierTakeDamage( INT8 bHeight, INT16 sLifeDeduct, INT16 sBreathDeduct, UINT8 ubReason, SoldierID ubAttacker, INT32 sSourceGrid, INT16 sSubsequent, BOOLEAN fShowDamage );

	// Deferred movement damage is owned by TacticalActorDamageQueue.

	// Palette functions for soldiers
	BOOLEAN CreateSoldierPalettes( void );

	// UTILITY FUNCTUIONS
	void MoveMerc( FLOAT dMovementChange, FLOAT dAngle, BOOLEAN fCheckRange );
	// This function is now obsolete.	Call ReduceAttackBusyCount instead.
	// void ReleaseSoldiersAttacker( TacticalActor *pSoldier );
	void SoldierGotoStationaryStance( void );
	void RemoveSoldierFromGridNo( void );
	void InternalRemoveSoldierFromGridNo( BOOLEAN fForce );


	void AdjustNoAPToFinishMove( BOOLEAN fSet );






	void SetSoldierCowerState( BOOLEAN fOn );
	void ResetSoldierChangeStatTimer( void );
	void SetSoldierGridNo( INT32 sNewGridNo, BOOLEAN fForceRemove );
	void SetSoldierHeight( FLOAT dNewHeight );
	void InternalSetSoldierHeight( FLOAT dNewHeight, BOOLEAN fUpdateLevel );//this function did not have a forward declaration


	BOOLEAN DoMercBattleSound( UINT8 ubBattleSoundID );
	BOOLEAN InternalDoMercBattleSound( UINT8 ubBattleSoundID, INT8 bSpecialCode );
	BOOLEAN GetProfileFlagsFromGridno( UINT16 usAnimState, INT32 sTestGridNo, UINT16 *usFlags );
	void HaultSoldierFromSighting( BOOLEAN fFromSightingEnemy );
	void ReLoadSoldierAnimationDueToHandItemChange( UINT16 usOldItem, UINT16 usNewItem );
	void PickDropItemAnimation( void );

	void HandleAnimationProfile( UINT16	usAnimState, BOOLEAN fRemove );
	// Overload taking the already-computed animation surface (avoids recomputing DetermineSoldierAnimationSurface)
	void HandleAnimationProfile( UINT16	usAnimState, UINT16 usAnimSurface, BOOLEAN fRemove );
	void HandleSoldierTakeDamageFeedback( void );

	// sevenfm

	BOOLEAN SoldierReadyWeapon( INT16 sTargetXPos, INT16 sTargetYPos, BOOLEAN fEndReady, BOOLEAN fRaiseToHipOnly );
	BOOLEAN SoldierReadyWeapon( void );
	BOOLEAN InternalSoldierReadyWeapon( UINT8 sFacingDir, BOOLEAN fEndReady, BOOLEAN fRaiseToHipOnly );

	BOOLEAN CheckSoldierHitRoof( void );
	// reset the extra stat variables
	void	ResetExtraStats();

	void InitializeExtraData(void);

	// Flugente: return a soldier's name. This allows for very easy manipulation of a soldier's name with pre- an suffixes, ranks etc.
	STR16		GetName();

	//void		AddDrugValues(UINT8 uDrugType, UINT8 usEffect, UINT8 usTravelRate, UINT8 usSideEffect );

	// Flugente: soldier profiles
	INT8		GetSoldierProfileType(UINT8 usTeam);		// retrieves the correct sub-array

	void		SoldierPropertyUpkeep();					// update functions for various properties (updating counters, resetting flags etc.)

	void	PrintFoodDesc( CHAR16* apStr, BOOLEAN fFullDesc = FALSE );
	void	PrintSleepDesc( CHAR16* apStr );

	//////////////////////////////////////////////////////////////////////////////

}; // TacticalActor;

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
// TYPEDEFS FOR ANIMATION PROFILES
typedef struct
{
	UINT16	usTileFlags;
	INT8		bTileX;
	INT8		bTileY;

} ANIM_PROF_TILE;

typedef struct
{
	UINT8							ubNumTiles;
	ANIM_PROF_TILE		*pTiles;

}	ANIM_PROF_DIR;

typedef struct ANIM_PROF
{
	
	ANIM_PROF_DIR		Dirs[8];

} ANIM_PROF;



// Globals
//////////

// VARIABLES FOR PALETTE REPLACEMENTS FOR HAIR, ETC
extern UINT32					guiNumPaletteSubRanges;
extern UINT8					*gubpNumReplacementsPerRange;
extern PaletteSubRangeType		*gpPaletteSubRanges;
extern UINT32					guiNumReplacements;
extern PaletteReplacementType	*gpPalRep;

extern UINT8					bHealthStrRanges[];


// Functions
////////////

// Soldier Management functions called by Overhead.c
void RevivePlayerTeam( );


// Palette functions for soldiers
BOOLEAN GetPaletteRepIndexFromID( const CHAR8 *aPalRep, UINT8 *pubPalIndex );
BOOLEAN	SetPaletteReplacement( SGPPaletteEntry *p8BPPPalette, PaletteRepID aPalRep );
BOOLEAN LoadPaletteData( );
BOOLEAN DeletePaletteData( );

// UTILITY FUNCTUIONS
void MoveMercFacingDirection( TacticalActor *pSoldier, BOOLEAN fReverse, FLOAT dMovementDist );
UINT8 GetDirectionFromXY( INT16 sXPos, INT16 sYPos, TacticalActor *pSoldier );
BOOLEAN GetDirectionChangeAmount( INT32 sGridNo, TacticalActor *pSoldier, UINT8 uiTurnAmount);
UINT8 GetDirectionFromGridNo( INT32 sGridNo, TacticalActor *pSoldier );
UINT8 atan8( INT16 sXPos, INT16 sYPos, INT16 sXPos2, INT16 sYPos2 );
UINT8 atan8FromAngle( DOUBLE dAngle );
INT16 GetDirectionToGridNoFromGridNo(INT32 sGridNoDest, INT32 sGridNoSrc);
INT16 GetDirectionFromCenterCellXYGridNo(INT32 EndGridNo, INT32 StartGridNo);
// This function is now obsolete.	Call ReduceAttackBusyCount instead.
// void ReleaseSoldiersAttacker( TacticalActor *pSoldier );



// WRAPPERS FOR SOLDIER EVENTS
void SendSoldierPositionEvent( TacticalActor *pSoldier, FLOAT dNewXPos, FLOAT dNewYPos );
void SendSoldierDestinationEvent( TacticalActor *pSoldier, UINT16 usNewDestination );
void SendGetNewSoldierPathEvent( TacticalActor *pSoldier, INT32 sDestGridNo, UINT16 usMovementAnim );
void SendSoldierSetDirectionEvent( TacticalActor *pSoldier, UINT16 usNewDirection );
void SendSoldierSetDesiredDirectionEvent( TacticalActor *pSoldier, UINT16 usDesiredDirection );
void SendChangeSoldierStanceEvent( TacticalActor *pSoldier, UINT8 ubNewStance );
void SendBeginFireWeaponEvent( TacticalActor *pSoldier, INT32 sTargetGridNo );
void SendBeginFireWeaponEvent(
	TacticalActor *pSoldier, INT32 sTargetGridNo,
	INT8 bTargetLevel, INT8 bTargetCubeLevel );



void HandleAnimationProfile( TacticalActor *pSoldier, UINT16	usAnimState, BOOLEAN fRemove );
BOOLEAN GetProfileFlagsFromGridno( TacticalActor *pSoldier, UINT16 usAnimState, INT32 sTestGridNo, UINT16 *usFlags );
BOOLEAN PreloadSoldierBattleSounds( TacticalActor *pSoldier, BOOLEAN fRemove );
void CrowsFlyAway( UINT8 ubTeam );
void DebugValidateSoldierData( );
void HandlePlayerTogglingLightEffects( BOOLEAN fToggleValue );

// added by SANDRO
UINT8 GetSquadleadersCountInVicinity( TacticalActor * pSoldier, BOOLEAN fWithHigherLevel, BOOLEAN fDontCheckDistance );
BOOLEAN ResolvePendingInterrupt( TacticalActor * pSoldier, UINT8 ubInterruptType );
BOOLEAN AIDecideHipOrShoulderStance( TacticalActor * pSoldier, INT32 iGridNo );
BOOLEAN DecideAltAnimForBigMerc( TacticalActor * pSoldier );

// added by Flugente
BOOLEAN TwoStagedTrait( UINT8 uiSkillTraitNumber );						// determine if this (new) trait has two stages
BOOLEAN MajorTrait( UINT8 uiSkillTraitNumber );							// determine if this is a major trait
UINT16	GetSuspiciousAnimationAPDuration( UINT16 usAnimation );			// get overt penalty duration in AP for using an animation

//typedef struct


void HandleTakeDamageDeath( TacticalActor *pSoldier, UINT8 bOldLife, UINT8 ubReason );

void SetDamageDisplayCounter(TacticalActor* pSoldier);

// SANDRO - This whole procedure was merged with the surgery ability of the doctor trait

// Flugente: apply a consumable item on a soldier. Returns true if item was successfully interacted with
BOOLEAN ApplyConsumable( TacticalActor* pSoldier, OBJECTTYPE *pObject, BOOLEAN fForce, BOOLEAN fUseAPs );

#endif
