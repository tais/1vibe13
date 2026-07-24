#ifndef ENGINE_ADAPTERS_JA2_CAMPAIGN_EVENT_SNAPSHOT_H
#define ENGINE_ADAPTERS_JA2_CAMPAIGN_EVENT_SNAPSHOT_H

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

// Pointer-free projection of one legacy STRATEGICEVENT. The numeric type,
// callback, and flag values remain opaque adapter data so mod-defined values
// do not require legacy headers or a new SDK contract.
struct CampaignEventSnapshot
{
	std::uint32_t scheduledSeconds = 0;
	std::uint32_t parameter = 0;
	std::uint32_t timeOffsetSeconds = 0;
	std::uint8_t type = 0;
	std::uint8_t callbackId = 0;
	std::uint8_t flags = 0;
};

inline bool operator==(
	const CampaignEventSnapshot& left,
	const CampaignEventSnapshot& right) noexcept
{
	return left.scheduledSeconds == right.scheduledSeconds &&
		left.parameter == right.parameter &&
		left.timeOffsetSeconds == right.timeOffsetSeconds &&
		left.type == right.type &&
		left.callbackId == right.callbackId &&
		left.flags == right.flags;
}

inline bool operator!=(
	const CampaignEventSnapshot& left,
	const CampaignEventSnapshot& right) noexcept
{
	return !(left == right);
}

enum class CampaignEventSnapshotCreateError
{
	None,
	TooManyEvents,
	UnorderedEvent
};

// Immutable queue view for packages, diagnostics, and headless tools. Equal
// timestamps retain their source order because JA2's event queue is FIFO at a
// given second. Construction validates that ordering and is transactional:
// rejected input leaves the caller's previous complete snapshot intact.
class CampaignEventQueueSnapshot
{
public:
	static constexpr std::size_t DefaultMaximumEvents = 65536;

	static CampaignEventSnapshotCreateError create(
		std::vector<CampaignEventSnapshot> events,
		CampaignEventQueueSnapshot& output,
		std::size_t maximumEvents = DefaultMaximumEvents)
	{
		const CampaignEventSnapshotCreateError validation =
			validate(events, maximumEvents);
		if (validation != CampaignEventSnapshotCreateError::None)
			return validation;

		CampaignEventQueueSnapshot accepted;
		accepted.events_ = std::move(events);
		output = std::move(accepted);
		return CampaignEventSnapshotCreateError::None;
	}

	// Live adapters keep their collection scratch while the output keeps its
	// own allocation. Reserve is the only throwing output operation and runs
	// before observable state changes; copies are required to be non-throwing.
	static CampaignEventSnapshotCreateError createReusableOrdered(
		const std::vector<CampaignEventSnapshot>& eventScratch,
		CampaignEventQueueSnapshot& output,
		std::size_t maximumEvents = DefaultMaximumEvents)
	{
		const CampaignEventSnapshotCreateError validation =
			validate(eventScratch, maximumEvents);
		if (validation != CampaignEventSnapshotCreateError::None)
			return validation;

		static_assert(
			std::is_nothrow_copy_constructible<CampaignEventSnapshot>::value,
			"reusable campaign-event capture requires non-throwing event copies");
		output.events_.reserve(eventScratch.size());
		output.events_.clear();
		for (const CampaignEventSnapshot& event : eventScratch)
			output.events_.push_back(event);
		return CampaignEventSnapshotCreateError::None;
	}

	// Copy into caller-owned reusable storage without exposing mutable queue
	// state. Allocation failure leaves all observable output values intact.
	bool copyTo(CampaignEventQueueSnapshot& output) const noexcept
	{
		if (&output == this) return true;
		try
		{
			static_assert(
				std::is_nothrow_copy_constructible<CampaignEventSnapshot>::value,
				"campaign-event copies require non-throwing event records");
			output.events_.reserve(events_.size());
			output.events_.clear();
			for (const CampaignEventSnapshot& event : events_)
				output.events_.push_back(event);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool empty() const noexcept { return events_.empty(); }
	std::size_t size() const noexcept { return events_.size(); }
	const std::vector<CampaignEventSnapshot>& events() const noexcept
	{
		return events_;
	}

private:
	static CampaignEventSnapshotCreateError validate(
		const std::vector<CampaignEventSnapshot>& events,
		std::size_t maximumEvents) noexcept
	{
		if (events.size() > maximumEvents)
			return CampaignEventSnapshotCreateError::TooManyEvents;
		for (std::size_t index = 1; index < events.size(); ++index)
			if (events[index].scheduledSeconds <
				events[index - 1].scheduledSeconds)
				return CampaignEventSnapshotCreateError::UnorderedEvent;
		return CampaignEventSnapshotCreateError::None;
	}

	std::vector<CampaignEventSnapshot> events_;
};

#endif
