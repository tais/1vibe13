/**
* @file
* @author Flugente (bears-pit.com)
*/

#include "laptop.h"
#include "LaptopSafety.h"
#include "insurance.h"
#include "insurance Contract.h"
#include "WCheck.h"
#include "Utilities.h"
#include "WordWrap.h"
#include "Cursors.h"
#include "line.h"
#include "Insurance Text.h"
#include "Encrypted File.h"
#include "Text.h"
#include "Multi Language Graphic Utils.h"
#include "Intelmarket.h"
#include "random.h"
#include "Interface.h"
#include "Soldier Add.h"
#include "Soldier Profile.h"
#include "DropDown.h"
#include "Overhead.h"
#include "Map Screen Interface.h"
#include "DynamicDialogue.h"
#include "Assignments.h"
#include "Town Militia.h"
#include "Game Clock.h"
#include "LuaInitNPCs.h"
#include "Game Event Hook.h"
#include "LaptopPageResourceOwner.h"
#include "LaptopUiStateModel.h"

#define		MERCOMP_FONT_COLOR								2
#define		CAMPHIS_FONT_COLOR_RED							FONT_MCOLOR_RED
#define		CAMPHIS_FONT_BIG								FONT14ARIAL
#define		CAMPHIS_FONT_MED								FONT12ARIAL
#define		CAMPHIS_FONT_SMALL								FONT10ARIAL

#define		MERCOMP_FONT_SHADOW								FONT_MCOLOR_WHITE

#define		CAMPAIGN_HISTORY_LINK_START_X					LAPTOP_SCREEN_UL_X
#define		CAMPAIGN_HISTORY_LINK_START_Y					LAPTOP_SCREEN_WEB_UL_Y + 5
#define		CAMPAIGN_HISTORY_LINK_TEXT_WIDTH				130
#define		CAMPAIGN_HISTORY_LINK_STEP_Y					14

#define		BACKGROUND_WIDTH				125
#define		CAMPAIGN_HISTORY_BACKGROUND_HEIGHT				100

#define		CAMPAIGN_HISTORY_BIG_TITLE_X					LAPTOP_SCREEN_UL_X + CAMPAIGN_HISTORY_LINK_TEXT_WIDTH + 10
#define		CAMPAIGN_HISTORY_BIG_TITLE_Y					10 + LAPTOP_SCREEN_WEB_UL_Y

#define		CAMPAIGN_HISTORY_SUBTITLE_X						CAMPAIGN_HISTORY_BIG_TITLE_X
#define		CAMPAIGN_HISTORY_SUBTITLE_Y						CAMPAIGN_HISTORY_BIG_TITLE_Y + 20

#define		NUM_LINKS										3
#define		MCA_START_CONTENT_Y								(LAPTOP_SCREEN_WEB_UL_Y + NUM_LINKS * 17)


extern UINT32	guiInsuranceBackGround;
extern UINT32	guiInsuranceSmallTitleImage;
extern UINT32	guiInsuranceBigRedLineImage;
extern UINT32	guiMercCompareBulletImage;
extern UINT32	guiMercCompareLogoImage;

//link to the various pages
MOUSE_REGION	gMercCompareLinkRegion_IM[NUM_LINKS];
static LaptopPageResourceOwner gIntelMarketPageResources;
static LaptopPageResourceOwner gIntelMarketBuyResources;
static LaptopPageResourceOwner gIntelMarketSellResources;

namespace
{
class ScopedDefaultFontShadow
{
public:
	~ScopedDefaultFontShadow() { SetFontShadow(DEFAULT_SHADOW); }
};
}

void SelectLinkRegionCallBack_IM( MOUSE_REGION * pRegion, INT32 iReason )
{
	if ( iReason & MSYS_CALLBACK_REASON_INIT )
	{
	}
	else if ( iReason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		UINT32 uiLink = MSYS_GetRegionUserData( pRegion, 0 );

		if ( uiLink == 0 )
			guiCurrentLaptopMode = LAPTOP_MODE_INTELMARKET_BUYINFO;
		else if ( uiLink == 1 )
			guiCurrentLaptopMode = LAPTOP_MODE_INTELMARKET_SELLINFO;
		else if ( uiLink == 2 )
			guiCurrentLaptopMode = LAPTOP_MODE_INTELMARKET_ABOUT;
	}
	else if ( iReason & MSYS_CALLBACK_REASON_RBUTTON_UP )
	{
	}
}

