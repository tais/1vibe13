#include "FullEngineCoopClientTransport.h"

#include "CoopHandshakeProtocol.h"

#include <algorithm>
#include <cstring>
#include <new>

namespace CoopSession
{
namespace
{
constexpr std::size_t SdlMessageFrameOverhead = 6;

bool SameName(const char* left, const char* right) noexcept
{
	return left != nullptr && right != nullptr &&
		std::strcmp(left, right) == 0;
}
}

FullEngineCoopClientTransport::FullEngineCoopClientTransport() noexcept
	: inbound_(new (std::nothrow)
		InboundMessage[MaximumFullEngineCoopClientInboundMessages]),
	  mainThread_(std::this_thread::get_id())
{
}

FullEngineCoopClientTransport::~FullEngineCoopClientTransport()
{
	// The core is normally destroyed first because this adapter must be
	// constructed first. Never call an external core from a destructor even if
	// a caller chose a different lifetime arrangement.
	client_ = nullptr;
	campaignSink_ = nullptr;
	notifyClientOnClose_ = false;
	preserveClientStateOnClose_ = true;
	requestClose(false, 0);
	finishClose();
}

FullEngineCoopClientTransportConnectResult
FullEngineCoopClientTransport::connect(
	FullEngineCoopClient& client,
	const FullEngineCoopClientTransportConfiguration& configuration) noexcept
{
	return connectInternal(client, nullptr, configuration);
}

FullEngineCoopClientTransportConnectResult
FullEngineCoopClientTransport::connect(
	FullEngineCoopClient& client,
	FullEngineCoopClientCampaignSyncSink& campaignSink,
	const FullEngineCoopClientTransportConfiguration& configuration) noexcept
{
	return connectInternal(client, &campaignSink, configuration);
}

FullEngineCoopClientTransportConnectResult
FullEngineCoopClientTransport::connectInternal(
	FullEngineCoopClient& client,
	FullEngineCoopClientCampaignSyncSink* campaignSink,
	const FullEngineCoopClientTransportConfiguration& configuration) noexcept
{
	if (!onMainThread())
		return FullEngineCoopClientTransportConnectResult::WrongThread;
	if (running_ || transport_ != nullptr || client_ != nullptr ||
		pollDepth_ != 0 || handlerDepth_ != 0 || closePending_)
		return FullEngineCoopClientTransportConnectResult::LifecycleBusy;
	if (!validConfiguration(configuration))
		return FullEngineCoopClientTransportConnectResult::InvalidConfiguration;
	if (!inbound_)
		return FullEngineCoopClientTransportConnectResult::TransportUnavailable;
	if (client.beginConnection() != FullEngineCoopClientResult::Success)
		return FullEngineCoopClientTransportConnectResult::ClientRejected;

	client_ = &client;
	campaignSink_ = campaignSink;
	maximumQueuedInboundMessages_ =
		configuration.maximumQueuedInboundMessages;
	maximumPendingWriteBytes_ = configuration.maximumPendingWriteBytes;
	lastFailure_ = FullEngineCoopClientTransportFailure::None;
	server_ = ja2::mp::NoConnection;
	pendingAccepted_ = ja2::mp::NoConnection;
	clearInbound();
	closePending_ = false;
	notifyClientOnClose_ = false;
	preserveClientStateOnClose_ = false;
	pendingDrainMilliseconds_ = 0;

	try
	{
		transport_ = ja2::mp::net::CreateSdlNetPeer();
	}
	catch (...)
	{
		transport_ = nullptr;
	}
	if (transport_ == nullptr)
	{
		client.transportDisconnected();
		client_ = nullptr;
		campaignSink_ = nullptr;
		maximumQueuedInboundMessages_ = 0;
		maximumPendingWriteBytes_ = 0;
		return FullEngineCoopClientTransportConnectResult::TransportUnavailable;
	}

	bool registered = false;
	try
	{
		registered = registerMessages();
		if (registered && campaignSink != nullptr)
		{
			ja2::mp::net::SdlNetInboundMessageBudget campaignBudget;
			campaignBudget.sustainedBytesPerSecond =
				FullEngineCoopCampaignInboundRateBytesPerSecond;
			campaignBudget.burstBytes =
				FullEngineCoopCampaignInboundBurstBytes;
			registered = transport_->SetInboundMessageBudget(campaignBudget);
		}
	}
	catch (...)
	{
		registered = false;
	}
	if (!registered)
	{
		transport_->Shutdown(0);
		ja2::mp::net::DestroySdlNetPeer(transport_);
		transport_ = nullptr;
		client.transportDisconnected();
		client_ = nullptr;
		campaignSink_ = nullptr;
		maximumQueuedInboundMessages_ = 0;
		maximumPendingWriteBytes_ = 0;
		return FullEngineCoopClientTransportConnectResult::TransportUnavailable;
	}

	bool started = false;
	try
	{
		// Port zero creates no listener. This peer can originate exactly one
		// connection and cannot accept an inbound/self connection.
		started = transport_->Start(1, ja2::mp::net::SdlNetEndpoint());
	}
	catch (...)
	{
		started = false;
	}
	if (!started)
	{
		ja2::mp::net::DestroySdlNetPeer(transport_);
		transport_ = nullptr;
		client.transportDisconnected();
		client_ = nullptr;
		campaignSink_ = nullptr;
		maximumQueuedInboundMessages_ = 0;
		maximumPendingWriteBytes_ = 0;
		return FullEngineCoopClientTransportConnectResult::TransportStartFailed;
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
		transport_->Shutdown(0);
		ja2::mp::net::DestroySdlNetPeer(transport_);
		transport_ = nullptr;
		client.transportDisconnected();
		client_ = nullptr;
		campaignSink_ = nullptr;
		maximumQueuedInboundMessages_ = 0;
		maximumPendingWriteBytes_ = 0;
		return FullEngineCoopClientTransportConnectResult::TransportConnectFailed;
	}

	running_ = true;
	connected_ = false;
	return FullEngineCoopClientTransportConnectResult::Success;
}

void FullEngineCoopClientTransport::poll() noexcept
{
	if (!onMainThread())
	{
		lastFailure_ = FullEngineCoopClientTransportFailure::WrongThread;
		return;
	}
	if ((!running_ || transport_ == nullptr) && !closePending_) return;
	if (pollDepth_ != 0) return;

	++pollDepth_;
	if (!closePending_ && transport_ != nullptr)
	{
		try
		{
			for (;;)
			{
				ja2::mp::net::SdlNetEvent* event = transport_->Poll();
				if (event == nullptr) break;
				handleEvent(*event);
				transport_->Release(event);
			}
		}
		catch (...)
		{
			failTransport(
				FullEngineCoopClientTransportFailure::TransportFailure);
		}
	}

	// The entire SDL pump is now off the stack. Only this section may mutate
	// the core or invoke the passive replica boundary.
	if (!closePending_ && pendingAccepted_)
	{
		server_ = pendingAccepted_;
		pendingAccepted_ = ja2::mp::NoConnection;
		connected_ = true;
		if (client_ == nullptr ||
			client_->transportConnected() !=
				FullEngineCoopClientResult::Success)
			failTransport(
				FullEngineCoopClientTransportFailure::ClientRejected);
	}
	if (!closePending_ && connected_) deliverInbound();

	--pollDepth_;
	if (pollDepth_ == 0 && closePending_) finishClose();
}

void FullEngineCoopClientTransport::stop(
	unsigned drainMilliseconds) noexcept
{
	if (!onMainThread())
	{
		lastFailure_ = FullEngineCoopClientTransportFailure::WrongThread;
		return;
	}
	requestClose(true, drainMilliseconds);
	if (pollDepth_ == 0 && handlerDepth_ == 0) finishClose();
}

bool FullEngineCoopClientTransport::send(const char* messageName,
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (!onMainThread() || !running_ || !connected_ || closePending_ ||
		transport_ == nullptr || client_ == nullptr || !server_ ||
		server_ == ja2::mp::AnyConnection || bytes == nullptr ||
		!validOutbound(messageName, size))
	{
		if (running_ && onMainThread())
			failTransport(
				FullEngineCoopClientTransportFailure::TransportFailure);
		return false;
	}

	std::size_t pending = 0;
	if (!transport_->PendingWriteBytes(server_, pending))
	{
		failTransport(
			FullEngineCoopClientTransportFailure::TransportFailure);
		return false;
	}
	const std::size_t nameSize = std::strlen(messageName);
	if (nameSize > maximumPendingWriteBytes_ ||
		size > maximumPendingWriteBytes_ - nameSize ||
		SdlMessageFrameOverhead >
			maximumPendingWriteBytes_ - nameSize - size)
	{
		failTransport(
			FullEngineCoopClientTransportFailure::PendingWriteLimit);
		return false;
	}
	const std::size_t frameSize =
		SdlMessageFrameOverhead + nameSize + size;
	if (pending > maximumPendingWriteBytes_ ||
		frameSize > maximumPendingWriteBytes_ - pending)
	{
		failTransport(
			FullEngineCoopClientTransportFailure::PendingWriteLimit);
		return false;
	}
	if (!transport_->SendMessage(
		messageName, bytes, size, server_, false))
	{
		failTransport(
			FullEngineCoopClientTransportFailure::TransportFailure);
		return false;
	}
	return true;
}

void FullEngineCoopClientTransport::close() noexcept
{
	if (!onMainThread()) return;
	// FullEngineCoopClient calls this while its own transition is still active.
	// Preserve Failed/ResyncRequired (or the explicit disconnect state it sets
	// immediately afterwards) and defer peer destruction until poll unwinds.
	preserveClientStateOnClose_ = true;
	notifyClientOnClose_ = false;
	requestClose(false, 0);
}

void FullEngineCoopClientTransport::HandleServerHello(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	QueueFromCallback(message, context, InboundKind::ServerHello);
}

void FullEngineCoopClientTransport::HandleAdmissionResponse(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	QueueFromCallback(message, context, InboundKind::AdmissionResponse);
}

void FullEngineCoopClientTransport::HandleTacticalBaseline(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	QueueFromCallback(message, context, InboundKind::TacticalBaseline);
}

void FullEngineCoopClientTransport::HandleTacticalDelta(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	QueueFromCallback(message, context, InboundKind::TacticalDelta);
}

void FullEngineCoopClientTransport::HandleTacticalReceipt(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	QueueFromCallback(message, context, InboundKind::TacticalReceipt);
}

void FullEngineCoopClientTransport::HandleSelfRetirementResult(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	QueueFromCallback(message, context, InboundKind::SelfRetirementResult);
}

void FullEngineCoopClientTransport::HandleCampaignMetadata(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	QueueFromCallback(message, context, InboundKind::CampaignMetadata);
}

void FullEngineCoopClientTransport::HandleCampaignChunk(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	QueueFromCallback(message, context, InboundKind::CampaignChunk);
}

void FullEngineCoopClientTransport::HandleCampaignComplete(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	QueueFromCallback(message, context, InboundKind::CampaignComplete);
}

void FullEngineCoopClientTransport::HandleCampaignReject(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	QueueFromCallback(message, context, InboundKind::CampaignReject);
}

void FullEngineCoopClientTransport::QueueFromCallback(
	ja2::mp::net::SdlNetMessage* message, void* context,
	InboundKind kind) noexcept
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopClientTransport& adapter =
		*static_cast<FullEngineCoopClientTransport*>(context);
	++adapter.handlerDepth_;
	adapter.queueInbound(*message, kind);
	if (adapter.handlerDepth_ != 0) --adapter.handlerDepth_;
}

