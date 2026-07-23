#ifndef ENGINE_CORE_RENDER_COMMANDS_H
#define ENGINE_CORE_RENDER_COMMANDS_H

#include <Engine/Core/RenderSurfaceAccess.h>

#include <cstdint>
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

// Half-open surface coordinates: [left, right) x [top, bottom).
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

struct RenderSurfaceStretchCommand
{
	RenderSurfaceId source = 0;
	RenderSurfaceId destination = 0;
	RenderSurfaceRegion sourceRegion;
	RenderSurfaceRegion destinationRegion;
	RenderSurfaceCopyMode mode = RenderSurfaceCopyMode::Opaque;
	RenderColor sourceColorKey;
};

inline bool operator==(
	const RenderSurfaceStretchCommand& left,
	const RenderSurfaceStretchCommand& right)
{
	return left.source == right.source &&
		left.destination == right.destination &&
		left.sourceRegion == right.sourceRegion &&
		left.destinationRegion == right.destinationRegion &&
		left.mode == right.mode &&
		left.sourceColorKey == right.sourceColorKey;
}

inline bool operator!=(
	const RenderSurfaceStretchCommand& left,
	const RenderSurfaceStretchCommand& right)
{
	return !(left == right);
}

// Multiplies RGB channels by numerator / denominator while preserving alpha.
// The fraction must be in [0, 1]. Regions are clipped to the actual surface.
struct RenderSurfaceShadeCommand
{
	RenderSurfaceId surface = 0;
	RenderSurfaceRegion region;
	std::uint16_t numerator = 1;
	std::uint16_t denominator = 1;
};

inline bool operator==(
	const RenderSurfaceShadeCommand& left,
	const RenderSurfaceShadeCommand& right)
{
	return left.surface == right.surface &&
		left.region == right.region &&
		left.numerator == right.numerator &&
		left.denominator == right.denominator;
}

inline bool operator!=(
	const RenderSurfaceShadeCommand& left,
	const RenderSurfaceShadeCommand& right)
{
	return !(left == right);
}

// Fills a half-open region of a Depth16 surface with one unsigned depth value.
// Padding bytes are never written. Depth remains separate from colour commands
// so a host cannot accidentally reinterpret ordering data as RGB565 pixels.
struct RenderDepthFillCommand
{
	RenderSurfaceId surface = 0;
	RenderSurfaceRegion region;
	std::uint16_t depth = 0;
};

inline bool operator==(
	const RenderDepthFillCommand& left,
	const RenderDepthFillCommand& right)
{
	return left.surface == right.surface &&
		left.region == right.region &&
		left.depth == right.depth;
}

inline bool operator!=(
	const RenderDepthFillCommand& left,
	const RenderDepthFillCommand& right)
{
	return !(left == right);
}

// Opaque image identity supplied by the host's render-resource adapter. Zero
// is reserved as "no image". Unlike a native pointer, this value can be
// recorded, inspected by headless hosts, and forwarded across an engine
// boundary without exposing the backing image representation.
using RenderImageId = std::uint64_t;

enum class RenderImageCompositeMode : std::uint8_t
{
	Opaque,
	SourceTransparency,
	Shadow,
	Intensity
};

// Draws one frame/sub-image at its anchor point inside an explicit half-open
// clipping region. Image-local offsets, palettes, compression, and physical
// storage remain responsibilities of the host adapter. Shadow and Intensity
// use visible image runs as a mask which transforms the destination rather than
// sampling palette colours. Callers and recording hosts see only stable values.
struct RenderImageDrawCommand
{
	RenderSurfaceId destination = 0;
	RenderImageId image = 0;
	std::uint32_t frame = 0;
	RenderSurfacePoint destinationOrigin;
	RenderSurfaceRegion clippingRegion;
	RenderImageCompositeMode mode =
		RenderImageCompositeMode::SourceTransparency;
};

inline bool operator==(
	const RenderImageDrawCommand& left,
	const RenderImageDrawCommand& right)
{
	return left.destination == right.destination &&
		left.image == right.image &&
		left.frame == right.frame &&
		left.destinationOrigin == right.destinationOrigin &&
		left.clippingRegion == right.clippingRegion &&
		left.mode == right.mode;
}

inline bool operator!=(
	const RenderImageDrawCommand& left,
	const RenderImageDrawCommand& right)
{
	return !(left == right);
}

enum class RenderDepthCompareMode : std::uint8_t
{
	GreaterOrEqual,
	Greater
};

