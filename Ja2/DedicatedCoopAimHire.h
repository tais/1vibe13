#ifndef JA2_DEDICATED_COOP_AIM_HIRE_H
#define JA2_DEDICATED_COOP_AIM_HIRE_H

#include "CoopCampaignHire.h"

enum class DedicatedCoopAimHireCode : std::uint8_t
{
	Applied, InvalidRequest, EconomyUnavailable, EconomyChanged,
	QuotesUnavailable, QuotesChanged, QuoteUnavailable, InsufficientFunds,
	HistoryUnavailable, HireRejected, HireFailed, FinanceFailed,
	HistoryFailed, PostconditionFailed, NativeFailure
};
struct DedicatedCoopAimHireResult
{
	DedicatedCoopAimHireCode code = DedicatedCoopAimHireCode::NativeFailure;
	bool mutationMayHaveStarted = false;
	TacticalEntityId actor{};
	std::int32_t chargedTotal = 0;
	std::uint8_t nativeDetail = 0;
};

// Campaign-thread, one-shot native operation after the session authority has
// consumed the shared control revision. Revalidates the observed economy and
// offers, constructs one delayed hire, then writes native finance/history.
// Any failure with mutationMayHaveStarted requires immediate fail-stop. No
// rollback or retry is safe; session receipts provide duplicate suppression.
DedicatedCoopAimHireResult HireDedicatedCoopAimMerc(
	const CoopSession::CoopCampaignHireRequest& request,
	const CoopSession::CoopCampaignEconomy& economy,
	const CoopSession::CoopCampaignAimQuotes& quotes) noexcept;

#endif
