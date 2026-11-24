#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_COMMAND_FEEDBACK_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_COMMAND_FEEDBACK_H

#include <Multiplayer/CoopTacticalProtocol.h>

#include <array>
#include <cstddef>
#include <cstdint>

// Presentation-only projection of the client's one-command causal lock and
// latest authoritative receipt.  It owns no pointers and grants no gameplay
// authority: Queued means only that the server accepted the command for
// processing, while Applied is the successful terminal result.
enum class FullEngineCoopClientPresentationCommandFeedbackState : std::uint8_t
{
	None,
	LocalFailure,
	Submitted,
	Accepted,
	Applied,
	Rejected,
	Discarded,
	Cancelled
};

// Closed presentation vocabulary for a request rejected before an
// authoritative receipt exists.  The screen maps the full client result at
// its ownership boundary; this pure model deliberately does not depend on the
// client runtime or represent these failures as server decisions.
enum class FullEngineCoopClientPresentationLocalFailure : std::uint8_t
{
	None,
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

struct FullEngineCoopClientPresentationCommandFeedbackInput
{
	std::uint64_t currentWorldGeneration = 0;
	std::uint64_t outstandingCommandId = 0;
	bool hasReceipt = false;
	CoopSession::CoopTacticalIntentReceipt receipt{};
	std::uint64_t localFailureWorldGeneration = 0;
	FullEngineCoopClientPresentationLocalFailure localFailure =
		FullEngineCoopClientPresentationLocalFailure::None;
};

struct FullEngineCoopClientPresentationCommandFeedback
{
	FullEngineCoopClientPresentationCommandFeedbackState state =
		FullEngineCoopClientPresentationCommandFeedbackState::None;
	std::uint64_t commandId = 0;
	// False for None, LocalFailure, and Submitted. In those states status is a
	// deliberately non-semantic default and consumers must not inspect it.
	bool hasReceipt = false;
	CoopSession::CoopTacticalIntentReceiptStatus status =
		CoopSession::CoopTacticalIntentReceiptStatus::Rejected;
	CoopSession::CoopTacticalIntentReceiptReason reason =
		CoopSession::CoopTacticalIntentReceiptReason::None;
	FullEngineCoopClientPresentationLocalFailure localFailure =
		FullEngineCoopClientPresentationLocalFailure::None;

	bool operator==(
		const FullEngineCoopClientPresentationCommandFeedback& other)
		const noexcept
	{
		return state == other.state && commandId == other.commandId &&
			hasReceipt == other.hasReceipt && status == other.status &&
			reason == other.reason && localFailure == other.localFailure;
	}

	bool operator!=(
		const FullEngineCoopClientPresentationCommandFeedback& other)
		const noexcept
	{
		return !(*this == other);
	}
};

// Classifies one already core-validated presentation view transactionally.
// A receipt from an older world is historical and suppressed.  A future-world
// receipt, a Queued receipt without its matching outstanding command, or a
// terminal receipt that still claims to be outstanding violates the client
// lifecycle and is rejected without changing output.
bool ClassifyFullEngineCoopClientPresentationCommandFeedback(
	const FullEngineCoopClientPresentationCommandFeedbackInput& input,
	FullEngineCoopClientPresentationCommandFeedback& output) noexcept;

// Includes the trailing NUL in both fixed-capacity representations.  ASCII is
// the canonical source; UTF-16 mirrors it code-unit for code-unit so the native
// bitmap-font renderer does not perform locale-dependent conversion.
inline constexpr std::size_t
	FullEngineCoopClientPresentationCommandFeedbackTextCapacity = 96;

struct FullEngineCoopClientPresentationCommandFeedbackTextPlan
{
	FullEngineCoopClientPresentationCommandFeedback feedback{};
	std::array<char,
		FullEngineCoopClientPresentationCommandFeedbackTextCapacity> ascii{};
	std::array<std::uint16_t,
		FullEngineCoopClientPresentationCommandFeedbackTextCapacity> utf16{};
	std::size_t textLength = 0;

	bool operator==(
		const FullEngineCoopClientPresentationCommandFeedbackTextPlan& other)
		const noexcept
	{
		return feedback == other.feedback && ascii == other.ascii &&
			utf16 == other.utf16 && textLength == other.textLength;
	}

	bool operator!=(
		const FullEngineCoopClientPresentationCommandFeedbackTextPlan& other)
		const noexcept
	{
		return !(*this == other);
	}
};

// Builds a complete, NUL-terminated human-readable line without allocation.
// Failure preserves the caller's previous output.
bool BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
	const FullEngineCoopClientPresentationCommandFeedbackInput& input,
	FullEngineCoopClientPresentationCommandFeedbackTextPlan& output) noexcept;

#endif
