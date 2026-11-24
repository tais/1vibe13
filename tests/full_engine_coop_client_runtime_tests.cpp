#include <Ja2/FullEngineCoopClientRuntime.h>
#include <Ja2/FullEngineCoopClientScreen.h>
#include <Ja2/DedicatedCoopRuntime.h>

#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
int Failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL: %s\n", message); \
		++Failures; \
	} \
} while (false)

template <typename Value>
void Append(std::vector<std::uint8_t>& bytes, const Value& value)
{
	const std::size_t offset = bytes.size();
	bytes.resize(offset + sizeof(value));
	std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::vector<std::uint8_t> MinimalPresentationMap()
{
	std::vector<std::uint8_t> bytes;
	const float major = 8.0f;
	const std::uint8_t minor = 31;
	const std::int32_t rows = 1;
	const std::int32_t columns = 1;
	const std::uint32_t flags = 0;
	const std::int32_t tileset = 9;
	const std::uint32_t legacySoldierSize = 0;
	Append(bytes, major);
	Append(bytes, minor);
	Append(bytes, rows);
	Append(bytes, columns);
	Append(bytes, flags);
	Append(bytes, tileset);
	Append(bytes, legacySoldierSize);
	bytes.resize(bytes.size() + 2 + 4 + 2, 0); // height, counts, room
	const std::size_t mapInfo = bytes.size();
	bytes.resize(mapInfo + 32, 0);
	for (std::size_t index = 0; index < 6; ++index)
	{
		const std::int32_t nowhere = -1;
		std::memcpy(bytes.data() + mapInfo + index * 4,
			&nowhere, sizeof(nowhere));
	}
	bytes[mapInfo + 26] = 31;
	return bytes;
}

TacticalWorldSnapshot PresentationSnapshot(
	std::uint64_t worldGeneration, const char* key = "A9.dat")
{
	TacticalSectorSnapshot sector{9, 10, 0, true};
	CHECK(AssignTacticalMapAssetKey(
		sector.mapAssetKey, key, std::strlen(key) + 1),
		"presentation binding fixture map key is valid");
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(worldGeneration, {1, 1}, sector,
		{}, {}, snapshot) == TacticalSnapshotCreateError::None,
		"presentation binding fixture snapshot is valid");
	return snapshot;
}

FullEngineCoopClientPresentationMapPlan PresentationPlan(
	const TacticalWorldSnapshot& snapshot)
{
	FullEngineCoopClientPresentationMapPlan plan;
	CHECK(ParseFullEngineCoopClientPresentationMap(snapshot.sector(),
		snapshot.dimensions(), MinimalPresentationMap(), {}, plan) ==
		FullEngineCoopClientPresentationMapPlanResult::Success,
		"presentation binding fixture plan parses");
	return plan;
}

void TestOwnerInventoryBorrowIsContextBound()
{
	const auto snapshot = PresentationSnapshot(7);
	CoopSession::CoopOwnerInventorySnapshot inventory;
	inventory.actor = {1, 2};
	inventory.sessionEpoch = 41;
	inventory.worldGeneration = 7;
	inventory.baselineId = 3;
	inventory.inventoryRevision = 9;
	FullEngineCoopClientPresentationView view;
	view.snapshot = &snapshot;
	view.state.sessionEpoch = 41;
	view.state.worldGeneration = 7;
	view.assignedActors[0] = inventory.actor;
	view.assignedActorCount = 1;
	view.ownerInventories[0] = &inventory;
	CHECK(view.inventoryFor(inventory.actor) == &inventory,
		"borrowed owner inventory matches the exact current session and world");
	CHECK(view.inventoryFor({1, 3}) == nullptr && view.inventoryFor({2, 2}) == nullptr,
		"another actor or incarnation cannot borrow an assigned private inventory");
	for (unsigned mutation = 0; mutation < 10; ++mutation)
	{
		auto stale = view;
		auto changed = inventory;
		stale.ownerInventories[0] = &changed;
		switch (mutation)
		{
			case 0: stale.resynchronizing = true; break;
			case 1: stale.snapshot = nullptr; break;
			case 2: stale.state.worldGeneration = 8; changed.worldGeneration = 8; break;
			case 3: stale.state.sessionEpoch = 42; break;
			case 4: stale.assignedActorCount = stale.assignedActors.size() + 1; break;
			case 5: stale.assignedActorCount = 0; break;
			case 6: stale.ownerInventories[0] = nullptr; break;
			case 7: changed.actor.incarnation = 3; break;
			case 8: changed.baselineId = 0; break;
			case 9: changed.inventoryRevision = 0; break;
		}
		CHECK(stale.inventoryFor(inventory.actor) == nullptr,
			"retired, resyncing, missing, mismatched and invalid private views fail closed");
	}
}

