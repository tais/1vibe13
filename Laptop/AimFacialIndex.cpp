	#include "laptop.h"
	#include "AimFacialIndex.h"
	#include "WordWrap.h"
	#include "Utilities.h"
	#include "WCheck.h"
	#include "stdio.h"
	#include "aim.h"
	#include "Soldier Profile.h"
	#include "TacticalActor.h"
	#include "Text.h"
	#include "AimSort.h"
	#include "Assignments.h"
	#include "GameSettings.h"
	#include "english.h"
	#include "sysutil.h"
	#include "LaptopPageResourceOwner.h"
	#include "AimWebsiteLayout.h"


extern UINT8	gubCurrentSortMode; // symbol already defined in AimSort.cpp (jonathanl)
extern UINT8	gubCurrentListMode; // symbol already declared globally in AimSort.cpp (jonathanl)
extern UINT8	gbCurrentIndex;


UINT32		guiMugShotBorder;
UINT32		guiAimFiFace[ NUM_PROFILES ]; //MAX_NUMBER_MERCS

BOOLEAN		gAimProfiles[ NUM_PROFILES ];

UINT8 START_MERC =0;

#define		AIM_FI_HELP_FONT				FONT10ARIAL
#define		AIM_FI_HELP_TITLE_FONT		FONT12ARIAL


//Mouse Regions

//Face regions
MOUSE_REGION		gMercFaceMouseRegions[ NUM_PROFILES ]; //MAX_NUMBER_MERCS
void SelectMercFaceRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );
void SelectMercFaceMoveRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//Screen region, used to right click to go back to previous page
MOUSE_REGION		gScreenMouseRegions;
void SelectScreenRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );


BOOLEAN DrawMercsFaceToScreen(UINT8 ubMercID, UINT8 ubImage);

//New profiles
INT32		guiPreviousNewProfilesNextButtonImage;
void BtnNewProfilesButtonCallback(GUI_BUTTON *btn,INT32 reason);

INT32 PAGE_BUTTON;

namespace
{
LaptopPageResourceOwner gAimFacialIndexResources;

AimWebsiteLayoutModel::FacialIndexLayout CurrentAimFacialIndexLayout()
{
	return AimWebsiteLayoutModel::MakeFacialIndexLayout(
		gGameExternalOptions.gfUseNewStartingGearInterface != FALSE,
		{iScreenWidthOffset, iScreenHeightOffset,
		 IMAGE_OFFSET_X, IMAGE_OFFSET_Y, LAPTOP_SCREEN_WEB_DELTA_Y});
}

void ChangeAimFacialIndexPage(bool forward)
{
	for (std::size_t i = 0; i < MAX_NUMBER_MERCS; ++i)
		gAimProfiles[i] = TRUE;

	const std::size_t nextStart = forward
		? AimWebsiteLayoutModel::NextFacialIndexPageStart(
			MAX_NUMBER_MERCS, START_MERC)
		: AimWebsiteLayoutModel::PreviousFacialIndexPageStart(
			MAX_NUMBER_MERCS, START_MERC);
	START_MERC = static_cast<UINT8>(nextStart);

	ExitAimFacialIndex();
	EnterAimFacialIndex();
}
}

//Hotkey Assignment
void HandleAimFacialIndexKeyBoardInput();

void GameInitAimFacialIndex()
{

}

/// Calculates the needed string for the page button based on number of mercs
/// and currently displayed page.
static STR16 GetPageButtonText() 
{
	return gszAimPages[AimWebsiteLayoutModel::FacialIndexPageTextIndex(
		MAX_NUMBER_MERCS, START_MERC)];
}

void BtnNewProfilesButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			ChangeAimFacialIndexPage(true);
			return;
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}