static BOOLEAN LoadDefaults_IM(LaptopPageResourceOwner& owner)
{
	VOBJECT_DESC	VObjectDesc;

	// load the Insurance bullet graphic and add it
	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP( "LAPTOP\\bullet.sti", VObjectDesc.ImageFile );
	CHECKF(owner.addVideoObject(&VObjectDesc, guiMercCompareBulletImage));

	// load the Flower Account Box graphic and add it
	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP( "LAPTOP\\BackGroundTile.sti", VObjectDesc.ImageFile );
	CHECKF(owner.addVideoObject(&VObjectDesc, guiInsuranceBackGround));

	// load the red bar on the side of the page and add it
	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP( "LAPTOP\\LargeBar.sti", VObjectDesc.ImageFile );
	CHECKF(owner.addVideoObject(&VObjectDesc, guiInsuranceBigRedLineImage));

	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP( "LAPTOP\\PressLogos.sti", VObjectDesc.ImageFile );
	CHECKF(owner.addVideoObject(&VObjectDesc, guiMercCompareLogoImage));

	UINT16 usPosX = CAMPAIGN_HISTORY_LINK_START_X;
	UINT16 usPosY = CAMPAIGN_HISTORY_LINK_START_Y;
	for ( int i = 0; i<NUM_LINKS; ++i )
	{
		MSYS_DefineRegion( &gMercCompareLinkRegion_IM[i], usPosX, usPosY, (UINT16)( usPosX + CAMPAIGN_HISTORY_LINK_TEXT_WIDTH ), usPosY + CAMPAIGN_HISTORY_LINK_STEP_Y, MSYS_PRIORITY_HIGH,
			CURSOR_WWW, MSYS_NO_CALLBACK, SelectLinkRegionCallBack_IM );
		CHECKF(owner.addRegion(gMercCompareLinkRegion_IM[i]));
		MSYS_SetRegionUserData( &gMercCompareLinkRegion_IM[i], 0, i );

		usPosY += CAMPAIGN_HISTORY_LINK_STEP_Y;
	}
	return TRUE;
}

void DisplayDefaults_IM()
{
	HVOBJECT	hPixHandle;
	GetVideoObject( &hPixHandle, guiMercCompareLogoImage );

	SetFontShadow( MERCOMP_FONT_SHADOW );
	
	WebPageTileBackground( 4, 4, BACKGROUND_WIDTH, CAMPAIGN_HISTORY_BACKGROUND_HEIGHT, guiInsuranceBackGround );

	//Display the title slogan
	DrawTextToScreen( szIntelWebsiteText[TEXT_INTEL_TITLE], CAMPAIGN_HISTORY_BIG_TITLE_X, CAMPAIGN_HISTORY_BIG_TITLE_Y, LAPTOP_SCREEN_LR_X - LAPTOP_SCREEN_UL_X, CAMPHIS_FONT_BIG, MERCOMP_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, 0 );

	//Display the subtitle slogan
	DrawTextToScreen( szIntelWebsiteText[TEXT_INTEL_SUBTITLE], CAMPAIGN_HISTORY_SUBTITLE_X, CAMPAIGN_HISTORY_SUBTITLE_Y, 0, CAMPHIS_FONT_BIG, MERCOMP_FONT_COLOR, FONT_MCOLOR_BLACK, FALSE, 0 );

	UINT16 usPosX = CAMPAIGN_HISTORY_LINK_START_X;
	UINT16 usPosY = CAMPAIGN_HISTORY_LINK_START_Y;
	for ( int i = 0; i<NUM_LINKS; ++i )
	{
		DisplayWrappedString( usPosX, usPosY, CAMPAIGN_HISTORY_LINK_TEXT_WIDTH, 2, CAMPHIS_FONT_MED, MERCOMP_FONT_COLOR, szIntelWebsiteText[TEXT_INTEL_LINK_1 + i], FONT_MCOLOR_BLACK, FALSE, 0 );

		usPosY += CAMPAIGN_HISTORY_LINK_STEP_Y;

		//Display the red bar under the link at the bottom.	and the text
		DisplaySmallColouredLineWithShadow( usPosX, usPosY - 2, LAPTOP_SCREEN_UL_X + CAMPAIGN_HISTORY_LINK_TEXT_WIDTH, usPosY - 2, FROMRGB( 137, 16, 231 ) );
	}

	// closing line that separates header from individual page
	DisplaySmallColouredLineWithShadow( usPosX, usPosY - 2, LAPTOP_SCREEN_LR_X, usPosY - 2, FROMRGB( 137, 16, 231 ) );

	usPosX = LAPTOP_SCREEN_LR_X - 70;
	usPosY = CAMPAIGN_HISTORY_LINK_START_Y - 2;
	BltVideoObject( FRAME_BUFFER, hPixHandle, 5, usPosX, usPosY, VO_BLT_SRCTRANSPARENCY, NULL );

	SetFontShadow( DEFAULT_SHADOW );
}

////////////////////////// INTELMARKET Sell intel for stuff ////////////////////////////////

