#include <Engine/Adapters/JA2/CampaignClockSession.h>

namespace
{
constexpr std::uint32_t SecondsPerMinute = 60;
constexpr std::uint32_t SecondsPerHour = 60 * SecondsPerMinute;
constexpr std::uint32_t SecondsPerDay = 24 * SecondsPerHour;
}

void CampaignClockSession::initialize(std::uint32_t startingSeconds) noexcept
{
	state_.totalSeconds = startingSeconds;
	state_.previousTotalSeconds = startingSeconds;
	updateCalendar();
}

void CampaignClockSession::advanceUncommitted(std::uint32_t seconds) noexcept
{
	// Unsigned wrap is deliberate: commitAdvance retains the legacy backwards-
	// time guard and clamps a wrapped clock to its previous checkpoint.
	state_.totalSeconds += seconds;
}

void CampaignClockSession::setEventTime(std::uint32_t totalSeconds) noexcept
{
	state_.totalSeconds = totalSeconds;
	updateCalendar();
}

CampaignClockSession::AdvanceCommit CampaignClockSession::commitAdvance() noexcept
{
	const AdvanceCommit result{
		state_.totalSeconds,
		state_.previousTotalSeconds,
		state_.totalSeconds < state_.previousTotalSeconds};
	if (result.movedBackward)
		state_.totalSeconds = state_.previousTotalSeconds;
	state_.previousTotalSeconds = state_.totalSeconds;
	updateCalendar();
	return result;
}

void CampaignClockSession::restoreSaved(
	std::uint32_t totalSeconds,
	std::uint32_t previousTotalSeconds) noexcept
{
	state_.totalSeconds = totalSeconds;
	state_.previousTotalSeconds = previousTotalSeconds;
	updateCalendar();
}

void CampaignClockSession::overrideCalendar(
	std::uint32_t day,
	std::uint32_t hour,
	std::uint32_t minute) noexcept
{
	state_.day = day;
	state_.hour = hour;
	state_.minute = minute;
}

void CampaignClockSession::updateCalendar() noexcept
{
	state_.day = state_.totalSeconds / SecondsPerDay;
	state_.hour =
		(state_.totalSeconds - state_.day * SecondsPerDay) / SecondsPerHour;
	state_.minute =
		(state_.totalSeconds -
			(state_.day * SecondsPerDay + state_.hour * SecondsPerHour)) /
		SecondsPerMinute;
}
