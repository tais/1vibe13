#include "PlatformInput.h"

#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

namespace
{
class MirroredInputSource final : public InputSource
{
public:
	bool poll(EngineInputEvent& event) override
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (events_.empty()) return false;
		event = std::move(events_.front());
		events_.pop_front();
		return true;
	}

	void publish(EngineInputEvent event)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		// Keep this observer bounded even while no engine/package consumer is
		// active. A continuously polling runtime sees every accepted event;
		// a late consumer receives the newest window instead of stale startup
		// input or unbounded memory growth.
		if (events_.size() == capacity_) events_.pop_front();
		events_.push_back(std::move(event));
	}

private:
	static constexpr std::size_t capacity_ = 256;
	std::mutex mutex_;
	std::deque<EngineInputEvent> events_;
};

MirroredInputSource& platformInputSource()
{
	static MirroredInputSource source;
	return source;
}
}

void PublishPlatformInputEvent(EngineInputEvent event)
{
	platformInputSource().publish(std::move(event));
}

InputSource& GetPlatformInputSource()
{
	return platformInputSource();
}