// In the intel market, the player can spend intel to buy information
// possible informations are:
// - positions of enemy patrols
// - size of enemy garrison
// - position of enemy VIPs
// - position of POWs
// - position of enemy helis
//
// There are always INTELINFO_MAXNUMBER different informations available at a time. Every hour these change.
// It thus follows that we first need a huge array for numbering the info.
// We then select INTELINFO_MAXNUMBER out of that array, depending on the hour and some number-crunching.
// As a result, the player can only change the info selection by progressing time.
int sIntelInfo[INTELINFO_MAXNUMBER];

MOUSE_REGION	gIntelmarketbuyLinkRegion[INTELINFO_MAXNUMBER];
BOOLEAN			gIntelmarketbuyLinkRegion_Exists[INTELINFO_MAXNUMBER];

BOOLEAN fIntelRedraw = FALSE;

template<>	void	DropDownTemplate<DROPDOWN_INTEL_BUY>::SetRefresh() { fIntelRedraw = TRUE; }

CHAR16 gIntelBuyMapPartNamesStr[16][64];

INT32 gIntelBuyButton[2] = { -1, -1 };
BOOLEAN gIntelBuyButtonCreated[2] = { FALSE, FALSE };

void BuyLinkRegionCallBack( MOUSE_REGION * pRegion, INT32 iReason )
{
	if ( iReason & MSYS_CALLBACK_REASON_INIT )
	{
	}
	else if ( iReason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		INT32 intelinfo = MSYS_GetRegionUserData( pRegion, 0 );
		INT32 intelcost = MSYS_GetRegionUserData( pRegion, 1 );

		if ( intelinfo > -1 && GetIntel() >= intelcost )
		{
			BuyIntelInfo( intelinfo );
			AddIntel( -intelcost, TRUE );

			GetIntelInfoOfferings( sIntelInfo );

			fIntelRedraw = TRUE;
		}
	}
	else if ( iReason & MSYS_CALLBACK_REASON_RBUTTON_UP )
	{
	}
}

void IntelBuyButtonCallback( GUI_BUTTON *btn, INT32 reason )
{
	if ( reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= ( BUTTON_CLICKED_ON );
	}
	else if ( reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if ( btn->uiFlags & BUTTON_CLICKED_ON )
		{
			btn->uiFlags &= ~( BUTTON_CLICKED_ON );

			INT16 mapregion = DropDownTemplate<DROPDOWN_INTEL_BUY>::getInstance().GetSelectedEntryKey();
			if (!IsValidIntelMapRegion(mapregion))
			{
				Assert(FALSE);
				return;
			}

			int intelcost = 50;
			if ( btn->IDNum == gIntelBuyButton[1] )
				intelcost = 70;

			if ( GetIntel() >= intelcost )
			{
				AddIntel( -intelcost, TRUE );

				LaptopSaveInfo.usMapIntelFlags |= UINT32{1} << mapregion;

				// if we buy an advanced scan, also add seond flag
				if ( btn->IDNum == gIntelBuyButton[1] )
					LaptopSaveInfo.usMapIntelFlags |= UINT32{1} << (16 + mapregion);

				fIntelRedraw = TRUE;
			}
		}
	}
}

void EnterIntelmarket()
{
	LaptopPageResourceOwner stagedResources;
	if (!LoadDefaults_IM(stagedResources)) return;
	std::vector<std::pair<INT16, STR16> > dropdownvector;

	for ( INT16 i = 0; i < 16; ++i )
	{
		if (!(LaptopSaveInfo.usMapIntelFlags & (UINT32{1} << i)))
		{
			LaptopUiStateModel::CopyText(gIntelBuyMapPartNamesStr[i],
				szIntelWebsiteText[TEXT_INTEL_MAPREGION_1 + i]);

			dropdownvector.push_back( std::make_pair( i, gIntelBuyMapPartNamesStr[i] ) );
		}
	}

	if ( !dropdownvector.empty() )
	{
		DropDownTemplate<DROPDOWN_INTEL_BUY>::getInstance().SetEntries( dropdownvector );
		DropDownTemplate<DROPDOWN_INTEL_BUY>::getInstance().SetHelpText( szIntelWebsiteText[TEXT_INTEL_DROPDOWN_HELPTEXT] );
		DropDownTemplate<DROPDOWN_INTEL_BUY>::getInstance().Create( LAPTOP_SCREEN_UL_X, MCA_START_CONTENT_Y + 40 );
		DropDownTemplate<DROPDOWN_INTEL_BUY>::getInstance().SetColorLine( Get16BPPColor( FROMRGB( 137, 16, 231 ) ) );
		DropDownTemplate<DROPDOWN_INTEL_BUY>::getInstance().SetColorMarked( Get16BPPColor( FROMRGB( 137, 16, 231 ) ) );
	}
	else
	{
		DropDownTemplate<DROPDOWN_INTEL_BUY>::getInstance().ClearEntries();
	}

	gIntelMarketPageResources = std::move(stagedResources);
	
	BuildIntelInfoArray();
	GetIntelInfoOfferings( sIntelInfo );

	fIntelRedraw = TRUE;

	HandleIntelmarket();
}

