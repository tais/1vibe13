#include "CampaignClockAdapter.h"

#include "GameSettings.h"
#include "types.h"

// Exact legacy symbols retained as read-compatible mirrors. Their primitive
// types and initial values remain unchanged for source and save compatibility.
UINT32 guiGameClock = gGameExternalOptions.iGameStartingTime;
UINT32 guiPreviousGameClock = 0;
UINT32 guiDay = 0;
UINT32 guiHour = 0;
UINT32 guiMin = 0;

namespace
{
CampaignClockSession ownedSession;
CampaignClockSession* activeSession = &ownedSession;
bool sessionImported = false;

CampaignClockSession::Snapshot LegacySnapshot() noexcept
{
	return CampaignClockSession::Snapshot{
		guiGameClock, guiPreviousGameClock, guiDay, guiHour, guiMin};
}

CampaignClockSession& ActiveSession() noexcept
{
	if (!sessionImported)
	{
		activeSession->restore(LegacySnapshot());
		sessionImported = true;
	}
	return *activeSession;
}

void SynchronizeLegacyClockMirrors() noexcept
{
	const CampaignClockSession::Snapshot& state = ActiveSession().snapshot();
	guiGameClock = state.totalSeconds;
	guiPreviousGameClock = state.previousTotalSeconds;
	guiDay = state.day;
	guiHour = state.hour;
	guiMin = state.minute;
}
}

void BindJa2CampaignClockSession(CampaignClockSession& session) noexcept
{
	session.restore(LegacySnapshot());
	activeSession = &session;
	sessionImported = true;
	SynchronizeLegacyClockMirrors();
}

const CampaignClockSession::Snapshot& CaptureJa2CampaignClock() noexcept
{
	return ActiveSession().snapshot();
}

void InitializeJa2CampaignClock(std::uint32_t startingSeconds) noexcept
{
	ActiveSession().initialize(startingSeconds);
	SynchronizeLegacyClockMirrors();
}

void AdvanceJa2CampaignClockUncommitted(std::uint32_t seconds) noexcept
{
	ActiveSession().advanceUncommitted(seconds);
	SynchronizeLegacyClockMirrors();
}

void SetJa2CampaignClockEventTime(std::uint32_t totalSeconds) noexcept
{
	ActiveSession().setEventTime(totalSeconds);
	SynchronizeLegacyClockMirrors();
}

CampaignClockSession::AdvanceCommit CommitJa2CampaignClockAdvance() noexcept
{
	const CampaignClockSession::AdvanceCommit result =
		ActiveSession().commitAdvance();
	SynchronizeLegacyClockMirrors();
	return result;
}

void RestoreJa2CampaignClock(
	std::uint32_t totalSeconds,
	std::uint32_t previousTotalSeconds) noexcept
{
	ActiveSession().restoreSaved(totalSeconds, previousTotalSeconds);
	SynchronizeLegacyClockMirrors();
}

void RestoreJa2CampaignClockSession(
	CampaignClockSession::Snapshot state) noexcept
{
	ActiveSession().restore(state);
	SynchronizeLegacyClockMirrors();
}

void OverrideJa2CampaignClockCalendar(
	std::uint32_t day,
	std::uint32_t hour,
	std::uint32_t minute) noexcept
{
	ActiveSession().overrideCalendar(day, hour, minute);
	SynchronizeLegacyClockMirrors();
}
