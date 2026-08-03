
	#include "laptop.h"
	#include "AimArchives.h"
	#include "aim.h"
	#include "WordWrap.h"
	#include "Utilities.h"
	#include "WCheck.h"
	#include "Encrypted File.h"
	#include "Text.h"
	#include "LaptopPageResourceOwner.h"
	#include "LaptopSafety.h"

#include "Soldier Profile.h"


#define		AIM_ALUMNI_NAME_FILE		"BINARYDATA\\AlumName.edt"
#define		AIM_ALUMNI_FILE					"BINARYDATA\\Alumni.edt"


#define		AIM_ALUMNI_TITLE_FONT					FONT14ARIAL
#define		AIM_ALUMNI_TITLE_COLOR				AIM_GREEN

#define		AIM_ALUMNI_POPUP_FONT					FONT10ARIAL
#define		AIM_ALUMNI_POPUP_COLOR				FONT_MCOLOR_WHITE

#define		AIM_ALUMNI_POPUP_NAME_FONT		FONT12ARIAL
#define		AIM_ALUMNI_POPUP_NAME_COLOR		FONT_MCOLOR_WHITE

#define		AIM_ALUMNI_NAME_FONT					FONT12ARIAL
#define		AIM_ALUMNI_NAME_COLOR					FONT_MCOLOR_WHITE
#define		AIM_ALUMNI_PAGE_FONT					FONT14ARIAL
#define		AIM_ALUMNI_PAGE_COLOR_UP			FONT_MCOLOR_DKWHITE
#define		AIM_ALUMNI_PAGE_COLOR_DOWN		138

#define		AIM_ALUMNI_NAME_LINESIZE			80 * 2
#define		AIM_ALUMNI_ALUMNI_LINESIZE		7 * 80 * 2

#define		AIM_ALUMNI_NUM_FACE_COLS			5
#define		AIM_ALUMNI_NUM_FACE_ROWS			4
#define		MAX_NUMBER_OLD_MERCS_ON_PAGE	AIM_ALUMNI_NUM_FACE_ROWS * AIM_ALUMNI_NUM_FACE_COLS
#define		NUM_AIM_ARCHIVE_PAGES			4

#define		AIM_ALUMNI_START_GRID_X				LAPTOP_SCREEN_UL_X + 37
#define		AIM_ALUMNI_START_GRID_Y				LAPTOP_SCREEN_WEB_UL_Y + 68

#define		AIM_ALUMNI_GRID_OFFSET_X				90
#define		AIM_ALUMNI_GRID_OFFSET_Y				72

#define		AIM_ALUMNI_ALUMNI_FRAME_WIDTH		66
#define		AIM_ALUMNI_ALUMNI_FRAME_HEIGHT	64

#define		AIM_ALUMNI_ALUMNI_FACE_WIDTH		56
#define		AIM_ALUMNI_ALUMNI_FACE_HEIGHT		50

#define		AIM_ALUMNI_NAME_OFFSET_X				5
#define		AIM_ALUMNI_NAME_OFFSET_Y				55
#define		AIM_ALUMNI_NAME_WIDTH						AIM_ALUMNI_ALUMNI_FRAME_WIDTH - AIM_ALUMNI_NAME_OFFSET_X * 2

#define		AIM_ALUMNI_PAGE1_X							LAPTOP_SCREEN_UL_X + 100
#define		AIM_ALUMNI_PAGE1_Y							LAPTOP_SCREEN_WEB_UL_Y + 357
#define		AIM_ALUMNI_PAGE_GAP							BOTTOM_BUTTON_START_WIDTH + 25

#define		AIM_ALUMNI_PAGE_END_X						AIM_ALUMNI_PAGE1_X + (BOTTOM_BUTTON_START_WIDTH + BOTTOM_BUTTON_START_WIDTH) * 3
#define		AIM_ALUMNI_PAGE_END_Y						AIM_ALUMNI_PAGE1_Y + BOTTOM_BUTTON_START_HEIGHT

#define		AIM_ALUMNI_TITLE_X							IMAGE_OFFSET_X + 149
#define		AIM_ALUMNI_TITLE_Y							AIM_SYMBOL_Y + AIM_SYMBOL_SIZE_Y // + 2
#define		AIM_ALUMNI_TITLE_WIDTH					AIM_SYMBOL_WIDTH

#define		AIM_POPUP_WIDTH									309
#define		AIM_POPUP_TEXT_WIDTH						296
#define		AIM_POPUP_SECTION_HEIGHT				9

