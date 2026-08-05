	#include "laptop.h"
	#include "AimPolicies.h"
	#include "aim.h"
	#include "WCheck.h"
	#include "Utilities.h"
	#include "WordWrap.h"
	#include "Encrypted File.h"
	#include "Text.h"
	#include "GameSettings.h"
	#include "LaptopPageResourceOwner.h"
	#include "LaptopSafety.h"
	#include "AimWebsiteLayout.h"

#include "LocalizedStrings.h"

#define	NUM_AIM_POLICY_PAGES				11
#define	NUM_AIM_POLICY_TOC_BUTTONS	9
#define	AIMPOLICYFILE		"BINARYDATA\\AimPol.edt"
#define AIM_POLICY_LINE_SIZE 80 * 5 * 2 // 80 columns of 5 lines that are wide chars, 800 bytes total

#define AIM_POLICY_TITLE_FONT				FONT14ARIAL
#define AIM_POLICY_TITLE_COLOR			AIM_GREEN
#define AIM_POLICY_TEXT_FONT				FONT10ARIAL
#define AIM_POLICY_TEXT_COLOR				FONT_MCOLOR_WHITE
#define AIM_POLICY_TOC_FONT					FONT12ARIAL
#define AIM_POLICY_TOC_COLOR				FONT_MCOLOR_WHITE
#define	AIM_POLICY_TOC_TITLE_FONT		FONT12ARIAL
#define	AIM_POLICY_TOC_TITLE_COLOR	FONT_MCOLOR_WHITE
#define AIM_POLICY_SUBTITLE_FONT		FONT12ARIAL
#define AIM_POLICY_SUBTITLE_COLOR		FONT_MCOLOR_WHITE
#define AIM_POLICY_AGREE_TOC_COLOR_ON			FONT_MCOLOR_WHITE
#define AIM_POLICY_AGREE_TOC_COLOR_OFF			FONT_MCOLOR_DKWHITE

#define	AIM_POLICY_MENU_BUTTON_AMOUNT	4

#define AIM_POLICY_TITLE_STATEMENT_WIDTH	300
#define	AIM_POLICY_TITLE_STATEMENT_X	IMAGE_OFFSET_X + (500 - AIM_POLICY_TITLE_STATEMENT_WIDTH) / 2 +5//80
#define AIM_POLICY_TITLE_STATEMENT_Y	AIM_SYMBOL_Y + AIM_SYMBOL_SIZE_Y + 75

#define	AIM_POLICY_SUBTITLE_NUMBER	AIM_POLICY_TITLE_STATEMENT_X - 75
#define	AIM_POLICY_SUBTITLE_X				AIM_POLICY_SUBTITLE_NUMBER + 20
#define	AIM_POLICY_SUBTITLE_Y					iScreenHeightOffset + 115 + LAPTOP_SCREEN_WEB_DELTA_Y

#define	AIM_POLICY_PARAGRAPH_NUMBER	AIM_POLICY_SUBTITLE_X - 12
#define	AIM_POLICY_PARAGRAPH_X			AIM_POLICY_PARAGRAPH_NUMBER + 23
#define	AIM_POLICY_PARAGRAPH_Y			AIM_POLICY_SUBTITLE_Y + 20
#define AIM_POLICY_PARAGRAPH_WIDTH	380
#define AIM_POLICY_PARAGRAPH_GAP		6
#define	AIM_POLICY_SUBPARAGRAPH_NUMBER	AIM_POLICY_PARAGRAPH_X
#define	AIM_POLICY_SUBPARAGRAPH_X		AIM_POLICY_SUBPARAGRAPH_NUMBER + 25

#define AIM_POLICY_TOC_PAGE					1
#define	AIM_POLICY_LAST_PAGE				10

#define AIM_POLICY_AGREE_PAGE				0

// These enums represent which paragraph they are located in the AimPol.edt file
enum
{
	AIM_STATEMENT_OF_POLICY,
	AIM_STATEMENT_OF_POLICY_1,
	AIM_STATEMENT_OF_POLICY_2,

	DEFINITIONS,
	DEFINITIONS_1,
	DEFINITIONS_2,
	DEFINITIONS_3,
	DEFINITIONS_4,

	LENGTH_OF_ENGAGEMENT,
	LENGTH_OF_ENGAGEMENT_1,
	LENGTH_OF_ENGAGEMENT_1_1,
	LENGTH_OF_ENGAGEMENT_1_2,
	LENGTH_OF_ENGAGEMENT_1_3,
	LENGTH_OF_ENGAGEMENT_2,

