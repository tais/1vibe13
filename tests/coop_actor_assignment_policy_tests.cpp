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

int main()
{
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