BOOLEAN EnterAimFacialIndex()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner stagedResources;
	STR				sFaceLoc = "FACES\\";
	char			sTemp[100];
	const auto layout = CurrentAimFacialIndexLayout();
	const std::size_t visibleSlots =
		AimWebsiteLayoutModel::FacialIndexVisibleSlotCount(
			MAX_NUMBER_MERCS, START_MERC);
	
	gAimFacialIndexResources.clear();
	for(std::size_t i=0; i<MAX_NUMBER_MERCS; i++)
	{
		gAimProfiles[i] = TRUE;
	}

	// load the Portait graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\MugShotBorder3.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiMugShotBorder));
	
	//Page button
	CHECKF(stagedResources.addButtonImage(
		LoadButtonImageOwned("LAPTOP\\BottomButtons2.sti", -1,0,-1,1,-1),
		guiPreviousNewProfilesNextButtonImage));
	
	STR16 buttonText = GetPageButtonText();
	const INT32 pageButton = CreateIconAndTextButton( guiPreviousNewProfilesNextButtonImage, buttonText, FONT14ARIAL,
														FONT_MCOLOR_DKWHITE, DEFAULT_SHADOW,
														138, DEFAULT_SHADOW,
														TEXT_CJUSTIFIED,
												layout.pageButton.x, layout.pageButton.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, BtnNewProfilesButtonCallback);
	CHECKF(stagedResources.addButton(pageButton, PAGE_BUTTON));
	SetButtonCursor(PAGE_BUTTON, CURSOR_WWW );
	
	for(std::size_t slot = 0; slot < visibleSlots; ++slot)
	{
		const std::size_t profileIndex = slot + START_MERC;
		if ( gAimProfiles[profileIndex] == TRUE ) //new profiles
		{
			const auto cell = layout.grid.cell(slot);
			MSYS_DefineRegion( &gMercFaceMouseRegions[slot], cell.x, cell.y, cell.right(), cell.bottom(), MSYS_PRIORITY_HIGH,
								CURSOR_WWW, SelectMercFaceMoveRegionCallBack, SelectMercFaceRegionCallBack);
			// Add region
			CHECKF(stagedResources.addRegion(gMercFaceMouseRegions[slot]));
			MSYS_SetRegionUserData( &gMercFaceMouseRegions[slot], 0, slot);

			if (gGameExternalOptions.fReadProfileDataFromXML)
			{
				// HEADROCK PROFEX: Do not read direct profile number, instead, look inside the profile for a ubFaceIndex value.
				sprintf(sTemp, "%s%02d.sti", sFaceLoc, gMercProfiles[AimMercArray[profileIndex]].ubFaceIndex);
			}
			else
			{
				sprintf(sTemp, "%s%02d.sti", sFaceLoc, AimMercArray[profileIndex] );
			}
			VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
			FilenameForBPP(sTemp, VObjectDesc.ImageFile);
			CHECKF(stagedResources.addVideoObject(
				&VObjectDesc, guiAimFiFace[slot]));
		}
	}

	MSYS_DefineRegion( &gScreenMouseRegions, LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y, LAPTOP_SCREEN_LR_X, LAPTOP_SCREEN_WEB_LR_Y, MSYS_PRIORITY_HIGH-1,
						CURSOR_LAPTOP_SCREEN, MSYS_NO_CALLBACK, SelectScreenRegionCallBack);
	// Add region
	CHECKF(stagedResources.addRegion(gScreenMouseRegions));

	CHECKF(InitAimDefaults());
	if (!InitAimMenuBar())
	{
		RemoveAimDefaults();
		return FALSE;
	}

	gAimFacialIndexResources = std::move(stagedResources);
	RenderAimFacialIndex();

	return( TRUE );
}

void ExitAimFacialIndex()
{
	gAimFacialIndexResources.clear();
	RemoveAimDefaults();
	ExitAimMenuBar();
}

void HandleAimFacialIndex()
{
//	if( fShowBookmarkInfo )
//		fPausedReDrawScreenFlag = TRUE;

	HandleAimFacialIndexKeyBoardInput();
}

