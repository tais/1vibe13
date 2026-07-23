#include <Engine/Adapters/Legacy/PlatformFramePresenter.h>

#include <Engine/Adapters/Legacy/PlatformVideoBackend.h>

namespace
{
class LegacyFramePresenter final : public FramePresenter
{
public:
	void present(FramePresentMode mode) override
	{
		if (mode == FramePresentMode::Immediate)
			PlatformVideoPresentImmediate();
		else
			PlatformVideoPresentPaced();
	}
};
}

FramePresenter& GetPlatformFramePresenter() noexcept
{
	static LegacyFramePresenter presenter;
	return presenter;
}