enum class RenderDepthWriteMode : std::uint8_t
{
	Preserve,
	ReplaceOnPass
};

enum class RenderImageDepthEffect : std::uint8_t
{
	SourcePalette,
	ShadeDestination,
	IntensifyDestination
};

// Draws the visible runs of one image frame after a depth test. SourcePalette
// pairs with an inclusive test and writes palette colours; destination
// shade/intensity effects use the image as a mask and pair with a strict test.
// Colour and depth storage remain separate resources, and ReplaceOnPass updates
// depth only for visible source pixels which pass. More specialized alpha,
// pixelation, and obscured semantics use distinct commands.
struct RenderImageDepthDrawCommand
{
	RenderSurfaceId destination = 0;
	RenderSurfaceId depthSurface = 0;
	RenderImageId image = 0;
	std::uint32_t frame = 0;
	RenderSurfacePoint destinationOrigin;
	RenderSurfaceRegion clippingRegion;
	std::uint16_t depth = 0;
	RenderDepthCompareMode comparison =
		RenderDepthCompareMode::GreaterOrEqual;
	RenderDepthWriteMode depthWrite =
		RenderDepthWriteMode::ReplaceOnPass;
	RenderImageDepthEffect effect =
		RenderImageDepthEffect::SourcePalette;
};

inline bool operator==(
	const RenderImageDepthDrawCommand& left,
	const RenderImageDepthDrawCommand& right)
{
	return left.destination == right.destination &&
		left.depthSurface == right.depthSurface &&
		left.image == right.image &&
		left.frame == right.frame &&
		left.destinationOrigin == right.destinationOrigin &&
		left.clippingRegion == right.clippingRegion &&
		left.depth == right.depth &&
		left.comparison == right.comparison &&
		left.depthWrite == right.depthWrite &&
		left.effect == right.effect;
}

inline bool operator!=(
	const RenderImageDepthDrawCommand& left,
	const RenderImageDepthDrawCommand& right)
{
	return !(left == right);
}

enum class RenderImageOutlineMode : std::uint8_t
{
	Color,
	Shadow
};

// Draws the outline-aware form of one image frame. Color mode renders normal
// image pixels and either paints or skips image-defined outline markers.
// Shadow mode darkens destination pixels covered by normal image pixels while
// leaving outline markers untouched. Physical marker values and shade tables
// remain host-adapter details.
struct RenderImageOutlineCommand
{
	RenderSurfaceId destination = 0;
	RenderImageId image = 0;
	std::uint32_t frame = 0;
	RenderSurfacePoint destinationOrigin;
	RenderSurfaceRegion clippingRegion;
	RenderImageOutlineMode mode = RenderImageOutlineMode::Color;
	RenderColor color;
	bool drawOutline = false;
};

inline bool operator==(
	const RenderImageOutlineCommand& left,
	const RenderImageOutlineCommand& right)
{
	return left.destination == right.destination &&
		left.image == right.image &&
		left.frame == right.frame &&
		left.destinationOrigin == right.destinationOrigin &&
		left.clippingRegion == right.clippingRegion &&
		left.mode == right.mode &&
		left.color == right.color &&
		left.drawOutline == right.drawOutline;
}

inline bool operator!=(
	const RenderImageOutlineCommand& left,
	const RenderImageOutlineCommand& right)
{
	return !(left == right);
}

// High-level renderer boundary. Commands use engine values and opaque surface
// and image identities; hosts decide whether to execute, record, forward, or
// reject them. New command methods default to rejection so existing external
// sinks remain source-compatible as the SDK surface grows.
class RenderCommandSink
{
public:
	virtual ~RenderCommandSink() = default;
	virtual bool fillSurface(const RenderSurfaceFillCommand& command) = 0;
	virtual bool copySurface(const RenderSurfaceCopyCommand&) { return false; }
	virtual bool stretchSurface(const RenderSurfaceStretchCommand&)
	{
		return false;
	}
	virtual bool shadeSurface(const RenderSurfaceShadeCommand&) { return false; }
	virtual bool fillDepth(const RenderDepthFillCommand&) { return false; }
	virtual bool drawImage(const RenderImageDrawCommand&) { return false; }
	virtual bool drawImageDepth(const RenderImageDepthDrawCommand&)
	{
		return false;
	}
	virtual bool drawImageOutline(const RenderImageOutlineCommand&)
	{
		return false;
	}
};

