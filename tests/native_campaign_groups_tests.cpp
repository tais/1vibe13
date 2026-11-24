// Real native group/actor directories; data-free linked-list fixture. Capture
// must not allocate, mutate paths, advance clocks, or inspect the enemy union.
#include "DedicatedCoopCampaignGroups.h"
#include "GameContext.h"
#include "Game Clock.h"
#include "StrategicGroupHost.h"
#include "Strategic Movement.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "TacticalEntityHost.h"
#include "Overhead.h"
#include <cstdio>
#include <cstdlib>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : ""); std::exit(1);
}
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
}
int main()
{
	using namespace CoopSession;
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(), "native fixture starts");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots(); ResetJa2TacticalActorRosters();
	CHECK(gpGroupList == nullptr, "native groups start empty");
	if (failures) return 1;
	CoopCampaignGroups captured;
	CHECK(!CaptureDedicatedCoopCampaignGroups(captured) && captured.available && !captured.groupCount, "empty established campaign is available");
	GROUP friendly{}, enemy{}, empty{};
	friendly.ubGroupID = 20; friendly.usGroupTeam = OUR_TEAM; friendly.ubGroupSize = 2;
	friendly.ubSectorX = 9; friendly.ubSectorY = 1; friendly.ubTransportationMask = 1;
	friendly.ubNextX = 10; friendly.ubNextY = 1; friendly.uiArrivalTime = 900; friendly.uiTraverseTime = 89;
	enemy.ubGroupID = 10; enemy.usGroupTeam = ENEMY_TEAM; enemy.ubGroupSize = 99;
	// Deliberately invalid union pointer: capture must not traverse it.
	enemy.pEnemyGroup = reinterpret_cast<ENEMYGROUP*>(1);
	empty.ubGroupID = 30; empty.usGroupTeam = OUR_TEAM; empty.fPersistant = TRUE;
	friendly.next = &enemy; enemy.next = &empty; gpGroupList = &friendly;
	CHECK(AdoptJa2StrategicGroup(friendly) && AdoptJa2StrategicGroup(enemy) && AdoptJa2StrategicGroup(empty), "native group identities adopted");
	PLAYERGROUP members[2]{};
	for (unsigned i = 0; i < 2; ++i)
	{
		auto& actor = *repository.resolve(i);
		actor.identity().id() = SoldierID{static_cast<UINT16>(i)}; actor.identity().incarnation() = 101 + i;
		actor.identity().profile() = static_cast<UINT8>(238 + i); actor.assignment().current() = static_cast<INT8>(i);
		actor.roster().active() = TRUE; actor.roster().team() = OUR_TEAM; actor.deployment().groupId() = 20;
		CHECK(AdoptJa2TacticalEntity(actor), "native actor identity adopted");
		members[i].actor = GetJa2TacticalEntityId(actor); members[i].ubProfileID = 1; // stale persistence-only field
	}
	members[1].next = &members[0]; friendly.pPlayerList = &members[1];
	const auto clock = GetWorldTotalSeconds();
	CHECK(!CaptureDedicatedCoopCampaignGroups(captured) && captured.available && captured.groupCount == 1 && captured.memberCount == 2 &&
		captured.groups[0].id == GetJa2StrategicGroupId(20) && captured.members[0].actor == members[0].actor &&
		captured.members[0].profile == 238 && captured.members[1].profile == 239 && captured.members[1].assignment == 1,
		"only occupied friendly groups, sorted exact actors and live profiles are published");
	CHECK(!captured.groups[0].nextX && !captured.groups[0].arrivalMinutes && !captured.groups[0].traverseMinutes &&
		friendly.ubNextX == 10 && friendly.uiArrivalTime == 900 && GetWorldTotalSeconds() == clock,
		"idle projection canonicalizes stale native transit fields without mutating them or clock");
	WAYPOINT first{10, 1, nullptr}, last{10, 3, nullptr}; first.next = &last; friendly.pWaypoints = &first;
	friendly.fBetweenSectors = TRUE;
	CHECK(!CaptureDedicatedCoopCampaignGroups(captured) && captured.groups[0].betweenSectors && captured.groups[0].nextX == 10 &&
		captured.groups[0].arrivalMinutes == 900 && captured.groups[0].destinationX == 10 && captured.groups[0].destinationY == 3 &&
		friendly.pWaypoints == &first && first.next == &last, "native next-leg timing and final waypoint are observations, not path mutations");
	captured.sessionEpoch = 7; captured.revision = 8;
	const auto before = captured;
	for (unsigned fault = 0; fault < 9; ++fault)
	{
		if (fault == 0) empty.next = &friendly;
		if (fault == 1) last.next = &first;
		if (fault == 2) members[0].next = &members[1];
		if (fault == 3) ++members[0].actor.incarnation;
		if (fault == 4) friendly.ubGroupSize = 3;
		if (fault == 5) repository.resolve(0)->deployment().groupId() = 99;
		if (fault == 6) friendly.ubNextY = 2;
		if (fault == 7) empty.ubGroupID = friendly.ubGroupID;
		if (fault == 8) { CHECK(ReleaseJa2StrategicGroup(friendly), "group released"); }
		CHECK(CaptureDedicatedCoopCampaignGroups(captured) && SameCoopCampaignGroups(captured, before),
			"cycles, stale actors/groups, bad members and diagonal transit reject without altering output");
		empty.next = nullptr; last.next = nullptr; members[0].next = nullptr;
		members[0].actor = GetJa2TacticalEntityId(*repository.resolve(0)); friendly.ubGroupSize = 2;
		repository.resolve(0)->deployment().groupId() = 20; friendly.ubNextY = 1; empty.ubGroupID = 30;
	}
	CHECK(AdoptJa2StrategicGroup(friendly) && !CaptureDedicatedCoopCampaignGroups(captured) &&
		captured.groups[0].id != before.groups[0].id, "native reused group slot receives a new observable incarnation");
	gpGroupList = nullptr; ResetJa2StrategicGroupDirectory();
	ResetJa2TacticalEntityDirectory();
	// Vehicle actors are real PLAYERGROUP members, in addition to the native
	// maximum of 254 mercenaries. The aggregate must span multiple groups.
	static_assert(CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS == 260, "fixture follows native player-slot capacity");
	GROUP full[2]{};
	PLAYERGROUP fullMembers[CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS]{};
	for (unsigned i = 0; i < 2; ++i)
	{
		full[i].ubGroupID = static_cast<UINT8>(40 + i); full[i].usGroupTeam = OUR_TEAM;
		full[i].ubGroupSize = i == 0 ? 255 : 5;
		full[i].ubSectorX = 9; full[i].ubSectorY = 1; full[i].fVehicle = TRUE;
		full[i].pPlayerList = &fullMembers[i == 0 ? 0 : 255];
	}
	full[0].next = &full[1]; gpGroupList = &full[0];
	CHECK(AdoptJa2StrategicGroup(full[0]) && AdoptJa2StrategicGroup(full[1]), "maximum roster groups adopted");
	for (unsigned i = 0; i < 260; ++i)
	{
		auto& actor = *repository.resolve(i);
		actor.identity().id() = SoldierID{static_cast<UINT16>(i)}; actor.identity().incarnation() = 1000 + i;
		actor.identity().profile() = static_cast<UINT8>(i < 254 ? i : 255);
		actor.roster().active() = TRUE; actor.roster().team() = OUR_TEAM;
		actor.deployment().groupId() = full[i < 255 ? 0 : 1].ubGroupID;
		if (i >= 254) actor.status().flags() |= SOLDIER_VEHICLE;
		CHECK(AdoptJa2TacticalEntity(actor), "maximum roster actor adopted");
		fullMembers[i].actor = GetJa2TacticalEntityId(actor);
		if (i != 254 && i != 259) fullMembers[i].next = &fullMembers[i + 1];
	}
	CHECK(!CaptureDedicatedCoopCampaignGroups(captured) && captured.available && captured.groupCount == 2 && captured.memberCount == 260 &&
		captured.groups[0].memberCount == 255 && captured.groups[1].firstMember == 255 && captured.groups[1].memberCount == 5 &&
		captured.groups[0].vehicle && captured.groups[1].vehicle,
		"native 255-plus-five group membership includes every mercenary and vehicle actor");
	for (unsigned i = 0; i < captured.memberCount && i < captured.members.size(); ++i)
		CHECK(captured.members[i].actor == fullMembers[i].actor && captured.members[i].profile == (i < 254 ? i : 255) &&
			fullMembers[i].next == ((i == 254 || i == 259) ? nullptr : &fullMembers[i + 1]),
			"maximum capture preserves each exact live identity and native membership link");
	CHECK(full[0].next == &full[1] && full[1].next == nullptr && full[0].ubGroupSize == 255 && full[1].ubGroupSize == 5 &&
		GetWorldTotalSeconds() == clock, "maximum capture never mutates native group counts or time");
	gpGroupList = nullptr; ResetJa2StrategicGroupDirectory(); ResetJa2TacticalEntityDirectory();
	return failures ? 1 : 0;
}
