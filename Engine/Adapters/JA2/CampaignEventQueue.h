#ifndef ENGINE_ADAPTERS_JA2_CAMPAIGN_EVENT_QUEUE_H
#define ENGINE_ADAPTERS_JA2_CAMPAIGN_EVENT_QUEUE_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/CampaignEventSnapshot.h>

struct CampaignEventId
{
	std::uint64_t value = 0;

	bool valid() const noexcept { return value != 0; }
	explicit operator bool() const noexcept { return valid(); }
};

inline bool operator==(CampaignEventId left, CampaignEventId right) noexcept
{
	return left.value == right.value;
}

inline bool operator!=(CampaignEventId left, CampaignEventId right) noexcept
{
	return !(left == right);
}

// Stable host node owned exclusively by CampaignEventQueue. The modern and
// legacy field names share storage so existing JA2 callback code can keep its
// STRATEGICEVENT source surface while allocation, ordering, and destruction
// move into EngineRuntime. Packages must use CampaignEventService instead.
struct CampaignEventQueueNode
{
	CampaignEventQueueNode() noexcept
		: next(nullptr),
		  scheduledSeconds(0),
		  parameter(0),
		  timeOffsetSeconds(0),
		  type(0),
		  callbackId(0),
		  flags(0)
	{
	}

	CampaignEventQueueNode(
		CampaignEventId eventId,
		const CampaignEventSnapshot& event) noexcept
		: next(nullptr),
		  id(eventId),
		  scheduledSeconds(event.scheduledSeconds),
		  parameter(event.parameter),
		  timeOffsetSeconds(event.timeOffsetSeconds),
		  type(event.type),
		  callbackId(event.callbackId),
		  flags(event.flags)
	{
	}

	CampaignEventSnapshot snapshot() const noexcept
	{
		return CampaignEventSnapshot{
			scheduledSeconds,
			parameter,
			timeOffsetSeconds,
			type,
			callbackId,
			flags};
	}

	CampaignEventQueueNode* next;
	CampaignEventId id;
	union
	{
		std::uint32_t scheduledSeconds;
		std::uint32_t uiTimeStamp;
	};
	union
	{
		std::uint32_t parameter;
		std::uint32_t uiParam;
	};
	union
	{
		std::uint32_t timeOffsetSeconds;
		std::uint32_t uiTimeOffset;
	};
	union
	{
		std::uint8_t type;
		std::uint8_t ubEventType;
	};
	union
	{
		std::uint8_t callbackId;
		std::uint8_t ubCallbackID;
	};
	union
	{
		std::uint8_t flags;
		std::uint8_t ubFlags;
	};
};

enum class CampaignEventQueueError
{
	None,
	CapacityReached,
	AllocationFailure,
	IdentityExhausted,
	UnorderedInput,
	InvalidNode
};

struct CampaignEventScheduleResult
{
	CampaignEventQueueNode* event = nullptr;
	CampaignEventQueueError error = CampaignEventQueueError::None;

	explicit operator bool() const noexcept
	{
		return event != nullptr && error == CampaignEventQueueError::None;
	}
};

// Engine-owned deterministic strategic queue. Nodes are stable until erased,
// equal timestamps are FIFO, IDs never repeat within one runtime session, and
// replacement is transactional. The class deliberately contains no callback,
// platform, save-file, or JA2-global dependency.
class CampaignEventQueue
{
public:
	static constexpr std::size_t DefaultMaximumEvents =
		CampaignEventQueueSnapshot::DefaultMaximumEvents;

	explicit CampaignEventQueue(
		std::size_t maximumEvents = DefaultMaximumEvents) noexcept
		: maximumEvents_(maximumEvents)
	{
	}

	~CampaignEventQueue() { clear(); }

	CampaignEventQueue(const CampaignEventQueue&) = delete;
	CampaignEventQueue& operator=(const CampaignEventQueue&) = delete;
	CampaignEventQueue(CampaignEventQueue&&) = delete;
	CampaignEventQueue& operator=(CampaignEventQueue&&) = delete;

	CampaignEventScheduleResult schedule(
		const CampaignEventSnapshot& event) noexcept;

	// Removes the node following previous, or the head when previous is null,
	// and returns the following live node. Callers use this while traversing
	// without retaining freed pointers.
	CampaignEventQueueNode* eraseAfter(
		CampaignEventQueueNode* previous) noexcept;
	CampaignEventQueueError erase(CampaignEventQueueNode* event) noexcept;
	void clear() noexcept;

	CampaignEventQueueError replace(
		const std::vector<CampaignEventSnapshot>& events) noexcept;
	bool capture(std::vector<CampaignEventSnapshot>& output) const noexcept;
	bool validate() const noexcept;
	void swap(CampaignEventQueue& other) noexcept;

	CampaignEventQueueNode* head() noexcept { return head_; }
	const CampaignEventQueueNode* head() const noexcept { return head_; }
	std::size_t size() const noexcept { return size_; }
	bool empty() const noexcept { return size_ == 0; }
	std::size_t maximumEvents() const noexcept { return maximumEvents_; }
	std::uint64_t nextIdentity() const noexcept { return nextIdentity_; }

private:
	CampaignEventQueueError appendOrdered(
		const CampaignEventSnapshot& event) noexcept;
	CampaignEventId issueIdentity() noexcept;

	CampaignEventQueueNode* head_ = nullptr;
	CampaignEventQueueNode* tail_ = nullptr;
	std::size_t size_ = 0;
	std::size_t maximumEvents_;
	std::uint64_t nextIdentity_ = 1;
};

#endif
