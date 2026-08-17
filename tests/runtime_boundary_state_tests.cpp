#include <Engine/Core/FrameDriver.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
using FrameError = FrameDriverBoundaryStateError;
using FrameState = FrameDriverBoundaryState;
using TickError = SimulationTickBoundaryStateError;
using TickState = SimulationTickBoundaryState;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::fprintf(stderr, "FAIL: %s\n", message);
	std::exit(1);
}

struct FrameHarness
{
	ManualTimeSource time;
	MemoryInputSource inputSource;
	EngineServices services{time};
	RuntimeMessageBus messages;
	InputDispatcher input{inputSource};
	RuntimeUpdateDispatcher updates;
	FrameTelemetry telemetry;
	SimulationTickDispatcher ticks;
	FrameDriver driver;

	explicit FrameHarness(
		std::uint64_t stepMicroseconds = 10,
		std::size_t maxCatchUpTicks = 4)
		: ticks(stepMicroseconds, maxCatchUpTicks),
		  driver(services, messages, input, updates, telemetry, ticks)
	{
	}

	FrameRunResult run()
	{
		return driver.runFrame(
			[] { return FramePlan{false, FramePresentMode::Paced}; }, [] {});
	}
};

class RecordingUpdateSink final : public RuntimeUpdateSink
{
public:
	void updateRuntime(const RuntimeUpdateContext& context) override
	{
		updates.push_back(context);
	}

	std::vector<RuntimeUpdateContext> updates;
};

class BoundaryInspectingTickSink final : public SimulationTickSink
{
public:
	SimulationTickDispatcher* dispatcher = nullptr;
	TickState candidate;
	TickError captureError = TickError::None;
	TickError validationError = TickError::None;
	TickError restoreError = TickError::None;
	TickState failedCaptureState;

	void simulate(const SimulationTickContext&) override
	{
		const SimulationTickBoundaryStateCaptureResult captured =
			dispatcher->captureBoundaryState();
		captureError = captured.error;
		failedCaptureState = captured.state;
		validationError = dispatcher->validateBoundaryState(candidate);
		restoreError = dispatcher->restoreBoundaryState(candidate);
	}
};

void TestValueContractsAndFailClosedDefaults()
{
	static_assert(std::is_standard_layout<FrameState>::value,
		"frame boundary state remains a data-only value");
	static_assert(std::is_trivially_copyable<FrameState>::value,
		"frame boundary state remains trivially copyable");
	static_assert(std::is_standard_layout<TickState>::value,
		"tick boundary state remains a data-only value");
	static_assert(std::is_trivially_copyable<TickState>::value,
		"tick boundary state remains trivially copyable");
	static_assert(noexcept(std::declval<const FrameDriver&>().captureBoundaryState()),
		"frame capture is non-throwing");
	static_assert(noexcept(std::declval<FrameDriver&>().restoreBoundaryState(
		std::declval<const FrameState&>())), "frame restore is non-throwing");
	static_assert(noexcept(
		std::declval<const SimulationTickDispatcher&>().captureBoundaryState()),
		"tick capture is non-throwing");
	static_assert(noexcept(
		std::declval<SimulationTickDispatcher&>().restoreBoundaryState(
			std::declval<const TickState&>())), "tick restore is non-throwing");

	FrameHarness frames;
	Check(!FrameDriverBoundaryStateCaptureResult{},
		"default frame capture result fails closed");
	const FrameDriverBoundaryStateCaptureResult initialFrame =
		frames.driver.captureBoundaryState();
	Check(initialFrame && initialFrame.state == FrameState{0, 1, false},
		"initial frame identity captures the first unconsumed sequence");
	Check(frames.driver.validateBoundaryState(FrameState{}) ==
		FrameError::InvalidNextFrameSequence,
		"default frame state fails closed");

	SimulationTickDispatcher ticks(10, 4);
	Check(!SimulationTickBoundaryStateCaptureResult{},
		"default tick capture result fails closed");
	const SimulationTickBoundaryStateCaptureResult initialTick =
		ticks.captureBoundaryState();
	Check(initialTick && initialTick.state == TickState{10, 4, 0, 0, 0, false},
		"initial tick state captures configuration and all counters");
	Check(ticks.validateBoundaryState(TickState{}) ==
		TickError::InvalidConfiguration,
		"default tick state fails closed");
}

