#include "lua/gameplay_lua_policy.h"

#include "Ja2/DedicatedServerOptions.h"
#include "sgp/random.h"

#include <Engine/Core/SimulationRandom.h>
#include <Lua Interpreter.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Random.cpp retains these ordinary-game dependencies. This data-free harness
// never draws from the legacy source in its dedicated-coop process.
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

void Run(lua_State* state, const char* script)
{
	if (luaL_dostring(state, script) != LUA_OK)
	{
		std::fprintf(stderr, "Lua failure: %s\n", lua_tostring(state, -1));
		std::exit(1);
	}
}

int TestOrdinaryDelegation()
{
	lua_State* state = luaL_newstate();
	Check(state != nullptr, "ordinary test creates a Lua state");
	luaL_openlibs(state);
	Run(state,
		"math.randomseed(123, 456); "
		"return math.random(100000), math.random(-20, 20)");
	const lua_Integer expectedSecond = lua_tointeger(state, -1);
	const lua_Integer expectedFirst = lua_tointeger(state, -2);
	lua_settop(state, 0);

	Run(state, "math.randomseed(123, 456)");
	const int stackBefore = lua_gettop(state);
	Check(!DedicatedCoopLuaRandomPolicyActive() &&
		ConfigureLuaStandardLibrariesForGame(state) &&
		ConfigureLuaStandardLibrariesForGame(state) &&
		lua_gettop(state) == stackBefore,
		"ordinary policy configuration is inactive, idempotent, and stack-neutral");
	Run(state, "return math.random(100000), math.random(-20, 20)");
	Check(lua_tointeger(state, -2) == expectedFirst &&
		lua_tointeger(state, -1) == expectedSecond,
		"ordinary dispatch preserves Lua's original seeded random sequence");
	lua_settop(state, 0);
	Run(state, "return math.randomseed(9, 10)");
	Check(lua_gettop(state) == 2 && lua_isinteger(state, -2) &&
		lua_isinteger(state, -1),
		"ordinary dispatch preserves Lua's original randomseed results");
	lua_close(state);
	return 0;
}

int TestDedicatedCoopPolicy()
{
	constexpr std::uint64_t Seed = 0x123456789abcdef0ULL;
	lua_State* state = luaL_newstate();
	Check(state != nullptr, "co-op test creates a Lua state");
	luaL_openlibs(state);
	Check(!DedicatedCoopLuaRandomPolicyActive() &&
		ConfigureLuaStandardLibrariesForGame(state),
		"a pre-main gameplay state installs dormant policy dispatchers");

	Check(InstallGameSimulationRandom(Seed) ==
		GameSimulationRandomInstallError::None,
		"co-op test installs the campaign RNG before observation");
	DedicatedServerOptions options;
	options.enabled = true;
	options.mode = DedicatedServerMode::Coop;
	InstallDedicatedServerOptions(options);
	Check(DedicatedCoopLuaRandomPolicyActive(),
		"installed co-op mode activates already-configured Lua states");

	SimulationRandom expected(Seed);
	const SimulationRandomResult first = expected.tryNext(10);
	const SimulationRandomResult second = expected.tryNext(11);
	const SimulationRandomResult fraction = expected.tryNext(1u << 24u);
	Run(state, "return math.random(10), math.random(-5, 5), math.random()");
	Check(first && second && fraction &&
		lua_tointeger(state, -3) == static_cast<lua_Integer>(first.value + 1) &&
		lua_tointeger(state, -2) == static_cast<lua_Integer>(second.value) - 5 &&
		std::fabs(lua_tonumber(state, -1) -
			(static_cast<lua_Number>(fraction.value) /
				static_cast<lua_Number>(1u << 24u))) == 0.0,
		"co-op Lua integer and fraction draws follow the canonical golden stream");
	lua_settop(state, 0);

	SimulationRandom* installed = GetGameSimulationRandomSource();
	Check(installed != nullptr && installed->checkpoint() == expected.checkpoint(),
		"Lua advances exactly the process-owned campaign stream");

	const auto canonicalSeedLow = static_cast<lua_Integer>(
		static_cast<std::uint32_t>(Seed));
	const auto canonicalSeedHigh = static_cast<lua_Integer>(
		static_cast<std::uint32_t>(Seed >> 32u));
	const SimulationRandomResult installedShapeFirst = expected.tryNext(1000);
	const SimulationRandomResult installedShapeSecond =
		expected.tryNext(1u << 24u);
	lua_settop(state, 0);
	Run(state,
		"local seed_low, seed_high = math.randomseed(os.time()); "
		"local first = math.random(1000); "
		"math.randomseed(17, 29); "
		"local second = math.random(); "
		"return seed_low, seed_high, first, second");
	Check(installedShapeFirst && installedShapeSecond &&
		lua_gettop(state) == 4 &&
		lua_tointeger(state, -4) == canonicalSeedLow &&
		lua_tointeger(state, -3) == canonicalSeedHigh &&
		lua_tointeger(state, -2) ==
			static_cast<lua_Integer>(installedShapeFirst.value + 1u) &&
		std::fabs(lua_tonumber(state, -1) -
			(static_cast<lua_Number>(installedShapeSecond.value) /
				static_cast<lua_Number>(1u << 24u))) == 0.0,
		"installed randomseed(os.time()) calls are deterministic no-ops");
	Check(installed->checkpoint() == expected.checkpoint(),
		"legacy reseeding does not perturb the campaign stream");

	const SimulationRandomCheckpoint beforeInvalidSeed = installed->checkpoint();
	lua_settop(state, 0);
	Run(state,
		"local empty_ok, empty_low, empty_high = pcall(math.randomseed); "
		"local bad_first = pcall(math.randomseed, 'seed'); "
		"local bad_second = pcall(math.randomseed, 1, {}); "
		"local zero_ok = pcall(math.random, 0); "
		"local wide_ok = pcall(math.random, -2147483648, 2147483647); "
		"return empty_ok, empty_low, empty_high, bad_first, bad_second, "
			"zero_ok, wide_ok");
	Check(lua_gettop(state) == 7 && lua_toboolean(state, -7) &&
		lua_tointeger(state, -6) == canonicalSeedLow &&
		lua_tointeger(state, -5) == canonicalSeedHigh &&
		!lua_toboolean(state, -4) && !lua_toboolean(state, -3) &&
		!lua_toboolean(state, -2) && !lua_toboolean(state, -1) &&
		installed->checkpoint() == beforeInvalidSeed,
		"co-op validates seed arguments without mutating the stream and "
		"rejects intervals outside the canonical bounded API");
	lua_close(state);
	return 0;
}
}

int main(int argc, char** argv)
{
	if (argc != 2) return 2;
	if (std::strcmp(argv[1], "--ordinary") == 0)
		return TestOrdinaryDelegation();
	if (std::strcmp(argv[1], "--dedicated-coop") == 0)
		return TestDedicatedCoopPolicy();
	return 3;
}
