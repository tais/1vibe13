#include "CampaignEventScheduling.h"
#include "CampaignEventAdapter.h"
#include "Game Events.h"
#include "message.h"

#include <cstdio>
#include <limits>
#include <vector>

extern BOOLEAN gfProcessingGameEvents;
extern UINT32 guiTimeStampOfCurrentlyExecutingEvent;

namespace
{
int failures = 0;
int assertionCalls = 0;
int notificationCalls = 0;

#define CHECK(condition, message) do { if (!(condition)) { \
	++failures; std::printf("FAIL %d: %s\n", __LINE__, message); } } while (0)

using Error = StrategicEventScheduleError;

// Swap only queue storage/capacity. All tests invoke production admission and
// the production adapter's currently bound queue; no scheduler is substituted.
class QueueFixture
{
public:
	explicit QueueFixture(std::size_t capacity)
		: saved_(capacity), processing_(gfProcessingGameEvents),
		  timestamp_(guiTimeStampOfCurrentlyExecutingEvent)
	{
		GetJa2CampaignEventQueue().swap(saved_);
		gfProcessingGameEvents = FALSE;
		guiTimeStampOfCurrentlyExecutingEvent = 0;
	}
	~QueueFixture()
	{
		GetJa2CampaignEventQueue().swap(saved_);
		gfProcessingGameEvents = processing_;
		guiTimeStampOfCurrentlyExecutingEvent = timestamp_;
	}
private:
	CampaignEventQueue saved_;
	BOOLEAN processing_;
	UINT32 timestamp_;
};

template<typename Schedule>
void RejectUnchanged(Schedule schedule, Error expected,
	CampaignEventQueueError queueError = CampaignEventQueueError::None)
{
	auto& queue = GetJa2CampaignEventQueue();
	std::vector<CampaignEventSnapshot> before, after;
	CHECK(queue.capture(before), "capture native queue before rejection");
	const auto head = queue.head();
	const auto identity = queue.nextIdentity();
	const auto size = queue.size();
	const auto processing = gfProcessingGameEvents;
	const auto timestamp = guiTimeStampOfCurrentlyExecutingEvent;
	const auto result = schedule();
	CHECK(!result && result.event == nullptr && result.error == expected &&
		result.queueError == queueError, "checked rejection has an exact reason");
	CHECK(queue.capture(after) && before == after && queue.head() == head &&
		queue.size() == size && queue.nextIdentity() == identity && queue.validate(),
		"rejection preserves native nodes, bytes, order and next identity");
	CHECK(gfProcessingGameEvents == processing &&
		guiTimeStampOfCurrentlyExecutingEvent == timestamp,
		"admission does not change the native processing guard");
	CHECK(assertionCalls == 0 && notificationCalls == 0,
		"checked rejection never asserts or issues screen notifications");
}

bool Exact(const StrategicEventScheduleResult& result, std::uint8_t type,
	std::uint8_t callback, std::uint32_t seconds, std::uint32_t parameter)
{
	return result && result.error == Error::None &&
		result.queueError == CampaignEventQueueError::None &&
		result.event->id.valid() && result.event->ubEventType == type &&
		result.event->ubCallbackID == callback &&
		result.event->uiTimeStamp == seconds && result.event->uiParam == parameter &&
		result.event->uiTimeOffset == 0 && result.event->ubFlags == 0;
}

