	//#include "Laptop All.h"
	#include "laptop.h"
	#include "aim.h"
	#include "Utilities.h"
	#include "WCheck.h"
	#include "WordWrap.h"
	#include "Text.h"
	#include "Encrypted File.h"
	#include "Soldier Profile.h"
	#include "Sound Control.h"
	#include "Campaign Types.h"
	#include "Quests.h"
	#include "Tactical Save.h"
	#include "BriefingRoom_Data.h"
	#include "LaptopPageResourceOwner.h"
	#include "LaptopRecordFile.h"
	#include "LaptopUiStateModel.h"
	#include "VideoResourceHandle.h"
	#include <algorithm>
	#include <cstdio>
	#include <iterator>


#define MAX_FILTR_LOCATION_BUTTONS 11
#define FILTR_INVENTORY_BUTTONS 5
#define FILTR_BUTTONS_CHARACTER 8
#define NO_FILTR -1
#define NONE_FILTR 0


UINT32 	gFiltrBox = -1;

#ifdef ENCYCLOPEDIA_WORKS
INT32 	guiEncyclopediaLocationPageButton[3] = { -1,-1,-1 };
INT32  	guiEncyclopediaLocationPageButtonImage = -1;
INT32 	guiEncyclopediaPageButton[3] = { -1,-1,-1 };
INT32  	guiEncyclopediaiPageButtonImage =  -1;
UINT32 	guiEncyclopediaPopUp;
#endif

INT32 	ID = 0;
INT32 	IDimage = 0;
INT32 	MaxImages = 1;
INT32 	IDNewLocation = 0;
UINT32 	LocationID = 0;
INT32 	MaxLocation = 0;
INT32 	ENCYCLOPEDIA_PAGE =	2;
BOOLEAN	fFirstTimeInEncyclopediaLocation = TRUE;
UINT32 FiltrID = 0;
BOOLEAN ResetVal = FALSE;

UINT32 IDPageEncyData;
static void InitData(BOOLEAN bInit);

#define		ENCYCLOPEDIA_LOCATION_PAGE1_X					(LAPTOP_SCREEN_UL_X + 100)
#define		ENCYCLOPEDIA_LOCATION_PAGE1_Y					(LAPTOP_SCREEN_WEB_UL_Y + 357)
#define		ENCYCLOPEDIA_LOCATION_FILTER_Y					(LAPTOP_SCREEN_WEB_UL_Y + 10)
#define		ENCYCLOPEDIA_LOCATION_PAGE_COLOR_UP				FONT_MCOLOR_DKWHITE
#define		ENCYCLOPEDIA_LOCATION_PAGE_COLOR_DOWN			138
#define		ENCYCLOPEDIA_LOCATION_PAGE_FONT					FONT10ARIAL
#define		ENCYCLOPEDIA_LOCATION_PAGE_GAP					ENCYCLOPEDIA_LOCATION_BOTTOM_BUTTON_START_WIDTH + 25
#define		ENCYCLOPEDIA_LOCATION_BOTTOM_BUTTON_START_WIDTH	75
#define		ENCYCLOPEDIA_LOCATION_BOX_X						LAPTOP_SCREEN_UL_X + (500-ENCYCLOPEDIA_LOCATION_BOX_WIDTH)/2
#define		ENCYCLOPEDIA_LOCATION_BOX_Y						iScreenHeightOffset + 260 + LAPTOP_SCREEN_WEB_DELTA_Y
#define		ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP			4
#define		ENCYCLOPEDIA_LOCATION_BOX_WIDTH					400//309
#define		ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH			400//296
#define		ENCYCLOPEDIA_LOCATION_BOX_SECTION_HEIGHT		9
#define		ENCYCLOPEDIA_LOCATION_BOX_FONT					FONT10ARIAL
#define		ENCYCLOPEDIA_LOCATION_BOX_COLOR					FONT_MCOLOR_WHITE
#define		ENCYCLOPEDIA_LOCATION_BOX_DESC_X				ENCYCLOPEDIA_LOCATION_BOX_X + 8
#define		ENCYCLOPEDIA_LOCATION_PANEL_HEIGHT				0//52
#define		ENCYCLOPEDIA_LOCATION_PANEL_Y					ENCYCLOPEDIA_LOCATION_BOX_Y + 6
#define		ENCYCLOPEDIA_LOCATION_BOX_DESC_Y				ENCYCLOPEDIA_LOCATION_PANEL_Y + ENCYCLOPEDIA_LOCATION_PANEL_HEIGHT + 5

#define ENCYCLOPEDIA_IMAGE_X LAPTOP_SCREEN_UL_X
#define ENCYCLOPEDIA_IMAGE_Y iScreenHeightOffset + LAPTOP_SCREEN_WEB_DELTA_Y
#define ENCYCLOPEDIA_FILTR_BUTTON_GAP	16

#ifdef ENCYCLOPEDIA_WORKS
void SelectEncyclopediaLocationPageRegionCallBack(GUI_BUTTON * btn, INT32 reason );
void SelectEncyclopediaLocationRegionCallBack(GUI_BUTTON * btn, INT32 reason );
#endif

void SelectFiltrButtonsRegionCallBack(GUI_BUTTON * btn, INT32 reason );

//void BtnEncyclopediaTogglesCallback( GUI_BUTTON *btn, INT32 reason );
void RenderButtonDisabled();
void RenderButtonDisabled2();
void RenderButtonDisabled3();
void RenderButtonDisabled4();
void RenderBoxDisabledButton();
static BOOLEAN RenderMap();
void CopyToTemp ( BRIEFINGROOM_M_DATA *Ency, BOOLEAN bFiltr, INT32 sort, INT32 TypFiltr, BOOLEAN ShowBox );
void ResetTemp();
static BOOLEAN LoadSoundButtons(LaptopPageResourceOwner& owner);

#ifdef ENCYCLOPEDIA_WORKS
MOUSE_REGION	gSelectedEncyclopediaTextRegion[ MAX_FILTR_LOCATION_BUTTONS ];
#endif

BOOLEAN bBriefingRoom  = FALSE;
BOOLEAN bBriefingRoomSpecialMission  = FALSE;

BOOLEAN bPage2 = FALSE;
BOOLEAN bPage3 = FALSE;

#define MAX_MISSION_BUTTONS 2

BOOLEAN bSoundButtons = FALSE;
INT32 	guiSoundButtons[ MAX_MISSION_BUTTONS ] = {
	BUTTON_NO_SLOT, BUTTON_NO_SLOT};
UINT32	uiSoundSampleBR = NO_SAMPLE;
INT32  	guiSoundButtonsImage = -1;
void SelectSoundButtonsRegionCallBack(GUI_BUTTON * btn, INT32 reason );

BOOLEAN LoadBriefingRoomFromLoadGameFile( HWFILE hFile );
BOOLEAN SaveBriefingRoomToSaveGameFile( HWFILE hFile );

void BackupBRandEncyclopedia ( BRIEFINGROOM_M_DATA *EncyBackup, BRIEFINGROOM_M_DATA *Ency, UINT32 NUM);

#ifdef ENABLE_BRIEFINGROOM
BRIEFINGROOM_M_DATA gbriefingRoomDataTemp[ NUM_MAX_TEMP ];
#endif

