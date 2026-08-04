	#include "laptop.h"
	#include "BobbyRGuns.h"
	#include "BobbyR.h"
	#include "BobbyRayCatalogueModel.h"
	#include "LaptopPageResourceOwner.h"
	#include "Utilities.h"
	#include "WCheck.h"
	#include "WordWrap.h"
	#include "Cursors.h"
	#include "Interface Items.h"
	#include "Encrypted File.h"
	#include "Text.h"
	#include "Store Inventory.h"
	#include "LaptopSave.h"
	#include "finances.h"
	#include "Overhead.h"
	#include "SoldierRepository.h"
	#include "Weapons.h"
	#include "GameSettings.h"
	#include "Points.h"
	#include "Multi Language Graphic Utils.h"
	#include "english.h"
	// HEADROCK HAM 4
	#include "input.h"
	#include "Encyclopedia_new.h"	//update encyclopedia item visibility when viewing that item
	#include <language.hpp>
	#include <algorithm>
	#include <utility>


#define		BOBBYR_DEFAULT_MENU_COLOR					255


#define		BOBBYR_GRID_PIC_WIDTH							118
#define		BOBBYR_GRID_PIC_HEIGHT						69

#define		BOBBYR_GRID_PIC_X									BOBBYR_GRIDLOC_X + 3
#define		BOBBYR_GRID_PIC_Y									BOBBYR_GRIDLOC_Y + 3

#define		BOBBYR_GRID_OFFSET								72

#define		BOBBYR_ORDER_TITLE_FONT						FONT14ARIAL
#define		BOBBYR_ORDER_TEXT_FONT						FONT10ARIAL
#define		BOBBYR_ORDER_TEXT_COLOR						75

#define		BOBBYR_STATIC_TEXT_COLOR					75
#define		BOBBYR_ITEM_DESC_TEXT_FONT					FONT10ARIAL
#define		BOBBYR_ITEM_DESC_TEXT_COLOR					FONT_MCOLOR_WHITE
#define		BOBBYR_ITEM_DESC_TEXT_COLOR_ALT				FONT_GRAY1
#define		BOBBYR_ITEM_NAME_TEXT_FONT					FONT10ARIAL
#define		BOBBYR_ITEM_NAME_TEXT_COLOR					FONT_MCOLOR_WHITE

#define		NUM_BOBBYRPAGE_MENU								6
#define		NUM_CATALOGUE_BUTTONS							5
#define		BOBBYR_NUM_WEAPONS_ON_PAGE				4

#define		BOBBYR_BRTITLE_X									LAPTOP_SCREEN_UL_X + 4
#define		BOBBYR_BRTITLE_Y									LAPTOP_SCREEN_WEB_UL_Y + 3
#define		BOBBYR_BRTITLE_WIDTH							46
#define		BOBBYR_BRTITLE_HEIGHT							42

#define		BOBBYR_TO_ORDER_TITLE_X						iScreenWidthOffset + 195
#define		BOBBYR_TO_ORDER_TITLE_Y						iScreenHeightOffset + 42 + LAPTOP_SCREEN_WEB_DELTA_Y

#define		BOBBYR_TO_ORDER_TEXT_X						BOBBYR_TO_ORDER_TITLE_X + 75
#define		BOBBYR_TO_ORDER_TEXT_Y						iScreenHeightOffset + 33 + LAPTOP_SCREEN_WEB_DELTA_Y
#define		BOBBYR_TO_ORDER_TEXT_WIDTH				330

#define		BOBBYR_PREVIOUS_BUTTON_X					LAPTOP_SCREEN_UL_X + 5			//BOBBYR_HOME_BUTTON_X + BOBBYR_CATALOGUE_BUTTON_WIDTH + 5 
#define		BOBBYR_PREVIOUS_BUTTON_Y					LAPTOP_SCREEN_WEB_UL_Y + 300	//LAPTOP_SCREEN_WEB_UL_Y + 340		//BOBBYR_HOME_BUTTON_Y 

#define		BOBBYR_NEXT_BUTTON_X							LAPTOP_SCREEN_UL_X + 412		//BOBBYR_ORDER_FORM_X + BOBBYR_ORDER_FORM_WIDTH + 5 
#define		BOBBYR_NEXT_BUTTON_Y							BOBBYR_PREVIOUS_BUTTON_Y		//BOBBYR_PREVIOUS_BUTTON_Y 

#define		BOBBYR_CATALOGUE_BUTTON_START_X		BOBBYR_PREVIOUS_BUTTON_X + 92 	//LAPTOP_SCREEN_UL_X + 93 - BOBBYR_CATALOGUE_BUTTON_WIDTH/2
#define		BOBBYR_CATALOGUE_BUTTON_GAP				( 318 - NUM_CATALOGUE_BUTTONS * BOBBYR_CATALOGUE_BUTTON_WIDTH) / (NUM_CATALOGUE_BUTTONS + 1) + BOBBYR_CATALOGUE_BUTTON_WIDTH + 1//80
#define		BOBBYR_CATALOGUE_BUTTON_Y					LAPTOP_SCREEN_WEB_UL_Y + 300
#define		BOBBYR_CATALOGUE_BUTTON_WIDTH			56//75

#define	BOBBYR_HOME_BUTTON_X							iScreenWidthOffset + 120
#define	BOBBYR_HOME_BUTTON_Y							iScreenHeightOffset + 400 + LAPTOP_SCREEN_WEB_DELTA_Y

#define		BOBBYR_CATALOGUE_BUTTON_TEXT_Y		BOBBYR_CATALOGUE_BUTTON_Y + 5

#define		BOBBYR_PREVIOUS_PAGE							0
#define		BOBBYR_NEXT_PAGE									1

#define		BOBBYR_ITEM_DESC_START_X					BOBBYR_GRIDLOC_X + 172 + 5
#define		BOBBYR_ITEM_DESC_START_Y					BOBBYR_GRIDLOC_Y + 6
#define		BOBBYR_ITEM_DESC_START_WIDTH			214 - 10 + 20

#define		BOBBYR_ITEM_NAME_X								BOBBYR_GRIDLOC_X + 6
#define		BOBBYR_ITEM_NAME_Y_OFFSET					54

#define		BOBBYR_ORDER_NUM_WIDTH						15
#define		BOBBYR_ORDER_NUM_X								BOBBYR_GRIDLOC_X + 120 - BOBBYR_ORDER_NUM_WIDTH	//BOBBYR_ITEM_STOCK_TEXT_X
#define		BOBBYR_ORDER_NUM_Y_OFFSET					1

#define		BOBBYR_ITEM_WEIGHT_TEXT_X					BOBBYR_GRIDLOC_X + 409 + 3
#define		BOBBYR_ITEM_WEIGHT_TEXT_Y					3

#define		BOBBYR_ITEM_WEIGHT_NUM_X					BOBBYR_GRIDLOC_X + 429 - 2
#define		BOBBYR_ITEM_WEIGHT_NUM_Y					3
#define		BOBBYR_ITEM_WEIGHT_NUM_WIDTH			60

#define		BOBBYR_ITEM_SPEC_GAP							2

#define		BOBBYR_ITEM_COST_TEXT_X						BOBBYR_GRIDLOC_X + 125
#define		BOBBYR_ITEM_COST_TEXT_Y						BOBBYR_GRIDLOC_Y + 6
#define		BOBBYR_ITEM_COST_TEXT_WIDTH				42

#define		BOBBYR_ITEM_COST_NUM_X						BOBBYR_ITEM_COST_TEXT_X
#define		BOBBYR_ITEM_COST_NUM_Y						BOBBYR_ITEM_COST_TEXT_Y + 10

#define		BOBBYR_ITEM_STOCK_TEXT_X					BOBBYR_ITEM_COST_TEXT_X

#define		BOBBYR_ITEM_QTY_TEXT_X						BOBBYR_GRIDLOC_X + 5//BOBBYR_ITEM_COST_TEXT_X
#define		BOBBYR_ITEM_QTY_TEXT_Y						BOBBYR_ITEM_COST_TEXT_Y + 28
#define		BOBBYR_ITEM_QTY_WIDTH							95

#define		BOBBYR_ITEM_QTY_NUM_X							BOBBYR_GRIDLOC_X + 105//BOBBYR_ITEM_COST_TEXT_X + 1
#define		BOBBYR_ITEM_QTY_NUM_Y							BOBBYR_ITEM_QTY_TEXT_Y//BOBBYR_ITEM_COST_TEXT_Y + 40

#define		BOBBYR_ITEMS_BOUGHT_X							BOBBYR_GRIDLOC_X + 105 - BOBBYR_ORDER_NUM_WIDTH//BOBBYR_ITEM_QTY_NUM_X

#define		BOBBY_RAY_NOT_PURCHASED						255
#define		BOBBY_RAY_MAX_AMOUNT_OF_ITEMS_TO_PURCHASE		200

#define		BOBBYR_ORDER_FORM_X								LAPTOP_SCREEN_UL_X + 200//204
#define		BOBBYR_ORDER_FORM_Y								LAPTOP_SCREEN_WEB_UL_Y + 367
#define		BOBBYR_ORDER_FORM_WIDTH						95

#define		BOBBYR_ORDER_SUBTOTAL_X						iScreenWidthOffset + 470//490
#define		BOBBYR_ORDER_SUBTOTAL_Y						BOBBYR_ORDER_FORM_Y+2//BOBBYR_HOME_BUTTON_Y

#define		BOBBYR_ORDER_PAGE_X						iScreenWidthOffset + 285
#define		BOBBYR_ORDER_PAGE_Y						BOBBYR_ORDER_FORM_Y+2//BOBBYR_HOME_BUTTON_Y

#define		BOBBYR_PERCENT_FUNTCIONAL_X				BOBBYR_ORDER_SUBTOTAL_X
#define		BOBBYR_PERCENT_FUNTCIONAL_Y				BOBBYR_ORDER_SUBTOTAL_Y + 15


BobbyRayPurchaseStruct BobbyRayPurchases[
	BobbyRayCommerceModel::PurchaseCapacity ];

static UINT8 GetConfiguredBobbyRayPurchaseLimit()
{
	return static_cast<UINT8>(BobbyRayCommerceModel::PurchaseLimit(
		gGameExternalOptions.ubBobbyRayMaxPurchaseAmount));
}

//BobbyRayOrderStruct *BobbyRayOrdersOnDeliveryArray=NULL;
//UINT8	usNumberOfBobbyRayOrderItems = 0;
//UINT8	usNumberOfBobbyRayOrderUsed = 0;

#define		FILTER_BUTTONS_GUN_START_X				BOBBYR_PREVIOUS_BUTTON_X
#define		FILTER_BUTTONS_AMMO_START_X				FILTER_BUTTONS_GUN_START_X
#define		FILTER_BUTTONS_USED_START_X				FILTER_BUTTONS_GUN_START_X	//FILTER_BUTTONS_GUN_START_X + 122
#define		FILTER_BUTTONS_ARMOUR_START_X			FILTER_BUTTONS_GUN_START_X
#define		FILTER_BUTTONS_MISC_START_X				FILTER_BUTTONS_GUN_START_X
#define		FILTER_BUTTONS_Y_OFFSET					22 //Madd: added to standardize this value, and decreased slightly from 25 to make more room
#define		FILTER_BUTTONS_Y						BOBBYR_PREVIOUS_BUTTON_Y + FILTER_BUTTONS_Y_OFFSET 

// The number of filter buttons which category uses
#define		NUMBER_GUNS_FILTER_BUTTONS			9
#define		NUMBER_AMMO_FILTER_BUTTONS			8
#define		NUMBER_ARMOUR_FILTER_BUTTONS		4
#define		NUMBER_MISC_FILTER_BUTTONS			16 // Madd: Increased to 16 for new categories
#define		NUMBER_USED_FILTER_BUTTONS			4

#define		BOBBYR_GUNS_FILTER_BUTTON_GAP			BOBBYR_CATALOGUE_BUTTON_GAP - 1
#define		BOBBYR_AMMO_FILTER_BUTTON_GAP			BOBBYR_CATALOGUE_BUTTON_GAP - 1
#define		BOBBYR_USED_FILTER_BUTTON_GAP			BOBBYR_CATALOGUE_BUTTON_GAP - 1
#define		BOBBYR_ARMOUR_FILTER_BUTTON_GAP			BOBBYR_CATALOGUE_BUTTON_GAP - 1
#define		BOBBYR_MISC_FILTER_BUTTON_GAP			BOBBYR_CATALOGUE_BUTTON_GAP - 1

//Madd: new BR filters
#define		BR_MISC_FILTER_OPTICS				(AC_SCOPE | AC_SIGHT | AC_IRONSIGHT)
#define		BR_MISC_FILTER_SIDE_BOTTOM			(AC_LASER | AC_FOREGRIP | AC_BIPOD)
#define		BR_MISC_FILTER_MUZZLE				(AC_MUZZLE | AC_EXTENDER)
#define		BR_MISC_FILTER_STOCK				AC_STOCK
#define		BR_MISC_FILTER_INTERNAL				(AC_MAGWELL | AC_INTERNAL | AC_EXTERNAL)
#define		BR_MISC_FILTER_STD_ATTACHMENTS		(BR_MISC_FILTER_OPTICS | BR_MISC_FILTER_SIDE_BOTTOM | BR_MISC_FILTER_MUZZLE | BR_MISC_FILTER_STOCK | BR_MISC_FILTER_INTERNAL)
#define		BR_MISC_FILTER_OTHER_ATTACHMENTS	AC_SLING
#define		BR_MISC_FILTER_NO_ATTACHMENTS		0


UINT32	guiBobbyRFilterGuns[ NUMBER_GUNS_FILTER_BUTTONS ];
UINT32	guiBobbyRFilterAmmo[ NUMBER_AMMO_FILTER_BUTTONS ];
UINT32	guiBobbyRFilterArmour[ NUMBER_ARMOUR_FILTER_BUTTONS ];
UINT32	guiBobbyRFilterMisc[ NUMBER_MISC_FILTER_BUTTONS ];
UINT32	guiBobbyRFilterUsed[ NUMBER_USED_FILTER_BUTTONS ];

INT32		guiBobbyRFilterImage;

void BtnBobbyRFilterGunsCallback(GUI_BUTTON *btn,INT32 reason);
void BtnBobbyRFilterAmmoCallback(GUI_BUTTON *btn,INT32 reason);
void BtnBobbyRFilterUsedCallback(GUI_BUTTON *btn,INT32 reason);
void BtnBobbyRFilterArmourCallback(GUI_BUTTON *btn,INT32 reason);
void BtnBobbyRFilterMiscCallback(GUI_BUTTON *btn,INT32 reason);

BOOLEAN IsAmmoMatchinWeaponType(UINT16 usItemIndex, UINT8 ubWeaponType);

INT8			ubFilterGunsButtonValues[] = {
							BOBBYR_FILTER_GUNS_PISTOL,
							BOBBYR_FILTER_GUNS_M_PISTOL,
							BOBBYR_FILTER_GUNS_SMG,
							BOBBYR_FILTER_GUNS_RIFLE,
							BOBBYR_FILTER_GUNS_SN_RIFLE,
							BOBBYR_FILTER_GUNS_AS_RIFLE,
							BOBBYR_FILTER_GUNS_LMG,
							BOBBYR_FILTER_GUNS_SHOTGUN,
							BOBBYR_FILTER_GUNS_HEAVY};

INT8			ubFilterAmmoButtonValues[] = {
							BOBBYR_FILTER_AMMO_PISTOL,
							BOBBYR_FILTER_AMMO_M_PISTOL,
							BOBBYR_FILTER_AMMO_SMG,
							BOBBYR_FILTER_AMMO_RIFLE,
							BOBBYR_FILTER_AMMO_SN_RIFLE,
							BOBBYR_FILTER_AMMO_AS_RIFLE,
							BOBBYR_FILTER_AMMO_LMG,
							BOBBYR_FILTER_AMMO_SHOTGUN};

INT8			ubFilterUsedButtonValues[] = {
							BOBBYR_FILTER_USED_GUNS,
							BOBBYR_FILTER_USED_ARMOR,
							BOBBYR_FILTER_USED_LBEGEAR,
							BOBBYR_FILTER_USED_MISC};

INT8			ubFilterArmourButtonValues[] = {
							BOBBYR_FILTER_ARMOUR_HELM,
							BOBBYR_FILTER_ARMOUR_VEST,
							BOBBYR_FILTER_ARMOUR_LEGGING,
							BOBBYR_FILTER_ARMOUR_PLATE};

INT8			ubFilterMiscButtonValues[] = {
							BOBBYR_FILTER_MISC_BLADE,
							BOBBYR_FILTER_MISC_THROWING_KNIFE,
							BOBBYR_FILTER_MISC_PUNCH,
							BOBBYR_FILTER_MISC_GRENADE,
							BOBBYR_FILTER_MISC_BOMB,
							BOBBYR_FILTER_MISC_MEDKIT,
							BOBBYR_FILTER_MISC_KIT,
							BOBBYR_FILTER_MISC_FACE,
							BOBBYR_FILTER_MISC_LBEGEAR,
							BOBBYR_FILTER_MISC_OPTICS_ATTACHMENTS, // Madd: new BR filters
							BOBBYR_FILTER_MISC_SIDE_AND_BOTTOM_ATTACHMENTS,
							BOBBYR_FILTER_MISC_MUZZLE_ATTACHMENTS,
							BOBBYR_FILTER_MISC_STOCK_ATTACHMENTS,
							BOBBYR_FILTER_MISC_INTERNAL_ATTACHMENTS,
							BOBBYR_FILTER_MISC_OTHER_ATTACHMENTS,
							BOBBYR_FILTER_MISC_MISC};

extern	BOOLEAN fExitingLaptopFlag;

UINT32		guiGunBackground;
UINT32		guiGunsGrid;
UINT32		guiBrTitle;
UINT16		gusCurWeaponIndex;
UINT16		gubCurPage;
UINT8			ubCatalogueButtonValues[] = {
							LAPTOP_MODE_BOBBY_R_GUNS,
							LAPTOP_MODE_BOBBY_R_AMMO,
							LAPTOP_MODE_BOBBY_R_ARMOR,
							LAPTOP_MODE_BOBBY_R_MISC,
							LAPTOP_MODE_BOBBY_R_USED};

UINT16		gusLastItemIndex=0;
UINT16		gusFirstItemIndex=0;
UINT8			gubNumItemsOnScreen;
UINT16		gubNumPages;


BOOLEAN		gfBigImageMouseRegionCreated;
UINT16		gusItemNumberForItemsOnScreen[ BOBBYR_NUM_WEAPONS_ON_PAGE ];

namespace
{
LaptopPageResourceOwner gBobbyRGunsResources;
LaptopPageResourceOwner gBobbyRBigImageResources;

bool IsLoadedBobbyRayItem(std::size_t itemIndex) noexcept
{
	return BobbyRayCatalogueModel::IsCatalogueIndexInBounds(
		itemIndex, std::min<std::size_t>(gMAXITEMS_READ, MAXITEMS));
}
}


BOOLEAN		gfOnUsedPage;

UINT16		gusOldItemNumOnTopOfPage=65535;

//The menu bar at the bottom that changes to different pages
void BtnBobbyRPageMenuCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiBobbyRPageMenu[ NUM_CATALOGUE_BUTTONS ];
INT32		guiBobbyRPageMenuImage;

//The next and previous buttons
void		BtnBobbyRNextPreviousPageCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiBobbyRPreviousPage;
INT32		guiBobbyRPreviousPageImage;

UINT32	guiBobbyRNextPage;
INT32		guiBobbyRNextPageImage;


// Big Image Mouse region
MOUSE_REGION	gSelectedBigImageRegion[ BOBBYR_NUM_WEAPONS_ON_PAGE ];
void SelectBigImageRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

// The order form button
void BtnBobbyROrderFormCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiBobbyROrderForm;
INT32		guiBobbyROrderFormImage;

// The Home button
void BtnBobbyRHomeButtonCallback(GUI_BUTTON *btn,INT32 reason);
extern UINT32	guiBobbyRHome; // symbol already defined in BobbyRMailOrder.cpp (jonathanl)
extern INT32 guiBobbyRHomeImage; // symbol already defined in BobbyRMailOrder.cpp (jonathanl)


// Link from the title
MOUSE_REGION	gSelectedTitleImageLinkRegion;
void SelectTitleImageLinkRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );



UINT32	guiTempCurrentMode;

BOOLEAN DisplayNonGunWeaponInfo(UINT16 usItemIndex, UINT16 usTextPosY, BOOLEAN fUsed, UINT16 usBobbyIndex);
BOOLEAN DisplayArmourInfo(UINT16 usItemIndex, UINT16 usTextPosY, BOOLEAN fUsed, UINT16 usBobbyIndex);
BOOLEAN DisplayMiscInfo(UINT16 usItemIndex, UINT16 usTextPosY, BOOLEAN fUsed, UINT16 usBobbyIndex);
BOOLEAN DisplayGunInfo(UINT16 usItemIndex, UINT16 usTextPosY, BOOLEAN fUsed, UINT16 usBobbyIndex);
BOOLEAN DisplayAmmoInfo(UINT16 usItemIndex, UINT16 usPosY, BOOLEAN fUsed, UINT16 usBobbyIndex);
BOOLEAN DisplayGrenadeBombInfo(UINT16 usIndex, UINT16 usTextPosY, BOOLEAN fUsed, UINT16 usBobbyIndex);