void TestTickArithmeticBoundariesRemainCanonical()
{
	const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
	for (const std::uint64_t step : {std::uint64_t{1}, std::uint64_t{2},
		std::uint64_t{7}, maximum / 2 + 1, maximum})
	{
		SimulationTickDispatcher ticks(step, 2);
		for (const std::uint64_t elapsed : {
			std::uint64_t{0}, std::uint64_t{1}, step - 1, step, maximum})
		{
			ticks.advance(elapsed);
			const SimulationTickBoundaryStateCaptureResult captured =
				ticks.captureBoundaryState();
			Check(captured && ticks.validateBoundaryState(captured.state) ==
				TickError::None,
				"captured tick arithmetic remains canonical at saturation boundaries");
		}
	}

	SimulationTickDispatcher dropToMaximum(1, 0);
	Check(dropToMaximum.advance(maximum).dropped == maximum,
		"drop-only scheduler reaches the terminal sequence without iteration");
	const TickState terminal = dropToMaximum.captureBoundaryState().state;
	Check(terminal == TickState{1, 0, maximum, maximum, 0, true} &&
		dropToMaximum.validateBoundaryState(terminal) == TickError::None,
		"saturated sequence, time, remainder, and exhaustion stay canonical");
}

void TestTickRoundTripAndFixedStepContinuation()
{
	SimulationTickDispatcher source(10, 2);
	const SimulationTickDispatchResult first = source.advance(25);
	const SimulationTickDispatchResult second = source.advance(40);
	const SimulationTickBoundaryStateCaptureResult captured =
		source.captureBoundaryState();
	Check(first.scheduled == 2 && first.executed == 2 && first.dropped == 0 &&
		second.scheduled == 4 && second.executed == 2 && second.dropped == 2,
		"fixture exercises executed and deterministically dropped ticks");
	Check(captured && captured.state == TickState{10, 2, 6, 60, 5, false},
		"tick capture includes sequence, time, remainder, and configuration");

	SimulationTickDispatcher restored(10, 2);
	Check(restored.restoreBoundaryState(captured.state) == TickError::None &&
		restored.captureBoundaryState().state == captured.state,
		"compatible tick state round-trips exactly");
	const SimulationTickDispatchResult continued = restored.advance(5);
	Check(continued.scheduled == 1 && continued.executed == 1 &&
		continued.accumulatedMicroseconds == 0 &&
		restored.captureBoundaryState().state ==
			TickState{10, 2, 7, 70, 0, false},
		"restored accumulator continues at the exact next fixed step");

	SimulationTickDispatcher dropOnly(7, 0);
	const SimulationTickDispatchResult dropped = dropOnly.advance(20);
	const TickState droppedState = dropOnly.captureBoundaryState().state;
	SimulationTickDispatcher restoredDropOnly(7, 0);
	Check(dropped.scheduled == 2 && dropped.executed == 0 && dropped.dropped == 2 &&
		droppedState == TickState{7, 0, 2, 14, 6, false} &&
		restoredDropOnly.restoreBoundaryState(droppedState) == TickError::None,
		"zero catch-up configuration and dropped simulation time round-trip");
}

template <typename Mutator>
void ExpectTickRejectedTransactionally(
	SimulationTickDispatcher& dispatcher, const TickState& valid,
	TickError expected, const char* message, Mutator mutate)
{
	const TickState before = dispatcher.captureBoundaryState().state;
	TickState rejected = valid;
	mutate(rejected);
	Check(dispatcher.validateBoundaryState(rejected) == expected &&
		dispatcher.restoreBoundaryState(rejected) == expected &&
		dispatcher.captureBoundaryState().state == before, message);
}

void TestTickMalformedAndIncompatibleStatesAreTransactional()
{
	SimulationTickDispatcher ticks(10, 2);
	Check(ticks.advance(25).executed == 2,
		"transaction fixture establishes non-default live tick state");
	const TickState valid{10, 2, 6, 60, 5, false};

	ExpectTickRejectedTransactionally(ticks, valid,
		TickError::InvalidConfiguration,
		"zero fixed step is malformed and leaves live state unchanged",
		[](TickState& state) { state.stepMicroseconds = 0; });
	ExpectTickRejectedTransactionally(ticks, valid,
		TickError::IncompatibleConfiguration,
		"different fixed step is incompatible and leaves live state unchanged",
		[](TickState& state) {
			state.stepMicroseconds = 11;
			state.simulatedTimeMicroseconds = 66;
		});
	ExpectTickRejectedTransactionally(ticks, valid,
		TickError::IncompatibleConfiguration,
		"different catch-up limit is incompatible and leaves live state unchanged",
		[](TickState& state) { state.maxCatchUpTicks = 3; });
	ExpectTickRejectedTransactionally(ticks, valid,
		TickError::InvalidAccumulator,
		"an out-of-range remainder is malformed and leaves live state unchanged",
		[](TickState& state) { state.accumulatedMicroseconds = 10; });
	ExpectTickRejectedTransactionally(ticks, valid,
		TickError::InvalidSequenceExhaustion,
		"premature tick exhaustion is malformed and leaves live state unchanged",
		[](TickState& state) { state.sequenceExhausted = true; });
	ExpectTickRejectedTransactionally(ticks, valid,
		TickError::InvalidSequenceExhaustion,
		"a maximum sequence must declare exhaustion without changing live state",
		[](TickState& state) {
			state.completedTickSequence =
				std::numeric_limits<std::uint64_t>::max();
			state.simulatedTimeMicroseconds =
				std::numeric_limits<std::uint64_t>::max();
		});
	ExpectTickRejectedTransactionally(ticks, valid,
		TickError::InvalidSimulatedTime,
		"inconsistent simulated time is malformed and leaves live state unchanged",
		[](TickState& state) { state.simulatedTimeMicroseconds = 61; });

	Check(ticks.captureBoundaryState().state == TickState{10, 2, 2, 20, 5, false},
		"every rejected tick restore preserves the original live state");
}

