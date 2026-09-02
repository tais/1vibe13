#include "FullEngineCoopClient.h"

#include "CoopHandshakeProtocol.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace CoopSession
{
namespace
{
bool ZeroContentManifest(const ContentManifestSha256& digest) noexcept
{
	return std::all_of(digest.begin(), digest.end(),
		[](std::uint8_t value) noexcept { return value == 0; });
}

bool ValidConfiguration(
	const FullEngineCoopClientConfiguration& configuration) noexcept
{
	return configuration.protocolVersion == CurrentProtocolVersion &&
		configuration.runtimeFingerprint.schema != 0 &&
		!ZeroContentManifest(configuration.contentManifestSha256) &&
		configuration.maximumInboundWireBytes >=
			CoopTacticalIntentReceiptWireSize &&
		configuration.maximumInboundWireBytes <=
			MaximumCoopTacticalWireSize &&
		configuration.maximumAssignedActors != 0 &&
		configuration.maximumAssignedActors <=
			MaximumCoopTacticalAssignedActors;
}

std::uint64_t CommandAfter(std::uint64_t commandId) noexcept
{
	return commandId == (std::numeric_limits<std::uint64_t>::max)()
		? 0 : commandId + 1;
}

bool CursorAtOrAfter(std::uint64_t candidate,
	std::uint64_t current) noexcept
{
	if (current == 0) return candidate == 0;
	return candidate == 0 || candidate >= current;
}

bool ReceiptSequenceMatches(std::uint64_t commandId,
	std::uint64_t nextExpectedCommandId) noexcept
{
	return commandId != 0 &&
		nextExpectedCommandId == CommandAfter(commandId);
}

bool ReportsInvalidCommandSequence(
	const CoopTacticalIntentReceipt& receipt) noexcept
{
	return receipt.status == CoopTacticalIntentReceiptStatus::Rejected &&
		receipt.reason ==
			CoopTacticalIntentReceiptReason::InvalidCommandSequence;
}

bool ReportsInboxSequenceExhausted(
	const CoopTacticalIntentReceipt& receipt) noexcept
{
	return receipt.status == CoopTacticalIntentReceiptStatus::Rejected &&
		receipt.reason ==
			CoopTacticalIntentReceiptReason::InboxSequenceExhausted;
}

bool ReceiptCursorValid(const CoopTacticalIntentReceipt& receipt) noexcept
{
	if (ReportsInvalidCommandSequence(receipt))
		return receipt.nextExpectedCommandId != 0;
	if (ReportsInboxSequenceExhausted(receipt))
		return receipt.nextExpectedCommandId == 0;
	return ReceiptSequenceMatches(receipt.commandId,
		receipt.nextExpectedCommandId);
}

bool CursorAlreadyConsumed(std::uint64_t commandId,
	std::uint64_t nextExpectedCommandId,
	std::uint64_t current) noexcept
{
	if (!ReceiptSequenceMatches(commandId, nextExpectedCommandId))
		return false;
	if (current == 0) return true;
	return commandId < current && nextExpectedCommandId <= current;
}

bool ReceiptStateNotFuture(const CoopTacticalStateIdentity& receipt,
	const CoopTacticalStateIdentity& accepted) noexcept
{
	if (receipt.sessionEpoch != accepted.sessionEpoch ||
		receipt.worldGeneration > accepted.worldGeneration)
		return false;
	if (receipt.worldGeneration < accepted.worldGeneration) return true;
	return receipt.revision <= accepted.revision &&
		receipt.turnSerial <= accepted.turnSerial &&
		(receipt.revision != accepted.revision ||
			receipt.turnSerial == accepted.turnSerial);
}

bool DeltaTurnEdgeValid(const TacticalWorldDelta& delta,
	std::uint64_t previousTurnSerial,
	std::uint64_t resultingTurnSerial) noexcept
{
	const TacticalTurnChangedEvent* turn = nullptr;
	for (const TacticalWorldEvent& event : delta.events)
	{
		if (!std::holds_alternative<TacticalTurnChangedEvent>(event))
			continue;
		if (turn != nullptr) return false;
		turn = &std::get<TacticalTurnChangedEvent>(event);
	}
	if (turn == nullptr)
		return previousTurnSerial == resultingTurnSerial;
	return turn->previous.serial == previousTurnSerial &&
		turn->current.serial == resultingTurnSerial;
}
}

FullEngineCoopClient::FullEngineCoopClient(
	FullEngineCoopClientWire& wire,
	FullEngineCoopPassiveReplicaSink& replica,
	FullEngineCoopReconnectCredentialStore* credentialStore) noexcept
	: wire_(wire), replica_(replica), credentialStore_(credentialStore)
{
}

FullEngineCoopClientResult FullEngineCoopClient::configure(
	const FullEngineCoopClientConfiguration& configuration) noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_)
		return FullEngineCoopClientResult::InvalidState;
	if (state_ != FullEngineCoopClientState::Disconnected)
		return FullEngineCoopClientResult::InvalidState;
	if (!ValidConfiguration(configuration) ||
		(configuration.durableReconnectCredentialRequired &&
			(configuration.expectedSessionEpoch == 0 ||
			 credentialStore_ == nullptr)))
	{
		configured_ = false;
		state_ = FullEngineCoopClientState::Failed;
		lastResult_ = FullEngineCoopClientResult::InvalidConfiguration;
		return lastResult_;
	}
	configuration_ = configuration;
	configured_ = true;
	lastResult_ = FullEngineCoopClientResult::Success;
	lastAdmissionRejectReason_ = AdmissionRejectReason::None;
	sessionEpoch_ = 0;
	clearCredentials();
	clearReplicaState();
	nextSelfRetirementRequestId_ = 1;
	selfRetirementRequestId_ = 0;
	selfRetirementAwaitingOutcome_ = false;
	selfRetirementResumeState_ = FullEngineCoopClientState::Disconnected;
	return lastResult_;
}

