	#include "laptop.h"
	#include "BobbyR.h"
	#include "BobbyRGuns.h"
	#include "BobbyRMailOrder.h"
	#include "BobbyRayLayout.h"
	#include "LaptopPageResourceOwner.h"
	#include "Utilities.h"
	#include "WordWrap.h"
	#include "Cursors.h"
	#include "Interface Items.h"
	#include "Weapons.h"
	#include "Store Inventory.h"
	#include "Game Event Hook.h"
	#include "Game Clock.h"
	#include "LaptopSave.h"
	#include "random.h"
	#include "Text.h"
	#include "Timer Control.h"
	#include "Multi Language Graphic Utils.h"
//	#include "Utility.h"
	#include "ArmsDealerInvInit.h"
	#include "GameSettings.h"
	#include "message.h"
	#include "PostalService.h"


#ifdef JA2TESTVERSION
	#define BR_INVENTORY_TURNOVER_DEBUG
#endif


#define BOBBIES_SIGN_FONT							FONT14ARIAL
#define BOBBIES_SIGN_COLOR						2
#define BOBBIES_SIGN_BACKCOLOR				FONT_MCOLOR_BLACK
#define BOBBIES_SIGN_BACKGROUNDCOLOR				78

#define BOBBIES_NUMBER_SIGNS					5

#define BOBBIES_SENTENCE_FONT					FONT12ARIAL
#define BOBBIES_SENTENCE_COLOR				FONT_MCOLOR_WHITE
#define BOBBIES_SENTENCE_BACKGROUNDCOLOR			2

#define BOBBY_R_NEW_PURCHASE_ARRIVAL_TIME		(1 * 60 * 24) // minutes in 1 day

#define	BOBBY_R_USED_PURCHASE_OFFSET		MAXITEMS

#define	BOBBYR_UNDERCONSTRUCTION_ANI_DELAY		150
#define	BOBBYR_UNDERCONSTRUCTION_NUM_FRAMES		5

namespace
{
BobbyRayLayoutModel::Anchors BobbyRayAnchors()
{
	return {
		LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y,
		LAPTOP_SCREEN_LR_X, LAPTOP_SCREEN_WEB_LR_Y,
		iScreenWidthOffset, iScreenHeightOffset,
		LAPTOP_SCREEN_WEB_DELTA_Y};
}

BobbyRayLayoutModel::HomeLayout BobbyRayHomeLayout()
{
	return BobbyRayLayoutModel::MakeHomeLayout(BobbyRayAnchors());
}
}



UINT32	guiBobbyName;
UINT32	guiPlaque;
UINT32	guiTopHinge;
UINT32	guiBottomHinge;
UINT32	guiStorePlaque;
UINT32	guiHandle;
UINT32	guiWoodBackground;
UINT32	guiUnderConstructionImage;

LaptopPageResourceOwner gBobbyRResources;

/*
UINT16	gusFirstGunIndex;
UINT16	gusLastGunIndex;
UINT8		gubNumGunPages;

UINT16	gusFirstAmmoIndex;
UINT16	gusLastAmmoIndex;
UINT8		gubNumAmmoPages;

UINT16	gusFirstMiscIndex;
UINT16	gusLastMiscIndex;
UINT8		gubNumMiscPages;

UINT16	gusFirstArmourIndex;
UINT16	gusLastArmourIndex;
UINT8		gubNumArmourPages;

UINT16	gusFirstUsedIndex;
UINT16	gusLastUsedIndex;
UINT8		gubNumUsedPages;
*/

UINT32	guiLastBobbyRayPage;



UINT8		gubBobbyRPages[]={
						LAPTOP_MODE_BOBBY_R_USED,
						LAPTOP_MODE_BOBBY_R_MISC,
						LAPTOP_MODE_BOBBY_R_GUNS,
						LAPTOP_MODE_BOBBY_R_AMMO,
						LAPTOP_MODE_BOBBY_R_ARMOR};

//Dealtar's Airport Externalization.
extern CPostalService gPostalService;
extern vector < PShipmentStruct > gShipmentTable;
extern void BobbyRDeliveryCallback(RefToCShipmentManipulator ShipmentManipulator);
//End Dealtar's Airport Externalization.

//Bobby's Sign menu mouse regions
MOUSE_REGION	gSelectedBobbiesSignMenuRegion[ BOBBIES_NUMBER_SIGNS ];
void SelectBobbiesSignMenuRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

BOOLEAN InitBobbiesMouseRegion(MOUSE_REGION *MouseRegion,
	LaptopPageResourceOwner& owner);
