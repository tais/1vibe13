#include "DedicatedCoopTacticalHost.h"

#include <Engine/Adapters/JA2/TacticalCommandResultCodec.h>
#include <Engine/Adapters/JA2/TacticalCommandResultPublisher.h>
#include <Engine/Core/Identifier.h>

#include <limits>
#include <type_traits>
#include <utility>
#include <variant>

namespace
{
using CoopSession::AuthorizedTacticalIntent;
using CoopSession::CoopTacticalIntentReceipt;
using CoopSession::CoopTacticalIntentReceiptReason;
using CoopSession::CoopTacticalIntentReceiptStatus;
using CoopSession::TacticalIntentExecutionDisposition;

constexpr std::uint8_t InvalidTacticalTeam =
	std::numeric_limits<std::uint8_t>::max();
// ChangeStanceCommand deliberately retains the established JA2 animation
// height vocabulary. The production adapter TU statically ratchets these
// values against the legacy definitions without importing those headers here.
constexpr std::uint8_t Ja2StandingStance = 6;
constexpr std::uint8_t Ja2CrouchedStance = 3;
constexpr std::uint8_t Ja2ProneStance = 1;
constexpr std::uint8_t Ja2TacticalTeamCount = 11;

static_assert(CoopSession::MaximumTacticalFirearmAimTime ==
	TacticalMaximumAimedFirearmAimTime,
	"co-op firearm aim vocabulary must match the command boundary");

bool ValidAuthorizedIntentMetadata(
	const AuthorizedTacticalIntent& intent) noexcept
{
	CoopSession::CoopTacticalStateIdentity state;
	state.sessionEpoch = intent.context.sessionEpoch;
	state.worldGeneration = intent.context.worldGeneration;
	state.revision = intent.context.revision;
	state.turnSerial = intent.context.turnSerial;
	return CoopSession::IsValidCoopTacticalStateIdentity(state) &&
		!CoopSession::IsZero(intent.peerIdentity) &&
		intent.commandId != 0 && intent.actor.valid() &&
		!intent.payload.valueless_by_exception();
}

bool IsPassInterruptIntent(
	const CoopSession::TacticalIntentPayload& payload) noexcept
{
	return std::holds_alternative<
		CoopSession::PassInterruptTacticalIntent>(payload);
}

CoopTacticalIntentReceiptReason SubmissionReason(
	TacticalCommandSubmissionError error) noexcept
{
	switch (error)
	{
		case TacticalCommandSubmissionError::CapacityReached:
			return CoopTacticalIntentReceiptReason::InboxCapacityReached;
		case TacticalCommandSubmissionError::SequenceExhausted:
			return CoopTacticalIntentReceiptReason::AuthoritySequenceExhausted;
		case TacticalCommandSubmissionError::AllocationFailure:
			return CoopTacticalIntentReceiptReason::AllocationFailure;
		case TacticalCommandSubmissionError::InvalidCommand:
			return CoopTacticalIntentReceiptReason::GameplayRejected;
		case TacticalCommandSubmissionError::InvalidOwner:
		case TacticalCommandSubmissionError::None:
			return CoopTacticalIntentReceiptReason::QueueUnavailable;
	}
	return CoopTacticalIntentReceiptReason::QueueUnavailable;
}
}

DedicatedCoopTacticalHost::DedicatedCoopTacticalHost(
	DedicatedCoopTacticalLiveState& liveState,
	TacticalCommandService& commands,
	DedicatedCoopTacticalReceiptSink& receipts,
	std::string campaignPackageId,
	std::size_t maximumCorrelations) noexcept
	: liveState_(&liveState), commands_(&commands), receipts_(&receipts),
	  campaignPackageId_(std::move(campaignPackageId)),
	  maximumCorrelations_(
		maximumCorrelations <= MaximumDedicatedCoopTacticalCorrelations
			? maximumCorrelations : 0)
{
	if (!IsValidEngineIdentifier(campaignPackageId_))
		campaignPackageId_.clear();
}

