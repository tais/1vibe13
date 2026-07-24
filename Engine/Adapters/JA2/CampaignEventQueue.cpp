#include <Engine/Adapters/JA2/CampaignEventQueue.h>

#include <limits>
#include <new>
#include <utility>

namespace
{
void BreakCampaignEventCycle(CampaignEventQueueNode* head) noexcept
{
	CampaignEventQueueNode* slow = head;
	CampaignEventQueueNode* fast = head;
	do
	{
		if (!fast || !fast->next) return;
		slow = slow->next;
		fast = fast->next->next;
	} while (slow != fast);

	CampaignEventQueueNode* cycleStart = head;
	while (cycleStart != slow)
	{
		cycleStart = cycleStart->next;
		slow = slow->next;
	}
	CampaignEventQueueNode* cycleEnd = cycleStart;
	while (cycleEnd->next != cycleStart)
		cycleEnd = cycleEnd->next;
	cycleEnd->next = nullptr;
}
}

CampaignEventId CampaignEventQueue::issueIdentity() noexcept
{
	if (nextIdentity_ == 0) return {};
	const CampaignEventId issued{nextIdentity_};
	nextIdentity_ =
		nextIdentity_ == std::numeric_limits<std::uint64_t>::max()
			? 0
			: nextIdentity_ + 1;
	return issued;
}

CampaignEventScheduleResult CampaignEventQueue::schedule(
	const CampaignEventSnapshot& event) noexcept
{
	if (size_ >= maximumEvents_)
		return {nullptr, CampaignEventQueueError::CapacityReached};
	const CampaignEventId id = issueIdentity();
	if (!id)
		return {nullptr, CampaignEventQueueError::IdentityExhausted};

	CampaignEventQueueNode* node =
		new (std::nothrow) CampaignEventQueueNode(id, event);
	if (!node)
	{
		nextIdentity_ = id.value;
		return {nullptr, CampaignEventQueueError::AllocationFailure};
	}

	if (!head_)
	{
		head_ = node;
		tail_ = node;
		++size_;
		return {node, CampaignEventQueueError::None};
	}
	if (tail_->scheduledSeconds <= event.scheduledSeconds)
	{
		tail_->next = node;
		tail_ = node;
		++size_;
		return {node, CampaignEventQueueError::None};
	}

	CampaignEventQueueNode** insertion = &head_;
	while (*insertion &&
		(*insertion)->scheduledSeconds <= event.scheduledSeconds)
		insertion = &(*insertion)->next;
	node->next = *insertion;
	*insertion = node;
	++size_;
	return {node, CampaignEventQueueError::None};
}

CampaignEventQueueNode* CampaignEventQueue::eraseAfter(
	CampaignEventQueueNode* previous) noexcept
{
	CampaignEventQueueNode* removed = previous ? previous->next : head_;
	if (!removed) return nullptr;
	CampaignEventQueueNode* next = removed->next;
	if (previous)
		previous->next = next;
	else
		head_ = next;
	if (tail_ == removed) tail_ = previous;
	removed->next = nullptr;
	delete removed;
	--size_;
	if (size_ == 0)
	{
		head_ = nullptr;
		tail_ = nullptr;
	}
	return next;
}

CampaignEventQueueError CampaignEventQueue::erase(
	CampaignEventQueueNode* event) noexcept
{
	if (!event) return CampaignEventQueueError::InvalidNode;
	CampaignEventQueueNode* previous = nullptr;
	CampaignEventQueueNode* current = head_;
	while (current && current != event)
	{
		previous = current;
		current = current->next;
	}
	if (!current) return CampaignEventQueueError::InvalidNode;
	(void)eraseAfter(previous);
	return CampaignEventQueueError::None;
}

void CampaignEventQueue::clear() noexcept
{
	BreakCampaignEventCycle(head_);
	while (head_)
	{
		CampaignEventQueueNode* removed = head_;
		head_ = head_->next;
		removed->next = nullptr;
		delete removed;
	}
	tail_ = nullptr;
	size_ = 0;
}

CampaignEventQueueError CampaignEventQueue::appendOrdered(
	const CampaignEventSnapshot& event) noexcept
{
	if (size_ >= maximumEvents_)
		return CampaignEventQueueError::CapacityReached;
	const CampaignEventId id = issueIdentity();
	if (!id) return CampaignEventQueueError::IdentityExhausted;
	CampaignEventQueueNode* node =
		new (std::nothrow) CampaignEventQueueNode(id, event);
	if (!node)
	{
		nextIdentity_ = id.value;
		return CampaignEventQueueError::AllocationFailure;
	}
	if (tail_)
		tail_->next = node;
	else
		head_ = node;
	tail_ = node;
	++size_;
	return CampaignEventQueueError::None;
}

CampaignEventQueueError CampaignEventQueue::replace(
	const std::vector<CampaignEventSnapshot>& events) noexcept
{
	if (events.size() > maximumEvents_)
		return CampaignEventQueueError::CapacityReached;
	for (std::size_t index = 1; index < events.size(); ++index)
		if (events[index].scheduledSeconds <
			events[index - 1].scheduledSeconds)
			return CampaignEventQueueError::UnorderedInput;

	CampaignEventQueue accepted(maximumEvents_);
	accepted.nextIdentity_ = nextIdentity_;
	for (const CampaignEventSnapshot& event : events)
	{
		const CampaignEventQueueError result = accepted.appendOrdered(event);
		if (result != CampaignEventQueueError::None) return result;
	}
	swap(accepted);
	return CampaignEventQueueError::None;
}

bool CampaignEventQueue::capture(
	std::vector<CampaignEventSnapshot>& output) const noexcept
{
	try
	{
		std::vector<CampaignEventSnapshot> accepted;
		accepted.reserve(size_);
		const CampaignEventQueueNode* event = head_;
		std::size_t captured = 0;
		while (event && captured < size_)
		{
			accepted.push_back(event->snapshot());
			event = event->next;
			++captured;
		}
		if (event || accepted.size() != size_) return false;
		output.swap(accepted);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool CampaignEventQueue::validate() const noexcept
{
	std::size_t counted = 0;
	const CampaignEventQueueNode* previous = nullptr;
	const CampaignEventQueueNode* current = head_;
	while (current && counted <= size_)
	{
		if (!current->id) return false;
		if (previous &&
			current->scheduledSeconds < previous->scheduledSeconds)
			return false;
		previous = current;
		current = current->next;
		++counted;
	}
	return current == nullptr &&
		counted == size_ &&
		(size_ == 0 ? head_ == nullptr && tail_ == nullptr : previous == tail_) &&
		(!tail_ || tail_->next == nullptr);
}

void CampaignEventQueue::swap(CampaignEventQueue& other) noexcept
{
	using std::swap;
	swap(head_, other.head_);
	swap(tail_, other.tail_);
	swap(size_, other.size_);
	swap(maximumEvents_, other.maximumEvents_);
	swap(nextIdentity_, other.nextIdentity_);
}
