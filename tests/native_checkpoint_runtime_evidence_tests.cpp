#include "DedicatedCheckpointRuntimeEvidence.h"
#include "DedicatedCoopRuntime.h"
#include "GameContext.h"
#include "random.h"
#include <Engine/Core/SimulationRandom.h>

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>
#include <vector>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : ""); std::exit(1); }

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)

struct CallbackProbe final : RuntimeMessageSink, SimulationTickSink
{
	DedicatedCoopRuntime& runtime;
	GameContext& context;
	std::vector<RuntimeMessage> received;
	bool tickObserved = false;
	CallbackProbe(DedicatedCoopRuntime& r, GameContext& c) : runtime(r), context(c) {}
	void receiveMessage(const RuntimeMessage& message) override
	{
		received.push_back(message);
		const auto empty = runtime.captureCheckpointRuntimeEvidence(context);
		CHECK(empty.packages.observed && empty.packages.messages.queuedMessages == 0 &&
			empty.packages.messages.dispatchInProgress,
			"popped final message is still an active callback, not an empty settled package");
		if (received.size() == 1)
		{
			CHECK(context.runtimeMessages().publish({"fixture.deferred", "fixture.owner", {9, 8, 7}}),
				"real callback queues deferred payload");
			for (unsigned i = 0; i < 2; ++i)
			{
				const auto pending = runtime.captureCheckpointRuntimeEvidence(context);
				CHECK(pending.packages.messages.queuedMessages == 1 && pending.packages.messages.dispatchInProgress,
					"repeated callback capture leaves deferred queue intact");
			}
		}
	}
	void simulate(const SimulationTickContext&) override
	{
		const auto evidence = runtime.captureCheckpointRuntimeEvidence(context);
		tickObserved = evidence.frame.tick.error == SimulationTickBoundaryStateError::OperationInProgress;
	}
};