CoopSession::CoopTacticalStateIdentity
DedicatedCoopTacticalHost::stateFor(
	const AuthorizedTacticalIntent& intent) const noexcept
{
	CoopSession::CoopTacticalStateIdentity state;
	state.sessionEpoch = intent.context.sessionEpoch;
	state.worldGeneration = intent.context.worldGeneration;
	state.revision = intent.context.revision;
	state.turnSerial = intent.context.turnSerial;
	return state;
}

CoopTacticalIntentReceipt DedicatedCoopTacticalHost::receiptFor(
	const AuthorizedTacticalIntent& intent,
	CoopTacticalIntentReceiptStatus status,
	CoopTacticalIntentReceiptReason reason) const noexcept
{
	CoopTacticalIntentReceipt receipt;
	receipt.state = stateFor(intent);
	receipt.peerIdentity = intent.peerIdentity;
	receipt.commandId = intent.commandId;
	receipt.nextExpectedCommandId =
		intent.commandId == std::numeric_limits<std::uint64_t>::max()
			? 0 : intent.commandId + 1;
	receipt.status = status;
	receipt.reason = reason;
	return receipt;
}

bool DedicatedCoopTacticalHost::tryPublish(
	const CoopTacticalIntentReceipt& receipt) noexcept
{
	return receipts_ != nullptr && receipts_->publish(receipt);
}

bool DedicatedCoopTacticalHost::hasPendingOutboundReceipt() const noexcept
{
	for (const Correlation& correlation : correlations_)
		if (correlation.occupied &&
			(correlation.immediateReceiptPending ||
			 correlation.queuedReceiptPending ||
			 correlation.terminalReceiptPending))
			return true;
	return false;
}

TacticalIntentExecutionDisposition DedicatedCoopTacticalHost::reject(
	const AuthorizedTacticalIntent& intent,
	CoopTacticalIntentReceiptReason reason,
	Correlation& obligation) noexcept
{
	obligation.terminalReceipt = receiptFor(intent,
		CoopTacticalIntentReceiptStatus::Rejected, reason);
	obligation.immediateReceiptPending = true;
	++immediateReceiptCount_;
	(void)flushPendingReceiptsInternal();
	return TacticalIntentExecutionDisposition::Rejected;
}

DedicatedCoopTacticalHost::Correlation*
DedicatedCoopTacticalHost::emptyCorrelation() noexcept
{
	if (obligationCount_ >= maximumCorrelations_) return nullptr;
	for (Correlation& correlation : correlations_)
		if (!correlation.occupied) return &correlation;
	return nullptr;
}

DedicatedCoopTacticalHost::Correlation*
DedicatedCoopTacticalHost::findCorrelation(std::uint64_t requestId) noexcept
{
	if (requestId == 0) return nullptr;
	for (Correlation& correlation : correlations_)
		if (correlation.occupied && correlation.requestId == requestId)
			return &correlation;
	return nullptr;
}

void DedicatedCoopTacticalHost::releaseCorrelation(
	Correlation& correlation) noexcept
{
	if (!correlation.occupied) return;
	if (correlation.requestId != 0 && correlationCount_ != 0)
		--correlationCount_;
	if (correlation.immediateReceiptPending && immediateReceiptCount_ != 0)
		--immediateReceiptCount_;
	correlation = Correlation{};
	if (obligationCount_ != 0) --obligationCount_;
}

