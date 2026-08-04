	#include "laptop.h"
	#include "florist.h"
	#include "florist Gallery.h"
	#include "Utilities.h"
	#include "WordWrap.h"
	#include "Cursors.h"
	#include "stdio.h"
	#include "Encrypted File.h"
	#include "Text.h"
	#include "FloristSiteModel.h"
	#include "LaptopPageResourceOwner.h"

#include <algorithm>
#include <iterator>
#include <utility>




#define	FLOR_GALLERY_TITLE_FONT								FONT10ARIAL
#define	FLOR_GALLERY_TITLE_COLOR							FONT_MCOLOR_WHITE

#define	FLOR_GALLERY_FLOWER_TITLE_FONT				FONT14ARIAL
#define	FLOR_GALLERY_FLOWER_TITLE_COLOR				FONT_MCOLOR_WHITE

#define	FLOR_GALLERY_FLOWER_PRICE_FONT				FONT12ARIAL
#define	FLOR_GALLERY_FLOWER_PRICE_COLOR				FONT_MCOLOR_WHITE

#define	FLOR_GALLERY_FLOWER_DESC_FONT					FONT12ARIAL
#define	FLOR_GALLERY_FLOWER_DESC_COLOR				FONT_MCOLOR_WHITE

#define	FLOR_GALLERY_NUMBER_FLORAL_BUTTONS		kFloristGalleryPageSize
#define	FLOR_GALLERY_NUMBER_FLORAL_IMAGES			kFloristGalleryFlowerCount

#define	FLOR_GALLERY_FLOWER_DESC_TEXT_FONT		FONT12ARIAL
#define	FLOR_GALLERY_FLOWER_DESC_TEXT_COLOR		FONT_MCOLOR_WHITE

#define FLOR_GALLERY_BACK_BUTTON_X						(LAPTOP_SCREEN_UL_X + 8)
#define FLOR_GALLERY_BACK_BUTTON_Y						LAPTOP_SCREEN_WEB_UL_Y + 12

#define FLOR_GALLERY_NEXT_BUTTON_X						LAPTOP_SCREEN_UL_X + 420
#define FLOR_GALLERY_NEXT_BUTTON_Y						FLOR_GALLERY_BACK_BUTTON_Y

#define FLOR_GALLERY_FLOWER_BUTTON_X					(LAPTOP_SCREEN_UL_X + 7)
#define FLOR_GALLERY_FLOWER_BUTTON_Y					LAPTOP_SCREEN_WEB_UL_Y + 74

#define FLOR_GALLERY_FLOWER_BUTTON_OFFSET_Y		112

#define FLOR_GALLERY_TITLE_TEXT_X							(LAPTOP_SCREEN_UL_X + 0)
#define FLOR_GALLERY_TITLE_TEXT_Y							LAPTOP_SCREEN_WEB_UL_Y + 48
#define FLOR_GALLERY_TITLE_TEXT_WIDTH						(613 - 111)

#define FLOR_GALLERY_FLOWER_TITLE_X						FLOR_GALLERY_FLOWER_BUTTON_X + 88

#define FLOR_GALLERY_DESC_WIDTH								390

#define FLOR_GALLERY_FLOWER_TITLE_OFFSET_Y		9
#define FLOR_GALLERY_FLOWER_PRICE_OFFSET_Y		FLOR_GALLERY_FLOWER_TITLE_OFFSET_Y + 17
#define FLOR_GALLERY_FLOWER_DESC_OFFSET_Y			FLOR_GALLERY_FLOWER_PRICE_OFFSET_Y + 15


UINT32	guiFlowerImages[ 3 ];

UINT32	guiCurrentlySelectedFlower=0;

UINT8		gubCurFlowerIndex=0;
UINT8		gubCurNumberOfFlowers=0;
BOOLEAN gfRedrawFloristGallery=FALSE;

BOOLEAN		FloristGallerySubPagesVisitedFlag[kFloristGalleryPageCount];

//Floral buttons
INT32	guiGalleryFlowerButtonImage;
void			BtnGalleryFlowerButtonCallback(GUI_BUTTON *btn,INT32 reason);
// File-private: same name is used as a scalar UINT32 in
// Laptop/florist.cpp (different size + meaning).
static UINT32	guiGalleryButton[ FLOR_GALLERY_NUMBER_FLORAL_BUTTONS ];

//Next Previous buttons
INT32		guiFloralGalleryButtonImage;
void		BtnFloralGalleryNextButtonCallback(GUI_BUTTON *btn,INT32 reason);
void		BtnFloralGalleryBackButtonCallback(GUI_BUTTON *btn,INT32 reason);
UINT32		guiFloralGalleryButton[2];


BOOLEAN InitFlowerButtons(LaptopPageResourceOwner& owner);
BOOLEAN DisplayFloralDescriptions();
void ChangingFloristGallerySubPage( UINT8 ubSubPageNumber );

