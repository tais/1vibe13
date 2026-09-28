#include "DedicatedCoopArrival.h"
#include "DedicatedCoopTravel.h"
#include "DedicatedCoopAimHire.h"
#include "DedicatedCoopCampaignEconomy.h"
#include "DedicatedCoopCampaignAimQuotes.h"
#include "DedicatedCoopCampaignGroups.h"
#include "CoopCampaignStatus.h"
#include "CoopCampaignTimeAuthority.h"
#include "CoopCampaignActionAuthority.h"
#include "CoopCampaignHireAuthority.h"
#include "DedicatedCoopSurrender.h"
#include "DedicatedCoopBattleNotice.h"
#include "DedicatedCoopMeanwhile.h"
#include "DedicatedCoopRuntime.h"
#include "DedicatedCheckpointRuntimeEvidence.h"

#include "DedicatedContentManifest.h"
#include "DedicatedCampaignSaveBridge.h"
#include "DedicatedCoopMissionBootstrap.h"
#include "DedicatedCoopPostCombatCheckpointPolicy.h"
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
constexpr auto PostCombatCheckpointTimeout = std::chrono::minutes(2);

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
		DialogueQueueIsEmpty() && !gfWaitingForTriggerTimer &&
		!DedicatedCoopArrivalDecisionPending() && !DedicatedCoopSurrenderPending() &&
		!DedicatedCoopBattleNoticePending() && !DedicatedCoopMeanwhilePending()
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

	bool publishCampaignStatus() noexcept
	{
		static_assert(CoopSession::MaximumCampaignStatusReadyPeers == CoopSession::MaximumAuthorityPeers);
		// Read only at this committed main-thread boundary. No clock/event/GUI
		// mutation and no tactical ownership is inferred from time leadership.
		std::array<CoopSession::PeerIdentity, CoopSession::MaximumAuthorityPeers> ready{};
		const std::size_t count = tactical->server.campaignReadyPeers(ready);
		CoopSession::CoopCampaignStatus captured;
		captured.worldSeconds = GetWorldTotalSeconds();
		if (giTimeCompressMode < -1 || giTimeCompressMode > 5)
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		captured.compressionMode = static_cast<std::int8_t>(giTimeCompressMode);
		captured.gamePaused = GamePaused() != FALSE;
		captured.pauseLocked = PauseStateLocked() != FALSE;
		captured.compressionActive = IsTimeBeingCompressed() != FALSE;
		captured.timeInterrupted = gfTimeInterrupt != FALSE;
		captured.phase = starterMission == StarterMissionState::StrategicIdle
			? CoopSession::CoopCampaignPhase::Strategic
			: starterMission == StarterMissionState::Playable
				? CoopSession::CoopCampaignPhase::Tactical
				: (worldDraining || starterMission == StarterMissionState::ReturningToStrategic ||
				   starterMission == StarterMissionState::WaitingForStrategicCheckpoint)
					? CoopSession::CoopCampaignPhase::Transition : CoopSession::CoopCampaignPhase::Starting;
		if (const auto* offer = surrenderDecision.pending()) captured.surrenderOffer = offer->id;
		if (const auto* notice = battleNotice.pending())
		{
			if (battleNotice.acknowledged()) captured.phase = CoopSession::CoopCampaignPhase::Transition;
			else captured.battleNotice = {notice->id, static_cast<CoopSession::CoopCampaignBattleNoticeKind>(notice->kind),
				static_cast<std::uint8_t>(notice->sector.x), static_cast<std::uint8_t>(notice->sector.y), static_cast<std::uint8_t>(notice->sector.z), notice->sectorControlLost};
		}
		if (const auto* notice = meanwhile.pending())
		{
			static_assert(NUM_MEANWHILES == 17 && END_OF_PLAYERS_FIRST_BATTLE == 0 && BALIME_LIBERATED == 16,
				"review the native Meanwhile to co-op scene mapping");
			if (meanwhile.acknowledged()) captured.phase = CoopSession::CoopCampaignPhase::Transition;
			else captured.meanwhile = {notice->id, static_cast<CoopSession::CoopCampaignMeanwhileScene>(notice->scene + 1)};
		}
		if (!arrivalDecisions.captureObservation(captured.arrival) || !campaignStatusLedger.observe(captured, ready.data(), count))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		CoopSession::CoopCampaignStatusBytes bytes;
		if (!CoopSession::EncodeCoopCampaignStatus(campaignStatusLedger.value(), bytes))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		for (auto& delivery : campaignStatusDeliveries)
			if (std::find(ready.begin(), ready.begin() + count, delivery.peer) == ready.begin() + count)
				delivery = {};
		for (std::size_t index = 0; index < count; ++index)
		{
			CoopSession::TransportPeer transport;
			if (!tactical->listener.authenticatedTransportForPeer(ready[index], transport)) continue;
			auto found = std::find_if(campaignStatusDeliveries.begin(), campaignStatusDeliveries.end(),
				[&](const CampaignStatusDelivery& delivery) { return delivery.peer == ready[index]; });
			if (found == campaignStatusDeliveries.end())
				found = std::find_if(campaignStatusDeliveries.begin(), campaignStatusDeliveries.end(),
					[](const CampaignStatusDelivery& delivery) { return CoopSession::IsZero(delivery.peer); });
			if (found == campaignStatusDeliveries.end())
			{
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
				return false;
			}
			if (found->peer == ready[index] && found->transport == transport &&
				found->revision == campaignStatusLedger.value().revision) continue;
			// The tiny full replacement coalesces under backpressure. Failed sends
			// never advance a delivery cursor; a new transport must receive it anew.
			if (tactical->listener.sendToPeer(ready[index], CoopSession::CoopCampaignStatusMessageName,
				bytes.data(), bytes.size()))
				*found = {ready[index], transport, campaignStatusLedger.value().revision};
		}
		return true;
	}

	bool publishCampaignGroups() noexcept
	{
		CoopSession::CoopCampaignGroups captured;
		const char* diagnostic = nullptr;
		if (!worldDraining && (starterMission == StarterMissionState::Playable || starterMission == StarterMissionState::StrategicIdle))
			diagnostic = CaptureDedicatedCoopCampaignGroups(captured);
		if (diagnostic)
		{
			// Read-only group observation must not crash the campaign or show a
			// truncated roster. Log once per changed fault and publish unavailable.
			captured = {};
			if (!lastCampaignGroupsDiagnostic || std::strcmp(lastCampaignGroupsDiagnostic, diagnostic) != 0)
				std::fprintf(stderr, "[dedicated] campaign groups unavailable: %s\n", diagnostic);
		}
		lastCampaignGroupsDiagnostic = diagnostic;
		if (!campaignGroupsLedger.observe(captured))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		std::array<CoopSession::PeerIdentity, CoopSession::MaximumAuthorityPeers> ready{};
		const std::size_t count = tactical->server.campaignReadyPeers(ready);
		for (auto& delivery : campaignGroupsDeliveries)
			if (std::find(ready.begin(), ready.begin() + count, delivery.peer) == ready.begin() + count) delivery = {};
		CoopSession::CoopCampaignGroupsBytes bytes;
		std::size_t size = 0;
		if (!CoopSession::EncodeCoopCampaignGroups(campaignGroupsLedger.value(), bytes, size))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		for (std::size_t index = 0; index < count; ++index)
		{
			CoopSession::TransportPeer transport;
			if (!tactical->listener.authenticatedTransportForPeer(ready[index], transport)) continue;
			auto found = std::find_if(campaignGroupsDeliveries.begin(), campaignGroupsDeliveries.end(),
				[&](const CampaignStatusDelivery& delivery) { return delivery.peer == ready[index]; });
			if (found == campaignGroupsDeliveries.end())
				found = std::find_if(campaignGroupsDeliveries.begin(), campaignGroupsDeliveries.end(),
					[](const CampaignStatusDelivery& delivery) { return CoopSession::IsZero(delivery.peer); });
			if (found == campaignGroupsDeliveries.end())
			{
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
				return false;
			}
			if (found->peer == ready[index] && found->transport == transport && found->revision == campaignGroupsLedger.value().revision) continue;
			// Latest full replacement coalesces under backpressure. A reconnect
			// gets a fresh copy even if no group moved since its previous transport.
			if (tactical->listener.sendToPeer(ready[index], CoopSession::CoopCampaignGroupsMessageName, bytes.data(), size))
				*found = {ready[index], transport, campaignGroupsLedger.value().revision};
		}
		return true;
	}

	template<class DeliveryList>
	bool queueCampaignEconomicObservation(const char* message, const std::uint8_t* bytes,
		std::size_t size, std::uint64_t revision, DeliveryList& deliveries,
		const DeliveryList* prerequisite = nullptr, std::uint64_t prerequisiteRevision = 0) noexcept
	{
		std::array<CoopSession::PeerIdentity, CoopSession::MaximumAuthorityPeers> ready{};
		const auto count = tactical->server.campaignReadyPeers(ready);
		for (auto& delivery : deliveries)
			if (std::find(ready.begin(), ready.begin() + count, delivery.peer) == ready.begin() + count) delivery = {};
		for (std::size_t i = 0; i < count; ++i)
		{
			CoopSession::TransportPeer transport;
			if (!tactical->listener.authenticatedTransportForPeer(ready[i], transport)) continue;
			if (prerequisite && !std::any_of(prerequisite->begin(), prerequisite->end(), [&](const auto& sent) {
				return sent.peer == ready[i] && sent.transport == transport && sent.revision == prerequisiteRevision;
			})) continue;
			auto found = std::find_if(deliveries.begin(), deliveries.end(), [&](const auto& sent) { return sent.peer == ready[i]; });
			if (found == deliveries.end()) found = std::find_if(deliveries.begin(), deliveries.end(),
				[](const auto& sent) { return CoopSession::IsZero(sent.peer); });
			if (found == deliveries.end())
			{
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
				return false;
			}
			if (found->peer == ready[i] && found->transport == transport && found->revision == revision) continue;
			if (tactical->listener.sendToPeer(ready[i], message, bytes, size)) *found = {ready[i], transport, revision};
		}
		return true;
	}

	bool publishCampaignEconomyAndQuotes() noexcept
	{
		CoopSession::CoopCampaignEconomy economy;
		const char* diagnostic = nullptr;
		if (!worldDraining && (starterMission == StarterMissionState::Playable || starterMission == StarterMissionState::StrategicIdle))
			diagnostic = CaptureDedicatedCoopCampaignEconomy(economy);
		if (diagnostic)
		{
			economy = {};
			if (!lastCampaignEconomyDiagnostic || std::strcmp(lastCampaignEconomyDiagnostic, diagnostic) != 0)
				std::fprintf(stderr, "[dedicated] campaign economy unavailable: %s\n", diagnostic);
		}
		lastCampaignEconomyDiagnostic = diagnostic;
		if (!campaignEconomyLedger.observe(economy))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		CoopSession::CoopCampaignAimQuotes quotes;
		diagnostic = nullptr;
		if (campaignEconomyLedger.value().available && worldlessStrategicTimeControl() && !campaignStatusLedger.value().arrival.decision)
			diagnostic = CaptureDedicatedCoopCampaignAimQuotes(campaignEconomyLedger.value(), quotes);
		if (diagnostic)
		{
			quotes = {};
			if (!lastCampaignQuotesDiagnostic || std::strcmp(lastCampaignQuotesDiagnostic, diagnostic) != 0)
				std::fprintf(stderr, "[dedicated] AIM quotes unavailable: %s\n", diagnostic);
		}
		lastCampaignQuotesDiagnostic = diagnostic;
		quotes.economyRevision = campaignEconomyLedger.value().revision;
		if (!campaignQuotesLedger.observe(quotes))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		CoopSession::CoopCampaignEconomyBytes economyBytes;
		CoopSession::CoopCampaignAimQuotesBytes quoteBytes;
		std::size_t economySize = 0, quoteSize = 0;
		if (!CoopSession::EncodeCoopCampaignEconomy(campaignEconomyLedger.value(), economyBytes, economySize) ||
			!CoopSession::EncodeCoopCampaignAimQuotes(campaignQuotesLedger.value(), quoteBytes, quoteSize))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		// Quotes bind to an exact economy. Backpressure must never queue them
		// ahead of the balance/roster replacement they reference.
		return queueCampaignEconomicObservation(CoopSession::CoopCampaignEconomyMessageName,
			economyBytes.data(), economySize, campaignEconomyLedger.value().revision, campaignEconomyDeliveries) &&
			queueCampaignEconomicObservation(CoopSession::CoopCampaignAimQuotesMessageName,
				quoteBytes.data(), quoteSize, campaignQuotesLedger.value().revision, campaignQuotesDeliveries,
				&campaignEconomyDeliveries, campaignEconomyLedger.value().revision);
	}

	bool campaignEconomicObservationsDelivered(const CoopSession::PeerIdentity& peer,
		CoopSession::TransportPeer transport) const noexcept
	{
		const auto sent = [&](const auto& deliveries, std::uint64_t revision) {
			return std::any_of(deliveries.begin(), deliveries.end(), [&](const auto& delivery) {
				return delivery.peer == peer && delivery.transport == transport && delivery.revision == revision;
			});
		};
		return sent(campaignEconomyDeliveries, campaignEconomyLedger.value().revision) &&
			sent(campaignQuotesDeliveries, campaignQuotesLedger.value().revision);
	}

	bool worldlessStrategicTimeControl() const noexcept
	{
		return starterMission == StarterMissionState::StrategicIdle && !worldDraining &&
			!tactical->server.worldActive() && IsDedicatedCoopStarterMissionMapReady();
	}

	bool campaignTimePeerReady(const CoopSession::PeerIdentity& peer) const noexcept
	{
		std::array<CoopSession::PeerIdentity, CoopSession::MaximumAuthorityPeers> gated{}, current{};
		const auto gatedCount = tactical->server.campaignReadyPeers(gated);
		const auto currentCount = tactical->campaignSync->readyPeers(current);
		return std::find(gated.begin(), gated.begin() + gatedCount, peer) != gated.begin() + gatedCount &&
			std::find(current.begin(), current.begin() + currentCount, peer) != current.begin() + currentCount;
	}

	void pauseStrategicTimeWithoutLeader() noexcept
	{
		if (!worldlessStrategicTimeControl()) return;
		const auto& leader = campaignStatusLedger.value().timeLeader;
		if (CoopSession::IsZero(leader) || !campaignTimePeerReady(leader))
			(void)TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_X0);
	}

	bool reconcileCampaignTimePeers() noexcept
	{
		std::array<CoopSession::FullEngineCoopAuthenticatedPeer, CoopSession::MaximumAuthorityPeers> authenticated{};
		std::array<CoopSession::CoopCampaignTimeAuthority::Peer, CoopSession::MaximumAuthorityPeers> peers{};
		const auto count = tactical->listener.authenticatedPeers(authenticated);
		for (std::size_t i = 0; i < count; ++i)
			peers[i] = {authenticated[i].peerIdentity, authenticated[i].transport};
		if (!campaignTimeAuthority.reconcile(peers.data(), count))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		return true;
	}

	void handleCampaignTimeRequest(const CoopSession::FullEngineCoopCampaignInboundMessage& message) noexcept
	{
		CoopSession::TransportPeer current;
		CoopSession::CoopCampaignTimeRequest request;
		if (!tactical->listener.authenticatedTransportForPeer(message.peerIdentity, current) || current != message.transport ||
			!CoopSession::DecodeCoopCampaignTimeRequest(message.bytes.data(), message.size, request)) return;
		(void)campaignTimeAuthority.submit(request, {message.peerIdentity, current},
			campaignTimePeerReady(message.peerIdentity), worldlessStrategicTimeControl(), campaignStatusLedger,
			[](CoopSession::CoopCampaignTimeAction action) {
				static_assert(static_cast<UINT32>(CoopSession::CoopCampaignTimeAction::FiveMinutes) == TIME_COMPRESS_5MINS &&
					static_cast<UINT32>(CoopSession::CoopCampaignTimeAction::ThirtyMinutes) == TIME_COMPRESS_30MINS &&
					static_cast<UINT32>(CoopSession::CoopCampaignTimeAction::SixtyMinutes) == TIME_COMPRESS_60MINS);
				const UINT32 mode = action == CoopSession::CoopCampaignTimeAction::Pause ? TIME_COMPRESS_X0 : static_cast<UINT32>(action);
				return TrySetWorldlessStrategicTimeCompression(mode) != FALSE;
			});
	}

	bool flushCampaignTimeResults() noexcept
	{
		const auto& deliveries = campaignTimeAuthority.deliveries();
		for (std::size_t i = 0; i < deliveries.size(); ++i)
		{
			const auto& delivery = deliveries[i];
			if (!delivery.pending) continue;
			CoopSession::TransportPeer current;
			if (!tactical->listener.authenticatedTransportForPeer(delivery.peer.identity, current) || current != delivery.peer.transport) continue;
			// Publish the post-command clock before its terminal result. A queued
			// status send is sufficient on this ordered reliable transport.
			const auto status = std::find_if(campaignStatusDeliveries.begin(), campaignStatusDeliveries.end(),
				[&](const CampaignStatusDelivery& sent) { return sent.peer == delivery.peer.identity &&
					sent.transport == current && sent.revision == campaignStatusLedger.value().revision; });
			if (status == campaignStatusDeliveries.end() || !campaignEconomicObservationsDelivered(delivery.peer.identity, current)) continue;
			CoopSession::CoopCampaignTimeResultBytes bytes;
			if (!CoopSession::EncodeCoopCampaignTimeResult(delivery.result, bytes))
			{
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
				return false;
			}
			if (tactical->listener.sendToPeer(delivery.peer.identity, CoopSession::CoopCampaignTimeResultMessageName, bytes.data(), bytes.size()))
				campaignTimeAuthority.delivered(i);
		}
		return true;
	}

	bool reconcileCampaignActionPeers() noexcept
	{
		std::array<CoopSession::FullEngineCoopAuthenticatedPeer, CoopSession::MaximumAuthorityPeers> authenticated{};
		std::array<CoopSession::CoopCampaignActionAuthority::Peer, CoopSession::MaximumAuthorityPeers> peers{};
		const auto count = tactical->listener.authenticatedPeers(authenticated);
		for (std::size_t i = 0; i < count; ++i) peers[i] = {authenticated[i].peerIdentity, authenticated[i].transport};
		if (campaignActionAuthority.reconcile(peers.data(), count)) return true;
		fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
		return false;
	}

	CoopSession::CoopCampaignActionNativeResult applyCampaignAction(
		const CoopSession::CoopCampaignActionRequest& request, DedicatedCoopRuntime& runtime) noexcept
	{
		using Action = CoopSession::CoopCampaignAction;
		using Outcome = CoopSession::CoopCampaignActionOutcome;
		if (request.action == Action::SkipMeanwhile)
		{
			if (selfRetirementActive) return {Outcome::NativeRejected, 0};
			const auto result = AcknowledgeDedicatedCoopMeanwhile(request.decision);
			using Code = DedicatedCoopMeanwhileResult;
			return {result == Code::Applied ? Outcome::Applied : result == Code::Failed ? Outcome::Failed : Outcome::NativeRejected,
				static_cast<std::uint16_t>(result)};
		}
		if (request.action == Action::AcknowledgeBattleNotice)
		{
			if (selfRetirementActive) return {Outcome::NativeRejected, 0};
			const auto result = AcknowledgeDedicatedCoopBattleNotice(request.decision);
			using Code = DedicatedCoopBattleNoticeResult;
			return {result == Code::Applied ? Outcome::Applied : result == Code::Failed ? Outcome::Failed : Outcome::NativeRejected,
				static_cast<std::uint16_t>(result)};
		}
		if (request.action == Action::DeclineSurrender || request.action == Action::AcceptSurrender)
		{
			if (selfRetirementActive) return {Outcome::NativeRejected, 0};
			const auto result = ReplyToDedicatedCoopSurrender(request.decision,
				request.action == Action::AcceptSurrender ? DedicatedCoopSurrenderReply::Surrender : DedicatedCoopSurrenderReply::ContinueFighting);
			using Code = DedicatedCoopSurrenderResult;
			return {result == Code::Applied ? Outcome::Applied : result == Code::Failed ? Outcome::Failed : Outcome::NativeRejected,
				static_cast<std::uint16_t>(result)};
		}
		if (!worldlessStrategicTimeControl() || selfRetirementActive) return {Outcome::NativeRejected, 0};
		switch (request.action)
		{
			case Action::Travel:
			{
				const auto result = StartDedicatedCoopTravel(request.group, request.destinationX, request.destinationY);
				using Code = DedicatedCoopTravelStartCode;
				return {result.code == Code::Started ? Outcome::Applied :
					result.code == Code::AdjacentSectorRequired ? Outcome::Unsupported : Outcome::NativeRejected,
					static_cast<std::uint16_t>(result.code)};
			}
			case Action::AcknowledgeArrival:
			case Action::StopArrival:
			{
				const auto result = ReplyToDedicatedCoopArrival(request.decision,
					request.action == Action::AcknowledgeArrival ? DedicatedCoopArrivalReply::Acknowledge : DedicatedCoopArrivalReply::Stop);
				using Code = DedicatedCoopArrivalReplyResult;
				return {result == Code::Applied ? Outcome::Applied :
					result == Code::UnsupportedDecision ? Outcome::Unsupported : Outcome::NativeRejected,
					static_cast<std::uint16_t>(result)};
			}
			case Action::EnterArrivalForced:
			case Action::EnterArrivalSpread:
			{
				const auto result = runtime.enterArrivalBattle(request.decision,
					request.action == Action::EnterArrivalForced ? NativePreBattleDeployment::Forced : NativePreBattleDeployment::Spread);
				using Code = DedicatedCoopArrivalEnterResult;
				return {result == Code::Entered ? Outcome::Applied : result == Code::Failed ? Outcome::Failed :
					result == Code::UnsupportedDecision ? Outcome::Unsupported : Outcome::NativeRejected,
					static_cast<std::uint16_t>(result)};
			}
			case Action::RetreatArrival:
			{
				const auto result = runtime.retreatArrivalBattle(request.decision);
				using Code = DedicatedCoopArrivalRetreatResult;
				return {result == Code::Retreated ? Outcome::Applied : result == Code::Failed ? Outcome::Failed :
					(result == Code::UnsupportedDecision || result == Code::AutoResolveRequired) ? Outcome::Unsupported : Outcome::NativeRejected,
					static_cast<std::uint16_t>(result)};
			}
		}
		return {Outcome::Failed, 0};
	}

	bool handleCampaignActionRequest(const CoopSession::FullEngineCoopCampaignInboundMessage& message,
		DedicatedCoopRuntime& runtime) noexcept
	{
		CoopSession::TransportPeer current;
		CoopSession::CoopCampaignActionRequest request;
		if (!tactical->listener.authenticatedTransportForPeer(message.peerIdentity, current) || current != message.transport ||
			!CoopSession::DecodeCoopCampaignActionRequest(message.bytes.data(), message.size, request)) return true;
		// Either admitted player may request a shared campaign action. Readiness,
		// exact observed state and native legality remain independent requirements.
		(void)campaignActionAuthority.submit(request, {message.peerIdentity, current},
			campaignTimePeerReady(message.peerIdentity), true, worldlessStrategicTimeControl(),
			campaignStatusLedger, campaignGroupsLedger.value(),
			[&](const CoopSession::CoopCampaignActionRequest& accepted) { return applyCampaignAction(accepted, runtime); });
		if (fatal) return false;
		if (campaignActionAuthority.failed())
		{
			fail(DedicatedCoopRuntimeError::InvalidState);
			return false;
		}
		// Recapture before another peer's queued request can validate against the
		// old group/arrival state, and before the receipt can release client input.
		return publishCampaignStatus() && publishCampaignGroups() && publishCampaignEconomyAndQuotes();
	}

	bool flushCampaignActionResults() noexcept
	{
		const auto& deliveries = campaignActionAuthority.deliveries();
		for (std::size_t i = 0; i < deliveries.size(); ++i)
		{
			const auto& delivery = deliveries[i];
			if (!delivery.pending) continue;
			CoopSession::TransportPeer current;
			if (!tactical->listener.authenticatedTransportForPeer(delivery.peer.identity, current) || current != delivery.peer.transport ||
				!campaignTimePeerReady(delivery.peer.identity)) continue;
			const auto sent = [&](const auto& observations, std::uint64_t revision) {
				return std::any_of(observations.begin(), observations.end(), [&](const CampaignStatusDelivery& value) {
					return value.peer == delivery.peer.identity && value.transport == current && value.revision == revision;
				});
			};
			if (!sent(campaignStatusDeliveries, campaignStatusLedger.value().revision) ||
				!sent(campaignGroupsDeliveries, campaignGroupsLedger.value().revision) ||
				!campaignEconomicObservationsDelivered(delivery.peer.identity, current)) continue;
			CoopSession::CoopCampaignActionResultBytes bytes;
			if (!CoopSession::EncodeCoopCampaignActionResult(delivery.result, bytes))
			{
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
				return false;
			}
			if (tactical->listener.sendToPeer(delivery.peer.identity, CoopSession::CoopCampaignActionResultMessageName, bytes.data(), bytes.size()))
				campaignActionAuthority.delivered(i);
		}
		return true;
	}

	bool reconcileCampaignHirePeers() noexcept
	{
		std::array<CoopSession::FullEngineCoopAuthenticatedPeer, CoopSession::MaximumAuthorityPeers> authenticated{};
		std::array<CoopSession::CoopCampaignHireAuthority::Peer, CoopSession::MaximumAuthorityPeers> peers{};
		const auto count = tactical->listener.authenticatedPeers(authenticated);
		for (std::size_t i = 0; i < count; ++i) peers[i] = {authenticated[i].peerIdentity, authenticated[i].transport};
		if (campaignHireAuthority.reconcile(peers.data(), count)) return true;
		fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
		return false;
	}

	bool handleCampaignHireRequest(const CoopSession::FullEngineCoopCampaignInboundMessage& message) noexcept
	{
		CoopSession::TransportPeer current;
		CoopSession::CoopCampaignHireRequest request;
		if (!tactical->listener.authenticatedTransportForPeer(message.peerIdentity, current) || current != message.transport ||
			!CoopSession::DecodeCoopCampaignHireRequest(message.bytes.data(), message.size, request)) return true;
		(void)campaignHireAuthority.submit(request, {message.peerIdentity, current}, campaignTimePeerReady(message.peerIdentity),
			true, worldlessStrategicTimeControl() && !selfRetirementActive, campaignStatusLedger,
			campaignEconomyLedger.value(), campaignQuotesLedger.value(), [&](const auto& accepted) {
				using Outcome = CoopSession::CoopCampaignHireOutcome;
				const auto hired = HireDedicatedCoopAimMerc(accepted, campaignEconomyLedger.value(), campaignQuotesLedger.value());
				CoopSession::CoopCampaignHireNativeResult result;
				result.nativeDetail = static_cast<std::uint16_t>((static_cast<unsigned>(hired.code) << 8) | hired.nativeDetail);
				if (hired.code == DedicatedCoopAimHireCode::Applied)
				{
					result.outcome = Outcome::Applied; result.actor = hired.actor; result.chargedTotal = hired.chargedTotal;
				}
				else
				{
					result.outcome = hired.mutationMayHaveStarted || hired.code == DedicatedCoopAimHireCode::NativeFailure
						? Outcome::Failed : Outcome::NativeRejected;
					std::fprintf(stderr, "[dedicated] AIM hire rejected: stage=%u detail=%u mutation=%u\n",
						static_cast<unsigned>(hired.code), hired.nativeDetail, hired.mutationMayHaveStarted ? 1 : 0);
				}
				return result;
			});
		if (campaignHireAuthority.failed())
		{
			fail(DedicatedCoopRuntimeError::InvalidState);
			return false;
		}
		// A second queued player request sees the new roster, balance, offers and
		// shared control barrier. Every receipt waits for those observations.
		return publishCampaignStatus() && publishCampaignGroups() && publishCampaignEconomyAndQuotes();
	}

	bool flushCampaignHireResults() noexcept
	{
		const auto& deliveries = campaignHireAuthority.deliveries();
		for (std::size_t i = 0; i < deliveries.size(); ++i)
		{
			const auto& delivery = deliveries[i];
			if (!delivery.pending) continue;
			CoopSession::TransportPeer current;
			if (!tactical->listener.authenticatedTransportForPeer(delivery.peer.identity, current) || current != delivery.peer.transport ||
				!campaignTimePeerReady(delivery.peer.identity) || !campaignEconomicObservationsDelivered(delivery.peer.identity, current)) continue;
			const auto sent = [&](const auto& observations, std::uint64_t revision) {
				return std::any_of(observations.begin(), observations.end(), [&](const auto& value) {
					return value.peer == delivery.peer.identity && value.transport == current && value.revision == revision;
				});
			};
			if (!sent(campaignStatusDeliveries, campaignStatusLedger.value().revision) ||
				!sent(campaignGroupsDeliveries, campaignGroupsLedger.value().revision)) continue;
			CoopSession::CoopCampaignHireResultBytes bytes;
			if (!CoopSession::EncodeCoopCampaignHireResult(delivery.result, bytes))
			{
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
				return false;
			}
			if (tactical->listener.sendToPeer(delivery.peer.identity, CoopSession::CoopCampaignHireResultMessageName, bytes.data(), bytes.size()))
				campaignHireAuthority.delivered(i);
		}
		return true;
	}

	bool pumpCampaignInboundAndOutbound(DedicatedCoopRuntime& runtime) noexcept
	{
		if (tactical == nullptr || tactical->campaignSync == nullptr)
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed);
			return false;
		}
		CoopSession::FullEngineCoopCampaignInboundMessage message;
		while (tactical->listener.popCampaignInbound(message))
		{
			if (message.kind == CoopSession::FullEngineCoopCampaignInboundKind::HireRequest)
			{
				if (!handleCampaignHireRequest(message)) return false;
				continue;
			}
			if (message.kind == CoopSession::FullEngineCoopCampaignInboundKind::ActionRequest)
			{
				if (!handleCampaignActionRequest(message, runtime)) return false;
				continue;
			}
			if (message.kind == CoopSession::FullEngineCoopCampaignInboundKind::TimeRequest)
			{
				handleCampaignTimeRequest(message);
				if (!publishCampaignStatus() || !publishCampaignGroups() || !publishCampaignEconomyAndQuotes()) return false;
				continue;
			}
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
			if (CoopSession::IsFatalCoopCampaignSyncInboundResult(handled))
			{
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
		if (GetGameContext().campaignSimulation().failed())
		{
			fail(DedicatedCoopRuntimeError::InvalidState);
			return false;
		}
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
			campaignStatusLedger.clear();
			campaignStatusDeliveries = {};
			campaignGroupsLedger.clear();
			campaignGroupsDeliveries = {};
			lastCampaignGroupsDiagnostic = nullptr;
			campaignTimeAuthority.clear();
			campaignActionAuthority.clear();
			campaignHireAuthority.clear();
			campaignEconomyLedger.clear(); campaignQuotesLedger.clear();
			campaignEconomyDeliveries = {}; campaignQuotesDeliveries = {};
			lastCampaignEconomyDiagnostic = lastCampaignQuotesDiagnostic = nullptr;
			if (!campaignStatusLedger.beginSession(epoch)) return false;
			if (!campaignGroupsLedger.beginSession(epoch)) return false;
			if (!campaignEconomyLedger.beginSession(epoch) || !campaignQuotesLedger.beginSession(epoch)) return false;
			admissionConfigured = true;
		}

		CoopSession::FullEngineCoopAdmissionListenerConfiguration configuration;
		configuration.enableCampaignRequests = true;
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
		if (context.campaignSimulation().failed())
		{
			fail(DedicatedCoopRuntimeError::InvalidState);
			return false;
		}
		// Cold checkpoint supersession currently requires disconnecting every
		// client and reloading its campaign. An optional timer must not interrupt
		// admission, campaign input or an outstanding receipt. Defer it while any
		// transport is attached, including peers still joining or syncing.
		// Required shutdown/victory checkpoints retain their explicit drain path.
		if (!required && tactical != nullptr && tactical->listener.hasConnections())
		{
			lastEligibility = DedicatedCheckpointEligibilityReason::NetworkQueueNotDrained;
			return false;
		}
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
			postCombatCheckpointDeadline = Clock::now() + PostCombatCheckpointTimeout;
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

	bool pumpTactical(GameContext& context, DedicatedCoopRuntime& runtime) noexcept
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
		if (!reconcileCampaignPeersAndGateTactical()) return false;
		pauseStrategicTimeWithoutLeader();
		if (!publishCampaignStatus() || !publishCampaignGroups() || !publishCampaignEconomyAndQuotes() || !reconcileCampaignTimePeers() ||
			!reconcileCampaignActionPeers() || !reconcileCampaignHirePeers() || !pumpCampaignInboundAndOutbound(runtime))
			return false;
		// A resync in this FIFO can remove readiness too. Never auto-resume on
		// reconnect or overwrite a native event pause on a later frame.
		pauseStrategicTimeWithoutLeader();
		if (!publishCampaignStatus() || !publishCampaignGroups() || !publishCampaignEconomyAndQuotes() ||
			!flushCampaignTimeResults() || !flushCampaignActionResults() || !flushCampaignHireResults())
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
	DedicatedCoopArrivalState arrivalDecisions;
	DedicatedCoopSurrenderState surrenderDecision;
	DedicatedCoopBattleNoticeState battleNotice;
	DedicatedCoopMeanwhileState meanwhile;
	std::uint64_t lastArrivalDecisionLogged = 0;
	using CampaignStatusDelivery = DedicatedCampaignObservationDelivery;
	CoopSession::CoopCampaignStatusLedger campaignStatusLedger;
	CoopSession::CoopCampaignGroupsLedger campaignGroupsLedger;
	const char* lastCampaignGroupsDiagnostic = nullptr;
	std::array<CampaignStatusDelivery, CoopSession::MaximumAuthorityPeers> campaignGroupsDeliveries{};
	CoopSession::CoopCampaignTimeAuthority campaignTimeAuthority;
	CoopSession::CoopCampaignActionAuthority campaignActionAuthority;
	CoopSession::CoopCampaignHireAuthority campaignHireAuthority;
	CoopSession::CoopCampaignEconomyLedger campaignEconomyLedger;
	CoopSession::CoopCampaignAimQuotesLedger campaignQuotesLedger;
	const char* lastCampaignEconomyDiagnostic = nullptr;
	const char* lastCampaignQuotesDiagnostic = nullptr;
	std::array<CampaignStatusDelivery, CoopSession::MaximumAuthorityPeers> campaignEconomyDeliveries{}, campaignQuotesDeliveries{};
	std::array<CampaignStatusDelivery, CoopSession::MaximumAuthorityPeers> campaignStatusDeliveries{};
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
	Clock::time_point postCombatCheckpointDeadline{};
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

DedicatedCheckpointRuntimeEvidence
DedicatedCoopRuntime::captureCheckpointRuntimeEvidence(
	const GameContext& context) const noexcept
{
	DedicatedCheckpointRuntimeEvidence result;
	result.frame.observed = true;
	result.frame.frame = context.frameDriver().captureBoundaryState();
	result.frame.tick = context.runtime().simulationTicks().captureBoundaryState();
	result.frame.campaignSimulationFailed = context.campaignSimulation().failed();
	result.packages.observed = true;
	result.packages.messages = context.runtimeMessages().observation();
	const TacticalCommandInboxSummary inbox = GetJa2TacticalCommandService().summary();
	const Ja2TacticalCommandHostDiagnostics commands = GetJa2TacticalCommandHostDiagnostics();
	result.localCommands.nativeObserved = true;
	result.localCommands.commandInbox = inbox.pending;
	result.localCommands.pendingHostReceipts = commands.pendingReceipts;
	result.localCommands.pendingDeferredCancellations = commands.pendingDeferredCancellations;
	result.localCommands.trackedCommands = commands.trackedCommands;
	if (impl_ == nullptr) return result;
	result.campaign.runtimeObserved = true;
	result.campaign.selfRetirementActive = impl_->selfRetirementActive;
	result.campaign.worldDraining = impl_->worldDraining;
	result.campaign.postCombatReturnArmed = impl_->postCombatReturnArmed;
	result.campaign.holdAdmissionAfterWorldDrain = impl_->holdAdmissionAfterWorldDrain;
	result.campaign.runtimeFailed = impl_->fatal;
	CaptureDedicatedCampaignResultEvidence(impl_->campaignTimeAuthority,
		impl_->campaignActionAuthority, impl_->campaignHireAuthority, result.campaign);
	if (impl_->tactical == nullptr) return result;
	const TacticalComposition& tactical = *impl_->tactical;
	result.localCommands.hostObserved = true;
	result.localCommands.hostCorrelations = tactical.host.correlationCount();
	result.localCommands.pendingImmediateReceipts = tactical.host.pendingImmediateReceiptCount();
	result.tactical.observed = true;
	result.tactical.contextMatchesComposition = impl_->tacticalContext == &context;
	result.tactical.state = tactical.server.observation();
	result.transport.observed = true;
	result.transport.listener = tactical.listener.observation();
	if (tactical.campaignSync != nullptr)
	{
		result.campaign.syncObserved = true;
		result.campaign.sync = tactical.campaignSync->diagnostics();
	}
	CaptureDedicatedCampaignObservationEvidence(tactical.server, tactical.listener,
		impl_->campaignStatusDeliveries, impl_->campaignStatusLedger.value().revision,
		impl_->campaignGroupsDeliveries, impl_->campaignGroupsLedger.value().revision,
		impl_->campaignEconomyDeliveries, impl_->campaignEconomyLedger.value().revision,
		impl_->campaignQuotesDeliveries, impl_->campaignQuotesLedger.value().revision,
		result.campaign);
	return result;
}

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
	if (impl_ && context.campaignSimulation().failed())
	{
		impl_->fail(DedicatedCoopRuntimeError::InvalidState);
		return;
	}
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
					DedicatedCoopStarterCampaignState::EstablishedCold &&
				starterState !=
					DedicatedCoopStarterCampaignState::EstablishedStrategicCold) ||
			impl_->entry != DedicatedCoopCampaignEntry::Resume)
		{
			std::fprintf(stderr,
				"[dedicated] starter campaign state rejected: %s\n",
				DedicatedCoopStarterCampaignStateName(starterState));
			impl_->fail(DedicatedCoopRuntimeError::MissionPrepareFailed);
			return;
		}
		// Established ownership must not depend on DidGameJustStart,
		// which can change while the initial helicopter's events are still pending.
		if (starterState == DedicatedCoopStarterCampaignState::EstablishedCold ||
			starterState == DedicatedCoopStarterCampaignState::EstablishedStrategicCold)
			impl_->arrivalDecisions.enableEstablishedAimArrivals();
		// The complete roster and its pending arrival events become durable before
		// admission or campaign transfer can expose this campaign to any peer. An
		// exact prepared resume already is that artifact and is left byte-for-byte
		// unchanged here.
		if (rosterCheckpointRequired && !impl_->checkpointNow(context, true))
		{
			return;
		}
		if (starterState ==
			DedicatedCoopStarterCampaignState::EstablishedStrategicCold)
		{
			// A loaded checkpoint can retain an active compression rate. A
			// traveling or wounded roster awaits an explicit time-leader request;
			// reconnecting must not advance arrivals or other campaign events.
			StopTimeCompression();
			PauseGame();
		}
		impl_->starterMission = starterState ==
			DedicatedCoopStarterCampaignState::EstablishedStrategicCold
			? StarterMissionState::StrategicIdle
			: (starterState == DedicatedCoopStarterCampaignState::EstablishedCold
				? StarterMissionState::WaitingForEstablishedCampaignReadyPeer
				: StarterMissionState::WaitingForCampaignReadyPeer);
		impl_->minimumControllableActors = 0;
		impl_->starterPeerGatherDeadline = {};
		if (!BindDedicatedCoopArrivalState(impl_->arrivalDecisions) || !BindDedicatedCoopSurrenderState(impl_->surrenderDecision) || !BindDedicatedCoopBattleNoticeState(impl_->battleNotice) || !BindDedicatedCoopMeanwhileState(impl_->meanwhile))
		{
			impl_->fail(DedicatedCoopRuntimeError::InvalidState);
			return;
		}
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
	if (const char* failure = impl_->meanwhile.failure())
	{
		impl_->fail(DedicatedCoopRuntimeError::InvalidState);
		return;
	}
	if (const char* failure = impl_->battleNotice.failure())
	{
		impl_->fail(DedicatedCoopRuntimeError::InvalidState);
		return;
	}
	if (const char* failure = impl_->surrenderDecision.failure())
	{
		impl_->fail(DedicatedCoopRuntimeError::InvalidState);
		return;
	}
	if (const char* failure = impl_->arrivalDecisions.failure())
	{
		impl_->fail(DedicatedCoopRuntimeError::InvalidState);
		return;
	}
	if (const auto* decision = impl_->arrivalDecisions.front())
	{
		StopTimeCompression(); PauseGame();
		if (decision->id != impl_->lastArrivalDecisionLogged)
		{
			if (decision->kind == DedicatedCoopArrivalKind::Battle)
			{
				NativePreBattlePreparation preparation;
				const auto result = PrepareDedicatedCoopArrivalBattle(decision->id, preparation);
				if (result == DedicatedCoopArrivalPrepareResult::Failed)
				{
					impl_->fail(DedicatedCoopRuntimeError::InvalidState);
					return;
				}
				if (result == DedicatedCoopArrivalPrepareResult::Prepared)
					std::printf("[dedicated] native pre-battle prepared: id=%llu; encounter=%u; involved=%u; auto=%u; enter=%u; retreat=%u; placement=%u; awaiting decision\n",
						static_cast<unsigned long long>(decision->id), preparation.encounterCode, preparation.involvedMercs,
						preparation.actions.autoResolve, preparation.actions.enterSector, preparation.actions.retreat, preparation.actions.tacticalPlacement);
				else
					std::printf("[dedicated] native pre-battle remains pending: id=%llu; reason=%s\n", static_cast<unsigned long long>(decision->id),
						result == DedicatedCoopArrivalPrepareResult::ReinforcementDecisionRequired ? "militia reinforcement decision required" : "native context not supported or changed");
			}
			const char* kind = decision->kind == DedicatedCoopArrivalKind::WildernessNpc ? "wilderness-npc" :
				decision->kind == DedicatedCoopArrivalKind::CoordinateAttack ? "coordinate-attack" : "battle";
			std::printf("[dedicated] campaign arrival decision pending: id=%llu; kind=%s; group=%u:%u; sector=%u,%u,%u; time=%u; campaign paused; awaiting an explicit player decision\n",
				static_cast<unsigned long long>(decision->id), kind, decision->group.slot, decision->group.incarnation,
				decision->x, decision->y, decision->z, decision->worldSeconds);
			std::fflush(stdout);
			impl_->lastArrivalDecisionLogged = decision->id;
		}
	}
	if (!impl_->pumpTactical(context, *this)) return;
	if (impl_->meanwhile.acknowledged())
	{
		for (const auto& delivery : impl_->campaignActionAuthority.deliveries())
			if (delivery.pending) return;
		if (!impl_->freshAssignmentBaselineBoundary()) return;
		const auto* notice = impl_->meanwhile.pending();
		if (!notice || CompleteDedicatedCoopMeanwhile(notice->id) != DedicatedCoopMeanwhileResult::Applied)
			impl_->fail(DedicatedCoopRuntimeError::InvalidState);
		return;
	}
	// Keep transport/observations alive, but an arrival hold must also stop the
	// bootstrap's automatic established-sector launch and checkpoint transitions.
	if (impl_->battleNotice.acknowledged())
	{
		// Queue every retained campaign receipt before closing transport. Native
		// unloading must not invalidate the publication captured by pumpTactical.
		for (const auto& delivery : impl_->campaignActionAuthority.deliveries())
			if (delivery.pending) return;
		if (!impl_->freshAssignmentBaselineBoundary()) return;
		const auto* notice = impl_->battleNotice.pending();
		if (!notice || !impl_->stopAdmissionForPostCombatReturn()) return;
		if (CompleteDedicatedCoopBattleNotice(notice->id) != DedicatedCoopBattleNoticeResult::Applied)
		{
			impl_->fail(DedicatedCoopRuntimeError::MissionReturnFailed);
			return;
		}
		if (!impl_->beginWorldDrain()) return;
		(void)impl_->tryFinishWorldDrain(context);
		return;
	}
	if (DedicatedCoopArrivalDecisionPending() || DedicatedCoopSurrenderPending() || DedicatedCoopBattleNoticePending() || DedicatedCoopMeanwhilePending()) return;
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
			// Keep a peaceful established campaign cold and connected so shared
			// travel actions can select the next encounter.
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
			impl_->arrivalDecisions.enableEstablishedAimArrivals();
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
		// Native dialogue completion may undo its own temporary pause. Campaign
		// time remains stopped until a ready time leader explicitly resumes it.
		StopTimeCompression();
		PauseGame();
		const bool mapReady = IsDedicatedCoopStarterMissionMapReady();
		const auto eligibility = EvaluateDedicatedCheckpointEligibility(
			CollectCheckpointEligibility(context, true,
				impl_->tacticalCommandsDrained(), impl_->tacticalNetworkDrained()));
		const auto step = EvaluateDedicatedCoopPostCombatCheckpointStep(
			mapReady, eligibility, now >= impl_->postCombatCheckpointDeadline);
		if (step == DedicatedCoopPostCombatCheckpointStep::WaitForNativeExit)
		{
			if (mapReady && impl_->lastEligibility != eligibility)
			{
				std::printf("[dedicated] post-combat checkpoint waiting for native exit: %s\n",
					DedicatedCheckpointEligibilityReasonName(eligibility));
				std::fflush(stdout);
			}
			impl_->lastEligibility = eligibility;
			return;
		}
		if (step != DedicatedCoopPostCombatCheckpointStep::Commit)
		{
			impl_->lastEligibility = eligibility;
			std::fprintf(stderr, "[dedicated] %s: %s\n",
				step == DedicatedCoopPostCombatCheckpointStep::TimedOut
					? "mission.return-checkpoint-timeout" : "mission.return-checkpoint",
				mapReady ? DedicatedCheckpointEligibilityReasonName(eligibility)
					: "native strategic screen exit did not complete");
			impl_->fail(DedicatedCoopRuntimeError::CheckpointNotEligible);
			return;
		}
		if (!impl_->checkpointNow(context, true)) return;
		if (!impl_->startAdmission()) return;
		impl_->postCombatCheckpointDeadline = {};
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