#define		AIM_POPUP_X											LAPTOP_SCREEN_UL_X + (500-AIM_POPUP_WIDTH)/2
#define		AIM_POPUP_Y										iScreenHeightOffset + 120 + LAPTOP_SCREEN_WEB_DELTA_Y

#define		AIM_POPUP_SHADOW_GAP						4

#define		AIM_POPUP_TEXT_X								AIM_POPUP_X

#define		AIM_ALUMNI_FACE_PANEL_X					AIM_POPUP_X + 6
#define		AIM_ALUMNI_FACE_PANEL_Y					AIM_POPUP_Y + 6
#define		AIM_ALUMNI_FACE_PANEL_WIDTH			58
#define		AIM_ALUMNI_FACE_PANEL_HEIGHT		52

#define		AIM_ALUMNI_POPUP_NAME_X					AIM_ALUMNI_FACE_PANEL_X + AIM_ALUMNI_FACE_PANEL_WIDTH + 10
#define		AIM_ALUMNI_POPUP_NAME_Y					AIM_ALUMNI_FACE_PANEL_Y + 20

#define		AIM_ALUMNI_POPUP_DESC_X					AIM_POPUP_X + 8
#define		AIM_ALUMNI_POPUP_DESC_Y					AIM_ALUMNI_FACE_PANEL_Y + AIM_ALUMNI_FACE_PANEL_HEIGHT + 5

#define		AIM_ALUMNI_DONE_X								AIM_POPUP_X + AIM_POPUP_WIDTH - AIM_ALUMNI_DONE_WIDTH - 7
#define		AIM_ALUMNI_DONE_WIDTH						36
#define		AIM_ALUMNI_DONE_HEIGHT					16

#define		AIM_ALUMNI_NAME_SIZE						80 * 2
#define		AIM_ALUMNI_DECRIPTION_SIZE			80 * 7 * 2
#define		AIM_ALUMNI_FILE_RECORD_SIZE			80 * 8 * 2
#define		AIM_ALUMNI_FULL_NAME_SIZE				80 * 2

UINT32		guiAlumniFrame;
UINT32		guiOldAim;
UINT32		guiPageButtons;
UINT32		guiAlumniPopUp;
UINT32		guiPopUpPic;
UINT32		guiDoneButton;

UINT8			gubPageNum;
UINT8			gunAlumniButtonDown=255;
BOOLEAN		gfExitingAimArchives;
UINT8			gubDrawOldMerc;
UINT8			gfDrawPopUpBox=FALSE;
BOOLEAN		gfDestroyPopUpBox;
BOOLEAN		gfFaceMouseRegionsActive;
//BOOLEAN		gfDestroyDoneRegion;
BOOLEAN		gfReDrawScreen=FALSE;

BOOLEAN		AimArchivesSubPagesVisitedFlag[NUM_AIM_ARCHIVE_PAGES] = {};

	//Mouse Regions

//Face regions
MOUSE_REGION		gMercAlumniFaceMouseRegions[ MAX_NUMBER_OLD_MERCS_ON_PAGE ];
void SelectAlumniFaceRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//Done region
MOUSE_REGION		gDoneRegion;
void SelectAlumniDoneRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//Previous Button
void		BtnAlumniPageButtonCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiAlumniPageButton[3];
INT32		guiAlumniPageButtonImage;

INT8 idPage = -1;
UINT8 cOldMerc;
BOOLEAN pageEnabled[4];

void ResetAimArchiveButtons();
void DisableAimArchiveButton();
void DisplayAlumniOldMercPopUp();
void DestroyPopUpBox();
void InitAlumniFaceRegions();
void RemoveAimAlumniFaceRegion();
void CreateDestroyDoneMouseRegion(UINT16 usPosY);
void ChangingAimArchiveSubPage( UINT8 ubSubPageNumber );

BOOLEAN vOldMerc[NUM_PROFILES];
UINT8 oldMercID;
OLD_MERC_ARCHIVES_VALUES gAimOldArchives[NUM_PROFILES];

namespace
{
LaptopPageResourceOwner gAimArchivesResources;
LaptopPageResourceOwner gAimArchivesFaceResources;
LaptopPageResourceOwner gAimArchivesDoneResources;
}

void GameInitAimArchives()
{

}

void EnterInitAimArchives()
{
	gfDrawPopUpBox=FALSE;
	gfDestroyPopUpBox = FALSE;

	memset(AimArchivesSubPagesVisitedFlag, 0,
		sizeof(AimArchivesSubPagesVisitedFlag));
	AimArchivesSubPagesVisitedFlag[0] = TRUE;
}


