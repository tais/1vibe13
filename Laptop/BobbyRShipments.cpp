	#include "laptop.h"
	#include "BobbyRShipments.h"
	#include "BobbyRayLayout.h"
	#include "BobbyRayFulfilmentModel.h"
	#include "LaptopPageResourceOwner.h"
	#include "LaptopSafety.h"
	#include "BobbyR.h"
	#include "Utilities.h"
	#include "Text.h"
	#include "BobbyRGuns.h"
	#include "Cursors.h"
	#include "BobbyRMailOrder.h"
	#include "WordWrap.h"
	#include "strategic.h"
	#include "strategicmap.h"
	#include "PostalService.h"
	#include "input.h"
	#include "english.h"

namespace
{
BobbyRayLayoutModel::ShipmentLayout BobbyRayShipmentLayout()
{
	return BobbyRayLayoutModel::MakeShipmentLayout({
		LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y,
		LAPTOP_SCREEN_LR_X, LAPTOP_SCREEN_WEB_LR_Y,
		iScreenWidthOffset, iScreenHeightOffset,
		LAPTOP_SCREEN_WEB_DELTA_Y});
}
}



#define		BOBBYR_SHIPMENT_TITLE_TEXT_FONT			FONT14ARIAL
#define		BOBBYR_SHIPMENT_TITLE_TEXT_COLOR			157

#define		BOBBYR_SHIPMENT_STATIC_TEXT_FONT			FONT12ARIAL
#define		BOBBYR_SHIPMENT_STATIC_TEXT_COLOR			145

extern UINT8 gubPurchaseAtTopOfList;
UINT32		guiBobbyRShipmentGrid;

LaptopPageResourceOwner gBobbyRShipmentResources;
LaptopPageResourceOwner gBobbyRPreviousShipmentRegionResources;

BOOLEAN		gfBobbyRShipmentsDirty = FALSE;

INT32			giBobbyRShipmentSelectedShipment = -1;

//Back Button
void BtnBobbyRShipmentBackCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiBobbyRShipmetBack;
INT32		guiBobbyRShipmentBackImage;

//Home Button
void BtnBobbyRShipmentHomeCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiBobbyRShipmentHome;
INT32		giBobbyRShipmentHomeImage;



MOUSE_REGION gSelectedPreviousShipmentsRegion[
	BobbyRayLayoutModel::ShipmentLayout::VisibleRowCount];
UINT32 gSelectedPreviousShipmentRegionCount = 0;
void SelectPreviousShipmentsRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );


//
// Function Prototypes
//

void DisplayShipmentGrid();
void DisplayPreviousShipments();
void DisplayShipmentTitles();
void RemovePreviousShipmentsMouseRegions();
BOOLEAN CreatePreviousShipmentsMouseRegions();
INT32	CountNumberValidShipmentForTheShipmentsPage();
//ppp
extern CPostalService gPostalService;
extern vector<PShipmentStruct> gShipmentTable;
extern UINT32		guiGoldArrowImages;
extern UINT32		guiBobbyROrderGrid;
void HandleBobbyRShipmentsKeyBoardInput();
//
// Function
//

void GameInitBobbyRShipments()
{

}


