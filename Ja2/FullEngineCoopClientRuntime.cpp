#include "FullEngineCoopClientRuntime.h"

#include "DedicatedContentManifest.h"
#include "FullEngineCoopClientCampaignScratch.h"
#include "FullEngineCoopClientOptions.h"
#include "FullEngineCoopClientPresentationMapPlan.h"
#include "GameContext.h"
#include "random.h"

#include <Multiplayer/FullEngineCoopCampaignSyncClient.h>
#include <Multiplayer/FullEngineCoopClient.h>
#include <Multiplayer/FullEngineCoopClientBootstrapTransport.h>
#include <Multiplayer/FullEngineCoopClientTransport.h>
#include <Multiplayer/FullEngineCoopSnapshotReplica.h>

#include <vfs/Core/vfs.h>

#include <chrono>
#include <cstdio>
#include <limits>
#include <memory>
#include <new>
#include <thread>
#include <utility>

namespace
{
constexpr unsigned LiveTransportDrainMilliseconds = 100;
constexpr unsigned MaximumReconnectAttempts = 8;
constexpr auto ReconnectDelay = std::chrono::seconds(1);

using MonotonicClock = std::chrono::steady_clock;

CoopSession::RuntimeCompatibilityFingerprint AdmissionFingerprint(
	const RuntimeCompatibilityFingerprint& fingerprint) noexcept
{
	return CoopSession::RuntimeCompatibilityFingerprint{
		fingerprint.schema, fingerprint.high, fingerprint.low};
}

bool CampaignResultKeepsTransportOpen(
	CoopSession::FullEngineCoopCampaignSyncClientResult result) noexcept
{
	using Result =
		CoopSession::FullEngineCoopCampaignSyncClientResult;
	return result == Result::Success ||
		result == Result::Backpressured ||
		result == Result::ResyncRequested ||
		result == Result::StaleMessage;
}

const char* BootstrapConnectResultName(
	CoopSession::FullEngineCoopClientBootstrapTransportConnectResult result)
	noexcept
{
	using Result =
		CoopSession::FullEngineCoopClientBootstrapTransportConnectResult;
	switch (result)
	{
		case Result::Success: return "success";
		case Result::InvalidConfiguration: return "invalid configuration";
		case Result::WrongThread: return "wrong thread";
		case Result::LifecycleBusy: return "lifecycle busy";
		case Result::TransportUnavailable: return "transport unavailable";
		case Result::TransportStartFailed: return "transport start failed";
		case Result::TransportConnectFailed: return "connection attempt could not start";
	}
	return "unknown bootstrap connect result";
}

const char* BootstrapResultName(
	CoopSession::FullEngineCoopClientBootstrapTransportResult result) noexcept
{
	using Result = CoopSession::FullEngineCoopClientBootstrapTransportResult;
	switch (result)
	{
		case Result::None: return "no terminal result";
		case Result::Success: return "success";
		case Result::Stopped: return "stopped";
		case Result::WrongThread: return "wrong thread";
		case Result::ConnectionAttemptFailed: return "connection attempt failed";
		case Result::ConnectionLost: return "connection lost";
		case Result::Timeout: return "handshake timed out";
		case Result::InboundCapacityReached: return "inbound capacity reached";
		case Result::InboundWrongSize: return "inbound message had wrong size";
		case Result::WrongMessageOrder: return "wrong handshake message order";
		case Result::HelloDecodeFailed: return "server hello decode failed";
		case Result::BootstrapDecodeFailed: return "campaign descriptor decode failed";
		case Result::DescriptorMismatch: return "hello and campaign descriptor mismatch";
		case Result::UnexpectedConnection: return "unexpected connection";
		case Result::UnexpectedTransportEvent: return "unexpected transport event";
		case Result::PendingWriteLimit: return "pending write limit reached";
		case Result::TransportFailure: return "transport failure";
	}
	return "unknown bootstrap result";
}

const char* ScratchPrepareResultName(
	CoopSession::FullEngineCoopClientCampaignScratchPrepareResult result) noexcept
{
	using Result =
		CoopSession::FullEngineCoopClientCampaignScratchPrepareResult;
	switch (result)
	{
		case Result::Success: return "success";
		case Result::AlreadyPrepared: return "already prepared";
		case Result::InvalidConfiguration: return "invalid configuration";
		case Result::IdentityMismatch: return "campaign identity mismatch";
		case Result::UnsafeProfile: return "unsafe private profile";
		case Result::LeaseHeld: return "private profile lease held";
		case Result::StorageFailure: return "private profile storage failure";
	}
	return "unknown scratch preparation result";
}

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

const char* CredentialLoadResultName(
	CoopSession::FullEngineCoopReconnectCredentialLoadResult result) noexcept
{
	using Result = CoopSession::FullEngineCoopReconnectCredentialLoadResult;
	switch (result)
	{
		case Result::Loaded: return "loaded";
		case Result::Missing: return "missing";
		case Result::Retired: return "retired";
		case Result::StaleSession: return "stale server session";
		case Result::InvalidState: return "invalid scratch state";
		case Result::CorruptRecord: return "corrupt credential record";
		case Result::BindingMismatch: return "credential binding mismatch";
		case Result::UnsafeStorage: return "unsafe credential storage";
		case Result::StorageFailure: return "credential storage failure";
	}
	return "unknown credential load result";
}

const char* ClientResultName(
	CoopSession::FullEngineCoopClientResult result) noexcept
{
	using Result = CoopSession::FullEngineCoopClientResult;
	switch (result)
	{
		case Result::Success: return "success";
		case Result::InvalidConfiguration: return "invalid configuration";
		case Result::InvalidState: return "invalid state";
		case Result::InvalidMessage: return "invalid message";
		case Result::CompatibilityMismatch: return "compatibility mismatch";
		case Result::AdmissionRejected: return "admission rejected";
		case Result::WireFailure: return "wire send failure";
		case Result::ResyncRequired: return "tactical resynchronization required";
		case Result::IntentOutstanding: return "intent already outstanding";
		case Result::ActorNotAssigned: return "actor not assigned";
		case Result::InvalidIntent: return "invalid tactical intent";
		case Result::SequenceExhausted: return "command sequence exhausted";
		case Result::AllocationFailure: return "allocation failure";
		case Result::CredentialStorageFailure: return "credential storage failure";
		case Result::CredentialRetirementPending: return "credential retirement pending";
		case Result::SelfRetirementRejected: return "self-retirement rejected";
		case Result::CredentialRetired: return "credential retired";
	}
	return "unknown client result";
}

const char* AdmissionRejectReasonName(
	CoopSession::AdmissionRejectReason reason) noexcept
{
	using Reason = CoopSession::AdmissionRejectReason;
	switch (reason)
	{
		case Reason::None: return "none";
		case Reason::MalformedRequest: return "malformed request";
		case Reason::AuthorityDisabled: return "authority disabled";
		case Reason::ConfigurationIncomplete: return "server configuration incomplete";
		case Reason::UnsupportedProtocol: return "unsupported protocol";
		case Reason::SessionEpochMismatch: return "server session changed";
		case Reason::RuntimeCompatibilityMismatch: return "runtime compatibility mismatch";
		case Reason::ContentManifestMismatch: return "installed content mismatch";
		case Reason::InvalidTransport: return "invalid transport";
		case Reason::InvalidPeerBinding: return "invalid peer binding";
		case Reason::UnknownPeer: return "reconnect identity is unknown to server";
		case Reason::InvalidReconnectToken: return "reconnect credential rejected";
		case Reason::TransportAlreadyBound: return "peer already connected";
		case Reason::CapacityReached: return "server peer capacity reached";
		case Reason::TokenSourceUnavailable: return "server credential source unavailable";
		case Reason::TokenIssuanceFailed: return "server credential issuance failed";
		case Reason::CredentialRetired: return "credential retired";
		case Reason::CredentialRetirementPending: return "credential retirement pending";
	}
	return "unknown admission rejection";
}

const char* TransportConnectResultName(
	CoopSession::FullEngineCoopClientTransportConnectResult result) noexcept
{
	using Result = CoopSession::FullEngineCoopClientTransportConnectResult;
	switch (result)
	{
		case Result::Success: return "success";
		case Result::InvalidConfiguration: return "invalid configuration";
		case Result::WrongThread: return "wrong thread";
		case Result::LifecycleBusy: return "lifecycle busy";
		case Result::ClientRejected: return "client core rejected connection start";
		case Result::TransportUnavailable: return "transport unavailable";
		case Result::TransportStartFailed: return "transport start failed";
		case Result::TransportConnectFailed: return "connection attempt could not start";
	}
	return "unknown live transport connect result";
}

const char* TransportFailureName(
	CoopSession::FullEngineCoopClientTransportFailure failure) noexcept
{
	using Failure = CoopSession::FullEngineCoopClientTransportFailure;
	switch (failure)
	{
		case Failure::None: return "none";
		case Failure::WrongThread: return "wrong thread";
		case Failure::ConnectionAttemptFailed: return "connection attempt failed";
		case Failure::ConnectionLost: return "connection lost";
		case Failure::InboundCapacityReached: return "inbound capacity reached";
		case Failure::InboundMessageTooLarge: return "inbound message too large";
		case Failure::UnexpectedConnection: return "unexpected connection";
		case Failure::UnexpectedTransportEvent: return "unexpected transport event";
		case Failure::PendingWriteLimit: return "pending write limit reached";
		case Failure::TransportFailure: return "transport failure";
		case Failure::ClientRejected: return "client rejected inbound message";
	}
	return "unknown live transport failure";
}

const char* CampaignClientResultName(
	CoopSession::FullEngineCoopCampaignSyncClientResult result) noexcept
{
	using Result = CoopSession::FullEngineCoopCampaignSyncClientResult;
	switch (result)
	{
		case Result::Success: return "success";
		case Result::Backpressured: return "wire backpressured";
		case Result::ResyncRequested: return "campaign resynchronization requested";
		case Result::StaleMessage: return "stale transfer message";
		case Result::InvalidState: return "invalid state";
		case Result::InvalidConfiguration: return "invalid configuration";
		case Result::InvalidMessage: return "invalid message";
		case Result::DescriptorMismatch: return "campaign descriptor mismatch";
		case Result::SequenceMismatch: return "campaign chunk sequence mismatch";
		case Result::StorageFailure: return "campaign storage failure";
		case Result::CommitFailed: return "campaign checkpoint commit or load failed";
		case Result::ServerRejected: return "server rejected campaign transfer";
		case Result::WireFailure: return "campaign wire send failure";
		case Result::ReentrantCall: return "reentrant campaign call";
	}
	return "unknown campaign synchronization result";
}

const char* CampaignFailureReasonName(
	CoopSession::CoopCampaignSyncFailureReason reason) noexcept
{
	using Reason = CoopSession::CoopCampaignSyncFailureReason;
	switch (reason)
	{
		case Reason::None: return "none";
		case Reason::MalformedMessage: return "malformed campaign message";
		case Reason::SessionMismatch: return "campaign session mismatch";
		case Reason::TransferMismatch: return "campaign transfer mismatch";
		case Reason::CheckpointMismatch: return "campaign checkpoint mismatch";
		case Reason::SequenceMismatch: return "campaign chunk sequence mismatch";
		case Reason::ChunkChecksumMismatch: return "campaign chunk checksum mismatch";
		case Reason::HashMismatch: return "campaign checkpoint hash mismatch";
		case Reason::StorageFailure: return "campaign storage failure";
		case Reason::LoadFailed: return "campaign checkpoint load failed";
		case Reason::CompatibilityMismatch: return "campaign compatibility mismatch";
		case Reason::CapacityReached: return "campaign transfer capacity reached";
		case Reason::Superseded: return "campaign checkpoint superseded";
		case Reason::ProtocolViolation: return "campaign protocol violation";
	}
	return "unknown campaign failure reason";
}
}

