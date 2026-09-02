#include "FullEngineCoopClientBootstrapTransport.h"

#include "CoopHandshakeProtocol.h"

#include <SDL3/SDL.h>

#include <cstring>

namespace CoopSession
{
FullEngineCoopClientBootstrapTransport::
	FullEngineCoopClientBootstrapTransport() noexcept
	: mainThread_(std::this_thread::get_id())
{
}

FullEngineCoopClientBootstrapTransport::
	~FullEngineCoopClientBootstrapTransport()
{
	// This object is a process-startup local and is normally destroyed on its
	// constructing thread. Never publish a partially completed descriptor from
	// destructor cleanup.
	descriptorAvailable_ = false;
	closePending_ = true;
	pendingResult_ = FullEngineCoopClientBootstrapTransportResult::Stopped;
	finishClose();
}

FullEngineCoopClientBootstrapTransportConnectResult
FullEngineCoopClientBootstrapTransport::connect(
	const FullEngineCoopClientBootstrapTransportConfiguration&
		configuration) noexcept
{
	if (!onMainThread())
		return FullEngineCoopClientBootstrapTransportConnectResult::WrongThread;
	if (state_ != FullEngineCoopClientBootstrapTransportState::Idle ||
		transport_ != nullptr || pollDepth_ != 0 || handlerDepth_ != 0 ||
		closePending_)
		return FullEngineCoopClientBootstrapTransportConnectResult::
			LifecycleBusy;
	if (!validConfiguration(configuration))
		return FullEngineCoopClientBootstrapTransportConnectResult::
			InvalidConfiguration;

	try
	{
		transport_ = ja2::mp::net::CreateSdlNetPeer();
	}
	catch (...)
	{
		transport_ = nullptr;
	}
	if (transport_ == nullptr)
		return FullEngineCoopClientBootstrapTransportConnectResult::
			TransportUnavailable;

	bool registered = false;
	try
	{
		registered = registerMessages();
	}
	catch (...)
	{
		registered = false;
	}
	if (!registered)
	{
		try
		{
			transport_->Shutdown(0);
		}
		catch (...)
		{
		}
		ja2::mp::net::DestroySdlNetPeer(transport_);
		transport_ = nullptr;
		return FullEngineCoopClientBootstrapTransportConnectResult::
			TransportUnavailable;
	}

	bool started = false;
	try
	{
		// Port zero creates no listener. The peer can originate exactly one
		// connection and cannot accept an inbound or self connection.
		started = transport_->Start(
			1, ja2::mp::net::SdlNetEndpoint());
	}
	catch (...)
	{
		started = false;
	}
	if (!started)
	{
		ja2::mp::net::DestroySdlNetPeer(transport_);
		transport_ = nullptr;
		return FullEngineCoopClientBootstrapTransportConnectResult::
			TransportStartFailed;
	}
	transport_->SetMaximumIncomingConnections(0);
	transport_->SetTimeout(configuration.timeoutMilliseconds);

	bool connecting = false;
	try
	{
		connecting = transport_->Connect(
			configuration.serverEndpoint.host,
			configuration.serverEndpoint.port);
	}
	catch (...)
	{
		connecting = false;
	}
	if (!connecting)
	{
		try
		{
			transport_->Shutdown(0);
		}
		catch (...)
		{
		}
		ja2::mp::net::DestroySdlNetPeer(transport_);
		transport_ = nullptr;
		return FullEngineCoopClientBootstrapTransportConnectResult::
			TransportConnectFailed;
	}

	server_ = ja2::mp::NoConnection;
	pendingAccepted_ = ja2::mp::NoConnection;
	callbackSender_ = ja2::mp::NoConnection;
	clearInbound();
	hello_ = CoopServerHello{};
	stagedDescriptor_ = CoopCampaignBootstrapDescriptor{};
	descriptor_ = CoopCampaignBootstrapDescriptor{};
	descriptorAvailable_ = false;
	maximumPendingWriteBytes_ = configuration.maximumPendingWriteBytes;
	timeoutMilliseconds_ = configuration.timeoutMilliseconds;
	startedAtMilliseconds_ = static_cast<std::uint64_t>(SDL_GetTicks());
	result_ = FullEngineCoopClientBootstrapTransportResult::None;
	pendingResult_ = FullEngineCoopClientBootstrapTransportResult::None;
	closePending_ = false;
	wrongThreadObserved_.store(false, std::memory_order_release);
	state_ = FullEngineCoopClientBootstrapTransportState::Connecting;
	return FullEngineCoopClientBootstrapTransportConnectResult::Success;
}

void FullEngineCoopClientBootstrapTransport::poll() noexcept
{
	if (!onMainThread())
	{
		wrongThreadObserved_.store(true, std::memory_order_release);
		return;
	}
	if (wrongThreadObserved_.exchange(false, std::memory_order_acq_rel) &&
		active())
		fail(FullEngineCoopClientBootstrapTransportResult::WrongThread);
	if (closePending_)
	{
		finishClose();
		return;
	}
	if (!active() || transport_ == nullptr) return;
	if (pollDepth_ != 0) return;
	if (timeoutExpired())
	{
		fail(FullEngineCoopClientBootstrapTransportResult::Timeout);
		finishClose();
		return;
	}

	++pollDepth_;
	try
	{
		while (!closePending_)
		{
			ja2::mp::net::SdlNetEvent* const event = transport_->Poll();
			if (event == nullptr) break;
			handleEvent(*event);
			transport_->Release(event);
		}
	}
	catch (...)
	{
		fail(FullEngineCoopClientBootstrapTransportResult::TransportFailure);
	}

	// Every SDL message callback is now off the stack. Only this region decodes
	// protocol bytes or advances the handshake state.
	if (!closePending_ && pendingAccepted_)
	{
		if (callbackSender_ && callbackSender_ != pendingAccepted_)
			fail(FullEngineCoopClientBootstrapTransportResult::
				UnexpectedConnection);
		else
		{
			server_ = pendingAccepted_;
			pendingAccepted_ = ja2::mp::NoConnection;
			state_ =
				FullEngineCoopClientBootstrapTransportState::AwaitingHello;
		}
	}
	if (!closePending_ && inboundCount_ != 0 && !server_)
		fail(FullEngineCoopClientBootstrapTransportResult::
			UnexpectedConnection);
	if (!closePending_ && timeoutExpired())
		fail(FullEngineCoopClientBootstrapTransportResult::Timeout);
	if (!closePending_ && server_)
	{
		std::size_t pending = 0;
		if (!transport_->PendingWriteBytes(server_, pending))
			fail(FullEngineCoopClientBootstrapTransportResult::
				TransportFailure);
		else if (pending > maximumPendingWriteBytes_)
			fail(FullEngineCoopClientBootstrapTransportResult::
				PendingWriteLimit);
	}
	if (!closePending_ && server_) deliverInbound();
	if (!closePending_ && timeoutExpired())
		fail(FullEngineCoopClientBootstrapTransportResult::Timeout);

	--pollDepth_;
	if (pollDepth_ == 0 && closePending_) finishClose();
}

void FullEngineCoopClientBootstrapTransport::stop() noexcept
{
	if (!onMainThread())
	{
		wrongThreadObserved_.store(true, std::memory_order_release);
		return;
	}
	if (state_ == FullEngineCoopClientBootstrapTransportState::Complete ||
		state_ == FullEngineCoopClientBootstrapTransportState::Failed ||
		state_ == FullEngineCoopClientBootstrapTransportState::Stopped)
		return;
	requestStop();
	if (pollDepth_ == 0 && handlerDepth_ == 0) finishClose();
}

bool FullEngineCoopClientBootstrapTransport::descriptor(
	CoopCampaignBootstrapDescriptor& descriptor) const noexcept
{
	if (!descriptorAvailable_ ||
		state_ != FullEngineCoopClientBootstrapTransportState::Complete ||
		result_ != FullEngineCoopClientBootstrapTransportResult::Success ||
		transport_ != nullptr)
		return false;
	descriptor = descriptor_;
	return true;
}

void FullEngineCoopClientBootstrapTransport::HandleServerHello(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	QueueFromCallback(message, context, InboundKind::ServerHello);
}

void FullEngineCoopClientBootstrapTransport::HandleCampaignBootstrap(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	QueueFromCallback(message, context, InboundKind::CampaignBootstrap);
}

void FullEngineCoopClientBootstrapTransport::QueueFromCallback(
	ja2::mp::net::SdlNetMessage* message, void* context,
	InboundKind kind) noexcept
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopClientBootstrapTransport& adapter =
		*static_cast<FullEngineCoopClientBootstrapTransport*>(context);
	++adapter.handlerDepth_;
	adapter.queueInbound(*message, kind);
	if (adapter.handlerDepth_ != 0) --adapter.handlerDepth_;
}