void FullEngineCoopClientTransport::queueInbound(
	const ja2::mp::net::SdlNetMessage& message,
	InboundKind kind) noexcept
{
	if (!running_ || closePending_) return;
	if (!message.sender || message.sender == ja2::mp::AnyConnection ||
		(server_ && message.sender != server_))
	{
		failTransport(
			FullEngineCoopClientTransportFailure::UnexpectedConnection);
		return;
	}
	if (!validInbound(kind, message.size) ||
		(message.size != 0 && message.data == nullptr))
	{
		failTransport(
			FullEngineCoopClientTransportFailure::InboundMessageTooLarge);
		return;
	}
	if (!inbound_)
	{
		failTransport(FullEngineCoopClientTransportFailure::TransportFailure);
		return;
	}
	if (inboundCount_ >= maximumQueuedInboundMessages_ ||
		inboundCount_ >= MaximumFullEngineCoopClientInboundMessages)
	{
		failTransport(
			FullEngineCoopClientTransportFailure::InboundCapacityReached);
		return;
	}

	const std::size_t insertion =
		(inboundHead_ + inboundCount_) %
			MaximumFullEngineCoopClientInboundMessages;
	InboundMessage& queued = inbound_[insertion];
	queued = InboundMessage{};
	queued.kind = kind;
	queued.sender = message.sender;
	queued.size = message.size;
	if (message.size != 0)
		std::memcpy(queued.bytes.data(), message.data, message.size);
	++inboundCount_;
}

