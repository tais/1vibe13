#include <Engine/Adapters/JA2/CampaignClockScheduler.h>

#include <algorithm>

CampaignClockScheduleResult CampaignClockScheduler::schedule(
	std::uint64_t elapsedMicroseconds,
	std::uint32_t gameSecondsPerRealSecond,
	std::uint8_t resolution) noexcept
{
	CampaignClockScheduleResult result;
	if (gameSecondsPerRealSecond == 0 || resolution == 0)
	{
		reset();
		result.error = CampaignClockScheduleError::Inactive;
		result.droppedElapsedMicroseconds = elapsedMicroseconds;
		return result;
	}
	if (resolution > MaximumResolution)
	{
		reset();
		result.error = CampaignClockScheduleError::InvalidResolution;
		result.droppedElapsedMicroseconds = elapsedMicroseconds;
		return result;
	}

	result.acceptedElapsedMicroseconds =
		std::min(elapsedMicroseconds, RealSecondMicroseconds);
	result.droppedElapsedMicroseconds =
		elapsedMicroseconds - result.acceptedElapsedMicroseconds;

	std::uint64_t remaining = result.acceptedElapsedMicroseconds;
	while (remaining != 0)
	{
		const std::uint64_t untilBoundary =
			RealSecondMicroseconds - elapsedWithinSecondMicroseconds_;
		const std::uint64_t consumed = std::min(remaining, untilBoundary);
		elapsedWithinSecondMicroseconds_ += consumed;
		remaining -= consumed;

		const std::uint64_t completedSlices =
			elapsedWithinSecondMicroseconds_ == RealSecondMicroseconds
				? resolution
				: elapsedWithinSecondMicroseconds_ * resolution /
					RealSecondMicroseconds;
		const std::uint64_t targetGameSeconds =
			static_cast<std::uint64_t>(gameSecondsPerRealSecond) *
				completedSlices /
			resolution;
		if (targetGameSeconds > emittedGameSecondsWithinSecond_)
		{
			result.advanceSeconds +=
				targetGameSeconds - emittedGameSecondsWithinSecond_;
			emittedGameSecondsWithinSecond_ = targetGameSeconds;
		}

		if (elapsedWithinSecondMicroseconds_ == RealSecondMicroseconds)
		{
			elapsedWithinSecondMicroseconds_ = 0;
			emittedGameSecondsWithinSecond_ = 0;
			++result.completedRealSeconds;
		}
	}
	return result;
}
