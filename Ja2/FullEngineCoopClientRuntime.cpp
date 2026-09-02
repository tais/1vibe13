#include "FullEngineCoopClientRuntime.h"

#include "DedicatedContentManifest.h"
#include "FullEngineCoopClientCampaignScratch.h"
#include "FullEngineCoopClientOptions.h"
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
						TacticalBeforeCampaignReady);
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
						TacticalBeforeCampaignReady);
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
				campaign_.receiveMetadata(bytes, size));
		}

		bool receiveCampaignChunk(const std::uint8_t* bytes,
			std::size_t size) noexcept override
		{
			if (retiring()) return true;
			return ensureSession() && accept(
				campaign_.receiveChunk(bytes, size));
		}

		bool receiveCampaignComplete(const std::uint8_t* bytes,
			std::size_t size) noexcept override
		{
			if (retiring()) return true;
			return ensureSession() && accept(
				campaign_.receiveComplete(bytes, size));
		}

		bool receiveCampaignReject(const std::uint8_t* bytes,
			std::size_t size) noexcept override
		{
			if (retiring()) return true;
			return ensureSession() && accept(
				campaign_.receiveReject(bytes, size));
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
					FullEngineCoopClientRuntimeError::CampaignSyncFailed);
				return false;
			}
			if (client_.sessionEpoch() != owner_.descriptor.sessionEpoch)
			{
				owner_.deferFailure(
					FullEngineCoopClientRuntimeError::
						LiveServerDescriptorMismatch);
				return false;
			}
			const auto begun = campaign_.beginSession(
				owner_.descriptor, client_.peerIdentity());
			if (begun != CoopSession::
				FullEngineCoopCampaignSyncClientResult::Success)
			{
				owner_.deferFailure(
					FullEngineCoopClientRuntimeError::CampaignSyncFailed);
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
			CoopSession::FullEngineCoopCampaignSyncClientResult result) noexcept
		{
			if (CampaignResultKeepsTransportOpen(result)) return true;
			owner_.deferFailure(
				FullEngineCoopClientRuntimeError::CampaignSyncFailed);
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

	void deferFailure(FullEngineCoopClientRuntimeError incoming) noexcept
	{
		if (pendingError == FullEngineCoopClientRuntimeError::None)
			pendingError = incoming;
	}

	void fail(FullEngineCoopClientRuntimeError incoming) noexcept
	{
		lifecycle.fail(incoming);
	}

	bool startTransport() noexcept
	{
		if (!composition) return false;
		CoopSession::FullEngineCoopClientTransportConfiguration transport;
		transport.serverEndpoint = ja2::mp::net::SdlNetEndpoint(
			options.serverPort, options.serverHost.c_str());
		return composition->transport.connect(
			composition->client, composition->campaignSink, transport) ==
			CoopSession::
				FullEngineCoopClientTransportConnectResult::Success;
	}

	void retireCampaignConnection() noexcept
	{
		if (!composition) return;
		if (composition->campaign.state() !=
			CoopSession::FullEngineCoopCampaignSyncClientState::Disconnected)
			composition->campaign.disconnect();
		composition->replica.clear();
		lifecycle.markCampaignReady(false);
	}

	CoopSession::FullEngineCoopClientCampaignScratch scratch;
	std::unique_ptr<Composition> composition;
	FullEngineCoopClientOptions options;
	CoopSession::CoopCampaignBootstrapDescriptor descriptor;
	DedicatedContentManifestSha256 contentManifest{};
	FullEngineCoopClientRuntimeLifecycle lifecycle;
	FullEngineCoopClientRuntimeError pendingError =
		FullEngineCoopClientRuntimeError::None;
	MonotonicClock::time_point reconnectAt{};
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
		impl_->fail(FullEngineCoopClientRuntimeError::InvalidState);
		return false;
	}
	if (!IsFullEngineCoopClientProcess())
	{
		impl_->fail(FullEngineCoopClientRuntimeError::NotClientProcess);
		return false;
	}
	try
	{
		impl_->options = GetFullEngineCoopClientOptions();
	}
	catch (...)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::ClientConfigurationFailed);
		return false;
	}

	CoopSession::FullEngineCoopClientBootstrapTransport bootstrap;
	CoopSession::FullEngineCoopClientBootstrapTransportConfiguration wire;
	wire.serverEndpoint = ja2::mp::net::SdlNetEndpoint(
		impl_->options.serverPort, impl_->options.serverHost.c_str());
	if (bootstrap.connect(wire) != CoopSession::
		FullEngineCoopClientBootstrapTransportConnectResult::Success)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::BootstrapConnectFailed);
		return false;
	}

	for (;;)
	{
		if (cancellationRequested != nullptr && cancellationRequested())
		{
			bootstrap.stop();
			impl_->fail(
				FullEngineCoopClientRuntimeError::BootstrapCancelled);
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
			impl_->fail(FullEngineCoopClientRuntimeError::BootstrapFailed);
			return false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	CoopSession::CoopCampaignBootstrapDescriptor descriptor;
	if (!bootstrap.descriptor(descriptor) ||
		!CoopSession::IsValidCoopCampaignBootstrapDescriptor(descriptor))
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::DescriptorUnavailable);
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
			FullEngineCoopClientRuntimeError::StateDirectoryInvalid);
		return false;
	}
	if (!stateRoot.is_absolute())
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::StateDirectoryInvalid);
		return false;
	}

	if (impl_->scratch.prepare(stateRoot, descriptor) !=
		CoopSession::
			FullEngineCoopClientCampaignScratchPrepareResult::Success)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::ScratchPrepareFailed);
		return false;
	}
	if (InstallGameSimulationRandom(descriptor.campaignSeed) !=
		GameSimulationRandomInstallError::None)
	{
		impl_->scratch.close();
		impl_->fail(
			FullEngineCoopClientRuntimeError::
				SimulationRandomInstallFailed);
		return false;
	}

	impl_->descriptor = descriptor;
	if (!impl_->lifecycle.markPrepared())
	{
		impl_->scratch.close();
		impl_->fail(FullEngineCoopClientRuntimeError::InvalidState);
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
		impl_->fail(FullEngineCoopClientRuntimeError::InvalidState);
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
		impl_->fail(FullEngineCoopClientRuntimeError::ContentManifestFailed);
		return false;
	}
	if (content != impl_->descriptor.contentManifestSha256)
	{
		impl_->fail(FullEngineCoopClientRuntimeError::CompatibilityMismatch);
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
		impl_->fail(FullEngineCoopClientRuntimeError::InvalidState);
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
			FullEngineCoopClientRuntimeError::RuntimeFingerprintFailed);
		return false;
	}
	if (fingerprint != impl_->descriptor.runtimeFingerprint)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::CompatibilityMismatch);
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
			impl_->fail(FullEngineCoopClientRuntimeError::InvalidState);
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
				ReconnectCredentialLoadFailed);
			return false;
		}
	}
	else if (loaded !=
			CoopSession::FullEngineCoopReconnectCredentialLoadResult::Missing &&
		loaded !=
			CoopSession::FullEngineCoopReconnectCredentialLoadResult::Loaded)
	{
		impl_->fail(
			FullEngineCoopClientRuntimeError::ReconnectCredentialLoadFailed);
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
			FullEngineCoopClientRuntimeError::ClientConfigurationFailed);
		return false;
	}
	CoopSession::FullEngineCoopClientConfiguration client;
	client.protocolVersion = impl_->descriptor.protocolVersion;
	client.runtimeFingerprint = impl_->descriptor.runtimeFingerprint;
	client.contentManifestSha256 =
		impl_->descriptor.contentManifestSha256;
	client.expectedSessionEpoch = impl_->descriptor.sessionEpoch;
	client.durableReconnectCredentialRequired = true;
	if (impl_->composition->client.configure(client) !=
		CoopSession::FullEngineCoopClientResult::Success)
	{
		impl_->composition.reset();
		impl_->fail(
			FullEngineCoopClientRuntimeError::ClientConfigurationFailed);
		return false;
	}
	if (loaded ==
			CoopSession::FullEngineCoopReconnectCredentialLoadResult::Loaded &&
		impl_->composition->client.restoreReconnectCredential(
			restoredCredential) != CoopSession::
				FullEngineCoopClientResult::Success)
	{
		impl_->composition.reset();
		impl_->fail(
			FullEngineCoopClientRuntimeError::ReconnectCredentialRestoreFailed);
		return false;
	}
	if (!impl_->startTransport())
	{
		impl_->composition.reset();
		impl_->fail(
			FullEngineCoopClientRuntimeError::TransportConnectFailed);
		return false;
	}

	if (!impl_->lifecycle.markNetworkOpen())
	{
		impl_->composition->transport.stop(0);
		impl_->composition.reset();
		impl_->fail(FullEngineCoopClientRuntimeError::InvalidState);
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

	if (impl_->pendingError != FullEngineCoopClientRuntimeError::None)
	{
		const FullEngineCoopClientRuntimeError pending = impl_->pendingError;
		impl_->pendingError = FullEngineCoopClientRuntimeError::None;
		impl_->fail(pending);
	}
	if (!impl_->lifecycle.failed() && live.client.sessionEpoch() != 0 &&
		live.client.sessionEpoch() != impl_->descriptor.sessionEpoch)
		impl_->fail(
			FullEngineCoopClientRuntimeError::LiveServerDescriptorMismatch);
	if (!impl_->lifecycle.failed() && live.client.state() ==
		CoopSession::FullEngineCoopClientState::Failed)
		impl_->fail(live.client.lastResult() == CoopSession::
			FullEngineCoopClientResult::CredentialStorageFailure
			? FullEngineCoopClientRuntimeError::
				ReconnectCredentialStorageFailed
				: FullEngineCoopClientRuntimeError::TacticalClientFailed);
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
		impl_->fail(FullEngineCoopClientRuntimeError::CampaignSyncFailed);
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
		impl_->fail(FullEngineCoopClientRuntimeError::CampaignSyncFailed);
		stopTransport();
		return;
	}

	if (live.campaign.hasPendingOutbound())
	{
		const auto flushed = live.campaign.flushOutbound();
		if (!CampaignResultKeepsTransportOpen(flushed))
		{
			impl_->fail(
				FullEngineCoopClientRuntimeError::CampaignSyncFailed);
			stopTransport();
			return;
		}
	}
	const bool wasCampaignReady = impl_->lifecycle.campaignReady();
	impl_->lifecycle.markCampaignReady(live.campaign.state() ==
		CoopSession::FullEngineCoopCampaignSyncClientState::Ready);
	if (!wasCampaignReady && impl_->lifecycle.campaignReady())
	{
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

	if (live.transport.running())
	{
		impl_->reconnectScheduled = false;
		return;
	}

	impl_->retireCampaignConnection();
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
		impl_->fail(
			FullEngineCoopClientRuntimeError::ReconnectLimitReached);
		stopTransport();
		return;
	}
	if (impl_->reconnectAttempts !=
		(std::numeric_limits<unsigned>::max)())
		++impl_->reconnectAttempts;
	if (impl_->startTransport())
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

bool FullEngineCoopClientRuntime::retired() const noexcept
{
	return impl_ != nullptr && impl_->lifecycle.retired();
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
		output.assignedActors[index] = client.assignedActor(index);
	output.outstandingCommandId = client.outstandingCommandId();
	output.state = client.acceptedState();
	output.resynchronizing = client.resyncPending();
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