void FullEngineCoopClientTransport::handleEvent(
	const ja2::mp::net::SdlNetEvent& event) noexcept
{
	if (event.size == 0 || event.data == nullptr)
	{
		failTransport(
			FullEngineCoopClientTransportFailure::UnexpectedTransportEvent);
		return;
	}
	// Name-resolution and pre-socket connection failures deliberately carry the
	// wildcard sentinel because no live transport identity exists yet.
	if (event.data[0] ==
		ja2::mp::net::SDLNET_CONNECTION_ATTEMPT_FAILED)
	{
		failTransport(
			FullEngineCoopClientTransportFailure::ConnectionAttemptFailed);
		return;
	}
	if (!event.connection || event.connection == ja2::mp::AnyConnection)
	{
		failTransport(
			FullEngineCoopClientTransportFailure::UnexpectedTransportEvent);
		return;
	}

	switch (event.data[0])
	{
		case ja2::mp::net::SDLNET_CONNECTION_ACCEPTED:
			if (connected_ || pendingAccepted_ || server_)
			{
				failTransport(
					FullEngineCoopClientTransportFailure::UnexpectedConnection);
				return;
			}
			pendingAccepted_ = event.connection;
			return;
		case ja2::mp::net::SDLNET_DISCONNECTION_NOTIFICATION:
		case ja2::mp::net::SDLNET_CONNECTION_LOST:
			if ((server_ && event.connection != server_) ||
				(pendingAccepted_ && event.connection != pendingAccepted_))
			{
				failTransport(
					FullEngineCoopClientTransportFailure::UnexpectedConnection);
				return;
			}
			failTransport(
				FullEngineCoopClientTransportFailure::ConnectionLost);
			return;
		case ja2::mp::net::SDLNET_NEW_INCOMING_CONNECTION:
		case ja2::mp::net::SDLNET_NO_FREE_INCOMING_CONNECTIONS:
			failTransport(
				FullEngineCoopClientTransportFailure::UnexpectedConnection);
			return;
		default:
			failTransport(
				FullEngineCoopClientTransportFailure::UnexpectedTransportEvent);
			return;
	}
}

