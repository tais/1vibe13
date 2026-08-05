	#include "laptop.h"
	#include "BobbyRAmmo.h"
	#include "BobbyRGuns.h"
	#include "BobbyR.h"
	#include "LaptopPageResourceOwner.h"
	#include "Utilities.h"
	#include "WCheck.h"
	#include "WordWrap.h"
	#include "Encrypted File.h"
	#include "Text.h"
	#include <utility>

UINT32		guiAmmoBackground;
UINT32		guiAmmoGrid;

namespace
{
LaptopPageResourceOwner gBobbyRAmmoResources;
}

BOOLEAN DisplayAmmoInfo();


void GameInitBobbyRAmmo()
{
}

BOOLEAN EnterBobbyRAmmo()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner staged;
	DeleteMouseRegionForBigImage();
	gBobbyRAmmoResources.clear();

	//gfBigImageMouseRegionCreated = FALSE;

	// load the background graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\ammobackground.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiAmmoBackground)) return FALSE;

	// load the gunsgrid graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\ammogrid.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiAmmoGrid)) return FALSE;

	if (!InitBobbyBrTitle(staged)) return FALSE;

	guiPrevAmmoFilterMode = -1;
	guiCurrentAmmoFilterMode = -1;

	SetFirstLastPagesForNew( IC_AMMO, guiCurrentAmmoFilterMode );

	//Draw menu bar
	if (!InitBobbyMenuBar(staged) ||
		!InitBobbyRAmmoFilterBar(staged)) return FALSE;
	gBobbyRAmmoResources = std::move(staged);

	RenderBobbyRAmmo( );

	return(TRUE);
}

void ExitBobbyRAmmo()
{
	DeleteMouseRegionForBigImage();
	gBobbyRAmmoResources.clear();

	giCurrentSubPage = gusCurWeaponIndex;
	guiLastBobbyRayPage = LAPTOP_MODE_BOBBY_R_AMMO;
}

void HandleBobbyRAmmo()
{
	HandleBobbyRGuns();
}

void RenderBobbyRAmmo()
{
	HVOBJECT hPixHandle;
	const BobbyRayLayoutModel::CatalogueLayout layout =
		GetBobbyRayCatalogueLayout();

	DrawBobbyRayCatalogueBackground(guiAmmoBackground);

	//Display title at top of page
	//DisplayBobbyRBrTitle();

	// GunForm
	GetVideoObject(&hPixHandle, guiAmmoGrid);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0,
		layout.catalogueGrid.x, layout.catalogueGrid.y,
		VO_BLT_SRCTRANSPARENCY,NULL);

	DisplayItemInfo(IC_AMMO, guiCurrentAmmoFilterMode);
	UpdateButtonText(guiCurrentLaptopMode);

	UpdateAmmoFilterButtons();

	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
	//Moa removed below. See comment above LAPTOP_SCREEN_UL_X in laptop.h
	//	fReDrawScreenFlag = TRUE;
	//fPausedReDrawScreenFlag = TRUE;
}









