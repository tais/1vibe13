#ifndef JA2_DEDICATED_COOP_TACTICAL_HOST_H
#define JA2_DEDICATED_COOP_TACTICAL_HOST_H

#include "CoopInventoryAuthority.h"
#include "DedicatedCoopAttack.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

#include <Engine/Adapters/JA2/TacticalCommandService.h>
#include <Engine/Adapters/JA2/TacticalCommandResult.h>
#include <Engine/Core/RuntimeMessageBus.h>
#include <Multiplayer/CoopTacticalProtocol.h>
#include <Multiplayer/FullEngineCoopIngress.h>

class GameContext;

inline constexpr std::size_t MaximumDedicatedCoopTacticalCorrelations =
	CoopSession::MaximumCoopTacticalAssignedActors;

using DedicatedCoopTacticalActorList = std::array<TacticalEntityId,
	CoopSession::MaximumCoopTacticalAssignedActors>;

// A receipt has already crossed the gameplay boundary when this callback runs.
// The implementation must copy it into its own bounded main-thread outbound
// state before returning true. It must never call back into this host.
class DedicatedCoopTacticalReceiptSink
{
public:
	virtual ~DedicatedCoopTacticalReceiptSink() = default;
	virtual bool publish(
		const CoopSession::CoopTacticalIntentReceipt& receipt) noexcept = 0;
};

struct DedicatedCoopTacticalActorState
{
	bool exactIdentity = false;
	bool active = false;
	bool inSector = false;
	bool playerTeam = false;
	bool controllable = false;
	// Authoritative native interrupt eligibility. This includes the actor's
	// active-queue/moved state and the co-op coordinator's pass vote.
	bool interruptActionEligible = false;
};

struct DedicatedCoopTacticalTurnState
{
	bool worldLoaded = false;
	std::uint64_t worldGeneration = 0;
	std::uint64_t turnSerial = 0;
	bool turnBased = false;
	bool inCombat = false;
	std::uint8_t currentTeam = 0;
	std::uint8_t playerTeam = 0;
	std::uint8_t nextTeam = 0;
	std::uint32_t pendingCombatActions = 0;
	bool interruptPending = false;
	TacticalInterruptPhase interruptPhase = TacticalInterruptPhase::None;
	std::uint64_t interruptSerial = 0;
	bool sharedDecisionPending = false;
};