void TestOrderedLifecycleAndSnapshotGate()
{
	FullEngineCoopClientRuntimeLifecycle lifecycle;
	CHECK(!lifecycle.prepared() && !lifecycle.networkOpen() &&
		!lifecycle.campaignReady() && !lifecycle.failed(),
		"lifecycle starts inert");
	CHECK(!lifecycle.markNetworkOpen(),
		"network cannot open before the private profile lease");
	CHECK(lifecycle.markPrepared(), "early preparation publishes once");
	CHECK(!lifecycle.markPrepared(),
		"early preparation cannot be published twice");
	CHECK(lifecycle.markNetworkOpen(),
		"network opens only after preparation");

	lifecycle.markCampaignReady(true);
	CHECK(lifecycle.campaignReady(),
		"campaign Ready records committed result transmission");
	CHECK(!lifecycle.snapshotPublishable(false),
		"Ready campaign alone cannot publish a tactical snapshot");
	CHECK(lifecycle.snapshotPublishable(true),
		"snapshot publishes for a Ready readable tactical replica, including retained resync state");
	CHECK(!lifecycle.markLeaseClosed(),
		"profile lease cannot close around a live composition");

	lifecycle.markTransportStopped();
	CHECK(!lifecycle.networkOpen() && !lifecycle.campaignReady(),
		"transport detach clears live and campaign-ready state");
	CHECK(lifecycle.prepared(),
		"transport detach deliberately retains the VFS profile lease");
	CHECK(lifecycle.markLeaseClosed(),
		"lease closes only after transport composition is detached");
	CHECK(lifecycle.leaseClosed() && !lifecycle.prepared(),
		"closed lease cannot remain advertised as prepared");
	CHECK(!lifecycle.markPrepared(),
		"one-shot process lifecycle cannot reopen a closed lease");
}

void TestFailureIsFirstWinsAndFailClosed()
{
	FullEngineCoopClientRuntimeLifecycle lifecycle;
	CHECK(lifecycle.markPrepared(), "failure fixture prepares");
	CHECK(lifecycle.markNetworkOpen(), "failure fixture opens network");
	lifecycle.markCampaignReady(true);
	lifecycle.fail(FullEngineCoopClientRuntimeError::None,
		"not-a-failure", "must be ignored", "must not capture");
	CHECK(!lifecycle.failed() && lifecycle.campaignReady() &&
		!lifecycle.failureDiagnostic().available(),
		"a None result cannot poison readiness or first-failure diagnostics");
	lifecycle.fail(
		FullEngineCoopClientRuntimeError::TacticalBeforeCampaignReady,
		"tactical.baseline.apply", "campaign checkpoint is not ready",
		"baseline rejected");
	lifecycle.fail(FullEngineCoopClientRuntimeError::CampaignSyncFailed,
		"campaign.sync", "wire failure", "should not replace first failure");

	CHECK(lifecycle.failed(), "terminal failure is observable");
	CHECK(lifecycle.error() ==
		FullEngineCoopClientRuntimeError::TacticalBeforeCampaignReady,
		"the first terminal failure is retained");
	const CoopRuntimeFailureDiagnostic& diagnostic =
		lifecycle.failureDiagnostic();
	CHECK(diagnostic.available() &&
		std::string_view(diagnostic.stage()) == "tactical.baseline.apply" &&
		std::string_view(diagnostic.reason()) ==
			"campaign checkpoint is not ready" &&
		std::string_view(diagnostic.detail()) == "baseline rejected",
		"the precise first-failure stage and lower-level cause are retained");
	CHECK(!lifecycle.campaignReady(),
		"failure immediately revokes campaign readiness");
	CHECK(!lifecycle.snapshotPublishable(true),
		"failure hides even an otherwise Active snapshot");
	CHECK(!lifecycle.markNetworkOpen(),
		"failed lifecycle cannot open another composition");
}

