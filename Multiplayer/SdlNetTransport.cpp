// Native SDL3_net transport for JA2 multiplayer.
//
// Everything is polled on the game thread. The current private arena wire is a
// reliable TCP stream framed as:
//
//     [u32 LE bodyLen][u8 frameType][body ...]
//
// frame types: FT_MESSAGE  [u8 nameLen][name][payload]
//              FT_BYE      graceful disconnect notification
//              FT_FULL     server refused us (max incoming connections reached)
//              FT_FILE     [u16 setID][u32 fileIndex][u32 setCount][u32 setTotalBytes]
//                          [u16 nameLen][name][u32 fileLen][u32 offset][u32 chunkLen][chunk]
//
// Connection events are queued for Poll(). Named messages are dispatched to
// registered handlers synchronously during Poll() while the legacy arena
// protocol is moved to typed codecs.

#include <SDL3_net/SDL_net.h>
#include <SDL3/SDL.h>

#include <deque>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <cstring>
#include <cstdio>

#include "SdlNetTransport.h"

// The whole protocol is little-endian on the wire (PutU16/PutU32 emit LE, the
// GetU16/GetU32 readers assume LE, and legacy payloads are raw struct bytes sent
// without byte-swap). A big-endian build would
// silently corrupt every frame, so fail the compile instead.
static_assert(SDL_BYTEORDER == SDL_LIL_ENDIAN,
	"SDL multiplayer wire format is little-endian only");

namespace ja2::mp::net
{

namespace
{

enum : unsigned char
{
	FT_MESSAGE  = 1,
	FT_BYE  = 2,
	FT_FULL = 3,
	FT_FILE = 4,
	FT_PING = 5,   // liveness heartbeat: empty-body frame, pure keepalive (no handler)
};

// Liveness: how often we emit a keepalive on an otherwise-idle connection.
// Kept well below a configured timeout so a healthy peer never trips the
// timeout. A frozen/half-open peer stops sending bytes (incl. its own pings) and
// is reaped once it has been silent for timeoutMs.
const unsigned int HEARTBEAT_INTERVAL_MS = 5000;

// Per-type frame ceilings. The old single 64MB limit was a forced-64MB-alloc
// primitive far above any legitimate frame; cap each type at a realistic size so
// a malformed/hostile length header can't make us reserve a huge body buffer.
const unsigned int MAX_MESSAGE_FRAME  = 1u * 1024u * 1024u;   // legacy payloads are well under 1MB
const unsigned int MAX_FILE_FRAME = 1u * 1024u * 1024u;   // file header + one chunk (chunkSize<=256KB)
const unsigned int MAX_TRANSFER_FILE_BYTES = 32u * 1024u * 1024u;
const unsigned int MAX_TRANSFER_SET_BYTES = 512u * 1024u * 1024u;
const unsigned int MAX_TRANSFER_FILES = 4096u;
const unsigned int MAX_ACTIVE_TRANSFER_SETS = 8u;
const size_t MAX_RECEIVE_REGISTRATIONS = 64u;
const unsigned short INVALID_TRANSFER_SET_ID = 0xffffu;
const size_t MAX_BUFFERED_TRANSFER_BYTES = 32u * 1024u * 1024u;
const uint64_t MAX_RESERVED_TRANSFER_BYTES = 512ull * 1024ull * 1024ull;
const unsigned int FILE_FRAME_FIXED_BYTES = 28u;
const unsigned int MAX_TRANSFER_NAME_BYTES = 511u;

// Slow-loris guards: cap how much we drain from one socket per pump pass (so no
// single peer starves the others), and how large a single peer's unparsed input
// buffer may grow before we treat it as hostile and drop the connection.
const size_t READ_CAP_PER_PASS = 256u * 1024u;
const size_t MAX_CONN_IN       = 4u * 1024u * 1024u;

// Per-connection token bucket: cap inbound message dispatch so a flooding peer
// can't amplify through the relay handlers. Tokens are bytes; refilled per ms.
static_assert(DefaultSdlNetInboundMessageRateBytesPerSecond <=
	MaximumSdlNetInboundMessageRateBytesPerSecond);
static_assert(DefaultSdlNetInboundMessageBurstBytes <=
	MaximumSdlNetInboundMessageBurstBytes);
static_assert(MaximumSdlNetInboundMessageBurstBytes <= MAX_CONN_IN);

int g_netInitRefs = 0;

bool NetRef()
{
	if ( g_netInitRefs == 0 && !NET_Init() )
		return false;
	++g_netInitRefs;
	return true;
}

void NetUnref()
{
	if ( g_netInitRefs > 0 && --g_netInitRefs == 0 )
		NET_Quit();
}

void PutU16( std::vector<unsigned char>& v, unsigned short x )
{
	v.push_back( (unsigned char)( x & 0xFF ) );
	v.push_back( (unsigned char)( ( x >> 8 ) & 0xFF ) );
}

void PutU32( std::vector<unsigned char>& v, unsigned int x )
{
	v.push_back( (unsigned char)( x & 0xFF ) );
	v.push_back( (unsigned char)( ( x >> 8 ) & 0xFF ) );
	v.push_back( (unsigned char)( ( x >> 16 ) & 0xFF ) );
	v.push_back( (unsigned char)( ( x >> 24 ) & 0xFF ) );
}

unsigned short GetU16( const unsigned char* p ) { return (unsigned short)( p[0] | ( p[1] << 8 ) ); }
unsigned int   GetU32( const unsigned char* p ) { return (unsigned int)p[0] | ( (unsigned int)p[1] << 8 ) | ( (unsigned int)p[2] << 16 ) | ( (unsigned int)p[3] << 24 ); }

bool IsLoopbackSocket(NET_StreamSocket* socket)
{
	NET_Address* address = socket ? NET_GetStreamSocketAddress(socket) : nullptr;
	if (!address) return false;
	// NET_GetAddressBytes returns protocol-specific sockaddr storage, not a raw
	// 4/16-byte IP address. SDL_net's numeric host string is the portable API.
	const char* const text = NET_GetAddressString(address);
	const bool loopback = text &&
		(std::strcmp(text, "::1") == 0 ||
		 std::strcmp(text, "0:0:0:0:0:0:0:1") == 0 ||
		 std::strncmp(text, "127.", 4) == 0 ||
		 std::strncmp(text, "::ffff:127.", 11) == 0 ||
		 std::strncmp(text, "0:0:0:0:0:ffff:127.", 19) == 0);
	NET_UnrefAddress(address);
	return loopback;
}

struct Conn
{
	NET_StreamSocket* sock = nullptr;
	ConnectionId     addr;
	std::vector<unsigned char> in;
	size_t inOff = 0;       // parse cursor into 'in' (avoids erase()+copy per frame)
	bool open = true;       // false => teardown deferred to the step-4 sweep
	bool sentBye = false;
	bool loopback = false;
	// A server must observe its connection lifecycle event before an eager peer
	// can invoke a named-message or file callback. Outbound client connections
	// retain their existing accepted/message timing.
	bool incomingEventReturned = true;
	Uint64 lastRecvMs = 0;  // SDL_GetTicks() of the last bytes read from this peer
	Uint64 lastPingMs = 0;  // SDL_GetTicks() of the last keepalive we emitted
	unsigned int drainMs = 0;   // how long the sweep may linger this socket draining writes
	// per-conn inbound token bucket (M18): bytes, refilled over time
	double tokens =
		static_cast<double>(DefaultSdlNetInboundMessageBurstBytes);
	Uint64 lastRefillMs = 0;
};

// A socket whose connection has been closed but still has buffered writes to
// flush. We hand it off here and poll NET_GetStreamSocketPendingWrites() each
// pump pass instead of blocking the game loop in NET_WaitUntilStreamSocketDrained.
struct Lingering
{
	NET_StreamSocket* sock = nullptr;
	Uint64 deadlineMs = 0;   // give up and destroy after this
};

// Receiver-side state_ for one incoming file-transfer set.
struct RxFile
{
	SdlNetFileInfo meta;
	std::string name;
	std::vector<char> buf;
	unsigned received = 0;
	unsigned partTotal = 1;
	unsigned partCount = 0;
};

struct RxSet
{
	SdlNetFileReceiver* handler = nullptr;
	ConnectionId allowedSender;
	bool started = false;
	unsigned int setCount = 0;
	unsigned int setTotal = 0;
	unsigned int nextFileIndex = 0;
	uint64_t completedBytes = 0;
	bool hasFile = false;
	RxFile file;
};

} // namespace

struct SdlNetFileTransferState
{
	unsigned short nextSetId = 0;
	std::map<unsigned short, RxSet> receivers;
	SdlNetFileProgress* progress = nullptr;
	unsigned int activeSets = 0;
	size_t bufferedBytes = 0;
	uint64_t reservedBytes = 0;
};

static void DropReceiversForSender( SdlNetFileTransferState* fs, const ConnectionId& sender )
{
	if ( !fs ) return;
	for ( std::map<unsigned short, RxSet>::iterator it = fs->receivers.begin();
	      it != fs->receivers.end(); )
	{
		RxSet& set = it->second;
		if ( set.allowedSender != sender )
		{
			++it;
			continue;
		}
		if ( set.hasFile ) fs->bufferedBytes -= set.file.buf.size();
		if ( set.started )
		{
			--fs->activeSets;
			fs->reservedBytes -= set.setTotal;
		}
		it = fs->receivers.erase( it );
	}
}

static void ClearReceivers( SdlNetFileTransferState* fs )
{
	if ( !fs ) return;
	fs->receivers.clear();
	fs->activeSets = 0;
	fs->bufferedBytes = 0;
	fs->reservedBytes = 0;
}

struct SdlNetPeerState
{
	struct MessageRegistration
	{
		SdlNetMessageHandler handler = nullptr;
		SdlNetContextMessageHandler contextHandler = nullptr;
		void* context = nullptr;
	};

