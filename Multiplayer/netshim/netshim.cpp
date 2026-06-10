// netshim.cpp -- RakNet 3.401 API-compat layer over SDL3_net TCP stream sockets.
//
// Replaces the prebuilt 32-bit Win32 RakNetLibStatic.lib the JA2 1.13 MP wrapper
// was written against, so Multiplayer/client.cpp + server.cpp run unmodified on
// every 64-bit platform the SDL3 port ships.
//
// Model: everything is polled on the main thread from the wrapper's per-frame
// client_packet()/server_packet() pumps (GameLoop). No threads, no locks.
// Transport is TCP (every wrapper send is HIGH_PRIORITY/RELIABLE, i.e. exactly
// TCP semantics), framed as:
//
//     [u32 LE bodyLen][u8 frameType][body ...]
//
// frame types: FT_RPC      [u8 nameLen][name][payload]
//              FT_BYE      graceful disconnect notification
//              FT_FULL     server refused us (max incoming connections reached)
//              FT_FILE     [u16 setID][u32 fileIndex][u32 setCount][u32 setTotalBytes]
//                          [u16 nameLen][name][u32 fileLen][u32 offset][u32 chunkLen][chunk]
//
// Connection events are synthesized into RakNet ID_* packets; RPC frames are
// dispatched to registered handlers synchronously inside Receive() and never
// returned to the caller (RakNet 3.x behavior the wrapper relies on -- its
// default: branch logs an error for unknown packets).

#include <SDL3_net/SDL_net.h>
#include <SDL3/SDL.h>

#include <deque>
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>

#include "RakPeerInterface.h"
#include "RakNetworkFactory.h"
#include "MessageIdentifiers.h"
#include "FileListTransfer.h"
#include "BitStream.h"
#include "RakSleep.h"
#include "SuperFastHash.h"

namespace
{

enum : unsigned char
{
	FT_RPC  = 1,
	FT_BYE  = 2,
	FT_FULL = 3,
	FT_FILE = 4,
};

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

// Derive a nonzero 32-bit value from a peer's real address for SystemAddress::
// binaryAddress. Real uniqueness per connection comes from the synthetic port.
unsigned int AddressToU32( NET_Address* addr )
{
	if ( addr )
	{
		int n = 0;
		const unsigned char* b = (const unsigned char*)NET_GetAddressBytes( addr, &n );
		if ( b && n >= 4 )
		{
			unsigned int v = GetU32( b );
			if ( n > 4 )   // IPv6: fold remaining bytes in so it stays distinctive
				for ( int i = 4; i < n; ++i ) v = v * 31u + b[i];
			if ( v != 0 && v != 0xFFFFFFFFu )
				return v;
		}
	}
	return 0x7F000001u;   // 127.0.0.1 fallback -- nonzero, not UNASSIGNED
}

struct Conn
{
	NET_StreamSocket* sock = nullptr;
	SystemAddress     addr;
	std::vector<unsigned char> in;
	bool open = true;       // false => swept after the current pump pass
	bool sentBye = false;
};

// Receiver-side state for one incoming file-transfer set.
struct RxFile
{
	FileListTransferCBInterface::OnFileStruct meta;
	std::vector<char> buf;
	unsigned received = 0;
	unsigned partTotal = 1;
	unsigned partCount = 0;
};

} // namespace

struct NetShimFltState
{
	unsigned short nextSetId = 0;
	std::map<unsigned short, FileListTransferCBInterface*> handlers;
	FileListProgress* progress = nullptr;
	std::map<unsigned short, RxFile> rx;   // keyed by setID (one in-flight file per set)
	std::map<unsigned short, unsigned> rxDone; // files completed per set
};

struct NetShimPeerState
{
	bool started = false;
	bool netRef = false;
	unsigned short maxIncoming = 0;
	NET_Server* listener = nullptr;

	// client-side async connect
	NET_Address* resolving = nullptr;
	NET_StreamSocket* connecting = nullptr;
	unsigned short connectPort = 0;
	SystemAddress serverAddr;

