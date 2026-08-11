#include "LegacyClockScheduler.h"

#include <algorithm>
#include <limits>

namespace ja2::runtime_control
{
std::uint64_t LegacyClockScheduler::saturatingAdd(
	std::uint64_t value, std::uint64_t increment) noexcept
{
	return value > std::numeric_limits<std::uint64_t>::max() - increment
		? std::numeric_limits<std::uint64_t>::max()
		: value + increment;
}

std::uint64_t LegacyClockScheduler::saturatingMultiply(
	std::uint64_t value, std::uint64_t multiplier) noexcept
{
	if (value == 0 || multiplier == 0) return 0;
	return value > std::numeric_limits<std::uint64_t>::max() / multiplier
		? std::numeric_limits<std::uint64_t>::max()
		: value * multiplier;
}

void LegacyClockScheduler::clear() noexcept
{
	debtHead_ = 0;
	debtCount_ = 0;
	queuedDebtMicroseconds_ = 0;
	nextStepMicroseconds_ = 0;
	lastPumpMicroseconds_ = 0;
	scheduledState_ = {};
}

void LegacyClockScheduler::anchor(
	std::uint64_t nowMicroseconds,
	LegacyClockScheduleState state,
	bool immediateStep) noexcept
{
	if (state.periodMicroseconds == 0) state.periodMicroseconds = 1;
	scheduledState_ = state;
	nextStepMicroseconds_ = immediateStep
		? nowMicroseconds
		: saturatingAdd(nowMicroseconds, state.periodMicroseconds);
	lastPumpMicroseconds_ = nowMicroseconds;
}

std::uint64_t LegacyClockScheduler::scheduledStepsDue(
	std::uint64_t nowMicroseconds) const noexcept
{
	if (nowMicroseconds <= nextStepMicroseconds_) return 0;
	const std::uint64_t elapsed = nowMicroseconds - nextStepMicroseconds_;
	return 1u + (elapsed - 1u) / scheduledState_.periodMicroseconds;
}

bool LegacyClockScheduler::queueDebt(
	std::uint64_t steps, std::uint64_t periodMicroseconds,
	bool paused)
{
	if (steps == 0) return true;
	const std::uint64_t duration = saturatingMultiply(steps, periodMicroseconds);
	if (duration > maximumRetainedDebtMicroseconds() ||
		queuedDebtMicroseconds_ >
			maximumRetainedDebtMicroseconds() - duration)
		return false;

	const std::size_t backIndex = debtCount_ == 0 ? 0 :
		(debtHead_ + debtCount_ - 1) % maximumDebtSegments_;
	if (debtCount_ != 0 &&
		debt_[backIndex].periodMicroseconds == periodMicroseconds &&
		debt_[backIndex].paused == paused)
	{
		debt_[backIndex].steps = saturatingAdd(debt_[backIndex].steps, steps);
	}
	else
	{
		if (debtCount_ >= maximumDebtSegments_) return false;
		const std::size_t tail =
			(debtHead_ + debtCount_) % maximumDebtSegments_;
		debt_[tail] = DebtSegment{steps, periodMicroseconds, paused};
		++debtCount_;
	}
	queuedDebtMicroseconds_ += duration;
	return true;
}

void LegacyClockScheduler::consumeSteps(
	std::uint32_t count, bool paused,
	StepConsumer consumer, void* context)
{
	if (consumer == nullptr) return;
	for (std::uint32_t step = 0; step < count; ++step)
		consumer(context, paused);
}

std::uint32_t LegacyClockScheduler::processQueuedDebt(
	std::uint32_t limit, StepConsumer consumer, void* context)
{
	std::uint32_t processed = 0;
	while (processed < limit && debtCount_ != 0)
	{
		DebtSegment& segment = debt_[debtHead_];
		const std::uint64_t available =
			static_cast<std::uint64_t>(limit - processed);
		const std::uint32_t count = static_cast<std::uint32_t>(
			std::min(segment.steps, available));
		consumeSteps(count, segment.paused, consumer, context);
		segment.steps -= count;
		processed += count;
		queuedDebtMicroseconds_ -=
			static_cast<std::uint64_t>(count) * segment.periodMicroseconds;
		if (segment.steps == 0)
		{
			debtHead_ = (debtHead_ + 1) % maximumDebtSegments_;
			--debtCount_;
		}
	}
	return processed;
}

bool LegacyClockScheduler::queueCurrentScheduleDebt(
	std::uint64_t nowMicroseconds)
{
	const std::uint64_t steps = scheduledStepsDue(nowMicroseconds);
	if (!queueDebt(steps, scheduledState_.periodMicroseconds,
		scheduledState_.paused))
		return false;
	nextStepMicroseconds_ = saturatingAdd(
		nextStepMicroseconds_,
		saturatingMultiply(steps, scheduledState_.periodMicroseconds));
	return true;
}

LegacyClockPumpResult LegacyClockScheduler::pump(
	std::uint64_t nowMicroseconds,
	LegacyClockScheduleState currentState,
	StepConsumer consumer, void* context)
{
	if (currentState.periodMicroseconds == 0)
		currentState.periodMicroseconds = 1;
	LegacyClockPumpResult result;
	if (nowMicroseconds < lastPumpMicroseconds_)
	{
		debtHead_ = 0;
		debtCount_ = 0;
		queuedDebtMicroseconds_ = 0;
		anchor(nowMicroseconds, currentState, false);
		result.reanchored = true;
		result.discontinuity = true;
		return result;
	}

	const std::uint64_t liveStepsDue = scheduledStepsDue(nowMicroseconds);
	const std::uint64_t forwardJump = nowMicroseconds - lastPumpMicroseconds_;
	const std::uint64_t liveRetainedDebt = liveStepsDue > 0
		? nowMicroseconds - nextStepMicroseconds_ : 0;
	const std::uint64_t retainedDebt = saturatingAdd(
		queuedDebtMicroseconds_, liveRetainedDebt);
	if (forwardJump > maximumRetainedDebtMicroseconds() ||
		retainedDebt > maximumRetainedDebtMicroseconds())
	{
		result.steps = processQueuedDebt(1, consumer, context);
		if (result.steps == 0 && liveStepsDue > 0)
		{
			consumeSteps(1, scheduledState_.paused, consumer, context);
			result.steps = 1;
		}
		debtHead_ = 0;
		debtCount_ = 0;
		queuedDebtMicroseconds_ = 0;
		anchor(nowMicroseconds, currentState, false);
		result.reanchored = true;
		result.discontinuity = true;
		return result;
	}

	result.steps = processQueuedDebt(
		maximumStepsPerPump(), consumer, context);
	if (result.steps < maximumStepsPerPump() && liveStepsDue > 0)
	{
		const std::uint32_t liveSteps = static_cast<std::uint32_t>(
			std::min<std::uint64_t>(
				liveStepsDue, maximumStepsPerPump() - result.steps));
		consumeSteps(liveSteps, scheduledState_.paused, consumer, context);
		nextStepMicroseconds_ = saturatingAdd(
			nextStepMicroseconds_,
			saturatingMultiply(liveSteps, scheduledState_.periodMicroseconds));
		result.steps += liveSteps;
	}

	lastPumpMicroseconds_ = nowMicroseconds;
	if (currentState != scheduledState_)
	{
		if (!queueCurrentScheduleDebt(nowMicroseconds))
		{
			debtHead_ = 0;
			debtCount_ = 0;
			queuedDebtMicroseconds_ = 0;
		}
		anchor(nowMicroseconds, currentState, false);
		result.reanchored = true;
	}
	return result;
}

LegacyClockPumpResult LegacyClockScheduler::settleBeforeTransition(
	std::uint64_t nowMicroseconds,
	LegacyClockScheduleState currentState,
	StepConsumer consumer, void* context)
{
	LegacyClockPumpResult result = pump(
		nowMicroseconds, currentState, consumer, context);
	if (!queueCurrentScheduleDebt(nowMicroseconds))
	{
		debtHead_ = 0;
		debtCount_ = 0;
		queuedDebtMicroseconds_ = 0;
	}
	return result;
}
}
