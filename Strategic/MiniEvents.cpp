/*
Mini events are a feature where the player can receive short text blurbs, and pick one of two outcomes.
Based on game implementations like Gloomhaven (road/city events) or Battletech (travel events).

The general idea is that every so often, the game checks to see if an event should be triggered. These shouldn't be very
frequent so that the usual gameplay isn't interrupted that often.

Mini events are set up in MiniEvents.lua. This file handles mini event triggers and defines functions for the lua file
to call into.
*/

#include "MiniEvents.h"
#include "SoldierRepository.h"
#include "TacticalWorldAdapter.h"

#include "Assignments.h"
#include "Campaign.h"
#include "Campaign Types.h"
#include "connect.h"
#include "finances.h"
#include "Game Clock.h"
#include "Game Event Hook.h"
#include "GameSettings.h"
#include "LaptopSave.h"
#include "mapscreen.h"
#include "Map Screen Helicopter.h"
#include "message.h"
#include "MessageBoxScreen.h"
#include "MilitiaIndividual.h"
#include "Morale.h"
#include "Overhead.h"
#include "Overhead Types.h"
#include "random.h"
#include "TacticalActor.h"
#include "Soldier Stat Types.h"
#include "Soldier Profile Constants.h"
#include "TacticalActorStateFlags.h"
#include "Soldier macros.h"
#include "Soldier Profile.h"
#include "Squads.h"
#include "strategic.h"
#include "strategicmap.h"
#include "Strategic Movement.h"
#include "Strategic Town Loyalty.h"
#include "Town Militia.h"
#include "Vehicles.h"

extern "C" {
#include "lua.h"
}

#include <lua_function.h>
#include <lua_state.h>

extern CHAR16 gzUserDefinedButton1[ 128 ];
extern CHAR16 gzUserDefinedButton2[ 128 ];
extern CHAR16 pTownNames[MAX_TOWNS][MAX_TOWN_NAME_LENGHT];

static size_t MAX_BUTTON_LENGTH = 60;
static size_t MAX_BODY_LENGTH = 450;

static UINT32 guiMiniEventsCachedScreen;
static LuaState gLS;
static std::vector<TacticalActor*> gAllMercs;

static void QueueNextMiniEvent(UINT32 nextEventId, UINT32 hoursToNextMiniEvent);

// LUA STUFF
static void MiniEventsLua(UINT32 eventId);
static int MiniEventsLua_MessageBox(lua_State* LS);
static void MiniEventsLua_MessageBoxCallback(UINT8 ubExitValue);
static int MiniEventsLua_ResolveEvent(lua_State* LS);
static int MiniEventsLua_ScreenMsg(lua_State* LS);

namespace MiniEventHelpers
{
	enum Skills
	{
		SKILL_START = 0,
		SKILL_AUTO_WEAPONS,
		SKILL_HEAVY_WEAPONS,
		SKILL_MARKSMAN,
		SKILL_HUNTER,
		SKILL_GUNSLINGER,
		SKILL_HAND_TO_HAND,
		SKILL_DEPUTY,
		SKILL_TECHNICIAN,
		SKILL_PARAMEDIC,
		SKILL_AMBIDEXTROUS,
		SKILL_MELEE,
		SKILL_THROWING,
		SKILL_NIGHT_OPS,
		SKILL_STEALTHY,
		SKILL_ATHLETICS,
		SKILL_BODYBUILDING,
		SKILL_DEMOLITIONS,
		SKILL_TEACHING,
		SKILL_SCOUTING,
		SKILL_COVERT_OPS,
		SKILL_RADIO_OPERATOR,
		SKILL_SNITCH,
		SKILL_SURVIVAL,
		SKILL_END
	};

	enum Stats
	{
		STAT_START = -1,
		STAT_LIFE = 0,
		STAT_STRENGTH,
		STAT_AGILITY,
		STAT_DEXTERITY,
		STAT_WISDOM,
		STAT_LEADERSHIP,
		STAT_MARKSMANSHIP,
		STAT_MECHANICAL,
		STAT_EXPLOSIVE,
		STAT_MEDICAL,
		STAT_EXPLEVEL,
		STAT_MAX
	};

	static BOOLEAN IsMajorSkill(const INT8 skillId)
	{
		return skillId >= Skills::SKILL_AUTO_WEAPONS && skillId <= Skills::SKILL_PARAMEDIC || skillId == Skills::SKILL_COVERT_OPS;
	}

	static int l_AddMoneyToPlayerAccount(lua_State* LS)
	{
		const INT32 currentBalance = LaptopSaveInfo.iCurrentBalance;
		const INT32 amount = lua_tointeger(LS, 1);
		bool forceToZero = false;	// default when the optional 2nd arg is omitted (was uninitialized -> nondeterministic below)

		if (lua_gettop(LS) == 2)
			forceToZero = lua_toboolean(LS, 2);

		if (currentBalance + amount < 0)
		{
			if (forceToZero)
			{
				AddTransactionToPlayersBook(MINI_EVENT, 0, GetWorldTotalMin(), -currentBalance);
				lua_pushboolean(LS, true);
			}
			else
			{
				lua_pushboolean(LS, false);
			}
		}
		else
		{
			AddTransactionToPlayersBook(MINI_EVENT, 0, GetWorldTotalMin(), amount);
			lua_pushboolean(LS, true);
		}

		return 1;
	}

	static int l_AddIntel(lua_State* LS)
	{
		INT32 amount = lua_tointeger(LS, 1);

		if (!gGameExternalOptions.fIntelResource)
		{
			// intel is disabled. give some cash instead
			amount *= 500;
			const INT32 currentBalance = LaptopSaveInfo.iCurrentBalance;
			if (currentBalance + amount < 0)
			{
				lua_pushboolean(LS, false);
			}
			else
			{
				AddTransactionToPlayersBook(MINI_EVENT, 0, GetWorldTotalMin(), amount);
				lua_pushboolean(LS, true);
			}

			return 1;
		}

		const float currentIntel = LaptopSaveInfo.dIntelPool;

		if (currentIntel + amount < 0)
		{
			lua_pushboolean(LS, false);
		}
		else
		{
			AddIntel(static_cast<FLOAT>(amount), TRUE);
			lua_pushboolean(LS, true);
		}

		return 1;	// was preceded by an unconditional lua_pushboolean(true) that masked the real success/fail result pushed above
	}

