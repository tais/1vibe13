#include "FullEngineCoopClientPresentationCommandFeedback.h"

#include <limits>

namespace
{
using FeedbackState =
	FullEngineCoopClientPresentationCommandFeedbackState;
using ReceiptStatus = CoopSession::CoopTacticalIntentReceiptStatus;
using ReceiptReason = CoopSession::CoopTacticalIntentReceiptReason;
using LocalFailure = FullEngineCoopClientPresentationLocalFailure;

bool NonzeroPeer(const CoopSession::PeerIdentity& peer) noexcept
{
	for (const std::uint8_t byte : peer)
		if (byte != 0) return true;
	return false;
}

bool KnownStatus(ReceiptStatus status) noexcept
{
	switch (status)
	{
		case ReceiptStatus::Queued:
		case ReceiptStatus::Rejected:
		case ReceiptStatus::Applied:
		case ReceiptStatus::Discarded:
		case ReceiptStatus::Cancelled:
			return true;
	}
	return false;
}

bool KnownReason(ReceiptReason reason) noexcept
{
	switch (reason)
	{
		case ReceiptReason::None:
		case ReceiptReason::MalformedIntent:
		case ReceiptReason::NotAdmitted:
		case ReceiptReason::SessionMismatch:
		case ReceiptReason::WorldMismatch:
		case ReceiptReason::RevisionMismatch:
		case ReceiptReason::TurnMismatch:
		case ReceiptReason::InvalidCommandSequence:
		case ReceiptReason::ActorNotOwned:
		case ReceiptReason::NotBaselineReady:
		case ReceiptReason::ActorUnavailable:
		case ReceiptReason::WrongTeam:
		case ReceiptReason::GameplayRejected:
		case ReceiptReason::InboxCapacityReached:
		case ReceiptReason::InboxSequenceExhausted:
		case ReceiptReason::AllocationFailure:
		case ReceiptReason::QueueUnavailable:
		case ReceiptReason::UnavailableContext:
		case ReceiptReason::AuthoritativeDiscard:
		case ReceiptReason::SessionEnded:
		case ReceiptReason::AuthoritySequenceExhausted:
			return true;
	}
	return false;
}

bool KnownLocalFailure(LocalFailure failure) noexcept
{
	switch (failure)
	{
		case LocalFailure::None:
		case LocalFailure::InvalidConfiguration:
		case LocalFailure::InvalidState:
		case LocalFailure::InvalidMessage:
		case LocalFailure::CompatibilityMismatch:
		case LocalFailure::AdmissionRejected:
		case LocalFailure::WireFailure:
		case LocalFailure::ResyncRequired:
		case LocalFailure::IntentOutstanding:
		case LocalFailure::ActorNotAssigned:
		case LocalFailure::InvalidIntent:
		case LocalFailure::SequenceExhausted:
		case LocalFailure::AllocationFailure:
		case LocalFailure::CredentialStorageFailure:
		case LocalFailure::CredentialRetirementPending:
		case LocalFailure::SelfRetirementRejected:
		case LocalFailure::CredentialRetired:
			return true;
	}
	return false;
}

const char* LocalFailureText(LocalFailure failure) noexcept
{
	switch (failure)
	{
		case LocalFailure::None: return nullptr;
		case LocalFailure::InvalidConfiguration: return "invalid configuration";
		case LocalFailure::InvalidState: return "client not ready";
		case LocalFailure::InvalidMessage: return "invalid message";
		case LocalFailure::CompatibilityMismatch:
			return "compatibility mismatch";
		case LocalFailure::AdmissionRejected: return "admission rejected";
		case LocalFailure::WireFailure: return "network write failed";
		case LocalFailure::ResyncRequired: return "resync required";
		case LocalFailure::IntentOutstanding: return "command already pending";
		case LocalFailure::ActorNotAssigned: return "actor is not assigned";
		case LocalFailure::InvalidIntent: return "invalid command";
		case LocalFailure::SequenceExhausted:
			return "command sequence exhausted";
		case LocalFailure::AllocationFailure: return "allocation failure";
		case LocalFailure::CredentialStorageFailure:
			return "credential storage failed";
		case LocalFailure::CredentialRetirementPending:
			return "leave is committing";
		case LocalFailure::SelfRetirementRejected:
			return "server retirement capacity reached";
		case LocalFailure::CredentialRetired: return "left server";
	}
	return nullptr;
}

bool CurrentLocalFailure(
	const FullEngineCoopClientPresentationCommandFeedbackInput& input,
	bool& current) noexcept
{
	if (!KnownLocalFailure(input.localFailure)) return false;
	if (input.localFailure == LocalFailure::None)
	{
		if (input.localFailureWorldGeneration != 0) return false;
		current = false;
		return true;
	}
	if (input.localFailureWorldGeneration == 0 ||
		input.localFailureWorldGeneration > input.currentWorldGeneration)
		return false;
	current = input.localFailureWorldGeneration == input.currentWorldGeneration;
	return true;
}

void SetLocalFailure(
	const FullEngineCoopClientPresentationCommandFeedbackInput& input,
	FullEngineCoopClientPresentationCommandFeedback& output) noexcept
{
	output = {};
	output.state = FeedbackState::LocalFailure;
	output.localFailure = input.localFailure;
}

bool ValidReceipt(
	const CoopSession::CoopTacticalIntentReceipt& receipt) noexcept
{
	if (receipt.state.wireVersion != CoopSession::CoopTacticalWireVersion ||
		receipt.state.protocolVersion != CoopSession::CurrentProtocolVersion ||
		receipt.state.sessionEpoch == 0 ||
		receipt.state.worldGeneration == 0 || receipt.state.revision == 0 ||
		receipt.state.turnSerial == 0 || !NonzeroPeer(receipt.peerIdentity) ||
		receipt.commandId == 0 || !KnownStatus(receipt.status) ||
		!KnownReason(receipt.reason))
		return false;

	const std::uint64_t after = receipt.commandId ==
		(std::numeric_limits<std::uint64_t>::max)()
		? 0 : receipt.commandId + 1;
	const bool sequenceMismatch = receipt.status == ReceiptStatus::Rejected &&
		receipt.reason == ReceiptReason::InvalidCommandSequence;
	const bool inboxExhausted = receipt.status == ReceiptStatus::Rejected &&
		receipt.reason == ReceiptReason::InboxSequenceExhausted;
	if ((!sequenceMismatch && !inboxExhausted &&
			receipt.nextExpectedCommandId != after) ||
		(sequenceMismatch && receipt.nextExpectedCommandId == 0) ||
		(inboxExhausted && receipt.nextExpectedCommandId != 0))
		return false;

	switch (receipt.status)
	{
		case ReceiptStatus::Queued:
			return receipt.reason == ReceiptReason::None;
		case ReceiptStatus::Rejected:
			return receipt.authoritativeSequence == 0 &&
				receipt.reason != ReceiptReason::None;
		case ReceiptStatus::Applied:
			return receipt.reason == ReceiptReason::None;
		case ReceiptStatus::Discarded:
			return receipt.reason == ReceiptReason::AuthoritativeDiscard;
		case ReceiptStatus::Cancelled:
			return receipt.reason == ReceiptReason::SessionEnded;
	}
	return false;
}

FeedbackState StateForTerminal(ReceiptStatus status) noexcept
{
	switch (status)
	{
		case ReceiptStatus::Applied: return FeedbackState::Applied;
		case ReceiptStatus::Rejected: return FeedbackState::Rejected;
		case ReceiptStatus::Discarded: return FeedbackState::Discarded;
		case ReceiptStatus::Cancelled: return FeedbackState::Cancelled;
		case ReceiptStatus::Queued: break;
	}
	return FeedbackState::None;
}

const char* ReasonText(ReceiptReason reason) noexcept
{
	switch (reason)
	{
		case ReceiptReason::None: return "none";
		case ReceiptReason::MalformedIntent: return "malformed intent";
		case ReceiptReason::NotAdmitted: return "not admitted";
		case ReceiptReason::SessionMismatch: return "session mismatch";
		case ReceiptReason::WorldMismatch: return "world mismatch";
		case ReceiptReason::RevisionMismatch: return "state advanced; retry";
		case ReceiptReason::TurnMismatch: return "turn mismatch";
		case ReceiptReason::InvalidCommandSequence:
			return "command sequence mismatch";
		case ReceiptReason::ActorNotOwned: return "actor not owned";
		case ReceiptReason::NotBaselineReady: return "state catching up; retry";
		case ReceiptReason::ActorUnavailable: return "actor unavailable";
		case ReceiptReason::WrongTeam: return "wrong team";
		case ReceiptReason::GameplayRejected: return "gameplay rejected";
		case ReceiptReason::InboxCapacityReached: return "server inbox full";
		case ReceiptReason::InboxSequenceExhausted:
			return "server sequence exhausted";
		case ReceiptReason::AllocationFailure:
			return "server allocation failure";
		case ReceiptReason::QueueUnavailable: return "server queue unavailable";
		case ReceiptReason::UnavailableContext:
			return "server context unavailable";
		case ReceiptReason::AuthoritativeDiscard:
			return "authority discarded command";
		case ReceiptReason::SessionEnded: return "session ended";
		case ReceiptReason::AuthoritySequenceExhausted:
			return "authority sequence exhausted";
	}
	return nullptr;
}

const char* StateText(FeedbackState state) noexcept
{
	switch (state)
	{
		case FeedbackState::None: return "No command submitted";
		case FeedbackState::LocalFailure: return nullptr;
		case FeedbackState::Submitted: return " submitted";
		case FeedbackState::Accepted: return " accepted (queued)";
		case FeedbackState::Applied: return " applied";
		case FeedbackState::Rejected: return " rejected";
		case FeedbackState::Discarded: return " discarded";
		case FeedbackState::Cancelled: return " cancelled";
	}
	return nullptr;
}

struct TextWriter
{
	std::array<char,
		FullEngineCoopClientPresentationCommandFeedbackTextCapacity>& text;
	std::size_t length = 0;

