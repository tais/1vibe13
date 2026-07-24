#ifndef ENGINE_ADAPTERS_JA2_CAMPAIGN_EVENT_SERVICE_H
#define ENGINE_ADAPTERS_JA2_CAMPAIGN_EVENT_SERVICE_H

#include <utility>

#include <Engine/Adapters/JA2/CampaignEventSnapshot.h>
#include <Engine/Core/ServiceCatalog.h>

inline constexpr const char* CampaignEventServiceId = "ja2.campaign-events";
inline constexpr EngineServiceVersion CampaignEventServiceVersion{1, 0};

enum class CampaignEventCaptureResult
{
	Success,
	Unavailable,
	CapacityReached,
	AllocationFailure,
	AdapterFailure
};

// Versioned read-only host service for packages and tooling. Implementations
// replace output only after a complete capture, so consumers retain their last
// good queue view if the live adapter encounters corruption or allocation
// pressure. Capture belongs on the main-thread package/frame boundary.
class CampaignEventService
{
public:
	virtual ~CampaignEventService() = default;
	virtual CampaignEventCaptureResult capture(
		CampaignEventQueueSnapshot& output) noexcept = 0;
};

inline constexpr EngineServiceContract<CampaignEventService>
	CampaignEventServiceContract{
		CampaignEventServiceId, CampaignEventServiceVersion};

inline EngineServiceRegistrationError RegisterCampaignEventService(
	ServiceCatalog& catalog, CampaignEventService& service) noexcept
{
	return catalog.registerService(CampaignEventServiceContract, service);
}

class NullCampaignEventService final : public CampaignEventService
{
public:
	static NullCampaignEventService& instance()
	{
		static NullCampaignEventService service;
		return service;
	}

	CampaignEventCaptureResult capture(
		CampaignEventQueueSnapshot&) noexcept override
	{
		return CampaignEventCaptureResult::Unavailable;
	}

private:
	NullCampaignEventService() = default;
};

// Deterministic provider used by package tests, replay tools, and hosts that do
// not run JA2's legacy strategic-event dispatcher.
class MemoryCampaignEventService final : public CampaignEventService
{
public:
	void publish(CampaignEventQueueSnapshot snapshot) noexcept
	{
		snapshot_ = std::move(snapshot);
		available_ = true;
	}

	void clear() noexcept { available_ = false; }

	CampaignEventCaptureResult capture(
		CampaignEventQueueSnapshot& output) noexcept override
	{
		if (!available_) return CampaignEventCaptureResult::Unavailable;
		return snapshot_.copyTo(output)
			? CampaignEventCaptureResult::Success
			: CampaignEventCaptureResult::AllocationFailure;
	}

private:
	CampaignEventQueueSnapshot snapshot_;
	bool available_ = false;
};

#endif