void FullEngineCoopClientTransport::deliverInbound() noexcept
{
	if (!inbound_)
	{
		failTransport(FullEngineCoopClientTransportFailure::TransportFailure);
		return;
	}
	while (!closePending_ && inboundCount_ != 0)
	{
		InboundMessage message = inbound_[inboundHead_];
		inbound_[inboundHead_] = InboundMessage{};
		inboundHead_ = (inboundHead_ + 1) %
			MaximumFullEngineCoopClientInboundMessages;
		--inboundCount_;
		if (inboundCount_ == 0) inboundHead_ = 0;
		if (message.sender != server_)
		{
			failTransport(
				FullEngineCoopClientTransportFailure::UnexpectedConnection);
			break;
		}
		const FullEngineCoopClientResult result = deliver(message);
		if (result == FullEngineCoopClientResult::Success ||
			result == FullEngineCoopClientResult::ResyncRequired ||
			result ==
				FullEngineCoopClientResult::SelfRetirementRejected ||
			((result == FullEngineCoopClientResult::
					CredentialRetirementPending ||
			  result == FullEngineCoopClientResult::CredentialRetired) &&
			 closePending_))
			continue;
		if (lastFailure_ == FullEngineCoopClientTransportFailure::None)
			lastFailure_ =
				FullEngineCoopClientTransportFailure::ClientRejected;
		requestClose(true, 0);
	}
}

FullEngineCoopClientResult FullEngineCoopClientTransport::deliver(
	const InboundMessage& message) noexcept
{
	if (client_ == nullptr) return FullEngineCoopClientResult::InvalidState;
	switch (message.kind)
	{
		case InboundKind::ServerHello:
			return client_->receiveServerHello(
				message.bytes.data(), message.size);
		case InboundKind::AdmissionResponse:
			return client_->receiveAdmissionResponse(
				message.bytes.data(), message.size);
		case InboundKind::TacticalBaseline:
			return client_->receiveBaseline(
				message.bytes.data(), message.size);
		case InboundKind::TacticalDelta:
			return client_->receiveDelta(
				message.bytes.data(), message.size);
		case InboundKind::TacticalReceipt:
			return client_->receiveIntentReceipt(
				message.bytes.data(), message.size);
		case InboundKind::SelfRetirementResult:
			return client_->receiveSelfRetirementResult(
				message.bytes.data(), message.size);
		case InboundKind::CampaignMetadata:
			return campaignSink_ != nullptr &&
				campaignSink_->receiveCampaignMetadata(
					message.bytes.data(), message.size)
				? FullEngineCoopClientResult::Success
				: FullEngineCoopClientResult::InvalidMessage;
		case InboundKind::CampaignChunk:
			return campaignSink_ != nullptr &&
				campaignSink_->receiveCampaignChunk(
					message.bytes.data(), message.size)
				? FullEngineCoopClientResult::Success
				: FullEngineCoopClientResult::InvalidMessage;
		case InboundKind::CampaignComplete:
			return campaignSink_ != nullptr &&
				campaignSink_->receiveCampaignComplete(
					message.bytes.data(), message.size)
				? FullEngineCoopClientResult::Success
				: FullEngineCoopClientResult::InvalidMessage;
		case InboundKind::CampaignReject:
			return campaignSink_ != nullptr &&
				campaignSink_->receiveCampaignReject(
					message.bytes.data(), message.size)
				? FullEngineCoopClientResult::Success
				: FullEngineCoopClientResult::InvalidMessage;
	}
	return FullEngineCoopClientResult::InvalidMessage;
}

