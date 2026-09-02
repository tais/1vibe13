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
}

#endif
