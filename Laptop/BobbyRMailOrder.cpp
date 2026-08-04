	#include "laptop.h"
	#include "BobbyRMailOrder.h"
	#include "BobbyR.h"
	#include "BobbyRayFulfilmentModel.h"
	#include "LaptopPageResourceOwner.h"
	#include "Utilities.h"
	#include "WordWrap.h"
	#include "Cursors.h"
	#include "Encrypted File.h"
	#include "BobbyRGuns.h"
	#include "finances.h"
	#include "Game Clock.h"
	#include "Game Event Hook.h"
	#include "random.h"
	#include "LaptopSave.h"
	#include "LaptopSafety.h"
	#include "Soldier Profile.h"
	#include "input.h"
	#include "line.h"
	#include "Text.h"
	#include "Campaign Types.h"
	#include "Multi Language Graphic Utils.h"
	#include "strategic.h"
	#include "strategicmap.h"
	#include "Isometric Utils.h"
	#include "PostalService.h"
	#include "english.h"
	#include <list>
	#include <memory>
	#include <vector>

#include "Strategic Event Handler.h"
#include "connect.h"
#include "GameSettings.h"
#include <vfs/Core/vfs.h>
#include <vfs/Core/File/vfs_file.h>
#include <language.hpp>

/*
typedef struct
{
	STR16	*psCityLoc;
	UINT16	usOverNightExpress;
	UINT16	us2DaysService;
	UINT16	usStandardService;
} BobbyROrderLocationStruct;
*/

/*
BobbyROrderLocationStruct BobbyROrderLocations[]=
{
	{	&pDeliveryLocationStrings[0],		20,		15,		10 },
	{	&pDeliveryLocationStrings[1],	295,	150,	85 },
	{	&pDeliveryLocationStrings[2],	200,	100,	50 },	// the only one that really matters
	{	&pDeliveryLocationStrings[3],		100,	55,		30 },
	{	&pDeliveryLocationStrings[4],	95,		65,		40 },
	{	&pDeliveryLocationStrings[5],	55,		40,		25 },
	{	&pDeliveryLocationStrings[6],		35,		25,		15 },
	{	&pDeliveryLocationStrings[7],	200,	100,	50 },
	{	&pDeliveryLocationStrings[8],		190,	90,		45 },
	{	&pDeliveryLocationStrings[9],	35,		25,		15 },
	{	&pDeliveryLocationStrings[10],	100,	55,		30 },
	{	&pDeliveryLocationStrings[11],	35,		25,		15 },
	{	&pDeliveryLocationStrings[12],	45,		30,		20 },
	{	&pDeliveryLocationStrings[13],	55,		40,		25 },
	{	&pDeliveryLocationStrings[14],	100,	55,		30 },
	{	&pDeliveryLocationStrings[15],	100,	55,		30 },
	{	&pDeliveryLocationStrings[16],	45,		30,		20 },
 };
*/

//drop down menu
enum
{
	BR_DROP_DOWN_NO_ACTION,
	BR_DROP_DOWN_CREATE,
	BR_DROP_DOWN_DESTROY,
	BR_DROP_DOWN_DISPLAY,
};

//#define		BOBBYR_ORDER_NUM_SHIPPING_CITIES	17
//#define		BOBBYR_NUM_DISPLAYED_CITIES				10

#define		BOBBYR_NUM_DISPLAYED_ITEMS				10

#define		OVERNIGHT_EXPRESS									1
#define		TWO_BUSINESS_DAYS									2
#define		STANDARD_SERVICE									3

#define		MIN_SHIPPING_WEIGHT								20

#define		BOBBYR_ORDER_TITLE_TEXT_FONT			FONT14ARIAL
#define		BOBBYR_ORDER_TITLE_TEXT_COLOR			157

#define		BOBBYR_FONT_BLACK									2

#define		BOBBYR_ORDER_STATIC_TEXT_FONT			FONT12ARIAL
#define		BOBBYR_ORDER_STATIC_TEXT_COLOR		145

#define		BOBBYR_DISCLAIMER_FONT						FONT10ARIAL

#define		BOBBYR_ORDER_DYNAMIC_TEXT_FONT		FONT12ARIAL
#define		BOBBYR_ORDER_DYNAMIC_TEXT_COLOR		FONT_MCOLOR_WHITE

#define		BOBBYR_ORDER_DROP_DOWN_SELEC_COLOR		FONT_MCOLOR_WHITE

#define		BOBBYR_DROPDOWN_FONT							FONT12ARIAL

#define		BOBBYR_ORDERGRID_X								LAPTOP_SCREEN_UL_X + 2
#define		BOBBYR_ORDERGRID_Y								LAPTOP_SCREEN_WEB_UL_Y + 62

#define		BOBBYR_BOBBY_RAY_TITLE_X					LAPTOP_SCREEN_UL_X + 171
#define		BOBBYR_BOBBY_RAY_TITLE_Y					LAPTOP_SCREEN_WEB_UL_Y + 3
#define		BOBBYR_BOBBY_RAY_TITLE_WIDTH			160
#define		BOBBYR_BOBBY_RAY_TITLE_HEIGHT			35

#define		BOBBYR_LOCATION_BOX_X							LAPTOP_SCREEN_UL_X + 276
#define		BOBBYR_LOCATION_BOX_Y							LAPTOP_SCREEN_WEB_UL_Y + 62

#define		BOBBYR_DELIVERYSPEED_X						LAPTOP_SCREEN_UL_X + 276
#define		BOBBYR_DELIVERYSPEED_Y						LAPTOP_SCREEN_WEB_UL_Y + 149

#define		BOBBYR_CLEAR_ORDER_X							LAPTOP_SCREEN_UL_X + 309
#define		BOBBYR_CLEAR_ORDER_Y							LAPTOP_SCREEN_WEB_UL_Y + 268	//LAPTOP_SCREEN_WEB_UL_Y + 252

#define		BOBBYR_ACCEPT_ORDER_X							LAPTOP_SCREEN_UL_X + 299
#define		BOBBYR_ACCEPT_ORDER_Y							LAPTOP_SCREEN_WEB_UL_Y + 303	//LAPTOP_SCREEN_WEB_UL_Y + 288

#define		BOBBYR_GRID_ROW_OFFSET						20
#define		BOBBYR_GRID_TITLE_OFFSET					27

#define		BOBBYR_GRID_FIRST_COLUMN_X				23//BOBBYR_ORDERGRID_X + 3
#define		BOBBYR_GRID_FIRST_COLUMN_Y				37//BOBBYR_ORDERGRID_Y + 37
#define		BOBBYR_GRID_FIRST_COLUMN_WIDTH		23

#define		BOBBYR_GRID_SECOND_COLUMN_X				48//BOBBYR_ORDERGRID_X + 28
#define		BOBBYR_GRID_SECOND_COLUMN_Y				BOBBYR_GRID_FIRST_COLUMN_Y
#define		BOBBYR_GRID_SECOND_COLUMN_WIDTH		40

#define		BOBBYR_GRID_THIRD_COLUMN_X				90//BOBBYR_ORDERGRID_X + 70
#define		BOBBYR_GRID_THIRD_COLUMN_Y				BOBBYR_GRID_FIRST_COLUMN_Y
#define		BOBBYR_GRID_THIRD_COLUMN_WIDTH		91

#define		BOBBYR_GRID_FOURTH_COLUMN_X				184//BOBBYR_ORDERGRID_X + 184
#define		BOBBYR_GRID_FOURTH_COLUMN_Y				BOBBYR_GRID_FIRST_COLUMN_Y
#define		BOBBYR_GRID_FOURTH_COLUMN_WIDTH		40

#define		BOBBYR_GRID_FIFTH_COLUMN_X					224//BOBBYR_ORDERGRID_X + 224
#define		BOBBYR_GRID_FIFTH_COLUMN_Y					BOBBYR_GRID_FIRST_COLUMN_Y
#define		BOBBYR_GRID_FIFTH_COLUMN_WIDTH			42

#define		BOBBYR_GRID_SCROLL_COLUMN_X				2
#define		BOBBYR_GRID_SCROLL_COLUMN_Y				BOBBYR_GRID_FIRST_COLUMN_Y - 2
#define		BOBBYR_GRID_SCROLL_COLUMN_WIDTH			23
#define		BOBBYR_GRID_SCROLL_COLUMN_HEIGHT		BOBBYR_GRID_ROW_OFFSET * 10
#define		BOBBYR_GRID_SCROLL_COLUMN_HEIGHT_MINUS_ARROWS	( BOBBYR_GRID_SCROLL_COLUMN_HEIGHT - (2 * BOBBYR_SCROLL_ARROW_HEIGHT) - 8 )
#define		BOBBYR_GRID_SCROLL_UP_ARROW_X				BOBBYR_GRID_SCROLL_COLUMN_X + 2
#define		BOBBYR_GRID_SCROLL_UP_ARROW_Y				BOBBYR_GRID_SCROLL_COLUMN_Y + 4
#define		BOBBYR_GRID_SCROLL_DOWN_ARROW_X				BOBBYR_GRID_SCROLL_UP_ARROW_X
#define		BOBBYR_GRID_SCROLL_DOWN_ARROW_Y				BOBBYR_GRID_SCROLL_COLUMN_Y+BOBBYR_GRID_SCROLL_COLUMN_HEIGHT-24

#define	BOBBYR_SUBTOTAL_WIDTH							212
#define		BOBBYR_SUBTOTAL_X									BOBBYR_GRID_FIRST_COLUMN_X - 20
#define		BOBBYR_SUBTOTAL_Y									BOBBYR_GRID_FIRST_COLUMN_Y + BOBBYR_GRID_ROW_OFFSET * 10 + 3

#define		BOBBYR_SHIPPING_N_HANDLE_Y				BOBBYR_SUBTOTAL_Y + 17
#define	BOBBYR_GRAND_TOTAL_Y							BOBBYR_SHIPPING_N_HANDLE_Y + 20

#define		BOBBYR_SHIPPING_LOCATION_TEXT_X		BOBBYR_LOCATION_BOX_X + 8
#define		BOBBYR_SHIPPING_LOCATION_TEXT_Y		BOBBYR_LOCATION_BOX_Y + 8

#define		BOBBYR_SHIPPING_SPEED_X						BOBBYR_SHIPPING_LOCATION_TEXT_X
#define		BOBBYR_SHIPPING_SPEED_Y						BOBBYR_DELIVERYSPEED_Y + 11

#define		BOBBYR_SHIPPING_COST_X						BOBBYR_SHIPPING_SPEED_X + 130

#define		BOBBYR_OVERNIGHT_EXPRESS_Y				BOBBYR_DELIVERYSPEED_Y + 42

#define		BOBBYR_ORDER_FORM_TITLE_X					BOBBYR_BOBBY_RAY_TITLE_X
#define		BOBBYR_ORDER_FORM_TITLE_Y					BOBBYR_BOBBY_RAY_TITLE_Y + 37
#define		BOBBYR_ORDER_FORM_TITLE_WIDTH			159

#define		BOBBYR_BACK_BUTTON_X							iScreenWidthOffset + 130
#define		BOBBYR_BACK_BUTTON_Y							iScreenHeightOffset + 400 + LAPTOP_SCREEN_WEB_DELTA_Y + 4

#define		BOBBYR_HOME_BUTTON_X							iScreenWidthOffset + 515
#define		BOBBYR_HOME_BUTTON_Y							BOBBYR_BACK_BUTTON_Y

#define		BOBBYR_SHIPMENT_BUTTON_X					iScreenWidthOffset + ( (LAPTOP_SCREEN_UL_X - iScreenWidthOffset) + ( (LAPTOP_SCREEN_LR_X - iScreenWidthOffset) - (LAPTOP_SCREEN_UL_X - iScreenWidthOffset - 75 )) / 2)
#define		BOBBYR_SHIPMENT_BUTTON_Y					BOBBYR_BACK_BUTTON_Y

#define		SHIPPING_SPEED_LIGHT_WIDTH				9
#define		SHIPPING_SPEED_LIGHT_HEIGHT				9

#define		BOBBYR_CONFIRM_ORDER_X						iScreenWidthOffset + 220
#define		BOBBYR_CONFIRM_ORDER_Y						iScreenHeightOffset + 170

#define		BOBBYR_CITY_START_LOCATION_X			BOBBYR_LOCATION_BOX_X + 6
#define		BOBBYR_CITY_START_LOCATION_Y			BOBBYR_LOCATION_BOX_Y + 61
#define		BOBBYR_DROP_DOWN_WIDTH						182//203
#define		BOBBYR_DROP_DOWN_HEIGHT						19
#define		BOBBYR_CITY_NAME_OFFSET						6

#define		BOBBYR_SCROLL_AREA_X							BOBBYR_CITY_START_LOCATION_X + BOBBYR_DROP_DOWN_WIDTH
#define		BOBBYR_SCROLL_AREA_Y							BOBBYR_CITY_START_LOCATION_Y
#define		BOBBYR_SCROLL_AREA_WIDTH					22
#define		BOBBYR_SCROLL_AREA_HEIGHT					139
#define		BOBBYR_SCROLL_AREA_HEIGHT_MINUS_ARROWS	( BOBBYR_SCROLL_AREA_HEIGHT - (2 * BOBBYR_SCROLL_ARROW_HEIGHT) - 8 )

#define		BOBBYR_SCROLL_UP_ARROW_X					BOBBYR_SCROLL_AREA_X
#define		BOBBYR_SCROLL_UP_ARROW_Y					BOBBYR_SCROLL_AREA_Y+5
#define		BOBBYR_SCROLL_DOWN_ARROW_X				BOBBYR_SCROLL_UP_ARROW_X
#define		BOBBYR_SCROLL_DOWN_ARROW_Y				BOBBYR_SCROLL_AREA_Y+BOBBYR_SCROLL_AREA_HEIGHT-24
#define		BOBBYR_SCROLL_ARROW_WIDTH					18
#define		BOBBYR_SCROLL_ARROW_HEIGHT				20


#define		BOBBYR_SHIPPING_LOC_AREA_L_X			BOBBYR_LOCATION_BOX_X + 9
#define		BOBBYR_SHIPPING_LOC_AREA_T_Y			BOBBYR_LOCATION_BOX_Y + 39

#define		BOBBYR_SHIPPING_LOC_AREA_R_X			BOBBYR_LOCATION_BOX_X + 206
#define		BOBBYR_SHIPPING_LOC_AREA_B_Y			BOBBYR_LOCATION_BOX_Y + 57

#define		BOBBYR_SHIPPING_SPEED_NUMBER_X		BOBBYR_SHIPPING_COST_X
#define		BOBBYR_SHIPPING_SPEED_NUMBER_WIDTH	37
#define		BOBBYR_SHIPPING_SPEED_NUMBER_1_Y	BOBBYR_OVERNIGHT_EXPRESS_Y

#define		BOBBYR_SHIPPING_SPEED_NUMBER_2_Y	BOBBYR_OVERNIGHT_EXPRESS_Y
#define		BOBBYR_SHIPPING_SPEED_NUMBER_3_Y	BOBBYR_OVERNIGHT_EXPRESS_Y

#define		BOBBYR_TOTAL_SAVED_AREA_X					BOBBYR_ORDERGRID_X + 221
#define		BOBBYR_TOTAL_SAVED_AREA_Y					BOBBYR_ORDERGRID_Y + 237

#define		BOBBYR_USED_WARNING_X							iScreenWidthOffset + 122
#define		BOBBYR_USED_WARNING_Y							iScreenHeightOffset + 382 + LAPTOP_SCREEN_WEB_DELTA_Y


#define		BOBBYR_PACKAXGE_WEIGHT_X					BOBBYR_LOCATION_BOX_X
#define		BOBBYR_PACKAXGE_WEIGHT_Y					LAPTOP_SCREEN_WEB_UL_Y + 249
#define		BOBBYR_PACKAXGE_WEIGHT_WIDTH			188



UINT16 gShippingSpeedAreas[] = {585, 218 + LAPTOP_SCREEN_WEB_DELTA_Y,
																585, 238 + LAPTOP_SCREEN_WEB_DELTA_Y,
																585, 258 + LAPTOP_SCREEN_WEB_DELTA_Y};

// Identifier for the images
UINT32		guiBobbyRayTitle;
UINT32		guiBobbyROrderGrid;
UINT32		guiBobbyRLocationGraphic;
UINT32		guiDeliverySpeedGraphic;
UINT32		guiConfirmGraphic;
UINT32		guiTotalSaveArea;		//used as a savebuffer for the subtotal, s&h, and grand total values
UINT32		guiDropDownBorder;
UINT32		guiGoldArrowImages;
UINT32		guiPackageWeightImage;



BOOLEAN		gfReDrawBobbyOrder=FALSE;

INT32			giGrandTotal;
UINT32		guiShippingCost;
UINT32		guiSubTotal;

UINT8			gubSelectedLight=0;

BOOLEAN		gfDrawConfirmOrderGrpahic;
BOOLEAN		gfDestroyConfirmGrphiArea;

BOOLEAN		gfCanAcceptOrder;

UINT8			gubDropDownAction;
INT32			gbSelectedCity=-1;				//keeps track of the currently selected city
std::size_t	gubCityAtTopOfList;

