// Data-free end-to-end test for the real standalone coordinator pump.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "CoordinatorProtocol.h"
#include "MessageIdentifiers.h"
#include "RakNetworkFactory.h"
#include "RakPeerInterface.h"

int ja2server_test_main(int argc, char** argv);

static int g_failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++g_failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, m); } else std::printf("ok   %s\n", m); } while (0)

struct ClientLog
{
	RakPeerInterface* peer = nullptr;
	SystemAddress server;
	bool accepted = false;
	bool closed = false;
	int admin = 0;
	int interrupts = 0;
	int resumes = 0;
	int gameovers = 0;
	std::vector<settings_struct> settings;
	std::vector<filetransfersettings_struct> transfer;
	std::vector<ready_struct> ready;
	std::vector<ready_struct> gui;
	std::vector<turn_struct> turns;
	std::vector<edgechange_struct> edges;
	std::vector<teamchange_struct> teams;
	std::vector<kickR> nullTeams;
	std::vector<death_struct> deaths;
	std::vector<int> disconnects;

	void Pump()
	{
		if (!peer) return;
		for (Packet* p = peer->Receive(); p; p = peer->Receive())
		{
			unsigned char id = p->data[0];
			if (id == ID_CONNECTION_REQUEST_ACCEPTED) { accepted = true; server = p->systemAddress; }
			if (id == ID_DISCONNECTION_NOTIFICATION || id == ID_CONNECTION_LOST ||
			    id == ID_CONNECTION_ATTEMPT_FAILED || id == ID_NO_FREE_INCOMING_CONNECTIONS)
				closed = true;
			peer->DeallocatePacket(p);
		}
	}

	void Stop()
	{
		if (!peer) return;
		peer->Shutdown(0);
		RakNetworkFactory::DestroyRakPeerInterface(peer);
		peer = nullptr;
	}
};

static ClientLog* g_logs[4] = { nullptr, nullptr, nullptr, nullptr };

template <typename T>
static void Capture(std::vector<T>& out, RPCParameters* p)
{
	if ((p->numberOfBitsOfData + 7) / 8 < sizeof(T)) return;
	T value;
	std::memcpy(&value, p->input, sizeof(value));
	out.push_back(value);
}

#define CLIENT_HANDLERS(tag, index) \
	static void tag##_settings(RPCParameters* p) { Capture(g_logs[index]->settings, p); } \
	static void tag##_transfer(RPCParameters* p) { Capture(g_logs[index]->transfer, p); } \
	static void tag##_ready(RPCParameters* p) { Capture(g_logs[index]->ready, p); } \
	static void tag##_gui(RPCParameters* p) { Capture(g_logs[index]->gui, p); } \
	static void tag##_turn(RPCParameters* p) { Capture(g_logs[index]->turns, p); } \
	static void tag##_edge(RPCParameters* p) { Capture(g_logs[index]->edges, p); } \
	static void tag##_team(RPCParameters* p) { Capture(g_logs[index]->teams, p); } \
	static void tag##_nullteam(RPCParameters* p) { Capture(g_logs[index]->nullTeams, p); } \
	static void tag##_death(RPCParameters* p) { Capture(g_logs[index]->deaths, p); } \
	static void tag##_disconnect(RPCParameters* p) { Capture(g_logs[index]->disconnects, p); } \
	static void tag##_admin(RPCParameters*) { g_logs[index]->admin++; } \
	static void tag##_interrupt(RPCParameters*) { g_logs[index]->interrupts++; } \
	static void tag##_resume(RPCParameters*) { g_logs[index]->resumes++; } \
	static void tag##_gameover(RPCParameters*) { g_logs[index]->gameovers++; }

CLIENT_HANDLERS(c0, 0)
CLIENT_HANDLERS(c1, 1)
CLIENT_HANDLERS(c2, 2)
CLIENT_HANDLERS(c3, 3)

