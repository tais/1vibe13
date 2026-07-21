#ifndef ENGINE_CORE_FRAME_DRIVER_H
#define ENGINE_CORE_FRAME_DRIVER_H

#include <cstdint>
#include <utility>

#include <Engine/Core/EngineServices.h>
#include <Engine/Core/FrameTelemetry.h>
#include <Engine/Core/InputDispatcher.h>
#include <Engine/Core/RuntimeUpdate.h>

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
	RuntimeUpdateDispatchResult runtimeUpdates;
	RuntimeMessageDispatchResult messages;
};

// Game-agnostic orchestration for one application frame. The application owns
// update/render policy and post-presentation bookkeeping; the engine owns their
// ordering, presentation, timing, and monotonically increasing frame identity.
// Exceptions deliberately propagate to the application's existing top-level
// recovery policy, and a failed frame is not counted as completed.
class FrameDriver
{
public:
	FrameDriver(EngineServices& services, RuntimeMessageBus& messages, InputDispatcher& input,
		RuntimeUpdateDispatcher& runtimeUpdates, FrameTelemetry& telemetry)
		: services_(services), messages_(messages), input_(input), runtimeUpdates_(runtimeUpdates),
		  telemetry_(telemetry) {}

	FrameDriver(const FrameDriver&) = delete;
	FrameDriver& operator=(const FrameDriver&) = delete;
	FrameDriver(FrameDriver&&) = delete;
	FrameDriver& operator=(FrameDriver&&) = delete;

	template<typename PrepareFrame, typename CompleteFrame>
	FrameRunResult runFrame(PrepareFrame&& prepareFrame, CompleteFrame&& completeFrame)
	{
		const std::uint64_t startedAt = services_.time.nowMicroseconds();
		const std::uint64_t sequence = completedFrames_ + 1;
		const RuntimeMessageDispatchResult messages = messages_.dispatchPending();
		const std::uint64_t messagesFinishedAt = services_.time.nowMicroseconds();
		const InputDispatchResult input = input_.dispatchPending();
		const std::uint64_t inputFinishedAt = services_.time.nowMicroseconds();
		const std::uint64_t elapsed = hasCompletedFrame_ && startedAt >= previousFrameStartedAt_
			? startedAt - previousFrameStartedAt_ : 0;
		const RuntimeUpdateDispatchResult runtimeUpdates = runtimeUpdates_.dispatch(
			RuntimeUpdateContext{sequence, startedAt, elapsed});
		const std::uint64_t runtimeUpdateFinishedAt = services_.time.nowMicroseconds();
		const FramePlan plan = std::forward<PrepareFrame>(prepareFrame)();
		const std::uint64_t applicationUpdateFinishedAt = services_.time.nowMicroseconds();
		if (plan.present)
			services_.frames.present(plan.presentationMode);
		const std::uint64_t presentationFinishedAt = services_.time.nowMicroseconds();
		std::forward<CompleteFrame>(completeFrame)();
		const std::uint64_t finishedAt = services_.time.nowMicroseconds();
		completedFrames_ = sequence;
		previousFrameStartedAt_ = startedAt;
		hasCompletedFrame_ = true;
		const FrameRunResult result{
			sequence, startedAt, finishedAt,
			plan.present, plan.presentationMode, input, runtimeUpdates, messages};
		telemetry_.record(FrameTelemetrySample{
			sequence, startedAt, messagesFinishedAt, inputFinishedAt, runtimeUpdateFinishedAt,
			applicationUpdateFinishedAt, presentationFinishedAt, finishedAt,
			plan.present, plan.presentationMode, messages, input, runtimeUpdates});
		return result;
	}

	std::uint64_t completedFrames() const { return completedFrames_; }
	void resetFrameSequence()
	{
		completedFrames_ = 0;
		previousFrameStartedAt_ = 0;
		hasCompletedFrame_ = false;
	}

private:
	EngineServices& services_;
	RuntimeMessageBus& messages_;
	InputDispatcher& input_;
	RuntimeUpdateDispatcher& runtimeUpdates_;
	FrameTelemetry& telemetry_;
	std::uint64_t completedFrames_ = 0;
	std::uint64_t previousFrameStartedAt_ = 0;
	bool hasCompletedFrame_ = false;
};

#endif