	bool started = false;
	bool netRef = false;
	unsigned short maxIncoming = 0;
	NET_Server* listener = nullptr;

	// client-side async connect
	NET_Address* resolving = nullptr;
	NET_StreamSocket* connecting = nullptr;
	unsigned short connectPort = 0;
	ConnectionId serverAddr;

	std::vector<Conn*> conns;
	std::vector<Lingering> lingering;   // closed sockets draining their write buffer (non-blocking)
	std::deque<SdlNetEvent*> q;
	std::map<std::string, MessageRegistration> handlers;
	SdlNetFileTransfer* flt = nullptr;
	// Opaque process-local identifiers never expose peer IP/port as authority.
	std::uint64_t nextConnectionId = 1;
	bool inShutdown = false;            // guard against reentrant Shutdown()
	unsigned int pumpDepth = 0;         // callbacks may call Poll(); never recurse I/O/parsing
	bool shutdownPending = false;       // callbacks may request teardown; execute after outer pump
	unsigned int pendingShutdownBlockDuration = 0;
	SdlNetFileTransfer* detachPending = nullptr; // cleanup after borrowed frame data expires
	SdlNetPeer* self = nullptr;
	std::size_t inboundMessageRateBytesPerSecond =
		DefaultSdlNetInboundMessageRateBytesPerSecond;
	std::size_t inboundMessageBurstBytes =
		DefaultSdlNetInboundMessageBurstBytes;
	std::uint16_t reservedIncomingLoopbackConnections = 0;
	// Dead-peer detection. 0 == disabled. A peer that has not sent any bytes for
	// timeoutMs is declared lost.
	unsigned int timeoutMs = 0;

	void Synthesize( unsigned char id, const ConnectionId& from )
	{
		SdlNetEvent* p = new SdlNetEvent();
		p->connection = from;
		p->size = 1;
		p->data = new unsigned char[1];
		p->data[0] = id;
		q.push_back( p );
	}

	Conn* Find( const ConnectionId& a )
	{
		for ( Conn* c : conns )
			if ( c->open && c->addr == a )
				return c;
		return nullptr;
	}

	ConnectionId MakeConnectionId()
	{
		for (;;)
		{
			ConnectionId id{nextConnectionId++};
			if (id == NoConnection || id == AnyConnection) continue;
			if (!Find(id)) return id;
		}
	}

	// Non-blocking close: hand the socket to the lingering list to flush its write
	// buffer on later pump passes (or destroy now if nothing is pending), instead
	// of blocking the game loop in NET_WaitUntilStreamSocketDrained.
	void Linger( NET_StreamSocket* sock, unsigned int maxDrainMs )
	{
		if ( !sock )
			return;
		if ( maxDrainMs == 0 || NET_GetStreamSocketPendingWrites( sock ) <= 0 )
		{
			NET_DestroyStreamSocket( sock );
			return;
		}
		Lingering l;
		l.sock = sock;
		l.deadlineMs = SDL_GetTicks() + maxDrainMs;
		lingering.push_back( l );
	}