	LOCATION_0F_ENGAGEMENT,
	LOCATION_0F_ENGAGEMENT_1,
	LOCATION_0F_ENGAGEMENT_2,
	LOCATION_0F_ENGAGEMENT_2_1,
	LOCATION_0F_ENGAGEMENT_2_2,
	LOCATION_0F_ENGAGEMENT_2_3,
	LOCATION_0F_ENGAGEMENT_2_4,
	LOCATION_0F_ENGAGEMENT_3,

	CONTRACT_EXTENSIONS,
	CONTRACT_EXTENSIONS_1,
	CONTRACT_EXTENSIONS_2,
	CONTRACT_EXTENSIONS_3,

	TERMS_OF_PAYMENT,
	TERMS_OF_PAYMENT_1,

	TERMS_OF_ENGAGEMENT,
	TERMS_OF_ENGAGEMENT_1,
	TERMS_OF_ENGAGEMENT_2A,
	TERMS_OF_ENGAGEMENT_2B,

	ENGAGEMENT_TERMINATION,
	ENGAGEMENT_TERMINATION_1,
	ENGAGEMENT_TERMINATION_1_1,
	ENGAGEMENT_TERMINATION_1_2,
	ENGAGEMENT_TERMINATION_1_3,

	EQUIPMENT_AND_INVENTORY,
	EQUIPMENT_AND_INVENTORY_1,
	EQUIPMENT_AND_INVENTORY_2,

	POLICY_MEDICAL,
	POLICY_MEDICAL_1,
	POLICY_MEDICAL_2,
	POLICY_MEDICAL_3A,
	POLICY_MEDICAL_3B,
	POLICY_MEDICAL_4,

	NUM_AIM_POLICY_LOCATIONS

}AimPolicyTextLocatoins;


//Toc menu mouse regions
MOUSE_REGION	gSelectedPolicyTocMenuRegion[ NUM_AIM_POLICY_TOC_BUTTONS ];
void SelectPolicyTocMenuRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//Agree/Disagree menu Buttons regions
void		BtnPoliciesAgreeButtonCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiPoliciesAgreeButton[ 2 ];
INT32		guiPoliciesButtonImage;

//Bottom Menu Buttons
void		BtnPoliciesMenuButtonCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiPoliciesMenuButton[ AIM_POLICY_MENU_BUTTON_AMOUNT ];
INT32		guiPoliciesMenuButtonImage;




UINT32		guiBottomButton;
UINT32		guiBottomButton2;
UINT8		gubCurPageNum;
BOOLEAN		gfInPolicyToc =	FALSE;
BOOLEAN		gfInAgreementPage = FALSE;
BOOLEAN		gfAimPolicyMenuBarLoaded = FALSE;
UINT32		guiContentButton;
BOOLEAN		gfExitingPolicesAgreeButton;
UINT8		gubPoliciesAgreeButtonDown;
UINT8		gubAimPolicyMenuButtonDown=255;
BOOLEAN		gfExitingAimPolicy;
BOOLEAN		AimPoliciesSubPagesVisitedFlag[NUM_AIM_POLICY_PAGES];




BOOLEAN InitAimPolicyMenuBar(void);
BOOLEAN ExitAimPolicyMenuBar(void);
BOOLEAN InitAimPolicyTocMenu(void);
BOOLEAN ExitAimPolicyTocMenu(void);
BOOLEAN DrawAimPolicyMenu();
BOOLEAN	DisplayAimPolicyStatement(void);
BOOLEAN	DisplayAimPolicyTitleText(void);
BOOLEAN InitAgreementRegion(void);
BOOLEAN ExitAgreementButton(void);
void DisableAimPolicyButton();
void ResetAimPolicyButtons();
void ChangingAimPoliciesSubPage( UINT8 ubSubPageNumber );



BOOLEAN	DisplayAimPolicyTitle(UINT16 usPosY, UINT8	ubPageNum, FLOAT fNumber);
UINT16 DisplayAimPolicyParagraph(UINT16 usPosY, UINT8	ubPageNum, FLOAT fNumber);
UINT16 DisplayAimPolicySubParagraph(UINT16 usPosY, UINT8	ubPageNum, FLOAT fNumber);

namespace
{
LaptopPageResourceOwner gAimPoliciesResources;
LaptopPageResourceOwner gAimPoliciesMenuResources;
LaptopPageResourceOwner gAimPoliciesTocResources;
LaptopPageResourceOwner gAimPoliciesAgreementResources;

AimWebsiteLayoutModel::PolicyLayout CurrentAimPolicyLayout()
{
	return AimWebsiteLayoutModel::MakePolicyLayout(
		gubCurPageNum == 0,
		gGameExternalOptions.gfUseNewStartingGearInterface != FALSE,
		{iScreenWidthOffset, iScreenHeightOffset,
		 IMAGE_OFFSET_X, IMAGE_OFFSET_Y, LAPTOP_SCREEN_WEB_DELTA_Y});
}
}



