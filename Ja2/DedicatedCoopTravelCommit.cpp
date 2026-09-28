#include "DedicatedCoopTravel.h"
#include "DedicatedCoopArrival.h"
#include "types.h"
#include "CampaignEventAdapter.h"
#include "Game Clock.h"
#include "Game Events.h"
#include "Strategic Movement.h"
#include "Strategic Path Types.h"
#include "Strategic Pathing.h"
#include "StrategicGroupHost.h"
#include "TacticalEntityHost.h"
#include "TacticalActor.h"
#include "Overhead.h"
#include "PreBattle Interface.h"
#include "Assignments.h"
#include "MemMan.h"

extern BOOLEAN gfProcessingGameEvents;
extern BOOLEAN gfRandomizingPatrolGroup;
extern void RemoveSoldierFromTacticalSector(TacticalActor*, BOOLEAN);

namespace
{
// Native path/waypoint consumers release nodes using MemFree, not delete.
// Storage stays private until every allocation and event insertion succeeds.
struct PreparedDeparture
{
	WAYPOINT* waypoint = nullptr;
	std::array<PathSt*, 256> paths{};
	~PreparedDeparture()
	{
		if (waypoint) MemFree(waypoint);
		for (auto* path : paths)
			while (path) { auto* next = path->pNext; MemFree(path); path = next; }
	}
	bool allocate(const DedicatedCoopTravelPlan& plan) noexcept
	{
		waypoint = static_cast<WAYPOINT*>(MemAlloc(sizeof(WAYPOINT)));
		if (!waypoint) return false;
		*waypoint = {plan.sectors[1].x, plan.sectors[1].y, nullptr};
		for (std::size_t i = 0; i < plan.memberCount; ++i)
		{
			PathSt* previous = nullptr;
			for (std::size_t j = 0; j < 2; ++j)
			{
				auto* node = static_cast<PathSt*>(MemAlloc(sizeof(PathSt)));
				if (!node) return false;
				*node = {};
				node->uiSectorId = plan.sectors[j].y * MAP_WORLD_X + plan.sectors[j].x;
				node->uiEta = plan.observedWorldSeconds / 60 + plan.sectors[j].minutesFromPrevious;
				node->fSpeed = NORMAL_MVT; node->pPrev = previous;
				if (previous) previous->pNext = node; else paths[i] = node;
				previous = node;
			}
		}
		return true;
	}
};
}

DedicatedCoopTravelStartResult StartDedicatedCoopTravel(StrategicGroupId identity,
	std::uint8_t destinationX, std::uint8_t destinationY) noexcept
{
	using Code = DedicatedCoopTravelStartCode;
	DedicatedCoopTravelPlan plan;
	const auto preflight = PlanDedicatedCoopTravel(identity, destinationX, destinationY, plan);
	if (preflight.code != DedicatedCoopTravelPlanCode::Planned) return {Code::PreflightRejected, preflight};
	if (plan.sectorCount != 2) return {Code::AdjacentSectorRequired, preflight};
	if (gfProcessingGameEvents || gfPreBattleInterfaceActive || gfTacticalTraversal || gfRandomizingPatrolGroup)
		return {Code::NativeInteractionRequired, preflight};
	if (DedicatedCoopArrivalDecisionPending()) return {Code::NativeInteractionRequired, preflight};
	GROUP* group = ResolveJa2StrategicGroup(identity);
	if (!group) return {Code::NativeInteractionRequired, preflight};
	std::array<TacticalActor*, 256> members{};
	for (std::size_t i = 0; i < plan.memberCount; ++i)
	{
		members[i] = ResolveJa2TacticalEntity(plan.members[i]);
		if (!members[i] || members[i]->roster().inSector()) return {Code::NativeInteractionRequired, preflight};
	}
	const auto minutes = plan.sectors[1].minutesFromPrevious;
	const auto arrival = plan.observedWorldSeconds / 60 + minutes;
	// Preserve the native crossing rule. That path deletes/reposts other groups'
	// events and consumes RNG; do not partly apply it through this simple commit.
	for (const GROUP* other = gpGroupList; other; other = other->next)
		if (other->usGroupTeam != OUR_TEAM && other->uiArrivalTime < arrival &&
			other->ubNextX == group->ubSectorX && other->ubNextY == group->ubSectorY &&
			other->ubSectorX == destinationX && other->ubSectorY == destinationY)
			return {Code::NativeInteractionRequired, preflight};
	auto& queue = GetJa2CampaignEventQueue();
	if (!queue.validate()) return {Code::EventQueueFailure, preflight};
	for (const auto* event = queue.head(); event; event = event->next)
		if (event->parameter == identity.slot &&
			(event->callbackId == EVENT_GROUP_ARRIVAL || event->callbackId == EVENT_GROUP_ABOUT_TO_ARRIVE))
			return {Code::EventConflict, preflight};
	PreparedDeparture prepared;
	if (!prepared.allocate(plan)) return {Code::AllocationFailure, preflight};
	// Match native arrival and optional 30-minute warning scheduling. The batch
	// retains equal-time FIFO ordering and cannot expose half a departure.
	std::array<CampaignEventSnapshot, 2> events{{
		{arrival * 60, identity.slot, 0, ONETIME_EVENT, EVENT_GROUP_ARRIVAL, 0},
		{minutes > 30 ? (arrival - 30) * 60 : 0, identity.slot, 0, ONETIME_EVENT, EVENT_GROUP_ABOUT_TO_ARRIVE, 0}}};
	if (queue.scheduleBatch(events.data(), minutes > 30 ? 2 : 1) != CampaignEventQueueError::None)
		return {Code::EventQueueFailure, preflight};
	// No allocation or failing operation remains. This is the idle, worldless,
	// on-foot subset of native departure; normal arrival processing owns the rest.
	group->pWaypoints = prepared.waypoint; prepared.waypoint = nullptr;
	group->ubNextWaypointID = 0;
	group->ubNextX = destinationX; group->ubNextY = destinationY;
	group->uiTraverseTime = minutes;
	SetGroupArrivalTime(group, arrival);
	group->fBetweenSectors = TRUE;
	for (std::size_t i = 0; i < plan.memberCount; ++i)
	{
		members[i]->strategicPath().adopt(prepared.paths[i]); prepared.paths[i] = nullptr;
		members[i]->deployment().beginStrategicTransit();
		RemoveSoldierFromTacticalSector(members[i], FALSE);
		members[i]->deployment().strategicInsertionCode() = 0;
	}
	gfReEvaluateEveryonesNothingToDo = TRUE;
	return {Code::Started, preflight, arrival};
}
