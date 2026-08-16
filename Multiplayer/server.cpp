#include "sgp_bounded_string.h"
#include "TacticalWorldAdapter.h"
#include "SdlNetTransport.h"
#include <assert.h>
#include <cstdio>
#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <list>
#include "connect.h"
#include "types.h"
#include "GameSettings.h"
#include "message.h"
#include "FileMan.h"
#include "INIReader.h"
#include <vfs/Core/vfs.h>
#include "transfer_rules.h"
#include "MPJoinScreen.h"
#include "Game Init.h"
#include "Text.h"
#include "network.h"
#include "message.h"
#include "Overhead.h"
#include "SoldierRepository.h"
#include "fresh_header.h"
#include "Debug Control.h"
#include "MPXmlTeams.hpp"

using namespace ja2::mp;
using namespace ja2::mp::net;

extern CHAR16 gzFileTransferDirectory[100];

// WANNE: FILE TRANSFER
unsigned int setID;

// Sender progress notification
class ServerFileListProgress : public SdlNetFileProgress
{
	virtual void OnFilePush(const char *fileName, unsigned int fileLengthBytes, unsigned int offset, unsigned int bytesBeingSent, bool done, ConnectionId targetSystem)
	{
		// WANNE: Removed output in strategy log screen, because otherwise we do not see chat messages easily
		/*
		if (done)
		{
			char* filename = ExtractFilename((char*)fileName);
			ScreenMsg( FONT_RED, MSG_MPSYSTEM, MPServerMessage[10], filename);
		}
		*/
	}

	char *ExtractFilename(char *pathname) 
	{
		char *s;

		if ((s=strrchr(pathname, '\\')) != NULL) s++;
		else if ((s=strrchr(pathname, '/')) != NULL) s++;
		else if ((s=strrchr(pathname, ':')) != NULL) s++;
		else s = pathname;
		return s;
	}
} serverFileListProgress;

// ------------------------------
// Global Server Variables from ja2_mp.ini
// ------------------------------
char gKitBag[100];
UINT8 gMaxMercs;
UINT8 gDisableMorale;
UINT8 gReportHiredMerc;
UINT8 gDisableBobbyRay;
UINT8 gDisableMercEquipment;
UINT8 gSameMercAllowed;
float gDamageMultiplier;
UINT8 gMaxClients ;
UINT8 gGameType;
UINT8 gDifficultyLevel;
UINT8 gSkillTraits;
UINT8 gSyncGameDirectory;
INT32 gSecondsPerTick;
INT32 gStartingCash;
float gStartingTime;
UINT8 gWeaponReadyBonus;
UINT8 gInventoryAttachment;
UINT8 gDisableSpectatorMode;
UINT8 gMaxEnemiesEnabled;
// ------------------------------

UINT16 nc; //number of open connection
UINT16 ncr; //number of ready confirmed connections
//something keep record of ready connections ..
int mercs_ready[255];

unsigned char SGetPacketIdentifier(SdlNetEvent *p);
unsigned char SpacketIdentifier;

SdlNetPeer *server;

// PR-1: central short-frame guard -- bail before a (Struct*)cast if the wire
// payload is smaller than the struct the handler reads (heap over-read otherwise).
#define RPC_REQUIRE_BYTES(p,T) do{ if ( ((long)(p)->size) < (long)sizeof(T) ) return; }while(0)

// WANNE: FILE TRANSFER
SdlNetFileTransfer fltServer;	// flt1
SdlNetFileList fileList;
// OJW - 20090405
long fileListTotalBytes=0;

int numreadyteams;
int readyteamreg[10];

bool Sawarded;

ConnectionId blank;

typedef struct
{
	ConnectionId address;
	int cl_number;
	
}client_data;

client_data client_d[4];
int client_mercteam[4] = { 0 , 1 , 2 , 3 }; // random index of random_merc_teams per player

// Dedicated-server admin: a connected client granted host-style control.
ConnectionId gAdminAddr;
bool          gHasAdmin = false;
char          gAdminPassword[64] = {0};   // from ja2_mp.ini; empty => first remote client auto-admin

bool inline can_joingame();

int f_rec_num(int mode, ConnectionId sender)//from client data
{
	int x;
	client_data cl_record;
	for ( x=0; x<4;x++)
	{
		cl_record = client_d[x];

		if(mode==0)//find empty slot for new record
		{
			if(!cl_record.address)
				return(x);
		}
		if(mode==1)//wipe clean all
		{
			client_d[x].address = NoConnection;
			client_d[x].cl_number=0;
		}
		if(mode==2)//clear one record
		{
			if(cl_record.address == sender)
			{
				client_d[x].address = NoConnection;
				client_d[x].cl_number=0;
				return(254);
			}

		}
		// OJW - 090212 - look up client number
		if (mode == 3)
		{
			if(cl_record.address == sender)
			{
				return (x);
			}
		}
	}
	if(mode == 0)//'no free slots'
	{
	ScreenMsg( FONT_RED, MSG_MPSYSTEM, L"Client Record Error, Restart Server, and Report Error." );
	return (255);
	}
	return(254);
}

// use AnyConnection instead of rpcParameters->sender to send it back to yourself (the sender)
// there is very little in here dependant on the game engine and originally started out as an independant dedicated server .exe, and could if go ther again ... hayden.
//********* RPC SECTION ************

