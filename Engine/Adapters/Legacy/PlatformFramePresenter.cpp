#include <Engine/Adapters/Legacy/PlatformFramePresenter.h>

#include "video.h"

namespace
{
class LegacyFramePresenter final : public FramePresenter
{
public:
	void present(FramePresentMode mode) override
	{
		if (mode == FramePresentMode::Immediate)
			PresentNow();
		else
			RefreshScreen(nullptr);
	}
};
}

FramePresenter& GetPlatformFramePresenter()
{
	static LegacyFramePresenter presenter;
	return presenter;
}
