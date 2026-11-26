#ifndef JA2_FULL_ENGINE_COOP_CLIENT_HAND_OVER_INPUT_H
#define JA2_FULL_ENGINE_COOP_CLIENT_HAND_OVER_INPUT_H

#include <Multiplayer/CoopCampaignAction.h>
#include <Multiplayer/CoopCampaignGroups.h>
#include <Multiplayer/CoopCampaignStatus.h>
#include <array>
#include <cstdint>
#include <optional>

// Giving a merc to a teammate: J arms it for the selected merc this player
// controls, and a separately pressed Enter confirms. The server gives it to the
// next player in the sector (the teammate, with two players) and checks again
// that this player controls it. Esc, key repeat, another selection, a pending
// campaign request, a modal decision or a new session disarms it.
// Authentication/revisions are filled by Client.
class FullEngineCoopClientHandOverInput
{
public:
	enum class Key : unsigned { None, Arm, Confirm, Cancel, Count };
	void reset() noexcept { *this = {}; }
	static bool available(const CoopSession::CoopCampaignStatus* status) noexcept
	{
		return status && CoopSession::ValidCoopCampaignStatus(*status) &&
			status->phase == CoopSession::CoopCampaignPhase::Tactical &&
			!status->surrenderOffer && !status->battleNotice.id && !status->meanwhile.id;
	}
	void synchronize(const CoopSession::CoopCampaignStatus* status, TacticalEntityId selected,
		bool selectedControlled, bool enabled, bool pending) noexcept
	{
		const bool valid = available(status);
		const auto session = valid ? status->sessionEpoch : 0;
		const TacticalEntityId current = selectedControlled ? selected : TacticalEntityId{};
		if (!valid || !enabled || pending || session != session_ || armed_ != current) armed_ = {};
		session_ = session;
		selected_ = current;
		enabled_ = enabled && valid && !pending && current.valid();
	}
	std::optional<CoopSession::CoopCampaignActionRequest> handle(Key key, bool down, bool up,
		const CoopSession::CoopCampaignStatus* status, TacticalEntityId selected, bool selectedControlled,
		bool enabled, bool pending) noexcept
	{
		const bool wasArmed = armed_.valid();
		synchronize(status, selected, selectedControlled, enabled, pending);
		const auto index = static_cast<unsigned>(key);
		if (key == Key::None || index >= held_.size()) return {};
		if (up) { held_[index] = false; return {}; }
		if (!down) return {};
		const bool wasHeld = held_[index]; held_[index] = true;
		if (wasHeld) return {};
		if (key == Key::Cancel) { armed_ = {}; return {}; }
		if (!enabled_) return {};
		if (key == Key::Arm) { armed_ = selected_; return {}; }
		if (key != Key::Confirm || !wasArmed || !armed_.valid()) return {};
		CoopSession::CoopCampaignActionRequest request;
		request.action = CoopSession::CoopCampaignAction::HandOverMerc;
		request.decision = static_cast<std::uint64_t>(armed_.slot) | (static_cast<std::uint64_t>(armed_.incarnation) << 16);
		armed_ = {};
		return request;
	}
	bool armed() const noexcept { return armed_.valid(); }
	TacticalEntityId armedActor() const noexcept { return armed_; }
	TacticalEntityId selected() const noexcept { return selected_; }
	bool enabled() const noexcept { return enabled_; }
private:
	std::array<bool, static_cast<unsigned>(Key::Count)> held_{};
	TacticalEntityId armed_{}, selected_{};
	std::uint64_t session_ = 0;
	bool enabled_ = false;
};

#endif