DedicatedCoopArrivalEnterResult DedicatedCoopRuntime::enterArrivalBattle(std::uint64_t decision,
	NativePreBattleDeployment deployment) noexcept
{
	using Result = DedicatedCoopArrivalEnterResult;
	if (!impl_ || impl_->error != DedicatedCoopRuntimeError::None || !impl_->campaignEntered ||
		impl_->starterMission != StarterMissionState::StrategicIdle || impl_->worldDraining ||
		!impl_->tactical || impl_->tactical->server.worldActive() || impl_->selfRetirementActive)
		return Result::NativeContextUnavailable;
	const auto result = EnterDedicatedCoopArrivalBattle(decision, deployment);
	if (result == Result::Failed)
		impl_->fail(DedicatedCoopRuntimeError::MissionLaunchFailed);
	if (result != Result::Entered) return result;
	// Reuse fresh world observation, assignment/baseline admission and native
	// victory/drain handling. Do not leave a loaded encounter marked strategic.
	impl_->starterMission = StarterMissionState::WaitingForControllableActor;
	// Wounded/unconscious native participants remain present but need not all
	// become controllable before their healthy squadmates can play.
	impl_->minimumControllableActors = 1;
	impl_->postCombatReturnArmed = true;
	impl_->starterActorArrivalDeadline = Clock::now() + StarterActorArrivalTimeout;
	return Result::Entered;
}

