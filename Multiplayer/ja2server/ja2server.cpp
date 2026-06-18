// ============================================================================
//  ja2server -- standalone JA2 1.13 multiplayer coordinator / relay server.
//
//  Speaks the netshim (RakNet-3.401-compat over SDL3_net TCP) wire protocol, so
//  stock JA2 MP clients connect to it UNCHANGED. Unlike the old "--dedicated"
//  headless-game mode, this is a PURE coordinator: no game engine, no SDL video,
//  no game loop, and -- crucially -- NO loopback "player 1". The server is never
//  a participant; it only assigns client numbers, brokers the lobby/start
//  handshake, and relays in-game RPC traffic between the real clients.
//
//  Design + scope: BUG_NOTES.md "NEXT MAJOR: standalone central-server executable".
//  Milestone 1 (this file): lobby -> load-into-sector. ~45 of 47 server RPCs are
//  pure relays (rebroadcast wire bytes); the coordinator logic lives in
//  requestSETTINGS / sendREADY / adminCmd / requestFILE_TRANSFER_SETTINGS /
//  disconnect. Combat turn-sequencing (startCOMBAT -> EndTurn state machine) is
//  Milestone 2; it is stubbed here and logged.
//
//  Build: cmake -DBUILD_JA2SERVER=ON ...  (see tests/CMakeLists.txt)
// ============================================================================

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <csignal>
#include <string>
#include <map>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>

// ---- netshim (RakNet-3.401 API-compat over SDL3_net) -----------------------
#include "MessageIdentifiers.h"
#include "RakNetworkFactory.h"
#include "RakPeerInterface.h"
#include "RakNetTypes.h"
#include "RakSleep.h"

// ---- game type shim --------------------------------------------------------
// Match sgp/types.h widths on this platform so the packet structs below have the
// IDENTICAL byte layout the clients compile (default alignment, no #pragma pack
// -- the game does not pack these either). Same compiler + same platform + same
// field types == same layout, which is exactly how client<->server compat works.
typedef uint8_t  UINT8;
typedef int8_t   INT8;
typedef uint16_t UINT16;
typedef int16_t  INT16;
typedef uint32_t UINT32;
typedef int32_t  INT32;
typedef float    FLOAT;
typedef unsigned char BOOLEAN;
typedef char     STRING512[512];
#define MAX_PREGENERATED_NUMS 256

// ---- packet structs (copied VERBATIM from Multiplayer/network.h, connect.h,
//      client.cpp -- field order/types must not change or the wire breaks) -----
typedef struct
{
	UINT8 cmd;
	char  password[64];
} admin_cmd_struct;                         // connect.h enum below

typedef struct
{
	UINT8 client_num;
	char client_name[30];
	int team;
	int cl_edge;
	char client_version[30];
} client_info;

typedef struct
{
	UINT8	maxClients;
	UINT8	sameMercAllowed;
	float	damageMultiplier;
	INT16	gsMercArriveSectorX;
	INT16	gsMercArriveSectorY;
	UINT8	enemyEnabled;
	UINT8	creatureEnabled;
	UINT8	militiaEnabled;
	UINT8	civEnabled;
	UINT8	gameType;
	INT32	secondsPerTick;
	INT32	startingCash;
	UINT8	disableBobbyRay;
	UINT8	disableMercEquipment;
	BOOLEAN sofGunNut;
	UINT8	soubGameStyle;
	UINT8	soubDifficultyLevel;
	UINT8	soubSkillTraits;
	BOOLEAN	sofTurnTimeLimit;
	BOOLEAN	sofIronManMode;
	UINT8	soubBobbyRayQuality;
	UINT8	soubBobbyRayQuantity;
	UINT8	maxMercs;
	UINT8	client_num;
	char	client_name[30];
	char	client_names[4][30];
	int		client_edges[5];
	int		client_teams[4];
	char	server_name[30];
	int		team;
	char	kitBag[100];
	UINT8	disableMorale;
	UINT8	reportHiredMerc;
	UINT8	startingSectorEdge;
	float	startingTime;
	UINT8	weaponReadyBonus;
	UINT8	inventoryAttachment;
	UINT8	disableSpectatorMode;
	UINT8	randomStartingEdge;
	UINT8	randomMercs;
	int		random_mercs[7];
	char	server_version[30];
	UINT32	random_table[MAX_PREGENERATED_NUMS];
} settings_struct;

typedef struct
{
	STRING512 fileTransferDirectory;
	int syncClientsDirectory;
	char serverName[30];
	long totalTransferBytes;
} filetransfersettings_struct;

typedef struct
{
	INT16 gsMercArriveSectorX;
	INT16 gsMercArriveSectorY;
	float startingTime;
} mapchange_struct;

typedef struct                              // client.cpp:435
{
	UINT8 client_num;
	bool status;
	UINT8 ready_stage;
} ready_struct;

typedef struct                              // real_struct -- realtime changeover vote
{
	INT8 bteam;
} real_struct;

typedef struct                              // client.cpp turn_struct -- turn handoff
{
	UINT8 tsnetbTeam;                       // team that ended its turn / authority stamp
	UINT8 tsubNextTeam;                     // team to go next
} turn_struct;

typedef struct                              // network.h sc_struct -- start-combat / wipe
{
	UINT8 ubStartingTeam;
} sc_struct;

typedef struct                              // fresh_header.h death_struct (SoldierID == UINT16)
{
	UINT16 soldier_id;
	UINT16 attacker_id;
	UINT8  attacker_team;                   // 1-based player index into the scoreboard
	UINT8  soldier_team;
} death_struct;

typedef struct                              // connect.h:191 -- scoreboard
{
	int kills;
	int deaths;
	int hits;
	int misses;
} player_stats;

enum { ADMIN_CMD_AUTH = 1, ADMIN_CMD_START = 2 };   // connect.h:69
enum { MP_TYPE_DEATHMATCH, MP_TYPE_TEAMDEATMATCH, MP_TYPE_COOP };

// MP spawn edges -- MUST match the game enum order (i18n/include/Text.h):
// NORTH, EAST, SOUTH, WEST, CENTER. (Earlier these had SOUTH/EAST swapped, so
// client 2 spawned East instead of South.)
#define MP_EDGE_NORTH  0
#define MP_EDGE_EAST   1
#define MP_EDGE_SOUTH  2
#define MP_EDGE_WEST   3
#define MP_EDGE_CENTER 4

#define MPVERSION "MP v3.1"                 // must match the client build (connect.h)

// ============================================================================
//  Coordinator state
// ============================================================================
static RakPeerInterface* g_server = NULL;

struct client_slot { SystemAddress address; int cl_number; };
static client_slot g_clients[4];            // f_rec_num roster; binaryAddress==0 == empty

static char  g_client_names[4][30];
// Which player NAME owns each slot. Unlike g_client_names (cleared on reset), this
// PERSISTS across the game-over rematch reset so a reconnecting player is re-seated
// into their old slot/number instead of being treated as a stranger ("welcome back").
// Cleared only when the server truly empties out, or on a manual reset.
static char  g_knownName[4][30] = { { 0 } };
static int   g_client_teams[4];
static int   g_client_edges[5];
static int   g_client_ready[4];             // server-side ready tracking (no loopback to do it)

static bool          g_allowlaptop = false; // false=lobby/hiring locked, true=hiring unlocked/locked-to-joins
static bool          g_battleStarted = false;
static int           g_connectedCount = 0;
static int           g_numReady = 0;
static int           g_guiLoaded = 0;       // placement: clients that loaded the sector (stage 1)
static int           g_guiPlaced = 0;       // placement: clients done placing mercs (stage 3)

static SystemAddress g_adminAddr;
static bool          g_hasAdmin = false;
static char          g_adminPassword[64] = {0};

static player_stats  g_scoreboard[5];       // local; broadcast on game-over (stat tracking = M2)

// realtime-changeover vote tracking (sendREAL)
static int g_rtVotes = 0;
static bool g_rtTeamVoted[16] = {false};

// turn-based combat state (the coordinator is the turn authority -- the old headless
// host's EndTurn loop). LAN player teams are 6 .. 5+connectedCount. Milestone 2 scope:
// PvP (deathmatch / team-deathmatch); coop's AI turns need a real game host.
static bool  g_inCombat    = false;
static UINT8 g_currentTeam = 0;             // 6..9 while in combat, 0 otherwise
// Server-arbitrated interrupts: at most ONE interrupt may be active at a time, so two
// clients can never both hold control ("both act" is impossible by construction).
static bool  g_interruptActive  = false;
static UINT8 g_preInterruptTeam = 0;        // whose turn to resume when the interrupt releases
static bool  g_gameOver    = false;
static bool  g_teamWiped[16] = { false };   // index by team number (6..9)