UINT8			gubPurchaseAtTopOfList;
UINT8			guidivisor;

extern BOOLEAN gfBobbyRShipmentsDirty;

BOOLEAN		gfRemoveItemsFromStock=FALSE;

NewBobbyRayOrderStruct	*gpNewBobbyrShipments;
INT32			giNumberOfNewBobbyRShipment;

static UINT8 GetConfiguredBobbyRayPurchaseLimit()
{
	return static_cast<UINT8>(BobbyRayCommerceModel::PurchaseLimit(
		gGameExternalOptions.ubBobbyRayMaxPurchaseAmount));
}

static void ClearBobbyRayPurchases()
{
	memset(BobbyRayPurchases, 0, sizeof(BobbyRayPurchases));
}

struct NewBobbyRayOrderAllocationDeleter
{
	void operator()(NewBobbyRayOrderStruct *orders) const noexcept
	{
		if (orders) MemFree(orders);
	}
};

static BOOLEAN NormalizeNewBobbyRayOrder(NewBobbyRayOrderStruct& order)
{
	if (order.fActive &&
		(order.ubDeliveryLoc > BR_VANCOUVER || order.ubDeliveryMethod > 2))
	{
		return FALSE;
	}

	const UINT8 readableCount =
		BobbyRayCommerceModel::PurchaseRecordCount(order.ubNumberPurchases);
	UINT8 writeIndex = 0;
	for (UINT8 readIndex = 0; readIndex < readableCount; ++readIndex)
	{
		BobbyRayPurchaseStruct purchase = order.BobbyRayPurchase[readIndex];
		if (!purchase.ubNumberPurchased || purchase.usItemIndex == 0 ||
			purchase.usItemIndex >= MAXITEMS)
		{
			continue;
		}
		if (purchase.bItemQuality < 0)
			purchase.bItemQuality = 0;
		else if (purchase.bItemQuality > 100)
			purchase.bItemQuality = 100;
		order.BobbyRayPurchase[writeIndex++] = purchase;
	}
	for (std::size_t index = writeIndex;
		index < BobbyRayCommerceModel::PurchaseCapacity; ++index)
	{
		order.BobbyRayPurchase[index] = BobbyRayPurchaseStruct{};
	}
	order.ubNumberPurchases = writeIndex;
	if (order.fActive && writeIndex == 0)
		return FALSE;
	return TRUE;
}


//
//Buttons
//

//Clear Order Button
void BtnBobbyRClearOrderCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiBobbyRClearOrder;
INT32		guiBobbyRClearOrderImage;

//Accept Order Button
void BtnBobbyRAcceptOrderCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiBobbyRAcceptOrder;
INT32		guiBobbyRAcceptOrderImage;

//Back Button
void BtnBobbyRBackCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiBobbyRBack;
INT32		guiBobbyRBackImage;

//Home Button
void BtnBobbyRHomeCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiBobbyRHome;
INT32		guiBobbyRHomeImage;

//Goto Shipment Page Button
void BtnBobbyRGotoShipmentPageCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiBobbyRGotoShipmentPage;
INT32		giBobbyRGotoShipmentPageImage;


//mouse region for the shipping speed selection area
MOUSE_REGION	gSelectedShippingSpeedRegion[3];
void SelectShippingSpeedRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//mouse region for the confirm area
MOUSE_REGION	gSelectedConfirmOrderRegion;
void SelectConfirmOrderRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

// Mouse regions whose backing storage changes with externalized destinations.
std::vector<MOUSE_REGION> gSelectedDropDownRegions;
void SelectDropDownRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );
void SelectDropDownMovementCallBack(MOUSE_REGION * pRegion, INT32 iReason );

std::vector<MOUSE_REGION> gSelectedScrollAreaDropDownRegions;
void SelectScrollAreaDropDownRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );
void SelectScrollAreaDropDownMovementCallBack(MOUSE_REGION * pRegion, INT32 iReason );


//mouse region to activate the shipping location drop down
MOUSE_REGION	gSelectedActivateCityDroDownRegion;
void SelectActivateCityDroDownRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//mouse region to close the drop down menu
MOUSE_REGION	gSelectedCloseDropDownRegion;
void SelectCloseDroDownRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//mouse region to click on the title to go to the home page
MOUSE_REGION	gSelectedTitleLinkRegion;
void SelectTitleLinkRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//mouse region to click on the up or down arrow on the scroll area
MOUSE_REGION	gSelectedUpDownArrowOnScrollAreaRegion[2];
void SelectUpDownArrowOnScrollAreaRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//mouse region to click on the up or down arrow on the grid scroll area
MOUSE_REGION	 gSelectedUpDownArrowOnGridScrollAreaRegion[2];
void SelectUpDownArrowOnGridScrollAreaRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );
//mouse region for scroll column in the grid area.
std::vector<MOUSE_REGION> gSelectedGridScrollColumnRegions;
void SelectGridScrollColumnRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );
void SelectGridScrollColumnMovementCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//Dealtar's Airport Externalization.
INT16 gusCurShipmentDestinationID;
extern CPostalService gPostalService;
std::size_t guiNumOfDisplayedCities=0;
vector<PDestinationStruct> gDestinationTable;
vector<PShipmentStruct> gShipmentTable;

LaptopPageResourceOwner gBobbyRMailOrderResources;
LaptopPageResourceOwner gBobbyRDropDownResources;
LaptopPageResourceOwner gBobbyROrderGridResources;
BOOLEAN gfBobbyRDropDownRegionsCreated = FALSE;

void ClearBobbyRayOrderGridMouseRegions()
{
	gBobbyROrderGridResources.clear();
	gSelectedGridScrollColumnRegions.clear();
}

void RefreshBobbyRayDestinationSnapshot()
{
	gDestinationTable.clear();
	for (DestinationStruct& destination :
		gPostalService.LookupDestinationList())
	{
		gDestinationTable.push_back(&destination);
	}

	guiNumOfDisplayedCities = BobbyRayFulfilmentModel::VisibleCount(
		gDestinationTable.size(), 10);
	gubCityAtTopOfList = BobbyRayFulfilmentModel::NormalizeWindowStart(
		gubCityAtTopOfList, gDestinationTable.size(),
		guiNumOfDisplayedCities);
	if (gbSelectedCity < 0 ||
		!BobbyRayFulfilmentModel::IsIndexInRange(
			static_cast<std::size_t>(gbSelectedCity),
			gDestinationTable.size()) ||
		!gDestinationTable[gbSelectedCity])
	{
		gbSelectedCity = -1;
	}
}

static void KeepSelectedBobbyRayDestinationVisible()
{
	if (gbSelectedCity < 0 || guiNumOfDisplayedCities == 0) return;
	const std::size_t selection = static_cast<std::size_t>(gbSelectedCity);
	if (!BobbyRayFulfilmentModel::IsIndexInRange(
		selection, gDestinationTable.size())) return;
	if (selection < gubCityAtTopOfList)
		gubCityAtTopOfList = selection;
	else if (selection >= gubCityAtTopOfList + guiNumOfDisplayedCities)
		gubCityAtTopOfList = selection - guiNumOfDisplayedCities + 1;
	gubCityAtTopOfList = BobbyRayFulfilmentModel::NormalizeWindowStart(
		gubCityAtTopOfList, gDestinationTable.size(),
		guiNumOfDisplayedCities);
}

void RefreshBobbyRayShipmentSnapshot()
{
	gShipmentTable.clear();
	for (ShipmentStruct& shipment : gPostalService.LookupShipmentList())
	{
		gShipmentTable.push_back(&shipment);
	}
}

// WANNE: This method gets called when the shippment arrived
void BobbyRDeliveryCallback(RefToCShipmentManipulator ShipmentManipulator)
{
	// ScreenMsg(FONT_MCOLOR_RED, MSG_DEBUG, L"Shipment from Bobby Ray has arrived at %s!", ShipmentManipulator.GetDestination().wstrName.c_str());
	gusCurShipmentDestinationID = ShipmentManipulator.GetDestination().usID;	
}


BOOLEAN DrawShippingSpeedLights(UINT8 ubSelectedLight);
BOOLEAN CreateDestroyBobbyRDropDown( UINT8 ubDropDownAction );
void DrawSelectedCity( INT32 selectedCity );
void DisplayShippingLocationCity();
void DisplayShippingCosts( BOOLEAN fCalledFromOrderPage, INT32 iSubTotal, UINT16 usGridX, UINT16 usGridY, INT32 iOrderNum );
void RemovePurchasedItemsFromBobbyRayInventory();
BOOLEAN IsAnythingPurchasedFromBobbyRayPage();
void DrawGoldRectangle( INT32 cityNumber );
UINT32	CalcCostFromWeightOfPackage( UINT8	ubTypeOfService );
void ConfirmBobbyRPurchaseMessageBoxCallBack( UINT8 bExitValue );
void PurchaseBobbyOrder();
UINT32	CalcPackageTotalWeight();
void DisplayPackageWeight( );
void ShutDownBobbyRNewMailOrders();
// sorts the list of selected items for the mail order screen
void SortBobbyRayPurchases();
//ppp

void DrawOrderGoldRectangle(UINT16 usGridX, UINT16 usGridY);
void HandleBobbyRMailOrderKeyBoardInput();

void GameInitBobbyRMailOrder()
{
	if (is_networked)
		gubSelectedLight = 2; //hayden
	else
		gubSelectedLight = 0;

	ShutDownBobbyRNewMailOrders();
}

BOOLEAN EnterBobbyRMailOrder()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner staged;
	UINT16					i;

	CreateDestroyBobbyRDropDown(BR_DROP_DOWN_DESTROY);
	ClearBobbyRayOrderGridMouseRegions();
	gBobbyRMailOrderResources.clear();
	RefreshBobbyRayDestinationSnapshot();

	gfReDrawBobbyOrder = FALSE;
	gfDrawConfirmOrderGrpahic = FALSE;
	gfDestroyConfirmGrphiArea = FALSE;
	gfCanAcceptOrder = TRUE;
	gubDropDownAction = BR_DROP_DOWN_NO_ACTION;
	gubPurchaseAtTopOfList = 0;

	gShippingSpeedAreas[0] = iScreenWidthOffset + 585;
	gShippingSpeedAreas[1] = iScreenHeightOffset + 218 + LAPTOP_SCREEN_WEB_DELTA_Y;
	gShippingSpeedAreas[2] = iScreenWidthOffset + 585;
	gShippingSpeedAreas[3] = iScreenHeightOffset + 238 + LAPTOP_SCREEN_WEB_DELTA_Y;
	gShippingSpeedAreas[4] = iScreenWidthOffset + 585;
	gShippingSpeedAreas[5] = iScreenHeightOffset + 258 + LAPTOP_SCREEN_WEB_DELTA_Y;

	// load the Order Grid graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BobbyOrderGrid.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiBobbyROrderGrid)) return FALSE;

	// load the Location graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BobbyLocationBox.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc,
		guiBobbyRLocationGraphic)) return FALSE;

	// load the delivery speed graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BobbyDeliverySpeed.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc,
		guiDeliverySpeedGraphic)) return FALSE;

	// load the delivery speed graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	GetMLGFilename( VObjectDesc.ImageFile, MLG_CONFIRMORDER );
	if (!staged.addVideoObject(&VObjectDesc, guiConfirmGraphic)) return FALSE;

	// load the delivery speed graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\TotalSaveArea.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiTotalSaveArea)) return FALSE;

	// border
	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP( "INTERFACE\\TactPopUp.sti", VObjectDesc.ImageFile );
	if (!staged.addVideoObject(&VObjectDesc, guiDropDownBorder)) return FALSE;

	// Gold Arrow for the scroll area
	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP( "LAPTOP\\GoldArrows.sti", VObjectDesc.ImageFile );
	if (!staged.addVideoObject(&VObjectDesc, guiGoldArrowImages)) return FALSE;

	// Package Weight Graphic
	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP( "LAPTOP\\PackageWeight.sti", VObjectDesc.ImageFile );
	if (!staged.addVideoObject(&VObjectDesc,
		guiPackageWeightImage)) return FALSE;


	if (!InitBobbyRWoodBackground(staged)) return FALSE;

	//
	//Init the button areas
	//

	// Clear Order button
	if (!staged.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\EraseOrderButton.sti", -1, 0, -1, 1, -1),
		guiBobbyRClearOrderImage)) return FALSE;
	if (!staged.addButton(CreateIconAndTextButton( guiBobbyRClearOrderImage, BobbyROrderFormText[BOBBYR_CLEAR_ORDER], BOBBYR_ORDER_TITLE_TEXT_FONT,
													BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
													BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
													TEXT_CJUSTIFIED,
													BOBBYR_CLEAR_ORDER_X, BOBBYR_CLEAR_ORDER_Y+4, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
													DEFAULT_MOVE_CALLBACK, BtnBobbyRClearOrderCallback),
		guiBobbyRClearOrder)) return FALSE;
	SetButtonCursor(guiBobbyRClearOrder, CURSOR_LAPTOP_SCREEN);
	SpecifyDisabledButtonStyle( guiBobbyRClearOrder, DISABLED_STYLE_NONE );
 //inshy: fix position of text for buttons
if(g_lang == i18n::Lang::fr) {
	SpecifyButtonTextOffsets( guiBobbyRClearOrder, 13, 10, TRUE );
} else {
	SpecifyButtonTextOffsets( guiBobbyRClearOrder, 39, 10, TRUE );
}

	// Accept Order button
	if (!staged.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\AcceptOrderButton.sti", 2, 0, -1, 1, -1),
		guiBobbyRAcceptOrderImage)) return FALSE;
	if (!staged.addButton(CreateIconAndTextButton( guiBobbyRAcceptOrderImage, BobbyROrderFormText[BOBBYR_ACCEPT_ORDER], BOBBYR_ORDER_TITLE_TEXT_FONT,
													BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
													BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
													TEXT_CJUSTIFIED,
													BOBBYR_ACCEPT_ORDER_X, BOBBYR_ACCEPT_ORDER_Y+4, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
													DEFAULT_MOVE_CALLBACK, BtnBobbyRAcceptOrderCallback),
		guiBobbyRAcceptOrder)) return FALSE;
	SetButtonCursor( guiBobbyRAcceptOrder, CURSOR_LAPTOP_SCREEN);
 //inshy: fix position of text for buttons
if(g_lang == i18n::Lang::fr) {
	SpecifyButtonTextOffsets( guiBobbyRAcceptOrder, 15, 24, TRUE );
} else {
	SpecifyButtonTextOffsets( guiBobbyRAcceptOrder, 43, 24, TRUE );
}
	SpecifyDisabledButtonStyle( guiBobbyRAcceptOrder, DISABLED_STYLE_SHADED );

	if( gbSelectedCity == -1 )
		DisableButton( guiBobbyRAcceptOrder );

	//if there is anything to buy, dont disable the accept button