void HandleBobbyRUnderConstructionAni( BOOLEAN fReset );

void SimulateBobbyRayCustomer(STORE_INVENTORY *pInventoryArray, BOOLEAN fUsed);



void GameInitBobbyR()
{
	//Dealtar's Airport Externalization.
	//Originally, this function was empty!
	gPostalService.RegisterDeliveryCallback(0, BobbyRDeliveryCallback);
}


BOOLEAN EnterBobbyR()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner staged;
	UINT8 i;

	gBobbyRResources.clear();
	RefreshBobbyRayDestinationSnapshot();
	RefreshBobbyRayShipmentSnapshot();

	if (!InitBobbyRWoodBackground(staged)) return FALSE;

	// load the Bobbyname graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	GetMLGFilename( VObjectDesc.ImageFile, MLG_BOBBYNAME );
	if (!staged.addVideoObject(&VObjectDesc, guiBobbyName)) return FALSE;

	// load the plaque graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BobbyPlaques.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiPlaque)) return FALSE;

	// load the TopHinge graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BobbyTopHinge.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiTopHinge)) return FALSE;

	// load the BottomHinge graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BobbyBottomHinge.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiBottomHinge)) return FALSE;

	// load the Store Plaque graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	GetMLGFilename( VObjectDesc.ImageFile, MLG_STOREPLAQUE );
	if (!staged.addVideoObject(&VObjectDesc, guiStorePlaque)) return FALSE;

	// load the Handle graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BobbyHandle.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiHandle)) return FALSE;


	if (!InitBobbiesMouseRegion(
		gSelectedBobbiesSignMenuRegion, staged)) return FALSE;


	if( !LaptopSaveInfo.fBobbyRSiteCanBeAccessed )
	{
		// load the Handle graphic and add it
		VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
		FilenameForBPP("LAPTOP\\UnderConstruction.sti", VObjectDesc.ImageFile);
		if (!staged.addVideoObject(&VObjectDesc,
			guiUnderConstructionImage)) return FALSE;

		for(i=0; i<BOBBIES_NUMBER_SIGNS; i++)
		{
			MSYS_DisableRegion( &gSelectedBobbiesSignMenuRegion[i] );
		}

		LaptopSaveInfo.ubHaveBeenToBobbyRaysAtLeastOnceWhileUnderConstruction = BOBBYR_BEEN_TO_SITE_ONCE;
	}
	gBobbyRResources = std::move(staged);


	SetBookMark(BOBBYR_BOOKMARK);
	HandleBobbyRUnderConstructionAni( TRUE );

	RenderBobbyR();

	return( TRUE );
}

void ExitBobbyR()
{
	gBobbyRResources.clear();

	guiLastBobbyRayPage = LAPTOP_MODE_BOBBY_R;
}

void HandleBobbyR()
{
	HandleBobbyRUnderConstructionAni( FALSE );
}

void RenderBobbyR()
{
	HVOBJECT hPixHandle;
	HVOBJECT hStorePlaqueHandle;
	const BobbyRayLayoutModel::HomeLayout layout = BobbyRayHomeLayout();

	DrawBobbyRWoodBackground();

	// Bobby's Name
	GetVideoObject(&hPixHandle, guiBobbyName);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0,
		layout.name.x, layout.name.y, VO_BLT_SRCTRANSPARENCY,NULL);

	// Plaque
	GetVideoObject(&hPixHandle, guiPlaque);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0,
		layout.plaques.x, layout.plaques.y, VO_BLT_SRCTRANSPARENCY,NULL);

	// Top Hinge
	GetVideoObject(&hPixHandle, guiTopHinge);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0,
		layout.topHinge.x, layout.topHinge.y, VO_BLT_SRCTRANSPARENCY,NULL);

	// Bottom Hinge
	GetVideoObject(&hPixHandle, guiBottomHinge);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0,
		layout.bottomHinge.x, layout.bottomHinge.y, VO_BLT_SRCTRANSPARENCY,NULL);

	// StorePlaque
	GetVideoObject(&hStorePlaqueHandle, guiStorePlaque);
	BltVideoObject(FRAME_BUFFER, hStorePlaqueHandle, 0,
		layout.storePlaque.x, layout.storePlaque.y,
		VO_BLT_SRCTRANSPARENCY,NULL);

	// Handle
	GetVideoObject(&hPixHandle, guiHandle);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0,
		layout.handle.x, layout.handle.y, VO_BLT_SRCTRANSPARENCY,NULL);

