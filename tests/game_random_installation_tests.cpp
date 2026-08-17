#include "sgp/random.h"

#include <Engine/Core/SimulationRandom.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

// Random.cpp retains these legacy dependencies for the uninstalled source.
// The focused test never draws from that source, so a complete game link would
// add no coverage; these definitions keep the installation contract isolated.
bool is_client = false;
bool is_server = false;
bool is_networked = false;
GAME_EXTERNAL_OPTIONS gGameExternalOptions{};
void MPDebugMsg(const CHAR8*) {}

namespace
{
void Check(bool condition, const char* message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

int TestLegacyObservation()
{
	RandomSource& first = GetGameRandomSource();
	RandomSource& second = GetGameRandomSource();
	Check(&first == &second,
		"the uninstalled process exposes one stable legacy source");
	Check(GetGameSimulationRandomSource() == nullptr,
		"the ordinary process remains on the legacy source");
	Check(InstallGameSimulationRandom(17) ==
		GameSimulationRandomInstallError::SourceAlreadyObserved,
		"installation fails after the legacy source is observed");
	Check(&GetGameRandomSource() == &first &&
		GetGameSimulationRandomSource() == nullptr,
		"failed installation cannot replace the observed legacy source");
	return 0;
}

int TestTypedObservation()
{
	Check(GetGameSimulationRandomSource() == nullptr,
		"the typed query reports an uninstalled source");
	Check(InstallGameSimulationRandom(23) ==
		GameSimulationRandomInstallError::SourceAlreadyObserved,
		"the typed query also seals the process source selection");
	Check(GetGameSimulationRandomSource() == nullptr,
		"a rejected install cannot create a typed source");
	return 0;
}

int TestInstalledSeed(std::uint64_t seed, std::uint64_t replacementSeed)
{
	Check(InstallGameSimulationRandom(seed) ==
		GameSimulationRandomInstallError::None,
		"installation succeeds before either source accessor is observed");
	SimulationRandom* installed = GetGameSimulationRandomSource();
	RandomSource& game = GetGameRandomSource();
	Check(installed != nullptr && static_cast<RandomSource*>(installed) == &game,
		"the generic and typed accessors expose the exact installed object");
	Check(installed->campaignSeed() == seed &&
		installed->checkpoint().campaignSeed == seed,
		"the installed source retains the full campaign seed");

	SimulationRandom expected(seed);
	const SimulationRandomResult expectedDraw = expected.tryNext(0xf0000001u);
	Check(expectedDraw && Random(0xf0000001u) == expectedDraw.value &&
		installed->checkpoint() == expected.checkpoint(),
		"the legacy Random entry point advances the installed simulation stream");

	const SimulationRandomCheckpoint beforeReinstall = installed->checkpoint();
	Check(InstallGameSimulationRandom(replacementSeed) ==
		GameSimulationRandomInstallError::AlreadyInstalled,
		"a second installation is rejected even with a different seed");
	Check(GetGameSimulationRandomSource() == installed &&
		&GetGameRandomSource() == static_cast<RandomSource*>(installed) &&
		installed->checkpoint() == beforeReinstall,
		"reinstallation cannot replace or rewind the installed source");
	return 0;
}
}

int main(int argc, char** argv)
{
	static_assert(!std::is_copy_constructible<SimulationRandom>::value &&
		!std::is_copy_assignable<SimulationRandom>::value &&
		!std::is_move_constructible<SimulationRandom>::value &&
		!std::is_move_assignable<SimulationRandom>::value,
		"the process-owned simulation source cannot be copied or moved");
	if (argc != 2) return 2;
	if (std::strcmp(argv[1], "--legacy-observation") == 0)
		return TestLegacyObservation();
	if (std::strcmp(argv[1], "--typed-observation") == 0)
		return TestTypedObservation();
	if (std::strcmp(argv[1], "--install-zero") == 0)
		return TestInstalledSeed(0, std::numeric_limits<std::uint64_t>::max());
	if (std::strcmp(argv[1], "--install-max") == 0)
		return TestInstalledSeed(
			std::numeric_limits<std::uint64_t>::max(), 0);
	return 3;
}