	void PumpLingering()
	{
		Uint64 now = SDL_GetTicks();
		for ( size_t i = 0; i < lingering.size(); )
		{
			int pending = NET_GetStreamSocketPendingWrites( lingering[i].sock );
			if ( pending <= 0 || now >= lingering[i].deadlineMs )
			{
				NET_DestroyStreamSocket( lingering[i].sock );
				lingering.erase( lingering.begin() + i );
			}
			else
				++i;
		}
	}

	bool SendFrame( Conn* c, unsigned char type, const unsigned char* body, unsigned int bodyLen )
	{
		if ( !c || !c->open || !c->sock )
			return false;
		std::vector<unsigned char> f;
		f.reserve( 5 + bodyLen );
		PutU32( f, bodyLen );
		f.push_back( type );
		if ( bodyLen )
			f.insert( f.end(), body, body + bodyLen );
		if ( !NET_WriteToStreamSocket( c->sock, f.data(), (int)f.size() ) )
		{
			// Write side died: synthesize the same disconnect event the read side
			// does, so HandleDisconnect fires and the player isn't left phantom in
			// ready/maxClients counts. Flag-close only; the sweep tears down.
			if ( c->open )
				Synthesize( SDLNET_CONNECTION_LOST, c->addr );
			c->open = false;
			c->sentBye = true;   // socket is dead; don't try to send FT_BYE
			return false;
		}
		return true;
	}

	// Flag-close only: send FT_BYE if asked, record how long the socket may drain,
	// then mark the conn closed. The actual NET_DestroyStreamSocket happens in the
	// step-4 sweep via the (non-blocking) lingering list. This keeps closes safe to
	// call from inside a message handler mid-iteration (no synchronous teardown / no
	// use-after-free) and never blocks the game loop on a wedged peer.
	void CloseConn( Conn* c, bool sendBye, unsigned int drainMs )
	{
		if ( !c )
			return;
		// Receiver cleanup is idempotent and must run even if SendFrame already
		// flag-closed the connection. During the socket pump, however, callbacks
		// still borrow RxFile storage; the post-parse sweep performs this same drop
		// after they unwind. Outside the pump (including Shutdown) clean immediately.
		if ( pumpDepth == 0 && flt && flt->state_ )
			DropReceiversForSender( flt->state_, c->addr );
		if ( !c->sock || !c->open )
			return;
		if ( sendBye && !c->sentBye )
		{
			SendFrame( c, FT_BYE, nullptr, 0 );   // SendFrame may itself flag-close on failure
			c->sentBye = true;
		}
		c->drainMs = drainMs;
		c->open = false;
	}