void FullEngineCoopClientBootstrapTransport::queueInbound(
	const ja2::mp::net::SdlNetMessage& message, InboundKind kind) noexcept
{
	if (!active() || closePending_) return;
	if (!message.sender || message.sender == ja2::mp::AnyConnection ||
		(server_ && message.sender != server_) ||
		(callbackSender_ && message.sender != callbackSender_))
	{
		fail(FullEngineCoopClientBootstrapTransportResult::
			UnexpectedConnection);
		return;
	}

	const std::size_t expectedSize = kind == InboundKind::ServerHello
		? CoopServerHelloWireSize
		: CoopCampaignBootstrapWireSize;
	if (message.size != expectedSize || message.data == nullptr)
	{
		fail(FullEngineCoopClientBootstrapTransportResult::
			InboundWrongSize);
		return;
	}
	if (inboundCount_ >= inbound_.size())
	{
		fail(FullEngineCoopClientBootstrapTransportResult::
			InboundCapacityReached);
		return;
	}

	if (!callbackSender_) callbackSender_ = message.sender;
	const std::size_t insertion =
		(inboundHead_ + inboundCount_) % inbound_.size();
	InboundMessage& queued = inbound_[insertion];
	queued = InboundMessage{};
	queued.kind = kind;
	queued.sender = message.sender;
	queued.size = message.size;
	std::memcpy(queued.bytes.data(), message.data, message.size);
	++inboundCount_;
}

