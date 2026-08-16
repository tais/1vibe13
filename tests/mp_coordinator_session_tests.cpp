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
#include "SdlNetTransport.h"

using namespace ja2::mp;
using namespace ja2::mp::net;

int ja2server_test_main(int argc, char** argv);
bool ja2server_test_dashboard_bind_resolves(const char* host);
const char* ja2server_test_dashboard_html();
const char* ja2server_test_phase();
void ja2server_test_request_reset();
std::size_t ja2server_test_transport_count();

static int g_failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++g_failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, m); } else std::printf("ok   %s\n", m); } while (0)

struct ClientLog
{
	SdlNetPeer* peer = nullptr;
	ConnectionId server;
	bool accepted = false;
	bool closed = false;
	int admin = 0;
	int interrupts = 0;
	int resumes = 0;
	int realtime = 0;
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
		for (SdlNetEvent* p = peer->Poll(); p; p = peer->Poll())
		{
			unsigned char id = p->data[0];
			if (id == SDLNET_CONNECTION_ACCEPTED) { accepted = true; server = p->connection; }
			if (id == SDLNET_DISCONNECTION_NOTIFICATION || id == SDLNET_CONNECTION_LOST ||
			    id == SDLNET_CONNECTION_ATTEMPT_FAILED || id == SDLNET_NO_FREE_INCOMING_CONNECTIONS)
				closed = true;
			peer->Release(p);
		}
	}

	void Stop()
	{
		if (!peer) return;
		peer->Shutdown(0);
		DestroySdlNetPeer(peer);
		peer = nullptr;
	}
};

static ClientLog* g_logs[4] = { nullptr, nullptr, nullptr, nullptr };

template <typename T>
static void Capture(std::vector<T>& out, SdlNetMessage* p)
{
	if (p->size < sizeof(T)) return;
	T value;
	std::memcpy(&value, p->data, sizeof(value));
	out.push_back(value);
}

#define CLIENT_HANDLERS(tag, index) \
	static void tag##_settings(SdlNetMessage* p) { Capture(g_logs[index]->settings, p); } \
	static void tag##_transfer(SdlNetMessage* p) { Capture(g_logs[index]->transfer, p); } \
	static void tag##_ready(SdlNetMessage* p) { Capture(g_logs[index]->ready, p); } \
	static void tag##_gui(SdlNetMessage* p) { Capture(g_logs[index]->gui, p); } \
	static void tag##_turn(SdlNetMessage* p) { Capture(g_logs[index]->turns, p); } \
	static void tag##_edge(SdlNetMessage* p) { Capture(g_logs[index]->edges, p); } \
	static void tag##_team(SdlNetMessage* p) { Capture(g_logs[index]->teams, p); } \
	static void tag##_nullteam(SdlNetMessage* p) { Capture(g_logs[index]->nullTeams, p); } \
	static void tag##_death(SdlNetMessage* p) { Capture(g_logs[index]->deaths, p); } \
	static void tag##_disconnect(SdlNetMessage* p) { Capture(g_logs[index]->disconnects, p); } \
	static void tag##_admin(SdlNetMessage*) { g_logs[index]->admin++; } \
	static void tag##_interrupt(SdlNetMessage*) { g_logs[index]->interrupts++; } \
	static void tag##_resume(SdlNetMessage*) { g_logs[index]->resumes++; } \
	static void tag##_realtime(SdlNetMessage*) { g_logs[index]->realtime++; } \
	static void tag##_gameover(SdlNetMessage*) { g_logs[index]->gameovers++; }

CLIENT_HANDLERS(c0, 0)
CLIENT_HANDLERS(c1, 1)
CLIENT_HANDLERS(c2, 2)
CLIENT_HANDLERS(c3, 3)

typedef void (*RpcHandler)(SdlNetMessage*);
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
static RpcHandler REALTIME[4] = { c0_realtime, c1_realtime, c2_realtime, c3_realtime };
static RpcHandler GAMEOVER[4] = { c0_gameover, c1_gameover, c2_gameover, c3_gameover };

