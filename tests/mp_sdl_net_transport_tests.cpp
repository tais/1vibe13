// Loopback and raw-wire tests for the native SDL3_net multiplayer transport.
// Links only the production transport target and SDL3_net, with no game code.
// Drives real TCP sockets on 127.0.0.1; exercises the exact semantics the JA2
// arena wrapper depends on (see SdlNetTransport.h).

#include <cstdio>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>

#include "SdlNetTransport.h"

using namespace ja2::mp;
using namespace ja2::mp::net;

static int g_failures = 0;
#define CHECK( cond, msg ) do { if ( !( cond ) ) { ++g_failures; printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, msg ); } else { printf( "ok   %s\n", msg ); } } while ( 0 )

static unsigned short g_port = 0;

static bool BytesEqual( const std::vector<unsigned char>& actual, const void* expected, size_t size )
{
	return actual.size() >= size && memcmp( actual.data(), expected, size ) == 0;
}

static bool BytesEqual( const std::vector<char>& actual, const void* expected, size_t size )
{
	return actual.size() >= size && memcmp( actual.data(), expected, size ) == 0;
}

struct PeerLog
{
	explicit PeerLog(SdlNetPeer* transport) : peer(transport) {}

	SdlNetPeer* peer = nullptr;
	std::vector<unsigned char> ids;
	std::vector<ConnectionId> addrs;
	bool Got( unsigned char id ) const
	{
		for ( unsigned char x : ids ) if ( x == id ) return true;
		return false;
	}
};

// Pump all peers, collecting synthesized packets, until pred() or timeout.
template <typename Pred>
static bool PumpUntil( std::vector<PeerLog*> peers, Pred pred, int timeoutMs = 5000 )
{
	Uint64 start = SDL_GetTicks();
	for ( ;; )
	{
		for ( PeerLog* pl : peers )
		{
			for ( SdlNetEvent* pk = pl->peer->Poll(); pk; pk = pl->peer->Poll() )
			{
				pl->ids.push_back( pk->data[0] );
				pl->addrs.push_back( pk->connection );
				pl->peer->Release( pk );
			}
		}
		if ( pred() )
			return true;
		if ( SDL_GetTicks() - start >= (Uint64)timeoutMs )
			return false;
		SDL_Delay( 2 );
	}
}

// ---- named-message capture --------------------------------------------------
struct Captured
{
	int count = 0;
	std::vector<unsigned char> bytes;   // payload + 2 peeked pad bytes
	std::size_t size = 0;
	ConnectionId sender;
};
static Captured g_capSrv, g_capA, g_capB;
static SdlNetPeer* g_relayPeer = nullptr;

struct ContextCapture
{
	int marker = 0;
	Captured captured;
};

static void Capture( Captured& c, SdlNetMessage* p )
{
	c.count++;
	c.size = p->size;
	std::size_t n = p->size;
	c.bytes.assign( p->data, p->data + n + 2 );   // +2: verify zero-padding
	c.sender = p->sender;
}
static void srvPING( SdlNetMessage* p )
{
	Capture( g_capSrv, p );
	if ( g_relayPeer )   // canonical server.cpp relay: everyone EXCEPT sender
		g_relayPeer->SendMessage("clPONG", (const char*)p->data, p->size, p->sender, true);
}
static void clPONG_A( SdlNetMessage* p ) { Capture( g_capA, p ); }
static void clPONG_B( SdlNetMessage* p ) { Capture( g_capB, p ); }
static void ContextHandler( SdlNetMessage* p, void* context )
{
	ContextCapture* capture = static_cast<ContextCapture*>( context );
	if ( capture ) Capture( capture->captured, p );
}

// ---- file transfer capture ---------------------------------------------------
struct FtCap : public SdlNetFileReceiver
{
	int files = 0, progress = 0, complete = 0;
	std::string lastName;
	std::vector<char> lastData;
	bool OnFile( SdlNetFileInfo* s ) override
	{
		files++;
		lastName = s->fileName;
		lastData.clear();
		if ( s->finalDataLength )
			lastData.assign( s->fileData, s->fileData + s->finalDataLength );
		return true;
	}
	void OnFileProgress( SdlNetFileInfo*, unsigned, unsigned, unsigned, char* ) override { progress++; }
	bool OnDownloadComplete( void ) override { complete++; return false; }
};

struct ReentrantFtCap : public SdlNetFileReceiver
{
	SdlNetPeer* peer = nullptr;
	std::vector<std::string> events;
	int nestedReceives = 0;

	void OnFileProgress( SdlNetFileInfo* s, unsigned, unsigned, unsigned, char* ) override
	{
		events.push_back( "progress" + std::to_string( s->fileIndex ) );
		++nestedReceives;
		SdlNetEvent* packet = peer->Poll();
		if ( packet ) peer->Release( packet );
	}
	bool OnFile( SdlNetFileInfo* s ) override
	{
		events.push_back( "file" + std::to_string( s->fileIndex ) );
		return true;
	}
	bool OnDownloadComplete( void ) override
	{
		events.push_back( "complete" );
		return false;
	}
};

struct ShutdownFtCap : public SdlNetFileReceiver
{
	SdlNetPeer* peer = nullptr;
	int progress = 0;
	int files = 0;
	int complete = 0;

	void OnFileProgress( SdlNetFileInfo*, unsigned, unsigned, unsigned, char* ) override
	{
		++progress;
		peer->Shutdown( 0 );
	}
	bool OnFile( SdlNetFileInfo* ) override { ++files; return true; }
	bool OnDownloadComplete( void ) override { ++complete; return false; }
};

struct CloseConnectionFtCap : public SdlNetFileReceiver
{
	SdlNetPeer* peer = nullptr;
	ConnectionId sender;
	int progress = 0;
	bool borrowedDataSurvived = false;

	void OnFileProgress( SdlNetFileInfo* s, unsigned, unsigned, unsigned, char* ) override
	{
		++progress;
		peer->CloseConnection( sender, false );
		// The transport promises these borrowed fields for the entire callback. Reading
		// them after the close request catches eager RxSet/RxFile destruction.
		borrowedDataSurvived = s && strcmp( s->fileName, "close-during-callback" ) == 0 &&
			s->fileData && s->fileData[0] == 'a';
	}
	bool OnFile( SdlNetFileInfo* ) override { return true; }
};

struct DetachTransferFtCap : public SdlNetFileReceiver
{
	SdlNetPeer* peer = nullptr;
	SdlNetFileTransfer* transfer = nullptr;
	int progress = 0;
	bool borrowedDataSurvived = false;

	void OnFileProgress( SdlNetFileInfo* s, unsigned, unsigned, unsigned, char* ) override
	{
		++progress;
		peer->DetachFileTransfer(*transfer);
		borrowedDataSurvived = s && strcmp( s->fileName, "detach-during-callback" ) == 0 &&
			s->fileData && s->fileData[0] == 'b';
	}
	bool OnFile( SdlNetFileInfo* ) override { return true; }
};

static void WireU16( std::vector<unsigned char>& out, unsigned int value )
{
	out.push_back( (unsigned char)( value & 0xff ) );
	out.push_back( (unsigned char)( ( value >> 8 ) & 0xff ) );
}

static void WireU32( std::vector<unsigned char>& out, unsigned int value )
{
	out.push_back( (unsigned char)( value & 0xff ) );
	out.push_back( (unsigned char)( ( value >> 8 ) & 0xff ) );
	out.push_back( (unsigned char)( ( value >> 16 ) & 0xff ) );
	out.push_back( (unsigned char)( ( value >> 24 ) & 0xff ) );
}

static std::vector<unsigned char> WireFrame( unsigned char type, const std::vector<unsigned char>& body )
{
	std::vector<unsigned char> out;
	WireU32( out, (unsigned int)body.size() );
	out.push_back( type );
	out.insert( out.end(), body.begin(), body.end() );
	return out;
}

