#include "Ja2/FullEngineCoopClientPresentationCommandFeedback.h"

#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

using Feedback = FullEngineCoopClientPresentationCommandFeedback;
using FeedbackInput = FullEngineCoopClientPresentationCommandFeedbackInput;
using FeedbackState =
	FullEngineCoopClientPresentationCommandFeedbackState;
using TextPlan =
	FullEngineCoopClientPresentationCommandFeedbackTextPlan;
using ReceiptStatus = CoopSession::CoopTacticalIntentReceiptStatus;
using ReceiptReason = CoopSession::CoopTacticalIntentReceiptReason;
using LocalFailure = FullEngineCoopClientPresentationLocalFailure;

CoopSession::CoopTacticalIntentReceipt Receipt(
	std::uint64_t worldGeneration, std::uint64_t commandId,
	ReceiptStatus status, ReceiptReason reason)
{
	CoopSession::CoopTacticalIntentReceipt receipt;
	receipt.state.sessionEpoch = 71;
	receipt.state.worldGeneration = worldGeneration;
	receipt.state.revision = 9;
	receipt.state.turnSerial = 3;
	receipt.peerIdentity[0] = 0x41;
	receipt.commandId = commandId;
	receipt.nextExpectedCommandId = commandId ==
		(std::numeric_limits<std::uint64_t>::max)()
		? 0 : commandId + 1;
	receipt.status = status;
	receipt.reason = reason;
	receipt.authoritativeSequence = status == ReceiptStatus::Rejected
		? 0 : 100 + commandId;
	receipt.simulationTick = 700;
	return receipt;
}

bool TextEquals(const TextPlan& plan, const char* expected)
{
	const std::size_t length = std::strlen(expected);
	if (plan.textLength != length || plan.ascii[length] != '\0' ||
		plan.utf16[length] != 0)
		return false;
	for (std::size_t index = 0; index < length; ++index)
		if (plan.ascii[index] != expected[index] ||
			plan.utf16[index] != static_cast<std::uint8_t>(expected[index]))
			return false;
	return true;
}

void TestNoneAndSubmitted()
{
	TextPlan plan;
	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, false, {}}, plan) &&
		plan.feedback.state == FeedbackState::None &&
		plan.feedback.commandId == 0 && !plan.feedback.hasReceipt &&
		TextEquals(plan, "No command submitted"),
		"an idle current world produces one bounded neutral line");

	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 42, false, {}}, plan) &&
		plan.feedback.state == FeedbackState::Submitted &&
		plan.feedback.commandId == 42 && !plan.feedback.hasReceipt &&
		TextEquals(plan, "Command 42 submitted"),
		"the one-command lock immediately becomes submitted feedback");
}

void TestMatchingQueuedIsAccepted()
{
	const auto queued = Receipt(
		12, 42, ReceiptStatus::Queued, ReceiptReason::None);
	TextPlan plan;
	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 42, true, queued}, plan) &&
		plan.feedback.state == FeedbackState::Accepted &&
		plan.feedback.commandId == 42 && plan.feedback.hasReceipt &&
		plan.feedback.status == ReceiptStatus::Queued &&
		plan.feedback.reason == ReceiptReason::None &&
		TextEquals(plan, "Command 42 accepted (queued)"),
		"a matching Queued receipt means accepted for processing, not applied");
}

void TestTerminalReceipts()
{
	struct Case
	{
		ReceiptStatus status;
		ReceiptReason reason;
		FeedbackState state;
		const char* text;
	};
	const Case cases[]{
		{ReceiptStatus::Applied, ReceiptReason::None,
			FeedbackState::Applied, "Command 42 applied"},
		{ReceiptStatus::Rejected, ReceiptReason::ActorUnavailable,
			FeedbackState::Rejected,
			"Command 42 rejected (actor unavailable)"},
		{ReceiptStatus::Rejected, ReceiptReason::RevisionMismatch,
			FeedbackState::Rejected,
			"Command 42 rejected (state advanced; retry)"},
		{ReceiptStatus::Rejected, ReceiptReason::NotBaselineReady,
			FeedbackState::Rejected,
			"Command 42 rejected (state catching up; retry)"},
		{ReceiptStatus::Discarded, ReceiptReason::AuthoritativeDiscard,
			FeedbackState::Discarded,
			"Command 42 discarded (authority discarded command)"},
		{ReceiptStatus::Cancelled, ReceiptReason::SessionEnded,
			FeedbackState::Cancelled,
			"Command 42 cancelled (session ended)"}
	};
	for (const Case& expected : cases)
	{
		const auto receipt = Receipt(
			12, 42, expected.status, expected.reason);
		TextPlan plan;
		CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
				FeedbackInput{12, 0, true, receipt}, plan) &&
			plan.feedback.state == expected.state &&
			plan.feedback.commandId == 42 && plan.feedback.hasReceipt &&
			plan.feedback.status == expected.status &&
			plan.feedback.reason == expected.reason &&
			TextEquals(plan, expected.text),
			"each authoritative terminal receipt retains exact status and reason");
	}
}