static bool StartClient(ClientLog& c, int index, unsigned short port)
{
	g_logs[index] = &c;
	c.peer = CreateSdlNetPeer();
	SdlNetEndpoint local;
	if (!c.peer->Start(1, local)) return false;
	c.peer->RegisterMessage("recieveSETTINGS", SETTINGS[index]);
	c.peer->RegisterMessage("recieveFILE_TRANSFER_SETTINGS", TRANSFER[index]);
	c.peer->RegisterMessage("recieveREADY", READY[index]);
	c.peer->RegisterMessage("recieveGUI", GUI[index]);
	c.peer->RegisterMessage("recieveEndTurn", TURN[index]);
	c.peer->RegisterMessage("recieveEDGECHANGE", EDGE[index]);
	c.peer->RegisterMessage("recieveTEAMCHANGE", TEAM[index]);
	c.peer->RegisterMessage("null_team", NULLTEAM[index]);
	c.peer->RegisterMessage("recieveDEATH", DEATH[index]);
	c.peer->RegisterMessage("recieveDISCONNECT", DISCONNECT[index]);
	c.peer->RegisterMessage("recieveADMIN", ADMIN[index]);
	c.peer->RegisterMessage("recieveINTERRUPT", INTERRUPT[index]);
	c.peer->RegisterMessage("resume_turn", RESUME[index]);
	c.peer->RegisterMessage("gotoRT", REALTIME[index]);
	c.peer->RegisterMessage("recieveGAMEOVER", GAMEOVER[index]);
	return c.peer->Connect("127.0.0.1", port);
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
	c.peer->SendMessage(rpc, data, bytes, AnyConnection, true);
}

template <typename T>
static void Send(ClientLog& c, const char* rpc, const T& value)
{
	SendRaw(c, rpc, &value, sizeof(value));
}

