#ifndef JA2_CAMPAIGN_EVENT_ADAPTER_H
#define JA2_CAMPAIGN_EVENT_ADAPTER_H

#include <cstddef>
#include <vector>

#include <Engine/Adapters/JA2/CampaignEventQueue.h>
#include <Engine/Adapters/JA2/CampaignEventService.h>

// Read-only production projection of JA2's runtime-owned strategic-event queue.
// Capture is intended for a main-thread package/frame boundary. It preserves
// equal-timestamp FIFO order and never exposes or mutates a host node.
class Ja2CampaignEventAdapter final : public CampaignEventService
{
public:
	explicit Ja2CampaignEventAdapter(
		std::size_t maximumEvents =
			CampaignEventQueueSnapshot::DefaultMaximumEvents)
		: maximumEvents_(maximumEvents)
	{
	}

	CampaignEventCaptureResult capture(
		CampaignEventQueueSnapshot& output) noexcept override;

private:
	std::size_t maximumEvents_;
	std::vector<CampaignEventSnapshot> eventScratch_;
};

Ja2CampaignEventAdapter& GetJa2CampaignEventAdapter();

// Application composition transfers any pre-context events from the fallback
// queue into EngineRuntime, then makes that runtime queue authoritative.
void BindJa2CampaignEventQueue(CampaignEventQueue& queue) noexcept;
CampaignEventQueue& GetJa2CampaignEventQueue() noexcept;

#endif
