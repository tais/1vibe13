#ifndef ENGINE_CORE_FRAME_DRIVER_H
#define ENGINE_CORE_FRAME_DRIVER_H

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

#include <Engine/Core/EngineServices.h>
#include <Engine/Core/FrameTelemetry.h>
#include <Engine/Core/InputDispatcher.h>
#include <Engine/Core/RuntimeUpdate.h>
#include <Engine/Core/SimulationTick.h>

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
	SimulationTickDispatchResult simulationTicks;
	RuntimeMessageDispatchResult messages;
};

// Game-agnostic orchestration for one application frame. The application owns
// update/render policy and post-presentation bookkeeping; the engine owns their
// ordering, presentation, timing, and monotonically increasing frame identity.
// Exceptions deliberately propagate to the application's existing top-level
// recovery policy, and a failed frame is not counted as completed. Attempt
// identity and committed simulation time still advance so recovery cannot
// reuse a frame ID or simulate the same elapsed interval twice.
class FrameDriver
{
public:
	FrameDriver(EngineServices& services, RuntimeMessageBus& messages, InputDispatcher& input,
		RuntimeUpdateDispatcher& runtimeUpdates, FrameTelemetry& telemetry,
		SimulationTickDispatcher& simulationTicks = SimulationTickDispatcher::disabled())
		: services_(services), messages_(messages), input_(input), runtimeUpdates_(runtimeUpdates),
		  telemetry_(telemetry), simulationTicks_(simulationTicks) {}

	FrameDriver(const FrameDriver&) = delete;
	FrameDriver& operator=(const FrameDriver&) = delete;
	FrameDriver(FrameDriver&&) = delete;
	FrameDriver& operator=(FrameDriver&&) = delete;

	template<typename PrepareFrame, typename CompleteFrame>
	FrameRunResult runFrame(PrepareFrame&& prepareFrame, CompleteFrame&& completeFrame)
	{
		if (runningFrame_)
			throw std::logic_error("frame execution already in progress");
		if (frameSequenceExhausted_)
			throw std::overflow_error("frame sequence exhausted");
		if (completedFrames_ == std::numeric_limits<std::uint64_t>::max())
			throw std::overflow_error("completed frame count exhausted");
		FrameGuard frame(runningFrame_);
		const std::uint64_t sequence = nextFrameSequence_;
		if (nextFrameSequence_ == std::numeric_limits<std::uint64_t>::max())
			frameSequenceExhausted_ = true;
		else
			++nextFrameSequence_;
		const std::uint64_t startedAt = services_.time.nowMicroseconds();
		const RuntimeMessageDispatchResult messages = messages_.dispatchPending();
		const std::uint64_t messagesFinishedAt = services_.time.nowMicroseconds();
		const InputDispatchResult input = input_.dispatchPending();
		const std::uint64_t inputFinishedAt = services_.time.nowMicroseconds();
		const std::uint64_t elapsed = hasAdvancedSimulation_ &&
			startedAt >= previousSimulationStartedAt_
			? startedAt - previousSimulationStartedAt_ : 0;
		const SimulationTickDispatchResult simulationTicks = simulationTicks_.advance(elapsed);
		previousSimulationStartedAt_ = startedAt;
		hasAdvancedSimulation_ = true;
		const std::uint64_t simulationTicksFinishedAt = services_.time.nowMicroseconds();
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
		++completedFrames_;
		const FrameRunResult result{
			sequence, startedAt, finishedAt,
			plan.present, plan.presentationMode, input, runtimeUpdates, simulationTicks, messages};
		telemetry_.record(FrameTelemetrySample{
			sequence, startedAt, messagesFinishedAt, inputFinishedAt,
			simulationTicksFinishedAt, runtimeUpdateFinishedAt,
			applicationUpdateFinishedAt, presentationFinishedAt, finishedAt,
			plan.present, plan.presentationMode, messages, input, runtimeUpdates,
			simulationTicks});
		return result;
	}

	std::uint64_t completedFrames() const { return completedFrames_; }
	std::uint64_t nextFrameSequence() const { return nextFrameSequence_; }
	void resetFrameSequence()
	{
		if (runningFrame_) return;
		completedFrames_ = 0;
		nextFrameSequence_ = 1;
		frameSequenceExhausted_ = false;
		previousSimulationStartedAt_ = 0;
		hasAdvancedSimulation_ = false;
		simulationTicks_.reset();
	}

private:
	class FrameGuard
	{
	public:
		explicit FrameGuard(bool& running) : running_(running) { running_ = true; }
		~FrameGuard() { running_ = false; }
	private:
		bool& running_;
	};

	EngineServices& services_;
	RuntimeMessageBus& messages_;
	InputDispatcher& input_;
	RuntimeUpdateDispatcher& runtimeUpdates_;
	FrameTelemetry& telemetry_;
	SimulationTickDispatcher& simulationTicks_;
	std::uint64_t completedFrames_ = 0;
	std::uint64_t nextFrameSequence_ = 1;
	std::uint64_t previousSimulationStartedAt_ = 0;
	bool frameSequenceExhausted_ = false;
	bool hasAdvancedSimulation_ = false;
	bool runningFrame_ = false;
};

#endif
