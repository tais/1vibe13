	#include "laptop.h"
	#include "AimMembers.h"
	#include "aim.h"
	#include "Utilities.h"
	#include "WCheck.h"
	#include "DEBUG.H"
	#include "stdio.h"
	#include "sysutil.h"
	#include "Soldier Profile.h"
#include "TacticalActor.h"
#include "Soldier Profile Constants.h"
	#include "SoldierRepository.h"
	#include "Interface Items.h"
	#include "Overhead.h"
	#include "WordWrap.h"
	#include "finances.h"
	#include "vsurface.h"
	#include "vobject.h"
	#include "CampaignClockAdapter.h"
	#include "Game Clock.h"
	#include "Overhead.h"
	#include "faces.h"
	#include "Dialogue Control.h"
	#include "Text.h"
	#include "history.h"
	#include "Game Event Hook.h"
	#include "MercTextBox.h"
	#include "Render Dirty.h"
	#include "Weapons.h"
	#include "Soldier Add.h"
	#include "Merc Hiring.h"
	#include "strategic.h"
	#include "AimFacialIndex.h"
	#include "LaptopSave.h"
	#include "english.h"
	#include "GameSettings.h"
	#include "CampaignAimSitePolicy.h"
	#include "GameContext.h"
	#include "random.h"
	#include "Strategic Status.h"
	#include "Merc Contract.h"
	#include "Strategic Merc Handler.h"
	#include "Assignments.h"
	#include "Sound Control.h"
	#include "Quests.h"
	#include "strategicmap.h"
	#include "personnel.h"
	#include "Encyclopedia_new.h"	//update encyclopedia item visibility when viewing that item

#include "Strategic Town Loyalty.h"
#include "connect.h"
#include "fresh_header.h"
#include "Encrypted File.h"
#include "InterfaceItemImages.h"
#include "LaptopPageResourceOwner.h"
#include "AimMemberProfileLayout.h"
#include "AimWebsiteLayout.h"
#include <sstream>
#include <vector>
#include <language.hpp>

//
//******	Defines	******
//


#define	MERCBIOSFILENAME		"BINARYDATA\\aimbios.edt"



#define	AIM_M_FONT_PREV_NEXT_CONTACT		FONT14ARIAL
#define	AIM_M_FONT_PREV_NEXT_CONTACT_COLOR_UP		FONT_MCOLOR_DKWHITE
#define	AIM_M_FONT_PREV_NEXT_CONTACT_COLOR_DOWN		138
#define AIM_M_FONT_STATIC_TEXT					FONT12ARIAL
#define AIM_M_COLOR_STATIC_TEXT							146
#define AIM_M_FONT_DYNAMIC_TEXT					FONT10ARIAL
#define AIM_M_COLOR_DYNAMIC_TEXT				FONT_MCOLOR_WHITE
#define AIM_M_WEAPON_TEXT_FONT					FONT10ARIAL
#define AIM_M_WEAPON_TEXT_COLOR					FONT_MCOLOR_WHITE
#define AIM_M_NUMBER_FONT								FONT12ARIAL
#define AIM_M_NUMBER_COLOR							FONT_MCOLOR_WHITE
#define	AIM_M_KIT_BUTTON_FONT						FONT12ARIAL
#define	AIM_M_KIT_BUTTON_UP_COLOR					FONT_MCOLOR_WHITE
#define	AIM_M_KIT_BUTTON_DOWN_COLOR					138
#define	AIM_M_ACTIVE_MEMBER_TITLE_COLOR				AIM_GREEN
#define	AIM_M_FEE_CONTRACT_COLOR					AIM_GREEN
#define	AIM_M_VIDEO_TITLE_COLOR						AIM_FONT_GOLD
#define	AIM_M_VIDEO_NAME_COLOR						FONT_MCOLOR_BLACK
#define	AIM_M_VIDEO_NAME_SHADOWCOLOR				AIM_FONT_GOLD


#define	AIM_M_VIDEO_CONTRACT_LENGTH_FONT	FONT12ARIAL
#define	AIM_M_VIDEO_CONTRACT_LENGTH_COLOR	FONT_MCOLOR_BLACK

#define	AIM_M_VIDEO_CONTRACT_AMOUNT_FONT	FONT10ARIAL
#define	AIM_M_VIDEO_CONTRACT_AMOUNT_COLOR	183

#define	AIM_POPUP_BOX_FONT							FONT12ARIAL
#define	AIM_POPUP_BOX_COLOR							FONT_MCOLOR_BLACK

#define	HIGH_STAT_COLOR									FONT_MCOLOR_WHITE
#define	MED_STAT_COLOR									FONT_MCOLOR_DKWHITE
#define	LOW_STAT_COLOR									FONT_MCOLOR_DKGRAY

#define	SIZE_MERC_BIO_INFO	400	* 2
#define SIZE_MERC_ADDITIONAL_INFO 160 * 2

#define	MERC_ANNOYED_WONT_CONTACT_TIME_MINUTES	6 * 60
#define	NUMBER_HATED_MERCS_ONTEAM							6

// Fixed-pixel profile and video-call geometry lives in the dependency-free
// A.I.M. layout models. Rendering, controls, and animation consume the same
// selected layouts.

#define		AIM_CONTRACT_LENGTH_ONE_DAY				0
#define		AIM_CONTRACT_LENGTH_ONE_WEEK			1
#define		AIM_CONTRACT_LENGTH_TWO_WEEKS			2

#define		AIM_POPUP_BOX_SUCCESS							0
#define		AIM_POPUP_BOX_FAILURE							1

#define		VC_CONTACT_STATIC_TIME						30
#define		VC_CONTACT_FUZZY_LINE_TIME				100
#define		VC_NUM_LINES_SNOW									6
#define		VC_NUM_FUZZ_LINES									10
#define		VC_NUM_STRAIGHT_LINES							9

#define		QUOTE_FIRST_ATTITUDE_TIME				3000
#define		QUOTE_ATTITUDE_TIME							10000

#define		QUOTE_DELAY_SMALL_TALK					1
#define		QUOTE_DELAY_IMPATIENT_TALK			2
#define		QUOTE_DELAY_VERY_IMPATIENT_TALK	3
#define		QUOTE_DELAY_HANGUP_TALK					4
#define		QUOTE_DELAY_NO_ACTION						5
#define		QUOTE_MERC_BUSY									6

#define		TEXT_POPUP_STRING_SIZE						512

#define		MINIMUM_TALKING_TIME_FOR_MERC			1500

#define		AIM_TEXT_SPEECH_MODIFIER					80

#define		AIM_WEAPONBOX_NAME_WIDTH					93


// enumerated types used for the Video Conferencing Display
enum
{
	AIM_VIDEO_NOT_DISPLAYED_MODE,							// The video popup is not displayed
	AIM_VIDEO_POPUP_MODE,											// The title bar pops up out of the Contact button
	AIM_VIDEO_INIT_MODE,											// When the player first tries to contact the merc, it will be snowy for a bit
	AIM_VIDEO_FIRST_CONTACT_MERC_MODE,				// The popup that is displayed when first contactinf the merc
	AIM_VIDEO_HIRE_MERC_MODE,									// The popup which deals with the contract length, and transfer funds
	AIM_VIDEO_MERC_ANSWERING_MACHINE_MODE,		// The popup which will be instread of the AIM_VIDEO_FIRST_CONTACT_MERC_MODE if the merc is not there
	AIM_VIDEO_MERC_UNAVAILABLE_MODE,					// The popup which will be instread of the AIM_VIDEO_FIRST_CONTACT_MERC_MODE if the merc is unavailable
	AIM_VIDEO_POPDOWN_MODE,										// The title bars pops down to the contact button
};


// Enumerated types used for the Pop Up Box
enum
{
	AIM_POPUP_NOTHING,
	AIM_POPUP_CREATE,
	AIM_POPUP_DISPLAY,
	AIM_POPUP_DELETE,
};

// Enumerated Types used for the different types of video distortion applied to the video face
enum
{
	VC_NO_STATIC,
	VC_FUZZY_LINE,
	VC_STRAIGHTLINE,
	VC_STATIC_IMAGE,
	VC_BW_SNOW,
	VC_PIXELATE,
	VC_TRANS_SNOW_IN,			// fade from clear to snowy
	VC_TRANS_SNOW_OUT,		// fade from snowy to clear
};


//Image Identifiers
UINT32		guiStats;
UINT32		guiPrice;
UINT32		guiPortrait;
UINT32		guiWeaponBox;
UINT32		guiFace;
//UINT32		guiVideoFace;
//UINT32		guiContactButton;
UINT32		guiVideoConfPopup;
UINT32		guiVideoConfTerminal;
UINT32		guiPopUpBox;
UINT32		guiVideoFaceBackground;
UINT32		guiBWSnow;
UINT32		guiFuzzLine;
UINT32		guiStraightLine;
UINT32		guiTransSnow;
UINT32		guiVideoContractCharge;
//UINT32		guiAnsweringMachineImage;
UINT32		guiVideoTitleBar;
INT32			iAimMembersBoxId = -1;

UINT8			gbCurrentSoldier = 0;
UINT8			gbCurrentSoldierBio = 0;

UINT8			gbCurrentIndex = 0;

UINT8			gubVideoConferencingMode;
UINT8			gubVideoConferencingPreviousMode;
BOOLEAN		gfJustSwitchedVideoConferenceMode;

BOOLEAN		gfMercIsTalking=FALSE;
BOOLEAN		gfVideoFaceActive=FALSE;

UINT8			gubPopUpBoxAction = AIM_POPUP_NOTHING;
BOOLEAN		gfRedrawScreen = FALSE;
extern UINT8	gubContractLength; // symbol already declared globally in Merc Contract.cpp (jonathanl)
BOOLEAN		gfBuyEquipment;
INT32			giContractAmount=0;
INT32			giMercFaceIndex;
CHAR16		gsTalkingMercText[ TEXT_POPUP_STRING_SIZE ];
UINT32		guiTimeThatMercStartedTalking;
UINT32		guiLastHandleMercTime;
BOOLEAN		gfFirstTimeInContactScreen;

UINT8			gubCurrentCount;
UINT8			gubCurrentStaticMode;
UINT32		guiMercAttitudeTime;			//retains the amount of time the user is in a screen, if over a certain time, the merc gets miffed
UINT8			gubMercAttitudeLevel;			//retains the current level the merc is	P.O.'ed at the caller.
BOOLEAN		gfHangUpMerc;							// if we have to cancel the video conferencing after the merc is finsihed talking
BOOLEAN		gfIsShutUpMouseRegionActive;
BOOLEAN		gfIsAnsweringMachineActive;
BOOLEAN		gfRenderTopLevel;
BOOLEAN		gfStopMercFromTalking;

UINT16		usAimMercSpeechDuration=0;

BOOLEAN		gfIsNewMailFlagSet = FALSE;

extern		UINT8			gubBasicInventoryPositions[];
extern		BOOLEAN fExitDueToMessageBox;

BOOLEAN		gfWaitingForMercToStopTalkingOrUserToClick=FALSE;

INT32			giIdOfLastHiredMerc = -1;

BOOLEAN		gfAimMemberDisplayFaceHelpText = FALSE;


BOOLEAN		gfAimMemberCanMercSayOpeningQuote = TRUE;

////////////////////////////////////////////////////////////////
//
//	Mouse and Buttons
//
////////////////////////////////////////////////////////////////

//Graphic for following
INT32		guiPreviousContactNextButtonImage;

//tais: nsgi, gearkit selection buttons and eventhandlers
INT32	giWeaponboxSelectionButton[NUM_MERCSTARTINGGEAR_KITS];

void BtnWeaponboxSelectButtonCallback(GUI_BUTTON *btn,INT32 reason);

//Previous Button
void BtnPreviousButtonCallback(GUI_BUTTON *btn,INT32 reason);
INT32	giPreviousButton;

//Contact
void BtnContactButtonCallback(GUI_BUTTON *btn,INT32 reason);
INT32	giContactButton;

//Next
void BtnNextButtonCallback(GUI_BUTTON *btn,INT32 reason);
INT32	giNextButton;

//Video conference buttons
INT32		guiVideoConferenceButtonImage[3];

//Contract Length Button
void BtnContractLengthButtonCallback(GUI_BUTTON *btn,INT32 reason);
INT32	giContractLengthButton[3];

//BuyEquipment Button
void BtnBuyEquipmentButtonCallback(GUI_BUTTON *btn,INT32 reason);
INT32	giBuyEquipmentButton[2];

//Authorize Payment Button
void BtnAuthorizeButtonCallback(GUI_BUTTON *btn,INT32 reason);
INT32	giAuthorizeButton[2];

//Hang up Button
void BtnHangUpButtonCallback(GUI_BUTTON *btn,INT32 reason);
INT32	giHangUpButton;

// PopupBox button
void BtnPopUpOkButtonCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiPopUpOkButton;
INT32		guiPopUpImage;

//First Contact Screen, Goto Hire merc Button
void BtnFirstContactButtonCallback(GUI_BUTTON *btn,INT32 reason);
INT32	giFirstContactButton[2];

//Leave Message merc Button
void BtnAnsweringMachineButtonCallback(GUI_BUTTON *btn,INT32 reason);
INT32	giAnsweringMachineButton[2];

//X to Close the video conference Button
INT32		giXToCloseVideoConfButtonImage;
void BtnXToCloseVideoConfButtonCallback(GUI_BUTTON *btn,INT32 reason);
INT32	giXToCloseVideoConfButton;


//Mouse Regions
//Clicking on guys Face
MOUSE_REGION	gSelectedFaceRegion;
void SelectFaceRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );
void SelectFaceMovementRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//Clicking To shut merc up
MOUSE_REGION	gSelectedShutUpMercRegion;
void SelectShutUpMercRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//tais, external callback for nsgi tooltips
extern void HelpTextDoneCallback( void );

//tais: tooltip regions for weaponbox images
MOUSE_REGION	gWeaponboxFasthelpRegion[
	AimMemberProfileLayoutModel::kExpandedInventoryCapacity];
extern void GetHelpTextForItemInLaptop( CHAR16 *pzStr, UINT16 usItemNumber );


//*******************************************
//
//	Function Prototypes
//
//*******************************************

using AimMemberProfileLayoutModel::Layout;

BOOLEAN	UpdateMercInfo(const Layout& layout);
BOOLEAN LoadMercBioInfo(UINT8 ubIndex, CHAR16 *pInfoString, CHAR16 *pAddInfo);
BOOLEAN DisplayMercsInventory(UINT8 ubMercID);
BOOLEAN DisplayMercsInventory(UINT8 ubMercID, const Layout& layout);
BOOLEAN DisplayMercsFace(const Layout& layout);
void		DisplayMercStats(const Layout& layout);
INT8		AimMemberHireMerc();
BOOLEAN DisplayVideoConferencingDisplay();
BOOLEAN DisplayMercsVideoFace();
void		DisplaySelectLights(BOOLEAN fContractDown, BOOLEAN fBuyEquipDown);
UINT32	DisplayMercChargeAmount();
BOOLEAN InitCreateDeleteAimPopUpBox(UINT8 ubFlag, STR16 sString1, STR16 sString2, UINT8 ubData);
BOOLEAN InitVideoFaceTalking(UINT8 ubMercID, UINT16 usQuoteNum);
BOOLEAN InitVideoFace(UINT8 ubMercID);
BOOLEAN	DisplaySnowBackground();
UINT8		WillMercAcceptCall();
void		HandleVideoDistortion();
UINT8		DisplayDistortionLine(UINT8 ubMode, UINT32 uiImageIdentifier, UINT8 ubMaxImages);
UINT8		DisplayPixelatedImage(UINT8 ubMaxImages);
void		HandleMercAttitude();
void		StopMercTalking();
UINT8		DisplayTransparentSnow(UINT8 ubMode, UINT32 uiImageIdentifier, UINT8 ubMaxImages, BOOLEAN bForward);

//tais: nsgi, kit selection
BOOLEAN CreateWeaponBoxMouseRegions(
	LaptopPageResourceOwner& resources, const Layout& layout);
BOOLEAN CreateKitSelectionButtons(
	LaptopPageResourceOwner& resources, const Layout& layout);
void CreateWeaponBoxBackground(const Layout& layout);
void EnableWeaponKitSelectionButtons();
void RefreshWeaponKitSelectionButtons();
void DisableWeaponKitSelectionButtons();
void WeaponKitSelectionUpdate(UINT8 selectedInventory);

BOOLEAN InitDeleteVideoConferencePopUp( );
BOOLEAN DeleteVideoConfPopUp();
BOOLEAN HandleCurrentVideoConfMode();

BOOLEAN EnableDisableCurrentVideoConferenceButtons( BOOLEAN fEnable);

//BOOLEAN DisplayAnimatedAnsweringMachineMsg( BOOLEAN fInit, UINT8 ubNumSubImages);
//BOOLEAN HandleAnsweringMachineMessage();


BOOLEAN CanMercBeHired();
BOOLEAN DisplayMovingTitleBar(BOOLEAN fForward, BOOLEAN fInit );
BOOLEAN DisplayBlackBackground(UINT8 ubMaxNumOfLoops);
void		DisplayDots(UINT16 usNameX, UINT16 usNameY, UINT16 usStatX, STR16 pString);

void		DelayMercSpeech( UINT8 ubMercID, UINT16 usQuoteNum, UINT16 usDelay, BOOLEAN fNewQuote, BOOLEAN fReset );
void		DisplayPopUpBoxExplainingMercArrivalLocationAndTimeCallBack( UINT8 bExitValue );
void		DisplayAimMemberClickOnFaceHelpText(const Layout& layout);

//ppp

UINT8	GetStatColor( INT8 bStat );



#ifdef JA2TESTVERSION
	BOOLEAN QuickHireMerc();
	void TempHandleAimMemberKeyBoardInput();
	extern	void SetFlagToForceHireMerc( BOOLEAN fForceHire );
#endif

//Hotkey Assignment
void HandleAimMemberKeyBoardInput();
void HandleAimMemberMouseInput(void);

void WaitForMercToFinishTalkingOrUserToClick();

namespace
{
LaptopPageResourceOwner gAimMembersResources;
LaptopPageResourceOwner gAimMembersPopupResources;
LaptopPageResourceOwner gAimMembersVideoConferenceResources;
LaptopPageResourceOwner gAimMembersVideoCloseResources;

Layout CurrentAimMemberProfileLayout()
{
	return AimMemberProfileLayoutModel::MakeLayout(
		gGameExternalOptions.gfUseNewStartingGearInterface != FALSE,
		{iScreenWidthOffset, iScreenHeightOffset,
		 LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y,
		 LAPTOP_SCREEN_WEB_DELTA_Y});
}

AimWebsiteLayoutModel::VideoConferenceLayout
CurrentAimVideoConferenceLayout()
{
	return AimWebsiteLayoutModel::MakeVideoConferenceLayout(
		{iScreenWidthOffset, iScreenHeightOffset,
		 LAPTOP_SCREEN_WEB_DELTA_Y});
}

void InvalidateAimMemberContactButton()
{
	const auto contact = CurrentAimMemberProfileLayout().navigation.contactButton;
	InvalidateRegion(contact.x, contact.y, contact.right(), contact.bottom());
}
}


//*******************************************
//
//	FUNCTIONS
//
//*******************************************

void GameInitAIMMembers()
{
}

void EnterInitAimMembers()
{
	gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;
	gubVideoConferencingPreviousMode = AIM_VIDEO_NOT_DISPLAYED_MODE;
	gfVideoFaceActive = FALSE;
//fShouldMercTalk = FALSE;
	gubPopUpBoxAction = AIM_POPUP_NOTHING;
	gfRedrawScreen = FALSE;
	giContractAmount = 0;
	giMercFaceIndex = 0;
	guiLastHandleMercTime = GetJA2NoPauseClock();	// match the no-pause clock used by the snow/black VC opening animation (consistent first-frame delta)
	gubCurrentCount = 0;
	gfFirstTimeInContactScreen = TRUE;

	//reset the variable so a pop up can be displyed this time in laptop
	LaptopSaveInfo.sLastHiredMerc.fHaveDisplayedPopUpInLaptop = FALSE;

		//reset the id of the last merc
	LaptopSaveInfo.sLastHiredMerc.iIdOfMerc = -1;
}

