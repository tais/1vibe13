#include "CampaignClockAdapter.h"

#include "GameSettings.h"

namespace
{
CampaignClockSession ownedSession;
CampaignClockSession* activeSession = &ownedSession;
bool sessionInitialized = false;

CampaignClockSession& ActiveSession() noexcept
{
	if (!sessionInitialized)
	{
		activeSession->initialize(gGameExternalOptions.iGameStartingTime);
		sessionInitialized = true;
	}
	return *activeSession;
}
}

void BindJa2CampaignClockSession(CampaignClockSession& session) noexcept
{
	session.restore(ActiveSession().snapshot());
	activeSession = &session;
	sessionInitialized = true;
}

const CampaignClockSession::Snapshot& CaptureJa2CampaignClock() noexcept
{
	return ActiveSession().snapshot();
}

void InitializeJa2CampaignClock(std::uint32_t startingSeconds) noexcept
{
	ActiveSession().initialize(startingSeconds);
}

void AdvanceJa2CampaignClockUncommitted(std::uint32_t seconds) noexcept
{
	ActiveSession().advanceUncommitted(seconds);
}

void SetJa2CampaignClockEventTime(std::uint32_t totalSeconds) noexcept
{
	ActiveSession().setEventTime(totalSeconds);
}

CampaignClockSession::AdvanceCommit CommitJa2CampaignClockAdvance() noexcept
{
	const CampaignClockSession::AdvanceCommit result =
		ActiveSession().commitAdvance();
	return result;
}

void RestoreJa2CampaignClock(
	std::uint32_t totalSeconds,
	std::uint32_t previousTotalSeconds) noexcept
{
	ActiveSession().restoreSaved(totalSeconds, previousTotalSeconds);
}

void RestoreJa2CampaignClockSession(
	CampaignClockSession::Snapshot state) noexcept
{
	ActiveSession().restore(state);
}

void OverrideJa2CampaignClockCalendar(
	std::uint32_t day,
	std::uint32_t hour,
	std::uint32_t minute) noexcept
{
	ActiveSession().overrideCalendar(day, hour, minute);
}