// ---- settings (from ja2_mp.ini) --------------------------------------------
static char   g_serverName[30]   = "My JA2 Server";
static char   g_kitBag[100]      = "";
static int    g_serverPort       = 60005;
static int    g_dashboardPort    = 0;       // ja2_mp.ini DASHBOARD_PORT (0 = disabled)
// Log verbosity (ja2_mp.ini LOG_LEVEL, default 0):
//   0 = normal  -- lifecycle + milestones (joins, lobby, combat start, game over)
//   1 = verbose -- + turn/interrupt/sighting/placement detail and client [mp] lines
//   2 = debug   -- + future low-level tracing
enum { LOG_NORMAL = 0, LOG_VERBOSE = 1, LOG_DEBUG = 2 };
static int    g_logLevel = LOG_NORMAL;
#define VLOG(...) do { if (g_logLevel >= LOG_VERBOSE) { printf(__VA_ARGS__); fflush(stdout); } } while (0)
#define DLOG(...) do { if (g_logLevel >= LOG_DEBUG)   { printf(__VA_ARGS__); fflush(stdout); } } while (0)
static int    g_maxClients       = 4;
static int    g_gameType         = MP_TYPE_DEATHMATCH;
static int    g_sameMercAllowed  = 1;
static int    g_maxMercs         = 6;
static float  g_damageMultiplier = 0.7f;
static int    g_secondsPerTick   = 100;
static int    g_startingCash     = 50000;
static float  g_startingTime     = 13.00f;
static int    g_reportHiredMerc  = 1;
static INT16  g_sectorX          = 9;       // A9 (Omerta) -- default MP arena
static INT16  g_sectorY          = 1;

// ============================================================================
//  Roster helpers (mirror server.cpp f_rec_num)
// ============================================================================
static int FindEmptySlot()
{
	for (int x = 0; x < 4; x++)
		if (g_clients[x].address.binaryAddress == 0) return x;
	return -1;
}
static int SlotOf(SystemAddress a)
{
	for (int x = 0; x < 4; x++)
		if (g_clients[x].address.binaryAddress == a.binaryAddress &&
		    g_clients[x].address.port == a.port) return x;
	return -1;
}
static int CountConnected()
{
	int n = 0;
	for (int x = 0; x < 4; x++) if (g_clients[x].address.binaryAddress != 0) n++;
	return n;
}

// ============================================================================
//  Broadcast helpers
// ============================================================================
static inline void RelayExcept(const char* recvName, RPCParameters* p)
{
	// broadcast=true + sender addr -> everyone EXCEPT the sender
	g_server->RPC(recvName, (const char*)p->input, p->numberOfBitsOfData,
	              HIGH_PRIORITY, RELIABLE, 0, p->sender, true, 0, UNASSIGNED_NETWORK_ID, 0);
}
static inline void BroadcastAll(const char* recvName, const char* data, int bytes)
{
	// broadcast=true + UNASSIGNED_SYSTEM_ADDRESS -> everyone
	g_server->RPC(recvName, data, (BitSize_t)(bytes * 8),
	              HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0);
}
static inline void SendTo(const char* recvName, const char* data, int bytes, SystemAddress to)
{
	// broadcast=false + addr -> only that client
	g_server->RPC(recvName, data, (BitSize_t)(bytes * 8),
	              HIGH_PRIORITY, RELIABLE, 0, to, false, 0, UNASSIGNED_NETWORK_ID, 0);
}

// PR-1 (H14): central short-frame guard. Handlers that (Struct*)cast p->input and
// read sizeof(Struct) must first verify the wire payload is at least that large,
// or they over-read the heap buffer (which is sized to the actual payload).
#define NEED(p,T) do{ if ( ((long)(((p)->numberOfBitsOfData)+7)/8) < (long)sizeof(T) ) return; }while(0)

// ============================================================================
//  Pure relay handlers (rebroadcast wire bytes). RPCParameters carries no name,
//  so each RPC needs its own tiny function -- generated by these macros.
// ============================================================================
#define RELAY_EXC(FN, RECV) static void FN(RPCParameters* p) { RelayExcept(RECV, p); }
#define RELAY_ALL(FN, RECV) static void FN(RPCParameters* p) { \
	g_server->RPC(RECV, (const char*)p->input, p->numberOfBitsOfData, \
	HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0); }

RELAY_EXC(sendPATH,             "recievePATH")
RELAY_EXC(sendDOWNLOADSTATUS,   "recieveDOWNLOADSTATUS")
RELAY_EXC(sendSTANCE,           "recieveSTANCE")
RELAY_EXC(sendDIR,              "recieveDIR")
RELAY_EXC(sendFIRE,             "recieveFIRE")
// sendHIT is NOT a pure relay -- it tallies hits for the scoreboard (coordinator fn below)
RELAY_EXC(sendHIRE,             "recieveHIRE")
RELAY_EXC(sendDISMISS,          "recieveDISMISS")
RELAY_EXC(sendguiPOS,           "recieveguiPOS")
RELAY_EXC(sendguiDIR,           "recieveguiDIR")
// sendEndTurn is NOT a pure relay -- it is the turn authority (coordinator fn below)
RELAY_EXC(sendAI,               "recieveAI")
RELAY_EXC(sendSTOP,             "recieveSTOP")
// sendINTERRUPT / endINTERRUPT are relays, but logged so the turn/interrupt
// interleave is visible in the server narration (coordinator fns below).
// sendGUI is NOT a pure relay -- it is the merc-placement barrier (coordinator fn below)
RELAY_EXC(sendBULLET,           "recieveBULLET")
RELAY_EXC(sendGRENADE,          "recieveGRENADE")
RELAY_EXC(sendGRENADERESULT,    "recieveGRENADERESULT")
RELAY_EXC(sendPLANTEXPLOSIVE,   "recievePLANTEXPLOSIVE")
RELAY_EXC(sendDETONATEEXPLOSIVE,"recieveDETONATEEXPLOSIVE")
RELAY_EXC(sendDISARMEXPLOSIVE,  "recieveDISARMEXPLOSIVE")
RELAY_EXC(sendSPREADEFFECT,     "recieveSPREADEFFECT")
RELAY_EXC(sendNEWSMOKEEFFECT,   "recieveNEWSMOKEEFFECT")
RELAY_EXC(sendEXPLOSIONDAMAGE,  "recieveEXPLOSIONDAMAGE")
RELAY_EXC(sendSTATE,            "recieveSTATE")
// sendDEATH / sendhitSTRUCT / sendhitWINDOW / sendMISS are NOT pure relays -- they
// tally kills/deaths/hits/misses for the scoreboard (coordinator fns below)
RELAY_EXC(updatenetworksoldier, "UpdateSoldierFromNetwork")
RELAY_EXC(sendFIREW,            "recieve_fireweapon")
RELAY_EXC(sendDOOR,             "recieve_door")
// sendWIPE is NOT a pure relay -- it drives the deathmatch win check (coordinator fn below)
RELAY_EXC(sendHEAL,             "recieve_heal")
RELAY_EXC(sendEDGECHANGE,       "recieveEDGECHANGE")
RELAY_EXC(sendTEAMCHANGE,       "recieveTEAMCHANGE")

RELAY_ALL(Snull_team,           "null_team")
RELAY_ALL(sendCHATMSG,          "recieveCHATMSG")

// ============================================================================
//  Coordinator handlers
// ============================================================================

// start signals -------------------------------------------------------------
static void BroadcastUnlockLaptop()
{
	if (g_allowlaptop) return;
	g_allowlaptop = true;
	memset(g_client_ready, 0, sizeof(g_client_ready));
	g_numReady = 0;
	ready_struct rs; memset(&rs, 0, sizeof(rs));
	rs.client_num = 1; rs.status = 1; rs.ready_stage = 36;   // 36 == unlock laptop
	BroadcastAll("recieveREADY", (const char*)&rs, sizeof(rs));
	printf("[ja2server] laptops UNLOCKED -- clients may hire mercs\n"); fflush(stdout);
}
static void BroadcastStartBattle()
{
	if (g_battleStarted) return;
	g_battleStarted = true;
	ready_struct rs; memset(&rs, 0, sizeof(rs));
	rs.client_num = 1; rs.status = 1; rs.ready_stage = 1;    // 1 == go-ahead, load sector
	BroadcastAll("recieveREADY", (const char*)&rs, sizeof(rs));
	printf("[ja2server] BATTLE START broadcast -- clients loading sector\n"); fflush(stdout);
}

static void requestFILE_TRANSFER_SETTINGS(RPCParameters* p)
{
	// Milestone 1: no file sync. Tell the client there is nothing to download so
	// it proceeds straight to requestSETTINGS.
	filetransfersettings_struct fts; memset(&fts, 0, sizeof(fts));
	fts.syncClientsDirectory = 0;
	strncpy(fts.serverName, g_serverName, 29);
	fts.totalTransferBytes = 0;
	SendTo("recieveFILE_TRANSFER_SETTINGS", (const char*)&fts, sizeof(fts), p->sender);
}

