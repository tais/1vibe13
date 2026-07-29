#include "VehiclePassengerHost.h"

#include <array>
#include <limits>
#include <utility>

#include <Engine/Adapters/JA2/TacticalEntityRoster.h>

#include "TacticalEntityHost.h"

namespace
{
template <std::size_t... Index>
std::array<TacticalEntityRoster, sizeof...(Index)> MakePassengerRosters(
	std::index_sequence<Index...>)
{
	return {
		((void)Index,
			TacticalEntityRoster(kJa2VehiclePassengerCapacity))...};
}

auto& PassengerRosters() noexcept
{
	static auto rosters = MakePassengerRosters(
		std::make_index_sequence<kJa2VehicleSlotCount>{});
	return rosters;
}

auto& DriverActors() noexcept
{
	static std::array<TacticalEntityId, kJa2VehicleSlotCount> drivers{};
	return drivers;
}

TacticalEntityRoster* PassengerRoster(std::size_t vehicle) noexcept
{
	return vehicle < PassengerRosters().size()
		? &PassengerRosters()[vehicle]
		: nullptr;
}

const TacticalEntityRoster* ReadPassengerRoster(
	std::size_t vehicle) noexcept
{
	return vehicle < PassengerRosters().size()
		? &PassengerRosters()[vehicle]
		: nullptr;
}

bool ActorBelongsToAnotherVehicle(
	std::size_t vehicle, TacticalEntityId actor) noexcept
{
	for (std::size_t other = 0;
		other < PassengerRosters().size(); ++other)
	{
		if (other != vehicle &&
			PassengerRosters()[other].contains(actor))
		{
			return true;
		}
	}
	return false;
}

void RebindRosterAfterRecordSwap(
	TacticalEntityRoster& roster) noexcept
{
	for (std::size_t seat = 0;
		seat < roster.highWaterMark(); ++seat)
	{
		const TacticalEntityId previous = roster.actor(seat);
		if (!previous.valid()) continue;

		const TacticalEntityId rebound =
			GetJa2TacticalEntityId(previous.slot);
		if (!rebound.valid() || !roster.replace(seat, rebound))
			(void)roster.erase(previous);
	}
}
}

void ResetJa2VehicleOccupants() noexcept
{
	for (TacticalEntityRoster& roster : PassengerRosters())
		roster.clear();
	DriverActors().fill(TacticalEntityId{});
}

bool ResetJa2VehicleOccupants(std::size_t vehicle) noexcept
{
	TacticalEntityRoster* roster = PassengerRoster(vehicle);
	if (!roster) return false;
	roster->clear();
	DriverActors()[vehicle] = {};
	return true;
}

std::size_t Ja2VehiclePassengerCount(std::size_t vehicle) noexcept
{
	const TacticalEntityRoster* roster = ReadPassengerRoster(vehicle);
	return roster ? roster->size() : 0;
}

TacticalEntityId GetJa2VehiclePassengerActor(
	std::size_t vehicle, std::size_t seat) noexcept
{
	const TacticalEntityRoster* roster = ReadPassengerRoster(vehicle);
	return roster ? roster->actor(seat) : TacticalEntityId{};
}

TacticalActor* ResolveJa2VehiclePassengerActor(
	std::size_t vehicle, std::size_t seat) noexcept
{
	return ResolveJa2TacticalEntity(
		GetJa2VehiclePassengerActor(vehicle, seat));
}

std::int32_t FindJa2VehiclePassengerSeat(
	std::size_t vehicle, TacticalEntityId actor) noexcept
{
	const TacticalEntityRoster* roster = ReadPassengerRoster(vehicle);
	if (!roster || !actor.valid()) return -1;
	for (std::size_t seat = 0;
		seat < roster->highWaterMark(); ++seat)
	{
		if (roster->actor(seat) != actor) continue;
		if (seat > static_cast<std::size_t>(
				std::numeric_limits<std::int32_t>::max()))
		{
			return -1;
		}
		return static_cast<std::int32_t>(seat);
	}
	return -1;
}

bool AssignJa2VehiclePassengerActor(
	std::size_t vehicle,
	std::size_t seat,
	TacticalEntityId actor) noexcept
{
	TacticalEntityRoster* roster = PassengerRoster(vehicle);
	if (!roster || !ResolveJa2TacticalEntity(actor) ||
		ActorBelongsToAnotherVehicle(vehicle, actor))
	{
		return false;
	}
	const TacticalEntityId current = roster->actor(seat);
	return (!current.valid() || current == actor) &&
		roster->assign(seat, actor);
}