BOOLEAN EnterAIMMembers()
{
	VOBJECT_DESC	VObjectDesc;
	VSURFACE_DESC		vs_desc;
	LaptopPageResourceOwner stagedResources;
	const Layout layout = CurrentAimMemberProfileLayout();
	const auto videoLayout = CurrentAimVideoConferenceLayout();

	gAimMembersPopupResources.clear();
	gAimMembersVideoConferenceResources.clear();
	gAimMembersVideoCloseResources.clear();
	gAimMembersResources.clear();

	// Create a background video surface to blt the face onto
	vs_desc.fCreateFlags = VSURFACE_CREATE_DEFAULT | VSURFACE_SYSTEM_MEM_USAGE;
	vs_desc.usWidth = videoLayout.face.width;
	vs_desc.usHeight = videoLayout.face.height;
	vs_desc.ubBitDepth = 16;
	CHECKF(stagedResources.addVideoSurface(&vs_desc, guiVideoFaceBackground));

	// load the stats graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\stats.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiStats));

	// load the Price graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\price.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiPrice));

	// load the Portait graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\portrait.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiPortrait));

	// load the WeaponBox graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	if(layout.expandedGear) FilenameForBPP("LAPTOP\\newweaponbox.sti", VObjectDesc.ImageFile);
	else FilenameForBPP("LAPTOP\\weaponbox.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiWeaponBox));

	// load the videoconf Popup graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\VideoConfPopup.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiVideoConfPopup));

	// load the video conf terminal graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\VideoConfTerminal.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiVideoConfTerminal));

	// load the background snow for the video conf terminal
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\BWSnow.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiBWSnow));

	// load the fuzzy line for the video conf terminal
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\FuzzLine.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiFuzzLine));

	// load the line distortion for the video conf terminal
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\LineInterference.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiStraightLine));

	// load the translucent snow for the video conf terminal
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\TransSnow.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiTransSnow));

	// load the translucent snow for the video conf terminal
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\VideoContractCharge.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiVideoContractCharge));


	//** Mouse Regions **
	MSYS_DefineRegion(&gSelectedFaceRegion,
		layout.portrait.x, layout.portrait.y,
		layout.portrait.right(), layout.portrait.bottom(),
		MSYS_PRIORITY_HIGH, CURSOR_WWW,
		SelectFaceMovementRegionCallBack, SelectFaceRegionCallBack);
	CHECKF(stagedResources.addRegion(gSelectedFaceRegion));

	//tais: nsgi, create mousregions for tooltips
	if(layout.expandedGear)
		CHECKF(CreateWeaponBoxMouseRegions(stagedResources, layout));

	//Set the fast help for the mouse region
	//	SetRegionFastHelpText( &gSelectedFaceRegion, AimMemberText[ AIM_MEMBER_CLICK_INSTRUCTIONS ] );


	// if user clicks in the area, the merc will shut up!
	MSYS_DefineRegion( &gSelectedShutUpMercRegion, LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y ,LAPTOP_SCREEN_LR_X, LAPTOP_SCREEN_WEB_LR_Y, MSYS_PRIORITY_HIGH-1,
							CURSOR_LAPTOP_SCREEN, MSYS_NO_CALLBACK, SelectShutUpMercRegionCallBack);
	CHECKF(stagedResources.addRegion(gSelectedShutUpMercRegion));
	//have it disbled at first
	MSYS_DisableRegion(&gSelectedShutUpMercRegion);


	//Button Regions
	CHECKF(stagedResources.addButtonImage(
		LoadButtonImageOwned("LAPTOP\\x_button.sti", -1,0,-1,1,-1),
		giXToCloseVideoConfButtonImage));
	CHECKF(stagedResources.addButtonImage(
		LoadButtonImageOwned("LAPTOP\\BottomButtons2.sti", -1,0,-1,1,-1),
		guiPreviousContactNextButtonImage));

	const INT32 previousButton = CreateIconAndTextButton( guiPreviousContactNextButtonImage, CharacterInfo[AIM_MEMBER_PREVIOUS], AIM_M_FONT_PREV_NEXT_CONTACT,
														AIM_M_FONT_PREV_NEXT_CONTACT_COLOR_UP, DEFAULT_SHADOW,
														AIM_M_FONT_PREV_NEXT_CONTACT_COLOR_DOWN, DEFAULT_SHADOW,
														TEXT_CJUSTIFIED,
												layout.navigation.previousButton.x,
												layout.navigation.previousButton.y,
												BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, BtnPreviousButtonCallback);
	CHECKF(stagedResources.addButton(previousButton, giPreviousButton));
	SetButtonCursor(giPreviousButton, CURSOR_WWW );


	const INT32 contactButton = CreateIconAndTextButton( guiPreviousContactNextButtonImage, CharacterInfo[AIM_MEMBER_CONTACT], AIM_M_FONT_PREV_NEXT_CONTACT,
														AIM_M_FONT_PREV_NEXT_CONTACT_COLOR_UP, DEFAULT_SHADOW,
														AIM_M_FONT_PREV_NEXT_CONTACT_COLOR_DOWN, DEFAULT_SHADOW,
														TEXT_CJUSTIFIED,
												layout.navigation.contactButton.x,
												layout.navigation.contactButton.y,
												BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, BtnContactButtonCallback);
	CHECKF(stagedResources.addButton(contactButton, giContactButton));
	SetButtonCursor(giContactButton, CURSOR_WWW );



	const INT32 nextButton = CreateIconAndTextButton( guiPreviousContactNextButtonImage, CharacterInfo[AIM_MEMBER_NEXT], AIM_M_FONT_PREV_NEXT_CONTACT,
														AIM_M_FONT_PREV_NEXT_CONTACT_COLOR_UP, DEFAULT_SHADOW,
														AIM_M_FONT_PREV_NEXT_CONTACT_COLOR_DOWN, DEFAULT_SHADOW,
														TEXT_CJUSTIFIED,
												layout.navigation.nextButton.x,
												layout.navigation.nextButton.y,
												BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, BtnNextButtonCallback);
	CHECKF(stagedResources.addButton(nextButton, giNextButton));
	SetButtonCursor(giNextButton, CURSOR_WWW );

	gbCurrentSoldier = AimMercArray[gbCurrentIndex];
	gbCurrentSoldierBio = gAimAvailability[AimMercArray[gbCurrentIndex]].AimBio;

	//tais: nsgi create kit selection buttons
	if(layout.expandedGear)
		CHECKF(CreateKitSelectionButtons(stagedResources, layout));

	gfStopMercFromTalking = FALSE;
	gubVideoConferencingMode = (UINT8)giCurrentSubPage;
	gubVideoConferencingPreviousMode = AIM_VIDEO_NOT_DISPLAYED_MODE;

	gfRenderTopLevel = FALSE;

	CHECKF(InitAimDefaults());
	if (!InitAimMenuBar())
	{
		RemoveAimDefaults();
		return FALSE;
	}

	// if we are re-entering but the video conference should still be up
	if( gubVideoConferencingMode != 0 )
	{
		//if we need to re initialize the talking face
		if( gubVideoConferencingMode !=	AIM_VIDEO_FIRST_CONTACT_MERC_MODE)
		{
			if (!InitVideoFace(gbCurrentSoldier))
			{
				ExitAimMenuBar();
				RemoveAimDefaults();
				return FALSE;
			}
		}

		if (!InitDeleteVideoConferencePopUp())
		{
			if (gfVideoFaceActive && giMercFaceIndex != -1)
				DeleteFace(giMercFaceIndex);
			gfVideoFaceActive = FALSE;
			giMercFaceIndex = -1;
			gAimMembersVideoConferenceResources.clear();
			gAimMembersVideoCloseResources.clear();
			ExitAimMenuBar();
			RemoveAimDefaults();
			return FALSE;
		}
	}

	gAimMembersResources = std::move(stagedResources);

	//LoadTextMercPopupImages( BASIC_MERC_POPUP_BACKGROUND, BASIC_MERC_POPUP_BORDER);

	RenderAIMMembers();
	gfIsNewMailFlagSet = FALSE;
	gfAimMemberCanMercSayOpeningQuote = TRUE;

	return( TRUE );
}

void ExitAIMMembers()
{
	//if we are exiting and the transfer of funds popup is enable, make sure we dont come back to it
	if( gubPopUpBoxAction )
		giCurrentSubPage = AIM_VIDEO_NOT_DISPLAYED_MODE;
	else
		giCurrentSubPage = gubVideoConferencingMode;

	gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;
	InitDeleteVideoConferencePopUp( );

	// Release the video-conference face slot so it isn't leaked across contacts. EnterAIMMembers
	// re-creates it on re-entry; DeleteFace self-guards a -1 index.
	if ( gfVideoFaceActive && giMercFaceIndex != -1 )
	{
		DeleteFace( giMercFaceIndex );
	}
	gfVideoFaceActive = FALSE;
	giMercFaceIndex = -1;

	InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);
	gAimMembersResources.clear();
	ExitAimMenuBar();
	RemoveAimDefaults();

	RemoveTextMercPopupImages( );

}

void HandleAIMMembers()
{
	//determine if the merc has a quote that is waiting to be said
	DelayMercSpeech( 0, 0, 0, FALSE, FALSE );

	if( gfHangUpMerc && !gfMercIsTalking)
	{
		if( gubVideoConferencingMode != AIM_VIDEO_NOT_DISPLAYED_MODE )
			gubVideoConferencingMode = AIM_VIDEO_POPDOWN_MODE;
		gfHangUpMerc = FALSE;
	}

	if( gfStopMercFromTalking )
	{
		StopMercTalking();
		gfStopMercFromTalking = FALSE;
/*
		//if we were waiting for the merc to stop talking
		if( gfWaitingForMercToStopTalkingOrUserToClick )
		{
			gubVideoConferencingMode = AIM_VIDEO_POPDOWN_MODE;
			gfWaitingForMercToStopTalkingOrUserToClick = FALSE;
		}
*/
	}

	// If we have to change video conference modes, change to new mode
	if( gubVideoConferencingMode != gubVideoConferencingPreviousMode && gubPopUpBoxAction != AIM_POPUP_DISPLAY )
	{
		InitDeleteVideoConferencePopUp( );

		//if we are exiting to display a popup box, dont rerender the display
		if( !fExitDueToMessageBox )
			gfRedrawScreen = TRUE;
	}


	// If we have to get rid of the popup box
	if( gubPopUpBoxAction == AIM_POPUP_DELETE )
	{
		InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);

		//if we are exiting to display a popup box, dont rerender the display
		if( !fExitDueToMessageBox )
			gfRedrawScreen = TRUE;
	}

	// Handle the current video conference screen
	HandleCurrentVideoConfMode();

	//If the answering machine is active, display the graphics for it
//	if( gfIsAnsweringMachineActive )
//		HandleAnsweringMachineMessage();

	// if the face is active, display the talking face
	if( gfVideoFaceActive )
	{
		gfMercIsTalking = DisplayTalkingMercFaceForVideoPopUp( giMercFaceIndex );

		//put the noise lines on the screen
		if( !gfIsAnsweringMachineActive )
			HandleVideoDistortion();

		//to handle when/if the merc is getting po'ed (waiting for player to do something)
		if( !gfMercIsTalking )
			HandleMercAttitude();
	}

	//if we have to rerender the popup, set the flag to render the PostButtonRender function in laptop.c
	if( gubPopUpBoxAction == AIM_POPUP_DISPLAY )
	{
		fReDrawPostButtonRender = TRUE;
	}

 	//Gets set in the InitDeleteVideoConferencePopUp() function
	if( gfJustSwitchedVideoConferenceMode )
		gfJustSwitchedVideoConferenceMode = FALSE;


	if( gfRedrawScreen )
	{
		RenderAIMMembers();
		gfRedrawScreen = FALSE;
	}

	HandleAimMemberKeyBoardInput();
	HandleAimMemberMouseInput();
	MarkButtonsDirty( );
}

BOOLEAN RenderAIMMembersTopLevel()
{
	InitCreateDeleteAimPopUpBox( AIM_POPUP_DISPLAY, NULL, NULL, 0);

	return(TRUE);
}

BOOLEAN RenderAIMMembers()
{
	HVOBJECT	hStatsHandle;
	HVOBJECT	hPriceHandle;
	const Layout layout = CurrentAimMemberProfileLayout();
	const CampaignAimSitePolicy aimSitePolicy(
		GetGameContext().capabilities());

	DrawAimDefaults();

	if(layout.expandedGear)
	{
		// Refresh which kit selection buttons are enabled whenever the profile changes.
		RefreshWeaponKitSelectionButtons();
	}

	GetVideoObject(&hStatsHandle, guiStats);
	BltVideoObject(FRAME_BUFFER, hStatsHandle, 0,
		layout.stats.panel.x, layout.stats.panel.y,
		VO_BLT_SRCTRANSPARENCY, NULL);
	GetVideoObject(&hPriceHandle, guiPrice);
	BltVideoObject(FRAME_BUFFER, hPriceHandle, 0,
		layout.price.panel.x, layout.price.panel.y,
		VO_BLT_SRCTRANSPARENCY, NULL);
	CreateWeaponBoxBackground(layout);
	UpdateMercInfo(layout);

	DrawTextToScreen(CharacterInfo[AIM_MEMBER_ACTIVE_MEMBERS],
		layout.activeMemberTitle.origin.x,
		layout.activeMemberTitle.origin.y,
		layout.activeMemberTitle.width, AIM_MAINTITLE_FONT,
		AIM_M_ACTIVE_MEMBER_TITLE_COLOR, FONT_MCOLOR_BLACK,
		FALSE, CENTER_JUSTIFIED);

	if (aimSitePolicy.usesMissionFee())
	{
		DrawTextToScreen(CharacterInfo[AIM_MEMBER_UB_MISSION_FEE],
			layout.price.missionFeeLabel.origin.x,
			layout.price.missionFeeLabel.origin.y, 0,
			AIM_M_FONT_PREV_NEXT_CONTACT, AIM_M_FEE_CONTRACT_COLOR,
			FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}
	else
	{
		DrawTextToScreen(CharacterInfo[AIM_MEMBER_FEE],
			layout.price.fee.origin.x, layout.price.fee.origin.y, 0,
			AIM_M_FONT_PREV_NEXT_CONTACT, AIM_M_FEE_CONTRACT_COLOR,
			FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
		DrawTextToScreen(CharacterInfo[AIM_MEMBER_CONTRACT],
			layout.price.contract.origin.x,
			layout.price.contract.origin.y,
			layout.price.contract.width, AIM_M_FONT_PREV_NEXT_CONTACT,
			AIM_M_FEE_CONTRACT_COLOR, FONT_MCOLOR_BLACK,
			FALSE, RIGHT_JUSTIFIED);

		const UINT16 payPeriodText[] = {
			AIM_MEMBER_1_DAY, AIM_MEMBER_1_WEEK, AIM_MEMBER_2_WEEKS};
		for (std::size_t i = 0; i < 3; ++i)
		{
			DrawTextToScreen(CharacterInfo[payPeriodText[i]],
				layout.price.contract.origin.x, layout.stats.rows[i],
				layout.price.contract.width, AIM_M_FONT_STATIC_TEXT,
				AIM_M_COLOR_STATIC_TEXT, FONT_MCOLOR_BLACK,
				FALSE, LEFT_JUSTIFIED);
		}

		const UINT16 optionalGearText = layout.expandedGear
			? AIM_MEMBER_OPTIONAL_GEAR_NSGI : AIM_MEMBER_OPTIONAL_GEAR;
		DrawTextToScreen(CharacterInfo[optionalGearText],
			layout.price.optionalGearLabel.origin.x,
			layout.price.optionalGearLabel.origin.y,
			layout.price.optionalGearLabel.width, AIM_M_FONT_STATIC_TEXT,
			AIM_M_COLOR_STATIC_TEXT, FONT_MCOLOR_BLACK, FALSE,
			layout.expandedGear ? RIGHT_JUSTIFIED : LEFT_JUSTIFIED);

		int optionalGearCostX = layout.price.optionalGearCost.origin.x;
		if (!layout.expandedGear)
		{
			optionalGearCostX += StringPixLength(
				CharacterInfo[optionalGearText], AIM_M_FONT_STATIC_TEXT) + 5;
		}
		DrawTextToScreen(
			FormatMoney(gMercProfiles[gbCurrentSoldier].usOptionalGearCost).data(),
			optionalGearCostX, layout.price.optionalGearCost.origin.y,
			layout.price.optionalGearCost.width, AIM_M_FONT_STATIC_TEXT,
			AIM_M_COLOR_DYNAMIC_TEXT, FONT_MCOLOR_BLACK, FALSE,
			layout.expandedGear ? RIGHT_JUSTIFIED : LEFT_JUSTIFIED);
	}

	DisableAimButton();

	DisplayMercsInventory(gbCurrentSoldier, layout);
	DisplayMercsFace(layout);
	DisplayMercStats(layout);

	if( gubVideoConferencingMode)
	{
		DisplayVideoConferencingDisplay();
	}
	else
	{
		gubMercAttitudeLevel=0;
		gfIsAnsweringMachineActive = FALSE;
	}


	//check to see if the merc is dead if so disable the contact button
	if( IsMercDead( gbCurrentSoldier ) )
	{
		DisableButton( giContactButton );
	}
	else
	{
		EnableButton( giContactButton );
	}


	//if we are to render the 'click face' text
	if(	gfAimMemberDisplayFaceHelpText )
	{
		DisplayAimMemberClickOnFaceHelpText(layout);
	}




	RenderWWWProgramTitleBar( );
	DisplayProgramBoundingBox( TRUE );
	fReDrawScreenFlag = TRUE;

	return(TRUE);
}


BOOLEAN DrawNumeralsToScreen(INT32 iNumber, INT8 bWidth, UINT16 usLocX, UINT16 usLocY, UINT32 ulFont, UINT8 ubColor)
{
	CHAR16		sStr[10];

	swprintf(sStr, L"%d", iNumber);

	DrawTextToScreen(sStr, usLocX, usLocY, bWidth, ulFont, ubColor, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED	);

	return(TRUE);
}



void SelectFaceRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
		guiCurrentLaptopMode = LAPTOP_MODE_AIM_MEMBERS_FACIAL_INDEX;
	}
	else if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		//if the merc is not dead, video conference with the merc
		if( !IsMercDead( gbCurrentSoldier ) )
		{
			gubVideoConferencingMode = AIM_VIDEO_POPUP_MODE;
			gfFirstTimeInContactScreen = TRUE;
		}
	}
}

void SelectFaceMovementRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if( iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		gfAimMemberDisplayFaceHelpText = FALSE;
		gfRedrawScreen = TRUE;
	}
	else if( iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		gfAimMemberDisplayFaceHelpText = TRUE;
		gfRedrawScreen = TRUE;
	}
	else if( iReason & MSYS_CALLBACK_REASON_MOVE )
	{
	}
}

#include "LocalizedStrings.h"

