#include "StrategicSquadHost.h"

#include <array>
#include <limits>
#include <optional>
#include <utility>

#include <Engine/Adapters/JA2/TacticalEntityRoster.h>

#include "TacticalEntityHost.h"

namespace
{
template <std::size_t... Index>
std::array<TacticalEntityRoster, sizeof...(Index)> MakeSquadRosters(
	std::index_sequence<Index...>)
{
	return {
		((void)Index, TacticalEntityRoster(kJa2StrategicSquadCapacity))...};
}

auto& SquadRosters() noexcept
{
	static auto rosters = MakeSquadRosters(
		std::make_index_sequence<kJa2StrategicSquadCount>{});
	return rosters;
}

TacticalEntityRoster* SquadRoster(std::size_t squad) noexcept
{
	return squad < SquadRosters().size()
		? &SquadRosters()[squad]
		: nullptr;
}

const TacticalEntityRoster* ReadSquadRoster(std::size_t squad) noexcept
{
	return squad < SquadRosters().size()
		? &SquadRosters()[squad]
		: nullptr;
}

bool ActorBelongsToAnotherSquad(
	std::size_t squad, TacticalEntityId actor) noexcept
{
	for (std::size_t other = 0; other < SquadRosters().size(); ++other)
	{
		if (other != squad && SquadRosters()[other].contains(actor))
			return true;
	}
	return false;
}

void RebindRosterAfterRecordSwap(
	TacticalEntityRoster& roster) noexcept
{
	for (std::size_t slot = 0;
		slot < roster.highWaterMark(); ++slot)
	{
		const TacticalEntityId previous = roster.actor(slot);
		if (!previous.valid()) continue;

		const TacticalEntityId rebound =
			GetJa2TacticalEntityId(previous.slot);
		if (!rebound.valid() || !roster.replace(slot, rebound))
			(void)roster.erase(previous);
	}
}
}

void ResetJa2StrategicSquadRosters() noexcept
{
	for (TacticalEntityRoster& roster : SquadRosters())
		roster.clear();
}

std::size_t Ja2StrategicSquadSize(std::size_t squad) noexcept
{
	const TacticalEntityRoster* roster = ReadSquadRoster(squad);
	return roster ? roster->size() : 0;
}

TacticalEntityId GetJa2StrategicSquadActor(
	std::size_t squad, std::size_t slot) noexcept
{
	const TacticalEntityRoster* roster = ReadSquadRoster(squad);
	return roster ? roster->actor(slot) : TacticalEntityId{};
}

SOLDIERTYPE* ResolveJa2StrategicSquadActor(
	std::size_t squad, std::size_t slot) noexcept
{
	return ResolveJa2TacticalEntity(
		GetJa2StrategicSquadActor(squad, slot));
}

std::int32_t AddJa2StrategicSquadActor(
	std::size_t squad, TacticalEntityId actor) noexcept
{
	TacticalEntityRoster* roster = SquadRoster(squad);
	if (!roster || !ResolveJa2TacticalEntity(actor) ||
		ActorBelongsToAnotherSquad(squad, actor))
	{
		return -1;
	}
	const std::optional<TacticalEntityRoster::Slot> slot =
		roster->insert(actor);
	if (!slot ||
		*slot > static_cast<std::size_t>(
			std::numeric_limits<std::int32_t>::max()))
	{
		return -1;
	}
	return static_cast<std::int32_t>(*slot);
}

bool AssignJa2StrategicSquadActor(
	std::size_t squad,
	std::size_t slot,
	TacticalEntityId actor) noexcept
{
	TacticalEntityRoster* roster = SquadRoster(squad);
	return roster && ResolveJa2TacticalEntity(actor) &&
		!ActorBelongsToAnotherSquad(squad, actor) &&
		roster->assign(slot, actor);
}

bool RemoveJa2StrategicSquadActor(
	std::size_t squad, TacticalEntityId actor) noexcept
{
	TacticalEntityRoster* roster = SquadRoster(squad);
	return roster && roster->erase(actor);
}

bool RemoveJa2StrategicSquadActor(TacticalEntityId actor) noexcept
{
	bool removed = false;
	for (TacticalEntityRoster& roster : SquadRosters())
		removed = roster.erase(actor) || removed;
	return removed;
}

bool CompactJa2StrategicSquad(std::size_t squad) noexcept
{
	TacticalEntityRoster* roster = SquadRoster(squad);
	if (!roster) return false;
	roster->compact();
	return true;
}

bool SortJa2StrategicSquadByIdentity(std::size_t squad) noexcept
{
	TacticalEntityRoster* roster = SquadRoster(squad);
	if (!roster) return false;
	roster->sortByIdentity();
	return true;
}

void RebindJa2StrategicSquadRostersAfterRecordSwap() noexcept
{
	for (TacticalEntityRoster& roster : SquadRosters())
		RebindRosterAfterRecordSwap(roster);
}