#ifdef ENABLE_BRIEFINGROOM
BRIEFINGROOM_M_DATA gBriefingRoomData[ NUM_MISSION ];
BRIEFINGROOM_M_DATA gBriefingRoomSpecialMissionData[ NUM_MISSION ];

BRIEFINGROOM_M_DATA gBriefingRoomDataBackup[ NUM_MISSION ];
BRIEFINGROOM_M_DATA gBriefingRoomSpecialMissionDataBackup[ NUM_MISSION ];

MISSION_SAVE saveBriefingRoomData[ NUM_MISSION ];
MISSION_SAVE saveBriefingRoomSpecialMissionData[ NUM_MISSION ];
MISSION_SAVE saveMissionData[ NUM_MISSION ];
#else
BRIEFINGROOM_M_DATA gBriefingRoomData[ 1 ];
#endif // ENABLE_BRIEFINGROOM

#ifdef ENABLE_BRIEFINGROOM
BRIEFINGROOM_M_DATA gLoadBriefingRoomReservedData1[ NUM_SECTOR ];
BRIEFINGROOM_M_DATA gLoadBriefingRoomReservedData2[ NUM_PROFILES ];
BRIEFINGROOM_M_DATA gLoadBriefingRoomReservedData3[ MAXITEMS ];
BRIEFINGROOM_M_DATA gLoadBriefingRoomReservedData4[ NUM_PROFILES ];
BRIEFINGROOM_M_DATA gLoadBriefingRoomReservedData5[ MAX_QUESTS ];

BOOLEAN saveBriefingRoomReservedData1[ NUM_SECTOR ];
ENCYCLOPEDIA_PROF_SAVE saveBriefingRoomReservedData2[ NUM_PROFILES ];
BOOLEAN saveBriefingRoomReservedData3[ MAXITEMS ];
BOOLEAN saveBriefingRoomReservedData4[ NUM_PROFILES ];
BOOLEAN saveBriefingRoomReservedData5[ MAX_QUESTS ];
#else
BRIEFINGROOM_M_DATA gLoadBriefingRoomReservedData1[];
#endif // ENABLE_BRIEFINGROOM

UINT32		guiRustEncyclopediaBackGround;
static LaptopPageResourceOwner gBriefingDataResources;

#ifdef ENABLE_BRIEFINGROOM
static BRIEFINGROOM_M_DATA* CurrentBriefingEntry()
{
	if (MaxLocation <= 0 ||
		!LaptopUiStateModel::IsValidIndex(NUM_MAX_TEMP, LocationID) ||
		LocationID >= static_cast<UINT32>(MaxLocation))
		return nullptr;
	return &gbriefingRoomDataTemp[LocationID];
}
#endif

void ShowNPCEncyclopediaEntry(UINT8 ubNPC, BOOLEAN hidden)
{
#ifdef ENABLE_BRIEFINGROOM
	if (LaptopUiStateModel::IsValidIndex(NUM_PROFILES, ubNPC))
		gLoadBriefingRoomReservedData2[ubNPC].Hidden = hidden;
#endif
}

BOOLEAN DrawEncyclopediaDefaults()
{
#ifdef ENABLE_BRIEFINGROOM
  HVOBJECT hRustBackGroundHandle;

	// Blt the rust background
	if (!GetVideoObject(&hRustBackGroundHandle, guiRustEncyclopediaBackGround))
		return FALSE;
	BltVideoObject(FRAME_BUFFER, hRustBackGroundHandle, 0,RUSTBACKGROUND_1_X, RUSTBACKGROUND_1_Y, VO_BLT_SRCTRANSPARENCY,NULL);
#endif // ENABLE_BRIEFINGROOM
	return(TRUE);
}

static BOOLEAN LoadEncyclopediaDefaults(LaptopPageResourceOwner& owner)
{
#ifdef ENABLE_BRIEFINGROOM
  VOBJECT_DESC    VObjectDesc;

	// load the Rust bacground graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("BriefingRoom\\background.sti", VObjectDesc.ImageFile);
	CHECKF(owner.addVideoObject(
		&VObjectDesc, guiRustEncyclopediaBackGround));

#endif // ENABLE_BRIEFINGROOM
	return(TRUE);
}

BOOLEAN DisplayEncyclopediaLocationText()
{
	//DumpMemoryInfoIntoFile( "ExtremeMemoryDump.txt", FALSE );
	return(TRUE);
}

void ResetTemp()
{
#ifdef ENABLE_BRIEFINGROOM 

	UINT32 i,NUM_TEMP;
  
	NUM_TEMP = NUM_MISSION;
		
  	for(i=0; i<NUM_TEMP; i++)
	{
			gbriefingRoomDataTemp[i].uiIndex = i;		
			gbriefingRoomDataTemp[i].Name[ENCYCLOPEDIA_NAME_SIZE-1]  = '\0';	
			gbriefingRoomDataTemp[i].sCode[ENCYCLOPEDIA_DECRIPTION_SIZE-1] = '\0';	
			gbriefingRoomDataTemp[i].sImagePositionX[0] = 0;
			gbriefingRoomDataTemp[i].sImagePositionY[0] = 0;	
			gbriefingRoomDataTemp[i].Hidden = FALSE;
			gbriefingRoomDataTemp[i].MaxPages = 0;
			gbriefingRoomDataTemp[i].MaxImages = -1;
						
			//Mission
			gbriefingRoomDataTemp[i].MissionID = i;
			gbriefingRoomDataTemp[i].NextMission = -1;
			gbriefingRoomDataTemp[i].CheckMission = 0;			
	}
#endif // ENABLE_BRIEFINGROOM
}

void BackupBRandEncyclopedia ( BRIEFINGROOM_M_DATA *EncyBackup, BRIEFINGROOM_M_DATA *Ency, UINT32 NUM)
{
#ifdef ENABLE_BRIEFINGROOM
	if (!EncyBackup || !Ency) return;
	const UINT32 NUM_TEMP = std::min(NUM, static_cast<UINT32>(NUM_MISSION));

	for(UINT32 i = 0; i < NUM_TEMP; ++i)
	{
		Ency[i].uiIndex = EncyBackup[i].uiIndex;
		
		Ency[i].Hidden = EncyBackup[i].Hidden;

		Ency[i].bvisible = EncyBackup[i].bvisible;	

		Ency[i].MissionID = EncyBackup[i].MissionID;
		Ency[i].NextMission = EncyBackup[i].NextMission;
			
		Ency[i].CheckMission = EncyBackup[i].CheckMission;		
	}
	
#endif // ENABLE_BRIEFINGROOM
}

