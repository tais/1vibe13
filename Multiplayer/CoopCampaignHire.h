#ifndef MULTIPLAYER_COOP_CAMPAIGN_HIRE_H
#define MULTIPLAYER_COOP_CAMPAIGN_HIRE_H

#include "CoopCampaignEconomy.h"
#include "CoopCampaignStatus.h"

namespace CoopSession
{
inline constexpr const char* CoopCampaignHireRequestMessageName = "coop.campaign.hire.request";
inline constexpr const char* CoopCampaignHireResultMessageName = "coop.campaign.hire.result";
inline constexpr std::size_t CoopCampaignHireRequestWireSize = 56;
inline constexpr std::size_t CoopCampaignHireResultWireSize = 96;
enum class CoopCampaignHireOutcome : std::uint8_t
{
	Applied = 1, NotReady = 2, Unauthorized = 3, Stale = 4, StaleQuote = 5,
	Unavailable = 6, AlreadyHired = 7, InsufficientFunds = 8,
	NativeRejected = 9, Unsupported = 10, Failed = 11
};
struct CoopCampaignHireRequest
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, economyRevision = 0, quoteRevision = 0, requestId = 0;
	std::uint16_t profile = 0;
	std::uint8_t days = 0;
	bool buyGear = false;
};
struct CoopCampaignHireResult
{
	CoopCampaignHireRequest request;
	// Economic/quote stamps identify the eligibility observations, not a saved
	// or committed post-hire state. The host publishes fresh full observations
	// before delivery; an Applied client also checks actor/profile in its roster.
	// Retained historical receipts never change when later observations advance.
	std::uint64_t controlRevision = 0, economyRevision = 0, quoteRevision = 0;
	TacticalEntityId actor{};
	std::int32_t chargedTotal = 0;
	CoopCampaignHireOutcome outcome = CoopCampaignHireOutcome::Unavailable;
	std::uint16_t nativeDetail = 0;
	// Explicitly distinguishes a preflight rejection from a native attempt that
	// consumed the shared control barrier, including rejected/failed attempts.
	bool nativeAttempted = false;
};
struct CoopCampaignHireNativeResult
{
	CoopCampaignHireOutcome outcome = CoopCampaignHireOutcome::Failed;
	TacticalEntityId actor{};
	std::int32_t chargedTotal = 0;
	std::uint16_t nativeDetail = 0;
};
using CoopCampaignHireRequestBytes = std::array<std::uint8_t, CoopCampaignHireRequestWireSize>;
using CoopCampaignHireResultBytes = std::array<std::uint8_t, CoopCampaignHireResultWireSize>;