	void DispatchMessage( Conn* c, const unsigned char* body, unsigned int len );
	bool HandleFileFrame( Conn* c, const unsigned char* body, unsigned int len );
	void ParseFrames( Conn* c );
	void Liveness();
	void PumpSockets();
};

// ---- frame handling --------------------------------------------------------

void SdlNetPeerState::DispatchMessage( Conn* c, const unsigned char* body, unsigned int len )
{
	if ( len < 1 )
		return;
	unsigned int nameLen = body[0];
	if ( 1 + nameLen > len )
		return;
	std::string name( (const char*)body + 1, nameLen );
	unsigned int payloadLen = len - 1 - nameLen;
	const unsigned char* payload = body + 1 + nameLen;

	// Per-connection inbound token bucket: refill by elapsed time, then charge this
	// frame's wire size, including unknown names. Reliable/TCP state_ cannot be
	// silently discarded without desynchronizing the session, so a peer that
	// exceeds the bounded burst is disconnected rather than partially relayed.
	Uint64 nowMs = SDL_GetTicks();
	if ( c->lastRefillMs == 0 )
		c->lastRefillMs = nowMs;
	c->tokens += static_cast<double>(nowMs - c->lastRefillMs) *
		static_cast<double>(inboundMessageRateBytesPerSecond) / 1000.0;
	if (c->tokens > static_cast<double>(inboundMessageBurstBytes))
		c->tokens = static_cast<double>(inboundMessageBurstBytes);
	c->lastRefillMs = nowMs;
	double cost = (double)( len + 5 );   // frame body + header overhead
	if ( c->tokens < cost )
	{
		Synthesize( SDLNET_CONNECTION_LOST, c->addr );
		CloseConn( c, false, 0 );
		return;
	}
	c->tokens -= cost;

	std::map<std::string, MessageRegistration>::iterator it = handlers.find( name );
	if ( it == handlers.end() )
		return;   // unknown legacy message name: ignore

	// Zero-padded copy: the wrapper atoi()s / wcscpy()s wire data and relies on
	// termination it never sends (e.g. receiveSETID's 1-byte payload).
	std::vector<unsigned char> buf( payloadLen + 4, 0 );
	if ( payloadLen )
		memcpy( buf.data(), payload, payloadLen );

	SdlNetMessage params;
	params.data = buf.data();
	params.size = payloadLen;
	params.sender = c->addr;
	// Copy before invoking: the callback may replace its own registration or
	// request peer shutdown, both of which can invalidate the map entry.
	const MessageRegistration registration = it->second;
	if ( registration.handler )
		registration.handler( &params );
	else if ( registration.contextHandler )
		registration.contextHandler( &params, registration.context );
}

bool SdlNetPeerState::HandleFileFrame( Conn* c, const unsigned char* body, unsigned int len )
{
	if ( !flt || !flt->state_ )
		return true;   // no transfer service/registration: ignore the frame
	SdlNetFileTransferState* fs = flt->state_;
	if ( len < FILE_FRAME_FIXED_BYTES )
		return false;

	unsigned int o = 0;
	unsigned short setID = GetU16( body + o ); o += 2;
	std::map<unsigned short, RxSet>::iterator sit = fs->receivers.find( setID );
	if ( sit == fs->receivers.end() )
		return true;   // completed/unknown set IDs cannot trigger callbacks
	RxSet& set = sit->second;
	if ( !c || ( set.allowedSender != AnyConnection && set.allowedSender != c->addr ) )
		return false;

	// Any malformed frame from the registered sender invalidates the whole set.
	// This both frees bounded live storage and prevents a later frame from
	// continuing with ambiguous state_.
	auto fail = [&]() -> bool
	{
		if ( set.hasFile )
			fs->bufferedBytes -= set.file.buf.size();
		if ( set.started )
		{
			--fs->activeSets;
			fs->reservedBytes -= set.setTotal;
		}
		fs->receivers.erase( sit );
		return false;
	};
	if ( !set.handler )
		return fail();

	unsigned int fileIndex = GetU32( body + o ); o += 4;
	unsigned int setCount = GetU32( body + o ); o += 4;
	unsigned int setTotal = GetU32( body + o ); o += 4;
	unsigned short nameLen = GetU16( body + o ); o += 2;
	if ( nameLen > MAX_TRANSFER_NAME_BYTES || nameLen > len - FILE_FRAME_FIXED_BYTES )
		return fail();
	std::string name( (const char*)body + o, nameLen ); o += nameLen;
	unsigned int fileLen = GetU32( body + o ); o += 4;
	unsigned int offset = GetU32( body + o ); o += 4;
	unsigned int chunkLen = GetU32( body + o ); o += 4;
	if ( chunkLen != len - o )
		return fail();

	if ( setCount == 0 )
	{
		// The empty-set marker is a single exact, all-zero metadata frame.
		if ( set.started || fileIndex != 0 || setTotal != 0 || nameLen != 0 ||
		     fileLen != 0 || offset != 0 || chunkLen != 0 )
			return fail();
		SdlNetFileReceiver* cb = set.handler;
		fs->receivers.erase( sit );   // retire before callback: replay is inert
		cb->OnDownloadComplete();
		return true;
	}

	if ( setCount > MAX_TRANSFER_FILES || fileIndex >= setCount ||
	     setTotal > MAX_TRANSFER_SET_BYTES || fileLen > MAX_TRANSFER_FILE_BYTES ||
	     name.find( '\0' ) != std::string::npos || offset > fileLen ||
	     chunkLen > fileLen - offset || ( fileLen != 0 && chunkLen == 0 ) ||
	     ( fileLen == 0 && ( offset != 0 || chunkLen != 0 ) ) )
		return fail();

	if ( !set.started )
	{
		if ( fileIndex != 0 || offset != 0 || fs->activeSets >= MAX_ACTIVE_TRANSFER_SETS ||
		     fs->reservedBytes > MAX_RESERVED_TRANSFER_BYTES - setTotal )
			return fail();
		set.started = true;
		if ( set.allowedSender == AnyConnection )
			set.allowedSender = c->addr;   // wildcard selects, then pins, one sender per set
		set.setCount = setCount;
		set.setTotal = setTotal;
		++fs->activeSets;
		fs->reservedBytes += setTotal;
	}
	else if ( set.setCount != setCount || set.setTotal != setTotal )
		return fail();

	if ( !set.hasFile )
	{
		if ( fileIndex != set.nextFileIndex || offset != 0 ||
		     set.completedBytes > set.setTotal ||
		     fileLen > set.setTotal - set.completedBytes ||
		     fs->bufferedBytes > MAX_BUFFERED_TRANSFER_BYTES ||
		     fileLen > MAX_BUFFERED_TRANSFER_BYTES - fs->bufferedBytes )
			return fail();

		set.hasFile = true;
		RxFile& rx = set.file;
		rx = RxFile();
		rx.name = name;
		rx.meta.fileIndex = fileIndex;
		rx.meta.setID = setID;
		rx.meta.setCount = setCount;
		rx.meta.setTotalFinalLength = setTotal;
		rx.meta.setTotalCompressedTransmissionLength = setTotal;
		rx.meta.finalDataLength = fileLen;
		rx.meta.compressedTransmissionLength = fileLen;
		memcpy( rx.meta.fileName, name.data(), nameLen );
		rx.meta.fileName[nameLen] = 0;
		rx.buf.assign( fileLen, 0 );
		fs->bufferedBytes += fileLen;
		rx.partTotal = fileLen == 0 ? 1 : ( fileLen - 1 ) / chunkLen + 1;
	}
	else if ( fileIndex != set.file.meta.fileIndex || fileLen != set.file.meta.finalDataLength ||
	          name != set.file.name )
		return fail();

	RxFile& rx = set.file;
	if ( offset != rx.received )   // exact contiguity rejects holes, overlap and duplicates
		return fail();
	if ( fileIndex + 1 == set.setCount &&
	     set.completedBytes + fileLen != set.setTotal )
		return fail();
	if ( chunkLen )
		memcpy( rx.buf.data() + offset, body + o, chunkLen );
	++rx.partCount;
	rx.received += chunkLen;
	rx.meta.fileData = rx.buf.empty() ? nullptr : rx.buf.data();
	SdlNetFileReceiver* cb = set.handler;

	if ( rx.received != fileLen )
	{
		// The callback may re-enter the peer/transfer service. All state mutation for this
		// chunk is complete, and no state_ references are touched after it returns.
		cb->OnFileProgress( &rx.meta, rx.partCount, rx.partTotal, chunkLen,
		                    chunkLen ? ( char* )( body + o ) : nullptr );
		return true;
	}

	// Move completed storage to the stack so fileData stays valid for the exact
	// duration of OnFile even though the receive slot is advanced/retired first.
	RxFile delivered = std::move( set.file );
	delivered.meta.fileData = delivered.buf.empty() ? nullptr : delivered.buf.data();
	fs->bufferedBytes -= delivered.buf.size();
	set.completedBytes += fileLen;
	set.hasFile = false;
	++set.nextFileIndex;
	const bool setComplete = set.nextFileIndex == set.setCount;
	if ( setComplete )
	{
		--fs->activeSets;
		fs->reservedBytes -= set.setTotal;
		fs->receivers.erase( sit );   // replay/reentrancy cannot find this set
	}

	cb->OnFileProgress( &delivered.meta, delivered.partCount, delivered.partTotal, chunkLen,
	                    chunkLen ? ( char* )( body + o ) : nullptr );
	cb->OnFile( &delivered.meta );  // delivered owns fileData through callback return
	if ( setComplete )
		cb->OnDownloadComplete();
	return true;
}

void SdlNetPeerState::ParseFrames( Conn* c )
{
	// Parse via an offset cursor (inOff) rather than erase()ing each consumed frame
	// off the front of the vector (which was O(n) per frame -> O(n^2) under load).
	// The consumed prefix is compacted in one shot at the end.
	for ( ;; )
	{
		size_t avail = c->in.size() - c->inOff;
		if ( !c->open || avail < 5 )
			break;
		const unsigned char* p = c->in.data() + c->inOff;
		unsigned int bodyLen = GetU32( p );
		unsigned char type = p[4];
		bool validHeader = false;
		switch ( type )
		{
			case FT_MESSAGE:
				validHeader = bodyLen >= 2 && bodyLen <= MAX_MESSAGE_FRAME;
				break;
			case FT_FILE:
				validHeader = bodyLen >= FILE_FRAME_FIXED_BYTES && bodyLen <= MAX_FILE_FRAME;
				break;
			case FT_BYE:
			case FT_FULL:
			case FT_PING:
				validHeader = bodyLen == 0;
				break;
			default:
				break;
		}
		if ( !validHeader )
		{
			Synthesize( SDLNET_CONNECTION_LOST, c->addr );
			CloseConn( c, false, 0 );
			break;
		}
		if ( avail < 5u + bodyLen )
			break;   // frame not fully arrived yet
		const unsigned char* body = p + 5;
		c->inOff += 5u + bodyLen;

		switch ( type )
		{
			case FT_MESSAGE:
				DispatchMessage( c, body, bodyLen );
				break;
			case FT_BYE:
				Synthesize( SDLNET_DISCONNECTION_NOTIFICATION, c->addr );
				CloseConn( c, false, 0 );
				break;
			case FT_FULL:
				Synthesize( SDLNET_NO_FREE_INCOMING_CONNECTIONS, c->addr );
				CloseConn( c, false, 0 );
				break;
			case FT_FILE:
				if ( !HandleFileFrame( c, body, bodyLen ) )
				{
					Synthesize( SDLNET_CONNECTION_LOST, c->addr );
					CloseConn( c, false, 0 );
				}
				break;
			case FT_PING:
				break;   // keepalive: arrival already refreshed lastRecvMs; nothing to do
			default:
				break;   // rejected above
		}
		// CloseConn / SendFrame-failure can flag-close mid-parse. A callback can
		// also request deferred shutdown or detach the active transfer service;
		// stop before dispatching another frame to state_ whose cleanup is waiting
		// for the borrowed callback storage to unwind.
		if ( shutdownPending || detachPending || !c->open )
			break;
	}

	// Compact: drop the parsed prefix so 'in' tracks only unparsed bytes.
	if ( c->inOff )
	{
		if ( c->inOff >= c->in.size() )
			c->in.clear();
		else
			c->in.erase( c->in.begin(), c->in.begin() + c->inOff );
		c->inOff = 0;
	}
}

// Heartbeat + dead-peer detection. TCP alone does not promptly time out a
// half-open peer that stops
// reading/writing leaves the socket "connected" until an OS keepalive fires
// minutes later). We emit an FT_PING on each idle link and, when SetTimeout
// has armed a timeout, synthesize SDLNET_CONNECTION_LOST for a peer gone silent.
void SdlNetPeerState::Liveness()
{
	Uint64 now = SDL_GetTicks();
	for ( Conn* c : conns )
	{
		if ( !c->open || !c->sock )
			continue;
		// keepalive so an idle-but-healthy peer keeps refreshing OUR lastRecvMs
		if ( now - c->lastPingMs >= HEARTBEAT_INTERVAL_MS )
		{
			c->lastPingMs = now;
			SendFrame( c, FT_PING, nullptr, 0 );   // marks c closed on write failure
		}
		// A peer silent past the configured timeout is declared lost.
		if ( c->open && timeoutMs != 0 && now - c->lastRecvMs >= (Uint64)timeoutMs )
		{
			Synthesize( SDLNET_CONNECTION_LOST, c->addr );
			CloseConn( c, false, 0 );
		}
	}
}

void SdlNetPeerState::PumpSockets()
{
	// 1. client-side pending connect
	if ( resolving )
	{
		NET_Status st = NET_GetAddressStatus( resolving );
		if ( st == NET_SUCCESS )
		{
			connecting = NET_CreateClient( resolving, connectPort, 0 );
			serverAddr = MakeConnectionId();
			NET_UnrefAddress( resolving );
			resolving = nullptr;
			if ( !connecting )
				Synthesize( SDLNET_CONNECTION_ATTEMPT_FAILED, AnyConnection );
		}
		else if ( st == NET_FAILURE )
		{
			NET_UnrefAddress( resolving );
			resolving = nullptr;
			Synthesize( SDLNET_CONNECTION_ATTEMPT_FAILED, AnyConnection );
		}
	}
	if ( connecting )
	{
		NET_Status st = NET_GetConnectionStatus( connecting );
		if ( st == NET_SUCCESS )
		{
			Conn* c = new Conn();
			c->sock = connecting;
			c->addr = serverAddr;
			c->tokens = static_cast<double>(inboundMessageBurstBytes);
			c->lastRecvMs = SDL_GetTicks();
			conns.push_back( c );
			connecting = nullptr;
			Synthesize( SDLNET_CONNECTION_ACCEPTED, c->addr );
		}
		else if ( st == NET_FAILURE )
		{
			NET_DestroyStreamSocket( connecting );
			connecting = nullptr;
			Synthesize( SDLNET_CONNECTION_ATTEMPT_FAILED, serverAddr );
		}
	}

	// 2. server-side accepts
	if ( listener )
	{
		for ( ;; )
		{
			NET_StreamSocket* s = nullptr;
			if ( !NET_AcceptClient( listener, &s ) )
			{
				// false-return is a listener error, NOT "no pending client" (that's
				// true + s==NULL). Log it so a broken listener isn't silently dead.
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
					"SDL multiplayer: NET_AcceptClient failed: %s", SDL_GetError());
				break;
			}
			if ( !s )
				break;   // no pending client
			const bool loopback = IsLoopbackSocket(s);
			unsigned int live = 0;
			unsigned int liveNonLoopback = 0;
			for (Conn* c : conns)
			{
				if (!c->open) continue;
				++live;
				if (!c->loopback) ++liveNonLoopback;
			}
			const unsigned int nonLoopbackLimit =
				maxIncoming > reservedIncomingLoopbackConnections
					? maxIncoming - reservedIncomingLoopbackConnections
					: 0;
			if (live >= maxIncoming ||
				(!loopback && liveNonLoopback >= nonLoopbackLimit))
			{
				// refuse: tell them why, then drop -- without blocking the game loop.
				std::vector<unsigned char> f;
				PutU32( f, 0 );
				f.push_back( FT_FULL );
				NET_WriteToStreamSocket( s, f.data(), (int)f.size() );
				Linger( s, 100 );   // flush the FT_FULL in the background, then destroy
				continue;
			}
			Conn* c = new Conn();
			c->sock = s;
			c->addr = MakeConnectionId();
			c->loopback = loopback;
			c->incomingEventReturned = false;
			c->tokens = static_cast<double>(inboundMessageBurstBytes);
			c->lastRecvMs = SDL_GetTicks();
			conns.push_back( c );
			Synthesize( SDLNET_NEW_INCOMING_CONNECTION, c->addr );
		}
	}

