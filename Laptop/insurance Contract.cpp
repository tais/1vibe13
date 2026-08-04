	#include "laptop.h"
#include "CampaignLaptopCommunicationsPolicy.h"
#include "GameContext.h"
#include "InsuranceSiteModel.h"
#include "LaptopPageResourceOwner.h"
#include "LaptopSafety.h"
#include "TacticalActorModifiers.h"
#include "TacticalActor.h"
#include "TacticalActorEmploymentTypes.h"
	#include "insurance.h"
	#include "insurance Contract.h"
	#include "WCheck.h"
	#include "Utilities.h"
	#include "WordWrap.h"
	#include "Cursors.h"
	#include "Insurance Text.h"
	#include "stdio.h"
	#include "Soldier Profile.h"
	#include "Soldier Profile Constants.h"
	#include "Overhead.h"
	#include "Soldier Add.h"
	#include "SoldierRepository.h"
	#include "Game Clock.h"
	#include "finances.h"
	#include "history.h"
	#include "Game Event Hook.h"
	#include "LaptopSave.h"
	#include "english.h"
	#include "Text.h"
	#include "random.h"
	#include "Strategic Status.h"
	#include "Assignments.h"
	#include "Map Screen Interface.h"
#include "Interface.h"				// added by Flugente
#include "Quests.h"
#include "ub_config.h"
#include "VideoResourceHandle.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#define		INS_CTRCT_ORDER_GRID_WIDTH					132
#define		INS_CTRCT_ORDER_GRID_HEIGHT					216
#define		INS_CTRCT_ORDER_GRID_OFFSET_X				INS_CTRCT_ORDER_GRID_WIDTH + 2


#define		INS_CTRCT_ORDER_GRID1_X							(76 + LAPTOP_SCREEN_UL_X)
#define		INS_CTRCT_ORDER_GRID1_Y							(126 + LAPTOP_SCREEN_WEB_UL_Y)

#define		INS_CTRCT_ORDER_GRID2_X							INS_CTRCT_ORDER_GRID1_X + INS_CTRCT_ORDER_GRID_OFFSET_X

#define		INS_CTRCT_ORDER_GRID3_X							INS_CTRCT_ORDER_GRID2_X + INS_CTRCT_ORDER_GRID_OFFSET_X

#define		INS_CTRCT_OG_FACE_OFFSET_X					5
#define		INS_CTRCT_OG_FACE_OFFSET_Y					4


#define		INS_CTRCT_OG_NICK_NAME_OFFSET_X			57
#define		INS_CTRCT_OG_NICK_NAME_OFFSET_Y			13

#define		INS_CTRCT_OG_HAS_CONTRACT_OFFSET_X	INS_CTRCT_OG_NICK_NAME_OFFSET_X
#define		INS_CTRCT_OG_HAS_CONTRACT_OFFSET_Y	INS_CTRCT_OG_NICK_NAME_OFFSET_Y + 13

#define		INS_CTRCT_TITLE_Y										(48 + LAPTOP_SCREEN_WEB_UL_Y)//52 + LAPTOP_SCREEN_WEB_UL_Y

#define		INS_CTRCT_FIRST_BULLET_TEXT_X				86 + LAPTOP_SCREEN_UL_X
#define		INS_CTRCT_FIRST_BULLET_TEXT_Y				65 + LAPTOP_SCREEN_WEB_UL_Y

#define		INS_CTRCT_SECOND_BULLET_TEXT_X			INS_CTRCT_FIRST_BULLET_TEXT_X
#define		INS_CTRCT_SECOND_BULLET_TEXT_Y			93 + LAPTOP_SCREEN_WEB_UL_Y

#define		INS_CTRCT_INTSRUCTION_TEXT_WIDTH		375

#define		INS_CTRCT_RED_BAR_UNDER_INSTRUCTION_TEXT_Y	123 + LAPTOP_SCREEN_WEB_UL_Y

#define		INS_CTRCT_EMPLYMNT_CNTRCT_TEXT_OFFSET_X			4
#define		INS_CTRCT_EMPLYMNT_CNTRCT_TEXT_OFFSET_Y			54

#define		INS_CTRCT_LENGTH_OFFSET_X										INS_CTRCT_EMPLYMNT_CNTRCT_TEXT_OFFSET_X
#define		INS_CTRCT_LENGTH_OFFSET_Y										71

#define		INS_CTRCT_DAYS_REMAINING_OFFSET_Y						87

#define		INS_CTRCT_INSURANCE_CNTRCT_OFFSET_Y					108

#define		INS_CTRCT_PREMIUM_OWING_OFFSET_Y						160


#define		INS_CTRCT_OG_BOX_OFFSET_X										92
#define		INS_CTRCT_OG_BOX_WIDTH											35

#define		INS_CTRCT_ACCEPT_BTN_X											( 132 / 2 - 43 / 2 ) //6
#define		INS_CTRCT_ACCEPT_BTN_Y											193

#define		INS_CTRCT_BOTTON_LINK_Y										351 + LAPTOP_SCREEN_WEB_UL_Y

#define		INS_CTRCT_BOTTOM_LINK_RED_BAR_X						171 + LAPTOP_SCREEN_UL_X
#define		INS_CTRCT_BOTTON_LINK_RED_BAR_Y						INS_CTRCT_BOTTON_LINK_Y + 41


#define		INS_CTRCT_BOTTOM_LINK_RED_BAR_OFFSET			117

#define		INS_CTRCT_BOTTOM_LINK_RED_WIDTH						97

#define		INS_CTRCT_CONTRACT_STATUS_TEXT_WIDTH			74

// this is the percentage of daily salary used as a base to calculate daily insurance premiums
#define		INSURANCE_PREMIUM_RATE										5

#define		INS_CTRCT_SKILL_BASE											42
#define		INS_CTRCT_FITNESS_BASE										85
#define		INS_CTRCT_EXP_LEVEL_BASE									3
#define		INS_CTRCT_SURVIVAL_BASE										90


UINT32	guiInsOrderGridImage;
UINT32	guiInsOrderBulletImage;

UINT8		gubNumberofDisplayedInsuranceGrids;

BOOLEAN	gfChangeInsuranceFormButtons = FALSE;

std::vector<UINT8> gInsuranceMercProfiles;
INT16		gsCurrentInsuranceMercIndex;


//link to the varios pages
MOUSE_REGION	gSelectedInsuranceContractLinkRegion[2];
void SelectInsuranceContractRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

INT32		guiInsContractPrevButtonImage;
void		BtnInsContractPrevButtonCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiInsContractPrevBackButton;

INT32		guiInsContractNextButtonImage;
void		BtnInsContractNextButtonCallBack(GUI_BUTTON *btn,INT32 reason);
UINT32	guiInsContractNextBackButton;


void		BtnInsuranceAcceptClearForm1ButtonCallback(GUI_BUTTON *btn,INT32 reason);

void		BtnInsuranceAcceptClearForm2ButtonCallback(GUI_BUTTON *btn,INT32 reason);

void		BtnInsuranceAcceptClearForm3ButtonCallback(GUI_BUTTON *btn,INT32 reason);

namespace
{
LaptopPageResourceOwner gInsuranceContractResources;
LaptopPageResourceOwner gInsuranceContractFormResources;
std::array<UINT8, kInsuranceContractsPerPage> gInsuranceMercForForm{
	NO_PROFILE, NO_PROFILE, NO_PROFILE};
std::array<INT32, kInsuranceContractsPerPage> gInsuranceFormButtons{
	-1, -1, -1};
}



//
//	Function Prototypes
//
BOOLEAN		DisplayOrderGrid( UINT8 ubGridNumber, UINT8 ubMercID );
void			DisableInsuranceContractNextPreviousbuttons();
BOOLEAN		AddInsuranceContractFormButtons(
	LaptopPageResourceOwner& owner, UINT8 formCount,
	std::array<INT32, kInsuranceContractsPerPage>& buttons);
void			HandleAcceptButton( SoldierID ubSoldierID );
FLOAT		DiffFromNormRatio( INT16 sThisValue, INT16 sNormalValue );
void			InsContractNoMercsPopupCallBack( UINT8 bExitValue );
void			BuildInsuranceArray();
void			NormalizeInsuranceContractPage();
BOOLEAN		MercIsInsurable( TacticalActor *pSoldier );
void			EnableDisableInsuranceContractAcceptButtons();
UINT32		GetTimeRemainingOnSoldiersContract( TacticalActor *pSoldier );
UINT32		GetTimeRemainingOnSoldiersInsuranceContract( TacticalActor *pSoldier );
void			EnableDisableIndividualInsuranceContractButton(
	UINT8 mercProfile, INT32 button);
BOOLEAN		CanSoldierExtendInsuranceContract( TacticalActor *pSoldier );
INT32		CalculateSoldiersInsuranceContractLength( TacticalActor *pSoldier );
INT32		CalcStartDayOfInsurance( TacticalActor *pSoldier );

