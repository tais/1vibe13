#include "CampaignSimulationHost.h"

#include "types.h"
#include "Game Clock.h"

#include <limits>

namespace
{
std::uint64_t SaturatingAdd(std::uint64_t left, std::uint64_t right) noexcept
{
	return right > std::numeric_limits<std::uint64_t>::max() - left
		? std::numeric_limits<std::uint64_t>::max()
		: left + right;
}
}

void CampaignSimulationHost::simulate(const SimulationTickContext& tick)
{
	if (diagnostics_.ticks != std::numeric_limits<std::uint64_t>::max())
		++diagnostics_.ticks;
	diagnostics_.lastTickSequence = tick.sequence;
	diagnostics_.lastSchedule =
		AdvanceClockFromFixedStep(scheduler_, tick.stepMicroseconds);
	diagnostics_.scheduledGameSeconds = SaturatingAdd(
		diagnostics_.scheduledGameSeconds,
		diagnostics_.lastSchedule.advanceSeconds);
	diagnostics_.droppedElapsedMicroseconds = SaturatingAdd(
		diagnostics_.droppedElapsedMicroseconds,
		diagnostics_.lastSchedule.droppedElapsedMicroseconds);
}