BOOLEAN DisplayBigItemImage(UINT16 ubIndex, UINT16 usPosY);
//void InitFirstAndLastGlobalIndex(UINT32 ubItemClassMask);
UINT16 DisplayCostAndQty(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight, UINT16 usBobbyIndex, BOOLEAN fUsed);
UINT16 DisplayRof(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayGunAP(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayMeleeAP(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayDamage(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayRange(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayMagazine(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
void DisplayItemNameAndInfo(UINT16 usPosY, UINT16 usIndex, UINT16 usBobbyIndex, BOOLEAN fUsed);
// CHRISL: New display function for LBE Gear
UINT16 DisplayLBEInfo(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayWeight(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayCaliber(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayExplosiveDamage(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayExplosiveStunDamage(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayProtection(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayCamo(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayAmmoArmourPierceModifier(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayAmmoDamageModifier(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
UINT16 DisplayAmmoProjectileCount(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight);
void CreateMouseRegionForBigImage(UINT16 usPosY, UINT8 ubCount, INT16 *pItemNumbers );
// HEADROCK HAM 4: Now can purchase ALL items available.
void PurchaseBobbyRayItem(UINT16	usItemNumber, BOOLEAN fAllItems );
UINT8 CheckIfItemIsPurchased(UINT16 usItemNumber);
UINT8 GetNextPurchaseNumber();
// HEADROCK HAM 4: Now can purchase ALL items available.
void UnPurchaseBobbyRayItem(UINT16	usItemNumber, BOOLEAN fAllItems );
UINT32 CalculateTotalPurchasePrice();
void DisableBobbyRButtons();
void CalcFirstIndexForPage( STORE_INVENTORY *pInv, UINT32 uiItemClass );
void OutOfStockMessageBoxCallBack( UINT8 bExitValue );
UINT8 CheckPlayersInventoryForGunMatchingGivenAmmoID( INT16 sItemID );
void BobbyrRGunsHelpTextDoneCallBack( void );

//Buggler: Seperated function for easy code reuse
void GetHelpTextForItemInLaptop( CHAR16 *pzStr, UINT16 usItemNumber );

#ifdef JA2BETAVERSION
	void ReportBobbyROrderError( UINT16 usItemNumber, UINT8 ubPurchaseNum, UINT8 ubQtyOnHand, UINT8 ubNumPurchasing );
#endif

//Hotkey Assignment
void HandleBobbyRGunsKeyBoardInput();
void HandleBobbyRayMouseWheel(void);

// Appends source STR16 to target STR16 using decorators ("\n" and "...").
// Returns TRUE if everything fits target, FALSE otherwise.
static BOOLEAN DecorateAppendString(CHAR16 *target, size_t targetCapacity, STR16 source, UINT32 frontDecoratorsCnt = 1)
{
	const CHAR16 DECORATOR0[] = L"\n";
	const CHAR16 DECORATOR1[] = L"\n...";
	BOOLEAN result = FALSE;
	size_t decoratorLen = wcslen(DECORATOR0) * frontDecoratorsCnt;

	if (wcslen(target) + decoratorLen + wcslen(source) + 1 < targetCapacity)
	{
		for (UINT32 i = 0; i < frontDecoratorsCnt; i++)
			wcscat(target, DECORATOR0);
		wcscat(target, source);
		result = TRUE;
	}
	else if (wcslen(target) + wcslen(DECORATOR1) + 1 < targetCapacity)
	{
		wcscat(target, DECORATOR1);
	}  // otherwise don't even touch the target
	return result;
}

void GameInitBobbyRGuns()
{
	guiTempCurrentMode=0;

	guiPrevGunFilterMode = -1;
	guiPrevAmmoFilterMode = -1;
	guiPrevUsedFilterMode = -1;
	guiPrevArmourFilterMode = -1;
	guiPrevMiscFilterMode = -1;

	guiCurrentGunFilterMode = -1;
	guiCurrentAmmoFilterMode = -1;
	guiCurrentUsedFilterMode = -1;
	guiCurrentArmourFilterMode = -1;
	guiCurrentMiscFilterMode = -1;
	guiCurrentMiscSubFilterMode = -1;

	memset(BobbyRayPurchases, 0, sizeof(BobbyRayPurchases));
}

void EnterInitBobbyRGuns()
{
	guiTempCurrentMode=0;

	memset(BobbyRayPurchases, 0, sizeof(BobbyRayPurchases));
}



BOOLEAN EnterBobbyRGuns()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner staged;

	DeleteMouseRegionForBigImage();
	gBobbyRGunsResources.clear();

	// load the background graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\gunbackground.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiGunBackground)) return FALSE;

	// load the gunsgrid graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\gunsgrid.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiGunsGrid)) return FALSE;

	if (!InitBobbyBrTitle(staged)) return FALSE;

	guiPrevGunFilterMode = -1;
	guiCurrentGunFilterMode = -1;

	// Search for all guns. Take NOT_GUN for all guns
	SetFirstLastPagesForNew( IC_BOBBY_GUN, guiCurrentGunFilterMode );

	//Draw menu bar
	if (!InitBobbyMenuBar(staged) ||
		!InitBobbyRGunsFilterBar(staged)) return FALSE;
	gBobbyRGunsResources = std::move(staged);

	// render once
	RenderBobbyRGuns( );

	return(TRUE);
}

void ExitBobbyRGuns()
{
	DeleteMouseRegionForBigImage();
	gBobbyRGunsResources.clear();

	giCurrentSubPage = gusCurWeaponIndex;
	guiLastBobbyRayPage = LAPTOP_MODE_BOBBY_R_GUNS;
}

void HandleBobbyRGuns()
{
	//Hotkey Assignment
	HandleBobbyRGunsKeyBoardInput();
	HandleBobbyRayMouseWheel();
}

void RenderBobbyRGuns()
{
	HVOBJECT hPixHandle;

	WebPageTileBackground(BOBBYR_NUM_HORIZONTAL_TILES, BOBBYR_NUM_VERTICAL_TILES, BOBBYR_BACKGROUND_WIDTH, BOBBYR_BACKGROUND_HEIGHT, guiGunBackground);

	// WANNE: Do not display the title
	//Display title at top of page
	//DisplayBobbyRBrTitle();

	// GunForm
	GetVideoObject(&hPixHandle, guiGunsGrid);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, BOBBYR_GRIDLOC_X, BOBBYR_GRIDLOC_Y, VO_BLT_SRCTRANSPARENCY,NULL);

	DisplayItemInfo( IC_BOBBY_GUN, guiCurrentGunFilterMode );
	UpdateButtonText(guiCurrentLaptopMode);
	UpdateGunFilterButtons();

	MarkButtonsDirty( );
	RenderWWWProgramTitleBar( );
	//InvalidateRegion(0,0,SCREEN_WIDTH, SCREEN_HEIGHT);
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
	//Moa removed below. See comment above LAPTOP_SCREEN_UL_X in laptop.h
	//	fReDrawScreenFlag = TRUE;
	//fPausedReDrawScreenFlag = TRUE;
}



BOOLEAN DisplayBobbyRBrTitle()
{
	HVOBJECT hPixHandle;

	// BR title
	GetVideoObject(&hPixHandle, guiBrTitle);
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, BOBBYR_BRTITLE_X, BOBBYR_BRTITLE_Y, VO_BLT_SRCTRANSPARENCY,NULL);

	// To Order Text
	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_TO_ORDER], BOBBYR_TO_ORDER_TITLE_X, BOBBYR_TO_ORDER_TITLE_Y, 0, BOBBYR_ORDER_TITLE_FONT, BOBBYR_ORDER_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	//First put a shadow behind the image
	ShadowVideoSurfaceRect( FRAME_BUFFER, BOBBYR_TO_ORDER_TEXT_X-2, BOBBYR_TO_ORDER_TEXT_Y-2, BOBBYR_TO_ORDER_TEXT_X+BOBBYR_TO_ORDER_TEXT_WIDTH, BOBBYR_TO_ORDER_TEXT_Y+31);

	//To Order text
	DisplayWrappedString(BOBBYR_TO_ORDER_TEXT_X, BOBBYR_TO_ORDER_TEXT_Y, BOBBYR_TO_ORDER_TEXT_WIDTH, 2, BOBBYR_ORDER_TEXT_FONT, BOBBYR_ORDER_TEXT_COLOR, BobbyRText[BOBBYR_GUNS_CLICK_ON_ITEMS], FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	return(TRUE);
}


BOOLEAN InitBobbyBrTitle(LaptopPageResourceOwner& owner)
{
	VOBJECT_DESC	VObjectDesc;

	// load the br title graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	GetMLGFilename( VObjectDesc.ImageFile, MLG_BR );
	if (!owner.addVideoObject(&VObjectDesc, guiBrTitle)) return FALSE;

	//initialize the link to the homepage by clicking on the title
	MSYS_DefineRegion( &gSelectedTitleImageLinkRegion, BOBBYR_BRTITLE_X, BOBBYR_BRTITLE_Y, (BOBBYR_BRTITLE_X + BOBBYR_BRTITLE_WIDTH), (UINT16)(BOBBYR_BRTITLE_Y + BOBBYR_BRTITLE_HEIGHT), MSYS_PRIORITY_HIGH,
							CURSOR_WWW, MSYS_NO_CALLBACK, SelectTitleImageLinkRegionCallBack);
	if (!owner.addRegion(gSelectedTitleImageLinkRegion)) return FALSE;


	gusOldItemNumOnTopOfPage=65535;

	return(TRUE);
}


void SelectTitleImageLinkRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		guiCurrentLaptopMode = LAPTOP_MODE_BOBBY_R;
	}
}

BOOLEAN InitBobbyRGunsFilterBar(LaptopPageResourceOwner& owner)
{
	UINT8	i;
	UINT16	usPosX;
	UINT8		bCurMode;
	UINT16	usYOffset = 0;

	bCurMode = 0;
	usPosX = FILTER_BUTTONS_GUN_START_X;

	std::fill_n(guiBobbyRFilterGuns, NUMBER_GUNS_FILTER_BUTTONS, 0);
	if (!owner.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\CatalogueButton1.sti", -1, 0, -1, 1, -1),
			guiBobbyRFilterImage)) return FALSE;

	// Loop through the filter buttons
	for(i=0; i<NUMBER_GUNS_FILTER_BUTTONS; i++)
	{
		// Next row
		if (i > 7)
		{
			usPosX = FILTER_BUTTONS_GUN_START_X;
			usYOffset = FILTER_BUTTONS_Y_OFFSET;
		}

		// Filter buttons
		if (!owner.addButton(CreateIconAndTextButton( guiBobbyRFilterImage, BobbyRFilter[BOBBYR_FILTER_GUNS_PISTOL+i], BOBBYR_GUNS_BUTTON_FONT,
													BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
													BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
													TEXT_CJUSTIFIED,
													usPosX, FILTER_BUTTONS_Y + usYOffset, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
											DEFAULT_MOVE_CALLBACK, BtnBobbyRFilterGunsCallback),
			guiBobbyRFilterGuns[i])) return FALSE;

		SetButtonCursor(guiBobbyRFilterGuns[i], CURSOR_LAPTOP_SCREEN);

		MSYS_SetBtnUserData( guiBobbyRFilterGuns[i], 0, ubFilterGunsButtonValues[bCurMode]);

		usPosX += BOBBYR_GUNS_FILTER_BUTTON_GAP;
		bCurMode++;
	}


	return(TRUE);
}

BOOLEAN InitBobbyRAmmoFilterBar(LaptopPageResourceOwner& owner)
{
	UINT8	i;
	UINT16	usPosX;
	UINT8		bCurMode;
	UINT16	usYOffset = 0;


	bCurMode = 0;
	usPosX = FILTER_BUTTONS_AMMO_START_X;

	std::fill_n(guiBobbyRFilterAmmo, NUMBER_AMMO_FILTER_BUTTONS, 0);
	if (!owner.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\CatalogueButton1.sti", -1, 0, -1, 1, -1),
			guiBobbyRFilterImage)) return FALSE;

	// Loop through the filter buttons
	for(i=0; i<NUMBER_AMMO_FILTER_BUTTONS; i++)
	{
		// Next row
		if (i > 7)
		{
			usPosX = FILTER_BUTTONS_AMMO_START_X;
			usYOffset = FILTER_BUTTONS_Y_OFFSET;
		}

		// Filter buttons
		if (!owner.addButton(CreateIconAndTextButton( guiBobbyRFilterImage, BobbyRFilter[BOBBYR_FILTER_AMMO_PISTOL+i], BOBBYR_GUNS_BUTTON_FONT,
													BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
													BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
													TEXT_CJUSTIFIED,
													usPosX, FILTER_BUTTONS_Y + usYOffset, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
											DEFAULT_MOVE_CALLBACK, BtnBobbyRFilterAmmoCallback),
			guiBobbyRFilterAmmo[i])) return FALSE;

		SetButtonCursor(guiBobbyRFilterAmmo[i], CURSOR_LAPTOP_SCREEN);

		MSYS_SetBtnUserData( guiBobbyRFilterAmmo[i], 0, ubFilterAmmoButtonValues[bCurMode]);

		usPosX += BOBBYR_AMMO_FILTER_BUTTON_GAP;
		bCurMode++;
	}


	return(TRUE);
}

BOOLEAN InitBobbyRArmourFilterBar(LaptopPageResourceOwner& owner)
{
	UINT8	i;
	UINT16	usPosX;
	UINT8		bCurMode;
	UINT16	usYOffset = 0;

	bCurMode = 0;
	usPosX = FILTER_BUTTONS_GUN_START_X;

	std::fill_n(guiBobbyRFilterArmour, NUMBER_ARMOUR_FILTER_BUTTONS, 0);
	if (!owner.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\CatalogueButton1.sti", -1, 0, -1, 1, -1),
			guiBobbyRFilterImage)) return FALSE;

	// Loop through the filter buttons
	for(i=0; i<NUMBER_ARMOUR_FILTER_BUTTONS; i++)
	{
		// Next row
		if (i > 7)
		{
			usPosX = FILTER_BUTTONS_ARMOUR_START_X;
			usYOffset = FILTER_BUTTONS_Y_OFFSET;
		}

		// Filter buttons
		if (!owner.addButton(CreateIconAndTextButton( guiBobbyRFilterImage, BobbyRFilter[BOBBYR_FILTER_ARMOUR_HELM+i], BOBBYR_GUNS_BUTTON_FONT,
											BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
											BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
											TEXT_CJUSTIFIED,
											usPosX, FILTER_BUTTONS_Y + usYOffset, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
											DEFAULT_MOVE_CALLBACK, BtnBobbyRFilterArmourCallback),
			guiBobbyRFilterArmour[i])) return FALSE;

		SetButtonCursor(guiBobbyRFilterArmour[i], CURSOR_LAPTOP_SCREEN);

		MSYS_SetBtnUserData( guiBobbyRFilterArmour[i], 0, ubFilterArmourButtonValues[bCurMode]);

		usPosX += BOBBYR_ARMOUR_FILTER_BUTTON_GAP;
		bCurMode++;
	}


	return(TRUE);
}


BOOLEAN InitBobbyRUsedFilterBar(LaptopPageResourceOwner& owner)
{
	UINT8	i;
	UINT16	usPosX;

	usPosX = FILTER_BUTTONS_USED_START_X;

	std::fill_n(guiBobbyRFilterUsed, NUMBER_USED_FILTER_BUTTONS, 0);
	if (!owner.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\CatalogueButton1.sti", -1, 0, -1, 1, -1),
			guiBobbyRFilterImage)) return FALSE;

	// Loop through the filter buttons
	for(i=0; i<NUMBER_USED_FILTER_BUTTONS; i++)
	{
		//CHRISL: Don't display the LBEGEAR button if we're using the old inventory system
		if((UsingNewInventorySystem() == false) && ubFilterUsedButtonValues[i] == BOBBYR_FILTER_USED_LBEGEAR)
		{
			continue;
		}

		// Filter buttons
		if (!owner.addButton(CreateIconAndTextButton( guiBobbyRFilterImage, BobbyRFilter[BOBBYR_FILTER_USED_GUNS+i], BOBBYR_GUNS_BUTTON_FONT,
											BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
											BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
											TEXT_CJUSTIFIED,
											usPosX, FILTER_BUTTONS_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
											DEFAULT_MOVE_CALLBACK, BtnBobbyRFilterUsedCallback),
			guiBobbyRFilterUsed[i])) return FALSE;

		SetButtonCursor(guiBobbyRFilterUsed[i], CURSOR_LAPTOP_SCREEN);

		MSYS_SetBtnUserData( guiBobbyRFilterUsed[i], 0, ubFilterUsedButtonValues[i]);

		usPosX += BOBBYR_USED_FILTER_BUTTON_GAP;
	}


	return(TRUE);
}

BOOLEAN InitBobbyRMiscFilterBar(LaptopPageResourceOwner& owner)
{
	UINT8	i;
	UINT16	usPosX = 0, usPosY = 0;
	UINT8	bCurMode;
	UINT16	usYOffset = FILTER_BUTTONS_Y_OFFSET - 2, sItemWidth = 8; //Madd: reduced Y offset to 20 to make more room
	UINT16	usXOffset = BOBBYR_MISC_FILTER_BUTTON_GAP;

	bCurMode = 0;

	std::fill_n(guiBobbyRFilterMisc, NUMBER_MISC_FILTER_BUTTONS, 0);
	if (!owner.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\CatalogueButton1.sti", -1, 0, -1, 1, -1),
			guiBobbyRFilterImage)) return FALSE;

	// Loop through the filter buttons
	for(i=0; i<NUMBER_MISC_FILTER_BUTTONS; i++)
	{
		//CHRISL: Don't display the LBEGEAR button if we're using the old inventory system
		if((UsingNewInventorySystem() == false) && ubFilterMiscButtonValues[i] == BOBBYR_FILTER_MISC_LBEGEAR)
		{
			continue;
		}


		usPosX = FILTER_BUTTONS_MISC_START_X + ( (bCurMode % sItemWidth) * usXOffset);
		usPosY = FILTER_BUTTONS_Y + ( (bCurMode / sItemWidth) * usYOffset);

		// Filter buttons
		if (!owner.addButton(CreateIconAndTextButton( guiBobbyRFilterImage, BobbyRFilter[BOBBYR_FILTER_MISC_BLADE+i], BOBBYR_GUNS_BUTTON_FONT,
											BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
											BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
											TEXT_CJUSTIFIED,
											usPosX, usPosY, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
											DEFAULT_MOVE_CALLBACK, BtnBobbyRFilterMiscCallback),
			guiBobbyRFilterMisc[i])) return FALSE;

		SetButtonCursor(guiBobbyRFilterMisc[i], CURSOR_LAPTOP_SCREEN);

		MSYS_SetBtnUserData( guiBobbyRFilterMisc[i], 0, ubFilterMiscButtonValues[i]);
		bCurMode++;
	}


	return(TRUE);
}

BOOLEAN InitBobbyMenuBar(LaptopPageResourceOwner& owner)
{
	UINT8	i;
	UINT16	usPosX;
	UINT8		bCurMode;

	// Previous button
	if (!owner.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\PreviousButton.sti", -1, 0, -1, 1, -1),
			guiBobbyRPreviousPageImage)) return FALSE;
	if (!owner.addButton(CreateIconAndTextButton( guiBobbyRPreviousPageImage, BobbyRText[BOBBYR_GUNS_PREVIOUS_ITEMS], BOBBYR_GUNS_BUTTON_FONT,
											BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
											BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
											TEXT_CJUSTIFIED,
											BOBBYR_PREVIOUS_BUTTON_X, BOBBYR_PREVIOUS_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
											DEFAULT_MOVE_CALLBACK, BtnBobbyRNextPreviousPageCallback),
		guiBobbyRPreviousPage)) return FALSE;
	SetButtonCursor(guiBobbyRPreviousPage, CURSOR_LAPTOP_SCREEN);
	MSYS_SetBtnUserData( guiBobbyRPreviousPage, 0, BOBBYR_PREVIOUS_PAGE);
	SpecifyDisabledButtonStyle( guiBobbyRPreviousPage, DISABLED_STYLE_SHADED );


	if (!owner.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\CatalogueButton1.sti", -1, 0, -1, 1, -1),
			guiBobbyRPageMenuImage)) return FALSE;

	// Next button
	if (!owner.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\NextButton.sti", -1, 0, -1, 1, -1),
			guiBobbyRNextPageImage)) return FALSE;
	if (!owner.addButton(CreateIconAndTextButton( guiBobbyRNextPageImage, BobbyRText[BOBBYR_GUNS_MORE_ITEMS], BOBBYR_GUNS_BUTTON_FONT,
											BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
											BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
											TEXT_CJUSTIFIED,
											BOBBYR_NEXT_BUTTON_X, BOBBYR_NEXT_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
											DEFAULT_MOVE_CALLBACK, BtnBobbyRNextPreviousPageCallback),
		guiBobbyRNextPage)) return FALSE;
	SetButtonCursor(guiBobbyRNextPage, CURSOR_LAPTOP_SCREEN);
	MSYS_SetBtnUserData( guiBobbyRNextPage, 0, BOBBYR_NEXT_PAGE);
	SpecifyDisabledButtonStyle( guiBobbyRNextPage, DISABLED_STYLE_SHADED );


	bCurMode = 0;
	usPosX = BOBBYR_CATALOGUE_BUTTON_START_X;
	std::fill_n(guiBobbyRPageMenu, NUM_CATALOGUE_BUTTONS, 0);
	for(i=0; i<NUM_CATALOGUE_BUTTONS; i++)
	{
		// Catalogue Buttons button
		if (!owner.addButton(CreateIconAndTextButton( guiBobbyRPageMenuImage, BobbyRText[BOBBYR_GUNS_GUNS+i], BOBBYR_GUNS_BUTTON_FONT,
											BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
											BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
											TEXT_CJUSTIFIED,
											usPosX, BOBBYR_CATALOGUE_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
											DEFAULT_MOVE_CALLBACK, BtnBobbyRPageMenuCallback),
			guiBobbyRPageMenu[i])) return FALSE;

		SetButtonCursor(guiBobbyRPageMenu[i], CURSOR_LAPTOP_SCREEN);

		MSYS_SetBtnUserData( guiBobbyRPageMenu[i], 0, ubCatalogueButtonValues[bCurMode]);

		usPosX += BOBBYR_CATALOGUE_BUTTON_GAP;
		bCurMode++;
	}

	// Order Form button
	if (!owner.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\OrderFormButton.sti", -1, 0, -1, 1, -1),
			guiBobbyROrderFormImage)) return FALSE;
	if (!owner.addButton(CreateIconAndTextButton( guiBobbyROrderFormImage, BobbyRText[BOBBYR_GUNS_ORDER_FORM], BOBBYR_GUNS_BUTTON_FONT,
											BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
											BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
											TEXT_CJUSTIFIED,
											BOBBYR_ORDER_FORM_X, BOBBYR_ORDER_FORM_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
											DEFAULT_MOVE_CALLBACK, BtnBobbyROrderFormCallback),
		guiBobbyROrderForm)) return FALSE;

	SetButtonCursor(guiBobbyROrderForm, CURSOR_LAPTOP_SCREEN);


	// Home button

	if (!owner.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\CatalogueButton.sti", -1, 0, -1, 1, -1),
			guiBobbyRHomeImage)) return FALSE;
	if (!owner.addButton(CreateIconAndTextButton( guiBobbyRHomeImage, BobbyRText[BOBBYR_GUNS_HOME], BOBBYR_GUNS_BUTTON_FONT,
											BOBBYR_GUNS_TEXT_COLOR_ON, BOBBYR_GUNS_SHADOW_COLOR,
											BOBBYR_GUNS_TEXT_COLOR_OFF, BOBBYR_GUNS_SHADOW_COLOR,
											TEXT_CJUSTIFIED,
											BOBBYR_HOME_BUTTON_X, BOBBYR_HOME_BUTTON_Y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
											DEFAULT_MOVE_CALLBACK, BtnBobbyRHomeButtonCallback),
		guiBobbyRHome)) return FALSE;
	SetButtonCursor(guiBobbyRHome, CURSOR_LAPTOP_SCREEN);

	return(TRUE);
}