static std::vector<unsigned char> FileBody( unsigned short setID, unsigned int fileIndex,
	unsigned int setCount, unsigned int setTotal, const std::string& name,
	unsigned int fileLen, unsigned int offset, const std::vector<unsigned char>& chunk,
	unsigned int encodedChunkLen = 0xffffffffu )
{
	std::vector<unsigned char> body;
	WireU16( body, setID );
	WireU32( body, fileIndex );
	WireU32( body, setCount );
	WireU32( body, setTotal );
	WireU16( body, (unsigned int)name.size() );
	body.insert( body.end(), name.begin(), name.end() );
	WireU32( body, fileLen );
	WireU32( body, offset );
	WireU32( body, encodedChunkLen == 0xffffffffu ? (unsigned int)chunk.size() : encodedChunkLen );
	body.insert( body.end(), chunk.begin(), chunk.end() );
	return body;
}

struct RawConn
{
	NET_StreamSocket* sock = nullptr;
	ConnectionId onServer;
};

static bool ConnectRaw( PeerLog& log, RawConn& raw )
{
	NET_Address* address = NET_ResolveHostname( "127.0.0.1" );
	if ( !address || NET_WaitUntilResolved( address, 5000 ) != NET_SUCCESS )
	{
		if ( address ) NET_UnrefAddress( address );
		return false;
	}
	raw.sock = NET_CreateClient( address, g_port, 0 );
	NET_UnrefAddress( address );
	if ( !raw.sock || NET_WaitUntilConnected( raw.sock, 5000 ) != NET_SUCCESS )
	{
		if ( raw.sock ) NET_DestroyStreamSocket( raw.sock );
		raw.sock = nullptr;
		return false;
	}
	size_t before = log.ids.size();
	if ( !PumpUntil( { &log }, [&] {
		for ( size_t i = before; i < log.ids.size(); ++i )
			if ( log.ids[i] == SDLNET_NEW_INCOMING_CONNECTION )
			{
				raw.onServer = log.addrs[i];
				return true;
			}
		return false;
	} ) )
	{
		NET_DestroyStreamSocket( raw.sock );
		raw.sock = nullptr;
		return false;
	}
	return true;
}

static bool SendRaw( RawConn& raw, const std::vector<unsigned char>& bytes )
{
	return raw.sock && NET_WriteToStreamSocket( raw.sock, bytes.data(), (int)bytes.size() );
}

static bool LostSince( const PeerLog& log, size_t before, const ConnectionId& address )
{
	for ( size_t i = before; i < log.ids.size(); ++i )
		if ( log.ids[i] == SDLNET_CONNECTION_LOST && log.addrs[i] == address )
			return true;
	return false;
}

#pragma pack( push, 1 )
struct WirePayload { int a; short b; char name[10]; };
#pragma pack( pop )

