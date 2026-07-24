#ifndef JA2_CAMPAIGN_SIMULATION_HOST_H
#define JA2_CAMPAIGN_SIMULATION_HOST_H

#include <cstdint>

#include <Engine/Adapters/JA2/CampaignClockScheduler.h>
#include <Engine/Core/SimulationTick.h>

struct CampaignSimulationDiagnostics
{
	std::uint64_t ticks = 0;
	std::uint64_t lastTickSequence = 0;
	std::uint64_t scheduledGameSeconds = 0;
	std::uint64_t droppedElapsedMicroseconds = 0;
	CampaignClockScheduleResult lastSchedule;
};

// Application bridge between the engine's fixed-step stream and JA2's
// strategic-event executor. Fractional pacing lives in EngineRuntime; this
// adapter only reads current legacy controls and invokes the authoritative
// event path at a deterministic tick boundary.
class CampaignSimulationHost final : public SimulationTickSink
{
public:
	explicit CampaignSimulationHost(CampaignClockScheduler& scheduler)
		: scheduler_(scheduler) {}

	void simulate(const SimulationTickContext& tick) override;

	const CampaignSimulationDiagnostics& diagnostics() const noexcept
	{
		return diagnostics_;
	}

private:
	CampaignClockScheduler& scheduler_;
	CampaignSimulationDiagnostics diagnostics_;
};

#endif
