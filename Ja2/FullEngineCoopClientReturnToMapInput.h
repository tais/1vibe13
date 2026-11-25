#ifndef JA2_FULL_ENGINE_COOP_CLIENT_RETURN_TO_MAP_INPUT_H
#define JA2_FULL_ENGINE_COOP_CLIENT_RETURN_TO_MAP_INPUT_H

#include <Multiplayer/CoopCampaignAction.h>
#include <array>
#include <optional>

// Leaving a secured sector is a shared, irreversible campaign action: W arms the
// return to the map, U arms taking the exit grid (stairs) for the selected merc's
// group, and a separately pressed Enter confirms either. Key repeat, a stale or
// replaced secured sector or group, another pending campaign request, or any
// modal decision disarms it. Authentication/revisions are filled by Client.
class FullEngineCoopClientReturnToMapInput
{
public:
	enum class Key : unsigned { None, Arm, Confirm, Cancel, ArmExit, Count };
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
		{
			armed_ = false;
			exitGroup_ = {};
		}
		session_ = session; control_ = control; groups_ = revision; sector_ = sector;
		enabled_ = enabled && valid && !pending;
	}
	std::optional<CoopSession::CoopCampaignActionRequest> handle(Key key, bool down, bool up,
		const CoopSession::CoopCampaignStatus* status, const CoopSession::CoopCampaignGroups* groups,
		bool enabled, bool pending, StrategicGroupId selectedGroup = {}) noexcept
	{
		synchronize(status, groups, enabled, pending);
		const auto index = static_cast<unsigned>(key);
		if (key == Key::None || index >= held_.size()) return {};
		if (up) { held_[index] = false; return {}; }
		if (!down) return {};
		const bool wasHeld = held_[index]; held_[index] = true;
		if (wasHeld || !enabled_) return {};
		if (key == Key::Cancel) { armed_ = false; exitGroup_ = {}; return {}; }
		if (key == Key::Arm) { armed_ = true; exitGroup_ = {}; return {}; }
		if (key == Key::ArmExit)
		{
			armed_ = false;
			exitGroup_ = observedGroup(*groups, selectedGroup) ? selectedGroup : StrategicGroupId{};
			return {};
		}
		if (key != Key::Confirm) return {};
		CoopSession::CoopCampaignActionRequest request;
		request.decision = sector_;
		if (armed_)
			request.action = CoopSession::CoopCampaignAction::ReturnToMap;
		else if (exitGroup_.valid() && observedGroup(*groups, exitGroup_))
		{
			request.action = CoopSession::CoopCampaignAction::TakeExitGrid;
			request.group = exitGroup_;
		}
		else return {};
		armed_ = false; exitGroup_ = {};
		return request;
	}
	bool armed() const noexcept { return armed_; }
	bool exitArmed() const noexcept { return exitGroup_.valid(); }
	StrategicGroupId exitGroup() const noexcept { return exitGroup_; }
	bool enabled() const noexcept { return enabled_; }
private:
	static bool observedGroup(const CoopSession::CoopCampaignGroups& groups, StrategicGroupId id) noexcept
	{
		if (!id.valid() || groups.groupCount > groups.groups.size()) return false;
		for (std::size_t i = 0; i < groups.groupCount; ++i)
			if (groups.groups[i].id == id) return !groups.groups[i].betweenSectors && !groups.groups[i].vehicle;
		return false;
	}
	std::array<bool, static_cast<unsigned>(Key::Count)> held_{};
	StrategicGroupId exitGroup_{};
	std::uint64_t session_ = 0, control_ = 0, groups_ = 0, sector_ = 0;
	bool armed_ = false;
	bool enabled_ = false;
};
#endif