static auto DrawDiscountedDailyCosts(const Layout& layout) -> void
{
	constexpr auto offset{ 65 };
	constexpr auto l{ L"(" };
	constexpr auto r{ L")" };
	const int x = layout.price.fee.origin.x + offset;
	const int width = layout.price.fee.width;

	DrawTextToScreen((l + FormatMoney(gMercProfiles[gbCurrentSoldier].uiWeeklySalary / 7) + r).data(), x, layout.stats.rows[1], width, AIM_M_NUMBER_FONT, AIM_M_COLOR_DYNAMIC_TEXT, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
	DrawTextToScreen((l + FormatMoney(gMercProfiles[gbCurrentSoldier].uiBiWeeklySalary / 14) + r).data(), x, layout.stats.rows[2], width, AIM_M_NUMBER_FONT, AIM_M_COLOR_DYNAMIC_TEXT, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
}

BOOLEAN	UpdateMercInfo(const Layout& layout)
{
	CHAR16					MercInfoString[ SIZE_MERC_BIO_INFO ]{};
	CHAR16					AdditionalInfoString[ SIZE_MERC_BIO_INFO ]{};
	const CampaignAimSitePolicy aimSitePolicy(
		GetGameContext().capabilities());

	if (!aimSitePolicy.showsSalaryBreakdown())
	{
		DrawTextToScreen(
			FormatMoney(gMercProfiles[gbCurrentSoldier].uiWeeklySalary).data(),
			layout.price.missionFeeValue.origin.x,
			layout.price.missionFeeValue.origin.y,
			layout.price.missionFeeValue.width, AIM_M_NUMBER_FONT,
			AIM_M_COLOR_DYNAMIC_TEXT, FONT_MCOLOR_BLACK,
			FALSE, RIGHT_JUSTIFIED);
		DisplayWrappedString(layout.price.missionFeeOffer.origin.x,
			layout.price.missionFeeOffer.origin.y,
			layout.price.missionFeeOffer.width, 2,
			AIM_M_FONT_DYNAMIC_TEXT, AIM_FONT_MCOLOR_WHITE,
			zNewTacticalMessages[TACT_MSG__AIMMEMBER_FEE_TEXT],
			FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	}
	else
	{
		const INT32 salaries[] = {
			gMercProfiles[gbCurrentSoldier].sSalary,
			static_cast<INT32>(gMercProfiles[gbCurrentSoldier].uiWeeklySalary),
			static_cast<INT32>(gMercProfiles[gbCurrentSoldier].uiBiWeeklySalary)};
		for (std::size_t i = 0; i < 3; ++i)
		{
			DrawTextToScreen(FormatMoney(salaries[i]).data(),
				layout.price.fee.origin.x, layout.stats.rows[i],
				layout.price.fee.width, AIM_M_NUMBER_FONT,
				AIM_M_COLOR_DYNAMIC_TEXT, FONT_MCOLOR_BLACK,
				FALSE, RIGHT_JUSTIFIED);
		}

		DrawDiscountedDailyCosts(layout);

		if( gMercProfiles[gbCurrentSoldier].bMedicalDeposit )
		{
			CHAR16	sMedicalString[40];
			swprintf(sMedicalString, L"%s %s",
				FormatMoney(gMercProfiles[gbCurrentSoldier].sMedicalDepositAmount).data(),
				CharacterInfo[AIM_MEMBER_MEDICAL_DEPOSIT_REQ]);
			const int lineCount = DisplayWrappedString(0, 0,
				layout.price.medicalDeposit.width, 2, AIM_FONT12ARIAL,
				AIM_M_COLOR_DYNAMIC_TEXT, sMedicalString, FONT_MCOLOR_BLACK,
				FALSE, CENTER_JUSTIFIED | DONT_DISPLAY_TEXT) /
				GetFontHeight(AIM_FONT12ARIAL);
			int medicalY = layout.price.medicalDeposit.origin.y;
			if (lineCount > 2)
			{
				medicalY -= GetFontHeight(AIM_FONT12ARIAL);
			}
			DisplayWrappedString(layout.price.medicalDeposit.origin.x,
				medicalY, layout.price.medicalDeposit.width, 2,
				AIM_FONT12ARIAL, AIM_M_COLOR_DYNAMIC_TEXT, sMedicalString,
				FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
		}
	}

	if(!g_bUseXML_Strings)
	{
		if (gbCurrentSoldierBio != 255)
			LoadMercBioInfo(gbCurrentSoldierBio,
				MercInfoString, AdditionalInfoString);
	}
	else
	{
		Loc::GetString(Loc::AIM_BIOGRAPHY, L"Bio", gbCurrentSoldier,
			MercInfoString, SIZE_MERC_BIO_INFO);
		Loc::GetString(Loc::AIM_BIOGRAPHY, L"Add", gbCurrentSoldier,
			AdditionalInfoString, SIZE_MERC_BIO_INFO);
	}
	if(MercInfoString[0] != 0)
	{
		DisplayWrappedString(layout.biography.biography.origin.x,
			layout.biography.biography.origin.y,
			layout.biography.biography.width, 2,
			AIM_M_FONT_DYNAMIC_TEXT, AIM_FONT_MCOLOR_WHITE,
			MercInfoString, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}
	if(AdditionalInfoString[0] != 0)
	{
		DrawTextToScreen(CharacterInfo[AIM_MEMBER_ADDTNL_INFO],
			layout.biography.additionalLabel.x,
			layout.biography.additionalLabel.y, 0,
			AIM_M_FONT_STATIC_TEXT, AIM_M_COLOR_STATIC_TEXT,
			FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
		DisplayWrappedString(layout.biography.additionalText.origin.x,
			layout.biography.additionalText.origin.y,
			layout.biography.additionalText.width, 2,
			AIM_M_FONT_DYNAMIC_TEXT, AIM_FONT_MCOLOR_WHITE,
			AdditionalInfoString, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}
	return(TRUE);
}



// EDT files store 16-bit chars (Win32 wchar_t layout). On macOS/Linux
// wchar_t is 32-bit, so we must widen on read. See Utils/Encrypted File.cpp
// for the canonical implementation; this helper inlines it for the two
// reads below so we don't reopen MERCBIOSFILENAME twice.
static BOOLEAN ReadAndWidenWideChars(HWFILE hFile, CHAR16 *pDest, UINT32 uiByteCount)
{
	const UINT32 charCount = uiByteCount / 2;
	std::vector<UINT16> tmp(charCount);
	UINT32 uiBytesRead = 0;
	if (!FileRead(hFile, tmp.data(), uiByteCount, &uiBytesRead))
		return FALSE;
	for (UINT32 i = 0; i < charCount; ++i)
		pDest[i] = (CHAR16)tmp[i];
	return TRUE;
}

BOOLEAN LoadMercBioInfo(UINT8 ubIndex, CHAR16 *pInfoString, CHAR16 *pAddInfo)
{
	HWFILE		hFile;
	UINT32		uiStartSeekAmount;


	hFile = FileOpen(MERCBIOSFILENAME, FILE_ACCESS_READ, FALSE);
	if ( !hFile )
	{
		return( FALSE );
	}


	// Get current mercs bio info
	uiStartSeekAmount = (SIZE_MERC_BIO_INFO + SIZE_MERC_ADDITIONAL_INFO) * ubIndex;

	if ( FileSeek( hFile, uiStartSeekAmount, FILE_SEEK_FROM_START ) == FALSE )
	{
		FileClose(hFile);
		return( FALSE );
	}

	if( !ReadAndWidenWideChars( hFile, pInfoString, SIZE_MERC_BIO_INFO ) )
	{
		FileClose(hFile);
		return( FALSE );
	}

	//// Decrement, by 1, any value > 32
	//for(i=0; (i<SIZE_MERC_BIO_INFO) && (pInfoString[i] != 0); i++ )
	//{
	//	if( pInfoString[i] > 33 )
	//		pInfoString[i] -= 1;
	//	#ifdef POLISH
	//		switch( pInfoString[ i ] )
	//		{
	//			case 260:		pInfoString[ i ] = 165;		break;
	//			case 262:		pInfoString[ i ] = 198;		break;
	//			case 280:		pInfoString[ i ] = 202;		break;
	//			case 321:		pInfoString[ i ] = 163;		break;
	//			case 323:		pInfoString[ i ] = 209;		break;
	//			case 211:		pInfoString[ i ] = 211;		break;

	//			case 346:		pInfoString[ i ] = 338;		break;
	//			case 379:		pInfoString[ i ] = 175;		break;
	//			case 377:		pInfoString[ i ] = 143;		break;
	//			case 261:		pInfoString[ i ] = 185;		break;
	//			case 263:		pInfoString[ i ] = 230;		break;
	//			case 281:		pInfoString[ i ] = 234;		break;

	//			case 322:		pInfoString[ i ] = 179;		break;
	//			case 324:		pInfoString[ i ] = 241;		break;
	//			case 243:		pInfoString[ i ] = 243;		break;
	//			case 347:		pInfoString[ i ] = 339;		break;
	//			case 380:		pInfoString[ i ] = 191;		break;
	//			case 378:		pInfoString[ i ] = 376;		break;
	//		}
	//	#endif
	//}
	
	// DecodeString indexes wchar_t entries; pass char count, not byte count.
	DecodeString(pInfoString, SIZE_MERC_BIO_INFO / 2);

	// Get the additional info
	uiStartSeekAmount = ((SIZE_MERC_BIO_INFO + SIZE_MERC_ADDITIONAL_INFO) * ubIndex )+ SIZE_MERC_BIO_INFO ;
	if ( FileSeek( hFile, uiStartSeekAmount, FILE_SEEK_FROM_START ) == FALSE )
	{
		FileClose(hFile);
		return( FALSE );
	}

	if( !ReadAndWidenWideChars( hFile, pAddInfo, SIZE_MERC_ADDITIONAL_INFO ) )
	{
		FileClose(hFile);
		return( FALSE );
	}

	//// Decrement, by 1, any value > 32
	//for(i=0; (i<SIZE_MERC_BIO_INFO) && (pAddInfo[i] != 0); i++ )
	//{
	//	if( pAddInfo[i] > 33 )
	//		pAddInfo[i] -= 1;
	//	#ifdef POLISH
	//		switch( pAddInfo[ i ] )
	//		{
	//			case 260:		pAddInfo[ i ] = 165;		break;
	//			case 262:		pAddInfo[ i ] = 198;		break;
	//			case 280:		pAddInfo[ i ] = 202;		break;
	//			case 321:		pAddInfo[ i ] = 163;		break;
	//			case 323:		pAddInfo[ i ] = 209;		break;
	//			case 211:		pAddInfo[ i ] = 211;		break;

	//			case 346:		pAddInfo[ i ] = 338;		break;
	//			case 379:		pAddInfo[ i ] = 175;		break;
	//			case 377:		pAddInfo[ i ] = 143;		break;
	//			case 261:		pAddInfo[ i ] = 185;		break;
	//			case 263:		pAddInfo[ i ] = 230;		break;
	//			case 281:		pAddInfo[ i ] = 234;		break;

	//			case 322:		pAddInfo[ i ] = 179;		break;
	//			case 324:		pAddInfo[ i ] = 241;		break;
	//			case 243:		pAddInfo[ i ] = 243;		break;
	//			case 347:		pAddInfo[ i ] = 339;		break;
	//			case 380:		pAddInfo[ i ] = 191;		break;
	//			case 378:		pAddInfo[ i ] = 376;		break;
	//		}
	//	#endif
	//}

	// Note: original code passed SIZE_MERC_BIO_INFO here as the bound,
	// not SIZE_MERC_ADDITIONAL_INFO -- preserved verbatim to avoid changing
	// semantics. Convert byte count -> char count for the wchar_t iterator.
	DecodeString(pAddInfo, SIZE_MERC_BIO_INFO / 2);

	FileClose(hFile);
	return(TRUE);
}


BOOLEAN DisplayMercsInventory(UINT8 ubMercID)
{
	return DisplayMercsInventory(ubMercID, CurrentAimMemberProfileLayout());
}

BOOLEAN DisplayMercsInventory(UINT8 ubMercID, const Layout& layout)
{
	UINT8			i;
	INT16			PosX, PosY, sCenX, sCenY;
	UINT16			usItem;
	INVTYPE			*pItem;
	HVOBJECT		hVObject;
	UINT32			usHeight, usWidth;
	ETRLEObject		*pTrav;
	CHAR16			gzItemName[ 5000 ];
	UINT8			ubItemCount=0;

	//if the mercs inventory has already been purchased, dont display the inventory
	if ( (gMercProfiles[ ubMercID ].ubMiscFlags & PROFILE_MISC_FLAG_ALREADY_USED_ITEMS) && !gGameExternalOptions.fGearKitsAlwaysAvailable )
		return( TRUE );

	if(layout.expandedGear)
	{		
		//tais: disable Weaponbox Mouseregions to stop crashing when changing kit selection and hovering over item
		//tooltips for weaponbox
		for(i=0; i<layout.inventory.capacity(); ++i)
		{
			MSYS_DisableRegion(&gWeaponboxFasthelpRegion[i]);
		}

		UINT8 invsize = gMercProfiles[ubMercID].inv.size();
		for(i=0; i<invsize; ++i)
		{
			usItem = gMercProfiles[ubMercID].inv[ i ];

			//if its a valid item AND we are only displaying less then 8 items
			if( usItem && ubItemCount < layout.inventory.capacity() )
			{
				const std::size_t slotIndex = ubItemCount;
				const auto content = layout.inventory.contentOrigin(slotIndex);
				PosX = static_cast<INT16>(content.x);
				PosY = static_cast<INT16>(content.y);

				//increase the item count
				ubItemCount++;

				pItem = &Item[ usItem ];
				GetVideoObject( &hVObject, GetInterfaceGraphicForItem( pItem ) );
				UINT16 usGraphicNum = g_bUsePngItemImages ? 0 : pItem->ubGraphicNum;
				
				if(usGraphicNum < hVObject->usNumberOfObjects)
				{
					pTrav = &(hVObject->pETRLEObject[ usGraphicNum ] );
				}
				else
				{
					SGP_THROW(_BS(L"Number of images in VObject [") << hVObject->usNumberOfObjects
						<< L"] is smaller than the requested index [" << (int)pItem->ubGraphicNum << L"]" << _BS::wget);
				}

				usHeight				= (UINT32)pTrav->usHeight;
				usWidth					= (UINT32)pTrav->usWidth;

				sCenX = PosX + ( abs( layout.inventory.firstCell.width -
					layout.inventory.contentInsetX - (int)usWidth ) / 2 ) -
					pTrav->sOffsetX;
				sCenY = PosY + ( abs( layout.inventory.firstCell.height -
					(int)usHeight ) / 2 ) - pTrav->sOffsetY;

				//blt the shadow of the item
				if(gGameSettings.fOptions[ TOPTION_SHOW_ITEM_SHADOW ]) 
					BltVideoObjectOutlineShadowFromIndex( FRAME_BUFFER, GetInterfaceGraphicForItem( pItem ), usGraphicNum, sCenX-2, sCenY+2);
				
				//blt the item
				BltVideoObjectOutlineFromIndex( FRAME_BUFFER, GetInterfaceGraphicForItem( pItem ), usGraphicNum, sCenX, sCenY, 0, FALSE );

				//if there are more then 1 piece of equipment in the current slot, display how many there are
				if( gMercProfiles[ubMercID].bInvNumber[ i ] > 1 )
				{
					CHAR16 zTempStr[ 32 ];

					swprintf( zTempStr, L"x%d", gMercProfiles[ ubMercID ].bInvNumber[ i ] );

					DrawTextToScreen( zTempStr, (UINT16)(PosX-1),
						(UINT16)(PosY+20), layout.inventory.countTextWidth,
						AIM_M_FONT_DYNAMIC_TEXT, AIM_M_WEAPON_TEXT_COLOR,
						FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED );
				}
				
				GetHelpTextForItemInLaptop( gzItemName, usItem );
				SetRegionFastHelpText(
					&gWeaponboxFasthelpRegion[slotIndex], gzItemName );
				SetRegionHelpEndCallback(
					&gWeaponboxFasthelpRegion[slotIndex], HelpTextDoneCallback );
				MSYS_EnableRegion(&gWeaponboxFasthelpRegion[slotIndex]);

#ifdef ENCYCLOPEDIA_WORKS
				//Moa: update encyclopedia item visibility when item gets displayed
				EncyclopediaSetItemAsVisible( usItem, ENC_ITEM_DISCOVERED_NOT_INSPECTABLE );
#endif
			}
		}
	}
	else
	{
		const UINT16 wnameY = static_cast<UINT16>(layout.inventory.itemNameY);

		UINT8 invsize = gMercProfiles[ubMercID].inv.size();
		for(i=0; i<invsize; ++i)
		{
			usItem = gMercProfiles[ubMercID].inv[ i ];

			//if its a valid item AND we are only displaying less then 8 items
			if( usItem && ubItemCount < layout.inventory.capacity() )
			{
				const auto content = layout.inventory.contentOrigin(ubItemCount);
				PosX = static_cast<INT16>(content.x);
				PosY = static_cast<INT16>(content.y);

				//increase the item count
				ubItemCount++;

				pItem = &Item[ usItem ];
				GetVideoObject( &hVObject, GetInterfaceGraphicForItem( pItem ) );
				UINT16 usGraphicNum = g_bUsePngItemImages ? 0 : pItem->ubGraphicNum;
				if(usGraphicNum < hVObject->usNumberOfObjects)
				{
					pTrav = &(hVObject->pETRLEObject[ usGraphicNum ] );
				}
				else
				{
					std::wstringstream wss;
					wss << L"Number of images in VObject [" << hVObject->usNumberOfObjects
						<< L"] is smaller than the requested index [" << (int)pItem->ubGraphicNum << L"]";
					SGP_THROW(wss.str().c_str());
				}

				usHeight				= (UINT32)pTrav->usHeight;
				usWidth					= (UINT32)pTrav->usWidth;

				sCenX = PosX + ( abs( layout.inventory.firstCell.width -
					layout.inventory.contentInsetX - (int)usWidth ) / 2 ) -
					pTrav->sOffsetX;
				sCenY = PosY + ( abs( layout.inventory.firstCell.height -
					(int)usHeight ) / 2 ) - pTrav->sOffsetY;

				//blt the shadow of the item
				if(gGameSettings.fOptions[ TOPTION_SHOW_ITEM_SHADOW ]) 
					BltVideoObjectOutlineShadowFromIndex( FRAME_BUFFER, GetInterfaceGraphicForItem( pItem ), usGraphicNum, sCenX-2, sCenY+2);

				//blt the item
				BltVideoObjectOutlineFromIndex( FRAME_BUFFER, GetInterfaceGraphicForItem( pItem ), usGraphicNum, sCenX, sCenY, 0, FALSE );

				//if there are more then 1 piece of equipment in the current slot, display how many there are
				if( gMercProfiles[ubMercID].bInvNumber[ i ] > 1 )
				{
					CHAR16 zTempStr[ 32 ];

					swprintf( zTempStr, L"x%d", gMercProfiles[ ubMercID ].bInvNumber[ i ] );

					DrawTextToScreen( zTempStr, (UINT16)(PosX-1),
						(UINT16)(PosY+20), layout.inventory.countTextWidth,
						AIM_M_FONT_DYNAMIC_TEXT, AIM_M_WEAPON_TEXT_COLOR,
						FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED );
				}

				wcscpy( gzItemName, ShortItemNames[ usItem ] );

				//if this will only be a single line, center it in the box
				if( ( DisplayWrappedString( (UINT16)(PosX-1), wnameY, layout.inventory.countTextWidth, 2, AIM_M_WEAPON_TEXT_FONT, AIM_M_WEAPON_TEXT_COLOR,	gzItemName, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED | DONT_DISPLAY_TEXT ) / GetFontHeight( AIM_M_WEAPON_TEXT_FONT ) ) == 1 )
					DisplayWrappedString( (UINT16)(PosX-1), (UINT16)(wnameY+GetFontHeight( AIM_M_WEAPON_TEXT_FONT )/2), layout.inventory.countTextWidth, 2, AIM_M_WEAPON_TEXT_FONT, AIM_M_WEAPON_TEXT_COLOR,	gzItemName, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED );
				else
					DisplayWrappedString( (UINT16)(PosX-1), wnameY, layout.inventory.countTextWidth, 2, AIM_M_WEAPON_TEXT_FONT, AIM_M_WEAPON_TEXT_COLOR,	gzItemName, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED );

#ifdef ENCYCLOPEDIA_WORKS
				//Moa: update encyclopedia item visibility when item gets displayed
				EncyclopediaSetItemAsVisible( usItem, ENC_ITEM_DISCOVERED_NOT_INSPECTABLE );
#endif
			}
		}
	}

	return(TRUE);
}


//tais: nsgi, kit selection button callbacks one to five
void BtnWeaponboxSelectButtonCallback( GUI_BUTTON *btn, INT32 reason )
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			gbCurrentSoldier = AimMercArray[gbCurrentIndex];

			//tais: handle selected kit
			for ( int i = 0; i < NUM_MERCSTARTINGGEAR_KITS; ++i )
			{
				if ( btn->IDNum == giWeaponboxSelectionButton[i] )
				{
					WeaponKitSelectionUpdate( i );
					break;
				}
			}

			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			gfRedrawScreen = TRUE;
			gbCurrentSoldier = AimMercArray[gbCurrentIndex];
			gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;
			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}

void PreviousAimMember(void)
{
	InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);

	if (gbCurrentIndex > 0)
	{
		if (_KeyDown(17)) // CTRL
			gbCurrentIndex = 0;
		else if (_KeyDown(16)) // SHIFT
			gbCurrentIndex = __max(gbCurrentIndex - 10, 0);
		else
			gbCurrentIndex--;
	}
	else
		gbCurrentIndex = MAX_NUMBER_MERCS - 1;

	gfRedrawScreen = TRUE;

	//			gbCurrentSoldier = AimMercArray[gbCurrentIndex];
	gbCurrentSoldier = AimMercArray[gbCurrentIndex];
	gbCurrentSoldierBio = gAimAvailability[AimMercArray[gbCurrentIndex]].AimBio;

	gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;
}

void NextAimMember(void)
{
	InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);

	if (gbCurrentIndex < MAX_NUMBER_MERCS - 1)
	{
		if (_KeyDown(17)) // CTRL
			gbCurrentIndex = MAX_NUMBER_MERCS - 1;
		else if (_KeyDown(16)) // SHIFT
			gbCurrentIndex = __min(MAX_NUMBER_MERCS - 1, gbCurrentIndex + 10);
		else
			gbCurrentIndex++;
	}
	else
		gbCurrentIndex = 0;

	//			gbCurrentSoldier = AimMercArray[gbCurrentIndex];
	gbCurrentSoldier = AimMercArray[gbCurrentIndex];

	gbCurrentSoldierBio = gAimAvailability[AimMercArray[gbCurrentIndex]].AimBio;

	gfRedrawScreen = TRUE;

	gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;
}

void BtnPreviousButtonCallback(GUI_BUTTON *btn,INT32 reason)
{

BOOLEAN Stop = FALSE;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			PreviousAimMember();
			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}

void BtnContactButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			//if no popup box
			if( gubPopUpBoxAction != AIM_POPUP_DISPLAY )
			{	
				//if we are not already in the video conferemce mode, go in to it
				if( !gubVideoConferencingMode )
				{
					gubVideoConferencingMode = AIM_VIDEO_POPUP_MODE;
//					gubVideoConferencingMode = AIM_VIDEO_INIT_MODE;
					gfFirstTimeInContactScreen = TRUE;
				}
				InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);
			}
			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			
			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);			
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}

void BtnNextButtonCallback(GUI_BUTTON *btn,INT32 reason)
{

BOOLEAN Stop = FALSE;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			NextAimMember();
			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


BOOLEAN DisplayMercsFace(const Layout& layout)
{
	HVOBJECT hFaceHandle;
	HVOBJECT hPortraitHandle;
	STR							sFaceLoc = "FACES\\BIGFACES\\";
	char						sTemp[100];
	VOBJECT_DESC	VObjectDesc;
	TacticalActor			*pSoldier=NULL;

	//See if the merc is currently hired
	pSoldier = FindSoldierByProfileID( gbCurrentSoldier, TRUE );

	// Portrait Frame
	GetVideoObject(&hPortraitHandle, guiPortrait);
	BltVideoObject(FRAME_BUFFER, hPortraitHandle, 0,
		layout.portrait.x, layout.portrait.y,
		VO_BLT_SRCTRANSPARENCY, NULL);

	// load the Face graphic and add it
	if (gGameExternalOptions.fReadProfileDataFromXML)
	{
		// HEADROCK PROFEX: Do not read direct profile number, instead, look inside the profile for a ubFaceIndex value.
		sprintf(sTemp, "%s%02d.sti", sFaceLoc, gMercProfiles[gbCurrentSoldier].ubFaceIndex);
	}
	else
	{
		sprintf(sTemp, "%s%02d.sti", sFaceLoc, gbCurrentSoldier);
	}
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP(sTemp, VObjectDesc.ImageFile);
	LaptopPageResourceOwner faceResource;
	CHECKF(faceResource.addVideoObject(&VObjectDesc, guiFace));

	// Both profile variants use the same face states; only their selected
	// layout changes the placement.
	GetVideoObject(&hFaceHandle, guiFace);
	BltVideoObject(FRAME_BUFFER, hFaceHandle, 0,
		layout.face.x, layout.face.y, VO_BLT_SRCTRANSPARENCY,NULL);

	if( IsMercDead( gbCurrentSoldier ) )
	{
		// Shade the face red to signify that the merc is dead.
		hFaceHandle->pShades[ 0 ] = Create16BPPPaletteShaded(
			hFaceHandle->pPaletteEntry, DEAD_MERC_COLOR_RED,
			DEAD_MERC_COLOR_GREEN, DEAD_MERC_COLOR_BLUE, TRUE );
		SetObjectHandleShade( guiFace, 0 );
		BltVideoObject(FRAME_BUFFER, hFaceHandle, 0,
			layout.face.x, layout.face.y, VO_BLT_SRCTRANSPARENCY,NULL);
		DrawTextToScreen(AimPopUpText[AIM_MEMBER_DEAD],
			layout.face.x + 1, layout.faceStatusY, layout.face.width,
			FONT14ARIAL, 145, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	}
	else if( gMercProfiles[ gbCurrentSoldier ].bMercStatus == MERC_FIRED_AS_A_POW ||
		( pSoldier && pSoldier->assignment().current() == ASSIGNMENT_POW ) )
	{
		ShadowVideoSurfaceRect(FRAME_BUFFER, layout.face.x, layout.face.y,
			layout.face.right(), layout.face.bottom());
		DrawTextToScreen(pPOWStrings[0], layout.face.x + 1,
			layout.faceStatusY, layout.face.width, FONT14ARIAL, 145,
			FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	}
	else if( pSoldier )
	{
		ShadowVideoSurfaceRect(FRAME_BUFFER, layout.face.x, layout.face.y,
			layout.face.right(), layout.face.bottom());
		DrawTextToScreen(MercInfo[MERC_FILES_ALREADY_HIRED],
			layout.face.x + 1, layout.faceStatusY, layout.face.width,
			FONT14ARIAL, 145, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	}
	else if( !IsMercHireable( gbCurrentSoldier ) )
	{
		ShadowVideoSurfaceRect(FRAME_BUFFER, layout.face.x, layout.face.y,
			layout.face.right(), layout.face.bottom());
		DrawTextToScreen(AimPopUpText[AIM_MEMBER_ON_ASSIGNMENT],
			layout.face.x + 1, layout.faceStatusY, layout.face.width,
			FONT14ARIAL, 145, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	}
	return( TRUE );
}


void DisplayMercStats(const Layout& layout)
{
	const MERCPROFILESTRUCT& profile = gMercProfiles[gbCurrentSoldier];
	constexpr std::array<UINT16, 6> firstLabels = {
		AIM_MEMBER_HEALTH, AIM_MEMBER_AGILITY, AIM_MEMBER_DEXTERITY,
		AIM_MEMBER_STRENGTH, AIM_MEMBER_LEADERSHIP, AIM_MEMBER_WISDOM};
	constexpr std::array<UINT16, 5> secondLabels = {
		AIM_MEMBER_EXP_LEVEL, AIM_MEMBER_MARKSMANSHIP,
		AIM_MEMBER_MECHANICAL, AIM_MEMBER_EXPLOSIVE, AIM_MEMBER_MEDICAL};
	const std::array<INT8, 6> firstValues = {
		layout.expandedGear ? profile.bLifeMax : profile.bLife,
		profile.bAgility, profile.bDexterity, profile.bStrength,
		profile.bLeadership, profile.bWisdom};
	const std::array<INT8, 5> secondValues = {
		profile.bExpLevel, profile.bMarksmanship, profile.bMechanical,
		profile.bExplosive, profile.bMedical};

	for (std::size_t i = 0; i < firstLabels.size(); ++i)
	{
		DrawTextToScreen(CharacterInfo[firstLabels[i]],
			layout.stats.firstLabelX, layout.stats.rows[i], 0,
			AIM_M_FONT_STATIC_TEXT, AIM_M_COLOR_STATIC_TEXT,
			FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
		DisplayDots(layout.stats.firstLabelX, layout.stats.rows[i],
			layout.stats.firstDotX, CharacterInfo[firstLabels[i]]);
		DrawNumeralsToScreen(firstValues[i], 3, layout.stats.firstValueX,
			layout.stats.rows[i], AIM_M_NUMBER_FONT,
			GetStatColor(firstValues[i]));
	}

	for (std::size_t i = 0; i < secondLabels.size(); ++i)
	{
		DrawTextToScreen(CharacterInfo[secondLabels[i]],
			layout.stats.secondLabelX, layout.stats.rows[i], 0,
			AIM_M_FONT_STATIC_TEXT, AIM_M_COLOR_STATIC_TEXT,
			FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
		DisplayDots(layout.stats.secondLabelX, layout.stats.rows[i],
			layout.stats.secondDotX, CharacterInfo[secondLabels[i]]);
		const UINT8 color = i == 0
			? FONT_MCOLOR_WHITE : GetStatColor(secondValues[i]);
		DrawNumeralsToScreen(secondValues[i], 3, layout.stats.secondValueX,
			layout.stats.rows[i], AIM_M_NUMBER_FONT, color);
	}

	DrawTextToScreen(profile.zName, layout.stats.name.x, layout.stats.name.y,
		0, FONT14ARIAL, AIM_M_COLOR_DYNAMIC_TEXT, FONT_MCOLOR_BLACK,
		FALSE, LEFT_JUSTIFIED);

	CHAR16 sPage[60];
	swprintf(sPage, L"%d / %d", gbCurrentIndex + 1, MAX_NUMBER_MERCS);
	DrawTextToScreen(sPage, layout.navigation.pageNumber.x,
		layout.navigation.pageNumber.y, 0, AIM_M_FONT_STATIC_TEXT,
		AIM_M_COLOR_STATIC_TEXT, FONT_MCOLOR_BLACK, FALSE, RIGHT_JUSTIFIED);
}
UINT8	GetStatColor( INT8 bStat )
{
	if( bStat >= 80 )
		return( HIGH_STAT_COLOR );
	else if( bStat >= 20 )
		return( MED_STAT_COLOR );
	else
		return( LOW_STAT_COLOR );
}

//displays the dots between the stats and the stat name
void DisplayDots(UINT16 usNameX, UINT16 usNameY, UINT16 usStatX, STR16 pString)
{
	UINT16 usStringLength = StringPixLength(pString, AIM_M_FONT_STATIC_TEXT);
	INT16	i;
	UINT16 usPosX;

	usPosX = usStatX;
	for(i=usNameX + usStringLength; i <= usPosX; usPosX-=7)
	{
		DrawTextToScreen(L".", (UINT16)usPosX, usNameY, 0, AIM_M_FONT_STATIC_TEXT, AIM_M_COLOR_STATIC_TEXT, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}
}


void BtnContractLengthButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		UINT8	ubRetValue = (UINT8)MSYS_GetBtnUserData( btn, 0 );

		btn->uiFlags |= BUTTON_CLICKED_ON;

		gubContractLength = ubRetValue;
		DisplaySelectLights(TRUE, FALSE);

		DisplayMercChargeAmount();
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

		DisplaySelectLights(FALSE, FALSE);

		guiMercAttitudeTime = GetJA2Clock();

		DisplayMercChargeAmount();
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);

//		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


void BtnBuyEquipmentButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		gfBuyEquipment = (UINT8)MSYS_GetBtnUserData( btn, 0 );
		DisplaySelectLights(FALSE, TRUE);

		DisplayMercChargeAmount();
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		DisplaySelectLights(FALSE, FALSE);
		DisplayMercChargeAmount();

		guiMercAttitudeTime = GetJA2Clock();

//		InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);

		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}

//Transfer funds button callback
void BtnAuthorizeButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
// WDS - make number of mercenaries, etc. be configurable
// WDS DEBUG
if (LaptopSaveInfo.iCurrentBalance == 4500001) {
	unsigned hired = 0;
	while (hired < OUR_TEAM_SIZE_NO_VEHICLE) {
		if (AimMemberHireMerc()) {
//    	  giIdOfLastHiredMerc = AimMercArray[gbCurrentIndex];
		  giIdOfLastHiredMerc = AimMercArray[gbCurrentIndex];
		  ++hired;
		}
		++gbCurrentIndex;
	}
	return;
}
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		UINT8	ubRetValue = (UINT8)MSYS_GetBtnUserData( btn, 0 );
		btn->uiFlags &= (~BUTTON_CLICKED_ON );

		gfStopMercFromTalking = TRUE;
		gubMercAttitudeLevel = QUOTE_DELAY_NO_ACTION;

		//If we try to hire the merc
		if( ubRetValue == 0)
		{
			StopMercTalking();

			//can the merc be hired?	(does he like/not like people on the team
//			if( CanMercBeHired() )
			{
				//Was the merc hired
				if( AimMemberHireMerc() )
				{
					// if merc was hired
					InitCreateDeleteAimPopUpBox(AIM_POPUP_CREATE, AimPopUpText[AIM_MEMBER_FUNDS_TRANSFER_SUCCESFUL], NULL, AIM_POPUP_BOX_SUCCESS);
					DelayMercSpeech( gbCurrentSoldier, QUOTE_CONTRACT_ACCEPTANCE, 750, TRUE, FALSE );

					//Disable the buttons behind the message box
					EnableDisableCurrentVideoConferenceButtons( TRUE );

					SpecifyDisabledButtonStyle( giBuyEquipmentButton[0], DISABLED_STYLE_NONE );
					SpecifyDisabledButtonStyle( giBuyEquipmentButton[1], DISABLED_STYLE_NONE );

//					giIdOfLastHiredMerc = AimMercArray[gbCurrentIndex];
					giIdOfLastHiredMerc = AimMercArray[gbCurrentIndex];
				}
			}
/*
			else
			{
				//else the merc doesnt like a player on the team, hang up when the merc is done complaining

				//reset ( in case merc was going to say something
				DelayMercSpeech( 0, 0, 0, FALSE, TRUE );

				gubVideoConferencingMode = AIM_VIDEO_HIRE_MERC_MODE;
			}
*/
		}
		// else we cancel
		else
		{
			gubVideoConferencingMode = AIM_VIDEO_FIRST_CONTACT_MERC_MODE;
		}
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}



INT8 AimMemberHireMerc()
{
	MERC_HIRE_STRUCT HireMercStruct;
//	UINT8		ubCurrentSoldier = AimMercArray[gbCurrentIndex];
	UINT8		ubCurrentSoldier =  AimMercArray[gbCurrentIndex];
	INT8			bReturnCode;
	SoldierID	sSoldierID = NOBODY;
	INT8			bTypeOfContract=0;


	if( LaptopSaveInfo.iCurrentBalance < giContractAmount )
	{
		//wasnt hired because of lack of funds
		InitCreateDeleteAimPopUpBox(AIM_POPUP_CREATE, AimPopUpText[AIM_MEMBER_FUNDS_TRANSFER_FAILED], AimPopUpText[AIM_MEMBER_NOT_ENOUGH_FUNDS], AIM_POPUP_BOX_FAILURE);

		//Disable the buttons behind the message box
		EnableDisableCurrentVideoConferenceButtons( TRUE );

		SpecifyDisabledButtonStyle( giBuyEquipmentButton[0], DISABLED_STYLE_NONE );
		SpecifyDisabledButtonStyle( giBuyEquipmentButton[1], DISABLED_STYLE_NONE );

		DelayMercSpeech( gbCurrentSoldier, QUOTE_REFUSAL_TO_JOIN_LACK_OF_FUNDS, 750, TRUE, FALSE );

		return(FALSE);
	}

	memset(&HireMercStruct, 0, sizeof(MERC_HIRE_STRUCT));

	HireMercStruct.ubProfileID = ubCurrentSoldier;

	//DEF: temp
	HireMercStruct.sSectorX = gsMercArriveSectorX;
	HireMercStruct.sSectorY = gsMercArriveSectorY;
	HireMercStruct.fUseLandingZoneForArrival = TRUE;
	HireMercStruct.ubInsertionCode	= INSERTION_CODE_ARRIVING_GAME;

	HireMercStruct.fCopyProfileItemsOver = gfBuyEquipment;
	//if the players is buying the equipment
	if( gfBuyEquipment )
	{
		gMercProfiles[ ubCurrentSoldier ].usOptionalGearCost = 0;
		gMercProfiles[ ubCurrentSoldier ].ubMiscFlags |= PROFILE_MISC_FLAG_ALREADY_USED_ITEMS;
	}

		//If 1 day
	if( gubContractLength == AIM_CONTRACT_LENGTH_ONE_DAY)
	{
		bTypeOfContract = CONTRACT_EXTEND_1_DAY;
		HireMercStruct.iTotalContractLength = 1;
	}
	else if( gubContractLength == AIM_CONTRACT_LENGTH_ONE_WEEK)
	{
		bTypeOfContract = CONTRACT_EXTEND_1_WEEK;
		HireMercStruct.iTotalContractLength = 7;
	}
	else if( gubContractLength == AIM_CONTRACT_LENGTH_TWO_WEEKS)
	{
		bTypeOfContract = CONTRACT_EXTEND_2_WEEK;
		HireMercStruct.iTotalContractLength = 14;
	}

	//specify when the merc should arrive
	HireMercStruct.uiTimeTillMercArrives = GetMercArrivalTimeOfDay( );// + ubCurrentSoldier

	//Set the time and ID of the last hired merc will arrive
//	LaptopSaveInfo.sLastHiredMerc.iIdOfMerc = HireMercStruct.ubProfileID;
//	LaptopSaveInfo.sLastHiredMerc.uiArrivalTime = HireMercStruct.uiTimeTillMercArrives;

	//if we succesfully hired the merc
	bReturnCode = HireMerc( &HireMercStruct );
	if( bReturnCode == MERC_HIRE_OVER_PLAYER_LIMIT )
	{
		//display a warning saying u cant hire more than the max mercs
		DoLapTopMessageBox( MSG_BOX_LAPTOP_DEFAULT, AimPopUpText[ AIM_MEMBER_ALREADY_HAVE_MAX_MERCS ], LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);
		return(FALSE);
	}
	else if( bReturnCode == MERC_HIRE_FAILED )
	{
		return(FALSE);
	}

	//Set the type of contract the merc is on
	sSoldierID = GetSoldierIDFromMercID( ubCurrentSoldier );
	if( sSoldierID == NOBODY )
		return( FALSE );

	TacticalActor* hiredSoldier =
		GetJa2SoldierRepository().resolve(sSoldierID.i);
	if ( !hiredSoldier )
		return( FALSE );
	hiredSoldier->employment().lastContractType() = bTypeOfContract;


	//add an entry in the finacial page for the hiring of the merc
	AddTransactionToPlayersBook(HIRED_MERC, ubCurrentSoldier, GetWorldTotalMin(), -( giContractAmount - gMercProfiles[gbCurrentSoldier].sMedicalDepositAmount ) );

	if( gMercProfiles[ gbCurrentSoldier ].bMedicalDeposit )
	{
		//add an entry in the finacial page for the medical deposit
		AddTransactionToPlayersBook(	MEDICAL_DEPOSIT, ubCurrentSoldier, GetWorldTotalMin(), -(gMercProfiles[gbCurrentSoldier].sMedicalDepositAmount) );
	}

	//add an entry in the history page for the hiring of the merc
	if(!is_client)AddHistoryToPlayersLog(HISTORY_HIRED_MERC_FROM_AIM, ubCurrentSoldier, GetWorldTotalMin(), -1, -1 );
	return(TRUE);
}



BOOLEAN DisplayVideoConferencingDisplay()
{
	CHAR16		sMercName[128];
	const auto layout = CurrentAimVideoConferenceLayout();
	const CampaignAimSitePolicy aimSitePolicy(
		GetGameContext().capabilities());

	if( ( gubVideoConferencingMode == AIM_VIDEO_NOT_DISPLAYED_MODE ) || ( gubVideoConferencingMode == AIM_VIDEO_POPUP_MODE ) )
		return(FALSE);

	DisplayMercsVideoFace();

	//Title & Name
	if( gubVideoConferencingMode == AIM_VIDEO_INIT_MODE)
	{
		swprintf( sMercName, L"%s",	VideoConfercingText[AIM_MEMBER_CONNECTING]);
		DrawTextToScreen(sMercName, layout.name.x, layout.name.y, 0, FONT12ARIAL, AIM_M_VIDEO_TITLE_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}
	else
	{
		swprintf( sMercName, L"%s %s",	VideoConfercingText[AIM_MEMBER_VIDEO_CONF_WITH], gMercProfiles[gbCurrentSoldier].zName);
		DrawTextToScreen(sMercName, layout.name.x, layout.name.y, 0, FONT12ARIAL, AIM_M_VIDEO_TITLE_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}

	//Display Contract charge text
	if( gubVideoConferencingMode == AIM_VIDEO_HIRE_MERC_MODE )
	{
		// Display the contract charge
		SetFontShadow(AIM_M_VIDEO_NAME_SHADOWCOLOR);
		DrawTextToScreen(VideoConfercingText[AIM_MEMBER_CONTRACT_CHARGE], layout.contractChargeLabel.x, layout.contractChargeLabel.y, 0, FONT12ARIAL, AIM_M_VIDEO_NAME_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
		SetFontShadow(DEFAULT_SHADOW);
	}

	DisplayMercChargeAmount();

	if (aimSitePolicy.showsOneTimeFeeOffer() &&
		gubVideoConferencingMode == AIM_VIDEO_HIRE_MERC_MODE)
	{
		CHAR16	offerText[190];
		swprintf(offerText, zNewTacticalMessages[TACT_MSG__AIMMEMBER_ONE_TIME_FEE], gMercProfiles[gbCurrentSoldier].zNickname);
		SetFontShadow(AIM_M_VIDEO_NAME_SHADOWCOLOR);
		DisplayWrappedString(layout.oneTimeFeeOffer.origin.x, layout.oneTimeFeeOffer.origin.y, layout.oneTimeFeeOffer.width, 2, AIM_M_FONT_DYNAMIC_TEXT, FONT_MCOLOR_BLACK, offerText, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
		SetFontShadow(DEFAULT_SHADOW);
	}


//	if( gfMercIsTalking && !gfIsAnsweringMachineActive)
	if( gfMercIsTalking && gGameSettings.fOptions[ TOPTION_SUBTITLES ] )
	{
		UINT16 usActualWidth;
		UINT16 usActualHeight;
		UINT16 usPosX;
//never use it anymore
//	SET_USE_WINFONTS( TRUE );
//	SET_WINFONT( giSubTitleWinFont );

	iAimMembersBoxId = PrepareMercPopupBox( iAimMembersBoxId ,BASIC_MERC_POPUP_BACKGROUND, BASIC_MERC_POPUP_BORDER, gsTalkingMercText, 300, 0, 0, 0, &usActualWidth, &usActualHeight);

//	SET_USE_WINFONTS( FALSE );

		usPosX = iScreenWidthOffset + ( 613 - usActualWidth ) / 2 ;

		RenderMercPopUpBoxFromIndex( iAimMembersBoxId, usPosX, layout.talkingTextPopupY, FRAME_BUFFER);

		if( RemoveMercPopupBoxFromIndex( iAimMembersBoxId ) )
		{
			iAimMembersBoxId = -1;
		}
	}

	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);

	return(TRUE);
}

BOOLEAN DisplayMercsVideoFace()
{
	HVOBJECT	hTerminalHandle;
	const auto layout = CurrentAimVideoConferenceLayout();
	//STR				sFaceLoc = "FACES\\";

	// Get and Blt Terminal Frame
	GetVideoObject(&hTerminalHandle, guiVideoConfTerminal);
	ShadowVideoSurfaceImage( FRAME_BUFFER, hTerminalHandle, layout.terminal.x, layout.terminal.y);
	BltVideoObject(FRAME_BUFFER, hTerminalHandle, 0,layout.terminal.x, layout.terminal.y, VO_BLT_SRCTRANSPARENCY,NULL);


	//Display the Select light on the merc
	if(gubVideoConferencingMode == AIM_VIDEO_HIRE_MERC_MODE)
		DisplaySelectLights(FALSE, FALSE);

	return(TRUE);
}

void DisplaySelectLights(BOOLEAN fContractDown, BOOLEAN fBuyEquipDown)
{
	const CampaignAimSitePolicy aimSitePolicy(
		GetGameContext().capabilities());
	if (!aimSitePolicy.showsSelectionLights())
		return;

	const auto layout = CurrentAimVideoConferenceLayout();
	UINT16 i;

	//First draw the select light for the contract length buttons
	for(i=0; i<3; i++)
	{
		const auto button = layout.contractButtons.at(i);
		// if the if is true, the light is on
		if( gubContractLength == i)
		{
			const auto light = layout.selectionLight(button, fContractDown != FALSE);
			ColorFillVideoSurfaceArea( FRAME_BUFFER, light.x, light.y, light.x+8, light.y+8, Get16BPPColor( FROMRGB( 0, 255, 0 ) ) );
		}
		else
		{
			const auto light = layout.selectionLight(button, false);
			ColorFillVideoSurfaceArea( FRAME_BUFFER, light.x, light.y, light.x+8, light.y+8, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );
		}
	}

	//draw the select light for the buy equipment buttons
	for(i=0; i<2; i++)
	{
		const auto button = layout.equipmentButtons.at(i);
		if( gfBuyEquipment == i)
		{
			const auto light = layout.selectionLight(button, fBuyEquipDown != FALSE);
			ColorFillVideoSurfaceArea( FRAME_BUFFER, light.x, light.y, light.x+8, light.y+8, Get16BPPColor( FROMRGB( 0, 255, 0 ) ) );
		}
		else
		{
			const auto light = layout.selectionLight(button, false);
			ColorFillVideoSurfaceArea( FRAME_BUFFER, light.x, light.y, light.x+8, light.y+8, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );
		}
	}
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}


UINT32 DisplayMercChargeAmount()
{
	HVOBJECT hImageHandle;
	const auto layout = CurrentAimVideoConferenceLayout();
	const CampaignAimSitePolicy aimSitePolicy(
		GetGameContext().capabilities());


	if( gubVideoConferencingMode != AIM_VIDEO_HIRE_MERC_MODE )
		return(0);

	// Display the 'black hole'for the contract charge	in the video conference terminal
	GetVideoObject(&hImageHandle, guiVideoContractCharge);
	BltVideoObject(FRAME_BUFFER, hImageHandle, 0,layout.contractImage.x, layout.contractImage.y, VO_BLT_SRCTRANSPARENCY,NULL);


	if( FindSoldierByProfileID( gbCurrentSoldier, TRUE ) == NULL )
	{
		giContractAmount = aimSitePolicy.contractCharge(
			gMercProfiles[gbCurrentSoldier].sSalary,
			gMercProfiles[gbCurrentSoldier].uiWeeklySalary,
			gMercProfiles[gbCurrentSoldier].uiBiWeeklySalary,
			gMercProfiles[gbCurrentSoldier].sMedicalDepositAmount,
			gMercProfiles[gbCurrentSoldier].usOptionalGearCost,
			gubContractLength,
			gMercProfiles[gbCurrentSoldier].bMedicalDeposit && !is_client,
			gfBuyEquipment);
	}


	//if the merc hasnt just been hired
//	if( FindSoldierByProfileID( gbCurrentSoldier, TRUE ) == NULL )
	{
		auto contractAmount{ FormatMoney(giContractAmount) };
		if (aimSitePolicy.appendsMedicalDepositLabel())
		{
			if (gMercProfiles[gbCurrentSoldier].bMedicalDeposit)
				contractAmount += L" ";
			contractAmount += VideoConfercingText[AIM_MEMBER_WITH_MEDICAL];
		}

		DrawTextToScreen(contractAmount.data(), layout.contractChargeAmount.x + 1, layout.contractChargeAmount.y + 3, 0, AIM_M_VIDEO_CONTRACT_AMOUNT_FONT, AIM_M_VIDEO_CONTRACT_AMOUNT_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}

	return(giContractAmount);
}

BOOLEAN InitCreateDeleteAimPopUpBox(UINT8 ubFlag, STR16 sString1, STR16 sString2, UINT8 ubData)
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner stagedResources;
	HVOBJECT			hPopupBoxHandle;
	static UINT16				usPopUpBoxPosX, usPopUpBoxPosY;
	static CHAR16				sPopUpString1[400], sPopUpString2[400];
	static BOOLEAN		fPopUpBoxActive = FALSE;;
	const auto layout = CurrentAimVideoConferenceLayout();

	switch( ubFlag )
	{
		case AIM_POPUP_CREATE:
		{
			if( fPopUpBoxActive )
				return(FALSE);

			gAimMembersPopupResources.clear();

			if(sString1 != NULL)
			{
				wcsncpy(sPopUpString1, sString1, 399);
				sPopUpString1[399] = L'\0';
			}
			else
				sPopUpString1[0] = L'\0';

			if(sString2 != NULL)
			{
				wcsncpy(sPopUpString2, sString2, 399);
				sPopUpString2[399] = L'\0';
			}
			else
				sPopUpString2[0] = L'\0';

			usPopUpBoxPosX = static_cast<UINT16>(layout.popup.x);
			usPopUpBoxPosY = static_cast<UINT16>(layout.popup.y);

			// load the popup box graphic
			VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
			FilenameForBPP("LAPTOP\\VideoConfPopUp.sti", VObjectDesc.ImageFile);
			CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiPopUpBox));

			GetVideoObject(&hPopupBoxHandle, guiPopUpBox);
			BltVideoObject(FRAME_BUFFER, hPopupBoxHandle, 0,layout.popup.x, layout.popup.y, VO_BLT_SRCTRANSPARENCY,NULL);

			//Create the popup boxes button
			CHECKF(stagedResources.addButtonImage(
				LoadButtonImageOwned("LAPTOP\\VideoConfButtons.sti", -1,2,-1,3,-1),
				guiPopUpImage));
			const INT32 popupButton = CreateIconAndTextButton( guiPopUpImage, VideoConfercingText[AIM_MEMBER_OK],
													FONT14ARIAL,
															AIM_POPUP_BOX_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
															AIM_POPUP_BOX_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
															TEXT_CJUSTIFIED,
													layout.popupButton.x, layout.popupButton.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH+5,
													DEFAULT_MOVE_CALLBACK, BtnPopUpOkButtonCallback);
			CHECKF(stagedResources.addButton(popupButton, guiPopUpOkButton));
			SetButtonCursor(guiPopUpOkButton, CURSOR_LAPTOP_SCREEN);
			MSYS_SetBtnUserData( guiPopUpOkButton, 0, ubData);

			gAimMembersPopupResources = std::move(stagedResources);
			// Disable the close control only after the popup is complete.
			DisableButton( giXToCloseVideoConfButton );
			fPopUpBoxActive = TRUE;
			gubPopUpBoxAction = AIM_POPUP_DISPLAY;


			// Disable the current video conference buttons
			//EnableDisableCurrentVideoConferenceButtons(TRUE);
			if( gubVideoConferencingPreviousMode == AIM_VIDEO_HIRE_MERC_MODE )
			{
				// Enable the current video conference buttons
				EnableDisableCurrentVideoConferenceButtons(FALSE);
			}



//
//	Create a new flag for the PostButtonRendering function
//
			fReDrawPostButtonRender = TRUE;
		}
		break;

		case AIM_POPUP_DISPLAY:
		{
			HVOBJECT			hPopupBoxHandle;
			UINT16				usTempPosY = usPopUpBoxPosY;

			if( gubPopUpBoxAction != AIM_POPUP_DISPLAY)
				return(FALSE);

			// load and display the popup box graphic
			GetVideoObject(&hPopupBoxHandle, guiPopUpBox);
			BltVideoObject(FRAME_BUFFER, hPopupBoxHandle, 0,usPopUpBoxPosX, usPopUpBoxPosY, VO_BLT_SRCTRANSPARENCY,NULL);

			SetFontShadow(AIM_M_VIDEO_NAME_SHADOWCOLOR);

			usTempPosY += layout.popupFirstTextY - layout.popup.y;
			if( sPopUpString1[0]	!= L'\0')
				usTempPosY += DisplayWrappedString(usPopUpBoxPosX, usTempPosY, layout.popup.width, 2, AIM_POPUP_BOX_FONT, AIM_POPUP_BOX_COLOR,	sPopUpString1, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
			if( sPopUpString2[0] != L'\0')
				DisplayWrappedString(usPopUpBoxPosX, (UINT16)(usTempPosY+4), layout.popup.width, 2, AIM_POPUP_BOX_FONT, AIM_POPUP_BOX_COLOR,	sPopUpString2, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

			SetFontShadow(DEFAULT_SHADOW);

			InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);

		}
		break;

		case AIM_POPUP_DELETE:
		{
			if( !fPopUpBoxActive )
				return(FALSE);

			//Disable the 'X' to close the pop upi video
			EnableButton( giXToCloseVideoConfButton );

			gAimMembersPopupResources.clear();

			fPopUpBoxActive = FALSE;
			gubPopUpBoxAction = AIM_POPUP_NOTHING;

			if( gubVideoConferencingPreviousMode == AIM_VIDEO_HIRE_MERC_MODE )
			{
				// Enable the current video conference buttons
				EnableDisableCurrentVideoConferenceButtons(FALSE);
			}
			else if( gubVideoConferencingPreviousMode == AIM_VIDEO_MERC_ANSWERING_MACHINE_MODE )
			{
				EnableButton( giAnsweringMachineButton[1]);
			}
		}
		break;
	}

	return(TRUE);
}


void BtnPopUpOkButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	static BOOLEAN fInCallback=TRUE;

	if( fInCallback )
	{
		if (!(btn->uiFlags & BUTTON_ENABLED))
			return;

		if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
		{
			btn->uiFlags |= BUTTON_CLICKED_ON;
			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
		if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
		{
			UINT8 ubCurPageNum = (UINT8)MSYS_GetBtnUserData( btn, 0 );

			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			fInCallback = FALSE;

//			gfStopMercFromTalking = TRUE;

			gubPopUpBoxAction = AIM_POPUP_DELETE;

			if( gubVideoConferencingMode != AIM_VIDEO_NOT_DISPLAYED_MODE )
			{
				if( ubCurPageNum == AIM_POPUP_BOX_SUCCESS)
				{
					gubVideoConferencingMode = AIM_VIDEO_HIRE_MERC_MODE;
					WaitForMercToFinishTalkingOrUserToClick();
				}
					//				gubVideoConferencingMode = AIM_VIDEO_POPDOWN_MODE;
				else
					gubVideoConferencingMode = AIM_VIDEO_HIRE_MERC_MODE;
			}

			fInCallback = TRUE;

			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
}

// we first contact merc.	We either go to hire him or cancel the call
void BtnFirstContactButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			UINT8	ubRetValue = (UINT8)MSYS_GetBtnUserData( btn, 0 );

//			gfStopMercFromTalking = TRUE;
			StopMercTalking();

			gfAimMemberCanMercSayOpeningQuote = FALSE;

			if( ubRetValue == 0)
			{
				if( CanMercBeHired() )
				{
					gubVideoConferencingMode = AIM_VIDEO_HIRE_MERC_MODE;
				}
			}
			else
			{
				gubVideoConferencingMode = AIM_VIDEO_POPDOWN_MODE;
			}

			btn->uiFlags &= (~BUTTON_CLICKED_ON );

		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


void BtnAnsweringMachineButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			UINT8	ubRetValue = (UINT8)MSYS_GetBtnUserData( btn, 0 );

			if( ubRetValue == 0)
			{
				//Set a flag indicating that the merc has a message
				gMercProfiles[ gbCurrentSoldier ].ubMiscFlags3 |= PROFILE_MISC_FLAG3_PLAYER_LEFT_MSG_FOR_MERC_AT_AIM;
				WaitForMercToFinishTalkingOrUserToClick();

				//Display a message box displaying a messsage that the message was recorded
//				DoLapTopMessageBox( 10, AimPopUpText[ AIM_MEMBER_MESSAGE_RECORDED ], LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL );
				InitCreateDeleteAimPopUpBox( AIM_POPUP_CREATE, L" ", AimPopUpText[ AIM_MEMBER_MESSAGE_RECORDED ], AIM_POPUP_BOX_SUCCESS );


				SpecifyDisabledButtonStyle( giAnsweringMachineButton[1], DISABLED_STYLE_NONE );
				DisableButton( giAnsweringMachineButton[1]);
				DisableButton( giAnsweringMachineButton[0] );
			}
			else
			{
				gubVideoConferencingMode = AIM_VIDEO_POPDOWN_MODE;
//				gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;
			}

			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
}


void BtnHangUpButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateAimMemberContactButton();
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
//			gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;
			gubVideoConferencingMode = AIM_VIDEO_POPDOWN_MODE;

			btn->uiFlags &= (~BUTTON_CLICKED_ON );

			InvalidateAimMemberContactButton();
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateAimMemberContactButton();
	}
}

// InitVideoFace() is called once to initialize things
BOOLEAN	InitVideoFace(UINT8 ubMercID)
{
	// Free any still-active video face before overwriting giMercFaceIndex -- otherwise every
	// merc contact leaks a gFacesData[] slot (and its restore surface), which accumulates across
	// a session toward pool exhaustion. DeleteFace self-guards a -1 index.
	if ( gfVideoFaceActive && giMercFaceIndex != -1 )
	{
		DeleteFace( giMercFaceIndex );
		gfVideoFaceActive = FALSE;
		giMercFaceIndex = -1;
	}

	//Create the facial index
	giMercFaceIndex = InitFace( ubMercID, NOBODY, 0 );

	// InitFace returns -1 when the face pool (NUM_FACE_SLOTS) is exhausted. Do NOT activate or
	// index gFacesData[-1] with it -- bail so the contact screen degrades instead of OOB-writing.
	if ( giMercFaceIndex == -1 )
	{
		gfVideoFaceActive = FALSE;
		return(FALSE);
	}

	SetAutoFaceActive( guiVideoFaceBackground, FACE_AUTO_RESTORE_BUFFER , giMercFaceIndex, 0, 0);

	RenderAutoFace( giMercFaceIndex );

	gubCurrentStaticMode = VC_NO_STATIC;

	gfVideoFaceActive = TRUE;

	guiMercAttitudeTime = GetJA2Clock();

	return(TRUE);
}


// InitVideoFaceTalking() is called to start a merc speaking a particular message
BOOLEAN	InitVideoFaceTalking(UINT8 ubMercID, UINT16 usQuoteNum)
{

	//Starts the merc talking
	if(!CharacterDialogue( ubMercID, usQuoteNum, giMercFaceIndex, DIALOGUE_CONTACTPAGE_UI, FALSE , FALSE) )
	{
		return(FALSE);
	}

	//Enables it so if a player clicks, he will shutup the merc
	MSYS_EnableRegion(&gSelectedShutUpMercRegion);

	gfIsShutUpMouseRegionActive = TRUE;
	gfMercIsTalking = TRUE;
	guiTimeThatMercStartedTalking	= GetJA2Clock();
	return(TRUE);
}


BOOLEAN DisplayTalkingMercFaceForVideoPopUp(INT32	iFaceIndex)
{
	static BOOLEAN fWasTheMercTalking=FALSE;
	BOOLEAN		fIsTheMercTalking;
	SGPRect		SrcRect;
	SGPRect		DestRect;
	const auto face = CurrentAimVideoConferenceLayout().face;


	//Test
	SrcRect.iLeft = 0;
	SrcRect.iTop = 0;
	SrcRect.iRight = 48;
	SrcRect.iBottom = 43;

	DestRect.iLeft = face.x;
	DestRect.iTop = face.y;
	DestRect.iRight = face.right();
	DestRect.iBottom = face.bottom();



	// Guard against a -1 (pool-exhausted) face index reaching gFacesData[-1] / the auto-face handlers.
	if ( iFaceIndex == -1 )
		return(FALSE);

	//If the answering machine graphics is up, dont handle the faces
	if( gfIsAnsweringMachineActive )
	{
		gFacesData[ iFaceIndex ].fInvalidAnim = TRUE;
	}

	HandleDialogue();
	HandleAutoFaces( );
	HandleTalkingAutoFaces( );

	//If the answering machine is up, dont display the face
//	if( !gfIsAnsweringMachineActive )
	{
		//Blt the face surface to the video background surface
		if(	!BltStretchVideoSurface(FRAME_BUFFER, guiVideoFaceBackground, 0, 0, VO_BLT_SRCTRANSPARENCY, &SrcRect, &DestRect ) )
			return(FALSE);

		//if the merc is not at home and the players is leaving a message, shade the players face
		if( gfIsAnsweringMachineActive )
			ShadowVideoSurfaceRect( FRAME_BUFFER, DestRect.iLeft, DestRect.iTop, DestRect.iRight-1, DestRect.iBottom-1 );


		//If the answering machine graphics is up, place a message on the screen
		if( gfIsAnsweringMachineActive )
		{
			//display a message over the mercs face
			DisplayWrappedString( face.x, face.y+20, face.width, 2, FONT14ARIAL, 145,	AimPopUpText[ AIM_MEMBER_PRERECORDED_MESSAGE ], FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED );
		}


		InvalidateRegion(face.x, face.y, face.right(), face.bottom());
	}

 	fIsTheMercTalking = gFacesData[iFaceIndex].fTalking;

	//if the merc is talking, reset their attitude time
	if( fIsTheMercTalking )
	{
		//def added 3/18/99
		guiMercAttitudeTime = GetJA2Clock();
	}

	//if the text the merc is saying is really short, extend the time that it is on the screen
	if( ( GetJA2Clock() - guiTimeThatMercStartedTalking ) > usAimMercSpeechDuration )
	{
		//if the merc just stopped talking
		if(fWasTheMercTalking && !fIsTheMercTalking )
		{
			fWasTheMercTalking = FALSE;

			gfRedrawScreen = TRUE;
			guiMercAttitudeTime = GetJA2Clock();

			StopMercTalking();
		}
	}
	else if( fIsTheMercTalking )
	{
		fWasTheMercTalking = fIsTheMercTalking;
	}

	return(fIsTheMercTalking);
}

void DisplayTextForMercFaceVideoPopUp(STR16 pString)
{
	swprintf( gsTalkingMercText, L"\"%s\"", pString );

	//Set the minimum time for the dialogue text to be present
	usAimMercSpeechDuration =	wcslen( gsTalkingMercText ) * AIM_TEXT_SPEECH_MODIFIER;

	if( usAimMercSpeechDuration < MINIMUM_TALKING_TIME_FOR_MERC )
		usAimMercSpeechDuration = MINIMUM_TALKING_TIME_FOR_MERC;


	gfRedrawScreen = TRUE;
}



void SelectShutUpMercRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	}
	else if (iReason & (MSYS_CALLBACK_REASON_RBUTTON_UP |
		MSYS_CALLBACK_REASON_LBUTTON_UP))
	{
		gfStopMercFromTalking = TRUE;
	}
}


UINT8 WillMercAcceptCall()
{
	//if merc has hung up on the player twice within a period of time (MERC_ANNOYED_WONT_CONTACT_TIME_MINUTES )the merc cant ber hired
	if( gMercProfiles[ gbCurrentSoldier ].bMercStatus == MERC_ANNOYED_WONT_CONTACT )
	{
		return(AIM_VIDEO_MERC_UNAVAILABLE_MODE);
	}

	//if the merc is currently on contract, the answering machine will pick up.
	if( (gMercProfiles[ gbCurrentSoldier ].bMercStatus > 0 ) || (gMercProfiles[ gbCurrentSoldier ].bMercStatus == MERC_HAS_NO_TEXT_FILE	) || (gMercProfiles[ gbCurrentSoldier ].bMercStatus == MERC_HIRED_BUT_NOT_ARRIVED_YET	) )
	{
		return(AIM_VIDEO_MERC_ANSWERING_MACHINE_MODE);
	}

	// if the merc is at home, or if the merc is only slightly annoyed at the player,	he will greet the player
	if( IsMercHireable( gbCurrentSoldier ) )
	{
		return(AIM_VIDEO_FIRST_CONTACT_MERC_MODE);
	}
	else
		return(AIM_VIDEO_MERC_ANSWERING_MACHINE_MODE);
}



BOOLEAN CanMercBeHired()
{
	UINT8	bMercID;
	BOOLEAN fRetVal = FALSE;
	BOOLEAN	fBuddyOnTeam=FALSE;
	
	StopMercTalking();

	// if the merc recently came back with poor morale, and hasn't gotten over it yet
	if (gMercProfiles[ gbCurrentSoldier ].ubDaysOfMoraleHangover > 0)
	{
		// then he refuses with a lame excuse.	Buddy or no buddy.
		WaitForMercToFinishTalkingOrUserToClick();
		InitVideoFaceTalking( gbCurrentSoldier, QUOTE_LAME_REFUSAL );
		return( FALSE );
	}


	// loop through the list of people the merc hates
	for ( UINT8 i = 0; i< NUMBER_HATED_MERCS_ONTEAM; ++i )
	{
		//see if someone the merc hates is on the team
		if( i< NUMBER_HATED_MERCS_ONTEAM - 1 )
		{
			bMercID = gMercProfiles[ gbCurrentSoldier ].bHated[i];
		}
		else
		{
			bMercID = gMercProfiles[ gbCurrentSoldier ].bLearnToHate;

			// ignore learn to hate, if he's not a foe yet
			if( gMercProfiles[ gbCurrentSoldier ].bLearnToHateCount > 0 )
				continue;
		}

		if( bMercID < 0 )
			continue;

		//if the hated merc is dead
		if( IsMercDead( bMercID ) )
		{
			//ignore the merc
			continue;
		}

		if( IsMercOnTeam( bMercID, FALSE, FALSE ) )
		{
			//if the merc hates someone on the team, see if a buddy is on the team
			for ( UINT8 j = 0; j< NUMBER_HATED_MERCS_ONTEAM; ++j )
			{
				//if a buddy is on the team, the merc will join
				if( j < NUMBER_HATED_MERCS_ONTEAM - 1 )
				{
					bMercID = gMercProfiles[ gbCurrentSoldier ].bBuddy[j];
				}
				else
				{
					bMercID = gMercProfiles[ gbCurrentSoldier ].bLearnToLike;

					// ignore learn to like, if he's not a buddy yet
					if( gMercProfiles[ gbCurrentSoldier ].bLearnToLikeCount > 0 )
						continue;
				}

				if( bMercID < 0 )
					continue;

				if( IsMercOnTeam( bMercID, FALSE, FALSE ) && !IsMercDead( bMercID ) )
				{
					if ( j == 0 )
					{
						InitVideoFaceTalking(gbCurrentSoldier, QUOTE_JOINING_CAUSE_BUDDY_1_ON_TEAM);
					}
					else if(j == 1 )
					{
						InitVideoFaceTalking(gbCurrentSoldier, QUOTE_JOINING_CAUSE_BUDDY_2_ON_TEAM);
					}
					else if(j == 2 )
					{
						InitVideoFaceTalking(gbCurrentSoldier, QUOTE_JOINING_CAUSE_BUDDY_3_ON_TEAM);
					}
					else if(j == 3 )
					{
						InitVideoFaceTalking(gbCurrentSoldier, QUOTE_JOINING_CAUSE_BUDDY_4_ON_TEAM);
					}
					else if(j == 4 )
					{
						InitVideoFaceTalking(gbCurrentSoldier, QUOTE_JOINING_CAUSE_BUDDY_5_ON_TEAM);
					}
					else
					{
						InitVideoFaceTalking(gbCurrentSoldier, QUOTE_JOINING_CAUSE_LEARNED_TO_LIKE_BUDDY_ON_TEAM);
					}

					return(TRUE);
				}
			}

			// the merc doesnt like anybody on the team
			//if merc doesnt like first hated merc
			if( i == 0)
			{
				if( gMercProfiles[ gbCurrentSoldier ].bHatedTime[ i ] < 24 )
				{
					WaitForMercToFinishTalkingOrUserToClick();
					InitVideoFaceTalking(gbCurrentSoldier, QUOTE_HATED_1_ON_TEAM );
					fRetVal = FALSE;
				}
				else
				{
					InitVideoFaceTalking(gbCurrentSoldier, QUOTE_HATED_1_ON_TEAM_LONGTIMETOHATE );
					fRetVal = TRUE;
				}
			}
			else if( i == 1)
			{
				if( gMercProfiles[ gbCurrentSoldier ].bHatedTime[ i ] < 24 )
				{
					WaitForMercToFinishTalkingOrUserToClick();
					InitVideoFaceTalking(gbCurrentSoldier, QUOTE_HATED_2_ON_TEAM );
					fRetVal = FALSE;
				}
				else
				{
					InitVideoFaceTalking(gbCurrentSoldier, QUOTE_HATED_2_ON_TEAM_LONGTIMETOHATE );
//					DelayMercSpeech( gbCurrentSoldier, QUOTE_HATED_2_ON_TEAM_LONGTIMETOHATE, 750, TRUE, FALSE );
					fRetVal = TRUE;
				}
			}
			else if( i == 2)
			{
				if( gMercProfiles[ gbCurrentSoldier ].bHatedTime[ i ] < 24 )
				{
					WaitForMercToFinishTalkingOrUserToClick();
					InitVideoFaceTalking(gbCurrentSoldier, QUOTE_HATED_3_ON_TEAM );
					fRetVal = FALSE;
				}
				else
				{
					InitVideoFaceTalking(gbCurrentSoldier, QUOTE_HATED_3_ON_TEAM_LONGTIMETOHATE );
					fRetVal = TRUE;
				}
			}
			else if( i == 3)
			{
				if( gMercProfiles[ gbCurrentSoldier ].bHatedTime[ i ] < 24 )
				{
					WaitForMercToFinishTalkingOrUserToClick();
					InitVideoFaceTalking(gbCurrentSoldier, QUOTE_HATED_4_ON_TEAM );
					fRetVal = FALSE;
				}
				else
				{
					InitVideoFaceTalking(gbCurrentSoldier, QUOTE_HATED_4_ON_TEAM_LONGTIMETOHATE );
					fRetVal = TRUE;
				}
			}
			else if( i == 4)
			{
				if( gMercProfiles[ gbCurrentSoldier ].bHatedTime[ i ] < 24 )
				{
					WaitForMercToFinishTalkingOrUserToClick();
					InitVideoFaceTalking(gbCurrentSoldier, QUOTE_HATED_5_ON_TEAM );
					fRetVal = FALSE;
				}
				else
				{
					InitVideoFaceTalking(gbCurrentSoldier, QUOTE_HATED_5_ON_TEAM_LONGTIMETOHATE );
					fRetVal = TRUE;
				}
			}
			else
			{
				WaitForMercToFinishTalkingOrUserToClick();
				InitVideoFaceTalking(gbCurrentSoldier, QUOTE_LEARNED_TO_HATE_MERC_ON_TEAM);
				fRetVal = FALSE;
			}

			return( fRetVal );
		}
	}

	//Is a buddy working on the team
	fBuddyOnTeam = DoesMercHaveABuddyOnTheTeam( gbCurrentSoldier );

	//If the merc doesnt have a buddy on the team
	if( !fBuddyOnTeam )
	{
		// Check the players Death rate
		if( MercThinksDeathRateTooHigh( gbCurrentSoldier ) )
		{
			WaitForMercToFinishTalkingOrUserToClick();
			InitVideoFaceTalking( gbCurrentSoldier, QUOTE_DEATH_RATE_REFUSAL );
			return( FALSE );
		}

		// Check the players Reputation
		if( MercThinksBadReputationTooHigh( gbCurrentSoldier ) )
		{
			WaitForMercToFinishTalkingOrUserToClick();
			InitVideoFaceTalking( gbCurrentSoldier, QUOTE_REPUTATION_REFUSAL );
			return( FALSE );
		}
	}

	return(TRUE);
}


BOOLEAN DisplaySnowBackground()
{
	UINT32		uiCurrentTime = 0;
	HVOBJECT	hSnowHandle;
	UINT8	ubCount;
	const auto face = CurrentAimVideoConferenceLayout().face;

	uiCurrentTime = GetJA2NoPauseClock();	// no-pause: UI animation must advance even when game-time is paused, else the contact opening sticks on static forever

	if(gubCurrentCount < VC_NUM_LINES_SNOW)
	{
		ubCount = gubCurrentCount;
	}
	else if( gubCurrentCount < VC_NUM_LINES_SNOW*2 )
	{
		ubCount = gubCurrentCount - VC_NUM_LINES_SNOW;
	}
	else
	{
		gfFirstTimeInContactScreen = FALSE;
		gubCurrentCount = 0;

		if( gubVideoConferencingMode == AIM_VIDEO_FIRST_CONTACT_MERC_MODE && gfAimMemberCanMercSayOpeningQuote )
			InitVideoFaceTalking(gbCurrentSoldier, QUOTE_GREETING);

		return(TRUE);
	}

	// if it is time to update the snow image
	if( (uiCurrentTime - guiLastHandleMercTime) > VC_CONTACT_STATIC_TIME)
	{
		gubCurrentCount++;
		guiLastHandleMercTime = uiCurrentTime;
	}
	// Get the snow background, and blit it
	GetVideoObject(&hSnowHandle, guiBWSnow);
	BltVideoObject(FRAME_BUFFER, hSnowHandle, ubCount,face.x, face.y, VO_BLT_SRCTRANSPARENCY,NULL);

	InvalidateRegion(face.x, face.y, face.right(), face.bottom());

	return(FALSE);
}


BOOLEAN DisplayBlackBackground(UINT8 ubMaxNumOfLoops)
{
	UINT32		uiCurrentTime = 0;
	const auto face = CurrentAimVideoConferenceLayout().face;

	uiCurrentTime = GetJA2NoPauseClock();	// no-pause: UI animation must advance even when game-time is paused, else the contact opening sticks on static forever

	if(gubCurrentCount >= ubMaxNumOfLoops)
	{
		gubCurrentCount = 0;
		return(TRUE);
	}

	// if it is time to update the snow image
	if( (uiCurrentTime - guiLastHandleMercTime) > VC_CONTACT_STATIC_TIME)
	{
		gubCurrentCount++;
		guiLastHandleMercTime = uiCurrentTime;
	}
	// Blit color to screen
	ColorFillVideoSurfaceArea( FRAME_BUFFER, face.x, face.y, face.right(), face.bottom(), Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );
	InvalidateRegion(face.x, face.y, face.right(), face.bottom());

	return(FALSE);
}




void HandleVideoDistortion()
{
	static UINT32	uiStaticNoiseSound = NO_SAMPLE;
	UINT8		ubOldMode = gubCurrentStaticMode;

	// if we are just entering the contact page, display a snowy background
	if( gfFirstTimeInContactScreen && !gfIsAnsweringMachineActive)
	{
		DisplaySnowBackground();

		//if it is time to start playing another sound
		if( uiStaticNoiseSound == NO_SAMPLE )
		{
			uiStaticNoiseSound = PlayJA2SampleFromFile( "LAPTOP\\static4.wav", RATE_11025, LOWVOLUME, 1, MIDDLEPAN );
		}
	}
	else
	{
		switch( gubCurrentStaticMode )
		{
			case VC_NO_STATIC:
				{
					static UINT32 uiCurTime=0;
					UINT8	ubNum;

					//if the sound is playing, stop it
					if( uiStaticNoiseSound != NO_SAMPLE )
					{
						SoundStop( uiStaticNoiseSound );
						uiStaticNoiseSound = NO_SAMPLE;
					}

					//DECIDE WHICH ONE TO BLIT NEXT
					if( (GetJA2Clock() - uiCurTime) > 2500)
					{
						ubNum = (UINT8)Random( 200 );//125;

						if( ubNum < 15)
							gubCurrentStaticMode = VC_FUZZY_LINE;

						else if( ubNum < 25)
							gubCurrentStaticMode = VC_STRAIGHTLINE;

						else if( ubNum < 35)
							gubCurrentStaticMode = VC_BW_SNOW;

						else if( ubNum < 40)
							gubCurrentStaticMode = VC_PIXELATE;

						else if( ubNum < 80)
							gubCurrentStaticMode = VC_TRANS_SNOW_OUT;

						else if( ubNum < 100)
							gubCurrentStaticMode = VC_TRANS_SNOW_IN;

						uiCurTime = GetJA2Clock();
					}
				}
				break;

			case VC_FUZZY_LINE:
				gubCurrentStaticMode = DisplayDistortionLine(VC_FUZZY_LINE, guiFuzzLine, VC_NUM_FUZZ_LINES);

				//if it is time to start playing another sound
				if( uiStaticNoiseSound == NO_SAMPLE )
				{
					uiStaticNoiseSound = PlayJA2SampleFromFile( "LAPTOP\\static1.wav", RATE_11025, LOWVOLUME, 1, MIDDLEPAN );
				}
				break;

			case VC_STRAIGHTLINE:
				gubCurrentStaticMode = DisplayDistortionLine(VC_STRAIGHTLINE, guiStraightLine, VC_NUM_STRAIGHT_LINES);

				//if it is time to start playing another sound
				if( uiStaticNoiseSound == NO_SAMPLE )
				{
					uiStaticNoiseSound = PlayJA2SampleFromFile( "LAPTOP\\static5.wav", RATE_11025, LOWVOLUME, 1, MIDDLEPAN );
				}
				break;

			case VC_BW_SNOW:
				gubCurrentStaticMode = DisplayDistortionLine(VC_BW_SNOW, guiBWSnow, 5);

				//if it is time to start playing another sound
				if( uiStaticNoiseSound == NO_SAMPLE )
				{
					uiStaticNoiseSound = PlayJA2SampleFromFile( "LAPTOP\\static6.wav", RATE_11025, LOWVOLUME, 1, MIDDLEPAN );
				}
				break;

			case VC_PIXELATE:
				gubCurrentStaticMode = DisplayPixelatedImage(4);

				//if it is time to start playing another sound
				if( uiStaticNoiseSound == NO_SAMPLE )
				{
					uiStaticNoiseSound = PlayJA2SampleFromFile( "LAPTOP\\static3.wav", RATE_11025, LOWVOLUME, 1, MIDDLEPAN );
				}
				break;

			case VC_TRANS_SNOW_OUT:
				gubCurrentStaticMode = DisplayTransparentSnow(VC_TRANS_SNOW_OUT, guiTransSnow, 7, FALSE);

				//if it is time to start playing another sound
				if( uiStaticNoiseSound == NO_SAMPLE )
				{
					uiStaticNoiseSound = PlayJA2SampleFromFile( "LAPTOP\\static5.wav", RATE_11025, LOWVOLUME, 1, MIDDLEPAN );
				}
				break;

			case VC_TRANS_SNOW_IN:
				gubCurrentStaticMode = DisplayTransparentSnow(VC_TRANS_SNOW_IN, guiTransSnow, 7, TRUE);

				//if it is time to start playing another sound
				if( uiStaticNoiseSound == NO_SAMPLE )
				{
					uiStaticNoiseSound = PlayJA2SampleFromFile( "LAPTOP\\static4.wav", RATE_11025, LOWVOLUME, 1, MIDDLEPAN );
				}
				break;
		}

		if( ubOldMode != gubCurrentStaticMode )
		{
			uiStaticNoiseSound = NO_SAMPLE;
		}
	}
}

//returns true when done. else false
UINT8 DisplayTransparentSnow(UINT8 ubMode, UINT32 uiImageIdentifier, UINT8 ubMaxImages, BOOLEAN bForward)
{
	HVOBJECT	hFuzzLineHandle;
	static INT8	bCount= 0;
	UINT32		uiCurrentTime = 0;
	static UINT32	uiLastTime=0;
	const auto face = CurrentAimVideoConferenceLayout().face;

	uiCurrentTime = GetJA2Clock();

	if( (uiCurrentTime - uiLastTime) > 100)
	{
		if( bForward )
		{
			if( bCount > ubMaxImages-1 )
				bCount = 0;
			else
				bCount++;
		}
		else
		{
			if( bCount <= 0 )
				bCount = ubMaxImages-1;
			else
				bCount--;
		}
		uiLastTime = uiCurrentTime;
	}

	if( bCount >= ubMaxImages)
		bCount = ubMaxImages - 1;

	// Get the snow background, and blit it
	GetVideoObject(&hFuzzLineHandle, uiImageIdentifier);
	BltVideoObject(FRAME_BUFFER, hFuzzLineHandle, bCount,face.x, face.y, VO_BLT_SRCTRANSPARENCY,NULL);


	if( bForward )
	{
		if( bCount == ubMaxImages-1)
		{
			bCount = 0;
			return(VC_BW_SNOW);
		}
		else
			return(ubMode);
	}
	else
	{
		if( bCount == 0)
		{
			bCount = 0;
			return(VC_NO_STATIC);
		}
		else
			return(ubMode);
	}
}


//returns true when done. else false
UINT8 DisplayDistortionLine(UINT8 ubMode, UINT32 uiImageIdentifier, UINT8 ubMaxImages)
{
	HVOBJECT	hFuzzLineHandle;
	static UINT8	ubCount=255;
	UINT32		uiCurrentTime = 0;
	static UINT32	uiLastTime=0;
	const auto face = CurrentAimVideoConferenceLayout().face;

	uiCurrentTime = GetJA2Clock();

	if( (uiCurrentTime - uiLastTime) > VC_CONTACT_FUZZY_LINE_TIME)
	{
		if( ubCount >= ubMaxImages-1 )
			ubCount = 0;
		else
			ubCount++;
		uiLastTime = uiCurrentTime;
	}

	if( ubCount >= ubMaxImages)
		ubCount = ubMaxImages - 1;

	// Get the snow background, and blit it
	GetVideoObject(&hFuzzLineHandle, uiImageIdentifier);
	BltVideoObject(FRAME_BUFFER, hFuzzLineHandle, ubCount,face.x, face.y, VO_BLT_SRCTRANSPARENCY,NULL);

	if( ubCount == ubMaxImages-1)
	{
		ubCount = 0;
		if( ubMode == VC_BW_SNOW)
			return(VC_TRANS_SNOW_OUT);
		else
			return(VC_NO_STATIC);
	}
	else
		return(ubMode);
}

UINT8 DisplayPixelatedImage(UINT8 ubMaxImages)
{
	static UINT8	ubCount=255;
	UINT32		uiCurrentTime = 0;
	static UINT32	uiLastTime=0;
	const auto face = CurrentAimVideoConferenceLayout().face;

	uiCurrentTime = GetJA2Clock();

	if( (uiCurrentTime - uiLastTime) > VC_CONTACT_FUZZY_LINE_TIME)
	{
		if( ubCount >= ubMaxImages-1 )
			ubCount = 0;
		else
			ubCount++;
		uiLastTime = uiCurrentTime;
	}

	ShadowVideoSurfaceRect( FRAME_BUFFER, face.x, face.y, face.right()-1, face.bottom()-1);

	if( ubCount == ubMaxImages-1)
	{
		ubCount = 0;
		return(VC_NO_STATIC);
	}
	else
		return(VC_PIXELATE);
}

void HandleMercAttitude()
{
	UINT32		uiCurrentTime = 0;

	uiCurrentTime = GetJA2Clock();

	if( ( gubMercAttitudeLevel <= 1 && ( ( uiCurrentTime -	guiMercAttitudeTime ) > QUOTE_FIRST_ATTITUDE_TIME ) ) ||
			( ( uiCurrentTime -	guiMercAttitudeTime ) > QUOTE_ATTITUDE_TIME ) )
	{

		if( gubMercAttitudeLevel == QUOTE_DELAY_SMALL_TALK)
		{
			InitVideoFaceTalking(gbCurrentSoldier, QUOTE_SMALL_TALK);
		}
		else if( gubMercAttitudeLevel == QUOTE_DELAY_IMPATIENT_TALK)
		{
			InitVideoFaceTalking(gbCurrentSoldier, QUOTE_IMPATIENT_QUOTE);
		}
		else if( gubMercAttitudeLevel == QUOTE_DELAY_VERY_IMPATIENT_TALK)
		{
			InitVideoFaceTalking(gbCurrentSoldier, QUOTE_PRECEDENT_TO_REPEATING_ONESELF);
			InitVideoFaceTalking(gbCurrentSoldier, QUOTE_IMPATIENT_QUOTE);
		}
		else if( gubMercAttitudeLevel == QUOTE_DELAY_HANGUP_TALK)
		{
			UINT32	uiResetTime;
			InitVideoFaceTalking(gbCurrentSoldier, QUOTE_COMMENT_BEFORE_HANG_UP);

			//if the merc is going to hang up disable the buttons, so user cant press any buttons
//			EnableDisableCurrentVideoConferenceButtons( FALSE);
			if( gubVideoConferencingPreviousMode == AIM_VIDEO_HIRE_MERC_MODE )
			{
				// Enable the current video conference buttons
				EnableDisableCurrentVideoConferenceButtons(FALSE);
			}


			//increments the merc 'annoyance' at the player
			if( gMercProfiles[ gbCurrentSoldier ].bMercStatus == 0 )
				gMercProfiles[ gbCurrentSoldier ].bMercStatus = MERC_ANNOYED_BUT_CAN_STILL_CONTACT;
			else if( gMercProfiles[ gbCurrentSoldier ].bMercStatus == MERC_ANNOYED_BUT_CAN_STILL_CONTACT )
				gMercProfiles[ gbCurrentSoldier ].bMercStatus = MERC_ANNOYED_WONT_CONTACT;

			// add an event so we can reset the 'annoyance factor'
			uiResetTime =	( Random( 600 ) );
			uiResetTime += GetWorldTotalMin() + MERC_ANNOYED_WONT_CONTACT_TIME_MINUTES;
			AddStrategicEvent( EVENT_AIM_RESET_MERC_ANNOYANCE, uiResetTime, gbCurrentSoldier );

			gfHangUpMerc = TRUE;
		}

		if( gubMercAttitudeLevel == QUOTE_MERC_BUSY )
		{
			InitVideoFaceTalking(gbCurrentSoldier, QUOTE_LAME_REFUSAL);
			gfHangUpMerc = TRUE;
		}
		else if( gubMercAttitudeLevel != QUOTE_DELAY_NO_ACTION )
			gubMercAttitudeLevel++;

		guiMercAttitudeTime = GetJA2Clock();
	}
}

void StopMercTalking()
{
	if( gfIsShutUpMouseRegionActive )
	{
	MSYS_DisableRegion(&gSelectedShutUpMercRegion);

		ShutupaYoFace( giMercFaceIndex );
		gfMercIsTalking = FALSE;
		guiMercAttitudeTime = GetJA2Clock();
		gfIsShutUpMouseRegionActive = FALSE;
		gfRedrawScreen = TRUE;
	}
}



void BtnXToCloseVideoConfButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
		InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
	}
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			gubVideoConferencingMode = AIM_VIDEO_POPDOWN_MODE;
//			gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;
			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			InvalidateRegion(btn->Area.RegionTopLeftX, btn->Area.RegionTopLeftY, btn->Area.RegionBottomRightX, btn->Area.RegionBottomRightY);
		}
	}
}


BOOLEAN InitDeleteVideoConferencePopUp( )
{
	const CampaignAimSitePolicy aimSitePolicy(
		GetGameContext().capabilities());
	LaptopPageResourceOwner stagedResources;
	UINT8	i;
	VOBJECT_DESC	VObjectDesc;
	VSURFACE_DESC		vs_desc;
	const auto layout = CurrentAimVideoConferenceLayout();

	//remove the face help text
	gfAimMemberDisplayFaceHelpText = FALSE;

	//Gets reset to FALSE in the HandleCurrentVideoConfMode() function
	gfJustSwitchedVideoConferenceMode = TRUE;


	//remove old mode
	DeleteVideoConfPopUp();

	//reset ( in case merc was going to say something
	DelayMercSpeech( 0, 0, 0, FALSE, TRUE );

	//if the video conferencing is currently displayed, put the 'x' to close it in the top right corner
	//and disable the ability to click on the BIG face to go to different screen
	if( ( gubVideoConferencingMode != AIM_VIDEO_NOT_DISPLAYED_MODE) && ( gubVideoConferencingMode != AIM_VIDEO_POPUP_MODE) )
	{
		if(gAimMembersVideoCloseResources.empty())
		{
			const INT32 closeButton = QuickCreateButton(giXToCloseVideoConfButtonImage, layout.closeButton.x, layout.closeButton.y,
																	BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
																	DEFAULT_MOVE_CALLBACK, BtnXToCloseVideoConfButtonCallback);
			CHECKF(gAimMembersVideoCloseResources.addButton(
				closeButton, giXToCloseVideoConfButton));
			SetButtonCursor(giXToCloseVideoConfButton, CURSOR_LAPTOP_SCREEN);
			SpecifyDisabledButtonStyle( giXToCloseVideoConfButton, DISABLED_STYLE_NONE );

			MSYS_DisableRegion(&gSelectedFaceRegion);
		}
	}



	// The video conference is not displayed
	if( gubVideoConferencingMode ==	AIM_VIDEO_NOT_DISPLAYED_MODE )
	{
		gfRedrawScreen = TRUE;

		if( gfVideoFaceActive )
		{
			StopMercTalking();

			//Get rid of the talking face
			DeleteFace(giMercFaceIndex);
		}

		//if the ansering machine is currently on, turn it off
		if( gfIsAnsweringMachineActive)
			gfIsAnsweringMachineActive = FALSE;

		gfVideoFaceActive = FALSE;

		gAimMembersVideoCloseResources.clear();

		MSYS_DisableRegion(&gSelectedShutUpMercRegion);

		//Enable the ability to click on the BIG face to go to different screen
		MSYS_EnableRegion(&gSelectedFaceRegion);

//		EnableDisableCurrentVideoConferenceButtons(FALSE);
		if( gubVideoConferencingPreviousMode == AIM_VIDEO_HIRE_MERC_MODE )
		{
			// Enable the current video conference buttons
			EnableDisableCurrentVideoConferenceButtons(FALSE);
		}


		fNewMailFlag = gfIsNewMailFlagSet;
		gfIsNewMailFlagSet = FALSE;

	}


	if( gubVideoConferencingMode == AIM_VIDEO_POPUP_MODE )
	{
		if( gfJustSwitchedVideoConferenceMode )
		{
			UINT32 uiVideoBackgroundGraphic;
			HVOBJECT hImageHandle;


			// load the answering machine graphic and add it
			VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
			FilenameForBPP("LAPTOP\\VideoTitleBar.sti", VObjectDesc.ImageFile);
			LaptopPageResourceOwner titleGraphic;
			CHECKF(titleGraphic.addVideoObject(
				&VObjectDesc, uiVideoBackgroundGraphic));

			// Create a background video surface to blt the face onto
			vs_desc.fCreateFlags = VSURFACE_CREATE_DEFAULT | VSURFACE_SYSTEM_MEM_USAGE;
			vs_desc.usWidth = layout.titleBarSource.width;
			vs_desc.usHeight = layout.titleBarSource.height;
			vs_desc.ubBitDepth = 16;
			CHECKF(stagedResources.addVideoSurface(
				&vs_desc, guiVideoTitleBar));


			gfAimMemberCanMercSayOpeningQuote = TRUE;

			GetVideoObject(&hImageHandle, uiVideoBackgroundGraphic);
			BltVideoObject(guiVideoTitleBar, hImageHandle, 0,0,0, VO_BLT_SRCTRANSPARENCY,NULL);

		}
	}


	// The opening animation of the vc (fuzzy screen, then goes to black)
	if( gubVideoConferencingMode == AIM_VIDEO_INIT_MODE )
	{
 		gubMercAttitudeLevel = 0;

		if (is_networked)
			gubContractLength = AIM_CONTRACT_LENGTH_ONE_DAY;
		else
			gubContractLength = AIM_CONTRACT_LENGTH_ONE_WEEK;

		if( gMercProfiles[gbCurrentSoldier].usOptionalGearCost == 0 )
			gfBuyEquipment = FALSE;
		else
			gfBuyEquipment = TRUE;

		gfMercIsTalking = FALSE;
		gfVideoFaceActive = FALSE;
		guiLastHandleMercTime = 0;
		gfHangUpMerc = FALSE;

	}

	// The screen in which you first contact the merc, you have the option to hang up or goto hire merc screen
	if( gubVideoConferencingMode ==	AIM_VIDEO_FIRST_CONTACT_MERC_MODE )
	{
		//if the last screen was the init screen, then we need to initialize the video face
		if( ( gubVideoConferencingPreviousMode == AIM_VIDEO_INIT_MODE) || ( gubVideoConferencingPreviousMode == AIM_VIDEO_NOT_DISPLAYED_MODE) )
		{
			//Put the merc face up on the screen
			CHECKF(InitVideoFace(gbCurrentSoldier));

//			if( gubVideoConferencingPreviousMode == AIM_VIDEO_INIT_MODE)
//				InitVideoFaceTalking(gbCurrentSoldier, QUOTE_GREETING);
		}

		// Hang up button
		CHECKF(stagedResources.addButtonImage(
			LoadButtonImageOwned("LAPTOP\\VideoConfButtons.sti", -1,2,-1,3,-1),
			guiVideoConferenceButtonImage[2]));
		for(i=0; i<2; i++)
		{
			const auto buttonPosition = layout.contactButtons.at(i);
			const INT32 button = CreateIconAndTextButton( guiVideoConferenceButtonImage[2], VideoConfercingText[i+AIM_MEMBER_HIRE],
															FONT12ARIAL,
															AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
															AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
															TEXT_CJUSTIFIED,
															buttonPosition.x, buttonPosition.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
																	DEFAULT_MOVE_CALLBACK, BtnFirstContactButtonCallback);
			CHECKF(stagedResources.addButton(button, giAuthorizeButton[i]));

			MSYS_SetBtnUserData( giAuthorizeButton[i], 0, i);
			SetButtonCursor(giAuthorizeButton[i], CURSOR_LAPTOP_SCREEN);
		}

		if( gfWaitingForMercToStopTalkingOrUserToClick )
		{
			DisableButton( giAuthorizeButton[0] );
			gfWaitingForMercToStopTalkingOrUserToClick = FALSE;

			//Display a popup msg box telling the user when and where the merc will arrive
//			DisplayPopUpBoxExplainingMercArrivalLocationAndTime( giIdOfLastHiredMerc );
			giIdOfLastHiredMerc = -1;
		}
	}



		// The screen in which you set the contract length, and the ability to buy equipment..
	if( gubVideoConferencingMode == AIM_VIDEO_HIRE_MERC_MODE)
	{
		// Contract Length button
		CHECKF(stagedResources.addButtonImage(
			LoadButtonImageOwned("LAPTOP\\VideoConfButtons.sti", -1,0,-1,1,-1),
			guiVideoConferenceButtonImage[0]));
		for(i=0; i<3; i++)
		{
			const auto buttonPosition = layout.contractButtons.at(i);
			const INT32 button = CreateIconAndTextButton( guiVideoConferenceButtonImage[0], VideoConfercingText[i+AIM_MEMBER_ONE_DAY], FONT12ARIAL,
																AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
																AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
																TEXT_LJUSTIFIED,
																buttonPosition.x, buttonPosition.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
																		DEFAULT_MOVE_CALLBACK, BtnContractLengthButtonCallback);
			CHECKF(stagedResources.addButton(button, giContractLengthButton[i]));

			SetButtonCursor(giContractLengthButton[i], CURSOR_LAPTOP_SCREEN);
			MSYS_SetBtnUserData( giContractLengthButton[i], 0, i);
			SpecifyDisabledButtonStyle( giContractLengthButton[i], DISABLED_STYLE_NONE );
		}

		if(is_client)//hayden : only needed for 1 day...
		{
						DisableButton( giContractLengthButton[1] );
						DisableButton( giContractLengthButton[2] );
		}
		// BuyEquipment button
		for(i=0; i<2; i++)
		{
			const auto buttonPosition = layout.equipmentButtons.at(i);
			const INT32 button = CreateIconAndTextButton( guiVideoConferenceButtonImage[0], VideoConfercingText[i+AIM_MEMBER_NO_EQUIPMENT],
																	FONT12ARIAL,
																AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
																AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
																TEXT_LJUSTIFIED,
															buttonPosition.x, buttonPosition.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
																	DEFAULT_MOVE_CALLBACK, BtnBuyEquipmentButtonCallback);
			CHECKF(stagedResources.addButton(button, giBuyEquipmentButton[i]));

			SetButtonCursor(giBuyEquipmentButton[i], CURSOR_LAPTOP_SCREEN);
			MSYS_SetBtnUserData( giBuyEquipmentButton[i], 0, i);
			SpecifyDisabledButtonStyle( giBuyEquipmentButton[i], DISABLED_STYLE_SHADED );
		}
		if( gMercProfiles[gbCurrentSoldier].usOptionalGearCost == 0 )
			DisableButton( giBuyEquipmentButton[1] );

		if(!cAllowMercEquipment && is_networked)
		{
			gfBuyEquipment = FALSE;
			DisableButton( giBuyEquipmentButton[0] );
			DisableButton( giBuyEquipmentButton[1] );
		}

		// Authorize button
		CHECKF(stagedResources.addButtonImage(
			LoadButtonImageOwned("LAPTOP\\VideoConfButtons.sti", -1,2,-1,3,-1),
			guiVideoConferenceButtonImage[1]));
		for(i=0; i<2; i++)
		{
			const auto buttonPosition = layout.authorizationButtons.at(i);
			const INT32 button = CreateIconAndTextButton( guiVideoConferenceButtonImage[1], VideoConfercingText[i+AIM_MEMBER_TRANSFER_FUNDS],
																FONT12ARIAL,
																AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
																AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
																TEXT_CJUSTIFIED,
															buttonPosition.x, buttonPosition.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
																		DEFAULT_MOVE_CALLBACK, BtnAuthorizeButtonCallback);
			CHECKF(stagedResources.addButton(button, giAuthorizeButton[i]));

			SetButtonCursor(giAuthorizeButton[i], CURSOR_LAPTOP_SCREEN);
			MSYS_SetBtnUserData( giAuthorizeButton[i], 0, i);
			SpecifyDisabledButtonStyle( giAuthorizeButton[i], DISABLED_STYLE_NONE );
		}

//		InitVideoFaceTalking(gbCurrentSoldier, QUOTE_LENGTH_OF_CONTRACT);
		DelayMercSpeech( gbCurrentSoldier, QUOTE_LENGTH_OF_CONTRACT, 750, TRUE, FALSE );

		if (aimSitePolicy.hidesContractAndEquipmentButtons())
		{
			// Disable and hide contract length & gear purchase buttons
			gfBuyEquipment = aimSitePolicy.forcesEquipmentPurchase();
			for (size_t i = 0; i < 3; i++)
			{
				DisableButton(giContractLengthButton[i]);
				HideButton(giContractLengthButton[i]);

				if (i < 2)
				{
					DisableButton(giBuyEquipmentButton[i]);
					HideButton(giBuyEquipmentButton[i]);
				}
			}
		}
	}




		// The merc is not home and the player gets the answering machine
	if( gubVideoConferencingMode ==	AIM_VIDEO_MERC_ANSWERING_MACHINE_MODE )
	{
		gfIsAnsweringMachineActive = TRUE;

		// Leave msg button
		CHECKF(stagedResources.addButtonImage(
			LoadButtonImageOwned("LAPTOP\\VideoConfButtons.sti", -1,2,-1,3,-1),
			guiVideoConferenceButtonImage[2]));

		const auto leaveMessagePosition = layout.contactButtons.at(0);
		const INT32 leaveMessageButton = CreateIconAndTextButton( guiVideoConferenceButtonImage[2], VideoConfercingText[AIM_MEMBER_LEAVE_MESSAGE],
														FONT12ARIAL,
														AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
														AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
														TEXT_CJUSTIFIED,
														leaveMessagePosition.x, leaveMessagePosition.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
																DEFAULT_MOVE_CALLBACK, BtnAnsweringMachineButtonCallback);
		CHECKF(stagedResources.addButton(
			leaveMessageButton, giAnsweringMachineButton[0]));
		MSYS_SetBtnUserData( giAnsweringMachineButton[0], 0, 0);
		SetButtonCursor(giAnsweringMachineButton[0], CURSOR_LAPTOP_SCREEN);

		//if the user has already left a message, disable the button
		if( gMercProfiles[ gbCurrentSoldier ].ubMiscFlags3 & PROFILE_MISC_FLAG3_PLAYER_LEFT_MSG_FOR_MERC_AT_AIM )
			DisableButton( giAnsweringMachineButton[0] );

		const auto hangUpPosition = layout.contactButtons.at(1);
		const INT32 hangUpButton = CreateIconAndTextButton( guiVideoConferenceButtonImage[2], VideoConfercingText[AIM_MEMBER_HANG_UP],
														FONT12ARIAL,
														AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
														AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
														TEXT_CJUSTIFIED,
														hangUpPosition.x, hangUpPosition.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
																DEFAULT_MOVE_CALLBACK, BtnAnsweringMachineButtonCallback);
		CHECKF(stagedResources.addButton(
			hangUpButton, giAnsweringMachineButton[1]));

		MSYS_SetBtnUserData( giAnsweringMachineButton[1], 0, 1);
		SetButtonCursor(giAnsweringMachineButton[1], CURSOR_LAPTOP_SCREEN);

		//The face must be inited even though the face wont appear.	It is so the voice is played
		CHECKF(InitVideoFace(gbCurrentSoldier));

		//Make sure the merc doesnt ramble away to the player
		gubMercAttitudeLevel = QUOTE_DELAY_NO_ACTION;


		gubCurrentStaticMode = VC_NO_STATIC;
	}




	// The merc is home but for some reason doesnt want to work for player
	if( gubVideoConferencingMode == AIM_VIDEO_MERC_UNAVAILABLE_MODE)
	{
		const auto hangUpPosition = layout.unavailableHangUp;
		// The hangup button
		CHECKF(stagedResources.addButtonImage(
			LoadButtonImageOwned("LAPTOP\\VideoConfButtons.sti", -1,2,-1,3,-1),
			guiVideoConferenceButtonImage[2]));

		const INT32 hangUpButton = CreateIconAndTextButton( guiVideoConferenceButtonImage[2], VideoConfercingText[AIM_MEMBER_HANG_UP],
														FONT12ARIAL,
														AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
														AIM_M_VIDEO_NAME_COLOR, AIM_M_VIDEO_NAME_SHADOWCOLOR,
														TEXT_CJUSTIFIED,
														hangUpPosition.x, hangUpPosition.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
																DEFAULT_MOVE_CALLBACK, BtnHangUpButtonCallback);
		CHECKF(stagedResources.addButton(hangUpButton, giHangUpButton));

		MSYS_SetBtnUserData( giHangUpButton, 0, 1);
		SetButtonCursor(giHangUpButton, CURSOR_LAPTOP_SCREEN);

		//set the flag saying specifying that merc is busy
		gubMercAttitudeLevel = QUOTE_MERC_BUSY;

		CHECKF(InitVideoFace(gbCurrentSoldier));
	}

	if( gubVideoConferencingMode == 	AIM_VIDEO_POPDOWN_MODE )
	{
		UINT32 uiVideoBackgroundGraphic;
		HVOBJECT hImageHandle;

		if( gubPopUpBoxAction == AIM_POPUP_DISPLAY )
		{
			return( TRUE );
		}

		gfIsAnsweringMachineActive = FALSE;


		// load the Video conference background graphic and add it
		VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
		FilenameForBPP("LAPTOP\\VideoTitleBar.sti", VObjectDesc.ImageFile);
		LaptopPageResourceOwner titleGraphic;
		CHECKF(titleGraphic.addVideoObject(
			&VObjectDesc, uiVideoBackgroundGraphic));

		// Create a background video surface to blt the face onto
		vs_desc.fCreateFlags = VSURFACE_CREATE_DEFAULT | VSURFACE_SYSTEM_MEM_USAGE;
		vs_desc.usWidth = layout.titleBarSource.width;
		vs_desc.usHeight = layout.titleBarSource.height;
		vs_desc.ubBitDepth = 16;
		CHECKF(stagedResources.addVideoSurface(
			&vs_desc, guiVideoTitleBar));


		GetVideoObject(&hImageHandle, uiVideoBackgroundGraphic);
		BltVideoObject(guiVideoTitleBar, hImageHandle, 0,0,0, VO_BLT_SRCTRANSPARENCY,NULL);

	}

//	gfWaitingForMercToStopTalkingOrUserToClick = FALSE;

	//reset the time in which the merc will get annoyed
	guiMercAttitudeTime = GetJA2Clock();
	gAimMembersVideoConferenceResources = std::move(stagedResources);
	gubVideoConferencingPreviousMode = gubVideoConferencingMode;
	return(TRUE);
}


BOOLEAN DeleteVideoConfPopUp()
{
	//reset ( in case merc was going to say something
	DelayMercSpeech( 0, 0, 0, FALSE, TRUE );

	if (gubVideoConferencingPreviousMode == AIM_VIDEO_POPDOWN_MODE)
	{
		// Preserve the title surface while a modal popup postpones the
		// pop-down transition.
		if (gubPopUpBoxAction == AIM_POPUP_DISPLAY)
			return TRUE;

		if (gfWaitingForMercToStopTalkingOrUserToClick)
		{
			gfWaitingForMercToStopTalkingOrUserToClick = FALSE;

//			DisplayPopUpBoxExplainingMercArrivalLocationAndTime( giIdOfLastHiredMerc );
		}
		gfWaitingForMercToStopTalkingOrUserToClick = FALSE;
	}

	gAimMembersVideoConferenceResources.clear();
	return(TRUE);
}




BOOLEAN HandleCurrentVideoConfMode()
{
	switch(	gubVideoConferencingMode )
	{
		// The video conference is not displayed
		case AIM_VIDEO_NOT_DISPLAYED_MODE:
		{
			gfWaitingForMercToStopTalkingOrUserToClick = FALSE;

			break;
		}


		case AIM_VIDEO_POPUP_MODE:
		{
			BOOLEAN ubDone;

			if( gfJustSwitchedVideoConferenceMode )
				ubDone = DisplayMovingTitleBar( TRUE, TRUE );
			else
				ubDone = DisplayMovingTitleBar( TRUE, FALSE );


			if(ubDone)
				gubVideoConferencingMode = AIM_VIDEO_INIT_MODE;

			break;
		}

		// The opening animation of the vc (fuzzy screen, then goes to black)
		case AIM_VIDEO_INIT_MODE:
		{
			static UINT8 ubCurMode = 0;
			BOOLEAN	fDone;

			if( ubCurMode == 0 )
			{
				fDone = DisplayBlackBackground( 10 );
				if( fDone )
					ubCurMode = 1;
			}
			else
				fDone = DisplaySnowBackground();

			if( fDone && ubCurMode )
			{
				ubCurMode = 0;

				gubVideoConferencingMode = WillMercAcceptCall();
			}

			break;
		}

		// The screen in which you first contact the merc, you have the option to hang up or goto hire merc screen
		case AIM_VIDEO_FIRST_CONTACT_MERC_MODE:
		{
			//if the merc is at home, play his greeting
//			if( gfJustSwitchedVideoConferenceMode )
//				InitVideoFaceTalking(gbCurrentSoldier, QUOTE_GREETING);

			break;
		}

		// The screen in which you set the contract length, and the ability to buy equipment..
		case AIM_VIDEO_HIRE_MERC_MODE:
		{

			break;
		}

		// The merc is not home and the player gets the answering machine
		case AIM_VIDEO_MERC_ANSWERING_MACHINE_MODE:
		{
			//if the merc is not at home, play his answering machine
			if( gfJustSwitchedVideoConferenceMode )
			{
				InitVideoFaceTalking(gbCurrentSoldier, QUOTE_ANSWERING_MACHINE_MSG);
			}

			break;
		}

		// The merc is home but doesnt want to work for player
		case AIM_VIDEO_MERC_UNAVAILABLE_MODE:
		{

			break;
		}

		case AIM_VIDEO_POPDOWN_MODE:
		{
			BOOLEAN ubDone;

			if( gfJustSwitchedVideoConferenceMode )
				ubDone = DisplayMovingTitleBar( FALSE, TRUE );
			else
				ubDone = DisplayMovingTitleBar( FALSE, FALSE );

			if(ubDone)
			{
				gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;


				//display the popup telling the user when the just hired merc is going to land
				DisplayPopUpBoxExplainingMercArrivalLocationAndTime( /*giIdOfLastHiredMerc*/ ); // no parameters (jonathanl)

				//render the screen immediately to get rid of the pop down stuff
				InitDeleteVideoConferencePopUp( );
				RenderAIMMembers();
				gfVideoFaceActive = FALSE;
			}

			break;
		}
	}

	//Gets set in the InitDeleteVideoConferencePopUp() function
//	gfJustSwitchedVideoConferenceMode = FALSE;

	return(TRUE);
}



BOOLEAN EnableDisableCurrentVideoConferenceButtons( BOOLEAN fEnable)
{
	INT8	i;
	static BOOLEAN fCreated = FALSE;
	if(!fEnable)
	{
		if( fCreated )
		{
			//enable buttons behind the acknowlegde button

			for( i=0; i<3; i++)
				EnableButton( giContractLengthButton[i] );

			for(i=0; i<2; i++)
				EnableButton( giBuyEquipmentButton[i] );

			for(i=0; i<2; i++)
				EnableButton( giAuthorizeButton[i] );

			fCreated = FALSE;
		}
	}
	else
	{
		if( !fCreated )
		{
			//disable buttons behind the acknowlegde button
			for( i=0; i<3; i++)
				DisableButton( giContractLengthButton[i] );

			for(i=0; i<2; i++)
				DisableButton( giBuyEquipmentButton[i] );

			for(i=0; i<2; i++)
				DisableButton( giAuthorizeButton[i] );


			fCreated = TRUE;
		}
	}
	return(TRUE);
}

/*
BOOLEAN HandleAnsweringMachineMessage()
{
	static BOOLEAN fDone;

	if( gfJustSwitchedVideoConferenceMode )
		fDone = DisplayAnimatedAnsweringMachineMsg( TRUE,	12);
	else if( !fDone )
	{
		fDone = DisplayAnimatedAnsweringMachineMsg( FALSE,	12);
		if( fDone )
		{
			fDone = FALSE;
		}
	}

	return(TRUE);
}
*/
void ResetMercAnnoyanceAtPlayer( UINT8 ubMercID )
{
	//if merc is still annoyed, reset back to 0

	if ( ubMercID == LARRY_NORMAL	)
	{
		if ( CheckFact( FACT_LARRY_CHANGED, 0 ) )
		{
			ubMercID = LARRY_DRUNK;
		}
	}
	else if ( ubMercID == LARRY_DRUNK )
	{
		if ( CheckFact( FACT_LARRY_CHANGED, 0 ) == FALSE )
		{
			ubMercID = LARRY_NORMAL;
		}
	}
	if( ( gMercProfiles[ ubMercID ].bMercStatus == MERC_ANNOYED_WONT_CONTACT ) || ( gMercProfiles[ ubMercID ].bMercStatus == MERC_ANNOYED_BUT_CAN_STILL_CONTACT ) )
		gMercProfiles[ ubMercID ].bMercStatus = 0;
}

BOOLEAN DisableNewMailMessage()
{
	if( fNewMailFlag && gubVideoConferencingMode )
	{
		gfIsNewMailFlagSet = TRUE;
		fNewMailFlag = FALSE;
		gfRedrawScreen = TRUE;

		return(TRUE);

	}
	return(FALSE);
}

BOOLEAN DisplayMovingTitleBar(BOOLEAN fForward, BOOLEAN fInit )
{
	static 	UINT8			ubCount;
	UINT16		usWidth, usHeight;
	SGPRect		SrcRect;
	SGPRect		DestRect;
	static SGPRect		LastRect;
	const auto layout = CurrentAimVideoConferenceLayout();
	constexpr auto iterations =
		AimWebsiteLayoutModel::VideoConferenceLayout::kTitleAnimationIterations;

	if( fForward )
	{
		if( fInit )
			ubCount = 1;
	}
	else
	{
		if( fInit )
			ubCount = static_cast<UINT8>(iterations - 1);
	}

	const auto frame = layout.titleFrame(ubCount);
	SrcRect.iLeft = layout.titleBarSource.x;
	SrcRect.iTop = layout.titleBarSource.y;
	SrcRect.iRight = layout.titleBarSource.right();
	SrcRect.iBottom = layout.titleBarSource.bottom();
	DestRect.iLeft = frame.x;
	DestRect.iTop = frame.y;
	DestRect.iRight = frame.right();
	DestRect.iBottom = frame.bottom();

	if( fForward )
	{
		//Restore the old rect
		if( ubCount > 2 )
		{
			usWidth = (UINT16)(LastRect.iRight - LastRect.iLeft);
			usHeight = (UINT16)(LastRect.iBottom - LastRect.iTop);
			BlitBufferToBuffer(guiSAVEBUFFER, guiRENDERBUFFER, (UINT16)LastRect.iLeft, (UINT16)LastRect.iTop, usWidth, usHeight);
		}

		//Save rectangle
		if( ubCount > 1 )
		{
			usWidth = (UINT16)(DestRect.iRight - DestRect.iLeft);
			usHeight = (UINT16)(DestRect.iBottom - DestRect.iTop);
			BlitBufferToBuffer(guiRENDERBUFFER, guiSAVEBUFFER, (UINT16)DestRect.iLeft, (UINT16)DestRect.iTop, usWidth, usHeight);
		}
	}
	else
	{
		//Restore the old rect
		if( ubCount < iterations - 2 )
		{
			usWidth = (UINT16)(LastRect.iRight - LastRect.iLeft);
			usHeight = (UINT16)(LastRect.iBottom - LastRect.iTop);
			BlitBufferToBuffer(guiSAVEBUFFER, guiRENDERBUFFER, (UINT16)LastRect.iLeft, (UINT16)LastRect.iTop, usWidth, usHeight);
		}

		//Save rectangle
		if( ubCount < iterations - 1 )
		{
			usWidth = (UINT16)(DestRect.iRight - DestRect.iLeft);
			usHeight = (UINT16)(DestRect.iBottom - DestRect.iTop);
			BlitBufferToBuffer(guiRENDERBUFFER, guiSAVEBUFFER, (UINT16)DestRect.iLeft, (UINT16)DestRect.iTop, usWidth, usHeight);
		}
	}



	BltStretchVideoSurface(FRAME_BUFFER, guiVideoTitleBar, 0, 0, VO_BLT_SRCTRANSPARENCY, &SrcRect, &DestRect );

	InvalidateRegion(DestRect.iLeft,DestRect.iTop, DestRect.iRight, DestRect.iBottom);
	InvalidateRegion(LastRect.iLeft,LastRect.iTop, LastRect.iRight, LastRect.iBottom);

	LastRect = DestRect;

	if( fForward )
	{
		ubCount++;
		if( ubCount == iterations-1)
			return(TRUE);
		else
			return(FALSE);
	}
	else
	{
		ubCount--;
		if( ubCount == 0)
			return(TRUE);
		else
			return(FALSE);
	}
}


#ifdef JA2TESTVERSION
//TEMP:
void TempHiringOfMercs( UINT8 ubNumberOfMercs, BOOLEAN fReset )
{
	INT16	i;
	UINT8	MercID[]={11,16,29,36,2,10,17,6,7,12,0,1,3,4,5,8,9,13,14,15,18,19 };
	MERC_HIRE_STRUCT HireMercStruct;
	static BOOLEAN	fHaveCalledBefore=FALSE;

	//if we should reset the global variable
	if( fReset )
	{
		fHaveCalledBefore = FALSE;
		return;
	}


	if( fHaveCalledBefore )
		return;

	if( guiCurrentLaptopMode != LAPTOP_MODE_NONE )
		return;

	fHaveCalledBefore = TRUE;

	for( i=0; i<ubNumberOfMercs; i++)
	{
		memset(&HireMercStruct, 0, sizeof(MERC_HIRE_STRUCT));

		if( !IsMercHireable( MercID[i] ) )
		{
			ubNumberOfMercs++;
			continue;
		}

		HireMercStruct.ubProfileID = MercID[i];

		//DEF: temp
		HireMercStruct.sSectorX = gsMercArriveSectorX;
		HireMercStruct.sSectorY = gsMercArriveSectorY;
		HireMercStruct.fUseLandingZoneForArrival = TRUE;
		HireMercStruct.ubInsertionCode	= INSERTION_CODE_ARRIVING_GAME;

		HireMercStruct.fCopyProfileItemsOver = TRUE;
		gMercProfiles[ MercID[i] ].ubMiscFlags |= PROFILE_MISC_FLAG_ALREADY_USED_ITEMS;


		if( gfKeyState[ ALT ] )
			HireMercStruct.iTotalContractLength = 14;
		else if( gfKeyState[ CTRL ] )
			HireMercStruct.iTotalContractLength = 7;
		else
			HireMercStruct.iTotalContractLength = 1;

		//specify when the merc should arrive
		HireMercStruct.uiTimeTillMercArrives = GetMercArrivalTimeOfDay( );// + MercID[i];

		//since this is only a testing function, make the merc available
		gMercProfiles[ MercID[i] ].bMercStatus = 0;

		//if we succesfully hired the merc
		HireMerc( &HireMercStruct );

		//add an entry in the finacial page for the hiring of the merc
		AddTransactionToPlayersBook(HIRED_MERC, MercID[i], GetWorldTotalMin(), -(INT32)( gMercProfiles[MercID[i]].sSalary ) );

		if( gMercProfiles[ MercID[i] ].bMedicalDeposit )
		{
				//add an entry in the finacial page for the medical deposit
			AddTransactionToPlayersBook(	MEDICAL_DEPOSIT, MercID[i], GetWorldTotalMin(), -(gMercProfiles[MercID[i]].sMedicalDepositAmount) );
		}

		//add an entry in the history page for the hiring of the merc
		AddHistoryToPlayersLog(HISTORY_HIRED_MERC_FROM_AIM, MercID[i], GetWorldTotalMin(), -1, -1 );
	}
}

#endif


void DelayMercSpeech( UINT8 ubMercID, UINT16 usQuoteNum, UINT16 usDelay, BOOLEAN fNewQuote, BOOLEAN fReset )
{
	static UINT32		uiLastTime=0;
	UINT32					uiCurTime;
	static UINT16		usCurQuoteNum;
	static UINT16		usCurDelay;
	static BOOLEAN	fQuoteWaiting = FALSE;		//a quote is waiting to be said
	static UINT8		ubCurMercID;
	static BOOLEAN	fHangUpAfter=FALSE;

	uiCurTime = GetJA2Clock();

	if( fReset )
		fQuoteWaiting = FALSE;

	if( fNewQuote )
	{
		//set up the counters
		uiLastTime = uiCurTime;

		ubCurMercID = ubMercID;
		usCurQuoteNum = usQuoteNum;
		usCurDelay = usDelay;

		if( gfHangUpMerc )
		{
			gfHangUpMerc = FALSE;
			fHangUpAfter = TRUE;
		}


		fQuoteWaiting = TRUE;
	}


	if( fQuoteWaiting )
	{
		if( ( uiCurTime - uiLastTime ) > usCurDelay )
		{
			InitVideoFaceTalking( ubCurMercID, usCurQuoteNum );
			fQuoteWaiting = FALSE;

			if( fHangUpAfter )
			{
				gfHangUpMerc = TRUE;
				fHangUpAfter = FALSE;
			}
		}
	}
}





#ifdef JA2TESTVERSION

//TEMP!!!
BOOLEAN QuickHireMerc()
{
	INT8	bReturnCode;
	MERC_HIRE_STRUCT HireMercStruct;
//	UINT8		ubCurrentSoldier = AimMercArray[gbCurrentIndex];
	UINT8	ubCurrentSoldier = 	AimMercArray[gbCurrentIndex];

	giContractAmount = 0;

//	if( !IsMercHireable( ubCurrentSoldier ) )
//		return( FALSE );
	if( FindSoldierByProfileID( ubCurrentSoldier, TRUE ) != NULL )
		return( FALSE );

	HireMercStruct.ubProfileID = ubCurrentSoldier;

	//DEF: temp
	HireMercStruct.sSectorX = gsMercArriveSectorX;
	HireMercStruct.sSectorY = gsMercArriveSectorY;
	HireMercStruct.bSectorZ = 0;
	HireMercStruct.fUseLandingZoneForArrival = TRUE;
	HireMercStruct.ubInsertionCode	= INSERTION_CODE_ARRIVING_GAME;

	HireMercStruct.fCopyProfileItemsOver = TRUE;
	gMercProfiles[ ubCurrentSoldier ].ubMiscFlags |= PROFILE_MISC_FLAG_ALREADY_USED_ITEMS;



	if( gfKeyState[ ALT ] )
		HireMercStruct.iTotalContractLength = 14;
	else if( gfKeyState[ CTRL ] )
		HireMercStruct.iTotalContractLength = 7;
	else
		HireMercStruct.iTotalContractLength = 1;


	//specify when the merc should arrive
	HireMercStruct.uiTimeTillMercArrives = GetMercArrivalTimeOfDay( );// + ubCurrentSoldier;

	SetFlagToForceHireMerc( TRUE );
	bReturnCode = HireMerc( &HireMercStruct );
	SetFlagToForceHireMerc( FALSE );
	if( bReturnCode == MERC_HIRE_OVER_PLAYER_LIMIT )
	{
		//display a warning saying u cant hire more than 18 mercs
		DoLapTopMessageBox( MSG_BOX_LAPTOP_DEFAULT, AimPopUpText[ AIM_MEMBER_ALREADY_HAVE_MAX_MERCS ], LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);
		return(FALSE);
	}
	else if( bReturnCode == MERC_HIRE_FAILED )
	{
		return(FALSE);
	}

	//add an entry in the finacial page for the hiring of the merc
	giContractAmount = gMercProfiles[gbCurrentSoldier].sSalary;

	AddTransactionToPlayersBook(HIRED_MERC, ubCurrentSoldier, GetWorldTotalMin(), -( giContractAmount ) );//- gMercProfiles[gbCurrentSoldier].sMedicalDepositAmount

	if( gMercProfiles[ gbCurrentSoldier ].bMedicalDeposit )
	{
		//add an entry in the finacial page for the medical deposit
		AddTransactionToPlayersBook(	MEDICAL_DEPOSIT, ubCurrentSoldier, GetWorldTotalMin(), -(gMercProfiles[gbCurrentSoldier].sMedicalDepositAmount) );
	}

	//add an entry in the history page for the hiring of the merc
	AddHistoryToPlayersLog(HISTORY_HIRED_MERC_FROM_AIM, ubCurrentSoldier, GetWorldTotalMin(), -1, -1 );

	gfRedrawScreen = TRUE;

	return( TRUE );
}

#endif

void HandleAimMemberKeyBoardInput()
{
	InputAtom					InputEvent;

	while (DequeueSpecificEvent(&InputEvent, KEY_DOWN|KEY_UP|KEY_REPEAT))
	{//!HandleTextInput( &InputEvent ) &&
		if( InputEvent.usEvent == KEY_DOWN )
		{
			switch (InputEvent.usParam)
			{
				case LEFTARROW:
				case 'a':
					// previous button
					InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);

					if( gbCurrentIndex > 0)
					{
						if (_KeyDown( 17 ) ) // CTRL
							gbCurrentIndex = 0;
						else
							gbCurrentIndex--;
					}
					else
						gbCurrentIndex = MAX_NUMBER_MERCS - 1;

					//gbCurrentSoldier = AimMercArray[gbCurrentIndex];
					gbCurrentSoldier = AimMercArray[gbCurrentIndex]; 			
					gbCurrentSoldierBio = gAimAvailability[AimMercArray[gbCurrentIndex]].AimBio;
					gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;

					gfRedrawScreen = TRUE;
				break;
				case RIGHTARROW:
				case 'd':
					// next button
					InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);

					if( gbCurrentIndex < MAX_NUMBER_MERCS - 1 )
					{
						if (_KeyDown( 17 ) ) // CTRL
							gbCurrentIndex = MAX_NUMBER_MERCS - 1;
						else
							gbCurrentIndex++;
					}
					else
						gbCurrentIndex = 0;

					//gbCurrentSoldier = AimMercArray[gbCurrentIndex];
					gbCurrentSoldier = AimMercArray[gbCurrentIndex]; 			
					gbCurrentSoldierBio = gAimAvailability[AimMercArray[gbCurrentIndex]].AimBio;
					gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;

					gfRedrawScreen = TRUE;
				break;
				case SHIFT_LEFTARROW:
				case 'A':
					// previous button
					InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);

					if( gbCurrentIndex > 0)
					{
						if (_KeyDown( 17 ) ) // CTRL
							gbCurrentIndex = 0;
						else
							gbCurrentIndex = __max(gbCurrentIndex - 10, 0);
					}
					else
						gbCurrentIndex = MAX_NUMBER_MERCS - 1;

					//gbCurrentSoldier = AimMercArray[gbCurrentIndex];
					gbCurrentSoldier = AimMercArray[gbCurrentIndex]; 			
					gbCurrentSoldierBio = gAimAvailability[AimMercArray[gbCurrentIndex]].AimBio;
					gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;

					gfRedrawScreen = TRUE;
				break;
				case SHIFT_RIGHTARROW:
				case 'D':
					// next button
					InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);

					if( gbCurrentIndex < MAX_NUMBER_MERCS - 1 )
					{
						if (_KeyDown( 17 ) ) // CTRL
							gbCurrentIndex = MAX_NUMBER_MERCS - 1;
						else
							gbCurrentIndex = __min(MAX_NUMBER_MERCS - 1, gbCurrentIndex + 10);
					}
					else
						gbCurrentIndex = 0;

					//gbCurrentSoldier = AimMercArray[gbCurrentIndex];
					gbCurrentSoldier = AimMercArray[gbCurrentIndex]; 			
					gbCurrentSoldierBio = gAimAvailability[AimMercArray[gbCurrentIndex]].AimBio;
					gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;

					gfRedrawScreen = TRUE;
				break;
				case ENTER:
				case 'e':
					// contact only if merc alive & no popup box
					if( !IsMercDead( gbCurrentSoldier ) && gubPopUpBoxAction != AIM_POPUP_DISPLAY )
					{
						if( !gubVideoConferencingMode)
						{
							gubVideoConferencingMode = AIM_VIDEO_POPUP_MODE;
							//gubVideoConferencingMode = AIM_VIDEO_INIT_MODE;
							gfFirstTimeInContactScreen = TRUE;
						}
						InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);
					}
				break;
				case BACKSPACE:
				case 'q':
					if( gubPopUpBoxAction != AIM_POPUP_DISPLAY )
					{
						if( gubVideoConferencingMode != AIM_VIDEO_NOT_DISPLAYED_MODE )
						{
							// close video conference
							gubVideoConferencingMode = AIM_VIDEO_POPDOWN_MODE;
							//gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;
						}
						else
						{
							// back to AIM Facial Index Screen
							guiCurrentLaptopMode = LAPTOP_MODE_AIM_MEMBERS_FACIAL_INDEX;
						}
					}
				break;