BOOLEAN EnterBobbyRShipments()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner staged;
	const BobbyRayLayoutModel::ShipmentLayout layout =
		BobbyRayShipmentLayout();
	gBobbyRShipmentResources.clear();
	gBobbyRPreviousShipmentRegionResources.clear();
	ClearBobbyRayOrderGridMouseRegions();
	RefreshBobbyRayShipmentSnapshot();

	if (!InitBobbyRWoodBackground(staged)) return FALSE;

	// load the Order Grid graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BobbyRay_OnOrder.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiBobbyRShipmentGrid)) return FALSE;

	// Gold Arrow for the scroll area
	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP( "LAPTOP\\GoldArrows.sti", VObjectDesc.ImageFile );
	if (!staged.addVideoObject(&VObjectDesc, guiGoldArrowImages)) return FALSE;

	// The order grid is part of the page, not a per-render temporary.
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BobbyOrderGrid.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiBobbyROrderGrid)) return FALSE;

	if (!staged.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\CatalogueButton.sti", -1, 0, -1, 1, -1),
		guiBobbyRShipmentBackImage)) return FALSE;
	if (!staged.addButton(CreateIconAndTextButton( guiBobbyRShipmentBackImage, BobbyROrderFormText[BOBBYR_BACK], BOBBYR_GUNS_BUTTON_FONT,
													BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
													BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
													TEXT_CJUSTIFIED,
											layout.backButton.x, layout.backButton.y,
											BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
													DEFAULT_MOVE_CALLBACK, BtnBobbyRShipmentBackCallback),
		guiBobbyRShipmetBack)) return FALSE;
	SetButtonCursor( guiBobbyRShipmetBack, CURSOR_LAPTOP_SCREEN);


	if (!staged.addButtonImage(UniqueButtonImageHandle(
		UseLoadedButtonImage(guiBobbyRShipmentBackImage, -1, 0, -1, 1, -1)),
		giBobbyRShipmentHomeImage)) return FALSE;
	if (!staged.addButton(CreateIconAndTextButton( giBobbyRShipmentHomeImage, BobbyROrderFormText[BOBBYR_HOME], BOBBYR_GUNS_BUTTON_FONT,
													BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
													BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
													TEXT_CJUSTIFIED,
											layout.homeButton.x, layout.homeButton.y,
											BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
													DEFAULT_MOVE_CALLBACK, BtnBobbyRShipmentHomeCallback),
		guiBobbyRShipmentHome)) return FALSE;
	SetButtonCursor( guiBobbyRShipmentHome, CURSOR_LAPTOP_SCREEN);

	if (!CreateBobbyRayOrderTitle(staged)) return FALSE;

	giBobbyRShipmentSelectedShipment = -1;
	gubPurchaseAtTopOfList = 0;

	const std::size_t firstShipment =
		BobbyRayFulfilmentModel::IndexForMatchingSlot(
			gShipmentTable.size(), 0, [](std::size_t index)
			{
				return gShipmentTable[index] &&
					gShipmentTable[index]->ShipmentStatus ==
						SHIPMENT_INTRANSIT;
			});
	giBobbyRShipmentSelectedShipment = firstShipment ==
		BobbyRayFulfilmentModel::NoSelection
		? -1 : static_cast<INT32>(firstShipment);
	
	ClearBobbyRayOrderGridMouseRegions();
	if (!CreatePreviousShipmentsMouseRegions()) return FALSE;
	gBobbyRShipmentResources = std::move(staged);

	return( TRUE );
}

void ExitBobbyRShipments()
{
	RemovePreviousShipmentsMouseRegions();
	ClearBobbyRayOrderGridMouseRegions();
	gBobbyRShipmentResources.clear();
}

void HandleBobbyRShipments()
{
	if( gfBobbyRShipmentsDirty )
	{
		gfBobbyRShipmentsDirty = FALSE;

		RenderBobbyRShipments();
	}

	HandleBobbyRShipmentsKeyBoardInput();
}

