#include "CoopActorAssignmentPolicy.h"

#include <algorithm>

namespace CoopSession
{
namespace
{
bool ValidPeers(const PeerIdentity* peers, std::size_t count) noexcept
{
	if (count > MaximumCoopTacticalSessionPeers ||
		(count != 0 && peers == nullptr))
		return false;
	for (std::size_t index = 0; index < count; ++index)
	{
		if (std::all_of(peers[index].begin(), peers[index].end(),
			[](std::uint8_t byte) { return byte == 0; })) return false;
		if (index != 0 && !(peers[index - 1] < peers[index])) return false;
	}
	return true;
}

bool ValidActors(const TacticalEntityId* actors, std::size_t count) noexcept
{
	if (count > MaximumCoopTacticalAssignments ||
		(count != 0 && actors == nullptr))
		return false;
	for (std::size_t index = 0; index < count; ++index)
	{
		if (!actors[index].valid()) return false;
		if (index != 0 && (!(actors[index - 1] < actors[index]) ||
			actors[index - 1].slot == actors[index].slot))
			return false;
	}
	return true;
}
}

CoopWorldParticipantPolicyResult GrowCoopWorldParticipants(
	const PeerIdentity* current,
	std::size_t currentCount,
	const PeerIdentity* ready,
	std::size_t readyCount,
	bool freshBaselineBoundary,
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers>& output,
	std::size_t& outputCount) noexcept
{
	if (!ValidPeers(current, currentCount))
		return CoopWorldParticipantPolicyResult::InvalidCurrentSet;
	if (!ValidPeers(ready, readyCount))
		return CoopWorldParticipantPolicyResult::InvalidReadySet;

	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> staged{};
	std::size_t stagedCount = 0;
	std::size_t currentIndex = 0;
	std::size_t readyIndex = 0;
	while (currentIndex < currentCount || readyIndex < readyCount)
	{
		PeerIdentity candidate{};
		if (readyIndex == readyCount ||
			(currentIndex < currentCount &&
				current[currentIndex] < ready[readyIndex]))
		{
			candidate = current[currentIndex++];
		}
		else if (currentIndex == currentCount ||
			ready[readyIndex] < current[currentIndex])
		{
			candidate = ready[readyIndex++];
		}
		else
		{
			candidate = current[currentIndex++];
			++readyIndex;
		}
		if (stagedCount == staged.size())
			return CoopWorldParticipantPolicyResult::CapacityReached;
		staged[stagedCount++] = candidate;
	}

	if (stagedCount == currentCount &&
		(currentCount == 0 ||
			std::equal(staged.begin(), staged.begin() + stagedCount, current)))
	{
		return CoopWorldParticipantPolicyResult::Unchanged;
	}
	if (currentCount != 0 && !freshBaselineBoundary)
		return CoopWorldParticipantPolicyResult::DeferredUntilFreshBaseline;

	output = staged;
	outputCount = stagedCount;
	return CoopWorldParticipantPolicyResult::Published;
}

CoopActorAssignmentPolicyResult BuildCoopActorAssignments(
	const PeerIdentity* peers,
	std::size_t peerCount,
	const TacticalEntityId* actors,
	std::size_t actorCount,
	std::array<CoopTacticalActorAssignment,
		MaximumCoopTacticalAssignments>& output,
	std::size_t& outputCount) noexcept
{
	if (!ValidPeers(peers, peerCount))
		return CoopActorAssignmentPolicyResult::InvalidPeerSet;
	if (actorCount > MaximumCoopTacticalAssignments)
		return CoopActorAssignmentPolicyResult::CapacityReached;
	if (!ValidActors(actors, actorCount))
		return CoopActorAssignmentPolicyResult::InvalidActorSet;

	std::array<CoopTacticalActorAssignment,
		MaximumCoopTacticalAssignments> staged{};
	const std::size_t assignedCount = peerCount == 0 ? 0 : actorCount;
	for (std::size_t index = 0; index < assignedCount; ++index)
	{
		staged[index].actor = actors[index];
		staged[index].peerIdentity = peers[index % peerCount];
	}
	output = staged;
	outputCount = assignedCount;
	return CoopActorAssignmentPolicyResult::Success;
}
}
