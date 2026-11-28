#include "Multiplayer/CoopActorAssignmentPolicy.h"

#include <array>
#include <cstdint>
#include <chrono>
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
	// Hand-over: an owner's merc changes owner for good; a lend moves.
	CHECK(!ledger.handOver(m1, Peer(2), Peer(1)), "only the controlling player can give a merc away");
	CHECK(!ledger.handOver(m1, Peer(1), Peer(1)), "giving a merc to oneself changes nothing");
	CHECK(ledger.handOver(m1, Peer(1), Peer(2)) && *ledger.ownerOf(m1) == Peer(2), "the owner gives it away");
	CHECK(ledger.assign(peers.data(), peers.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m1) == Peer(2) &&
		OwnerIn(deal, count, m3) == Peer(1), "the next deal follows the hand-over and nothing else");
	ledger.beginWorld();
	CHECK(ledger.assign(alone.data(), alone.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m2) == Peer(1), "lent while player 2 is away");
	CHECK(ledger.handOver(m2, Peer(1), Peer(3)) && *ledger.ownerOf(m2) == Peer(2),
		"a lent merc's lend moves; its owner stays");
	ledger.beginWorld();
	(void)ledger.handOver(m1, Peer(2), Peer(1));
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

// The playtest: the battle starts once the first player is ready, so the
// second arrives after all four mercs were dealt to the first.
void TestJoiningPlayerTakesAShare()
{
	const TacticalEntityId m1{1, 1}, m2{2, 1}, m3{3, 1}, m4{4, 1}, m5{5, 1};
	const std::array<TacticalEntityId, 4> four{m1, m2, m3, m4};
	const std::array<PeerIdentity, 1> first{Peer(1)};
	const std::array<PeerIdentity, 2> both{Peer(1), Peer(2)};
	Deal deal{}; std::size_t count = 0;
	CoopActorOwnershipLedger ledger;
	CHECK(ledger.assign(first.data(), first.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m4) == Peer(1), "the first player is dealt all four");
	CHECK(ledger.assign(both.data(), both.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m1) == Peer(1) &&
		OwnerIn(deal, count, m2) == Peer(1) && OwnerIn(deal, count, m3) == Peer(2) &&
		OwnerIn(deal, count, m4) == Peer(2), "a joining player takes the last two");
	CHECK(ledger.joinShares() == 2 && *ledger.ownerOf(m3) == Peer(2), "and owns them for good");
	CHECK(ledger.assign(both.data(), both.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m2) == Peer(1) &&
		OwnerIn(deal, count, m3) == Peer(2) && ledger.joinShares() == 2, "a seated player takes nothing more");
	// Giving everything away sticks: the giver is seated, not joining.
	CHECK(ledger.handOver(m3, Peer(2), Peer(1)) && ledger.handOver(m4, Peer(2), Peer(1)), "player 2 gives both away");
	CHECK(ledger.assign(both.data(), both.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m3) == Peer(1) &&
		OwnerIn(deal, count, m4) == Peer(1) && ledger.joinShares() == 2, "a hand-over of every merc is not undone");
	CoopActorOwnershipLedger late;
	(void)late.assign(first.data(), first.size(), four.data(), four.size(), deal, count);
	const std::array<TacticalEntityId, 1> one{m1};
	CoopActorOwnershipLedger single;
	(void)single.assign(first.data(), first.size(), one.data(), one.size(), deal, count);
	CHECK(single.assign(both.data(), both.size(), one.data(), one.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m1) == Peer(1) && single.joinShares() == 0,
		"one merc is not split: the joiner is seated with none");
	const std::array<TacticalEntityId, 2> hired{m1, m5};
	CHECK(single.assign(both.data(), both.size(), hired.data(), hired.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m5) == Peer(2), "the next hire goes to them");
	// A world with no mercs present seats nobody.
	CHECK(late.assign(both.data(), both.size(), nullptr, 0, deal, count) ==
		CoopActorAssignmentPolicyResult::Success && late.joinShares() == 0, "an empty world seats nobody");
	CHECK(late.assign(both.data(), both.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m4) == Peer(2) && late.joinShares() == 2,
		"so the joiner still takes a share in the next");
	// A lent merc moves its lend; its absent owner keeps it.
	CoopActorOwnershipLedger lent;
	const std::array<PeerIdentity, 2> owners{Peer(1), Peer(3)};
	(void)lent.assign(owners.data(), owners.size(), four.data(), four.size(), deal, count);
	lent.beginWorld();
	(void)lent.assign(first.data(), first.size(), four.data(), four.size(), deal, count);
	CHECK(lent.assign(both.data(), both.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m4) == Peer(2) &&
		*lent.ownerOf(m4) == Peer(3), "a joiner takes a lend, not the absent owner's merc");
	// Two joiners at once share with the first player.
	CoopActorOwnershipLedger three;
	const std::array<PeerIdentity, 3> all{Peer(1), Peer(2), Peer(3)};
	(void)three.assign(first.data(), first.size(), four.data(), four.size(), deal, count);
	CHECK(three.assign(all.data(), all.size(), four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m1) == Peer(1) &&
		OwnerIn(deal, count, m2) == Peer(3) && OwnerIn(deal, count, m3) == Peer(2) &&
		OwnerIn(deal, count, m4) == Peer(2), "two joiners: nobody holds two more than anyone");
}

