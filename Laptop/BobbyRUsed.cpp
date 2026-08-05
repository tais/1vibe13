	#include "laptop.h"
	#include "BobbyRUsed.h"
	#include "BobbyR.h"
	#include "BobbyRGuns.h"
	#include "LaptopPageResourceOwner.h"
	#include "Utilities.h"
	#include "WCheck.h"
	#include "WordWrap.h"
	#include "Text.h"
	#include <utility>

UINT32		guiUsedBackground;
UINT32		guiUsedGrid;

namespace
{
LaptopPageResourceOwner gBobbyRUsedResources;
}


void GameInitBobbyRUsed()
{

}

BOOLEAN EnterBobbyRUsed()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner staged;
	DeleteMouseRegionForBigImage();
	gBobbyRUsedResources.clear();

	//gfBigImageMouseRegionCreated = FALSE;

	// load the background graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\usedbackground.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiUsedBackground)) return FALSE;

	// load the gunsgrid graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\usedgrid.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiUsedGrid)) return FALSE;

	if (!InitBobbyBrTitle(staged)) return FALSE;

	guiPrevUsedFilterMode = -1;
	guiCurrentUsedFilterMode = -1;

	SetFirstLastPagesForUsed(guiCurrentUsedFilterMode);

	//Draw menu bar
	if (!InitBobbyMenuBar(staged) ||
		!InitBobbyRUsedFilterBar(staged)) return FALSE;
	gBobbyRUsedResources = std::move(staged);

	RenderBobbyRUsed( );

	return(TRUE);
}

void ExitBobbyRUsed()
{
	DeleteMouseRegionForBigImage();
	gBobbyRUsedResources.clear();

	giCurrentSubPage = gusCurWeaponIndex;
	guiLastBobbyRayPage = LAPTOP_MODE_BOBBY_R_USED;
}

void HandleBobbyRUsed()
{
	HandleBobbyRGuns();
}

void RenderBobbyRUsed()
{
	HVOBJECT hPixHandle;
	const BobbyRayLayoutModel::CatalogueLayout layout =
		GetBobbyRayCatalogueLayout();

	DrawBobbyRayCatalogueBackground(guiUsedBackground);

	//Display title at top of page
	//DisplayBobbyRBrTitle();

	// GunForm
	GetVideoObject(&hPixHandle, guiUsedGrid);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0,
		layout.catalogueGrid.x, layout.catalogueGrid.y,
		VO_BLT_SRCTRANSPARENCY,NULL);

	DisplayItemInfo(BOBBYR_USED_ITEMS, guiCurrentUsedFilterMode);

	UpdateButtonText(guiCurrentLaptopMode);
	UpdateUsedFilterButtons();

	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
	//Moa removed below. See comment above LAPTOP_SCREEN_UL_X in laptop.h
	//	fReDrawScreenFlag = TRUE;
	//fPausedReDrawScreenFlag = TRUE;
}










