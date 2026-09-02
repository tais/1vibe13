#include "DedicatedCoopRuntime.h"

#include "DedicatedContentManifest.h"
#include "DedicatedCampaignSaveBridge.h"
#include "DedicatedCoopMissionBootstrap.h"
#include "DedicatedCoopTacticalHost.h"
#include "CampaignPackage.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "SaveLoadGame.h"
#include "TacticalCommandHost.h"
#include "TacticalWorldAdapter.h"
#include "TacticalWorldObserverHost.h"
#include "gameloop.h"

#include "Auto Resolve.h"
#include "Boxing.h"
#include "Bullets.h"
#include "Dialogue Control.h"
#include "Explosion Control.h"
#include "Game Clock.h"
#include "Handle UI.h"
#include "Meanwhile.h"
#include "Overhead.h"
#include "PreBattle Interface.h"
#include "Queen Command.h"
#include "Reinforcement.h"
#include "Scheduling.h"
#include "Timer Control.h"
#include "World Items.h"
#include "gamescreen.h"
#include "random.h"
#include "screenids.h"
#include "strategicmap.h"

#include "FullEngineCoopAdmissionListener.h"
#include "CoopActorAssignmentPolicy.h"
#include "FullEngineCoopCampaignSyncServer.h"
#include "FullEngineCoopTacticalServer.h"
#include "OsAdmissionTokenSource.h"

#include <Engine/Core/DedicatedCheckpointEligibility.h>
#include <Engine/Core/RuntimeFingerprint.h>

#include <vfs/Core/vfs.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include "gameplay_lua_policy.h"

extern BOOLEAN gfDedicatedServerProcessFailed;
extern BOOLEAN gfProgramIsRunning;
extern BOOLEAN gfWaitingForTriggerTimer;
extern UINT32 guiArrived;
extern UINT32 guiMilitiaArrived;

namespace
{
using Clock = std::chrono::steady_clock;

constexpr auto IneligibleRetryDelay = std::chrono::seconds(5);
constexpr auto StarterPeerGatherGrace = std::chrono::seconds(10);
constexpr auto StarterActorArrivalTimeout = std::chrono::minutes(2);

static_assert(CoopSession::MaximumAuthorityPeers ==
	DedicatedCoopStarterRosterSize,
	"starter launch capacity must match the deterministic roster");

enum class StarterMissionState : std::uint8_t
{
	Unprepared,
	WaitingForCampaignReadyPeer,
	WaitingForEstablishedCampaignReadyPeer,
	WaitingForControllableActor,
	Playable,
	ReturningToStrategic,
	WaitingForStrategicCheckpoint,
	StrategicIdle
};

bool HasTemporarySchedules() noexcept
{
	for (const SCHEDULENODE* schedule = gpScheduleList;
		schedule != nullptr; schedule = schedule->next)
	{
		if ((schedule->usFlags & SCHEDULE_FLAGS_TEMPORARY) != 0) return true;
	}
	return false;
}

DedicatedCoopPostCombatReturnEvidence CapturePostCombatReturnEvidence(
	bool missionPlayable,
	bool hostileWorldArmed) noexcept
{
	DedicatedCoopPostCombatReturnEvidence evidence;
	evidence.missionPlayable = missionPlayable;
	evidence.hostileWorldArmed = hostileWorldArmed;
	evidence.worldLoaded = IsJa2TacticalWorldLoaded();
	evidence.gameScreen = GetCurrentScreen() == GAME_SCREEN;
	evidence.validWorldSector = gWorldSectorX >= 1 && gWorldSectorX <= 16 &&
		gWorldSectorY >= 1 && gWorldSectorY <= 16 && gbWorldSectorZ >= 0 &&
		gbWorldSectorZ <= 3;
	evidence.lastBattleWon = gTacticalStatus.fLastBattleWon != FALSE;
	evidence.enemyInSector = gTacticalStatus.fEnemyInSector != FALSE;
	evidence.enemiesRemaining = NumEnemyInSector() != 0;
	evidence.combatActive = IsJa2TacticalCombatActive();
	evidence.tacticalActionsPending =
		GetJa2PendingTacticalCombatActions() != 0;
	evidence.interruptPending =
		CaptureJa2TacticalInterruptState().pending != 0;
	evidence.bulletsPending = guiNumBullets != 0;
	evidence.explosionsPending = gfExplosionQueueActive != FALSE ||
		gubElementsOnExplosionQueue != 0;
	evidence.dialogueActive = DialogueActive() != FALSE;
	evidence.dialogueQueued = !DialogueQueueIsEmpty();
	evidence.triggerTimerPending = gfWaitingForTriggerTimer != FALSE;
	evidence.autoResolveActive = IsAutoResolveActive() != FALSE;
	evidence.autoResolvePending = gfAutomaticallyStartAutoResolve != FALSE;
	evidence.meanwhileActive = gfInMeanwhile != FALSE;
	evidence.meanwhilePending = gfMeanwhileTryingToStart != FALSE;
	evidence.tacticalTraversal = gfTacticalTraversal != FALSE;
	evidence.autoBandageActive =
		gTacticalStatus.fAutoBandageMode != FALSE ||
		gTacticalStatus.fAutoBandagePending != FALSE;
	evidence.boxingActive = gTacticalStatus.bBoxingState != NOT_BOXING;
	evidence.saveLoadActive =
		(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) != 0;
	evidence.uiTransitionPending = guiPendingOverrideEvent != I_DO_NOTHING ||
		gfEnteringMapScreen != FALSE;
	evidence.customTimerPending = gpCustomizableTimerCallback != nullptr;
	evidence.temporarySchedulePending = HasTemporarySchedules();
	return evidence;
}

DedicatedCheckpointEligibilitySnapshot CollectCheckpointEligibility(
	GameContext& context,
	bool networkDrained,
	bool tacticalCommandsDrained,
	bool tacticalNetworkDrained) noexcept
{
	DedicatedCheckpointEligibilitySnapshot snapshot;
	snapshot.resumeMode = DedicatedCheckpointResumeMode::Cold;
	const TacticalCommandInboxSummary commandSummary =
		GetJa2TacticalCommandService().summary();
	const Ja2TacticalCommandHostDiagnostics commandDiagnostics =
		GetJa2TacticalCommandHostDiagnostics();
	snapshot.commandQueue = tacticalCommandsDrained &&
		commandSummary.pending == 0 &&
		commandDiagnostics.pendingReceipts == 0 &&
		commandDiagnostics.pendingDeferredCancellations == 0 &&
		commandDiagnostics.trackedCommands == 0
		? DedicatedCheckpointDrainState::Drained
		: DedicatedCheckpointDrainState::Pending;
	snapshot.networkQueue = networkDrained && tacticalNetworkDrained
		? DedicatedCheckpointDrainState::Drained
		: DedicatedCheckpointDrainState::Pending;
	snapshot.packageQueue = context.runtimeMessages().queued() == 0
		? DedicatedCheckpointDrainState::Drained
		: DedicatedCheckpointDrainState::Pending;
	snapshot.dialogueQueue =
		DialogueQueueIsEmpty() && !gfWaitingForTriggerTimer
		? DedicatedCheckpointDrainState::Drained
		: DedicatedCheckpointDrainState::Pending;

	const FrameDriverBoundaryStateCaptureResult frame =
		context.frameDriver().captureBoundaryState();
	snapshot.simulation = frame
		? DedicatedCheckpointRunState::Paused
		: DedicatedCheckpointRunState::Running;
	snapshot.frameBoundary = frame
		? DedicatedCheckpointFrameBoundary::Committed
		: DedicatedCheckpointFrameBoundary::InProgress;

	snapshot.tacticalWorldLoaded = IsJa2TacticalWorldLoaded();
	// The vector/counter retain allocation capacity after sector unload.  Only
	// fExists entries are live serializer state; counting backing storage would
	// permanently disable checkpoints after the first tactical sector.
	snapshot.worldItemsLoaded = GetNumUsedWorldItems() != 0;
	snapshot.combatActive = IsJa2TacticalCombatActive();
	snapshot.autoResolveActive = IsAutoResolveActive() != FALSE;
	snapshot.meanwhileActive = gfInMeanwhile != FALSE ||
		gfMeanwhileTryingToStart != FALSE;
	snapshot.projectileActive = guiNumBullets != 0;
	snapshot.explosionActive = gfExplosionQueueActive != FALSE;
	snapshot.dialogueActive = DialogueActive() != FALSE;
	snapshot.realtimeAiActive = IsJa2TacticalWorldLoaded() &&
		!IsJa2TacticalTurnBased();
	snapshot.customizableCallbackPending =
		gpCustomizableTimerCallback != nullptr;
	snapshot.reinforcementTurnCounter = guiTurnCnt;
	snapshot.enemyReinforcementTurn = guiReinforceTurn;
	snapshot.enemyReinforcementsArrived = guiArrived;
	snapshot.militiaReinforcementTurn = guiMilitiaReinforceTurn;
	snapshot.militiaReinforcementsArrived = guiMilitiaArrived;
	snapshot.temporarySchedulesPresent = HasTemporarySchedules();
	snapshot.miniEventsEnabled =
		gGameExternalOptions.fMiniEventsEnabled != FALSE;
	snapshot.unrestrictedLuaRandomnessEnabled =
		!DedicatedCoopLuaRandomPolicyActive();
	return snapshot;
}

DedicatedCampaignRuntimeFingerprint CampaignFingerprint(
	const RuntimeCompatibilityFingerprint& fingerprint) noexcept
{
	return DedicatedCampaignRuntimeFingerprint{
		fingerprint.schema, fingerprint.high, fingerprint.low};
}

CoopSession::RuntimeCompatibilityFingerprint AdmissionFingerprint(
	const RuntimeCompatibilityFingerprint& fingerprint) noexcept
{
	return CoopSession::RuntimeCompatibilityFingerprint{
		fingerprint.schema, fingerprint.high, fingerprint.low};
}

class DedicatedCampaignSyncCheckpointSource final :
	public CoopSession::FullEngineCoopCampaignCheckpointSource
{
public:
	DedicatedCampaignSyncCheckpointSource(
		DedicatedCampaignCheckpointReader&& reader,
		std::uint64_t campaignSeed,
		const CoopSession::CoopCampaignIdentitySha256& campaignIdentity) noexcept
		: reader_(std::move(reader)),
		  campaignSeed_(campaignSeed),
		  campaignIdentity_(campaignIdentity)
	{
	}

	bool metadata(
		CoopSession::FullEngineCoopCampaignCheckpointMetadata& output) const
		noexcept override
	{
		if (!reader_.isOpen()) return false;
		CoopSession::FullEngineCoopCampaignCheckpointMetadata captured;
		captured.campaignSeed = campaignSeed_;
		captured.campaignIdentitySha256 = campaignIdentity_;
		captured.checkpointGeneration = reader_.generation();
		captured.totalSize = reader_.size();
		captured.checkpointSha256 = reader_.checkpointSha256();
		captured.worldMinutes = reader_.worldMinutes();
		output = captured;
		return true;
	}

	CoopSession::FullEngineCoopCampaignCheckpointReadResult readExact(
		const CoopSession::CoopCampaignCheckpointSha256& expectedCheckpointSha256,
		std::uint64_t offset,
		std::uint8_t* output,
		std::size_t size) noexcept override
	{
		if (!reader_.isOpen() ||
			expectedCheckpointSha256 != reader_.checkpointSha256())
			return CoopSession::
				FullEngineCoopCampaignCheckpointReadResult::DescriptorMismatch;
		return reader_.readExact(offset, output, size)
			? CoopSession::FullEngineCoopCampaignCheckpointReadResult::Success
			: CoopSession::FullEngineCoopCampaignCheckpointReadResult::Unavailable;
	}

private:
	// The native reader pins one already-hashed immutable file identity. A new
	// checkpoint is represented by a new wrapper instance, never by mutation.
	DedicatedCampaignCheckpointReader reader_;
	std::uint64_t campaignSeed_ = 0;
	CoopSession::CoopCampaignIdentitySha256 campaignIdentity_{};
};

class CampaignSyncListenerWireSink final :
	public CoopSession::FullEngineCoopCampaignSyncWireSink
{
public:
	explicit CampaignSyncListenerWireSink(
		CoopSession::FullEngineCoopAdmissionListener& listener) noexcept
		: listener_(listener)
	{
	}

	bool send(const CoopSession::PeerIdentity& peer,
		const CoopSession::TransportPeer& transport,
		CoopSession::FullEngineCoopCampaignSyncOutboundKind,
		const char* messageName,
		const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		CoopSession::TransportPeer current;
		return listener_.authenticatedTransportForPeer(peer, current) &&
			current == transport &&
			listener_.sendToPeer(peer, messageName, bytes, size);
	}

private:
	CoopSession::FullEngineCoopAdmissionListener& listener_;
};

class TacticalReceiptRouter final : public DedicatedCoopTacticalReceiptSink
{
public:
	bool publish(const CoopSession::CoopTacticalIntentReceipt& receipt)
		noexcept override
	{
		return server_ != nullptr && server_->recordReceipt(receipt) ==
			CoopSession::FullEngineCoopTacticalServerResult::Success;
	}