void TestFailureDiagnosticIsBoundedAndSingleLine()
{
	CoopRuntimeFailureDiagnostic diagnostic;
	diagnostic.capture("boot\x1b\f\x7f\x9b" "strap\nhandshake", "connection\tlost",
		"transport\rclosed");
	diagnostic.capture("later", "secret-like later state", "later detail");
	CHECK(diagnostic.available() &&
		std::string_view(diagnostic.stage()) == "boot    strap handshake" &&
		std::string_view(diagnostic.reason()) == "connection lost" &&
		std::string_view(diagnostic.detail()) == "transport closed",
		"diagnostics are first-wins and allow only printable ASCII on one log line");

	CoopRuntimeFailureDiagnostic unavailable;
	CHECK(!unavailable.available() &&
		std::string_view(unavailable.stage()) == "<unavailable>" &&
		std::string_view(unavailable.reason()) == "<unavailable>" &&
		std::string_view(unavailable.detail()) == "<unavailable>",
		"an unavailable diagnostic never exposes empty log fields");
	unavailable.capture(nullptr, "", nullptr);
	CHECK(unavailable.available() &&
		std::string_view(unavailable.stage()) == "unknown stage" &&
		std::string_view(unavailable.reason()) == "unknown reason" &&
		std::string_view(unavailable.detail()) == "none",
		"null and empty diagnostic fields use explicit safe fallbacks");

	const std::string overlong(160, 'x');
	CoopRuntimeFailureDiagnostic bounded;
	bounded.capture(overlong.c_str(), overlong.c_str(), overlong.c_str());
	CHECK(std::strlen(bounded.stage()) == 63 &&
		std::strlen(bounded.reason()) == 95 &&
		std::strlen(bounded.detail()) == 95,
		"diagnostic fields truncate at their documented NUL-terminated bounds");
}

void TestTacticalReadyMarkerIsOneShotPerWorld()
{
	FullEngineCoopClientTacticalReadyMarker marker;
	CHECK(!marker.observe(false, true, true, 1, 10) &&
		!marker.observe(true, false, true, 1, 10) &&
		!marker.observe(true, true, false, 1, 10) &&
		!marker.observe(true, true, true, 0, 10) &&
		!marker.observe(true, true, true, 1, 0),
		"readiness marker waits for campaign, active baseline, replica, and assignment");
	CHECK(marker.observe(true, true, true, 1, 10),
		"first committed assigned baseline reports the world");
	CHECK(!marker.observe(true, true, true, 1, 10) &&
		!marker.observe(false, false, false, 0, 10) &&
		!marker.observe(true, true, true, 1, 10),
		"same-world resync and reconnect cannot duplicate the marker");
	CHECK(marker.observe(true, true, true, 2, 11) &&
		!marker.observe(true, true, true, 2, 11),
		"a new authoritative world generation reports exactly once");
}

void TestReconnectSafetyGate()
{
	FullEngineCoopClientRuntimeLifecycle lifecycle;
	CHECK(lifecycle.markPrepared(), "reconnect fixture prepares");
	CHECK(lifecycle.markNetworkOpen(), "reconnect fixture opens");
	CHECK(lifecycle.reconnectAllowed(true, true, 0, 8),
		"same-epoch retained credentials permit reconnect");
	CHECK(lifecycle.reconnectAllowed(true, true, 8, 8) &&
		lifecycle.reconnectAllowed(true, true,
			(std::numeric_limits<unsigned>::max)(), 8),
		"durable same-epoch credentials outlive the startup retry budget");
	CHECK(!lifecycle.reconnectAllowed(false, true, 0, 8),
		"changed epoch cannot reuse reconnect credentials");
	CHECK(lifecycle.reconnectAllowed(false, false, 0, 8),
		"credential-less startup may use its bounded retry budget");
	CHECK(!lifecycle.reconnectAllowed(true, false, 8, 8),
		"credential-less startup fails closed at retry budget exhaustion");
	lifecycle.markCampaignReady(true);
	CHECK(!lifecycle.reconnectAllowed(true, true, 0, 8),
		"an already Ready live session is not a reconnect candidate");
}

void TestRetiredLifecycleStopsWithoutFailureOrReconnect()
{
	FullEngineCoopClientRuntimeLifecycle lifecycle;
	CHECK(lifecycle.markPrepared() && lifecycle.markNetworkOpen(),
		"retired lifecycle fixture opens normally");
	lifecycle.markCampaignReady(true);
	CHECK(lifecycle.markRetired() && lifecycle.retired() &&
		!lifecycle.failed() && !lifecycle.campaignReady(),
		"exact retirement is a clean terminal result, not a failure");
	CHECK(!lifecycle.reconnectAllowed(true, false, 0, 8) &&
		!lifecycle.reconnectAllowed(true, true, 0, 8),
		"retired runtime never reconnects with or without a credential");
	lifecycle.markTransportStopped();
	CHECK(lifecycle.retired() && !lifecycle.networkOpen() &&
		!lifecycle.markNetworkOpen(),
		"transport teardown preserves terminal Retired/Stopped state");

	FullEngineCoopClientRuntimeLifecycle restarted;
	CHECK(restarted.markPrepared() && restarted.markDurablyRetired() &&
		restarted.retired() && !restarted.networkOpen() &&
		!restarted.markNetworkOpen() && !restarted.failed(),
		"durable retired marker terminates a restart before network open");
}

