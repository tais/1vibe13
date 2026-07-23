#ifndef ENGINE_CORE_RENDER_COMMANDS_H
#define ENGINE_CORE_RENDER_COMMANDS_H

#include <Engine/Core/RenderSurfaceAccess.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

struct RenderColor
{
	std::uint8_t red = 0;
	std::uint8_t green = 0;
	std::uint8_t blue = 0;
	std::uint8_t alpha = 255;
};

inline bool operator==(const RenderColor& left, const RenderColor& right)
{
	return left.red == right.red && left.green == right.green &&
		left.blue == right.blue && left.alpha == right.alpha;
}

inline bool operator!=(const RenderColor& left, const RenderColor& right)
{
	return !(left == right);
}

// Half-open surface coordinates: [left, right) x [top, bottom). Fill
// implementations normalize inverted edges and clip to the mapped surface.
struct RenderSurfaceRegion
{
	std::int32_t left = 0;
	std::int32_t top = 0;
	std::int32_t right = 0;
	std::int32_t bottom = 0;
};

inline bool operator==(
	const RenderSurfaceRegion& left, const RenderSurfaceRegion& right)
{
	return left.left == right.left && left.top == right.top &&
		left.right == right.right && left.bottom == right.bottom;
}

inline bool operator!=(
	const RenderSurfaceRegion& left, const RenderSurfaceRegion& right)
{
	return !(left == right);
}

struct RenderSurfaceFillCommand
{
	RenderSurfaceId surface = 0;
	RenderSurfaceRegion region;
	RenderColor color;
};

inline bool operator==(
	const RenderSurfaceFillCommand& left,
	const RenderSurfaceFillCommand& right)
{
	return left.surface == right.surface && left.region == right.region &&
		left.color == right.color;
}

inline bool operator!=(
	const RenderSurfaceFillCommand& left,
	const RenderSurfaceFillCommand& right)
{
	return !(left == right);
}

struct RenderSurfacePoint
{
	std::int32_t x = 0;
	std::int32_t y = 0;
};

inline bool operator==(
	const RenderSurfacePoint& left, const RenderSurfacePoint& right)
{
	return left.x == right.x && left.y == right.y;
}

inline bool operator!=(
	const RenderSurfacePoint& left, const RenderSurfacePoint& right)
{
	return !(left == right);
}

enum class RenderSurfaceCopyMode : std::uint8_t
{
	Opaque,
	SourceColorKeyRgb
};

struct RenderSurfaceCopyCommand
{
	RenderSurfaceId source = 0;
	RenderSurfaceId destination = 0;
	RenderSurfaceRegion sourceRegion;
	RenderSurfacePoint destinationOrigin;
	RenderSurfaceCopyMode mode = RenderSurfaceCopyMode::Opaque;
	RenderColor sourceColorKey;
};

inline bool operator==(
	const RenderSurfaceCopyCommand& left,
	const RenderSurfaceCopyCommand& right)
{
	return left.source == right.source &&
		left.destination == right.destination &&
		left.sourceRegion == right.sourceRegion &&
		left.destinationOrigin == right.destinationOrigin &&
		left.mode == right.mode &&
		left.sourceColorKey == right.sourceColorKey;
}

inline bool operator!=(
	const RenderSurfaceCopyCommand& left,
	const RenderSurfaceCopyCommand& right)
{
	return !(left == right);
}

// High-level renderer boundary. Commands use engine values and opaque surface
// identities; hosts decide whether to execute, record, forward, or reject them.
class RenderCommandSink
{
public:
	virtual ~RenderCommandSink() = default;
	virtual bool fillSurface(const RenderSurfaceFillCommand& command) = 0;
	virtual bool copySurface(const RenderSurfaceCopyCommand&) { return false; }
};

class NullRenderCommandSink final : public RenderCommandSink
{
public:
	bool fillSurface(const RenderSurfaceFillCommand&) override { return false; }
	bool copySurface(const RenderSurfaceCopyCommand&) override { return false; }
	static NullRenderCommandSink& instance()
	{
		static NullRenderCommandSink commands;
		return commands;
	}
};