FullEngineCoopClientResult FullEngineCoopClient::restoreReconnectCredential(
	const AdmissionAck& credential) noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_ ||
		!configured_ || state_ != FullEngineCoopClientState::Disconnected ||
		hasReconnectCredential() || sessionEpoch_ != 0)
		return FullEngineCoopClientResult::InvalidState;
	if (credential.protocolVersion != configuration_.protocolVersion ||
		credential.sessionEpoch == 0 ||
		(configuration_.expectedSessionEpoch != 0 &&
			credential.sessionEpoch != configuration_.expectedSessionEpoch) ||
		IsZero(credential.peerIdentity) || IsZero(credential.reconnectToken))
		return FullEngineCoopClientResult::InvalidMessage;

	sessionEpoch_ = credential.sessionEpoch;
	peerIdentity_ = credential.peerIdentity;
	reconnectToken_ = credential.reconnectToken;
	lastResult_ = FullEngineCoopClientResult::Success;
	return lastResult_;
}

FullEngineCoopClientResult FullEngineCoopClient::beginConnection() noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_)
		return FullEngineCoopClientResult::InvalidState;
	if (!configured_)
		return FullEngineCoopClientResult::InvalidConfiguration;
	if (state_ != FullEngineCoopClientState::Disconnected)
		return FullEngineCoopClientResult::InvalidState;
	clearConnectionState();
	state_ = FullEngineCoopClientState::Connecting;
	lastResult_ = FullEngineCoopClientResult::Success;
	return lastResult_;
}

FullEngineCoopClientResult FullEngineCoopClient::transportConnected() noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_)
		return FullEngineCoopClientResult::InvalidState;
	if (state_ != FullEngineCoopClientState::Connecting)
		return FullEngineCoopClientResult::InvalidState;
	state_ = FullEngineCoopClientState::Hello;
	lastResult_ = FullEngineCoopClientResult::Success;
	return lastResult_;
}

void FullEngineCoopClient::transportDisconnected() noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_) return;
	if (state_ == FullEngineCoopClientState::Retired) return;
	clearConnectionState();
	state_ = FullEngineCoopClientState::Disconnected;
}

void FullEngineCoopClient::disconnect() noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_) return;
	closeWire();
	transportDisconnected();
}

FullEngineCoopClientResult FullEngineCoopClient::receiveServerHello(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_)
		return FullEngineCoopClientResult::InvalidState;
	if (state_ != FullEngineCoopClientState::Hello)
		return FullEngineCoopClientResult::InvalidState;
	if (bytes == nullptr || size > configuration_.maximumInboundWireBytes)
		return fail(FullEngineCoopClientResult::InvalidMessage);

	CoopServerHello hello;
	if (DecodeCoopServerHello(bytes, size, hello) !=
		CoopServerHelloDecodeResult::Success)
		return fail(FullEngineCoopClientResult::InvalidMessage);
	if (hello.protocolVersion != configuration_.protocolVersion ||
		hello.runtimeFingerprint != configuration_.runtimeFingerprint ||
		hello.contentManifestSha256 !=
			configuration_.contentManifestSha256 ||
		(configuration_.expectedSessionEpoch != 0 &&
			hello.sessionEpoch != configuration_.expectedSessionEpoch))
		return fail(FullEngineCoopClientResult::CompatibilityMismatch);

	clearReplicaState();
	if (sessionEpoch_ != hello.sessionEpoch)
	{
		clearCredentials();
		sessionEpoch_ = hello.sessionEpoch;
	}

	AdmissionRequest request;
	request.protocolVersion = configuration_.protocolVersion;
	request.sessionEpoch = sessionEpoch_;
	request.runtimeFingerprint = configuration_.runtimeFingerprint;
	request.contentManifestSha256 =
		configuration_.contentManifestSha256;
	request.peerIdentity = peerIdentity_;
	request.reconnectToken = reconnectToken_;
	AdmissionRequestBytes encoded{};
	if (!EncodeAdmissionRequest(request, encoded))
		return fail(FullEngineCoopClientResult::InvalidMessage);
	if (!sendFrame(CoopAdmissionRequestMessageName,
		encoded.data(), encoded.size()))
		return fail(FullEngineCoopClientResult::WireFailure);

	state_ = FullEngineCoopClientState::Admission;
	lastResult_ = FullEngineCoopClientResult::Success;
	return lastResult_;
}