void FullEngineCoopClientBootstrapTransport::handleEvent(
	const ja2::mp::net::SdlNetEvent& event) noexcept
{
	if (event.size != 1 || event.data == nullptr)
	{
		fail(FullEngineCoopClientBootstrapTransportResult::
			UnexpectedTransportEvent);
		return;
	}
	if (event.data[0] ==
		ja2::mp::net::SDLNET_CONNECTION_ATTEMPT_FAILED)
	{
		fail(FullEngineCoopClientBootstrapTransportResult::
			ConnectionAttemptFailed);
		return;
	}
	if (!event.connection || event.connection == ja2::mp::AnyConnection)
	{
		fail(FullEngineCoopClientBootstrapTransportResult::
			UnexpectedTransportEvent);
		return;
	}

	switch (event.data[0])
	{
		case ja2::mp::net::SDLNET_CONNECTION_ACCEPTED:
			if (state_ !=
					FullEngineCoopClientBootstrapTransportState::Connecting ||
				server_ || pendingAccepted_ ||
				(callbackSender_ && callbackSender_ != event.connection))
			{
				fail(FullEngineCoopClientBootstrapTransportResult::
					UnexpectedConnection);
				return;
			}
			pendingAccepted_ = event.connection;
			return;
		case ja2::mp::net::SDLNET_DISCONNECTION_NOTIFICATION:
		case ja2::mp::net::SDLNET_CONNECTION_LOST:
			if ((server_ && event.connection != server_) ||
				(pendingAccepted_ && event.connection != pendingAccepted_) ||
				(callbackSender_ && event.connection != callbackSender_))
			{
				fail(FullEngineCoopClientBootstrapTransportResult::
					UnexpectedConnection);
				return;
			}
			fail(FullEngineCoopClientBootstrapTransportResult::
				ConnectionLost);
			return;
		case ja2::mp::net::SDLNET_NEW_INCOMING_CONNECTION:
		case ja2::mp::net::SDLNET_NO_FREE_INCOMING_CONNECTIONS:
			fail(FullEngineCoopClientBootstrapTransportResult::
				UnexpectedConnection);
			return;
		default:
			fail(FullEngineCoopClientBootstrapTransportResult::
				UnexpectedTransportEvent);
			return;
	}
}