class NullRenderCommandSink final : public RenderCommandSink
{
public:
	bool fillSurface(const RenderSurfaceFillCommand&) override { return false; }
	bool copySurface(const RenderSurfaceCopyCommand&) override { return false; }
	bool stretchSurface(const RenderSurfaceStretchCommand&) override
	{
		return false;
	}
	bool shadeSurface(const RenderSurfaceShadeCommand&) override { return false; }
	bool fillDepth(const RenderDepthFillCommand&) override { return false; }
	bool drawImage(const RenderImageDrawCommand&) override { return false; }
	bool drawImageDepth(const RenderImageDepthDrawCommand&) override
	{
		return false;
	}
	bool drawImageOutline(const RenderImageOutlineCommand&) override
	{
		return false;
	}
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
		fillCommands_.push_back(command);
		return accepting_;
	}

	bool copySurface(const RenderSurfaceCopyCommand& command) override
	{
		copyCommands_.push_back(command);
		return accepting_;
	}

	bool stretchSurface(const RenderSurfaceStretchCommand& command) override
	{
		stretchCommands_.push_back(command);
		return accepting_;
	}

	bool shadeSurface(const RenderSurfaceShadeCommand& command) override
	{
		shadeCommands_.push_back(command);
		return accepting_;
	}

	bool fillDepth(const RenderDepthFillCommand& command) override
	{
		depthFillCommands_.push_back(command);
		return accepting_;
	}

	bool drawImage(const RenderImageDrawCommand& command) override
	{
		imageCommands_.push_back(command);
		return accepting_;
	}

	bool drawImageDepth(
		const RenderImageDepthDrawCommand& command) override
	{
		imageDepthCommands_.push_back(command);
		return accepting_;
	}

	bool drawImageOutline(const RenderImageOutlineCommand& command) override
	{
		imageOutlineCommands_.push_back(command);
		return accepting_;
	}

	const std::vector<RenderSurfaceFillCommand>& commands() const
	{
		return fillCommands_;
	}
	const std::vector<RenderSurfaceCopyCommand>& copyCommands() const
	{
		return copyCommands_;
	}
	const std::vector<RenderSurfaceStretchCommand>& stretchCommands() const
	{
		return stretchCommands_;
	}
	const std::vector<RenderSurfaceShadeCommand>& shadeCommands() const
	{
		return shadeCommands_;
	}
	const std::vector<RenderDepthFillCommand>& depthFillCommands() const
	{
		return depthFillCommands_;
	}
	const std::vector<RenderImageDrawCommand>& imageCommands() const
	{
		return imageCommands_;
	}
	const std::vector<RenderImageDepthDrawCommand>&
	imageDepthCommands() const
	{
		return imageDepthCommands_;
	}
	const std::vector<RenderImageOutlineCommand>& imageOutlineCommands() const
	{
		return imageOutlineCommands_;
	}
	void setAccepting(bool accepting) { accepting_ = accepting; }
	void clear()
	{
		fillCommands_.clear();
		copyCommands_.clear();
		stretchCommands_.clear();
		shadeCommands_.clear();
		depthFillCommands_.clear();
		imageCommands_.clear();
		imageDepthCommands_.clear();
		imageOutlineCommands_.clear();
	}

private:
	std::vector<RenderSurfaceFillCommand> fillCommands_;
	std::vector<RenderSurfaceCopyCommand> copyCommands_;
	std::vector<RenderSurfaceStretchCommand> stretchCommands_;
	std::vector<RenderSurfaceShadeCommand> shadeCommands_;
	std::vector<RenderDepthFillCommand> depthFillCommands_;
	std::vector<RenderImageDrawCommand> imageCommands_;
	std::vector<RenderImageDepthDrawCommand> imageDepthCommands_;
	std::vector<RenderImageOutlineCommand> imageOutlineCommands_;
	bool accepting_ = true;
};

// CPU implementation shared by the compiled game, headless hosts, and tools.
// It maps only for the duration of one command and never writes row padding.
class MappedRenderCommandSink final : public RenderCommandSink
{
public:
	explicit MappedRenderCommandSink(RenderSurfaceAccess& surfaces);

	bool fillSurface(const RenderSurfaceFillCommand& command) override;
	bool copySurface(const RenderSurfaceCopyCommand& command) override;
	bool stretchSurface(const RenderSurfaceStretchCommand& command) override;
	bool shadeSurface(const RenderSurfaceShadeCommand& command) override;
	bool fillDepth(const RenderDepthFillCommand& command) override;

	RenderSurfaceAccess& surfaces() const;

private:
	RenderSurfaceAccess& surfaces_;
};

#endif
