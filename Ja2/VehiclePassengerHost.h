#ifndef JA2_VEHICLE_PASSENGER_HOST_H
#define JA2_VEHICLE_PASSENGER_HOST_H

#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/TacticalEntity.h>

class TacticalActor;

inline constexpr std::size_t kJa2VehicleSlotCount = 10;
inline constexpr std::size_t kJa2VehiclePassengerCapacity = 10;

// Runtime vehicle occupancy retains exact tactical identities in fixed seats.
// Vehicle rules and persistence remain in Tactical/Vehicles.cpp; this host owns
// only pointer-free passenger membership and the exact current driver.
void ResetJa2VehicleOccupants() noexcept;
bool ResetJa2VehicleOccupants(std::size_t vehicle) noexcept;

std::size_t Ja2VehiclePassengerCount(std::size_t vehicle) noexcept;
TacticalEntityId GetJa2VehiclePassengerActor(
	std::size_t vehicle, std::size_t seat) noexcept;
TacticalActor* ResolveJa2VehiclePassengerActor(
	std::size_t vehicle, std::size_t seat) noexcept;
std::int32_t FindJa2VehiclePassengerSeat(
	std::size_t vehicle, TacticalEntityId actor) noexcept;

bool AssignJa2VehiclePassengerActor(
	std::size_t vehicle,
	std::size_t seat,
	TacticalEntityId actor) noexcept;
bool RemoveJa2VehiclePassengerSeat(
	std::size_t vehicle, std::size_t seat) noexcept;
bool RemoveJa2VehiclePassengerActor(
	std::size_t vehicle, TacticalEntityId actor) noexcept;
bool RemoveJa2VehiclePassengerActor(TacticalEntityId actor) noexcept;
bool MoveJa2VehiclePassengerActor(
	std::size_t vehicle,
	std::size_t sourceSeat,
	std::size_t destinationSeat) noexcept;
bool SwapJa2VehiclePassengerActors(
	std::size_t vehicle,
	std::size_t firstSeat,
	std::size_t secondSeat) noexcept;

TacticalEntityId GetJa2VehicleDriverActor(
	std::size_t vehicle) noexcept;
TacticalActor* ResolveJa2VehicleDriverActor(
	std::size_t vehicle) noexcept;
bool SetJa2VehicleDriverActor(
	std::size_t vehicle, TacticalEntityId actor) noexcept;

// Whole-record swaps retain fixed legacy addresses. Rebinding by canonical
// repository slot preserves that historical behavior without stale identities.
void RebindJa2VehicleOccupantsAfterRecordSwap() noexcept;

#endif