#ifdef JA2TESTVERSION
				case SPACE:
					QuickHireMerc();
					break;

				case '~':
				{
					// to test going on other assignments, unhired merc improvements & deaths
					const CampaignClockSession::Snapshot& clock =
						CaptureJa2CampaignClock();
					if (clock.day == 1)
						OverrideJa2CampaignClockCalendar(
							clock.day + 1, clock.hour, clock.minute);
					MercDailyUpdate();
					gfRedrawScreen = TRUE;
					break;
				}
#endif
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
					if((gGameExternalOptions.gfUseNewStartingGearInterface) && (UsingNewInventorySystem() == true))
					{
						gbCurrentSoldier = AimMercArray[gbCurrentIndex];

						if ( !(gMercProfiles[gbCurrentSoldier].ubMiscFlags & PROFILE_MISC_FLAG_ALREADY_USED_ITEMS) || gGameExternalOptions.fGearKitsAlwaysAvailable )
						{
							for ( UINT8 i = INV_START_POS; i<NUM_INV_SLOTS; ++i )
							{
								if(gMercProfileGear[gbCurrentSoldier][0].inv[i] != NONE)
								{
									gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;

									//tais: handle selected kit
									switch ( InputEvent.usParam )
									{
									case '1':	WeaponKitSelectionUpdate( 0 );	break;
									case '2':	WeaponKitSelectionUpdate( 1 );	break;
									case '3':	WeaponKitSelectionUpdate( 2 );	break;
									case '4':	WeaponKitSelectionUpdate( 3 );	break;
									case '5':	WeaponKitSelectionUpdate( 4 );	break;
									}

									gfRedrawScreen = TRUE;
									break;
								}
							}
						}
					}
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
					// previous button
					InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);

					if( gbCurrentIndex > 0)
						gbCurrentIndex--;
					else
						gbCurrentIndex = MAX_NUMBER_MERCS - 1;

					//gbCurrentSoldier = AimMercArray[gbCurrentIndex];
					gbCurrentSoldier = AimMercArray[gbCurrentIndex]; 			
					gbCurrentSoldierBio = gAimAvailability[AimMercArray[gbCurrentIndex]].AimBio;
					gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;

					gfRedrawScreen = TRUE;
				break;
				case RIGHTARROW:
				case 'd':
					// next button
					InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);

					if( gbCurrentIndex < MAX_NUMBER_MERCS -1 )
						gbCurrentIndex++;
					else
						gbCurrentIndex = 0;

					//gbCurrentSoldier = AimMercArray[gbCurrentIndex];
					gbCurrentSoldier = AimMercArray[gbCurrentIndex]; 			
					gbCurrentSoldierBio = gAimAvailability[AimMercArray[gbCurrentIndex]].AimBio;
					gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;

					gfRedrawScreen = TRUE;
				break;
				case SHIFT_LEFTARROW:
				case 'A':
					// previous button
					InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);

					if( gbCurrentIndex > 0)
						gbCurrentIndex = __max(gbCurrentIndex - 10, 0);
					else
						gbCurrentIndex = MAX_NUMBER_MERCS - 1;

					//gbCurrentSoldier = AimMercArray[gbCurrentIndex];
					gbCurrentSoldier = AimMercArray[gbCurrentIndex]; 			
					gbCurrentSoldierBio = gAimAvailability[AimMercArray[gbCurrentIndex]].AimBio;
					gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;

					gfRedrawScreen = TRUE;
				break;
				case SHIFT_RIGHTARROW:
				case 'D':
					// next button
					InitCreateDeleteAimPopUpBox(AIM_POPUP_DELETE, NULL, NULL, 0);

					if( gbCurrentIndex < MAX_NUMBER_MERCS - 1 )
						gbCurrentIndex = __min(MAX_NUMBER_MERCS - 1, gbCurrentIndex + 10);
					else
						gbCurrentIndex = 0;

					//gbCurrentSoldier = AimMercArray[gbCurrentIndex];
					gbCurrentSoldier = AimMercArray[gbCurrentIndex]; 			
					gbCurrentSoldierBio = gAimAvailability[AimMercArray[gbCurrentIndex]].AimBio;
					gubVideoConferencingMode = AIM_VIDEO_NOT_DISPLAYED_MODE;

					gfRedrawScreen = TRUE;
				break;
			}
		}
	}
}


