#ifndef ENGINE_CORE_RENDER_SURFACE_ACCESS_H
#define ENGINE_CORE_RENDER_SURFACE_ACCESS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

// Opaque surface identity supplied by a RenderSurfaceAccess implementation.
// Zero is reserved as "no surface"; adapters may preserve established numeric
// handles when doing so is useful for source-compatible callers.
using RenderSurfaceId = std::uint64_t;

enum class RenderSurfaceRole : std::uint8_t
{
	Primary,
	BackBuffer,
	FrameBuffer,
	Cursor,
	Count
};

// Storage format of a mapped surface. contentBitDepth in the description is
// separate because compatibility assets may retain a 16-bit logical depth
// while the live framebuffer is stored as 32-bit ARGB.
enum class RenderPixelFormat : std::uint8_t
{
	Indexed8,
	Rgb565,
	Argb8888
};

inline std::size_t RenderPixelBytes(RenderPixelFormat format)
{
	switch (format)
	{
	case RenderPixelFormat::Indexed8: return 1;
	case RenderPixelFormat::Rgb565: return 2;
	case RenderPixelFormat::Argb8888: return 4;
	}
	return 0;
}

struct RenderSurfaceDescription
{
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	RenderPixelFormat format = RenderPixelFormat::Argb8888;
	std::uint8_t contentBitDepth = 0;
};

inline bool operator==(
	const RenderSurfaceDescription& left,
	const RenderSurfaceDescription& right)
{
	return left.width == right.width && left.height == right.height &&
		left.format == right.format &&
		left.contentBitDepth == right.contentBitDepth;
}

inline bool operator!=(
	const RenderSurfaceDescription& left,
	const RenderSurfaceDescription& right)
{
	return !(left == right);
}

struct MutableRenderSurface
{
	std::byte* pixels = nullptr;
	std::size_t sizeBytes = 0;
	std::size_t pitchBytes = 0;
	RenderSurfaceDescription description;

	explicit operator bool() const
	{
		return pixels != nullptr && sizeBytes != 0 && pitchBytes != 0;
	}
};

// Low-level, platform-neutral access to renderer-owned pixel surfaces. Mapped
// storage remains owned by the implementation and is valid only until the
// matching unmap call. Hosts must serialize mapping, renderer lifetime, and
// surface registration on the render thread.
class RenderSurfaceAccess
{
public:
	virtual ~RenderSurfaceAccess() = default;
	virtual RenderSurfaceId surfaceFor(RenderSurfaceRole role) const = 0;
	virtual bool describe(
		RenderSurfaceId surface, RenderSurfaceDescription& description) const = 0;
	virtual bool map(RenderSurfaceId surface, MutableRenderSurface& mapping) = 0;
	virtual void unmap(RenderSurfaceId surface) = 0;
};

class NullRenderSurfaceAccess final : public RenderSurfaceAccess
{
public:
	RenderSurfaceId surfaceFor(RenderSurfaceRole) const override { return 0; }
	bool describe(
		RenderSurfaceId, RenderSurfaceDescription&) const override
	{
		return false;
	}
	bool map(RenderSurfaceId, MutableRenderSurface&) override { return false; }
	void unmap(RenderSurfaceId) override {}
	static NullRenderSurfaceAccess& instance()
	{
		static NullRenderSurfaceAccess access;
		return access;
	}
};

// Deterministic in-memory implementation for headless hosts and package tests.
// Definitions cannot be replaced or removed while mapped, so successful map
// pointers retain their documented lifetime even if other surfaces are added.
class MemoryRenderSurfaceAccess final : public RenderSurfaceAccess
{
public:
	static constexpr std::size_t DefaultMaximumBytes =
		256u * 1024u * 1024u;

	explicit MemoryRenderSurfaceAccess(
		std::size_t maximumBytes = DefaultMaximumBytes)
		: maximumBytes_(maximumBytes)
	{
	}

