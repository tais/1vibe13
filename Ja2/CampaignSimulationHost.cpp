#include "CampaignSimulationHost.h"

#include "types.h"
#include "Game Clock.h"
#include "DedicatedCoopRuntime.h"

#include <limits>
#include <cstdio>
#include <exception>
#include <stdexcept>

extern BOOLEAN gfDedicatedServerProcessFailed;
extern BOOLEAN gfProgramIsRunning;

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
	if (failed_) return;
	if (diagnostics_.ticks != std::numeric_limits<std::uint64_t>::max())
		++diagnostics_.ticks;
	diagnostics_.lastTickSequence = tick.sequence;
	try
	{
		diagnostics_.lastSchedule =
			AdvanceClockFromFixedStep(scheduler_, tick.stepMicroseconds);
	}
	catch (const std::exception&)
	{
		if (IsDedicatedCoopProcess()) fail(tick, "native campaign standard exception");
		throw;
	}
	catch (...)
	{
		if (IsDedicatedCoopProcess()) fail(tick, "unknown native campaign exception");
		throw;
	}
	diagnostics_.scheduledGameSeconds = SaturatingAdd(
		diagnostics_.scheduledGameSeconds,
		diagnostics_.lastSchedule.advanceSeconds);
	diagnostics_.droppedElapsedMicroseconds = SaturatingAdd(
		diagnostics_.droppedElapsedMicroseconds,
		diagnostics_.lastSchedule.droppedElapsedMicroseconds);
}

void CampaignSimulationHost::fail(const SimulationTickContext& tick, const char* reason) noexcept
{
	if (failed_) return;
	failed_ = true;
	gfDedicatedServerProcessFailed = TRUE;
	gfProgramIsRunning = FALSE;
	failureTickSequence_ = tick.sequence;
	std::snprintf(failureReason_.data(), failureReason_.size(), "%s",
		reason ? reason : "native campaign exception");
	// The event and clock may already be partially mutated. Do not repair,
	// retire or retry either; preserve the last durable checkpoint on teardown.
	InterruptTime();
	StopTimeCompression();
	PauseGame();
}

void CampaignSimulationHost::throwIfFailed() const
{
	if (failed_) throw std::runtime_error(failureReason_.data());
}