void RenderBobbyRShipments()
{
//	HVOBJECT hPixHandle;

	// Dealtar: this must be static as this is accessed after this function has returned
	static BobbyRayPurchaseStruct brps[
		BobbyRayCommerceModel::PurchaseCapacity];
	const BobbyRayLayoutModel::ShipmentLayout layout =
		BobbyRayShipmentLayout();
	for (std::size_t i = 0;
		i < BobbyRayCommerceModel::PurchaseCapacity; ++i)
	{
		memset(&brps[i], 0, sizeof(BobbyRayPurchaseStruct));
	}

	DrawBobbyRWoodBackground();

	DrawBobbyROrderTitle();

	//Output the title
	DrawTextToScreen(gzBobbyRShipmentText[BOBBYR_SHIPMENT__TITLE],
		layout.title.origin.x, layout.title.origin.y, layout.title.width,
		BOBBYR_SHIPMENT_TITLE_TEXT_FONT, BOBBYR_SHIPMENT_TITLE_TEXT_COLOR,
		FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	DisplayShipmentGrid();

	const bool hasSelectedShipment =
		giBobbyRShipmentSelectedShipment >= 0 &&
		IsValidLaptopIndex(gShipmentTable.size(),
			giBobbyRShipmentSelectedShipment) &&
		gShipmentTable[giBobbyRShipmentSelectedShipment];
	if (hasSelectedShipment)
	{
		ShipmentPackageList::iterator spli = gShipmentTable[giBobbyRShipmentSelectedShipment]->ShipmentPackages.begin();
		int j;
		for (std::size_t i = 0;
			i < gShipmentTable[giBobbyRShipmentSelectedShipment]
				->ShipmentPackages.size() &&
			i < BobbyRayCommerceModel::PurchaseCapacity; ++i, ++spli)
		{
			if (spli->usItemIndex == 0 || spli->usItemIndex >= MAXITEMS ||
				!spli->ubNumber)
				continue;
			brps[i].bItemQuality = ((ShipmentPackageStruct)*spli).bItemQuality;
			brps[i].ubNumberPurchased = ((ShipmentPackageStruct)*spli).ubNumber;
			brps[i].usItemIndex = ((ShipmentPackageStruct)*spli).usItemIndex;
			brps[i].fUsed = (((ShipmentPackageStruct)*spli).bItemQuality < 100);

			if(brps[i].fUsed)
			{
				j = GetInventorySlotForItem(
					LaptopSaveInfo.BobbyRayUsedInventory,
					brps[i].usItemIndex, BOBBY_RAY_USED);
				brps[i].usBobbyItemIndex = j >= 0 ? j : 0;
			}
			else
			{
				j = GetInventorySlotForItem(
					LaptopSaveInfo.BobbyRayInventory,
					brps[i].usItemIndex, BOBBY_RAY_NEW);
				brps[i].usBobbyItemIndex = j >= 0 ? j : 0;
			}
		}
	}
	
	if (hasSelectedShipment &&
		gShipmentTable[ giBobbyRShipmentSelectedShipment ]->ShipmentStatus == SHIPMENT_INTRANSIT) // &&
	{
		DisplayPurchasedItems(FALSE, layout.orderGrid.x, layout.orderGrid.y,
			&brps[0], FALSE, giBobbyRShipmentSelectedShipment);
	}
	else
	{
		DisplayPurchasedItems(FALSE, layout.orderGrid.x, layout.orderGrid.y,
			NULL, TRUE, giBobbyRShipmentSelectedShipment);
	}
	

	DisplayShipmentTitles();
	DisplayPreviousShipments();

	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}

void BtnBobbyRShipmentBackCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

		guiCurrentLaptopMode = LAPTOP_MODE_BOBBY_R_MAILORDER;

		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


void BtnBobbyRShipmentHomeCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

		guiCurrentLaptopMode	= LAPTOP_MODE_BOBBY_R;

		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


void DisplayShipmentGrid()
{
	HVOBJECT hPixHandle, hPixGrid;
	const BobbyRayLayoutModel::ShipmentLayout layout =
		BobbyRayShipmentLayout();
	GetVideoObject(&hPixHandle, guiBobbyRShipmentGrid);
	// Shipment Order Grid
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0,
		layout.deliveryGrid.x, layout.deliveryGrid.y,
		VO_BLT_SRCTRANSPARENCY,NULL);

	GetVideoObject(&hPixGrid, guiBobbyROrderGrid);
	BltVideoObject(FRAME_BUFFER, hPixGrid, 0,
		layout.orderGrid.x, layout.orderGrid.y,
		VO_BLT_SRCTRANSPARENCY,NULL);
}