// Read-only seams used by the main-thread bridge. The production implementation
// below resolves exact JA2 incarnations; focused tests can supply pointer-free
// snapshots without constructing a tactical world.
class DedicatedCoopTacticalLiveState
{
public:
	virtual ~DedicatedCoopTacticalLiveState() = default;
	virtual bool onMainThread() const noexcept = 0;
	virtual bool dedicatedCoopActive() const noexcept = 0;
	virtual bool campaignPackageActive(
		const std::string& packageId) const noexcept = 0;
	virtual bool legacyNetworkingActive() const noexcept = 0;
	virtual bool captureTurn(
		DedicatedCoopTacticalTurnState& state) const noexcept = 0;
	virtual bool captureActor(
		TacticalEntityId actor,
		DedicatedCoopTacticalActorState& state) const noexcept = 0;
	virtual bool canBeginMoveToGrid(
		TacticalEntityId actor,
		std::int32_t destinationGrid,
		std::uint16_t movementMode,
		bool reverse) const noexcept = 0;
	virtual bool prepareAimedFirearmAttack(
		TacticalEntityId actor,
		TacticalEntityId target,
		std::uint8_t aimTime,
		AimedFirearmAttackCommand& command) const noexcept = 0;
	virtual DedicatedCoopAttackOutcome captureAttackOutcome(
		std::uint64_t sequence, std::uint64_t world, TacticalEntityId actor) const noexcept = 0;
	virtual void releaseAttackOutcome(
		std::uint64_t sequence, std::uint64_t world, TacticalEntityId actor) noexcept = 0;
	virtual bool prepareReloadWeapon(
		TacticalEntityId actor,
		ReloadWeaponCommand& command) const noexcept = 0;
	virtual bool prepareBeginFirstAid(
		TacticalEntityId actor, TacticalEntityId target,
		BeginFirstAidCommand& command) const noexcept = 0;
	// Talk preflight; on success remembers `peer` as the requester of exactly
	// this merc/NPC pair so the panel that opens takes it as initiator.
	virtual bool prepareTalk(const CoopSession::PeerIdentity& peer,
		TacticalEntityId actor, TacticalEntityId target,
		StartConversationCommand& command) noexcept = 0;
	// Answer preflight against the exact published conversation.
	virtual bool prepareConverse(const CoopSession::PeerIdentity& peer,
		TacticalEntityId actor, std::uint32_t conversation, std::uint8_t approach,
		std::uint64_t worldGeneration, AuthoritativeConverseCommand& command) const noexcept = 0;
	virtual bool prepareSwapInventorySlots(
		TacticalEntityId actor, std::uint16_t sourceSlot, std::uint16_t destinationSlot,
		std::uint64_t expectedInventoryRevision,
		SwapInventorySlotsCommand& command) const noexcept = 0;
	virtual bool prepareTransferInventoryItem(
		TacticalEntityId actor, TacticalEntityId target,
		std::uint16_t sourceSlot, std::uint16_t destinationSlot,
		std::uint64_t expectedInventoryRevision,
		std::uint64_t expectedTargetInventoryRevision,
		TransferInventoryItemCommand& command) const noexcept = 0;
	virtual bool prepareDoorOpenClose(
		TacticalEntityId actor,
		TacticalWorldObjectId object,
		bool desiredOpen,
		AuthoritativeDoorOpenCloseCommand& command) const noexcept = 0;
	virtual bool prepareDoorAction(
		TacticalEntityId actor,
		TacticalWorldObjectId object,
		std::uint8_t action,
		AuthoritativeDoorActionCommand& command) const noexcept = 0;
	virtual bool prepareWeaponMode(
		TacticalEntityId actor,
		std::uint8_t expectedWeaponMode,
		AuthoritativeWeaponModeCommand& command) const noexcept = 0;
	virtual bool prepareThrow(
		TacticalEntityId actor,
		std::int32_t targetGrid,
		std::int8_t targetLevel,
		AuthoritativeThrowCommand& command) const noexcept = 0;
	virtual bool prepareAttachItem(
		TacticalEntityId actor,
		std::uint8_t sourceSlot,
		std::uint8_t targetSlot,
		AuthoritativeAttachItemCommand& command) const noexcept = 0;
	virtual bool preparePickupWorldItem(
		TacticalEntityId actor, TacticalWorldItemId item, std::uint16_t destinationSlot,
		std::uint64_t expectedInventoryRevision,
		AuthoritativePickupWorldItemCommand& command) const noexcept = 0;
	virtual bool prepareDropInventoryItem(
		TacticalEntityId actor, std::uint16_t sourceSlot,
		std::uint64_t expectedInventoryRevision,
		AuthoritativeDropInventoryItemCommand& command) const noexcept = 0;
	virtual bool prepareGiveItem(
		TacticalEntityId actor, TacticalEntityId target, std::uint16_t sourceSlot,
		std::uint64_t expectedInventoryRevision,
		AuthoritativeGiveItemCommand& command) const noexcept = 0;
	virtual bool collectControllableActors(
		DedicatedCoopTacticalActorList& actors,
		std::size_t& count) const noexcept = 0;
};

