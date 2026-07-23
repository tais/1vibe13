#ifndef ENGINE_CORE_FRAME_INVALIDATOR_H
#define ENGINE_CORE_FRAME_INVALIDATOR_H

#include <cstddef>
#include <cstdint>
#include <vector>

// Half-open framebuffer coordinates: [left, right) x [top, bottom).
// Implementations own clipping to their current framebuffer extent and ignore
// requests that are empty after clipping.
struct FrameRegion
{
	std::int32_t left = 0;
	std::int32_t top = 0;
	std::int32_t right = 0;
	std::int32_t bottom = 0;
};

inline bool operator==(const FrameRegion& left, const FrameRegion& right)
{
	return left.left == right.left && left.top == right.top &&
		left.right == right.right && left.bottom == right.bottom;
}

inline bool operator!=(const FrameRegion& left, const FrameRegion& right)
{
	return !(left == right);
}

// Engine-owned boundary for submitting framebuffer damage. Rendering remains
// adapter-owned while Core and headless hosts can select where damage goes.
class FrameInvalidator
{
public:
	virtual ~FrameInvalidator() = default;
	virtual void invalidateRegion(FrameRegion region) = 0;
	virtual void invalidateAll() = 0;

	// Records a semantic visual change that has no additional damage rectangle.
	// This preserves legacy frame telemetry without forcing a full redraw.
	virtual void markChanged() = 0;
};

class NullFrameInvalidator final : public FrameInvalidator
{
public:
	void invalidateRegion(FrameRegion) override {}
	void invalidateAll() override {}
	void markChanged() override {}
	static NullFrameInvalidator& instance()
	{
		static NullFrameInvalidator invalidator;
		return invalidator;
	}
};

// Capture-only invalidator for headless hosts and deterministic assertions.
class RecordingFrameInvalidator final : public FrameInvalidator
{
public:
	void invalidateRegion(FrameRegion region) override
	{
		regions_.push_back(region);
	}
	void invalidateAll() override { ++fullInvalidations_; }
	void markChanged() override { ++changeMarks_; }

	const std::vector<FrameRegion>& regions() const { return regions_; }
	std::size_t fullInvalidations() const { return fullInvalidations_; }
	std::size_t changeMarks() const { return changeMarks_; }
	void clear()
	{
		regions_.clear();
		fullInvalidations_ = 0;
		changeMarks_ = 0;
	}

private:
	std::vector<FrameRegion> regions_;
	std::size_t fullInvalidations_ = 0;
	std::size_t changeMarks_ = 0;
};

#endif