inline bool ValidCoopCampaignHireRequest(const CoopCampaignHireRequest& request) noexcept
{
	return request.sessionEpoch && request.controlRevision && request.economyRevision && request.quoteRevision && request.requestId &&
		request.profile <= 254 && (request.days == 1 || request.days == 7 || request.days == 14);
}
inline bool SameCoopCampaignHireRequest(const CoopCampaignHireRequest& a, const CoopCampaignHireRequest& b) noexcept
{
	return a.sessionEpoch == b.sessionEpoch && a.controlRevision == b.controlRevision && a.economyRevision == b.economyRevision &&
		a.quoteRevision == b.quoteRevision && a.requestId == b.requestId && a.profile == b.profile && a.days == b.days && a.buyGear == b.buyGear;
}
inline bool EncodeCoopCampaignHireRequest(const CoopCampaignHireRequest& value, CoopCampaignHireRequestBytes& output) noexcept
{
	if (!ValidCoopCampaignHireRequest(value)) return false;
	using namespace CampaignEconomyWire;
	CoopCampaignHireRequestBytes bytes{{'J','2','H','Q'}};
	Put(bytes.data() + 4, CurrentProtocolVersion, 2); bytes[6] = value.days; bytes[7] = value.buyGear ? 1 : 0;
	Put(bytes.data() + 8, value.sessionEpoch, 8); Put(bytes.data() + 16, value.controlRevision, 8);
	Put(bytes.data() + 24, value.economyRevision, 8); Put(bytes.data() + 32, value.quoteRevision, 8);
	Put(bytes.data() + 40, value.requestId, 8); Put(bytes.data() + 48, value.profile, 2);
	output = bytes; return true;
}
inline bool DecodeCoopCampaignHireRequest(const std::uint8_t* bytes, std::size_t size, CoopCampaignHireRequest& output) noexcept
{
	using namespace CampaignEconomyWire;
	if (!bytes || size != CoopCampaignHireRequestWireSize || bytes[0] != 'J' || bytes[1] != '2' || bytes[2] != 'H' || bytes[3] != 'Q' ||
		Get(bytes + 4, 2) != CurrentProtocolVersion || bytes[7] > 1 || !Zero(bytes + 50, 6)) return false;
	CoopCampaignHireRequest value;
	value.days = bytes[6]; value.buyGear = bytes[7] != 0;
	value.sessionEpoch = Get(bytes + 8, 8); value.controlRevision = Get(bytes + 16, 8); value.economyRevision = Get(bytes + 24, 8);
	value.quoteRevision = Get(bytes + 32, 8); value.requestId = Get(bytes + 40, 8); value.profile = static_cast<std::uint16_t>(Get(bytes + 48, 2));
	if (!ValidCoopCampaignHireRequest(value)) return false;
	output = value; return true;
}
inline bool ValidCoopCampaignHireResult(const CoopCampaignHireResult& value) noexcept
{
	using Outcome = CoopCampaignHireOutcome;
	if (!ValidCoopCampaignHireRequest(value.request) || !value.controlRevision || !value.economyRevision || !value.quoteRevision ||
		value.outcome < Outcome::Applied || value.outcome > Outcome::Failed) return false;
	if (value.nativeAttempted)
	{
		if (value.controlRevision <= value.request.controlRevision || value.economyRevision < value.request.economyRevision ||
			value.quoteRevision < value.request.quoteRevision ||
			(value.outcome != Outcome::Applied && value.outcome != Outcome::NativeRejected && value.outcome != Outcome::Unsupported && value.outcome != Outcome::Failed)) return false;
	}
	else if (value.nativeDetail || value.outcome == Outcome::Applied || value.outcome == Outcome::NativeRejected) return false;
	if (value.outcome == Outcome::Applied) return value.actor.valid() && value.chargedTotal >= 0;
	return value.actor == TacticalEntityId{} && !value.chargedTotal;
}
inline bool EncodeCoopCampaignHireResult(const CoopCampaignHireResult& value, CoopCampaignHireResultBytes& output) noexcept
{
	if (!ValidCoopCampaignHireResult(value)) return false;
	using namespace CampaignEconomyWire;
	CoopCampaignHireRequestBytes request;
	if (!EncodeCoopCampaignHireRequest(value.request, request)) return false;
	CoopCampaignHireResultBytes bytes{};
	std::copy(request.begin(), request.end(), bytes.begin()); bytes[3] = 'R';
	Put(bytes.data() + 56, value.controlRevision, 8); Put(bytes.data() + 64, value.economyRevision, 8); Put(bytes.data() + 72, value.quoteRevision, 8);
	bytes[82] = static_cast<std::uint8_t>(value.outcome); bytes[83] = value.nativeAttempted ? 1 : 0;
	if (value.outcome == CoopCampaignHireOutcome::Applied)
	{
		Put(bytes.data() + 80, value.actor.slot, 2); Put(bytes.data() + 84, value.actor.incarnation, 4);
		Put(bytes.data() + 88, static_cast<std::uint32_t>(value.chargedTotal), 4);
	}
	Put(bytes.data() + 92, value.nativeDetail, 2);
	output = bytes; return true;
}
inline bool DecodeCoopCampaignHireResult(const std::uint8_t* bytes, std::size_t size, CoopCampaignHireResult& output) noexcept
{
	using namespace CampaignEconomyWire;
	if (!bytes || size != CoopCampaignHireResultWireSize || bytes[3] != 'R' || bytes[83] > 1 || !Zero(bytes + 94, 2)) return false;
	CoopCampaignHireRequestBytes request;
	std::copy(bytes, bytes + request.size(), request.begin()); request[3] = 'Q';
	CoopCampaignHireResult value;
	if (!DecodeCoopCampaignHireRequest(request.data(), request.size(), value.request)) return false;
	value.controlRevision = Get(bytes + 56, 8); value.economyRevision = Get(bytes + 64, 8); value.quoteRevision = Get(bytes + 72, 8);
	value.outcome = static_cast<CoopCampaignHireOutcome>(bytes[82]); value.nativeAttempted = bytes[83] != 0;
	value.nativeDetail = static_cast<std::uint16_t>(Get(bytes + 92, 2));
	if (value.outcome == CoopCampaignHireOutcome::Applied)
	{
		value.actor = {static_cast<std::uint16_t>(Get(bytes + 80, 2)), static_cast<std::uint32_t>(Get(bytes + 84, 4))};
		value.chargedTotal = Signed32(bytes + 88);
	}
	else if (!Zero(bytes + 80, 2) || !Zero(bytes + 84, 8)) return false;
	if (!ValidCoopCampaignHireResult(value)) return false;
	output = value; return true;
}
// Returns six for unsupported terms, so callers never silently price them as
// another contract. A missing gear choice is represented by its explicit flag.
inline std::size_t CoopCampaignHireChoiceIndex(std::uint8_t days, bool buyGear) noexcept
{
	return days == 1 ? (buyGear ? 1 : 0) : days == 7 ? (buyGear ? 3 : 2) : days == 14 ? (buyGear ? 5 : 4) : 6;
}
inline const CoopCampaignAimQuote* FindCoopCampaignAimQuote(const CoopCampaignAimQuotes& quotes, std::uint16_t profile) noexcept
{
	if (quotes.quoteCount > quotes.quotes.size()) return nullptr;
	for (std::size_t i = 0; i < quotes.quoteCount; ++i) if (quotes.quotes[i].profile == profile) return &quotes.quotes[i];
	return nullptr;
}
// Applied is eligibility only. The host must revalidate native prices, hiring
// policy, capacity, funds and arrival scheduling immediately before mutation.
inline CoopCampaignHireOutcome ValidateCoopCampaignHireRequest(const CoopCampaignHireRequest& request,
	const CoopCampaignStatus& status, const CoopCampaignEconomy& economy, const CoopCampaignAimQuotes& quotes,
	bool ready, bool authorized, bool worldlessStrategic) noexcept
{
	using Outcome = CoopCampaignHireOutcome;
	if (!ready) return Outcome::NotReady;
	if (!authorized) return Outcome::Unauthorized;
	if (!ValidCoopCampaignHireRequest(request) || !ValidCoopCampaignStatus(status) || !ValidCoopCampaignEconomy(economy) ||
		!ValidCoopCampaignAimQuotes(quotes) || request.sessionEpoch != status.sessionEpoch || request.sessionEpoch != economy.sessionEpoch ||
		request.sessionEpoch != quotes.sessionEpoch || request.controlRevision != status.timeControlRevision ||
		request.economyRevision != economy.revision) return Outcome::Stale;
	if (request.quoteRevision != quotes.revision || quotes.economyRevision != economy.revision) return Outcome::StaleQuote;
	if (!worldlessStrategic || status.phase != CoopCampaignPhase::Strategic || status.arrival.decision || !economy.available || !quotes.available)
		return Outcome::Unavailable;
	for (std::size_t i = 0; i < economy.rosterCount; ++i) if (economy.roster[i].profile == request.profile) return Outcome::AlreadyHired;
	const auto* quote = FindCoopCampaignAimQuote(quotes, request.profile);
	if (!quote) return Outcome::Unavailable;
	if (quote->status == CoopCampaignAimQuoteStatus::AlreadyHired) return Outcome::AlreadyHired;
	if (quote->status == CoopCampaignAimQuoteStatus::Unsupported || (quote->status == CoopCampaignAimQuoteStatus::Available && request.buyGear && !quote->gearAvailable))
		return Outcome::Unsupported;
	if (quote->status != CoopCampaignAimQuoteStatus::Available || economy.mercenaryCount >= economy.mercenaryLimit) return Outcome::Unavailable;
	return economy.balance < quote->total[CoopCampaignHireChoiceIndex(request.days, request.buyGear)] ? Outcome::InsufficientFunds : Outcome::Applied;
}
}
#endif