void FullEngineCoopClientBootstrapTransport::deliverInbound() noexcept
{
	while (!closePending_ && inboundCount_ != 0)
	{
		const InboundMessage message = inbound_[inboundHead_];
		inbound_[inboundHead_] = InboundMessage{};
		inboundHead_ = (inboundHead_ + 1) % inbound_.size();
		--inboundCount_;
		if (inboundCount_ == 0) inboundHead_ = 0;
		if (message.sender != server_)
		{
			fail(FullEngineCoopClientBootstrapTransportResult::
				UnexpectedConnection);
			break;
		}

		if (state_ ==
			FullEngineCoopClientBootstrapTransportState::AwaitingHello)
		{
			if (message.kind != InboundKind::ServerHello)
			{
				fail(FullEngineCoopClientBootstrapTransportResult::
					WrongMessageOrder);
				break;
			}
			CoopServerHello decoded;
			if (DecodeCoopServerHello(message.bytes.data(), message.size,
				decoded) != CoopServerHelloDecodeResult::Success)
			{
				fail(FullEngineCoopClientBootstrapTransportResult::
					HelloDecodeFailed);
				break;
			}
			hello_ = decoded;
			state_ = FullEngineCoopClientBootstrapTransportState::
				AwaitingBootstrap;
			continue;
		}

		if (state_ ==
			FullEngineCoopClientBootstrapTransportState::AwaitingBootstrap)
		{
			if (message.kind != InboundKind::CampaignBootstrap)
			{
				fail(FullEngineCoopClientBootstrapTransportResult::
					WrongMessageOrder);
				break;
			}
			CoopCampaignBootstrapDescriptor decoded;
			if (DecodeCoopCampaignBootstrap(message.bytes.data(),
				message.size, decoded) !=
					CoopCampaignBootstrapDecodeResult::Success)
			{
				fail(FullEngineCoopClientBootstrapTransportResult::
					BootstrapDecodeFailed);
				break;
			}
			if (decoded.protocolVersion != hello_.protocolVersion ||
				decoded.sessionEpoch != hello_.sessionEpoch ||
				decoded.runtimeFingerprint != hello_.runtimeFingerprint ||
				decoded.contentManifestSha256 !=
					hello_.contentManifestSha256)
			{
				fail(FullEngineCoopClientBootstrapTransportResult::
					DescriptorMismatch);
				break;
			}
			// Completion consumes exactly two frames. A registered duplicate
			// already copied by this Poll is an ordering violation, not trailing
			// data that may be hidden by closing the one-shot socket.
			if (inboundCount_ != 0)
			{
				fail(FullEngineCoopClientBootstrapTransportResult::
					WrongMessageOrder);
				break;
			}
			requestCompletion(decoded);
			continue;
		}

		fail(FullEngineCoopClientBootstrapTransportResult::
			WrongMessageOrder);
	}
}

void FullEngineCoopClientBootstrapTransport::fail(
	FullEngineCoopClientBootstrapTransportResult result) noexcept
{
	if (closePending_) return;
	pendingResult_ = result;
	descriptorAvailable_ = false;
	closePending_ = true;
}

void FullEngineCoopClientBootstrapTransport::requestCompletion(
	const CoopCampaignBootstrapDescriptor& descriptor) noexcept
{
	if (closePending_) return;
	stagedDescriptor_ = descriptor;
	pendingResult_ =
		FullEngineCoopClientBootstrapTransportResult::Success;
	closePending_ = true;
}

void FullEngineCoopClientBootstrapTransport::requestStop() noexcept
{
	if (closePending_) return;
	pendingResult_ =
		FullEngineCoopClientBootstrapTransportResult::Stopped;
	descriptorAvailable_ = false;
	closePending_ = true;
}

