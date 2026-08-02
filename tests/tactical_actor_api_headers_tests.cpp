#include "Laptop/CampaignStats.h"
#include "Laptop/insurance Contract.h"
#include "Laptop/personnel.h"
#include "Strategic/Facilities.h"
#include "Strategic/Map Screen Interface Bottom.h"
#include "Strategic/MilitiaSquads.h"
#include "Strategic/Queen Command.h"
#include "Strategic/Strategic Movement.h"
#include "Strategic/Strategic Status.h"
#include "Strategic/Town Militia.h"
#include "Strategic/strategic town reputation.h"
#include "Tactical/Air Raid.h"
#include "Tactical/Interface Utils.h"
#include "Tactical/Merc Hiring.h"
#include "Tactical/Militia Control.h"
#include "Tactical/Morale.h"
#include "Tactical/SkillCheck.h"
#include "Tactical/Soldier Class.h"
#include "Tactical/Soldier Functions.h"
#include "Tactical/Squads.h"
#include "Tactical/TeamTurns.h"
#include "Tactical/soldier tile.h"
#include "Tactical/opplist.h"
#include "TileEngine/Smell.h"

#include <type_traits>

static_assert(std::is_class_v<TacticalActor>);
static_assert(SOLDIER_CLASS_NONE == 0);
static_assert(SOLDIER_CLASS_CREATURE == 7);
static_assert(SOLDIER_CLASS_MAX == 14);
static_assert(SOLDIER_GUN_CHOICE_SELECTIONS == SOLDIER_CLASS_CREATURE);
static_assert(SOLDIER_CLASS_ENEMY(SOLDIER_CLASS_ADMINISTRATOR));
static_assert(!SOLDIER_CLASS_ENEMY(SOLDIER_CLASS_ELITE_MILITIA));
static_assert(SOLDIER_CLASS_MILITIA(SOLDIER_CLASS_GREEN_MILITIA));
static_assert(!SOLDIER_CLASS_MILITIA(SOLDIER_CLASS_ARMY));

int main()
{
	return 0;
}