void BtnBobbyRPageMenuCallback(GUI_BUTTON *btn,INT32 reason)
{
	UINT32		bNewValue;
	bNewValue = MSYS_GetBtnUserData( btn, 0 );

	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	//gbMessageDisplayed = TRUE;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;

		guiTempCurrentMode = bNewValue;

		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

		guiTempCurrentMode = BOBBYR_DEFAULT_MENU_COLOR;
		UpdateButtonText(guiCurrentLaptopMode);

		// Set the new category button
		guiCurrentLaptopMode = bNewValue;

		// Set no Filter!
		switch (bNewValue)
		{
			case LAPTOP_MODE_BOBBY_R_GUNS:
				guiPrevGunFilterMode = guiCurrentGunFilterMode;
				guiCurrentGunFilterMode = -1;
				UpdateGunFilterButtons();
				SetFirstLastPagesForNew(IC_BOBBY_GUN, guiCurrentGunFilterMode);
				break;
			case LAPTOP_MODE_BOBBY_R_AMMO:
				guiPrevAmmoFilterMode = guiCurrentAmmoFilterMode;
				guiCurrentAmmoFilterMode = -1;
				UpdateAmmoFilterButtons();
				SetFirstLastPagesForNew(IC_AMMO, guiCurrentAmmoFilterMode);
				break;
			case LAPTOP_MODE_BOBBY_R_ARMOR:
				guiPrevArmourFilterMode = guiCurrentArmourFilterMode;
				guiCurrentArmourFilterMode = -1;
				UpdateArmourFilterButtons();
				SetFirstLastPagesForNew(IC_ARMOUR, guiCurrentArmourFilterMode);
				break;
			case LAPTOP_MODE_BOBBY_R_MISC:
				guiPrevMiscFilterMode = guiCurrentMiscFilterMode;
				guiPrevMiscSubFilterMode = guiCurrentMiscSubFilterMode;
				guiCurrentMiscFilterMode = -1;
				guiCurrentMiscSubFilterMode = -1;
				UpdateMiscFilterButtons();
				SetFirstLastPagesForNew(IC_BOBBY_MISC, guiCurrentMiscFilterMode, guiCurrentMiscSubFilterMode);
				break;
			case LAPTOP_MODE_BOBBY_R_USED:
				guiPrevUsedFilterMode = guiCurrentUsedFilterMode;
				guiCurrentUsedFilterMode = -1;

				UpdateUsedFilterButtons();

				SetFirstLastPagesForUsed(guiCurrentUsedFilterMode);
				break;
		}
	}

	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		guiTempCurrentMode = BOBBYR_DEFAULT_MENU_COLOR;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


void BtnBobbyRFilterGunsCallback(GUI_BUTTON *btn,INT32 reason)
{
	UINT32		bNewValue;
	bNewValue = MSYS_GetBtnUserData( btn, 0 );

	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;

		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

		guiPrevGunFilterMode = guiCurrentGunFilterMode;

		switch (bNewValue)
		{
			case BOBBYR_FILTER_GUNS_PISTOL:
				guiCurrentGunFilterMode = GUN_PISTOL;
				break;
			case BOBBYR_FILTER_GUNS_M_PISTOL:
				guiCurrentGunFilterMode = GUN_M_PISTOL;
				break;
			case BOBBYR_FILTER_GUNS_SMG:
				guiCurrentGunFilterMode = GUN_SMG;
				break;
			case BOBBYR_FILTER_GUNS_RIFLE:
				guiCurrentGunFilterMode = GUN_RIFLE;
				break;
			case BOBBYR_FILTER_GUNS_SN_RIFLE:
				guiCurrentGunFilterMode = GUN_SN_RIFLE;
				break;
			case BOBBYR_FILTER_GUNS_AS_RIFLE:
				guiCurrentGunFilterMode = GUN_AS_RIFLE;
				break;
			case BOBBYR_FILTER_GUNS_LMG:
				guiCurrentGunFilterMode = GUN_LMG;
				break;
			case BOBBYR_FILTER_GUNS_SHOTGUN:
				guiCurrentGunFilterMode = GUN_SHOTGUN;
				break;
			// "Heavy Weapons" (Weapon Type = NOT_GUN
			// ItemClass = IC_LAUNCHER. (IC_LAUNCHER is already included in IC_BOBBY_GUN!)
			case BOBBYR_FILTER_GUNS_HEAVY:
				guiCurrentGunFilterMode = NOT_GUN;
				break;
		}

		SetFirstLastPagesForNew(IC_BOBBY_GUN, guiCurrentGunFilterMode);
		UpdateGunFilterButtons();

		DeleteMouseRegionForBigImage();
		fReDrawScreenFlag = TRUE;
		fPausedReDrawScreenFlag = TRUE;
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}

void BtnBobbyRFilterAmmoCallback(GUI_BUTTON *btn,INT32 reason)
{
	UINT32		bNewValue;
	bNewValue = MSYS_GetBtnUserData( btn, 0 );

	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	//gbMessageDisplayed = TRUE;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

		guiPrevAmmoFilterMode = guiCurrentAmmoFilterMode;

		switch (bNewValue)
		{
			case BOBBYR_FILTER_AMMO_PISTOL:
				guiCurrentAmmoFilterMode = GUN_PISTOL;
				break;
			case BOBBYR_FILTER_AMMO_M_PISTOL:
				guiCurrentAmmoFilterMode = GUN_M_PISTOL;
				break;
			case BOBBYR_FILTER_AMMO_SMG:
				guiCurrentAmmoFilterMode = GUN_SMG;
				break;
			case BOBBYR_FILTER_AMMO_RIFLE:
				guiCurrentAmmoFilterMode = GUN_RIFLE;
				break;
			case BOBBYR_FILTER_AMMO_SN_RIFLE:
				guiCurrentAmmoFilterMode = GUN_SN_RIFLE;
				break;
			case BOBBYR_FILTER_AMMO_AS_RIFLE:
				guiCurrentAmmoFilterMode = GUN_AS_RIFLE;
				break;
			case BOBBYR_FILTER_AMMO_LMG:
				guiCurrentAmmoFilterMode = GUN_LMG;
				break;
			case BOBBYR_FILTER_AMMO_SHOTGUN:
				guiCurrentAmmoFilterMode = GUN_SHOTGUN;
				break;
		}

		SetFirstLastPagesForNew(IC_AMMO, guiCurrentAmmoFilterMode);
		UpdateAmmoFilterButtons();

		DeleteMouseRegionForBigImage();
		fReDrawScreenFlag = TRUE;
		fPausedReDrawScreenFlag = TRUE;
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}

void BtnBobbyRFilterUsedCallback(GUI_BUTTON *btn,INT32 reason)
{
	UINT32		bNewValue;
	bNewValue = MSYS_GetBtnUserData( btn, 0 );

	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	//gbMessageDisplayed = TRUE;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

		guiPrevUsedFilterMode = guiCurrentUsedFilterMode;

		switch (bNewValue)
		{
			case BOBBYR_FILTER_USED_GUNS:
				guiCurrentUsedFilterMode = IC_BOBBY_GUN;
				break;
			case BOBBYR_FILTER_USED_ARMOR:
				guiCurrentUsedFilterMode = IC_ARMOUR;
				break;
			case BOBBYR_FILTER_USED_LBEGEAR:
				guiCurrentUsedFilterMode = IC_LBEGEAR;
				break;
			case BOBBYR_FILTER_USED_MISC:
				guiCurrentUsedFilterMode = IC_BOBBY_MISC;
				break;
		}

		UpdateUsedFilterButtons();
		SetFirstLastPagesForUsed(guiCurrentUsedFilterMode);

		DeleteMouseRegionForBigImage();
		fReDrawScreenFlag = TRUE;
		fPausedReDrawScreenFlag = TRUE;
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


void BtnBobbyRFilterArmourCallback(GUI_BUTTON *btn,INT32 reason)
{
	UINT32		bNewValue;
	bNewValue = MSYS_GetBtnUserData( btn, 0 );

	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;

		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

		guiPrevArmourFilterMode = guiCurrentArmourFilterMode;

		switch (bNewValue)
		{
			case BOBBYR_FILTER_ARMOUR_HELM:
				guiCurrentArmourFilterMode = ARMOURCLASS_HELMET;
				break;
			case BOBBYR_FILTER_ARMOUR_VEST:
				guiCurrentArmourFilterMode = ARMOURCLASS_VEST;
				break;
			case BOBBYR_FILTER_ARMOUR_LEGGING:
				guiCurrentArmourFilterMode = ARMOURCLASS_LEGGINGS;
				break;
			case BOBBYR_FILTER_ARMOUR_PLATE:
				guiCurrentArmourFilterMode = ARMOURCLASS_PLATE;
				break;
		}

		SetFirstLastPagesForNew(IC_ARMOUR, guiCurrentArmourFilterMode);

		UpdateArmourFilterButtons();

		DeleteMouseRegionForBigImage();
		fReDrawScreenFlag = TRUE;
		fPausedReDrawScreenFlag = TRUE;
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}

void BtnBobbyRFilterMiscCallback(GUI_BUTTON *btn,INT32 reason)
{
	UINT32		bNewValue;
	bNewValue = MSYS_GetBtnUserData( btn, 0 );

	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;

		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

		guiPrevMiscFilterMode = guiCurrentMiscFilterMode;
		guiPrevMiscSubFilterMode = guiCurrentMiscSubFilterMode;

		guiCurrentMiscSubFilterMode = -1;
		switch (bNewValue)
		{
			case BOBBYR_FILTER_MISC_BLADE:
				guiCurrentMiscFilterMode = IC_BLADE;
				break;
			case BOBBYR_FILTER_MISC_THROWING_KNIFE:
				guiCurrentMiscFilterMode = IC_THROWING_KNIFE;
				break;
			case BOBBYR_FILTER_MISC_PUNCH:
				guiCurrentMiscFilterMode = IC_PUNCH;
				break;
			case BOBBYR_FILTER_MISC_GRENADE:
				guiCurrentMiscFilterMode = IC_GRENADE;
				break;
			case BOBBYR_FILTER_MISC_BOMB:
				guiCurrentMiscFilterMode = IC_BOMB;
				break;
			case BOBBYR_FILTER_MISC_MEDKIT:
				guiCurrentMiscFilterMode = IC_MEDKIT;
				break;
			case BOBBYR_FILTER_MISC_KIT:
				guiCurrentMiscFilterMode = IC_KIT;
				break;
			case BOBBYR_FILTER_MISC_FACE:
				guiCurrentMiscFilterMode = IC_FACE;
				break;
			case BOBBYR_FILTER_MISC_LBEGEAR:
				guiCurrentMiscFilterMode = IC_LBEGEAR;
				break;
			case BOBBYR_FILTER_MISC_OPTICS_ATTACHMENTS: // Madd:  New BR filters
				guiCurrentMiscFilterMode = IC_MISC;
				guiCurrentMiscSubFilterMode = BR_MISC_FILTER_OPTICS;
				break;
			case BOBBYR_FILTER_MISC_SIDE_AND_BOTTOM_ATTACHMENTS:
				guiCurrentMiscFilterMode = IC_MISC;
				guiCurrentMiscSubFilterMode = BR_MISC_FILTER_SIDE_BOTTOM;
				break;
			case BOBBYR_FILTER_MISC_MUZZLE_ATTACHMENTS:
				guiCurrentMiscFilterMode = IC_MISC;
				guiCurrentMiscSubFilterMode = BR_MISC_FILTER_MUZZLE;
				break;
			case BOBBYR_FILTER_MISC_STOCK_ATTACHMENTS:
				guiCurrentMiscFilterMode = IC_MISC;
				guiCurrentMiscSubFilterMode = BR_MISC_FILTER_STOCK;
				break;
			case BOBBYR_FILTER_MISC_INTERNAL_ATTACHMENTS:
				guiCurrentMiscFilterMode = IC_MISC;
				guiCurrentMiscSubFilterMode = BR_MISC_FILTER_INTERNAL;
				break;
			case BOBBYR_FILTER_MISC_OTHER_ATTACHMENTS:
				guiCurrentMiscFilterMode = IC_MISC;
				guiCurrentMiscSubFilterMode = BR_MISC_FILTER_OTHER_ATTACHMENTS;
				break;
			case BOBBYR_FILTER_MISC_MISC:
				guiCurrentMiscFilterMode = IC_MISC;
				guiCurrentMiscSubFilterMode = BR_MISC_FILTER_NO_ATTACHMENTS;
				break;
		}

		SetFirstLastPagesForNew(IC_BOBBY_MISC, guiCurrentMiscFilterMode,guiCurrentMiscSubFilterMode);

		UpdateMiscFilterButtons();

		DeleteMouseRegionForBigImage();
		fReDrawScreenFlag = TRUE;
		fPausedReDrawScreenFlag = TRUE;
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}

void BtnBobbyRNextPreviousPageCallback(GUI_BUTTON *btn,INT32 reason)
{
	UINT32		bNewValue;

	bNewValue = MSYS_GetBtnUserData( btn, 0 );

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;

		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);


		///////////////////////////////////////////////////////////////////////////////
		// HEADROCK HAM 4: Added ability to scroll forward 5 pages instead of one.
		// This is done by holding down SHIFT while clicking.
		//

		//if previous screen
		if( bNewValue == BOBBYR_PREVIOUS_PAGE)
		{
			const std::size_t jump = _KeyDown(17) ? gubNumPages :
				(_KeyDown(16) ? 10 : 1);
			gubCurPage = static_cast<UINT16>(
				BobbyRayCatalogueModel::PreviousPage(
					gubCurPage, gubNumPages, jump));
		}
		//else next screen
		else
		{
			const std::size_t jump = _KeyDown(17) ? gubNumPages :
				(_KeyDown(16) ? 10 : 1);
			gubCurPage = static_cast<UINT16>(
				BobbyRayCatalogueModel::NextPage(
					gubCurPage, gubNumPages, jump));
		}


		DeleteMouseRegionForBigImage();

		fReDrawScreenFlag = TRUE;
		fPausedReDrawScreenFlag = TRUE;
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}

BOOLEAN DisplayItemInfo(UINT32 uiItemClass, INT32 iFilter, INT32 iSubFilter)
{
	UINT16	i;
	UINT8		ubCount=0;
	UINT16	PosY, usTextPosY;
	UINT16	usItemIndex;
	CHAR16	sPage[60];
	INT16		pItemNumbers[ BOBBYR_NUM_WEAPONS_ON_PAGE ];
	BOOLEAN		bAddItem = FALSE;

	PosY = BOBBYR_GRID_PIC_Y;
	usTextPosY = BOBBYR_ITEM_DESC_START_Y;
	gubCurPage = static_cast<UINT16>(
		BobbyRayCatalogueModel::NormalizePage(gubCurPage, gubNumPages));

	//Display the subtotal at the bottom of the screen
	auto subtotal{ std::wstring(BobbyRText[BOBBYR_GUNS_SUB_TOTAL]) + std::wstring(L" ") + FormatMoney(CalculateTotalPurchasePrice()) };
	DrawTextToScreen(subtotal.data(), BOBBYR_ORDER_SUBTOTAL_X, BOBBYR_ORDER_SUBTOTAL_Y, 0, BOBBYR_ORDER_TITLE_FONT, BOBBYR_ORDER_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED | TEXT_SHADOWED);

	//Buggler: Display the current page & total pages at the bottom of the screen
	swprintf(sPage, L"%u / %u",
		static_cast<unsigned>(BobbyRayCatalogueModel::DisplayPageNumber(
			gubCurPage, gubNumPages)),
		static_cast<unsigned>(gubNumPages));
	DrawTextToScreen(sPage, BOBBYR_ORDER_PAGE_X, BOBBYR_ORDER_PAGE_Y, 0, BOBBYR_ORDER_TITLE_FONT, BOBBYR_ORDER_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED | TEXT_SHADOWED);

	//Display the Used item disclaimer
	if( gfOnUsedPage )
	{
		DrawTextToScreen( BobbyRText[BOBBYR_GUNS_PERCENT_FUNCTIONAL], BOBBYR_PERCENT_FUNTCIONAL_X, BOBBYR_PERCENT_FUNTCIONAL_Y, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ORDER_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED | TEXT_SHADOWED);
	}

//	InitFirstAndLastGlobalIndex( uiItemClass );

	//if there are no items then return
	if( gusFirstItemIndex == BOBBYR_NO_ITEMS )
	{
		DeleteMouseRegionForBigImage();
		if( fExitingLaptopFlag )
			return( TRUE );

		DisableBobbyRButtons();

		if (gbMessageDisplayed == FALSE)
		{
			//Display a popup saying we are out of stock
			DoLapTopMessageBox( MSG_BOX_LAPTOP_DEFAULT, BobbyRText[ BOBBYR_NO_MORE_STOCK ], LAPTOP_SCREEN, MSG_BOX_FLAG_OK, OutOfStockMessageBoxCallBack );

			gbMessageDisplayed = TRUE;
		}
		return( TRUE );
	}


	if( uiItemClass == BOBBYR_USED_ITEMS )
		CalcFirstIndexForPage( LaptopSaveInfo.BobbyRayUsedInventory, uiItemClass );
	else
		CalcFirstIndexForPage( LaptopSaveInfo.BobbyRayInventory, uiItemClass );

	DisableBobbyRButtons();

	for(i=gusCurWeaponIndex;
		((i<=gusLastItemIndex) &&
			(ubCount < BobbyRayCatalogueModel::ItemsPerPage)); i++)
	{
		bAddItem = FALSE;

		if( uiItemClass == BOBBYR_USED_ITEMS )
		{
			//If there is not items in stock
			if( LaptopSaveInfo.BobbyRayUsedInventory[ i ].ubQtyOnHand == 0 )
				continue;

			usItemIndex = LaptopSaveInfo.BobbyRayUsedInventory[ i ].usItemIndex;
			gfOnUsedPage = TRUE;
		}
		else
		{
			//If there is not items in stock
			if( LaptopSaveInfo.BobbyRayInventory[ i ].ubQtyOnHand == 0 )
				continue;

			usItemIndex = LaptopSaveInfo.BobbyRayInventory[ i ].usItemIndex;
			gfOnUsedPage = FALSE;
		}
		if (!IsLoadedBobbyRayItem(usItemIndex))
			continue;

		// skip items that aren't of the right item class
		if( ! ( Item[ usItemIndex ].usItemClass & uiItemClass ) )
		{
			continue;
		}

		// No Filter->Take all
		if (iFilter == -1)
		{
			bAddItem = TRUE;
		}

		pItemNumbers[ ubCount ] = usItemIndex;

		switch( Item[ usItemIndex ].usItemClass )
		{
			case IC_GUN:
			case IC_LAUNCHER:
				// Used
				if (uiItemClass == BOBBYR_USED_ITEMS)
				{
					if (iFilter > -1)
					{
						if (Item[usItemIndex].usItemClass == IC_GUN ||
							Item[usItemIndex].usItemClass == IC_LAUNCHER)
						{
							bAddItem = TRUE;
						}
					}
				}
				// Guns
				else
				{
					if (iFilter > -1)
					{
						if (Weapon[ Item[usItemIndex].ubClassIndex ].ubWeaponType == iFilter)
						{
							bAddItem = TRUE;
						}
					}
				}

				if (bAddItem == TRUE)
				{
					gusItemNumberForItemsOnScreen[ ubCount ] = i;

					DisplayBigItemImage( usItemIndex, PosY);

					//Display Items Name
					DisplayItemNameAndInfo(usTextPosY, usItemIndex, i, gfOnUsedPage);

					DisplayGunInfo(usItemIndex, usTextPosY, gfOnUsedPage, i);

					PosY += BOBBYR_GRID_OFFSET;
					usTextPosY += BOBBYR_GRID_OFFSET;
					ubCount++;
				}
				break;

			case IC_AMMO:
				// USED
				if (uiItemClass == BOBBYR_USED_ITEMS)
				{
					if (iFilter > -1)
					{
						if (Item[usItemIndex].usItemClass == iFilter)
						{
							bAddItem = TRUE;
						}
					}
				}
				// AMMO
				else
				{
					if (iFilter > -1)
					{
						if (IsAmmoMatchinWeaponType(usItemIndex, iFilter))
						{
							bAddItem = TRUE;
						}
					}
				}

				if (bAddItem == TRUE)
				{
					gusItemNumberForItemsOnScreen[ ubCount ] = i;

					DisplayBigItemImage( usItemIndex, PosY);

					//Display Items Name
					DisplayItemNameAndInfo(usTextPosY, usItemIndex, i, gfOnUsedPage);

					DisplayAmmoInfo( usItemIndex, usTextPosY, gfOnUsedPage, i);

					PosY += BOBBYR_GRID_OFFSET;
					usTextPosY += BOBBYR_GRID_OFFSET;
					ubCount++;
				}
				break;

			case IC_ARMOUR:
				// USED
				if (uiItemClass == BOBBYR_USED_ITEMS)
				{
					if (iFilter > -1)
					{
						if (Item[usItemIndex].usItemClass == iFilter)
						{
							bAddItem = TRUE;
						}
					}
				}
				// ARMOUR
				else
				{
					if (iFilter > -1)
					{
						if (Armour[ Item[usItemIndex].ubClassIndex ].ubArmourClass == iFilter)
						{
							bAddItem = TRUE;
						}
					}
				}

				if (bAddItem == TRUE)
				{
					gusItemNumberForItemsOnScreen[ ubCount ] = i;

					DisplayBigItemImage( usItemIndex, PosY);

					//Display Items Name
					DisplayItemNameAndInfo(usTextPosY, usItemIndex, i, gfOnUsedPage);

					DisplayArmourInfo( usItemIndex, usTextPosY, gfOnUsedPage, i);

					PosY += BOBBYR_GRID_OFFSET;
					usTextPosY += BOBBYR_GRID_OFFSET;
					ubCount++;
				}
				break;

			case IC_BLADE:
			case IC_THROWING_KNIFE:
			case IC_PUNCH:
				// USED
				if (uiItemClass == BOBBYR_USED_ITEMS)
				{
					if (iFilter > -1)
					{
						if (Item[usItemIndex].usItemClass == IC_BLADE ||
							Item[usItemIndex].usItemClass == IC_THROWING_KNIFE ||
							Item[usItemIndex].usItemClass == IC_PUNCH)
						{
							bAddItem = TRUE;
						}
					}
				}
				// MISC
				else
				{
					if (iFilter > -1)
					{
						if (Item[usItemIndex].usItemClass == iFilter)
						{
							bAddItem = true;
						}
					}
				}

				if (bAddItem == TRUE)
				{
					gusItemNumberForItemsOnScreen[ ubCount ] = i;

					DisplayBigItemImage( usItemIndex, PosY);

					//Display Items Name
					DisplayItemNameAndInfo(usTextPosY, usItemIndex, i, gfOnUsedPage);

					DisplayNonGunWeaponInfo(usItemIndex, usTextPosY, gfOnUsedPage, i);

					PosY += BOBBYR_GRID_OFFSET;
					usTextPosY += BOBBYR_GRID_OFFSET;
					ubCount++;
				}
				break;

			case IC_GRENADE:
			case IC_BOMB:
				// USED
				if (uiItemClass == BOBBYR_USED_ITEMS)
				{
					if (iFilter > -1)
					{
						if (Item[usItemIndex].usItemClass == IC_GRENADE ||
							Item[usItemIndex].usItemClass == IC_BOMB)
						{
							bAddItem = TRUE;
						}
					}
				}
				// MISC
				else
				{
					if (iFilter > -1)
					{
						if (Item[usItemIndex].usItemClass == iFilter)
						{
							bAddItem = true;
						}
					}
				}

				if (bAddItem == TRUE)
				{
					gusItemNumberForItemsOnScreen[ ubCount ] = i;

					DisplayBigItemImage( usItemIndex, PosY);

					//Display Items Name
					DisplayItemNameAndInfo(usTextPosY, usItemIndex, i, gfOnUsedPage);

					DisplayGrenadeBombInfo(usItemIndex, usTextPosY, gfOnUsedPage, i);

					PosY += BOBBYR_GRID_OFFSET;
					usTextPosY += BOBBYR_GRID_OFFSET;
					ubCount++;
				}
				break;
			case IC_MISC:
			case IC_MEDKIT:
			case IC_KIT:
			case IC_FACE:
			case IC_LBEGEAR:
				// USED
				if (uiItemClass == BOBBYR_USED_ITEMS)
				{
					if (iFilter > -1)
					{
						if (Item[usItemIndex].usItemClass == IC_MISC ||
							Item[usItemIndex].usItemClass == IC_MEDKIT ||
							Item[usItemIndex].usItemClass == IC_KIT ||
							Item[usItemIndex].usItemClass == IC_LBEGEAR ||
							Item[usItemIndex].usItemClass == IC_FACE)
						{
							bAddItem = TRUE;
						}
					}
				}
				// MISC
				else
				{
					if (iFilter > -1 && Item[usItemIndex].usItemClass == iFilter)
					{
						if ( iSubFilter > -1 ) // Madd: new BR filters
						{
							if (ItemIsAttachment(usItemIndex) && Item[usItemIndex].attachmentclass & iSubFilter )
								bAddItem = TRUE;
							else if (iSubFilter == BR_MISC_FILTER_OTHER_ATTACHMENTS && !(Item[usItemIndex].attachmentclass & BR_MISC_FILTER_STD_ATTACHMENTS) && ItemIsAttachment(usItemIndex))
								bAddItem = TRUE;
							else if (iSubFilter == BR_MISC_FILTER_NO_ATTACHMENTS && !ItemIsAttachment(usItemIndex))
								bAddItem = TRUE;
						}
						else
							bAddItem = TRUE;
					}
				}

				if (bAddItem == TRUE)
				{
					gusItemNumberForItemsOnScreen[ ubCount ] = i;

					DisplayBigItemImage( usItemIndex, PosY);

					//Display Items Name
					DisplayItemNameAndInfo(usTextPosY, usItemIndex, i, gfOnUsedPage);

					DisplayMiscInfo( usItemIndex, usTextPosY, gfOnUsedPage, i);

					PosY += BOBBYR_GRID_OFFSET;
					usTextPosY += BOBBYR_GRID_OFFSET;
					ubCount++;
				}
				break;
		}
	}

	if( gusOldItemNumOnTopOfPage != gusCurWeaponIndex ||
		gubNumItemsOnScreen != ubCount )
	{
		DeleteMouseRegionForBigImage();
		CreateMouseRegionForBigImage(BOBBYR_GRID_PIC_Y, ubCount, pItemNumbers );

		gusOldItemNumOnTopOfPage = gusCurWeaponIndex;
	}

	return(TRUE);
}