bool DedicatedCoopTacticalHost::validateContextAndActor(
	const AuthorizedTacticalIntent& intent,
	DedicatedCoopTacticalTurnState& turn,
	CoopTacticalIntentReceiptReason& reason) const noexcept
{
	if (liveState_ == nullptr || !liveState_->captureTurn(turn) ||
		!turn.worldLoaded || turn.worldGeneration == 0 || turn.turnSerial == 0 ||
		turn.worldGeneration != intent.context.worldGeneration ||
		turn.turnSerial != intent.context.turnSerial ||
		turn.playerTeam >= Ja2TacticalTeamCount ||
		(turn.inCombat &&
			(!turn.turnBased ||
			 turn.currentTeam >= Ja2TacticalTeamCount)))
	{
		reason = CoopTacticalIntentReceiptReason::UnavailableContext;
		return false;
	}

	DedicatedCoopTacticalActorState actor;
	if (!liveState_->captureActor(intent.actor, actor))
	{
		reason = CoopTacticalIntentReceiptReason::UnavailableContext;
		return false;
	}
	if (!actor.exactIdentity || !actor.active || !actor.inSector)
	{
		reason = CoopTacticalIntentReceiptReason::ActorUnavailable;
		return false;
	}
	if (!actor.playerTeam)
	{
		reason = CoopTacticalIntentReceiptReason::WrongTeam;
		return false;
	}
	if (!actor.controllable)
	{
		reason = CoopTacticalIntentReceiptReason::GameplayRejected;
		return false;
	}

	if (turn.pendingCombatActions != 0 || turn.interruptPending ||
		turn.interruptPhase == TacticalInterruptPhase::Resolving)
	{
		reason = CoopTacticalIntentReceiptReason::GameplayRejected;
		return false;
	}

	switch (turn.interruptPhase)
	{
		case TacticalInterruptPhase::None:
			if (IsPassInterruptIntent(intent.payload) ||
				(turn.turnBased && turn.inCombat &&
				 turn.currentTeam != turn.playerTeam))
			{
				reason = CoopTacticalIntentReceiptReason::GameplayRejected;
				return false;
			}
			break;
		case TacticalInterruptPhase::Resolving:
			// Rejected above together with the live pending-interrupt gate.
			reason = CoopTacticalIntentReceiptReason::GameplayRejected;
			return false;
		case TacticalInterruptPhase::Active:
			if (!turn.turnBased || !turn.inCombat ||
				turn.interruptSerial == 0 ||
				turn.currentTeam != turn.playerTeam ||
				!actor.interruptActionEligible ||
				std::holds_alternative<
					CoopSession::EndTurnTacticalIntent>(intent.payload))
			{
				reason = CoopTacticalIntentReceiptReason::GameplayRejected;
				return false;
			}
			break;
		default:
			reason = CoopTacticalIntentReceiptReason::UnavailableContext;
			return false;
	}
	return true;
}

