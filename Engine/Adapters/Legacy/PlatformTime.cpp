#include <Engine/Adapters/Legacy/PlatformTime.h>

#include <atomic>
#include <chrono>
#include <limits>

class SteadyPlatformTimeSource final : public MonotonicTimeSource
{
public:
	std::uint64_t nowMicroseconds() const override
	{
		const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
		return static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
	}
};

namespace
{

SteadyPlatformTimeSource& DefaultTimeSource()
{
	static SteadyPlatformTimeSource timeSource;
	return timeSource;
}

std::atomic<MonotonicTimeSource*> gBoundTimeSource{nullptr};
}

void BindPlatformTimeSource(MonotonicTimeSource& source) noexcept
{
	gBoundTimeSource.store(&source, std::memory_order_release);
}

void ResetPlatformTimeSource() noexcept
{
	gBoundTimeSource.store(nullptr, std::memory_order_release);
}

MonotonicTimeSource& GetPlatformTimeSource() noexcept
{
	MonotonicTimeSource* const source =
		gBoundTimeSource.load(std::memory_order_acquire);
	return source ? *source : DefaultTimeSource();
}

std::uint64_t PlatformNowMicroseconds() noexcept
{
	try
	{
		return GetPlatformTimeSource().nowMicroseconds();
	}
	catch (...)
	{
		return DefaultTimeSource().nowMicroseconds();
	}
}

std::uint64_t PlatformNowMilliseconds() noexcept
{
	return PlatformNowMicroseconds() / 1000u;
}

std::uint64_t PlatformNowNanoseconds() noexcept
{
	const std::uint64_t microseconds = PlatformNowMicroseconds();
	constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
	return microseconds > maximum / 1000u ? maximum : microseconds * 1000u;
}