static void requestSETTINGS(RPCParameters* p)
{
	if (g_allowlaptop) { // can_joingame() == !allowlaptop : reject late joins
		printf("[ja2server] rejecting join -- game already locked\n"); fflush(stdout);
		g_server->CloseConnection(p->sender, true);
		return;
	}
	NEED(p, client_info);   // H14: short-frame over-read guard
	client_info* ci = (client_info*)p->input;
	ci->client_name[29] = 0;
	ci->client_version[29] = 0;

	if (strcmp(ci->client_version, MPVERSION) != 0)
	{
		printf("[ja2server] REJECT '%s': version '%s' != '%s'\n",
		       ci->client_name, ci->client_version, MPVERSION); fflush(stdout);
		g_server->CloseConnection(p->sender, true);
		return;
	}

	int slot = SlotOf(p->sender);                 // already in roster (re-send)?
	bool welcomeBack = false;
	if (slot < 0) {
		// "Welcome back": re-seat a reconnecting player into the slot their NAME held
		// last time, if it's free -- keeps their player number / team / spawn edge
		// stable across a rematch instead of shuffling them.
		for (int i = 0; i < 4; i++)
			if (g_knownName[i][0] && strcmp(g_knownName[i], ci->client_name) == 0
			    && g_clients[i].address.binaryAddress == 0) { slot = i; welcomeBack = true; break; }
	}
	if (slot < 0) slot = FindEmptySlot();
	if (slot < 0) { g_server->CloseConnection(p->sender, true); return; }

	g_clients[slot].address   = p->sender;
	g_clients[slot].cl_number = slot + 1;
	int cl_num = slot + 1;
	strncpy(g_client_names[slot], ci->client_name, 29);
	g_client_teams[slot] = ci->team;
	// remember name->slot, and drop any stale duplicate of this name in another slot
	for (int i = 0; i < 4; i++)
		if (i != slot && strcmp(g_knownName[i], ci->client_name) == 0) g_knownName[i][0] = 0;
	strncpy(g_knownName[slot], ci->client_name, 29); g_knownName[slot][29] = 0;
	g_connectedCount = CountConnected();
	printf("[ja2server] %s '%s' -> player #%d  (%d connected)\n",
	       welcomeBack ? "WELCOME BACK" : "client", ci->client_name, cl_num, g_connectedCount); fflush(stdout);

	// Admin model: with no loopback host, the FIRST client to connect is admin
	// (if no password is configured). With a password, admin is claimed via
	// adminCmd(ADMIN_CMD_AUTH).
	if (!g_hasAdmin && g_adminPassword[0] == 0)
	{
		g_adminAddr = p->sender;
		g_hasAdmin = true;
		unsigned char one = 1;
		SendTo("recieveADMIN", (const char*)&one, 1, p->sender);
		printf("[ja2server] client #%d is now ADMIN (press G to start)\n", cl_num); fflush(stdout);
	}

	// default per-client spawn edges for (team) deathmatch
	g_client_edges[0] = MP_EDGE_NORTH;
	g_client_edges[1] = MP_EDGE_SOUTH;
	g_client_edges[2] = MP_EDGE_EAST;
	g_client_edges[3] = MP_EDGE_WEST;

	settings_struct lan; memset(&lan, 0, sizeof(lan));
	lan.maxClients        = (UINT8)g_maxClients;
	lan.sameMercAllowed   = (UINT8)g_sameMercAllowed;
	lan.damageMultiplier  = g_damageMultiplier;
	lan.gsMercArriveSectorX = g_sectorX;
	lan.gsMercArriveSectorY = g_sectorY;
	lan.gameType          = (UINT8)g_gameType;
	lan.secondsPerTick    = g_secondsPerTick;
	lan.startingCash      = g_startingCash;
	lan.sofGunNut         = 1;
	lan.soubGameStyle     = 0;   // STYLE_REALISTIC
	lan.soubDifficultyLevel = 3;
	lan.sofTurnTimeLimit  = 1;
	lan.sofIronManMode    = 0;
	lan.soubBobbyRayQuality  = 3;
	lan.soubBobbyRayQuantity = 3;
	lan.maxMercs          = (UINT8)g_maxMercs;
	lan.client_num        = (UINT8)cl_num;
	strncpy(lan.client_name, ci->client_name, 29);
	memcpy(lan.client_names, g_client_names, sizeof(char) * 4 * 30);
	memcpy(lan.client_edges, g_client_edges, sizeof(int) * 5);
	memcpy(lan.client_teams, g_client_teams, sizeof(int) * 4);
	strncpy(lan.server_name, g_serverName, 29);
	lan.team              = ci->team;
	memcpy(lan.kitBag, g_kitBag, sizeof(char) * 100);
	lan.reportHiredMerc   = (UINT8)g_reportHiredMerc;
	lan.startingSectorEdge = (UINT8)g_client_edges[cl_num - 1];
	lan.startingTime      = g_startingTime;
	lan.inventoryAttachment = 0;
	lan.randomStartingEdge = 0;
	lan.randomMercs       = 0;
	strncpy(lan.server_version, MPVERSION, 29);
	// shared, deterministic RNG table (all clients receive the SAME table)
	{
		UINT32 s = 0x12345678u;
		for (int i = 0; i < MAX_PREGENERATED_NUMS; i++) { s = s * 1103515245u + 12345u; lan.random_table[i] = s; }
	}

	BroadcastAll("recieveSETTINGS", (const char*)&lan, sizeof(lan));
	VLOG("[ja2server] settings broadcast (sector A?=%d,%d type=%d)\n",
	       g_sectorX, g_sectorY, g_gameType); fflush(stdout);
}

static void sendREADY(RPCParameters* p)
{
	NEED(p, ready_struct);   // H14: short-frame over-read guard
	ready_struct* rs = (ready_struct*)p->input;
	// relay the toggle so the other clients' lobby displays update
	RelayExcept("recieveREADY", p);

	if (rs->ready_stage == 0)
	{
		if (rs->client_num >= 1 && rs->client_num <= 4)
			g_client_ready[rs->client_num - 1] = rs->status ? 1 : 0;
		g_numReady = 0;
		for (int i = 0; i < 4; i++) g_numReady += g_client_ready[i];
		VLOG("[ja2server] ready %d/%d\n", g_numReady, g_connectedCount); fflush(stdout);

		// Auto-barrier: with laptops unlocked and every connected client ready,
		// begin the battle. (The standalone host casts no ready vote of its own,
		// which is what made the old headless host hang at "2/3".)
		if (g_allowlaptop && g_connectedCount > 0 && g_numReady >= g_connectedCount)
		{
			printf("[ja2server] all clients ready -> auto-start\n"); fflush(stdout);
			BroadcastStartBattle();
		}
	}
}

// Merc-placement barrier. After loading the sector each client calls send_loaded()
// -> sendGUI{stage1}, then waits (UI locked, "waiting for clients" box up) for the
// server to confirm everyone loaded. The engine's barrier (numready==cMaxClients &&
// is_server -> emit stage2) used to run on the loopback client; the standalone host
// has none, so the coordinator must aggregate here. stage2 makes each client run
// lockui(1) -> PlaceMercs() + dismiss the dialog. Likewise stage3(done) -> stage4(go).
static void sendGUI(RPCParameters* p)
{
	NEED(p, ready_struct);   // H14: short-frame over-read guard
	ready_struct* info = (ready_struct*)p->input;
	if (info->ready_stage == 1 && info->status)            // a client loaded the sector
	{
		g_guiLoaded++;
		VLOG("[ja2server] placement: %d/%d loaded\n", g_guiLoaded, g_connectedCount); fflush(stdout);
		if (g_connectedCount > 0 && g_guiLoaded >= g_connectedCount)
		{
			ready_struct r; memset(&r, 0, sizeof(r));
			r.client_num = 1; r.ready_stage = 2; r.status = 1;   // unlock placement UI
			BroadcastAll("recieveGUI", (const char*)&r, sizeof(r));
			printf("[ja2server] all loaded -> placement UNLOCKED\n"); fflush(stdout);
		}
	}
	else if (info->ready_stage == 3 && info->status)       // a client finished placing
	{
		g_guiPlaced++;
		VLOG("[ja2server] placement: %d/%d placed\n", g_guiPlaced, g_connectedCount); fflush(stdout);
		if (g_connectedCount > 0 && g_guiPlaced >= g_connectedCount)
		{
			g_guiPlaced = 0;
			ready_struct r; memset(&r, 0, sizeof(r));
			r.client_num = 1; r.ready_stage = 4; r.status = 1;   // kill placement GUI, begin
			BroadcastAll("recieveGUI", (const char*)&r, sizeof(r));
			printf("[ja2server] all placed -> entering tactical\n"); fflush(stdout);
		}
	}
	else if (info->ready_stage == 3 && !info->status)      // a client retracted "done"
	{
		if (g_guiPlaced > 0) g_guiPlaced--;
	}
	else                                                   // stage 2/4 etc: pass through
	{
		RelayExcept("recieveGUI", p);
	}
}

static void adminCmd(RPCParameters* p)
{
	NEED(p, admin_cmd_struct);   // H14: short-frame over-read guard (reads password[63])
	admin_cmd_struct* ac = (admin_cmd_struct*)p->input;
	ac->password[63] = 0;
	if (ac->cmd == ADMIN_CMD_AUTH)
	{
		if (g_adminPassword[0] != 0 && strncmp(ac->password, g_adminPassword, 63) == 0)
		{
			g_adminAddr = p->sender;
			g_hasAdmin = true;
			unsigned char one = 1;
			SendTo("recieveADMIN", (const char*)&one, 1, p->sender);
			printf("[ja2server] client authenticated as ADMIN\n"); fflush(stdout);
		}
		return;
	}
	if (ac->cmd == ADMIN_CMD_START)
	{
		if (g_hasAdmin && p->sender == g_adminAddr)
		{
			if (!g_allowlaptop) { printf("[ja2server] admin START -> unlock laptops\n"); fflush(stdout); BroadcastUnlockLaptop(); }
			else                { printf("[ja2server] admin START -> begin battle\n");   fflush(stdout); BroadcastStartBattle(); }
		}
		else { VLOG("[ja2server] START ignored -- sender is not the admin\n"); fflush(stdout); }
	}
}

static void sendGAMEOVER(RPCParameters* /*p*/)
{
	// broadcast the (server-side) scoreboard to everyone. Stat tracking is M2; for
	// now this carries whatever g_scoreboard holds (zeros until tracking lands).
	BroadcastAll("recieveGAMEOVER", (const char*)g_scoreboard, sizeof(g_scoreboard));
}

static void ResetGameState();   // defined below; used by the game-over rematch reset