	void bind(CoopSession::FullEngineCoopTacticalServer& server) noexcept
	{
		server_ = &server;
	}
	void unbind() noexcept { server_ = nullptr; }

private:
	CoopSession::FullEngineCoopTacticalServer* server_ = nullptr;
};

// Declaration order is the lifetime contract: both servers are destroyed
// before their source/sink/listener dependencies, while the tactical host and
// its receipt router outlive every possible terminal result publication.
struct TacticalComposition
{
	TacticalComposition(GameContext& context,
		CoopSession::OsAdmissionTokenSource& tokens,
		std::string campaignPackageId)
		: live(context),
		  host(live, GetJa2TacticalCommandService(), router,
			  std::move(campaignPackageId)),
		  ingress(tokens, host),
		  listener(ingress),
		  campaignWire(listener),
		  server(ingress, listener)
	{
		router.bind(server);
	}

	TacticalReceiptRouter router;
	DedicatedCoopTacticalJa2LiveState live;
	DedicatedCoopTacticalHost host;
	CoopSession::FullEngineCoopIngress ingress;
	CoopSession::FullEngineCoopAdmissionListener listener;
	CampaignSyncListenerWireSink campaignWire;
	std::unique_ptr<DedicatedCampaignSyncCheckpointSource> campaignSource;
	std::unique_ptr<CoopSession::FullEngineCoopCampaignSyncServer> campaignSync;
	CoopSession::FullEngineCoopTacticalServer server;
};
}

struct DedicatedCoopRuntime::Impl
{
	bool captureCampaignSource(
		std::unique_ptr<DedicatedCampaignSyncCheckpointSource>& output) noexcept
	{
		DedicatedCampaignCheckpointReader reader;
		if (!boot.openActiveCheckpointReader(reader)) return false;
		std::unique_ptr<DedicatedCampaignSyncCheckpointSource> captured(
			new (std::nothrow) DedicatedCampaignSyncCheckpointSource(
				std::move(reader), boot.campaignSeed(), campaignIdentity));
		if (!captured) return false;
		output = std::move(captured);
		return true;
	}

	bool startCampaignSync() noexcept
	{
		if (tactical == nullptr || !admissionConfigured || sessionEpoch == 0 ||
			tactical->campaignSync != nullptr)
			return false;
		std::unique_ptr<DedicatedCampaignSyncCheckpointSource> source;
		if (!captureCampaignSource(source)) return false;
		std::unique_ptr<CoopSession::FullEngineCoopCampaignSyncServer> server(
			new (std::nothrow) CoopSession::FullEngineCoopCampaignSyncServer(
				*source, tactical->campaignWire));
		if (!server || server->beginSession(sessionEpoch) !=
			CoopSession::FullEngineCoopCampaignSyncServerResult::Success)
			return false;
		tactical->campaignSource = std::move(source);
		tactical->campaignSync = std::move(server);
		return true;
	}

	bool endCampaignSync() noexcept
	{
		if (tactical == nullptr) return true;
		bool ended = true;
		if (tactical->campaignSync != nullptr &&
			tactical->campaignSync->active())
		{
			ended = tactical->campaignSync->endSession() ==
				CoopSession::FullEngineCoopCampaignSyncServerResult::Success;
		}
		tactical->campaignSync.reset();
		tactical->campaignSource.reset();
		return ended;
	}

	bool supersedeCampaignCheckpoint() noexcept
	{
		if (tactical == nullptr || tactical->campaignSync == nullptr) return true;
		std::unique_ptr<DedicatedCampaignSyncCheckpointSource> source;
		if (!captureCampaignSource(source)) return false;
		const CoopSession::FullEngineCoopCampaignSyncServerResult superseded =
			tactical->campaignSync->supersedeCheckpoint(*source);
		if (superseded !=
			CoopSession::FullEngineCoopCampaignSyncServerResult::Success)
			return false;
		// supersedeCheckpoint has atomically redirected the server before the old
		// immutable reader is released.
		tactical->campaignSource = std::move(source);
		return true;
	}

	bool reconcileCampaignPeersAndGateTactical() noexcept
	{
		if (tactical == nullptr || tactical->campaignSync == nullptr ||
			!tactical->campaignSync->active())
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		std::array<CoopSession::FullEngineCoopAuthenticatedPeer,
			CoopSession::MaximumAuthorityPeers> authenticated{};
		const std::size_t count =
			tactical->listener.authenticatedPeers(authenticated);
		std::array<CoopSession::FullEngineCoopCampaignSyncAuthenticatedPeer,
			CoopSession::MaximumFullEngineCoopCampaignSyncPeers> campaignPeers{};
		for (std::size_t index = 0; index < count; ++index)
		{
			campaignPeers[index].peerIdentity =
				authenticated[index].peerIdentity;
			campaignPeers[index].transport = authenticated[index].transport;
		}
		const CoopSession::FullEngineCoopCampaignSyncServerResult reconciled =
			tactical->campaignSync->reconcilePeers(
				count == 0 ? nullptr : campaignPeers.data(), count);
		if (reconciled !=
			CoopSession::FullEngineCoopCampaignSyncServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}

		// Capture readiness before consuming this poll's campaign FIFO. A client
		// cannot make a previously queued tactical frame eligible merely by placing
		// Result::Committed later in the same transport poll; promotion occurs on
		// the next committed-frame pump.
		std::array<CoopSession::PeerIdentity,
			CoopSession::MaximumFullEngineCoopCampaignSyncPeers> ready{};
		std::size_t readyCount =
			tactical->campaignSync->readyPeers(ready);
		if (tactical->server.drainState().inboundMessages != 0)
		{
			// Preserve removals immediately, but do not promote a newly committed
			// peer across tactical bytes queued before that commit. This remains
			// fail-closed even when the execution sink is backpressured and cannot
			// yet let the tactical coordinator retire an ineligible head intent.
			std::array<CoopSession::PeerIdentity,
				CoopSession::MaximumCoopTacticalSessionPeers> previouslyReady{};
			const std::size_t previousCount =
				tactical->server.campaignReadyPeers(previouslyReady);
			std::size_t retainedCount = 0;
			for (std::size_t index = 0; index < readyCount; ++index)
			{
				if (std::find(previouslyReady.begin(),
						previouslyReady.begin() + previousCount, ready[index]) ==
					previouslyReady.begin() + previousCount)
					continue;
				ready[retainedCount++] = ready[index];
			}
			for (std::size_t index = retainedCount; index < ready.size(); ++index)
				ready[index] = CoopSession::PeerIdentity{};
			readyCount = retainedCount;
		}
		const CoopSession::FullEngineCoopTacticalServerResult gated =
			tactical->server.setCampaignReadyPeers(
				readyCount == 0 ? nullptr : ready.data(), readyCount);
		if (gated != CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
			return false;
		}
		return true;
	}

