#include "FullEngineCoopCampaignSyncClient.h"

#include <algorithm>
#include <cstring>

namespace CoopSession
{
namespace
{
class ScratchCallbackGuard
{
public:
	explicit ScratchCallbackGuard(bool& active) noexcept : active_(active)
	{
		active_ = true;
	}
	~ScratchCallbackGuard() { active_ = false; }

private:
	bool& active_;
};

bool ZeroIdentity(const PeerIdentity& identity) noexcept
{
	return std::all_of(identity.begin(), identity.end(),
		[](std::uint8_t value) noexcept { return value == 0; });
}

CoopCampaignSyncFailureReason CommitFailureReason(
	FullEngineCoopCampaignScratchCommitResult result) noexcept
{
	switch (result)
	{
		case FullEngineCoopCampaignScratchCommitResult::Committed:
			return CoopCampaignSyncFailureReason::None;
		case FullEngineCoopCampaignScratchCommitResult::HashMismatch:
			return CoopCampaignSyncFailureReason::HashMismatch;
		case FullEngineCoopCampaignScratchCommitResult::StorageFailure:
			return CoopCampaignSyncFailureReason::StorageFailure;
		case FullEngineCoopCampaignScratchCommitResult::LoadFailed:
			return CoopCampaignSyncFailureReason::LoadFailed;
		case FullEngineCoopCampaignScratchCommitResult::CompatibilityMismatch:
			return CoopCampaignSyncFailureReason::CompatibilityMismatch;
	}
	return CoopCampaignSyncFailureReason::ProtocolViolation;
}
}

FullEngineCoopCampaignSyncClient::FullEngineCoopCampaignSyncClient(
	FullEngineCoopCampaignScratch& scratch,
	FullEngineCoopCampaignSyncClientWire& wire) noexcept
	: scratch_(scratch), wire_(wire)
{
}

FullEngineCoopCampaignSyncClientResult
FullEngineCoopCampaignSyncClient::beginSession(
	const CoopCampaignBootstrapDescriptor& bootstrap,
	const PeerIdentity& peerIdentity) noexcept
{
	if (inCallback_ || wireCalling_)
		return FullEngineCoopCampaignSyncClientResult::ReentrantCall;
	if (state_ != FullEngineCoopCampaignSyncClientState::Disconnected)
		return FullEngineCoopCampaignSyncClientResult::InvalidState;
	if (!IsValidCoopCampaignBootstrapDescriptor(bootstrap) ||
		ZeroIdentity(peerIdentity))
		return fail(
			FullEngineCoopCampaignSyncClientResult::InvalidConfiguration,
			CoopCampaignSyncFailureReason::ProtocolViolation);

	{
		ScratchCallbackGuard guard(inCallback_);
		scratch_.abort();
	}
	resetTransfer();
	bootstrap_ = bootstrap;
	peerIdentity_ = peerIdentity;
	configured_ = true;
	state_ = FullEngineCoopCampaignSyncClientState::AwaitingMetadata;
	lastResult_ = FullEngineCoopCampaignSyncClientResult::Success;
	terminalResult_ = FullEngineCoopCampaignSyncClientResult::Success;
	failureReason_ = CoopCampaignSyncFailureReason::None;
	return lastResult_;
}

void FullEngineCoopCampaignSyncClient::disconnect() noexcept
{
	if (inCallback_ || wireCalling_) return;
	if (state_ != FullEngineCoopCampaignSyncClientState::Disconnected &&
		state_ != FullEngineCoopCampaignSyncClientState::Failed)
		closeWire();
	{
		ScratchCallbackGuard guard(inCallback_);
		scratch_.abort();
	}
	resetTransfer();
	configured_ = false;
	peerIdentity_ = {};
	bootstrap_ = {};
	state_ = FullEngineCoopCampaignSyncClientState::Disconnected;
	lastResult_ = FullEngineCoopCampaignSyncClientResult::Success;
	terminalResult_ = FullEngineCoopCampaignSyncClientResult::Success;
	failureReason_ = CoopCampaignSyncFailureReason::None;
}

FullEngineCoopCampaignSyncClientResult
FullEngineCoopCampaignSyncClient::receiveMetadata(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (inCallback_ || wireCalling_)
		return FullEngineCoopCampaignSyncClientResult::ReentrantCall;
	if (!configured_ ||
		(state_ != FullEngineCoopCampaignSyncClientState::AwaitingMetadata &&
		 state_ != FullEngineCoopCampaignSyncClientState::Receiving &&
		 state_ != FullEngineCoopCampaignSyncClientState::Ready))
		return FullEngineCoopCampaignSyncClientResult::InvalidState;

	CoopCampaignSyncMetadata incoming;
	const CoopCampaignSyncCodecResult decoded =
		DecodeCoopCampaignSyncMetadata(bytes, size, incoming);
	if (decoded != CoopCampaignSyncCodecResult::Success)
		return fail(FullEngineCoopCampaignSyncClientResult::InvalidMessage,
			CoopCampaignSyncFailureReason::MalformedMessage);
	if (!transferMatchesBootstrap(incoming.transfer))
		return fail(FullEngineCoopCampaignSyncClientResult::DescriptorMismatch,
			CoopCampaignSyncFailureReason::CheckpointMismatch);

	if (hasMetadata_ &&
		SameCoopCampaignSyncTransfer(metadata_.transfer, incoming.transfer))
	{
		if (metadata_.worldMinutes != incoming.worldMinutes)
			return fail(
				FullEngineCoopCampaignSyncClientResult::DescriptorMismatch,
				CoopCampaignSyncFailureReason::CheckpointMismatch);
		return queueAck(nextExpectedOffset_, previousChunkChecksum_);
	}
	if (hasMetadata_ && incoming.transfer.checkpointGeneration <=
		metadata_.transfer.checkpointGeneration)
		return FullEngineCoopCampaignSyncClientResult::StaleMessage;

	FullEngineCoopCampaignScratchBeginResult begun =
		FullEngineCoopCampaignScratchBeginResult::StorageFailure;
	{
		ScratchCallbackGuard guard(inCallback_);
		if (hasMetadata_) scratch_.abort();
		begun = scratch_.begin(incoming);
	}
	metadata_ = incoming;
	hasMetadata_ = true;
	nextExpectedOffset_ = 0;
	previousChunkOffset_ = 0;
	previousChunkChecksum_ = 0;
	previousChunkSize_ = 0;
	pendingOutbound_ = {};
	if (begun != FullEngineCoopCampaignScratchBeginResult::Success)
	{
		const CoopCampaignSyncFailureReason reason = begun ==
			FullEngineCoopCampaignScratchBeginResult::CapacityReached
			? CoopCampaignSyncFailureReason::CapacityReached
			: CoopCampaignSyncFailureReason::StorageFailure;
		return rejectTransfer(
			FullEngineCoopCampaignSyncClientResult::StorageFailure, reason);
	}
	state_ = FullEngineCoopCampaignSyncClientState::Receiving;
	failureReason_ = CoopCampaignSyncFailureReason::None;
	return queueAck(0, 0);
}

FullEngineCoopCampaignSyncClientResult
FullEngineCoopCampaignSyncClient::receiveChunk(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (inCallback_ || wireCalling_)
		return FullEngineCoopCampaignSyncClientResult::ReentrantCall;
	if (state_ != FullEngineCoopCampaignSyncClientState::Receiving ||
		!hasMetadata_)
		return FullEngineCoopCampaignSyncClientResult::InvalidState;

	CoopCampaignSyncChunk chunk;
	const CoopCampaignSyncCodecResult decoded =
		DecodeCoopCampaignSyncChunk(bytes, size, chunk);
	if (decoded != CoopCampaignSyncCodecResult::Success)
		return fail(FullEngineCoopCampaignSyncClientResult::InvalidMessage,
			decoded == CoopCampaignSyncCodecResult::ChecksumMismatch
				? CoopCampaignSyncFailureReason::ChunkChecksumMismatch
				: CoopCampaignSyncFailureReason::MalformedMessage);
	if (!SameCoopCampaignSyncTransfer(
		metadata_.transfer, chunk.transfer))
	{
		if (staleTransfer(chunk.transfer))
			return FullEngineCoopCampaignSyncClientResult::StaleMessage;
		return fail(FullEngineCoopCampaignSyncClientResult::DescriptorMismatch,
			CoopCampaignSyncFailureReason::TransferMismatch);
	}

	const std::uint64_t chunkEnd = chunk.offset + chunk.payload.size();
	if (chunk.offset == previousChunkOffset_ &&
		chunk.payload.size() == previousChunkSize_ &&
		chunk.payloadChecksum == previousChunkChecksum_ &&
		chunkEnd == nextExpectedOffset_)
		return queueAck(nextExpectedOffset_, previousChunkChecksum_);

	const CoopCampaignSyncChunkSequenceResult sequence =
		ValidateCoopCampaignSyncChunkAtOffset(
			metadata_, chunk, nextExpectedOffset_);
	if (sequence != CoopCampaignSyncChunkSequenceResult::Success)
	{
		if (sequence == CoopCampaignSyncChunkSequenceResult::Gap ||
			sequence == CoopCampaignSyncChunkSequenceResult::Overlap ||
			sequence ==
				CoopCampaignSyncChunkSequenceResult::InvalidExpectedOffset)
			return queueResync(
				CoopCampaignSyncFailureReason::SequenceMismatch);
		return fail(FullEngineCoopCampaignSyncClientResult::InvalidMessage,
			sequence == CoopCampaignSyncChunkSequenceResult::ChecksumMismatch
				? CoopCampaignSyncFailureReason::ChunkChecksumMismatch
				: CoopCampaignSyncFailureReason::CheckpointMismatch);
	}

	FullEngineCoopCampaignScratchWriteResult written =
		FullEngineCoopCampaignScratchWriteResult::StorageFailure;
	{
		ScratchCallbackGuard guard(inCallback_);
		written = scratch_.writeExact(chunk.offset, chunk.payload.data(),
			chunk.payload.size());
	}
	if (written != FullEngineCoopCampaignScratchWriteResult::Success)
		return rejectTransfer(
			FullEngineCoopCampaignSyncClientResult::StorageFailure,
			CoopCampaignSyncFailureReason::StorageFailure);

	previousChunkOffset_ = chunk.offset;
	previousChunkSize_ = static_cast<std::uint32_t>(chunk.payload.size());
	previousChunkChecksum_ = chunk.payloadChecksum;
	nextExpectedOffset_ = chunkEnd;
	return queueAck(nextExpectedOffset_, previousChunkChecksum_);
}

FullEngineCoopCampaignSyncClientResult
FullEngineCoopCampaignSyncClient::receiveComplete(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (inCallback_ || wireCalling_)
		return FullEngineCoopCampaignSyncClientResult::ReentrantCall;
	if ((state_ != FullEngineCoopCampaignSyncClientState::Receiving &&
		 state_ != FullEngineCoopCampaignSyncClientState::CommitPending &&
		 state_ != FullEngineCoopCampaignSyncClientState::Ready) ||
		!hasMetadata_)
		return FullEngineCoopCampaignSyncClientResult::InvalidState;

	CoopCampaignSyncComplete complete;
	if (DecodeCoopCampaignSyncComplete(bytes, size, complete) !=
		CoopCampaignSyncCodecResult::Success)
		return fail(FullEngineCoopCampaignSyncClientResult::InvalidMessage,
			CoopCampaignSyncFailureReason::MalformedMessage);
	if (!SameCoopCampaignSyncTransfer(
		metadata_.transfer, complete.transfer))
	{
		if (staleTransfer(complete.transfer))
			return FullEngineCoopCampaignSyncClientResult::StaleMessage;
		return fail(FullEngineCoopCampaignSyncClientResult::DescriptorMismatch,
			CoopCampaignSyncFailureReason::TransferMismatch);
	}
	if (state_ == FullEngineCoopCampaignSyncClientState::CommitPending)
		return queueResult(CoopCampaignSyncResultStatus::Committed,
			CoopCampaignSyncFailureReason::None);
	if (state_ == FullEngineCoopCampaignSyncClientState::Ready)
	{
		// If the replayed result is backpressured, return to the same commit
		// gate so newer metadata cannot erase the server's renewed obligation.
		state_ = FullEngineCoopCampaignSyncClientState::CommitPending;
		return queueResult(CoopCampaignSyncResultStatus::Committed,
			CoopCampaignSyncFailureReason::None);
	}
	if (nextExpectedOffset_ != metadata_.transfer.totalSize)
		return queueResync(
			CoopCampaignSyncFailureReason::SequenceMismatch);

	FullEngineCoopCampaignScratchCommitResult committed =
		FullEngineCoopCampaignScratchCommitResult::StorageFailure;
	{
		ScratchCallbackGuard guard(inCallback_);
		committed = scratch_.commitAndLoad(metadata_);
	}
	if (committed != FullEngineCoopCampaignScratchCommitResult::Committed)
		return rejectTransfer(
			FullEngineCoopCampaignSyncClientResult::CommitFailed,
			CommitFailureReason(committed));
	state_ = FullEngineCoopCampaignSyncClientState::CommitPending;
	return queueResult(CoopCampaignSyncResultStatus::Committed,
		CoopCampaignSyncFailureReason::None);
}

FullEngineCoopCampaignSyncClientResult
FullEngineCoopCampaignSyncClient::receiveReject(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (inCallback_ || wireCalling_)
		return FullEngineCoopCampaignSyncClientResult::ReentrantCall;
	if (!hasMetadata_ || !configured_ ||
		state_ == FullEngineCoopCampaignSyncClientState::Disconnected)
		return FullEngineCoopCampaignSyncClientResult::InvalidState;
	if (state_ != FullEngineCoopCampaignSyncClientState::Rejected)
		return fail(FullEngineCoopCampaignSyncClientResult::InvalidState,
			CoopCampaignSyncFailureReason::ProtocolViolation);

	CoopCampaignSyncReject rejection;
	if (DecodeCoopCampaignSyncReject(bytes, size, rejection) !=
		CoopCampaignSyncCodecResult::Success)
		return fail(FullEngineCoopCampaignSyncClientResult::InvalidMessage,
			CoopCampaignSyncFailureReason::MalformedMessage);
	if (!SameCoopCampaignSyncTransfer(
		metadata_.transfer, rejection.transfer))
	{
		if (staleTransfer(rejection.transfer))
			return FullEngineCoopCampaignSyncClientResult::StaleMessage;
		return fail(FullEngineCoopCampaignSyncClientResult::DescriptorMismatch,
			CoopCampaignSyncFailureReason::TransferMismatch);
	}
	{
		ScratchCallbackGuard guard(inCallback_);
		scratch_.abort();
	}
	pendingOutbound_ = {};
	state_ = FullEngineCoopCampaignSyncClientState::Rejected;
	failureReason_ = rejection.reason;
	lastResult_ = FullEngineCoopCampaignSyncClientResult::ServerRejected;
	terminalResult_ = lastResult_;
	return lastResult_;
}

FullEngineCoopCampaignSyncClientResult
FullEngineCoopCampaignSyncClient::flushOutbound() noexcept
{
	if (inCallback_ || wireCalling_)
		return FullEngineCoopCampaignSyncClientResult::ReentrantCall;
	if (pendingOutbound_.kind == PendingKind::None)
	{
		if (state_ == FullEngineCoopCampaignSyncClientState::Failed ||
			state_ == FullEngineCoopCampaignSyncClientState::Rejected)
		{
			lastResult_ = terminalResult_;
			return lastResult_;
		}
		if (state_ == FullEngineCoopCampaignSyncClientState::Disconnected)
			return FullEngineCoopCampaignSyncClientResult::InvalidState;
		lastResult_ = FullEngineCoopCampaignSyncClientResult::Success;
		return lastResult_;
	}
	const char* name = nullptr;
	switch (pendingOutbound_.kind)
	{
		case PendingKind::Ack: name = CoopCampaignSyncAckMessageName; break;
		case PendingKind::Result:
			name = CoopCampaignSyncResultMessageName;
			break;
		case PendingKind::Resync:
			name = CoopCampaignSyncResyncMessageName;
			break;
		case PendingKind::None: break;
	}
	if (name == nullptr || pendingOutbound_.size == 0)
		return fail(FullEngineCoopCampaignSyncClientResult::WireFailure,
			CoopCampaignSyncFailureReason::ProtocolViolation);
	wireCalling_ = true;
	const bool sent = wire_.send(name, pendingOutbound_.bytes.data(),
		pendingOutbound_.size);
	wireCalling_ = false;
	if (!sent)
	{
		lastResult_ = FullEngineCoopCampaignSyncClientResult::Backpressured;
		return lastResult_;
	}
	const PendingKind sentKind = pendingOutbound_.kind;
	pendingOutbound_ = {};
	if (sentKind == PendingKind::Result)
	{
		if (state_ == FullEngineCoopCampaignSyncClientState::CommitPending)
			state_ = FullEngineCoopCampaignSyncClientState::Ready;
		else if (state_ ==
			FullEngineCoopCampaignSyncClientState::RejectPending)
		{
			state_ = FullEngineCoopCampaignSyncClientState::Rejected;
			lastResult_ = terminalResult_;
			return FullEngineCoopCampaignSyncClientResult::Success;
		}
	}
	lastResult_ = FullEngineCoopCampaignSyncClientResult::Success;
	return lastResult_;
}

FullEngineCoopCampaignSyncClientResult
FullEngineCoopCampaignSyncClient::fail(
	FullEngineCoopCampaignSyncClientResult result,
	CoopCampaignSyncFailureReason reason) noexcept
{
	if (!inCallback_)
	{
		ScratchCallbackGuard guard(inCallback_);
		scratch_.abort();
	}
	pendingOutbound_ = {};
	failureReason_ = reason;
	state_ = FullEngineCoopCampaignSyncClientState::Failed;
	lastResult_ = result;
	terminalResult_ = result;
	closeWire();
	return lastResult_;
}

FullEngineCoopCampaignSyncClientResult
FullEngineCoopCampaignSyncClient::queueAck(
	std::uint64_t cursor, std::uint32_t checksum) noexcept
{
	CoopCampaignSyncAck acknowledgement;
	acknowledgement.transfer = metadata_.transfer;
	acknowledgement.peerIdentity = peerIdentity_;
	acknowledgement.nextExpectedOffset = cursor;
	acknowledgement.precedingChunkChecksum = checksum;
	CoopCampaignSyncAckBytes encoded{};
	if (EncodeCoopCampaignSyncAck(acknowledgement, encoded) !=
		CoopCampaignSyncCodecResult::Success)
		return fail(FullEngineCoopCampaignSyncClientResult::InvalidMessage,
			CoopCampaignSyncFailureReason::ProtocolViolation);
	pendingOutbound_.kind = PendingKind::Ack;
	pendingOutbound_.size = encoded.size();
	std::copy(encoded.begin(), encoded.end(),
		pendingOutbound_.bytes.begin());
	return flushOutbound();
}

FullEngineCoopCampaignSyncClientResult
FullEngineCoopCampaignSyncClient::queueResync(
	CoopCampaignSyncFailureReason reason) noexcept
{
	CoopCampaignSyncResync resync;
	resync.transfer = metadata_.transfer;
	resync.peerIdentity = peerIdentity_;
	resync.expectedOffset = nextExpectedOffset_;
	resync.precedingChunkChecksum = previousChunkChecksum_;
	resync.reason = reason;
	CoopCampaignSyncResyncBytes encoded{};
	if (EncodeCoopCampaignSyncResync(resync, encoded) !=
		CoopCampaignSyncCodecResult::Success)
		return fail(FullEngineCoopCampaignSyncClientResult::InvalidMessage,
			CoopCampaignSyncFailureReason::ProtocolViolation);
	pendingOutbound_.kind = PendingKind::Resync;
	pendingOutbound_.size = encoded.size();
	std::copy(encoded.begin(), encoded.end(),
		pendingOutbound_.bytes.begin());
	const FullEngineCoopCampaignSyncClientResult flushed = flushOutbound();
	if (flushed == FullEngineCoopCampaignSyncClientResult::Success)
	{
		lastResult_ = FullEngineCoopCampaignSyncClientResult::ResyncRequested;
		return lastResult_;
	}
	return flushed;
}

FullEngineCoopCampaignSyncClientResult
FullEngineCoopCampaignSyncClient::queueResult(
	CoopCampaignSyncResultStatus status,
	CoopCampaignSyncFailureReason reason) noexcept
{
	CoopCampaignSyncResult result;
	result.transfer = metadata_.transfer;
	result.peerIdentity = peerIdentity_;
	result.status = status;
	result.reason = reason;
	CoopCampaignSyncResultBytes encoded{};
	if (EncodeCoopCampaignSyncResult(result, encoded) !=
		CoopCampaignSyncCodecResult::Success)
		return fail(FullEngineCoopCampaignSyncClientResult::InvalidMessage,
			CoopCampaignSyncFailureReason::ProtocolViolation);
	pendingOutbound_.kind = PendingKind::Result;
	pendingOutbound_.size = encoded.size();
	std::copy(encoded.begin(), encoded.end(),
		pendingOutbound_.bytes.begin());
	return flushOutbound();
}

FullEngineCoopCampaignSyncClientResult
FullEngineCoopCampaignSyncClient::rejectTransfer(
	FullEngineCoopCampaignSyncClientResult result,
	CoopCampaignSyncFailureReason reason) noexcept
{
	{
		ScratchCallbackGuard guard(inCallback_);
		scratch_.abort();
	}
	failureReason_ = reason;
	state_ = FullEngineCoopCampaignSyncClientState::RejectPending;
	lastResult_ = result;
	terminalResult_ = result;
	const FullEngineCoopCampaignSyncClientResult queued = queueResult(
		CoopCampaignSyncResultStatus::Rejected, reason);
	if (queued == FullEngineCoopCampaignSyncClientResult::Success)
	{
		lastResult_ = result;
		return lastResult_;
	}
	return queued;
}

bool FullEngineCoopCampaignSyncClient::transferMatchesBootstrap(
	const CoopCampaignSyncTransferIdentity& transfer) const noexcept
{
	return IsValidCoopCampaignSyncTransferIdentity(transfer) &&
		transfer.protocolVersion == bootstrap_.protocolVersion &&
		transfer.sessionEpoch == bootstrap_.sessionEpoch &&
		transfer.campaignSeed == bootstrap_.campaignSeed &&
		transfer.campaignIdentitySha256 ==
			bootstrap_.campaignIdentitySha256;
}

bool FullEngineCoopCampaignSyncClient::staleTransfer(
	const CoopCampaignSyncTransferIdentity& transfer) const noexcept
{
	return transfer.protocolVersion == bootstrap_.protocolVersion &&
		transfer.sessionEpoch == bootstrap_.sessionEpoch &&
		transfer.campaignSeed == bootstrap_.campaignSeed &&
		transfer.campaignIdentitySha256 ==
			bootstrap_.campaignIdentitySha256 &&
		transfer.checkpointGeneration <=
			metadata_.transfer.checkpointGeneration;
}

void FullEngineCoopCampaignSyncClient::resetTransfer() noexcept
{
	metadata_ = {};
	hasMetadata_ = false;
	nextExpectedOffset_ = 0;
	previousChunkOffset_ = 0;
	previousChunkChecksum_ = 0;
	previousChunkSize_ = 0;
	pendingOutbound_ = {};
}

void FullEngineCoopCampaignSyncClient::closeWire() noexcept
{
	if (wireCalling_) return;
	wireCalling_ = true;
	wire_.close();
	wireCalling_ = false;
}
}
