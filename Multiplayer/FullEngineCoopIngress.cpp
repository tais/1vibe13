#include "FullEngineCoopIngress.h"

#include <type_traits>

namespace CoopSession
{
static_assert(std::is_nothrow_copy_assignable<TacticalIntentPayload>::value,
	"authorized tactical payload handoff must remain noexcept");

namespace
{
bool ValidContext(const TacticalAuthorityContext& context) noexcept
{
	return context.sessionEpoch != 0 && context.worldGeneration != 0 &&
		context.revision != 0 && context.turnSerial != 0;
}

AdmissionRejectReason RejectReasonForDecode(DecodeResult result) noexcept
{
	return result == DecodeResult::UnsupportedProtocol
		? AdmissionRejectReason::UnsupportedProtocol
		: AdmissionRejectReason::MalformedRequest;
}
}

FullEngineCoopIngress::FullEngineCoopIngress(
	AdmissionTokenSource& tokenSource,
	TacticalIntentExecutionSink& executionSink) noexcept
	: admission_(&tokenSource),
	  authority_(admission_),
	  executionSink_(executionSink)
{
}

FullEngineCoopStartResult FullEngineCoopIngress::beginSession(
	const FullEngineCoopSessionConfiguration& configuration) noexcept
{
	const FullEngineCoopStartResult admissionResult =
		beginAdmissionSession(configuration.admission);
	if (admissionResult != FullEngineCoopStartResult::Success)
		return admissionResult;
	const FullEngineCoopStartResult tacticalResult =
		beginTacticalSession(configuration.tactical);
	if (tacticalResult != FullEngineCoopStartResult::Success)
	{
		endSession();
		return tacticalResult;
	}
	return FullEngineCoopStartResult::Success;
}

FullEngineCoopStartResult FullEngineCoopIngress::beginAdmissionSession(
	const AuthorityConfiguration& configuration) noexcept
{
	endSession();
	if (!configuration.enabled)
		return FullEngineCoopStartResult::AuthorityDisabled;
	if (!configuration.complete())
		return FullEngineCoopStartResult::ConfigurationIncomplete;
	admission_.beginSession(configuration);
	authority_.resetAdmissionEpoch(configuration.sessionEpoch);
	admissionActive_ = true;
	return FullEngineCoopStartResult::Success;
}

FullEngineCoopStartResult FullEngineCoopIngress::beginTacticalSession(
	const TacticalAuthorityContext& context) noexcept
{
	endTacticalSession();
	if (!admissionActive_)
		return FullEngineCoopStartResult::AdmissionSessionInactive;
	if (!ValidContext(context))
		return FullEngineCoopStartResult::InvalidTacticalContext;
	if (admission_.configuration().sessionEpoch != context.sessionEpoch)
		return FullEngineCoopStartResult::SessionEpochMismatch;
	if (authority_.beginSession(context) !=
		TacticalAuthorityConfigurationResult::Success)
		return FullEngineCoopStartResult::ConfigurationIncomplete;
	tacticalActive_ = true;
	return FullEngineCoopStartResult::Success;
}

void FullEngineCoopIngress::endTacticalSession() noexcept
{
	tacticalActive_ = false;
	(void)authority_.beginSession(TacticalAuthorityContext{});
}

void FullEngineCoopIngress::endSession() noexcept
{
	endTacticalSession();
	admissionActive_ = false;
	admission_.beginSession(AuthorityConfiguration{});
	authority_.resetAdmissionEpoch(0);
}

AdmissionIngressResult FullEngineCoopIngress::handleAdmission(
	const TransportPeer& sender,
	const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	AdmissionIngressResult result;
	AdmissionRequest request;
	result.decodeResult = DecodeAdmissionRequest(bytes, size, request);
	if (result.decodeResult == DecodeResult::Ok)
	{
		result.request = request;
		const AdmissionRegistryResult admission =
			admission_.admitWithEffects(sender, request);
		result.response = admission.response;
		result.displacedTransport = admission.displacedTransport;
	}
	else
	{
		result.response.sessionEpoch = admission_.configuration().sessionEpoch;
		result.response.rejectReason = RejectReasonForDecode(result.decodeResult);
	}
	result.responseReady =
		EncodeAdmissionResponse(result.response, result.responseBytes);
	return result;
}

AdmissionCredentialAbandonIngressResult
FullEngineCoopIngress::handleCredentialAbandon(
	const TransportPeer& sender,
	const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	AdmissionCredentialAbandonIngressResult result;
	AdmissionCredentialAbandon abandonment;
	result.decodeResult = DecodeAdmissionCredentialAbandon(
		bytes, size, abandonment);
	if (result.decodeResult == DecodeResult::Ok)
	{
		result.abandonment = abandonment;
		result.response = admission_.abandonUnknownCredential(
			sender, abandonment).response;
	}
	else
	{
		result.response.sessionEpoch =
			admission_.configuration().sessionEpoch;
		result.response.rejectReason =
			RejectReasonForDecode(result.decodeResult);
	}
	result.responseReady = EncodeAdmissionResponse(
		result.response, result.responseBytes);
	return result;
}

AdmissionAckIngressResult FullEngineCoopIngress::handleAdmissionAck(
	const TransportPeer& sender,
	const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	AdmissionAckIngressResult result;
	AdmissionAck acknowledgement;
	result.decodeResult = DecodeAdmissionAck(
		bytes, size, acknowledgement);
	if (result.decodeResult == DecodeResult::Ok)
		result.rejectReason = admission_.acknowledge(sender, acknowledgement);
	else
		result.rejectReason = RejectReasonForDecode(result.decodeResult);
	return result;
}

AdmissionSelfRetirementRegistryBegin
FullEngineCoopIngress::beginSelfRetirement(
	const TransportPeer& sender,
	std::uint64_t sessionEpoch,
	std::uint64_t requestId) noexcept
{
	if (!admissionActive_)
		return AdmissionSelfRetirementRegistryBegin{};
	return admission_.beginSelfRetirement(
		sender, sessionEpoch, requestId);
}

AdmissionSelfRetirementRegistryResult
FullEngineCoopIngress::completeSelfRetirement(
	const PeerIdentity& peer,
	std::uint64_t requestId) noexcept
{
	if (!admissionActive_)
		return AdmissionSelfRetirementRegistryResult::InvalidContext;
	return admission_.completeSelfRetirement(peer, requestId);
}

bool FullEngineCoopIngress::resolveAuthenticatedPeer(
	const TransportPeer& sender,
	std::uint64_t sessionEpoch,
	PeerIdentity& identity) const noexcept
{
	return admissionActive_ && admission_.resolveAuthenticatedPeer(
		sender, sessionEpoch, identity);
}

bool FullEngineCoopIngress::tacticalExecutionReady() const noexcept
{
	return tacticalActive_ && executionSink_.ready();
}

TacticalIntentIngressResult FullEngineCoopIngress::handleTacticalIntent(
	const TransportPeer& sender,
	const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	TacticalIntentIngressResult result;
	if (!tacticalActive_) return result;

	TacticalIntent intent;
	result.decodeResult = DecodeTacticalIntent(bytes, size, intent);
	if (result.decodeResult != TacticalIntentCodecResult::Success)
		return result;
	if (!executionSink_.ready()) return result;

	result.authorization = authority_.authorize(sender, intent);
	if (!result.authorization) return result;

	AuthorizedTacticalIntent authorized;
	authorized.peerIdentity = result.authorization.peerIdentity;
	authorized.commandId = result.authorization.commandId;
	authorized.context = authority_.context();
	authorized.actor = intent.actor;
	authorized.payload = intent.payload;
	result.execution = executionSink_.execute(authorized);
	result.executionAttempted = true;
	return result;
}

TacticalIntentIngressResult FullEngineCoopIngress::rejectTacticalIntent(
	const TransportPeer& sender,
	const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	TacticalIntentIngressResult result;
	if (!tacticalActive_) return result;
	TacticalIntent intent;
	result.decodeResult = DecodeTacticalIntent(bytes, size, intent);
	if (result.decodeResult != TacticalIntentCodecResult::Success)
		return result;
	result.authorization = authority_.authorize(sender, intent);
	return result;
}

void FullEngineCoopIngress::disconnect(const TransportPeer& sender) noexcept
{
	admission_.disconnect(sender);
}

void FullEngineCoopIngress::clearTransportBindings() noexcept
{
	admission_.clearTransportBindings();
}

TacticalActorBindingResult FullEngineCoopIngress::bindActorForTransport(
	const TransportPeer& sender,
	TacticalEntityId actor) noexcept
{
	if (!tacticalActive_) return TacticalActorBindingResult::NotConfigured;
	PeerIdentity peer{};
	if (!admission_.resolvePeerForIntent(
		sender, authority_.context().sessionEpoch, peer))
		return TacticalActorBindingResult::InvalidPeer;
	return authority_.bindActor(peer, actor);
}

bool FullEngineCoopIngress::unbindActor(TacticalEntityId actor) noexcept
{
	return tacticalActive_ && authority_.unbindActor(actor);
}

void FullEngineCoopIngress::clearActorBindings() noexcept
{
	if (tacticalActive_) authority_.clearActorBindings();
}

bool FullEngineCoopIngress::canRetireTacticalAuthorityPeer(
	const PeerIdentity& peer) const noexcept
{
	return admissionActive_ && authority_.canRetirePeerSequence(peer);
}

bool FullEngineCoopIngress::retireTacticalAuthorityPeer(
	const PeerIdentity& peer) noexcept
{
	return admissionActive_ && authority_.retirePeerSequence(peer);
}

TacticalAuthorityConfigurationResult FullEngineCoopIngress::beginGeneration(
	std::uint64_t generation,
	std::uint64_t revision,
	std::uint64_t turnSerial) noexcept
{
	if (!tacticalActive_)
		return TacticalAuthorityConfigurationResult::AdmissionUnavailable;
	return authority_.beginGeneration(generation, revision, turnSerial);
}

TacticalAuthorityConfigurationResult FullEngineCoopIngress::advanceContext(
	std::uint64_t revision,
	std::uint64_t turnSerial) noexcept
{
	if (!tacticalActive_)
		return TacticalAuthorityConfigurationResult::AdmissionUnavailable;
	return authority_.advanceContext(revision, turnSerial);
}
}
