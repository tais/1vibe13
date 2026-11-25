#ifndef JA2_FULL_ENGINE_COOP_CLIENT_RUNTIME_H
#define JA2_FULL_ENGINE_COOP_CLIENT_RUNTIME_H

#include "CoopRuntimeFailureDiagnostic.h"
#include "FullEngineCoopClientOptions.h"
#include "FullEngineCoopClientPresentationMapPlan.h"

#include <Multiplayer/CoopCampaignBootstrapProtocol.h>
#include <Multiplayer/CoopCampaignAction.h>
#include <Multiplayer/CoopCampaignHire.h>
#include <Multiplayer/CoopInventoryProtocol.h>
#include <Multiplayer/CoopTacticalIntent.h>
#include <Multiplayer/CoopTacticalProtocol.h>
#include <Multiplayer/FullEngineCoopClient.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <utility>

class GameContext;
class TacticalWorldSnapshot;

namespace CoopSession
{
class FullEngineCoopSnapshotReplica;
}

// Owns one fully parsed local map asset and binds it to the exact replicated
// world identity that authorized the read. Failed attempts retain only their
// identity/result so a bad asset is not reopened every frame; no failed,
// stale, or resynchronizing identity can borrow the previous owning plan.
class FullEngineCoopClientPresentationMapPlanBinding final
{
public:
	void clear() noexcept
	{
		plan_ = FullEngineCoopClientPresentationMapPlan{};
		sector_ = {};
		dimensions_ = {};
		sessionEpoch_ = 0;
		worldGeneration_ = 0;
		result_ = FullEngineCoopClientPresentationMapPlanResult::InvalidSnapshot;
		attempted_ = false;
		verified_ = false;
	}

	bool beginAttempt(std::uint64_t sessionEpoch,
		std::uint64_t worldGeneration,
		const TacticalWorldSnapshot& snapshot) noexcept
	{
		clear();
		if (sessionEpoch == 0 || worldGeneration == 0 ||
			snapshot.epoch() != worldGeneration ||
			!snapshot.dimensions().valid() || !snapshot.sector().loaded ||
			!IsValidTacticalSectorSnapshot(snapshot.sector()))
			return false;
		sessionEpoch_ = sessionEpoch;
		worldGeneration_ = worldGeneration;
		sector_ = snapshot.sector();
		dimensions_ = snapshot.dimensions();
		attempted_ = true;
		return true;
	}

	bool attemptedIdentityMatches(std::uint64_t sessionEpoch,
		std::uint64_t worldGeneration,
		const TacticalWorldSnapshot& snapshot) const noexcept
	{
		return attempted_ && sessionEpoch_ == sessionEpoch &&
			worldGeneration_ == worldGeneration &&
			snapshot.epoch() == worldGeneration && sameSector(sector_,
				snapshot.sector()) && sameDimensions(dimensions_,
				snapshot.dimensions());
	}

	bool commit(FullEngineCoopClientPresentationMapPlan&& plan) noexcept
	{
		if (!attempted_ || plan.bytes().empty() ||
			!sameSector(sector_, plan.sector()) ||
			!sameDimensions(dimensions_, plan.dimensions()))
			return false;
		plan_ = std::move(plan);
		result_ = FullEngineCoopClientPresentationMapPlanResult::Success;
		verified_ = true;
		return true;
	}

	void recordFailure(
		FullEngineCoopClientPresentationMapPlanResult result) noexcept
	{
		plan_ = FullEngineCoopClientPresentationMapPlan{};
		verified_ = false;
		result_ = result ==
			FullEngineCoopClientPresentationMapPlanResult::Success
			? FullEngineCoopClientPresentationMapPlanResult::InvalidSnapshot
			: result;
	}

