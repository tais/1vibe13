#ifndef MULTIPLAYER_COOP_ACTOR_ASSIGNMENT_POLICY_H
#define MULTIPLAYER_COOP_ACTOR_ASSIGNMENT_POLICY_H

#include "FullEngineCoopServerSession.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace CoopSession
{
enum class CoopActorAssignmentPolicyResult : std::uint8_t
{
	Success,
	InvalidPeerSet,
	InvalidActorSet,
	CapacityReached
};

enum class CoopWorldParticipantPolicyResult : std::uint8_t
{
	Unchanged,
	Published,
	DeferredUntilFreshBaseline,
	InvalidCurrentSet,
	InvalidReadySet,
	CapacityReached
};

// Grows the retained participant roster from the currently authenticated,
// campaign-ready identities without ever removing an established identity.
// That monotonic rule preserves reconnect ownership and prevents a transient
// disconnect from transferring actors. Once a world has participants, a new
// identity is published only at a caller-proven fresh-baseline boundary.
// Inputs and output are strict identity order; failure or deferral preserves
// both output arguments.
CoopWorldParticipantPolicyResult GrowCoopWorldParticipants(
	const PeerIdentity* current,
	std::size_t currentCount,
	const PeerIdentity* ready,
	std::size_t readyCount,
	bool freshBaselineBoundary,
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers>& output,
	std::size_t& outputCount) noexcept;

// Builds the stable, actor-sorted ACL used for one tactical world. Peers and
// actors must already be in canonical strict order. The caller retains peer
// identities across transient disconnects and changes the roster only behind
// a fresh-baseline gate. Actors are distributed round-robin so every selected
// peer receives one before any peer receives a second. Failure preserves both
// output arguments.
CoopActorAssignmentPolicyResult BuildCoopActorAssignments(
	const PeerIdentity* peers,
	std::size_t peerCount,
	const TacticalEntityId* actors,
	std::size_t actorCount,
	std::array<CoopTacticalActorAssignment,
		MaximumCoopTacticalAssignments>& output,
	std::size_t& outputCount) noexcept;

inline constexpr std::size_t MaximumCoopActorOwnershipEntries = 256;
// Stable merc ownership for one server run. Dealing round-robin over whoever
// is present re-dealt every merc whenever the present set changed (a merc
// falling unconscious, getting up, arriving or leaving), so ownership flipped
// mid-battle. The ledger records each merc's owner the first time it is dealt
// and keeps it, whether or not the merc is present or the owner connected.
// Per world: a merc whose owner is not a participant of this world is lent
// to one who is, and that lend holds until beginWorld(); a merc with no owner
// yet goes to the participant with the fewest present mercs (ties: identity
// order), which on a first deal is exactly the round-robin. A player's first
// deal with mercs present seats them: a player who joined after the others
// were dealt (the battle starts when the first player is ready) takes a fair
// share then, the last mercs in actor order of whoever holds the most, until
// no one holds more than one merc above them. A seated player is never
// re-dealt, so giving every merc away by hand-over sticks. assign() is
// transactional: failure leaves the ledger and both outputs untouched.
class CoopActorOwnershipLedger
{
public:
	void beginWorld() noexcept { lendCount_ = 0; }
	void clear() noexcept { ownerCount_ = 0; lendCount_ = 0; seatedCount_ = 0; joinShares_ = 0; }
	CoopActorAssignmentPolicyResult assign(
		const PeerIdentity* peers,
		std::size_t peerCount,
		const TacticalEntityId* actors,
		std::size_t actorCount,
		std::array<CoopTacticalActorAssignment,
			MaximumCoopTacticalAssignments>& output,
		std::size_t& outputCount) noexcept;
	// The player now controlling `actor` (its owner, or the holder of this
	// world's lend) gives it to `to`: an owner's merc changes owner for good, a
	// lent merc moves its lend. False (nothing changed) when `from` does not
	// control it this way or `to` is `from`.
	bool handOver(const TacticalEntityId& actor, const PeerIdentity& from, const PeerIdentity& to) noexcept;
	// The recorded owner (not a per-world lend), or nullptr.
	const PeerIdentity* ownerOf(const TacticalEntityId& actor) const noexcept;
	std::size_t ownerCount() const noexcept { return ownerCount_; }
	// Mercs moved to joining players so far this run (a log counter).
	std::size_t joinShares() const noexcept { return joinShares_; }
	static constexpr std::size_t MaximumSeatedPlayers = 32;
private:
	std::array<CoopTacticalActorAssignment, MaximumCoopActorOwnershipEntries> owners_{};
	std::size_t ownerCount_ = 0;
	std::array<CoopTacticalActorAssignment, MaximumCoopTacticalAssignments> lends_{};
	std::size_t lendCount_ = 0;
	std::array<PeerIdentity, MaximumSeatedPlayers> seated_{};
	std::size_t seatedCount_ = 0;
	std::size_t joinShares_ = 0;
};
}

#endif
