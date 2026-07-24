#include "CampaignEventAdapter.h"

namespace
{
CampaignEventQueue fallbackQueue;
CampaignEventQueue* activeQueue = &fallbackQueue;
}

CampaignEventCaptureResult Ja2CampaignEventAdapter::capture(
	CampaignEventQueueSnapshot& output) noexcept
{
	try
	{
		eventScratch_.clear();
		const std::size_t initialCapacity =
			maximumEvents_ < 256 ? maximumEvents_ : 256;
		eventScratch_.reserve(initialCapacity);

		const CampaignEventQueue& queue = GetJa2CampaignEventQueue();
		const CampaignEventQueueNode* current = queue.head();
		const CampaignEventQueueNode* cycleSlow = queue.head();
		const CampaignEventQueueNode* cycleFast = queue.head();
		while (current)
		{
			if (eventScratch_.size() >= maximumEvents_)
				return CampaignEventCaptureResult::CapacityReached;
			eventScratch_.push_back(CampaignEventSnapshot{
				current->uiTimeStamp,
				current->uiParam,
				current->uiTimeOffset,
				current->ubEventType,
				current->ubCallbackID,
				current->ubFlags});
			current = current->next;

			// Reject a cyclic or self-linked legacy queue without allocating a
			// visited-node set. The output remains the caller's last good view.
			if (cycleFast && cycleFast->next)
			{
				cycleSlow = cycleSlow->next;
				cycleFast = cycleFast->next->next;
				if (cycleSlow == cycleFast)
					return CampaignEventCaptureResult::AdapterFailure;
			}
			else
			{
				cycleFast = nullptr;
			}
		}

		const CampaignEventSnapshotCreateError result =
			CampaignEventQueueSnapshot::createReusableOrdered(
				eventScratch_, output, maximumEvents_);
		if (result == CampaignEventSnapshotCreateError::TooManyEvents)
			return CampaignEventCaptureResult::CapacityReached;
		if (result != CampaignEventSnapshotCreateError::None)
			return CampaignEventCaptureResult::AdapterFailure;
		return CampaignEventCaptureResult::Success;
	}
	catch (...)
	{
		return CampaignEventCaptureResult::AllocationFailure;
	}
}

Ja2CampaignEventAdapter& GetJa2CampaignEventAdapter()
{
	static Ja2CampaignEventAdapter adapter;
	return adapter;
}

void BindJa2CampaignEventQueue(CampaignEventQueue& queue) noexcept
{
	if (activeQueue != &queue)
	{
		queue.swap(*activeQueue);
		activeQueue = &queue;
	}
}

CampaignEventQueue& GetJa2CampaignEventQueue() noexcept
{
	return *activeQueue;
}