FullEngineCoopClientResult FullEngineCoopClient::receiveAdmissionResponse(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_)
		return FullEngineCoopClientResult::InvalidState;
	if (state_ != FullEngineCoopClientState::Admission)
		return FullEngineCoopClientResult::InvalidState;
	if (bytes == nullptr || size > configuration_.maximumInboundWireBytes)
		return fail(FullEngineCoopClientResult::InvalidMessage);

	AdmissionResponse response;
	if (DecodeAdmissionResponse(bytes, size, response) != DecodeResult::Ok ||
		response.protocolVersion != configuration_.protocolVersion ||
		response.sessionEpoch != sessionEpoch_)
		return fail(FullEngineCoopClientResult::InvalidMessage);
	const bool reconnect = hasReconnectCredential() &&
		!credentialAbandonPending_;
	const bool retrySelfRetirement = reconnect &&
		selfRetirementAwaitingOutcome_ && selfRetirementRequestId_ != 0;
	if (!response.admitted())
	{
		lastAdmissionRejectReason_ = response.rejectReason;
		if (reconnect && response.peerIdentity == peerIdentity_ &&
			response.rejectReason ==
				AdmissionRejectReason::CredentialRetirementPending)
		{
			selfRetirementAwaitingOutcome_ = true;
			clearReplicaState();
			state_ = FullEngineCoopClientState::Disconnected;
			lastResult_ =
				FullEngineCoopClientResult::CredentialRetirementPending;
			closeWire();
			return lastResult_;
		}
		if (reconnect && response.peerIdentity == peerIdentity_ &&
			response.rejectReason ==
				AdmissionRejectReason::CredentialRetired)
			return retireCredential();
		if (reconnect &&
			response.rejectReason == AdmissionRejectReason::UnknownPeer &&
			response.peerIdentity == peerIdentity_)
		{
			AdmissionCredentialAbandon abandonment;
			abandonment.protocolVersion = configuration_.protocolVersion;
			abandonment.sessionEpoch = sessionEpoch_;
			abandonment.runtimeFingerprint =
				configuration_.runtimeFingerprint;
			abandonment.contentManifestSha256 =
				configuration_.contentManifestSha256;
			abandonment.peerIdentity = peerIdentity_;
			abandonment.reconnectToken = reconnectToken_;
			AdmissionCredentialAbandonBytes encoded{};
			if (!EncodeAdmissionCredentialAbandon(abandonment, encoded))
				return fail(FullEngineCoopClientResult::InvalidMessage);
			if (!sendFrame(CoopAdmissionCredentialAbandonMessageName,
				encoded.data(), encoded.size()))
				return fail(FullEngineCoopClientResult::WireFailure);
			credentialAbandonPending_ = true;
			lastResult_ = FullEngineCoopClientResult::Success;
			return lastResult_;
		}
		return fail(FullEngineCoopClientResult::AdmissionRejected);
	}

	if (credentialAbandonPending_)
	{
		if (!hasReconnectCredential() ||
			response.peerIdentity == peerIdentity_ ||
			response.reconnectToken == reconnectToken_)
			return fail(FullEngineCoopClientResult::InvalidMessage);
	}
	else if (reconnect && (response.peerIdentity != peerIdentity_ ||
		response.reconnectToken != reconnectToken_))
		return fail(FullEngineCoopClientResult::InvalidMessage);

	AdmissionAck acknowledgement;
	acknowledgement.protocolVersion = configuration_.protocolVersion;
	acknowledgement.sessionEpoch = sessionEpoch_;
	acknowledgement.peerIdentity = response.peerIdentity;
	acknowledgement.reconnectToken = response.reconnectToken;
	AdmissionAckBytes encoded{};
	if (!EncodeAdmissionAck(acknowledgement, encoded))
		return fail(FullEngineCoopClientResult::InvalidMessage);
	if (credentialStore_ != nullptr)
	{
		credentialStoreCalling_ = true;
		const bool stored = credentialStore_->persistReconnectCredential(
			acknowledgement);
		credentialStoreCalling_ = false;
		if (!stored)
			return fail(
				FullEngineCoopClientResult::CredentialStorageFailure);
	}
	else if (configuration_.durableReconnectCredentialRequired)
	{
		return fail(FullEngineCoopClientResult::CredentialStorageFailure);
	}
	if (credentialAbandonPending_) clearCredentials();
	peerIdentity_ = response.peerIdentity;
	reconnectToken_ = response.reconnectToken;
	credentialAbandonPending_ = false;
	if (!sendFrame(CoopAdmissionAckMessageName,
		encoded.data(), encoded.size()))
	{
		if (!retrySelfRetirement)
			return fail(FullEngineCoopClientResult::WireFailure);
		clearReplicaState();
		state_ = FullEngineCoopClientState::Disconnected;
		lastResult_ = FullEngineCoopClientResult::WireFailure;
		closeWire();
		return lastResult_;
	}

	clearReplicaState();
	lastAdmissionRejectReason_ = AdmissionRejectReason::None;
	if (retrySelfRetirement)
	{
		// ACK must precede this exact replay on the ordered transport so the server
		// can resolve the transport-owned identity. A locally queued request may
		// have been lost before the server captured it; reconnect never cancels the
		// same-process leave intent or silently restores gameplay.
		AdmissionSelfRetirementRequest retirement;
		retirement.protocolVersion = configuration_.protocolVersion;
		retirement.sessionEpoch = sessionEpoch_;
		retirement.requestId = selfRetirementRequestId_;
		AdmissionSelfRetirementRequestBytes retirementBytes{};
		if (!EncodeAdmissionSelfRetirementRequest(
				retirement, retirementBytes))
			return fail(FullEngineCoopClientResult::InvalidMessage);
		selfRetirementResumeState_ =
			FullEngineCoopClientState::AwaitingBaseline;
		state_ = FullEngineCoopClientState::Retiring;
		if (!sendFrame(CoopAdmissionSelfRetirementRequestMessageName,
				retirementBytes.data(), retirementBytes.size()))
		{
			state_ = FullEngineCoopClientState::Disconnected;
			lastResult_ = FullEngineCoopClientResult::WireFailure;
			closeWire();
			return lastResult_;
		}
		lastResult_ = FullEngineCoopClientResult::Success;
		return lastResult_;
	}
	selfRetirementAwaitingOutcome_ = false;
	selfRetirementRequestId_ = 0;
	selfRetirementResumeState_ = FullEngineCoopClientState::Disconnected;
	state_ = FullEngineCoopClientState::AwaitingBaseline;
	lastResult_ = FullEngineCoopClientResult::Success;
	return lastResult_;
}