/*
	if( !LaptopSaveInfo.fBobbyRSiteCanBeAccessed )
	{
		// The undercontsruction graphic
		GetVideoObject(&hPixHandle, guiUnderConstructionImage );
		BltVideoObject(FRAME_BUFFER, hPixHandle, 0,BOBBIES_FIRST_SENTENCE_X, BOBBIES_FIRST_SENTENCE_Y, VO_BLT_SRCTRANSPARENCY,NULL);
		BltVideoObject(FRAME_BUFFER, hPixHandle, 0,BOBBIES_3RD_SENTENCE_X, BOBBIES_3RD_SENTENCE_Y, VO_BLT_SRCTRANSPARENCY,NULL);
	}
*/

	SetFontShadow(BOBBIES_SENTENCE_BACKGROUNDCOLOR);


	if( LaptopSaveInfo.fBobbyRSiteCanBeAccessed )
	{
		//Bobbys first sentence
	//	ShadowText( FRAME_BUFFER, BobbyRaysFrontText[BOBBYR_ADVERTISMENT_1], BOBBIES_SENTENCE_FONT, BOBBIES_FIRST_SENTENCE_X, BOBBIES_FIRST_SENTENCE_Y );
		DrawTextToScreen(BobbyRaysFrontText[BOBBYR_ADVERTISMENT_1],
			layout.advertisements[0].origin.x,
			layout.advertisements[0].origin.y,
			layout.advertisements[0].width, BOBBIES_SENTENCE_FONT,
			BOBBIES_SENTENCE_COLOR, BOBBIES_SIGN_BACKCOLOR, FALSE,
			CENTER_JUSTIFIED | TEXT_SHADOWED );

		//Bobbys second sentence
		DrawTextToScreen(BobbyRaysFrontText[BOBBYR_ADVERTISMENT_2],
			layout.advertisements[1].origin.x,
			layout.advertisements[1].origin.y,
			layout.advertisements[1].width, BOBBIES_SENTENCE_FONT,
			BOBBIES_SENTENCE_COLOR, BOBBIES_SIGN_BACKCOLOR, FALSE,
			CENTER_JUSTIFIED | TEXT_SHADOWED );
		SetFontShadow(DEFAULT_SHADOW);
	}


	SetFontShadow(BOBBIES_SIGN_BACKGROUNDCOLOR);
	for (std::size_t index = 0; index < layout.signs.size(); ++index)
	{
		const BobbyRayLayoutModel::HomeSign& sign = layout.signs[index];
		DisplayWrappedString(sign.label.origin.x, sign.label.origin.y,
			sign.label.width, 2, BOBBIES_SIGN_FONT, BOBBIES_SIGN_COLOR,
			BobbyRaysFrontText[BOBBYR_USED + index],
			BOBBIES_SIGN_BACKCOLOR, FALSE, CENTER_JUSTIFIED);
	}
	SetFontShadow(DEFAULT_SHADOW);


	if( LaptopSaveInfo.fBobbyRSiteCanBeAccessed )
	{
		//Bobbys Third sentence
		SetFontShadow(BOBBIES_SENTENCE_BACKGROUNDCOLOR);
		DrawTextToScreen(BobbyRaysFrontText[BOBBYR_ADVERTISMENT_3],
			layout.advertisements[2].origin.x,
			layout.advertisements[2].origin.y,
			layout.advertisements[2].width, BOBBIES_SENTENCE_FONT,
			BOBBIES_SENTENCE_COLOR, BOBBIES_SIGN_BACKCOLOR, FALSE,
			CENTER_JUSTIFIED | TEXT_SHADOWED );
		SetFontShadow(DEFAULT_SHADOW);
	}

	//if we cant go to any sub pages, darken the page out
	if( !LaptopSaveInfo.fBobbyRSiteCanBeAccessed )
	{
		ShadowVideoSurfaceRect(FRAME_BUFFER,
			layout.pageBounds.x, layout.pageBounds.y,
			layout.pageBounds.right(), layout.pageBounds.bottom());
	}

	RenderWWWProgramTitleBar( );
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}




BOOLEAN InitBobbyRWoodBackground(LaptopPageResourceOwner& owner)
{
	VOBJECT_DESC	VObjectDesc;

	// load the Wood bacground graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BobbyWood.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiWoodBackground)) return FALSE;

	return(TRUE);
}


BOOLEAN DrawBobbyRWoodBackground()
{
	HVOBJECT hWoodBackGroundHandle;
	const BobbyRayLayoutModel::TileGrid tiles = BobbyRayHomeLayout().wood;

	// Blt the Wood background
	GetVideoObject(&hWoodBackGroundHandle, guiWoodBackground);

	for (std::size_t index = 0; index < tiles.capacity(); ++index)
	{
		const LaptopLayoutModel::Rect tile = tiles.tile(index);
		BltVideoObject(FRAME_BUFFER, hWoodBackGroundHandle, 0,
			tile.x, tile.y, VO_BLT_SRCTRANSPARENCY,NULL);
	}

	return(TRUE);
}


