#ifndef JA2_FULL_ENGINE_COOP_CLIENT_MEANWHILE_INPUT_H
#define JA2_FULL_ENGINE_COOP_CLIENT_MEANWHILE_INPUT_H

#include <Multiplayer/CoopCampaignAction.h>
#include <array>
#include <optional>

inline const wchar_t* FullEngineCoopClientMeanwhileTitle(CoopSession::CoopCampaignMeanwhileScene scene) noexcept
{
	using Scene = CoopSession::CoopCampaignMeanwhileScene;
	switch (scene)
	{
		case Scene::FirstBattle: return L"The Queen learns of your first battle";
		case Scene::Drassen: return L"Drassen liberated";
		case Scene::Cambria: return L"Cambria liberated";
		case Scene::Alma: return L"Alma liberated";
		case Scene::Grumm: return L"Grumm liberated";
		case Scene::Chitzena: return L"Chitzena liberated";
		case Scene::NorthwestSam: return L"Northwest SAM site liberated";
		case Scene::NortheastSam: return L"Northeast SAM site liberated";
		case Scene::CentralSam: return L"Central SAM site liberated";
		case Scene::Flowers: return L"Flowers for the Queen";
		case Scene::LostTown: return L"A town has fallen";
		case Scene::Interrogation: return L"Captured mercenaries";
		case Scene::Creatures: return L"Creatures";
		case Scene::Helicopter: return L"The Queen learns of your helicopter";
		case Scene::Scientist: return L"The missing scientist";
		case Scene::Meduna: return L"The outskirts of Meduna";
		case Scene::Balime: return L"Balime liberated";
		case Scene::None: break;
	}
	return L"Campaign scene";
}

// Presentation-only scene skipping requires Skip followed by
// a separately pressed Enter; key repeat, stale state and reconnect cannot
// acknowledge a scene. The request's authentication/revisions are filled by Client.
class FullEngineCoopClientMeanwhileInput
{
public:
	enum class Key : unsigned { None, Skip, Confirm, Cancel, Count };
	void reset() noexcept { *this = {}; }
	void synchronize(const CoopSession::CoopCampaignStatus* status,
		const CoopSession::CoopCampaignGroups* groups, bool enabled, bool pending) noexcept
	{
		const bool valid = status && groups && CoopSession::ValidCoopCampaignStatus(*status) &&
			CoopSession::ValidCoopCampaignGroups(*groups) && groups->sessionEpoch == status->sessionEpoch && status->meanwhile.id;
		const auto session = valid ? status->sessionEpoch : 0;
		const auto control = valid ? status->timeControlRevision : 0;
		const auto revision = valid ? groups->revision : 0;
		const auto notice = valid ? status->meanwhile.id : 0;
		if (!enabled || pending || session != session_ || control != control_ || revision != groups_ || notice != notice_)
			armed_ = 0;
		session_ = session; control_ = control; groups_ = revision; notice_ = notice;
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
		if (key == Key::Skip)
		{
			armed_ = static_cast<unsigned>(CoopSession::CoopCampaignAction::SkipMeanwhile);
			return {};
		}
		if (key != Key::Confirm || !armed_) return {};
		CoopSession::CoopCampaignActionRequest request;
		request.action = static_cast<CoopSession::CoopCampaignAction>(armed_);
		request.decision = notice_; armed_ = 0;
		return request;
	}
	unsigned armed() const noexcept { return armed_; }
	bool enabled() const noexcept { return enabled_; }
private:
	std::array<bool, static_cast<unsigned>(Key::Count)> held_{};
	std::uint64_t session_ = 0, control_ = 0, groups_ = 0, notice_ = 0;
	unsigned armed_ = 0;
	bool enabled_ = false;
};
#endif