typedef void (*RpcHandler)(RPCParameters*);
static RpcHandler SETTINGS[4] = { c0_settings, c1_settings, c2_settings, c3_settings };
static RpcHandler TRANSFER[4] = { c0_transfer, c1_transfer, c2_transfer, c3_transfer };
static RpcHandler READY[4] = { c0_ready, c1_ready, c2_ready, c3_ready };
static RpcHandler GUI[4] = { c0_gui, c1_gui, c2_gui, c3_gui };
static RpcHandler TURN[4] = { c0_turn, c1_turn, c2_turn, c3_turn };
static RpcHandler EDGE[4] = { c0_edge, c1_edge, c2_edge, c3_edge };
static RpcHandler TEAM[4] = { c0_team, c1_team, c2_team, c3_team };
static RpcHandler NULLTEAM[4] = { c0_nullteam, c1_nullteam, c2_nullteam, c3_nullteam };
static RpcHandler DEATH[4] = { c0_death, c1_death, c2_death, c3_death };
static RpcHandler DISCONNECT[4] = { c0_disconnect, c1_disconnect, c2_disconnect, c3_disconnect };
static RpcHandler ADMIN[4] = { c0_admin, c1_admin, c2_admin, c3_admin };
static RpcHandler INTERRUPT[4] = { c0_interrupt, c1_interrupt, c2_interrupt, c3_interrupt };
static RpcHandler RESUME[4] = { c0_resume, c1_resume, c2_resume, c3_resume };
static RpcHandler GAMEOVER[4] = { c0_gameover, c1_gameover, c2_gameover, c3_gameover };

static bool StartClient(ClientLog& c, int index, unsigned short port)
{
	g_logs[index] = &c;
	c.peer = RakNetworkFactory::GetRakPeerInterface();
	SocketDescriptor local;
	if (!c.peer->Startup(1, 30, &local, 1)) return false;
	c.peer->RegisterAsRemoteProcedureCall("recieveSETTINGS", SETTINGS[index]);
	c.peer->RegisterAsRemoteProcedureCall("recieveFILE_TRANSFER_SETTINGS", TRANSFER[index]);
	c.peer->RegisterAsRemoteProcedureCall("recieveREADY", READY[index]);
	c.peer->RegisterAsRemoteProcedureCall("recieveGUI", GUI[index]);
	c.peer->RegisterAsRemoteProcedureCall("recieveEndTurn", TURN[index]);
	c.peer->RegisterAsRemoteProcedureCall("recieveEDGECHANGE", EDGE[index]);
	c.peer->RegisterAsRemoteProcedureCall("recieveTEAMCHANGE", TEAM[index]);
	c.peer->RegisterAsRemoteProcedureCall("null_team", NULLTEAM[index]);
	c.peer->RegisterAsRemoteProcedureCall("recieveDEATH", DEATH[index]);
	c.peer->RegisterAsRemoteProcedureCall("recieveDISCONNECT", DISCONNECT[index]);
	c.peer->RegisterAsRemoteProcedureCall("recieveADMIN", ADMIN[index]);
	c.peer->RegisterAsRemoteProcedureCall("recieveINTERRUPT", INTERRUPT[index]);
	c.peer->RegisterAsRemoteProcedureCall("resume_turn", RESUME[index]);
	c.peer->RegisterAsRemoteProcedureCall("recieveGAMEOVER", GAMEOVER[index]);
	return c.peer->Connect("127.0.0.1", port, nullptr, 0);
}

static std::vector<ClientLog*> g_active;

template <typename Pred>
static bool PumpUntil(Pred pred, int timeoutMs = 4000)
{
	auto start = std::chrono::steady_clock::now();
	for (;;)
	{
		for (ClientLog* c : g_active) c->Pump();
		if (pred()) return true;
		if (std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - start).count() >= timeoutMs) return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}