BOOLEAN InitBobbiesMouseRegion(MOUSE_REGION *MouseRegion,
	LaptopPageResourceOwner& owner)
{
	const BobbyRayLayoutModel::HomeLayout layout = BobbyRayHomeLayout();

	for (std::size_t i = 0; i < layout.signs.size(); ++i)
	{
		const LaptopLayoutModel::Rect bounds = layout.signs[i].bounds;
		//Mouse region for the toc buttons
		MSYS_DefineRegion(&MouseRegion[i], bounds.x, bounds.y,
			bounds.right(), bounds.bottom(), MSYS_PRIORITY_HIGH,
								CURSOR_WWW, MSYS_NO_CALLBACK, SelectBobbiesSignMenuRegionCallBack);
		if (!owner.addRegion(MouseRegion[i])) return FALSE;
		MSYS_SetRegionUserData( &MouseRegion[i], 0, gubBobbyRPages[i]);
	}
	return(TRUE);
}




void SelectBobbiesSignMenuRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{

	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		UINT8	ubNewPage = (UINT8)MSYS_GetRegionUserData( pRegion, 0 );
		guiCurrentLaptopMode = ubNewPage;
//		FindLastItemIndex(ubNewPage);

	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
	}
}


/*
BOOLEAN WebPageTileBackground(UINT8 ubNumX, UINT8 ubNumY, UINT16 usWidth, UINT16 usHeight, UINT32 uiBackground)
{
	HVOBJECT hBackGroundHandle;
	UINT16	x,y, uiPosX, uiPosY;

	// Blt the Wood background
	GetVideoObject(&hBackGroundHandle, uiBackground);

	uiPosY = LAPTOP_SCREEN_WEB_UL_Y;
	for(y=0; y<ubNumY; y++)
	{
		uiPosX = LAPTOP_SCREEN_UL_X;
		for(x=0; x<ubNumX; x++)
		{
		BltVideoObject(FRAME_BUFFER, hBackGroundHandle, 0,uiPosX, uiPosY, VO_BLT_SRCTRANSPARENCY,NULL);
			uiPosX += usWidth;
		}
		uiPosY += usHeight;
	}
	return(TRUE);
}
*/


void HandleBobbyRUnderConstructionAni( BOOLEAN fReset )
{
	HVOBJECT hPixHandle;
	static UINT32	uiLastTime=1;
	static UINT16	usCount=0;
	UINT32	uiCurTime=GetJA2Clock();
	const BobbyRayLayoutModel::HomeLayout layout = BobbyRayHomeLayout();


	if( LaptopSaveInfo.fBobbyRSiteCanBeAccessed )
		return;

	if( fReset )
		usCount =1;

	if( fShowBookmarkInfo )
	{
		fReDrawBookMarkInfo = TRUE;
	}

	if( ( ( uiCurTime - uiLastTime ) > BOBBYR_UNDERCONSTRUCTION_ANI_DELAY )||( fReDrawScreenFlag ) )
	{
		// The undercontsruction graphic
		GetVideoObject(&hPixHandle, guiUnderConstructionImage );
		const LaptopLayoutModel::Rect top =
			layout.underConstruction.at(0);
		const LaptopLayoutModel::Rect bottom =
			layout.underConstruction.at(1);
		BltVideoObject(FRAME_BUFFER, hPixHandle, usCount,
			top.x, top.y, VO_BLT_SRCTRANSPARENCY,NULL);

		BltVideoObject(FRAME_BUFFER, hPixHandle, usCount,
			bottom.x, bottom.y, VO_BLT_SRCTRANSPARENCY,NULL);

		DrawTextToScreen(BobbyRaysFrontText[BOBBYR_UNDER_CONSTRUCTION],
			layout.underConstructionText.origin.x,
			layout.underConstructionText.origin.y,
			layout.underConstructionText.width, FONT16ARIAL,
			BOBBIES_SENTENCE_COLOR, BOBBIES_SIGN_BACKCOLOR, FALSE,
			CENTER_JUSTIFIED | INVALIDATE_TEXT);

		InvalidateRegion(top.x, top.y, top.right(), top.bottom());
		InvalidateRegion(bottom.x, bottom.y, bottom.right(), bottom.bottom());

		uiLastTime = GetJA2Clock();

		usCount++;

		if( usCount >= BOBBYR_UNDERCONSTRUCTION_NUM_FRAMES )
			usCount = 0;
	}
}