	bool append(char character) noexcept
	{
		if (length + 1 >= text.size()) return false;
		text[length++] = character;
		text[length] = '\0';
		return true;
	}

	bool append(const char* source) noexcept
	{
		if (source == nullptr) return false;
		for (std::size_t index = 0; source[index] != '\0'; ++index)
			if (!append(source[index])) return false;
		return true;
	}

	bool appendUnsigned(std::uint64_t value) noexcept
	{
		char reversed[20]{};
		std::size_t count = 0;
		do
		{
			reversed[count++] = static_cast<char>('0' + value % 10);
			value /= 10;
		} while (value != 0 && count < sizeof(reversed));
		if (value != 0) return false;
		while (count != 0)
			if (!append(reversed[--count])) return false;
		return true;
	}
};

bool BuildText(
	const FullEngineCoopClientPresentationCommandFeedback& feedback,
	std::array<char,
		FullEngineCoopClientPresentationCommandFeedbackTextCapacity>& output,
	std::size_t& length) noexcept
{
	output = {};
	TextWriter writer{output};
	if (feedback.state == FeedbackState::LocalFailure)
	{
		const char* const failure = LocalFailureText(feedback.localFailure);
		if (feedback.commandId != 0 || feedback.hasReceipt || failure == nullptr ||
			!writer.append("Command not sent: ") || !writer.append(failure))
			return false;
		length = writer.length;
		return true;
	}
	if (feedback.state == FeedbackState::None)
	{
		if (!writer.append(StateText(feedback.state))) return false;
		length = writer.length;
		return true;
	}
	if (feedback.commandId == 0 || !writer.append("Command ") ||
		!writer.appendUnsigned(feedback.commandId) ||
		!writer.append(StateText(feedback.state)))
		return false;
	if (feedback.hasReceipt && feedback.reason != ReceiptReason::None)
	{
		const char* const reason = ReasonText(feedback.reason);
		if (reason == nullptr || !writer.append(" (") ||
			!writer.append(reason) || !writer.append(')'))
			return false;
	}
	length = writer.length;
	return true;
}
}