void TestTickExhaustionAndInProgressRefusal()
{
	SimulationTickDispatcher exhausted(10, 2);
	const TickState finalState{
		10, 2, std::numeric_limits<std::uint64_t>::max(),
		std::numeric_limits<std::uint64_t>::max(), 9, true};
	Check(exhausted.restoreBoundaryState(finalState) == TickError::None,
		"canonical saturated tick state restores");
	const SimulationTickDispatchResult terminal = exhausted.advance(1);
	Check(terminal.sequenceExhausted && terminal.executed == 0 &&
		terminal.dropped == 1 && terminal.accumulatedMicroseconds == 0 &&
		exhausted.captureBoundaryState().state ==
			TickState{10, 2, std::numeric_limits<std::uint64_t>::max(),
				std::numeric_limits<std::uint64_t>::max(), 0, true},
		"restored terminal sequence remains exhausted without wrapping");

	SimulationTickDispatcher active(10, 1);
	BoundaryInspectingTickSink sink;
	sink.dispatcher = &active;
	sink.candidate = TickState{10, 1, 0, 0, 0, false};
	Check(active.addSink(sink) == SimulationTickSinkRegistrationError::None &&
		active.advance(10).executed == 1,
		"in-progress fixture dispatches one callback");
	Check(sink.captureError == TickError::OperationInProgress &&
		sink.validationError == TickError::OperationInProgress &&
		sink.restoreError == TickError::OperationInProgress &&
		sink.failedCaptureState == TickState{} &&
		active.captureBoundaryState().state == TickState{10, 1, 1, 10, 0, false},
		"tick capture, validation, and restore fail closed during dispatch");
}

template <typename Mutator>
void ExpectFrameRejectedTransactionally(
	FrameDriver& driver, const FrameState& valid, FrameError expected,
	const char* message, Mutator mutate)
{
	const FrameState before = driver.captureBoundaryState().state;
	FrameState rejected = valid;
	mutate(rejected);
	Check(driver.validateBoundaryState(rejected) == expected &&
		driver.restoreBoundaryState(rejected) == expected &&
		driver.captureBoundaryState().state == before, message);
}

void TestFrameIdentityRoundTripAndMalformedStates()
{
	FrameHarness source;
	Check(source.run().sequence == 1,
		"frame fixture commits its first identity");
	bool failed = false;
	try
	{
		source.driver.runFrame(
			[]() -> FramePlan { throw std::runtime_error("expected failure"); },
			[] {});
	}
	catch (const std::runtime_error&)
	{
		failed = true;
	}
	const FrameDriverBoundaryStateCaptureResult captured =
		source.driver.captureBoundaryState();
	Check(failed && captured && captured.state == FrameState{1, 3, false},
		"frame capture preserves completed count and consumed failed identity");

	FrameHarness restored;
	Check(restored.driver.restoreBoundaryState(captured.state) == FrameError::None &&
		restored.driver.captureBoundaryState().state == captured.state &&
		restored.run().sequence == 3,
		"frame state round-trips to the exact next identity");

	const FrameState valid{4, 7, false};
	ExpectFrameRejectedTransactionally(restored.driver, valid,
		FrameError::InvalidNextFrameSequence,
		"zero next-frame identity is malformed and leaves live state unchanged",
		[](FrameState& state) { state.nextFrameSequence = 0; });
	ExpectFrameRejectedTransactionally(restored.driver, valid,
		FrameError::InvalidSequenceExhaustion,
		"premature frame exhaustion is malformed and leaves live state unchanged",
		[](FrameState& state) { state.sequenceExhausted = true; });
	ExpectFrameRejectedTransactionally(restored.driver, valid,
		FrameError::CompletedFramesExceedAttempts,
		"impossible completed count is malformed and leaves live state unchanged",
		[](FrameState& state) {
			state.completedFrames = 7;
			state.nextFrameSequence = 7;
		});
	Check(restored.driver.captureBoundaryState().state == FrameState{2, 4, false},
		"every rejected frame restore preserves the original live identity");
	restored.time.advanceMicroseconds(7);
	const FrameRunResult afterRejectedRestores = restored.run();
	Check(afterRejectedRestores.sequence == 4 &&
		afterRejectedRestores.runtimeUpdates.delivered == 0 &&
		afterRejectedRestores.simulationTicks.accumulatedMicroseconds == 7,
		"rejected frame restores also preserve the hidden monotonic-time anchor");
}

