	#include "laptop.h"
	#include "AimSort.h"
	#include "aim.h"
	#include "WCheck.h"
	#include "Utilities.h"
	#include "WordWrap.h"
	#include "Soldier Profile.h"
	#include "stdlib.h"
	#include "Text.h"
	#include "Multi Language Graphic Utils.h"
	#include "english.h"
	#include "sysutil.h"
	#include "LaptopPageResourceOwner.h"
	#include "AimWebsiteLayout.h"

#include <array>

//#define

#define		AIM_SORT_FONT_TITLE								FONT14ARIAL
#define		AIM_SORT_FONT_SORT_TEXT							FONT10ARIAL

#define		AIM_SORT_COLOR_SORT_TEXT						AIM_FONT_MCOLOR_WHITE
#define		AIM_SORT_SORT_BY_COLOR							146
#define		AIM_SORT_LINK_TEXT_COLOR						146

#define		AIM_SORT_ON									0
#define		AIM_SORT_OFF								1

UINT8			gubCurrentSortMode;
UINT8			gubOldSortMode;
UINT8			gubCurrentListMode;
UINT8			gubOldListMode;

namespace
{
LaptopPageResourceOwner gAimSortResources;

std::array<MOUSE_REGION, AimWebsiteLayoutModel::kSortNavigationCount>
	gAimSortNavigationRegions;
std::array<MOUSE_REGION, AimWebsiteLayoutModel::kSortCriterionCount>
	gAimSortCriterionRegions;
std::array<MOUSE_REGION, 2> gAimSortOrderRegions;

constexpr std::array<int, AimWebsiteLayoutModel::kSortCriterionCount>
	kCriterionText = {{
		PRICE, EXPERIENCE, AIMMARKSMANSHIP,
		AIMMECHANICAL, AIMEXPLOSIVES, AIMMEDICAL,
		AIMHEALTH, AIMAGILITY, AIMDEXTERITY,
		AIMSTRENGTH, AIMLEADERSHIP, AIMWISDOM, NAME}};

constexpr std::array<int, AimWebsiteLayoutModel::kSortNavigationCount>
	kNavigationText = {{MUGSHOT_INDEX, MERCENARY_FILES, ALUMNI_GALLERY}};

constexpr std::array<UINT32, AimWebsiteLayoutModel::kSortNavigationCount>
	kNavigationModes = {{
		LAPTOP_MODE_AIM_MEMBERS_FACIAL_INDEX,
		LAPTOP_MODE_AIM_MEMBERS,
		LAPTOP_MODE_AIM_MEMBERS_ARCHIVES}};

AimWebsiteLayoutModel::SortLayout CurrentAimSortLayout()
{
	return AimWebsiteLayoutModel::MakeSortLayout(
		{iScreenWidthOffset, iScreenHeightOffset,
		 IMAGE_OFFSET_X, IMAGE_OFFSET_Y, LAPTOP_SCREEN_WEB_DELTA_Y});
}

void SelectAimSortNavigationRegionCallback(
	MOUSE_REGION* region, INT32 reason);
void SelectAimSortCriterionRegionCallback(
	MOUSE_REGION* region, INT32 reason);
void SelectAimSortOrderRegionCallback(
	MOUSE_REGION* region, INT32 reason);
}



void DrawSelectLight(UINT8 ubMode, UINT8 ubImage);
INT32 QsortCompare( const void *pNum1, const void *pNum2);
INT32 CompareValue(const INT32 Num1, const INT32 Num2);
INT32 CompareName(const STR16 name1, const STR16 name2);
BOOLEAN SortMercArray(void);


UINT32		guiSortByBox;
UINT32		guiToAlumni;
UINT32		guiToMugShots;
UINT32		guiToStats;
UINT32		guiSelectLight;


//Hotkey Assignment
void HandleAimSortKeyBoardInput();


void GameInitAimSort()
{
	// initial sort is by name, ascending (A-Z)
	gubCurrentSortMode=12;
	gubOldSortMode=12;
	gubCurrentListMode = AIM_DESCEND;
	gubOldListMode = AIM_DESCEND;
}

