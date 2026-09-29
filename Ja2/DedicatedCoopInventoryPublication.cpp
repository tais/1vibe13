#include "DedicatedCoopInventoryPublication.h"
#include "DedicatedCoopTacticalHost.h"
#include "FullEngineCoopTacticalServer.h"

bool StageDedicatedCoopOwnerInventories(
	const DedicatedCoopTacticalJa2LiveState& live,
	CoopSession::FullEngineCoopTacticalServer& server,
	std::uint64_t worldGeneration) noexcept
{
	using namespace CoopSession;
	const auto& replication = server.replication();
	if (!live.onMainThread() || !server.worldActive() || worldGeneration == 0 ||
		replication.worldGeneration() != worldGeneration) return false;
	for (std::size_t index = 0; index < replication.assignmentCount(); ++index)
	{
		const CoopTacticalActorAssignment* assignment = replication.assignment(index);
		if (assignment == nullptr) return false;
		CoopTacticalPeerReplicationState peer;
		if (!replication.peerState(assignment->peerIdentity, peer) ||
			!peer.connected || peer.baselineId == 0 ||
			(peer.phase != CoopTacticalPeerPhase::Active &&
				peer.phase != CoopTacticalPeerPhase::AwaitingBaselineAck)) continue;
		CoopOwnerInventorySnapshot inventory;
		if (!live.captureInventory(assignment->actor, worldGeneration, inventory) ||
			server.stageInventory(assignment->peerIdentity, inventory) !=
				FullEngineCoopTacticalServerResult::Success) return false;
	}
	return true;
}