namespace
{
	LaptopPageResourceOwner gFloristGalleryResources;
	LaptopPageResourceOwner gFloristGalleryFlowerResources;
}

void GameInitFloristGallery()
{

}

void EnterInitFloristGallery()
{
	std::fill(std::begin(FloristGallerySubPagesVisitedFlag),
		std::end(FloristGallerySubPagesVisitedFlag), FALSE);
}


BOOLEAN EnterFloristGallery()
{
	LaptopPageResourceOwner staged;
	LaptopPageResourceOwner stagedFlowers;
	gFloristGalleryFlowerResources.clear();
	gFloristGalleryResources.clear();
	gubCurFlowerIndex = static_cast<UINT8>(FloristGalleryPageStart(
		gubCurFlowerIndex, FLOR_GALLERY_NUMBER_FLORAL_IMAGES,
		FLOR_GALLERY_NUMBER_FLORAL_BUTTONS));
	if (!AddFloristDefaults(staged)) return FALSE;

	//the next previous buttons
	if (!staged.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\FloristButtons.sti", -1, 0, -1, 1, -1),
		guiFloralGalleryButtonImage)) return FALSE;

	if (!staged.addButton(CreateIconAndTextButton( guiFloralGalleryButtonImage, sFloristGalleryText[FLORIST_GALLERY_PREV], FLORIST_BUTTON_TEXT_FONT,
													FLORIST_BUTTON_TEXT_UP_COLOR, FLORIST_BUTTON_TEXT_SHADOW_COLOR,
													FLORIST_BUTTON_TEXT_DOWN_COLOR, FLORIST_BUTTON_TEXT_SHADOW_COLOR,
													TEXT_CJUSTIFIED,
													FLOR_GALLERY_BACK_BUTTON_X, FLOR_GALLERY_BACK_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
													DEFAULT_MOVE_CALLBACK, BtnFloralGalleryBackButtonCallback),
		guiFloralGalleryButton[0])) return FALSE;
	SetButtonCursor(guiFloralGalleryButton[0], CURSOR_WWW );

	if (!staged.addButton(CreateIconAndTextButton( guiFloralGalleryButtonImage, sFloristGalleryText[FLORIST_GALLERY_NEXT], FLORIST_BUTTON_TEXT_FONT,
													FLORIST_BUTTON_TEXT_UP_COLOR, FLORIST_BUTTON_TEXT_SHADOW_COLOR,
													FLORIST_BUTTON_TEXT_DOWN_COLOR, FLORIST_BUTTON_TEXT_SHADOW_COLOR,
													TEXT_CJUSTIFIED,
													FLOR_GALLERY_NEXT_BUTTON_X, FLOR_GALLERY_NEXT_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
													DEFAULT_MOVE_CALLBACK, BtnFloralGalleryNextButtonCallback),
		guiFloralGalleryButton[1])) return FALSE;
	SetButtonCursor(guiFloralGalleryButton[1], CURSOR_WWW );

	if (!InitFlowerButtons(stagedFlowers)) return FALSE;
	gFloristGalleryResources = std::move(staged);
	gFloristGalleryFlowerResources = std::move(stagedFlowers);
	RenderFloristGallery();

	return(TRUE);
}

void ExitFloristGallery()
{
	gFloristGalleryFlowerResources.clear();
	gFloristGalleryResources.clear();
}

void HandleFloristGallery()
{
	if( gfRedrawFloristGallery )
	{
		LaptopPageResourceOwner staged;
		if (!InitFlowerButtons(staged)) return;
		gFloristGalleryFlowerResources.clear();
		gFloristGalleryFlowerResources = std::move(staged);
		gfRedrawFloristGallery=FALSE;

		fPausedReDrawScreenFlag = TRUE;
	}

}

void RenderFloristGallery()
{
	DisplayFloristDefaults();

	DrawTextToScreen(sFloristGalleryText[FLORIST_GALLERY_CLICK_TO_ORDER], FLOR_GALLERY_TITLE_TEXT_X, FLOR_GALLERY_TITLE_TEXT_Y, FLOR_GALLERY_TITLE_TEXT_WIDTH, FLOR_GALLERY_TITLE_FONT, FLOR_GALLERY_TITLE_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);
	DrawTextToScreen(sFloristGalleryText[FLORIST_GALLERY_ADDIFTIONAL_FEE], FLOR_GALLERY_TITLE_TEXT_X, FLOR_GALLERY_TITLE_TEXT_Y+11, FLOR_GALLERY_TITLE_TEXT_WIDTH, FLOR_GALLERY_TITLE_FONT, FLOR_GALLERY_TITLE_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED );

	DisplayFloralDescriptions();

	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}


