#include <Engine/Adapters/Legacy/PlatformRenderSurfaceAccess.h>

#include <Engine/Adapters/Legacy/PlatformDepthBufferBackend.h>
#include <Engine/Adapters/Legacy/PlatformVideoSurfaceBackend.h>

#include "vsurface.h"

#include <limits>

namespace
{
bool DescribePlatformSurface(
	RenderSurfaceId surface, RenderSurfaceDescription& description)
{
	if (surface == 0 ||
		surface > std::numeric_limits<std::uint32_t>::max())
		return false;
	if (surface == DEPTH_BUFFER)
	{
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		std::uint32_t pitchBytes = 0;
		if (!PlatformDepthBufferDescribe(
				width, height, pitchBytes) ||
			width == 0 || height == 0 ||
			static_cast<std::uint64_t>(pitchBytes) <
				static_cast<std::uint64_t>(width) *
					sizeof(std::uint16_t))
			return false;
		description = RenderSurfaceDescription{
			width, height, RenderPixelFormat::Depth16, 16};
		return true;
	}
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint8_t contentBitDepth = 0;
	std::uint8_t pixelBytes = 0;
	if (!PlatformVideoSurfaceDescribe(
			static_cast<std::uint32_t>(surface), width, height,
			contentBitDepth, pixelBytes))
		return false;

	RenderPixelFormat format;
	switch (pixelBytes)
	{
	case 1: format = RenderPixelFormat::Indexed8; break;
	case 2: format = RenderPixelFormat::Rgb565; break;
	case 4: format = RenderPixelFormat::Argb8888; break;
	default: return false;
	}
	description =
		RenderSurfaceDescription{width, height, format, contentBitDepth};
	return true;
}

class LegacyRenderSurfaceAccess final : public RenderSurfaceAccess
{
public:
	RenderSurfaceId surfaceFor(RenderSurfaceRole role) const override
	{
		switch (role)
		{
		case RenderSurfaceRole::Primary: return PRIMARY_SURFACE;
		case RenderSurfaceRole::BackBuffer: return BACKBUFFER;
		case RenderSurfaceRole::FrameBuffer: return FRAME_BUFFER;
		case RenderSurfaceRole::Cursor: return MOUSE_BUFFER;
		case RenderSurfaceRole::DepthBuffer: return DEPTH_BUFFER;
		case RenderSurfaceRole::Count: break;
		}
		return 0;
	}

	bool describe(
		RenderSurfaceId surface,
		RenderSurfaceDescription& description) const override
	{
		return DescribePlatformSurface(surface, description);
	}

	bool map(
		RenderSurfaceId surface, MutableRenderSurface& mapping) override
	{
		if (surface == 0 ||
			surface > std::numeric_limits<std::uint32_t>::max())
			return false;
		const std::uint32_t platformSurface =
			static_cast<std::uint32_t>(surface);
		std::uint32_t pitchBytes = 0;
		std::uint8_t* const pixels = platformSurface == DEPTH_BUFFER ?
			PlatformDepthBufferMap(pitchBytes) :
			PlatformVideoSurfaceMap(platformSurface, pitchBytes);
		if (!pixels) return false;

		RenderSurfaceDescription description;
		if (!DescribePlatformSurface(surface, description) ||
			pitchBytes == 0 ||
			description.height >
				std::numeric_limits<std::size_t>::max() / pitchBytes)
		{
			if (platformSurface == DEPTH_BUFFER)
				PlatformDepthBufferUnmap();
			else
				PlatformVideoSurfaceUnmap(platformSurface);
			return false;
		}
		mapping = MutableRenderSurface{
			reinterpret_cast<std::byte*>(pixels),
			static_cast<std::size_t>(pitchBytes) * description.height,
			pitchBytes, description};
		return true;
	}

	void unmap(RenderSurfaceId surface) override
	{
		if (surface == 0 ||
			surface > std::numeric_limits<std::uint32_t>::max())
			return;
		if (surface == DEPTH_BUFFER)
			PlatformDepthBufferUnmap();
		else
			PlatformVideoSurfaceUnmap(
				static_cast<std::uint32_t>(surface));
	}
};
}

RenderSurfaceAccess& GetPlatformRenderSurfaceAccess() noexcept
{
	static LegacyRenderSurfaceAccess access;
	return access;
}
