#pragma once

// Wire declarations shared by the standalone coordinator and its loopback
// session test. Field order and widths are part of the legacy client protocol.

#include <cstdint>

#include "../InterruptWire.h"

typedef uint8_t UINT8;
typedef int8_t INT8;
typedef uint16_t UINT16;
typedef int16_t INT16;
typedef uint32_t UINT32;
typedef int32_t INT32;
typedef int64_t INT64;
typedef float FLOAT;
typedef unsigned char BOOLEAN;
typedef char STRING512[512];

#define MAX_PREGENERATED_NUMS 256

// Portable INT wire format (Multiplayer/client.cpp SerializeINT): an 8-byte
// header followed by persons+1 little-endian UINT16 order entries. The tactical
// repository currently has 1284 soldier slots, so persons is at most 1283.
static constexpr UINT16 COORDINATOR_INT_WIRE_ORDER_ENTRIES = MpInterruptWire::kSoldierSlots;
static constexpr UINT16 COORDINATOR_INT_WIRE_HEADER_BYTES = MpInterruptWire::kHeaderBytes;
static constexpr UINT16 COORDINATOR_INT_WIRE_MAX_BYTES =
	static_cast<UINT16>(MpInterruptWire::kMaxBytes);

struct admin_cmd_struct { UINT8 cmd; char password[64]; };

struct client_info
{
	UINT8 client_num;
	char client_name[30];
	int team;
	int cl_edge;
	char client_version[30];
};

struct settings_struct
{
	UINT8 maxClients;
	UINT8 sameMercAllowed;
	float damageMultiplier;
	INT16 gsMercArriveSectorX;
	INT16 gsMercArriveSectorY;
	UINT8 enemyEnabled;
	UINT8 creatureEnabled;
	UINT8 militiaEnabled;
	UINT8 civEnabled;
	UINT8 gameType;
	INT32 secondsPerTick;
	INT32 startingCash;
	UINT8 disableBobbyRay;
	UINT8 disableMercEquipment;
	BOOLEAN sofGunNut;
	UINT8 soubGameStyle;
	UINT8 soubDifficultyLevel;
	UINT8 soubSkillTraits;
	BOOLEAN sofTurnTimeLimit;
	BOOLEAN sofIronManMode;
	UINT8 soubBobbyRayQuality;
	UINT8 soubBobbyRayQuantity;
	UINT8 maxMercs;
	UINT8 client_num;
	char client_name[30];
	char client_names[4][30];
	int client_edges[5];
	int client_teams[4];
	char server_name[30];
	int team;
	char kitBag[100];
	UINT8 disableMorale;
	UINT8 reportHiredMerc;
	UINT8 startingSectorEdge;
	float startingTime;
	UINT8 weaponReadyBonus;
	UINT8 inventoryAttachment;
	UINT8 disableSpectatorMode;
	UINT8 randomStartingEdge;
	UINT8 randomMercs;
	int random_mercs[7];
	char server_version[30];
	UINT32 random_table[MAX_PREGENERATED_NUMS];
};

struct filetransfersettings_struct
{
	STRING512 fileTransferDirectory;
	int syncClientsDirectory;
	char serverName[30];
	INT64 totalTransferBytes;
};
static_assert(sizeof(filetransfersettings_struct) == 560,
	"filetransfersettings_struct wire size changed");

struct mapchange_struct { INT16 gsMercArriveSectorX; INT16 gsMercArriveSectorY; float startingTime; };
struct edgechange_struct { UINT8 client_num; UINT8 newedge; };
struct teamchange_struct { UINT8 client_num; UINT8 newteam; };
struct kickR { UINT8 ubResult; };
struct ready_struct { UINT8 client_num; bool status; UINT8 ready_stage; };
struct real_struct { INT8 bteam; };
struct turn_struct { UINT8 tsnetbTeam; UINT8 tsubNextTeam; };
struct sc_struct { UINT8 ubStartingTeam; };
struct death_struct { UINT16 soldier_id; UINT16 attacker_id; UINT8 attacker_team; UINT8 soldier_team; };
struct player_stats { int kills; int deaths; int hits; int misses; };