void ExitIntelmarket()
{
	gIntelMarketBuyResources.clear();
	std::fill(std::begin(gIntelmarketbuyLinkRegion_Exists),
		std::end(gIntelmarketbuyLinkRegion_Exists), FALSE);
	std::fill(std::begin(gIntelBuyButtonCreated),
		std::end(gIntelBuyButtonCreated), FALSE);
	gIntelMarketPageResources.clear();

	DropDownTemplate<DROPDOWN_INTEL_BUY>::getInstance().Destroy();
}

void HandleIntelmarket()
{
	if ( fIntelRedraw )
	{
		RenderIntelmarket();

		fIntelRedraw = FALSE;
	}
}

void RenderIntelmarket()
{
	CHAR16		sText[800] = {};
	UINT16	usPosX, usPosY;
	HVOBJECT	hPixHandle;

	//Get the bullet
	if (!GetVideoObject(&hPixHandle, guiMercCompareBulletImage)) return;

	gIntelMarketBuyResources.clear();
	std::fill(std::begin(gIntelmarketbuyLinkRegion_Exists),
		std::end(gIntelmarketbuyLinkRegion_Exists), FALSE);
	std::fill(std::begin(gIntelBuyButtonCreated),
		std::end(gIntelBuyButtonCreated), FALSE);
	LaptopPageResourceOwner stagedResources;

	DisplayDefaults_IM();

	SetFontShadow( MERCOMP_FONT_SHADOW );
	ScopedDefaultFontShadow restoreFontShadow;

	usPosX = LAPTOP_SCREEN_UL_X;
	usPosY = MCA_START_CONTENT_Y;
	
	FLOAT budget = GetIntel();

	sgp_swprintf(sText, 800, szIntelWebsiteText[TEXT_INTEL_BUDGET],
		(int)budget);
	usPosY += DisplayWrappedString( usPosX, usPosY, LAPTOP_SCREEN_LR_X - LAPTOP_SCREEN_UL_X, 2, CAMPHIS_FONT_MED, FONT_BLACK, sText, FONT_MCOLOR_BLACK, FALSE, 0 );
	usPosY += 12;

	if ( DropDownTemplate<DROPDOWN_INTEL_BUY>::getInstance().HasEntries() )
	{
		sgp_swprintf(sText, 800,
			szIntelWebsiteText[TEXT_INTEL_AIRREGION],
			((1 + GetWorldHour() / 8) * 8));
		usPosY += DisplayWrappedString( usPosX, usPosY, LAPTOP_SCREEN_LR_X - LAPTOP_SCREEN_UL_X, 2, CAMPHIS_FONT_MED, FONT_BLACK, sText, FONT_MCOLOR_BLACK, FALSE, 0 );

		DropDownTemplate<DROPDOWN_INTEL_BUY>::getInstance().Display();

		SetFontShadow( MERCOMP_FONT_SHADOW );

		if (!stagedResources.addButton(CreateTextButton(
			szIntelWebsiteText[TEXT_INTEL_AIRREGION_BUY1], CAMPHIS_FONT_MED,
			(50 <= budget ? FONT_BLACK : FONT_RED), FONT_BLACK,
			BUTTON_USE_DEFAULT, usPosX + 140, usPosY, 150, 20,
			BUTTON_TOGGLE, MSYS_PRIORITY_HIGH, DEFAULT_MOVE_CALLBACK,
			IntelBuyButtonCallback), gIntelBuyButton[0])) return;
		gIntelBuyButtonCreated[0] = TRUE;

		if (!stagedResources.addButton(CreateTextButton(
			szIntelWebsiteText[TEXT_INTEL_AIRREGION_BUY2], CAMPHIS_FONT_MED,
			(70 <= budget ? FONT_BLACK : FONT_RED), FONT_BLACK,
			BUTTON_USE_DEFAULT, usPosX + 300, usPosY, 150, 20,
			BUTTON_TOGGLE, MSYS_PRIORITY_HIGH, DEFAULT_MOVE_CALLBACK,
			IntelBuyButtonCallback), gIntelBuyButton[1])) return;
		gIntelBuyButtonCreated[1] = TRUE;
	}

	usPosX = LAPTOP_SCREEN_UL_X;
	usPosY = MCA_START_CONTENT_Y + 200;

	BOOLEAN anyfound = FALSE;

	for ( int cnt = 0; cnt < INTELINFO_MAXNUMBER; ++cnt )
	{
		CHAR16 sIntelText[800];
		int intelcost;

		GetIntelInfoTextAndPrice( sIntelInfo[cnt], sIntelText, intelcost );

		if ( sIntelInfo[cnt] > -2 )
		{
			if ( !anyfound )
			{
				LaptopUiStateModel::CopyText(sText,
					szIntelWebsiteText[TEXT_INTEL_TEXT_1]);
				usPosY += DisplayWrappedString( usPosX, usPosY, LAPTOP_SCREEN_LR_X - LAPTOP_SCREEN_UL_X, 2, CAMPHIS_FONT_MED, FONT_BLACK, sText, FONT_MCOLOR_BLACK, FALSE, 0 );
				usPosY += 20;

				anyfound = TRUE;
			}

			// display bullet
			BltVideoObject( FRAME_BUFFER, hPixHandle, 0, usPosX, usPosY, VO_BLT_SRCTRANSPARENCY, NULL );

			sgp_swprintf(sText, 800,
				szIntelWebsiteText[TEXT_INTEL_OFFER_1], intelcost, sIntelText);
			UINT16 yoffset = DisplayWrappedString( usPosX + 25, usPosY, LAPTOP_SCREEN_LR_X - LAPTOP_SCREEN_UL_X - 25, 2,
				CAMPHIS_FONT_MED, ( sIntelInfo[cnt] > -1 ) ? ( intelcost <= budget ? FONT_BLACK : FONT_RED ) : FONT_GRAY4, sText, FONT_MCOLOR_BLACK, FALSE, 0 );

			if ( sIntelInfo[cnt] > -1 )
			{
				MSYS_DefineRegion( &gIntelmarketbuyLinkRegion[cnt], usPosX + 25, usPosY, (UINT16)( usPosX + 25 + StringPixLength( sText, CAMPHIS_FONT_MED ) ), usPosY + yoffset,
					MSYS_PRIORITY_HIGH, CURSOR_WWW, MSYS_NO_CALLBACK, BuyLinkRegionCallBack );
				if (!stagedResources.addRegion(
					gIntelmarketbuyLinkRegion[cnt])) return;
				MSYS_SetRegionUserData( &gIntelmarketbuyLinkRegion[cnt], 0, sIntelInfo[cnt] );
				MSYS_SetRegionUserData( &gIntelmarketbuyLinkRegion[cnt], 1, intelcost );
				gIntelmarketbuyLinkRegion_Exists[cnt] = TRUE;
			}

			usPosY += yoffset;
			usPosY += 20;
		}
	}

	if ( !anyfound )
	{
		LaptopUiStateModel::CopyText(sText,
			szIntelWebsiteText[TEXT_INTEL_TEXT_2]);
		DisplayWrappedString( usPosX, usPosY, LAPTOP_SCREEN_LR_X - LAPTOP_SCREEN_UL_X, 2, CAMPHIS_FONT_MED, FONT_BLACK, sText, FONT_MCOLOR_BLACK, FALSE, 0 );
	}

	SetFontShadow( DEFAULT_SHADOW );

	MarkButtonsDirty();
	RenderWWWProgramTitleBar();
	InvalidateRegion( LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y, LAPTOP_SCREEN_LR_X, LAPTOP_SCREEN_WEB_LR_Y );
	gIntelMarketBuyResources = std::move(stagedResources);
}
////////////////////////// INTELMARKET Sell intel for stuff ////////////////////////////////


