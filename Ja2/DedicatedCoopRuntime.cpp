#include "DedicatedCoopAttack.h"
#include "DedicatedCoopRuntime.h"
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
#include "DedicatedCheckpointRuntimeEvidence.h"

#include "DedicatedContentManifest.h"
#include "DedicatedCampaignSaveBridge.h"
#include "DedicatedCoopMissionBootstrap.h"
#include "DedicatedCoopExitGrid.h"
#include "DedicatedCoopAssignment.h"
#include "DedicatedCoopConversation.h"
#include "DedicatedCoopPostCombatCheckpointPolicy.h"
#include "DedicatedCoopTacticalHost.h"
#include "DedicatedCoopInventoryPublication.h"
#include "CampaignPackage.h"
#include "GameContext.h"
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
#include "CoopCampaignStatus.h"
#include "DedicatedCoopCampaignGroups.h"
#include "CoopCampaignTimeAuthority.h"
#include "CoopCampaignActionAuthority.h"
#include "DedicatedCoopSurrender.h"
#include "DedicatedCoopBattleNotice.h"
#include "DedicatedCoopMeanwhile.h"
#include "CoopCampaignHireAuthority.h"
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
#include "GameSettings.h"
#include "Soldier Profile Constants.h"
#include "Soldier Profile.h"
#include "interface Dialogue.h"

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

const char* RandomInstallResultName(
	GameSimulationRandomInstallError result) noexcept
{
	switch (result)
	{
		case GameSimulationRandomInstallError::None: return "success";
		case GameSimulationRandomInstallError::SourceAlreadyObserved:
			return "random source was already observed";
		case GameSimulationRandomInstallError::AlreadyInstalled:
			return "simulation random source was already installed";
	}
	return "unknown random installation result";
}

const char* CampaignStoreErrorName(DedicatedCampaignStoreError error) noexcept
{
	switch (error)
	{
		case DedicatedCampaignStoreError::None: return "none";
		case DedicatedCampaignStoreError::InvalidIdentity: return "invalid campaign identity";
		case DedicatedCampaignStoreError::BackendIdentityMismatch: return "backend identity mismatch";
		case DedicatedCampaignStoreError::MissingContentManifestSha256: return "missing content manifest SHA-256";
		case DedicatedCampaignStoreError::AlreadyOpen: return "campaign store already open";
		case DedicatedCampaignStoreError::AlreadyExists: return "campaign already exists";
		case DedicatedCampaignStoreError::NotFound: return "campaign not found";
		case DedicatedCampaignStoreError::NoValidCheckpoint: return "no valid checkpoint";
		case DedicatedCampaignStoreError::IncompatibleManifest: return "incompatible campaign manifest";
		case DedicatedCampaignStoreError::UnsupportedManifestFormat: return "unsupported campaign manifest";
		case DedicatedCampaignStoreError::SplitBrain: return "conflicting checkpoint generations";
		case DedicatedCampaignStoreError::NotOpen: return "campaign store not open";
		case DedicatedCampaignStoreError::GenerationExhausted: return "checkpoint generation exhausted";
		case DedicatedCampaignStoreError::PublicationDurabilityUnknown: return "manifest durability unknown";
		case DedicatedCampaignStoreError::PublicationStateUnknown: return "manifest publication state unknown";
		case DedicatedCampaignStoreError::BackendFailure: return "campaign storage backend failure";
	}
	return "unknown campaign store error";
}

const char* CampaignBootDetail(
	const DedicatedCampaignBootResult& result) noexcept
{
	if (result.filesystemError != DedicatedCampaignFilesystemError::None)
		return DedicatedCampaignFilesystemErrorName(result.filesystemError);
	if (result.storeError != DedicatedCampaignStoreError::None)
		return CampaignStoreErrorName(result.storeError);
	return "none";
}

const char* CampaignSyncServerResultName(
	CoopSession::FullEngineCoopCampaignSyncServerResult result) noexcept
{
	using Result = CoopSession::FullEngineCoopCampaignSyncServerResult;
	switch (result)
	{
		case Result::Success: return "success";
		case Result::InvalidConfiguration: return "invalid configuration";
		case Result::AlreadyActive: return "already active";
		case Result::NotActive: return "not active";
		case Result::InvalidSessionEpoch: return "invalid session epoch";
		case Result::InvalidCheckpoint: return "invalid checkpoint";
		case Result::CheckpointNotNewer: return "checkpoint is not newer";
		case Result::SourceUnavailable: return "checkpoint source unavailable";
		case Result::SourceDescriptorMismatch: return "checkpoint source descriptor mismatch";
		case Result::InvalidPeerSet: return "invalid peer set";
		case Result::PeerCapacityReached: return "peer capacity reached";
		case Result::InvalidPeer: return "invalid peer";
		case Result::StaleTransport: return "stale transport";
		case Result::MalformedFrame: return "malformed campaign frame";
		case Result::ClaimedIdentityMismatch: return "claimed identity mismatch";
		case Result::StaleTransfer: return "stale campaign transfer";
		case Result::UnexpectedFrame: return "unexpected campaign frame";
		case Result::SequenceMismatch: return "campaign sequence mismatch";
		case Result::IntegrityMismatch: return "campaign integrity mismatch";
		case Result::ClientRejected: return "client rejected campaign transfer";
		case Result::TransferExhausted: return "campaign transfer sequence exhausted";
		case Result::CodecFailure: return "campaign codec failure";
		case Result::AllocationFailure: return "campaign allocation failure";
		case Result::TransportBackpressured: return "campaign transport backpressured";
		case Result::Busy: return "campaign synchronizer busy";
		case Result::TerminalFailure: return "campaign synchronizer terminal failure";
	}
	return "unknown campaign synchronization result";
}

const char* TacticalServerResultName(
	CoopSession::FullEngineCoopTacticalServerResult result) noexcept
{
	using Result = CoopSession::FullEngineCoopTacticalServerResult;
	switch (result)
	{
		case Result::Success: return "success";
		case Result::InvalidConfiguration: return "invalid configuration";
		case Result::NotActive: return "not active";
		case Result::AlreadyActive: return "already active";
		case Result::AdmissionInactive: return "admission inactive";
		case Result::SessionEpochMismatch: return "session epoch mismatch";
		case Result::NoWorld: return "no tactical world";
		case Result::InvalidContext: return "invalid tactical context";
		case Result::InvalidAssignment: return "invalid actor assignment";
		case Result::InvalidPeerSet: return "invalid tactical peer set";
		case Result::PeerCapacityReached: return "tactical peer capacity reached";
		case Result::PeerReconciliationFailed: return "peer reconciliation failed";
		case Result::BaselineUnavailable: return "tactical baseline unavailable";
		case Result::ReplicationFailure: return "tactical replication failure";
		case Result::ActorBindingFailure: return "actor binding failure";
		case Result::ReceiptCapacityReached: return "receipt capacity reached";
		case Result::ReceiptRejected: return "receipt rejected";
		case Result::SequenceDiverged: return "command sequence diverged";
		case Result::ExecutionBackpressured: return "tactical execution backpressured";
		case Result::CommandCompletionDeadlineExceeded:
			return "tactical command completion deadline exceeded";
		case Result::TransportBackpressured: return "tactical transport backpressured";
		case Result::PendingReceipts: return "pending tactical receipts";
		case Result::PendingInput: return "pending tactical input";
		case Result::InputRejected: return "tactical input rejected";
		case Result::Busy: return "tactical server busy";
		case Result::InternalFailure: return "tactical server internal failure";
	}
	return "unknown tactical server result";
}

const char* ReplicationResultName(
	CoopSession::FullEngineCoopServerSessionResult result) noexcept
{
	using Result = CoopSession::FullEngineCoopServerSessionResult;
	switch (result)
	{
		case Result::Success: return "success";
		case Result::InvalidConfiguration: return "invalid configuration";
		case Result::NotActive: return "not active";
		case Result::NoWorld: return "no tactical world";
		case Result::InvalidPeer: return "invalid peer";
		case Result::PeerCapacityReached: return "peer capacity reached";
		case Result::InvalidContext: return "invalid replication context";
		case Result::StaleContext: return "stale replication context";
		case Result::InvalidAssignment: return "invalid actor assignment";
		case Result::AssignmentCapacityReached: return "assignment capacity reached";
		case Result::SequenceExhausted: return "replication sequence exhausted";
		case Result::DeltaPayloadTooLarge: return "delta payload too large";
		case Result::CodecFailure: return "replication codec failure";
		case Result::ReceiptCapacityReached: return "receipt capacity reached";
		case Result::ConflictingReceipt: return "conflicting receipt";
		case Result::UnexpectedAcknowledgement: return "unexpected acknowledgement";
		case Result::IntegrityMismatch: return "replication integrity mismatch";
		case Result::ResyncRequired: return "replication resynchronization required";
		case Result::NotFound: return "replication record not found";
		case Result::AllocationFailure: return "replication allocation failure";
		case Result::Busy: return "replication session busy";
	}
	return "unknown replication result";
}

