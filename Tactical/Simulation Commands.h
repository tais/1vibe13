#ifndef TACTICAL_SIMULATION_COMMANDS_H
#define TACTICAL_SIMULATION_COMMANDS_H

#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Core/CommandProcessor.h>

// Authoritative completion seam for hosts that must correlate package/network
// requests with actual simulation disposition. This is independent of the
// best-effort command journal and is invoked only after queue acknowledgement
// (or for an explicit retry observation).
class SimulationCommandExecutionSink
{
public:
	virtual ~SimulationCommandExecutionSink() = default;
	virtual void commandProcessed(
		const SimulationCommand& command,
		std::uint64_t tick,
		std::uint64_t sequence,
		CommandDisposition disposition) noexcept = 0;
};

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

std::uint64_t DispatchBeginFireWeaponCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer);

CommandProcessingResult ExecuteSimulationCommandsThrough(std::uint64_t tick);
CommandProcessingResult ExecuteSimulationCommandsThrough(
	std::uint64_t tick, std::size_t maximumCommands);
CommandProcessingResult ExecuteSimulationCommandsThrough(
	std::uint64_t tick, std::size_t maximumCommands,
	SimulationCommandExecutionSink& sink);

#endif