BOOLEAN DisplayGunInfo(UINT16 usIndex, UINT16 usTextPosY, BOOLEAN fUsed, UINT16 usBobbyIndex)
{
	UINT16	usHeight;
	UINT16 usFontHeight;
	usFontHeight = GetFontHeight(BOBBYR_ITEM_DESC_TEXT_FONT);

	//Display Items Name
	//DisplayItemNameAndInfo(usTextPosY, usIndex, fUsed);

	usHeight = usTextPosY;
	//Display the weight, caliber, mag, rng, dam, rof text

	//Weight - Removed due to limited space
	//usHeight = DisplayWeight(usHeight, usIndex, usFontHeight);

	//Caliber
	usHeight = DisplayCaliber(usHeight, usIndex, usFontHeight);

	//Magazine
	usHeight = DisplayMagazine(usHeight, usIndex, usFontHeight);

	//Damage
	usHeight = DisplayDamage(usHeight, usIndex, usFontHeight);

	//Range
	usHeight = DisplayRange(usHeight, usIndex, usFontHeight);

	//ROF
	usHeight = DisplayRof(usHeight, usIndex, usFontHeight);

	//AP
	DisplayGunAP(usHeight, usIndex, usFontHeight);

	//Display the Cost and the qty bought and on hand
	DisplayCostAndQty(usTextPosY, usIndex, usFontHeight, usBobbyIndex, fUsed);

	return(TRUE);
} //DisplayGunInfo

BOOLEAN DisplayNonGunWeaponInfo(UINT16 usIndex, UINT16 usTextPosY, BOOLEAN fUsed, UINT16 usBobbyIndex)
{
	UINT16	usHeight;
	UINT16 usFontHeight;
	usFontHeight = GetFontHeight(BOBBYR_ITEM_DESC_TEXT_FONT);

	//Display Items Name
//	DisplayItemNameAndInfo(usTextPosY, usIndex, fUsed);

	usHeight = usTextPosY;
	//Display the weight, caliber, mag, rng, dam, rof text

	//Weight
	usHeight = DisplayWeight(usHeight, usIndex, usFontHeight);

	//Damage
	usHeight = DisplayDamage(usHeight, usIndex, usFontHeight);

	//AP
	DisplayMeleeAP(usHeight, usIndex, usFontHeight);

	//Display the Cost and the qty bought and on hand
	DisplayCostAndQty(usTextPosY, usIndex, usFontHeight, usBobbyIndex, fUsed);

	return(TRUE);
} //DisplayNonGunWeaponInfo



BOOLEAN DisplayAmmoInfo(UINT16 usIndex, UINT16 usTextPosY, BOOLEAN fUsed, UINT16 usBobbyIndex)
{
	UINT16	usHeight;
	UINT16 usFontHeight;
	usFontHeight = GetFontHeight(BOBBYR_ITEM_DESC_TEXT_FONT);

	//Display Items Name
//	DisplayItemNameAndInfo(usTextPosY, usIndex, fUsed);

	usHeight = usTextPosY;
	//Display the weight, caliber, mag, rng, dam, rof text

	//Caliber
	usHeight = DisplayCaliber(usHeight, usIndex, usFontHeight);

	//Magazine
	usHeight = DisplayMagazine(usHeight, usIndex, usFontHeight);

	// rftr: display ammo stats
	usHeight = DisplayAmmoArmourPierceModifier(usHeight, usIndex, usFontHeight);
	usHeight = DisplayAmmoDamageModifier(usHeight, usIndex, usFontHeight);
	DisplayAmmoProjectileCount(usHeight, usIndex, usFontHeight);

	//Display the Cost and the qty bought and on hand
	DisplayCostAndQty(usTextPosY, usIndex, usFontHeight, usBobbyIndex, fUsed);

	return(TRUE);
} //DisplayAmmoInfo


BOOLEAN DisplayGrenadeBombInfo(UINT16 usIndex, UINT16 usTextPosY, BOOLEAN fUsed, UINT16 usBobbyIndex)
{
	UINT16	usHeight;
	UINT16 usFontHeight;
	usFontHeight = GetFontHeight(BOBBYR_ITEM_DESC_TEXT_FONT);

	//Display Items Name
//	DisplayItemNameAndInfo(usTextPosY, usIndex, fUsed);

	usHeight = usTextPosY;
	//Display the weight, caliber, mag, rng, dam, rof text

	//Weight
	usHeight = DisplayWeight(usHeight, usIndex, usFontHeight);

	//Explosive Damage
	usHeight = DisplayExplosiveDamage(usHeight, usIndex, usFontHeight);

	//Explosive Stun Damage
	DisplayExplosiveStunDamage(usHeight, usIndex, usFontHeight);

	//Display the Cost and the qty bought and on hand
	DisplayCostAndQty(usTextPosY, usIndex, usFontHeight, usBobbyIndex, fUsed);

	return(TRUE);
} //DisplayAmmoInfo


BOOLEAN DisplayBigItemImage(UINT16 usIndex, UINT16 PosY)
{
	INT16		PosX, sCenX, sCenY;
	UINT32		usWidth = 0;
	INT32		sOffsetX = 0;
	ETRLEObject	*pTrav;
	INVTYPE		*pItem;
	UINT32		uiImage = 0;
	HVOBJECT	hPixHandle = nullptr;

	PosX = BOBBYR_GRID_PIC_X;

	if (!IsLoadedBobbyRayItem(usIndex))
		return FALSE;
	pItem = &Item[ usIndex ];
	if (!LoadTileGraphicForItem(pItem, &uiImage))
		return FALSE;
	UniqueVideoObjectHandle itemImage(uiImage);

	if (!GetVideoObject(&hPixHandle, uiImage) || !hPixHandle)
	{
		SET_ERROR("Error: Could not load item uiIndex %d graphics. ubGraphicType %d and ubGraphicNum %d", usIndex, pItem->ubGraphicType, pItem->ubGraphicNum);
		AssertMsg(false, gubErrorText);
		return FALSE;
	}

	if(hPixHandle->ubBitDepth == 8 && hPixHandle->pETRLEObject)
	{
		pTrav = &(hPixHandle->pETRLEObject[ 0 ] );
	
		//center picture in frame
		usWidth					= (UINT32)pTrav->usWidth;
		sOffsetX				= (INT32)pTrav->sOffsetX;
	}
	else if(hPixHandle->ubBitDepth == 16 && hPixHandle->p16BPPObject)
	{
		usWidth					= hPixHandle->p16BPPObject->usWidth;
		sOffsetX				= hPixHandle->p16BPPObject->sOffsetX;
	}
	else if(hPixHandle->ubBitDepth == 32 && hPixHandle->p16BPPObject)
	{
		usWidth					= hPixHandle->p16BPPObject->usWidth;
		sOffsetX				= hPixHandle->p16BPPObject->sOffsetX;
	}
    else
    {
        Assert(false);
		return FALSE;
    }

//	sCenX = PosX + ( abs( BOBBYR_GRID_PIC_WIDTH - usWidth ) / 2 );
//	sCenY = PosY + 8;
	sCenX = PosX + ( abs( BOBBYR_GRID_PIC_WIDTH - (int)usWidth ) / 2 ) - sOffsetX;
	sCenY = PosY + 8;


	//blt the shadow of the item
	if(gGameSettings.fOptions[ TOPTION_SHOW_ITEM_SHADOW ])
	{
		BltVideoObjectOutlineShadowFromIndex( FRAME_BUFFER, uiImage, 0, sCenX-2, sCenY+2);//pItem->ubGraphicNum
	}
	BltVideoObject(FRAME_BUFFER, hPixHandle, 0, sCenX, sCenY, VO_BLT_SRCTRANSPARENCY,NULL);

	return(TRUE);
}


BOOLEAN DisplayArmourInfo(UINT16 usIndex, UINT16 usTextPosY, BOOLEAN fUsed, UINT16 usBobbyIndex)
{
	UINT16	usHeight;
	UINT16 usFontHeight;
	usFontHeight = GetFontHeight(BOBBYR_ITEM_DESC_TEXT_FONT);

	//Display Items Name
//	DisplayItemNameAndInfo(usTextPosY, usIndex, fUsed);

	usHeight = usTextPosY;
	//Display the weight, caliber, mag, rng, dam, rof text

	//Weight
	usHeight = DisplayWeight(usHeight, usIndex, usFontHeight);

	//Protection
	usHeight = DisplayProtection(usHeight, usIndex, usFontHeight);

	//Camo
	DisplayCamo(usHeight, usIndex, usFontHeight);

	//Display the Cost and the qty bought and on hand
	DisplayCostAndQty(usTextPosY, usIndex, usFontHeight, usBobbyIndex, fUsed);

	return(TRUE);
} //DisplayArmourInfo


BOOLEAN DisplayMiscInfo(UINT16 usIndex, UINT16 usTextPosY, BOOLEAN fUsed, UINT16 usBobbyIndex)
{
	UINT16 usFontHeight;
	usFontHeight = GetFontHeight(BOBBYR_ITEM_DESC_TEXT_FONT);

	//CHRISL: Display extra information for LBE Items when using new inventory system
	if((UsingNewInventorySystem() == true) && Item[usIndex].usItemClass == IC_LBEGEAR)
	{
		DisplayLBEInfo(usTextPosY, usIndex, usFontHeight);
	}

	//Display Items Name
//	DisplayItemNameAndInfo(usTextPosY, usIndex, fUsed);

	//Display the Cost and the qty bought and on hand
	DisplayCostAndQty(usTextPosY, usIndex, usFontHeight, usBobbyIndex, fUsed);

	return(TRUE);
} //DisplayMiscInfo



UINT16 DisplayCostAndQty(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight, UINT16 usBobbyIndex, BOOLEAN fUsed)
{
	CHAR16	sTemp[20];
//	UINT8	ubPurchaseNumber;

	//
	//Display the cost and the qty
	//

	//Display the cost
	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_COST], BOBBYR_ITEM_COST_TEXT_X, (UINT16)usPosY, BOBBYR_ITEM_COST_TEXT_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usPosY += usFontHeight + 2;

	DrawTextToScreen(FormatMoney(CalcBobbyRayCost(usIndex, usBobbyIndex, fUsed)).data(), BOBBYR_ITEM_COST_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_COST_TEXT_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;



/*
	//Display the # bought
	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_QTY_ON_ORDER], BOBBYR_ITEM_QTY_TEXT_X, (UINT16)usPosY, BOBBYR_ITEM_COST_TEXT_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usPosY += usFontHeight + 2;

	ubPurchaseNumber = CheckIfItemIsPurchased(usBobbyIndex);
	if( ubPurchaseNumber == BOBBY_RAY_NOT_PURCHASED)
		swprintf(sTemp, L"% 4d", 0);
	else
		swprintf(sTemp, L"% 4d", BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased);

	DrawTextToScreen(sTemp, BOBBYR_ITEMS_BOUGHT_X, (UINT16)usPosY, BOBBYR_ITEM_COST_TEXT_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
*/

	//Display Weight Number
	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_WGHT], BOBBYR_ITEM_STOCK_TEXT_X, (UINT16)(usPosY), BOBBYR_ITEM_COST_TEXT_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usPosY += usFontHeight + 2;


	swprintf( sTemp, L"%3.2f", GetWeightBasedOnMetricOption( Item[ usIndex ].ubWeight ) / (FLOAT)( 10.0 ) );
	DrawTextToScreen(sTemp, BOBBYR_ITEM_STOCK_TEXT_X, (UINT16)(usPosY), BOBBYR_ITEM_COST_TEXT_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;


	//Display the # In Stock
	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_IN_STOCK], BOBBYR_ITEM_STOCK_TEXT_X, (UINT16)usPosY, BOBBYR_ITEM_COST_TEXT_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	usPosY += usFontHeight + 2;

	if( fUsed )
		swprintf(sTemp, L"% 4d", LaptopSaveInfo.BobbyRayUsedInventory[ usBobbyIndex ].ubQtyOnHand);
	else
		swprintf(sTemp, L"% 4d", LaptopSaveInfo.BobbyRayInventory[ usBobbyIndex ].ubQtyOnHand);

	DrawTextToScreen(sTemp, BOBBYR_ITEM_STOCK_TEXT_X, (UINT16)usPosY, BOBBYR_ITEM_COST_TEXT_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;


	return(usPosY);
}

UINT16 DisplayRof(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16	sTemp[20];

	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_ROF], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	if( WeaponROF[ usIndex ] == -1 )
		swprintf(sTemp, L"? %s", pMessageStrings[ MSG_RPM ] );
	else
		swprintf(sTemp, L"%3d/%s", WeaponROF[ usIndex ], pMessageStrings[ MSG_MINUTE_ABBREVIATION ]);


	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;
	return(usPosY);
}

UINT16 DisplayGunAP(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16 sTemp[20];
	CHAR16 sTemp2[20];
	OBJECTTYPE pObject;

	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_AP], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	
	CreateItem(usIndex, 100, &pObject);
	UINT16		readyAPs = (UINT16)(( Weapon[ usIndex ].ubReadyTime * (100 - Item[ usIndex ].percentreadytimeapreduction)) / 100);
	INT16		ubAttackAPs = BaseAPsToShootOrStab( APBPConstants[DEFAULT_APS], APBPConstants[DEFAULT_AIMSKILL], &pObject, NULL );
	
	swprintf( sTemp, L"(%d)", readyAPs );

	if ( Weapon[ usIndex ].NoSemiAuto )
		wcscat( sTemp, L"-" );
	else
	{
		swprintf( sTemp2, L"%d", ubAttackAPs );
		wcscat( sTemp, sTemp2 );
	}

	if (GetShotsPerBurst(&pObject) > 0)
	{
		swprintf( sTemp2, L"/%d", ubAttackAPs + CalcAPsToBurst( APBPConstants[DEFAULT_APS], &pObject, NULL ) );
		wcscat( sTemp, sTemp2 );
	}
	else
		wcscat( sTemp, L"/-" );

	if (GetAutofireShotsPerFiveAPs(&pObject) > 0)
	{
		swprintf( sTemp2, L"/%d", ubAttackAPs + CalcAPsToAutofire( APBPConstants[DEFAULT_APS], &pObject, 3, NULL ) );
		wcscat( sTemp, sTemp2 );
	}
	else
		wcscat( sTemp, L"/-" );

	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR_ALT, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;
	return(usPosY);
}

UINT16 DisplayMeleeAP(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16 sTemp[20];
	OBJECTTYPE pObject;

	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_AP], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	
	CreateItem(usIndex, 100, &pObject);
	INT16		ubAttackAPs = BaseAPsToShootOrStab( APBPConstants[DEFAULT_APS], APBPConstants[DEFAULT_AIMSKILL], &pObject, NULL );

	swprintf( sTemp, L"%d", ubAttackAPs );
	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR_ALT, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;
	return(usPosY);
}

UINT16 DisplayDamage(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16	sTemp[20];
	UINT16 gunDamage = 0;

	if ( Item[ usIndex ].usItemClass == IC_GUN || Item[ usIndex ].usItemClass == IC_LAUNCHER )
	{
		// HEADROCK HAM 3.6: Can now take a negative modifier
		gunDamage = (UINT16)GetModifiedGunDamage( Weapon[ usIndex ].ubImpact );
		//gunDamage = (UINT16)( Weapon[ usIndex ].ubImpact + ( (double) Weapon[ usIndex ].ubImpact / 100) * gGameExternalOptions.ubGunDamageMultiplier );

		// modify by ini values
		if ( Item[ usIndex ].usItemClass == IC_GUN )
			gunDamage *= gItemSettings.fDamageModifierGun[ Weapon[ usIndex ].ubWeaponType ];
	}
	else
	{
		// HEADROCK HAM 3.6: Can now take a negative modifier
		gunDamage = (UINT16)GetModifiedMeleeDamage( Weapon[ usIndex ].ubImpact );
		//gunDamage = (UINT16)( Weapon[ usIndex ].ubImpact + ( (double) Weapon[ usIndex ].ubImpact / 100) * gGameExternalOptions.ubMeleeDamageMultiplier );

		// modify by ini values
		if ( Item[ usIndex ].usItemClass == IC_BLADE )
			gunDamage *= gItemSettings.fDamageModifierBlade;
		else if ( Item[ usIndex ].usItemClass == IC_PUNCH )
			gunDamage *= gItemSettings.fDamageModifierPunch;
	}

	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_DAMAGE], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	swprintf(sTemp, L"%4d", gunDamage);
	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;
	return(usPosY);
}


UINT16 DisplayRange(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16	sTemp[20];
	UINT16  gunRange = 0;

	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_RANGE], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	gunRange = (UINT16)GetModifiedGunRange( usIndex);

	swprintf(sTemp, L"%3d %s", gunRange, pMessageStrings[ MSG_METER_ABBREVIATION ] );
	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR_ALT, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;
	return(usPosY);
}

UINT16 DisplayMagazine(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16	sTemp[20];

	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_MAGAZINE], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	if(Item[usIndex].usItemClass & IC_BOBBY_GUN){
		swprintf(sTemp, L"%3d %s", Weapon[Item[usIndex].ubClassIndex].ubMagSize, pMessageStrings[ MSG_ROUNDS_ABBREVIATION ] );
	}else{
		swprintf(sTemp, L"%3d %s", Magazine[Item[usIndex].ubClassIndex].ubMagSize, pMessageStrings[ MSG_ROUNDS_ABBREVIATION ] );
	}
	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR_ALT, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;
	return(usPosY);
}


UINT16 DisplayCaliber(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16	zTemp[128];
	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_CALIBRE], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	//	if ammo is begin drawn
	if( Item[ usIndex].usItemClass == IC_AMMO )
	{
		swprintf( zTemp, L"%s", BobbyRayAmmoCaliber[ Magazine[ Item[ usIndex ].ubClassIndex ].ubCalibre] );
//		DrawTextToScreen( AmmoCaliber[ Magazine[ Item[ usIndex ].ubClassIndex ].ubCalibre], BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	}
	else
	{
		//else a gun is being displayed
		swprintf( zTemp, L"%s", BobbyRayAmmoCaliber[ Weapon[ Item[ usIndex ].ubClassIndex ].ubCalibre ] );
//		DrawTextToScreen( AmmoCaliber[ Weapon[ Item[ usIndex ].ubClassIndex ].ubCalibre ], BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	}


	if( StringPixLength( zTemp, BOBBYR_ITEM_DESC_TEXT_FONT ) > BOBBYR_ITEM_WEIGHT_NUM_WIDTH )
		ReduceStringLength( zTemp, BOBBYR_GRID_PIC_WIDTH, BOBBYR_ITEM_NAME_TEXT_FONT );

	DrawTextToScreen( zTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);


	usPosY += usFontHeight + 2;
	return(usPosY);
}

UINT16 DisplayExplosiveDamage(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16	sTemp[20];

	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_DAMAGE], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	
	UINT16 explDamage = (UINT16) GetModifiedExplosiveDamage( Explosive[Item[ usIndex ].ubClassIndex].ubDamage, 0 );
	
	swprintf(sTemp, L"%4d", explDamage);
	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;
	return(usPosY);
}