const char* AuthorizationReasonName(
	CoopSession::TacticalIntentAuthorizationReason reason) noexcept
{
	using Reason = CoopSession::TacticalIntentAuthorizationReason;
	switch (reason)
	{
		case Reason::None: return "none";
		case Reason::NotConfigured: return "authority not configured";
		case Reason::NotAdmitted: return "peer not admitted";
		case Reason::WrongSessionEpoch: return "wrong session epoch";
		case Reason::ClaimedIdentityMismatch: return "claimed identity mismatch";
		case Reason::InvalidIntent: return "invalid tactical intent";
		case Reason::WrongGeneration: return "wrong world generation";
		case Reason::StaleRevision: return "stale world revision";
		case Reason::FutureRevision: return "future world revision";
		case Reason::StaleTurn: return "stale turn";
		case Reason::FutureTurn: return "future turn";
		case Reason::InvalidCommandId: return "invalid command id";
		case Reason::DuplicateCommand: return "duplicate command";
		case Reason::OutOfOrderCommand: return "out-of-order command";
		case Reason::SequenceExhausted: return "command sequence exhausted";
		case Reason::ActorNotOwned: return "actor not owned";
		case Reason::PeerCapacityReached: return "authority peer capacity reached";
	}
	return "unknown tactical authorization reason";
}

const char* TacticalPumpDetail(
	const CoopSession::FullEngineCoopTacticalServerPumpResult& result) noexcept
{
	if (result.replicationResult !=
		CoopSession::FullEngineCoopServerSessionResult::Success)
		return ReplicationResultName(result.replicationResult);
	if (result.authorizationReason !=
		CoopSession::TacticalIntentAuthorizationReason::None)
		return AuthorizationReasonName(result.authorizationReason);
	return "none";
}

const char* ObserverUpdateResultName(
	TacticalWorldObserverUpdateResult result) noexcept
{
	switch (result)
	{
		case TacticalWorldObserverUpdateResult::PublishedBaseline: return "baseline published";
		case TacticalWorldObserverUpdateResult::PublishedDelta: return "delta published";
		case TacticalWorldObserverUpdateResult::SourceUnavailable: return "source unavailable";
		case TacticalWorldObserverUpdateResult::SourceCapacityReached: return "source capacity reached";
		case TacticalWorldObserverUpdateResult::SourceAllocationFailure: return "source allocation failure";
		case TacticalWorldObserverUpdateResult::SourceAdapterFailure: return "source adapter failure";
		case TacticalWorldObserverUpdateResult::InvalidSnapshot: return "invalid tactical snapshot";
		case TacticalWorldObserverUpdateResult::ActorCapacityReached: return "actor capacity reached";
		case TacticalWorldObserverUpdateResult::EventCapacityReached: return "event capacity reached";
		case TacticalWorldObserverUpdateResult::AllocationFailure: return "observer allocation failure";
		case TacticalWorldObserverUpdateResult::SerialExhausted: return "observer serial exhausted";
		case TacticalWorldObserverUpdateResult::Unchanged: return "unchanged";
		case TacticalWorldObserverUpdateResult::DoorCapacityReached: return "door capacity reached";
	}
	return "unknown tactical observer result";
}

const char* AdmissionStartResultName(
	CoopSession::FullEngineCoopStartResult result) noexcept
{
	using Result = CoopSession::FullEngineCoopStartResult;
	switch (result)
	{
		case Result::Success: return "success";
		case Result::AuthorityDisabled: return "authority disabled";
		case Result::ConfigurationIncomplete: return "configuration incomplete";
		case Result::InvalidTacticalContext: return "invalid tactical context";
		case Result::SessionEpochMismatch: return "session epoch mismatch";
		case Result::AdmissionSessionInactive: return "admission session inactive";
	}
	return "unknown admission start result";
}

const char* ListenerStartResultName(
	CoopSession::FullEngineCoopAdmissionListenerStartResult result) noexcept
{
	using Result = CoopSession::FullEngineCoopAdmissionListenerStartResult;
	switch (result)
	{
		case Result::Success: return "success";
		case Result::AdmissionSessionInactive: return "admission session inactive";
		case Result::InvalidConfiguration: return "invalid listener configuration";
		case Result::TransportUnavailable: return "listener transport unavailable";
		case Result::TransportStartFailed: return "listener transport start failed";
		case Result::LifecycleBusy: return "listener lifecycle busy";
	}
	return "unknown listener start result";
}

const char* WorldParticipantResultName(
	CoopSession::CoopWorldParticipantPolicyResult result) noexcept
{
	using Result = CoopSession::CoopWorldParticipantPolicyResult;
	switch (result)
	{
		case Result::Unchanged: return "unchanged";
		case Result::Published: return "published";
		case Result::DeferredUntilFreshBaseline: return "deferred until fresh baseline";
		case Result::InvalidCurrentSet: return "invalid current participant set";
		case Result::InvalidReadySet: return "invalid ready participant set";
		case Result::CapacityReached: return "participant capacity reached";
	}
	return "unknown world participant result";
}

const char* ActorAssignmentResultName(
	CoopSession::CoopActorAssignmentPolicyResult result) noexcept
{
	using Result = CoopSession::CoopActorAssignmentPolicyResult;
	switch (result)
	{
		case Result::Success: return "success";
		case Result::InvalidPeerSet: return "invalid assignment peer set";
		case Result::InvalidActorSet: return "invalid controllable actor set";
		case Result::CapacityReached: return "actor assignment capacity reached";
	}
	return "unknown actor assignment result";
}

