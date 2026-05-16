// Multiplayer stubs for non-Windows builds.
//
// JA2's networking is RakNet 3.401, shipped only as a prebuilt Win32
// .lib (Multiplayer/raknet/RakNetLibStatic.lib). The matching RakNet
// 3.x source is not publicly available; Facebook open-sourced only
// 4.x, which has incompatible API.
//
// This file provides do-nothing definitions for every external
// symbol exported by the Multiplayer wrapper (client.cpp, server.cpp,
// transfer_rules.cpp), so the main JA2 executable links on
// non-Windows. is_networked / is_connected etc. stay false, the
// send_* functions are no-ops; the game runs in single-player.
//
// Real multiplayer on non-Windows requires either porting JA2's
// wrapper to RakNet 4 (substantial: ~327 RakNet API references,
// several removed APIs like RakNetworkFactory / AutoRPC / RPC) or
// retargeting to a different netlib (SDL3_net, enet, asio, ...).

#ifndef _WIN32

#include "types.h"
#include "connect.h"
#include "fresh_header.h"
#include "network.h"
#include "transfer_rules.h"

// ---- Global flags / counters ------------------------------------------------
bool isOwnTeamWipedOut = false;
bool is_connected      = false;
bool is_connecting     = false;
bool is_client         = false;
bool is_server         = false;
bool is_networked      = false;
bool is_host           = false;
bool recieved_settings = false;
bool recieved_transfer_settings = false;
bool allowlaptop       = false;
bool is_game_started   = false;
bool requested         = false;
bool getReal           = false;
bool auto_retry        = false;
bool Sawarded          = false;

INT16   serverSyncClientsDirectory = 0;
// PLAYER_TEAM_TIMER_SEC_PER_TICKS lives in Tactical/Interface.cpp;
// stub removed -- the real definition gets pulled in.
int     CLIENT_NUM = 0;
int     numreadyteams = 0;
int     giNumTries = 0;
int     readyteamreg[10]      = {0};
int     client_ready[4]       = {0};
int     client_teams[4]       = {0};
int     client_edges[5]       = {0};
int     client_downloading[4] = {0};
int     client_progress[4]    = {0};

UINT8   gEnemyEnabled = 0;
UINT8   gCreatureEnabled = 0;
UINT8   gMilitiaEnabled = 0;
UINT8   gCivEnabled = 0;
UINT8   cMaxClients = 0;
UINT8   cSameMercAllowed = 0;
UINT8   cGameType = 0;
UINT8   netbTeam = 0;
UINT8   gRandomStartingEdge = 0;
UINT8   gRandomMercs = 0;
UINT8   gMaxEnemiesEnabled = 0;
UINT8   cAllowMercEquipment = 0;
UINT8   cMaxMercs = 0;
// gubCheatLevel lives in Strategic/Game_Init.cpp.
UINT8   cWeaponReadyBonus = 0;
UINT8   cDisableSpectatorMode = 0;
UINT8   cDisableMorale = 0;

UINT16  ubID_prefix = 0;

INT32   gCurrentTransferBytes = 0;
INT32   gTotalTransferBytes = 0;

UINT32  crate_usMapPos = 0;
UINT32  setID = 0;

FLOAT   cDamageMultiplier = 1.0f;

STRING512 gCurrentTransferFilename = {0};
CHAR16    TeamNameStrings[1][30]   = {{0}};
char      cServerName[30]          = {0};
char      cClientName[30]          = {0};
char      client_names[4][30]      = {{0}};
player_stats gMPPlayerStats[5]     = {};

// gfUIInterfaceSetBusy lives in Tactical/Handle_UI.cpp.
BOOLEAN   fClientReceivedAllFiles  = FALSE;

