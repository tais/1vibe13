#ifndef MULTIPLAYER_SDL_NET_TRANSPORT_H
#define MULTIPLAYER_SDL_NET_TRANSPORT_H

#include "ConnectionId.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Win32's window-procedure API uses a function-like SendMessage macro. This
// transport owns that identifier as a C++ member and never calls the Win32 API.
#ifdef SendMessage
#undef SendMessage
#endif

namespace ja2::mp::net
{
inline constexpr std::size_t DefaultSdlNetInboundMessageRateBytesPerSecond =
	256u * 1024u;
inline constexpr std::size_t DefaultSdlNetInboundMessageBurstBytes =
	1u * 1024u * 1024u;
inline constexpr std::size_t MaximumSdlNetInboundMessageRateBytesPerSecond =
	32u * 1024u * 1024u;
inline constexpr std::size_t MaximumSdlNetInboundMessageBurstBytes =
	4u * 1024u * 1024u;

struct SdlNetInboundMessageBudget
{
	std::size_t sustainedBytesPerSecond =
		DefaultSdlNetInboundMessageRateBytesPerSecond;
	std::size_t burstBytes = DefaultSdlNetInboundMessageBurstBytes;
};

// Values identify transport-local events while the legacy arena protocol is
// migrated. They are not part of the framed message wire.
enum SdlNetEventCode : std::uint8_t
{
	SDLNET_CONNECTION_ACCEPTED = 1,
	SDLNET_CONNECTION_ATTEMPT_FAILED = 2,
	SDLNET_NEW_INCOMING_CONNECTION = 3,
	SDLNET_NO_FREE_INCOMING_CONNECTIONS = 4,
	SDLNET_DISCONNECTION_NOTIFICATION = 5,
	SDLNET_CONNECTION_LOST = 6
};

struct SdlNetEndpoint
{
	SdlNetEndpoint() = default;
	SdlNetEndpoint(std::uint16_t endpointPort, const char* bindHost) noexcept;

	std::uint16_t port = 0;
	char host[256]{};
};

struct SdlNetEvent
{
	ConnectionId connection;
	std::size_t size = 0;
	std::uint8_t* data = nullptr;
};

class SdlNetPeer;

struct SdlNetMessage
{
	// The buffer is zero-padded past size for the remaining legacy arena
	// decoders. It is borrowed only for the duration of the handler call.
	std::uint8_t* data = nullptr;
	std::size_t size = 0;
	ConnectionId sender;
};

using SdlNetMessageHandler = void (*)(SdlNetMessage* message);
using SdlNetContextMessageHandler = void (*)(
	SdlNetMessage* message, void* context);

struct SdlNetFileInfo
{
	unsigned fileIndex = 0;
	std::uint16_t setID = 0;
	unsigned setCount = 0;
	unsigned setTotalCompressedTransmissionLength = 0;
	unsigned setTotalFinalLength = 0;
	unsigned compressedTransmissionLength = 0;
	unsigned finalDataLength = 0;
	char fileName[512]{};
	char* fileData = nullptr;
};

class SdlNetFileReceiver
{
public:
	virtual ~SdlNetFileReceiver() = default;
	virtual bool OnFile(SdlNetFileInfo* file) = 0;
	virtual void OnFileProgress(SdlNetFileInfo*, unsigned,
		unsigned, unsigned, char*) {}
	virtual bool OnDownloadComplete() { return false; }
};

class SdlNetFileProgress
{
public:
	virtual ~SdlNetFileProgress() = default;
	virtual void OnFilePush(const char*, unsigned,
		unsigned, unsigned, bool, ConnectionId) {}
};

class SdlNetFileList
{
public:
	struct FileEntry
	{
		std::string filename;
		std::vector<char> data;
	};

	void AddFile(const char* filename, const char* data, unsigned dataLength);
	void Clear();

private:
	std::vector<FileEntry> files_;

	friend class SdlNetFileTransfer;
};

struct SdlNetFileTransferState;

class SdlNetFileTransfer
{
public:
	SdlNetFileTransfer();
	~SdlNetFileTransfer();
	SdlNetFileTransfer(const SdlNetFileTransfer&) = delete;
	SdlNetFileTransfer& operator=(const SdlNetFileTransfer&) = delete;

	std::uint16_t SetupReceive(SdlNetFileReceiver* receiver,
		ConnectionId allowedSender);
	void Send(SdlNetFileList& files, SdlNetPeer& peer,
		ConnectionId recipient, std::uint16_t setId,
		unsigned chunkSize = 262144);
	void SetCallback(SdlNetFileProgress* callback);

private:
	SdlNetFileTransferState* state_ = nullptr;

	friend struct SdlNetPeerState;
	friend class SdlNetPeer;
};

struct SdlNetPeerState;

class SdlNetPeer
{
public:
	SdlNetPeer();
	~SdlNetPeer();
	SdlNetPeer(const SdlNetPeer&) = delete;
	SdlNetPeer& operator=(const SdlNetPeer&) = delete;

	bool Start(std::uint16_t maxConnections, const SdlNetEndpoint& endpoint);
	bool Connect(const char* host, std::uint16_t remotePort);
	void Shutdown(unsigned drainMilliseconds);

	SdlNetEvent* Poll();
	void Release(SdlNetEvent* event);

	bool RegisterMessage(const char* name, SdlNetMessageHandler handler);
	// Contextual registrations let an isolated protocol adapter retain its own
	// state without process-global callback trampolines. Registering the same
	// name still replaces the previous handler, exactly like the legacy overload.
	bool RegisterMessage(const char* name,
		SdlNetContextMessageHandler handler, void* context);
	bool SendMessage(const char* name, const void* data, std::size_t size,
		ConnectionId connection, bool broadcast);
	// Non-blocking byte-level backpressure for application protocols. The
	// output is replaced only for a live connection and a successful query.
	bool PendingWriteBytes(
		ConnectionId connection, std::size_t& bytes) noexcept;
	// The strict default protects generic/legacy peers. A bounded protocol with
	// larger legitimate streams may opt in before Start(); live reconfiguration
	// is rejected so every connection begins with one immutable budget.
	bool SetInboundMessageBudget(
		const SdlNetInboundMessageBudget& budget) noexcept;
	// A public listener can keep capacity for an in-process loopback authority.
	// Remote sockets cannot consume this portion of the total connection cap.
	bool SetReservedIncomingLoopbackConnections(
		std::uint16_t count) noexcept;

	void SetMaximumIncomingConnections(std::uint16_t maximum);
	void SetTimeout(unsigned milliseconds);
	void CloseConnection(ConnectionId connection, bool notifyPeer);

	void AttachFileTransfer(SdlNetFileTransfer& transfer);
	void DetachFileTransfer(SdlNetFileTransfer& transfer);

private:
	SdlNetPeerState* state_ = nullptr;

	friend class SdlNetFileTransfer;
};

SdlNetPeer* CreateSdlNetPeer();
void DestroySdlNetPeer(SdlNetPeer* peer);
}

#define REGISTER_SDLNET_MESSAGE(peer, functionName) \
	(peer)->RegisterMessage(#functionName, functionName)

#endif