void InitBobbyRayInventory()
{
	//Initializes which NEW items can be bought at Bobby Rays
	InitBobbyRayNewInventory();

	//Initializes the starting values for Bobby Rays NEW Inventory
	SetupStoreInventory( LaptopSaveInfo.BobbyRayInventory, FALSE );

	//Initializes which USED items can be bought at Bobby Rays
	InitBobbyRayUsedInventory();

	//Initializes the starting values for Bobby Rays USED Inventory
	SetupStoreInventory( LaptopSaveInfo.BobbyRayUsedInventory, TRUE);
}


BOOLEAN InitBobbyRayNewInventory()
{
	UINT16	usBobbyrIndex = 0;

	memset( LaptopSaveInfo.BobbyRayInventory, 0, sizeof(STORE_INVENTORY) * MAXITEMS);

	// add all the NEW items he can ever sell into his possible inventory list, for now in order by item #
	for( UINT16 i = 0; i < MAXITEMS; ++i )
	{
		//if Bobby Ray sells this, it can be sold, and it's allowed into this game (some depend on e.g. gun-nut option)
//		if( ( StoreInventory[ i ][ BOBBY_RAY_NEW ] != 0) && !( Item[ i ].fFlags & ITEM_NOT_BUYABLE ) && ItemIsLegal( i ) )
		LaptopSaveInfo.BobbyRayInventory[ usBobbyrIndex ].usItemIndex = i;
		++usBobbyrIndex;
	}

	if ( usBobbyrIndex > 1 )
	{
		// sort this list by object category, and by ascending price within each category
		qsort( LaptopSaveInfo.BobbyRayInventory, usBobbyrIndex, sizeof( STORE_INVENTORY ), BobbyRayItemQsortCompare );
	}

	// remember how many entries in the list are valid
	LaptopSaveInfo.usInventoryListLength[ BOBBY_RAY_NEW ] = usBobbyrIndex;

	return(TRUE);
}


BOOLEAN InitBobbyRayUsedInventory()
{
	UINT16	usBobbyrIndex = 0;


	memset( LaptopSaveInfo.BobbyRayUsedInventory, 0, sizeof(STORE_INVENTORY) * MAXITEMS);

	// add all the NEW items he can ever sell into his possible inventory list, for now in order by item #
	for( UINT16 i = 0; i < MAXITEMS; ++i )
	{
		//if Bobby Ray sells this, it can be sold, and it's allowed into this game (some depend on e.g. gun-nut option)
//		if( ( StoreInventory[ i ][ BOBBY_RAY_USED ] != 0) && !( Item[ i ].fFlags & ITEM_NOT_BUYABLE ) && ItemIsLegal( i ) )
		// in case his store inventory list is wrong, make sure this category of item can be sold used
		if ( CanDealerItemBeSoldUsed( i ) )
		{
			LaptopSaveInfo.BobbyRayUsedInventory[ usBobbyrIndex ].usItemIndex = i;
			++usBobbyrIndex;
		}
	}

	if ( usBobbyrIndex > 1 )
	{
		// sort this list by object category, and by ascending price within each category
		qsort( LaptopSaveInfo.BobbyRayUsedInventory, usBobbyrIndex, sizeof( STORE_INVENTORY ), BobbyRayItemQsortCompare );
	}

	// remember how many entries in the list are valid
	LaptopSaveInfo.usInventoryListLength[BOBBY_RAY_USED] = usBobbyrIndex;

	return(TRUE);
}