	bool pumpCampaignInboundAndOutbound() noexcept
	{
		if (tactical == nullptr || tactical->campaignSync == nullptr)
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		CoopSession::FullEngineCoopCampaignInboundMessage message;
		while (tactical->listener.popCampaignInbound(message))
		{
			CoopSession::FullEngineCoopCampaignSyncInboundKind kind;
			switch (message.kind)
			{
				case CoopSession::FullEngineCoopCampaignInboundKind::Ack:
					kind = CoopSession::
						FullEngineCoopCampaignSyncInboundKind::Ack;
					break;
				case CoopSession::FullEngineCoopCampaignInboundKind::Result:
					kind = CoopSession::
						FullEngineCoopCampaignSyncInboundKind::Result;
					break;
				case CoopSession::FullEngineCoopCampaignInboundKind::Resync:
					kind = CoopSession::
						FullEngineCoopCampaignSyncInboundKind::Resync;
					break;
				default:
					fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
					return false;
			}
			const CoopSession::FullEngineCoopCampaignSyncServerResult handled =
				tactical->campaignSync->handleInbound(message.peerIdentity,
					message.transport, kind, message.bytes.data(), message.size);
			switch (handled)
			{
				case CoopSession::FullEngineCoopCampaignSyncServerResult::Success:
				case CoopSession::FullEngineCoopCampaignSyncServerResult::InvalidPeer:
				case CoopSession::FullEngineCoopCampaignSyncServerResult::StaleTransport:
				case CoopSession::FullEngineCoopCampaignSyncServerResult::ClaimedIdentityMismatch:
				case CoopSession::FullEngineCoopCampaignSyncServerResult::StaleTransfer:
				case CoopSession::FullEngineCoopCampaignSyncServerResult::UnexpectedFrame:
				case CoopSession::FullEngineCoopCampaignSyncServerResult::SequenceMismatch:
				case CoopSession::FullEngineCoopCampaignSyncServerResult::IntegrityMismatch:
				case CoopSession::FullEngineCoopCampaignSyncServerResult::ClientRejected:
					// Untrusted stale, mismatched, or rejected input advances no server
					// authority. The peer stays unready, which is the fail-closed result.
					break;
				default:
					fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
					return false;
			}
		}

		const CoopSession::FullEngineCoopCampaignSyncFlushResult flushed =
			tactical->campaignSync->flushOutbound();
		if (flushed.result ==
				CoopSession::FullEngineCoopCampaignSyncServerResult::Success ||
			flushed.result == CoopSession::
				FullEngineCoopCampaignSyncServerResult::TransportBackpressured)
			return true;
		fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
		return false;
	}

	void fail(DedicatedCoopRuntimeError reason) noexcept
	{
		if (reason == DedicatedCoopRuntimeError::None) return;
		error = reason;
		fatal = true;
		if (tactical != nullptr)
		{
			tactical->listener.stop(0);
			(void)endCampaignSync();
			if (tactical->server.active() &&
				!tactical->server.worldActive())
				(void)tactical->server.endEpoch();
		}
		gfDedicatedServerProcessFailed = TRUE;
		gfProgramIsRunning = FALSE;
	}

	bool tacticalCommandsDrained() const noexcept
	{
		if (tactical == nullptr) return true;
		const TacticalCommandInboxSummary commandSummary =
			GetJa2TacticalCommandService().summary();
		const Ja2TacticalCommandHostDiagnostics diagnostics =
			GetJa2TacticalCommandHostDiagnostics();
		return DedicatedCoopRetirementLocalDrainState{
			tactical->host.correlationCount(),
			tactical->host.pendingImmediateReceiptCount(),
			commandSummary.pending,
			diagnostics.pendingReceipts,
			diagnostics.pendingDeferredCancellations,
			diagnostics.trackedCommands}.drained();
	}

	bool captureSelfRetirementRequest() noexcept
	{
		if (tactical == nullptr || selfRetirementActive) return true;
		CoopSession::FullEngineCoopSelfRetirementInbound captured;
		if (!tactical->listener.popSelfRetirement(captured)) return true;
		if (tactical->server.discardInboundAfterSelfRetirementGate() !=
			CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
			return false;
		}
		selfRetirement = captured;
		selfRetirementActive = true;
		return true;
	}

	void removeRetiredWorldParticipant(
		const CoopSession::PeerIdentity& identity) noexcept
	{
		std::size_t output = 0;
		for (std::size_t index = 0; index < worldParticipantCount; ++index)
		{
			if (worldParticipants[index] == identity) continue;
			if (output != index)
				worldParticipants[output] = worldParticipants[index];
			++output;
		}
		for (std::size_t index = output; index < worldParticipantCount; ++index)
			worldParticipants[index] = CoopSession::PeerIdentity{};
		worldParticipantCount = output;
		worldParticipantsSelected = output != 0;

		// The replication layer has already removed the retired peer's assignments.
		// Force the next committed publication to deterministically redistribute the
		// actors and make every reconnected survivor enter through a fresh baseline.
		publishedAssignments = {};
		publishedAssignmentCount = 0;
		assignmentsPublished = false;
	}

	bool finishSelfRetirementAtBoundary() noexcept
	{
		if (!selfRetirementActive || tactical == nullptr) return true;
		if (!tacticalCommandsDrained())
			return true;

		CoopSession::FullEngineCoopTacticalPeerCommandState commandState;
		if (tactical->server.peerCommandState(
				selfRetirement.peerIdentity, commandState) &&
			commandState.pendingCommands != 0)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
			return false;
		}

