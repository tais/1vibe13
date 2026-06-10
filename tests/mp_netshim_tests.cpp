// mp_netshim_tests.cpp -- loopback tests for the RakNet-compat netshim over
// SDL3_net (Multiplayer/netshim/). Links ONLY the shim + SDL3_net, no game code.
// Drives real TCP sockets on 127.0.0.1; exercises the exact semantics the JA2
// MP wrapper depends on (see RakPeerInterface.h).

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <SDL3/SDL.h>

#include "RakPeerInterface.h"
#include "RakNetworkFactory.h"
#include "MessageIdentifiers.h"
#include "FileListTransfer.h"

static int g_failures = 0;
#define CHECK( cond, msg ) do { if ( !( cond ) ) { ++g_failures; printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, msg ); } else { printf( "ok   %s\n", msg ); } } while ( 0 )

static const unsigned short PORT = 42117;

struct PeerLog
{
	RakPeerInterface* peer;
	std::vector<unsigned char> ids;
	std::vector<SystemAddress> addrs;
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
			for ( Packet* pk = pl->peer->Receive(); pk; pk = pl->peer->Receive() )
			{
				pl->ids.push_back( pk->data[0] );
				pl->addrs.push_back( pk->systemAddress );
				pl->peer->DeallocatePacket( pk );
			}
		}
		if ( pred() )
			return true;
		if ( SDL_GetTicks() - start >= (Uint64)timeoutMs )
			return false;
		SDL_Delay( 2 );
	}
}

// ---- RPC capture (static handlers, RakNet-style) ----------------------------
struct Captured
{
	int count = 0;
	std::vector<unsigned char> bytes;   // payload + 2 peeked pad bytes
	unsigned int bits = 0;
	SystemAddress sender;
};
static Captured g_capSrv, g_capA, g_capB;
static RakPeerInterface* g_relayPeer = nullptr;

static void Capture( Captured& c, RPCParameters* p )
{
	c.count++;
	c.bits = p->numberOfBitsOfData;
	unsigned int n = ( p->numberOfBitsOfData + 7 ) / 8;
	c.bytes.assign( p->input, p->input + n + 2 );   // +2: verify zero-padding
	c.sender = p->sender;
}
static void srvPING( RPCParameters* p )
{
	Capture( g_capSrv, p );
	if ( g_relayPeer )   // canonical server.cpp relay: everyone EXCEPT sender
		g_relayPeer->RPC( "clPONG", (const char*)p->input, p->numberOfBitsOfData, HIGH_PRIORITY, RELIABLE, 0,
		                  p->sender, true, 0, UNASSIGNED_NETWORK_ID, 0 );
}
static void clPONG_A( RPCParameters* p ) { Capture( g_capA, p ); }
static void clPONG_B( RPCParameters* p ) { Capture( g_capB, p ); }

// ---- file transfer capture ---------------------------------------------------
struct FtCap : public FileListTransferCBInterface
{
	int files = 0, progress = 0, complete = 0;
	std::string lastName;
	std::vector<char> lastData;
	bool OnFile( OnFileStruct* s ) override
	{
		files++;
		lastName = s->fileName;
		lastData.assign( s->fileData, s->fileData + s->finalDataLength );
		return true;
	}
	void OnFileProgress( OnFileStruct*, unsigned, unsigned, unsigned, char* ) override { progress++; }
	bool OnDownloadComplete( void ) override { complete++; return false; }
};

#pragma pack( push, 1 )
struct WirePayload { int a; short b; char name[10]; };
#pragma pack( pop )