struct FullEngineCoopClientRuntime::Impl
{
	class CampaignWire final :
		public CoopSession::FullEngineCoopCampaignSyncClientWire
	{
	public:
		explicit CampaignWire(
			CoopSession::FullEngineCoopClientTransport& transport) noexcept
			: transport_(transport)
		{
		}

		bool send(const char* messageName, const std::uint8_t* bytes,
			std::size_t size) noexcept override
		{
			return transport_.send(messageName, bytes, size);
		}

		void close() noexcept override
		{
			// A transport-originated disconnect has already retired its peer and
			// detached the tactical core. Do not manufacture a new close-pending
			// lifecycle on that stopped adapter before a safe reconnect.
			if (transport_.running()) transport_.close();
		}

	private:
		CoopSession::FullEngineCoopClientTransport& transport_;
	};

	class ReplicaGate final :
		public CoopSession::FullEngineCoopPassiveReplicaSink
	{
	public:
		ReplicaGate(Impl& owner,
			CoopSession::FullEngineCoopCampaignSyncClient& campaign,
			CoopSession::FullEngineCoopSnapshotReplica& replica) noexcept
			: owner_(owner), campaign_(campaign), replica_(replica)
		{
		}

		CoopSession::FullEngineCoopReplicaApplyResult applyBaseline(
			const CoopSession::CoopTacticalBaseline& baseline) noexcept override
		{
			if (campaign_.state() !=
				CoopSession::FullEngineCoopCampaignSyncClientState::Ready)
			{
				owner_.deferFailure(
					FullEngineCoopClientRuntimeError::
						TacticalBeforeCampaignReady,
					"tactical.baseline.apply",
					"campaign checkpoint is not ready");
				return CoopSession::FullEngineCoopReplicaApplyResult::Rejected;
			}
			return replica_.applyBaseline(baseline);
		}

		CoopSession::FullEngineCoopReplicaApplyResult applyDelta(
			const CoopSession::CoopTacticalDelta& delta) noexcept override
		{
			if (campaign_.state() !=
				CoopSession::FullEngineCoopCampaignSyncClientState::Ready)
			{
				owner_.deferFailure(
					FullEngineCoopClientRuntimeError::
						TacticalBeforeCampaignReady,
					"tactical.delta.apply",
					"campaign checkpoint is not ready");
				return CoopSession::FullEngineCoopReplicaApplyResult::Rejected;
			}
			return replica_.applyDelta(delta);
		}

	private:
		Impl& owner_;
		CoopSession::FullEngineCoopCampaignSyncClient& campaign_;
		CoopSession::FullEngineCoopSnapshotReplica& replica_;
	};