void TestStaleReceiptAndOldWorldSuppression()
{
	const auto priorCommand = Receipt(
		12, 41, ReceiptStatus::Applied, ReceiptReason::None);
	TextPlan plan;
	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 42, true, priorCommand}, plan) &&
		plan.feedback.state == FeedbackState::Submitted &&
		plan.feedback.commandId == 42 && !plan.feedback.hasReceipt &&
		TextEquals(plan, "Command 42 submitted"),
		"a stale terminal receipt cannot label the newer outstanding command");

	const auto priorWorld = Receipt(
		11, 99, ReceiptStatus::Rejected, ReceiptReason::GameplayRejected);
	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, true, priorWorld}, plan) &&
		plan.feedback.state == FeedbackState::None &&
		TextEquals(plan, "No command submitted"),
		"receipt history from a retired world is suppressed while idle");
	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 42, true, priorWorld}, plan) &&
		plan.feedback.state == FeedbackState::Submitted &&
		plan.feedback.commandId == 42 &&
		TextEquals(plan, "Command 42 submitted"),
		"an old-world receipt cannot mask a new-world submission");
}

void TestCurrentWorldLocalFailureAndAuthorityPrecedence()
{
	TextPlan plan;
	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, false, {}, 12,
				LocalFailure::WireFailure}, plan) &&
		plan.feedback.state == FeedbackState::LocalFailure &&
		plan.feedback.commandId == 0 && !plan.feedback.hasReceipt &&
		plan.feedback.localFailure == LocalFailure::WireFailure &&
		TextEquals(plan, "Command not sent: network write failed"),
		"a current-world local send failure is distinct from an authority receipt");

	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, false, {}, 11,
				LocalFailure::ActorNotAssigned}, plan) &&
		plan.feedback.state == FeedbackState::None &&
		TextEquals(plan, "No command submitted"),
		"a local send failure from an older tactical world is suppressed");

	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 42, false, {}, 12,
				LocalFailure::IntentOutstanding}, plan) &&
		plan.feedback.state == FeedbackState::Submitted &&
		plan.feedback.commandId == 42 &&
		TextEquals(plan, "Command 42 submitted"),
		"the causal outstanding lock takes precedence over a local failure");

	const auto queued = Receipt(
		12, 42, ReceiptStatus::Queued, ReceiptReason::None);
	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 42, true, queued, 12,
				LocalFailure::IntentOutstanding}, plan) &&
		plan.feedback.state == FeedbackState::Accepted &&
		TextEquals(plan, "Command 42 accepted (queued)"),
		"an authoritative queued receipt also takes precedence over local failure");

	const auto applied = Receipt(
		12, 41, ReceiptStatus::Applied, ReceiptReason::None);
	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, true, applied, 12,
				LocalFailure::InvalidIntent}, plan) &&
		plan.feedback.state == FeedbackState::LocalFailure &&
		TextEquals(plan, "Command not sent: invalid command"),
		"a current local failure supersedes an older terminal receipt");
}

void TestMaximumCommandIdFormatting()
{
	const std::uint64_t maximum =
		(std::numeric_limits<std::uint64_t>::max)();
	TextPlan plan;
	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, maximum, false, {}}, plan) &&
		plan.feedback.commandId == maximum &&
		TextEquals(plan,
			"Command 18446744073709551615 submitted") &&
		plan.textLength + 1 <=
			FullEngineCoopClientPresentationCommandFeedbackTextCapacity,
		"UINT64_MAX formats exactly without allocation or truncation");

	const auto queued = Receipt(
		12, maximum, ReceiptStatus::Queued, ReceiptReason::None);
	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, maximum, true, queued}, plan) &&
		TextEquals(plan,
			"Command 18446744073709551615 accepted (queued)"),
		"the exhausted-sentinel cursor remains valid for a maximum-ID receipt");

	const auto exhausted = Receipt(12, maximum, ReceiptStatus::Rejected,
		ReceiptReason::AuthoritySequenceExhausted);
	CHECK(BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, true, exhausted}, plan) &&
		TextEquals(plan,
			"Command 18446744073709551615 rejected (authority sequence exhausted)") &&
		plan.textLength + 1 <=
			FullEngineCoopClientPresentationCommandFeedbackTextCapacity,
		"the longest useful terminal line remains exactly terminated in capacity");
}

