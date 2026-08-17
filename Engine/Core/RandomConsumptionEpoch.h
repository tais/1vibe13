#ifndef ENGINE_CORE_RANDOM_CONSUMPTION_EPOCH_H
#define ENGINE_CORE_RANDOM_CONSUMPTION_EPOCH_H

#include <atomic>
#include <cstdint>
#include <limits>

// A process-lifetime observation stamp for random consumption. Unlike a
// serializable RNG checkpoint, this value only moves forward: restoring a
// checkpoint must not make a draw-and-restore sequence invisible to a runtime
// transaction guard.
class NonRewindableRandomEpoch final
{
public:
	explicit NonRewindableRandomEpoch(
		std::uint64_t initial = 0) noexcept : value_(initial)
	{
	}

	NonRewindableRandomEpoch(const NonRewindableRandomEpoch&) = delete;
	NonRewindableRandomEpoch& operator=(
		const NonRewindableRandomEpoch&) = delete;
	NonRewindableRandomEpoch(NonRewindableRandomEpoch&&) = delete;
	NonRewindableRandomEpoch& operator=(
		NonRewindableRandomEpoch&&) = delete;

	std::uint64_t value() const noexcept
	{
		return value_.load(std::memory_order_acquire);
	}

	bool tryAdvance(std::uint64_t count = 1) noexcept
	{
		if (count == 0) return true;

		std::uint64_t current = value_.load(std::memory_order_acquire);
		for (;;)
		{
			if (count > std::numeric_limits<std::uint64_t>::max() - current)
				return false;
			if (value_.compare_exchange_weak(current, current + count,
				std::memory_order_acq_rel, std::memory_order_acquire))
				return true;
		}
	}

private:
	std::atomic<std::uint64_t> value_;
};

#endif
