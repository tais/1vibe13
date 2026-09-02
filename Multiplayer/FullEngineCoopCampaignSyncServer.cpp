#include "FullEngineCoopCampaignSyncServer.h"

#include <algorithm>
#include <utility>

namespace CoopSession
{
namespace
{
bool IdentityLess(const PeerIdentity& left,
	const PeerIdentity& right) noexcept
{
	return std::lexicographical_compare(
		left.begin(), left.end(), right.begin(), right.end());
}

template<typename Digest>
bool ZeroDigest(const Digest& digest) noexcept
{
	for (std::uint8_t byte : digest)
		if (byte != 0) return false;
	return true;
}

class BusyGuard
{
public:
	explicit BusyGuard(bool& busy) noexcept : busy_(busy)
	{
		busy_ = true;
	}
	~BusyGuard() { busy_ = false; }

private:
	bool& busy_;
};
}

const char* FullEngineCoopCampaignSyncOutboundMessageName(
	FullEngineCoopCampaignSyncOutboundKind kind) noexcept
{
	switch (kind)
	{
		case FullEngineCoopCampaignSyncOutboundKind::Metadata:
			return CoopCampaignSyncMetadataMessageName;
		case FullEngineCoopCampaignSyncOutboundKind::Chunk:
			return CoopCampaignSyncChunkMessageName;
		case FullEngineCoopCampaignSyncOutboundKind::Complete:
			return CoopCampaignSyncCompleteMessageName;
		case FullEngineCoopCampaignSyncOutboundKind::Reject:
			return CoopCampaignSyncRejectMessageName;
	}
	return "";
}

void FullEngineCoopCampaignSyncServer::PendingMessage::clear() noexcept
{
	size = 0;
	kind = FullEngineCoopCampaignSyncPendingKind::None;
	chunkOffset = 0;
	chunkEndOffset = 0;
	chunkChecksum = 0;
}

FullEngineCoopCampaignSyncServer::FullEngineCoopCampaignSyncServer(
	FullEngineCoopCampaignCheckpointSource& source,
	FullEngineCoopCampaignSyncWireSink& sink,
	FullEngineCoopCampaignSyncServerConfiguration configuration) noexcept
	: source_(&source), sink_(sink), configuration_(configuration)
{
}

bool FullEngineCoopCampaignSyncServer::configurationValid() const noexcept
{
	return configuration_.maximumTransferId != 0 &&
		configuration_.maximumMessagesPerFlush != 0 &&
		configuration_.maximumMessagesPerFlush <=
			MaximumFullEngineCoopCampaignSyncMessagesPerFlush;
}

bool FullEngineCoopCampaignSyncServer::checkpointMetadataValid(
	const FullEngineCoopCampaignCheckpointMetadata& metadata) const noexcept
{
	return !ZeroDigest(metadata.campaignIdentitySha256) &&
		metadata.checkpointGeneration != 0 && metadata.totalSize != 0 &&
		metadata.totalSize <= MaximumCoopCampaignCheckpointBytes &&
		!ZeroDigest(metadata.checkpointSha256);
}

bool FullEngineCoopCampaignSyncServer::sameCheckpointMetadata(
	const FullEngineCoopCampaignCheckpointMetadata& left,
	const FullEngineCoopCampaignCheckpointMetadata& right) const noexcept
{
	return left.campaignSeed == right.campaignSeed &&
		left.campaignIdentitySha256 == right.campaignIdentitySha256 &&
		left.checkpointGeneration == right.checkpointGeneration &&
		left.totalSize == right.totalSize &&
		left.checkpointSha256 == right.checkpointSha256 &&
		left.worldMinutes == right.worldMinutes;
}

bool FullEngineCoopCampaignSyncServer::canAllocateTransfers(
	std::size_t count) const noexcept
{
	if (count == 0) return true;
	if (nextTransferId_ == 0 ||
		nextTransferId_ > configuration_.maximumTransferId)
		return false;
	return static_cast<std::uint64_t>(count - 1) <=
		configuration_.maximumTransferId - nextTransferId_;
}

std::uint64_t FullEngineCoopCampaignSyncServer::allocateTransferId(
	std::uint64_t& cursor) const noexcept
{
	if (cursor == 0 || cursor > configuration_.maximumTransferId) return 0;
	const std::uint64_t allocated = cursor;
	cursor = allocated == configuration_.maximumTransferId
		? 0 : allocated + 1;
	return allocated;
}

CoopCampaignSyncTransferIdentity
FullEngineCoopCampaignSyncServer::transferIdentity(
	std::uint64_t transferId,
	const FullEngineCoopCampaignCheckpointMetadata& metadata) const noexcept
{
	CoopCampaignSyncTransferIdentity transfer;
	transfer.sessionEpoch = sessionEpoch_;
	transfer.transferId = transferId;
	transfer.campaignSeed = metadata.campaignSeed;
	transfer.campaignIdentitySha256 = metadata.campaignIdentitySha256;
	transfer.checkpointGeneration = metadata.checkpointGeneration;
	transfer.totalSize = metadata.totalSize;
	transfer.checkpointSha256 = metadata.checkpointSha256;
	return transfer;
}

FullEngineCoopCampaignSyncServer::PeerRecord*
FullEngineCoopCampaignSyncServer::findPeer(
	const PeerIdentity& identity) noexcept
{
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].identity == identity) return &peers_[index];
	return nullptr;
}