	static int l_AddSkill(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));
		const INT8 skillId = static_cast<INT8>(lua_tointeger(LS, 2));

		std::for_each(gAllMercs.begin(), gAllMercs.end(), [profileId, skillId](TacticalActor* merc) {
			if (merc->identity().profile() != profileId)
				return;

			int firstAvailableIndex = sizeof(gMercProfiles[merc->identity().profile()].bSkillTraits) / sizeof(gMercProfiles[merc->identity().profile()].bSkillTraits[0]);
			int skillCount = 0;
			INT8* skillTraits = gMercProfiles[merc->identity().profile()].bSkillTraits;

			for (int i = 0; i < sizeof(gMercProfiles[merc->identity().profile()].bSkillTraits) / sizeof(gMercProfiles[merc->identity().profile()].bSkillTraits[0]); ++i)
			{
				if (*(skillTraits + i) == 0 && firstAvailableIndex > i)
					firstAvailableIndex = i;

				if (*(skillTraits + i) == skillId)
					skillCount++;
			}

			if (skillCount == 0 || (IsMajorSkill(skillId) && skillCount < 2))
				*(skillTraits + firstAvailableIndex) = skillId;
		});


		return 0;
	}

	static int l_AddTownLoyalty(lua_State* LS)
	{
		const INT8 townId = static_cast<INT8>(lua_tointeger(LS, 1));
		const int points = lua_tointeger(LS, 2);

		if (points > 0)
		{
			IncrementTownLoyalty(townId, static_cast<UINT32>(points));
		}
		else if (points < 0)
		{
			DecrementTownLoyalty(townId, static_cast<UINT32>(std::abs(points)));
		}

		return 0;
	}

	static int l_AdjustBreathMax(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));
		const int val = lua_tointeger(LS, 2);

		if (val == 0)
			return 0;

		std::for_each(gAllMercs.begin(), gAllMercs.end(), [profileId, val](TacticalActor* merc) {
			if (merc->identity().profile() == profileId)
			{
				int newBreathValue = merc->vitals().maximumBreath() + val;
				newBreathValue = max(min(newBreathValue, 100), 0);

				merc->vitals().maximumBreath() = newBreathValue;
				merc->vitals().breath() = max(min(merc->vitals().breath(), merc->vitals().maximumBreath()), 0);
			}
		});

		return 0;
	}

	static int l_AdjustEnemyStrengthInSector(lua_State* LS)
	{
		const INT16 x = lua_tointeger(LS, 1);
		const INT16 y = lua_tointeger(LS, 2);
		const INT8 adminAdjustment = static_cast<INT8>(lua_tointeger(LS, 3));
		const INT8 troopAdjustment = static_cast<INT8>(lua_tointeger(LS, 4));
		const INT8 eliteAdjustment = static_cast<INT8>(lua_tointeger(LS, 5));
		const INT8 robotAdjustment = gGameExternalOptions.fASDActive && gGameExternalOptions.fASDAssignsRobots ? static_cast<INT8>(lua_tointeger(LS, 6)) : 0;
		const INT8 jeepAdjustment = gGameExternalOptions.fASDActive && gGameExternalOptions.fASDAssignsJeeps ? static_cast<INT8>(lua_tointeger(LS, 7)) : 0;
		const INT8 tankAdjustment = gGameExternalOptions.fASDActive && gGameExternalOptions.fASDAssignsTanks ? static_cast<INT8>(lua_tointeger(LS, 8)) : 0;

		SECTORINFO *sector = &(SectorInfo[SECTOR(x,y)]);
		if (!sector)
			return 0;

		sector->ubNumAdmins = min(max(0, sector->ubNumAdmins + adminAdjustment), 30);
		sector->ubNumTroops = min(max(0, sector->ubNumTroops + troopAdjustment), 30);
		sector->ubNumElites = min(max(0, sector->ubNumElites + eliteAdjustment), 30);
		sector->ubNumRobots = min(max(0, sector->ubNumRobots + robotAdjustment), 30);
		sector->ubNumJeeps = min(max(0, sector->ubNumJeeps + jeepAdjustment), 30);
		sector->ubNumTanks = min(max(0, sector->ubNumTanks + tankAdjustment), 30);

		return 0;
	}

	static int l_AdjustMorale(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));;
		const INT8 val = static_cast<INT8>(lua_tointeger(LS, 2));

		if (val == 0)
			return 0;

		std::for_each(gAllMercs.begin(), gAllMercs.end(), [val, profileId](TacticalActor* merc) {
			if (merc->identity().profile() == profileId)
			{
				merc->morale().strategicModifier() += val;
				merc->morale().strategicModifier() = min(merc->morale().strategicModifier(), gMoraleSettings.bModifiers[MORALE_MOD_MAX]);
				merc->morale().strategicModifier() = max(merc->morale().strategicModifier(), -gMoraleSettings.bModifiers[MORALE_MOD_MAX]);
				RefreshSoldierMorale(merc);
			}
		});

		return 0;
	}

	static int l_AdjustStat(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));;
		const UINT16 stat = lua_tointeger(LS, 2);
		const INT16 val = lua_tointeger(LS, 3);

		if (stat <= STAT_START || stat >= STAT_EXPLEVEL || val == 0)
			return 0;

		std::for_each(gAllMercs.begin(), gAllMercs.end(), [stat, val, profileId](TacticalActor* merc) {
			if (merc->identity().profile() != profileId)
				return;

			INT16 amount = val;
			CHAR16 wTempString[ 128 ];
			int statId = -1;

			switch (stat)
			{
			case STAT_LIFE:
				amount = max(min(100 - merc->vitals().maximumHealth(), amount), -merc->vitals().maximumHealth());
				merc->vitals().maximumHealth() += amount;
				merc->vitals().health() += amount;
				statId = HEALTHAMT;
				merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Health, GetJA2Clock());
				
				if (amount < 0)
				{
					merc->vitals().criticalStatDamage()[DAMAGED_STAT_HEALTH] -= amount;
					gMercProfiles[merc->identity().profile()].bLifeMax = merc->vitals().maximumHealth();
					gMercProfiles[merc->identity().profile()].bLife = min(gMercProfiles[merc->identity().profile()].bLife, gMercProfiles[merc->identity().profile()].bLifeMax);
					merc->statProgress().clearIncreased(HEALTH_INCREASE);
				}
				else if (amount > 0)
				{
					gMercProfiles[merc->identity().profile()].bLifeDelta += amount;
					merc->statProgress().markIncreased(HEALTH_INCREASE);
				}
				break;
			case STAT_STRENGTH:
				amount = max(min(100 - merc->statistics().strength(), amount), -merc->statistics().strength());
				merc->statistics().strength() += amount;
				statId = STRAMT;
				merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Strength, GetJA2Clock());

				if (amount < 0)
				{
					merc->vitals().criticalStatDamage()[DAMAGED_STAT_STRENGTH] -= amount;
					gMercProfiles[merc->identity().profile()].bStrength = merc->statistics().strength();
					merc->statProgress().clearIncreased(STRENGTH_INCREASE);
				}
				else if (amount > 0)
				{
					gMercProfiles[merc->identity().profile()].bStrengthDelta += amount;
					merc->statProgress().markIncreased(STRENGTH_INCREASE);
				}
				break;
			case STAT_AGILITY:
				amount = max(min(100 - merc->statistics().agility(), amount), -merc->statistics().agility());
				merc->statistics().agility() += amount;
				statId = AGILAMT;
				merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Agility, GetJA2Clock());

				if (amount < 0)
				{
					merc->vitals().criticalStatDamage()[DAMAGED_STAT_AGILITY] -= amount;
					gMercProfiles[merc->identity().profile()].bAgility = merc->statistics().agility();
					merc->statProgress().clearIncreased(AGIL_INCREASE);
				}
				else if (amount > 0)
				{
					gMercProfiles[merc->identity().profile()].bAgilityDelta += amount;
					merc->statProgress().markIncreased(AGIL_INCREASE);
				}
				break;
			case STAT_DEXTERITY:
				amount = max(min(100 - merc->statistics().dexterity(), amount), -merc->statistics().dexterity());
				merc->statistics().dexterity() += amount;
				statId = DEXTAMT;
				merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Dexterity, GetJA2Clock());

				if (amount < 0)
				{
					merc->vitals().criticalStatDamage()[DAMAGED_STAT_DEXTERITY] -= amount;
					gMercProfiles[merc->identity().profile()].bDexterity = merc->statistics().dexterity();
					merc->statProgress().clearIncreased(DEX_INCREASE);
				}
				else if (amount > 0)
				{
					gMercProfiles[merc->identity().profile()].bDexterityDelta += amount;
					merc->statProgress().markIncreased(DEX_INCREASE);
				}
				break;
			case STAT_WISDOM:
				amount = max(min(100 - merc->statistics().wisdom(), amount), -merc->statistics().wisdom());
				merc->statistics().wisdom() += amount;
				statId = WISDOMAMT;
				merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Wisdom, GetJA2Clock());

				if (amount < 0)
				{
					merc->vitals().criticalStatDamage()[DAMAGED_STAT_WISDOM] -= amount;
					gMercProfiles[merc->identity().profile()].bWisdom = merc->statistics().wisdom();
					merc->statProgress().clearIncreased(WIS_INCREASE);
				}
				else if (amount > 0)
				{
					gMercProfiles[merc->identity().profile()].bWisdomDelta += amount;
					merc->statProgress().markIncreased(WIS_INCREASE);
				}
				break;
			case STAT_LEADERSHIP:
				amount = max(min(100 - merc->statistics().leadership(), amount), -merc->statistics().leadership());
				merc->statistics().leadership() += amount;
				statId = LDRAMT;
				merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Leadership, GetJA2Clock());

				if (amount < 0)
				{
					gMercProfiles[merc->identity().profile()].bLeadership = merc->statistics().leadership();
					merc->statProgress().clearIncreased(LDR_INCREASE);
				}
				else if (amount > 0)
				{
					gMercProfiles[merc->identity().profile()].bLeadershipDelta += amount;
					merc->statProgress().markIncreased(LDR_INCREASE);
				}
				break;
			case STAT_MARKSMANSHIP:
				amount = max(min(100 - merc->statistics().marksmanship(), amount), -merc->statistics().marksmanship());
				merc->statistics().marksmanship() += amount;
				statId = MARKAMT;
				merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Marksmanship, GetJA2Clock());

				if (amount < 0)
				{
					gMercProfiles[merc->identity().profile()].bMarksmanship = merc->statistics().marksmanship();
					merc->statProgress().clearIncreased(MRK_INCREASE);
				}
				else if (amount > 0)
				{
					gMercProfiles[merc->identity().profile()].bMarksmanshipDelta += amount;
					merc->statProgress().markIncreased(MRK_INCREASE);
				}
				break;
			case STAT_MECHANICAL:
				amount = max(min(100 - merc->statistics().mechanical(), amount), -merc->statistics().mechanical());
				merc->statistics().mechanical() += amount;
				statId = MECHANAMT;
				merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Mechanical, GetJA2Clock());

				if (amount < 0)
				{
					gMercProfiles[merc->identity().profile()].bMechanical = merc->statistics().mechanical();
					merc->statProgress().clearIncreased(MECH_INCREASE);
				}
				else if (amount > 0)
				{
					gMercProfiles[merc->identity().profile()].bMechanicDelta += amount;
					merc->statProgress().markIncreased(MECH_INCREASE);
				}
				break;
			case STAT_EXPLOSIVE:
				amount = max(min(100 - merc->statistics().explosives(), amount), -merc->statistics().explosives());
				merc->statistics().explosives() += amount;
				statId = EXPLODEAMT;
				merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Explosives, GetJA2Clock());

				if (amount < 0)
				{
					gMercProfiles[merc->identity().profile()].bExplosive = merc->statistics().explosives();
					merc->statProgress().clearIncreased(EXP_INCREASE);
				}
				else if (amount > 0)
				{
					gMercProfiles[merc->identity().profile()].bExplosivesDelta += amount;
					merc->statProgress().markIncreased(EXP_INCREASE);
				}
				break;
			case STAT_MEDICAL:
				amount = max(min(100 - merc->statistics().medical(), amount), -merc->statistics().medical());
				merc->statistics().medical() += amount;
				statId = MEDICALAMT;
				merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Medical, GetJA2Clock());

				if (amount < 0)
				{
					gMercProfiles[merc->identity().profile()].bMedical = merc->statistics().medical();
					merc->statProgress().clearIncreased(MED_INCREASE);
				}
				else if (amount > 0)
				{
					gMercProfiles[merc->identity().profile()].bMedicalDelta += amount;
					merc->statProgress().markIncreased(MED_INCREASE);
				}
				break;
			}

			if (amount != 0)
			{
				BuildStatChangeString(wTempString, merc->GetName(), amount > 0, amount > 0 ? amount : -amount, statId);
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, wTempString );
			}
		});

		return 0;
	}

	static int l_AdjustVehicleFuel(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));;
		const INT16 val = lua_tointeger(LS, 2);
		INT32 vehicleId = -1;
		
		for (auto iter = gAllMercs.begin(); iter != gAllMercs.end(); ++iter)
		{
			if ((*iter)->identity().profile() == profileId)
			{
				vehicleId = (*iter)->deployment().vehicleId();
				break;

			}
		}

		if (vehicleId == -1)
		{
			// invalid vehicle id
			lua_pushboolean(LS, false);
			lua_pushstring(LS, "");
			return 2;
		}

		if (vehicleId >= 0 && vehicleId < ubNumberOfVehicles && pVehicleList[vehicleId].fValid == TRUE)
		{
			TacticalActor* vehicle = GetSoldierStructureForVehicle(vehicleId);	// the merc's own vehicle, not the first valid one

			if (vehicle)
			{
				SpendVehicleFuel(vehicle, -(100*val));

				lua_pushboolean(LS, true);
				const MERCPROFILESTRUCT& mps = gMercProfiles[vehicle->identity().profile()];
				CHAR8 nickname[50];
				sprintf(nickname, "%ls", mps.zNickname);
				lua_pushstring(LS, nickname);
				return 2;
			}
		}

		lua_pushboolean(LS, false);
		lua_pushstring(LS, "");
		return 2;
	}

	static int l_AdjustVehicleHealth(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));;
		const INT16 val = lua_tointeger(LS, 2);
		INT32 vehicleId = -1;
		
		for (auto iter = gAllMercs.begin(); iter != gAllMercs.end(); ++iter)
		{
			if ((*iter)->identity().profile() == profileId)
			{
				vehicleId = (*iter)->deployment().vehicleId();
				break;

			}
		}

		if (vehicleId == -1)
		{
			// invalid vehicle id
			lua_pushboolean(LS, false);
			lua_pushstring(LS, "");
			return 2;
		}

		if (vehicleId >= 0 && vehicleId < ubNumberOfVehicles && pVehicleList[vehicleId].fValid == TRUE)
		{
			TacticalActor* vehicle = GetSoldierStructureForVehicle(vehicleId);	// the merc's own vehicle, not the first valid one

			if (vehicle)
			{
				vehicle->vitals().health() += val;
				vehicle->vitals().health() = max(min(vehicle->vitals().health(), 100), 0);

				lua_pushboolean(LS, true);
				const MERCPROFILESTRUCT& mps = gMercProfiles[vehicle->identity().profile()];
				CHAR8 nickname[50];
				sprintf(nickname, "%ls", mps.zNickname);
				lua_pushstring(LS, nickname);
				return 2;
			}
		}

		lua_pushboolean(LS, false);
		lua_pushstring(LS, "");
		return 2;
	}

	static int l_ApplyPermanentStatDamage(lua_State* LS)
	{
		// note that we're intentionally NOT incrementing the damaged stat array (ubCriticalStatDamage) as we do not want these penalties to be doctorable!

		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));
		const UINT16 stat = static_cast<UINT16>(lua_tointeger(LS, 2));
		const int amount = lua_tointeger(LS, 3);

		if (amount <= 0)
			return 0;

		std::for_each(gAllMercs.begin(), gAllMercs.end(), [profileId, stat, amount](TacticalActor* merc) {
			if (merc->identity().profile() != profileId)
				return;

			int loss = amount;

			switch (stat)
			{
			case STAT_LIFE:
				if (loss >= merc->vitals().maximumHealth())
				{
					loss = merc->vitals().maximumHealth() - 1;
				}
				merc->vitals().maximumHealth() -= loss;
				merc->vitals().health() = min(merc->vitals().health(), merc->vitals().maximumHealth());

				if (merc->identity().profile() != NO_PROFILE)
				{
					gMercProfiles[merc->identity().profile()].bLifeMax = merc->vitals().maximumHealth();
					gMercProfiles[merc->identity().profile()].bLife = min(gMercProfiles[merc->identity().profile()].bLife, gMercProfiles[merc->identity().profile()].bLifeMax);
				}

				if (merc->identity().name()[0] && merc->awareness().visibility() == TRUE)
				{
					merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Health, GetJA2Clock());
					merc->statProgress().clearIncreased(HEALTH_INCREASE);
				}
				break;
			case STAT_STRENGTH:
				if (loss >= merc->statistics().strength())
				{
					loss = merc->statistics().strength() - 1;
				}
				merc->statistics().strength() -= loss;

				if (merc->identity().profile() != NO_PROFILE)
				{
					gMercProfiles[ merc->identity().profile() ].bStrength = merc->statistics().strength();
				}

				if (merc->identity().name()[0] && merc->awareness().visibility() == TRUE)
				{
					merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Strength, GetJA2Clock());
					merc->statProgress().clearIncreased(STRENGTH_INCREASE);
				}
				break;
			case STAT_AGILITY:
				if (loss >= merc->statistics().agility())
				{
					loss = merc->statistics().agility() - 1;
				}
				merc->statistics().agility() -= loss;

				if (merc->identity().profile() != NO_PROFILE)
				{
					gMercProfiles[ merc->identity().profile() ].bAgility = merc->statistics().agility();
				}

				if (merc->identity().name()[0] && merc->awareness().visibility() == TRUE)
				{
					merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Agility, GetJA2Clock());
					merc->statProgress().clearIncreased(AGIL_INCREASE);
				}
				break;
			case STAT_DEXTERITY:
				if (loss >= merc->statistics().dexterity())
				{
					loss = merc->statistics().dexterity() - 1;
				}
				merc->statistics().dexterity() -= loss;

				if (merc->identity().profile() != NO_PROFILE)
				{
					gMercProfiles[ merc->identity().profile() ].bDexterity = merc->statistics().dexterity();
				}

				if (merc->identity().name()[0] && merc->awareness().visibility() == TRUE)
				{
					merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Dexterity, GetJA2Clock());
					merc->statProgress().clearIncreased(DEX_INCREASE);
				}
				break;
			case STAT_WISDOM:
				if (loss >= merc->statistics().wisdom())
				{
					loss = merc->statistics().wisdom() - 1;
				}
				merc->statistics().wisdom() -= loss;

				if (merc->identity().profile() != NO_PROFILE)
				{
					gMercProfiles[ merc->identity().profile() ].bWisdom = merc->statistics().wisdom();
				}

				if (merc->identity().name()[0] && merc->awareness().visibility() == TRUE)
				{
					merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Wisdom, GetJA2Clock());
					merc->statProgress().clearIncreased(WIS_INCREASE);
				}
				break;
			case STAT_LEADERSHIP:
				if (loss >= merc->statistics().leadership())
				{
					loss = merc->statistics().leadership() - 1;
				}
				merc->statistics().leadership() -= loss;

				if (merc->identity().profile() != NO_PROFILE)
				{
					gMercProfiles[ merc->identity().profile() ].bLeadership = merc->statistics().leadership();
				}

				if (merc->identity().name()[0] && merc->awareness().visibility() == TRUE)
				{
					merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Leadership, GetJA2Clock());
					merc->statProgress().clearIncreased(LDR_INCREASE);
				}
				break;
			case STAT_MARKSMANSHIP:
				if (loss >= merc->statistics().marksmanship())
				{
					loss = merc->statistics().marksmanship() - 1;
				}
				merc->statistics().marksmanship() -= loss;

				if (merc->identity().profile() != NO_PROFILE)
				{
					gMercProfiles[ merc->identity().profile() ].bMarksmanship = merc->statistics().marksmanship();
				}

				if (merc->identity().name()[0] && merc->awareness().visibility() == TRUE)
				{
					merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Marksmanship, GetJA2Clock());
					merc->statProgress().clearIncreased(MRK_INCREASE);
				}
				break;
			case STAT_MECHANICAL:
				if (loss >= merc->statistics().mechanical())
				{
					loss = merc->statistics().mechanical() - 1;
				}
				merc->statistics().mechanical() -= loss;

				if (merc->identity().profile() != NO_PROFILE)
				{
					gMercProfiles[ merc->identity().profile() ].bMechanical = merc->statistics().mechanical();
				}

				if (merc->identity().name()[0] && merc->awareness().visibility() == TRUE)
				{
					merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Mechanical, GetJA2Clock());
					merc->statProgress().clearIncreased(MECH_INCREASE);
				}
				break;
			case STAT_EXPLOSIVE:
				if (loss >= merc->statistics().explosives())
				{
					loss = merc->statistics().explosives() - 1;
				}
				merc->statistics().explosives() -= loss;

				if (merc->identity().profile() != NO_PROFILE)
				{
					gMercProfiles[ merc->identity().profile() ].bExplosive = merc->statistics().explosives();
				}

				if (merc->identity().name()[0] && merc->awareness().visibility() == TRUE)
				{
					merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Explosives, GetJA2Clock());
					merc->statProgress().clearIncreased(EXP_INCREASE);
				}
				break;
			case STAT_MEDICAL:
				if (loss >= merc->statistics().medical())
				{
					loss = merc->statistics().medical() - 1;
				}
				merc->statistics().medical() -= loss;

				if (merc->identity().profile() != NO_PROFILE)
				{
					gMercProfiles[ merc->identity().profile() ].bMedical = merc->statistics().medical();
				}

				if (merc->identity().name()[0] && merc->awareness().visibility() == TRUE)
				{
					merc->statProgress().recordChange(SoldierStatProgressComponent::Stat::Medical, GetJA2Clock());
					merc->statProgress().clearIncreased(MED_INCREASE);
				}
				break;
			}
		});

		return 0;
	}

	static int l_ApplyDamage(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));
		const int amount = lua_tointeger(LS, 2);

		std::for_each(gAllMercs.begin(), gAllMercs.end(), [profileId, amount](TacticalActor* merc) {
			if (merc->identity().profile() != profileId)
				return;

			int newLifeValue = merc->vitals().health() - amount;
			newLifeValue = max(min(merc->vitals().maximumHealth(), newLifeValue), 0);

			merc->vitals().health() = newLifeValue;

			if (merc->identity().profile() != NO_PROFILE)
			{
				gMercProfiles[merc->identity().profile()].bLife = merc->vitals().health();
			}

			if (merc->vitals().health() <= 0)
			{
				HandleStrategicDeath(merc);
			}
			else if (merc->vitals().health() < 15)
			{
				merc->vitals().health() = 15;
			}
		});

		return 0;
	}

	static int l_CheckForAssignment(lua_State* LS)
	{
		const INT8 assignment = lua_tointeger(LS, 1);
		INT16 sectorX = 0;
		INT16 sectorY = 0;
		INT8 sectorZ = 0;
		bool globalSearch = true;

		if (lua_gettop(LS) == 4)
		{
			globalSearch = false;
			sectorX = lua_tointeger(LS, 2);
			sectorY = lua_tointeger(LS, 3);
			sectorZ = lua_tointeger(LS, 4);
		}

		std::vector<TacticalActor*> foundMercs;

		for (auto iter = gAllMercs.begin(); iter != gAllMercs.end(); ++iter)
		{
			if (globalSearch || ((*iter)->deployment().sectorX() == sectorX && (*iter)->deployment().sectorY() == sectorY && (*iter)->deployment().sectorZ() == sectorZ))
			{
				if ((*iter)->assignment().current() == assignment)
				{
					foundMercs.push_back(*iter);
				}
				else if (assignment == ON_DUTY && (*iter)->assignment().current() < ON_DUTY)
				{
					foundMercs.push_back(*iter);
				}
			}
		}

		if (foundMercs.size() == 0)
		{
			lua_pushboolean(LS, false);
			lua_pushstring(LS, "");
			lua_pushinteger(LS, 0);
		}
		else
		{
			const UINT8 index = Random(foundMercs.size());

			const MERCPROFILESTRUCT& merc = gMercProfiles[foundMercs[index]->identity().profile()];
			CHAR8 nickname[50];
			sprintf(nickname, "%ls", merc.zNickname);
			lua_pushboolean(LS, true);
			lua_pushstring(LS, nickname);
			lua_pushinteger(LS, foundMercs[index]->identity().profile());
		}

		return 3;
	}

	static int l_CheckForSkill(lua_State* LS)
	{
		const INT8 skill = lua_tointeger(LS, 1);
		INT16 sectorX = 0;
		INT16 sectorY = 0;
		INT8 sectorZ = 0;
		INT8 profileId = 0;
		bool globalSearch = true;
		bool searchAllMercs = true;

		if (lua_gettop(LS) == 4)
		{
			globalSearch = false;
			sectorX = lua_tointeger(LS, 2);
			sectorY = lua_tointeger(LS, 3);
			sectorZ = lua_tointeger(LS, 4);
		}

		if (lua_gettop(LS) == 2)
		{
			searchAllMercs = false;
			profileId = static_cast<UINT8>(lua_tointeger(LS, 2));
		}

		std::vector<TacticalActor*> foundMercs;

		if (gGameOptions.fNewTraitSystem)
		{
			for (auto iter = gAllMercs.begin(); iter != gAllMercs.end(); ++iter)
			{
				const MERCPROFILESTRUCT& merc = gMercProfiles[(*iter)->identity().profile()];
				if (searchAllMercs || (*iter)->identity().profile() == profileId)
				{
					for (int i = 0; i < sizeof(merc.bSkillTraits) / sizeof(merc.bSkillTraits[0]); ++i)
					{
						if (globalSearch || ((*iter)->deployment().sectorX() == sectorX && (*iter)->deployment().sectorY() == sectorY && (*iter)->deployment().sectorZ() == sectorZ))
						{
							if(merc.bSkillTraits[i] == skill)
							{
								foundMercs.push_back(*iter);
							}
						}
					}
				}
			}
		}

		if (foundMercs.size() == 0)
		{
			lua_pushboolean(LS, false);
			lua_pushstring(LS, "");
			lua_pushinteger(LS, 0);
		}
		else
		{
			const UINT8 index = Random(foundMercs.size());

			const MERCPROFILESTRUCT& merc = gMercProfiles[foundMercs[index]->identity().profile()];
			CHAR8 nickname[50];
			sprintf(nickname, "%ls", merc.zNickname);
			lua_pushboolean(LS, true);
			lua_pushstring(LS, nickname);
			lua_pushinteger(LS, foundMercs[index]->identity().profile());
		}

		return 3;
	}

	static int l_CheckForSleep(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));

		for (auto iter = gAllMercs.begin(); iter != gAllMercs.end(); ++iter)
		{
			if ((*iter)->identity().profile() == profileId && (*iter)->assignment().isAsleep())
			{
				lua_pushboolean(LS, true);
				return 1;
			}
		}

		lua_pushboolean(LS, false);
		return 1;
	}

	static int l_CheckForTravel(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));

		for (auto iter = gAllMercs.begin(); iter != gAllMercs.end(); ++iter)
		{
			if ((*iter)->identity().profile() == profileId && (*iter)->deployment().isBetweenSectors())
			{
				lua_pushboolean(LS, true);
				return 1;
			}
		}

		lua_pushboolean(LS, false);
		return 1;
	}

	static int l_CheckForTravelOnFoot(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));

		for (auto iter = gAllMercs.begin(); iter != gAllMercs.end(); ++iter)
		{
			if ((*iter)->identity().profile() == profileId && (*iter)->deployment().isBetweenSectors() && (*iter)->assignment().current() != VEHICLE)
			{
				lua_pushboolean(LS, true);
				return 1;
			}
		}

		lua_pushboolean(LS, false);
		return 1;
	}

	static int l_CheckForTravelInHelicopter(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));

		for (auto iter = gAllMercs.begin(); iter != gAllMercs.end(); ++iter)
		{
			if ((*iter)->identity().profile() == profileId && SoldierAboardAirborneHeli(*iter))
			{
				lua_pushboolean(LS, true);
				return 1;
			}
		}

		lua_pushboolean(LS, false);
		return 1;
	}

	static int l_CreateMilitia(lua_State* LS)
	{
		const int greenMilitia = lua_tointeger(LS, 1);
		const int regularMilitia = lua_tointeger(LS, 2);
		const int eliteMilitia = lua_tointeger(LS, 3);
		const INT16 sectorX = lua_tointeger(LS, 4);
		const INT16 sectorY = lua_tointeger(LS, 5);

		if (greenMilitia > 0)
		{
			StrategicAddMilitiaToSector(sectorX, sectorY, GREEN_MILITIA, greenMilitia);
			for (int i = 0; i < greenMilitia; ++i)
			{
				CreateNewIndividualMilitia( GREEN_MILITIA, MO_ARULCO, SECTOR(sectorX, sectorY) );
			}
		}

		if (regularMilitia > 0)
		{
			StrategicAddMilitiaToSector(sectorX, sectorY, REGULAR_MILITIA, regularMilitia);
			for (int i = 0; i < regularMilitia; ++i)
			{
				CreateNewIndividualMilitia( REGULAR_MILITIA, MO_ARULCO, SECTOR(sectorX, sectorY) );
			}
		}

		if (eliteMilitia > 0)
		{
			StrategicAddMilitiaToSector(sectorX, sectorY, ELITE_MILITIA, eliteMilitia);
			for (int i = 0; i < eliteMilitia; ++i)
			{
				CreateNewIndividualMilitia( ELITE_MILITIA, MO_ARULCO, SECTOR(sectorX, sectorY) );
			}
		}

		return 0;
	}

	static int l_GetCoordinates(lua_State* LS)
	{
		const UINT8 profileId = lua_tointeger(LS, 1);

		INT16 x = 0;
		INT16 y = 0;
		INT8 z = 0;
		for (SoldierID i = gTacticalStatus.Team[OUR_TEAM].bFirstID; i <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++i)
		{
			const TacticalActor* merc = GetJa2SoldierRepository().resolve(i);
			if (merc && merc->identity().profile() == profileId)
			{
				x = merc->deployment().sectorX();
				y = merc->deployment().sectorY();
				z = merc->deployment().sectorZ();
				break;
			}
		}

		// invalid profileid
		lua_pushinteger(LS, x);
		lua_pushinteger(LS, y);
		lua_pushinteger(LS, z);
		return 3;
	}

	static int l_GetHealth(lua_State* LS)
	{
		const UINT8 profileId = lua_tointeger(LS, 1);

		for (auto iter = gAllMercs.begin(); iter != gAllMercs.end(); ++iter)
		{
			if ((*iter)->identity().profile() == profileId)
			{
				lua_pushinteger(LS, (*iter)->vitals().health());
				lua_pushinteger(LS, (*iter)->vitals().maximumHealth());
				return 2;
			}
		}

		lua_pushinteger(LS, 0);
		lua_pushinteger(LS, 0);
		return 2;
	}

	static int l_GetHoursRemainingOnMiniEvent(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));

		for (auto iter = gAllMercs.begin(); iter != gAllMercs.end(); ++iter)
		{
			if ((*iter)->identity().profile() == profileId)
			{
				lua_pushinteger(LS, (*iter)->assignment().miniEventHoursRemaining());
				return 1;
			}
		}

		lua_pushinteger(LS, -1);
		return 1;
	}

	static int l_GetSectorIDString(lua_State* LS)
	{
		const INT16 x = lua_tointeger(LS, 1);
		const INT16 y = lua_tointeger(LS, 2);
		const INT8 z = lua_tointeger(LS, 3);
		CHAR16 sectorName[512];
		GetSectorIDString(x, y, z, sectorName, FALSE);
		std::wstring wsSectorName(sectorName);
		std::string strSectorName(wsSectorName.begin(), wsSectorName.end());

		lua_pushstring(LS, strSectorName.c_str());
		return 1;
	}

	static int l_GetSkills(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));

		lua_newtable(LS);
		for (auto iter = gAllMercs.begin(); iter != gAllMercs.end(); ++iter)
		{
			if ((*iter)->identity().profile() == profileId)
			{
				const INT8* skillTraits = gMercProfiles[(*iter)->identity().profile()].bSkillTraits;
				for (int i = 0; i < sizeof(gMercProfiles[(*iter)->identity().profile()].bSkillTraits) / sizeof(gMercProfiles[(*iter)->identity().profile()].bSkillTraits[0]) ;++i)
				{
					const int skillId = *(skillTraits + i);
					if (skillId > 0)
					{
						lua_pushinteger(LS, i + 1);
						lua_pushinteger(LS, skillId);
						lua_settable(LS, -3);
					}
					else
					{
						break;
					}
				}
			}
		}
		
		return 1;
	}

	static int l_GetStat(lua_State* LS)
	{
		const UINT16 stat = lua_tointeger(LS, 1);
		INT16 sectorX = 0;
		INT16 sectorY = 0;
		INT8 sectorZ = 0;
		UINT8 profileId = 0;
		bool globalSearch = true;
		bool lookAtAllMercs = true;

		if (lua_gettop(LS) == 4)
		{
			globalSearch = false;
			sectorX = lua_tointeger(LS, 2);
			sectorY = lua_tointeger(LS, 3);
			sectorZ = lua_tointeger(LS, 4);
		}

		if (lua_gettop(LS) == 2)
		{
			lookAtAllMercs = false;
			profileId = static_cast<UINT8>(lua_tointeger(LS, 2));
		}

		INT8 bestStat = 0;
		if (gAllMercs.empty())	// no mercs -> gAllMercs[0] is OOB and bestSoldier would be dereferenced below
		{
			lua_pushinteger(LS, 0);
			lua_pushstring(LS, "");
			lua_pushinteger(LS, 0);
			return 3;
		}
		TacticalActor* bestSoldier = gAllMercs[0];
		for (auto iter = gAllMercs.begin(); iter != gAllMercs.end(); ++iter)
		{
			if (lookAtAllMercs || ((*iter)->identity().profile() == profileId))
			{
				if (globalSearch || ((*iter)->deployment().sectorX() == sectorX && (*iter)->deployment().sectorY() == sectorY && (*iter)->deployment().sectorZ() == sectorZ))
				{
					switch (stat)
					{
						case STAT_LIFE:
							if ((*iter)->vitals().maximumHealth() > bestStat)
							{
								bestStat = (*iter)->vitals().maximumHealth();
								bestSoldier = *iter;
							}
							break;
						case STAT_STRENGTH:
							if ((*iter)->statistics().strength() > bestStat)
							{
								bestStat = (*iter)->statistics().strength();
								bestSoldier = *iter;
							}
							break;
						case STAT_AGILITY:
							if ((*iter)->statistics().agility() > bestStat)
							{
								bestStat = (*iter)->statistics().agility();
								bestSoldier = *iter;
							}
							break;
						case STAT_DEXTERITY:
							if ((*iter)->statistics().dexterity() > bestStat)
							{
								bestStat = (*iter)->statistics().dexterity();
								bestSoldier = *iter;
							}
							break;
						case STAT_WISDOM:
							if ((*iter)->statistics().wisdom() > bestStat)
							{
								bestStat = (*iter)->statistics().wisdom();
								bestSoldier = *iter;
							}
							break;
						case STAT_LEADERSHIP:
							if ((*iter)->statistics().leadership() > bestStat)
							{
								bestStat = (*iter)->statistics().leadership();
								bestSoldier = *iter;
							}
							break;
						case STAT_MARKSMANSHIP:
							if ((*iter)->statistics().marksmanship() > bestStat)
							{
								bestStat = (*iter)->statistics().marksmanship();
								bestSoldier = *iter;
							}
							break;
						case STAT_MECHANICAL:
							if ((*iter)->statistics().mechanical() > bestStat)
							{
								bestStat = (*iter)->statistics().mechanical();
								bestSoldier = *iter;
							}
							break;
						case STAT_EXPLOSIVE:
							if ((*iter)->statistics().explosives() > bestStat)
							{
								bestStat = (*iter)->statistics().explosives();
								bestSoldier = *iter;
							}
							break;
						case STAT_MEDICAL:
							if ((*iter)->statistics().medical() > bestStat)
							{
								bestStat = (*iter)->statistics().medical();
								bestSoldier = *iter;
							}
							break;
						case STAT_EXPLEVEL:
							if ((*iter)->statistics().experienceLevel() > bestStat)
							{
								bestStat = (*iter)->statistics().experienceLevel();
								bestSoldier = *iter;
							}
							break;
					}
				}
			}
		}

		CHAR8 nickname[50];
		sprintf(nickname, "%ls", gMercProfiles[bestSoldier->identity().profile()].zNickname);
		lua_pushinteger(LS, bestStat);
		lua_pushstring(LS, nickname);
		lua_pushinteger(LS, bestSoldier->identity().profile());
		return 3;
	}

	static int l_GetProgress(lua_State* LS)
	{
		lua_pushinteger(LS, HighestPlayerProgressPercentage());
		return 1;
	}

	static int l_GetTownId(lua_State* LS)
	{
		const INT16 x = lua_tointeger(LS, 1);
		const INT16 y = lua_tointeger(LS, 2);

		const UINT8 townId = GetTownIdForSector(x, y);
		CHAR8 townName[MAX_TOWN_NAME_LENGHT];
		sprintf( townName, "%ls", pTownNames[townId] );

		lua_pushinteger(LS, townId);
		lua_pushstring( LS, townName );
		return 2;
	}

	static int l_SendMercOnMiniEvent(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));
		const UINT16 hoursOnMiniEvent = static_cast<UINT16>(lua_tointeger(LS, 2));

		if (hoursOnMiniEvent == 0)
			return 0;

		std::for_each(gAllMercs.begin(), gAllMercs.end(), [profileId, hoursOnMiniEvent](TacticalActor* merc) {
			if (merc->identity().profile() != profileId)
				return;

			TakeSoldierOutOfVehicle(merc);
			RemoveCharacterFromSquads(merc);
			merc->assignment().miniEventHoursRemaining() = hoursOnMiniEvent;
			merc->deployment().sectorZ() += MINI_EVENT_Z_OFFSET;
			merc->vitals().bleeding() = 0;
			SetTimeOfAssignmentChangeForMerc(merc);
			ChangeSoldiersAssignment(merc, ASSIGNMENT_MINIEVENT);
			
			// see HandleMiniEventAssignments() in Assignments.cpp for returning merc to normal
		});

		return 0;
	}

	static int l_SetEnemyGroupVisibility(lua_State* LS)
	{
		const bool visible = lua_toboolean(LS, 1);
		INT16 x = 0;
		INT16 y = 0;

		if (lua_gettop(LS) == 3)
		{
			x = lua_tointeger(LS, 2);
			y = lua_tointeger(LS, 3);
		}

		if (x == 0 && y == 0)
		{
			for ( INT16 sX = 1; sX < (MAP_WORLD_X - 1); ++sX )
			{
				for ( INT16 sY = 1; sY < (MAP_WORLD_Y - 1); ++sY )
				{
					if (visible)
					{
						SectorInfo[SECTOR(sX, sY)].ubDetectionLevel |= (1 << 2);
					}
					else
					{
						SectorInfo[SECTOR(sX, sY)].ubDetectionLevel &= ~(1 << 2);
					}
				}
			}
		}
		else
		{
			if (visible)
			{
				SectorInfo[SECTOR(x,y)].ubDetectionLevel |= (1 << 2);
			}
			else
			{
				SectorInfo[SECTOR(x,y)].ubDetectionLevel &= ~(1 << 2);
			}
		}

		fMapPanelDirty = true;

		return 0;
	}

	static int l_SetMercCoordinates(lua_State* LS)
	{
		const UINT8 profileId = static_cast<UINT8>(lua_tointeger(LS, 1));
		const INT16 sectorX = lua_tointeger(LS, 2);
		const INT16 sectorY = lua_tointeger(LS, 3);
		const INT8 sectorZ = lua_tointeger(LS, 4);

		std::for_each(gAllMercs.begin(), gAllMercs.end(), [profileId, sectorX, sectorY, sectorZ](TacticalActor* merc) {
			if (merc->identity().profile() != profileId)
				return;

			merc->deployment().sectorX() = sectorX;
			merc->deployment().sectorY() = sectorY;
			merc->deployment().sectorZ() = sectorZ;

			TakeSoldierOutOfVehicle(merc);
			RemoveCharacterFromSquads(merc);
			merc->deployment().insertionDirection() = DIRECTION_IRRELEVANT;
			merc->deployment().strategicInsertionCode() = INSERTION_CODE_CENTER;
			AddCharacterToAnySquad(merc);
			});

		return 0;
	}
}