bool RemoveJa2VehiclePassengerSeat(
	std::size_t vehicle, std::size_t seat) noexcept
{
	TacticalEntityRoster* roster = PassengerRoster(vehicle);
	if (!roster) return false;
	const TacticalEntityId actor = roster->actor(seat);
	if (!roster->eraseAt(seat)) return false;
	if (DriverActors()[vehicle] == actor)
		DriverActors()[vehicle] = {};
	return true;
}

bool RemoveJa2VehiclePassengerActor(
	std::size_t vehicle, TacticalEntityId actor) noexcept
{
	TacticalEntityRoster* roster = PassengerRoster(vehicle);
	if (!roster || !actor.valid()) return false;
	bool removed = roster->erase(actor);
	if (DriverActors()[vehicle] == actor)
	{
		DriverActors()[vehicle] = {};
		removed = true;
	}
	return removed;
}

bool RemoveJa2VehiclePassengerActor(TacticalEntityId actor) noexcept
{
	if (!actor.valid()) return false;
	bool removed = false;
	for (std::size_t vehicle = 0;
		vehicle < PassengerRosters().size(); ++vehicle)
	{
		removed =
			RemoveJa2VehiclePassengerActor(vehicle, actor) || removed;
	}
	return removed;
}

bool MoveJa2VehiclePassengerActor(
	std::size_t vehicle,
	std::size_t sourceSeat,
	std::size_t destinationSeat) noexcept
{
	TacticalEntityRoster* roster = PassengerRoster(vehicle);
	if (!roster || sourceSeat == destinationSeat ||
		roster->actor(destinationSeat).valid())
	{
		return false;
	}
	const TacticalEntityId actor = roster->actor(sourceSeat);
	if (!actor.valid() || !roster->eraseAt(sourceSeat))
		return false;
	if (roster->assign(destinationSeat, actor)) return true;
	(void)roster->assign(sourceSeat, actor);
	return false;
}

bool SwapJa2VehiclePassengerActors(
	std::size_t vehicle,
	std::size_t firstSeat,
	std::size_t secondSeat) noexcept
{
	TacticalEntityRoster* roster = PassengerRoster(vehicle);
	if (!roster || firstSeat == secondSeat) return false;
	const TacticalEntityId first = roster->actor(firstSeat);
	const TacticalEntityId second = roster->actor(secondSeat);
	if (!first.valid() || !second.valid()) return false;

	if (!roster->eraseAt(firstSeat) ||
		!roster->eraseAt(secondSeat))
	{
		return false;
	}
	const bool firstAssigned = roster->assign(firstSeat, second);
	const bool secondAssigned = roster->assign(secondSeat, first);
	if (firstAssigned && secondAssigned) return true;

	if (firstAssigned) (void)roster->eraseAt(firstSeat);
	if (secondAssigned) (void)roster->eraseAt(secondSeat);
	(void)roster->assign(firstSeat, first);
	(void)roster->assign(secondSeat, second);
	return false;
}

TacticalEntityId GetJa2VehicleDriverActor(
	std::size_t vehicle) noexcept
{
	return vehicle < DriverActors().size()
		? DriverActors()[vehicle]
		: TacticalEntityId{};
}

TacticalActor* ResolveJa2VehicleDriverActor(
	std::size_t vehicle) noexcept
{
	return ResolveJa2TacticalEntity(
		GetJa2VehicleDriverActor(vehicle));
}

bool SetJa2VehicleDriverActor(
	std::size_t vehicle, TacticalEntityId actor) noexcept
{
	TacticalEntityRoster* roster = PassengerRoster(vehicle);
	if (!roster) return false;
	if (!actor.valid())
	{
		DriverActors()[vehicle] = {};
		return true;
	}
	if (!ResolveJa2TacticalEntity(actor) || !roster->contains(actor))
		return false;
	DriverActors()[vehicle] = actor;
	return true;
}

void RebindJa2VehicleOccupantsAfterRecordSwap() noexcept
{
	for (std::size_t vehicle = 0;
		vehicle < PassengerRosters().size(); ++vehicle)
	{
		TacticalEntityRoster& roster = PassengerRosters()[vehicle];
		RebindRosterAfterRecordSwap(roster);

		const TacticalEntityId previousDriver =
			DriverActors()[vehicle];
		if (!previousDriver.valid()) continue;
		const TacticalEntityId reboundDriver =
			GetJa2TacticalEntityId(previousDriver.slot);
		DriverActors()[vehicle] =
			reboundDriver.valid() && roster.contains(reboundDriver)
				? reboundDriver
				: TacticalEntityId{};
	}
}