enum class StarterMissionState : std::uint8_t
{
	Unprepared,
	WaitingForCampaignReadyPeer,
	WaitingForEstablishedCampaignReadyPeer,
	WaitingForControllableActor,
	Playable,
	// The encounter is won and the sector stays loaded for peaceful play
	// (movement, loot, first aid) until a player explicitly returns to the map.
	PeacefulTactical,
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
		DialogueQueueIsEmpty() && !gfWaitingForTriggerTimer && !DedicatedCoopArrivalDecisionPending() && !DedicatedCoopSurrenderPending() && !DedicatedCoopBattleNoticePending() && !DedicatedCoopMeanwhilePending() && !DedicatedCoopAttackPending()
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
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign-sync.start", "runtime prerequisites are invalid");
			return false;
		}
		std::unique_ptr<DedicatedCampaignSyncCheckpointSource> source;
		if (!captureCampaignSource(source))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign-sync.source", "active checkpoint reader unavailable");
			return false;
		}
		std::unique_ptr<CoopSession::FullEngineCoopCampaignSyncServer> server(
			new (std::nothrow) CoopSession::FullEngineCoopCampaignSyncServer(
				*source, tactical->campaignWire));
		if (!server)
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign-sync.allocate", "campaign synchronizer allocation failed");
			return false;
		}
		const CoopSession::FullEngineCoopCampaignSyncServerResult begun =
			server->beginSession(sessionEpoch);
		if (begun !=
			CoopSession::FullEngineCoopCampaignSyncServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign-sync.begin", CampaignSyncServerResultName(begun));
			return false;
		}
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
			lastCampaignSyncResult = tactical->campaignSync->endSession();
			ended = lastCampaignSyncResult == CoopSession::
				FullEngineCoopCampaignSyncServerResult::Success;
		}
		tactical->campaignSync.reset();
		tactical->campaignSource.reset();
		return ended;
	}

	bool supersedeCampaignCheckpoint() noexcept
	{
		if (tactical == nullptr || tactical->campaignSync == nullptr) return true;
		std::unique_ptr<DedicatedCampaignSyncCheckpointSource> source;
		if (!captureCampaignSource(source))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign-sync.supersede-source",
				"new checkpoint reader unavailable");
			return false;
		}
		const CoopSession::FullEngineCoopCampaignSyncServerResult superseded =
			tactical->campaignSync->supersedeCheckpoint(*source);
		if (superseded !=
			CoopSession::FullEngineCoopCampaignSyncServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign-sync.supersede",
				CampaignSyncServerResultName(superseded));
			return false;
		}
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
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign-sync.reconcile", "campaign synchronizer is not active");
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
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign-sync.reconcile",
				CampaignSyncServerResultName(reconciled));
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
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
				"tactical.campaign-ready", TacticalServerResultName(gated));
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
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign.status", "invalid native compression mode");
			return false;
		}
		captured.compressionMode = static_cast<std::int8_t>(giTimeCompressMode);
		captured.gamePaused = GamePaused() != FALSE;
		captured.pauseLocked = PauseStateLocked() != FALSE;
		captured.compressionActive = IsTimeBeingCompressed() != FALSE;
		captured.timeInterrupted = gfTimeInterrupt != FALSE;
		captured.phase = starterMission == StarterMissionState::StrategicIdle
			? CoopSession::CoopCampaignPhase::Strategic
			: (starterMission == StarterMissionState::Playable ||
			   starterMission == StarterMissionState::PeacefulTactical)
				? CoopSession::CoopCampaignPhase::Tactical
				: (worldDraining || starterMission == StarterMissionState::ReturningToStrategic ||
				   starterMission == StarterMissionState::WaitingForStrategicCheckpoint)
					? CoopSession::CoopCampaignPhase::Transition : CoopSession::CoopCampaignPhase::Starting;
		if (starterMission == StarterMissionState::PeacefulTactical) captured.securedSector = securedSector;
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
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign.status", "invalid or regressing campaign observation");
			return false;
		}
		CoopSession::CoopCampaignStatusBytes bytes;
		if (!CoopSession::EncodeCoopCampaignStatus(campaignStatusLedger.value(), bytes))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign.status", "campaign observation encoding failed");
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
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
					"campaign.status", "campaign delivery capacity exhausted");
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
		if (!worldDraining && (starterMission == StarterMissionState::Playable ||
			starterMission == StarterMissionState::PeacefulTactical || starterMission == StarterMissionState::StrategicIdle))
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
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "campaign.groups", "invalid campaign group observation");
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
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "campaign.groups", "group observation encoding failed");
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
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "campaign.groups", "group delivery capacity exhausted");
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
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed, message, "economic delivery capacity exhausted");
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
		if (!worldDraining && (starterMission == StarterMissionState::Playable ||
			starterMission == StarterMissionState::PeacefulTactical || starterMission == StarterMissionState::StrategicIdle))
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
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "campaign.economy", "invalid native economy observation");
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
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "campaign.aim.quotes", "invalid native AIM quote observation");
			return false;
		}
		CoopSession::CoopCampaignEconomyBytes economyBytes;
		CoopSession::CoopCampaignAimQuotesBytes quoteBytes;
		std::size_t economySize = 0, quoteSize = 0;
		if (!CoopSession::EncodeCoopCampaignEconomy(campaignEconomyLedger.value(), economyBytes, economySize) ||
			!CoopSession::EncodeCoopCampaignAimQuotes(campaignQuotesLedger.value(), quoteBytes, quoteSize))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "campaign.economy", "economic observation encoding failed");
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

	// The native talk panel, replicated read-only to every campaign-ready peer.
	// A runtime-scoped id is assigned each time the panel opens.
	bool publishConversation() noexcept
	{
		const bool open = DedicatedCoopTalkPanelOpen();
		const bool opened = open && !talkPanelWasOpen;
		if (opened)
		{
			if (lastConversation == UINT32_MAX)
			{
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "conversation", "conversation identities exhausted");
				return false;
			}
			conversation = ++lastConversation;
		}
		if (!open) { conversation = 0; conversationInitiator = {}; }
		talkPanelWasOpen = open;
		CoopSession::CoopConversation captured;
		if (!open || !CaptureDedicatedCoopConversation(conversation, conversationInitiator, captured)) captured = {};
		// A panel opened by an accepted Talk intent for exactly this pair belongs
		// to that player; one opened by native gameplay has no initiator.
		if (opened && captured.conversation)
			captured.initiator = conversationInitiator =
				TakeDedicatedCoopConversationRequest(captured.merc, captured.npc);
		SetDedicatedCoopConversationState(captured.conversation
			? DedicatedCoopConversationState{captured.conversation, captured.initiator, captured.merc, captured.npcProfile,
				captured.mercProfile, captured.npcSource}
			: DedicatedCoopConversationState{});
		if (!conversationLedger.observe(captured))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "conversation", "invalid native conversation observation");
			return false;
		}
		CoopSession::CoopConversationBytes bytes;
		if (!CoopSession::EncodeCoopConversation(conversationLedger.value(), bytes))
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "conversation", "conversation encoding failed");
			return false;
		}
		return queueCampaignEconomicObservation(CoopSession::CoopConversationMessageName, bytes.data(), bytes.size(),
			conversationLedger.value().revision, conversationDeliveries);
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
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "campaign.time.reconcile", "invalid authenticated time-control peers");
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
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "campaign.time.result", "invalid time-control outcome");
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
		fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "campaign.action.reconcile", "invalid authenticated campaign-action peers");
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
		if (request.action == Action::ReturnToMap)
		{
			// Accept now; the frame loop unloads only after this result and every
			// other campaign receipt has been delivered to its peer.
			if (selfRetirementActive || starterMission != StarterMissionState::PeacefulTactical ||
				!securedSector || request.decision != securedSector || returnToMapRequested)
				return {Outcome::NativeRejected, 0};
			returnToMapRequested = true; returnWaitLogged = false;
			return {Outcome::Applied, 0};
		}
		if (request.action == Action::TakeExitGrid)
		{
			if (selfRetirementActive || starterMission != StarterMissionState::PeacefulTactical ||
				!securedSector || request.decision != securedSector || returnToMapRequested)
				return {Outcome::NativeRejected, 0};
			DedicatedCoopExitGridPlan plan;
			const auto code = PlanDedicatedCoopExitGridTraversal(request.group, plan);
			using Code = DedicatedCoopExitGridCode;
			if (code != Code::Planned)
				return {code == Code::UnsupportedGroup || code == Code::HostileDestination ? Outcome::Unsupported :
					Outcome::NativeRejected, static_cast<std::uint16_t>(code)};
			exitGrid = plan; exitGridPending = true; returnToMapRequested = true; returnWaitLogged = false;
			std::printf("[dedicated] exit-grid traversal accepted to (%u,%u,%u) grid %d with %zu mercs\n",
				unsigned(plan.x), unsigned(plan.y), unsigned(plan.z), plan.destinationGrid, plan.memberCount);
			std::fflush(stdout);
			return {Outcome::Applied, static_cast<std::uint16_t>(code)};
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
			case Action::SetAssignment:
			{
				const TacticalEntityId merc{static_cast<std::uint16_t>(request.decision & 0xFFFFu),
					static_cast<std::uint32_t>(request.decision >> 16)};
				const auto result = ApplyDedicatedCoopAssignment(merc, request.destinationX, request.destinationY);
				using Code = DedicatedCoopAssignmentCode;
				return {result == Code::Applied ? Outcome::Applied :
					result == Code::UnsupportedKind ? Outcome::Unsupported : Outcome::NativeRejected,
					static_cast<std::uint16_t>(result)};
			}
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
			case Action::EnterSector:
			{
				const auto result = runtime.enterPeacefulSector(request.group);
				using Code = DedicatedCoopPeacefulEntryCode;
				return {result == Code::Entered ? Outcome::Applied : result == Code::EntryFailed ? Outcome::Failed :
					result == Code::UnsupportedGroup ? Outcome::Unsupported : Outcome::NativeRejected,
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
			fail(DedicatedCoopRuntimeError::InvalidState, "campaign.action", "native campaign action failed");
			return false;
		}
		// Recapture before another peer's queued request can validate against the
		// old group/arrival state, and before the receipt can release client input.
		return publishCampaignStatus() && publishCampaignGroups() && publishCampaignEconomyAndQuotes() && publishConversation();
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
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "campaign.action.result", "invalid campaign-action outcome");
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
		fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "campaign.hire.reconcile", "invalid authenticated hiring peers");
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
			fail(DedicatedCoopRuntimeError::InvalidState, "campaign.hire", "native hiring operation failed");
			return false;
		}
		// A second queued player request sees the new roster, balance, offers and
		// shared control barrier. Every receipt waits for those observations.
		return publishCampaignStatus() && publishCampaignGroups() && publishCampaignEconomyAndQuotes() && publishConversation();
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
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed, "campaign.hire.result", "invalid hiring receipt");
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
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign-sync.pump", "campaign synchronizer is unavailable");
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
				if (!publishCampaignStatus() || !publishCampaignGroups() || !publishCampaignEconomyAndQuotes() || !publishConversation()) return false;
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
					fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
						"campaign-sync.inbound", "unknown inbound message kind");
					return false;
			}
			const CoopSession::FullEngineCoopCampaignSyncServerResult handled =
				tactical->campaignSync->handleInbound(message.peerIdentity,
					message.transport, kind, message.bytes.data(), message.size);
			if (CoopSession::IsFatalCoopCampaignSyncInboundResult(handled))
			{
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
					"campaign-sync.inbound",
					CampaignSyncServerResultName(handled));
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
		fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
			"campaign-sync.flush",
			CampaignSyncServerResultName(flushed.result));
		return false;
	}

	void fail(DedicatedCoopRuntimeError incoming,
		const char* stage = "server.runtime", const char* reason = nullptr,
		const char* detail = "none") noexcept
	{
		if (incoming == DedicatedCoopRuntimeError::None) return;
		if (!fatal)
		{
			error = incoming;
			failureDiagnostic.capture(stage,
				reason != nullptr ? reason :
					DedicatedCoopRuntimeErrorName(incoming), detail);
		}
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
		const CoopSession::FullEngineCoopTacticalServerResult discarded =
			tactical->server.discardInboundAfterSelfRetirementGate();
		if (discarded !=
			CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
				"tactical.retirement-gate", TacticalServerResultName(discarded));
			return false;
		}
		selfRetirement = captured;
		selfRetirementActive = true;
		if (captured.peerIdentity == campaignStatusLedger.value().timeLeader && worldlessStrategicTimeControl())
			(void)TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_X0);
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
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
				"tactical.retirement-drain",
				"retiring peer still has pending commands");
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
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
				"tactical.retirement-result", "result encoding failed");
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
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
				"tactical.retirement-commit",
				"admission tombstone completion failed");
			return false;
		}

		// Completion is post-tombstone and best-effort on the exact transport ticket.
		// If it is lost, the same bearer deterministically learns CredentialRetired
		// from admission. No campaign/delta ACK is a prerequisite for this boundary.
		(void)tactical->listener.sendCommittedSelfRetirementResult(
			selfRetirement, encoded);
		if (!stopAdmissionAndReconcile(100)) return false;
		const CoopSession::FullEngineCoopTacticalServerResult retired =
			tactical->server.retirePeer(selfRetirement.peerIdentity);
		if (retired != CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
				"tactical.retirement-finalize", TacticalServerResultName(retired));
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
				fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
					"campaign-sync.disconnect",
					CampaignSyncServerResultName(campaignReconciled));
				return false;
			}
		}
		if (!tactical->server.active()) return true;
		const CoopSession::FullEngineCoopTacticalServerResult gated =
			tactical->server.setCampaignReadyPeers(nullptr, 0);
		if (gated != CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
				"tactical.disconnect-gate", TacticalServerResultName(gated));
			return false;
		}
		const CoopSession::FullEngineCoopTacticalServerResult reconciled =
			tactical->server.reconcilePeers();
		if (reconciled ==
			CoopSession::FullEngineCoopTacticalServerResult::Success)
			return true;
		fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
			"tactical.disconnect-reconcile",
			TacticalServerResultName(reconciled));
		return false;
	}

	bool startAdmission() noexcept
	{
		if (GetGameContext().campaignSimulation().failed())
		{
			fail(DedicatedCoopRuntimeError::InvalidState, "campaign.simulation",
				GetGameContext().campaignSimulation().failureReason());
			return false;
		}
		if (tactical == nullptr)
		{
			fail(DedicatedCoopRuntimeError::TacticalCompositionFailed,
				"admission.listener", "tactical composition is unavailable");
			return false;
		}
		if (tactical->listener.running()) return true;
		if (!admissionConfigured)
		{
			std::uint64_t epoch = 0;
			if (!tokens.issueSessionEpoch(epoch))
			{
				fail(DedicatedCoopRuntimeError::SessionEpochFailed,
					"admission.epoch", "OS random source could not issue an epoch");
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
			const CoopSession::FullEngineCoopStartResult admissionStarted =
				tactical->ingress.beginAdmissionSession(configuration);
			if (admissionStarted != CoopSession::FullEngineCoopStartResult::Success)
			{
				fail(DedicatedCoopRuntimeError::AdmissionStartFailed,
					"admission.session", AdmissionStartResultName(admissionStarted));
				return false;
			}
			const CoopSession::FullEngineCoopTacticalServerResult epochStarted =
				tactical->server.beginEpoch(epoch);
			if (epochStarted !=
				CoopSession::FullEngineCoopTacticalServerResult::Success)
			{
				tactical->ingress.endSession();
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
					"tactical.epoch", TacticalServerResultName(epochStarted));
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
			campaignEconomyLedger.clear(); campaignQuotesLedger.clear(); conversationLedger.clear();
			campaignEconomyDeliveries = {}; campaignQuotesDeliveries = {}; conversationDeliveries = {};
			lastCampaignEconomyDiagnostic = lastCampaignQuotesDiagnostic = nullptr;
			if (!campaignStatusLedger.beginSession(epoch)) return false;
			if (!campaignGroupsLedger.beginSession(epoch)) return false;
			if (!campaignEconomyLedger.beginSession(epoch) || !campaignQuotesLedger.beginSession(epoch) ||
				!conversationLedger.beginSession(epoch)) return false;
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
		if (!nonLoopbackBindWarningEmitted &&
			!IsDedicatedCoopCanonicalLoopbackBindAddress(
				options.coopBindAddress))
		{
			std::fprintf(stderr,
				"[dedicated] WARNING: co-op admission bind %s:%u is not a canonical "
				"loopback address; first join is permissionless and transport is plaintext; "
				"restrict this endpoint to a trusted LAN/VPN or bind loopback\n",
				options.coopBindAddress.c_str(),
				static_cast<unsigned>(options.coopPort));
			nonLoopbackBindWarningEmitted = true;
		}
		const CoopSession::FullEngineCoopAdmissionListenerStartResult
			listenerStarted = tactical->listener.start(configuration);
		if (listenerStarted !=
			CoopSession::FullEngineCoopAdmissionListenerStartResult::Success)
		{
			fail(DedicatedCoopRuntimeError::AdmissionStartFailed,
				"admission.listener", ListenerStartResultName(listenerStarted));
			return false;
		}
		return true;
	}

	bool checkpointNow(GameContext& context, bool required) noexcept
	{
		if (context.campaignSimulation().failed())
		{
			fail(DedicatedCoopRuntimeError::InvalidState, "campaign.simulation",
				context.campaignSimulation().failureReason());
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
			if (required)
				fail(DedicatedCoopRuntimeError::CheckpointNotEligible,
					"checkpoint.preflight",
					DedicatedCheckpointEligibilityReasonName(preflight));
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
			if (required)
				fail(DedicatedCoopRuntimeError::CheckpointNotEligible,
					"checkpoint.drained-eligibility",
					DedicatedCheckpointEligibilityReasonName(eligibility));
			return false;
		}

		campaignResult = boot.checkpoint(GetWorldTotalMin());
		if (!campaignResult)
		{
			fail(DedicatedCoopRuntimeError::CheckpointFailed,
				"checkpoint.commit",
				DedicatedCampaignBootErrorName(campaignResult.error),
				CampaignBootDetail(campaignResult));
			return false;
		}
		// Pin and validate the newly committed immutable file, then supersede
		// every campaign transfer before any admission transport can restart.
		if (!supersedeCampaignCheckpoint())
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign-sync.supersede", "checkpoint supersession failed");
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
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
					"tactical.participants", WorldParticipantResultName(result));
				return false;
		}
		fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
			"tactical.participants", WorldParticipantResultName(result));
		return false;
	}

	bool stageCurrentWorld(const TacticalWorldSnapshot& snapshot, bool& assignmentsDeferred) noexcept
	{
		assignmentsDeferred = false;
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
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
					"tactical.actor-capture",
					actorCount > assignments.size()
						? "controllable actor capacity reached"
						: "controllable actor capture failed");
				return false;
			}
			const CoopSession::CoopActorAssignmentPolicyResult built =
				CoopSession::BuildCoopActorAssignments(
				worldParticipants.data(), worldParticipantCount,
				actors.data(), actorCount, assignments, assignmentCount);
			if (built != CoopSession::CoopActorAssignmentPolicyResult::Success)
			{
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
					"tactical.actor-assignments",
					ActorAssignmentResultName(built));
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
			// Retained commands keep the ownership under which they were
			// admitted until their terminal outcome. Retry the new assignment
			// set on the next observation; do not publish a replacement baseline
			// or advance the local assignment bookkeeping while it is deferred.
			if (assigned == CoopSession::FullEngineCoopTacticalServerResult::Busy)
			{
				assignmentsDeferred = true;
				return true;
			}
			if (assigned !=
				CoopSession::FullEngineCoopTacticalServerResult::Success)
			{
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
					"tactical.assignment-publish",
					TacticalServerResultName(assigned));
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
			fail(DedicatedCoopRuntimeError::TacticalReplicationFailed,
				"tactical.baseline-stage", TacticalServerResultName(staged));
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
			starterMission == StarterMissionState::PeacefulTactical ||
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
		fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
			"tactical.world-drain-input", TacticalServerResultName(retired));
		return false;
	}

	bool stopAdmissionForPostCombatReturn() noexcept
	{
		if (tactical == nullptr || !tactical->server.worldActive())
		{
			fail(DedicatedCoopRuntimeError::MissionReturnFailed,
				"mission.return-freeze", "tactical world is not active");
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
		fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
			"tactical.return-input", TacticalServerResultName(retired));
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
		// The ordinary inbound path is skipped while the world drains. Keep its
		// deterministic pending-command frame bound advancing here so a lost
		// engine result cannot hold teardown (and the next admission epoch) forever.
		const CoopSession::FullEngineCoopTacticalServerPumpResult recoveryPump =
			tactical->server.pumpInbound(diagnostics.simulationTick);
		if (recoveryPump.result !=
				CoopSession::FullEngineCoopTacticalServerResult::Success &&
			recoveryPump.result !=
				CoopSession::FullEngineCoopTacticalServerResult::NoWorld &&
			recoveryPump.result !=
				CoopSession::FullEngineCoopTacticalServerResult::InputRejected &&
			recoveryPump.result !=
				CoopSession::FullEngineCoopTacticalServerResult::TransportBackpressured)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
				"tactical.world-drain-command",
				TacticalServerResultName(recoveryPump.result),
				TacticalPumpDetail(recoveryPump));
			return false;
		}
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
			fail(DedicatedCoopRuntimeError::TacticalReplicationFailed,
				"tactical.world-drain-flush",
				TacticalServerResultName(flushed.result),
				TacticalPumpDetail(flushed));
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
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
				"tactical.world-end", TacticalServerResultName(ended));
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
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
					"tactical.world-begin", TacticalServerResultName(begun));
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
			fail(DedicatedCoopRuntimeError::TacticalReplicationFailed,
				"tactical.observer-order", "observer revision moved backwards");
			return false;
		}
		if (publication.serial == observedRevision) return true;
		if (publication.status != TacticalWorldPublicationStatus::Delta ||
			publication.delta == nullptr || publication.snapshot == nullptr)
		{
			fail(DedicatedCoopRuntimeError::TacticalReplicationFailed,
				"tactical.observer-delta",
				"new observer revision has no delta or resulting snapshot payload");
			return false;
		}
		bool baselineFallbackPublished = false;
		const CoopSession::FullEngineCoopTacticalServerResult published =
			tactical->server.publishDeltaWithBaselineFallback(
				*publication.delta, *publication.snapshot,
				publication.serial, turnSerial,
				&baselineFallbackPublished);
		if (published !=
			CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalReplicationFailed,
				"tactical.delta-publish", TacticalServerResultName(published));
			return false;
		}
		if (baselineFallbackPublished)
		{
			std::printf(
				"[dedicated] oversized tactical delta replaced with revision %llu baseline\n",
				static_cast<unsigned long long>(publication.serial));
		}
		observedRevision = publication.serial;
		return true;
	}

	bool stageOwnerInventories(const TacticalWorldSnapshot& snapshot) noexcept
	{
		// One staging implementation (unit-tested in isolation); this wrapper
		// only translates its failure step into the runtime's diagnostics.
		DedicatedCoopOwnerInventoryStageFailure failure =
			DedicatedCoopOwnerInventoryStageFailure::None;
		CoopSession::FullEngineCoopTacticalServerResult stageResult =
			CoopSession::FullEngineCoopTacticalServerResult::Success;
		if (StageDedicatedCoopOwnerInventories(tactical->live, tactical->server,
			snapshot.epoch(), &failure, &stageResult))
			return true;
		const char* detail = "owner inventory staging failed";
		switch (failure)
		{
			case DedicatedCoopOwnerInventoryStageFailure::Context:
				detail = "owner inventory context unavailable"; break;
			case DedicatedCoopOwnerInventoryStageFailure::Assignment:
				detail = "owner inventory assignment missing"; break;
			case DedicatedCoopOwnerInventoryStageFailure::Capture:
				detail = "native inventory capture failed"; break;
			case DedicatedCoopOwnerInventoryStageFailure::Stage:
				detail = TacticalServerResultName(stageResult); break;
			case DedicatedCoopOwnerInventoryStageFailure::None: break;
		}
		fail(DedicatedCoopRuntimeError::TacticalReplicationFailed,
			"tactical.owner-inventory", detail);
		return false;
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
				const char* const detail = observerDiagnostics.lastUpdate ==
					TacticalWorldObserverUpdateResult::SourceAdapterFailure
					? Ja2TacticalWorldCaptureFailureName(
						GetJa2TacticalWorldAdapter().lastCaptureFailure())
					: "none";
				fail(DedicatedCoopRuntimeError::TacticalReplicationFailed,
					"tactical.observer",
					ObserverUpdateResultName(observerDiagnostics.lastUpdate),
					detail);
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
		if (!publishCampaignStatus() || !publishCampaignGroups() || !publishCampaignEconomyAndQuotes() || !publishConversation() || !reconcileCampaignTimePeers() ||
			!reconcileCampaignActionPeers() || !reconcileCampaignHirePeers() || !pumpCampaignInboundAndOutbound(runtime))
			return false;
		// A resync in this FIFO can remove readiness too. Never auto-resume on
		// reconnect or overwrite a native event pause on a later frame.
		pauseStrategicTimeWithoutLeader();
		if (!publishCampaignStatus() || !publishCampaignGroups() || !publishCampaignEconomyAndQuotes() || !publishConversation() ||
			!flushCampaignTimeResults() || !flushCampaignActionResults() || !flushCampaignHireResults())
			return false;
		const CoopSession::FullEngineCoopTacticalServerResult reconciled =
			tactical->server.reconcilePeers();
		if (reconciled !=
			CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
				"tactical.peer-reconcile", TacticalServerResultName(reconciled));
			return false;
		}
		bool assignmentsDeferred = false;
		if (publication && tactical->server.worldActive() &&
			!stageCurrentWorld(*publication.snapshot, assignmentsDeferred))
			return false;
		if (assignmentsDeferred)
		{
			// An old assignment may name an actor that has just left the world.
			// Do not capture that actor's inventory or flush staged terminal
			// receipts against old inventories. Native work progresses outside
			// this network pump; retain its bounded watchdog while waiting for
			// the next observation to publish the new ownership/baseline set.
			const auto deferred = tactical->server.pumpPendingCommandDeadlines();
			if (deferred.result != CoopSession::FullEngineCoopTacticalServerResult::Success)
			{
				fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
					"tactical.assignment-wait", TacticalServerResultName(deferred.result),
					TacticalPumpDetail(deferred));
				return false;
			}
			return true;
		}
		// Inventory can change without a public actor delta (reserve slots).
		// Stage its own revision every committed boundary, before inbound stale
		// checks and before any resulting terminal receipt can cross the wire.
		// Native work has committed and its public revision is staged. Capture
		// carried contents even when that public revision was unchanged: private
		// resources/metadata can change independently. pumpInbound also flushes,
		// so this barrier must precede it as well as the explicit outbound flush.
		if (publication && tactical->server.worldActive() &&
			!stageOwnerInventories(*publication.snapshot))
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
			fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
				"tactical.inbound", TacticalServerResultName(pumped.result),
				TacticalPumpDetail(pumped));
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
				fail(DedicatedCoopRuntimeError::TacticalReplicationFailed,
					"tactical.outbound", TacticalServerResultName(flushed.result),
					TacticalPumpDetail(flushed));
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
		if (!campaignEnded)
		{
			fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"tactical-composition.campaign-sync",
				CampaignSyncServerResultName(lastCampaignSyncResult));
			return false;
		}
		if (tacticalSinkRegistered && tacticalContext != nullptr)
		{
			const RuntimeMessageSinkRegistrationError removed =
				tacticalContext->runtimeMessages().removeSink(tactical->host);
			if (removed ==
				RuntimeMessageSinkRegistrationError::DispatchInProgress)
			{
				fail(DedicatedCoopRuntimeError::TacticalCompositionFailed,
					"tactical-composition.sink-remove",
					"runtime message sink is still dispatching");
				return false;
			}
			if (removed != RuntimeMessageSinkRegistrationError::None &&
				removed != RuntimeMessageSinkRegistrationError::NotFound)
			{
				fail(DedicatedCoopRuntimeError::TacticalCompositionFailed,
					"tactical-composition.sink-remove",
					"runtime message sink removal failed");
				return false;
			}
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
		UnbindDedicatedCoopArrivalState(arrivalDecisions);
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
		if (!fatal)
			fail(DedicatedCoopRuntimeError::TacticalCompositionFailed,
				"tactical-composition.detach",
				"tactical composition teardown failed");
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
	DedicatedCoopAttackState nativeAttack;
	std::uint64_t lastArrivalDecisionLogged = 0;
	// Same value type the checkpoint-evidence capture consumes.
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
	CoopSession::CoopConversationLedger conversationLedger;
	std::array<CampaignStatusDelivery, CoopSession::MaximumAuthorityPeers> conversationDeliveries{};
	std::uint32_t conversation = 0, lastConversation = 0;
	CoopSession::PeerIdentity conversationInitiator{};
	bool talkPanelWasOpen = false;
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
	CoopRuntimeFailureDiagnostic failureDiagnostic;
	DedicatedCoopCampaignEntry entry = DedicatedCoopCampaignEntry::None;
	DedicatedCheckpointEligibilityReason lastEligibility =
		DedicatedCheckpointEligibilityReason::None;
	CoopSession::FullEngineCoopCampaignSyncServerResult
		lastCampaignSyncResult =
			CoopSession::FullEngineCoopCampaignSyncServerResult::Success;
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
	bool nonLoopbackBindWarningEmitted = false;
	CoopSession::FullEngineCoopSelfRetirementInbound selfRetirement;
	bool selfRetirementActive = false;
	bool fatal = false;
	StarterMissionState starterMission = StarterMissionState::Unprepared;
	// Runtime-scoped identity of the secured sector in PeacefulTactical, and
	// an accepted shared request to return to the strategic map.
	std::uint32_t securedSector = 0;
	std::uint32_t lastSecuredSector = 0;
	bool returnToMapRequested = false;
	bool returnWaitLogged = false;
	// The loaded sector was entered peacefully (no battle was won in it).
	bool sectorSecuredByEntry = false;
	// An accepted exit-grid traversal: leave through the ordinary return, then
	// relocate the group worldless and enter its destination peacefully.
	bool exitGridPending = false;
	// The pending traversal is the deferred Omerta hideout cutscene; once
	// relocated it owes the stock surface edits and Carlos's welcome.
	bool hideoutTraversal = false;
	bool hideoutWelcomePending = false;
	DedicatedCoopExitGridPlan exitGrid{};

	DedicatedCoopPostCombatReturnEvidence captureSecuredSectorEvidence() const noexcept
	{
		DedicatedCoopPostCombatReturnEvidence evidence =
			CapturePostCombatReturnEvidence(true, postCombatReturnArmed);
		// A peacefully entered sector has no won battle: its hostile-free state,
		// checked by the same evidence, is the proof instead.
		if (sectorSecuredByEntry) evidence.lastBattleWon = true;
		return evidence;
	}
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
		impl_->fail(DedicatedCoopRuntimeError::CampaignPrepareFailed,
			"campaign.prepare",
			DedicatedCampaignBootErrorName(impl_->campaignResult.error),
			CampaignBootDetail(impl_->campaignResult));
		return false;
	}
	const GameSimulationRandomInstallError randomInstalled =
		InstallGameSimulationRandom(impl_->boot.campaignSeed());
	if (randomInstalled != GameSimulationRandomInstallError::None)
	{
		impl_->fail(DedicatedCoopRuntimeError::SimulationRandomInstallFailed,
			"simulation-random.install",
			RandomInstallResultName(randomInstalled));
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
		if (impl_)
			impl_->fail(DedicatedCoopRuntimeError::InvalidState,
				"content-manifest.capture",
				"runtime is not ready for manifest capture");
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
		impl_->fail(DedicatedCoopRuntimeError::ContentManifestFailed,
			"content-manifest.capture",
			DedicatedContentManifestErrorName(manifestResult));
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
		impl_->fail(DedicatedCoopRuntimeError::CampaignSimulationUnavailable,
			"campaign.open", "campaign simulation is disabled");
		return false;
	}
	const RuntimeCompatibilityFingerprint fingerprint =
		context.runtime().compatibilityFingerprint();
	impl_->campaignResult = impl_->boot.openCampaign(
		CampaignFingerprint(fingerprint), impl_->contentManifest);
	if (!impl_->campaignResult)
	{
		impl_->fail(DedicatedCoopRuntimeError::CampaignOpenFailed,
			"campaign.open",
			DedicatedCampaignBootErrorName(impl_->campaignResult.error),
			CampaignBootDetail(impl_->campaignResult));
		return false;
	}
	const DedicatedCampaignBootEntry openedEntry = impl_->boot.entry();
	if ((impl_->entry == DedicatedCoopCampaignEntry::Create &&
			openedEntry != DedicatedCampaignBootEntry::CreateNewCampaign) ||
		(impl_->entry == DedicatedCoopCampaignEntry::Resume &&
			openedEntry != DedicatedCampaignBootEntry::ResumeCheckpoint))
	{
		impl_->fail(DedicatedCoopRuntimeError::CampaignOpenFailed,
			"campaign.open-entry",
			"opened campaign does not match requested create/resume action");
		return false;
	}
	impl_->admissionFingerprint = AdmissionFingerprint(fingerprint);
	if (!CoopSession::ComputeCoopCampaignIdentitySha256(
		impl_->options.campaignId, impl_->boot.campaignSeed(),
		impl_->campaignIdentity))
	{
		impl_->fail(DedicatedCoopRuntimeError::CampaignOpenFailed,
			"campaign.identity", "campaign identity digest failed");
		return false;
	}
	TacticalComposition* composition = nullptr;
	try
	{
		const std::string packageId = context.packages().activeCampaign();
		if (packageId.empty())
		{
			impl_->fail(DedicatedCoopRuntimeError::TacticalCompositionFailed,
				"tactical-composition.package", "active campaign package is empty");
			return false;
		}
		composition = new TacticalComposition(
			context, impl_->tokens, packageId);
		if (context.runtimeMessages().addSink(composition->host) !=
			RuntimeMessageSinkRegistrationError::None)
		{
			composition->router.unbind();
			delete composition;
			impl_->fail(DedicatedCoopRuntimeError::TacticalCompositionFailed,
				"tactical-composition.message-sink",
				"runtime message sink registration failed");
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
		impl_->fail(DedicatedCoopRuntimeError::TacticalCompositionFailed,
			"tactical-composition.allocate",
			"composition construction threw an exception");
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
		impl_->fail(DedicatedCoopRuntimeError::InvalidState, "campaign.simulation",
			context.campaignSimulation().failureReason());
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
				impl_->fail(DedicatedCoopRuntimeError::CampaignEntryFailed,
					"campaign.resume-load",
					!state ? "campaign store state unavailable" :
						!state->hasCheckpoint ? "campaign has no checkpoint" :
							"checkpoint validation or load failed");
				return;
			}
			if (guiScreenToGotoAfterLoadingSavedGame != MAP_SCREEN)
			{
				impl_->fail(DedicatedCoopRuntimeError::CampaignEntryFailed,
					"campaign.resume-route",
					"loaded checkpoint did not route to strategic map");
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
				impl_->fail(DedicatedCoopRuntimeError::MissionPrepareFailed,
					"mission.prepare",
					DedicatedCoopMissionBootstrapErrorName(mission.error));
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
			impl_->fail(DedicatedCoopRuntimeError::MissionPrepareFailed,
				"mission.inspect",
				DedicatedCoopStarterCampaignStateName(starterState));
			return;
		}
		// Established ownership must not depend on DidGameJustStart,
		// which can change while the initial helicopter's events are still pending.
		if (starterState == DedicatedCoopStarterCampaignState::EstablishedCold ||
			starterState == DedicatedCoopStarterCampaignState::EstablishedStrategicCold)
			impl_->arrivalDecisions.enableEstablishedAimArrivals();
		if (!rosterCheckpointRequired)
		{
			const auto* stored = impl_->boot.campaignState();
			PassiveCampaignView view;
			const auto prepared = stored && stored->hasCheckpoint
				? PreparePassiveDedicatedCampaignCheckpoint(stored->activeSlot, GetWorldTotalMin(), view)
				: PassiveCampaignPreparationResult::InvalidCheckpoint;
			if (prepared == PassiveCampaignPreparationResult::MissingView)
				rosterCheckpointRequired = true;
			else if (prepared != PassiveCampaignPreparationResult::Ready)
			{
				impl_->fail(DedicatedCoopRuntimeError::CampaignEntryFailed,
					"campaign.resume-projection", "passive checkpoint preflight failed");
				return;
			}
		}
		// Publish prepared roster changes or a missing passive projection before
		// admission. checkpointNow rechecks the unchanged strategic eligibility
		// policy and atomically publishes a new generation; failure retains the
		// previous committed file. Exact resumes already containing PCVW stay intact.
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
		if (!BindDedicatedCoopArrivalState(impl_->arrivalDecisions) || !BindDedicatedCoopSurrenderState(impl_->surrenderDecision) || !BindDedicatedCoopAttackState(impl_->nativeAttack) || !BindDedicatedCoopBattleNoticeState(impl_->battleNotice) || !BindDedicatedCoopMeanwhileState(impl_->meanwhile))
		{
			impl_->fail(DedicatedCoopRuntimeError::InvalidState,
				"campaign.arrival.bind", "another arrival decision host is bound");
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
			impl_->fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
				"campaign-sync.start", "campaign synchronizer start failed");
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
		impl_->fail(DedicatedCoopRuntimeError::InvalidState, "campaign.meanwhile", failure);
		return;
	}
	if (const char* failure = impl_->nativeAttack.failure())
	{
		impl_->fail(DedicatedCoopRuntimeError::InvalidState, "tactical.attack", failure);
		return;
	}
	if (const char* failure = impl_->battleNotice.failure())
	{
		impl_->fail(DedicatedCoopRuntimeError::InvalidState, "campaign.battle-notice", failure);
		return;
	}
	if (const char* failure = impl_->surrenderDecision.failure())
	{
		impl_->fail(DedicatedCoopRuntimeError::InvalidState, "campaign.surrender", failure);
		return;
	}
	if (const char* failure = impl_->arrivalDecisions.failure())
	{
		impl_->fail(DedicatedCoopRuntimeError::InvalidState, "campaign.arrival.capture", failure);
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
					impl_->fail(DedicatedCoopRuntimeError::InvalidState, "campaign.arrival.prepare", impl_->arrivalDecisions.failure());
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
			impl_->fail(DedicatedCoopRuntimeError::InvalidState, "campaign.meanwhile.skip", impl_->meanwhile.failure());
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
			impl_->fail(DedicatedCoopRuntimeError::MissionReturnFailed, "campaign.battle-notice.unload", impl_->battleNotice.failure());
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
			const DedicatedCoopActorArrivalDiagnostics beforeLaunch =
				CaptureDedicatedCoopActorArrivalDiagnostics();
			std::printf(
				"[dedicated] starter actor-arrival launch-before: clock=%u; "
				"world-time=%u; "
				"interrupted=%u; events=%zu; delayed-hires=%zu; "
				"head-present=%u; head-callback=%u; head-time=%u; "
				"first-delayed-hire-time=%u; transit=%zu; seats=%d\n",
				beforeLaunch.clock, beforeLaunch.worldTime,
				beforeLaunch.timeInterrupted ? 1u : 0u,
				beforeLaunch.strategicEvents,
				beforeLaunch.delayedHiringEvents,
				beforeLaunch.queueHeadPresent ? 1u : 0u,
				beforeLaunch.queueHeadCallback,
				beforeLaunch.queueHeadTimestamp,
				beforeLaunch.firstDelayedHiringTimestamp,
				beforeLaunch.inTransitActors,
				beforeLaunch.queuedHelicopterActors);
			std::fflush(stdout);
			const DedicatedCoopMissionBootstrapError launched =
				LaunchDedicatedCoopStarterMission();
			if (launched != DedicatedCoopMissionBootstrapError::None)
			{
				std::fprintf(stderr,
					"[dedicated] starter mission launch failed: %s\n",
					DedicatedCoopMissionBootstrapErrorName(launched));
				impl_->fail(DedicatedCoopRuntimeError::MissionLaunchFailed,
					"mission.launch",
					DedicatedCoopMissionBootstrapErrorName(launched));
				return;
			}
			const DedicatedCoopActorArrivalDiagnostics afterLaunch =
				CaptureDedicatedCoopActorArrivalDiagnostics();
			std::printf(
				"[dedicated] starter actor-arrival launch-after: clock=%u; "
				"world-time=%u; "
				"interrupted=%u; events=%zu; delayed-hires=%zu; "
				"head-present=%u; head-callback=%u; head-time=%u; "
				"first-delayed-hire-time=%u; active=%zu; in-sector=%zu; "
				"transit=%zu; chopper=%zu; seats=%d\n",
				afterLaunch.clock, afterLaunch.worldTime,
				afterLaunch.timeInterrupted ? 1u : 0u,
				afterLaunch.strategicEvents,
				afterLaunch.delayedHiringEvents,
				afterLaunch.queueHeadPresent ? 1u : 0u,
				afterLaunch.queueHeadCallback,
				afterLaunch.queueHeadTimestamp,
				afterLaunch.firstDelayedHiringTimestamp,
				afterLaunch.activePlayerActors,
				afterLaunch.inSectorActors,
				afterLaunch.inTransitActors,
				afterLaunch.chopperActors,
				afterLaunch.queuedHelicopterActors);
			std::fflush(stdout);
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
			impl_->fail(DedicatedCoopRuntimeError::MissionLaunchFailed,
				"mission.resume-launch",
				DedicatedCoopMissionBootstrapErrorName(launched));
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
			const DedicatedCoopActorArrivalDiagnostics diagnostics =
				CaptureDedicatedCoopActorArrivalDiagnostics();
			char detail[96]{};
			std::snprintf(detail, sizeof(detail),
				"active=%zu,in-sector=%zu,transit=%zu,chopper=%zu,arriving=%zu",
				diagnostics.activePlayerActors, diagnostics.inSectorActors,
				diagnostics.inTransitActors, diagnostics.chopperActors,
				diagnostics.arrivingGameActors);
			std::fprintf(stderr,
				"[dedicated] starter actor-arrival state: active=%zu; "
				"in-sector=%zu; controllable=%zu; in-transit=%zu; "
				"chopper=%zu; arriving-game=%zu; heli-requested=%u; "
				"heli-active=%u; seats=%d; dropped=%d; script=%d; "
				"heli-state=%u; drop-engaged=%u; clock=%u; world-time=%u; "
				"clock-paused=%u; game-paused=%u; screen=%u; "
				"interrupted=%u; events=%zu; delayed-hires=%zu; "
				"head-present=%u; head-callback=%u; head-time=%u; "
				"first-delayed-hire-time=%u\n",
				diagnostics.activePlayerActors, diagnostics.inSectorActors,
				diagnostics.controllableActors,
				diagnostics.inTransitActors, diagnostics.chopperActors,
				diagnostics.arrivingGameActors,
				diagnostics.helicopterRequested ? 1u : 0u,
				diagnostics.helicopterActive ? 1u : 0u,
				diagnostics.queuedHelicopterActors,
				diagnostics.droppedHelicopterActors,
				diagnostics.helicopterScript,
				diagnostics.helicopterState,
				diagnostics.helicopterDropEngaged ? 1u : 0u,
				diagnostics.clock,
				diagnostics.worldTime,
				diagnostics.clockPaused ? 1u : 0u,
				diagnostics.gamePaused ? 1u : 0u,
				diagnostics.screen,
				diagnostics.timeInterrupted ? 1u : 0u,
				diagnostics.strategicEvents,
				diagnostics.delayedHiringEvents,
				diagnostics.queueHeadPresent ? 1u : 0u,
				diagnostics.queueHeadCallback,
				diagnostics.queueHeadTimestamp,
				diagnostics.firstDelayedHiringTimestamp);
			impl_->fail(DedicatedCoopRuntimeError::MissionActorUnavailable,
				"mission.actor-arrival",
				"controllable actors did not arrive before timeout", detail);
		}
		return;
	}
	if (impl_->starterMission == StarterMissionState::Playable)
	{
		const DedicatedCoopPostCombatReturnEvidence evidence =
			impl_->captureSecuredSectorEvidence();
		if (DedicatedCoopPostCombatReturnReady(evidence))
		{
			// Victory secures the sector. Keep it loaded for peaceful play, as the
			// stock game does; the campaign clock stays paused, so no strategic
			// event can fire while the world is loaded. Returning to the map is an
			// explicit shared action that reuses the post-combat unload below.
			if (impl_->lastSecuredSector == UINT32_MAX)
			{
				impl_->fail(DedicatedCoopRuntimeError::InvalidState,
					"mission.secured-sector", "secured-sector identities exhausted");
				return;
			}
			impl_->securedSector = ++impl_->lastSecuredSector;
			impl_->returnToMapRequested = false;
			impl_->starterMission = StarterMissionState::PeacefulTactical;
			StopTimeCompression();
			PauseGame();
			std::printf("[dedicated] co-op sector secured; peaceful tactical play (%u)\n",
				impl_->securedSector);
			std::fflush(stdout);
			if (impl_->hideoutWelcomePending)
			{
				impl_->hideoutWelcomePending = false;
				std::printf("[dedicated] hideout welcome %s\n",
					StartDedicatedCoopHideoutWelcome() ? "started" : "skipped (Carlos absent)");
				std::fflush(stdout);
			}
		}
		return;
	}
	if (impl_->starterMission == StarterMissionState::PeacefulTactical)
	{
		StopTimeCompression();
		PauseGame();
		// The stock hideout cutscene fired: take the squad that followed Fatima
		// down through the checkpointed traversal instead of the native fade.
		if (!impl_->returnToMapRequested && TakeDedicatedCoopHideoutRequest())
		{
			DedicatedCoopExitGridPlan plan;
			const auto code = PlanDedicatedCoopScriptedTraversal(DedicatedCoopGroupNearestToProfile(FATIMA),
				gModSettings.ubHideoutSectorX, gModSettings.ubHideoutSectorY, gModSettings.ubHideoutSectorZ,
				gModSettings.iBasementEntry, std::size(gModSettings.iBasementEntry), plan);
			if (code == DedicatedCoopExitGridCode::Planned)
			{
				impl_->exitGrid = plan;
				impl_->exitGridPending = true;
				impl_->hideoutTraversal = true;
				impl_->returnToMapRequested = true;
				impl_->returnWaitLogged = false;
				std::printf("[dedicated] hideout traversal accepted to (%u,%u,%u) with %zu mercs\n",
					unsigned(plan.x), unsigned(plan.y), unsigned(plan.z), plan.memberCount);
			}
			else
				std::printf("[dedicated] hideout traversal refused (%u); the exit grid stays for the players\n",
					static_cast<unsigned>(code));
			std::fflush(stdout);
		}
		const DedicatedCoopPostCombatReturnEvidence evidence =
			impl_->captureSecuredSectorEvidence();
		const DedicatedCoopPeacefulTacticalStep step =
			EvaluateDedicatedCoopPeacefulTacticalStep(evidence,
				impl_->returnToMapRequested);
		if (step == DedicatedCoopPeacefulTacticalStep::ResumeEncounter)
		{
			// Hostiles are back (reinforcements or a revealed enemy): the sector is
			// no longer secured and the ordinary encounter rules apply again.
			impl_->securedSector = 0;
			impl_->returnToMapRequested = false;
			impl_->sectorSecuredByEntry = false;
			impl_->exitGridPending = false;
			impl_->hideoutTraversal = false;
			impl_->starterMission = StarterMissionState::Playable;
			std::printf("[dedicated] secured sector contested; encounter resumed\n");
			std::fflush(stdout);
			return;
		}
		if (step != DedicatedCoopPeacefulTacticalStep::ReturnToMap)
		{
			// Explain, once per request, what keeps an accepted return waiting.
			if (impl_->returnToMapRequested && !impl_->returnWaitLogged)
			{
				impl_->returnWaitLogged = true;
				const auto& e = evidence;
				std::printf("[dedicated] secured-sector return waiting: playable=%u armed=%u world=%u game-screen=%u "
					"sector=%u won=%u actions=%u interrupt=%u bullets=%u explosions=%u dialogue=%u/%u trigger=%u "
					"autoresolve=%u/%u meanwhile=%u/%u traversal=%u bandage=%u boxing=%u save=%u ui=%u timer=%u\n",
					e.missionPlayable, e.hostileWorldArmed, e.worldLoaded, e.gameScreen, e.validWorldSector, e.lastBattleWon,
					e.tacticalActionsPending, e.interruptPending, e.bulletsPending, e.explosionsPending, e.dialogueActive,
					e.dialogueQueued, e.triggerTimerPending, e.autoResolveActive, e.autoResolvePending, e.meanwhileActive,
					e.meanwhilePending, e.tacticalTraversal, e.autoBandageActive, e.boxingActive, e.saveLoadActive,
					e.uiTransitionPending, e.customTimerPending);
				std::fflush(stdout);
			}
			return;
		}
		impl_->returnWaitLogged = false;
		// Queue every retained campaign receipt before closing transport, then
		// freeze ingress at a fresh-baseline boundary like a battle notice.
		for (const auto& delivery : impl_->campaignActionAuthority.deliveries())
			if (delivery.pending) return;
		if (!impl_->freshAssignmentBaselineBoundary()) return;
		if (impl_->exitGridPending)
		{
			// The accepted plan must still describe this exact world: otherwise
			// abandon the traversal and keep playing in the secured sector.
			DedicatedCoopExitGridPlan fresh;
			if (ReplanDedicatedCoopTraversal(impl_->exitGrid, fresh) != DedicatedCoopExitGridCode::Planned ||
				!SameDedicatedCoopExitGridPlan(fresh, impl_->exitGrid))
			{
				impl_->exitGridPending = false;
				impl_->hideoutTraversal = false;
				impl_->returnToMapRequested = false;
				std::printf("[dedicated] exit-grid traversal abandoned; the group moved before leaving\n");
				std::fflush(stdout);
				return;
			}
		}
		impl_->securedSector = 0;
		impl_->returnToMapRequested = false;
		impl_->starterMission = StarterMissionState::ReturningToStrategic;
		std::printf("[dedicated] secured sector returning to the strategic map\n");
		std::fflush(stdout);
		if (!impl_->stopAdmissionForPostCombatReturn()) return;
		return;
	}
	if (impl_->starterMission == StarterMissionState::ReturningToStrategic)
	{
		const DedicatedCoopPostCombatReturnEvidence rechecked =
			impl_->captureSecuredSectorEvidence();
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
		if (!(impl_->sectorSecuredByEntry ? UnloadCurrentWorldForDedicatedCoopPeacefulReturn()
				: UnloadCurrentWorldForDedicatedCoopPostCombatReturn()))
		{
			impl_->fail(DedicatedCoopRuntimeError::MissionReturnFailed,
				"mission.return-unload", "tactical world unload failed");
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
			impl_->fail(DedicatedCoopRuntimeError::CheckpointNotEligible,
				step == DedicatedCoopPostCombatCheckpointStep::TimedOut
					? "mission.return-checkpoint-timeout" : "mission.return-checkpoint",
				mapReady ? DedicatedCheckpointEligibilityReasonName(eligibility)
					: "native strategic screen exit did not complete");
			return;
		}
		if (!impl_->checkpointNow(context, true)) return;
		if (!impl_->startAdmission()) return;
		impl_->postCombatCheckpointDeadline = {};
		impl_->sectorSecuredByEntry = false;
		impl_->starterMission = StarterMissionState::StrategicIdle;
		std::printf(
			"[dedicated] post-combat strategic checkpoint committed; admission reopened\n");
		std::fflush(stdout);
		if (impl_->exitGridPending)
		{
			impl_->exitGridPending = false;
			const bool hideout = impl_->hideoutTraversal;
			impl_->hideoutTraversal = false;
			StrategicGroupId moved{};
			const auto relocated = ApplyDedicatedCoopExitGridRelocation(impl_->exitGrid, moved);
			if (relocated == DedicatedCoopExitGridCode::Planned && hideout)
			{
				// The stock fade-out's surface edits, now that the surface is unloaded.
				ApplyDedicatedCoopHideoutSurfaceChanges();
				impl_->hideoutWelcomePending = true;
			}
			if (relocated != DedicatedCoopExitGridCode::Planned)
			{
				std::printf("[dedicated] exit-grid relocation refused (%u); the group stays on the map\n",
					static_cast<unsigned>(relocated));
				std::fflush(stdout);
				return;
			}
			std::printf("[dedicated] exit-grid group relocated to (%u,%u,%u)\n", unsigned(impl_->exitGrid.x),
				unsigned(impl_->exitGrid.y), unsigned(impl_->exitGrid.z));
			std::fflush(stdout);
			const auto entered = enterPeacefulSector(moved);
			if (entered != DedicatedCoopPeacefulEntryCode::Entered && !impl_->fatal)
			{
				std::printf("[dedicated] exit-grid destination entry refused (%u); the group waits on the map\n",
					static_cast<unsigned>(entered));
				std::fflush(stdout);
			}
		}
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
		impl_->fail(DedicatedCoopRuntimeError::MissionLaunchFailed, "campaign.arrival.enter", impl_->arrivalDecisions.failure());
	if (result != Result::Entered) return result;
	// Reuse fresh world observation, assignment/baseline admission and native
	// victory/drain handling. Do not leave a loaded encounter marked strategic.
	impl_->starterMission = StarterMissionState::WaitingForControllableActor;
	impl_->sectorSecuredByEntry = false;
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
		impl_->fail(DedicatedCoopRuntimeError::InvalidState, "campaign.arrival.retreat", impl_->arrivalDecisions.failure());
	// Remain strategic and paused. Native movement events own the return trip;
	// only a later explicit time request may advance it.
	return result;
}