const FullEngineCoopCampaignSyncServer::PeerRecord*
FullEngineCoopCampaignSyncServer::findPeer(
	const PeerIdentity& identity) const noexcept
{
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].identity == identity) return &peers_[index];
	return nullptr;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::resolvePeer(
	const PeerIdentity& authenticatedPeer,
	const TransportPeer& transport,
	PeerRecord*& peer) noexcept
{
	peer = findPeer(authenticatedPeer);
	if (!peer) return FullEngineCoopCampaignSyncServerResult::InvalidPeer;
	if (!transport || peer->transport != transport)
		return FullEngineCoopCampaignSyncServerResult::StaleTransport;
	return FullEngineCoopCampaignSyncServerResult::Success;
}

void FullEngineCoopCampaignSyncServer::failTerminal(
	FullEngineCoopCampaignSyncServerResult cause) noexcept
{
	terminal_ = true;
	terminalCause_ = cause;
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		PeerRecord& peer = peers_[index];
		peer.phase = FullEngineCoopCampaignSyncPeerPhase::Failed;
		peer.pending.clear();
		peer.inFlightCount = 0;
		peer.rejectionReason = CoopCampaignSyncFailureReason::ProtocolViolation;
	}
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::verifySource() noexcept
{
	FullEngineCoopCampaignCheckpointMetadata observed;
	if (source_ == nullptr || !source_->metadata(observed))
	{
		failTerminal(FullEngineCoopCampaignSyncServerResult::SourceUnavailable);
		return FullEngineCoopCampaignSyncServerResult::SourceUnavailable;
	}
	if (!checkpointMetadataValid(observed) ||
		!sameCheckpointMetadata(observed, checkpoint_))
	{
		failTerminal(
			FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch);
		return FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch;
	}
	return FullEngineCoopCampaignSyncServerResult::Success;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::beginSession(
	std::uint64_t sessionEpoch) noexcept
{
	if (busy_) return FullEngineCoopCampaignSyncServerResult::Busy;
	if (active_) return FullEngineCoopCampaignSyncServerResult::AlreadyActive;
	if (!configurationValid())
		return FullEngineCoopCampaignSyncServerResult::InvalidConfiguration;
	if (sessionEpoch == 0)
		return FullEngineCoopCampaignSyncServerResult::InvalidSessionEpoch;
	BusyGuard guard(busy_);
	FullEngineCoopCampaignCheckpointMetadata metadata;
	if (source_ == nullptr || !source_->metadata(metadata))
		return FullEngineCoopCampaignSyncServerResult::SourceUnavailable;
	if (!checkpointMetadataValid(metadata))
		return FullEngineCoopCampaignSyncServerResult::InvalidCheckpoint;

	checkpoint_ = metadata;
	sessionEpoch_ = sessionEpoch;
	nextTransferId_ = 1;
	peers_ = {};
	peerCount_ = 0;
	active_ = true;
	terminal_ = false;
	terminalCause_ = FullEngineCoopCampaignSyncServerResult::Success;
	return FullEngineCoopCampaignSyncServerResult::Success;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::endSession() noexcept
{
	if (busy_) return FullEngineCoopCampaignSyncServerResult::Busy;
	if (!active_) return FullEngineCoopCampaignSyncServerResult::NotActive;
	BusyGuard guard(busy_);
	peers_ = {};
	peerCount_ = 0;
	checkpoint_ = {};
	sessionEpoch_ = 0;
	nextTransferId_ = 1;
	active_ = false;
	terminal_ = false;
	terminalCause_ = FullEngineCoopCampaignSyncServerResult::Success;
	return FullEngineCoopCampaignSyncServerResult::Success;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::stageMetadata(
	PeerRecord& peer,
	const FullEngineCoopCampaignCheckpointMetadata& metadata) noexcept
{
	CoopCampaignSyncMetadata wireMetadata;
	wireMetadata.transfer = peer.transfer;
	wireMetadata.worldMinutes = metadata.worldMinutes;
	CoopCampaignSyncMetadataBytes encoded{};
	const CoopCampaignSyncCodecResult encodedResult =
		EncodeCoopCampaignSyncMetadata(wireMetadata, encoded);
	if (encodedResult != CoopCampaignSyncCodecResult::Success)
		return FullEngineCoopCampaignSyncServerResult::CodecFailure;
	std::copy(encoded.begin(), encoded.end(), peer.pending.bytes.begin());
	peer.pending.size = encoded.size();
	peer.pending.kind = FullEngineCoopCampaignSyncPendingKind::Metadata;
	peer.pending.chunkOffset = 0;
	peer.pending.chunkEndOffset = 0;
	peer.pending.chunkChecksum = 0;
	return FullEngineCoopCampaignSyncServerResult::Success;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::stageFreshPeer(
	PeerRecord& peer,
	const FullEngineCoopCampaignSyncAuthenticatedPeer& authenticated,
	std::uint64_t transferId,
	const FullEngineCoopCampaignCheckpointMetadata& metadata) noexcept
{
	peer = {};
	peer.identity = authenticated.peerIdentity;
	peer.transport = authenticated.transport;
	peer.phase = FullEngineCoopCampaignSyncPeerPhase::MetadataPending;
	peer.transfer = transferIdentity(transferId, metadata);
	return stageMetadata(peer, metadata);
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::reconcilePeers(
	const FullEngineCoopCampaignSyncAuthenticatedPeer* authenticated,
	std::size_t count) noexcept
{
	if (busy_) return FullEngineCoopCampaignSyncServerResult::Busy;
	if (!active_) return FullEngineCoopCampaignSyncServerResult::NotActive;
	if (terminal_) return FullEngineCoopCampaignSyncServerResult::TerminalFailure;
	if (count > MaximumFullEngineCoopCampaignSyncPeers)
		return FullEngineCoopCampaignSyncServerResult::PeerCapacityReached;
	if (count != 0 && authenticated == nullptr)
		return FullEngineCoopCampaignSyncServerResult::InvalidPeerSet;
	for (std::size_t index = 0; index < count; ++index)
	{
		if (IsZero(authenticated[index].peerIdentity) ||
			!authenticated[index].transport ||
			(index != 0 && !IdentityLess(
				authenticated[index - 1].peerIdentity,
				authenticated[index].peerIdentity)))
			return FullEngineCoopCampaignSyncServerResult::InvalidPeerSet;
		for (std::size_t previous = 0; previous < index; ++previous)
			if (authenticated[previous].transport ==
				authenticated[index].transport)
				return FullEngineCoopCampaignSyncServerResult::InvalidPeerSet;
	}

	BusyGuard guard(busy_);
	const FullEngineCoopCampaignSyncServerResult sourceResult = verifySource();
	if (sourceResult != FullEngineCoopCampaignSyncServerResult::Success)
		return sourceResult;

	std::size_t transfersNeeded = 0;
	for (std::size_t index = 0; index < count; ++index)
	{
		const PeerRecord* existing = findPeer(authenticated[index].peerIdentity);
		if (!existing || existing->transport != authenticated[index].transport)
			++transfersNeeded;
	}
	if (!canAllocateTransfers(transfersNeeded))
	{
		failTerminal(FullEngineCoopCampaignSyncServerResult::TransferExhausted);
		return FullEngineCoopCampaignSyncServerResult::TransferExhausted;
	}

	std::array<PeerRecord, MaximumFullEngineCoopCampaignSyncPeers> staged{};
	std::uint64_t transferCursor = nextTransferId_;
	for (std::size_t index = 0; index < count; ++index)
	{
		const PeerRecord* existing = findPeer(authenticated[index].peerIdentity);
		if (existing && existing->transport == authenticated[index].transport)
		{
			staged[index] = *existing;
			continue;
		}
		const std::uint64_t transferId = allocateTransferId(transferCursor);
		const FullEngineCoopCampaignSyncServerResult stagedResult =
			stageFreshPeer(staged[index], authenticated[index], transferId,
				checkpoint_);
		if (stagedResult != FullEngineCoopCampaignSyncServerResult::Success)
			return stagedResult;
	}

	peers_ = staged;
	peerCount_ = count;
	nextTransferId_ = transferCursor;
	return FullEngineCoopCampaignSyncServerResult::Success;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::supersedeCheckpoint(
	FullEngineCoopCampaignCheckpointSource& source) noexcept
{
	if (busy_) return FullEngineCoopCampaignSyncServerResult::Busy;
	if (!active_) return FullEngineCoopCampaignSyncServerResult::NotActive;
	if (terminal_) return FullEngineCoopCampaignSyncServerResult::TerminalFailure;
	BusyGuard guard(busy_);

	FullEngineCoopCampaignCheckpointMetadata metadata;
	if (!source.metadata(metadata))
	{
		failTerminal(FullEngineCoopCampaignSyncServerResult::SourceUnavailable);
		return FullEngineCoopCampaignSyncServerResult::SourceUnavailable;
	}
	if (!checkpointMetadataValid(metadata))
	{
		failTerminal(
			FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch);
		return FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch;
	}
	if (metadata.campaignSeed != checkpoint_.campaignSeed ||
		metadata.campaignIdentitySha256 != checkpoint_.campaignIdentitySha256)
	{
		failTerminal(
			FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch);
		return FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch;
	}
	if (metadata.checkpointGeneration <= checkpoint_.checkpointGeneration)
	{
		if (sameCheckpointMetadata(metadata, checkpoint_))
			return FullEngineCoopCampaignSyncServerResult::CheckpointNotNewer;
		failTerminal(
			FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch);
		return FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch;
	}
	if (!canAllocateTransfers(peerCount_))
	{
		failTerminal(FullEngineCoopCampaignSyncServerResult::TransferExhausted);
		return FullEngineCoopCampaignSyncServerResult::TransferExhausted;
	}

	std::array<PeerRecord, MaximumFullEngineCoopCampaignSyncPeers> staged{};
	std::uint64_t transferCursor = nextTransferId_;
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		FullEngineCoopCampaignSyncAuthenticatedPeer authenticated;
		authenticated.peerIdentity = peers_[index].identity;
		authenticated.transport = peers_[index].transport;
		const std::uint64_t transferId = allocateTransferId(transferCursor);
		const FullEngineCoopCampaignSyncServerResult stagedResult =
			stageFreshPeer(staged[index], authenticated, transferId, metadata);
		if (stagedResult != FullEngineCoopCampaignSyncServerResult::Success)
			return stagedResult;
	}

	source_ = &source;
	checkpoint_ = metadata;
	peers_ = staged;
	nextTransferId_ = transferCursor;
	return FullEngineCoopCampaignSyncServerResult::Success;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::stageComplete(PeerRecord& peer) noexcept
{
	CoopCampaignSyncComplete completion;
	completion.transfer = peer.transfer;
	CoopCampaignSyncCompleteBytes encoded{};
	if (EncodeCoopCampaignSyncComplete(completion, encoded) !=
		CoopCampaignSyncCodecResult::Success)
		return FullEngineCoopCampaignSyncServerResult::CodecFailure;
	std::copy(encoded.begin(), encoded.end(), peer.pending.bytes.begin());
	peer.pending.size = encoded.size();
	peer.pending.kind = FullEngineCoopCampaignSyncPendingKind::Complete;
	peer.pending.chunkOffset = 0;
	peer.pending.chunkEndOffset = 0;
	peer.pending.chunkChecksum = 0;
	return FullEngineCoopCampaignSyncServerResult::Success;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::stageReject(PeerRecord& peer,
	CoopCampaignSyncFailureReason reason) noexcept
{
	CoopCampaignSyncReject rejection;
	rejection.transfer = peer.transfer;
	rejection.reason = reason;
	CoopCampaignSyncRejectBytes encoded{};
	if (EncodeCoopCampaignSyncReject(rejection, encoded) !=
		CoopCampaignSyncCodecResult::Success)
		return FullEngineCoopCampaignSyncServerResult::CodecFailure;
	std::copy(encoded.begin(), encoded.end(), peer.pending.bytes.begin());
	peer.pending.size = encoded.size();
	peer.pending.kind = FullEngineCoopCampaignSyncPendingKind::Reject;
	peer.pending.chunkOffset = 0;
	peer.pending.chunkEndOffset = 0;
	peer.pending.chunkChecksum = 0;
	return FullEngineCoopCampaignSyncServerResult::Success;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::stageNextChunk(PeerRecord& peer) noexcept
{
	if (peer.nextSendOffset >= peer.transfer.totalSize ||
		peer.inFlightCount >= MaximumCoopCampaignSyncChunkWindow ||
		peer.pending.occupied())
		return FullEngineCoopCampaignSyncServerResult::UnexpectedFrame;

	const std::uint64_t remaining =
		peer.transfer.totalSize - peer.nextSendOffset;
	const std::size_t payloadSize = static_cast<std::size_t>(
		std::min<std::uint64_t>(remaining,
			CoopCampaignSyncCanonicalChunkBytes));

	try
	{
		std::vector<std::uint8_t> payload(payloadSize);
		// This check is intentionally adjacent to the backend read, after
		// scratch allocation. A source object is immutable, but a violated or
		// replaced descriptor still fails closed before bytes are observed.
		const FullEngineCoopCampaignSyncServerResult sourceResult =
			verifySource();
		if (sourceResult !=
			FullEngineCoopCampaignSyncServerResult::Success)
			return sourceResult;
		const FullEngineCoopCampaignCheckpointReadResult readResult =
			source_->readExact(checkpoint_.checkpointSha256,
				peer.nextSendOffset, payload.data(), payload.size());
		if (readResult != FullEngineCoopCampaignCheckpointReadResult::Success)
		{
			const FullEngineCoopCampaignSyncServerResult failure =
				readResult ==
					FullEngineCoopCampaignCheckpointReadResult::DescriptorMismatch
				? FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch
				: FullEngineCoopCampaignSyncServerResult::SourceUnavailable;
			failTerminal(failure);
			return failure;
		}

		CoopCampaignSyncChunk chunk;
		chunk.transfer = peer.transfer;
		chunk.offset = peer.nextSendOffset;
		chunk.payloadChecksum = CoopCampaignSyncPayloadChecksum(
			payload.data(), payload.size());
		chunk.payload = std::move(payload);
		std::vector<std::uint8_t> encoded;
		const CoopCampaignSyncCodecResult encodedResult =
			EncodeCoopCampaignSyncChunk(chunk, encoded);
		if (encodedResult != CoopCampaignSyncCodecResult::Success)
			return encodedResult == CoopCampaignSyncCodecResult::AllocationFailure
				? FullEngineCoopCampaignSyncServerResult::AllocationFailure
				: FullEngineCoopCampaignSyncServerResult::CodecFailure;
		if (encoded.size() > peer.pending.bytes.size())
			return FullEngineCoopCampaignSyncServerResult::CodecFailure;
		std::copy(encoded.begin(), encoded.end(), peer.pending.bytes.begin());
		peer.pending.size = encoded.size();
		peer.pending.kind = FullEngineCoopCampaignSyncPendingKind::Chunk;
		peer.pending.chunkOffset = peer.nextSendOffset;
		peer.pending.chunkEndOffset =
			peer.nextSendOffset + chunk.payload.size();
		peer.pending.chunkChecksum = chunk.payloadChecksum;
		return FullEngineCoopCampaignSyncServerResult::Success;
	}
	catch (...)
	{
		return FullEngineCoopCampaignSyncServerResult::AllocationFailure;
	}
}

const char* FullEngineCoopCampaignSyncServer::pendingMessageName(
	FullEngineCoopCampaignSyncPendingKind kind) const noexcept
{
	return FullEngineCoopCampaignSyncOutboundMessageName(
		pendingOutboundKind(kind));
}

FullEngineCoopCampaignSyncOutboundKind
FullEngineCoopCampaignSyncServer::pendingOutboundKind(
	FullEngineCoopCampaignSyncPendingKind kind) const noexcept
{
	switch (kind)
	{
		case FullEngineCoopCampaignSyncPendingKind::Metadata:
			return FullEngineCoopCampaignSyncOutboundKind::Metadata;
		case FullEngineCoopCampaignSyncPendingKind::Chunk:
			return FullEngineCoopCampaignSyncOutboundKind::Chunk;
		case FullEngineCoopCampaignSyncPendingKind::Complete:
			return FullEngineCoopCampaignSyncOutboundKind::Complete;
		case FullEngineCoopCampaignSyncPendingKind::Reject:
			return FullEngineCoopCampaignSyncOutboundKind::Reject;
		case FullEngineCoopCampaignSyncPendingKind::None:
			break;
	}
	return FullEngineCoopCampaignSyncOutboundKind::Metadata;
}

void FullEngineCoopCampaignSyncServer::commitPendingSend(
	PeerRecord& peer,
	FullEngineCoopCampaignSyncFlushResult& result) noexcept
{
	switch (peer.pending.kind)
	{
		case FullEngineCoopCampaignSyncPendingKind::Metadata:
			peer.phase =
				FullEngineCoopCampaignSyncPeerPhase::AwaitingInitialAck;
			break;
		case FullEngineCoopCampaignSyncPendingKind::Chunk:
		{
			SentChunk& sent = peer.inFlight[peer.inFlightCount++];
			sent.offset = peer.pending.chunkOffset;
			sent.endOffset = peer.pending.chunkEndOffset;
			sent.checksum = peer.pending.chunkChecksum;
			peer.highestSentOffset = sent.endOffset;
			peer.nextSendOffset = sent.endOffset;
			++result.chunksSent;
			break;
		}
		case FullEngineCoopCampaignSyncPendingKind::Complete:
			peer.phase = FullEngineCoopCampaignSyncPeerPhase::AwaitingResult;
			break;
		case FullEngineCoopCampaignSyncPendingKind::Reject:
			peer.phase = FullEngineCoopCampaignSyncPeerPhase::Rejected;
			break;
		case FullEngineCoopCampaignSyncPendingKind::None:
			break;
	}
	peer.pending.clear();
	++result.messagesSent;
}

FullEngineCoopCampaignSyncFlushResult
FullEngineCoopCampaignSyncServer::flushOutbound() noexcept
{
	FullEngineCoopCampaignSyncFlushResult result;
	if (busy_)
	{
		result.result = FullEngineCoopCampaignSyncServerResult::Busy;
		return result;
	}
	if (!active_)
	{
		result.result = FullEngineCoopCampaignSyncServerResult::NotActive;
		return result;
	}
	if (terminal_)
	{
		result.result = FullEngineCoopCampaignSyncServerResult::TerminalFailure;
		return result;
	}
	BusyGuard guard(busy_);
	const FullEngineCoopCampaignSyncServerResult sourceResult = verifySource();
	if (sourceResult != FullEngineCoopCampaignSyncServerResult::Success)
	{
		result.result = sourceResult;
		return result;
	}

	for (std::size_t peerIndex = 0; peerIndex < peerCount_; ++peerIndex)
	{
		PeerRecord& peer = peers_[peerIndex];
		while (result.messagesSent < configuration_.maximumMessagesPerFlush)
		{
			if (!peer.pending.occupied())
			{
				FullEngineCoopCampaignSyncServerResult stageResult =
					FullEngineCoopCampaignSyncServerResult::Success;
				switch (peer.phase)
				{
					case FullEngineCoopCampaignSyncPeerPhase::MetadataPending:
						stageResult = stageMetadata(peer, checkpoint_);
						break;
					case FullEngineCoopCampaignSyncPeerPhase::Streaming:
						if (peer.inFlightCount >=
								MaximumCoopCampaignSyncChunkWindow ||
							peer.nextSendOffset >= peer.transfer.totalSize)
							break;
						stageResult = stageNextChunk(peer);
						break;
					case FullEngineCoopCampaignSyncPeerPhase::CompletePending:
						stageResult = stageComplete(peer);
						break;
					case FullEngineCoopCampaignSyncPeerPhase::RejectPending:
						stageResult = stageReject(
							peer, peer.rejectionReason);
						break;
					default:
						break;
				}
				if (stageResult !=
					FullEngineCoopCampaignSyncServerResult::Success)
				{
					result.result = stageResult;
					result.peerIdentity = peer.identity;
					return result;
				}
				if (!peer.pending.occupied()) break;
			}

			const FullEngineCoopCampaignSyncOutboundKind outboundKind =
				pendingOutboundKind(peer.pending.kind);
			if (!sink_.send(peer.identity, peer.transport, outboundKind,
					pendingMessageName(peer.pending.kind),
					peer.pending.bytes.data(), peer.pending.size))
			{
				result.result =
					FullEngineCoopCampaignSyncServerResult::TransportBackpressured;
				result.peerIdentity = peer.identity;
				result.backpressured = true;
				return result;
			}
			commitPendingSend(peer, result);
		}
		if (result.messagesSent >= configuration_.maximumMessagesPerFlush)
			return result;
	}
	return result;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::validateEchoedTransfer(
	const PeerRecord& peer,
	const CoopCampaignSyncTransferIdentity& transfer) const noexcept
{
	return SameCoopCampaignSyncTransfer(peer.transfer, transfer)
		? FullEngineCoopCampaignSyncServerResult::Success
		: FullEngineCoopCampaignSyncServerResult::StaleTransfer;
}

const FullEngineCoopCampaignSyncServer::SentChunk*
FullEngineCoopCampaignSyncServer::findInFlightEnd(
	const PeerRecord& peer, std::uint64_t cursor) const noexcept
{
	for (std::size_t index = 0; index < peer.inFlightCount; ++index)
		if (peer.inFlight[index].endOffset == cursor)
			return &peer.inFlight[index];
	return nullptr;
}

void FullEngineCoopCampaignSyncServer::acknowledgeThrough(
	PeerRecord& peer,
	std::uint64_t cursor,
	std::uint32_t checksum) noexcept
{
	std::size_t removed = 0;
	while (removed < peer.inFlightCount &&
		peer.inFlight[removed].endOffset <= cursor)
		++removed;
	for (std::size_t index = removed; index < peer.inFlightCount; ++index)
		peer.inFlight[index - removed] = peer.inFlight[index];
	for (std::size_t index = peer.inFlightCount - removed;
		index < peer.inFlightCount; ++index)
		peer.inFlight[index] = {};
	peer.inFlightCount -= removed;
	peer.acknowledgedOffset = cursor;
	peer.precedingAcknowledgedChecksum = checksum;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::handleAckInternal(
	const PeerIdentity& authenticatedPeer,
	const TransportPeer& transport,
	const CoopCampaignSyncAck& acknowledgement) noexcept
{
	PeerRecord* peer = nullptr;
	const FullEngineCoopCampaignSyncServerResult resolved =
		resolvePeer(authenticatedPeer, transport, peer);
	if (resolved != FullEngineCoopCampaignSyncServerResult::Success)
		return resolved;
	if (acknowledgement.peerIdentity != authenticatedPeer)
		return FullEngineCoopCampaignSyncServerResult::ClaimedIdentityMismatch;
	const FullEngineCoopCampaignSyncServerResult transferResult =
		validateEchoedTransfer(*peer, acknowledgement.transfer);
	if (transferResult != FullEngineCoopCampaignSyncServerResult::Success)
		return transferResult;

	if (peer->phase ==
		FullEngineCoopCampaignSyncPeerPhase::AwaitingInitialAck)
	{
		if (acknowledgement.nextExpectedOffset != 0 ||
			acknowledgement.precedingChunkChecksum != 0)
			return FullEngineCoopCampaignSyncServerResult::SequenceMismatch;
		peer->phase = FullEngineCoopCampaignSyncPeerPhase::Streaming;
		return FullEngineCoopCampaignSyncServerResult::Success;
	}

	if (acknowledgement.nextExpectedOffset == peer->acknowledgedOffset)
	{
		if (acknowledgement.precedingChunkChecksum !=
			peer->precedingAcknowledgedChecksum)
			return FullEngineCoopCampaignSyncServerResult::IntegrityMismatch;
		switch (peer->phase)
		{
			case FullEngineCoopCampaignSyncPeerPhase::Streaming:
			case FullEngineCoopCampaignSyncPeerPhase::CompletePending:
			case FullEngineCoopCampaignSyncPeerPhase::AwaitingResult:
			case FullEngineCoopCampaignSyncPeerPhase::Ready:
				return FullEngineCoopCampaignSyncServerResult::Success;
			default:
				return FullEngineCoopCampaignSyncServerResult::UnexpectedFrame;
		}
	}
	if (peer->phase != FullEngineCoopCampaignSyncPeerPhase::Streaming)
		return FullEngineCoopCampaignSyncServerResult::UnexpectedFrame;
	if (acknowledgement.nextExpectedOffset < peer->acknowledgedOffset ||
		acknowledgement.nextExpectedOffset > peer->highestSentOffset)
		return FullEngineCoopCampaignSyncServerResult::SequenceMismatch;
	const SentChunk* acknowledged = findInFlightEnd(
		*peer, acknowledgement.nextExpectedOffset);
	if (!acknowledged)
		return FullEngineCoopCampaignSyncServerResult::SequenceMismatch;
	if (acknowledged->checksum !=
		acknowledgement.precedingChunkChecksum)
		return FullEngineCoopCampaignSyncServerResult::IntegrityMismatch;

	if (acknowledgement.nextExpectedOffset == peer->transfer.totalSize)
	{
		const FullEngineCoopCampaignSyncServerResult staged =
			stageComplete(*peer);
		if (staged != FullEngineCoopCampaignSyncServerResult::Success)
		{
			failTerminal(staged);
			return staged;
		}
	}
	acknowledgeThrough(*peer, acknowledgement.nextExpectedOffset,
		acknowledgement.precedingChunkChecksum);
	if (peer->acknowledgedOffset == peer->transfer.totalSize)
		peer->phase = FullEngineCoopCampaignSyncPeerPhase::CompletePending;
	return FullEngineCoopCampaignSyncServerResult::Success;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::handleResultInternal(
	const PeerIdentity& authenticatedPeer,
	const TransportPeer& transport,
	const CoopCampaignSyncResult& result) noexcept
{
	PeerRecord* peer = nullptr;
	const FullEngineCoopCampaignSyncServerResult resolved =
		resolvePeer(authenticatedPeer, transport, peer);
	if (resolved != FullEngineCoopCampaignSyncServerResult::Success)
		return resolved;
	if (result.peerIdentity != authenticatedPeer)
		return FullEngineCoopCampaignSyncServerResult::ClaimedIdentityMismatch;
	const FullEngineCoopCampaignSyncServerResult transferResult =
		validateEchoedTransfer(*peer, result.transfer);
	if (transferResult != FullEngineCoopCampaignSyncServerResult::Success)
		return transferResult;

	if (result.status == CoopCampaignSyncResultStatus::Committed)
	{
		if (peer->phase == FullEngineCoopCampaignSyncPeerPhase::Ready)
			return FullEngineCoopCampaignSyncServerResult::Success;
		if (peer->phase !=
			FullEngineCoopCampaignSyncPeerPhase::AwaitingResult)
			return FullEngineCoopCampaignSyncServerResult::UnexpectedFrame;
		peer->phase = FullEngineCoopCampaignSyncPeerPhase::Ready;
		return FullEngineCoopCampaignSyncServerResult::Success;
	}

	if (peer->phase == FullEngineCoopCampaignSyncPeerPhase::RejectPending ||
		peer->phase == FullEngineCoopCampaignSyncPeerPhase::Rejected)
		return peer->rejectionReason == result.reason
			? FullEngineCoopCampaignSyncServerResult::ClientRejected
			: FullEngineCoopCampaignSyncServerResult::UnexpectedFrame;
	if (peer->phase !=
			FullEngineCoopCampaignSyncPeerPhase::AwaitingInitialAck &&
		peer->phase != FullEngineCoopCampaignSyncPeerPhase::Streaming &&
		peer->phase != FullEngineCoopCampaignSyncPeerPhase::CompletePending &&
		peer->phase != FullEngineCoopCampaignSyncPeerPhase::AwaitingResult)
		return FullEngineCoopCampaignSyncServerResult::UnexpectedFrame;
	const FullEngineCoopCampaignSyncServerResult staged =
		stageReject(*peer, result.reason);
	if (staged != FullEngineCoopCampaignSyncServerResult::Success)
	{
		failTerminal(staged);
		return staged;
	}
	peer->phase = FullEngineCoopCampaignSyncPeerPhase::RejectPending;
	peer->rejectionReason = result.reason;
	peer->inFlightCount = 0;
	return FullEngineCoopCampaignSyncServerResult::ClientRejected;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::handleResyncInternal(
	const PeerIdentity& authenticatedPeer,
	const TransportPeer& transport,
	const CoopCampaignSyncResync& resync) noexcept
{
	PeerRecord* peer = nullptr;
	const FullEngineCoopCampaignSyncServerResult resolved =
		resolvePeer(authenticatedPeer, transport, peer);
	if (resolved != FullEngineCoopCampaignSyncServerResult::Success)
		return resolved;
	if (resync.peerIdentity != authenticatedPeer)
		return FullEngineCoopCampaignSyncServerResult::ClaimedIdentityMismatch;
	const FullEngineCoopCampaignSyncServerResult transferResult =
		validateEchoedTransfer(*peer, resync.transfer);
	if (transferResult != FullEngineCoopCampaignSyncServerResult::Success)
		return transferResult;
	if (resync.reason != CoopCampaignSyncFailureReason::SequenceMismatch &&
		resync.reason !=
			CoopCampaignSyncFailureReason::ChunkChecksumMismatch)
		return FullEngineCoopCampaignSyncServerResult::UnexpectedFrame;
	if (peer->phase != FullEngineCoopCampaignSyncPeerPhase::Streaming &&
		peer->phase != FullEngineCoopCampaignSyncPeerPhase::CompletePending &&
		peer->phase != FullEngineCoopCampaignSyncPeerPhase::AwaitingResult)
		return FullEngineCoopCampaignSyncServerResult::UnexpectedFrame;
	if (resync.expectedOffset < peer->acknowledgedOffset ||
		resync.expectedOffset > peer->highestSentOffset)
		return FullEngineCoopCampaignSyncServerResult::SequenceMismatch;

	if (resync.expectedOffset == peer->acknowledgedOffset)
	{
		if (resync.precedingChunkChecksum !=
			peer->precedingAcknowledgedChecksum)
			return FullEngineCoopCampaignSyncServerResult::IntegrityMismatch;
	}
	else
	{
		const SentChunk* accepted = findInFlightEnd(
			*peer, resync.expectedOffset);
		if (!accepted)
			return FullEngineCoopCampaignSyncServerResult::SequenceMismatch;
		if (accepted->checksum != resync.precedingChunkChecksum)
			return FullEngineCoopCampaignSyncServerResult::IntegrityMismatch;
	}

	if (resync.expectedOffset == peer->transfer.totalSize)
	{
		const FullEngineCoopCampaignSyncServerResult staged =
			stageComplete(*peer);
		if (staged != FullEngineCoopCampaignSyncServerResult::Success)
		{
			failTerminal(staged);
			return staged;
		}
	}
	else
	{
		peer->pending.clear();
	}
	peer->acknowledgedOffset = resync.expectedOffset;
	peer->precedingAcknowledgedChecksum =
		resync.precedingChunkChecksum;
	peer->highestSentOffset = resync.expectedOffset;
	peer->nextSendOffset = resync.expectedOffset;
	peer->inFlightCount = 0;
	peer->phase = resync.expectedOffset == peer->transfer.totalSize
		? FullEngineCoopCampaignSyncPeerPhase::CompletePending
		: FullEngineCoopCampaignSyncPeerPhase::Streaming;
	return FullEngineCoopCampaignSyncServerResult::Success;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::handleInbound(
	const PeerIdentity& authenticatedPeer,
	const TransportPeer& transport,
	FullEngineCoopCampaignSyncInboundKind kind,
	const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	if (busy_) return FullEngineCoopCampaignSyncServerResult::Busy;
	if (!active_) return FullEngineCoopCampaignSyncServerResult::NotActive;
	if (terminal_) return FullEngineCoopCampaignSyncServerResult::TerminalFailure;
	BusyGuard guard(busy_);
	const FullEngineCoopCampaignSyncServerResult sourceResult = verifySource();
	if (sourceResult != FullEngineCoopCampaignSyncServerResult::Success)
		return sourceResult;

	switch (kind)
	{
		case FullEngineCoopCampaignSyncInboundKind::Ack:
		{
			CoopCampaignSyncAck acknowledgement;
			if (DecodeCoopCampaignSyncAck(bytes, size, acknowledgement) !=
				CoopCampaignSyncCodecResult::Success)
				return FullEngineCoopCampaignSyncServerResult::MalformedFrame;
			return handleAckInternal(
				authenticatedPeer, transport, acknowledgement);
		}
		case FullEngineCoopCampaignSyncInboundKind::Result:
		{
			CoopCampaignSyncResult result;
			if (DecodeCoopCampaignSyncResult(bytes, size, result) !=
				CoopCampaignSyncCodecResult::Success)
				return FullEngineCoopCampaignSyncServerResult::MalformedFrame;
			return handleResultInternal(authenticatedPeer, transport, result);
		}
		case FullEngineCoopCampaignSyncInboundKind::Resync:
		{
			CoopCampaignSyncResync resync;
			if (DecodeCoopCampaignSyncResync(bytes, size, resync) !=
				CoopCampaignSyncCodecResult::Success)
				return FullEngineCoopCampaignSyncServerResult::MalformedFrame;
			return handleResyncInternal(authenticatedPeer, transport, resync);
		}
	}
	return FullEngineCoopCampaignSyncServerResult::MalformedFrame;
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::handleAck(
	const PeerIdentity& authenticatedPeer,
	const TransportPeer& transport,
	const CoopCampaignSyncAck& acknowledgement) noexcept
{
	if (busy_) return FullEngineCoopCampaignSyncServerResult::Busy;
	if (!active_) return FullEngineCoopCampaignSyncServerResult::NotActive;
	if (terminal_) return FullEngineCoopCampaignSyncServerResult::TerminalFailure;
	CoopCampaignSyncAckBytes canonical{};
	if (EncodeCoopCampaignSyncAck(acknowledgement, canonical) !=
		CoopCampaignSyncCodecResult::Success)
		return FullEngineCoopCampaignSyncServerResult::MalformedFrame;
	BusyGuard guard(busy_);
	const FullEngineCoopCampaignSyncServerResult sourceResult = verifySource();
	if (sourceResult != FullEngineCoopCampaignSyncServerResult::Success)
		return sourceResult;
	return handleAckInternal(authenticatedPeer, transport, acknowledgement);
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::handleResult(
	const PeerIdentity& authenticatedPeer,
	const TransportPeer& transport,
	const CoopCampaignSyncResult& result) noexcept
{
	if (busy_) return FullEngineCoopCampaignSyncServerResult::Busy;
	if (!active_) return FullEngineCoopCampaignSyncServerResult::NotActive;
	if (terminal_) return FullEngineCoopCampaignSyncServerResult::TerminalFailure;
	CoopCampaignSyncResultBytes canonical{};
	if (EncodeCoopCampaignSyncResult(result, canonical) !=
		CoopCampaignSyncCodecResult::Success)
		return FullEngineCoopCampaignSyncServerResult::MalformedFrame;
	BusyGuard guard(busy_);
	const FullEngineCoopCampaignSyncServerResult sourceResult = verifySource();
	if (sourceResult != FullEngineCoopCampaignSyncServerResult::Success)
		return sourceResult;
	return handleResultInternal(authenticatedPeer, transport, result);
}

FullEngineCoopCampaignSyncServerResult
FullEngineCoopCampaignSyncServer::handleResync(
	const PeerIdentity& authenticatedPeer,
	const TransportPeer& transport,
	const CoopCampaignSyncResync& resync) noexcept
{
	if (busy_) return FullEngineCoopCampaignSyncServerResult::Busy;
	if (!active_) return FullEngineCoopCampaignSyncServerResult::NotActive;
	if (terminal_) return FullEngineCoopCampaignSyncServerResult::TerminalFailure;
	CoopCampaignSyncResyncBytes canonical{};
	if (EncodeCoopCampaignSyncResync(resync, canonical) !=
		CoopCampaignSyncCodecResult::Success)
		return FullEngineCoopCampaignSyncServerResult::MalformedFrame;
	BusyGuard guard(busy_);
	const FullEngineCoopCampaignSyncServerResult sourceResult = verifySource();
	if (sourceResult != FullEngineCoopCampaignSyncServerResult::Success)
		return sourceResult;
	return handleResyncInternal(authenticatedPeer, transport, resync);
}

std::size_t FullEngineCoopCampaignSyncServer::readyPeers(
	std::array<PeerIdentity,
		MaximumFullEngineCoopCampaignSyncPeers>& output) const noexcept
{
	output = {};
	if (!active_ || terminal_) return 0;
	std::size_t count = 0;
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].phase == FullEngineCoopCampaignSyncPeerPhase::Ready)
			output[count++] = peers_[index].identity;
	return count;
}

bool FullEngineCoopCampaignSyncServer::peerDiagnostics(
	const PeerIdentity& identity,
	FullEngineCoopCampaignSyncPeerDiagnostics& diagnostics) const noexcept
{
	const PeerRecord* peer = findPeer(identity);
	if (!peer) return false;
	FullEngineCoopCampaignSyncPeerDiagnostics output;
	output.peerIdentity = peer->identity;
	output.transport = peer->transport;
	output.phase = peer->phase;
	output.transferId = peer->transfer.transferId;
	output.acknowledgedOffset = peer->acknowledgedOffset;
	output.highestSentOffset = peer->highestSentOffset;
	output.nextSendOffset = peer->nextSendOffset;
	output.precedingAcknowledgedChecksum =
		peer->precedingAcknowledgedChecksum;
	output.inFlightChunks = peer->inFlightCount;
	output.pendingKind = peer->pending.kind;
	output.pendingBytes = peer->pending.size;
	output.rejectionReason = peer->rejectionReason;
	output.campaignReady = active_ && !terminal_ &&
		peer->phase == FullEngineCoopCampaignSyncPeerPhase::Ready;
	diagnostics = output;
	return true;
}

FullEngineCoopCampaignSyncServerDiagnostics
FullEngineCoopCampaignSyncServer::diagnostics() const noexcept
{
	FullEngineCoopCampaignSyncServerDiagnostics output;
	output.active = active_;
	output.terminal = terminal_;
	output.sessionEpoch = sessionEpoch_;
	output.checkpointGeneration = checkpoint_.checkpointGeneration;
	output.checkpointSize = checkpoint_.totalSize;
	output.nextTransferId = nextTransferId_;
	output.connectedPeers = peerCount_;
	output.terminalCause = terminalCause_;
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		const PeerRecord& peer = peers_[index];
		if (peer.phase == FullEngineCoopCampaignSyncPeerPhase::Ready &&
			!terminal_)
			++output.readyPeers;
		if (peer.pending.occupied()) ++output.pendingMessages;
		output.inFlightChunks += peer.inFlightCount;
	}
	return output;
}
}