static void PumpFor(int milliseconds)
{
	auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
	while (std::chrono::steady_clock::now() < end)
	{
		for (ClientLog* c : g_active) c->Pump();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}

static void SendRaw(ClientLog& c, const char* rpc, const void* data, size_t bytes)
{
	c.peer->RPC(rpc, (const char*)data, (BitSize_t)(bytes * 8), HIGH_PRIORITY,
	            RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0,
	            UNASSIGNED_NETWORK_ID, 0);
}

template <typename T>
static void Send(ClientLog& c, const char* rpc, const T& value)
{
	SendRaw(c, rpc, &value, sizeof(value));
}

static void Join(ClientLog& c, const char* name, const char* version = MPVERSION)
{
	SendRaw(c, "requestFILE_TRANSFER_SETTINGS", "", 0);
	client_info info = {};
	std::strncpy(info.client_name, name, sizeof(info.client_name) - 1);
	std::strncpy(info.client_version, version, sizeof(info.client_version) - 1);
	Send(c, "requestSETTINGS", info);
}

static int ReadyStage(const ClientLog& c, int stage)
{
	int count = 0;
	for (const ready_struct& r : c.ready) if (r.ready_stage == stage) count++;
	return count;
}

static int GuiStage(const ClientLog& c, int stage)
{
	int count = 0;
	for (const ready_struct& r : c.gui) if (r.ready_stage == stage) count++;
	return count;
}

static unsigned char InterruptWireTeam(const unsigned char wire[10]) { return wire[2]; }

int main(int argc, char** argv)
{
	const auto seed = (unsigned long long)std::chrono::steady_clock::now().time_since_epoch().count();
	const unsigned short port = (unsigned short)(40000 + seed % 20000);
	const std::string base = (std::filesystem::temp_directory_path() /
		("ja2-mp-coordinator-" + std::to_string(seed))).string();
	const std::string coopIni = base + "-coop.ini";
	const std::string dmIni = base + "-dm.ini";
	{
		std::ofstream f(coopIni); f << "GAME_TYPE = 2\n";
	}
	char serverName[] = "ja2server";
	char iniFlag[] = "--ini";
	char* coopArgs[] = { serverName, iniFlag, const_cast<char*>(coopIni.c_str()) };
	CHECK(ja2server_test_main(3, coopArgs) == 2,
	      "standalone GAME_TYPE=2 exits explicitly before networking");
	if (argc > 1 && !std::strcmp(argv[1], "--coop-only"))
	{
		std::remove(coopIni.c_str());
		return g_failures ? 1 : 0;
	}

	{
		std::ofstream f(dmIni);
		f << "SERVER_NAME = Coordinator E2E\nSERVER_BIND = 127.0.0.1\nSERVER_PORT = " << port
		  << "\nGAME_TYPE = 1\nMAX_PLAYERS = 4\nDASHBOARD_PORT = 0\nLOG_LEVEL = 1\n";
	}
	std::atomic<int> serverResult(-999);
	char* dmArgs[] = { serverName, iniFlag, const_cast<char*>(dmIni.c_str()) };
	std::thread serverThread([&] { serverResult.store(ja2server_test_main(3, dmArgs)); });
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	if (serverResult.load() != -999)
	{
		if (serverResult.load() == 77)
		{
			std::printf("SKIP loopback listener denied by execution sandbox\n");
			serverThread.join();
			std::remove(coopIni.c_str()); std::remove(dmIni.c_str());
			return 77;
		}
		CHECK(false, "coordinator listener started");
		serverThread.join();
		std::remove(coopIni.c_str()); std::remove(dmIni.c_str());
		return 1;
	}

	ClientLog a, b;
	g_active = { &a, &b };
	CHECK(StartClient(a, 0, port), "client A connection initiated");
	CHECK(PumpUntil([&] { return a.accepted; }), "client A connected to real coordinator pump");
	Join(a, "Alice");
	CHECK(PumpUntil([&] { return !a.settings.empty() && !a.transfer.empty() && a.admin == 1; }),
	      "client A admitted in slot 1 and becomes admin");
	CHECK(a.settings.back().client_num == 1 && a.settings.back().gameType == MP_TYPE_TEAMDEATMATCH,
	      "slot 1 receives PvP settings");

	CHECK(StartClient(b, 1, port), "client B connection initiated");
	CHECK(PumpUntil([&] { return b.accepted; }), "client B connected");
	Join(b, "Bob");
	CHECK(PumpUntil([&] { return !b.settings.empty() && a.settings.size() >= 2; }),
	      "second admission broadcasts the updated roster");
	const settings_struct& roster = b.settings.back();
	CHECK(roster.client_num == 2 && !std::strcmp(roster.client_names[0], "Alice") &&
	      !std::strcmp(roster.client_names[1], "Bob"), "stable slots 1/2 and two-player roster");
	CHECK(a.admin == 1 && b.admin == 0, "only first admitted client is admin");

	// Lobby roster mutations are sender-authored and value/phase constrained.
	edgechange_struct edge = { 1, MP_EDGE_WEST };
	Send(b, "sendEDGECHANGE", edge);
	CHECK(PumpUntil([&] { return a.edges.size() == 1; }) &&
	      a.edges.back().client_num == 2 && a.edges.back().newedge == MP_EDGE_WEST,
	      "edge change is authored from slot 2, not forged client_num");
	edge.newedge = 9; Send(b, "sendEDGECHANGE", edge); PumpFor(100);
	CHECK(a.edges.size() == 1, "invalid spawn edge is rejected");
	teamchange_struct team = { 1, 2 };
	Send(b, "sendTEAMCHANGE", team);
	CHECK(PumpUntil([&] { return a.teams.size() == 1; }) &&
	      a.teams.back().client_num == 2 && a.teams.back().newteam == 2,
	      "team change is authored from slot 2, not forged client_num");
	size_t settingsBefore = a.settings.size();
	Join(a, "Alice");
	CHECK(PumpUntil([&] { return a.settings.size() > settingsBefore; }) &&
	      a.settings.back().client_edges[1] == MP_EDGE_WEST &&
	      a.settings.back().client_teams[1] == 2,
	      "authored edge/team choices persist in coordinator roster settings");
	team.newteam = 4; Send(b, "sendTEAMCHANGE", team); PumpFor(100);
	CHECK(a.teams.size() == 1, "invalid team selection is rejected");
	kickR target = { 6 }; Send(b, "Snull_team", target); PumpFor(100);
	CHECK(a.nullTeams.empty(), "non-admin cannot null another player's team");
	target.ubResult = 9; Send(a, "Snull_team", target); PumpFor(100);
	CHECK(a.nullTeams.empty(), "admin cannot target an unoccupied team");
	target.ubResult = 7; Send(a, "Snull_team", target);
	CHECK(PumpUntil([&] { return a.nullTeams.size() == 1 && b.nullTeams.size() == 1; }) &&
	      a.nullTeams.back().ubResult == 7, "admin can issue a validated occupied-team null");

	// A connected but unadmitted peer cannot invoke any session authority.
	ClientLog rogue;
	g_active.push_back(&rogue);
	CHECK(StartClient(rogue, 3, port), "unadmitted peer connection initiated");
	CHECK(PumpUntil([&] { return rogue.accepted; }), "unadmitted peer connected");
	admin_cmd_struct start = {}; start.cmd = ADMIN_CMD_START;
	ready_struct fakeReady = { 1, true, 0 };
	sc_struct fakeCombat = { 6 };
	Send(rogue, "adminCmd", start); Send(rogue, "sendREADY", fakeReady);
	Send(rogue, "startCOMBAT", fakeCombat); SendRaw(rogue, "sendGAMEOVER", "", 0);
	PumpFor(120);
	CHECK(ReadyStage(a, 36) == 0 && a.turns.empty() && a.gameovers == 0,
	      "unadmitted sender cannot advance lobby, combat, or game-over");
	Join(rogue, "OldClient", "MP v0");
	CHECK(PumpUntil([&] { return rogue.closed; }), "protocol-version mismatch is disconnected");
	CHECK(rogue.settings.empty() && rogue.admin == 0, "rejected version receives no slot or admin");
	rogue.Stop(); g_active.pop_back(); g_logs[3] = nullptr;

	// Admin ownership and idempotent, sender-authored ready barrier.
	Send(b, "adminCmd", start); PumpFor(100);
	CHECK(ReadyStage(a, 36) == 0, "non-admin cannot unlock the lobby");
	Send(a, "adminCmd", start);
	CHECK(PumpUntil([&] { return ReadyStage(a, 36) == 1 && ReadyStage(b, 36) == 1; }),
	      "admin unlock reaches both admitted clients");
	unsigned char invalidReady[3] = { 1, 2, 0 };
	SendRaw(a, "sendREADY", invalidReady, sizeof(invalidReady)); PumpFor(100);
	CHECK(ReadyStage(b, 0) == 0, "invalid raw bool cannot cast a ready vote");
	ready_struct aReady = { 4, true, 0 };
	Send(a, "sendREADY", aReady);
	CHECK(PumpUntil([&] { return ReadyStage(b, 0) == 1; }), "ready vote is relayed");
	CHECK(b.ready.back().client_num == 1, "ready identity is derived from Alice's slot");
	Send(a, "sendREADY", aReady); PumpFor(100);
	CHECK(ReadyStage(b, 0) == 1 && ReadyStage(a, 1) == 0,
	      "duplicate ready vote is a no-op and cannot trip the barrier");
	ready_struct bReady = { 1, true, 0 };
	Send(b, "sendREADY", bReady);
	CHECK(PumpUntil([&] { return ReadyStage(a, 1) == 1 && ReadyStage(b, 1) == 1; }),
	      "two distinct admitted senders cross the ready barrier");

	ClientLog late;
	g_active.push_back(&late);
	CHECK(StartClient(late, 3, port), "late peer connection initiated");
	CHECK(PumpUntil([&] { return late.closed; }), "late join is rejected after lobby lock");
	CHECK(late.settings.empty(), "late join never enters the roster");
	late.Stop(); g_active.pop_back(); g_logs[3] = nullptr;

	// Placement stages are coordinator-owned and count each admitted slot once.
	unsigned char invalidGui[3] = { 1, 2, 1 };
	SendRaw(a, "sendGUI", invalidGui, sizeof(invalidGui)); PumpFor(100);
	CHECK(GuiStage(a, 2) == 0, "invalid raw bool cannot cast a placement vote");
	ready_struct injected = { 2, true, 2 };
	Send(b, "sendGUI", injected); PumpFor(100);
	CHECK(GuiStage(a, 2) == 0, "client cannot inject coordinator GUI stage 2");
	ready_struct loadedA = { 2, true, 1 };
	Send(a, "sendGUI", loadedA); Send(a, "sendGUI", loadedA); PumpFor(100);
	CHECK(GuiStage(a, 2) == 0, "duplicate loaded vote cannot unlock placement");
	ready_struct loadedB = { 1, true, 1 };
	Send(b, "sendGUI", loadedB);
	CHECK(PumpUntil([&] { return GuiStage(a, 2) == 1 && GuiStage(b, 2) == 1; }),
	      "two distinct loaded clients unlock placement");
	ready_struct placedA = { 3, true, 3 };
	Send(a, "sendGUI", placedA); Send(a, "sendGUI", placedA); PumpFor(100);
	CHECK(GuiStage(a, 4) == 0, "duplicate placed vote cannot enter tactical");
	ready_struct placedB = { 1, true, 3 };
	Send(b, "sendGUI", placedB);
	CHECK(PumpUntil([&] { return GuiStage(a, 4) == 1 && GuiStage(b, 4) == 1; }),
	      "two distinct placed clients enter tactical");
	edge.newedge = MP_EDGE_NORTH; Send(b, "sendEDGECHANGE", edge); PumpFor(100);
	CHECK(a.edges.size() == 1, "roster changes are rejected after lobby lock");

	death_struct death = { 10, 20, 1, 1 };
	Send(b, "sendDEATH", death); PumpFor(100);
	CHECK(a.deaths.empty(), "client cannot report another slot as its dead soldier");
	death.soldier_team = 2; death.attacker_team = 4;
	Send(b, "sendDEATH", death); PumpFor(100);
	CHECK(a.deaths.empty(), "client cannot credit an unoccupied attacker slot");
	death.attacker_team = 1;
	Send(b, "sendDEATH", death);
	CHECK(PumpUntil([&] { return a.deaths.size() == 1; }) &&
	      a.deaths.back().soldier_team == 2 && a.deaths.back().attacker_team == 1,
	      "valid cross-client death credits the occupied attacker slot");

	Send(b, "sendGAMEOVER", bReady); PumpFor(100);
	CHECK(a.gameovers == 0, "client GAMEOVER claim is denied");
	sc_struct spoofWipe = { 6 };
	Send(b, "sendWIPE", spoofWipe); PumpFor(100);
	CHECK(a.gameovers == 0, "client cannot wipe another slot's team");

	// Turn and interrupt authority use the admitted sender's slot identity.
	sc_struct team6 = { 6 };
	Send(b, "startCOMBAT", team6); PumpFor(100);
	CHECK(a.turns.empty(), "spoofed combat-start team is rejected");
	Send(a, "startCOMBAT", team6);
	CHECK(PumpUntil([&] { return !a.turns.empty() && !b.turns.empty(); }),
	      "valid slot-1 combat start broadcasts the first turn");
	CHECK(a.turns.back().tsubNextTeam == 6, "team 6 owns the first turn");
	turn_struct end6 = { 6, 0 }, end7 = { 7, 0 };
	size_t turns = a.turns.size();
	Send(b, "sendEndTurn", end6); PumpFor(100);
	CHECK(a.turns.size() == turns, "slot 2 cannot end slot 1's turn");
	Send(a, "sendEndTurn", end6);
	CHECK(PumpUntil([&] { return a.turns.size() == turns + 1; }) && a.turns.back().tsubNextTeam == 7,
	      "valid slot-1 end-turn advances to team 7");
	turns = a.turns.size();
	Send(a, "sendEndTurn", end7); PumpFor(100);
	CHECK(a.turns.size() == turns, "slot 1 cannot claim team 7 identity");
	Send(b, "sendEndTurn", end7);
	CHECK(PumpUntil([&] { return a.turns.size() == turns + 1; }) && a.turns.back().tsubNextTeam == 6,
	      "valid slot-2 end-turn returns authority to team 6");

	unsigned char int6[10] = { 1, 0, 6, 0, 0, 1, 2, 0, 1, 0 };
	unsigned char int7[10]; std::memcpy(int7, int6, sizeof(int7)); int7[2] = 7;
	CHECK(InterruptWireTeam(int7) == 7, "interrupt fixture carries team byte at wire offset 2");
	unsigned char countMismatch[10]; std::memcpy(countMismatch, int7, sizeof(countMismatch));
	countMismatch[3] = 1;
	SendRaw(b, "sendINTERRUPT", countMismatch, sizeof(countMismatch));
	std::vector<unsigned char> trailing(int7, int7 + sizeof(int7)); trailing.push_back(0);
	SendRaw(b, "sendINTERRUPT", trailing.data(), trailing.size());
	std::vector<unsigned char> maxInterrupt(COORDINATOR_INT_WIRE_MAX_BYTES, 0);
	maxInterrupt[2] = 7;
	const UINT16 maxPersons = COORDINATOR_INT_WIRE_ORDER_ENTRIES - 1;
	maxInterrupt[3] = (unsigned char)(maxPersons & 0xff);
	maxInterrupt[4] = (unsigned char)(maxPersons >> 8);
	std::vector<unsigned char> invalidPersons = maxInterrupt;
	invalidPersons[3] = (unsigned char)(COORDINATOR_INT_WIRE_ORDER_ENTRIES & 0xff);
	invalidPersons[4] = (unsigned char)(COORDINATOR_INT_WIRE_ORDER_ENTRIES >> 8);
	std::vector<unsigned char> overInterrupt = maxInterrupt; overInterrupt.push_back(0);
	SendRaw(b, "sendINTERRUPT", invalidPersons.data(), invalidPersons.size());
	SendRaw(b, "sendINTERRUPT", overInterrupt.data(), overInterrupt.size());
	PumpFor(100);
	CHECK(a.interrupts == 0 && b.interrupts == 0,
	      "mismatched, trailing, invalid-count, and over-bound interrupt frames are rejected");
	SendRaw(b, "sendINTERRUPT", maxInterrupt.data(), maxInterrupt.size());
	CHECK(PumpUntil([&] { return a.interrupts == 1 && b.interrupts == 1; }),
	      "exact maximum portable interrupt frame is accepted");
	SendRaw(b, "endINTERRUPT", overInterrupt.data(), overInterrupt.size()); PumpFor(100);
	CHECK(a.resumes == 0 && b.resumes == 0, "over-bound interrupt release is rejected");
	SendRaw(b, "endINTERRUPT", maxInterrupt.data(), maxInterrupt.size());
	CHECK(PumpUntil([&] { return a.resumes == 1 && b.resumes == 1; }),
	      "exact maximum portable interrupt release is accepted");
	a.interrupts = b.interrupts = 0;
	a.resumes = b.resumes = 0;
	SendRaw(a, "sendINTERRUPT", int7, sizeof(int7)); PumpFor(100);
	CHECK(a.interrupts == 0 && b.interrupts == 0, "spoofed interrupt identity is rejected");
	SendRaw(b, "sendINTERRUPT", int7, sizeof(int7));
	CHECK(PumpUntil([&] { return a.interrupts == 1 && b.interrupts == 1; }),
	      "valid slot-2 interrupt is granted to the session");
	SendRaw(a, "sendINTERRUPT", int6, sizeof(int6));
	SendRaw(a, "sendINTERRUPT", int6, sizeof(int6));
	SendRaw(a, "sendINTERRUPT", int6, sizeof(int6)); PumpFor(100);
	CHECK(a.interrupts == 1 && b.interrupts == 1,
	      "queued interrupt requests do not alter the active grant");
	SendRaw(a, "endINTERRUPT", int6, sizeof(int6)); PumpFor(100);
	CHECK(a.resumes == 0 && b.resumes == 0, "non-holder cannot release an interrupt");
	SendRaw(b, "endINTERRUPT", int7, sizeof(int7));
	CHECK(PumpUntil([&] { return a.resumes == 1 && b.resumes == 1 &&
	                              a.interrupts == 2 && b.interrupts == 2; }),
	      "holder release resumes the turn and chains one queued sender");
	SendRaw(a, "endINTERRUPT", int6, sizeof(int6));
	CHECK(PumpUntil([&] { return a.resumes == 2 && b.resumes == 2; }),
	      "queued sender releases the chained interrupt");
	PumpFor(100);
	CHECK(a.interrupts == 2 && b.interrupts == 2,
	      "duplicate queued requests cannot create additional grants");

	const int beforeGameover = a.gameovers;
	b.peer->CloseConnection(b.server, true);
	CHECK(PumpUntil([&] { return !a.disconnects.empty(); }), "remaining client receives slot disconnect");
	CHECK(a.disconnects.back() == 2, "disconnect notice identifies slot 2");
	CHECK(PumpUntil([&] { return a.gameovers == beforeGameover + 1; }),
	      "coordinator, not a client, publishes last-standing game-over");
	b.Stop();
	a.peer->CloseConnection(a.server, true); PumpFor(50); a.Stop();
	g_active.clear();

	if (serverResult.load() == -999) std::raise(SIGTERM);
	serverThread.join();
	CHECK(serverResult.load() == 0, "coordinator shuts down cleanly after loopback session");
	std::remove(coopIni.c_str()); std::remove(dmIni.c_str());

	std::printf(g_failures ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", g_failures);
	return g_failures ? 1 : 0;
}