bool DedicatedCoopTacticalHost::translate(
	const AuthorizedTacticalIntent& intent,
	const DedicatedCoopTacticalTurnState& turn,
	SimulationCommand& command,
	CoopTacticalIntentReceiptReason& reason) const noexcept
{
	if (intent.payload.valueless_by_exception())
	{
		reason = CoopTacticalIntentReceiptReason::GameplayRejected;
		return false;
	}

	bool translated = true;
	std::visit([&](const auto& payload) noexcept {
		using Payload = typename std::decay<decltype(payload)>::type;
		if constexpr (std::is_same<Payload,
			CoopSession::MoveTacticalIntent>::value)
		{
			// MoveToGridCommand predates TacticalEventPolicy and its executor's
			// route call defaults to replication. Keeping every legacy role off is
			// therefore part of the production non-replication precondition.
			if (liveState_->legacyNetworkingActive())
			{
				reason = CoopTacticalIntentReceiptReason::UnavailableContext;
				translated = false;
				return;
			}
			command = MoveToGridCommand{
				intent.actor, payload.destinationGrid, payload.movementMode,
				payload.reverse, false, SimulationCommandSource::NetworkPeer,
				TacticalMoveOrigin::TeamAwareUi,
				TacticalPendingActionPolicy::Clear,
				TacticalCommandAuthorityPolicy::DedicatedCoop};
		}
		else if constexpr (std::is_same<Payload,
			CoopSession::FaceTacticalIntent>::value)
		{
			command = SetFacingCommand{
				intent.actor, payload.direction,
				SimulationCommandSource::NetworkPeer,
				TacticalEventPolicy::LocalOnly,
				TacticalCommandAuthorityPolicy::DedicatedCoop};
		}
		else if constexpr (std::is_same<Payload,
			CoopSession::StanceTacticalIntent>::value)
		{
			std::uint8_t stance = 0;
			switch (payload.stance)
			{
				case CoopSession::TacticalIntentStance::Standing:
					stance = Ja2StandingStance;
					break;
				case CoopSession::TacticalIntentStance::Crouched:
					stance = Ja2CrouchedStance;
					break;
				case CoopSession::TacticalIntentStance::Prone:
					stance = Ja2ProneStance;
					break;
				default:
					reason = CoopTacticalIntentReceiptReason::GameplayRejected;
					translated = false;
					return;
			}
			command = ChangeStanceCommand{
				intent.actor, stance, SimulationCommandSource::NetworkPeer,
				TacticalEventPolicy::LocalOnly,
				TacticalCommandAuthorityPolicy::DedicatedCoop};
		}
		else if constexpr (std::is_same<Payload,
			CoopSession::StopTacticalIntent>::value)
		{
			command = StopMovementCommand{
				intent.actor, SimulationCommandSource::NetworkPeer,
				TacticalCommandAuthorityPolicy::DedicatedCoop};
		}
		else if constexpr (std::is_same<Payload,
			CoopSession::AimedFirearmAttackTacticalIntent>::value)
		{
			if (liveState_->legacyNetworkingActive())
			{
				reason = CoopTacticalIntentReceiptReason::UnavailableContext;
				translated = false;
				return;
			}
			AimedFirearmAttackCommand prepared{};
			if (!liveState_->prepareAimedFirearmAttack(
					intent.actor, payload.target, payload.aimTime, prepared))
			{
				reason = CoopTacticalIntentReceiptReason::GameplayRejected;
				translated = false;
				return;
			}
			command = prepared;
		}
		else if constexpr (std::is_same<Payload,
			CoopSession::ReloadTacticalIntent>::value)
		{
			ReloadWeaponCommand prepared{};
			if (!liveState_->prepareReloadWeapon(intent.actor, prepared))
			{
				reason = CoopTacticalIntentReceiptReason::GameplayRejected;
				translated = false;
				return;
			}
			command = prepared;
		}
		else if constexpr (std::is_same<Payload,
			CoopSession::DoorOpenCloseTacticalIntent>::value)
		{
			// The native synchronous door seam assumes there is no legacy event
			// producer that could reflect or race the same mutation.
			if (liveState_->legacyNetworkingActive())
			{
				reason = CoopTacticalIntentReceiptReason::UnavailableContext;
				translated = false;
				return;
			}
			AuthoritativeDoorOpenCloseCommand prepared{};
			if (!liveState_->prepareDoorOpenClose(
					intent.actor,
					TacticalWorldObjectId{
						payload.baseGrid, payload.structureId},
					payload.desiredOpen, prepared))
			{
				reason = CoopTacticalIntentReceiptReason::GameplayRejected;
				translated = false;
				return;
			}
			command = prepared;
		}
		else if constexpr (std::is_same<Payload,
			CoopSession::EndTurnTacticalIntent>::value)
		{
			if (!turn.turnBased || !turn.inCombat ||
				turn.currentTeam != turn.playerTeam ||
				turn.playerTeam >= Ja2TacticalTeamCount - 1 ||
				turn.nextTeam >= Ja2TacticalTeamCount ||
				turn.nextTeam != turn.playerTeam + 1 ||
				turn.nextTeam == InvalidTacticalTeam ||
				turn.nextTeam == turn.playerTeam)
			{
				reason = CoopTacticalIntentReceiptReason::GameplayRejected;
				translated = false;
				return;
			}
			command = EndTurnCommand{
				turn.nextTeam, SimulationCommandSource::NetworkPeer,
				TacticalCommandAuthorityPolicy::DedicatedCoop};
		}
		else if constexpr (std::is_same<Payload,
			CoopSession::PassInterruptTacticalIntent>::value)
		{
			if (turn.interruptPhase != TacticalInterruptPhase::Active ||
				turn.interruptSerial == 0 ||
				payload.interruptSerial != turn.interruptSerial)
			{
				reason = CoopTacticalIntentReceiptReason::GameplayRejected;
				translated = false;
				return;
			}
			command = PassInterruptCommand{
				intent.actor, turn.worldGeneration, turn.interruptSerial,
				SimulationCommandSource::NetworkPeer,
				TacticalCommandAuthorityPolicy::DedicatedCoop};
		}
		else
		{
			reason = CoopTacticalIntentReceiptReason::GameplayRejected;
			translated = false;
		}
	}, intent.payload);
	if (!translated) return false;
	if (!IsStructurallyValidSimulationCommand(command))
	{
		reason = CoopTacticalIntentReceiptReason::GameplayRejected;
		return false;
	}
	return true;
}

