#include "PlatformInput.h"

#include <array>
#include <cstddef>
#include <mutex>

namespace
{
class MirroredInputSource final : public InputSource
{
public:
	bool poll(EngineInputEvent& event) override
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (count_ == 0) return false;
		event = events_[head_];
		event.droppedBefore = unreportedDrops_;
		unreportedDrops_ = 0;
		head_ = (head_ + 1) % capacity_;
		--count_;
		return true;
	}

	void publish(EngineInputEvent event) noexcept
	{
		try
		{
			std::lock_guard<std::mutex> lock(mutex_);
			event.sequence = nextSequence_++;
			if (count_ == capacity_)
			{
				head_ = (head_ + 1) % capacity_;
				--count_;
				++unreportedDrops_;
			}
			const std::size_t tail = (head_ + count_) % capacity_;
			events_[tail] = event;
			++count_;
		}
		catch (...)
		{
			// This observer must never interfere with the authoritative legacy
			// queue, even if the host cannot acquire its mirror lock.
		}
	}

	void reset() noexcept
	{
		try
		{
			std::lock_guard<std::mutex> lock(mutex_);
			head_ = 0;
			count_ = 0;
			nextSequence_ = 1;
			unreportedDrops_ = 0;
		}
		catch (...)
		{
		}
	}

private:
	static constexpr std::size_t capacity_ = 256;
	std::mutex mutex_;
	std::array<EngineInputEvent, capacity_> events_{};
	std::size_t head_ = 0;
	std::size_t count_ = 0;
	std::uint64_t nextSequence_ = 1;
	std::uint64_t unreportedDrops_ = 0;
};

MirroredInputSource& platformInputSource()
{
	static MirroredInputSource source;
	return source;
}
}

void PublishPlatformInputEvent(EngineInputEvent event) noexcept
{
	platformInputSource().publish(event);
}

void ResetPlatformInputEvents() noexcept
{
	platformInputSource().reset();
}

InputSource& GetPlatformInputSource()
{
	return platformInputSource();
}
