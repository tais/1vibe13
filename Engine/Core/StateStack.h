#ifndef ENGINE_CORE_STATE_STACK_H
#define ENGINE_CORE_STATE_STACK_H

#include <cstddef>
#include <utility>
#include <vector>

template <typename State>
class StateStack
{
public:
	struct Entry
	{
		State state;
		bool overlay;
	};

	void reset(State state)
	{
		entries_.clear();
		entries_.push_back(Entry{std::move(state), false});
	}

	bool replace(State state)
	{
		if (entries_.empty())
		{
			reset(std::move(state));
			return true;
		}
		entries_.back() = Entry{std::move(state), false};
		return true;
	}

	bool pushOverlay(State state)
	{
		if (entries_.empty()) return false;
		entries_.push_back(Entry{std::move(state), true});
		return true;
	}

	bool popOverlay()
	{
		if (entries_.size() < 2 || !entries_.back().overlay) return false;
		entries_.pop_back();
		return true;
	}

	bool empty() const { return entries_.empty(); }
	std::size_t size() const { return entries_.size(); }
	const Entry* current() const { return entries_.empty() ? nullptr : &entries_.back(); }
	const Entry* underlay() const { return entries_.size() < 2 ? nullptr : &entries_[entries_.size() - 2]; }

private:
	std::vector<Entry> entries_;
};

#endif