void TestCapacityAndExactValues()
{
	{
		QueueFixture fixture(0);
		RejectUnchanged([] { return AddStrategicEventChecked(
			EVENT_DELAYED_HIRING_OF_MERC, 90, 57); }, Error::QueueFailure,
			CampaignEventQueueError::CapacityReached);
		RejectUnchanged([] { return AddStrategicEventUsingSecondsChecked(
			EVENT_GROUP_ARRIVAL, 123, 0xfedcba98); }, Error::QueueFailure,
			CampaignEventQueueError::CapacityReached);
		RejectUnchanged([] { return AddAdvancedStrategicEventChecked(
			RANGED_EVENT, EVENT_AMBIENT, 123, 9); }, Error::QueueFailure,
			CampaignEventQueueError::CapacityReached);
	}
	{
		QueueFixture fixture(1);
		auto& queue = GetJa2CampaignEventQueue();
		const auto result = AddStrategicEventChecked(
			EVENT_DELAYED_HIRING_OF_MERC, 90, 0xfedcba98);
		CHECK(Exact(result, ONETIME_EVENT, EVENT_DELAYED_HIRING_OF_MERC,
			5400, 0xfedcba98) && queue.head() == result.event && queue.size() == 1,
			"minutes schedule one exact delayed-hire event in the native queue");
		RejectUnchanged([] { return AddStrategicEventUsingSecondsChecked(
			EVENT_GROUP_ARRIVAL, 1, 9); }, Error::QueueFailure,
			CampaignEventQueueError::CapacityReached);
		CHECK(queue.erase(result.event) == CampaignEventQueueError::None,
			"native event removal releases queue capacity");
		const auto second = AddAdvancedStrategicEventChecked(
			RANGED_EVENT, EVENT_AMBIENT, 77, 0xffffffff);
		CHECK(Exact(second, RANGED_EVENT, EVENT_AMBIENT, 77, 0xffffffff) &&
			second.event->id.value == 2 && queue.validate(),
			"advanced seconds preserve type and payload; rejected append consumed no ID");
	}
}

void TestProcessingGuard()
{
	QueueFixture fixture(3);
	gfProcessingGameEvents = TRUE;
	guiTimeStampOfCurrentlyExecutingEvent = 3600;
	RejectUnchanged([] { return AddStrategicEventUsingSecondsChecked(
		EVENT_GROUP_ARRIVAL, 3599, 1); }, Error::ProcessingTimeRejected);
	RejectUnchanged([] { return AddAdvancedStrategicEventChecked(
		RANGED_EVENT, EVENT_AMBIENT, 3600, 2); }, Error::ProcessingTimeRejected);
	RejectUnchanged([] { return AddStrategicEventChecked(
		EVENT_DELAYED_HIRING_OF_MERC, 60, 3); }, Error::ProcessingTimeRejected);
	const auto future = AddStrategicEventUsingSecondsChecked(
		EVENT_GROUP_ARRIVAL, 3601, 4);
	CHECK(Exact(future, ONETIME_EVENT, EVENT_GROUP_ARRIVAL, 3601, 4),
		"native processing guard permits strictly later events");
	gfProcessingGameEvents = FALSE;
	const auto past = AddStrategicEventUsingSecondsChecked(EVENT_AMBIENT, 0, 5);
	CHECK(Exact(past, ONETIME_EVENT, EVENT_AMBIENT, 0, 5) &&
		GetJa2CampaignEventQueue().head() == past.event && past.event->next == future.event,
		"outside processing, native admission retains its historical past-time behavior");
	{
		QueueFixture full(0);
		gfProcessingGameEvents = TRUE;
		guiTimeStampOfCurrentlyExecutingEvent = 3600;
		RejectUnchanged([] { return AddStrategicEventChecked(
			EVENT_AMBIENT, 60, 6); }, Error::ProcessingTimeRejected);
		RejectUnchanged([] { return AddStrategicEventChecked(
			EVENT_AMBIENT, 61, 7); }, Error::QueueFailure,
			CampaignEventQueueError::CapacityReached);
	}
}