FullEngineCoopClientResult FullEngineCoopClient::receiveBaseline(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_)
		return FullEngineCoopClientResult::InvalidState;
	if (state_ != FullEngineCoopClientState::AwaitingBaseline &&
		state_ != FullEngineCoopClientState::Active &&
		state_ != FullEngineCoopClientState::Retiring &&
		state_ != FullEngineCoopClientState::ResyncRequired)
		return FullEngineCoopClientResult::InvalidState;
	const bool retiring = state_ == FullEngineCoopClientState::Retiring;
	const bool replacingActiveState =
		state_ == FullEngineCoopClientState::Active ||
		state_ == FullEngineCoopClientState::ResyncRequired ||
		(retiring && hasAcceptedState_);
	if (bytes == nullptr || size > configuration_.maximumInboundWireBytes ||
		size > MaximumCoopTacticalBaselineWireSize)
		return replacingActiveState
			? requestResync(CoopTacticalResyncReason::InvalidEnvelope, true)
			: fail(FullEngineCoopClientResult::InvalidMessage);

	CoopTacticalBaseline baseline;
	const CoopTacticalCodecResult decoded = DecodeCoopTacticalBaseline(
		bytes, size, baseline);
	if (decoded != CoopTacticalCodecResult::Success)
		return decoded == CoopTacticalCodecResult::AllocationFailure
			? fail(FullEngineCoopClientResult::AllocationFailure)
			: (replacingActiveState
				? requestResync(
					decoded == CoopTacticalCodecResult::ChecksumMismatch
						? CoopTacticalResyncReason::PayloadChecksumMismatch
						: CoopTacticalResyncReason::InvalidEnvelope,
					true)
				: fail(FullEngineCoopClientResult::InvalidMessage));
	bool preserveOutstanding = false;
	bool replacementCursorValid = true;
	if (replacingActiveState)
	{
		if (outstandingCommandId_ == 0)
			replacementCursorValid =
				baseline.nextExpectedCommandId == nextExpectedCommandId_;
		else if (baseline.nextExpectedCommandId ==
			outstandingNextExpectedCommandId_)
			preserveOutstanding = hasAcceptedState_ &&
				baseline.state.worldGeneration ==
					acceptedState_.worldGeneration;
		else
			replacementCursorValid =
				baseline.nextExpectedCommandId == nextExpectedCommandId_;
	}
	if (baseline.state.sessionEpoch != sessionEpoch_ ||
		baseline.assignedActors.size() >
			configuration_.maximumAssignedActors ||
		(!replacingActiveState &&
		 !CursorAtOrAfter(baseline.nextExpectedCommandId,
			nextExpectedCommandId_)) || !replacementCursorValid)
		return replacingActiveState
			? requestResync(CoopTacticalResyncReason::StateMismatch, true)
			: fail(FullEngineCoopClientResult::InvalidMessage);
	if (replacingActiveState && hasAcceptedState_)
	{
		if (baseline.state.worldGeneration <
			acceptedState_.worldGeneration)
			return requestResync(
				CoopTacticalResyncReason::StateMismatch, true);
		if (baseline.state.worldGeneration ==
			acceptedState_.worldGeneration &&
			(baseline.state.revision < acceptedState_.revision ||
				baseline.state.turnSerial < acceptedState_.turnSerial ||
				(baseline.state.revision == acceptedState_.revision &&
					baseline.state.turnSerial !=
						acceptedState_.turnSerial)))
			return requestResync(
				CoopTacticalResyncReason::StateMismatch, true);
	}

	replicaApplying_ = true;
	const FullEngineCoopReplicaApplyResult applied =
		replica_.applyBaseline(baseline);
	replicaApplying_ = false;
	if (applied !=
		FullEngineCoopReplicaApplyResult::Committed)
		return requestResync(
			CoopTacticalResyncReason::BaselineRejected, true);

	acceptedState_ = baseline.state;
	hasAcceptedState_ = true;
	assignedActorCount_ = baseline.assignedActors.size();
	for (std::size_t index = 0; index < assignedActorCount_; ++index)
		assignedActors_[index] = baseline.assignedActors[index];
	for (std::size_t index = assignedActorCount_;
		index < assignedActors_.size(); ++index)
		assignedActors_[index] = TacticalEntityId{};
	nextExpectedCommandId_ = baseline.nextExpectedCommandId;
	if (!preserveOutstanding)
	{
		outstandingCommandId_ = 0;
		outstandingNextExpectedCommandId_ = 0;
	}
	lastDeltaId_ = 0;
	acceptedBaselineId_ = baseline.baselineId;
	lastPayloadChecksum_ = baseline.payloadChecksum;

	CoopTacticalBaselineAck acknowledgement;
	acknowledgement.state = baseline.state;
	acknowledgement.peerIdentity = peerIdentity_;
	acknowledgement.baselineId = baseline.baselineId;
	acknowledgement.payloadChecksum = baseline.payloadChecksum;
	acknowledgement.nextExpectedCommandId =
		baseline.nextExpectedCommandId;
	CoopTacticalBaselineAckBytes encoded{};
	if (EncodeCoopTacticalBaselineAck(acknowledgement, encoded) !=
		CoopTacticalCodecResult::Success)
		return fail(FullEngineCoopClientResult::InvalidMessage);
	if (!sendFrame(CoopTacticalBaselineAckMessageName,
		encoded.data(), encoded.size()))
		return fail(FullEngineCoopClientResult::WireFailure);

	if (retiring)
		selfRetirementResumeState_ = FullEngineCoopClientState::Active;
	else
		state_ = FullEngineCoopClientState::Active;
	resyncAttempts_ = 0;
	lastResult_ = FullEngineCoopClientResult::Success;
	return lastResult_;
}