void HandleAimMemberMouseInput(void)
{
	gSelectedFaceRegion.WheelState = gSelectedFaceRegion.WheelState * (gGameSettings.fOptions[TOPTION_INVERT_WHEEL] ? -1 : 1);

	if (gSelectedFaceRegion.uiFlags & MSYS_MOUSE_IN_AREA)
	{
		if (gSelectedFaceRegion.WheelState != 0)
		{
			if (gSelectedFaceRegion.WheelState > 0)
			{
				PreviousAimMember();
			}
			else
			{
				NextAimMember();
			}
			ResetWheelState(&gSelectedFaceRegion);
		}
	}
}

void WaitForMercToFinishTalkingOrUserToClick()
{
	//if the region is not active
	if( !gfIsShutUpMouseRegionActive )
	{
		//Enables it so if a player clicks, he will shutup the merc
		MSYS_EnableRegion(&gSelectedShutUpMercRegion);
		gfIsShutUpMouseRegionActive = TRUE;
	}

	if( gfIsAnsweringMachineActive )
		gubVideoConferencingMode = AIM_VIDEO_MERC_ANSWERING_MACHINE_MODE;
	else
		gubVideoConferencingMode = AIM_VIDEO_FIRST_CONTACT_MERC_MODE;

	gfWaitingForMercToStopTalkingOrUserToClick = TRUE;
	gfHangUpMerc = TRUE;
	gfStopMercFromTalking = FALSE;
}




