	#include "laptop.h"
	#include "florist.h"
	#include "florist Cards.h"
	#include "Utilities.h"
	#include "WordWrap.h"
	#include "Cursors.h"
	#include "Encrypted File.h"
	#include "Text.h"
	#include "FloristSiteModel.h"
	#include "LaptopPageResourceOwner.h"

#include <utility>



#define		FLORIST_CARDS_SENTENCE_FONT			FONT12ARIAL
#define		FLORIST_CARDS_SENTENCE_COLOR		FONT_MCOLOR_WHITE

UINT32		guiCardBackground;

INT8			gbCurrentlySelectedCard;

//link to the card gallery
MOUSE_REGION	gSelectedFloristCardsRegion[kFloristCardCount];
void SelectFloristCardsRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );


INT32		guiFlowerCardsButtonImage;
void		BtnFlowerCardsBackButtonCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiFlowerCardsBackButton;

namespace
{
	LaptopPageResourceOwner gFloristCardsResources;

	FloristCardsLayout CurrentFloristCardsLayout()
	{
		return MakeFloristCardsLayout(
			{LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y});
	}
}


void GameInitFloristCards()
{

}

BOOLEAN EnterFloristCards()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner staged;
	const auto layout = CurrentFloristCardsLayout();

	gFloristCardsResources.clear();
	if (!AddFloristDefaults(staged)) return FALSE;

	// load the Flower Account Box graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\CardBlank.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiCardBackground)) return FALSE;

	for(std::size_t cardIndex = 0;
		cardIndex < layout.cards.capacity(); ++cardIndex)
	{
		const auto card = layout.cards.card(cardIndex);
		MSYS_DefineRegion( &gSelectedFloristCardsRegion[cardIndex], card.x, card.y, card.right(), card.bottom(), MSYS_PRIORITY_HIGH,
							CURSOR_WWW, MSYS_NO_CALLBACK, SelectFloristCardsRegionCallBack );
		if (!staged.addRegion(gSelectedFloristCardsRegion[cardIndex]))
			return FALSE;
		MSYS_SetRegionUserData( &gSelectedFloristCardsRegion[cardIndex], 0, cardIndex );
	}


	if (!staged.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\FloristButtons.sti", -1, 0, -1, 1, -1),
		guiFlowerCardsButtonImage)) return FALSE;

	if (!staged.addButton(CreateIconAndTextButton( guiFlowerCardsButtonImage, sFloristCards[FLORIST_CARDS_BACK], FLORIST_BUTTON_TEXT_FONT,
													FLORIST_BUTTON_TEXT_UP_COLOR, FLORIST_BUTTON_TEXT_SHADOW_COLOR,
													FLORIST_BUTTON_TEXT_DOWN_COLOR, FLORIST_BUTTON_TEXT_SHADOW_COLOR,
													TEXT_CJUSTIFIED,
													layout.backButton.x, layout.backButton.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
													DEFAULT_MOVE_CALLBACK, BtnFlowerCardsBackButtonCallback),
		guiFlowerCardsBackButton)) return FALSE;
	SetButtonCursor(guiFlowerCardsBackButton, CURSOR_WWW );
	gFloristCardsResources = std::move(staged);


	//passing the currently selected card to -1, so it is not used
	gbCurrentlySelectedCard = -1;

	RenderFloristCards();
	return(TRUE);
}

void ExitFloristCards()
{
	gFloristCardsResources.clear();
}

void HandleFloristCards()
{

}

void RenderFloristCards()
{
	CHAR16		sTemp[ 640 ];
	UINT32	uiStartLoc=0;
	HVOBJECT hPixHandle;
	UINT16		usHeightOffset;
	const auto layout = CurrentFloristCardsLayout();

	DisplayFloristDefaults();

	DrawTextToScreen( sFloristCards[FLORIST_CARDS_CLICK_SELECTION], layout.title.origin.x, layout.title.origin.y, layout.title.width, FONT10ARIAL, FLORIST_CARDS_SENTENCE_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED );

	GetVideoObject(&hPixHandle, guiCardBackground);
	for(std::size_t cardIndex = 0;
		cardIndex < layout.cards.capacity(); ++cardIndex)
	{
		const auto card = layout.cards.card(cardIndex);
		//The flower account box
		BltVideoObject(FRAME_BUFFER, hPixHandle, 0, card.x, card.y, VO_BLT_SRCTRANSPARENCY,NULL);

		//Get and display the card saying
		uiStartLoc = FLOR_CARD_TEXT_TITLE_SIZE * cardIndex;
		LoadEncryptedDataFromFile(FLOR_CARD_TEXT_FILE, sTemp, uiStartLoc, FLOR_CARD_TEXT_TITLE_SIZE);

			usHeightOffset = IanWrappedStringHeight( card.x + layout.cardTextInsetX, card.y, layout.cardTextWidth, 2,
														FLORIST_CARDS_SENTENCE_FONT, FLORIST_CARDS_SENTENCE_COLOR, sTemp,
														0, FALSE, 0);

			usHeightOffset = static_cast<UINT16>(
				CenteredFloristTextOffset(layout.cardTextHeight,
					usHeightOffset));

			IanDisplayWrappedString( card.x + layout.cardTextInsetX, card.y + layout.cardTextInsetY + usHeightOffset, layout.cardTextWidth, 2,
														FLORIST_CARDS_SENTENCE_FONT, FLORIST_CARDS_SENTENCE_COLOR, sTemp,
														0, FALSE, 0);
	}

	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}



void SelectFloristCardsRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		gbCurrentlySelectedCard = (UINT8) MSYS_GetRegionUserData( pRegion, 0 );

		guiCurrentLaptopMode = LAPTOP_MODE_FLORIST_ORDERFORM;
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
	}
}



void BtnFlowerCardsBackButtonCallback(GUI_BUTTON *btn,INT32 reason)
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

			guiCurrentLaptopMode = LAPTOP_MODE_FLORIST_ORDERFORM;

			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}