BOOLEAN		AreAnyAimMercsOnTeam( );
void GameInitInsuranceContract()
{
	gsCurrentInsuranceMercIndex = 0;
}


BOOLEAN EnterInsuranceContract()
{
	VOBJECT_DESC	VObjectDesc;
	UINT16					usPosX,i;
	LaptopPageResourceOwner staged;
	LaptopPageResourceOwner stagedForms;
	std::array<INT32, kInsuranceContractsPerPage> stagedFormButtons{
		-1, -1, -1};

	//build the list of mercs that are can be displayed
	BuildInsuranceArray();
	NormalizeInsuranceContractPage();
	gfChangeInsuranceFormButtons = FALSE;
	gInsuranceContractFormResources.clear();
	gInsuranceContractResources.clear();
	if (!AddInsuranceDefaults(staged)) return FALSE;



	// load the Insurance title graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\InsOrderGrid.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiInsOrderGridImage))
		return FALSE;

	// load the Insurance bullet graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\bullet.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiInsOrderBulletImage))
		return FALSE;


	usPosX = INS_CTRCT_BOTTOM_LINK_RED_BAR_X;
	for(i=0; i<2; i++)
	{
		MSYS_DefineRegion( &gSelectedInsuranceContractLinkRegion[i], usPosX, INS_CTRCT_BOTTON_LINK_RED_BAR_Y-37, (UINT16)(usPosX + INS_CTRCT_BOTTOM_LINK_RED_WIDTH), INS_CTRCT_BOTTON_LINK_RED_BAR_Y+2, MSYS_PRIORITY_HIGH,
						CURSOR_WWW, MSYS_NO_CALLBACK, SelectInsuranceContractRegionCallBack);
		if (!staged.addRegion(gSelectedInsuranceContractLinkRegion[i]))
			return FALSE;
		MSYS_SetRegionUserData( &gSelectedInsuranceContractLinkRegion[i], 0, i );

		usPosX += INS_CTRCT_BOTTOM_LINK_RED_BAR_OFFSET;
	}


	//left arrow
	if (!staged.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\InsLeftButton.sti", 2, 0, -1, 1, -1),
		guiInsContractPrevButtonImage)) return FALSE;
	if (!staged.addButton(CreateIconAndTextButton( guiInsContractPrevButtonImage, InsContractText[INS_CONTRACT_PREVIOUS], INS_FONT_BIG,
												INS_FONT_COLOR, INS_FONT_SHADOW,
												INS_FONT_COLOR, INS_FONT_SHADOW,
												TEXT_CJUSTIFIED,
												INS_INFO_LEFT_ARROW_BUTTON_X, INS_INFO_LEFT_ARROW_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, BtnInsContractPrevButtonCallback),
		guiInsContractPrevBackButton)) return FALSE;
	SetButtonCursor( guiInsContractPrevBackButton, CURSOR_WWW );
	SpecifyButtonTextOffsets( guiInsContractPrevBackButton, 17, 16, FALSE );


	//Right arrow
	if (!staged.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\InsRightButton.sti", 2, 0, -1, 1, -1),
		guiInsContractNextButtonImage)) return FALSE;
	if (!staged.addButton(CreateIconAndTextButton( guiInsContractNextButtonImage, InsContractText[INS_CONTRACT_NEXT], INS_FONT_BIG,
												INS_FONT_COLOR, INS_FONT_SHADOW,
												INS_FONT_COLOR, INS_FONT_SHADOW,
												TEXT_CJUSTIFIED,
												INS_INFO_RIGHT_ARROW_BUTTON_X, INS_INFO_RIGHT_ARROW_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, BtnInsContractNextButtonCallBack),
		guiInsContractNextBackButton)) return FALSE;
	SetButtonCursor( guiInsContractNextBackButton, CURSOR_WWW );
	SpecifyButtonTextOffsets( guiInsContractNextBackButton, 18, 16, FALSE );

	//create the new set of buttons
	if (!AddInsuranceContractFormButtons(stagedForms,
			gubNumberofDisplayedInsuranceGrids, stagedFormButtons))
		return FALSE;
	gInsuranceContractResources = std::move(staged);
	gInsuranceContractFormResources = std::move(stagedForms);
	gInsuranceFormButtons = stagedFormButtons;

//	RenderInsuranceContract();
	return(TRUE);
}

void ExitInsuranceContract()
{
	gInsuranceContractFormResources.clear();
	gInsuranceContractResources.clear();
	gInsuranceFormButtons.fill(-1);
	gInsuranceMercForForm.fill(NO_PROFILE);
}



void HandleInsuranceContract()
{
	if (!gfChangeInsuranceFormButtons)
	{
		const std::vector<UINT8> previousProfiles =
			gInsuranceMercProfiles;
		BuildInsuranceArray();
		gfChangeInsuranceFormButtons =
			previousProfiles != gInsuranceMercProfiles;
	}
	if( gfChangeInsuranceFormButtons )
	{
		LaptopPageResourceOwner stagedForms;
		std::array<INT32, kInsuranceContractsPerPage>
			stagedFormButtons{-1, -1, -1};
		BuildInsuranceArray();
		NormalizeInsuranceContractPage();
		if (!AddInsuranceContractFormButtons(stagedForms,
				gubNumberofDisplayedInsuranceGrids,
				stagedFormButtons))
		{
			return;
		}
		gInsuranceContractFormResources.clear();
		gInsuranceContractFormResources = std::move(stagedForms);
		gInsuranceFormButtons = stagedFormButtons;
		gfChangeInsuranceFormButtons = FALSE;

		//force a redraw of the screen to erase the old buttons
		fPausedReDrawScreenFlag = TRUE;
		RenderInsuranceContract();

		MarkButtonsDirty();
	}

	EnableDisableInsuranceContractAcceptButtons();
}



void RenderInsuranceContract()
{
	HVOBJECT	hPixHandle;
	CHAR16		sText[800];
	UINT8			ubCount=0;
	UINT8			sMercID;
	std::size_t rosterIndex;
	UINT16		usPosX;
	TacticalActor *pSoldier = NULL;


	SetFontShadow( INS_FONT_SHADOW );

	DisplayInsuranceDefaults();

	//disable the next or previous button depending on how many more mercs we have to display
	DisableInsuranceContractNextPreviousbuttons();

	usPosX = INS_CTRCT_BOTTOM_LINK_RED_BAR_X;

	//Display the red bar under the link at the bottom.	and the text
	DisplaySmallColouredLineWithShadow( usPosX, INS_CTRCT_BOTTON_LINK_RED_BAR_Y, (UINT16)(usPosX + INS_CTRCT_BOTTOM_LINK_RED_WIDTH), INS_CTRCT_BOTTON_LINK_RED_BAR_Y );
	swprintf( sText, L"%s", pMessageStrings[ MSG_HOMEPAGE ] );
	DisplayWrappedString( usPosX, INS_CTRCT_BOTTON_LINK_Y+18, INS_CTRCT_BOTTOM_LINK_RED_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	usPosX += INS_CTRCT_BOTTOM_LINK_RED_BAR_OFFSET;

	//Display the red bar under the link at the bottom.	and the text
	DisplaySmallColouredLineWithShadow( usPosX, INS_CTRCT_BOTTON_LINK_RED_BAR_Y, (UINT16)(usPosX + INS_CTRCT_BOTTOM_LINK_RED_WIDTH), INS_CTRCT_BOTTON_LINK_RED_BAR_Y );
	GetInsuranceText( INS_SNGL_HOW_DOES_INS_WORK, sText );
	DisplayWrappedString( usPosX, INS_CTRCT_BOTTON_LINK_Y+12, INS_CTRCT_BOTTOM_LINK_RED_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);


	//Display the title slogan
	GetInsuranceText( INS_SNGL_ENTERING_REVIEWING_CLAIM, sText );
	DrawTextToScreen( sText, LAPTOP_SCREEN_UL_X, INS_CTRCT_TITLE_Y, LAPTOP_SCREEN_LR_X-(LAPTOP_SCREEN_UL_X)/*-iScreenWidthOffset*/, INS_FONT_BIG, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED );


	//Get and display the insurance bullet
	GetVideoObject(&hPixHandle, guiInsOrderBulletImage );
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, INS_CTRCT_FIRST_BULLET_TEXT_X, INS_CTRCT_FIRST_BULLET_TEXT_Y, VO_BLT_SRCTRANSPARENCY,NULL);

	//Display the first instruction sentence
	GetInsuranceText( INS_MLTI_TO_PURCHASE_INSURANCE, sText );
	DisplayWrappedString( INS_CTRCT_FIRST_BULLET_TEXT_X+INSURANCE_BULLET_TEXT_OFFSET_X, INS_CTRCT_FIRST_BULLET_TEXT_Y, INS_CTRCT_INTSRUCTION_TEXT_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);


	//Get and display the insurance bullet
	GetVideoObject(&hPixHandle, guiInsOrderBulletImage );
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, INS_CTRCT_FIRST_BULLET_TEXT_X, INS_CTRCT_SECOND_BULLET_TEXT_Y, VO_BLT_SRCTRANSPARENCY,NULL);

	//Display the second instruction sentence
	GetInsuranceText( INS_MLTI_ONCE_SATISFIED_CLICK_ACCEPT, sText );
	DisplayWrappedString( INS_CTRCT_FIRST_BULLET_TEXT_X+INSURANCE_BULLET_TEXT_OFFSET_X, INS_CTRCT_SECOND_BULLET_TEXT_Y, INS_CTRCT_INTSRUCTION_TEXT_WIDTH, 2, INS_FONT_MED, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	//Display the red bar under the instruction text
	DisplaySmallColouredLineWithShadow( INS_CTRCT_FIRST_BULLET_TEXT_X, INS_CTRCT_RED_BAR_UNDER_INSTRUCTION_TEXT_Y, INS_CTRCT_FIRST_BULLET_TEXT_X + INS_CTRCT_INTSRUCTION_TEXT_WIDTH, INS_CTRCT_RED_BAR_UNDER_INSTRUCTION_TEXT_Y );


	gInsuranceMercForForm.fill(NO_PROFILE);
	rosterIndex = static_cast<std::size_t>(std::max<INT16>(
		gsCurrentInsuranceMercIndex, 0));
	while (ubCount < gubNumberofDisplayedInsuranceGrids &&
		rosterIndex < gInsuranceMercProfiles.size())
	{
		sMercID = gInsuranceMercProfiles[rosterIndex];

		SoldierID ID = GetSoldierIDFromMercID( sMercID );
		if ( ID != NOBODY )
		{
			TacticalActor* soldier =
				GetJa2SoldierRepository().resolve(ID.i);
			if (MercIsInsurable(soldier))
			{
				if (DisplayOrderGrid(ubCount, sMercID)) ubCount++;
			}
		}

		rosterIndex++;
	}

	//if there are no valid mercs to insure
	if( ubCount == 0 )
	{
		//if there where AIM mercs ( on short contract )
		if( AreAnyAimMercsOnTeam( ) )
		{
			//Display Error Message, all aim mercs are on short contract
			GetInsuranceText( INS_MLTI_ALL_AIM_MERCS_ON_SHORT_CONTRACT, sText );
			DoLapTopMessageBox( MSG_BOX_RED_ON_WHITE, sText, LAPTOP_SCREEN, MSG_BOX_FLAG_OK, InsContractNoMercsPopupCallBack);
		}
		else
		{
			//Display Error Message, no valid mercs
			GetInsuranceText( INS_MLTI_NO_QUALIFIED_MERCS, sText );
			DoLapTopMessageBox( MSG_BOX_RED_ON_WHITE, sText, LAPTOP_SCREEN, MSG_BOX_FLAG_OK, InsContractNoMercsPopupCallBack);
		}
	}



	SetFontShadow(DEFAULT_SHADOW);
	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}