DedicatedCoopPeacefulEntryCode DedicatedCoopRuntime::enterPeacefulSector(StrategicGroupId group) noexcept
{
	using Code = DedicatedCoopPeacefulEntryCode;
	if (!impl_ || impl_->error != DedicatedCoopRuntimeError::None || !impl_->campaignEntered ||
		impl_->starterMission != StarterMissionState::StrategicIdle || impl_->worldDraining ||
		!impl_->tactical || impl_->tactical->server.worldActive() || impl_->selfRetirementActive ||
		impl_->arrivalDecisions.front())
		return Code::HostUnavailable;
	const Code result = EnterDedicatedCoopPeacefulSector(group);
	if (result == Code::EntryFailed)
	{
		impl_->fail(DedicatedCoopRuntimeError::MissionLaunchFailed, "campaign.enter-sector",
			"peaceful sector entry failed after native validation");
		return result;
	}
	if (result != Code::Entered) return result;
	// Reuse fresh world observation and assignment/baseline admission. The
	// sector is secured on entry; the ordinary victory/return path applies.
	impl_->starterMission = StarterMissionState::WaitingForControllableActor;
	impl_->minimumControllableActors = 1;
	impl_->postCombatReturnArmed = true;
	impl_->sectorSecuredByEntry = true;
	impl_->starterActorArrivalDeadline = Clock::now() + StarterActorArrivalTimeout;
	std::printf("[dedicated] peaceful sector entered (%d,%d,%d)\n", gWorldSectorX, gWorldSectorY, gbWorldSectorZ);
	std::fflush(stdout);
	return result;
}