		// Encode the truthful result before the irreversible transition. begin()
		// already reserved tombstone capacity, so completion has no allocation or
		// capacity failure after this point.
		CoopSession::AdmissionSelfRetirementResult result;
		result.sessionEpoch = selfRetirement.request.sessionEpoch;
		result.requestId = selfRetirement.request.requestId;
		result.peerIdentity = selfRetirement.peerIdentity;
		result.result = CoopSession::AdmissionSelfRetirementResultCode::
			CredentialRetired;
		CoopSession::AdmissionSelfRetirementResultBytes encoded{};
		if (!CoopSession::EncodeAdmissionSelfRetirementResult(result, encoded))
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
			return false;
		}
		const CoopSession::AdmissionSelfRetirementRegistryResult completed =
			tactical->ingress.completeSelfRetirement(
				selfRetirement.peerIdentity, selfRetirement.request.requestId);
		if (completed !=
				CoopSession::AdmissionSelfRetirementRegistryResult::Success &&
			completed != CoopSession::AdmissionSelfRetirementRegistryResult::
				AlreadyCompleted)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
			return false;
		}

		// Completion is post-tombstone and best-effort on the exact transport ticket.
		// If it is lost, the same bearer deterministically learns CredentialRetired
		// from admission. No campaign/delta ACK is a prerequisite for this boundary.
		(void)tactical->listener.sendCommittedSelfRetirementResult(
			selfRetirement, encoded);
		if (!stopAdmissionAndReconcile(100)) return false;
		if (tactical->server.retirePeer(selfRetirement.peerIdentity) !=
			CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
			return false;
		}
		removeRetiredWorldParticipant(selfRetirement.peerIdentity);
		selfRetirement = {};
		selfRetirementActive = false;
		return startAdmission();
	}

	std::size_t campaignReadyPeerCount() const noexcept
	{
		if (tactical == nullptr || !tactical->server.active()) return 0;
		std::array<CoopSession::PeerIdentity,
			CoopSession::MaximumCoopTacticalSessionPeers> ready{};
		return tactical->server.campaignReadyPeers(ready);
	}

	bool tacticalNetworkDrained() const noexcept
	{
		return tactical == nullptr || tactical->server.drained();
	}

	bool stopAdmissionAndReconcile(unsigned drainMilliseconds) noexcept
	{
		if (tactical == nullptr) return true;
		tactical->listener.stop(drainMilliseconds);
		if (tactical->campaignSync != nullptr)
		{
			const CoopSession::FullEngineCoopCampaignSyncServerResult
				campaignReconciled = tactical->campaignSync->reconcilePeers(nullptr, 0);
			if (campaignReconciled !=
				CoopSession::FullEngineCoopCampaignSyncServerResult::Success)
			{
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
				return false;
			}
		}
		if (!tactical->server.active()) return true;
		const CoopSession::FullEngineCoopTacticalServerResult gated =
			tactical->server.setCampaignReadyPeers(nullptr, 0);
		if (gated != CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
			return false;
		}
		const CoopSession::FullEngineCoopTacticalServerResult reconciled =
			tactical->server.reconcilePeers();
		if (reconciled ==
			CoopSession::FullEngineCoopTacticalServerResult::Success)
			return true;
		fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
		return false;
	}

	bool startAdmission() noexcept
	{
		if (tactical == nullptr)
		{
			fail(DedicatedCoopRuntimeError::TacticalCompositionFailed);
			return false;
		}
		if (tactical->listener.running()) return true;
		if (!admissionConfigured)
		{
			std::uint64_t epoch = 0;
			if (!tokens.issueSessionEpoch(epoch))
			{
				fail(DedicatedCoopRuntimeError::SessionEpochFailed);
				return false;
			}
			CoopSession::AuthorityConfiguration configuration;
			configuration.enabled = true;
			configuration.sessionEpoch = epoch;
			configuration.runtimeFingerprintSupplied = true;
			configuration.runtimeFingerprint = admissionFingerprint;
			configuration.contentManifestSupplied = true;
			configuration.contentManifestSha256 = contentManifest;
			configuration.maximumPeers = CoopSession::MaximumAuthorityPeers;
			if (tactical->ingress.beginAdmissionSession(configuration) !=
				CoopSession::FullEngineCoopStartResult::Success)
			{
				fail(DedicatedCoopRuntimeError::AdmissionStartFailed);
				return false;
			}
			if (tactical->server.beginEpoch(epoch) !=
				CoopSession::FullEngineCoopTacticalServerResult::Success)
			{
				tactical->ingress.endSession();
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
				return false;
			}
			sessionEpoch = epoch;
			admissionConfigured = true;
		}

		CoopSession::FullEngineCoopAdmissionListenerConfiguration configuration;
		configuration.endpoint = ja2::mp::net::SdlNetEndpoint(
			options.coopPort, options.coopBindAddress.c_str());
		configuration.maximumConnections =
			static_cast<std::uint16_t>(CoopSession::MaximumAuthorityPeers * 2);
		configuration.campaignBootstrap.protocolVersion =
			CoopSession::CurrentProtocolVersion;
		configuration.campaignBootstrap.sessionEpoch = sessionEpoch;
		configuration.campaignBootstrap.campaignSeed = boot.campaignSeed();
		configuration.campaignBootstrap.campaignIdentitySha256 =
			campaignIdentity;
		configuration.campaignBootstrap.runtimeFingerprint =
			admissionFingerprint;
		configuration.campaignBootstrap.contentManifestSha256 =
			contentManifest;
		if (tactical->listener.start(configuration) !=
			CoopSession::FullEngineCoopAdmissionListenerStartResult::Success)
		{
			fail(DedicatedCoopRuntimeError::AdmissionStartFailed);
			return false;
		}
		return true;
	}

	bool checkpointNow(GameContext& context, bool required) noexcept
	{
		const bool restartListener = tactical != nullptr &&
			tactical->listener.running();
		// First evaluate every non-network hazard while admission remains live.
		// NetworkQueue is projected as drained solely for this preflight; only a
		// campaign that is otherwise checkpointable pays the disconnect/drain
		// cost.  In particular, a tactical battle no longer disconnects every
		// client on each periodic retry.
		const DedicatedCheckpointEligibilityReason preflight =
			EvaluateDedicatedCheckpointEligibility(
				CollectCheckpointEligibility(context, true,
					tacticalCommandsDrained(), true));
		if (preflight != DedicatedCheckpointEligibilityReason::None)
		{
			lastEligibility = preflight;
			if (required) fail(DedicatedCoopRuntimeError::CheckpointNotEligible);
			return false;
		}

		if (restartListener && !stopAdmissionAndReconcile(100)) return false;
		const DedicatedCheckpointEligibilityReason eligibility =
			EvaluateDedicatedCheckpointEligibility(
				CollectCheckpointEligibility(context, true,
					tacticalCommandsDrained(), tacticalNetworkDrained()));
		if (eligibility != DedicatedCheckpointEligibilityReason::None)
		{
			lastEligibility = eligibility;
			if (restartListener && !fatal) (void)startAdmission();
			if (required) fail(DedicatedCoopRuntimeError::CheckpointNotEligible);
			return false;
		}

		campaignResult = boot.checkpoint(GetWorldTotalMin());
		if (!campaignResult)
		{
			fail(DedicatedCoopRuntimeError::CheckpointFailed);
			return false;
		}
		// Pin and validate the newly committed immutable file, then supersede
		// every campaign transfer before any admission transport can restart.
		if (!supersedeCampaignCheckpoint())
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		lastEligibility = DedicatedCheckpointEligibilityReason::None;
		lastCheckpoint = Clock::now();
		nextCheckpointAttempt = lastCheckpoint + checkpointInterval;
		if (restartListener && !startAdmission()) return false;
		return true;
	}

	bool freshAssignmentBaselineBoundary() const noexcept
	{
		if (tactical == nullptr || !tacticalCommandsDrained() ||
			(tacticalContext != nullptr &&
				tacticalContext->runtimeMessages().queued() != 0))
		{
			return false;
		}
		const CoopSession::FullEngineCoopTacticalServerDrainState drain =
			tactical->server.drainState();
		return drain.inboundMessages == 0 &&
			drain.receiptObligationsCleared() &&
			drain.peersAwaitingReplication == 0 &&
			drain.inFlightDeltas == 0;
	}

	bool reconcileWorldParticipants() noexcept
	{
		if (tactical == nullptr) return true;
		std::array<CoopSession::FullEngineCoopAuthenticatedPeer,
			CoopSession::MaximumAuthorityPeers> peers{};
		const std::size_t authenticatedCount =
			tactical->listener.authenticatedPeers(peers);
		std::array<CoopSession::PeerIdentity,
			CoopSession::MaximumCoopTacticalSessionPeers> ready{};
		const std::size_t readyCount =
			tactical->server.campaignReadyPeers(ready);
		std::array<CoopSession::PeerIdentity,
			CoopSession::MaximumCoopTacticalSessionPeers> eligible{};
		std::size_t eligibleCount = 0;
		for (std::size_t index = 0; index < authenticatedCount; ++index)
		{
			if (std::find(ready.begin(), ready.begin() + readyCount,
					peers[index].peerIdentity) == ready.begin() + readyCount)
				continue;
			eligible[eligibleCount++] = peers[index].peerIdentity;
		}

		std::array<CoopSession::PeerIdentity,
			CoopSession::MaximumCoopTacticalSessionPeers> staged{};
		std::size_t stagedCount = 0;
		const CoopSession::CoopWorldParticipantPolicyResult result =
			CoopSession::GrowCoopWorldParticipants(
				worldParticipantCount == 0 ? nullptr : worldParticipants.data(),
				worldParticipantCount,
				eligibleCount == 0 ? nullptr : eligible.data(), eligibleCount,
				!assignmentsPublished || freshAssignmentBaselineBoundary(),
				staged, stagedCount);
		switch (result)
		{
			case CoopSession::CoopWorldParticipantPolicyResult::Unchanged:
			case CoopSession::CoopWorldParticipantPolicyResult::
				DeferredUntilFreshBaseline:
				return true;
			case CoopSession::CoopWorldParticipantPolicyResult::Published:
				worldParticipants = staged;
				worldParticipantCount = stagedCount;
				worldParticipantsSelected = stagedCount != 0;
				return true;
			case CoopSession::CoopWorldParticipantPolicyResult::InvalidCurrentSet:
			case CoopSession::CoopWorldParticipantPolicyResult::InvalidReadySet:
			case CoopSession::CoopWorldParticipantPolicyResult::CapacityReached:
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
				return false;
		}
		fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
		return false;
	}

	bool stageCurrentWorld(const TacticalWorldSnapshot& snapshot) noexcept
	{
		if (tactical == nullptr || !tactical->server.worldActive()) return true;
		if (!reconcileWorldParticipants()) return false;

		std::array<CoopSession::CoopTacticalActorAssignment,
			CoopSession::MaximumCoopTacticalAssignments> assignments{};
		std::size_t assignmentCount = 0;
		if (worldParticipantsSelected)
		{
			DedicatedCoopTacticalActorList actors{};
			std::size_t actorCount = 0;
			if (!tactical->host.collectControllableActors(actors, actorCount) ||
				actorCount > assignments.size())
			{
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
				return false;
			}
			if (CoopSession::BuildCoopActorAssignments(
				worldParticipants.data(), worldParticipantCount,
				actors.data(), actorCount, assignments, assignmentCount) !=
				CoopSession::CoopActorAssignmentPolicyResult::Success)
			{
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
				return false;
			}
		}

		bool assignmentsChanged = !assignmentsPublished ||
			assignmentCount != publishedAssignmentCount;
		for (std::size_t index = 0;
			!assignmentsChanged && index < assignmentCount; ++index)
		{
			assignmentsChanged =
				publishedAssignments[index].actor != assignments[index].actor ||
				publishedAssignments[index].peerIdentity !=
					assignments[index].peerIdentity;
		}
		if (assignmentsChanged)
		{
			const CoopSession::FullEngineCoopTacticalServerResult assigned =
				tactical->server.replaceAssignments(
					assignmentCount == 0 ? nullptr : assignments.data(),
					assignmentCount);
			if (assigned !=
				CoopSession::FullEngineCoopTacticalServerResult::Success)
			{
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
				return false;
			}
			publishedAssignments = assignments;
			publishedAssignmentCount = assignmentCount;
			assignmentsPublished = true;
		}

		std::array<CoopSession::PeerIdentity,
			CoopSession::MaximumCoopTacticalSessionPeers> peersNeedingBaseline{};
		if (tactical->server.peersNeedingBaseline(peersNeedingBaseline) == 0)
			return true;
		const CoopSession::FullEngineCoopTacticalServerResult staged =
			tactical->server.stageBaselines(snapshot);
		if (staged !=
			CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalReplicationFailed);
			return false;
		}
		return true;
	}

	bool beginWorldDrain() noexcept
	{
		if (tactical == nullptr || !tactical->server.worldActive()) return true;
		if (worldDraining) return true;
		const bool tacticalMissionActive =
			starterMission == StarterMissionState::WaitingForControllableActor ||
			starterMission == StarterMissionState::Playable ||
			starterMission == StarterMissionState::ReturningToStrategic;
		if (DedicatedCoopWorldDrainRequiresStrategicCheckpoint(
				postCombatReturnArmed, tacticalMissionActive))
		{
			// Victory normally reaches the explicit return path below. Defeat is
			// different: native JA2 unloads the world asynchronously, so the
			// vanished observer publication is the first committed boundary the
			// runtime can use. Preserve the strategic mutations with the same
			// drain/checkpoint barrier used by victory.
			holdAdmissionAfterWorldDrain = true;
		}
		worldDraining = true;
		if (!stopAdmissionAndReconcile(100)) return false;
		const CoopSession::FullEngineCoopTacticalServerResult retired =
			tactical->server.discardInboundAfterTransportStop();
		if (retired ==
			CoopSession::FullEngineCoopTacticalServerResult::Success)
			return true;
		fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
		return false;
	}

	bool stopAdmissionForPostCombatReturn() noexcept
	{
		if (tactical == nullptr || !tactical->server.worldActive())
		{
			fail(DedicatedCoopRuntimeError::MissionReturnFailed);
			return false;
		}
		if (!stopAdmissionAndReconcile(100)) return false;
		const CoopSession::FullEngineCoopTacticalServerResult retired =
			tactical->server.discardInboundAfterTransportStop();
		if (retired ==
			CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			return true;
		}
		fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
		return false;
	}

	bool tryFinishWorldDrain(GameContext& context) noexcept
	{
		if (!worldDraining || tactical == nullptr) return true;
		(void)tactical->host.flushPendingReceipts();
		const TacticalCommandInboxSummary commandSummary =
			GetJa2TacticalCommandService().summary();
		const Ja2TacticalCommandHostDiagnostics diagnostics =
			GetJa2TacticalCommandHostDiagnostics();
		if (commandSummary.pending != 0 || diagnostics.pendingReceipts != 0 ||
			diagnostics.pendingDeferredCancellations != 0 ||
			diagnostics.trackedCommands != 0 ||
			context.runtimeMessages().queued() != 0)
			return true;
		if (!tactical->host.endWorld()) return true;
		(void)tactical->host.flushPendingReceipts();

		const CoopSession::FullEngineCoopTacticalServerPumpResult flushed =
			tactical->server.flushOutbound();
		if (flushed.backpressured || flushed.result ==
			CoopSession::FullEngineCoopTacticalServerResult::TransportBackpressured)
			return true;
		if (flushed.result !=
			CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalReplicationFailed);
			return false;
		}
		const CoopSession::FullEngineCoopTacticalServerDrainState drain =
			tactical->server.drainState();
		if (drain.inboundMessages != 0 || !drain.receiptObligationsCleared())
			return true;
		const CoopSession::FullEngineCoopTacticalServerResult ended =
			tactical->server.endWorld();
		if (ended == CoopSession::FullEngineCoopTacticalServerResult::PendingInput ||
			ended == CoopSession::FullEngineCoopTacticalServerResult::PendingReceipts ||
			ended ==
				CoopSession::FullEngineCoopTacticalServerResult::TransportBackpressured)
			return true;
		if (ended != CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
			return false;
		}
		(void)tactical->server.takeTransportRestartRequired();
		observedWorldGeneration = 0;
		observedRevision = 0;
		publishedAssignments = {};
		publishedAssignmentCount = 0;
		assignmentsPublished = false;
		worldParticipants = {};
		worldParticipantCount = 0;
		worldParticipantsSelected = false;
		worldDraining = false;
		postCombatReturnArmed = false;
		if (holdAdmissionAfterWorldDrain)
		{
			holdAdmissionAfterWorldDrain = false;
			starterMission = StarterMissionState::WaitingForStrategicCheckpoint;
			return true;
		}
		return startAdmission();
	}

	bool updateObservedWorld(
		const TacticalWorldPublicationView& publication) noexcept
	{
		if (tactical == nullptr || !publication || publication.snapshot == nullptr)
			return true;
		const std::uint64_t generation = publication.snapshot->epoch();
		const std::uint64_t turnSerial = publication.snapshot->turn().serial;
		if (!tactical->server.worldActive())
		{
			const CoopSession::FullEngineCoopTacticalServerResult begun =
				tactical->server.beginWorld(
					generation, publication.serial, turnSerial);
			if (begun !=
				CoopSession::FullEngineCoopTacticalServerResult::Success)
			{
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
				return false;
			}
			observedWorldGeneration = generation;
			observedRevision = publication.serial;
			publishedAssignments = {};
			publishedAssignmentCount = 0;
			assignmentsPublished = false;
			worldParticipants = {};
			worldParticipantCount = 0;
			worldParticipantsSelected = false;
			return true;
		}
		if (generation != observedWorldGeneration)
			return beginWorldDrain();
		if (publication.serial < observedRevision)
		{
			fail(DedicatedCoopRuntimeError::TacticalReplicationFailed);
			return false;
		}
		if (publication.serial == observedRevision) return true;
		if (publication.status != TacticalWorldPublicationStatus::Delta ||
			publication.delta == nullptr)
		{
			fail(DedicatedCoopRuntimeError::TacticalReplicationFailed);
			return false;
		}
		const CoopSession::FullEngineCoopTacticalServerResult published =
			tactical->server.publishDelta(
				*publication.delta, publication.serial, turnSerial);
		if (published !=
			CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalReplicationFailed);
			return false;
		}
		observedRevision = publication.serial;
		return true;
	}

	bool pumpTactical(GameContext& context) noexcept
	{
		if (tactical == nullptr || !admissionConfigured) return true;
		const Ja2TacticalWorldObserverDiagnostics observerDiagnostics =
			GetJa2TacticalWorldObserverDiagnostics();
		switch (observerDiagnostics.lastUpdate)
		{
			case TacticalWorldObserverUpdateResult::PublishedBaseline:
			case TacticalWorldObserverUpdateResult::PublishedDelta:
			case TacticalWorldObserverUpdateResult::Unchanged:
			case TacticalWorldObserverUpdateResult::SourceUnavailable:
				break;
			case TacticalWorldObserverUpdateResult::SourceCapacityReached:
			case TacticalWorldObserverUpdateResult::SourceAllocationFailure:
			case TacticalWorldObserverUpdateResult::SourceAdapterFailure:
			case TacticalWorldObserverUpdateResult::InvalidSnapshot:
			case TacticalWorldObserverUpdateResult::ActorCapacityReached:
			case TacticalWorldObserverUpdateResult::DoorCapacityReached:
			case TacticalWorldObserverUpdateResult::EventCapacityReached:
			case TacticalWorldObserverUpdateResult::AllocationFailure:
			case TacticalWorldObserverUpdateResult::SerialExhausted:
			{
				// latest() deliberately retains its last good value on capture/diff
				// failure. Never pair a newly completed command with that stale
				// publication: stop the session before any receipt can cross the wire.
				fail(DedicatedCoopRuntimeError::TacticalReplicationFailed);
				return false;
			}
		}
		const TacticalWorldPublicationView publication =
			GetJa2TacticalWorldObserverService().latest();
		if (!publication && tactical->server.worldActive() &&
			!beginWorldDrain())
			return false;
		if (worldDraining) return tryFinishWorldDrain(context);
		if (!updateObservedWorld(publication)) return false;
		if (worldDraining) return tryFinishWorldDrain(context);
		// The observer's new revision must be published before a terminal command
		// receipt is recorded. The server then sends that delta before the receipt,
		// so a client never unlocks input against state it has not yet applied.
		(void)tactical->host.flushPendingReceipts();
		if (selfRetirementActive)
			return finishSelfRetirementAtBoundary();

		tactical->listener.poll();
		if (!captureSelfRetirementRequest()) return false;
		if (selfRetirementActive)
			return finishSelfRetirementAtBoundary();
		if (!reconcileCampaignPeersAndGateTactical() ||
			!pumpCampaignInboundAndOutbound())
			return false;
		const CoopSession::FullEngineCoopTacticalServerResult reconciled =
			tactical->server.reconcilePeers();
		if (reconciled !=
			CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
			return false;
		}
		if (publication && tactical->server.worldActive() &&
			!stageCurrentWorld(*publication.snapshot))
			return false;

		const Ja2TacticalCommandHostDiagnostics diagnostics =
			GetJa2TacticalCommandHostDiagnostics();
		const CoopSession::FullEngineCoopTacticalServerPumpResult pumped =
			tactical->server.pumpInbound(diagnostics.simulationTick);
		if (pumped.result !=
				CoopSession::FullEngineCoopTacticalServerResult::Success &&
			pumped.result !=
				CoopSession::FullEngineCoopTacticalServerResult::NoWorld &&
			pumped.result !=
				CoopSession::FullEngineCoopTacticalServerResult::InputRejected &&
			pumped.result !=
				CoopSession::FullEngineCoopTacticalServerResult::ExecutionBackpressured &&
			pumped.result !=
				CoopSession::FullEngineCoopTacticalServerResult::TransportBackpressured)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
			return false;
		}
		(void)tactical->host.flushPendingReceipts();
		if (tactical->server.worldActive())
		{
			const CoopSession::FullEngineCoopTacticalServerPumpResult flushed =
				tactical->server.flushOutbound();
			if (flushed.result !=
					CoopSession::FullEngineCoopTacticalServerResult::Success &&
				flushed.result !=
					CoopSession::FullEngineCoopTacticalServerResult::TransportBackpressured)
			{
				fail(DedicatedCoopRuntimeError::TacticalReplicationFailed);
				return false;
			}
		}
		return true;
	}

	bool destroyTacticalComposition() noexcept
	{
		if (tactical == nullptr) return true;
		tactical->listener.stop(0);
		const bool campaignEnded = endCampaignSync();
		if (tactical->server.active() && !tactical->server.worldActive())
			(void)tactical->server.endEpoch();
		if (!campaignEnded) return false;
		if (tacticalSinkRegistered && tacticalContext != nullptr)
		{
			const RuntimeMessageSinkRegistrationError removed =
				tacticalContext->runtimeMessages().removeSink(tactical->host);
			if (removed ==
				RuntimeMessageSinkRegistrationError::DispatchInProgress)
				return false;
			if (removed != RuntimeMessageSinkRegistrationError::None &&
				removed != RuntimeMessageSinkRegistrationError::NotFound)
				return false;
			tacticalSinkRegistered = false;
		}
		tactical->router.unbind();
		delete tactical;
		tactical = nullptr;
		tacticalContext = nullptr;
		return true;
	}

	bool detachTacticalComposition() noexcept
	{
		if (destroyTacticalComposition()) return true;
		// A sink that is still inside RuntimeMessageBus dispatch cannot be
		// destroyed safely.  This is an exceptional process-exit path: stop its
		// transport, deliberately abandon the composition, and sever every
		// pointer the later campaign-lease boundary could otherwise dereference
		// after GameContext has gone away.
		if (tactical != nullptr) tactical->listener.stop(0);
		tactical = nullptr;
		tacticalContext = nullptr;
		tacticalSinkRegistered = false;
		if (!fatal) fail(DedicatedCoopRuntimeError::TacticalCompositionFailed);
		else
		{
			gfDedicatedServerProcessFailed = TRUE;
			gfProgramIsRunning = FALSE;
		}
		return false;
	}

	DedicatedCampaignBoot boot;
	CoopSession::OsAdmissionTokenSource tokens;
	TacticalComposition* tactical = nullptr;
	GameContext* tacticalContext = nullptr;
	DedicatedServerOptions options;
	DedicatedCampaignBootResult campaignResult;
	DedicatedCoopRuntimeError error = DedicatedCoopRuntimeError::None;
	DedicatedCoopCampaignEntry entry = DedicatedCoopCampaignEntry::None;
	DedicatedCheckpointEligibilityReason lastEligibility =
		DedicatedCheckpointEligibilityReason::None;
	CoopSession::RuntimeCompatibilityFingerprint admissionFingerprint;
	CoopSession::ContentManifestSha256 contentManifest{};
	CoopSession::CoopCampaignIdentitySha256 campaignIdentity{};
	std::array<CoopSession::PeerIdentity,
		CoopSession::MaximumCoopTacticalSessionPeers> worldParticipants{};
	std::array<CoopSession::CoopTacticalActorAssignment,
		CoopSession::MaximumCoopTacticalAssignments> publishedAssignments{};
	std::chrono::seconds checkpointInterval{300};
	Clock::time_point lastCheckpoint{};
	Clock::time_point nextCheckpointAttempt{};
	Clock::time_point starterPeerGatherDeadline{};
	Clock::time_point starterActorArrivalDeadline{};
	std::uint64_t sessionEpoch = 0;
	std::uint64_t observedWorldGeneration = 0;
	std::uint64_t observedRevision = 0;
	std::size_t publishedAssignmentCount = 0;
	std::size_t worldParticipantCount = 0;
	std::size_t minimumControllableActors = 0;
	bool prepared = false;
	bool campaignOpen = false;
	bool entryRequested = false;
	bool campaignEntered = false;
	bool admissionConfigured = false;
	bool tacticalSinkRegistered = false;
	bool contentManifestCaptured = false;
	bool worldParticipantsSelected = false;
	bool assignmentsPublished = false;
	bool worldDraining = false;
	bool postCombatReturnArmed = false;
	bool holdAdmissionAfterWorldDrain = false;
	CoopSession::FullEngineCoopSelfRetirementInbound selfRetirement;
	bool selfRetirementActive = false;
	bool fatal = false;
	StarterMissionState starterMission = StarterMissionState::Unprepared;
};

