// Exercise the real runtime save container and passive checkpoint preparation without
// game assets. The opaque domain prefix is deliberately never interpreted by
// a passive client; native world, actors, events, and RNG are production state.
#include "types.h"
#include "GameContext.h"
#include "RuntimeSaveState.h"
#include "PassiveCampaignView.h"
#include "TacticalWorldAdapter.h"
#include "TacticalActor.h"
#include "SoldierRepository.h"
#include "Soldier Profile.h"
#include "CampaignEventAdapter.h"
#include "World Items.h"
#include "World Tile Map.h"
#include "worlddef.h"
#include <tuple>
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
		("ja2-passive-checkpoint-" + std::to_string(
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
    const bool restoreControl = argc == 2 && std::strcmp(argv[1], "--authority-restore-control") == 0;
    if (argc > 2 || (argc == 2 && !restoreControl)) return 2;
    Check(InstallGameSimulationRandom(0x723651) == GameSimulationRandomInstallError::None, "native RNG installed");
    auto* random = GetGameSimulationRandomSource();
    auto& native = GetGameContext();
    Check(native.beginInitialization() && native.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && native.markRunning(), "native runtime starts");
    if (!random || failures) return 1;
    GAME_SETTINGS settings{}; GAME_OPTIONS options{}; DiskStorage storage;
    EngineServices services{ZeroTimeSource::instance(), *random, storage};
    GameContext context(settings, options, GameCapabilities{}, services, NullPackageEventSink::instance(), random->campaignSeed());
    Check(context.beginInitialization() && context.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && context.markRunning(), "real disk runtime starts");
    auto& actors = GetJa2SoldierRepository(); actors.initializeSlots();
    auto& actor = *actors.resolve(4);
    actor.identity().id() = SoldierID{4}; actor.identity().incarnation() = 73;
    actor.roster().active() = TRUE; actor.position().gridNo() = 15467;
    actor.actionPoints().current() = 41;
    auto& events = GetJa2CampaignEventQueue(); events.clear();
    const auto event = events.schedule({1234, 8, 0, 0, 1, 0});
    Check(bool(event), "native event sentinel scheduled");
    for (auto& profile : gMercProfiles) std::memset(profile.zNickname, 0, sizeof(profile.zNickname));
    gMercProfiles[4].zNickname[0] = 'R'; gMercProfiles[4].zNickname[1] = 0x03a9;
    gMercProfiles[10].zNickname[0] = 'S';
    PassiveCampaignView strategic;
    Check(CapturePassiveCampaignView(strategic) && strategic.profiles.size() == 2 &&
        strategic.worldKind == PassiveCampaignWorldKind::Strategic, "capture native profile Unicode without restoring gameplay");
    NotifyJa2TacticalWorldLoaded(1); SetJa2TacticalWorldSector(9, 1, 0);
    PassiveCampaignView tactical;
    Check(CapturePassiveCampaignView(tactical) && tactical.worldKind == PassiveCampaignWorldKind::Tactical &&
        tactical.sectorX == 9 && tactical.sectorY == 1 && tactical.rows == WORLD_ROWS && tactical.columns == WORLD_COLS,
        "capture an explicit bounded tactical descriptor without changing checkpoint eligibility");
    NotifyJa2TacticalWorldUnloaded();
    const std::vector<std::uint8_t> domain{0x4a, 0x41, 0x32, 0x03};
    const auto write = [&](const char* path, const PassiveCampaignView* view) {
        if (!storage.writeAll(path, domain)) return false;
        auto guard = BeginRuntimeSaveExecution(context, RuntimeSavePolicy::DedicatedDeterministic);
        auto prepared = PrepareRuntimeSave(context, guard);
        prepared.reinforcementState = TacticalReinforcementSaveState{};
        prepared.scheduleState = TacticalScheduleSaveState{};
        if (view) prepared.passiveView = *view;
        return bool(CommitRuntimeSave(context, path, std::move(prepared), guard));
    };
    Check(write("tactical.sav", &tactical) && write("strategic.sav", &strategic) && write("legacy.sav", nullptr),
        "real strict disk envelopes contain optional owned projections");
    // Local process state has advanced since this checkpoint. Passive preparation
    // must not restore it, even if restoring the authority RNG would be valid.
    (void)Random(51);
    gMercProfiles[4].zNickname[0] = 'X';
    const auto state = [&] {
        return std::make_tuple(IsJa2TacticalWorldLoaded(), GetWorldTileMapSize(), GetNumUsedWorldItems(),
            &GetJa2SoldierRepository(), actor.identity().incarnation(), actor.position().gridNo(),
            actor.actionPoints().current(), actor.roster().active(), events.head(), events.size(),
            CaptureJa2TacticalStatusFlags(), GetJa2TacticalCurrentTeam(),
            random->checkpoint(), random->consumptionEpoch(), context.frameDriver().captureBoundaryState().state,
            gMercProfiles[4].zNickname[0]);
    };
    const auto before = state();
    PassiveCampaignView adopted;
    auto prepared = PreparePassiveCampaignCheckpoint(context, "tactical.sav", tactical.worldMinutes, adopted);
    if (restoreControl)
    {
        auto guard = BeginRuntimeLoadExecution(context, RuntimeSavePolicy::DedicatedDeterministic);
        auto authority = PrepareRuntimeLoad(context, "tactical.sav", guard);
        Check(authority && RestorePreparedRuntimeSave(context, authority, guard), "authority restoration control runs the production RNG/frame restore");
    }
    Check(prepared == PassiveCampaignPreparationResult::Ready && adopted == tactical && state() == before &&
        !IsJa2TacticalWorldLoaded(), "passive tactical bootstrap preserves descriptor/display data and every native/RNG/frame sentinel");
    std::array<wchar_t, 32> label{};
    Check(PassiveCampaignNickname(adopted, 4, label) && label[0] == 'R' && label[1] == 0x03a9 && !label[2] &&
        !PassiveCampaignNickname(adopted, 7, label), "owned names survive changed local profiles and absent labels stay absent");
    if (restoreControl)
    {
        events.clear(); std::printf("authority restoration negative control: %d failures\n", failures);
        return failures ? 1 : 0;
    }
    Check(PreparePassiveCampaignCheckpoint(context, "strategic.sav", strategic.worldMinutes, adopted) ==
        PassiveCampaignPreparationResult::Ready && adopted == strategic && state() == before,
        "ordinary strategic passive preparation also avoids all authority mutation");
    const auto oldView = adopted;
    Check(PreparePassiveCampaignCheckpoint(context, "legacy.sav", strategic.worldMinutes, adopted) ==
        PassiveCampaignPreparationResult::MissingView && adopted == oldView && state() == before,
        "legacy authority envelope without projection fails passive adoption atomically");
    { auto guard = BeginRuntimeLoadExecution(context, RuntimeSavePolicy::DedicatedDeterministic);
      auto legacy = PrepareRuntimeLoad(context, "legacy.sav", guard);
      Check(legacy && !legacy.passiveView && guard.rollback(), "legacy native host preflight stays supported for eligible refresh"); }
    Check(PreparePassiveCampaignCheckpoint(context, "strategic.sav", strategic.worldMinutes + 1, adopted) ==
        PassiveCampaignPreparationResult::WorldMinutesMismatch && adopted == oldView && state() == before,
        "transfer minute mismatch cannot publish a descriptor from another checkpoint");
    NotifyJa2TacticalWorldLoaded(2);
    Check(PreparePassiveCampaignCheckpoint(context, "tactical.sav", tactical.worldMinutes, adopted) ==
        PassiveCampaignPreparationResult::WorldActive && adopted == oldView, "existing native world rejects passive adoption before preflight");
    NotifyJa2TacticalWorldUnloaded();
    RuntimeSaveContainer original;
    Check(bool(context.runtimeSaveContainers().inspect("tactical.sav", original)), "inspect real tactical projection envelope");
    Check(original.sections.size() == 6 && original.find(TacticalScheduleSaveSection) &&
        original.find(TacticalReinforcementSaveSection),
        "real strict envelope composes PCVW, SCHD and RINF with all required sections");
    const auto* section = original.find(PassiveCampaignViewSection);
    Check(section && section->payload.size() == 20 + 2 * 130 &&
        section->payload[0] == 1 && section->payload[4] == 2 && section->payload[5] == 9 &&
        section->payload[16] == 2 && section->payload[20] == 4 && section->payload[22] == 'R',
        "version, descriptor, profile IDs and Unicode have a fixed portable byte layout");
    for (unsigned fault = 0; fault < 18; ++fault)
    {
        auto sections = original.sections;
        for (auto& part : sections)
        {
            if (part.type != PassiveCampaignViewSection) continue;
            if (fault == 0) part.payload.pop_back();
            if (fault == 1) part.payload.push_back(0);
            if (fault == 2) part.payload[0] = 2;
            if (fault == 3) part.payload[18] = 1;
            if (fault == 4) part.payload[17] = 1;
            if (fault == 5) part.payload[4] = 3;
            if (fault == 6) part.payload[5] = 17;
            if (fault == 7) part.payload[8] = part.payload[9] = 0;
            if (fault == 8) part.payload[20 + 130] = part.payload[20];
            if (fault == 9) part.payload[20] = 255;
            if (fault == 10) part.payload[22] = 0;
            if (fault == 11) { part.payload[22] = 0; part.payload[23] = 0xd8; }
            if (fault == 12) { part.payload[26] = 1; part.payload[27] = part.payload[28] = part.payload[29] = 0; }
            if (fault == 13) part.payload[20 + 129] = 1;
            if (fault == 14) part.type ^= 0x80000000u;
            if (fault == 15) { part.payload[22] = 0; part.payload[23] = 0; part.payload[24] = 0x11; }
            if (fault == 16) for (unsigned i = 0; i < 32; ++i) part.payload[22 + 4 * i] = 'R';
            if (fault == 17) for (unsigned i = 0; i < 16; ++i) {
                part.payload[22 + 4 * i] = 0; part.payload[23 + 4 * i] = 0; part.payload[24 + 4 * i] = 1;
            }
        }
        Check(storage.writeAll("bad.sav", domain) && context.runtimeSaveContainers().seal("bad.sav", sections) ==
            RuntimeSaveContainerSaveError::None, "reseal malformed projection with valid outer checksums");
        const auto nativeBefore = state();
        Check(PreparePassiveCampaignCheckpoint(context, "bad.sav", tactical.worldMinutes, adopted) ==
            PassiveCampaignPreparationResult::InvalidCheckpoint && adopted == oldView && state() == nativeBefore,
            "malformed Unicode/identity/descriptor/bounds reject before adoption and native mutation");
    }
    auto malformedSchedule = original.sections;
    for (auto& part : malformedSchedule) if (part.type == TacticalScheduleSaveSection) part.payload[0] = 2;
    Check(storage.writeAll("bad-schedule.sav", domain) && context.runtimeSaveContainers().seal("bad-schedule.sav", malformedSchedule) ==
        RuntimeSaveContainerSaveError::None, "reseal valid projection alongside malformed schedule extension");
    const auto beforeScheduleFailure = state();
    Check(PreparePassiveCampaignCheckpoint(context, "bad-schedule.sav", tactical.worldMinutes, adopted) ==
        PassiveCampaignPreparationResult::InvalidCheckpoint && adopted == oldView && state() == beforeScheduleFailure,
        "passive preparation validates the composed schedule section without native restoration");
    // Valid projection inside an incompatible runtime envelope is still rejected.
    auto incompatible = original.sections;
    for (auto& part : incompatible) if (part.type == 0x504b4843u /* CHKP */)
    {
        RuntimeCheckpoint checkpoint;
        Check(bool(context.runtime().runtimeCheckpoints().decode(part.payload,
            context.runtime().compatibilityFingerprint(), checkpoint)), "decode valid checkpoint metadata for incompatibility fixture");
        checkpoint.compatibility.high ^= 1;
        Check(context.runtime().runtimeCheckpoints().encode(checkpoint, part.payload) == RuntimeCheckpointSaveError::None,
            "encode another valid runtime fingerprint with fresh inner checksums");
    }
    Check(storage.writeAll("incompatible.sav", domain) && context.runtimeSaveContainers().seal("incompatible.sav", incompatible) ==
        RuntimeSaveContainerSaveError::None, "seal incompatible runtime metadata independently of projection");
    const auto nativeBefore = state();
    Check(PreparePassiveCampaignCheckpoint(context, "incompatible.sav", tactical.worldMinutes, adopted) ==
        PassiveCampaignPreparationResult::InvalidCheckpoint && adopted == oldView && state() == nativeBefore,
        "runtime incompatibility remains enforced before passive adoption");
    PassiveCampaignView empty;
    Check(write("empty.sav", &empty) && PreparePassiveCampaignCheckpoint(context, "empty.sav", 0, adopted) ==
        PassiveCampaignPreparationResult::Ready && adopted == empty, "explicit empty display catalog is supported");
    events.clear(); std::printf("native passive checkpoint preparation: %d failures\n", failures);
    return failures ? 1 : 0;
}
