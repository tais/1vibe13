#ifndef JA2_FULL_ENGINE_COOP_CLIENT_CAMPAIGN_TIME_INPUT_H
#define JA2_FULL_ENGINE_COOP_CLIENT_CAMPAIGN_TIME_INPUT_H
#include <Multiplayer/CoopCampaignTime.h>
#include <optional>

// Physical down/up latch, independent of rendering/network latency. Holding a
// key cannot retry a stale or native-rejected resume after its obstacle clears.
class FullEngineCoopClientCampaignTimeInput
{
public:
	void reset() noexcept { held_ = {}; }
	std::optional<CoopSession::CoopCampaignTimeAction> handle(unsigned key, bool down, bool up, bool enabled) noexcept
	{
		const unsigned index = key == 'p' || key == 'P' ? 0 : key >= '1' && key <= '3' ? key - '1' + 1 : 4;
		if (index == 4) return {};
		if (up) { held_[index] = false; return {}; }
		if (!down) return {};
		const bool held = held_[index];
		held_[index] = true;
		if (held || !enabled) return {};
		return static_cast<CoopSession::CoopCampaignTimeAction>(index + 1);
	}
private:
	std::array<bool, 4> held_{};
};

inline const wchar_t* FullEngineCoopClientCampaignTimeOutcomeText(CoopSession::CoopCampaignTimeOutcome outcome) noexcept
{
	switch (outcome)
	{
		case CoopSession::CoopCampaignTimeOutcome::Applied: return L"Time setting applied by the server.";
		case CoopSession::CoopCampaignTimeOutcome::NotLeader: return L"Only the designated time leader can change strategic time.";
		case CoopSession::CoopCampaignTimeOutcome::Unavailable: return L"Time controls are available only in worldless strategic state.";
		case CoopSession::CoopCampaignTimeOutcome::Stale: return L"Time state changed. Review the clock and press again if appropriate.";
		case CoopSession::CoopCampaignTimeOutcome::NativeBlocked: return L"Native gameplay blocked this speed change (event, dialogue, battle or pause lock).";
		case CoopSession::CoopCampaignTimeOutcome::NotReady: return L"Campaign synchronization is required before changing time.";
	}
	return L"Unknown time-control result.";
}
#endif
