#include "DedicatedCoopCampaignEconomy.h"
#include "DedicatedCoopCampaignGroups.h"
#include "CampaignLedger.h"
#include "LaptopSave.h"
#include "SoldierRepository.h"
#include "TacticalEntityHost.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "GameSettings.h"
#include "Merc Hiring.h"
#include "Assignments.h"
#include "Overhead.h"

const char* CaptureDedicatedCoopCampaignEconomy(
	CoopSession::CoopCampaignEconomy& output) noexcept
{
	using namespace CoopSession;
	auto& repository = GetJa2SoldierRepository();
	const auto first = gTacticalStatus.Team[OUR_TEAM].bFirstID.i;
	const auto last = gTacticalStatus.Team[OUR_TEAM].bLastID.i;
	if (gbPlayerNum != OUR_TEAM || first > last || last >= repository.capacity())
		return "invalid player team bounds";
	const int limit = static_cast<int>(OUR_TEAM_SIZE_NO_VEHICLE);
	if (limit <= 0 || limit > CODE_MAXIMUM_NUMBER_OF_PLAYER_MERCS)
		return "invalid player team capacity";
	// The native count and willingness readers require a complete slot table.
	// Validate it before either reader can dereference an unresolved slot.
	for (std::size_t slot = first; slot <= last; ++slot)
	{
		const auto* actor = repository.resolve(slot);
		if (!actor || !repository.contains(slot, *actor) ||
			(actor->roster().active() && actor->roster().team() != OUR_TEAM))
			return "invalid player team slot";
	}
	CampaignLedgerSnapshot finance;
	if (InspectFinanceLedger(finance) != CampaignLedgerError::None)
		return "finance ledger unavailable";
	if (finance.balance != LaptopSaveInfo.iCurrentBalance)
		return "finance ledger and native balance disagree";
	// This bounded reader also validates group-list cycles before identity
	// gateways traverse native links. Retain its exact public group identities.
	CoopCampaignGroups groups;
	if (CaptureDedicatedCoopCampaignGroups(groups))
		return "campaign groups unavailable";
	CoopCampaignEconomy captured;
	captured.available = true;
	captured.balance = finance.balance;
	captured.mercenaryCount = NumberOfMercsOnPlayerTeam();
	captured.mercenaryLimit = static_cast<std::uint16_t>(limit);
	for (std::size_t slot = 0; slot < repository.capacity(); ++slot)
	{
		const auto* source = repository.resolve(slot);
		if (!source || !source->roster().active() || source->roster().team() != OUR_TEAM) continue;
		if (slot < first || slot > last || captured.rosterCount == captured.roster.size())
			return "friendly roster exceeds player team";
		const auto& deployment = source->deployment();
		if (deployment.sectorX() < 1 || deployment.sectorX() > 16 ||
			deployment.sectorY() < 1 || deployment.sectorY() > 16 ||
			deployment.sectorZ() < 0 || deployment.sectorZ() > 3 ||
			source->employment().endTime() < 0)
			return "invalid friendly employment or sector";
		auto& actor = captured.roster[captured.rosterCount++];
		actor.actor = GetJa2TacticalEntityId(*source);
		if (!actor.actor.valid() || ResolveJa2TacticalEntity(actor.actor) != source)
			return "unresolved friendly actor identity";
		actor.profile = source->identity().profile();
		actor.assignment = source->assignment().current();
		actor.x = static_cast<std::uint8_t>(deployment.sectorX());
		actor.y = static_cast<std::uint8_t>(deployment.sectorY());
		actor.z = static_cast<std::uint8_t>(deployment.sectorZ());
		actor.pendingHire = actor.assignment == IN_TRANSIT;
		actor.betweenSectors = deployment.isBetweenSectors();
		actor.inSector = source->roster().inSector() != FALSE;
		actor.vehicle = (source->status().flags() & SOLDIER_VEHICLE) != 0;
		actor.arrivalMinutes = actor.pendingHire ? deployment.arrivalTime() : 0;
		actor.contractEndMinutes = static_cast<std::uint32_t>(source->employment().endTime());
		if (deployment.groupId())
		{
			for (std::size_t i = 0; i < groups.groupCount; ++i)
			{
				const auto& group = groups.groups[i];
				if (group.id.slot != deployment.groupId()) continue;
				for (std::size_t member = group.firstMember; member < group.firstMember + group.memberCount; ++member)
					if (groups.members[member].actor == actor.actor) actor.group = group.id;
			}
			if (!actor.group.valid()) return "unresolved friendly movement group";
		}
	}
	captured.sessionEpoch = 1; captured.revision = 1;
	if (!ValidCoopCampaignEconomy(captured)) return "invalid campaign economy projection";
	captured.sessionEpoch = 0; captured.revision = 0;
	output = captured;
	return nullptr;
}
