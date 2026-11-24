#include "DedicatedCoopInventoryPublication.h"
#include "DedicatedCoopTacticalHost.h"
#include "FullEngineCoopTacticalServer.h"

bool StageDedicatedCoopOwnerInventories(
	const DedicatedCoopTacticalJa2LiveState& live,
	CoopSession::FullEngineCoopTacticalServer& server,
	std::uint64_t worldGeneration,
	DedicatedCoopOwnerInventoryStageFailure* failure,
	CoopSession::FullEngineCoopTacticalServerResult* stageResult) noexcept
{
	using namespace CoopSession;
	const auto stop = [&](DedicatedCoopOwnerInventoryStageFailure reason) noexcept {
		if (failure) *failure = reason;
		return false;
	};
	if (failure) *failure = DedicatedCoopOwnerInventoryStageFailure::None;
	const auto& replication = server.replication();
	if (!live.onMainThread() || !server.worldActive() || worldGeneration == 0 ||
		replication.worldGeneration() != worldGeneration)
		return stop(DedicatedCoopOwnerInventoryStageFailure::Context);
	for (std::size_t index = 0; index < replication.assignmentCount(); ++index)
	{
		const CoopTacticalActorAssignment* assignment = replication.assignment(index);
		if (assignment == nullptr)
			return stop(DedicatedCoopOwnerInventoryStageFailure::Assignment);
		CoopTacticalPeerReplicationState peer;
		if (!replication.peerState(assignment->peerIdentity, peer) ||
			!peer.connected || peer.baselineId == 0 ||
			(peer.phase != CoopTacticalPeerPhase::Active &&
				peer.phase != CoopTacticalPeerPhase::AwaitingBaselineAck)) continue;
		CoopOwnerInventorySnapshot inventory;
		if (!live.captureInventory(assignment->actor, worldGeneration, inventory))
			return stop(DedicatedCoopOwnerInventoryStageFailure::Capture);
		const FullEngineCoopTacticalServerResult result =
			server.stageInventory(assignment->peerIdentity, inventory);
		if (result != FullEngineCoopTacticalServerResult::Success)
		{
			if (stageResult) *stageResult = result;
			return stop(DedicatedCoopOwnerInventoryStageFailure::Stage);
		}
	}
	return true;
}
