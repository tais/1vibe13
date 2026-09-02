#ifndef MULTIPLAYER_FULL_ENGINE_COOP_CAMPAIGN_SYNC_CLIENT_H
#define MULTIPLAYER_FULL_ENGINE_COOP_CAMPAIGN_SYNC_CLIENT_H

#include "CoopCampaignBootstrapProtocol.h"
#include "CoopCampaignSyncProtocol.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace CoopSession
{
enum class FullEngineCoopCampaignSyncClientState : std::uint8_t
{
	Disconnected,
	AwaitingMetadata,
	Receiving,
	CommitPending,
	Ready,
	RejectPending,
	Rejected,
	Failed
};

enum class FullEngineCoopCampaignSyncClientResult : std::uint8_t
{
	Success,
	Backpressured,
	ResyncRequested,
	StaleMessage,
	InvalidState,
	InvalidConfiguration,
	InvalidMessage,
	DescriptorMismatch,
	SequenceMismatch,
	StorageFailure,
	CommitFailed,
	ServerRejected,
	WireFailure,
	ReentrantCall
};

enum class FullEngineCoopCampaignScratchBeginResult : std::uint8_t
{
	Success,
	StorageFailure,
	CapacityReached
};

enum class FullEngineCoopCampaignScratchWriteResult : std::uint8_t
{
	Success,
	StorageFailure
};

enum class FullEngineCoopCampaignScratchCommitResult : std::uint8_t
{
	Committed,
	HashMismatch,
	StorageFailure,
	LoadFailed,
	CompatibilityMismatch
};

// This boundary owns the private client scratch artifact. begin() must replace
// any older uncommitted transfer transactionally. writeExact() must not publish
// partial bytes. Committed promises full SHA-256 verification, atomic scratch
// publication, and successful cold load at the passive main-thread boundary.
class FullEngineCoopCampaignScratch
{
public:
	virtual ~FullEngineCoopCampaignScratch() = default;
	virtual FullEngineCoopCampaignScratchBeginResult begin(
		const CoopCampaignSyncMetadata& metadata) noexcept = 0;
	virtual FullEngineCoopCampaignScratchWriteResult writeExact(
		std::uint64_t offset, const std::uint8_t* bytes,
		std::size_t size) noexcept = 0;
	virtual FullEngineCoopCampaignScratchCommitResult commitAndLoad(
		const CoopCampaignSyncMetadata& metadata) noexcept = 0;
	virtual void abort() noexcept = 0;
};

// send() must copy or enqueue the complete frame before returning. false is
// bounded backpressure and leaves ownership with this coordinator. close() is
// fail-closed and must not synchronously call back into the coordinator.
class FullEngineCoopCampaignSyncClientWire
{
public:
	virtual ~FullEngineCoopCampaignSyncClientWire() = default;
	virtual bool send(const char* messageName, const std::uint8_t* bytes,
		std::size_t size) noexcept = 0;
	virtual void close() noexcept = 0;
};

class FullEngineCoopCampaignSyncClient final
{
public:
	FullEngineCoopCampaignSyncClient(
		FullEngineCoopCampaignScratch& scratch,
		FullEngineCoopCampaignSyncClientWire& wire) noexcept;

	FullEngineCoopCampaignSyncClient(
		const FullEngineCoopCampaignSyncClient&) = delete;
	FullEngineCoopCampaignSyncClient& operator=(
		const FullEngineCoopCampaignSyncClient&) = delete;

	// Starts a fresh admitted transport session. The peer identity is always
	// taken from the admission core, never from a campaign-sync frame.
	FullEngineCoopCampaignSyncClientResult beginSession(
		const CoopCampaignBootstrapDescriptor& bootstrap,
		const PeerIdentity& peerIdentity) noexcept;
	// Retires any active wire before clearing its transfer. A caller must reach
	// this explicit boundary before beginSession may bind a new admitted peer.
	void disconnect() noexcept;

	FullEngineCoopCampaignSyncClientResult receiveMetadata(
		const std::uint8_t* bytes, std::size_t size) noexcept;
	FullEngineCoopCampaignSyncClientResult receiveChunk(
		const std::uint8_t* bytes, std::size_t size) noexcept;
	FullEngineCoopCampaignSyncClientResult receiveComplete(
		const std::uint8_t* bytes, std::size_t size) noexcept;
	FullEngineCoopCampaignSyncClientResult receiveReject(
		const std::uint8_t* bytes, std::size_t size) noexcept;

	FullEngineCoopCampaignSyncClientResult flushOutbound() noexcept;

	FullEngineCoopCampaignSyncClientState state() const noexcept
	{
		return state_;
	}
	FullEngineCoopCampaignSyncClientResult lastResult() const noexcept
	{
		return lastResult_;
	}
	CoopCampaignSyncFailureReason failureReason() const noexcept
	{
		return failureReason_;
	}
	bool hasTransfer() const noexcept { return hasMetadata_; }
	const CoopCampaignSyncMetadata& metadata() const noexcept
	{
		return metadata_;
	}
	std::uint64_t nextExpectedOffset() const noexcept
	{
		return nextExpectedOffset_;
	}
	bool hasPendingOutbound() const noexcept
	{
		return pendingOutbound_.kind != PendingKind::None;
	}

private:
	enum class PendingKind : std::uint8_t
	{
		None,
		Ack,
		Result,
		Resync
	};

	struct PendingOutbound
	{
		PendingKind kind = PendingKind::None;
		std::size_t size = 0;
		std::array<std::uint8_t, CoopCampaignSyncAckWireSize> bytes{};
	};

	FullEngineCoopCampaignSyncClientResult fail(
		FullEngineCoopCampaignSyncClientResult result,
		CoopCampaignSyncFailureReason reason) noexcept;
	FullEngineCoopCampaignSyncClientResult queueAck(
		std::uint64_t cursor, std::uint32_t checksum) noexcept;
	FullEngineCoopCampaignSyncClientResult queueResync(
		CoopCampaignSyncFailureReason reason) noexcept;
	FullEngineCoopCampaignSyncClientResult queueResult(
		CoopCampaignSyncResultStatus status,
		CoopCampaignSyncFailureReason reason) noexcept;
	FullEngineCoopCampaignSyncClientResult rejectTransfer(
		FullEngineCoopCampaignSyncClientResult result,
		CoopCampaignSyncFailureReason reason) noexcept;
	bool transferMatchesBootstrap(
		const CoopCampaignSyncTransferIdentity& transfer) const noexcept;
	bool staleTransfer(
		const CoopCampaignSyncTransferIdentity& transfer) const noexcept;
	void resetTransfer() noexcept;
	void closeWire() noexcept;

	FullEngineCoopCampaignScratch& scratch_;
	FullEngineCoopCampaignSyncClientWire& wire_;
	CoopCampaignBootstrapDescriptor bootstrap_{};
	PeerIdentity peerIdentity_{};
	CoopCampaignSyncMetadata metadata_{};
	PendingOutbound pendingOutbound_{};
	FullEngineCoopCampaignSyncClientState state_ =
		FullEngineCoopCampaignSyncClientState::Disconnected;
	FullEngineCoopCampaignSyncClientResult lastResult_ =
		FullEngineCoopCampaignSyncClientResult::Success;
	// Retains the semantic terminal rejection/failure across a backpressured
	// result-frame retry. A successful wire flush must not erase why this
	// transfer became terminal.
	FullEngineCoopCampaignSyncClientResult terminalResult_ =
		FullEngineCoopCampaignSyncClientResult::Success;
	CoopCampaignSyncFailureReason failureReason_ =
		CoopCampaignSyncFailureReason::None;
	std::uint64_t nextExpectedOffset_ = 0;
	std::uint64_t previousChunkOffset_ = 0;
	std::uint32_t previousChunkChecksum_ = 0;
	std::uint32_t previousChunkSize_ = 0;
	bool configured_ = false;
	bool hasMetadata_ = false;
	bool inCallback_ = false;
	bool wireCalling_ = false;
};
}

#endif
