#include <Engine/Adapters/JA2/CampaignEventQueue.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

namespace { int failures = 0; long failAfter = -1; }
void* operator new(std::size_t n)
{
	if (failAfter == 0) { failAfter = -1; throw std::bad_alloc(); }
	if (failAfter > 0) --failAfter;
	if (void* p = std::malloc(n ? n : 1)) return p;
	throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{ try { return ::operator new(n); } catch (...) { return nullptr; } }
void operator delete(void* p, const std::nothrow_t&) noexcept { ::operator delete(p); }
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)

int main()
{
	using Error = CampaignEventQueueError;
	CampaignEventQueue queue(7);
	const auto first = queue.schedule({200, 1}), second = queue.schedule({300, 2}), third = queue.schedule({200, 3});
	CHECK(first && second && third, "existing stable events");
	const std::array<CampaignEventSnapshot, 4> batch{{{200, 4}, {100, 5}, {200, 6}, {400, 7}}};
	std::vector<CampaignEventSnapshot> before, after;
	CHECK(queue.capture(before), "capture original queue");
	const auto next = queue.nextIdentity();
	for (long allocation = 0; allocation < 4; ++allocation)
	{
		failAfter = allocation;
		const auto result = queue.scheduleBatch(batch.data(), batch.size());
		failAfter = -1;
		CHECK(result == Error::AllocationFailure && queue.nextIdentity() == next && queue.validate() &&
			queue.capture(after) && before == after && queue.head() == first.event && first.event->next == third.event &&
			third.event->next == second.event, "every preparation allocation failure preserves nodes, order, values and next ID");
	}
	CHECK(queue.scheduleBatch(nullptr, 0) == Error::None && queue.nextIdentity() == next, "empty batch is a no-op");
	CHECK(queue.scheduleBatch(nullptr, 1) == Error::InvalidNode && queue.nextIdentity() == next, "invalid input changes nothing");
	CHECK(queue.scheduleBatch(batch.data(), 5) == Error::CapacityReached && queue.nextIdentity() == next, "capacity checked before reading/allocating batch");
	const std::vector<CampaignEventSnapshot> expected{{100, 5}, {200, 1}, {200, 3}, {200, 4}, {200, 6}, {300, 2}, {400, 7}};
	CHECK(queue.scheduleBatch(batch.data(), batch.size()) == Error::None && queue.validate() && queue.capture(after) && after == expected,
		"batch merges by timestamp, existing-before-new FIFO and input-order ties");
	CHECK(queue.head()->id.value == next + 1 && first.event->id.value == 1 && second.event->id.value == 2 &&
		third.event->id.value == 3 && queue.nextIdentity() == next + 4, "original identities survive and batch identities follow submission order");
	CHECK(queue.erase(second.event) == Error::None && queue.validate(), "old event pointers remain usable after merge");
	const auto retained = queue.nextIdentity();
	auto* tail = queue.head();
	while (tail->next) tail = tail->next;
	tail->next = queue.head();
	CHECK(queue.scheduleBatch(batch.data(), 1) == Error::InvalidNode && queue.nextIdentity() == retained, "damaged live queue rejected before insertion");
	queue.clear();
	CHECK(queue.scheduleBatch(batch.data(), batch.size()) == Error::None && queue.validate() && queue.nextIdentity() == retained + 4,
		"batch also initializes an empty queue without identity reuse");
	return failures ? 1 : 0;
}
