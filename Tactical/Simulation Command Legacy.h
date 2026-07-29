#ifndef TACTICAL_SIMULATION_COMMAND_LEGACY_H
#define TACTICAL_SIMULATION_COMMAND_LEGACY_H

#include <cstdint>

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

#endif
