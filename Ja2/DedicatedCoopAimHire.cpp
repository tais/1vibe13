#include "DedicatedCoopAimHire.h"
#include "DedicatedCoopCampaignEconomy.h"
#include "DedicatedCoopCampaignAimQuotes.h"
#include "CampaignAimHire.h"
#include "CampaignLedger.h"
#include "TacticalEntityHost.h"
#include "TacticalActor.h"
#include "Soldier Profile.h"
#include "Game Clock.h"
#include "Assignments.h"
#include "LaptopSave.h"
#include "finances.h"
#include "history.h"

DedicatedCoopAimHireResult HireDedicatedCoopAimMerc(
	const CoopSession::CoopCampaignHireRequest& request,
	const CoopSession::CoopCampaignEconomy& economy,
	const CoopSession::CoopCampaignAimQuotes& quotes) noexcept
{
	using namespace CoopSession;
	using Code = DedicatedCoopAimHireCode;
	DedicatedCoopAimHireResult result;
	const auto reject = [&](Code code, std::uint8_t detail = 0) {
		result.code = code; result.nativeDetail = detail;
		// Actor/charge are evidence only for an entirely successful hire.
		result.actor = {}; result.chargedTotal = 0;
		return result;
	};
	if (!ValidCoopCampaignHireRequest(request) || !ValidCoopCampaignEconomy(economy) ||
		!ValidCoopCampaignAimQuotes(quotes) || !economy.available || !quotes.available ||
		request.sessionEpoch != economy.sessionEpoch || request.sessionEpoch != quotes.sessionEpoch ||
		request.economyRevision != economy.revision || request.quoteRevision != quotes.revision ||
		quotes.economyRevision != economy.revision)
		return reject(Code::InvalidRequest);
	try
	{
		CoopCampaignEconomy freshEconomy;
		if (CaptureDedicatedCoopCampaignEconomy(freshEconomy)) return reject(Code::EconomyUnavailable);
		if (!SameCoopCampaignEconomyContent(economy, freshEconomy)) return reject(Code::EconomyChanged);
		freshEconomy.sessionEpoch = economy.sessionEpoch; freshEconomy.revision = economy.revision;
		CoopCampaignAimQuotes freshQuotes;
		if (CaptureDedicatedCoopCampaignAimQuotes(freshEconomy, freshQuotes)) return reject(Code::QuotesUnavailable);
		if (!SameCoopCampaignAimQuotesContent(quotes, freshQuotes)) return reject(Code::QuotesChanged);
		const auto* quote = FindCoopCampaignAimQuote(quotes, request.profile);
		if (!quote || quote->status != CoopCampaignAimQuoteStatus::Available ||
			(request.buyGear && !quote->gearAvailable)) return reject(Code::QuoteUnavailable);
		const auto choice = CoopCampaignHireChoiceIndex(request.days, request.buyGear);
		const auto total = quote->total[choice];
		if (economy.balance < total) return reject(Code::InsufficientFunds);
		const auto profileId = static_cast<std::uint8_t>(request.profile);
		const bool depositRecord = gMercProfiles[profileId].bMedicalDeposit != 0;
		CampaignLedgerSnapshot financeBefore, historyBefore;
		const auto financeError = PrepareFinanceLedgerAppend(depositRecord ? 2 : 1, financeBefore);
		if (financeError != CampaignLedgerError::None || financeBefore.balance != economy.balance)
			return reject(Code::EconomyUnavailable, static_cast<std::uint8_t>(financeError));
		const auto historyError = PrepareHistoryLedgerAppend(1, historyBefore);
		if (historyError != CampaignLedgerError::None)
			return reject(Code::HistoryUnavailable, static_cast<std::uint8_t>(historyError));
		const auto date = GetWorldTotalMin();
		const auto hired = HireAimMercChecked({request.profile, request.days, request.buyGear});
		result.mutationMayHaveStarted = hired.mutationMayHaveStarted;
		if (!hired) return reject(hired.mutationMayHaveStarted ? Code::HireFailed : Code::HireRejected,
			static_cast<std::uint8_t>(hired.error));
		// Success itself means native actor/event mutation occurred, regardless
		// of a future change to the lower constructor's diagnostic flag.
		result.mutationMayHaveStarted = true;
		auto* actor = ResolveJa2TacticalEntity(hired.actor);
		if (!actor || actor->identity().profile() != profileId || actor->assignment().current() != IN_TRANSIT ||
			actor->deployment().arrivalTime() != quotes.arrivalMinutes)
			return reject(Code::PostconditionFailed);
		// The medical deposit is a separate native ledger row. Charge the
		// actually quoted deposit only; a profile's inactive deposit field must
		// never reduce its salary/equipment payment.
		const auto hireCharge = total - quote->medicalDeposit;
		const auto charge = AddTransactionToPlayersBookChecked(HIRED_MERC, profileId, date, -hireCharge);
		if (!charge.succeeded()) return reject(Code::FinanceFailed, static_cast<std::uint8_t>(charge.error));
		if (depositRecord)
		{
			const auto deposit = AddTransactionToPlayersBookChecked(MEDICAL_DEPOSIT, profileId, date, -quote->medicalDeposit);
			if (!deposit.succeeded()) return reject(Code::FinanceFailed, static_cast<std::uint8_t>(deposit.error));
		}
		const auto history = AddHistoryToPlayersLogChecked(HISTORY_HIRED_MERC_FROM_AIM, profileId, date, -1, -1);
		if (!history.succeeded()) return reject(Code::HistoryFailed, static_cast<std::uint8_t>(history.error));
		if (request.buyGear)
		{
			gMercProfiles[profileId].usOptionalGearCost = 0;
			gMercProfiles[profileId].ubMiscFlags |= PROFILE_MISC_FLAG_ALREADY_USED_ITEMS;
		}
		CampaignLedgerSnapshot financeAfter, historyAfter;
		if (InspectFinanceLedger(financeAfter) != CampaignLedgerError::None ||
			InspectHistoryLedger(historyAfter) != CampaignLedgerError::None ||
			financeAfter.balance != economy.balance - total || LaptopSaveInfo.iCurrentBalance != financeAfter.balance ||
			static_cast<std::uint64_t>(financeAfter.recordCount) != static_cast<std::uint64_t>(financeBefore.recordCount) + 1 + depositRecord ||
			static_cast<std::uint64_t>(historyAfter.recordCount) != static_cast<std::uint64_t>(historyBefore.recordCount) + 1 ||
			ResolveJa2TacticalEntity(hired.actor) != actor)
			return reject(Code::PostconditionFailed);
		result.code = Code::Applied; result.actor = hired.actor; result.chargedTotal = total;
		return result;
	}
	catch (...) { return reject(Code::NativeFailure); }
}