	class CampaignSink final :
		public CoopSession::FullEngineCoopClientCampaignSyncSink
	{
	public:
		CampaignSink(Impl& owner,
			CoopSession::FullEngineCoopClient& client,
			CoopSession::FullEngineCoopCampaignSyncClient& campaign) noexcept
			: owner_(owner), client_(client), campaign_(campaign)
		{
		}

		bool receiveCampaignMetadata(const std::uint8_t* bytes,
			std::size_t size) noexcept override
		{
			if (retiring()) return true;
			return ensureSession() && accept(
				campaign_.receiveMetadata(bytes, size),
				"campaign.sync.metadata");
		}

		bool receiveCampaignChunk(const std::uint8_t* bytes,
			std::size_t size) noexcept override
		{
			if (retiring()) return true;
			return ensureSession() && accept(
				campaign_.receiveChunk(bytes, size),
				"campaign.sync.chunk");
		}

		bool receiveCampaignComplete(const std::uint8_t* bytes,
			std::size_t size) noexcept override
		{
			if (retiring()) return true;
			return ensureSession() && accept(
				campaign_.receiveComplete(bytes, size),
				"campaign.sync.complete");
		}

		bool receiveCampaignReject(const std::uint8_t* bytes,
			std::size_t size) noexcept override
		{
			if (retiring()) return true;
			return ensureSession() && accept(
				campaign_.receiveReject(bytes, size),
				"campaign.sync.reject");
		}

		bool ensureSession() noexcept
		{
			if (campaign_.state() !=
				CoopSession::FullEngineCoopCampaignSyncClientState::Disconnected)
				return true;
			if ((client_.state() !=
					CoopSession::FullEngineCoopClientState::AwaitingBaseline &&
				 client_.state() !=
					CoopSession::FullEngineCoopClientState::Active) ||
				!client_.hasReconnectCredential())
			{
				owner_.deferFailure(
					FullEngineCoopClientRuntimeError::CampaignSyncFailed,
					"campaign.sync.begin",
					!client_.hasReconnectCredential()
						? "admission completed without a reconnect credential"
						: "client admission state is not ready");
				return false;
			}
			if (client_.sessionEpoch() != owner_.descriptor.sessionEpoch)
			{
				owner_.deferFailure(
					FullEngineCoopClientRuntimeError::
						LiveServerDescriptorMismatch,
					"campaign.sync.begin", "server session changed");
				return false;
			}
			const auto begun = campaign_.beginSession(
				owner_.descriptor, client_.peerIdentity());
			if (begun != CoopSession::
				FullEngineCoopCampaignSyncClientResult::Success)
			{
				owner_.deferFailure(
					FullEngineCoopClientRuntimeError::CampaignSyncFailed,
					"campaign.sync.begin", CampaignClientResultName(begun),
					CampaignFailureReasonName(campaign_.failureReason()));
				return false;
			}
			return true;
		}

	private:
		bool retiring() const noexcept
		{
			return client_.state() ==
				CoopSession::FullEngineCoopClientState::Retiring;
		}

		bool accept(
			CoopSession::FullEngineCoopCampaignSyncClientResult result,
			const char* stage) noexcept
		{
			if (CampaignResultKeepsTransportOpen(result)) return true;
			owner_.deferFailure(
				FullEngineCoopClientRuntimeError::CampaignSyncFailed, stage,
				CampaignClientResultName(result),
				CampaignFailureReasonName(campaign_.failureReason()));
			return false;
		}

		Impl& owner_;
		CoopSession::FullEngineCoopClient& client_;
		CoopSession::FullEngineCoopCampaignSyncClient& campaign_;
	};

	struct Composition
	{
		explicit Composition(Impl& owner) noexcept
			: campaignWire(transport),
			  campaign(owner.scratch, campaignWire),
			  replicaGate(owner, campaign, replica),
			  client(transport, replicaGate, &owner.scratch),
			  campaignSink(owner, client, campaign)
		{
		}

		CoopSession::FullEngineCoopClientTransport transport;
		CampaignWire campaignWire;
		CoopSession::FullEngineCoopCampaignSyncClient campaign;
		CoopSession::FullEngineCoopSnapshotReplica replica;
		ReplicaGate replicaGate;
		CoopSession::FullEngineCoopClient client;
		CampaignSink campaignSink;
	};

	void deferFailure(FullEngineCoopClientRuntimeError incoming,
		const char* stage, const char* reason,
		const char* detail = "none") noexcept
	{
		if (incoming == FullEngineCoopClientRuntimeError::None) return;
		if (pendingError == FullEngineCoopClientRuntimeError::None)
		{
			pendingError = incoming;
			pendingDiagnostic.capture(stage, reason, detail);
		}
	}

	void fail(FullEngineCoopClientRuntimeError incoming,
		const char* stage = "client.runtime", const char* reason = nullptr,
		const char* detail = "none") noexcept
	{
		lifecycle.fail(incoming, stage,
			reason != nullptr ? reason :
				FullEngineCoopClientRuntimeErrorName(incoming), detail);
	}

	void commitPendingFailure() noexcept
	{
		if (pendingError == FullEngineCoopClientRuntimeError::None) return;
		fail(pendingError, pendingDiagnostic.stage(),
			pendingDiagnostic.reason(), pendingDiagnostic.detail());
		pendingError = FullEngineCoopClientRuntimeError::None;
		pendingDiagnostic = CoopRuntimeFailureDiagnostic{};
	}

	CoopSession::FullEngineCoopClientTransportConnectResult
	startTransport() noexcept
	{
		if (!composition)
			return CoopSession::FullEngineCoopClientTransportConnectResult::
				ClientRejected;
		CoopSession::FullEngineCoopClientTransportConfiguration transport;
		transport.serverEndpoint = ja2::mp::net::SdlNetEndpoint(
			options.serverPort, options.serverHost.c_str());
		lastTransportConnectResult = composition->transport.connect(
			composition->client, composition->campaignSink, transport);
		return lastTransportConnectResult;
	}

	void retireCampaignConnection() noexcept
	{
		if (!composition) return;
		if (composition->campaign.state() !=
			CoopSession::FullEngineCoopCampaignSyncClientState::Disconnected)
			composition->campaign.disconnect();
		composition->replica.clear();
		clearPresentationMapAttestation();
		lifecycle.markCampaignReady(false);
	}

	void clearPresentationMapAttestation() noexcept
	{
		presentationMap.clear();
	}

	bool presentationMapIdentityMatches(std::uint64_t sessionEpoch,
		std::uint64_t worldGeneration,
		const TacticalWorldSnapshot& snapshot) const noexcept
	{
		return presentationMap.attemptedIdentityMatches(
			sessionEpoch, worldGeneration, snapshot);
	}

