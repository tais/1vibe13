#include "Multiplayer/CoopActorAssignmentPolicy.h"

#include <array>
#include <cstdint>
#include <cstdio>

using namespace CoopSession;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); \
		++failures; \
	} \
} while (false)

PeerIdentity Peer(std::uint8_t first)
{
	PeerIdentity peer{};
	peer[0] = first;
	return peer;
}

void TestBalancedCanonicalAssignment()
{
	const std::array<PeerIdentity, 3> peers{Peer(1), Peer(2), Peer(3)};
	const std::array<TacticalEntityId, 7> actors{{
		{1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}}};
	std::array<CoopTacticalActorAssignment,
		MaximumCoopTacticalAssignments> assignments{};
	std::size_t count = 99;
	CHECK(BuildCoopActorAssignments(peers.data(), peers.size(),
		actors.data(), actors.size(), assignments, count) ==
		CoopActorAssignmentPolicyResult::Success,
		"canonical roster is assigned");
	CHECK(count == actors.size(), "every actor is assigned");
	for (std::size_t index = 0; index < count; ++index)
	{
		CHECK(assignments[index].actor == actors[index],
			"assignment preserves canonical actor order");
		CHECK(assignments[index].peerIdentity == peers[index % peers.size()],
			"assignment is deterministic round-robin");
	}
}

void TestNoPeerProducesNoAcl()
{
	const std::array<TacticalEntityId, 2> actors{{{1, 1}, {2, 1}}};
	std::array<CoopTacticalActorAssignment,
		MaximumCoopTacticalAssignments> assignments{};
	assignments[0].actor = {99, 1};
	std::size_t count = 7;
	CHECK(BuildCoopActorAssignments(nullptr, 0, actors.data(), actors.size(),
		assignments, count) == CoopActorAssignmentPolicyResult::Success,
		"an empty selected roster is valid");
	CHECK(count == 0, "no peer yields no actor authority");
	CHECK(!assignments[0].actor.valid(),
		"successful empty result clears stale output");
}

void TestInvalidInputsPreserveOutputs()
{
	std::array<CoopTacticalActorAssignment,
		MaximumCoopTacticalAssignments> assignments{};
	assignments[0].actor = {77, 3};
	assignments[0].peerIdentity = Peer(9);
	std::size_t count = 1;
	const std::size_t preservedCount = count;
	auto outputPreserved = [&]() noexcept {
		return assignments[0].actor == TacticalEntityId{77, 3} &&
			assignments[0].peerIdentity == Peer(9) &&
			count == preservedCount;
	};

	const std::array<PeerIdentity, 2> duplicatePeers{Peer(1), Peer(1)};
	const TacticalEntityId actor{1, 1};
	CHECK(BuildCoopActorAssignments(duplicatePeers.data(),
		duplicatePeers.size(), &actor, 1, assignments, count) ==
		CoopActorAssignmentPolicyResult::InvalidPeerSet,
		"duplicate peers fail closed");
	CHECK(outputPreserved(),
		"peer failure preserves output");

	const PeerIdentity peer = Peer(1);
	const std::array<TacticalEntityId, 2> duplicateSlots{{{1, 1}, {1, 2}}};
	CHECK(BuildCoopActorAssignments(&peer, 1, duplicateSlots.data(),
		duplicateSlots.size(), assignments, count) ==
		CoopActorAssignmentPolicyResult::InvalidActorSet,
		"two incarnations of one slot fail closed");
	CHECK(outputPreserved(),
		"actor failure preserves output");

	CHECK(BuildCoopActorAssignments(nullptr, 1, &actor, 1,
		assignments, count) ==
		CoopActorAssignmentPolicyResult::InvalidPeerSet,
		"null nonempty peer input fails closed");
	CHECK(outputPreserved(),
		"null input preserves output");
}

void TestGrowOnlyWorldParticipants()
{
	const std::array<PeerIdentity, 2> current{Peer(2), Peer(4)};
	const std::array<PeerIdentity, 3> ready{Peer(1), Peer(2), Peer(3)};
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> participants{};
	participants[0] = Peer(9);
	std::size_t count = 1;

	CHECK(GrowCoopWorldParticipants(current.data(), current.size(),
		ready.data(), ready.size(), false, participants, count) ==
		CoopWorldParticipantPolicyResult::DeferredUntilFreshBaseline,
		"late identities wait for a fresh-baseline boundary");
	CHECK(count == 1 && participants[0] == Peer(9),
		"deferred growth preserves caller output");

	CHECK(GrowCoopWorldParticipants(current.data(), current.size(),
		ready.data(), ready.size(), true, participants, count) ==
		CoopWorldParticipantPolicyResult::Published,
		"late identities publish at a fresh-baseline boundary");
	CHECK(count == 4 && participants[0] == Peer(1) &&
		participants[1] == Peer(2) && participants[2] == Peer(3) &&
		participants[3] == Peer(4),
		"participant growth is a canonical union");

	const std::array<PeerIdentity, 1> onlyReconnect{Peer(2)};
	CHECK(GrowCoopWorldParticipants(current.data(), current.size(),
		onlyReconnect.data(), onlyReconnect.size(), false,
		participants, count) == CoopWorldParticipantPolicyResult::Unchanged,
		"disconnects never remove retained participant identities");
}

