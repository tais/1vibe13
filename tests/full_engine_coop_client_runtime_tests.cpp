#include <Ja2/FullEngineCoopClientRuntime.h>
#include <Ja2/FullEngineCoopClientScreen.h>
#include <Ja2/DedicatedCoopRuntime.h>

#include <cstdio>
#include <limits>

namespace
{
int Failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL: %s\n", message); \
		++Failures; \
	} \
} while (false)

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
	lifecycle.fail(
		FullEngineCoopClientRuntimeError::TacticalBeforeCampaignReady);
	lifecycle.fail(FullEngineCoopClientRuntimeError::CampaignSyncFailed);

	CHECK(lifecycle.failed(), "terminal failure is observable");
	CHECK(lifecycle.error() ==
		FullEngineCoopClientRuntimeError::TacticalBeforeCampaignReady,
		"the first terminal failure is retained");
	CHECK(!lifecycle.campaignReady(),
		"failure immediately revokes campaign readiness");
	CHECK(!lifecycle.snapshotPublishable(true),
		"failure hides even an otherwise Active snapshot");
	CHECK(!lifecycle.markNetworkOpen(),
		"failed lifecycle cannot open another composition");
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
}

int main()
{
	TestOrderedLifecycleAndSnapshotGate();
	TestFailureIsFirstWinsAndFailClosed();
	TestReconnectSafetyGate();
	TestRetiredLifecycleStopsWithoutFailureOrReconnect();
	TestRetirementConfirmationRequiresReleaseAndSecondPress();
	TestRetirementDrainIgnoresUnrelatedRuntimeMessages();
	if (Failures == 0)
		std::printf("full-engine co-op client runtime tests passed\n");
	return Failures == 0 ? 0 : 1;
}