void sendPATH(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recievePATH", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

// OJW - 20090405
void sendDOWNLOADSTATUS(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveDOWNLOADSTATUS", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendSTANCE(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveSTANCE", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendDIR(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveDIR", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendFIRE(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveFIRE", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendHIT(SdlNetMessage *rpcParameters)
{
	EV_S_WEAPONHIT* hit = (EV_S_WEAPONHIT*)rpcParameters->data;

	// MP wire guard: the attacker id is raw wire data; the slot can be empty or
	// the sentinel NOBODY -- never deref unchecked (mp_audit_findings.json)
	TacticalActor* pAtt =
		hit->ubAttackerID != NOBODY
			? GetJa2SoldierRepository().resolve(
				hit->ubAttackerID.i)
			: NULL;
	if ( pAtt != NULL )
	{
		int team = pAtt->roster().team();

		// AI
		if (team == 1)
			team = 4;
		// Client
		else if (team >= 6)
			team -= 6;
		else if (team == 0)
			team = CLIENT_NUM-1; // this case should not be possible, including as a precaution

		if ( team >= 0 && team < 5 )
			gMPPlayerStats[team].hits++;
	}

	server->SendMessage("recieveHIT", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendDISMISS(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveDISMISS", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendHIRE(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveHIRE", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendguiPOS(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveguiPOS", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendguiDIR(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveguiDIR", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendEndTurn(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveEndTurn", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendAI(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveAI", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendSTOP(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveSTOP", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}
void sendINTERRUPT(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveINTERRUPT", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}
void sendREADY(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveREADY", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendGUI(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveGUI", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendBULLET(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveBULLET", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendGRENADE(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveGRENADE", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendGRENADERESULT(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveGRENADERESULT", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendPLANTEXPLOSIVE(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recievePLANTEXPLOSIVE", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendDETONATEEXPLOSIVE(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveDETONATEEXPLOSIVE", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendDISARMEXPLOSIVE(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveDISARMEXPLOSIVE", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendSPREADEFFECT(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveSPREADEFFECT", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendNEWSMOKEEFFECT(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveNEWSMOKEEFFECT", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendEXPLOSIONDAMAGE(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveEXPLOSIONDAMAGE", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendSTATE(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveSTATE", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendDEATH(SdlNetMessage *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, death_struct);	// short-frame guard (H12/H13)
	// the master copy of the scoreboard is kept on the server
	death_struct* nDeath = (death_struct*)rpcParameters->data;

	// Save Stats on the server side
	// H12: wire team-1 indexes gMPPlayerStats[5]; team==0 underflows to [-1], >5 overflows.
	// Clamp the same way sendHIT does before touching the scoreboard.
	if ( nDeath->soldier_team >= 1 && nDeath->soldier_team <= 5 )
		gMPPlayerStats[nDeath->soldier_team-1].deaths++;
	if ( nDeath->attacker_team >= 1 && nDeath->attacker_team <= 5 )
		gMPPlayerStats[nDeath->attacker_team-1].kills++;
	
	// get the client number of the client sending the message
	int iCLnum = f_rec_num(3,rpcParameters->sender)+1;

	server->SendMessage("recieveDEATH", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);

#ifdef JA2BETAVERSION
	wchar_t ateam[5];
	wchar_t steam[5];
	wchar_t clnum[5];
	_itow(nDeath->attacker_team,ateam,10);
	_itow(nDeath->soldier_team,steam,10);
	_itow(iCLnum,clnum,10);
	ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, L"DEBUG: Soldier Killed : Attacking team %s , Soldier Team %s, Sender %s",ateam,steam,clnum);
	char logmsg[100];
	sprintf( logmsg, "MP DEBUG: Soldier Killed #%i : Attacking team %i , Soldier Team %i, Sender %i\n", nDeath->soldier_id.i, nDeath->attacker_team, nDeath->soldier_team, iCLnum );
	MPDebugMsg( logmsg );
#endif
}
void sendhitSTRUCT(SdlNetMessage *rpcParameters)
{
	EV_S_STRUCTUREHIT* miss = (EV_S_STRUCTUREHIT*)rpcParameters->data;
	
	TacticalActor* attacker =
		miss->ubAttackerID != NOBODY
			? GetJa2SoldierRepository().resolve(
				miss->ubAttackerID.i)
			: NULL;
	if ( attacker )
	{
		int team = attacker->roster().team();
		
		// AI
		if (team == 1) 
			team = 4;
		// Clients
		else if (team >= 6) 
			team -= 6;
		else if (team == 0) 
			team = CLIENT_NUM-1; // this case should not be possible, including as a precaution

        Assert(team<5); // FIXME
		if ( team >= 0 && team < 5 )
			gMPPlayerStats[team].misses++;
	}

	server->SendMessage("recievehitSTRUCT", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}
void sendhitWINDOW(SdlNetMessage *rpcParameters)
{
	EV_S_WINDOWHIT* miss = (EV_S_WINDOWHIT*)rpcParameters->data;
	

	TacticalActor* attacker =
		miss->ubAttackerID != NOBODY
			? GetJa2SoldierRepository().resolve(
				miss->ubAttackerID.i)
			: NULL;
	if ( attacker )
	{
		int team = attacker->roster().team();
		
		// AI
		if (team == 1) 
			team = 4;
		// Clients
		else if (team >= 6) 
			team -= 6;
		else if (team == 0) 
			team = CLIENT_NUM-1; // this case should not be possible, including as a precaution

        Assert(team<5); // FIXME
		if ( team >= 0 && team < 5 )
			gMPPlayerStats[team].misses++;
	}

	server->SendMessage("recievehitWINDOW", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}
void sendMISS(SdlNetMessage *rpcParameters)
{
	EV_S_MISS* miss = (EV_S_MISS*)rpcParameters->data;

	TacticalActor* attacker =
		miss->ubAttackerID != NOBODY
			? GetJa2SoldierRepository().resolve(
				miss->ubAttackerID.i)
			: NULL;
	if ( attacker )
	{
		int team = attacker->roster().team();
		
		// AI
		if (team == 1) 
			team = 4;
		// Clients
		else if (team >= 6) 
			team -= 6;
		else if (team == 0) 
			team = CLIENT_NUM-1; // this case should not be possible, including as a precaution

        Assert(team<5); // FIXME
		if ( team >= 0 && team < 5 )
			gMPPlayerStats[team].misses++;
	}

	server->SendMessage("recieveMISS", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}
void updatenetworksoldier(SdlNetMessage *rpcParameters)
{
	server->SendMessage("UpdateSoldierFromNetwork", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void Snull_team(SdlNetMessage *rpcParameters)
{
	server->SendMessage("null_team", (const char*)rpcParameters->data, (*rpcParameters).size, AnyConnection, true);
}

void sendFIREW(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieve_fireweapon", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendDOOR(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieve_door", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void endINTERRUPT(SdlNetMessage *rpcParameters)
{
	server->SendMessage("resume_turn", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendWIPE(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieve_wipe", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendHEAL(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieve_heal", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

// OJW - edge and team changes
void sendEDGECHANGE(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveEDGECHANGE", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void sendTEAMCHANGE(SdlNetMessage *rpcParameters)
{
	server->SendMessage("recieveTEAMCHANGE", (const char*)rpcParameters->data, (*rpcParameters).size, rpcParameters->sender, true);
}

void requestSETID(ConnectionId addr)
{
	server->SendMessage("requestSETID", "", 0, addr, false);
}

void receiveSETID(SdlNetMessage *rpcParameters)
{
	setID = atoi((const char *)rpcParameters->data);

	// WANNE: FILE TRANSFER: Send the files to the client
	fltServer.Send(fileList, *server, rpcParameters->sender, setID, 5000);
}

void startCOMBAT(SdlNetMessage *rpcParameters)
{
	if(!( IsJa2TacticalCombatActive() ))
	
	{

		SetJa2TacticalCombatMode( true );

		sc_struct* data = (sc_struct*)rpcParameters->data;
		EndTurn( data->ubStartingTeam );
	}

}

void sendREAL(SdlNetMessage *rpcParameters)
{
	real_struct* rData = (real_struct*)rpcParameters->data;

	if(readyteamreg[rData->bteam]==0)
	{
		readyteamreg[rData->bteam]=1;//register vote, to prevent double voting ;p~ //hayden
		numreadyteams++;

		int numactiveteams=0;
		int b;
		for(int i=6;i<=LAST_TEAM;i++)
		{
			if(i==6)
				b=0;
			else 
				b=i;

			if(IsTacticalTeamActive( b ))
				numactiveteams++;
		}

		//check # clients ready for realtime
		if (numreadyteams >= numactiveteams)
		{
			//if all send notification for realtime changeover
			//ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, L"Switching to realtime..." );
			numreadyteams=0;
			memset( &readyteamreg , 0 , sizeof (int) * 10);

			server->SendMessage("gotoRT", (const char*)rpcParameters->data, (*rpcParameters).size, AnyConnection, true);
		}
	}
}

// 20081222 - OJW
void sendGAMEOVER(SdlNetMessage *rpcParameters)
{
	// ignore the RPCParams and send the server side scoreboard
	server->SendMessage("recieveGAMEOVER", (const char*)gMPPlayerStats, sizeof(gMPPlayerStats), AnyConnection, true);
}

void sendCHATMSG(SdlNetMessage *rpcParameters)
{
	// ignore the RPCParams and send the server side scoreboard
	server->SendMessage("recieveCHATMSG", (const char*)rpcParameters->data, (*rpcParameters).size, AnyConnection, true);
}

// OJW - 20081223
// fix client disconnecting mid game, allowing the game to proceed
void HandleDisconnect(ConnectionId sender)
{
	// find the CLIENT_NUM of the player
	int x;
	client_data cl_record;

	for ( x=0; x<4;x++)
	{
		cl_record = client_d[x];
		if(cl_record.address == sender)
		{
			// notify all the clients of the disconnect
			server->SendMessage("recieveDISCONNECT", (const char*)&cl_record.cl_number, sizeof(int), AnyConnection, true);
			f_rec_num(2,sender); // remove from server's client list

			// dedicated server: if the admin dropped, release the admin slot so the
			// next remote client (or a reconnect) can become/claim admin again.
			if ( gHasAdmin && sender == gAdminAddr )
			{
				gHasAdmin = false;
				gAdminAddr = NoConnection;
				printf( "[dedicated] admin (client #%d) dropped -- admin slot released\n", cl_record.cl_number ); fflush( stdout );
			}
			break;
		}
	}
}

// OJW - 20081218
// shuffle an integer array
//<TODO> remove shuffledList and put directly into arr[]
void rSortArray(int* arr, int len)
{
	std::list<int> tmpList;
	std::list<int> shuffledList;
	std::list<int>::iterator Iter;
	int i=0;

	// add all our array items
	for(i=0; i<len; i++)
		tmpList.push_front(arr[i]);

	// shuffle the items
	while(tmpList.size())
	{
		int iRandPos =  rand() % tmpList.size();
		for(Iter = tmpList.begin(); iRandPos>0; iRandPos--, Iter++);
		
		shuffledList.push_front(*Iter);
		tmpList.erase(Iter);
	}

	// add all our elements
	for(i=0; i<len; i++)
	{
		arr[i] = shuffledList.back();
		shuffledList.pop_back();
	}
}

// WANNE: FILE TRANSFER: Send all the settings the client needs to know for the file transfer before file transfer starts
void requestFILE_TRANSFER_SETTINGS(SdlNetMessage *rpcParameters)
{
	ConnectionId sender = rpcParameters->sender;//get senders address

	// The complete struct is sent over the wire. Clear padding as well as fields
	// so the packet never exposes stale stack bytes.
	filetransfersettings_struct fts = {};

	fts.syncClientsDirectory = gSyncGameDirectory;
	sgp_strlcpy(fts.fileTransferDirectory, s_ServerId.getServerId(vfs::Path(gzFileTransferDirectory)).utf8().c_str());
	sgp_strlcpy(fts.serverName, cServerName);
	fts.totalTransferBytes = fileListTotalBytes;

	// OJW - 200907819 - Only send to the client that asked for it
	server->SendMessage("recieveFILE_TRANSFER_SETTINGS", (const char*)&fts, sizeof(filetransfersettings_struct), sender, false);
}

//************************* //AnyConnection
//START INTERNAL SERVER
//*************************
//void send_settings (void)//send server settings to client
void adminCmd(SdlNetMessage *rpcParameters)
{
	admin_cmd_struct* ac = (admin_cmd_struct*)rpcParameters->data;
	ac->password[63] = 0;
	printf( "[dedicated] adminCmd received: cmd=%d (hasAdmin=%d isAdminSender=%d)\n", (int)ac->cmd, gHasAdmin?1:0, (gHasAdmin && rpcParameters->sender == gAdminAddr)?1:0 ); fflush( stdout );
	if ( ac->cmd == ADMIN_CMD_AUTH )
	{
		if ( gAdminPassword[0] != 0 && strncmp( ac->password, gAdminPassword, 63 ) == 0 )
		{
			gAdminAddr = rpcParameters->sender;
			gHasAdmin = true;
			unsigned char one = 1;
			server->SendMessage("recieveADMIN", (const char*)&one, 1, rpcParameters->sender, false);
			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, L"A client authenticated as the server admin" );
			printf( "[dedicated] a client authenticated as admin\n" ); fflush( stdout );
		}
	}
	else if ( ac->cmd == ADMIN_CMD_START )
	{
		if ( gHasAdmin && rpcParameters->sender == gAdminAddr )
		{
			extern bool allowlaptop;
			extern bool goahead;
			// The admin pressing START IS the "everyone is here, go" authority. Once
			// laptops are unlocked (phase 1 done), the second START must begin the
			// battle -- the headless host never readies, so don't wait on its vote;
			// force the go-ahead. (cMaxClients still gates real-player placement.)
			if ( allowlaptop )
				goahead = true;
			printf( "[dedicated] admin requested START (allowlaptop=%d -> goahead forced=%d)\n", allowlaptop?1:0, allowlaptop?1:0 ); fflush( stdout );
			start_battle();
		}
	}
}

void requestSETTINGS(SdlNetMessage *rpcParameters )
{
	RPC_REQUIRE_BYTES(rpcParameters, client_info);	// short-frame guard (H14/H13)
	// dont generate or send settings to a new user if they are about to be disconnected
	// because no more players can join the the game
	if (can_joingame())
	{
		client_info* clinf = (client_info*)rpcParameters->data;

		// L3: wire name/version are strcmp'd and strcpy'd -- force NUL-termination so a
		// non-terminated field can't over-read past the fixed buffers.
		clinf->client_version[29] = 0;
		clinf->client_name[29] = 0;

		// OJW - 20090507
		// Disconnect if version is wrong
		if (strcmp(clinf->client_version,MPVERSION)!=0)
		{
			CHAR16 verErrMsg[255];
			sgp_swprintf(verErrMsg, 255, MPClientMessage[66], clinf->client_version,MPVERSION);

			// send disconnect reason only to this client
			server->SendMessage("recieveDISCONNECTREASON", (const char*)&verErrMsg, sizeof(CHAR16) * 255, rpcParameters->sender, false);

			ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"CONNECTION REJECTED - CLIENT HAS WRONG VERSION");
			// disconnect this client
			server->CloseConnection(rpcParameters->sender, true);
			return;
		}
		
		//server assigned client numbers - hayden.
		ConnectionId sender = rpcParameters->sender;//get senders address
		int bslot = f_rec_num(0,blank);//get empty record slot
		client_d[bslot].address=sender; //record clients address
		int new_cl_num = bslot+1;//client number to assign
		client_d[bslot].cl_number=new_cl_num; //record clients number
		printf( "[dedicated] client registered: cl_number=%d (pw_set=%d hasAdmin=%d)\n", new_cl_num, gAdminPassword[0]!=0, gHasAdmin?1:0 ); fflush( stdout );

		{
			extern BOOLEAN gfDedicatedServer;
			// dedicated server, no password configured: the first REMOTE client
			// (cl_number >= 2; #1 is the headless host's own loopback) becomes admin.
			if ( gfDedicatedServer && !gHasAdmin && gAdminPassword[0] == 0 && new_cl_num >= 2 )
			{
				gAdminAddr = sender;
				gHasAdmin = true;
				unsigned char one = 1;
				server->SendMessage("recieveADMIN", (const char*)&one, 1, sender, false);
				ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, L"Client #%d is now the server admin", new_cl_num );
				printf( "[dedicated] client #%d is now admin\n", new_cl_num ); fflush( stdout );
			}
		}

		// This is serialized byte-for-byte, including padding and fields that are
		// conditional below. Value-initialize it to make the wire data deterministic.
		settings_struct lan = {};
		
		lan.client_num = new_cl_num; //new server assigned number
		// client_name arrives off the wire (untrusted); bound the copy to the dest
		// [30] and force NUL-termination instead of strcpy'ing a possibly-oversized
		// or unterminated field.
		sgp_strlcpy(lan.client_name, clinf->client_name);

		lan.randomStartingEdge = gRandomStartingEdge;
		lan.randomMercs = gRandomMercs;

		lan.maxClients = gMaxClients;
		memcpy(lan.kitBag , gKitBag,sizeof (char)*100);
		lan.damageMultiplier = gDamageMultiplier;

		lan.sameMercAllowed = gSameMercAllowed;
		lan.gsMercArriveSectorX = gsMercArriveSectorX;
		lan.gsMercArriveSectorY = gsMercArriveSectorY;

		lan.enemyEnabled = gEnemyEnabled;
		lan.creatureEnabled = gCreatureEnabled;
		lan.militiaEnabled = gMilitiaEnabled;
		lan.civEnabled = gCivEnabled;

		lan.gameType = gGameType;

		lan.disableMorale = gDisableMorale;
		lan.reportHiredMerc = gReportHiredMerc;
		lan.secondsPerTick = gSecondsPerTick;

		// WANNE.MP: Check
		lan.soubBobbyRayQuality = BR_AWESOME;
		lan.soubBobbyRayQuantity = BR_AWESOME;
		lan.sofGunNut = TRUE;	
		lan.soubGameStyle = STYLE_REALISTIC;
		lan.soubDifficultyLevel = gDifficultyLevel;
		lan.soubSkillTraits = gSkillTraits;
		lan.sofTurnTimeLimit = TRUE;
		lan.sofIronManMode = FALSE;
		lan.startingCash = gStartingCash;
		
		// Old/Old
		if (gInventoryAttachment == INVENTORY_OLD)
		{			
			lan.inventoryAttachment = 0;	
		}
		else
		{
			// New/Old
			if (gGameOptions.ubAttachmentSystem == ATTACHMENT_OLD)
			{				
				lan.inventoryAttachment = 1; // New/Old
			}
			// New/New
			else
			{				
				lan.inventoryAttachment = 2;	// New/New
			}
		}		
		
		lan.disableBobbyRay=gDisableBobbyRay;
		lan.disableMercEquipment=gDisableMercEquipment;

		lan.maxMercs = gMaxMercs;
		
		memcpy( lan.client_names , client_names, sizeof( char ) * 4 * 30 );
		lan.team=clinf->team;
		// OJW - 20090530 - fix teams not initialised properly
		client_teams[ lan.client_num - 1 ] = lan.team;
		
		// OJW - 20081218
		if (gRandomStartingEdge)
		{
			// Get the edge from the randomized "client_edges"
			lan.startingSectorEdge = client_edges[lan.client_num-1];
		}
		else
		{
			// WANNE: on DM, each client should get a unique starting edge per default
			if (gGameType == MP_TYPE_DEATHMATCH || gGameType == MP_TYPE_TEAMDEATMATCH)
			{
				client_edges[0] = MP_EDGE_NORTH;	// client 1
				client_edges[1] = MP_EDGE_SOUTH;	// client 2
				client_edges[2] = MP_EDGE_EAST;		// client 3
				client_edges[3] = MP_EDGE_WEST;		// client 4

				lan.startingSectorEdge = client_edges[lan.client_num-1];
			}
			else
			{
				lan.startingSectorEdge=clinf->cl_edge;
			}
		}

		// OJW - 20081223
		if (gRandomMercs)
		{			
			mpTeams.SerializeProfiles(lan.random_mercs);
		}

		lan.startingTime = gStartingTime;
		lan.weaponReadyBonus = gWeaponReadyBonus;
		lan.inventoryAttachment = gInventoryAttachment;
		lan.disableSpectatorMode = gDisableSpectatorMode;

		// OJW - 20081204
		sgp_strlcpy(lan.server_name, cServerName);
		memcpy(lan.client_edges,client_edges,sizeof(int)*5);
		memcpy(lan.client_teams,client_teams,sizeof(int)*4);

		// OJW - 20091024 - send servers random table
		memcpy(lan.random_table,guiPreRandomNums,sizeof(UINT32)*MAX_PREGENERATED_NUMS);

		// OJW - 20090507
		// send server version to client
		sgp_strlcpy(lan.server_version, MPVERSION);

		server->SendMessage("recieveSETTINGS", (const char*)&lan, sizeof(settings_struct), AnyConnection, true);

		// WANNE: FILE TRANSFER: A client connected -> start the file transfer!
		if (gSyncGameDirectory)
			requestSETID(rpcParameters->sender);
	}
}

// added 081201 by Owen , allow the server to change the map while its still not in laptop mode
void send_mapchange(void)
{
	if (is_server  && !allowlaptop)
	{
		mapchange_struct lan;

		lan.gsMercArriveSectorX=gsMercArriveSectorX;
		lan.gsMercArriveSectorY=gsMercArriveSectorY;
		lan.startingTime = gStartingTime;

		server->SendMessage("recieveMAPCHANGE", (const char*)&lan, sizeof(mapchange_struct), AnyConnection, true);
	}
	else
	{
		ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, MPServerMessage[45]);
	}
}

// OJW 090212
bool inline can_joingame()
{
	return !allowlaptop;
}

// Allow server to disconnect incoming clients after the game has started
void CheckIncomingConnection(SdlNetEvent* p)
{
	// some clients might reconnect after disconnecting, either after the laptop is unlocked
	// or after the game has started
	// we dont want to allow this as thier game will be out of state
	if (!can_joingame())
	{
		ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, L"CONNECTION REJECTED - GAME HAS STARTED");
		// disconnect this client, no need to notify them as they will know if they disconnected
		// before receiving a settings packet that they were not allowed to join
		server->CloseConnection(p->connection, true);
	}
}

void AddFilesToSendList()
{
	// we cannot just iterate over "*/*" as we would get ALL files and we don't want to send all files
	// instead we only iterate over the files in the "_MULTIPLAYER" profile
	vfs::CProfileStack *PS = getVFS()->getProfileStack();
	vfs::CVirtualProfile *prof = PS->getProfile("_MULTIPLAYER");
	if(prof != PS->topProfile())
	{
		// there is not supposed to be another profile?
		// output error message
		return;
	}
	CTransferRules transferRules;
	transferRules.initFromTxtFile("transfer_rules.txt");
	vfs::IBaseLocation* loc = prof->getLocation("");
	SGP_THROW_IFFALSE(loc != NULL, "MP profile was successfully created, but the root directory is not included");
	vfs::IBaseLocation::Iterator it = loc->begin();
	int i=0;
	for(; !it.end(); it.next(), i++)
	{
		vfs::Path const& valid_path = it.value()->getPath();
		if(transferRules.applyRule(valid_path()) == CTransferRules::ACCEPT)
		{
			// transfer only those files that are not on the ignore list
			vfs::tReadableFile* rfile = vfs::tReadableFile::cast(it.value());
			if(!rfile)
			{
				continue;
			}
			vfs::size_t fsize = rfile->getSize();
			fileListTotalBytes += (long)fsize;
			if( (fsize>0) && rfile->openRead())
			{
				std::vector<vfs::Byte> data(fsize,0);
				rfile->read(&data[0], fsize);
				rfile->close();
				fileList.AddFile(
					vfs::String::as_utf8(valid_path()).c_str(), &data[0], fsize);
			}
		}
	}	
}

void start_server (void)
{
	if(!is_server)
	{	
		f_rec_num(1,blank);//wipe clean
				
		// ----------------------------
		// Read from ja2_mp.ini
		// ----------------------------

		CIniReader iniReader(JA2MP_INI_FILENAME);	// Wird nur für Strings gebraucht
		strncpy(cServerName, iniReader.ReadString(JA2MP_INI_INITIAL_SECTION, JA2MP_SERVER_NAME, "My JA2 Server"), 30 );				
		strncpy(gKitBag, iniReader.ReadString(JA2MP_INI_INITIAL_SECTION,JA2MP_KIT_BAG, ""), 100);
		strncpy(gAdminPassword, iniReader.ReadString(JA2MP_INI_INITIAL_SECTION, JA2MP_ADMIN_PASSWORD, ""), 63);
		
		vfs::PropertyContainer props;
		props.initFromIniFile(JA2MP_INI_FILENAME);
		UINT16 serverPort = (UINT16)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_SERVER_PORT, 60005);		
		UINT8 maxClients = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION,JA2MP_MAX_CLIENTS, 4);										
		UINT8 sameMercAllowed = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION,JA2MP_SAME_MERC, 1);
		UINT8 civEnabled = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION,JA2MP_CIV_ENABLED, 0);
		UINT8 gameType = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION,JA2MP_GAME_MODE, 0);
		UINT8 difficultyLevel = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION,JA2MP_DIFFICULT_LEVEL, 3);
		UINT8 skillTraits = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION,JA2MP_NEW_TRAITS, 0);
		UINT8 randomMercs = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION,JA2MP_RANDOM_MERCS, 0);
		UINT8 randomStartingEdge = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_RANDOM_EDGES, 0);		
		UINT8 damageSelection = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_DAMAGE_MULTIPLIER, 1);
		UINT8 maxEnemiesEnabled = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_OVERRIDE_MAX_AI, 0);
		UINT8 syncGameDirectory = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_SYNC_CLIENTS_MP_DIR, 1);
		UINT8 reportHiredMerc = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_REPORT_NAME, 1);
		UINT8 startingCashSelection = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_STARTING_BALANCE, 25000);
		UINT8 timeTurnsSelection = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_TIMED_TURN_SECS_PER_TICK, 2);
		UINT8 disableBobbyRay = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_DISABLE_BOBBY_RAYS, 0);
		UINT8 maxMercs = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_MAX_MERCS, 6);
		UINT8 timeSelection = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_TIME, 1);
		UINT8 inventoryAttachment = (UINT8)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_ALLOW_CUSTOM_NIV, 0);

		// ----------------------------
		// Save to global values
		// ----------------------------		
		gMaxClients = maxClients;
		gMaxEnemiesEnabled = 0;
		gDisableMorale = 0;
		gSyncGameDirectory = syncGameDirectory;										
		gReportHiredMerc = reportHiredMerc;			
		gDisableBobbyRay = disableBobbyRay;
		gDisableMercEquipment = 0;		// Disable AIM and MERC equipment				
		gMaxMercs = maxMercs;		
		gGameType = gameType;
		gDifficultyLevel = difficultyLevel + 1;
		gSkillTraits = skillTraits;
		gEnemyEnabled = 0;
		gCreatureEnabled = 0;
		gMilitiaEnabled = 0;
		gCivEnabled = 0;
		gRandomMercs = randomMercs;
		gRandomStartingEdge = randomStartingEdge;
		gWeaponReadyBonus = 0;
		gDisableSpectatorMode = 0;
		gInventoryAttachment = inventoryAttachment;

		if (gRandomStartingEdge)
		{
			// create random starting edges
			int spawns[5] = { 0 , 1 , 2 , 3, 9 };	// 9 == Center
			
			// Randomize spawns
			rSortArray(spawns,5);
			memcpy(client_edges,spawns,sizeof(int)*5);
		}

		if (gRandomMercs)
		{
			// randomly sort team indexes to give client
			// one of four random merc teams
			rSortArray(client_mercteam,4);
			mpTeams.HandleServerStarted();
		}

		if(gGameType == MP_TYPE_COOP)//only enable ai during coop
		{
			gEnemyEnabled = 1;				// always enable enemies in co-op
			gMilitiaEnabled = 0;			// always disable militia
			gCivEnabled = civEnabled;
			gMaxEnemiesEnabled = maxEnemiesEnabled;				
		}

		// random_mercs implies same_merc
		gSameMercAllowed = gRandomMercs ? 1 : sameMercAllowed;
				
		switch (damageSelection)
		{
			case 0:	// Very Low
				gDamageMultiplier = 0.2f;
				break;
			case 1:	// Low
				gDamageMultiplier = 0.7f;
				break;
			case 2:	// Normal
				gDamageMultiplier = 1.0f;
				break;
		}									

		switch (timeTurnsSelection)
		{
			case 0:		// Never
				gSecondsPerTick = 0;
				break;
			case 1:		// Slow
				gSecondsPerTick = 5;
				break;
			case 2:		// Medium
				gSecondsPerTick = 100;
				break;
			case 3:		// Fast
				gSecondsPerTick = 400;
				break;
		}

		switch (startingCashSelection)
		{
			case 0:	// Low
				gStartingCash = 5000;
				break;
			case 1:	// Medium
				gStartingCash = 50000;
				break;
			case 2:	// High
				gStartingCash = 100000;
				break;
			case 3:	// Unlimited
				gStartingCash = 999999999;
				break;
		}
		
		switch (timeSelection)
		{
			case 0:	// Morning
				gStartingTime = 7.00f;
				break;
			case 1:	// Afternoon
				gStartingTime = 13.00f;
				break;
			case 2:	// Night
				gStartingTime = 2.00;
				break;
		}
					
		//**********************

		ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, MPServerMessage[0] );

		server=CreateSdlNetPeer();
		
		// WANNE: Set higher timeout than default (30 seconds)
		server->SetTimeout(120000);	// 120 Seconds


		SdlNetEndpoint sd(serverPort,0);
		bool b = server->Start(gMaxClients, sd);

		server->SetMaximumIncomingConnections((gMaxClients));

		//RPC's
		REGISTER_SDLNET_MESSAGE(server, sendPATH);
		REGISTER_SDLNET_MESSAGE(server, sendDOWNLOADSTATUS);
		REGISTER_SDLNET_MESSAGE(server, sendSTANCE);
		REGISTER_SDLNET_MESSAGE(server, sendDIR);
		REGISTER_SDLNET_MESSAGE(server, sendFIRE);
		REGISTER_SDLNET_MESSAGE(server, sendHIT);
		REGISTER_SDLNET_MESSAGE(server, sendHIRE);
		REGISTER_SDLNET_MESSAGE(server, sendDISMISS);
		REGISTER_SDLNET_MESSAGE(server, sendguiPOS);
		REGISTER_SDLNET_MESSAGE(server, sendguiDIR);
		REGISTER_SDLNET_MESSAGE(server, sendEndTurn);
		REGISTER_SDLNET_MESSAGE(server, sendAI);
		REGISTER_SDLNET_MESSAGE(server, sendSTOP);
		REGISTER_SDLNET_MESSAGE(server, sendINTERRUPT);
		REGISTER_SDLNET_MESSAGE(server, sendREADY);
		REGISTER_SDLNET_MESSAGE(server, sendGUI);
		REGISTER_SDLNET_MESSAGE(server, sendBULLET);
		REGISTER_SDLNET_MESSAGE(server, sendGRENADE);
		REGISTER_SDLNET_MESSAGE(server, sendGRENADERESULT);
		REGISTER_SDLNET_MESSAGE(server, sendPLANTEXPLOSIVE);
		REGISTER_SDLNET_MESSAGE(server, sendDETONATEEXPLOSIVE);
		REGISTER_SDLNET_MESSAGE(server, sendDISARMEXPLOSIVE);
		REGISTER_SDLNET_MESSAGE(server, sendSPREADEFFECT);
		REGISTER_SDLNET_MESSAGE(server, sendNEWSMOKEEFFECT);
		REGISTER_SDLNET_MESSAGE(server, sendEXPLOSIONDAMAGE);
		REGISTER_SDLNET_MESSAGE(server, requestSETTINGS);
		REGISTER_SDLNET_MESSAGE(server, requestFILE_TRANSFER_SETTINGS);
		REGISTER_SDLNET_MESSAGE(server, sendSTATE);
		REGISTER_SDLNET_MESSAGE(server, sendDEATH);
		REGISTER_SDLNET_MESSAGE(server, sendhitSTRUCT);
		REGISTER_SDLNET_MESSAGE(server, sendhitWINDOW);
		REGISTER_SDLNET_MESSAGE(server, sendMISS);
		REGISTER_SDLNET_MESSAGE(server, updatenetworksoldier);
		REGISTER_SDLNET_MESSAGE(server, Snull_team);
		REGISTER_SDLNET_MESSAGE(server, sendFIREW);
		REGISTER_SDLNET_MESSAGE(server, sendDOOR);
		REGISTER_SDLNET_MESSAGE(server, endINTERRUPT);
		REGISTER_SDLNET_MESSAGE(server, adminCmd);
		REGISTER_SDLNET_MESSAGE(server, sendREAL);
		REGISTER_SDLNET_MESSAGE(server, startCOMBAT);
		REGISTER_SDLNET_MESSAGE(server, sendWIPE);
		REGISTER_SDLNET_MESSAGE(server, sendHEAL);
		REGISTER_SDLNET_MESSAGE(server, sendEDGECHANGE);
		REGISTER_SDLNET_MESSAGE(server, sendTEAMCHANGE);
		REGISTER_SDLNET_MESSAGE(server, sendGAMEOVER);
		REGISTER_SDLNET_MESSAGE(server, sendCHATMSG);
		REGISTER_SDLNET_MESSAGE(server, receiveSETID);

		if (b)
		{
			ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, MPServerMessage[1]);			
			is_server = true;

			// WANNE: FILE TRANSFER
			server->AttachFileTransfer(fltServer);
			fltServer.SetCallback(&serverFileListProgress);

			fileListTotalBytes=0;
			if (gSyncGameDirectory == 1)
			{
				AddFilesToSendList();
			}

			connect_client();//connect client to server
		}
		else
		{
			ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, MPServerMessage[4]);
		}
	}
	else
	{
		ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, MPServerMessage[3]);
	}
}