static void Join(ClientLog& c, const char* name, const char* version = MPVERSION, int team = 0)
{
	SendRaw(c, "requestFILE_TRANSFER_SETTINGS", "", 0);
	client_info info = {};
	std::strncpy(info.client_name, name, sizeof(info.client_name) - 1);
	std::strncpy(info.client_version, version, sizeof(info.client_version) - 1);
	info.team = team;
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
static bool EncodeInterruptWire(
	unsigned char (&wire)[10], UINT16 actor, UINT8 team,
	UINT8 markOccurred, UINT16 interrupted)
{
	const UINT16 order[1] = { 1 };
	return MpInterruptWire::Encode(
		wire, sizeof(wire), actor, team, 0, markOccurred, interrupted, order) == sizeof(wire);
}

int main(int argc, char** argv)
{
	CHECK(CoordinatorNextBarrierAction(true, false, false, false, 1, 1, 1, 0, 0) ==
	      CoordinatorBarrierAction::None,
	      "ready barrier cannot advance a sole remaining side");
	CHECK(CoordinatorNextBarrierAction(true, true, false, false, 1, 1, 0, 1, 0) ==
	      CoordinatorBarrierAction::None,
	      "load barrier cannot advance a sole remaining side");
	CHECK(CoordinatorNextBarrierAction(true, true, true, false, 1, 1, 0, 1, 1) ==
	      CoordinatorBarrierAction::None,
	      "placement barrier cannot advance a sole remaining side");
	CHECK(CoordinatorNextBarrierAction(true, false, false, false, 2, 2, 2, 0, 0) ==
	      CoordinatorBarrierAction::StartBattle,
	      "ready barrier advances two opposing sides");
	{
		const bool connected[4] = { true, true, true, true };
		const bool wiped[4] = { false, false, true, true };
		const int alliances[4] = { 0, 0, 1, 1 };
		CHECK(CoordinatorStandingSideCount(
		          MP_TYPE_TEAMDEATMATCH, connected, wiped, alliances) == 1,
		      "team deathmatch last-standing collapses transport slots onto alliances");
		CHECK(!CoordinatorTransportTeamActive(true, true),
		      "wiped-but-connected transport teams cannot issue tactical authority");
	}
	{
		const char* dashboard = ja2server_test_dashboard_html();
		CHECK(dashboard && !std::strstr(dashboard, "insertAdjacentHTML") &&
		      !std::strstr(dashboard, "innerHTML") && std::strstr(dashboard, "textContent=String(v)"),
		      "dashboard renders untrusted player names through textContent only");
		CHECK(dashboard && !std::strstr(dashboard, "Reset lobby</button>") &&
		      std::strstr(dashboard, "Disconnect all &amp; reset lobby"),
		      "dashboard exposes no roster-clearing reset without disconnecting transports");
	}
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

	// Manual reset owns every accepted transport, including a connection that has
	// not sent requestSETTINGS and therefore has no roster slot yet.
	{
		ClientLog orphan;
		g_active = { &orphan };
		CHECK(StartClient(orphan, 0, port), "pre-admission reset peer connection initiated");
		CHECK(PumpUntil([&] { return orphan.accepted; }), "pre-admission reset peer connected");
		ja2server_test_request_reset();
		CHECK(PumpUntil([&] { return orphan.closed; }),
		      "manual reset disconnects a pre-admission transport");
		orphan.Stop();
		g_active.clear(); g_logs[0] = nullptr;
	}

	// Real-pump disconnect coverage for all three session barriers. A remaining
	// sole TDM side must never auto-progress through ready/loading/placement.
	auto runBarrierDisconnect = [&](int phase)
	{
		ClientLog left, right;
		g_active = { &left, &right };
		CHECK(StartClient(left, 0, port), "barrier-left connection initiated");
		CHECK(PumpUntil([&] { return left.accepted; }), "barrier-left connected");
		Join(left, "GateLeft", MPVERSION, 0);
		CHECK(PumpUntil([&] { return !left.settings.empty() && left.admin == 1; }),
		      "barrier-left admitted as admin side 0");
		CHECK(StartClient(right, 1, port), "barrier-right connection initiated");
		CHECK(PumpUntil([&] { return right.accepted; }), "barrier-right connected");
		Join(right, "GateRight", MPVERSION, 1);
		CHECK(PumpUntil([&] { return !right.settings.empty() && left.settings.size() >= 2; }),
		      "barrier-right admitted as opposing side 1");

		admin_cmd_struct unlock = {}; unlock.cmd = ADMIN_CMD_START;
		Send(left, "adminCmd", unlock);
		CHECK(PumpUntil([&] { return ReadyStage(left, 36) == 1; }),
		      "barrier session hiring unlocked");
		ready_struct leftReady = { 1, true, 0 }, rightReady = { 1, true, 0 };
		Send(left, "sendREADY", leftReady);
		if (phase > 0)
		{
			Send(right, "sendREADY", rightReady);
			CHECK(PumpUntil([&] { return ReadyStage(left, 1) == 1; }),
			      "barrier session entered sector loading");
			ready_struct leftLoaded = { 1, true, 1 }, rightLoaded = { 1, true, 1 };
			Send(left, "sendGUI", leftLoaded);
			if (phase > 1)
			{
				Send(right, "sendGUI", rightLoaded);
				CHECK(PumpUntil([&] { return GuiStage(left, 2) == 1; }),
				      "barrier session unlocked placement");
				ready_struct leftPlaced = { 1, true, 3 };
				Send(left, "sendGUI", leftPlaced);
			}
		}

		const std::size_t disconnectsBefore = left.disconnects.size();
		right.peer->CloseConnection(right.server, true);
		CHECK(PumpUntil([&] { return left.disconnects.size() > disconnectsBefore; }),
		      "opposing barrier peer disconnect is observed");
		if (phase == 0)
		{
			CHECK(PumpUntil([&] { return left.closed; }),
			      "ready disconnect aborts and disconnects the locked sole side");
			CHECK(ReadyStage(left, 1) == 0 && left.gameovers == 0,
			      "ready disconnect cannot auto-start a sole side");
		}
		else if (phase == 1)
		{
			CHECK(PumpUntil([&] { return left.gameovers == 1; }),
			      "loading disconnect aborts a match that lost its opposing side");
			CHECK(GuiStage(left, 2) == 0,
			      "loading disconnect cannot unlock solo placement");
		}
		else
		{
			CHECK(PumpUntil([&] { return left.gameovers == 1; }),
			      "placement disconnect aborts a match that lost its opposing side");
			CHECK(GuiStage(left, 4) == 0,
			      "placement disconnect cannot enter solo tactical play");
		}

		right.Stop();
		if (!left.closed) left.peer->CloseConnection(left.server, true);
		PumpFor(75); left.Stop();
		g_active.clear(); g_logs[0] = nullptr; g_logs[1] = nullptr;

		if (phase == 0)
		{
			ClientLog replacement;
			g_active = { &replacement };
			CHECK(StartClient(replacement, 0, port), "replacement connection initiated after ready abort");
			CHECK(PumpUntil([&] { return replacement.accepted; }),
			      "replacement connects to reopened lobby");
			Join(replacement, "Replacement", MPVERSION, 0);
			CHECK(PumpUntil([&] { return !replacement.settings.empty() && replacement.admin == 1; }),
			      "replacement is admitted into the fresh lobby");
			ja2server_test_request_reset();
			CHECK(PumpUntil([&] { return replacement.closed; }),
			      "replacement cleanup reset closes its transport");
			replacement.Stop();
			g_active.clear(); g_logs[0] = nullptr;
		}
		else
		{
			ja2server_test_request_reset();
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
		}
	};
	runBarrierDisconnect(0);
	runBarrierDisconnect(1);
	runBarrierDisconnect(2);

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
	CHECK(ja2server_test_dashboard_bind_resolves("*"),
	      "explicit dashboard wildcard is a valid all-interface bind request");
	CHECK(!ja2server_test_dashboard_bind_resolves("invalid host name"),
	      "invalid explicit dashboard bind fails instead of becoming a wildcard");

	admin_cmd_struct start = {}; start.cmd = ADMIN_CMD_START;
	Send(a, "adminCmd", start); PumpFor(100);
	CHECK(ReadyStage(a, 36) == 0,
	      "team deathmatch cannot start until two selected alliances oppose each other");

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

	ClientLog badTeam;
	g_active.push_back(&badTeam);
	CHECK(StartClient(badTeam, 2, port), "invalid-team peer connection initiated");
	CHECK(PumpUntil([&] { return badTeam.accepted; }), "invalid-team peer connected");
	Join(badTeam, "BadTeam", MPVERSION, 99);
	CHECK(PumpUntil([&] { return badTeam.closed; }),
	      "out-of-range initial team is rejected before roster admission");
	CHECK(badTeam.settings.empty() && badTeam.admin == 0,
	      "invalid initial team cannot reach client team arrays or authority");
	badTeam.Stop(); g_active.pop_back(); g_logs[2] = nullptr;
	for (int attempt = 0; attempt < 6; ++attempt)
	{
		ClientLog rejected;
		g_active.push_back(&rejected);
		CHECK(StartClient(rejected, 2, port), "repeated rejected peer connection initiated");
		CHECK(PumpUntil([&] { return rejected.accepted; }), "repeated rejected peer connected");
		Join(rejected, "BadTeam", MPVERSION, 99);
		CHECK(PumpUntil([&] { return rejected.closed; }), "repeated invalid admission is closed");
		rejected.Stop(); g_active.pop_back(); g_logs[2] = nullptr;
	}
	CHECK(ja2server_test_transport_count() == 2,
	      "server-rejected transports retire without growing the live tracker");

	// A connected but unadmitted peer cannot invoke any session authority.
	ClientLog rogue;
	g_active.push_back(&rogue);
	CHECK(StartClient(rogue, 3, port), "unadmitted peer connection initiated");
	CHECK(PumpUntil([&] { return rogue.accepted; }), "unadmitted peer connected");
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

	// Keep four admitted peers through lobby/placement so the live combat checks
	// can exercise wiped-team and multi-peer barrier/interrupt authority.
	ClientLog c, d;
	g_active.push_back(&c);
	CHECK(StartClient(c, 2, port), "client C connection initiated");
	CHECK(PumpUntil([&] { return c.accepted; }), "client C connected");
	Join(c, "Carol", MPVERSION, 0);
	CHECK(PumpUntil([&] { return !c.settings.empty(); }) && c.settings.back().client_num == 3,
	      "client C admitted in stable slot 3");
	g_active.push_back(&d);
	CHECK(StartClient(d, 3, port), "client D connection initiated");
	CHECK(PumpUntil([&] { return d.accepted; }), "client D connected");
	Join(d, "Dave", MPVERSION, 2);
	CHECK(PumpUntil([&] { return !d.settings.empty(); }) && d.settings.back().client_num == 4,
	      "client D admitted in stable slot 4");

	// Admin ownership and idempotent, sender-authored ready barrier.
	Send(b, "adminCmd", start); PumpFor(100);
	CHECK(ReadyStage(a, 36) == 0, "non-admin cannot unlock the lobby");
	Send(a, "adminCmd", start);
	CHECK(PumpUntil([&] { return ReadyStage(a, 36) == 1 && ReadyStage(b, 36) == 1; }),
	      "admin unlock reaches both admitted clients");
	// Tactical authority remains inert until the coordinator has emitted stage 4.
	sc_struct prematureCombat = { 6 }, prematureWipe = { 6 };
	turn_struct prematureTurn = { 6, 0 };
	real_struct prematureReal = { 6 };
	death_struct prematureDeath = { 10, 20, 1, 1 };
	Send(a, "startCOMBAT", prematureCombat); Send(a, "sendEndTurn", prematureTurn);
	Send(a, "sendWIPE", prematureWipe); Send(a, "sendREAL", prematureReal);
	Send(a, "sendDEATH", prematureDeath); PumpFor(100);
	CHECK(a.turns.empty() && b.turns.empty() && b.deaths.empty() &&
	      a.gameovers == 0 && b.realtime == 0,
	      "combat, wipe, realtime, and scoring authority are inert before tactical stage 4");
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
	PumpFor(100);
	CHECK(ReadyStage(a, 1) == 0,
	      "two ready votes cannot cross a four-participant barrier");
	ready_struct cReady = { 1, true, 0 }, dReady = { 1, true, 0 };
	Send(c, "sendREADY", cReady); Send(d, "sendREADY", dReady);
	CHECK(PumpUntil([&] { return ReadyStage(a, 1) == 1 && ReadyStage(b, 1) == 1 &&
	                             ReadyStage(c, 1) == 1 && ReadyStage(d, 1) == 1; }),
	      "four distinct admitted senders cross the ready barrier");

	ClientLog late;
	g_active.push_back(&late);
	CHECK(StartClient(late, 3, port), "late peer connection initiated");
	CHECK(PumpUntil([&] { return late.closed; }), "late join is rejected after lobby lock");
	CHECK(late.settings.empty(), "late join never enters the roster");
	late.Stop(); g_active.pop_back(); g_logs[3] = &d;

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
	ready_struct loadedC = { 1, true, 1 }, loadedD = { 1, true, 1 };
	Send(b, "sendGUI", loadedB); Send(c, "sendGUI", loadedC); PumpFor(100);
	CHECK(GuiStage(a, 2) == 0,
	      "three loaded votes cannot cross a four-participant barrier");
	Send(d, "sendGUI", loadedD);
	CHECK(PumpUntil([&] { return GuiStage(a, 2) == 1 && GuiStage(b, 2) == 1 &&
	                             GuiStage(c, 2) == 1 && GuiStage(d, 2) == 1; }),
	      "four distinct loaded clients unlock placement");
	ready_struct placedA = { 3, true, 3 };
	Send(a, "sendGUI", placedA); Send(a, "sendGUI", placedA); PumpFor(100);
	CHECK(GuiStage(a, 4) == 0, "duplicate placed vote cannot enter tactical");
	ready_struct placedB = { 1, true, 3 };
	ready_struct placedC = { 1, true, 3 }, placedD = { 1, true, 3 };
	Send(b, "sendGUI", placedB); Send(c, "sendGUI", placedC); PumpFor(100);
	CHECK(GuiStage(a, 4) == 0,
	      "three placed votes cannot cross a four-participant barrier");
	Send(d, "sendGUI", placedD);
	CHECK(PumpUntil([&] { return GuiStage(a, 4) == 1 && GuiStage(b, 4) == 1 &&
	                             GuiStage(c, 4) == 1 && GuiStage(d, 4) == 1; }),
	      "four distinct placed clients enter tactical");
	CHECK(!std::strcmp(ja2server_test_phase(), "tactical"),
	      "dashboard reports tactical after stage 4");
	const int stage4Count = GuiStage(a, 4);
	ready_struct retractAfterTactical = { 4, false, 3 };
	Send(d, "sendGUI", retractAfterTactical); Send(d, "sendGUI", loadedD); PumpFor(100);
	CHECK(GuiStage(a, 4) == stage4Count,
	      "GUI votes are immutable after the tactical completion latch");
	edge.newedge = MP_EDGE_NORTH; Send(b, "sendEDGECHANGE", edge); PumpFor(100);
	CHECK(a.edges.size() == 1, "roster changes are rejected after lobby lock");

	// A current-team wipe during an interrupt must author a resume before the
	// turn handoff and discard queued requests whose old-turn premise is stale.
	unsigned char handoffInt7[10], handoffInt8[10], handoffInt9[10];
	CHECK(EncodeInterruptWire(handoffInt7, 1, 7, 1, 2) &&
	      EncodeInterruptWire(handoffInt8, 1, 8, 1, 2) &&
	      EncodeInterruptWire(handoffInt9, 1, 9, 1, 2),
	      "shared production interrupt encoder builds handoff fixtures");
	sc_struct team9 = { 9 };
	Send(d, "startCOMBAT", team9);
	CHECK(PumpUntil([&] { return !a.turns.empty() && a.turns.back().tsubNextTeam == 9; }),
	      "slot-4 tactical contact starts on its active transport team");
	SendRaw(b, "sendINTERRUPT", handoffInt7, sizeof(handoffInt7));
	CHECK(PumpUntil([&] { return a.interrupts == 1 && b.interrupts == 1 && c.interrupts == 1; }),
	      "interrupt is granted before the current-team wipe");
	SendRaw(b, "sendINTERRUPT", handoffInt7, sizeof(handoffInt7));
	SendRaw(c, "sendINTERRUPT", handoffInt8, sizeof(handoffInt8)); PumpFor(100);
	CHECK(a.interrupts == 1,
	      "active holder retry is ignored while a distinct requester queues once");
	sc_struct wipe9 = { 9 };
	Send(d, "sendWIPE", wipe9);
	CHECK(PumpUntil([&] { return a.resumes == 1 && a.turns.back().tsubNextTeam == 6; }),
	      "current-team wipe resumes the active interrupt before handing off the turn");
	PumpFor(100);
	CHECK(a.interrupts == 1 && b.interrupts == 1 && c.interrupts == 1,
	      "turn handoff discards queued old-turn interrupt requests instead of chaining them");
	const size_t authorityTurns = a.turns.size();
	const int authorityInterrupts = a.interrupts;
	turn_struct end9 = { 9, 0 };
	Send(d, "startCOMBAT", team9); Send(d, "sendEndTurn", end9);
	SendRaw(d, "sendINTERRUPT", handoffInt9, sizeof(handoffInt9)); PumpFor(100);
	CHECK(a.turns.size() == authorityTurns && a.interrupts == authorityInterrupts,
	      "wiped-but-connected sender cannot start, advance, or interrupt combat");

	// Realtime votes count active tactical teams only. With A/B voted, C's
	// disconnect removes the sole active non-voter; wiped D cannot hold the barrier.
	real_struct rt6 = { 6 }, rt7 = { 7 };
	Send(a, "sendREAL", rt6); Send(b, "sendREAL", rt7); PumpFor(100);
	CHECK(a.realtime == 0 && b.realtime == 0,
	      "realtime waits for the remaining active non-voter");
	c.peer->CloseConnection(c.server, true);
	CHECK(PumpUntil([&] { return a.realtime == 1 && b.realtime == 1; }),
	      "active non-voter disconnect re-evaluates and completes realtime transition");
	CHECK(!std::strcmp(ja2server_test_phase(), "tactical"),
	      "dashboard returns to tactical after realtime transition");
	c.Stop(); g_active.erase(g_active.begin() + 2); g_logs[2] = nullptr;
	a.turns.clear(); b.turns.clear();
	a.interrupts = b.interrupts = 0;
	a.resumes = b.resumes = 0;

	death_struct death = { 10, 20, 1, 1 };
	Send(b, "sendDEATH", death); PumpFor(100);
	CHECK(a.deaths.empty(), "client cannot report another slot as its dead soldier");
	death.soldier_team = 2; death.attacker_team = 5;
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

	unsigned char int6[10], int7[10], release6[10], release7[10];
	CHECK(EncodeInterruptWire(int6, 1, 6, 1, 2) &&
	      EncodeInterruptWire(int7, 1, 7, 1, 2) &&
	      EncodeInterruptWire(release6, 1, 6, 1, COORDINATOR_INT_WIRE_ORDER_ENTRIES) &&
	      EncodeInterruptWire(release7, 1, 7, 1, COORDINATOR_INT_WIRE_ORDER_ENTRIES),
	      "shared production serializer emits request and NOBODY release wires");
	CHECK(MpInterruptWire::Validate(int7, sizeof(int7), false) &&
	      MpInterruptWire::Validate(release7, sizeof(release7), true) &&
	      !MpInterruptWire::Validate(release7, sizeof(release7), false),
	      "coordinator accepts NOBODY only for the release-only Interrupted field");
	CHECK(InterruptWireTeam(int7) == 7, "interrupt fixture carries team byte at wire offset 2");
	unsigned char countMismatch[10]; std::memcpy(countMismatch, int7, sizeof(countMismatch));
	countMismatch[3] = 1;
	SendRaw(b, "sendINTERRUPT", countMismatch, sizeof(countMismatch));
	std::vector<unsigned char> trailing(int7, int7 + sizeof(int7)); trailing.push_back(0);
	SendRaw(b, "sendINTERRUPT", trailing.data(), trailing.size());
	unsigned char invalidActor[10]; std::memcpy(invalidActor, int7, sizeof(invalidActor)); invalidActor[0] = invalidActor[1] = 0xff;
	unsigned char invalidMarker[10]; std::memcpy(invalidMarker, int7, sizeof(invalidMarker)); invalidMarker[5] = 2;
	unsigned char invalidInterrupted[10]; std::memcpy(invalidInterrupted, int7, sizeof(invalidInterrupted)); invalidInterrupted[6] = invalidInterrupted[7] = 0xff;
	unsigned char invalidOrder[10]; std::memcpy(invalidOrder, int7, sizeof(invalidOrder)); invalidOrder[8] = invalidOrder[9] = 0xff;
	SendRaw(b, "sendINTERRUPT", invalidActor, sizeof(invalidActor));
	SendRaw(b, "sendINTERRUPT", invalidMarker, sizeof(invalidMarker));
	SendRaw(b, "sendINTERRUPT", invalidInterrupted, sizeof(invalidInterrupted));
	SendRaw(b, "sendINTERRUPT", invalidOrder, sizeof(invalidOrder));
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
	      "shape, bool, actor, interrupted, order-ID, count, and size violations are rejected");
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
	SendRaw(a, "endINTERRUPT", release6, sizeof(release6)); PumpFor(100);
	CHECK(a.resumes == 0 && b.resumes == 0, "non-holder cannot release an interrupt");
	SendRaw(b, "endINTERRUPT", release7, sizeof(release7));
	CHECK(PumpUntil([&] { return a.resumes == 1 && b.resumes == 1 &&
	                              a.interrupts == 2 && b.interrupts == 2; }),
	      "holder release resumes the turn and chains one queued sender");
	SendRaw(a, "endINTERRUPT", release6, sizeof(release6));
	CHECK(PumpUntil([&] { return a.resumes == 2 && b.resumes == 2; }),
	      "queued sender releases the chained interrupt");
	PumpFor(100);
	CHECK(a.interrupts == 2 && b.interrupts == 2,
	      "duplicate queued requests cannot create additional grants");

	const int beforeGameover = a.gameovers;
	const std::size_t beforeDisconnects = a.disconnects.size();
	b.peer->CloseConnection(b.server, true);
	CHECK(PumpUntil([&] { return a.disconnects.size() > beforeDisconnects; }),
	      "remaining client receives slot disconnect");
	CHECK(a.disconnects.back() == 2, "disconnect notice identifies slot 2");
	CHECK(PumpUntil([&] { return a.gameovers == beforeGameover + 1; }),
	      "coordinator, not a client, publishes last-standing game-over");
	b.Stop();
	d.Stop(); g_logs[3] = nullptr;
	g_active.clear(); g_active.push_back(&a);
	a.peer->CloseConnection(a.server, true); PumpFor(50); a.Stop();
	g_active.clear();

	if (serverResult.load() == -999) std::raise(SIGTERM);
	serverThread.join();
	CHECK(serverResult.load() == 0, "coordinator shuts down cleanly after loopback session");
	std::remove(coopIni.c_str()); std::remove(dmIni.c_str());

	std::printf(g_failures ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", g_failures);
	return g_failures ? 1 : 0;
}