void BtnFloralGalleryNextButtonCallback(GUI_BUTTON *btn,INT32 reason)
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


			const UINT8 next = static_cast<UINT8>(
				NextFloristGalleryPageStart(gubCurFlowerIndex,
					FLOR_GALLERY_NUMBER_FLORAL_IMAGES,
					FLOR_GALLERY_NUMBER_FLORAL_BUTTONS));
			if (next == gubCurFlowerIndex) return;
			gubCurFlowerIndex = next;
			ChangingFloristGallerySubPage(gubCurFlowerIndex);

			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);

			gfRedrawFloristGallery = TRUE;
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


void BtnFloralGalleryBackButtonCallback(GUI_BUTTON *btn,INT32 reason)
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


			if( gubCurFlowerIndex != 0 )
			{
				gubCurFlowerIndex = static_cast<UINT8>(
					PreviousFloristGalleryPageStart(gubCurFlowerIndex,
						FLOR_GALLERY_NUMBER_FLORAL_IMAGES,
						FLOR_GALLERY_NUMBER_FLORAL_BUTTONS));

				ChangingFloristGallerySubPage( gubCurFlowerIndex );
			}
			else
			{
				guiCurrentLaptopMode = LAPTOP_MODE_FLORIST;
			}

			gfRedrawFloristGallery = TRUE;

			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}



void BtnGalleryFlowerButtonCallback(GUI_BUTTON *btn,INT32 reason)
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

			guiCurrentlySelectedFlower = (UINT8) MSYS_GetBtnUserData( btn, 0 );
			guiCurrentLaptopMode = LAPTOP_MODE_FLORIST_ORDERFORM;

			gfShowBookmarks = FALSE;

			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


BOOLEAN InitFlowerButtons(LaptopPageResourceOwner& owner)
{
	UINT16 i,j, count;
	UINT16 usPosY;
	char		sTemp[40];
	VOBJECT_DESC	VObjectDesc;


	gubCurFlowerIndex = static_cast<UINT8>(FloristGalleryPageStart(
		gubCurFlowerIndex, FLOR_GALLERY_NUMBER_FLORAL_IMAGES,
		FLOR_GALLERY_NUMBER_FLORAL_BUTTONS));
	gubCurNumberOfFlowers = static_cast<UINT8>(std::min<UINT16>(
		FLOR_GALLERY_NUMBER_FLORAL_BUTTONS,
		FLOR_GALLERY_NUMBER_FLORAL_IMAGES - gubCurFlowerIndex));

	//the 10 pictures of the flowers
	count = gubCurFlowerIndex;
	for(i=0; i<gubCurNumberOfFlowers; i++)
	{
		// load the handbullet graphic and add it
		snprintf(sTemp, sizeof(sTemp), "LAPTOP\\Flower_%d.sti", count);
		VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
		FilenameForBPP(sTemp, VObjectDesc.ImageFile);
		if (!owner.addVideoObject(&VObjectDesc, guiFlowerImages[i]))
			return FALSE;
		count++;
	}

	//the buttons with the flower pictures on them
	usPosY = FLOR_GALLERY_FLOWER_BUTTON_Y;
//	usPosX = FLOR_GALLERY_FLOWER_BUTTON_X;
	count = gubCurFlowerIndex;
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\GalleryButtons.sti", -1, 0, -1, 1, -1),
		guiGalleryFlowerButtonImage)) return FALSE;
	for(j=0; j<gubCurNumberOfFlowers; j++)
	{
		if (!owner.addButton(QuickCreateButton( guiGalleryFlowerButtonImage, FLOR_GALLERY_FLOWER_BUTTON_X, usPosY,
																	BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
																	DEFAULT_MOVE_CALLBACK, (GUI_CALLBACK)BtnGalleryFlowerButtonCallback),
			guiGalleryButton[j])) return FALSE;
		SetButtonCursor( guiGalleryButton[j], CURSOR_WWW);
		MSYS_SetBtnUserData( guiGalleryButton[j], 0, count);

		SpecifyButtonIcon( guiGalleryButton[j], guiFlowerImages[ j ], 0, 5, 5, FALSE );
		usPosY += FLOR_GALLERY_FLOWER_BUTTON_OFFSET_Y;
		count ++;
	}

	//if its the first page, display the 'back' text	in place of the 'prev' text on the top left button
	if( gubCurFlowerIndex == 0 )
		SpecifyButtonText( guiFloralGalleryButton[0], sFloristGalleryText[FLORIST_GALLERY_HOME] );
	else
		SpecifyButtonText( guiFloralGalleryButton[0], sFloristGalleryText[FLORIST_GALLERY_PREV] );

	//if it is the last page disable the next button
	if (NextFloristGalleryPageStart(gubCurFlowerIndex,
			FLOR_GALLERY_NUMBER_FLORAL_IMAGES,
			FLOR_GALLERY_NUMBER_FLORAL_BUTTONS) == gubCurFlowerIndex)
		DisableButton( guiFloralGalleryButton[1] );
	else
		EnableButton( guiFloralGalleryButton[1] );


	return(TRUE);
}