	// 3. reads + framing (index loop: handlers don't add conns; they may flag
	//    closes via CloseConnection, which is now deferred to the step-4 sweep)
	for ( size_t i = 0; i < conns.size() && !shutdownPending && !detachPending; ++i )
	{
		Conn* c = conns[i];
			if ( !c->open || !c->sock || !c->incomingEventReturned )
				continue;
		unsigned char tmp[8192];
		size_t readThisPass = 0;
		for ( ;; )
		{
			// Cap bytes drained from one socket per pass so a fast/slow-loris peer
			// can't monopolize the pump and starve the others (head-of-line).
			if ( readThisPass >= READ_CAP_PER_PASS )
				break;
			int n = NET_ReadFromStreamSocket( c->sock, tmp, (int)sizeof( tmp ) );
			if ( n > 0 )
			{
				c->lastRecvMs = SDL_GetTicks();   // any traffic = peer alive
				c->in.insert( c->in.end(), tmp, tmp + n );
				readThisPass += (size_t)n;
				if ( n < (int)sizeof( tmp ) )
					break;
			}
			else if ( n == 0 )
				break;
			else
			{
				Synthesize( SDLNET_CONNECTION_LOST, c->addr );
				CloseConn( c, false, 0 );
				break;
			}
		}
		// Cap unparsed-buffer growth: a peer that sends bytes that never form a
		// complete frame (slow loris) is treated as hostile and dropped.
		if ( c->open && c->in.size() - c->inOff > MAX_CONN_IN )
		{
			Synthesize( SDLNET_CONNECTION_LOST, c->addr );
			CloseConn( c, false, 0 );
		}
		if ( c->open )
			ParseFrames( c );
	}
	if ( shutdownPending )
		return;

