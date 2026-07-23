#include <Engine/Adapters/Legacy/LegacyFrameGateway.h>

#include <Engine/Adapters/Legacy/PlatformFramePresenter.h>

#include "video.h"

#include <atomic>

namespace
{
std::atomic<FramePresenter*> gBoundPresenter{nullptr};
std::atomic_flag gPresenting = ATOMIC_FLAG_INIT;

class PresentationGuard
{
public:
	PresentationGuard() noexcept
		: acquired_(!gPresenting.test_and_set(std::memory_order_acquire))
	{
	}

	~PresentationGuard()
	{
		if (acquired_) gPresenting.clear(std::memory_order_release);
	}

	bool acquired() const noexcept { return acquired_; }

private:
	bool acquired_;
};
}

void BindLegacyFramePresenter(FramePresenter& presenter) noexcept
{
	gBoundPresenter.store(&presenter, std::memory_order_release);
}

void ResetLegacyFramePresenter() noexcept
{
	gBoundPresenter.store(nullptr, std::memory_order_release);
}

FramePresenter& GetLegacyFramePresenter() noexcept
{
	FramePresenter* const presenter =
		gBoundPresenter.load(std::memory_order_acquire);
	return presenter ? *presenter : GetPlatformFramePresenter();
}

bool PresentLegacyFrame(FramePresentMode mode) noexcept
{
	PresentationGuard guard;
	if (!guard.acquired()) return false;
	try
	{
		GetLegacyFramePresenter().present(mode);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

void RefreshScreen(void*)
{
	(void)PresentLegacyFrame(FramePresentMode::Paced);
}

void PresentNow(void)
{
	(void)PresentLegacyFrame(FramePresentMode::Immediate);
}
