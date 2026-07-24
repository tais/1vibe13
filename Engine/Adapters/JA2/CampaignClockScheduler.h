#ifndef ENGINE_ADAPTERS_JA2_CAMPAIGN_CLOCK_SCHEDULER_H
#define ENGINE_ADAPTERS_JA2_CAMPAIGN_CLOCK_SCHEDULER_H

#include <cstdint>

enum class CampaignClockScheduleError
{
	None,
	Inactive,
	InvalidResolution
};

struct CampaignClockScheduleResult
{
	CampaignClockScheduleError error = CampaignClockScheduleError::None;
	std::uint64_t advanceSeconds = 0;
	std::uint64_t acceptedElapsedMicroseconds = 0;
	std::uint64_t droppedElapsedMicroseconds = 0;
	std::uint32_t completedRealSeconds = 0;

	explicit operator bool() const noexcept
	{
		return error == CampaignClockScheduleError::None;
	}
};

// Deterministic real-time to campaign-time conversion for JA2's established
// speed/resolution model. The scheduler owns only fractional pacing state:
// strategic event execution and the authoritative campaign clock remain behind
// the application adapter.
//
// One call accepts at most one real second. The engine's fixed-step dispatcher
// normally supplies 16,667 microseconds; the explicit bound protects custom
// hosts from turning one misconfigured tick into an unbounded strategic update.
class CampaignClockScheduler
{
public:
	static constexpr std::uint64_t RealSecondMicroseconds = 1000000;
	static constexpr std::uint8_t MaximumResolution = 60;

	CampaignClockScheduleResult schedule(
		std::uint64_t elapsedMicroseconds,
		std::uint32_t gameSecondsPerRealSecond,
		std::uint8_t resolution) noexcept;

	void reset() noexcept
	{
		elapsedWithinSecondMicroseconds_ = 0;
		emittedGameSecondsWithinSecond_ = 0;
	}

	std::uint64_t elapsedWithinSecondMicroseconds() const noexcept
	{
		return elapsedWithinSecondMicroseconds_;
	}

	std::uint64_t emittedGameSecondsWithinSecond() const noexcept
	{
		return emittedGameSecondsWithinSecond_;
	}

private:
	std::uint64_t elapsedWithinSecondMicroseconds_ = 0;
	std::uint64_t emittedGameSecondsWithinSecond_ = 0;
};

#endif