// A merc died. death_struct carries the 1-based player indices plainly (no pointer
// deref needed, unlike the hit/miss handlers), so the coordinator can keep the
// kill/death scoreboard the headless host used to own. Then relay the death event.
static void sendDEATH(RPCParameters* p)
{
	NEED(p, death_struct);   // H14: short-frame over-read guard
	death_struct* d = (death_struct*)p->input;
	if (d->soldier_team  >= 1 && d->soldier_team  <= 5) g_scoreboard[d->soldier_team  - 1].deaths++;
	if (d->attacker_team >= 1 && d->attacker_team <= 5) g_scoreboard[d->attacker_team - 1].kills++;
	VLOG("[ja2server] death: player %d killed by player %d\n", d->soldier_team, d->attacker_team); fflush(stdout);
	RelayExcept("recieveDEATH", p);
}

// Hits & misses for the scoreboard. Unlike deaths (where the victim reports and the
// struct carries an explicit attacker_team), a shot is reported by the FIRER's own
// client -- it simulates its own bullet -- so the sender IS the attacker. ja2server
// is stateless and can't deref the wire attacker id into a SOLDIERTYPE the way the
// old in-game host did, so attribute the shot to the sender's scoreboard slot. That
// slot (0-based) is exactly the death path's (attacker_team-1) row, so a player's
// hits and kills accumulate together. A bullet hitting a structure/window/nothing is
// a miss (no enemy hit); accuracy = hits/(hits+misses), as MPScoreScreen computes.
// (Coop AI shots are reported by the host and would mis-attribute, but DM/PvP -- the
// only mode wired up -- always has firer == sender.)
static void TallyShot(RPCParameters* p, bool hit)
{
	int s = SlotOf(p->sender);
	if (s < 0 || s >= 4) return;
	if (hit) g_scoreboard[s].hits++; else g_scoreboard[s].misses++;
	int h = g_scoreboard[s].hits;
	int shots = h + g_scoreboard[s].misses;
	VLOG("[ja2server] player %d %s -- %d/%d hits (%.0f%%)\n",
	     s + 1, hit ? "HIT " : "miss", h, shots, shots ? (100.0 * h / shots) : 0.0);
	fflush(stdout);
}
static void sendHIT(RPCParameters* p)       { TallyShot(p, true);  RelayExcept("recieveHIT",       p); }
static void sendMISS(RPCParameters* p)      { TallyShot(p, false); RelayExcept("recieveMISS",      p); }
static void sendhitSTRUCT(RPCParameters* p) { TallyShot(p, false); RelayExcept("recievehitSTRUCT", p); }
static void sendhitWINDOW(RPCParameters* p) { TallyShot(p, false); RelayExcept("recievehitWINDOW", p); }

// A team was wiped out (its client sent teamwiped() -> sendWIPE{ubStartingTeam}).
// Relay the notice, then -- for deathmatch -- end the game when only one connected
// team is left standing. The win used to be declared by the host game-instance's
// is_server-gated path, which the coordinator now owns.
static void sendWIPE(RPCParameters* p)
{
	NEED(p, sc_struct);   // H14: short-frame over-read guard
	sc_struct* sc = (sc_struct*)p->input;
	UINT8 team = sc->ubStartingTeam;
	if (team >= 6 && team < 16) g_teamWiped[team] = true;
	RelayExcept("recieve_wipe", p);
	printf("[ja2server] team %d WIPED OUT\n", team); fflush(stdout);

	if (g_gameType != MP_TYPE_COOP && g_battleStarted && !g_gameOver)
	{
		int alive = 0, lastTeam = 0;
		for (int i = 0; i < g_connectedCount; i++)
			if (!g_teamWiped[6 + i]) { alive++; lastTeam = 6 + i; }
		if (alive <= 1)
		{
			g_gameOver = true;
			g_inCombat = false;
			printf("[ja2server] >>> GAME OVER -- team %d wins (last standing)\n", lastTeam); fflush(stdout);
			BroadcastAll("recieveGAMEOVER", (const char*)g_scoreboard, sizeof(g_scoreboard));
			// Re-open immediately for the rematch. Score-screen "Continue" makes each
			// client disconnect + auto-reconnect; if one reconnects before the other
			// has left, the old game would still be "active" (allowlaptop) and reject
			// it. Resetting now means both reconnect into a fresh, joinable lobby.
			ResetGameState();
		}
	}
}

static void sendREAL(RPCParameters* p)
{
	// realtime-changeover vote: when every connected client has voted, tell all
	// clients to switch back to realtime. (Approximation of the engine's
	// per-active-team count, which we cannot see without game state.)
	NEED(p, real_struct);   // H14: short-frame over-read guard
	real_struct* rd = (real_struct*)p->input;
	int b = (rd->bteam >= 0 && rd->bteam < 16) ? rd->bteam : 0;
	if (!g_rtTeamVoted[b]) { g_rtTeamVoted[b] = true; g_rtVotes++; }
	if (g_rtVotes >= g_connectedCount && g_connectedCount > 0)
	{
		g_rtVotes = 0;
		memset(g_rtTeamVoted, 0, sizeof(g_rtTeamVoted));
		// combat is over -- back to realtime; allow the next contact to re-open combat
		g_inCombat = false;
		g_currentTeam = 0;
		g_interruptActive = false;
		BroadcastAll("gotoRT", (const char*)p->input, (p->numberOfBitsOfData + 7) / 8);
		VLOG("[ja2server] <<< realtime (combat ended)\n"); fflush(stdout);
	}
}

// ---- turn authority (Milestone 2) -----------------------------------------
// The next LAN player team after `cur`, wrapping within 6 .. 5+connectedCount.
static UINT8 NextActiveTeam(UINT8 cur)
{
	int n = g_connectedCount > 0 ? g_connectedCount : 1;
	int idx = (int)cur - 6;
	if (idx < 0) idx = 0;
	idx = (idx + 1) % n;
	return (UINT8)(6 + idx);
}
// Announce whose turn it is. Stamped tsnetbTeam=6 so every client treats it as the
// authority (client recieveEndTurn gate: is_server || sender_bTeam==6). Sent to ALL,
// including the team that now has the turn; that client maps team==netbTeam -> "my turn".
static void BroadcastTurn(UINT8 team)
{
	g_currentTeam = team;
	g_interruptActive = false;   // turn boundary: no interrupt should outlive a turn handoff
	turn_struct ts;
	ts.tsnetbTeam = 6;
	ts.tsubNextTeam = team;
	BroadcastAll("recieveEndTurn", (const char*)&ts, sizeof(ts));
	VLOG("[ja2server] >>> turn: team %d\n", team); fflush(stdout);
}

// ---- interrupt ARBITER --------------------------------------------------------
// sendINTERRUPT = a client's interrupt REQUEST. Grant it only if none is active,
// then broadcast the grant (recieveINTERRUPT) to EVERYONE -- the requester acts on
// the grant, the others pause. endINTERRUPT = the RELEASE -> resume the paused turn.
// One holder at a time means turn ownership can never diverge.
//
// DESIGN NOTE (so this isn't re-misdiagnosed as a "blind relay" that should compute
// interrupt weights): the server is a deliberate SERIALIZER, not an arbiter of *who*
// wins a duel. JA2's interrupt resolution -- interrupt points from exp/sight/stamina/
// lighting/RNG, producing the out-of-turn order -- is computed CLIENT-SIDE and shipped
// in the packet (INT_STRUCT.gubOutOfTurnOrder/gubOutOfTurnPersons). The coordinator
// holds no tactical state and MUST NOT recompute weights or add a time-window vote:
// that would add latency and duplicate client math it cannot actually perform.
// The single-threaded "grant only if none active" below also means it can never grant
// two interrupts at once (RPCs dispatch sequentially) -- so double-turns are impossible
// here, contrary to a common report.
// KNOWN LIMITATION: two *different* sighting clients interrupting in the same instant ->
// the second request is DROPPED (logged below), not merged into one combined out-of-turn
// order. The correct fix is to QUEUE/MERGE concurrent interrupt sequences here; it needs
// a 2-client repro to validate, so it is intentionally left as a follow-up, not a blind
// server-side weight/window arbiter.
static void sendINTERRUPT(RPCParameters* p)
{
	if (g_interruptActive)
	{
		VLOG("[ja2server]   interrupt request DROPPED -- one already active\n"); fflush(stdout);
		return;
	}
	g_interruptActive  = true;
	g_preInterruptTeam = g_currentTeam;
	VLOG("[ja2server]   >>> INTERRUPT GRANTED (team %d's turn paused)\n", g_currentTeam); fflush(stdout);
	g_server->RPC("recieveINTERRUPT", (const char*)p->input, p->numberOfBitsOfData,
	              HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0);
}
static void endINTERRUPT(RPCParameters* p)
{
	if (!g_interruptActive)
	{
		VLOG("[ja2server]   interrupt release ignored -- none active\n"); fflush(stdout);
		return;
	}
	g_interruptActive = false;
	VLOG("[ja2server]   <<< INTERRUPT RELEASED (resuming team %d)\n", g_preInterruptTeam); fflush(stdout);
	g_server->RPC("resume_turn", (const char*)p->input, p->numberOfBitsOfData,
	              HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0);
}

// First contact: a client switched to turn-based and is asking the authority to run
// the turn order. The team that sighted (ubStartingTeam, 6..9) takes the first turn.
static void startCOMBAT(RPCParameters* p)
{
	NEED(p, sc_struct);   // H14: short-frame over-read guard
	sc_struct* sc = (sc_struct*)p->input;
	if (g_inCombat)                         // first request wins; ignore the rest
	{
		VLOG("[ja2server] startCOMBAT(team %d) ignored -- already in combat\n", sc->ubStartingTeam); fflush(stdout);
		return;
	}
	UINT8 first = (sc->ubStartingTeam >= 6 && sc->ubStartingTeam <= 9) ? sc->ubStartingTeam : 6;
	g_inCombat = true;
	printf("[ja2server] COMBAT START (team %d sighted)\n", first); fflush(stdout);
	BroadcastTurn(first);
}

