#ifndef MULTIPLAYER_COOP_TACTICAL_AUTHORITY_H
#define MULTIPLAYER_COOP_TACTICAL_AUTHORITY_H

#include "CoopAdmission.h"
#include "CoopTacticalIntent.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace CoopSession
{
inline constexpr std::size_t MaximumAuthoritativeActors = 256;

enum class TacticalAuthorityConfigurationResult
{
	Success,
	AdmissionUnavailable,
	InvalidContext,
	StaleContext
};

enum class TacticalActorBindingResult
{
	Success,
	NotConfigured,
	InvalidPeer,
	InvalidActor,
	ActorAlreadyOwned,
	CapacityReached
};

enum class TacticalIntentAuthorizationReason
{
	None,
	NotConfigured,
	NotAdmitted,
	WrongSessionEpoch,
	ClaimedIdentityMismatch,
	InvalidIntent,
	WrongGeneration,
	StaleRevision,
	FutureRevision,
	StaleTurn,
	FutureTurn,
	InvalidCommandId,
	DuplicateCommand,
	OutOfOrderCommand,
	SequenceExhausted,
	ActorNotOwned,
	PeerCapacityReached
};

struct TacticalIntentAuthorizationResult
{
	TacticalIntentAuthorizationReason reason =
		TacticalIntentAuthorizationReason::NotConfigured;
	PeerIdentity peerIdentity{};
	std::uint64_t commandId = 0;
	// The admission-epoch-wide cursor after this decision. Zero is exhausted.
	std::uint64_t nextExpectedCommandId = 0;
	// True exactly once for a structurally valid, current-admission-epoch ID
	// equal to the prior cursor, including later context/ACL rejections.
	bool commandConsumed = false;

	explicit operator bool() const noexcept
	{
		return reason == TacticalIntentAuthorizationReason::None;
	}
};

struct TacticalAuthorityContext
{
	std::uint64_t sessionEpoch = 0;
	std::uint64_t worldGeneration = 0;
	std::uint64_t revision = 0;
	std::uint64_t turnSerial = 0;
};

// Bounded admission/order/ownership gate. It owns no game state and executes
// no command. A full-engine dedicated adapter may submit a successful intent
// to the existing tactical command queue after applying live gameplay rules.
class TacticalIntentAuthority
{
public:
	explicit TacticalIntentAuthority(AdmissionRegistry& admission) noexcept;
	// This is the only sequence reset. Tactical world/generation transitions and
	// transport reconnects deliberately preserve admission-epoch ordering.
	void resetAdmissionEpoch(std::uint64_t sessionEpoch) noexcept;

	TacticalAuthorityConfigurationResult beginSession(
		TacticalAuthorityContext context) noexcept;
	TacticalAuthorityConfigurationResult beginGeneration(
		std::uint64_t generation,
		std::uint64_t revision,
		std::uint64_t turnSerial) noexcept;
	TacticalAuthorityConfigurationResult advanceContext(
		std::uint64_t revision,
		std::uint64_t turnSerial) noexcept;

	const TacticalAuthorityContext& context() const noexcept { return context_; }
	bool configured() const noexcept { return configured_; }

	TacticalActorBindingResult bindActor(
		const PeerIdentity& peer,
		TacticalEntityId actor) noexcept;
	bool unbindActor(TacticalEntityId actor) noexcept;
	void clearActorBindings() noexcept;
	std::size_t actorBindingCount() const noexcept { return actorBindingCount_; }

	TacticalIntentAuthorizationResult authorize(
		const TransportPeer& sender,
		const TacticalIntent& intent) noexcept;
	// A completed authenticated retirement may free the fixed admission-epoch
	// sequence slot and that peer's actor ACLs atomically. The preflight/mutation
	// split lets the coordinator prove this cannot fail after its replication
	// compaction has committed; all survivor bindings/cursors remain unchanged.
	bool canRetirePeerSequence(const PeerIdentity& peer) const noexcept;
	bool retirePeerSequence(const PeerIdentity& peer) noexcept;
	std::size_t peerSequenceCount() const noexcept
	{
		return peerSequenceCount_;
	}

private:
	struct ActorBinding
	{
		PeerIdentity peer{};
		TacticalEntityId actor;
	};

	struct PeerSequence
	{
		PeerIdentity peer{};
		std::uint64_t nextCommandId = 1;
		bool exhausted = false;
	};

	const ActorBinding* findActor(TacticalEntityId actor) const noexcept;
	const PeerSequence* findPeerSequence(
		const PeerIdentity& peer) const noexcept;
	PeerSequence* findPeerSequence(const PeerIdentity& peer) noexcept;
	PeerSequence* findOrCreatePeerSequence(const PeerIdentity& peer) noexcept;
	bool peerOwnsActor(
		const PeerIdentity& peer,
		TacticalEntityId actor) const noexcept;
	void clearSequences() noexcept;

	AdmissionRegistry& admission_;
	std::uint64_t sequenceEpoch_ = 0;
	TacticalAuthorityContext context_;
	bool configured_ = false;
	std::array<ActorBinding, MaximumAuthoritativeActors> actorBindings_{};
	std::size_t actorBindingCount_ = 0;
	std::array<PeerSequence, MaximumAuthorityPeers> peerSequences_{};
	std::size_t peerSequenceCount_ = 0;
};
}

#endif
