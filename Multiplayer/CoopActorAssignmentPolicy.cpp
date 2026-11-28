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

bool CoopActorOwnershipLedger::handOver(const TacticalEntityId& actor, const PeerIdentity& from,
	const PeerIdentity& to) noexcept
{
	if (from == PeerIdentity{} || to == PeerIdentity{} || from == to) return false;
	for (std::size_t i = 0; i < lendCount_; ++i)
		if (lends_[i].actor == actor)
		{
			if (lends_[i].peerIdentity != from) return false;
			lends_[i].peerIdentity = to;
			return true;
		}
	for (std::size_t i = 0; i < ownerCount_; ++i)
		if (owners_[i].actor == actor)
		{
			if (owners_[i].peerIdentity != from) return false;
			owners_[i].peerIdentity = to;
			return true;
		}
	return false;
}

const PeerIdentity* CoopActorOwnershipLedger::ownerOf(const TacticalEntityId& actor) const noexcept
{
	for (std::size_t i = 0; i < ownerCount_; ++i)
		if (owners_[i].actor == actor) return &owners_[i].peerIdentity;
	return nullptr;
}

CoopActorAssignmentPolicyResult CoopActorOwnershipLedger::assign(
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
	std::array<CoopTacticalActorAssignment, MaximumCoopTacticalAssignments> staged{};
	if (peerCount == 0)
	{
		output = staged;
		outputCount = 0;
		return CoopActorAssignmentPolicyResult::Success;
	}
	auto owners = owners_;
	std::size_t ownerCount = ownerCount_;
	auto lends = lends_;
	std::size_t lendCount = lendCount_;
	const auto participant = [&](const PeerIdentity& peer) noexcept
	{
		for (std::size_t p = 0; p < peerCount; ++p)
			if (peers[p] == peer) return p;
		return peerCount;
	};
	const auto find = [](const auto& table, std::size_t count, const TacticalEntityId& actor) noexcept
	{
		for (std::size_t i = 0; i < count; ++i)
			if (table[i].actor == actor) return i;
		return count;
	};
	// 0. Seat players dealt for the first time with mercs present.
	auto seated = seated_;
	std::size_t seatedCount = seatedCount_;
	std::array<bool, MaximumCoopTacticalSessionPeers> joining{};
	for (std::size_t p = 0; actorCount != 0 && p < peerCount; ++p)
	{
		if (std::find(seated.begin(), seated.begin() + seatedCount, peers[p]) != seated.begin() + seatedCount)
			continue;
		joining[p] = true;
		if (seatedCount == seated.size())
		{
			// Full: forget the seat of a player who is not in this world.
			const auto stale = std::find_if(seated.begin(), seated.begin() + seatedCount,
				[&](const PeerIdentity& peer) { return participant(peer) == peerCount; });
			if (stale == seated.begin() + seatedCount) return CoopActorAssignmentPolicyResult::CapacityReached;
			std::move(stale + 1, seated.begin() + seatedCount, stale);
			--seatedCount;
		}
		seated[seatedCount++] = peers[p];
	}
	std::array<std::size_t, MaximumCoopTacticalSessionPeers> present{};
	std::array<std::size_t, MaximumCoopTacticalAssignments> dealt{};
	// 1. Recorded owners who take part in this world, then this world's lends.
	for (std::size_t a = 0; a < actorCount; ++a)
	{
		dealt[a] = peerCount;
		const std::size_t owned = find(owners, ownerCount, actors[a]);
		if (owned != ownerCount) dealt[a] = participant(owners[owned].peerIdentity);
		if (dealt[a] == peerCount)
		{
			const std::size_t lent = find(lends, lendCount, actors[a]);
			if (lent != lendCount) dealt[a] = participant(lends[lent].peerIdentity);
		}
		if (dealt[a] != peerCount) ++present[dealt[a]];
	}
	// 2. Everyone else, in actor order, to the participant with the fewest.
	for (std::size_t a = 0; a < actorCount; ++a)
	{
		if (dealt[a] != peerCount) continue;
		std::size_t fewest = 0;
		for (std::size_t p = 1; p < peerCount; ++p)
			if (present[p] < present[fewest]) fewest = p;
		dealt[a] = fewest;
		++present[fewest];
		const std::size_t owned = find(owners, ownerCount, actors[a]);
		if (owned != ownerCount)
		{
			// Owned by someone absent from this world: lend it for the world.
			const std::size_t lent = find(lends, lendCount, actors[a]);
			if (lent == lendCount)
			{
				if (lendCount == lends.size()) return CoopActorAssignmentPolicyResult::CapacityReached;
				lends[lendCount++] = {actors[a], peers[fewest]};
			}
			else lends[lent].peerIdentity = peers[fewest];
			continue;
		}
		if (ownerCount == owners.size())
		{
			// Full: forget the owner of a merc that is not in this world.
			std::size_t evict = ownerCount;
			for (std::size_t i = 0; i < ownerCount && evict == ownerCount; ++i)
				if (!std::any_of(actors, actors + actorCount,
					[&](const TacticalEntityId& actor) { return actor == owners[i].actor; }))
					evict = i;
			if (evict == ownerCount) return CoopActorAssignmentPolicyResult::CapacityReached;
			std::move(owners.begin() + evict + 1, owners.begin() + ownerCount, owners.begin() + evict);
			--ownerCount;
		}
		owners[ownerCount++] = {actors[a], peers[fewest]};
	}
	// 3. A joining player's fair share: the last mercs of whoever holds the
	// most move to them (an owned merc changes owner, a lent one its lend)
	// until no one holds more than one above them. On a first deal step 2 is
	// already that balanced, so nothing moves.
	std::size_t shared = 0;
	for (std::size_t p = 0; p < peerCount; ++p)
	{
		if (!joining[p]) continue;
		for (;;)
		{
			std::size_t most = 0;
			for (std::size_t q = 1; q < peerCount; ++q)
				if (present[q] > present[most]) most = q;
			if (present[most] <= present[p] + 1) break;
			std::size_t a = actorCount;
			while (dealt[--a] != most) {}
			dealt[a] = p;
			--present[most];
			++present[p];
			++shared;
			const std::size_t owned = find(owners, ownerCount, actors[a]);
			if (owned != ownerCount && owners[owned].peerIdentity == peers[most])
				owners[owned].peerIdentity = peers[p];
			else
			{
				const std::size_t lent = find(lends, lendCount, actors[a]);
				if (lent != lendCount) lends[lent].peerIdentity = peers[p];
			}
		}
	}
	for (std::size_t a = 0; a < actorCount; ++a)
		staged[a] = {actors[a], peers[dealt[a]]};
	owners_ = owners;
	ownerCount_ = ownerCount;
	lends_ = lends;
	lendCount_ = lendCount;
	seated_ = seated;
	seatedCount_ = seatedCount;
	joinShares_ += shared;
	output = staged;
	outputCount = actorCount;
	return CoopActorAssignmentPolicyResult::Success;
}
}
