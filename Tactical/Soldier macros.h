#ifndef SOLDIER_MACROS_H
#define SOLDIER_MACROS_H

// MACROS FOR EASIER SOLDIER CONTROL
#include "TeamTurns.h"
#include "Soldier Profile.h"
#include "Assignments.h"
#include "Animation Data.h"

#include <type_traits>
#include <utility>

template<typename SoldierLike, typename = void>
struct HasSoldierIdentityComponent : std::false_type {};

template<typename SoldierLike>
struct HasSoldierIdentityComponent<SoldierLike, std::void_t<
	decltype(std::declval<const SoldierLike&>().identity())>> : std::true_type {};

template<typename SoldierLike, typename = void>
struct HasSoldierRosterComponent : std::false_type {};

template<typename SoldierLike>
struct HasSoldierRosterComponent<SoldierLike, std::void_t<
	decltype(std::declval<const SoldierLike&>().roster())>> : std::true_type {};

template<typename SoldierLike>
inline auto SoldierMacroBodyType(const SoldierLike* soldier) noexcept
{
	if constexpr (HasSoldierIdentityComponent<SoldierLike>::value)
		return soldier->identity().bodyType();
	else
		return soldier->ubBodyType;
}

template<typename SoldierLike>
inline auto SoldierMacroTeam(const SoldierLike* soldier) noexcept
{
	if constexpr (HasSoldierRosterComponent<SoldierLike>::value)
		return soldier->roster().team();
	else
		return soldier->bTeam;
}

// MACROS
#define RPC_RECRUITED( p )	( ( p->identity().profile() == NO_PROFILE ) ? FALSE : ( gMercProfiles[ p->identity().profile() ].ubMiscFlags & PROFILE_MISC_FLAG_RECRUITED ) )

#define AM_AN_EPC( p )	( ( p->identity().profile() == NO_PROFILE ) ? FALSE : ( gMercProfiles[ p->identity().profile() ].ubMiscFlags & PROFILE_MISC_FLAG_EPCACTIVE ) )

// rftr - this is for madlab's robot, since it has a profile
#define AM_A_ROBOT( p )	( ( p->identity().profile() == NO_PROFILE ) ? FALSE : ( gMercProfiles[ p->identity().profile() ].ubBodyType == ROBOTNOWEAPON ) )


#define OK_ENEMY_MERC( p ) ( !p->aiBehavior().neutral() && (p->roster().side() != gbPlayerNum ) && p->vitals().health() >= OKLIFE && (p->roster().team() < 5 ))

// Checks if our guy can be controllable .... checks bInSector, team, on duty, etc...

// Checks if our guy is controllable but doesn't care about current assignment
#define OK_CONTROL_MERC( p ) ( p->vitals().health() >= OKLIFE && p->roster().active() && p->roster().inSector() && p->roster().team() == gbPlayerNum && !(p->skillState().cooldown(SOLDIER_COOLDOWN_CRYO)) )

#define OK_CONTROLLABLE_MERC( p ) ( OK_CONTROL_MERC(p) && ( p->assignment().current() < ON_DUTY || p->assignment().current() == VEHICLE )	)

// Checks if our guy can be controllable .... checks bInSector, team, on duty, etc...
#define OK_INSECTOR_MERC( p ) ( p->vitals().health() >= OKLIFE && p->roster().active() && p->roster().inSector() && p->roster().team() == gbPlayerNum && p->assignment().current() < ON_DUTY )

// Checkf if our guy can be selected and is not in a position where our team has an interupt and he does not have one...
#define OK_INTERRUPT_MERC( p ) ( ( INTERRUPT_QUEUED != 0 ) ? ( ( p->turnState().moved() ) ? FALSE : TRUE ) : TRUE )

#define CREATURE_OR_BLOODCAT( p ) ( (p->status().flags() & SOLDIER_MONSTER) || p->identity().bodyType() == BLOODCAT )

#define COMBAT_JEEP( p ) ( SoldierMacroBodyType( p ) == COMBAT_JEEP )
#define TANK( p ) (SoldierMacroBodyType( p ) == TANK_NE || SoldierMacroBodyType( p ) == TANK_NW )
#define ENEMYROBOT( p ) (SoldierMacroBodyType( p ) == ROBOTNOWEAPON && SoldierMacroTeam( p ) == ENEMY_TEAM)
#define ARMED_VEHICLE( p )	( TANK( p ) || COMBAT_JEEP(p) )

//#define OK_ENTERABLE_VEHICLE( p )	( ( p->status().flags() & SOLDIER_VEHICLE ) && !TANK( p ) && p->vitals().health() >= OKLIFE	)
#define OK_ENTERABLE_VEHICLE( p )	( ( p->status().flags() & SOLDIER_VEHICLE ) && (!ARMED_VEHICLE( p ) || !(p->status().flags() & SOLDIER_ENEMY) ) && p->vitals().health() >= OKLIFE	)

#endif