void InitMiniEvents()
{
	// if a user really wants to reschedule an event by disabling this flag, saving their game, re-enabling the flag, then loading their game, then fine. it's a single player game, be my guest
	if (gGameExternalOptions.fMiniEventsEnabled == false || is_networked)
	{
		DeleteAllStrategicEventsOfType(EVENT_MINIEVENT);
	}
	else
	{
		if (GetAllStrategicEventsOfType(EVENT_MINIEVENT).size() == 0)
			QueueNextMiniEvent(0, 0);

		const char* filename = "scripts\\MiniEvents.lua";

		if (gLS() != nullptr)
		{
			// clear old lua state (on load or new game)
			LuaState::CLOSE(gLS);
		}
		gLS = LuaState::INIT(true);

		SGP_THROW_IFFALSE( gLS.EvalFile(filename), _BS("Cannot open file: ") << filename << _BS::cget );

		lua_register(gLS(), "CScreenMsg", MiniEventsLua_ScreenMsg);
		lua_register(gLS(), "CMsgBox", MiniEventsLua_MessageBox);
		lua_register(gLS(), "CResolveEvent", MiniEventsLua_ResolveEvent);

		using namespace MiniEventHelpers;
		{
			lua_register(gLS(), "CAddIntel", l_AddIntel);
			lua_register(gLS(), "CAddMoneyToPlayerAccount", l_AddMoneyToPlayerAccount);
			lua_register(gLS(), "CAddTownLoyalty", l_AddTownLoyalty);
			lua_register(gLS(), "CAddSkill", l_AddSkill);
			lua_register(gLS(), "CAdjustBreathMax", l_AdjustBreathMax);
			lua_register(gLS(), "CAdjustMorale", l_AdjustMorale);
			lua_register(gLS(), "CAdjustEnemyStrengthInSector", l_AdjustEnemyStrengthInSector);
			lua_register(gLS(), "CAdjustStat", l_AdjustStat);
			lua_register(gLS(), "CAdjustVehicleFuel", l_AdjustVehicleFuel);
			lua_register(gLS(), "CAdjustVehicleHealth", l_AdjustVehicleHealth);
			lua_register(gLS(), "CApplyDamage", l_ApplyDamage);
			lua_register(gLS(), "CApplyPermanentStatDamage", l_ApplyPermanentStatDamage);
			lua_register(gLS(), "CCheckForAssignment", l_CheckForAssignment);
			lua_register(gLS(), "CCheckForSkill", l_CheckForSkill);
			lua_register(gLS(), "CCheckForSleep", l_CheckForSleep);
			lua_register(gLS(), "CCheckForTravel", l_CheckForTravel);
			lua_register(gLS(), "CCheckForTravelOnFoot", l_CheckForTravelOnFoot);
			lua_register(gLS(), "CCheckForTravelInHelicopter", l_CheckForTravelInHelicopter);
			lua_register(gLS(), "CCreateMilitia", l_CreateMilitia);
			lua_register(gLS(), "CGetCoordinates", l_GetCoordinates);
			lua_register(gLS(), "CGetHealth", l_GetHealth);
			lua_register(gLS(), "CGetHoursRemainingOnMiniEvent", l_GetHoursRemainingOnMiniEvent);
			lua_register(gLS(), "CGetSectorIDString", l_GetSectorIDString);
			lua_register(gLS(), "CGetSkills", l_GetSkills);
			lua_register(gLS(), "CGetStat", l_GetStat);
			lua_register(gLS(), "CGetProgress", l_GetProgress);
			lua_register(gLS(), "CGetTownId", l_GetTownId);
			lua_register(gLS(), "CSendMercOnMiniEvent", l_SendMercOnMiniEvent);
			lua_register(gLS(), "CSetEnemyGroupVisibility", l_SetEnemyGroupVisibility);
			lua_register(gLS(), "CSetMercCoordinates", l_SetMercCoordinates);
		}
	}
}

