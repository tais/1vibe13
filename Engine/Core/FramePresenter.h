#ifndef ENGINE_CORE_FRAME_PRESENTER_H
#define ENGINE_CORE_FRAME_PRESENTER_H

#include <vector>

enum class FramePresentMode
{
	Paced,
	Immediate
};

// Engine-owned presentation boundary. Drawing and dirty-region ownership stay
// with the current renderer; this contract only schedules a completed frame
// for display, allowing headless hosts to run without a window or GPU.
class FramePresenter
{
public:
	virtual ~FramePresenter() = default;
	virtual void present(FramePresentMode mode) = 0;
};

class NullFramePresenter final : public FramePresenter
{
public:
	void present(FramePresentMode) override {}
	static NullFramePresenter& instance()
	{
		static NullFramePresenter presenter;
		return presenter;
	}
};

class RecordingFramePresenter final : public FramePresenter
{
public:
	void present(FramePresentMode mode) override { presentations_.push_back(mode); }
	const std::vector<FramePresentMode>& presentations() const { return presentations_; }
	void clear() { presentations_.clear(); }

private:
	std::vector<FramePresentMode> presentations_;
};

#endif