// A client finished its turn (END TURN button -> send_EndTurn(netbTeam+1)). Only the
// team whose turn it actually is may advance it; hand the turn to the next active team.
static void sendEndTurn(RPCParameters* p)
{
	NEED(p, turn_struct);   // H14: short-frame over-read guard
	turn_struct* ts = (turn_struct*)p->input;
	if (!g_inCombat)                        // an end-turn before combat formally opened
	{
		g_inCombat = true;
		BroadcastTurn(ts->tsnetbTeam >= 6 && ts->tsnetbTeam <= 9 ? ts->tsnetbTeam : 6);
		return;
	}
	if (ts->tsnetbTeam != g_currentTeam)    // stale / not-your-turn: ignore
	{
		VLOG("[ja2server] endturn from team %d IGNORED (current team %d)\n",
		       ts->tsnetbTeam, g_currentTeam); fflush(stdout);
		return;
	}
	VLOG("[ja2server] endturn ACCEPTED from team %d\n", ts->tsnetbTeam); fflush(stdout);
	BroadcastTurn(NextActiveTeam(g_currentTeam));
}

// Return the coordinator to a fresh, joinable lobby. Called when the last player
// leaves -- otherwise g_allowlaptop stays true and the server rejects all new joins
// ("busy") until the process is restarted.
static void ResetGameState()
{
	g_allowlaptop  = false;
	g_battleStarted = false;
	g_inCombat     = false;
	g_currentTeam  = 0;
	g_interruptActive = false;
	g_gameOver     = false;
	memset(g_teamWiped, 0, sizeof(g_teamWiped));
	memset(g_scoreboard, 0, sizeof(g_scoreboard));
	g_numReady     = 0;
	g_guiLoaded    = 0;
	g_guiPlaced    = 0;
	g_rtVotes      = 0;
	memset(g_rtTeamVoted, 0, sizeof(g_rtTeamVoted));
	memset(g_client_ready, 0, sizeof(g_client_ready));
	g_hasAdmin     = false;
	g_adminAddr    = SystemAddress();
	for (int i = 0; i < 4; i++)
	{
		g_clients[i].address = SystemAddress();
		g_clients[i].cl_number = 0;
		g_client_names[i][0] = 0;
	}
	g_connectedCount = 0;
	printf("[ja2server] ---- game reset, lobby open for new players ----\n"); fflush(stdout);
}

static void HandleDisconnect(SystemAddress who)
{
	int slot = SlotOf(who);
	if (slot < 0) return;
	int cl_num = g_clients[slot].cl_number;
	BroadcastAll("recieveDISCONNECT", (const char*)&cl_num, sizeof(int));
	g_clients[slot].address.binaryAddress = 0;
	g_clients[slot].address.port = 0;
	g_clients[slot].cl_number = 0;
	g_client_names[slot][0] = 0;
	g_client_ready[slot] = 0;
	g_connectedCount = CountConnected();
	g_numReady = 0; for (int i = 0; i < 4; i++) g_numReady += g_client_ready[i];
	printf("[ja2server] client #%d dropped (%d connected)\n", cl_num, g_connectedCount); fflush(stdout);
	if (g_hasAdmin && who == g_adminAddr)
	{
		g_hasAdmin = false;
		g_adminAddr = SystemAddress();   // binaryAddress 0
		printf("[ja2server] admin dropped -- admin slot released\n"); fflush(stdout);
	}
	// last one out resets the server back to a joinable lobby AND forgets the known
	// players -- a truly empty server starts a fresh session (the game-over rematch
	// reset, by contrast, keeps g_knownName so reconnects are recognized).
	if (g_connectedCount == 0)
	{
		ResetGameState();
		memset(g_knownName, 0, sizeof(g_knownName));
	}
}

// Diagnostic log channel. A client sends a preformatted text line (zero-padded,
// NUL-terminated); the coordinator prints it centrally when verbose logging is on.
// Lets the clients narrate things only THEY know (who sighted whom, ranges, etc.).
static void serverLog(RPCParameters* p)
{
	if (g_logLevel < LOG_VERBOSE) return;
	char line[260];
	size_t n = (p->numberOfBitsOfData + 7) / 8;
	if (n >= sizeof(line)) n = sizeof(line) - 1;
	memcpy(line, p->input, n);
	line[n] = 0;
	line[sizeof(line) - 1] = 0;
	printf("[mp] %s\n", line); fflush(stdout);
}

// ============================================================================
//  Registration + pump
// ============================================================================
static void RegisterHandlers()
{
	REGISTER_STATIC_RPC(g_server, sendPATH);
	REGISTER_STATIC_RPC(g_server, sendDOWNLOADSTATUS);
	REGISTER_STATIC_RPC(g_server, sendSTANCE);
	REGISTER_STATIC_RPC(g_server, sendDIR);
	REGISTER_STATIC_RPC(g_server, sendFIRE);
	REGISTER_STATIC_RPC(g_server, sendHIT);
	REGISTER_STATIC_RPC(g_server, sendHIRE);
	REGISTER_STATIC_RPC(g_server, sendDISMISS);
	REGISTER_STATIC_RPC(g_server, sendguiPOS);
	REGISTER_STATIC_RPC(g_server, sendguiDIR);
	REGISTER_STATIC_RPC(g_server, sendEndTurn);
	REGISTER_STATIC_RPC(g_server, sendAI);
	REGISTER_STATIC_RPC(g_server, sendSTOP);
	REGISTER_STATIC_RPC(g_server, sendINTERRUPT);
	REGISTER_STATIC_RPC(g_server, sendREADY);
	REGISTER_STATIC_RPC(g_server, sendGUI);
	REGISTER_STATIC_RPC(g_server, sendBULLET);
	REGISTER_STATIC_RPC(g_server, sendGRENADE);
	REGISTER_STATIC_RPC(g_server, sendGRENADERESULT);
	REGISTER_STATIC_RPC(g_server, sendPLANTEXPLOSIVE);
	REGISTER_STATIC_RPC(g_server, sendDETONATEEXPLOSIVE);
	REGISTER_STATIC_RPC(g_server, sendDISARMEXPLOSIVE);
	REGISTER_STATIC_RPC(g_server, sendSPREADEFFECT);
	REGISTER_STATIC_RPC(g_server, sendNEWSMOKEEFFECT);
	REGISTER_STATIC_RPC(g_server, sendEXPLOSIONDAMAGE);
	REGISTER_STATIC_RPC(g_server, requestSETTINGS);
	REGISTER_STATIC_RPC(g_server, requestFILE_TRANSFER_SETTINGS);
	REGISTER_STATIC_RPC(g_server, sendSTATE);
	REGISTER_STATIC_RPC(g_server, sendDEATH);
	REGISTER_STATIC_RPC(g_server, sendhitSTRUCT);
	REGISTER_STATIC_RPC(g_server, sendhitWINDOW);
	REGISTER_STATIC_RPC(g_server, sendMISS);
	REGISTER_STATIC_RPC(g_server, updatenetworksoldier);
	REGISTER_STATIC_RPC(g_server, Snull_team);
	REGISTER_STATIC_RPC(g_server, sendFIREW);
	REGISTER_STATIC_RPC(g_server, sendDOOR);
	REGISTER_STATIC_RPC(g_server, endINTERRUPT);
	REGISTER_STATIC_RPC(g_server, adminCmd);
	REGISTER_STATIC_RPC(g_server, sendREAL);
	REGISTER_STATIC_RPC(g_server, startCOMBAT);
	REGISTER_STATIC_RPC(g_server, sendWIPE);
	REGISTER_STATIC_RPC(g_server, sendHEAL);
	REGISTER_STATIC_RPC(g_server, sendEDGECHANGE);
	REGISTER_STATIC_RPC(g_server, sendTEAMCHANGE);
	REGISTER_STATIC_RPC(g_server, sendGAMEOVER);
	REGISTER_STATIC_RPC(g_server, sendCHATMSG);
	REGISTER_STATIC_RPC(g_server, serverLog);
}

static unsigned char PacketId(Packet* p)
{
	if (!p) return 255;
	if ((unsigned char)p->data[0] == ID_TIMESTAMP)
		return (unsigned char)p->data[sizeof(unsigned char) + sizeof(RakNetTime)];
	return (unsigned char)p->data[0];
}

static volatile sig_atomic_t g_run = 1;
static void OnSignal(int) { g_run = 0; }
// SIGHUP forces a fresh lobby (kick everyone + reset) without restarting the process
// -- handy when a game wedges mid-combat: `kill -HUP <pid>`.
static volatile sig_atomic_t g_reset = 0;
static void OnReset(int) { g_reset = 1; }