	std::vector<Conn*> conns;
	std::deque<Packet*> q;
	std::map<std::string, void ( * )( RPCParameters* )> rpcs;
	std::vector<PluginInterface*> plugins;
	FileListTransfer* flt = nullptr;
	unsigned short nextSyntheticPort = 50000;
	RakPeerInterface* self = nullptr;

	void Synthesize( unsigned char id, const SystemAddress& from )
	{
		Packet* p = new Packet();
		p->systemAddress = from;
		p->length = 1;
		p->bitSize = 8;
		p->data = new unsigned char[1];
		p->data[0] = id;
		q.push_back( p );
	}

	Conn* Find( const SystemAddress& a )
	{
		for ( Conn* c : conns )
			if ( c->open && c->addr == a )
				return c;
		return nullptr;
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
			c->open = false;
			return false;
		}
		return true;
	}

	void CloseConn( Conn* c, bool sendBye, unsigned int drainMs )
	{
		if ( !c->sock )
			return;
		if ( sendBye && c->open && !c->sentBye )
		{
			SendFrame( c, FT_BYE, nullptr, 0 );
			c->sentBye = true;
		}
		NET_WaitUntilStreamSocketDrained( c->sock, (Sint32)drainMs );
		NET_DestroyStreamSocket( c->sock );
		c->sock = nullptr;
		c->open = false;
	}

	void DispatchRPC( Conn* c, const unsigned char* body, unsigned int len );
	void HandleFileFrame( const unsigned char* body, unsigned int len );
	void ParseFrames( Conn* c );
	void PumpSockets();
};

// ---- frame handling --------------------------------------------------------

void NetShimPeerState::DispatchRPC( Conn* c, const unsigned char* body, unsigned int len )
{
	if ( len < 1 )
		return;
	unsigned int nameLen = body[0];
	if ( 1 + nameLen > len )
		return;
	std::string name( (const char*)body + 1, nameLen );
	unsigned int payloadLen = len - 1 - nameLen;
	const unsigned char* payload = body + 1 + nameLen;

	std::map<std::string, void ( * )( RPCParameters* )>::iterator it = rpcs.find( name );
	if ( it == rpcs.end() )
		return;   // unknown RPC name: drop silently (matches RakNet behavior for unregistered IDs)

	// Zero-padded copy: the wrapper atoi()s / wcscpy()s wire data and relies on
	// termination it never sends (e.g. receiveSETID's 1-byte payload).
	std::vector<unsigned char> buf( payloadLen + 4, 0 );
	if ( payloadLen )
		memcpy( buf.data(), payload, payloadLen );

	RPCParameters params;
	params.input = buf.data();
	params.numberOfBitsOfData = payloadLen * 8;
	params.sender = c->addr;
	params.recipient = self;
	params.replyToSender = nullptr;
	it->second( &params );
}