DedicatedCoopRuntime::DedicatedCoopRuntime() noexcept
	: impl_(new (std::nothrow) Impl())
{
}

DedicatedCoopRuntime::~DedicatedCoopRuntime() noexcept
{
	close();
	delete impl_;
	impl_ = nullptr;
}

bool DedicatedCoopRuntime::prepareEarly() noexcept
{
	if (!impl_ || impl_->prepared || impl_->fatal) return false;
	impl_->options = GetDedicatedServerOptions();
	if (!impl_->options.enabled ||
		impl_->options.mode != DedicatedServerMode::Coop)
	{
		impl_->error = DedicatedCoopRuntimeError::NotDedicatedCoop;
		return false;
	}
	impl_->campaignResult = impl_->boot.prepare(impl_->options);
	if (!impl_->campaignResult)
	{
		impl_->fail(DedicatedCoopRuntimeError::CampaignPrepareFailed);
		return false;
	}
	if (InstallGameSimulationRandom(impl_->boot.campaignSeed()) !=
		GameSimulationRandomInstallError::None)
	{
		impl_->fail(DedicatedCoopRuntimeError::SimulationRandomInstallFailed);
		return false;
	}
	// The boot object deliberately does not expose an entry until the second
	// (identity-checked) phase opens the store.  Screen routing happens before
	// that phase completes, so preserve the already validated CLI intent here
	// instead of treating the boot object's temporary None value as Resume.
	impl_->entry = impl_->options.campaignAction ==
		DedicatedCampaignAction::Create
		? DedicatedCoopCampaignEntry::Create
		: DedicatedCoopCampaignEntry::Resume;
	impl_->checkpointInterval =
		std::chrono::seconds(impl_->options.checkpointSeconds);
	impl_->prepared = true;
	return true;
}