UINT16 DisplayExplosiveStunDamage(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16	sTemp[20];

	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_STUN], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	
	UINT16 explStunDamage = (UINT16) GetModifiedExplosiveDamage( Explosive[Item[ usIndex ].ubClassIndex].ubStunDamage, 1 );
	
	swprintf(sTemp, L"%4d", explStunDamage);
	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR_ALT, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;
	return(usPosY);
}

UINT16 DisplayProtection(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16	sTemp[20];
	CHAR16	sTemp2[20];

	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_PROTECTION], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	
	INT32 iProtection = Armour[ Item[ usIndex ].ubClassIndex ].ubProtection;

	switch( Armour[ Item[ usIndex ].ubClassIndex ].ubArmourClass )
	{
	case( ARMOURCLASS_HELMET ):
		iProtection = 15 * iProtection / Armour[ Item[ SPECTRA_HELMET_18 ].ubClassIndex ].ubProtection;
		break;

	case( ARMOURCLASS_VEST ):
		iProtection = 65 * iProtection / ( Armour[ Item[ SPECTRA_VEST_18 ].ubClassIndex ].ubProtection + Armour[ Item[ CERAMIC_PLATES ].ubClassIndex ].ubProtection );
		break;

	case( ARMOURCLASS_LEGGINGS ):
		iProtection = 25 * iProtection / Armour[ Item[ SPECTRA_LEGGINGS_18 ].ubClassIndex ].ubProtection;
		break;

	case( ARMOURCLASS_PLATE ):
		iProtection = 65 * iProtection / ( Armour[ Item[ CERAMIC_PLATES ].ubClassIndex ].ubProtection );
		break;
	}
	
	swprintf(sTemp, L"%d", iProtection);
	wcscat( sTemp, L"%%" );
	swprintf( sTemp2, L"(%d)", Armour[ Item[ usIndex ].ubClassIndex ].ubProtection );
	wcscat( sTemp, sTemp2 );
	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;
	return(usPosY);
}

UINT16 DisplayCamo(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16 sTemp[20];

	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_CAMO], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	
	swprintf(sTemp, L"%d", Item[ usIndex ].camobonus);
	wcscat( sTemp, L"%%" );
	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR_ALT, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;
	return(usPosY);
}

UINT16 DisplayAmmoArmourPierceModifier(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16 sTemp[10];

	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_ARMOUR_PIERCING_MODIFIER], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	// armour piercing modifier
	const FLOAT armourImpactReductionModifier = (FLOAT)AmmoTypes[Magazine[Item[usIndex].ubClassIndex].ubAmmoType].armourImpactReductionMultiplier / (FLOAT)AmmoTypes[Magazine[Item[usIndex].ubClassIndex].ubAmmoType].armourImpactReductionDivisor;
	swprintf(sTemp, L"%1.2f", armourImpactReductionModifier);
	UINT8 color = BOBBYR_ITEM_DESC_TEXT_COLOR_ALT;
	if (armourImpactReductionModifier > 1.0f)
		color = FONT_MCOLOR_DKRED;
	else if (armourImpactReductionModifier < 1.0f)
		color = FONT_MCOLOR_LTGREEN;
	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, color, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);

	usPosY += usFontHeight + 2;
	return(usPosY);
}

UINT16 DisplayAmmoDamageModifier(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16 sTemp[10];

	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_BULLET_TUMBLE_MODIFIER], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	// body damage modifier (bullet tumble)
	const FLOAT afterArmourDamageModifier = (FLOAT)AmmoTypes[Magazine[Item[usIndex].ubClassIndex].ubAmmoType].afterArmourDamageMultiplier / (FLOAT)AmmoTypes[Magazine[Item[usIndex].ubClassIndex].ubAmmoType].afterArmourDamageDivisor;
	swprintf(sTemp, L"%1.2f", afterArmourDamageModifier);
	UINT8 color = BOBBYR_ITEM_DESC_TEXT_COLOR_ALT;
	if (afterArmourDamageModifier > 1.0f)
		color = FONT_MCOLOR_LTGREEN;
	else if (afterArmourDamageModifier < 1.0f)
		color = FONT_MCOLOR_DKRED;
	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, color, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);

	usPosY += usFontHeight + 2;
	return(usPosY);
}

UINT16 DisplayAmmoProjectileCount(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	const UINT8 numProjectiles = AmmoTypes[Magazine[Item[usIndex].ubClassIndex].ubAmmoType].numberOfBullets;
	if (numProjectiles == 1)
		return(usPosY);

	CHAR16 sTemp[4];
	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_NUM_PROJECTILES], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	swprintf(sTemp, L"%d", numProjectiles);
	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR_ALT, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);

	usPosY += usFontHeight + 2;
	return(usPosY);
}

// CHRISL: New display function for LBE Gear
UINT16 DisplayLBEInfo(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16				sTemp[20];
	CHAR16				pName[80];
	int					lnCnt=0;
	if (!IsLoadedBobbyRayItem(usIndex))
		return usPosY;

	const std::size_t lbeIndex = Item[usIndex].ubClassIndex;
	if (!BobbyRayCatalogueModel::IsCatalogueIndexInBounds(
			lbeIndex, LoadBearingEquipment.size()))
	{
		return usPosY;
	}
	std::vector<int> pocketNum(LBEPocketType.size(), 0);

	// Populate "Number" for each type of pocket this LBE item has
	const std::size_t pocketSlotCount = std::min<std::size_t>(
		MAX_ITEMS_IN_LBE, LoadBearingEquipment[lbeIndex].lbePocketIndex.size());
	for(std::size_t pocketSlot = 0; pocketSlot < pocketSlotCount; ++pocketSlot)
	{
		const std::size_t pIndex =
			LoadBearingEquipment[lbeIndex].lbePocketIndex[pocketSlot];
		if (BobbyRayCatalogueModel::IsCatalogueIndexInBounds(
				pIndex, pocketNum.size()))
		{
			pocketNum[pIndex]++;
		}
	}
	// Go through and display the pocket type and number
	for(std::size_t count = 1; count < pocketNum.size(); ++count)
	{
		if(pocketNum[count]>0)
		{
			if(lnCnt>4)
			{
				swprintf(sTemp, L"More..." );
				DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
				usPosY += usFontHeight + 2;
				break;
			}
			else
			{
				//mbstowcs(pName,LBEPocketType[count].pName,80);
				for(UINT8 i = 0; i<14; i++)
					pName[i] = LBEPocketType[count].pName[i];
				//pName = LBEPocketType[count].pName;
				pName[14] = '\0';
				swprintf(sTemp, L"%s(x%d)", pName, pocketNum[count] );
				DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
				usPosY += usFontHeight + 2;
				lnCnt++;
			}
		}
	}
	return(usPosY);
}