void NetShimPeerState::HandleFileFrame( const unsigned char* body, unsigned int len )
{
	if ( !flt || !flt->state || len < 2 + 4 + 4 + 4 + 2 )
		return;
	NetShimFltState* fs = flt->state;
	unsigned int o = 0;
	unsigned short setID = GetU16( body + o ); o += 2;
	unsigned int fileIndex = GetU32( body + o ); o += 4;
	unsigned int setCount = GetU32( body + o ); o += 4;
	unsigned int setTotal = GetU32( body + o ); o += 4;
	unsigned short nameLen = GetU16( body + o ); o += 2;
	if ( o + nameLen + 12 > len )
		return;
	std::string name( (const char*)body + o, nameLen ); o += nameLen;
	unsigned int fileLen = GetU32( body + o ); o += 4;
	unsigned int offset = GetU32( body + o ); o += 4;
	unsigned int chunkLen = GetU32( body + o ); o += 4;
	if ( o + chunkLen > len )
		return;

	std::map<unsigned short, FileListTransferCBInterface*>::iterator hit = fs->handlers.find( setID );
	if ( hit == fs->handlers.end() )
		return;
	FileListTransferCBInterface* cb = hit->second;

	if ( setCount == 0 )
	{
		// Empty file set: real RakNet still sent the set header, and the receiver
		// completed at once. Without this the joining client waits forever when the
		// host's sync directory is empty.
		fs->rx.erase( setID );
		fs->rxDone.erase( setID );
		cb->OnDownloadComplete();
		return;
	}

	RxFile& rx = fs->rx[setID];
	if ( offset == 0 )
	{
		rx = RxFile();
		rx.meta.fileIndex = fileIndex;
		rx.meta.setID = setID;
		rx.meta.setCount = setCount;
		rx.meta.setTotalFinalLength = setTotal;
		rx.meta.setTotalCompressedTransmissionLength = setTotal;
		rx.meta.finalDataLength = fileLen;
		rx.meta.compressedTransmissionLength = fileLen;
		snprintf( rx.meta.fileName, sizeof( rx.meta.fileName ), "%s", name.c_str() );
		rx.buf.assign( fileLen, 0 );
		rx.partTotal = fileLen ? ( fileLen + chunkLen - 1 ) / ( chunkLen ? chunkLen : 1 ) : 1;
	}
	if ( offset + chunkLen <= rx.buf.size() && chunkLen )
		memcpy( rx.buf.data() + offset, body + o, chunkLen );
	rx.partCount++;
	cb->OnFileProgress( &rx.meta, rx.partCount, rx.partTotal, chunkLen, ( char* )( body + o ) );
	rx.received += chunkLen;

	if ( rx.received >= rx.meta.finalDataLength )
	{
		rx.meta.fileData = rx.buf.empty() ? nullptr : rx.buf.data();
		cb->OnFile( &rx.meta );      // shim retains ownership of fileData
		fs->rx.erase( setID );
		unsigned& done = fs->rxDone[setID];
		++done;
		if ( done >= setCount )
		{
			fs->rxDone.erase( setID );
			cb->OnDownloadComplete();
		}
	}
}

void NetShimPeerState::ParseFrames( Conn* c )
{
	for ( ;; )
	{
		if ( !c->open || c->in.size() < 5 )
			return;
		unsigned int bodyLen = GetU32( c->in.data() );
		if ( bodyLen > 64u * 1024u * 1024u )   // insane frame: kill the connection
		{
			c->open = false;
			Synthesize( ID_CONNECTION_LOST, c->addr );
			return;
		}
		if ( c->in.size() < 5u + bodyLen )
			return;
		unsigned char type = c->in[4];
		std::vector<unsigned char> body( c->in.begin() + 5, c->in.begin() + 5 + bodyLen );
		c->in.erase( c->in.begin(), c->in.begin() + 5 + bodyLen );

		switch ( type )
		{
			case FT_RPC:
				DispatchRPC( c, body.data(), bodyLen );
				break;
			case FT_BYE:
				Synthesize( ID_DISCONNECTION_NOTIFICATION, c->addr );
				CloseConn( c, false, 0 );
				return;
			case FT_FULL:
				Synthesize( ID_NO_FREE_INCOMING_CONNECTIONS, c->addr );
				CloseConn( c, false, 0 );
				return;
			case FT_FILE:
				HandleFileFrame( body.data(), bodyLen );
				break;
			default:
				break;   // unknown frame type from a future version: skip
		}
	}
}