bool DedicatedCoopRuntime::captureContentManifestAfterPackageMount() noexcept
{
	if (!impl_ || !impl_->prepared || impl_->campaignOpen || impl_->fatal ||
		impl_->contentManifestCaptured)
	{
		if (impl_) impl_->fail(DedicatedCoopRuntimeError::InvalidState);
		return false;
	}
	vfs::CVirtualFileSystem* const fileSystem = getVFS();
	DedicatedContentManifestSha256 content{};
	const DedicatedContentManifestError manifestResult = fileSystem
		? ComputeDedicatedContentManifestFromVfs(*fileSystem, content)
		: DedicatedContentManifestError::SourceFailure;
	if (manifestResult != DedicatedContentManifestError::None)
	{
		std::fprintf(stderr,
			"[dedicated] installed content manifest capture failed: %s\n",
			DedicatedContentManifestErrorName(manifestResult));
		impl_->fail(DedicatedCoopRuntimeError::ContentManifestFailed);
		return false;
	}
	std::copy(content.begin(), content.end(), impl_->contentManifest.begin());
	impl_->contentManifestCaptured = true;
	return true;
}

bool DedicatedCoopRuntime::openCampaignAfterBootstrap(
	GameContext& context) noexcept
{
	if (!impl_ || !impl_->prepared || !impl_->contentManifestCaptured ||
		impl_->campaignOpen || impl_->fatal)
		return false;
	if (!context.campaignSimulationEnabled())
	{
		impl_->fail(DedicatedCoopRuntimeError::CampaignSimulationUnavailable);
		return false;
	}
	const RuntimeCompatibilityFingerprint fingerprint =
		context.runtime().compatibilityFingerprint();
	impl_->campaignResult = impl_->boot.openCampaign(
		CampaignFingerprint(fingerprint), impl_->contentManifest);
	if (!impl_->campaignResult)
	{
		impl_->fail(DedicatedCoopRuntimeError::CampaignOpenFailed);
		return false;
	}
	const DedicatedCampaignBootEntry openedEntry = impl_->boot.entry();
	if ((impl_->entry == DedicatedCoopCampaignEntry::Create &&
			openedEntry != DedicatedCampaignBootEntry::CreateNewCampaign) ||
		(impl_->entry == DedicatedCoopCampaignEntry::Resume &&
			openedEntry != DedicatedCampaignBootEntry::ResumeCheckpoint))
	{
		impl_->fail(DedicatedCoopRuntimeError::CampaignOpenFailed);
		return false;
	}
	impl_->admissionFingerprint = AdmissionFingerprint(fingerprint);
	if (!CoopSession::ComputeCoopCampaignIdentitySha256(
		impl_->options.campaignId, impl_->boot.campaignSeed(),
		impl_->campaignIdentity))
	{
		impl_->fail(DedicatedCoopRuntimeError::CampaignOpenFailed);
		return false;
	}
	TacticalComposition* composition = nullptr;
	try
	{
		const std::string packageId = context.packages().activeCampaign();
		if (packageId.empty())
		{
			impl_->fail(DedicatedCoopRuntimeError::TacticalCompositionFailed);
			return false;
		}
		composition = new TacticalComposition(
			context, impl_->tokens, packageId);
		if (context.runtimeMessages().addSink(composition->host) !=
			RuntimeMessageSinkRegistrationError::None)
		{
			composition->router.unbind();
			delete composition;
			impl_->fail(DedicatedCoopRuntimeError::TacticalCompositionFailed);
			return false;
		}
	}
	catch (...)
	{
		if (composition != nullptr)
		{
			composition->router.unbind();
			delete composition;
		}
		impl_->fail(DedicatedCoopRuntimeError::TacticalCompositionFailed);
		return false;
	}
	impl_->tactical = composition;
	impl_->tacticalContext = &context;
	impl_->tacticalSinkRegistered = true;
	impl_->campaignOpen = true;
	return true;
}