void DailyUpdateOfBobbyRaysNewInventory()
{
	INT32 i;
	UINT16 usItemIndex;
	BOOLEAN fPrevElig;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3,String("DailyUpdateOfBobbyRaysNewInventory: list length = %d", LaptopSaveInfo.usInventoryListLength[BOBBY_RAY_NEW]));

	//simulate other buyers by reducing the current quantity on hand
	SimulateBobbyRayCustomer(LaptopSaveInfo.BobbyRayInventory, BOBBY_RAY_NEW);

	//loop through all items BR can stock to see what needs reordering
	const UINT16 inventoryLength = static_cast<UINT16>(
		BobbyRayCommerceModel::BoundedLength(
			LaptopSaveInfo.usInventoryListLength[BOBBY_RAY_NEW], MAXITEMS));
	for(i = 0; i < inventoryLength; ++i)
	{
		// the index is NOT the item #, get that from the table
		usItemIndex = LaptopSaveInfo.BobbyRayInventory[ i ].usItemIndex;

		if (usItemIndex >= MAXITEMS)
			continue;

		DebugMsg(TOPIC_JA2, DBG_LEVEL_3,String("DailyUpdateOfBobbyRaysNewInventory: checking item = %d, qty on order = %d",usItemIndex,LaptopSaveInfo.BobbyRayInventory[ i ].ubQtyOnOrder));
		// make sure this item is still sellable in the latest version of the store inventory
		if ( StoreInventory[ usItemIndex ][ BOBBY_RAY_NEW ] == 0 )
		{
			DebugMsg(TOPIC_JA2, DBG_LEVEL_3,String("DailyUpdateOfBobbyRaysNewInventory: skipping item = %d",usItemIndex));
			continue;
		}

		//if the item isn't already on order
		if( LaptopSaveInfo.BobbyRayInventory[ i ].ubQtyOnOrder == 0 || gGameOptions.ubBobbyRayQuality == BR_AWESOME )
		{
			DebugMsg(TOPIC_JA2, DBG_LEVEL_3,String("DailyUpdateOfBobbyRaysNewInventory: item = %d, qty on hand = %d, half desired amount = %d",usItemIndex,LaptopSaveInfo.BobbyRayInventory[ i ].ubQtyOnHand,(StoreInventory[ usItemIndex ][ BOBBY_RAY_NEW ] * gGameOptions.ubBobbyRayQuantity )/2));
			//if the qty on hand is half the desired amount or fewer
			if( LaptopSaveInfo.BobbyRayInventory[ i ].ubQtyOnHand <= (StoreInventory[ usItemIndex ][ BOBBY_RAY_NEW ] * gGameOptions.ubBobbyRayQuantity )/2 )
			{
				// remember value of the "previously eligible" flag
				fPrevElig = LaptopSaveInfo.BobbyRayInventory[ i ].fPreviouslyEligible;

				//determine if any can/should be ordered, and how many
				LaptopSaveInfo.BobbyRayInventory[ i ].ubQtyOnOrder = HowManyBRItemsToOrder( usItemIndex, LaptopSaveInfo.BobbyRayInventory[ i ].ubQtyOnHand, BOBBY_RAY_NEW);

				//if he found some to buy
				if( LaptopSaveInfo.BobbyRayInventory[ i ].ubQtyOnOrder > 0 )
				{
					// if this is the first day the player is eligible to have access to this thing
					if ( !fPrevElig || gGameOptions.ubBobbyRayQuality == BR_AWESOME )
					{
						DebugMsg(TOPIC_JA2, DBG_LEVEL_3,String("DailyUpdateOfBobbyRaysNewInventory: item = %d, add fresh inventory",usItemIndex));
						// eliminate the ordering delay and stock the items instantly!
						// This is just a way to reward the player right away for making progress without the reordering lag...
						AddFreshBobbyRayInventory( usItemIndex );
					}
					else
					{
						OrderBobbyRItem(usItemIndex);

#ifdef BR_INVENTORY_TURNOVER_DEBUG
						if ( usItemIndex == ROCKET_LAUNCHER )
							MapScreenMessage( 0, MSG_DEBUG, L"%s: BR Ordered %d, Has %d", WORLDTIMESTR, LaptopSaveInfo.BobbyRayInventory[ i ].ubQtyOnOrder, LaptopSaveInfo.BobbyRayInventory[ i ].ubQtyOnHand );
#endif
					}
				}
			}
		}
	}
}