////////////////////////// INTELMARKET About us ////////////////////////////////
void EnterIntelmarket_About()
{
	LaptopPageResourceOwner stagedResources;
	if (!LoadDefaults_IM(stagedResources)) return;
	gIntelMarketPageResources = std::move(stagedResources);

	RenderIntelmarket_About();
}

void ExitIntelmarket_About()
{
	gIntelMarketPageResources.clear();
}

void HandleIntelmarket_About()
{

}

void RenderIntelmarket_About()
{
	CHAR16		sText[800] = {};
	UINT16	usPosX, usPosY;
	HVOBJECT	hPixHandle;

	//Get the bullet
	GetVideoObject( &hPixHandle, guiMercCompareBulletImage );
	
	DisplayDefaults_IM();

	SetFontShadow( MERCOMP_FONT_SHADOW );

	usPosX = LAPTOP_SCREEN_UL_X;
	usPosY = MCA_START_CONTENT_Y;

	for ( int i = TEXT_INTEL_ABOUTUS_1; i < TEXT_INTEL_ABOUTUS_MAX; ++i )
	{
		// display bullet
		BltVideoObject( FRAME_BUFFER, hPixHandle, 0, usPosX, usPosY, VO_BLT_SRCTRANSPARENCY, NULL );

		LaptopUiStateModel::CopyText(sText, szIntelWebsiteText[i]);
		usPosY += DisplayWrappedString( usPosX + 25, usPosY, LAPTOP_SCREEN_LR_X - ( LAPTOP_SCREEN_UL_X + 25 ), 2, CAMPHIS_FONT_MED, MERCOMP_FONT_COLOR, sText, FONT_MCOLOR_BLACK, FALSE, 0 );
		usPosY += 4;
	}

	SetFontShadow( DEFAULT_SHADOW );

	MarkButtonsDirty();
	RenderWWWProgramTitleBar();
	InvalidateRegion( LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y, LAPTOP_SCREEN_LR_X, LAPTOP_SCREEN_WEB_LR_Y );
}
////////////////////////// INTELMARKET About us ////////////////////////////////