// ============================================================================
//  ja2_mp.ini reader (tolerant: KEY = VALUE, ; comments, [sections])
// ============================================================================
static std::map<std::string, std::string> g_ini;
static void TrimInPlace(std::string& s)
{
	size_t a = s.find_first_not_of(" \t\r\n");
	size_t b = s.find_last_not_of(" \t\r\n");
	if (a == std::string::npos) { s.clear(); return; }
	s = s.substr(a, b - a + 1);
}
static bool LoadIni(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) return false;
	char line[1024];
	while (fgets(line, sizeof(line), f))
	{
		std::string s(line);
		size_t c = s.find(';'); if (c != std::string::npos) s = s.substr(0, c);
		size_t eq = s.find('=');
		if (eq == std::string::npos) continue;
		std::string k = s.substr(0, eq), v = s.substr(eq + 1);
		TrimInPlace(k); TrimInPlace(v);
		if (!k.empty()) g_ini[k] = v;
	}
	fclose(f);
	return true;
}
static int IniInt(const char* key, int def)
{
	auto it = g_ini.find(key);
	return it == g_ini.end() || it->second.empty() ? def : atoi(it->second.c_str());
}
static const char* IniStr(const char* key, const char* def)
{
	auto it = g_ini.find(key);
	return it == g_ini.end() || it->second.empty() ? def : it->second.c_str();
}

static void ApplyIni()
{
	strncpy(g_serverName, IniStr("SERVER_NAME", "My JA2 Server"), 29);
	strncpy(g_kitBag,     IniStr("KIT_BAG", ""), 99);
	strncpy(g_adminPassword, IniStr("ADMIN_PASSWORD", ""), 63);
	g_serverPort      = IniInt("SERVER_PORT", 60005);
	g_dashboardPort   = IniInt("DASHBOARD_PORT", 0);   // opt-in web panel
	g_logLevel        = IniInt("LOG_LEVEL", 0);          // 0 normal / 1 verbose / 2 debug
	g_maxClients      = IniInt("MAX_PLAYERS", 4);
	g_gameType        = IniInt("GAME_TYPE", 0);
	g_sameMercAllowed = IniInt("SAME_MERC_ALLOWED", 1);
	g_maxMercs        = IniInt("MAX_MERCS", 6);
	g_reportHiredMerc = IniInt("REPORT_HIRED_MERC_NAME", 1);
	switch (IniInt("WEAPON_DAMAGE", 1)) { case 0: g_damageMultiplier=0.2f; break; case 2: g_damageMultiplier=1.0f; break; default: g_damageMultiplier=0.7f; }
	switch (IniInt("TIMED_TURNS", 2))   { case 0: g_secondsPerTick=0; break; case 1: g_secondsPerTick=5; break; case 3: g_secondsPerTick=400; break; default: g_secondsPerTick=100; }
	switch (IniInt("STARTING_CASH", 1)) { case 0: g_startingCash=5000; break; case 2: g_startingCash=100000; break; case 3: g_startingCash=999999999; break; default: g_startingCash=50000; }
	switch (IniInt("STARTING_TIME", 1)) { case 0: g_startingTime=7.00f; break; case 2: g_startingTime=2.00f; break; default: g_startingTime=13.00f; }
	if (g_maxClients < 1) g_maxClients = 1;
	if (g_maxClients > 4) g_maxClients = 4;
}

// ============================================================================
//  Embedded web dashboard (optional). Tiny non-blocking HTTP/1.1 server on a
//  side port -- ja2_mp.ini DASHBOARD_PORT (0 = disabled). Live status + match
//  configuration. Reuses the SDL3_net already linked for the game protocol.
//  UNAUTHENTICATED -> trusted LAN only. One client served at a time; bounded
//  per-tick work so it never stalls the game pump.
// ============================================================================
static NET_Server* g_httpListener = NULL;
static const size_t   HTTP_MAX_REQUEST = 64 * 1024;
static const unsigned HTTP_MAX_TICKS   = 300;     // ~3s at 10ms/tick before we drop a slow client

struct HttpClient { NET_StreamSocket* sock; std::vector<unsigned char> acc; unsigned ticks; };
static HttpClient* g_httpClient = NULL;

struct HttpReq { std::string method, path, query, body; };

static std::string JsonEsc(const char* s)
{
	std::string o;
	for (; s && *s; ++s) {
		unsigned char c = (unsigned char)*s;
		if (c == '"' || c == '\\') { o.push_back('\\'); o.push_back((char)c); }
		else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
		else o.push_back((char)c);
	}
	return o;
}
static std::string UrlDecode(const std::string& s)
{
	std::string o;
	for (size_t i = 0; i < s.size(); ++i) {
		char c = s[i];
		if (c == '+') o.push_back(' ');
		else if (c == '%' && i + 2 < s.size()) {
			auto hex = [](char h)->int { if (h>='0'&&h<='9') return h-'0'; if (h>='a'&&h<='f') return h-'a'+10; if (h>='A'&&h<='F') return h-'A'+10; return 0; };
			o.push_back((char)(hex(s[i+1]) * 16 + hex(s[i+2]))); i += 2;
		} else o.push_back(c);
	}
	return o;
}
static std::string HttpResponse(int code, const char* reason, const char* ctype, const std::string& body)
{
	char hdr[256];
	snprintf(hdr, sizeof(hdr),
		"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
		"Connection: close\r\nCache-Control: no-store\r\n\r\n",
		code, reason, ctype, body.size());
	return std::string(hdr) + body;
}

static const char* PhaseStr()
{
	if (g_inCombat)      return "combat";
	if (g_battleStarted) return "placement";
	if (g_allowlaptop)   return "hiring";
	return "lobby";
}
static std::string StatusJson()
{
	std::string o = "{";
	char b[512];
	snprintf(b, sizeof(b),
		"\"server\":\"%s\",\"port\":%d,\"dashboardPort\":%d,\"phase\":\"%s\","
		"\"connected\":%d,\"maxClients\":%d,\"numReady\":%d,\"inCombat\":%s,"
		"\"currentTeam\":%d,\"hasAdmin\":%s,\"guiLoaded\":%d,\"guiPlaced\":%d,",
		JsonEsc(g_serverName).c_str(), g_serverPort, g_dashboardPort, PhaseStr(),
		g_connectedCount, g_maxClients, g_numReady, g_inCombat ? "true" : "false",
		g_currentTeam, g_hasAdmin ? "true" : "false", g_guiLoaded, g_guiPlaced);
	o += b;
	o += "\"players\":[";
	bool first = true;
	for (int i = 0; i < 4; i++) {
		if (!g_clients[i].address.binaryAddress) continue;
		bool isAdmin = g_hasAdmin && g_clients[i].address == g_adminAddr;
		snprintf(b, sizeof(b),
			"%s{\"num\":%d,\"name\":\"%s\",\"team\":%d,\"ready\":%s,\"admin\":%s}",
			first ? "" : ",", g_clients[i].cl_number, JsonEsc(g_client_names[i]).c_str(),
			g_client_teams[i], g_client_ready[i] ? "true" : "false", isAdmin ? "true" : "false");
		o += b; first = false;
	}
	o += "],\"config\":{";
	snprintf(b, sizeof(b),
		"\"serverName\":\"%s\",\"maxClients\":%d,\"gameType\":%d,\"sectorX\":%d,"
		"\"sectorY\":%d,\"damage\":%.2f,\"cash\":%d,\"time\":%.2f,\"maxMercs\":%d,"
		"\"sameMerc\":%d,\"logLevel\":%d,\"adminPasswordSet\":%s}",
		JsonEsc(g_serverName).c_str(), g_maxClients, g_gameType, g_sectorX, g_sectorY,
		g_damageMultiplier, g_startingCash, g_startingTime, g_maxMercs,
		g_sameMercAllowed, g_logLevel, g_adminPassword[0] ? "true" : "false");
	o += b; o += "}";   // config snprintf already closes its object; this closes the root
	return o;
}

// Apply a single config field (validated + clamped). Takes effect on the NEXT game.
static void ApplyConfigKV(const std::string& k, const std::string& v)
{
	if      (k == "serverName")  { strncpy(g_serverName, v.c_str(), 29); g_serverName[29] = 0; }
	else if (k == "adminPassword"){ strncpy(g_adminPassword, v.c_str(), 63); g_adminPassword[63] = 0; }
	else if (k == "maxClients")  { int x = atoi(v.c_str()); g_maxClients = x < 1 ? 1 : (x > 4 ? 4 : x); }
	else if (k == "gameType")    { int x = atoi(v.c_str()); g_gameType = x < 0 ? 0 : (x > 2 ? 2 : x); }
	else if (k == "sectorX")     { int x = atoi(v.c_str()); g_sectorX = (INT16)(x < 1 ? 1 : (x > 16 ? 16 : x)); }
	else if (k == "sectorY")     { int x = atoi(v.c_str()); g_sectorY = (INT16)(x < 1 ? 1 : (x > 16 ? 16 : x)); }
	else if (k == "damage")      { float x = (float)atof(v.c_str()); g_damageMultiplier = x < 0.1f ? 0.1f : (x > 2.0f ? 2.0f : x); }
	else if (k == "cash")        { long x = atol(v.c_str()); g_startingCash = (int)(x < 0 ? 0 : (x > 999999999L ? 999999999L : x)); }
	else if (k == "time")        { float x = (float)atof(v.c_str()); g_startingTime = x < 0.0f ? 0.0f : (x > 23.99f ? 23.99f : x); }
	else if (k == "maxMercs")    { int x = atoi(v.c_str()); g_maxMercs = x < 1 ? 1 : (x > 18 ? 18 : x); }
	else if (k == "sameMerc")    { g_sameMercAllowed = atoi(v.c_str()) ? 1 : 0; }
	else if (k == "logLevel")    { int x = atoi(v.c_str()); g_logLevel = x < 0 ? 0 : (x > 2 ? 2 : x); }  // applies immediately
}
static int ApplyConfigBody(const std::string& body)
{
	int n = 0;
	size_t i = 0;
	while (i < body.size()) {
		size_t amp = body.find('&', i);
		std::string pair = body.substr(i, amp == std::string::npos ? std::string::npos : amp - i);
		size_t eq = pair.find('=');
		if (eq != std::string::npos) { ApplyConfigKV(UrlDecode(pair.substr(0, eq)), UrlDecode(pair.substr(eq + 1))); n++; }
		if (amp == std::string::npos) break;
		i = amp + 1;
	}
	if (n) { printf("[ja2server] dashboard: config updated (%d fields, applies next game)\n", n); fflush(stdout); }
	return n;
}