bool DedicatedCoopRuntime::shutdownAtCommittedBoundary(
	GameContext& context) noexcept
{
	if (impl_ && context.campaignSimulation().failed())
	{
		impl_->fail(DedicatedCoopRuntimeError::InvalidState, "campaign.simulation",
			context.campaignSimulation().failureReason());
		return false;
	}
	if (!impl_ || !impl_->campaignEntered || impl_->fatal) return !failed();
	if (!impl_->stopAdmissionAndReconcile(100)) return false;
	if (impl_->tactical != nullptr && impl_->tactical->server.active())
	{
		if (impl_->tactical->server.worldActive())
		{
			impl_->fail(DedicatedCoopRuntimeError::CheckpointNotEligible,
				"shutdown.checkpoint", "tactical world is still active");
			return false;
		}
	}
	if (!impl_->checkpointNow(context, true)) return false;
	// Campaign replication owns the admission epoch too, so end it before the
	// tactical coordinator tears down ingress/admission state.
	if (!impl_->endCampaignSync())
	{
		impl_->fail(DedicatedCoopRuntimeError::CampaignSyncFailed,
			"shutdown.campaign-sync",
			CampaignSyncServerResultName(impl_->lastCampaignSyncResult));
		return false;
	}
	if (impl_->tactical != nullptr && impl_->tactical->server.active())
	{
		const CoopSession::FullEngineCoopTacticalServerResult ended =
			impl_->tactical->server.endEpoch();
		if (ended != CoopSession::FullEngineCoopTacticalServerResult::Success)
		{
			impl_->fail(DedicatedCoopRuntimeError::TacticalSessionFailed,
				"shutdown.tactical-epoch", TacticalServerResultName(ended));
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
	impl_->arrivalDecisions.reset();
	UnbindDedicatedCoopAttackState(impl_->nativeAttack);
	impl_->nativeAttack.reset();
	UnbindDedicatedCoopBattleNoticeState(impl_->battleNotice);
	impl_->battleNotice.reset();
	UnbindDedicatedCoopMeanwhileState(impl_->meanwhile);
	impl_->meanwhile.reset();
	UnbindDedicatedCoopSurrenderState(impl_->surrenderDecision);
	impl_->surrenderDecision.reset();
	impl_->lastArrivalDecisionLogged = 0;
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

const CoopRuntimeFailureDiagnostic&
DedicatedCoopRuntime::failureDiagnostic() const noexcept
{
	static const CoopRuntimeFailureDiagnostic unavailable;
	return impl_ ? impl_->failureDiagnostic : unavailable;
}

const char* DedicatedCoopRuntime::bindAddress() const noexcept
{
	return impl_ ? impl_->options.coopBindAddress.c_str() : "";
}

std::uint16_t DedicatedCoopRuntime::bindPort() const noexcept
{
	return impl_ ? impl_->options.coopPort : 0;
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