void FullEngineCoopClientTransport::clearInbound() noexcept
{
	if (inbound_)
	{
		for (std::size_t index = 0;
			index < MaximumFullEngineCoopClientInboundMessages; ++index)
			inbound_[index] = InboundMessage{};
	}
	inboundHead_ = 0;
	inboundCount_ = 0;
}

void FullEngineCoopClientTransport::failTransport(
	FullEngineCoopClientTransportFailure failure) noexcept
{
	if (lastFailure_ == FullEngineCoopClientTransportFailure::None)
		lastFailure_ = failure;
	requestClose(true, 0);
}

void FullEngineCoopClientTransport::requestClose(bool notifyClient,
	unsigned drainMilliseconds) noexcept
{
	closePending_ = true;
	if (drainMilliseconds > pendingDrainMilliseconds_)
		pendingDrainMilliseconds_ = drainMilliseconds;
	if (!preserveClientStateOnClose_ && notifyClient)
		notifyClientOnClose_ = true;
}

void FullEngineCoopClientTransport::finishClose() noexcept
{
	if (!closePending_ || pollDepth_ != 0 || handlerDepth_ != 0) return;
	FullEngineCoopClient* const client = client_;
	const bool notifyClient = notifyClientOnClose_ &&
		!preserveClientStateOnClose_;
	const unsigned drainMilliseconds = pendingDrainMilliseconds_;

	running_ = false;
	connected_ = false;
	clearInbound();
	server_ = ja2::mp::NoConnection;
	pendingAccepted_ = ja2::mp::NoConnection;
	if (transport_ != nullptr)
	{
		transport_->Shutdown(drainMilliseconds);
		ja2::mp::net::DestroySdlNetPeer(transport_);
		transport_ = nullptr;
	}
	client_ = nullptr;
	campaignSink_ = nullptr;
	maximumQueuedInboundMessages_ = 0;
	maximumPendingWriteBytes_ = 0;
	closePending_ = false;
	notifyClientOnClose_ = false;
	preserveClientStateOnClose_ = false;
	pendingDrainMilliseconds_ = 0;
	if (notifyClient && client != nullptr) client->transportDisconnected();
}

bool FullEngineCoopClientTransport::onMainThread() const noexcept
{
	return std::this_thread::get_id() == mainThread_;
}

bool FullEngineCoopClientTransport::registerMessages() noexcept
{
	if (transport_ == nullptr) return false;
	try
	{
		return transport_->RegisterMessage(CoopServerHelloMessageName,
			&FullEngineCoopClientTransport::HandleServerHello, this) &&
			transport_->RegisterMessage(CoopAdmissionResponseMessageName,
				&FullEngineCoopClientTransport::HandleAdmissionResponse, this) &&
			transport_->RegisterMessage(CoopTacticalBaselineMessageName,
				&FullEngineCoopClientTransport::HandleTacticalBaseline, this) &&
			transport_->RegisterMessage(CoopTacticalDeltaMessageName,
				&FullEngineCoopClientTransport::HandleTacticalDelta, this) &&
			transport_->RegisterMessage(CoopTacticalIntentReceiptMessageName,
				&FullEngineCoopClientTransport::HandleTacticalReceipt, this) &&
			transport_->RegisterMessage(
				CoopAdmissionSelfRetirementResultMessageName,
				&FullEngineCoopClientTransport::HandleSelfRetirementResult,
				this) &&
			transport_->RegisterMessage(CoopCampaignSyncMetadataMessageName,
				&FullEngineCoopClientTransport::HandleCampaignMetadata, this) &&
			transport_->RegisterMessage(CoopCampaignSyncChunkMessageName,
				&FullEngineCoopClientTransport::HandleCampaignChunk, this) &&
			transport_->RegisterMessage(CoopCampaignSyncCompleteMessageName,
				&FullEngineCoopClientTransport::HandleCampaignComplete, this) &&
			transport_->RegisterMessage(CoopCampaignSyncRejectMessageName,
				&FullEngineCoopClientTransport::HandleCampaignReject, this);
	}
	catch (...)
	{
		return false;
	}
}

