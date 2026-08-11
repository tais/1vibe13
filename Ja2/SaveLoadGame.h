#ifndef _SAVE_LOAD_GAME_H_
#define _SAVE_LOAD_GAME_H_

#include "GameSettings.h"

//forward declarations of common classes to eliminate includes
class OBJECTTYPE;
class TacticalActor;


#define		BYTESINMEGABYTE						1048576 //1024*1024
#define		REQUIRED_FREE_SPACE				(20 * BYTESINMEGABYTE)

#define		SIZE_OF_SAVE_GAME_DESC				128

#define		GAME_VERSION_LENGTH					16

#define		SAVE__ERROR_NUM						249//99

// WANNE:	The end turn have 2 saves (Auto00.sav and Auto01.sav). This 2 save slots should be underneath the 5 auto save slots
#define		SAVE__END_TURN_NUM					248//98
#define		SAVE__END_TURN_NUM_1				6//98
#define		SAVE__END_TURN_NUM_2				7//98

// WDS - Automatically try to save when an assertion failure occurs

// WANNE:	This SAVEGAME should not show up in the load screen
#define		SAVE__ASSERTION_FAILURE				247//97

#define     SAVE__TIMED_AUTOSAVE_SLOT1				1 //19	// WANNE: This slot does not show up in the load/save screen because it is > 18
#define     SAVE__TIMED_AUTOSAVE_SLOT2				2 
#define     SAVE__TIMED_AUTOSAVE_SLOT3				3
#define     SAVE__TIMED_AUTOSAVE_SLOT4				4
#define     SAVE__TIMED_AUTOSAVE_SLOT5				5

#define		EARLIST_SPECIAL_SAVE				247//97


typedef struct
{
	UINT32			uiSavedGameVersion;
	CHAR8			zGameVersionNumber[ GAME_VERSION_LENGTH ];
	CHAR16			sSavedGameDesc[ SIZE_OF_SAVE_GAME_DESC ];
	UINT32			uiFlags;
#ifdef CRIPPLED_VERSION
	UINT8			ubCrippleFiller[20];
#endif
	//The following will be used to quickly access info to display in the save/load screen
	UINT32			uiDay;
	UINT8			ubHour;
	UINT8			ubMin;
	INT16			sSectorX;
	INT16			sSectorY;
	INT8				bSectorZ;
	UINT16			ubNumOfMercsOnPlayersTeam;
	INT32			iCurrentBalance;
	UINT32			uiCurrentScreen;
	BOOLEAN			fAlternateSector;
	BOOLEAN			fWorldLoaded;
	UINT8			ubLoadScreenID;		//The load screen that should be used when loading the saved game
	GAME_OPTIONS		sInitialGameOptions;	//need these in the header so we can get the info from it on the save load screen.
	UINT32			uiRandom;
	UINT8			ubFiller[494];	// WANNE: Decrease this filler by 1, for each new UINT8 variable!

} SAVED_GAME_HEADER;

// The portable save schema is deliberately visible here so production and
// data-free golden tests instantiate the exact same field visitor. Keeping
// this list next to the record declaration makes changes to either side
// reviewable as one save-format decision. The save/load screen reads this
// header before it knows the version, so the portable form is unconditional:
// CHAR16 description through wstr, scalar-only GAME_OPTIONS and reserved
// filler as exact byte blocks.
template<class Ar>
inline void XferSaveGameHeaderFields( Ar& ar, SAVED_GAME_HEADER& h )
{
	ar.u32 (h.uiSavedGameVersion);
	ar.str8(h.zGameVersionNumber, GAME_VERSION_LENGTH);
	ar.wstr(h.sSavedGameDesc, SIZE_OF_SAVE_GAME_DESC);
	ar.u32 (h.uiFlags);
#ifdef CRIPPLED_VERSION
	ar.bytes(h.ubCrippleFiller, sizeof(h.ubCrippleFiller));
#endif
	ar.u32 (h.uiDay);
	ar.u8  (h.ubHour);
	ar.u8  (h.ubMin);
	ar.i16 (h.sSectorX);
	ar.i16 (h.sSectorY);
	ar.i8  (h.bSectorZ);
	ar.u16 (h.ubNumOfMercsOnPlayersTeam);
	ar.i32 (h.iCurrentBalance);
	ar.u32 (h.uiCurrentScreen);
	ar.boolean(h.fAlternateSector);
	ar.boolean(h.fWorldLoaded);
	ar.u8  (h.ubLoadScreenID);
	ar.bytes(&h.sInitialGameOptions, sizeof(GAME_OPTIONS));
	ar.u32 (h.uiRandom);
	ar.bytes(h.ubFiller, sizeof(h.ubFiller));
}