	const FullEngineCoopClientPresentationMapPlan* borrow(
		std::uint64_t sessionEpoch, std::uint64_t worldGeneration,
		const TacticalWorldSnapshot& snapshot,
		bool resynchronizing) const noexcept
	{
		return !resynchronizing && verified_ &&
			result_ == FullEngineCoopClientPresentationMapPlanResult::Success &&
			attemptedIdentityMatches(sessionEpoch, worldGeneration, snapshot) &&
			sameSector(plan_.sector(), snapshot.sector()) &&
			sameDimensions(plan_.dimensions(), snapshot.dimensions())
			? &plan_ : nullptr;
	}

	FullEngineCoopClientPresentationMapPlanResult result() const noexcept
	{
		return result_;
	}
	bool attempted() const noexcept { return attempted_; }
	bool verified() const noexcept { return verified_; }

private:
	static bool sameDimensions(const TacticalWorldDimensions& left,
		const TacticalWorldDimensions& right) noexcept
	{
		return left.columns == right.columns && left.rows == right.rows;
	}

	static bool sameSector(const TacticalSectorSnapshot& left,
		const TacticalSectorSnapshot& right) noexcept
	{
		return left.x == right.x && left.y == right.y && left.z == right.z &&
			left.loaded == right.loaded && left.mapAssetKey == right.mapAssetKey;
	}

	FullEngineCoopClientPresentationMapPlan plan_;
	TacticalSectorSnapshot sector_;
	TacticalWorldDimensions dimensions_;
	std::uint64_t sessionEpoch_ = 0;
	std::uint64_t worldGeneration_ = 0;
	FullEngineCoopClientPresentationMapPlanResult result_ =
		FullEngineCoopClientPresentationMapPlanResult::InvalidSnapshot;
	bool attempted_ = false;
	bool verified_ = false;
};

// Borrowed, main-thread-only view of the last transactionally committed
// replica and authenticated owner cache. The snapshot, mapPlan and owner
// inventory pointers remain valid only until the next runtime pump or runtime
// reset. All other fields are copied so presentation code never reaches into
// the transport/core composition or authoritative JA2 state.
struct FullEngineCoopClientPresentationView
{
	const TacticalWorldSnapshot* snapshot = nullptr;
	const FullEngineCoopClientPresentationMapPlan* mapPlan = nullptr;
	std::array<TacticalEntityId,
		CoopSession::MaximumCoopTacticalAssignedActors> assignedActors{};
	std::size_t assignedActorCount = 0;
	std::uint64_t outstandingCommandId = 0;
	CoopSession::CoopTacticalStateIdentity state{};
	CoopSession::CoopTacticalIntentReceipt lastReceipt{};
	bool hasLastReceipt = false;
	bool resynchronizing = false;
	// True only after the exact authority-published basename resolved through
	// the already manifest-matched VFS and its map dimensions matched this
	// committed snapshot. The same identity also owns mapPlan;
	// geometry and actor proxies are separate later gates.
	bool exactMapAssetIdentityVerified = false;
	// Borrowed current-baseline owner summaries, parallel to assignedActors.
	// No native object pointers or other peers' inventories enter this view.
	std::array<const CoopSession::CoopOwnerInventorySnapshot*,
		CoopSession::MaximumCoopTacticalAssignedActors> ownerInventories{};
	const CoopSession::CoopOwnerInventorySnapshot* inventoryFor(TacticalEntityId actor) const noexcept
	{
		if (resynchronizing || !snapshot || !actor.valid() ||
			assignedActorCount > assignedActors.size() || state.sessionEpoch == 0 ||
			snapshot->epoch() != state.worldGeneration) return nullptr;
		for (std::size_t index = 0; index < assignedActorCount; ++index)
			if (assignedActors[index] == actor)
			{
				const auto* inventory = ownerInventories[index];
				return inventory && inventory->actor == actor &&
					inventory->sessionEpoch == state.sessionEpoch &&
					inventory->worldGeneration == state.worldGeneration &&
					inventory->inventoryRevision != 0 && inventory->baselineId != 0 ? inventory : nullptr;
			}
		return nullptr;
	}

	bool ready() const noexcept { return snapshot != nullptr; }
};