void GameInitAimPolicies()
{

}

void EnterInitAimPolicies()
{
	memset(AimPoliciesSubPagesVisitedFlag, 0,
		sizeof(AimPoliciesSubPagesVisitedFlag));
}


BOOLEAN EnterAimPolicies()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner stagedResources;

	gAimPoliciesResources.clear();
	gAimPoliciesMenuResources.clear();
	gAimPoliciesTocResources.clear();
	gAimPoliciesAgreementResources.clear();

	gubCurPageNum = IsValidLaptopIndex(
		NUM_AIM_POLICY_PAGES, giCurrentSubPage)
		? static_cast<UINT8>(giCurrentSubPage) : 0;

	gfAimPolicyMenuBarLoaded = FALSE;
	gfExitingAimPolicy = FALSE;

	gubPoliciesAgreeButtonDown = 255;
	gubAimPolicyMenuButtonDown	= 255;

	gfInPolicyToc = FALSE;
	gfInAgreementPage = FALSE;

	// load the Bottom Buttons graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BottomButton.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiBottomButton));

	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BottomButton2.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiBottomButton2));

	// load the Content Buttons graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\ContentButton.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiContentButton));

	CHECKF(InitAimDefaults());
	if (gubCurPageNum != 0 && !InitAimPolicyMenuBar())
	{
		RemoveAimDefaults();
		return FALSE;
	}

	gAimPoliciesResources = std::move(stagedResources);
	RenderAimPolicies();
	return(TRUE);
}

void ExitAimPolicies()
{
	gfExitingAimPolicy = TRUE;

	ExitAgreementButton();
	ExitAimPolicyTocMenu();
	ExitAimPolicyMenuBar();
	gAimPoliciesResources.clear();
	RemoveAimDefaults();

	giCurrentSubPage = gubCurPageNum;

}

void HandleAimPolicies()
{
	if( (gfAimPolicyMenuBarLoaded != TRUE) && gubCurPageNum != 0)
	{
		InitAimPolicyMenuBar();
//		RenderAimPolicies();
		fPausedReDrawScreenFlag = TRUE;
	}

}

void RenderAimPolicies()
{
	UINT16	usNumPixles;

	DrawAimDefaults();

	DisplayAimPolicyTitleText();

	if( gfInAgreementPage && gubCurPageNum != 0 )
		ExitAgreementButton();

	switch( gubCurPageNum )
	{
		case 0:
			DisplayAimPolicyStatement();
			if (!gfInAgreementPage)
				InitAgreementRegion();
			break;

		case 1:
			InitAimPolicyTocMenu();
			InitAimPolicyMenuBar();
			DisableAimPolicyButton();
			DrawAimPolicyMenu();
			break;

		case 2:
			//Display the Definitions title
			DisplayAimPolicyTitle(AIM_POLICY_SUBTITLE_Y, DEFINITIONS, (FLOAT)1.0);
			usNumPixles = AIM_POLICY_PARAGRAPH_Y;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, DEFINITIONS_1, (FLOAT)1.1) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, DEFINITIONS_2, (FLOAT)1.2) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, DEFINITIONS_3, (FLOAT)1.3) + AIM_POLICY_PARAGRAPH_GAP;
			(void)DisplayAimPolicyParagraph(usNumPixles, DEFINITIONS_4, (FLOAT)1.4);
			break;

		case 3:
			DisplayAimPolicyTitle(AIM_POLICY_SUBTITLE_Y, LENGTH_OF_ENGAGEMENT, (FLOAT)2.0);
			usNumPixles = AIM_POLICY_PARAGRAPH_Y;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, LENGTH_OF_ENGAGEMENT_1, (FLOAT)2.1) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicySubParagraph(usNumPixles, LENGTH_OF_ENGAGEMENT_1_1, (FLOAT)2.11) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicySubParagraph(usNumPixles, LENGTH_OF_ENGAGEMENT_1_2, (FLOAT)2.12) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicySubParagraph(usNumPixles, LENGTH_OF_ENGAGEMENT_1_3, (FLOAT)2.13) + AIM_POLICY_PARAGRAPH_GAP;
			(void)DisplayAimPolicyParagraph(usNumPixles, LENGTH_OF_ENGAGEMENT_2, (FLOAT)2.2);
			break;

		case 4:
			DisplayAimPolicyTitle(AIM_POLICY_SUBTITLE_Y, LOCATION_0F_ENGAGEMENT, (FLOAT)3.0);
			usNumPixles = AIM_POLICY_PARAGRAPH_Y;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, LOCATION_0F_ENGAGEMENT_1, (FLOAT)3.1) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, LOCATION_0F_ENGAGEMENT_2, (FLOAT)3.2) + AIM_POLICY_PARAGRAPH_GAP;

			usNumPixles += DisplayAimPolicySubParagraph(usNumPixles, LOCATION_0F_ENGAGEMENT_2_1, (FLOAT)3.21) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicySubParagraph(usNumPixles, LOCATION_0F_ENGAGEMENT_2_2, (FLOAT)3.22) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicySubParagraph(usNumPixles, LOCATION_0F_ENGAGEMENT_2_3, (FLOAT)3.23) + AIM_POLICY_PARAGRAPH_GAP;
