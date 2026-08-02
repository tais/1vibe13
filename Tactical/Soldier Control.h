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
#include "Soldier Stat Types.h"
#include "Strategic Path Types.h"
#include "Taunt Types.h"
#include "TacticalDestinationTypes.h"
#include "TacticalActor.h"
#include "TacticalActorAnimationState.h"
#include "TacticalActorBattleSounds.h"
#include "TacticalActorBloodState.h"
#include "TacticalActorConsumables.h"
#include "TacticalActorCrowBehavior.h"
#include "TacticalActorDamageFeedback.h"
#include "TacticalActorDamageResolution.h"
#include "TacticalActorDebug.h"
#include "TacticalActorEmploymentTypes.h"
#include "TacticalActorEvents.h"
#include "TacticalActorLighting.h"
#include "TacticalActorLocomotion.h"
#include "TacticalActorLongActions.h"
#include "TacticalActorInterrupts.h"
#include "TacticalActorMovementState.h"
#include "TacticalActorPendingActionTypes.h"
#include "TacticalActorPredicates.h"
#include "TacticalActorQuoteFlags.h"
#include "TacticalActorSkills.h"
#include "TacticalActorStateFlags.h"

static_assert(MAXPATROLGRIDS == SOLDIER_PATROL_GRID_COUNT,
	"Soldier patrol storage must retain the established save-schema capacity");

// TEMP VALUES FOR NAMES
#define MAXCIVLASTNAMES		30
extern UINT16 CivLastNames[MAXCIVLASTNAMES][10];

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
#define BANDAGED( s ) (s->vitals().maximumHealth() - s->vitals().health() - s->vitals().bleeding())

// MACROS
// #######################################################

#define MAX_FULLTILE_DIRECTIONS 3
static_assert(
	SoldierFrontArcComponent::DirectionCount == MAX_FULLTILE_DIRECTIONS,
	"front-arc component capacity must retain the established soldier schema");

// DIGICRAB: Burst UnCap. Keep the legacy spelling as a source-compatible
// alias; persistent capacity is now owned by SoldierFireControlComponent.
#define MAX_BURST_SPREAD_TARGETS SoldierFireControlComponent::SpreadTargetCapacity

// ADB makes the code clearer, used like "thisSoldier->foo();"
//CHRISL: Not sure if it make the code easier to read or not, but it does make it harder to debug
//#define thisSoldier this

enum class BackgroundVectorTypes;

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
// Functions
////////////

// Soldier Management functions called by Overhead.c
void RevivePlayerTeam( );


// UTILITY FUNCTUIONS
// This function is now obsolete.	Call ReduceAttackBusyCount instead.
// void ReleaseSoldiersAttacker( TacticalActor *pSoldier );



BOOLEAN PreloadSoldierBattleSounds( TacticalActor *pSoldier, BOOLEAN fRemove );

//typedef struct


// SANDRO - This whole procedure was merged with the surgery ability of the doctor trait

#endif