bool DedicatedCoopRuntime::requestCampaignEntry() noexcept
{
	if (!impl_ || !impl_->campaignOpen || impl_->campaignEntered || impl_->fatal)
		return false;
	impl_->entryRequested = true;
	return true;
}

void DedicatedCoopRuntime::pumpAfterCommittedFrame(GameContext& context) noexcept
{
	if (!impl_ || impl_->fatal || !impl_->campaignOpen) return;
	if (impl_->entryRequested && !impl_->campaignEntered)
	{
		if (impl_->entry == DedicatedCoopCampaignEntry::Resume)
		{
			const DedicatedCampaignStoreState* const state =
				impl_->boot.campaignState();
			if (!state || !state->hasCheckpoint ||
				!LoadDedicatedCampaignGame(state->activeSlot))
			{
				impl_->fail(DedicatedCoopRuntimeError::CampaignEntryFailed);
				return;
			}
			if (guiScreenToGotoAfterLoadingSavedGame != MAP_SCREEN)
			{
				impl_->fail(DedicatedCoopRuntimeError::CampaignEntryFailed);
				return;
			}
			SetPendingNewScreen(MAP_SCREEN);
		}
		const DedicatedCoopStarterCampaignState starterState =
			InspectDedicatedCoopStarterCampaign();
		bool rosterCheckpointRequired = false;
		if (starterState ==
			DedicatedCoopStarterCampaignState::UntouchedInitial)
		{
			const DedicatedCoopMissionPreparationResult mission =
				PrepareDedicatedCoopStarterMission();
			if (!mission)
			{
				std::fprintf(stderr,
					"[dedicated] starter mission preparation failed: %s\n",
					DedicatedCoopMissionBootstrapErrorName(mission.error));
				impl_->fail(DedicatedCoopRuntimeError::MissionPrepareFailed);
				return;
			}
			rosterCheckpointRequired = true;
		}
		else if ((starterState !=
					DedicatedCoopStarterCampaignState::PreparedInitial &&
				starterState !=
					DedicatedCoopStarterCampaignState::EstablishedCold) ||
			impl_->entry != DedicatedCoopCampaignEntry::Resume)
		{
			std::fprintf(stderr,
				"[dedicated] starter campaign state rejected: %s\n",
				DedicatedCoopStarterCampaignStateName(starterState));
			impl_->fail(DedicatedCoopRuntimeError::MissionPrepareFailed);
			return;
		}
		// The complete roster and its pending arrival events become durable before
		// admission or campaign transfer can expose this campaign to any peer. An
		// exact prepared resume already is that artifact and is left byte-for-byte
		// unchanged here.
		if (rosterCheckpointRequired && !impl_->checkpointNow(context, true))
		{
			return;
		}
		impl_->starterMission = starterState ==
			DedicatedCoopStarterCampaignState::EstablishedCold
			? StarterMissionState::WaitingForEstablishedCampaignReadyPeer
			: StarterMissionState::WaitingForCampaignReadyPeer;
		impl_->minimumControllableActors = 0;
		impl_->starterPeerGatherDeadline = {};
		impl_->campaignEntered = true;
		impl_->entryRequested = false;
		impl_->lastCheckpoint = Clock::now();
		impl_->nextCheckpointAttempt =
			impl_->lastCheckpoint + impl_->checkpointInterval;
		if (!impl_->startAdmission()) return;
		if (!impl_->startCampaignSync())
		{
			impl_->fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return;
		}
		if (!impl_->reconcileCampaignPeersAndGateTactical()) return;
		std::printf("[dedicated] co-op campaign %s; admission listening on %s:%u\n",
			impl_->entry == DedicatedCoopCampaignEntry::Create
				? "created" : "resumed",
			impl_->options.coopBindAddress.c_str(),
			static_cast<unsigned>(impl_->options.coopPort));
		std::fflush(stdout);
		return;
	}

	if (!impl_->campaignEntered) return;
	if (!impl_->pumpTactical(context)) return;
	const Clock::time_point now = Clock::now();
	if (impl_->starterMission ==
		StarterMissionState::WaitingForCampaignReadyPeer)
	{
		const std::size_t readyPeers = impl_->campaignReadyPeerCount();
		if (readyPeers == 0)
		{
			// A disconnect before launch starts a fresh grace for the next cohort.
			impl_->starterPeerGatherDeadline = {};
			return;
		}
		if (impl_->starterPeerGatherDeadline == Clock::time_point{})
			impl_->starterPeerGatherDeadline = now + StarterPeerGatherGrace;
		const bool graceElapsed =
			now >= impl_->starterPeerGatherDeadline;
		if (DedicatedCoopStarterLaunchReady(readyPeers, graceElapsed,
				IsDedicatedCoopStarterMissionMapReady()))
		{
			const DedicatedCoopMissionBootstrapError launched =
				LaunchDedicatedCoopStarterMission();
			if (launched != DedicatedCoopMissionBootstrapError::None)
			{
				std::fprintf(stderr,
					"[dedicated] starter mission launch failed: %s\n",
					DedicatedCoopMissionBootstrapErrorName(launched));
				impl_->fail(DedicatedCoopRuntimeError::MissionLaunchFailed);
				return;
			}
			impl_->starterMission =
				StarterMissionState::WaitingForControllableActor;
			impl_->minimumControllableActors =
				DedicatedCoopStarterRosterSize;
			impl_->postCombatReturnArmed = true;
			impl_->starterActorArrivalDeadline =
				now + StarterActorArrivalTimeout;
		}
		return;
	}
	if (impl_->starterMission ==
		StarterMissionState::WaitingForEstablishedCampaignReadyPeer)
	{
		if (impl_->campaignReadyPeerCount() == 0 ||
			!IsDedicatedCoopStarterMissionMapReady())
		{
			return;
		}
		const DedicatedCoopMissionBootstrapError launched =
			LaunchDedicatedCoopEstablishedMission();
		if (launched == DedicatedCoopMissionBootstrapError::NoHostileEncounter)
		{
			// There is no strategic intent/sector-exit protocol yet. Keep a
			// peaceful established campaign cold and connected instead of trapping
			// it in an unfinishable tactical world.
			impl_->starterMission = StarterMissionState::StrategicIdle;
			std::printf(
				"[dedicated] established campaign remains worldless; no hostile occupied sector\n");
			std::fflush(stdout);
			return;
		}
		if (launched != DedicatedCoopMissionBootstrapError::None)
		{
			std::fprintf(stderr,
				"[dedicated] established campaign tactical entry failed: %s\n",
				DedicatedCoopMissionBootstrapErrorName(launched));
			impl_->fail(DedicatedCoopRuntimeError::MissionLaunchFailed);
			return;
		}
		impl_->starterMission =
			StarterMissionState::WaitingForControllableActor;
		impl_->minimumControllableActors = 1;
		impl_->postCombatReturnArmed = true;
		impl_->starterActorArrivalDeadline =
			now + StarterActorArrivalTimeout;
		return;
	}
	if (impl_->starterMission ==
		StarterMissionState::WaitingForControllableActor)
	{
		const std::size_t actors = CountDedicatedCoopControllableActors();
		if (impl_->minimumControllableActors != 0 &&
			actors >= impl_->minimumControllableActors)
		{
			impl_->starterMission = StarterMissionState::Playable;
			std::printf(
				"[dedicated] co-op tactical encounter playable with %zu actors\n",
				actors);
			std::fflush(stdout);
		}
		else if (now >= impl_->starterActorArrivalDeadline)
		{
			impl_->fail(DedicatedCoopRuntimeError::MissionActorUnavailable);
		}
		return;
	}
	if (impl_->starterMission == StarterMissionState::Playable)
	{
		const DedicatedCoopPostCombatReturnEvidence evidence =
			CapturePostCombatReturnEvidence(true,
				impl_->postCombatReturnArmed);
		if (DedicatedCoopPostCombatReturnReady(evidence))
		{
			// Freeze ingress immediately at the first committed victory observation.
			// A peer can therefore hold neither the campaign hostage with an ACK nor
			// the local drain busy with a continuous stream of otherwise valid
			// commands. Already-submitted work is finite, executes under the normal
			// authority path, and is covered by the next-frame evidence recheck.
			impl_->starterMission = StarterMissionState::ReturningToStrategic;
			if (!impl_->stopAdmissionForPostCombatReturn()) return;
		}
		return;
	}
	if (impl_->starterMission == StarterMissionState::ReturningToStrategic)
	{
		const DedicatedCoopPostCombatReturnEvidence rechecked =
			CapturePostCombatReturnEvidence(true,
				impl_->postCombatReturnArmed);
		const DedicatedCoopPostCombatReturnStep next =
			EvaluateDedicatedCoopPostCombatReturnStep(
				DedicatedCoopPostCombatReturnReady(rechecked),
				impl_->freshAssignmentBaselineBoundary());
		if (next == DedicatedCoopPostCombatReturnStep::ResumePlayable)
		{
			// The unload proof regressed after ingress was frozen. Restore the
			// live world and its same-epoch admission session instead of leaving
			// every client disconnected forever.
			impl_->starterMission = StarterMissionState::Playable;
			if (!impl_->startAdmission()) return;
			return;
		}
		if (next ==
			DedicatedCoopPostCombatReturnStep::WaitForFreshBoundary)
		{
			return;
		}
		if (!UnloadCurrentWorldForDedicatedCoopPostCombatReturn())
		{
			impl_->fail(DedicatedCoopRuntimeError::MissionReturnFailed);
			return;
		}
		impl_->holdAdmissionAfterWorldDrain = true;
		if (!impl_->beginWorldDrain()) return;
		return;
	}
	if (impl_->starterMission ==
		StarterMissionState::WaitingForStrategicCheckpoint)
	{
		if (!IsDedicatedCoopStarterMissionMapReady()) return;
		if (!impl_->checkpointNow(context, true)) return;
		if (!impl_->startAdmission()) return;
		impl_->starterMission = StarterMissionState::StrategicIdle;
		std::printf(
			"[dedicated] post-combat strategic checkpoint committed; admission reopened\n");
		std::fflush(stdout);
		return;
	}
	if (now < impl_->nextCheckpointAttempt) return;
	if (!impl_->checkpointNow(context, false) && !impl_->fatal)
		impl_->nextCheckpointAttempt = now + IneligibleRetryDelay;
}