#if defined ( JA2TESTVERSION )

void DemoHiringOfMercs( )
{
	INT16	i;
	UINT8 MercID[5];
	MercID[0] = 7;
	MercID[1] = 10;
	MercID[2] = 4;
	if( g_lang == i18n::Lang::de ) {
		MercID[3] = 14;
		MercID[4] = 50;
	} else {
		MercID[3] = 42;
		MercID[4] = 33;
	}
	MERC_HIRE_STRUCT HireMercStruct;
	static BOOLEAN	fHaveCalledBefore=FALSE;

	if( fHaveCalledBefore )
		return;

	fHaveCalledBefore = TRUE;

	if( guiCurrentLaptopMode != LAPTOP_MODE_NONE )
		return;

	for( i=0; i<5; i++)
	{
		memset(&HireMercStruct, 0, sizeof(MERC_HIRE_STRUCT));

		HireMercStruct.ubProfileID = MercID[i];

		//DEF: temp
		HireMercStruct.sSectorX = 1;
		HireMercStruct.sSectorY = 16;
		HireMercStruct.ubInsertionCode	= INSERTION_CODE_ARRIVING_GAME;

		HireMercStruct.fCopyProfileItemsOver = TRUE;
		gMercProfiles[ MercID[i] ].ubMiscFlags |= PROFILE_MISC_FLAG_ALREADY_USED_ITEMS;


		HireMercStruct.iTotalContractLength = 60;

		//specify when the merc should arrive
		HireMercStruct.uiTimeTillMercArrives = GetMercArrivalTimeOfDay( );// + MercID[i];

		//since this is only a testing function, make the merc available
		gMercProfiles[ MercID[i] ].bMercStatus = 0;

		//if we succesfully hired the merc
		HireMerc( &HireMercStruct );

		//add an entry in the finacial page for the hiring of the merc
		AddTransactionToPlayersBook(HIRED_MERC, MercID[i], GetWorldTotalMin(), -(INT32)( gMercProfiles[MercID[i]].sSalary ) );

		if( gMercProfiles[ MercID[i] ].bMedicalDeposit )
		{
				//add an entry in the finacial page for the medical deposit
			AddTransactionToPlayersBook(	MEDICAL_DEPOSIT, MercID[i], GetWorldTotalMin(), -(gMercProfiles[MercID[i]].sMedicalDepositAmount) );
		}

		//add an entry in the history page for the hiring of the merc
		AddHistoryToPlayersLog(HISTORY_HIRED_MERC_FROM_AIM, MercID[i], GetWorldTotalMin(), -1, -1 );
	}
}

