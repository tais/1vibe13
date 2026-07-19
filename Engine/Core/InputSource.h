#ifndef ENGINE_CORE_INPUT_SOURCE_H
#define ENGINE_CORE_INPUT_SOURCE_H

#include <cstdint>
#include <deque>
#include <utility>

// Stable engine-side representation of an input atom. Event codes and payloads
// are intentionally numeric while the legacy bridge is in place; a future API
// major may replace them with versioned action identifiers.
struct EngineInputEvent
{
	std::uint64_t timestamp = 0;
	std::uint32_t modifiers = 0;
	std::uint32_t type = 0;
	std::uint32_t primary = 0;
	std::uint32_t secondary = 0;
	std::uint64_t sequence = 0;
	std::uint64_t droppedBefore = 0;
};

class InputSource
{
public:
	virtual ~InputSource() = default;
	virtual bool poll(EngineInputEvent& event) = 0;
};

class NullInputSource final : public InputSource
{
public:
	bool poll(EngineInputEvent&) override { return false; }
	static NullInputSource& instance()
	{
		static NullInputSource source;
		return source;
	}
};

// Deterministic input source for headless hosts, replays, and package tests.
class MemoryInputSource final : public InputSource
{
public:
	void push(EngineInputEvent event) { events_.push_back(std::move(event)); }
	bool poll(EngineInputEvent& event) override
	{
		if (events_.empty()) return false;
		event = events_.front();
		events_.pop_front();
		return true;
	}
	bool empty() const { return events_.empty(); }

private:
	std::deque<EngineInputEvent> events_;
};

#endif
