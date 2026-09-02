// Data-free end-to-end test for the real standalone coordinator pump.

#include <atomic>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "CoordinatorProtocol.h"
#include "LegacyServerIngress.h"
#include "SdlNetTransport.h"

using namespace ja2::mp;
using namespace ja2::mp::net;

int ja2server_test_main(int argc, char** argv);
bool ja2server_test_dashboard_bind_resolves(const char* host);
const char* ja2server_test_dashboard_html();
const char* ja2server_test_phase();
unsigned int ja2server_test_request_reset();
bool ja2server_test_reset_complete(unsigned int generation);
std::size_t ja2server_test_transport_count();
int ja2server_test_clamp_max_mercs(int value);
std::size_t ja2server_test_explosive_ledger_count();
std::size_t ja2server_test_shared_explosive_claim_count();
std::uint64_t ja2server_test_interrupt_elapsed_milliseconds(
	std::uint64_t now, std::uint64_t& grantedMs);

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
	std::vector<std::vector<unsigned char> > interruptPayloads;
	int resumes = 0;
	std::vector<std::vector<unsigned char> > resumePayloads;
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
	std::vector<std::array<unsigned char, 7> > hires;
	std::vector<std::array<unsigned char, 2> > dismisses;
	std::vector<std::array<unsigned char, LegacyGrenadePayloadBytes> > grenades;
	int ai = 0;
	int paths = 0;
	int fires = 0;
	int stops = 0;
	std::vector<UINT16> stoppedActors;
	int fireWeapons = 0;
	int actorCommands = 0;
	int shotOutcomes = 0;
	int immediateProjectiles = 0;
	int delayedEffects = 0;
	int heals = 0;
	int chats = 0;

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

template <std::size_t Bytes>
static void CaptureWire(std::vector<std::array<unsigned char, Bytes> >& out, SdlNetMessage* p)
{
	if (p->size != Bytes) return;
	std::array<unsigned char, Bytes> value = {};
	std::memcpy(value.data(), p->data, value.size());
	out.push_back(value);
}

static void CaptureBytes(
	std::vector<std::vector<unsigned char> >& out, SdlNetMessage* p)
{
	if (!p || !p->data) return;
	out.emplace_back(p->data, p->data + p->size);
}

