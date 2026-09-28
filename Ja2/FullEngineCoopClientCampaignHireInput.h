#ifndef JA2_FULL_ENGINE_COOP_CLIENT_CAMPAIGN_HIRE_INPUT_H
#define JA2_FULL_ENGINE_COOP_CLIENT_CAMPAIGN_HIRE_INPUT_H

#include <Multiplayer/CoopCampaignHire.h>
#include <optional>

enum class FullEngineCoopClientCampaignHireKey : unsigned
{
	None, Toggle, Next, Previous, OneDay, SevenDays, FourteenDays, Confirm, Cancel, Count
};
struct FullEngineCoopClientCampaignHireSelection
{
	std::uint16_t profile = 0;
	std::uint8_t days = 0;
};

// Local selection and confirmation only. No laptop handler, profile mutation,
// client price or actor construction crosses this boundary.
class FullEngineCoopClientCampaignHireInput
{
public:
	using Key = FullEngineCoopClientCampaignHireKey;
	static constexpr std::uint16_t NoProfile = UINT16_MAX;
	void reset() noexcept { *this = {}; }
	void synchronize(const CoopSession::CoopCampaignStatus* status, const CoopSession::CoopCampaignEconomy* economy,
		const CoopSession::CoopCampaignAimQuotes* quotes, bool enabled, bool pending) noexcept
	{
		const bool strategic = status && CoopSession::ValidCoopCampaignStatus(*status) &&
			status->phase == CoopSession::CoopCampaignPhase::Strategic && !status->arrival.decision && !status->meanwhile.id;
		const bool haveEconomy = strategic && economy && CoopSession::ValidCoopCampaignEconomy(*economy) && economy->sessionEpoch == status->sessionEpoch;
		const bool haveQuotes = haveEconomy && quotes && CoopSession::ValidCoopCampaignAimQuotes(*quotes) &&
			quotes->sessionEpoch == status->sessionEpoch && quotes->economyRevision == economy->revision;
		const auto session = strategic ? status->sessionEpoch : 0;
		const auto control = strategic ? status->timeControlRevision : 0;
		const auto economic = haveEconomy ? economy->revision : 0;
		const auto quoted = haveQuotes ? quotes->revision : 0;
		if (session != session_) { open_ = false; profile_ = NoProfile; days_ = 7; }
		if (!enabled || pending || session != session_ || control != control_ || economic != economyRevision_ || quoted != quoteRevision_)
			armed_ = false;
		session_ = session; control_ = control; economyRevision_ = economic; quoteRevision_ = quoted;
		if (!strategic) open_ = false;
		enabled_ = enabled && !pending && haveQuotes && economy->available && quotes->available;
		if (haveQuotes)
		{
			if (!quotes->available || !quotes->quoteCount) profile_ = NoProfile;
			else if (!selectedQuote(*quotes)) { profile_ = quotes->quotes[0].profile; armed_ = false; }
		}
	}
	std::optional<FullEngineCoopClientCampaignHireSelection> handle(Key key, bool down, bool up,
		const CoopSession::CoopCampaignStatus* status, const CoopSession::CoopCampaignEconomy* economy,
		const CoopSession::CoopCampaignAimQuotes* quotes, bool enabled, bool pending) noexcept
	{
		synchronize(status,economy,quotes,enabled,pending);
		const auto index = static_cast<unsigned>(key);
		if (key == Key::None || index >= held_.size()) return {};
		if (up) { held_[index] = false; return {}; }
		if (!down) return {};
		const bool held = held_[index]; held_[index] = true;
		if (held) return {};
		if (key == Key::Toggle && open_) { open_ = armed_ = false; return {}; }
		if (key == Key::Cancel && open_) { if (armed_) armed_ = false; else open_ = false; return {}; }
		if (!enabled_) return {};
		if (key == Key::Toggle) { open_ = true; armed_ = false; return {}; }
		if (!open_) return {};
		const auto* quote = selectedQuote(*quotes);
		if (!quote) return {};
		if (key == Key::Next || key == Key::Previous)
		{
			const auto at = static_cast<std::size_t>(quote - quotes->quotes.data());
			const auto next = key == Key::Next ? (at + 1) % quotes->quoteCount : (at + quotes->quoteCount - 1) % quotes->quoteCount;
			profile_ = quotes->quotes[next].profile; armed_ = false; return {};
		}
		if (key == Key::OneDay || key == Key::SevenDays || key == Key::FourteenDays)
		{
			days_ = key == Key::OneDay ? 1 : key == Key::SevenDays ? 7 : 14; armed_ = false; return {};
		}
		if (key != Key::Confirm || !canHire(*status,*economy,*quotes)) return {};
		if (!armed_) { armed_ = true; return {}; }
		armed_ = false;
		return FullEngineCoopClientCampaignHireSelection{profile_,days_};
	}
	bool canHire(const CoopSession::CoopCampaignStatus& status, const CoopSession::CoopCampaignEconomy& economy,
		const CoopSession::CoopCampaignAimQuotes& quotes) const noexcept
	{
		CoopSession::CoopCampaignHireRequest request;
		request.sessionEpoch = session_; request.controlRevision = control_; request.economyRevision = economyRevision_;
		request.quoteRevision = quoteRevision_; request.requestId = 1; request.profile = profile_; request.days = days_;
		return enabled_ && CoopSession::ValidateCoopCampaignHireRequest(request,status,economy,quotes,true,true,true) == CoopSession::CoopCampaignHireOutcome::Applied;
	}
	const CoopSession::CoopCampaignAimQuote* selectedQuote(const CoopSession::CoopCampaignAimQuotes& quotes) const noexcept
	{ return profile_ == NoProfile ? nullptr : CoopSession::FindCoopCampaignAimQuote(quotes,profile_); }
	bool open() const noexcept { return open_; }
	bool armed() const noexcept { return armed_; }
	bool enabled() const noexcept { return enabled_; }
	std::uint16_t profile() const noexcept { return profile_; }
	std::uint8_t days() const noexcept { return days_; }
private:
	std::array<bool,static_cast<unsigned>(Key::Count)> held_{};
	std::uint64_t session_ = 0, control_ = 0, economyRevision_ = 0, quoteRevision_ = 0;
	std::uint16_t profile_ = NoProfile;
	std::uint8_t days_ = 7;
	bool open_ = false, armed_ = false, enabled_ = false;
};

