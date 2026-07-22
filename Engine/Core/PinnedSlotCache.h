#ifndef ENGINE_CORE_PINNED_SLOT_CACHE_H
#define ENGINE_CORE_PINNED_SLOT_CACHE_H

#include <cstddef>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

// A fixed-capacity owner for resources addressed by compact numeric slots.
// Occupied slots are pinned until their final release, so insertion never
// evicts a live resource and always chooses the lowest available slot.
template <typename Resource, typename PinCount = std::size_t>
class PinnedSlotCache
{
	static_assert(std::is_integral<PinCount>::value,
		"PinnedSlotCache pin counters must be integral");

public:
	using Slot = std::size_t;

	enum class ReleaseResult
	{
		Invalid,
		Retained,
		Removed
	};

	explicit PinnedSlotCache(std::size_t capacity)
		: slots_(capacity)
	{
	}

	PinnedSlotCache(const PinnedSlotCache&) = delete;
	PinnedSlotCache& operator=(const PinnedSlotCache&) = delete;
	PinnedSlotCache(PinnedSlotCache&& other) noexcept
		: slots_(std::move(other.slots_)), size_(other.size_),
		  highWaterMark_(other.highWaterMark_)
	{
		other.size_ = 0;
		other.highWaterMark_ = 0;
	}

	PinnedSlotCache& operator=(PinnedSlotCache&& other) noexcept
	{
		if (this == &other) return *this;
		slots_ = std::move(other.slots_);
		size_ = other.size_;
		highWaterMark_ = other.highWaterMark_;
		other.size_ = 0;
		other.highWaterMark_ = 0;
		return *this;
	}

	std::optional<Slot> insert(Resource resource)
	{
		for (Slot slot = 0; slot < slots_.size(); ++slot)
		{
			if (slots_[slot]) continue;
			slots_[slot].emplace(std::move(resource), static_cast<PinCount>(1));
			++size_;
			if (slot >= highWaterMark_) highWaterMark_ = slot + 1;
			return slot;
		}
		return std::nullopt;
	}

	bool retain(Slot slot)
	{
		Entry* const entry = entryAt(slot);
		if (!entry || entry->pins == std::numeric_limits<PinCount>::max())
			return false;
		++entry->pins;
		return true;
	}

	ReleaseResult release(Slot slot)
	{
		Entry* const entry = entryAt(slot);
		if (!entry || entry->pins <= 0) return ReleaseResult::Invalid;
		if (entry->pins > 1)
		{
			--entry->pins;
			return ReleaseResult::Retained;
		}

		slots_[slot].reset();
		--size_;
		while (highWaterMark_ > 0 && !slots_[highWaterMark_ - 1])
			--highWaterMark_;
		return ReleaseResult::Removed;
	}

	Resource* find(Slot slot)
	{
		Entry* const entry = entryAt(slot);
		return entry ? &entry->resource : nullptr;
	}

	const Resource* find(Slot slot) const
	{
		const Entry* const entry = entryAt(slot);
		return entry ? &entry->resource : nullptr;
	}

	PinCount pins(Slot slot) const
	{
		const Entry* const entry = entryAt(slot);
		return entry ? entry->pins : 0;
	}

	void clear()
	{
		for (auto& slot : slots_) slot.reset();
		size_ = 0;
		highWaterMark_ = 0;
	}

	template <typename Visitor>
	void forEach(Visitor&& visitor)
	{
		for (Slot slot = 0; slot < highWaterMark_; ++slot)
		{
			Entry* const entry = entryAt(slot);
			if (entry) visitor(slot, entry->resource, entry->pins);
		}
	}

	template <typename Visitor>
	void forEach(Visitor&& visitor) const
	{
		for (Slot slot = 0; slot < highWaterMark_; ++slot)
		{
			const Entry* const entry = entryAt(slot);
			if (entry) visitor(slot, entry->resource, entry->pins);
		}
	}

	std::size_t size() const { return size_; }
	std::size_t capacity() const { return slots_.size(); }
	std::size_t highWaterMark() const { return highWaterMark_; }
	bool empty() const { return size_ == 0; }
	bool full() const { return size_ == slots_.size(); }

private:
	struct Entry
	{
		Entry(Resource value, PinCount count)
			: resource(std::move(value)), pins(count)
		{
		}

		Resource resource;
		PinCount pins;
	};

	Entry* entryAt(Slot slot)
	{
		return slot < slots_.size() && slots_[slot]
			? &*slots_[slot] : nullptr;
	}

	const Entry* entryAt(Slot slot) const
	{
		return slot < slots_.size() && slots_[slot]
			? &*slots_[slot] : nullptr;
	}

	std::vector<std::optional<Entry>> slots_;
	std::size_t size_ = 0;
	std::size_t highWaterMark_ = 0;
};

#endif
