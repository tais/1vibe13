#include "DedicatedCoopExitGrid.h"
#include "StrategicGroupHost.h"
#include "TacticalEntityHost.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "Exit Grids.h"
#include "TacticalWorldAdapter.h"
#include "PreBattle Interface.h"
#include "Isometric Utils.h"
#include "strategic.h"
#include "Strategic Movement.h"
#include "Queen Command.h"
#include "strategicmap.h"
#include "Soldier macros.h"
#include "Overhead.h"
#include "Assignments.h"
#include "screenids.h"
#include "sgp.h"
#include "Soldier Profile.h"
#include "SoldierRepository.h"

bool SameDedicatedCoopExitGridPlan(const DedicatedCoopExitGridPlan& a, const DedicatedCoopExitGridPlan& b) noexcept
{
	if (a.group != b.group || a.sourceGrid != b.sourceGrid || a.destinationGrid != b.destinationGrid ||
		a.x != b.x || a.y != b.y || a.z != b.z || a.originX != b.originX || a.originY != b.originY ||
		a.originZ != b.originZ || a.memberCount != b.memberCount || a.scripted != b.scripted) return false;
	for (std::size_t i = 0; i < a.memberCount; ++i)
		if (a.members[i] != b.members[i] || a.insertionGrids[i] != b.insertionGrids[i]) return false;
	return true;
}

namespace
{
// Whole-group readiness shared by exit-grid and scripted traversals: the group
// is in the loaded sector and every member is conscious, controllable, on foot
// and in it. Fills origin and members only.
DedicatedCoopExitGridCode CaptureTraversalGroup(StrategicGroupId identity, DedicatedCoopExitGridPlan& plan)
{
	using Code = DedicatedCoopExitGridCode;
	if (!IsJa2TacticalWorldLoaded() || GetCurrentScreen() != GAME_SCREEN || gfTacticalTraversal ||
		IsJa2TacticalCombatActive() || gWorldSectorX < 1 || gWorldSectorX > 16 ||
		gWorldSectorY < 1 || gWorldSectorY > 16 || gbWorldSectorZ < 0 || gbWorldSectorZ > 3)
		return Code::WorldUnavailable;
	GROUP* const group = identity.valid() ? ResolveJa2StrategicGroup(identity) : nullptr;
	if (!group || group->usGroupTeam != OUR_TEAM) return Code::GroupUnavailable;
	if (group->fVehicle || group->ubTransportationMask != FOOT || group->fBetweenSectors || group->pWaypoints)
		return Code::UnsupportedGroup;
	if (group->ubSectorX != gWorldSectorX || group->ubSectorY != gWorldSectorY ||
		group->ubSectorZ != static_cast<UINT8>(gbWorldSectorZ)) return Code::GroupUnavailable;
	plan.group = identity;
	plan.originX = group->ubSectorX; plan.originY = group->ubSectorY; plan.originZ = group->ubSectorZ;
	for (PLAYERGROUP* entry = group->pPlayerList; entry; entry = entry->next)
	{
		TacticalActor* const member = ResolvePlayerGroupMember(entry);
		if (!member) return Code::GroupUnavailable;
		if (plan.memberCount == plan.members.size()) return Code::UnsupportedGroup;
		if (!OK_CONTROLLABLE_MERC(member) || !member->roster().inSector() ||
			member->vitals().health() < OKLIFE || member->assignment().current() == VEHICLE ||
			(member->status().flags() & (SOLDIER_VEHICLE | SOLDIER_DRIVER | SOLDIER_PASSENGER)) != 0 ||
			member->deployment().groupId() != group->ubGroupID)
			return Code::MemberNotReady;
		plan.members[plan.memberCount++] = GetJa2TacticalEntityId(*member);
	}
	return plan.memberCount ? Code::Planned : Code::GroupUnavailable;
}
}

