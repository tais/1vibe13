	#include "laptop.h"
	#include "insurance Info.h"
	#include "insurance.h"
	#include "WCheck.h"
	#include "Utilities.h"
	#include "WordWrap.h"
	#include "Cursors.h"
	#include "Insurance Text.h"
	#include "Text.h"
#include "InsuranceSiteModel.h"
#include "LaptopPageResourceOwner.h"

#include <algorithm>
#include <iterator>
#include <utility>


#define		INS_INFO_FRAUD_TEXT_COLOR					FONT_MCOLOR_RED


#define		INS_INFO_SUBTITLE_X								86 + LAPTOP_SCREEN_UL_X
#define		INS_INFO_SUBTITLE_Y								62 + LAPTOP_SCREEN_WEB_UL_Y

#define		INS_INFO_SUBTITLE_LINE_Y					INS_INFO_SUBTITLE_Y + 14
#define		INS_INFO_SUBTITLE_LINE_WIDTH			375

#define		INS_INFO_FIRST_PARAGRAPH_WIDTH		INS_INFO_SUBTITLE_LINE_WIDTH
#define		INS_INFO_FIRST_PARAGRAPH_X				INS_INFO_SUBTITLE_X
#define		INS_INFO_FIRST_PARAGRAPH_Y				INS_INFO_SUBTITLE_LINE_Y + 9

#define		INS_INFO_SPACE_BN_PARAGRAPHS			12

#define		INS_INFO_INFO_TOC_TITLE_X					(iScreenWidthOffset + 170)	// ROMAN
#define		INS_INFO_INFO_TOC_TITLE_Y					54 + LAPTOP_SCREEN_WEB_UL_Y

#define		INS_INFO_TOC_SUBTITLE_X						INS_INFO_SUBTITLE_X


#define		INS_INFO_LINK_TO_CONTRACT_Y				392 + LAPTOP_SCREEN_WEB_UL_Y
#define		INS_INFO_LINK_TO_CONTRACT_WIDTH		97//107

#define		INS_INFO_LINK_START_OFFSET				20//14
#define		INS_INFO_LINK_START_X							262 + INS_INFO_LINK_START_OFFSET + iScreenWidthOffset // ROMAN

#define		INS_INFO_LINK_TO_CONTRACT_TEXT_Y	355 + LAPTOP_SCREEN_WEB_UL_Y


UINT32	guiBulletImage;

//The list of Info sub pages
enum
{
	INS_INFO_INFO_TOC,
	INS_INFO_SUBMIT_CLAIM,
	INS_INFO_PREMIUMS,
	INS_INFO_RENEWL,
	INS_INFO_CANCELATION,
	INS_INFO_LAST_PAGE,
};
UINT8	gubCurrentInsInfoSubPage = 0;

BOOLEAN		InsuranceInfoSubPagesVisitedFlag[ INS_INFO_LAST_PAGE ];



INT32		guiInsPrevButtonImage;
void		BtnInsPrevButtonCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiInsPrevBackButton;

INT32		guiInsNextButtonImage;
void		BtnInsNextButtonCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiInsNextBackButton;


//link to the varios pages
MOUSE_REGION	gSelectedInsuranceInfoLinkRegion;
void SelectInsuranceLinkRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

MOUSE_REGION	gSelectedInsuranceInfoHomeLinkRegion;
void SelectInsuranceInfoHomeLinkRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );



void DisplaySubmitClaimPage();
void DisplayPremiumPage();
void DisplayRenewingPremiumPage();
void DisplayCancelationPagePage();
void DisableArrowButtonsIfOnLastOrFirstPage();
void ChangingInsuranceInfoSubPage( UINT8 ubSubPageNumber );
void DisplayInfoTocPage();

namespace
{
LaptopPageResourceOwner gInsuranceInfoResources;
}

void GameInitInsuranceInfo()
{

}

void EnterInitInsuranceInfo()
{
	std::fill(std::begin(InsuranceInfoSubPagesVisitedFlag),
		std::end(InsuranceInfoSubPagesVisitedFlag), FALSE);

}