////////////////////////// INTELMARKET SELL ////////////////////////////////
CHAR16 gIntelPhotoTakenTextChar16[256][1000];
CHAR16 gIntelPhotoVerifiedTextChar16[256][1000];

std::vector<std::pair<INT16, STR16> > gPhotoFactsTakenVector;
std::vector<std::pair<INT16, STR16> > gPhotoFactsVerifiedVector;

INT16 gPhotoFactsCurrentTaken = -1;
INT16 gPhotoFactsCurrentVerified = -1;

INT32 gIntelSellButton[5] = { -1, -1, -1, -1, -1 };
BOOLEAN gIntelSellButtonCreated[5] = { FALSE, FALSE, FALSE, FALSE, FALSE };

void IntelSellButtonCallback( GUI_BUTTON *btn, INT32 reason )
{
	if ( reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= ( BUTTON_CLICKED_ON );
	}
	else if ( reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if ( btn->uiFlags & BUTTON_CLICKED_ON )
		{
			btn->uiFlags &= ~( BUTTON_CLICKED_ON );

			if ( btn->IDNum == gIntelSellButton[0] )
			{
				if (gPhotoFactsTakenVector.empty()) return;
				const auto current = LaptopUiStateModel::NormalizeIndex(
					gPhotoFactsTakenVector.size(), gPhotoFactsCurrentTaken);
				gPhotoFactsCurrentTaken = static_cast<INT16>(*current);
				gPhotoFactsCurrentTaken -= 1;
				if ( gPhotoFactsCurrentTaken < 0 )
					gPhotoFactsCurrentTaken = gPhotoFactsTakenVector.size() - 1;
			}
			else if ( btn->IDNum == gIntelSellButton[1] )
			{
				if (gPhotoFactsTakenVector.empty()) return;
				const auto current = LaptopUiStateModel::NormalizeIndex(
					gPhotoFactsTakenVector.size(), gPhotoFactsCurrentTaken);
				gPhotoFactsCurrentTaken = static_cast<INT16>(*current);
				gPhotoFactsCurrentTaken += 1;
				if ( gPhotoFactsCurrentTaken >= gPhotoFactsTakenVector.size() )
					gPhotoFactsCurrentTaken = 0;
			}
			else if ( btn->IDNum == gIntelSellButton[2] )
			{
				if (!LaptopUiStateModel::IsValidIndex(
					gPhotoFactsTakenVector.size(), gPhotoFactsCurrentTaken))
					return;
				INT16 factid = gPhotoFactsTakenVector[gPhotoFactsCurrentTaken].first;

				if ( factid >= 0 )
				{
					LuaSetPhotoState( factid, PHOTOFACT_UPLOADED );

					AddStrategicEvent( EVENT_INTEL_PHOTOFACT_VERIFY, GetWorldTotalMin() + ( ( 2 + Random( 3 ) ) * 60 ), (UINT32)factid );
				}
			}
			else if ( btn->IDNum == gIntelSellButton[3] )
			{
				if (gPhotoFactsVerifiedVector.empty()) return;
				const auto current = LaptopUiStateModel::NormalizeIndex(
					gPhotoFactsVerifiedVector.size(), gPhotoFactsCurrentVerified);
				gPhotoFactsCurrentVerified = static_cast<INT16>(*current);
				gPhotoFactsCurrentVerified -= 1;
				if ( gPhotoFactsCurrentVerified < 0 )
					gPhotoFactsCurrentVerified = gPhotoFactsVerifiedVector.size() - 1;
			}
			else if ( btn->IDNum == gIntelSellButton[4] )
			{
				if (gPhotoFactsVerifiedVector.empty()) return;
				const auto current = LaptopUiStateModel::NormalizeIndex(
					gPhotoFactsVerifiedVector.size(), gPhotoFactsCurrentVerified);
				gPhotoFactsCurrentVerified = static_cast<INT16>(*current);
				gPhotoFactsCurrentVerified += 1;
				if ( gPhotoFactsCurrentVerified >= gPhotoFactsVerifiedVector.size() )
					gPhotoFactsCurrentVerified = 0;
			}

			fIntelRedraw = TRUE;
		}
	}
}