DedicatedCoopExitGridCode PlanDedicatedCoopScriptedTraversal(StrategicGroupId identity,
	std::uint8_t x, std::uint8_t y, std::uint8_t z, const std::int32_t* grids, std::size_t gridCount,
	DedicatedCoopExitGridPlan& output) noexcept
{
	using Code = DedicatedCoopExitGridCode;
	try
	{
		DedicatedCoopExitGridPlan plan;
		const Code captured = CaptureTraversalGroup(identity, plan);
		if (captured != Code::Planned) return captured;
		if (!grids || !gridCount || x < 1 || x > 16 || y < 1 || y > 16 || z > 3 ||
			(x == plan.originX && y == plan.originY && z == plan.originZ))
			return Code::InvalidDestination;
		for (std::size_t i = 0; i < plan.memberCount; ++i)
		{
			plan.insertionGrids[i] = grids[i < gridCount ? i : gridCount - 1];
			if (TileIsOutOfBounds(plan.insertionGrids[i])) return Code::InvalidDestination;
		}
		if (NumHostilesInSector(x, y, z) != 0) return Code::HostileDestination;
		plan.scripted = true;
		plan.destinationGrid = plan.insertionGrids[0];
		plan.x = x; plan.y = y; plan.z = z;
		output = plan;
		return Code::Planned;
	}
	catch (...)
	{
		return Code::WorldUnavailable;
	}
}

DedicatedCoopExitGridCode ReplanDedicatedCoopTraversal(const DedicatedCoopExitGridPlan& accepted,
	DedicatedCoopExitGridPlan& fresh) noexcept
{
	if (!accepted.scripted) return PlanDedicatedCoopExitGridTraversal(accepted.group, fresh);
	return PlanDedicatedCoopScriptedTraversal(accepted.group, accepted.x, accepted.y, accepted.z,
		accepted.insertionGrids.data(), accepted.memberCount, fresh);
}

StrategicGroupId DedicatedCoopLargestInSectorGroup() noexcept
{
	try
	{
		std::array<std::pair<UINT8, unsigned>, 32> counts{};
		std::size_t used = 0;
		for (SoldierID id = gTacticalStatus.Team[OUR_TEAM].bFirstID; id <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++id)
		{
			const TacticalActor* const merc = GetJa2SoldierRepository().resolve(id);
			if (!merc || !OK_CONTROLLABLE_MERC(merc) || !merc->roster().inSector() ||
				merc->vitals().health() < OKLIFE || !merc->deployment().groupId()) continue;
			const UINT8 group = merc->deployment().groupId();
			std::size_t at = 0;
			while (at < used && counts[at].first != group) ++at;
			if (at == used)
			{
				if (used == counts.size()) continue;
				counts[used++] = {group, 0};
			}
			++counts[at].second;
		}
		const std::pair<UINT8, unsigned>* best = nullptr;
		for (std::size_t i = 0; i < used; ++i)
			if (!best || counts[i].second > best->second ||
				(counts[i].second == best->second && counts[i].first < best->first)) best = &counts[i];
		return best ? GetJa2StrategicGroupId(best->first) : StrategicGroupId{};
	}
	catch (...)
	{
		return {};
	}
}

StrategicGroupId DedicatedCoopGroupNearestToProfile(std::uint8_t profile) noexcept
{
	try
	{
		const TacticalActor* const npc = FindSoldierByProfileID(profile, FALSE);
		if (!npc || !npc->roster().active() || !npc->roster().inSector()) return {};
		const TacticalActor* best = nullptr;
		INT16 bestDistance = 0;
		for (SoldierID id = gTacticalStatus.Team[OUR_TEAM].bFirstID; id <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++id)
		{
			const TacticalActor* const merc = GetJa2SoldierRepository().resolve(id);
			if (!merc || !OK_CONTROLLABLE_MERC(merc) || !merc->roster().inSector() ||
				merc->vitals().health() < OKLIFE || merc->position().level() != npc->position().level()) continue;
			const INT16 distance = PythSpacesAway(merc->position().gridNo(), npc->position().gridNo());
			if (best && distance >= bestDistance) continue;
			best = merc; bestDistance = distance;
		}
		return best ? GetJa2StrategicGroupId(best->deployment().groupId()) : StrategicGroupId{};
	}
	catch (...)
	{
		return {};
	}
}

