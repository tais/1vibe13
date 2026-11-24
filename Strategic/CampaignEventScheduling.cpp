#include "CampaignEventScheduling.h"

#include "CampaignEventAdapter.h"
#include "Game Events.h"
#include "Game Clock.h"

#include <limits>

// The native dispatcher in Game Events.cpp updates this state around its
// callback loop. Checked and legacy admission observe exactly the same guard.
BOOLEAN gfProcessingGameEvents = FALSE;
UINT32 guiTimeStampOfCurrentlyExecutingEvent = 0;

StrategicEventScheduleResult AddAdvancedStrategicEventChecked(
	std::uint8_t eventType, std::uint8_t callbackId,
	std::uint32_t seconds, std::uint32_t parameter) noexcept
{
	if (gfProcessingGameEvents &&
		seconds <= guiTimeStampOfCurrentlyExecutingEvent)
	{
		return {nullptr, StrategicEventScheduleError::ProcessingTimeRejected};
	}
	const CampaignEventScheduleResult scheduled =
		GetJa2CampaignEventQueue().schedule(CampaignEventSnapshot{
			seconds, parameter, 0, eventType, callbackId, 0});
	if (!scheduled)
	{
		return {nullptr, StrategicEventScheduleError::QueueFailure,
			scheduled.error};
	}
	return {scheduled.event};
}

StrategicEventScheduleResult AddStrategicEventChecked(
	std::uint8_t callbackId, std::uint32_t minutes,
	std::uint32_t parameter) noexcept
{
	if (minutes > std::numeric_limits<std::uint32_t>::max() / NUM_SEC_IN_MIN)
		return {nullptr, StrategicEventScheduleError::TimestampOutOfRange};
	return AddStrategicEventUsingSecondsChecked(
		callbackId, minutes * NUM_SEC_IN_MIN, parameter);
}

StrategicEventScheduleResult AddStrategicEventUsingSecondsChecked(
	std::uint8_t callbackId, std::uint32_t seconds,
	std::uint32_t parameter) noexcept
{
	return AddAdvancedStrategicEventChecked(
		ONETIME_EVENT, callbackId, seconds, parameter);
}
