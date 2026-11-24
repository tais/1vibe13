#include "DedicatedCoopCampaignAimQuotes.h"
#include "Soldier Profile Constants.h"
#include "DedicatedCoopCampaignEconomy.h"
#include "CampaignAimHire.h"
#include "CampaignAimSitePolicy.h"
#include "CampaignAimWillingness.h"
#include "CampaignLedger.h"
#include "GameContext.h"
#include "Soldier Profile.h"

namespace
{
CoopSession::CoopCampaignAimQuoteStatus QuoteStatus(CampaignAimHireError error) noexcept
{
	using Status = CoopSession::CoopCampaignAimQuoteStatus;
	switch (error)
	{
		case CampaignAimHireError::None: return Status::Available;
		case CampaignAimHireError::Unavailable: return Status::Unavailable;
		case CampaignAimHireError::DuplicateProfile: return Status::AlreadyHired;
		case CampaignAimHireError::CapacityReached: return Status::TeamFull;
		default: return Status::Unsupported;
	}
}
}

const char* CaptureDedicatedCoopCampaignAimQuotes(
	const CoopSession::CoopCampaignEconomy& economy,
	CoopSession::CoopCampaignAimQuotes& output) noexcept
{
	using namespace CoopSession;
	if (!ValidCoopCampaignEconomy(economy) || !economy.available)
		return "campaign economy unavailable for quotes";
	CoopCampaignEconomy freshEconomy;
	if (CaptureDedicatedCoopCampaignEconomy(freshEconomy) ||
		!SameCoopCampaignEconomyContent(economy, freshEconomy))
		return "campaign economy changed before quotes";
	CampaignAimHireArrival arrival;
	if (ReadCampaignAimHireArrival(arrival) != CampaignAimHireError::None)
		return "AIM arrival context unavailable";
	CampaignLedgerSnapshot finance, depositFinance, history;
	if (PrepareFinanceLedgerAppend(1, finance) != CampaignLedgerError::None || finance.balance != economy.balance)
		return "finance ledger cannot accept a hire";
	if (PrepareHistoryLedgerAppend(1, history) != CampaignLedgerError::None)
		return "history ledger cannot accept a hire";
	const bool canRecordDeposit = PrepareFinanceLedgerAppend(2, depositFinance) == CampaignLedgerError::None &&
		depositFinance.balance == economy.balance;
	CoopCampaignAimQuotes captured;
	captured.available = true;
	captured.arrivalMinutes = arrival.arrivalMinute;
	captured.landingX = arrival.landingX; captured.landingY = arrival.landingY;
	const CampaignAimSitePolicy pricing(GetGameContext().capabilities());
	constexpr std::array<std::uint32_t, 3> days{1, 7, 14};
	for (std::uint32_t profileId = 0; profileId < NUM_PROFILES; ++profileId)
	{
		if (gMercProfiles[profileId].Type != PROFILETYPE_AIM) continue;
		if (captured.quoteCount == captured.quotes.size()) return "AIM quote capacity exceeded";
		auto& quote = captured.quotes[captured.quoteCount++];
		quote.profile = static_cast<std::uint16_t>(profileId);
		// The native sentinel is an in-range array slot, not a hireable profile.
		// John's missed-flight lifecycle needs its own checked continuation.
		if (profileId == NO_PROFILE || profileId == JOHN_MERC)
		{ quote.status = CoopCampaignAimQuoteStatus::Unsupported; continue; }
		bool alreadyHired = false;
		for (std::size_t i = 0; i < economy.rosterCount; ++i)
			if (economy.roster[i].profile == profileId) alreadyHired = true;
		if (alreadyHired) { quote.status = CoopCampaignAimQuoteStatus::AlreadyHired; continue; }
		if (gMercProfiles[profileId].bMedicalDeposit && !canRecordDeposit) continue;
		CampaignAimHirePlan plan;
		const auto eligibility = PrepareCampaignAimHire({profileId, 1, false}, plan);
		quote.status = QuoteStatus(eligibility);
		if (eligibility != CampaignAimHireError::None) continue;
		CampaignAimWillingnessDecision willingness;
		if (!ReadCampaignAimWillingness(profileId, willingness)) return "invalid AIM willingness profile";
		if (!willingness.accepted())
		{
			quote.status = CoopCampaignAimQuoteStatus::Unwilling;
			quote.willingnessReason = static_cast<std::uint8_t>(willingness.reason);
			continue;
		}
		const auto& profile = gMercProfiles[profileId];
		CoopCampaignAimQuote offered;
		offered.profile = quote.profile; offered.status = CoopCampaignAimQuoteStatus::Available;
		offered.willingnessReason = static_cast<std::uint8_t>(willingness.reason);
		// Offer only the original, unpurchased kit. A previously purchased kit
		// loses its price in native state; offering it again needs a separate
		// authoritative kit-selection/repricing flow.
		offered.gearAvailable = !(profile.ubMiscFlags & PROFILE_MISC_FLAG_ALREADY_USED_ITEMS) &&
			PrepareCampaignAimHire({profileId, 1, true}, plan) == CampaignAimHireError::None;
		bool valid = profile.sSalary >= 0;
		for (std::size_t length = 0; valid && length < days.size(); ++length)
		{
			// Equipment does not change flight or contract timing. Its private
			// distribution was checked once above; do not rebuild it for each price.
			if (PrepareCampaignAimHire({profileId, days[length], false}, plan) != CampaignAimHireError::None)
			{ valid = false; break; }
			if (plan.arrivalMinute != arrival.arrivalMinute || plan.landingX != arrival.landingX || plan.landingY != arrival.landingY)
				return "AIM arrival context changed during quotes";
			for (unsigned gear = 0; valid && gear < 2; ++gear)
			{
				if (gear && !offered.gearAvailable) continue;
				CampaignAimSitePolicy::ContractQuote price;
				if (pricing.quoteContract(static_cast<std::uint32_t>(profile.sSalary), profile.uiWeeklySalary,
					profile.uiBiWeeklySalary, profile.sMedicalDepositAmount, profile.usOptionalGearCost,
					static_cast<std::uint8_t>(length), profile.bMedicalDeposit != 0, gear != 0, price) !=
					CampaignAimSitePolicy::ContractQuoteError::None)
				{ valid = false; break; }
				offered.salary[length] = static_cast<std::int32_t>(price.salary);
				offered.medicalDeposit = static_cast<std::int32_t>(price.medicalDeposit);
				if (gear) offered.gearCost = static_cast<std::int32_t>(price.equipmentCost);
				offered.total[length * 2 + gear] = static_cast<std::int32_t>(price.total);
			}
		}
		if (!valid) quote.status = CoopCampaignAimQuoteStatus::Unsupported;
		else quote = offered;
	}
	captured.sessionEpoch = economy.sessionEpoch;
	captured.revision = 1; captured.economyRevision = economy.revision;
	if (!ValidCoopCampaignAimQuotes(captured)) return "invalid AIM quote projection";
	captured.sessionEpoch = 0; captured.revision = 0; captured.economyRevision = 0;
	output = captured;
	return nullptr;
}