// ---- Function stubs ---------------------------------------------------------
// lockui lives in TileEngine/Tactical_Placement_GUI.cpp.
void start_battle ( void ) {}
void DropOffItemsInSector( UINT8 ) {}
void mp_help (void) {}
void mp_help2 (void) {}
void grid_display ( void) {}
void send_loaded (void) {}
void send_donegui ( UINT8 ) {}
UINT8 numenemyLAN( UINT8, UINT8 ) { return 0; }
void connect_client ( void ) {}
void start_server (void) {}
void client_packet ( void ) {}
void server_packet ( void ) {}
void NetworkAutoStart() {}
void server_disconnect (void) {}
void client_disconnect (void) {}
// DialogRemoved lives in TileEngine/Tactical_Placement_GUI.cpp.
void manual_overide(void) {}
void send_path ( SOLDIERTYPE*, INT32, UINT16, BOOLEAN, BOOLEAN ) {}
void send_stance ( SOLDIERTYPE*, UINT8 ) {}
void send_dir ( SOLDIERTYPE*, UINT16 ) {}
void send_fire( SOLDIERTYPE*, INT32 ) {}
void send_hit( EV_S_WEAPONHIT* ) {}
void send_bullet( BULLET*, UINT16 ) {}
void send_hire( SoldierID, UINT8, INT16, BOOLEAN ) {}
void send_dismiss( UINT16 ) {}
void send_gui_pos( SOLDIERTYPE*, FLOAT, FLOAT ) {}
void send_gui_dir( SOLDIERTYPE*, UINT16 ) {}
void send_EndTurn( UINT8 ) {}
void send_AI( SOLDIERCREATE_STRUCT* ) {}
void send_stop( EV_S_STOP_MERC* ) {}
void send_interrupt( SOLDIERTYPE* ) {}
void send_grenade( OBJECTTYPE*, float, float, float, float, float, float, float, UINT32, SoldierID, UINT8, UINT32, INT32, bool ) {}
void send_grenade_result( float, float, float, INT32, SoldierID, INT32, bool ) {}
void send_plant_explosive( SoldierID, UINT16, UINT8, UINT16, UINT32, UINT8, UINT32 ) {}
void send_detonate_explosive( UINT32, SoldierID ) {}
void send_spreadeffect( INT32, UINT8, UINT16, SoldierID, BOOLEAN, INT8, INT32 ) {}
void send_newsmokeeffect( INT32, UINT16, INT8, SoldierID, INT32 ) {}
void send_gasdamage( SOLDIERTYPE*, UINT16, INT16, BOOLEAN, INT16, INT16, SoldierID ) {}
void send_explosivedamage( SoldierID, SoldierID, INT32, INT16, INT16, UINT32, UINT16, INT16 ) {}
void send_disarm_explosive( UINT32, UINT32, SoldierID ) {}
void OpenChatMsgBox(void) {}
// FireBullet lives in Tactical/LOS.cpp.
void reapplySETTINGS() {}
void send_settings() {}
void send_mapchange() {}
void send_teamchange( int ) {}
void send_edgechange( int ) {}
bool can_teamchange() { return false; }
void game_over( void ) {}

// fresh_header.h additional functions
// DisplayMercsInventory lives in Laptop/AimMembers.cpp.
void send_door( SOLDIERTYPE*, INT32, BOOLEAN ) {}
void send_changestate( EV_S_CHANGESTATE* ) {}
void send_death( SOLDIERTYPE* ) {}
void send_hitstruct( EV_S_STRUCTUREHIT* ) {}
void send_hitwindow( EV_S_WINDOWHIT* ) {}
void send_miss( EV_S_MISS* ) {}
void unlock(void) {}
void UpdateSoldierToNetwork( SOLDIERTYPE* ) {}
// TrashAllSoldiers lives in Tactical/Soldier_Create.cpp.
void kick_player(void) {}
void overide_turn(void) {}
void send_fireweapon( EV_S_FIREWEAPON* ) {}
void end_interrupt( BOOLEAN ) {}
// EndInterrupt lives in Tactical/TeamTurns.cpp.
void sendRT(void) {}
void startCombat(UINT8) {}
void intAI( SOLDIERTYPE* ) {}
void teamwiped(void) {}
BOOLEAN check_status(void) { return FALSE; }
void send_heal( SOLDIERTYPE* ) {}
void requestAIint( SOLDIERTYPE* ) {}

// ---- CTransferRules ---------------------------------------------------------
CTransferRules::CTransferRules() : m_eDefaultAction(DENY) {}
bool    CTransferRules::initFromTxtFile(vfs::Path const&)         { return true; }
bool    CTransferRules::initFromTxtFile(vfs::tReadableFile*)      { return true; }
void    CTransferRules::setDefaultAction(EAction act)             { m_eDefaultAction = act; }
CTransferRules::EAction CTransferRules::getDefaultAction()        { return m_eDefaultAction; }
CTransferRules::EAction CTransferRules::applyRule(vfs::String const&) { return m_eDefaultAction; }

#endif // !_WIN32