FullEngineCoopClientResult FullEngineCoopClient::receiveDelta(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_)
		return FullEngineCoopClientResult::InvalidState;
	if (resyncPending()) return FullEngineCoopClientResult::Success;
	if (state_ != FullEngineCoopClientState::Active &&
		state_ != FullEngineCoopClientState::Retiring)
		return state_ == FullEngineCoopClientState::AwaitingBaseline
			? fail(FullEngineCoopClientResult::InvalidMessage)
			: FullEngineCoopClientResult::InvalidState;
	if (bytes == nullptr || size > configuration_.maximumInboundWireBytes ||
		size > MaximumCoopTacticalDeltaWireSize)
		return requestResync(CoopTacticalResyncReason::InvalidEnvelope);

	CoopTacticalDelta delta;
	const CoopTacticalCodecResult decoded = DecodeCoopTacticalDelta(
		bytes, size, delta);
	if (decoded != CoopTacticalCodecResult::Success)
		return decoded == CoopTacticalCodecResult::AllocationFailure
			? fail(FullEngineCoopClientResult::AllocationFailure)
			: requestResync(decoded == CoopTacticalCodecResult::ChecksumMismatch
				? CoopTacticalResyncReason::PayloadChecksumMismatch
				: CoopTacticalResyncReason::InvalidEnvelope);
	if (!hasAcceptedState_ || delta.state.sessionEpoch != sessionEpoch_ ||
		delta.state.worldGeneration != acceptedState_.worldGeneration ||
		delta.baseRevision != acceptedState_.revision ||
		delta.state.revision <= acceptedState_.revision ||
		delta.state.turnSerial < acceptedState_.turnSerial ||
		!DeltaTurnEdgeValid(delta.delta,
			acceptedState_.turnSerial, delta.state.turnSerial))
		return requestResync(CoopTacticalResyncReason::StateMismatch);
	if (lastDeltaId_ != 0 &&
		(lastDeltaId_ == (std::numeric_limits<std::uint64_t>::max)() ||
			delta.deltaId != lastDeltaId_ + 1))
		return requestResync(CoopTacticalResyncReason::DeltaSequenceGap);

	replicaApplying_ = true;
	const FullEngineCoopReplicaApplyResult applied =
		replica_.applyDelta(delta);
	replicaApplying_ = false;
	if (applied !=
		FullEngineCoopReplicaApplyResult::Committed)
		return requestResync(CoopTacticalResyncReason::ReplicaRejected);

	acceptedState_ = delta.state;
	lastDeltaId_ = delta.deltaId;
	lastPayloadChecksum_ = delta.payloadChecksum;
	CoopTacticalDeltaAck acknowledgement;
	acknowledgement.state = delta.state;
	acknowledgement.peerIdentity = peerIdentity_;
	acknowledgement.deltaId = delta.deltaId;
	acknowledgement.payloadChecksum = delta.payloadChecksum;
	CoopTacticalDeltaAckBytes encoded{};
	if (EncodeCoopTacticalDeltaAck(acknowledgement, encoded) !=
		CoopTacticalCodecResult::Success)
		return fail(FullEngineCoopClientResult::InvalidMessage);
	if (!sendFrame(CoopTacticalDeltaAckMessageName,
		encoded.data(), encoded.size()))
		return fail(FullEngineCoopClientResult::WireFailure);

	lastResult_ = FullEngineCoopClientResult::Success;
	return lastResult_;
}