void BtnInsContractPrevButtonCallback(GUI_BUTTON *btn,INT32 reason)
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

			gsCurrentInsuranceMercIndex = static_cast<INT16>(
				PreviousInsuranceContractPageStart(
					static_cast<std::size_t>(std::max<INT16>(
						gsCurrentInsuranceMercIndex, 0)),
					gInsuranceMercProfiles.size()));

			//signal that we want to change the number of forms on the page
			gfChangeInsuranceFormButtons = TRUE;

			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}



void BtnInsContractNextButtonCallBack(GUI_BUTTON *btn,INT32 reason)
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

			gsCurrentInsuranceMercIndex = static_cast<INT16>(
				NextInsuranceContractPageStart(
					static_cast<std::size_t>(std::max<INT16>(
						gsCurrentInsuranceMercIndex, 0)),
					gInsuranceMercProfiles.size()));

			//signal that we want to change the number of forms on the page
			gfChangeInsuranceFormButtons = TRUE;

			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}




BOOLEAN DisplayOrderGrid( UINT8 ubGridNumber, UINT8 ubMercID )
{
	VOBJECT_DESC		VObjectDesc;
	HVOBJECT			hPixHandle;
	UINT16			usPosX;
	INT32			iCostOfContract=0;
	char				sTemp[100];
	CHAR16			sText[800];
	BOOLEAN			fDisplayMercContractStateTextColorInRed = FALSE;
	if (ubMercID >= NUM_PROFILES ||
		ubGridNumber >= gInsuranceMercForForm.size())
	{
		return(FALSE);
	}
	SoldierID usID = GetSoldierIDFromMercID(ubMercID);
	if (usID == NOBODY) return FALSE;

	TacticalActor	*pSoldier =
		GetJa2SoldierRepository().resolve(usID.i);
	if ( !pSoldier )
		return(FALSE);
	switch( ubGridNumber )
	{
		case 0:
			usPosX = INS_CTRCT_ORDER_GRID1_X;
			break;

		case 1:
			usPosX = INS_CTRCT_ORDER_GRID2_X;
			break;

		case 2:
			usPosX = INS_CTRCT_ORDER_GRID3_X;
			break;

		default:
			//should never get in here
			return FALSE;
	}
	//Get and display the insurance order grid #1
	GetVideoObject(&hPixHandle, guiInsOrderGridImage );
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, usPosX, INS_CTRCT_ORDER_GRID1_Y, VO_BLT_SRCTRANSPARENCY,NULL);


	// load the mercs face graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	snprintf(sTemp, sizeof(sTemp), "FACES\\%02d.sti",
		gMercProfiles[pSoldier->identity().profile()].ubFaceIndex );
	FilenameForBPP( sTemp, VObjectDesc.ImageFile);
	UniqueVideoObjectHandle faceImage = AddVideoObjectOwned(&VObjectDesc);
	if (!faceImage) return FALSE;
	const UINT32 uiInsMercFaceImage = faceImage.get();
	gInsuranceMercForForm[ubGridNumber] = ubMercID;

	//Get the merc's face
	GetVideoObject(&hPixHandle, uiInsMercFaceImage );

	//if the merc is dead, shade the face red
	if( IsMercDead( pSoldier->identity().profile() ) )
	{
		//if the merc is dead
		//shade the face red, (to signify that he is dead)
		hPixHandle->pShades[ 0 ]		= Create16BPPPaletteShaded( hPixHandle->pPaletteEntry, DEAD_MERC_COLOR_RED, DEAD_MERC_COLOR_GREEN, DEAD_MERC_COLOR_BLUE, TRUE );

		//set the red pallete to the face
		SetObjectHandleShade( uiInsMercFaceImage, 0 );
	}

	//Get and display the mercs face
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, usPosX+INS_CTRCT_OG_FACE_OFFSET_X, INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_OG_FACE_OFFSET_Y, VO_BLT_SRCTRANSPARENCY,NULL);

	//display the mercs nickname
	DrawTextToScreen(gMercProfiles[ ubMercID ].zNickname, (UINT16)(usPosX + INS_CTRCT_OG_NICK_NAME_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y + INS_CTRCT_OG_NICK_NAME_OFFSET_Y, 0, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED	);

	//Get the text to display the mercs current insurance contract status
	if( IsMercDead( pSoldier->identity().profile() ) )
	{
		//if the merc has a contract
		if( pSoldier->employment().lifeInsurance() )
		{
			//Display the contract text
			GetInsuranceText( INS_SNGL_DEAD_WITH_CONTRACT, sText );
		}
		else
		{
			//Display the contract text
			GetInsuranceText( INS_SNGL_DEAD_NO_CONTRACT, sText );
		}
		DisplayWrappedString( (UINT16)(usPosX+INS_CTRCT_OG_HAS_CONTRACT_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_OG_HAS_CONTRACT_OFFSET_Y, INS_CTRCT_CONTRACT_STATUS_TEXT_WIDTH, 2, INS_FONT_SMALL, INS_FONT_COLOR_RED,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}
	else
	{
		//if the merc has a contract
		if( pSoldier->employment().lifeInsurance() )
		{
			//if the soldier can extend their insurance
			if( CanSoldierExtendInsuranceContract( pSoldier ) )
			{
				//Display the contract text
				GetInsuranceText( INS_SNGL_PARTIALLY_INSURED, sText );
				fDisplayMercContractStateTextColorInRed = TRUE;
			}
			else
			{
				//Display the contract text
				GetInsuranceText( INS_SNGL_CONTRACT, sText );
				fDisplayMercContractStateTextColorInRed = FALSE;
			}
		}
		else
		{
			//Display the contract text
			GetInsuranceText( INS_SNGL_NOCONTRACT, sText );
			fDisplayMercContractStateTextColorInRed = TRUE;
		}
		if( fDisplayMercContractStateTextColorInRed )
			DisplayWrappedString( (UINT16)(usPosX+INS_CTRCT_OG_HAS_CONTRACT_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_OG_HAS_CONTRACT_OFFSET_Y, INS_CTRCT_CONTRACT_STATUS_TEXT_WIDTH, 2, INS_FONT_SMALL, INS_FONT_COLOR_RED,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
		else
			DisplayWrappedString( (UINT16)(usPosX+INS_CTRCT_OG_HAS_CONTRACT_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_OG_HAS_CONTRACT_OFFSET_Y, INS_CTRCT_CONTRACT_STATUS_TEXT_WIDTH, 2, INS_FONT_SMALL, INS_FONT_COLOR,	sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}



	//Display the Emplyment contract text
	GetInsuranceText( INS_SNGL_EMPLOYMENT_CONTRACT, sText );
	DrawTextToScreen( sText, (UINT16)(usPosX+INS_CTRCT_EMPLYMNT_CNTRCT_TEXT_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_EMPLYMNT_CNTRCT_TEXT_OFFSET_Y, INS_CTRCT_ORDER_GRID_WIDTH, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED );

	//Display the merc contract Length text
	GetInsuranceText( INS_SNGL_LENGTH, sText );
	DrawTextToScreen( sText, (UINT16)(usPosX+INS_CTRCT_LENGTH_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_LENGTH_OFFSET_Y, 0, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED );

	//Display the mercs contract length
	swprintf( sText, L"%d", pSoldier->employment().totalLength() );
	DrawTextToScreen( sText, (UINT16)(usPosX+INS_CTRCT_OG_BOX_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_LENGTH_OFFSET_Y, INS_CTRCT_OG_BOX_WIDTH, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED );


	//Display the days remaining for the emplyment contract text
	GetInsuranceText( INS_SNGL_DAYS_REMAINING, sText );
	DrawTextToScreen( sText, (UINT16)(usPosX+INS_CTRCT_LENGTH_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_DAYS_REMAINING_OFFSET_Y, 0, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED );


	//display the amount of time the merc has left on their Regular contract
	if( IsMercDead( pSoldier->identity().profile() ) )
		swprintf( sText, L"%s", pMessageStrings[ MSG_LOWERCASE_NA ] );
	else
		swprintf( sText, L"%d", GetTimeRemainingOnSoldiersContract( pSoldier ) );

	DrawTextToScreen( sText, (UINT16)(usPosX+INS_CTRCT_OG_BOX_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_DAYS_REMAINING_OFFSET_Y, INS_CTRCT_OG_BOX_WIDTH, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED );

	//Display the Insurqance contract
	GetInsuranceText( INS_SNGL_INSURANCE_CONTRACT, sText );
	DrawTextToScreen( sText, (UINT16)(usPosX+INS_CTRCT_EMPLYMNT_CNTRCT_TEXT_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_INSURANCE_CNTRCT_OFFSET_Y, INS_CTRCT_ORDER_GRID_WIDTH, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED );


	GetInsuranceText( INS_SNGL_LENGTH, sText );
	DrawTextToScreen( sText, (UINT16)(usPosX+INS_CTRCT_LENGTH_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_LENGTH_OFFSET_Y+54, 0, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED );

	//Display the insurance days remaining text
	GetInsuranceText( INS_SNGL_DAYS_REMAINING, sText );
	DrawTextToScreen( sText, (UINT16)(usPosX+INS_CTRCT_LENGTH_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_DAYS_REMAINING_OFFSET_Y+54, 0, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED );



	//
	//display the amount of time the merc has left on the insurance contract
	//

	//if the soldier has insurance, disply the length of time the merc has left
	if( IsMercDead( pSoldier->identity().profile() ) )
		swprintf( sText, L"%s", pMessageStrings[ MSG_LOWERCASE_NA ] );

	else if( pSoldier->employment().lifeInsurance() != 0 )
		swprintf( sText, L"%d", GetTimeRemainingOnSoldiersInsuranceContract( pSoldier ) );

	else
		swprintf( sText, L"%d", 0 );

	DrawTextToScreen( sText, (UINT16)(usPosX+INS_CTRCT_OG_BOX_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_DAYS_REMAINING_OFFSET_Y+54, INS_CTRCT_OG_BOX_WIDTH, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED );


	//
	// Calculate the insurance cost
	//

	//if the soldier can get insurance, calculate a new cost
	if( CanSoldierExtendInsuranceContract( pSoldier ) )
	{
		iCostOfContract =CalculateInsuranceContractCost( CalculateSoldiersInsuranceContractLength( pSoldier ), pSoldier->identity().profile() );
	}

	else
	{
		iCostOfContract = 0;
	}

	std::wstring amountRefund{};
	if( iCostOfContract < 0 )
	{
		//shouldnt get in here now since we can longer give refunds
		Assert( 0 );
	}
	else
	{
		//Display the premium owing text
		GetInsuranceText( INS_SNGL_PREMIUM_OWING, sText );
		DrawTextToScreen( sText, (UINT16)(usPosX+INS_CTRCT_EMPLYMNT_CNTRCT_TEXT_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_PREMIUM_OWING_OFFSET_Y, INS_CTRCT_ORDER_GRID_WIDTH, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED );

		//display the amount of refund
		amountRefund = FormatMoney(iCostOfContract);
	}


	if( IsMercDead( ubMercID ) )
	{
		amountRefund = L"$0";
	}
	//display the amount owing
	DrawTextToScreen( amountRefund.data(), (UINT16)(usPosX + 32), INS_CTRCT_ORDER_GRID1_Y + 179, 72, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);



	//
	// Get the insurance contract length for the merc
	//
	swprintf( sText, L"%d", CalculateSoldiersInsuranceContractLength( pSoldier ) );


	//Display the length of time the player can get for the insurance contract
	DrawTextToScreen( sText, (UINT16)(usPosX+INS_CTRCT_OG_BOX_OFFSET_X), INS_CTRCT_ORDER_GRID1_Y+INS_CTRCT_LENGTH_OFFSET_Y+52+2, INS_CTRCT_OG_BOX_WIDTH, INS_FONT_MED, INS_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED );

	return( TRUE );
}



void BtnInsuranceAcceptClearForm1ButtonCallback(GUI_BUTTON *btn,INT32 reason)
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
			UINT8		ubButton = (UINT8) MSYS_GetBtnUserData( btn, 0 );
			const UINT8 mercProfile = gInsuranceMercForForm[0];
			SoldierID ubSoldierID = NOBODY;
			if (IsInsuranceMercProfile(mercProfile, NUM_PROFILES))
				ubSoldierID = GetSoldierIDFromMercID(mercProfile);

			btn->uiFlags &= (~BUTTON_CLICKED_ON );

			//the accept button
			if( ubButton == 0 && ubSoldierID != NOBODY )
			{
				HandleAcceptButton( ubSoldierID );
			}

			//redraw the screen
			fPausedReDrawScreenFlag = TRUE;

			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}



void BtnInsuranceAcceptClearForm2ButtonCallback(GUI_BUTTON *btn,INT32 reason)
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
			UINT8		ubButton = (UINT8) MSYS_GetBtnUserData( btn, 0 );
			const UINT8 mercProfile = gInsuranceMercForForm[1];
			SoldierID ubSoldierID = NOBODY;
			if (IsInsuranceMercProfile(mercProfile, NUM_PROFILES))
				ubSoldierID = GetSoldierIDFromMercID(mercProfile);

			btn->uiFlags &= (~BUTTON_CLICKED_ON );

			//the accept button
			if( ubButton == 0 && ubSoldierID != NOBODY )
			{
				HandleAcceptButton( ubSoldierID );
			}

			//redraw the screen
			fPausedReDrawScreenFlag = TRUE;

			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}




void BtnInsuranceAcceptClearForm3ButtonCallback(GUI_BUTTON *btn,INT32 reason)
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
			UINT8		ubButton = (UINT8) MSYS_GetBtnUserData( btn, 0 );
			const UINT8 mercProfile = gInsuranceMercForForm[2];
			SoldierID ubSoldierID = NOBODY;
			if (IsInsuranceMercProfile(mercProfile, NUM_PROFILES))
				ubSoldierID = GetSoldierIDFromMercID(mercProfile);

			btn->uiFlags &= (~BUTTON_CLICKED_ON );

			//the accept button
			if( ubButton == 0 && ubSoldierID != NOBODY )
			{
				HandleAcceptButton( ubSoldierID );
			}

			//redraw the screen
			fPausedReDrawScreenFlag = TRUE;

			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}



void SelectInsuranceContractRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		UINT32 uiInsuranceLink = MSYS_GetRegionUserData( pRegion, 0 );

		if( uiInsuranceLink == 0 )
			guiCurrentLaptopMode = LAPTOP_MODE_INSURANCE;
		else if( uiInsuranceLink == 1 )
			guiCurrentLaptopMode = LAPTOP_MODE_INSURANCE_INFO;
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
	}
}

void DisableInsuranceContractNextPreviousbuttons()
{
	//disable the next button if there is no more mercs to display
	const std::size_t current = static_cast<std::size_t>(
		std::max<INT16>(gsCurrentInsuranceMercIndex, 0));
	if (NextInsuranceContractPageStart(current,
			gInsuranceMercProfiles.size()) != current)
	{
		EnableButton( guiInsContractNextBackButton );
	}
	else
		DisableButton( guiInsContractNextBackButton );

	//if we are currently displaying the first set of mercs, disable the previous button
	if (current == 0)
	{
		DisableButton( guiInsContractPrevBackButton );
	}
	else
		EnableButton( guiInsContractPrevBackButton );

}

BOOLEAN AddInsuranceContractFormButtons(
	LaptopPageResourceOwner& owner, UINT8 formCount,
	std::array<INT32, kInsuranceContractsPerPage>& buttons)
{
	buttons.fill(-1);
	formCount = static_cast<UINT8>(std::min<std::size_t>(
		formCount, kInsuranceContractsPerPage));
	if (formCount == 0) return TRUE;

	INT32 image = -1;
	if (!owner.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\AcceptClearBox.sti", -1, 0, -1, 1, -1), image))
		return FALSE;

	const std::array<INT16, kInsuranceContractsPerPage> positions{
		static_cast<INT16>(INS_CTRCT_ORDER_GRID1_X +
			INS_CTRCT_ACCEPT_BTN_X),
		static_cast<INT16>(INS_CTRCT_ORDER_GRID2_X +
			INS_CTRCT_ACCEPT_BTN_X),
		static_cast<INT16>(INS_CTRCT_ORDER_GRID3_X +
			INS_CTRCT_ACCEPT_BTN_X)};
	const std::array<GUI_CALLBACK, kInsuranceContractsPerPage> callbacks{
		BtnInsuranceAcceptClearForm1ButtonCallback,
		BtnInsuranceAcceptClearForm2ButtonCallback,
		BtnInsuranceAcceptClearForm3ButtonCallback};
	for (UINT8 i = 0; i < formCount; ++i)
	{
		if (!owner.addButton(CreateIconAndTextButton(image,
				InsContractText[INS_CONTRACT_ACCEPT], INS_FONT_MED,
				INS_FONT_BTN_COLOR, INS_FONT_BTN_SHADOW_COLOR,
				INS_FONT_BTN_COLOR, INS_FONT_BTN_SHADOW_COLOR,
				TEXT_CJUSTIFIED, positions[i],
				INS_CTRCT_ORDER_GRID1_Y + INS_CTRCT_ACCEPT_BTN_Y,
				BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
				DEFAULT_MOVE_CALLBACK, callbacks[i]), buttons[i]))
			return FALSE;
		SetButtonCursor(buttons[i], CURSOR_LAPTOP_SCREEN);
		MSYS_SetBtnUserData(buttons[i], 0, 0);
	}
	return TRUE;
}



void HandleAcceptButton( SoldierID ubSoldierID )
{
	TacticalActor* soldier =
		GetJa2SoldierRepository().resolve(ubSoldierID.i);
	if (soldier && MercIsInsurable(soldier))
		PurchaseOrExtendInsuranceForSoldier(
			soldier,
			CalculateSoldiersInsuranceContractLength( soldier ) );

	RenderInsuranceContract();
}









// determines if a merc will run out of their insurance contract
void DailyUpdateOfInsuredMercs()
{
	SoldierID Soldier = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	SoldierID bLastTeamID = gTacticalStatus.Team[gbPlayerNum].bLastID;

	for ( ; Soldier <= bLastTeamID; ++Soldier)
	{
		TacticalActor* soldier =
			GetJa2SoldierRepository().resolve(Soldier.i);
		//if the soldier is in the team array
		if( soldier && soldier->roster().active() )
		{
			//if the merc has life insurance
			if( soldier->employment().lifeInsurance() )
			{
				//if the merc wasn't just hired
				if( (INT16)GetWorldDay() !=
					soldier->employment().insuranceStartDay() )
				{
					//if the contract has run out of time
					if( GetTimeRemainingOnSoldiersInsuranceContract(
						soldier ) <= 0 )
					{
						//if the soldier isn't dead
						if( !IsMercDead( soldier->identity().profile() ) )
						{
							soldier->employment().lifeInsurance() = 0;
							soldier->employment().insuranceLengthDays() = 0;
							soldier->employment().insuranceStartDay() = 0;
						}
					}
				}
			}
		}
	}
}


#define MIN_INSURANCE_RATIO		0.1f
#define MAX_INSURANCE_RATIO		10.0f


INT32	CalculateInsuranceContractCost( INT32 iLength, UINT8 ubMercID )
{
	MERCPROFILESTRUCT * pProfile;
	INT16	sTotalSkill=0;
	FLOAT flSkillFactor, flFitnessFactor, flExpFactor, flSurvivalFactor;
	FLOAT flRiskFactor;
	std::uint64_t uiDailyInsurancePremium;
	std::uint64_t uiTotalInsurancePremium;
	TacticalActor	*pSoldier;


	if (iLength <= 0 || ubMercID >= NUM_PROFILES) return 0;
	const SoldierID soldierID = GetSoldierIDFromMercID( ubMercID );
	pSoldier = GetJa2SoldierRepository().resolve(soldierID.i);
	if ( !pSoldier )
		return( 0 );


	// only mercs with at least 2 days to go on their employment contract are insurable
	// def: 2/5/99.	However if they already have insurance is SHOULD be ok
	if( GetTimeRemainingOnSoldiersContract( pSoldier ) < 2 && !( pSoldier->employment().lifeInsurance() != 0 && GetTimeRemainingOnSoldiersContract( pSoldier ) >= 1 ) )
	{
		return( 0 );
	}

	//If the merc is currently being held captive, get out
	if (pSoldier->assignment().current() == ASSIGNMENT_POW)
	{
		return( 0 );
	}

/*
	replaced with the above check

	if (iLength < 2)
	{
		return(0);
	}
	*/

	pProfile = &gMercProfiles[ ubMercID ];

	// calculate the degree of training
	sTotalSkill = (pProfile->bMarksmanship + pProfile->bMedical + pProfile->bMechanical + pProfile->bExplosive + pProfile->bLeadership) / 5;
	flSkillFactor = DiffFromNormRatio( sTotalSkill, INS_CTRCT_SKILL_BASE );

	// calc relative fitness level
	flFitnessFactor = DiffFromNormRatio( pProfile->bLife, INS_CTRCT_FITNESS_BASE );

	// calc relative experience
	flExpFactor = DiffFromNormRatio( pProfile->bExpLevel, INS_CTRCT_EXP_LEVEL_BASE );

	// calc player's survival rate (death rate subtracted from 100)
	flSurvivalFactor = DiffFromNormRatio( (INT16) (100 - CalcDeathRate()), INS_CTRCT_SURVIVAL_BASE );

	// calculate the overall insurability risk factor for this merc by combining all the subfactors
	flRiskFactor = flSkillFactor * flFitnessFactor * flExpFactor * flSurvivalFactor;

	// Flugente: backgrounds
	flRiskFactor = flRiskFactor * (100 + TacticalActorModifiers::backgroundValue(*pSoldier, BG_PERC_INSURANCE)) / 100;

	// restrict the overall factor to within reasonable limits
	if (flRiskFactor < MIN_INSURANCE_RATIO)
	{
		flRiskFactor = MIN_INSURANCE_RATIO;
	}
	else
	if (flRiskFactor > MAX_INSURANCE_RATIO)
	{
		flRiskFactor = MAX_INSURANCE_RATIO;
	}

	// premium depend on merc's salary, the base insurance rate, and the individual's risk factor at this time
	uiDailyInsurancePremium = static_cast<std::uint64_t>(
		pProfile->sSalary * INSURANCE_PREMIUM_RATE * flRiskFactor /
		100.0f + 0.5f);
	// multiply by the insurance contract length
	uiTotalInsurancePremium = uiDailyInsurancePremium *
		static_cast<std::uint64_t>(iLength);

	return static_cast<INT32>(std::min<std::uint64_t>(
		uiTotalInsurancePremium,
		static_cast<std::uint64_t>(std::numeric_limits<INT32>::max())));
}


// values passed in must be such that exceeding the normal value REDUCES insurance premiums
FLOAT DiffFromNormRatio( INT16 sThisValue, INT16 sNormalValue )
{
	FLOAT flRatio;

	if (sThisValue > 0)
	{
		flRatio = (FLOAT) sNormalValue / sThisValue;

		// restrict each ratio to within a reasonable range
		if (flRatio < MIN_INSURANCE_RATIO)
		{
			flRatio = MIN_INSURANCE_RATIO;
		}
		else
		if (flRatio > MAX_INSURANCE_RATIO)
		{
			flRatio = MAX_INSURANCE_RATIO;
		}
	}
	else
	{
		// use maximum allowable ratio
		flRatio = MAX_INSURANCE_RATIO;
	}

	return( flRatio );
}


void InsContractNoMercsPopupCallBack( UINT8 bExitValue )
{
	// yes, so start over, else stay here and do nothing for now
	if( bExitValue == MSG_BOX_RETURN_OK )
	{
		guiCurrentLaptopMode = LAPTOP_MODE_INSURANCE;
	}

	return;
}

void BuildInsuranceArray()
{
	SoldierID Soldier = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	SoldierID bLastTeamID = gTacticalStatus.Team[gbPlayerNum].bLastID;
	std::vector<UINT8> insurableProfiles;
	if (bLastTeamID >= Soldier)
		insurableProfiles.reserve(static_cast<std::size_t>(
			bLastTeamID.i - Soldier.i + 1));

	// store profile #s of all insurable mercs in an array
	for ( ; Soldier <= bLastTeamID; ++Soldier)
	{
		TacticalActor* soldier =
			GetJa2SoldierRepository().resolve(Soldier.i);
		if( MercIsInsurable(soldier) )
		{
			insurableProfiles.push_back(soldier->identity().profile());
		}
	}
	gInsuranceMercProfiles = std::move(insurableProfiles);
}

void NormalizeInsuranceContractPage()
{
	const std::size_t requested = static_cast<std::size_t>(
		std::max<INT16>(gsCurrentInsuranceMercIndex, 0));
	const std::size_t start = InsuranceContractPageStart(
		requested, gInsuranceMercProfiles.size());
	gsCurrentInsuranceMercIndex = static_cast<INT16>(start);
	gubNumberofDisplayedInsuranceGrids = static_cast<UINT8>(
		InsuranceContractPageSize(start,
			gInsuranceMercProfiles.size()));
}


BOOLEAN AddLifeInsurancePayout( TacticalActor *pSoldier )
{
	UINT8	ubPayoutID;
	UINT32 uiTimeInMinutes;
	MERCPROFILESTRUCT *pProfile;
	UINT32 uiCostPerDay;
	UINT32 uiDaysToPay;


	if (!pSoldier || pSoldier->identity().profile() == NO_PROFILE ||
		pSoldier->identity().profile() >= NUM_PROFILES)
	{
		Assert(FALSE);
		return FALSE;
	}

	pProfile = &(gMercProfiles[ pSoldier->identity().profile() ]);
	if (!InsurancePayoutStorageIsConsistent(
			LaptopSaveInfo.ubNumberLifeInsurancePayouts,
			LaptopSaveInfo.ubNumberLifeInsurancePayoutUsed,
			LaptopSaveInfo.pLifeInsurancePayouts != nullptr))
	{
		Assert(FALSE);
		return FALSE;
	}

	//if we need to add more array elements
	if( LaptopSaveInfo.ubNumberLifeInsurancePayouts <= LaptopSaveInfo.ubNumberLifeInsurancePayoutUsed )
	{
		if (LaptopSaveInfo.ubNumberLifeInsurancePayouts ==
			std::numeric_limits<UINT8>::max())
		{
			Assert(FALSE);
			return FALSE;
		}
		const UINT8 newPayoutCount =
			LaptopSaveInfo.ubNumberLifeInsurancePayouts + 1;
		auto* resizedPayouts = static_cast<LIFE_INSURANCE_PAYOUT*>(MemRealloc(
			LaptopSaveInfo.pLifeInsurancePayouts,
			sizeof(LIFE_INSURANCE_PAYOUT) * newPayoutCount));
		if (!resizedPayouts)
			return( FALSE );
		LaptopSaveInfo.pLifeInsurancePayouts = resizedPayouts;
		LaptopSaveInfo.ubNumberLifeInsurancePayouts = newPayoutCount;

		memset( &LaptopSaveInfo.pLifeInsurancePayouts[ LaptopSaveInfo.ubNumberLifeInsurancePayouts - 1 ], 0, sizeof( LIFE_INSURANCE_PAYOUT ) );
	}

	for( ubPayoutID = 0; ubPayoutID < LaptopSaveInfo.ubNumberLifeInsurancePayouts; ubPayoutID++ )
	{
		//get an empty element in the array
		if( !LaptopSaveInfo.pLifeInsurancePayouts[ ubPayoutID ].fActive )
			break;
	}
	if (ubPayoutID >= LaptopSaveInfo.ubNumberLifeInsurancePayouts)
	{
		Assert(FALSE);
		return FALSE;
	}

	// This uses the merc's latest salaries, ignoring that they may be higher than the salaries paid under the current
	// contract if the guy has recently gained a level.	We could store his daily salary when he was last contracted,
	// and use that, but it still doesn't easily account for the fact that renewing a leveled merc early means that the
	// first part of his contract is under his old salary and the second part is under his new one.	Therefore, I chose
	// to ignore this wrinkle, and let the player awlays get paid out using the higher amount.	ARM

	// figure out which of the 3 salary rates the merc has is the cheapest, and use it to calculate the paid amount, to
	// avoid getting back more than the merc cost if he was on a 2-week contract!

	// start with the daily salary
	uiCostPerDay = pProfile->sSalary;

	// consider weekly salary / day
	if ((pProfile->uiWeeklySalary / 7) < uiCostPerDay)
	{
		uiCostPerDay = (pProfile->uiWeeklySalary / 7);
	}

	// consider biweekly salary / day
	if ((pProfile->uiBiWeeklySalary / 14) < uiCostPerDay)
	{
		uiCostPerDay = (pProfile->uiBiWeeklySalary / 14);
	}

	// calculate how many full, insured days of work the merc is going to miss
	uiDaysToPay = RemainingLaptopDays(
		pSoldier->employment().insuranceLengthDays(),
		static_cast<INT32>(GetWorldDay()) + 1 -
			pSoldier->employment().insuranceStartDay());

	const std::uint64_t payoutPrice =
		static_cast<std::uint64_t>(uiDaysToPay) * uiCostPerDay;
	if (payoutPrice > static_cast<std::uint64_t>(
			std::numeric_limits<INT32>::max()))
	{
		Assert(FALSE);
		return FALSE;
	}

	LIFE_INSURANCE_PAYOUT& payout =
		LaptopSaveInfo.pLifeInsurancePayouts[ubPayoutID];
	payout.ubSoldierID = pSoldier->identity().id();
	payout.ubMercID = pSoldier->identity().profile();
	payout.iPayOutPrice = static_cast<INT32>(payoutPrice);
	payout.fActive = TRUE;

	// 4pm next day
	uiTimeInMinutes = GetMidnightOfFutureDayInMinutes( 1 ) + 16 * 60;

	// if the death was suspicious, or he's already been investigated twice or more
	if (pProfile->ubSuspiciousDeath || (gStrategicStatus.ubInsuranceInvestigationsCnt >= 2))
	{
		// fraud suspected, claim will be investigated first
		AddStrategicEvent( EVENT_INSURANCE_INVESTIGATION_STARTED, uiTimeInMinutes, ubPayoutID );
	}
	else
	{
		// is ok, make a prompt payment
		AddStrategicEvent( EVENT_PAY_LIFE_INSURANCE_FOR_DEAD_MERC, uiTimeInMinutes, ubPayoutID );
	}

	LaptopSaveInfo.ubNumberLifeInsurancePayoutUsed++;

	return( TRUE );
}


namespace
{
using CommunicationsPolicy = CampaignLaptopCommunicationsPolicy;

static_assert(static_cast<UINT8>(
	CommunicationsPolicy::Substitution::InsuranceVerySuspiciousFraud) ==
	TYPE_E_INSURANCE_L2);
static_assert(static_cast<UINT8>(
	CommunicationsPolicy::Substitution::InsurancePayment) ==
	TYPE_E_INSURANCE_L3);
static_assert(static_cast<UINT8>(
	CommunicationsPolicy::Substitution::InsuranceFirstInvestigation) ==
	TYPE_E_INSURANCE_L4);
static_assert(static_cast<UINT8>(
	CommunicationsPolicy::Substitution::InsuranceRepeatInvestigation) ==
	TYPE_E_INSURANCE_L5);
static_assert(static_cast<UINT8>(
	CommunicationsPolicy::Substitution::InsuranceInvestigationComplete) ==
	TYPE_E_INSURANCE_L6);

bool IsValidInsurancePayout(UINT16 payoutId)
{
	return InsurancePayoutStorageIsConsistent(
			LaptopSaveInfo.ubNumberLifeInsurancePayouts,
			LaptopSaveInfo.ubNumberLifeInsurancePayoutUsed,
			LaptopSaveInfo.pLifeInsurancePayouts != nullptr) &&
		IsValidLaptopIndex(
			LaptopSaveInfo.ubNumberLifeInsurancePayouts, payoutId) &&
		LaptopSaveInfo.pLifeInsurancePayouts[payoutId].fActive &&
		LaptopSaveInfo.pLifeInsurancePayouts[payoutId].ubMercID < NUM_PROFILES;
}

void SendInsuranceNotice(
	CommunicationsPolicy::InsuranceNotice notice,
	INT32 payoutPrice,
	UINT8 mercId,
	UINT16 xmlEmail)
{
	const CommunicationsPolicy policy(GetGameContext().capabilities());
	if (!policy.insuranceAvailable(
			gubQuest[QUEST_FIX_LAPTOP] == QUESTDONE,
			gGameUBOptions.LaptopQuestEnabled == TRUE,
			gGameUBOptions.LaptopLinkInsurance == TRUE))
	{
		return;
	}

	const CommunicationsPolicy::EmailRecord record =
		policy.insuranceRecord(notice);
	if (!record.available) return;

	const UINT8 emailVersion = policy.usesUnfinishedBusinessCatalog()
		? TYPE_EMAIL_INSURANCE_COMPANY_EMAIL_JA2_EDT
		: TYPE_EMAIL_EMAIL_EDT;
	AddEmailWithSpecialData(
		record.offset,
		record.length,
		INSURANCE_COMPANY,
		GetWorldTotalMin(),
		payoutPrice,
		mercId,
		emailVersion,
		static_cast<UINT8>(record.substitution),
		xmlEmail);
}
}


void StartInsuranceInvestigation( UINT16	ubPayoutID )
{
	if (!IsValidInsurancePayout(ubPayoutID))
	{
		Assert(FALSE);
		return;
	}

	const auto mercID = LaptopSaveInfo.pLifeInsurancePayouts[ubPayoutID].ubMercID;
	const auto payoutPrice = LaptopSaveInfo.pLifeInsurancePayouts[ubPayoutID].iPayOutPrice;

	// send an email telling player an investigation is taking place
	if (gStrategicStatus.ubInsuranceInvestigationsCnt == 0)
	{
		// first offense
		SendInsuranceNotice(
			CommunicationsPolicy::InsuranceNotice::FirstInvestigation,
			payoutPrice, mercID, XML_INSURANCE_SUSPICIOUS);
	}
	else
	{
		// subsequent offense
		SendInsuranceNotice(
			CommunicationsPolicy::InsuranceNotice::RepeatInvestigation,
			payoutPrice, mercID, XML_INSURANCE_INVESTIGATION);
	}

	UINT8 ubDays;
	if ( gMercProfiles[ mercID ].ubSuspiciousDeath == VERY_SUSPICIOUS_DEATH )
	{
		// the fact that you tried to cheat them gets realized very quickly. :-)
		ubDays = 1;
	}
	else
	{
		// calculate how many days the investigation will take
		ubDays = (UINT8) (2 + gStrategicStatus.ubInsuranceInvestigationsCnt + Random(3));		// 2-4 days, +1 for every previous investigation
	}

	// post an event to end the investigation that many days in the future (at 4pm)
	AddStrategicEvent( EVENT_INSURANCE_INVESTIGATION_OVER, GetMidnightOfFutureDayInMinutes( ubDays ) + 16 * 60, ubPayoutID );

	// increment counter of all investigations
	gStrategicStatus.ubInsuranceInvestigationsCnt++;
}


void EndInsuranceInvestigation( UINT16	ubPayoutID )
{
	if (!IsValidInsurancePayout(ubPayoutID))
	{
		Assert(FALSE);
		return;
	}

	const auto mercID = LaptopSaveInfo.pLifeInsurancePayouts[ubPayoutID].ubMercID;
	const auto payoutPrice = LaptopSaveInfo.pLifeInsurancePayouts[ubPayoutID].iPayOutPrice;

	// send an email telling player the investigation is over
	if ( gMercProfiles[ mercID ].ubSuspiciousDeath == VERY_SUSPICIOUS_DEATH )
	{
		// fraud, no payout!
		SendInsuranceNotice(
			CommunicationsPolicy::InsuranceNotice::VerySuspiciousFraud,
			payoutPrice, mercID, XML_INSURANCE_REFUSED);
	}
	// Flugente: also don't pay out if the death was suspicious. I mean, we get this if there were no enemies of the player straight up shot the guy...
	else if ( gMercProfiles[mercID].ubSuspiciousDeath == SUSPICIOUS_DEATH )
	{
		// fraud, no payout!
		SendInsuranceNotice(
			CommunicationsPolicy::InsuranceNotice::SuspiciousDeathFraud,
			payoutPrice, mercID, XML_INSURANCE_POLICYVIOLATION);
	}
	else
	{
		SendInsuranceNotice(
			CommunicationsPolicy::InsuranceNotice::InvestigationComplete,
			payoutPrice, mercID, XML_INSURANCE_COMPLETED);

		// only now make a payment (immediately)
		InsuranceContractPayLifeInsuranceForDeadMerc( ubPayoutID );
	}
}


//void InsuranceContractPayLifeInsuranceForDeadMerc( LIFE_INSURANCE_PAYOUT *pPayoutStruct )
void InsuranceContractPayLifeInsuranceForDeadMerc( UINT16 ubPayoutID )
{
	if (!IsValidInsurancePayout(ubPayoutID))
	{
		Assert(FALSE);
		return;
	}

	const auto mercID = LaptopSaveInfo.pLifeInsurancePayouts[ubPayoutID].ubMercID;
	const auto payoutPrice = LaptopSaveInfo.pLifeInsurancePayouts[ubPayoutID].iPayOutPrice;
	TacticalActor* soldier = GetJa2SoldierRepository().resolve(
		LaptopSaveInfo.pLifeInsurancePayouts[ubPayoutID].ubSoldierID.i);

	//if the mercs id number is the same what is in the soldier array
	if( soldier &&
		LaptopSaveInfo.pLifeInsurancePayouts[ ubPayoutID ].ubSoldierID ==
			soldier->identity().id() )
	{
		// and if the soldier is still active ( player hasn't removed carcass yet ), reset insurance flag
		if( soldier->roster().active() )
			soldier->employment().lifeInsurance() = 0;
	}

	//add transaction to players account
	AddTransactionToPlayersBook( INSURANCE_PAYOUT, mercID, GetWorldTotalMin(), payoutPrice );

	//add to the history log the fact that the we paid the insurance claim
	AddHistoryToPlayersLog( HISTORY_INSURANCE_CLAIM_PAYOUT, mercID, GetWorldTotalMin(), -1, -1 );

	//if there WASNT an investigation
	if( gMercProfiles[ mercID ].ubSuspiciousDeath == 0 )
	{
		//Add an email telling the user that he received an insurance payment
		SendInsuranceNotice(
			CommunicationsPolicy::InsuranceNotice::Payment,
			payoutPrice, mercID, XML_INSURANCE_APPROVED);
	}

	if (LaptopSaveInfo.ubNumberLifeInsurancePayoutUsed > 0)
		LaptopSaveInfo.ubNumberLifeInsurancePayoutUsed--;
	LaptopSaveInfo.pLifeInsurancePayouts[ ubPayoutID ].fActive = FALSE;
//	MemFree( pPayoutStruct );
}


//Gets called at the very end of the game
void InsuranceContractEndGameShutDown()
{
	//Free up the memory allocated to the insurance payouts
	if( LaptopSaveInfo.pLifeInsurancePayouts )
	{
		MemFree( LaptopSaveInfo.pLifeInsurancePayouts );
		LaptopSaveInfo.pLifeInsurancePayouts = NULL;
	}
	LaptopSaveInfo.ubNumberLifeInsurancePayouts = 0;
	LaptopSaveInfo.ubNumberLifeInsurancePayoutUsed = 0;
}


BOOLEAN MercIsInsurable( TacticalActor *pSoldier )
{
	if (!pSoldier || !IsInsuranceMercProfile(
			pSoldier->identity().profile(), NUM_PROFILES))
		return(FALSE);

	// only A.I.M. mercs currently on player's team
	if( ( pSoldier->roster().active() ) && ( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC ) )
	{
		// with more than one day left on their employment contract are eligible for insurance
		// the second part is because the insurance doesn't pay for any working day already started at time of death
//		if( ( (pSoldier->employment().endTime() - GetWorldTotalMin()) > 24 * 60) || pSoldier->employment().lifeInsurance() )
		if( CanSoldierExtendInsuranceContract( pSoldier ) || pSoldier->employment().lifeInsurance() )
		{
			// who aren't currently being held POW
			// POWs are also uninsurable - if already insured, that insurance IS valid but no new contracts or extension allowed
			if (pSoldier->assignment().current() != ASSIGNMENT_POW)
			{
				return(TRUE);
			}
		}
	}

	return(FALSE);
}


void EnableDisableInsuranceContractAcceptButtons()
{
	for (std::size_t i = 0;
		i < gubNumberofDisplayedInsuranceGrids &&
		i < gInsuranceFormButtons.size(); ++i)
		EnableDisableIndividualInsuranceContractButton(
			gInsuranceMercForForm[i], gInsuranceFormButtons[i]);
}


void EnableDisableIndividualInsuranceContractButton(
	UINT8 mercProfile, INT32 button)
{
	if (button < 0) return;
	if (!IsInsuranceMercProfile(mercProfile, NUM_PROFILES))
	{
		DisableButton(button);
		return;
	}
	SoldierID sSoldierID = GetSoldierIDFromMercID(mercProfile);
	if (sSoldierID == NOBODY)
	{
		DisableButton(button);
		return;
	}
	TacticalActor* soldier =
		GetJa2SoldierRepository().resolve(sSoldierID.i);
	if ( !soldier )
	{
		DisableButton(button);
		return;
	}

	// if the soldiers contract can be extended, enable the button
	if( CanSoldierExtendInsuranceContract( soldier ) )
		EnableButton(button);

	// else the soldier cant extend their insurance contract, disable the button
	else
		DisableButton(button);

	//if the merc is dead, disable the button
	if( IsMercDead(mercProfile) )
		DisableButton(button);
}


UINT32	GetTimeRemainingOnSoldiersInsuranceContract( TacticalActor *pSoldier )
{
	if (!pSoldier) return 0;
	//if the soldier has life insurance
	if( pSoldier->employment().lifeInsurance() )
	{
		//if the insurance contract hasnt started yet
		if( (INT32)GetWorldDay() < pSoldier->employment().insuranceStartDay() )
			return RemainingLaptopDays(
				pSoldier->employment().insuranceLengthDays(), 0);
		return RemainingLaptopDays(
			pSoldier->employment().insuranceLengthDays(),
			static_cast<INT32>(GetWorldDay()) -
				pSoldier->employment().insuranceStartDay());
	}
	else
		return( 0 );
}

UINT32	GetTimeRemainingOnSoldiersContract( TacticalActor *pSoldier )
{
	if (!pSoldier) return 0;
	std::int64_t dayMercLeaves =
		(static_cast<std::int64_t>(pSoldier->employment().endTime()) /
			1440) - 1;

	//Since the merc is leaving in the afternoon, we must adjust since the time left would be different if we did the calc
	//at 11:59 or 12:01 ( noon )
	if( pSoldier->employment().endTime() % 1440 )
		dayMercLeaves++;
	return RemainingInsuranceEmploymentDays(dayMercLeaves, GetWorldDay(),
		pSoldier->employment().totalLength());
}


BOOLEAN PurchaseOrExtendInsuranceForSoldier(
	TacticalActor *pSoldier, UINT32 uiInsuranceLength )
{
	if (!pSoldier || pSoldier->identity().profile() == NO_PROFILE ||
		pSoldier->identity().profile() >= NUM_PROFILES)
	{
		AssertMsg(FALSE, "Invalid soldier passed to insurance purchase");
		return FALSE;
	}

	const BOOLEAN hadInsurance =
		pSoldier->employment().lifeInsurance() != 0;
	const UINT32 currentCoverage = hadInsurance
		? GetTimeRemainingOnSoldiersInsuranceContract(pSoldier)
		: 0;
	const UINT32 maximumCoverage =
		GetTimeRemainingOnSoldiersContract(pSoldier);
	const INT32 cost = CalculateInsuranceContractCost(
		static_cast<INT32>(std::min<UINT32>(uiInsuranceLength,
			std::numeric_limits<INT32>::max())),
		pSoldier->identity().profile());
	const InsurancePurchaseValidation validation =
		ValidateInsurancePurchase(uiInsuranceLength, cost,
			LaptopSaveInfo.iCurrentBalance);
	if (validation != InsurancePurchaseValidation::Ready)
	{
		if (validation == InsurancePurchaseValidation::InsufficientFunds)
		{
			CHAR16 sText[800];
			GetInsuranceText(INS_MLTI_NOT_ENOUGH_FUNDS, sText);
			if (GetCurrentScreen() == LAPTOP_SCREEN)
				DoLapTopMessageBox(MSG_BOX_RED_ON_WHITE, sText,
					LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);
			else
				DoMapMessageBox(MSG_BOX_RED_ON_WHITE, sText,
					MAP_SCREEN, MSG_BOX_FLAG_OK, NULL);
		}
		return FALSE;
	}

	const INT32 newCoverage = InsuranceCoverageAfterPurchase(
		currentCoverage, uiInsuranceLength, maximumCoverage);
	if (newCoverage <= 0 ||
		static_cast<UINT32>(newCoverage) <= currentCoverage)
		return FALSE;

	const UINT32 now = GetWorldTotalMin();
	AddTransactionToPlayersBook(hadInsurance
			? EXTENDED_INSURANCE : PURCHASED_INSURANCE,
		pSoldier->identity().profile(), now, -cost);
	if (!hadInsurance)
		AddHistoryToPlayersLog(HISTORY_PURCHASED_INSURANCE,
			pSoldier->identity().profile(), now, -1, -1);

	pSoldier->employment().insuranceStartDay() =
		CalcStartDayOfInsurance(pSoldier);
	if (!hadInsurance)
		pSoldier->employment().insuranceStartTime() = now;
	pSoldier->employment().insuranceLengthDays() = newCoverage;
	pSoldier->employment().lifeInsurance() = 1;
	return TRUE;
}

BOOLEAN	CanSoldierExtendInsuranceContract( TacticalActor *pSoldier )
{
	if ( !pSoldier )
		return( FALSE );

	if( CalculateSoldiersInsuranceContractLength( pSoldier ) != 0 )
		return( TRUE );
	else
		return( FALSE );
}


INT32 CalculateSoldiersInsuranceContractLength( TacticalActor *pSoldier )
{
	if (!pSoldier || pSoldier->identity().profile() == NO_PROFILE ||
		pSoldier->identity().profile() >= NUM_PROFILES)
		return( 0 );

	INT32 iInsuranceContractLength=0;
	UINT32 uiTimeRemainingOnSoldiersContract = GetTimeRemainingOnSoldiersContract( pSoldier );


	//if the merc is dead
	if( IsMercDead( pSoldier->identity().profile() ) )
		return( 0 );


	// only mercs with at least 2 days to go on their employment contract are insurable
	// def: 2/5/99.	However if they already have insurance is SHOULD be ok
	if( uiTimeRemainingOnSoldiersContract < 2 && !( pSoldier->employment().lifeInsurance() != 0 && uiTimeRemainingOnSoldiersContract >= 1 ) )
	{
		return( 0 );
	}

	//
	//Calculate the insurance contract length
	//

	//if the soldier has an insurance contract, dont deduct a day
	if( pSoldier->employment().lifeInsurance() || DidGameJustStart() )
		iInsuranceContractLength = uiTimeRemainingOnSoldiersContract - GetTimeRemainingOnSoldiersInsuranceContract( pSoldier );

	//else deduct a day
	else
		iInsuranceContractLength = uiTimeRemainingOnSoldiersContract - GetTimeRemainingOnSoldiersInsuranceContract( pSoldier ) - 1;

	//make sure the length doesnt exceed the contract length
	if( ( GetTimeRemainingOnSoldiersInsuranceContract( pSoldier ) + iInsuranceContractLength ) > uiTimeRemainingOnSoldiersContract )
	{
		iInsuranceContractLength = uiTimeRemainingOnSoldiersContract - GetTimeRemainingOnSoldiersInsuranceContract( pSoldier );
	}

	//Is the mercs insurace contract is less then a day, set it to 0
	if( iInsuranceContractLength < 0 )
		iInsuranceContractLength = 0;

	if( pSoldier->employment().lifeInsurance() && pSoldier->employment().insuranceStartDay() >= (INT32)GetWorldDay() && iInsuranceContractLength < 2 )
		iInsuranceContractLength = 0;

	return( iInsuranceContractLength );
}

INT32	CalcStartDayOfInsurance( TacticalActor *pSoldier )
{
	UINT32	uiDayToStartInsurance=0;

	//if the soldier was just hired ( in transit ), and the game didnt just start
	if( pSoldier->assignment().current() == IN_TRANSIT && !DidGameJustStart() )
	{
		uiDayToStartInsurance = GetWorldDay( );
	}
	else
	{
		//Get tomorows date ( and convert it to days )
		uiDayToStartInsurance = GetMidnightOfFutureDayInMinutes( 1 ) / 1440;
	}

	return( uiDayToStartInsurance );
}

BOOLEAN AreAnyAimMercsOnTeam( )
{
	SoldierID Soldier = gTacticalStatus.Team[gbPlayerNum].bFirstID;

	for( ; Soldier <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++Soldier)
	{
		TacticalActor* soldier =
			GetJa2SoldierRepository().resolve(Soldier.i);
		//check to see if any of the mercs are AIM mercs
		if( soldier && soldier->roster().active() &&
			soldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
		{
			return TRUE;
		}
	}

	return FALSE;
}