void TestInvalidInputsPreserveOutput()
{
	TextPlan retained;
	retained.feedback.state = FeedbackState::Discarded;
	retained.feedback.commandId = 777;
	retained.feedback.hasReceipt = true;
	retained.feedback.status = ReceiptStatus::Discarded;
	retained.feedback.reason = ReceiptReason::AuthoritativeDiscard;
	retained.ascii[0] = 'X';
	retained.utf16[0] = static_cast<std::uint16_t>('X');
	retained.textLength = 1;
	const TextPlan original = retained;

	CHECK(!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{}, retained) && retained == original,
		"zero current world generation fails transactionally");

	auto malformed = Receipt(
		12, 42, ReceiptStatus::Queued, ReceiptReason::GameplayRejected);
	CHECK(!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 42, true, malformed}, retained) &&
		retained == original,
		"an invalid status/reason pair fails transactionally");

	malformed = Receipt(
		12, 42, ReceiptStatus::Applied, ReceiptReason::None);
	malformed.nextExpectedCommandId = 99;
	CHECK(!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, true, malformed}, retained) &&
		retained == original,
		"a malformed consuming cursor fails transactionally");

	malformed = Receipt(
		12, 42, ReceiptStatus::Applied, ReceiptReason::None);
	malformed.peerIdentity.fill(0);
	CHECK(!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, true, malformed}, retained) &&
		retained == original,
		"an unauthenticated receipt shape fails transactionally");

	const auto terminal = Receipt(
		12, 42, ReceiptStatus::Applied, ReceiptReason::None);
	CHECK(!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 42, true, terminal}, retained) &&
		retained == original,
		"a terminal receipt cannot coexist with the same outstanding lock");

	const auto futureCommand = Receipt(
		12, 43, ReceiptStatus::Queued, ReceiptReason::None);
	CHECK(!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 42, true, futureCommand}, retained) &&
		retained == original,
		"a receipt ahead of the outstanding command fails closed");

	const auto futureWorld = Receipt(
		13, 42, ReceiptStatus::Applied, ReceiptReason::None);
	CHECK(!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, true, futureWorld}, retained) &&
		retained == original,
		"a future-world receipt is malformed rather than historical");

	const auto orphanedQueued = Receipt(
		12, 42, ReceiptStatus::Queued, ReceiptReason::None);
	CHECK(!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, true, orphanedQueued}, retained) &&
		retained == original,
		"Queued without its outstanding causal lock fails closed");

	CHECK(!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, false, {}, 13,
				LocalFailure::WireFailure}, retained) &&
		retained == original,
		"a future-world local failure fails transactionally");
	CHECK(!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, false, {}, 12, LocalFailure::None},
			retained) && retained == original,
		"a nonzero stamp without a local failure fails transactionally");
	CHECK(!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			FeedbackInput{12, 0, false, {}, 12,
				static_cast<LocalFailure>(255)}, retained) &&
		retained == original,
		"an unknown local failure value fails transactionally");

	Feedback classified;
	classified.state = FeedbackState::Cancelled;
	classified.commandId = 88;
	const Feedback classifiedOriginal = classified;
	CHECK(!ClassifyFullEngineCoopClientPresentationCommandFeedback(
			FeedbackInput{12, 0, true, orphanedQueued}, classified) &&
		classified == classifiedOriginal,
		"the standalone classifier is independently transactional");
}
}

int main()
{
	TestNoneAndSubmitted();
	TestMatchingQueuedIsAccepted();
	TestTerminalReceipts();
	TestStaleReceiptAndOldWorldSuppression();
	TestCurrentWorldLocalFailureAndAuthorityPrecedence();
	TestMaximumCommandIdFormatting();
	TestInvalidInputsPreserveOutput();
	if (failures == 0)
		std::printf("full-engine co-op command-feedback tests passed\n");
	return failures == 0 ? 0 : 1;
}