BOOLEAN EnterAimArchives()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner stagedResources;
	UINT16	usPosX, i;

	gAimArchivesResources.clear();
	RemoveAimAlumniFaceRegion();
	CreateDestroyDoneMouseRegion(0);
	idPage = -1;
	
	for(i=0; i<NUM_PROFILES; i++)
	{
		vOldMerc[i] = FALSE;	
	}
	
	for(i=0; i<NUM_PROFILES; i++) 
	{
		if ( gAimOldArchives[i].FaceID !=-1 ) vOldMerc[i] = TRUE;
	}
	
	pageEnabled[0] = vOldMerc[0];
	pageEnabled[1] = vOldMerc[20];
	pageEnabled[2] = vOldMerc[40];
	pageEnabled[3] = vOldMerc[60];
	
	
	gfExitingAimArchives = FALSE;
//	gubDrawOldMerc = 255;
	gfDrawPopUpBox=FALSE;
	gfDestroyPopUpBox=FALSE;

	gubPageNum = IsValidLaptopIndex(
		NUM_AIM_ARCHIVE_PAGES, giCurrentSubPage)
		? static_cast<UINT8>(giCurrentSubPage) : 0;

	// load the Alumni Frame and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\AlumniFrame.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiAlumniFrame));

	// load the 1st set of faces and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\Old_Aim.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiOldAim));

	// load the Bottom Buttons graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BottomButton.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiPageButtons));

	// load the PopupPic graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\PopupPicFrame.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiPopUpPic));

		// load the AlumniPopUp graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\AlumniPopUp.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiAlumniPopUp));

		// load the Done Button graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\DoneButton.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiDoneButton));

	//Load graphic for buttons
	if ( pageEnabled[0] == TRUE && ( pageEnabled[1] == TRUE || pageEnabled[2] == TRUE || pageEnabled[3] == TRUE ) )
	{
	CHECKF(stagedResources.addButtonImage(
		LoadButtonImageOwned("LAPTOP\\BottomButtons2.sti", -1,0,-1,1,-1),
		guiAlumniPageButtonImage));

	usPosX = AIM_ALUMNI_PAGE1_X;
	
		const INT32 pageButton = CreateIconAndTextButton( guiAlumniPageButtonImage, AimAlumniText[5], AIM_ALUMNI_PAGE_FONT,
												AIM_ALUMNI_PAGE_COLOR_UP, DEFAULT_SHADOW,
												AIM_ALUMNI_PAGE_COLOR_DOWN, DEFAULT_SHADOW,
												TEXT_CJUSTIFIED,
												usPosX+AIM_ALUMNI_PAGE_GAP, AIM_ALUMNI_PAGE1_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, BtnAlumniPageButtonCallback);
		CHECKF(stagedResources.addButton(pageButton, guiAlumniPageButton[0]));
		SetButtonCursor(guiAlumniPageButton[0], CURSOR_WWW);
		MSYS_SetBtnUserData( guiAlumniPageButton[0], 0, 0);
	}
	
	/*
	for(i=0; i<3; i++) 
	{
		guiAlumniPageButton[i] = CreateIconAndTextButton( guiAlumniPageButtonImage, AimAlumniText[i], AIM_ALUMNI_PAGE_FONT,
														AIM_ALUMNI_PAGE_COLOR_UP, DEFAULT_SHADOW,
														AIM_ALUMNI_PAGE_COLOR_DOWN, DEFAULT_SHADOW,
														TEXT_CJUSTIFIED,
														usPosX, AIM_ALUMNI_PAGE1_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
														DEFAULT_MOVE_CALLBACK, BtnAlumniPageButtonCallback);
		SetButtonCursor(guiAlumniPageButton[i], CURSOR_WWW);
		MSYS_SetBtnUserData( guiAlumniPageButton[i], 0, i);

		usPosX += AIM_ALUMNI_PAGE_GAP;
	}
	*/

	CHECKF(InitAimDefaults());
	if (!InitAimMenuBar())
	{
		RemoveAimDefaults();
		return FALSE;
	}

	gAimArchivesResources = std::move(stagedResources);
	InitAlumniFaceRegions();

	DisableAimArchiveButton();
	RenderAimArchives();
	return(TRUE);
}

void ExitAimArchives()
{
//	UINT16 i;

	gfExitingAimArchives = TRUE;

	RemoveAimAlumniFaceRegion();
	gAimArchivesResources.clear();

	RemoveAimDefaults();
	ExitAimMenuBar();
	giCurrentSubPage = gubPageNum;

	CreateDestroyDoneMouseRegion(0);
	gfDestroyPopUpBox = FALSE;
	gfDrawPopUpBox = FALSE;
	
	idPage = -1;
}