bool DedicatedCoopRuntime::shutdownAtCommittedBoundary(
	GameContext& context) noexcept
{
	if (!impl_ || !impl_->campaignEntered || impl_->fatal) return !failed();
	if (!impl_->stopAdmissionAndReconcile(100)) return false;
	if (impl_->tactical != nullptr && impl_->tactical->server.active())
	{
		if (impl_->tactical->server.worldActive())
		{
			impl_->fail(DedicatedCoopRuntimeError::CheckpointNotEligible);
			return false;
		}
	}
	if (!impl_->checkpointNow(context, true)) return false;
	// Campaign replication owns the admission epoch too, so end it before the
	// tactical coordinator tears down ingress/admission state.
	if (!impl_->endCampaignSync())
	{
		impl_->fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
		return false;
	}
	if (impl_->tactical != nullptr && impl_->tactical->server.active())
	{
		if (impl_->tactical->server.endEpoch() !=
			CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			impl_->fail(DedicatedCoopRuntimeError::TacticalSessionFailed);
			return false;
		}
		(void)impl_->tactical->server.takeTransportRestartRequired();
	}
	impl_->admissionConfigured = false;
	impl_->contentManifest = {};
	impl_->contentManifestCaptured = false;
	impl_->sessionEpoch = 0;
	return impl_->detachTacticalComposition();
}

bool DedicatedCoopRuntime::detachTacticalComposition() noexcept
{
	return !impl_ || impl_->detachTacticalComposition();
}

void DedicatedCoopRuntime::stopAdmissionTransport() noexcept
{
	if (!impl_) return;
	if (impl_->tactical != nullptr) impl_->tactical->listener.stop(100);
}

void DedicatedCoopRuntime::close() noexcept
{
	if (!impl_) return;
	(void)impl_->detachTacticalComposition();
	impl_->boot.close();
	impl_->prepared = false;
	impl_->campaignOpen = false;
	impl_->entryRequested = false;
	impl_->campaignEntered = false;
	impl_->admissionConfigured = false;
	impl_->sessionEpoch = 0;
	impl_->observedWorldGeneration = 0;
	impl_->observedRevision = 0;
	impl_->worldParticipantsSelected = false;
	impl_->worldParticipants = {};
	impl_->worldParticipantCount = 0;
	impl_->campaignIdentity = {};
	impl_->publishedAssignments = {};
	impl_->publishedAssignmentCount = 0;
	impl_->assignmentsPublished = false;
	impl_->worldDraining = false;
	impl_->postCombatReturnArmed = false;
	impl_->holdAdmissionAfterWorldDrain = false;
	impl_->minimumControllableActors = 0;
	impl_->starterMission = StarterMissionState::Unprepared;
}

bool DedicatedCoopRuntime::prepared() const noexcept
{
	return impl_ && impl_->prepared;
}

bool DedicatedCoopRuntime::campaignOpen() const noexcept
{
	return impl_ && impl_->campaignOpen;
}

bool DedicatedCoopRuntime::campaignEntered() const noexcept
{
	return impl_ && impl_->campaignEntered;
}

bool DedicatedCoopRuntime::admissionRunning() const noexcept
{
	return impl_ && impl_->tactical != nullptr &&
		impl_->tactical->listener.running();
}

bool DedicatedCoopRuntime::failed() const noexcept
{
	return !impl_ || impl_->fatal;
}

DedicatedCoopRuntimeError DedicatedCoopRuntime::error() const noexcept
{
	return impl_ ? impl_->error : DedicatedCoopRuntimeError::InvalidState;
}

DedicatedCoopCampaignEntry DedicatedCoopRuntime::entry() const noexcept
{
	return impl_ ? impl_->entry : DedicatedCoopCampaignEntry::None;
}

const std::filesystem::path& DedicatedCoopRuntime::profileDirectory() const noexcept
{
	static const std::filesystem::path empty;
	return impl_ ? impl_->boot.profileDirectory() : empty;
}

const DedicatedCampaignBootResult&
DedicatedCoopRuntime::campaignResult() const noexcept
{
	static const DedicatedCampaignBootResult unavailable{
		DedicatedCampaignBootError::InvalidState};
	return impl_ ? impl_->campaignResult : unavailable;
}

DedicatedCoopRuntime& GetDedicatedCoopRuntime() noexcept
{
	static DedicatedCoopRuntime runtime;
	return runtime;
}

bool IsDedicatedCoopProcess() noexcept
{
	const DedicatedServerOptions& options = GetDedicatedServerOptions();
	return options.enabled && options.mode == DedicatedServerMode::Coop;
}

const char* DedicatedCoopRuntimeErrorName(
	DedicatedCoopRuntimeError error) noexcept
{
	switch (error)
	{
		case DedicatedCoopRuntimeError::None: return "none";
		case DedicatedCoopRuntimeError::NotDedicatedCoop:
			return "not a dedicated co-op process";
		case DedicatedCoopRuntimeError::InvalidState: return "invalid state";
		case DedicatedCoopRuntimeError::CampaignPrepareFailed:
			return "campaign preparation failed";
		case DedicatedCoopRuntimeError::SimulationRandomInstallFailed:
			return "simulation random installation failed";
		case DedicatedCoopRuntimeError::CampaignSimulationUnavailable:
			return "campaign simulation unavailable";
		case DedicatedCoopRuntimeError::ContentManifestFailed:
			return "content manifest failed";
		case DedicatedCoopRuntimeError::CampaignOpenFailed:
			return "campaign open failed";
		case DedicatedCoopRuntimeError::CampaignEntryFailed:
			return "campaign entry failed";
		case DedicatedCoopRuntimeError::CheckpointNotEligible:
			return "checkpoint not eligible";
		case DedicatedCoopRuntimeError::CheckpointFailed:
			return "checkpoint failed";
		case DedicatedCoopRuntimeError::SessionEpochFailed:
			return "session epoch generation failed";
		case DedicatedCoopRuntimeError::AdmissionStartFailed:
			return "admission listener start failed";
		case DedicatedCoopRuntimeError::CampaignSyncFailed:
			return "campaign synchronization failed";
		case DedicatedCoopRuntimeError::MissionPrepareFailed:
			return "starter mission preparation failed";
		case DedicatedCoopRuntimeError::MissionLaunchFailed:
			return "starter mission launch failed";
		case DedicatedCoopRuntimeError::MissionReturnFailed:
			return "post-combat strategic return failed";
		case DedicatedCoopRuntimeError::MissionActorUnavailable:
			return "starter mission produced no controllable actor";
		case DedicatedCoopRuntimeError::TacticalCompositionFailed:
			return "tactical composition failed";
		case DedicatedCoopRuntimeError::TacticalSessionFailed:
			return "tactical session failed";
		case DedicatedCoopRuntimeError::TacticalReplicationFailed:
			return "tactical replication failed";
	}
	return "unknown dedicated co-op runtime error";
}