void FullEngineCoopClientBootstrapTransport::finishClose() noexcept
{
	if (!closePending_ || pollDepth_ != 0 || handlerDepth_ != 0) return;
	const FullEngineCoopClientBootstrapTransportResult terminal =
		pendingResult_;

	clearInbound();
	server_ = ja2::mp::NoConnection;
	pendingAccepted_ = ja2::mp::NoConnection;
	callbackSender_ = ja2::mp::NoConnection;
	if (transport_ != nullptr)
	{
		try
		{
			transport_->Shutdown(0);
		}
		catch (...)
		{
		}
		ja2::mp::net::DestroySdlNetPeer(transport_);
		transport_ = nullptr;
	}
	timeoutMilliseconds_ = 0;
	maximumPendingWriteBytes_ = 0;
	startedAtMilliseconds_ = 0;
	closePending_ = false;
	pendingResult_ = FullEngineCoopClientBootstrapTransportResult::None;

	if (terminal == FullEngineCoopClientBootstrapTransportResult::Success)
	{
		// The peer is already gone. Only now may the caller observe Complete or
		// copy the immutable descriptor into pre-RNG process state.
		descriptor_ = stagedDescriptor_;
		descriptorAvailable_ = true;
		result_ = FullEngineCoopClientBootstrapTransportResult::Success;
		state_ = FullEngineCoopClientBootstrapTransportState::Complete;
	}
	else if (terminal ==
		FullEngineCoopClientBootstrapTransportResult::Stopped)
	{
		descriptorAvailable_ = false;
		result_ = FullEngineCoopClientBootstrapTransportResult::Stopped;
		state_ = FullEngineCoopClientBootstrapTransportState::Stopped;
	}
	else
	{
		descriptorAvailable_ = false;
		result_ = terminal ==
				FullEngineCoopClientBootstrapTransportResult::None
			? FullEngineCoopClientBootstrapTransportResult::TransportFailure
			: terminal;
		state_ = FullEngineCoopClientBootstrapTransportState::Failed;
	}
	stagedDescriptor_ = CoopCampaignBootstrapDescriptor{};
	hello_ = CoopServerHello{};
}

void FullEngineCoopClientBootstrapTransport::clearInbound() noexcept
{
	inbound_ = {};
	inboundHead_ = 0;
	inboundCount_ = 0;
}

bool FullEngineCoopClientBootstrapTransport::registerMessages() noexcept
{
	if (transport_ == nullptr) return false;
	try
	{
		return transport_->RegisterMessage(CoopServerHelloMessageName,
			&FullEngineCoopClientBootstrapTransport::HandleServerHello,
			this) &&
			transport_->RegisterMessage(CoopCampaignBootstrapMessageName,
				&FullEngineCoopClientBootstrapTransport::
					HandleCampaignBootstrap,
				this);
	}
	catch (...)
	{
		return false;
	}
}

bool FullEngineCoopClientBootstrapTransport::validConfiguration(
	const FullEngineCoopClientBootstrapTransportConfiguration& configuration)
	const noexcept
{
	return configuration.serverEndpoint.port != 0 &&
		configuration.serverEndpoint.host[0] != '\0' &&
		std::memchr(configuration.serverEndpoint.host, '\0',
			sizeof(configuration.serverEndpoint.host)) != nullptr &&
		configuration.timeoutMilliseconds != 0 &&
		configuration.timeoutMilliseconds <=
			MaximumFullEngineCoopClientBootstrapTimeoutMilliseconds &&
		configuration.maximumPendingWriteBytes != 0 &&
		configuration.maximumPendingWriteBytes <=
			MaximumFullEngineCoopClientBootstrapPendingWriteBytes;
}

bool FullEngineCoopClientBootstrapTransport::timeoutExpired() const noexcept
{
	return timeoutMilliseconds_ != 0 &&
		static_cast<std::uint64_t>(SDL_GetTicks()) -
			startedAtMilliseconds_ >= timeoutMilliseconds_;
}

bool FullEngineCoopClientBootstrapTransport::onMainThread() const noexcept
{
	return std::this_thread::get_id() == mainThread_;
}

bool FullEngineCoopClientBootstrapTransport::active() const noexcept
{
	return state_ == FullEngineCoopClientBootstrapTransportState::Connecting ||
		state_ ==
			FullEngineCoopClientBootstrapTransportState::AwaitingHello ||
		state_ ==
			FullEngineCoopClientBootstrapTransportState::AwaitingBootstrap;
}
}
