// Exercise the real runtime save container and native load restoration without
// game assets. The opaque domain prefix is a fixture; native schedule
// lists, event queue and canonical simulation RNG are production instances.
#include "types.h"
#include "GameContext.h"
#include "RuntimeSaveState.h"
#include "Reinforcement.h"
#include "Queen Command.h"
#include "Scheduling.h"
#include "Animation Control.h"
#include "CampaignEventAdapter.h"
#include "Game Events.h"
#include "Game Event Hook.h"
#include "SoldierRepository.h"
#include "Soldier Profile Constants.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "Map Information.h"
#include "MemMan.h"
#include "Overhead.h"
#include "worlddef.h"
#include <cstring>
#include "random.h"
#include <Engine/Core/SimulationRandom.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "%s\n", message ? message : ""); std::exit(1); }
namespace
{
int failures = 0;
void Check(bool ok, const char* message)
{
	std::printf("%s: %s\n", ok ? "PASS" : "FAIL", message);
	if (!ok) ++failures;
}

class DiskStorage final : public ByteStorage
{
public:
	DiskStorage() : root(std::filesystem::temp_directory_path() /
		("ja2-schedule-save-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count())))
	{ std::filesystem::create_directory(root); }
	~DiskStorage() override { std::error_code error; std::filesystem::remove_all(root, error); }
	bool exists(const std::string& path) const override
	{ return std::filesystem::exists(root / path); }
	bool readAll(const std::string& path, std::vector<std::uint8_t>& bytes) const override
	{
		std::ifstream file(root / path, std::ios::binary);
		if (!file) return false;
		bytes.assign(std::istreambuf_iterator<char>(file), {});
		return !file.bad();
	}
	bool writeAll(const std::string& path, const std::vector<std::uint8_t>& bytes) override
	{
		std::ofstream file(root / path, std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		file.close();
		return !file.fail();
	}
	bool remove(const std::string& path) override
	{ return !exists(path) || std::filesystem::remove(root / path); }
	std::filesystem::path root;
};

}

int main(int argc, char** argv)
{
    const bool reconstructionControl = argc == 2 && std::strcmp(argv[1], "--reconstruction-control") == 0;
    if (argc > 2 || (argc == 2 && !reconstructionControl)) return 2;
    Check(InstallGameSimulationRandom(0x51ced123) == GameSimulationRandomInstallError::None, "install native RNG");
    auto* random = GetGameSimulationRandomSource();
    auto& nativeContext = GetGameContext();
    Check(nativeContext.beginInitialization() && nativeContext.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && nativeContext.markRunning(), "native context starts");
    if (!random || failures) return 1;
    GAME_SETTINGS settings{}; GAME_OPTIONS options{}; DiskStorage storage;
    EngineServices services{ZeroTimeSource::instance(), *random, storage};
    GameContext context(settings, options, GameCapabilities{}, services, NullPackageEventSink::instance(), random->campaignSeed());
    Check(context.beginInitialization() && context.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && context.markRunning(), "disk envelope runtime starts");
    auto& actors = GetJa2SoldierRepository();
    actors.initializeSlots(); ResetJa2TacticalActorRosters();
    auto& events = GetJa2CampaignEventQueue();
    events.clear();
    RestoreJa2TacticalTurnState(LOADING_SAVED_GAME, OUR_TEAM, 0);
    SetJa2TacticalWorldSector(1, 1, 0);
    gMapInformation.sNorthGridNo = 1000;
    gMapInformation.sEastGridNo = gMapInformation.sSouthGridNo = gMapInformation.sWestGridNo = -1;
    for (unsigned slot = 100; slot < 103; ++slot)
    {
        auto& actor = *actors.resolve(slot);
        actor.identity().id() = SoldierID{static_cast<UINT16>(slot)};
        actor.identity().incarnation() = slot + 7; actor.identity().profile() = NO_PROFILE;
        actor.identity().bodyType() = REGMALE; actor.roster().active() = TRUE;
        actor.roster().inSector() = slot != 101; actor.roster().team() = CIV_TEAM;
        actor.position().initialGrid() = 20000 + slot;
        Check(AdoptJa2TacticalEntity(actor), "native civilian identity adopted");
    }
    SOLDIERINITNODE init[2]{};
    init[0].pSoldier = actors.resolve(100); init[0].next = &init[1];
    init[1].pSoldier = actors.resolve(101); init[1].prev = &init[0];
    gSoldierInitHead = &init[0]; gSoldierInitTail = &init[1];
    const auto clearNative = [&] {
        DestroyAllSchedules();
        events.clear();
        for (unsigned slot = 100; slot < 103; ++slot) actors.resolve(slot)->schedule().id() = 0;
    };
    const auto eventState = [&] {
        std::vector<CampaignEventSnapshot> values;
        Check(events.capture(values), "native event queue captured"); return values;
    };
    const std::vector<std::uint8_t> domain{0x4a, 0x41, 0x32, 0x02};
    const auto write = [&](const char* path, bool extension, RuntimeSavePolicy policy) {
        if (!storage.writeAll(path, domain)) return false;
        auto guard = BeginRuntimeSaveExecution(context, policy);
        auto prepared = PrepareRuntimeSave(context, guard);
        prepared.reinforcementState = CaptureTacticalReinforcementState();
        if (extension)
        {
            prepared.scheduleState.emplace();
            if (!CaptureTacticalScheduleState(*prepared.scheduleState)) return false;
        }
        return bool(CommitRuntimeSave(context, path, std::move(prepared), guard));
    };

    clearNative(); gubScheduleID = 60;
    PostSchedules(); // Real defaults and native event posting, including one out-of-sector civilian.
    Check(gpScheduleList && (gpScheduleList->usFlags & SCHEDULE_FLAGS_TEMPORARY) && gubScheduleID == 62,
        "native default creation supplies timed schedules and actor associations");
    SCHEDULENODE** tail = &gpScheduleList;
    while (*tail) tail = &(*tail)->next;
    for (unsigned id = 1; id <= 33; ++id)
    {
        auto* node = static_cast<SCHEDULENODE*>(MemAlloc(sizeof(SCHEDULENODE)));
        if (!node) return 1;
        std::memset(node, 0, sizeof(*node));
        node->ubScheduleID = id; node->ubSoldierID = NOBODY;
        for (unsigned i = 0; i < 4; ++i)
        { node->usTime[i] = 0xffff; node->usData1[i] = 0xffffffffu - i; node->usData2[i] = 0x80000000u + i; }
        node->usTime[0] = 1200; node->ubAction[0] = SCHEDULE_ACTION_GRIDNO;
        *tail = node; tail = &node->next;
    }
    gubScheduleID = 91;
    TacticalScheduleSaveState saved;
    Check(CaptureTacticalScheduleState(saved) && saved.nodes.size() == 35 &&
        saved.nodes[0].actorSlot == 101 && saved.nodes.back().actorSlot == TacticalScheduleSaveNoActor,
        "capture ordered defaults, more than 32 authored nodes, unbound and out-of-sector associations");
    const auto before = random->checkpoint(); const auto epoch = random->consumptionEpoch();
    const auto savedEvents = eventState();
    Check(write("native.sav", true, RuntimeSavePolicy::DedicatedDeterministic), "write real disk envelope with both native extensions");
    RuntimeSaveContainer original;
    Check(bool(context.runtimeSaveContainers().inspect("native.sav", original)), "inspect disk schedule container");
    const auto* section = original.find(TacticalScheduleSaveSection);
    Check(section && section->payload.size() == 8 + 35 * 53 &&
        std::vector<std::uint8_t>(section->payload.begin(), section->payload.begin() + 17) ==
            std::vector<std::uint8_t>{1,0,0,0,35,0,91,0,62,0,1,101,0,108,0,0,0},
        "SCHD version, ordered record prefix and actor identity have a fixed portable layout");
    DestroyAllSchedulesWithoutDestroyingEvents(); gubScheduleID = 222;
    {
        auto guard = BeginRuntimeLoadExecution(context, RuntimeSavePolicy::DedicatedDeterministic);
        auto prepared = PrepareRuntimeLoad(context, "native.sav", guard);
        const auto* head = events.head();
        Check(prepared && prepared.scheduleState, "schedule extension preflights before native restoration");
        const bool restored = prepared && RestoreTacticalSchedulesAfterLoad(
            reconstructionControl ? nullptr : &*prepared.scheduleState);
        TacticalScheduleSaveState actual;
        Check(restored && CaptureTacticalScheduleState(actual) && actual == saved &&
            events.head() == head && eventState() == savedEvents &&
            random->checkpoint() == before && random->consumptionEpoch() == epoch,
            "native restore preserves full list, counter, actor associations, event nodes and RNG state/epoch");
        Check(prepared && RestorePreparedRuntimeSave(context, prepared, guard), "strict runtime load accepts zero-consumption native restoration");
    }
    if (reconstructionControl)
    {
        clearNative(); gSoldierInitHead = gSoldierInitTail = nullptr;
        std::printf("old reconstruction negative control: %d failures\n", failures);
        return failures ? 1 : 0;
    }
    PostDefaultSchedule(actors.resolve(102));
    Check(gubScheduleID == 92 && gpScheduleList->ubScheduleID == 92 && actors.resolve(102)->schedule().id() == 92,
        "next ordinary native allocation uses the preserved counter");
    // Restore domain actor associations/event snapshots as a real domain load would.
    actors.resolve(102)->schedule().id() = 0;
    Check(events.replace(savedEvents) == CampaignEventQueueError::None && RestoreTacticalSchedulesAfterLoad(&saved), "reset mixed native fixture");

    // Graph failures validate before swapping or touching the already loaded queue.
    for (unsigned fault = 0; fault < 5; ++fault)
    {
        auto invalid = saved;
        if (fault == 0) invalid.nodes[0].actorIncarnation++;
        if (fault == 1) invalid.nodes[0].actorSlot = 60000;
        if (fault == 2) invalid.nodes[1].id = invalid.nodes[0].id;
        if (fault == 3) actors.resolve(101)->schedule().id() = 99;
        if (fault == 4) actors.resolve(101)->roster().active() = FALSE;
        auto* oldHead = gpScheduleList; const auto count = gubScheduleID;
        const auto rng = random->checkpoint(); const auto work = random->consumptionEpoch();
        Check(!RestoreTacticalSchedulesAfterLoad(&invalid) && gpScheduleList == oldHead && gubScheduleID == count &&
            eventState() == savedEvents && random->checkpoint() == rng && random->consumptionEpoch() == work,
            "malformed graph or stale actor identity leaves native list, queue and RNG intact");
        actors.resolve(101)->schedule().id() = saved.nodes[0].id;
        actors.resolve(101)->roster().active() = TRUE;
    }
    CampaignEventSnapshot orphan{0xffffffffu, 254, 0, ONETIME_EVENT, EVENT_PROCESS_TACTICAL_SCHEDULE, 0};
    const auto added = events.schedule(orphan);
    auto* unchangedHead = gpScheduleList;
    Check(added && !RestoreTacticalSchedulesAfterLoad(&saved) && gpScheduleList == unchangedHead,
        "orphan native schedule-event association rejects before replacement");
    if (added) Check(events.erase(added.event) == CampaignEventQueueError::None, "remove only orphan event fixture");

    for (unsigned fault = 0; fault < 12; ++fault)
    {
        auto sections = original.sections;
        for (auto& part : sections)
        {
            if (part.type != TacticalScheduleSaveSection) continue;
            if (fault == 0) part.payload.pop_back();
            if (fault == 1) part.payload.push_back(0);
            if (fault == 2) part.payload[0] = 2;
            if (fault == 3) part.payload[7] = 1;
            if (fault == 4) part.payload[5] = 1;
            if (fault == 5) part.payload[8] = 0;
            if (fault == 6) part.payload[8 + 53] = part.payload[8];
            if (fault == 7) part.payload[13] = part.payload[14] = part.payload[15] = part.payload[16] = 0;
            if (fault == 8) part.payload[10] = 0x80;
            if (fault == 9) { part.payload[17] = 0xa0; part.payload[18] = 5; }
            if (fault == 10) part.payload[57] = 11;
            if (fault == 11) part.type ^= 0x80000000u;
        }
        Check(storage.writeAll("bad.sav", domain) && context.runtimeSaveContainers().seal("bad.sav", sections) == RuntimeSaveContainerSaveError::None,
            "seal malformed section with valid outer checksums");
        auto* oldHead = gpScheduleList; const auto rng = random->checkpoint(); const auto work = random->consumptionEpoch();
        auto guard = BeginRuntimeLoadExecution(context, RuntimeSavePolicy::DedicatedDeterministic);
        auto rejected = PrepareRuntimeLoad(context, "bad.sav", guard);
        Check(!rejected && rejected.containerError == RuntimeSaveContainerLoadError::MalformedContainer && gpScheduleList == oldHead &&
            random->checkpoint() == rng && random->consumptionEpoch() == work,
            "malformed/truncated/versioned schedule section rejects before native mutation or RNG use");
    }
    SCHEDULENODE* last = gpScheduleList;
    while (last->next) last = last->next;
    last->next = gpScheduleList;
    TacticalScheduleSaveState rejectedCapture;
    Check(!CaptureTacticalScheduleState(rejectedCapture), "cyclic native list fails bounded capture");
    last->next = nullptr;

    clearNative(); gubScheduleID = 193;
    const auto emptyRandom = random->checkpoint(); const auto emptyEpoch = random->consumptionEpoch();
    Check(write("empty.sav", true, RuntimeSavePolicy::DedicatedDeterministic), "write present empty native list");
    {
        auto guard = BeginRuntimeLoadExecution(context, RuntimeSavePolicy::DedicatedDeterministic);
        auto prepared = PrepareRuntimeLoad(context, "empty.sav", guard);
        Check(prepared && prepared.scheduleState && prepared.scheduleState->nodes.empty() &&
            RestoreTacticalSchedulesAfterLoad(&*prepared.scheduleState) && RestorePreparedRuntimeSave(context, prepared, guard) &&
            !gpScheduleList && gubScheduleID == 193 && events.empty() &&
            random->checkpoint() == emptyRandom && random->consumptionEpoch() == emptyEpoch,
            "present empty list skips random default creation even with eligible civilians");
    }

    // Compare the absent extension with a direct call to the unchanged old loader.
    clearNative(); gubScheduleID = 7;
    Check(write("legacy.sav", false, RuntimeSavePolicy::Interactive), "legacy envelope omits SCHD while retaining RINF");
    const auto legacyBefore = random->checkpoint(); const auto legacyEpoch = random->consumptionEpoch();
    PostSchedules();
    TacticalScheduleSaveState expected;
    Check(CaptureTacticalScheduleState(expected), "capture legacy reconstruction oracle");
    const auto legacyEvents = eventState(); const auto legacyAfter = random->checkpoint();
    const auto legacyDraws = random->consumptionEpoch() - legacyEpoch;
    Check(legacyDraws > 0 && !legacyEvents.empty(), "old reconstruction demonstrably consumes RNG and posts events");
    clearNative(); gubScheduleID = 7;
    Check(random->restoreCheckpoint(legacyBefore) == SimulationRandomCheckpointError::None, "reset only the legacy test oracle RNG");
    const auto newEpoch = random->consumptionEpoch();
    {
        auto guard = BeginRuntimeLoadExecution(context, RuntimeSavePolicy::Interactive);
        auto prepared = PrepareRuntimeLoad(context, "legacy.sav", guard);
        TacticalScheduleSaveState actual;
        Check(prepared && !prepared.scheduleState && RestoreTacticalSchedulesAfterLoad(nullptr) &&
            CaptureTacticalScheduleState(actual) && actual == expected && eventState() == legacyEvents &&
            random->checkpoint() == legacyAfter && random->consumptionEpoch() - newEpoch == legacyDraws &&
            RestorePreparedRuntimeSave(context, prepared, guard),
            "absent extension retains exact legacy list, allocation, event and RNG behavior");
    }
    clearNative(); gSoldierInitHead = gSoldierInitTail = nullptr;
    std::printf("native schedule save: %d failures\n", failures);
    return failures ? 1 : 0;
}