bool DedicatedCoopTacticalHost::ready() const noexcept
{
	if (liveState_ == nullptr || commands_ == nullptr || receipts_ == nullptr ||
		campaignPackageId_.empty() || !liveState_->onMainThread() ||
		operationActive_ || obligationCount_ >= maximumCorrelations_ ||
		hasPendingOutboundReceipt() || !liveState_->dedicatedCoopActive() ||
		!liveState_->campaignPackageActive(campaignPackageId_))
		return false;
	DedicatedCoopTacticalTurnState turn;
	return liveState_->captureTurn(turn) && turn.worldLoaded &&
		turn.worldGeneration != 0 && turn.turnSerial != 0;
}

TacticalIntentExecutionDisposition DedicatedCoopTacticalHost::execute(
	const AuthorizedTacticalIntent& intent) noexcept
{
	if (liveState_ == nullptr || commands_ == nullptr || receipts_ == nullptr ||
		!liveState_->onMainThread() || operationActive_)
		return TacticalIntentExecutionDisposition::Rejected;
	OperationGuard operation(operationActive_);
	(void)flushPendingReceiptsInternal();
	Correlation* obligation = emptyCorrelation();
	if (obligation == nullptr)
		return TacticalIntentExecutionDisposition::Rejected;
	obligation->occupied = true;
	++obligationCount_;

	if (!ValidAuthorizedIntentMetadata(intent))
	{
		releaseCorrelation(*obligation);
		return TacticalIntentExecutionDisposition::Rejected;
	}
	if (campaignPackageId_.empty() || !liveState_->dedicatedCoopActive() ||
		!liveState_->campaignPackageActive(campaignPackageId_))
		return reject(intent,
			CoopTacticalIntentReceiptReason::UnavailableContext, *obligation);

	DedicatedCoopTacticalTurnState turn;
	CoopTacticalIntentReceiptReason reason =
		CoopTacticalIntentReceiptReason::GameplayRejected;
	if (!validateContextAndActor(intent, turn, reason))
		return reject(intent, reason, *obligation);

	SimulationCommand command = StopMovementCommand{
		intent.actor, SimulationCommandSource::NetworkPeer};
	if (!translate(intent, turn, command, reason))
		return reject(intent, reason, *obligation);

	const TacticalCommandSubmissionResult submitted =
		commands_->submit(campaignPackageId_, command);
	if (!submitted)
		return reject(intent, SubmissionReason(submitted.error), *obligation);
	if (findCorrelation(submitted.requestId) != nullptr)
		return reject(intent,
			CoopTacticalIntentReceiptReason::QueueUnavailable, *obligation);

	obligation->requestId = submitted.requestId;
	obligation->peerIdentity = intent.peerIdentity;
	obligation->commandId = intent.commandId;
	obligation->nextExpectedCommandId =
		intent.commandId == std::numeric_limits<std::uint64_t>::max()
			? 0 : intent.commandId + 1;
	obligation->state = stateFor(intent);
	++correlationCount_;

	obligation->queuedReceiptPending = true;
	(void)flushPendingReceiptsInternal();
	return TacticalIntentExecutionDisposition::Retained;
}

