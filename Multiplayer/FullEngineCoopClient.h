#ifndef MULTIPLAYER_FULL_ENGINE_COOP_CLIENT_H
#define MULTIPLAYER_FULL_ENGINE_COOP_CLIENT_H

#include "CoopSessionProtocol.h"
#include "CoopCampaignStatus.h"
#include "CoopCampaignGroups.h"
#include "CoopCampaignTime.h"
#include "CoopCampaignAction.h"
#include "CoopCampaignHire.h"
#include "CoopInventoryProtocol.h"
#include "CoopTacticalIntent.h"
#include "CoopTacticalProtocol.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace CoopSession
{
inline constexpr std::size_t MaximumFullEngineCoopClientReceiptHistory = 64;

// This core is driven only from the application's main thread. It owns no
// socket, UI, JA2 object, or legacy multiplayer callback and never logs bearer
// credentials. A transport adapter merely forwards connection events and the
// exact co-op wire messages to these methods.
enum class FullEngineCoopClientState : std::uint8_t
{
	Disconnected,
	Connecting,
	Hello,
	Admission,
	AwaitingBaseline,
	Active,
	Retiring,
	Retired,
	ResyncRequired,
	Failed
};

enum class FullEngineCoopClientResult : std::uint8_t
{
	Success,
	InvalidConfiguration,
	InvalidState,
	InvalidMessage,
	CompatibilityMismatch,
	AdmissionRejected,
	WireFailure,
	ResyncRequired,
	IntentOutstanding,
	ActorNotAssigned,
	InvalidIntent,
	SequenceExhausted,
	AllocationFailure,
	CredentialStorageFailure,
	CredentialRetirementPending,
	SelfRetirementRejected,
	CredentialRetired
};

struct FullEngineCoopClientConfiguration
{
	std::uint16_t protocolVersion = CurrentProtocolVersion;
	RuntimeCompatibilityFingerprint runtimeFingerprint;
	ContentManifestSha256 contentManifestSha256{};
	// Zero preserves the reusable core's unpinned test/embedding mode. The
	// production passive runtime pins the preflight descriptor epoch so a
	// restarted server cannot trigger a fresh-seat request on the live socket.
	std::uint64_t expectedSessionEpoch = 0;
	std::size_t maximumInboundWireBytes =
		MaximumCoopTacticalWireSize;
	std::size_t maximumAssignedActors =
		MaximumCoopTacticalAssignedActors;
	bool durableReconnectCredentialRequired = false;
};

// send() must copy or enqueue all bytes before returning and return false on
// its own bounded-queue backpressure. Neither method may invoke this client
// synchronously; the client also rejects such re-entry defensively.
class FullEngineCoopClientWire
{
public:
	virtual ~FullEngineCoopClientWire() = default;
	virtual bool send(const char* messageName,
		const std::uint8_t* bytes, std::size_t size) noexcept = 0;
	virtual void close() noexcept = 0;
};

enum class FullEngineCoopReplicaApplyResult : std::uint8_t
{
	Committed,
	Rejected
};

// The sink is a passive replica boundary. Returning Committed promises that
// the complete baseline/delta is visible at a committed main-thread boundary;
// only then may the client emit the matching ACK.
class FullEngineCoopPassiveReplicaSink
{
public:
	virtual ~FullEngineCoopPassiveReplicaSink() = default;
	virtual FullEngineCoopReplicaApplyResult applyBaseline(
		const CoopTacticalBaseline& baseline) noexcept = 0;
	virtual FullEngineCoopReplicaApplyResult applyDelta(
		const CoopTacticalDelta& delta) noexcept = 0;
};

// A production client must durably publish every accepted bearer credential
// before it sends the admission ACK which makes that seat authoritative. The
// callback is synchronous, main-thread-only, and may not re-enter the client.
class FullEngineCoopReconnectCredentialStore
{
public:
	virtual ~FullEngineCoopReconnectCredentialStore() = default;
	virtual bool persistReconnectCredential(
		const AdmissionAck& credential) noexcept = 0;
	// Convert the live bearer into a durable terminal marker. Success must not
	// leave the state looking Missing, because Missing means first admission.
	virtual bool retireReconnectCredential(
		const AdmissionAck& credential) noexcept = 0;
};

class FullEngineCoopClient final
{
public:
	FullEngineCoopClient(FullEngineCoopClientWire& wire,
		FullEngineCoopPassiveReplicaSink& replica,
		FullEngineCoopReconnectCredentialStore* credentialStore = nullptr)
		noexcept;

	FullEngineCoopClient(const FullEngineCoopClient&) = delete;
	FullEngineCoopClient& operator=(const FullEngineCoopClient&) = delete;

	FullEngineCoopClientResult configure(
		const FullEngineCoopClientConfiguration& configuration) noexcept;
	// Installs an exact credential read from private durable storage. This is
	// accepted only after configure() and before beginConnection(). Tactical
	// cursor/receipt state is deliberately restored by the fresh baseline.
	FullEngineCoopClientResult restoreReconnectCredential(
		const AdmissionAck& credential) noexcept;
	FullEngineCoopClientResult beginConnection() noexcept;
	FullEngineCoopClientResult transportConnected() noexcept;
	void transportDisconnected() noexcept;
	void disconnect() noexcept;

	FullEngineCoopClientResult receiveServerHello(
		const std::uint8_t* bytes, std::size_t size) noexcept;
	FullEngineCoopClientResult receiveAdmissionResponse(
		const std::uint8_t* bytes, std::size_t size) noexcept;
	FullEngineCoopClientResult receiveBaseline(
		const std::uint8_t* bytes, std::size_t size) noexcept;
	FullEngineCoopClientResult receiveDelta(
		const std::uint8_t* bytes, std::size_t size) noexcept;
	FullEngineCoopClientResult receiveOwnerInventory(
		const std::uint8_t* bytes, std::size_t size) noexcept;
	FullEngineCoopClientResult receiveCampaignStatus(
		const std::uint8_t* bytes, std::size_t size) noexcept;
	const CoopCampaignStatus* campaignStatus() const noexcept;
	FullEngineCoopClientResult receiveCampaignGroups(const std::uint8_t* bytes, std::size_t size) noexcept;
	const CoopCampaignGroups* campaignGroups() const noexcept;
	FullEngineCoopClientResult receiveCampaignEconomy(const std::uint8_t* bytes, std::size_t size) noexcept;
	FullEngineCoopClientResult receiveCampaignAimQuotes(const std::uint8_t* bytes, std::size_t size) noexcept;
	const CoopCampaignEconomy* campaignEconomy() const noexcept;
	// Old offers are hidden while the newer economy replacement is waiting for
	// its matching quotes in the ordered inbound stream.
	const CoopCampaignAimQuotes* campaignAimQuotes() const noexcept;
	FullEngineCoopClientResult requestCampaignHire(std::uint16_t profile, std::uint8_t days, bool buyGear) noexcept;
	FullEngineCoopClientResult receiveCampaignHireResult(const std::uint8_t* bytes, std::size_t size) noexcept;
	bool campaignHirePending() const noexcept { return pendingCampaignHire_.requestId != 0; }
	const CoopCampaignHireResult* lastCampaignHireResult() const noexcept
	{
		return campaignStatus() && !campaignHirePending() && lastCampaignHireResult_.request.requestId ? &lastCampaignHireResult_ : nullptr;
	}
	FullEngineCoopClientResult requestCampaignTime(CoopCampaignTimeAction action) noexcept;
	FullEngineCoopClientResult receiveCampaignTimeResult(const std::uint8_t* bytes, std::size_t size) noexcept;
	bool campaignTimePending() const noexcept { return pendingCampaignTime_.requestId != 0; }
	const CoopCampaignTimeResult* campaignTimeResult() const noexcept
	{
		return campaignStatus() && lastCampaignTimeResult_.request.requestId ? &lastCampaignTimeResult_ : nullptr;
	}
	// Selection-only request: this core supplies current session/revisions and
	// a fresh sequence. It never retries unknown actions after reconnect.
	FullEngineCoopClientResult requestCampaignAction(const CoopCampaignActionRequest& selection) noexcept;
	FullEngineCoopClientResult receiveCampaignActionResult(const std::uint8_t* bytes, std::size_t size) noexcept;
	bool campaignActionPending() const noexcept { return pendingCampaignAction_.requestId != 0; }
	const CoopCampaignActionResult* lastCampaignActionResult() const noexcept
	{
		return campaignStatus() && lastCampaignActionResult_.request.requestId ? &lastCampaignActionResult_ : nullptr;
	}
	FullEngineCoopClientResult receiveIntentReceipt(
		const std::uint8_t* bytes, std::size_t size) noexcept;
	FullEngineCoopClientResult receiveSelfRetirementResult(
		const std::uint8_t* bytes, std::size_t size) noexcept;

	// TacticalIntentPayload is a closed variant containing exactly Move, Face,
	// Stance, Stop, EndTurn, AimedFirearmAttack, Reload, DoorOpenClose, and
	// PassInterrupt.
	// Every authority-
	// controlled identity/context field is derived here; callers can provide
	// only an assigned actor and the typed intent payload.
	FullEngineCoopClientResult sendIntent(
		TacticalEntityId actor,
		const TacticalIntentPayload& payload) noexcept;
	// Voluntarily and permanently leaves this admission seat. The request has no
	// selectable peer field. On exact completion the durable bearer is erased
	// before the core enters its clean terminal Retired state.
	FullEngineCoopClientResult requestSelfRetirement() noexcept;

	FullEngineCoopClientState state() const noexcept { return state_; }
	FullEngineCoopClientResult lastResult() const noexcept
	{
		return lastResult_;
	}
	AdmissionRejectReason lastAdmissionRejectReason() const noexcept
	{
		return lastAdmissionRejectReason_;
	}
	bool configured() const noexcept { return configured_; }
	bool hasReconnectCredential() const noexcept
	{
		return !IsZero(peerIdentity_) && !IsZero(reconnectToken_);
	}
	bool selfRetirementPending() const noexcept
	{
		return state_ == FullEngineCoopClientState::Retiring ||
			selfRetirementAwaitingOutcome_;
	}
	std::uint64_t selfRetirementRequestId() const noexcept
	{
		return selfRetirementRequestId_;
	}
	const PeerIdentity& peerIdentity() const noexcept { return peerIdentity_; }
	std::uint64_t sessionEpoch() const noexcept { return sessionEpoch_; }
	bool hasAcceptedState() const noexcept { return hasAcceptedState_; }
	// True while the last committed tactical view remains readable but intent
	// submission is frozen until a replacement baseline commits.
	bool resyncPending() const noexcept;
	const CoopTacticalStateIdentity& acceptedState() const noexcept
	{
		return acceptedState_;
	}
	std::uint64_t nextExpectedCommandId() const noexcept
	{
		return nextExpectedCommandId_;
	}
	std::uint64_t outstandingCommandId() const noexcept
	{
		return outstandingCommandId_;
	}
	std::size_t assignedActorCount() const noexcept
	{
		return assignedActorCount_;
	}
	TacticalEntityId assignedActor(std::size_t index) const noexcept
	{
		return index < assignedActorCount_
			? assignedActors_[index] : TacticalEntityId{};
	}
	bool isActorAssigned(TacticalEntityId actor) const noexcept;
	// Borrowed until the next received frame/reset. No inventory is exposed
	// outside an active, exact-owner baseline. Contains bounded inventory and
	// nearby ground-item summaries only; it never drives local simulation.
	const CoopOwnerInventorySnapshot* ownerInventory(TacticalEntityId actor) const noexcept;
	bool hasLastIntentReceipt() const noexcept
	{
		return hasLastIntentReceipt_;
	}
	const CoopTacticalIntentReceipt& lastIntentReceipt() const noexcept
	{
		return lastIntentReceipt_;
	}

private:
	static constexpr unsigned MaximumResyncAttempts = 3;
	struct ReceiptHistoryEntry
	{
		std::uint64_t commandId = 0;
		CoopTacticalIntentReceiptStatus status =
			CoopTacticalIntentReceiptStatus::Rejected;
		CoopTacticalIntentReceiptBytes bytes{};
	};

	FullEngineCoopClientResult fail(
		FullEngineCoopClientResult result) noexcept;
	FullEngineCoopClientResult requestResync(
		CoopTacticalResyncReason reason, bool retry = false) noexcept;
	FullEngineCoopClientResult retireCredential() noexcept;
	bool sendFrame(const char* messageName,
		const std::uint8_t* bytes, std::size_t size) noexcept;
	void closeWire() noexcept;
	void clearCredentials() noexcept;
	bool acceptReceiptHistory(
		const CoopTacticalIntentReceipt& receipt,
		const CoopTacticalIntentReceiptBytes& bytes) noexcept;
	void clearReceiptHistory() noexcept;
	void clearReplicaState() noexcept;
	void clearConnectionState() noexcept;
	bool canReceiveCampaignResult() const noexcept;

	FullEngineCoopClientWire& wire_;
	FullEngineCoopPassiveReplicaSink& replica_;
	FullEngineCoopReconnectCredentialStore* credentialStore_ = nullptr;
	FullEngineCoopClientConfiguration configuration_;
	CoopCampaignStatus campaignStatus_;
	std::uint64_t lastArrivalDecision_ = 0;
	std::uint64_t lastSurrenderOffer_ = 0;
	std::uint64_t lastBattleNotice_ = 0;
	std::uint64_t lastMeanwhile_ = 0;
	CoopCampaignGroups campaignGroups_;
	CoopCampaignEconomy campaignEconomy_;
	CoopCampaignAimQuotes campaignAimQuotes_;
	CoopCampaignHireRequest pendingCampaignHire_;
	CoopCampaignHireResult lastCampaignHireResult_;
	std::int32_t pendingCampaignHireQuotedTotal_ = 0;
	std::uint64_t nextCampaignHireRequestId_ = 1;
	CoopCampaignActionRequest pendingCampaignAction_;
	CoopCampaignActionResult lastCampaignActionResult_;
	std::uint64_t nextCampaignActionRequestId_ = 1;
	CoopCampaignTimeRequest pendingCampaignTime_;
	CoopCampaignTimeResult lastCampaignTimeResult_;
	std::uint64_t nextCampaignTimeRequestId_ = 1;
	FullEngineCoopClientState state_ =
		FullEngineCoopClientState::Disconnected;
	FullEngineCoopClientResult lastResult_ =
		FullEngineCoopClientResult::Success;
	AdmissionRejectReason lastAdmissionRejectReason_ =
		AdmissionRejectReason::None;
	PeerIdentity peerIdentity_{};
	ReconnectToken reconnectToken_{};
	CoopTacticalStateIdentity acceptedState_{};
	std::array<TacticalEntityId,
		MaximumCoopTacticalAssignedActors> assignedActors_{};
	std::array<CoopOwnerInventorySnapshot,
		MaximumCoopTacticalAssignedActors> ownerInventories_{};
	std::array<ReceiptHistoryEntry,
		MaximumFullEngineCoopClientReceiptHistory> receiptHistory_{};
	std::size_t assignedActorCount_ = 0;
	std::size_t receiptHistoryHead_ = 0;
	std::size_t receiptHistoryCount_ = 0;
	std::uint64_t sessionEpoch_ = 0;
	std::uint64_t nextExpectedCommandId_ = 1;
	std::uint64_t outstandingCommandId_ = 0;
	std::uint64_t outstandingNextExpectedCommandId_ = 0;
	std::uint64_t lastDeltaId_ = 0;
	std::uint64_t acceptedBaselineId_ = 0;
	TacticalWorldDimensions acceptedDimensions_{};
	std::uint32_t lastPayloadChecksum_ = 0;
	std::uint64_t nextResyncRequestId_ = 1;
	unsigned resyncAttempts_ = 0;
	std::uint64_t nextSelfRetirementRequestId_ = 1;
	std::uint64_t selfRetirementRequestId_ = 0;
	CoopTacticalIntentReceipt lastIntentReceipt_{};
	bool configured_ = false;
	bool hasAcceptedState_ = false;
	bool hasLastIntentReceipt_ = false;
	bool credentialAbandonPending_ = false;
	bool selfRetirementAwaitingOutcome_ = false;
	FullEngineCoopClientState selfRetirementResumeState_ =
		FullEngineCoopClientState::Disconnected;
	bool replicaApplying_ = false;
	bool wireCalling_ = false;
	bool credentialStoreCalling_ = false;
};
}

#endif