int main( int, char** )
{
	SDL_Init( 0 );

	// SDL_GetTicks is the deadline source used by both this transport and
	// SDL3_net. Guard the dependency pin against an Apple regression that paired
	// a nanosecond counter with the Mach raw-counter frequency and made time run
	// roughly 41.67x too fast. A deliberately broad ratio tolerates scheduler
	// noise while still rejecting that unit mismatch by an order of magnitude.
	const auto steadyTickOuterStart = std::chrono::steady_clock::now();
	const Uint64 sdlTickStart = SDL_GetTicks();
	const auto steadyTickInnerStart = std::chrono::steady_clock::now();
	SDL_Delay( 100 );
	const auto steadyTickInnerEnd = std::chrono::steady_clock::now();
	const Uint64 sdlTickEnd = SDL_GetTicks();
	const auto steadyTickOuterEnd = std::chrono::steady_clock::now();
	const auto steadyTickMinimum =
		std::chrono::duration_cast<std::chrono::microseconds>(
			steadyTickInnerEnd - steadyTickInnerStart).count();
	const auto steadyTickMaximum =
		std::chrono::duration_cast<std::chrono::microseconds>(
			steadyTickOuterEnd - steadyTickOuterStart).count();
	const Uint64 sdlTickElapsedMicroseconds = sdlTickEnd >= sdlTickStart
		? (sdlTickEnd - sdlTickStart) * 1000u
		: 0;
	CHECK( steadyTickMinimum >= 50000 &&
	       sdlTickElapsedMicroseconds >=
		       static_cast<Uint64>( steadyTickMinimum / 4 ) &&
	       sdlTickElapsedMicroseconds <=
		       static_cast<Uint64>( steadyTickMaximum * 4 ),
	       "SDL ticks track steady-clock milliseconds on this platform" );

	// ---------- 1. handshake: accept + connect events ----------
	SdlNetPeer* srv = CreateSdlNetPeer();
	// A fixed port makes concurrent CI jobs and an immediately repeated test run
	// contend with one another. Pick a high per-run starting point and probe a
	// small range. Startup() is explicitly retry-safe after a bind failure.
	const Uint64 seed = (Uint64)std::chrono::steady_clock::now().time_since_epoch().count();
	bool serverStarted = false;
	for ( unsigned int attempt = 0; attempt < 128 && !serverStarted; ++attempt )
	{
		g_port = (unsigned short)( 40000 + ( seed + attempt ) % 20000 );
		SdlNetEndpoint sd( g_port, "127.0.0.1" );
		serverStarted = srv->Start( 4, sd );
	}
	CHECK( serverStarted, "server Startup binds listener" );
	if ( !serverStarted )
	{
		DestroySdlNetPeer( srv );
		SDL_Quit();
		return 1;
	}
	srv->SetMaximumIncomingConnections( 2 );
	srv->SetTimeout( 120000 );
	REGISTER_SDLNET_MESSAGE( srv, srvPING );
	g_relayPeer = srv;
	ContextCapture connectionOrder;
	CHECK(srv->RegisterMessage(
		"connection.order", ContextHandler, &connectionOrder),
		"connection-order handler registers" );

	SdlNetPeer* clA = CreateSdlNetPeer();
	SdlNetEndpoint sd0;
	CHECK( clA->Start( 1, sd0 ), "client A startup" );
	clA->RegisterMessage( "clPONG", clPONG_A );
	CHECK( clA->Connect( "127.0.0.1", g_port ), "client A connect initiated" );

	PeerLog L_srv{ srv }, L_A{ clA };
	const bool clientAccepted = PumpUntil( { &L_A }, [&] {
		return L_A.Got( SDLNET_CONNECTION_ACCEPTED );
	} );
	CHECK(clientAccepted, "client observes accepted transport before server poll" );
	const unsigned char eagerPayload = 0x4a;
	CHECK(clientAccepted && clA->SendMessage(
		"connection.order", &eagerPayload, sizeof(eagerPayload),
		AnyConnection, true),
		"eager client can queue its first RPC before server lifecycle handling" );

	SdlNetEvent* firstServerEvent = srv->Poll();
	const bool incomingFirst = firstServerEvent && firstServerEvent->size != 0 &&
		firstServerEvent->data[0] == SDLNET_NEW_INCOMING_CONNECTION;
	CHECK(incomingFirst,
		"server returns incoming-connection event before eager message callback" );
	CHECK(connectionOrder.captured.count == 0,
		"eager first RPC remains gated until connection event is observed" );
	if (firstServerEvent)
	{
		L_srv.ids.push_back(firstServerEvent->data[0]);
		L_srv.addrs.push_back(firstServerEvent->connection);
		srv->Release(firstServerEvent);
	}
	CHECK(PumpUntil({ &L_srv, &L_A }, [&] {
		return connectionOrder.captured.count == 1;
	}), "eager first RPC dispatches after lifecycle handling" );
	const bool handshakeComplete = incomingFirst && clientAccepted;
	CHECK( handshakeComplete, "handshake events on both sides" );
	if ( !handshakeComplete )
	{
		clA->Shutdown( 0 );
		srv->Shutdown( 0 );
		DestroySdlNetPeer( clA );
		DestroySdlNetPeer( srv );
		SDL_Quit();
		return 1;
	}

	// the wrapper's empty-slot sentinel must stay valid: real peers are nonzero/non-UNASSIGNED
	ConnectionId aOnSrv;
	for ( size_t i = 0; i < L_srv.ids.size(); ++i )
		if ( L_srv.ids[i] == SDLNET_NEW_INCOMING_CONNECTION ) aOnSrv = L_srv.addrs[i];
	CHECK(aOnSrv && aOnSrv != AnyConnection,
		"client connection id is nonzero and not the wildcard");
	ConnectionId serverOnA;
	for ( size_t i = 0; i < L_A.ids.size(); ++i )
		if ( L_A.ids[i] == SDLNET_CONNECTION_ACCEPTED ) serverOnA = L_A.addrs[i];
	CHECK(serverOnA && serverOnA != AnyConnection,
		"client A retained its accepted server connection id");

	// ---------- 2. RPC client->server: bytes, bit count, zero-pad, sender ----------
	WirePayload pay; memset( &pay, 0, sizeof( pay ) );
	pay.a = 0x11223344; pay.b = 0x55; strcpy( pay.name, "merc" );
	clA->SendMessage("srvPING", &pay, sizeof( pay ), AnyConnection, true);
	CHECK( PumpUntil( { &L_srv, &L_A }, [&] { return g_capSrv.count >= 1; } ), "RPC reached server handler" );
	CHECK( g_capSrv.size == sizeof( pay ), "size exact" );
	CHECK( BytesEqual( g_capSrv.bytes, &pay, sizeof( pay ) ), "payload byte-exact" );
	CHECK( g_capSrv.bytes.size() >= sizeof( pay ) + 2 &&
	       g_capSrv.bytes[sizeof( pay )] == 0 && g_capSrv.bytes[sizeof( pay ) + 1] == 0,
	       "input zero-padded past payload" );
	CHECK( g_capSrv.sender == aOnSrv, "SdlNetMessage::sender == accept-time address" );
	CHECK( !clA->SendMessage("srvPING", nullptr, 1, AnyConnection, true),
	       "RPC rejects nonempty null payload" );
	CHECK( !clA->SendMessage("srvPING", &pay, 1024u * 1024u, AnyConnection, true),
	       "message rejects body at the full-frame cap" );
	CHECK( !clA->SendMessage("srvPING", &pay, 0xffffffffu, AnyConnection, true),
	       "message rejects oversized byte length" );

	ContextCapture contextual;
	contextual.marker = 0x4a32;
	CHECK( srv->RegisterMessage("coop.test.context", ContextHandler, &contextual),
	       "contextual message handler registers without process-global state" );
	CHECK( clA->SendMessage("coop.test.context", &pay, sizeof( pay ), AnyConnection, true),
	       "contextual test message sends" );
	CHECK( PumpUntil( { &L_srv, &L_A }, [&] {
		return contextual.captured.count == 1;
	} ), "contextual message handler receives its bound instance" );
	CHECK( contextual.marker == 0x4a32 &&
	       contextual.captured.size == sizeof( pay ) &&
	       contextual.captured.sender == aOnSrv &&
	       BytesEqual( contextual.captured.bytes, &pay, sizeof( pay ) ),
	       "contextual registration preserves payload and sender behavior" );

	// relay went back out broadcast-except-sender; A must NOT get its own echo
	SDL_Delay( 100 );
	PumpUntil( { &L_srv, &L_A }, [] { return false; }, 150 );
	CHECK( g_capA.count == 0, "broadcast-except-sender excludes the sender" );

	// ---------- 3. second client: relay reaches the OTHER client ----------
	SdlNetPeer* clB = CreateSdlNetPeer();
	CHECK( clB->Start( 1, sd0 ), "client B startup" );
	clB->RegisterMessage( "clPONG", clPONG_B );
	clB->Connect( "127.0.0.1", g_port );
	PeerLog L_B{ clB };
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return L_B.Got( SDLNET_CONNECTION_ACCEPTED ); } ), "client B connected" );
	ConnectionId serverOnB;
	for ( size_t i = 0; i < L_B.ids.size(); ++i )
		if ( L_B.ids[i] == SDLNET_CONNECTION_ACCEPTED ) serverOnB = L_B.addrs[i];
	CHECK(serverOnB && serverOnB != AnyConnection,
	       "client B retained its accepted server address" );

	g_capSrv = Captured(); g_capA = Captured(); g_capB = Captured();
	clA->SendMessage("srvPING", &pay, sizeof( pay ), AnyConnection, true);
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return g_capB.count >= 1; } ), "relay delivered to client B" );
	CHECK( g_capA.count == 0, "relay still excludes sender A" );
	CHECK( BytesEqual( g_capB.bytes, &pay, sizeof( pay ) ), "relayed payload byte-exact" );

	// ---------- 4. targeted send (broadcast=false) ----------
	g_capA = Captured(); g_capB = Captured();
	srv->SendMessage("clPONG", &pay, sizeof( pay ), aOnSrv, false);
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return g_capA.count >= 1; } ), "targeted RPC reached A" );
	SDL_Delay( 50 );
	PumpUntil( { &L_srv, &L_A, &L_B }, [] { return false; }, 100 );
	CHECK( g_capB.count == 0, "targeted RPC did not reach B" );

	// ---------- 5. unknown RPC name: dropped silently ----------
	clA->SendMessage("noSuchHandler", &pay, sizeof( pay ), AnyConnection, true);
	size_t srvPackets = L_srv.ids.size();
	SDL_Delay( 50 );
	PumpUntil( { &L_srv, &L_A, &L_B }, [] { return false; }, 100 );
	CHECK( L_srv.ids.size() == srvPackets, "unknown RPC produced no user packet" );

	// ---------- 6. server full: third client refused ----------
	SdlNetPeer* clC = CreateSdlNetPeer();
	CHECK( clC->Start( 1, sd0 ), "client C startup" );
	clC->Connect( "127.0.0.1", g_port );
	PeerLog L_C{ clC };
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B, &L_C }, [&] { return L_C.Got( SDLNET_NO_FREE_INCOMING_CONNECTIONS ); } ),
	       "third client got SDLNET_NO_FREE_INCOMING_CONNECTIONS" );
	clC->Shutdown( 0 );
	DestroySdlNetPeer( clC );

	// ---------- 7. big payload (256 KB) framed across many reads ----------
	{
		std::vector<char> big( 256 * 1024 );
		for ( size_t i = 0; i < big.size(); ++i ) big[i] = (char)( i * 31 + 7 );
		g_capSrv = Captured(); g_relayPeer = nullptr;   // no relay for this one
		clA->SendMessage("srvPING", big.data(), big.size(), AnyConnection, true);
		CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return g_capSrv.count >= 1; }, 10000 ), "256KB RPC arrived" );
		CHECK( g_capSrv.size == big.size() && BytesEqual( g_capSrv.bytes, big.data(), big.size() ),
		       "256KB payload byte-exact" );
	}

	// ---------- 8. file transfer: 2 files, progress + bytes + completion ----------
	{
		SdlNetFileTransfer fltS, fltR;
		srv->AttachFileTransfer(fltS );
		srv->AttachFileTransfer(fltS );          // wrapper re-attaches on retry: must be idempotent
		clA->AttachFileTransfer(fltR );

		FtCap cap;
		unsigned short setID = fltR.SetupReceive(&cap, serverOnA );

		std::vector<char> f1( 200 * 1024 );
		for ( size_t i = 0; i < f1.size(); ++i ) f1[i] = (char)( i ^ 0x5A );
		const char* f2 = "tiny file";
		SdlNetFileList fl;
		fl.AddFile("Data/Maps/big.dat", f1.data(), (unsigned)f1.size());
		fl.AddFile("Data/tiny.txt", f2, 9);
		fltS.Send(fl, *srv, aOnSrv, setID, 5000);

		CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return cap.complete >= 1; }, 10000 ), "file set completed" );
		CHECK( cap.files == 2, "both files delivered" );
		CHECK( cap.progress >= (int)( f1.size() / 5000 ), "per-chunk progress callbacks fired" );
		CHECK( cap.lastName == "Data/tiny.txt" && BytesEqual( cap.lastData, f2, 9 ), "file order + bytes exact" );
		// empty file set (host sync dir empty) must still complete -- the
		// joining client otherwise hangs forever on the download screen
		FtCap capEmpty;
		unsigned short setE = fltR.SetupReceive(&capEmpty, serverOnA );
		SdlNetFileList flEmpty;
		fltS.Send(flEmpty, *srv, aOnSrv, setE, 5000);
		CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return capEmpty.complete >= 1; } ), "empty file set completes immediately" );
		CHECK( capEmpty.files == 0, "empty set delivered zero files" );

		// A zero-length file is distinct from an empty set: it still reports one
		// progress part and one OnFile callback before set completion.
		FtCap capZero;
		unsigned short setZ = fltR.SetupReceive(&capZero, serverOnA );
		SdlNetFileList flZero;
		flZero.AddFile("Data/empty.dat", nullptr, 0);
		fltS.Send(flZero, *srv, aOnSrv, setZ, 5000);
		CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return capZero.complete >= 1; } ),
		       "zero-length file set completes" );
		CHECK( capZero.files == 1 && capZero.progress == 1 && capZero.lastData.empty(),
		       "zero-length file delivers exactly one zero-chunk file" );

		srv->DetachFileTransfer(fltS );
		clA->DetachFileTransfer(fltR );
	}
	{
		SdlNetFileTransfer registrations;
		FtCap cap;
		std::vector<unsigned short> ids;
		for ( unsigned int i = 0; i < 64; ++i )
			ids.push_back( registrations.SetupReceive(&cap, AnyConnection ) );
		bool unique = true;
		for ( size_t i = 0; i < ids.size(); ++i )
			for ( size_t j = i + 1; j < ids.size(); ++j )
				if ( ids[i] == ids[j] || ids[i] == 0xffffu ) unique = false;
		CHECK( unique, "bounded receive registrations use unique live set IDs" );
		CHECK( registrations.SetupReceive(&cap, AnyConnection ) == 0xffffu,
		       "65th pending receive registration is rejected" );
	}
	{
		// Peer shutdown clears the file-transfer attachment, while the game wrapper
		// persists and reattaches the SdlNetFileTransfer object on its next session.
		SdlNetPeer* lifecyclePeer = CreateSdlNetPeer();
		SdlNetEndpoint lifecycleSocket;
		SdlNetFileTransfer registrations;
		FtCap cap;
		CHECK( lifecyclePeer->Start( 1, lifecycleSocket ),
		       "receiver lifecycle peer starts" );
		lifecyclePeer->AttachFileTransfer(registrations );
		for ( unsigned int i = 0; i < 64; ++i )
			registrations.SetupReceive(&cap, AnyConnection );
		lifecyclePeer->Shutdown( 0 );
		CHECK( lifecyclePeer->Start( 1, lifecycleSocket ),
		       "receiver lifecycle peer restarts" );
		lifecyclePeer->AttachFileTransfer(registrations );
		bool reset = true;
		for ( unsigned int i = 0; i < 64; ++i )
			if ( registrations.SetupReceive(&cap, AnyConnection ) == 0xffffu )
				reset = false;
		CHECK( reset && registrations.SetupReceive(&cap, AnyConnection ) == 0xffffu,
		       "Shutdown retires all pending receiver registrations before reattach" );
		lifecyclePeer->DetachFileTransfer(registrations );
		lifecyclePeer->Shutdown( 0 );
		DestroySdlNetPeer( lifecyclePeer );
	}

	// ---------- 9. graceful disconnect ----------
	// The real peers run in separate processes and pump concurrently. In this
	// single-threaded loopback test, a blocking Shutdown(B) prevents the server
	// from reading B's BYE until after B has torn down its socket, which can turn
	// a graceful notification into an OS-dependent connection-lost event. Start
	// the same graceful close non-blockingly and pump the receiver first.
	size_t before = L_srv.ids.size();
	clB->CloseConnection( serverOnB, true );
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] {
		for ( size_t i = before; i < L_srv.ids.size(); ++i )
			if ( L_srv.ids[i] == SDLNET_DISCONNECTION_NOTIFICATION ) return true;
		return false;
	} ), "server saw SDLNET_DISCONNECTION_NOTIFICATION for B" );
	clB->Shutdown( 0 );
	DestroySdlNetPeer( clB );

	// ---------- 10. a second peer cannot inject into A's receive set ----------
	SdlNetPeer* clD = CreateSdlNetPeer();
	CHECK( clD->Start( 1, sd0 ), "client D startup" );
	CHECK( clD->Connect( "127.0.0.1", g_port ), "client D connect initiated" );
	PeerLog L_D{ clD };
	size_t beforeDOnSrv = L_srv.ids.size();
	CHECK( PumpUntil( { &L_srv, &L_A, &L_D }, [&] {
		if (!L_D.Got(SDLNET_CONNECTION_ACCEPTED)) return false;
		for (size_t i = beforeDOnSrv; i < L_srv.ids.size(); ++i)
			if (L_srv.ids[i] == SDLNET_NEW_INCOMING_CONNECTION) return true;
		return false;
	} ),
	       "client D connected for sender-binding test" );
	ConnectionId serverOnD;
	for ( size_t i = 0; i < L_D.ids.size(); ++i )
		if ( L_D.ids[i] == SDLNET_CONNECTION_ACCEPTED ) serverOnD = L_D.addrs[i];
	ConnectionId dOnSrv;
	for ( size_t i = beforeDOnSrv; i < L_srv.ids.size(); ++i )
		if ( L_srv.ids[i] == SDLNET_NEW_INCOMING_CONNECTION ) dOnSrv = L_srv.addrs[i];
	{
		SdlNetFileTransfer receiveA, sendD;
		srv->AttachFileTransfer(receiveA );
		clD->AttachFileTransfer(sendD );
		FtCap cap;
		unsigned short setID = receiveA.SetupReceive(&cap, aOnSrv );
		const char rogueByte = 'x';
		SdlNetFileList rogue;
		rogue.AddFile("rogue.dat", &rogueByte, 1);
		size_t beforeLoss = L_srv.ids.size();
		sendD.Send(rogue, *clD, serverOnD, setID, 1024);
		CHECK( PumpUntil( { &L_srv, &L_A, &L_D }, [&] { return LostSince( L_srv, beforeLoss, dOnSrv ); } ),
		       "wrong sender file frame disconnects injecting peer" );
		CHECK( cap.files == 0 && cap.progress == 0 && cap.complete == 0,
		       "wrong sender cannot advance or complete A's transfer" );
		clD->DetachFileTransfer(sendD );
		srv->DetachFileTransfer(receiveA );
	}
	clD->Shutdown( 0 );
	DestroySdlNetPeer( clD );

	// ---------- 11. CloseConnection kick: client sees the drop ----------
	size_t beforeA = L_A.ids.size();
	srv->CloseConnection( aOnSrv, true );
	CHECK( PumpUntil( { &L_A }, [&] {
		for ( size_t i = beforeA; i < L_A.ids.size(); ++i )
			if ( L_A.ids[i] == SDLNET_DISCONNECTION_NOTIFICATION || L_A.ids[i] == SDLNET_CONNECTION_LOST ) return true;
		return false;
	} ), "kicked client A notified" );

	clA->Shutdown( 0 );
	DestroySdlNetPeer( clA );

	// ---------- 12. adversarial raw-wire file/control frames ----------
	// These bypass SdlNetFileTransfer::Send so malformed lengths and state changes
	// reach the receiver exactly as a hostile TCP peer could encode them.
	auto RunBadFile = [&]( const char* label, auto buildBodies, int expectedFiles )
	{
		RawConn raw;
		bool connected = ConnectRaw( L_srv, raw );
		CHECK( connected, "raw adversarial client connected" );
		if ( !connected ) return;
		SdlNetFileTransfer receiver;
		srv->AttachFileTransfer(receiver );
		FtCap cap;
		unsigned short setID = receiver.SetupReceive(&cap, raw.onServer );
		std::vector<std::vector<unsigned char> > bodies = buildBodies( setID );
		size_t beforeLoss = L_srv.ids.size();
		bool wrote = true;
		for ( const std::vector<unsigned char>& body : bodies )
			wrote = SendRaw( raw, WireFrame( 4, body ) ) && wrote;
		CHECK( wrote, "raw adversarial frames queued" );
		CHECK( PumpUntil( { &L_srv }, [&] { return LostSince( L_srv, beforeLoss, raw.onServer ); } ), label );
		CHECK( cap.files == expectedFiles && cap.complete == 0,
		       "rejected file sequence cannot complete or duplicate a file callback" );
		srv->DetachFileTransfer(receiver );
		NET_DestroyStreamSocket( raw.sock );
	};

	RunBadFile( "oversized file declaration is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 32u * 1024u * 1024u + 1u, "huge", 32u * 1024u * 1024u + 1u,
			          0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "wrapped/truncated chunk length is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 1, "chunk", 1, 0, std::vector<unsigned char>{ 'x' }, 0xfffffffeu ) };
	}, 0 );
	RunBadFile( "zero-progress chunk for nonempty file is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 1, "stalled", 1, 0, std::vector<unsigned char>() ) };
	}, 0 );
	RunBadFile( "fileIndex outside setCount is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 1, 1, 1, "index", 1, 0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "setCount above the file cap is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 4097, 1, "count-cap", 1, 0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "setTotal above the byte cap is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 2, 512u * 1024u * 1024u + 1u, "total-cap", 1, 0,
			          std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "non-sequential first fileIndex is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 1, 2, 2, "index", 1, 0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "oversized non-terminating file name is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 1, std::string( 512, 'n' ), 1, 0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "truncated claimed file name is rejected", []( unsigned short id ) {
		std::vector<unsigned char> body = FileBody( id, 0, 1, 0, "", 0, 0,
		                                                 std::vector<unsigned char>() );
		body[14] = 0xff; body[15] = 0x01;   // claim 511 bytes, provide none
		return std::vector<std::vector<unsigned char> >{ body };
	}, 0 );
	RunBadFile( "embedded-NUL file name alias is rejected", []( unsigned short id ) {
		std::string name( "ab", 2 ); name.push_back( '\0' ); name += "cd";
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 1, name, 1, 0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "declared set total must match completed file lengths", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 2, "total", 1, 0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "overlapping/duplicate chunk offset is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 3, "overlap", 3, 0, std::vector<unsigned char>{ 'a' } ),
			FileBody( id, 0, 1, 3, "overlap", 3, 0, std::vector<unsigned char>{ 'a' } ) };
	}, 0 );
	RunBadFile( "chunk offset hole is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 3, "hole", 3, 0, std::vector<unsigned char>{ 'a' } ),
			FileBody( id, 0, 1, 3, "hole", 3, 2, std::vector<unsigned char>{ 'c' } ) };
	}, 0 );
	RunBadFile( "active-file name mutation is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 2, "before", 2, 0, std::vector<unsigned char>{ 'a' } ),
			FileBody( id, 0, 1, 2, "after", 2, 1, std::vector<unsigned char>{ 'b' } ) };
	}, 0 );
	RunBadFile( "setCount mutation is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 2, 4, "count", 2, 0, std::vector<unsigned char>{ 'a' } ),
			FileBody( id, 0, 3, 4, "count", 2, 1, std::vector<unsigned char>{ 'b' } ) };
	}, 0 );
	RunBadFile( "setTotal mutation is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 2, 4, "total", 2, 0, std::vector<unsigned char>{ 'a' } ),
			FileBody( id, 0, 2, 5, "total", 2, 1, std::vector<unsigned char>{ 'b' } ) };
	}, 0 );
	RunBadFile( "completed fileIndex cannot be replayed", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 2, 2, "first", 1, 0, std::vector<unsigned char>{ 'a' } ),
			FileBody( id, 0, 2, 2, "first", 1, 0, std::vector<unsigned char>{ 'a' } ) };
	}, 1 );

	// Valid zero-length files remain legal, but completion retires the set before
	// callbacks so a wire replay cannot deliver it twice.
	{
		RawConn raw;
		CHECK( ConnectRaw( L_srv, raw ), "raw zero-file client connected" );
		if ( raw.sock )
		{
			SdlNetFileTransfer receiver;
			srv->AttachFileTransfer(receiver );
			FtCap cap;
			unsigned short setID = receiver.SetupReceive(&cap, raw.onServer );
			std::vector<unsigned char> frame = WireFrame( 4,
				FileBody( setID, 0, 1, 0, "zero", 0, 0, std::vector<unsigned char>() ) );
			CHECK( SendRaw( raw, frame ), "raw zero-file frame queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return cap.complete == 1; } ),
			       "raw zero-length file completes" );
			CHECK( cap.files == 1 && cap.progress == 1, "raw zero-length file callback semantics preserved" );
			size_t beforeReplay = L_srv.ids.size();
			CHECK( SendRaw( raw, frame ), "raw completed-set replay queued" );
			PumpUntil( { &L_srv }, [] { return false; }, 100 );
			CHECK( cap.files == 1 && cap.complete == 1 && !LostSince( L_srv, beforeReplay, raw.onServer ),
			       "completed set replay is inert" );
			srv->DetachFileTransfer(receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// Retiring each completed registration lets the 16-bit allocator wrap without
	// ever overwriting a live entry; 0xffff remains the explicit failure sentinel.
	{
		RawConn raw;
		CHECK( ConnectRaw( L_srv, raw ), "raw set-ID-wrap client connected" );
		if ( raw.sock )
		{
			SdlNetFileTransfer receiver;
			srv->AttachFileTransfer(receiver );
			FtCap cap;
			unsigned int allocated = 0;
			bool idsValid = true;
			while ( allocated < 65535u )
			{
				unsigned int batch = 65535u - allocated;
				if ( batch > 64u ) batch = 64u;
				std::vector<unsigned char> stream;
				for ( unsigned int i = 0; i < batch; ++i )
				{
					unsigned short id = receiver.SetupReceive(&cap, raw.onServer );
					if ( id == 0xffffu ) idsValid = false;
					std::vector<unsigned char> frame = WireFrame( 4,
						FileBody( id, 0, 0, 0, "", 0, 0, std::vector<unsigned char>() ) );
					stream.insert( stream.end(), frame.begin(), frame.end() );
				}
				allocated += batch;
				if ( !SendRaw( raw, stream ) ||
				     !PumpUntil( { &L_srv }, [&] { return cap.complete == (int)allocated; } ) )
				{
					idsValid = false;
					break;
				}
			}
			CHECK( idsValid, "set IDs remain usable through the full non-sentinel range" );
			CHECK( receiver.SetupReceive(&cap, raw.onServer ) == 0,
			       "set-ID allocator skips 0xffff and wraps to a free zero" );
			srv->DetachFileTransfer(receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// Callbacks are synchronous and legacy handlers may poll Receive() themselves.
	// Nested Receive must never recurse the socket pump: doing so can deliver the
	// second file before the first OnFile and can sweep the outer parser's Conn.
	{
		RawConn raw;
		CHECK( ConnectRaw( L_srv, raw ), "raw callback-reentry client connected" );
		if ( raw.sock )
		{
			SdlNetFileTransfer receiver;
			srv->AttachFileTransfer(receiver );
			ReentrantFtCap cap;
			cap.peer = srv;
			unsigned short setID = receiver.SetupReceive(&cap, raw.onServer );
			std::vector<unsigned char> stream = WireFrame( 4,
				FileBody( setID, 0, 2, 2, "first", 1, 0,
				          std::vector<unsigned char>{ 'a' } ) );
			std::vector<unsigned char> second = WireFrame( 4,
				FileBody( setID, 1, 2, 2, "second", 1, 0,
				          std::vector<unsigned char>{ 'b' } ) );
			stream.insert( stream.end(), second.begin(), second.end() );
			std::vector<unsigned char> bye = WireFrame( 2, std::vector<unsigned char>() );
			stream.insert( stream.end(), bye.begin(), bye.end() );
			CHECK( SendRaw( raw, stream ), "reentrant callback stream queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return cap.events.size() == 5; } ),
			       "reentrant Receive callback stream completes" );
			const std::vector<std::string> expected{
				"progress0", "file0", "progress1", "file1", "complete" };
			CHECK( cap.events == expected && cap.nestedReceives == 2,
			       "nested Receive preserves file callback order and one outer pump" );
			srv->DetachFileTransfer(receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// CloseConnection requested from a partial-file callback is flag-only until
	// the outer parser unwinds. The callback's borrowed metadata and file buffer
	// must remain valid for the complete callback invocation.
	{
		RawConn raw;
		CHECK( ConnectRaw( L_srv, raw ), "raw callback-close client connected" );
		if ( raw.sock )
		{
			SdlNetFileTransfer receiver;
			srv->AttachFileTransfer(receiver );
			CloseConnectionFtCap cap;
			cap.peer = srv;
			cap.sender = raw.onServer;
			unsigned short setID = receiver.SetupReceive(&cap, raw.onServer );
			std::vector<unsigned char> frame = WireFrame( 4,
				FileBody( setID, 0, 1, 2, "close-during-callback", 2, 0,
				          std::vector<unsigned char>{ 'a' } ) );
			CHECK( SendRaw( raw, frame ), "callback-close partial frame queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return cap.progress == 1; } ),
			       "CloseConnection callback returns through the outer parser" );
			CHECK( cap.borrowedDataSurvived,
			       "CloseConnection preserves borrowed file data through callback return" );
			srv->DetachFileTransfer(receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// Detaching the persistent SdlNetFileTransfer from its own callback follows the
	// same borrowed-storage rule, then retires the partial registration before a
	// later reattach.
	{
		RawConn raw;
		CHECK( ConnectRaw( L_srv, raw ), "raw callback-detach client connected" );
		if ( raw.sock )
		{
			SdlNetFileTransfer receiver;
			srv->AttachFileTransfer(receiver );
			DetachTransferFtCap cap;
			cap.peer = srv;
			cap.transfer = &receiver;
			unsigned short setID = receiver.SetupReceive(&cap, raw.onServer );
			std::vector<unsigned char> stream = WireFrame( 4,
				FileBody( setID, 0, 1, 2, "detach-during-callback", 2, 0,
				          std::vector<unsigned char>{ 'b' } ) );
			std::vector<unsigned char> finalFrame = WireFrame( 4,
				FileBody( setID, 0, 1, 2, "detach-during-callback", 2, 1,
				          std::vector<unsigned char>{ 'c' } ) );
			stream.insert( stream.end(), finalFrame.begin(), finalFrame.end() );
			CHECK( SendRaw( raw, stream ), "callback-detach buffered frames queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return cap.progress == 1; } ),
			       "file-transfer detach callback returns through the outer parser" );
			CHECK( cap.borrowedDataSurvived,
			       "file-transfer detach preserves borrowed data through callback return" );
			PumpUntil( { &L_srv }, [] { return false; }, 100 );
			CHECK( cap.progress == 1,
			       "deferred detach stops buffered file callbacks after the detach request" );
			srv->AttachFileTransfer(receiver );
			CHECK( receiver.SetupReceive(&cap, raw.onServer ) != 0xffffu,
			       "deferred detach retires the partial registration before reattach" );
			srv->DetachFileTransfer(receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// Nine simultaneous partial sets exceed the per-plugin active-set bound.
	{
		RawConn raw;
		CHECK( ConnectRaw( L_srv, raw ), "raw active-set-cap client connected" );
		if ( raw.sock )
		{
			SdlNetFileTransfer receiver;
			srv->AttachFileTransfer(receiver );
			FtCap caps[9];
			std::vector<unsigned char> stream;
			for ( unsigned int i = 0; i < 9; ++i )
			{
				unsigned short id = receiver.SetupReceive(&caps[i], raw.onServer );
				std::vector<unsigned char> frame = WireFrame( 4,
					FileBody( id, 0, 1, 2, "active", 2, 0, std::vector<unsigned char>{ 'a' } ) );
				stream.insert( stream.end(), frame.begin(), frame.end() );
			}
			size_t beforeLoss = L_srv.ids.size();
			CHECK( SendRaw( raw, stream ), "nine partial sets queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return LostSince( L_srv, beforeLoss, raw.onServer ); } ),
			       "ninth active transfer set is rejected" );
			bool noneComplete = true;
			for ( const FtCap& cap : caps ) if ( cap.files || cap.complete ) noneComplete = false;
			CHECK( noneComplete, "active-set cap cannot synthesize completion" );
			srv->DetachFileTransfer(receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// Declared totals are reserved separately from the much smaller live file
	// buffers, preventing many cheap partial sets from claiming unbounded state.
	{
		RawConn raw;
		CHECK( ConnectRaw( L_srv, raw ), "raw aggregate-reservation client connected" );
		if ( raw.sock )
		{
			SdlNetFileTransfer receiver;
			srv->AttachFileTransfer(receiver );
			FtCap caps[2];
			unsigned short a = receiver.SetupReceive(&caps[0], raw.onServer );
			unsigned short b = receiver.SetupReceive(&caps[1], raw.onServer );
			std::vector<unsigned char> stream = WireFrame( 4,
				FileBody( a, 0, 2, 300u * 1024u * 1024u, "reserve-a", 2, 0,
				          std::vector<unsigned char>{ 'a' } ) );
			std::vector<unsigned char> second = WireFrame( 4,
				FileBody( b, 0, 2, 300u * 1024u * 1024u, "reserve-b", 2, 0,
				          std::vector<unsigned char>{ 'b' } ) );
			stream.insert( stream.end(), second.begin(), second.end() );
			size_t beforeLoss = L_srv.ids.size();
			CHECK( SendRaw( raw, stream ), "aggregate reservation frames queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return LostSince( L_srv, beforeLoss, raw.onServer ); } ),
			       "aggregate declared-set cap is enforced" );
			srv->DetachFileTransfer(receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// The live buffer sum has its own hard cap. This intentionally allocates the
	// exact 32MiB boundary once; the next one-byte file must be rejected pre-alloc.
	{
		RawConn raw;
		CHECK( ConnectRaw( L_srv, raw ), "raw aggregate-buffer client connected" );
		if ( raw.sock )
		{
			SdlNetFileTransfer receiver;
			srv->AttachFileTransfer(receiver );
			FtCap caps[2];
			unsigned short a = receiver.SetupReceive(&caps[0], raw.onServer );
			unsigned short b = receiver.SetupReceive(&caps[1], raw.onServer );
			std::vector<unsigned char> first = WireFrame( 4,
				FileBody( a, 0, 2, 32u * 1024u * 1024u + 1u, "buffer-a", 32u * 1024u * 1024u,
				          0, std::vector<unsigned char>{ 'a' } ) );
			CHECK( SendRaw( raw, first ), "32MiB boundary declaration queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return caps[0].progress == 1; } ),
			       "32MiB live-buffer boundary accepted" );
			std::vector<unsigned char> second = WireFrame( 4,
				FileBody( b, 0, 1, 1, "buffer-b", 1, 0, std::vector<unsigned char>{ 'b' } ) );
			size_t beforeLoss = L_srv.ids.size();
			CHECK( SendRaw( raw, second ), "over-cap buffer declaration queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return LostSince( L_srv, beforeLoss, raw.onServer ); } ),
			       "aggregate live-buffer cap is enforced before allocation" );
			srv->DetachFileTransfer(receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// The shipped client/server detach persistent SdlNetFileTransfer plugins before
	// Shutdown and attach them again on reconnect.  A partial old-session set must
	// release its handler, live buffer, and declared-byte reservation at detach;
	// otherwise this valid new-session reservation is rejected as an aggregate
	// overflow even though the old transport is no longer reachable.
	{
		RawConn raw;
		CHECK( ConnectRaw( L_srv, raw ), "raw detach-reset client connected" );
		if ( raw.sock )
		{
			SdlNetFileTransfer receiver;
			srv->AttachFileTransfer(receiver );
			FtCap oldCap;
			unsigned short oldID = receiver.SetupReceive(&oldCap, raw.onServer );
			std::vector<unsigned char> oldFrame = WireFrame( 4,
				FileBody( oldID, 0, 10, 300u * 1024u * 1024u, "old-session", 2, 0,
				          std::vector<unsigned char>{ 'a' } ) );
			CHECK( SendRaw( raw, oldFrame ), "old-session partial set queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return oldCap.progress == 1; } ),
			       "old-session partial set reserves capacity" );

			srv->DetachFileTransfer(receiver );
			srv->AttachFileTransfer(receiver );
			FtCap newCap;
			unsigned short newID = receiver.SetupReceive(&newCap, raw.onServer );
			std::vector<unsigned char> newFrame = WireFrame( 4,
				FileBody( newID, 0, 10, 300u * 1024u * 1024u, "new-session", 2, 0,
				          std::vector<unsigned char>{ 'b' } ) );
			size_t beforeLoss = L_srv.ids.size();
			CHECK( SendRaw( raw, newFrame ), "new-session partial set queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return newCap.progress == 1; } ),
			       "detach retires stale receiver accounting before reattach" );
			CHECK( !LostSince( L_srv, beforeLoss, raw.onServer ) && oldCap.progress == 1,
			       "reattach accepts new reservation without reviving old handler" );
			srv->DetachFileTransfer(receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	auto RunBadHeader = [&]( const char* label, unsigned char type, unsigned int bodyLen )
	{
		RawConn raw;
		bool connected = ConnectRaw( L_srv, raw );
		CHECK( connected, "raw control-header client connected" );
		if ( !connected ) return;
		std::vector<unsigned char> header;
		WireU32( header, bodyLen );
		header.push_back( type );
		size_t beforeLoss = L_srv.ids.size();
		CHECK( SendRaw( raw, header ), "malformed control header queued without body" );
		CHECK( PumpUntil( { &L_srv }, [&] { return LostSince( L_srv, beforeLoss, raw.onServer ); } ), label );
		NET_DestroyStreamSocket( raw.sock );
	};
	RunBadHeader( "BYE requires an exact empty body", 2, 1 );
	RunBadHeader( "FULL requires an exact empty body", 3, 1 );
	RunBadHeader( "PING requires an exact empty body", 5, 1 );
	RunBadHeader( "unknown frame type is rejected from its header", 99, 0 );
	RunBadHeader( "undersized RPC body is rejected from its header", 1, 0 );
	RunBadHeader( "oversized RPC body is rejected from its header", 1, 1024u * 1024u + 1u );
	RunBadHeader( "undersized file body is rejected from its header", 4, 27 );
	RunBadHeader( "oversized file frame is rejected from its header", 4, 1024u * 1024u + 1u );

	// Reliable/TCP RPC frames cannot be silently discarded when a peer exceeds
	// its work budget: that would leave the two simulations on different event
	// streams. Use an isolated server with a negligible refill rate so scheduler
	// pauses cannot replenish the 1MiB burst between the four 300KiB frames. The
	// first three fit, while the fourth must close the peer instead of disappearing.
	{
		SdlNetPeer* budgetServer = CreateSdlNetPeer();
		SdlNetInboundMessageBudget budget;
		budget.sustainedBytesPerSecond = 1;
		budget.burstBytes = DefaultSdlNetInboundMessageBurstBytes;
		CHECK( budgetServer->SetInboundMessageBudget( budget ),
		       "reliable RPC budget test fixes its refill rate" );
		const unsigned short mainPort = g_port;
		bool budgetServerStarted = false;
		for ( unsigned int attempt = 0; attempt < 128 && !budgetServerStarted; ++attempt )
		{
			g_port = (unsigned short)( 40000 + ( seed + 257 + attempt ) % 20000 );
			budgetServerStarted = budgetServer->Start(
				1, SdlNetEndpoint( g_port, "127.0.0.1" ) );
		}
		CHECK( budgetServerStarted, "isolated reliable RPC budget server starts" );
		budgetServer->SetTimeout( 120000 );
		REGISTER_SDLNET_MESSAGE( budgetServer, srvPING );
		PeerLog budgetLog{ budgetServer };
		RawConn raw;
		CHECK( budgetServerStarted && ConnectRaw( budgetLog, raw ),
		       "raw RPC-budget client connected" );
		if ( raw.sock )
		{
			const std::string rpcName = "srvPING";
			std::vector<unsigned char> body;
			body.reserve( 1 + rpcName.size() + 300u * 1024u );
			body.push_back( (unsigned char)rpcName.size() );
			body.insert( body.end(), rpcName.begin(), rpcName.end() );
			body.resize( 1 + rpcName.size() + 300u * 1024u, 0x5a );
			const std::vector<unsigned char> frame = WireFrame( 1, body );
			g_capSrv = Captured();
			bool firstThree = true;
			for ( int expected = 1; expected <= 3; ++expected )
			{
				firstThree = SendRaw( raw, frame ) && firstThree;
				firstThree = PumpUntil( { &budgetLog }, [&] { return g_capSrv.count == expected; } ) && firstThree;
			}
			CHECK( firstThree, "first three in-budget reliable RPCs dispatch" );
			size_t beforeLoss = budgetLog.ids.size();
			CHECK( SendRaw( raw, frame ), "over-budget reliable RPC queued" );
			CHECK( PumpUntil( { &budgetLog }, [&] { return LostSince( budgetLog, beforeLoss, raw.onServer ); } ),
			       "over-budget reliable RPC disconnects instead of silently dropping state" );
			CHECK( g_capSrv.count == 3,
			       "only in-budget RPCs are dispatched before disconnect" );
			NET_DestroyStreamSocket( raw.sock );
		}
		budgetServer->Shutdown( 0 );
		DestroySdlNetPeer( budgetServer );
		g_port = mainPort;
	}

	// Shutdown requested by a callback is deferred until the active parser and
	// all callbacks for its completed file have unwound. This is intentionally
	// last because successful deferred shutdown stops the server peer.
	{
		RawConn raw;
		CHECK( ConnectRaw( L_srv, raw ), "raw callback-shutdown client connected" );
		if ( raw.sock )
		{
			SdlNetFileTransfer receiver;
			srv->AttachFileTransfer(receiver );
			ShutdownFtCap cap;
			cap.peer = srv;
			unsigned short setID = receiver.SetupReceive(&cap, raw.onServer );
			std::vector<unsigned char> frame = WireFrame( 4,
				FileBody( setID, 0, 1, 1, "shutdown", 1, 0,
				          std::vector<unsigned char>{ 'x' } ) );
			CHECK( SendRaw( raw, frame ), "callback-shutdown frame queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return cap.complete == 1; } ),
			       "callback-requested shutdown unwinds safely" );
			CHECK( cap.progress == 1 && cap.files == 1 && cap.complete == 1,
			       "deferred shutdown preserves the current completed-file callbacks" );
			CHECK( srv->Poll() == nullptr,
			       "deferred shutdown completes before outer Receive returns" );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	srv->Shutdown( 0 );
	DestroySdlNetPeer( srv );

	// A listener may reserve its final slot for the embedded authority. With a
	// non-loopback allowance of zero, successful acceptance proves that the
	// production address classifier recognizes the real loopback socket.
	const auto ExerciseReservedLoopback = [&](const char* host,
		unsigned int portSalt, bool required) {
		SdlNetPeer* reservedServer = CreateSdlNetPeer();
		CHECK(reservedServer->SetReservedIncomingLoopbackConnections(1),
		       "loopback reservation configures before listener startup" );
		bool reservedStarted = false;
		unsigned short reservedPort = 0;
		for (unsigned int attempt = 0;
			attempt < 128 && !reservedStarted; ++attempt)
		{
			reservedPort = static_cast<unsigned short>(
				40000 + (seed + portSalt + attempt) % 20000);
			reservedStarted = reservedServer->Start(
				1, SdlNetEndpoint(reservedPort, host));
		}
		if (required)
			CHECK(reservedStarted,
			       "reserved IPv4 loopback listener binds" );
		if (!reservedStarted)
		{
			printf("skip reserved loopback address unavailable: %s\n", host);
			DestroySdlNetPeer(reservedServer);
			return;
		}
		CHECK(!reservedServer->SetReservedIncomingLoopbackConnections(1),
		       "live loopback reservation reconfiguration is rejected" );

		SdlNetPeer* reservedClient = CreateSdlNetPeer();
		CHECK(reservedClient->Start(1, SdlNetEndpoint()),
		       "reserved-loopback client starts" );
		CHECK(reservedClient->Connect(host, reservedPort),
		       "reserved-loopback client connection starts" );
		PeerLog reservedServerLog{ reservedServer };
		PeerLog reservedClientLog{ reservedClient };
		CHECK(PumpUntil({ &reservedServerLog, &reservedClientLog }, [&] {
			return reservedServerLog.Got(SDLNET_NEW_INCOMING_CONNECTION) &&
				reservedClientLog.Got(SDLNET_CONNECTION_ACCEPTED);
		}), "reserved capacity accepts its real loopback connection" );

		reservedClient->Shutdown(0);
		DestroySdlNetPeer(reservedClient);
		reservedServer->Shutdown(0);
		DestroySdlNetPeer(reservedServer);
	};
	ExerciseReservedLoopback("127.0.0.1", 512, true);
	ExerciseReservedLoopback("::1", 768, false);
	{
		SdlNetPeer* invalidReservation = CreateSdlNetPeer();
		CHECK(invalidReservation->SetReservedIncomingLoopbackConnections(1),
		       "oversized loopback reservation configures before capacity is known" );
		CHECK(!invalidReservation->Start(
			0, SdlNetEndpoint(g_port, "127.0.0.1")),
		       "listener rejects a reservation larger than total capacity" );
		DestroySdlNetPeer(invalidReservation);
	}

	// Campaign-sized streams opt in explicitly, before startup, and remain
	// bounded by transport-wide ceilings. This real socket sends well over the
	// strict generic 1 MiB burst without weakening the default flood regression.
	{
		SdlNetPeer* streamServer = CreateSdlNetPeer();
		SdlNetInboundMessageBudget invalidBudget;
		invalidBudget.sustainedBytesPerSecond = 0;
		CHECK(!streamServer->SetInboundMessageBudget(invalidBudget),
		       "zero inbound sustained budget is rejected before startup" );
		invalidBudget = SdlNetInboundMessageBudget();
		invalidBudget.burstBytes =
			MaximumSdlNetInboundMessageBurstBytes + 1u;
		CHECK(!streamServer->SetInboundMessageBudget(invalidBudget),
		       "inbound burst above the transport ceiling is rejected" );

		SdlNetInboundMessageBudget campaignBudget;
		campaignBudget.sustainedBytesPerSecond =
			MaximumSdlNetInboundMessageRateBytesPerSecond;
		campaignBudget.burstBytes = MaximumSdlNetInboundMessageBurstBytes;
		CHECK(streamServer->SetInboundMessageBudget(campaignBudget),
		       "bounded campaign inbound budget is accepted before startup" );

		bool streamStarted = false;
		for ( unsigned int attempt = 0; attempt < 128 && !streamStarted; ++attempt )
		{
			g_port = (unsigned short)( 40000 + ( seed + 256 + attempt ) % 20000 );
			streamStarted = streamServer->Start(
				1, SdlNetEndpoint(g_port, "127.0.0.1"));
		}
		CHECK(streamStarted, "campaign-budget server binds listener" );
		CHECK(!streamServer->SetInboundMessageBudget(campaignBudget),
		       "live inbound budget reconfiguration is rejected" );

		ContextCapture streamCapture;
		CHECK(streamServer->RegisterMessage(
			"campaign.stream", ContextHandler, &streamCapture),
		       "campaign stream handler registers" );
		PeerLog streamLog{ streamServer };
		RawConn raw;
		CHECK(streamStarted && ConnectRaw(streamLog, raw),
		       "campaign-budget raw client connects" );
		if (raw.sock)
		{
			const std::string rpcName = "campaign.stream";
			std::vector<unsigned char> body;
			body.reserve(1 + rpcName.size() + 60u * 1024u);
			body.push_back((unsigned char)rpcName.size());
			body.insert(body.end(), rpcName.begin(), rpcName.end());
			body.resize(1 + rpcName.size() + 60u * 1024u, 0x4c);
			const std::vector<unsigned char> frame = WireFrame(1, body);
			bool streamed = true;
			for (int expected = 1; expected <= 80; ++expected)
			{
				streamed = SendRaw(raw, frame) && streamed;
				streamed = PumpUntil({ &streamLog }, [&] {
					return streamCapture.captured.count == expected;
				}) && streamed;
				// Keep the stream just inside the configured 32 MiB/s sustained
				// rate while carrying more than the entire 4 MiB burst.
				SDL_Delay(2);
			}
			CHECK(streamed && streamCapture.captured.count == 80,
			       "opt-in peer sustains a bounded stream beyond its 4 MiB burst" );
			NET_DestroyStreamSocket(raw.sock);
		}
		streamServer->Shutdown(0);
		DestroySdlNetPeer(streamServer);
	}
	{
		SdlNetPeer* invalidBind = CreateSdlNetPeer();
		SdlNetEndpoint invalidSocket( g_port, "not a valid bind address" );
		CHECK( !invalidBind->Start( 1, invalidSocket ),
		       "invalid explicit bind address fails instead of exposing all interfaces" );
		DestroySdlNetPeer( invalidBind );
	}

	printf( g_failures ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", g_failures );
	SDL_Quit();
	return g_failures == 0 ? 0 : 1;
}
