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

bool SameDedicatedCoopExitGridPlan(const DedicatedCoopExitGridPlan& a, const DedicatedCoopExitGridPlan& b) noexcept
{
	if (a.group != b.group || a.sourceGrid != b.sourceGrid || a.destinationGrid != b.destinationGrid ||
		a.x != b.x || a.y != b.y || a.z != b.z || a.originX != b.originX || a.originY != b.originY ||
		a.originZ != b.originZ || a.memberCount != b.memberCount) return false;
	for (std::size_t i = 0; i < a.memberCount; ++i)
		if (a.members[i] != b.members[i]) return false;
	return true;
}

DedicatedCoopExitGridCode PlanDedicatedCoopExitGridTraversal(StrategicGroupId identity,
	DedicatedCoopExitGridPlan& output) noexcept
{
	using Code = DedicatedCoopExitGridCode;
	try
	{
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
		DedicatedCoopExitGridPlan plan;
		plan.group = identity;
		plan.originX = group->ubSectorX; plan.originY = group->ubSectorY; plan.originZ = group->ubSectorZ;
		bool haveExit = false;
		for (PLAYERGROUP* entry = group->pPlayerList; entry; entry = entry->next)
		{
			TacticalActor* const member = ResolvePlayerGroupMember(entry);
			if (!member) return Code::GroupUnavailable;
			if (plan.memberCount == plan.members.size()) return Code::UnsupportedGroup;
			// The whole group moves; unconscious, vehicle or busy members must be
			// handled (moved to their own squad) by the players first.
			if (!OK_CONTROLLABLE_MERC(member) || !member->roster().inSector() ||
				member->vitals().health() < OKLIFE || member->assignment().current() == VEHICLE ||
				(member->status().flags() & (SOLDIER_VEHICLE | SOLDIER_DRIVER | SOLDIER_PASSENGER)) != 0 ||
				member->deployment().groupId() != group->ubGroupID)
				return Code::MemberNotReady;
			plan.members[plan.memberCount++] = GetJa2TacticalEntityId(*member);
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
		if (!plan.memberCount) return Code::GroupUnavailable;
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
			member->deployment().strategicInsertionData() = plan.destinationGrid;
			member->deployment().useExitGridForReentryDirection() = TRUE;
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
