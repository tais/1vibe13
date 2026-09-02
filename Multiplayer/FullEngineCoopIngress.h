#ifndef MULTIPLAYER_FULL_ENGINE_COOP_INGRESS_H
#define MULTIPLAYER_FULL_ENGINE_COOP_INGRESS_H

#include "CoopAdmission.h"
#include "CoopTacticalAuthority.h"

#include <cstddef>
#include <cstdint>

namespace CoopSession
{
enum class FullEngineCoopStartResult
{
	Success,
	AuthorityDisabled,
	ConfigurationIncomplete,
	InvalidTacticalContext,
	SessionEpochMismatch,
	AdmissionSessionInactive
};

struct FullEngineCoopSessionConfiguration
{
	AuthorityConfiguration admission;
	TacticalAuthorityContext tactical;
};

// The ingress passes only server-resolved identity and already-checked context
// across the execution boundary. The peer-claimed identity is deliberately not
// present in this value.
struct AuthorizedTacticalIntent
{
	PeerIdentity peerIdentity{};
	std::uint64_t commandId = 0;
	TacticalAuthorityContext context;
	TacticalEntityId actor;
	TacticalIntentPayload payload = StopTacticalIntent{};
};

// Every disposition is terminal for the authorized command identifier. A
// retained command has been accepted by a bounded live queue; rejected means
// the live gameplay adapter declined it and must eventually publish a receipt.
enum class TacticalIntentExecutionDisposition
{
	Applied,
	Retained,
	Rejected
};

class TacticalIntentExecutionSink
{
public:
	virtual ~TacticalIntentExecutionSink() = default;
	// Called before the authority consumes an at-most-once command identifier.
	// False applies bounded backpressure and guarantees execute() is not called.
	virtual bool ready() const noexcept { return true; }
	virtual TacticalIntentExecutionDisposition execute(
		const AuthorizedTacticalIntent& intent) noexcept = 0;
};

struct AdmissionIngressResult
{
	DecodeResult decodeResult = DecodeResult::WrongSize;
	AdmissionRequest request;
	AdmissionResponse response;
	AdmissionResponseBytes responseBytes{};
	// Process-local transport effect; never encoded in responseBytes.
	TransportPeer displacedTransport;
	bool responseReady = false;
};

struct AdmissionCredentialAbandonIngressResult
{
	DecodeResult decodeResult = DecodeResult::WrongSize;
	AdmissionCredentialAbandon abandonment;
	AdmissionResponse response;
	AdmissionResponseBytes responseBytes{};
	bool responseReady = false;
};

struct AdmissionAckIngressResult
{
	DecodeResult decodeResult = DecodeResult::WrongSize;
	AdmissionRejectReason rejectReason = AdmissionRejectReason::MalformedRequest;

	bool acknowledged() const noexcept
	{
		return decodeResult == DecodeResult::Ok &&
			rejectReason == AdmissionRejectReason::None;
	}
};

struct TacticalIntentIngressResult
{
	TacticalIntentCodecResult decodeResult = TacticalIntentCodecResult::Invalid;
	TacticalIntentAuthorizationResult authorization;
	TacticalIntentExecutionDisposition execution =
		TacticalIntentExecutionDisposition::Rejected;
	bool executionAttempted = false;
};

// Composition seam for the full-engine dedicated process. It owns no transport
// and performs no JA2 mutation itself: the caller derives TransportPeer from
// the received connection, sends admission responseBytes only to that sender,
// and supplies a live execution sink. No legacy MP v3.2 message is reused.
class FullEngineCoopIngress
{
public:
	FullEngineCoopIngress(
		AdmissionTokenSource& tokenSource,
		TacticalIntentExecutionSink& executionSink) noexcept;

	// Admission starts before any campaign world is exposed. Every call closes
	// the previous admission and tactical sessions, so credentials and bindings
	// can never cross an epoch boundary.
	FullEngineCoopStartResult beginAdmissionSession(
		const AuthorityConfiguration& configuration) noexcept;
	// Tactical authority may be enabled only after admission is live for the same
	// epoch. A failed tactical start leaves admission available for a corrected
	// retry and clears actor bindings, while admission-epoch command ordering is
	// preserved across tactical world/generation transitions.
	FullEngineCoopStartResult beginTacticalSession(
		const TacticalAuthorityContext& context) noexcept;
	void endTacticalSession() noexcept;

