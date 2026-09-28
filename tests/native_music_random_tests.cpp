// Real playlist selection must not consume campaign randomness during cold
// loading, including failed playback on a host without an audio device.
#include "Music Control.h"
#include "soundman.h"
#include "random.h"
#include "GameContext.h"
#include "RuntimeSaveState.h"
#include "FileMan.h"
#include <Engine/Core/SimulationRandom.h>
#include <vfs/Core/vfs.h>
#include <vfs/Core/Location/vfs_directory_tree.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "unexpected shutdown: %s\n", message ? message : ""); std::exit(1);
}

int main()
{
	int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, m); } } while (0)
	CHECK(InstallGameSimulationRandom(7321) == GameSimulationRandomInstallError::None,
		"canonical campaign stream installed before native service construction");
	auto* random = GetGameSimulationRandomSource();
	if (!random) return 1;
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
		"native music fixture starts");
	const auto root = std::filesystem::temp_directory_path() /
		("ja2-music-random-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(root / "MUSIC");
	// Discovery uses actual private files; no mixer is opened, so playback is
	// deliberately unavailable. Selection occurs before that ordinary failure.
	for (const auto* file : {"Tactical_00.wav", "Tactical_01.wav", "Victory_00.wav", "Victory_01.wav"})
	{
		std::ofstream output(root / "MUSIC" / file, std::ios::binary);
		output << "unplayable native discovery fixture";
		CHECK(output.good(), "private playlist entry written");
	}
	auto* profile = new vfs::CVirtualProfile(L"_MUSIC_RANDOM_TEST", vfs::Path(root.c_str()), true);
	getVFS()->getProfileStack()->pushProfile(profile);
	auto* tree = new vfs::CDirectoryTree(vfs::Path(""), vfs::Path(root.c_str()));
	CHECK(tree->init(), "private music tree initialized");
	profile->addLocation(tree);
	CHECK(getVFS()->addLocation(tree, profile), "private music tree mounted");
	CHECK(InitializeFileManager(nullptr), "native file service initialized");
	InitializeMusicLists();
	CHECK(MusicListSize(MUSICLIST_TACTICAL_NOTHING) == 2 && MusicListSize(MUSICLIST_TACTICAL_VICTORY) == 2,
		"both restored music modes have multiple real discovered candidates");
	const auto before = random->checkpoint();
	const auto epoch = random->consumptionEpoch();
	{
		auto guard = BeginRuntimeLoadExecution(game, RuntimeSavePolicy::DedicatedDeterministic);
		CHECK(static_cast<bool>(guard), "strict native load guard armed");
		for (unsigned i = 0; i < 8; ++i)
		{
			CHECK(SetMusicMode(MUSIC_TACTICAL_VICTORY) && SetMusicMode(MUSIC_TACTICAL_NOTHING),
				"native mode restoration selects victory and post-battle playlists");
			CHECK(!IsMusicPlaying(), "no audio device or playback is required for safe selection");
		}
		CHECK(random->checkpoint() == before && random->consumptionEpoch() == epoch,
			"native playlist selection preserves both saved RNG and nonrewindable consumption evidence");
		CHECK(static_cast<bool>(guard.rollback()),
			"native load guard cleans up after playlist restoration");
	}
	for (UINT32 bound : {0u, 1u, 2u, 7u, 0x80000000u, std::numeric_limits<UINT32>::max()})
		for (unsigned i = 0; i < 100; ++i)
		{
			const auto value = SoundRandomRange(bound);
			CHECK(bound <= 1 ? value == 0 : value < bound, "cosmetic audio bounds cover the full unsigned range");
		}
	CHECK(random->checkpoint() == before && random->consumptionEpoch() == epoch,
		"cosmetic audio scheduling also leaves the gameplay stream unchanged");
	ShutdownMusicLists(); ShutdownFileManager(); vfs::CVirtualFileSystem::shutdownVFS();
	std::error_code ignored; std::filesystem::remove_all(root, ignored);
	std::printf("native music RNG: %d failures\n", failures);
	return failures ? 1 : 0;
}