void CopyToTemp ( BRIEFINGROOM_M_DATA *Ency, BOOLEAN bFiltr, INT32 sort, INT32 TypFiltr, BOOLEAN ShowBox )
{
#ifdef ENABLE_BRIEFINGROOM
	UINT32 i, NUM_TEMP;
	
	BOOLEAN bBoxShow;
	ID = 0;
	IDimage = 0;
	MaxImages = 1;
	IDNewLocation = 0;
	LocationID = 0;
	MaxLocation = 0;
    bBoxShow = TRUE;
	ResetVal = FALSE;
	
	NUM_TEMP = NUM_MISSION;
		
  	for(i=0; i<NUM_TEMP; i++)
	{
	
		gbriefingRoomDataTemp[i].uiIndex = i;
		Ency[i].uiIndex = i;
			
			
		if ( bBriefingRoomSpecialMission  == TRUE )
		{
			
		}
		else if ( bBriefingRoom == TRUE )
		{
			
			//Mission ID
			gbriefingRoomDataTemp[MaxLocation].MissionID = Ency[i].MissionID;
			gbriefingRoomDataTemp[MaxLocation].NextMission = Ency[i].NextMission;
			gbriefingRoomDataTemp[MaxLocation].CheckMission = Ency[i].CheckMission;
		/*	
			if ( saveMissionData[i].CheckMission == MISSIONEND || saveMissionData[i].CheckMission == MISSIONNOSTARTED ) 
			{
				saveMissionData[i].CheckMission = MISSIONNOSTARTED;
				Ency[i].Hidden = TRUE;
			}
			else 
			{
				saveMissionData[i].CheckMission = MISSIONNOSTARTED;
				Ency[i].Hidden = FALSE; 
			}
		*/	
		
		}	
		
			
		if ( Ency[i].Hidden == TRUE && TypFiltr == NO_FILTR ) //&& Ency[i].Filtr != FALSE ) 
		{	

			bBoxShow = FALSE;	
					
			LaptopUiStateModel::CopyText(
				gbriefingRoomDataTemp[MaxLocation].Name, Ency[i].Name);
			
			gbriefingRoomDataTemp[MaxLocation].sImagePositionX[0] = Ency[i].sImagePositionX[0];
			gbriefingRoomDataTemp[MaxLocation].sImagePositionY[0] = Ency[i].sImagePositionY[0];
			
			
			gbriefingRoomDataTemp[MaxLocation].bvisible = Ency[i].bvisible;
			gbriefingRoomDataTemp[MaxLocation].Hidden = Ency[i].Hidden;	
				
			gbriefingRoomDataTemp[MaxLocation].MaxImages = Ency[i].MaxImages;
			
			gbriefingRoomDataTemp[MaxLocation].MaxPages = Ency[i].MaxPages;
			
			MaxLocation++;
			
		}
	}
	
	if ( bBoxShow == TRUE && ShowBox == TRUE ) 
	{
		ResetVal = TRUE;
		//DoLapTopMessageBox( MSG_BOX_LAPTOP_DEFAULT, pSectorPageText[ 4 ], LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);
	}
#endif // ENABLE_BRIEFINGROOM
}

static BOOLEAN LoadSoundButtons(LaptopPageResourceOwner& owner)
{
	INT32 pendingButtons[MAX_MISSION_BUTTONS] = {
		BUTTON_NO_SLOT, BUTTON_NO_SLOT};
	auto buttonImage = LoadButtonImageOwned("BriefingRoom\\BUTTONF.sti",
		BUTTON_NO_IMAGE, 0, BUTTON_NO_IMAGE, 1, BUTTON_NO_IMAGE);
	CHECKF(buttonImage);
	INT32 pendingImage = BUTTON_NO_IMAGE;
	CHECKF(owner.addButtonImage(std::move(buttonImage), pendingImage));

	UINT16 usPosY = ENCYCLOPEDIA_LOCATION_FILTER_Y;
	for (UINT16 i = 0; i < MAX_MISSION_BUTTONS; ++i)
	{
		const INT32 button = CreateIconAndTextButton( pendingImage,
			pOtherButtonsText[i], ENCYCLOPEDIA_LOCATION_PAGE_FONT,
												ENCYCLOPEDIA_LOCATION_PAGE_COLOR_UP, DEFAULT_SHADOW,
												ENCYCLOPEDIA_LOCATION_PAGE_COLOR_DOWN, DEFAULT_SHADOW,
												TEXT_CJUSTIFIED,
												ENCYCLOPEDIA_LOCATION_PAGE1_X-92, usPosY, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, SelectSoundButtonsRegionCallBack);
		CHECKF(owner.addButton(button, pendingButtons[i]));
		SetButtonCursor(pendingButtons[i], CURSOR_WWW);
		MSYS_SetBtnUserData(pendingButtons[i], 0, i);
		usPosY += ENCYCLOPEDIA_FILTR_BUTTON_GAP;
		SetButtonFastHelpText(pendingButtons[i], pOtherButtonsHelpText[i]);
	}

	guiSoundButtonsImage = pendingImage;
	std::copy(std::begin(pendingButtons), std::end(pendingButtons),
		std::begin(guiSoundButtons));
	return TRUE;
}

static void InitData(BOOLEAN bInit)
{
#ifdef ENABLE_BRIEFINGROOM

  IDimage = 0;
  MaxImages = 1;
  LocationID = 0;
  ID = 0;
  IDNewLocation = 0;
  MaxLocation = 0;
  ENCYCLOPEDIA_PAGE = 0;
  
  
  ResetTemp();
  
  
  if ( bBriefingRoom == TRUE )
	{
		#ifdef ENABLE_BRIEFINGROOM
		CopyToTemp (gBriefingRoomData, TRUE, NONE_FILTR , NO_FILTR, FALSE );
		#endif
	}
  else if ( bBriefingRoomSpecialMission  == TRUE )
	{
		#ifdef ENABLE_BRIEFINGROOM
		CopyToTemp (gBriefingRoomSpecialMissionData, TRUE, NONE_FILTR , NO_FILTR, FALSE );
		#endif
	}
	
    if ( bBriefingRoomSpecialMission  == TRUE || bBriefingRoom == TRUE ) bSoundButtons = TRUE; else bSoundButtons = FALSE;
	
	ResetVal = bInit;
#endif // ENABLE_BRIEFINGROOM
}