void HandleAimArchives()
{
	if( gfReDrawScreen )
	{
//		RenderAimArchives();
		fPausedReDrawScreenFlag = TRUE;

		gfReDrawScreen = FALSE;
	}
	if( gfDestroyPopUpBox )
	{
		gfDestroyPopUpBox = FALSE;

		CreateDestroyDoneMouseRegion(0);
		InitAlumniFaceRegions();
		gfDestroyPopUpBox = FALSE;
	}
}

void RenderAimArchives()
{
	HVOBJECT	hFrameHandle;
	HVOBJECT	hFaceHandle;
//	HVOBJECT	hBottomButtonHandle;
	UINT16		usPosX, usPosY,x,y,i=0;
	UINT8			ubNumRows=0;
	UINT32			uiStartLoc=0;
//	CHAR16			sText[400];
//	UINT8 p;

	DrawAimDefaults();
	DisableAimButton();

	//Draw Link Title
	DrawTextToScreen(AimAlumniText[AIM_ALUMNI_ALUMNI], AIM_ALUMNI_TITLE_X, AIM_ALUMNI_TITLE_Y, AIM_ALUMNI_TITLE_WIDTH, AIM_ALUMNI_TITLE_FONT, AIM_ALUMNI_TITLE_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	//Draw the mug shot border and face
	GetVideoObject(&hFrameHandle, guiAlumniFrame);
	GetVideoObject(&hFaceHandle, guiOldAim);
	
	if ( gubPageNum == 0)
	{
		ubNumRows = AIM_ALUMNI_NUM_FACE_ROWS;
		i=0;
	}
	else
	{
		ubNumRows = AIM_ALUMNI_NUM_FACE_ROWS;
		i=gubPageNum*20;
	}
	
/*	
	switch(gubPageNum)
	{
		case 0:
			ubNumRows = AIM_ALUMNI_NUM_FACE_ROWS;
			i=0;
			break;
		case 1:
			ubNumRows = AIM_ALUMNI_NUM_FACE_ROWS;
			i=20;
			break;
		case 2:
			ubNumRows = 2;
			i=40;
			break;
		default:
			Assert(0);
			break;
	}
*/
	usPosX = AIM_ALUMNI_START_GRID_X;
	usPosY = AIM_ALUMNI_START_GRID_Y;
	for(y=0; y<ubNumRows; y++)
	{
		for(x=0; x<AIM_ALUMNI_NUM_FACE_COLS; x++)
		{
			if ( vOldMerc[i] == TRUE ) 
			{
			//Blt face to screen
			//BltVideoObject(FRAME_BUFFER, hFaceHandle, i,usPosX+4, usPosY+4, VO_BLT_SRCTRANSPARENCY,NULL);
			BltVideoObject(FRAME_BUFFER, hFaceHandle, gAimOldArchives[i].FaceID,usPosX+4, usPosY+4, VO_BLT_SRCTRANSPARENCY,NULL);

			//Blt the alumni frame background
			BltVideoObject(FRAME_BUFFER, hFrameHandle, 0,usPosX, usPosY, VO_BLT_SRCTRANSPARENCY,NULL);

			//Display the merc's name
			//uiStartLoc = AIM_ALUMNI_NAME_LINESIZE * i;
			//LoadEncryptedDataFromFile(AIM_ALUMNI_NAME_FILE, sText, uiStartLoc, AIM_ALUMNI_NAME_SIZE );
			//DrawTextToScreen(sText, (UINT16)(usPosX + AIM_ALUMNI_NAME_OFFSET_X), (UINT16)(usPosY + AIM_ALUMNI_NAME_OFFSET_Y), AIM_ALUMNI_NAME_WIDTH, AIM_ALUMNI_NAME_FONT, AIM_ALUMNI_NAME_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);
			
			DrawTextToScreen(gAimOldArchives[i].szNickName, (UINT16)(usPosX + AIM_ALUMNI_NAME_OFFSET_X), (UINT16)(usPosY + AIM_ALUMNI_NAME_OFFSET_Y), AIM_ALUMNI_NAME_WIDTH, AIM_ALUMNI_NAME_FONT, AIM_ALUMNI_NAME_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);

			usPosX += AIM_ALUMNI_GRID_OFFSET_X;
			}
			i++;
		}
		usPosX = AIM_ALUMNI_START_GRID_X;
		usPosY += AIM_ALUMNI_GRID_OFFSET_Y;
	}
/*
	//the 3rd page now has an additional row with 1 merc on it, so add a new row
	if( gubPageNum == 2 )
	{
		//Blt face to screen
		BltVideoObject(FRAME_BUFFER, hFaceHandle, i,usPosX+4, usPosY+4, VO_BLT_SRCTRANSPARENCY,NULL);

		//Blt the alumni frame background
		BltVideoObject(FRAME_BUFFER, hFrameHandle, 0,usPosX, usPosY, VO_BLT_SRCTRANSPARENCY,NULL);

		//Display the merc's name
		uiStartLoc = AIM_ALUMNI_NAME_LINESIZE * i;
		LoadEncryptedDataFromFile(AIM_ALUMNI_NAME_FILE, sText, uiStartLoc, AIM_ALUMNI_NAME_SIZE );
		DrawTextToScreen(sText, (UINT16)(usPosX + AIM_ALUMNI_NAME_OFFSET_X), (UINT16)(usPosY + AIM_ALUMNI_NAME_OFFSET_Y), AIM_ALUMNI_NAME_WIDTH, AIM_ALUMNI_NAME_FONT, AIM_ALUMNI_NAME_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);

		usPosX += AIM_ALUMNI_GRID_OFFSET_X;
		
	}
*/



	if( gfDrawPopUpBox )
	{
		DisplayAlumniOldMercPopUp();
		RemoveAimAlumniFaceRegion();
	}


	MarkButtonsDirty( );

	RenderWWWProgramTitleBar( );

	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}





void SelectAlumniFaceRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		gfDrawPopUpBox = TRUE;
		gfReDrawScreen = TRUE;

		//gubDrawOldMerc = (UINT8)MSYS_GetRegionUserData( pRegion, 0 );
		if ( gubPageNum == 0)
			gubDrawOldMerc = (UINT8)MSYS_GetRegionUserData( pRegion, 0 );
		else if ( gubPageNum == 1)
			gubDrawOldMerc = 20 + (UINT8)MSYS_GetRegionUserData( pRegion, 0 );
		else if ( gubPageNum == 2)
			gubDrawOldMerc = 40 + (UINT8)MSYS_GetRegionUserData( pRegion, 0 );		
		else if ( gubPageNum == 3)
			gubDrawOldMerc = 60 + (UINT8)MSYS_GetRegionUserData( pRegion, 0 );	
			
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
	}
}


void BtnAlumniPageButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
		
		    if ( idPage == -1) idPage = 0;
			
			if ( idPage == 1 && pageEnabled[1] == TRUE  && pageEnabled[2] == FALSE ) idPage = -1;
			if ( idPage == 2 && pageEnabled[2] == TRUE && pageEnabled[3] == FALSE ) idPage = -1;
			if ( idPage == 3 && pageEnabled[3] == TRUE ) idPage = -1;
			idPage++;
		
			btn->uiFlags &= (~BUTTON_CLICKED_ON );

			RemoveAimAlumniFaceRegion();

			ChangingAimArchiveSubPage( idPage );

			gubPageNum = idPage;

			gfReDrawScreen = TRUE;

			gfDestroyPopUpBox = TRUE;

			gunAlumniButtonDown=255;
			ResetAimArchiveButtons();
			DisableAimArchiveButton();
			gfDrawPopUpBox = FALSE;

			InvalidateRegion(AIM_ALUMNI_PAGE1_X,AIM_ALUMNI_PAGE1_Y, AIM_ALUMNI_PAGE_END_X,AIM_ALUMNI_PAGE_END_Y);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		gunAlumniButtonDown=255;
		DisableAimArchiveButton();
		InvalidateRegion(AIM_ALUMNI_PAGE1_X,AIM_ALUMNI_PAGE1_Y, AIM_ALUMNI_PAGE_END_X,AIM_ALUMNI_PAGE_END_Y);
	}
/*
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;

		gunAlumniButtonDown=ubRetValue;

		InvalidateRegion(AIM_ALUMNI_PAGE1_X,AIM_ALUMNI_PAGE1_Y, AIM_ALUMNI_PAGE_END_X,AIM_ALUMNI_PAGE_END_Y);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			btn->uiFlags &= (~BUTTON_CLICKED_ON );

			RemoveAimAlumniFaceRegion();

			ChangingAimArchiveSubPage( ubRetValue );

			gubPageNum = ubRetValue;

			gfReDrawScreen = TRUE;

			gfDestroyPopUpBox = TRUE;

			gunAlumniButtonDown=255;
			ResetAimArchiveButtons();
			DisableAimArchiveButton();
			gfDrawPopUpBox = FALSE;

			InvalidateRegion(AIM_ALUMNI_PAGE1_X,AIM_ALUMNI_PAGE1_Y, AIM_ALUMNI_PAGE_END_X,AIM_ALUMNI_PAGE_END_Y);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		gunAlumniButtonDown=255;
		DisableAimArchiveButton();
		InvalidateRegion(AIM_ALUMNI_PAGE1_X,AIM_ALUMNI_PAGE1_Y, AIM_ALUMNI_PAGE_END_X,AIM_ALUMNI_PAGE_END_Y);
	}
*/
}


