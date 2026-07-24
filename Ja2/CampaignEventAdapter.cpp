#include "CampaignEventAdapter.h"

#include "Game Events.h"

CampaignEventCaptureResult Ja2CampaignEventAdapter::capture(
	CampaignEventQueueSnapshot& output) noexcept
{
	try
	{
		eventScratch_.clear();
		const std::size_t initialCapacity =
			maximumEvents_ < 256 ? maximumEvents_ : 256;
		eventScratch_.reserve(initialCapacity);

		const STRATEGICEVENT* current = gpEventList;
		const STRATEGICEVENT* cycleSlow = gpEventList;
		const STRATEGICEVENT* cycleFast = gpEventList;
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