// A player who stays offline past the grace is left out of the deal, so the
// ledger lends their mercs to whoever is here; they get them back on return.
void TestOfflinePlayerIsLeftOutAfterGrace()
{
	using Clock = CoopOfflineParticipantFilter::Clock;
	const auto t0 = Clock::time_point{} + std::chrono::hours(1);
	const auto grace = std::chrono::seconds(45);
	const std::array<PeerIdentity, 2> both{Peer(1), Peer(2)};
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> dealt{};
	CoopOfflineParticipantFilter filter;
	const bool bothOnline[2] = {true, true};
	const bool secondOffline[2] = {true, false};
	const bool nobody[2] = {false, false};
	CHECK(filter.select(both.data(), 2, bothOnline, t0, grace, dealt) == 2, "everybody online is dealt");
	CHECK(filter.select(both.data(), 2, secondOffline, t0, grace, dealt) == 2 &&
		filter.select(both.data(), 2, secondOffline, t0 + std::chrono::seconds(44), grace, dealt) == 2,
		"a drop shorter than the grace moves nothing");
	CHECK(filter.select(both.data(), 2, secondOffline, t0 + grace, grace, dealt) == 1 && dealt[0] == Peer(1),
		"past the grace only the player who is here is dealt");
	CHECK(filter.select(both.data(), 2, bothOnline, t0 + std::chrono::seconds(50), grace, dealt) == 2,
		"back online, the player is dealt again");
	CHECK(filter.select(both.data(), 2, secondOffline, t0 + std::chrono::seconds(51), grace, dealt) == 2,
		"a new drop starts a new grace");
	CHECK(filter.select(both.data(), 2, nobody, t0 + std::chrono::minutes(10), grace, dealt) == 2,
		"with nobody online everybody stays dealt");
	// Through the ledger: the absent owner's mercs are lent, then returned.
	const TacticalEntityId m1{1, 1}, m2{2, 1}, m3{3, 1}, m4{4, 1};
	const std::array<TacticalEntityId, 4> four{m1, m2, m3, m4};
	CoopActorOwnershipLedger ledger;
	Deal deal{}; std::size_t count = 0;
	(void)ledger.assign(both.data(), 2, four.data(), four.size(), deal, count);
	const std::array<PeerIdentity, 1> alone{Peer(1)};
	CHECK(ledger.assign(alone.data(), 1, four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m2) == Peer(1) &&
		OwnerIn(deal, count, m4) == Peer(1) && *ledger.ownerOf(m2) == Peer(2),
		"the offline player's mercs are lent to the one who is here, ownership kept");
	CHECK(ledger.assign(both.data(), 2, four.data(), four.size(), deal, count) ==
		CoopActorAssignmentPolicyResult::Success && OwnerIn(deal, count, m2) == Peer(2) &&
		OwnerIn(deal, count, m4) == Peer(2) && OwnerIn(deal, count, m1) == Peer(1),
		"the returning player gets exactly their own mercs back");
}

int main()
{
	TestStableOwnershipLedger();
	TestOfflinePlayerIsLeftOutAfterGrace();
	TestJoiningPlayerTakesAShare();
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