BOOLEAN EnterEncyclopediaLocation()
{
#ifdef ENABLE_BRIEFINGROOM
	CHAR8 str[MAX_ENCYCLOPEDIA_CHARS];
	LaptopPageResourceOwner stagedResources;

	InitData (ResetVal);
	if (bSoundButtons)
		CHECKF(LoadSoundButtons(stagedResources));

#ifdef ENCYCLOPEDIA_WORKS
	UINT16	usPosX, usPosY, i;
	CHECKF(LoadEncyclopediaDefaults(stagedResources));

	auto locationButtonImage = LoadButtonImageOwned(
		"ENCYCLOPEDIA\\BottomButtons2.sti", -1, 0, -1, 1, -1);
	CHECKF(locationButtonImage);
	CHECKF(stagedResources.addButtonImage(std::move(locationButtonImage),
		guiEncyclopediaLocationPageButtonImage));

	usPosX = ENCYCLOPEDIA_LOCATION_PAGE1_X+15;
	usPosY = ENCYCLOPEDIA_LOCATION_PAGE1_Y;

	for(i=0; i<3; i++)
	{
		const INT32 button = CreateIconAndTextButton( guiEncyclopediaLocationPageButtonImage, pSectorPageText[i], ENCYCLOPEDIA_LOCATION_PAGE_FONT,
														ENCYCLOPEDIA_LOCATION_PAGE_COLOR_UP, DEFAULT_SHADOW,
														ENCYCLOPEDIA_LOCATION_PAGE_COLOR_DOWN, DEFAULT_SHADOW,
														TEXT_CJUSTIFIED,
														usPosX, usPosY, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, SelectEncyclopediaLocationRegionCallBack);
		CHECKF(stagedResources.addButton(button,
			guiEncyclopediaLocationPageButton[i]));
		SetButtonCursor(guiEncyclopediaLocationPageButton[i], CURSOR_WWW);
		MSYS_SetBtnUserData( guiEncyclopediaLocationPageButton[i], 0, i);
		usPosX += ENCYCLOPEDIA_LOCATION_PAGE_GAP;
	}
	
	//Load graphic for buttons
	auto pageButtonImage = LoadButtonImageOwned(
		"ENCYCLOPEDIA\\BottomButtons2.sti", -1, 0, -1, 1, -1);
	CHECKF(pageButtonImage);
	CHECKF(stagedResources.addButtonImage(std::move(pageButtonImage),
		guiEncyclopediaiPageButtonImage));

	usPosX = ENCYCLOPEDIA_LOCATION_PAGE1_X-44;
	usPosY = ENCYCLOPEDIA_LOCATION_PAGE1_Y-130;

	for(i=0; i<3; i++)
	{
		const INT32 button = CreateIconAndTextButton( guiEncyclopediaiPageButtonImage, pLocationPageText[i], ENCYCLOPEDIA_LOCATION_PAGE_FONT,
														ENCYCLOPEDIA_LOCATION_PAGE_COLOR_UP, DEFAULT_SHADOW,
														ENCYCLOPEDIA_LOCATION_PAGE_COLOR_DOWN, DEFAULT_SHADOW,
														TEXT_CJUSTIFIED,
														usPosX, usPosY, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, SelectEncyclopediaLocationPageRegionCallBack);
		CHECKF(stagedResources.addButton(button,
			guiEncyclopediaPageButton[i]));
		SetButtonCursor(guiEncyclopediaPageButton[i], CURSOR_WWW);
		MSYS_SetBtnUserData( guiEncyclopediaPageButton[i], 0, i);

		usPosX += ENCYCLOPEDIA_LOCATION_PAGE_GAP+60;
	}
#endif
	gBriefingDataResources = std::move(stagedResources);

	fFirstTimeInEncyclopediaLocation = FALSE;

	RenderEncyclopediaLocation(FALSE);
	
	RenderButtonDisabled();
	
#ifdef ENCYCLOPEDIA_WORKS
	DisableButton( guiEncyclopediaPageButton[0] );	
	DisableButton( guiEncyclopediaLocationPageButton[0] );	
#endif

	if (const BRIEFINGROOM_M_DATA* entry = CurrentBriefingEntry())
	{
		if (entry->CheckMission == MISSIONSTART ||
			entry->CheckMission == MISSIONEND)
			DisableButton(guiSoundButtons[1]);
		else if (entry->CheckMission == MISSIONNOSTARTED)
			EnableButton(guiSoundButtons[1]);
	
#ifdef ENCYCLOPEDIA_WORKS
	if ( MaxLocation == -1 || MaxLocation == 0 || MaxLocation == 1 ) 
		DisableButton( guiEncyclopediaLocationPageButton[2] );
#endif

		std::snprintf(str, std::size(str), "BriefingRoom\\mission%d.wav",
			entry->MissionID);
		if( FileExists(str)  )
			EnableButton ( guiSoundButtons[0] );
		else
			DisableButton( guiSoundButtons[0] );
	}
	else if (bSoundButtons)
	{
		DisableButton(guiSoundButtons[0]);
		DisableButton(guiSoundButtons[1]);
	}

	RenderMap();

#endif // ENABLE_BRIEFINGROOM 

	return(TRUE);
}

void ExitEncyclopediaLocation()
{
	if (uiSoundSampleBR != NO_SAMPLE)
	{
		SoundStop(uiSoundSampleBR);
		uiSoundSampleBR = NO_SAMPLE;
	}
	gBriefingDataResources.clear();
	guiSoundButtonsImage = BUTTON_NO_IMAGE;
	std::fill(std::begin(guiSoundButtons), std::end(guiSoundButtons),
		BUTTON_NO_SLOT);
	

	bSoundButtons = FALSE;
		
	bBriefingRoom  = FALSE;
	bBriefingRoomSpecialMission = FALSE;
	
}

void HandleEncyclopediaLocation()
{

}

void RenderButtonDisabled()
{
#ifdef ENABLE_BRIEFINGROOM
#ifdef ENCYCLOPEDIA_WORKS
	if ( bPage2 == FALSE && bPage3 == FALSE ) 
	{
		DisableButton( guiEncyclopediaPageButton[0] );
		DisableButton( guiEncyclopediaPageButton[2] );
	}
	else
	{
		EnableButton( guiEncyclopediaPageButton[2] );
	}
#endif
#endif // ENABLE_BRIEFINGROOM
}

void RenderButtonDisabled2()
{
#ifdef ENABLE_BRIEFINGROOM
#ifdef ENCYCLOPEDIA_WORKS
	DisableButton( guiEncyclopediaPageButton[0] );
	EnableButton( guiEncyclopediaPageButton[2] );
#endif
#endif // ENABLE_BRIEFINGROOM
}

void RenderButtonDisabled3()
{
#ifdef ENABLE_BRIEFINGROOM
#ifdef ENCYCLOPEDIA_WORKS
	DisableButton( guiEncyclopediaLocationPageButton[0] );
	
	if ( MaxLocation == -1 || MaxLocation == 0 || MaxLocation == 1 ) 
		DisableButton( guiEncyclopediaLocationPageButton[2] );
	else
		EnableButton( guiEncyclopediaLocationPageButton[2] );
#endif
#endif // ENABLE_BRIEFINGROOM
}

void RenderButtonDisabled4()
{
#ifdef ENABLE_BRIEFINGROOM
#ifdef ENCYCLOPEDIA_WORKS
	DisableButton( guiEncyclopediaPageButton[0] );
	DisableButton( guiEncyclopediaPageButton[2] );
#endif
#endif // ENABLE_BRIEFINGROOM
}

void RenderBoxDisabledButton()
{
#ifdef ENABLE_BRIEFINGROOM
	MaxImages = 1;
	MaxLocation = 0;
	IDimage = 0;
	
#ifdef ENCYCLOPEDIA_WORKS
	DisableButton( guiEncyclopediaLocationPageButton[0] );
	//DisableButton( guiEncyclopediaLocationPageButton[1] );
	DisableButton( guiEncyclopediaLocationPageButton[2] );
		
	DisableButton( guiEncyclopediaPageButton[0] );	
	DisableButton( guiEncyclopediaPageButton[1] );
	DisableButton( guiEncyclopediaPageButton[2] );
#endif
#endif // ENABLE_BRIEFINGROOM
}

static BOOLEAN RenderMap()
{
#ifdef ENABLE_BRIEFINGROOM
	BRIEFINGROOM_M_DATA* entry = CurrentBriefingEntry();
	if (!entry || (!bBriefingRoom && !bBriefingRoomSpecialMission) ||
		entry->MaxImages < 0)
		return TRUE;

	const INT32 lastImage = std::min(MaxImages, entry->MaxImages);
	if (IDimage < 0 || IDimage > lastImage) IDimage = 0;

	CHAR8 imagePath[MAX_ENCYCLOPEDIA_CHARS] = {};
	std::snprintf(imagePath, std::size(imagePath),
		"BriefingRoom\\mission%d_%d.STI", entry->MissionID, IDimage);
	if (!FileExists(imagePath))
	{
		IDimage = -1;
		return TRUE;
	}

	VOBJECT_DESC description{};
	description.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	LaptopUiStateModel::CopyText(description.ImageFile, imagePath);
	UniqueVideoObjectHandle graphic = AddVideoObjectOwned(&description);
	CHECKF(graphic);
	CHECKF(BltVideoObjectFromIndex(FRAME_BUFFER, graphic.get(), 0,
		ENCYCLOPEDIA_IMAGE_X + entry->sImagePositionX[0],
		ENCYCLOPEDIA_IMAGE_Y + entry->sImagePositionY[0],
		VO_BLT_SRCTRANSPARENCY, NULL));
#endif // ENABLE_BRIEFINGROOM
	return(TRUE);
}