DedicatedCoopArrivalRetreatResult DedicatedCoopRuntime::retreatArrivalBattle(std::uint64_t decision) noexcept
{
	using Result = DedicatedCoopArrivalRetreatResult;
	if (!impl_ || impl_->error != DedicatedCoopRuntimeError::None || !impl_->campaignEntered ||
		impl_->starterMission != StarterMissionState::StrategicIdle || impl_->worldDraining ||
		!impl_->tactical || impl_->tactical->server.worldActive() || impl_->selfRetirementActive)
		return Result::NativeContextUnavailable;
	const auto result = RetreatFromDedicatedCoopArrivalBattle(decision);
	if (result == Result::Failed)
		impl_->fail(DedicatedCoopRuntimeError::InvalidState);
	// Remain strategic and paused. Native movement events own the return trip;
	// only a later explicit time request may advance it.
	return result;
}

bool DedicatedCoopRuntime::shutdownAtCommittedBoundary(
	GameContext& context) noexcept
{
	if (impl_ && context.campaignSimulation().failed())
	{
		impl_->fail(DedicatedCoopRuntimeError::InvalidState);
		return false;
	}
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
	UnbindDedicatedCoopArrivalState(impl_->arrivalDecisions);
	UnbindDedicatedCoopSurrenderState(impl_->surrenderDecision);
	UnbindDedicatedCoopBattleNoticeState(impl_->battleNotice);
	UnbindDedicatedCoopMeanwhileState(impl_->meanwhile);
	impl_->arrivalDecisions.reset();
	impl_->surrenderDecision.reset();
	impl_->battleNotice.reset();
	impl_->meanwhile.reset();
	impl_->lastArrivalDecisionLogged = 0;
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
