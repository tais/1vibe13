#include <Engine/Core/RenderCommands.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
constexpr std::size_t MaximumStretchSnapshotBytes =
	256u * 1024u * 1024u;

class MappingLease
{
public:
	MappingLease(RenderSurfaceAccess& surfaces, RenderSurfaceId surface)
		: surfaces_(surfaces), surface_(surface)
	{
	}

	~MappingLease() { (void)close(); }

	bool close() noexcept
	{
		if (!active_) return true;
		active_ = false;
		try
		{
			surfaces_.unmap(surface_);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

private:
	RenderSurfaceAccess& surfaces_;
	RenderSurfaceId surface_;
	bool active_ = true;
};

bool Describe(
	RenderSurfaceAccess& surfaces,
	RenderSurfaceId surface,
	RenderSurfaceDescription& description) noexcept
{
	try
	{
		return surfaces.describe(surface, description) &&
			IsValidRenderSurfaceDescription(description);
	}
	catch (...)
	{
		return false;
	}
}

bool Map(
	RenderSurfaceAccess& surfaces,
	RenderSurfaceId surface,
	MutableRenderSurface& mapping) noexcept
{
	try
	{
		return surfaces.map(surface, mapping);
	}
	catch (...)
	{
		return false;
	}
}

bool SupportsCopyMode(RenderSurfaceCopyMode mode, RenderPixelFormat format)
{
	if (format == RenderPixelFormat::Depth16) return false;
	switch (mode)
	{
	case RenderSurfaceCopyMode::Opaque:
		return true;
	case RenderSurfaceCopyMode::SourceColorKeyRgb:
		return format != RenderPixelFormat::Indexed8;
	}
	return false;
}

std::size_t EncodeColor(
	RenderColor color,
	RenderPixelFormat format,
	std::array<std::byte, 4>& encoded)
{
	switch (format)
	{
	case RenderPixelFormat::Rgb565:
	{
		const std::uint16_t packed =
			static_cast<std::uint16_t>(
				(static_cast<std::uint16_t>(color.red >> 3) << 11) |
				(static_cast<std::uint16_t>(color.green >> 2) << 5) |
				static_cast<std::uint16_t>(color.blue >> 3));
		std::memcpy(encoded.data(), &packed, sizeof(packed));
		return sizeof(packed);
	}
	case RenderPixelFormat::Argb8888:
	{
		const std::uint32_t packed =
			(static_cast<std::uint32_t>(color.alpha) << 24) |
			(static_cast<std::uint32_t>(color.red) << 16) |
			(static_cast<std::uint32_t>(color.green) << 8) |
			static_cast<std::uint32_t>(color.blue);
		std::memcpy(encoded.data(), &packed, sizeof(packed));
		return sizeof(packed);
	}
	case RenderPixelFormat::Indexed8:
	case RenderPixelFormat::Depth16:
		break;
	}
	return 0;
}

struct CopyArea
{
	std::int64_t sourceLeft;
	std::int64_t sourceTop;
	std::int64_t sourceRight;
	std::int64_t sourceBottom;
	std::int64_t destinationX;
	std::int64_t destinationY;
};

bool ClipCopy(
	const RenderSurfaceDescription& source,
	const RenderSurfaceDescription& destination,
	CopyArea& area)
{
	if (area.sourceLeft >= area.sourceRight ||
		area.sourceTop >= area.sourceBottom)
		return false;

	if (area.sourceLeft < 0)
	{
		area.destinationX -= area.sourceLeft;
		area.sourceLeft = 0;
	}
	if (area.sourceTop < 0)
	{
		area.destinationY -= area.sourceTop;
		area.sourceTop = 0;
	}
	area.sourceRight = std::min<std::int64_t>(
		area.sourceRight, source.width);
	area.sourceBottom = std::min<std::int64_t>(
		area.sourceBottom, source.height);
	if (area.sourceLeft >= area.sourceRight ||
		area.sourceTop >= area.sourceBottom)
		return false;

	if (area.destinationX < 0)
	{
		area.sourceLeft -= area.destinationX;
		area.destinationX = 0;
	}
	if (area.destinationY < 0)
	{
		area.sourceTop -= area.destinationY;
		area.destinationY = 0;
	}
	if (area.destinationX >= destination.width ||
		area.destinationY >= destination.height ||
		area.sourceLeft >= area.sourceRight ||
		area.sourceTop >= area.sourceBottom)
		return false;

	const std::int64_t maximumWidth =
		static_cast<std::int64_t>(destination.width) -
		area.destinationX;
	const std::int64_t maximumHeight =
		static_cast<std::int64_t>(destination.height) -
		area.destinationY;
	area.sourceRight = std::min(
		area.sourceRight, area.sourceLeft + maximumWidth);
	area.sourceBottom = std::min(
		area.sourceBottom, area.sourceTop + maximumHeight);
	return area.sourceLeft < area.sourceRight &&
		area.sourceTop < area.sourceBottom;
}

bool CopyBottomUp(const CopyArea& area, bool sameSurface)
{
	return sameSurface && area.destinationY > area.sourceTop;
}

std::size_t RowIndex(
	std::size_t iteration, std::size_t height, bool bottomUp)
{
	return bottomUp ? height - iteration - 1 : iteration;
}

void CopyOpaque(
	const CopyArea& area,
	const MutableRenderSurface& source,
	MutableRenderSurface& destination,
	bool sameSurface,
	std::size_t pixelBytes,
	std::size_t width,
	std::size_t height)
{
	const std::size_t rowBytes = width * pixelBytes;
	const bool bottomUp = CopyBottomUp(area, sameSurface);
	for (std::size_t iteration = 0; iteration < height; ++iteration)
	{
		const std::size_t row = RowIndex(iteration, height, bottomUp);
		const std::byte* const sourceRow =
			source.pixels +
			(static_cast<std::size_t>(area.sourceTop) + row) *
				source.pitchBytes +
			static_cast<std::size_t>(area.sourceLeft) * pixelBytes;
		std::byte* const destinationRow =
			destination.pixels +
			(static_cast<std::size_t>(area.destinationY) + row) *
				destination.pitchBytes +
			static_cast<std::size_t>(area.destinationX) * pixelBytes;
		if (sameSurface)
			std::memmove(destinationRow, sourceRow, rowBytes);
		else
			std::memcpy(destinationRow, sourceRow, rowBytes);
	}
}

template <typename Pixel>
void CopyColorKeyedPixels(
	Pixel packedKey,
	Pixel comparisonMask,
	const CopyArea& area,
	const MutableRenderSurface& source,
	MutableRenderSurface& destination,
	bool sameSurface,
	std::size_t width,
	std::size_t height)
{
	constexpr std::size_t pixelBytes = sizeof(Pixel);
	const bool bottomUp = CopyBottomUp(area, sameSurface);
	const bool rightToLeft =
		sameSurface &&
		area.destinationY == area.sourceTop &&
		area.destinationX > area.sourceLeft;
	for (std::size_t rowIteration = 0;
		rowIteration < height; ++rowIteration)
	{
		const std::size_t row =
			RowIndex(rowIteration, height, bottomUp);
		const std::byte* const sourceRow =
			source.pixels +
			(static_cast<std::size_t>(area.sourceTop) + row) *
				source.pitchBytes +
			static_cast<std::size_t>(area.sourceLeft) * pixelBytes;
		std::byte* const destinationRow =
			destination.pixels +
			(static_cast<std::size_t>(area.destinationY) + row) *
				destination.pitchBytes +
			static_cast<std::size_t>(area.destinationX) * pixelBytes;
		for (std::size_t columnIteration = 0;
			columnIteration < width; ++columnIteration)
		{
			const std::size_t column = rightToLeft ?
				width - columnIteration - 1 : columnIteration;
			const std::byte* const sourcePixel =
				sourceRow + column * pixelBytes;
			Pixel packedPixel = 0;
			std::memcpy(&packedPixel, sourcePixel, pixelBytes);
			if ((packedPixel & comparisonMask) !=
				(packedKey & comparisonMask))
			{
				std::memcpy(
					destinationRow + column * pixelBytes,
					sourcePixel, pixelBytes);
			}
		}
	}
}

void CopyColorKeyed(
	RenderColor colorKey,
	const CopyArea& area,
	const MutableRenderSurface& source,
	MutableRenderSurface& destination,
	bool sameSurface,
	std::size_t width,
	std::size_t height)
{
	std::array<std::byte, 4> encodedKey{};
	(void)EncodeColor(colorKey, source.description.format, encodedKey);
	switch (source.description.format)
	{
	case RenderPixelFormat::Rgb565:
	{
		std::uint16_t packedKey = 0;
		std::memcpy(&packedKey, encodedKey.data(), sizeof(packedKey));
		CopyColorKeyedPixels(
			packedKey, std::numeric_limits<std::uint16_t>::max(),
			area, source, destination, sameSurface, width, height);
		break;
	}
	case RenderPixelFormat::Argb8888:
	{
		std::uint32_t packedKey = 0;
		std::memcpy(&packedKey, encodedKey.data(), sizeof(packedKey));
		CopyColorKeyedPixels(
			packedKey, 0x00ffffffu, area, source, destination,
			sameSurface, width, height);
		break;
	}
	case RenderPixelFormat::Indexed8:
	case RenderPixelFormat::Depth16:
		break;
	}
}

bool CopyPixels(
	const RenderSurfaceCopyCommand& command,
	const CopyArea& area,
	const MutableRenderSurface& source,
	MutableRenderSurface& destination,
	bool sameSurface)
{
	const std::size_t pixelBytes =
		RenderPixelBytes(source.description.format);
	const std::size_t width =
		static_cast<std::size_t>(area.sourceRight - area.sourceLeft);
	const std::size_t height =
		static_cast<std::size_t>(area.sourceBottom - area.sourceTop);
	if (pixelBytes == 0 || width == 0 || height == 0) return false;

	switch (command.mode)
	{
	case RenderSurfaceCopyMode::Opaque:
		CopyOpaque(
			area, source, destination, sameSurface,
			pixelBytes, width, height);
		return true;
	case RenderSurfaceCopyMode::SourceColorKeyRgb:
		CopyColorKeyed(
			command.sourceColorKey, area, source, destination,
			sameSurface, width, height);
		return true;
	}
	return false;
}

struct StretchArea
{
	std::int64_t sourceLeft;
	std::int64_t sourceTop;
	std::int64_t sourceRight;
	std::int64_t sourceBottom;
	std::int64_t destinationLeft;
	std::int64_t destinationTop;
	std::int64_t destinationRight;
	std::int64_t destinationBottom;
	std::int64_t clippedLeft;
	std::int64_t clippedTop;
	std::int64_t clippedRight;
	std::int64_t clippedBottom;
};

bool PrepareStretchArea(
	const RenderSurfaceStretchCommand& command,
	const RenderSurfaceDescription& source,
	const RenderSurfaceDescription& destination,
	StretchArea& area)
{
	area = StretchArea{
		command.sourceRegion.left,
		command.sourceRegion.top,
		command.sourceRegion.right,
		command.sourceRegion.bottom,
		command.destinationRegion.left,
		command.destinationRegion.top,
		command.destinationRegion.right,
		command.destinationRegion.bottom,
		0, 0, 0, 0};
	if (area.sourceLeft >= area.sourceRight ||
		area.sourceTop >= area.sourceBottom ||
		area.destinationLeft >= area.destinationRight ||
		area.destinationTop >= area.destinationBottom)
		return false;

	area.clippedLeft = std::max<std::int64_t>(area.destinationLeft, 0);
	area.clippedTop = std::max<std::int64_t>(area.destinationTop, 0);
	area.clippedRight = std::min<std::int64_t>(
		area.destinationRight, destination.width);
	area.clippedBottom = std::min<std::int64_t>(
		area.destinationBottom, destination.height);
	if (area.clippedLeft >= area.clippedRight ||
		area.clippedTop >= area.clippedBottom)
		return false;

	// An out-of-range source region is a transparent/no-op portion of a
	// stretch, never permission to read beyond mapped storage.
	return area.sourceRight > 0 && area.sourceBottom > 0 &&
		area.sourceLeft < source.width && area.sourceTop < source.height;
}

struct StretchSource
{
	const std::byte* pixels = nullptr;
	std::size_t pitchBytes = 0;
	std::int64_t originX = 0;
	std::int64_t originY = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

struct StretchSnapshot
{
	std::vector<std::byte> pixels;
	StretchSource source;
};

bool CaptureStretchSource(
	const MutableRenderSurface& mapping,
	const StretchArea& area,
	StretchSnapshot& snapshot)
{
	const std::int64_t left = std::max<std::int64_t>(area.sourceLeft, 0);
	const std::int64_t top = std::max<std::int64_t>(area.sourceTop, 0);
	const std::int64_t right = std::min<std::int64_t>(
		area.sourceRight, mapping.description.width);
	const std::int64_t bottom = std::min<std::int64_t>(
		area.sourceBottom, mapping.description.height);
	if (left >= right || top >= bottom) return false;

	const std::size_t pixelBytes =
		RenderPixelBytes(mapping.description.format);
	const std::size_t width =
		static_cast<std::size_t>(right - left);
	const std::size_t height =
		static_cast<std::size_t>(bottom - top);
	if (pixelBytes == 0 ||
		width > std::numeric_limits<std::size_t>::max() / pixelBytes)
		return false;
	const std::size_t pitchBytes = width * pixelBytes;
	if (height > std::numeric_limits<std::size_t>::max() / pitchBytes)
		return false;
	const std::size_t sizeBytes = height * pitchBytes;
	if (sizeBytes > MaximumStretchSnapshotBytes) return false;

	try
	{
		snapshot.pixels.resize(sizeBytes);
	}
	catch (...)
	{
		return false;
	}
	for (std::size_t row = 0; row < height; ++row)
	{
		const std::byte* const sourceRow =
			mapping.pixels +
			(static_cast<std::size_t>(top) + row) *
				mapping.pitchBytes +
			static_cast<std::size_t>(left) * pixelBytes;
		std::memcpy(
			snapshot.pixels.data() + row * pitchBytes,
			sourceRow, pitchBytes);
	}
	snapshot.source = StretchSource{
		snapshot.pixels.data(), pitchBytes, left, top,
		static_cast<std::uint32_t>(width),
		static_cast<std::uint32_t>(height)};
	return true;
}

template <typename Pixel>
bool StretchPixels(
	const RenderSurfaceStretchCommand& command,
	const StretchArea& area,
	const StretchSource& source,
	MutableRenderSurface& destination,
	Pixel packedKey,
	Pixel comparisonMask)
{
	const std::uint64_t sourceWidth =
		static_cast<std::uint64_t>(
			area.sourceRight - area.sourceLeft);
	const std::uint64_t sourceHeight =
		static_cast<std::uint64_t>(
			area.sourceBottom - area.sourceTop);
	const std::uint64_t destinationWidth =
		static_cast<std::uint64_t>(
			area.destinationRight - area.destinationLeft);
	const std::uint64_t destinationHeight =
		static_cast<std::uint64_t>(
			area.destinationBottom - area.destinationTop);
	if (sourceWidth == 0 || sourceHeight == 0 ||
		destinationWidth == 0 || destinationHeight == 0)
		return false;

	for (std::int64_t destinationY = area.clippedTop;
		destinationY < area.clippedBottom; ++destinationY)
	{
		const std::uint64_t destinationOffsetY =
			static_cast<std::uint64_t>(
				destinationY - area.destinationTop);
		const std::int64_t sourceY =
			area.sourceTop +
			static_cast<std::int64_t>(
				(destinationOffsetY * sourceHeight) /
				destinationHeight);
		if (sourceY < source.originY ||
			sourceY >= source.originY +
				static_cast<std::int64_t>(source.height))
			continue;

		std::byte* const destinationRow =
			destination.pixels +
			static_cast<std::size_t>(destinationY) *
				destination.pitchBytes;
		for (std::int64_t destinationX = area.clippedLeft;
			destinationX < area.clippedRight; ++destinationX)
		{
			const std::uint64_t destinationOffsetX =
				static_cast<std::uint64_t>(
					destinationX - area.destinationLeft);
			const std::int64_t sourceX =
				area.sourceLeft +
				static_cast<std::int64_t>(
					(destinationOffsetX * sourceWidth) /
					destinationWidth);
			if (sourceX < source.originX ||
				sourceX >= source.originX +
					static_cast<std::int64_t>(source.width))
				continue;

			const std::size_t sourceRow =
				static_cast<std::size_t>(sourceY - source.originY);
			const std::size_t sourceColumn =
				static_cast<std::size_t>(sourceX - source.originX);
			const std::byte* const sourcePixel =
				source.pixels + sourceRow * source.pitchBytes +
				sourceColumn * sizeof(Pixel);
			Pixel pixel = 0;
			std::memcpy(&pixel, sourcePixel, sizeof(pixel));
			if (command.mode ==
					RenderSurfaceCopyMode::SourceColorKeyRgb &&
				(pixel & comparisonMask) ==
					(packedKey & comparisonMask))
			{
				continue;
			}
			std::memcpy(
				destinationRow +
					static_cast<std::size_t>(destinationX) *
						sizeof(Pixel),
				&pixel, sizeof(pixel));
		}
	}
	return true;
}

bool DispatchStretch(
	const RenderSurfaceStretchCommand& command,
	const StretchArea& area,
	const StretchSource& source,
	MutableRenderSurface& destination)
{
	std::array<std::byte, 4> encodedKey{};
	(void)EncodeColor(
		command.sourceColorKey,
		destination.description.format, encodedKey);
	switch (destination.description.format)
	{
	case RenderPixelFormat::Indexed8:
		return StretchPixels<std::uint8_t>(
			command, area, source, destination, 0, 0xffu);
	case RenderPixelFormat::Rgb565:
	{
		std::uint16_t packedKey = 0;
		std::memcpy(&packedKey, encodedKey.data(), sizeof(packedKey));
		return StretchPixels<std::uint16_t>(
			command, area, source, destination, packedKey,
			std::numeric_limits<std::uint16_t>::max());
	}
	case RenderPixelFormat::Argb8888:
	{
		std::uint32_t packedKey = 0;
		std::memcpy(&packedKey, encodedKey.data(), sizeof(packedKey));
		return StretchPixels<std::uint32_t>(
			command, area, source, destination, packedKey,
			0x00ffffffu);
	}
	case RenderPixelFormat::Depth16:
		return false;
	}
	return false;
}

bool ClipRegion(
	const RenderSurfaceRegion& region,
	const RenderSurfaceDescription& description,
	std::int64_t& left,
	std::int64_t& top,
	std::int64_t& right,
	std::int64_t& bottom)
{
	left = std::max<std::int64_t>(region.left, 0);
	top = std::max<std::int64_t>(region.top, 0);
	right = std::min<std::int64_t>(region.right, description.width);
	bottom = std::min<std::int64_t>(region.bottom, description.height);
	return left < right && top < bottom;
}

template <typename Value>
Value ScaleChannel(
	Value channel,
	std::uint16_t numerator,
	std::uint16_t denominator)
{
	return static_cast<Value>(
		(static_cast<std::uint32_t>(channel) * numerator) /
		denominator);
}

void ShadeRgb565(
	MutableRenderSurface& mapping,
	std::int64_t left,
	std::int64_t top,
	std::int64_t right,
	std::int64_t bottom,
	std::uint16_t numerator,
	std::uint16_t denominator)
{
	for (std::int64_t y = top; y < bottom; ++y)
	{
		std::byte* const row =
			mapping.pixels +
			static_cast<std::size_t>(y) * mapping.pitchBytes;
		for (std::int64_t x = left; x < right; ++x)
		{
			std::uint16_t pixel = 0;
			std::byte* const target =
				row + static_cast<std::size_t>(x) * sizeof(pixel);
			std::memcpy(&pixel, target, sizeof(pixel));
			const std::uint16_t red =
				ScaleChannel<std::uint16_t>(
					static_cast<std::uint16_t>((pixel >> 11) & 0x1fu),
					numerator, denominator);
			const std::uint16_t green =
				ScaleChannel<std::uint16_t>(
					static_cast<std::uint16_t>((pixel >> 5) & 0x3fu),
					numerator, denominator);
			const std::uint16_t blue =
				ScaleChannel<std::uint16_t>(
					static_cast<std::uint16_t>(pixel & 0x1fu),
					numerator, denominator);
			pixel = static_cast<std::uint16_t>(
				(red << 11) | (green << 5) | blue);
			std::memcpy(target, &pixel, sizeof(pixel));
		}
	}
}

void ShadeArgb8888(
	MutableRenderSurface& mapping,
	std::int64_t left,
	std::int64_t top,
	std::int64_t right,
	std::int64_t bottom,
	std::uint16_t numerator,
	std::uint16_t denominator)
{
	for (std::int64_t y = top; y < bottom; ++y)
	{
		std::byte* const row =
			mapping.pixels +
			static_cast<std::size_t>(y) * mapping.pitchBytes;
		for (std::int64_t x = left; x < right; ++x)
		{
			std::uint32_t pixel = 0;
			std::byte* const target =
				row + static_cast<std::size_t>(x) * sizeof(pixel);
			std::memcpy(&pixel, target, sizeof(pixel));
			const std::uint32_t alpha = pixel & 0xff000000u;
			const std::uint32_t red =
				ScaleChannel<std::uint32_t>(
					(pixel >> 16) & 0xffu, numerator, denominator);
			const std::uint32_t green =
				ScaleChannel<std::uint32_t>(
					(pixel >> 8) & 0xffu, numerator, denominator);
			const std::uint32_t blue =
				ScaleChannel<std::uint32_t>(
					pixel & 0xffu, numerator, denominator);
			pixel = alpha | (red << 16) | (green << 8) | blue;
			std::memcpy(target, &pixel, sizeof(pixel));
		}
	}
}
}

MappedRenderCommandSink::MappedRenderCommandSink(
	RenderSurfaceAccess& surfaces)
	: surfaces_(surfaces)
{
}

bool MappedRenderCommandSink::fillSurface(
	const RenderSurfaceFillCommand& command)
{
	if (command.surface == 0) return false;

	MutableRenderSurface mapping;
	if (!Map(surfaces_, command.surface, mapping)) return false;
	MappingLease lease(surfaces_, command.surface);
	if (!IsValidRenderSurfaceMapping(mapping) ||
		mapping.description.format == RenderPixelFormat::Indexed8 ||
		mapping.description.format == RenderPixelFormat::Depth16)
		return false;

	std::int64_t left = command.region.left;
	std::int64_t top = command.region.top;
	std::int64_t right = command.region.right;
	std::int64_t bottom = command.region.bottom;
	if (right < left) std::swap(left, right);
	if (bottom < top) std::swap(top, bottom);
	left = std::max<std::int64_t>(left, 0);
	top = std::max<std::int64_t>(top, 0);
	right = std::min<std::int64_t>(
		right, mapping.description.width);
	bottom = std::min<std::int64_t>(
		bottom, mapping.description.height);
	if (left >= right || top >= bottom) return lease.close();

	std::array<std::byte, 4> encoded{};
	const std::size_t pixelBytes =
		EncodeColor(command.color, mapping.description.format, encoded);
	if (pixelBytes == 0) return false;
	const std::size_t rowBytes =
		static_cast<std::size_t>(right - left) * pixelBytes;
	std::byte* const firstRow =
		mapping.pixels +
		static_cast<std::size_t>(top) * mapping.pitchBytes +
		static_cast<std::size_t>(left) * pixelBytes;

	std::memcpy(firstRow, encoded.data(), pixelBytes);
	for (std::size_t filled = pixelBytes; filled < rowBytes;)
	{
		const std::size_t copied = std::min(filled, rowBytes - filled);
		std::memcpy(firstRow + filled, firstRow, copied);
		filled += copied;
	}
	for (std::int64_t row = top + 1; row < bottom; ++row)
	{
		std::byte* const destination =
			mapping.pixels +
			static_cast<std::size_t>(row) * mapping.pitchBytes +
			static_cast<std::size_t>(left) * pixelBytes;
		std::memcpy(destination, firstRow, rowBytes);
	}
	return lease.close();
}

bool MappedRenderCommandSink::copySurface(
	const RenderSurfaceCopyCommand& command)
{
	if (command.source == 0 || command.destination == 0)
		return false;

	RenderSurfaceDescription sourceDescription;
	RenderSurfaceDescription destinationDescription;
	if (!Describe(surfaces_, command.source, sourceDescription))
		return false;
	if (command.source == command.destination)
		destinationDescription = sourceDescription;
	else if (!Describe(
		surfaces_, command.destination, destinationDescription))
		return false;
	if (sourceDescription.format != destinationDescription.format ||
		sourceDescription.contentBitDepth !=
			destinationDescription.contentBitDepth ||
		!SupportsCopyMode(command.mode, sourceDescription.format))
		return false;

	CopyArea area{
		command.sourceRegion.left,
		command.sourceRegion.top,
		command.sourceRegion.right,
		command.sourceRegion.bottom,
		command.destinationOrigin.x,
		command.destinationOrigin.y};
	if (!ClipCopy(sourceDescription, destinationDescription, area))
		return true;

	MutableRenderSurface sourceMapping;
	if (!Map(surfaces_, command.source, sourceMapping)) return false;
	MappingLease sourceLease(surfaces_, command.source);
	if (!IsValidRenderSurfaceMapping(sourceMapping) ||
		sourceMapping.description != sourceDescription)
		return false;

	if (command.source == command.destination)
	{
		const bool copied =
			CopyPixels(command, area, sourceMapping, sourceMapping, true);
		const bool unmapped = sourceLease.close();
		return copied && unmapped;
	}

	MutableRenderSurface destinationMapping;
	if (!Map(surfaces_, command.destination, destinationMapping))
		return false;
	MappingLease destinationLease(surfaces_, command.destination);
	if (!IsValidRenderSurfaceMapping(destinationMapping) ||
		destinationMapping.description != destinationDescription)
		return false;

	const bool copied =
		CopyPixels(
			command, area, sourceMapping, destinationMapping, false);
	const bool destinationUnmapped = destinationLease.close();
	const bool sourceUnmapped = sourceLease.close();
	return copied && destinationUnmapped && sourceUnmapped;
}

bool MappedRenderCommandSink::stretchSurface(
	const RenderSurfaceStretchCommand& command)
{
	if (command.source == 0 || command.destination == 0)
		return false;

	RenderSurfaceDescription sourceDescription;
	RenderSurfaceDescription destinationDescription;
	if (!Describe(surfaces_, command.source, sourceDescription))
		return false;
	if (command.source == command.destination)
		destinationDescription = sourceDescription;
	else if (!Describe(
		surfaces_, command.destination, destinationDescription))
		return false;
	if (sourceDescription.format != destinationDescription.format ||
		sourceDescription.contentBitDepth !=
			destinationDescription.contentBitDepth ||
		!SupportsCopyMode(command.mode, sourceDescription.format))
		return false;

	const std::int64_t sourceWidth =
		static_cast<std::int64_t>(command.sourceRegion.right) -
		command.sourceRegion.left;
	const std::int64_t sourceHeight =
		static_cast<std::int64_t>(command.sourceRegion.bottom) -
		command.sourceRegion.top;
	const std::int64_t destinationWidth =
		static_cast<std::int64_t>(command.destinationRegion.right) -
		command.destinationRegion.left;
	const std::int64_t destinationHeight =
		static_cast<std::int64_t>(command.destinationRegion.bottom) -
		command.destinationRegion.top;
	if (sourceWidth <= 0 || sourceHeight <= 0 ||
		destinationWidth <= 0 || destinationHeight <= 0)
		return true;

	if (sourceWidth == destinationWidth &&
		sourceHeight == destinationHeight)
	{
		return copySurface(RenderSurfaceCopyCommand{
			command.source,
			command.destination,
			command.sourceRegion,
			RenderSurfacePoint{
				command.destinationRegion.left,
				command.destinationRegion.top},
			command.mode,
			command.sourceColorKey});
	}

	StretchArea area;
	if (!PrepareStretchArea(
		command, sourceDescription, destinationDescription, area))
		return true;

	MutableRenderSurface sourceMapping;
	if (!Map(surfaces_, command.source, sourceMapping)) return false;
	MappingLease sourceLease(surfaces_, command.source);
	if (!IsValidRenderSurfaceMapping(sourceMapping) ||
		sourceMapping.description != sourceDescription)
		return false;

	if (command.source == command.destination)
	{
		StretchSnapshot snapshot;
		if (!CaptureStretchSource(sourceMapping, area, snapshot))
			return false;
		const bool stretched =
			DispatchStretch(
				command, area, snapshot.source, sourceMapping);
		const bool unmapped = sourceLease.close();
		return stretched && unmapped;
	}

	MutableRenderSurface destinationMapping;
	if (!Map(surfaces_, command.destination, destinationMapping))
		return false;
	MappingLease destinationLease(surfaces_, command.destination);
	if (!IsValidRenderSurfaceMapping(destinationMapping) ||
		destinationMapping.description != destinationDescription)
		return false;

	const StretchSource source{
		sourceMapping.pixels, sourceMapping.pitchBytes, 0, 0,
		sourceMapping.description.width,
		sourceMapping.description.height};
	const bool stretched =
		DispatchStretch(command, area, source, destinationMapping);
	const bool destinationUnmapped = destinationLease.close();
	const bool sourceUnmapped = sourceLease.close();
	return stretched && destinationUnmapped && sourceUnmapped;
}

bool MappedRenderCommandSink::shadeSurface(
	const RenderSurfaceShadeCommand& command)
{
	if (command.surface == 0 || command.denominator == 0 ||
		command.numerator > command.denominator)
		return false;

	RenderSurfaceDescription description;
	if (!Describe(surfaces_, command.surface, description) ||
		description.format == RenderPixelFormat::Indexed8 ||
		description.format == RenderPixelFormat::Depth16)
		return false;

	std::int64_t left = 0;
	std::int64_t top = 0;
	std::int64_t right = 0;
	std::int64_t bottom = 0;
	if (!ClipRegion(
		command.region, description, left, top, right, bottom) ||
		command.numerator == command.denominator)
		return true;

	MutableRenderSurface mapping;
	if (!Map(surfaces_, command.surface, mapping)) return false;
	MappingLease lease(surfaces_, command.surface);
	if (!IsValidRenderSurfaceMapping(mapping) ||
		mapping.description != description)
		return false;

	switch (description.format)
	{
	case RenderPixelFormat::Rgb565:
		ShadeRgb565(
			mapping, left, top, right, bottom,
			command.numerator, command.denominator);
		break;
	case RenderPixelFormat::Argb8888:
		ShadeArgb8888(
			mapping, left, top, right, bottom,
			command.numerator, command.denominator);
		break;
	case RenderPixelFormat::Indexed8:
	case RenderPixelFormat::Depth16:
		return false;
	}
	return lease.close();
}

bool MappedRenderCommandSink::fillDepth(
	const RenderDepthFillCommand& command)
{
	if (command.surface == 0) return false;

	RenderSurfaceDescription description;
	if (!Describe(surfaces_, command.surface, description) ||
		description.format != RenderPixelFormat::Depth16 ||
		description.contentBitDepth != 16)
		return false;

	std::int64_t left = 0;
	std::int64_t top = 0;
	std::int64_t right = 0;
	std::int64_t bottom = 0;
	if (!ClipRegion(
		command.region, description, left, top, right, bottom))
		return true;

	MutableRenderSurface mapping;
	if (!Map(surfaces_, command.surface, mapping)) return false;
	MappingLease lease(surfaces_, command.surface);
	if (!IsValidRenderSurfaceMapping(mapping) ||
		mapping.description != description)
		return false;

	std::array<std::byte, sizeof(std::uint16_t)> encoded{};
	std::memcpy(
		encoded.data(), &command.depth, sizeof(command.depth));
	const std::size_t rowBytes =
		static_cast<std::size_t>(right - left) *
		sizeof(std::uint16_t);
	std::byte* const firstRow =
		mapping.pixels +
		static_cast<std::size_t>(top) * mapping.pitchBytes +
		static_cast<std::size_t>(left) * sizeof(std::uint16_t);

	std::memcpy(firstRow, encoded.data(), encoded.size());
	for (std::size_t filled = encoded.size(); filled < rowBytes;)
	{
		const std::size_t copied =
			std::min(filled, rowBytes - filled);
		std::memcpy(firstRow + filled, firstRow, copied);
		filled += copied;
	}
	for (std::int64_t row = top + 1; row < bottom; ++row)
	{
		std::byte* const destination =
			mapping.pixels +
			static_cast<std::size_t>(row) * mapping.pitchBytes +
			static_cast<std::size_t>(left) *
				sizeof(std::uint16_t);
		std::memcpy(destination, firstRow, rowBytes);
	}
	return lease.close();
}

RenderSurfaceAccess& MappedRenderCommandSink::surfaces() const
{
	return surfaces_;
}