	CoopSession::FullEngineCoopClientCampaignScratch scratch;
	std::unique_ptr<Composition> composition;
	FullEngineCoopClientOptions options;
	CoopSession::CoopCampaignBootstrapDescriptor descriptor;
	DedicatedContentManifestSha256 contentManifest{};
	FullEngineCoopClientRuntimeLifecycle lifecycle;
	FullEngineCoopClientRuntimeError pendingError =
		FullEngineCoopClientRuntimeError::None;
	CoopRuntimeFailureDiagnostic pendingDiagnostic;
	CoopSession::FullEngineCoopClientTransportConnectResult
		lastTransportConnectResult =
			CoopSession::FullEngineCoopClientTransportConnectResult::Success;
	MonotonicClock::time_point reconnectAt{};
	FullEngineCoopClientTacticalReadyMarker tacticalReadyMarker;
	FullEngineCoopClientPresentationMapPlanBinding presentationMap;
	unsigned reconnectAttempts = 0;
	bool reconnectScheduled = false;
	bool contentManifestCaptured = false;
};

FullEngineCoopClientRuntime::FullEngineCoopClientRuntime() noexcept
	: impl_(new (std::nothrow) Impl())
{
}

FullEngineCoopClientRuntime::~FullEngineCoopClientRuntime() noexcept
{
	if (impl_ != nullptr)
	{
		stopTransport();
		impl_->scratch.close();
		delete impl_;
		impl_ = nullptr;
	}
}

bool FullEngineCoopClientRuntime::prepareEarly(
	CancellationRequested cancellationRequested) noexcept
{
	if (impl_ == nullptr) return false;
	if (impl_->lifecycle.prepared() || impl_->lifecycle.failed())
	{
		impl_->fail(FullEngineCoopClientRuntimeError::InvalidState,
			"bootstrap.preflight", "runtime already prepared or failed");
		return false;
	}
	if (!IsFullEngineCoopClientProcess())
	{
		impl_->fail(FullEngineCoopClientRuntimeError::NotClientProcess,
			"bootstrap.preflight", "co-op client mode is not enabled");
		return false;
	}
	try
	{
		impl_->options = GetFullEngineCoopClientOptions();
	}
	catch (...)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::ClientConfigurationFailed,
			"bootstrap.options", "client options are unavailable");
		return false;
	}

	CoopSession::FullEngineCoopClientBootstrapTransport bootstrap;
	CoopSession::FullEngineCoopClientBootstrapTransportConfiguration wire;
	wire.serverEndpoint = ja2::mp::net::SdlNetEndpoint(
		impl_->options.serverPort, impl_->options.serverHost.c_str());
	const CoopSession::FullEngineCoopClientBootstrapTransportConnectResult
		bootstrapConnected = bootstrap.connect(wire);
	if (bootstrapConnected != CoopSession::
		FullEngineCoopClientBootstrapTransportConnectResult::Success)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::BootstrapConnectFailed,
			"bootstrap.connect", BootstrapConnectResultName(bootstrapConnected));
		return false;
	}

	for (;;)
	{
		if (cancellationRequested != nullptr && cancellationRequested())
		{
			bootstrap.stop();
			impl_->fail(
				FullEngineCoopClientRuntimeError::BootstrapCancelled,
				"bootstrap.handshake", "cancelled by process termination request");
			return false;
		}
		bootstrap.poll();
		const auto state = bootstrap.state();
		if (state == CoopSession::
			FullEngineCoopClientBootstrapTransportState::Complete)
			break;
		if (state == CoopSession::
				FullEngineCoopClientBootstrapTransportState::Failed ||
			state == CoopSession::
				FullEngineCoopClientBootstrapTransportState::Stopped)
		{
			impl_->fail(FullEngineCoopClientRuntimeError::BootstrapFailed,
				"bootstrap.handshake", BootstrapResultName(bootstrap.result()));
			return false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	CoopSession::CoopCampaignBootstrapDescriptor descriptor;
	if (!bootstrap.descriptor(descriptor) ||
		!CoopSession::IsValidCoopCampaignBootstrapDescriptor(descriptor))
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::DescriptorUnavailable,
			"bootstrap.descriptor", "descriptor missing or invalid");
		return false;
	}

	std::filesystem::path stateRoot;
	try
	{
		stateRoot = std::filesystem::u8path(
			impl_->options.stateDirectory.begin(),
			impl_->options.stateDirectory.end());
	}
	catch (...)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::StateDirectoryInvalid,
			"client-state.path", "state directory is not valid UTF-8");
		return false;
	}
	if (!stateRoot.is_absolute())
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::StateDirectoryInvalid,
			"client-state.path", "state directory must be absolute");
		return false;
	}

	const CoopSession::FullEngineCoopClientCampaignScratchPrepareResult
		scratchPrepared = impl_->scratch.prepare(stateRoot, descriptor);
	if (scratchPrepared != CoopSession::
		FullEngineCoopClientCampaignScratchPrepareResult::Success)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::ScratchPrepareFailed,
			"client-state.prepare", ScratchPrepareResultName(scratchPrepared));
		return false;
	}
	const GameSimulationRandomInstallError randomInstalled =
		InstallGameSimulationRandom(descriptor.campaignSeed);
	if (randomInstalled != GameSimulationRandomInstallError::None)
	{
		impl_->scratch.close();
		impl_->fail(
			FullEngineCoopClientRuntimeError::
				SimulationRandomInstallFailed,
			"simulation-random.install",
			RandomInstallResultName(randomInstalled));
		return false;
	}

	impl_->descriptor = descriptor;
	if (!impl_->lifecycle.markPrepared())
	{
		impl_->scratch.close();
		impl_->fail(FullEngineCoopClientRuntimeError::InvalidState,
			"bootstrap.publish", "prepared lifecycle transition rejected");
		return false;
	}
	return true;
}

bool FullEngineCoopClientRuntime::captureContentManifestAfterPackageMount()
	noexcept
{
	if (impl_ == nullptr) return false;
	if (!impl_->lifecycle.prepared() || impl_->lifecycle.networkOpen() ||
		impl_->lifecycle.failed() || impl_->composition ||
		impl_->contentManifestCaptured)
	{
		impl_->fail(FullEngineCoopClientRuntimeError::InvalidState,
			"content-manifest.capture", "runtime is not ready for manifest capture");
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
			"[co-op client] installed content manifest capture failed: %s\n",
			DedicatedContentManifestErrorName(manifestResult));
		impl_->fail(FullEngineCoopClientRuntimeError::ContentManifestFailed,
			"content-manifest.capture",
			DedicatedContentManifestErrorName(manifestResult));
		return false;
	}
	if (content != impl_->descriptor.contentManifestSha256)
	{
		impl_->fail(FullEngineCoopClientRuntimeError::CompatibilityMismatch,
			"content-manifest.compare", "installed content differs from server");
		return false;
	}
	impl_->contentManifest = content;
	impl_->contentManifestCaptured = true;
	return true;
}