BOOLEAN RenderAimFacialIndex()
{
	CHAR16		sString[150];
	const auto layout = CurrentAimFacialIndexLayout();
	const std::size_t visibleSlots =
		AimWebsiteLayoutModel::FacialIndexVisibleSlotCount(
			MAX_NUMBER_MERCS, START_MERC);
	
	SpecifyButtonText( PAGE_BUTTON, GetPageButtonText() );

	if ( MAX_NUMBER_MERCS > 40 )
	{
		ShowButton( PAGE_BUTTON );
	}
	else
	{
		HideButton( PAGE_BUTTON );
	}

	DrawAimDefaults();

	//Display the 'A.I.M. Members Sorted Ascending By Price' type string
	if( gubCurrentListMode == AIM_ASCEND )
		swprintf(sString, AimFiText[ AIM_FI_AIM_MEMBERS_SORTED_ASCENDING ], AimFiText[gubCurrentSortMode] );
	else
		swprintf(sString, AimFiText[ AIM_FI_AIM_MEMBERS_SORTED_DESCENDING ], AimFiText[gubCurrentSortMode] );

	DrawTextToScreen(sString, layout.memberTitle.origin.x, layout.memberTitle.origin.y, layout.memberTitle.width, AIM_MAINTITLE_FONT, AIM_MAINTITLE_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);
	

	//Draw the mug shot border and face
	for(std::size_t slot = 0; slot < visibleSlots; ++slot)
	{
		const std::size_t profileIndex = slot + START_MERC;
		if ( gAimProfiles[profileIndex] == TRUE ) //new profiles
		{
			const auto nickname = layout.grid.nickname(slot);
			DrawMercsFaceToScreen(static_cast<UINT8>(slot), 1);
			DrawTextToScreen(gMercProfiles[AimMercArray[profileIndex]].zNickname, nickname.origin.x, nickname.origin.y, nickname.width, AIM_FONT12ARIAL, AIM_FONT_MCOLOR_WHITE, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);
		}
	}

	DisableAimButton();

	//display the 'left and right click' onscreen help msg
	DrawTextToScreen(AimFiText[AIM_FI_LEFT_CLICK], layout.help.leftClick.origin.x, layout.help.leftClick.origin.y, layout.help.leftClick.width, AIM_FI_HELP_TITLE_FONT, AIM_FONT_MCOLOR_WHITE, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);
	DrawTextToScreen(AimFiText[AIM_FI_TO_SELECT], layout.help.leftClick.origin.x, layout.help.leftClick.origin.y+layout.help.descriptionOffsetY, layout.help.leftClick.width, AIM_FI_HELP_FONT, AIM_FONT_MCOLOR_WHITE, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);
	DrawTextToScreen(AimFiText[AIM_FI_RIGHT_CLICK], layout.help.rightClick.origin.x, layout.help.rightClick.origin.y, layout.help.rightClick.width, AIM_FI_HELP_TITLE_FONT, AIM_FONT_MCOLOR_WHITE, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);
	DrawTextToScreen(AimFiText[AIM_FI_TO_ENTER_SORT_PAGE], layout.help.rightClick.origin.x, layout.help.rightClick.origin.y+layout.help.descriptionOffsetY, layout.help.rightClick.width, AIM_FI_HELP_FONT, AIM_FONT_MCOLOR_WHITE, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);

	MarkButtonsDirty( );

	RenderWWWProgramTitleBar( );

	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
	return(TRUE);
}

void SelectMercFaceRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		guiCurrentLaptopMode = LAPTOP_MODE_AIM_MEMBERS;
		gbCurrentIndex = (UINT8) MSYS_GetRegionUserData( pRegion, 0 ) + START_MERC;
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
		guiCurrentLaptopMode = LAPTOP_MODE_AIM_MEMBERS_SORTED_FILES;
	}
}


void SelectScreenRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
		guiCurrentLaptopMode = LAPTOP_MODE_AIM_MEMBERS_SORTED_FILES;
	}
}


void SelectMercFaceMoveRegionCallBack(MOUSE_REGION * pRegion, INT32 reason )
{
	UINT8	ubMercNum;

	ubMercNum = (UINT8) MSYS_GetRegionUserData( pRegion, 0 );

//	fReDrawNewMailFlag = TRUE;
	if ( gAimProfiles[ubMercNum + START_MERC] == TRUE ) //new  profiles
	{
	if( reason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		pRegion->uiFlags &= (~BUTTON_CLICKED_ON );
		DrawMercsFaceToScreen(ubMercNum, 1);
		InvalidateRegion(pRegion->RegionTopLeftX, pRegion->RegionTopLeftY, pRegion->RegionBottomRightX, pRegion->RegionBottomRightY);
	}
	else if( reason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		pRegion->uiFlags |= BUTTON_CLICKED_ON ;
		DrawMercsFaceToScreen(ubMercNum, 0);
		InvalidateRegion(pRegion->RegionTopLeftX, pRegion->RegionTopLeftY, pRegion->RegionBottomRightX, pRegion->RegionBottomRightY);
	}
	
	}
}