void CheckMiniEvents(UINT32 nextEventId)
{
	if (gGameExternalOptions.fMiniEventsEnabled == false || is_networked)
		return;

	// no events if we're in combat or a hostile sector
	if ((IsJa2TacticalCombatActive()) || gTacticalStatus.fEnemyInSector)
		return;

	StopTimeCompression();
	MiniEventsLua(nextEventId);
}

static void QueueNextMiniEvent(UINT32 nextEventId, UINT32 hoursToNextMiniEvent)
{
	const UINT32 timestamp = GetWorldTotalMin() + gGameExternalOptions.fMiniEventsMinHoursBetweenEvents * 60 + Random((gGameExternalOptions.fMiniEventsMaxHoursBetweenEvents - gGameExternalOptions.fMiniEventsMinHoursBetweenEvents) * 60);
	AddStrategicEvent(EVENT_MINIEVENT, hoursToNextMiniEvent > 0 ? GetWorldTotalMin() + 60 * hoursToNextMiniEvent : timestamp, nextEventId);
}

// LUA STUFF FOLLOWS

void MiniEventsLua(UINT32 eventId)
{
	gAllMercs.clear();

	// get all mercs eligible to get a mini event
	for (SoldierID cnt = gTacticalStatus.Team[OUR_TEAM].bFirstID; cnt <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++cnt)
	{
		TacticalActor* pSoldier = GetJa2SoldierRepository().resolve(cnt);

		if (pSoldier && pSoldier->roster().active()
			&& pSoldier->vitals().health() > 0
			&& pSoldier->assignment().current() != IN_TRANSIT
			&& pSoldier->assignment().current() != ASSIGNMENT_POW
			&& pSoldier->assignment().current() != ASSIGNMENT_REBELCOMMAND
			&& !(pSoldier->status().flags() & SOLDIER_VEHICLE))
		{
			gAllMercs.push_back(pSoldier);
		}
	}

	if (eventId > 0)
	{
		LuaFunction f = LuaFunction(gLS, "BeginSpecificEvent");
		// first param: the event to trigger
		f.Param<int>(eventId);

		// second param: a table containing basic info about all of the player's mercs ({ nickname = profileid })
		f.TableOpen();
		for (SoldierID i = gTacticalStatus.Team[OUR_TEAM].bFirstID; i <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++i)
		{
			const TacticalActor* merc = GetJa2SoldierRepository().resolve(i);
			if (merc && merc->roster().active() && merc->assignment().current() != IN_TRANSIT && !(merc->status().flags() & SOLDIER_VEHICLE) && !(AM_A_ROBOT(merc)))
			{
				std::wstring ws(gMercProfiles[merc->identity().profile()].zNickname);
				std::string str(ws.begin(), ws.end());
				f.TParam(str.c_str(), (int)merc->identity().profile());
			}
		}
		f.TableClose();

		SGP_THROW_IFFALSE(f.Call(2), "call to lua function BeginSpecificEvent failed");
	}
	else
	{
		LuaFunction f = LuaFunction(gLS, "BeginRandomEvent");
		// first param: a table containing basic info about all of the player's mercs ({ nickname = profileid })
		f.TableOpen();
		for ( SoldierID i = gTacticalStatus.Team[OUR_TEAM].bFirstID; i <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++i)
		{
			const TacticalActor* merc = GetJa2SoldierRepository().resolve(i);
			if (merc && merc->roster().active() && merc->assignment().current() != IN_TRANSIT && !(merc->status().flags() & SOLDIER_VEHICLE) && !(AM_A_ROBOT(merc)))
			{
				std::wstring ws(gMercProfiles[merc->identity().profile()].zNickname);
				std::string str(ws.begin(), ws.end());
				f.TParam(str.c_str(), (int)merc->identity().profile());
			}
		}
		f.TableClose();

		SGP_THROW_IFFALSE(f.Call(1), "call to lua function BeginRandomEvent failed");
	}
}