FullEngineCoopClientResult FullEngineCoopClient::receiveIntentReceipt(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_)
		return FullEngineCoopClientResult::InvalidState;
	if (resyncPending()) return FullEngineCoopClientResult::Success;
	if (state_ != FullEngineCoopClientState::Active &&
		state_ != FullEngineCoopClientState::Retiring)
		return FullEngineCoopClientResult::InvalidState;
	if (bytes == nullptr || size > configuration_.maximumInboundWireBytes ||
		size != CoopTacticalIntentReceiptWireSize)
		return fail(FullEngineCoopClientResult::InvalidMessage);

	CoopTacticalIntentReceipt receipt;
	const CoopTacticalCodecResult decoded = DecodeCoopTacticalIntentReceipt(
		bytes, size, receipt);
	if (decoded != CoopTacticalCodecResult::Success)
		return decoded == CoopTacticalCodecResult::AllocationFailure
			? fail(FullEngineCoopClientResult::AllocationFailure)
			: fail(FullEngineCoopClientResult::InvalidMessage);
	CoopTacticalIntentReceiptBytes receiptBytes{};
	std::copy(bytes, bytes + size, receiptBytes.begin());
	if (receipt.peerIdentity != peerIdentity_ ||
		receipt.state.sessionEpoch != sessionEpoch_ ||
		!ReceiptCursorValid(receipt))
		return fail(FullEngineCoopClientResult::InvalidMessage);
	if (!hasAcceptedState_ ||
		!ReceiptStateNotFuture(receipt.state, acceptedState_))
		return requestResync(CoopTacticalResyncReason::StateMismatch);

	const bool reportsSequenceMismatch =
		ReportsInvalidCommandSequence(receipt);
	const bool reportsSequenceExhausted =
		ReportsInboxSequenceExhausted(receipt);
	const bool matchesOutstanding = outstandingCommandId_ != 0 &&
		receipt.commandId == outstandingCommandId_ &&
		(reportsSequenceMismatch || reportsSequenceExhausted ||
			receipt.nextExpectedCommandId ==
				outstandingNextExpectedCommandId_);
	const bool alreadyConsumed = CursorAlreadyConsumed(
		receipt.commandId, receipt.nextExpectedCommandId,
		nextExpectedCommandId_);
	if (!matchesOutstanding && !alreadyConsumed)
		return fail(FullEngineCoopClientResult::InvalidMessage);
	if (matchesOutstanding &&
		receipt.state.worldGeneration != acceptedState_.worldGeneration)
		return requestResync(CoopTacticalResyncReason::StateMismatch);
	if (matchesOutstanding && reportsSequenceMismatch)
		return requestResync(CoopTacticalResyncReason::StateMismatch);
	if (!acceptReceiptHistory(receipt, receiptBytes))
		return fail(FullEngineCoopClientResult::InvalidMessage);
	lastIntentReceipt_ = receipt;
	hasLastIntentReceipt_ = true;
	if (receipt.reason ==
		CoopTacticalIntentReceiptReason::AuthoritySequenceExhausted)
	{
		// This terminal result is consuming, so retain its exact cursor and
		// diagnostic receipt, but the active authority cannot ever accept
		// another command. Close instead of unlocking into endless rejections.
		if (matchesOutstanding)
			nextExpectedCommandId_ = receipt.nextExpectedCommandId;
		return fail(FullEngineCoopClientResult::SequenceExhausted);
	}
	if (!matchesOutstanding)
	{
		lastResult_ = FullEngineCoopClientResult::Success;
		return lastResult_;
	}

	if (receipt.status == CoopTacticalIntentReceiptStatus::Queued)
	{
		lastResult_ = FullEngineCoopClientResult::Success;
		return lastResult_;
	}
	nextExpectedCommandId_ = receipt.nextExpectedCommandId;
	outstandingCommandId_ = 0;
	outstandingNextExpectedCommandId_ = 0;
	lastResult_ = FullEngineCoopClientResult::Success;
	return lastResult_;
}

FullEngineCoopClientResult FullEngineCoopClient::sendIntent(
	TacticalEntityId actor,
	const TacticalIntentPayload& payload) noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_)
		return FullEngineCoopClientResult::InvalidState;
	if (state_ != FullEngineCoopClientState::Active ||
		!hasAcceptedState_)
		return FullEngineCoopClientResult::InvalidState;
	if (outstandingCommandId_ != 0)
		return FullEngineCoopClientResult::IntentOutstanding;
	if (nextExpectedCommandId_ == 0)
		return FullEngineCoopClientResult::SequenceExhausted;
	if (!isActorAssigned(actor))
		return FullEngineCoopClientResult::ActorNotAssigned;

	TacticalIntent intent;
	intent.protocolVersion = TacticalIntentWireVersion;
	intent.sessionEpoch = sessionEpoch_;
	intent.claimedPeerIdentity = peerIdentity_;
	intent.commandId = nextExpectedCommandId_;
	intent.worldGeneration = acceptedState_.worldGeneration;
	intent.baseRevision = acceptedState_.revision;
	intent.turnSerial = acceptedState_.turnSerial;
	intent.actor = actor;
	intent.payload = payload;
	if (!IsStructurallyValidTacticalIntent(intent))
		return FullEngineCoopClientResult::InvalidIntent;

	std::vector<std::uint8_t> encoded;
	const TacticalIntentCodecResult result =
		EncodeTacticalIntent(intent, encoded);
	if (result == TacticalIntentCodecResult::AllocationFailure)
		return fail(FullEngineCoopClientResult::AllocationFailure);
	if (result != TacticalIntentCodecResult::Success ||
		encoded.empty() || encoded.size() > MaximumTacticalIntentWireSize)
		return FullEngineCoopClientResult::InvalidIntent;
	if (!sendFrame(CoopTacticalIntentMessageName,
		encoded.data(), encoded.size()))
		return fail(FullEngineCoopClientResult::WireFailure);

	outstandingCommandId_ = intent.commandId;
	outstandingNextExpectedCommandId_ = CommandAfter(intent.commandId);
	lastResult_ = FullEngineCoopClientResult::Success;
	return lastResult_;
}