void NetShimPeerState::PumpSockets()
{
	// 1. client-side pending connect
	if ( resolving )
	{
		NET_Status st = NET_GetAddressStatus( resolving );
		if ( st == NET_SUCCESS )
		{
			connecting = NET_CreateClient( resolving, connectPort, 0 );
			serverAddr = SystemAddress( AddressToU32( resolving ), connectPort );
			NET_UnrefAddress( resolving );
			resolving = nullptr;
			if ( !connecting )
				Synthesize( ID_CONNECTION_ATTEMPT_FAILED, UNASSIGNED_SYSTEM_ADDRESS );
		}
		else if ( st == NET_FAILURE )
		{
			NET_UnrefAddress( resolving );
			resolving = nullptr;
			Synthesize( ID_CONNECTION_ATTEMPT_FAILED, UNASSIGNED_SYSTEM_ADDRESS );
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
			conns.push_back( c );
			connecting = nullptr;
			Synthesize( ID_CONNECTION_REQUEST_ACCEPTED, c->addr );
		}
		else if ( st == NET_FAILURE )
		{
			NET_DestroyStreamSocket( connecting );
			connecting = nullptr;
			Synthesize( ID_CONNECTION_ATTEMPT_FAILED, serverAddr );
		}
	}

	// 2. server-side accepts
	if ( listener )
	{
		for ( ;; )
		{
			NET_StreamSocket* s = nullptr;
			if ( !NET_AcceptClient( listener, &s ) || !s )
				break;
			unsigned int live = 0;
			for ( Conn* c : conns ) if ( c->open ) ++live;
			if ( live >= maxIncoming )
			{
				// refuse: tell them why, then drop
				std::vector<unsigned char> f;
				PutU32( f, 0 );
				f.push_back( FT_FULL );
				NET_WriteToStreamSocket( s, f.data(), (int)f.size() );
				NET_WaitUntilStreamSocketDrained( s, 100 );
				NET_DestroyStreamSocket( s );
				continue;
			}
			Conn* c = new Conn();
			c->sock = s;
			c->addr = SystemAddress( AddressToU32( NET_GetStreamSocketAddress( s ) ), nextSyntheticPort++ );
			conns.push_back( c );
			Synthesize( ID_NEW_INCOMING_CONNECTION, c->addr );
		}
	}

	// 3. reads + framing (index loop: handlers may add conns? they don't, but
	//    they may flag closes via CloseConnection)
	for ( size_t i = 0; i < conns.size(); ++i )
	{
		Conn* c = conns[i];
		if ( !c->open || !c->sock )
			continue;
		unsigned char tmp[8192];
		for ( ;; )
		{
			int n = NET_ReadFromStreamSocket( c->sock, tmp, (int)sizeof( tmp ) );
			if ( n > 0 )
			{
				c->in.insert( c->in.end(), tmp, tmp + n );
				if ( n < (int)sizeof( tmp ) )
					break;
			}
			else if ( n == 0 )
				break;
			else
			{
				Synthesize( ID_CONNECTION_LOST, c->addr );
				CloseConn( c, false, 0 );
				break;
			}
		}
		if ( c->open )
			ParseFrames( c );
	}

	// 4. sweep closed connections
	for ( size_t i = 0; i < conns.size(); )
	{
		if ( !conns[i]->open && !conns[i]->sock )
		{
			delete conns[i];
			conns.erase( conns.begin() + i );
		}
		else
			++i;
	}
}

// ---- RakPeerInterface ------------------------------------------------------

RakPeerInterface::RakPeerInterface()
{
	state = new NetShimPeerState();
	state->self = this;
}

RakPeerInterface::~RakPeerInterface()
{
	Shutdown( 0 );
	delete state;
}

bool RakPeerInterface::Startup( unsigned short maxConnections, int /*threadSleepTimer*/, SocketDescriptor* socketDescriptors, unsigned /*socketDescriptorCount*/ )
{
	if ( state->started )
		return true;
	if ( !NetRef() )
		return false;
	state->netRef = true;
	unsigned short port = socketDescriptors ? socketDescriptors->port : 0;
	if ( port != 0 )
	{
		state->listener = NET_CreateServer( nullptr, port, 0 );
		if ( !state->listener )
		{
			NetUnref();
			state->netRef = false;
			return false;
		}
		state->maxIncoming = maxConnections;
	}
	state->started = true;
	return true;
}

bool RakPeerInterface::Connect( const char* host, unsigned short remotePort, const char*, int )
{
	if ( !state->started || state->connecting || state->resolving )
		return false;
	state->resolving = NET_ResolveHostname( host );
	state->connectPort = remotePort;
	if ( !state->resolving )
	{
		state->Synthesize( ID_CONNECTION_ATTEMPT_FAILED, UNASSIGNED_SYSTEM_ADDRESS );
		return false;
	}
	return true;
}