static int MiniEventsLua_MessageBox(lua_State* LS)
{
	size_t len = 0;
	CHAR16 w_str[500];

	std::string str = lua_tolstring(LS, 1, &len);
	MultiByteToWideChar( CP_UTF8, 0, len > MAX_BUTTON_LENGTH ? str.substr(0, MAX_BUTTON_LENGTH).c_str() : str.c_str(), -1, w_str, sizeof(w_str) / sizeof(w_str[0]) );
	w_str[sizeof(w_str) / sizeof(w_str[0]) - 1] = '\0';
	wcscpy( gzUserDefinedButton1, w_str );

	str = lua_tolstring(LS, 2, &len);
	MultiByteToWideChar( CP_UTF8, 0, len > MAX_BUTTON_LENGTH ? str.substr(0, MAX_BUTTON_LENGTH).c_str() : str.c_str(), -1, w_str, sizeof(w_str) / sizeof(w_str[0]) );
	w_str[sizeof(w_str) / sizeof(w_str[0]) - 1] = '\0';
	wcscpy( gzUserDefinedButton2, w_str );

	str = lua_tolstring(LS, 3, &len);
	MultiByteToWideChar( CP_UTF8, 0, len > MAX_BODY_LENGTH ? str.substr(0, MAX_BODY_LENGTH).c_str() : str.c_str(), -1, w_str, sizeof(w_str) / sizeof(w_str[0]) );
	w_str[sizeof(w_str) / sizeof(w_str[0]) - 1] = '\0';

	// we need to cache the screen here so that the second msgbox doesn't keep the global screen state in MSG_BOX_SCREEN (causes infinite recursion)
	guiMiniEventsCachedScreen = GetCurrentScreen();
	DoMessageBox(MSG_BOX_MINIEVENT_STYLE, w_str,
		GetCurrentScreen(), MSG_BOX_FLAG_WIDE_BUTTONS | MSG_BOX_FLAG_BIGGER, MiniEventsLua_MessageBoxCallback , NULL);

	return 0;
}