void DisplayShipmentTitles()
{
	const BobbyRayLayoutModel::ShipmentLayout layout =
		BobbyRayShipmentLayout();
	//output the order #
	DrawTextToScreen(gzBobbyRShipmentText[BOBBYR_SHIPMENT__ORDERED_ON],
		layout.orderedOnHeader.origin.x, layout.orderedOnHeader.origin.y,
		layout.orderedOnHeader.width, BOBBYR_SHIPMENT_STATIC_TEXT_FONT,
		BOBBYR_SHIPMENT_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK,
		FALSE, CENTER_JUSTIFIED);

	//Output the # of items
	DrawTextToScreen(gzBobbyRShipmentText[BOBBYR_SHIPMENT__NUM_ITEMS],
		layout.itemCountHeader.origin.x, layout.itemCountHeader.origin.y,
		layout.itemCountHeader.width, BOBBYR_SHIPMENT_STATIC_TEXT_FONT,
		BOBBYR_SHIPMENT_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK,
		FALSE, CENTER_JUSTIFIED);
}

void DisplayPreviousShipments()
{
	UINT32 uiCnt;
	CHAR16	zText[512];
	UINT32	uiNumItems; // = CountNumberValidShipmentForTheShipmentsPage();
	UINT32	uiNumberItemsInShipments = 0;
	UINT32	uiItemCnt;
	UINT8		ubFontColor = BOBBYR_SHIPMENT_STATIC_TEXT_COLOR;
	const BobbyRayLayoutModel::ShipmentLayout layout =
		BobbyRayShipmentLayout();
	
	uiNumItems = static_cast<UINT32>(gShipmentTable.size());

	//loop through all the shipments
	UINT32 displayedCount = 0;
	for (uiCnt = 0; uiCnt < uiNumItems &&
		displayedCount <
			BobbyRayLayoutModel::ShipmentLayout::VisibleRowCount; ++uiCnt)
	{
		// if it is a shipment that is active (= in transit)
		if (gShipmentTable[uiCnt] &&
			gShipmentTable[uiCnt]->ShipmentStatus == SHIPMENT_INTRANSIT)
		{
			const std::size_t rowSlot = displayedCount++;
			const LaptopLayoutModel::TextArea orderedOn =
				layout.orderedOn(rowSlot);
			const LaptopLayoutModel::TextArea itemCount =
				layout.itemCount(rowSlot);
			if( uiCnt == (UINT32)giBobbyRShipmentSelectedShipment )
			{
				ubFontColor = FONT_MCOLOR_WHITE;
			}
			else
			{
				ubFontColor = BOBBYR_SHIPMENT_STATIC_TEXT_COLOR;
			}

			//Display the "ordered on day num"
			//swprintf( zText, L"%s %d", gpGameClockString[0], gpNewBobbyrShipments[ uiCnt ].uiOrderedOnDayNum );
			swprintf( zText, L"%s %d", gpGameClockString[0], gShipmentTable[ uiCnt ]->uiOrderDate );
			DrawTextToScreen(zText, orderedOn.origin.x, orderedOn.origin.y,
				orderedOn.width, BOBBYR_SHIPMENT_STATIC_TEXT_FONT,
				ubFontColor, 0, FALSE, CENTER_JUSTIFIED);

			uiNumberItemsInShipments = 0;

			for( uiItemCnt=0; uiItemCnt<gShipmentTable[ uiCnt ]->ShipmentPackages.size(); uiItemCnt++ )
			{
				uiNumberItemsInShipments += gShipmentTable[ uiCnt ]->ShipmentPackages[uiItemCnt].ubNumber;
			}


			//Display the # of items
			swprintf( zText, L"%d", uiNumberItemsInShipments );
			DrawTextToScreen(zText, itemCount.origin.x, itemCount.origin.y,
				itemCount.width, BOBBYR_SHIPMENT_STATIC_TEXT_FONT,
				ubFontColor, 0, FALSE, CENTER_JUSTIFIED);
		}
	}
}