static void CaptureActorAt(
	std::vector<UINT16>& out, SdlNetMessage* p, std::size_t offset)
{
	if (!p || !p->data || offset + sizeof(UINT16) > p->size) return;
	UINT16 actor = 0;
	std::memcpy(&actor, p->data + offset, sizeof(actor));
	out.push_back(actor);
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
	static void tag##_hire(SdlNetMessage* p) { CaptureWire<7>(g_logs[index]->hires, p); } \
	static void tag##_dismiss(SdlNetMessage* p) { CaptureWire<2>(g_logs[index]->dismisses, p); } \
	static void tag##_admin(SdlNetMessage*) { g_logs[index]->admin++; } \
	static void tag##_ai(SdlNetMessage*) { g_logs[index]->ai++; } \
	static void tag##_path(SdlNetMessage*) { g_logs[index]->paths++; g_logs[index]->actorCommands++; } \
	static void tag##_stance(SdlNetMessage*) { g_logs[index]->actorCommands++; } \
	static void tag##_dir(SdlNetMessage*) { g_logs[index]->actorCommands++; } \
	static void tag##_fire(SdlNetMessage*) { g_logs[index]->fires++; g_logs[index]->actorCommands++; } \
	static void tag##_stop(SdlNetMessage* p) { g_logs[index]->stops++; CaptureActorAt(g_logs[index]->stoppedActors, p, 8); g_logs[index]->actorCommands++; } \
	static void tag##_state(SdlNetMessage*) { g_logs[index]->actorCommands++; } \
	static void tag##_update(SdlNetMessage*) { g_logs[index]->actorCommands++; } \
	static void tag##_firew(SdlNetMessage*) { g_logs[index]->fireWeapons++; g_logs[index]->actorCommands++; } \
	static void tag##_door(SdlNetMessage*) { g_logs[index]->actorCommands++; } \
	static void tag##_hit(SdlNetMessage*) { g_logs[index]->shotOutcomes++; } \
	static void tag##_miss(SdlNetMessage*) { g_logs[index]->shotOutcomes++; } \
	static void tag##_structure(SdlNetMessage*) { g_logs[index]->shotOutcomes++; } \
	static void tag##_window(SdlNetMessage*) { g_logs[index]->shotOutcomes++; } \
	static void tag##_bullet(SdlNetMessage*) { g_logs[index]->immediateProjectiles++; } \
	static void tag##_grenade(SdlNetMessage* p) { CaptureWire<LegacyGrenadePayloadBytes>(g_logs[index]->grenades, p); g_logs[index]->immediateProjectiles++; } \
	static void tag##_plant(SdlNetMessage*) { g_logs[index]->immediateProjectiles++; } \
	static void tag##_detonate(SdlNetMessage*) { g_logs[index]->immediateProjectiles++; } \
	static void tag##_disarm(SdlNetMessage*) { g_logs[index]->immediateProjectiles++; } \
	static void tag##_grenade_result(SdlNetMessage*) { g_logs[index]->delayedEffects++; } \
	static void tag##_spread(SdlNetMessage*) { g_logs[index]->delayedEffects++; } \
	static void tag##_smoke(SdlNetMessage*) { g_logs[index]->delayedEffects++; } \
	static void tag##_explosion_damage(SdlNetMessage*) { g_logs[index]->delayedEffects++; } \
	static void tag##_heal(SdlNetMessage*) { g_logs[index]->heals++; } \
	static void tag##_chat(SdlNetMessage*) { g_logs[index]->chats++; } \
	static void tag##_interrupt(SdlNetMessage* p) { g_logs[index]->interrupts++; CaptureBytes(g_logs[index]->interruptPayloads, p); } \
	static void tag##_resume(SdlNetMessage* p) { g_logs[index]->resumes++; CaptureBytes(g_logs[index]->resumePayloads, p); } \
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
static RpcHandler HIRE[4] = { c0_hire, c1_hire, c2_hire, c3_hire };
static RpcHandler DISMISS[4] = { c0_dismiss, c1_dismiss, c2_dismiss, c3_dismiss };
static RpcHandler ADMIN[4] = { c0_admin, c1_admin, c2_admin, c3_admin };
static RpcHandler AI[4] = { c0_ai, c1_ai, c2_ai, c3_ai };
static RpcHandler PATH[4] = { c0_path, c1_path, c2_path, c3_path };
static RpcHandler STANCE[4] = { c0_stance, c1_stance, c2_stance, c3_stance };
static RpcHandler DIR[4] = { c0_dir, c1_dir, c2_dir, c3_dir };
static RpcHandler FIRE[4] = { c0_fire, c1_fire, c2_fire, c3_fire };
static RpcHandler STOP[4] = { c0_stop, c1_stop, c2_stop, c3_stop };
static RpcHandler STATE[4] = { c0_state, c1_state, c2_state, c3_state };
static RpcHandler UPDATE[4] = { c0_update, c1_update, c2_update, c3_update };
static RpcHandler FIREW[4] = { c0_firew, c1_firew, c2_firew, c3_firew };
static RpcHandler DOOR[4] = { c0_door, c1_door, c2_door, c3_door };
static RpcHandler HIT[4] = { c0_hit, c1_hit, c2_hit, c3_hit };
static RpcHandler MISS[4] = { c0_miss, c1_miss, c2_miss, c3_miss };
static RpcHandler STRUCTURE[4] = { c0_structure, c1_structure, c2_structure, c3_structure };
static RpcHandler WINDOW[4] = { c0_window, c1_window, c2_window, c3_window };
static RpcHandler BULLET[4] = { c0_bullet, c1_bullet, c2_bullet, c3_bullet };
static RpcHandler GRENADE[4] = { c0_grenade, c1_grenade, c2_grenade, c3_grenade };
static RpcHandler PLANT[4] = { c0_plant, c1_plant, c2_plant, c3_plant };
static RpcHandler DETONATE[4] = { c0_detonate, c1_detonate, c2_detonate, c3_detonate };
static RpcHandler DISARM[4] = { c0_disarm, c1_disarm, c2_disarm, c3_disarm };
static RpcHandler GRENADE_RESULT[4] = { c0_grenade_result, c1_grenade_result, c2_grenade_result, c3_grenade_result };
static RpcHandler SPREAD[4] = { c0_spread, c1_spread, c2_spread, c3_spread };
static RpcHandler SMOKE[4] = { c0_smoke, c1_smoke, c2_smoke, c3_smoke };
static RpcHandler EXPLOSION_DAMAGE[4] = { c0_explosion_damage, c1_explosion_damage, c2_explosion_damage, c3_explosion_damage };
static RpcHandler HEAL[4] = { c0_heal, c1_heal, c2_heal, c3_heal };
static RpcHandler CHAT[4] = { c0_chat, c1_chat, c2_chat, c3_chat };
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
	c.peer->RegisterMessage("recieveHIRE", HIRE[index]);
	c.peer->RegisterMessage("recieveDISMISS", DISMISS[index]);
	c.peer->RegisterMessage("recieveADMIN", ADMIN[index]);
	c.peer->RegisterMessage("recieveAI", AI[index]);
	c.peer->RegisterMessage("recievePATH", PATH[index]);
	c.peer->RegisterMessage("recieveSTANCE", STANCE[index]);
	c.peer->RegisterMessage("recieveDIR", DIR[index]);
	c.peer->RegisterMessage("recieveFIRE", FIRE[index]);
	c.peer->RegisterMessage("recieveSTOP", STOP[index]);
	c.peer->RegisterMessage("recieveSTATE", STATE[index]);
	c.peer->RegisterMessage("UpdateSoldierFromNetwork", UPDATE[index]);
	c.peer->RegisterMessage("recieve_fireweapon", FIREW[index]);
	c.peer->RegisterMessage("recieve_door", DOOR[index]);
	c.peer->RegisterMessage("recieveHIT", HIT[index]);
	c.peer->RegisterMessage("recieveMISS", MISS[index]);
	c.peer->RegisterMessage("recievehitSTRUCT", STRUCTURE[index]);
	c.peer->RegisterMessage("recievehitWINDOW", WINDOW[index]);
	c.peer->RegisterMessage("recieveBULLET", BULLET[index]);
	c.peer->RegisterMessage("recieveGRENADE", GRENADE[index]);
	c.peer->RegisterMessage("recievePLANTEXPLOSIVE", PLANT[index]);
	c.peer->RegisterMessage("recieveDETONATEEXPLOSIVE", DETONATE[index]);
	c.peer->RegisterMessage("recieveDISARMEXPLOSIVE", DISARM[index]);
	c.peer->RegisterMessage("recieveGRENADERESULT", GRENADE_RESULT[index]);
	c.peer->RegisterMessage("recieveSPREADEFFECT", SPREAD[index]);
	c.peer->RegisterMessage("recieveNEWSMOKEEFFECT", SMOKE[index]);
	c.peer->RegisterMessage("recieveEXPLOSIONDAMAGE", EXPLOSION_DAMAGE[index]);
	c.peer->RegisterMessage("recieve_heal", HEAL[index]);
	c.peer->RegisterMessage("recieveCHATMSG", CHAT[index]);
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

static unsigned char InterruptWireTeam(const unsigned char* wire) { return wire[2]; }
static std::array<unsigned char, 7> HireWire(
	unsigned char profile, int alliance = 99,
	unsigned char copyItems = 1, unsigned char tacticalTeam = 9)
{
	std::array<unsigned char, 7> wire = {};
	wire[0] = profile;
	std::memcpy(wire.data() + 1, &alliance, sizeof(alliance));
	wire[5] = copyItems;
	wire[6] = tacticalTeam;
	return wire;
}

struct ActorRelayFixture
{
	const char* rpc;
	std::size_t bytes;
	std::size_t actorOffset;
};

static std::vector<unsigned char> ActorWire(
	const ActorRelayFixture& fixture, UINT16 actor, int extraBytes = 0)
{
	std::vector<unsigned char> wire(fixture.bytes + extraBytes, 0);
	if (wire.size() >= fixture.actorOffset + sizeof(actor))
		std::memcpy(wire.data() + fixture.actorOffset, &actor, sizeof(actor));
	// EV_S_STOP_MERC ends with its one-byte fset field. Grid/x/y/direction zero
	// is a coherent world-origin stop; mark it as an applied position update.
	if (!std::strcmp(fixture.rpc, "sendSTOP") && wire.size() >= fixture.bytes)
		wire[15] = 1;
	return wire;
}
static bool EncodeInterruptRequestWire(
	unsigned char (&wire)[14], UINT16 actor, UINT8 team,
	UINT8 markOccurred, UINT16 interrupted)
{
	const UINT16 order[3] = { 255, interrupted, actor };
	return MpInterruptWire::Encode(
		wire, sizeof(wire), actor, team, 2, markOccurred, interrupted, order) == sizeof(wire);
}

static bool EncodeInterruptReleaseWire(
	unsigned char (&wire)[12], UINT16 actor, UINT8 team,
	UINT8 markOccurred)
{
	const UINT16 order[2] = { 255, actor };
	return MpInterruptWire::Encode(
		wire, sizeof(wire), actor, team, 1, markOccurred,
		COORDINATOR_INT_WIRE_ORDER_ENTRIES, order) == sizeof(wire);
}

int main(int argc, char** argv)
{
	std::uint64_t interruptGrantMs = 1000;
	CHECK(ja2server_test_interrupt_elapsed_milliseconds(
	          999, interruptGrantMs) == 0 &&
	      interruptGrantMs == 999 &&
	      ja2server_test_interrupt_elapsed_milliseconds(
	          10999, interruptGrantMs) == 10000 &&
	      ja2server_test_interrupt_elapsed_milliseconds(
	          11000, interruptGrantMs) == 10001 &&
	      ja2server_test_interrupt_elapsed_milliseconds(
	          30998, interruptGrantMs) == 29999 &&
	      ja2server_test_interrupt_elapsed_milliseconds(
	          30999, interruptGrantMs) == 30000,
	      "interrupt timing rebases clock rollback and preserves exact duration boundaries");
	CHECK(ja2server_test_clamp_max_mercs(0) == 1 &&
	      ja2server_test_clamp_max_mercs(1) == 1 &&
	      ja2server_test_clamp_max_mercs(7) == 7 &&
	      ja2server_test_clamp_max_mercs(99) == 7,
	      "standalone merc cap is clamped to the seven-ID LAN wire window");
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
		CHECK(dashboard && std::strstr(dashboard, "name=maxMercs type=number min=1 max=7"),
		      "dashboard advertises the same seven-merc wire limit as admission settings");
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
		  << "\nGAME_TYPE = 1\nMAX_PLAYERS = 4\nMAX_MERCS = 99\nSAME_MERC_ALLOWED = 0"
		  << "\nDASHBOARD_PORT = 0\nLOG_LEVEL = 1\n";
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
		const unsigned int resetGeneration = ja2server_test_request_reset();
		CHECK(PumpUntil([&] {
			return ja2server_test_reset_complete(resetGeneration) && orphan.closed;
		}),
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
			const unsigned int resetGeneration = ja2server_test_request_reset();
			CHECK(PumpUntil([&] {
				return ja2server_test_reset_complete(resetGeneration) && replacement.closed;
			}),
			      "replacement cleanup reset closes its transport");
			replacement.Stop();
			g_active.clear(); g_logs[0] = nullptr;
		}
		else
		{
			const unsigned int resetGeneration = ja2server_test_request_reset();
			CHECK(PumpUntil([&] {
				return ja2server_test_reset_complete(resetGeneration);
			}), "barrier cleanup reset completes before the next session");
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
	CHECK(a.settings.back().client_num == 1 && a.settings.back().gameType == MP_TYPE_TEAMDEATMATCH &&
	      a.settings.back().maxMercs == 7 && a.settings.back().sameMercAllowed == 0,
	      "slot 1 receives PvP settings with MAX_MERCS clamped to the wire capacity");

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

	// A rejected request from an already admitted transport must retire the
	// application roster immediately; SDL3_net does not generate a local disconnect
	// event for the server's own close.
	{
		ClientLog renamed;
		g_active.push_back(&renamed);
		CHECK(StartClient(renamed, 2, port), "duplicate-identity peer connection initiated");
		CHECK(PumpUntil([&] { return renamed.accepted; }), "duplicate-identity peer connected");
		Join(renamed, "Original", MPVERSION, 0);
		CHECK(PumpUntil([&] { return !renamed.settings.empty(); }) &&
		      renamed.settings.back().client_num == 3,
		      "duplicate-identity peer first occupies slot 3");
		const std::size_t disconnectsBefore = a.disconnects.size();
		client_info changedIdentity = {};
		std::strncpy(changedIdentity.client_name, "ReplacementIdentity",
		             sizeof(changedIdentity.client_name) - 1);
		std::strncpy(changedIdentity.client_version, MPVERSION,
		             sizeof(changedIdentity.client_version) - 1);
		changedIdentity.team = 0;
		Send(renamed, "requestSETTINGS", changedIdentity);
		CHECK(PumpUntil([&] { return renamed.closed && a.disconnects.size() > disconnectsBefore; }),
		      "server close retires an admitted identity-changing request immediately");
		CHECK(!a.disconnects.empty() && a.disconnects.back() == 3,
		      "application-aware close publishes and frees the rejected slot");
		renamed.Stop(); g_active.pop_back(); g_logs[2] = nullptr;
	}

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
	const int aChats = a.chats, bChats = b.chats, bPaths = b.paths;
	SendRaw(a, "sendCHATMSG", "x", 1);
	SendRaw(a, "sendPATH", "x", 1);
	PumpFor(100);
	CHECK(a.chats == aChats && b.chats == bChats,
	      "short chat frame is rejected instead of being blindly broadcast");
	std::array<unsigned char, 1026> chatWire = {};
	chatWire[1] = 1;
	chatWire[2] = 'x';
	SendRaw(a, "sendCHATMSG", chatWire.data(), chatWire.size());
	CHECK(PumpUntil([&] { return a.chats == aChats + 1 && b.chats == bChats + 1; }),
	      "exact canonical chat still reaches admitted recipients");
	PumpFor(100);
	CHECK(b.paths == bPaths,
	      "short actor-command frame is rejected instead of being blindly relayed");
	CHECK(rogue.chats == 0 && rogue.paths == 0,
	      "unadmitted socket cannot eavesdrop on broadcast egress");
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
	const std::array<unsigned char, 7> preUnlockHire = HireWire(10);
	SendRaw(b, "sendHIRE", preUnlockHire.data(), preUnlockHire.size());
	std::array<unsigned char, 474> aiWire = {};
	SendRaw(b, "sendAI", aiWire.data(), aiWire.size());
	PumpFor(100);
	CHECK(a.hires.empty(), "hire relay is inert before the coordinator unlocks hiring");
	CHECK(a.ai == 0, "standalone coordinator never relays client-authored AI creation");

	// Admin ownership and idempotent, sender-authored ready barrier.
	Send(b, "adminCmd", start); PumpFor(100);
	CHECK(ReadyStage(a, 36) == 0, "non-admin cannot unlock the lobby");
	Send(a, "adminCmd", start);
	CHECK(PumpUntil([&] { return ReadyStage(a, 36) == 1 && ReadyStage(b, 36) == 1; }),
	      "admin unlock reaches both admitted clients");

	// A reliable duplicate admission can arrive after the lock. It replays settings
	// only to its existing slot and must not disconnect or perturb other participants.
	const std::size_t aSettingsBeforeLateReplay = a.settings.size();
	const std::size_t bSettingsBeforeLateReplay = b.settings.size();
	Join(a, "Alice", MPVERSION, 0);
	CHECK(PumpUntil([&] { return a.settings.size() == aSettingsBeforeLateReplay + 1; }) &&
	      !a.closed && a.settings.back().client_num == 1,
	      "late duplicate SETTINGS is an idempotent canonical replay, not a disconnect");
	PumpFor(100);
	CHECK(b.settings.size() == bSettingsBeforeLateReplay,
	      "duplicate SETTINGS replay is private to the already admitted sender");

	// HIRE/DISMISS are exact, hiring-phase operations tied to the admitted sender.
	// The server authors both selected alliance and tactical team, reserves the same
	// first-free seven-ID slot that remote clients create, and enforces that cap.
	std::array<unsigned char, 7> malformedHire = HireWire(10);
	SendRaw(b, "sendHIRE", malformedHire.data(), malformedHire.size() - 1);
	std::array<unsigned char, 8> trailingHire = {};
	std::memcpy(trailingHire.data(), malformedHire.data(), malformedHire.size());
	SendRaw(b, "sendHIRE", trailingHire.data(), trailingHire.size());
	std::array<unsigned char, 7> badProfile = HireWire(255);
	std::array<unsigned char, 7> badCopyFlag = HireWire(10, 99, 2);
	SendRaw(b, "sendHIRE", badProfile.data(), badProfile.size());
	SendRaw(b, "sendHIRE", badCopyFlag.data(), badCopyFlag.size());
	PumpFor(100);
	CHECK(a.hires.empty(), "short, trailing, invalid-profile, and invalid-bool HIRE frames are rejected");
	for (int hireNumber = 0; hireNumber < 7; ++hireNumber)
	{
		const std::array<unsigned char, 7> hire = HireWire(10);
		SendRaw(b, "sendHIRE", hire.data(), hire.size());
	}
	CHECK(PumpUntil([&] { return a.hires.size() == 7; }),
	      "seven exact HIRE frames fill the complete slot-2 wire window without an unacknowledged global profile rejection");
	int authoredAlliance = -1;
	if (!a.hires.empty())
		std::memcpy(&authoredAlliance, a.hires.front().data() + 1, sizeof(authoredAlliance));
	CHECK(!a.hires.empty() && authoredAlliance == 2 && a.hires.front()[6] == 7,
	      "HIRE alliance and tactical team are authored from slot-2 coordinator state");
	const std::array<unsigned char, 7> overCapHire = HireWire(17);
	SendRaw(b, "sendHIRE", overCapHire.data(), overCapHire.size()); PumpFor(100);
	CHECK(a.hires.size() == 7, "eighth HIRE cannot exceed the seven-ID team window");

	UINT16 foreignActor = 120;
	SendRaw(b, "sendDISMISS", &foreignActor, sizeof(foreignActor));
	unsigned char trailingDismiss[3] = { 127, 0, 0 };
	SendRaw(b, "sendDISMISS", trailingDismiss, sizeof(trailingDismiss)); PumpFor(100);
	CHECK(a.dismisses.empty(), "foreign and trailing-byte DISMISS frames are rejected");
	UINT16 ownedActor = 127;
	SendRaw(b, "sendDISMISS", &ownedActor, sizeof(ownedActor));
	CHECK(PumpUntil([&] { return a.dismisses.size() == 1; }),
	      "DISMISS relays a currently tracked actor owned by the sender");
	UINT16 dismissedActor = 0;
	if (!a.dismisses.empty())
		std::memcpy(&dismissedActor, a.dismisses.back().data(), sizeof(dismissedActor));
	CHECK(dismissedActor == ownedActor,
	      "DISMISS preserves the canonical tracked actor ID");
	SendRaw(b, "sendDISMISS", &ownedActor, sizeof(ownedActor)); PumpFor(100);
	CHECK(a.dismisses.size() == 1, "duplicate DISMISS cannot release an actor twice");
	SendRaw(b, "sendHIRE", overCapHire.data(), overCapHire.size());
	CHECK(PumpUntil([&] { return a.hires.size() == 8; }),
	      "a valid dismissal releases one deterministic HIRE slot");
	SendRaw(b, "sendAI", aiWire.data(), aiWire.size()); PumpFor(100);
	CHECK(a.ai == 0, "AI creation remains disabled during standalone hiring");
	const std::array<unsigned char, 7> dHire = HireWire(30);
	SendRaw(d, "sendHIRE", dHire.data(), dHire.size());
	CHECK(PumpUntil([&] { return a.hires.size() == 9; }) &&
	      a.hires.back()[6] == 9,
	      "slot-4 hire reserves its canonical actor window for tactical authorization");
	const std::array<unsigned char, 7> dSecondHire = HireWire(33);
	SendRaw(d, "sendHIRE", dSecondHire.data(), dSecondHire.size());
	CHECK(PumpUntil([&] { return a.hires.size() == 10; }) &&
	      a.hires.back()[6] == 9,
	      "slot-4 reserves a second actor for exact paused-actor arbitration");
	const std::size_t bHiresBeforeAlice = b.hires.size();
	const std::array<unsigned char, 7> aHire = HireWire(31);
	SendRaw(a, "sendHIRE", aHire.data(), aHire.size());
	CHECK(PumpUntil([&] { return b.hires.size() == bHiresBeforeAlice + 1; }) &&
	      b.hires.back()[6] == 6,
	      "slot-1 hire provides a tracked enemy actor for alliance authorization tests");
	const std::array<unsigned char, 7> aSecondHire = HireWire(34);
	SendRaw(a, "sendHIRE", aSecondHire.data(), aSecondHire.size());
	CHECK(PumpUntil([&] { return b.hires.size() == bHiresBeforeAlice + 2; }) &&
	      b.hires.back()[6] == 6,
	      "slot-1 reserves a second tracked actor for exact continuation binding");
	const std::array<unsigned char, 7> cHire = HireWire(32);
	SendRaw(c, "sendHIRE", cHire.data(), cHire.size());
	CHECK(PumpUntil([&] { return a.hires.size() == 11; }) &&
	      a.hires.back()[6] == 8,
	      "slot-3 hire provides a tracked actor for mixed interrupt arbitration");

	// Tactical authority remains inert until the coordinator has emitted stage 4.
	sc_struct prematureCombat = { 6 }, prematureWipe = { 6 };
	turn_struct prematureTurn = { 6, 0 };
	real_struct prematureReal = { 6 };
	death_struct prematureDeath = { 10, 20, 1, 1 };
	const ActorRelayFixture prematurePathFixture = { "sendPATH", 76, 0 };
	const std::vector<unsigned char> prematurePath =
		ActorWire(prematurePathFixture, 127);
	Send(a, "startCOMBAT", prematureCombat); Send(a, "sendEndTurn", prematureTurn);
	Send(a, "sendWIPE", prematureWipe); Send(a, "sendREAL", prematureReal);
	Send(a, "sendDEATH", prematureDeath);
	SendRaw(b, "sendPATH", prematurePath.data(), prematurePath.size());
	PumpFor(100);
	CHECK(a.turns.empty() && b.turns.empty() && b.deaths.empty() &&
	      a.gameovers == 0 && b.realtime == 0 && a.actorCommands == 0,
	      "combat, actor commands, wipe, realtime, and scoring authority are inert before tactical stage 4");
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
	const std::array<unsigned char, 7> readyHire = HireWire(18);
	SendRaw(b, "sendHIRE", readyHire.data(), readyHire.size());
	SendRaw(b, "sendDISMISS", &ownedActor, sizeof(ownedActor)); PumpFor(100);
	CHECK(a.hires.size() == 11 && a.dismisses.size() == 1,
	      "a ready participant cannot mutate its frozen hiring roster");
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

	// Straightforward actor-authored tactical frames have fixed legacy layouts.
	// Exercise the real coordinator pump for every layout: short/trailing frames and
	// another slot's ID must be inert, while the tracked first actor for Bob relays.
	const ActorRelayFixture actorRelays[] = {
		{ "sendPATH", 76, 0 },
		{ "sendSTANCE", 12, 4 },
		{ "sendDIR", 8, 4 },
		{ "sendFIRE", 12, 8 },
		{ "sendSTOP", 16, 8 },
		{ "sendSTATE", 20, 8 },
		{ "updatenetworksoldier", 16, 4 },
		{ "sendFIREW", 12, 8 },
		{ "sendDOOR", 12, 0 }
	};
	const int actorCommandsBefore = a.actorCommands;
	for (const ActorRelayFixture& fixture : actorRelays)
	{
		const std::vector<unsigned char> foreign = ActorWire(fixture, 120);
		const std::vector<unsigned char> trailing = ActorWire(fixture, 127, 1);
		SendRaw(b, fixture.rpc, foreign.data(), foreign.size());
		SendRaw(b, fixture.rpc, trailing.data(), trailing.size());
		SendRaw(b, fixture.rpc, trailing.data(), fixture.bytes - 1);
	}
	PumpFor(120);
	CHECK(a.actorCommands == actorCommandsBefore,
	      "short, trailing, and foreign-ID tactical command frames are rejected");
	for (const ActorRelayFixture& fixture : actorRelays)
	{
		const std::vector<unsigned char> owned = ActorWire(fixture, 127);
		SendRaw(b, fixture.rpc, owned.data(), owned.size());
	}
	CHECK(PumpUntil([&] {
		return a.actorCommands == actorCommandsBefore +
		       (int)(sizeof(actorRelays) / sizeof(actorRelays[0]));
	}), "all exact tactical commands relay only for a currently hired sender-owned actor");

	// Shot outcomes are also exact and attacker-bound, but are intentionally a
	// separate class: the coordinator retains them as delayed results even if the
	// firing team is marked wiped before transport delivery.
	const ActorRelayFixture shotRelays[] = {
		{ "sendHIT", 32, 10 },
		{ "sendMISS", 8, 4 },
		{ "sendhitSTRUCT", 24, 18 },
		{ "sendhitWINDOW", 16, 12 }
	};
	const int shotOutcomesBefore = a.shotOutcomes;
	for (const ActorRelayFixture& fixture : shotRelays)
	{
		const std::vector<unsigned char> foreign = ActorWire(fixture, 120);
		const std::vector<unsigned char> trailing = ActorWire(fixture, 127, 1);
		SendRaw(b, fixture.rpc, foreign.data(), foreign.size());
		SendRaw(b, fixture.rpc, trailing.data(), trailing.size());
		SendRaw(b, fixture.rpc, trailing.data(), fixture.bytes - 1);
	}
	PumpFor(120);
	CHECK(a.shotOutcomes == shotOutcomesBefore,
	      "malformed and foreign-attacker shot outcomes cannot alter or relay scoring");
	for (const ActorRelayFixture& fixture : shotRelays)
	{
		const std::vector<unsigned char> owned = ActorWire(fixture, 127);
		SendRaw(b, fixture.rpc, owned.data(), owned.size());
	}
	CHECK(PumpUntil([&] {
		return a.shotOutcomes == shotOutcomesBefore +
		       (int)(sizeof(shotRelays) / sizeof(shotRelays[0]));
	}), "exact shot outcomes require the sender's tracked attacker ID");

	// Projectile and explosive initiators are tactical-only, exact-layout, and
	// tied to a currently hired actor owned by the admitted sender.
	const ActorRelayFixture projectileInitiators[] = {
		{ "sendBULLET", LegacyBulletPayloadBytes, LegacyBulletFirerOffset },
		{ "sendGRENADE", LegacyGrenadePayloadBytes, LegacyGrenadeActorOffset },
		{ "sendPLANTEXPLOSIVE", LegacyPlantExplosivePayloadBytes,
		  LegacyPlantExplosiveActorOffset },
		{ "sendDETONATEEXPLOSIVE", LegacyDetonateExplosivePayloadBytes,
		  LegacyDetonateExplosiveActorOffset },
		{ "sendDISARMEXPLOSIVE", LegacyDisarmExplosivePayloadBytes,
		  LegacyDisarmExplosiveActorOffset }
	};
	auto ProjectileWire = [&](const ActorRelayFixture& fixture, UINT16 actor,
	                         int extraBytes = 0) {
		std::vector<unsigned char> wire = ActorWire(fixture, actor, extraBytes);
		const UINT32 grid = 123;
		const UINT32 worldIndex = 73;
		if (!std::strcmp(fixture.rpc, "sendPLANTEXPLOSIVE"))
		{
			const UINT16 item = 1;
			std::memcpy(wire.data() + LegacyPlantExplosiveGridOffset,
			            &grid, sizeof(grid));
			std::memcpy(wire.data() + LegacyPlantExplosiveWorldIndexOffset,
			            &worldIndex, sizeof(worldIndex));
			std::memcpy(wire.data() + LegacyPlantExplosiveItemOffset,
			            &item, sizeof(item));
			wire[LegacyPlantExplosiveStatusOffset] = 99;
			wire[LegacyPlantExplosiveLevelOffset] = 0;
			wire[LegacyPlantExplosiveDetonatorOffset] = 1;
		}
		if (!std::strcmp(fixture.rpc, "sendDETONATEEXPLOSIVE"))
		{
			wire[LegacyDetonateExplosiveTeamOffset] = 7;
			std::memcpy(wire.data() + LegacyDetonateExplosiveWorldIndexOffset,
			            &worldIndex, sizeof(worldIndex));
		}
		if (!std::strcmp(fixture.rpc, "sendDISARMEXPLOSIVE"))
		{
			wire[LegacyDisarmExplosiveTeamOffset] = 7;
			std::memcpy(wire.data() + LegacyDisarmExplosiveWorldIndexOffset,
			            &worldIndex, sizeof(worldIndex));
			std::memcpy(wire.data() + LegacyDisarmExplosiveGridOffset,
			            &grid, sizeof(grid));
		}
		return wire;
	};
	auto SharedDetonationWire = [&](UINT16 actor, UINT32 worldIndex,
	                                UINT8 originTeam = 0) {
		std::vector<unsigned char> wire =
			ProjectileWire(projectileInitiators[3], actor);
		wire[LegacyDetonateExplosiveTeamOffset] = originTeam;
		std::memcpy(wire.data() + LegacyDetonateExplosiveWorldIndexOffset,
		            &worldIndex, sizeof(worldIndex));
		return wire;
	};
	auto SharedDisarmWire = [&](UINT16 actor, UINT32 worldIndex,
	                            UINT32 grid = 321, UINT8 originTeam = 0) {
		std::vector<unsigned char> wire =
			ProjectileWire(projectileInitiators[4], actor);
		wire[LegacyDisarmExplosiveTeamOffset] = originTeam;
		std::memcpy(wire.data() + LegacyDisarmExplosiveWorldIndexOffset,
		            &worldIndex, sizeof(worldIndex));
		std::memcpy(wire.data() + LegacyDisarmExplosiveGridOffset,
		            &grid, sizeof(grid));
		return wire;
	};

	// The planted-bomb ledger and the pre-placed map-bomb namespace share the
	// engine's bounded world-item domain. Out-of-domain PLANT frames must be
	// rejected before they can reserve a ledger key.
	CHECK(ja2server_test_explosive_ledger_count() == 0,
	      "standalone explosive ledger starts empty for the tactical session");
	const int boundedPlantBefore = a.immediateProjectiles;
	const std::size_t ledgerBeforeBounds =
		ja2server_test_explosive_ledger_count();
	std::vector<unsigned char> boundedPlant =
		ProjectileWire(projectileInitiators[2], 127);
	const UINT32 worldIndexAtLimit = LegacySharedExplosiveWorldIndexLimit;
	std::memcpy(boundedPlant.data() + LegacyPlantExplosiveWorldIndexOffset,
	            &worldIndexAtLimit, sizeof(worldIndexAtLimit));
	SendRaw(b, "sendPLANTEXPLOSIVE", boundedPlant.data(), boundedPlant.size());
	const UINT32 maximumWorldIndex = static_cast<UINT32>(-1);
	std::memcpy(boundedPlant.data() + LegacyPlantExplosiveWorldIndexOffset,
	            &maximumWorldIndex, sizeof(maximumWorldIndex));
	SendRaw(b, "sendPLANTEXPLOSIVE", boundedPlant.data(), boundedPlant.size());
	PumpFor(100);
	CHECK(a.immediateProjectiles == boundedPlantBefore &&
	      ja2server_test_explosive_ledger_count() == ledgerBeforeBounds,
	      "PLANT rejects the world-index limit and UINT32_MAX without ledger mutation");
	const UINT32 largestValidWorldIndex =
		LegacySharedExplosiveWorldIndexLimit - 1;
	std::memcpy(boundedPlant.data() + LegacyPlantExplosiveWorldIndexOffset,
	            &largestValidWorldIndex, sizeof(largestValidWorldIndex));
	SendRaw(b, "sendPLANTEXPLOSIVE", boundedPlant.data(), boundedPlant.size());
	CHECK(PumpUntil([&] {
		return a.immediateProjectiles == boundedPlantBefore + 1;
	}) && ja2server_test_explosive_ledger_count() == ledgerBeforeBounds + 1,
	      "PLANT accepts the largest in-domain world index after rejected bounds");
	std::vector<unsigned char> boundedPlantDetonation =
		SharedDetonationWire(127, largestValidWorldIndex, 7);
	SendRaw(b, "sendDETONATEEXPLOSIVE", boundedPlantDetonation.data(),
	        boundedPlantDetonation.size());
	CHECK(PumpUntil([&] {
		return a.immediateProjectiles == boundedPlantBefore + 2;
	}) && ja2server_test_explosive_ledger_count() == ledgerBeforeBounds,
	      "the in-domain planted-bomb key retains the existing one-shot ledger path");

	// Origin team zero identifies a pre-placed map bomb, for which no PLANT frame
	// exists. DETONATE and DISARM share one permanent session tombstone so neither
	// a same-operation retry nor the opposite operation can replay the map item.
	CHECK(ja2server_test_shared_explosive_claim_count() == 0,
	      "shared map-bomb claim set starts empty for the tactical session");
	const int sharedBefore = a.immediateProjectiles;
	const UINT32 detonateFirstIndex = 3100000;
	std::vector<unsigned char> sharedDetonation =
		SharedDetonationWire(127, detonateFirstIndex);
	SendRaw(b, "sendDETONATEEXPLOSIVE", sharedDetonation.data(),
	        sharedDetonation.size());
	CHECK(PumpUntil([&] {
		return a.immediateProjectiles == sharedBefore + 1;
	}) && ja2server_test_shared_explosive_claim_count() == 1,
	      "first origin-zero map-bomb detonation claims and relays its shared index");
	SendRaw(b, "sendDETONATEEXPLOSIVE", sharedDetonation.data(),
	        sharedDetonation.size());
	std::vector<unsigned char> detonateThenDisarm =
		SharedDisarmWire(127, detonateFirstIndex);
	SendRaw(b, "sendDISARMEXPLOSIVE", detonateThenDisarm.data(),
	        detonateThenDisarm.size());
	PumpFor(100);
	CHECK(a.immediateProjectiles == sharedBefore + 1 &&
	      ja2server_test_shared_explosive_claim_count() == 1,
	      "same-operation replay and DETONATE-to-DISARM replay share one tombstone");

	const UINT32 disarmFirstIndex = 3100001;
	std::vector<unsigned char> sharedDisarm =
		SharedDisarmWire(127, disarmFirstIndex);
	SendRaw(b, "sendDISARMEXPLOSIVE", sharedDisarm.data(), sharedDisarm.size());
	CHECK(PumpUntil([&] {
		return a.immediateProjectiles == sharedBefore + 2;
	}) && ja2server_test_shared_explosive_claim_count() == 2,
	      "first origin-zero map-bomb disarm claims and relays its shared index");
	SendRaw(b, "sendDISARMEXPLOSIVE", sharedDisarm.data(), sharedDisarm.size());
	std::vector<unsigned char> disarmThenDetonate =
		SharedDetonationWire(127, disarmFirstIndex);
	SendRaw(b, "sendDETONATEEXPLOSIVE", disarmThenDetonate.data(),
	        disarmThenDetonate.size());
	PumpFor(100);
	CHECK(a.immediateProjectiles == sharedBefore + 2 &&
	      ja2server_test_shared_explosive_claim_count() == 2,
	      "same-operation replay and DISARM-to-DETONATE replay share one tombstone");

	std::vector<unsigned char> sharedLimitDetonation =
		SharedDetonationWire(127, worldIndexAtLimit);
	std::vector<unsigned char> sharedMaximumDetonation =
		SharedDetonationWire(127, maximumWorldIndex);
	std::vector<unsigned char> sharedLimitDisarm =
		SharedDisarmWire(127, worldIndexAtLimit);
	std::vector<unsigned char> sharedMaximumDisarm =
		SharedDisarmWire(127, maximumWorldIndex);
	SendRaw(b, "sendDETONATEEXPLOSIVE", sharedLimitDetonation.data(),
	        sharedLimitDetonation.size());
	SendRaw(b, "sendDETONATEEXPLOSIVE", sharedMaximumDetonation.data(),
	        sharedMaximumDetonation.size());
	SendRaw(b, "sendDISARMEXPLOSIVE", sharedLimitDisarm.data(),
	        sharedLimitDisarm.size());
	SendRaw(b, "sendDISARMEXPLOSIVE", sharedMaximumDisarm.data(),
	        sharedMaximumDisarm.size());
	const UINT32 rejectedOriginDetonationIndex = 3100002;
	const UINT32 rejectedOriginDisarmIndex = 3100003;
	std::vector<unsigned char> rejectedOriginDetonation =
		SharedDetonationWire(127, rejectedOriginDetonationIndex, 1);
	std::vector<unsigned char> rejectedOriginDisarm =
		SharedDisarmWire(127, rejectedOriginDisarmIndex, 321, 5);
	SendRaw(b, "sendDETONATEEXPLOSIVE", rejectedOriginDetonation.data(),
	        rejectedOriginDetonation.size());
	SendRaw(b, "sendDISARMEXPLOSIVE", rejectedOriginDisarm.data(),
	        rejectedOriginDisarm.size());
	PumpFor(100);
	CHECK(a.immediateProjectiles == sharedBefore + 2 &&
	      ja2server_test_shared_explosive_claim_count() == 2,
	      "origin-zero bounds and reserved origin teams 1 through 5 reject without claims");

	const UINT32 unauthorizedDetonationIndex = 3100004;
	const UINT32 unauthorizedDisarmIndex = 3100005;
	std::vector<unsigned char> unauthorizedDetonation =
		SharedDetonationWire(120, unauthorizedDetonationIndex);
	std::vector<unsigned char> unauthorizedDisarm =
		SharedDisarmWire(120, unauthorizedDisarmIndex);
	SendRaw(b, "sendDETONATEEXPLOSIVE", unauthorizedDetonation.data(),
	        unauthorizedDetonation.size());
	SendRaw(b, "sendDISARMEXPLOSIVE", unauthorizedDisarm.data(),
	        unauthorizedDisarm.size());
	PumpFor(100);
	CHECK(a.immediateProjectiles == sharedBefore + 2 &&
	      ja2server_test_shared_explosive_claim_count() == 2,
	      "foreign actor cannot poison either shared map-bomb operation");
	std::vector<unsigned char> authorizedDetonation =
		SharedDetonationWire(127, unauthorizedDetonationIndex);
	std::vector<unsigned char> authorizedDisarm =
		SharedDisarmWire(127, unauthorizedDisarmIndex);
	SendRaw(b, "sendDETONATEEXPLOSIVE", authorizedDetonation.data(),
	        authorizedDetonation.size());
	SendRaw(b, "sendDISARMEXPLOSIVE", authorizedDisarm.data(),
	        authorizedDisarm.size());
	CHECK(PumpUntil([&] {
		return a.immediateProjectiles == sharedBefore + 4;
	}) && ja2server_test_shared_explosive_claim_count() == 4,
	      "owned actors can claim the same shared indices after rejected spoof attempts");

	const UINT32 sharedDisconnectWorldIndex = 3100006;
	std::vector<unsigned char> disconnectClaim =
		SharedDetonationWire(134, sharedDisconnectWorldIndex);
	SendRaw(c, "sendDETONATEEXPLOSIVE", disconnectClaim.data(),
	        disconnectClaim.size());
	CHECK(PumpUntil([&] {
		return a.immediateProjectiles == sharedBefore + 5;
	}) && ja2server_test_shared_explosive_claim_count() == 5,
	      "a second participant can claim a shared map bomb before disconnect");

	const int immediateBefore = a.immediateProjectiles;
	for (const ActorRelayFixture& fixture : projectileInitiators)
	{
		const std::vector<unsigned char> foreign = ProjectileWire(fixture, 120);
		const std::vector<unsigned char> trailing = ProjectileWire(fixture, 127, 1);
		SendRaw(b, fixture.rpc, foreign.data(), foreign.size());
		SendRaw(b, fixture.rpc, trailing.data(), trailing.size());
		SendRaw(b, fixture.rpc, trailing.data(), fixture.bytes - 1);
	}
	PumpFor(120);
	CHECK(a.immediateProjectiles == immediateBefore,
	      "malformed and foreign-actor projectile/explosive initiators are rejected");
	std::vector<unsigned char> zeroItemPlant =
		ProjectileWire(projectileInitiators[2], 127);
	const UINT16 zeroItem = 0;
	std::memcpy(zeroItemPlant.data() + LegacyPlantExplosiveItemOffset,
	            &zeroItem, sizeof(zeroItem));
	SendRaw(b, "sendPLANTEXPLOSIVE", zeroItemPlant.data(),
	        zeroItemPlant.size());
	PumpFor(100);
	CHECK(a.immediateProjectiles == immediateBefore,
	      "zero-item plant cannot reserve or relay a standalone bomb key");
	// A plant establishes the team/index namespace. Detonation/disarm without a
	// matching live ledger entry must remain inert even for an owned actor.
	for (std::size_t index = 0; index < 3; ++index)
	{
		const ActorRelayFixture& fixture = projectileInitiators[index];
		const std::vector<unsigned char> owned = ProjectileWire(fixture, 127);
		SendRaw(b, fixture.rpc, owned.data(), owned.size());
	}
	CHECK(PumpUntil([&] {
		return a.immediateProjectiles == immediateBefore + 3;
	}), "exact projectile initiators and a valid plant require a sender-owned hired actor");
	std::vector<unsigned char> detonation =
		ProjectileWire(projectileInitiators[3], 127);
	detonation[LegacyDetonateExplosiveTeamOffset] = 8;
	SendRaw(b, "sendDETONATEEXPLOSIVE", detonation.data(), detonation.size());
	PumpFor(100);
	CHECK(a.immediateProjectiles == immediateBefore + 3,
	      "a guessed explosive team namespace cannot detonate a tracked bomb");
	detonation[LegacyDetonateExplosiveTeamOffset] = 7;
	SendRaw(b, "sendDETONATEEXPLOSIVE", detonation.data(), detonation.size());
	CHECK(PumpUntil([&] { return a.immediateProjectiles == immediateBefore + 4; }),
	      "an exact tracked explosive key can be consumed once by detonation");
	SendRaw(b, "sendDETONATEEXPLOSIVE", detonation.data(), detonation.size());
	PumpFor(100);
	CHECK(a.immediateProjectiles == immediateBefore + 4,
	      "a consumed explosive key rejects detonation replay");

	std::vector<unsigned char> secondPlant =
		ProjectileWire(projectileInitiators[2], 127);
	const UINT32 secondGrid = 124;
	const UINT32 secondWorldIndex = 74;
	std::memcpy(secondPlant.data() + LegacyPlantExplosiveGridOffset,
	            &secondGrid, sizeof(secondGrid));
	std::memcpy(secondPlant.data() + LegacyPlantExplosiveWorldIndexOffset,
	            &secondWorldIndex, sizeof(secondWorldIndex));
	SendRaw(b, "sendPLANTEXPLOSIVE", secondPlant.data(), secondPlant.size());
	CHECK(PumpUntil([&] { return a.immediateProjectiles == immediateBefore + 5; }),
	      "a second valid plant establishes an independent explosive key");
	std::vector<unsigned char> disarm =
		ProjectileWire(projectileInitiators[4], 127);
	std::memcpy(disarm.data() + LegacyDisarmExplosiveWorldIndexOffset,
	            &secondWorldIndex, sizeof(secondWorldIndex));
	const UINT32 wrongGrid = secondGrid + 1;
	std::memcpy(disarm.data() + LegacyDisarmExplosiveGridOffset,
	            &wrongGrid, sizeof(wrongGrid));
	SendRaw(b, "sendDISARMEXPLOSIVE", disarm.data(), disarm.size());
	PumpFor(100);
	CHECK(a.immediateProjectiles == immediateBefore + 5,
	      "disarm cannot consume a tracked bomb at a forged grid");
	std::memcpy(disarm.data() + LegacyDisarmExplosiveGridOffset,
	            &secondGrid, sizeof(secondGrid));
	SendRaw(b, "sendDISARMEXPLOSIVE", disarm.data(), disarm.size());
	CHECK(PumpUntil([&] { return a.immediateProjectiles == immediateBefore + 6; }),
	      "a valid disarm consumes the exact tracked bomb once");
	SendRaw(b, "sendDISARMEXPLOSIVE", disarm.data(), disarm.size());
	PumpFor(100);
	CHECK(a.immediateProjectiles == immediateBefore + 6,
	      "a consumed disarm key rejects replay");

	// Traps use canonical NOBODY (1284) instead of a firer. Preserve that legacy
	// actorless case while retaining tactical-stage and non-wiped sender checks.
	const std::vector<unsigned char> actorlessBullet =
		ProjectileWire(projectileInitiators[0], COORDINATOR_INT_WIRE_ORDER_ENTRIES);
	SendRaw(b, "sendBULLET", actorlessBullet.data(), actorlessBullet.size());
	CHECK(PumpUntil([&] {
		return a.immediateProjectiles == immediateBefore + 7;
	}), "canonical actorless trap bullet remains compatible");

	// Catch actions historically carried a process-local team-0 target. The
	// coordinator rewrites that target to the sender's tracked LAN actor ID.
	std::vector<unsigned char> catchGrenade =
		ProjectileWire(projectileInitiators[1], 127);
	catchGrenade[LegacyGrenadeActionCodeOffset] = 2;
	const UINT32 localCatchTarget = 0;
	std::memcpy(catchGrenade.data() + LegacyGrenadeActionDataOffset,
	            &localCatchTarget, sizeof(localCatchTarget));
	SendRaw(b, "sendGRENADE", catchGrenade.data(), catchGrenade.size());
	CHECK(PumpUntil([&] {
		return a.immediateProjectiles == immediateBefore + 8;
	}), "legacy local catch target is accepted only through canonical ownership mapping");
	UINT32 canonicalCatchTarget = 0;
	if (!a.grenades.empty())
		std::memcpy(&canonicalCatchTarget,
		            a.grenades.back().data() + LegacyGrenadeActionDataOffset,
		            sizeof(canonicalCatchTarget));
	CHECK(canonicalCatchTarget == 127,
	      "grenade catch target is rewritten to the sender's canonical LAN actor");
	const int afterCanonicalCatch = a.immediateProjectiles;
	const UINT32 enemyCatchTarget = 120;
	std::memcpy(catchGrenade.data() + LegacyGrenadeActionDataOffset,
	            &enemyCatchTarget, sizeof(enemyCatchTarget));
	SendRaw(b, "sendGRENADE", catchGrenade.data(), catchGrenade.size());
	catchGrenade[LegacyGrenadeActionCodeOffset] = 3;
	SendRaw(b, "sendGRENADE", catchGrenade.data(), catchGrenade.size());
	PumpFor(100);
	CHECK(a.immediateProjectiles == afterCanonicalCatch,
	      "foreign catch targets and unknown grenade action codes are rejected");

	// Keep a timed bomb armed across the later team-wipe transition. Its
	// detonation is a delayed consequence authenticated by the plant ledger,
	// not a new action by the then-wiped planter.
	std::vector<unsigned char> timedPlant =
		ProjectileWire(projectileInitiators[2], 141);
	const UINT32 timedGrid = 125;
	const UINT32 timedWorldIndex = 75;
	std::memcpy(timedPlant.data() + LegacyPlantExplosiveGridOffset,
	            &timedGrid, sizeof(timedGrid));
	std::memcpy(timedPlant.data() + LegacyPlantExplosiveWorldIndexOffset,
	            &timedWorldIndex, sizeof(timedWorldIndex));
	SendRaw(d, "sendPLANTEXPLOSIVE", timedPlant.data(), timedPlant.size());
	CHECK(PumpUntil([&] {
		return a.immediateProjectiles == afterCanonicalCatch + 1;
	}), "a live planter can arm a bomb for delayed post-wipe detonation");
	std::vector<unsigned char> timedDetonation =
		ProjectileWire(projectileInitiators[3], 141);
	timedDetonation[LegacyDetonateExplosiveTeamOffset] = 9;
	std::memcpy(timedDetonation.data() +
	            LegacyDetonateExplosiveWorldIndexOffset,
	            &timedWorldIndex, sizeof(timedWorldIndex));

	// Grenade completion, smoke/spread, and victim-authored explosion damage can
	// arrive after the owner is wiped, but still require exact layouts and an
	// existing sender-owned actor/victim record.
	const ActorRelayFixture delayedEffectRelays[] = {
		{ "sendGRENADERESULT", LegacyGrenadeResultPayloadBytes,
		  LegacyGrenadeResultActorOffset },
		{ "sendSPREADEFFECT", LegacySpreadEffectPayloadBytes,
		  LegacySpreadEffectActorOffset },
		{ "sendNEWSMOKEEFFECT", LegacySpreadEffectPayloadBytes,
		  LegacySpreadEffectActorOffset },
		{ "sendEXPLOSIONDAMAGE", LegacyExplosionDamagePayloadBytes,
		  LegacyExplosionDamageVictimOffset }
	};
	const int delayedBefore = a.delayedEffects;
	for (const ActorRelayFixture& fixture : delayedEffectRelays)
	{
		const std::vector<unsigned char> foreign = ActorWire(fixture, 120);
		const std::vector<unsigned char> trailing = ActorWire(fixture, 127, 1);
		SendRaw(b, fixture.rpc, foreign.data(), foreign.size());
		SendRaw(b, fixture.rpc, trailing.data(), trailing.size());
		SendRaw(b, fixture.rpc, trailing.data(), fixture.bytes - 1);
	}
	PumpFor(120);
	CHECK(a.delayedEffects == delayedBefore,
	      "malformed and foreign-owner delayed effect frames are rejected");
	for (const ActorRelayFixture& fixture : delayedEffectRelays)
	{
		const std::vector<unsigned char> owned = ActorWire(fixture, 127);
		SendRaw(b, fixture.rpc, owned.data(), owned.size());
	}
	CHECK(PumpUntil([&] {
		return a.delayedEffects == delayedBefore +
		       (int)(sizeof(delayedEffectRelays) / sizeof(delayedEffectRelays[0]));
	}), "exact delayed effects bind their owner or victim to the sender registry");

	const ActorRelayFixture healFixture = { "sendHEAL", 4, 0 };
	auto HealWire = [&](UINT16 patient, INT8 life = 50, INT8 bleeding = 2,
	                    int extraBytes = 0) {
		std::vector<unsigned char> wire = ActorWire(healFixture, patient, extraBytes);
		wire[2] = static_cast<unsigned char>(life);
		wire[3] = static_cast<unsigned char>(bleeding);
		return wire;
	};
	const int healsBefore = a.heals;
	const std::vector<unsigned char> enemyHeal = HealWire(120);
	const std::vector<unsigned char> trailingHeal = HealWire(127, 50, 2, 1);
	const std::vector<unsigned char> zeroLifeHeal = HealWire(127, 0, 2);
	const std::vector<unsigned char> negativeBleedHeal = HealWire(127, 50, -1);
	SendRaw(b, "sendHEAL", enemyHeal.data(), enemyHeal.size());
	SendRaw(b, "sendHEAL", trailingHeal.data(), trailingHeal.size());
	SendRaw(b, "sendHEAL", trailingHeal.data(), 3);
	SendRaw(b, "sendHEAL", zeroLifeHeal.data(), zeroLifeHeal.size());
	SendRaw(b, "sendHEAL", negativeBleedHeal.data(), negativeBleedHeal.size());
	PumpFor(100);
	CHECK(a.heals == healsBefore,
	      "enemy, malformed, dead-patient, and invalid-bleeding HEAL frames are rejected");
	const std::vector<unsigned char> ownHeal = HealWire(127);
	const std::vector<unsigned char> alliedHeal = HealWire(141);
	SendRaw(b, "sendHEAL", ownHeal.data(), ownHeal.size());
	SendRaw(b, "sendHEAL", alliedHeal.data(), alliedHeal.size());
	CHECK(PumpUntil([&] { return a.heals == healsBefore + 2; }),
	      "standalone TDM permits exact own-patient and same-alliance HEAL results");

	// A current-team wipe during an interrupt must author a resume before the
	// turn handoff and discard queued requests whose old-turn premise is stale.
	unsigned char handoffInt6[14], retargetedHandoffInt6[14];
	unsigned char handoffInt7[14], handoffInt8[14], handoffInt9[14];
	unsigned char handoffRelease9[12];
	CHECK(EncodeInterruptRequestWire(handoffInt6, 120, 6, 1, 141) &&
	      EncodeInterruptRequestWire(retargetedHandoffInt6, 120, 6, 1, 142) &&
	      EncodeInterruptRequestWire(handoffInt7, 127, 7, 1, 141) &&
	      EncodeInterruptRequestWire(handoffInt8, 134, 8, 1, 141) &&
	      EncodeInterruptRequestWire(handoffInt9, 141, 9, 1, 127),
	      "shared production interrupt encoder builds handoff fixtures");
	CHECK(EncodeInterruptReleaseWire(handoffRelease9, 141, 9, 1),
	      "shared production interrupt encoder builds the evolved handoff release");
	sc_struct team9 = { 9 };
	Send(d, "startCOMBAT", team9);
	CHECK(PumpUntil([&] { return !a.turns.empty() && a.turns.back().tsubNextTeam == 9; }),
	      "slot-4 tactical contact starts on its active transport team");
	SendRaw(b, "sendINTERRUPT", handoffInt7, sizeof(handoffInt7));
	PumpFor(100);
	CHECK(a.interrupts == 0,
	      "team-deathmatch ally cannot seize an interrupt against its teammate");
	SendRaw(c, "sendINTERRUPT", handoffInt8, sizeof(handoffInt8));
	CHECK(PumpUntil([&] { return a.interrupts == 1 && b.interrupts == 1 && c.interrupts == 1; }),
	      "interrupt is granted before the current-team wipe");
	SendRaw(c, "sendINTERRUPT", handoffInt8, sizeof(handoffInt8));
	SendRaw(a, "sendINTERRUPT", retargetedHandoffInt6,
	        sizeof(retargetedHandoffInt6));
	SendRaw(a, "sendINTERRUPT", handoffInt6, sizeof(handoffInt6)); PumpFor(100);
	CHECK(a.interrupts == 1,
	      "active holder retry and a different-actor retarget do not alter the active grant");
	SendRaw(c, "endINTERRUPT", handoffRelease9, sizeof(handoffRelease9));
	CHECK(PumpUntil([&] {
		return a.resumes == 1 && a.interrupts == 2;
	}), "holder release chains a simultaneous interrupt against the original current actor");
	CHECK(a.interruptPayloads.size() >= 2 &&
	      MpInterruptWire::Get16(a.interruptPayloads.back().data(), 6) == 141,
	      "queued request remains bound to the exact paused actor, not merely its team");
	SendRaw(a, "endINTERRUPT", handoffRelease9, sizeof(handoffRelease9));
	CHECK(PumpUntil([&] { return a.resumes == 2; }),
	      "simultaneous requester can release its chained grant");
	SendRaw(c, "sendINTERRUPT", handoffInt8, sizeof(handoffInt8));
	CHECK(PumpUntil([&] { return a.interrupts == 3; }),
	      "a fresh grant remains available for current-team wipe coverage");
	SendRaw(a, "sendINTERRUPT", handoffInt6, sizeof(handoffInt6));
	PumpFor(100);
	CHECK(a.interrupts == 3,
	      "a second simultaneous request remains queued before the handoff");
	sc_struct wipe9 = { 9 };
	Send(d, "sendWIPE", wipe9);
	CHECK(PumpUntil([&] { return a.resumes == 3 && a.turns.back().tsubNextTeam == 6; }),
	      "current-team wipe resumes the active interrupt before handing off the turn");
	const std::vector<unsigned char>& forcedResume = a.resumePayloads.back();
	CHECK(forcedResume.size() == 12 &&
	      MpInterruptWire::Get16(forcedResume.data(), 0) == 141 &&
	      forcedResume[2] == 9 &&
	      MpInterruptWire::Get16(forcedResume.data(), 3) == 1 &&
	      MpInterruptWire::Get16(forcedResume.data(), 6) ==
	          COORDINATOR_INT_WIRE_ORDER_ENTRIES &&
	      MpInterruptWire::Get16(forcedResume.data(), 8) == 255 &&
	      MpInterruptWire::Get16(forcedResume.data(), 10) == 141,
	      "forced release carries the evolved minimal queue instead of replaying the grant");
	PumpFor(100);
	CHECK(a.interrupts == 3 && b.interrupts == 3 && c.interrupts == 3,
	      "turn handoff discards queued old-turn interrupt requests instead of chaining them");
	const int commandsAfterWipe = a.actorCommands;
	const int outcomesAfterWipe = a.shotOutcomes;
	const std::vector<unsigned char> wipedPath = ActorWire(actorRelays[0], 141);
	const std::vector<unsigned char> delayedMiss = ActorWire(shotRelays[1], 141);
	SendRaw(d, actorRelays[0].rpc, wipedPath.data(), wipedPath.size());
	SendRaw(d, shotRelays[1].rpc, delayedMiss.data(), delayedMiss.size());
	CHECK(PumpUntil([&] { return a.shotOutcomes == outcomesAfterWipe + 1; }),
	      "delayed shot outcome from a just-wiped hired actor reaches peers");
	PumpFor(100);
	CHECK(a.actorCommands == commandsAfterWipe,
	      "wiped team cannot issue a new actor command");
	CHECK(a.shotOutcomes == outcomesAfterWipe + 1,
	      "already-authored shot outcome remains deliverable after the firing team is wiped");
	const int immediateAfterWipe = a.immediateProjectiles;
	const int delayedAfterWipe = a.delayedEffects;
	const int healsAfterWipe = a.heals;
	const std::vector<unsigned char> wipedGrenade =
		ProjectileWire(projectileInitiators[1], 141);
	SendRaw(d, "sendGRENADE", wipedGrenade.data(), wipedGrenade.size());
	SendRaw(d, "sendDETONATEEXPLOSIVE",
		timedDetonation.data(), timedDetonation.size());
	SendRaw(d, "sendDETONATEEXPLOSIVE",
		timedDetonation.data(), timedDetonation.size());
	for (const ActorRelayFixture& fixture : delayedEffectRelays)
	{
		const std::vector<unsigned char> delayed = ActorWire(fixture, 141);
		SendRaw(d, fixture.rpc, delayed.data(), delayed.size());
	}
	const std::vector<unsigned char> wipedSenderHeal = HealWire(127);
	const std::vector<unsigned char> wipedPatientHeal = HealWire(141);
	SendRaw(d, "sendHEAL", wipedSenderHeal.data(), wipedSenderHeal.size());
	SendRaw(b, "sendHEAL", wipedPatientHeal.data(), wipedPatientHeal.size());
	CHECK(PumpUntil([&] {
		return a.delayedEffects == delayedAfterWipe +
		       (int)(sizeof(delayedEffectRelays) / sizeof(delayedEffectRelays[0]));
	}), "delayed grenade/effect/damage results survive a sender wipe");
	PumpFor(100);
	CHECK(a.immediateProjectiles == immediateAfterWipe + 1,
	      "wiped team cannot initiate a grenade, while its tracked timed detonation resolves exactly once");
	CHECK(a.heals == healsAfterWipe,
	      "HEAL rejects both a wiped sender and a wiped allied patient");
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
	const int bProjectilesAfterClaimantDisconnect = b.immediateProjectiles;
	std::vector<unsigned char> replayAfterClaimantDisconnect =
		SharedDetonationWire(120, sharedDisconnectWorldIndex);
	SendRaw(a, "sendDETONATEEXPLOSIVE",
	        replayAfterClaimantDisconnect.data(),
	        replayAfterClaimantDisconnect.size());
	PumpFor(100);
	CHECK(b.immediateProjectiles == bProjectilesAfterClaimantDisconnect &&
	      ja2server_test_shared_explosive_claim_count() == 5,
	      "an individual disconnect does not clear a shared map-bomb replay tombstone");
	a.turns.clear(); b.turns.clear();
	a.interrupts = b.interrupts = 0;
	a.resumes = b.resumes = 0;

	death_struct death = { 120, 120, 1, 2 };
	Send(b, "sendDEATH", death); PumpFor(100);
	CHECK(a.deaths.empty(),
	      "client cannot corpse another player's actor while claiming its own score slot");
	death.soldier_id = 127;
	death.attacker_id = 120;
	death.soldier_team = 1;
	Send(b, "sendDEATH", death); PumpFor(100);
	CHECK(a.deaths.empty(), "client cannot report another slot as its dead score team");
	death.soldier_team = 2;
	death.attacker_id = 148;
	death.attacker_team = 5;
	Send(b, "sendDEATH", death); PumpFor(100);
	CHECK(a.deaths.empty(), "client cannot name an actor never hired this session");
	death.attacker_id = 120;
	death.attacker_team = 2;
	Send(b, "sendDEATH", death);
	CHECK(PumpUntil([&] { return a.deaths.size() == 1; }) &&
	      a.deaths.back().soldier_team == 2 && a.deaths.back().attacker_team == 1,
	      "valid death derives victim and attacker score identities from actor provenance");

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
	const int aCommandsAtTeam6 = a.actorCommands;
	const int bCommandsAtTeam6 = b.actorCommands;
	const std::vector<unsigned char> aTurnPath = ActorWire(actorRelays[0], 120);
	const std::vector<unsigned char> bTurnPath = ActorWire(actorRelays[0], 127);
	SendRaw(b, "sendPATH", bTurnPath.data(), bTurnPath.size());
	SendRaw(a, "sendPATH", aTurnPath.data(), aTurnPath.size());
	CHECK(PumpUntil([&] { return b.actorCommands == bCommandsAtTeam6 + 1; }),
	      "current turn owner can issue an immediate tactical command");
	PumpFor(100);
	CHECK(a.actorCommands == aCommandsAtTeam6,
	      "out-of-turn player cannot issue an immediate tactical command");
	const int aActorlessBulletsAtTeam6 = a.immediateProjectiles;
	const int bActorlessBulletsAtTeam6 = b.immediateProjectiles;
	SendRaw(b, "sendBULLET", actorlessBullet.data(), actorlessBullet.size());
	PumpFor(100);
	CHECK(a.immediateProjectiles == aActorlessBulletsAtTeam6,
	      "standalone actorless bullet remains gated to the current transport sender");
	SendRaw(a, "sendBULLET", actorlessBullet.data(), actorlessBullet.size());
	CHECK(PumpUntil([&] {
		return b.immediateProjectiles == bActorlessBulletsAtTeam6 + 1;
	}), "standalone current sender can relay an actorless trap bullet");
	turn_struct end6 = { 6, 0 }, end7 = { 7, 0 };
	size_t turns = a.turns.size();
	Send(b, "sendEndTurn", end6); PumpFor(100);
	CHECK(a.turns.size() == turns, "slot 2 cannot end slot 1's turn");
	std::array<unsigned char, sizeof(turn_struct) + 1> trailingEndTurn = {};
	std::memcpy(trailingEndTurn.data(), &end6, sizeof(end6));
	SendRaw(a, "sendEndTurn", trailingEndTurn.data(), sizeof(end6) - 1);
	SendRaw(a, "sendEndTurn", trailingEndTurn.data(), trailingEndTurn.size());
	PumpFor(100);
	CHECK(a.turns.size() == turns,
	      "short and trailing-byte end-turn frames are rejected exactly");
	Send(a, "sendEndTurn", end6);
	CHECK(PumpUntil([&] { return a.turns.size() == turns + 1; }) && a.turns.back().tsubNextTeam == 7,
	      "valid slot-1 end-turn advances to team 7");
	const int aCommandsAtTeam7 = a.actorCommands;
	const int bCommandsAtTeam7 = b.actorCommands;
	SendRaw(a, "sendPATH", aTurnPath.data(), aTurnPath.size());
	SendRaw(b, "sendPATH", bTurnPath.data(), bTurnPath.size());
	CHECK(PumpUntil([&] { return a.actorCommands == aCommandsAtTeam7 + 1; }),
	      "new current turn owner gains immediate tactical authority");
	PumpFor(100);
	CHECK(b.actorCommands == bCommandsAtTeam7,
	      "previous turn owner loses immediate tactical authority");
	turns = a.turns.size();
	Send(a, "sendEndTurn", end7); PumpFor(100);
	CHECK(a.turns.size() == turns, "slot 1 cannot claim team 7 identity");
	Send(b, "sendEndTurn", end7);
	CHECK(PumpUntil([&] { return a.turns.size() == turns + 1; }) && a.turns.back().tsubNextTeam == 6,
	      "valid slot-2 end-turn returns authority to team 6");

	unsigned char int6[14], int7[14], release6[12];
	CHECK(EncodeInterruptRequestWire(int6, 120, 6, 1, 127) &&
	      EncodeInterruptRequestWire(int7, 127, 7, 1, 120) &&
	      EncodeInterruptReleaseWire(release6, 120, 6, 1),
	      "shared production serializer emits request and NOBODY release wires");
	unsigned char mixedInt7[14] = {};
	const UINT16 mixedOrder[3] = { 255, 120, 127 };
	CHECK(MpInterruptWire::Encode(
	          mixedInt7, sizeof(mixedInt7), 127, 7, 2, 1, 120,
	          mixedOrder) == sizeof(mixedInt7),
	      "production-shaped sentinel and mixed-team interrupt order is encoded");
	CHECK(MpInterruptWire::Validate(int7, sizeof(int7), false) &&
	      MpInterruptWire::Validate(release6, sizeof(release6), true) &&
	      !MpInterruptWire::Validate(release6, sizeof(release6), false),
	      "coordinator accepts NOBODY only for the release-only Interrupted field");
	unsigned char unrelatedInt7[14] = {};
	CHECK(EncodeInterruptRequestWire(unrelatedInt7, 127, 7, 1, 134),
	      "interrupt fixture can name a hostile actor outside the paused team");
	SendRaw(b, "sendINTERRUPT", unrelatedInt7, sizeof(unrelatedInt7));
	unsigned char missingPausedActor[12] = {};
	const UINT16 missingPausedOrder[2] = { 255, 127 };
	CHECK(MpInterruptWire::Encode(
	          missingPausedActor, sizeof(missingPausedActor), 127, 7, 1, 1,
	          120, missingPausedOrder) == sizeof(missingPausedActor),
	      "malicious request fixture omits its claimed paused actor from the queue");
	SendRaw(b, "sendINTERRUPT", missingPausedActor,
	        sizeof(missingPausedActor));
	unsigned char foreignSuffix[16] = {};
	const UINT16 foreignSuffixOrder[4] = { 255, 120, 141, 127 };
	CHECK(MpInterruptWire::Encode(
	          foreignSuffix, sizeof(foreignSuffix), 127, 7, 3, 1, 120,
	          foreignSuffixOrder) == sizeof(foreignSuffix),
	      "malicious request fixture injects another player's actor into the holder suffix");
	SendRaw(b, "sendINTERRUPT", foreignSuffix, sizeof(foreignSuffix));
	CHECK(InterruptWireTeam(int7) == 7, "interrupt fixture carries team byte at wire offset 2");
	unsigned char countMismatch[14]; std::memcpy(countMismatch, int7, sizeof(countMismatch));
	countMismatch[3] = 3;
	SendRaw(b, "sendINTERRUPT", countMismatch, sizeof(countMismatch));
	std::vector<unsigned char> trailing(int7, int7 + sizeof(int7)); trailing.push_back(0);
	SendRaw(b, "sendINTERRUPT", trailing.data(), trailing.size());
	unsigned char invalidActor[14]; std::memcpy(invalidActor, int7, sizeof(invalidActor)); invalidActor[0] = invalidActor[1] = 0xff;
	unsigned char invalidMarker[14]; std::memcpy(invalidMarker, int7, sizeof(invalidMarker)); invalidMarker[5] = 2;
	unsigned char invalidInterrupted[14]; std::memcpy(invalidInterrupted, int7, sizeof(invalidInterrupted)); invalidInterrupted[6] = invalidInterrupted[7] = 0xff;
	unsigned char invalidOrder[14]; std::memcpy(invalidOrder, int7, sizeof(invalidOrder)); invalidOrder[8] = invalidOrder[9] = 0xff;
	SendRaw(b, "sendINTERRUPT", invalidActor, sizeof(invalidActor));
	SendRaw(b, "sendINTERRUPT", invalidMarker, sizeof(invalidMarker));
	SendRaw(b, "sendINTERRUPT", invalidInterrupted, sizeof(invalidInterrupted));
	SendRaw(b, "sendINTERRUPT", invalidOrder, sizeof(invalidOrder));
	std::vector<unsigned char> maxInterrupt(COORDINATOR_INT_WIRE_MAX_BYTES, 0);
	MpInterruptWire::Put16(maxInterrupt.data(), 0, 127);
	maxInterrupt[2] = 7;
	const UINT16 maxPersons = COORDINATOR_INT_WIRE_ORDER_ENTRIES - 1;
	MpInterruptWire::Put16(maxInterrupt.data(), 3, maxPersons);
	MpInterruptWire::Put16(maxInterrupt.data(), 6, 120);
	MpInterruptWire::Put16(
		maxInterrupt.data(), MpInterruptWire::kHeaderBytes, 255);
	MpInterruptWire::Put16(
		maxInterrupt.data(), MpInterruptWire::kHeaderBytes + 2, 120);
	for (std::size_t offset = MpInterruptWire::kHeaderBytes + 4;
	     offset < maxInterrupt.size(); offset += 2)
		MpInterruptWire::Put16(maxInterrupt.data(), offset, 127);
	std::vector<unsigned char> invalidPersons = maxInterrupt;
	invalidPersons[3] = (unsigned char)(COORDINATOR_INT_WIRE_ORDER_ENTRIES & 0xff);
	invalidPersons[4] = (unsigned char)(COORDINATOR_INT_WIRE_ORDER_ENTRIES >> 8);
	std::vector<unsigned char> overInterrupt = maxInterrupt; overInterrupt.push_back(0);
	SendRaw(b, "sendINTERRUPT", invalidPersons.data(), invalidPersons.size());
	SendRaw(b, "sendINTERRUPT", overInterrupt.data(), overInterrupt.size());
	PumpFor(100);
	CHECK(a.interrupts == 0 && b.interrupts == 0,
	      "shape, identity, paused-team, order-ID, count, and size violations are rejected");
	CHECK(MpInterruptWire::Validate(
	          maxInterrupt.data(), maxInterrupt.size(), false),
	      "portable codec accepts its exact maximum bounded wire shape");
	SendRaw(b, "sendINTERRUPT", maxInterrupt.data(), maxInterrupt.size());
	PumpFor(100);
	CHECK(a.interrupts == 0 && b.interrupts == 0,
	      "authority rejects a non-production duplicate-actor maximum queue");
	SendRaw(b, "endINTERRUPT", overInterrupt.data(), overInterrupt.size()); PumpFor(100);
	CHECK(a.resumes == 0 && b.resumes == 0, "over-bound interrupt release is rejected");
	SendRaw(b, "endINTERRUPT", release6, sizeof(release6));
	PumpFor(100);
	CHECK(a.resumes == 0 && b.resumes == 0,
	      "release without an active grant is rejected");
	a.interrupts = b.interrupts = 0;
	a.resumes = b.resumes = 0;
	SendRaw(a, "sendINTERRUPT", int7, sizeof(int7)); PumpFor(100);
	CHECK(a.interrupts == 0 && b.interrupts == 0, "spoofed interrupt identity is rejected");
	// A weapon action is a reliable multi-RPC sequence.  Fully admit FIRE
	// before another connection wins an interrupt, then prove that the paused
	// actor can finish only that already-started FIREW/BULLET sequence. One STOP
	// for the exact paused actor may publish its authoritative final position.
	const ActorRelayFixture fireFixture = { "sendFIRE", 12, 8 };
	const ActorRelayFixture fireWeaponFixture = { "sendFIREW", 12, 8 };
	const ActorRelayFixture stopFixture = { "sendSTOP", 16, 8 };
	const ActorRelayFixture bulletFixture = {
		"sendBULLET", LegacyBulletPayloadBytes, LegacyBulletFirerOffset };
	const std::vector<unsigned char> acceptedFire =
		ActorWire(fireFixture, 120);
	const std::vector<unsigned char> continuedFireWeapon =
		ActorWire(fireWeaponFixture, 120);
	const std::vector<unsigned char> continuedBullet =
		ActorWire(bulletFixture, 120);
	const std::vector<unsigned char> continuedStop =
		ActorWire(stopFixture, 120);
	const std::vector<unsigned char> wrongActorStop =
		ActorWire(stopFixture, 121);
	const int bFiresBeforeInterrupt = b.fires;
	SendRaw(a, "sendFIRE", acceptedFire.data(), acceptedFire.size());
	CHECK(PumpUntil([&] { return b.fires == bFiresBeforeInterrupt + 1; }),
	      "current standalone actor's FIRE is accepted before interrupt arbitration");
	const UINT16 attackStartActor = 120;
	SendRaw(a, "sendATTACKSTART", &attackStartActor,
	        sizeof(attackStartActor));
	PumpFor(100);
	CHECK(b.fires == bFiresBeforeInterrupt + 1,
	      "standalone direct-throw attack marker opens continuation without relaying an animation");
	SendRaw(b, "sendINTERRUPT", mixedInt7, sizeof(mixedInt7));
	CHECK(PumpUntil([&] { return a.interrupts == 1 && b.interrupts == 1; }),
	      "valid slot-2 interrupt is granted to the session");
	const int bCommandsBeforeStop = b.actorCommands;
	const std::vector<unsigned char> pausedPreStopStance =
		ActorWire(actorRelays[1], 120);
	SendRaw(a, "sendPATH", aTurnPath.data(), aTurnPath.size());
	SendRaw(a, "sendSTANCE", pausedPreStopStance.data(),
	        pausedPreStopStance.size());
	CHECK(PumpUntil([&] {
		return b.actorCommands == bCommandsBeforeStop + 2;
	}), "exact paused actor drains ordinary reliable commands queued before STOP");
	const int bFireWeaponsBefore = b.fireWeapons;
	const int bBulletsBefore = b.immediateProjectiles;
	const std::size_t bGrenadesBefore = b.grenades.size();
	const int bStopsBefore = b.stops;
	std::vector<unsigned char> continuedGrenade =
		ProjectileWire(projectileInitiators[1], 120);
	continuedGrenade[LegacyGrenadeActionCodeOffset] = 2;
	const UINT32 continuedCatchTarget = 120;
	std::memcpy(continuedGrenade.data() + LegacyGrenadeActionDataOffset,
	            &continuedCatchTarget, sizeof(continuedCatchTarget));
	SendRaw(a, "sendSTOP", continuedStop.data(), continuedStop.size());
	SendRaw(a, "sendSTOP", continuedStop.data(), continuedStop.size());
	SendRaw(a, "sendSTOP", wrongActorStop.data(), wrongActorStop.size());
	SendRaw(a, "sendFIREW", continuedFireWeapon.data(),
	        continuedFireWeapon.size());
	SendRaw(a, "sendBULLET", continuedBullet.data(), continuedBullet.size());
	SendRaw(a, "sendBULLET", continuedBullet.data(), continuedBullet.size());
	SendRaw(a, "sendFIREW", continuedFireWeapon.data(),
	        continuedFireWeapon.size());
	SendRaw(a, "sendGRENADE", continuedGrenade.data(),
	        continuedGrenade.size());
	const bool standaloneContinuationRelayed = PumpUntil([&] {
		return b.fireWeapons == bFireWeaponsBefore + 2 &&
		       b.immediateProjectiles == bBulletsBefore + 3 &&
		       b.grenades.size() == bGrenadesBefore + 1 &&
		       b.stops == bStopsBefore + 1;
	});
	PumpFor(100);
	CHECK(standaloneContinuationRelayed,
	      "accepted standalone attack keeps bounded multi-frame FIREW/BULLET continuation and one stored-target STOP across a foreign interrupt grant");
	CHECK(b.fireWeapons == bFireWeaponsBefore + 2 &&
	      b.immediateProjectiles == bBulletsBefore + 3 &&
	      b.grenades.size() == bGrenadesBefore + 1 &&
	      b.stops == bStopsBefore + 1 && !b.stoppedActors.empty() &&
	      b.stoppedActors.back() == 120,
	      "standalone continuation accepts burst frames while duplicate and wrong-actor STOP are rejected");
	const int aPathsDuringInterrupt = a.paths;
	const int aFiresDuringInterrupt = a.fires;
	const int aProjectilesDuringInterrupt = a.immediateProjectiles;
	const int bCommandsDuringInterrupt = b.actorCommands;
	const std::vector<unsigned char> pausedStance =
		ActorWire(actorRelays[1], 120);
	const std::vector<unsigned char> listedHolderFire =
		ActorWire(fireFixture, 127);
	const std::vector<unsigned char> unlistedHolderPath =
		ActorWire(actorRelays[0], 128);
	const std::vector<unsigned char> unlistedHolderFire =
		ActorWire(fireFixture, 128);
	std::vector<unsigned char> listedHolderPlant =
		ProjectileWire(projectileInitiators[2], 127);
	std::vector<unsigned char> unlistedHolderPlant =
		ProjectileWire(projectileInitiators[2], 128);
	const UINT32 listedHolderGrid = 701;
	const UINT32 listedHolderWorldIndex = 701;
	const UINT32 unlistedHolderGrid = 702;
	const UINT32 unlistedHolderWorldIndex = 702;
	std::memcpy(listedHolderPlant.data() + LegacyPlantExplosiveGridOffset,
	            &listedHolderGrid, sizeof(listedHolderGrid));
	std::memcpy(listedHolderPlant.data() +
	            LegacyPlantExplosiveWorldIndexOffset,
	            &listedHolderWorldIndex, sizeof(listedHolderWorldIndex));
	std::memcpy(unlistedHolderPlant.data() + LegacyPlantExplosiveGridOffset,
	            &unlistedHolderGrid, sizeof(unlistedHolderGrid));
	std::memcpy(unlistedHolderPlant.data() +
	            LegacyPlantExplosiveWorldIndexOffset,
	            &unlistedHolderWorldIndex, sizeof(unlistedHolderWorldIndex));
	SendRaw(a, "sendPATH", aTurnPath.data(), aTurnPath.size());
	SendRaw(a, "sendSTANCE", pausedStance.data(), pausedStance.size());
	SendRaw(b, "sendPATH", bTurnPath.data(), bTurnPath.size());
	SendRaw(b, "sendFIRE", listedHolderFire.data(), listedHolderFire.size());
	SendRaw(b, "sendPLANTEXPLOSIVE", listedHolderPlant.data(),
	        listedHolderPlant.size());
	SendRaw(b, "sendPATH", unlistedHolderPath.data(),
	        unlistedHolderPath.size());
	SendRaw(b, "sendFIRE", unlistedHolderFire.data(),
	        unlistedHolderFire.size());
	SendRaw(b, "sendPLANTEXPLOSIVE", unlistedHolderPlant.data(),
	        unlistedHolderPlant.size());
	CHECK(PumpUntil([&] {
		return a.paths == aPathsDuringInterrupt + 1 &&
		       a.fires == aFiresDuringInterrupt + 1 &&
		       a.immediateProjectiles == aProjectilesDuringInterrupt + 1;
	}), "only grant-suffix actors gain immediate PATH/FIRE/PLANT authority");
	PumpFor(100);
	CHECK(a.paths == aPathsDuringInterrupt + 1 &&
	      a.fires == aFiresDuringInterrupt + 1 &&
	      a.immediateProjectiles == aProjectilesDuringInterrupt + 1 &&
	      b.actorCommands == bCommandsDuringInterrupt,
	      "post-STOP paused commands and an unlisted same-owner holder actor are rejected");
	SendRaw(a, "sendINTERRUPT", int6, sizeof(int6));
	SendRaw(a, "sendINTERRUPT", int6, sizeof(int6));
	SendRaw(a, "sendINTERRUPT", int6, sizeof(int6)); PumpFor(100);
	CHECK(a.interrupts == 1 && b.interrupts == 1,
	      "unsupported nested-holder interrupt requests do not alter the active grant");
	SendRaw(a, "endINTERRUPT", release6, sizeof(release6)); PumpFor(100);
	CHECK(a.resumes == 0 && b.resumes == 0, "non-holder cannot release an interrupt");
	unsigned char injectedRelease[14] = {};
	const UINT16 injectedReleaseOrder[3] = { 255, 127, 120 };
	CHECK(MpInterruptWire::Encode(
	          injectedRelease, sizeof(injectedRelease), 120, 6, 2, 1,
	          COORDINATOR_INT_WIRE_ORDER_ENTRIES,
	          injectedReleaseOrder) == sizeof(injectedRelease),
	      "malicious evolved release fixture carries an injected known actor");
	SendRaw(b, "endINTERRUPT", injectedRelease, sizeof(injectedRelease));
	PumpFor(100);
	CHECK(a.resumes == 0 && b.resumes == 0,
	      "holder cannot inject unrelated actors into the resume queue");
	SendRaw(b, "endINTERRUPT", release6, sizeof(release6));
	CHECK(PumpUntil([&] { return a.resumes == 1 && b.resumes == 1; }),
	      "holder release authenticates the evolved paused-actor payload and resumes the turn");
	PumpFor(100);
	CHECK(a.interrupts == 1 && b.interrupts == 1,
	      "rejected nested requests cannot create later grants");

	CHECK(ja2server_test_shared_explosive_claim_count() == 5,
	      "shared map-bomb tombstones survive for the complete active session");
	const int beforeGameover = a.gameovers;
	const std::size_t beforeDisconnects = a.disconnects.size();
	b.peer->CloseConnection(b.server, true);
	CHECK(PumpUntil([&] { return a.disconnects.size() > beforeDisconnects; }),
	      "remaining client receives slot disconnect");
	CHECK(a.disconnects.back() == 2, "disconnect notice identifies slot 2");
	CHECK(PumpUntil([&] { return a.gameovers == beforeGameover + 1; }),
	      "coordinator, not a client, publishes last-standing game-over");
	CHECK(ja2server_test_shared_explosive_claim_count() == 0,
	      "full game-state reset clears the shared map-bomb claim set");
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
