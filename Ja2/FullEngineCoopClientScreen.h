#ifndef JA2_FULL_ENGINE_COOP_CLIENT_SCREEN_H
#define JA2_FULL_ENGINE_COOP_CLIENT_SCREEN_H

#include <cstdint>
#include <Engine/Adapters/JA2/StrategicGroup.h>

struct FullEngineCoopClientCampaignControls
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, groupsRevision = 0, decision = 0;
	StrategicGroupId selected{};
	std::uint8_t destinationX = 0, destinationY = 0;
	bool retreatArmed = false, enabled = false;
};

// Copied local choices bound to the current authoritative strategic state.
// No live native group, map selection, clock or simulation state is exposed.
bool CaptureFullEngineCoopClientCampaignControls(FullEngineCoopClientCampaignControls& output) noexcept;

struct FullEngineCoopClientCampaignHireControls
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, economyRevision = 0, quoteRevision = 0;
	std::uint16_t profile = UINT16_MAX;
	std::uint8_t days = 7;
	bool buyGear = false;
	bool open = false, armed = false, enabled = false, canHire = false;
};
bool CaptureFullEngineCoopClientCampaignHireControls(FullEngineCoopClientCampaignHireControls& output) noexcept;

// Small allocation-free UI ledger for irreversible voluntary retirement.
// A first L down cannot confirm itself: the key must be released before a
// later L down can commit. Repeats and multiple queued downs therefore remain
// harmless. The screen cancels this ledger on any mode/connection-state change.
class FullEngineCoopClientRetirementConfirmation final
{
public:
	static constexpr std::uint64_t FrameBudget = 180;

	bool pressLeave(std::uint64_t frame) noexcept
	{
		advance(frame);
		if (state_ == State::Armed)
		{
			cancel();
			return true;
		}
		if (state_ == State::Idle)
		{
			state_ = State::AwaitRelease;
			deadline_ = frame > UINT64_MAX - FrameBudget
				? UINT64_MAX : frame + FrameBudget;
		}
		return false;
	}

	void releaseLeave(std::uint64_t frame) noexcept
	{
		advance(frame);
		if (state_ == State::AwaitRelease) state_ = State::Armed;
	}

	void advance(std::uint64_t frame) noexcept
	{
		if (state_ != State::Idle && frame > deadline_) cancel();
	}

	void cancel() noexcept
	{
		state_ = State::Idle;
		deadline_ = 0;
	}

	bool pending() const noexcept { return state_ != State::Idle; }
	bool armed() const noexcept { return state_ == State::Armed; }

private:
	enum class State : std::uint8_t { Idle, AwaitRelease, Armed };
	State state_ = State::Idle;
	std::uint64_t deadline_ = 0;
};

// Render and handle the passive, worldless co-op control surface. This is an
// INIT_SCREEN child, not a tactical or strategic JA2 screen.
void HandleFullEngineCoopClientScreen() noexcept;

// Shared input owner for the existing passive screen.
void HandleFullEngineCoopClientInput() noexcept;

struct FullEngineCoopClientSurrenderControls
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, groupsRevision = 0, offer = 0;
	unsigned armed = 0;
	bool enabled = false, pending = false;
};
bool CaptureFullEngineCoopClientSurrenderControls(FullEngineCoopClientSurrenderControls& output) noexcept;

struct FullEngineCoopClientBattleNoticeControls
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, groupsRevision = 0, notice = 0;
	unsigned armed = 0, kind = 0, x = 0, y = 0, z = 0;
	bool enabled = false, pending = false, sectorControlLost = false;
};
bool CaptureFullEngineCoopClientBattleNoticeControls(FullEngineCoopClientBattleNoticeControls& output) noexcept;

struct FullEngineCoopClientMeanwhileControls
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, groupsRevision = 0, notice = 0;
	unsigned armed = 0, scene = 0;
	bool enabled = false, pending = false;
};
bool CaptureFullEngineCoopClientMeanwhileControls(FullEngineCoopClientMeanwhileControls& output) noexcept;

#endif
