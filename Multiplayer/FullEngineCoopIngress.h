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
	SessionEpochMismatch
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
	virtual TacticalIntentExecutionDisposition execute(
		const AuthorizedTacticalIntent& intent) noexcept = 0;
};

struct AdmissionIngressResult
{
	DecodeResult decodeResult = DecodeResult::WrongSize;
	AdmissionResponse response;
	AdmissionResponseBytes responseBytes{};
	bool responseReady = false;
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

	// Every call first closes the previous session. Failure therefore leaves the
	// ingress inactive and empty; success starts a new epoch with no prior
	// credentials, transport bindings, actor bindings, or command sequences.
	FullEngineCoopStartResult beginSession(
		const FullEngineCoopSessionConfiguration& configuration) noexcept;
	void endSession() noexcept;
	bool active() const noexcept { return active_; }

	AdmissionIngressResult handleAdmission(
		const TransportPeer& sender,
		const std::uint8_t* bytes,
		std::size_t size) noexcept;
	TacticalIntentIngressResult handleTacticalIntent(
		const TransportPeer& sender,
		const std::uint8_t* bytes,
		std::size_t size) noexcept;

	void disconnect(const TransportPeer& sender) noexcept;
	TacticalActorBindingResult bindActorForTransport(
		const TransportPeer& sender,
		TacticalEntityId actor) noexcept;
	bool unbindActor(TacticalEntityId actor) noexcept;
	void clearActorBindings() noexcept;

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
	std::size_t actorBindingCount() const noexcept
	{
		return authority_.actorBindingCount();
	}

private:
	AdmissionRegistry admission_;
	TacticalIntentAuthority authority_;
	TacticalIntentExecutionSink& executionSink_;
	bool active_ = false;
};
}

#endif