BOOLEAN EnterAimSort()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner stagedResources;
	const auto layout = CurrentAimSortLayout();

	gAimSortResources.clear();

	// load the SortBy box graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\SortBy.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiSortByBox));

	// load the ToAlumni graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	GetMLGFilename( VObjectDesc.ImageFile, MLG_TOALUMNI );
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiToAlumni));

	// load the ToMugShots graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	GetMLGFilename( VObjectDesc.ImageFile, MLG_TOMUGSHOTS );
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiToMugShots));

	// load the ToStats graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	GetMLGFilename( VObjectDesc.ImageFile, MLG_TOSTATS );
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiToStats));

	// load the SelectLight graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\SelectLight.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiSelectLight));


	for (std::size_t index = 0;
		index < AimWebsiteLayoutModel::kSortNavigationCount; ++index)
	{
		const auto bounds = layout.navigationArtwork.at(index);
		MSYS_DefineRegion(&gAimSortNavigationRegions[index],
			bounds.x, bounds.y, bounds.right(), bounds.bottom(),
			MSYS_PRIORITY_HIGH, CURSOR_WWW, MSYS_NO_CALLBACK,
			SelectAimSortNavigationRegionCallback);
		CHECKF(stagedResources.addRegion(gAimSortNavigationRegions[index]));
		MSYS_SetRegionUserData(&gAimSortNavigationRegions[index], 0, index);
	}

	for (std::size_t criterion = 0;
		criterion < AimWebsiteLayoutModel::kSortCriterionCount; ++criterion)
	{
		const int textWidth = StringPixLength(
			AimSortText[kCriterionText[criterion]], AIM_SORT_FONT_SORT_TEXT);
		const auto bounds = layout.criterionHitbox(criterion, textWidth);
		MSYS_DefineRegion(&gAimSortCriterionRegions[criterion],
			bounds.x, bounds.y, bounds.right(), bounds.bottom(),
			MSYS_PRIORITY_HIGH, MSYS_NO_CURSOR, MSYS_NO_CALLBACK,
			SelectAimSortCriterionRegionCallback);
		CHECKF(stagedResources.addRegion(gAimSortCriterionRegions[criterion]));
		MSYS_SetRegionUserData(&gAimSortCriterionRegions[criterion], 0,
			criterion);
	}

	for (std::size_t index = 0; index < gAimSortOrderRegions.size(); ++index)
	{
		const std::size_t mode = AIM_ASCEND + index;
		const int textIndex = index == 0 ? ASCENDING : DESCENDING;
		const int textWidth = StringPixLength(
			AimSortText[textIndex], AIM_SORT_FONT_SORT_TEXT);
		const auto bounds = layout.orderHitbox(mode, textWidth);
		MSYS_DefineRegion(&gAimSortOrderRegions[index],
			bounds.x, bounds.y, bounds.right(), bounds.bottom(),
			MSYS_PRIORITY_HIGH, MSYS_NO_CURSOR, MSYS_NO_CALLBACK,
			SelectAimSortOrderRegionCallback);
		CHECKF(stagedResources.addRegion(gAimSortOrderRegions[index]));
		MSYS_SetRegionUserData(&gAimSortOrderRegions[index], 0, mode);
	}

	CHECKF(InitAimDefaults());
	if (!InitAimMenuBar())
	{
		RemoveAimDefaults();
		return FALSE;
	}

	gAimSortResources = std::move(stagedResources);
	RenderAimSort();

	return( TRUE );
}

void ExitAimSort()
{
	// Sort the merc array
	SortMercArray();
	gAimSortResources.clear();
	RemoveAimDefaults();
	ExitAimMenuBar();

}

void HandleAimSort()
{
	HandleAimSortKeyBoardInput();
}