bool FullEngineCoopClientRuntime::openAfterPackageBootstrap(
	GameContext& context) noexcept
{
	if (impl_ == nullptr) return false;
	if (!impl_->lifecycle.prepared() || !impl_->contentManifestCaptured ||
		impl_->lifecycle.networkOpen() ||
		impl_->lifecycle.failed() ||
		impl_->composition)
	{
		impl_->fail(FullEngineCoopClientRuntimeError::InvalidState,
			"client-composition.preflight",
			"runtime prerequisites are incomplete or already consumed");
		return false;
	}

	CoopSession::RuntimeCompatibilityFingerprint fingerprint;
	try
	{
		fingerprint = AdmissionFingerprint(
			context.runtime().compatibilityFingerprint());
	}
	catch (...)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::RuntimeFingerprintFailed,
			"runtime-fingerprint.capture", "runtime fingerprint threw an exception");
		return false;
	}
	if (fingerprint != impl_->descriptor.runtimeFingerprint)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::CompatibilityMismatch,
			"runtime-fingerprint.compare", "runtime differs from server");
		return false;
	}
	CoopSession::AdmissionAck restoredCredential;
	const CoopSession::FullEngineCoopReconnectCredentialLoadResult loaded =
		impl_->scratch.loadReconnectCredential(restoredCredential);
	if (loaded ==
		CoopSession::FullEngineCoopReconnectCredentialLoadResult::Retired)
	{
		// The private campaign state has already completed voluntary leave. It
		// is a clean terminal process state, not a credential-less first join.
		// Do not even construct the live transport composition.
		if (!impl_->lifecycle.markDurablyRetired())
		{
			impl_->fail(FullEngineCoopClientRuntimeError::InvalidState,
				"credential.retired", "retired lifecycle transition rejected");
			return false;
		}
		return true;
	}
	if (loaded ==
		CoopSession::FullEngineCoopReconnectCredentialLoadResult::StaleSession)
	{
		// The local VFS manifest and runtime fingerprint were exact-checked above.
		// Only now may a new process epoch retire its unreachable old bearer.
		if (!impl_->scratch.eraseStaleReconnectCredential())
		{
			impl_->fail(FullEngineCoopClientRuntimeError::
				ReconnectCredentialLoadFailed,
				"credential.retire-stale",
				"could not retire the credential from the previous server session");
			return false;
		}
	}
	else if (loaded !=
			CoopSession::FullEngineCoopReconnectCredentialLoadResult::Missing &&
		loaded !=
			CoopSession::FullEngineCoopReconnectCredentialLoadResult::Loaded)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::ReconnectCredentialLoadFailed,
			"credential.load", CredentialLoadResultName(loaded));
		return false;
	}

	try
	{
		impl_->composition = std::make_unique<Impl::Composition>(*impl_);
	}
	catch (...)
	{
		impl_->composition.reset();
	}
	if (!impl_->composition)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::ClientConfigurationFailed,
			"client-composition.allocate", "composition allocation failed");
		return false;
	}
	CoopSession::FullEngineCoopClientConfiguration client;
	client.protocolVersion = impl_->descriptor.protocolVersion;
	client.runtimeFingerprint = impl_->descriptor.runtimeFingerprint;
	client.contentManifestSha256 =
		impl_->descriptor.contentManifestSha256;
	client.expectedSessionEpoch = impl_->descriptor.sessionEpoch;
	client.durableReconnectCredentialRequired = true;
	const CoopSession::FullEngineCoopClientResult configured =
		impl_->composition->client.configure(client);
	if (configured != CoopSession::FullEngineCoopClientResult::Success)
	{
		impl_->composition.reset();
		impl_->fail(
			FullEngineCoopClientRuntimeError::ClientConfigurationFailed,
			"client-core.configure", ClientResultName(configured));
		return false;
	}
	CoopSession::FullEngineCoopClientResult restored =
		CoopSession::FullEngineCoopClientResult::Success;
	if (loaded ==
			CoopSession::FullEngineCoopReconnectCredentialLoadResult::Loaded &&
		(restored = impl_->composition->client.restoreReconnectCredential(
			restoredCredential)) != CoopSession::
				FullEngineCoopClientResult::Success)
	{
		impl_->composition.reset();
		impl_->fail(
			FullEngineCoopClientRuntimeError::ReconnectCredentialRestoreFailed,
			"credential.restore", ClientResultName(restored));
		return false;
	}
	const CoopSession::FullEngineCoopClientTransportConnectResult connected =
		impl_->startTransport();
	if (connected !=
		CoopSession::FullEngineCoopClientTransportConnectResult::Success)
	{
		impl_->composition.reset();
		impl_->fail(
			FullEngineCoopClientRuntimeError::TransportConnectFailed,
			"live-transport.connect", TransportConnectResultName(connected));
		return false;
	}

	if (!impl_->lifecycle.markNetworkOpen())
	{
		impl_->composition->transport.stop(0);
		impl_->composition.reset();
		impl_->fail(FullEngineCoopClientRuntimeError::InvalidState,
			"live-transport.publish", "network lifecycle transition rejected");
		return false;
	}
	impl_->reconnectAttempts = 0;
	impl_->reconnectScheduled = false;
	return true;
}