class RecordingRenderCommandSink final : public RenderCommandSink
{
public:
	bool fillSurface(const RenderSurfaceFillCommand& command) override
	{
		commands_.push_back(command);
		return accepting_;
	}

	bool copySurface(const RenderSurfaceCopyCommand& command) override
	{
		copyCommands_.push_back(command);
		return accepting_;
	}

	const std::vector<RenderSurfaceFillCommand>& commands() const
	{
		return commands_;
	}
	const std::vector<RenderSurfaceCopyCommand>& copyCommands() const
	{
		return copyCommands_;
	}
	void setAccepting(bool accepting) { accepting_ = accepting; }
	void clear()
	{
		commands_.clear();
		copyCommands_.clear();
	}

private:
	std::vector<RenderSurfaceFillCommand> commands_;
	std::vector<RenderSurfaceCopyCommand> copyCommands_;
	bool accepting_ = true;
};

// CPU implementation shared by the compiled game, headless hosts, and tools.
// It maps only for the duration of one command, never writes row padding, and
// supports opaque copies for every storage format plus RGB colour keys for
// true-colour surfaces. Same-surface overlapping copies are defined and safe.
class MappedRenderCommandSink final : public RenderCommandSink
{
public:
	explicit MappedRenderCommandSink(RenderSurfaceAccess& surfaces)
		: surfaces_(surfaces)
	{
	}

