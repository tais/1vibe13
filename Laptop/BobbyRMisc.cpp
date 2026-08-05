	#include "laptop.h"
	#include "BobbyRMisc.h"
	#include "BobbyR.h"
	#include "BobbyRGuns.h"
	#include "LaptopPageResourceOwner.h"
	#include "Utilities.h"
	#include "WCheck.h"
	#include "WordWrap.h"
	#include "Text.h"
	#include <utility>


UINT32		guiMiscBackground;
UINT32		guiMiscGrid;

namespace
{
LaptopPageResourceOwner gBobbyRMiscResources;
}



void GameInitBobbyRMisc()
{

}

BOOLEAN EnterBobbyRMisc()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner staged;
	DeleteMouseRegionForBigImage();
	gBobbyRMiscResources.clear();

	// load the background graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\miscbackground.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiMiscBackground)) return FALSE;

	// load the gunsgrid graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\miscgrid.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiMiscGrid)) return FALSE;

	if (!InitBobbyBrTitle(staged)) return FALSE;

	guiPrevMiscFilterMode = -1;
	guiCurrentMiscFilterMode = -1;
	guiCurrentMiscSubFilterMode = -1;
	guiPrevMiscSubFilterMode = -1;

	SetFirstLastPagesForNew( IC_BOBBY_MISC, guiCurrentMiscFilterMode, guiCurrentMiscSubFilterMode );

	//Draw menu bar
	if (!InitBobbyMenuBar(staged)) return FALSE;

	if (!InitBobbyRMiscFilterBar(staged)) return FALSE;
	gBobbyRMiscResources = std::move(staged);

//	CalculateFirstAndLastIndexs();

	RenderBobbyRMisc( );

	return(TRUE);
}

void ExitBobbyRMisc()
{
	DeleteMouseRegionForBigImage();
	gBobbyRMiscResources.clear();

	guiLastBobbyRayPage = LAPTOP_MODE_BOBBY_R_MISC;
}

void HandleBobbyRMisc()
{
	HandleBobbyRGuns();
}

void RenderBobbyRMisc()
{
	HVOBJECT hPixHandle;
	const BobbyRayLayoutModel::CatalogueLayout layout =
		GetBobbyRayCatalogueLayout();

	DrawBobbyRayCatalogueBackground(guiMiscBackground);

	//Display title at top of page
	//DisplayBobbyRBrTitle();

	// GunForm
	GetVideoObject(&hPixHandle, guiMiscGrid);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0,
		layout.catalogueGrid.x, layout.catalogueGrid.y,
		VO_BLT_SRCTRANSPARENCY,NULL);

	DisplayItemInfo(IC_BOBBY_MISC, guiCurrentMiscFilterMode, guiCurrentMiscSubFilterMode);
	UpdateButtonText(guiCurrentLaptopMode);
	UpdateMiscFilterButtons();

	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
	//Moa removed below. See comment above LAPTOP_SCREEN_UL_X in laptop.h
	//	fReDrawScreenFlag = TRUE;
	//fPausedReDrawScreenFlag = TRUE;
}