void FullEngineCoopClientRuntime::pumpAfterCommittedFrame() noexcept
{
	if (impl_ == nullptr || !impl_->lifecycle.networkOpen() ||
		impl_->lifecycle.failed() ||
		!impl_->composition)
		return;
	Impl::Composition& live = *impl_->composition;
	live.transport.poll();

	impl_->commitPendingFailure();
	if (!impl_->lifecycle.failed() && live.client.sessionEpoch() != 0 &&
		live.client.sessionEpoch() != impl_->descriptor.sessionEpoch)
		impl_->fail(
			FullEngineCoopClientRuntimeError::LiveServerDescriptorMismatch,
			"admission.hello", "server session changed after bootstrap");
	if (!impl_->lifecycle.failed() && live.client.state() ==
		CoopSession::FullEngineCoopClientState::Failed)
	{
		const CoopSession::FullEngineCoopClientResult clientResult =
			live.client.lastResult();
		const bool credentialFailure = clientResult == CoopSession::
			FullEngineCoopClientResult::CredentialStorageFailure;
		impl_->fail(credentialFailure
				? FullEngineCoopClientRuntimeError::
					ReconnectCredentialStorageFailed
				: FullEngineCoopClientRuntimeError::TacticalClientFailed,
			credentialFailure ? "credential.persist" : "tactical.client",
			ClientResultName(clientResult),
			clientResult == CoopSession::FullEngineCoopClientResult::AdmissionRejected
				? AdmissionRejectReasonName(
					live.client.lastAdmissionRejectReason())
				: TransportFailureName(live.transport.lastFailure()));
	}
	if (!impl_->lifecycle.failed() && live.client.state() ==
		CoopSession::FullEngineCoopClientState::Retired)
	{
		(void)impl_->lifecycle.markRetired();
		stopTransport();
		return;
	}
	if (!impl_->lifecycle.failed() &&
		(live.campaign.state() == CoopSession::
				FullEngineCoopCampaignSyncClientState::Failed ||
		 live.campaign.state() == CoopSession::
				FullEngineCoopCampaignSyncClientState::Rejected))
		impl_->fail(FullEngineCoopClientRuntimeError::CampaignSyncFailed,
			"campaign.sync", CampaignClientResultName(live.campaign.lastResult()),
			CampaignFailureReasonName(live.campaign.failureReason()));
	if (impl_->lifecycle.failed())
	{
		stopTransport();
		return;
	}

	// Admission may complete without metadata in this poll. Bind the campaign
	// coordinator now; CampaignSink performs the same lazy check inline when
	// metadata was already copied behind the response in this very poll.
	if (live.campaign.state() == CoopSession::
			FullEngineCoopCampaignSyncClientState::Disconnected &&
		(live.client.state() ==
				CoopSession::FullEngineCoopClientState::AwaitingBaseline ||
		 live.client.state() ==
				CoopSession::FullEngineCoopClientState::Active) &&
		!live.campaignSink.ensureSession())
	{
		impl_->commitPendingFailure();
		stopTransport();
		return;
	}

	if (live.campaign.hasPendingOutbound())
	{
		const auto flushed = live.campaign.flushOutbound();
		if (!CampaignResultKeepsTransportOpen(flushed))
		{
			impl_->fail(
				FullEngineCoopClientRuntimeError::CampaignSyncFailed,
				"campaign.sync.flush", CampaignClientResultName(flushed),
				CampaignFailureReasonName(live.campaign.failureReason()));
			stopTransport();
			return;
		}
	}
	const bool wasCampaignReady = impl_->lifecycle.campaignReady();
	const bool wasReconnecting = impl_->lifecycle.reconnecting();
	impl_->lifecycle.markCampaignReady(live.campaign.state() ==
		CoopSession::FullEngineCoopCampaignSyncClientState::Ready);
	if (!wasCampaignReady && impl_->lifecycle.campaignReady())
	{
		if (wasReconnecting)
			std::printf("[co-op client] server connection restored\n");
		std::printf("[co-op client] campaign synchronized and ready\n");
		std::fflush(stdout);
	}
	if (impl_->lifecycle.campaignReady() || live.client.state() ==
		CoopSession::FullEngineCoopClientState::AwaitingBaseline ||
		live.client.state() == CoopSession::FullEngineCoopClientState::Active)
	{
		// Reset the bounded retry budget only after the server has completed
		// admission, rather than after a hostile accept-and-drop TCP handshake.
		impl_->reconnectAttempts = 0;
	}
	const bool tacticalActive = live.client.state() ==
		CoopSession::FullEngineCoopClientState::Active;
	const bool tacticalResynchronizing = live.client.resyncPending();
	if (impl_->lifecycle.campaignReady() && tacticalActive &&
		live.replica.hasSnapshot() && !tacticalResynchronizing)
	{
		const TacticalWorldSnapshot& snapshot = live.replica.snapshot();
		const std::uint64_t sessionEpoch = live.client.sessionEpoch();
		const std::uint64_t worldGeneration =
			live.client.acceptedState().worldGeneration;
		if (!impl_->presentationMapIdentityMatches(
				sessionEpoch, worldGeneration, snapshot))
		{
			FullEngineCoopClientPresentationMapPlan candidate;
			FullEngineCoopClientPresentationMapPlanResult result =
				FullEngineCoopClientPresentationMapPlanResult::InvalidSnapshot;
			if (impl_->presentationMap.beginAttempt(
					sessionEpoch, worldGeneration, snapshot))
				result = ReadFullEngineCoopClientPresentationMapAsset(
					snapshot.sector(), snapshot.dimensions(), candidate);
			const bool committed = result ==
					FullEngineCoopClientPresentationMapPlanResult::Success &&
				impl_->presentationMap.commit(std::move(candidate));
			if (!committed)
			{
				if (result ==
					FullEngineCoopClientPresentationMapPlanResult::Success)
					result = FullEngineCoopClientPresentationMapPlanResult::
						InvalidSnapshot;
				impl_->presentationMap.recordFailure(result);
			}
			if (committed)
				std::printf(
					"[co-op client] exact tactical map verified: %s\n",
					snapshot.sector().mapAssetKey.c_str());
			else
				std::printf(
					"[co-op client] exact tactical map unavailable: %s (%s)\n",
					snapshot.sector().mapAssetKey.c_str(),
					FullEngineCoopClientPresentationMapPlanResultName(
						impl_->presentationMap.result()));
			std::fflush(stdout);
		}
	}
	else
	{
		impl_->clearPresentationMapAttestation();
	}
	if (impl_->tacticalReadyMarker.observe(
		impl_->lifecycle.campaignReady(), tacticalActive,
		live.replica.hasSnapshot(), live.client.assignedActorCount(),
		live.client.acceptedState().worldGeneration))
	{
		// A reconnect or same-world resync commits another baseline but must not
		// duplicate the operational marker. A new authoritative world generation
		// naturally arms it again without trusting any client or credential field.
		std::printf(
			"[co-op client] tactical baseline committed; assigned actors ready\n");
		std::fflush(stdout);
	}

	if (live.transport.running())
	{
		impl_->reconnectScheduled = false;
		return;
	}

	impl_->retireCampaignConnection();
	if (impl_->lifecycle.markReconnecting())
	{
		std::printf(
			"[co-op client] reconnecting to server; reason=%s; endpoint=%s:%u\n",
			TransportFailureName(live.transport.lastFailure()),
			impl_->options.serverHost.c_str(),
			static_cast<unsigned>(impl_->options.serverPort));
		std::fflush(stdout);
	}
	if (!impl_->reconnectScheduled)
	{
		impl_->reconnectAt = MonotonicClock::now() + ReconnectDelay;
		impl_->reconnectScheduled = true;
		return;
	}
	if (MonotonicClock::now() < impl_->reconnectAt) return;
	const bool credentialsRetained =
		live.client.hasReconnectCredential();
	const bool reconnectAllowed = impl_->lifecycle.reconnectAllowed(
		live.client.sessionEpoch() == impl_->descriptor.sessionEpoch,
		credentialsRetained, impl_->reconnectAttempts,
		MaximumReconnectAttempts);
	if (!reconnectAllowed)
	{
		const bool serverSessionChanged = credentialsRetained &&
			live.client.sessionEpoch() != impl_->descriptor.sessionEpoch;
		const char* const transportDetail =
			impl_->lastTransportConnectResult != CoopSession::
				FullEngineCoopClientTransportConnectResult::Success
			? TransportConnectResultName(impl_->lastTransportConnectResult)
			: TransportFailureName(live.transport.lastFailure());
		impl_->fail(
			serverSessionChanged
				? FullEngineCoopClientRuntimeError::LiveServerDescriptorMismatch
				: FullEngineCoopClientRuntimeError::ReconnectLimitReached,
			"live-transport.reconnect",
			serverSessionChanged ? "server session changed" :
				"retry limit reached",
			transportDetail);
		stopTransport();
		return;
	}
	if (impl_->reconnectAttempts !=
		(std::numeric_limits<unsigned>::max)())
		++impl_->reconnectAttempts;
	if (impl_->startTransport() == CoopSession::
		FullEngineCoopClientTransportConnectResult::Success)
	{
		impl_->reconnectScheduled = false;
		return;
	}
	impl_->reconnectAt = MonotonicClock::now() + ReconnectDelay;
}

