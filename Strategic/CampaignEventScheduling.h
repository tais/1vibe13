#ifndef CAMPAIGN_EVENT_SCHEDULING_H
#define CAMPAIGN_EVENT_SCHEDULING_H

#include <Engine/Adapters/JA2/CampaignEventQueue.h>

enum class StrategicEventScheduleError : std::uint8_t
{
	None,
	TimestampOutOfRange,
	ProcessingTimeRejected,
	QueueFailure
};

struct StrategicEventScheduleResult
{
	CampaignEventQueueNode* event = nullptr;
	StrategicEventScheduleError error = StrategicEventScheduleError::None;
	CampaignEventQueueError queueError = CampaignEventQueueError::None;

	explicit operator bool() const noexcept
	{
		return event != nullptr && error == StrategicEventScheduleError::None;
	}
};

// Campaign-thread admission to the same runtime-owned queue as the legacy
// scheduler. These functions never assert, notify the UI, execute callbacks or
// advance the clock. Rejection preserves the queue and its identity sequence.
// Success returns a native node whose lifetime ends when the event is erased;
// it is not a durable transaction identity or a network replay key.
StrategicEventScheduleResult AddAdvancedStrategicEventChecked(
	std::uint8_t eventType, std::uint8_t callbackId,
	std::uint32_t seconds, std::uint32_t parameter) noexcept;
StrategicEventScheduleResult AddStrategicEventChecked(
	std::uint8_t callbackId, std::uint32_t minutes,
	std::uint32_t parameter) noexcept;
StrategicEventScheduleResult AddStrategicEventUsingSecondsChecked(
	std::uint8_t callbackId, std::uint32_t seconds,
	std::uint32_t parameter) noexcept;

#endif