enum class FullEngineCoopClientRuntimeError : std::uint8_t
{
	None,
	NotClientProcess,
	InvalidState,
	BootstrapConnectFailed,
	BootstrapFailed,
	BootstrapCancelled,
	DescriptorUnavailable,
	StateDirectoryInvalid,
	ScratchPrepareFailed,
	SimulationRandomInstallFailed,
	ContentManifestFailed,
	RuntimeFingerprintFailed,
	CompatibilityMismatch,
	ClientConfigurationFailed,
	ReconnectCredentialLoadFailed,
	ReconnectCredentialRestoreFailed,
	ReconnectCredentialStorageFailed,
	TransportConnectFailed,
	LiveServerDescriptorMismatch,
	CampaignSyncFailed,
	TacticalBeforeCampaignReady,
	TacticalClientFailed,
	ReconnectLimitReached
};

const char* FullEngineCoopClientRuntimeErrorName(
	FullEngineCoopClientRuntimeError error) noexcept;

enum class FullEngineCoopClientConnectionPhase : std::uint8_t
{
	OpeningSession,
	Reconnecting,
	SynchronizingCampaign,
	AwaitingBaseline,
	Ready,
	Retired,
	Failed
};

// Allocation-free lifecycle ledger shared by the production composition and
// its focused tests. It makes the two teardown boundaries explicit: stopping
// transport never releases the profile lease, and closing the lease is rejected
// until transport/composition ownership has been retired.
class FullEngineCoopClientRuntimeLifecycle final
{
public:
	bool markPrepared() noexcept
	{
		if (failed() || prepared_ || leaseClosed_) return false;
		prepared_ = true;
		return true;
	}

	bool markNetworkOpen() noexcept
	{
		if (failed() || retired_ || !prepared_ || networkOpen_ || leaseClosed_)
			return false;
		networkOpen_ = true;
		return true;
	}

	void markCampaignReady(bool ready) noexcept
	{
		campaignReady_ = ready && !failed() && networkOpen_;
		if (campaignReady_) reconnecting_ = false;
	}

	// networkOpen names composition ownership, not a connected socket. Keep
	// interrupted transport visible throughout its retry/admission interval.
	// Returning true identifies the transition, so repeated retries stay quiet.
	bool markReconnecting() noexcept
	{
		if (!networkOpen_ || failed() || retired_ || reconnecting_) return false;
		campaignReady_ = false;
		reconnecting_ = true;
		return true;
	}

	void markTransportStopped() noexcept
	{
		networkOpen_ = false;
		campaignReady_ = false;
		reconnecting_ = false;
	}

	bool markRetired() noexcept
	{
		if (failed() || retired_ || !networkOpen_) return false;
		retired_ = true;
		campaignReady_ = false;
		reconnecting_ = false;
		return true;
	}

	bool markDurablyRetired() noexcept
	{
		if (failed() || retired_ || !prepared_ || networkOpen_ || leaseClosed_)
			return false;
		retired_ = true;
		campaignReady_ = false;
		reconnecting_ = false;
		return true;
	}

	bool markLeaseClosed() noexcept
	{
		if (networkOpen_) return false;
		prepared_ = false;
		campaignReady_ = false;
		reconnecting_ = false;
		leaseClosed_ = true;
		return true;
	}

	void fail(FullEngineCoopClientRuntimeError incoming,
		const char* stage = "client.runtime",
		const char* reason = "client runtime failure",
		const char* detail = "none") noexcept
	{
		if (incoming == FullEngineCoopClientRuntimeError::None) return;
		if (retired_) return;
		if (error_ == FullEngineCoopClientRuntimeError::None)
		{
			error_ = incoming;
			failureDiagnostic_.capture(stage, reason, detail);
		}
		campaignReady_ = false;
	}

