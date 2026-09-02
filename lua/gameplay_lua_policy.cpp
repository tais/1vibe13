#include "gameplay_lua_policy.h"

#include "Ja2/DedicatedServerOptions.h"
#include "sgp/random.h"

#include <Engine/Core/SimulationRandom.h>
#include <Lua Interpreter.h>

#include <cstdint>
#include <limits>

namespace
{
char PolicyRegistryKey;
constexpr const char* OriginalRandomField = "original-random";
constexpr const char* OriginalRandomSeedField = "original-randomseed";
constexpr std::uint32_t LuaFractionResolution = 1u << 24u;

int CallOriginalMathFunction(lua_State* state, const char* field)
{
	const int argumentCount = lua_gettop(state);
	lua_rawgetp(state, LUA_REGISTRYINDEX, &PolicyRegistryKey);
	if (!lua_istable(state, -1))
		return luaL_error(state, "JA2 gameplay Lua policy registry is missing");

	lua_getfield(state, -1, field);
	if (!lua_isfunction(state, -1))
		return luaL_error(state, "JA2 gameplay Lua policy original is missing");
	lua_remove(state, -2);
	lua_insert(state, 1);
	lua_call(state, argumentCount, LUA_MULTRET);
	return lua_gettop(state);
}

int CanonicalMathRandom(lua_State* state)
{
	if (!DedicatedCoopLuaRandomPolicyActive())
		return CallOriginalMathFunction(state, OriginalRandomField);
	if (GetGameSimulationRandomSource() == nullptr)
		return luaL_error(state,
			"dedicated co-op math.random requires the campaign RNG");

	const int argumentCount = lua_gettop(state);
	if (argumentCount == 0)
	{
		const std::uint32_t value = Random(LuaFractionResolution);
		lua_pushnumber(state, static_cast<lua_Number>(value) /
			static_cast<lua_Number>(LuaFractionResolution));
		return 1;
	}
	if (argumentCount > 2)
		return luaL_error(state, "wrong number of arguments");

	lua_Integer lower = 1;
	lua_Integer upper = luaL_checkinteger(state, 1);
	if (argumentCount == 2)
	{
		lower = upper;
		upper = luaL_checkinteger(state, 2);
	}
	else if (upper == 0)
	{
		return luaL_error(state,
			"full-width math.random is unavailable in dedicated co-op");
	}

	luaL_argcheck(state, lower <= upper, 1, "interval is empty");
	const lua_Unsigned distance = static_cast<lua_Unsigned>(upper) -
		static_cast<lua_Unsigned>(lower);
	constexpr lua_Unsigned MaximumDistance =
		static_cast<lua_Unsigned>(
			std::numeric_limits<std::uint32_t>::max()) - 1u;
	luaL_argcheck(state, distance <= MaximumDistance, 1,
		"interval exceeds the JA2 random range");

	const std::uint32_t range = static_cast<std::uint32_t>(distance + 1u);
	const lua_Integer result = lower + static_cast<lua_Integer>(Random(range));
	lua_pushinteger(state, result);
	return 1;
}

int CanonicalMathRandomSeed(lua_State* state)
{
	if (!DedicatedCoopLuaRandomPolicyActive())
		return CallOriginalMathFunction(state, OriginalRandomSeedField);

	// Match Lua's public argument shape, but never let legacy package scripts
	// replace the campaign-owned stream.  Several installed JA2 data packages
	// call math.randomseed(os.time()) during bootstrap and ignore its return
	// values.  Accepting that call as a deterministic no-op keeps those scripts
	// compatible without admitting wall-clock entropy into authoritative state.
	if (!lua_isnone(state, 1))
	{
		(void)luaL_checkinteger(state, 1);
		(void)luaL_optinteger(state, 2, 0);
	}

	SimulationRandom* source = GetGameSimulationRandomSource();
	if (source == nullptr)
		return luaL_error(state,
			"dedicated co-op math.randomseed requires the campaign RNG");

	// Lua's stock function returns two seed integers.  Return a stable pair
	// derived from the immutable campaign seed rather than echoing a caller's
	// potentially nondeterministic values.
	const std::uint64_t campaignSeed = source->campaignSeed();
	lua_pushinteger(state, static_cast<lua_Integer>(
		static_cast<std::uint32_t>(campaignSeed)));
	lua_pushinteger(state, static_cast<lua_Integer>(
		static_cast<std::uint32_t>(campaignSeed >> 32u)));
	return 2;
}
}

bool DedicatedCoopLuaRandomPolicyActive() noexcept
{
	const DedicatedServerOptions& options = GetDedicatedServerOptions();
	return options.enabled && options.mode == DedicatedServerMode::Coop;
}

bool ConfigureLuaStandardLibrariesForGame(lua_State* state) noexcept
{
	if (state == nullptr) return false;
	const int originalTop = lua_gettop(state);

	lua_rawgetp(state, LUA_REGISTRYINDEX, &PolicyRegistryKey);
	if (lua_istable(state, -1))
	{
		lua_settop(state, originalTop);
		return true;
	}
	lua_settop(state, originalTop);

	lua_getglobal(state, "math");
	if (!lua_istable(state, -1))
	{
		lua_settop(state, originalTop);
		return false;
	}
	const int mathTable = lua_absindex(state, -1);
	lua_getfield(state, mathTable, "random");
	lua_getfield(state, mathTable, "randomseed");
	if (!lua_isfunction(state, -2) || !lua_isfunction(state, -1))
	{
		lua_settop(state, originalTop);
		return false;
	}

	lua_newtable(state);
	lua_pushvalue(state, -3);
	lua_setfield(state, -2, OriginalRandomField);
	lua_pushvalue(state, -2);
	lua_setfield(state, -2, OriginalRandomSeedField);
	lua_pushvalue(state, -1);
	lua_rawsetp(state, LUA_REGISTRYINDEX, &PolicyRegistryKey);

	lua_pushcfunction(state, CanonicalMathRandom);
	lua_setfield(state, mathTable, "random");
	lua_pushcfunction(state, CanonicalMathRandomSeed);
	lua_setfield(state, mathTable, "randomseed");
	lua_settop(state, originalTop);
	return true;
}