// These declarations deliberately avoid pulling engine headers into the
// data-free coordinator. Pin every copied legacy ABI size so a compiler/target
// drift fails at build time instead of silently changing MP v3.2 on the wire.
static_assert(sizeof(bool) == 1, "legacy multiplayer wire requires 1-byte bool");
static_assert(sizeof(int) == 4, "legacy multiplayer wire requires 4-byte int");
static_assert(sizeof(float) == 4, "legacy multiplayer wire requires 4-byte float");
static_assert(sizeof(admin_cmd_struct) == 65, "admin_cmd_struct wire size changed");
static_assert(sizeof(client_info) == 72, "client_info wire size changed");
static_assert(sizeof(settings_struct) == 1464, "settings_struct wire size changed");
static_assert(sizeof(mapchange_struct) == 8, "mapchange_struct wire size changed");
static_assert(sizeof(edgechange_struct) == 2, "edgechange_struct wire size changed");
static_assert(sizeof(teamchange_struct) == 2, "teamchange_struct wire size changed");
static_assert(sizeof(kickR) == 1, "kickR wire size changed");
static_assert(sizeof(ready_struct) == 3, "ready_struct wire size changed");
static_assert(sizeof(real_struct) == 1, "real_struct wire size changed");
static_assert(sizeof(turn_struct) == 2, "turn_struct wire size changed");
static_assert(sizeof(sc_struct) == 1, "sc_struct wire size changed");
static_assert(sizeof(death_struct) == 6, "death_struct wire size changed");
static_assert(sizeof(player_stats) == 16, "player_stats wire size changed");

enum { ADMIN_CMD_AUTH = 1, ADMIN_CMD_START = 2 };
enum { MP_TYPE_DEATHMATCH, MP_TYPE_TEAMDEATMATCH, MP_TYPE_COOP };

// Coordinator-owned lobby/placement barriers advance when every currently
// connected participant has reached the relevant stage. This pure decision is
// shared with the data-free loopback test so disconnect re-evaluation cannot
// regress into a permanently wedged session.
enum class CoordinatorBarrierAction : UINT8
{
	None,
	StartBattle,
	UnlockPlacement,
	EnterTactical
};

inline CoordinatorBarrierAction CoordinatorNextBarrierAction(
	bool laptopUnlocked, bool battleStarted, bool placementUnlocked,
	bool tacticalEntered, int connected, int standingSides,
	int ready, int loaded, int placed) noexcept
{
	if (connected <= 0 || standingSides < 2) return CoordinatorBarrierAction::None;
	if (laptopUnlocked && !battleStarted && ready >= connected)
		return CoordinatorBarrierAction::StartBattle;
	if (battleStarted && !placementUnlocked && loaded >= connected)
		return CoordinatorBarrierAction::UnlockPlacement;
	if (placementUnlocked && !tacticalEntered && placed >= connected)
		return CoordinatorBarrierAction::EnterTactical;
	return CoordinatorBarrierAction::None;
}

inline bool CoordinatorTransportTeamActive(bool connected, bool wiped) noexcept
{
	return connected && !wiped;
}

// Deathmatch counts each occupied transport slot as its own side. Team
// deathmatch instead collapses active slots onto their validated selected
// alliance (0..3), both for the lobby's two-side start requirement and for the
// last-standing decision after wipes/disconnects.
inline int CoordinatorStandingSideCount(
	int gameType, const bool connected[4], const bool wiped[4],
	const int selectedTeams[4], int* lastSide = nullptr) noexcept
{
	bool seen[4] = { false, false, false, false };
	int count = 0;
	int last = -1;
	for (int slot = 0; slot < 4; ++slot)
	{
		if (!CoordinatorTransportTeamActive(connected[slot], wiped[slot])) continue;
		int side = gameType == MP_TYPE_TEAMDEATMATCH ? selectedTeams[slot] : slot;
		if (side < 0 || side >= 4 || seen[side]) continue;
		seen[side] = true;
		++count;
		last = side;
	}
	if (lastSide) *lastSide = last;
	return count;
}

#define MP_EDGE_NORTH 0
#define MP_EDGE_EAST 1
#define MP_EDGE_SOUTH 2
#define MP_EDGE_WEST 3
#define MP_EDGE_CENTER 4

#define MPVERSION "MP v3.2"