//	if( !IsAnythingPurchasedFromBobbyRayPage() )
	{
//		DisableButton( guiBobbyRAcceptOrder );
	}


	if (!staged.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\CatalogueButton.sti", -1, 0, -1, 1, -1),
		guiBobbyRBackImage)) return FALSE;
	if (!staged.addButton(CreateIconAndTextButton( guiBobbyRBackImage, BobbyROrderFormText[BOBBYR_BACK], BOBBYR_GUNS_BUTTON_FONT,
													BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
													BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
													TEXT_CJUSTIFIED,
													BOBBYR_BACK_BUTTON_X, BOBBYR_BACK_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
													DEFAULT_MOVE_CALLBACK, BtnBobbyRBackCallback),
		guiBobbyRBack)) return FALSE;
	SetButtonCursor(guiBobbyRBack, CURSOR_LAPTOP_SCREEN);

	if (!staged.addButtonImage(UniqueButtonImageHandle(
		UseLoadedButtonImage(guiBobbyRBackImage, -1, 0, -1, 1, -1)),
		guiBobbyRHomeImage)) return FALSE;
	if (!staged.addButton(CreateIconAndTextButton( guiBobbyRHomeImage, BobbyROrderFormText[BOBBYR_HOME], BOBBYR_GUNS_BUTTON_FONT,
													BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
													BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
													TEXT_CJUSTIFIED,
													BOBBYR_HOME_BUTTON_X, BOBBYR_HOME_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
													DEFAULT_MOVE_CALLBACK, BtnBobbyRHomeCallback),
		guiBobbyRHome)) return FALSE;
	SetButtonCursor(guiBobbyRHome, CURSOR_LAPTOP_SCREEN);


	if (!staged.addButtonImage(UniqueButtonImageHandle(
		UseLoadedButtonImage(guiBobbyRBackImage, -1, 0, -1, 1, -1)),
		giBobbyRGotoShipmentPageImage)) return FALSE;
	if (!staged.addButton(CreateIconAndTextButton( giBobbyRGotoShipmentPageImage, BobbyROrderFormText[BOBBYR_GOTOSHIPMENT_PAGE], BOBBYR_GUNS_BUTTON_FONT,
													BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
													BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
													TEXT_CJUSTIFIED,
													BOBBYR_SHIPMENT_BUTTON_X, BOBBYR_SHIPMENT_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
													DEFAULT_MOVE_CALLBACK, BtnBobbyRGotoShipmentPageCallback ),
		guiBobbyRGotoShipmentPage)) return FALSE;
	SetButtonCursor(guiBobbyRGotoShipmentPage, CURSOR_LAPTOP_SCREEN);


	for(i=0; i<3; i++)
	{

		MSYS_DefineRegion( &gSelectedShippingSpeedRegion[i], gShippingSpeedAreas[i*2], gShippingSpeedAreas[i*2+1] , (UINT16)(gShippingSpeedAreas[i*2]+SHIPPING_SPEED_LIGHT_WIDTH), (UINT16)(gShippingSpeedAreas[i*2+1]+SHIPPING_SPEED_LIGHT_HEIGHT), MSYS_PRIORITY_HIGH,
								CURSOR_WWW, MSYS_NO_CALLBACK, SelectShippingSpeedRegionCallBack );
		if (!staged.addRegion(gSelectedShippingSpeedRegion[i])) return FALSE;
		MSYS_SetRegionUserData( &gSelectedShippingSpeedRegion[ i ], 0, i);
	}

	//confirmorder mouse region, occupies the entrie screen and is present only when the confirm order graphic
	//s on screen.	When user clicks anywhere the graphic disappears
	MSYS_DefineRegion( &gSelectedConfirmOrderRegion, LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y , LAPTOP_SCREEN_LR_X, LAPTOP_SCREEN_WEB_LR_Y, MSYS_PRIORITY_HIGH+1,
							CURSOR_WWW, MSYS_NO_CALLBACK, SelectConfirmOrderRegionCallBack);
	if (!staged.addRegion(gSelectedConfirmOrderRegion)) return FALSE;
	MSYS_DisableRegion(&gSelectedConfirmOrderRegion);


	//click on the shipping location to activate the drop down menu
	MSYS_DefineRegion( &gSelectedActivateCityDroDownRegion, BOBBYR_SHIPPING_LOC_AREA_L_X, BOBBYR_SHIPPING_LOC_AREA_T_Y, BOBBYR_SHIPPING_LOC_AREA_R_X, BOBBYR_SHIPPING_LOC_AREA_B_Y, MSYS_PRIORITY_HIGH,
							CURSOR_WWW, MSYS_NO_CALLBACK, SelectActivateCityDroDownRegionCallBack);
	if (!staged.addRegion(gSelectedActivateCityDroDownRegion)) return FALSE;
	if (gDestinationTable.empty())
		MSYS_DisableRegion(&gSelectedActivateCityDroDownRegion);


	//click anywhere on the screen to close the window( only when the drop down window is active)
	MSYS_DefineRegion( &gSelectedCloseDropDownRegion, LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y , LAPTOP_SCREEN_LR_X, LAPTOP_SCREEN_WEB_LR_Y, MSYS_PRIORITY_HIGH-1,
							CURSOR_LAPTOP_SCREEN, MSYS_NO_CALLBACK, SelectCloseDroDownRegionCallBack);
	if (!staged.addRegion(gSelectedCloseDropDownRegion)) return FALSE;
	MSYS_DisableRegion(&gSelectedCloseDropDownRegion);

	if (!CreateBobbyRayOrderTitle(staged)) return FALSE;
	gBobbyRMailOrderResources = std::move(staged);

	guiShippingCost = 0;

	gfRemoveItemsFromStock = FALSE;

	// silversurfer: This sorts the list of items that we marked for purchase. 
	// This helps us get rid of the empty line display errors in the mail order screen.
	SortBobbyRayPurchases();

	RenderBobbyRMailOrder();
	return(TRUE);
}

void ExitBobbyRMailOrder()
{
	//if we are to remove the items from stock
	if( gfRemoveItemsFromStock )
	{
		//Remove the items for Boby Rqys Inventory
		RemovePurchasedItemsFromBobbyRayInventory();
	}


	//if the drop down box is active, destroy it
	gubDropDownAction = BR_DROP_DOWN_DESTROY;
	CreateDestroyBobbyRDropDown( BR_DROP_DOWN_DESTROY );
	ClearBobbyRayOrderGridMouseRegions();
	gBobbyRMailOrderResources.clear();
}

void HandleBobbyRMailOrder()
{
	if( gfReDrawBobbyOrder )
	{
//		RenderBobbyRMailOrder();
		fPausedReDrawScreenFlag = TRUE;
		gfReDrawBobbyOrder = FALSE;
	}

	if(	gfDrawConfirmOrderGrpahic )
	{
		HVOBJECT hPixHandle;

		// Bobbyray title
		GetVideoObject(&hPixHandle, guiConfirmGraphic);
		BltVideoObjectOutlineShadowFromIndex( FRAME_BUFFER, guiConfirmGraphic, 0, BOBBYR_CONFIRM_ORDER_X+3, BOBBYR_CONFIRM_ORDER_Y+3);

		BltVideoObject(FRAME_BUFFER, hPixHandle, 0, BOBBYR_CONFIRM_ORDER_X, BOBBYR_CONFIRM_ORDER_Y, VO_BLT_SRCTRANSPARENCY,NULL);
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);

		gfDrawConfirmOrderGrpahic = FALSE;
	}

	if( gfDestroyConfirmGrphiArea )
	{
		gfDestroyConfirmGrphiArea = FALSE;
		gfReDrawBobbyOrder = TRUE;
	MSYS_DisableRegion(&gSelectedConfirmOrderRegion);
		gfCanAcceptOrder = TRUE;
	}

	if( gubDropDownAction != BR_DROP_DOWN_NO_ACTION )
	{
		CreateDestroyBobbyRDropDown( gubDropDownAction );

		if( gubDropDownAction == BR_DROP_DOWN_CREATE )
			gubDropDownAction = BR_DROP_DOWN_DISPLAY;
		else
			gubDropDownAction = BR_DROP_DOWN_NO_ACTION;
	}

	HandleBobbyRMailOrderKeyBoardInput();
}

void RenderBobbyRMailOrder()
{
	UINT16 usPosY;
	HVOBJECT hPixHandle;
	UINT16 usHeight;//usWidth,
	CHAR16	sTemp[ 128 ];

	DrawBobbyRWoodBackground();

	DrawBobbyROrderTitle();

	// Order Grid
	GetVideoObject(&hPixHandle, guiBobbyROrderGrid);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, BOBBYR_ORDERGRID_X, BOBBYR_ORDERGRID_Y, VO_BLT_SRCTRANSPARENCY,NULL);

	// Location graphic
	GetVideoObject(&hPixHandle, guiBobbyRLocationGraphic);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, BOBBYR_LOCATION_BOX_X, BOBBYR_LOCATION_BOX_Y, VO_BLT_SRCTRANSPARENCY,NULL);

	// DeliverySpeedGraphic
	GetVideoObject(&hPixHandle, guiDeliverySpeedGraphic);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, BOBBYR_DELIVERYSPEED_X, BOBBYR_DELIVERYSPEED_Y, VO_BLT_SRCTRANSPARENCY,NULL);

	//Package Weight
	GetVideoObject(&hPixHandle, guiPackageWeightImage);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, BOBBYR_PACKAXGE_WEIGHT_X, BOBBYR_PACKAXGE_WEIGHT_Y, VO_BLT_SRCTRANSPARENCY,NULL);


	//
	// Display the STATIC text
	//

	//Output the title
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_ORDER_FORM], BOBBYR_ORDER_FORM_TITLE_X, BOBBYR_ORDER_FORM_TITLE_Y, BOBBYR_ORDER_FORM_TITLE_WIDTH, BOBBYR_ORDER_TITLE_TEXT_FONT, BOBBYR_ORDER_TITLE_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

/*
	//Output the qty
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_QTY], BOBBYR_GRID_FIRST_COLUMN_X, BOBBYR_GRID_FIRST_COLUMN_Y-BOBBYR_GRID_TITLE_OFFSET, BOBBYR_GRID_FIRST_COLUMN_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	//Create a string for the weight ( %s ) ( where %s is the weight string, either kg or lbs )
	swprintf( sTemp, BobbyROrderFormText[BOBBYR_WEIGHT], GetWeightUnitString( ) );

	//Output the Weight
	DisplayWrappedString(BOBBYR_GRID_SECOND_COLUMN_X, BOBBYR_GRID_SECOND_COLUMN_Y-30, BOBBYR_GRID_SECOND_COLUMN_WIDTH, 2, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, sTemp, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED );

	//Output the name
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_NAME], BOBBYR_GRID_THIRD_COLUMN_X, BOBBYR_GRID_THIRD_COLUMN_Y-BOBBYR_GRID_TITLE_OFFSET, BOBBYR_GRID_THIRD_COLUMN_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	//Output the unit price
	DisplayWrappedString(BOBBYR_GRID_FOURTH_COLUMN_X, BOBBYR_GRID_FOURTH_COLUMN_Y-30, BOBBYR_GRID_FOURTH_COLUMN_WIDTH, 2, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, BobbyROrderFormText[BOBBYR_UNIT_PRICE], FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	//Output the total
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_TOTAL], BOBBYR_GRID_FIFTH_COLUMN_X, BOBBYR_GRID_FIFTH_COLUMN_Y-BOBBYR_GRID_TITLE_OFFSET, BOBBYR_GRID_FIFTH_COLUMN_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	//Output the sub total, shipping and handling, and the grand total
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_SUB_TOTAL], BOBBYR_SUBTOTAL_X, BOBBYR_SUBTOTAL_Y, BOBBYR_SUBTOTAL_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_S_H], BOBBYR_SUBTOTAL_X, BOBBYR_SHIPPING_N_HANDLE_Y, BOBBYR_SUBTOTAL_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_GRAND_TOTAL], BOBBYR_SUBTOTAL_X, BOBBYR_GRAND_TOTAL_Y, BOBBYR_SUBTOTAL_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
*/

	//Output the shipping location
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_SHIPPING_LOCATION], BOBBYR_SHIPPING_LOCATION_TEXT_X, BOBBYR_SHIPPING_LOCATION_TEXT_Y, 0, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	//Output the shipping speed
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_SHIPPING_SPEED], BOBBYR_SHIPPING_SPEED_X, BOBBYR_SHIPPING_SPEED_Y, 0, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	//Create a string for the weight ( %s ) ( where %s is the weight string, either kg or lbs )
	swprintf( sTemp, BobbyROrderFormText[BOBBYR_COST], GetWeightUnitString( ) );

	//Output the cost
	DrawTextToScreen( sTemp, BOBBYR_SHIPPING_COST_X, BOBBYR_SHIPPING_SPEED_Y, 0, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	//Output the overnight, business days, standard service
	usPosY = BOBBYR_OVERNIGHT_EXPRESS_Y;
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_OVERNIGHT_EXPRESS], BOBBYR_SHIPPING_SPEED_X, usPosY, 0, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usPosY +=BOBBYR_GRID_ROW_OFFSET;
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_BUSINESS_DAYS], BOBBYR_SHIPPING_SPEED_X, usPosY, 0, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usPosY +=BOBBYR_GRID_ROW_OFFSET;
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_STANDARD_SERVICE], BOBBYR_SHIPPING_SPEED_X, usPosY, 0, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

//	DisplayPurchasedItems();
	DisplayPurchasedItems( TRUE, BOBBYR_ORDERGRID_X, BOBBYR_ORDERGRID_Y, BobbyRayPurchases, FALSE, -1 );

	DrawShippingSpeedLights(gubSelectedLight);

	DisplayShippingLocationCity();

	//Display the 'used' text at the bottom of the screen
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_USED_TEXT], BOBBYR_USED_WARNING_X, BOBBYR_USED_WARNING_Y+1, 0, BOBBYR_DISCLAIMER_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED | TEXT_SHADOWED );

	//Display the minimum weight disclaimer at the bottom of the page
	usHeight = GetFontHeight( BOBBYR_DISCLAIMER_FONT ) + 2;
	swprintf( sTemp, L"%s %2.1f %s.", BobbyROrderFormText[BOBBYR_MINIMUM_WEIGHT], GetWeightBasedOnMetricOption( MIN_SHIPPING_WEIGHT ) / 10.0, GetWeightUnitString() );
	DrawTextToScreen( sTemp, BOBBYR_USED_WARNING_X, (UINT16)(BOBBYR_USED_WARNING_Y+usHeight+1), 0, BOBBYR_DISCLAIMER_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED | TEXT_SHADOWED );


	//Calculate and display the total package weight
	DisplayPackageWeight( );

	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}



void BtnBobbyRClearOrderCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

		ClearBobbyRayPurchases();
		gubSelectedLight = 0;
		gfReDrawBobbyOrder = TRUE;
		gbSelectedCity = -1;
		gubCityAtTopOfList = 0;
		gubPurchaseAtTopOfList = 0;

		//Get rid of the city drop dowm, if it is being displayed
		gubDropDownAction = BR_DROP_DOWN_DESTROY;

		//disable the accept order button
		DisableButton( guiBobbyRAcceptOrder );

		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


void BtnBobbyRAcceptOrderCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

		const bool hasSelectedDestination = gbSelectedCity >= 0 &&
			BobbyRayFulfilmentModel::IsIndexInRange(
				static_cast<std::size_t>(gbSelectedCity),
				gDestinationTable.size()) &&
			gDestinationTable[gbSelectedCity];
		if( guiSubTotal && gfCanAcceptOrder && hasSelectedDestination )
		{
			//if the player doesnt have enough money
			if( LaptopSaveInfo.iCurrentBalance < giGrandTotal )
			{
				DoLapTopMessageBox( MSG_BOX_LAPTOP_DEFAULT, BobbyROrderFormText[BOBBYR_CANT_AFFORD_PURCHASE], LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);
			}
			else
			{
				CHAR16	zTemp[ 128 ];

				//Dealtar's Airport Externalization.
				/*Old Code:
				//if the city is Drassen, and the airport sector is player controlled
				if( gbSelectedCity == BR_DRASSEN && !StrategicMap[ SECTOR_INFO_TO_STRATEGIC_INDEX( SEC_B13 ) ].fEnemyControlled || is_client )
				* Dealtar's New Code: */
				//if the selected destination's sector is player controlled
				if( (gDestinationTable[gbSelectedCity]->ubMapX >= 0 ||
					 gDestinationTable[gbSelectedCity]->ubMapY >= 0 ||
					 gDestinationTable[gbSelectedCity]->sGridNo >= 0) &&				
					 !StrategicMap[ CALCULATE_STRATEGIC_INDEX(gDestinationTable[gbSelectedCity]->ubMapX, gDestinationTable[gbSelectedCity]->ubMapY) ].fEnemyControlled || is_client)
				//End Dealtar's Code.
				{
					//Quick hack to bypass the confirmation box
					ConfirmBobbyRPurchaseMessageBoxCallBack( MSG_BOX_RETURN_YES );
				}
				else
				{
					//else pop up a confirmation box
					//swprintf( zTemp, BobbyROrderFormText[BOBBYR_CONFIRM_DEST],	*BobbyROrderLocations[gbSelectedCity].psCityLoc );
					
					// WANNE: Replaced hard coded town names with the externalized ones (from ShippingDestination.xml).
					swprintf( zTemp, BobbyROrderFormText[BOBBYR_CONFIRM_DEST],	gDestinationTable[gbSelectedCity]->wstrName.c_str() );
					DoLapTopMessageBox( MSG_BOX_LAPTOP_DEFAULT, zTemp, LAPTOP_SCREEN, MSG_BOX_FLAG_YESNO, ConfirmBobbyRPurchaseMessageBoxCallBack );
				}

			}
		}

		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


