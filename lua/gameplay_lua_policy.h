#ifndef JA2_GAMEPLAY_LUA_POLICY_H
#define JA2_GAMEPLAY_LUA_POLICY_H

struct lua_State;

// Installs an idempotent standard-library dispatch policy on one gameplay Lua
// state. Ordinary and PvP games continue to call Lua's original math.random
// and math.randomseed functions. Dedicated co-op dispatches math.random to the
// process-owned JA2 simulation stream. Its math.randomseed wrapper validates
// Lua's stock argument shape but is a deterministic compatibility no-op: it
// cannot replace or advance the immutable campaign stream.
//
// The function preserves the Lua stack. It returns false for a null state or a
// malformed/missing math library and leaves existing globals unchanged.
bool ConfigureLuaStandardLibrariesForGame(lua_State* state) noexcept;

// This is deliberately a live query: a few legacy gameplay Lua states are
// constructed before command-line options are installed. Their dispatch
// wrappers must observe the immutable mode selected later during process boot.
bool DedicatedCoopLuaRandomPolicyActive() noexcept;

#endif