BOOLEAN EnterInsuranceInfo()
{
	VOBJECT_DESC	VObjectDesc;
	UINT16					usPosX;
	LaptopPageResourceOwner staged;

	gInsuranceInfoResources.clear();
	if (!AddInsuranceDefaults(staged)) return FALSE;

	// load the Insurance bullet graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\bullet.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiBulletImage)) return FALSE;


	//left arrow
	if (!staged.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\InsLeftButton.sti", 2, 0, -1, 1, -1),
		guiInsPrevButtonImage)) return FALSE;
	if (!staged.addButton(CreateIconAndTextButton( guiInsPrevButtonImage, InsInfoText[INS_INFO_PREVIOUS], INS_FONT_BIG,
													INS_FONT_COLOR, INS_FONT_SHADOW,
													INS_FONT_COLOR, INS_FONT_SHADOW,
													TEXT_CJUSTIFIED,
													INS_INFO_LEFT_ARROW_BUTTON_X, INS_INFO_LEFT_ARROW_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
													DEFAULT_MOVE_CALLBACK, BtnInsPrevButtonCallback),
		guiInsPrevBackButton)) return FALSE;
	SetButtonCursor( guiInsPrevBackButton, CURSOR_WWW );
	SpecifyButtonTextOffsets( guiInsPrevBackButton, 17, 16, FALSE );


	//Right arrow
	if (!staged.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\InsRightButton.sti", 2, 0, -1, 1, -1),
		guiInsNextButtonImage)) return FALSE;
	if (!staged.addButton(CreateIconAndTextButton( guiInsNextButtonImage, InsInfoText[INS_INFO_NEXT], INS_FONT_BIG,
													INS_FONT_COLOR, INS_FONT_SHADOW,
													INS_FONT_COLOR, INS_FONT_SHADOW,
													TEXT_CJUSTIFIED,
													INS_INFO_RIGHT_ARROW_BUTTON_X, INS_INFO_RIGHT_ARROW_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
													DEFAULT_MOVE_CALLBACK, BtnInsNextButtonCallback),
		guiInsNextBackButton)) return FALSE;
	SetButtonCursor( guiInsNextBackButton, CURSOR_WWW );
	SpecifyButtonTextOffsets( guiInsNextBackButton, 18, 16, FALSE );


	usPosX = INS_INFO_LINK_START_X;
	//link to go to the contract page
	//link to go to the home page
	MSYS_DefineRegion( &gSelectedInsuranceInfoHomeLinkRegion, usPosX, INS_INFO_LINK_TO_CONTRACT_Y-37, (UINT16)(usPosX + INS_INFO_LINK_TO_CONTRACT_WIDTH), INS_INFO_LINK_TO_CONTRACT_Y+2, MSYS_PRIORITY_HIGH,
					CURSOR_WWW, MSYS_NO_CALLBACK, SelectInsuranceInfoHomeLinkRegionCallBack);
	if (!staged.addRegion(gSelectedInsuranceInfoHomeLinkRegion)) return FALSE;

	usPosX += INS_INFO_LINK_START_OFFSET + INS_INFO_LINK_TO_CONTRACT_WIDTH;
	MSYS_DefineRegion( &gSelectedInsuranceInfoLinkRegion, usPosX, INS_INFO_LINK_TO_CONTRACT_Y-37, (UINT16)(usPosX + INS_INFO_LINK_TO_CONTRACT_WIDTH), INS_INFO_LINK_TO_CONTRACT_Y+2, MSYS_PRIORITY_HIGH,
					CURSOR_WWW, MSYS_NO_CALLBACK, SelectInsuranceLinkRegionCallBack);
	if (!staged.addRegion(gSelectedInsuranceInfoLinkRegion)) return FALSE;


	gubCurrentInsInfoSubPage = INS_INFO_INFO_TOC;
	gInsuranceInfoResources = std::move(staged);

	RenderInsuranceInfo();

	return(TRUE);
}

void ExitInsuranceInfo()
{
	gInsuranceInfoResources.clear();
}

void HandleInsuranceInfo()
{

}