void FullEngineCoopClientRuntime::stopTransport() noexcept
{
	if (impl_ == nullptr || !impl_->composition)
	{
		if (impl_ != nullptr)
		{
			impl_->clearPresentationMapAttestation();
			impl_->lifecycle.markTransportStopped();
		}
		return;
	}
	Impl::Composition& live = *impl_->composition;
	live.transport.stop(LiveTransportDrainMilliseconds);
	impl_->retireCampaignConnection();
	impl_->composition.reset();
	impl_->lifecycle.markTransportStopped();
	impl_->reconnectScheduled = false;
}

void FullEngineCoopClientRuntime::closeAfterVfs() noexcept
{
	if (impl_ == nullptr) return;
	// Normal ownership reaches this boundary only after the separate transport
	// subsystem has detached every core while GameContext and SDL were alive.
	// Preserve safety on exceptional rollback without conflating that detach
	// with the lease boundary itself.
	if (impl_->composition) stopTransport();
	impl_->scratch.close();
	(void)impl_->lifecycle.markLeaseClosed();
}

bool FullEngineCoopClientRuntime::prepared() const noexcept
{
	return impl_ != nullptr && impl_->lifecycle.prepared();
}

bool FullEngineCoopClientRuntime::networkOpen() const noexcept
{
	return impl_ != nullptr && impl_->lifecycle.networkOpen();
}

bool FullEngineCoopClientRuntime::campaignReady() const noexcept
{
	return impl_ != nullptr && impl_->lifecycle.campaignReady() &&
		impl_->composition != nullptr &&
		impl_->composition->campaign.state() == CoopSession::
			FullEngineCoopCampaignSyncClientState::Ready;
}

bool FullEngineCoopClientRuntime::campaignGroups(CoopSession::CoopCampaignGroups& output) const noexcept
{
	output = {};
	if (!campaignReady() || failed()) return false;
	const auto* groups = impl_->composition->client.campaignGroups();
	if (!groups) return false;
	output = *groups;
	return true;
}

bool FullEngineCoopClientRuntime::campaignEconomy(CoopSession::CoopCampaignEconomy& output) const noexcept
{
	output = {};
	if (!campaignReady() || failed()) return false;
	const auto* value = impl_->composition->client.campaignEconomy();
	if (!value) return false;
	output = *value; return true;
}

bool FullEngineCoopClientRuntime::campaignAimQuotes(CoopSession::CoopCampaignAimQuotes& output) const noexcept
{
	output = {};
	if (!campaignReady() || failed()) return false;
	const auto* value = impl_->composition->client.campaignAimQuotes();
	if (!value) return false;
	output = *value; return true;
}

bool FullEngineCoopClientRuntime::campaignProfileNickname(std::uint16_t profile, std::array<wchar_t,32>& output) const noexcept
{
	output = {};
	if (!campaignReady() || failed()) return false;
	const auto* quotes = impl_->composition->client.campaignAimQuotes();
	if (!quotes || !CoopSession::FindCoopCampaignAimQuote(*quotes,profile)) return false;
	const auto* view = impl_->scratch.activeView();
	return view && PassiveCampaignNickname(*view, profile, output);
}

CoopSession::FullEngineCoopClientResult FullEngineCoopClientRuntime::requestCampaignHire(
	std::uint16_t profile, std::uint8_t days, bool buyGear) noexcept
{
	if (!campaignReady() || failed() || selfRetirementPending() || retired())
		return CoopSession::FullEngineCoopClientResult::InvalidState;
	return impl_->composition->client.requestCampaignHire(profile, days, buyGear);
}

bool FullEngineCoopClientRuntime::campaignHireFeedback(CoopSession::CoopCampaignHireResult& output, bool& pending) const noexcept
{
	output = {}; pending = false;
	if (!campaignReady() || failed() || selfRetirementPending() || retired()) return false;
	const auto& client = impl_->composition->client;
	pending = client.campaignHirePending();
	const auto* result = client.lastCampaignHireResult();
	if (!result) return false;
	output = *result; return true;
}

bool FullEngineCoopClientRuntime::campaignStatus(CoopSession::CoopCampaignStatus& output,
	bool& localTimeLeader) const noexcept
{
	output = {};
	localTimeLeader = false;
	if (!campaignReady() || failed()) return false;
	const auto* status = impl_->composition->client.campaignStatus();
	if (!status) return false;
	output = *status;
	localTimeLeader = status->timeLeaderReady &&
		status->timeLeader == impl_->composition->client.peerIdentity();
	return true;
}

CoopSession::FullEngineCoopClientResult FullEngineCoopClientRuntime::requestCampaignTime(CoopSession::CoopCampaignTimeAction action) noexcept
{
	if (!campaignReady() || failed() || selfRetirementPending() || retired())
		return CoopSession::FullEngineCoopClientResult::InvalidState;
	return impl_->composition->client.requestCampaignTime(action);
}

bool FullEngineCoopClientRuntime::campaignTimeFeedback(CoopSession::CoopCampaignTimeResult& output, bool& pending) const noexcept
{
	output = {}; pending = false;
	if (!campaignReady() || failed() || selfRetirementPending() || retired()) return false;
	const auto& client = impl_->composition->client;
	pending = client.campaignTimePending();
	const auto* result = client.campaignTimeResult();
	if (!result) return false;
	output = *result;
	return true;
}

CoopSession::FullEngineCoopClientResult FullEngineCoopClientRuntime::requestCampaignAction(
	const CoopSession::CoopCampaignActionRequest& request) noexcept
{
	if (!campaignReady() || failed() || selfRetirementPending() || retired())
		return CoopSession::FullEngineCoopClientResult::InvalidState;
	return impl_->composition->client.requestCampaignAction(request);
}

bool FullEngineCoopClientRuntime::campaignActionFeedback(CoopSession::CoopCampaignActionResult& output,
	bool& pending) const noexcept
{
	output = {}; pending = false;
	if (!campaignReady() || failed() || selfRetirementPending() || retired()) return false;
	const auto& client = impl_->composition->client;
	pending = client.campaignActionPending();
	const auto* result = client.lastCampaignActionResult();
	if (!result) return false;
	output = *result;
	return true;
}

bool FullEngineCoopClientRuntime::retired() const noexcept
{
	return impl_ != nullptr && impl_->lifecycle.retired();
}

FullEngineCoopClientConnectionPhase
FullEngineCoopClientRuntime::connectionPhase() const noexcept
{
	if (impl_ == nullptr) return FullEngineCoopClientConnectionPhase::Failed;
	return impl_->lifecycle.connectionPhase(impl_->composition != nullptr
		? impl_->composition->client.state()
		: CoopSession::FullEngineCoopClientState::Disconnected);
}

bool FullEngineCoopClientRuntime::selfRetirementPending() const noexcept
{
	return impl_ != nullptr && impl_->composition != nullptr &&
		impl_->composition->client.selfRetirementPending();
}

bool FullEngineCoopClientRuntime::failed() const noexcept
{
	return impl_ == nullptr || impl_->lifecycle.failed();
}