//			usNumPixles += DisplayAimPolicySubParagraph(usNumPixles, LOCATION_0F_ENGAGEMENT_2_4, (FLOAT)3.24) + AIM_POLICY_PARAGRAPH_GAP;

			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, LOCATION_0F_ENGAGEMENT_2_4, (FLOAT)3.3) + AIM_POLICY_PARAGRAPH_GAP;

			(void)DisplayAimPolicyParagraph(usNumPixles, LOCATION_0F_ENGAGEMENT_3, (FLOAT)3.4);
			break;

		case 5:
			DisplayAimPolicyTitle(AIM_POLICY_SUBTITLE_Y, CONTRACT_EXTENSIONS, (FLOAT)4.0);
			usNumPixles = AIM_POLICY_PARAGRAPH_Y;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, CONTRACT_EXTENSIONS_1, (FLOAT)4.1) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, CONTRACT_EXTENSIONS_2, (FLOAT)4.2) + AIM_POLICY_PARAGRAPH_GAP;
			(void)DisplayAimPolicyParagraph(usNumPixles, CONTRACT_EXTENSIONS_3, (FLOAT)4.3);
			break;

		case 6:
			DisplayAimPolicyTitle(AIM_POLICY_SUBTITLE_Y, TERMS_OF_PAYMENT, (FLOAT)5.0);
			usNumPixles = AIM_POLICY_PARAGRAPH_Y;
			(void)DisplayAimPolicyParagraph(usNumPixles, TERMS_OF_PAYMENT_1, (FLOAT)5.1);
			break;

		case 7:
			DisplayAimPolicyTitle(AIM_POLICY_SUBTITLE_Y, TERMS_OF_ENGAGEMENT, (FLOAT)6.0);
			usNumPixles = AIM_POLICY_PARAGRAPH_Y;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, TERMS_OF_ENGAGEMENT_1, (FLOAT)6.1) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, TERMS_OF_ENGAGEMENT_2A, (FLOAT)6.2) + AIM_POLICY_PARAGRAPH_GAP;
			(void)DisplayAimPolicyParagraph(usNumPixles, TERMS_OF_ENGAGEMENT_2B, (FLOAT)0.0);
			break;

		case 8:
			DisplayAimPolicyTitle(AIM_POLICY_SUBTITLE_Y, ENGAGEMENT_TERMINATION, (FLOAT)7.0);
			usNumPixles = AIM_POLICY_PARAGRAPH_Y;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, ENGAGEMENT_TERMINATION_1, (FLOAT)7.1) + AIM_POLICY_PARAGRAPH_GAP;

			usNumPixles += DisplayAimPolicySubParagraph(usNumPixles, ENGAGEMENT_TERMINATION_1_1, (FLOAT)7.11) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicySubParagraph(usNumPixles, ENGAGEMENT_TERMINATION_1_2, (FLOAT)7.12) + AIM_POLICY_PARAGRAPH_GAP;
			(void)DisplayAimPolicySubParagraph(usNumPixles, ENGAGEMENT_TERMINATION_1_3, (FLOAT)7.13);
			break;

		case 9:
			DisplayAimPolicyTitle(AIM_POLICY_SUBTITLE_Y, EQUIPMENT_AND_INVENTORY, (FLOAT)8.0);
			usNumPixles = AIM_POLICY_PARAGRAPH_Y;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, EQUIPMENT_AND_INVENTORY_1, (FLOAT)8.1) + AIM_POLICY_PARAGRAPH_GAP;
			(void)DisplayAimPolicyParagraph(usNumPixles, EQUIPMENT_AND_INVENTORY_2, (FLOAT)8.2);
			break;

		case 10:
			DisableAimPolicyButton();

			DisplayAimPolicyTitle(AIM_POLICY_SUBTITLE_Y, POLICY_MEDICAL, (FLOAT)9.0);
			usNumPixles = AIM_POLICY_PARAGRAPH_Y;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, POLICY_MEDICAL_1, (FLOAT)9.1) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, POLICY_MEDICAL_2, (FLOAT)9.2) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, POLICY_MEDICAL_3A, (FLOAT)9.3) + AIM_POLICY_PARAGRAPH_GAP;
			usNumPixles += DisplayAimPolicyParagraph(usNumPixles, POLICY_MEDICAL_3B, (FLOAT)0.0) + AIM_POLICY_PARAGRAPH_GAP;
			(void)DisplayAimPolicyParagraph(usNumPixles, POLICY_MEDICAL_4, (FLOAT)9.4);
			break;
	}

	MarkButtonsDirty( );

	RenderWWWProgramTitleBar( );

	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}