	// 3.5 heartbeat + timeout (after reads so a peer that just spoke isn't reaped)
	Liveness();

	// 4. sweep closed connections: hand each socket to the non-blocking lingering
	//    list (deferred teardown -- safe even when flag-closed inside a handler),
	//    then delete the Conn.
	for ( size_t i = 0; i < conns.size(); )
	{
		if ( !conns[i]->open )
		{
			if ( flt && flt->state_ )
				DropReceiversForSender( flt->state_, conns[i]->addr );
			Linger( conns[i]->sock, conns[i]->drainMs );
			delete conns[i];
			conns.erase( conns.begin() + i );
		}
		else
			++i;
	}

	// 5. poll sockets still flushing their write buffers after close
	PumpLingering();
}

// ---- public transport ------------------------------------------------------

SdlNetEndpoint::SdlNetEndpoint(
	std::uint16_t endpointPort, const char* bindHost) noexcept
	: port(endpointPort)
{
	if (!bindHost) return;
	std::strncpy(host, bindHost, sizeof(host) - 1);
	host[sizeof(host) - 1] = '\0';
}

SdlNetPeer::SdlNetPeer()
{
	state_ = new SdlNetPeerState();
	state_->self = this;
}

SdlNetPeer::~SdlNetPeer()
{
	Shutdown( 0 );
	delete state_;
}

bool SdlNetPeer::Start(
	std::uint16_t maxConnections, const SdlNetEndpoint& endpoint)
{
	if ( state_->started )
		return true;
	if (endpoint.port != 0 &&
		state_->reservedIncomingLoopbackConnections > maxConnections)
		return false;
	if ( !NetRef() )
		return false;
	state_->netRef = true;
	const std::uint16_t port = endpoint.port;
	if ( port != 0 )
	{
		// Honor an explicit bind address when set, so a
		// caller can restrict the listener to e.g. 127.0.0.1 instead of all interfaces.
		// Empty / "0.0.0.0" / "::" / "*" keep the all-interfaces behavior (nullptr).
		NET_Address* bindAddr = nullptr;
		const char* host = endpoint.host;
		if ( host && host[0] &&
		     strcmp( host, "0.0.0.0" ) != 0 && strcmp( host, "::" ) != 0 && strcmp( host, "*" ) != 0 )
		{
			bindAddr = NET_ResolveHostname( host );
			if ( !bindAddr || NET_WaitUntilResolved( bindAddr, 5000 ) != NET_SUCCESS )
			{
				if ( bindAddr ) NET_UnrefAddress( bindAddr );
				NetUnref();
				state_->netRef = false;
				return false;
			}
		}
		state_->listener = NET_CreateServer( bindAddr, port, 0 );
		if ( bindAddr )
			NET_UnrefAddress( bindAddr );
		if ( !state_->listener )
		{
			NetUnref();
			state_->netRef = false;
			return false;
		}
		state_->maxIncoming = maxConnections;
	}
	state_->started = true;
	return true;
}

bool SdlNetPeer::Connect(const char* host, std::uint16_t remotePort)
{
	if ( !state_->started || state_->connecting || state_->resolving )
		return false;
	state_->resolving = NET_ResolveHostname( host );
	state_->connectPort = remotePort;
	if ( !state_->resolving )
	{
		state_->Synthesize( SDLNET_CONNECTION_ATTEMPT_FAILED, AnyConnection );
		return false;
	}
	return true;
}

void SdlNetPeer::Shutdown(unsigned int blockDuration)
{
	// Reentry guard: a handler calling Shutdown() while a Shutdown is already in
	// flight (or from the destructor after one) must be a no-op, not a double-free.
	if ( !state_->started || state_->inShutdown )
		return;
	// File/message callbacks run synchronously inside the socket pump. Teardown there
	// would delete the Conn currently owned by the outer parser. Defer the whole
	// operation until Poll() has unwound the one active pump.
	if ( state_->pumpDepth != 0 )
	{
		state_->shutdownPending = true;
		if ( blockDuration > state_->pendingShutdownBlockDuration )
			state_->pendingShutdownBlockDuration = blockDuration;
		return;
	}
	state_->inShutdown = true;
	state_->shutdownPending = false;
	state_->pendingShutdownBlockDuration = 0;

	// Flag-close every conn (sends FT_BYE) and hand its socket to the lingering
	// list so writes flush without blocking N x blockDuration on the game loop.
	for ( Conn* c : state_->conns )
	{
		state_->CloseConn( c, true, blockDuration );
		state_->Linger( c->sock, blockDuration );
		delete c;
	}
	state_->conns.clear();
	// Shutdown also retires registrations which never received a first frame and
	// therefore are still bound to the wildcard sender.  This keeps a persistent
	// SdlNetFileTransfer reusable across a peer restart without carrying stale
	// capacity charges or handlers into the next session.
	if ( state_->flt && state_->flt->state_ )
		ClearReceivers( state_->flt->state_ );

	// Single bounded global drain: poll the lingering sockets for up to
	// blockDuration total wall-time (NOT per-socket), then force-destroy the rest.
	if ( blockDuration && !state_->lingering.empty() )
	{
		Uint64 deadline = SDL_GetTicks() + blockDuration;
		while ( !state_->lingering.empty() && SDL_GetTicks() < deadline )
		{
			state_->PumpLingering();
			if ( !state_->lingering.empty() )
				SDL_Delay( 1 );
		}
	}
	for ( Lingering& l : state_->lingering )
		NET_DestroyStreamSocket( l.sock );
	state_->lingering.clear();

	if ( state_->connecting )
	{
		NET_DestroyStreamSocket( state_->connecting );
		state_->connecting = nullptr;
	}
	if ( state_->resolving )
	{
		NET_UnrefAddress( state_->resolving );
		state_->resolving = nullptr;
	}
	if ( state_->listener )
	{
		NET_DestroyServer( state_->listener );
		state_->listener = nullptr;
	}
	for ( SdlNetEvent* p : state_->q )
	{
		delete[] p->data;
		delete p;
	}
	state_->q.clear();
	state_->handlers.clear();
	state_->flt = nullptr;
	state_->detachPending = nullptr;
	state_->started = false;
	state_->inShutdown = false;
	if ( state_->netRef )
	{
		NetUnref();
		state_->netRef = false;
	}
}