bool ClassifyFullEngineCoopClientPresentationCommandFeedback(
	const FullEngineCoopClientPresentationCommandFeedbackInput& input,
	FullEngineCoopClientPresentationCommandFeedback& output) noexcept
{
	if (input.currentWorldGeneration == 0) return false;
	bool currentLocalFailure = false;
	if (!CurrentLocalFailure(input, currentLocalFailure)) return false;
	FullEngineCoopClientPresentationCommandFeedback candidate;
	if (!input.hasReceipt)
	{
		if (input.outstandingCommandId != 0)
		{
			candidate.state = FeedbackState::Submitted;
			candidate.commandId = input.outstandingCommandId;
		}
		else if (currentLocalFailure)
			SetLocalFailure(input, candidate);
		output = candidate;
		return true;
	}
	if (!ValidReceipt(input.receipt) ||
		input.receipt.state.worldGeneration > input.currentWorldGeneration)
		return false;

	// Receipt history spans the admission epoch.  A new tactical world adopts
	// its own command state, so an otherwise valid old-world replay is not a
	// current-world status line.
	if (input.receipt.state.worldGeneration < input.currentWorldGeneration)
	{
		if (input.outstandingCommandId != 0)
		{
			candidate.state = FeedbackState::Submitted;
			candidate.commandId = input.outstandingCommandId;
		}
		else if (currentLocalFailure)
			SetLocalFailure(input, candidate);
		output = candidate;
		return true;
	}

	if (input.outstandingCommandId != 0)
	{
		if (input.receipt.commandId > input.outstandingCommandId)
			return false;
		candidate.state = FeedbackState::Submitted;
		candidate.commandId = input.outstandingCommandId;
		if (input.receipt.commandId < input.outstandingCommandId)
		{
			output = candidate;
			return true;
		}
		if (input.receipt.status != ReceiptStatus::Queued)
			return false;
		candidate.state = FeedbackState::Accepted;
		candidate.hasReceipt = true;
		candidate.status = input.receipt.status;
		candidate.reason = input.receipt.reason;
		output = candidate;
		return true;
	}

	if (input.receipt.status == ReceiptStatus::Queued) return false;
	if (currentLocalFailure)
	{
		SetLocalFailure(input, candidate);
		output = candidate;
		return true;
	}
	candidate.state = StateForTerminal(input.receipt.status);
	if (candidate.state == FeedbackState::None) return false;
	candidate.commandId = input.receipt.commandId;
	candidate.hasReceipt = true;
	candidate.status = input.receipt.status;
	candidate.reason = input.receipt.reason;
	output = candidate;
	return true;
}

bool BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
	const FullEngineCoopClientPresentationCommandFeedbackInput& input,
	FullEngineCoopClientPresentationCommandFeedbackTextPlan& output) noexcept
{
	FullEngineCoopClientPresentationCommandFeedbackTextPlan candidate;
	if (!ClassifyFullEngineCoopClientPresentationCommandFeedback(
			input, candidate.feedback) ||
		!BuildText(candidate.feedback, candidate.ascii, candidate.textLength))
		return false;
	for (std::size_t index = 0; index < candidate.textLength; ++index)
		candidate.utf16[index] =
			static_cast<std::uint8_t>(candidate.ascii[index]);
	candidate.utf16[candidate.textLength] = 0;
	output = candidate;
	return true;
}