void ResetAimArchiveButtons()
{
	//int i=0;

	//for(i=0; i<3; i++)
	//{
	//	ButtonList[ guiAlumniPageButton[i] ]->uiFlags &= ~BUTTON_CLICKED_ON;
	//}
}


void DisableAimArchiveButton()
{
	if( gfExitingAimArchives == TRUE)
		return;
/*
	if( (gubPageNum == 0 ) )
	{
		ButtonList[ guiAlumniPageButton[ 0 ] ]->uiFlags |= (BUTTON_CLICKED_ON );
	}
	else if( gubPageNum == 1 )
	{
		ButtonList[ guiAlumniPageButton[ 1 ] ]->uiFlags |= (BUTTON_CLICKED_ON );
	}
	else if( gubPageNum == 2 )
	{
		ButtonList[ guiAlumniPageButton[ 2 ] ]->uiFlags |= (BUTTON_CLICKED_ON );
	}
*/
}


void DisplayAlumniOldMercPopUp()
{
	UINT8			i,ubNumLines=11; //17
	UINT16		usPosY;
	UINT8			ubNumDescLines;
	HVOBJECT	hAlumniPopUpHandle;
	HVOBJECT	hDoneHandle;
	HVOBJECT	hFacePaneHandle;
	HVOBJECT	hFaceHandle;
//	CHAR16	sName[AIM_ALUMNI_NAME_SIZE];
//	CHAR16	sDesc[AIM_ALUMNI_DECRIPTION_SIZE];
	//UINT32		uiStartLoc;
	UINT16	usStringPixLength;

	GetVideoObject(&hAlumniPopUpHandle, guiAlumniPopUp);
	GetVideoObject(&hDoneHandle, guiDoneButton);
	GetVideoObject(&hFacePaneHandle, guiPopUpPic);
	GetVideoObject(&hFaceHandle, guiOldAim);

	//Load the description
	//uiStartLoc = AIM_ALUMNI_FILE_RECORD_SIZE * gubDrawOldMerc + AIM_ALUMNI_FULL_NAME_SIZE;
	//LoadEncryptedDataFromFile(AIM_ALUMNI_FILE, sDesc, uiStartLoc, AIM_ALUMNI_DECRIPTION_SIZE);

//	usStringPixLength = StringPixLength( sDesc, AIM_ALUMNI_POPUP_FONT);
	usStringPixLength = StringPixLength( gAimOldArchives[gubDrawOldMerc].szBio, AIM_ALUMNI_POPUP_FONT);
	ubNumDescLines = (UINT8) (usStringPixLength / AIM_POPUP_TEXT_WIDTH);

	ubNumLines += ubNumDescLines;

	usPosY = AIM_POPUP_Y;

	//draw top line of the popup background
	ShadowVideoSurfaceRect( FRAME_BUFFER, AIM_POPUP_X+AIM_POPUP_SHADOW_GAP, usPosY+AIM_POPUP_SHADOW_GAP, AIM_POPUP_X + AIM_POPUP_WIDTH+AIM_POPUP_SHADOW_GAP, usPosY + AIM_POPUP_SECTION_HEIGHT+AIM_POPUP_SHADOW_GAP-1);
	BltVideoObject(FRAME_BUFFER, hAlumniPopUpHandle, 0,AIM_POPUP_X, usPosY, VO_BLT_SRCTRANSPARENCY,NULL);
	//draw mid section of the popup background
	usPosY += AIM_POPUP_SECTION_HEIGHT;
	for(i=0; i<ubNumLines; i++)
	{
		ShadowVideoSurfaceRect( FRAME_BUFFER, AIM_POPUP_X+AIM_POPUP_SHADOW_GAP, usPosY+AIM_POPUP_SHADOW_GAP, AIM_POPUP_X + AIM_POPUP_WIDTH+AIM_POPUP_SHADOW_GAP, usPosY + AIM_POPUP_SECTION_HEIGHT+AIM_POPUP_SHADOW_GAP-1);
		BltVideoObject(FRAME_BUFFER, hAlumniPopUpHandle, 1,AIM_POPUP_X, usPosY, VO_BLT_SRCTRANSPARENCY,NULL);
		usPosY += AIM_POPUP_SECTION_HEIGHT;
	}
	//draw the bottom line and done button
	ShadowVideoSurfaceRect( FRAME_BUFFER, AIM_POPUP_X+AIM_POPUP_SHADOW_GAP, usPosY+AIM_POPUP_SHADOW_GAP, AIM_POPUP_X + AIM_POPUP_WIDTH+AIM_POPUP_SHADOW_GAP, usPosY + AIM_POPUP_SECTION_HEIGHT+AIM_POPUP_SHADOW_GAP-1);
	BltVideoObject(FRAME_BUFFER, hAlumniPopUpHandle, 2,AIM_POPUP_X, usPosY, VO_BLT_SRCTRANSPARENCY,NULL);
	BltVideoObject(FRAME_BUFFER, hDoneHandle, 0,AIM_ALUMNI_DONE_X, usPosY-AIM_ALUMNI_DONE_HEIGHT, VO_BLT_SRCTRANSPARENCY,NULL);
	DrawTextToScreen(AimAlumniText[AIM_ALUMNI_DONE], (UINT16)(AIM_ALUMNI_DONE_X+1), (UINT16)(usPosY-AIM_ALUMNI_DONE_HEIGHT+3), AIM_ALUMNI_DONE_WIDTH, AIM_ALUMNI_POPUP_NAME_FONT, AIM_ALUMNI_POPUP_NAME_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	
	CreateDestroyDoneMouseRegion(usPosY);

	///blt face panale and the mecs fce
	BltVideoObject(FRAME_BUFFER, hFacePaneHandle, 0,AIM_ALUMNI_FACE_PANEL_X, AIM_ALUMNI_FACE_PANEL_Y, VO_BLT_SRCTRANSPARENCY,NULL);
	
	//BltVideoObject(FRAME_BUFFER, hFaceHandle, gubDrawOldMerc, AIM_ALUMNI_FACE_PANEL_X+1, AIM_ALUMNI_FACE_PANEL_Y+1, VO_BLT_SRCTRANSPARENCY,NULL);
	BltVideoObject(FRAME_BUFFER, hFaceHandle, gAimOldArchives[gubDrawOldMerc].FaceID, AIM_ALUMNI_FACE_PANEL_X+1, AIM_ALUMNI_FACE_PANEL_Y+1, VO_BLT_SRCTRANSPARENCY,NULL);

	//Load and display the name
//	uiStartLoc = AIM_ALUMNI_NAME_SIZE * gubDrawOldMerc;
//	LoadEncryptedDataFromFile(AIM_ALUMNI_NAME_FILE, sName, uiStartLoc, AIM_ALUMNI_NAME_SIZE);
//	uiStartLoc = AIM_ALUMNI_FILE_RECORD_SIZE * gubDrawOldMerc;
//	LoadEncryptedDataFromFile( AIM_ALUMNI_FILE, sName, uiStartLoc, AIM_ALUMNI_FULL_NAME_SIZE );

	//DrawTextToScreen(sName, AIM_ALUMNI_POPUP_NAME_X, AIM_ALUMNI_POPUP_NAME_Y, 0, AIM_ALUMNI_POPUP_NAME_FONT, AIM_ALUMNI_POPUP_NAME_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	
	DrawTextToScreen(gAimOldArchives[gubDrawOldMerc].szName, AIM_ALUMNI_POPUP_NAME_X, AIM_ALUMNI_POPUP_NAME_Y, 0, AIM_ALUMNI_POPUP_NAME_FONT, AIM_ALUMNI_POPUP_NAME_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	//Display the description
//	DisplayWrappedString(AIM_ALUMNI_POPUP_DESC_X, AIM_ALUMNI_POPUP_DESC_Y, AIM_POPUP_TEXT_WIDTH, 2, AIM_ALUMNI_POPUP_FONT, AIM_ALUMNI_POPUP_COLOR, sDesc, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	DisplayWrappedString(AIM_ALUMNI_POPUP_DESC_X, AIM_ALUMNI_POPUP_DESC_Y, AIM_POPUP_TEXT_WIDTH, 2, AIM_ALUMNI_POPUP_FONT, AIM_ALUMNI_POPUP_COLOR, gAimOldArchives[gubDrawOldMerc].szBio, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);


	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}


void DestroyPopUpBox()
{
	gfDestroyPopUpBox = FALSE;
	RenderAimArchives();
}



void InitAlumniFaceRegions()
{
	UINT16	usPosX, usPosY,i,x,y, usNumRows, p;
	LaptopPageResourceOwner stagedResources;

	if(gfFaceMouseRegionsActive)
		return;
/*
	if( gubPageNum == 2 )
		usNumRows = 2;
	else
		usNumRows = AIM_ALUMNI_NUM_FACE_ROWS;
*/

	usNumRows = AIM_ALUMNI_NUM_FACE_ROWS;

	usPosX = AIM_ALUMNI_START_GRID_X;
	usPosY = AIM_ALUMNI_START_GRID_Y;
	
	
	if (gubPageNum == 0) 
	{
		p=0;
	}
	else if (gubPageNum == 1) 
	{
		p=20;
	}
	else if (gubPageNum == 2) 
	{
		p=40;
	}
	else if (gubPageNum == 3) 
	{
		p=60;
	}
	else p=0;
	
	i=0;
	for(y=0; y<usNumRows; y++)
	{
	
		for(x=0; x<AIM_ALUMNI_NUM_FACE_COLS; x++)
		{
			if ( vOldMerc[p] == TRUE ) 
			{
			MSYS_DefineRegion( &gMercAlumniFaceMouseRegions[ i ], usPosX, usPosY, (INT16)(usPosX + AIM_ALUMNI_ALUMNI_FACE_WIDTH), (INT16)(usPosY + AIM_ALUMNI_ALUMNI_FACE_HEIGHT), MSYS_PRIORITY_HIGH,
								CURSOR_WWW, MSYS_NO_CALLBACK, SelectAlumniFaceRegionCallBack);
			// Add region
			stagedResources.addRegion(gMercAlumniFaceMouseRegions[i]);
			//MSYS_SetRegionUserData( &gMercAlumniFaceMouseRegions[ i ], 0, i+(20*gubPageNum));
			MSYS_SetRegionUserData( &gMercAlumniFaceMouseRegions[ i ], 0, i);
			usPosX += AIM_ALUMNI_GRID_OFFSET_X;
			}
			i++;
			p++;
		}
		usPosX = AIM_ALUMNI_START_GRID_X;
		usPosY += AIM_ALUMNI_GRID_OFFSET_Y;
	}

	gAimArchivesFaceResources = std::move(stagedResources);
	gfFaceMouseRegionsActive = TRUE;
}

void RemoveAimAlumniFaceRegion()
{
	if(!gfFaceMouseRegionsActive)
		return;

	gAimArchivesFaceResources.clear();
	gfFaceMouseRegionsActive = FALSE;
}




void CreateDestroyDoneMouseRegion(UINT16 usPosY)
{
	static BOOLEAN DoneRegionCreated=FALSE;

	if( ( !DoneRegionCreated ) && ( usPosY != 0) )
	{
		usPosY -= AIM_ALUMNI_DONE_HEIGHT;
		MSYS_DefineRegion( &gDoneRegion, AIM_ALUMNI_DONE_X-2, usPosY, (AIM_ALUMNI_DONE_X-2 + AIM_ALUMNI_DONE_WIDTH), (INT16)(usPosY + AIM_ALUMNI_DONE_HEIGHT), MSYS_PRIORITY_HIGH,
							CURSOR_WWW, MSYS_NO_CALLBACK, SelectAlumniDoneRegionCallBack);
		DoneRegionCreated =
			gAimArchivesDoneResources.addRegion(gDoneRegion);
	}

	if( DoneRegionCreated && usPosY == 0)
	{
		gAimArchivesDoneResources.clear();
		DoneRegionCreated = FALSE;
//		gfDestroyDoneRegion = FALSE;
	}
}


void SelectAlumniDoneRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		gfDestroyPopUpBox = TRUE;
		gfDrawPopUpBox = FALSE;
		gfReDrawScreen = TRUE;
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
	}
}

void ChangingAimArchiveSubPage( UINT8 ubSubPageNumber )
{
	if (!IsValidLaptopIndex(NUM_AIM_ARCHIVE_PAGES, ubSubPageNumber))
		return;

	fLoadPendingFlag = TRUE;

	if( AimArchivesSubPagesVisitedFlag[ ubSubPageNumber ] == FALSE )
	{
		fConnectingToSubPage = TRUE;
		fFastLoadFlag = FALSE;

		AimArchivesSubPagesVisitedFlag[ ubSubPageNumber ] = TRUE;
	}
	else
	{
		fConnectingToSubPage = TRUE;
		fFastLoadFlag = TRUE;
	}
}