void DisplayPurchasedItems( BOOLEAN fCalledFromOrderPage, UINT16 usGridX, UINT16 usGridY, BobbyRayPurchaseStruct *pBobbyRayPurchase, BOOLEAN fJustDisplayTitles, INT32 iOrderNum )
{
	ClearBobbyRayOrderGridMouseRegions();
	UINT16	i,j;
	CHAR16	sText[400];
	CHAR16	sBack[400];
	CHAR16	sTemp[20];
	UINT16	usPosY, usPosX, usHeight;
	UINT32	uiTotal;
	UINT16	usStringLength;
	UINT16	usPixLength;
	CHAR16	OneChar[2];
	INT32 iSubTotal = 0;
	UINT32 uiDestPitchBYTES;
	UINT8	*pDestBuf;
	HVOBJECT	hArrowHandle;
	//JMich TODO add a way to check for empty index in the middle of pBobbyRayPurchase
	// silversurfer: Not necessary anymore. Function SortBobbyRayPurchases() will remove any empty indexes for us before we call the mail order screen.

	//Output the qty
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_QTY], (UINT16)(usGridX+BOBBYR_GRID_FIRST_COLUMN_X), (UINT16)(usGridY+BOBBYR_GRID_FIRST_COLUMN_Y-BOBBYR_GRID_TITLE_OFFSET), BOBBYR_GRID_FIRST_COLUMN_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	//Create a string for the weight ( %s ) ( where %s is the weight string, either kg or lbs )
	swprintf( sTemp, BobbyROrderFormText[BOBBYR_WEIGHT], GetWeightUnitString( ) );

	//Output the Weight
	DisplayWrappedString((UINT16)(usGridX+BOBBYR_GRID_SECOND_COLUMN_X), (UINT16)(usGridY+BOBBYR_GRID_SECOND_COLUMN_Y-30), BOBBYR_GRID_SECOND_COLUMN_WIDTH, 2, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, sTemp, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED );

	//Output the name
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_NAME], (UINT16)(usGridX+BOBBYR_GRID_THIRD_COLUMN_X), (UINT16)(usGridY+BOBBYR_GRID_THIRD_COLUMN_Y-BOBBYR_GRID_TITLE_OFFSET), BOBBYR_GRID_THIRD_COLUMN_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	//Output the unit price
	DisplayWrappedString((UINT16)(usGridX+BOBBYR_GRID_FOURTH_COLUMN_X), (UINT16)(usGridY+BOBBYR_GRID_FOURTH_COLUMN_Y-30), BOBBYR_GRID_FOURTH_COLUMN_WIDTH, 2, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, BobbyROrderFormText[BOBBYR_UNIT_PRICE], FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	//Output the total
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_TOTAL], (UINT16)(usGridX+BOBBYR_GRID_FIFTH_COLUMN_X), (UINT16)(usGridY+BOBBYR_GRID_FIFTH_COLUMN_Y-BOBBYR_GRID_TITLE_OFFSET), BOBBYR_GRID_FIFTH_COLUMN_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	//Output the sub total, shipping and handling, and the grand total
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_SUB_TOTAL], (UINT16)(usGridX+BOBBYR_SUBTOTAL_X), (UINT16)(usGridY+BOBBYR_SUBTOTAL_Y), BOBBYR_SUBTOTAL_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_S_H], (UINT16)(usGridX+BOBBYR_SUBTOTAL_X), (UINT16)(usGridY+BOBBYR_SHIPPING_N_HANDLE_Y), BOBBYR_SUBTOTAL_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	DrawTextToScreen(BobbyROrderFormText[BOBBYR_GRAND_TOTAL], (UINT16)(usGridX+BOBBYR_SUBTOTAL_X), (UINT16)(usGridY+BOBBYR_GRAND_TOTAL_Y), BOBBYR_SUBTOTAL_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);


	if( fJustDisplayTitles )
	{
		return;
	}

	if( fCalledFromOrderPage )
	{
		guiSubTotal = 0;
		giGrandTotal = 0;
	}

	if( pBobbyRayPurchase == NULL )
	{
		return;
	}
	const UINT16 purchaseLimit = static_cast<UINT16>(fCalledFromOrderPage
		? GetConfiguredBobbyRayPurchaseLimit()
		: BobbyRayCommerceModel::PurchaseCapacity);

	//loop through the array of purchases to display only the items that are purchased
	usPosY = usGridY+BOBBYR_GRID_FIRST_COLUMN_Y + 4;
	const UINT16 displayEnd = static_cast<UINT16>(
		BobbyRayCommerceModel::BoundedLength(
			static_cast<std::size_t>(gubPurchaseAtTopOfList) +
				BOBBYR_NUM_DISPLAYED_ITEMS,
			purchaseLimit));
	for(i = gubPurchaseAtTopOfList; i < displayEnd; ++i)
	{
		//if the item was purchased
//		if( BobbyRayPurchases[ i ].ubNumberPurchased )
		if( pBobbyRayPurchase[i].ubNumberPurchased &&
			pBobbyRayPurchase[i].usItemIndex < MAXITEMS)
		{
			uiTotal = 0;

			//Display the qty, order#, item name, unit price and the total

			//qty
			swprintf(sTemp, L"%3d", pBobbyRayPurchase[i].ubNumberPurchased );
			DrawTextToScreen(sTemp, (UINT16)(usGridX+BOBBYR_GRID_FIRST_COLUMN_X-2), usPosY, BOBBYR_GRID_FIRST_COLUMN_WIDTH, BOBBYR_ORDER_DYNAMIC_TEXT_FONT, BOBBYR_ORDER_DYNAMIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);

			//weight
			swprintf( sTemp, L"%3.1f", GetWeightBasedOnMetricOption( Item[ pBobbyRayPurchase[i].usItemIndex ].ubWeight ) / (FLOAT)( 10.0 ) * pBobbyRayPurchase[i].ubNumberPurchased );
			DrawTextToScreen(sTemp, (UINT16)(usGridX+BOBBYR_GRID_SECOND_COLUMN_X-2), usPosY, BOBBYR_GRID_SECOND_COLUMN_WIDTH, BOBBYR_ORDER_DYNAMIC_TEXT_FONT, BOBBYR_ORDER_DYNAMIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);

			//Display Items Name
			if( pBobbyRayPurchase[i].fUsed )
			{
				LoadBRName(pBobbyRayPurchase[i].usItemIndex,sBack);
				swprintf(sText, L"%s %s", L"*", sBack);
			}
			else
				LoadBRName(pBobbyRayPurchase[i].usItemIndex,sText);

			//if the name is bigger then can fit into the slot, reduce the size
			if( StringPixLength(sText, BOBBYR_ORDER_DYNAMIC_TEXT_FONT) > BOBBYR_GRID_THIRD_COLUMN_WIDTH-4 )
			{
				usStringLength = wcslen(sText);
				usPixLength=0;
				OneChar[1] = L'\0';
				for(j=0; (j<usStringLength)&&(usPixLength < BOBBYR_GRID_THIRD_COLUMN_WIDTH-16); j++)
				{
					sBack[j] = sText[j];
					OneChar[0] = sBack[j];
					usPixLength += StringPixLength(OneChar, BOBBYR_ORDER_DYNAMIC_TEXT_FONT);
				}
				sBack[j] = 0;
				swprintf(sText, L"%s...", sBack);
			}

			DrawTextToScreen(sText, (UINT16)(usGridX+BOBBYR_GRID_THIRD_COLUMN_X+2), usPosY, BOBBYR_GRID_THIRD_COLUMN_WIDTH, BOBBYR_ORDER_DYNAMIC_TEXT_FONT, BOBBYR_ORDER_DYNAMIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

			//unit price
			DrawTextToScreen(FormatMoney(CalcBobbyRayCost(pBobbyRayPurchase[i].usItemIndex, pBobbyRayPurchase[i].usBobbyItemIndex, pBobbyRayPurchase[i].fUsed, pBobbyRayPurchase[i].bItemQuality)).data(), (UINT16)(usGridX+BOBBYR_GRID_FOURTH_COLUMN_X-2), usPosY, BOBBYR_GRID_FOURTH_COLUMN_WIDTH, BOBBYR_ORDER_DYNAMIC_TEXT_FONT, BOBBYR_ORDER_DYNAMIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);

			uiTotal += CalcBobbyRayCost( pBobbyRayPurchase[i].usItemIndex, pBobbyRayPurchase[i].usBobbyItemIndex, pBobbyRayPurchase[i].fUsed, pBobbyRayPurchase[i].bItemQuality ) * pBobbyRayPurchase[i].ubNumberPurchased;

			DrawTextToScreen(FormatMoney(uiTotal).data(), (UINT16)(usGridX+BOBBYR_GRID_FIFTH_COLUMN_X-2), usPosY, BOBBYR_GRID_FIFTH_COLUMN_WIDTH, BOBBYR_ORDER_DYNAMIC_TEXT_FONT, BOBBYR_ORDER_DYNAMIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);

			usPosY += BOBBYR_GRID_ROW_OFFSET;
		}
	}
	guidivisor = 0;
	for (i = 0; i < purchaseLimit; ++i)
	{
		if( pBobbyRayPurchase[i].ubNumberPurchased )
		{
			guidivisor++;
			uiTotal = CalcBobbyRayCost( pBobbyRayPurchase[i].usItemIndex, pBobbyRayPurchase[i].usBobbyItemIndex, pBobbyRayPurchase[i].fUsed, pBobbyRayPurchase[i].bItemQuality ) * pBobbyRayPurchase[i].ubNumberPurchased;
			//add the current item total to the sub total
			if( fCalledFromOrderPage )
			{
				guiSubTotal += uiTotal;
			}
			else
			{
				iSubTotal += uiTotal;
			}
		}
	}
	if (guidivisor != 0)
	{
		if (guidivisor < 11)
		{
			guidivisor = 1;
		}
		else
		{
			guidivisor -= 9;
		}
	}
	//Add the scroll list mouse regions
	ColorFillVideoSurfaceArea( FRAME_BUFFER, usGridX + BOBBYR_GRID_SCROLL_COLUMN_X, usGridY + BOBBYR_GRID_SCROLL_COLUMN_Y, usGridX + BOBBYR_GRID_SCROLL_COLUMN_X+BOBBYR_GRID_SCROLL_COLUMN_WIDTH, usGridY + BOBBYR_GRID_SCROLL_COLUMN_Y + BOBBYR_GRID_SCROLL_COLUMN_HEIGHT, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );

	pDestBuf = LockVideoSurface( FRAME_BUFFER, &uiDestPitchBYTES );
	SetClippingRegionAndImageWidth( uiDestPitchBYTES, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

	//top
	LineDraw(FALSE, usGridX + BOBBYR_GRID_SCROLL_COLUMN_X, usGridY + BOBBYR_GRID_SCROLL_COLUMN_Y, usGridX + BOBBYR_GRID_SCROLL_COLUMN_X + BOBBYR_GRID_SCROLL_COLUMN_WIDTH, usGridY + BOBBYR_GRID_SCROLL_COLUMN_Y, Get16BPPColor( FROMRGB( 235, 222, 171 ) ), pDestBuf);
	//bottom
	LineDraw(FALSE, usGridX + BOBBYR_GRID_SCROLL_COLUMN_X, usGridY + BOBBYR_GRID_SCROLL_COLUMN_Y + BOBBYR_GRID_SCROLL_COLUMN_HEIGHT, usGridX + BOBBYR_GRID_SCROLL_COLUMN_X + BOBBYR_GRID_SCROLL_COLUMN_WIDTH, usGridY + BOBBYR_GRID_SCROLL_COLUMN_Y+BOBBYR_GRID_SCROLL_COLUMN_HEIGHT, Get16BPPColor( FROMRGB( 235, 222, 171 ) ), pDestBuf);
	//left
	LineDraw(FALSE, usGridX + BOBBYR_GRID_SCROLL_COLUMN_X, usGridY + BOBBYR_GRID_SCROLL_COLUMN_Y, usGridX + BOBBYR_GRID_SCROLL_COLUMN_X, usGridY + BOBBYR_GRID_SCROLL_COLUMN_Y + BOBBYR_GRID_SCROLL_COLUMN_HEIGHT, Get16BPPColor( FROMRGB( 235, 222, 171 ) ), pDestBuf);
	//right
	LineDraw(FALSE, usGridX + BOBBYR_GRID_SCROLL_COLUMN_X + BOBBYR_GRID_SCROLL_COLUMN_WIDTH, usGridY + BOBBYR_GRID_SCROLL_COLUMN_Y, usGridX + BOBBYR_GRID_SCROLL_COLUMN_X + BOBBYR_GRID_SCROLL_COLUMN_WIDTH, usGridY + BOBBYR_GRID_SCROLL_COLUMN_Y + BOBBYR_GRID_SCROLL_COLUMN_HEIGHT, Get16BPPColor( FROMRGB( 235, 222, 171 ) ), pDestBuf);


	// unlock frame buffer
	UnLockVideoSurface( FRAME_BUFFER );

	//Arrows

	//get and display the up and down arrows
	GetVideoObject(&hArrowHandle, guiGoldArrowImages);
	//top arrow
	BltVideoObject(FRAME_BUFFER, hArrowHandle, 1, usGridX + BOBBYR_GRID_SCROLL_UP_ARROW_X, usGridY + BOBBYR_GRID_SCROLL_UP_ARROW_Y, VO_BLT_SRCTRANSPARENCY,NULL);

	//bottom arrow
	BltVideoObject(FRAME_BUFFER, hArrowHandle, 0, usGridX + BOBBYR_GRID_SCROLL_DOWN_ARROW_X, usGridY + BOBBYR_GRID_SCROLL_DOWN_ARROW_Y, VO_BLT_SRCTRANSPARENCY,NULL);

	LaptopPageResourceOwner stagedGridResources;
	MSYS_DefineRegion( &gSelectedUpDownArrowOnGridScrollAreaRegion[0], usGridX + BOBBYR_GRID_SCROLL_UP_ARROW_X, usGridY + BOBBYR_GRID_SCROLL_UP_ARROW_Y, (UINT16)(usGridX + BOBBYR_GRID_SCROLL_UP_ARROW_X+BOBBYR_SCROLL_ARROW_WIDTH),
		(UINT16)(usGridY + BOBBYR_GRID_SCROLL_UP_ARROW_Y+BOBBYR_SCROLL_ARROW_HEIGHT), MSYS_PRIORITY_HIGH, CURSOR_WWW, MSYS_NO_CALLBACK, SelectUpDownArrowOnGridScrollAreaRegionCallBack);
	if (!stagedGridResources.addRegion(
		gSelectedUpDownArrowOnGridScrollAreaRegion[0])) return;
	MSYS_SetRegionUserData( &gSelectedUpDownArrowOnGridScrollAreaRegion[ 0 ], 0, 0);
	MSYS_EnableRegion(&gSelectedUpDownArrowOnGridScrollAreaRegion[0]);
	MSYS_DefineRegion( &gSelectedUpDownArrowOnGridScrollAreaRegion[1], usGridX + BOBBYR_GRID_SCROLL_DOWN_ARROW_X, usGridY + BOBBYR_GRID_SCROLL_DOWN_ARROW_Y, (UINT16)(usGridX + BOBBYR_GRID_SCROLL_DOWN_ARROW_X+BOBBYR_SCROLL_ARROW_WIDTH),
		(UINT16)(usGridY + BOBBYR_GRID_SCROLL_DOWN_ARROW_Y+BOBBYR_SCROLL_ARROW_HEIGHT), MSYS_PRIORITY_HIGH, CURSOR_WWW, MSYS_NO_CALLBACK, SelectUpDownArrowOnGridScrollAreaRegionCallBack);
	if (!stagedGridResources.addRegion(
		gSelectedUpDownArrowOnGridScrollAreaRegion[1])) return;
	MSYS_SetRegionUserData( &gSelectedUpDownArrowOnGridScrollAreaRegion[ 1 ], 0, 1);
	MSYS_EnableRegion(&gSelectedUpDownArrowOnGridScrollAreaRegion[1]);
	if (HasScrollableBobbyOrder(guidivisor))
	{//Scroll Bar
		usPosX = usGridX + BOBBYR_GRID_SCROLL_COLUMN_X;
		usPosY = usGridY + BOBBYR_GRID_SCROLL_UP_ARROW_Y + BOBBYR_SCROLL_ARROW_HEIGHT;
		usHeight = BOBBYR_GRID_SCROLL_COLUMN_HEIGHT_MINUS_ARROWS / guidivisor;
		gSelectedGridScrollColumnRegions.resize(guidivisor);
		for(i=0; i<guidivisor; i++)
		{
				MSYS_DefineRegion( &gSelectedGridScrollColumnRegions[i], usPosX, usPosY, (UINT16)(usPosX+BOBBYR_SCROLL_ARROW_WIDTH), (UINT16)(usPosY+usHeight), MSYS_PRIORITY_HIGH,
					CURSOR_WWW, SelectGridScrollColumnMovementCallBack, SelectGridScrollColumnRegionCallBack );
				if (!stagedGridResources.addRegion(
					gSelectedGridScrollColumnRegions[i])) return;
				MSYS_EnableRegion(&gSelectedGridScrollColumnRegions[i]);
				MSYS_SetRegionUserData( &gSelectedGridScrollColumnRegions[ i ], 0, i);
				usPosY += usHeight;
		}
		//Draw the rectangle
		DrawOrderGoldRectangle( usGridX, usGridY);
	}
	gBobbyROrderGridResources = std::move(stagedGridResources);
	DisplayShippingCosts( fCalledFromOrderPage, iSubTotal, usGridX, usGridY, iOrderNum );
}


void DisplayShippingCosts( BOOLEAN fCalledFromOrderPage, INT32 iSubTotal, UINT16 usGridX, UINT16 usGridY, INT32 iOrderNum )
{
	HVOBJECT hPixHandle;
	INT32	iShippingCost = 0;
//	INT32 iTotal;

	if( fCalledFromOrderPage )
	{
		iSubTotal = guiSubTotal;
//		iTotal = giGrandTotal;

		if( gubSelectedLight == 0 )
			guiShippingCost = CalcCostFromWeightOfPackage( 0 );
		else if( gubSelectedLight == 1 )
			guiShippingCost = CalcCostFromWeightOfPackage( 1 );
		else if( gubSelectedLight == 2 )
			guiShippingCost = CalcCostFromWeightOfPackage( 2 );

		iShippingCost = guiShippingCost;
	}
	else
	{
		if (!IsValidLaptopIndex(gShipmentTable.size(), iOrderNum) ||
			!gShipmentTable[iOrderNum] ||
			!gShipmentTable[iOrderNum]->pDestinationDeliveryInfo)
		{
			return;
		}
		//Dealtar's Airport Externalization.
		/* Old code:
		UINT16	usStandardCost;

		switch( gpNewBobbyrShipments[ iOrderNum ].ubDeliveryMethod )
		{
			case 0:
				usStandardCost = BobbyROrderLocations[gpNewBobbyrShipments[ iOrderNum ].ubDeliveryLoc].usOverNightExpress;
				break;
			case 1:
				usStandardCost = BobbyROrderLocations[gpNewBobbyrShipments[ iOrderNum ].ubDeliveryLoc].us2DaysService;
				break;
			case 2:
				usStandardCost = BobbyROrderLocations[gpNewBobbyrShipments[ iOrderNum ].ubDeliveryLoc].usStandardService;
				break;

			default:
				usStandardCost = 0;
		}
		iShippingCost = (INT32)( ( gpNewBobbyrShipments[ iOrderNum ].uiPackageWeight / (FLOAT)10 ) * usStandardCost + .5 );
		* Dealtar's Code: */
		UINT32 uiPackageWeight=0;

		ShipmentPackageList::iterator spi = gShipmentTable[iOrderNum]->ShipmentPackages.begin();

		while(spi != gShipmentTable[iOrderNum]->ShipmentPackages.end())
		{
			const ShipmentPackageStruct& package = *spi;
			if (package.usItemIndex < MAXITEMS)
				uiPackageWeight += Item[package.usItemIndex].ubWeight *
					package.ubNumber;
			++spi;
		}

		uiPackageWeight = uiPackageWeight > 20 ? uiPackageWeight : 20;

		iShippingCost = (INT32)( ( uiPackageWeight / (FLOAT)10 ) * gShipmentTable[iOrderNum]->pDestinationDeliveryInfo->usDestinationFee + .5 );
		
	}


	//erase the old area
	// bli the total Saved area onto the grid
	if( fCalledFromOrderPage )
	{
		GetVideoObject(&hPixHandle, guiTotalSaveArea);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, BOBBYR_TOTAL_SAVED_AREA_X, BOBBYR_TOTAL_SAVED_AREA_Y, VO_BLT_SRCTRANSPARENCY,NULL);
	}


//if there is a shipment, display the s&h charge
	if( iSubTotal )
	{
		//Display the subtotal
		DrawTextToScreen(FormatMoney(iSubTotal).data(), (UINT16)(usGridX + BOBBYR_GRID_FIFTH_COLUMN_X - 2), (UINT16)(usGridY + BOBBYR_SUBTOTAL_Y), BOBBYR_GRID_FIFTH_COLUMN_WIDTH, BOBBYR_ORDER_DYNAMIC_TEXT_FONT, BOBBYR_ORDER_DYNAMIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);

		//Display the shipping and handling charge
		DrawTextToScreen(FormatMoney(iShippingCost).data(), (UINT16)(usGridX + BOBBYR_GRID_FIFTH_COLUMN_X - 2), (UINT16)(usGridY + BOBBYR_SHIPPING_N_HANDLE_Y), BOBBYR_GRID_FIFTH_COLUMN_WIDTH, BOBBYR_ORDER_DYNAMIC_TEXT_FONT, BOBBYR_ORDER_DYNAMIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);


		//Display the grand total
		giGrandTotal = iSubTotal + iShippingCost;
		DrawTextToScreen(FormatMoney(giGrandTotal).data(), (UINT16)(usGridX + BOBBYR_GRID_FIFTH_COLUMN_X - 2), (UINT16)(usGridY + BOBBYR_GRAND_TOTAL_Y), BOBBYR_GRID_FIFTH_COLUMN_WIDTH, BOBBYR_ORDER_DYNAMIC_TEXT_FONT, BOBBYR_ORDER_DYNAMIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	}

	InvalidateRegion(iScreenWidthOffset + 333,iScreenHeightOffset + 326,iScreenWidthOffset + 374,iScreenHeightOffset + 400);
}




void BtnBobbyRBackCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		guiCurrentLaptopMode = guiLastBobbyRayPage;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


void BtnBobbyRHomeCallback(GUI_BUTTON *btn,INT32 reason)
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

void SelectShippingSpeedRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		gubSelectedLight = (UINT8)MSYS_GetRegionUserData( pRegion, 0 );
		DrawShippingSpeedLights( gubSelectedLight );
		DisplayShippingCosts( TRUE, 0, BOBBYR_ORDERGRID_X, BOBBYR_ORDERGRID_Y, -1 );
	}
}