// The only legacy-facing part of the composition. It is read-only and records
// the constructing thread as the JA2 coordinator thread.
class DedicatedCoopTacticalJa2LiveState final
	: public DedicatedCoopTacticalLiveState
{
public:
	explicit DedicatedCoopTacticalJa2LiveState(GameContext& game) noexcept;

	bool onMainThread() const noexcept override;
	bool dedicatedCoopActive() const noexcept override;
	bool campaignPackageActive(
		const std::string& packageId) const noexcept override;
	bool legacyNetworkingActive() const noexcept override;
	bool captureTurn(
		DedicatedCoopTacticalTurnState& state) const noexcept override;
	bool captureActor(
		TacticalEntityId actor,
		DedicatedCoopTacticalActorState& state) const noexcept override;
	bool canBeginMoveToGrid(
		TacticalEntityId actor,
		std::int32_t destinationGrid,
		std::uint16_t movementMode,
		bool reverse) const noexcept override;
	bool prepareAimedFirearmAttack(
		TacticalEntityId actor,
		TacticalEntityId target,
		std::uint8_t aimTime,
		AimedFirearmAttackCommand& command) const noexcept override;
	DedicatedCoopAttackOutcome captureAttackOutcome(
		std::uint64_t sequence, std::uint64_t world, TacticalEntityId actor) const noexcept override;
	void releaseAttackOutcome(
		std::uint64_t sequence, std::uint64_t world, TacticalEntityId actor) noexcept override;
	bool prepareReloadWeapon(
		TacticalEntityId actor,
		ReloadWeaponCommand& command) const noexcept override;
	bool prepareBeginFirstAid(
		TacticalEntityId actor, TacticalEntityId target,
		BeginFirstAidCommand& command) const noexcept override;
	bool prepareTalk(const CoopSession::PeerIdentity& peer,
		TacticalEntityId actor, TacticalEntityId target,
		StartConversationCommand& command) noexcept override;
	bool prepareConverse(const CoopSession::PeerIdentity& peer,
		TacticalEntityId actor, std::uint32_t conversation, std::uint8_t approach,
		std::uint64_t worldGeneration, AuthoritativeConverseCommand& command) const noexcept override;
	bool prepareSwapInventorySlots(
		TacticalEntityId actor, std::uint16_t sourceSlot, std::uint16_t destinationSlot,
		std::uint64_t expectedInventoryRevision,
		SwapInventorySlotsCommand& command) const noexcept override;
	bool prepareTransferInventoryItem(
		TacticalEntityId actor, TacticalEntityId target,
		std::uint16_t sourceSlot, std::uint16_t destinationSlot,
		std::uint64_t expectedInventoryRevision,
		std::uint64_t expectedTargetInventoryRevision,
		TransferInventoryItemCommand& command) const noexcept override;
	bool preparePickupWorldItem(
		TacticalEntityId actor, TacticalWorldItemId item, std::uint16_t destinationSlot,
		std::uint64_t expectedInventoryRevision,
		AuthoritativePickupWorldItemCommand& command) const noexcept override;
	bool prepareDropInventoryItem(
		TacticalEntityId actor, std::uint16_t sourceSlot,
		std::uint64_t expectedInventoryRevision,
		AuthoritativeDropInventoryItemCommand& command) const noexcept override;
	bool prepareGiveItem(
		TacticalEntityId actor, TacticalEntityId target, std::uint16_t sourceSlot,
		std::uint64_t expectedInventoryRevision,
		AuthoritativeGiveItemCommand& command) const noexcept override;
	bool prepareDoorOpenClose(
		TacticalEntityId actor,
		TacticalWorldObjectId object,
		bool desiredOpen,
		AuthoritativeDoorOpenCloseCommand& command) const noexcept override;
	bool prepareDoorAction(
		TacticalEntityId actor,
		TacticalWorldObjectId object,
		std::uint8_t action,
		AuthoritativeDoorActionCommand& command) const noexcept override;
	bool prepareWeaponMode(
		TacticalEntityId actor,
		std::uint8_t expectedWeaponMode,
		AuthoritativeWeaponModeCommand& command) const noexcept override;
	bool prepareThrow(
		TacticalEntityId actor,
		std::int32_t targetGrid,
		std::int8_t targetLevel,
		AuthoritativeThrowCommand& command) const noexcept override;
	bool prepareAttachItem(
		TacticalEntityId actor,
		std::uint8_t sourceSlot,
		std::uint8_t targetSlot,
		AuthoritativeAttachItemCommand& command) const noexcept override;
	bool collectControllableActors(
		DedicatedCoopTacticalActorList& actors,
		std::size_t& count) const noexcept override;

	// Read native carried contents only on the committed-frame coordinator thread.
	bool captureInventory(TacticalEntityId actor, std::uint64_t worldGeneration,
		CoopSession::CoopOwnerInventorySnapshot& inventory) const noexcept;

private:
	mutable Ja2CoopInventoryAuthority inventoryAuthority_;
	GameContext* game_ = nullptr;
	std::thread::id mainThread_;
};