BOOLEAN DrawMercsFaceToScreen(UINT8 ubMercID, UINT8 ubImage)
{
	HVOBJECT	hMugShotBorderHandle;
	HVOBJECT	hFaceHandle;
	TacticalActor	*pSoldier=NULL;
	const auto grid = CurrentAimFacialIndexLayout().grid;
	const auto portrait = grid.cell(ubMercID);
	const auto face = grid.face(ubMercID);
	const auto status = grid.status(ubMercID);

	//pSoldier = FindSoldierByProfileID( AimMercArray[ubMercID], TRUE );
	pSoldier = FindSoldierByProfileID( AimMercArray[ubMercID + START_MERC], TRUE );
	
	


	//Blt the portrait background
	GetVideoObject(&hMugShotBorderHandle, guiMugShotBorder);
	BltVideoObject(FRAME_BUFFER, hMugShotBorderHandle, ubImage,portrait.x, portrait.y, VO_BLT_SRCTRANSPARENCY,NULL);

	//Blt face to screen
	GetVideoObject(&hFaceHandle, guiAimFiFace[ubMercID]);
	BltVideoObject(FRAME_BUFFER, hFaceHandle, 0,face.x, face.y, VO_BLT_SRCTRANSPARENCY,NULL);

	//if( IsMercDead( AimMercArray[ubMercID] ) )
	if( IsMercDead( AimMercArray[ubMercID + START_MERC] ) )
	{
		//get the face object
		GetVideoObject(&hFaceHandle, guiAimFiFace[ubMercID]);

		//if the merc is dead
		//shade the face red, (to signif that he is dead)
		hFaceHandle->pShades[ 0 ]		= Create16BPPPaletteShaded( hFaceHandle->pPaletteEntry, DEAD_MERC_COLOR_RED, DEAD_MERC_COLOR_GREEN, DEAD_MERC_COLOR_BLUE, TRUE );

		//set the red pallete to the face
		SetObjectHandleShade( guiAimFiFace[ubMercID], 0 );

		//Blt face to screen
		BltVideoObject(FRAME_BUFFER, hFaceHandle, 0,face.x, face.y, VO_BLT_SRCTRANSPARENCY,NULL);

		DrawTextToScreen(AimFiText[AIM_FI_DEAD], status.origin.x, status.origin.y, status.width, FONT10ARIAL, 145, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);
	}

	//else if the merc is currently a POW or, the merc was fired as a pow
	//else if( gMercProfiles[ AimMercArray[ubMercID] ].bMercStatus == MERC_FIRED_AS_A_POW	|| ( pSoldier &&	pSoldier->assignment().current() == ASSIGNMENT_POW ) )
	else if( gMercProfiles[ AimMercArray[ubMercID + START_MERC] ].bMercStatus == MERC_FIRED_AS_A_POW	|| ( pSoldier &&	pSoldier->assignment().current() == ASSIGNMENT_POW ) )
	{
		ShadowVideoSurfaceRect( FRAME_BUFFER, face.x, face.y, face.right(), face.bottom());
		DrawTextToScreen( pPOWStrings[0], status.origin.x, status.origin.y, status.width, FONT10ARIAL, 145, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);
	}

	//if the merc is on our team
	else if( pSoldier != NULL )
	{
		ShadowVideoSurfaceRect( FRAME_BUFFER, face.x, face.y, face.right(), face.bottom());
		DrawTextToScreen( MercInfo[MERC_FILES_ALREADY_HIRED], status.origin.x, status.origin.y, status.width, FONT10ARIAL, 145, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);
	}

	//if the merc is away, shadow his/her face and blit 'away' over top
	//else if( !IsMercHireable( AimMercArray[ubMercID] ) )
	else if( !IsMercHireable( AimMercArray[ubMercID + START_MERC ] ) )	
	{
		ShadowVideoSurfaceRect( FRAME_BUFFER, face.x, face.y, face.right(), face.bottom());
		DrawTextToScreen( AimFiText[AIM_FI_DEAD+1], status.origin.x, status.origin.y, status.width, FONT10ARIAL, 145, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);
		//if not enough room use this..
		//AimFiText[AIM_FI_AWAY]
	}

	return(TRUE);
}



void HandleAimFacialIndexKeyBoardInput()
{
	InputAtom					InputEvent;

	while (DequeueSpecificEvent(&InputEvent, KEY_DOWN|KEY_UP|KEY_REPEAT))
	{//!HandleTextInput( &InputEvent ) &&
		if( InputEvent.usEvent == KEY_DOWN )
		{
			switch (InputEvent.usParam)
			{
				case BACKSPACE:
				case 'q':
					// back to AIM sorting screen
					guiCurrentLaptopMode = LAPTOP_MODE_AIM_MEMBERS_SORTED_FILES;
					break;
				case ENTER:
				case 'e':
					guiCurrentLaptopMode = LAPTOP_MODE_AIM_MEMBERS;
					break;
				case LEFTARROW:
				case 'a':
					if (MAX_NUMBER_MERCS >
						AimWebsiteLayoutModel::kFacialIndexPageCapacity)
						ChangeAimFacialIndexPage(false);
					break;
				case RIGHTARROW:
				case 'd':
					if (MAX_NUMBER_MERCS >
						AimWebsiteLayoutModel::kFacialIndexPageCapacity)
						ChangeAimFacialIndexPage(true);
					break;
				default:
					HandleKeyBoardShortCutsForLapTop( InputEvent.usEvent, InputEvent.usParam, InputEvent.usKeyState );
					break;
			}
		}
	}
}