// PathSt is forward-declared below, so keep the node type dependent. Runtime
// links are intentionally absent from the stream and are rebuilt by each
// route owner after loading.
template<class Ar, class PathNode>
inline void XferPathNodeFields( Ar& ar, PathNode& p )
{
	ar.u32(p.uiSectorId);
	ar.u32(p.uiEta);
	ar.boolean(p.fSpeed);
}

extern	UINT32		guiScreenToGotoAfterLoadingSavedGame;
extern UINT32 guiCurrentSaveGameVersion;

void			CreateSavedGameFileNameFromNumber( UINT8 ubSaveGameID, CHAR8 *pzNewFileName, size_t filenameCapacity );


BOOLEAN InitSaveDir();

BOOLEAN SaveGame( int ubSaveGameID, CHAR16 *pGameDesc );
BOOLEAN LoadSavedGame( int ubSavedGameID );

// Portable (save-format v2) header read/write -- see SaveLoadGame.cpp.
BOOLEAN SaveSaveGameHeaderToFile( HWFILE hFile, SAVED_GAME_HEADER& h );
BOOLEAN LoadSaveGameHeaderFromFile( HWFILE hFile, SAVED_GAME_HEADER& h );

// Portable (save-format v2) PathSt node (path data only; links re-built on load).
struct path;
BOOLEAN SavePathNodeToFile( HWFILE hFile, struct path* p );
BOOLEAN LoadPathNodeFromFile( HWFILE hFile, struct path* p );
BOOLEAN SaveMercPathFromSoldierStruct( HWFILE hFile, UINT16 ubID );
BOOLEAN LoadMercPathToSoldierStruct( HWFILE hFile, UINT16 ubID );

UINT32 ComputeTacticalActorChecksum( const TacticalActor& actor );
BOOLEAN SaveTacticalActor( HWFILE hFile, TacticalActor& actor );
BOOLEAN LoadTacticalActor( HWFILE hFile, TacticalActor& actor );

BOOLEAN CopySavedSoldierInfoToNewSoldier( TacticalActor *pDestSourceInfo, TacticalActor *pSourceInfo );

BOOLEAN		SaveFilesToSavedGame( STR pSrcFileName, HWFILE hFile );
BOOLEAN		LoadFilesFromSavedGame( STR pSrcFileName, HWFILE hFile );

void GetBestPossibleSectorXYZValues( INT16 *psSectorX, INT16 *psSectorY, INT8 *pbSectorZ );


extern UINT32	guiLastSaveGameNum;			// The end turn auto save number (0 = Auto00.sav, 1 = Auto01.sav)

/*CHRISL: This function is designed to allow reading the save game file one field at a time.  We currently save structures by saving a block of memory,
but variables are stored in memory so that they fit neatly into a WORD resulting in the program automatically adding some padding.  This padding is saved
during the save game process and this function is designed to calculate where that padding is so that we can account for it during the load process.  The
use of this function should allow changes to be made to various structures within the designated "POD", while still allowing for save game continuity.*/
INT32 ReadFieldByField( HWFILE hFile, PTR pDest, UINT32 uiFieldSize, UINT32 uiElementSize, UINT32  uiCurByteCount );

#endif