// recieve and process server info packets

void server_packet ( void )
{
	
	SdlNetEvent* p;

	if (is_server)
	{

	p = server->Poll();

	while(p)
	{
			//continue; // Didn't get any packets

		// We got a packet, get the identifier with our handy function
		SpacketIdentifier = SGetPacketIdentifier(p);
		//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"packet recieved");
		// Check if this is a network message packet
		switch (SpacketIdentifier)
		{
			case SDLNET_DISCONNECTION_NOTIFICATION://client disconnected purposefullly
				// Connection lost normally
				ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"SDLNET_DISCONNECTION_NOTIFICATION");
				HandleDisconnect(p->connection);//clear record
				break;
			case SDLNET_CONNECTION_ATTEMPT_FAILED:
				ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"SDLNET_CONNECTION_ATTEMPT_FAILED");
				break;
			case SDLNET_NO_FREE_INCOMING_CONNECTIONS:
				// Sorry, the server is full.  I don't do anything here but
				// A real app should tell the user
				ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"SDLNET_NO_FREE_INCOMING_CONNECTIONS");
				break;
			case SDLNET_CONNECTION_LOST:
				// Couldn't deliver a reliable packet - i.e. the other system was abnormally
				// terminated
				ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"SDLNET_CONNECTION_LOST");//client dropped
				HandleDisconnect(p->connection);//clear record
				break;
			case SDLNET_CONNECTION_ACCEPTED:
				// This tells the client they have connected
				ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"SDLNET_CONNECTION_ACCEPTED");
				break;
			case SDLNET_NEW_INCOMING_CONNECTION:
				//tells server client has connected
				#ifdef JA2BETAVERSION
					ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"SDLNET_NEW_INCOMING_CONNECTION");
				#endif
				// make sure they can connect
				CheckIncomingConnection(p);
				//send_settings();//send off server set settings

				break;
			default:
				#ifdef JA2BETAVERSION	
					ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"** a packet has been recieved for which i dont know what to do... **");
				#endif
				break;
		}


		// We're done with the packet, get more :)
		server->Release(p);
		p = server->Poll();
	}
	}
}
unsigned char SGetPacketIdentifier(SdlNetEvent *p)
{
	return !p || p->size == 0 ? 255 : p->data[0];
}

void server_disconnect (void)
{
	if(is_server)
	{
		server->DetachFileTransfer(fltServer);
	server->Shutdown(300);
	is_server = false;
	fileList.Clear();
	// We're done with the network
	DestroySdlNetPeer(server);
	ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, MPServerMessage[6]);
	}
	else
	{
	ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, MPServerMessage[7]);
	}
}