void RakPeerInterface::Shutdown( unsigned int blockDuration, unsigned char )
{
	if ( !state->started )
		return;
	for ( Conn* c : state->conns )
		state->CloseConn( c, true, blockDuration );
	for ( Conn* c : state->conns )
		delete c;
	state->conns.clear();
	if ( state->connecting )
	{
		NET_DestroyStreamSocket( state->connecting );
		state->connecting = nullptr;
	}
	if ( state->resolving )
	{
		NET_UnrefAddress( state->resolving );
		state->resolving = nullptr;
	}
	if ( state->listener )
	{
		NET_DestroyServer( state->listener );
		state->listener = nullptr;
	}
	for ( Packet* p : state->q )
	{
		delete[] p->data;
		delete p;
	}
	state->q.clear();
	state->rpcs.clear();
	state->plugins.clear();
	state->flt = nullptr;
	state->started = false;
	if ( state->netRef )
	{
		NetUnref();
		state->netRef = false;
	}
}

Packet* RakPeerInterface::Receive( void )
{
	if ( !state->started )
		return nullptr;
	state->PumpSockets();
	if ( state->q.empty() )
		return nullptr;
	Packet* p = state->q.front();
	state->q.pop_front();
	return p;
}

void RakPeerInterface::DeallocatePacket( Packet* packet )
{
	if ( !packet )
		return;
	delete[] packet->data;
	delete packet;
}

bool RakPeerInterface::RegisterAsRemoteProcedureCall( const char* uniqueID, void ( *functionPointer )( RPCParameters* ) )
{
	if ( !uniqueID || !functionPointer )
		return false;
	state->rpcs[uniqueID] = functionPointer;
	return true;
}

bool RakPeerInterface::RPC( const char* uniqueID, const char* data, BitSize_t bitLength,
                            PacketPriority, PacketReliability, char,
                            SystemAddress systemAddress, bool broadcast, RakNetTime*,
                            NetworkID, RakNet::BitStream* )
{
	if ( !state->started || !uniqueID )
		return false;
	unsigned int nameLen = (unsigned int)strlen( uniqueID );
	if ( nameLen == 0 || nameLen > 255 )
		return false;
	unsigned int payloadLen = ( bitLength + 7 ) / 8;   // round bits up to bytes

	std::vector<unsigned char> body;
	body.reserve( 1 + nameLen + payloadLen );
	body.push_back( (unsigned char)nameLen );
	body.insert( body.end(), (const unsigned char*)uniqueID, (const unsigned char*)uniqueID + nameLen );
	if ( payloadLen && data )
		body.insert( body.end(), (const unsigned char*)data, (const unsigned char*)data + payloadLen );

	bool sentAny = false;
	for ( Conn* c : state->conns )
	{
		if ( !c->open )
			continue;
		if ( broadcast )
		{
			if ( systemAddress != UNASSIGNED_SYSTEM_ADDRESS && c->addr == systemAddress )
				continue;   // broadcast=true + addr: everyone EXCEPT addr
		}
		else if ( !( c->addr == systemAddress ) )
			continue;       // broadcast=false: only addr
		sentAny |= state->SendFrame( c, FT_RPC, body.data(), (unsigned int)body.size() );
	}
	return sentAny;
}

void RakPeerInterface::SetMaximumIncomingConnections( unsigned short numberAllowed )
{
	state->maxIncoming = numberAllowed;
}

void RakPeerInterface::SetOccasionalPing( bool ) {}                       // TCP keepalive territory; no-op
void RakPeerInterface::SetTimeoutTime( RakNetTime, const SystemAddress ) {} // TCP handles dead peers via read errors

void RakPeerInterface::CloseConnection( const SystemAddress target, bool sendDisconnectionNotification, unsigned char )
{
	Conn* c = state->Find( target );
	if ( c )
		state->CloseConn( c, sendDisconnectionNotification, 100 );
}