void TestRealRuntimeBoundary()
{
	static_assert(std::is_trivially_copyable<DedicatedCheckpointRuntimeEvidence>::value,
		"diagnostic owns only bounded values");
	static_assert(noexcept(std::declval<const DedicatedCoopRuntime&>().captureCheckpointRuntimeEvidence(
		std::declval<const GameContext&>())), "read-only diagnostic is non-throwing");
	CHECK(InstallGameSimulationRandom(711) == GameSimulationRandomInstallError::None,
		"real canonical native RNG is installed before observation");
	auto* random = GetGameSimulationRandomSource();
	CHECK(random != nullptr, "canonical RNG exists");
	if (!random) return;
	const auto randomBefore = random->checkpoint();
	const auto epochBefore = random->consumptionEpoch();
	auto& context = GetGameContext();
	DedicatedCoopRuntime runtime;
	const auto initial = runtime.captureCheckpointRuntimeEvidence(context);
	CHECK(initial.frame.observed && initial.frame.frame && initial.frame.tick &&
		initial.frame.frame.state.completedFrames == 0 && initial.frame.frame.state.nextFrameSequence == 1,
		"initial frame identity stays distinguishable from a committed frame");
	CHECK(initial.packages.observed && !initial.packages.retainedPackageWorkObserved &&
		initial.localCommands.nativeObserved && !initial.localCommands.hostObserved &&
		!initial.tactical.observed && !initial.transport.observed &&
		!initial.transport.socketWriteBytesObserved && !initial.campaign.syncObserved &&
		!initial.campaign.observationDeliveriesObserved,
		"absent composition and unobserved responsibilities never become synthetic drained evidence");
	CallbackProbe probe(runtime, context);
	CHECK(context.runtimeMessages().addSink(probe) == RuntimeMessageSinkRegistrationError::None &&
		context.runtime().simulationTicks().addSink(probe) == SimulationTickSinkRegistrationError::None,
		"real message and tick callbacks registered");
	CHECK(context.runtimeMessages().publish({"fixture.original", "fixture.owner", {1, 2}}),
		"real package request queued");
	for (unsigned i = 0; i < 2; ++i)
		CHECK(runtime.captureCheckpointRuntimeEvidence(context).packages.messages.queuedMessages == 1 &&
			probe.received.empty(), "capture neither dispatches nor consumes a queued payload");
	const auto first = context.runtimeMessages().dispatchPending();
	CHECK(first.messages == 1 && first.queuedForNextDispatch == 1 && probe.received.size() == 1 &&
		probe.received[0].sequence == 1 && probe.received[0].payload == std::vector<std::uint8_t>({1, 2}),
		"ordinary dispatch preserves original sequence and payload");
	const auto second = context.runtimeMessages().dispatchPending();
	CHECK(second.messages == 1 && second.queuedForNextDispatch == 0 && probe.received.size() == 2 &&
		probe.received[1].sequence == 2 && probe.received[1].topic == "fixture.deferred" &&
		probe.received[1].payload == std::vector<std::uint8_t>({9, 8, 7}),
		"capture preserves deferred sequence, order and payload");
	(void)context.runtime().simulationTicks().advance(20000);
	CHECK(probe.tickObserved, "capture reports a real executing tick");
	bool prepareObserved = false, completeObserved = false;
	context.frameDriver().runFrame([&] {
		prepareObserved = runtime.captureCheckpointRuntimeEvidence(context).frame.frame.error ==
			FrameDriverBoundaryStateError::OperationInProgress;
		return FramePlan{};
	}, [&] {
		completeObserved = runtime.captureCheckpointRuntimeEvidence(context).frame.frame.error ==
			FrameDriverBoundaryStateError::OperationInProgress;
	});
	const auto committed = runtime.captureCheckpointRuntimeEvidence(context);
	CHECK(prepareObserved && completeObserved && committed.frame.frame &&
		committed.frame.frame.state.completedFrames == 1 && committed.frame.frame.state.nextFrameSequence == 2,
		"real frame callbacks and committed identity remain separate raw observations");
	try { context.frameDriver().runFrame([]() -> FramePlan { throw std::runtime_error("frame fixture"); }, [] {}); }
	catch (const std::runtime_error&) {}
	const auto failed = runtime.captureCheckpointRuntimeEvidence(context);
	CHECK(failed.frame.frame && failed.frame.frame.state.completedFrames == 1 &&
		failed.frame.frame.state.nextFrameSequence == 3,
		"failed attempted frame is not mislabeled as a new completed frame");
	const auto again = runtime.captureCheckpointRuntimeEvidence(context);
	CHECK(again.frame.frame.state == failed.frame.frame.state && again.frame.tick.state == failed.frame.tick.state &&
		again.localCommands.commandInbox == initial.localCommands.commandInbox &&
		again.localCommands.pendingHostReceipts == initial.localCommands.pendingHostReceipts &&
		again.localCommands.trackedCommands == initial.localCommands.trackedCommands &&
		random->checkpoint() == randomBefore && random->consumptionEpoch() == epochBefore,
		"repeated aggregate capture changes no boundary, command count or canonical RNG evidence");
	CHECK(context.runtimeMessages().removeSink(probe) == RuntimeMessageSinkRegistrationError::None &&
		context.runtime().simulationTicks().removeSink(probe) == SimulationTickSinkRegistrationError::None,
		"fixture releases actual callback registrations");
}

