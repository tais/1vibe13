#ifndef TACTICAL_SIMULATION_COMMANDS_H
#define TACTICAL_SIMULATION_COMMANDS_H

#include <cstdint>

#include <Engine/Core/SimulationCommand.h>

// Compatibility adapter: queue an engine-owned value command, then execute all
// commands ready at the same simulation boundary. Existing EndTurn behavior
// remains synchronous while replay/network producers gain a deterministic seam.
std::uint64_t DispatchEndTurnCommandNow(
	std::uint8_t nextTeam,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer);

std::uint64_t DispatchChangeStanceCommandNow(
	std::uint16_t soldierId,
	std::uint8_t stance,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer);

void ExecuteSimulationCommandsThrough(std::uint64_t tick);

#endif