	// Compatibility wrapper for callers that already have both configurations.
	// Its historical atomic semantics remain: any failure leaves all ingress
	// inactive and empty.
	FullEngineCoopStartResult beginSession(
		const FullEngineCoopSessionConfiguration& configuration) noexcept;
	void endSession() noexcept;
	bool admissionActive() const noexcept { return admissionActive_; }
	bool tacticalActive() const noexcept { return tacticalActive_; }
	bool active() const noexcept { return tacticalActive(); }

	AdmissionIngressResult handleAdmission(
		const TransportPeer& sender,
		const std::uint8_t* bytes,
		std::size_t size) noexcept;
	AdmissionAckIngressResult handleAdmissionAck(
		const TransportPeer& sender,
		const std::uint8_t* bytes,
		std::size_t size) noexcept;
	AdmissionCredentialAbandonIngressResult handleCredentialAbandon(
		const TransportPeer& sender,
		const std::uint8_t* bytes,
		std::size_t size) noexcept;
	AdmissionSelfRetirementRegistryBegin beginSelfRetirement(
		const TransportPeer& sender,
		std::uint64_t sessionEpoch,
		std::uint64_t requestId) noexcept;
	AdmissionSelfRetirementRegistryResult completeSelfRetirement(
		const PeerIdentity& peer,
		std::uint64_t requestId) noexcept;
	// Resolves only a live, ACK-confirmed transport binding for the supplied
	// admission epoch. The output is left unchanged on failure.
	bool resolveAuthenticatedPeer(
		const TransportPeer& sender,
		std::uint64_t sessionEpoch,
		PeerIdentity& identity) const noexcept;
	bool tacticalExecutionReady() const noexcept;
	TacticalIntentIngressResult handleTacticalIntent(
		const TransportPeer& sender,
		const std::uint8_t* bytes,
		std::size_t size) noexcept;
	// Decodes and applies the same admission/at-most-once authority decision but
	// never calls the execution sink and intentionally bypasses sink readiness.
	// The caller must retain a terminal receipt for every consumed command.
	TacticalIntentIngressResult rejectTacticalIntent(
		const TransportPeer& sender,
		const std::uint8_t* bytes,
		std::size_t size) noexcept;

	void disconnect(const TransportPeer& sender) noexcept;
	void clearTransportBindings() noexcept;
	TacticalActorBindingResult bindActorForTransport(
		const TransportPeer& sender,
		TacticalEntityId actor) noexcept;
	bool unbindActor(TacticalEntityId actor) noexcept;
	void clearActorBindings() noexcept;
	bool canRetireTacticalAuthorityPeer(
		const PeerIdentity& peer) const noexcept;
	bool retireTacticalAuthorityPeer(
		const PeerIdentity& peer) noexcept;

	TacticalAuthorityConfigurationResult beginGeneration(
		std::uint64_t generation,
		std::uint64_t revision,
		std::uint64_t turnSerial) noexcept;
	TacticalAuthorityConfigurationResult advanceContext(
		std::uint64_t revision,
		std::uint64_t turnSerial) noexcept;

	const AuthorityConfiguration& admissionConfiguration() const noexcept
	{
		return admission_.configuration();
	}
	const TacticalAuthorityContext& tacticalContext() const noexcept
	{
		return authority_.context();
	}
	std::size_t admittedPeerCount() const noexcept
	{
		return admission_.peerCount();
	}
	std::size_t boundPeerCount() const noexcept
	{
		return admission_.boundPeerCount();
	}
	std::size_t retiredCredentialCount() const noexcept
	{
		return admission_.retiredCredentialCount();
	}
	bool credentialRetired(const PeerIdentity& peer) const noexcept
	{
		return admission_.credentialRetired(peer);
	}
	std::size_t pendingSelfRetirementCount() const noexcept
	{
		return admission_.pendingSelfRetirementCount();
	}
	std::size_t actorBindingCount() const noexcept
	{
		return authority_.actorBindingCount();
	}

private:
	AdmissionRegistry admission_;
	TacticalIntentAuthority authority_;
	TacticalIntentExecutionSink& executionSink_;
	bool admissionActive_ = false;
	bool tacticalActive_ = false;
};
}

#endif