void DailyUpdateOfBobbyRaysUsedInventory()
{
	INT16 i;
	UINT16 usItemIndex;
	BOOLEAN fPrevElig;

	//simulate other buyers by reducing the current quantity on hand
	SimulateBobbyRayCustomer(LaptopSaveInfo.BobbyRayUsedInventory, BOBBY_RAY_USED);

	const UINT16 inventoryLength = static_cast<UINT16>(
		BobbyRayCommerceModel::BoundedLength(
			LaptopSaveInfo.usInventoryListLength[BOBBY_RAY_USED], MAXITEMS));
	for(i = 0; i < inventoryLength; i++)
	{
		//if the used item isn't already on order
		if( LaptopSaveInfo.BobbyRayUsedInventory[ i ].ubQtyOnOrder == 0 || gGameOptions.ubBobbyRayQuality == BR_AWESOME )
		{
			//if we don't have ANY
			if( LaptopSaveInfo.BobbyRayUsedInventory[ i ].ubQtyOnHand == 0 )
			{
				// the index is NOT the item #, get that from the table
				usItemIndex = LaptopSaveInfo.BobbyRayUsedInventory[ i ].usItemIndex;
				if (usItemIndex >= MAXITEMS)
					continue;

				// make sure this item is still sellable in the latest version of the store inventory
				if ( StoreInventory[ usItemIndex ][ BOBBY_RAY_USED ] == 0 )
				{
					continue;
				}

				// remember value of the "previously eligible" flag
				fPrevElig = LaptopSaveInfo.BobbyRayUsedInventory[ i ].fPreviouslyEligible;

				//determine if any can/should be ordered, and how many
				LaptopSaveInfo.BobbyRayUsedInventory[ i ].ubQtyOnOrder = HowManyBRItemsToOrder(usItemIndex, LaptopSaveInfo.BobbyRayUsedInventory[ i ].ubQtyOnHand, BOBBY_RAY_USED);

				//if he found some to buy
				if( LaptopSaveInfo.BobbyRayUsedInventory[ i ].ubQtyOnOrder > 0 )
				{
					// if this is the first day the player is eligible to have access to this thing
					if ( !fPrevElig || gGameOptions.ubBobbyRayQuality == BR_AWESOME )
					{
						// eliminate the ordering delay and stock the items instantly!
						// This is just a way to reward the player right away for making progress without the reordering lag...
						AddFreshBobbyRayInventory( usItemIndex );
					}
					else
					{
						OrderBobbyRItem((INT16) (usItemIndex + BOBBY_R_USED_PURCHASE_OFFSET));
					}
				}
			}
		}
	}
}


//returns the number of items to order
UINT8 HowManyBRItemsToOrder(UINT16 usItemIndex, UINT8 ubCurrentlyOnHand, UINT8 ubBobbyRayNewUsed )
{
	UINT8	ubItemsOrdered = 0;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3,String("HowManyBRItemsToOrder: item = %d",usItemIndex));

	if (usItemIndex >= MAXITEMS ||
		ubBobbyRayNewUsed >= BOBBY_RAY_LISTS)
	{
		Assert(0);
		return 0;
	}
	// formulas below will fail if there are more items already in stock than optimal
	//Assert(ubCurrentlyOnHand <= StoreInventory[ usItemIndex ][ ubBobbyRayNewUsed ] * gGameOptions.ubBobbyRayQuantity) ;
	// decide if he can get stock for this item (items are reordered an entire batch at a time)
	if (ItemTransactionOccurs( -1, usItemIndex, DEALER_BUYING, ubBobbyRayNewUsed ))
	{
		if (ubBobbyRayNewUsed == BOBBY_RAY_NEW)
		{
			ubItemsOrdered = HowManyItemsToReorder(StoreInventory[ usItemIndex ][ ubBobbyRayNewUsed ] * gGameOptions.ubBobbyRayQuantity, ubCurrentlyOnHand);
		}
		else
		{
			//Since these are used items we only should order 1 of each type
			ubItemsOrdered = 1;
		}
	}
	else
	{
		// can't obtain this item from suppliers
		ubItemsOrdered = 0;
	}


	return(ubItemsOrdered);
}


void OrderBobbyRItem(UINT16 usItemIndex)
{
	UINT32 uiArrivalTime;

	//add the new item to the queue.	The new item will arrive in 'uiArrivalTime' minutes.
	uiArrivalTime = BOBBY_R_NEW_PURCHASE_ARRIVAL_TIME + Random( BOBBY_R_NEW_PURCHASE_ARRIVAL_TIME / 2 );
	uiArrivalTime += GetWorldTotalMin();
	AddStrategicEvent( EVENT_UPDATE_BOBBY_RAY_INVENTORY, uiArrivalTime, usItemIndex);
}


