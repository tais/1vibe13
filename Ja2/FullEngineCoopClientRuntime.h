#ifndef JA2_FULL_ENGINE_COOP_CLIENT_RUNTIME_H
#define JA2_FULL_ENGINE_COOP_CLIENT_RUNTIME_H

#include "FullEngineCoopClientOptions.h"

#include <Multiplayer/CoopCampaignBootstrapProtocol.h>
#include <Multiplayer/CoopTacticalIntent.h>
#include <Multiplayer/CoopTacticalProtocol.h>
#include <Multiplayer/FullEngineCoopClient.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>

class GameContext;
class TacticalWorldSnapshot;

namespace CoopSession
{
class FullEngineCoopSnapshotReplica;
}

// Borrowed, main-thread-only view of the last transactionally committed
// replica. The snapshot pointer remains valid only until the next runtime pump.
// All other fields are copied so presentation code never reaches into the
// transport/core composition or any authoritative JA2 state.
struct FullEngineCoopClientPresentationView
{
	const TacticalWorldSnapshot* snapshot = nullptr;
	std::array<TacticalEntityId,
		CoopSession::MaximumCoopTacticalAssignedActors> assignedActors{};
	std::size_t assignedActorCount = 0;
	std::uint64_t outstandingCommandId = 0;
	CoopSession::CoopTacticalStateIdentity state{};
	CoopSession::CoopTacticalIntentReceipt lastReceipt{};
	bool hasLastReceipt = false;
	bool resynchronizing = false;

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
	}

	void markTransportStopped() noexcept
	{
		networkOpen_ = false;
		campaignReady_ = false;
	}

	bool markRetired() noexcept
	{
		if (failed() || retired_ || !networkOpen_) return false;
		retired_ = true;
		campaignReady_ = false;
		return true;
	}

	bool markDurablyRetired() noexcept
	{
		if (failed() || retired_ || !prepared_ || networkOpen_ || leaseClosed_)
			return false;
		retired_ = true;
		campaignReady_ = false;
		return true;
	}

	bool markLeaseClosed() noexcept
	{
		if (networkOpen_) return false;
		prepared_ = false;
		campaignReady_ = false;
		leaseClosed_ = true;
		return true;
	}

	void fail(FullEngineCoopClientRuntimeError incoming) noexcept
	{
		if (retired_) return;
		if (error_ == FullEngineCoopClientRuntimeError::None)
			error_ = incoming;
		campaignReady_ = false;
	}

	bool prepared() const noexcept { return prepared_; }
	bool networkOpen() const noexcept { return networkOpen_; }
	bool campaignReady() const noexcept { return campaignReady_; }
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

private:
	FullEngineCoopClientRuntimeError error_ =
		FullEngineCoopClientRuntimeError::None;
	bool prepared_ = false;
	bool networkOpen_ = false;
	bool campaignReady_ = false;
	bool leaseClosed_ = false;
	bool retired_ = false;
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
	bool retired() const noexcept;
	bool selfRetirementPending() const noexcept;
	bool failed() const noexcept;
	FullEngineCoopClientRuntimeError error() const noexcept;
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
const char* FullEngineCoopClientRuntimeErrorName(
	FullEngineCoopClientRuntimeError error) noexcept;

#endif