	bool prepared() const noexcept { return prepared_; }
	bool networkOpen() const noexcept { return networkOpen_; }
	bool campaignReady() const noexcept { return campaignReady_; }
	bool reconnecting() const noexcept { return reconnecting_; }
	bool leaseClosed() const noexcept { return leaseClosed_; }
	bool retired() const noexcept { return retired_; }
	bool failed() const noexcept
	{
		return error_ != FullEngineCoopClientRuntimeError::None;
	}
	FullEngineCoopClientRuntimeError error() const noexcept
	{
		return error_;
	}
	const CoopRuntimeFailureDiagnostic& failureDiagnostic() const noexcept
	{
		return failureDiagnostic_;
	}
	bool snapshotPublishable(bool tacticalReplicaReadable) const noexcept
	{
		return campaignReady_ && networkOpen_ && tacticalReplicaReadable &&
			!failed();
	}
	bool reconnectAllowed(bool sameDescriptorEpoch,
		bool credentialsRetained, unsigned attempts,
		unsigned maximumAttempts) const noexcept
	{
		if (!prepared_ || !networkOpen_ || campaignReady_ || retired_ || failed())
			return false;
		// A durable same-epoch bearer names an already admitted server seat. The
		// authority deliberately stops its listener across world unload/checkpoint
		// boundaries, whose duration is not bounded by a connection-attempt budget.
		if (credentialsRetained) return sameDescriptorEpoch;
		return maximumAttempts != 0 && attempts < maximumAttempts;
	}
	FullEngineCoopClientConnectionPhase connectionPhase(
		CoopSession::FullEngineCoopClientState clientState) const noexcept
	{
		using Phase = FullEngineCoopClientConnectionPhase;
		using State = CoopSession::FullEngineCoopClientState;
		if (failed() || clientState == State::Failed) return Phase::Failed;
		if (retired_ || clientState == State::Retired) return Phase::Retired;
		if (!networkOpen_) return Phase::OpeningSession;
		const bool admitted = clientState == State::AwaitingBaseline ||
			clientState == State::Active || clientState == State::Retiring ||
			clientState == State::ResyncRequired;
		if (!admitted)
			return reconnecting_ ? Phase::Reconnecting : Phase::OpeningSession;
		if (!campaignReady_) return Phase::SynchronizingCampaign;
		return clientState == State::Active ? Phase::Ready : Phase::AwaitingBaseline;
	}

private:
	FullEngineCoopClientRuntimeError error_ =
		FullEngineCoopClientRuntimeError::None;
	CoopRuntimeFailureDiagnostic failureDiagnostic_;
	bool prepared_ = false;
	bool networkOpen_ = false;
	bool campaignReady_ = false;
	bool reconnecting_ = false;
	bool leaseClosed_ = false;
	bool retired_ = false;
};

// Process-local operational marker gate. Temporary loss of transport or a
// same-world replacement baseline must not duplicate the readiness marker;
// the next authoritative world generation reports once after its committed
// baseline carries at least one assigned actor.
class FullEngineCoopClientTacticalReadyMarker final
{
public:
	bool observe(bool campaignReady, bool tacticalActive,
		bool snapshotReadable, std::size_t assignedActorCount,
		std::uint64_t worldGeneration) noexcept
	{
		if (!campaignReady || !tacticalActive || !snapshotReadable ||
			assignedActorCount == 0 || worldGeneration == 0)
			return false;
		if (reported_ && worldGeneration == reportedWorldGeneration_)
			return false;
		reported_ = true;
		reportedWorldGeneration_ = worldGeneration;
		return true;
	}

private:
	std::uint64_t reportedWorldGeneration_ = 0;
	bool reported_ = false;
};

// The passive full-engine client composition is deliberately split across two
// lifetime boundaries. prepareEarly() runs after SDL is available but before
// InitializeRandom/GetGameContext/VFS discovery. stopTransport() destroys every
// socket/core adapter while GameContext and SDL are still alive. Only the later
// closeAfterVfs() releases the private profile lease.
class FullEngineCoopClientRuntime final
{
public:
	using CancellationRequested = bool (*)() noexcept;

	FullEngineCoopClientRuntime() noexcept;
	~FullEngineCoopClientRuntime() noexcept;
	FullEngineCoopClientRuntime(
		const FullEngineCoopClientRuntime&) = delete;
	FullEngineCoopClientRuntime& operator=(
		const FullEngineCoopClientRuntime&) = delete;