FullEngineCoopClientResult
FullEngineCoopClient::requestSelfRetirement() noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_)
		return FullEngineCoopClientResult::InvalidState;
	if ((state_ != FullEngineCoopClientState::AwaitingBaseline &&
		 state_ != FullEngineCoopClientState::Active) ||
		!hasReconnectCredential() || sessionEpoch_ == 0 ||
		outstandingCommandId_ != 0 || selfRetirementAwaitingOutcome_ ||
		nextSelfRetirementRequestId_ == 0)
		return FullEngineCoopClientResult::InvalidState;

	AdmissionSelfRetirementRequest request;
	request.protocolVersion = configuration_.protocolVersion;
	request.sessionEpoch = sessionEpoch_;
	request.requestId = nextSelfRetirementRequestId_;
	AdmissionSelfRetirementRequestBytes encoded{};
	if (!EncodeAdmissionSelfRetirementRequest(request, encoded))
		return fail(FullEngineCoopClientResult::InvalidMessage);

	// Publish the exact RAM-retained intent before touching the transport. If the
	// enqueue or connection fails, the same process replays this request ID after
	// its next admission ACK. A new process intentionally has no such marker.
	selfRetirementResumeState_ = state_;
	selfRetirementRequestId_ = request.requestId;
	selfRetirementAwaitingOutcome_ = true;
	nextSelfRetirementRequestId_ = request.requestId ==
		(std::numeric_limits<std::uint64_t>::max)()
		? 0 : request.requestId + 1;
	state_ = FullEngineCoopClientState::Retiring;
	if (!sendFrame(CoopAdmissionSelfRetirementRequestMessageName,
		encoded.data(), encoded.size()))
	{
		clearReplicaState();
		state_ = FullEngineCoopClientState::Disconnected;
		lastResult_ = FullEngineCoopClientResult::WireFailure;
		closeWire();
		return lastResult_;
	}
	lastResult_ = FullEngineCoopClientResult::Success;
	return lastResult_;
}

FullEngineCoopClientResult
FullEngineCoopClient::receiveSelfRetirementResult(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (replicaApplying_ || wireCalling_ || credentialStoreCalling_)
		return FullEngineCoopClientResult::InvalidState;
	if (state_ != FullEngineCoopClientState::Retiring)
		return FullEngineCoopClientResult::InvalidState;
	if (bytes == nullptr || size > configuration_.maximumInboundWireBytes ||
		size != AdmissionSelfRetirementResultWireSize)
		return fail(FullEngineCoopClientResult::InvalidMessage);

	AdmissionSelfRetirementResult result;
	if (DecodeAdmissionSelfRetirementResult(bytes, size, result) !=
			DecodeResult::Ok ||
		result.protocolVersion != configuration_.protocolVersion ||
		result.sessionEpoch != sessionEpoch_ ||
		result.requestId != selfRetirementRequestId_ ||
		result.peerIdentity != peerIdentity_)
		return fail(FullEngineCoopClientResult::InvalidMessage);
	if (result.result == AdmissionSelfRetirementResultCode::
		TombstoneCapacityReached)
	{
		state_ = selfRetirementResumeState_;
		selfRetirementResumeState_ = FullEngineCoopClientState::Disconnected;
		selfRetirementRequestId_ = 0;
		selfRetirementAwaitingOutcome_ = false;
		lastResult_ = FullEngineCoopClientResult::SelfRetirementRejected;
		return lastResult_;
	}
	if (result.result !=
		AdmissionSelfRetirementResultCode::CredentialRetired)
		return fail(FullEngineCoopClientResult::InvalidMessage);
	return retireCredential();
}

bool FullEngineCoopClient::isActorAssigned(
	TacticalEntityId actor) const noexcept
{
	return std::binary_search(assignedActors_.begin(),
		assignedActors_.begin() + assignedActorCount_, actor);
}

FullEngineCoopClientResult FullEngineCoopClient::fail(
	FullEngineCoopClientResult result) noexcept
{
	clearReplicaState();
	state_ = FullEngineCoopClientState::Failed;
	lastResult_ = result;
	closeWire();
	return result;
}

bool FullEngineCoopClient::resyncPending() const noexcept
{
	return state_ == FullEngineCoopClientState::ResyncRequired ||
		(state_ == FullEngineCoopClientState::Retiring &&
		 selfRetirementResumeState_ ==
			FullEngineCoopClientState::ResyncRequired);
}

FullEngineCoopClientResult FullEngineCoopClient::requestResync(
	CoopTacticalResyncReason reason, bool retry) noexcept
{
	if (!hasAcceptedState_ || acceptedBaselineId_ == 0 ||
		!IsKnownCoopTacticalResyncReason(reason))
		return fail(FullEngineCoopClientResult::InvalidMessage);
	if (resyncPending() && !retry)
		return FullEngineCoopClientResult::ResyncRequired;
	if (resyncAttempts_ >= MaximumResyncAttempts ||
		nextResyncRequestId_ == 0)
		return fail(FullEngineCoopClientResult::ResyncRequired);

	CoopTacticalResyncRequest request;
	request.acceptedState = acceptedState_;
	request.requestId = nextResyncRequestId_;
	request.acceptedBaselineId = acceptedBaselineId_;
	request.lastAppliedDeltaId = lastDeltaId_;
	request.lastPayloadChecksum = lastPayloadChecksum_;
	request.reason = reason;
	request.nextExpectedCommandId = nextExpectedCommandId_;
	CoopTacticalResyncRequestBytes encoded{};
	if (EncodeCoopTacticalResyncRequest(request, encoded) !=
		CoopTacticalCodecResult::Success)
		return fail(FullEngineCoopClientResult::InvalidMessage);
	if (!sendFrame(CoopTacticalResyncRequestMessageName,
		encoded.data(), encoded.size()))
		return fail(FullEngineCoopClientResult::WireFailure);
	++resyncAttempts_;
	nextResyncRequestId_ = nextResyncRequestId_ ==
		(std::numeric_limits<std::uint64_t>::max)()
		? 0 : nextResyncRequestId_ + 1;
	if (state_ == FullEngineCoopClientState::Retiring)
		selfRetirementResumeState_ =
			FullEngineCoopClientState::ResyncRequired;
	else
		state_ = FullEngineCoopClientState::ResyncRequired;
	lastResult_ = FullEngineCoopClientResult::ResyncRequired;
	return lastResult_;
}

