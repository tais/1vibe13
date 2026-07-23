#include <Engine/Adapters/Legacy/LegacyRenderSurfaceGateway.h>

#include <Engine/Adapters/Legacy/PlatformRenderSurfaceAccess.h>

#include "vsurface.h"

#include <atomic>
#include <limits>

namespace
{
std::atomic<RenderSurfaceAccess*> gBoundAccess{nullptr};
thread_local bool gAccessingSurface = false;

class SurfaceAccessGuard
{
public:
	SurfaceAccessGuard() noexcept
		: acquired_(!gAccessingSurface)
	{
		if (acquired_) gAccessingSurface = true;
	}

	~SurfaceAccessGuard()
	{
		if (acquired_) gAccessingSurface = false;
	}

	bool acquired() const noexcept { return acquired_; }

private:
	bool acquired_;
};

template <typename Callback>
bool AccessSurface(Callback&& callback) noexcept
{
	SurfaceAccessGuard guard;
	if (!guard.acquired()) return false;
	try
	{
		return callback(GetLegacyRenderSurfaceAccess());
	}
	catch (...)
	{
		return false;
	}
}

}

void BindLegacyRenderSurfaceAccess(RenderSurfaceAccess& access) noexcept
{
	gBoundAccess.store(&access, std::memory_order_release);
}

void ResetLegacyRenderSurfaceAccess() noexcept
{
	gBoundAccess.store(nullptr, std::memory_order_release);
}

RenderSurfaceAccess& GetLegacyRenderSurfaceAccess() noexcept
{
	RenderSurfaceAccess* const access =
		gBoundAccess.load(std::memory_order_acquire);
	return access ? *access : GetPlatformRenderSurfaceAccess();
}

bool DescribeLegacyRenderSurface(
	RenderSurfaceId surface, RenderSurfaceDescription& description) noexcept
{
	RenderSurfaceDescription candidate;
	const bool described = AccessSurface(
		[surface, &candidate](RenderSurfaceAccess& access) {
			return access.describe(surface, candidate) &&
				IsValidRenderSurfaceDescription(candidate);
		});
	if (described) description = candidate;
	return described;
}

bool MapLegacyRenderSurface(
	RenderSurfaceId surface, MutableRenderSurface& mapping) noexcept
{
	MutableRenderSurface candidate;
	const bool mapped = AccessSurface(
		[surface, &candidate](RenderSurfaceAccess& access) {
			if (!access.map(surface, candidate)) return false;
			if (IsValidRenderSurfaceMapping(candidate)) return true;
			access.unmap(surface);
			candidate = MutableRenderSurface{};
			return false;
		});
	if (mapped) mapping = candidate;
	return mapped;
}

bool UnmapLegacyRenderSurface(RenderSurfaceId surface) noexcept
{
	return AccessSurface(
		[surface](RenderSurfaceAccess& access) {
			access.unmap(surface);
			return true;
		});
}

BOOLEAN GetVideoSurfaceDescription(
	UINT32 surface, UINT16* width, UINT16* height, UINT8* bitDepth)
{
	RenderSurfaceDescription description;
	if (!DescribeLegacyRenderSurface(surface, description) ||
		description.width > std::numeric_limits<UINT16>::max() ||
		description.height > std::numeric_limits<UINT16>::max())
		return FALSE;
	if (width) *width = static_cast<UINT16>(description.width);
	if (height) *height = static_cast<UINT16>(description.height);
	if (bitDepth) *bitDepth = description.contentBitDepth;
	return TRUE;
}

BYTE* LockVideoSurface(UINT32 surface, UINT32* pitchBytes)
{
	if (!pitchBytes) return nullptr;
	MutableRenderSurface mapping;
	if (!MapLegacyRenderSurface(surface, mapping) ||
		mapping.pitchBytes > std::numeric_limits<UINT32>::max())
	{
		if (mapping) (void)UnmapLegacyRenderSurface(surface);
		return nullptr;
	}
	*pitchBytes = static_cast<UINT32>(mapping.pitchBytes);
	return reinterpret_cast<BYTE*>(mapping.pixels);
}

void UnLockVideoSurface(UINT32 surface)
{
	(void)UnmapLegacyRenderSurface(surface);
}
