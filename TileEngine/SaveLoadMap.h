#ifndef		_SAVELOADMAP__H_
#define		_SAVELOADMAP__H_

#include "worlddef.h"
#include "Exit Grids.h"
#include "CampaignMapChangeCodes.h"

// Common legacy raw values remain source-compatible. The campaign-specific
// tail is deliberately qualified because its numeric slots overlap.
inline constexpr UINT8 SLM_NONE = 0;
inline constexpr UINT8 SLM_LAND = 1;
inline constexpr UINT8 SLM_OBJECT = 2;
inline constexpr UINT8 SLM_STRUCT = 3;
inline constexpr UINT8 SLM_SHADOW = 4;
inline constexpr UINT8 SLM_MERC = 5;
inline constexpr UINT8 SLM_ROOF = 6;
inline constexpr UINT8 SLM_ONROOF = 7;
inline constexpr UINT8 SLM_TOPMOST = 8;
inline constexpr UINT8 SLM_REMOVE_LAND = 9;
inline constexpr UINT8 SLM_REMOVE_OBJECT = 10;
inline constexpr UINT8 SLM_REMOVE_STRUCT = 11;
inline constexpr UINT8 SLM_REMOVE_SHADOW = 12;
inline constexpr UINT8 SLM_REMOVE_MERC = 13;
inline constexpr UINT8 SLM_REMOVE_ROOF = 14;
inline constexpr UINT8 SLM_REMOVE_ONROOF = 15;
inline constexpr UINT8 SLM_REMOVE_TOPMOST = 16;
inline constexpr UINT8 SLM_BLOOD_SMELL = 17;
inline constexpr UINT8 SLM_DAMAGED_STRUCT = 18;
inline constexpr UINT8 SLM_EXIT_GRIDS = 19;
inline constexpr UINT8 SLM_OPENABLE_STRUCT = 20;
inline constexpr UINT8 SLM_WINDOW_HIT = 21;
inline constexpr UINT8 SLM_ARULCO_MINE_PRESENT =
	CampaignMapChangeCode::ArulcoMinePresent;
inline constexpr UINT8 SLM_ARULCO_REMOVE_MINE_PRESENT =
	CampaignMapChangeCode::ArulcoRemoveMinePresent;
inline constexpr UINT8 SLM_ARULCO_DECAL =
	CampaignMapChangeCode::ArulcoDecal;
inline constexpr UINT8 SLM_UB_REMOVE_EXIT_GRID =
	CampaignMapChangeCode::UnfinishedBusinessRemoveExitGrid;
inline constexpr UINT8 SLM_UB_MINE_PRESENT =
	CampaignMapChangeCode::UnfinishedBusinessMinePresent;
inline constexpr UINT8 SLM_UB_REMOVE_MINE_PRESENT =
	CampaignMapChangeCode::UnfinishedBusinessRemoveMinePresent;
inline constexpr UINT8 SLM_UB_DECAL =
	CampaignMapChangeCode::UnfinishedBusinessDecal;

typedef struct//dnl ch86 250214
{
	INT32 usGridNo;				// The gridno the graphic will be applied to
	UINT16 usImageType;			// graphic index
	UINT16 usSubImageIndex;		// ExitGrid low WORD of usGridno is stored here
	UINT8 ubType;				// the layer it will be applied to
	UINT8 ubExtra;				// Misc. variable used to strore arbritary values
	UINT16 usHiExitGridNo;		// ExitGrid.usGridno is store in usSubImageIndex which is not enough for big maps so high WORD goes here just to preserve compatibility
} MODIFY_MAP;

// Call this function, to set whether the map changes will be added to the	map temp file
void	ApplyMapChangesToMapTempFile( BOOLEAN fAddToMap );

BOOLEAN SaveModifiedMapStructToMapTempFile( MODIFY_MAP *pMap, INT16 sSectorX, INT16 sSectorY, INT8 bSectorZ );


//Applies a change TO THE MAP TEMP file
void AddStructToMapTempFile( INT32 iMapIndex, UINT16 usIndex );


//Applies a change TO THE MAP from the temp file
void AddStructFromMapTempFileToMap( INT32 iMapIndex, UINT16 usIndex );


void AddObjectToMapTempFile( INT32 uiMapIndex, UINT16 usIndex );


BOOLEAN LoadAllMapChangesFromMapTempFileAndApplyThem( );


void RemoveStructFromMapTempFile( INT32 uiMapIndex, UINT16 usIndex );

// Flugente: permanently remove other tiles
void RemoveRoofFromMapTempFile( INT32 uiMapIndex, UINT16 usIndex );
void RemoveOnRoofFromMapTempFile( INT32 uiMapIndex, UINT16 usIndex );

void AddRemoveObjectToMapTempFile( INT32 uiMapIndex, UINT16 usIndex );

void SaveBloodSmellAndRevealedStatesFromMapToTempFile();

// sevenfm
void SaveMineFlagFromMapToTempFile();
void RemoveMineFlagFromMapTempFile( INT32 usGridNo);

BOOLEAN SaveRevealedStatusArrayToRevealedTempFile( INT16 sSectorX, INT16 sSectorY, INT8 bSectorZ );

BOOLEAN LoadRevealedStatusArrayFromRevealedTempFile();


void AddRemoveObjectToUnLoadedMapTempFile( INT32 uiMapIndex, UINT16 usIndex, INT16 sSectorX, INT16 sSectorY, UINT8 ubSectorZ  );
void RemoveStructFromUnLoadedMapTempFile( INT32 uiMapIndex, UINT16 usIndex, INT16 sSectorX, INT16 sSectorY, UINT8 ubSectorZ  );
void AddObjectToUnLoadedMapTempFile( INT32 uiMapIndex, UINT16 usIndex, INT16 sSectorX, INT16 sSectorY, UINT8 ubSectorZ  );
void AddStructToUnLoadedMapTempFile( INT32 uiMapIndex, UINT16 usIndex, INT16 sSectorX, INT16 sSectorY, UINT8 ubSectorZ  );

//Adds the exit grid to
void AddExitGridToMapTempFile( INT32 usGridNo, EXITGRID *pExitGrid, INT16 sSectorX, INT16 sSectorY, UINT8 ubSectorZ );


//This function removes a struct with the same MapIndex and graphic index from the given sectors temp file
BOOLEAN RemoveGraphicFromTempFile( INT32 uiMapIndex, UINT16 usIndex, INT16 sSectorX, INT16 sSectorY, UINT8 ubSectorZ );


void SetOpenableStructStatusFromMapTempFile( INT32 uiMapIndex, BOOLEAN fOpened );
void AddOpenableStructStatusToMapTempFile( INT32 uiMapIndex, BOOLEAN fOpened );

void AddWindowHitToMapTempFile( INT32 uiMapIndex );

BOOLEAN ChangeStatusOfOpenableStructInUnloadedSector( UINT16 usSectorX, UINT16 usSectorY, INT8 bSectorZ, INT32 usGridNo, BOOLEAN fChangeToOpen );

//ja25 ub
void AddRemoveExitGridToUnloadedMapTempFile( UINT32 usGridNo, INT16 sSectorX, INT16 sSectorY, UINT8 ubSectorZ );

#endif