BOOLEAN DrawShippingSpeedLights(UINT8 ubSelected)
{

	if(ubSelected == 0)
		ColorFillVideoSurfaceArea( FRAME_BUFFER, gShippingSpeedAreas[0], gShippingSpeedAreas[1], gShippingSpeedAreas[0]+SHIPPING_SPEED_LIGHT_WIDTH,	gShippingSpeedAreas[1]+SHIPPING_SPEED_LIGHT_HEIGHT, Get16BPPColor( FROMRGB( 0, 255, 0 ) ) );
	else
		ColorFillVideoSurfaceArea( FRAME_BUFFER, gShippingSpeedAreas[0], gShippingSpeedAreas[1], gShippingSpeedAreas[0]+SHIPPING_SPEED_LIGHT_WIDTH,	gShippingSpeedAreas[1]+SHIPPING_SPEED_LIGHT_HEIGHT, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );

	if(ubSelected == 1)
		ColorFillVideoSurfaceArea( FRAME_BUFFER, gShippingSpeedAreas[2], gShippingSpeedAreas[3], gShippingSpeedAreas[2]+SHIPPING_SPEED_LIGHT_WIDTH,	gShippingSpeedAreas[3]+SHIPPING_SPEED_LIGHT_HEIGHT, Get16BPPColor( FROMRGB( 0, 255, 0 ) ) );
	else
		ColorFillVideoSurfaceArea( FRAME_BUFFER, gShippingSpeedAreas[2], gShippingSpeedAreas[3], gShippingSpeedAreas[2]+SHIPPING_SPEED_LIGHT_WIDTH,	gShippingSpeedAreas[3]+SHIPPING_SPEED_LIGHT_HEIGHT, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );

	if(ubSelected == 2)
		ColorFillVideoSurfaceArea( FRAME_BUFFER, gShippingSpeedAreas[4], gShippingSpeedAreas[5], gShippingSpeedAreas[4]+SHIPPING_SPEED_LIGHT_WIDTH,	gShippingSpeedAreas[5]+SHIPPING_SPEED_LIGHT_HEIGHT, Get16BPPColor( FROMRGB( 0, 255, 0 ) ) );
	else
		ColorFillVideoSurfaceArea( FRAME_BUFFER, gShippingSpeedAreas[4], gShippingSpeedAreas[5], gShippingSpeedAreas[4]+SHIPPING_SPEED_LIGHT_WIDTH,	gShippingSpeedAreas[5]+SHIPPING_SPEED_LIGHT_HEIGHT, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );

	InvalidateRegion(iScreenWidthOffset + 585, iScreenHeightOffset + 218, iScreenWidthOffset + 594, iScreenHeightOffset + 287);
	return(TRUE);
}


void SelectConfirmOrderRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		//Remove the items for Boby Rqys Inventory
		RemovePurchasedItemsFromBobbyRayInventory();

			//delete the order
		ClearBobbyRayPurchases();
		gubSelectedLight = 0;
		gfDestroyConfirmGrphiArea = TRUE;
		gubSelectedLight = 0;

		//Goto The homepage
		guiCurrentLaptopMode = LAPTOP_MODE_BOBBY_R;

	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
		//Remove the items for Boby Rqys Inventory
		RemovePurchasedItemsFromBobbyRayInventory();

		//delete the order
		ClearBobbyRayPurchases();
		gubSelectedLight = 0;
		gfDestroyConfirmGrphiArea = TRUE;
		gubSelectedLight = 0;

	}
}


BOOLEAN CreateDestroyBobbyRDropDown( UINT8 ubDropDownAction )
{
	switch( ubDropDownAction )
	{
		case 	BR_DROP_DOWN_NO_ACTION:
		{

		}
		break;

		case 	BR_DROP_DOWN_CREATE:
		{
			std::size_t i;
			UINT16 usPosX, usPosY, usHeight;
			UINT16 usFontHeight = GetFontHeight( BOBBYR_DROPDOWN_FONT );

			if( gfBobbyRDropDownRegionsCreated )
			{
				gubDropDownAction = BR_DROP_DOWN_DESTROY;

				break;
			}
			const std::size_t destinationCount = gDestinationTable.size();
			if (destinationCount == 0 || guiNumOfDisplayedCities == 0)
			{
				gubDropDownAction = BR_DROP_DOWN_NO_ACTION;
				break;
			}

			LaptopPageResourceOwner stagedDropDownResources;
			gSelectedDropDownRegions.resize(guiNumOfDisplayedCities);
			gSelectedScrollAreaDropDownRegions.resize(destinationCount);

			usPosX = BOBBYR_CITY_START_LOCATION_X;
			usPosY = BOBBYR_CITY_START_LOCATION_Y;
			//for( i=0; i< BOBBYR_NUM_DISPLAYED_CITIES; i++)
			for( i=0; i< guiNumOfDisplayedCities; i++)
			{
				MSYS_DefineRegion( &gSelectedDropDownRegions[i], usPosX, (UINT16)(usPosY+4), (UINT16)(usPosX+BOBBYR_DROP_DOWN_WIDTH-6), (UINT16)(usPosY+usFontHeight+7), MSYS_PRIORITY_HIGH,
										CURSOR_WWW, SelectDropDownMovementCallBack, SelectDropDownRegionCallBack);
				if (!stagedDropDownResources.addRegion(
					gSelectedDropDownRegions[i])) return FALSE;
				MSYS_SetRegionUserData( &gSelectedDropDownRegions[ i ], 0,
					static_cast<INT32>(i));

				usPosY += usFontHeight + 2;
			}
			//create the scroll bars regions
			//up arrow
			usPosX = BOBBYR_SCROLL_UP_ARROW_X;
			usPosY = BOBBYR_SCROLL_UP_ARROW_Y;
			for(i=0; i<2; i++)
			{
				MSYS_DefineRegion( &gSelectedUpDownArrowOnScrollAreaRegion[i], usPosX, usPosY, (UINT16)(usPosX+BOBBYR_SCROLL_ARROW_WIDTH), (UINT16)(usPosY+BOBBYR_SCROLL_ARROW_HEIGHT), MSYS_PRIORITY_HIGH,
										CURSOR_WWW, MSYS_NO_CALLBACK, SelectUpDownArrowOnScrollAreaRegionCallBack);
				if (!stagedDropDownResources.addRegion(
					gSelectedUpDownArrowOnScrollAreaRegion[i])) return FALSE;
				MSYS_SetRegionUserData( &gSelectedUpDownArrowOnScrollAreaRegion[ i ], 0,
					static_cast<INT32>(i));
				usPosX = BOBBYR_SCROLL_DOWN_ARROW_X;
				usPosY = BOBBYR_SCROLL_DOWN_ARROW_Y;
			}

			//the scroll area itself
			usPosX = BOBBYR_SCROLL_AREA_X;
			usPosY = BOBBYR_SCROLL_UP_ARROW_Y + BOBBYR_SCROLL_ARROW_HEIGHT;
			//Dealtar's Airport Externalization.
			/*
			usHeight = BOBBYR_SCROLL_AREA_HEIGHT_MINUS_ARROWS / BOBBYR_ORDER_NUM_SHIPPING_CITIES;
			for(i=0; i<BOBBYR_ORDER_NUM_SHIPPING_CITIES-1; i++)
			*/
			usHeight = static_cast<UINT16>(
				BOBBYR_SCROLL_AREA_HEIGHT_MINUS_ARROWS / destinationCount);
			if (usHeight == 0) usHeight = 1;
			for(i=0; i<destinationCount; i++)
			{
				const UINT16 bottom = i + 1 == destinationCount
					? BOBBYR_SCROLL_DOWN_ARROW_Y
					: static_cast<UINT16>(usPosY + usHeight);
				MSYS_DefineRegion( &gSelectedScrollAreaDropDownRegions[i], usPosX, usPosY, (UINT16)(usPosX+BOBBYR_SCROLL_ARROW_WIDTH), bottom, MSYS_PRIORITY_HIGH+1,
										CURSOR_LAPTOP_SCREEN, SelectScrollAreaDropDownMovementCallBack, SelectScrollAreaDropDownRegionCallBack);
				if (!stagedDropDownResources.addRegion(
					gSelectedScrollAreaDropDownRegions[i])) return FALSE;
				MSYS_SetRegionUserData( &gSelectedScrollAreaDropDownRegions[ i ], 0,
					static_cast<INT32>(i));
				usPosY += usHeight;
			}
			gBobbyRDropDownResources = std::move(stagedDropDownResources);
			gfBobbyRDropDownRegionsCreated = TRUE;


		MSYS_EnableRegion(&gSelectedCloseDropDownRegion);

			//disable the clear order and accept order buttons, (their rendering interferes with the drop down graphics)
		DisableButton(guiBobbyRClearOrder);

//FERAL
//			if( IsAnythingPurchasedFromBobbyRayPage() )
//			{
//				SpecifyDisabledButtonStyle( guiBobbyRAcceptOrder, DISABLED_STYLE_NONE );
//				DisableButton(guiBobbyRAcceptOrder);
//			}
		}
		break;

		case 	BR_DROP_DOWN_DESTROY:
		{
			if( !gfBobbyRDropDownRegionsCreated )
				break;
			gBobbyRDropDownResources.clear();
			gSelectedDropDownRegions.clear();
			gSelectedScrollAreaDropDownRegions.clear();

			//display the name on the title bar
			ColorFillVideoSurfaceArea( FRAME_BUFFER, BOBBYR_SHIPPING_LOC_AREA_L_X, BOBBYR_SHIPPING_LOC_AREA_T_Y, BOBBYR_SHIPPING_LOC_AREA_L_X+175,	BOBBYR_SHIPPING_LOC_AREA_T_Y+BOBBYR_DROP_DOWN_HEIGHT, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );

			/*
			if( gbSelectedCity == -1 )
				DrawTextToScreen( BobbyROrderFormText[BOBBYR_SELECT_DEST], BOBBYR_CITY_START_LOCATION_X+BOBBYR_CITY_NAME_OFFSET, BOBBYR_SHIPPING_LOC_AREA_T_Y+3, 0, BOBBYR_DROPDOWN_FONT, BOBBYR_ORDER_DROP_DOWN_SELEC_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED	);
			else			
				DrawTextToScreen( *(BobbyROrderLocations[gbSelectedCity].psCityLoc), BOBBYR_CITY_START_LOCATION_X+BOBBYR_CITY_NAME_OFFSET, BOBBYR_SHIPPING_LOC_AREA_T_Y+3, 0, BOBBYR_DROPDOWN_FONT, BOBBYR_ORDER_DROP_DOWN_SELEC_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED	);
			*/

			//disable the r\close regiuon
		MSYS_DisableRegion(&gSelectedCloseDropDownRegion);

			//enable the clear order and accept order buttons, (because their rendering interferes with the drop down graphics)
			EnableButton(guiBobbyRClearOrder);
//FERAL
//			if( IsAnythingPurchasedFromBobbyRayPage() )
//			{
//				SpecifyDisabledButtonStyle( guiBobbyRAcceptOrder, DISABLED_STYLE_SHADED );
//			EnableButton(guiBobbyRAcceptOrder);
//			}

			gfReDrawBobbyOrder = TRUE;
			gfBobbyRDropDownRegionsCreated = FALSE;
			gubDropDownAction = BR_DROP_DOWN_NO_ACTION;
		}
		break;

		case 	BR_DROP_DOWN_DISPLAY:
		{
			UINT8 i;
			UINT16 usPosY, usPosX;
			const UINT16 usHeight = BOBBYR_SCROLL_AREA_HEIGHT;
			HVOBJECT	hImageHandle;
		HVOBJECT	hArrowHandle;

			//Display the background for the drop down window
			ColorFillVideoSurfaceArea( FRAME_BUFFER, BOBBYR_CITY_START_LOCATION_X, BOBBYR_CITY_START_LOCATION_Y, BOBBYR_CITY_START_LOCATION_X+BOBBYR_DROP_DOWN_WIDTH,	BOBBYR_CITY_START_LOCATION_Y+BOBBYR_SCROLL_AREA_HEIGHT, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );

			//
			// Place the border around the background
			//
			GetVideoObject(&hImageHandle, guiDropDownBorder);

			usPosY = 0;
			//blit top & bottom row of images
			for(i=10; i< BOBBYR_DROP_DOWN_WIDTH-10; i+=10)
			{
				//TOP ROW
				BltVideoObject(FRAME_BUFFER, hImageHandle, 1,i+BOBBYR_CITY_START_LOCATION_X, usPosY+BOBBYR_CITY_START_LOCATION_Y, VO_BLT_SRCTRANSPARENCY,NULL);
				//BOTTOM ROW
				BltVideoObject(FRAME_BUFFER, hImageHandle, 6,i+BOBBYR_CITY_START_LOCATION_X, usHeight-10+6+BOBBYR_CITY_START_LOCATION_Y, VO_BLT_SRCTRANSPARENCY,NULL);
			}

			//blit the left and right row of images
			usPosX = 0;
			for(i=10; i< usHeight-10; i+=10)
			{
				BltVideoObject(FRAME_BUFFER, hImageHandle, 3,usPosX+BOBBYR_CITY_START_LOCATION_X, i+BOBBYR_CITY_START_LOCATION_Y, VO_BLT_SRCTRANSPARENCY,NULL);
				BltVideoObject(FRAME_BUFFER, hImageHandle, 4,usPosX+BOBBYR_DROP_DOWN_WIDTH-4+BOBBYR_CITY_START_LOCATION_X, i+BOBBYR_CITY_START_LOCATION_Y, VO_BLT_SRCTRANSPARENCY,NULL);
			}

			//blt the corner images for the row
			//top left
			BltVideoObject(FRAME_BUFFER, hImageHandle, 0, 0+BOBBYR_CITY_START_LOCATION_X, usPosY+BOBBYR_CITY_START_LOCATION_Y, VO_BLT_SRCTRANSPARENCY,NULL);
			//top right
			BltVideoObject(FRAME_BUFFER, hImageHandle, 2, BOBBYR_DROP_DOWN_WIDTH-10+BOBBYR_CITY_START_LOCATION_X, usPosY+BOBBYR_CITY_START_LOCATION_Y, VO_BLT_SRCTRANSPARENCY,NULL);
			//bottom left
			BltVideoObject(FRAME_BUFFER, hImageHandle, 5, 0+BOBBYR_CITY_START_LOCATION_X, usHeight-10+BOBBYR_CITY_START_LOCATION_Y, VO_BLT_SRCTRANSPARENCY,NULL);
			//bottom right
			BltVideoObject(FRAME_BUFFER, hImageHandle, 7, BOBBYR_DROP_DOWN_WIDTH-10+BOBBYR_CITY_START_LOCATION_X, usHeight-10+BOBBYR_CITY_START_LOCATION_Y, VO_BLT_SRCTRANSPARENCY,NULL);


			DrawSelectedCity( gbSelectedCity );

			//display the scroll bars regions
			ColorFillVideoSurfaceArea( FRAME_BUFFER, BOBBYR_SCROLL_AREA_X, BOBBYR_SCROLL_AREA_Y, BOBBYR_SCROLL_AREA_X+BOBBYR_SCROLL_AREA_WIDTH,	BOBBYR_SCROLL_AREA_Y+BOBBYR_SCROLL_AREA_HEIGHT, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );

			//blt right bar of scroll area
			for(i=10; i< BOBBYR_SCROLL_AREA_HEIGHT-10; i+=10)
			{
				BltVideoObject(FRAME_BUFFER, hImageHandle, 3,BOBBYR_SCROLL_AREA_X+BOBBYR_SCROLL_AREA_WIDTH-4, i+BOBBYR_CITY_START_LOCATION_Y, VO_BLT_SRCTRANSPARENCY,NULL);
			}

			//blit top row of images
			for(i=0; i< BOBBYR_SCROLL_AREA_WIDTH; i+=10)
			{
				//TOP ROW
				BltVideoObject(FRAME_BUFFER, hImageHandle, 1,i+BOBBYR_SCROLL_AREA_X-10, BOBBYR_SCROLL_AREA_Y, VO_BLT_SRCTRANSPARENCY,NULL);
				//BOTTOM ROW
				BltVideoObject(FRAME_BUFFER, hImageHandle, 6,i+BOBBYR_SCROLL_AREA_X-10, BOBBYR_SCROLL_AREA_Y-10+6+BOBBYR_SCROLL_AREA_HEIGHT, VO_BLT_SRCTRANSPARENCY,NULL);
			}

			//top right
			BltVideoObject(FRAME_BUFFER, hImageHandle, 2, BOBBYR_SCROLL_AREA_X+BOBBYR_SCROLL_AREA_WIDTH-10, BOBBYR_SCROLL_AREA_Y, VO_BLT_SRCTRANSPARENCY,NULL);
			//bottom right
			BltVideoObject(FRAME_BUFFER, hImageHandle, 7, BOBBYR_SCROLL_AREA_X+BOBBYR_SCROLL_AREA_WIDTH-10, BOBBYR_SCROLL_AREA_Y+BOBBYR_SCROLL_AREA_HEIGHT-10, VO_BLT_SRCTRANSPARENCY,NULL);

			//fix
			BltVideoObject(FRAME_BUFFER, hImageHandle, 4, BOBBYR_DROP_DOWN_WIDTH-4+BOBBYR_CITY_START_LOCATION_X, BOBBYR_CITY_START_LOCATION_Y+2, VO_BLT_SRCTRANSPARENCY,NULL);


			//get and display the up and down arrows
			GetVideoObject(&hArrowHandle, guiGoldArrowImages);
			//top arrow
			BltVideoObject(FRAME_BUFFER, hArrowHandle, 1, BOBBYR_SCROLL_UP_ARROW_X, BOBBYR_SCROLL_UP_ARROW_Y, VO_BLT_SRCTRANSPARENCY,NULL);

			//top arrow
			BltVideoObject(FRAME_BUFFER, hArrowHandle, 0, BOBBYR_SCROLL_DOWN_ARROW_X, BOBBYR_SCROLL_DOWN_ARROW_Y, VO_BLT_SRCTRANSPARENCY,NULL);

			//display the scroll rectangle
			DrawGoldRectangle( gbSelectedCity );


		InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
		}
		break;
	}

	return(TRUE);
}


void SelectDropDownRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		const INT32 selectedSlot = MSYS_GetRegionUserData(pRegion, 0);
		if (selectedSlot < 0) return;
		const std::size_t selectedIndex =
			BobbyRayFulfilmentModel::IndexForVisibleSlot(
				static_cast<std::size_t>(selectedSlot),
				gubCityAtTopOfList, guiNumOfDisplayedCities,
				gDestinationTable.size());
		if (selectedIndex == BobbyRayFulfilmentModel::NoSelection) return;
		gbSelectedCity = static_cast<INT32>(selectedIndex);

		DrawSelectedCity( gbSelectedCity );

		gubDropDownAction = BR_DROP_DOWN_DESTROY;
	}
}

void SelectActivateCityDroDownRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		gubDropDownAction = BR_DROP_DOWN_CREATE;
	}
}


void SelectDropDownMovementCallBack(MOUSE_REGION * pRegion, INT32 reason )
{
	if( reason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		pRegion->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(pRegion->RegionTopLeftX, pRegion->RegionTopLeftY, pRegion->RegionBottomRightX, pRegion->RegionBottomRightY);
	}
	else if( reason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		pRegion->uiFlags |= BUTTON_CLICKED_ON ;

		const INT32 selectedSlot = MSYS_GetRegionUserData(pRegion, 0);
		if (selectedSlot < 0) return;
		const std::size_t selectedIndex =
			BobbyRayFulfilmentModel::IndexForVisibleSlot(
				static_cast<std::size_t>(selectedSlot),
				gubCityAtTopOfList, guiNumOfDisplayedCities,
				gDestinationTable.size());
		if (selectedIndex == BobbyRayFulfilmentModel::NoSelection) return;
		gbSelectedCity = static_cast<INT32>(selectedIndex);

		gubDropDownAction = BR_DROP_DOWN_DISPLAY;

		InvalidateRegion(pRegion->RegionTopLeftX, pRegion->RegionTopLeftY, pRegion->RegionBottomRightX, pRegion->RegionBottomRightY);
	}
}



void DrawSelectedCity( INT32 selectedCity )
{
	const UINT16 fontHeight = GetFontHeight(BOBBYR_DROPDOWN_FONT);
	if (gDestinationTable.empty() || guiNumOfDisplayedCities == 0)
	{
		gbSelectedCity = -1;
		DisplayShippingLocationCity();
		return;
	}
	if (selectedCity < 0 ||
		!BobbyRayFulfilmentModel::IsIndexInRange(
			static_cast<std::size_t>(selectedCity),
			gDestinationTable.size()) ||
		!gDestinationTable[static_cast<std::size_t>(selectedCity)])
	{
		selectedCity = -1;
	}
	gbSelectedCity = selectedCity;
	KeepSelectedBobbyRayDestinationVisible();
	gubCityAtTopOfList = BobbyRayFulfilmentModel::NormalizeWindowStart(
		gubCityAtTopOfList, gDestinationTable.size(),
		guiNumOfDisplayedCities);

	UINT16 positionY = BOBBYR_CITY_START_LOCATION_Y + 5;
	for (std::size_t index = gubCityAtTopOfList;
		index < gubCityAtTopOfList + guiNumOfDisplayedCities; ++index)
	{
		if (gDestinationTable[index])
		{
			DrawTextToScreen(
				(STR16)gDestinationTable[index]->wstrName.c_str(),
				BOBBYR_CITY_START_LOCATION_X + BOBBYR_CITY_NAME_OFFSET,
				positionY, 0, BOBBYR_DROPDOWN_FONT,
				BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK,
				FALSE, LEFT_JUSTIFIED);
		}
		positionY += fontHeight + 2;
	}

	const std::size_t highlightedIndex = selectedCity >= 0
		? static_cast<std::size_t>(selectedCity) : gubCityAtTopOfList;
	positionY = static_cast<UINT16>(BOBBYR_CITY_START_LOCATION_Y +
		(fontHeight + 2) * (highlightedIndex - gubCityAtTopOfList));
	ColorFillVideoSurfaceArea(FRAME_BUFFER,
		BOBBYR_CITY_START_LOCATION_X + 4, positionY + 4,
		BOBBYR_CITY_START_LOCATION_X + BOBBYR_DROP_DOWN_WIDTH - 4,
		positionY + fontHeight + 6,
		Get16BPPColor(FROMRGB(200, 169, 87)));

	SetFontShadow(NO_SHADOW);
	if (gDestinationTable[highlightedIndex])
	{
		DrawTextToScreen(
			(STR16)gDestinationTable[highlightedIndex]->wstrName.c_str(),
			BOBBYR_CITY_START_LOCATION_X + BOBBYR_CITY_NAME_OFFSET,
			static_cast<UINT16>(positionY + 5), 0,
			BOBBYR_DROPDOWN_FONT, BOBBYR_FONT_BLACK,
			FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}
	SetFontShadow(DEFAULT_SHADOW);

	DisplayShippingLocationCity();
	if (guiBobbyRAcceptOrder != static_cast<UINT32>(-1) &&
		IsAnythingPurchasedFromBobbyRayPage() && gbSelectedCity != -1)
	{
		EnableButton(guiBobbyRAcceptOrder);
	}
}

void DisplayShippingLocationCity()
{
	std::wstring sTemp{ L"$0" };
	UINT16 usPosY;
	const bool hasSelectedCity = gbSelectedCity >= 0 &&
		BobbyRayFulfilmentModel::IsIndexInRange(
			static_cast<std::size_t>(gbSelectedCity),
			gDestinationTable.size()) &&
		gDestinationTable[gbSelectedCity];
	if (!hasSelectedCity) gbSelectedCity = -1;

	//display the name on the title bar
	ColorFillVideoSurfaceArea( FRAME_BUFFER, BOBBYR_SHIPPING_LOC_AREA_L_X, BOBBYR_SHIPPING_LOC_AREA_T_Y, BOBBYR_SHIPPING_LOC_AREA_L_X+175,	BOBBYR_SHIPPING_LOC_AREA_T_Y+BOBBYR_DROP_DOWN_HEIGHT, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );

	//if there is no city selected
	if( !hasSelectedCity )
		DrawTextToScreen( BobbyROrderFormText[BOBBYR_SELECT_DEST], BOBBYR_CITY_START_LOCATION_X+BOBBYR_CITY_NAME_OFFSET, BOBBYR_SHIPPING_LOC_AREA_T_Y+3, 0, BOBBYR_DROPDOWN_FONT, BOBBYR_ORDER_DROP_DOWN_SELEC_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED	);
	else
//		DrawTextToScreen( *(BobbyROrderLocations[gbSelectedCity].psCityLoc), BOBBYR_CITY_START_LOCATION_X+BOBBYR_CITY_NAME_OFFSET, BOBBYR_SHIPPING_LOC_AREA_T_Y+3, 0, BOBBYR_DROPDOWN_FONT, BOBBYR_ORDER_DROP_DOWN_SELEC_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED	);
		DrawTextToScreen( (STR16)gDestinationTable[gbSelectedCity]->wstrName.c_str(), BOBBYR_CITY_START_LOCATION_X+BOBBYR_CITY_NAME_OFFSET, BOBBYR_SHIPPING_LOC_AREA_T_Y+3, 0, BOBBYR_DROPDOWN_FONT, BOBBYR_ORDER_DROP_DOWN_SELEC_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED	);


	DisplayShippingCosts( TRUE, 0, BOBBYR_ORDERGRID_X, BOBBYR_ORDERGRID_Y, -1 );

	if( gubDropDownAction == BR_DROP_DOWN_DISPLAY)
		return;


	//Display the shipping cost
	usPosY = BOBBYR_OVERNIGHT_EXPRESS_Y;

	if( hasSelectedCity )
	{
//		swprintf( sTemp, L"%d", ( INT32 )(BobbyROrderLocations[gbSelectedCity].usOverNightExpress/GetWeightBasedOnMetricOption( 1 ) )	);
		sTemp = FormatMoney(static_cast<INT32>(gPostalService.GetDestinationFee(0, gDestinationTable[gbSelectedCity]->usID) / GetWeightBasedOnMetricOption(1)));
	}

	DrawTextToScreen(sTemp.data(), BOBBYR_SHIPPING_SPEED_NUMBER_X, usPosY, BOBBYR_SHIPPING_SPEED_NUMBER_WIDTH, BOBBYR_DROPDOWN_FONT, BOBBYR_ORDER_DYNAMIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY +=BOBBYR_GRID_ROW_OFFSET;

	if( hasSelectedCity )
	{
		sTemp = FormatMoney(static_cast<INT32>(gPostalService.GetDestinationFee(1, gDestinationTable[gbSelectedCity]->usID) / GetWeightBasedOnMetricOption(1)));
	}

	DrawTextToScreen(sTemp.data(), BOBBYR_SHIPPING_SPEED_NUMBER_X, usPosY, BOBBYR_SHIPPING_SPEED_NUMBER_WIDTH, BOBBYR_DROPDOWN_FONT, BOBBYR_ORDER_DYNAMIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY +=BOBBYR_GRID_ROW_OFFSET;

	if( hasSelectedCity )
	{
		sTemp = FormatMoney(static_cast<INT32>(gPostalService.GetDestinationFee(2, gDestinationTable[gbSelectedCity]->usID) / GetWeightBasedOnMetricOption(1)));
	}

	DrawTextToScreen(sTemp.data(), BOBBYR_SHIPPING_SPEED_NUMBER_X, usPosY, BOBBYR_SHIPPING_SPEED_NUMBER_WIDTH, BOBBYR_DROPDOWN_FONT, BOBBYR_ORDER_DYNAMIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
}

void SelectCloseDroDownRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		gubDropDownAction = BR_DROP_DOWN_DESTROY;
	}
}


void RemovePurchasedItemsFromBobbyRayInventory()
{
	INT16 i;

	for(i=0; i<GetConfiguredBobbyRayPurchaseLimit(); i++)
	{
		//if the item was purchased
		if( BobbyRayPurchases[ i ].ubNumberPurchased &&
			BobbyRayPurchases[i].usItemIndex < MAXITEMS)
		{
			//if the item is used
			if( BobbyRayPurchases[ i ].fUsed)
			{
				if (!BobbyRayCommerceModel::IsIndexInBoundedList(
						BobbyRayPurchases[i].usBobbyItemIndex,
						LaptopSaveInfo.usInventoryListLength[BOBBY_RAY_USED],
						MAXITEMS))
					continue;
				auto& stock = LaptopSaveInfo.BobbyRayUsedInventory[
					BobbyRayPurchases[i].usBobbyItemIndex].ubQtyOnHand;
				stock = BobbyRayCommerceModel::RemoveStock(
					stock, BobbyRayPurchases[i].ubNumberPurchased);
			}

			//else the purchase is new
			else
			{
				if (!BobbyRayCommerceModel::IsIndexInBoundedList(
						BobbyRayPurchases[i].usBobbyItemIndex,
						LaptopSaveInfo.usInventoryListLength[BOBBY_RAY_NEW],
						MAXITEMS))
					continue;
				auto& stock = LaptopSaveInfo.BobbyRayInventory[
					BobbyRayPurchases[i].usBobbyItemIndex].ubQtyOnHand;
				stock = BobbyRayCommerceModel::RemoveStock(
					stock, BobbyRayPurchases[i].ubNumberPurchased);
			}
		}
	}
	gfRemoveItemsFromStock = FALSE;
}


BOOLEAN IsAnythingPurchasedFromBobbyRayPage()
{
	UINT16 i;
	BOOLEAN	fReturnType = FALSE;

	for(i=0; i<GetConfiguredBobbyRayPurchaseLimit(); i++)
	{
		//if the item was purchased
		if( BobbyRayPurchases[ i ].ubNumberPurchased )
		{
			fReturnType = TRUE;
		}
	}
	return( fReturnType );
}

void SelectTitleLinkRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		guiCurrentLaptopMode = LAPTOP_MODE_BOBBY_R;
	}
}


void SelectScrollAreaDropDownRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (!(iReason & (MSYS_CALLBACK_REASON_LBUTTON_UP |
		MSYS_CALLBACK_REASON_LBUTTON_REPEAT))) return;
	const INT32 target = MSYS_GetRegionUserData(pRegion, 0);
	if (target < 0 || !BobbyRayFulfilmentModel::IsIndexInRange(
		static_cast<std::size_t>(target), gDestinationTable.size())) return;
	std::size_t selection = gbSelectedCity >= 0
		? static_cast<std::size_t>(gbSelectedCity)
		: BobbyRayFulfilmentModel::NoSelection;
	if (selection == BobbyRayFulfilmentModel::NoSelection ||
		static_cast<std::size_t>(target) > selection)
		selection = BobbyRayFulfilmentModel::NextSelection(
			selection, gDestinationTable.size());
	else if (static_cast<std::size_t>(target) < selection)
		selection = BobbyRayFulfilmentModel::PreviousSelection(
			selection, gDestinationTable.size());
	if (selection == BobbyRayFulfilmentModel::NoSelection) return;
	gbSelectedCity = static_cast<INT32>(selection);
	KeepSelectedBobbyRayDestinationVisible();
	pRegion->uiFlags |= BUTTON_CLICKED_ON;
	gubDropDownAction = BR_DROP_DOWN_DISPLAY;
	InvalidateRegion(pRegion->RegionTopLeftX, pRegion->RegionTopLeftY,
		pRegion->RegionBottomRightX, pRegion->RegionBottomRightY);
}


void SelectScrollAreaDropDownMovementCallBack(MOUSE_REGION * pRegion, INT32 reason )
{
	if( reason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		pRegion->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(pRegion->RegionTopLeftX, pRegion->RegionTopLeftY, pRegion->RegionBottomRightX, pRegion->RegionBottomRightY);
	}
	else if (reason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		if( gfLeftButtonState )
		{
			const INT32 target = MSYS_GetRegionUserData(pRegion, 0);
			if (target < 0 || !BobbyRayFulfilmentModel::IsIndexInRange(
				static_cast<std::size_t>(target),
				gDestinationTable.size())) return;
			pRegion->uiFlags |= BUTTON_CLICKED_ON ;
			gbSelectedCity = target;
			KeepSelectedBobbyRayDestinationVisible();
			gubDropDownAction = BR_DROP_DOWN_DISPLAY;

			InvalidateRegion(pRegion->RegionTopLeftX, pRegion->RegionTopLeftY, pRegion->RegionBottomRightX, pRegion->RegionBottomRightY);
		}
	}
}


void SelectUpDownArrowOnScrollAreaRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if( iReason & MSYS_CALLBACK_REASON_LBUTTON_UP || iReason & MSYS_CALLBACK_REASON_LBUTTON_REPEAT )
	{
		const bool down = MSYS_GetRegionUserData(pRegion, 0) != 0;
		const std::size_t selection = gbSelectedCity >= 0
			? static_cast<std::size_t>(gbSelectedCity)
			: BobbyRayFulfilmentModel::NoSelection;
		const std::size_t next = down
			? BobbyRayFulfilmentModel::NextSelection(
				selection, gDestinationTable.size())
			: BobbyRayFulfilmentModel::PreviousSelection(
				selection, gDestinationTable.size());
		gbSelectedCity = next == BobbyRayFulfilmentModel::NoSelection
			? -1 : static_cast<INT32>(next);
		KeepSelectedBobbyRayDestinationVisible();

		gubDropDownAction = BR_DROP_DOWN_DISPLAY;
	}
}


void DrawGoldRectangle( INT32 cityNumber )
{
	UINT32 uiDestPitchBYTES;
	UINT8	*pDestBuf;
	UINT16 usWidth, usTempHeight, usTempPosY, usHeight;
	UINT16 usPosX, usPosY;

	UINT16 temp;

	if (cityNumber < 0 ||
		!BobbyRayFulfilmentModel::IsIndexInRange(
			static_cast<std::size_t>(cityNumber),
			gDestinationTable.size()))
	{
		cityNumber = 0;
	}

	usTempPosY = BOBBYR_SCROLL_UP_ARROW_Y;
	usTempPosY += BOBBYR_SCROLL_ARROW_HEIGHT;
	usPosX = BOBBYR_SCROLL_AREA_X;
	usWidth = BOBBYR_SCROLL_AREA_WIDTH - 5;
	usTempHeight = ( BOBBYR_SCROLL_AREA_HEIGHT - 2 * BOBBYR_SCROLL_ARROW_HEIGHT ) - 8;

	//usHeight = usTempHeight / (BOBBYR_ORDER_NUM_SHIPPING_CITIES+1);
	usHeight = static_cast<UINT16>(usTempHeight /
		(gDestinationTable.size() + 1));
	if (usHeight == 0) usHeight = 1;

	//usPosY = usTempPosY + (UINT16)( ( ( BOBBYR_SCROLL_AREA_HEIGHT - 2 * BOBBYR_SCROLL_ARROW_HEIGHT ) / (FLOAT)(BOBBYR_ORDER_NUM_SHIPPING_CITIES +1) ) * bCityNum );
	usPosY = usTempPosY + (UINT16)( ( ( BOBBYR_SCROLL_AREA_HEIGHT - 2 * BOBBYR_SCROLL_ARROW_HEIGHT ) / (FLOAT)(gDestinationTable.size()+1) ) * cityNumber );

	temp = BOBBYR_SCROLL_AREA_Y + BOBBYR_SCROLL_AREA_HEIGHT - BOBBYR_SCROLL_ARROW_HEIGHT - usHeight - 1;

	if( usPosY >= temp )
		usPosY = BOBBYR_SCROLL_AREA_Y + BOBBYR_SCROLL_AREA_HEIGHT - BOBBYR_SCROLL_ARROW_HEIGHT - usHeight - 5;

	ColorFillVideoSurfaceArea( FRAME_BUFFER, BOBBYR_SCROLL_AREA_X, usPosY, BOBBYR_SCROLL_AREA_X+usWidth,	usPosY+usHeight, Get16BPPColor( FROMRGB( 186, 165, 68 ) ) );

	//display the line
	pDestBuf = LockVideoSurface( FRAME_BUFFER, &uiDestPitchBYTES );
	SetClippingRegionAndImageWidth( uiDestPitchBYTES, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

	// draw the gold highlite line on the top and left
	LineDraw(FALSE, usPosX, usPosY, usPosX+usWidth, usPosY, Get16BPPColor( FROMRGB( 235, 222, 171 ) ), pDestBuf);
	LineDraw(FALSE, usPosX, usPosY, usPosX, usPosY+usHeight, Get16BPPColor( FROMRGB( 235, 222, 171 ) ), pDestBuf);

	// draw the shadow line on the bottom and right
	LineDraw(FALSE, usPosX, usPosY+usHeight, usPosX+usWidth, usPosY+usHeight, Get16BPPColor( FROMRGB( 65, 49, 6 ) ), pDestBuf);
	LineDraw(FALSE, usPosX+usWidth, usPosY, usPosX+usWidth, usPosY+usHeight, Get16BPPColor( FROMRGB( 65, 49, 6 ) ), pDestBuf);

	// unlock frame buffer
	UnLockVideoSurface( FRAME_BUFFER );
}

void SelectGridScrollColumnMovementCallBack(MOUSE_REGION * pRegion, INT32 reason )
{
	if( reason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		pRegion->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(pRegion->RegionTopLeftX, pRegion->RegionTopLeftY, pRegion->RegionBottomRightX, pRegion->RegionBottomRightY);
	}
	else if (reason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		if( gfLeftButtonState )
		{
			UINT8	ubPurchaseNumber = (UINT8)MSYS_GetRegionUserData( pRegion, 0 );

			pRegion->uiFlags |= BUTTON_CLICKED_ON ;

			// scroll up
			if( ubPurchaseNumber < gubPurchaseAtTopOfList )
			{
				gubPurchaseAtTopOfList = ubPurchaseNumber;
			}

			// scroll down
			if( ubPurchaseNumber > gubPurchaseAtTopOfList )
			{
				// silversurfer: No idea what this was supposed to do. We always have only BOBBYR_NUM_DISPLAYED_ITEMS in the list.
				// Why should we handle scrolling up and down differently?
/*				if( ( ubPurchaseNumber - gubPurchaseAtTopOfList ) >= BOBBYR_NUM_DISPLAYED_ITEMS )   
					gubPurchaseAtTopOfList = ubPurchaseNumber - BOBBYR_NUM_DISPLAYED_ITEMS + 1;     */
				gubPurchaseAtTopOfList = ubPurchaseNumber;
			}
			gfReDrawBobbyOrder = TRUE;
			gfBobbyRShipmentsDirty = TRUE;
			InvalidateRegion(pRegion->RegionTopLeftX, pRegion->RegionTopLeftY, pRegion->RegionBottomRightX, pRegion->RegionBottomRightY);
		}
	}
}

void SelectGridScrollColumnRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		UINT8	ubPurchaseNumber = (UINT8)MSYS_GetRegionUserData( pRegion, 0 );

		if( ubPurchaseNumber < gubPurchaseAtTopOfList )
		{
				gubPurchaseAtTopOfList--;
		}

		if( ubPurchaseNumber > gubPurchaseAtTopOfList )
		{
				gubPurchaseAtTopOfList++;
		}

	}
	else if (iReason & MSYS_CALLBACK_REASON_LBUTTON_REPEAT)
	{
		UINT8	ubPurchaseNumber = (UINT8)MSYS_GetRegionUserData( pRegion, 0 );

		pRegion->uiFlags |= BUTTON_CLICKED_ON ;

		if( ubPurchaseNumber < gubPurchaseAtTopOfList )
		{
				gubPurchaseAtTopOfList--;
		}

		if( ubPurchaseNumber > gubPurchaseAtTopOfList )
		{
				gubPurchaseAtTopOfList++;
		}

		gfReDrawBobbyOrder = TRUE;
		gfBobbyRShipmentsDirty = TRUE;
		InvalidateRegion(pRegion->RegionTopLeftX, pRegion->RegionTopLeftY, pRegion->RegionBottomRightX, pRegion->RegionBottomRightY);
	}
}

void SelectUpDownArrowOnGridScrollAreaRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if( iReason & MSYS_CALLBACK_REASON_LBUTTON_UP || iReason & MSYS_CALLBACK_REASON_LBUTTON_REPEAT )
	{
		UINT8	ubDownArrow = (UINT8)MSYS_GetRegionUserData( pRegion, 0 );

		if( ubDownArrow )
		{
			if( gubPurchaseAtTopOfList < guidivisor-1 )
			{
				gubPurchaseAtTopOfList++;
			}
		}
		else
		{
			if( gubPurchaseAtTopOfList > 0 )
				gubPurchaseAtTopOfList--;

		}
		gfReDrawBobbyOrder = TRUE;
		gfBobbyRShipmentsDirty = TRUE;
		InvalidateRegion(pRegion->RegionTopLeftX, pRegion->RegionTopLeftY, pRegion->RegionBottomRightX, pRegion->RegionBottomRightY);
	}
}