void AddFreshBobbyRayInventory( UINT16 usItemIndex )
{
	INT16 sInventorySlot;
	STORE_INVENTORY *pInventoryArray;
	BOOLEAN fUsed;
	UINT8 ubItemQuality;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3,String("AddFreshBobbyRayInventory: item = %d", usItemIndex ));

	if( usItemIndex >= BOBBY_R_USED_PURCHASE_OFFSET )
	{
		usItemIndex -= BOBBY_R_USED_PURCHASE_OFFSET;
		pInventoryArray = LaptopSaveInfo.BobbyRayUsedInventory;
		fUsed = BOBBY_RAY_USED;
		ubItemQuality = 20 + (UINT8) Random( 60 );
	}
	else
	{
		pInventoryArray = LaptopSaveInfo.BobbyRayInventory;
		fUsed = BOBBY_RAY_NEW;
		ubItemQuality = 100;
	}


	// find out which inventory slot that item is stored in
	sInventorySlot = GetInventorySlotForItem(pInventoryArray, usItemIndex, fUsed);
	if (sInventorySlot == -1)
	{
		DebugMsg(TOPIC_JA2, DBG_LEVEL_3,String("AddFreshBobbyRayInventory: item not found! = %d", usItemIndex ));
		AssertMsg( FALSE, String( "AddFreshBobbyRayInventory(), Item %d not found.	AM-0.", usItemIndex ) );
		return;
	}

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3,String("AddFreshBobbyRayInventory: item = %d, qty on hand = %d, qty on order = %d", usItemIndex,pInventoryArray[ sInventorySlot ].ubQtyOnHand, pInventoryArray[ sInventorySlot ].ubQtyOnOrder ));

	pInventoryArray[ sInventorySlot ].ubQtyOnHand =
		BobbyRayCommerceModel::AddStock(
			pInventoryArray[sInventorySlot].ubQtyOnHand,
			pInventoryArray[sInventorySlot].ubQtyOnOrder);
	pInventoryArray[ sInventorySlot ].ubItemQuality = ubItemQuality;

#ifdef BR_INVENTORY_TURNOVER_DEBUG
	if ( usItemIndex == ROCKET_LAUNCHER && !fUsed )
		MapScreenMessage( 0, MSG_DEBUG, L"%s: BR Bought %d, Has %d", WORLDTIMESTR, pInventoryArray[ sInventorySlot ].ubQtyOnOrder, pInventoryArray[ sInventorySlot ].ubQtyOnHand );
#endif

	// cancel order
	pInventoryArray[ sInventorySlot ].ubQtyOnOrder = 0;
}


INT16 GetInventorySlotForItem(STORE_INVENTORY *pInventoryArray, UINT16 usItemIndex, BOOLEAN fUsed)
{
	INT16 i;
	if (!pInventoryArray || fUsed >= BOBBY_RAY_LISTS)
		return -1;

	const UINT16 inventoryLength = static_cast<UINT16>(
		BobbyRayCommerceModel::BoundedLength(
			LaptopSaveInfo.usInventoryListLength[fUsed], MAXITEMS));
	for(i = 0; i < inventoryLength; i++)
	{
		//if we have some of this item in stock
		if( pInventoryArray[ i ].usItemIndex == usItemIndex)
		{
			return(i);
		}
	}

	// not found!
	return(-1);
}


void SimulateBobbyRayCustomer(STORE_INVENTORY *pInventoryArray, BOOLEAN fUsed)
{
	INT16 i;
	UINT8 ubItemsSold;
	if (!pInventoryArray || fUsed >= BOBBY_RAY_LISTS)
		return;
	const UINT16 inventoryLength = static_cast<UINT16>(
		BobbyRayCommerceModel::BoundedLength(
			LaptopSaveInfo.usInventoryListLength[fUsed], MAXITEMS));

	//loop through all items BR can stock to see what gets sold
	for(i = 0; i < inventoryLength; i++)
	{
		//if we have some of this item in stock
		if( pInventoryArray[ i ].ubQtyOnHand > 0 &&
			pInventoryArray[i].usItemIndex < MAXITEMS)
		{
			ubItemsSold = HowManyItemsAreSold( -1, pInventoryArray[ i ].usItemIndex, pInventoryArray[ i ].ubQtyOnHand, fUsed);
			pInventoryArray[ i ].ubQtyOnHand -= ubItemsSold;

#ifdef BR_INVENTORY_TURNOVER_DEBUG
			if (ubItemsSold > 0 )
			{
				if ( i == ROCKET_LAUNCHER && !fUsed )
					MapScreenMessage( 0, MSG_DEBUG, L"%s: BR Sold %d, Has %d", WORLDTIMESTR, ubItemsSold, pInventoryArray[ i ].ubQtyOnHand );
			}
#endif
		}
	}
}


void CancelAllPendingBRPurchaseOrders(void)
{
	// remove all the BR-Order events off the event queue
	DeleteAllStrategicEventsOfType( EVENT_UPDATE_BOBBY_RAY_INVENTORY );

	// zero out all the quantities on order
	for( UINT16 i = 0; i < MAXITEMS; ++i)
	{
		LaptopSaveInfo.BobbyRayInventory[ i ].ubQtyOnOrder = 0;
		LaptopSaveInfo.BobbyRayUsedInventory[ i ].ubQtyOnOrder = 0;
	}

	// do an extra daily update immediately to create new reorders ASAP
	DailyUpdateOfBobbyRaysNewInventory();
	DailyUpdateOfBobbyRaysUsedInventory();
}