bool DedicatedCoopTacticalHost::mapTerminalResult(
	const TacticalCommandResult& result,
	const Correlation& correlation,
	CoopTacticalIntentReceipt& receipt) const noexcept
{
	CoopTacticalIntentReceipt mapped;
	mapped.state = correlation.state;
	mapped.peerIdentity = correlation.peerIdentity;
	mapped.commandId = correlation.commandId;
	mapped.nextExpectedCommandId = correlation.nextExpectedCommandId;
	mapped.authoritativeSequence = result.authoritativeSequence;
	mapped.simulationTick = result.simulationTick;

	switch (result.status)
	{
		case TacticalCommandTerminalStatus::Applied:
			if (result.reason != TacticalCommandTerminalReason::None)
				return false;
			mapped.status = CoopTacticalIntentReceiptStatus::Applied;
			mapped.reason = CoopTacticalIntentReceiptReason::None;
			break;
		case TacticalCommandTerminalStatus::Discarded:
			if (result.reason !=
				TacticalCommandTerminalReason::AuthoritativeDiscard)
				return false;
			mapped.status = CoopTacticalIntentReceiptStatus::Discarded;
			mapped.reason =
				CoopTacticalIntentReceiptReason::AuthoritativeDiscard;
			break;
		case TacticalCommandTerminalStatus::Cancelled:
			if (result.reason != TacticalCommandTerminalReason::PackageTeardown)
				return false;
			mapped.status = CoopTacticalIntentReceiptStatus::Cancelled;
			mapped.reason = CoopTacticalIntentReceiptReason::SessionEnded;
			break;
		case TacticalCommandTerminalStatus::Rejected:
			mapped.status = CoopTacticalIntentReceiptStatus::Rejected;
			switch (result.reason)
			{
				case TacticalCommandTerminalReason::InactiveOwner:
				case TacticalCommandTerminalReason::UnavailableContext:
					mapped.reason =
						CoopTacticalIntentReceiptReason::UnavailableContext;
					break;
				case TacticalCommandTerminalReason::InvalidDomain:
					mapped.reason =
						CoopTacticalIntentReceiptReason::GameplayRejected;
					break;
					case TacticalCommandTerminalReason::SequenceExhausted:
						mapped.reason = CoopTacticalIntentReceiptReason::
							AuthoritySequenceExhausted;
					break;
				case TacticalCommandTerminalReason::None:
				case TacticalCommandTerminalReason::PackageTeardown:
				case TacticalCommandTerminalReason::AuthoritativeDiscard:
					return false;
			}
			break;
	}
	receipt = mapped;
	return true;
}

void DedicatedCoopTacticalHost::receiveMessage(
	const RuntimeMessage& message) noexcept
{
	if (liveState_ == nullptr || !liveState_->onMainThread() ||
		operationActive_)
		return;
	OperationGuard operation(operationActive_);
	if (message.topic != TacticalCommandResultMessageTopic ||
		message.source != TacticalCommandResultMessageSource)
		return;

	TacticalCommandResult result;
	if (DecodeTacticalCommandResult(message.payload, result) !=
		TacticalCommandResultDecodeError::None ||
		result.packageId != campaignPackageId_)
		return;
	Correlation* correlation = findCorrelation(result.requestId);
	if (correlation == nullptr || correlation->terminalReceiptPending) return;

	CoopTacticalIntentReceipt terminal;
	if (!mapTerminalResult(result, *correlation, terminal)) return;
	correlation->terminalReceipt = terminal;
	correlation->terminalReceiptPending = true;
	// RuntimeMessage dispatch precedes the committed-frame world observer. Keep
	// the terminal result private until the coordinator explicitly flushes: the
	// production sink then normalizes it to the newly published revision and
	// server-owned command cursor.
}