void TestDisconnectedReadyClientReportsReconnectUntilReadmitted()
{
	using Phase = FullEngineCoopClientConnectionPhase;
	using State = CoopSession::FullEngineCoopClientState;
	FullEngineCoopClientRuntimeLifecycle lifecycle;
	CHECK(lifecycle.connectionPhase(State::Disconnected) == Phase::OpeningSession,
		"a client with no live composition reports session opening");
	CHECK(lifecycle.markPrepared() && lifecycle.markNetworkOpen(),
		"connection status fixture owns the live composition");
	CHECK(lifecycle.connectionPhase(State::Admission) == Phase::OpeningSession,
		"initial admission does not claim campaign checkpoint synchronization");
	CHECK(lifecycle.connectionPhase(State::AwaitingBaseline) ==
		Phase::SynchronizingCampaign,
		"an admitted client reports its pending campaign checkpoint");
	lifecycle.markCampaignReady(true);
	CHECK(lifecycle.connectionPhase(State::AwaitingBaseline) ==
		Phase::AwaitingBaseline &&
		lifecycle.connectionPhase(State::Active) == Phase::Ready,
		"campaign completion still waits for the first committed tactical baseline");

	// The server disappears after both campaign and tactical readiness. The
	// runtime retains composition ownership while it retries the same seat.
	lifecycle.markCampaignReady(false);
	CHECK(lifecycle.markReconnecting() && lifecycle.networkOpen() &&
		!lifecycle.snapshotPublishable(false) &&
		lifecycle.connectionPhase(State::Disconnected) == Phase::Reconnecting,
		"server loss reports reconnecting instead of endless campaign synchronization");
	for (unsigned attempt = 0; attempt < 16; ++attempt)
	{
		CHECK(lifecycle.reconnectAllowed(true, true, attempt, 8),
			"connection feedback never caps an admitted same-epoch seat's retries");
		CHECK(!lifecycle.markReconnecting() &&
			lifecycle.connectionPhase(State::Connecting) == Phase::Reconnecting &&
			lifecycle.connectionPhase(State::Hello) == Phase::Reconnecting &&
			lifecycle.connectionPhase(State::Admission) == Phase::Reconnecting,
			"repeated connection attempts retain one interruption transition until readmission");
		lifecycle.markCampaignReady(false);
	}
	CHECK(lifecycle.connectionPhase(State::AwaitingBaseline) ==
		Phase::SynchronizingCampaign && lifecycle.reconnecting(),
		"readmission reports actual campaign synchronization before recovery completes");
	lifecycle.markCampaignReady(true);
	CHECK(!lifecycle.reconnecting() &&
		lifecycle.connectionPhase(State::AwaitingBaseline) == Phase::AwaitingBaseline &&
		lifecycle.connectionPhase(State::Active) == Phase::Ready,
		"committed campaign recovery clears interruption and waits for tactical readiness");
	CHECK(lifecycle.markReconnecting(),
		"a later independent server interruption emits a new transition");
	lifecycle.markTransportStopped();
	CHECK(!lifecycle.reconnecting() && !lifecycle.markReconnecting() &&
		lifecycle.connectionPhase(State::Disconnected) == Phase::OpeningSession,
		"composition teardown clears reconnect feedback and cannot restart it");

	FullEngineCoopClientRuntimeLifecycle failed;
	CHECK(failed.markPrepared() && failed.markNetworkOpen() &&
		failed.markReconnecting(), "failure feedback fixture starts reconnecting");
	failed.fail(FullEngineCoopClientRuntimeError::LiveServerDescriptorMismatch);
	CHECK(failed.connectionPhase(State::Disconnected) == Phase::Failed,
		"a changed server session reports terminal failure instead of further retries");
	FullEngineCoopClientRuntimeLifecycle retired;
	CHECK(retired.markPrepared() && retired.markDurablyRetired() &&
		retired.connectionPhase(State::Disconnected) == Phase::Retired,
		"a permanently retired directory never presents as reconnecting");
}