#endif



void DisplayPopUpBoxExplainingMercArrivalLocationAndTime( )
{
	CHAR16	szLocAndTime[512];
	TacticalActor *pSoldier = NULL;
	CHAR16		zTimeString[128];
	CHAR16		zSectorIDString[512];
	UINT32		uiHour;

	//if the id of the merc is invalid, dont display the pop up
	if( LaptopSaveInfo.sLastHiredMerc.iIdOfMerc == -1 )
		return;

	//if the pop up has already been displayed, dont display it again for this occurence of laptop
	if( LaptopSaveInfo.sLastHiredMerc.fHaveDisplayedPopUpInLaptop )
		return;

	if(is_client)
		return;
	pSoldier = FindSoldierByProfileID( (UINT8)LaptopSaveInfo.sLastHiredMerc.iIdOfMerc, TRUE );

	if( pSoldier == NULL )
		return;

	//calc the approximate hour the mercs will arrive at
	uiHour = ( ( LaptopSaveInfo.sLastHiredMerc.uiArrivalTime ) - ( ( ( LaptopSaveInfo.sLastHiredMerc.uiArrivalTime ) / 1440 ) * 1440 ) ) / 60;

	//create the time string
	swprintf( zTimeString, L"%02d:%02d", uiHour, 0 );

	//get the id string
	GetSectorIDString( gsMercArriveSectorX, gsMercArriveSectorY, 0, zSectorIDString, FALSE );

	//create the string to display to the user, looks like....
	//	L"%s should arrive at the designated drop-off point ( sector %d:%d %s ) on day %d, at approximately %s.",		//first %s is mercs name, next is the sector location and name where they will be arriving in, lastely is the day an the time of arrival

if( g_lang == i18n::Lang::de ) {
	//Germans version has a different argument order
	swprintf( szLocAndTime, pMessageStrings[ MSG_JUST_HIRED_MERC_ARRIVAL_LOCATION_POPUP ],
							gMercProfiles[ pSoldier->identity().profile() ].zNickname,
							LaptopSaveInfo.sLastHiredMerc.uiArrivalTime / 1440,
							zTimeString,
							zSectorIDString );
} else {
	swprintf( szLocAndTime, pMessageStrings[ MSG_JUST_HIRED_MERC_ARRIVAL_LOCATION_POPUP ],
							gMercProfiles[ pSoldier->identity().profile() ].zNickname,
							zSectorIDString,
							LaptopSaveInfo.sLastHiredMerc.uiArrivalTime / 1440,
							zTimeString );
}



	//display the message box
	DoLapTopMessageBox( MSG_BOX_LAPTOP_DEFAULT, szLocAndTime, LAPTOP_SCREEN, MSG_BOX_FLAG_OK, DisplayPopUpBoxExplainingMercArrivalLocationAndTimeCallBack );

	//reset the id of the last merc
	LaptopSaveInfo.sLastHiredMerc.iIdOfMerc = -1;

	//set the fact that the pop up has been displayed this time in laptop
	LaptopSaveInfo.sLastHiredMerc.fHaveDisplayedPopUpInLaptop = TRUE;

}

