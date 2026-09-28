#include "DedicatedCoopTravel.h"
#include "DedicatedCoopCampaignGroups.h"
#include "DedicatedCoopMissionBootstrap.h"
#include "StrategicGroupHost.h"
#include "TacticalEntityHost.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "Strategic Movement.h"
#include "StrategicPathQuery.h"
#include "Map Screen Interface.h"
#include "Campaign Types.h"
#include "Assignments.h"
#include "Game Clock.h"
#include "Overhead.h"
#include <algorithm>

DedicatedCoopTravelPlanningResult PlanDedicatedCoopTravel(StrategicGroupId identity,
	std::uint8_t destinationX, std::uint8_t destinationY, DedicatedCoopTravelPlan& output) noexcept
{
	using Code = DedicatedCoopTravelPlanCode;
	if (!IsDedicatedCoopStarterMissionMapReady() || gTacticalStatus.fDidGameJustStart) return {Code::HostUnavailable};
	if (!CoopSession::ValidCoopCampaignSector(destinationX, destinationY)) return {Code::InvalidDestination};
	// Bound and validate the native graph before any legacy group traversal.
	CoopSession::CoopCampaignGroups groups;
	if (!identity.valid() || CaptureDedicatedCoopCampaignGroups(groups)) return {Code::GroupUnavailable};
	const auto found = std::find_if(groups.groups.begin(), groups.groups.begin() + groups.groupCount,
		[&](const CoopSession::CoopCampaignGroup& group) { return group.id == identity; });
	if (found == groups.groups.begin() + groups.groupCount) return {Code::GroupUnavailable};
	GROUP* group = ResolveJa2StrategicGroup(identity);
	if (!group) return {Code::GroupUnavailable};
	if (group->fVehicle || group->ubTransportationMask != FOOT || group->ubSectorZ || group->ubMoveType != ONE_WAY)
		return {Code::UnsupportedGroup};
	if (group->fBetweenSectors || group->pWaypoints) return {Code::Busy};
	if (group->ubSectorX == destinationX && group->ubSectorY == destinationY) return {Code::InvalidDestination};
	DedicatedCoopTravelPlan plan;
	plan.group = identity; plan.observedWorldSeconds = GetWorldTotalSeconds();
	for (std::size_t i = 0; i < found->memberCount; ++i)
	{
		const auto actorId = groups.members[found->firstMember + i].actor;
		auto* actor = ResolveJa2TacticalEntity(actorId);
		if (!actor || actor->deployment().groupId() != identity.slot ||
			actor->deployment().sectorX() != group->ubSectorX || actor->deployment().sectorY() != group->ubSectorY ||
			actor->deployment().sectorZ() != group->ubSectorZ || actor->deployment().isBetweenSectors())
			return {Code::GroupUnavailable, actorId};
		if ((actor->status().flags() & SOLDIER_VEHICLE) || actor->assignment().current() == VEHICLE || actor->assignment().current() < 0)
			return {Code::UnsupportedGroup, actorId};
		if (!actor->strategicPath().empty()) return {Code::Busy, actorId};
		INT8 error = 0;
		if (!CanCharacterMoveInStrategicWithoutSideEffects(actor, &error))
			return {error == STRATEGIC_MOVE_REQUIRES_TACTICAL_CONTEXT ? Code::NativeContextUnavailable : Code::NativeRejected, actorId, error};
		plan.members[plan.memberCount++] = actorId;
	}
	StrategicPathDirections path;
	if (!QueryStrategicPathWithoutUi(*group, destinationX, destinationY, path)) return {Code::NoRoute};
	if (!path.count || path.count >= plan.sectors.size()) return {Code::InvalidRoute};
	std::array<bool, 256> visited{};
	std::uint8_t x = group->ubSectorX, y = group->ubSectorY;
	visited[SECTOR(x, y)] = true; plan.sectors[plan.sectorCount++] = {x, y, 0};
	for (std::size_t i = 0; i < path.count; ++i)
	{
		const auto direction = path.directions[i];
		if (direction > 6 || direction % 2) return {Code::InvalidRoute};
		const auto minutes = GetSectorMvtTimeForGroupWithoutUiCache(SECTOR(x, y), direction / 2, group);
		if (minutes <= 0) return {Code::InvalidRoute};
		if (direction == 0) --y;
		else if (direction == 2) ++x;
		else if (direction == 4) ++y;
		else --x;
		if (!CoopSession::ValidCoopCampaignSector(x, y) || visited[SECTOR(x, y)]) return {Code::InvalidRoute};
		visited[SECTOR(x, y)] = true;
		const std::uint64_t total = static_cast<std::uint64_t>(plan.totalMinutes) + static_cast<std::uint32_t>(minutes);
		// Native event timestamps are seconds, even though group arrivals use
		// minutes. Do not expose a plan that would overflow either representation.
		if ((static_cast<std::uint64_t>(plan.observedWorldSeconds / 60) + total) * 60 > UINT32_MAX) return {Code::InvalidRoute};
		plan.totalMinutes = static_cast<std::uint32_t>(total);
		plan.sectors[plan.sectorCount++] = {x, y, static_cast<std::uint32_t>(minutes)};
	}
	if (x != destinationX || y != destinationY) return {Code::InvalidRoute};
	output = plan;
	return {Code::Planned};
}