bool FullEngineCoopClientTransport::validInbound(
	InboundKind kind, std::size_t size) const noexcept
{
	switch (kind)
	{
		case InboundKind::ServerHello:
		case InboundKind::AdmissionResponse:
		case InboundKind::TacticalBaseline:
		case InboundKind::TacticalDelta:
		case InboundKind::TacticalReceipt:
			// Preserve the pre-campaign adapter contract exactly: the tactical
			// core, not the socket callback, rejects malformed short shapes. This
			// per-domain ceiling only prevents the widened storage from admitting
			// frames that the old FIFO could not retain.
			return size <= MaximumCoopTacticalWireSize;
		case InboundKind::SelfRetirementResult:
			return size == AdmissionSelfRetirementResultWireSize;
		case InboundKind::CampaignMetadata:
			return size == CoopCampaignSyncMetadataWireSize;
		case InboundKind::CampaignChunk:
			return size >= CoopCampaignSyncChunkHeaderWireSize &&
				size <= MaximumCoopCampaignSyncWireSize;
		case InboundKind::CampaignComplete:
			return size == CoopCampaignSyncCompleteWireSize;
		case InboundKind::CampaignReject:
			return size == CoopCampaignSyncRejectWireSize;
	}
	return false;
}

bool FullEngineCoopClientTransport::validConfiguration(
	const FullEngineCoopClientTransportConfiguration& configuration)
	const noexcept
{
	return configuration.serverEndpoint.port != 0 &&
		configuration.serverEndpoint.host[0] != '\0' &&
		std::memchr(configuration.serverEndpoint.host, '\0',
			sizeof(configuration.serverEndpoint.host)) != nullptr &&
		configuration.timeoutMilliseconds != 0 &&
		configuration.timeoutMilliseconds <=
			MaximumFullEngineCoopClientTimeoutMilliseconds &&
		configuration.maximumQueuedInboundMessages != 0 &&
		configuration.maximumQueuedInboundMessages <=
			MaximumFullEngineCoopClientInboundMessages &&
		configuration.maximumPendingWriteBytes != 0 &&
		configuration.maximumPendingWriteBytes <=
			MaximumFullEngineCoopClientPendingWriteBytes;
}

bool FullEngineCoopClientTransport::validOutbound(
	const char* messageName, std::size_t size) const noexcept
{
	if (SameName(messageName, CoopAdmissionRequestMessageName))
		return size == AdmissionRequestWireSize;
	if (SameName(messageName,
		CoopAdmissionCredentialAbandonMessageName))
		return size == AdmissionCredentialAbandonWireSize;
	if (SameName(messageName, CoopAdmissionAckMessageName))
		return size == AdmissionAckWireSize;
	if (SameName(messageName,
		CoopAdmissionSelfRetirementRequestMessageName))
		return size == AdmissionSelfRetirementRequestWireSize;
	if (SameName(messageName, CoopTacticalBaselineAckMessageName))
		return size == CoopTacticalBaselineAckWireSize;
	if (SameName(messageName, CoopTacticalDeltaAckMessageName))
		return size == CoopTacticalDeltaAckWireSize;
	if (SameName(messageName, CoopTacticalResyncRequestMessageName))
		return size == CoopTacticalResyncRequestWireSize;
	if (SameName(messageName, CoopTacticalIntentMessageName))
		return size >= TacticalIntentHeaderWireSize &&
			size <= MaximumTacticalIntentWireSize;
	if (SameName(messageName, CoopCampaignSyncAckMessageName))
		return size == CoopCampaignSyncAckWireSize;
	if (SameName(messageName, CoopCampaignSyncResultMessageName))
		return size == CoopCampaignSyncResultWireSize;
	if (SameName(messageName, CoopCampaignSyncResyncMessageName))
		return size == CoopCampaignSyncResyncWireSize;
	return false;
}
}