void TestRealCampaignAuthorities()
{
	using namespace CoopSession;
	PeerIdentity identity{}; identity[0] = 1;
	CoopCampaignStatusLedger status;
	CoopCampaignStatus clock; clock.phase = CoopCampaignPhase::Strategic;
	CHECK(status.beginSession(1) && status.observe(clock, &identity, 1), "real campaign status starts");
	CoopCampaignTimeAuthority time;
	CoopCampaignActionAuthority action;
	CoopCampaignHireAuthority hire;
	CoopCampaignTimeAuthority::Peer tp{identity, TransportPeer{11}};
	CoopCampaignActionAuthority::Peer ap{identity, TransportPeer{11}};
	CoopCampaignHireAuthority::Peer hp{identity, TransportPeer{11}};
	CHECK(time.reconcile(&tp, 1) && action.reconcile(&ap, 1) && hire.reconcile(&hp, 1),
		"actual authorities bind exact authenticated transport");
	unsigned nativeCalls = 0;
	CoopCampaignTimeRequest tr{1, status.value().timeControlRevision, 1, CoopCampaignTimeAction::FiveMinutes};
	CHECK(time.submit(tr, tp, true, true, status, [&](auto) { ++nativeCalls; return true; }),
		"actual applied time request retains a result");
	CoopCampaignGroups groups; groups.revision = 1;
	CoopCampaignActionRequest ar; ar.sessionEpoch = 1; ar.controlRevision = status.value().timeControlRevision;
	ar.groupsRevision = 1; ar.requestId = 2; ar.group = {3, 4}; ar.destinationX = 10; ar.destinationY = 1;
	CHECK(action.submit(ar, ap, false, true, true, status, groups, [&](auto) {
		++nativeCalls; return CoopCampaignActionNativeResult{};
	}), "actual not-ready action still retains its terminal result");
	CoopCampaignEconomy economy; economy.revision = 2;
	CoopCampaignAimQuotes quotes; quotes.revision = 3;
	CoopCampaignHireRequest hr; hr.sessionEpoch = 1; hr.controlRevision = status.value().timeControlRevision;
	hr.economyRevision = 2; hr.quoteRevision = 3; hr.requestId = 3; hr.profile = 0; hr.days = 7; hr.buyGear = true;
	CHECK(hire.submit(hr, hp, false, true, true, status, economy, quotes, [&](auto) {
		++nativeCalls; return CoopCampaignHireNativeResult{};
	}), "actual not-ready hire retains its terminal result");
	const auto revision = status.value().timeControlRevision;
	for (unsigned i = 0; i < 2; ++i)
	{
		DedicatedCheckpointRuntimeEvidence::Campaign captured;
		CaptureDedicatedCampaignResultEvidence(time, action, hire, captured);
		CHECK(captured.resultDeliveriesObserved && captured.pendingTimeResults == 1 &&
			captured.pendingActionResults == 1 && captured.pendingHireResults == 1 &&
			nativeCalls == 1 && status.value().timeControlRevision == revision &&
			SameCoopCampaignTimeRequest(time.deliveries()[0].result.request, tr) &&
			SameCoopCampaignActionRequest(action.deliveries()[0].result.request, ar) &&
			SameCoopCampaignHireRequest(hire.deliveries()[0].result.request, hr),
			"production scan preserves actual pending terminal outcomes and control revision");
	}
	CHECK(time.reconcile(&tp, 1) && action.reconcile(&ap, 1) && hire.reconcile(&hp, 1),
		"same-transport resync retains deliveries");
	DedicatedCheckpointRuntimeEvidence::Campaign captured;
	CaptureDedicatedCampaignResultEvidence(time, action, hire, captured);
	CHECK(captured.pendingTimeResults == 1 && captured.pendingActionResults == 1 && captured.pendingHireResults == 1,
		"resync evidence retains all actual obligations");
	time.delivered(0); action.delivered(0); hire.delivered(0);
	CaptureDedicatedCampaignResultEvidence(time, action, hire, captured);
	CHECK(captured.pendingTimeResults == 0 && captured.pendingActionResults == 0 && captured.pendingHireResults == 0 &&
		time.deliveries()[0].result.request.requestId == 1 && action.deliveries()[0].result.request.requestId == 2 &&
		hire.deliveries()[0].result.request.requestId == 3,
		"explicit delivery clears counts while immutable result history is excluded");
}
}

int main()
{
	TestRealRuntimeBoundary();
	TestRealCampaignAuthorities();
	if (!failures) std::puts("native checkpoint runtime evidence: PASS");
	return failures ? 1 : 0;
}