void TestFrameColdResumeResetsOnlyMonotonicAnchor()
{
	FrameHarness runtime(10, 4);
	RecordingUpdateSink updates;
	Check(runtime.updates.addSink(updates) == RuntimeUpdateSinkRegistrationError::None,
		"cold-resume fixture records elapsed runtime updates");
	runtime.time.setMicroseconds(1000);
	Check(runtime.run().runtimeUpdates.delivered == 1,
		"target process establishes an old monotonic anchor");
	Check(runtime.ticks.advance(7).accumulatedMicroseconds == 7,
		"target tick scheduler establishes state independent of frame identity");
	const TickState ticksBeforeRestore = runtime.ticks.captureBoundaryState().state;

	runtime.time.advanceMicroseconds(100);
	Check(runtime.driver.restoreBoundaryState(FrameState{4, 9, false}) ==
		FrameError::None &&
		runtime.ticks.captureBoundaryState().state == ticksBeforeRestore,
		"frame restore preserves separately restored tick state");
	const FrameRunResult first = runtime.run();
	runtime.time.advanceMicroseconds(3);
	const FrameRunResult second = runtime.run();
	Check(first.sequence == 9 && second.sequence == 10 &&
		updates.updates.size() == 3 &&
		updates.updates[1].elapsedSincePreviousFrameMicroseconds == 0 &&
		updates.updates[2].elapsedSincePreviousFrameMicroseconds == 3,
		"first cold-resume frame anchors locally and only later frames use elapsed time");
	Check(first.simulationTicks.executed == 0 &&
		second.simulationTicks.executed == 1 &&
		runtime.ticks.captureBoundaryState().state ==
			TickState{10, 4, 1, 10, 0, false},
		"cold anchor reset neither discards nor double-counts the restored accumulator");
}

void TestFrameExhaustionAndInProgressRefusal()
{
	FrameHarness terminal;
	const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
	Check(terminal.driver.restoreBoundaryState(FrameState{0, maximum, false}) ==
		FrameError::None,
		"last available frame identity restores before exhaustion");
	Check(terminal.run().sequence == maximum &&
		terminal.driver.captureBoundaryState().state ==
			FrameState{1, maximum, true},
		"last frame identity is consumed exactly once and exhaustion is captured");
	bool overflowRejected = false;
	try
	{
		terminal.run();
	}
	catch (const std::overflow_error&)
	{
		overflowRejected = true;
	}
	Check(overflowRejected &&
		terminal.driver.restoreBoundaryState(FrameState{maximum, maximum, true}) ==
			FrameError::None,
		"restored exhausted frame identity cannot wrap");
	Check(terminal.driver.validateBoundaryState(
		FrameState{maximum, maximum, false}) ==
			FrameError::CompletedFramesExceedAttempts,
		"unexhausted maximum identity cannot claim the still-unattempted frame");

	FrameHarness active;
	FrameError captureError = FrameError::None;
	FrameError validationError = FrameError::None;
	FrameError restoreError = FrameError::None;
	FrameState failedCaptureState{9, 9, true};
	const FrameState candidate{0, 1, false};
	active.driver.runFrame(
		[&] {
			const FrameDriverBoundaryStateCaptureResult captured =
				active.driver.captureBoundaryState();
			captureError = captured.error;
			failedCaptureState = captured.state;
			validationError = active.driver.validateBoundaryState(candidate);
			restoreError = active.driver.restoreBoundaryState(candidate);
			return FramePlan{false, FramePresentMode::Paced};
		}, [] {});
	Check(captureError == FrameError::OperationInProgress &&
		validationError == FrameError::OperationInProgress &&
		restoreError == FrameError::OperationInProgress &&
		failedCaptureState == FrameState{} &&
		active.driver.captureBoundaryState().state == FrameState{1, 2, false},
		"frame capture, validation, and restore fail closed during execution");
}
}

int main()
{
	TestValueContractsAndFailClosedDefaults();
	TestTickArithmeticBoundariesRemainCanonical();
	TestTickRoundTripAndFixedStepContinuation();
	TestTickMalformedAndIncompatibleStatesAreTransactional();
	TestTickExhaustionAndInProgressRefusal();
	TestFrameIdentityRoundTripAndMalformedStates();
	TestFrameColdResumeResetsOnlyMonotonicAnchor();
	TestFrameExhaustionAndInProgressRefusal();
	std::puts("runtime boundary state tests passed");
	return 0;
}
