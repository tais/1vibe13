#ifndef JA2_FULL_ENGINE_COOP_CLIENT_RETURN_TO_MAP_INPUT_H
#define JA2_FULL_ENGINE_COOP_CLIENT_RETURN_TO_MAP_INPUT_H

#include <Multiplayer/CoopCampaignAction.h>
#include <array>
#include <optional>

// Leaving a secured sector is a shared, irreversible campaign action: W arms it
// and a separately pressed Enter confirms. Key repeat, a stale or replaced
// secured sector, another pending campaign request, or any modal decision
// disarms it. The request's authentication/revisions are filled by Client.
class FullEngineCoopClientReturnToMapInput
{
public:
	enum class Key : unsigned { None, Arm, Confirm, Cancel, Count };
	void reset() noexcept { *this = {}; }
	static bool available(const CoopSession::CoopCampaignStatus* status,
		const CoopSession::CoopCampaignGroups* groups) noexcept
	{
		return status && groups && CoopSession::ValidCoopCampaignStatus(*status) &&
			CoopSession::ValidCoopCampaignGroups(*groups) && groups->sessionEpoch == status->sessionEpoch &&
			status->phase == CoopSession::CoopCampaignPhase::Tactical && status->securedSector &&
			!status->surrenderOffer && !status->battleNotice.id && !status->meanwhile.id;
	}
	void synchronize(const CoopSession::CoopCampaignStatus* status,
		const CoopSession::CoopCampaignGroups* groups, bool enabled, bool pending) noexcept
	{
		const bool valid = available(status, groups);
		const auto session = valid ? status->sessionEpoch : 0;
		const auto control = valid ? status->timeControlRevision : 0;
		const auto revision = valid ? groups->revision : 0;
		const auto sector = valid ? status->securedSector : 0;
		if (!enabled || pending || session != session_ || control != control_ || revision != groups_ || sector != sector_)
			armed_ = false;
		session_ = session; control_ = control; groups_ = revision; sector_ = sector;
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
		if (key == Key::Cancel) { armed_ = false; return {}; }
		if (key == Key::Arm) { armed_ = true; return {}; }
		if (key != Key::Confirm || !armed_) return {};
		CoopSession::CoopCampaignActionRequest request;
		request.action = CoopSession::CoopCampaignAction::ReturnToMap;
		request.decision = sector_; armed_ = false;
		return request;
	}
	bool armed() const noexcept { return armed_; }
	bool enabled() const noexcept { return enabled_; }
private:
	std::array<bool, static_cast<unsigned>(Key::Count)> held_{};
	std::uint64_t session_ = 0, control_ = 0, groups_ = 0, sector_ = 0;
	bool armed_ = false;
	bool enabled_ = false;
};
#endif