BOOLEAN CreatePreviousShipmentsMouseRegions()
{
	gBobbyRPreviousShipmentRegionResources.clear();
	gSelectedPreviousShipmentRegionCount = 0;
	LaptopPageResourceOwner staged;
	UINT32 uiCnt;
	UINT16	usHeight = GetFontHeight( BOBBYR_SHIPMENT_STATIC_TEXT_FONT );
	const BobbyRayLayoutModel::ShipmentLayout layout =
		BobbyRayShipmentLayout();
	UINT32 snapshotInTransit = 0;
	for (const PShipmentStruct shipment : gShipmentTable)
	{
		if (shipment && shipment->ShipmentStatus == SHIPMENT_INTRANSIT)
			++snapshotInTransit;
	}
	const UINT32 max = static_cast<UINT32>(
		BobbyRayCommerceModel::VisibleShipmentCount(
			gPostalService.GetShipmentCount(SHIPMENT_INTRANSIT),
			snapshotInTransit,
			BobbyRayLayoutModel::ShipmentLayout::VisibleRowCount));
	for( uiCnt=0; uiCnt<max; uiCnt++ )
	{
		LaptopLayoutModel::Rect row = layout.rows.at(uiCnt);
		row.height = usHeight;
		MSYS_DefineRegion(&gSelectedPreviousShipmentsRegion[uiCnt],
			row.x, row.y, row.right(), row.bottom(), MSYS_PRIORITY_HIGH,
								CURSOR_WWW, MSYS_NO_CALLBACK, SelectPreviousShipmentsRegionCallBack );
		if (!staged.addRegion(gSelectedPreviousShipmentsRegion[uiCnt]))
			return FALSE;
		MSYS_SetRegionUserData( &gSelectedPreviousShipmentsRegion[uiCnt], 0, uiCnt);

	}
	gBobbyRPreviousShipmentRegionResources = std::move(staged);
	gSelectedPreviousShipmentRegionCount = max;
	return TRUE;
}

void RemovePreviousShipmentsMouseRegions()
{
	gBobbyRPreviousShipmentRegionResources.clear();
	gSelectedPreviousShipmentRegionCount = 0;
}

void SelectPreviousShipmentsRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		gubPurchaseAtTopOfList = 0;
		INT32 iSlotID = MSYS_GetRegionUserData( pRegion, 0 );


//		if( CountNumberOfBobbyPurchasesThatAreInTransit() > iSlotID )
		if (iSlotID >= 0 && static_cast<UINT32>(iSlotID) <
			gSelectedPreviousShipmentRegionCount)
		{
			const std::size_t shipmentIndex =
				BobbyRayFulfilmentModel::IndexForMatchingSlot(
					gShipmentTable.size(),
					static_cast<std::size_t>(iSlotID),
					[](std::size_t index)
					{
						return gShipmentTable[index] &&
							gShipmentTable[index]->ShipmentStatus ==
								SHIPMENT_INTRANSIT;
					});
			giBobbyRShipmentSelectedShipment = shipmentIndex ==
				BobbyRayFulfilmentModel::NoSelection
				? -1 : static_cast<INT32>(shipmentIndex);
		}

		gfBobbyRShipmentsDirty = TRUE;
	}
}
void HandleBobbyRShipmentsKeyBoardInput()
{
	InputAtom					InputEvent;

	//while (DequeueSpecificEvent(&InputEvent, KEY_DOWN |KEY_REPEAT) == TRUE)
	while (DequeueEvent(&InputEvent) == TRUE)
	{
		if( InputEvent.usEvent == KEY_DOWN )
		{
			switch (InputEvent.usParam)
			{
				case BACKSPACE:
				case 'q':
					guiCurrentLaptopMode = LAPTOP_MODE_BOBBY_R_MAILORDER;
				break;
				default:
					HandleKeyBoardShortCutsForLapTop( InputEvent.usEvent, InputEvent.usParam, InputEvent.usKeyState );
				break;
			}
		}
	}
}