void AddPhotoFactTaken(UINT8 aType, INT16 asIndex, STR16 aText)
{
	if (!LaptopUiStateModel::IsValidIndex(256, asIndex) || !aText) return;
	switch ( aType )
	{
	case PHOTOFACT_TAKEN:
		{
			LaptopUiStateModel::CopyText(
				gIntelPhotoTakenTextChar16[asIndex], aText);

			gPhotoFactsTakenVector.push_back( std::make_pair( asIndex, gIntelPhotoTakenTextChar16[asIndex] ) );
		}
		break;

	case PHOTOFACT_VERIFIED:
		{
			LaptopUiStateModel::CopyText(
				gIntelPhotoVerifiedTextChar16[asIndex], aText);
			
			gPhotoFactsVerifiedVector.push_back( std::make_pair( asIndex, gIntelPhotoVerifiedTextChar16[asIndex] ) );
		}
		break;

	default:
		break;
	}
}

void EnterIntelmarket_Sell()
{
	LaptopPageResourceOwner stagedResources;
	if (!LoadDefaults_IM(stagedResources)) return;
	gIntelMarketPageResources = std::move(stagedResources);

	fIntelRedraw = TRUE;

	HandleIntelmarket_Sell();
}

void ExitIntelmarket_Sell()
{
	gIntelMarketSellResources.clear();
	std::fill(std::begin(gIntelSellButton), std::end(gIntelSellButton), -1);
	std::fill(std::begin(gIntelSellButtonCreated),
		std::end(gIntelSellButtonCreated), FALSE);
	gIntelMarketPageResources.clear();
}

void HandleIntelmarket_Sell()
{
	if ( fIntelRedraw )
	{
		// fill dropdowns
		gPhotoFactsTakenVector.clear();
		gPhotoFactsVerifiedVector.clear();

		// fill vector with list of all uploadable facts
		LuaGetPhotoData( PHOTOFACT_TAKEN );

		// fill vector with list of all verified facts
		LuaGetPhotoData( PHOTOFACT_VERIFIED );
		
		const auto taken = LaptopUiStateModel::NormalizeIndex(
			gPhotoFactsTakenVector.size(), gPhotoFactsCurrentTaken);
		const auto verified = LaptopUiStateModel::NormalizeIndex(
			gPhotoFactsVerifiedVector.size(), gPhotoFactsCurrentVerified);
		gPhotoFactsCurrentTaken = taken ? static_cast<INT16>(*taken) : -1;
		gPhotoFactsCurrentVerified =
			verified ? static_cast<INT16>(*verified) : -1;

		RenderIntelmarket_Sell();

		fIntelRedraw = FALSE;
	}
}

