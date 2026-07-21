#ifndef ENGINE_CORE_SIMULATION_COMMAND_H
#define ENGINE_CORE_SIMULATION_COMMAND_H

#include <cstdint>
#include <variant>

enum class SimulationCommandSource : std::uint8_t
{
	LocalPlayer,
	NetworkPeer,
	System,
	Replay
};

struct EndTurnCommand
{
	std::uint8_t nextTeam;
	SimulationCommandSource source;
};

// A closed, value-only command set keeps the deterministic queue independent
// from JA2 globals and pointers. New commands extend this variant while their
// legacy executors remain in the compatibility layer during migration.
using SimulationCommand = std::variant<EndTurnCommand>;

#endif