void RenderEncyclopediaLocation( BOOLEAN bHidden )
{
#ifdef ENABLE_BRIEFINGROOM
  UINT16	i;
  UINT8		ubNumLines=11; 
  UINT16	usPosY;
  //UINT8			ubFontHeight;
  CHAR16	zString[512]; 
  CHAR16	zString2[512];
  
 CHAR16 P1[1120],P2[1120];
	CHAR8 fileNameEdt[80];
	BRIEFINGROOM_M_DATA* entry = CurrentBriefingEntry();
			
  P1[0] = 0;
  P2[0] = 0;


	DrawEncyclopediaDefaults();

	DisplayEncyclopediaLocationText();
	if (!entry)
	{
		InvalidateRegion(LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y,
			LAPTOP_SCREEN_LR_X, LAPTOP_SCREEN_WEB_LR_Y);
		return;
	}
	
	//ubFontHeight = (UINT8)GetFontHeight(ENCYCLOPEDIA_LOCATION_BOX_FONT);
	
	bPage2 = TRUE;
	bPage3 = TRUE;

	ENCYCLOPEDIA_PAGE = entry->MaxPages > 0 ?
		static_cast<INT32>(entry->MaxPages - 1) : 0;
	ID = std::max(0, std::min(ID, ENCYCLOPEDIA_PAGE));
	std::snprintf(fileNameEdt, std::size(fileNameEdt),
		"BriefingRoom\\EDT\\mission%d.edt", entry->MissionID);
	LoadEncryptedDataFromFile(fileNameEdt, P1, 1120*2*ID, 1120);

	MaxImages = entry->MaxImages;
	

	if ((bBriefingRoom || bBriefingRoomSpecialMission) && !ResetVal)
	{
		LaptopUiStateModel::CopyText(zString, entry->Name);
		LaptopUiStateModel::CopyText(zString2, zString);
	}
	else
	{
		zString[0] = L'\0';
		zString2[0] = L'\0';
	}
	
	ShadowVideoSurfaceRect( FRAME_BUFFER, ENCYCLOPEDIA_LOCATION_BOX_X+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP, ENCYCLOPEDIA_LOCATION_BOX_Y + 13 + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP - 227 , ENCYCLOPEDIA_LOCATION_BOX_X + ENCYCLOPEDIA_LOCATION_BOX_WIDTH + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP, ENCYCLOPEDIA_LOCATION_BOX_Y + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP-42);

	ShadowVideoSurfaceRect( FRAME_BUFFER, ENCYCLOPEDIA_LOCATION_BOX_X+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP, ENCYCLOPEDIA_LOCATION_BOX_Y + 13 + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP - 50 , ENCYCLOPEDIA_LOCATION_BOX_X + ENCYCLOPEDIA_LOCATION_BOX_WIDTH + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP, ENCYCLOPEDIA_LOCATION_BOX_Y + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP-20);
	
	//Reset
    //if ( ResetVal == TRUE )
	//DisplayWrappedString(ENCYCLOPEDIA_LOCATION_BOX_X, ENCYCLOPEDIA_LOCATION_BOX_Y-225, ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH, 6, FONT14ARIAL, ENCYCLOPEDIA_LOCATION_BOX_COLOR, pSectorPageText[ 4 ], FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	
	//Reset
	if ( ResetVal == TRUE )
	DisplayWrappedString(ENCYCLOPEDIA_LOCATION_BOX_X+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP + 3, ENCYCLOPEDIA_LOCATION_BOX_Y + 18 + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP - 65, ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH, 6, ENCYCLOPEDIA_LOCATION_BOX_FONT, ENCYCLOPEDIA_LOCATION_BOX_COLOR, L"", FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	
	if ( /* gbriefingRoomDataTemp[LocationID].SpecialQuestImage == FALSE && */ ResetVal == FALSE)
	{
	
	if ( IDimage == -1 || MaxImages == -1 ) IDimage = 0;	
	
	std::snprintf(fileNameEdt, std::size(fileNameEdt),
		"BriefingRoom\\EDT\\description%d.edt", entry->MissionID);
	if ( FileExists(fileNameEdt)  )
		LoadEncryptedDataFromFile(fileNameEdt, P2, 1120*2*IDimage, 1120);
	
	if ( FileExists(fileNameEdt)  )
		DisplayWrappedString(ENCYCLOPEDIA_LOCATION_BOX_X+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP + 3, ENCYCLOPEDIA_LOCATION_BOX_Y + 18 + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP - 50, ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH, 6, ENCYCLOPEDIA_LOCATION_BOX_FONT, ENCYCLOPEDIA_LOCATION_BOX_COLOR, P2, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	else
		DisplayWrappedString(ENCYCLOPEDIA_LOCATION_BOX_X+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP + 3, ENCYCLOPEDIA_LOCATION_BOX_Y + 18 + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP - 50, ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH, 6, ENCYCLOPEDIA_LOCATION_BOX_FONT, ENCYCLOPEDIA_LOCATION_BOX_COLOR, L"", FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	
/*	
	if ( IDimage == 1 )
		DisplayWrappedString(ENCYCLOPEDIA_LOCATION_BOX_X+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP + 3, ENCYCLOPEDIA_LOCATION_BOX_Y + 18 + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP - 50, ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH, 6, ENCYCLOPEDIA_LOCATION_BOX_FONT, ENCYCLOPEDIA_LOCATION_BOX_COLOR, gbriefingRoomDataTemp[LocationID].sImageDesc1, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	else if (IDimage == 2 && gbriefingRoomDataTemp[LocationID].sImageDesc2 != '\0' )
		DisplayWrappedString(ENCYCLOPEDIA_LOCATION_BOX_X+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP + 3, ENCYCLOPEDIA_LOCATION_BOX_Y + 18 + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP - 50, ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH, 6, ENCYCLOPEDIA_LOCATION_BOX_FONT, ENCYCLOPEDIA_LOCATION_BOX_COLOR, gbriefingRoomDataTemp[LocationID].sImageDesc2, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	else if (IDimage == 3 && gbriefingRoomDataTemp[LocationID].sImageDesc3 != '\0' )
		DisplayWrappedString(ENCYCLOPEDIA_LOCATION_BOX_X+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP + 3, ENCYCLOPEDIA_LOCATION_BOX_Y + 18 + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP - 50, ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH, 6, ENCYCLOPEDIA_LOCATION_BOX_FONT, ENCYCLOPEDIA_LOCATION_BOX_COLOR, gbriefingRoomDataTemp[LocationID].sImageDesc3, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	else if (IDimage == 4 && gbriefingRoomDataTemp[LocationID].sImageDesc4 != '\0' )
		DisplayWrappedString(ENCYCLOPEDIA_LOCATION_BOX_X+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP + 3, ENCYCLOPEDIA_LOCATION_BOX_Y + 18 + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP - 50, ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH, 6, ENCYCLOPEDIA_LOCATION_BOX_FONT, ENCYCLOPEDIA_LOCATION_BOX_COLOR, gbriefingRoomDataTemp[LocationID].sImageDesc4, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
*/
	}
	
	//Reset
	if ( ResetVal == TRUE )
	DisplayWrappedString(ENCYCLOPEDIA_LOCATION_BOX_X+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP + 3, ENCYCLOPEDIA_LOCATION_BOX_Y + 18 + ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP - 50, ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH, 6, ENCYCLOPEDIA_LOCATION_BOX_FONT, ENCYCLOPEDIA_LOCATION_BOX_COLOR, L"", FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	
	/*
	if ( ID == 0 )
		usStringPixLength = StringPixLength( P1, ENCYCLOPEDIA_LOCATION_BOX_FONT);
	else if (ID == 1 ) // && gbriefingRoomDataTemp[LocationID].sDesc2 != '\0' )
		usStringPixLength = StringPixLength( P2, ENCYCLOPEDIA_LOCATION_BOX_FONT);
	else if (ID == 2 ) // && gbriefingRoomDataTemp[LocationID].sDesc3 != '\0' )
		usStringPixLength = StringPixLength( P3, ENCYCLOPEDIA_LOCATION_BOX_FONT);
	else if (ID == 3 ) // && gbriefingRoomDataTemp[LocationID].sDesc4 != '\0' )
	*/
	//Inventory name
	if (bBriefingRoom == TRUE || ResetVal == FALSE ) 
	DisplayWrappedString(ENCYCLOPEDIA_LOCATION_BOX_X, ENCYCLOPEDIA_LOCATION_BOX_Y-225, ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH, 6, FONT14ARIAL, ENCYCLOPEDIA_LOCATION_BOX_COLOR, zString2, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	usPosY = ENCYCLOPEDIA_LOCATION_BOX_Y + 13;
	
	//draw top line of the popup background
	ShadowVideoSurfaceRect( FRAME_BUFFER, ENCYCLOPEDIA_LOCATION_BOX_X+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP, usPosY+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP, ENCYCLOPEDIA_LOCATION_BOX_X + ENCYCLOPEDIA_LOCATION_BOX_WIDTH+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP, usPosY + ENCYCLOPEDIA_LOCATION_BOX_SECTION_HEIGHT+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP-1);

	//draw mid section of the popup background
	usPosY += ENCYCLOPEDIA_LOCATION_BOX_SECTION_HEIGHT;
	for(i=0; i< 10; i++)
	{
		ShadowVideoSurfaceRect( FRAME_BUFFER, ENCYCLOPEDIA_LOCATION_BOX_X+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP, usPosY+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP, ENCYCLOPEDIA_LOCATION_BOX_X + ENCYCLOPEDIA_LOCATION_BOX_WIDTH+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP, usPosY + ENCYCLOPEDIA_LOCATION_BOX_SECTION_HEIGHT+ENCYCLOPEDIA_LOCATION_BOX_SHADOW_GAP-1);
		usPosY += ENCYCLOPEDIA_LOCATION_BOX_SECTION_HEIGHT;
	}
		
	if ( ResetVal == FALSE ) 
		DisplayWrappedString(ENCYCLOPEDIA_LOCATION_BOX_DESC_X, ENCYCLOPEDIA_LOCATION_BOX_DESC_Y + 13, ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH, 6, ENCYCLOPEDIA_LOCATION_BOX_FONT, ENCYCLOPEDIA_LOCATION_BOX_COLOR, P1, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	//Reset
	if ( ResetVal == TRUE )
	   DisplayWrappedString(ENCYCLOPEDIA_LOCATION_BOX_DESC_X, ENCYCLOPEDIA_LOCATION_BOX_DESC_Y + 13, ENCYCLOPEDIA_LOCATION_BOX_TEXT_WIDTH, 6, ENCYCLOPEDIA_LOCATION_BOX_FONT, ENCYCLOPEDIA_LOCATION_BOX_COLOR, L"", FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
		
	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	
	if ( ENCYCLOPEDIA_PAGE == -1 || ENCYCLOPEDIA_PAGE == 0 )
		RenderButtonDisabled4();
		
#ifdef ENCYCLOPEDIA_WORKS
	if ( bHidden == TRUE && ( MaxImages == 0 || MaxImages == -1 || MaxImages == 1 ) ) //|| gbriefingRoomDataTemp[0].MaxImages == 1 ) )
		DisableButton( guiEncyclopediaPageButton[1] );
	else if ( bHidden == FALSE && MaxImages > 1 )
		EnableButton( guiEncyclopediaPageButton[1] );
		
	//Reset
	if ( ResetVal == TRUE )	
		DisableButton( guiEncyclopediaPageButton[1] );
#endif

	if (entry->CheckMission == MISSIONSTART ||
		entry->CheckMission == MISSIONEND)
		DisableButton ( guiSoundButtons[1] );
	else if (entry->CheckMission == MISSIONNOSTARTED)
		EnableButton ( guiSoundButtons[1] );
	
	//if ( ResetVal == TRUE ) 
	//DoLapTopMessageBox( MSG_BOX_LAPTOP_DEFAULT, pSectorPageText[ 4 ], LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);

  InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
#endif // ENABLE_BRIEFINGROOM  
}

void SelectEncyclopediaLocationPageRegionCallBack(GUI_BUTTON * btn, INT32 reason )
{
#ifdef ENABLE_BRIEFINGROOM
	UINT8	ubRetValue = (UINT8)MSYS_GetBtnUserData( btn, 0 );
	BRIEFINGROOM_M_DATA* entry = CurrentBriefingEntry();
	if (!entry) return;
	
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			
			if ( ubRetValue == 2 )
			{
				const auto nextPage = LaptopUiStateModel::AdjacentIndex(
					entry->MaxPages, ID, true);
				if (!nextPage) return;
				ID = static_cast<INT32>(*nextPage);
				
#ifdef ENCYCLOPEDIA_WORKS
				if ( ID == ENCYCLOPEDIA_PAGE || ID > ENCYCLOPEDIA_PAGE ) DisableButton( guiEncyclopediaPageButton[2] );
				if ( ID > 0 ) EnableButton( guiEncyclopediaPageButton[0] );
#endif

				RenderEncyclopediaLocation(FALSE);
				RenderMap();
			}
			else if ( ubRetValue == 0 )
			{
				const auto previousPage = LaptopUiStateModel::AdjacentIndex(
					entry->MaxPages, ID, false);
				if (!previousPage) return;
				ID = static_cast<INT32>(*previousPage);

#ifdef ENCYCLOPEDIA_WORKS
				if ( ID == 0 ) DisableButton( guiEncyclopediaPageButton[0] );
				if ( ID < ENCYCLOPEDIA_PAGE ) EnableButton( guiEncyclopediaPageButton[2] );
#endif
				RenderEncyclopediaLocation(FALSE);
				RenderMap();
			}
			else if ( ubRetValue == 1 )
			{
				const std::size_t imageCount = MaxImages >= 0 ?
					static_cast<std::size_t>(MaxImages) + 1 : 0;
				const auto nextImage = LaptopUiStateModel::AdjacentIndex(
					imageCount, IDimage, true);
				IDimage = nextImage ? static_cast<INT32>(*nextImage) : 0;
	
				RenderEncyclopediaLocation(FALSE);
				RenderMap();
			}
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
	}
#endif // ENABLE_BRIEFINGROOM
}

void SelectEncyclopediaLocationRegionCallBack(GUI_BUTTON * btn, INT32 reason )
{
#ifdef ENABLE_BRIEFINGROOM
	UINT8	ubRetValue = (UINT8)MSYS_GetBtnUserData( btn, 0 );
	
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			if ( ubRetValue == 2 )
			{
				const auto nextLocation = LaptopUiStateModel::AdjacentIndex(
					static_cast<std::size_t>(std::max(0, MaxLocation)),
					static_cast<INT32>(LocationID), true);
				if (!nextLocation) return;
				IDNewLocation = static_cast<INT32>(*nextLocation);
				ID = 0;

				LocationID = *nextLocation;
				
#ifdef ENCYCLOPEDIA_WORKS
				if ( IDNewLocation == MaxLocation - 1 || IDNewLocation == -1 ) DisableButton( guiEncyclopediaLocationPageButton[2] );
				if ( IDNewLocation > 0 ) EnableButton( guiEncyclopediaLocationPageButton[0] );
#endif

				IDimage = 0;
				
				RenderEncyclopediaLocation(FALSE);
				RenderButtonDisabled2();
				
					if ( ENCYCLOPEDIA_PAGE == -1 || ENCYCLOPEDIA_PAGE == 0 )
						RenderButtonDisabled4();
						
#ifdef ENCYCLOPEDIA_WORKS
					if ( MaxImages == -1 || MaxImages == 0 || MaxImages == 1 )
						DisableButton( guiEncyclopediaPageButton[1] );
					else
						EnableButton( guiEncyclopediaPageButton[1] );
#endif

					if( uiSoundSampleBR!=NO_SAMPLE )
					{
						SoundStop( uiSoundSampleBR );
					}	
						
				RenderMap();

			}
			else if ( ubRetValue == 0 )
			{
				const auto previousLocation = LaptopUiStateModel::AdjacentIndex(
					static_cast<std::size_t>(std::max(0, MaxLocation)),
					static_cast<INT32>(LocationID), false);
				if (!previousLocation) return;
				ID = 0;

				IDNewLocation = static_cast<INT32>(*previousLocation);

				LocationID = *previousLocation;
				
#ifdef ENCYCLOPEDIA_WORKS
				if ( IDNewLocation == 0 || IDNewLocation == -1 ) DisableButton( guiEncyclopediaLocationPageButton[0] );
				if ( IDNewLocation < MaxLocation-1) EnableButton( guiEncyclopediaLocationPageButton[2] );
#endif

				IDimage = 0;
				
				RenderEncyclopediaLocation(FALSE);
				RenderButtonDisabled2();
				
				if ( ENCYCLOPEDIA_PAGE == -1 || ENCYCLOPEDIA_PAGE == 0 )
					RenderButtonDisabled4();
				
#ifdef ENCYCLOPEDIA_WORKS
				if ( MaxImages == -1 || MaxImages == 0 || MaxImages == 1 )
					DisableButton( guiEncyclopediaPageButton[1] );
				else
					EnableButton( guiEncyclopediaPageButton[1] );
#endif

				if( uiSoundSampleBR!=NO_SAMPLE )
				{
					SoundStop( uiSoundSampleBR );
				}	
						
				RenderMap();

			}
			else if ( ubRetValue == 1 )
			{
				//ExitEncyclopediaLocation();
				
				if( uiSoundSampleBR!=NO_SAMPLE )
				{
					SoundStop( uiSoundSampleBR );
				}	
				
				if ( bBriefingRoom == TRUE || bBriefingRoomSpecialMission == TRUE )
				{
					if(!fFirstTimeInEncyclopediaLocation) guiCurrentLaptopMode = LAPTOP_MODE_BRIEFING_ROOM_ENTER;
				}
#ifdef ENCYCLOPEDIA_WORKS
				else
				{
					if(!fFirstTimeInEncyclopediaLocation) guiCurrentLaptopMode = LAPTOP_MODE_ENCYCLOPEDIA;
				}
#endif
			}
		}
	}

	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
	}
#endif // ENABLE_BRIEFINGROOM
}

void SelectSoundButtonsRegionCallBack(GUI_BUTTON * btn, INT32 reason )
{
#ifdef ENABLE_BRIEFINGROOM
	UINT8	ubRetValue = (UINT8)MSYS_GetBtnUserData( btn, 0 );
	CHAR8 str[MAX_ENCYCLOPEDIA_CHARS];
	BRIEFINGROOM_M_DATA* entry = CurrentBriefingEntry();
	if (!entry) return;
	
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			
		
		   if ( bBriefingRoom == TRUE )
			{
			
				if ( ubRetValue == 0 )
				{
				
					if( uiSoundSampleBR!=NO_SAMPLE )
					{
						SoundStop( uiSoundSampleBR );
					}			
					
					std::snprintf(str, std::size(str),
						"BriefingRoom\\mission%d.wav", entry->MissionID);
					uiSoundSampleBR = PlayJA2SampleFromFile( str, RATE_11025, HIGHVOLUME, 1, MIDDLE );
					
				}
				
				if ( ubRetValue == 1 )
				{
					if (LaptopUiStateModel::IsValidIndex(
						NUM_MISSION, entry->MissionID) &&
						gBriefingRoomData[entry->MissionID].CheckMission ==
							MISSIONNOSTARTED)
					{

						entry->CheckMission = MISSIONSTART;
						gBriefingRoomData[entry->MissionID].CheckMission =
							MISSIONSTART;
						
						DisableButton ( guiSoundButtons[1] );
					}
				}
				
			}
			else if ( bBriefingRoomSpecialMission == TRUE )
			{
			
				if ( ubRetValue == 0 )
				{
					if( uiSoundSampleBR!=NO_SAMPLE )
					{
						SoundStop( uiSoundSampleBR );
					}			
					
					std::snprintf(str, std::size(str),
						"BriefingRoom\\mission%d.wav", entry->MissionID);
					uiSoundSampleBR = PlayJA2SampleFromFile( str, RATE_11025, HIGHVOLUME, 1, MIDDLE );
			
				}
			
			}
				
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

	}
#endif // ENABLE_BRIEFINGROOM
}

//--------------------------------------------------------------


BOOLEAN SaveBriefingRoomToSaveGameFile( HWFILE hFile )
{
#ifdef ENABLE_BRIEFINGROOM
	UINT32 i;
#endif
	

#ifdef ENABLE_BRIEFINGROOM 
	//Briefing room
	for(i=0; i<NUM_MISSION; i++)
	{
		saveBriefingRoomData[i].Hidden = gBriefingRoomData[ i ].Hidden;
		saveBriefingRoomData[i].CheckMission = gBriefingRoomData[ i ].CheckMission;
	}

	if (!WriteLaptopFileExact(hFile, &saveBriefingRoomData,
		sizeof(saveBriefingRoomData)))
	{
		return( FALSE );
	}
	
	//Briefing room , special mission 
	for(i=0; i<NUM_MISSION; i++)
	{
		saveBriefingRoomSpecialMissionData[i].Hidden = gBriefingRoomSpecialMissionData[ i ].Hidden;
		saveBriefingRoomSpecialMissionData[i].CheckMission = gBriefingRoomSpecialMissionData[ i ].CheckMission;
	}

	if (!WriteLaptopFileExact(hFile, &saveBriefingRoomSpecialMissionData,
		sizeof(saveBriefingRoomSpecialMissionData)))
	{
		return( FALSE );
	}
#endif // ENABLE_BRIEFINGROOM
/*
	//ReservedData5
	for(i=0; i<MAX_QUESTS; i++)
	{
		saveBriefingRoomReservedData5[i] = gLoadBriefingRoomReservedData5[ i ].Hidden;
	}

	FileWrite( hFile, &saveBriefingRoomReservedData5, sizeof( saveBriefingRoomReservedData5), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( saveBriefingRoomReservedData5 ) )
	{
		return( FALSE );
	}

	//ReservedData1
	for(i=0; i<NUM_SECTOR; i++)
	{
		saveBriefingRoomReservedData1[i] = gLoadBriefingRoomReservedData1[ i ].Hidden;
	}

	FileWrite( hFile, &saveBriefingRoomReservedData1, sizeof( saveBriefingRoomReservedData1), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( saveBriefingRoomReservedData1 ) )
	{
		return( FALSE );
	}
	
	//ReservedData2
	for(i=0; i<NUM_PROFILES; i++)
	{
		saveBriefingRoomReservedData2[i].Hidden = gLoadBriefingRoomReservedData2[ i ].Hidden;
		saveBriefingRoomReservedData2[i].bvisible = gLoadBriefingRoomReservedData2[ i ].bvisible;
	}

	FileWrite( hFile, &saveBriefingRoomReservedData2, sizeof( saveBriefingRoomReservedData2), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( saveBriefingRoomReservedData2 ) )
	{
		return( FALSE );
	}
	
	//ReservedData3
	for(i=0; i<MAXITEMS; i++)
	{
		saveBriefingRoomReservedData3[i] = gLoadBriefingRoomReservedData3[ i ].Hidden;
	}

	FileWrite( hFile, &saveBriefingRoomReservedData3, sizeof( saveBriefingRoomReservedData3), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( saveBriefingRoomReservedData3 ) )
	{
		return( FALSE );
	}

	//ReservedData4
	for(i=0; i<NUM_PROFILES; i++)
	{
		saveBriefingRoomReservedData4[i] = gLoadBriefingRoomReservedData4[ i ].Hidden;
	}

	FileWrite( hFile, &saveBriefingRoomReservedData4, sizeof( saveBriefingRoomReservedData4), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( saveBriefingRoomReservedData4 ) )
	{
		return( FALSE );
	}
*/
	
	return( TRUE );
}

BOOLEAN LoadBriefingRoomFromLoadGameFile( HWFILE hFile )
{
#ifdef ENABLE_BRIEFINGROOM
	UINT32	uiNumBytesRead;
	UINT32 i;
	decltype(saveBriefingRoomData) pendingData{};
	decltype(saveBriefingRoomSpecialMissionData) pendingSpecialData{};

	// Flugente: compatibility fix
	BOOLEAN briefingroomreadingdone = FALSE;

	//Briefing room
	uiNumBytesRead = 0;
	const BOOLEAN readResult = FileRead(hFile, &pendingData,
		sizeof(pendingData), &uiNumBytesRead);
	if (!readResult || uiNumBytesRead != sizeof(pendingData))
	{
		// Flugente: one can start a game without this feature and later switch to an exe where it is enabled. At that point, loading the savgame would not be possible - 
		// the game expects saveBriefingRoomData but doesn't get any. Pretty odd, considering that the data has ALREADY BEEN READ into gBriefingRoomData at this point. Sigh.
		// For this reason, we don't return with an error here an simply use what the xml gave us here.
		// Of course, I'm not familiar with this feature. It would be good if someone who knows this fixes this.
		if ( uiNumBytesRead == 0 )
			briefingroomreadingdone = TRUE;
		else
			return( FALSE );
	}

	if ( !briefingroomreadingdone )
	{
		//Briefing room , special mission
		if (!ReadLaptopFileExact(hFile, &pendingSpecialData,
			sizeof(pendingSpecialData)))
		{
			return( FALSE );
		}

		memcpy(saveBriefingRoomData, pendingData, sizeof(pendingData));
		memcpy(saveBriefingRoomSpecialMissionData, pendingSpecialData,
			sizeof(pendingSpecialData));
		for(i=0; i<NUM_MISSION; ++i)
		{
			gBriefingRoomData[i].Hidden = pendingData[i].Hidden;
			gBriefingRoomData[i].CheckMission = pendingData[i].CheckMission;
			gBriefingRoomSpecialMissionData[ i ].Hidden = saveBriefingRoomSpecialMissionData[i].Hidden;
			gBriefingRoomSpecialMissionData[ i ].CheckMission = saveBriefingRoomSpecialMissionData[i].CheckMission;
		}
	}
#endif // ENABLE_BRIEFINGROOM

/*
	//ReservedData5
	FileRead( hFile, &saveBriefingRoomReservedData5, sizeof( saveBriefingRoomReservedData5), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( saveBriefingRoomReservedData5 ) )
	{
		return( FALSE );
	}	
	
	for(i=0; i<MAX_QUESTS; i++)
	{
		gLoadBriefingRoomReservedData5[ i ].Hidden = FALSE;
	}

	//ReservedData1
	FileRead( hFile, &saveBriefingRoomReservedData1, sizeof( saveBriefingRoomReservedData1), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( saveBriefingRoomReservedData1 ) )
	{
		return( FALSE );
	}
	
	for(i=0; i<NUM_SECTOR; i++)
	{
		gLoadBriefingRoomReservedData1[ i ].Hidden = saveBriefingRoomReservedData1[i];
	}
	
	//ReservedData2
	FileRead( hFile, &saveBriefingRoomReservedData2, sizeof( saveBriefingRoomReservedData2), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( saveBriefingRoomReservedData2 ) )
	{
		return( FALSE );
	}
	
	for(i=0; i<NUM_PROFILES; i++)
	{
		gLoadBriefingRoomReservedData2[ i ].Hidden = saveBriefingRoomReservedData2[i].Hidden;
		gLoadBriefingRoomReservedData2[ i ].bvisible = saveBriefingRoomReservedData2[i].bvisible;
	}	
	
	//ReservedData3
	FileRead( hFile, &saveBriefingRoomReservedData3, sizeof( saveBriefingRoomReservedData3), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( saveBriefingRoomReservedData3 ) )
	{
		return( FALSE );
	}
	
	for(i=0; i<MAXITEMS; i++)
	{
		gLoadBriefingRoomReservedData3[ i ].Hidden = saveBriefingRoomReservedData3[i];
	}
	
	//ReservedData4
	FileRead( hFile, &saveBriefingRoomReservedData4, sizeof( saveBriefingRoomReservedData4), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( saveBriefingRoomReservedData4 ) )
	{
		return( FALSE );
	}
	
	for(i=0; i<NUM_PROFILES; i++)
	{
		gLoadBriefingRoomReservedData4[ i ].Hidden = saveBriefingRoomReservedData4[i];
	}
	*/
	return( TRUE );
}