void RenderIntelmarket_Sell()
{
	CHAR16		sText[800] = {};
	UINT16	usPosX, usPosY;
	HVOBJECT	hPixHandle;

	gIntelMarketSellResources.clear();
	std::fill(std::begin(gIntelSellButton), std::end(gIntelSellButton), -1);
	std::fill(std::begin(gIntelSellButtonCreated),
		std::end(gIntelSellButtonCreated), FALSE);
	LaptopPageResourceOwner stagedResources;

	//Get the bullet
	if (!GetVideoObject(&hPixHandle, guiMercCompareBulletImage)) return;

	DisplayDefaults_IM();

	SetFontShadow( MERCOMP_FONT_SHADOW );
	ScopedDefaultFontShadow restoreFontShadow;

	usPosX = LAPTOP_SCREEN_UL_X;
	usPosY = MCA_START_CONTENT_Y;

	FLOAT budget = GetIntel();

	sgp_swprintf(sText, 800, szIntelWebsiteText[TEXT_INTEL_BUDGET],
		(int)budget);
	usPosY += DisplayWrappedString( usPosX, usPosY, LAPTOP_SCREEN_LR_X - LAPTOP_SCREEN_UL_X, 2, CAMPHIS_FONT_MED, FONT_BLACK, sText, FONT_MCOLOR_BLACK, FALSE, 0 );
	usPosY += 12;
	
	if ( gPhotoFactsCurrentTaken >= 0 && !gPhotoFactsTakenVector.empty() )
	{
		LaptopUiStateModel::CopyText(sText,
			szIntelWebsiteText[TEXT_INTEL_SELL_1]);
		usPosY += DisplayWrappedString( usPosX, usPosY, LAPTOP_SCREEN_LR_X - usPosX, 2, CAMPHIS_FONT_MED, FONT_BLACK, sText, FONT_MCOLOR_BLACK, FALSE, 0 );
		usPosY += 12;

		if (!stagedResources.addButton(CreateTextButton(
			szIntelWebsiteText[TEXT_INTEL_SELL_BUTTON_1], CAMPHIS_FONT_MED,
			FONT_BLACK, FONT_BLACK, BUTTON_USE_DEFAULT, usPosX, usPosY,
			80, 20, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH, DEFAULT_MOVE_CALLBACK,
			IntelSellButtonCallback), gIntelSellButton[0])) return;
		gIntelSellButtonCreated[0] = TRUE;

		if (!stagedResources.addButton(CreateTextButton(
			szIntelWebsiteText[TEXT_INTEL_SELL_BUTTON_2], CAMPHIS_FONT_MED,
			FONT_BLACK, FONT_BLACK, BUTTON_USE_DEFAULT, usPosX, usPosY + 20,
			80, 20, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH, DEFAULT_MOVE_CALLBACK,
			IntelSellButtonCallback), gIntelSellButton[1])) return;
		gIntelSellButtonCreated[1] = TRUE;

		if (!stagedResources.addButton(CreateTextButton(
			szIntelWebsiteText[TEXT_INTEL_SELL_BUTTON_3], CAMPHIS_FONT_MED,
			FONT_BLACK, FONT_BLACK, BUTTON_USE_DEFAULT, usPosX, usPosY + 40,
			80, 20, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH, DEFAULT_MOVE_CALLBACK,
			IntelSellButtonCallback), gIntelSellButton[2])) return;
		gIntelSellButtonCreated[2] = TRUE;
		
		LaptopUiStateModel::CopyText(
			sText, gPhotoFactsTakenVector[gPhotoFactsCurrentTaken].second);
		usPosY += DisplayWrappedString( usPosX + 100, usPosY, LAPTOP_SCREEN_LR_X - (usPosX + 100), 2, CAMPHIS_FONT_MED, FONT_BLACK, sText, FONT_MCOLOR_BLACK, FALSE, 0 );
		usPosY += 150;
	}
	else
	{
		LaptopUiStateModel::CopyText(sText,
			szIntelWebsiteText[TEXT_INTEL_SELL_NOTHING_1]);
		usPosY += DisplayWrappedString( usPosX, usPosY, LAPTOP_SCREEN_LR_X - usPosX, 2, CAMPHIS_FONT_MED, FONT_BLACK, sText, FONT_MCOLOR_BLACK, FALSE, 0 );
		usPosY += 12;
	}
	
	if ( gPhotoFactsCurrentVerified >= 0 && !gPhotoFactsVerifiedVector.empty() )
	{
		LaptopUiStateModel::CopyText(sText,
			szIntelWebsiteText[TEXT_INTEL_SELL_ALREADYGOT_1]);
		usPosY += DisplayWrappedString( usPosX, usPosY, LAPTOP_SCREEN_LR_X - usPosX, 2, CAMPHIS_FONT_MED, FONT_BLACK, sText, FONT_MCOLOR_BLACK, FALSE, 0 );
		usPosY += 12;

		if (!stagedResources.addButton(CreateTextButton(
			szIntelWebsiteText[TEXT_INTEL_SELL_BUTTON_1], CAMPHIS_FONT_MED,
			FONT_BLACK, FONT_BLACK, BUTTON_USE_DEFAULT, usPosX, usPosY,
			80, 20, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH, DEFAULT_MOVE_CALLBACK,
			IntelSellButtonCallback), gIntelSellButton[3])) return;
		gIntelSellButtonCreated[3] = TRUE;

		if (!stagedResources.addButton(CreateTextButton(
			szIntelWebsiteText[TEXT_INTEL_SELL_BUTTON_2], CAMPHIS_FONT_MED,
			FONT_BLACK, FONT_BLACK, BUTTON_USE_DEFAULT, usPosX, usPosY + 20,
			80, 20, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH, DEFAULT_MOVE_CALLBACK,
			IntelSellButtonCallback), gIntelSellButton[4])) return;
		gIntelSellButtonCreated[4] = TRUE;
		
		LaptopUiStateModel::CopyText(
			sText, gPhotoFactsVerifiedVector[gPhotoFactsCurrentVerified].second);
		DisplayWrappedString( usPosX + 100, usPosY, LAPTOP_SCREEN_LR_X - (usPosX + 100), 2, CAMPHIS_FONT_MED, FONT_BLACK, sText, FONT_MCOLOR_BLACK, FALSE, 0 );
	}

	SetFontShadow( DEFAULT_SHADOW );

	MarkButtonsDirty();
	RenderWWWProgramTitleBar();
	InvalidateRegion( LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y, LAPTOP_SCREEN_LR_X, LAPTOP_SCREEN_WEB_LR_Y );
	gIntelMarketSellResources = std::move(stagedResources);
}
////////////////////////// INTELMARKET SELL ////////////////////////////////
