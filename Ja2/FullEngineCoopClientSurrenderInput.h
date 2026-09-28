#ifndef JA2_FULL_ENGINE_COOP_CLIENT_SURRENDER_INPUT_H
#define JA2_FULL_ENGINE_COOP_CLIENT_SURRENDER_INPUT_H

#include <Multiplayer/CoopCampaignAction.h>
#include <array>
#include <optional>

// Presentation-only confirmation. Either answer requires a choice followed by
// a separately pressed Enter; key repeat, stale state and reconnect cannot
// answer an offer. The request's authentication/revisions are filled by Client.
class FullEngineCoopClientSurrenderInput
{
public:
	enum class Key : unsigned { None, Fight, Surrender, Confirm, Cancel, Count };
	void reset() noexcept { *this = {}; }
	void synchronize(const CoopSession::CoopCampaignStatus* status,
		const CoopSession::CoopCampaignGroups* groups, bool enabled, bool pending) noexcept
	{
		const bool valid = status && groups && CoopSession::ValidCoopCampaignStatus(*status) &&
			CoopSession::ValidCoopCampaignGroups(*groups) && groups->sessionEpoch == status->sessionEpoch && status->surrenderOffer;
		const auto session = valid ? status->sessionEpoch : 0;
		const auto control = valid ? status->timeControlRevision : 0;
		const auto revision = valid ? groups->revision : 0;
		const auto offer = valid ? status->surrenderOffer : 0;
		if (!enabled || pending || session != session_ || control != control_ || revision != groups_ || offer != offer_)
			armed_ = 0;
		session_ = session; control_ = control; groups_ = revision; offer_ = offer;
		enabled_ = enabled && valid && !pending;
	}
	std::optional<CoopSession::CoopCampaignActionRequest> handle(Key key, bool down, bool up,
		const CoopSession::CoopCampaignStatus* status, const CoopSession::CoopCampaignGroups* groups,
		bool enabled, bool pending) noexcept
	{
		synchronize(status, groups, enabled, pending);
		const auto index = static_cast<unsigned>(key);
		if (key == Key::None || index >= held_.size()) return {};
		if (up) { held_[index] = false; return {}; }
		if (!down) return {};
		const bool wasHeld = held_[index]; held_[index] = true;
		if (wasHeld || !enabled_) return {};
		if (key == Key::Cancel) { armed_ = 0; return {}; }
		if (key == Key::Fight || key == Key::Surrender)
		{
			armed_ = static_cast<unsigned>(key == Key::Fight ? CoopSession::CoopCampaignAction::DeclineSurrender : CoopSession::CoopCampaignAction::AcceptSurrender);
			return {};
		}
		if (key != Key::Confirm || !armed_) return {};
		CoopSession::CoopCampaignActionRequest request;
		request.action = static_cast<CoopSession::CoopCampaignAction>(armed_);
		request.decision = offer_; armed_ = 0;
		return request;
	}
	unsigned armed() const noexcept { return armed_; }
	bool enabled() const noexcept { return enabled_; }
private:
	std::array<bool, static_cast<unsigned>(Key::Count)> held_{};
	std::uint64_t session_ = 0, control_ = 0, groups_ = 0, offer_ = 0;
	unsigned armed_ = 0;
	bool enabled_ = false;
};
#endif