	bool prepareEarly(
		CancellationRequested cancellationRequested = nullptr) noexcept;
	// Called once after package profiles are mounted and before legacy game
	// initialization may publish derived files into the writable profile.
	bool captureContentManifestAfterPackageMount() noexcept;
	bool openAfterPackageBootstrap(GameContext& context) noexcept;

	// Called only after an ordinary FrameDriver commit or the passive client's
	// render-only shell boundary. The socket callback FIFO, campaign synchronizer,
	// and reconnect policy are all advanced within that main-thread boundary.
	void pumpAfterCommittedFrame() noexcept;

	void stopTransport() noexcept;
	void closeAfterVfs() noexcept;

	bool prepared() const noexcept;
	bool networkOpen() const noexcept;
	bool campaignReady() const noexcept;
	// Copied friendly-group observation, independent of tactical geometry.
	// True means a current message exists; inspect output.available separately.
	bool campaignGroups(CoopSession::CoopCampaignGroups& output) const noexcept;
	bool campaignEconomy(CoopSession::CoopCampaignEconomy& output) const noexcept;
	bool campaignAimQuotes(CoopSession::CoopCampaignAimQuotes& output) const noexcept;
	bool conversation(CoopSession::CoopConversation& output) const noexcept;
	// True when the open conversation may be answered by this player: its
	// initiator, or the owner of its merc when native gameplay opened it.
	bool conversationAnswerable(const CoopSession::CoopConversation& conversation) const noexcept;
	// Display label from the synchronized passive checkpoint; never offer,
	// price or hiring authority. Empty/invalid labels use a caller fallback.
	bool campaignProfileNickname(std::uint16_t profile, std::array<wchar_t,32>& output) const noexcept;
	CoopSession::FullEngineCoopClientResult requestCampaignHire(std::uint16_t profile, std::uint8_t days, bool buyGear) noexcept;
	bool campaignHireFeedback(CoopSession::CoopCampaignHireResult& output, bool& pending) const noexcept;
	// Copied server observation, independent of tactical geometry/baseline.
	// Never exposes the paused local checkpoint's clock as live campaign time.
	bool campaignStatus(CoopSession::CoopCampaignStatus& output,
		bool& localTimeLeader) const noexcept;
	CoopSession::FullEngineCoopClientResult requestCampaignTime(CoopSession::CoopCampaignTimeAction action) noexcept;
	bool campaignTimeFeedback(CoopSession::CoopCampaignTimeResult& output, bool& pending) const noexcept;
	CoopSession::FullEngineCoopClientResult requestCampaignAction(const CoopSession::CoopCampaignActionRequest& request) noexcept;
	bool campaignActionFeedback(CoopSession::CoopCampaignActionResult& output, bool& pending) const noexcept;
	FullEngineCoopClientConnectionPhase connectionPhase() const noexcept;
	bool retired() const noexcept;
	bool selfRetirementPending() const noexcept;
	bool failed() const noexcept;
	FullEngineCoopClientRuntimeError error() const noexcept;
	const CoopRuntimeFailureDiagnostic& failureDiagnostic() const noexcept;
	const char* serverHost() const noexcept;
	std::uint16_t serverPort() const noexcept;
	const std::filesystem::path& profileDirectory() const noexcept;
	const CoopSession::CoopCampaignBootstrapDescriptor& descriptor()
		const noexcept;
	const CoopSession::FullEngineCoopSnapshotReplica* snapshotReplica()
		const noexcept;
	bool presentationView(
		FullEngineCoopClientPresentationView& output) const noexcept;
	CoopSession::FullEngineCoopClientResult sendIntent(
		TacticalEntityId actor,
		const CoopSession::TacticalIntentPayload& payload) noexcept;
	CoopSession::FullEngineCoopClientResult requestSelfRetirement() noexcept;

private:
	struct Impl;
	Impl* impl_ = nullptr;
};

FullEngineCoopClientRuntime& GetFullEngineCoopClientRuntime() noexcept;

#endif
