#include "DedicatedCoopCampaignGroups.h"
#include "StrategicGroupHost.h"
#include "Strategic Movement.h"
#include "TacticalActor.h"
#include "Overhead.h"

const char* CaptureDedicatedCoopCampaignGroups(CoopSession::CoopCampaignGroups& output) noexcept
{
	using namespace CoopSession;
	// Check the entire list before calling identity gateways which internally
	// traverse it. This bounds cycles/duplicate slots without touching AI unions.
	std::array<const GROUP*, MaximumCoopCampaignGroups> native{};
	std::array<bool, MaximumCoopCampaignGroups + 1> slots{};
	std::size_t count = 0;
	for (const GROUP* group = gpGroupList; group; group = group->next)
	{
		if (count == native.size() || !group->ubGroupID || slots[group->ubGroupID]) return "invalid group list";
		slots[group->ubGroupID] = true; native[count++] = group;
	}
	std::sort(native.begin(), native.begin() + count,
		[](const GROUP* a, const GROUP* b) { return a->ubGroupID < b->ubGroupID; });
	CoopCampaignGroups captured;
	captured.available = true;
	for (std::size_t i = 0; i < count; ++i)
	{
		const auto& source = *native[i];
		if (source.usGroupTeam != OUR_TEAM) continue;
		// Native persistent squads may be empty and have unset coordinates.
		if (!source.ubGroupSize && !source.pPlayerList) continue;
		if (!source.ubGroupSize || !source.pPlayerList) return "inconsistent group membership";
		auto& group = captured.groups[captured.groupCount++];
		group.id = GetJa2StrategicGroupId(source.ubGroupID);
		if (!group.id.valid() || ResolveJa2StrategicGroup(group.id) != &source) return "unresolved group identity";
		group.x = source.ubSectorX; group.y = source.ubSectorY; group.z = source.ubSectorZ;
		group.vehicle = source.fVehicle != FALSE; group.betweenSectors = source.fBetweenSectors != FALSE;
		group.transportationMask = source.ubTransportationMask;
		if (group.betweenSectors)
		{
			group.nextX = source.ubNextX; group.nextY = source.ubNextY;
			group.arrivalMinutes = source.uiArrivalTime; group.traverseMinutes = source.uiTraverseTime;
		}
		// The last waypoint is a destination, not an ETA for the whole route.
		// Bound even corrupt cyclic lists; do not allocate or rebuild paths.
		std::size_t waypoints = 0;
		for (const WAYPOINT* waypoint = source.pWaypoints; waypoint; waypoint = waypoint->next)
		{
			if (++waypoints > 1024 || !ValidCoopCampaignSector(waypoint->x, waypoint->y)) return "invalid waypoint list";
			group.destinationX = waypoint->x; group.destinationY = waypoint->y;
		}
		group.firstMember = captured.memberCount;
		for (const PLAYERGROUP* member = source.pPlayerList; member; member = member->next)
		{
			if (captured.memberCount == captured.members.size()) return "group member capacity exceeded";
			const auto* actor = ResolvePlayerGroupMember(member);
			if (!actor || !actor->roster().active() || actor->roster().team() != OUR_TEAM ||
				actor->deployment().groupId() != source.ubGroupID) return "unresolved group member";
			captured.members[captured.memberCount++] = {GetPlayerGroupMemberActor(member),
				actor->identity().profile(), actor->assignment().current()};
			++group.memberCount;
		}
		if (group.memberCount != source.ubGroupSize) return "inconsistent group size";
		std::sort(captured.members.begin() + group.firstMember, captured.members.begin() + captured.memberCount,
			[](const CoopCampaignGroupMember& a, const CoopCampaignGroupMember& b) { return a.actor.slot < b.actor.slot; });
	}
	// Validate the projection with temporary stamps. The session ledger owns
	// real stamps; capture never reads or advances clocks or simulation state.
	captured.sessionEpoch = 1; captured.revision = 1;
	if (!ValidCoopCampaignGroups(captured)) return "invalid group projection";
	captured.sessionEpoch = 0; captured.revision = 0;
	output = captured;
	return nullptr;
}
