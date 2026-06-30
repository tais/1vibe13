	#include "builddefines.h"
	#include "sgp_bounded_string.h"
	#include "Bullets.h"
	#include <stdio.h>
	#include <string.h>
	#include "WCheck.h"
	#include "stdlib.h"
	#include "DEBUG.H"
	#include "math.h"
	#include "worlddef.h"
	#include "worldman.h"
	#include "renderworld.h"
	#include "Assignments.h"
	#include "Soldier Control.h"
	#include "Animation Control.h"
	#include "Animation Data.h"
	#include "Isometric Utils.h"
	#include "Event Pump.h"
	#include "Timer Control.h"
	#include "Render Fun.h" 
	#include "Render Dirty.h"
	#include "mousesystem.h"
	#include "Interface.h"
	#include "sysutil.h"
	#include "FileMan.h"
	#include "Points.h"
	#include "random.h"
	#include "ai.h"
	#include "Interactive Tiles.h"
	#include "Soldier Ani.h"
	#include "english.h"
	#include "Overhead.h"
	#include "Soldier Profile.h"
	#include "Game Clock.h"
	#include "Soldier Create.h"
	#include "Merc Hiring.h"
	#include "Game Event Hook.h"
	#include "message.h"
	#include "strategicmap.h"
	#include "strategic.h"
	#include "Items.h"
	#include "Soldier Add.h"
	#include "history.h"
	#include "Squads.h"
	#include "Strategic Merc Handler.h"
	#include "Dialogue Control.h"
	#include "Map Screen Interface.h"
	#include "Map Screen Interface Map.h"
	#include "screenids.h"
	#include "jascreens.h"
	#include "Text.h"
	#include "Merc Contract.h"
	#include "LaptopSave.h"
	#include "personnel.h"
	#include "Auto Resolve.h"
	#include "Map Screen Interface Bottom.h"
	#include "Quests.h"
	#include "GameSettings.h"
	#include "mercs.h"
	#include "Handle Doors.h"
	#include "Campaign Types.h"
	#include "Tactical Save.h"
	#include "BobbyRMailOrder.h"
	#include "finances.h"
	#include "TeamTurns.h"
	#include "gameloop.h"
	#include "Options Screen.h"
#include "physics.h"
#include "Explosion Control.h"
#include "SmokeEffects.h"
#include "MPChatScreen.h"
#include "sgp_logger.h"

#include "MessageIdentifiers.h"
#include "RakNetworkFactory.h"
#include "RakPeerInterface.h"
#include "RakNetStatistics.h"
#include "RakNetTypes.h"
#include "RakSleep.h"

#include "FileListTransfer.h"
#include "FileListTransferCBInterface.h"
#include "FileOperations.h"
#include "SuperFastHash.h"
#include "RakAssert.h"
#include "IncrementalReadInterface.h"

#include "BitStream.h"
#include <assert.h>
#include <cstdio>
#include <cstring>
#include <stdlib.h>
#include "Music Control.h"
#include "Map Edgepoints.h"

#include "fresh_header.h"
#include "network.h"
#include "opplist.h"

#include "Tactical Placement GUI.h"
#include "PreBattle Interface.h"
#include "mapscreen.h"

#include "MessageBoxScreen.h"

#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_init.h>
#include <vfs/Core/vfs_profile.h>
#include <vfs/Core/vfs_file_raii.h>
#include <vfs/Core/vfs_os_functions.h>
#include <vfs/Core/File/vfs_file.h>
#include "transfer_rules.h"

#include "Keys.h"
#include "types.h"
#include "connect.h"
#include "message.h"
#include "Event Pump.h"
#include "Soldier Init List.h"
#include "Overhead.h"
#include "Weapons.h"
#include "Merc Hiring.h"
#include "Soldier Profile.h"
#include "environment.h"
#include "lighting.h"
#include "laptop.h"
#include "Interface Panels.h"
#include "Game Init.h"
#include "Debug Control.h"
#include "MPConnectScreen.h"
#include "INIReader.h"
#include "Map Screen Interface Border.h"

extern CHAR16 gzFileTransferDirectory[100];

// WANNE: FILE TRANSFER
BOOLEAN		fClientReceivedAllFiles;
STRING512	client_executableDir;
STRING512	client_fileTransferDirectoryPath;	// the clients file transfer directory absolut path
STRING512	server_fileTransferDirectoryPath;	// the server file transfer directory absolut path
INT16		fileTransferProgress = 0;
INT16		serverSyncClientsDirectory = 0;

unsigned char GetPacketIdentifier(Packet *p);
unsigned char packetIdentifier;

// OJW - 20090405
STRING512	gCurrentTransferFilename;
INT32		gCurrentTransferBytes = 0;
INT32		gTotalTransferBytes = 0;

extern BOOLEAN gfTemporaryDisablingOfLoadPendingFlag;

extern INT8 SquadMovementGroups[ ];
RakPeerInterface *client;
// WANNE: FILE TRANSFER
FileListTransfer fltClient;	// flt2

// --- MP wire-hardening helpers (PR-1) -------------------------------------
// Every recieveX/sendX RPC handler receives a heap buffer sized to the ACTUAL
// wire payload (payloadLen+4), then (Struct*)casts and reads sizeof(Struct).
// A short/malformed frame is therefore a heap over-read. RPC_REQUIRE_BYTES is
// the central length guard: bail before the cast if the payload is too small.
// numberOfBitsOfData is unsigned (BitSize_t); cast sizeof to long so the
// comparison is well-defined.
#define RPC_REQUIRE_BYTES(p,T) do{ if ( ((long)(((p)->numberOfBitsOfData)+7)/8) < (long)sizeof(T) ) return; }while(0)

// Resolve a wire soldier id to a live merc pointer, or NULL. On a raw RPC
// memcpy the SoldierID clamping constructor never runs, so the index is
// attacker-controlled 0..65535; this is the only safe way to deref MercPtrs[]
// from wire data. (MercPtrs is [TOTAL_SOLDIERS].)
static SOLDIERTYPE* SafeMerc(UINT16 i)
{
	return ( i < TOTAL_SOLDIERS && MercPtrs[i] && MercPtrs[i]->bActive ) ? MercPtrs[i] : NULL;
}

// gAnimControl[] / EVENT_InitNewSoldierAnim() are indexed by animation state;
// any wire value >= NUMANIMATIONSTATES is an OOB read / invalid anim init.
static BOOLEAN IsValidAnimState(UINT16 s)
{
	return s < NUMANIMATIONSTATES;
}
// --------------------------------------------------------------------------

char *ReplaceCharactersInString_Client(char *str, char *orig, char *rep)
{
	static char buffer[4096];
	char *p;

	if(!(p = strstr(str, orig)))  // Is 'orig' even in 'str'?
		return str;

	strncpy(buffer, str, p-str); // Copy characters from 'str' start to 'orig' st$
	buffer[p-str] = '\0';

	sprintf(buffer+(p-str), "%s%s", rep, p+strlen(orig));

	return buffer;
}

class ClientTransferCB : public FileListTransferCBInterface
{
	public:
		// This method gets called for each file when it is completly received by the client.
		// Now the file will be saved on the client
		bool OnFile(OnFileStruct *onFileStruct)
		{

			if(!transferRules)
			{
				transferRules = new CTransferRules();
				transferRules->initFromTxtFile("transfer_rules.txt");
			}
			// Get the directory path of the file and output it to the user!
			char* targetFileName = ExtractFilename(onFileStruct->fileName);
			//ScreenMsg( FONT_BCOLOR_ORANGE, MSG_CHAT, MPClientMessage[58], targetFileName);

			vfs::Path fileName(onFileStruct->fileName);
			vfs::String::str_t const& valid_str = fileName.c_wcs();
			vfs::String::size_t pos = valid_str.find(L":");
			if(pos != vfs::String::str_t::npos)
			{
				// absolute path?? these are invalid and the server is not supposed to send us such paths
				// potentialy malicious server -> output error
				return false;
			}
			if(valid_str.substr(0,2) == vfs::Const::DOTDOT())
			{
				// trying to break out from the profile?
				// potentialy malicious server -> output error
				return false;
			}
			if(transferRules && (transferRules->applyRule(valid_str) == CTransferRules::DENY))
			{
				// sent file was on our ignore list
				// it may be OK that the server's list and the clients' lists diverge
				// send message to server that we cannot accept this file
				return false;
			}

			strcpy(gCurrentTransferFilename,onFileStruct->fileName);
			
			try
			{
				vfs::COpenWriteFile wfile(fileName,true,true);
				wfile->write(onFileStruct->fileData,onFileStruct->finalDataLength);
			}
			catch(vfs::Exception& ex)
			{
				SGP_ERROR(ex.what());
				ScreenMsg( FONT_BCOLOR_BLUE, MSG_CHAT, MPClientMessage[70], targetFileName);				
			}

			/*
			// Make sure it worked
			unsigned int hash1 = SuperFastHashFile(fileToSend);
			if (RakNet::BitStream::DoEndianSwap())
				RakNet::BitStream::ReverseBytesInPlace((unsigned char*) &hash1, sizeof(hash1));
			unsigned int hash2 = SuperFastHashFile(targetFileName);
			if (RakNet::BitStream::DoEndianSwap())
				RakNet::BitStream::ReverseBytesInPlace((unsigned char*) &hash2, sizeof(hash2));
			RakAssert(hash1==hash2);
			*/

			//ScreenMsg( FONT_BCOLOR_ORANGE, MSG_MPSYSTEM, L"Saved file local in %S", file);

			// Return true to have RakNet delete the memory allocated to hold this file.
			// False if you hold onto the memory, and plan to delete it yourself later
			return true;
		}

