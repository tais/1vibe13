#include <Engine/Adapters/Legacy/PlatformFrameInvalidator.h>

#include <Engine/Adapters/Legacy/PlatformVideoBackend.h>

namespace
{
class LegacyFrameInvalidator final : public FrameInvalidator
{
public:
	void invalidateRegion(FrameRegion region) override
	{
		PlatformVideoInvalidateRegion(
			region.left, region.top, region.right, region.bottom);
	}

	void invalidateAll() override
	{
		PlatformVideoInvalidateAll();
	}

	void markChanged() override
	{
		PlatformVideoMarkFrameChanged();
	}
};
}

FrameInvalidator& GetPlatformFrameInvalidator() noexcept
{
	static LegacyFrameInvalidator invalidator;
	return invalidator;
}