DedicatedCoopExitGridCode PlanDedicatedCoopExitGridTraversal(StrategicGroupId identity,
	DedicatedCoopExitGridPlan& output) noexcept
{
	using Code = DedicatedCoopExitGridCode;
	try
	{
		DedicatedCoopExitGridPlan plan;
		const Code captured = CaptureTraversalGroup(identity, plan);
		if (captured != Code::Planned) return captured;
		bool haveExit = false;
		for (std::size_t i = 0; i < plan.memberCount; ++i)
		{
			const TacticalActor* const member = ResolveJa2TacticalEntity(plan.members[i]);
			if (!member) return Code::GroupUnavailable;
			EXITGRID exit;
			const INT32 grid = member->position().gridNo();
			if (TileIsOutOfBounds(grid) || !GetExitGrid(static_cast<UINT32>(grid), &exit)) continue;
			if (haveExit && (exit.ubGotoSectorX != plan.x || exit.ubGotoSectorY != plan.y ||
				exit.ubGotoSectorZ != plan.z || exit.usGridNo != plan.destinationGrid))
				return Code::AmbiguousExitGrid;
			if (!haveExit)
			{
				haveExit = true;
				plan.sourceGrid = grid;
				plan.destinationGrid = exit.usGridNo;
				plan.x = exit.ubGotoSectorX; plan.y = exit.ubGotoSectorY; plan.z = exit.ubGotoSectorZ;
			}
		}
		if (!haveExit) return Code::NotOnExitGrid;
		if (plan.x < 1 || plan.x > 16 || plan.y < 1 || plan.y > 16 || plan.z > 3 ||
			TileIsOutOfBounds(plan.destinationGrid) ||
			(plan.x == gWorldSectorX && plan.y == gWorldSectorY && plan.z == static_cast<UINT8>(gbWorldSectorZ)))
			return Code::InvalidDestination;
		// Hostile destinations need native battle entry; this path is peaceful only.
		if (NumHostilesInSector(plan.x, plan.y, plan.z) != 0) return Code::HostileDestination;
		output = plan;
		return Code::Planned;
	}
	catch (...)
	{
		return Code::WorldUnavailable;
	}
}

DedicatedCoopExitGridCode ApplyDedicatedCoopExitGridRelocation(const DedicatedCoopExitGridPlan& plan,
	StrategicGroupId& resultGroup) noexcept
{
	using Code = DedicatedCoopExitGridCode;
	try
	{
		if (IsJa2TacticalWorldLoaded() || gfTacticalTraversal) return Code::WorldUnavailable;
		GROUP* const group = plan.group.valid() ? ResolveJa2StrategicGroup(plan.group) : nullptr;
		if (!group || group->usGroupTeam != OUR_TEAM || group->fVehicle || group->ubTransportationMask != FOOT ||
			group->fBetweenSectors || group->pWaypoints || group->ubSectorX != plan.originX ||
			group->ubSectorY != plan.originY || group->ubSectorZ != plan.originZ || !plan.memberCount)
			return Code::Changed;
		std::size_t members = 0;
		for (PLAYERGROUP* entry = group->pPlayerList; entry; entry = entry->next) ++members;
		if (members != plan.memberCount) return Code::Changed;
		for (std::size_t i = 0; i < plan.memberCount; ++i)
		{
			TacticalActor* const member = ResolveJa2TacticalEntity(plan.members[i]);
			if (!member || member->deployment().groupId() != group->ubGroupID ||
				member->deployment().sectorX() != plan.originX || member->deployment().sectorY() != plan.originY ||
				member->deployment().sectorZ() != plan.originZ || member->vitals().health() < OKLIFE)
				return Code::Changed;
		}
		if (NumHostilesInSector(plan.x, plan.y, plan.z) != 0) return Code::HostileDestination;
		// The stock exit-grid insertion (SetInsertionDataFromAdjacentMoveDirection
		// case 255), using the destination grid captured while the map was loaded.
		for (std::size_t i = 0; i < plan.memberCount; ++i)
		{
			TacticalActor* const member = ResolveJa2TacticalEntity(plan.members[i]);
			member->deployment().strategicInsertionCode() = INSERTION_CODE_GRIDNO;
			// A scripted traversal uses its stock per-member grids and no
			// exit-grid facing (the hideout cutscene sets neither).
			member->deployment().strategicInsertionData() = plan.scripted ? plan.insertionGrids[i] : plan.destinationGrid;
			member->deployment().useExitGridForReentryDirection() = plan.scripted ? FALSE : TRUE;
		}
		SetGroupSectorValue(plan.x, plan.y, plan.z, group->ubGroupID);
		(void)AttemptToMergeSeparatedGroups(group, FALSE);
		TacticalActor* const first = ResolveJa2TacticalEntity(plan.members[0]);
		resultGroup = first ? GetJa2StrategicGroupId(first->deployment().groupId()) : StrategicGroupId{};
		return resultGroup.valid() ? Code::Planned : Code::GroupUnavailable;
	}
	catch (...)
	{
		return Code::GroupUnavailable;
	}
}