UINT16 DisplayWeight(UINT16 usPosY, UINT16 usIndex, UINT16 usFontHeight)
{
	CHAR16	sTemp[20];

	//display the 'weight' string
	DrawTextToScreen(BobbyRText[BOBBYR_GUNS_WEIGHT], BOBBYR_ITEM_WEIGHT_TEXT_X, (UINT16)usPosY, 0, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	swprintf(sTemp, L"%3.2f %s", GetWeightBasedOnMetricOption(Item[ usIndex ].ubWeight)/10, GetWeightUnitString() );
	DrawTextToScreen(sTemp, BOBBYR_ITEM_WEIGHT_NUM_X, (UINT16)usPosY, BOBBYR_ITEM_WEIGHT_NUM_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR_ALT, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	usPosY += usFontHeight + 2;
	return(usPosY);
}


void DisplayItemNameAndInfo(UINT16 usPosY, UINT16 usIndex, UINT16 usBobbyIndex, BOOLEAN fUsed)
{
	CHAR16	sText[400];
	CHAR16	sTemp[20];

	UINT8	ubPurchaseNumber;

	LoadBRName(usIndex,sText);

	if( StringPixLength( sText, BOBBYR_ITEM_NAME_TEXT_FONT ) > ( BOBBYR_GRID_PIC_WIDTH - 6 ) )
		ReduceStringLength( sText, BOBBYR_GRID_PIC_WIDTH - 6, BOBBYR_ITEM_NAME_TEXT_FONT );

	DrawTextToScreen(sText, BOBBYR_ITEM_NAME_X, (UINT16)(usPosY+BOBBYR_ITEM_NAME_Y_OFFSET), 0, BOBBYR_ITEM_NAME_TEXT_FONT, BOBBYR_ITEM_NAME_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	//number bought
	//Display the # bought
	ubPurchaseNumber = CheckIfItemIsPurchased(usBobbyIndex);
	if( ubPurchaseNumber != BOBBY_RAY_NOT_PURCHASED)
	{
		DrawTextToScreen(BobbyRText[BOBBYR_GUNS_QTY_ON_ORDER], BOBBYR_ITEM_QTY_TEXT_X, (UINT16)usPosY, BOBBYR_ITEM_QTY_WIDTH, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_STATIC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);

		if( ubPurchaseNumber != BOBBY_RAY_NOT_PURCHASED)
		{
			swprintf(sTemp, L"% 4d", BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased);
			auto bobbyRItemsBoughtX{ BOBBYR_ITEMS_BOUGHT_X };
			if (g_lang == i18n::Lang::zh) {
				bobbyRItemsBoughtX -= 10;
			}
			DrawTextToScreen(sTemp, bobbyRItemsBoughtX, (UINT16)usPosY, 0, FONT14ARIAL, BOBBYR_ITEM_DESC_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
		}
	}




	//if it's a used item, display how damaged the item is
	if( fUsed )
	{
		if ( g_lang == i18n::Lang::zh ) {
			swprintf( sTemp, ChineseSpecString2, LaptopSaveInfo.BobbyRayUsedInventory[ usBobbyIndex ].ubItemQuality );//zww
		} else {
			swprintf( sTemp, L"*%3d%%%%", LaptopSaveInfo.BobbyRayUsedInventory[ usBobbyIndex ].ubItemQuality );
		}
		
		DrawTextToScreen(sTemp, (UINT16)(BOBBYR_ITEM_NAME_X-2), (UINT16)(usPosY - BOBBYR_ORDER_NUM_Y_OFFSET), BOBBYR_ORDER_NUM_WIDTH, BOBBYR_ITEM_NAME_TEXT_FONT, BOBBYR_ITEM_NAME_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}

	//Display Items description
	LoadBRDesc(usIndex,sText);
	DisplayWrappedString(BOBBYR_ITEM_DESC_START_X, usPosY, BOBBYR_ITEM_DESC_START_WIDTH, 2, BOBBYR_ITEM_DESC_TEXT_FONT, BOBBYR_ITEM_DESC_TEXT_COLOR, sText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

#ifdef ENCYCLOPEDIA_WORKS
	//Moa: update encyclopedia item visibility when item gets displayed
	EncyclopediaSetItemAsVisible( usIndex, ENC_ITEM_DISCOVERED_NOT_INSPECTABLE );
#endif
}

/*
void InitFirstAndLastGlobalIndex(UINT32 uiItemClass)
{
	switch(uiItemClass)
	{
		case IC_BOBBY_GUN:
			gusLastItemIndex = gusLastGunIndex ;
			gusFirstItemIndex = gusFirstGunIndex;
			gubNumPages = gubNumGunPages;
			break;
		case IC_BOBBY_MISC:
			gusLastItemIndex = gusLastMiscIndex ;
			gusFirstItemIndex = gusFirstMiscIndex;
			gubNumPages = gubNumMiscPages;
			break;
		case IC_AMMO:
			gusLastItemIndex = gusLastAmmoIndex ;
			gusFirstItemIndex = gusFirstAmmoIndex;
			gubNumPages = gubNumAmmoPages;
			break;
		case IC_ARMOUR:
			gusLastItemIndex = gusLastArmourIndex;
			gusFirstItemIndex = gusFirstArmourIndex;
			gubNumPages = gubNumArmourPages;
			break;
		case BOBBYR_USED_ITEMS:
			gusLastItemIndex = gusLastUsedIndex;
			gusFirstItemIndex = gusFirstUsedIndex;
			gubNumPages = gubNumUsedPages;
			break;
		default:
			Assert(0);
			break;
	}
}

void CalculateFirstAndLastIndexs()
{
	//Get the first and last gun index
	SetFirstLastPagesForNew( IC_BOBBY_GUN, &gusFirstGunIndex, &gusLastGunIndex, &gubNumGunPages );

	//Get the first and last misc index
	SetFirstLastPagesForNew( IC_BOBBY_MISC, &gusFirstMiscIndex, &gusLastMiscIndex, &gubNumMiscPages );

	//Get the first and last Ammo index
	SetFirstLastPagesForNew( IC_AMMO, &gusFirstAmmoIndex, &gusLastAmmoIndex, &gubNumAmmoPages );

	//Get the first and last Armour index
	SetFirstLastPagesForNew( IC_ARMOUR, &gusFirstArmourIndex, &gusLastArmourIndex, &gubNumArmourPages );

	//Get the first and last Used index
	SetFirstLastPagesForUsed( &gusFirstUsedIndex, &gusLastUsedIndex, &gubNumUsedPages );
}
*/

//Loops through Bobby Rays Inventory to find the first and last index 
void SetFirstLastPagesForNew( UINT32 uiClassMask, INT32 iFilter, INT32 iSubFilter )
{
	UINT16	i;
	INT16	sFirst = -1;
	INT16	sLast = -1;
	UINT16	ubNumItems=0;
	UINT16	usItemIndex = 0;

	BOOLEAN bCntNumItems = FALSE;

	gubCurPage = 0;
	const std::size_t inventoryLength = BobbyRayCommerceModel::BoundedLength(
		LaptopSaveInfo.usInventoryListLength[BOBBY_RAY_NEW], MAXITEMS);

	//First loop through to get the first and last index indexs
	for(i=0; i<inventoryLength; ++i)
	{
		//If we have some of the inventory on hand
		if( LaptopSaveInfo.BobbyRayInventory[ i ].ubQtyOnHand != 0 )
		{
			usItemIndex = LaptopSaveInfo.BobbyRayInventory[i].usItemIndex;
			if (!IsLoadedBobbyRayItem(usItemIndex))
				continue;
			if( Item[usItemIndex].usItemClass & uiClassMask )
			{
				// No Filter->Take all
				if (iFilter == -1)
				{
					bCntNumItems = TRUE;
				}
				// Filter!
				else
				{
					bCntNumItems = FALSE;

					switch (uiClassMask)
					{
						// Guns
						case IC_BOBBY_GUN:
							if (Weapon[ Item[usItemIndex].ubClassIndex ].ubWeaponType == iFilter)
							{
								bCntNumItems = TRUE;
							}
							break;
						// Ammo
						case IC_AMMO:
							if (IsAmmoMatchinWeaponType(usItemIndex, guiCurrentAmmoFilterMode))
							{
								bCntNumItems = TRUE;
							}
							break;
						// Armour
						case IC_ARMOUR:
							if (Armour[ Item[usItemIndex].ubClassIndex ].ubArmourClass == iFilter)
							{
								bCntNumItems = TRUE;
							}
							break;
						// Misc
						case IC_BOBBY_MISC: 
							if (Item[usItemIndex].usItemClass == iFilter) // Madd: new BR filter options
							{
								if (iSubFilter > -1 )
								{
									if (ItemIsAttachment(usItemIndex) && Item[usItemIndex].attachmentclass & iSubFilter)
										bCntNumItems = TRUE;
									else if (iSubFilter == BR_MISC_FILTER_OTHER_ATTACHMENTS && !(Item[usItemIndex].attachmentclass & BR_MISC_FILTER_STD_ATTACHMENTS) && ItemIsAttachment(usItemIndex))
										bCntNumItems = TRUE;
									else if (iSubFilter == BR_MISC_FILTER_NO_ATTACHMENTS && !ItemIsAttachment(usItemIndex))
										bCntNumItems = TRUE;
								}
								else
									bCntNumItems = TRUE;
							}
							break;
					}
				}

				if (bCntNumItems == TRUE)
				{
					ubNumItems++;

					if( sFirst == -1 )
						sFirst = i;
					sLast = i;
				}
			}
		}
	}

	if( ubNumItems == 0 )
	{
		gbMessageDisplayed = FALSE;
		gusFirstItemIndex = BOBBYR_NO_ITEMS;
		gusLastItemIndex = BOBBYR_NO_ITEMS;
		gubNumPages = 0;
		return;
	}

	gusFirstItemIndex = (UINT16)sFirst;
	gusLastItemIndex = (UINT16)sLast;
	gubNumPages = static_cast<UINT16>(
		BobbyRayCatalogueModel::PageCount(ubNumItems));
}

//Loops through Bobby Rays Used Inventory to find the first and last index 
void SetFirstLastPagesForUsed(INT32 iFilter)
{
	UINT16  i;
	INT16	sFirst = -1;
	INT16	sLast = -1;
	UINT16	ubNumItems=0;
	UINT16	usItemIndex;

	gubCurPage = 0;
	BOOLEAN bCntNumItems = FALSE;
	const std::size_t inventoryLength = BobbyRayCommerceModel::BoundedLength(
		LaptopSaveInfo.usInventoryListLength[BOBBY_RAY_USED], MAXITEMS);

	//First loop through to get the first and last index indexs
	for(i=0; i<inventoryLength; ++i)
	{
		//If we have some of the inventory on hand
		if( LaptopSaveInfo.BobbyRayUsedInventory[ i ].ubQtyOnHand != 0 )
		{
			usItemIndex = LaptopSaveInfo.BobbyRayUsedInventory[i].usItemIndex;
			if (!IsLoadedBobbyRayItem(usItemIndex))
				continue;
			// No Filter->Take all
			if (iFilter == -1)
			{
				bCntNumItems = TRUE;
			}
			else
			{
				bCntNumItems = FALSE;
				switch (iFilter)
				{
					case IC_BOBBY_GUN:
						if (Item[usItemIndex].usItemClass == IC_GUN ||
							Item[usItemIndex].usItemClass == IC_LAUNCHER)
						{
							bCntNumItems = TRUE;
						}
						break;
					case IC_AMMO:
						if (Item[usItemIndex].usItemClass == IC_AMMO)
						{
							bCntNumItems = TRUE;
						}
						break;
					case IC_ARMOUR:
						if (Item[usItemIndex].usItemClass == IC_ARMOUR)
						{
							bCntNumItems = TRUE;
						}
						break;
					case IC_LBEGEAR:
						if (Item[usItemIndex].usItemClass == IC_LBEGEAR)
						{
							bCntNumItems = TRUE;
						}
						break;
					case IC_BOBBY_MISC:
						if (Item[usItemIndex].usItemClass == IC_BLADE ||
							Item[usItemIndex].usItemClass == IC_THROWING_KNIFE ||
							Item[usItemIndex].usItemClass == IC_PUNCH ||
							Item[usItemIndex].usItemClass == IC_GRENADE ||
							Item[usItemIndex].usItemClass == IC_BOMB ||
							Item[usItemIndex].usItemClass == IC_MISC ||
							Item[usItemIndex].usItemClass == IC_MEDKIT ||
							Item[usItemIndex].usItemClass == IC_KIT ||
							Item[usItemIndex].usItemClass == IC_FACE)
						{
							bCntNumItems = TRUE;
						}
						break;
				}
			}

			if (bCntNumItems == TRUE)
			{
				ubNumItems++;

				if( sFirst == -1 )
					sFirst = i;
				sLast = i;
			}
		}
	}
	if( sFirst == -1 )
	{
		gbMessageDisplayed = FALSE;
		gusFirstItemIndex = BOBBYR_NO_ITEMS;
		gusLastItemIndex = BOBBYR_NO_ITEMS;
		gubNumPages = 0;
		return;
	}

	gusFirstItemIndex = (UINT16)sFirst;
	gusLastItemIndex = (UINT16)sLast;
	gubNumPages = static_cast<UINT16>(
		BobbyRayCatalogueModel::PageCount(ubNumItems));
}


void CreateMouseRegionForBigImage( UINT16 usPosY, UINT8 ubCount, INT16 *pItemNumbers )
{
	UINT8	i;
	UINT16	usItem;
	LaptopPageResourceOwner stagedResources;
	// HEADROCK HAM 3: Increased size, to allow larger tooltips. Used for Attachments Shown on Tooltips feature.
	// Please note, if a mod introduces a weapon that can take several hundred attachments, this value MIGHT overflow.
	// I do not have the expertise to think of a better solution. Still, 5000 is enough for around 200 possible attachments,
	// so overflow will first be felt ON-SCREEN. I consider that scenario extremely unlikely.
	//CHAR16	pStr[ 250 ];
	CHAR16	pStr[ 5000 ];

	if( gfBigImageMouseRegionCreated )
		return;
	if (!pItemNumbers)
		return;

	ubCount = static_cast<UINT8>(std::min<std::size_t>(
		ubCount, BobbyRayCatalogueModel::ItemsPerPage));

	for(i=0; i<ubCount; i++)
	{
		//Mouse region for the Big Item Image
		MSYS_DefineRegion( &gSelectedBigImageRegion[ i ], BOBBYR_GRID_PIC_X, usPosY, (BOBBYR_GRID_PIC_X + BOBBYR_GRID_PIC_WIDTH), (UINT16)(usPosY + BOBBYR_GRID_PIC_HEIGHT), MSYS_PRIORITY_HIGH, CURSOR_WWW, MSYS_NO_CALLBACK, SelectBigImageRegionCallBack);
		if (!stagedResources.addRegion(gSelectedBigImageRegion[i]))
			return;
		MSYS_SetRegionUserData( &gSelectedBigImageRegion[ i ], 0, i);

		usItem = pItemNumbers[ i ];

		GetHelpTextForItemInLaptop( pStr, usItem );

		SetRegionFastHelpText( &gSelectedBigImageRegion[ i ], pStr );
		SetRegionHelpEndCallback( &gSelectedBigImageRegion[ i ], BobbyrRGunsHelpTextDoneCallBack );

		usPosY += BOBBYR_GRID_OFFSET;
	}

	gBobbyRBigImageResources.clear();
	gBobbyRBigImageResources = std::move(stagedResources);
	gubNumItemsOnScreen = ubCount;
	gfBigImageMouseRegionCreated = ubCount > 0;
}

void DeleteMouseRegionForBigImage()
{
	gBobbyRBigImageResources.clear();
	gfBigImageMouseRegionCreated = FALSE;
	gusOldItemNumOnTopOfPage = 65535;
	gubNumItemsOnScreen = 0;
}


void SelectBigImageRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
		return;
	}

	const std::size_t itemSlot = static_cast<std::size_t>(
		MSYS_GetRegionUserData(pRegion, 0));
	if (!BobbyRayCatalogueModel::IsVisibleItemSlot(
			itemSlot, gubNumItemsOnScreen))
	{
		return;
	}
	const UINT16 itemNumber = gusItemNumberForItemsOnScreen[itemSlot];

	if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		// HEADROCK HAM 4: Purchase all at once.
		if (_KeyDown( 16 ) ) // SHIFT
			PurchaseBobbyRayItem(itemNumber, TRUE);
		else
			PurchaseBobbyRayItem(itemNumber, FALSE);

		fReDrawScreenFlag = TRUE;
		fPausedReDrawScreenFlag = TRUE;
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
		// HEADROCK HAM 4: Unpurchase all at once.
		if (_KeyDown( 16 ) ) // SHIFT
			UnPurchaseBobbyRayItem(itemNumber, TRUE);
		else
			UnPurchaseBobbyRayItem(itemNumber, FALSE);
		fReDrawScreenFlag = TRUE;
		fPausedReDrawScreenFlag = TRUE;
	}
	else if(iReason & MSYS_CALLBACK_REASON_LBUTTON_REPEAT)
	{
		PurchaseBobbyRayItem(itemNumber, FALSE);
		fReDrawScreenFlag = TRUE;
		fPausedReDrawScreenFlag = TRUE;
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_REPEAT)
	{
		UnPurchaseBobbyRayItem(itemNumber, FALSE);
		fReDrawScreenFlag = TRUE;
		fPausedReDrawScreenFlag = TRUE;
	}
}



void PurchaseBobbyRayItem(UINT16	usItemNumber, BOOLEAN fAllItems )
{
	UINT8	ubPurchaseNumber;
	const UINT8 inventoryKind =
		guiCurrentLaptopMode == LAPTOP_MODE_BOBBY_R_USED
			? BOBBY_RAY_USED : BOBBY_RAY_NEW;
	if (!BobbyRayCommerceModel::IsIndexInBoundedList(
			usItemNumber,
			LaptopSaveInfo.usInventoryListLength[inventoryKind], MAXITEMS))
	{
		return;
	}

	ubPurchaseNumber = CheckIfItemIsPurchased(usItemNumber);

	//if we are in the used page
	if( guiCurrentLaptopMode == LAPTOP_MODE_BOBBY_R_USED )
	{
		//if there is enough inventory in stock to cover the purchase
		if( ubPurchaseNumber == BOBBY_RAY_NOT_PURCHASED || LaptopSaveInfo.BobbyRayUsedInventory[ usItemNumber ].ubQtyOnHand >= ( BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased + 1) )
		{
			// If the item has not yet been purchased
			if( ubPurchaseNumber == BOBBY_RAY_NOT_PURCHASED )
			{
				ubPurchaseNumber = GetNextPurchaseNumber();

				if( ubPurchaseNumber != BOBBY_RAY_NOT_PURCHASED )
				{
					UINT8 ubNumItems = fAllItems ? LaptopSaveInfo.BobbyRayUsedInventory[ usItemNumber ].ubQtyOnHand : 1;

					BobbyRayPurchases[ ubPurchaseNumber ].usItemIndex = LaptopSaveInfo.BobbyRayUsedInventory[ usItemNumber ].usItemIndex;
					BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased = ubNumItems;
					BobbyRayPurchases[ ubPurchaseNumber ].bItemQuality = LaptopSaveInfo.BobbyRayUsedInventory[ usItemNumber ].ubItemQuality;
					BobbyRayPurchases[ ubPurchaseNumber ].usBobbyItemIndex = usItemNumber;
					BobbyRayPurchases[ ubPurchaseNumber ].fUsed = TRUE;
				}
				else
				{
					//display error popup because the player is trying to purchase more thenn 10 items
					CHAR16 sMaxPurchase[255];
					swprintf(sMaxPurchase, L"%s%d%s", BobbyRText[ BOBBYR_MORE_THEN_10_PURCHASES_A], GetConfiguredBobbyRayPurchaseLimit(), BobbyRText[ BOBBYR_MORE_THEN_10_PURCHASES_B]);
					DoLapTopMessageBox( MSG_BOX_LAPTOP_DEFAULT, sMaxPurchase, LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);

				}
			}
			// Else If the item is already purchased increment purchase amount.	Only if ordering less then the max amount!
			else
			{
				UINT8 ubNumItems = fAllItems ? LaptopSaveInfo.BobbyRayUsedInventory[ usItemNumber ].ubQtyOnHand - BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased : 1;

				if( BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased <= BOBBY_RAY_MAX_AMOUNT_OF_ITEMS_TO_PURCHASE - (ubNumItems-1))
					BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased += ubNumItems;
			}
		}
		else
		{
			DoLapTopMessageBox( MSG_BOX_LAPTOP_DEFAULT, BobbyRText[ BOBBYR_MORE_NO_MORE_IN_STOCK ], LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);

			#ifdef JA2BETAVERSION
				ReportBobbyROrderError( usItemNumber, ubPurchaseNumber, LaptopSaveInfo.BobbyRayUsedInventory[ usItemNumber ].ubQtyOnHand, BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased );
			#endif
		}
	}
	//else the player is on a any other page except the used page
	else
	{
		//if there is enough inventory in stock to cover the purchase
		if( ubPurchaseNumber == BOBBY_RAY_NOT_PURCHASED || LaptopSaveInfo.BobbyRayInventory[ usItemNumber ].ubQtyOnHand >= ( BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased + 1) )
		{
			// If the item has not yet been purchased
			if( ubPurchaseNumber == BOBBY_RAY_NOT_PURCHASED )
			{
				ubPurchaseNumber = GetNextPurchaseNumber();

				if( ubPurchaseNumber != BOBBY_RAY_NOT_PURCHASED )
				{
					UINT8 ubNumItems = fAllItems ? LaptopSaveInfo.BobbyRayInventory[ usItemNumber ].ubQtyOnHand : 1;

					BobbyRayPurchases[ ubPurchaseNumber ].usItemIndex = LaptopSaveInfo.BobbyRayInventory[ usItemNumber ].usItemIndex;
					BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased = ubNumItems;
					BobbyRayPurchases[ ubPurchaseNumber ].bItemQuality = 100;
					BobbyRayPurchases[ ubPurchaseNumber ].usBobbyItemIndex = usItemNumber;
					BobbyRayPurchases[ ubPurchaseNumber ].fUsed = FALSE;
				}
				else
				{
					//display error popup because the player is trying to purchase more thenn 10 items
					CHAR16 sMaxPurchase[255];
					swprintf(sMaxPurchase, L"%s%d%s", BobbyRText[ BOBBYR_MORE_THEN_10_PURCHASES_A], GetConfiguredBobbyRayPurchaseLimit(), BobbyRText[ BOBBYR_MORE_THEN_10_PURCHASES_B]);
					DoLapTopMessageBox( MSG_BOX_LAPTOP_DEFAULT, sMaxPurchase, LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);
				}
			}
			// Else If the item is already purchased increment purchase amount.	Only if ordering less then the max amount!
			else
			{
				UINT8 ubNumItems = fAllItems ? LaptopSaveInfo.BobbyRayInventory[ usItemNumber ].ubQtyOnHand - BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased : 1;

				if( BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased <= BOBBY_RAY_MAX_AMOUNT_OF_ITEMS_TO_PURCHASE - (ubNumItems-1))
					BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased += ubNumItems;
			}
		}
		else
		{
			DoLapTopMessageBox( MSG_BOX_LAPTOP_DEFAULT, BobbyRText[ BOBBYR_MORE_NO_MORE_IN_STOCK ], LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);

			#ifdef JA2BETAVERSION
				ReportBobbyROrderError( usItemNumber, ubPurchaseNumber, LaptopSaveInfo.BobbyRayInventory[ usItemNumber ].ubQtyOnHand, BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased );
			#endif
		}
	}
}


// Checks to see if the clicked item is already bought or not.
UINT8 CheckIfItemIsPurchased(UINT16 usItemNumber)
{
	UINT8	i;

	for(i=0; i<GetConfiguredBobbyRayPurchaseLimit(); i++)
	{
		if( ( usItemNumber == BobbyRayPurchases[i].usBobbyItemIndex ) && ( BobbyRayPurchases[i].ubNumberPurchased != 0 ) && ( BobbyRayPurchases[i].fUsed == gfOnUsedPage ) )
			return(i);
	}
	return(BOBBY_RAY_NOT_PURCHASED);
}

UINT8 GetNextPurchaseNumber()
{
	UINT8	i;

	for(i=0; i<GetConfiguredBobbyRayPurchaseLimit(); i++)
	{
		if( ( BobbyRayPurchases[i].usBobbyItemIndex == 0) && ( BobbyRayPurchases[i].ubNumberPurchased == 0 ) )
			return(i);
	}
	return(BOBBY_RAY_NOT_PURCHASED);
}



void UnPurchaseBobbyRayItem(UINT16	usItemNumber, BOOLEAN fAllItems )
{
	UINT8	ubPurchaseNumber;

	ubPurchaseNumber = CheckIfItemIsPurchased(usItemNumber);

	if( ubPurchaseNumber != BOBBY_RAY_NOT_PURCHASED )
	{
		if( BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased > 1 && !fAllItems)
			BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased--;
		else
		{
			BobbyRayPurchases[ ubPurchaseNumber ].ubNumberPurchased = 0;
			BobbyRayPurchases[ ubPurchaseNumber ].usBobbyItemIndex = 0;
		}
	}
}




void BtnBobbyROrderFormCallback(GUI_BUTTON *btn,INT32 reason)
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

void BtnBobbyRHomeButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		guiCurrentLaptopMode = LAPTOP_MODE_BOBBY_R;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


void UpdateButtonText(UINT32	uiCurPage)
{
	switch( uiCurPage )
	{
		case LAPTOP_MODE_BOBBY_R_GUNS:
			if (guiCurrentGunFilterMode != -1)
			{
				EnableButton( guiBobbyRPageMenu[0] );
			}
			else
			{
				DisableButton( guiBobbyRPageMenu[0] );
			}
			break;

		case LAPTOP_MODE_BOBBY_R_AMMO:
			if (guiCurrentAmmoFilterMode != -1)
			{
				EnableButton( guiBobbyRPageMenu[1] );
			}
			else
			{
				DisableButton( guiBobbyRPageMenu[1] );
			}
			break;

		case LAPTOP_MODE_BOBBY_R_ARMOR:
			if (guiCurrentArmourFilterMode != -1)
			{
				EnableButton( guiBobbyRPageMenu[2] );
			}
			else
			{
				DisableButton( guiBobbyRPageMenu[2] );
			}
			break;

		case LAPTOP_MODE_BOBBY_R_MISC:
			if (guiCurrentMiscFilterMode != -1)
			{
				EnableButton( guiBobbyRPageMenu[3] );
			}
			else
			{
				DisableButton( guiBobbyRPageMenu[3] );
			}
			break;

		case LAPTOP_MODE_BOBBY_R_USED:
			if (guiCurrentUsedFilterMode != -1)
			{
				EnableButton( guiBobbyRPageMenu[4] );
			}
			else
			{
				DisableButton( guiBobbyRPageMenu[4] );
			}
			break;
	}
}

void UpdateAmmoFilterButtons()
{
	EnableButton(guiBobbyRFilterAmmo[0]);
	EnableButton(guiBobbyRFilterAmmo[1]);
	EnableButton(guiBobbyRFilterAmmo[2]);
	EnableButton(guiBobbyRFilterAmmo[3]);
	EnableButton(guiBobbyRFilterAmmo[4]);
	EnableButton(guiBobbyRFilterAmmo[5]);
	EnableButton(guiBobbyRFilterAmmo[6]);
	EnableButton(guiBobbyRFilterAmmo[7]);

	switch (guiCurrentAmmoFilterMode)
	{
		case GUN_PISTOL:
			DisableButton(guiBobbyRFilterAmmo[0]);
			break;
		case GUN_M_PISTOL:
			DisableButton(guiBobbyRFilterAmmo[1]);
			break;
		case GUN_SMG:
			DisableButton(guiBobbyRFilterAmmo[2]);
			break;
		case GUN_RIFLE:
			DisableButton(guiBobbyRFilterAmmo[3]);
			break;
		case GUN_SN_RIFLE:
			DisableButton(guiBobbyRFilterAmmo[4]);
			break;
		case GUN_AS_RIFLE:
			DisableButton(guiBobbyRFilterAmmo[5]);
			break;
		case GUN_LMG:
			DisableButton(guiBobbyRFilterAmmo[6]);
			break;
		case GUN_SHOTGUN:
			DisableButton(guiBobbyRFilterAmmo[7]);
			break;
	}

	//// Bugged when >1 row of buttons
	//if (iNewButton != iOldButton)
	//{
	//	if (iNewButton > -1)
	//	{
	//		// Disable new Button
	//		DisableButton(guiBobbyRFilterAmmo[iNewButton - 1]);
	//	}

	//	if (iOldButton > -1)
	//	{
	//		// Enable old Button
	//		EnableButton(guiBobbyRFilterAmmo[iOldButton - 1]);
	//	}
	//}
}

void UpdateGunFilterButtons()
{
	EnableButton(guiBobbyRFilterGuns[0]);
	EnableButton(guiBobbyRFilterGuns[1]);
	EnableButton(guiBobbyRFilterGuns[2]);
	EnableButton(guiBobbyRFilterGuns[3]);
	EnableButton(guiBobbyRFilterGuns[4]);
	EnableButton(guiBobbyRFilterGuns[5]);
	EnableButton(guiBobbyRFilterGuns[6]);
	EnableButton(guiBobbyRFilterGuns[7]);
	EnableButton(guiBobbyRFilterGuns[8]);

	switch (guiCurrentGunFilterMode)
	{
		case GUN_PISTOL:
			DisableButton(guiBobbyRFilterGuns[0]);
			break;
		case GUN_M_PISTOL:
			DisableButton(guiBobbyRFilterGuns[1]);
			break;
		case GUN_SMG:
			DisableButton(guiBobbyRFilterGuns[2]);
			break;
		case GUN_RIFLE:
			DisableButton(guiBobbyRFilterGuns[3]);
			break;
		case GUN_SN_RIFLE:
			DisableButton(guiBobbyRFilterGuns[4]);
			break;
		case GUN_AS_RIFLE:
			DisableButton(guiBobbyRFilterGuns[5]);
			break;
		case GUN_LMG:
			DisableButton(guiBobbyRFilterGuns[6]);
			break;
		case GUN_SHOTGUN:
			DisableButton(guiBobbyRFilterGuns[7]);
			break;
		case NOT_GUN:
			DisableButton(guiBobbyRFilterGuns[8]);
			break;
	}

	//if (iNewButton != iOldButton)
	//{
	//	if (iNewButton > -1)
	//	{
	//		// Disable new Button
	//		DisableButton(guiBobbyRFilterGuns[iNewButton - 1]);
	//	}

	//	if (iOldButton > -1)
	//	{
	//		// Enable old Button
	//		EnableButton(guiBobbyRFilterGuns[iOldButton - 1]);
	//	}
	//}
}

void UpdateUsedFilterButtons()
{
	EnableButton(guiBobbyRFilterUsed[0]);
	EnableButton(guiBobbyRFilterUsed[1]);
	if(guiBobbyRFilterUsed[2])
		EnableButton(guiBobbyRFilterUsed[2]);
	EnableButton(guiBobbyRFilterUsed[3]);

	switch (guiCurrentUsedFilterMode)
	{
		case IC_BOBBY_GUN:
			DisableButton(guiBobbyRFilterUsed[0]);
			break;
		case IC_ARMOUR:
			DisableButton(guiBobbyRFilterUsed[1]);
			break;
		case IC_LBEGEAR:
			DisableButton(guiBobbyRFilterUsed[2]);
			break;
		case IC_BOBBY_MISC:
			DisableButton(guiBobbyRFilterUsed[3]);
			break;
	}
}

void UpdateArmourFilterButtons()
{
	EnableButton(guiBobbyRFilterArmour[0]);
	EnableButton(guiBobbyRFilterArmour[1]);
	EnableButton(guiBobbyRFilterArmour[2]);
	EnableButton(guiBobbyRFilterArmour[3]);

	switch (guiCurrentArmourFilterMode)
	{
		case ARMOURCLASS_HELMET:
			DisableButton(guiBobbyRFilterArmour[0]);
			break;
		case ARMOURCLASS_VEST:
			DisableButton(guiBobbyRFilterArmour[1]);
			break;
		case ARMOURCLASS_LEGGINGS:
			DisableButton(guiBobbyRFilterArmour[2]);
			break;
		case ARMOURCLASS_PLATE:
			DisableButton(guiBobbyRFilterArmour[3]);
			break;
	}

	//if (iNewButton != iOldButton)
	//{
	//	if (iNewButton > -1)
	//	{
	//		// Disable new Button
	//		DisableButton(guiBobbyRFilterArmor[iNewButton]);
	//	}

	//	if (iOldButton > -1)
	//	{
	//		// Enable old Button
	//		EnableButton(guiBobbyRFilterArmor[iOldButton]);
	//	}
	//}
}

void UpdateMiscFilterButtons()
{
	EnableButton(guiBobbyRFilterMisc[0]);
	EnableButton(guiBobbyRFilterMisc[1]);
	EnableButton(guiBobbyRFilterMisc[2]);
	EnableButton(guiBobbyRFilterMisc[3]);
	EnableButton(guiBobbyRFilterMisc[4]);
	EnableButton(guiBobbyRFilterMisc[5]);
	EnableButton(guiBobbyRFilterMisc[6]);
	EnableButton(guiBobbyRFilterMisc[7]);
	if(guiBobbyRFilterMisc[8])
		EnableButton(guiBobbyRFilterMisc[8]);
	EnableButton(guiBobbyRFilterMisc[9]);
	EnableButton(guiBobbyRFilterMisc[10]); // Madd: New BR filter options
	EnableButton(guiBobbyRFilterMisc[11]);
	EnableButton(guiBobbyRFilterMisc[12]);
	EnableButton(guiBobbyRFilterMisc[13]);
	EnableButton(guiBobbyRFilterMisc[14]);
	EnableButton(guiBobbyRFilterMisc[15]);

	switch (guiCurrentMiscFilterMode)
	{
		case IC_BLADE:
			DisableButton(guiBobbyRFilterMisc[0]);
			break;
		case IC_THROWING_KNIFE:
			DisableButton(guiBobbyRFilterMisc[1]);
			break;
		case IC_PUNCH:
			DisableButton(guiBobbyRFilterMisc[2]);
			break;
		case IC_GRENADE:
			DisableButton(guiBobbyRFilterMisc[3]);
			break;
		case IC_BOMB:
			DisableButton(guiBobbyRFilterMisc[4]);
			break;
		case IC_MEDKIT:
			DisableButton(guiBobbyRFilterMisc[5]);
			break;
		case IC_KIT:
			DisableButton(guiBobbyRFilterMisc[6]);
			break;
		case IC_FACE:
			DisableButton(guiBobbyRFilterMisc[7]);
			break;
		case IC_LBEGEAR:
			DisableButton(guiBobbyRFilterMisc[8]);
			break;
		case IC_MISC:
			// Madd: new BR filters
			if (guiCurrentMiscSubFilterMode == BR_MISC_FILTER_OPTICS)
				DisableButton(guiBobbyRFilterMisc[9]);
			else if (guiCurrentMiscSubFilterMode == BR_MISC_FILTER_SIDE_BOTTOM)
				DisableButton(guiBobbyRFilterMisc[10]);
			else if (guiCurrentMiscSubFilterMode == BR_MISC_FILTER_MUZZLE)
				DisableButton(guiBobbyRFilterMisc[11]);
			else if (guiCurrentMiscSubFilterMode == BR_MISC_FILTER_STOCK)
				DisableButton(guiBobbyRFilterMisc[12]);
			else if (guiCurrentMiscSubFilterMode == BR_MISC_FILTER_INTERNAL)
				DisableButton(guiBobbyRFilterMisc[13]);
			else if (guiCurrentMiscSubFilterMode == BR_MISC_FILTER_OTHER_ATTACHMENTS)
				DisableButton(guiBobbyRFilterMisc[14]);
			else if (guiCurrentMiscSubFilterMode == BR_MISC_FILTER_NO_ATTACHMENTS)
				DisableButton(guiBobbyRFilterMisc[15]);
			break;
	}

	//if (iNewButton != iOldButton)
	//{
	//	if (iNewButton > -1)
	//	{
	//		// Disable new Button
	//		DisableButton(guiBobbyRFilterMisc[iNewButton]);
	//	}

	//	if (iOldButton > -1)
	//	{
	//		// Enable old Button
	//		EnableButton(guiBobbyRFilterMisc[iOldButton]);
	//	}
	//}
}

UINT16 CalcBobbyRayCost( UINT16 usIndex, UINT16 usBobbyIndex, BOOLEAN fUsed,
	INT8 bItemQuality)
{
	if (!IsLoadedBobbyRayItem(usIndex))
		return 0;
	DOUBLE value = Item[usIndex].usPrice;
	if (fUsed)
	{
		INT32 quality = bItemQuality;
		if (BobbyRayCommerceModel::IsIndexInBoundedList(
				usBobbyIndex,
				LaptopSaveInfo.usInventoryListLength[BOBBY_RAY_USED],
				MAXITEMS) &&
			LaptopSaveInfo.BobbyRayUsedInventory[usBobbyIndex].usItemIndex ==
				usIndex)
		{
			quality = LaptopSaveInfo.BobbyRayUsedInventory[
				usBobbyIndex].ubItemQuality;
		}
		if (quality < 0) quality = 0;
		if (quality > 100) quality = 100;
		value *= .5 + .5 * quality / 100.0;
		value += .5;
	}

	return( (UINT16) value);
}


UINT32 CalculateTotalPurchasePrice()
{
	UINT16	i;
	UINT32	uiTotal = 0;

	for(i=0; i<GetConfiguredBobbyRayPurchaseLimit(); i++)
	{
		//if the item was purchased
		if( BobbyRayPurchases[ i ].ubNumberPurchased )
		{
			uiTotal += CalcBobbyRayCost( BobbyRayPurchases[ i ].usItemIndex, BobbyRayPurchases[ i ].usBobbyItemIndex, BobbyRayPurchases[ i ].fUsed, BobbyRayPurchases[i].bItemQuality ) * BobbyRayPurchases[ i ].ubNumberPurchased;
		}
	}

	return(uiTotal);
}

void DisableBobbyRButtons()
{
	//if it is the last page, disable the next page button
	if( gubNumPages == 0 )
		DisableButton( guiBobbyRNextPage );
	else
	{
		if( gubCurPage >= gubNumPages-1 )
			DisableButton( guiBobbyRNextPage );
		else
			EnableButton( guiBobbyRNextPage );
	}


	// if it is the first page, disable the prev page buitton
	if( gubCurPage == 0 )
		DisableButton( guiBobbyRPreviousPage );
	else
		EnableButton( guiBobbyRPreviousPage );

/*
	//if it is the last page, disable the next page button
	if( !(gusCurWeaponIndex < (gusLastItemIndex - BOBBYR_NUM_WEAPONS_ON_PAGE) ) )
		DisableButton( guiBobbyRNextPage );
	else
		EnableButton( guiBobbyRNextPage );
		

	// if it is the first page, disable the prev page buitton
	if( (gusCurWeaponIndex == gusFirstItemIndex ) )
		DisableButton( guiBobbyRPreviousPage );
	else
		EnableButton( guiBobbyRPreviousPage );
*/
}

void CalcFirstIndexForPage( STORE_INVENTORY *pInv, UINT32	uiItemClass )
{
	UINT16	i;
	UINT16	usNumItems=0;
	BOOLEAN bCntItem = FALSE;
	UINT32 usItemIndex = 0;

	//Reset the Current weapon Index
	gusCurWeaponIndex = 0;

	if( uiItemClass == BOBBYR_USED_ITEMS )
	{
		//Get to the first index on the page
		for(i=gusFirstItemIndex; i<=gusLastItemIndex; i++)
		{
			bCntItem = FALSE;

			if (guiCurrentUsedFilterMode == -1)
			{
				bCntItem = TRUE;
			}
			else
			{
				usItemIndex = pInv[ i ].usItemIndex;

				switch (guiCurrentUsedFilterMode)
				{
					case IC_BOBBY_GUN:
						if (Item[usItemIndex].usItemClass == IC_GUN ||
							Item[usItemIndex].usItemClass == IC_LAUNCHER)
						{
							bCntItem = TRUE;
						}
						break;
					case IC_AMMO:
						if (Item[usItemIndex].usItemClass == IC_AMMO)
						{
							bCntItem = TRUE;
						}
						break;
					case IC_ARMOUR:
						if (Item[usItemIndex].usItemClass == IC_ARMOUR)
						{
							bCntItem = TRUE;
						}
						break;
					case IC_LBEGEAR:
						if (Item[usItemIndex].usItemClass == IC_LBEGEAR)
						{
							bCntItem = TRUE;
						}
						break;
					case IC_BOBBY_MISC:
						if (Item[usItemIndex].usItemClass == IC_BLADE ||
							Item[usItemIndex].usItemClass == IC_THROWING_KNIFE ||
							Item[usItemIndex].usItemClass == IC_PUNCH ||
							Item[usItemIndex].usItemClass == IC_GRENADE ||
							Item[usItemIndex].usItemClass == IC_BOMB ||
							Item[usItemIndex].usItemClass == IC_MISC ||
							Item[usItemIndex].usItemClass == IC_MEDKIT ||
							Item[usItemIndex].usItemClass == IC_KIT ||
							Item[usItemIndex].usItemClass == IC_FACE)
						{
							bCntItem = TRUE;
						}
						break;
				}
			}

			//If we have some of the inventory on hand
			if( pInv[ i ].ubQtyOnHand != 0 )
			{
				if (bCntItem == TRUE)
				{
					if( gubCurPage == 0 )
					{
						gusCurWeaponIndex = i;
						break;
					}

					if( usNumItems <= ( gubCurPage *
						BobbyRayCatalogueModel::ItemsPerPage ) )
						gusCurWeaponIndex = i;

					usNumItems++;
				}
			}
		}
	}
	else
	{
		//Get to the first index on the page
		for(i=gusFirstItemIndex; i<=gusLastItemIndex; i++)
		{
			bCntItem = FALSE;

			if( Item[ pInv[ i ].usItemIndex ].usItemClass & uiItemClass )
			{
				if (uiItemClass == IC_BOBBY_GUN)
				{
					if (guiCurrentGunFilterMode == -1)
					{
						bCntItem = TRUE;
					}
					else
					{
						usItemIndex = LaptopSaveInfo.BobbyRayInventory[ i ].usItemIndex;

						if (Weapon[ Item[usItemIndex].ubClassIndex ].ubWeaponType == guiCurrentGunFilterMode)
						{
							bCntItem = TRUE;
						}
					}
				}
				else if (uiItemClass == IC_AMMO)
				{
					if (guiCurrentAmmoFilterMode == -1)
					{
						bCntItem = TRUE;
					}
					else
					{
						usItemIndex = LaptopSaveInfo.BobbyRayInventory[ i ].usItemIndex;

						if (IsAmmoMatchinWeaponType(usItemIndex, guiCurrentAmmoFilterMode))
						{
							bCntItem = TRUE;
						}

					}
				}
				else if (uiItemClass == IC_ARMOUR)
				{
					if (guiCurrentArmourFilterMode == -1)
					{
						bCntItem = TRUE;
					}
					else
					{
						usItemIndex = LaptopSaveInfo.BobbyRayInventory[ i ].usItemIndex;

						if (Armour[ Item[usItemIndex].ubClassIndex ].ubArmourClass == guiCurrentArmourFilterMode)
						{
							bCntItem = TRUE;
						}
					}
				}
				else if (uiItemClass == IC_BOBBY_MISC)
				{
					if (guiCurrentMiscFilterMode == -1)
					{
						bCntItem = TRUE;
					}
					else
					{
						usItemIndex = LaptopSaveInfo.BobbyRayInventory[ i ].usItemIndex; 
						if (Item[usItemIndex].usItemClass == guiCurrentMiscFilterMode)
						{
							if (guiCurrentMiscSubFilterMode > -1) // Madd: new BR filter options
							{
								if (ItemIsAttachment(usItemIndex) && Item[usItemIndex].attachmentclass & guiCurrentMiscSubFilterMode)
									bCntItem = TRUE;
								else if (guiCurrentMiscSubFilterMode == BR_MISC_FILTER_OTHER_ATTACHMENTS && !(Item[usItemIndex].attachmentclass & BR_MISC_FILTER_STD_ATTACHMENTS) && ItemIsAttachment(usItemIndex))
									bCntItem = TRUE;
								else if (guiCurrentMiscSubFilterMode == BR_MISC_FILTER_NO_ATTACHMENTS && !ItemIsAttachment(usItemIndex))
									bCntItem = TRUE;
							}
							else 
								bCntItem = TRUE;
						}
					}
				}

				//If we have some of the inventory on hand
				if( pInv[ i ].ubQtyOnHand != 0 )
				{
					if (bCntItem == TRUE)
					{
						if( gubCurPage == 0 )
						{
							gusCurWeaponIndex = i;
							break;
						}

						if( usNumItems <= ( gubCurPage *
							BobbyRayCatalogueModel::ItemsPerPage ) )
							gusCurWeaponIndex = i;

						usNumItems++;
					}
				}
			}
		}
	}
}

BOOLEAN IsAmmoMatchinWeaponType(UINT16 usItemIndex, UINT8 ubWeaponType)
{
	if (!IsLoadedBobbyRayItem(usItemIndex))
		return FALSE;
	BOOLEAN bRetValue = FALSE;

	for (std::size_t i = 0;
		i < std::min<std::size_t>(gMAXITEMS_READ, MAXITEMS); ++i)
	{
		if (Item[i].usItemClass != IC_GUN )
			continue;

		// We found the Weapon that uses this Ammo
		if (Magazine[ Item[ usItemIndex ].ubClassIndex ].ubCalibre == Weapon[i].ubCalibre)
		{
			// Weapon has the correct filter ammo weapon type
			if (Weapon[i].ubWeaponType == ubWeaponType)
			{
				//Weapon has correct magazine size
				if(Magazine[ Item[ usItemIndex ].ubClassIndex ].ubMagType >= AMMO_BOX || (Magazine[ Item[ usItemIndex ].ubClassIndex ].ubMagType < AMMO_BOX && Weapon[i].ubMagSize == Magazine[Item[usItemIndex].ubClassIndex].ubMagSize ))
				{
					bRetValue = TRUE;
					break;
				}
			}
		}
	}

	return bRetValue;
}


void OutOfStockMessageBoxCallBack( UINT8 bExitValue )
{
	// yes, load the game
	if( bExitValue == MSG_BOX_RETURN_OK )
	{
//		guiCurrentLaptopMode	= LAPTOP_MODE_BOBBY_R;
	}
}



UINT8 CheckPlayersInventoryForGunMatchingGivenAmmoID( INT16 sItemID )
{
	if (sItemID < 0 || !IsLoadedBobbyRayItem(
			static_cast<std::size_t>(sItemID)))
	{
		return 0;
	}
	UINT8	ubItemCount=0;

	SoldierID 	id;
	SoldierID 	ubFirstID = gTacticalStatus.Team[ OUR_TEAM ].bFirstID;
	SoldierID 	ubLastID = gTacticalStatus.Team[ OUR_TEAM ].bLastID;

	//loop through all the mercs on the team
	for( id = ubFirstID; id <= ubLastID; ++id )
	{
		TacticalActor *pSoldier =
			GetJa2SoldierRepository().resolve(id.i);
		if( pSoldier && pSoldier->roster().active() )
		{
			//loop through all the pockets on the merc
			const std::size_t inventorySize = pSoldier->inventory().size();
			for(std::size_t pocket = 0; pocket < inventorySize; ++pocket)
			{
				const UINT16 itemIndex = pSoldier->inventory()[pocket].usItem;
				if (!IsLoadedBobbyRayItem(itemIndex))
					continue;
				//if there is a weapon here
				if( Item[itemIndex].usItemClass == IC_GUN )
				{
					//if the weapon uses the same kind of ammo as the one passed in, return true
					if( Weapon[itemIndex].ubCalibre ==
						Magazine[Item[sItemID].ubClassIndex].ubCalibre )
					{
						++ubItemCount;
					}
				}
			}
		}
	}

	return( ubItemCount );
}



void BobbyrRGunsHelpTextDoneCallBack( void )
{
	fReDrawScreenFlag = TRUE;
	fPausedReDrawScreenFlag = TRUE;
}

#ifdef JA2BETAVERSION
void ReportBobbyROrderError( UINT16 usItemNumber, UINT8 ubPurchaseNum, UINT8 ubQtyOnHand, UINT8 ubNumPurchasing )
{
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("**** Bobby Rays Ordering Error ****" ) );
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("usItemNumber = %d", usItemNumber ) );
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("ubPurchaseNum = %d", ubPurchaseNum ) );
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("ubQtyOnHand = %d", ubQtyOnHand ) );
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("ubNumPurchasing = %d", ubNumPurchasing ) );
}
#endif