void TestInitialParticipantsAndMalformedGrowth()
{
	const std::array<PeerIdentity, 2> ready{Peer(1), Peer(3)};
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> participants{};
	std::size_t count = 0;
	CHECK(GrowCoopWorldParticipants(nullptr, 0, ready.data(), ready.size(),
		false, participants, count) ==
		CoopWorldParticipantPolicyResult::Published &&
		count == ready.size() && participants[0] == ready[0] &&
		participants[1] == ready[1],
		"an empty world roster captures its first ready cohort immediately");

	const std::array<PeerIdentity, 2> duplicate{Peer(1), Peer(1)};
	const auto preserved = participants;
	const std::size_t preservedCount = count;
	CHECK(GrowCoopWorldParticipants(ready.data(), ready.size(),
		duplicate.data(), duplicate.size(), true, participants, count) ==
		CoopWorldParticipantPolicyResult::InvalidReadySet,
		"malformed ready identities fail closed");
	CHECK(participants == preserved && count == preservedCount,
		"malformed growth preserves output");

	const std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> full{
		Peer(1), Peer(2), Peer(3), Peer(4)};
	const PeerIdentity fifth = Peer(5);
	CHECK(GrowCoopWorldParticipants(full.data(), full.size(), &fifth, 1,
		true, participants, count) ==
		CoopWorldParticipantPolicyResult::CapacityReached,
		"participant growth cannot exceed authority capacity");
	CHECK(participants == preserved && count == preservedCount,
		"capacity failure preserves output");
}
}

using Deal = std::array<CoopTacticalActorAssignment, MaximumCoopTacticalAssignments>;

bool SameDeal(const Deal& a, const Deal& b)
{
	for (std::size_t i = 0; i < a.size(); ++i)
		if (a[i].actor != b[i].actor || a[i].peerIdentity != b[i].peerIdentity) return false;
	return true;
}

PeerIdentity OwnerIn(const Deal& deal, std::size_t count, TacticalEntityId actor)
{
	for (std::size_t i = 0; i < count; ++i)
		if (deal[i].actor == actor) return deal[i].peerIdentity;
	return Peer(0);
}

void TestStableOwnershipLedger()
{
	const std::array<PeerIdentity, 2> peers{Peer(1), Peer(2)};
	const TacticalEntityId m1{1, 1}, m2{2, 1}, m3{3, 1}, m4{4, 1}, m5{5, 1};
	CoopActorOwnershipLedger ledger;
	Deal deal{}; std::size_t count = 0;
	const std::array<TacticalEntityId, 4> four{m1, m2, m3, m4};
	CHECK(ledger.assign(peers.data(), peers.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && count == 4, "first deal succeeds");
	CHECK(OwnerIn(deal, count, m1) == Peer(1) && OwnerIn(deal, count, m2) == Peer(2) &&
		OwnerIn(deal, count, m3) == Peer(1) && OwnerIn(deal, count, m4) == Peer(2),
		"a first deal is the round-robin");
	// The old round-robin over {m1, m3, m4} would flip m3 and m4.
	const std::array<TacticalEntityId, 3> downed{m1, m3, m4};
	Deal plain{}; std::size_t plainCount = 0;
	(void)BuildCoopActorAssignments(peers.data(), peers.size(), downed.data(), downed.size(), plain, plainCount);
	CHECK(OwnerIn(plain, plainCount, m3) == Peer(2) && OwnerIn(plain, plainCount, m4) == Peer(1),
		"control: the plain round-robin re-deals when a merc drops out");
	CHECK(ledger.assign(peers.data(), peers.size(), downed.data(), downed.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m3) == Peer(1) &&
		OwnerIn(deal, count, m4) == Peer(2), "a merc going down changes nobody else's owner");
	CHECK(ledger.assign(peers.data(), peers.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m2) == Peer(2),
		"a merc getting up returns to its owner");
	// A newly hired merc goes to the player with fewer present mercs.
	const std::array<TacticalEntityId, 4> withNew{m1, m3, m4, m5};
	CHECK(ledger.assign(peers.data(), peers.size(), withNew.data(), withNew.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m5) == Peer(2),
		"a new merc goes to the player with the fewest present");
	CHECK(ledger.ownerOf(m5) && *ledger.ownerOf(m5) == Peer(2), "and is recorded for good");
	// A world without player 2: their mercs are lent to player 1, not given.
	ledger.beginWorld();
	const std::array<PeerIdentity, 1> alone{Peer(1)};
	CHECK(ledger.assign(alone.data(), alone.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m2) == Peer(1) &&
		*ledger.ownerOf(m2) == Peer(2), "an absent owner's mercs are lent for the world, ownership kept");
	ledger.beginWorld();
	CHECK(ledger.assign(peers.data(), peers.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m2) == Peer(2) &&
		OwnerIn(deal, count, m4) == Peer(2), "the owner back in a later world gets them back");
	// Transactional failure.
	const std::array<TacticalEntityId, 2> unsorted{m3, m1};
	const auto before = deal; const auto beforeCount = count; const auto owners = ledger.ownerCount();
	CHECK(ledger.assign(peers.data(), peers.size(), unsorted.data(), unsorted.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::InvalidActorSet && SameDeal(deal, before) && count == beforeCount &&
		ledger.ownerCount() == owners, "a malformed deal changes nothing");
	CHECK(ledger.assign(nullptr, 0, four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && count == 0 && ledger.ownerCount() == owners,
		"no participants deal nothing and record nothing");
}

int main()
{
	TestStableOwnershipLedger();
	TestBalancedCanonicalAssignment();
	TestNoPeerProducesNoAcl();
	TestInvalidInputsPreserveOutputs();
	TestGrowOnlyWorldParticipants();
	TestInitialParticipantsAndMalformedGrowth();
	if (failures != 0)
	{
		std::printf("%d co-op actor assignment policy test(s) failed\n",
			failures);
		return 1;
	}
	std::printf("co-op actor assignment policy tests passed\n");
	return 0;
}