int main( int, char** )
{
	SDL_Init( 0 );

	// ---------- 1. handshake: accept + connect events ----------
	RakPeerInterface* srv = RakNetworkFactory::GetRakPeerInterface();
	SocketDescriptor sd( PORT, 0 );
	CHECK( srv->Startup( 4, 30, &sd, 1 ), "server Startup binds listener" );
	srv->SetMaximumIncomingConnections( 2 );
	srv->SetOccasionalPing( true );
	srv->SetTimeoutTime( 120000, UNASSIGNED_SYSTEM_ADDRESS );
	REGISTER_STATIC_RPC( srv, srvPING );
	g_relayPeer = srv;

	RakPeerInterface* clA = RakNetworkFactory::GetRakPeerInterface();
	SocketDescriptor sd0;
	CHECK( clA->Startup( 1, 30, &sd0, 1 ), "client A Startup" );
	clA->RegisterAsRemoteProcedureCall( "clPONG", clPONG_A );
	CHECK( clA->Connect( "127.0.0.1", PORT, 0, 0 ), "client A Connect initiated" );

	PeerLog L_srv{ srv }, L_A{ clA };
	CHECK( PumpUntil( { &L_srv, &L_A }, [&] {
		return L_srv.Got( ID_NEW_INCOMING_CONNECTION ) && L_A.Got( ID_CONNECTION_REQUEST_ACCEPTED );
	} ), "handshake events on both sides" );

	// the wrapper's empty-slot sentinel must stay valid: real peers are nonzero/non-UNASSIGNED
	SystemAddress aOnSrv;
	for ( size_t i = 0; i < L_srv.ids.size(); ++i )
		if ( L_srv.ids[i] == ID_NEW_INCOMING_CONNECTION ) aOnSrv = L_srv.addrs[i];
	CHECK( aOnSrv.binaryAddress != 0 && aOnSrv != UNASSIGNED_SYSTEM_ADDRESS, "client addr nonzero + not UNASSIGNED" );

	// ---------- 2. RPC client->server: bytes, bit count, zero-pad, sender ----------
	WirePayload pay; memset( &pay, 0, sizeof( pay ) );
	pay.a = 0x11223344; pay.b = 0x55; strcpy( pay.name, "merc" );
	clA->RPC( "srvPING", (const char*)&pay, (unsigned int)sizeof( pay ) * 8, HIGH_PRIORITY, RELIABLE, 0,
	          UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0 );
	CHECK( PumpUntil( { &L_srv, &L_A }, [&] { return g_capSrv.count >= 1; } ), "RPC reached server handler" );
	CHECK( g_capSrv.bits == sizeof( pay ) * 8, "numberOfBitsOfData exact" );
	CHECK( memcmp( g_capSrv.bytes.data(), &pay, sizeof( pay ) ) == 0, "payload byte-exact" );
	CHECK( g_capSrv.bytes[sizeof( pay )] == 0 && g_capSrv.bytes[sizeof( pay ) + 1] == 0, "input zero-padded past payload" );
	CHECK( g_capSrv.sender == aOnSrv, "RPCParameters::sender == accept-time address" );

	// relay went back out broadcast-except-sender; A must NOT get its own echo
	SDL_Delay( 100 );
	PumpUntil( { &L_srv, &L_A }, [] { return false; }, 150 );
	CHECK( g_capA.count == 0, "broadcast-except-sender excludes the sender" );

	// ---------- 3. second client: relay reaches the OTHER client ----------
	RakPeerInterface* clB = RakNetworkFactory::GetRakPeerInterface();
	CHECK( clB->Startup( 1, 30, &sd0, 1 ), "client B Startup" );
	clB->RegisterAsRemoteProcedureCall( "clPONG", clPONG_B );
	clB->Connect( "127.0.0.1", PORT, 0, 0 );
	PeerLog L_B{ clB };
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return L_B.Got( ID_CONNECTION_REQUEST_ACCEPTED ); } ), "client B connected" );

	g_capSrv = Captured(); g_capA = Captured(); g_capB = Captured();
	clA->RPC( "srvPING", (const char*)&pay, (unsigned int)sizeof( pay ) * 8, HIGH_PRIORITY, RELIABLE, 0,
	          UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0 );
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return g_capB.count >= 1; } ), "relay delivered to client B" );
	CHECK( g_capA.count == 0, "relay still excludes sender A" );
	CHECK( memcmp( g_capB.bytes.data(), &pay, sizeof( pay ) ) == 0, "relayed payload byte-exact" );

	// ---------- 4. targeted send (broadcast=false) ----------
	g_capA = Captured(); g_capB = Captured();
	srv->RPC( "clPONG", (const char*)&pay, (unsigned int)sizeof( pay ) * 8, HIGH_PRIORITY, RELIABLE, 0,
	          aOnSrv, false, 0, UNASSIGNED_NETWORK_ID, 0 );
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return g_capA.count >= 1; } ), "targeted RPC reached A" );
	SDL_Delay( 50 );
	PumpUntil( { &L_srv, &L_A, &L_B }, [] { return false; }, 100 );
	CHECK( g_capB.count == 0, "targeted RPC did not reach B" );

	// ---------- 5. unknown RPC name: dropped silently ----------
	clA->RPC( "noSuchHandler", (const char*)&pay, (unsigned int)sizeof( pay ) * 8, HIGH_PRIORITY, RELIABLE, 0,
	          UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0 );
	size_t srvPackets = L_srv.ids.size();
	SDL_Delay( 50 );
	PumpUntil( { &L_srv, &L_A, &L_B }, [] { return false; }, 100 );
	CHECK( L_srv.ids.size() == srvPackets, "unknown RPC produced no user packet" );

	// ---------- 6. server full: third client refused ----------
	RakPeerInterface* clC = RakNetworkFactory::GetRakPeerInterface();
	CHECK( clC->Startup( 1, 30, &sd0, 1 ), "client C Startup" );
	clC->Connect( "127.0.0.1", PORT, 0, 0 );
	PeerLog L_C{ clC };
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B, &L_C }, [&] { return L_C.Got( ID_NO_FREE_INCOMING_CONNECTIONS ); } ),
	       "third client got ID_NO_FREE_INCOMING_CONNECTIONS" );
	clC->Shutdown( 0 );
	RakNetworkFactory::DestroyRakPeerInterface( clC );

	// ---------- 7. big payload (256 KB) framed across many reads ----------
	{
		std::vector<char> big( 256 * 1024 );
		for ( size_t i = 0; i < big.size(); ++i ) big[i] = (char)( i * 31 + 7 );
		g_capSrv = Captured(); g_relayPeer = nullptr;   // no relay for this one
		clA->RPC( "srvPING", big.data(), (unsigned int)big.size() * 8, HIGH_PRIORITY, RELIABLE, 0,
		          UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0 );
		CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return g_capSrv.count >= 1; }, 10000 ), "256KB RPC arrived" );
		CHECK( g_capSrv.bits == big.size() * 8 && memcmp( g_capSrv.bytes.data(), big.data(), big.size() ) == 0,
		       "256KB payload byte-exact" );
	}

	// ---------- 8. file transfer: 2 files, progress + bytes + completion ----------
	{
		FileListTransfer fltS, fltR;
		srv->AttachPlugin( &fltS );
		srv->AttachPlugin( &fltS );          // wrapper re-attaches on retry: must be idempotent
		clA->AttachPlugin( &fltR );
		clA->SetSplitMessageProgressInterval( 1 );

		FtCap cap;
		unsigned short setID = fltR.SetupReceive( &cap, false, SystemAddress() );

		std::vector<char> f1( 200 * 1024 );
		for ( size_t i = 0; i < f1.size(); ++i ) f1[i] = (char)( i ^ 0x5A );
		const char* f2 = "tiny file";
		FileList fl;
		fl.AddFile( "Data/Maps/big.dat", f1.data(), (unsigned)f1.size(), (unsigned)f1.size(), FileListNodeContext( 0, 0 ), false );
		fl.AddFile( "Data/tiny.txt", f2, 9, 9, FileListNodeContext( 0, 0 ), false );
		fltS.Send( &fl, srv, aOnSrv, setID, MEDIUM_PRIORITY, 0, false, 0, 5000 );

		CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return cap.complete >= 1; }, 10000 ), "file set completed" );
		CHECK( cap.files == 2, "both files delivered" );
		CHECK( cap.progress >= (int)( f1.size() / 5000 ), "per-chunk progress callbacks fired" );
		CHECK( cap.lastName == "Data/tiny.txt" && memcmp( cap.lastData.data(), f2, 9 ) == 0, "file order + bytes exact" );
		srv->DetachPlugin( &fltS );
		clA->DetachPlugin( &fltR );
	}

	// ---------- 9. graceful disconnect ----------
	size_t before = L_srv.ids.size();
	clB->Shutdown( 300 );
	CHECK( PumpUntil( { &L_srv, &L_A }, [&] {
		for ( size_t i = before; i < L_srv.ids.size(); ++i )
			if ( L_srv.ids[i] == ID_DISCONNECTION_NOTIFICATION ) return true;
		return false;
	} ), "server saw ID_DISCONNECTION_NOTIFICATION for B" );
	RakNetworkFactory::DestroyRakPeerInterface( clB );

	// ---------- 10. CloseConnection kick: client sees the drop ----------
	size_t beforeA = L_A.ids.size();
	srv->CloseConnection( aOnSrv, true );
	CHECK( PumpUntil( { &L_A }, [&] {
		for ( size_t i = beforeA; i < L_A.ids.size(); ++i )
			if ( L_A.ids[i] == ID_DISCONNECTION_NOTIFICATION || L_A.ids[i] == ID_CONNECTION_LOST ) return true;
		return false;
	} ), "kicked client A notified" );

	clA->Shutdown( 0 );
	srv->Shutdown( 0 );
	RakNetworkFactory::DestroyRakPeerInterface( clA );
	RakNetworkFactory::DestroyRakPeerInterface( srv );

	printf( g_failures ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", g_failures );
	SDL_Quit();
	return g_failures == 0 ? 0 : 1;
}