void RakPeerInterface::AttachPlugin( PluginInterface* plugin )
{
	if ( !plugin )
		return;
	for ( PluginInterface* p : state->plugins )
		if ( p == plugin )
			return;   // idempotent: the wrapper re-attaches on every connect retry
	state->plugins.push_back( plugin );
	if ( plugin->NetShimPluginId() == 1 )
		state->flt = static_cast<FileListTransfer*>( plugin );
}

void RakPeerInterface::DetachPlugin( PluginInterface* plugin )
{
	for ( size_t i = 0; i < state->plugins.size(); ++i )
	{
		if ( state->plugins[i] == plugin )
		{
			state->plugins.erase( state->plugins.begin() + i );
			break;
		}
	}
	if ( state->flt == plugin )
		state->flt = nullptr;
}

void RakPeerInterface::SetSplitMessageProgressInterval( int ) {}   // progress is per-chunk already

// ---- factory ---------------------------------------------------------------

RakPeerInterface* RakNetworkFactory::GetRakPeerInterface( void )
{
	return new RakPeerInterface();
}

void RakNetworkFactory::DestroyRakPeerInterface( RakPeerInterface* i )
{
	delete i;
}

// ---- FileList / FileListTransfer -------------------------------------------

void FileList::AddFile( const char* filename, const char* data, const unsigned dataLength, const unsigned fileLength, FileListNodeContext, bool )
{
	FileEntry e;
	e.filename = filename ? filename : "";
	e.fileLength = fileLength;
	if ( data && dataLength )
		e.data.assign( data, data + dataLength );
	files.push_back( e );
}

void FileList::Clear( void )
{
	files.clear();
}

FileListTransfer::FileListTransfer()
{
	state = new NetShimFltState();
}

FileListTransfer::~FileListTransfer()
{
	delete state;
}

unsigned short FileListTransfer::SetupReceive( FileListTransferCBInterface* handler, bool, SystemAddress )
{
	unsigned short id = state->nextSetId++;
	state->handlers[id] = handler;
	return id;
}

void FileListTransfer::Send( FileList* fileList, RakPeerInterface* rakPeer, SystemAddress recipient, unsigned short setID,
                             PacketPriority, char, bool, IncrementalReadInterface*, unsigned int chunkSize )
{
	if ( !fileList || !rakPeer || !rakPeer->state )
		return;
	if ( chunkSize == 0 || chunkSize > 256 * 1024 )
		chunkSize = 64 * 1024;
	NetShimPeerState* ps = rakPeer->state;
	Conn* c = ps->Find( recipient );
	if ( !c )
		return;

	unsigned int setCount = (unsigned int)fileList->files.size();
	unsigned int setTotal = 0;
	for ( const FileList::FileEntry& e : fileList->files )
		setTotal += (unsigned int)e.data.size();

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
		const FileList::FileEntry& e = fileList->files[fi];
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
			if ( state->progress )
				state->progress->OnFilePush( e.filename.c_str(), fileLen, offset, chunk, offset >= fileLen, recipient );
		} while ( offset < fileLen );
	}
}

void FileListTransfer::SetCallback( FileListProgress* callback )
{
	state->progress = callback;
}

// ---- misc compat -----------------------------------------------------------

void RakSleep( unsigned int ms )
{
	SDL_Delay( ms );
}

unsigned int SuperFastHash( const char* data, int length )
{
	// FNV-1a; only referenced from commented-out wrapper code, kept linkable.
	unsigned int h = 2166136261u;
	for ( int i = 0; i < length; ++i )
	{
		h ^= (unsigned char)data[i];
		h *= 16777619u;
	}
	return h;
}

namespace RakNet
{
	bool BitStream::DoEndianSwap( void ) { return false; }
	void BitStream::ReverseBytesInPlace( unsigned char* data, unsigned int length )
	{
		for ( unsigned int i = 0; i < length / 2; ++i )
		{
			unsigned char t = data[i];
			data[i] = data[length - 1 - i];
			data[length - 1 - i] = t;
		}
	}
}