void RenderInsuranceInfo()
{
	CHAR16		sText[800];
	UINT16		usPosX;

	if (!IsInsuranceInfoPage(gubCurrentInsInfoSubPage,
			INS_INFO_LAST_PAGE))
		gubCurrentInsInfoSubPage = INS_INFO_INFO_TOC;
	DisableArrowButtonsIfOnLastOrFirstPage();

	DisplayInsuranceDefaults();

	SetFontShadow( INS_FONT_SHADOW );


	//Display the red bar under the link at the bottom
	DisplaySmallColouredLineWithShadow( INS_INFO_SUBTITLE_X, INS_INFO_SUBTITLE_LINE_Y, INS_INFO_SUBTITLE_X + INS_INFO_SUBTITLE_LINE_WIDTH, INS_INFO_SUBTITLE_LINE_Y );

	switch( gubCurrentInsInfoSubPage )
	{
		case INS_INFO_INFO_TOC:
			DisplayInfoTocPage();
			break;

		case INS_INFO_SUBMIT_CLAIM:
			DisplaySubmitClaimPage();
			break;

		case INS_INFO_PREMIUMS:
			DisplayPremiumPage();
			break;

		case INS_INFO_RENEWL:
			DisplayRenewingPremiumPage();
			break;

		case INS_INFO_CANCELATION:
			DisplayCancelationPagePage();
			break;
	}

	usPosX = INS_INFO_LINK_START_X;

	//Display the red bar under the link at the bottom.	and the text
	DisplaySmallColouredLineWithShadow( usPosX, INS_INFO_LINK_TO_CONTRACT_Y, (UINT16)(usPosX + INS_INFO_LINK_TO_CONTRACT_WIDTH), INS_INFO_LINK_TO_CONTRACT_Y );
	swprintf( sText, L"%s", pMessageStrings[ MSG_HOMEPAGE ] );
	DisplayWrappedString( usPosX, INS_INFO_LINK_TO_CONTRACT_TEXT_Y+14, INS_INFO_LINK_TO_CONTRACT_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	usPosX += INS_INFO_LINK_START_OFFSET + INS_INFO_LINK_TO_CONTRACT_WIDTH;

	//Display the red bar under the link at the bottom.	and the text
	DisplaySmallColouredLineWithShadow( usPosX, INS_INFO_LINK_TO_CONTRACT_Y, (UINT16)(usPosX + INS_INFO_LINK_TO_CONTRACT_WIDTH), INS_INFO_LINK_TO_CONTRACT_Y );
	GetInsuranceText( INS_SNGL_TO_ENTER_REVIEW, sText );
	DisplayWrappedString( usPosX, INS_INFO_LINK_TO_CONTRACT_TEXT_Y, INS_INFO_LINK_TO_CONTRACT_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);


	SetFontShadow(DEFAULT_SHADOW);

	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}



void BtnInsPrevButtonCallback(GUI_BUTTON *btn,INT32 reason)
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

			if( gubCurrentInsInfoSubPage > 0 )
				gubCurrentInsInfoSubPage --;

			ChangingInsuranceInfoSubPage( gubCurrentInsInfoSubPage );
//			fPausedReDrawScreenFlag = TRUE;

			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}

void BtnInsNextButtonCallback(GUI_BUTTON *btn,INT32 reason)
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

			if( gubCurrentInsInfoSubPage < (INS_INFO_LAST_PAGE -1) )
				gubCurrentInsInfoSubPage ++;

			ChangingInsuranceInfoSubPage( gubCurrentInsInfoSubPage );

//			fPausedReDrawScreenFlag = TRUE;

			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}

void SelectInsuranceLinkRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		guiCurrentLaptopMode = LAPTOP_MODE_INSURANCE_CONTRACT;
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
	}
}

void SelectInsuranceInfoHomeLinkRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		guiCurrentLaptopMode = LAPTOP_MODE_INSURANCE;
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
	}
}