void TestTimestampBoundsAndFifo()
{
	QueueFixture fixture(4);
	constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
	constexpr auto lastMinute = maximum / 60;
	RejectUnchanged([] { return AddStrategicEventChecked(
		EVENT_AMBIENT, lastMinute + 1, 1); }, Error::TimestampOutOfRange);
	RejectUnchanged([] { return AddStrategicEventChecked(
		EVENT_AMBIENT, maximum, 2); }, Error::TimestampOutOfRange);
	const auto first = AddStrategicEventChecked(EVENT_AMBIENT, lastMinute, 3);
	const auto equal = AddStrategicEventUsingSecondsChecked(
		EVENT_GROUP_ARRIVAL, lastMinute * 60, 4);
	const auto last = AddStrategicEventUsingSecondsChecked(EVENT_AMBIENT, maximum, 5);
	const auto zero = AddStrategicEventChecked(EVENT_AMBIENT, 0, 6);
	CHECK(Exact(first, ONETIME_EVENT, EVENT_AMBIENT, lastMinute * 60, 3) &&
		Exact(equal, ONETIME_EVENT, EVENT_GROUP_ARRIVAL, lastMinute * 60, 4) &&
		Exact(last, ONETIME_EVENT, EVENT_AMBIENT, maximum, 5) &&
		Exact(zero, ONETIME_EVENT, EVENT_AMBIENT, 0, 6),
		"full seconds range and largest lossless minute value remain representable");
	CHECK(zero && first && equal && last &&
		GetJa2CampaignEventQueue().head() == zero.event &&
		zero.event->next == first.event && first.event->next == equal.event &&
		equal.event->next == last.event && last.event->next == nullptr &&
		GetJa2CampaignEventQueue().validate(),
		"real native queue orders timestamps and preserves equal-time FIFO");
}

void TestBoundQueue()
{
	QueueFixture fixture(1);
	auto& previous = GetJa2CampaignEventQueue();
	CampaignEventQueue runtimeQueue;
	BindJa2CampaignEventQueue(runtimeQueue);
	const auto accepted = AddStrategicEventChecked(EVENT_AMBIENT, 1, 7);
	CHECK(&GetJa2CampaignEventQueue() == &runtimeQueue &&
		runtimeQueue.head() == accepted.event && runtimeQueue.size() == 1 && previous.empty(),
		"checked admission follows production binding instead of a fallback queue");
	RejectUnchanged([] { return AddStrategicEventChecked(
		EVENT_AMBIENT, 2, 8); }, Error::QueueFailure,
		CampaignEventQueueError::CapacityReached);
	BindJa2CampaignEventQueue(previous);
	CHECK(previous.head() == accepted.event && runtimeQueue.empty(),
		"binding restoration keeps accepted native node identity");
}
}

// Notification sinks are the only substituted boundary. An accidental use of
// legacy failure reporting becomes a test failure; queue admission is real.
void _FailMessage(const char*, unsigned, const char*, const char*)
{
	++assertionCalls;
}
void ScreenMsg(UINT16, UINT8, STR16, ...)
{
	++notificationCalls;
}

int main()
{
	const auto sentinel = AddStrategicEventUsingSecondsChecked(EVENT_AMBIENT, 9, 19);
	CHECK(Exact(sentinel, ONETIME_EVENT, EVENT_AMBIENT, 9, 19), "seed original queue");
	const auto nextIdentity = GetJa2CampaignEventQueue().nextIdentity();
	TestCapacityAndExactValues();
	TestProcessingGuard();
	TestTimestampBoundsAndFifo();
	TestBoundQueue();
	CHECK(GetJa2CampaignEventQueue().head() == sentinel.event &&
		GetJa2CampaignEventQueue().size() == 1 &&
		GetJa2CampaignEventQueue().nextIdentity() == nextIdentity &&
		!gfProcessingGameEvents && guiTimeStampOfCurrentlyExecutingEvent == 0,
		"native fixtures restore queue storage, identity and processing state");
	CHECK(assertionCalls == 0 && notificationCalls == 0,
		"checked scheduling has no assertion or screen-notification side effects");
	std::printf("Native checked event scheduling: %d failure(s)\n", failures);
	return failures ? 1 : 0;
}