BOOLEAN InitAimPolicyMenuBar(void)
{
	LaptopPageResourceOwner stagedResources;
	UINT16					i;
	const auto layout = CurrentAimPolicyLayout();

	if(gfAimPolicyMenuBarLoaded)
		return(TRUE);

	//Load graphic for buttons
	gAimPoliciesMenuResources.clear();
	CHECKF(stagedResources.addButtonImage(
		LoadButtonImageOwned("LAPTOP\\BottomButtons2.sti", -1,0,-1,1,-1),
		guiPoliciesMenuButtonImage));

	for(i=0; i<AIM_POLICY_MENU_BUTTON_AMOUNT; i++)
	{
		const auto buttonPosition = layout.menuButtons.at(i);
		const INT32 button = CreateIconAndTextButton( guiPoliciesMenuButtonImage, AimPolicyText[i], FONT10ARIAL,
														AIM_BUTTON_ON_COLOR, DEFAULT_SHADOW,
														AIM_BUTTON_OFF_COLOR, DEFAULT_SHADOW,
														TEXT_CJUSTIFIED,
												buttonPosition.x, buttonPosition.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, BtnPoliciesMenuButtonCallback);
		CHECKF(stagedResources.addButton(button, guiPoliciesMenuButton[i]));
		SetButtonCursor(guiPoliciesMenuButton[i], CURSOR_WWW);
		MSYS_SetBtnUserData( guiPoliciesMenuButton[i], 0, i);

	}




	gAimPoliciesMenuResources = std::move(stagedResources);
	gfAimPolicyMenuBarLoaded = TRUE;

	return(TRUE);
}

BOOLEAN ExitAimPolicyMenuBar(void)
{
	gAimPoliciesMenuResources.clear();
	gfAimPolicyMenuBarLoaded = FALSE;

	return(TRUE);
}



BOOLEAN DrawAimPolicyMenu()
{
	UINT16		i;
	CHAR16		sText[400];
	HVOBJECT	hContentButtonHandle;
	const auto layout = CurrentAimPolicyLayout();
	UINT8		ubLocInFile[]= {
					DEFINITIONS,
					LENGTH_OF_ENGAGEMENT,
					LOCATION_0F_ENGAGEMENT,
					CONTRACT_EXTENSIONS,
					TERMS_OF_PAYMENT,
					TERMS_OF_ENGAGEMENT,
					ENGAGEMENT_TERMINATION,
					EQUIPMENT_AND_INVENTORY,
					POLICY_MEDICAL};

	GetVideoObject(&hContentButtonHandle, guiContentButton);

	for(i=0; i<NUM_AIM_POLICY_TOC_BUTTONS; i++)
	{
		const auto button = layout.tocButtons.at(i);
		BltVideoObject(FRAME_BUFFER, hContentButtonHandle, 0,button.x, button.y, VO_BLT_SRCTRANSPARENCY,NULL);
		if(!g_bUseXML_Strings)
		{
			UINT32 uiStartLoc = AIM_POLICY_LINE_SIZE * ubLocInFile[i];
			LoadEncryptedDataFromFile(AIMPOLICYFILE, sText, uiStartLoc, AIM_HISTORY_LINE_SIZE);
		}
		else
		{
			Loc::GetString(Loc::AIM_POLICY, L"Line", ubLocInFile[i], sText, 400);
		}
		DrawTextToScreen(sText, button.x + layout.tocTextInset.x, button.y + layout.tocTextInset.y, button.width, AIM_POLICY_TOC_FONT, AIM_POLICY_TOC_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}
	gfInPolicyToc = TRUE;

	return(TRUE);
}

BOOLEAN InitAimPolicyTocMenu(void)
{
	LaptopPageResourceOwner stagedResources;
	UINT16			i;
	const auto layout = CurrentAimPolicyLayout();
	if(gfInPolicyToc)
		return(TRUE);

	gAimPoliciesTocResources.clear();
	for(i=0; i<NUM_AIM_POLICY_TOC_BUTTONS; i++)
	{
		const auto button = layout.tocButtons.at(i);
		//Mouse region for the toc buttons
		MSYS_DefineRegion( &gSelectedPolicyTocMenuRegion[i], button.x, button.y, button.right(), button.bottom(), MSYS_PRIORITY_HIGH,
								CURSOR_WWW, MSYS_NO_CALLBACK, SelectPolicyTocMenuRegionCallBack);
		stagedResources.addRegion(gSelectedPolicyTocMenuRegion[i]);
		MSYS_SetRegionUserData( &gSelectedPolicyTocMenuRegion[i], 0, i+2);
	}
	gAimPoliciesTocResources = std::move(stagedResources);
	gfInPolicyToc = TRUE;

	return(TRUE);
}



BOOLEAN ExitAimPolicyTocMenu()
{
	gAimPoliciesTocResources.clear();
	gfInPolicyToc = FALSE;

	return(TRUE);
}



void SelectPolicyTocMenuRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if(gfInPolicyToc)
	{
		if (iReason & MSYS_CALLBACK_REASON_INIT)
		{
		}
		else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
		{
			gubCurPageNum = (UINT8)MSYS_GetRegionUserData( pRegion, 0 );

			ChangingAimPoliciesSubPage( gubCurPageNum );

			ExitAimPolicyTocMenu();
			ResetAimPolicyButtons();
			DisableAimPolicyButton();
		}
		else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
		{
		}
	}
}


