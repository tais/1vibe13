	#include "laptop.h"
	#include "AimLinks.h"
	#include "aim.h"
	#include "WCheck.h"
	#include "WordWrap.h"
	#include "TextCatalog.h"
	#include "Multi Language Graphic Utils.h"
	#include "CampaignAimSitePolicy.h"
	#include "GameContext.h"
	#include "ub_config.h"
	#include "LaptopPageResourceOwner.h"

#include <array>

#define		AIM_LINK_TITLE_FONT						FONT14ARIAL
#define		AIM_LINK_TITLE_COLOR					AIM_GREEN

#define		AIM_LINK_LINK_OFFSET_Y				94//90

#define		AIM_LINK_LINK_WIDTH						420
#define		AIM_LINK_LINK_HEIGHT					70

#define		AIM_LINK_BOBBY_LINK_X					LAPTOP_SCREEN_UL_X + 40
#define		AIM_LINK_BOBBY_LINK_Y					LAPTOP_SCREEN_WEB_UL_Y + 91

#define		AIM_LINK_TITLE_X							IMAGE_OFFSET_X + 149
#define		AIM_LINK_TITLE_Y							AIM_SYMBOL_Y + AIM_SYMBOL_SIZE_Y + 10
#define		AIM_LINK_TITLE_WIDTH					AIM_SYMBOL_WIDTH

enum class AimLink : UINT8
{
	BobbyRay,
	Funeral,
	Insurance,
	Count
};

static constexpr auto AIM_LINK_COUNT =
	static_cast<std::size_t>(AimLink::Count);
static constexpr std::array<UINT16, AIM_LINK_COUNT> AIM_LINK_GRAPHICS = {
	MLG_BOBBYRAYLINK,
	MLG_MORTUARYLINK,
	MLG_INSURANCELINK};
static constexpr std::array<UINT8, AIM_LINK_COUNT> AIM_LINK_PAGES = {
	BOBBYR_BOOKMARK,
	FUNERAL_BOOKMARK,
	INSURANCE_BOOKMARK};

static std::array<UINT32, AIM_LINK_COUNT> gAimLinkImages;
static std::array<MOUSE_REGION, AIM_LINK_COUNT> gAimLinkRegions;
static std::array<BOOLEAN, AIM_LINK_COUNT> gAimLinkEnabled;
static LaptopPageResourceOwner gAimLinkResources;

static bool IsUnfinishedBusinessLinkConfigured(AimLink link)
{
	switch (link)
	{
		case AimLink::BobbyRay:
			return gGameUBOptions.LaptopLinkBobby == TRUE;
		case AimLink::Funeral:
			return gGameUBOptions.LaptopLinkFuneral == TRUE;
		case AimLink::Insurance:
			return gGameUBOptions.LaptopLinkInsurance == TRUE;
		case AimLink::Count:
		default:
			return false;
	}
}

static void RefreshAimLinkAvailability()
{
	const CampaignAimSitePolicy aimSitePolicy(
		GetGameContext().capabilities());
	for (std::size_t index = 0; index < AIM_LINK_COUNT; ++index)
	{
		gAimLinkEnabled[index] = aimSitePolicy.linkEnabled(
			IsUnfinishedBusinessLinkConfigured(
				static_cast<AimLink>(index)));
	}
}

static UINT16 AimLinkY(std::size_t linkIndex)
{
	return static_cast<UINT16>(
		AIM_LINK_BOBBY_LINK_Y + linkIndex * AIM_LINK_LINK_OFFSET_Y);
}

//Clicking on guys Face
void SelectLinkRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );


void GameInitAimLinks()
{

}

BOOLEAN EnterAimLinks()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner stagedResources;

	gAimLinkResources.clear();
	RefreshAimLinkAvailability();

	for (std::size_t index = 0; index < AIM_LINK_COUNT; ++index)
	{
		if (!gAimLinkEnabled[index])
			continue;

		VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
		GetMLGFilename(VObjectDesc.ImageFile, AIM_LINK_GRAPHICS[index]);
		CHECKF(stagedResources.addVideoObject(
			&VObjectDesc, gAimLinkImages[index]));

		const UINT16 linkY = AimLinkY(index);
		MSYS_DefineRegion( &gAimLinkRegions[index], AIM_LINK_BOBBY_LINK_X, linkY , AIM_LINK_BOBBY_LINK_X + AIM_LINK_LINK_WIDTH, (UINT16)(linkY + AIM_LINK_LINK_HEIGHT), MSYS_PRIORITY_HIGH,
								CURSOR_WWW, MSYS_NO_CALLBACK, SelectLinkRegionCallBack );
		CHECKF(stagedResources.addRegion(gAimLinkRegions[index]));
		MSYS_SetRegionUserData(
			&gAimLinkRegions[index], 0, AIM_LINK_PAGES[index]);
	}

	CHECKF(InitAimDefaults());
	if (!InitAimMenuBar())
	{
		RemoveAimDefaults();
		return FALSE;
	}
	gAimLinkResources = std::move(stagedResources);
	RenderAimLinks();
	return(TRUE);
}

void ExitAimLinks()
{
	gAimLinkResources.clear();
	RemoveAimDefaults();
	ExitAimMenuBar();

}

void HandleAimLinks()
{

}

void RenderAimLinks()
{
	HVOBJECT hPixHandle;

	DrawAimDefaults();
	DisableAimButton();

	for (std::size_t index = 0; index < AIM_LINK_COUNT; ++index)
	{
		if (!gAimLinkEnabled[index])
			continue;
		GetVideoObject(&hPixHandle, gAimLinkImages[index]);
		BltVideoObject(FRAME_BUFFER, hPixHandle, 0,
			AIM_LINK_BOBBY_LINK_X, AimLinkY(index),
			VO_BLT_SRCTRANSPARENCY, NULL);
	}
	
	//Draw Link Title
	DrawTextToScreen(
		i18n::GetCompiledTextPack().text(i18n::TextKey::AimLinksTitle).data(),
		AIM_LINK_TITLE_X, AIM_LINK_TITLE_Y, AIM_LINK_TITLE_WIDTH,
		AIM_LINK_TITLE_FONT, AIM_LINK_TITLE_COLOR, FONT_MCOLOR_BLACK, FALSE,
		CENTER_JUSTIFIED);

	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}

void SelectLinkRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_DWN)
	{
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		UINT32	gNextLaptopPage;

		gNextLaptopPage = MSYS_GetRegionUserData( pRegion, 0 );

		GoToWebPage( gNextLaptopPage );
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
	}
}