void DisplaySubmitClaimPage()
{
	CHAR16		sText[800];
	UINT16 usNewLineOffset = 0;
	UINT16	usPosX;

	usNewLineOffset = INS_INFO_FIRST_PARAGRAPH_Y;

	//Display the title slogan
	GetInsuranceText( INS_SNGL_SUBMITTING_CLAIM, sText );
	DrawTextToScreen( sText, INS_INFO_SUBTITLE_X, INS_INFO_SUBTITLE_Y, 0, INS_FONT_BIG, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED );

	//Display the title slogan
	GetInsuranceText( INS_MLTI_U_CAN_REST_ASSURED, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;

	GetInsuranceText( INS_MLTI_HAD_U_HIRED_AN_INDIVIDUAL, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;

	//display the BIG FRAUD
	GetInsuranceText( INS_SNGL_FRAUD, sText );
	DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, (UINT16)(usNewLineOffset-1), INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_BIG, INS_INFO_FRAUD_TEXT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	usPosX = INS_INFO_FIRST_PARAGRAPH_X + StringPixLength( sText, INS_FONT_BIG ) + 2;
	GetInsuranceText( INS_MLTI_WE_RESERVE_THE_RIGHT, sText );
	usNewLineOffset += DisplayWrappedString( usPosX, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	GetInsuranceText( INS_MLTI_SHOULD_THERE_BE_GROUNDS, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;

	GetInsuranceText( INS_MLTI_SHOULD_SUCH_A_SITUATION, sText );
	DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

}

void DisplayPremiumPage()
{
	CHAR16		sText[800];
	UINT16 usNewLineOffset = 0;
	HVOBJECT hPixHandle;

	usNewLineOffset = INS_INFO_FIRST_PARAGRAPH_Y;


	//Display the title slogan
	GetInsuranceText( INS_SNGL_PREMIUMS, sText );
	DrawTextToScreen( sText, INS_INFO_SUBTITLE_X, INS_INFO_SUBTITLE_Y, 0, INS_FONT_BIG, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED );

	GetInsuranceText( INS_MLTI_EACH_TIME_U_COME_TO_US, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;

	//Get and display the insurance bullet
	GetVideoObject(&hPixHandle, guiBulletImage );
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, VO_BLT_SRCTRANSPARENCY,NULL);

	GetInsuranceText( INS_MLTI_LENGTH_OF_EMPLOYMENT_CONTRACT, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X+INSURANCE_BULLET_TEXT_OFFSET_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;


	//Get and display the insurance bullet
	GetVideoObject(&hPixHandle, guiBulletImage );
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, VO_BLT_SRCTRANSPARENCY,NULL);

	GetInsuranceText( INS_MLTI_EMPLOYEES_AGE_AND_HEALTH, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X+INSURANCE_BULLET_TEXT_OFFSET_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;


	//Get and display the insurance bullet
	GetVideoObject(&hPixHandle, guiBulletImage );
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, VO_BLT_SRCTRANSPARENCY,NULL);

	GetInsuranceText( INS_MLTI_EMPLOOYEES_TRAINING_AND_EXP, sText );
	DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X+INSURANCE_BULLET_TEXT_OFFSET_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
}



void DisplayRenewingPremiumPage()
{
	CHAR16		sText[800];
	UINT16 usNewLineOffset = 0;
//	HVOBJECT hPixHandle;

	usNewLineOffset = INS_INFO_FIRST_PARAGRAPH_Y;

	//Display the title slogan
	GetInsuranceText( INS_SNGL_RENEWL_PREMIUMS, sText );
	DrawTextToScreen( sText, INS_INFO_SUBTITLE_X, INS_INFO_SUBTITLE_Y, 0, INS_FONT_BIG, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED );

	GetInsuranceText( INS_MLTI_WHEN_IT_COMES_TIME_TO_RENEW, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;

	GetInsuranceText( INS_MLTI_SHOULD_THE_PROJECT_BE_GOING_WELL, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;

	//display the LOWER PREMIUM FOR RENWING EARLY
	GetInsuranceText( INS_SNGL_LOWER_PREMIUMS_4_RENEWING, sText );
	DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, (UINT16)(usNewLineOffset-1), INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_BIG, INS_INFO_FRAUD_TEXT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

/*
	//Get and display the insurance bullet
	GetVideoObject(&hPixHandle, guiBulletImage );
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, VO_BLT_SRCTRANSPARENCY,NULL);

	GetInsuranceText( INS_MLTI_IF_U_EXTEND_THE_CONTRACT, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X+INSURANCE_BULLET_TEXT_OFFSET_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;
*/
}




void DisplayCancelationPagePage()
{
	CHAR16		sText[800];
	UINT16 usNewLineOffset = 0;

	usNewLineOffset = INS_INFO_FIRST_PARAGRAPH_Y;

	//Display the title slogan
	GetInsuranceText( INS_SNGL_POLICY_CANCELATIONS, sText );
	DrawTextToScreen( sText, INS_INFO_SUBTITLE_X, INS_INFO_SUBTITLE_Y, 0, INS_FONT_BIG, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED );

	GetInsuranceText( INS_MLTI_WE_WILL_ACCEPT_INS_CANCELATION, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;

	GetInsuranceText( INS_MLTI_1_HOUR_EXCLUSION_A, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;

	GetInsuranceText( INS_MLTI_1_HOUR_EXCLUSION_B, sText );
	DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
}

void DisableArrowButtonsIfOnLastOrFirstPage()
{
	if( gubCurrentInsInfoSubPage == INS_INFO_INFO_TOC)
		DisableButton( guiInsPrevBackButton);
	else
		EnableButton( guiInsPrevBackButton );

	if( gubCurrentInsInfoSubPage == INS_INFO_LAST_PAGE - 1 )
		DisableButton( guiInsNextBackButton);
	else
		EnableButton( guiInsNextBackButton );
}



void ChangingInsuranceInfoSubPage( UINT8 ubSubPageNumber )
{
	if (!IsInsuranceInfoPage(ubSubPageNumber, INS_INFO_LAST_PAGE)) return;
	fLoadPendingFlag = TRUE;

	if( InsuranceInfoSubPagesVisitedFlag[ ubSubPageNumber ] == FALSE )
	{
		fConnectingToSubPage = TRUE;
		fFastLoadFlag = FALSE;

		InsuranceInfoSubPagesVisitedFlag[ ubSubPageNumber ] = TRUE;
	}
	else
	{
		fConnectingToSubPage = TRUE;
		fFastLoadFlag = TRUE;
	}
}

void DisplayInfoTocPage()
{
	CHAR16		sText[800];
	UINT16 usNewLineOffset = 0;
	HVOBJECT hPixHandle;
	UINT16		usPosY;

	usNewLineOffset = INS_INFO_FIRST_PARAGRAPH_Y;

	//Display the title slogan
	GetInsuranceText( INS_SNGL_HOW_DOES_INS_WORK, sText );
	DrawTextToScreen( sText, INS_INFO_INFO_TOC_TITLE_X, INS_INFO_INFO_TOC_TITLE_Y, 439, INS_FONT_BIG, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED );

	//Display the First paragraph
	GetInsuranceText( INS_MLTI_HIRING_4_SHORT_TERM_HIGH_RISK_1, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;

	//Display the 2nd paragraph
	GetInsuranceText( INS_MLTI_HIRING_4_SHORT_TERM_HIGH_RISK_2, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;


	//Display the sub title
	GetInsuranceText( INS_SNGL_WE_CAN_OFFER_U, sText );
	DrawTextToScreen( sText, INS_INFO_TOC_SUBTITLE_X, usNewLineOffset, 640-INS_INFO_INFO_TOC_TITLE_X, INS_FONT_BIG, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED );
	usPosY = usNewLineOffset + 12;
	DisplaySmallColouredLineWithShadow( INS_INFO_SUBTITLE_X, usPosY, (UINT16)(INS_INFO_SUBTITLE_X + INS_INFO_SUBTITLE_LINE_WIDTH), usPosY );

	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;


	//
	// Premiuns bulleted sentence
	//

	//Get and display the insurance bullet
	GetVideoObject(&hPixHandle, guiBulletImage );
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, VO_BLT_SRCTRANSPARENCY,NULL);

	GetInsuranceText( INS_MLTI_REASONABLE_AND_FLEXIBLE, sText );
	usNewLineOffset += DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X+INSURANCE_BULLET_TEXT_OFFSET_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usNewLineOffset += INS_INFO_SPACE_BN_PARAGRAPHS;



	//
	// Quick and efficient claims
	//

	//Get and display the insurance bullet
	GetVideoObject(&hPixHandle, guiBulletImage );
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, INS_INFO_FIRST_PARAGRAPH_X, usNewLineOffset, VO_BLT_SRCTRANSPARENCY,NULL);

	GetInsuranceText( INS_MLTI_QUICKLY_AND_EFFICIENT, sText );
	DisplayWrappedString( INS_INFO_FIRST_PARAGRAPH_X+INSURANCE_BULLET_TEXT_OFFSET_X, usNewLineOffset, INS_INFO_FIRST_PARAGRAPH_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
}