// Main-thread bridge from authenticated co-op intent to the existing bounded
// package command inbox, and from its runtime result topic back to wire-level
// co-op receipts. It never observes a peer-claimed identity.
class DedicatedCoopTacticalHost final
	: public CoopSession::TacticalIntentExecutionSink,
	  public RuntimeMessageSink
{
public:
	explicit DedicatedCoopTacticalHost(
		DedicatedCoopTacticalLiveState& liveState,
		TacticalCommandService& commands,
		DedicatedCoopTacticalReceiptSink& receipts,
		std::string campaignPackageId,
		std::size_t maximumCorrelations =
			MaximumDedicatedCoopTacticalCorrelations) noexcept;

	DedicatedCoopTacticalHost(const DedicatedCoopTacticalHost&) = delete;
	DedicatedCoopTacticalHost& operator=(
		const DedicatedCoopTacticalHost&) = delete;

	CoopSession::TacticalIntentExecutionDisposition execute(
		const CoopSession::AuthorizedTacticalIntent& intent) noexcept override;
	// FullEngineCoopIngress queries this before authorization consumes the
	// admission-epoch command identifier. It is intentionally read-only.
	bool ready() const noexcept override;
	// Runtime-result delivery only retains the terminal obligation. The
	// coordinator publishes it with flushPendingReceipts() after the resulting
	// authoritative world revision has crossed the observer boundary.
	void receiveMessage(const RuntimeMessage& message) noexcept override;

	// Retry receipts retained after bounded outbound backpressure. This performs
	// no work off the constructing/main thread.
	std::size_t flushPendingReceipts() noexcept;
	// Convert every still-correlated request into a terminal session-ended
	// receipt. The coordinator must first drain/cancel the underlying command
	// service; this bridge deliberately has no host-only inbox mutation API.
	// False means outbound backpressure retained one or more receipts for a later
	// flush/endWorld retry.
	bool endWorld() noexcept;

	// Server-originated (System) interrupt pass for one eligible merc whose
	// owner is offline, against the live interrupt grant. No peer receipt: the
	// result is uncorrelated. False when no live grant/context allows it.
	bool passInterruptForOfflineOwner(TacticalEntityId actor) noexcept;

	// Deterministic assignment input for the server coordinator. Failure leaves
	// both caller outputs unchanged.
	bool collectControllableActors(
		DedicatedCoopTacticalActorList& actors,
		std::size_t& count) noexcept;

	std::size_t correlationCount() const noexcept
	{
		return correlationCount_;
	}
	std::size_t pendingImmediateReceiptCount() const noexcept
	{
		return immediateReceiptCount_;
	}
	const std::string& campaignPackageId() const noexcept
	{
		return campaignPackageId_;
	}

private:
	struct Correlation
	{
		bool occupied = false;
		std::uint64_t requestId = 0;
		CoopSession::PeerIdentity peerIdentity{};
		std::uint64_t commandId = 0;
		std::uint64_t nextExpectedCommandId = 0;
		CoopSession::CoopTacticalStateIdentity state;
		bool immediateReceiptPending = false;
		bool queuedReceiptPending = false;
		bool terminalReceiptPending = false;
		bool nativeAttack = false;
		bool nativeAttackTerminal = false;
		TacticalEntityId actor{};
		CoopSession::CoopTacticalIntentReceipt terminalReceipt;
	};

	class OperationGuard
	{
	public:
		explicit OperationGuard(bool& active) noexcept : active_(active)
		{
			active_ = true;
		}
		~OperationGuard() { active_ = false; }
		OperationGuard(const OperationGuard&) = delete;
		OperationGuard& operator=(const OperationGuard&) = delete;
	private:
		bool& active_;
	};

	CoopSession::CoopTacticalStateIdentity stateFor(
		const CoopSession::AuthorizedTacticalIntent& intent) const noexcept;
	CoopSession::CoopTacticalIntentReceipt receiptFor(
		const CoopSession::AuthorizedTacticalIntent& intent,
		CoopSession::CoopTacticalIntentReceiptStatus status,
		CoopSession::CoopTacticalIntentReceiptReason reason) const noexcept;
	CoopSession::TacticalIntentExecutionDisposition reject(
		const CoopSession::AuthorizedTacticalIntent& intent,
		CoopSession::CoopTacticalIntentReceiptReason reason,
		Correlation& obligation) noexcept;
	bool tryPublish(
		const CoopSession::CoopTacticalIntentReceipt& receipt) noexcept;
	bool hasPendingOutboundReceipt() const noexcept;
	std::size_t flushPendingReceiptsInternal() noexcept;
	Correlation* emptyCorrelation() noexcept;
	Correlation* findCorrelation(std::uint64_t requestId) noexcept;
	void releaseCorrelation(Correlation& correlation) noexcept;
	bool validateContextAndActor(
		const CoopSession::AuthorizedTacticalIntent& intent,
		DedicatedCoopTacticalTurnState& turn,
		CoopSession::CoopTacticalIntentReceiptReason& reason) const noexcept;
	bool translate(
		const CoopSession::AuthorizedTacticalIntent& intent,
		const DedicatedCoopTacticalTurnState& turn,
		SimulationCommand& command,
		CoopSession::CoopTacticalIntentReceiptReason& reason) const noexcept;
	bool mapTerminalResult(
		const TacticalCommandResult& result,
		const Correlation& correlation,
		CoopSession::CoopTacticalIntentReceipt& receipt) const noexcept;

	DedicatedCoopTacticalLiveState* liveState_ = nullptr;
	TacticalCommandService* commands_ = nullptr;
	DedicatedCoopTacticalReceiptSink* receipts_ = nullptr;
	std::string campaignPackageId_;
	std::size_t maximumCorrelations_ = 0;
	std::array<Correlation,
		MaximumDedicatedCoopTacticalCorrelations> correlations_{};
	std::size_t obligationCount_ = 0;
	std::size_t correlationCount_ = 0;
	std::size_t immediateReceiptCount_ = 0;
	bool operationActive_ = false;
};

#endif
