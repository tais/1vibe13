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
	endSession();
	if (!configuration.admission.enabled)
		return FullEngineCoopStartResult::AuthorityDisabled;
	if (!configuration.admission.complete())
		return FullEngineCoopStartResult::ConfigurationIncomplete;
	if (!ValidContext(configuration.tactical))
		return FullEngineCoopStartResult::InvalidTacticalContext;
	if (configuration.admission.sessionEpoch !=
		configuration.tactical.sessionEpoch)
		return FullEngineCoopStartResult::SessionEpochMismatch;

	admission_.beginSession(configuration.admission);
	if (authority_.beginSession(configuration.tactical) !=
		TacticalAuthorityConfigurationResult::Success)
	{
		endSession();
		return FullEngineCoopStartResult::ConfigurationIncomplete;
	}
	active_ = true;
	return FullEngineCoopStartResult::Success;
}

void FullEngineCoopIngress::endSession() noexcept
{
	active_ = false;
	admission_.beginSession(AuthorityConfiguration{});
	(void)authority_.beginSession(TacticalAuthorityContext{});
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
		result.response = admission_.admit(sender, request);
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

TacticalIntentIngressResult FullEngineCoopIngress::handleTacticalIntent(
	const TransportPeer& sender,
	const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	TacticalIntentIngressResult result;
	if (!active_) return result;

	TacticalIntent intent;
	result.decodeResult = DecodeTacticalIntent(bytes, size, intent);
	if (result.decodeResult != TacticalIntentCodecResult::Success)
		return result;

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

void FullEngineCoopIngress::disconnect(const TransportPeer& sender) noexcept
{
	admission_.disconnect(sender);
}

TacticalActorBindingResult FullEngineCoopIngress::bindActorForTransport(
	const TransportPeer& sender,
	TacticalEntityId actor) noexcept
{
	if (!active_) return TacticalActorBindingResult::NotConfigured;
	PeerIdentity peer{};
	if (!admission_.resolvePeerForIntent(
		sender, authority_.context().sessionEpoch, peer))
		return TacticalActorBindingResult::InvalidPeer;
	return authority_.bindActor(peer, actor);
}

bool FullEngineCoopIngress::unbindActor(TacticalEntityId actor) noexcept
{
	return active_ && authority_.unbindActor(actor);
}

void FullEngineCoopIngress::clearActorBindings() noexcept
{
	if (active_) authority_.clearActorBindings();
}

TacticalAuthorityConfigurationResult FullEngineCoopIngress::beginGeneration(
	std::uint64_t generation,
	std::uint64_t revision,
	std::uint64_t turnSerial) noexcept
{
	if (!active_)
		return TacticalAuthorityConfigurationResult::AdmissionUnavailable;
	return authority_.beginGeneration(generation, revision, turnSerial);
}

TacticalAuthorityConfigurationResult FullEngineCoopIngress::advanceContext(
	std::uint64_t revision,
	std::uint64_t turnSerial) noexcept
{
	if (!active_)
		return TacticalAuthorityConfigurationResult::AdmissionUnavailable;
	return authority_.advanceContext(revision, turnSerial);
}
}