static void MiniEventsLua_MessageBoxCallback(UINT8 ubExitValue)
{
	// appears to be first button = 2, second button = 3
	gfDontOverRideSaveBuffer = FALSE;
	LuaFunction(gLS, "ResolveMsgBox" ).Param<bool>(ubExitValue == 2).Call(1);
}

static int MiniEventsLua_ResolveEvent(lua_State* LS)
{
	size_t len = 0;
	std::string str = lua_tolstring(LS, 1, &len);

	UINT32 nextEventId = 0;
	if (lua_gettop(LS) >= 2)
		nextEventId = static_cast<UINT32>(lua_tointeger(LS, 2));

	UINT32 hoursToNextMiniEvent = 0;
	if (lua_gettop(LS) == 3)
		hoursToNextMiniEvent = static_cast<UINT32>(lua_tointeger(LS, 3));

	CHAR16 w_str[500];
	MultiByteToWideChar( CP_UTF8, 0, len > MAX_BODY_LENGTH ? str.substr(0, MAX_BODY_LENGTH).c_str() : str.c_str(), -1, w_str, sizeof(w_str) / sizeof(w_str[0]) );
	w_str[sizeof(w_str) / sizeof(w_str[0]) - 1] = '\0';

	MSYS_RemoveRegion(&(gMsgBox.BackRegion));
	DoMessageBox(MSG_BOX_MINIEVENT_STYLE, w_str,
		guiMiniEventsCachedScreen, MSG_BOX_FLAG_OK | MSG_BOX_FLAG_BIGGER, [](UINT8 ubExitValue) { gfDontOverRideSaveBuffer = FALSE; }, NULL);

	QueueNextMiniEvent(nextEventId, hoursToNextMiniEvent);

	return 0;
}

static int MiniEventsLua_ScreenMsg(lua_State* LS)
{
	size_t len = 0;
	std::string str = lua_tolstring(LS, 1, &len);

	CHAR16 w_str[250];
	MultiByteToWideChar( CP_UTF8, 0, len > 250 ? str.substr(0, 250).c_str() : str.c_str(), -1, w_str, sizeof(w_str) / sizeof(w_str[0]) );
	w_str[sizeof(w_str) / sizeof(w_str[0]) - 1] = '\0';

	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"%s", w_str );
	return 0;
}