void DisplayPopUpBoxExplainingMercArrivalLocationAndTimeCallBack( UINT8 bExitValue )
{
	//unset the flag so the msgbox WONT dislay its save buffer
	gfDontOverRideSaveBuffer = FALSE;

	if( guiCurrentLaptopMode == LAPTOP_MODE_AIM_MEMBERS )
	{
		//render the screen
		gfRedrawScreen = TRUE;
		RenderAIMMembers();
	}
}

void DisplayAimMemberClickOnFaceHelpText(const Layout& layout)
{
	CHAR16 sString[ 6000 ], sTemp[ 64 ];	// was [128]/[20]: many/long skill-trait names overflowed both

	DrawTextToScreen(AimMemberText[0], layout.help.leftClick.origin.x,
		layout.help.leftClick.origin.y, layout.help.leftClick.width,
		FONT12ARIAL, AIM_FONT_MCOLOR_WHITE, FONT_MCOLOR_BLACK,
		FALSE, CENTER_JUSTIFIED);
	DrawTextToScreen(AimMemberText[1], layout.help.leftClick.origin.x,
		layout.help.leftClick.origin.y + layout.help.descriptionOffsetY,
		layout.help.leftClick.width, FONT10ARIAL, AIM_FONT_MCOLOR_WHITE,
		FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	//display the 'right click' onscreen help msg
	DrawTextToScreen(AimMemberText[2], layout.help.rightClick.origin.x,
		layout.help.rightClick.origin.y, layout.help.rightClick.width,
		FONT12ARIAL, AIM_FONT_MCOLOR_WHITE, FONT_MCOLOR_BLACK,
		FALSE, CENTER_JUSTIFIED);
	DrawTextToScreen(AimMemberText[3], layout.help.rightClick.origin.x,
		layout.help.rightClick.origin.y + layout.help.descriptionOffsetY,
		layout.help.rightClick.width, FONT10ARIAL, AIM_FONT_MCOLOR_WHITE,
		FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);

	// Buggler: skills/traits tooltip on merc portrait

	// clear string value
	swprintf( sString, L"");

	if (gGameOptions.fNewTraitSystem) // SANDRO - old/new traits check
	{
		UINT8 ubTempSkillArray[30];
		INT8 bNumSkillTraits = 0;

		// lets rearrange our skills to a temp array
		// we also get the number of lines (skills) to be displayed 
		for ( UINT8 ubCnt = 1; ubCnt < NUM_SKILLTRAITS_NT; ubCnt++ )
		{
			if ( ProfileHasSkillTrait( gbCurrentSoldier, ubCnt ) == 2 )
			{
				ubTempSkillArray[bNumSkillTraits] = (ubCnt + NEWTRAIT_MERCSKILL_EXPERTOFFSET);
				bNumSkillTraits++;
			}
			else if ( ProfileHasSkillTrait( gbCurrentSoldier, ubCnt ) == 1 )
			{
				ubTempSkillArray[bNumSkillTraits] = ubCnt;
				bNumSkillTraits++;
			}
		}

		if ( bNumSkillTraits == 0 )
		{
			swprintf( sString, L"%s", pPersonnelScreenStrings[ PRSNL_TXT_NOSKILLS ] );
		}
		else
		{
			for ( UINT8 ubCnt = 0; ubCnt < bNumSkillTraits; ubCnt++ )
			{
				swprintf( sTemp, L"%s\n", gzMercSkillTextNew[ ubTempSkillArray[ubCnt] ] );
				wcscat( sString, sTemp );
			}
		}
	}
	else
	{
		INT8 bSkill1 = 0, bSkill2 = 0; 	
		bSkill1 = gMercProfiles[ gbCurrentSoldier ].bSkillTraits[0];
		bSkill2 = gMercProfiles[ gbCurrentSoldier ].bSkillTraits[1];

		if ( bSkill1 == 0 && bSkill2 == 0 )
		{
			swprintf( sString, L"%s", pPersonnelScreenStrings[ PRSNL_TXT_NOSKILLS ] );
		}
		else
		{
			//if the 2 skills are the same, add the '(expert)' at the end
			if( bSkill1 == bSkill2 )
			{
				swprintf( sString, L"%s %s", gzMercSkillText[bSkill1], gzMercSkillText[EXPERT] );
			}
			else
			{
				//Display the first skill
				if( bSkill1 != 0 )
				{
					swprintf( sString, L"%s\n", gzMercSkillText[bSkill1] );
				}
				if( bSkill2 != 0 )
				{
					swprintf( sTemp, L"%s", gzMercSkillText[bSkill2] );
					wcscat( sString, sTemp );
				}
			}
		}
	}
	SetRegionFastHelpText( &gSelectedFaceRegion, sString );
}

BOOLEAN CreateWeaponBoxMouseRegions(
	LaptopPageResourceOwner& resources, const Layout& layout)
{
	//tais: Weaponbox Mouseregions
	for(std::size_t item = 0; item < layout.inventory.capacity(); ++item)
	{
		const auto cell = layout.inventory.cell(item);
		MSYS_DefineRegion(&gWeaponboxFasthelpRegion[item],
			cell.x, cell.y, cell.right(), cell.bottom(),
			MSYS_PRIORITY_HIGHEST, MSYS_NO_CURSOR,
			MSYS_NO_CALLBACK, MSYS_NO_CALLBACK);
		CHECKF(resources.addRegion(gWeaponboxFasthelpRegion[item]));
		MSYS_DisableRegion(&gWeaponboxFasthelpRegion[item]);
	}
	return TRUE;
}
BOOLEAN CreateKitSelectionButtons(
	LaptopPageResourceOwner& resources, const Layout& layout)
{
	//tais: nsgi, create kit selection buttons one to five
	for ( int i = 0; i < NUM_MERCSTARTINGGEAR_KITS; ++i )
	{
		const auto buttonRect = layout.kitButtons.button(i);
		const INT32 button = CreateIconAndTextButton( guiPreviousContactNextButtonImage, CharacterInfo[AIM_MEMBER_GEAR_KIT_ONE + i], AIM_M_KIT_BUTTON_FONT,
																 AIM_M_KIT_BUTTON_UP_COLOR, DEFAULT_SHADOW,
																 AIM_M_KIT_BUTTON_DOWN_COLOR, DEFAULT_SHADOW,
																 TEXT_CJUSTIFIED,
																 buttonRect.x, buttonRect.y,
																 BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
																	 DEFAULT_MOVE_CALLBACK, BtnWeaponboxSelectButtonCallback );
		CHECKF(resources.addButton(button, giWeaponboxSelectionButton[i]));

		SetButtonCursor( giWeaponboxSelectionButton[i], CURSOR_WWW );
	}

	return TRUE;
}

void CreateWeaponBoxBackground(const Layout& layout)
{
	HVOBJECT	hWeaponBoxHandle;

	GetVideoObject(&hWeaponBoxHandle, guiWeaponBox);
	for(std::size_t item = 0; item < layout.inventory.capacity(); ++item)
	{
		const auto cell = layout.inventory.cell(item);
		BltVideoObject(FRAME_BUFFER, hWeaponBoxHandle, 0,
			cell.x, cell.y, VO_BLT_SRCTRANSPARENCY, NULL);
	}
}

void RefreshWeaponKitSelectionButtons()
{
	//tais: shorthand function to refresh buttons
	DisableWeaponKitSelectionButtons();

	for ( int i = 0; i < NUM_MERCSTARTINGGEAR_KITS; ++i )
	{
		STR16 kitlabel = CharacterInfo[AIM_MEMBER_GEAR_KIT_ONE + i];

		if ( gbCurrentSoldier != NO_PROFILE )
		{
			if ( gMercProfileGear[gbCurrentSoldier][i].mGearKitName[0] != '\0' )
				kitlabel = gMercProfileGear[gbCurrentSoldier][i].mGearKitName;
		}

		SpecifyButtonText( giWeaponboxSelectionButton[i], kitlabel );
	}
	
	EnableWeaponKitSelectionButtons();
}

void DisableWeaponKitSelectionButtons()
{
	//tais: hide all kit selection buttons
	for ( int i = 0; i < NUM_MERCSTARTINGGEAR_KITS; ++i )
	{
		HideButton( giWeaponboxSelectionButton[i] );
	}
}

void EnableWeaponKitSelectionButtons()
{
	//tais: weaponbox gear selection buttons
	if ( UsingNewInventorySystem() )
	{
		if ( !(gMercProfiles[gbCurrentSoldier].ubMiscFlags & PROFILE_MISC_FLAG_ALREADY_USED_ITEMS) || gGameExternalOptions.fGearKitsAlwaysAvailable )
		{
			for(int i=0; i<NUM_MERCSTARTINGGEAR_KITS; ++i)
			{
				for(int j=INV_START_POS; j<NUM_INV_SLOTS; ++j)
				{
					if(gMercProfileGear[gbCurrentSoldier][i].inv[j] != NONE)
					{
						ShowButton( giWeaponboxSelectionButton[i] );
						break;
					}
				}
			}
		}
	}
}

void WeaponKitSelectionUpdate(UINT8 selectedInventory = 0)
{	
	UINT32 uiLoop;
	INT16 usItem;
	if(UsingNewInventorySystem() == true){
		// Start by resetting all profile inventory values to 0
		gMercProfiles[gbCurrentSoldier].clearInventory();
		gMercProfiles[gbCurrentSoldier].ubInvUndroppable = 0;
		// Next, go through and assign everything but lbe gear
		for(uiLoop=INV_START_POS; uiLoop<NUM_INV_SLOTS; uiLoop++)
		{
			if(gMercProfileGear[gbCurrentSoldier][selectedInventory].inv[uiLoop] != NONE)
			{
				gMercProfiles[gbCurrentSoldier].inv[uiLoop] = gMercProfileGear[gbCurrentSoldier][selectedInventory].inv[uiLoop];
				gMercProfiles[gbCurrentSoldier].bInvStatus[uiLoop] = gMercProfileGear[gbCurrentSoldier][selectedInventory].iStatus[uiLoop];
				if(uiLoop > 5)
					gMercProfiles[gbCurrentSoldier].bInvNumber[uiLoop] = gMercProfileGear[gbCurrentSoldier][selectedInventory].iNumber[uiLoop];
				else
					gMercProfiles[gbCurrentSoldier].bInvNumber[uiLoop] = 1;
			}
		}
		// Last, go through and assign LBE items.  Only needed for new inventory system
		if((UsingNewInventorySystem() == true))
		{
			for(uiLoop=0; uiLoop<5; uiLoop++)
			{
				UINT32 uiLoop2 = uiLoop + VESTPOCKPOS;
				if(gMercProfileGear[gbCurrentSoldier][selectedInventory].lbe[uiLoop] != NONE){
					gMercProfiles[gbCurrentSoldier].inv[uiLoop2] = gMercProfileGear[gbCurrentSoldier][selectedInventory].lbe[uiLoop];
					gMercProfiles[gbCurrentSoldier].bInvStatus[uiLoop2] = gMercProfileGear[gbCurrentSoldier][selectedInventory].lStatus[uiLoop];
					gMercProfiles[gbCurrentSoldier].bInvNumber[uiLoop2] = 1;
				}
			}
		}
	}
	gMercProfiles[gbCurrentSoldier].bMainGunAttractiveness		= -1;
	gMercProfiles[gbCurrentSoldier].bArmourAttractiveness			= -1;

	UINT32 invsize = gMercProfiles[ gbCurrentSoldier ].inv.size();
	for ( uiLoop = 0; uiLoop < invsize; ++uiLoop )
	{
		usItem = gMercProfiles[gbCurrentSoldier].inv[ uiLoop ];

		if ( usItem != NOTHING )
		{
			// Check if it's a gun
			if ( Item[ usItem ].usItemClass & IC_GUN )
			{
				gMercProfiles[gbCurrentSoldier].bMainGunAttractiveness = Weapon[ usItem ].ubDeadliness;
			}

			// If it's armour
			if ( Item[ usItem ].usItemClass & IC_ARMOUR )
			{
				gMercProfiles[gbCurrentSoldier].bArmourAttractiveness = std::min<int>(128, Armour[ Item[ usItem ].ubClassIndex ].ubProtection);
			}
		}
	}

	gMercProfiles[ gbCurrentSoldier ].usOptionalGearCost = 0;
	//tais: new tag in gearkit that sets an absolute price for gearkit that will override item value and price modifier if it's a sensible value between 0 and 32000
	if(gMercProfileGear[gbCurrentSoldier][selectedInventory].AbsolutePrice >= 0 && gMercProfileGear[gbCurrentSoldier][selectedInventory].AbsolutePrice <= 32000)
	{
		gMercProfiles[ gbCurrentSoldier ].usOptionalGearCost = gMercProfileGear[gbCurrentSoldier][selectedInventory].AbsolutePrice;
	}
	else
	{
		UINT16 tempGearCost = 0;
		UINT32 invsize = gMercProfiles[ gbCurrentSoldier ].inv.size();
		for ( uiLoop = 0; uiLoop< invsize; ++uiLoop )
		{
			if ( gMercProfiles[ gbCurrentSoldier ].inv[ uiLoop ] != NOTHING )
			{
				//get the item
				usItem = gMercProfiles[ gbCurrentSoldier ].inv[ uiLoop ];

				// for an item stack, we obviously need to account fot the number of items
				// for single items, the number is not always set, so just to be sure...
				int number = 1;
				if ( gMercProfileGear[gbCurrentSoldier][selectedInventory].iNumber.size( ) >= uiLoop )
					number = max( 1, gMercProfileGear[gbCurrentSoldier][selectedInventory].iNumber[uiLoop] );

				//add the cost
				tempGearCost += number * Item[usItem].usPrice;
			}
		}
		//tais: added optional price modifier for gearkits, reads the xml tag mPriceMod from MercStartingGear.xml
		if(gMercProfileGear[gbCurrentSoldier][selectedInventory].PriceModifier != 0 &&
			gMercProfileGear[gbCurrentSoldier][selectedInventory].PriceModifier <= 200 &&
			gMercProfileGear[gbCurrentSoldier][selectedInventory].PriceModifier >= -100)
		{
			FLOAT mod;
			mod = (FLOAT) (gMercProfileGear[gbCurrentSoldier][selectedInventory].PriceModifier + 100) / 100;
			gMercProfiles[ gbCurrentSoldier ].usOptionalGearCost = (UINT16)(tempGearCost * mod);
		}
		else
		{
			gMercProfiles[ gbCurrentSoldier ].usOptionalGearCost = tempGearCost;
		}
	}
}
