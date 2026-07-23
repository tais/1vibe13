#include <Engine/Adapters/Legacy/LegacyFrameInvalidationGateway.h>

#include <Engine/Adapters/Legacy/PlatformFrameInvalidator.h>

#include "video.h"

#include <atomic>

namespace
{
std::atomic<FrameInvalidator*> gBoundInvalidator{nullptr};
thread_local bool gInvalidating = false;

class InvalidationGuard
{
public:
	InvalidationGuard() noexcept
		: acquired_(!gInvalidating)
	{
		if (acquired_) gInvalidating = true;
	}

	~InvalidationGuard()
	{
		if (acquired_) gInvalidating = false;
	}

	bool acquired() const noexcept { return acquired_; }

private:
	bool acquired_;
};

template <typename Callback>
bool SubmitInvalidation(Callback&& callback) noexcept
{
	InvalidationGuard guard;
	if (!guard.acquired()) return false;
	try
	{
		callback(GetLegacyFrameInvalidator());
		return true;
	}
	catch (...)
	{
		return false;
	}
}
}

void BindLegacyFrameInvalidator(FrameInvalidator& invalidator) noexcept
{
	gBoundInvalidator.store(&invalidator, std::memory_order_release);
}

void ResetLegacyFrameInvalidator() noexcept
{
	gBoundInvalidator.store(nullptr, std::memory_order_release);
}

FrameInvalidator& GetLegacyFrameInvalidator() noexcept
{
	FrameInvalidator* const invalidator =
		gBoundInvalidator.load(std::memory_order_acquire);
	return invalidator ? *invalidator : GetPlatformFrameInvalidator();
}

bool InvalidateLegacyFrameRegion(FrameRegion region) noexcept
{
	return SubmitInvalidation(
		[region](FrameInvalidator& invalidator) {
			invalidator.invalidateRegion(region);
		});
}

bool InvalidateLegacyFrameAll() noexcept
{
	return SubmitInvalidation(
		[](FrameInvalidator& invalidator) {
			invalidator.invalidateAll();
		});
}

bool MarkLegacyFrameChanged() noexcept
{
	return SubmitInvalidation(
		[](FrameInvalidator& invalidator) {
			invalidator.markChanged();
		});
}

void InvalidateRegion(INT32 left, INT32 top, INT32 right, INT32 bottom)
{
	(void)InvalidateLegacyFrameRegion(FrameRegion{left, top, right, bottom});
}

void InvalidateRegions(SGPRect* regions, UINT32 count)
{
	if (!regions) return;
	for (UINT32 index = 0; index < count; ++index)
	{
		(void)InvalidateLegacyFrameRegion(FrameRegion{
			regions[index].iLeft,
			regions[index].iTop,
			regions[index].iRight,
			regions[index].iBottom});
	}
}

void InvalidateScreen(void)
{
	(void)InvalidateLegacyFrameAll();
}

void InvalidateFrameBuffer(void)
{
	(void)InvalidateLegacyFrameAll();
	guiFrameBufferState = BUFFER_DIRTY;
}

void InvalidateRegionEx(
	INT32 left, INT32 top, INT32 right, INT32 bottom, UINT32)
{
	(void)InvalidateLegacyFrameRegion(FrameRegion{left, top, right, bottom});
}

void MarkFrameDirty(void)
{
	(void)MarkLegacyFrameChanged();
}