std::size_t DedicatedCoopTacticalHost::flushPendingReceiptsInternal() noexcept
{
	std::size_t published = 0;
	for (Correlation& correlation : correlations_)
	{
		if (!correlation.occupied) continue;
		if (correlation.immediateReceiptPending)
		{
			if (!tryPublish(correlation.terminalReceipt)) return published;
			++published;
			releaseCorrelation(correlation);
			continue;
		}
		if (correlation.queuedReceiptPending)
		{
			CoopTacticalIntentReceipt queued;
			queued.state = correlation.state;
			queued.peerIdentity = correlation.peerIdentity;
			queued.commandId = correlation.commandId;
			queued.nextExpectedCommandId =
				correlation.nextExpectedCommandId;
			queued.status = CoopTacticalIntentReceiptStatus::Queued;
			queued.reason = CoopTacticalIntentReceiptReason::None;
			if (!tryPublish(queued)) return published;
			correlation.queuedReceiptPending = false;
			++published;
		}
		if (!correlation.terminalReceiptPending) continue;
		if (!tryPublish(correlation.terminalReceipt)) return published;
		++published;
		releaseCorrelation(correlation);
	}
	return published;
}

std::size_t DedicatedCoopTacticalHost::flushPendingReceipts() noexcept
{
	if (liveState_ == nullptr || !liveState_->onMainThread() ||
		operationActive_)
		return 0;
	OperationGuard operation(operationActive_);
	return flushPendingReceiptsInternal();
}

bool DedicatedCoopTacticalHost::endWorld() noexcept
{
	if (liveState_ == nullptr || !liveState_->onMainThread() ||
		operationActive_)
		return false;
	OperationGuard operation(operationActive_);
	(void)flushPendingReceiptsInternal();
	for (Correlation& correlation : correlations_)
	{
		// A failed validation/submission is already a terminal obligation.  Keep
		// its precise rejection reason if world teardown races outbound
		// backpressure; only accepted requests still awaiting a result become
		// session-ended cancellations here.
		if (!correlation.occupied || correlation.immediateReceiptPending ||
			correlation.terminalReceiptPending)
			continue;
		CoopTacticalIntentReceipt cancelled;
		cancelled.state = correlation.state;
		cancelled.peerIdentity = correlation.peerIdentity;
		cancelled.commandId = correlation.commandId;
		cancelled.nextExpectedCommandId =
			correlation.nextExpectedCommandId;
		cancelled.status = CoopTacticalIntentReceiptStatus::Cancelled;
		cancelled.reason = CoopTacticalIntentReceiptReason::SessionEnded;
		correlation.terminalReceipt = cancelled;
		correlation.terminalReceiptPending = true;
	}
	(void)flushPendingReceiptsInternal();
	return obligationCount_ == 0;
}

bool DedicatedCoopTacticalHost::collectControllableActors(
	DedicatedCoopTacticalActorList& actors,
	std::size_t& count) noexcept
{
	if (liveState_ == nullptr || !liveState_->onMainThread() ||
		operationActive_ || campaignPackageId_.empty())
		return false;
	OperationGuard operation(operationActive_);
	if (!liveState_->dedicatedCoopActive() ||
		!liveState_->campaignPackageActive(campaignPackageId_))
		return false;

	DedicatedCoopTacticalTurnState turn;
	if (!liveState_->captureTurn(turn) || !turn.worldLoaded ||
		turn.worldGeneration == 0 || turn.turnSerial == 0)
		return false;
	DedicatedCoopTacticalActorList captured{};
	std::size_t capturedCount = 0;
	if (!liveState_->collectControllableActors(captured, capturedCount) ||
		capturedCount > captured.size())
		return false;

	for (std::size_t index = 0; index < capturedCount; ++index)
	{
		if (!captured[index].valid() ||
			(index != 0 && !(captured[index - 1] < captured[index])))
			return false;
		DedicatedCoopTacticalActorState state;
		if (!liveState_->captureActor(captured[index], state) ||
			!state.exactIdentity || !state.active || !state.inSector ||
			!state.playerTeam || !state.controllable)
			return false;
	}
	actors = captured;
	count = capturedCount;
	return true;
}