SdlNetEvent* SdlNetPeer::Poll()
{
	if ( !state_->started )
		return nullptr;
	if ( state_->pumpDepth == 0 )
	{
		struct PumpScope
		{
			explicit PumpScope( unsigned int& depth ) : depth( depth ) { ++depth; }
			~PumpScope() { --depth; }
			unsigned int& depth;
		} scope( state_->pumpDepth );
		state_->PumpSockets();
	}
	// File callbacks borrow metadata/storage from the active receive set. A
	// callback may detach its own transfer service, but cleanup waits until that callback
	// and the outer parser have returned so those borrowed pointers remain valid.
	if ( state_->pumpDepth == 0 && state_->detachPending )
	{
		SdlNetFileTransfer* transfer = state_->detachPending;
		state_->detachPending = nullptr;
		DetachFileTransfer(*transfer);
	}
	// A nested Poll() deliberately skips I/O and parsing. Only the outermost
	// call owns connection lifetime and services a teardown requested in a
	// callback after every parser reference has unwound.
	if ( state_->pumpDepth == 0 && state_->shutdownPending )
	{
		const unsigned int blockDuration = state_->pendingShutdownBlockDuration;
		state_->shutdownPending = false;
		state_->pendingShutdownBlockDuration = 0;
		Shutdown( blockDuration );
	}
	if ( !state_->started )
		return nullptr;
	if ( state_->q.empty() )
		return nullptr;
	SdlNetEvent* p = state_->q.front();
	state_->q.pop_front();
	if (p->size != 0 && p->data &&
		p->data[0] == SDLNET_NEW_INCOMING_CONNECTION)
	{
		Conn* connection = state_->Find(p->connection);
		if (connection)
			connection->incomingEventReturned = true;
	}
	return p;
}

void SdlNetPeer::Release(SdlNetEvent* event)
{
	if ( !event )
		return;
	delete[] event->data;
	delete event;
}

bool SdlNetPeer::RegisterMessage(
	const char* name, SdlNetMessageHandler handler)
{
	if (!name || !handler)
		return false;
	SdlNetPeerState::MessageRegistration registration;
	registration.handler = handler;
	state_->handlers[name] = registration;
	return true;
}

bool SdlNetPeer::RegisterMessage(const char* name,
	SdlNetContextMessageHandler handler, void* context)
{
	if (!name || !handler)
		return false;
	SdlNetPeerState::MessageRegistration registration;
	registration.contextHandler = handler;
	registration.context = context;
	state_->handlers[name] = registration;
	return true;
}

bool SdlNetPeer::SendMessage(const char* name, const void* data,
	std::size_t size, ConnectionId connection, bool broadcast)
{
	if (!state_->started || !name)
		return false;
	const unsigned int nameLen = static_cast<unsigned int>(std::strlen(name));
	if ( nameLen == 0 || nameLen > 255 )
		return false;
	if ((size != 0 && !data) || size > MAX_MESSAGE_FRAME - 1u - nameLen)
		return false;
	const unsigned int payloadLen = static_cast<unsigned int>(size);

	std::vector<unsigned char> body;
	body.reserve( 1 + nameLen + payloadLen );
	body.push_back( (unsigned char)nameLen );
	body.insert(body.end(), reinterpret_cast<const unsigned char*>(name),
		reinterpret_cast<const unsigned char*>(name) + nameLen);
	if ( payloadLen && data )
		body.insert(body.end(), reinterpret_cast<const unsigned char*>(data),
			reinterpret_cast<const unsigned char*>(data) + payloadLen);

	bool sentAny = false;
	for ( Conn* c : state_->conns )
	{
		if ( !c->open )
			continue;
		if ( broadcast )
		{
			if ( connection != AnyConnection && c->addr == connection )
				continue;   // broadcast=true + addr: everyone EXCEPT addr
		}
		else if (c->addr != connection)
			continue;       // broadcast=false: only addr
		sentAny |= state_->SendFrame( c, FT_MESSAGE, body.data(), (unsigned int)body.size() );
	}
	return sentAny;
}

bool SdlNetPeer::PendingWriteBytes(
	ConnectionId connection, std::size_t& bytes) noexcept
{
	if (!state_ || !state_->started) return false;
	Conn* const peer = state_->Find(connection);
	if (!peer || !peer->sock) return false;
	const int pending = NET_GetStreamSocketPendingWrites(peer->sock);
	if (pending < 0)
	{
		if (peer->open)
			state_->Synthesize(SDLNET_CONNECTION_LOST, peer->addr);
		peer->open = false;
		peer->sentBye = true;
		return false;
	}
	bytes = static_cast<std::size_t>(pending);
	return true;
}

bool SdlNetPeer::SetInboundMessageBudget(
	const SdlNetInboundMessageBudget& budget) noexcept
{
	if (!state_ || state_->started || budget.sustainedBytesPerSecond == 0 ||
		budget.sustainedBytesPerSecond >
			MaximumSdlNetInboundMessageRateBytesPerSecond ||
		budget.burstBytes == 0 ||
		budget.burstBytes > MaximumSdlNetInboundMessageBurstBytes)
		return false;
	state_->inboundMessageRateBytesPerSecond =
		budget.sustainedBytesPerSecond;
	state_->inboundMessageBurstBytes = budget.burstBytes;
	return true;
}

bool SdlNetPeer::SetReservedIncomingLoopbackConnections(
	std::uint16_t count) noexcept
{
	if (!state_ || state_->started) return false;
	state_->reservedIncomingLoopbackConnections = count;
	return true;
}

