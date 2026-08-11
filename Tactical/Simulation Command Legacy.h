#ifndef TACTICAL_SIMULATION_COMMAND_LEGACY_H
#define TACTICAL_SIMULATION_COMMAND_LEGACY_H

#include <cstdint>

#include <Engine/Adapters/JA2/SimulationCommand.h>

class TacticalActor;

// Compatibility completion seams for delayed JA2 actions that still store
// their pending state on TacticalActor. New command producers use the
// pointer-free TacticalEntityId API in Simulation Commands.h.
bool TryCompletePendingConversationCommand(TacticalActor& soldier) noexcept;
bool TryCompletePendingVehicleCommand(TacticalActor& soldier) noexcept;
bool TryCompletePendingStealCommand(TacticalActor& soldier) noexcept;
TacticalActor* ResolveAndConsumePendingStealTarget(
	TacticalActor& soldier,
	std::int32_t targetGrid,
	std::int8_t targetLevel) noexcept;
bool TryValidatePendingWorldItemPickup(TacticalActor& soldier) noexcept;
bool TryConsumePendingWorldItemPickup(
	TacticalActor& soldier,
	std::int32_t itemIndex,
	std::int32_t grid,
	std::int8_t level) noexcept;

// Compatibility executor for the established reload-all inventory policy.
// Command producers capture the complete exact roster in Simulation Commands.h;
// this seam owns only the legacy inventory/world-item application.
bool ExecuteBulkReloadWeaponsCommand(
	const BulkReloadWeaponsCommand& command);

#endif