static const char* DASH_HTML = R"DASH(<!doctype html><html lang=en><head>
<meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>ja2server</title><style>
:root{--bg:#15171c;--panel:#1e2128;--line:#2c313c;--txt:#d7dbe2;--mut:#8b93a3;--acc:#6fae54;--warn:#caa23b}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--txt);font:14px/1.5 system-ui,Segoe UI,sans-serif}
header{padding:16px 22px;border-bottom:1px solid var(--line);display:flex;align-items:center;gap:14px}
h1{font-size:18px;margin:0;font-weight:600}.badge{font-size:12px;padding:3px 10px;border-radius:20px;background:var(--panel);border:1px solid var(--line);text-transform:uppercase;letter-spacing:.5px}
.badge.combat{color:#e2705a;border-color:#5a2e28}.badge.hiring,.badge.placement{color:var(--warn)}.badge.lobby{color:var(--mut)}
main{max-width:760px;margin:0 auto;padding:22px;display:grid;gap:18px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:16px 18px}
.card h2{margin:0 0 12px;font-size:13px;text-transform:uppercase;letter-spacing:.6px;color:var(--mut)}
table{width:100%;border-collapse:collapse}th,td{text-align:left;padding:7px 8px;border-bottom:1px solid var(--line)}
th{color:var(--mut);font-weight:500;font-size:12px}tr:last-child td{border-bottom:none}
.tag{font-size:11px;padding:2px 7px;border-radius:5px;background:#272b34}.tag.rdy{color:var(--acc)}.tag.adm{color:var(--warn)}
.stat{display:flex;gap:26px;flex-wrap:wrap}.stat div{display:flex;flex-direction:column}.stat b{font-size:18px;font-weight:600}.stat span{color:var(--mut);font-size:12px}
form.cfg{display:grid;grid-template-columns:1fr 1fr;gap:12px 18px}label{display:flex;flex-direction:column;gap:4px;font-size:12px;color:var(--mut)}
input,select{background:#13151a;border:1px solid var(--line);color:var(--txt);border-radius:6px;padding:7px 9px;font:inherit}
.row{grid-column:1/-1;display:flex;gap:10px;align-items:center;margin-top:4px}
button{background:var(--acc);color:#0c130a;border:0;border-radius:6px;padding:9px 16px;font:inherit;font-weight:600;cursor:pointer}
button.sec{background:#2c313c;color:var(--txt)}button.danger{background:#7a3a32;color:#ffd9d2}
.msg{font-size:12px;color:var(--acc)}.empty{color:var(--mut);font-style:italic}.note{color:var(--mut);font-size:11px;margin-top:8px}
</style></head><body>
<header><h1 id=sv>ja2server</h1><span class=badge id=ph>...</span><span style=margin-left:auto;color:var(--mut);font-size:12px id=meta></span></header>
<main>
<div class=card><h2>Status</h2><div class=stat>
<div><b id=s_players>0/0</b><span>players</span></div>
<div><b id=s_ready>0</b><span>ready</span></div>
<div><b id=s_turn>-</b><span>current turn</span></div>
<div><b id=s_phase>lobby</b><span>phase</span></div></div></div>
<div class=card><h2>Players</h2><table><thead><tr><th>#</th><th>Name</th><th>Team</th><th></th></tr></thead><tbody id=plist></tbody></table></div>
<div class=card><h2>Configuration <span class=note>(applies to the next game)</span></h2>
<form class=cfg id=cfg>
<label>Server name<input name=serverName maxlength=29></label>
<label>Admin password <span class=note id=pwset></span><input name=adminPassword type=password placeholder="(unchanged)"></label>
<label>Max players<input name=maxClients type=number min=1 max=4></label>
<label>Game type<select name=gameType><option value=0>Deathmatch</option><option value=1>Team Deathmatch</option><option value=2>Co-op</option></select></label>
<label>Arena sector X (1-16)<input name=sectorX type=number min=1 max=16></label>
<label>Arena sector Y (1-16)<input name=sectorY type=number min=1 max=16></label>
<label>Damage multiplier<input name=damage type=number step=0.1 min=0.1 max=2></label>
<label>Starting cash<input name=cash type=number min=0 max=999999999></label>
<label>Starting time (0-23.99)<input name=time type=number step=0.5 min=0 max=23.99></label>
<label>Max mercs / player<input name=maxMercs type=number min=1 max=18></label>
<label>Same merc allowed<select name=sameMerc><option value=1>Yes</option><option value=0>No</option></select></label>
<label>Log level (live)<select name=logLevel><option value=0>Normal</option><option value=1>Verbose</option><option value=2>Debug</option></select></label>
<div class=row><button type=submit>Save settings</button><span class=msg id=saved></span></div>
</form></div>
<div class=card><h2>Actions</h2><div class=row>
<button class=sec id=reset>Reset lobby</button>
<button class=danger id=kick>Reset &amp; kick all</button>
<span class=msg id=acted></span></div>
<div class=note>Reset returns the server to a fresh, joinable lobby. Mid-game rejoin of one player is not supported.</div></div>
</main>
<script>
let filled=false;
const $=id=>document.getElementById(id);
function team(t){return t>=6?('Player '+(t-5)):(t||'-')}
async function refresh(){
 try{const j=await(await fetch('/api/status')).json();
 $('sv').textContent=j.server;$('ph').textContent=j.phase;$('ph').className='badge '+j.phase;
 $('meta').textContent='game port '+j.port+' · dashboard '+j.dashboardPort;
 $('s_players').textContent=j.connected+'/'+j.maxClients;$('s_ready').textContent=j.numReady;
 $('s_phase').textContent=j.phase;$('s_turn').textContent=j.inCombat?team(j.currentTeam):'-';
 const tb=$('plist');tb.innerHTML='';
 if(!j.players.length){tb.innerHTML='<tr><td colspan=4 class=empty>no players connected</td></tr>'}
 j.players.forEach(p=>{const tags=(p.ready?'<span class="tag rdy">ready</span> ':'')+(p.admin?'<span class="tag adm">admin</span>':'');
  tb.insertAdjacentHTML('beforeend','<tr><td>'+p.num+'</td><td>'+p.name+'</td><td>'+team(p.team)+'</td><td>'+tags+'</td></tr>')});
 if(!filled){const c=j.config,f=$('cfg');
  f.serverName.value=c.serverName;f.maxClients.value=c.maxClients;f.gameType.value=c.gameType;
  f.sectorX.value=c.sectorX;f.sectorY.value=c.sectorY;f.damage.value=c.damage;f.cash.value=c.cash;
  f.time.value=c.time;f.maxMercs.value=c.maxMercs;f.sameMerc.value=c.sameMerc;f.logLevel.value=c.logLevel;
  $('pwset').textContent=c.adminPasswordSet?'(set)':'(open: first joiner is admin)';filled=true}
 }catch(e){$('ph').textContent='offline';$('ph').className='badge'}
}
$('cfg').addEventListener('submit',async e=>{e.preventDefault();
 const fd=new FormData(e.target);if(!fd.get('adminPassword'))fd.delete('adminPassword');
 await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(fd)});
 $('saved').textContent='saved (next game)';setTimeout(()=>$('saved').textContent='',2500)});
$('reset').addEventListener('click',async()=>{await fetch('/api/reset',{method:'POST'});$('acted').textContent='lobby reset';setTimeout(()=>$('acted').textContent='',2500)});
$('kick').addEventListener('click',async()=>{if(!confirm('Kick all players and reset?'))return;await fetch('/api/reset?kick=1',{method:'POST'});$('acted').textContent='kicked + reset';setTimeout(()=>$('acted').textContent='',2500)});
refresh();setInterval(refresh,1500);
</script></body></html>)DASH";

static std::string HttpHandle(const HttpReq& r)
{
	if (r.method == "GET" && r.path == "/")            return HttpResponse(200, "OK", "text/html; charset=utf-8", DASH_HTML);
	if (r.method == "GET" && r.path == "/api/status")  return HttpResponse(200, "OK", "application/json", StatusJson());
	if (r.method == "POST" && r.path == "/api/config") {
		int applied = ApplyConfigBody(r.body);
		char b[64]; snprintf(b, sizeof(b), "{\"ok\":true,\"applied\":%d}", applied);
		return HttpResponse(200, "OK", "application/json", b);
	}
	if (r.method == "POST" && r.path == "/api/reset") {
		if (r.query.find("kick=1") != std::string::npos)
			for (int i = 0; i < 4; i++)
				if (g_clients[i].address.binaryAddress) g_server->CloseConnection(g_clients[i].address, true);
		ResetGameState();
		memset(g_knownName, 0, sizeof(g_knownName));   // manual reset = fresh session
		return HttpResponse(200, "OK", "application/json", "{\"ok\":true}");
	}
	return HttpResponse(404, "Not Found", "text/plain", "not found");
}

static bool HttpComplete(const std::vector<unsigned char>& a, size_t* sep, size_t* clen)
{
	static const unsigned char N[4] = { '\r','\n','\r','\n' };
	if (a.size() < 4) return false;
	size_t i = std::string::npos;
	for (size_t k = 0; k + 4 <= a.size(); ++k) if (!memcmp(a.data() + k, N, 4)) { i = k; break; }
	if (i == std::string::npos) return false;
	*sep = i; *clen = 0;
	std::string h((const char*)a.data(), i);
	for (char& c : h) c = (char)tolower((unsigned char)c);
	size_t cl = h.find("content-length:");
	if (cl != std::string::npos) {
		*clen = (size_t)strtoul(h.c_str() + cl + 15, NULL, 10);
		if (*clen > HTTP_MAX_REQUEST) return true;   // oversized -> let handler bail
	}
	return a.size() >= i + 4 + *clen;
}
static bool HttpParse(const std::vector<unsigned char>& a, size_t sep, size_t clen, HttpReq& r)
{
	std::string head((const char*)a.data(), sep);
	size_t eol = head.find("\r\n");
	std::string line = (eol == std::string::npos) ? head : head.substr(0, eol);
	size_t s1 = line.find(' '); if (s1 == std::string::npos) return false;
	size_t s2 = line.find(' ', s1 + 1); if (s2 == std::string::npos) return false;
	r.method = line.substr(0, s1);
	std::string target = line.substr(s1 + 1, s2 - s1 - 1);
	if (r.method.size() > 8 || target.size() > 1024) return false;
	size_t q = target.find('?');
	r.path  = (q == std::string::npos) ? target : target.substr(0, q);
	r.query = (q == std::string::npos) ? std::string() : target.substr(q + 1);
	if (clen) r.body.assign((const char*)a.data() + sep + 4, clen);
	return true;
}

static void HttpCloseClient()
{
	if (!g_httpClient) return;
	if (g_httpClient->sock) NET_DestroyStreamSocket(g_httpClient->sock);
	delete g_httpClient; g_httpClient = NULL;
}
static void HttpInit()
{
	if (g_dashboardPort <= 0) return;
	g_httpListener = NET_CreateServer(NULL, (Uint16)g_dashboardPort, 0);
	if (!g_httpListener) { printf("[ja2server] dashboard: FAILED to bind port %d (%s)\n", g_dashboardPort, SDL_GetError()); return; }
	printf("[ja2server] dashboard: http://<host>:%d  (UNAUTHENTICATED -- trusted LAN only)\n", g_dashboardPort);
}
static void HttpTick()
{
	if (!g_httpListener) return;
	if (!g_httpClient) {
		NET_StreamSocket* s = NULL;
		if (!NET_AcceptClient(g_httpListener, &s) || !s) return;   // error or nothing pending
		g_httpClient = new HttpClient(); g_httpClient->sock = s; g_httpClient->ticks = 0;
	}
	unsigned char tmp[4096];
	int n = NET_ReadFromStreamSocket(g_httpClient->sock, tmp, sizeof(tmp));
	if (n < 0) { HttpCloseClient(); return; }                      // peer gone
	if (n > 0) {
		if (g_httpClient->acc.size() + (size_t)n > HTTP_MAX_REQUEST) { HttpCloseClient(); return; }
		g_httpClient->acc.insert(g_httpClient->acc.end(), tmp, tmp + n);
	}
	if (++g_httpClient->ticks > HTTP_MAX_TICKS) { HttpCloseClient(); return; }
	size_t sep, clen;
	if (!HttpComplete(g_httpClient->acc, &sep, &clen)) return;     // keep reading next tick
	HttpReq req;
	std::string resp = HttpParse(g_httpClient->acc, sep, clen, req)
		? HttpHandle(req) : HttpResponse(400, "Bad Request", "text/plain", "bad request");
	NET_WriteToStreamSocket(g_httpClient->sock, resp.data(), (int)resp.size());
	NET_WaitUntilStreamSocketDrained(g_httpClient->sock, 250);     // flush small response (local/LAN: ~instant)
	HttpCloseClient();
}
static void HttpShutdown()
{
	HttpCloseClient();
	if (g_httpListener) { NET_DestroyServer(g_httpListener); g_httpListener = NULL; }
}

int main(int argc, char** argv)
{
	// Make server logs appear promptly when piped to a file/terminal.
	// NB: the MSVC runtime rejects _IOLBF with a 0-size buffer as an invalid
	// parameter -> its invalid-parameter handler aborts the process before main()
	// does anything (a SILENT startup crash on Windows -- which is why the MSVC
	// ja2server.exe never started). _IONBF (unbuffered) is valid on every CRT and
	// is, if anything, promter; use it on Windows.
#ifdef _WIN32
	setvbuf(stdout, NULL, _IONBF, 0);
#else
	setvbuf(stdout, NULL, _IOLBF, 0);
#endif
	const char* iniPath = "ja2_mp.ini";
	int portOverride = 0;
	int dashOverride = -1;
	int logOverride  = -1;
	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--ini") && i + 1 < argc) iniPath = argv[++i];
		else if (!strcmp(argv[i], "--port") && i + 1 < argc) portOverride = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--dashboard") && i + 1 < argc) dashOverride = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--loglevel") && i + 1 < argc) logOverride = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--verbose")) logOverride = LOG_VERBOSE;
		else if (!strcmp(argv[i], "--debug"))   logOverride = LOG_DEBUG;
		else if (!strcmp(argv[i], "--help")) {
			printf("ja2server -- standalone JA2 1.13 MP coordinator\n"
			       "  --ini <path>     ja2_mp.ini to read (default ./ja2_mp.ini)\n"
			       "  --port <n>       override SERVER_PORT\n"
			       "  --dashboard <n>  enable the web dashboard on port n (0 = off)\n"
			       "  --loglevel <n>   0 normal / 1 verbose / 2 debug (overrides LOG_LEVEL)\n"
			       "  --verbose        shorthand for --loglevel 1\n"
			       "  --debug          shorthand for --loglevel 2\n");
			return 0;
		}
	}

	if (LoadIni(iniPath)) { printf("[ja2server] loaded %s\n", iniPath); ApplyIni(); }
	else printf("[ja2server] no %s found -- using defaults\n", iniPath);
	if (portOverride) g_serverPort = portOverride;
	if (dashOverride >= 0) g_dashboardPort = dashOverride;
	if (logOverride >= 0) g_logLevel = logOverride > LOG_DEBUG ? LOG_DEBUG : logOverride;

	signal(SIGINT, OnSignal);
	signal(SIGTERM, OnSignal);
#ifdef SIGHUP
	signal(SIGHUP, OnReset);   // POSIX only: `kill -HUP` resets the lobby for a rematch
#else
	(void)&OnReset;            // Windows has no SIGHUP; keep the handler referenced
#endif

	SDL_Init(0);   // netshim's NET_Init (refcounted in Startup) needs SDL up

	for (int x = 0; x < 4; x++) { g_clients[x].address = SystemAddress(); g_clients[x].cl_number = 0; }

	g_server = RakNetworkFactory::GetRakPeerInterface();
	g_server->SetTimeoutTime(120000, UNASSIGNED_SYSTEM_ADDRESS);
	SocketDescriptor sd((unsigned short)g_serverPort, 0);
	if (!g_server->Startup((unsigned short)g_maxClients, 30, &sd, 1))
	{
		fprintf(stderr, "[ja2server] FATAL: could not bind port %d\n", g_serverPort);
		return 1;
	}
	g_server->SetMaximumIncomingConnections((unsigned short)g_maxClients);
	g_server->SetOccasionalPing(true);
	RegisterHandlers();

	printf("========================================================\n");
	printf(" ja2server  '%s'\n", g_serverName);
	printf(" listening on port %d   max players %d   game type %d\n", g_serverPort, g_maxClients, g_gameType);
	printf(" admin: %s\n", g_adminPassword[0] ? "password (ADMIN_PASSWORD)" : "first client to connect");
	printf(" Ctrl-C to shut down.\n");
	printf("========================================================\n");
	fflush(stdout);

	HttpInit();   // optional web dashboard (DASHBOARD_PORT)

	// Pump loop. Receive() does socket I/O AND dispatches RPC handlers in-place.
	while (g_run)
	{
		Packet* p = g_server->Receive();
		while (p)
		{
			switch (PacketId(p))
			{
				case ID_NEW_INCOMING_CONNECTION:
					if (g_allowlaptop) { // reject joins once the game is locked
						VLOG("[ja2server] rejecting late connection (game locked)\n"); fflush(stdout);
						g_server->CloseConnection(p->systemAddress, true);
					}
					break;
				case ID_DISCONNECTION_NOTIFICATION:
				case ID_CONNECTION_LOST:
					HandleDisconnect(p->systemAddress);
					break;
				default: break;
			}
			g_server->DeallocatePacket(p);
			p = g_server->Receive();
		}
		if (g_reset)
		{
			g_reset = 0;
			printf("[ja2server] SIGHUP -- kicking all clients and resetting\n"); fflush(stdout);
			for (int i = 0; i < 4; i++)
				if (g_clients[i].address.binaryAddress)
					g_server->CloseConnection(g_clients[i].address, true);
			ResetGameState();
			memset(g_knownName, 0, sizeof(g_knownName));   // manual reset = fresh session
		}
		HttpTick();   // serve at most one slice of dashboard work per tick
		RakSleep(10);
	}

	printf("\n[ja2server] shutting down...\n"); fflush(stdout);
	HttpShutdown();
	g_server->Shutdown(300);
	RakNetworkFactory::DestroyRakPeerInterface(g_server);
	SDL_Quit();
	return 0;
}
