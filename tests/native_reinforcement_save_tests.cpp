// Exercise the real runtime save container and native load restoration without
// game assets. The opaque domain prefix is a fixture; native reinforcement
// counters and the canonical simulation RNG are the production instances.
#include "types.h"
#include "GameContext.h"
#include "RuntimeSaveState.h"
#include "Reinforcement.h"
#include "Queen Command.h"
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
extern UINT32 guiArrived, guiMilitiaArrived;

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
		("ja2-reinforcement-save-" + std::to_string(
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

void Seed(const TacticalReinforcementSaveState& state)
{
	guiTurnCnt = state.turnCounter;
	guiReinforceTurn = state.enemyTurn;
	guiArrived = state.enemyArrived;
	guiMilitiaReinforceTurn = state.militiaTurn;
	guiMilitiaArrived = state.militiaArrived;
}
}

int main()
{
	Check(InstallGameSimulationRandom(0x1a2b3c4d) == GameSimulationRandomInstallError::None,
		"install native deterministic RNG");
	auto* random = GetGameSimulationRandomSource();
	if (!random) return 1;
	GAME_SETTINGS settings{};
	GAME_OPTIONS options{};
	DiskStorage storage;
	EngineServices services{ZeroTimeSource::instance(), *random, storage};
	GameContext context(settings, options, GameCapabilities{}, services,
		NullPackageEventSink::instance(), random->campaignSeed());
	Check(context.beginInitialization() &&
		context.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && context.markRunning(),
		"start real runtime");
	const std::vector<std::uint8_t> domain{0x4a, 0x41, 0x32, 0x01};
	const auto write = [&](const char* path, bool extension, RuntimeSavePolicy policy)
	{
		if (!storage.writeAll(path, domain)) return false;
		auto guard = BeginRuntimeSaveExecution(context, policy);
		auto prepared = PrepareRuntimeSave(context, guard);
		if (extension) prepared.reinforcementState = CaptureTacticalReinforcementState();
		return static_cast<bool>(CommitRuntimeSave(context, path, std::move(prepared), guard));
	};
	const TacticalReinforcementSaveState saved{37, 44, 7, 52, 3};
	const TacticalReinforcementSaveState dirty{999, 1001, 23, 1002, 29};
	const auto roundtrip = [&](TacticalReinforcementSaveState state, RuntimeSavePolicy policy)
	{
		Seed(state);
		const auto before = random->checkpoint();
		const auto epoch = random->consumptionEpoch();
		if (!write("native.sav", true, policy)) return false;
		ResetTacticalReinforcementState(); // Native sector loading resets the globals.
		Seed(dirty); // Hot loading must replace every live value, including zeros.
		auto guard = BeginRuntimeLoadExecution(context, policy);
		auto prepared = PrepareRuntimeLoad(context, "native.sav", guard);
		if (!prepared || !prepared.reinforcementState) return false;
		RestoreTacticalReinforcementStateAfterLoad(&*prepared.reinforcementState);
		const bool restored = static_cast<bool>(RestorePreparedRuntimeSave(context, prepared, guard));
		return restored && CaptureTacticalReinforcementState() == state &&
			random->checkpoint() == before && random->consumptionEpoch() == epoch;
	};
	Check(roundtrip(saved, RuntimeSavePolicy::DedicatedDeterministic),
		"disk save/load restores all native deadlines/backlogs without RNG consumption");
	Check(roundtrip({}, RuntimeSavePolicy::DedicatedDeterministic),
		"saved zero state replaces dirty native globals without RNG consumption");
	Check(roundtrip({0xffffffffu, 0xfffffffeu, 0xffffffffu, 1, 0x80000000u},
		RuntimeSavePolicy::Interactive), "interactive saves preserve the complete native uint32 domain");

	gGameExternalOptions.sMinDelayEnemyReinforcements = 8;
	gGameExternalOptions.sRndDelayEnemyReinforcements = 9;
	gGameExternalOptions.sMinDelayMilitiaReinforcements = 12;
	gGameExternalOptions.sRndDelayMilitiaReinforcements = 7;
	for (unsigned mask = 0; mask < 4; ++mask)
	{
		const TacticalReinforcementSaveState legacy{37, mask & 1 ? 44u : 0u, 7,
			mask & 2 ? 52u : 0u, 3};
		Seed(legacy);
		Check(write("legacy.sav", false, RuntimeSavePolicy::Interactive),
			"write legacy envelope without extension");
		const auto before = random->checkpoint();
		const auto epoch = random->consumptionEpoch();
		auto expected = legacy;
		if (mask & 1) expected.enemyTurn = 37 + 8 / 2 + Random(9 + 1);
		if (mask & 2) expected.militiaTurn = 37 + 12 / 2 + Random(7 + 1);
		const auto expectedRandom = random->checkpoint();
		const auto draws = random->consumptionEpoch() - epoch;
		Check(random->restoreCheckpoint(before) == SimulationRandomCheckpointError::None,
			"reset only test oracle's RNG state");
		const auto restoreEpoch = random->consumptionEpoch();
		auto guard = BeginRuntimeLoadExecution(context, RuntimeSavePolicy::Interactive);
		auto prepared = PrepareRuntimeLoad(context, "legacy.sav", guard);
		Check(prepared && !prepared.reinforcementState, "absent extension remains a valid legacy save");
		if (!prepared) return 1;
		RestoreTacticalReinforcementStateAfterLoad(nullptr);
		Check(RestorePreparedRuntimeSave(context, prepared, guard) &&
			CaptureTacticalReinforcementState() == expected && random->checkpoint() == expectedRandom &&
			random->consumptionEpoch() - restoreEpoch == draws,
			"legacy conditional deadline values and exact RNG draws are unchanged");
	}

	Seed(saved);
	Check(write("valid.sav", true, RuntimeSavePolicy::DedicatedDeterministic), "write preflight fixture");
	RuntimeSaveContainer original;
	Check(static_cast<bool>(context.runtimeSaveContainers().inspect("valid.sav", original)),
		"inspect complete native runtime envelope");
	const auto* reinforcement = original.find(TacticalReinforcementSaveSection);
	Check(reinforcement && reinforcement->payload == std::vector<std::uint8_t>{
		1, 0, 0, 0, 37, 0, 0, 0, 44, 0, 0, 0,
		7, 0, 0, 0, 52, 0, 0, 0, 3, 0, 0, 0},
		"RINF version 1 retains its exact portable field order and byte layout");
	// Re-seal each damaged extension so the outer checksum is valid. This proves
	// semantic preflight rejects the extension, rather than only a bad checksum.
	for (unsigned fault = 0; fault < 5; ++fault)
	{
		auto sections = original.sections;
		for (auto& section : sections)
		{
			if (section.type != TacticalReinforcementSaveSection) continue;
			if (fault == 0) section.payload.pop_back();
			if (fault == 1) section.payload.push_back(0);
			if (fault == 2) section.payload[0] = 2;
			if (fault == 3) section.payload.clear();
			if (fault == 4) section.type ^= 0x80000000u;
		}
		Check(storage.writeAll("bad.sav", domain) &&
			context.runtimeSaveContainers().seal("bad.sav", sections) == RuntimeSaveContainerSaveError::None,
			"seal malformed native-extension fixture with valid outer checksums");
		Seed(dirty);
		const auto before = random->checkpoint();
		const auto epoch = random->consumptionEpoch();
		auto guard = BeginRuntimeLoadExecution(context, RuntimeSavePolicy::DedicatedDeterministic);
		auto rejected = PrepareRuntimeLoad(context, "bad.sav", guard);
		Check(!rejected && rejected.containerError == RuntimeSaveContainerLoadError::MalformedContainer &&
			CaptureTacticalReinforcementState() == dirty && random->checkpoint() == before &&
			random->consumptionEpoch() == epoch,
			"malformed/truncated/unsupported extension rejects before native state or RNG mutation");
	}
	ResetTacticalReinforcementState();
	return failures ? 1 : 0;
}
