#ifndef ENGINE_CORE_RENDER_COMMANDS_H
#define ENGINE_CORE_RENDER_COMMANDS_H

#include <Engine/Core/RenderSurfaceAccess.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

// High-level renderer boundary. Commands use engine values and opaque surface
// identities; hosts decide whether to execute, record, forward, or reject them.
class RenderCommandSink
{
public:
	virtual ~RenderCommandSink() = default;
	virtual bool fillSurface(const RenderSurfaceFillCommand& command) = 0;
};

class NullRenderCommandSink final : public RenderCommandSink
{
public:
	bool fillSurface(const RenderSurfaceFillCommand&) override { return false; }
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

	const std::vector<RenderSurfaceFillCommand>& commands() const
	{
		return commands_;
	}
	void setAccepting(bool accepting) { accepting_ = accepting; }
	void clear() { commands_.clear(); }

private:
	std::vector<RenderSurfaceFillCommand> commands_;
	bool accepting_ = true;
};

// CPU implementation shared by the compiled game, headless hosts, and tools.
// It maps only for the duration of one command, never writes row padding, and
// supports the renderer's true-colour storage formats.
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

	RenderSurfaceAccess& surfaces() const { return surfaces_; }

private:
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
