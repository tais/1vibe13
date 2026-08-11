#ifndef JA2_UTILS_LEGACY_CLOCK_SCHEDULER_H
#define JA2_UTILS_LEGACY_CLOCK_SCHEDULER_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace ja2::runtime_control
{
struct LegacyClockScheduleState
{
	std::uint64_t periodMicroseconds = 1;
	bool paused = false;
	bool fastForward = false;

	friend bool operator==(
		const LegacyClockScheduleState& left,
		const LegacyClockScheduleState& right) noexcept
	{
		return left.periodMicroseconds == right.periodMicroseconds &&
			left.paused == right.paused &&
			left.fastForward == right.fastForward;
	}
	friend bool operator!=(
		const LegacyClockScheduleState& left,
		const LegacyClockScheduleState& right) noexcept
	{
		return !(left == right);
	}
};

struct LegacyClockPumpResult
{
	std::uint32_t steps = 0;
	bool reanchored = false;
	bool discontinuity = false;
};

class LegacyClockScheduler
{
public:
	using StepConsumer = void (*)(void* context, bool paused);

	void clear() noexcept;
	void anchor(
		std::uint64_t nowMicroseconds,
		LegacyClockScheduleState state,
		bool immediateStep) noexcept;
	LegacyClockPumpResult pump(
		std::uint64_t nowMicroseconds,
		LegacyClockScheduleState currentState,
		StepConsumer consumer, void* context);
	LegacyClockPumpResult settleBeforeTransition(
		std::uint64_t nowMicroseconds,
		LegacyClockScheduleState currentState,
		StepConsumer consumer, void* context);

	std::uint64_t nextStepMicroseconds() const noexcept
	{
		return nextStepMicroseconds_;
	}
	std::uint64_t lastPumpMicroseconds() const noexcept
	{
		return lastPumpMicroseconds_;
	}
	const LegacyClockScheduleState& scheduledState() const noexcept
	{
		return scheduledState_;
	}
	bool hasQueuedDebt() const noexcept { return debtCount_ != 0; }
	std::uint64_t queuedDebtMicroseconds() const noexcept
	{
		return queuedDebtMicroseconds_;
	}

	static constexpr std::uint32_t maximumStepsPerPump() noexcept
	{
		return 100;
	}
	static constexpr std::uint64_t maximumRetainedDebtMicroseconds() noexcept
	{
		return 1000000;
	}

private:
	struct DebtSegment
	{
		std::uint64_t steps;
		std::uint64_t periodMicroseconds;
		bool paused;
	};

	static std::uint64_t saturatingAdd(
		std::uint64_t value, std::uint64_t increment) noexcept;
	static std::uint64_t saturatingMultiply(
		std::uint64_t value, std::uint64_t multiplier) noexcept;
	std::uint64_t scheduledStepsDue(std::uint64_t nowMicroseconds) const noexcept;
	bool queueDebt(
		std::uint64_t steps, std::uint64_t periodMicroseconds,
		bool paused);
	bool queueCurrentScheduleDebt(std::uint64_t nowMicroseconds);
	std::uint32_t processQueuedDebt(
		std::uint32_t limit, StepConsumer consumer, void* context);
	static void consumeSteps(
		std::uint32_t count, bool paused,
		StepConsumer consumer, void* context);

	static constexpr std::size_t maximumDebtSegments_ = 1024;
	std::array<DebtSegment, maximumDebtSegments_> debt_{};
	std::size_t debtHead_ = 0;
	std::size_t debtCount_ = 0;
	std::uint64_t queuedDebtMicroseconds_ = 0;
	std::uint64_t nextStepMicroseconds_ = 0;
	std::uint64_t lastPumpMicroseconds_ = 0;
	LegacyClockScheduleState scheduledState_{};
};
}

#endif