		// WANNE: FILE TRANSFER: This method gets called each periodically
		virtual void OnFileProgress(OnFileStruct *onFileStruct,unsigned int partCount,unsigned int partTotal,unsigned int partLength, char *firstDataChunk)
		{
			static UINT32 iNextTransferProgressUpdateTime;

			gCurrentTransferBytes += (INT32)(onFileStruct->finalDataLength * (float)(1.0f/(float)partTotal));

			if (guiBaseJA2NoPauseClock >= iNextTransferProgressUpdateTime)
			{
				char *relativeFname = ReplaceCharactersInString_Client(onFileStruct->fileName, server_fileTransferDirectoryPath,"");
				strcpy(gCurrentTransferFilename,relativeFname);

				iNextTransferProgressUpdateTime = guiBaseJA2NoPauseClock + 100;
				// update all clients of our file transfer progress
				INT8 currentProgress = (INT8)(100.0f * (float)((float)gCurrentTransferBytes/(float)gTotalTransferBytes));

				SetConnectScreenSubMessageA(gCurrentTransferFilename); // setting this also causes connect screen to refresh

				if (recieved_settings && CLIENT_NUM > 0 && CLIENT_NUM <= 4)
				{
					client_progress[CLIENT_NUM-1] = currentProgress;
					client_downloading[CLIENT_NUM-1] = 1;
					fDrawCharacterList = true;

					progress_struct prog;
					prog.client_num = CLIENT_NUM;
					prog.downloading = 1;
					prog.progress = currentProgress;

					client->RPC("sendDOWNLOADSTATUS",(const char*)&prog, (int)sizeof(progress_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
				}
			}

			/*INT16 currentProgress = 100 * partCount/partTotal;
			INT16 fileTransferProgressNew = 0;

			// WANNE: FILE TRANSFER: Only output (0, 25, 50, 75) percentage
			if (currentProgress < 25)
				fileTransferProgressNew = 0;
			else if (currentProgress < 50)
				fileTransferProgressNew = 25;
			else if (currentProgress < 75)
				fileTransferProgressNew = 50;
			else
				fileTransferProgressNew = 75;

			if (fileTransferProgressNew != fileTransferProgress)
			{
				char* targetFileName = ExtractFilename(onFileStruct->fileName);
				fileTransferProgress = fileTransferProgressNew;
				ScreenMsg( FONT_BCOLOR_BROWN, MSG_MPSYSTEM, MPClientMessage[59], targetFileName, fileTransferProgress);
			}*/


			//ScreenMsg( FONT_BLUE, MSG_MPSYSTEM, L"(%i%%) %S", 100*partCount/partTotal, onFileStruct->fileName);
			//ScreenMsg( FONT_BLUE, MSG_MPSYSTEM, L"%i (%i%%) %i/%i %S %ib->%ib / %ib->%ib\n", onFileStruct->setID, 100*partCount/partTotal, onFileStruct->fileIndex+1, onFileStruct->setCount, onFileStruct->fileName, onFileStruct->compressedTransmissionLength, onFileStruct->finalDataLength, onFileStruct->setTotalCompressedTransmissionLength, onFileStruct->setTotalFinalLength, firstDataChunk);

			//printf("%i (%i%%) %i/%i %s %ib->%ib / %ib->%ib\n", onFileStruct->setID, 100*partCount/partTotal, onFileStruct->fileIndex+1, onFileStruct->setCount, onFileStruct->fileName, onFileStruct->compressedTransmissionLength, onFileStruct->finalDataLength, onFileStruct->setTotalCompressedTransmissionLength, onFileStruct->setTotalFinalLength, firstDataChunk);
		}

		virtual bool OnDownloadComplete(void)
		{
			//printf("Download complete.\n");
			if (serverSyncClientsDirectory)
			{
				ScreenMsg( FONT_RED, MSG_MPSYSTEM, MPClientMessage[60]);
				SetConnectScreenSubMessageW(MPClientMessage[60]); // setting this also causes connect screen to refresh

				//fileTransferProgress = 0;
				if(transferRules)
				{
					delete transferRules;
					transferRules = NULL;
				}

				if (recieved_settings && CLIENT_NUM > 0 && CLIENT_NUM <= 4)
				{
					// notify ourselves
					client_progress[CLIENT_NUM-1] = 0;
					client_downloading[CLIENT_NUM-1] = 0;
					// notify others
					progress_struct prog;
					prog.client_num = CLIENT_NUM;
					prog.downloading = 0; // notify clients we have finished
					prog.progress = 0;

					fDrawCharacterList = true;

					client->RPC("sendDOWNLOADSTATUS",(const char*)&prog, (int)sizeof(progress_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
				}
			}

			// Set a flag that we have received all the files. This is used that when another client connects we do not want to receive the files again!
			fClientReceivedAllFiles = TRUE;

			// Returning false automatically deallocates the automatically allocated handler that was created by DirectoryDeltaTransfer
			return false;
		}

	private:
		char *ExtractFilename(char *pathname) 
		{
			char *s;

			if ((s=strrchr(pathname, '\\')) != NULL) s++;
			else if ((s=strrchr(pathname, '/')) != NULL) s++;
			else if ((s=strrchr(pathname, ':')) != NULL) s++;
			else s = pathname;
			return s;
		}

		// client has an ignore list too, as the server doesn't have to play by the rules
		static CTransferRules* transferRules;

} transferCallback;


CTransferRules* ClientTransferCB::transferRules = NULL;


ClientTransferCB transferCBClient;

// OJW - 20081222
player_stats gMPPlayerStats[5];

// Arbiter-driven banner state: when the server grants an enemy interrupt against
// us, our turn is paused but our LOCAL turn machine (StartPlayerTeamTurn, the
// per-frame turn timer, a late EndInterrupt) keeps trying to re-assert the green
// "PLAYER'S TURN" bar -- which made the GUI lie that we could still act (the bug
// only showed on the FIRST interrupt of a turn, a pure ordering race). This holds
// the interrupting team (0 = not interrupted); AddTopMessage forces any green bar
// to the enemy-interrupt bar while it is non-zero. Cleared on resume_turn / new turn.
int gMpEnemyInterruptTeam = 0;

// OJW - 20090503 - get rid of compile error
#pragma pack (1)

typedef struct
{
	UINT8		ubProfileID;
	int		team;
	BOOLEAN fCopyProfileItemsOver;
	INT8 bTeam;

} send_hire_struct;

typedef struct
{
	UINT16	ubProfileID;

} send_dismiss_struct;

typedef struct
{
	SoldierID	usSoldierID;
	FLOAT		dNewXPos;
	FLOAT		dNewYPos;

} gui_pos;

typedef struct
{
	SoldierID	usSoldierID;
	INT16		usNewDirection;

} gui_dir;

typedef struct
{	
	UINT8 tsnetbTeam;
	UINT8 tsubNextTeam;
} turn_struct;

// PORTABLE WIRE FORMAT (H17): AI_STRUCT (SOLDIERCREATE_STRUCT + OBJECTTYPE slot[55]) is gone.
// It embedded std::vector/std::list and a SOLDIERTYPE* whose layout is STL/ABI-specific and
// which leaked sender heap pointers over the wire. See SerializeAI/DeserializeAI below.

// PORTABLE WIRE FORMAT (H16): the old netb_struct shipped the WHOLE BULLET struct, which
// embeds heap pointers (LEVELNODE* pNodes[60], SOLDIERTYPE* pFirer, ANITILE* x2) and is not
// pack(1) -- so its size/offsets differ 32<->64-bit and by ABI, AND it leaked sender heap
// addresses over the wire. The receiver only ever reconstructs from a handful of scalars,
// so carry exactly those in an in-memory bullet_wire and serialize it field-by-field below.
struct bullet_wire
{
	INT32   iBullet;
	UINT16  ubFirerID;            // SoldierID.i (already MP-encoded by the sender)
	UINT16  usFlags;
	UINT16  usHandItem;
	INT8    bStartCubesAboveLevelZ;
	INT8    bEndCubesAboveLevelZ;
	UINT8   fCheckForRoof;
	UINT8   fAimed;
	UINT16  ubItemStatus;
	INT16   sHitBy;
	INT32   sTargetGridNo;
	INT32   iImpact;
	INT32   iRange;
	INT32   iDistanceLimit;
	INT64   qCurrX;              // FIXEDPT
	INT64   qCurrY;
	INT64   qCurrZ;
	INT64   qIncrX;
	INT64   qIncrY;
	INT64   qIncrZ;
	double  ddHorizAngle;
};
// SerializeBullet byte budget: 4 +2+2+2 +1*4 +2+2 +4*4 +8*6 +8 = 90
#define BULLET_WIRE_BYTES 90

typedef struct
{
	SoldierID ubID;
	INT8 bTeam;
	UINT16 gubOutOfTurnPersons;
	UINT16 gubOutOfTurnOrder[MAXMERCS];
	BOOLEAN fMarkInterruptOccurred;
	SoldierID Interrupted;
} INT_STRUCT;

typedef struct
{
	UINT8 client_num;
	bool status;
	UINT8 ready_stage;
} ready_struct;

typedef struct
{
	INT32 remote_id;
	INT32 local_id;

}bullets_table;

typedef struct
	{
		UINT8 ubResult;
	}kickR;

typedef struct
{
	UINT8 client_num;
	BOOLEAN bToAll;
	// PORTABLE WIRE FORMAT (H15/M4): was `CHAR16 msg[512]` == `wchar_t[512]`, which is
	// 2048B on mac/linux (4B wchar) but 1024B on Windows (2B wchar) -- a Windows client and
	// a Linux peer could not exchange chat at all. Carry the text as fixed-width UTF-16LE
	// (uint16_t) and convert at the send/receive boundary (MPTextToWire / MPTextFromWire).
	uint16_t msg[512];
} chat_msg;
// 1 (client_num) + 1 (bToAll) + 512*2 (msg), packed -> 1026 bytes on every target.
static_assert(sizeof(chat_msg) == 1026, "chat_msg wire size changed");

// OJW - 20091002 - explosions
typedef struct
{
	float dLifeSpan;
	float dX;
	float dY;
	float dZ;
	float dForceX;
	float dForceY;
	float dForceZ;
	UINT32 sTargetGridNo;
	SoldierID ubID;
	UINT8 ubActionCode;
	UINT32 uiActionData;
	UINT16 usItem;
	INT32 RealObjectID; // the local ID on the initiating client
	bool IsThrownGrenade; // could be mortar
	UINT32 uiPreRandomIndex;
	// M12 - short-term inventory/ammo replication: carry the real thrown-object
	// state instead of reconstructing it from item defaults on the receiver.
	INT16 sItemStatus;       // (*pGameObj)[0]->data.objectStatus (status %)
	UINT16 usShotsLeft;      // (*pGameObj)[0]->data.gun.ubGunShotsLeft (loaded launcher shells)
	UINT8 ubNumberOfObjects; // stack count thrown / consumed
	// L1 - sequence number so a dropped/duplicated/out-of-order grenade event
	// cannot silently drift guiPreRandomIndex on the receiver.
	UINT32 uiGrenadeEventSeq;
} physics_object;

typedef struct
{
	float dX;
	float dY;
	float dZ;
	INT32 sGridNo;
	bool bWasDud;
	SoldierID ubOwnerID;
	INT32 RealObjectID; // the local ID on the initiating client
	UINT32 uiPreRandomIndex; // send out our current pre-random index
} grenade_result;

typedef struct
{
	UINT32 sGridNo;
	SoldierID ubID;
	UINT16 usItem;
	UINT8 ubItemStatus;
	UINT32 uiWorldIndex; // the local World Index of this bomb on its creators client
	UINT16 usFlags;
	UINT8 ubLevel;
	INT8 bDetonatorType;
	INT8 bDelayFreq;
} explosive_obj;

typedef struct
{
	SoldierID ubID;
	UINT32 uiWorldItemIndex;
	UINT8 ubMPTeamIndex;
	UINT32 uiPreRandomIndex; // send out our current pre-random index
} detonate_struct;

typedef struct
{
	UINT32 uiWorldItemIndex;
	UINT8 ubMPTeamIndex;
	SoldierID ubID;
	UINT32 sGridNo;
	UINT32 uiPreRandomIndex; // send out our current pre-random index
} disarm_struct;

typedef struct
{
	INT32 sGridNo;
	UINT8 ubRadius;
	UINT16 usItem;
	SoldierID ubOwner;
	BOOLEAN fSubsequent;
	INT8 bLevel;
	INT32 iSmokeEffectID;
	UINT32 uiPreRandomIndex;
} spreadeffect_struct;

typedef struct
{
	UINT8		ubDamageFunc; // 1 - gas damage , 2 - explosive damage
	SoldierID	ubSoldierID;
	UINT16		usExplosiveClassID;
	INT16		sSubsequent;
	BOOL			fRecompileMovementCosts;
	INT16		sWoundAmt;
	INT16		sBreathAmt;
	SoldierID	ubAttackerID;
	UINT16		usItem;
	INT32		sBombGridNo;
	UINT32		uiDist;
	UINT32		uiPreRandomIndex;
} explosiondamage_struct;

// ---------------------------------------------------------------------------
// PORTABLE WIRE FORMAT helpers (H15/H16/H17/L6)
//
// The on-wire protocol must be byte-identical regardless of platform/ABI, so text and
// multi-byte scalars are serialized field-by-field at fixed widths rather than memcpy'd.
//
// Text is carried as fixed-width UTF-16LE (uint16_t code units), NOT CHAR16/wchar_t whose
// width changes per platform. Engine string buffers are wchar_t, so convert at the
// boundary: widen on receive, narrow on send. Both helpers always NUL-terminate.
// ---------------------------------------------------------------------------
static void MPTextToWire( uint16_t* dst, size_t dstCount, const wchar_t* src )
{
	if ( dstCount == 0 )
		return;
	size_t i = 0;
	for ( ; src && src[i] != L'\0' && i + 1 < dstCount; ++i )
		dst[i] = (uint16_t)src[i];	// truncate each code unit to 16 bits (UTF-16LE)
	dst[i] = 0;
	// zero the tail so the wire buffer is deterministic (no sender heap leakage)
	for ( size_t j = i + 1; j < dstCount; ++j )
		dst[j] = 0;
}

static void MPTextFromWire( wchar_t* dst, size_t dstCount, const uint16_t* src )
{
	if ( dstCount == 0 )
		return;
	size_t i = 0;
	for ( ; src && i + 1 < dstCount && src[i] != 0; ++i )
		dst[i] = (wchar_t)src[i];
	dst[i] = L'\0';
}

// ---------------------------------------------------------------------------
// Little-endian byte cursor for explicit field-by-field (de)serialization. All MP
// targets are little-endian today (netshim.cpp:DoEndianSwap==false); writing/reading
// byte-by-byte in LE order keeps the wire identical regardless of host endianness or
// struct padding. The reader is bounds-checked against the actual payload length so a
// short/malformed frame can never over-read past the heap buffer.
// ---------------------------------------------------------------------------
struct MPWireWriter
{
	uint8_t* p;
	size_t   cap;
	size_t   off;
	MPWireWriter( uint8_t* buf, size_t capacity ) : p(buf), cap(capacity), off(0) {}
	void put8 ( uint8_t  v ) { if ( off + 1 <= cap ) { p[off] = v; } off += 1; }
	void put16( uint16_t v ) { if ( off + 2 <= cap ) { p[off] = (uint8_t)(v); p[off+1] = (uint8_t)(v>>8); } off += 2; }
	void put32( uint32_t v ) { if ( off + 4 <= cap ) { for (int k=0;k<4;++k) p[off+k]=(uint8_t)(v>>(8*k)); } off += 4; }
	void put64( uint64_t v ) { if ( off + 8 <= cap ) { for (int k=0;k<8;++k) p[off+k]=(uint8_t)(v>>(8*k)); } off += 8; }
	void putbytes( const void* src, size_t n ) { if ( off + n <= cap ) memcpy( p + off, src, n ); off += n; }
};

struct MPWireReader
{
	const uint8_t* p;
	size_t         len;
	size_t         off;
	bool           ok;
	MPWireReader( const void* buf, size_t length ) : p((const uint8_t*)buf), len(length), off(0), ok(true) {}
	uint8_t  get8 () { if ( off + 1 > len ) { ok = false; return 0; } uint8_t  v = p[off]; off += 1; return v; }
	uint16_t get16() { if ( off + 2 > len ) { ok = false; return 0; } uint16_t v = (uint16_t)(p[off] | (p[off+1]<<8)); off += 2; return v; }
	uint32_t get32() { if ( off + 4 > len ) { ok = false; return 0; } uint32_t v = 0; for (int k=0;k<4;++k) v |= (uint32_t)p[off+k]<<(8*k); off += 4; return v; }
	uint64_t get64() { if ( off + 8 > len ) { ok = false; return 0; } uint64_t v = 0; for (int k=0;k<8;++k) v |= (uint64_t)p[off+k]<<(8*k); off += 8; return v; }
	void getbytes( void* dst, size_t n ) { if ( off + n > len ) { ok = false; memset( dst, 0, n ); return; } memcpy( dst, p + off, n ); off += n; }
};

// ---- INT_STRUCT wire (L6) -------------------------------------------------
// The interrupt struct embedded gubOutOfTurnOrder[MAXMERCS] (~2.5KB) and shipped the WHOLE
// array every time, even though only entries [0..gubOutOfTurnPersons] are ever consumed.
// Serialize the small fixed-width header plus exactly (gubOutOfTurnPersons+1) order entries.
// Header: ubID(u16) bTeam(u8) gubOutOfTurnPersons(u16) fMarkInterruptOccurred(u8) Interrupted(u16)
#define INT_WIRE_HEADER_BYTES 8
#define INT_WIRE_MAX_BYTES    (INT_WIRE_HEADER_BYTES + MAXMERCS * 2)

// Returns the number of bytes written to dst (dst must be >= INT_WIRE_MAX_BYTES).
static int SerializeINT( uint8_t* dst, size_t cap, const INT_STRUCT& src )
{
	UINT16 persons = src.gubOutOfTurnPersons;
	if ( persons >= MAXMERCS )
		persons = MAXMERCS - 1;	// never serialize past the array
	MPWireWriter w( dst, cap );
	w.put16( src.ubID.i );
	w.put8 ( (uint8_t)src.bTeam );
	w.put16( persons );
	w.put8 ( (uint8_t)src.fMarkInterruptOccurred );
	w.put16( src.Interrupted.i );
	for ( int i = 0; i <= (int)persons; ++i )
		w.put16( src.gubOutOfTurnOrder[i] );
	return (int)w.off;
}

// Deserializes into dst. Zeroes the unused tail of gubOutOfTurnOrder so downstream loops
// over [0..gubOutOfTurnPersons] never read stale data. Returns false on a short/bad frame.
static bool DeserializeINT( INT_STRUCT& dst, const void* buf, size_t len )
{
	memset( &dst, 0, sizeof(dst) );
	MPWireReader r( buf, len );
	dst.ubID                  = (UINT16)r.get16();
	dst.bTeam                 = (INT8)r.get8();
	UINT16 persons            = r.get16();
	dst.fMarkInterruptOccurred= (BOOLEAN)r.get8();
	dst.Interrupted           = (UINT16)r.get16();
	if ( !r.ok )
		return false;
	if ( persons >= MAXMERCS )
		persons = MAXMERCS - 1;	// wire bound
	dst.gubOutOfTurnPersons = persons;
	for ( int i = 0; i <= (int)persons; ++i )
	{
		uint16_t v = r.get16();
		if ( !r.ok )
			return false;
		dst.gubOutOfTurnOrder[i] = v;
	}
	return true;
}

// ---- bullet_wire serialization (H16) --------------------------------------
// double is IEEE-754 on every target; carry its raw 64 bits in LE order.
static int SerializeBullet( uint8_t* dst, size_t cap, const bullet_wire& b )
{
	double   angle = b.ddHorizAngle;	// copy out of the (packed) struct before bit-casting
	uint64_t angleBits;
	memcpy( &angleBits, &angle, sizeof(angleBits) );
	MPWireWriter w( dst, cap );
	w.put32( (uint32_t)b.iBullet );
	w.put16( b.ubFirerID );
	w.put16( b.usFlags );
	w.put16( b.usHandItem );
	w.put8 ( (uint8_t)b.bStartCubesAboveLevelZ );
	w.put8 ( (uint8_t)b.bEndCubesAboveLevelZ );
	w.put8 ( b.fCheckForRoof );
	w.put8 ( b.fAimed );
	w.put16( b.ubItemStatus );
	w.put16( (uint16_t)b.sHitBy );
	w.put32( (uint32_t)b.sTargetGridNo );
	w.put32( (uint32_t)b.iImpact );
	w.put32( (uint32_t)b.iRange );
	w.put32( (uint32_t)b.iDistanceLimit );
	w.put64( (uint64_t)b.qCurrX );
	w.put64( (uint64_t)b.qCurrY );
	w.put64( (uint64_t)b.qCurrZ );
	w.put64( (uint64_t)b.qIncrX );
	w.put64( (uint64_t)b.qIncrY );
	w.put64( (uint64_t)b.qIncrZ );
	w.put64( angleBits );
	return (int)w.off;
}

static bool DeserializeBullet( bullet_wire& b, const void* buf, size_t len )
{
	memset( &b, 0, sizeof(b) );
	MPWireReader r( buf, len );
	b.iBullet               = (INT32)r.get32();
	b.ubFirerID             = r.get16();
	b.usFlags               = r.get16();
	b.usHandItem            = r.get16();
	b.bStartCubesAboveLevelZ= (INT8)r.get8();
	b.bEndCubesAboveLevelZ  = (INT8)r.get8();
	b.fCheckForRoof         = r.get8();
	b.fAimed                = r.get8();
	b.ubItemStatus          = r.get16();
	b.sHitBy                = (INT16)r.get16();
	b.sTargetGridNo         = (INT32)r.get32();
	b.iImpact               = (INT32)r.get32();
	b.iRange                = (INT32)r.get32();
	b.iDistanceLimit        = (INT32)r.get32();
	b.qCurrX                = (INT64)r.get64();
	b.qCurrY                = (INT64)r.get64();
	b.qCurrZ                = (INT64)r.get64();
	b.qIncrX                = (INT64)r.get64();
	b.qIncrY                = (INT64)r.get64();
	b.qIncrZ                = (INT64)r.get64();
	uint64_t angleBits      = r.get64();
	double   angle;
	memcpy( &angle, &angleBits, sizeof(angleBits) );
	b.ddHorizAngle          = angle;
	return r.ok;
}

// ---- AI_STRUCT wire (H17) -------------------------------------------------
// The old AI_STRUCT memcpy'd a SOLDIERCREATE_STRUCT (which embeds std::vector x3 in its
// Inventory, a SOLDIERTYPE* pExistingSoldier, and CHAR16 name[10]) plus 55 OBJECTTYPEs
// (each embedding a std::list). sizeof and every offset are STL/ABI-specific and the wire
// leaked sender heap pointers. The receiver only ever consumes the POD scalar prefix and
// {usItem, ubNumberOfObjects} per slot. Serialize EXACTLY those, field-by-field, fixed-width.
//
// AI_WIRE_SLOTS must match the loop counts in send_AI / recieveAI (the engine Inv has 55).
#define AI_WIRE_SLOTS 55
// header (POD scalars) + per-slot {usItem(u16), ubNumberOfObjects(u8), fFlags(u8)}
#define AI_WIRE_MAX_BYTES 1024

static int SerializeAI( uint8_t* dst, size_t cap, SOLDIERCREATE_STRUCT* s )
{
	MPWireWriter w( dst, cap );
	// --- profile / flags ---
	w.put8 ( (uint8_t)s->fStatic );
	w.put8 ( s->ubProfile );
	w.put8 ( (uint8_t)s->fPlayerMerc );
	w.put8 ( (uint8_t)s->fPlayerPlan );
	w.put8 ( (uint8_t)s->fCopyProfileItemsOver );
	// --- location ---
	w.put16( (uint16_t)s->sSectorX );
	w.put16( (uint16_t)s->sSectorY );
	w.put8 ( s->ubDirection );
	w.put32( (uint32_t)s->sInsertionGridNo );
	// --- team / body / orders ---
	w.put8 ( (uint8_t)s->bTeam );
	w.put8 ( (uint8_t)s->ubBodyType );
	w.put8 ( (uint8_t)s->bAttitude );
	w.put8 ( (uint8_t)s->bOrders );
	// --- attributes ---
	w.put8 ( (uint8_t)s->bLifeMax );
	w.put8 ( (uint8_t)s->bLife );
	w.put8 ( (uint8_t)s->bAgility );
	w.put8 ( (uint8_t)s->bDexterity );
	w.put8 ( (uint8_t)s->bExpLevel );
	w.put8 ( (uint8_t)s->bMarksmanship );
	w.put8 ( (uint8_t)s->bMedical );
	w.put8 ( (uint8_t)s->bMechanical );
	w.put8 ( (uint8_t)s->bExplosive );
	w.put8 ( (uint8_t)s->bLeadership );
	w.put8 ( (uint8_t)s->bStrength );
	w.put8 ( (uint8_t)s->bWisdom );
	w.put8 ( (uint8_t)s->bMorale );
	w.put8 ( (uint8_t)s->bAIMorale );
	// --- palettes (fixed CHAR8[30] each) ---
	w.putbytes( s->HeadPal,  sizeof(PaletteRepID) );
	w.putbytes( s->PantsPal, sizeof(PaletteRepID) );
	w.putbytes( s->VestPal,  sizeof(PaletteRepID) );
	w.putbytes( s->SkinPal,  sizeof(PaletteRepID) );
	w.putbytes( s->MiscPal,  sizeof(PaletteRepID) );
	// --- patrol waypoints (INT32 x MAXPATROLGRIDS) ---
	for ( int i = 0; i < MAXPATROLGRIDS; ++i )
		w.put32( (uint32_t)s->sPatrolGrid[i] );
	w.put8 ( (uint8_t)s->bPatrolCnt );
	// --- misc ---
	w.put8 ( (uint8_t)s->fVisible );
	// name: fixed-width UTF-16LE (NOT CHAR16/wchar_t) -- H15
	for ( int i = 0; i < 10; ++i )
		w.put16( (uint16_t)s->name[i] );
	w.put8 ( s->ubSoldierClass );
	w.put8 ( (uint8_t)s->fOnRoof );
	w.put8 ( (uint8_t)s->bSectorZ );
	// pExistingSoldier is a sender heap pointer -- NEVER serialized; receiver forces NULL.
	w.put8 ( (uint8_t)s->fUseExistingSoldier );
	w.put8 ( s->ubCivilianGroup );
	w.put8 ( (uint8_t)s->fKillSlotIfOwnerDies );
	w.put8 ( s->ubScheduleID );
	w.put8 ( (uint8_t)s->fUseGivenVehicle );
	w.put8 ( (uint8_t)s->bUseGivenVehicleID );
	w.put8 ( (uint8_t)s->fHasKeys );
	// --- inventory: only the POD scalars the receiver rebuilds from ---
	for ( int x = 0; x < AI_WIRE_SLOTS; ++x )
	{
		OBJECTTYPE& o = s->Inv[x];
		w.put16( o.usItem );
		w.put8 ( o.ubNumberOfObjects );
		w.put8 ( o.fFlags );
	}
	return (int)w.off;
}

// Per-slot inventory tuple the receiver reconstructs an OBJECTTYPE from.
struct ai_wire_slot { UINT16 usItem; UINT8 ubNumberOfObjects; UINT8 fFlags; };

// Deserialize into a POD-zeroed SOLDIERCREATE_STRUCT prefix (caller passes one that has had
// SIZEOF_SOLDIERCREATE_STRUCT_POD memset to 0) plus the slot tuples. Returns false on a
// short/bad frame.
static bool DeserializeAI( SOLDIERCREATE_STRUCT& s, ai_wire_slot* slots, const void* buf, size_t len )
{
	MPWireReader r( buf, len );
	s.fStatic              = (BOOLEAN)r.get8();
	s.ubProfile            = r.get8();
	s.fPlayerMerc          = (BOOLEAN)r.get8();
	s.fPlayerPlan          = (BOOLEAN)r.get8();
	s.fCopyProfileItemsOver= (BOOLEAN)r.get8();
	s.sSectorX             = (INT16)r.get16();
	s.sSectorY             = (INT16)r.get16();
	s.ubDirection          = r.get8();
	s.sInsertionGridNo     = (INT32)r.get32();
	s.bTeam                = (INT8)r.get8();
	s.ubBodyType           = (INT8)r.get8();
	s.bAttitude            = (INT8)r.get8();
	s.bOrders              = (INT8)r.get8();
	s.bLifeMax             = (INT8)r.get8();
	s.bLife                = (INT8)r.get8();
	s.bAgility             = (INT8)r.get8();
	s.bDexterity           = (INT8)r.get8();
	s.bExpLevel            = (INT8)r.get8();
	s.bMarksmanship        = (INT8)r.get8();
	s.bMedical             = (INT8)r.get8();
	s.bMechanical          = (INT8)r.get8();
	s.bExplosive           = (INT8)r.get8();
	s.bLeadership          = (INT8)r.get8();
	s.bStrength            = (INT8)r.get8();
	s.bWisdom              = (INT8)r.get8();
	s.bMorale              = (INT8)r.get8();
	s.bAIMorale            = (INT8)r.get8();
	r.getbytes( s.HeadPal,  sizeof(PaletteRepID) );
	r.getbytes( s.PantsPal, sizeof(PaletteRepID) );
	r.getbytes( s.VestPal,  sizeof(PaletteRepID) );
	r.getbytes( s.SkinPal,  sizeof(PaletteRepID) );
	r.getbytes( s.MiscPal,  sizeof(PaletteRepID) );
	for ( int i = 0; i < MAXPATROLGRIDS; ++i )
		s.sPatrolGrid[i] = (INT32)r.get32();
	s.bPatrolCnt           = (INT8)r.get8();
	s.fVisible             = (BOOLEAN)r.get8();
	for ( int i = 0; i < 10; ++i )
		s.name[i] = (CHAR16)r.get16();
	s.ubSoldierClass       = r.get8();
	s.fOnRoof              = (BOOLEAN)r.get8();
	s.bSectorZ             = (INT8)r.get8();
	s.pExistingSoldier     = NULL;	// never read a sender pointer
	s.fUseExistingSoldier  = (BOOLEAN)r.get8();
	s.ubCivilianGroup      = r.get8();
	s.fKillSlotIfOwnerDies = (BOOLEAN)r.get8();
	s.ubScheduleID         = r.get8();
	s.fUseGivenVehicle     = (BOOLEAN)r.get8();
	s.bUseGivenVehicleID   = (INT8)r.get8();
	s.fHasKeys             = (BOOLEAN)r.get8();
	for ( int x = 0; x < AI_WIRE_SLOTS; ++x )
	{
		slots[x].usItem            = r.get16();
		slots[x].ubNumberOfObjects = r.get8();
		slots[x].fFlags            = r.get8();
	}
	return r.ok;
}

bullets_table bTable[MAXTEAMS][NUM_BULLET_SLOTS];	// rows=global teams, cols=sender bullet slot (audit [40]/[48])

// L1 - grenade pre-random sync: each side keeps a monotonic counter. The sender
// stamps every grenade event; the receiver only adopts the event's
// guiPreRandomIndex when its sequence is newer than the last one applied, so a
// dropped/duplicated/reordered event can no longer drift the shared RNG cursor.
static UINT32 guiGrenadeEventSeqOut = 0;       // next seq to stamp on outgoing grenade events
static UINT32 guiLastGrenadeEventSeqIn = 0;    // highest seq adopted from incoming grenade events

char client_names[4][30];

char team_names[1][30];//hayden need client_names with AI

// OJW - 20081204
int	 client_ready[4];
int	 client_edges[5];
int	 client_teams[4];
int	 random_mercs[7];
// OJW - 20090305
int	 client_downloading[4];
int	 client_progress[4];
int		TEAM;

UINT8	netbTeam;

UINT16	ubID_prefix;

bool is_connected=false;
bool is_connecting=false;
bool is_client=false;
bool is_server=false;
bool is_networked=false;
bool is_host=false; // OJW - added 20081130 - new flag to signal our intention to host, coming in from the HOST screen
bool auto_retry=true;
int  giNumTries = MAX_CONNECT_RETRIES; // default is 5 retries
UINT32 giNextRetryTime = 0;
bool requested=false;
int kit[20];
bool allowlaptop=false;
bool gfIAmAdmin=false;   // dedicated-server admin (this client controls the lobby)
bool recieved_settings=false;
// OJW - 20090422
bool recieved_transfer_settings = false;
bool getReal=false;

int CLIENT_NUM;

// --------------------
// Global Client Variables
// --------------------
UINT8 cStartingSectorEdge;
char cKitBag[100];
char cClientName[30];
UINT8	gRandomStartingEdge;
UINT8	gRandomMercs;
char	cServerName[30];
char cGameDataSyncDirectory[100];
UINT8 cReportHiredMerc;
UINT8 cWeaponReadyBonus;
UINT8 cDisableSpectatorMode;
UINT8 gEnemyEnabled;
UINT8 gCreatureEnabled;
UINT8 gMilitiaEnabled;
UINT8 gCivEnabled;
UINT8 cAllowMercEquipment;
UINT8 cSameMercAllowed;
UINT8 cGameType;
UINT8 cMaxMercs;
FLOAT cDamageMultiplier;
UINT8 cMaxClients;
UINT8 cDisableMorale;
// --------------------

// --------------------
// Other client variables
// --------------------
bool goahead = 0;
int numready = 0;
int readystage = 0;
bool status = 0;
bool wiped;
UINT32 iScoreScreenTime = 0;
int iTeamsWiped = 0; // counts how many teams have wiped
bool is_game_over = false;
bool isOwnTeamWipedOut = false;
int iDisconnectedScreen = 0;
int bClosingChatBoxToStartGame = false;
UINT32 iCCStartGameTime = 0;
bool is_game_started = false;

void overide_callback( UINT8 ubResult );
void kick_callback( UINT8 ubResult );
void turn_callback (UINT8 ubResult);
void ChatCallback (UINT8 ubResult);
void HandleClientConnectionLost();
void disconnected_callback(UINT8 ubResult);

//hayden - i need this for message references
CHAR16 TeamNameStrings[][30] =
{
	L"You", // player's turn
	L"AI",
	L"Creature",
	L"Militia",
	L"Civilian",
	L"Player_Plan",// planning turn
	L"Client #1",//hayden
	L"Client #2",//hayden
	L"Client #3",//hayden
	L"Client #4",//hayden

};

// OJW - 20081222
void send_gameover( void );
void StartScoreScreen(); // this screen will send us to the multiplayer score screen

// OJW - 20090403
extern BOOLEAN		gfMPSScoreScreenCanContinue; // can the score screen continue

// OJW - 20090430
SystemAddress serverAddr;

// OJW - 20090507
settings_struct gMPServerSettings; // store a copy of our settings after we receive them
CHAR16			gszDisconnectReason[255]; // the reason we were disconnected from the server

// OJW - added 20081130
// <TODO> - add retry timer and notification
void NetworkAutoStart()
{
	if (!is_networked)
		return; // not networked, bad call

	if (!is_host || is_server) 
	{
		// is pure client or the server has been started
		// so do client connection checking...though probably not needed for the server client. cant hurt.

		// user JOIN'd game
		if (is_client && is_connected)
		{
			return; // should be set up and running
		}
		else if (is_connecting)
		{
			return; // we're waiting on a connection , do nothing
		}
		else
		{
			if (giNumTries <= 0 || !auto_retry)
				return; // dont auto-retry

			if (guiBaseJA2NoPauseClock < giNextRetryTime)
				return; // we are waiting for a retry timer

			// try and connect
			giNumTries--;
			connect_client();

			// next rety time is set in client_packet()
		}
	}
	else 
	{
		// is host and server isnt started so start it
		start_server();
	}
}

void InvalidClientSettingsOkBoxCallback( UINT8 bExitValue )
{
	// yes, load the game
	if( bExitValue == MSG_BOX_RETURN_OK )
	{		
		// gracefully disconnect to the main menu
		client_disconnect();
		guiPendingScreen = MP_JOIN_SCREEN;
	}

	return;
}


// OJW - this is just a fudge for now and needs to be improved later
bool are_clients_downloading()
{
	bool bDownloading = false;
	for (int i=0; i < 4; i++)
	{
		if (client_downloading[i] == 1)
			bDownloading = true;
	}

	return bDownloading;
}



void HireRandomMercs()
{
	MERC_HIRE_STRUCT HireMercStruct;
	for (int i=0; i < cMaxMercs; i++)
	{
		memset(&HireMercStruct, 0, sizeof(MERC_HIRE_STRUCT));

		HireMercStruct.ubProfileID = random_mercs[i];// use the merc list recieved from the server

		//DEF: temp
		HireMercStruct.sSectorX = gsMercArriveSectorX;
		HireMercStruct.sSectorY = gsMercArriveSectorY;
		HireMercStruct.fUseLandingZoneForArrival = TRUE;
		HireMercStruct.ubInsertionCode	= INSERTION_CODE_ARRIVING_GAME;
		HireMercStruct.iTotalContractLength = 1;
		HireMercStruct.fCopyProfileItemsOver = true;

		gMercProfiles[ HireMercStruct.ubProfileID ].ubMiscFlags |= PROFILE_MISC_FLAG_ALREADY_USED_ITEMS;

		HireMerc(&HireMercStruct);
	}

	fDrawCharacterList = true;
	fTeamPanelDirty = true;
	ReBuildCharactersList();
}


//*****************
//RPC sends and recieves:
//********************

void send_path (  SOLDIERTYPE *pSoldier, INT32 sDestGridNo, UINT16 usMovementAnim, BOOLEAN fFromUI, BOOLEAN fForceRestartAnim  )
{
	if(pSoldier->ubID < 120)
	{
		//ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, L"Sending new path" );

		EV_S_SENDPATHTONETWORK SNetPath;

		if(pSoldier->ubID < 20)
			SNetPath.usSoldierID = (pSoldier->ubID)+ubID_prefix;
		else
			SNetPath.usSoldierID = pSoldier->ubID;
	
		SNetPath.sAtGridNo=pSoldier->sGridNo;
				
		SNetPath.ubNewState=usMovementAnim;
		SNetPath.usCurrentPathIndex=pSoldier->pathing.usPathIndex;
		memcpy(SNetPath.usPathData, pSoldier->pathing.usPathingData, sizeof(UINT16)*30);
		SNetPath.usPathDataSize=pSoldier->pathing.usPathDataSize;
		SNetPath.sDestGridNo=sDestGridNo;
			
		client->RPC("sendPATH",(const char*)&SNetPath, (int)sizeof(EV_S_SENDPATHTONETWORK)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
	}
}

void recievePATH(RPCParameters *rpcParameters)
{
	//ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, L"Recieving new path," );
				
	RPC_REQUIRE_BYTES(rpcParameters, EV_S_SENDPATHTONETWORK);	// short-frame guard (H6/H13)
	EV_S_SENDPATHTONETWORK* SNetPath = (EV_S_SENDPATHTONETWORK*)rpcParameters->input;

	// H6: ubNewState indexes gAnimControl[] / EVENT_InitNewSoldierAnim() -- bound it.
	if ( !IsValidAnimState( SNetPath->ubNewState ) )
		return;

	SOLDIERTYPE *pSoldier = SNetPath->usSoldierID;
	if ( pSoldier == NULL || !pSoldier->bActive || !pSoldier->bInSector )
	{
		return;	// MP wire guard: ignore events for soldiers not in our world (mp_audit_findings.json)
	}


	memcpy(pSoldier->pathing.usPathingData, SNetPath->usPathData,sizeof(UINT16)*30);

	pSoldier->pathing.sDestination = SNetPath->sDestGridNo;
	pSoldier->pathing.sFinalDestination = SNetPath->sDestGridNo;
	pSoldier->pathing.usPathIndex=SNetPath->usCurrentPathIndex;
	pSoldier->pathing.usPathDataSize=SNetPath->usPathDataSize;

	SendGetNewSoldierPathEvent( pSoldier, SNetPath->sDestGridNo, SNetPath->ubNewState );	

	INT16 sCellX, sCellY;	
	ConvertGridNoToCenterCellXY(SNetPath->sAtGridNo, &sCellX, &sCellY);

	if (( gAnimControl[ pSoldier->usAnimState ].uiFlags & ( ANIM_MOVING | ANIM_SPECIALMOVE ) ) && !(pSoldier->flags.fNoAPToFinishMove ) )
	{
	}
	else
	{
		pSoldier->EVENT_InternalSetSoldierPosition( sCellX, sCellY ,FALSE, FALSE, FALSE );		
	}

	if(pSoldier)pSoldier->EVENT_InitNewSoldierAnim( SNetPath->ubNewState, 0, FALSE );			
}


void send_stance ( SOLDIERTYPE *pSoldier, UINT8 ubDesiredStance )

{
	if(pSoldier->ubID < 120)
	{
		EV_S_CHANGESTANCE			SChangeStance;

		SChangeStance.ubNewStance   = ubDesiredStance;
		//SChangeStance.usSoldierID  = pSoldier->ubID;
		

		if(pSoldier->ubID < 20)
			SChangeStance.usSoldierID = (pSoldier->ubID)+ubID_prefix;
		else
			SChangeStance.usSoldierID = pSoldier->ubID;


		SChangeStance.sXPos				= pSoldier->sX;
		SChangeStance.sYPos				= pSoldier->sY;
		SChangeStance.uiUniqueId = pSoldier -> uiUniqueSoldierIdValue;

		//ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, L"change stance: %d",ubDesiredStance );
	
		client->RPC("sendSTANCE",(const char*)&SChangeStance, (int)sizeof(EV_S_CHANGESTANCE)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
	}
}


void recieveSTANCE(RPCParameters *rpcParameters)
{


		EV_S_CHANGESTANCE* SChangeStance = (EV_S_CHANGESTANCE*)rpcParameters->input;
	
		SOLDIERTYPE *pSoldier = SChangeStance->usSoldierID;
	if ( pSoldier == NULL || !pSoldier->bActive || !pSoldier->bInSector )
	{
		return;	// MP wire guard: ignore events for soldiers not in our world (mp_audit_findings.json)
	}
	
		pSoldier->ChangeSoldierStance( SChangeStance->ubNewStance );

		//SendChangeSoldierStanceEvent( pSoldier, SChangeStance->ubNewStance );
		//AddGameEvent( S_CHANGESTANCE, 0, &SChangeStance );
		//********* implemented using event pump system ... :)
}



void send_dir ( SOLDIERTYPE *pSoldier, UINT16 usDesiredDirection )

{
	if((is_server && pSoldier->ubID < 120) || (!is_server && pSoldier->ubID < 20))
	{

		EV_S_SETDESIREDDIRECTION	SSetDesiredDirection;

		//SSetDesiredDirection.usSoldierID = pSoldier->ubID;
			

		if(pSoldier->ubID < 20)
			SSetDesiredDirection.usSoldierID = (pSoldier->ubID)+ubID_prefix;
		else
			SSetDesiredDirection.usSoldierID = pSoldier->ubID;

	

		SSetDesiredDirection.usDesiredDirection = usDesiredDirection;
		SSetDesiredDirection.uiUniqueId = pSoldier -> uiUniqueSoldierIdValue;

		client->RPC("sendDIR",(const char*)&SSetDesiredDirection, (int)sizeof(EV_S_SETDESIREDDIRECTION)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
	}
}


void recieveDIR(RPCParameters *rpcParameters)
{

		EV_S_SETDESIREDDIRECTION* SSetDesiredDirection = (EV_S_SETDESIREDDIRECTION*)rpcParameters->input;			
			

		SOLDIERTYPE *pSoldier = SSetDesiredDirection->usSoldierID;
	if ( pSoldier == NULL || !pSoldier->bActive || !pSoldier->bInSector )
	{
		return;	// MP wire guard: ignore events for soldiers not in our world (mp_audit_findings.json)
	}

		pSoldier->EVENT_SetSoldierDesiredDirection( SSetDesiredDirection->usDesiredDirection );

		//AddGameEvent( S_SETDESIREDDIRECTION, 0, &SSetDesiredDirection );
		//********* implemented using event pump system ... :)
}

void send_fire( SOLDIERTYPE *pSoldier, INT32 sTargetGridNo )
{
	if(pSoldier->ubID < 120)
	{
	
	EV_S_BEGINFIREWEAPON SBeginFireWeapon;


		

	if(pSoldier->ubID < 20)
		SBeginFireWeapon.usSoldierID = (pSoldier->ubID)+ubID_prefix;
	else
		SBeginFireWeapon.usSoldierID = pSoldier->ubID;


	SBeginFireWeapon.sTargetGridNo = sTargetGridNo;
	SBeginFireWeapon.bTargetLevel = pSoldier->bTargetLevel;
	SBeginFireWeapon.bTargetCubeLevel = pSoldier->bTargetCubeLevel;
	SBeginFireWeapon.uiUniqueId = pSoldier->usAttackingWeapon;
		

	client->RPC("sendFIRE",(const char*)&SBeginFireWeapon, (int)sizeof(EV_S_BEGINFIREWEAPON)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
	}
}

void recieveFIRE(RPCParameters *rpcParameters)
{		
	EV_S_BEGINFIREWEAPON* SBeginFireWeapon = (EV_S_BEGINFIREWEAPON*)rpcParameters->input;
	//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"SendBeginFireWeaponEvent" );

	SOLDIERTYPE *pSoldier = SBeginFireWeapon->usSoldierID;
	if ( pSoldier == NULL || !pSoldier->bActive || !pSoldier->bInSector )
	{
		return;	// MP wire guard: ignore events for soldiers not in our world (mp_audit_findings.json)
	}

	pSoldier->sTargetGridNo = SBeginFireWeapon->sTargetGridNo;
	pSoldier->bTargetLevel = SBeginFireWeapon->bTargetLevel;
	pSoldier->bTargetCubeLevel = SBeginFireWeapon->bTargetCubeLevel;
	pSoldier->usAttackingWeapon = SBeginFireWeapon->uiUniqueId; //cheap hack to pass wep id. 
					
	SendBeginFireWeaponEvent( pSoldier, SBeginFireWeapon->sTargetGridNo );
}



void send_hit(  EV_S_WEAPONHIT *SWeaponHit  )
{
	//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"sendHIT" );
	//EV_S_WEAPONHIT* pWeaponHit =  (EV_S_WEAPONHIT*)pEventData;
	//SOLDIERTYPE *pSoldier = MercPtrs[ usSoldierID ];

	EV_S_WEAPONHIT weaphit_struct;				
			
	memcpy( &weaphit_struct , SWeaponHit, sizeof( EV_S_WEAPONHIT ));
	
	SoldierID usSoldierID = weaphit_struct.usSoldierID;

	if(SWeaponHit->usSoldierID < 20)weaphit_struct.usSoldierID = weaphit_struct.usSoldierID+ubID_prefix;
	if(SWeaponHit->ubAttackerID < 20)weaphit_struct.ubAttackerID = weaphit_struct.ubAttackerID+ubID_prefix;
		
	client->RPC("sendHIT",(const char*)&weaphit_struct, (int)sizeof(EV_S_WEAPONHIT)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveHIT(RPCParameters *rpcParameters)
{
	//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"recieveHIT" );
		
	RPC_REQUIRE_BYTES(rpcParameters, EV_S_WEAPONHIT);	// short-frame guard (H11/H13)
	EV_S_WEAPONHIT* SWeaponHit = (EV_S_WEAPONHIT*)rpcParameters->input;
	
	SOLDIERTYPE *pSoldier = SWeaponHit->usSoldierID;
	SoldierID usSoldierID;
	SoldierID ubAttackerID;

	if((SWeaponHit->usSoldierID >= ubID_prefix) && (SWeaponHit->usSoldierID < (ubID_prefix+6))) // within our netbTeam range...
		usSoldierID = (SWeaponHit->usSoldierID - ubID_prefix);
	else
		usSoldierID = SWeaponHit->usSoldierID;

	if((SWeaponHit->ubAttackerID >= ubID_prefix) && (SWeaponHit->ubAttackerID < (ubID_prefix+6)))
		ubAttackerID = (SWeaponHit->ubAttackerID - ubID_prefix);
	else
		ubAttackerID = SWeaponHit->ubAttackerID;

	// H11: WeaponHit derefs the victim unconditionally (sGridNo -> gpWorldLevelData[]).
	// The victim may not exist on this client yet (spawn-order race) or be a bad wire
	// id -- drop the frame if it doesn't resolve to a live merc.
	if ( SafeMerc( usSoldierID.i ) == NULL )
		return;

	WeaponHit( usSoldierID, SWeaponHit->usWeaponIndex, SWeaponHit->sDamage, SWeaponHit->sBreathLoss, SWeaponHit->usDirection, SWeaponHit->sXPos, SWeaponHit->sYPos, SWeaponHit->sZPos, SWeaponHit->sRange, ubAttackerID, SWeaponHit->fHit, SWeaponHit->ubSpecial, SWeaponHit->ubLocation );

	if(SWeaponHit->fStopped)
	{
		// bTable rows are written keyed by the FIRER's team with the raw wire id
		// (recieveBULLET); key the read the same way, bounded. The old code keyed
		// by the untranslated VICTIM -- wrong row, and pSoldier can be NULL here.
		SOLDIERTYPE *pFirer = ( SWeaponHit->ubAttackerID != NOBODY ) ? (SOLDIERTYPE*)SWeaponHit->ubAttackerID : NULL;
		if ( pFirer != NULL && pFirer->bTeam >= 0 && pFirer->bTeam < 11
			&& SWeaponHit->iBullet >= 0 && SWeaponHit->iBullet < NUM_BULLET_SLOTS )
		{
		INT8 bTeam=pFirer->bTeam;
		INT32 iBullet = bTable[bTeam][SWeaponHit->iBullet].local_id;
				
		StopBullet( iBullet );
		RemoveBullet(iBullet);
		}

		//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"removed bullet" );	
	}		
}


void send_dismiss(UINT16 ubCurrentSoldierID)
{
	send_dismiss_struct sDismissMerc;

	sDismissMerc.ubProfileID = ubCurrentSoldierID + ubID_prefix;

	client->RPC("sendDISMISS",(const char*)&sDismissMerc, (int)sizeof(send_dismiss_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void send_hire( SoldierID iNewIndex, UINT8 ubCurrentSoldier, INT16 iTotalContractLength, BOOLEAN fCopyProfileItemsOver)
{
	send_hire_struct sHireMerc;

	sHireMerc.ubProfileID=ubCurrentSoldier;
	sHireMerc.team=TEAM;
	sHireMerc.fCopyProfileItemsOver=fCopyProfileItemsOver;
	sHireMerc.bTeam=netbTeam;

	SOLDIERTYPE *pSoldier = iNewIndex;

	mp_log_soldier( pSoldier, "hired (joined the team)" );   // -> coordinator log (verbose+)

	UINT8 sectorEdge = cStartingSectorEdge;
	
	// WANNE - MP: Center
	if (sectorEdge == MP_EDGE_CENTER)
		sectorEdge = INSERTION_CODE_CENTER;

	pSoldier->ubStrategicInsertionCode = sectorEdge;

	if(ubCurrentSoldier==64)//slay
	{
		pSoldier->ubBodyType = REGMALE;
		gMercProfiles[ pSoldier->ubProfile ].ubBodyType = REGMALE;
	}

	ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[44], pSoldier->name);

	AddCharacterToAnySquad( pSoldier );
	
	//add recruited flag
	gMercProfiles[ pSoldier->ubProfile ].ubMiscFlags |= PROFILE_MISC_FLAG_RECRUITED;

	OBJECTTYPE		Object;
	int cnt;
	for (cnt=0; cnt<20;cnt++)
	{
		int item=kit[cnt];

		if(item > 0 && item < (int)gMAXITEMS_READ)
		{
			if ( CreateItem( (UINT16)item, 100, &Object ) )
				AutoPlaceObject( pSoldier, &Object, TRUE );
		}
	}

	client->RPC("sendHIRE",(const char*)&sHireMerc, (int)sizeof(send_hire_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveDISMISS(RPCParameters *rpcParameters)
{
	send_dismiss_struct* sDismissMerc = (send_dismiss_struct*)rpcParameters->input;

	// Get soldier we should dismiss
	if ( sDismissMerc->ubProfileID >= TOTAL_SOLDIERS )
	{
		return;	// MP wire guard (mp_audit_findings.json)
	}
	SOLDIERTYPE * pSoldier=MercPtrs[ sDismissMerc->ubProfileID ];
	if ( pSoldier == NULL || !pSoldier->bActive )
	{
		return;
	}

	TacticalRemoveSoldier( pSoldier->ubID );
}

void recieveHIRE(RPCParameters *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, send_hire_struct);	// short-frame guard (M6/H13)
	send_hire_struct* sHireMerc = (send_hire_struct*)rpcParameters->input;

	// M6: bTeam indexes gTacticalStatus.Team[] (writes) and client_names[bTeam-6];
	// ubProfileID indexes gMercProfiles[]. A bad wire value creates a mis-sided merc
	// and corrupts team state -- only accept the 4 LAN teams and a valid profile.
	if ( sHireMerc->bTeam < LAN_TEAM_ONE || sHireMerc->bTeam >= LAN_TEAM_ONE + 4 )
		return;
	if ( sHireMerc->ubProfileID >= NUM_PROFILES )
		return;

	SOLDIERTYPE	*pSoldier;
	SoldierID	iNewIndex;

	SOLDIERCREATE_STRUCT		MercCreateStruct;
	memset( (void*)&MercCreateStruct, 0, SIZEOF_SOLDIERCREATE_STRUCT_POD );
	BOOLEAN fReturn = FALSE;
	
	MercCreateStruct.ubProfile						= sHireMerc->ubProfileID;
	MercCreateStruct.fPlayerMerc					= 0;
	MercCreateStruct.sSectorX							= gsMercArriveSectorX;
	MercCreateStruct.sSectorY							= gsMercArriveSectorY;
	MercCreateStruct.bSectorZ							= 0;
	MercCreateStruct.bTeam								= sHireMerc->bTeam;
	MercCreateStruct.fCopyProfileItemsOver			= sHireMerc->fCopyProfileItemsOver;

	if ( TacticalCreateSoldier( &MercCreateStruct, &iNewIndex ) == NULL || iNewIndex == NOBODY )
	{
		return;	// creation refused (e.g. client-side AI skip) -- do not touch MercPtrs[NOBODY]
	}

	pSoldier = iNewIndex;

	// Spawn the enemy copy at its OWNER's starting edge (from the settings), not the
	// zeroed default -- otherwise every copy lands near OUR edge and the LOS engine
	// "sees" it at point-blank range, falsely triggering turn-based at game start.
	// (Exact tiles still re-sync via guiPOS once the owner moves.) Same field send_hire
	// sets on the owner's own merc.
	{
		int ownerIdx = (int)sHireMerc->bTeam - LAN_TEAM_ONE;   // 0..3 for LAN teams 6..9
		if ( ownerIdx >= 0 && ownerIdx < 5 )
		{
			UINT8 edge = (UINT8)client_edges[ ownerIdx ];
			pSoldier->ubStrategicInsertionCode = ( edge == MP_EDGE_CENTER ) ? (UINT8)INSERTION_CODE_CENTER : edge;
		}
	}
	pSoldier->flags.uiStatusFlags |= SOLDIER_PC;
	if ( cGameType == MP_TYPE_DEATHMATCH && pSoldier->bTeam >= LAN_TEAM_ONE )
		pSoldier->bSide = 1;	// LAN squads must read as hostile side in DM (audit: morale)
	pSoldier->aiData.bNeutral = FALSE;
	gMercProfiles[ pSoldier->ubProfile ].ubMiscFlags |= PROFILE_MISC_FLAG_RECRUITED;
	
	if(!cSameMercAllowed)
		gMercProfiles[ pSoldier->ubProfile ].bMercStatus = MERC_WORKING_ELSEWHERE;

	pSoldier->bSide=0; //default coop only
	pSoldier->aiData.bNeutral = FALSE;
	gTacticalStatus.Team[MercCreateStruct.bTeam	].bSide=0;

#ifdef ENABLE_MP_FRIENDLY_PLAYERS_SHARE_SAME_FOV
	pSoldier->bVisible = 1;
#endif

	if(MercCreateStruct.ubProfile==SLAY)//slay
	{
		pSoldier->ubBodyType = REGMALE;
		gMercProfiles[ pSoldier->ubProfile ].ubBodyType = REGMALE;
	}	
	
	if(cGameType==MP_TYPE_DEATHMATCH)//all vs all only
	{
		pSoldier->bSide=1;

#ifdef ENABLE_MP_FRIENDLY_PLAYERS_SHARE_SAME_FOV
		pSoldier->bVisible = 0;
#endif

		gTacticalStatus.Team[MercCreateStruct.bTeam	].bSide=1;
	}
	if(cGameType==MP_TYPE_TEAMDEATMATCH) //allow teams
	{
		if(sHireMerc->team != TEAM)
		{
			pSoldier->bSide=1;

#ifdef ENABLE_MP_FRIENDLY_PLAYERS_SHARE_SAME_FOV
			pSoldier->bVisible = 0;
#endif

			gTacticalStatus.Team[MercCreateStruct.bTeam	].bSide=1;
		}
	}

	AddSoldierToSector( iNewIndex );

	if(cReportHiredMerc == 1)
	{	
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[5],MercCreateStruct.bTeam-5,client_names[MercCreateStruct.bTeam-6],pSoldier->name );
	}
	else 
	{
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[6],MercCreateStruct.bTeam-5,client_names[MercCreateStruct.bTeam-6] );
	}
}

void send_gui_pos(SOLDIERTYPE *pSoldier,  FLOAT dNewXPos, FLOAT dNewYPos)
{
	gui_pos gnPOS;

	gnPOS.usSoldierID = pSoldier->ubID + ubID_prefix;
	
	gnPOS.dNewXPos = dNewXPos;
	gnPOS.dNewYPos = dNewYPos;

	client->RPC("sendguiPOS",(const char*)&gnPOS, (int)sizeof(gui_pos)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveguiPOS(RPCParameters *rpcParameters)
{
	gui_pos* gnPOS = (gui_pos*)rpcParameters->input;

	SOLDIERTYPE *pSoldier = gnPOS->usSoldierID;

	if ( pSoldier == NULL || !pSoldier->bActive || !pSoldier->bInSector )
	{
		// Sector-load / placement race: the other player already broadcasts GUI
		// position/facing sync while this machine is still loading the sector --
		// the soldier slot may be inactive (stale pLevelNode garbage -> crash in
		// AddMercStructureInfoFromAnimSurface). Drop the cosmetic update.
		return;
	}

	INT32 sNewGridNo;

	sNewGridNo = GETWORLDINDEXFROMWORLDCOORDS(gnPOS->dNewXPos, gnPOS->dNewYPos );
	pSoldier->usStrategicInsertionData=sNewGridNo;
	pSoldier->ubStrategicInsertionCode=INSERTION_CODE_GRIDNO;
	pSoldier->sInsertionGridNo = pSoldier->usStrategicInsertionData;

	pSoldier->EVENT_SetSoldierPosition( gnPOS->dNewXPos, gnPOS->dNewYPos );
}

void send_gui_dir(SOLDIERTYPE *pSoldier, UINT16	usNewDirection)
{	
	gui_dir gnDIR;

	gnDIR.usSoldierID = (pSoldier->ubID)+ubID_prefix;
	gnDIR.usNewDirection = usNewDirection;
	
	client->RPC("sendguiDIR",(const char*)&gnDIR, (int)sizeof(gui_dir)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveguiDIR(RPCParameters *rpcParameters)
{
	gui_dir* gnDIR = (gui_dir*)rpcParameters->input;

	SOLDIERTYPE *pSoldier = gnDIR->usSoldierID;

	if ( pSoldier == NULL || !pSoldier->bActive || !pSoldier->bInSector )
	{
		// Sector-load / placement race: the other player already broadcasts GUI
		// position/facing sync while this machine is still loading the sector --
		// the soldier slot may be inactive (stale pLevelNode garbage -> crash in
		// AddMercStructureInfoFromAnimSurface). Drop the cosmetic update.
		return;
	}

	if ( gnDIR->usNewDirection >= NUM_WORLD_DIRECTIONS )
	{
		return;   // direction off the wire must be 0..7
	}

	pSoldier->EVENT_SetSoldierDirection( gnDIR->usNewDirection );
}


void send_EndTurn( UINT8 ubNextTeam )
{
	if(ubNextTeam==0)
	{
		// translate next team id for clients
		ubNextTeam=netbTeam;
	}
	
	turn_struct tStruct;

	if(is_server)
		Sawarded=false;

	tStruct.tsnetbTeam = netbTeam;
	tStruct.tsubNextTeam = ubNextTeam;
	
	client->RPC("sendEndTurn",(const char*)&tStruct, (int)sizeof(turn_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveEndTurn(RPCParameters *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, turn_struct);	// short-frame guard (M7/H13)
	turn_struct* tStruct = (turn_struct*)rpcParameters->input;
	UINT8 sender_bTeam;
	UINT8 ubTeam;
	sender_bTeam=tStruct->tsnetbTeam;
	ubTeam=tStruct->tsubNextTeam;

	// a fresh turn boundary means any enemy interrupt against us is done -> let the
	// banner go green again (safety net in case a resume_turn was missed/reordered).
	gMpEnemyInterruptTeam = 0;

	if(is_server)
		Sawarded=false;
	
	// if the message was recieved from the server..
	if(is_server || sender_bTeam==6)
	{
		// if not the server and we're not in combat...
		if (!is_server && !(gTacticalStatus.uiFlags & INCOMBAT))
		{
			EnterCombatMode(0); 
		}

		if(ubTeam==netbTeam)ubTeam=0;
		// M7: ubTeam drives EndTurn()/BeginTeamTurn() (ubCurrentTeam -> Team[]) -- bound it.
		if(ubTeam>=MAXTEAMS)
			return;
		{
			if(!is_server && is_client)
				EndTurnEvents();
		}

		if(!is_server && is_client)
			EndTurn( ubTeam );

		requested=false ;//request for realtime made or not
		BeginTeamTurn( ubTeam );
	}
}

UINT8 numenemyLAN( UINT8 ubSectorX, UINT8 ubSectorY )
{
	SOLDIERTYPE *pSoldier;
	UINT8				cnt ; //first posible lan player
	UINT8				ubNumEnemies = 0;

	for ( cnt=120 ; cnt <= 155; cnt++ )
	{
		pSoldier = MercPtrs[cnt];
		if ( pSoldier->bActive && pSoldier->bInSector && pSoldier->stats.bLife > 0 )
		{
			if ( !pSoldier->aiData.bNeutral && (pSoldier->bSide != 0 ) )
			{
				ubNumEnemies++;
			}
		}	
	}

	return ubNumEnemies;
}

void send_AI( SOLDIERCREATE_STRUCT *pCreateStruct )
{
	// PORTABLE WIRE FORMAT (H17): serialize only the POD scalar prefix + per-slot
	// {usItem,ubNumberOfObjects,fFlags}. No std::vector/std::list/pointers cross the wire.
	uint8_t wire[AI_WIRE_MAX_BYTES];
	int wireBytes = SerializeAI( wire, sizeof(wire), pCreateStruct );
	client->RPC("sendAI",(const char*)wire, wireBytes*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveAI (RPCParameters *rpcParameters)
{
	SoldierID iNewIndex;
	SOLDIERTYPE *pSoldier;

	SOLDIERCREATE_STRUCT new_standard_data;
	memset( (void*)&new_standard_data, 0, SIZEOF_SOLDIERCREATE_STRUCT_POD );

	// PORTABLE WIRE FORMAT (H17): deserialize the POD prefix + per-slot inventory tuples
	// from the fixed-width payload. pExistingSoldier is forced NULL (never a sender pointer);
	// the wire carries no std::vector/std::list. Drop short/bad frames.
	ai_wire_slot slots[AI_WIRE_SLOTS];
	if ( !DeserializeAI( new_standard_data, slots, rpcParameters->input, (rpcParameters->numberOfBitsOfData + 7) / 8 ) )
		return;
	new_standard_data.pExistingSoldier = NULL;   // raw pointer from the sender's address space -- never dereference
	new_standard_data.fUseExistingSoldier = FALSE;

	// M6: wire bTeam feeds TacticalCreateSoldier (Team classification); ubProfile
	// indexes gMercProfiles[]. AI can be on any engine team, so accept any valid team
	// but drop garbage. Validated after the portable deserialize fills new_standard_data.
	if ( new_standard_data.bTeam < 0 || new_standard_data.bTeam >= MAXTEAMS )
		return;
	if ( new_standard_data.ubProfile >= NUM_PROFILES )
		return;

	for(int x=0;x<AI_WIRE_SLOTS;x++)
	{
		// M6: usItem indexes Item[] inside CreateItems -- drop out-of-range items.
		if(slots[x].usItem != 0 && slots[x].usItem < MAXITEMS)
			CreateItems( slots[x].usItem, 100, slots[x].ubNumberOfObjects, &new_standard_data.Inv[x] );
	}

	new_standard_data.fPlayerPlan=1;

	if ( TacticalCreateSoldier( &new_standard_data, &iNewIndex ) == NULL || iNewIndex == NOBODY )
	{
		return;	// creation refused -- do not touch MercPtrs[NOBODY]
	}
	pSoldier = iNewIndex;
	pSoldier->flags.uiStatusFlags |= SOLDIER_PC;

	AddSoldierToSector( iNewIndex );
}

void send_ready ( void )
{
	ready_struct info;
	info.client_num = CLIENT_NUM;

	if(readystage==0)
	{
		info.ready_stage = 0;
		if(status==0)
		{
			info.status = 1; 
			status=1;
			numready = numready+1;
			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[7],numready,cMaxClients );
			// OJW - 20081204
			client_ready[info.client_num-1]=1;
			fDrawCharacterList = true; // set the character list to be redrawn
		}
		else
		{
			info.status = 0; 
			status=0;
			numready = numready-1;
			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[8],numready,cMaxClients );
			// OJW - 20081204
			client_ready[info.client_num-1]=0;
			fDrawCharacterList = true; // set the character list to be redrawn
		}	
	}
			
	if(is_server && numready == cMaxClients) //all ready. and server tells all to load...
	{
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[9] );

		goahead=1;
		readystage=1;

		info.ready_stage = 1;
		info.status = 1; 					
	}	
			
	client->RPC("sendREADY",(const char*)&info, (int)sizeof(ready_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);

	if(is_server && numready == cMaxClients)
	{
		status=0;//reset
		numready=0;
		start_battle();//server loads
	}
}

void recieveREADY (RPCParameters *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, ready_struct);	// short-frame guard (H8/H13)
	ready_struct* info = (ready_struct*)rpcParameters->input;

	// H8: client_num indexes client_names[4]/client_ready[4] as [client_num-1];
	// an out-of-range wire byte is an OOB read/write. (1-based, 1..4.)
	if ( info->client_num < 1 || info->client_num > 4 )
		return;

	if(info->ready_stage==1)//recived ok for go ahead from server for level load
	{
		numready++;
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[10], info->client_num,client_names[info->client_num-1],numready,cMaxClients );
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[9] );
		status=0;//reset
		numready=0;
		goahead=1;
		start_battle();
	}
	else if (info->ready_stage != 36) // recieved status update from client
	{
		if (info->status==1)
		{
			numready = numready+1;
			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[10], info->client_num,client_names[info->client_num-1],numready,cMaxClients );
			// OJW - 20081204
			client_ready[info->client_num-1]=1;
			fDrawCharacterList = true; // set the character list to be redrawn
		}
		else
		{
			numready = numready-1;
			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[11], info->client_num,client_names[info->client_num-1],numready,cMaxClients );
			// OJW - 20081204
			client_ready[info->client_num-1]=0;
			fDrawCharacterList = true; // set the character list to be redrawn
		}
		
		if(is_server && numready == cMaxClients) //all ready. and server tells all to load...and loads himself
		{
			goahead=1;
			readystage=1;
			send_ready();
			start_battle();
		}
	}
	else if(info->ready_stage==36)//server allows laptop access
	{
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[36] );
		allowlaptop=1;
		if (gRandomMercs)
			HireRandomMercs();

		// server is starting the game, adjust the games max players
		// this will allow the game to be started, rather than saying 2/4 players are connected
		// where there are only two players connected
		int iPlayersConnected = 0;
		for (int i=0; i< 4; i++)
			if (client_names[i] != NULL && strcmp(client_names[i],"") != 0)
				iPlayersConnected++;

		{
			// Coordinator model: a dedicated host is NOT a playing participant, so
			// it must not be counted among the players that ready / place / load.
			// Every barrier checks numready==cMaxClients; excluding the host here
			// means those barriers are satisfied by the real players alone.
			extern BOOLEAN gfDedicatedServer;
			cMaxClients = iPlayersConnected - ( gfDedicatedServer ? 1 : 0 );
		}
	}
}

void send_loaded (void)
{
	ready_struct info;
	info.client_num = CLIENT_NUM;
	info.ready_stage = 1;//done loading level
	info.status=1;

	numready++;
	if(numready==cMaxClients && is_server)
	{
		lockui(1);//unlock ui
		readystage=0;
		numready=0;

		info.ready_stage = 2;//done placing mercs
		info.status=1;
	}

	client->RPC("sendGUI",(const char*)&info, (int)sizeof(ready_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void send_donegui ( UINT8 ubResult )
{
	if(ubResult==1 && status==0)
		return;//avoid double remove callback response from final message box removal
		
	ready_struct info;
	info.client_num = CLIENT_NUM;
	
	if(ubResult==1)
		DialogRemoved(1);//cleanup msgbox after not ready
	
	if(status==0)//now ready
	{
		status=1;
		numready++;
		info.ready_stage = 3;//done placing mercs
		info.status=1;
		
		SGPRect CenterRect = { 100 + xResOffset, 100 + yResOffset, SCREEN_WIDTH - 100 - xResOffset, 300 + yResOffset };
		DoMessageBox( MSG_BOX_BASIC_STYLE, MPClientMessage[12],  guiCurrentScreen, MSG_BOX_FLAG_OK | MSG_BOX_FLAG_USE_CENTERING_RECT, send_donegui,  &CenterRect );

		if(numready==cMaxClients && is_server)//all done
		{
			numready=0;
			status=0;
			info.ready_stage = 4;//done placing mercs
			info.status=1;
			gMsgBox.bHandled = MSG_BOX_RETURN_OK;
			KillTacticalPlacementGUI(); //send and kill
			ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, MPClientMessage[13]);
		}
	}
	else if(status==1)//was ready
	{
		status=0;
		numready--;
		info.ready_stage = 3;//not done placing mercs
		info.status=0;
	}

	client->RPC("sendGUI",(const char*)&info, (int)sizeof(ready_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveGUI (RPCParameters *rpcParameters)
{
	ready_struct* info = (ready_struct*)rpcParameters->input;

	if(info->ready_stage==1 && info->status==1)
	{
		numready++;
		if(numready==cMaxClients && is_server)
		{
			lockui(1);//unlock ui
			readystage=0;
			numready=0;

			ready_struct info;
			info.client_num = CLIENT_NUM;
			info.ready_stage = 2;//done placing mercs
			info.status=1;

			client->RPC("sendGUI",(const char*)&info, (int)sizeof(ready_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
		}
	}

	if(info->ready_stage==2 && info->status ==1)
	{
		lockui(1);//unlock ui
		readystage=0;
		numready=0;
	}

	if(info->ready_stage==3 && info->status==1)//recieved client done placement
	{
		numready++;
		if(numready==cMaxClients && is_server)//all done
		{
			numready=0;

			ready_struct info;
			info.client_num = CLIENT_NUM;
			info.ready_stage = 4;//all done placing mercs, kill all
			info.status=1;
			gMsgBox.bHandled = MSG_BOX_RETURN_OK;
			status=0;
			KillTacticalPlacementGUI();
			ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, MPClientMessage[13]);

			client->RPC("sendGUI",(const char*)&info, (int)sizeof(ready_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
		}
	}

	if(info->ready_stage==3 && info->status==0)//recieved client retracted place ready...
	{
		numready--;
	}

	if(info->ready_stage==4 && info->status==1)
	{
		gMsgBox.bHandled = MSG_BOX_RETURN_OK;
		KillTacticalPlacementGUI();
		ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, MPClientMessage[13]);
		numready=0;
		status=0;
	}
}

void recieveADMIN(RPCParameters *rpcParameters)
{
	gfIAmAdmin = true;
	ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, L"You are the server admin. Press 'G' to start the game once everyone has joined and readied." );
}

void send_admin_cmd(UINT8 cmd, const char* password)
{
	admin_cmd_struct ac;
	memset( &ac, 0, sizeof( ac ) );
	ac.cmd = cmd;
	if ( password ) strncpy( ac.password, password, 63 );
	client->RPC("adminCmd",(const char*)&ac, (int)sizeof(admin_cmd_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0);
}

void allowlaptop_callback ( UINT8 ubResult )
{
	if(ubResult==2)
	{
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[36] );
		allowlaptop=1;

		ready_struct info;
		info.client_num = CLIENT_NUM;
		info.ready_stage=36;
		info.status=1;

		if (gRandomMercs)
			HireRandomMercs();

		// server client is starting the game, adjust the games max players
		// this will allow the game to be started, rather than saying 2/4 players are connected
		// where there are only two players connected
		int iPlayersConnected = 0;
		for (int i=0; i< 4; i++)
			if (client_names[i] != NULL && strcmp(client_names[i],"") != 0)
				iPlayersConnected++;

		{
			// Coordinator model: a dedicated host is NOT a playing participant, so
			// it must not be counted among the players that ready / place / load.
			// Every barrier checks numready==cMaxClients; excluding the host here
			// means those barriers are satisfied by the real players alone.
			extern BOOLEAN gfDedicatedServer;
			cMaxClients = iPlayersConnected - ( gfDedicatedServer ? 1 : 0 );
		}

		client->RPC("sendREADY",(const char*)&info, (int)sizeof(ready_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
	}
}

// Coordinator force-start. The admin pressing START is the authority to begin the
// battle, but the dedicated host never readies, so the auto-barrier (numready ==
// cMaxClients in recieveREADY) never trips. The admin path used to only call
// start_battle() on the host -- which loaded the host's empty sector while the real
// clients waited forever, because nothing ever broadcast the go-ahead to THEM.
// This mirrors the laptop-unlock broadcast above (allowlaptop_callback): send
// ready_stage=1 through the loopback client so the server re-broadcasts recieveREADY
// to every real client, whose recieveREADY(stage==1) then loads the sector locally.
// Called from the dedicated server's adminCmd START handler (server.cpp).
void mp_broadcast_force_start ( void )
{
	ready_struct info;
	memset( &info, 0, sizeof(info) );
	info.client_num = CLIENT_NUM;
	info.ready_stage = 1;   // "go ahead, load the level"
	info.status = 1;
	printf( "[dedicated] broadcasting force-start (ready_stage=1) to all clients\n" ); fflush( stdout );
	client->RPC("sendREADY",(const char*)&info, (int)sizeof(ready_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void StartBattleChatBoxClosedCallback(void)
{
	// The chat box has been closed, now we can start
	SetCurrentWorldSector( gubPBSectorX, gubPBSectorY, gubPBSectorZ );
	is_game_started = true;
}

void start_battle ( void )
{ 
	{
		extern BOOLEAN gfDedicatedServer;
		if ( gfDedicatedServer )
		{
			int n = 0;
			for ( int i = 0; i < 4; i++ ) if ( strcmp( client_names[i], "" ) != 0 ) n++;
			printf( "[dedicated] start_battle: allowlaptop=%d players=%d goahead=%d\n", allowlaptop?1:0, n, goahead );
			fflush( stdout );
		}
	}
	if(!is_client)
	{
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[14] );
	}
	else if(!allowlaptop && is_server)
	{
		bool numPlayersValid = TRUE;
		bool clientsFinishedDownloading = TRUE;
		bool teamsValid = TRUE;

		// check that another player is actually connected
		int iPlayersConnected = 0;
		for (int i=0; i< 4; i++)
			if (client_names[i] != NULL && strcmp(client_names[i],"") != 0)
				iPlayersConnected++;

		if (iPlayersConnected <= 1)
		{
			numPlayersValid = FALSE;

			// notify the server that at least one other player must be connected
			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[51] );
		}
		else if (are_clients_downloading())
		{
			clientsFinishedDownloading = FALSE;

			// notify the server that some of the clients are still downloading
			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[63] );
		}
		// WANNE - MP: If choosen team deathmatch, there must be at least 2 different teams
		else if (cGameType == MP_TYPE_TEAMDEATMATCH)
		{
			bool areTeamsValid = FALSE;
			int clientTeam = client_teams[0];

			for (int i = 1; i < iPlayersConnected; i++)
			{
				if (clientTeam != client_teams[i])
				{
					areTeamsValid = TRUE;
					break;
				}
			}

			if (!areTeamsValid)
			{
				teamsValid = FALSE;

				// notify the server that the teams are not different for the choosen team-deathmatch
				ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[68] );
			}	
		}

		// Go to "ready" state!
		if (numPlayersValid && clientsFinishedDownloading && teamsValid)
		{
			extern BOOLEAN gfDedicatedServer;
			if ( gfDedicatedServer )
			{
				// headless host: no one to answer the "start & allow laptops?"
				// prompt -- confirm it directly so clients get their laptop.
				allowlaptop_callback( 2 );
			}
			else
			{
				SGPRect CenterRect = { 100 + xResOffset, 100 + yResOffset, SCREEN_WIDTH - 100 - xResOffset, 300 + yResOffset };
				DoMessageBox( MSG_BOX_BASIC_STYLE, MPClientMessage[35],  guiCurrentScreen, MSG_BOX_FLAG_YESNO | MSG_BOX_FLAG_USE_CENTERING_RECT, allowlaptop_callback,  &CenterRect );
			}
		}
	}
	else if(allowlaptop)
	{
		extern BOOLEAN gfDedicatedServer;
		if ( !gfDedicatedServer && NumberOfMercsOnPlayerTeam() ==0)
		{
			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[15] );
		}
		else if(goahead==1)
		{	
			goahead=0; // this ensures that we dont reload the sector again the next time this function is called

			if (is_game_started)
				return; // 20090317 - OJW - there is a bug or out of sync condition that rarely somestimes causes a client to reach here
						// twice, and reload the sector, which causes an assertion failure on line 1782 of strategicmap.cpp in SetCurrentWorldSector()
						// therefore i'm adding this to make sure if they client loaded the map once, it wont do it again
			
			status=0;//reset
			numready=0;

			// Find first active soldier. This is needed, because if the soldier is dismissed it does not belong to any group
			SOLDIERTYPE *pSoldier = NULL;
			for (int i = 0; i <= 19; i++)
			{
				if (MercPtrs[ i ]->bActive)
				{
					pSoldier = MercPtrs[i];
					break;
				}
			}

			extern BOOLEAN gfDedicatedServer;
			if ( pSoldier != NULL )
			{
				gpBattleGroup = GetGroup( pSoldier->ubGroupID );
				gubPBSectorX = gpBattleGroup->ubSectorX;
				gubPBSectorY = gpBattleGroup->ubSectorY;
				gubPBSectorZ = gpBattleGroup->ubSectorZ;
				gfEnterTacticalPlacementGUI = 1;
			}
			else
			{
				// Dedicated/non-playing host: no mercs of our own. The battle
				// sector is the shared MP arrival sector; we load it as the world
				// authority but have nothing to place ourselves.
				gpBattleGroup = NULL;
				gubPBSectorX = gsMercArriveSectorX;
				gubPBSectorY = gsMercArriveSectorY;
				gubPBSectorZ = 0;
				gfEnterTacticalPlacementGUI = 0;
			}
			// OJW - 20090205
			iTeamsWiped = 0; // reset the number of wiped teams to Zero

			UINT32	i;
			for(i=0; i<4;i++)
			{	
				CHAR16 name[30];
				int nm = mbstowcs( name, client_names[i], sizeof (char)*30 );
				//copy in client specified name for the player turn bar :)
				if(nm)
				{
					// OJW - 20090318 - fixed name copying bug with multiple games
					CHAR16 full[255];
					memset(full,0,sizeof(CHAR16)*255);
					swprintf(full, MPClientMessage[57],i+1,name);

					memcpy( TeamTurnString[ (i+6) ] , full, sizeof( CHAR16) * 255 );

					memcpy( TeamNameStrings[ (i+6) ] , name, sizeof( CHAR16) * 30 );
					//give me a copy too ;) - hayden
				}
			}

			// this closes the chat window if its open and the game is starting
			// if this is open then the client will crash when the screen returns
			if (guiCurrentScreen == MP_CHAT_SCREEN)
			{
				gChatBox.bHandled = MSG_BOX_RETURN_NO;
				bClosingChatBoxToStartGame = true;
				iCCStartGameTime = guiBaseJA2NoPauseClock+1000;
			}
			else
			{
				SetCurrentWorldSector( gubPBSectorX, gubPBSectorY, gubPBSectorZ );
				is_game_started = true;
			}
		}
		else
		{
			send_ready();
		}
	}
	else if(!allowlaptop && is_client && !is_server)
	{
	   ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[16] );
	}
}

void DropOffItemsInSector( UINT8 ubOrderNum )
{
	BOOLEAN	fSectorLoaded = FALSE;
	OBJECTTYPE		Object;
	UINT32	uiCount = 0;
	OBJECTTYPE	*pObject=NULL;
	UINT16	usNumberOfItems=0, usItem;
	UINT8		ubItemsDelivered, ubTempNumItems;
	UINT32	i;

	// determine if the sector is loaded
	if( ( gWorldSectorX == gsMercArriveSectorX ) && ( gWorldSectorY == gsMercArriveSectorY ) && ( gbWorldSectorZ == 0 ) )
		fSectorLoaded = TRUE;
	else
		fSectorLoaded = FALSE;

	SetSectorFlag( gsMercArriveSectorX, gsMercArriveSectorY, 0, SF_ALREADY_VISITED);//allows update of item count

	for(i=0; i<gpNewBobbyrShipments[ ubOrderNum ].ubNumberPurchases; i++)
	{
		// Count how many items were purchased
		usNumberOfItems += gpNewBobbyrShipments[ ubOrderNum ].BobbyRayPurchase[i].ubNumberPurchased;
	}

	//if we are NOT currently in the right sector
	if( !fSectorLoaded )
	{
		//build an array of objects to be added
		pObject = new OBJECTTYPE[ usNumberOfItems ];
		if( pObject == NULL )
			return;
	}

	uiCount = 0;

	//loop through the number of purchases
	for( i=0; i< gpNewBobbyrShipments->ubNumberPurchases; i++)
	{
		ubItemsDelivered = gpNewBobbyrShipments[ ubOrderNum ].BobbyRayPurchase[i].ubNumberPurchased;
		usItem = gpNewBobbyrShipments[ ubOrderNum ].BobbyRayPurchase[i].usItemIndex;

		while ( ubItemsDelivered )
		{
			// treat 0s as 1s :-)
			ubTempNumItems = __min( ubItemsDelivered, __max( 1, Item[ usItem ].ubPerPocket ) );
			CreateItems( usItem, gpNewBobbyrShipments[ ubOrderNum ].BobbyRayPurchase[i].bItemQuality, ubTempNumItems, &Object );

			// stack as many as possible
			if( fSectorLoaded )
			{
				AddItemToPool( 12880, &Object, 1, 0, WOLRD_ITEM_FIND_SWEETSPOT_FROM_GRIDNO | WORLD_ITEM_REACHABLE, 0 );
			}
			else
			{
				pObject[ uiCount ] = Object;
				uiCount++;
			}

			ubItemsDelivered -= ubTempNumItems;
		}
	}

	//if the sector WASNT loaded
	if( !fSectorLoaded )
	{
		//add all the items from the array that was built above

		//The item are to be added to the Top part of Drassen, grid loc's  10112, 9950
		if( !AddItemsToUnLoadedSector( gsMercArriveSectorX, gsMercArriveSectorY, 0, 12880, uiCount, pObject, 0, WOLRD_ITEM_FIND_SWEETSPOT_FROM_GRIDNO | WORLD_ITEM_REACHABLE, 0, 1, FALSE ) )
		{
			//error
			Assert( 0 );
		}
		delete[] pObject ;
		pObject = NULL;
	}

	//mark that the shipment has arrived
	gpNewBobbyrShipments[ ubOrderNum ].fActive = FALSE;
}

void send_stop (EV_S_STOP_MERC *SStopMerc) // used to stop a merc when he spots new enemies...
{
	EV_S_STOP_MERC stop_struct;
	if(SStopMerc->usSoldierID < 120)
	{
		if(SStopMerc->usSoldierID < 20)
		{
			stop_struct.usSoldierID = (SStopMerc->usSoldierID)+ubID_prefix;
		}
		else
			stop_struct.usSoldierID = SStopMerc->usSoldierID;

		stop_struct.sGridNo=SStopMerc->sGridNo;
		stop_struct.ubDirection=SStopMerc->ubDirection;
		stop_struct.fset=SStopMerc->fset;
		
		if(stop_struct.fset==FALSE)
		{
			return;
		}

		stop_struct.sXPos=SStopMerc->sXPos;
		stop_struct.sYPos=SStopMerc->sYPos;
		client->RPC("sendSTOP",(const char*)&stop_struct, (int)sizeof(EV_S_STOP_MERC)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
	}
}

void recieveSTOP (RPCParameters *rpcParameters)
{
	EV_S_STOP_MERC* SStopMerc =(EV_S_STOP_MERC*)rpcParameters->input;
	
	SOLDIERTYPE *pSoldier = SStopMerc->usSoldierID;
	if ( pSoldier == NULL || !pSoldier->bActive || !pSoldier->bInSector )
	{
		return;	// MP wire guard: ignore events for soldiers not in our world (mp_audit_findings.json)
	}
		
	pSoldier->EVENT_InternalSetSoldierPosition( SStopMerc->sXPos, SStopMerc->sYPos,FALSE, FALSE, FALSE );
	pSoldier->EVENT_SetSoldierDirection( SStopMerc->ubDirection );
	if ( SStopMerc->fset && pSoldier->bTeam >= LAN_TEAM_ONE
		&& pSoldier->sGridNo >= 0 && pSoldier->sGridNo < WORLD_MAX
		&& ( gAnimControl[ pSoldier->usAnimState ].uiFlags & ANIM_MOVING ) )
	{
		pSoldier->EVENT_StopMerc( pSoldier->sGridNo, pSoldier->ubDirection );
	}
	pSoldier->AdjustNoAPToFinishMove( SStopMerc->fset );
	pSoldier->flags.bTurningFromPronePosition = FALSE;	
}

// ---- diagnostic logging to the coordinator (serverLog RPC) -------------------
// Clients narrate things only they know (who sighted, who interrupts, ranges).
// The coordinator prints these centrally when VERBOSE_LOG is on.
void mp_log_event( const char* msg )
{
	if (!is_networked || !is_client || !client) return;
	char buf[256];
	strncpy( buf, msg, sizeof(buf) - 1 ); buf[sizeof(buf) - 1] = 0;
	client->RPC("serverLog", buf, (int)(strlen(buf) + 1) * 8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0);
}
void mp_log_soldier( SOLDIERTYPE* pSoldier, const char* event )
{
	if (!is_networked || !is_client || !client || pSoldier == NULL) return;
	char name[32]; int i = 0;
	for (; pSoldier->name[i] && i < 31; i++) name[i] = (pSoldier->name[i] < 128) ? (char)pSoldier->name[i] : '?';
	name[i] = 0;
	char buf[256];
	snprintf( buf, sizeof(buf), "client#%d '%s' (id %d, team %d) %s",
	          CLIENT_NUM, name, (int)pSoldier->ubID, (int)pSoldier->bTeam, event );
	mp_log_event( buf );
}

// Rich sighting log: which team/merc saw which enemy team/merc, and at what range.
// pSeen may be NULL (saw an enemy but the closest-seen lookup came up empty).
void mp_log_sighting( SOLDIERTYPE* pSighter, SOLDIERTYPE* pSeen, int range )
{
	if (!is_networked || !is_client || !client || pSighter == NULL) return;
	char sn[32], en[32]; int i;
	for (i = 0; pSighter->name[i] && i < 31; i++) sn[i] = (pSighter->name[i] < 128) ? (char)pSighter->name[i] : '?';
	sn[i] = 0;
	char buf[256];
	if (pSeen)
	{
		for (i = 0; pSeen->name[i] && i < 31; i++) en[i] = (pSeen->name[i] < 128) ? (char)pSeen->name[i] : '?';
		en[i] = 0;
		snprintf( buf, sizeof(buf),
		          "SIGHTING -> turn-based: client#%d team %d '%s'(id %d) sighted enemy team %d '%s'(id %d) at range %d",
		          CLIENT_NUM, (int)pSighter->bTeam, sn, (int)pSighter->ubID,
		          (int)pSeen->bTeam, en, (int)pSeen->ubID, range );
	}
	else
	{
		snprintf( buf, sizeof(buf),
		          "SIGHTING -> turn-based: client#%d team %d '%s'(id %d) sighted an enemy",
		          CLIENT_NUM, (int)pSighter->bTeam, sn, (int)pSighter->ubID );
	}
	mp_log_event( buf );
}

void send_interrupt (SOLDIERTYPE *pSoldier)
{
	INT_STRUCT INT;

	INT.ubID = pSoldier->ubID;
	INT.bTeam = pSoldier->bTeam;
	memcpy(INT.gubOutOfTurnOrder, gubOutOfTurnOrder, sizeof(UINT16) * MAXMERCS);
	INT.gubOutOfTurnPersons = gubOutOfTurnPersons;
	
	INT.Interrupted = gusSelectedSoldier + ubID_prefix;

	if(INT.bTeam==0)
	{
		INT.bTeam=netbTeam;
		INT.ubID=INT.ubID+ubID_prefix;
	}

	for(int i=0; i <= INT.gubOutOfTurnPersons; i++)
	{
		if(INT.gubOutOfTurnOrder[i] < 20)
		{
			INT.gubOutOfTurnOrder[i]=INT.gubOutOfTurnOrder[i]+ubID_prefix;
		}
	}
	
	if(INT.bTeam !=netbTeam)
		gTacticalStatus.ubCurrentTeam=INT.bTeam;

	// PORTABLE WIRE FORMAT (L6): serialize the fixed-width header + only the consumed
	// (gubOutOfTurnPersons+1) order entries instead of memcpy'ing the whole MAXMERCS array.
	uint8_t wire[INT_WIRE_MAX_BYTES];
	int wireBytes = SerializeINT( wire, sizeof(wire), INT );
	client->RPC("sendINTERRUPT",(const char*)wire, wireBytes*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

#ifdef INTERRUPT_MP_DEADLOCK_FIX
	void recieveINTERRUPT (RPCParameters *rpcParameters)
	{
		// PORTABLE WIRE FORMAT (L6): deserialize the fixed-width interrupt payload into a
		// local struct (bounds-checked, gubOutOfTurnPersons clamped). Drop short/bad frames.
		INT_STRUCT _intWire;
		if ( !DeserializeINT( _intWire, rpcParameters->input, (rpcParameters->numberOfBitsOfData + 7) / 8 ) )
			return;
		INT_STRUCT* INT = &_intWire;
		if (cGameType == MP_TYPE_COOP)
		{
			// C1: INT->Interrupted is a wire SoldierID -- clamp before any MercPtrs[] deref.
			SOLDIERTYPE* pOpponent = SafeMerc( INT->Interrupted.i );
			if ( pOpponent == NULL )
				return;

			if( INT->bTeam == netbTeam || is_server)//its for us or we are server and its for AI which we control
			{
				if(INT->bTeam == netbTeam)
				{
					//for me
					INT->bTeam=0;
					INT->ubID=INT->ubID - ubID_prefix;
					AddTopMessage( PLAYER_INTERRUPT_MESSAGE, TeamTurnString[ INT->bTeam ] );
				}
				else
				{
					//for ai
					//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"starting ai" );
					AddTopMessage( COMPUTER_INTERRUPT_MESSAGE, TeamTurnString[ INT->bTeam ] );
				}

				for(int i=0; i <= INT->gubOutOfTurnPersons; i++)//this loop translates soldier id's from what they are in someone else's game to what they are locally
				{
					if((INT->gubOutOfTurnOrder[i] >= ubID_prefix) && (INT->gubOutOfTurnOrder[i] < (ubID_prefix+6)))
					{
						INT->gubOutOfTurnOrder[i]=INT->gubOutOfTurnOrder[i]-ubID_prefix;
					}
				}
				memcpy(gubOutOfTurnOrder,INT->gubOutOfTurnOrder, sizeof(UINT16) * MAXMERCS);
				gubOutOfTurnPersons = INT->gubOutOfTurnPersons;

				if(INT->bTeam==netbTeam)//for us
					AddTopMessage( PLAYER_INTERRUPT_MESSAGE, TeamTurnString[ INT->bTeam ] );

				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Recieved interrupt between %s and %s.", TeamNameStrings[pOpponent->bTeam], TeamNameStrings[INT->bTeam] );

				 //start interrupt turn //real interrupt code
				SOLDIERTYPE* pSoldier = SafeMerc( INT->ubID.i );	// C1: wire id, clamp before deref
				if ( pSoldier == NULL )
					return;
				ManSeesMan(pSoldier,pOpponent,pOpponent->sGridNo,pOpponent->pathing.bLevel,2,1);
				StartInterrupt();
			}
			// It is our team
			else if(INT->bTeam == 0)
			{
				//it for us ! :)
				if(INT->gubOutOfTurnPersons==0)//indicates finished interrupt maybe can just call end interrupt
				{
					//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"old int finish" );
				}
				else //start our interrupt turn
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Interrupt of %s awarded to you.", TeamNameStrings[pOpponent->bTeam] );//was MPClientMessage[37], can be reconnected if text updated and translated

					SOLDIERTYPE* pSoldier = SafeMerc( INT->ubID.i );	// C1: wire id, clamp before deref
					if ( pSoldier == NULL )
						return;
					ManSeesMan(pSoldier,pOpponent,pOpponent->sGridNo,pOpponent->pathing.bLevel,2,1);
					StartInterrupt();
				}
			}
		}
		else
		{
			// INT already deserialized at the top of recieveINTERRUPT (L6).
			// C1: clamp wire SoldierID before MercPtrs[] deref.
			SOLDIERTYPE* pOpponent = SafeMerc( INT->Interrupted.i );
			if ( pOpponent == NULL )
				return;

			// MP: an interrupt freezes the action, so no remote LAN copy should still be
			// mid-stride. The halt below only stops one merc (INT->ubID); any OTHER moving
			// copy -- e.g. the mover we just interrupted, still cycling walk frames 703/704
			// in place on the interrupter's screen -- replays footsteps forever (endless
			// walking sound). Stop every moving LAN copy the instant the grant lands.
			for ( SoldierID _sid = 0; _sid < TOTAL_SOLDIERS; ++_sid )
			{
				SOLDIERTYPE* _s = MercPtrs[ _sid ];
				if ( _s && _s->bActive && _s->bInSector && _s->bTeam >= LAN_TEAM_ONE
					&& _s->sGridNo >= 0 && _s->sGridNo < WORLD_MAX
					&& ( gAnimControl[ _s->usAnimState ].uiFlags & ANIM_MOVING ) )
				{
					_s->EVENT_StopMerc( _s->sGridNo, _s->ubDirection );
				}
			}

			if(INT->bTeam==netbTeam)//for us
			{
				INT->bTeam=0;
				INT->ubID=INT->ubID - ubID_prefix;

				for(int i=0; i <= INT->gubOutOfTurnPersons; i++)//this loop translates soldier id's from what they are in someone else's game to what they are locally
				{
					if((INT->gubOutOfTurnOrder[i] >= ubID_prefix) && (INT->gubOutOfTurnOrder[i] < (ubID_prefix+6)))
					{
						INT->gubOutOfTurnOrder[i]=INT->gubOutOfTurnOrder[i]-ubID_prefix;
					}
				}
				memcpy(gubOutOfTurnOrder,INT->gubOutOfTurnOrder, sizeof(UINT16) * MAXMERCS);
				gubOutOfTurnPersons = INT->gubOutOfTurnPersons;

				AddTopMessage( PLAYER_INTERRUPT_MESSAGE, TeamTurnString[ INT->bTeam ] );
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Interrupt of %s awarded to %s.", TeamNameStrings[pOpponent->bTeam], TeamNameStrings[INT->bTeam] );
			}

			// WANNE - MP: This seems to cause the HANG on AI interrupt where we have to press ALT + E on the server!
			if(	INT->bTeam != 0)//not for our team - hayden
			{
				// M1: bTeam (wire INT8) latches gMpEnemyInterruptTeam and indexes the 10-row
				// TeamTurnString[]/TeamNameStrings[] banner tables -- drop out-of-range teams.
				if ( INT->bTeam < 0 || INT->bTeam > 9 )
					return;
				//stop moving merc who was interrupted and init UI bar
				SOLDIERTYPE* pMerc = SafeMerc( INT->ubID.i );	// C1: wire id, clamp before deref
				if ( pMerc == NULL )
					return;
				pMerc->HaultSoldierFromSighting(TRUE);
				FreezeInterfaceForEnemyTurn();
				// HARD-obey the arbiter: hand turn ownership to the interrupting team so
				// the engine's real not-your-turn lock engages (the cosmetic freeze alone
				// let us keep acting -> "both sides act"). EndInterrupt restores it on resume.
				gTacticalStatus.ubCurrentTeam = INT->bTeam;
				InitEnemyUIBar( 0, 0 );
				// Flip the top banner off green "PLAYER'S TURN": the freeze + ubCurrentTeam
				// hand-over already block input (clock cursor), but InitEnemyUIBar only sets
				// the progress counters -- the banner stayed PLAYER_TURN_MESSAGE, so the GUI
				// lied that we could still act. Latch the interrupting team so AddTopMessage
				// keeps overriding any late green re-assert (StartPlayerTeamTurn / the
				// per-frame timer) for the whole interrupt -- a one-shot set here loses the
				// race on the first interrupt of a turn. Cleared on resume_turn / new turn.
				gMpEnemyInterruptTeam = INT->bTeam;
				AddTopMessage( COMPUTER_INTERRUPT_MESSAGE, TeamTurnString[ INT->bTeam ] );
				fInterfacePanelDirty = DIRTYLEVEL2;
				gTacticalStatus.fInterruptOccurred = TRUE;

				//this needed to add details of who's interrupt it is - hayden
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Interrupt with %s awarded to %s.", TeamNameStrings[pOpponent->bTeam], TeamNameStrings[INT->bTeam] );//was MPClientMessage[17], can be reconnected if text updated and translated
			}
			else
			{
				//it for us ! :)
				if(INT->gubOutOfTurnPersons==0)//indicates finished interrupt maybe can just call end interrupt
				{
					//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"old int finish" );
				}
				else //start our interrupt turn
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Interrupt of %s awarded to you.", TeamNameStrings[pOpponent->bTeam] );//was MPClientMessage[37], can be reconnected if text updated and translated

					SOLDIERTYPE* pSoldier = SafeMerc( INT->ubID.i );	// C1: wire id, clamp before deref
					if ( pSoldier == NULL )
						return;
					ManSeesMan(pSoldier,pOpponent,pOpponent->sGridNo,pOpponent->pathing.bLevel,2,1);
					StartInterrupt();
				}
			}
		}
	}
#else

	void recieveINTERRUPT (RPCParameters *rpcParameters)
	{
		// PORTABLE WIRE FORMAT (L6): deserialize the fixed-width interrupt payload.
		INT_STRUCT _intWire;
		if ( !DeserializeINT( _intWire, rpcParameters->input, (rpcParameters->numberOfBitsOfData + 7) / 8 ) )
			return;
		INT_STRUCT* INT = &_intWire;
		// C1: clamp wire SoldierID before MercPtrs[] deref.
		SOLDIERTYPE* pOpponent = SafeMerc( INT->Interrupted.i );
		if ( pOpponent == NULL )
			return;

		if(INT->bTeam==netbTeam)//for us
		{
			INT->bTeam=0;
			INT->ubID=INT->ubID - ubID_prefix;

			for(int i=0; i <= INT->gubOutOfTurnPersons; i++)//this loop translates soldier id's from what they are in someone else's game to what they are locally
			{
				if((INT->gubOutOfTurnOrder[i] >= ubID_prefix) && (INT->gubOutOfTurnOrder[i] < (ubID_prefix+6)))
				{
					INT->gubOutOfTurnOrder[i]=INT->gubOutOfTurnOrder[i]-ubID_prefix;
				}
			}
			memcpy(gubOutOfTurnOrder,INT->gubOutOfTurnOrder, sizeof(UINT8) * MAXMERCS);
			gubOutOfTurnPersons = INT->gubOutOfTurnPersons;


			AddTopMessage( PLAYER_INTERRUPT_MESSAGE, TeamTurnString[ INT->bTeam ] );
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Interrupt of %s awarded to %s.", TeamNameStrings[pOpponent->bTeam], TeamNameStrings[INT->bTeam] );

		}

		// WANNE - MP: This seems to cause the HANG on AI interrupt where we have to press ALT + E on the server!
		if(	INT->bTeam != 0)//not for our team - hayden
		{
			
			//stop moving merc who was interrupted and init UI bar
			SOLDIERTYPE* pMerc = SafeMerc( INT->ubID.i );	// C1: wire id, clamp before deref
			if ( pMerc == NULL )
				return;
			pMerc->HaultSoldierFromSighting(TRUE);
			FreezeInterfaceForEnemyTurn();
			InitEnemyUIBar( 0, 0 );
			fInterfacePanelDirty = DIRTYLEVEL2;
			gTacticalStatus.fInterruptOccurred = TRUE;

			//this needed to add details of who's interrupt it is - hayden
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Interrupt with %s awarded to %s.", TeamNameStrings[pOpponent->bTeam], TeamNameStrings[INT->bTeam] );//was MPClientMessage[17], can be reconnected if text updated and translated
		}
		else
		{
			//it for us ! :)
			if(INT->gubOutOfTurnPersons==0)//indicates finished interrupt maybe can just call end interrupt
			{
				//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"old int finish" );
			}
			else //start our interrupt turn
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Interrupt of %s awarded to you.", TeamNameStrings[pOpponent->bTeam] );//was MPClientMessage[37], can be reconnected if text updated and translated

				SOLDIERTYPE* pSoldier = SafeMerc( INT->ubID.i );	// C1: wire id, clamp before deref
				if ( pSoldier == NULL )
					return;
				ManSeesMan(pSoldier,pOpponent,pOpponent->sGridNo,pOpponent->pathing.bLevel,2,1);
				StartInterrupt();
			}
		}
	}

#endif

void intAI (SOLDIERTYPE *pSoldier )
{
	//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"intAI with %s", TeamNameStrings[pSoldier->bTeam] );//was MPClientMessage[17], can be reconnected if text updated and translated
	AddTopMessage( COMPUTER_INTERRUPT_MESSAGE, TeamTurnString[ pSoldier->bTeam ] );
	gTacticalStatus.fInterruptOccurred = TRUE;		
}

void end_interrupt ( BOOLEAN fMarkInterruptOccurred )
{
	// C1: gubOutOfTurnOrder[] is populated from wire interrupt data; clamp the index
	// soldier before dereferencing MercPtrs[] to build the release packet.
	if ( gubOutOfTurnPersons >= MAXMERCS )
		return;
	SOLDIERTYPE * pSoldier = SafeMerc( gubOutOfTurnOrder[gubOutOfTurnPersons] );
	if ( pSoldier == NULL )
		return;

	INT_STRUCT INT;
	INT.ubID = pSoldier->ubID;
	INT.bTeam = pSoldier->bTeam;
	memcpy(INT.gubOutOfTurnOrder, gubOutOfTurnOrder, sizeof(UINT16) * MAXMERCS);
	INT.gubOutOfTurnPersons = gubOutOfTurnPersons;
	INT.fMarkInterruptOccurred=fMarkInterruptOccurred;
	if(is_server)Sawarded=false;

	if(INT.bTeam==0)
	{
		INT.bTeam=netbTeam;
		INT.ubID=INT.ubID+ubID_prefix;
	}

	for(int i=0; i <= INT.gubOutOfTurnPersons; i++)
	{
		if(INT.gubOutOfTurnOrder[i] < 20)
		{
			INT.gubOutOfTurnOrder[i]=INT.gubOutOfTurnOrder[i]+ubID_prefix;
		}
	}

	// PORTABLE WIRE FORMAT (L6): serialize header + consumed order entries only.
	uint8_t wire[INT_WIRE_MAX_BYTES];
	int wireBytes = SerializeINT( wire, sizeof(wire), INT );
	client->RPC("endINTERRUPT",(const char*)wire, wireBytes*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void resume_turn(RPCParameters *rpcParameters)
{
	// PORTABLE WIRE FORMAT (L6): deserialize the fixed-width interrupt payload
	// (bounds-checked, gubOutOfTurnPersons clamped). Drop short/bad frames.
	INT_STRUCT _intWire;
	if ( !DeserializeINT( _intWire, rpcParameters->input, (rpcParameters->numberOfBitsOfData + 7) / 8 ) )
		return;
	INT_STRUCT* INT = &_intWire;

	// arbiter says the interrupt is over -> stop forcing the enemy-interrupt bar.
	// Cleared before EndInterrupt below so its InitPlayerUIBar(2) can restore green.
	gMpEnemyInterruptTeam = 0;

	if(is_server)
		Sawarded=false;
	
	if(INT->bTeam==netbTeam || (INT->bTeam==1 && is_server))//may need working //its for us or we are the server and its for the AI
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Resumed turn after interrupt of %s", TeamNameStrings[INT->bTeam] );//was MPClientMessage[18], can be reconnected if text updated and translated

		for(int i=0; i <= INT->gubOutOfTurnPersons; i++)
		{
			if((INT->gubOutOfTurnOrder[i] >= ubID_prefix) && (INT->gubOutOfTurnOrder[i] < (ubID_prefix+6)))
			{
				INT->gubOutOfTurnOrder[i]=INT->gubOutOfTurnOrder[i]-ubID_prefix;
			}
		}

		memcpy(gubOutOfTurnOrder,INT->gubOutOfTurnOrder, sizeof(UINT16) * MAXMERCS);
		gubOutOfTurnPersons = INT->gubOutOfTurnPersons;
		EndInterrupt( INT->fMarkInterruptOccurred );
	}
	// WANNE - MP: This happens, when client 1 (=server) has done its interrupt and now it is enemies turn!
	else if(INT->bTeam==1)
	{
		//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"im not server but ai just got back its turn after being interrupted..." );
		AddTopMessage( COMPUTER_TURN_MESSAGE, TeamTurnString[ 1 ] );
	}
}

void grid_display ( void ) //print mouse coordinates, helpfull for crate placement.
{
	INT16	sGridX, sGridY;
	UINT32 usMapPos;

	GetMouseXY( &sGridX, &sGridY );
	usMapPos = MAPROWCOLTOPOS( sGridY, sGridX );
	
	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, MPClientMessage[19] );
	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, MPClientMessage[20], sGridX, sGridY );
	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, MPClientMessage[21], usMapPos );
}

void overide_callback( UINT8 ubResult )
{
	if(is_server)
	{
		if(ubResult==1)
		{
			allowlaptop=true;
		}

		if(ubResult==2)//overide stop #1 (awaiting client ready for launch/load)
		{	
			goahead=0;
			status=0;//reset
			numready=0;
			SOLDIERTYPE *pSoldier = MercPtrs[ 0 ];
			UINT8 ubGroupID = pSoldier->ubGroupID;

			GROUP *pGroup;
			pGroup = GetGroup( ubGroupID ); 
			gpBattleGroup = pGroup;
			gubPBSectorX = gpBattleGroup->ubSectorX;
			gubPBSectorY = gpBattleGroup->ubSectorY;
			gubPBSectorZ = gpBattleGroup->ubSectorZ;

			gfEnterTacticalPlacementGUI = 1;
			goahead=1;
			readystage=1;
			ready_struct info; //send
			info.client_num = CLIENT_NUM;
			info.ready_stage = 1;
			info.status = 1; 		
			
			client->RPC("sendREADY",(const char*)&info, (int)sizeof(ready_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
	
			status=0;//reset
			numready=0;
		
			SetCurrentWorldSector( gubPBSectorX, gubPBSectorY, gubPBSectorZ ); //load
		}
		
		if(ubResult==3)
		{
			lockui(1);//unlock ui paused while wainting for loaders

			ready_struct info;
			info.client_num = CLIENT_NUM;
			readystage=0;
			numready=0;
			info.ready_stage = 2;//done placing mercs
			info.status=1;
			client->RPC("sendGUI",(const char*)&info, (int)sizeof(ready_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
		}

		if(ubResult==4) //overide waiting on merc placement
		{
			numready=0;

			ready_struct info; //send
			info.client_num = CLIENT_NUM;
			info.ready_stage = 4;//all done placing mercs, kill all
			info.status=1;
			gMsgBox.bHandled = MSG_BOX_RETURN_OK;
			status=0;
			KillTacticalPlacementGUI(); //kill
			ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, MPClientMessage[13]);

			client->RPC("sendGUI",(const char*)&info, (int)sizeof(ready_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
		}
	
		goahead = 0;
		numready = 0;
		readystage = 0;
		status = 0;
	}
}

void requestFILE_TRANSFER_SETTINGS(void)
{
	client->RPC("requestFILE_TRANSFER_SETTINGS","", 0, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

// OJW - 20090430
// give clients a choice to accept the security risk
void allowDownloadCallback( UINT8 ubResult )
{
	if (ubResult == 2)
	{
		// yes
		// begin downloading of files
		setID = fltClient.SetupReceive(&transferCBClient, false, serverAddr);

		char buffer[3];
		sprintf(buffer, "%i", setID);

		client->RPC("receiveSETID", (const char*) buffer, (int)sizeof(char*), HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
	}
	else
	{
		// no
		// gracefully disconnect to the main menu
		client_disconnect();
		guiPendingScreen = MP_JOIN_SCREEN;
	}
}

// THIS METHOD IS CALLED FROM THE SERVER WHENEVER A NEW CLIENT CONNECTS
void requestSETID(RPCParameters *rpcParameters)
{
	// WANNE: FILE TRANSFER
	if (!is_server)
	{
		if (is_connected) // added this here for version disconnections
		{
			// We did not recieved the files, get them!
			if (!fClientReceivedAllFiles)
			{
				serverAddr = rpcParameters->sender;
				
				SGPRect CenteringRect= {0 + xResOffset, 0 + yResOffset, SCREEN_WIDTH - xResOffset, SCREEN_HEIGHT - yResOffset };
				DoMessageBox( MSG_BOX_BASIC_STYLE , MPClientMessage[64] , guiCurrentScreen, MSG_BOX_FLAG_YESNO | MSG_BOX_FLAG_USE_CENTERING_RECT, allowDownloadCallback, &CenteringRect );
			}
		}
	}
}

void requestSETTINGS(void)
{
	client_info cl_name;
	strcpy(cl_name.client_name , cClientName);
	cl_name.team = TEAM;
	cl_name.cl_edge = cStartingSectorEdge;

	// OJW - 20090507
	// send client version to server
	strcpy(cl_name.client_version,MPVERSION);
	client->RPC("requestSETTINGS",(const char*)&cl_name, (int)sizeof(client_info)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

// OJW: FILE TRANSFER: Clients get notified of other clients transfer progress
void recieveDOWNLOADSTATUS(RPCParameters *rpcParameters)
{
	progress_struct* prog = (progress_struct*)rpcParameters->input;
	int i = prog->client_num - 1;

	if (client_downloading[i] != prog->downloading)
	{
		if (prog->downloading == 0)
			ScreenMsg( FONT_RED, MSG_MPSYSTEM, (is_server? MPServerMessage[11] : MPClientMessage[61]), client_names[i]); // message client has recieved all files
		else
			ScreenMsg( FONT_RED, MSG_MPSYSTEM, (is_server? MPServerMessage[12] : MPClientMessage[62]), client_names[i]);// send message client has started downloading files
	}

	client_downloading[i] = prog->downloading;
	client_progress[i] = prog->progress;
	
	fDrawCharacterList = true;
}

// WANNE: FILE TRANSFER: Get executable Directory from Server. This is used to get corret file location on client side
//
// DESIGN NOTE (so this isn't re-misdiagnosed as the cause of a laptop "Downloading"
// freeze): this is a CONNECT-TIME handshake -- requestFILE_TRANSFER_SETTINGS() is sent
// exactly once, from ID_CONNECTION_REQUEST_ACCEPTED, NOT when the laptop / AIM / Bobby
// Ray opens. And syncClientsDirectory == 0 is the INTENTIONAL "no file sync, no download
// dialog" mode, not a failure: we record the value and return; only == 1 does any sync.
// There is no "0 == fatal VFS error" path and no connect semaphore left dangling here.
void recieveFILE_TRANSFER_SETTINGS (RPCParameters *rpcParameters)
{
	if (!is_server && recieved_transfer_settings == 0)
	{
		filetransfersettings_struct* fts = (filetransfersettings_struct*)rpcParameters->input;

		gCurrentTransferBytes = 0;
		gTotalTransferBytes = fts->totalTransferBytes;

		// Now get directory
		strcpy( server_fileTransferDirectoryPath, fts->fileTransferDirectory );
		vfs::Path profileRoot = vfs::Path(gzFileTransferDirectory) + vfs::Path(server_fileTransferDirectoryPath);

		/////////////////////////////////////////////////////////////////////
		SGP_TRYCATCH_RETHROW( ja2::mp::InitializeMultiplayerProfile(profileRoot), L"" );
		/////////////////////////////////////////////////////////////////////

		recieved_transfer_settings = 1;
		serverSyncClientsDirectory = fts->syncClientsDirectory;

		// We sync our MP game dir from the server
		if (fts->syncClientsDirectory == 1)
		{
			// profile is setup, nothing to do here
		}
	}
}

void recieveSETTINGS (RPCParameters *rpcParameters) //recive settings from server
{
	int startingEdge = MP_EDGE_NORTH;

	settings_struct* cl_lan = (settings_struct*)rpcParameters->input;
	if ( cl_lan->client_num < 1 || cl_lan->client_num > 4 )
	{
		return;	// client_names[4] / Team[6..9] only valid for 1..4 (audit [36])
	}

	char szDefault[30];
	cl_lan->client_name[sizeof(cl_lan->client_name)-1] = 0;
	snprintf(szDefault, sizeof(szDefault), "%s", cl_lan->client_name);

	// OJW - 20081204
	// get complete client data from the server
	memcpy( client_edges, cl_lan->client_edges , sizeof(int) * 5);
	memcpy( client_teams, cl_lan->client_teams , sizeof(int) * 4);

	if(!recieved_settings && strcmp(cl_lan->client_name, cClientName)==0)
	{
		// This settings packet contains information and settings specifically for us
		recieved_settings=1;

		// save the settings so if we need to re-apply them after a reinitialisation ( after file transfer ) we can
		memcpy ( &gMPServerSettings , cl_lan , sizeof(settings_struct) );

		CLIENT_NUM=cl_lan->client_num;//assign client number from server

		netbTeam = (CLIENT_NUM)+5;
		ubID_prefix = gTacticalStatus.Team[ netbTeam ].bFirstID;//over here now

		{
			// if this client carries an ADMIN_PASSWORD in its own ja2_mp.ini, offer
			// it to claim admin on a password-protected server (sent once)
			static bool fSentAuth = false;
			if ( !fSentAuth )
			{
				fSentAuth = true;
				CIniReader adminIni(JA2MP_INI_FILENAME);
				const char* pw = adminIni.ReadString(JA2MP_INI_INITIAL_SECTION, JA2MP_ADMIN_PASSWORD, "");
				if ( pw && pw[0] != 0 )
					send_admin_cmd( ADMIN_CMD_AUTH, pw );
			}
		}

		memcpy( client_names , cl_lan->client_names, sizeof( char ) * 4 * 30 );
		
		if ( cl_lan->client_num >= 1 && cl_lan->client_num <= 4 )
			strcpy(client_names[cl_lan->client_num-1],szDefault);

		// OJW - 20081204
		// server_name arrives off the wire (untrusted); bound to cServerName[30] + NUL.
		sgp_strlcpy(cServerName, cl_lan->server_name);
		gRandomMercs = cl_lan->randomMercs;
		gRandomStartingEdge = cl_lan->randomStartingEdge;

		cStartingSectorEdge = cl_lan->startingSectorEdge;

		if(gRandomMercs)
		{
			// copy the random merc list locally
			memcpy(random_mercs,cl_lan->random_mercs,sizeof(int) * 7);
		}

		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[2] );
		

		cMaxClients=cl_lan->maxClients;
		memcpy( cKitBag, cl_lan->kitBag, sizeof(char)*100);
		
		int cnt;
		int numnum = 0;
		int kitnum = 0;
		char tempstring[8];
	memset( tempstring, 0, sizeof( tempstring ) );	// first token must parse from a clean buffer
		memset( &kit, 0, sizeof( int )*20 );
	
		if (strcmp(cKitBag, "") != 0)
		{
			for(cnt=0;  cnt < 100; cnt++)
			{
				char tempc = cKitBag[cnt];
				
				if( _strnicmp(&tempc, "[",1) == 0)
				{
					
					continue;
				}
				
				else if( _strnicmp(&tempc, ",",1) == 0)
				{
					numnum=0;

					if (kitnum < 20) kit[kitnum]=atoi(tempstring);
				memset( tempstring, 0, sizeof( tempstring ) );

					memset( tempstring, 0, sizeof( tempstring ) );

					kitnum++;
					continue;
				}
			
				else if( _strnicmp(&tempc, "]",1) == 0)
				{
					if (kitnum < 20) kit[kitnum]=atoi(tempstring);
					break;
				}
				else
				{
				if (numnum < (int)sizeof(tempstring) - 1)
					strncpy(&tempstring[numnum],&tempc,1);
				else
					continue;	// drop excess digits
				numnum++;
				}
			}
		}

		cDamageMultiplier=cl_lan->damageMultiplier;
		cSameMercAllowed=cl_lan->sameMercAllowed;
		

		gsMercArriveSectorX = gGameExternalOptions.ubDefaultArrivalSectorX = cl_lan->gsMercArriveSectorX;
		gsMercArriveSectorY = gGameExternalOptions.ubDefaultArrivalSectorY = cl_lan->gsMercArriveSectorY;


		// WANNE - BMP: We have to initialize the map size here!!
		InitializeWorldSize(gsMercArriveSectorX, gsMercArriveSectorY, 0);

		cGameType = cl_lan->gameType;
		cDisableMorale=cl_lan->disableMorale;
		ChangeSelectedMapSector( gsMercArriveSectorX, gsMercArriveSectorY, 0 );
		CHAR16 str[128];
		GetSectorIDString( gsMercArriveSectorX, gsMercArriveSectorY, 0, str, TRUE );
	
		// WANNE - MP: If time set to 0, then no time turns
		INT32 secs_per_tick;
		if (cl_lan->secondsPerTick == 0)
		{
			gGameOptions.fTurnTimeLimit=FALSE;
			secs_per_tick=1;
		}
		else
		{
			gGameOptions.fTurnTimeLimit=cl_lan->sofTurnTimeLimit;
			secs_per_tick=cl_lan->secondsPerTick;
		}
		
		PLAYER_TEAM_TIMER_SEC_PER_TICKS=secs_per_tick;

		INT32 clstarting_balance=cl_lan->startingCash;//set starting balance

		if(LaptopSaveInfo.iCurrentBalance<clstarting_balance)
		{
			AddTransactionToPlayersBook( ANONYMOUS_DEPOSIT, 0, GetWorldTotalMin(), clstarting_balance-LaptopSaveInfo.iCurrentBalance );
		}
		else
		{
			AddTransactionToPlayersBook( TRANSACTION_FEE, 0, GetWorldTotalMin(), clstarting_balance-LaptopSaveInfo.iCurrentBalance );
		}

		gGameOptions.fGunNut=cl_lan->sofGunNut;	
		gGameOptions.ubGameStyle=cl_lan->soubGameStyle;
		gGameOptions.ubDifficultyLevel=cl_lan->soubDifficultyLevel;
		gGameOptions.fNewTraitSystem = cl_lan->soubSkillTraits;
		gGameOptions.fIronManMode=cl_lan->sofIronManMode;
		gGameOptions.ubBobbyRayQuality=cl_lan->soubBobbyRayQuality;
		gGameOptions.ubBobbyRayQuantity=cl_lan->soubBobbyRayQuantity;

		// Set Bobby Ray "Under Construction"?
		if(!cl_lan->disableBobbyRay)
		{
			SetBookMark( BOBBYR_BOOKMARK );
			LaptopSaveInfo.fBobbyRSiteCanBeAccessed = TRUE;
		}

		// Enable "AIM" and "MERC" only if random merc is false!
		if (!cl_lan->randomMercs)
		{
			SetBookMark( AIM_BOOKMARK );
			SetBookMark( MERC_BOOKMARK );
		}

		if(!cl_lan->disableMercEquipment)
			cAllowMercEquipment = 1;

		cMaxMercs = cl_lan->maxMercs;		
		cReportHiredMerc = cl_lan->reportHiredMerc;

		// WANNE: Removed
		/*
		ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, MPClientMessage[24],str,cMaxClients,cMaxMercs,cGameType,cSameMercAllowed,cDamageMultiplier,gGameOptions.fTurnTimeLimit,secs_per_tick,cl_lan->disableBobbyRay,cl_lan->disableMercEquipment,cDisableMorale,0);		
		*/
	
		// WANNE - MP: I just added the NUM_SEC_IN_DAY so the game starts at Day 1 instead of Day 0
		gGameExternalOptions.iGameStartingTime= NUM_SEC_IN_DAY + int(cl_lan->startingTime*3600);
		// WANNE - MP: In mulitplayer the hired merc arrive immediatly, so iFirstArrivalDelay must be set to 0
		// This also fixed the bug of the wrong hired hours!
		gGameExternalOptions.iFirstArrivalDelay = 0;

		// Disable Reinforcements
		gGameExternalOptions.gfAllowReinforcements				= false;
		gGameExternalOptions.gfAllowReinforcementsOnlyInCity	= false;
				
		gGameSettings.fOptions[TOPTION_ALLOW_REAL_TIME_SNEAK] = false;
		gGameExternalOptions.fQuietRealTimeSneak = false;
		
		// WANNE: fix HOT DAY in night at arrival by night.
		// Explanation: If game starting time + first arrival delay < 07:00 (111600) -> we arrive before the sun rises or
		// if game starting time + first arrival delay >= 21:00 (162000) -> we arrive after the sun goes down
		if( (gGameExternalOptions.iGameStartingTime + gGameExternalOptions.	iFirstArrivalDelay) < 111600 ||
			(gGameExternalOptions.iGameStartingTime + gGameExternalOptions.iFirstArrivalDelay >= 162000))
		{ 
			// Default: Night
			gubEnvLightValue = 12; 
		}
		else
		{
			// Default: Day
			gubEnvLightValue = 3;
		}

		LightSetBaseLevel(gubEnvLightValue); 

		InitNewGameClock( );
					
		cWeaponReadyBonus = cl_lan->weaponReadyBonus;
		cDisableSpectatorMode = cl_lan->disableSpectatorMode;
		
		// -----------------
		// RW: Inventory
		switch (cl_lan->inventoryAttachment)
		{
			case 0:	// Old/Old
				gGameOptions.ubInventorySystem = INVENTORY_OLD;
				gGameOptions.ubAttachmentSystem = ATTACHMENT_OLD;
				break;
			case 1:	// New/Old
				gGameOptions.ubInventorySystem = INVENTORY_NEW;
				gGameOptions.ubAttachmentSystem = ATTACHMENT_OLD;
				break;
			case 2:	// New/New
				gGameOptions.ubInventorySystem = INVENTORY_NEW;
				gGameOptions.ubAttachmentSystem = ATTACHMENT_NEW;
				break;
		}

		gGameOptions.ubSquadSize = 6;
		
		// Server forces us to play with new Inventory, but NIV is not allowed on the client,
		// because of wrong resolution or other stuff
		if ( UsingNewInventorySystem() == true && !IsNIVModeValid(true) )
		{
			SGPRect CenteringRect= {0 + xResOffset, 0 + yResOffset, SCREEN_WIDTH-1 - 2 * xResOffset, SCREEN_HEIGHT-1 - 2 * yResOffset };
			DoMessageBox( MSG_BOX_BASIC_STYLE , MPClientMessage[69] , guiCurrentScreen, MSG_BOX_FLAG_OK | MSG_BOX_FLAG_USE_CENTERING_RECT, InvalidClientSettingsOkBoxCallback, &CenteringRect );
		}
		else
		{
			// WANNE - MP: We have to re-initialize the correct interface
			// Have to initialize map UI Coordinates, because inventory panel layout location depends on them.
			initMapViewAndBorderCoordinates();
			if((UsingNewInventorySystem() == true))
			{
				InitNewInventorySystem();
				InitializeSMPanelCoordsNew();
				InitializeInvPanelCoordsNew();
			}
			else
			{
				gGameOptions.ubInventorySystem = INVENTORY_OLD;
				InitOldInventorySystem();
				InitializeSMPanelCoordsOld();
				InitializeInvPanelCoordsOld();
			}
			
			// WANNE - MP: We also have to reinitialize the merc profiles because
			// they depend on the inventory!
			LoadMercProfiles();

			InitializeFaceGearGraphics();

			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[26],cl_lan->client_num,szDefault );
			
			fDrawCharacterList = true; // set the character list to be redrawn
	
			if ( cl_lan->client_num >= 1 && cl_lan->client_num <= 4 )
				strcpy(client_names[cl_lan->client_num-1],szDefault);

			// OJW - 20091024 - extract random table
			if (!is_server)
				memcpy(guiPreRandomNums,cl_lan->random_table,sizeof(UINT32)*MAX_PREGENERATED_NUMS);			

			// WANNE: Turn on airspace mode (to switch maps) for the server!
			if (is_server)			
				TurnOnAirSpaceMode();			
		}
	}
	else 
	{
		fDrawCharacterList = true; // set the character list to be redrawn

		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[26],cl_lan->client_num,szDefault );
			
		if ( cl_lan->client_num >= 1 && cl_lan->client_num <= 4 )
			strcpy(client_names[cl_lan->client_num-1],szDefault);				
	}
}

void reapplySETTINGS()
{
	// reapply some settings that would be lost if a game is re-initialised
	// after joining ( from downloading files )

	// Stuff from connect_client()
	//**********************
	//here some nifty little tweaks
	LaptopSaveInfo.guiNumberOfMercPaymentsInDays += 20;
	LaptopSaveInfo.gubLastMercIndex = LAST_MERC_ID;
	
	LaptopSaveInfo.ubLastMercAvailableId = 7;
	gGameExternalOptions.fEnableSlayForever	=1;
	LaptopSaveInfo.gubPlayersMercAccountStatus = 4;
		
	// WANNE: This should fix the bug playing a "tilt" sound and showing the mini laptop graphic on the screen, when opening the laptop / option screen from map screen
	gfDontStartTransitionFromLaptop = TRUE;

	gMercProfiles[ 57 ].sSalary = 2000;
	gMercProfiles[ 58 ].sSalary = 1500;
	gMercProfiles[ 59 ].sSalary = 600;
	gMercProfiles[ 60 ].sSalary = 500;
	gMercProfiles[ 64 ].sSalary = 1500;
	gMercProfiles[ 72 ].sSalary = 1000;
	gMercProfiles[ 148 ].sSalary = 100;
	gMercProfiles[ 68 ].sSalary = 2200; //iggy;

	// Stuff from recieveSETTINGS
	gsMercArriveSectorX = gGameExternalOptions.ubDefaultArrivalSectorX = gMPServerSettings.gsMercArriveSectorX;
	gsMercArriveSectorY = gGameExternalOptions.ubDefaultArrivalSectorY = gMPServerSettings.gsMercArriveSectorY;

	cGameType=gMPServerSettings.gameType;
	cDisableMorale=gMPServerSettings.disableMorale;
	ChangeSelectedMapSector( gsMercArriveSectorX, gsMercArriveSectorY, 0 );
	CHAR16 str[128];
	GetSectorIDString( gsMercArriveSectorX, gsMercArriveSectorY, 0, str, TRUE );
	
	// WANNE - MP: If time set to 0, then no time turns
	INT32 secs_per_tick;
	if (gMPServerSettings.secondsPerTick == 0)
	{
		gGameOptions.fTurnTimeLimit=FALSE;
		secs_per_tick=1;
	}
	else
	{
		gGameOptions.fTurnTimeLimit=gMPServerSettings.sofTurnTimeLimit;
		secs_per_tick=gMPServerSettings.secondsPerTick;
	}
	
	PLAYER_TEAM_TIMER_SEC_PER_TICKS=secs_per_tick;

	INT32 clstarting_balance=gMPServerSettings.startingCash;//set starting balance

	if(LaptopSaveInfo.iCurrentBalance<clstarting_balance)
	{
		AddTransactionToPlayersBook( ANONYMOUS_DEPOSIT, 0, GetWorldTotalMin(), clstarting_balance-LaptopSaveInfo.iCurrentBalance );
	}
	else
	{
		AddTransactionToPlayersBook( TRANSACTION_FEE, 0, GetWorldTotalMin(), clstarting_balance-LaptopSaveInfo.iCurrentBalance );
	}

	gGameOptions.fGunNut=gMPServerSettings.sofGunNut;	
	gGameOptions.ubGameStyle=gMPServerSettings.soubGameStyle;
	gGameOptions.ubDifficultyLevel=gMPServerSettings.soubDifficultyLevel;
	gGameOptions.fNewTraitSystem = gMPServerSettings.soubSkillTraits;
	gGameOptions.fIronManMode=gMPServerSettings.sofIronManMode;
	gGameOptions.ubBobbyRayQuality=gMPServerSettings.soubBobbyRayQuality;
	gGameOptions.ubBobbyRayQuantity=gMPServerSettings.soubBobbyRayQuantity;

	// Set Bobby Ray "Under Construction"?
	if(!gMPServerSettings.disableBobbyRay)
	{
		SetBookMark( BOBBYR_BOOKMARK );
		LaptopSaveInfo.fBobbyRSiteCanBeAccessed = TRUE;
	}

	// Enable "AIM" and "MERC" only if random merc is false!
	if (!gMPServerSettings.randomMercs)
	{
		SetBookMark( AIM_BOOKMARK );
		SetBookMark( MERC_BOOKMARK );
	}

	if(!gMPServerSettings.disableMercEquipment)
		cAllowMercEquipment = 1;

	cMaxMercs = gMPServerSettings.maxMercs;	
	cReportHiredMerc = gMPServerSettings.reportHiredMerc;

	// WANNE - MP: I just added the NUM_SEC_IN_DAY so the game starts at Day 1 instead of Day 0
	gGameExternalOptions.iGameStartingTime= NUM_SEC_IN_DAY + int(gMPServerSettings.startingTime*3600);

	// WANNE - MP: In mulitplayer the hired merc arrive immediatly, so iFirstArrivalDelay must be set to 0
	// This also fixed the bug of the wrong hired hours!
	gGameExternalOptions.iFirstArrivalDelay = 0;

	// Disable Reinforcements
	gGameExternalOptions.gfAllowReinforcements				= false;
	gGameExternalOptions.gfAllowReinforcementsOnlyInCity	= false;
	
	// Disable Real-Time Mode
	// SANDRO - real-time sneak is in preferences
	//gGameExternalOptions.fAllowRealTimeSneak = false;
	gGameSettings.fOptions[TOPTION_ALLOW_REAL_TIME_SNEAK] = false;
	gGameExternalOptions.fQuietRealTimeSneak = false;
	
	// WANNE: fix HOT DAY in night at arrival by night.
	// Explanation: If game starting time + first arrival delay < 07:00 (111600) -> we arrive before the sun rises or
	// if game starting time + first arrival delay >= 21:00 (162000) -> we arrive after the sun goes down
	if( (gGameExternalOptions.iGameStartingTime + gGameExternalOptions.	iFirstArrivalDelay) < 111600 ||
		(gGameExternalOptions.iGameStartingTime + gGameExternalOptions.iFirstArrivalDelay >= 162000))
	{ 
		// Default: Night
		gubEnvLightValue = 12; 
	}
	else
	{
		// Default: Day
		gubEnvLightValue = 3;
	}

	LightSetBaseLevel(gubEnvLightValue); 

	InitNewGameClock( );
				
	cWeaponReadyBonus = gMPServerSettings.weaponReadyBonus;
	cDisableSpectatorMode = gMPServerSettings.disableSpectatorMode;
	
	// -----------------
	// RW: Inventory
	switch (gMPServerSettings.inventoryAttachment)
	{
		case 0:	// Old/Old
			gGameOptions.ubInventorySystem = INVENTORY_OLD;
			gGameOptions.ubAttachmentSystem = ATTACHMENT_OLD;
			break;
		case 1:	// New/Old
			gGameOptions.ubInventorySystem = INVENTORY_NEW;
			gGameOptions.ubAttachmentSystem = ATTACHMENT_OLD;
			break;
		case 2:	// New/New
			gGameOptions.ubInventorySystem = INVENTORY_NEW;
			gGameOptions.ubAttachmentSystem = ATTACHMENT_NEW;
			break;
	}

	gGameOptions.ubSquadSize = 6;
	
	// WANNE - MP: We have to re-initialize the correct interface
	// Have to initialize map UI Coordinates, because inventory panel layout location depends on them.
	initMapViewAndBorderCoordinates();
	if((UsingNewInventorySystem() == true) && IsNIVModeValid(true))
	{
		InitNewInventorySystem();
		InitializeSMPanelCoordsNew();
		InitializeInvPanelCoordsNew();
	}
	else
	{
		gGameOptions.ubInventorySystem = INVENTORY_OLD;
		InitOldInventorySystem();
		InitializeSMPanelCoordsOld();
		InitializeInvPanelCoordsOld();
	}

	// WANNE - MP: We also have to reinitialize the merc profiles because
	// they depend on the inventory!
	LoadMercProfiles();

	InitializeFaceGearGraphics();
	
	fDrawCharacterList = true; // set the character list to be redrawn
}

void recieveTEAMCHANGE( RPCParameters *rpcParameters )
{
	RPC_REQUIRE_BYTES(rpcParameters, teamchange_struct);	// short-frame guard (H9/H13)
	teamchange_struct* cl_lan = (teamchange_struct*)rpcParameters->input;
	// H9: client_num indexes client_teams[4] as [client_num-1] -- bound it.
	if ( cl_lan->client_num < 1 || cl_lan->client_num > 4 )
		return;

	if (!can_teamchange())
	{
		// error
		ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, L"An error occured in recieveTEAMCHANGE that should not occur");
	}
	else
	{
		// redraw the character list on the map screen
		fTeamPanelDirty = true; 
		fDrawCharacterList = true;
		client_teams[cl_lan->client_num-1] = cl_lan->newteam;
		if (cl_lan->client_num == CLIENT_NUM)
		{
			TEAM = cl_lan->newteam;
		}
	}
}

void recieveEDGECHANGE( RPCParameters *rpcParameters )
{
	RPC_REQUIRE_BYTES(rpcParameters, edgechange_struct);	// short-frame guard (H9/H13)
	edgechange_struct* cl_lan = (edgechange_struct*)rpcParameters->input;
	// H9: client_num indexes client_edges[5] as [client_num-1] -- bound it.
	if ( cl_lan->client_num < 1 || cl_lan->client_num > 5 )
		return;

	if (!can_edgechange())
	{
		// error
		ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, L"An error occured in recieveEDGECHANGE that should not occur");
	}
	else
	{
		// redraw the character list on the map screen
		fTeamPanelDirty = true;
		fDrawCharacterList = true;
		client_edges[cl_lan->client_num-1] = cl_lan->newedge;
		if (cl_lan->client_num == CLIENT_NUM)
		{
			// store the setting locally
			cStartingSectorEdge = cl_lan->newedge;
		}
	}
}

void recieveMAPCHANGE( RPCParameters *rpcParameters )
{
	mapchange_struct* cl_lan = (mapchange_struct*)rpcParameters->input;

	if (!is_client || allowlaptop)
	{
		// error
		ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, L"An error occured in recieveMAPCHANGE that should not occur");
	}
	else
	{
		// copy new map settings locally
		gsMercArriveSectorX = gGameExternalOptions.ubDefaultArrivalSectorX = cl_lan->gsMercArriveSectorX;
		gsMercArriveSectorY = gGameExternalOptions.ubDefaultArrivalSectorY = cl_lan->gsMercArriveSectorY;


		ChangeSelectedMapSector( gsMercArriveSectorX, gsMercArriveSectorY, 0 );

		// WANNE - BMP:
		InitializeWorldSize( gsMercArriveSectorX, gsMercArriveSectorY, 0 );

		gGameExternalOptions.iGameStartingTime= NUM_SEC_IN_DAY + int(cl_lan->startingTime*3600);

		CHAR16 str[128];
		GetSectorIDString( gsMercArriveSectorX, gsMercArriveSectorY, 0, str, TRUE );

		// notify clients of map change in console
		ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, MPClientMessage[46],str);
	}
}

// 20091002 - OJW - Explosives
void send_grenade (OBJECTTYPE *pGameObj, float dLifeLength, float xPos, float yPos, float zPos, float xForce, float yForce, float zForce, UINT32 sTargetGridNo, SoldierID ubOwner, UINT8 ubActionCode, UINT32 uiActionData, INT32 iRealObjectID, bool bIsThrownGrenade)
{
	ubOwner = MPEncodeSoldierID(ubOwner); // translate our soldier to the "network" version

	SOLDIERTYPE* pSoldier = ubOwner;
	if (pSoldier != NULL)
	{
		if ((pSoldier->bTeam == 1 && is_server) || IsOurSoldier(pSoldier))
		{
			physics_object gren;
			gren.dForceX = xForce;
			gren.dForceY = yForce;
			gren.dForceZ = zForce;
			gren.dX = xPos;
			gren.dY = yPos;
			gren.dZ = zPos;
			gren.dLifeSpan = dLifeLength;
			gren.ubActionCode = ubActionCode;
			gren.uiActionData = uiActionData;
			gren.ubID = ubOwner;
			gren.usItem = pGameObj->usItem;
			gren.sTargetGridNo = sTargetGridNo;
			gren.RealObjectID = iRealObjectID;
			gren.IsThrownGrenade = bIsThrownGrenade;
			gren.uiPreRandomIndex = guiPreRandomIndex;

			// M12 - transmit the real thrown-object state so the receiver no longer
			// rebuilds it as CreateItem(usItem,99,...) (default status, default
			// magazine). This is the structural root of the "fires the wrong round
			// count"/phantom-item reports. objectStatus is the top-level union value
			// (status %); ubGunShotsLeft lives in the gun sub-struct (a different
			// offset) and matters for loaded shells thrown from a launcher.
			if ( (*pGameObj)[0] != NULL )
			{
				gren.sItemStatus = (*pGameObj)[0]->data.objectStatus;
				gren.usShotsLeft = (*pGameObj)[0]->data.gun.ubGunShotsLeft;
			}
			else
			{
				gren.sItemStatus = 99;
				gren.usShotsLeft = 0;
			}
			gren.ubNumberOfObjects = pGameObj->ubNumberOfObjects;

			// L1 - stamp a monotonic sequence so the receiver can reject
			// dropped/duplicated/reordered events that would drift guiPreRandomIndex.
			gren.uiGrenadeEventSeq = ++guiGrenadeEventSeqOut;

#ifdef JA2BETAVERSION
			CHAR tmpMPDbgString[512];
			sprintf(tmpMPDbgString,"MP - send_grenade ( usItem : %i , sGridNo : %i , ubSoldierID : %i , uiPreRandomIndex : %i )\n",gren.usItem, gren.sTargetGridNo , gren.ubID.i , guiPreRandomIndex );
			MPDebugMsg(tmpMPDbgString);
			gfMPDebugOutputRandoms = true;
#endif

			client->RPC("sendGRENADE",(const char*)&gren, (int)sizeof(physics_object)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
		}
	}
}

void recieveGRENADE (RPCParameters *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, physics_object);	// short-frame guard (M8/H13)
	physics_object* gren = (physics_object*)rpcParameters->input;

	gren->ubID = MPDecodeSoldierID(gren->ubID);

	SOLDIERTYPE* pThrower =  gren->ubID;
	if (pThrower != NULL)
	{
		// L1 - only adopt the sender's pre-random cursor for an in-order, not-yet-seen
		// event. A dropped event never advances guiLastGrenadeEventSeqIn, so a later
		// event still applies; a duplicate/stale event is ignored instead of silently
		// rewinding/advancing guiPreRandomIndex out of step with the RNG we consumed.
		if ( gren->uiGrenadeEventSeq > guiLastGrenadeEventSeqIn )
		{
			guiLastGrenadeEventSeqIn = gren->uiGrenadeEventSeq;
			guiPreRandomIndex = gren->uiPreRandomIndex;
		}

		// grenade wasnt thrown by one of our guys, so we should do it on the client
		if (!IsOurSoldier(pThrower) && (pThrower->bTeam != 1 || !is_server))
		{
#ifdef JA2BETAVERSION
			CHAR tmpMPDbgString[512];
			sprintf(tmpMPDbgString,"MP - recieveGRENADE ( usItem : %i , sGridNo : %i , ubSoldierID : %i , uiPreRandomIndex : %i )\n",gren->usItem, gren->sTargetGridNo , gren->ubID.i , guiPreRandomIndex );
			MPDebugMsg(tmpMPDbgString);
			gfMPDebugOutputRandoms = true;
#endif

			// M12 - short-term inventory/ammo replication: rebuild the thrown object
			// with the real status, round count and stack size sent over the wire,
			// instead of the old CreateItem(usItem,99,...) that always reconstructed
			// item defaults (the "fires the wrong round count"/phantom-item root).
			// The full periodic per-soldier inventory-delta sync is a follow-up.
			// M8: usItem indexes Item[] inside CreateItems -- drop out-of-range items.
			if ( gren->usItem == 0 || gren->usItem >= MAXITEMS )
				return;
			OBJECTTYPE* newObj = new OBJECTTYPE();
			INT16 sItemStatus = gren->sItemStatus;
			// guard against an old/corrupt peer sending a zeroed status
			if ( sItemStatus <= 0 || sItemStatus > 100 )
				sItemStatus = 99;
			UINT8 ubNumObjects = ( gren->ubNumberOfObjects > 0 ) ? gren->ubNumberOfObjects : 1;
			CreateItems( gren->usItem, sItemStatus, ubNumObjects, newObj );
			// Restore the loaded round count for launcher shells. ubGunShotsLeft lives
			// at a distinct offset in the data union, so it does not disturb the status
			// already written above; CreateItems leaves it at the item default.
			if ( (*newObj)[0] != NULL )
				(*newObj)[0]->data.gun.ubGunShotsLeft = gren->usShotsLeft;
			OBJECTTYPE::CopyToOrCreateAt(&pThrower->pTempObject, newObj);

			// M12 - broadcast item consumption: mirror the throwing client, which
			// removed the thrown stack from its hand (Weapons.cpp:5064). Without this
			// the remote copy of the thrower keeps a phantom grenade in hand. Only
			// touch the hand slot when it still holds the same item, so a diverged
			// inventory is left untouched rather than corrupted.
			if ( pThrower->inv[ HANDPOS ].exists() && pThrower->inv[ HANDPOS ].usItem == gren->usItem )
			{
				pThrower->inv[ HANDPOS ].RemoveObjectsFromStack( ubNumObjects );
			}
			// this will create a grenade and launch it
			INT32 i = CreatePhysicalObject( pThrower->pTempObject, gren->dLifeSpan, gren->dX, gren->dY, gren->dZ, gren->dForceX, gren->dForceY, gren->dForceZ, pThrower->ubID, gren->ubActionCode, gren->uiActionData, false);
			// M8: CreatePhysicalObject returns -1 on slot exhaustion -> ObjectSlots[-1]
			// OOB write. Drop the frame (free the temp object) rather than write past the array.
			if ( i < 0 || i >= NUM_OBJECT_SLOTS )
			{
				delete newObj;
				return;
			}
			// save extra state info so we can check and feed it result later
			ObjectSlots[ i ].mpRealObjectID = gren->RealObjectID;
			ObjectSlots[ i ].mpTeam = pThrower->bTeam;
			ObjectSlots[ i ].mpIsFromRemoteClient = true;
			ObjectSlots[ i ].mpHaveClientResult = false;
			ObjectSlots[ i ].mpWasDud = false;

			// Do grenade animation (todo fix this for mortars)
			if (gren->IsThrownGrenade)
			{
				{
					Assert(pThrower->pThrowParams == NULL);

					// not a mem leak
					// will be freed in AdjustToNextAnimationFrame(SOLDIERTYPE*), case 461
					pThrower->pThrowParams = (THROW_PARAMS*) malloc(sizeof(THROW_PARAMS));
					pThrower->pThrowParams->dForceX = gren->dForceX;
					pThrower->pThrowParams->dForceY = gren->dForceY;
					pThrower->pThrowParams->dForceZ = gren->dForceZ;
					pThrower->pThrowParams->dLifeSpan = gren->dLifeSpan;
					pThrower->pThrowParams->dX = gren->dX;
					pThrower->pThrowParams->dY = gren->dY;
					pThrower->pThrowParams->dZ = gren->dZ;
					pThrower->pThrowParams->ubActionCode = gren->ubActionCode;
					pThrower->pThrowParams->uiActionData = gren->uiActionData;
				}

				pThrower->usGrenadeItem = 0;
				HandleSoldierThrowItem( pThrower, gren->sTargetGridNo );
			}
		}
	}
	else
	{
#ifdef JA2BETAVERSION
		char tmpMsg[128];
		sprintf(tmpMsg,"ERROR! - Invalid Soldier pointer from ubID %i in recieveGRENADE()", gren->ubID.i);
		//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
		MPDebugMsg(tmpMsg);
#endif
	}
}

// we send a grenade result out to the clients as it may have been a fizzer
void send_grenade_result (float xPos, float yPos, float zPos, INT32 sGridNo, SoldierID ubOwnerID, INT32 iRealObjectID, bool bIsDud)
{
	ubOwnerID = MPEncodeSoldierID(ubOwnerID); // translate our soldier to the "network" version

	SOLDIERTYPE* pSoldier = ubOwnerID;
	if (pSoldier != NULL)
	{
		if ((pSoldier->bTeam == 1 && is_server) || IsOurSoldier(pSoldier))
		{
			grenade_result gres;
			gres.dX = xPos;
			gres.dY = yPos;
			gres.dZ = zPos;
			gres.sGridNo = sGridNo;
			gres.ubOwnerID = ubOwnerID;
			gres.RealObjectID = iRealObjectID;
			gres.bWasDud = bIsDud;
			gres.uiPreRandomIndex = guiPreRandomIndex;

#ifdef JA2BETAVERSION
			CHAR tmpMPDbgString[512];
			sprintf( tmpMPDbgString, "MP - send_grenade_result ( RealObjectID : %i , sGridNo : %i , ubSoldierID : %i , uiPreRandomIndex : %i )\n", gres.RealObjectID, gres.sGridNo, gres.ubOwnerID.i, guiPreRandomIndex );
			MPDebugMsg(tmpMPDbgString);
			gfMPDebugOutputRandoms = true;
#endif

			client->RPC("sendGRENADERESULT",(const char*)&gres, (int)sizeof(grenade_result)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
		}
	}
}

void recieveGRENADERESULT (RPCParameters *rpcParameters)
{
	grenade_result* gres = (grenade_result*)rpcParameters->input;

	gres->ubOwnerID = MPDecodeSoldierID(gres->ubOwnerID);

	SOLDIERTYPE* pThrower = gres->ubOwnerID;
	if (pThrower != NULL)
	{
		// grenade wasnt thrown by one of our guys, so we should do it on the client
		if (!IsOurSoldier(pThrower) && (pThrower->bTeam != 1 || !is_server))
		{
#ifdef JA2BETAVERSION
			CHAR tmpMPDbgString[512];
			sprintf( tmpMPDbgString, "MP - recieveGRENADERESULT ( RealObjectID : %i , sGridNo : %i , ubSoldierID : %i , uiPreRandomIndex : %i )\n", gres->RealObjectID, gres->sGridNo, gres->ubOwnerID.i, gres->uiPreRandomIndex );
			MPDebugMsg(tmpMPDbgString);
			gfMPDebugOutputRandoms = true;
#endif

			bool bFound = false;
			INT32 usCnt;
			// loop through and find the local object we assigned for the remote grenade
			for( usCnt=0; usCnt<NUM_OBJECT_SLOTS; usCnt++ )
			{
				if (ObjectSlots[ usCnt ].mpTeam == pThrower->bTeam && ObjectSlots[ usCnt ].mpRealObjectID == gres->RealObjectID)
				{
					bFound = true;
					break;
				}
			}

			if (bFound)
			{
				// override the local predictions with the ones from the client that threw it
				ObjectSlots[ usCnt ].mpHaveClientResult = true;
				ObjectSlots[ usCnt ].mpWasDud = gres->bWasDud;
				ObjectSlots[ usCnt ].Position.x = gres->dX;
				ObjectSlots[ usCnt ].Position.y = gres->dY;
				ObjectSlots[ usCnt ].Position.z = gres->dZ;
				ObjectSlots[ usCnt ].sGridNo = gres->sGridNo;

				HandleArmedObjectImpact( &ObjectSlots[ usCnt ] );

				guiPreRandomIndex = gres->uiPreRandomIndex; // do this here because it should be in the same sequence as the sending computer which sends the grenade result at the end of HandleArmedObjectImpact()
			}
			else
			{
#ifdef JA2BETAVERSION
				char tmpMsg1[128];
				sprintf(tmpMsg1,"ERROR! - Couldnt find a local PhysicsObject for the RealObjectID %i sent remotely from Team %i in recievePLANTEXPLOSIVE()",gres->RealObjectID, pThrower->bTeam );
				//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
				MPDebugMsg(tmpMsg1);
#endif
			}
		}
	}
	else
	{
#ifdef JA2BETAVERSION
		char tmpMsg[128];
		sprintf(tmpMsg,"ERROR! - Invalid Soldier pointer from ubID %i in recieveGRENADERESULT()", gres->ubOwnerID.i);
		//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
		MPDebugMsg(tmpMsg);
#endif
	}
}

void send_plant_explosive (SoldierID ubID,UINT16 usItem,UINT8 ubItemStatus,UINT16 usFlags, UINT32 sGridNo,UINT8 ubLevel, UINT32 uiWorldItemIndex)
{
	explosive_obj exp;

	exp.sGridNo = sGridNo;
	exp.ubID = MPEncodeSoldierID(ubID);
	exp.usItem = usItem;
	exp.ubItemStatus = ubItemStatus;
	exp.usFlags = usFlags;
	exp.uiWorldIndex = uiWorldItemIndex;
	exp.ubLevel = ubLevel;
	exp.bDetonatorType = gWorldItems[ uiWorldItemIndex ].object[0]->data.misc.bDetonatorType;
	if (exp.bDetonatorType == BOMB_REMOTE)
		exp.bDelayFreq = gWorldItems[ uiWorldItemIndex ].object[0]->data.misc.bFrequency;
	else
		exp.bDelayFreq = gWorldItems[ uiWorldItemIndex ].object[0]->data.misc.bDelay;

#ifdef JA2BETAVERSION
	CHAR tmpMPDbgString[512];
	sprintf(tmpMPDbgString,"MP - send_plant_explosive ( usItem : %i , sGridNo : %i , ubSoldierID : %i , uiPreRandomIndex : %i , uiWorldItemIndex : %i )\n",exp.usItem, exp.sGridNo , exp.ubID.i , guiPreRandomIndex , uiWorldItemIndex );
	MPDebugMsg(tmpMPDbgString);
#endif

	client->RPC("sendPLANTEXPLOSIVE",(const char*)&exp, (int)sizeof(explosive_obj)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recievePLANTEXPLOSIVE (RPCParameters *rpcParameters)
{
	explosive_obj* exp = (explosive_obj*)rpcParameters->input;

	exp->ubID = MPDecodeSoldierID( exp->ubID );

	SOLDIERTYPE* pSoldier = exp->ubID;
	if (pSoldier != NULL)
	{
		// explosive wasnt planted on our client, so we should do it on the client
		if (!IsOurSoldier(pSoldier) && (pSoldier->bTeam != 1 || !is_server))
		{
#ifdef JA2BETAVERSION
			CHAR tmpMPDbgString[512];
			sprintf( tmpMPDbgString, "MP - recievePLANTEXPLOSIVE ( usItem : %i , sGridNo : %i , ubSoldierID : %i , uiPreRandomIndex : %i )\n", exp->usItem, exp->sGridNo, exp->ubID.i, guiPreRandomIndex );
			MPDebugMsg(tmpMPDbgString);
#endif

			// this is a bit of a hack until we do the inventory sync
			OBJECTTYPE* newObj = new OBJECTTYPE();
			CreateItem( exp->usItem, exp->ubItemStatus, newObj );
			INT32 iNewItemIndex;
			OBJECTTYPE* pObj = AddItemToPoolAndGetIndex( exp->sGridNo, newObj, VISIBLE, exp->ubLevel, exp->usFlags,0, exp->ubID,&iNewItemIndex);
			// need to save Item Type metadata agaist the world item
			(*pObj)[0]->data.misc.ubBombOwner = exp->ubID + 2; // this is a hack the designers put into the game, storing the side as well (which isnt relevant in MP, but still have to do it)
			(*pObj)[0]->data.misc.usBombItem = exp->usItem;
			(*pObj)[0]->data.misc.bDetonatorType = exp->bDetonatorType;
			if (exp->bDetonatorType == BOMB_REMOTE)
				(*pObj)[0]->data.misc.bFrequency = exp->bDelayFreq;
			else
				(*pObj)[0]->data.misc.bDelay = exp->bDelayFreq;

			(*pObj)[0]->data.ubDirection = DIRECTION_IRRELEVANT;
			(*pObj)[0]->data.ubWireNetworkFlag = (TRIPWIRE_NETWORK_OWNER_ENEMY|TRIPWIRE_NETWORK_NET_1|TRIPWIRE_NETWORK_LVL_1);
			(*pObj)[0]->data.bDefuseFrequency = 0;
			
			// save old clients WorldID if we can
			// loop through world bombs and find the one linked to the item we just created
			UINT32 uiCount;
			bool bFound = false;
			for(uiCount=0; uiCount < guiNumWorldBombs; uiCount++)
			{
				if ( gWorldBombs[ uiCount ].fExists == TRUE && gWorldBombs[ uiCount ].iItemIndex == iNewItemIndex)
				{
					bFound = true;
					gWorldBombs[uiCount].iMPWorldItemIndex = exp->uiWorldIndex;
					gWorldBombs[uiCount].ubMPTeamIndex = pSoldier->bTeam;
					gWorldBombs[uiCount].bIsFromRemotePlayer = true;
					break;
				}
			}

			if (!bFound)
			{
#ifdef JA2BETAVERSION
				// this is a local failure really and will probably NEVER happen
				char tmpMsg1[128];
				sprintf(tmpMsg1,"ERROR! - Couldnt link our local WorldBomb to the ID sent remotely from Team %i in recievePLANTEXPLOSIVE()", pSoldier->bTeam );
				//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
				MPDebugMsg(tmpMsg1);
#endif
			}
		}
	}
	else
	{
#ifdef JA2BETAVERSION
		char tmpMsg[128];
		sprintf(tmpMsg,"ERROR! - Invalid Soldier pointer from ubID %i in recievePLANTEXPLOSIVE()",exp->ubID.i);
		//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
		MPDebugMsg(tmpMsg);
#endif
	}
}

void send_detonate_explosive (UINT32 uiWorldIndex, SoldierID ubID)
{
	ubID = MPEncodeSoldierID(ubID);

	SOLDIERTYPE* pSoldier = ubID;
	if (pSoldier != NULL)
	{
		// explosive detonated on this client, notify the other clients
		if ((pSoldier->bTeam == 1 && is_server) || IsOurSoldier(pSoldier))
		{
			// find the appropriate world bomb for the world item
			INT32 uiBombIndex = -1;
			UINT32 uiCount;
			for(uiCount=0; uiCount < guiNumWorldBombs; uiCount++)
			{
				if (gWorldBombs[uiCount].iItemIndex == uiWorldIndex)
				{
					uiBombIndex = uiCount;
					break;
				}
			}

			if (uiBombIndex > -1)
			{
				detonate_struct det;
				det.ubID = ubID;
										
				if ( gWorldBombs[ uiBombIndex ].bIsFromRemotePlayer ) 
				{
					// it is possible for players from other teams to set off a bomb that does not belong to them if they fail disarming it
					// but we must send the ID for the world item of the bomb that all the other clients recognise
					det.ubMPTeamIndex = gWorldBombs[ uiBombIndex ].ubMPTeamIndex;
					det.uiWorldItemIndex = gWorldBombs[ uiBombIndex ].iMPWorldItemIndex;
				}
				else
				{
					// it is a bomb that originated on our client
					det.uiWorldItemIndex = uiWorldIndex;
					det.ubMPTeamIndex = pSoldier->bTeam;
				}
				det.uiPreRandomIndex = guiPreRandomIndex;

#ifdef JA2BETAVERSION
				CHAR tmpMPDbgString[512];
				sprintf(tmpMPDbgString,"MP - send_detonate_explosive ( MPTeam : %i , uiWorldIndex : %i , uiPreRandomIndex : %i )\n",det.ubMPTeamIndex, det.uiWorldItemIndex , det.uiPreRandomIndex );
				MPDebugMsg(tmpMPDbgString);
				gfMPDebugOutputRandoms = true;
#endif

				client->RPC("sendDETONATEEXPLOSIVE",(const char*)&det, (int)sizeof(detonate_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
			}
			else
			{
				char tmpMsg1[128];
				sprintf(tmpMsg1,"ERROR! - Cant find a local WorldBomb for WorldIndex (locally) %i in send_detonate_explosive()",uiWorldIndex);
				//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
				MPDebugMsg(tmpMsg1);
			}
		}
	}
}

void recieveDETONATEEXPLOSIVE (RPCParameters *rpcParameters)
{
	detonate_struct* det = (detonate_struct*)rpcParameters->input;

	det->ubID = MPDecodeSoldierID(det->ubID);

	SOLDIERTYPE* pSoldier = det->ubID;
	if (pSoldier != NULL)
	{
		// if explosive detonation didnt originate from this client then its need to be performed here
		if (pSoldier->bTeam != netbTeam && (pSoldier->bTeam != 1 || !is_server))
		{
#ifdef JA2BETAVERSION
			CHAR tmpMPDbgString[512];
			sprintf(tmpMPDbgString,"MP - recieveDETONATEEXPLOSIVE ( MPTeam : %i , uiWorldIndex : %i , uiPreRandomIndex : %i , ubID : %i )\n",det->ubMPTeamIndex, det->uiWorldItemIndex , det->uiPreRandomIndex , det->ubID.i );
			MPDebugMsg(tmpMPDbgString);
			gfMPDebugOutputRandoms = true;
#endif

			guiPreRandomIndex = det->uiPreRandomIndex; // syncronise random number generator

			UINT32 uiCount;
			UINT32 ubWorldIndexToCheck = -1;
			bool bFound = false;
			for(uiCount=0; uiCount < guiNumWorldBombs; uiCount++)
			{
				// we could be recieving a message that a player from another team has detonated our bomb (while disarming), in this case we would check the local ids
				// otherwise we check MPCreatorID's like normal
				ubWorldIndexToCheck = (det->ubMPTeamIndex == netbTeam ? gWorldBombs[ uiCount ].iItemIndex : gWorldBombs[ uiCount ].iMPWorldItemIndex);
				if ( gWorldBombs[ uiCount ].fExists == TRUE && 
					 ubWorldIndexToCheck == det->uiWorldItemIndex &&
					 (gWorldBombs[ uiCount ].ubMPTeamIndex == det->ubMPTeamIndex || det->ubMPTeamIndex == netbTeam) )
				{
					bFound = true;
					AddBombToQueue(uiCount, guiBaseJA2Clock, TRUE); // blow up now :)
					break;
				}
			}

			if (!bFound)
			{
#ifdef JA2BETAVERSION
				char tmpMsg1[128];
				sprintf(tmpMsg1,"ERROR! - Cant find a local WorldBomb for remote WorldIndex %i from Team %i in recieveDETONATEEXPLOSIVE()",det->uiWorldItemIndex,det->ubMPTeamIndex );
				//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
				MPDebugMsg(tmpMsg1);
#endif
			}
		}
	}
}

void send_disarm_explosive(UINT32 sGridNo, UINT32 uiWorldItem, SoldierID ubID)
{
	ubID = MPEncodeSoldierID(ubID);

	SOLDIERTYPE* pSoldier = ubID;
	if (pSoldier != NULL)
	{
		// explosive disarmed on this client, notify the other clients
		if ((pSoldier->bTeam == 1 && is_server) || IsOurSoldier(pSoldier))
		{
			// find the appropriate world bomb for the world item
			INT32 uiBombIndex = -1;
			UINT32 uiCount;
			for(uiCount=0; uiCount < guiNumWorldBombs; uiCount++)
			{
				if (gWorldBombs[uiCount].iItemIndex == uiWorldItem)
				{
					uiBombIndex = uiCount;
					break;
				}
			}

			if (uiBombIndex > -1)
			{
				disarm_struct disarm;
				disarm.ubID = ubID;
				if ( gWorldBombs[ uiBombIndex ].bIsFromRemotePlayer )
				{
					// it is possible for players from other teams to defuse a bomb in the world
					// but we must send the ID for the world item of the bomb that all the other clients recognise
					disarm.ubMPTeamIndex = gWorldBombs[ uiBombIndex ].ubMPTeamIndex;
					disarm.uiWorldItemIndex = gWorldBombs[ uiBombIndex ].iMPWorldItemIndex;
				}
				else
				{
					disarm.ubMPTeamIndex = pSoldier->bTeam;
					disarm.uiWorldItemIndex = uiWorldItem;
				}
				disarm.sGridNo = sGridNo;
				disarm.uiPreRandomIndex = guiPreRandomIndex;

	#ifdef JA2BETAVERSION
				CHAR tmpMPDbgString[512];
				sprintf(tmpMPDbgString,"MP - send_disarm_explosive ( MPTeam : %i , uiWorldIndex : %i , uiPreRandomIndex : %i , sGridNo : %i )\n", disarm.ubMPTeamIndex, disarm.uiWorldItemIndex , disarm.uiPreRandomIndex, disarm.sGridNo);
				MPDebugMsg(tmpMPDbgString);
				gfMPDebugOutputRandoms = true;
	#endif

				client->RPC("sendDISARMEXPLOSIVE",(const char*)&disarm, (int)sizeof(disarm_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
			}
			else
			{
				char tmpMsg1[128];
				sprintf(tmpMsg1,"ERROR! - Cant find a local WorldBomb for WorldIndex (locally) %i in send_disarm_explosive()",uiWorldItem);
				//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
				MPDebugMsg(tmpMsg1);
			}
		}
	}
}

void recieveDISARMEXPLOSIVE (RPCParameters *rpcParameters)
{
	disarm_struct* disarm = (disarm_struct*)rpcParameters->input; 

	disarm->ubID = MPDecodeSoldierID(disarm->ubID);

	SOLDIERTYPE* pSoldier = disarm->ubID;
	if (pSoldier != NULL)
	{
		// if explosive disarm didnt originate from this client then its need to be performed here
		if (pSoldier->bTeam != netbTeam && (pSoldier->bTeam != 1 || !is_server))
		{
#ifdef JA2BETAVERSION
			CHAR tmpMPDbgString[512];
			sprintf(tmpMPDbgString,"MP - recieveDISARMEXPLOSIVE ( MPTeam : %i , uiWorldItemIndex : %i , uiPreRandomIndex : %i , ubID : %i , sGridNo : %i )\n",disarm->ubMPTeamIndex, disarm->uiWorldItemIndex , disarm->uiPreRandomIndex , disarm->ubID.i, disarm->sGridNo );
			MPDebugMsg(tmpMPDbgString);
			gfMPDebugOutputRandoms = true;
#endif

			guiPreRandomIndex = disarm->uiPreRandomIndex; // syncronise random number generator

			UINT32 uiCount;
			UINT32 ubWorldIndexToCheck = -1;
			bool bFound = false;
			for(uiCount=0; uiCount < guiNumWorldBombs; uiCount++)
			{
				ubWorldIndexToCheck = (disarm->ubMPTeamIndex == netbTeam ? gWorldBombs[ uiCount ].iItemIndex : gWorldBombs[ uiCount ].iMPWorldItemIndex);
				if ( gWorldBombs[ uiCount ].fExists == TRUE && 
					 disarm->uiWorldItemIndex == ubWorldIndexToCheck &&
					 (gWorldBombs[ uiCount ].ubMPTeamIndex == disarm->ubMPTeamIndex || disarm->ubMPTeamIndex == netbTeam) )
				{
					bFound = true;
					// print out a screen message if it was our bomb
					if (disarm->ubMPTeamIndex == netbTeam)
					{
						SOLDIERTYPE * pBombOwner = gWorldItems[ gWorldBombs[ uiCount ].iItemIndex ].soldierID;
						if (pBombOwner != NULL)
						{
							ScreenMsg( FONT_LTBLUE , MSG_MPSYSTEM , MPClientMessage[71], pBombOwner->name, pSoldier->name);
						}
					}

					// removing from the item pool will remove world item and world bomb
					UINT8 ubLevel = gWorldItems[ gWorldBombs[ uiCount ].iItemIndex ].ubLevel;
					RemoveItemFromPool( disarm->sGridNo , gWorldBombs[ uiCount ].iItemIndex, ubLevel );
					break;
				}
			}

			if (!bFound)
			{
#ifdef JA2BETAVERSION
				char tmpMsg1[128];
				sprintf(tmpMsg1,"ERROR! - Cant find a local WorldBomb for remote WorldIndex %i from Team %i in recieveDISARMEXPLOSIVE()",disarm->uiWorldItemIndex,disarm->ubMPTeamIndex );
				//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
				MPDebugMsg(tmpMsg1);
#endif
			}
		}
	}
}

void send_spreadeffect ( INT32 sGridNo, UINT8 ubRadius, UINT16 usItem, SoldierID ubOwner, BOOLEAN fSubsequent, INT8 bLevel, INT32 iSmokeEffectID )
{
	spreadeffect_struct sef;

	sef.sGridNo = sGridNo;
	sef.ubRadius = ubRadius;
	sef.usItem = usItem;
	sef.ubOwner = MPEncodeSoldierID(ubOwner);
	sef.fSubsequent = fSubsequent;
	sef.bLevel = bLevel;
	sef.iSmokeEffectID = iSmokeEffectID;
	sef.uiPreRandomIndex = guiPreRandomIndex;

#ifdef JA2BETAVERSION
	CHAR tmpMPDbgString[512];
	sprintf(tmpMPDbgString,"MP - send_spreadeffect ( sGridNo : %i , ubRadius : %i , usItem : %i , ubOwner : %i , fSubsequent : %i , bLevel : %i , iSmokeEffectID : %i , uiPreRandomIndex : %i )\n",sef.sGridNo, sef.ubRadius ,sef.usItem, sef.ubOwner.i, sef.fSubsequent, sef.bLevel, sef.iSmokeEffectID, sef.uiPreRandomIndex );
	MPDebugMsg(tmpMPDbgString);
#endif

	client->RPC("sendSPREADEFFECT",(const char*)&sef, (int)sizeof(spreadeffect_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveSPREADEFFECT (RPCParameters *rpcParameters)
{
	spreadeffect_struct* sef = (spreadeffect_struct*)rpcParameters->input;

	sef->ubOwner = MPDecodeSoldierID(sef->ubOwner);

	SOLDIERTYPE* pSoldier = sef->ubOwner;
	if (pSoldier != NULL)
	{

		// spread effect didnt originate from us
		if (!IsOurSoldier(pSoldier) && (pSoldier->bTeam != 1 || !is_server))
		{
#ifdef JA2BETAVERSION
			CHAR tmpMPDbgString[512];
			sprintf(tmpMPDbgString,"MP - recieveSPREADEFFECT ( sGridNo : %i , ubRadius : %i , usItem : %i , ubOwner : %i , fSubsequent : %i , bLevel : %i , iSmokeEffectID : %i , uiPreRandomIndex : %i )\n",sef->sGridNo, sef->ubRadius ,sef->usItem, sef->ubOwner.i, sef->fSubsequent, sef->bLevel, sef->iSmokeEffectID, sef->uiPreRandomIndex );
			MPDebugMsg(tmpMPDbgString);
#endif

			guiPreRandomIndex = sef->uiPreRandomIndex; // syncronise random number generator

			// translate SmokeEffectID
			if (sef->iSmokeEffectID >= 0)
			{
				UINT32 uiCount;
				bool bFound = false;
				for(uiCount=0; uiCount < guiNumSmokeEffects; uiCount++)
				{
					if ( gSmokeEffectData[ uiCount ].fAllocated == TRUE && gSmokeEffectData[ uiCount ].iMPTeamIndex == pSoldier->bTeam && gSmokeEffectData[ uiCount ].iMPSmokeEffectID == sef->iSmokeEffectID)
					{
						bFound = true;
						SpreadEffect( sef->sGridNo, sef->ubRadius, sef->usItem, sef->ubOwner, sef->fSubsequent, sef->bLevel, uiCount, TRUE);
						break;
					}
				}

				if (!bFound)
				{
#ifdef JA2BETAVERSION
					char tmpMsg1[128];
					sprintf(tmpMsg1,"ERROR! - Cant find a local SmokeEffectID for remote ID %i from team %i in recieveSPREADEFFECT()",sef->iSmokeEffectID, pSoldier->bTeam);
					//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
					MPDebugMsg(tmpMsg1);
#endif
				}
			}
			else
			{
				SpreadEffect( sef->sGridNo, sef->ubRadius, sef->usItem, sef->ubOwner, sef->fSubsequent, sef->bLevel, sef->iSmokeEffectID, TRUE);
			}
		}
	}
	else
	{
#ifdef JA2BETAVERSION
		char tmpMsg2[128];
		sprintf(tmpMsg2,"ERROR! - Invalid Soldier pointer from ubID %i in recieveSPREADEFFECT()",sef->ubOwner.i );
		//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
		MPDebugMsg(tmpMsg2);
#endif
	}
}

void send_newsmokeeffect(INT32 sGridNo, UINT16 usItem, INT8 bLevel, SoldierID ubOwner, INT32 iSmokeEffectID)
{
	// i'm reusing this struct, the parameters are essentially the same
	spreadeffect_struct sef;

	sef.sGridNo = sGridNo;
	sef.usItem = usItem;
	sef.ubOwner = MPEncodeSoldierID(ubOwner);
	sef.bLevel = bLevel;
	sef.iSmokeEffectID = iSmokeEffectID;
	sef.uiPreRandomIndex = guiPreRandomIndex;

#ifdef JA2BETAVERSION
	CHAR tmpMPDbgString[512];
	sprintf(tmpMPDbgString,"MP - send_newsmokeeffect ( sGridNo : %i , usItem : %i , ubOwner : %i , bLevel : %i , iSmokeEffectID : %i , uiPreRandomIndex : %i )\n",sef.sGridNo, sef.usItem, sef.ubOwner.i, sef.bLevel, sef.iSmokeEffectID, sef.uiPreRandomIndex );
	MPDebugMsg(tmpMPDbgString);
#endif

	client->RPC("sendNEWSMOKEEFFECT",(const char*)&sef, (int)sizeof(spreadeffect_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveNEWSMOKEEFFECT (RPCParameters *rpcParameters)
{
	spreadeffect_struct* sef = (spreadeffect_struct*)rpcParameters->input;

	// translate any of our soldier ids back to the correct local copy
	sef->ubOwner = MPDecodeSoldierID(sef->ubOwner);

	SOLDIERTYPE* pSoldier = sef->ubOwner;
	if (pSoldier != NULL)
	{
		// new smoke effect didnt originate from us
		if (!IsOurSoldier(pSoldier) && (pSoldier->bTeam != 1 || !is_server))
		{
#ifdef JA2BETAVERSION
			CHAR tmpMPDbgString[512];
			sprintf(tmpMPDbgString,"MP - recieveNEWSMOKEEFFECT ( sGridNo : %i , usItem : %i , ubOwner : %i , bLevel : %i , iSmokeEffectID : %i , uiPreRandomIndex : %i )\n",sef->sGridNo, sef->usItem, sef->ubOwner.i, sef->bLevel, sef->iSmokeEffectID, sef->uiPreRandomIndex );
			MPDebugMsg(tmpMPDbgString);
#endif

			guiPreRandomIndex = sef->uiPreRandomIndex;

			// start new smoke effect
			INT32 iNewSmokeIndex = NewSmokeEffect( sef->sGridNo, sef->usItem, sef->bLevel, sef->ubOwner, TRUE );
			
			// attach remote id to local smoke effect
			gSmokeEffectData[iNewSmokeIndex].iMPTeamIndex = pSoldier->bTeam;
			gSmokeEffectData[iNewSmokeIndex].iMPSmokeEffectID = sef->iSmokeEffectID;
		}
	}
	else
	{
#ifdef JA2BETAVERSION
		char tmpMsg[128];
		sprintf(tmpMsg,"ERROR! - Invalid Soldier pointer from ubID %i in recieveNEWSMOKEEFFECT()",sef->ubOwner.i );
		//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
		MPDebugMsg(tmpMsg);
#endif
	}
}

void send_gasdamage( SOLDIERTYPE * pSoldier, UINT16 usExplosiveClassID, INT16 sSubsequent, BOOLEAN fRecompileMovementCosts, INT16 sWoundAmt, INT16 sBreathAmt, SoldierID ubOwner )
{
	explosiondamage_struct exp;
	exp.ubDamageFunc = 1;
	exp.ubSoldierID = MPEncodeSoldierID(pSoldier->ubID);
	exp.usExplosiveClassID = usExplosiveClassID;
	exp.sSubsequent = sSubsequent;
	exp.fRecompileMovementCosts = fRecompileMovementCosts;
	exp.sWoundAmt = sWoundAmt;
	exp.sBreathAmt = sBreathAmt;
	exp.ubAttackerID = MPEncodeSoldierID(ubOwner);
	exp.uiPreRandomIndex = guiPreRandomIndex;

#ifdef JA2BETAVERSION
	CHAR tmpMPDbgString[512];
	sprintf(tmpMPDbgString, "MP - send_gasdamage ( ubSoldierID : %i , usExplosiveClassID : %i , sSubsequent : %i , recompileMoveCosts : %i , sWoundAmt : %i , sBreathAmt : %i , ubOwner : %i )\n", exp.ubSoldierID.i, usExplosiveClassID , sSubsequent , fRecompileMovementCosts , sWoundAmt , sBreathAmt , exp.ubAttackerID.i );
	MPDebugMsg(tmpMPDbgString);
#endif

	client->RPC("sendEXPLOSIONDAMAGE",(const char*)&exp, (int)sizeof(explosiondamage_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void send_explosivedamage( SoldierID ubPerson, SoldierID ubOwner, INT32 sBombGridNo, INT16 sWoundAmt, INT16 sBreathAmt, UINT32 uiDist, UINT16 usItem, INT16 sSubsequent )
{
	explosiondamage_struct exp;
	exp.ubDamageFunc = 2;
	exp.ubSoldierID = MPEncodeSoldierID(ubPerson);
	exp.usItem = usItem;
	exp.uiDist = uiDist;
	exp.sSubsequent = sSubsequent;
	exp.sBombGridNo = sBombGridNo;
	exp.sWoundAmt = sWoundAmt;
	exp.sBreathAmt = sBreathAmt;
	exp.ubAttackerID = MPEncodeSoldierID(ubOwner);
	exp.uiPreRandomIndex = guiPreRandomIndex;

#ifdef JA2BETAVERSION
	CHAR tmpMPDbgString[512];
	sprintf(tmpMPDbgString, "MP - send_explosivedamage ( ubPerson : %i , ubOwner : %i , sBombGridNo : %i , sWoundAmt : %i , sBreathAmt : %i , uiDist : %i , usItem : %i , sSubs : %i )\n", ubPerson.i, ubOwner.i, sBombGridNo , sWoundAmt , sBreathAmt , uiDist , usItem , sSubsequent );
	MPDebugMsg(tmpMPDbgString);
#endif

	client->RPC("sendEXPLOSIONDAMAGE",(const char*)&exp, (int)sizeof(explosiondamage_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveEXPLOSIONDAMAGE (RPCParameters *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, explosiondamage_struct);	// short-frame guard (H5/H13)
	explosiondamage_struct* exp = (explosiondamage_struct*)rpcParameters->input;

	// H5: usItem indexes Item[]/Explosive[] (compounding OOB read) -- bound it.
	if ( exp->usItem == 0 || exp->usItem >= MAXITEMS )
		return;

	exp->ubSoldierID = MPDecodeSoldierID(exp->ubSoldierID);
	exp->ubAttackerID = MPDecodeSoldierID(exp->ubAttackerID);


	SOLDIERTYPE* pSoldier = exp->ubSoldierID;
	if (pSoldier != NULL)
	{

		// damage isnt for our merc (or we wouldve handled it locally) or it is for an AI but we are NOT the server
		if (!IsOurSoldier(pSoldier) && (pSoldier->bTeam != 1 || !is_server))
		{
#ifdef JA2BETAVERSION
			CHAR tmpMPDbgString[512];
			sprintf(tmpMPDbgString, "MP - recieveEXPLOSIONDAMAGE ( ubDamageFunc : %i , ubSoldierID : %i , ubAttackerID : %i , usItem : %i , usExplosiveClassID : %i , sWoundAmt : %i , sBreathAmt : %i , uiDist : %i , sSubs : %i , sBombGridNo : %i , uiPreRandomIndex : %i )\n", exp->ubDamageFunc , exp->ubSoldierID.i, exp->ubAttackerID.i, exp->usItem , exp->usExplosiveClassID , exp->sWoundAmt , exp->sBreathAmt , exp->uiDist , exp->sSubsequent , exp->sBombGridNo , exp->uiPreRandomIndex );
			MPDebugMsg(tmpMPDbgString);
#endif

			guiPreRandomIndex = exp->uiPreRandomIndex;

			if (exp->ubDamageFunc == 1)
			{
				//EXPLOSIVETYPE* pExplosive = &(Explosive[ exp->usExplosiveClassID ] );
				//DishOutGasDamage(pSoldier, pExplosive, exp->sSubsequent , exp->fRecompileMovementCosts , exp->sWoundAmt , exp->sBreathAmt , exp->ubAttackerID , TRUE );

				// can use DishOutGasDamage() as it is dependant on the local state of the gas cloud which is not always in sync
				// but we have the definite results of damage on a merc, so :
				pSoldier->SoldierTakeDamage( ANIM_STAND, exp->sWoundAmt, exp->sBreathAmt, Explosive[Item[exp->usItem].ubClassIndex].ubType == EXPLOSV_BURNABLEGAS ? TAKE_DAMAGE_GAS_FIRE : TAKE_DAMAGE_GAS_NOTFIRE, NOBODY, NOWHERE, 0, TRUE );
			}
			else if (exp->ubDamageFunc == 2)
			{
				DamageSoldierFromBlast( exp->ubSoldierID, exp->ubAttackerID, exp->sBombGridNo, exp->sWoundAmt, exp->sBreathAmt, exp->uiDist, exp->usItem, exp->sSubsequent, TRUE);
			}
		}
	}
	else
	{
#ifdef JA2BETAVERSION
		char tmpMsg[128];
		sprintf(tmpMsg, "ERROR! - Invalid Soldier pointer from ubID %i in recieveEXPLOSIONDAMAGE()", exp->ubAttackerID.i );
		//ScreenMsg(FONT_RED,MSG_MPSYSTEM,tmpMsg);
		MPDebugMsg(tmpMsg);
#endif
	}
}


void send_bullet(  BULLET * pBullet,UINT16 usHandItem )
{
	// PORTABLE WIRE FORMAT (H16): copy only the consumed scalars into bullet_wire and
	// serialize field-by-field. No heap pointers / no ABI-variant struct layout cross the
	// wire. The firer id is MP-encoded exactly as before.
	bullet_wire b;
	memset( &b, 0, sizeof(b) );
	b.iBullet                = pBullet->iBullet;
	b.ubFirerID              = pBullet->ubFirerID.i;
	if ( pBullet->ubFirerID < 20 )
		b.ubFirerID = b.ubFirerID + ubID_prefix;
	b.usFlags                = pBullet->usFlags;
	b.usHandItem             = usHandItem;
	b.bStartCubesAboveLevelZ = pBullet->bStartCubesAboveLevelZ;
	b.bEndCubesAboveLevelZ   = pBullet->bEndCubesAboveLevelZ;
	b.fCheckForRoof          = pBullet->fCheckForRoof;
	b.fAimed                 = pBullet->fAimed;
	b.ubItemStatus           = pBullet->ubItemStatus;
	b.sHitBy                 = pBullet->sHitBy;
	b.sTargetGridNo          = pBullet->sTargetGridNo;
	b.iImpact                = pBullet->iImpact;
	b.iRange                 = pBullet->iRange;
	b.iDistanceLimit         = pBullet->iDistanceLimit;
	b.qCurrX                 = pBullet->qCurrX;
	b.qCurrY                 = pBullet->qCurrY;
	b.qCurrZ                 = pBullet->qCurrZ;
	b.qIncrX                 = pBullet->qIncrX;
	b.qIncrY                 = pBullet->qIncrY;
	b.qIncrZ                 = pBullet->qIncrZ;
	b.ddHorizAngle           = pBullet->ddHorizAngle;

	uint8_t wire[BULLET_WIRE_BYTES];
	int wireBytes = SerializeBullet( wire, sizeof(wire), b );
	client->RPC("sendBULLET",(const char*)wire, wireBytes*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveBULLET(RPCParameters *rpcParameters)
{
	// PORTABLE WIRE FORMAT (H16): deserialize the fixed-width bullet payload. Drop short frames.
	bullet_wire b;
	if ( !DeserializeBullet( b, rpcParameters->input, (rpcParameters->numberOfBitsOfData + 7) / 8 ) )
		return;

	INT32 net_iBullet=b.iBullet;
	if ( net_iBullet < 0 || net_iBullet >= NUM_BULLET_SLOTS )
	{
		return;	// wire bullet slot out of range
	}

	SoldierID firerID = (UINT16)b.ubFirerID;	// run the clamping ctor on the wire id
	SOLDIERTYPE * pFirer = NULL;
	INT8 bTeam = OUR_TEAM;
	if ( firerID != NOBODY )
	{
		pFirer = firerID;
		bTeam=pFirer->bTeam;
	}
	if ( bTeam < 0 || bTeam >= MAXTEAMS )
		return;	// wire team out of range -> would OOB bTable[]

	BULLET * pBullet;

	INT32 iBullet = CreateBullet( firerID, 0, b.usFlags, b.usHandItem );

	if ( iBullet < 0 )
		return;	// slot exhaustion -> GetBulletPtr(-1) would OOB (H7-adjacent)

#ifdef BETAVERSION
	if (iBullet == -1)
	{
		ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, L"Failed to create bullet");
	}
#endif

	// H7: CreateBullet returns -1 on slot exhaustion; GetBulletPtr(-1) -> &gBullets[-1]
	// OOB write. Drop the frame rather than write ~17 fields past the array.
	if ( iBullet < 0 )
		return;

	//add bullet to bullet table for translation
	bTable[bTeam][net_iBullet].remote_id = net_iBullet;
	bTable[bTeam][net_iBullet].local_id = iBullet;

	pBullet = GetBulletPtr( iBullet );

	//ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, L"Created Bullet Id: %d",iBullet);

	pBullet->fCheckForRoof=b.fCheckForRoof;
	pBullet->qIncrX=b.qIncrX;
	pBullet->qIncrY=b.qIncrY;
	pBullet->qIncrZ=b.qIncrZ;
	pBullet->sHitBy=b.sHitBy;
	pBullet->ddHorizAngle=b.ddHorizAngle;
	pBullet->fAimed=b.fAimed;
	pBullet->ubItemStatus=b.ubItemStatus;
	pBullet->qCurrX=b.qCurrX;
	pBullet->qCurrY=b.qCurrY;
	pBullet->qCurrZ=b.qCurrZ;
	pBullet->iImpact=b.iImpact;
	pBullet->iRange=b.iRange;
	pBullet->sTargetGridNo=b.sTargetGridNo;
	pBullet->bStartCubesAboveLevelZ=b.bStartCubesAboveLevelZ;
	pBullet->bEndCubesAboveLevelZ=b.bEndCubesAboveLevelZ;
	pBullet->iDistanceLimit=b.iDistanceLimit;

	if ( pFirer != NULL )
		FireBullet( pFirer->ubID, pBullet, FALSE );
	else
		FireBullet( NOBODY, pBullet, FALSE );	// NOBODY-firer bullets are legal (bullets.h)
}

void send_changestate (EV_S_CHANGESTATE * SChangeState)
{
	EV_S_CHANGESTATE	new_state;

	memcpy( &new_state , SChangeState, sizeof( EV_S_CHANGESTATE ));

	if(new_state.usSoldierID < 20)
		new_state.usSoldierID = new_state.usSoldierID+ubID_prefix;
	
	client->RPC("sendSTATE",(const char*)&new_state, (int)sizeof(EV_S_CHANGESTATE)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveSTATE(RPCParameters *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, EV_S_CHANGESTATE);	// short-frame guard (H6/H13)
	EV_S_CHANGESTATE*	new_state = (EV_S_CHANGESTATE*)rpcParameters->input;

	// H6: usNewState indexes gAnimControl[] and drives EVENT_InitNewSoldierAnim() --
	// an out-of-range wire value is an OOB read + invalid anim init.
	if ( !IsValidAnimState( new_state->usNewState ) )
		return;

	SOLDIERTYPE * pSoldier = new_state->usSoldierID;
	if ( pSoldier == NULL || !pSoldier->bActive || !pSoldier->bInSector )
	{
		return;	// MP wire guard: ignore events for soldiers not in our world (mp_audit_findings.json)
	}

	if(pSoldier->bActive)
	{
		// Start first AID
		if(new_state->usNewState==START_AID)
		{
			pSoldier->EVENT_InternalSetSoldierPosition( new_state->sXPos, new_state->sYPos ,FALSE, FALSE, FALSE );
			pSoldier->EVENT_SetSoldierDirection(	new_state->usNewDirection );
			// SANDRO - we can now bandage mercs when prone, so change stance only if standing
			if ( gAnimControl[ pSoldier->usAnimState ].ubEndHeight == ANIM_STAND )
			{
				pSoldier->ChangeSoldierStance( ANIM_CROUCH );
			}

		}
		// SANDRO - if ordered to bandage in prone position...
		else if (new_state->usNewState==START_AID_PRN)
		{
			pSoldier->EVENT_InternalSetSoldierPosition( new_state->sXPos, new_state->sYPos ,FALSE, FALSE, FALSE );
			pSoldier->EVENT_SetSoldierDirection(	new_state->usNewDirection );
			if ( gAnimControl[ pSoldier->usAnimState ].ubEndHeight != ANIM_PRONE )
			{
				pSoldier->ChangeSoldierStance( ANIM_PRONE );
			}
		}
		// Start cutting fence
		else if (new_state->usNewState==CUTTING_FENCE)
		{
			// The usTargetGridNo holds the GridNo of the fence tile
			pSoldier->sTargetGridNo = new_state->usTargetGridNo;

			pSoldier->EVENT_InternalSetSoldierPosition( new_state->sXPos, new_state->sYPos ,FALSE, FALSE, FALSE );
			pSoldier->ChangeSoldierStance( ANIM_CROUCH );
			pSoldier->EVENT_SetSoldierDirection(	new_state->usNewDirection );
		}

		// MP echo guard: already collapsed locally -- don't stand him up and replay
		// the fall (anim + thump + AP/BP) when the owner's collapse echo arrives. (audit [27])
		if ( pSoldier->bCollapsed &&
			gAnimControl[ pSoldier->usAnimState ].ubEndHeight == ANIM_PRONE &&
			gAnimControl[ new_state->usNewState ].ubEndHeight == ANIM_PRONE &&
			gAnimControl[ new_state->usNewState ].ubHeight != ANIM_PRONE )
		{
			return;
		}
		pSoldier->EVENT_InitNewSoldierAnim( new_state->usNewState, new_state->usStartingAniCode, new_state->fForce );
	}
}

void send_death( SOLDIERTYPE *pSoldier )
{
	death_struct nDeath;
	nDeath.soldier_id = pSoldier->ubID;
	nDeath.attacker_id = pSoldier->ubAttackerID;

	// Translate soldier id for other clients if the soldier was one of ours
	if(pSoldier->ubID<20)nDeath.soldier_id=nDeath.soldier_id+ubID_prefix;
	
	// if soldier died from bleeding
	if(pSoldier->ubAttackerID >= NOBODY)
	{
		if (pSoldier->ubPreviousAttackerID < NOBODY)
			nDeath.attacker_id = pSoldier->ubPreviousAttackerID;
		else if (pSoldier->ubNextToPreviousAttackerID < NOBODY)
			nDeath.attacker_id = pSoldier->ubNextToPreviousAttackerID;
	}

	SOLDIERTYPE * pAttacker = nDeath.attacker_id;
	INT8 pA_bTeam=CLIENT_NUM;
	CHAR16	pA_name[ 10 ];
	INT8 pS_bTeam=CLIENT_NUM;
	CHAR16	pS_name[ 10 ];

	// OJW - 20081222
	// save stats
	if(pAttacker)
	{
		// if attacker was one of our own mercs, use the last hostile attacker as the killer if there is one
		if (pAttacker->bTeam == pSoldier->bTeam && pSoldier->ubPreviousAttackerID < NOBODY)
		{
			pAttacker = pSoldier->ubPreviousAttackerID;
			// check if the new attacker was also a friendly...
			if (pAttacker->bTeam == pSoldier->bTeam && pSoldier->ubNextToPreviousAttackerID < NOBODY)
				pAttacker = pSoldier->ubNextToPreviousAttackerID;
			// if its still a friendly, use the original attacker id...for posterity
			// guy must snore too loudly if all his mates wanna kill him :)
			if (pAttacker->bTeam == pSoldier->bTeam && pSoldier->ubAttackerID != NOBODY)
				pAttacker = pSoldier->ubAttackerID;

			nDeath.attacker_id = pAttacker->ubID;
		}
	
		// Translate attacker id for other clients if attacker was one of ours
		if(pAttacker->ubID<20)nDeath.attacker_id=pAttacker->ubID+ubID_prefix;

		pA_bTeam=pAttacker->bTeam;
		memcpy(pA_name,pAttacker->name,sizeof(CHAR16)*10);

		if (pA_bTeam==1 && cGameType==MP_TYPE_COOP)
		{
			// CO-OP Kill by an enemy ai
			pA_bTeam = 5; // the server subtracts 1 from these numbers to score in scoreboard, AI is index 4
		}
		else
		{
			// Any mode, kill by a players merc
			if(pA_bTeam>5)
				pA_bTeam=pA_bTeam-5;

			if(pA_bTeam==0)
				pA_bTeam= CLIENT_NUM;
		}
	}

	if(pSoldier)
	{
		pS_bTeam=pSoldier->bTeam;
		memcpy(pS_name,pSoldier->name,sizeof(CHAR16)*10);
		if(pS_bTeam==1 && cGameType==MP_TYPE_COOP) 
		{
			// CO-OP Death of an enemy ai
			pS_bTeam = 5; // the server subtracts 1 from these numbers to score in scoreboard, AI is index 4
		}
		else
		{
			// Any mode death of a players merc
			if(pS_bTeam>5)
				pS_bTeam=pS_bTeam-5;

			if(pS_bTeam==0)
				pS_bTeam=CLIENT_NUM;
		}
	}

	nDeath.attacker_team = pA_bTeam;
	nDeath.soldier_team = pS_bTeam;

	// notify other clients of death
	client->RPC("sendDEATH",(const char*)&nDeath, (int)sizeof(death_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
	
	// print kill notice to screen	
	if (pSoldier && pSoldier->bTeam==1)  
		ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, MPClientMessage[67]);	
	else  
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[28],pS_name,(pS_bTeam),client_names[(pS_bTeam-1)],pA_name,(pA_bTeam),client_names[(pA_bTeam-1)] );

#ifdef JA2BETAVERSION
    if (pSoldier)  {
	    char s_name[10];
	    char a_name[10];
	    WideCharToMultiByte(CP_UTF8,0,pS_name,-1, s_name,10,NULL,NULL);
	    WideCharToMultiByte(CP_UTF8,0,pA_name,-1, a_name,10,NULL,NULL);
	
	    if (pSoldier->bTeam==1) 
		    MPDebugMsg( String ( "MPDEBUG SEND - Enemy AI #%d was killed by ('%s' - %d) (client %d - '%s')\n",nDeath.soldier_id,a_name,nDeath.attacker_id,pA_bTeam,client_names[pA_bTeam-1]) );
	    else if (pAttacker->bTeam==1) 
		    MPDebugMsg( String ( "MPDEBUG SEND - '%s' (client %d - '%S') was killed by '%s' (client %d - '%s')\n",s_name,pS_bTeam,client_names[(pS_bTeam-1)],a_name,pA_bTeam,"Queens Army") );
	    else 
		    MPDebugMsg( String ( "MPDEBUG SEND - '%s' (client %d - '%S') was killed by '%s' (client %d - '%s')\n",s_name,pS_bTeam,client_names[(pS_bTeam-1)],a_name,pA_bTeam,client_names[(pA_bTeam-1)]) );
    }
#endif
}

void recieveDEATH (RPCParameters *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, death_struct);	// short-frame guard (M2/H13)
	death_struct* nDeath = (death_struct*)rpcParameters->input;
	SOLDIERTYPE * pSoldier = nDeath->soldier_id;
	if ( pSoldier == NULL )
	{
		return;	// MP wire guard: unknown victim id (mp_audit_findings.json)
	}

	SoldierID ubAttackerID;
	if((nDeath->attacker_id >= ubID_prefix) && (nDeath->attacker_id < (ubID_prefix+6)))
		ubAttackerID = (nDeath->attacker_id - ubID_prefix);
	else
		ubAttackerID = nDeath->attacker_id;

	SOLDIERTYPE * pAttacker = ubAttackerID;
	// M2: pA_bTeam/pS_bTeam are used as client_names[x-1] indices unconditionally;
	// default them to our own (valid) client number so a missing attacker / unset
	// branch can't index the array with garbage.
	INT8 pA_bTeam=CLIENT_NUM;
	CHAR16	pA_name[ 10 ] = {0};
	INT8 pS_bTeam=CLIENT_NUM;
	CHAR16	pS_name[ 10 ] = {0};

	if(pAttacker)
	{
		pA_bTeam=pAttacker->bTeam;
		memcpy(pA_name,pAttacker->name,sizeof(CHAR16)*10);
		
		if(pA_bTeam>5)
			pA_bTeam=pA_bTeam-5;
		if(pA_bTeam==0)
			pA_bTeam=CLIENT_NUM;
		// M2: keep the client_names[pA_bTeam-1] index in range [0,3].
		if(pA_bTeam<1 || pA_bTeam>4)
			pA_bTeam=CLIENT_NUM;
	}

	if(pSoldier)
	{
		pS_bTeam=pSoldier->bTeam;
		memcpy(pS_name,pSoldier->name,sizeof(CHAR16)*10);
		
		if(pS_bTeam>5)
			pS_bTeam=pS_bTeam-5;
		if(pS_bTeam==0)
			pS_bTeam=CLIENT_NUM;
		// M2: keep the client_names[pS_bTeam-1] index in range [0,3].
		if(pS_bTeam<1 || pS_bTeam>4)
			pS_bTeam=CLIENT_NUM;
	}
			
	if(pSoldier->bActive)
	{
		pSoldier->usAnimState=50;

		#ifdef JA2BETAVERSION
			ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, L"made merc corpse/dead");	
		#endif

		RemoveManAsTarget(pSoldier);
		TurnSoldierIntoCorpse( pSoldier, TRUE, TRUE );
	

		if ( CheckForEndOfBattle( FALSE ) )
		{
			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[72]);
		}

		if (pSoldier->bTeam==1)  
			ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, MPClientMessage[67]);	
		else 
			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[28],pS_name,(pS_bTeam),client_names[(pS_bTeam-1)],pA_name,(pA_bTeam),client_names[(pA_bTeam-1)] );
	}
	else
	{
		#ifdef JA2BETAVERSION
			ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, L"merc already corpse/dead");	
		#endif

		if (pSoldier->bTeam==1) 
			ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, MPClientMessage[67]);	
		
		CheckForEndOfBattle( FALSE );
	}

#ifdef JA2BETAVERSION
	char s_name[10];
	char a_name[10];
	WideCharToMultiByte(CP_UTF8,0,pS_name,-1, s_name,10,NULL,NULL);
	WideCharToMultiByte(CP_UTF8,0,pA_name,-1, a_name,10,NULL,NULL);
	
	if (pSoldier->bTeam==1) 
		MPDebugMsg( String ( "MPDEBUG RECV - Enemy AI #%d was killed by ('%s' - #%d) (client %d - '%s')\n",nDeath->soldier_id,a_name,nDeath->attacker_id,pA_bTeam,client_names[pA_bTeam-1]) );
	else if (pAttacker && pAttacker->bTeam==1) 	// M2: pAttacker may be NULL here
		MPDebugMsg( String ( "MPDEBUG RECV - '%s' (client %d - '%s') was killed by '%s' (client %d - '%s')\n",s_name,pS_bTeam,client_names[(pS_bTeam-1)],a_name,pA_bTeam,"Queens Army") );
	else 
		MPDebugMsg( String ( "MPDEBUG RECV - '%s' (client %d - '%s') was killed by '%s' (client %d - '%s')\n",s_name,pS_bTeam,client_names[(pS_bTeam-1)],a_name,pA_bTeam,client_names[(pA_bTeam-1)]) );
#endif
}

void send_hitstruct(EV_S_STRUCTUREHIT	*	SStructureHit)
{
	EV_S_STRUCTUREHIT struct_hit;
	memcpy( &struct_hit , SStructureHit, sizeof( EV_S_STRUCTUREHIT ));
	if(SStructureHit->ubAttackerID <20)struct_hit.ubAttackerID = SStructureHit->ubAttackerID+ubID_prefix;
			
	client->RPC("sendhitSTRUCT",(const char*)&struct_hit, (int)sizeof(EV_S_STRUCTUREHIT)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void send_hitwindow(EV_S_WINDOWHIT * SWindowHit)
{
	EV_S_WINDOWHIT window_hit;
	memcpy( &window_hit , SWindowHit, sizeof( EV_S_WINDOWHIT ));
	
	if(SWindowHit->ubAttackerID <20)
		window_hit.ubAttackerID = SWindowHit->ubAttackerID+ubID_prefix;
			
	client->RPC("sendhitWINDOW",(const char*)&window_hit, (int)sizeof(EV_S_WINDOWHIT)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void send_miss(EV_S_MISS * SMiss)
{
	EV_S_MISS shot_miss;
	memcpy( &shot_miss , SMiss, sizeof( EV_S_MISS ));
	
	if(SMiss->ubAttackerID <20)
		shot_miss.ubAttackerID = SMiss->ubAttackerID+ubID_prefix;
			
	client->RPC("sendMISS",(const char*)&shot_miss, (int)sizeof(EV_S_MISS)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recievehitSTRUCT  (RPCParameters *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, EV_S_STRUCTUREHIT);	// short-frame guard (H1/H13)
	EV_S_STRUCTUREHIT* struct_hit = (EV_S_STRUCTUREHIT*)rpcParameters->input;
	// H1: attacker is a wire SoldierID -- resolve safely, range-check before bTable[][].
	SOLDIERTYPE *pSoldier = SafeMerc( struct_hit->ubAttackerID.i );
	if ( pSoldier == NULL
		|| pSoldier->bTeam < 0 || pSoldier->bTeam >= MAXTEAMS
		|| struct_hit->iBullet < 0 || struct_hit->iBullet >= NUM_BULLET_SLOTS )
		return;
	INT8 bTeam=pSoldier->bTeam;
	INT32 iBullet = bTable[bTeam][struct_hit->iBullet].local_id;

	if(struct_hit->fStopped)
		StopBullet( iBullet );
	
	StructureHit( iBullet, struct_hit->usWeaponIndex, struct_hit->bWeaponStatus, struct_hit->ubAttackerID, struct_hit->sXPos, struct_hit->sYPos, struct_hit->sZPos, struct_hit->usStructureID, struct_hit->iImpact, struct_hit->fStopped );
	
	if(struct_hit->fStopped)
		RemoveBullet(iBullet);
}

void recievehitWINDOW  (RPCParameters *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, EV_S_WINDOWHIT);	// short-frame guard (H3/H13)
	EV_S_WINDOWHIT* window_hit = (EV_S_WINDOWHIT*)rpcParameters->input;
	// H3: wire gridno walks gpWorldLevelData[] (WORLD_MAX-sized) -- bound it.
	if ( window_hit->sGridNo < 0 || window_hit->sGridNo >= WORLD_MAX )
		return;
	WindowHit( window_hit->sGridNo, window_hit->usStructureID, window_hit->fBlowWindowSouth, window_hit->fLargeForce );
}

void recieveMISS  (RPCParameters *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, EV_S_MISS);	// short-frame guard (H2/H13)
	EV_S_MISS* shot_miss = (EV_S_MISS*)rpcParameters->input;

	// H2: attacker is a wire SoldierID -- resolve safely, range-check before bTable[][].
	SOLDIERTYPE *pSoldier = SafeMerc( shot_miss->ubAttackerID.i );
	if ( pSoldier == NULL
		|| pSoldier->bTeam < 0 || pSoldier->bTeam >= MAXTEAMS
		|| shot_miss->iBullet < 0 || shot_miss->iBullet >= NUM_BULLET_SLOTS )
		return;
	INT8 bTeam=pSoldier->bTeam;
	INT32 iBullet = bTable[bTeam][shot_miss->iBullet].local_id;

	ShotMiss( shot_miss->ubAttackerID, iBullet );
}

BOOLEAN check_status (void)// any 'enemies' and clients left to fight ??
{
	SOLDIERTYPE *pSoldier;
	int soldiers= 0 ;
	int numActiveSides = 0;
	
	int dm_teams[4] = {0 , 0 , 0 , 0};

	for(int x=0;x < MAXTEAMS; x++)
	{
		soldiers=0;

		for( SoldierID cnt = gTacticalStatus.Team[ x ].bFirstID;cnt <= gTacticalStatus.Team[ x ].bLastID; ++cnt)
		{
			pSoldier = cnt;
			if(pSoldier->stats.bLife >= OKLIFE && pSoldier->bActive && pSoldier->bInSector)
			{
				soldiers++;
			}
		}

		if(soldiers>0)
		{
			gTacticalStatus.Team[ x ].bTeamActive=1;
			gTacticalStatus.Team[ x ].bMenInSector=soldiers;

			if (cGameType==MP_TYPE_TEAMDEATMATCH && (x==0 || x>5))
			{
				// store the number of active DM teams, and number of players per team still alive
				int cl_num = CLIENT_NUM; // 1 based
				if (x>5) cl_num = x - 5;
				dm_teams[ client_teams[ cl_num - 1 ] ]++;
			}
			else if (cGameType != MP_TYPE_TEAMDEATMATCH)
			{
				// count number of active teams
				numActiveSides++;
			}
		}
		else
		{
			gTacticalStatus.Team[ x ].bTeamActive=0;
			gTacticalStatus.Team[x].bMenInSector=0;
		}
	}

	if( (gTacticalStatus.Team[ 0 ].bTeamActive == 0) && wiped==0)//server's team has been knocked out
	{		
		wiped=1;
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[40] );
		if(!cDisableSpectatorMode)
		{
			gTacticalStatus.uiFlags |= SHOW_ALL_MERCS;//hayden
			ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, MPClientMessage[41] );
		}
		else 
		{
			ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, MPClientMessage[73]);
		}

		teamwiped();
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, MPClientMessage[42] );
	}

	if (cGameType == MP_TYPE_DEATHMATCH)
	{
		// check game end for DeathMatch
		return (numActiveSides > 1);
	}
	else if (cGameType == MP_TYPE_TEAMDEATMATCH)
	{
		// check game end for Team Deathmatch
		// count how many active deathmatch teams are alive (two players could be alive but might be on the same TEAM)
		for (int i=0; i < 4; i++)
		{
			if (dm_teams[i] > 0)
			{
				numActiveSides++;
			}
		}

		return (numActiveSides > 1);
	}
	else if (cGameType == MP_TYPE_COOP)
	{
		// check for game end for CO-OP
		// If any player team is alive && the number of enemies > 0 then continue game (true), else quit (false) 
		return ((gTacticalStatus.Team[ 0 ].bTeamActive==1 || gTacticalStatus.Team[ 6 ].bTeamActive==1 || gTacticalStatus.Team[ 7 ].bTeamActive==1 || gTacticalStatus.Team[ 8 ].bTeamActive==1 || gTacticalStatus.Team[ 9 ].bTeamActive==1  )&& NumEnemyInSector() > 0);
	}

	// dont stop the game by default
	return true;	
}

void UpdateSoldierToNetwork ( SOLDIERTYPE *pSoldier )
{
	//this send stats to other clients at intervals
	SoldierID id = pSoldier->ubID;
	UINT32 time = GetJA2Clock();

	if(id < 20 || (is_server && id <120))
	{
		if(pSoldier->usLastUpdateTime==0)
		{
			pSoldier->usLastUpdateTime = time;
		}
		if((time - (pSoldier->usLastUpdateTime)) > 2000 && pSoldier->stats.bLife!=0)
		{
			pSoldier->usLastUpdateTime = time;

			EV_S_UPDATENETWORKSOLDIER SUpdateNetworkSoldier;

			SUpdateNetworkSoldier.usSoldierID=pSoldier->ubID;
			
			if(pSoldier->ubID < 20)
				SUpdateNetworkSoldier.usSoldierID=pSoldier->ubID+ubID_prefix;
			
			SUpdateNetworkSoldier.sAtGridNo=pSoldier->sGridNo;
			SUpdateNetworkSoldier.bActionPoints=pSoldier->bActionPoints;	// owner-authoritative AP, reconciled on copies (DeductPoints no longer spends AP on copies)
			SUpdateNetworkSoldier.bBreath=pSoldier->bBreath;
			SUpdateNetworkSoldier.ubDirection=pSoldier->ubDirection;

			SUpdateNetworkSoldier.bLife=pSoldier->stats.bLife;
			SUpdateNetworkSoldier.bBleeding=pSoldier->bBleeding;
			SUpdateNetworkSoldier.ubNewStance= gAnimControl[ pSoldier->usAnimState ].ubEndHeight;
			
			if((gTacticalStatus.combatUI.ubTopMessageType == PLAYER_TURN_MESSAGE || gTacticalStatus.combatUI.ubTopMessageType == PLAYER_INTERRUPT_MESSAGE || ((gTacticalStatus.combatUI.ubTopMessageType == COMPUTER_INTERRUPT_MESSAGE || gTacticalStatus.combatUI.ubTopMessageType == COMPUTER_TURN_MESSAGE )&& is_server)) && (gTacticalStatus.uiFlags & TURNBASED) && (gTacticalStatus.uiFlags & INCOMBAT))//update progress bar, not supporting coop yet...
			{
				SUpdateNetworkSoldier.usTactialTurnLimitCounter = gTacticalStatus.usTactialTurnLimitCounter;
				SUpdateNetworkSoldier.usTactialTurnLimitMax = gTacticalStatus.usTactialTurnLimitMax;
			}
			else
				SUpdateNetworkSoldier.usTactialTurnLimitCounter = 9999;
			
			client->RPC("updatenetworksoldier",(const char*)&SUpdateNetworkSoldier, (int)sizeof(EV_S_UPDATENETWORKSOLDIER)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
		}
	}
}

void UpdateSoldierFromNetwork  (RPCParameters *rpcParameters)
{
	EV_S_UPDATENETWORKSOLDIER* SUpdateNetworkSoldier = (EV_S_UPDATENETWORKSOLDIER*)rpcParameters->input;

	SOLDIERTYPE *pSoldier = SUpdateNetworkSoldier->usSoldierID;
	if ( pSoldier == NULL || !pSoldier->bActive || !pSoldier->bInSector )
	{
		return;	// MP wire guard: ignore events for soldiers not in our world (mp_audit_findings.json)
	}
	pSoldier->bActionPoints=SUpdateNetworkSoldier->bActionPoints;	// owner-authoritative AP; DeductPoints does not spend AP on remote copies
	pSoldier->bBreath=SUpdateNetworkSoldier->bBreath;
	pSoldier->stats.bLife=SUpdateNetworkSoldier->bLife;

	INT16  sCellX, sCellY;
	ConvertGridNoToCenterCellXY(SUpdateNetworkSoldier->sAtGridNo, &sCellX, &sCellY);

	if( pSoldier->sGridNo != SUpdateNetworkSoldier->sAtGridNo)
	{
		pSoldier->EVENT_InternalSetSoldierPosition( sCellX, sCellY ,FALSE, FALSE, FALSE );//new syncing call to correct network lag/drift
	}

	if(pSoldier->ubDirection != SUpdateNetworkSoldier->ubDirection)
	{
		pSoldier->EVENT_SetSoldierDesiredDirection( SUpdateNetworkSoldier->ubDirection );
	}
	if(gAnimControl[ pSoldier->usAnimState ].ubEndHeight != SUpdateNetworkSoldier->ubNewStance && pSoldier->bCollapsed != TRUE)
	{
		pSoldier->ChangeSoldierStance( SUpdateNetworkSoldier->ubNewStance );
	}
		
	pSoldier->bBleeding=SUpdateNetworkSoldier->bBleeding;

	if( (SUpdateNetworkSoldier->usTactialTurnLimitCounter != 9999) && (gTacticalStatus.combatUI.ubTopMessageType != PLAYER_TURN_MESSAGE) && (gTacticalStatus.combatUI.ubTopMessageType != PLAYER_INTERRUPT_MESSAGE))
	{
		gTacticalStatus.usTactialTurnLimitCounter = SUpdateNetworkSoldier->usTactialTurnLimitCounter;
		gTacticalStatus.usTactialTurnLimitMax = SUpdateNetworkSoldier->usTactialTurnLimitMax;
	}
}


void kick_player (void)
{
	if(is_server)
	{		
		CHAR16 Cmsg[255];

		if (cMaxClients == 2)
			swprintf(Cmsg, MPClientMessage[74], client_names[1],"<?>","<?>");
		else if (cMaxClients == 3)
			swprintf(Cmsg, MPClientMessage[74], client_names[1],client_names[2],"<?>");
		else 
			swprintf(Cmsg, MPClientMessage[74], client_names[1],client_names[2],client_names[3]);
		
		SGPRect CenterRect = { 100 + xResOffset, 100 + yResOffset, SCREEN_WIDTH - xResOffset, 300 + yResOffset };

		DoMessageBox( MSG_BOX_BASIC_STYLE, Cmsg,  guiCurrentScreen, MSG_BOX_FLAG_FOUR_NUMBERED_BUTTONS | MSG_BOX_FLAG_USE_CENTERING_RECT, kick_callback,  &CenterRect );
	}
	else	
		ScreenMsg( FONT_LTGREEN, MSG_INTERFACE, MPClientMessage[22] );
	
}

void kick_callback (UINT8 ubResult)
{	
	if (is_server)
	{
		// Pressed '1'
		if(ubResult ==1)
		{
			// WANNE: Nothing should happen			
		}
		else 
		{
			if (ubResult <= cMaxClients)
			{
				kickR kick;
				kick.ubResult=ubResult+5;
				
				client->RPC("Snull_team",(const char*)&kick, (int)sizeof(kickR)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);

				// If the team that should be kicked has the turn, give the turn to the server
				if (gTacticalStatus.ubCurrentTeam == kick.ubResult)
				{					
					EndTurn(0);
				}
			}
			else
			{
				// The client to which we should give the turn doe not exists. Do nothinig!
			}
		}		
	}
}

void null_team (RPCParameters *rpcParameters)
{
	kickR* kick = (kickR*)rpcParameters->input;
	ScreenMsg( FONT_LTGREEN, MSG_INTERFACE, MPClientMessage[29],(kick->ubResult-5),client_names[kick->ubResult-6] );
	SoldierID fID = gTacticalStatus.Team[ kick->ubResult ].bFirstID;
	SoldierID lID = gTacticalStatus.Team[ kick->ubResult ].bLastID;
	
	if(kick->ubResult==netbTeam)
		fID=0,lID=19;
	
	SoldierID cnt;
	for ( cnt=fID ; cnt <= lID; ++cnt )
	{
		TacticalRemoveSoldier( cnt );
	}

	if (kick->ubResult==netbTeam)
	{		
		gTacticalStatus.uiFlags |= SHOW_ALL_MERCS;//hayden
		ScreenMsg( FONT_YELLOW, MSG_MPSYSTEM, MPClientMessage[41] );
	}	
}

void overide_turn (void)
{
	if(is_server)
	{
		//manual overide command for server
		CHAR16 Cmsg[255];
		
		if (cMaxClients == 2)
			swprintf(Cmsg, MPClientMessage[30], client_names[1],"<?>","<?>");
		else if (cMaxClients == 3)
			swprintf(Cmsg, MPClientMessage[30], client_names[1],client_names[2],"<?>");
		else 
			swprintf(Cmsg, MPClientMessage[30], client_names[1],client_names[2],client_names[3]);
			
		SGPRect CenterRect = { 100 + xResOffset, 100 + yResOffset, SCREEN_WIDTH - 100 - xResOffset, 300 + yResOffset };
		
		DoMessageBox( MSG_BOX_BASIC_STYLE, Cmsg,  guiCurrentScreen, MSG_BOX_FLAG_FOUR_NUMBERED_BUTTONS | MSG_BOX_FLAG_USE_CENTERING_RECT | MSG_BOX_FLAG_OK, turn_callback,  &CenterRect );
	}
	else	
		ScreenMsg( FONT_LTGREEN, MSG_INTERFACE, MPClientMessage[22] );
}

void turn_callback (UINT8 ubResult)
{
	if(is_server)
	{				
		// Pressed '1'
		if(ubResult ==1)
		{
			// WANNE: Nothing should happen. Do not give the turn to the server!
			//EndTurn( 0 );
		}
		else 
		{
			if (ubResult <= cMaxClients)
			{
				ScreenMsg( FONT_LTGREEN, MSG_INTERFACE, MPClientMessage[31],ubResult );
	
				if(!( gTacticalStatus.uiFlags & INCOMBAT ))
				{
					gTacticalStatus.uiFlags |= INCOMBAT;
				}

				EndTurn( ubResult+5 );
			}
			else
			{
				// The client to which we should give the turn doe not exists. Do nothinig!
			}
		}
	}
}

void send_fireweapon (EV_S_FIREWEAPON  *SFireWeapon)
{
	EV_S_FIREWEAPON sFire;

	if(SFireWeapon->usSoldierID < 20)
		sFire.usSoldierID = (SFireWeapon->usSoldierID)+ubID_prefix;
	else
		sFire.usSoldierID = SFireWeapon->usSoldierID;

	sFire.sTargetGridNo=SFireWeapon->sTargetGridNo;
	sFire.bTargetCubeLevel=SFireWeapon->bTargetCubeLevel;
	sFire.bTargetLevel=SFireWeapon->bTargetLevel;

	client->RPC("sendFIREW",(const char*)&sFire, (int)sizeof(EV_S_FIREWEAPON)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieve_fireweapon (RPCParameters *rpcParameters)
{
	EV_S_FIREWEAPON* SFireWeapon = (EV_S_FIREWEAPON*)rpcParameters->input;

	SOLDIERTYPE *pSoldier = SFireWeapon->usSoldierID;
	if ( pSoldier == NULL || !pSoldier->bActive || !pSoldier->bInSector )
	{
		return;	// MP wire guard: ignore events for soldiers not in our world (mp_audit_findings.json)
	}

	pSoldier->sTargetGridNo = SFireWeapon->sTargetGridNo;
	pSoldier->bTargetLevel = SFireWeapon->bTargetLevel;
	pSoldier->bTargetCubeLevel = SFireWeapon->bTargetCubeLevel;
	FireWeapon( pSoldier, SFireWeapon->sTargetGridNo  );
}

void send_door ( SOLDIERTYPE *pSoldier, INT32 sGridNo, BOOLEAN fNoAnimations )
{
	if((is_server && pSoldier->ubID<120) || (!is_server && is_client && pSoldier->ubID<20) || (!is_server && !is_client) )
	{
		doors sDoor;
		sDoor.ubID=pSoldier->ubID;
		sDoor.sGridNo=sGridNo;
		sDoor.fNoAnimations=fNoAnimations;
		
		client->RPC("sendDOOR",(const char*)&sDoor, (int)sizeof(doors)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
	}
}

void recieve_door (RPCParameters *rpcParameters)
{
	doors* sDoor = (doors*)rpcParameters->input;

	SOLDIERTYPE *pSoldier = sDoor->ubID;
	if ( pSoldier == NULL || !pSoldier->bActive || !pSoldier->bInSector )
	{
		return;	// MP wire guard: ignore events for soldiers not in our world (mp_audit_findings.json)
	}
	BOOLEAN fNoAnimations = FALSE;

	if ( !AllMercsLookForDoor( sDoor->sGridNo, FALSE ) )//check for los
	{
		fNoAnimations = TRUE;
	}

	HandleDoorChangeFromGridNo( pSoldier, sDoor->sGridNo, fNoAnimations );
}

void recieveCHATMSG(RPCParameters* rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, chat_msg);	// short-frame guard (M4/H13)
	chat_msg* cmsg = (chat_msg*)rpcParameters->input;

	// PORTABLE WIRE FORMAT (H15): widen fixed-width UTF-16LE back to the engine's wchar_t.
	// MPTextFromWire always NUL-terminates, fixing the unterminated-msg over-read (M4).
	wchar_t szChat[512];
	MPTextFromWire(szChat, 512, cmsg->msg);

	if (cGameType==MP_TYPE_TEAMDEATMATCH && cmsg->bToAll == false)
	{
		// wire guard (M4): client_num indexes client_teams[4]; clamp before the read
		if (cmsg->client_num < 1 || cmsg->client_num > 4)
			return;
		// If Team deathmatch && msg is allies only
		if (client_teams[cmsg->client_num-1] == TEAM)
		{
			// only display on an ally client
			ChatLogMessage( CHAT_FONT_COLOR, MSG_CHAT, szChat);
		}
	}
	else
	{
		// display to all clients
		ChatLogMessage( CHAT_FONT_COLOR, MSG_CHAT, szChat);
	}
}

/// OJW - 20081223
// recieveDISCONNECT - Handle disconnection
void recieveDISCONNECT(RPCParameters* rpcParameters)
{
	// H10: payload is a single client-number byte; require it before reading.
	if ( ((long)((rpcParameters->numberOfBitsOfData)+7)/8) < 1 )
		return;
	// for starters - we shouldnt get a message for ourselves :)
	int cl_num = (int) *rpcParameters->input; // cl_num starts at 1
	// H10: cl_num indexes client_names[4]/client_ready[4]/client_teams[4] as [cl_num-1]
	// and Team[cl_num+5]; an out-of-range wire byte is an OOB write/read. (1..4.)
	if ( cl_num < 1 || cl_num > 4 )
		return;

	wchar_t szPlayerName[30];
	memset(szPlayerName,0,30*sizeof(wchar_t));
	mbstowcs( szPlayerName,client_names[cl_num-1],30);
	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, MPClientMessage[47], szPlayerName );

	// clear our records from this client
	memset(client_names[cl_num-1],NULL,sizeof(char)*30);
	memset(&client_ready[cl_num-1],0,sizeof(int));
	memset(&client_teams[cl_num-1],0,sizeof(int));

	if (guiCurrentScreen == MAP_SCREEN && !(gTacticalStatus.uiFlags & INCOMBAT))
	{
		// in the map screen and not in combat
		// refresh player list to remove from the game
		fDrawCharacterList = true; // set the character list to be redrawn
		fTeamPanelDirty = true; // redraw the background
	}
	else if (guiCurrentScreen == GAME_SCREEN) // <TODO> get a more valid check that the game is in progress here
	{
		// in tactical screen and in combat
		// kill the dead clients mercs out of the game

		UINT8 iNetbTeam = (cl_num)+5;
		UINT16 iubID_prefix = gTacticalStatus.Team[ iNetbTeam ].bFirstID;//over here now

		// kill any alive soldiers for the disconnected team
		SOLDIERTYPE *pTeamSoldier;
		INT32				cnt = 0;

		for ( pTeamSoldier = Menptr, cnt = 0; cnt < TOTAL_SOLDIERS; pTeamSoldier++, cnt++ )
		{
			if ( pTeamSoldier->bActive && pTeamSoldier->bInSector  && !( pTeamSoldier->flags.uiStatusFlags & SOLDIER_DEAD ) )
			{
				// Checkf for any more bacguys
				if ( !pTeamSoldier->aiData.bNeutral && (pTeamSoldier->bTeam == iNetbTeam ) )
				{
					// KIll......
					pTeamSoldier->SoldierTakeDamage( ANIM_CROUCH, pTeamSoldier->stats.bLife, 100, TAKE_DAMAGE_BLOODLOSS, NOBODY, NOWHERE, 0, TRUE );
				}
			}
		}

		// if it was that teams turn then end it
		if(is_server)
		{
			if(gTacticalStatus.ubCurrentTeam==iNetbTeam)EndTurn( iNetbTeam+1 );	
		}
	}
}

// OJW - 20090507
// this function stores a reason from the server that we were disconnected
void recieveDISCONNECTREASON(RPCParameters *rpcParameters )
{
	CHAR16* reason = (CHAR16*)rpcParameters->input;
	wcsncpy(gszDisconnectReason, reason, 254);
	gszDisconnectReason[254] = L'\0';

	is_connected=false;
	auto_retry = false;
}

void disconnected_callback(UINT8 ubResult)
{
	if (iDisconnectedScreen == MAP_SCREEN)
	{
		// clean up all resources and exit from map to main menu
		RequestTriggerExitFromMapscreen(MAP_EXIT_TO_MAINMENU);
		// game is restarted in HandleExitsFromMapScreen()
	}
	else if (iDisconnectedScreen == GAME_SCREEN)
	{
		// clean up all resources and exit from tactical to main menu
		LeaveTacticalScreen(MAINMENU_SCREEN);
	}
	else if (iDisconnectedScreen == OPTIONS_SCREEN)
	{
		// Re-initialise the game
		ReStartingGame();
		// clean up all resources and exit from options to main menu
		SetOptionsExitScreen( MAINMENU_SCREEN );
	}
	else if (iDisconnectedScreen == LAPTOP_SCREEN)
	{
		// Re-initialise the game
		ReStartingGame();
		// clean up all resources and exit from laptop to main menu
		SetPendingNewScreen(MAINMENU_SCREEN); // Laptop screen is always cleaned up on screen change in gameloop
	}
	else if (iDisconnectedScreen == MP_CONNECT_SCREEN)
	{
		// Re-initialise the game
		ReStartingGame();
		// else dont clean "everything" but still exit to main menu
		SetPendingNewScreen(MP_JOIN_SCREEN);
	}
	else
	{
		// Re-initialise the game
		ReStartingGame();
		// else dont clean "everything" but still exit to main menu
		SetPendingNewScreen(MAINMENU_SCREEN);
	}
}

// Gracefully handle self-disconnection of the client by Dropout
void HandleClientConnectionLost()
{
	if (guiCurrentScreen != MP_SCORE_SCREEN)
	{
		// cleanup client
		client_disconnect();
		auto_retry = false;

		if (is_server)
			server_disconnect();

		// connection lost, let user know via popup then quit to main menu
		iDisconnectedScreen = guiCurrentScreen;
		SGPRect CenteringRect= {0 + xResOffset, 0 + yResOffset, SCREEN_WIDTH - xResOffset, SCREEN_HEIGHT - yResOffset };

		if (wcscmp(gszDisconnectReason,L"")==0)
		{
			UINT32 giMPHMessageBox = DoMessageBox(	MSG_BOX_BASIC_STYLE,	MPClientMessage[48],	guiCurrentScreen, ( UINT16 ) ( MSG_BOX_FLAG_OK | MSG_BOX_FLAG_USE_CENTERING_RECT ),disconnected_callback,	&CenteringRect );
		}
		else
		{
			UINT32 giMPHMessageBox = DoMessageBox(	MSG_BOX_BASIC_STYLE,	gszDisconnectReason,	guiCurrentScreen, ( UINT16 ) ( MSG_BOX_FLAG_OK | MSG_BOX_FLAG_USE_CENTERING_RECT ),disconnected_callback,	&CenteringRect );
		}

	}
	else
	{
		// Tell the score screen it can continue
		gfMPSScoreScreenCanContinue = TRUE;
	}
}

void sendRT(void)
{
	if(!requested)
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, MPClientMessage[32] );
		requested=true;
		real_struct rData;
		rData.bteam=netbTeam;

		client->RPC("sendREAL",(const char*)&rData, (int)sizeof(real_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
	}
}

void gotoRT(RPCParameters *rpcParameters)
{
	getReal=true;//MAY NOT BE NEEDED ANY MORE

	gTacticalStatus.bConsNumTurnsNotSeen = 0;
	gTacticalStatus.ubCurrentTeam = OUR_TEAM;
	guiPendingOverrideEvent = LA_ENDUIOUTURNLOCK;
	ExitCombatMode();
	fInterfacePanelDirty = DIRTYLEVEL2;

	if ( (gTacticalStatus.uiFlags & TURNBASED) && (gTacticalStatus.uiFlags & INCOMBAT) )
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, MPClientMessage[34] );
	}
	else
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, MPClientMessage[33] );
	}
	
	getReal=false;
}

void startCombat(UINT8 ubStartingTeam)
{
	sc_struct data;
	
	if(ubStartingTeam==0)
		ubStartingTeam=netbTeam;

	data.ubStartingTeam=ubStartingTeam;

	client->RPC("startCOMBAT",(const char*)&data, (int)sizeof(sc_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void teamwiped ( void )
{
	extern BOOLEAN gfDedicatedServer;
	// Coordinator host is not a participant -- it has no team to be wiped, and must
	// not broadcast a spurious wipe (which would corrupt scoring / end the game).
	if ( gfDedicatedServer )
		return;

	isOwnTeamWipedOut = true;

	sc_struct data;
	data.ubStartingTeam=netbTeam;

	client->RPC("sendWIPE",(const char*)&data, (int)sizeof(sc_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);

	if (is_server)
	{
		// end the co-op game if all player teams have wiped
		if (cGameType==MP_TYPE_COOP)
		{
			iTeamsWiped++;

			if (iTeamsWiped >= cMaxClients)
				game_over();
		}
	}
}

void recieve_wipe (RPCParameters *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, sc_struct);	// short-frame guard (M5/H13)
	sc_struct* data = (sc_struct*)rpcParameters->input;
	// M5: wire team indexes TeamNameStrings[] and drives EndTurn() -- bound it.
	if ( data->ubStartingTeam >= MAXTEAMS )
		return;
	ScreenMsg( FONT_LTGREEN, MSG_INTERFACE, MPClientMessage[75], TeamNameStrings[data->ubStartingTeam] );
	if(is_server)
	{
		if(gTacticalStatus.ubCurrentTeam==data->ubStartingTeam)EndTurn( data->ubStartingTeam+1 );	

		// end the co-op game if all player teams have wiped
		if (cGameType==MP_TYPE_COOP)
		{
			iTeamsWiped++;
			if (iTeamsWiped >= cMaxClients)
				game_over();
		}
	}
}

void send_heal (SOLDIERTYPE *pSoldier )
{
	heal data;
	data.ubID=pSoldier->ubID;
	data.bLife=pSoldier->stats.bLife;
	data.bBleeding=pSoldier->bBleeding;

	client->RPC("sendHEAL",(const char*)&data, (int)sizeof(heal)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieve_heal (RPCParameters *rpcParameters)
{
	RPC_REQUIRE_BYTES(rpcParameters, heal);	// short-frame guard (H4/H13)
	heal* data = (heal*)rpcParameters->input;

	// H4: decode via the shared helper (+7, matches the rest of the codebase; the
	// old hand-rolled +6 mis-decoded the 7th merc) and guard the resolved pointer.
	SoldierID healed = MPDecodeSoldierID( data->ubID );

	SOLDIERTYPE *pSoldier = SafeMerc( healed.i );
	if ( pSoldier == NULL )
		return;
	pSoldier->bBleeding=data->bBleeding;
	pSoldier->stats.bLife=data->bLife;

#ifdef BETAVERSION
	ScreenMsg( FONT_LTGREEN, MSG_INTERFACE, L"healing..." );
#endif
}

void requestAIint(SOLDIERTYPE *pSoldier )
{
	AIint data;
	data.ubID=pSoldier->ubID;
	data.bteam=netbTeam;

#ifdef BETAVERSION
	ScreenMsg( FONT_LTGREEN, MSG_INTERFACE, L"interrupt requested" );
#endif
		
	client->RPC("rINT",(const char*)&data, (int)sizeof(AIint)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void awardINT (RPCParameters *rpcParameters)
{
	AIint* data= (AIint*)rpcParameters->input;

	SOLDIERTYPE *pSoldier = data->ubID;

	StartInterrupt();

#ifdef BETAVERSION
	ScreenMsg( FONT_LTGREEN, MSG_INTERFACE, L"interrupt awarded" );
#endif

}

void game_over()
{
	// wait 3 seconds then notify all clients
	// reset
	isOwnTeamWipedOut = false;
	
	is_game_over = true;
	iScoreScreenTime = guiBaseJA2NoPauseClock + 5000;
}

// OJW - note: i dont use the internal timer callback for this because
// death notices use it and i want all those to go through normally
// so all clients have time to receive death notice of the last merc
// which creates victory condition

void send_gameover()
{
	// handle the user calling the wrong function first
	if (!is_game_over)
	{
		game_over(); // start the event
		return;
	}

	// stop the event from firing again
	is_game_over = false;
	iScoreScreenTime = 0;

	// notify all the clients that the game is over
	client->RPC("sendGAMEOVER",(const char*)&CLIENT_NUM, (int)sizeof(CLIENT_NUM)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
}

void recieveGAMEOVER(RPCParameters *rpcParameters)
{
	player_stats* data= (player_stats*)rpcParameters->input;
	memcpy(gMPPlayerStats,data,sizeof(player_stats)*5);

	// fire the score screen
	StartScoreScreen();
}

//***************************
//*** client connection*****
//*************************

void connect_client ( void )
{
	if(!is_client)
	{
		ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, MPClientMessage[0] );
			
		client = RakNetworkFactory::GetRakPeerInterface();
		SocketDescriptor sd;
		bool b = client->Startup(1,30,&sd, 1);

		//RPC's
		REGISTER_STATIC_RPC(client, recievePATH);
		REGISTER_STATIC_RPC(client, recieveADMIN);
		REGISTER_STATIC_RPC(client, recieveSTANCE);
		REGISTER_STATIC_RPC(client, recieveDIR);
		REGISTER_STATIC_RPC(client, recieveFIRE);
		REGISTER_STATIC_RPC(client, recieveHIT);
		REGISTER_STATIC_RPC(client, recieveHIRE);
		REGISTER_STATIC_RPC(client, recieveDISMISS);
		REGISTER_STATIC_RPC(client, recieveguiPOS);
		REGISTER_STATIC_RPC(client, recieveguiDIR);
		REGISTER_STATIC_RPC(client, recieveEndTurn);
		REGISTER_STATIC_RPC(client, recieveAI);
		REGISTER_STATIC_RPC(client, recieveSTOP);
		REGISTER_STATIC_RPC(client, recieveINTERRUPT);
		REGISTER_STATIC_RPC(client, recieveREADY);
		REGISTER_STATIC_RPC(client, recieveGUI);
		REGISTER_STATIC_RPC(client, recieveSETTINGS);
		REGISTER_STATIC_RPC(client, recieveDOWNLOADSTATUS);
		REGISTER_STATIC_RPC(client, recieveFILE_TRANSFER_SETTINGS);
		REGISTER_STATIC_RPC(client, recieveTEAMCHANGE);
		REGISTER_STATIC_RPC(client, recieveEDGECHANGE);
		REGISTER_STATIC_RPC(client, recieveMAPCHANGE);
		REGISTER_STATIC_RPC(client, recieveBULLET);
		REGISTER_STATIC_RPC(client, recieveGRENADE);
		REGISTER_STATIC_RPC(client, recieveGRENADERESULT);
		REGISTER_STATIC_RPC(client, recievePLANTEXPLOSIVE);
		REGISTER_STATIC_RPC(client, recieveDETONATEEXPLOSIVE);
		REGISTER_STATIC_RPC(client, recieveDISARMEXPLOSIVE);
		REGISTER_STATIC_RPC(client, recieveSPREADEFFECT);
		REGISTER_STATIC_RPC(client, recieveNEWSMOKEEFFECT);
		REGISTER_STATIC_RPC(client, recieveEXPLOSIONDAMAGE);
		REGISTER_STATIC_RPC(client, recieveSTATE);
		REGISTER_STATIC_RPC(client, recieveDEATH);
		REGISTER_STATIC_RPC(client, recievehitSTRUCT);
		REGISTER_STATIC_RPC(client, recievehitWINDOW);
		REGISTER_STATIC_RPC(client, recieveMISS);
		REGISTER_STATIC_RPC(client, resume_turn);
		REGISTER_STATIC_RPC(client, UpdateSoldierFromNetwork);
		REGISTER_STATIC_RPC(client, recieve_fireweapon);
		REGISTER_STATIC_RPC(client, recieve_door);
		REGISTER_STATIC_RPC(client, null_team);
		REGISTER_STATIC_RPC(client, gotoRT);
		REGISTER_STATIC_RPC(client, recieve_wipe);
		REGISTER_STATIC_RPC(client, recieve_heal);
		REGISTER_STATIC_RPC(client, awardINT);
		REGISTER_STATIC_RPC(client, recieveGAMEOVER);
		REGISTER_STATIC_RPC(client, recieveDISCONNECT);
		REGISTER_STATIC_RPC(client, recieveCHATMSG);
		REGISTER_STATIC_RPC(client, requestSETID);
		REGISTER_STATIC_RPC(client, recieveDISCONNECTREASON);
		//***
		
		if (b)
		{
			is_client=true;
		}
		else
		{ 
			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[76]);
		}
			
		is_connected=false;
	}	
	
	//reconnect/connect
	if( !is_connected && !is_connecting)
	{
		gTacticalStatus.uiFlags&= (~SHOW_ALL_MERCS );

		memset( &readyteamreg , 0 , sizeof (int) * 10);
		//OJW - 20081204
		memset ( &client_names,NULL,sizeof(int)*4);
		memset ( &client_ready,0,sizeof(int)*4);
		memset ( &client_teams,0,sizeof(int)*4);

		if (!gRandomStartingEdge)
			memset ( &client_edges,0,sizeof(int)*5);

		if (gRandomMercs)
			memset (random_mercs,0,sizeof(int)*7);

		memset( gMPPlayerStats,0,sizeof(player_stats)*5);
		memset ( &client_downloading,0,sizeof(int)*4);
		memset ( &client_progress,0,sizeof(int)*4);
		memset( &gszDisconnectReason,0,sizeof(CHAR16)*255);

		// ----------------------------
		// Read from ja2_mp.ini
		// ----------------------------
		char serverIP[30];

		CIniReader iniReader(JA2MP_INI_FILENAME);
		strncpy(serverIP, iniReader.ReadString(JA2MP_INI_INITIAL_SECTION,JA2MP_SERVER_IP, ""), 30);
		strncpy(cClientName, iniReader.ReadString(JA2MP_INI_INITIAL_SECTION,JA2MP_CLIENT_NAME, ""), 30);
		strncpy(cGameDataSyncDirectory, iniReader.ReadString(JA2MP_INI_INITIAL_SECTION,JA2MP_FILE_TRANSFER_DIRECTORY, "MULTIPLAYER/Servers/My Server"), 100);

		vfs::PropertyContainer props;
		props.initFromIniFile(JA2MP_INI_FILENAME);
		UINT16 serverPort = (UINT16)props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_SERVER_PORT, 60005);
			
		// ----------------------------
		// Save to global values
		// ----------------------------
		recieved_settings=0;
		recieved_transfer_settings=0;
		goahead = 0;
		numready = 0;
		readystage = 0;
		status = 0;
		is_game_started = false;
		TEAM = 0;
		isOwnTeamWipedOut = false;
		wiped=0;
		//disable cheating
		gubCheatLevel = 0;
		cMaxClients = 0;	//reset server only set settings.			
		cDamageMultiplier=0;
		cSameMercAllowed=0;
		cDisableSpectatorMode=0;

		if(is_server)
			strcpy(serverIP, "127.0.0.1" );

		//**********************
		//here some nifty little tweaks

		LaptopSaveInfo.guiNumberOfMercPaymentsInDays += 20;
		LaptopSaveInfo.gubLastMercIndex = LAST_MERC_ID;
		LaptopSaveInfo.ubLastMercAvailableId = 7;
		gGameExternalOptions.fEnableSlayForever	= 1;
		LaptopSaveInfo.gubPlayersMercAccountStatus = 4;
				
		// WANNE: This should fix the bug playing a "tilt" sound and showing the mini laptop graphic on the screen, when opening the laptop / option screen from map screen from map screen
		gfDontStartTransitionFromLaptop = TRUE;
										
		//**********************

		// WANNE: FILE TRANSFER: Build the absolut file transfer directory path for the client
		GetExecutableDirectory(client_executableDir);

		strcpy(client_fileTransferDirectoryPath, client_executableDir);
		strcat(client_fileTransferDirectoryPath, "\\");
		strcat(client_fileTransferDirectoryPath, cGameDataSyncDirectory);

		// WANNE: FILE TRANSFER
		client->AttachPlugin(&fltClient);
		client->SetSplitMessageProgressInterval(1);
		
		CHAR16 tmpMessage[512];
		swprintf( tmpMessage, MPClientMessage[1],serverIP );
		ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, tmpMessage); // we are connecting
		SetConnectScreenSubMessageW( tmpMessage );

		client->Connect(serverIP, serverPort, 0,0);
		is_connecting = true;	
	}
	else if (is_connecting)
	{
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[4] );
	}
	else if (is_connected)
	{
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[3] );
	}
}

void client_packet ( void )
{	
	Packet* p;

	if (is_client)
	{
		p = client->Receive();

		while(p)
		{
			// We got a packet, get the identifier with our handy function
			packetIdentifier = GetPacketIdentifier(p);
			
			// Check if this is a network message packet
			switch (packetIdentifier)
			{
				case ID_DISCONNECTION_NOTIFICATION:
					  // Connection lost normally
					ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, L"ID_DISCONNECTION_NOTIFICATION");
					is_connected=false;
					//OJW - 20081223
					//Gracefully notify and disconnect the client
					client->DeallocatePacket(p); // HandleClientConnectionLost shuts down the client
					HandleClientConnectionLost();
					//OJW - 2009
					return;
					break;
				case ID_ALREADY_CONNECTED:
					// Connection lost normally
					ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, L"ID_ALREADY_CONNECTED");
					break;
				case ID_REMOTE_DISCONNECTION_NOTIFICATION: // Server telling the clients of another client disconnecting gracefully.  You can manually broadcast this in a peer to peer enviroment if you want.
					ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, L"ID_REMOTE_DISCONNECTION_NOTIFICATION");
					break;
				case ID_REMOTE_CONNECTION_LOST: // Server telling the clients of another client disconnecting forcefully.  You can manually broadcast this in a peer to peer enviroment if you want.
					ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, L"ID_REMOTE_CONNECTION_LOST");
					break;
				case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
					ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, L"ID_REMOTE_NEW_INCOMING_CONNECTION");
					break;
				case ID_CONNECTION_ATTEMPT_FAILED:
					ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, L"ID_CONNECTION_ATTEMPT_FAILED");
					is_connected=false;
					is_connecting=false;

					//OJW - 20081224
					CHAR16 msgString[512];
					// handle auto retry
					if (auto_retry && giNumTries > 0)
					{
						swprintf( msgString, MPClientMessage[49],giNumTries );
						ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, msgString); // we already tried once, let the user know we are retrying
						SetConnectScreenSubMessageW( msgString );
					}
					else
					{
						swprintf( msgString, MPClientMessage[50],giNumTries );
						ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, msgString); // we already tried once, let the user know we are retrying
						SetConnectScreenSubMessageW( msgString );
					}
					giNextRetryTime = guiBaseJA2NoPauseClock + 5000; // 5 seconds?
					break;
				case ID_NO_FREE_INCOMING_CONNECTIONS:
					 // Sorry, the server is full.  I don't do anything here but
					// A real app should tell the user
					ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, L"ID_NO_FREE_INCOMING_CONNECTIONS");
					break;
				case ID_CONNECTION_LOST:
					// Couldn't deliver a reliable packet - i.e. the other system was abnormally
					// terminated
					ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, L"ID_CONNECTION_LOST");
					
					is_connected=false;
					//OJW - 20081223
					//Gracefully notify and disconnect the client
					client->DeallocatePacket(p); // HandleClientConnectionLost shuts down the client
					HandleClientConnectionLost();
					return;
					break;
				case ID_CONNECTION_REQUEST_ACCEPTED:
					// This tells the client they have connected
					ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, L"ID_CONNECTION_REQUEST_ACCEPTED");
					is_connected=true;
					is_connecting=false;

					// WANNE: FILE TRANSFER: Send all the data that is needed for the file transfer to the client,
					// before the actual file transfer begins
					requestFILE_TRANSFER_SETTINGS();

					requestSETTINGS();
					break;
				case ID_NEW_INCOMING_CONNECTION:
					//tells server client has connected
					ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, L"ID_NEW_INCOMING_CONNECTION");
					break;
				case ID_MODIFIED_PACKET:
					// Cheater!
					ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, L"ID_MODIFIED_PACKET");
					break;
				default:
					ScreenMsg( FONT_BEIGE, MSG_MPSYSTEM, L"** a packet has been recieved for which i dont know what to do... **");
					break;
			}

			// We're done with the packet, get more :)
			client->DeallocatePacket(p);
			p = client->Receive();
		}

		// OJW - 20081223
		if (is_game_over)
		{
			if (guiBaseJA2NoPauseClock >= iScoreScreenTime)
			{
				send_gameover();
			}
		}

		// OJW - 20090203
		// Using the built in callback functions didnt work, so doing manually here
		if (bClosingChatBoxToStartGame)
		{
			if (guiBaseJA2NoPauseClock >= iCCStartGameTime)
			{
				bClosingChatBoxToStartGame = false;
				StartBattleChatBoxClosedCallback();
			}
		}
	}
}

// Copied from Multiplayer.cpp
// If the first byte is ID_TIMESTAMP, then we want the 5th byte
// Otherwise we want the 1st byte
unsigned char GetPacketIdentifier(Packet *p)
{
	if (p==0)
		return 255;

	if ((unsigned char)p->data[0] == ID_TIMESTAMP)
	{
		assert(p->length > sizeof(unsigned char) + sizeof(unsigned long));
		return (unsigned char) p->data[sizeof(unsigned char) + sizeof(unsigned long)];
	}
	else
		return (unsigned char) p->data[0];
}


void client_disconnect (void)
{
	if(is_client)
	{
		client->DetachPlugin(&fltClient);

		client->Shutdown(300);
		is_client = false;
		is_connected=false;
		is_connecting=false;
		
		fileTransferProgress = 0;
		allowlaptop=false;

		TEAM=0;	
		
		// clear local client cache
		memset(client_names,0,sizeof(char)*4*30);
		memset(client_edges,0,sizeof(int)*5);
		memset(client_ready,0,sizeof(int)*4);
		memset(client_teams,0,sizeof(int)*4);
		memset(gMPPlayerStats,0,sizeof(player_stats)*5);
		memset(random_mercs,0,sizeof(int)*7);
		memset ( &client_downloading,0,sizeof(int)*4);
		memset ( &client_progress,0,sizeof(int)*4);	

		// We're done with the network
		RakNetworkFactory::DestroyRakPeerInterface(client);
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[77]);

		#ifdef JA2BETAVERSION
			MPDebugMsg( "client_disconnect()\n" );
		#endif
	}
	else
	{
		ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, MPClientMessage[78]);
	}
}

//OJW - 20081204 - send a starting edge change to all the clients
void send_edgechange(int newedge)
{
	// <TODO> remove this godawful hack with a proper game status check
	if (can_edgechange())
	{
		edgechange_struct lan;

		lan.client_num = CLIENT_NUM;
		lan.newedge = newedge;

		client->RPC("sendEDGECHANGE",(const char*)&lan, (int)sizeof(edgechange_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);

		// redraw the character list on the map screen
		fDrawCharacterList = true;
		fTeamPanelDirty = true;
		client_edges[CLIENT_NUM-1] =newedge;
		cStartingSectorEdge = newedge;
	}
	else
	{
		ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, gszMPMapscreenText[3]);
	}
}

bool can_edgechange()
{
	return (is_game_started != 1 && client_ready[CLIENT_NUM-1] == 0 && !allowlaptop && !gRandomStartingEdge);
}

//OJW - 20081204 - send a starting team change to all the clients
void send_teamchange(int newteam)
{
	// <TODO> remove this godawful hack with a proper game status check
	if (can_teamchange())
	{
		teamchange_struct lan;

		lan.client_num = CLIENT_NUM;
		lan.newteam = newteam;

		client->RPC("sendTEAMCHANGE",(const char*)&lan, (int)sizeof(teamchange_struct)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);

		// redraw the character list on the map screen
		fDrawCharacterList = true;
		fTeamPanelDirty = true;
		client_teams[lan.client_num-1] = lan.newteam;
		TEAM = newteam;
	}
	else
	{
		ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, gszMPMapscreenText[4]);
	}
}

bool can_teamchange()
{
	bool isGeneralTeamchangeValid = (is_game_started != 1 && client_ready[CLIENT_NUM-1] == 0 && !allowlaptop);
	
	if (isGeneralTeamchangeValid && cGameType == MP_TYPE_TEAMDEATMATCH)
		return TRUE;
	else
		return FALSE;
}

// 20081222 - OJW
void StartScoreScreen( void )
{
	// pause game
	
	// set main screen as score screen
	LeaveTacticalScreen( MP_SCORE_SCREEN );
}

void ChatCallback( UINT8 ubResult )
{
	if (ubResult == MSG_BOX_RETURN_OK && wcscmp(gszChatBoxInputString,L"") > 0)
	{
		chat_msg cmsg;
		memset(&cmsg, 0, sizeof(cmsg));
		wchar_t szPlayerName[30];
		memset(szPlayerName,0,30*sizeof(wchar_t));
		mbstowcs( szPlayerName,cClientName,30);

		cmsg.bToAll = gbChatSendToAll;
		// PORTABLE WIRE FORMAT (H15): format into a wchar_t buffer, then narrow to fixed-width
		// UTF-16LE for the wire so the byte layout is identical on every platform.
		wchar_t szChat[512];
		szChat[0] = L'\0';
		if (cGameType==MP_TYPE_TEAMDEATMATCH && !cmsg.bToAll)
			swprintf(szChat,MPClientMessage[56], szPlayerName, gszChatBoxInputString);
		else
			swprintf(szChat,MPClientMessage[52], szPlayerName, gszChatBoxInputString);
		MPTextToWire(cmsg.msg, 512, szChat);
		cmsg.client_num = CLIENT_NUM;

		// notify all of the chat message
		client->RPC("sendCHATMSG",(const char*)&cmsg, (int)sizeof(chat_msg)*8, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID,0);
	}

	memset(gszMsgBoxInputString,0,sizeof(gszMsgBoxInputString));
}

void OpenChatMsgBox( void )
{
	DoChatBox((guiCurrentScreen == GAME_SCREEN? true : false),gzMPChatboxText[1],guiCurrentScreen,ChatCallback,NULL);
}