	bool fillSurface(const RenderSurfaceFillCommand& command) override
	{
		if (command.surface == 0) return false;

		MutableRenderSurface mapping;
		try
		{
			if (!surfaces_.map(command.surface, mapping)) return false;
		}
		catch (...)
		{
			return false;
		}
		MappingLease lease(surfaces_, command.surface);
		if (!IsValidRenderSurfaceMapping(mapping) ||
			mapping.description.format == RenderPixelFormat::Indexed8)
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
			encode(command.color, mapping.description.format, encoded);
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

	bool copySurface(const RenderSurfaceCopyCommand& command) override
	{
		if (command.source == 0 || command.destination == 0)
			return false;

		RenderSurfaceDescription sourceDescription;
		RenderSurfaceDescription destinationDescription;
		if (!describe(command.source, sourceDescription))
			return false;
		if (command.source == command.destination)
			destinationDescription = sourceDescription;
		else if (!describe(command.destination, destinationDescription))
			return false;
		if (sourceDescription.format != destinationDescription.format ||
			sourceDescription.contentBitDepth !=
				destinationDescription.contentBitDepth ||
			!supports(command.mode, sourceDescription.format))
			return false;

		CopyArea area{
			command.sourceRegion.left,
			command.sourceRegion.top,
			command.sourceRegion.right,
			command.sourceRegion.bottom,
			command.destinationOrigin.x,
			command.destinationOrigin.y};
		if (!clip(
				sourceDescription, destinationDescription, area))
			return true;

		MutableRenderSurface sourceMapping;
		if (!map(command.source, sourceMapping)) return false;
		MappingLease sourceLease(surfaces_, command.source);
		if (!IsValidRenderSurfaceMapping(sourceMapping) ||
			sourceMapping.description != sourceDescription)
			return false;

		if (command.source == command.destination)
		{
			const bool copied =
				copyPixels(command, area, sourceMapping, sourceMapping, true);
			const bool unmapped = sourceLease.close();
			return copied && unmapped;
		}

		MutableRenderSurface destinationMapping;
		if (!map(command.destination, destinationMapping)) return false;
		MappingLease destinationLease(surfaces_, command.destination);
		if (!IsValidRenderSurfaceMapping(destinationMapping) ||
			destinationMapping.description != destinationDescription)
			return false;

		const bool copied =
			copyPixels(command, area, sourceMapping, destinationMapping, false);
		const bool destinationUnmapped = destinationLease.close();
		const bool sourceUnmapped = sourceLease.close();
		return copied && destinationUnmapped && sourceUnmapped;
	}

	RenderSurfaceAccess& surfaces() const { return surfaces_; }

private:
	struct CopyArea
	{
		std::int64_t sourceLeft;
		std::int64_t sourceTop;
		std::int64_t sourceRight;
		std::int64_t sourceBottom;
		std::int64_t destinationX;
		std::int64_t destinationY;
	};

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

	bool describe(
		RenderSurfaceId surface,
		RenderSurfaceDescription& description) const noexcept
	{
		try
		{
			return surfaces_.describe(surface, description) &&
				IsValidRenderSurfaceDescription(description);
		}
		catch (...)
		{
			return false;
		}
	}

	bool map(
		RenderSurfaceId surface,
		MutableRenderSurface& mapping) noexcept
	{
		try
		{
			return surfaces_.map(surface, mapping);
		}
		catch (...)
		{
			return false;
		}
	}

	static bool supports(
		RenderSurfaceCopyMode mode, RenderPixelFormat format)
	{
		switch (mode)
		{
		case RenderSurfaceCopyMode::Opaque:
			return true;
		case RenderSurfaceCopyMode::SourceColorKeyRgb:
			return format != RenderPixelFormat::Indexed8;
		}
		return false;
	}

	static bool clip(
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

	static bool copyPixels(
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
			copyOpaque(
				area, source, destination, sameSurface,
				pixelBytes, width, height);
			return true;
		case RenderSurfaceCopyMode::SourceColorKeyRgb:
			copyColorKeyed(
				command.sourceColorKey, area, source, destination,
				sameSurface, width, height);
			return true;
		}
		return false;
	}

	static bool copyBottomUp(const CopyArea& area, bool sameSurface)
	{
		return sameSurface &&
			area.destinationY > area.sourceTop;
	}

	static std::size_t rowIndex(
		std::size_t iteration, std::size_t height, bool bottomUp)
	{
		return bottomUp ? height - iteration - 1 : iteration;
	}

	static void copyOpaque(
		const CopyArea& area,
		const MutableRenderSurface& source,
		MutableRenderSurface& destination,
		bool sameSurface,
		std::size_t pixelBytes,
		std::size_t width,
		std::size_t height)
	{
		const std::size_t rowBytes = width * pixelBytes;
		const bool bottomUp = copyBottomUp(area, sameSurface);
		for (std::size_t iteration = 0; iteration < height; ++iteration)
		{
			const std::size_t row =
				rowIndex(iteration, height, bottomUp);
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

	static void copyColorKeyed(
		RenderColor colorKey,
		const CopyArea& area,
		const MutableRenderSurface& source,
		MutableRenderSurface& destination,
		bool sameSurface,
		std::size_t width,
		std::size_t height)
	{
		std::array<std::byte, 4> encodedKey{};
		(void)encode(colorKey, source.description.format, encodedKey);
		switch (source.description.format)
		{
		case RenderPixelFormat::Rgb565:
		{
			std::uint16_t packedKey = 0;
			std::memcpy(
				&packedKey, encodedKey.data(), sizeof(packedKey));
			copyColorKeyedPixels(
				packedKey, std::numeric_limits<std::uint16_t>::max(),
				area, source, destination, sameSurface, width, height);
			break;
		}
		case RenderPixelFormat::Argb8888:
		{
			std::uint32_t packedKey = 0;
			std::memcpy(
				&packedKey, encodedKey.data(), sizeof(packedKey));
			copyColorKeyedPixels(
				packedKey, 0x00ffffffu, area, source, destination,
				sameSurface, width, height);
			break;
		}
		case RenderPixelFormat::Indexed8:
			break;
		}
	}

	template <typename Pixel>
	static void copyColorKeyedPixels(
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
		const bool bottomUp = copyBottomUp(area, sameSurface);
		const bool rightToLeft =
			sameSurface &&
			area.destinationY == area.sourceTop &&
			area.destinationX > area.sourceLeft;
		for (std::size_t rowIteration = 0;
			rowIteration < height; ++rowIteration)
		{
			const std::size_t row =
				rowIndex(rowIteration, height, bottomUp);
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

	static std::size_t encode(
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
			break;
		}
		return 0;
	}

	RenderSurfaceAccess& surfaces_;
};

#endif