FullEngineCoopClientResult FullEngineCoopClient::retireCredential() noexcept
{
	if (!hasReconnectCredential())
		return fail(FullEngineCoopClientResult::InvalidMessage);
	if (credentialStore_ != nullptr)
	{
		AdmissionAck credential;
		credential.protocolVersion = configuration_.protocolVersion;
		credential.sessionEpoch = sessionEpoch_;
		credential.peerIdentity = peerIdentity_;
		credential.reconnectToken = reconnectToken_;
		credentialStoreCalling_ = true;
		const bool retired = credentialStore_->retireReconnectCredential(
			credential);
		credentialStoreCalling_ = false;
		if (!retired)
			return fail(
				FullEngineCoopClientResult::CredentialStorageFailure);
	}
	else if (configuration_.durableReconnectCredentialRequired)
	{
		return fail(FullEngineCoopClientResult::CredentialStorageFailure);
	}
	clearCredentials();
	clearReplicaState();
	credentialAbandonPending_ = false;
	selfRetirementAwaitingOutcome_ = false;
	selfRetirementRequestId_ = 0;
	selfRetirementResumeState_ = FullEngineCoopClientState::Disconnected;
	state_ = FullEngineCoopClientState::Retired;
	lastResult_ = FullEngineCoopClientResult::CredentialRetired;
	closeWire();
	return lastResult_;
}

bool FullEngineCoopClient::sendFrame(const char* messageName,
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (wireCalling_ || messageName == nullptr || bytes == nullptr || size == 0)
		return false;
	wireCalling_ = true;
	const bool sent = wire_.send(messageName, bytes, size);
	wireCalling_ = false;
	return sent;
}

void FullEngineCoopClient::closeWire() noexcept
{
	if (wireCalling_) return;
	wireCalling_ = true;
	wire_.close();
	wireCalling_ = false;
}

void FullEngineCoopClient::clearCredentials() noexcept
{
	peerIdentity_.fill(0);
	reconnectToken_.fill(0);
	nextExpectedCommandId_ = 1;
	clearReceiptHistory();
}

bool FullEngineCoopClient::acceptReceiptHistory(
	const CoopTacticalIntentReceipt& receipt,
	const CoopTacticalIntentReceiptBytes& bytes) noexcept
{
	for (std::size_t offset = 0; offset < receiptHistoryCount_; ++offset)
	{
		ReceiptHistoryEntry& existing = receiptHistory_[
			(receiptHistoryHead_ + offset) % receiptHistory_.size()];
		if (existing.commandId != receipt.commandId) continue;
		if (existing.bytes == bytes) return true;
		if (existing.status != CoopTacticalIntentReceiptStatus::Queued ||
			receipt.status == CoopTacticalIntentReceiptStatus::Queued)
			return false;
		existing.status = receipt.status;
		existing.bytes = bytes;
		return true;
	}

	std::size_t insertion = receiptHistoryCount_;
	if (receiptHistoryCount_ == receiptHistory_.size())
	{
		receiptHistoryHead_ =
			(receiptHistoryHead_ + 1) % receiptHistory_.size();
		--receiptHistoryCount_;
		insertion = receiptHistoryCount_;
	}
	ReceiptHistoryEntry& accepted = receiptHistory_[
		(receiptHistoryHead_ + insertion) % receiptHistory_.size()];
	accepted.commandId = receipt.commandId;
	accepted.status = receipt.status;
	accepted.bytes = bytes;
	++receiptHistoryCount_;
	return true;
}

void FullEngineCoopClient::clearReceiptHistory() noexcept
{
	receiptHistory_ = {};
	receiptHistoryHead_ = 0;
	receiptHistoryCount_ = 0;
	lastIntentReceipt_ = CoopTacticalIntentReceipt{};
	hasLastIntentReceipt_ = false;
}

void FullEngineCoopClient::clearReplicaState() noexcept
{
	acceptedState_ = CoopTacticalStateIdentity{};
	hasAcceptedState_ = false;
	assignedActors_ = {};
	assignedActorCount_ = 0;
	outstandingCommandId_ = 0;
	outstandingNextExpectedCommandId_ = 0;
	lastDeltaId_ = 0;
	acceptedBaselineId_ = 0;
	lastPayloadChecksum_ = 0;
	resyncAttempts_ = 0;
}

void FullEngineCoopClient::clearConnectionState() noexcept
{
	clearReplicaState();
	nextResyncRequestId_ = 1;
	credentialAbandonPending_ = false;
	lastAdmissionRejectReason_ = AdmissionRejectReason::None;
}
}