void HandleBobbyRGunsKeyBoardInput()
{
	InputAtom					InputEvent;
	BOOLEAN fCtrl;

	fCtrl = _KeyDown( CTRL );

	//while (DequeueSpecificEvent(&InputEvent, KEY_DOWN |KEY_REPEAT) == TRUE)
	while (DequeueEvent(&InputEvent) == TRUE)
	{
		if( InputEvent.usEvent == KEY_DOWN )
		{
			switch (InputEvent.usParam)
			{
				case LEFTARROW:
				case 'a':
					// previous page
					gubCurPage = static_cast<UINT16>(
						BobbyRayCatalogueModel::PreviousPage(
							gubCurPage, gubNumPages,
							fCtrl ? gubNumPages : 1));
					DeleteMouseRegionForBigImage();
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case RIGHTARROW:
				case 'd':
					// next page
					gubCurPage = static_cast<UINT16>(
						BobbyRayCatalogueModel::NextPage(
							gubCurPage, gubNumPages,
							fCtrl ? gubNumPages : 1));
					DeleteMouseRegionForBigImage();
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case SHIFT_LEFTARROW:
				case 'A':
					// jump to 10 pages back
					gubCurPage = static_cast<UINT16>(
						BobbyRayCatalogueModel::PreviousPage(
							gubCurPage, gubNumPages,
							fCtrl ? gubNumPages : 10));
					DeleteMouseRegionForBigImage();
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case SHIFT_RIGHTARROW:
				case 'D':
					// jump to 10 pages forward
					gubCurPage = static_cast<UINT16>(
						BobbyRayCatalogueModel::NextPage(
							gubCurPage, gubNumPages,
							fCtrl ? gubNumPages : 10));
					DeleteMouseRegionForBigImage();
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case ENTER:
				case 'e':
					// order form
					guiCurrentLaptopMode = LAPTOP_MODE_BOBBY_R_MAILORDER;
				break;
				case '1':
					// 1st item on screen
					if (BobbyRayCatalogueModel::IsVisibleItemSlot(
							0, gubNumItemsOnScreen))
					{
						if( fCtrl )
							UnPurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 0 ], FALSE);
						else
							PurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 0 ], FALSE);
					}
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case '2':
					// 2nd item on screen
					if (BobbyRayCatalogueModel::IsVisibleItemSlot(
							1, gubNumItemsOnScreen))
					{
						if( fCtrl )
							UnPurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 1 ], FALSE);
						else
							PurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 1 ], FALSE);
					}
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case '3':
					// 3rd item on screen
					if (BobbyRayCatalogueModel::IsVisibleItemSlot(
							2, gubNumItemsOnScreen))
					{
						if( fCtrl )
							UnPurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 2 ], FALSE);
						else
							PurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 2 ], FALSE);
					}
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case '4':
					// 4th item on screen
					if (BobbyRayCatalogueModel::IsVisibleItemSlot(
							3, gubNumItemsOnScreen))
					{
						if( fCtrl )
							UnPurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 3 ], FALSE);
						else
							PurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 3 ], FALSE);
					}
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case '!':
					// 1st item on screen all at once
					if (BobbyRayCatalogueModel::IsVisibleItemSlot(
							0, gubNumItemsOnScreen))
					{
						if( fCtrl )
							UnPurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 0 ], TRUE);
						else
							PurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 0 ], TRUE);
					}
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case '@':
					// 2nd item on screen all at once
					if (BobbyRayCatalogueModel::IsVisibleItemSlot(
							1, gubNumItemsOnScreen))
					{
						if( fCtrl )
							UnPurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 1 ], TRUE);
						else
							PurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 1 ], TRUE);
					}
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case '#':
					// 3rd item on screen all at once
					if (BobbyRayCatalogueModel::IsVisibleItemSlot(
							2, gubNumItemsOnScreen))
					{
						if( fCtrl )
							UnPurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 2 ], TRUE);
						else
							PurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 2 ], TRUE);
					}
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case '$':
					// 4th item on screen all at once
					if (BobbyRayCatalogueModel::IsVisibleItemSlot(
							3, gubNumItemsOnScreen))
					{
						if( fCtrl )
							UnPurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 3 ], TRUE);
						else
							PurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 3 ], TRUE);
					}
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				default:
					HandleKeyBoardShortCutsForLapTop( InputEvent.usEvent, InputEvent.usParam, InputEvent.usKeyState );
				break;
			}
		}
		else if( InputEvent.usEvent == KEY_REPEAT )
		{
			switch( InputEvent.usParam )
			{
				case LEFTARROW:
				case 'a':
					// previous page
					gubCurPage = static_cast<UINT16>(
						BobbyRayCatalogueModel::PreviousPage(
							gubCurPage, gubNumPages));
					DeleteMouseRegionForBigImage();

					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case RIGHTARROW:
				case 'd':
					// next page
					gubCurPage = static_cast<UINT16>(
						BobbyRayCatalogueModel::NextPage(
							gubCurPage, gubNumPages));
					DeleteMouseRegionForBigImage();

					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case SHIFT_LEFTARROW:
				case 'A':
					// jump to 10 pages back
					gubCurPage = static_cast<UINT16>(
						BobbyRayCatalogueModel::PreviousPage(
							gubCurPage, gubNumPages, 10));
					DeleteMouseRegionForBigImage();

					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case SHIFT_RIGHTARROW:
				case 'D':
					// jump to 10 pages forward
					gubCurPage = static_cast<UINT16>(
						BobbyRayCatalogueModel::NextPage(
							gubCurPage, gubNumPages, 10));
					DeleteMouseRegionForBigImage();

					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case '1':
					// 1st item on screen
					if (BobbyRayCatalogueModel::IsVisibleItemSlot(
							0, gubNumItemsOnScreen))
					{
						if( fCtrl )
							UnPurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 0 ], FALSE);
						else
							PurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 0 ], FALSE);
					}
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case '2':
					// 2nd item on screen
					if (BobbyRayCatalogueModel::IsVisibleItemSlot(
							1, gubNumItemsOnScreen))
					{
						if( fCtrl )
							UnPurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 1 ], FALSE);
						else
							PurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 1 ], FALSE);
					}
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case '3':
					// 3rd item on screen
					if (BobbyRayCatalogueModel::IsVisibleItemSlot(
							2, gubNumItemsOnScreen))
					{
						if( fCtrl )
							UnPurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 2 ], FALSE);
						else
							PurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 2 ], FALSE);
					}
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case '4':
					// 4th item on screen
					if (BobbyRayCatalogueModel::IsVisibleItemSlot(
							3, gubNumItemsOnScreen))
					{
						if( fCtrl )
							UnPurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 3 ], FALSE);
						else
							PurchaseBobbyRayItem(gusItemNumberForItemsOnScreen[ 3 ], FALSE);
					}
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
			}
		}
		else
		{
			extern void HandleDefaultEvent(InputAtom *Event);
			HandleDefaultEvent(&InputEvent);
		}
	}
}

void HandleBobbyRayMouseWheel(void)
{
	// Purchase and unpurchase via mousewheel if pressing ctrl
	const std::size_t activeRegionCount = gfBigImageMouseRegionCreated
		? std::min<std::size_t>(gubNumItemsOnScreen,
			BobbyRayCatalogueModel::ItemsPerPage)
		: 0;
	for (std::size_t i = 0; i < activeRegionCount; ++i)
	{
		auto &mouseRegion = gSelectedBigImageRegion[i];
		auto wheelState = mouseRegion.WheelState * (gGameSettings.fOptions[TOPTION_INVERT_WHEEL] ? -1 : 1);
		if (mouseRegion.uiFlags & MSYS_MOUSE_IN_AREA)
		{
			if (_KeyDown(17)) // CTRL
			{
				const std::size_t itemSlot = static_cast<std::size_t>(
					MSYS_GetRegionUserData(&mouseRegion, 0));
				if (!BobbyRayCatalogueModel::IsVisibleItemSlot(
						itemSlot, activeRegionCount))
				{
					ResetWheelState(&mouseRegion);
					continue;
				}
				if (wheelState < 0)
				{
					PurchaseBobbyRayItem(
						gusItemNumberForItemsOnScreen[itemSlot], FALSE);
				}
				else if (wheelState > 0)
				{
					UnPurchaseBobbyRayItem(
						gusItemNumberForItemsOnScreen[itemSlot], FALSE);
				}
				ResetWheelState(&mouseRegion);

				fReDrawScreenFlag = TRUE;
				fPausedReDrawScreenFlag = TRUE;
			}
		}
	}

	// Scroll through pages with regular mouse wheel
	SGPPoint	MousePos;
	GetMousePos(&MousePos);
	const auto x = MousePos.iX;
	const auto y = MousePos.iY;
	if ( (LAPTOP_SCREEN_UL_X < x && x < LAPTOP_SCREEN_LR_X) && (LAPTOP_SCREEN_WEB_UL_Y < y && y < LAPTOP_SCREEN_WEB_LR_Y) )
	{
		if (!_KeyDown(17)) // CTRL
		{
			const auto Wheelstate = _WheelValue * (gGameSettings.fOptions[TOPTION_INVERT_WHEEL] ? -1 : 1);
			if (Wheelstate < 0)
			{
				const auto nextPage = BobbyRayCatalogueModel::NextPage(
					gubCurPage, gubNumPages);
				if (nextPage != gubCurPage)
				{
					gubCurPage = static_cast<UINT16>(nextPage);
					DeleteMouseRegionForBigImage();
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				}
			}
			else if (Wheelstate > 0)
			{
				const auto previousPage = BobbyRayCatalogueModel::PreviousPage(
					gubCurPage, gubNumPages);
				if (previousPage != gubCurPage)
				{
					gubCurPage = static_cast<UINT16>(previousPage);
					DeleteMouseRegionForBigImage();
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				}
			}
		}
		_WheelValue = 0;
	}
}

