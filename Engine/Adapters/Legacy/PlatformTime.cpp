#include <Engine/Adapters/Legacy/PlatformTime.h>

#include <chrono>

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

MonotonicTimeSource& GetPlatformTimeSource()
{
	static SteadyPlatformTimeSource timeSource;
	return timeSource;
}