BOOLEAN DisplayFloralDescriptions()
{
	CHAR16		sTemp[ 640 ];
	UINT32	uiStartLoc=0, i;
	UINT16	usPosY, usPrice = 0;

	gubCurFlowerIndex = static_cast<UINT8>(FloristGalleryPageStart(
		gubCurFlowerIndex, FLOR_GALLERY_NUMBER_FLORAL_IMAGES,
		FLOR_GALLERY_NUMBER_FLORAL_BUTTONS));
	gubCurNumberOfFlowers = static_cast<UINT8>(std::min<UINT16>(
		FLOR_GALLERY_NUMBER_FLORAL_BUTTONS,
		FLOR_GALLERY_NUMBER_FLORAL_IMAGES - gubCurFlowerIndex));

	usPosY = FLOR_GALLERY_FLOWER_BUTTON_Y;
	for(i=0; i<gubCurNumberOfFlowers; i++)
	{
		//Display Flower title
		uiStartLoc = FLOR_GALLERY_TEXT_TOTAL_SIZE * (i + gubCurFlowerIndex);
		LoadEncryptedDataFromFile(FLOR_GALLERY_TEXT_FILE, sTemp, uiStartLoc, FLOR_GALLERY_TEXT_TITLE_SIZE);
		DrawTextToScreen(sTemp, FLOR_GALLERY_FLOWER_TITLE_X, (UINT16)(usPosY+FLOR_GALLERY_FLOWER_TITLE_OFFSET_Y), 0, FLOR_GALLERY_FLOWER_TITLE_FONT, FLOR_GALLERY_FLOWER_TITLE_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED	);

		//Display Flower Price
		uiStartLoc = FLOR_GALLERY_TEXT_TOTAL_SIZE * (i + gubCurFlowerIndex) + FLOR_GALLERY_TEXT_TITLE_SIZE;
		LoadEncryptedDataFromFile(FLOR_GALLERY_TEXT_FILE, sTemp, uiStartLoc, FLOR_GALLERY_TEXT_PRICE_SIZE);
		if (swscanf(sTemp, L"%hu", &usPrice) != 1) usPrice = 0;
		swprintf( sTemp, L"$%d.00 %s", usPrice, pMessageStrings[ MSG_USDOLLAR_ABBREVIATION ] );
		DrawTextToScreen(sTemp, FLOR_GALLERY_FLOWER_TITLE_X, (UINT16)(usPosY+FLOR_GALLERY_FLOWER_PRICE_OFFSET_Y), 0, FLOR_GALLERY_FLOWER_PRICE_FONT, FLOR_GALLERY_FLOWER_PRICE_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED	);

		//Display Flower Desc
		uiStartLoc = FLOR_GALLERY_TEXT_TOTAL_SIZE * (i + gubCurFlowerIndex) + FLOR_GALLERY_TEXT_TITLE_SIZE + FLOR_GALLERY_TEXT_PRICE_SIZE;
		LoadEncryptedDataFromFile(FLOR_GALLERY_TEXT_FILE, sTemp, uiStartLoc, FLOR_GALLERY_TEXT_DESC_SIZE);
		DisplayWrappedString(FLOR_GALLERY_FLOWER_TITLE_X, (UINT16)(usPosY+FLOR_GALLERY_FLOWER_DESC_OFFSET_Y), FLOR_GALLERY_DESC_WIDTH, 2, FLOR_GALLERY_FLOWER_DESC_FONT, FLOR_GALLERY_FLOWER_DESC_COLOR,	sTemp, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

		usPosY += FLOR_GALLERY_FLOWER_BUTTON_OFFSET_Y;
	}

	return(TRUE);
}


void ChangingFloristGallerySubPage( UINT8 ubSubPageNumber )
{
	const std::size_t subPageNumber = FloristGalleryPageNumber(
		ubSubPageNumber, FLOR_GALLERY_NUMBER_FLORAL_IMAGES,
		FLOR_GALLERY_NUMBER_FLORAL_BUTTONS);
	if (subPageNumber >= std::size(FloristGallerySubPagesVisitedFlag))
		return;
	fLoadPendingFlag = TRUE;

	if( FloristGallerySubPagesVisitedFlag[subPageNumber] == FALSE )
	{
		fConnectingToSubPage = TRUE;
		fFastLoadFlag = FALSE;

		FloristGallerySubPagesVisitedFlag[subPageNumber] = TRUE;
	}
	else
	{
		fConnectingToSubPage = TRUE;
		fFastLoadFlag = TRUE;
	}
}


