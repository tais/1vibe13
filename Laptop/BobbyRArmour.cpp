	#include "laptop.h"
	#include "BobbyRArmour.h"
	#include "BobbyRGuns.h"
	#include "BobbyR.h"
	#include "LaptopPageResourceOwner.h"
	#include "Utilities.h"
	#include "WCheck.h"
	#include "WordWrap.h"
	#include "Text.h"
	#include <utility>


UINT32		guiArmourBackground;
UINT32		guiArmourGrid;

namespace
{
LaptopPageResourceOwner gBobbyRArmourResources;
}



void GameInitBobbyRArmour()
{

}

BOOLEAN EnterBobbyRArmour()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner staged;
	DeleteMouseRegionForBigImage();
	gBobbyRArmourResources.clear();

	// load the background graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\Armourbackground.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiArmourBackground)) return FALSE;

	// load the gunsgrid graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\Armourgrid.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiArmourGrid)) return FALSE;

	if (!InitBobbyBrTitle(staged)) return FALSE;

	guiPrevArmourFilterMode = -1;
	guiCurrentArmourFilterMode = -1;

	SetFirstLastPagesForNew( IC_ARMOUR, guiCurrentArmourFilterMode );

	//Draw menu bar
	if (!InitBobbyMenuBar(staged)) return FALSE;

	if (!InitBobbyRArmourFilterBar(staged)) return FALSE;
	gBobbyRArmourResources = std::move(staged);

	RenderBobbyRArmour( );

	return(TRUE);
}

void ExitBobbyRArmour()
{
	DeleteMouseRegionForBigImage();
	gBobbyRArmourResources.clear();

	giCurrentSubPage = gusCurWeaponIndex;
	guiLastBobbyRayPage = LAPTOP_MODE_BOBBY_R_ARMOR;
}

void HandleBobbyRArmour()
{
	HandleBobbyRGuns();
}

void RenderBobbyRArmour()
{

	HVOBJECT hPixHandle;
	const BobbyRayLayoutModel::CatalogueLayout layout =
		GetBobbyRayCatalogueLayout();

	DrawBobbyRayCatalogueBackground(guiArmourBackground);

	//Display title at top of page
	//DisplayBobbyRBrTitle();

	// GunForm
	GetVideoObject(&hPixHandle, guiArmourGrid);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0,
		layout.catalogueGrid.x, layout.catalogueGrid.y,
		VO_BLT_SRCTRANSPARENCY,NULL);

	DisplayItemInfo(IC_ARMOUR, guiCurrentArmourFilterMode);

	UpdateButtonText(guiCurrentLaptopMode);
	// TODO

	UpdateArmourFilterButtons();

	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
	//Moa removed below. See comment above LAPTOP_SCREEN_UL_X in laptop.h
	//	fReDrawScreenFlag = TRUE;
	//fPausedReDrawScreenFlag = TRUE;
}











