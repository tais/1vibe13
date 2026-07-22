#ifndef ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_H
#define ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_H

#include <cstdint>
#include <variant>

#include <Engine/Adapters/JA2/TacticalEntity.h>

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

struct ChangeStanceCommand
{
	std::uint16_t soldierId;
	std::uint8_t stance;
	SimulationCommandSource source;
};

struct BeginFireWeaponCommand
{
	TacticalEntityId soldier;
	std::int32_t targetGrid;
	std::int8_t targetLevel;
	std::int8_t targetCubeLevel;
	SimulationCommandSource source;
};

// A closed, value-only command set keeps the deterministic queue independent
// from JA2 globals and pointers. New commands extend this variant while their
// legacy executors remain in the compatibility layer during migration.
using SimulationCommand = std::variant<
	EndTurnCommand,
	ChangeStanceCommand,
	BeginFireWeaponCommand>;

#endif