FullEngineCoopClientRuntimeError FullEngineCoopClientRuntime::error()
	const noexcept
{
	return impl_ != nullptr ? impl_->lifecycle.error() :
		FullEngineCoopClientRuntimeError::InvalidState;
}

const CoopRuntimeFailureDiagnostic&
FullEngineCoopClientRuntime::failureDiagnostic() const noexcept
{
	static const CoopRuntimeFailureDiagnostic unavailable;
	return impl_ != nullptr
		? impl_->lifecycle.failureDiagnostic() : unavailable;
}

const char* FullEngineCoopClientRuntime::serverHost() const noexcept
{
	return impl_ != nullptr ? impl_->options.serverHost.c_str() : "";
}

std::uint16_t FullEngineCoopClientRuntime::serverPort() const noexcept
{
	return impl_ != nullptr ? impl_->options.serverPort : 0;
}

const std::filesystem::path&
FullEngineCoopClientRuntime::profileDirectory() const noexcept
{
	static const std::filesystem::path empty;
	return impl_ != nullptr ? impl_->scratch.profileDirectory() : empty;
}

const CoopSession::CoopCampaignBootstrapDescriptor&
FullEngineCoopClientRuntime::descriptor() const noexcept
{
	static const CoopSession::CoopCampaignBootstrapDescriptor unavailable;
	return impl_ != nullptr ? impl_->descriptor : unavailable;
}

const CoopSession::FullEngineCoopSnapshotReplica*
FullEngineCoopClientRuntime::snapshotReplica() const noexcept
{
	return impl_ != nullptr && impl_->composition != nullptr &&
		impl_->lifecycle.snapshotPublishable(
			impl_->composition->client.state() ==
				CoopSession::FullEngineCoopClientState::Active ||
			impl_->composition->client.resyncPending())
		? &impl_->composition->replica : nullptr;
}

bool FullEngineCoopClientRuntime::presentationView(
	FullEngineCoopClientPresentationView& output) const noexcept
{
	output = FullEngineCoopClientPresentationView{};
	const CoopSession::FullEngineCoopSnapshotReplica* const replica =
		snapshotReplica();
	if (replica == nullptr || !replica->hasSnapshot() ||
		impl_ == nullptr || !impl_->composition)
		return false;

	const CoopSession::FullEngineCoopClient& client =
		impl_->composition->client;
	output.snapshot = &replica->snapshot();
	output.assignedActorCount = client.assignedActorCount();
	for (std::size_t index = 0;
		index < output.assignedActorCount; ++index)
	{
		output.assignedActors[index] = client.assignedActor(index);
		output.ownerInventories[index] = client.ownerInventory(output.assignedActors[index]);
	}
	output.outstandingCommandId = client.outstandingCommandId();
	output.state = client.acceptedState();
	output.resynchronizing = client.resyncPending();
	output.mapPlan = impl_->presentationMap.borrow(
		client.sessionEpoch(), client.acceptedState().worldGeneration,
		replica->snapshot(), output.resynchronizing);
	output.exactMapAssetIdentityVerified =
		output.mapPlan != nullptr;
	output.hasLastReceipt = client.hasLastIntentReceipt();
	if (output.hasLastReceipt)
		output.lastReceipt = client.lastIntentReceipt();
	return true;
}

CoopSession::FullEngineCoopClientResult
FullEngineCoopClientRuntime::sendIntent(
	TacticalEntityId actor,
	const CoopSession::TacticalIntentPayload& payload) noexcept
{
	if (snapshotReplica() == nullptr || impl_ == nullptr ||
		!impl_->composition || impl_->composition->client.resyncPending())
		return CoopSession::FullEngineCoopClientResult::InvalidState;
	return impl_->composition->client.sendIntent(actor, payload);
}

CoopSession::FullEngineCoopClientResult
FullEngineCoopClientRuntime::requestSelfRetirement() noexcept
{
	if (impl_ == nullptr || !impl_->lifecycle.networkOpen() ||
		impl_->lifecycle.failed() || impl_->lifecycle.retired() ||
		!impl_->composition)
		return CoopSession::FullEngineCoopClientResult::InvalidState;
	return impl_->composition->client.requestSelfRetirement();
}

FullEngineCoopClientRuntime& GetFullEngineCoopClientRuntime() noexcept
{
	static FullEngineCoopClientRuntime runtime;
	return runtime;
}

const char* FullEngineCoopClientRuntimeErrorName(
	FullEngineCoopClientRuntimeError error) noexcept
{
	switch (error)
	{
		case FullEngineCoopClientRuntimeError::None: return "none";
		case FullEngineCoopClientRuntimeError::NotClientProcess:
			return "not a full-engine co-op client process";
		case FullEngineCoopClientRuntimeError::InvalidState:
			return "invalid client runtime state";
		case FullEngineCoopClientRuntimeError::BootstrapConnectFailed:
			return "bootstrap transport connection failed";
		case FullEngineCoopClientRuntimeError::BootstrapFailed:
			return "bootstrap transport failed";
		case FullEngineCoopClientRuntimeError::BootstrapCancelled:
			return "bootstrap cancelled";
		case FullEngineCoopClientRuntimeError::DescriptorUnavailable:
			return "campaign descriptor unavailable";
		case FullEngineCoopClientRuntimeError::StateDirectoryInvalid:
			return "client state directory invalid";
		case FullEngineCoopClientRuntimeError::ScratchPrepareFailed:
			return "client campaign scratch preparation failed";
		case FullEngineCoopClientRuntimeError::SimulationRandomInstallFailed:
			return "client simulation random installation failed";
		case FullEngineCoopClientRuntimeError::ContentManifestFailed:
			return "client content manifest failed";
		case FullEngineCoopClientRuntimeError::RuntimeFingerprintFailed:
			return "client runtime fingerprint failed";
		case FullEngineCoopClientRuntimeError::CompatibilityMismatch:
			return "client runtime compatibility mismatch";
		case FullEngineCoopClientRuntimeError::ClientConfigurationFailed:
			return "client core configuration failed";
		case FullEngineCoopClientRuntimeError::ReconnectCredentialLoadFailed:
			return "reconnect credential load failed";
		case FullEngineCoopClientRuntimeError::ReconnectCredentialRestoreFailed:
			return "reconnect credential restore failed";
		case FullEngineCoopClientRuntimeError::ReconnectCredentialStorageFailed:
			return "reconnect credential storage failed";
		case FullEngineCoopClientRuntimeError::TransportConnectFailed:
			return "live client transport connection failed";
		case FullEngineCoopClientRuntimeError::LiveServerDescriptorMismatch:
			return "live server descriptor mismatch";
		case FullEngineCoopClientRuntimeError::CampaignSyncFailed:
			return "client campaign synchronization failed";
		case FullEngineCoopClientRuntimeError::TacticalBeforeCampaignReady:
			return "tactical state arrived before campaign load committed";
		case FullEngineCoopClientRuntimeError::TacticalClientFailed:
			return "passive tactical client failed";
		case FullEngineCoopClientRuntimeError::ReconnectLimitReached:
			return "client reconnect limit reached";
	}
	return "unknown full-engine co-op client runtime error";
}