void RenderAimSort()
{
	HVOBJECT	hSortByHandle;
	const auto layout = CurrentAimSortLayout();
	const std::array<UINT32, AimWebsiteLayoutModel::kSortNavigationCount>
		navigationGraphics = {{guiToMugShots, guiToStats, guiToAlumni}};

	DrawAimDefaults();
	GetVideoObject(&hSortByHandle, guiSortByBox);
	BltVideoObject(FRAME_BUFFER, hSortByHandle, 0,
		layout.sortPanel.x, layout.sortPanel.y,
		VO_BLT_SRCTRANSPARENCY, NULL);
	for (std::size_t index = 0; index < navigationGraphics.size(); ++index)
	{
		HVOBJECT graphic;
		const auto bounds = layout.navigationArtwork.at(index);
		GetVideoObject(&graphic, navigationGraphics[index]);
		BltVideoObject(FRAME_BUFFER, graphic, 0, bounds.x, bounds.y,
			VO_BLT_SRCTRANSPARENCY, NULL);
	}

	DisplayAimSlogan();
	DrawTextToScreen(AimSortText[AIM_AIMMEMBERS],
		layout.memberTitle.origin.x, layout.memberTitle.origin.y,
		layout.memberTitle.width, AIM_MAINTITLE_FONT, AIM_MAINTITLE_COLOR,
		FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	DrawTextToScreen(AimSortText[SORT_BY],
		layout.sortTitle.x, layout.sortTitle.y, 0, AIM_SORT_FONT_TITLE,
		AIM_SORT_SORT_BY_COLOR, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	for (std::size_t criterion = 0; criterion < kCriterionText.size(); ++criterion)
	{
		const auto position = layout.criterionText(criterion);
		DrawTextToScreen(AimSortText[kCriterionText[criterion]],
			position.x, position.y, 0, AIM_SORT_FONT_SORT_TEXT,
			AIM_SORT_COLOR_SORT_TEXT, FONT_MCOLOR_BLACK, FALSE,
			LEFT_JUSTIFIED);
	}
	for (std::size_t index = 0; index < gAimSortOrderRegions.size(); ++index)
	{
		const std::size_t mode = AIM_ASCEND + index;
		const auto text = layout.orderText(mode);
		DrawTextToScreen(AimSortText[index == 0 ? ASCENDING : DESCENDING],
			text.origin.x, text.origin.y, text.width, AIM_SORT_FONT_SORT_TEXT,
			AIM_SORT_COLOR_SORT_TEXT, FONT_MCOLOR_BLACK, FALSE,
			RIGHT_JUSTIFIED);
	}
	for (std::size_t index = 0; index < kNavigationText.size(); ++index)
	{
		const auto position = layout.navigationText(index);
		DrawTextToScreen(AimSortText[kNavigationText[index]],
			position.x, position.y, 0, AIM_SORT_FONT_SORT_TEXT,
			AIM_SORT_LINK_TEXT_COLOR, FONT_MCOLOR_BLACK, FALSE,
			LEFT_JUSTIFIED);
	}

	for (std::size_t mode = 0;
		mode < AimWebsiteLayoutModel::kSortControlCount; ++mode)
		DrawSelectLight(static_cast<UINT8>(mode), AIM_SORT_OFF);

	DrawSelectLight(gubCurrentSortMode, AIM_SORT_ON);
	DrawSelectLight(gubCurrentListMode, AIM_SORT_ON);

	DisableAimButton();

	MarkButtonsDirty( );

	RenderWWWProgramTitleBar( );

	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}




namespace
{
void SelectAimSortNavigationRegionCallback(
	MOUSE_REGION* region, INT32 reason)
{
	if (!(reason & MSYS_CALLBACK_REASON_LBUTTON_UP))
		return;
	const std::size_t index = MSYS_GetRegionUserData(region, 0);
	if (index < kNavigationModes.size())
		guiCurrentLaptopMode = kNavigationModes[index];
}



void SelectAimSortCriterionRegionCallback(
	MOUSE_REGION* region, INT32 reason)
{
	if (!(reason & MSYS_CALLBACK_REASON_LBUTTON_UP))
		return;
	const std::size_t mode = MSYS_GetRegionUserData(region, 0);
	if (mode >= AimWebsiteLayoutModel::kSortCriterionCount ||
		gubCurrentSortMode == mode)
		return;
	gubCurrentSortMode = static_cast<UINT8>(mode);
	DrawSelectLight(gubCurrentSortMode, AIM_SORT_ON);
	DrawSelectLight(gubOldSortMode, AIM_SORT_OFF);
	gubOldSortMode = gubCurrentSortMode;
}



void SelectAimSortOrderRegionCallback(
	MOUSE_REGION* region, INT32 reason)
{
	if (!(reason & MSYS_CALLBACK_REASON_LBUTTON_UP))
		return;
	const std::size_t mode = MSYS_GetRegionUserData(region, 0);
	if ((mode != AIM_ASCEND && mode != AIM_DESCEND) ||
		gubCurrentListMode == mode)
		return;
	gubCurrentListMode = static_cast<UINT8>(mode);
	DrawSelectLight(gubCurrentListMode, AIM_SORT_ON);
	DrawSelectLight(gubOldListMode, AIM_SORT_OFF);
	gubOldListMode = gubCurrentListMode;
}
}

void DrawSelectLight(UINT8 ubMode, UINT8 ubImage)
{
	HVOBJECT	hSelectLightHandle;
	const auto layout = CurrentAimSortLayout();
	if (!layout.hasControl(ubMode))
		return;
	const auto bounds = layout.control(ubMode);

	GetVideoObject(&hSelectLightHandle, guiSelectLight);
	BltVideoObject(FRAME_BUFFER, hSelectLightHandle, ubImage,
		bounds.x, bounds.y, VO_BLT_SRCTRANSPARENCY, NULL);
	InvalidateRegion(bounds.x, bounds.y, bounds.right(), bounds.bottom());
}





BOOLEAN SortMercArray(void)
{
	qsort( (LPVOID)AimMercArray, (size_t) MAX_NUMBER_MERCS, sizeof(UINT8), QsortCompare);

	return(TRUE);
}




INT32 QsortCompare( const void *pNum1, const void *pNum2)
{
	UINT8 Num1 = *(UINT8*)pNum1;
	UINT8 Num2 = *(UINT8*)pNum2;

	switch( gubCurrentSortMode )
	{
		//Price						INT16	uiWeeklySalary
		case 0:
			return( CompareValue((INT32)gMercProfiles[ Num1 ].uiWeeklySalary,	(INT32)gMercProfiles[Num2].uiWeeklySalary ) );
			break;
		//Experience			INT16	bExpLevel
		case 1:
			return( CompareValue((INT32)gMercProfiles[ Num1 ].bExpLevel,	(INT32)gMercProfiles[Num2].bExpLevel) );
			break;
		//Marksmanship		INT16	bMarksmanship
		case 2:
			return( CompareValue((INT32)gMercProfiles[ Num1 ].bMarksmanship,	(INT32)gMercProfiles[Num2].bMarksmanship ) );
			break;
		//Mechanical			INT16	bMechanical
		case 3:
			return( CompareValue((INT32)gMercProfiles[ Num1 ].bMechanical,	(INT32)gMercProfiles[Num2].bMechanical ) );
			break;
		//Explosives			INT16	bExplosive
		case 4:
			return( CompareValue((INT32)gMercProfiles[ Num1 ].bExplosive,	(INT32)gMercProfiles[Num2].bExplosive ) );
			break;
		//Medical					INT16	bMedical
		case 5:
			return( CompareValue((INT32)gMercProfiles[ Num1 ].bMedical,	(INT32)gMercProfiles[Num2].bMedical ) );
			break;
		//Health					INT16	bLifeMax
		case 6:
			return( CompareValue((INT32)gMercProfiles[ Num1 ].bLifeMax,	(INT32)gMercProfiles[Num2].bLifeMax ) );
			break;
		//Agility					INT16	bAgility
		case 7:
			return( CompareValue((INT32)gMercProfiles[ Num1 ].bAgility,	(INT32)gMercProfiles[Num2].bAgility ) );
			break;
		//Dexterity					INT16	bDexterity
		case 8:
			return( CompareValue((INT32)gMercProfiles[ Num1 ].bDexterity,	(INT32)gMercProfiles[Num2].bDexterity ) );
			break;
		//Strength					INT16	bStrength
		case 9:
			return( CompareValue((INT32)gMercProfiles[ Num1 ].bStrength,	(INT32)gMercProfiles[Num2].bStrength ) );
			break;
		//Leadership				INT16	bLeadership
		case 10:
			return( CompareValue((INT32)gMercProfiles[ Num1 ].bLeadership,	(INT32)gMercProfiles[Num2].bLeadership ) );
			break;
		//Wisdom					INT16	bWisdom
		case 11:
			return( CompareValue((INT32)gMercProfiles[ Num1 ].bWisdom,	(INT32)gMercProfiles[Num2].bWisdom ) );
			break;
		//Name						CHAR16	zNickname
		case 12:
			return (CompareName(gMercProfiles[Num1].zNickname, gMercProfiles[Num2].zNickname));
			break;

		default:
			Assert( 0 );
			return( 0 );
			break;
	}
}




INT32 CompareValue(const INT32 Num1, const INT32 Num2)
{
	// Ascending
	if( gubCurrentListMode == AIM_ASCEND)
	{
		if( Num1 < Num2)
			return(-1);
		else if( Num1 == Num2)
			return(0);
		else
			return(1);
	}

	// Descending
	else if( gubCurrentListMode == AIM_DESCEND )
	{
		if( Num1 > Num2)
			return(-1);
		else if( Num1 == Num2)
			return(0);
		else
			return(1);
	}

	return( 0 );
}

INT32 CompareName(const STR16 name1, const STR16 name2)
{
	INT16 result = wcscmp(name1, name2);
	if (result == 0) return 0;

	if (gubCurrentListMode == AIM_ASCEND)
	{
		return result < 0 ? -1 : 1;
	}
	else if (gubCurrentListMode == AIM_DESCEND)
	{
		return result > 0 ? -1 : 1;
	}

	return 0;
}

void HandleAimSortKeyBoardInput()
{
	InputAtom					InputEvent;

	while (DequeueSpecificEvent(&InputEvent, KEY_DOWN|KEY_UP|KEY_REPEAT))
	{//!HandleTextInput( &InputEvent ) &&
		if( InputEvent.usEvent == KEY_DOWN )
		{
			switch (InputEvent.usParam)
			{
				case BACKSPACE:
				case 'q':
					guiCurrentLaptopMode = LAPTOP_MODE_AIM;
					break;
				case 'a':
					guiCurrentLaptopMode = LAPTOP_MODE_AIM_MEMBERS_ARCHIVES;
					break;
				case 'f':
					guiCurrentLaptopMode = LAPTOP_MODE_AIM_MEMBERS;
					break;
				case ENTER:
				case 'e':
				case 'm':
					guiCurrentLaptopMode = LAPTOP_MODE_AIM_MEMBERS_FACIAL_INDEX;
					break;
				default:
					HandleKeyBoardShortCutsForLapTop( InputEvent.usEvent, InputEvent.usParam, InputEvent.usKeyState );
					break;
			}
		}
	}
}
