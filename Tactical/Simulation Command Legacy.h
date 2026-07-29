#ifndef TACTICAL_SIMULATION_COMMAND_LEGACY_H
#define TACTICAL_SIMULATION_COMMAND_LEGACY_H

#include <cstdint>

class SOLDIERTYPE;

// Compatibility completion seams for delayed JA2 actions that still store
// their pending state on SOLDIERTYPE. New command producers use the
// pointer-free TacticalEntityId API in Simulation Commands.h.
bool TryCompletePendingConversationCommand(SOLDIERTYPE& soldier) noexcept;
bool TryCompletePendingVehicleCommand(SOLDIERTYPE& soldier) noexcept;
bool TryCompletePendingStealCommand(SOLDIERTYPE& soldier) noexcept;
SOLDIERTYPE* ResolveAndConsumePendingStealTarget(
	SOLDIERTYPE& soldier,
	std::int32_t targetGrid,
	std::int8_t targetLevel) noexcept;
bool TryValidatePendingWorldItemPickup(SOLDIERTYPE& soldier) noexcept;
bool TryConsumePendingWorldItemPickup(
	SOLDIERTYPE& soldier,
	std::int32_t itemIndex,
	std::int32_t grid,
	std::int8_t level) noexcept;

#endif
