#ifndef JA2_CAMPAIGN_CLOCK_ADAPTER_H
#define JA2_CAMPAIGN_CLOCK_ADAPTER_H

#include <cstdint>

#include <Engine/Adapters/JA2/CampaignClockSession.h>

// Exact legacy mirrors remain available to old modules as read-only by
// convention. Architecture checks below the gateway enforce that convention.
extern std::uint32_t guiGameClock;
extern std::uint32_t guiPreviousGameClock;

// Application composition and the only production write gateway for JA2's
// campaign-time compatibility globals.
void BindJa2CampaignClockSession(CampaignClockSession& session) noexcept;
const CampaignClockSession::Snapshot& CaptureJa2CampaignClock() noexcept;
void InitializeJa2CampaignClock(std::uint32_t startingSeconds) noexcept;
void AdvanceJa2CampaignClockUncommitted(std::uint32_t seconds) noexcept;
void SetJa2CampaignClockEventTime(std::uint32_t totalSeconds) noexcept;
CampaignClockSession::AdvanceCommit CommitJa2CampaignClockAdvance() noexcept;
void RestoreJa2CampaignClock(
	std::uint32_t totalSeconds,
	std::uint32_t previousTotalSeconds) noexcept;
void RestoreJa2CampaignClockSession(
	CampaignClockSession::Snapshot state) noexcept;

// Retains one JA2TESTVERSION shortcut whose calendar-only override predates the
// total-seconds clock. Production transitions derive these fields from time.
void OverrideJa2CampaignClockCalendar(
	std::uint32_t day,
	std::uint32_t hour,
	std::uint32_t minute) noexcept;

#endif