void SdlNetPeer::SetMaximumIncomingConnections(std::uint16_t numberAllowed)
{
	state_->maxIncoming = numberAllowed;
}

void SdlNetPeer::SetTimeout(unsigned milliseconds)
{
	state_->timeoutMs = milliseconds;
}

void SdlNetPeer::CloseConnection(ConnectionId target, bool notifyPeer)
{
	Conn* c = state_->Find( target );
	if ( c )
		state_->CloseConn(c, notifyPeer, 100);
}

void SdlNetPeer::AttachFileTransfer(SdlNetFileTransfer& transfer)
{
	if (state_->flt == &transfer) return;
	if (state_->flt && state_->flt->state_)
		ClearReceivers(state_->flt->state_);
	state_->flt = &transfer;
}

void SdlNetPeer::DetachFileTransfer(SdlNetFileTransfer& transfer)
{
	if (state_->flt != &transfer) return;
	if (state_->pumpDepth != 0)
	{
		state_->detachPending = &transfer;
		return;
	}
	// Persistent transfer objects are reattached after reconnect. Retire pending
	// and partial receives so handlers and reservation accounting cannot leak.
	if (state_->flt->state_) ClearReceivers(state_->flt->state_);
	state_->flt = nullptr;
}

// ---- factory ---------------------------------------------------------------

SdlNetPeer* CreateSdlNetPeer()
{
	return new SdlNetPeer();
}

void DestroySdlNetPeer(SdlNetPeer* peer)
{
	delete peer;
}

// ---- SdlNetFileList / SdlNetFileTransfer -------------------------------------------

void SdlNetFileList::AddFile(
	const char* filename, const char* data, unsigned dataLength)
{
	FileEntry e;
	e.filename = filename ? filename : "";
	if ( data && dataLength )
		e.data.assign( data, data + dataLength );
	files_.push_back( e );
}

void SdlNetFileList::Clear( void )
{
	files_.clear();
}

SdlNetFileTransfer::SdlNetFileTransfer()
{
	state_ = new SdlNetFileTransferState();
}

SdlNetFileTransfer::~SdlNetFileTransfer()
{
	delete state_;
	state_ = nullptr;   // hardening (L12): a stale flt->state_ access faults cleanly as null
}

std::uint16_t SdlNetFileTransfer::SetupReceive(
	SdlNetFileReceiver* handler, ConnectionId allowedSender)
{
	if ( !handler || state_->receivers.size() >= MAX_RECEIVE_REGISTRATIONS )
		return INVALID_TRANSFER_SET_ID;
	// With at most 64 registrations, inspecting 65 usable consecutive IDs is
	// sufficient to find a free slot even when the 16-bit counter wraps. Never
	// overwrite a live transfer, and reserve 0xffff as the failure sentinel.
	for ( size_t attempts = 0; attempts < MAX_RECEIVE_REGISTRATIONS + 2; ++attempts )
	{
		unsigned short id = state_->nextSetId++;
		if ( id == INVALID_TRANSFER_SET_ID || state_->receivers.find( id ) != state_->receivers.end() )
			continue;
		RxSet receiver;
		receiver.handler = handler;
		receiver.allowedSender = allowedSender;
		state_->receivers.insert( std::make_pair( id, receiver ) );
		return id;
	}
	return INVALID_TRANSFER_SET_ID;
}

void SdlNetFileTransfer::Send(SdlNetFileList& fileList, SdlNetPeer& peer,
	ConnectionId recipient, std::uint16_t setID, unsigned int chunkSize)
{
	if (!peer.state_)
		return;
	if (fileList.files_.size() > MAX_TRANSFER_FILES)
		return;
	uint64_t checkedTotal = 0;
	for (const SdlNetFileList::FileEntry& e : fileList.files_)
	{
		if ( e.filename.size() > MAX_TRANSFER_NAME_BYTES ||
		     e.filename.find( '\0' ) != std::string::npos ||
		     e.data.size() > MAX_TRANSFER_FILE_BYTES ||
		     checkedTotal > MAX_TRANSFER_SET_BYTES - e.data.size() )
			return;
		checkedTotal += e.data.size();
	}
	if ( chunkSize == 0 || chunkSize > 256 * 1024 )
		chunkSize = 64 * 1024;
	SdlNetPeerState* ps = peer.state_;
	Conn* c = ps->Find( recipient );
	if ( !c )
		return;

	unsigned int setCount = (unsigned int)fileList.files_.size();
	unsigned int setTotal = (unsigned int)checkedTotal;

	if ( setCount == 0 )
	{
		std::vector<unsigned char> body;
		PutU16( body, setID );
		PutU32( body, 0 );   // fileIndex
		PutU32( body, 0 );   // setCount == 0 -> empty-set marker
		PutU32( body, 0 );   // setTotal
		PutU16( body, 0 );   // nameLen
		PutU32( body, 0 );   // fileLen
		PutU32( body, 0 );   // offset
		PutU32( body, 0 );   // chunkLen
		ps->SendFrame( c, FT_FILE, body.data(), (unsigned int)body.size() );
		return;
	}

	for ( unsigned int fi = 0; fi < setCount && c->open; ++fi )
	{
		const SdlNetFileList::FileEntry& e = fileList.files_[fi];
		unsigned int fileLen = (unsigned int)e.data.size();
		unsigned int offset = 0;
		do
		{
			unsigned int chunk = fileLen - offset;
			if ( chunk > chunkSize )
				chunk = chunkSize;
			std::vector<unsigned char> body;
			body.reserve( 2 + 4 + 4 + 4 + 2 + e.filename.size() + 12 + chunk );
			PutU16( body, setID );
			PutU32( body, fi );
			PutU32( body, setCount );
			PutU32( body, setTotal );
			PutU16( body, (unsigned short)e.filename.size() );
			body.insert( body.end(), e.filename.begin(), e.filename.end() );
			PutU32( body, fileLen );
			PutU32( body, offset );
			PutU32( body, chunk );
			if ( chunk )
				body.insert( body.end(), (const unsigned char*)e.data.data() + offset, (const unsigned char*)e.data.data() + offset + chunk );
			ps->SendFrame( c, FT_FILE, body.data(), (unsigned int)body.size() );
			offset += chunk;
			if ( state_->progress )
				state_->progress->OnFilePush( e.filename.c_str(), fileLen, offset, chunk, offset >= fileLen, recipient );
		} while ( offset < fileLen );
	}
}

void SdlNetFileTransfer::SetCallback( SdlNetFileProgress* callback )
{
	state_->progress = callback;
}

}