void GetHelpTextForItemInLaptop( CHAR16 *pzStr, UINT16 usItemNumber )
{
	if (!pzStr)
		return;
	pzStr[0] = L'\0';
	if (!IsLoadedBobbyRayItem(usItemNumber))
		return;

	const size_t ATTACHMENTS_STRBUF_SIZE = 3800;
	CHAR16	zItemName[ SIZE_ITEM_NAME ];
	UINT8	ubItemCount=0;

	//get item weight
	FLOAT fWeight = GetWeightBasedOnMetricOption(Item[ usItemNumber ].ubWeight)/10;

	switch( Item[ usItemNumber ].usItemClass )
	{
	case IC_GUN:
		{
			// HEADROCK HAM 3.6: Can now take a negative modifier.
			//UINT16	gunDamage = (UINT16)( Weapon[ usItemNumber ].ubImpact + ( (double) Weapon[ usItemNumber ].ubImpact / 100) * gGameExternalOptions.ubGunDamageMultiplier );
			UINT16		gunDamage = (UINT16)GetModifiedGunDamage( Weapon[ usItemNumber ].ubImpact );
			// modify by ini values
						gunDamage *= gItemSettings.fDamageModifierGun[ Weapon[ usItemNumber ].ubWeaponType ];
			UINT16		readyAPs = (UINT16)(( Weapon[ usItemNumber ].ubReadyTime * (100 - Item[ usItemNumber ].percentreadytimeapreduction)) / 100);
			UINT16		gunRange = (UINT16)GetModifiedGunRange(usItemNumber);

			//Calculate AP's
			CHAR16		apStr[20];
			CHAR16		apStr2[20];
			OBJECTTYPE	pObject;

			// HEADROCK HAM 3: Variables for "Possible Attachment List"
			BOOLEAN		fAttachmentsFound = FALSE;
			// Contains entire string of attachment names
			CHAR16		attachStr[ATTACHMENTS_STRBUF_SIZE];
			// Contains temporary attachment list before added to string constant from text.h
			CHAR16		attachStr3[ATTACHMENTS_STRBUF_SIZE];

			CreateItem(usItemNumber, 100, &pObject);
			INT16		ubAttackAPs = BaseAPsToShootOrStab( APBPConstants[DEFAULT_APS], APBPConstants[DEFAULT_AIMSKILL], &pObject, NULL );

			if ( Weapon[ usItemNumber ].NoSemiAuto )
				swprintf( apStr, L"-" );
			else
				swprintf( apStr, L"%d", ubAttackAPs );

			if (GetShotsPerBurst(&pObject) > 0)
			{
				swprintf( apStr2, L" / %d", ubAttackAPs + CalcAPsToBurst( APBPConstants[DEFAULT_APS], &pObject, NULL ) );
				wcscat( apStr, apStr2 );
			}
			else
				wcscat( apStr, L" / -" );

			if (GetAutofireShotsPerFiveAPs(&pObject) > 0)
			{
				swprintf( apStr2, L" / %d", ubAttackAPs + CalcAPsToAutofire( APBPConstants[DEFAULT_APS], &pObject, 3, NULL ) );
				wcscat( apStr, apStr2 );
			}
			else
				wcscat( apStr, L" / -" );

			attachStr[0] = 0;
			attachStr3[0] = 0;

			// HEADROCK HAM 3: Generate list of possible attachments to a gun (Guns only!)
			if (gGameExternalOptions.fBobbyRayTooltipsShowAttachments)
			{
				if (UsingNewAttachmentSystem())
				{
					std::pair<std::multimap<UINT16, AttachmentStruct>::iterator, std::multimap<UINT16, AttachmentStruct>::iterator> range;
					std::multimap<UINT16, AttachmentStruct>::iterator it;
					range = AttachmentBackmap.equal_range(Item[usItemNumber].uiIndex);
					for (it = range.first; it != range.second; it++)
					{
						UINT16 attachmentId = it->second.attachmentIndex;
						if (!ItemIsHiddenAddon(attachmentId) && !ItemIsHiddenAttachment(attachmentId) && ItemIsLegal(attachmentId, TRUE))
						{
							fAttachmentsFound = TRUE;
							if (DecorateAppendString(attachStr3, ATTACHMENTS_STRBUF_SIZE, Item[attachmentId].szItemName) == FALSE)
								break;
						}
					}
				}
				else  // old attachment system
				{
					for (UINT32 itemId = 1; itemId < gMAXITEMS_READ; itemId++)
					{
						// If the attachment is not hidden and attachable to the gun (usItemNumber)
						if (!ItemIsHiddenAddon(itemId) && !ItemIsHiddenAttachment(itemId) &&
							ItemIsLegal(itemId, TRUE) && IsAttachmentPointAvailable(Item[usItemNumber].uiIndex, itemId))
						{
							fAttachmentsFound = TRUE;
							if (DecorateAppendString(attachStr3, ATTACHMENTS_STRBUF_SIZE, Item[itemId].szItemName) == FALSE)
								break;
						}
					}
				}

				if (fAttachmentsFound)
				{
					DecorateAppendString(attachStr, ATTACHMENTS_STRBUF_SIZE, gWeaponStatsDesc[14], 2);  // 2 new lines and "Attachments:" title
					DecorateAppendString(attachStr, ATTACHMENTS_STRBUF_SIZE, attachStr3, 0);  // no new line, list of attachments (starts with new line)
				}
			}

			//Sum up default attachments.
			BOOLEAN fFoundDefault = FALSE;
			attachStr3[0] = 0;
			for (UINT8 cnt = 0; cnt < MAX_DEFAULT_ATTACHMENTS; cnt++)
			{
				if (Item[usItemNumber].defaultattachments[cnt] != 0)
				{
					if (DecorateAppendString(attachStr3, ATTACHMENTS_STRBUF_SIZE, Item[Item[usItemNumber].defaultattachments[cnt]].szItemName) == FALSE)
						break;
					fFoundDefault = TRUE;
				}
				else  // If we found an empty entry, we can assume the rest will be empty too.
					break;
			}

			if (fFoundDefault)
			{
				DecorateAppendString(attachStr, ATTACHMENTS_STRBUF_SIZE, gWeaponStatsDesc[17], 2);  // 2 new lines and "Default:" title
				DecorateAppendString(attachStr, ATTACHMENTS_STRBUF_SIZE, attachStr3, 0);  // no new line, list of attachments (starts with new line)
			}

			// HEADROCK HAM 3: Added last string (attachStr), for display of the possible attachment list. 
			// If the feature is deactivated, the attachStr will simply be empty at this point 
			// (remember? we emptied it earlier!).
			INT8 accuracy = (UsingNewCTHSystem()==true?Weapon[ usItemNumber ].nAccuracy:Weapon[ usItemNumber ].bAccuracy);
			sgp_swprintf( pzStr, 5000, L"%s (%s)\n%s %d\n%s %d\n%s %d\n%s (%d) %s\n%s %1.1f %s%s",
				ItemNames[ usItemNumber ],
				AmmoCaliber[ Weapon[ usItemNumber ].ubCalibre ],
				gWeaponStatsDesc[ 9 ],					//Accuracy String
				accuracy,								//Accuracy
				gWeaponStatsDesc[ 11 ],					//Damage String
				gunDamage,								//Gun damage
				gWeaponStatsDesc[ 10 ],					//Range String
				gGameSettings.fOptions[ TOPTION_SHOW_WEAPON_RANGE_IN_TILES ] ? gunRange/10 : gunRange,	//Gun Range 
				gWeaponStatsDesc[ 6 ],					//AP String
				readyAPs,
				apStr,									//AP's
				gWeaponStatsDesc[ 12 ],					//Weight String
				fWeight,								//Weight
				GetWeightUnitString(),					//Weight units
				attachStr
				);
		}
		break;

	case IC_LAUNCHER:
		{
			// HEADROCK HAM 3.6: Can now take a negative modifier.
			UINT16	gunDamage = (UINT16)GetModifiedGunDamage( Weapon[ usItemNumber ].ubImpact );
			UINT16	readyAPs = (UINT16)(( Weapon[ usItemNumber ].ubReadyTime * (100 - Item[ usItemNumber ].percentreadytimeapreduction)) / 100);
			UINT16	usRange = (UINT16)GetModifiedGunRange(usItemNumber);
			INT8	accuracy = (UsingNewCTHSystem()==true?Weapon[ usItemNumber ].nAccuracy:Weapon[ usItemNumber ].bAccuracy);
			//UINT16 gunDamage = (UINT16)( Weapon[ usItemNumber ].ubImpact + ( (double) Weapon[ usItemNumber ].ubImpact / 100) * gGameExternalOptions.ubGunDamageMultiplier );

			//Calculate AP's
			CHAR16		apStr[20];
			CHAR16		apStr2[20];
			OBJECTTYPE	pObject;

			CreateItem(usItemNumber, 100, &pObject);
			INT16		ubAttackAPs = BaseAPsToShootOrStab( APBPConstants[DEFAULT_APS], APBPConstants[DEFAULT_AIMSKILL], &pObject, NULL );

			if ( Weapon[ usItemNumber ].NoSemiAuto )
				swprintf( apStr, L"-" );
			else
				swprintf( apStr, L"%d", ubAttackAPs );

			if (GetShotsPerBurst(&pObject) > 0)
			{
				swprintf( apStr2, L" / %d", ubAttackAPs + CalcAPsToBurst( APBPConstants[DEFAULT_APS], &pObject, NULL ) );
				wcscat( apStr, apStr2 );
			}
			else
				wcscat( apStr, L" / -" );

			if (GetAutofireShotsPerFiveAPs(&pObject) > 0)
			{
				swprintf( apStr2, L" / %d", ubAttackAPs + CalcAPsToAutofire( APBPConstants[DEFAULT_APS], &pObject, 3, NULL ) );
				wcscat( apStr, apStr2 );
			}
			else
				wcscat( apStr, L" / -" );
			
			sgp_swprintf( pzStr, 5000, L"%s\n%s %d\n%s %d\n%s %d\n%s (%d) %s\n%s %1.1f %s",
				ItemNames[ usItemNumber ],
				gWeaponStatsDesc[ 9 ],					//Accuracy String
				accuracy,								//Accuracy
				gWeaponStatsDesc[ 11 ],					//Damage String
				gunDamage,								//Gun damage
				gWeaponStatsDesc[ 10 ],					//Range String
				gGameSettings.fOptions[ TOPTION_SHOW_WEAPON_RANGE_IN_TILES ] ? usRange/10 : usRange,	//Gun Range 
				gWeaponStatsDesc[ 6 ],					//AP String
				readyAPs,
				apStr,									//AP's
				gWeaponStatsDesc[ 12 ],					//Weight String
				fWeight,								//Weight
				GetWeightUnitString()					//Weight units
				);
		}
		break;

	case IC_BLADE:
	case IC_THROWING_KNIFE:
	case IC_PUNCH:
		{
			// HEADROCK HAM 3.6: Can now take a negative modifier 
			//UINT16 meleeDamage = (UINT16)( Weapon[ usItemNumber ].ubImpact + ( (double) Weapon[ usItemNumber ].ubImpact / 100) * gGameExternalOptions.ubMeleeDamageMultiplier );
			//UINT16 meleeDamage = (UINT16)GetModifiedMeleeDamage( Weapon[ usItemNumber ].ubImpact );
			OBJECTTYPE pObject;

			CreateItem(usItemNumber, 100, &pObject);
			sgp_swprintf( pzStr, 5000, L"%s\n%s %d\n%s %d\n%s %1.1f %s",
				ItemNames[ usItemNumber ],
				gWeaponStatsDesc[ 11 ],					//Damage String
				GetDamage(&pObject),					//Melee damage
				gWeaponStatsDesc[ 6 ],					//AP String
				BaseAPsToShootOrStab( APBPConstants[DEFAULT_APS], APBPConstants[DEFAULT_AIMSKILL], &pObject, NULL ), //AP's
				gWeaponStatsDesc[ 12 ],					//Weight String
				fWeight,								//Weight
				GetWeightUnitString()					//Weight units
				);
		}
		break;

	case IC_AMMO:
		{
			sgp_swprintf( pzStr, 5000, L"%s\n%s %1.1f %s",
				ItemNames[ usItemNumber ],	//Item long name
				gWeaponStatsDesc[ 12 ],			//Weight String
				fWeight,						//Weight
				GetWeightUnitString()			//Weight units
				);

			//Lal: do not delete, commented out for next version
			//sgp_swprintf( pzStr, 5000, L"%s %s %s %d [%d rnds]\n%s %1.1f %s", 				
			//	AmmoCaliber[ Magazine[ Item[usItem].ubClassIndex ].ubCalibre ],			//Ammo calibre
			//	AmmoTypes[Magazine[ Item[usItem].ubClassIndex ].ubAmmoType].ammoName,	//Ammo type
			//	MagNames[Magazine[ Item[usItem].ubClassIndex ].ubMagType],				//Magazine type
			//	Magazine[ Item[usItem].ubClassIndex ].ubMagSize,						//Magazine capacity
			//	(*pObject)[0]->data.ubShotsLeft,	//Shots left
			//	gWeaponStatsDesc[ 12 ],		//Weight String
			//	fWeight,					//Weight
			//	GetWeightUnitString()		//Weight units
			//	);

			//specify the help text only if the items is ammo
			//and only if the user has an item that can use the particular type of ammo
			ubItemCount = CheckPlayersInventoryForGunMatchingGivenAmmoID( usItemNumber );
			if( ubItemCount != 0 )
			{
				swprintf( zItemName, L"\n%s %d %s",BobbyRText[BOBBYR_GUNS_NUM_GUNS_THAT_USE_AMMO_1], ubItemCount, BobbyRText[BOBBYR_GUNS_NUM_GUNS_THAT_USE_AMMO_2] );
				wcscat(	pzStr, zItemName );
			}
		}
		break;

	case IC_GRENADE:
	case IC_BOMB:
		// explosives
		//if ( (Item[ usItemNumber ].usItemClass == IC_GRENADE)||(Item[ usItemNumber ].usItemClass == IC_BOMB) )
		{
			// HEADROCK HAM 3.6: Can now use negative modifier.
			//UINT16 explDamage = (UINT16)( Explosive[Item[ usItemNumber ].ubClassIndex].ubDamage + ( (double) Explosive[Item[ usItemNumber ].ubClassIndex].ubDamage / 100) * gGameExternalOptions.bExplosivesDamageModifier );
			//UINT16 explStunDamage = (UINT16)( Explosive[Item[ usItemNumber ].ubClassIndex].ubStunDamage + ( (double) Explosive[Item[ usItemNumber ].ubClassIndex].ubStunDamage / 100) * gGameExternalOptions.ubExplosivesDamageMultiplier );
			UINT16 explDamage = (UINT16) GetModifiedExplosiveDamage( Explosive[Item[ usItemNumber ].ubClassIndex].ubDamage, 0 );
			UINT16 explStunDamage = (UINT16) GetModifiedExplosiveDamage( Explosive[Item[ usItemNumber ].ubClassIndex].ubStunDamage, 1 );


			sgp_swprintf( pzStr, 5000, L"%s\n%s %d\n%s %d\n%s %1.1f %s",
				ItemNames[ usItemNumber ],
				gWeaponStatsDesc[ 11 ],		//Damage String
				explDamage,					//Expl damage
				gWeaponStatsDesc[ 13 ],		//Stun Damage String
				explStunDamage,				//Stun Damage
				gWeaponStatsDesc[ 12 ],		//Weight String
				fWeight,					//Weight
				GetWeightUnitString()		//Weight units
				);
		}
		break;

	case IC_ARMOUR:
		//Armor
		//if (Item[ usItemNumber ].usItemClass == IC_ARMOUR)
		{
			INT32 iProtection = Armour[ Item[ usItemNumber ].ubClassIndex ].ubProtection;

			switch( Armour[ Item[ usItemNumber ].ubClassIndex ].ubArmourClass )
			{
			case( ARMOURCLASS_HELMET ):
				iProtection = 15 * iProtection / Armour[ Item[ SPECTRA_HELMET_18 ].ubClassIndex ].ubProtection;
				break;

			case( ARMOURCLASS_VEST ):
				iProtection = 65 * iProtection / ( Armour[ Item[ SPECTRA_VEST_18 ].ubClassIndex ].ubProtection + Armour[ Item[ CERAMIC_PLATES ].ubClassIndex ].ubProtection );
				break;

			case( ARMOURCLASS_LEGGINGS ):
				iProtection = 25 * iProtection / Armour[ Item[ SPECTRA_LEGGINGS_18 ].ubClassIndex ].ubProtection;
				break;

			case( ARMOURCLASS_PLATE ):
				iProtection = 65 * iProtection / ( Armour[ Item[ CERAMIC_PLATES ].ubClassIndex ].ubProtection );
				break;
			}

			sgp_swprintf( pzStr, 5000, L"%s\n%s %d%% (%d)\n%s %d%%\n%s %1.1f %s",
				ItemNames[ usItemNumber ],									//Item long name
				pInvPanelTitleStrings[ 4 ],										//Protection string
				iProtection,													//Protection rating in % based on best armor
				Armour[ Item[ usItemNumber ].ubClassIndex ].ubProtection,	//Protection (raw data)
				pInvPanelTitleStrings[ 3 ],				//Camo string
				Item[ usItemNumber ].camobonus,	//Camo bonus
				gWeaponStatsDesc[ 12 ],					//Weight string
				fWeight,								//Weight
				GetWeightUnitString()					//Weight units
				);
		}
		break;

	// rftr: better LBE tooltips
	case IC_LBEGEAR:
		if(gGameExternalOptions.fBobbyRayTooltipsShowLBEDetails)
		{
			CHAR16 lbeStr[1000];
			CHAR16 temp[1000];

			swprintf(lbeStr, L"");
			swprintf(temp, L"");

			if (UsingNewInventorySystem() &&
				BobbyRayCatalogueModel::IsCatalogueIndexInBounds(
					Item[usItemNumber].ubClassIndex,
					LoadBearingEquipment.size()))
			{
				const INVTYPE& item = Item[usItemNumber];
				const LBETYPE& lbe = LoadBearingEquipment[item.ubClassIndex];
				const bool validLbeClass = lbe.lbeClass >= THIGH_PACK &&
					lbe.lbeClass <= OTHER_POCKET;
				const auto firstPocket = std::find_if(
					lbe.lbePocketIndex.begin(), lbe.lbePocketIndex.end(),
					[](UINT8 pocket) { return pocket != 0; });
				const std::size_t firstPocketIndex =
					firstPocket == lbe.lbePocketIndex.end() ? 0 : *firstPocket;
				const UINT8 firstPocketVolume =
					BobbyRayCatalogueModel::IsCatalogueIndexInBounds(
						firstPocketIndex, LBEPocketType.size())
						? LBEPocketType[firstPocketIndex].pVolume : 0;

				if (lbe.lbeAvailableVolume > 0 && validLbeClass) // if this item is a MOLLE carrier (thigh rig, vest, combat pack, backpack)
				{
					swprintf(temp, L"\n \n%s\n%s %d", gLbeStatsDesc[6 + lbe.lbeClass], gLbeStatsDesc[0], lbe.lbeAvailableVolume);
					wcscat(lbeStr, temp);

					UINT8 molleSmallCount = 0;
					UINT8 molleMediumCount = 0;

					for (UINT32 sCount = 1; sCount < gMAXITEMS_READ; ++sCount)
					{
						AttachmentSlotStruct attachmentSlot = AttachmentSlots[sCount];
						if (attachmentSlot.uiSlotIndex == 0)
							break;

						// count MOLLE slots for this carrier
						if (attachmentSlot.nasLayoutClass == ((UINT64)1 << (lbe.lbeClass + 1)))
						{
							const std::size_t pocketBit =
								static_cast<std::size_t>(attachmentSlot.ubPocketMapping) - 1;
							if (!BobbyRayCatalogueModel::IsCatalogueIndexInBounds(
									pocketBit, sizeof(lbe.lbePocketsAvailable) * 8))
							{
								continue;
							}
							if (lbe.lbePocketsAvailable &
								(static_cast<UINT16>(1) << pocketBit))
							{
								// seems to be equivalent to checking attachmentSlot.fBigSlot
								if (attachmentSlot.nasAttachmentClass == MOLLE_SMALL)
								{
									molleSmallCount++;
								}
								else if (attachmentSlot.nasAttachmentClass == MOLLE_MEDIUM)
								{
									molleMediumCount++;
								}
							}
						}
					}

					if (molleSmallCount > 0)
					{
						swprintf(temp, L"\n%s %d", gLbeStatsDesc[2], molleSmallCount);
						wcscat(lbeStr, temp);
					}

					if (molleMediumCount > 0)
					{
						swprintf(temp, L"\n%s %d", gLbeStatsDesc[3], molleMediumCount);
						wcscat(lbeStr, temp);
					}
				}
				else if (item.nasAttachmentClass == MOLLE_SMALL)
				{
					swprintf(temp, L"\n \n%s\n%s\n%s %d", gLbeStatsDesc[11], gLbeStatsDesc[4], gLbeStatsDesc[1], firstPocketVolume);
					wcscat(lbeStr, temp);
				}
				else if (item.nasAttachmentClass == MOLLE_MEDIUM)
				{
					// special case for hydration pack (see AttachmentPoint.xml)
					swprintf(temp, L"\n \n%s\n%s\n%s %d", gLbeStatsDesc[11], item.ulAttachmentPoint == 4 ? gLbeStatsDesc[6] : gLbeStatsDesc[5], gLbeStatsDesc[1], firstPocketVolume);
					wcscat(lbeStr, temp);
				}
				else if (validLbeClass) // non-MOLLE LBE
				{
					swprintf(temp, L"\n \n%s", gLbeStatsDesc[6 + lbe.lbeClass]);
					wcscat(lbeStr, temp);
				}

				// check for combat pack/backpack combos
				if (lbe.lbeCombo > 0)
				{
					bool foundCombo = false;
					CHAR16 lbeComboStr[1000];
					swprintf(lbeComboStr, L"");
					for (UINT32 itemIndex = 0; itemIndex < gMAXITEMS_READ; ++itemIndex)
					{
						INVTYPE otherItem = Item[itemIndex];

						if (otherItem.usItemClass != IC_LBEGEAR)
							continue;

						if (itemIndex == usItemNumber)
							continue;

						if (!BobbyRayCatalogueModel::IsCatalogueIndexInBounds(
								otherItem.ubClassIndex,
								LoadBearingEquipment.size()))
						{
							continue;
						}
						const LBETYPE& otherLbe =
							LoadBearingEquipment[otherItem.ubClassIndex];

						if (lbe.lbeClass == otherLbe.lbeClass)
							continue;

						if (lbe.lbeCombo == otherLbe.lbeCombo)
						{
							foundCombo = true;
							if (wcslen(lbeComboStr) + wcslen(otherItem.szBRName) < 800)
							{
								swprintf(temp, L"\n%s", otherItem.szBRName);
								wcscat(lbeComboStr, temp);
							}
							else
							{
								wcscat(lbeComboStr, L"\n...");
								break;
							}
						}
					}

					if (foundCombo)
					{
						// only combat packs and backpacks will have lbeCombo
						swprintf(temp, L"\n \n%s\n%s", gLbeStatsDesc[lbe.lbeClass == COMBAT_PACK ? 12 : 13], lbeComboStr);
						wcscat(lbeStr, temp);
					}
				}
			}

			sgp_swprintf(pzStr, 5000, L"%s\n%s %1.1f %s%s",
				ItemNames[usItemNumber],	//Item long name
				gWeaponStatsDesc[12],		//Weight String
				fWeight,					//Weight
				GetWeightUnitString(),		//Weight units
				lbeStr
			);
		}
	break;

	case IC_MISC:
	case IC_MEDKIT:
	case IC_KIT:
	case IC_FACE:
	default:
		// The final, and typical case, is that of an item with a percent status
		{
			sgp_swprintf( pzStr, 5000, L"%s\n%s %1.1f %s",
				ItemNames[ usItemNumber ],	//Item long name
				gWeaponStatsDesc[ 12 ],			//Weight String
				fWeight,						//Weight
				GetWeightUnitString()			//Weight units
				);
		}
		break;
	}
}
