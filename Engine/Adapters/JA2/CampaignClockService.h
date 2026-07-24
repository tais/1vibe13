#ifndef ENGINE_ADAPTERS_JA2_CAMPAIGN_CLOCK_SERVICE_H
#define ENGINE_ADAPTERS_JA2_CAMPAIGN_CLOCK_SERVICE_H

#include <Engine/Adapters/JA2/CampaignClockSession.h>
#include <Engine/Core/ServiceCatalog.h>

inline constexpr const char* CampaignClockServiceId = "ja2.campaign-clock";
inline constexpr EngineServiceVersion CampaignClockServiceVersion{1, 0};

enum class CampaignClockCaptureResult
{
	Success,
	Unavailable
};

// Versioned read-only strategic-time view for packages and tooling. Capture
// copies one pointer-free snapshot, so callers never retain a reference to the
// host session. Consumers should capture on the main-thread package boundary;
// an uncommitted event slice is visible when totalSeconds differs from the
// previousTotalSeconds checkpoint.
class CampaignClockService
{
public:
	virtual ~CampaignClockService() = default;
	virtual CampaignClockCaptureResult capture(
		CampaignClockSession::Snapshot& output) const noexcept = 0;
};

inline constexpr EngineServiceContract<CampaignClockService>
	CampaignClockServiceContract{
		CampaignClockServiceId, CampaignClockServiceVersion};

inline EngineServiceRegistrationError RegisterCampaignClockService(
	ServiceCatalog& catalog, CampaignClockService& service) noexcept
{
	return catalog.registerService(CampaignClockServiceContract, service);
}

// Stable adapter owned alongside its session by EngineRuntime.
class CampaignClockSessionService final : public CampaignClockService
{
public:
	explicit CampaignClockSessionService(
		const CampaignClockSession& session) noexcept
		: session_(&session)
	{
	}

	CampaignClockCaptureResult capture(
		CampaignClockSession::Snapshot& output) const noexcept override
	{
		output = session_->snapshot();
		return CampaignClockCaptureResult::Success;
	}

private:
	const CampaignClockSession* session_;
};

class NullCampaignClockService final : public CampaignClockService
{
public:
	static NullCampaignClockService& instance()
	{
		static NullCampaignClockService service;
		return service;
	}

	CampaignClockCaptureResult capture(
		CampaignClockSession::Snapshot&) const noexcept override
	{
		return CampaignClockCaptureResult::Unavailable;
	}

private:
	NullCampaignClockService() = default;
};

// Deterministic provider for package tests, replay tools, and hosts that do not
// run the JA2 application clock.
class MemoryCampaignClockService final : public CampaignClockService
{
public:
	void publish(CampaignClockSession::Snapshot snapshot) noexcept
	{
		snapshot_ = snapshot;
		available_ = true;
	}

	void clear() noexcept { available_ = false; }

	CampaignClockCaptureResult capture(
		CampaignClockSession::Snapshot& output) const noexcept override
	{
		if (!available_) return CampaignClockCaptureResult::Unavailable;
		output = snapshot_;
		return CampaignClockCaptureResult::Success;
	}

private:
	CampaignClockSession::Snapshot snapshot_;
	bool available_ = false;
};

#endif