inline const wchar_t* FullEngineCoopClientCampaignHireOutcomeText(CoopSession::CoopCampaignHireOutcome outcome) noexcept
{
	using O = CoopSession::CoopCampaignHireOutcome;
	switch (outcome)
	{
		case O::Applied: return L"Hired by the server; arrival is scheduled.";
		case O::NotReady: return L"Campaign synchronization is required.";
		case O::Unauthorized: return L"This player cannot hire for the campaign.";
		case O::Stale: case O::StaleQuote: return L"Campaign or offer changed. Review the current terms and choose again.";
		case O::Unavailable: return L"This mercenary cannot be hired right now.";
		case O::AlreadyHired: return L"This mercenary has already been hired.";
		case O::InsufficientFunds: return L"The shared campaign balance cannot cover this contract.";
		case O::NativeRejected: return L"The server could not accept these hiring terms.";
		case O::Unsupported: return L"This hiring option is not available yet.";
		case O::Failed: return L"The server could not complete the hire. Check the server log.";
	}
	return L"Unknown hire result.";
}
inline const wchar_t* FullEngineCoopClientCampaignAimQuoteText(CoopSession::CoopCampaignAimQuoteStatus status) noexcept
{
	using S = CoopSession::CoopCampaignAimQuoteStatus;
	switch (status)
	{
		case S::Available: return L"Available";
		case S::Unavailable: return L"Unavailable";
		case S::AlreadyHired: return L"Already hired";
		case S::Unwilling: return L"Declines this campaign";
		case S::TeamFull: return L"Team is full";
		case S::Unsupported: return L"Offer not supported yet";
	}
	return L"Unavailable";
}
#endif