BOOLEAN	DisplayAimPolicyTitleText(void)
{
	CHAR16	sText[400];
	const auto layout = CurrentAimPolicyLayout();

	//Load anfd display title
	if(!g_bUseXML_Strings)
	{
		UINT32 uiStartLoc = AIM_POLICY_LINE_SIZE * AIM_STATEMENT_OF_POLICY;
		LoadEncryptedDataFromFile(AIMPOLICYFILE, sText, uiStartLoc, AIM_POLICY_LINE_SIZE);
	}
	else
	{
		Loc::GetString(Loc::AIM_POLICY, L"Line", AIM_STATEMENT_OF_POLICY);
	}
	DrawTextToScreen(sText, layout.title.origin.x, layout.title.origin.y,
		layout.title.width, AIM_POLICY_TITLE_FONT, AIM_POLICY_TITLE_COLOR,
		FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	
	return(TRUE);
}


BOOLEAN	DisplayAimPolicyStatement(void)
{
	CHAR16	sText[400];
	UINT16	usNumPixels;

	//load and display the statment of policies
	if(!g_bUseXML_Strings)
	{
		UINT32 uiStartLoc = AIM_POLICY_LINE_SIZE * AIM_STATEMENT_OF_POLICY_1;
		LoadEncryptedDataFromFile(AIMPOLICYFILE, sText, uiStartLoc, AIM_POLICY_LINE_SIZE);
	}
	else
	{
		Loc::GetString(Loc::AIM_POLICY, L"Line", AIM_STATEMENT_OF_POLICY_1, sText, 400);
	}
	usNumPixels = DisplayWrappedString(AIM_POLICY_TITLE_STATEMENT_X, AIM_POLICY_TITLE_STATEMENT_Y, AIM_POLICY_TITLE_STATEMENT_WIDTH, 2, AIM_POLICY_TEXT_FONT, AIM_POLICY_TEXT_COLOR, sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	//load and display the statment of policies
	if(!g_bUseXML_Strings)
	{
		UINT32 uiStartLoc = AIM_POLICY_LINE_SIZE * AIM_STATEMENT_OF_POLICY_2;
		LoadEncryptedDataFromFile(AIMPOLICYFILE, sText, uiStartLoc, AIM_POLICY_LINE_SIZE);
	}
	else
	{
		Loc::GetString(Loc::AIM_POLICY, L"Line", AIM_STATEMENT_OF_POLICY_2, sText, 400);
	}
	DisplayWrappedString(AIM_POLICY_TITLE_STATEMENT_X, (UINT16)(AIM_POLICY_TITLE_STATEMENT_Y + usNumPixels+15), AIM_POLICY_TITLE_STATEMENT_WIDTH, 2, AIM_POLICY_TEXT_FONT, AIM_POLICY_TEXT_COLOR, sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	return(TRUE);
}




BOOLEAN InitAgreementRegion(void)
{
	LaptopPageResourceOwner stagedResources;
	UINT16	i;
	const auto layout = CurrentAimPolicyLayout();
	if (gfInAgreementPage)
		return TRUE;

	gfExitingPolicesAgreeButton = FALSE;
	gAimPoliciesAgreementResources.clear();

	//Load graphic for buttons
	CHECKF(stagedResources.addButtonImage(
		LoadButtonImageOwned("LAPTOP\\BottomButtons2.sti", -1,0,-1,1,-1),
		guiPoliciesButtonImage));

	for(i=0; i < 2; i++)
	{
		const auto buttonPosition = layout.agreementButtons.at(i);
		const INT32 button = CreateIconAndTextButton( guiPoliciesButtonImage, AimPolicyText[i+AIM_POLICIES_DISAGREE], AIM_POLICY_TOC_FONT,
														AIM_POLICY_AGREE_TOC_COLOR_ON, DEFAULT_SHADOW,
														AIM_POLICY_AGREE_TOC_COLOR_OFF, DEFAULT_SHADOW,
														TEXT_CJUSTIFIED,
												buttonPosition.x, buttonPosition.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, BtnPoliciesAgreeButtonCallback);
		CHECKF(stagedResources.addButton(button, guiPoliciesAgreeButton[i]));
		SetButtonCursor(guiPoliciesAgreeButton[i], CURSOR_WWW);
		MSYS_SetBtnUserData( guiPoliciesAgreeButton[i], 0, i);

	}
	gAimPoliciesAgreementResources = std::move(stagedResources);
	gfInAgreementPage = TRUE;
	return(TRUE);
}

BOOLEAN ExitAgreementButton(void)
{
	gfExitingPolicesAgreeButton = TRUE;
	gAimPoliciesAgreementResources.clear();
	gfInAgreementPage = FALSE;

	return(TRUE);
}



BOOLEAN	DisplayAimPolicyTitle(UINT16 usPosY, UINT8	ubPageNum, FLOAT fNumber)
{
	CHAR16	sText[400];

	//Load and display title
	if(!g_bUseXML_Strings)
	{
		UINT32 uiStartLoc = AIM_POLICY_LINE_SIZE * ubPageNum;
		LoadEncryptedDataFromFile(AIMPOLICYFILE, sText, uiStartLoc, AIM_POLICY_LINE_SIZE);
	}
	else
	{
		Loc::GetString(Loc::AIM_POLICY,L"Line", ubPageNum, sText, 400); 
	}
	DrawTextToScreen(sText, AIM_POLICY_SUBTITLE_NUMBER, usPosY, 0, AIM_POLICY_SUBTITLE_FONT, AIM_POLICY_SUBTITLE_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	return(TRUE);
}


UINT16 DisplayAimPolicyParagraph(UINT16 usPosY, UINT8	ubPageNum, FLOAT fNumber)
{
	CHAR16	sText[400];
	CHAR16	sTemp[20];
	UINT16	usNumPixels;
	if(!g_bUseXML_Strings)
	{
		UINT32 uiStartLoc = AIM_POLICY_LINE_SIZE * ubPageNum;
		LoadEncryptedDataFromFile(AIMPOLICYFILE, sText, uiStartLoc, AIM_POLICY_LINE_SIZE);
	}
	else
	{
		Loc::GetString(Loc::AIM_POLICY, L"Line", ubPageNum, sText, 400);
	}
	if(fNumber != 0.0)
	{
		//Display the section number
		swprintf(sTemp, L"%2.1f", fNumber);
		DrawTextToScreen(sTemp, AIM_POLICY_PARAGRAPH_NUMBER, usPosY, 0, AIM_POLICY_TEXT_FONT, AIM_POLICY_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}

	//Display the text beside the section number
	usNumPixels = DisplayWrappedString(AIM_POLICY_PARAGRAPH_X, usPosY, AIM_POLICY_PARAGRAPH_WIDTH, 2, AIM_POLICY_TEXT_FONT, AIM_POLICY_TEXT_COLOR, sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	return(usNumPixels);
}

UINT16 DisplayAimPolicySubParagraph(UINT16 usPosY, UINT8	ubPageNum, FLOAT fNumber)
{
	CHAR16	sText[400];
	CHAR16	sTemp[20];
	UINT16	usNumPixels;
	if(!g_bUseXML_Strings)
	{
		UINT32 uiStartLoc = AIM_POLICY_LINE_SIZE * ubPageNum;
		LoadEncryptedDataFromFile(AIMPOLICYFILE, sText, uiStartLoc, AIM_POLICY_LINE_SIZE);
	}
	else
	{
		Loc::GetString(Loc::AIM_POLICY, L"Line", ubPageNum, sText, 400);
	}
	//Display the section number
	swprintf(sTemp, L"%2.2f", fNumber);
	DrawTextToScreen(sTemp, AIM_POLICY_SUBPARAGRAPH_NUMBER, usPosY, 0, AIM_POLICY_TEXT_FONT, AIM_POLICY_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	//Display the text beside the section number
	usNumPixels = DisplayWrappedString(AIM_POLICY_SUBPARAGRAPH_X, usPosY, AIM_POLICY_PARAGRAPH_WIDTH, 2, AIM_POLICY_TEXT_FONT, AIM_POLICY_TEXT_COLOR, sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	return(usNumPixels);
}




void BtnPoliciesAgreeButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	UINT8	ubRetValue;
	static BOOLEAN fOnPage=TRUE;
	if(fOnPage)
	{
		ubRetValue = (UINT8)MSYS_GetBtnUserData( btn, 0 );
		if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
		{
			btn->uiFlags |= BUTTON_CLICKED_ON;
			InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
			gubPoliciesAgreeButtonDown = ubRetValue;
		}

		if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
		{
			if (btn->uiFlags & BUTTON_CLICKED_ON)
			{
				btn->uiFlags &= (~BUTTON_CLICKED_ON );

				//Agree
				fOnPage = FALSE;
				if(ubRetValue == 1)
				{
					gubCurPageNum++;
					ChangingAimPoliciesSubPage( gubCurPageNum );
				}

				//Disagree
				else
				{
					guiCurrentLaptopMode = LAPTOP_MODE_AIM;
				}
				InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
				fOnPage = TRUE;
				gubPoliciesAgreeButtonDown = 255;
			}
		}
		if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
		{
			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			gubPoliciesAgreeButtonDown = 255;
			InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
		}
	}
}

void BtnPoliciesMenuButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	UINT8	ubRetValue;
	static BOOLEAN fOnPage=TRUE;
	if(fOnPage)
	{
		ubRetValue = (UINT8)MSYS_GetBtnUserData( btn, 0 );
		if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
		{
			btn->uiFlags |= BUTTON_CLICKED_ON;
			gubAimPolicyMenuButtonDown	= ubRetValue;
			InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
		}

		if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
		{
			if (btn->uiFlags & BUTTON_CLICKED_ON)
			{
				btn->uiFlags &= (~BUTTON_CLICKED_ON );

				gubAimPolicyMenuButtonDown	= 255;
				//If previous Page
				if( ubRetValue == 0 )
				{
					if( gubCurPageNum > 1)
					{
						gubCurPageNum--;
						ChangingAimPoliciesSubPage( gubCurPageNum );
					}
				}

				// Home Page
				else if( ubRetValue == 1 )
				{
					guiCurrentLaptopMode = LAPTOP_MODE_AIM;
				}

				//Company policies index
				else if( ubRetValue == 2 )
				{
					if( gubCurPageNum != 1 )
					{
						gubCurPageNum=1;
						ChangingAimPoliciesSubPage( gubCurPageNum );
					}
				}

				//Next Page
				else if( ubRetValue == 3 )
				{
					if( gubCurPageNum < NUM_AIM_POLICY_PAGES-1 )
					{
						gubCurPageNum++;
						ChangingAimPoliciesSubPage( gubCurPageNum );

						fOnPage = FALSE;
						if(gfInPolicyToc)
						{
							ExitAimPolicyTocMenu();
						}
						fOnPage = TRUE;
					}
				}
				InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
				ResetAimPolicyButtons();
				DisableAimPolicyButton();
				fOnPage = TRUE;
			}
		}
		if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
		{
			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			gubAimPolicyMenuButtonDown	= 255;
			DisableAimPolicyButton();
			InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
		}
	}
}


void ResetAimPolicyButtons()
{
	int i=0;

	for(i=0; i<AIM_POLICY_MENU_BUTTON_AMOUNT; i++)
	{
		ButtonList[ guiPoliciesMenuButton[i] ]->uiFlags &= ~BUTTON_CLICKED_ON;
	}
}


void DisableAimPolicyButton()
{
	if( gfExitingAimPolicy == TRUE || gfAimPolicyMenuBarLoaded == FALSE )
		return;

	if( gubCurPageNum == AIM_POLICY_TOC_PAGE )
	{
		ButtonList[ guiPoliciesMenuButton[ 0 ] ]->uiFlags |= (BUTTON_CLICKED_ON );
		ButtonList[ guiPoliciesMenuButton[ 2 ] ]->uiFlags |= (BUTTON_CLICKED_ON );
	}
	else if( gubCurPageNum == AIM_POLICY_LAST_PAGE )
	{
		ButtonList[ guiPoliciesMenuButton[ 3 ] ]->uiFlags |= (BUTTON_CLICKED_ON );
	}
}


void ChangingAimPoliciesSubPage( UINT8 ubSubPageNumber )
{
	if (!IsValidLaptopIndex(NUM_AIM_POLICY_PAGES, ubSubPageNumber))
		return;

	fLoadPendingFlag = TRUE;

	if( AimPoliciesSubPagesVisitedFlag[ ubSubPageNumber ] == FALSE )
	{
		fConnectingToSubPage = TRUE;
		fFastLoadFlag = FALSE;

		AimPoliciesSubPagesVisitedFlag[ ubSubPageNumber ] = TRUE;
	}
	else
	{
		fConnectingToSubPage = TRUE;
		fFastLoadFlag = TRUE;
	}
}