	bool defineSurface(
		RenderSurfaceId surface, RenderSurfaceDescription description)
	{
		if (surface == 0 || description.width == 0 ||
			description.height == 0 || description.contentBitDepth == 0)
			return false;
		const std::size_t pixelBytes = RenderPixelBytes(description.format);
		if (pixelBytes == 0 ||
			description.width >
				std::numeric_limits<std::size_t>::max() / pixelBytes)
			return false;
		const std::size_t pitch =
			static_cast<std::size_t>(description.width) * pixelBytes;
		if (description.height >
			std::numeric_limits<std::size_t>::max() / pitch)
			return false;
		const std::size_t size =
			pitch * static_cast<std::size_t>(description.height);
		if (size == 0) return false;

		auto found = surfaces_.find(surface);
		if (found != surfaces_.end() && found->second.mappingCount != 0)
			return false;
		const std::size_t previousBytes =
			found == surfaces_.end() ? 0 : found->second.pixels.size();
		if (previousBytes > totalBytes_) return false;
		const std::size_t retainedBytes = totalBytes_ - previousBytes;
		if (retainedBytes > maximumBytes_ ||
			size > maximumBytes_ - retainedBytes)
			return false;
		try
		{
			Entry replacement;
			replacement.description = description;
			replacement.pitchBytes = pitch;
			replacement.pixels.resize(size);
			if (found == surfaces_.end())
				surfaces_.emplace(surface, std::move(replacement));
			else
				found->second = std::move(replacement);
			totalBytes_ = retainedBytes + size;
		}
		catch (...)
		{
			return false;
		}
		return true;
	}

	bool removeSurface(RenderSurfaceId surface)
	{
		auto found = surfaces_.find(surface);
		if (found == surfaces_.end() || found->second.mappingCount != 0)
			return false;
		if (found->second.pixels.size() > totalBytes_) return false;
		totalBytes_ -= found->second.pixels.size();
		surfaces_.erase(found);
		for (RenderSurfaceId& target : targets_)
			if (target == surface) target = 0;
		return true;
	}

	bool setSurfaceFor(RenderSurfaceRole role, RenderSurfaceId surface)
	{
		const std::size_t index = roleIndex(role);
		if (index >= targets_.size()) return false;
		if (surface != 0 && surfaces_.find(surface) == surfaces_.end())
			return false;
		targets_[index] = surface;
		return true;
	}

	RenderSurfaceId surfaceFor(RenderSurfaceRole role) const override
	{
		const std::size_t index = roleIndex(role);
		return index < targets_.size() ? targets_[index] : 0;
	}

	bool describe(
		RenderSurfaceId surface,
		RenderSurfaceDescription& description) const override
	{
		const auto found = surfaces_.find(surface);
		if (found == surfaces_.end()) return false;
		description = found->second.description;
		return true;
	}

	bool map(
		RenderSurfaceId surface, MutableRenderSurface& mapping) override
	{
		auto found = surfaces_.find(surface);
		if (found == surfaces_.end()) return false;
		Entry& entry = found->second;
		if (entry.mappingCount == std::numeric_limits<std::size_t>::max())
			return false;
		mapping = MutableRenderSurface{
			entry.pixels.data(), entry.pixels.size(), entry.pitchBytes,
			entry.description};
		++entry.mappingCount;
		return true;
	}

	void unmap(RenderSurfaceId surface) override
	{
		auto found = surfaces_.find(surface);
		if (found == surfaces_.end() || found->second.mappingCount == 0)
			return;
		--found->second.mappingCount;
	}

	std::size_t mappingCount(RenderSurfaceId surface) const
	{
		const auto found = surfaces_.find(surface);
		return found == surfaces_.end() ? 0 : found->second.mappingCount;
	}

	std::size_t size() const { return surfaces_.size(); }
	std::size_t totalBytes() const { return totalBytes_; }
	std::size_t maximumBytes() const { return maximumBytes_; }

private:
	struct Entry
	{
		RenderSurfaceDescription description;
		std::size_t pitchBytes = 0;
		std::vector<std::byte> pixels;
		std::size_t mappingCount = 0;
	};

	static std::size_t roleIndex(RenderSurfaceRole role)
	{
		return static_cast<std::size_t>(role);
	}

	std::size_t maximumBytes_;
	std::size_t totalBytes_ = 0;
	std::unordered_map<RenderSurfaceId, Entry> surfaces_;
	std::array<RenderSurfaceId,
		static_cast<std::size_t>(RenderSurfaceRole::Count)> targets_{};
};

#endif