void DrawOrderGoldRectangle (UINT16 usGridX, UINT16 usGridY)
{
	if (!HasScrollableBobbyOrder(guidivisor)) return;
	UINT32 uiDestPitchBYTES;
	UINT8	*pDestBuf;
	UINT16 usWidth, usTempHeight, usTempPosY, usHeight;
	UINT16 usPosX, usPosY;
	UINT16 temp;

	usTempPosY = usGridY + BOBBYR_GRID_SCROLL_UP_ARROW_Y + BOBBYR_SCROLL_ARROW_HEIGHT;
	usPosX = usGridX + BOBBYR_GRID_SCROLL_COLUMN_X + 2;
	usWidth = BOBBYR_GRID_SCROLL_COLUMN_WIDTH - 5;
	usTempHeight = ( BOBBYR_GRID_SCROLL_COLUMN_HEIGHT - 2 * BOBBYR_SCROLL_ARROW_HEIGHT );

	usHeight = usTempHeight / guidivisor;

	usPosY = usTempPosY + (UINT16)( ( ( BOBBYR_GRID_SCROLL_COLUMN_HEIGHT - 2 * BOBBYR_SCROLL_ARROW_HEIGHT ) /	(FLOAT)(guidivisor) ) * gubPurchaseAtTopOfList );

	temp = usGridY + BOBBYR_GRID_SCROLL_COLUMN_Y + BOBBYR_GRID_SCROLL_COLUMN_HEIGHT - BOBBYR_SCROLL_ARROW_HEIGHT - usHeight - 1;

	if( usPosY >= temp )
		usPosY = usGridY + BOBBYR_GRID_SCROLL_COLUMN_Y + BOBBYR_GRID_SCROLL_COLUMN_HEIGHT - BOBBYR_SCROLL_ARROW_HEIGHT - usHeight - 5;

	ColorFillVideoSurfaceArea( FRAME_BUFFER, usPosX, usPosY, usPosX+usWidth, usPosY+usHeight, Get16BPPColor( FROMRGB( 186, 165, 68 ) ) );

	//display the line
	pDestBuf = LockVideoSurface( FRAME_BUFFER, &uiDestPitchBYTES );
	SetClippingRegionAndImageWidth( uiDestPitchBYTES, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	
	// draw the shadow line around the rectangle
	LineDraw(FALSE, usPosX, usPosY, usPosX+usWidth, usPosY, Get16BPPColor( FROMRGB( 65, 49, 6) ), pDestBuf);
	LineDraw(FALSE, usPosX, usPosY, usPosX, usPosY+usHeight, Get16BPPColor( FROMRGB( 65, 49, 6 ) ), pDestBuf);
	LineDraw(FALSE, usPosX, usPosY+usHeight, usPosX+usWidth, usPosY+usHeight, Get16BPPColor( FROMRGB( 65, 49, 6 ) ), pDestBuf);
	LineDraw(FALSE, usPosX+usWidth, usPosY, usPosX+usWidth, usPosY+usHeight, Get16BPPColor( FROMRGB( 65, 49, 6 ) ), pDestBuf);

	// unlock frame buffer
	UnLockVideoSurface( FRAME_BUFFER );
}

UINT32	CalcCostFromWeightOfPackage( UINT8	ubTypeOfService )
{
	UINT32	uiTotalWeight = 0;
	UINT16	usStandardCost=0;
	UINT32	uiTotalCost=0;

	if (ubTypeOfService >= 3 || gbSelectedCity < 0 ||
		!BobbyRayFulfilmentModel::IsIndexInRange(
			static_cast<std::size_t>(gbSelectedCity),
			gDestinationTable.size()) ||
		!gDestinationTable[gbSelectedCity])
	{
		// shipping rates unknown until destination selected
		return(0);
	}

	//Get the package's weight
	uiTotalWeight = CalcPackageTotalWeight();

	//Dealtar's Airport Externalization
	/*
	switch( ubTypeOfService )
	{
		case 0:
			usStandardCost = BobbyROrderLocations[gbSelectedCity].usOverNightExpress;
			break;
		case 1:
			usStandardCost = BobbyROrderLocations[gbSelectedCity].us2DaysService;
			break;
		case 2:
			usStandardCost = BobbyROrderLocations[gbSelectedCity].usStandardService;
			break;

		default:
			usStandardCost = 0;
	}
	*/
	usStandardCost = gPostalService.GetDestinationFee(ubTypeOfService, gDestinationTable[gbSelectedCity]->usID);

	//Get the actual weight ( either in lbs or metric )
///	usStandardCost = (UINT16) GetWeightBasedOnMetricOption( usStandardCost );

	//if the total weight is below a set minimum amount ( 2 kg )
	if( uiTotalWeight < MIN_SHIPPING_WEIGHT )
	{
		//bring up the base cost
		uiTotalWeight = MIN_SHIPPING_WEIGHT;
	}

	uiTotalCost = (UINT32)( ( uiTotalWeight / (FLOAT)10 ) * usStandardCost + .5 );

	return( uiTotalCost );
}

void BobbyRayMailOrderEndGameShutDown()
{
	ShutDownBobbyRNewMailOrders();
/*
	if( LaptopSaveInfo.BobbyRayOrdersOnDeliveryArray )
	{
		MemFree( LaptopSaveInfo.BobbyRayOrdersOnDeliveryArray );
		LaptopSaveInfo.BobbyRayOrdersOnDeliveryArray = NULL;
	}
*/

}

void ShutDownBobbyRNewMailOrders()
{
	if( gpNewBobbyrShipments != NULL )
	{
		MemFree( gpNewBobbyrShipments );
		gpNewBobbyrShipments = NULL;
	}
	giNumberOfNewBobbyRShipment = 0;
}

BOOLEAN ImportOldBobbyRayOrders(
	const BobbyRayOrderStruct *orders, UINT8 slotCount)
{
	if (!orders && slotCount != 0)
		return FALSE;

	const std::size_t activeUpperBound =
		BobbyRayCommerceModel::CountActiveOrders(orders, slotCount);
	if (activeUpperBound == 0)
	{
		ShutDownBobbyRNewMailOrders();
		return TRUE;
	}

	const std::size_t allocationSize =
		activeUpperBound * sizeof(NewBobbyRayOrderStruct);
	std::unique_ptr<NewBobbyRayOrderStruct[],
		NewBobbyRayOrderAllocationDeleter> imported(
			static_cast<NewBobbyRayOrderStruct*>(MemAlloc(allocationSize)));
	if (!imported)
		return FALSE;
	memset(imported.get(), 0, allocationSize);

	INT32 writeIndex = 0;
	for (UINT8 readIndex = 0; readIndex < slotCount; ++readIndex)
	{
		if (!orders[readIndex].fActive)
			continue;

		NewBobbyRayOrderStruct& destination = imported[writeIndex];
		destination.fActive = TRUE;
		destination.ubDeliveryLoc = BR_DRASSEN;
		destination.ubDeliveryMethod = 0;
		destination.uiPackageWeight = 1;
		destination.uiOrderedOnDayNum = GetWorldDay();
		destination.fDisplayedInShipmentPage = TRUE;
		const UINT8 purchaseCount =
			BobbyRayCommerceModel::PurchaseRecordCount(
				orders[readIndex].ubNumberPurchases);
		for (UINT8 purchaseIndex = 0;
			purchaseIndex < purchaseCount; ++purchaseIndex)
		{
			destination.BobbyRayPurchase[purchaseIndex] =
				orders[readIndex].BobbyRayPurchase[purchaseIndex];
		}
		destination.ubNumberPurchases = purchaseCount;
		if (!NormalizeNewBobbyRayOrder(destination))
			continue;
		++writeIndex;
	}

	ShutDownBobbyRNewMailOrders();
	if (writeIndex == 0)
		return TRUE;
	gpNewBobbyrShipments = imported.release();
	giNumberOfNewBobbyRShipment = writeIndex;
	return TRUE;
}

INT8 CalculateOrderDelay( UINT8 ubSelectedService )
{
	INT8 bDaysAhead;

	//get the length of time to receive the shipment
	if( ubSelectedService == 0 )
	{
		bDaysAhead = OVERNIGHT_EXPRESS;
	}
	else if( ubSelectedService == 1 )
	{
		bDaysAhead = TWO_BUSINESS_DAYS;
	}
	else if( ubSelectedService == 2 )
	{
		bDaysAhead = STANDARD_SERVICE;
	}
	else
	{
		bDaysAhead = 0;
	}

	if ( gMercProfiles[ SAL ].bLife == 0 )
	{
		// Sal is dead, so Pablo is dead, so the airport is badly run
		// CJC comment: this seems really extreme!! maybe delay by 1 day randomly but that's it!
		bDaysAhead += (UINT8) Random( 5 ) + 1;
	}

	return( bDaysAhead );
}

void PurchaseBobbyOrder()
{
	if (!IsValidLaptopIndex(gDestinationTable.size(), gbSelectedCity) ||
		!gDestinationTable[gbSelectedCity])
	{
		Assert(0);
		return;
	}

	if (!AddNewBobbyRShipment(BobbyRayPurchases,
			gDestinationTable[gbSelectedCity]->usID, gubSelectedLight, TRUE,
			CalcPackageTotalWeight()))
	{
		Assert(0);
		return;
	}
	gfCanAcceptOrder = FALSE;

	//Add the transaction to the finance page
	AddTransactionToPlayersBook(BOBBYR_PURCHASE, 0, GetWorldTotalMin(), -giGrandTotal);

	//display the confirm order graphic
	gfDrawConfirmOrderGrpahic = TRUE;

	//Get rid of the city drop dowm, if it is being displayed
	gubDropDownAction = BR_DROP_DOWN_DESTROY;

	MSYS_EnableRegion(&gSelectedConfirmOrderRegion);
	gfRemoveItemsFromStock = TRUE;

	gbSelectedCity = -1;
}

void AddJohnsGunShipment()
{
	BobbyRayPurchaseStruct Temp[
		BobbyRayCommerceModel::PurchaseCapacity];
	memset(Temp, 0, sizeof(Temp));

	// Add two Automags and two packages of compatible ammunition.
	Temp[0].usItemIndex = AUTOMAG_III;
	Temp[0].ubNumberPurchased = 2;
	Temp[0].bItemQuality = 100;
	Temp[0].fUsed = FALSE;

	if (getVFS()->getProfileStack()->getProfile(L"v1.13") != NULL)
		Temp[1].usItemIndex = 557;
	else
		Temp[1].usItemIndex = CLIP762N_5_AP;
	Temp[1].ubNumberPurchased = 2;
	Temp[1].bItemQuality = 5;
	Temp[1].fUsed = FALSE;

	if (!AddNewBobbyRShipment(Temp, 3, 2, FALSE, 1))
		Assert(0);
}


void ConfirmBobbyRPurchaseMessageBoxCallBack( UINT8 bExitValue )
{
	// yes, load the game
	if( bExitValue == MSG_BOX_RETURN_YES )
	{
		PurchaseBobbyOrder();
	}
}

void EnterInitBobbyRayOrder()
{
	ClearBobbyRayPurchases();
	
	if (is_networked)
		gubSelectedLight = 2; //hayden
	else
		gubSelectedLight = 0;

	gfReDrawBobbyOrder = TRUE;

	if (is_networked)
		gbSelectedCity = 2; //hayden , was -1
	else
		gbSelectedCity = -1;

	gubCityAtTopOfList = 0;

	gubPurchaseAtTopOfList = 0;
	//Get rid of the city drop dowm, if it is being displayed
	gubDropDownAction = BR_DROP_DOWN_DESTROY;

	//disable the accept order button
	DisableButton( guiBobbyRAcceptOrder );
}


UINT32	CalcPackageTotalWeight()
{
	UINT16	i;
	UINT32	uiTotalWeight=0;

	//loop through all the packages
	for(i=0; i<GetConfiguredBobbyRayPurchaseLimit(); i++)
	{
		//if the item was purchased
		if( BobbyRayPurchases[ i ].ubNumberPurchased &&
			BobbyRayPurchases[i].usItemIndex < MAXITEMS)
		{
			//add the current weight to the total
			uiTotalWeight += Item[ BobbyRayPurchases[ i ].usItemIndex ].ubWeight * BobbyRayPurchases[ i ].ubNumberPurchased;
		}
	}

	return( uiTotalWeight );
}


void DisplayPackageWeight( )
{
	CHAR16		zTemp[32];
	UINT32	uiTotalWeight = CalcPackageTotalWeight();
//	FLOAT			fWeight = (FLOAT)(uiTotalWeight / 10.0);

	//Display the 'Package Weight' text
	DrawTextToScreen(BobbyROrderFormText[ BOBBYR_PACKAGE_WEIGHT ], BOBBYR_PACKAXGE_WEIGHT_X+8, BOBBYR_PACKAXGE_WEIGHT_Y+4, BOBBYR_PACKAXGE_WEIGHT_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	//Display the weight
//	swprintf( zTemp, L"%3.1f %s", fWeight, pMessageStrings[ MSG_KILOGRAM_ABBREVIATION ] );
	swprintf( zTemp, L"%3.1f %s", ( GetWeightBasedOnMetricOption( uiTotalWeight ) / 10.0f ), GetWeightUnitString() );
	DrawTextToScreen( zTemp, BOBBYR_PACKAXGE_WEIGHT_X+3, BOBBYR_PACKAXGE_WEIGHT_Y+4, BOBBYR_PACKAXGE_WEIGHT_WIDTH, BOBBYR_ORDER_STATIC_TEXT_FONT, BOBBYR_ORDER_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
}



void BtnBobbyRGotoShipmentPageCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		guiCurrentLaptopMode	= LAPTOP_MODE_BOBBYR_SHIPMENTS;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


BOOLEAN CreateBobbyRayOrderTitle(LaptopPageResourceOwner& owner)
{
	VOBJECT_DESC	VObjectDesc;

		// load BobbyRayTitle graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	GetMLGFilename( VObjectDesc.ImageFile,  MLG_BOBBYRAYTITLE );
	if (!owner.addVideoObject(&VObjectDesc, guiBobbyRayTitle)) return FALSE;

	//the link to home page from the title
	MSYS_DefineRegion( &gSelectedTitleLinkRegion, BOBBYR_BOBBY_RAY_TITLE_X, BOBBYR_BOBBY_RAY_TITLE_Y, (BOBBYR_BOBBY_RAY_TITLE_X + BOBBYR_BOBBY_RAY_TITLE_WIDTH), (UINT16)(BOBBYR_BOBBY_RAY_TITLE_Y + BOBBYR_BOBBY_RAY_TITLE_HEIGHT), MSYS_PRIORITY_HIGH,
							CURSOR_WWW, MSYS_NO_CALLBACK, SelectTitleLinkRegionCallBack);
	if (!owner.addRegion(gSelectedTitleLinkRegion)) return FALSE;

	return( TRUE );
}

void DrawBobbyROrderTitle()
{
	HVOBJECT hPixHandle;

	// Bobbyray title
	GetVideoObject(&hPixHandle, guiBobbyRayTitle);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, BOBBYR_BOBBY_RAY_TITLE_X, BOBBYR_BOBBY_RAY_TITLE_Y, VO_BLT_SRCTRANSPARENCY,NULL);
}


BOOLEAN AddNewBobbyRShipment( BobbyRayPurchaseStruct *pPurchaseStruct, UINT16 usDeliveryLoc, UINT8 ubDeliveryMethod, BOOLEAN fPurchasedFromBobbyR, UINT32 uiPackageWeight )
{
	if (!pPurchaseStruct)
		return FALSE;
	BOOLEAN hasPackage = FALSE;
	for (std::size_t index = 0;
		index < BobbyRayCommerceModel::PurchaseCapacity; ++index)
	{
		const BobbyRayPurchaseStruct& purchase = pPurchaseStruct[index];
		if (!purchase.ubNumberPurchased)
			continue;
		if (purchase.usItemIndex == 0 || purchase.usItemIndex >= MAXITEMS)
			return FALSE;
		hasPackage = TRUE;
	}
	if (!hasPackage)
		return FALSE;

	// Shipment from Bobby Ray
	UINT16 usID;
	if(fPurchasedFromBobbyR)
		usID = gPostalService.CreateNewShipment(usDeliveryLoc, ubDeliveryMethod, BOBBYR_SENDER_ID);
	// Shipment from John Kulba (Automag 2)
	else
		usID = gPostalService.CreateNewShipment(usDeliveryLoc, 2, JOHN_KULBA_SENDER_ID);

	if (usID == 0)
		return FALSE;
	for (std::size_t index = 0;
		index < BobbyRayCommerceModel::PurchaseCapacity; ++index)
	{
		const BobbyRayPurchaseStruct& purchase = pPurchaseStruct[index];
		if (!purchase.ubNumberPurchased)
			continue;
		if (!gPostalService.AddPackageToShipment(usID, purchase.usItemIndex,
			purchase.ubNumberPurchased, purchase.bItemQuality))
			return FALSE;
	}

	return gPostalService.SendShipment(usID);
}


UINT16	CountNumberOfBobbyPurchasesThatAreInTransit()
{
	UINT16	usItemCount=0;
	INT32		iCnt;

	if (!gpNewBobbyrShipments || giNumberOfNewBobbyRShipment <= 0)
		return 0;
	for( iCnt=0; iCnt<giNumberOfNewBobbyRShipment; iCnt++ )
	{
		if( gpNewBobbyrShipments[iCnt].fActive )
		{
			usItemCount++;
		}
	}

	return( usItemCount );
}

void SortBobbyRayPurchases()
{
	INT16 ubLastEmptyIndex = -1;

	for (UINT8 i = 0; i < GetConfiguredBobbyRayPurchaseLimit(); ++i)
	{
		// we have no item here
		if ( BobbyRayPurchases[i].ubNumberPurchased == 0 )
		{
			// store this index as the first last empty index
			if ( ubLastEmptyIndex < 0 )
				ubLastEmptyIndex = i;
		}
		else
		{
			if ( ubLastEmptyIndex >= 0 )
			{
				// move data to last empty index
				BobbyRayPurchases[ubLastEmptyIndex] = BobbyRayPurchases[i];
				// clean up data
				BobbyRayPurchases[i].bItemQuality = 0;
				BobbyRayPurchases[i].fUsed = 0;
				BobbyRayPurchases[i].ubNumberPurchased = 0;
				BobbyRayPurchases[i].usBobbyItemIndex = 0;
				BobbyRayPurchases[i].usItemIndex = 0;
				// set new last empty index
				ubLastEmptyIndex++;
			}
		}
	}
}

BOOLEAN NewWayOfSavingBobbyRMailOrdersToSaveGameFile( HWFILE hFile )
{
	INT32		iCnt;
	UINT32	uiNumBytesWritten;
	if (giNumberOfNewBobbyRShipment < 0 ||
		static_cast<std::size_t>(giNumberOfNewBobbyRShipment) >
			BobbyRayCommerceModel::MaximumLegacyShipmentSlots ||
		(giNumberOfNewBobbyRShipment != 0 && !gpNewBobbyrShipments))
	{
		return FALSE;
	}

	//Write the number of orders
	FileWrite( hFile, &giNumberOfNewBobbyRShipment, sizeof( INT32 ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( INT32 ) )
	{
		return( FALSE );
	}

	//loop through and save all the mail order slots
	for( iCnt=0; iCnt<giNumberOfNewBobbyRShipment; iCnt++ )
	{
		NewBobbyRayOrderStruct order = gpNewBobbyrShipments[iCnt];
		if (!NormalizeNewBobbyRayOrder(order))
			return FALSE;
		//Write the order
		FileWrite(hFile, &order, sizeof(NewBobbyRayOrderStruct),
			&uiNumBytesWritten);
		if( uiNumBytesWritten != sizeof( NewBobbyRayOrderStruct ) )
		{
			return( FALSE );
		}
	}

	return( TRUE );
}



BOOLEAN NewWayOfLoadingBobbyRMailOrdersToSaveGameFile( HWFILE hFile )
{
	INT32		slotCount = 0;
	UINT32	uiNumBytesRead;

	//Read the number of orders
	FileRead(hFile, &slotCount, sizeof(INT32), &uiNumBytesRead);
	if( uiNumBytesRead != sizeof( INT32 ) )
	{
		return( FALSE );
	}
	const INT32 filePosition = FileGetPos(hFile);
	const UINT32 fileSize = FileGetSize(hFile);
	if (filePosition < 0 || static_cast<UINT32>(filePosition) > fileSize ||
		!BobbyRayCommerceModel::LegacyShipmentCountFits(
			slotCount, fileSize - static_cast<UINT32>(filePosition),
			sizeof(NewBobbyRayOrderStruct)))
	{
		return FALSE;
	}

	if (slotCount == 0)
	{
		ShutDownBobbyRNewMailOrders();
		return TRUE;
	}

	const std::size_t allocationSize =
		static_cast<std::size_t>(slotCount) * sizeof(NewBobbyRayOrderStruct);
	std::unique_ptr<NewBobbyRayOrderStruct[],
		NewBobbyRayOrderAllocationDeleter> loaded(
			static_cast<NewBobbyRayOrderStruct*>(MemAlloc(allocationSize)));
	if (!loaded)
	{
		return FALSE;
	}
	memset(loaded.get(), 0, allocationSize);

	for (INT32 index = 0; index < slotCount; ++index)
	{
		FileRead(hFile, &loaded[index], sizeof(NewBobbyRayOrderStruct),
			&uiNumBytesRead);
		if (uiNumBytesRead != sizeof(NewBobbyRayOrderStruct) ||
			!NormalizeNewBobbyRayOrder(loaded[index]))
		{
			return FALSE;
		}
	}

	ShutDownBobbyRNewMailOrders();
	gpNewBobbyrShipments = loaded.release();
	giNumberOfNewBobbyRShipment = slotCount;
	return( TRUE );
}

void HandleBobbyRMailOrderKeyBoardInput()
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
					guiCurrentLaptopMode = guiLastBobbyRayPage;
				break;
				case '1':
					gubSelectedLight = 0;
					DrawShippingSpeedLights( gubSelectedLight );
					DisplayShippingCosts( TRUE, 0, BOBBYR_ORDERGRID_X, BOBBYR_ORDERGRID_Y, -1 );
				break;
				case '2':
					gubSelectedLight = 1;
					DrawShippingSpeedLights( gubSelectedLight );
					DisplayShippingCosts( TRUE, 0, BOBBYR_ORDERGRID_X, BOBBYR_ORDERGRID_Y, -1 );
				break;
				case '3':
					gubSelectedLight = 2;
					DrawShippingSpeedLights( gubSelectedLight );
					DisplayShippingCosts( TRUE, 0, BOBBYR_ORDERGRID_X, BOBBYR_ORDERGRID_Y, -1 );
				break;
				default:
					HandleKeyBoardShortCutsForLapTop( InputEvent.usEvent, InputEvent.usParam, InputEvent.usKeyState );
				break;
			}
		}
	}
}
