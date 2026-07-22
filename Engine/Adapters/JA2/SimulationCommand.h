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
	// Incarnation zero is reserved for decoded version-1 journals and is never
	// emitted by the version-2 encoder. Executors must deliberately resolve or
	// reject such a legacy-unresolved reference before changing live state.
	TacticalEntityId soldier;
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

struct MoveToGridCommand
{
	TacticalEntityId soldier;
	std::int32_t destinationGrid;
	std::uint16_t movementMode;
	bool reverse;
	bool forceRestart;
	SimulationCommandSource source;
};

// A closed, value-only command set keeps the deterministic queue independent
// from JA2 globals and pointers. New commands extend this variant while their
// legacy executors remain in the compatibility layer during migration.
using SimulationCommand = std::variant<
	EndTurnCommand,
	ChangeStanceCommand,
	BeginFireWeaponCommand,
	MoveToGridCommand>;

#endif