void TestRetirementConfirmationRequiresReleaseAndSecondPress()
{
	FullEngineCoopClientRetirementConfirmation confirmation;
	CHECK(!confirmation.pressLeave(10) && confirmation.pending() &&
		!confirmation.armed(),
		"first leave down waits for a physical key release");
	CHECK(!confirmation.pressLeave(10) && !confirmation.armed(),
		"queued or repeated leave downs cannot confirm retirement");
	confirmation.releaseLeave(11);
	CHECK(confirmation.armed(),
		"leave key release arms the explicit second step");
	CHECK(confirmation.pressLeave(12) && !confirmation.pending(),
		"only a later leave down confirms once and resets the ledger");

	CHECK(!confirmation.pressLeave(20),
		"confirmation can be armed again after an explicit reset");
	confirmation.releaseLeave(21);
	confirmation.cancel();
	CHECK(!confirmation.pending() && !confirmation.pressLeave(22),
		"Esc, another command, or a state change cancels back to step one");
	confirmation.releaseLeave(23);
	confirmation.advance(22 +
		FullEngineCoopClientRetirementConfirmation::FrameBudget + 1);
	CHECK(!confirmation.pending(),
		"bounded frame expiry cancels an abandoned confirmation prompt");
}

void TestRetirementDrainIgnoresUnrelatedRuntimeMessages()
{
	DedicatedCoopRetirementLocalDrainState local;
	const std::size_t continuouslyPublishedUnrelatedMessages = 37;
	CHECK(local.drained() && continuouslyPublishedUnrelatedMessages != 0,
		"unrelated per-frame RuntimeMessageBus traffic cannot starve retirement");
	local.trackedCommands = 1;
	CHECK(!local.drained(),
		"an already-authorized tracked tactical command still delays retirement");
	local.trackedCommands = 0;
	local.pendingImmediateReceipts = 1;
	CHECK(!local.drained(),
		"a local terminal-receipt obligation still delays retirement");
}

void TestPresentationMapPlanBindingIsExactAndRevocable()
{
	const TacticalWorldSnapshot snapshot = PresentationSnapshot(7);
	FullEngineCoopClientPresentationMapPlanBinding binding;
	CHECK(binding.beginAttempt(101, 7, snapshot) && binding.attempted() &&
		!binding.verified(),
		"map plan binding records one exact pending identity");
	auto plan = PresentationPlan(snapshot);
	CHECK(binding.commit(std::move(plan)) && binding.verified() &&
		binding.result() ==
			FullEngineCoopClientPresentationMapPlanResult::Success,
		"successful full parse transfers immutable plan ownership");
	const auto* borrowed = binding.borrow(101, 7, snapshot, false);
	CHECK(borrowed != nullptr && borrowed->bytes().size() == 65 &&
		borrowed->sector().mapAssetKey == snapshot.sector().mapAssetKey,
		"matching presentation identity borrows the retained exact bytes");
	CHECK(binding.borrow(102, 7, snapshot, false) == nullptr &&
		binding.borrow(101, 8, snapshot, false) == nullptr &&
		binding.borrow(101, 7, snapshot, true) == nullptr,
		"session, generation, and resynchronization mismatches hide the plan");

	const TacticalWorldSnapshot replacement = PresentationSnapshot(8, "B9.dat");
	CHECK(binding.beginAttempt(101, 8, replacement) && !binding.verified() &&
		binding.borrow(101, 7, snapshot, false) == nullptr,
		"identity replacement releases the previous owning plan before IO");
	binding.recordFailure(
		FullEngineCoopClientPresentationMapPlanResult::TruncatedData);
	CHECK(!binding.verified() &&
		binding.result() ==
			FullEngineCoopClientPresentationMapPlanResult::TruncatedData &&
		binding.attemptedIdentityMatches(101, 8, replacement) &&
		binding.borrow(101, 8, replacement, false) == nullptr,
		"failed full parse retains only retry-suppressing identity and result");
	binding.clear();
	CHECK(!binding.attempted() && !binding.verified() &&
		binding.borrow(101, 8, replacement, false) == nullptr,
		"disconnect or resync clear revokes all plan state");
}
}

int main()
{
	TestOwnerInventoryBorrowIsContextBound();
	TestOrderedLifecycleAndSnapshotGate();
	TestFailureIsFirstWinsAndFailClosed();
	TestFailureDiagnosticIsBoundedAndSingleLine();
	TestTacticalReadyMarkerIsOneShotPerWorld();
	TestReconnectSafetyGate();
	TestRetiredLifecycleStopsWithoutFailureOrReconnect();
	TestDisconnectedReadyClientReportsReconnectUntilReadmitted();
	TestRetirementConfirmationRequiresReleaseAndSecondPress();
	TestRetirementDrainIgnoresUnrelatedRuntimeMessages();
	TestPresentationMapPlanBindingIsExactAndRevocable();
	if (Failures == 0)
		std::printf("full-engine co-op client runtime tests passed\n");
	return Failures == 0 ? 0 : 1;
}
