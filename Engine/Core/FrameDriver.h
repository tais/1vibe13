#ifndef ENGINE_CORE_FRAME_DRIVER_H
#define ENGINE_CORE_FRAME_DRIVER_H

#include <cstdint>
#include <utility>

#include <Engine/Core/EngineServices.h>
#include <Engine/Core/InputDispatcher.h>

struct FramePlan
{
	bool present = true;
	FramePresentMode presentationMode = FramePresentMode::Paced;
};

struct FrameRunResult
{
	std::uint64_t sequence = 0;
	std::uint64_t startedAtMicroseconds = 0;
	std::uint64_t finishedAtMicroseconds = 0;
	bool presented = false;
	FramePresentMode presentationMode = FramePresentMode::Paced;
	InputDispatchResult input;
};

// Game-agnostic orchestration for one application frame. The application owns
// update/render policy and post-presentation bookkeeping; the engine owns their
// ordering, presentation, timing, and monotonically increasing frame identity.
// Exceptions deliberately propagate to the application's existing top-level
// recovery policy, and a failed frame is not counted as completed.
class FrameDriver
{
public:
	FrameDriver(EngineServices& services, InputDispatcher& input)
		: services_(services), input_(input) {}

	FrameDriver(const FrameDriver&) = delete;
	FrameDriver& operator=(const FrameDriver&) = delete;
	FrameDriver(FrameDriver&&) = delete;
	FrameDriver& operator=(FrameDriver&&) = delete;

	template<typename PrepareFrame, typename CompleteFrame>
	FrameRunResult runFrame(PrepareFrame&& prepareFrame, CompleteFrame&& completeFrame)
	{
		const std::uint64_t startedAt = services_.time.nowMicroseconds();
		const InputDispatchResult input = input_.dispatchPending();
		const FramePlan plan = std::forward<PrepareFrame>(prepareFrame)();
		if (plan.present)
			services_.frames.present(plan.presentationMode);
		std::forward<CompleteFrame>(completeFrame)();
		const std::uint64_t sequence = ++completedFrames_;
		return FrameRunResult{
			sequence, startedAt, services_.time.nowMicroseconds(),
			plan.present, plan.presentationMode, input};
	}

	std::uint64_t completedFrames() const { return completedFrames_; }
	void resetFrameSequence() { completedFrames_ = 0; }

private:
	EngineServices& services_;
	InputDispatcher& input_;
	std::uint64_t completedFrames_ = 0;
};

#endif
