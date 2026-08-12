	#include "laptop.h"
	#include "personnel.h"
	#include "LaptopPageResourceOwner.h"
	#include "PersonnelRosterModel.h"
	#include "Utilities.h"
	#include "WCheck.h"
	#include "DEBUG.H"
	#include "WordWrap.h"
	#include "Encrypted File.h"
	#include "Cursors.h"
	#include "Overhead.h"
	#include "Soldier Profile.h"
	#include "TacticalActor.h"
	#include "TacticalActorStateFlags.h"
	#include "TacticalActorEmploymentTypes.h"
	#include "Soldier Profile Constants.h"
	#include "Text.h"
	#include "TextCatalog.h"
	#include "mapscreen.h"
	#include "Game Clock.h"
	#include "finances.h"
	#include "LaptopSave.h"
	#include "LaptopSafety.h"
	#include "Map Screen Interface Map.h"
	#include "input.h"
	#include "english.h"
	#include "random.h"
	#include "line.h"
	#include "Assignments.h"
	#include "gameloop.h"
	#include "Soldier Add.h"
	#include "SoldierRepository.h"
	#include "Interface Items.h"
	#include "Weapons.h"
	#include "strategicmap.h"
	#include "GameSettings.h"
	#include "Merc Contract.h"
	#include "_Ja25EnglishText.h" // added by SANDRO

#include "Soldier macros.h"
#include "InterfaceItemImages.h"

#include "IMP Skill Trait.h"		// added by Flugente
#include "Map Screen Interface.h"	// added by Flugente
#include "Interface.h"				// added by Flugente
#include "IMP Background.h"			// added by Flugente for AssignBackgroundHelpText()
#include "NPC.h"					// added by Flugente for GetEffectiveApproachValue(...)
#include "Drugs And Alcohol.h"		// added by Flugente for DoesMercHaveDisability(...)
#include "CampaignMercenaryPolicy.h"
#include "GameContext.h"

#include <array>
#include <limits>
#include <utility>
#include <vector>


// WDS - make number of mercenaries, etc. be configurable
#define MAX_MERCS_ON_SCREEN 20

#define NUM_BACKGROUND_REPS					40
#define BACKGROUND_HEIGHT						10
#define BACKGROUND_WIDTH						125
#define IMAGE_BOX_X										iScreenWidthOffset + 395
#define IMAGE_BOX_Y									LAPTOP_SCREEN_UL_Y+24
#define IMAGE_BOX_WIDTH							112
#define	IMAGE_BOX_WITH_NO_BORDERS		106
#define IMAGE_BOX_COUNT							4
#define	IMAGE_NAME_WIDTH						106
#define	IMAGE_FULL_NAME_OFFSET_Y		111
#define TEXT_BOX_WIDTH							160
#define	TEXT_DELTA_OFFSET						9
#define TEXT_BOX_Y LAPTOP_SCREEN_UL_Y+188
#define PAGE_BOX_X LAPTOP_SCREEN_UL_X+250 - 10
#define PAGE_BOX_Y LAPTOP_SCREEN_UL_Y+3
#define PAGE_BOX_WIDTH	58
#define PAGE_BOX_HEIGHT 24
#define MAX_SLOTS 4
#define PERS_CURR_TEAM_X LAPTOP_SCREEN_UL_X + 39 - 15
#define PERS_CURR_TEAM_Y LAPTOP_SCREEN_UL_Y + 218
#define PERS_DEPART_TEAM_Y LAPTOP_SCREEN_UL_Y + 247

#define BUTTON_Y LAPTOP_SCREEN_UL_Y+34
#define LEFT_BUTTON_X	LAPTOP_SCREEN_UL_X+3 - 10
#define RIGHT_BUTTON_X LAPTOP_SCREEN_UL_X+476 - 10
#define PERS_COUNT 15
#define MAX_STATS 20
#define PERS_FONT						FONT10ARIAL
#define PERS_HEADER_FONT		FONT14ARIAL
#define CHAR_NAME_FONT			FONT12ARIAL
#define CHAR_NAME_Y										iScreenHeightOffset + 177
#define CHAR_LOC_Y										iScreenHeightOffset + 189
#define PERS_TEXT_FONT_COLOR							FONT_WHITE
#define PERS_TEXT_FONT_ALTERNATE_COLOR FONT_YELLOW
#define PERS_FONT_COLOR FONT_WHITE
#define PAGE_X PAGE_BOX_X+2 - 10
#define PAGE_Y PAGE_BOX_Y+2

#define	FACES_DIR "FACES\\BIGFACES\\"
#define	SMALL_FACES_DIR "FACES\\"

#define	IMP_FACES_DIR "IMPFACES\\BIGFACES\\"
#define	IMP_SMALL_FACES_DIR "IMPFACES\\"

#define NEXT_MERC_FACE_X	LAPTOP_SCREEN_UL_X + 448
#define MERC_FACE_SCROLL_Y LAPTOP_SCREEN_UL_Y + 150
#define PREV_MERC_FACE_X	LAPTOP_SCREEN_UL_X + 285

#define DEPARTED_X LAPTOP_SCREEN_UL_X + 29 - 10
#define DEPARTED_Y LAPTOP_SCREEN_UL_Y + 207

#define PERSONNEL_PORTRAIT_NUMBER 20
#define PERSONNEL_PORTRAIT_NUMBER_WIDTH 5

#define SMALL_PORTRAIT_WIDTH 46
#define SMALL_PORTRAIT_HEIGHT 42

#define SMALL_PORT_WIDTH 52
#define SMALL_PORT_HEIGHT 45

#define	SMALL_PORTRAIT_WIDTH_NO_BORDERS		48

#define SMALL_PORTRAIT_START_X							iScreenWidthOffset + 141 - 10
#define SMALL_PORTRAIT_START_Y							iScreenHeightOffset + 53

#define PERS_CURR_TEAM_COST_X LAPTOP_SCREEN_UL_X + 150 - 10
#define PERS_CURR_TEAM_COST_Y LAPTOP_SCREEN_UL_Y + 218

#define PERS_CURR_TEAM_HIGHEST_Y PERS_CURR_TEAM_COST_Y + 15
#define PERS_CURR_TEAM_LOWEST_Y PERS_CURR_TEAM_HIGHEST_Y + 15

#define PERS_CURR_TEAM_WIDTH 286 - 160

#define PERS_DEPART_TEAM_WIDTH PERS_CURR_TEAM_WIDTH - 20

#define PERS_STAT_AVG_X LAPTOP_SCREEN_UL_X + 157 - 10
#define PERS_STAT_AVG_Y LAPTOP_SCREEN_UL_Y + 274
#define PERS_STAT_AVG_WIDTH 202 - 159
#define PERS_STAT_LOWEST_X LAPTOP_SCREEN_UL_X + 72 - 10
#define PERS_STAT_LOWEST_WIDTH 155 - 75
#define PERS_STAT_HIGHEST_X LAPTOP_SCREEN_UL_X + 205 - 10
#define PERS_STAT_LIST_X LAPTOP_SCREEN_UL_X + 33 - 10

#define PERS_TOGGLE_CUR_DEPART_WIDTH 106 - 35
#define PERS_TOGGLE_CUR_DEPART_HEIGHT 236 - 212

#define PERS_TOGGLE_CUR_DEPART_X LAPTOP_SCREEN_UL_X + 35 - 10
#define PERS_TOGGLE_CUR_Y LAPTOP_SCREEN_UL_Y + 208
#define PERS_TOGGLE_DEPART_Y LAPTOP_SCREEN_UL_Y + 238

#define PERS_DEPARTED_UP_X	LAPTOP_SCREEN_UL_X + 265 - 10
#define PERS_DEPARTED_UP_Y	LAPTOP_SCREEN_UL_Y + 210
#define PERS_DEPARTED_DOWN_Y LAPTOP_SCREEN_UL_Y + 237

#define PERS_TITLE_X									iScreenWidthOffset + 140
#define PERS_TITLE_Y									iScreenHeightOffset + 33

#define ATM_UL_X LAPTOP_SCREEN_UL_X + 397
#define ATM_UL_Y LAPTOP_SCREEN_UL_Y + 27

/// atm font
#define ATM_FONT PERS_FONT

#define ATM_DISPLAY_X									iScreenWidthOffset + 509
#define ATM_DISPLAY_Y									iScreenHeightOffset + 58
#define ATM_DISPLAY_HEIGHT 10
#define ATM_DISPLAY_WIDTH	81


// the number of inventory items per personnel page
#define NUMBER_OF_INVENTORY_PERSONNEL 8
#define Y_SIZE_OF_PERSONNEL_SCROLL_REGION ( 422 - 219 )
#define X_SIZE_OF_PERSONNEL_SCROLL_REGION ( 589 - 573 )
#define Y_OF_PERSONNEL_SCROLL_REGION					(iScreenHeightOffset + 219)
#define X_OF_PERSONNEL_SCROLL_REGION					(iScreenWidthOffset + 573)
#define SIZE_OF_PERSONNEL_CURSOR 19

// enums for the buttons in the information side bar ( used with giPersonnelATMStartButton[] )
enum
{
	PERSONNEL_STAT_BTN,
	PERSONNEL_PERSONALITY_BTN,
	PERSONNEL_EMPLOYMENT_BTN,	
	PERSONNEL_INV_BTN,

	PERSONNEL_NUM_BTN,
};

UINT8	gubPersonnelInfoState = PERSONNEL_STAT_BTN;

extern BOOLEAN gfTemporaryDisablingOfLoadPendingFlag;
extern BOOLEAN fExitingLaptopFlag;
extern void HandleLapTopESCKey( void );
extern void HandleAltTabKeyInLaptop( void );
extern void HandleShiftAltTabKeyInLaptop( void );

UINT8 uiCurrentInventoryIndex = 0;

UINT32 guiSliderPosition;

#define PrsnlOffSetX	(-15) //-20
#define Prsnl_DATA_OffSetX	(36)
#define PrsnlOffSetY	10

POINT pPersonnelScreenPoints[]=
{
	{422+PrsnlOffSetX, 205+PrsnlOffSetY},
	{422+PrsnlOffSetX, 215+PrsnlOffSetY},
	{422+PrsnlOffSetX, 225+PrsnlOffSetY},
	{422+PrsnlOffSetX, 235+PrsnlOffSetY},
	{422+PrsnlOffSetX, 245+PrsnlOffSetY},
	{422+PrsnlOffSetX, 255+PrsnlOffSetY},
	{422+PrsnlOffSetX, 315+PrsnlOffSetY},
	{422+PrsnlOffSetX, 270+PrsnlOffSetY},
	{422+PrsnlOffSetX, 280+PrsnlOffSetY},
	{422+PrsnlOffSetX, 290+PrsnlOffSetY},
	{422+PrsnlOffSetX, 300+PrsnlOffSetY},			//10
	{422+PrsnlOffSetX, 395+PrsnlOffSetY},
	{422+PrsnlOffSetX, 385+PrsnlOffSetY},
	{422+PrsnlOffSetX, 415+PrsnlOffSetY},
	{422+PrsnlOffSetX, 425+PrsnlOffSetY},
	{422+PrsnlOffSetX, 445+PrsnlOffSetY},
	{422+PrsnlOffSetX, 380+PrsnlOffSetY}, // for contract price
	{422+PrsnlOffSetX, 435+PrsnlOffSetY},
	{140,33},	// Personnel Header
	{422+PrsnlOffSetX, 318+PrsnlOffSetY},
	{422+PrsnlOffSetX, 340+PrsnlOffSetY},	//20
	{422+PrsnlOffSetX, 355+PrsnlOffSetY},
	{422+PrsnlOffSetX, 365+PrsnlOffSetY},
	{422+PrsnlOffSetX, 375+PrsnlOffSetY},
	{422+PrsnlOffSetX, 385+PrsnlOffSetY},
	{422+PrsnlOffSetX, 395+PrsnlOffSetY},
};




UINT32 guiSCREEN;
extern UINT32 guiTITLE; // symbol already defined in laptop.cpp (jonathanl)
UINT32 guiDEPARTEDTEAM;
UINT32 guiCURRENTTEAM;
UINT32 guiPersonnelInventory;
UINT32 guiQMark;

INT32 giPersonnelButton[6];
INT32 giPersonnelButtonImage[6];
INT32 giPersonnelInventoryButtons[ 2 ];
INT32 giPersonnelInventoryButtonsImages[ 2 ];
INT32 giDepartedButtonImage[ 2 ];
INT32 giDepartedButton[ 2 ];

// buttons for ATM
INT32 giPersonnelATMStartButton[PERSONNEL_NUM_BTN];
INT32 giPersonnelATMStartButtonImage[PERSONNEL_NUM_BTN];
INT32 giPersonnelATMButton;
INT32 giPersonnelATMButtonImage;

// which mode are we showing?..current team?...or deadly departed?
BOOLEAN fCurrentTeamMode = TRUE;

static bool showPersonnelButtons{ true };

// waitr one frame
BOOLEAN fOneFrameDelayInPersonnel = FALSE;

// WDS - make number of mercenaries, etc. be configurable
// mouse regions
MOUSE_REGION gPortraitMouseRegions[ MAX_MERCS_ON_SCREEN ];

MOUSE_REGION gTogglePastCurrentTeam[ 2 ];

MOUSE_REGION gMouseScrollPersonnelINV;

// Popup help-text regions are owned individually because the active set is
// rebuilt as the selected roster entry and information panel change.
MOUSE_REGION gSkillTraitHelpTextRegion[13];


namespace
{
LaptopPageResourceOwner gPersonnelPageResources;
LaptopPageResourceOwner gPersonnelDepartedResources;
LaptopPageResourceOwner gPersonnelInventoryResources;
LaptopPageResourceOwner gPersonnelAtmResources;
std::array<LaptopPageResourceOwner, 13> gPersonnelTraitResources;

std::vector<SoldierID> currentTeamList;
std::vector<PersonnelRosterModel::DepartedEntry> gDepartedRoster;
PersonnelRosterModel::RosterCursor gCurrentRosterCursor(MAX_MERCS_ON_SCREEN);
PersonnelRosterModel::RosterCursor gDepartedRosterCursor(MAX_MERCS_ON_SCREEN);

void RefreshDepartedRoster()
{
	gDepartedRoster = PersonnelRosterModel::BuildDepartedRoster(
		LaptopSaveInfo.ubDeadCharactersList,
		LaptopSaveInfo.ubLeftCharactersList,
		LaptopSaveInfo.ubOtherCharactersList,
		static_cast<INT16>(-1), NUM_PROFILES);
	gDepartedRosterCursor.normalize(gDepartedRoster.size());
}

const PersonnelRosterModel::DepartedEntry* SelectedDepartedEntry()
{
	if (!gDepartedRosterCursor.hasSelection() ||
		gDepartedRosterCursor.selected() >= gDepartedRoster.size()) return nullptr;
	return &gDepartedRoster[gDepartedRosterCursor.selected()];
}

const MERCPROFILESTRUCT* ProfileFor(const TacticalActor* soldier)
{
	if (!soldier || !PersonnelRosterModel::IsValidProfileId(
		soldier->identity().profile(), NUM_PROFILES)) return nullptr;
	return &gMercProfiles[soldier->identity().profile()];
}

INT32 DailyCostFor(const TacticalActor* soldier)
{
	const MERCPROFILESTRUCT* profile = ProfileFor(soldier);
	if (!profile) return 0;
	if (soldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC)
	{
		if (soldier->employment().lastContractType() == CONTRACT_EXTEND_2_WEEK)
			return static_cast<INT32>(profile->uiBiWeeklySalary / 14);
		if (soldier->employment().lastContractType() == CONTRACT_EXTEND_1_WEEK)
			return static_cast<INT32>(profile->uiWeeklySalary / 7);
		return std::max<INT32>(profile->sSalary, 0);
	}
	if (soldier->employment().mercenaryType() == MERC_TYPE__MERC)
		return std::max<INT32>(profile->sSalary, 0);
	return 0;
}

void ClearPersonnelTraitRegion(std::size_t index)
{
	if (index >= gPersonnelTraitResources.size()) return;
	gPersonnelTraitResources[index].clear();
}

bool PublishPersonnelTraitRegion(std::size_t index)
{
	if (index >= gPersonnelTraitResources.size()) return false;
	if (!gPersonnelTraitResources[index].addRegion(
		gSkillTraitHelpTextRegion[index])) return false;
	return true;
}

template<std::size_t Capacity>
void AppendPersonnelText(CHAR16 (&destination)[Capacity],
	const CHAR16* source)
{
	PersonnelRosterModel::AppendText(destination, source);
}
}


BOOLEAN LoadPersonnelGraphics(LaptopPageResourceOwner& owner);
void RenderPersonnel( void );
void RenderPersonnelStats(INT32 iId, INT32 iSlot);
void RenderPersonnelFace(SoldierID iId, INT32 iSlot, BOOLEAN fDead, BOOLEAN fFired, BOOLEAN fOther );
void LeftButtonCallBack(GUI_BUTTON *btn,INT32 reason);
void RightButtonCallBack(GUI_BUTTON *btn,INT32 reason);
void PersonnelPortraitCallback( MOUSE_REGION * pRegion, INT32 iReason );
BOOLEAN CreatePersonnelButtons(LaptopPageResourceOwner& owner);
void DisplayHeader( void );
void DisplayCharName( SoldierID iId, INT32 iSlot );
void DisplayCharStats(SoldierID iId, INT32 iSlot);
void DisplayCharPersonality( SoldierID iId, INT32 iSlot );
void SetPersonnelButtonStates( void );
BOOLEAN LoadPersonnelScreenBackgroundGraphics(LaptopPageResourceOwner& owner);
void RenderPersonnelScreenBackground( void );
INT32 GetNumberOfMercsOnPlayersTeam( void );
BOOLEAN CreatePersonnelPortraitMouseRegions(LaptopPageResourceOwner& owner);
void DisplayPicturesOfCurrentTeam( void );
void DisplayFaceOfDisplayedMerc( );
void DisplayNumberOnCurrentTeam( void );
void DisplayNumberDeparted( void );
INT32 GetTotalDailyCostOfCurrentTeam( void );
void DisplayCostOfCurrentTeam( void );
INT32 GetLowestDailyCostOfCurrentTeam( void );
INT32 GetHighestDailyCostOfCurrentTeam( void );
INT32 GetAvgStatOfCurrentTeamStat( INT32 iStat );
void DisplayAverageStatValuesForCurrentTeam( void );
void DisplayLowestStatValuesForCurrentTeam( void );
void DisplayHighestStatValuesForCurrentTeam( void );
void DisplayPersonnelTeamStats( void );
void InitPastCharactersList( void );
INT32 GetNumberOfPastMercsOnPlayersTeam( void );
INT32 GetNumberOfOtherOnPastTeam( void );
INT32 GetNumberOfLeftOnPastTeam( void );
INT32 GetNumberOfDeadOnPastTeam( void );
void DisplayStateOfPastTeamMembers( void );
BOOLEAN CreateCurrentDepartedMouseRegions(LaptopPageResourceOwner& owner);
void PersonnelCurrentTeamCallback( MOUSE_REGION * pRegion, INT32 iReason );
void PersonnelDepartedTeamCallback( MOUSE_REGION * pRegion, INT32 iReason );
void CreateDestroyButtonsForDepartedTeamList( void );
void DepartedDownCallBack(GUI_BUTTON *btn,INT32 reason);
void DepartedUpCallBack(GUI_BUTTON *btn,INT32 reason);
void DisplayPastMercsPortraits( void );
void DisplayPortraitOfPastMerc( INT32 iId , INT32 iCounter, BOOLEAN fDead, BOOLEAN fFired, BOOLEAN fOther );
void DisplayDepartedCharStats(INT32 iId, INT32 iSlot, INT32 iState);
void EnableDisableDeparturesButtons( void );
void DisplayDepartedCharName( INT32 iId, INT32 iSlot, INT32 iState );
void DisplayPersonnelTextOnTitleBar( void );
INT32 GetIdOfDepartedMercWithHighestStat( INT32 iStat );
INT32 GetIdOfDepartedMercWithLowestStat( INT32 iStat );
void RenderInventoryForCharacter( SoldierID iId, INT32 iSlot );
void DisplayInventoryForSelectedChar( void );
INT32 GetNumberOfInventoryItemsOnCurrentMerc( void );
void CreateDestroyPersonnelInventoryScrollButtons( void );
void EnableDisableInventoryScrollButtons( void );
void PersonnelDataButtonCallback(GUI_BUTTON *btn,INT32 reason);
void HandleSliderBarClickCallback( MOUSE_REGION *pRegion, INT32 iReason );

void RenderSliderBarForPersonnelInventory( void );
void FindPositionOfPersInvSlider( void );


// check if current guy can have atm
void UpDateStateOfStartButton( void );
void HandlePersonnelKeyboard( void );


void DisplayEmploymentinformation( SoldierID iId, INT32 iSlot );



// AIM merc:	Returns the amount of time left on mercs contract
// MERC merc: Returns the amount of time the merc has worked
// IMP merc:	Returns the amount of time the merc has worked
// else:			returns -1
INT32 CalcTimeLeftOnMercContract( TacticalActor *pSoldier );


// display box around currently selected merc
BOOLEAN DisplayHighLightBox( void );


// grab appropriate id of soldier first being displayed
INT32 GetIdOfFirstDisplayedMerc( );

// get avg for this stat
INT32 GetAvgStatOfPastTeamStat( INT32 iStat );

// render atm panel
BOOLEAN RenderAtmPanel( void );

// create destroy ATM button
void CreateDestroyStartATMButton( void );


// atm misc functions

void DisplayAmountOnCurrentMerc( void );

void AssignPersonnelCharacterTraitHelpText( UINT8 ubCharacterNumber );
void AssignPersonnelDisabilityHelpText( UINT8 ubDisabilityNumber );
void AssignPersonnelMultipleDisabilityHelpText( const TacticalActor* pSoldier );
void AssignPersonnelKillsHelpText( INT32 ubProfile );
void AssignPersonnelAssistsHelpText( INT32 ubProfile );
void AssignPersonnelHitPercentageHelpText( INT32 ubProfile );
void AssignPersonnelBattlesHelpText( INT32 ubProfile );
void AssignPersonnelAchievementsHelpText( INT32 ubProfile );
void AssignPersonnelWoundsHelpText( INT32 ubProfile );
INT8 CalculateMercsAchievementPercentage( INT32 ubProfile );

// Flugente: personality info
void AssignPersonalityHelpText( const TacticalActor* pSoldier, MOUSE_REGION* pMouseregion );

BOOLEAN fShowRecordsIfZero = TRUE;

void GameInitPersonnel( void )
{
	// init past characters lists
	InitPastCharactersList( );
}

void InitVariables(void)
{
	pPersonnelScreenPoints[0].x = iScreenWidthOffset + 422+PrsnlOffSetX;		//0
	pPersonnelScreenPoints[0].y = iScreenHeightOffset + 205+PrsnlOffSetY;

	pPersonnelScreenPoints[1].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[1].y = iScreenHeightOffset + 215+PrsnlOffSetY;

	pPersonnelScreenPoints[2].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[2].y = iScreenHeightOffset + 225+PrsnlOffSetY;

	pPersonnelScreenPoints[3].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[3].y = iScreenHeightOffset + 235+PrsnlOffSetY;

	pPersonnelScreenPoints[4].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[4].y = iScreenHeightOffset + 245+PrsnlOffSetY;

	pPersonnelScreenPoints[5].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[5].y = iScreenHeightOffset + 255+PrsnlOffSetY;

	pPersonnelScreenPoints[6].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[6].y = iScreenHeightOffset + 315+PrsnlOffSetY;

	pPersonnelScreenPoints[7].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[7].y = iScreenHeightOffset + 270+PrsnlOffSetY;

	pPersonnelScreenPoints[8].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[8].y = iScreenHeightOffset + 280+PrsnlOffSetY;

	pPersonnelScreenPoints[9].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[9].y = iScreenHeightOffset + 290+PrsnlOffSetY;

	pPersonnelScreenPoints[10].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[10].y = iScreenHeightOffset + 300+PrsnlOffSetY; // 10

	pPersonnelScreenPoints[11].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[11].y = iScreenHeightOffset + 395+PrsnlOffSetY;

	pPersonnelScreenPoints[12].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[12].y = iScreenHeightOffset + 385+PrsnlOffSetY;

	pPersonnelScreenPoints[13].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[13].y = iScreenHeightOffset + 415+PrsnlOffSetY;

	pPersonnelScreenPoints[14].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[14].y = iScreenHeightOffset + 425+PrsnlOffSetY;

	pPersonnelScreenPoints[15].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[15].y = iScreenHeightOffset + 445+PrsnlOffSetY;

	pPersonnelScreenPoints[16].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[16].y = iScreenHeightOffset + 380+PrsnlOffSetY;

	pPersonnelScreenPoints[17].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[17].y = iScreenHeightOffset + 435+PrsnlOffSetY;

	pPersonnelScreenPoints[18].x = iScreenWidthOffset + 140;		// Personal Header // 18
	pPersonnelScreenPoints[18].y = iScreenHeightOffset + 33;

	pPersonnelScreenPoints[19].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[19].y = iScreenHeightOffset + 333+PrsnlOffSetY;

	pPersonnelScreenPoints[20].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[20].y = iScreenHeightOffset + 355+PrsnlOffSetY;

	pPersonnelScreenPoints[21].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[21].y = iScreenHeightOffset + 365+PrsnlOffSetY;

	pPersonnelScreenPoints[22].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[22].y = iScreenHeightOffset + 375+PrsnlOffSetY;

	pPersonnelScreenPoints[23].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[23].y = iScreenHeightOffset + 385+PrsnlOffSetY;

	pPersonnelScreenPoints[24].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[24].y = iScreenHeightOffset + 395+PrsnlOffSetY;

	pPersonnelScreenPoints[25].x = iScreenWidthOffset + 422+PrsnlOffSetX;
	pPersonnelScreenPoints[25].y = iScreenHeightOffset + 405+PrsnlOffSetY;
}

void EnterPersonnel( void )
{
	InitVariables();

	// Snapshot the player-team range. The actor at slot zero is not guaranteed
	// to exist, and a missing actor inside the range must not be dereferenced by
	// the loop condition on the next iteration.
	currentTeamList.clear();
	currentTeamList.reserve(giMAXIMUM_NUMBER_OF_PLAYER_SLOTS);
	const SoldierID firstTeamId = gTacticalStatus.Team[ OUR_TEAM ].bFirstID;
	const SoldierID lastTeamId = gTacticalStatus.Team[ OUR_TEAM ].bLastID;
	for (INT32 rawId = firstTeamId.i; rawId <= lastTeamId.i; ++rawId)
	{
		const SoldierID idx = rawId;
		TacticalActor* pTeamSoldier =
			GetJa2SoldierRepository().resolve(rawId);

		// WANNE: Bugfix: Also show the roboter in ther personnel screen. This bug was introduced in revision 2498, when Many Mercenary was included.
		//if ((pTeamSoldier->roster().active()) &&
		//	!(pTeamSoldier->status().flags() & SOLDIER_VEHICLE)  &&
		//	/*(pTeamSoldier->vitals().health() > 0 ) && */  !AM_A_ROBOT(pTeamSoldier)  )

		if (pTeamSoldier && (pTeamSoldier->roster().active()) &&
			!(pTeamSoldier->status().flags() & SOLDIER_VEHICLE))
		{
			currentTeamList.push_back(idx);
		}
	}

	gCurrentRosterCursor.reset(currentTeamList.size());
	RefreshDepartedRoster();
	gDepartedRosterCursor.reset(gDepartedRoster.size());

	fReDrawScreenFlag=TRUE;

	uiCurrentInventoryIndex = 0;
	guiSliderPosition = 0;

	showPersonnelButtons = true;
	LaptopPageResourceOwner staged;
	if (!LoadPersonnelGraphics(staged) ||
		!LoadPersonnelScreenBackgroundGraphics(staged) ||
		!CreatePersonnelPortraitMouseRegions(staged) ||
		!CreateCurrentDepartedMouseRegions(staged) ||
		!CreatePersonnelButtons(staged))
	{
		showPersonnelButtons = false;
		return;
	}
	gPersonnelPageResources = std::move(staged);

	// render screen
	RenderPersonnel( );

	// set states of en- dis able buttons
	SetPersonnelButtonStates( );

	return;
}

void ExitPersonnel( void )
{
	showPersonnelButtons = false;
	gubPersonnelInfoState = PERSONNEL_STAT_BTN;
	gPersonnelAtmResources.clear();
	gPersonnelInventoryResources.clear();
	gPersonnelDepartedResources.clear();
	for (std::size_t i = 0; i < gPersonnelTraitResources.size(); ++i)
		ClearPersonnelTraitRegion(i);
	gPersonnelPageResources.clear();
}

void HandlePersonnel( void )
{
	// create / destroy buttons for scrolling departed list
	CreateDestroyButtonsForDepartedTeamList( );

	// enable / disable departures buttons
	EnableDisableDeparturesButtons( );

	// create destroy inv buttons as needed
	CreateDestroyPersonnelInventoryScrollButtons( );

	// enable disable buttons as needed
	EnableDisableInventoryScrollButtons( );

	HandlePersonnelKeyboard( );
}

BOOLEAN LoadPersonnelGraphics(LaptopPageResourceOwner& owner)
{
	// load graphics needed for personnel screen
	VOBJECT_DESC	VObjectDesc;

	// load graphics

	// title bar
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\programtitlebar.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiTITLE)) return FALSE;

	// the background grpahics
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\personnelwindow.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiSCREEN)) return FALSE;

	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\personnel_inventory.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiPersonnelInventory)) return FALSE;

	// load ? marks for tooltips
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\PERSONNEL_TOOLTIP_MARK.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiQMark)) return FALSE;
	return TRUE;
}

void RenderPersonnel( void )
{
	HVOBJECT hHandle;
	// re-renders personnel screen
	// render main background

	// blit title
	GetVideoObject(&hHandle, guiTITLE);
	BltVideoObject( FRAME_BUFFER, hHandle, 0,LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_UL_Y - 2, VO_BLT_SRCTRANSPARENCY,NULL );

	// blit screen
	GetVideoObject(&hHandle, guiSCREEN);
	BltVideoObject( FRAME_BUFFER, hHandle, 0,LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_UL_Y + 22, VO_BLT_SRCTRANSPARENCY,NULL );

	// render personnel screen background
	RenderPersonnelScreenBackground( );

	// show team
	DisplayPicturesOfCurrentTeam( );

	DisplayPastMercsPortraits( );

	// show selected merc
	DisplayFaceOfDisplayedMerc( );

	// show current team size
	DisplayNumberOnCurrentTeam( );

	// show departed team size
	DisplayNumberDeparted( );

	// list stats row headers for team stats list
	DisplayPersonnelTeamStats( );

	// showinventory of selected guy if applicable
	DisplayInventoryForSelectedChar( );

	// the average stats for the current team
	DisplayAverageStatValuesForCurrentTeam( );

	// lowest stat values
	DisplayLowestStatValuesForCurrentTeam( );

	// past team
	DisplayStateOfPastTeamMembers( );

	// title bar
	BlitTitleBarIcons(	);

	// show text on titlebar
	DisplayPersonnelTextOnTitleBar( );

	// the highest stats
	DisplayHighestStatValuesForCurrentTeam( );

	// render the atm panel
	RenderAtmPanel( );

	DisplayAmountOnCurrentMerc( );

	// en-dis-able start button
	UpDateStateOfStartButton( );
}

void RenderPersonnelStats( INT32 iId, INT32 iSlot )
{
	// will render the stats of person iId in slot iSlot
	SetFont(PERS_FONT);
	SetFontForeground(PERS_TEXT_FONT_COLOR);
	SetFontBackground(FONT_BLACK);

	if( gubPersonnelInfoState == PERSONNEL_STAT_BTN )
		DisplayCharStats(iId, iSlot);
	else if( gubPersonnelInfoState == PERSONNEL_EMPLOYMENT_BTN )
		DisplayEmploymentinformation( iId, iSlot );
	else if ( gubPersonnelInfoState == PERSONNEL_PERSONALITY_BTN )
		DisplayCharPersonality( iId, iSlot );
}

// ID -> fortlaufende ID, und nicht die mercID
// -> bei aktuellen Merc passt es
// -> bei departed Merc wird die MercId anstatt der fortlaufenden ID übergeben!!
void RenderPersonnelFace(SoldierID iId, INT32 iSlot, BOOLEAN fDead, BOOLEAN fFired, BOOLEAN fOther )
{
	TacticalActor* activeSoldier =
		GetJa2SoldierRepository().resolve(iSlot);
	// Get the profile id (from profileId or slotId)
	INT32 profileId = iId;
	if (profileId == NOBODY)
	{
		if ( !activeSoldier )
			return;
		profileId = activeSoldier->identity().profile();
	}

	if (!PersonnelRosterModel::IsValidProfileId(profileId, NUM_PROFILES))
		return;

	char sTemp[100];
	HVOBJECT hFaceHandle;
	VOBJECT_DESC	VObjectDesc;
	// draw face to soldier iId in slot iSlot

	// Currently active merc!
	// special case?..player generated merc
	if (fCurrentTeamMode) 
	{
		const char* directory = gMercProfiles[profileId].Type == PROFILETYPE_IMP
			? IMP_FACES_DIR : FACES_DIR;
		snprintf(sTemp, sizeof(sTemp), "%s%02d.sti", directory,
			gMercProfiles[profileId].ubFaceIndex);
		
		// TODO: Check if needed!
		if( activeSoldier &&
			activeSoldier->status().flags() & SOLDIER_VEHICLE )
		{
			return;
		}
	} 
	// departed mercs
	else 
	{
		//if this is not a valid merc
		if( !fDead && !fFired && !fOther ) 
		{
			return;
		}

		const char* directory = gMercProfiles[profileId].Type == PROFILETYPE_IMP
			? IMP_FACES_DIR : FACES_DIR;
		snprintf(sTemp, sizeof(sTemp), "%s%02d.sti", directory,
			gMercProfiles[profileId].ubFaceIndex);
	}

	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP(sTemp, VObjectDesc.ImageFile);
	UniqueVideoObjectHandle face = AddVideoObjectOwned(&VObjectDesc);
	if (!face) return;
	const UINT32 faceHandle = face.get();

	//Blt face to screen to
	GetVideoObject(&hFaceHandle, faceHandle);

	if (fCurrentTeamMode) 
	{
		if( activeSoldier && activeSoldier->vitals().health() <= 0 )
		{
			hFaceHandle->pShades[ 0 ]		= Create16BPPPaletteShaded( hFaceHandle->pPaletteEntry, DEAD_MERC_COLOR_RED, DEAD_MERC_COLOR_GREEN, DEAD_MERC_COLOR_BLUE, TRUE );
			//set the red pallete to the face
			SetObjectHandleShade(faceHandle, 0);
		}
	} 
	else 
	{
		if (fDead == TRUE)
		{
			hFaceHandle->pShades[ 0 ]		= Create16BPPPaletteShaded( hFaceHandle->pPaletteEntry, DEAD_MERC_COLOR_RED, DEAD_MERC_COLOR_GREEN, DEAD_MERC_COLOR_BLUE, TRUE );
			//set the red pallete to the face
			SetObjectHandleShade(faceHandle, 0);
		}
	}

	// TODO:Check
	BltVideoObject(FRAME_BUFFER, hFaceHandle, 0,IMAGE_BOX_X, IMAGE_BOX_Y, VO_BLT_SRCTRANSPARENCY,NULL);

	//if the merc is dead, display it
	if (!fCurrentTeamMode)
	{
		INT32 iHeightOfText = DisplayWrappedString(IMAGE_BOX_X, (UINT16)(IMAGE_BOX_Y+IMAGE_FULL_NAME_OFFSET_Y), IMAGE_NAME_WIDTH, 1, PERS_FONT, PERS_FONT_COLOR, gMercProfiles[profileId].zName, 0, FALSE, CENTER_JUSTIFIED | DONT_DISPLAY_TEXT );

		//if the string will rap
		if( ( iHeightOfText - 2 ) > GetFontHeight( PERS_FONT ) ) 
		{
			//raise where we display it, and rap it
			DisplayWrappedString(IMAGE_BOX_X, (UINT16)(IMAGE_BOX_Y+IMAGE_FULL_NAME_OFFSET_Y - GetFontHeight( PERS_FONT )), IMAGE_NAME_WIDTH, 1, PERS_FONT, PERS_FONT_COLOR, gMercProfiles[profileId].zName, 0, FALSE, CENTER_JUSTIFIED);
		} 
		else 
		{
			DrawTextToScreen( gMercProfiles[profileId].zName, IMAGE_BOX_X, (UINT16)(IMAGE_BOX_Y+IMAGE_FULL_NAME_OFFSET_Y), IMAGE_NAME_WIDTH, PERS_FONT, PERS_FONT_COLOR, 0, FALSE, CENTER_JUSTIFIED );
		}
	}

}


// WDS - make number of mercenaries, etc. be configurable
static BOOLEAN NextPersonnelFace( void )
{
	const bool changed = fCurrentTeamMode
		? gCurrentRosterCursor.next(currentTeamList.size())
		: gDepartedRosterCursor.next(gDepartedRoster.size());
	if (changed) fReDrawScreenFlag = TRUE;
	return changed ? TRUE : FALSE;
}

static BOOLEAN PrevPersonnelFace( void )
{
	const bool changed = fCurrentTeamMode
		? gCurrentRosterCursor.previous(currentTeamList.size())
		: gDepartedRosterCursor.previous(gDepartedRoster.size());
	if (changed) fReDrawScreenFlag = TRUE;
	return changed ? TRUE : FALSE;
}

static BOOLEAN NextPersonnelFacePage(void)
{
	const bool changed = fCurrentTeamMode
		? gCurrentRosterCursor.nextPage(currentTeamList.size())
		: gDepartedRosterCursor.nextPage(gDepartedRoster.size());
	if (changed) fReDrawScreenFlag = TRUE;
	return changed ? TRUE : FALSE;
}

static BOOLEAN PrevPersonnelFacePage(void)
{
	const bool changed = fCurrentTeamMode
		? gCurrentRosterCursor.previousPage(currentTeamList.size())
		: gDepartedRosterCursor.previousPage(gDepartedRoster.size());
	if (changed) fReDrawScreenFlag = TRUE;
	return changed ? TRUE : FALSE;
}


BOOLEAN CreatePersonnelButtons(LaptopPageResourceOwner& owner)
{

	// left button
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\personnelbuttons.sti", -1, 0, -1, 1, -1),
		giPersonnelButtonImage[0])) return FALSE;
	if (!owner.addButton(QuickCreateButton( giPersonnelButtonImage[0], PREV_MERC_FACE_X, MERC_FACE_SCROLL_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)LeftButtonCallBack),
		giPersonnelButton[0])) return FALSE;

	// right button
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\personnelbuttons.sti", -1, 2, -1, 3, -1),
		giPersonnelButtonImage[1])) return FALSE;
	if (!owner.addButton(QuickCreateButton( giPersonnelButtonImage[1], NEXT_MERC_FACE_X, MERC_FACE_SCROLL_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)RightButtonCallBack),
		giPersonnelButton[1])) return FALSE;
	// set up cursors
	SetButtonCursor(giPersonnelButton[0], CURSOR_LAPTOP_SCREEN);
	SetButtonCursor(giPersonnelButton[1], CURSOR_LAPTOP_SCREEN);


	return TRUE;
}


void LeftButtonCallBack(GUI_BUTTON *btn,INT32 reason)
{
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		if(!(btn->uiFlags & BUTTON_CLICKED_ON))
		{
			fReDrawScreenFlag=TRUE;
		}
	btn->uiFlags|=(BUTTON_CLICKED_ON);
	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if(btn->uiFlags & BUTTON_CLICKED_ON)
		{
		btn->uiFlags&=~(BUTTON_CLICKED_ON);
		fReDrawScreenFlag=TRUE;
		if (_KeyDown(SHIFT))
		{
			PrevPersonnelFacePage();
		}
		else
		{
			PrevPersonnelFace( );
		}
		uiCurrentInventoryIndex = 0;
		guiSliderPosition = 0;

		}
	}
}

void RightButtonCallBack(GUI_BUTTON *btn,INT32 reason)
{
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		if(!(btn->uiFlags & BUTTON_CLICKED_ON))
		{
			fReDrawScreenFlag=TRUE;
		}
		btn->uiFlags|=(BUTTON_CLICKED_ON);
	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if(btn->uiFlags & BUTTON_CLICKED_ON)
		{
			btn->uiFlags&=~(BUTTON_CLICKED_ON);
			fReDrawScreenFlag=TRUE;
			if (_KeyDown(SHIFT))
			{
				NextPersonnelFacePage();
			}
			else
			{
				NextPersonnelFace( );
			}
			uiCurrentInventoryIndex = 0;
			guiSliderPosition = 0;
		}
	}
}

void DisplayHeader( void )
{
	SetFont(PERS_HEADER_FONT);
	SetFontForeground(PERS_FONT_COLOR);
	SetFontBackground( 0 );

	mprintf(pPersonnelScreenPoints[18].x, pPersonnelScreenPoints[18].y, L"%s",
		i18n::GetCompiledTextPack().text(i18n::TextKey::PersonnelTitle).data());

	return;
}


void DisplayCharName( SoldierID Id, INT32 iSlot )
{
	// get merc's nickName, assignment, and sector location info
	INT16 sX, sY;
	TacticalActor *pSoldier;
	CHAR16 sString[ 64 ];
	CHAR16 sTownName[ 256 ];
	INT8 bTownId =	-1;
	INT32	iHeightOfText;

	sTownName[0] = L'\0';

	if ( Id == NOBODY )
	{
		return;
	}
	pSoldier = GetJa2SoldierRepository().resolve(Id.i);
	if ( !pSoldier )
		return;
	const MERCPROFILESTRUCT* profile = ProfileFor(pSoldier);
	if (!profile) return;

	SetFont(CHAR_NAME_FONT);
	SetFontForeground(PERS_TEXT_FONT_COLOR);
	SetFontBackground(FONT_BLACK);

	if( pSoldier->status().flags() & SOLDIER_VEHICLE )
	{
		return;
	}

	if( pSoldier->assignment().current() == ASSIGNMENT_POW )
	{
	}
	else if( pSoldier->assignment().current() == IN_TRANSIT )
	{
	}
	else
	{
		// name of town, if any
		bTownId = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

		if( bTownId != BLANK_SECTOR )
		{
			sgp_swprintf(sTownName, std::size(sTownName), L"%s", pTownNames[ bTownId ] );
		}
	}



	if( sTownName[0] != L'\0' )
	{
		//nick name - town name
		sgp_swprintf(sString, std::size(sString), L"%s - %s", profile->zNickname, sTownName );
	}
	else
	{
		//nick name
		sgp_swprintf(sString, std::size(sString), L"%s", profile->zNickname );
	}


	// nick name - assignment
	FindFontCenterCoordinates(IMAGE_BOX_X-5,0,IMAGE_BOX_WIDTH + 90 , 0,sString,CHAR_NAME_FONT, &sX, &sY );

	// check to see if we are going to go off the left edge
	if( sX < pPersonnelScreenPoints[ 0 ].x )
	{
		sX = ( INT16 )pPersonnelScreenPoints[ 0 ].x;
	}

	//Display the mercs name
	mprintf( sX + iSlot*IMAGE_BOX_WIDTH, CHAR_NAME_Y, L"%s", sString );

	if ( gGameExternalOptions.fUseXMLSquadNames && pSoldier->assignment().current() < std::min<size_t>(ON_DUTY, gSquadNameVector.size() ) )
		sgp_swprintf(sString, std::size(sString), L"%s", gSquadNameVector[pSoldier->assignment().current()].c_str() );
	else
		sgp_swprintf(sString, std::size(sString), L"%s", pPersonnelAssignmentStrings[pSoldier->assignment().current()]);

	// nick name - assignment
	FindFontCenterCoordinates(IMAGE_BOX_X-5,0,IMAGE_BOX_WIDTH + 90 , 0,sString,CHAR_NAME_FONT, &sX, &sY );

	// check to see if we are going to go off the left edge
	if( sX < pPersonnelScreenPoints[ 0 ].x )
	{
		sX = ( INT16 )pPersonnelScreenPoints[ 0 ].x;
	}

	mprintf(sX+iSlot*IMAGE_BOX_WIDTH, CHAR_LOC_Y, L"%s", sString );


	//
	// Display the mercs FULL name over top of their portrait
	//

	//first get height of text to be displayed
	iHeightOfText = DisplayWrappedString(IMAGE_BOX_X, (UINT16)(IMAGE_BOX_Y+IMAGE_FULL_NAME_OFFSET_Y), IMAGE_NAME_WIDTH, 1, PERS_FONT, PERS_FONT_COLOR, profile->zName, 0, FALSE, CENTER_JUSTIFIED | DONT_DISPLAY_TEXT );

	//if the string will rap
	if( ( iHeightOfText - 2 ) > GetFontHeight( PERS_FONT ) )
	{
		//raise where we display it, and rap it
		DisplayWrappedString(IMAGE_BOX_X, (UINT16)(IMAGE_BOX_Y+IMAGE_FULL_NAME_OFFSET_Y - GetFontHeight( PERS_FONT )), IMAGE_NAME_WIDTH, 1, PERS_FONT, PERS_FONT_COLOR, profile->zName, 0, FALSE, CENTER_JUSTIFIED);
	}
	else
	{
		DrawTextToScreen( profile->zName, IMAGE_BOX_X, (UINT16)(IMAGE_BOX_Y+IMAGE_FULL_NAME_OFFSET_Y), IMAGE_NAME_WIDTH, PERS_FONT, PERS_FONT_COLOR, 0, FALSE, CENTER_JUSTIFIED );
	}

/*
Moved so the name of the town will be in the same line as the name


	if( pSoldier->assignment().current() == ASSIGNMENT_POW )
	{
//		FindFontCenterCoordinates(IMAGE_BOX_X-5,0,IMAGE_BOX_WIDTH, 0,pPOWStrings[ 1 ],CHAR_NAME_FONT, &sX, &sY );
//	mprintf(sX+iSlot*IMAGE_BOX_WIDTH, CHAR_NAME_Y+20,L"%s", pPOWStrings[ 1 ] );
	}
	else if( pSoldier->assignment().current() == IN_TRANSIT )
	{
		return;
	}
	else
	{
		// name of town, if any
		bTownId = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

		if( bTownId != BLANK_SECTOR )
		{
			FindFontCenterCoordinates( IMAGE_BOX_X - 5, 0, IMAGE_BOX_WIDTH, 0, pTownNames[ bTownId ], CHAR_NAME_FONT, &sX, &sY );
			mprintf( sX + ( iSlot * IMAGE_BOX_WIDTH ), CHAR_NAME_Y + 20, L"%s", pTownNames[ bTownId ]);
		}
	}
*/

	return;
}

static void PrintStatChange(const INT16 change, const INT32 x, const INT32 y, CHAR16 *sString)
{
	if ( change )
	{
		INT16 sX, sY;

		sgp_swprintf( sString, 32, change > 0 ? L"( +%d )" : L"( %d )", change );
		FindFontRightCoordinates( (INT16)(x + TEXT_BOX_WIDTH - 20 + TEXT_DELTA_OFFSET), 0, 30, 0, sString, PERS_FONT, &sX, &sY );
		mprintf( sX, y, L"%s", sString );
	}
}

static void PrintCharStatText(const INT32 x, const INT32 y, const INT32 iCounter, const STR16 sString)
{
	INT16 sX, sY;
	mprintf( x, y, L"%s", pPersonnelScreenStrings[iCounter] );
	FindFontRightCoordinates( x, 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
	mprintf( sX, y, L"%s", sString );
}

void DisplayCharStats( SoldierID iId, INT32 iSlot )
{
	CHAR16	apStr[5000];
	CHAR16 sString[50];
	INT16 sX, sY;
	const TacticalActor *pSoldier =
		GetJa2SoldierRepository().resolve(iId.i);
	if ( !pSoldier )
		return;
	const BOOLEAN fAmIaRobot = AM_A_ROBOT( pSoldier );
	const MERCPROFILESTRUCT *pMercProfile = &gMercProfiles[pSoldier->identity().profile()];

	// SANDRO - remove the regions
	for ( INT8 i = 0; i < 13; i++ )
	{
		ClearPersonnelTraitRegion(i);
	}


	if ( pSoldier->status().flags() & SOLDIER_VEHICLE )
	{
		return;
	}


	// display the stats for a char
	for (INT32 iCounter = 0; iCounter < MAX_STATS; iCounter++ )
	{
		const INT32 x = pPersonnelScreenPoints[iCounter].x + (iSlot * TEXT_BOX_WIDTH);
		const INT32 y = pPersonnelScreenPoints[iCounter].y;

		switch ( iCounter )
		{
		case 0:
			// health
			if ( pSoldier->assignment().current() != ASSIGNMENT_POW )
			{
				// Flugente: stats can have gone up or down, find out which 
				const INT16 change = pMercProfile->bLifeDelta - (INT16)(pSoldier->vitals().criticalStatDamage()[DAMAGED_STAT_HEALTH]);
				PrintStatChange( change, x, y, sString );
				sgp_swprintf(sString, std::size(sString), L"%d/%d", pSoldier->vitals().health(), pSoldier->vitals().maximumHealth() );
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%s", pPOWStrings[1] );
			}

			PrintCharStatText( x, y, iCounter, sString );
			break;
		case 1:
			// agility
			if ( !fAmIaRobot )
			{
				// Flugente: stats can have gone up or down, find out which 
				INT16 change = pMercProfile->bAgilityDelta - (INT16)(pSoldier->vitals().criticalStatDamage()[DAMAGED_STAT_AGILITY]);
				PrintStatChange( change, x, y, sString );
				sgp_swprintf(sString, std::size(sString), L"%d", pSoldier->statistics().agility() );
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%s", gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION] );
			}

			PrintCharStatText( x, y, iCounter, sString );
			break;
		case 2:
			// dexterity
			if ( !fAmIaRobot )
			{
				// Flugente: stats can have gone up or down, find out which 
				INT16 change = pMercProfile->bDexterityDelta - (INT16)(pSoldier->vitals().criticalStatDamage()[DAMAGED_STAT_DEXTERITY]);
				PrintStatChange( change, x, y, sString );
				sgp_swprintf(sString, std::size(sString), L"%d", pSoldier->statistics().dexterity() );
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%s", gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION] );
			}

			PrintCharStatText( x, y, iCounter, sString );
			break;
		case 3:
			// strength
			if ( !fAmIaRobot )
			{
				// Flugente: stats can have gone up or down, find out which 
				INT16 change = pMercProfile->bStrengthDelta - (INT16)(pSoldier->vitals().criticalStatDamage()[DAMAGED_STAT_STRENGTH]);
				PrintStatChange( change, x, y, sString );
				sgp_swprintf(sString, std::size(sString), L"%d", pSoldier->statistics().strength() );
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%s", gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION] );
			}

			PrintCharStatText( x, y, iCounter, sString );
			break;
		case 4:
			// leadership
			if ( !fAmIaRobot )
			{
				// Flugente: stats can have gone up or down, find out which 
				INT16 change = pMercProfile->bLeadershipDelta - (INT16)(pSoldier->vitals().criticalStatDamage()[DAMAGED_STAT_LEADERSHIP]);
				PrintStatChange( change, x, y, sString );
				sgp_swprintf(sString, std::size(sString), L"%d", pSoldier->statistics().leadership() );
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%s", gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION] );
			}

			PrintCharStatText( x, y, iCounter, sString );
			break;
		case 5:
			// wisdom
			if ( !fAmIaRobot )
			{
				// Flugente: stats can have gone up or down, find out which 
				INT16 change = pMercProfile->bWisdomDelta - (INT16)(pSoldier->vitals().criticalStatDamage()[DAMAGED_STAT_WISDOM]);
				PrintStatChange( change, x, y, sString );
				sgp_swprintf(sString, std::size(sString), L"%d", pSoldier->statistics().wisdom() );
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%s", gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION] );
			}

			PrintCharStatText( x, y, iCounter, sString );
			break;
		case 6:
			// exper
			if ( !fAmIaRobot )
			{

				if ( pMercProfile->bExpLevelDelta > 0 )
				{
					sgp_swprintf(sString, std::size(sString), L"( %+d )", pMercProfile->bExpLevelDelta );
					FindFontRightCoordinates( (INT16)(x + TEXT_BOX_WIDTH - 20 + TEXT_DELTA_OFFSET), 0, 30, 0, sString, PERS_FONT, &sX, &sY );
					mprintf( sX, y, L"%s", sString );
				}
				//else
				//{
				sgp_swprintf(sString, std::size(sString), L"%d", pSoldier->statistics().experienceLevel() );
				//}
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%s", gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION] );
			}


			PrintCharStatText( x, y, iCounter, sString );
			break;
		case 7:
			//mrkmanship
			if ( !fAmIaRobot )
			{
				// Flugente: stats can have gone up or down, find out which 
				INT16 change = pMercProfile->bMarksmanshipDelta - (INT16)(pSoldier->vitals().criticalStatDamage()[DAMAGED_STAT_MARKSMANSHIP]);
				PrintStatChange( change, x, y, sString );
				sgp_swprintf(sString, std::size(sString), L"%d", pSoldier->statistics().marksmanship() );
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%s", gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION] );
			}

			PrintCharStatText( x, y, iCounter, sString );
			break;
		case 8:
			// mech
			if ( !fAmIaRobot )
			{
				// Flugente: stats can have gone up or down, find out which 
				INT16 change = pMercProfile->bMechanicDelta - (INT16)(pSoldier->vitals().criticalStatDamage()[DAMAGED_STAT_MECHANICAL]);
				PrintStatChange( change, x, y, sString );
				sgp_swprintf(sString, std::size(sString), L"%d", pSoldier->statistics().mechanical() );
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%s", gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION] );
			}

			PrintCharStatText( x, y, iCounter, sString );
			break;
		case 9:
			// exp
			if ( !fAmIaRobot )
			{
				// Flugente: stats can have gone up or down, find out which 
				INT16 change = pMercProfile->bExplosivesDelta - (INT16)(pSoldier->vitals().criticalStatDamage()[DAMAGED_STAT_EXPLOSIVES]);
				PrintStatChange( change, x, y, sString );
				sgp_swprintf(sString, std::size(sString), L"%d", pSoldier->statistics().explosives() );
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%s", gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION] );
			}

			PrintCharStatText( x, y, iCounter, sString );
			break;
		case 10:
			// med
			if ( !fAmIaRobot )
			{
				// Flugente: stats can have gone up or down, find out which 
				INT16 change = pMercProfile->bMedicalDelta - (INT16)(pSoldier->vitals().criticalStatDamage()[DAMAGED_STAT_MEDICAL]);
				PrintStatChange( change, x, y, sString );
				sgp_swprintf(sString, std::size(sString), L"%d", pSoldier->statistics().medical() );
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%s", gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION] );
			}

			PrintCharStatText( x, y, iCounter, sString );
			break;

			// Added by Flugente
		case 16:
			// Background

			// display background
			if (UsingBackGroundSystem())
			{
				const UINT8 loc = 21;
				const UINT8 regionnr = 12;

				mprintf( (INT16)(pPersonnelScreenPoints[loc].x + (iSlot*TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + 15), L"%s", pPersonnelRecordsHelpTexts[47] ); //L"Background:"

				if ( !pMercProfile->usBackground )
					sgp_swprintf(sString, std::size(sString), L"%s", pwMiscSectorStrings[3] ); //L"unknown"
				else
					sgp_swprintf(sString, std::size(sString), L"%s", zBackground[pMercProfile->usBackground].szShortName );

				FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[loc].x + (iSlot*TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
				mprintf( sX, (pPersonnelScreenPoints[loc].y + 15), L"%s", sString );

				// Add specific region for fast help window
				ClearPersonnelTraitRegion(regionnr);
				MSYS_DefineRegion( &gSkillTraitHelpTextRegion[regionnr], (sX - 3), (UINT16)(pPersonnelScreenPoints[loc].y + 15),
								   (sX + StringPixLength( sString, PERS_FONT ) + 3), (UINT16)(pPersonnelScreenPoints[loc].y + 23), MSYS_PRIORITY_HIGH,
								   MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );

				PublishPersonnelTraitRegion(regionnr);

				// Info about our background
				AssignBackgroundHelpText( pMercProfile->usBackground, &(gSkillTraitHelpTextRegion[12]) );
			}

			break;

			// Added by Flugente
		case 17:
			// sexism, racism, nationalities etc.
		{
				   const UINT8 loc = 22;
				   const UINT8 regionnr = 8;

				   mprintf( (INT16)(pPersonnelScreenPoints[loc].x + (iSlot*TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + 15), L"%s", pPersonnelRecordsHelpTexts[48] ); //L"Personality:"

				   sgp_swprintf(sString, std::size(sString), L"%s", L"->" );

				   FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[loc].x + (iSlot*TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
				   mprintf( sX, (pPersonnelScreenPoints[loc].y + 15), L"%s", sString );

				   // Add specific region for fast help window
				   ClearPersonnelTraitRegion(regionnr);
				   MSYS_DefineRegion( &gSkillTraitHelpTextRegion[regionnr], (sX - 3), (UINT16)(pPersonnelScreenPoints[loc].y + 15),
									  (sX + StringPixLength( sString, PERS_FONT ) + 3), (UINT16)(pPersonnelScreenPoints[loc].y + 23), MSYS_PRIORITY_HIGH,
									  MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );

				   PublishPersonnelTraitRegion(regionnr);

				   // assign bubblehelp text
				   AssignPersonalityHelpText( pSoldier, &(gSkillTraitHelpTextRegion[regionnr]) );
		}

			break;

			/////////////////////////////////////////////////////////////////////////
			// Added by SANDRO
		case 15:
			// Character Trait

			// WANNE: With old trait system, display "Attitudes" instead of "Character"
			if ( gGameOptions.fNewTraitSystem )
				mprintf( (INT16)(pPersonnelScreenPoints[23].x + (iSlot*TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[23].y + 15), L"%s", pPersonnelRecordsHelpTexts[43] ); //L"Character:"
			else
				mprintf( (INT16)(pPersonnelScreenPoints[23].x + (iSlot*TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[23].y + 15), L"%s", pPersonnelRecordsHelpTexts[45] ); //L"Attitudes:"

			if ( gGameOptions.fNewTraitSystem )
				sgp_swprintf(sString, std::size(sString), L"%s", gzIMPCharacterTraitText[pMercProfile->bCharacterTrait] );
			else
				sgp_swprintf(sString, std::size(sString), L"%s", gzIMPAttitudesText[pMercProfile->bAttitude] );

			FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[23].x + (iSlot*TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
			mprintf( sX, (pPersonnelScreenPoints[23].y + 15), L"%s", sString );

			//GetVideoObject(&hHandle, guiQMark);
			//BltVideoObject( FRAME_BUFFER, hHandle, 0,(pPersonnelScreenPoints[23].x + 148), ( pPersonnelScreenPoints[23].y + 5), VO_BLT_SRCTRANSPARENCY,NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(5);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[5], (sX - 3), (UINT16)(pPersonnelScreenPoints[23].y + 15),	// 10
							   (sX + StringPixLength( sString, PERS_FONT ) + 3), (UINT16)(pPersonnelScreenPoints[23].y + 23), MSYS_PRIORITY_HIGH,	// 17
							   MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			//MSYS_DefineRegion( &gSkillTraitHelpTextRegion[3], (UINT16)( pPersonnelScreenPoints[23].x + 147 ), (UINT16)(pPersonnelScreenPoints[23].y + 4),
			//				(UINT16)( pPersonnelScreenPoints[23].x + 166 ), (UINT16)(pPersonnelScreenPoints[23].y + 15), MSYS_PRIORITY_HIGH,
			//					MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(5);
			// Assign the text

			// Only new traits have help text
			if ( gGameOptions.fNewTraitSystem )
				AssignPersonnelCharacterTraitHelpText( pMercProfile->bCharacterTrait );

			break;

		case 18:
			// Disability
			{
				UINT8 loc = 25;
				UINT8 regionnr = 6;

				mprintf( (INT16)( pPersonnelScreenPoints[loc].x + ( iSlot*TEXT_BOX_WIDTH ) ), ( pPersonnelScreenPoints[loc].y + 10 ), L"%s", pPersonnelRecordsHelpTexts[44] ); //L"Disability:"

				int numdisabilities = 0;
				UINT8 disabilityfound = 0;
				for ( UINT8 i = NO_DISABILITY + 1; i < NUM_DISABILITIES; ++i )
				{
					if ( DoesMercHaveDisability( pSoldier, i ) )
					{
						disabilityfound = i;
						++numdisabilities;
					}
				}

				if ( numdisabilities <= 1 )
					sgp_swprintf(sString, std::size(sString), L"%s", gzIMPDisabilityTraitText[disabilityfound] );
				else
					sgp_swprintf(sString, std::size(sString), L"%s, ...", gzIMPDisabilityTraitText[disabilityfound] );
				
				FindFontRightCoordinates( (INT16)( pPersonnelScreenPoints[loc].x + ( iSlot*TEXT_BOX_WIDTH ) ), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
				mprintf( sX, ( pPersonnelScreenPoints[loc].y + 10 ), L"%s", sString );

				// Add specific region for fast help window
				ClearPersonnelTraitRegion(regionnr);
				MSYS_DefineRegion( &gSkillTraitHelpTextRegion[regionnr], ( sX - 3 ), (UINT16)( pPersonnelScreenPoints[loc].y + 10 ),
					( sX + StringPixLength( sString, PERS_FONT ) + 3 ), (UINT16)( pPersonnelScreenPoints[loc].y + 17 ), MSYS_PRIORITY_HIGH,
					MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );

				PublishPersonnelTraitRegion(regionnr);

				// Assign the text
				if ( numdisabilities <= 1 )
					AssignPersonnelDisabilityHelpText( pMercProfile->bDisability );
				else
					AssignPersonnelMultipleDisabilityHelpText( pSoldier );
			}
			break;
			/////////////////////////////////////////////////////////////////////////

			// The Mercs Skills
		case 19:
		{
				   INT32 iWidth;
				   INT32 iMinimumX;
				   INT8	bScreenLocIndex = 19;	//if you change the '19', change it below in the if statement

				   //Display the 'Skills' text
				   mprintf( (INT16)(pPersonnelScreenPoints[bScreenLocIndex].x + (iSlot*TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[bScreenLocIndex].y), L"%s", pPersonnelScreenStrings[PRSNL_TXT_SKILLS] );

				   //KM: April 16, 1999
				   //Added support for the German version, which has potential string overrun problems.	For example, the text "Skills:" can
				   //overlap "NightOps (Expert)" because the German strings are much longer.	In these cases, I ensure that the right
				   //justification of the traits don't overlap.	If it would, I move it over to the right.
				   iWidth = StringPixLength( pPersonnelScreenStrings[PRSNL_TXT_SKILLS], PERS_FONT );
				   iMinimumX = iWidth + pPersonnelScreenPoints[bScreenLocIndex].x + iSlot * TEXT_BOX_WIDTH + 2;

				   if ( !fAmIaRobot )
				   {
					   if ( gGameOptions.fNewTraitSystem ) // SANDRO - old/new traits check
					   {
						   UINT8 ubTempSkillArray[30];
						   INT8 bNumSkillTraits = 0;

						   // lets rearrange our skills to a temp array
						   // we also get the number of lines (skills) to be displayed 
						   for ( UINT8 ubCnt = 1; ubCnt < NUM_SKILLTRAITS_NT; ubCnt++ )
						   {
							   if ( ProfileHasSkillTrait( pSoldier->identity().profile(), ubCnt ) == 2 )
							   {
								   ubTempSkillArray[bNumSkillTraits] = (ubCnt + NEWTRAIT_MERCSKILL_EXPERTOFFSET);
								   bNumSkillTraits++;
							   }
							   else if ( ProfileHasSkillTrait( pSoldier->identity().profile(), ubCnt ) == 1 )
							   {
								   ubTempSkillArray[bNumSkillTraits] = ubCnt;
								   bNumSkillTraits++;
							   }
						   }

						   if ( bNumSkillTraits == 0 )
						   {
							   sgp_swprintf(sString, std::size(sString), L"%s", pPersonnelScreenStrings[PRSNL_TXT_NOSKILLS] );

							   FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[bScreenLocIndex].x + (iSlot*TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
							   mprintf( sX, pPersonnelScreenPoints[bScreenLocIndex].y, L"%s", sString );

							   // Add specific region for fast help window
							   ClearPersonnelTraitRegion(0);
							   MSYS_DefineRegion( &gSkillTraitHelpTextRegion[0], (sX - 3), (UINT16)(pPersonnelScreenPoints[bScreenLocIndex].y),
												  (sX + StringPixLength( sString, PERS_FONT ) + 3), (UINT16)(pPersonnelScreenPoints[bScreenLocIndex].y + 7), MSYS_PRIORITY_HIGH,
												  MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
							   PublishPersonnelTraitRegion(0);

							   // Assign the text
							   sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
							   AssignPersonnelSkillTraitHelpText( 0, FALSE, (gMercProfiles[iId].ubBodyType == REGMALE), apStr );

							   // Set region help text
							   SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[0]), apStr );
							   SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[0], MSYS_NO_CALLBACK );
						   }
						   else
						   {
							   CHAR16 sString2[500];
							   sgp_swprintf(sString2, std::size(sString2), L"%s", L"" );
							   BOOLEAN fDisplayMoreTraits = FALSE;

							   for ( UINT8 ubCnt = 0; ubCnt < bNumSkillTraits; ubCnt++ )
							   {
								   if ( ubCnt >= 3 && bNumSkillTraits > 4 )
								   {
									   fDisplayMoreTraits = TRUE;
									   sgp_swprintf(sString, std::size(sString), L"%s\n", gzMercSkillTextNew[ubTempSkillArray[ubCnt]] );
									   AppendPersonnelText( sString2, sString );
								   }
								   else
								   {
									   sgp_swprintf(sString, std::size(sString), L"%s", gzMercSkillTextNew[ubTempSkillArray[ubCnt]] );

									   if ( ubTempSkillArray[ubCnt] > NEWTRAIT_MERCSKILL_EXPERTOFFSET )
									   {
										   FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[19].x + (iSlot*TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH - 20, 0, sString, FONT10ARIALBOLD, &sX, &sY );

										   if ( sX <= iMinimumX )
										   {
											   FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[19].x + (iSlot*TEXT_BOX_WIDTH) + TEXT_BOX_WIDTH - 20 + TEXT_DELTA_OFFSET), 0, 30, 0, sString, FONT10ARIALBOLD, &sX, &sY );
											   sX = (INT16)std::max<INT32>( sX, iMinimumX );
										   }
										   sY = (INT16)(pPersonnelScreenPoints[19].y + (ubCnt * 12));

										   SetFont( FONT10ARIALBOLD );
										   mprintf( sX, sY, L"%s", sString );
										   SetFont( PERS_FONT );
									   }
									   else
									   {
										   FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[19].x + (iSlot*TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
										   if ( sX <= iMinimumX )
										   {
											   FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[19].x + (iSlot*TEXT_BOX_WIDTH) + TEXT_BOX_WIDTH - 20 + TEXT_DELTA_OFFSET), 0, 30, 0, sString, PERS_FONT, &sX, &sY );
											   sX = (INT16)std::max<INT32>( sX, iMinimumX );
										   }
										   sY = (INT16)(pPersonnelScreenPoints[19].y + (ubCnt * 12));

										   mprintf( sX, sY, L"%s", sString );

									   }

									   // Add specific region for fast help window
									   ClearPersonnelTraitRegion(ubCnt);
									   MSYS_DefineRegion( &gSkillTraitHelpTextRegion[ubCnt], (sX), (sY),
														  (sX + StringPixLength( sString, PERS_FONT )), (sY + 7), MSYS_PRIORITY_HIGH,
														  MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
									   PublishPersonnelTraitRegion(ubCnt);

									   // Assign the text
									   BOOLEAN fExpert = (ubTempSkillArray[ubCnt] > NEWTRAIT_MERCSKILL_EXPERTOFFSET);

									   UINT8 traitnr = ubTempSkillArray[ubCnt];
									   if ( fExpert )
										   traitnr -= NEWTRAIT_MERCSKILL_EXPERTOFFSET;

									   sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
									   AssignPersonnelSkillTraitHelpText( traitnr, fExpert, (pMercProfile->ubBodyType == REGMALE), apStr );

									   // Set region help text
									   SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[ubCnt]), apStr );
									   SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[ubCnt], MSYS_NO_CALLBACK );
								   }
							   }

							   // if we have more skills than we can display, show "more" and create a tooltip box with the rest of them
							   if ( fDisplayMoreTraits )
							   {
								   sgp_swprintf(sString, std::size(sString), L"%s", gzMercSkillTextNew[2 * NEWTRAIT_MERCSKILL_EXPERTOFFSET + 1] ); // display "More..."
								   FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[19].x + (iSlot*TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
								   if ( sX <= iMinimumX )
								   {
									   FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[19].x + (iSlot*TEXT_BOX_WIDTH) + TEXT_BOX_WIDTH - 20 + TEXT_DELTA_OFFSET), 0, 30, 0, sString, PERS_FONT, &sX, &sY );
									   sX = (INT16)std::max<INT32>( sX, iMinimumX );
								   }
								   sY = (INT16)(pPersonnelScreenPoints[19].y + 36);

								   mprintf( sX, sY, L"%s", sString );

								   // Add specific region for fast help window
								   ClearPersonnelTraitRegion(4);
								   MSYS_DefineRegion( &gSkillTraitHelpTextRegion[4], (sX), (sY),
													  (sX + StringPixLength( sString, PERS_FONT )), (sY + 7), MSYS_PRIORITY_HIGH,
													  MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
								   PublishPersonnelTraitRegion(4);
								   // Set region help text
								   SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[4]), sString2 );
								   SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[4], MSYS_NO_CALLBACK );
							   }
						   }
					   }
					   else
					   {
						   INT8 bSkill1 = 0, bSkill2 = 0;
						   bSkill1 = pMercProfile->bSkillTraits[0];
						   bSkill2 = pMercProfile->bSkillTraits[1];

						   //if the 2 skills are the same, add the '(expert)' at the end
						   if ( bSkill1 == bSkill2 && bSkill1 != 0 )
						   {
							   sgp_swprintf(sString, std::size(sString), L"%s %s", gzMercSkillText[bSkill1], gzMercSkillText[EXPERT] );

							   FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[bScreenLocIndex].x + (iSlot*TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );

							   //KM: April 16, 1999
							   //Perform the potential overrun check
							   if ( sX <= iMinimumX )
							   {
								   FindFontRightCoordinates( (INT16)(x + TEXT_BOX_WIDTH - 20 + TEXT_DELTA_OFFSET), 0, 30, 0, sString, PERS_FONT, &sX, &sY );
								   sX = (INT16)std::max<INT32>( sX, iMinimumX );
							   }

							   mprintf( sX, pPersonnelScreenPoints[bScreenLocIndex].y, L"%s", sString );

							   // Add specific region for fast help window
							   ClearPersonnelTraitRegion(0);
							   MSYS_DefineRegion( &gSkillTraitHelpTextRegion[0], (sX - 3), (UINT16)(pPersonnelScreenPoints[bScreenLocIndex].y),
												  (sX + StringPixLength( sString, PERS_FONT ) + 3), (UINT16)(pPersonnelScreenPoints[bScreenLocIndex].y + 7), MSYS_PRIORITY_HIGH,
												  MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
							   PublishPersonnelTraitRegion(0);

							   // Assign the text
							   sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
							   AssignPersonnelSkillTraitHelpText( bSkill1, TRUE, (gMercProfiles[iId].ubBodyType == REGMALE), apStr );

							   // Set region help text
							   SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[0]), apStr );
							   SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[0], MSYS_NO_CALLBACK );
						   }
						   else
						   {
							   //Display the first skill
							   if ( bSkill1 != 0 )
							   {
								   sgp_swprintf(sString, std::size(sString), L"%s", gzMercSkillText[bSkill1] );

								   FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[bScreenLocIndex].x + (iSlot*TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );

								   //KM: April 16, 1999
								   //Perform the potential overrun check
								   sX = (INT16)std::max<INT32>( sX, iMinimumX );
								   mprintf( sX, pPersonnelScreenPoints[bScreenLocIndex].y, L"%s", sString );

								   // Add specific region for fast help window
								   ClearPersonnelTraitRegion(0);
								   MSYS_DefineRegion( &gSkillTraitHelpTextRegion[0], (sX - 3), (UINT16)(pPersonnelScreenPoints[bScreenLocIndex].y),
													  (sX + StringPixLength( sString, PERS_FONT ) + 3), (UINT16)(pPersonnelScreenPoints[bScreenLocIndex].y + 7), MSYS_PRIORITY_HIGH,
													  MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
								   PublishPersonnelTraitRegion(0);

								   // Assign the text
								   sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
								   AssignPersonnelSkillTraitHelpText( bSkill1, FALSE, (gMercProfiles[iId].ubBodyType == REGMALE), apStr );

								   // Set region help text
								   SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[0]), apStr );
								   SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[0], MSYS_NO_CALLBACK );

								   ++bScreenLocIndex;
							   }

							   //Display the second skill
							   if ( bSkill2 != 0 )
							   {
								   sgp_swprintf(sString, std::size(sString), L"%s", gzMercSkillText[bSkill2] );

								   FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[bScreenLocIndex].x + (iSlot*TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );

								   //KM: April 16, 1999
								   //Perform the potential overrun check
								   sX = (INT16)std::max<INT32>( sX, iMinimumX );
								   mprintf( sX, pPersonnelScreenPoints[bScreenLocIndex].y, L"%s", sString );

								   // Add specific region for fast help window
								   ClearPersonnelTraitRegion(1);
								   MSYS_DefineRegion( &gSkillTraitHelpTextRegion[1], (sX - 3), (UINT16)(pPersonnelScreenPoints[bScreenLocIndex].y),
													  (sX + StringPixLength( sString, PERS_FONT ) + 3), (UINT16)(pPersonnelScreenPoints[bScreenLocIndex].y + 7), MSYS_PRIORITY_HIGH,
													  MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
								   PublishPersonnelTraitRegion(1);

								   // Assign the text
								   sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
								   AssignPersonnelSkillTraitHelpText( bSkill2, FALSE, (gMercProfiles[iId].ubBodyType == REGMALE), apStr );

								   // Set region help text
								   SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[1]), apStr );
								   SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[1], MSYS_NO_CALLBACK );

								   ++bScreenLocIndex;
							   }

							   //if no skill was displayed
							   if ( bScreenLocIndex == 19 )
							   {
								   sgp_swprintf(sString, std::size(sString), L"%s", pPersonnelScreenStrings[PRSNL_TXT_NOSKILLS] );

								   FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[bScreenLocIndex].x + (iSlot*TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
								   mprintf( sX, pPersonnelScreenPoints[bScreenLocIndex].y, L"%s", sString );

								   // Add specific region for fast help window
								   ClearPersonnelTraitRegion(0);
								   MSYS_DefineRegion( &gSkillTraitHelpTextRegion[0], (sX - 3), (UINT16)(pPersonnelScreenPoints[bScreenLocIndex].y),
													  (sX + StringPixLength( sString, PERS_FONT ) + 3), (UINT16)(pPersonnelScreenPoints[bScreenLocIndex].y + 7), MSYS_PRIORITY_HIGH,
													  MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
								   PublishPersonnelTraitRegion(0);

								   // Assign the text
								   sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
								   AssignPersonnelSkillTraitHelpText( bSkill1, FALSE, (gMercProfiles[iId].ubBodyType == REGMALE), apStr );

								   // Set region help text
								   SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[0]), apStr );
								   SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[0], MSYS_NO_CALLBACK );
							   }
						   }
					   }
				   }
				   else
				   {
					   sgp_swprintf(sString, std::size(sString), L"%s", gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION] );
				   }
		}
			break;
		}
	}
}

void DisplayCharPersonality(SoldierID iId, INT32 iSlot)
{
	INT32 iCounter=0;
	INT16 sX, sY;
	TacticalActor *pSoldier =
		GetJa2SoldierRepository().resolve(iId.i);
	if ( !pSoldier )
		return;
	const MERCPROFILESTRUCT *pMercProfile = ProfileFor(pSoldier);
	if (!pMercProfile) return;

	// SANDRO - remove the regions
	for( INT8 i = 0; i < 13; ++i )
	{
		ClearPersonnelTraitRegion(i);
	}

	if ( !pSoldier || pSoldier->status().flags() & SOLDIER_VEHICLE || AM_A_ROBOT( pSoldier ) || pSoldier->identity().profile() == NO_PROFILE )
	{
		return;
	}

	int loc = 0;
	int region = 0;

	for ( int i = APPROACH_FRIENDLY; i <= APPROACH_RECRUIT; ++i )
	{
		mprintf( (INT16)(pPersonnelScreenPoints[loc].x + (iSlot*TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + 15), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_FRIENDLY_APPROACH-1 + i] ); // APPROACH_FRIENDLY is 1 so fix the offset

		CHAR16 sStr[200];
		sgp_swprintf(sStr, std::size(sStr), L"%s", L"" );
		CHAR16 sString[200];
		sgp_swprintf(sString, std::size(sString), L"%s", L"" );
		INT32 val = GetEffectiveApproachValue( pSoldier->identity().profile(), i, sString );
	
		sgp_swprintf(sStr, std::size(sStr), L"%d", val );

		FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[loc].x + (iSlot*TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH, 0, sStr, PERS_FONT, &sX, &sY );
		mprintf( sX, (pPersonnelScreenPoints[loc].y + 15), L"%s", sStr );

		// Add specific region for fast help window
		ClearPersonnelTraitRegion(region);

		MSYS_DefineRegion( &gSkillTraitHelpTextRegion[region], (sX - 3), (UINT16)(pPersonnelScreenPoints[loc].y + 15),
						   (sX + StringPixLength( sString, PERS_FONT ) + 3), (UINT16)(pPersonnelScreenPoints[loc].y + 23), MSYS_PRIORITY_HIGH,
						   MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );

		PublishPersonnelTraitRegion(region);
	
		// Set region help text
		SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[region]), sString );
		SetRegionHelpEndCallback( &(gSkillTraitHelpTextRegion[region]), MSYS_NO_CALLBACK );

		++loc;
		++region;
	}

	if ( (pMercProfile->ubMiscFlags3 & PROFILE_MISC_FLAG3_GOODGUY) )
	{
		CHAR16 sStr1[200];
		sgp_swprintf(sStr1, std::size(sStr1), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_GOOD_GUY] );

		CHAR16 sStr2[200];
		sgp_swprintf(sStr2, std::size(sStr2), szLaptopStatText[LAPTOP_STAT_TEXT_REFUSES_TO_ATTACK_NON_HOSTILES], pSoldier->GetName( ) );

		sX = pPersonnelScreenPoints[loc].x + (iSlot*TEXT_BOX_WIDTH);

		mprintf( (INT16)(sX), (pPersonnelScreenPoints[loc].y + 15), L"%s", sStr1 );

		// Add specific region for fast help window
		ClearPersonnelTraitRegion(region);

		MSYS_DefineRegion( &gSkillTraitHelpTextRegion[region], (sX - 3), (UINT16)(pPersonnelScreenPoints[loc].y + 15),
						   (sX + StringPixLength( sStr2, PERS_FONT ) + 3), (UINT16)(pPersonnelScreenPoints[loc].y + 23), MSYS_PRIORITY_HIGH,
						   MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );

		PublishPersonnelTraitRegion(region);

		// Set region help text
		SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[region]), sStr2 );
		SetRegionHelpEndCallback( &(gSkillTraitHelpTextRegion[region]), MSYS_NO_CALLBACK );

		++loc;
		++region;
	}

	if (gGameExternalOptions.fMercGrowthModifiersEnabled)
	{
		if (pMercProfile->fRegresses)
		{
			CHAR16 sStr2[200];
			sgp_swprintf(sStr2, std::size(sStr2), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_MERC_REGRESSES]);

			mprintf((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + 15), L"%s", sStr2);

			++loc;
			++region;
		}
		else
		{
			const int THRESHOLD_FAST = -3; // this value and below: "fast"
			const int THRESHOLD_SLOW = 3; // this value and above: "slow"
			int yOffset = 15;
			CHAR16 statTxt[200];
			// health
			{
				sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_HEALTH_SPEED]);
				mprintf((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);

				if (pMercProfile->bGrowthModifierLife <= THRESHOLD_FAST)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_FAST]);
				else if (pMercProfile->bGrowthModifierLife >= THRESHOLD_SLOW)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_SLOW]);
				else
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_AVERAGE]);
				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH, 0, statTxt, PERS_FONT, &sX, &sY);
				mprintf(sX, (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);
				yOffset += 10;
			}
			// strength
			{
				sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_STRENGTH_SPEED]);
				mprintf((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);

				if (pMercProfile->bGrowthModifierStrength <= THRESHOLD_FAST)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_FAST]);
				else if (pMercProfile->bGrowthModifierStrength >= THRESHOLD_SLOW)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_SLOW]);
				else
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_AVERAGE]);
				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH, 0, statTxt, PERS_FONT, &sX, &sY);
				mprintf(sX, (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);
				yOffset += 10;
			}
			// agility
			{
				sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_AGILITY_SPEED]);
				mprintf((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);

				if (pMercProfile->bGrowthModifierAgility <= THRESHOLD_FAST)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_FAST]);
				else if (pMercProfile->bGrowthModifierAgility >= THRESHOLD_SLOW)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_SLOW]);
				else
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_AVERAGE]);
				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH, 0, statTxt, PERS_FONT, &sX, &sY);
				mprintf(sX, (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);
				yOffset += 10;
			}
			// dexterity
			{
				sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_DEXTERITY_SPEED]);
				mprintf((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);

				if (pMercProfile->bGrowthModifierDexterity <= THRESHOLD_FAST)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_FAST]);
				else if (pMercProfile->bGrowthModifierDexterity >= THRESHOLD_SLOW)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_SLOW]);
				else
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_AVERAGE]);
				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH, 0, statTxt, PERS_FONT, &sX, &sY);
				mprintf(sX, (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);
				yOffset += 10;
			}
			// wisdom
			{
				sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_WISDOM_SPEED]);
				mprintf((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);

				if (pMercProfile->bGrowthModifierWisdom <= THRESHOLD_FAST)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_FAST]);
				else if (pMercProfile->bGrowthModifierWisdom >= THRESHOLD_SLOW)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_SLOW]);
				else
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_AVERAGE]);
				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH, 0, statTxt, PERS_FONT, &sX, &sY);
				mprintf(sX, (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);
				yOffset += 10;
			}
			// marksmanship
			{
				sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_MARKSMANSHIP_SPEED]);
				mprintf((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);

				if (pMercProfile->bGrowthModifierMarksmanship <= THRESHOLD_FAST)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_FAST]);
				else if (pMercProfile->bGrowthModifierMarksmanship >= THRESHOLD_SLOW)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_SLOW]);
				else
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_AVERAGE]);
				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH, 0, statTxt, PERS_FONT, &sX, &sY);
				mprintf(sX, (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);
				yOffset += 10;
			}
			// explosives
			{
				sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_EXPLOSIVES_SPEED]);
				mprintf((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);

				if (pMercProfile->bGrowthModifierExplosive <= THRESHOLD_FAST)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_FAST]);
				else if (pMercProfile->bGrowthModifierExplosive >= THRESHOLD_SLOW)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_SLOW]);
				else
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_AVERAGE]);
				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH, 0, statTxt, PERS_FONT, &sX, &sY);
				mprintf(sX, (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);
				yOffset += 10;
			}
			// leadership
			{
				sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_LEADERSHIP_SPEED]);
				mprintf((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);

				if (pMercProfile->bGrowthModifierLeadership <= THRESHOLD_FAST)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_FAST]);
				else if (pMercProfile->bGrowthModifierLeadership >= THRESHOLD_SLOW)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_SLOW]);
				else
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_AVERAGE]);
				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH, 0, statTxt, PERS_FONT, &sX, &sY);
				mprintf(sX, (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);
				yOffset += 10;
			}
			// medical
			{
				sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_MEDICAL_SPEED]);
				mprintf((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);

				if (pMercProfile->bGrowthModifierMedical <= THRESHOLD_FAST)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_FAST]);
				else if (pMercProfile->bGrowthModifierMedical >= THRESHOLD_SLOW)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_SLOW]);
				else
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_AVERAGE]);
				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH, 0, statTxt, PERS_FONT, &sX, &sY);
				mprintf(sX, (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);
				yOffset += 10;
			}
			// mechanical
			{
				sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_MECHANICAL_SPEED]);
				mprintf((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);

				if (pMercProfile->bGrowthModifierMechanical <= THRESHOLD_FAST)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_FAST]);
				else if (pMercProfile->bGrowthModifierMechanical >= THRESHOLD_SLOW)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_SLOW]);
				else
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_AVERAGE]);
				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH, 0, statTxt, PERS_FONT, &sX, &sY);
				mprintf(sX, (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);
				yOffset += 10;
			}
			// exp level
			{
				sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_EXPERIENCE_SPEED]);
				mprintf((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);

				if (pMercProfile->bGrowthModifierExpLevel <= THRESHOLD_FAST)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_FAST]);
				else if (pMercProfile->bGrowthModifierExpLevel >= THRESHOLD_SLOW)
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_SLOW]);
				else
					sgp_swprintf(statTxt, std::size(statTxt), L"%s", szLaptopStatText[LAPTOP_STAT_TEXT_AVERAGE]);
				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[loc].x + (iSlot * TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH, 0, statTxt, PERS_FONT, &sX, &sY);
				mprintf(sX, (pPersonnelScreenPoints[loc].y + yOffset), L"%s", statTxt);
			}
		}
	}
}


void SetPersonnelButtonStates( void )
{
	const std::size_t count = fCurrentTeamMode
		? currentTeamList.size() : gDepartedRoster.size();
	if (count > 1)
	{
		EnableButton(giPersonnelButton[0]);
		EnableButton(giPersonnelButton[1]);
	}
	else
	{
		DisableButton(giPersonnelButton[0]);
		DisableButton(giPersonnelButton[1]);
	}
}


void RenderPersonnelScreenBackground( void )
{
	HVOBJECT hHandle;
	// this fucntion will render the background for the personnel screen
	if (fCurrentTeamMode) {
		// blit title
		GetVideoObject(&hHandle, guiCURRENTTEAM);
	} else {
		// blit title
		GetVideoObject(&hHandle, guiDEPARTEDTEAM);
	}
	BltVideoObject( FRAME_BUFFER, hHandle, 0,DEPARTED_X, DEPARTED_Y, VO_BLT_SRCTRANSPARENCY,NULL );
}


BOOLEAN LoadPersonnelScreenBackgroundGraphics(LaptopPageResourceOwner& owner)
{
	// will load the graphics for the personeel screen background
	VOBJECT_DESC	VObjectDesc;

	// departed bar
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\departed.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiDEPARTEDTEAM)) return FALSE;

	// current bar
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\CurrentTeam.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiCURRENTTEAM)) return FALSE;
	return TRUE;
}


INT32 GetNumberOfMercsOnPlayersTeam( void )
{
	INT32 iCounter = 0;
	const SoldierID firstTeamId = gTacticalStatus.Team[OUR_TEAM].bFirstID;
	const SoldierID lastTeamId = gTacticalStatus.Team[OUR_TEAM].bLastID;
	for (INT32 rawId = firstTeamId.i; rawId <= lastTeamId.i; ++rawId)
	{
		const TacticalActor* pTeamSoldier =
			GetJa2SoldierRepository().resolve(rawId);
		if( pTeamSoldier && ( pTeamSoldier->roster().active()) &&
			!( pTeamSoldier->status().flags() & SOLDIER_VEHICLE ) &&
			( pTeamSoldier->vitals().health() > 0 ) )
			iCounter++;
	}

	return ( iCounter );
}


BOOLEAN CreatePersonnelPortraitMouseRegions(LaptopPageResourceOwner& owner)
{
	for (INT16 sCounter = 0;
		sCounter < PERSONNEL_PORTRAIT_NUMBER; ++sCounter)
	{
		MSYS_DefineRegion(&gPortraitMouseRegions[ sCounter ], ( INT16 ) ( SMALL_PORTRAIT_START_X + ( sCounter % PERSONNEL_PORTRAIT_NUMBER_WIDTH ) * SMALL_PORT_WIDTH ), ( INT16 ) ( SMALL_PORTRAIT_START_Y +	( sCounter / PERSONNEL_PORTRAIT_NUMBER_WIDTH ) * SMALL_PORT_HEIGHT ) , ( INT16 ) ( ( SMALL_PORTRAIT_START_X ) + ( ( sCounter % PERSONNEL_PORTRAIT_NUMBER_WIDTH ) * SMALL_PORT_WIDTH )	+	SMALL_PORTRAIT_WIDTH ) , ( INT16 )( SMALL_PORTRAIT_START_Y + ( sCounter / PERSONNEL_PORTRAIT_NUMBER_WIDTH ) * SMALL_PORT_HEIGHT + SMALL_PORTRAIT_HEIGHT ),
		 MSYS_PRIORITY_HIGHEST,CURSOR_LAPTOP_SCREEN, MSYS_NO_CALLBACK, PersonnelPortraitCallback);
		MSYS_SetRegionUserData( &gPortraitMouseRegions[ sCounter ] ,0 , sCounter );
		if (!owner.addRegion(gPortraitMouseRegions[sCounter])) return FALSE;
	}
	return TRUE;
}


// WDS - make number of mercenaries, etc. be configurable
// Will display the MAX_MERCS_ON_SCREEN small portraits of the current team
void DisplayPicturesOfCurrentTeam( void )
{
	if (!fCurrentTeamMode || currentTeamList.empty()) return;

	char sTemp[100];
	HVOBJECT hFaceHandle;
	VOBJECT_DESC	VObjectDesc;

	std::size_t countOnScreen = 0;
	const std::size_t first = gCurrentRosterCursor.first();
	for (std::size_t rosterIndex = first;
		rosterIndex < currentTeamList.size() &&
		countOnScreen < MAX_MERCS_ON_SCREEN;
		++rosterIndex)
	{

		TacticalActor *pSoldier =
			GetJa2SoldierRepository().resolve(
				currentTeamList[rosterIndex].i);
		if ( !pSoldier )
			continue;
		const INT32 profileId = pSoldier->identity().profile();
		if (!PersonnelRosterModel::IsValidProfileId(profileId, NUM_PROFILES))
			continue;
		const char* directory = gMercProfiles[profileId].Type == PROFILETYPE_IMP
			? IMP_SMALL_FACES_DIR : SMALL_FACES_DIR;
		snprintf(sTemp, sizeof(sTemp), "%s%02d.sti", directory,
			gMercProfiles[profileId].ubFaceIndex);

		VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
		FilenameForBPP(sTemp, VObjectDesc.ImageFile);
		UniqueVideoObjectHandle face = AddVideoObjectOwned(&VObjectDesc);
		if (!face) continue;
		const UINT32 faceHandle = face.get();

		//Blt face to screen to
		GetVideoObject(&hFaceHandle, faceHandle);

		if (pSoldier->vitals().health() <= 0) {
			hFaceHandle->pShades[ 0 ] = Create16BPPPaletteShaded( hFaceHandle->pPaletteEntry, DEAD_MERC_COLOR_RED, DEAD_MERC_COLOR_GREEN, DEAD_MERC_COLOR_BLUE, TRUE );
			//set the red pallete to the face
			SetObjectHandleShade(faceHandle, 0);
		} // if

		BltVideoObject(FRAME_BUFFER, hFaceHandle, 0,( INT16 ) ( SMALL_PORTRAIT_START_X+ ( countOnScreen % PERSONNEL_PORTRAIT_NUMBER_WIDTH ) * SMALL_PORT_WIDTH ), ( INT16 ) ( SMALL_PORTRAIT_START_Y + ( countOnScreen / PERSONNEL_PORTRAIT_NUMBER_WIDTH ) * SMALL_PORT_HEIGHT ), VO_BLT_SRCTRANSPARENCY,NULL);

		if (pSoldier->vitals().health() <= 0)	{
			//if the merc is dead, display it
			DrawTextToScreen(AimPopUpText[AIM_MEMBER_DEAD], ( INT16 ) ( SMALL_PORTRAIT_START_X+ ( countOnScreen % PERSONNEL_PORTRAIT_NUMBER_WIDTH ) * SMALL_PORT_WIDTH ), ( INT16 ) ( SMALL_PORTRAIT_START_Y + ( countOnScreen / PERSONNEL_PORTRAIT_NUMBER_WIDTH ) * SMALL_PORT_HEIGHT + SMALL_PORT_HEIGHT / 2 ), SMALL_PORTRAIT_WIDTH_NO_BORDERS, FONT10ARIAL, 145, FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED	);
		} // if
		++countOnScreen;
	}
}



void PersonnelPortraitCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	const INT32 iPortraitId = MSYS_GetRegionUserData(pRegion, 0);
	if (iPortraitId < 0) return;

	// callback handler for the minize region that is attatched to the laptop program icon
	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP) {
		if (fCurrentTeamMode) {
			const std::size_t oldSelection = gCurrentRosterCursor.selected();
			if (!gCurrentRosterCursor.selectVisible(
				static_cast<std::size_t>(iPortraitId), currentTeamList.size())) return;
			fReDrawScreenFlag = TRUE;
			// if the selected merc is valid, and they are a POW, change to the inventory display
			TacticalActor* selectedSoldier =
				GetJa2SoldierRepository().resolve(
					currentTeamList[gCurrentRosterCursor.selected()].i);
			if( selectedSoldier &&
				selectedSoldier->assignment().current() == ASSIGNMENT_POW &&
				gubPersonnelInfoState == PERSONNEL_INV_BTN ) {
				gubPersonnelInfoState = PERSONNEL_STAT_BTN;
			}
			if (oldSelection != gCurrentRosterCursor.selected())
			{
				uiCurrentInventoryIndex = 0;
				guiSliderPosition = 0;
			}
		} else {
			if (!gDepartedRosterCursor.selectVisible(
				static_cast<std::size_t>(iPortraitId), gDepartedRoster.size())) return;
			fReDrawScreenFlag = TRUE;
			uiCurrentInventoryIndex = 0;
			guiSliderPosition = 0;
		}
	}

	if( iReason & MSYS_CALLBACK_REASON_RBUTTON_UP )
	{
		if (fCurrentTeamMode) {
			const std::size_t requested = gCurrentRosterCursor.first() +
				static_cast<std::size_t>(iPortraitId);
			if (requested == gCurrentRosterCursor.selected()) {
				//increment the info page when the user right clicks
				if( gubPersonnelInfoState < PERSONNEL_NUM_BTN-1 )
					gubPersonnelInfoState++;
				else
					gubPersonnelInfoState = PERSONNEL_STAT_BTN;
			}
			if (!gCurrentRosterCursor.selectVisible(
				static_cast<std::size_t>(iPortraitId), currentTeamList.size())) return;
			fReDrawScreenFlag = TRUE;

			uiCurrentInventoryIndex = 0;
			guiSliderPosition = 0;

			//if the selected merc is valid, and they are a POW, change to the inventory display
			TacticalActor* selectedSoldier =
				GetJa2SoldierRepository().resolve(
					currentTeamList[gCurrentRosterCursor.selected()].i);
			if( selectedSoldier &&
				selectedSoldier->assignment().current() == ASSIGNMENT_POW &&
				gubPersonnelInfoState == PERSONNEL_INV_BTN) {
				gubPersonnelInfoState = PERSONNEL_STAT_BTN;
			}
		}
	}
}


void DisplayFaceOfDisplayedMerc( )
{
	// if showing inventory, leave
	if (fCurrentTeamMode)
	{
		if (!gCurrentRosterCursor.hasSelection() ||
			gCurrentRosterCursor.selected() >= currentTeamList.size()) return;
		const SoldierID selectedId =
			currentTeamList[gCurrentRosterCursor.selected()];
		DisplayHighLightBox();
		RenderPersonnelFace(-1, selectedId, FALSE, FALSE, FALSE );
		DisplayCharName(selectedId, 0 );
		if ( gubPersonnelInfoState == PERSONNEL_INV_BTN )
		{
			return;
		}

		RenderPersonnelStats(selectedId, 0 );
	}
	else
	{
		const auto* departed = SelectedDepartedEntry();
		if (!departed) return;

		DisplayHighLightBox();
		const INT32 state = static_cast<INT32>(departed->state);
		RenderPersonnelFace(departed->profileId, 0,
			departed->state == PersonnelRosterModel::DepartedState::Dead,
			departed->state == PersonnelRosterModel::DepartedState::Fired,
			departed->state == PersonnelRosterModel::DepartedState::Other);
		DisplayDepartedCharName(departed->profileId, 0, state);
		
		if ( gubPersonnelInfoState == PERSONNEL_INV_BTN )
		{
			return;
		}
		
		DisplayDepartedCharStats(departed->profileId, 0, state);
	}
}

void DisplayInventoryForSelectedChar( void )
{
	// display the inventory for this merc
	if ( gubPersonnelInfoState != PERSONNEL_INV_BTN ) {
		return;
	}

	CreateDestroyPersonnelInventoryScrollButtons( );

	if (!fCurrentTeamMode || !gCurrentRosterCursor.hasSelection() ||
		gCurrentRosterCursor.selected() >= currentTeamList.size()) return;
	RenderInventoryForCharacter(
		currentTeamList[gCurrentRosterCursor.selected()], 0);
}

void RenderInventoryForCharacter( SoldierID iId, INT32 iSlot )
{
	TacticalActor *pSoldier;
	HVOBJECT hHandle;
	ETRLEObject	*pTrav;
	INVTYPE			*pItem;
	INT16				PosX, PosY, sCenX, sCenY;
	UINT32			usHeight, usWidth;
	std::size_t ubItemCount = 0;
	std::size_t ubUpToCount = 0;
	INT16 sX, sY;
	CHAR16 sString[ 128 ];
	INT32 iTotalAmmo = 0;

	GetVideoObject(&hHandle, guiPersonnelInventory);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,( INT16 ) ( iScreenWidthOffset + 397 ), ( INT16 ) ( iScreenHeightOffset + 200 ), VO_BLT_SRCTRANSPARENCY,NULL);

	// SANDRO - remove the regions
	for( INT8 i = 0; i < 13; i++ )
	{
		ClearPersonnelTraitRegion(i);
	}

	if (!fCurrentTeamMode) {
		return;
	}

	// render the bar for the character
	RenderSliderBarForPersonnelInventory( );

	pSoldier = GetJa2SoldierRepository().resolve(iId.i);
	if ( !pSoldier )
		return;

	//if this is a robot, dont display any inventory
	if( AM_A_ROBOT( pSoldier ) )
	{
		return;
	}

	const std::size_t invsize = pSoldier->inventory().size();
	for (std::size_t ubCounter = 0; ubCounter < invsize; ++ubCounter)
	{
		PosX = iScreenWidthOffset + 397 + 3;
		PosY = iScreenHeightOffset + 200 + 8 +( ubItemCount * ( 29 ) );

		//if the character is a robot, only display the inv for the hand pos
		if( CampaignMercenaryPolicy(GetGameContext().capabilities()).isProfile(
				pSoldier->identity().profile(), CampaignProfileCode::Role::Robot) &&
			ubCounter != HANDPOS )
		{
			continue;
		}

		const auto& object = pSoldier->inventory()[ubCounter];
		if (object.exists())
		{
			if( uiCurrentInventoryIndex > ubUpToCount )
			{
				++ubUpToCount;
			}
			else
			{
				const UINT16 itemIndex = object.usItem;
				if (!PersonnelRosterModel::IsValidIndex(itemIndex, MAXITEMS))
					continue;
				pItem = &Item[itemIndex];

				const UINT32 interfaceGraphic = GetInterfaceGraphicForItem(pItem);
				if (!GetVideoObject(&hHandle, interfaceGraphic) || !hHandle)
					continue;
				UINT16 usGraphicNum = g_bUsePngItemImages ? 0 : pItem->ubGraphicNum;
				if (usGraphicNum >= hHandle->usNumberOfObjects ||
					!hHandle->pETRLEObject) continue;
				pTrav = &(hHandle->pETRLEObject[ usGraphicNum ] );

				usHeight				= (UINT32)pTrav->usHeight;
				usWidth					= (UINT32)pTrav->usWidth;

				sCenX = PosX + ( abs( 57 - (int)usWidth ) /	2 ) - pTrav->sOffsetX;
				sCenY = PosY + ( abs( 22 - (int)usHeight ) / 2 ) - pTrav->sOffsetY;

				// shadow
				if(gGameSettings.fOptions[ TOPTION_SHOW_ITEM_SHADOW ]) BltVideoObjectOutlineShadowFromIndex( FRAME_BUFFER, interfaceGraphic, usGraphicNum, sCenX-2, sCenY+2);

				//blt the item
				BltVideoObjectOutlineFromIndex( FRAME_BUFFER, interfaceGraphic, usGraphicNum, sCenX, sCenY, 0, FALSE );

				SetFont( FONT10ARIAL );
				SetFontForeground( FONT_WHITE );
				SetFontBackground( FONT_BLACK );
				SetFontDestBuffer( FRAME_BUFFER, 0,0,SCREEN_WIDTH, SCREEN_HEIGHT, FALSE );

				// grab item name
				LoadItemInfo(itemIndex, sString, NULL );

				// shorten if needed
				if( StringPixLength( sString, FONT10ARIAL) > ( 171 - 75 ) )
				{
					ReduceStringLength( sString, ( 171 - 75 ), FONT10ARIAL );
				}

				// print name
				mprintf(PosX + 65, PosY + 3, L"%s", sString);

				// condition
				if (pItem->usItemClass & IC_AMMO)
				{
					// Ammo
					iTotalAmmo = 0;
					for (UINT8 cnt = 0; cnt < object.ubNumberOfObjects; ++cnt)
					{
						// get total ammo
						iTotalAmmo += object[cnt]->data.ubShotsLeft;
					}
					const UINT16 classIndex = pItem->ubClassIndex;
					const UINT32 capacity = PersonnelRosterModel::IsValidIndex(
						classIndex, MAXITEMS + 1)
						? object.ubNumberOfObjects * Magazine[classIndex].ubMagSize : 0;
					sgp_swprintf(sString, 128, L"%d/%u", iTotalAmmo, capacity);
					FindFontRightCoordinates( ( INT16 )( PosX + 65 ), ( INT16 ) ( PosY + 15 ), ( INT16 ) ( 171 - 75 ),
					( INT16 )( GetFontHeight( FONT10ARIAL ) ), sString, FONT10ARIAL, &sX, &sY );
				}
				else
				{
						sgp_swprintf(sString, 128, L"%2d%%%%", object[0]->data.objectStatus);
						FindFontRightCoordinates( ( INT16 )( PosX + 65 ), ( INT16 ) ( PosY + 15 ), ( INT16 ) ( 171 - 75 ),
							( INT16 )( GetFontHeight( FONT10ARIAL ) ), sString, FONT10ARIAL, &sX, &sY );

						sX += StringPixLength( sSpecialCharacters[0], FONT10ARIAL );
				}



				mprintf(sX, sY, L"%s", sString);

				if (pItem->usItemClass & IC_GUN)
				{
					const UINT16 classIndex = pItem->ubClassIndex;
					if (!PersonnelRosterModel::IsValidIndex(classIndex, MAXITEMS))
						continue;
					const UINT8 calibre = Weapon[classIndex].ubCalibre;
					if (!PersonnelRosterModel::IsValidIndex(calibre, MAXITEMS))
						continue;
					sgp_swprintf(sString, 128, L"%s", AmmoCaliber[calibre]);

					// shorten if needed
					if( StringPixLength( sString, FONT10ARIAL) > ( 171 - 75 ) )
					{
						ReduceStringLength( sString, ( 171 - 75 ), FONT10ARIAL );
					}

					// print name
					mprintf(PosX + 65, PosY + 15, L"%s", sString);


				}

				// if more than 1?
				if (object.ubNumberOfObjects > 1)
				{
					sgp_swprintf(sString, 128, L"x%d", object.ubNumberOfObjects);
					FindFontRightCoordinates( ( INT16 )( PosX ), ( INT16 ) ( PosY + 15 ), ( INT16 ) ( 58 ),
						( INT16 )( GetFontHeight( FONT10ARIAL ) ), sString, FONT10ARIAL, &sX, &sY );
					mprintf(sX, sY, L"%s", sString);
				}

				// display info about it

				ubItemCount++;
			}
		}

		if( ubItemCount == NUMBER_OF_INVENTORY_PERSONNEL )
		{
			break;
		}
	}



	return;
}


void InventoryUpButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;


	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
	btn->uiFlags|=(BUTTON_CLICKED_ON);
	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if( btn->uiFlags & (BUTTON_CLICKED_ON) )
		{
			btn->uiFlags &= ~(BUTTON_CLICKED_ON);

			if( uiCurrentInventoryIndex == 0 )
			{
				return;
			}

			// up one element
			uiCurrentInventoryIndex--;
			fReDrawScreenFlag = TRUE;

			FindPositionOfPersInvSlider( );
		}
	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_REPEAT )
	{
		if( uiCurrentInventoryIndex == 0 )
		{
			return;
		}

		// up one element
		uiCurrentInventoryIndex--;
		fReDrawScreenFlag = TRUE;
		FindPositionOfPersInvSlider( );
	}
}


void InventoryDownButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_REPEAT )
	{
		if( ( INT32 )uiCurrentInventoryIndex >= ( INT32 )( GetNumberOfInventoryItemsOnCurrentMerc( ) - NUMBER_OF_INVENTORY_PERSONNEL ) )
		{
			return;
		}

		// up one element
		uiCurrentInventoryIndex++;
		fReDrawScreenFlag = TRUE;
		FindPositionOfPersInvSlider( );

	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
	btn->uiFlags|=(BUTTON_CLICKED_ON);
	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if( btn->uiFlags & (BUTTON_CLICKED_ON) )
		{
			btn->uiFlags &= ~(BUTTON_CLICKED_ON);

			if( ( INT32 )uiCurrentInventoryIndex >= ( INT32 )( GetNumberOfInventoryItemsOnCurrentMerc( ) - NUMBER_OF_INVENTORY_PERSONNEL ) )
			{
				return;
			}

			// up one element
			uiCurrentInventoryIndex++;
			fReDrawScreenFlag = TRUE;

			FindPositionOfPersInvSlider( );

		}
	}

}


// decide which buttons can and can't be accessed based on what the current item is
void EnableDisableInventoryScrollButtons( void )
{
//	if( fShowInventory == FALSE )
	if ( gubPersonnelInfoState != PERSONNEL_INV_BTN )
	{
		return;
	}

	const std::size_t itemCount = static_cast<std::size_t>(
		std::max(GetNumberOfInventoryItemsOnCurrentMerc(), 0));
	uiCurrentInventoryIndex = static_cast<UINT8>(
		PersonnelRosterModel::NormalizeWindowStart(uiCurrentInventoryIndex,
			itemCount, NUMBER_OF_INVENTORY_PERSONNEL));
	if( uiCurrentInventoryIndex == 0 )
	{
		ButtonList[ giPersonnelInventoryButtons[ 0 ] ]->uiFlags &= ~( BUTTON_CLICKED_ON );
		DisableButton( giPersonnelInventoryButtons[ 0 ] );
	}
	else
	{
		EnableButton( giPersonnelInventoryButtons[ 0 ] );
	}


	if (!PersonnelRosterModel::CanScrollWindowDown(uiCurrentInventoryIndex,
		itemCount, NUMBER_OF_INVENTORY_PERSONNEL))
	{
		ButtonList[ giPersonnelInventoryButtons[ 1 ] ]->uiFlags &= ~( BUTTON_CLICKED_ON );
		DisableButton( giPersonnelInventoryButtons[ 1 ] );
	}
	else
	{
		EnableButton( giPersonnelInventoryButtons[ 1 ] );
	}


	return;
}

INT32 GetNumberOfInventoryItemsOnCurrentMerc( void )
{
	// in current team mode?..nope...move on
	if (!fCurrentTeamMode || !gCurrentRosterCursor.hasSelection() ||
		gCurrentRosterCursor.selected() >= currentTeamList.size())
		return( 0 );

	TacticalActor *pSoldier =
		GetJa2SoldierRepository().resolve(
			currentTeamList[gCurrentRosterCursor.selected()].i);
	if ( !pSoldier )
		return 0;

	INT32 ubCount = 0;
	UINT8 invsize = pSoldier->inventory().size();
	for (UINT8 ubCounter = 0; ubCounter < invsize; ++ubCounter)
	{
		if( ( pSoldier->inventory()[ ubCounter ].exists() == true) )
			++ubCount;
	}

	return ubCount;
}

void CreateDestroyPersonnelInventoryScrollButtons( void )
{
	const bool shouldCreate = fCurrentTeamMode &&
		gubPersonnelInfoState == PERSONNEL_INV_BTN;
	if (shouldCreate && gPersonnelInventoryResources.empty())
	{
		LaptopPageResourceOwner staged;
		if (!staged.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\personnel_inventory.sti", -1, 1, -1, 2, -1),
			giPersonnelInventoryButtonsImages[0])) return;
		if (!staged.addButton(QuickCreateButton( giPersonnelInventoryButtonsImages[0], iScreenWidthOffset + 176 + 397, iScreenHeightOffset + 2 + 200,
					 BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
											BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)InventoryUpButtonCallback),
			giPersonnelInventoryButtons[0])) return;

		if (!staged.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\personnel_inventory.sti", -1, 3, -1, 4, -1),
			giPersonnelInventoryButtonsImages[1])) return;
		if (!staged.addButton(QuickCreateButton( giPersonnelInventoryButtonsImages[1], iScreenWidthOffset + 397 + 176, iScreenHeightOffset + 200 + 223,
					 BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
											BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)InventoryDownButtonCallback),
			giPersonnelInventoryButtons[1])) return;

			// set up cursors for these buttons
		SetButtonCursor( giPersonnelInventoryButtons[ 0 ], CURSOR_LAPTOP_SCREEN);
	SetButtonCursor( giPersonnelInventoryButtons[ 1 ], CURSOR_LAPTOP_SCREEN);

		MSYS_DefineRegion( &gMouseScrollPersonnelINV, X_OF_PERSONNEL_SCROLL_REGION, Y_OF_PERSONNEL_SCROLL_REGION, X_OF_PERSONNEL_SCROLL_REGION + X_SIZE_OF_PERSONNEL_SCROLL_REGION, Y_OF_PERSONNEL_SCROLL_REGION + Y_SIZE_OF_PERSONNEL_SCROLL_REGION,
			MSYS_PRIORITY_HIGHEST - 3, CURSOR_LAPTOP_SCREEN, MSYS_NO_CALLBACK, HandleSliderBarClickCallback );
		if (!staged.addRegion(gMouseScrollPersonnelINV)) return;
		gPersonnelInventoryResources = std::move(staged);
	}
	else if (!shouldCreate && !gPersonnelInventoryResources.empty())
	{
		gPersonnelInventoryResources.clear();
	}
}

void DisplayNumberOnCurrentTeam( void )
{
	// display number on team
	CHAR16 sString[ 32 ];
	INT16 sX = 0, sY = 0;

	// font stuff
	SetFont( FONT10ARIAL );
	SetFontBackground( FONT_BLACK );
	SetFontForeground( PERS_TEXT_FONT_COLOR );

	if (fCurrentTeamMode) {
		sgp_swprintf(sString, 32, L"%s ( %zu )",
			pPersonelTeamStrings[0], currentTeamList.size());
		sX = PERS_CURR_TEAM_X;
	} else {
		sgp_swprintf(sString, std::size(sString), L"%s", pPersonelTeamStrings[ 0 ] );

		FindFontCenterCoordinates( PERS_CURR_TEAM_X, 0, 65, 0,sString, FONT10ARIAL, &sX, &sY );
	}

	mprintf( sX, PERS_CURR_TEAM_Y, L"%s", sString );

	// now the cost of the current team, if applicable
	DisplayCostOfCurrentTeam( );
}

void DisplayNumberDeparted( void )
{
	// display number departed from team
	CHAR16 sString[ 32 ];
	INT16 sX = 0, sY = 0;


	// font stuff
	SetFont( FONT10ARIAL );
	SetFontBackground( FONT_BLACK );
	SetFontForeground( PERS_TEXT_FONT_COLOR );

	if (!fCurrentTeamMode) {
		sgp_swprintf(sString, std::size(sString), L"%s ( %d )", pPersonelTeamStrings[ 1 ], GetNumberOfPastMercsOnPlayersTeam( ) );
		sX = PERS_CURR_TEAM_X;
	} else {
		sgp_swprintf(sString, std::size(sString), L"%s", pPersonelTeamStrings[ 1 ] );
		FindFontCenterCoordinates( PERS_CURR_TEAM_X, 0, 65, 0,sString, FONT10ARIAL, &sX, &sY );
	}

	mprintf( sX, PERS_DEPART_TEAM_Y, L"%s", sString );
}


INT32 GetTotalDailyCostOfCurrentTeam( void )
{
	std::uint64_t total = 0;
	for (const SoldierID id : currentTeamList)
	{
		const TacticalActor* soldier = GetJa2SoldierRepository().resolve(id.i);
		if (!soldier || !soldier->roster().active() ||
			soldier->status().flags() & SOLDIER_VEHICLE ||
			soldier->vitals().health() <= 0) continue;
		total += static_cast<std::uint64_t>(DailyCostFor(soldier));
	}
	return PersonnelRosterModel::ClampCurrency(total);
}

INT32 GetLowestDailyCostOfCurrentTeam( void )
{
	INT32 lowest = std::numeric_limits<INT32>::max();
	for (const SoldierID id : currentTeamList)
	{
		const TacticalActor* soldier = GetJa2SoldierRepository().resolve(id.i);
		if (!soldier || !soldier->roster().active() ||
			soldier->status().flags() & SOLDIER_VEHICLE ||
			soldier->vitals().health() <= 0) continue;
		lowest = std::min(lowest, DailyCostFor(soldier));
	}
	return lowest == std::numeric_limits<INT32>::max() ? 0 : lowest;
}

INT32 GetHighestDailyCostOfCurrentTeam( void )
{
	INT32 highest = 0;
	for (const SoldierID id : currentTeamList)
	{
		const TacticalActor* soldier = GetJa2SoldierRepository().resolve(id.i);
		if (!soldier || !soldier->roster().active() ||
			soldier->status().flags() & SOLDIER_VEHICLE ||
			soldier->vitals().health() <= 0) continue;
		highest = std::max(highest, DailyCostFor(soldier));
	}
	return highest;
}

void DisplayCostOfCurrentTeam( void )
{
	// display number on team
	INT16 sX, sY;
	
	// font stuff
	SetFont( FONT10ARIAL );
	SetFontBackground( FONT_BLACK );
	SetFontForeground( PERS_TEXT_FONT_COLOR );

	if (fCurrentTeamMode) {
		std::wstring sString{};
		// daily cost
		mprintf(	PERS_CURR_TEAM_COST_X, PERS_CURR_TEAM_COST_Y, L"%s", pPersonelTeamStrings[ 2 ] );

		sString = FormatMoney( GetTotalDailyCostOfCurrentTeam( ) );
		FindFontRightCoordinates((INT16)(PERS_CURR_TEAM_COST_X),0,PERS_CURR_TEAM_WIDTH,0,sString.data(), PERS_FONT, &sX, &sY);

		mprintf( sX ,PERS_CURR_TEAM_COST_Y, L"%s", sString.data() );

		// highest cost
		mprintf(	PERS_CURR_TEAM_COST_X, PERS_CURR_TEAM_HIGHEST_Y, L"%s", pPersonelTeamStrings[ 3 ] );

		sString = FormatMoney( GetHighestDailyCostOfCurrentTeam( ) );
		FindFontRightCoordinates((INT16)(PERS_CURR_TEAM_COST_X),0,PERS_CURR_TEAM_WIDTH,0,sString.data(), PERS_FONT, &sX, &sY);

		mprintf( sX ,PERS_CURR_TEAM_HIGHEST_Y, L"%s", sString.data() );

		// the lowest cost
		mprintf(	PERS_CURR_TEAM_COST_X, PERS_CURR_TEAM_LOWEST_Y, L"%s", pPersonelTeamStrings[ 4 ] );

		sString = FormatMoney( GetLowestDailyCostOfCurrentTeam( ) );
		FindFontRightCoordinates((INT16)(PERS_CURR_TEAM_COST_X),0,PERS_CURR_TEAM_WIDTH,0,sString.data(), PERS_FONT, &sX, &sY);

		mprintf( sX ,PERS_CURR_TEAM_LOWEST_Y, L"%s", sString.data() );
	}
}

INT32 GetIdOfDepartedMercWithHighestStat( INT32 iStat )
{
	// will return the id value of the merc on the players team with highest in this stat
	// -1 means error
	INT32 iId = -1;
	INT32 iValue =0;
	MERCPROFILESTRUCT *pTeamSoldier;
	TacticalActor *pSoldier;

	for (const auto& entry : gDepartedRoster)
	{
		const INT32 cnt = entry.profileId;
		pTeamSoldier = &( gMercProfiles[ cnt ] );

		switch( iStat )
		{
			case 0:
			// health

				//if the soldier is a pow, dont use the health cause it aint known
			pSoldier = FindSoldierByProfileID( (UINT8)cnt, FALSE );
			if( pSoldier && pSoldier->assignment().current() == ASSIGNMENT_POW )
			{
				continue;
			}

			if( pTeamSoldier->bLife >= iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bLife;
			}
			break;
		case 1:
		// agility
			if( pTeamSoldier->bAgility >= iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bAgility;
			}
			break;
			case 2:
			// dexterity
			if( pTeamSoldier->bDexterity >= iValue )
			{
					iId = cnt;
					iValue = pTeamSoldier->bDexterity;
			}
			break;
			case 3:
			// strength
		if(	pTeamSoldier->bStrength >= iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bStrength;
			}
			break;
			case 4:
			// leadership
		if(	pTeamSoldier->bLeadership >= iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bLeadership;
			}
			break;
			case 5:
		// wisdom
			if(	pTeamSoldier->bWisdom >= iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bWisdom;
			}
			break;
			case 6:
			// exper
		if( pTeamSoldier->bExpLevel >= iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bExpLevel;
			}

			break;
			case 7:
			//mrkmanship
			if(	pTeamSoldier->bMarksmanship >= iValue )
			{
					iId = cnt;
					iValue = pTeamSoldier->bMarksmanship;
			}

		break;
			case 8:
			// mech
			if(	pTeamSoldier->bMechanical >= iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bMechanical;
			}
		break;
			case 9:
			// exp
			if(pTeamSoldier->bExplosive >= iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bExplosive;
			}
			break;
			case 10:
			// med
			if(	pTeamSoldier->bMedical >= iValue )
			{
					iId = cnt;
					iValue = pTeamSoldier->bMedical;
			}
			break;
		}

	}
	
	return( iId );
}

INT32 GetIdOfDepartedMercWithLowestStat( INT32 iStat )
{
	// will return the id value of the merc on the players team with highest in this stat
	// -1 means error
	INT32 iId = -1;
	INT32 iValue =9999999;
	MERCPROFILESTRUCT *pTeamSoldier;
	TacticalActor		*pSoldier;

	for (const auto& entry : gDepartedRoster)
	{
		const INT32 cnt = entry.profileId;
		pTeamSoldier = &( gMercProfiles[ cnt ] );


		switch( iStat )
		{
			case 0:
			// health

			pSoldier = FindSoldierByProfileID( (UINT8)cnt, FALSE );
			if( pSoldier && pSoldier->assignment().current() == ASSIGNMENT_POW )
			{
				continue;
			}

			if( pTeamSoldier->bLife < iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bLife;
			}
			break;
		case 1:
		// agility
			if( pTeamSoldier->bAgility < iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bAgility;
			}
			break;
			case 2:
			// dexterity
			if( pTeamSoldier->bDexterity < iValue )
			{
					iId = cnt;
					iValue = pTeamSoldier->bDexterity;
			}
			break;
			case 3:
			// strength
		if(	pTeamSoldier->bStrength < iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bStrength;
			}
			break;
			case 4:
			// leadership
		if(	pTeamSoldier->bLeadership < iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bLeadership;
			}
			break;
			case 5:
		// wisdom
			if(	pTeamSoldier->bWisdom < iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bWisdom;
			}
			break;
			case 6:
			// exper
		if( pTeamSoldier->bExpLevel < iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bExpLevel;
			}

			break;
			case 7:
			//mrkmanship
			if(	pTeamSoldier->bMarksmanship < iValue )
			{
					iId = cnt;
					iValue = pTeamSoldier->bMarksmanship;
			}

		break;
			case 8:
			// mech
			if(	pTeamSoldier->bMechanical < iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bMechanical;
			}
		break;
			case 9:
			// exp
			if(pTeamSoldier->bExplosive < iValue )
			{
				iId = cnt;
				iValue = pTeamSoldier->bExplosive;
			}
			break;
			case 10:
			// med
			if(	pTeamSoldier->bMedical < iValue )
			{
					iId = cnt;
					iValue = pTeamSoldier->bMedical;
			}
			break;
		}

	}


	return( iId );
}


SoldierID GetIdOfMercWithHighestStat( INT32 iStat )
{
	// will return the id value of the merc on the players team with highest in this stat
	INT32 iId = NOBODY;
	INT32 iValue =0;
	TacticalActor *pTeamSoldier;

	// run through active soldiers
	for (const SoldierID id : currentTeamList)
	{
		const INT32 cnt = id.i;
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if( pTeamSoldier && ( pTeamSoldier->roster().active()) &&
			!( pTeamSoldier->status().flags() & SOLDIER_VEHICLE ) &&
			( pTeamSoldier->vitals().health() > 0 ) &&
			!AM_A_ROBOT( pTeamSoldier ) )
		{
			switch( iStat )
			{
				case 0:
			// health
						if( pTeamSoldier->assignment().current() == ASSIGNMENT_POW )
						{
							continue;
						}

					if( pTeamSoldier->vitals().maximumHealth() >= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->vitals().maximumHealth();
					}
				break;
				case 1:
					// agility
					if( pTeamSoldier->statistics().agility() >= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().agility();
					}
					break;
				case 2:
					// dexterity
					if( pTeamSoldier->statistics().dexterity() >= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().dexterity();
					}
					break;
				case 3:
				// strength
				if(	pTeamSoldier->statistics().strength() >= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().strength();
					}
				break;
				case 4:
					// leadership
				if(	pTeamSoldier->statistics().leadership() >= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().leadership();
					}
				break;
				case 5:
					// wisdom
					if(	pTeamSoldier->statistics().wisdom() >= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().wisdom();
					}
				break;
				case 6:
					// exper
			if( pTeamSoldier->statistics().experienceLevel() >= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().experienceLevel();
					}

				break;
				case 7:
					//mrkmanship
					if(	pTeamSoldier->statistics().marksmanship() >= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().marksmanship();
					}

				break;
				case 8:
					// mech
					if(	pTeamSoldier->statistics().mechanical() >= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().mechanical();
					}
				break;
				case 9:
					// exp
					if(pTeamSoldier->statistics().explosives() >= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().explosives();
					}
				break;
				case 10:
					// med
					if(	pTeamSoldier->statistics().medical() >= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().medical();
					}
				break;
				}
		}
	}


	return( iId );
}

SoldierID GetIdOfMercWithLowestStat( INT32 iStat )
{
	// will return the id value of the merc on the players team with highest in this stat
	SoldierID iId = NOBODY;
	INT32 iValue =999999;
	TacticalActor *pTeamSoldier;

	// run through active soldiers
	for (const SoldierID id : currentTeamList)
	{
		const INT32 cnt = id.i;
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if( pTeamSoldier && ( pTeamSoldier->roster().active()) &&
			!( pTeamSoldier->status().flags() & SOLDIER_VEHICLE ) &&
			( pTeamSoldier->vitals().health() > 0 ) &&
			!AM_A_ROBOT( pTeamSoldier ) )
		{

			switch( iStat )
			{
				case 0:
			// health

					if( pTeamSoldier->assignment().current() == ASSIGNMENT_POW )
					{
						continue;
					}

					if( pTeamSoldier->vitals().maximumHealth() <= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->vitals().maximumHealth();
					}
				break;
				case 1:
					// agility
					if( pTeamSoldier->statistics().agility() <= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().agility();
					}
					break;
				case 2:
					// dexterity
					if(	pTeamSoldier->statistics().dexterity() <= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().dexterity();
					}
					break;
				case 3:
				// strength
				if(	pTeamSoldier->statistics().strength() <= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().strength();
					}
				break;
				case 4:
					// leadership
				if( pTeamSoldier->statistics().leadership() <= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().leadership();
					}
				break;
				case 5:
					// wisdom
					if( pTeamSoldier->statistics().wisdom() <= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().wisdom();
					}
				break;
				case 6:
					// exper
					if(	pTeamSoldier->statistics().experienceLevel() <= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().experienceLevel();
					}

				break;
				case 7:
					//mrkmanship
					if(	pTeamSoldier->statistics().marksmanship() <= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().marksmanship();
					}

				break;
				case 8:
					// mech
					if(	pTeamSoldier->statistics().mechanical() <= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().mechanical();
					}
				break;
				case 9:
					// exp
					if(	pTeamSoldier->statistics().explosives() <= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().explosives();
					}
				break;
				case 10:
					// med
					if(	pTeamSoldier->statistics().medical() <= iValue )
					{
						iId = cnt;
						iValue = pTeamSoldier->statistics().medical();
					}
				break;
				}
		}
	}


	return( iId );
}


INT32 GetAvgStatOfCurrentTeamStat( INT32 iStat )
{
	// will return the id value of the merc on the players team with highest in this stat
	// -1 means error
	TacticalActor *pTeamSoldier;
	INT32 iTotalStatValue = 0;
	INT8	bNumberOfPows = 0;
	UINT8	ubNumberOfMercsInCalculation = 0;


	// run through active soldiers
	for (const SoldierID id : currentTeamList)
	{
		const INT32 cnt = id.i;
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if ( !pTeamSoldier )
			continue;
		// Only count stats of merc (not vehicles)
		if ( !( pTeamSoldier->status().flags() & SOLDIER_VEHICLE ) )
		{
		if(( pTeamSoldier->roster().active())&&( pTeamSoldier->vitals().health() > 0 ) && !AM_A_ROBOT( pTeamSoldier ) )
		{
			switch( iStat )
			{
				case 0:
			// health

						//if this is a pow, dont count his stats
						if( pTeamSoldier->assignment().current() == ASSIGNMENT_POW )
						{
							bNumberOfPows++;
							continue;
						}

					iTotalStatValue += pTeamSoldier->vitals().maximumHealth();

				break;
				case 1:
					// agility
					iTotalStatValue +=pTeamSoldier->statistics().agility();

					break;
				case 2:
					// dexterity
					iTotalStatValue +=	pTeamSoldier->statistics().dexterity();

					break;
				case 3:
				// strength
				iTotalStatValue +=	pTeamSoldier->statistics().strength();

				break;
				case 4:
					// leadership
					iTotalStatValue +=	pTeamSoldier->statistics().leadership();

				break;
				case 5:
					// wisdom

					iTotalStatValue += pTeamSoldier->statistics().wisdom();
				break;
				case 6:
					// exper

					iTotalStatValue +=	pTeamSoldier->statistics().experienceLevel();

				break;
				case 7:
					//mrkmanship

					iTotalStatValue +=	pTeamSoldier->statistics().marksmanship();

				break;
				case 8:
					// mech

					iTotalStatValue +=	pTeamSoldier->statistics().mechanical();
				break;
				case 9:
					// exp

					iTotalStatValue +=	pTeamSoldier->statistics().explosives();
				break;
				case 10:
					// med

					iTotalStatValue +=	pTeamSoldier->statistics().medical();
				break;
			}

			ubNumberOfMercsInCalculation++;
		}
	}
	}


	//if the stat is health, and there are only pow's
	if( GetNumberOfMercsOnPlayersTeam( ) != 0 && GetNumberOfMercsOnPlayersTeam( ) == bNumberOfPows && iStat == 0 )
	{
		return( - 1 );
	}
	else if( ( ubNumberOfMercsInCalculation - bNumberOfPows ) > 0 )
	{
		return( iTotalStatValue / ( ubNumberOfMercsInCalculation - bNumberOfPows ) );
	}
	else
	{
		return( 0 );
	}
}


INT32 GetAvgStatOfPastTeamStat( INT32 iStat )
{
	INT32 iTotalStatValue = 0;
	MERCPROFILESTRUCT *pTeamSoldier;

	for (const auto& entry : gDepartedRoster)
	{
		const INT32 cnt = entry.profileId;
		pTeamSoldier = &( gMercProfiles[ cnt ] );



		switch( iStat )
		{
			case 0:
			// health

				iTotalStatValue += pTeamSoldier->bLife;

			break;
		case 1:
		// agility

				iTotalStatValue += pTeamSoldier->bAgility;

			break;
			case 2:
			// dexterity

					iTotalStatValue += pTeamSoldier->bDexterity;

			break;
			case 3:
			// strength

				iTotalStatValue += pTeamSoldier->bStrength;

			break;
			case 4:
			// leadership

				iTotalStatValue += pTeamSoldier->bLeadership;

			break;
			case 5:
		// wisdom

				iTotalStatValue += pTeamSoldier->bWisdom;

			break;
			case 6:
			// exper

				iTotalStatValue += pTeamSoldier->bExpLevel;


			break;
			case 7:
			//mrkmanship

					iTotalStatValue += pTeamSoldier->bMarksmanship;


		break;
			case 8:
			// mech

				iTotalStatValue += pTeamSoldier->bMechanical;

		break;
			case 9:
			// exp

				iTotalStatValue += pTeamSoldier->bExplosive;

			break;
			case 10:
			// med

				iTotalStatValue += pTeamSoldier->bMedical;
			break;
		}

	}

	return gDepartedRoster.empty() ? 0
		: iTotalStatValue / static_cast<INT32>(gDepartedRoster.size());
}

void DisplayAverageStatValuesForCurrentTeam( void )
{
	// will display the average values for stats for the current team
	INT16 sX, sY;
	INT32 iCounter = 0;
	CHAR16 sString[ 32 ];

	// set up font
	SetFont( FONT10ARIAL );
	SetFontBackground( FONT_BLACK );
	SetFontForeground( PERS_TEXT_FONT_COLOR );

	// display header

	// center
	FindFontCenterCoordinates( PERS_STAT_AVG_X, 0 ,PERS_STAT_AVG_WIDTH, 0 , pPersonnelCurrentTeamStatsStrings[ 1 ], FONT10ARIAL , &sX, &sY );

	mprintf( sX, PERS_STAT_AVG_Y, L"%s", pPersonnelCurrentTeamStatsStrings[ 1 ] );

	// nobody on team leave
	if (fCurrentTeamMode) {
		if (currentTeamList.empty()) {
			return;
		}
	} else  {
	    if (GetNumberOfPastMercsOnPlayersTeam() == 0) {
			return;
		}
	}

	for( iCounter = 0; iCounter < 11; iCounter++ )
	{
		// even or odd?..color black or yellow?
		if( iCounter % 2 == 0 )
		{
			SetFontForeground( PERS_TEXT_FONT_ALTERNATE_COLOR );
		}
		else
		{
			SetFontForeground( PERS_TEXT_FONT_COLOR );
		}

		if (fCurrentTeamMode)
		{
			INT32	iValue = GetAvgStatOfCurrentTeamStat( iCounter );

			//if there are no values
			if( iValue == -1 )
				sgp_swprintf(sString, std::size(sString), L"%s", pPOWStrings[ 1 ] );
			else
				sgp_swprintf(sString, std::size(sString), L"%d", iValue );

		}
		else
		{
			sgp_swprintf(sString, std::size(sString), L"%d", GetAvgStatOfPastTeamStat( iCounter ) );
		}
		// center
		FindFontCenterCoordinates( PERS_STAT_AVG_X, 0 ,PERS_STAT_AVG_WIDTH, 0 , sString, FONT10ARIAL , &sX, &sY );
		mprintf( sX, PERS_STAT_AVG_Y + ( iCounter + 1 ) * ( GetFontHeight( FONT10ARIAL ) + 3 ), L"%s", sString );
	}
}

void DisplayLowestStatValuesForCurrentTeam( void )
{
	// will display the average values for stats for the current team
	INT16		sX, sY;
	INT32		iCounter = 0;
	CHAR16		sString[ 32 ];
	INT32		iStat = 0;
	INT32		iDepartedId = -1;
	SoldierID	iId;
	const bool currentTeamMode = fCurrentTeamMode != FALSE;

	// set up font
	SetFont( FONT10ARIAL );
	SetFontBackground( FONT_BLACK );
	SetFontForeground( PERS_TEXT_FONT_COLOR );

	// display header

	// center
	FindFontCenterCoordinates( PERS_STAT_LOWEST_X, 0 ,PERS_STAT_LOWEST_WIDTH, 0 , pPersonnelCurrentTeamStatsStrings[ 0 ], FONT10ARIAL , &sX, &sY );

	mprintf( sX, PERS_STAT_AVG_Y, L"%s", pPersonnelCurrentTeamStatsStrings[ 0 ] );

	// nobody on team leave
	if (currentTeamMode) {
		if (currentTeamList.empty()) {
			return;
		}
	} else  {
	    if (GetNumberOfPastMercsOnPlayersTeam() == 0) {
			return;
		}
	}

	for( iCounter = 0; iCounter < 11; iCounter++ )
	{
		if (currentTeamMode) {
			iId = GetIdOfMercWithLowestStat( iCounter );
			if( iId == NOBODY )
				continue;
		} else {
			iDepartedId = GetIdOfDepartedMercWithLowestStat( iCounter );
			if( iDepartedId == -1 )
				continue;
		}

		// even or odd?..color black or yellow?
		if( iCounter % 2 == 0 ) {
			SetFontForeground( PERS_TEXT_FONT_ALTERNATE_COLOR );
		} else {
			SetFontForeground( PERS_TEXT_FONT_COLOR );
		}

		TacticalActor* currentSoldier = ResolveLaptopRosterActor(
			currentTeamMode, iId.i,
			[](UINT16 id)
			{
				return GetJa2SoldierRepository().resolve(id);
			});
		if ( currentTeamMode && !currentSoldier )
			continue;
		if (currentTeamMode) {
			sgp_swprintf(sString, std::size(sString), L"%s",
				currentSoldier->GetName() );
		} else {
			// get name
			sgp_swprintf(sString, std::size(sString), L"%s", gMercProfiles[ iDepartedId ].zNickname );
		}
		// print name
		mprintf( PERS_STAT_LOWEST_X, PERS_STAT_AVG_Y + ( iCounter + 1 ) * ( GetFontHeight( FONT10ARIAL ) + 3 ), L"%s", sString );

		switch (iCounter) {
			case 0:
				// health
				if (currentTeamMode) {
					iStat =
						currentSoldier->vitals().maximumHealth();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bLife;
				}
				break;
			case 1:
				// agility
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().agility();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bAgility;
				}
				break;
			case 2:
				// dexterity
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().dexterity();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bDexterity;
				}
				break;
			case 3:
				// strength
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().strength();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bStrength;
				}
				break;
			case 4:
				// leadership
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().leadership();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bLeadership;
				}
				break;
			case 5:
				// wisdom
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().wisdom();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bWisdom;
				}
				break;
			case 6:
				// exper
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().experienceLevel();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bExpLevel;
				}
				break;
			case 7:
				//mrkmanship
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().marksmanship();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bMarksmanship;
				}
				break;
			case 8:
				// mech
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().mechanical();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bMechanical;
				}
				break;
			case 9:
				// exp
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().explosives();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bExplosive;
				}
				break;
			case 10:
				// med
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().medical();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bMedical;
				}
				break;
		}

		sgp_swprintf(sString, std::size(sString), L"%d", iStat );

		// right justify
		FindFontRightCoordinates(	PERS_STAT_LOWEST_X, 0 ,PERS_STAT_LOWEST_WIDTH, 0 , sString, FONT10ARIAL , &sX, &sY );
		mprintf( sX, PERS_STAT_AVG_Y + ( iCounter + 1 ) * ( GetFontHeight( FONT10ARIAL ) + 3 ), L"%s", sString );
	}
}


void DisplayHighestStatValuesForCurrentTeam( void )
{
	// will display the average values for stats for the current team
	INT16 sX, sY;
	INT32 iCounter = 0;
	CHAR16 sString[ 32 ];
	INT32 iStat = 0;
	SoldierID iId;
	INT32 iDepartedId = -1;
	const bool currentTeamMode = fCurrentTeamMode != FALSE;

	// set up font
	SetFont( FONT10ARIAL );
	SetFontBackground( FONT_BLACK );
	SetFontForeground( PERS_TEXT_FONT_COLOR );

	// display header

	// center
	FindFontCenterCoordinates( PERS_STAT_HIGHEST_X, 0 ,PERS_STAT_LOWEST_WIDTH, 0 , pPersonnelCurrentTeamStatsStrings[ 2 ], FONT10ARIAL , &sX, &sY );

	mprintf( sX, PERS_STAT_AVG_Y, L"%s", pPersonnelCurrentTeamStatsStrings[ 2 ] );

	// nobody on team leave
	if (currentTeamMode) {
		if (currentTeamList.empty()) {
			return;
		}
	} else  {
	    if (GetNumberOfPastMercsOnPlayersTeam() == 0) {
			return;
		}
	}

	for( iCounter = 0; iCounter < 11; iCounter++ )
	{
		if ( currentTeamMode )
		{
			iId = GetIdOfMercWithHighestStat( iCounter );
			if ( iId == NOBODY )
				continue;
		}
		else
		{
			iDepartedId = GetIdOfDepartedMercWithHighestStat( iCounter );
			if ( iDepartedId == -1 )
				continue;
		}

		// even or odd?..color black or yellow?
		if( iCounter % 2 == 0 )
		{
			SetFontForeground( PERS_TEXT_FONT_ALTERNATE_COLOR );
		}
		else
		{
			SetFontForeground( PERS_TEXT_FONT_COLOR );
		}

		TacticalActor* currentSoldier = ResolveLaptopRosterActor(
			currentTeamMode, iId.i,
			[](UINT16 id)
			{
				return GetJa2SoldierRepository().resolve(id);
			});
		if ( currentTeamMode && !currentSoldier )
			continue;
		if (currentTeamMode)
		{
			sgp_swprintf(sString, std::size(sString), L"%s",
				currentSoldier->GetName() );
		}
		else
		{
			// get name
			sgp_swprintf(sString, std::size(sString), L"%s", gMercProfiles[ iDepartedId ].zNickname );
		}
		// print name
		mprintf( PERS_STAT_HIGHEST_X, PERS_STAT_AVG_Y + ( iCounter + 1 ) * ( GetFontHeight( FONT10ARIAL ) + 3 ), L"%s", sString );

		switch (iCounter) {
			case 0:
				// health
				if (currentTeamMode) {
					iStat =
						currentSoldier->vitals().maximumHealth();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bLife;
				}
				break;
			case 1:
				// agility
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().agility();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bAgility;
				}
				break;
			case 2:
				// dexterity
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().dexterity();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bDexterity;
				}
				break;
			case 3:
				// strength
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().strength();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bStrength;
				}
				break;
			case 4:
				// leadership
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().leadership();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bLeadership;
				}
				break;
			case 5:
				// wisdom
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().wisdom();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bWisdom;
				}
				break;
			case 6:
				// exper
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().experienceLevel();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bExpLevel;
				}
				break;
			case 7:
				//mrkmanship
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().marksmanship();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bMarksmanship;
				}
				break;
			case 8:
				// mech
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().mechanical();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bMechanical;
				}
				break;
			case 9:
				// exp
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().explosives();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bExplosive;
				}
				break;
			case 10:
				// med
				if (currentTeamMode) {
					iStat = currentSoldier->statistics().medical();
				} else {
					iStat =	gMercProfiles[ iDepartedId ] . bMedical;
				}
				break;
		}

		sgp_swprintf(sString, std::size(sString), L"%d", iStat );

		// right justify
		FindFontRightCoordinates(	PERS_STAT_HIGHEST_X, 0 ,PERS_STAT_LOWEST_WIDTH, 0 , sString, FONT10ARIAL , &sX, &sY );
		mprintf( sX, PERS_STAT_AVG_Y + ( iCounter + 1 ) * ( GetFontHeight( FONT10ARIAL ) + 3 ), L"%s", sString );
	}
}



void DisplayPersonnelTeamStats( void )
{
	// displays the stat title for each row in the team stat list
	INT32 iCounter =0;


	// set up font
	SetFont( FONT10ARIAL );
	SetFontBackground( FONT_BLACK );
	SetFontForeground( FONT_WHITE );


	// display titles for each row
	for( iCounter = 0; iCounter < 11; iCounter++ )
	{
			// even or odd?..color black or yellow?
		if( iCounter % 2 == 0 )
		{
			SetFontForeground( PERS_TEXT_FONT_ALTERNATE_COLOR );
		}
		else
		{
			SetFontForeground( PERS_TEXT_FONT_COLOR );
		}

		mprintf( PERS_STAT_LIST_X, PERS_STAT_AVG_Y + ( iCounter + 1 ) * ( GetFontHeight( FONT10ARIAL ) + 3 ), L"%s", pPersonnelTeamStatsStrings[ iCounter ] );
	}


	return;
}


INT32 GetNumberOfPastMercsOnPlayersTeam( void )
{
	return static_cast<INT32>(gDepartedRoster.size());
}


void InitPastCharactersList( void )
{
	// inits the past characters list
	memset( &LaptopSaveInfo.ubDeadCharactersList, -1, sizeof( LaptopSaveInfo.ubDeadCharactersList ) );
	memset( &LaptopSaveInfo.ubLeftCharactersList, -1, sizeof( LaptopSaveInfo.ubLeftCharactersList ) );
	memset( &LaptopSaveInfo.ubOtherCharactersList, -1, sizeof( LaptopSaveInfo.ubOtherCharactersList ) );
	RefreshDepartedRoster();
}


INT32 GetNumberOfDeadOnPastTeam( void )
{

	return static_cast<INT32>(std::count_if(gDepartedRoster.begin(),
		gDepartedRoster.end(), [](const auto& entry)
		{
			return entry.state == PersonnelRosterModel::DepartedState::Dead;
		}));

}


INT32 GetNumberOfLeftOnPastTeam( void )
{

	return static_cast<INT32>(std::count_if(gDepartedRoster.begin(),
		gDepartedRoster.end(), [](const auto& entry)
		{
			return entry.state == PersonnelRosterModel::DepartedState::Fired;
		}));

}


INT32 GetNumberOfOtherOnPastTeam( void )
{

	return static_cast<INT32>(std::count_if(gDepartedRoster.begin(),
		gDepartedRoster.end(), [](const auto& entry)
		{
			return entry.state == PersonnelRosterModel::DepartedState::Other;
		}));

}


void DisplayStateOfPastTeamMembers( void )
{
	INT16 sX, sY;
	CHAR16 sString[ 32 ];

	// font stuff
	SetFont( FONT10ARIAL );
	SetFontBackground( FONT_BLACK );
	SetFontForeground( PERS_TEXT_FONT_COLOR );


	// display numbers fired, dead and othered
	if (!fCurrentTeamMode) {
		// dead
		mprintf(	PERS_CURR_TEAM_COST_X, PERS_CURR_TEAM_COST_Y, L"%s", pPersonelTeamStrings[ 5 ] );
		sgp_swprintf(sString, std::size(sString), L"%d", GetNumberOfDeadOnPastTeam( ) );

		FindFontRightCoordinates((INT16)(PERS_CURR_TEAM_COST_X),0,PERS_DEPART_TEAM_WIDTH,0,sString, PERS_FONT,	&sX, &sY);

		mprintf( sX ,PERS_CURR_TEAM_COST_Y, L"%s", sString );

		// fired
		mprintf(	PERS_CURR_TEAM_COST_X, PERS_CURR_TEAM_HIGHEST_Y, L"%s", pPersonelTeamStrings[ 6 ] );
		sgp_swprintf(sString, std::size(sString), L"%d", GetNumberOfLeftOnPastTeam( ) );

		FindFontRightCoordinates((INT16)(PERS_CURR_TEAM_COST_X),0,PERS_DEPART_TEAM_WIDTH,0,sString, PERS_FONT,	&sX, &sY);

		mprintf( sX ,PERS_CURR_TEAM_HIGHEST_Y, L"%s", sString );

		// other
		mprintf(	PERS_CURR_TEAM_COST_X, PERS_CURR_TEAM_LOWEST_Y, L"%s", pPersonelTeamStrings[ 7 ] );
		sgp_swprintf(sString, std::size(sString), L"%d", GetNumberOfOtherOnPastTeam( ) );

		FindFontRightCoordinates((INT16)(PERS_CURR_TEAM_COST_X),0,PERS_DEPART_TEAM_WIDTH,0,sString, PERS_FONT,	&sX, &sY);

		mprintf( sX ,PERS_CURR_TEAM_LOWEST_Y, L"%s", sString );
	}
}



BOOLEAN CreateCurrentDepartedMouseRegions(LaptopPageResourceOwner& owner)
{
	MSYS_DefineRegion(&gTogglePastCurrentTeam[ 0 ], PERS_TOGGLE_CUR_DEPART_X, PERS_TOGGLE_CUR_Y, PERS_TOGGLE_CUR_DEPART_X + PERS_TOGGLE_CUR_DEPART_WIDTH, PERS_TOGGLE_CUR_Y + PERS_TOGGLE_CUR_DEPART_HEIGHT,
		 MSYS_PRIORITY_HIGHEST - 3 ,CURSOR_LAPTOP_SCREEN, MSYS_NO_CALLBACK, PersonnelCurrentTeamCallback);
	if (!owner.addRegion(gTogglePastCurrentTeam[0])) return FALSE;
	MSYS_DefineRegion(&gTogglePastCurrentTeam[ 1 ], PERS_TOGGLE_CUR_DEPART_X, PERS_TOGGLE_DEPART_Y , PERS_TOGGLE_CUR_DEPART_X + PERS_TOGGLE_CUR_DEPART_WIDTH, PERS_TOGGLE_DEPART_Y + PERS_TOGGLE_CUR_DEPART_HEIGHT,
		 MSYS_PRIORITY_HIGHEST - 3,CURSOR_LAPTOP_SCREEN, MSYS_NO_CALLBACK, PersonnelDepartedTeamCallback);
	return owner.addRegion(gTogglePastCurrentTeam[1]) ? TRUE : FALSE;
}

INT32 GetTheStateOfDepartedMerc(INT32 profileId)
{
	const auto state = PersonnelRosterModel::FindDepartedState(
		LaptopSaveInfo.ubDeadCharactersList,
		LaptopSaveInfo.ubLeftCharactersList,
		LaptopSaveInfo.ubOtherCharactersList,
		profileId, static_cast<INT16>(-1), NUM_PROFILES);
	if (!state) return -1;
	if (*state == PersonnelRosterModel::DepartedState::Dead)
		return DEPARTED_DEAD;
	if (*state == PersonnelRosterModel::DepartedState::Fired)
		return DEPARTED_FIRED;
	return DEPARTED_OTHER;
}



void PersonnelCurrentTeamCallback( MOUSE_REGION * pRegion, INT32 iReason ) {
	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP) {
		fCurrentTeamMode = TRUE;
		gCurrentRosterCursor.reset(currentTeamList.size());
		SetPersonnelButtonStates();
		fReDrawScreenFlag = TRUE;
	} // if
}


void PersonnelDepartedTeamCallback( MOUSE_REGION * pRegion, INT32 iReason ) {
	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP) {
		fCurrentTeamMode = FALSE;
		RefreshDepartedRoster();
		gDepartedRosterCursor.reset(gDepartedRoster.size());
		//Switch the panel on the right to be the stat panel
		gubPersonnelInfoState = PERSONNEL_STAT_BTN;
		SetPersonnelButtonStates();
		fReDrawScreenFlag = TRUE;
	}
}



void CreateDestroyButtonsForDepartedTeamList( void )
{
	if (!fCurrentTeamMode && gPersonnelDepartedResources.empty()) {
		LaptopPageResourceOwner staged;
		if (!staged.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\departuresbuttons.sti", -1, 0, -1, 2, -1),
			giPersonnelButtonImage[4])) return;
		if (!staged.addButton(QuickCreateButton( giPersonnelButtonImage[4], PERS_DEPARTED_UP_X, PERS_DEPARTED_UP_Y,
											BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
											BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)DepartedUpCallBack),
			giPersonnelButton[4])) return;

		// right button
		if (!staged.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\departuresbuttons.sti", -1, 1, -1, 3, -1),
			giPersonnelButtonImage[5])) return;
		if (!staged.addButton(QuickCreateButton( giPersonnelButtonImage[5], PERS_DEPARTED_UP_X, PERS_DEPARTED_DOWN_Y,
											BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
											BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)DepartedDownCallBack),
			giPersonnelButton[5])) return;

		// set up cursors for these buttons
		SetButtonCursor( giPersonnelButton[ 4 ], CURSOR_LAPTOP_SCREEN);
		SetButtonCursor( giPersonnelButton[ 5 ], CURSOR_LAPTOP_SCREEN);

		gPersonnelDepartedResources = std::move(staged);
	}
	else if (fCurrentTeamMode && !gPersonnelDepartedResources.empty()) {
		gPersonnelDepartedResources.clear();
		fReDrawScreenFlag = TRUE;
	}
}


void DepartedUpCallBack(GUI_BUTTON *btn,INT32 reason)
{
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		if(!(btn->uiFlags & BUTTON_CLICKED_ON))
		{

		}
	btn->uiFlags|=(BUTTON_CLICKED_ON);
	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if(btn->uiFlags & BUTTON_CLICKED_ON) {
			btn->uiFlags&=~(BUTTON_CLICKED_ON);
			if (gDepartedRosterCursor.pageUp(gDepartedRoster.size())) {
				fReDrawScreenFlag = TRUE;
			}
		}
	}
}


void DepartedDownCallBack(GUI_BUTTON *btn,INT32 reason)
{
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		if(!(btn->uiFlags & BUTTON_CLICKED_ON))
		{

		}
	btn->uiFlags|=(BUTTON_CLICKED_ON);
	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if(btn->uiFlags & BUTTON_CLICKED_ON)
		{
			btn->uiFlags&=~(BUTTON_CLICKED_ON);
			if (gDepartedRosterCursor.pageDown(gDepartedRoster.size()))
			{
				fReDrawScreenFlag = TRUE;
			}
		}
	}
}


// Display the bounded current page from the flattened departed-roster view.
void DisplayPastMercsPortraits( void )
{
	if (fCurrentTeamMode) return;
	gDepartedRosterCursor.normalize(gDepartedRoster.size());
	const std::size_t first = gDepartedRosterCursor.first();
	const std::size_t end = std::min(first + MAX_MERCS_ON_SCREEN,
		gDepartedRoster.size());
	for (std::size_t rosterIndex = first; rosterIndex < end; ++rosterIndex)
	{
		const auto& entry = gDepartedRoster[rosterIndex];
		DisplayPortraitOfPastMerc(entry.profileId,
			static_cast<INT32>(rosterIndex - first),
			entry.state == PersonnelRosterModel::DepartedState::Dead,
			entry.state == PersonnelRosterModel::DepartedState::Fired,
			entry.state == PersonnelRosterModel::DepartedState::Other);
	}
}


void DisplayPortraitOfPastMerc( INT32 iId , INT32 iCounter, BOOLEAN fDead, BOOLEAN fFired, BOOLEAN fOther )
{
	if (!PersonnelRosterModel::IsValidProfileId(iId, NUM_PROFILES) ||
		iCounter < 0 || iCounter >= MAX_MERCS_ON_SCREEN) return;
	char sTemp[100];
	HVOBJECT hFaceHandle;
	VOBJECT_DESC	VObjectDesc;
	
	const char* directory = gMercProfiles[iId].Type == PROFILETYPE_IMP
		? IMP_SMALL_FACES_DIR : SMALL_FACES_DIR;
	snprintf(sTemp, sizeof(sTemp), "%s%02d.sti", directory,
		gMercProfiles[iId].ubFaceIndex);
	
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP(sTemp, VObjectDesc.ImageFile);
	UniqueVideoObjectHandle face = AddVideoObjectOwned(&VObjectDesc);
	if (!face) return;
	const UINT32 faceHandle = face.get();

	//Blt face to screen to
	GetVideoObject(&hFaceHandle, faceHandle);

	if( fDead ) {
		hFaceHandle->pShades[ 0 ]		= Create16BPPPaletteShaded( hFaceHandle->pPaletteEntry, DEAD_MERC_COLOR_RED, DEAD_MERC_COLOR_GREEN, DEAD_MERC_COLOR_BLUE, TRUE );
		//set the red pallete to the face
		SetObjectHandleShade(faceHandle, 0);
	}

	BltVideoObject(FRAME_BUFFER, hFaceHandle, 0,( INT16 ) ( SMALL_PORTRAIT_START_X+ ( iCounter % PERSONNEL_PORTRAIT_NUMBER_WIDTH ) * SMALL_PORT_WIDTH ), ( INT16 ) ( SMALL_PORTRAIT_START_Y + ( iCounter / PERSONNEL_PORTRAIT_NUMBER_WIDTH ) * SMALL_PORT_HEIGHT ), VO_BLT_SRCTRANSPARENCY,NULL);

}




void DisplayDepartedCharStats(INT32 iId, INT32 iSlot, INT32 iState)
{
	if (!PersonnelRosterModel::IsValidProfileId(iId, NUM_PROFILES)) return;
	INT32 iCounter=0;
	CHAR16 sString[50];
	INT16 sX, sY;
	UINT32 uiHits = 0;
	HVOBJECT hHandle;

		// font stuff
	SetFont( FONT10ARIAL );
	SetFontBackground( FONT_BLACK );
	SetFontForeground( PERS_TEXT_FONT_COLOR );

	// SANDRO - remove the regions
	for( INT8 i = 0; i < 13; i++ )
	{
		ClearPersonnelTraitRegion(i);
	}

	// display the stats for a char
	for(iCounter=0;iCounter <MAX_STATS; iCounter++)
	{
		switch(iCounter)
		{
		case 0:
			// health

			// dead?
			if( iState == 0 )
			{
			sgp_swprintf(sString, std::size(sString), L"%d/%d",0,gMercProfiles[iId].bLife);
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%d/%d",gMercProfiles[iId].bLife,gMercProfiles[iId].bLife);
			}

			mprintf((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter].y,L"%s", pPersonnelScreenStrings[iCounter]);
		FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
		mprintf(sX,pPersonnelScreenPoints[iCounter].y,L"%s", sString);
			break;
		case 1:
			// agility
		sgp_swprintf(sString, std::size(sString), L"%d",gMercProfiles[iId].bAgility);
		mprintf((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter].y,L"%s", pPersonnelScreenStrings[iCounter]);
		FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
		mprintf(sX,pPersonnelScreenPoints[iCounter].y,L"%s", sString);
			break;
		case 2:
		// dexterity
		sgp_swprintf(sString, std::size(sString), L"%d",gMercProfiles[iId].bDexterity);
		mprintf((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter].y,L"%s", pPersonnelScreenStrings[iCounter]);
		FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
		mprintf(sX,pPersonnelScreenPoints[iCounter].y,L"%s", sString);
			break;
		case 3:
		// strength
		sgp_swprintf(sString, std::size(sString), L"%d",gMercProfiles[iId].bStrength);
		mprintf((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter].y,L"%s", pPersonnelScreenStrings[iCounter]);
		FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
		mprintf(sX,pPersonnelScreenPoints[iCounter].y,L"%s", sString);
		break;
		case 4:
		// leadership
		sgp_swprintf(sString, std::size(sString), L"%d",gMercProfiles[iId].bLeadership);
		mprintf((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter].y,L"%s", pPersonnelScreenStrings[iCounter]);
		FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
		mprintf(sX,pPersonnelScreenPoints[iCounter].y,L"%s", sString);
		break;
		case 5:
		// wisdom
		sgp_swprintf(sString, std::size(sString), L"%d",gMercProfiles[iId].bWisdom);
		mprintf((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter].y,L"%s", pPersonnelScreenStrings[iCounter]);
		FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
		mprintf(sX,pPersonnelScreenPoints[iCounter].y,L"%s", sString);
		break;
		case 6:
		// exper
		sgp_swprintf(sString, std::size(sString), L"%d",gMercProfiles[iId].bExpLevel);
		mprintf((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter].y,L"%s", pPersonnelScreenStrings[iCounter]);
		FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
		mprintf(sX,pPersonnelScreenPoints[iCounter].y,L"%s", sString);
		break;
		case 7:
			//mrkmanship
		sgp_swprintf(sString, std::size(sString), L"%d",gMercProfiles[iId].bMarksmanship);
		mprintf((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter].y,L"%s", pPersonnelScreenStrings[iCounter]);
		FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
		mprintf(sX,pPersonnelScreenPoints[iCounter].y,L"%s", sString);
		break;
	 case 8:
		// mech
		sgp_swprintf(sString, std::size(sString), L"%d",gMercProfiles[iId].bMechanical);
		mprintf((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter].y,L"%s", pPersonnelScreenStrings[iCounter]);
		FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
		mprintf(sX,pPersonnelScreenPoints[iCounter].y,L"%s", sString);
		break;
	 case 9:
		// exp
		sgp_swprintf(sString, std::size(sString), L"%d",gMercProfiles[iId].bExplosive);
		mprintf((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter].y,L"%s", pPersonnelScreenStrings[iCounter]);
		FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
		mprintf(sX,pPersonnelScreenPoints[iCounter].y,L"%s", sString);
		break;
		case 10:
		// med
			mprintf((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter].y,L"%s", pPersonnelScreenStrings[iCounter]);

		sgp_swprintf(sString, std::size(sString), L"%d",gMercProfiles[iId].bMedical);


			FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
			mprintf(sX,pPersonnelScreenPoints[iCounter].y,L"%s", sString);
		break;


		case 14:
		// kills
			mprintf((INT16)(pPersonnelScreenPoints[20].x+(iSlot*TEXT_BOX_WIDTH)),(pPersonnelScreenPoints[20].y - 12),L"%s", pPersonnelScreenStrings[PRSNL_TXT_KILLS]);

			sgp_swprintf(sString, std::size(sString), L"%d",(gMercProfiles[iId].records.usKillsElites + gMercProfiles[iId].records.usKillsRegulars + gMercProfiles[iId].records.usKillsAdmins + gMercProfiles[iId].records.usKillsHostiles + gMercProfiles[iId].records.usKillsCreatures + gMercProfiles[iId].records.usKillsZombies + gMercProfiles[iId].records.usKillsTanks + gMercProfiles[iId].records.usKillsOthers));

			FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[20].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
			mprintf(sX,(pPersonnelScreenPoints[20].y - 12),L"%s", sString);

			GetVideoObject(&hHandle, guiQMark);
			BltVideoObject( FRAME_BUFFER, hHandle, 0,(pPersonnelScreenPoints[20].x + 148), ( pPersonnelScreenPoints[20].y - 13 ), VO_BLT_SRCTRANSPARENCY,NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(7);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[7], (UINT16)( pPersonnelScreenPoints[20].x + 147 ), (UINT16)( pPersonnelScreenPoints[20].y - 14 ),
							(UINT16)( pPersonnelScreenPoints[20].x + 166 ), (UINT16)(pPersonnelScreenPoints[20].y - 3), MSYS_PRIORITY_HIGH,
								MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(7);
			// Assign the text
			AssignPersonnelKillsHelpText( iId );

		break;
		case 15:
			// assists
			mprintf((INT16)(pPersonnelScreenPoints[21].x+(iSlot*TEXT_BOX_WIDTH)),(pPersonnelScreenPoints[21].y - 10),L"%s", pPersonnelScreenStrings[PRSNL_TXT_ASSISTS]);
			sgp_swprintf(sString, std::size(sString), L"%d",(gMercProfiles[iId].records.usAssistsMercs + gMercProfiles[iId].records.usAssistsMilitia + gMercProfiles[iId].records.usAssistsOthers));
			FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[21].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
			mprintf(sX,(pPersonnelScreenPoints[21].y - 10),L"%s", sString);
		
			GetVideoObject(&hHandle, guiQMark);
			BltVideoObject( FRAME_BUFFER, hHandle, 0,(pPersonnelScreenPoints[21].x + 148), ( pPersonnelScreenPoints[21].y - 11 ), VO_BLT_SRCTRANSPARENCY,NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(8);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[8], (UINT16)( pPersonnelScreenPoints[21].x + 147 ), (UINT16)( pPersonnelScreenPoints[21].y - 12 ),
							(UINT16)( pPersonnelScreenPoints[21].x + 166 ), (UINT16)(pPersonnelScreenPoints[21].y - 1), MSYS_PRIORITY_HIGH,
								MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(8);
			// Assign the text
			AssignPersonnelAssistsHelpText( iId );

		break;
		case 16:
			// shots/hits
			mprintf((INT16)(pPersonnelScreenPoints[22].x+(iSlot*TEXT_BOX_WIDTH)),(pPersonnelScreenPoints[22].y - 8),L"%s", pPersonnelScreenStrings[PRSNL_TXT_HIT_PERCENTAGE]);
			uiHits = ( UINT32 )gMercProfiles[iId].records.usShotsHit;
			uiHits *= 100;

			// check we have shot at least once
			if( (gMercProfiles[iId].records.usShotsFired + gMercProfiles[iId].records.usMissilesLaunched + gMercProfiles[iId].records.usGrenadesThrown + gMercProfiles[iId].records.usKnivesThrown + gMercProfiles[iId].records.usBladeAttacks + gMercProfiles[iId].records.usHtHAttacks) > 0 )
			{
				uiHits /= ( UINT32 )(gMercProfiles[iId].records.usShotsFired + gMercProfiles[iId].records.usMissilesLaunched + gMercProfiles[iId].records.usGrenadesThrown + gMercProfiles[iId].records.usKnivesThrown + gMercProfiles[iId].records.usBladeAttacks + gMercProfiles[iId].records.usHtHAttacks);
				if ( uiHits > 100 )
					uiHits = 100;
			}
			else
			{
				// no, set hit % to 0
				uiHits = 0;
			}


			sgp_swprintf(sString, std::size(sString), L"%d %%%%",uiHits);
			FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[22].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
			sX += StringPixLength( sSpecialCharacters[0],	PERS_FONT );
			mprintf(sX,(pPersonnelScreenPoints[22].y - 8),L"%s", sString);
			
			GetVideoObject(&hHandle, guiQMark);
			BltVideoObject( FRAME_BUFFER, hHandle, 0,(pPersonnelScreenPoints[22].x + 148), ( pPersonnelScreenPoints[22].y - 9 ), VO_BLT_SRCTRANSPARENCY,NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(9);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[9], (UINT16)( pPersonnelScreenPoints[22].x + 147 ), (UINT16)( pPersonnelScreenPoints[22].y - 10 ),
							(UINT16)( pPersonnelScreenPoints[22].x + 166 ), (UINT16)(pPersonnelScreenPoints[22].y + 1), MSYS_PRIORITY_HIGH,
								MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(9);
			// Assign the text
			AssignPersonnelHitPercentageHelpText( iId );

		break;
		case 17:
			// Achievements
			mprintf((INT16)(pPersonnelScreenPoints[23].x+(iSlot*TEXT_BOX_WIDTH)),(pPersonnelScreenPoints[23].y - 6),L"%s", pPersonnelScreenStrings[PRSNL_TXT_ACHIEVEMNTS]);
			sgp_swprintf(sString, std::size(sString), L"%d %%%%",CalculateMercsAchievementPercentage( iId ));
			FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[23].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
			sX += StringPixLength( sSpecialCharacters[0],	PERS_FONT );
			mprintf(sX,(pPersonnelScreenPoints[23].y - 6),L"%s", sString);
			
			GetVideoObject(&hHandle, guiQMark);
			BltVideoObject( FRAME_BUFFER, hHandle, 0,(pPersonnelScreenPoints[23].x + 148), ( pPersonnelScreenPoints[23].y - 7 ), VO_BLT_SRCTRANSPARENCY,NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(10);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[10], (UINT16)( pPersonnelScreenPoints[23].x + 147 ), (UINT16)( pPersonnelScreenPoints[23].y - 8 ),
							(UINT16)( pPersonnelScreenPoints[23].x + 166 ), (UINT16)(pPersonnelScreenPoints[23].y + 3), MSYS_PRIORITY_HIGH,
								MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(10);
			// Assign the text
			AssignPersonnelAchievementsHelpText( iId );

		break;
		case 18:
			// battles
			mprintf((INT16)(pPersonnelScreenPoints[24].x+(iSlot*TEXT_BOX_WIDTH)),(pPersonnelScreenPoints[24].y - 4),L"%s", pPersonnelScreenStrings[PRSNL_TXT_BATTLES]);
			sgp_swprintf(sString, std::size(sString), L"%d",(gMercProfiles[iId].records.usBattlesTactical + gMercProfiles[iId].records.usBattlesAutoresolve));
			FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[24].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
			mprintf(sX,(pPersonnelScreenPoints[24].y - 4),L"%s", sString);
			
			GetVideoObject(&hHandle, guiQMark);
			BltVideoObject( FRAME_BUFFER, hHandle, 0,(pPersonnelScreenPoints[24].x + 148), ( pPersonnelScreenPoints[24].y - 5 ), VO_BLT_SRCTRANSPARENCY,NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(11);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[11], (UINT16)( pPersonnelScreenPoints[24].x + 147 ), (UINT16)( pPersonnelScreenPoints[24].y - 6 ),
							(UINT16)( pPersonnelScreenPoints[24].x + 166 ), (UINT16)(pPersonnelScreenPoints[24].y + 5), MSYS_PRIORITY_HIGH,
								MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(11);
			// Assign the text
			AssignPersonnelBattlesHelpText( iId );

		break;
		case 19:
			// wounds
			mprintf((INT16)(pPersonnelScreenPoints[25].x+(iSlot*TEXT_BOX_WIDTH)),(pPersonnelScreenPoints[25].y - 2),L"%s", pPersonnelScreenStrings[PRSNL_TXT_TIMES_WOUNDED]);
			sgp_swprintf(sString, std::size(sString), L"%d",(gMercProfiles[iId].records.usTimesWoundedShot + gMercProfiles[iId].records.usTimesWoundedStabbed + (gMercProfiles[iId].records.usTimesWoundedPunched/2) + gMercProfiles[iId].records.usTimesWoundedBlasted));
			FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[25].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
			mprintf(sX,(pPersonnelScreenPoints[25].y - 2),L"%s", sString);
			
			GetVideoObject(&hHandle, guiQMark);
			BltVideoObject( FRAME_BUFFER, hHandle, 0,(pPersonnelScreenPoints[25].x + 148), ( pPersonnelScreenPoints[25].y - 3 ), VO_BLT_SRCTRANSPARENCY,NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(12);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[12], (UINT16)( pPersonnelScreenPoints[25].x + 147 ), (UINT16)( pPersonnelScreenPoints[25].y - 4 ),
							(UINT16)( pPersonnelScreenPoints[25].x + 166 ), (UINT16)(pPersonnelScreenPoints[25].y + 7), MSYS_PRIORITY_HIGH,
								MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(12);
			// Assign the text
			AssignPersonnelWoundsHelpText( iId );

		break;
		}
	}

	return;
}


void EnableDisableDeparturesButtons( void )
{
	// will enable or disable departures buttons based on upperleft picutre index value
	if (fCurrentTeamMode || fNewMailFlag) {
		return;
	}

	// disable both buttons
	DisableButton( giPersonnelButton[ 4 ] );
	DisableButton( giPersonnelButton[ 5 ] );

	if (gDepartedRosterCursor.canPageUp()) {
		// enable up button
		EnableButton( giPersonnelButton[ 4 ] );
	}
	if (gDepartedRosterCursor.canPageDown(gDepartedRoster.size())) {
		// enable down button
		EnableButton( giPersonnelButton[ 5 ] );
	}
}


void DisplayDepartedCharName( INT32 iId, INT32 iSlot, INT32 iState )
{
	// get merc's nickName, assignment, and sector location info
	INT16 sX, sY;
	CHAR16 sString[ 32 ];

	SetFont(CHAR_NAME_FONT);
	SetFontForeground(PERS_TEXT_FONT_COLOR);
	SetFontBackground(FONT_BLACK);

	if (iState < 0 ||
		!PersonnelRosterModel::IsValidProfileId(iId, NUM_PROFILES))
	{
		return;
	}

	sgp_swprintf(sString, std::size(sString), L"%s", gMercProfiles[ iId ].zNickname );

		// nick name - assignment
	FindFontCenterCoordinates(IMAGE_BOX_X-5,0,IMAGE_BOX_WIDTH + 90 , 0,sString,CHAR_NAME_FONT, &sX, &sY );

	// cehck to se eif we are going to go off the left edge
	if( sX < pPersonnelScreenPoints[ 0 ].x )
	{
		sX = ( INT16 )pPersonnelScreenPoints[ 0 ].x;
	}

	mprintf( sX + iSlot * IMAGE_BOX_WIDTH, CHAR_NAME_Y, L"%s", sString );


	// state
	if( gMercProfiles[ iId ].ubMiscFlags2 & PROFILE_MISC_FLAG2_MARRIED_TO_HICKS )
	{
		//displaye 'married'
		sgp_swprintf(sString, std::size(sString), L"%s", pPersonnelDepartedStateStrings[ DEPARTED_MARRIED ] );
	}
	else if(	iState == DEPARTED_DEAD )
	{
		sgp_swprintf(sString, std::size(sString), L"%s", pPersonnelDepartedStateStrings[ DEPARTED_DEAD ] );
	}
	//if the merc is an AIM merc
	//else if( iId < BIFF )
	else if ( gMercProfiles[iId].Type == PROFILETYPE_AIM )
	{
		//if dismissed
		if( iState == DEPARTED_FIRED )
			sgp_swprintf(sString, std::size(sString), L"%s", pPersonnelDepartedStateStrings[ DEPARTED_FIRED ] );
		else
			sgp_swprintf(sString, std::size(sString), L"%s", pPersonnelDepartedStateStrings[ DEPARTED_CONTRACT_EXPIRED ] );
	}

	//else if its a MERC merc
	//else if( iId >= BIFF && iId <= BUBBA )
	else if ( gMercProfiles[iId].Type == PROFILETYPE_MERC )
	{
		if( iState == DEPARTED_FIRED )
			sgp_swprintf(sString, std::size(sString), L"%s", pPersonnelDepartedStateStrings[ DEPARTED_FIRED ] );
		else
			sgp_swprintf(sString, std::size(sString), L"%s", pPersonnelDepartedStateStrings[ DEPARTED_QUIT ] );
	}
	//must be a RPC
	//else
	else if ( gMercProfiles[iId].Type == PROFILETYPE_RPC )
	{
		if( iState == DEPARTED_FIRED )
			sgp_swprintf(sString, std::size(sString), L"%s", pPersonnelDepartedStateStrings[ DEPARTED_FIRED ] );
		else
			sgp_swprintf(sString, std::size(sString), L"%s", pPersonnelDepartedStateStrings[ DEPARTED_QUIT ] );
	}

	// nick name - assignment
	FindFontCenterCoordinates(IMAGE_BOX_X-5,0,IMAGE_BOX_WIDTH + 90 , 0,sString,CHAR_NAME_FONT, &sX, &sY );

	// cehck to se eif we are going to go off the left edge
	if( sX < pPersonnelScreenPoints[ 0 ].x )
	{
		sX = ( INT16 )pPersonnelScreenPoints[ 0 ].x;
	}

	mprintf( sX + iSlot * IMAGE_BOX_WIDTH, CHAR_NAME_Y + 10 , L"%s", sString );
}


void DisplayPersonnelTextOnTitleBar( void )
{
	// draw email screen title text

	// font stuff
	SetFont( FONT14ARIAL );
	SetFontForeground( FONT_WHITE );
	SetFontBackground( FONT_BLACK );

	// printf the title
	mprintf( PERS_TITLE_X, PERS_TITLE_Y, L"%s", pPersTitleText[0] );

	// reset the shadow

}

BOOLEAN DisplayHighLightBox( void )
{
	const auto& cursor = fCurrentTeamMode
		? gCurrentRosterCursor : gDepartedRosterCursor;
	if (!cursor.hasSelection() || cursor.visibleSlot() >= MAX_MERCS_ON_SCREEN)
		return FALSE;

	// bounding
	VOBJECT_DESC VObjectDesc;
	HVOBJECT hHandle;
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\PicBorde.sti", VObjectDesc.ImageFile);
	UniqueVideoObjectHandle box = AddVideoObjectOwned(&VObjectDesc);
	if (!box) return FALSE;
	const UINT32 uiBox = box.get();

	// blit it
	GetVideoObject(&hHandle, uiBox);
	const std::size_t slot = cursor.visibleSlot();
	BltVideoObject(FRAME_BUFFER, hHandle, 0,
		(INT16)(SMALL_PORTRAIT_START_X +
			(slot % PERSONNEL_PORTRAIT_NUMBER_WIDTH) * SMALL_PORT_WIDTH - 2),
		(INT16)(SMALL_PORTRAIT_START_Y +
			(slot / PERSONNEL_PORTRAIT_NUMBER_WIDTH) * SMALL_PORT_HEIGHT - 3),
		VO_BLT_SRCTRANSPARENCY, NULL);

	return ( TRUE );
}

// add to dead list
void AddCharacterToDeadList( TacticalActor *pSoldier )
{
	if (!pSoldier) return;
	PersonnelRosterModel::MoveDepartedProfile(
		LaptopSaveInfo.ubDeadCharactersList,
		LaptopSaveInfo.ubLeftCharactersList,
		LaptopSaveInfo.ubOtherCharactersList,
		static_cast<INT16>(pSoldier->identity().profile()),
		PersonnelRosterModel::DepartedState::Dead,
		static_cast<INT16>(-1), NUM_PROFILES);
	RefreshDepartedRoster();
}


void AddCharacterToFiredList( TacticalActor *pSoldier )
{
	if (!pSoldier) return;
	PersonnelRosterModel::MoveDepartedProfile(
		LaptopSaveInfo.ubDeadCharactersList,
		LaptopSaveInfo.ubLeftCharactersList,
		LaptopSaveInfo.ubOtherCharactersList,
		static_cast<INT16>(pSoldier->identity().profile()),
		PersonnelRosterModel::DepartedState::Fired,
		static_cast<INT16>(-1), NUM_PROFILES);
	RefreshDepartedRoster();
}

void AddCharacterToOtherList( TacticalActor *pSoldier )
{
	if (!pSoldier) return;
	PersonnelRosterModel::MoveDepartedProfile(
		LaptopSaveInfo.ubDeadCharactersList,
		LaptopSaveInfo.ubLeftCharactersList,
		LaptopSaveInfo.ubOtherCharactersList,
		static_cast<INT16>(pSoldier->identity().profile()),
		PersonnelRosterModel::DepartedState::Other,
		static_cast<INT16>(-1), NUM_PROFILES);
	RefreshDepartedRoster();
}


// If you have hired a merc before, then the they left for whatever reason, and now you are hiring them again,
// we must get rid of them from the departed section in the personnel screen.	( wouldnt make sense for them
//to be on your team list, and departed list )
BOOLEAN RemoveNewlyHiredMercFromPersonnelDepartedList( UINT8 ubProfile )
{
	const bool removed = PersonnelRosterModel::RemoveDepartedProfile(
		LaptopSaveInfo.ubDeadCharactersList,
		LaptopSaveInfo.ubLeftCharactersList,
		LaptopSaveInfo.ubOtherCharactersList,
		static_cast<INT16>(ubProfile), static_cast<INT16>(-1));
	RefreshDepartedRoster();
	return removed ? TRUE : FALSE;
}

// grab the id of the first merc being displayed
INT32 GetIdOfFirstDisplayedMerc( )
{
	if (fCurrentTeamMode) {
		return gCurrentRosterCursor.first() < currentTeamList.size()
			? currentTeamList[gCurrentRosterCursor.first()].i : -1;
	} else {
		return gDepartedRosterCursor.first() < gDepartedRoster.size()
			? gDepartedRoster[gDepartedRosterCursor.first()].profileId : -1;
	}
}


BOOLEAN RenderAtmPanel( void )
{

	VOBJECT_DESC VObjectDesc;
	HVOBJECT hHandle;

	// just show basic panel
	// bounding
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\AtmButtons.sti", VObjectDesc.ImageFile);
	UniqueVideoObjectHandle box = AddVideoObjectOwned(&VObjectDesc);
	if (!box) return FALSE;
	const UINT32 uiBox = box.get();

	GetVideoObject(&hHandle, uiBox);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,( INT16 ) ( ATM_UL_X ), ( INT16 ) ( ATM_UL_Y ), VO_BLT_SRCTRANSPARENCY,NULL);

	// blit it
	GetVideoObject(&hHandle, uiBox);
	BltVideoObject(FRAME_BUFFER, hHandle, 1,( INT16 ) ( ATM_UL_X + 1 ), ( INT16 ) ( ATM_UL_Y + 18), VO_BLT_SRCTRANSPARENCY,NULL);


	//DisplayAmountOnCurrentMerc( );

	// create destroy
	CreateDestroyStartATMButton( );

	return( TRUE );
}

void CreateDestroyStartATMButton( void )
{
	if (showPersonnelButtons && gPersonnelAtmResources.empty())
	{
		LaptopPageResourceOwner staged;
		INT16 x = iScreenWidthOffset + 519;
		INT16 y = iScreenHeightOffset + 74;

		for ( int i = 0; i < PERSONNEL_NUM_BTN; ++i )
		{
			if (!staged.addButtonImage(LoadButtonImageOwned(
				"LAPTOP\\AtmButtons.sti", -1, 2, -1, 3, -1),
				giPersonnelATMStartButtonImage[i])) return;
			if (!staged.addButton(QuickCreateButton(
				giPersonnelATMStartButtonImage[i], x, y,
														  BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
														  MSYS_NO_CALLBACK, (GUI_CALLBACK)PersonnelDataButtonCallback),
				giPersonnelATMStartButton[i])) return;

			// set text and what not
			SpecifyButtonText( giPersonnelATMStartButton[i], gsAtmStartButtonText[i] );
			SpecifyButtonUpTextColors( giPersonnelATMStartButton[i], FONT_BLACK, FONT_BLACK );
			SpecifyButtonFont( giPersonnelATMStartButton[i], PERS_FONT );
			SetButtonCursor( giPersonnelATMStartButton[i], CURSOR_LAPTOP_SCREEN );

			y += 24;
		}
		gPersonnelAtmResources = std::move(staged);
	}
	else if (!showPersonnelButtons && !gPersonnelAtmResources.empty())
	{
		gPersonnelAtmResources.clear();
	}
}

void FindPositionOfPersInvSlider( void )
{
	const std::size_t itemCount = static_cast<std::size_t>(
		std::max(GetNumberOfInventoryItemsOnCurrentMerc(), 0));
	guiSliderPosition = static_cast<UINT32>(
		PersonnelRosterModel::SliderPosition(uiCurrentInventoryIndex,
			itemCount, NUMBER_OF_INVENTORY_PERSONNEL,
			Y_SIZE_OF_PERSONNEL_SCROLL_REGION - SIZE_OF_PERSONNEL_CURSOR));
}


void HandleSliderBarClickCallback( MOUSE_REGION *pRegion, INT32 iReason )
{
	if( ( iReason & MSYS_CALLBACK_REASON_LBUTTON_DWN ) || ( iReason & MSYS_CALLBACK_REASON_LBUTTON_REPEAT ) )
	{
		const std::size_t itemCount = static_cast<std::size_t>(
			std::max(GetNumberOfInventoryItemsOnCurrentMerc(), 0));
		const INT32 relativeY = std::max<INT32>(
			static_cast<INT32>(gusMouseYPos) - Y_OF_PERSONNEL_SCROLL_REGION, 0);
		const std::size_t currentItemValue =
			PersonnelRosterModel::WindowStartFromSlider(
				static_cast<std::size_t>(relativeY), itemCount,
				NUMBER_OF_INVENTORY_PERSONNEL,
				Y_SIZE_OF_PERSONNEL_SCROLL_REGION - SIZE_OF_PERSONNEL_CURSOR);

		if( uiCurrentInventoryIndex != currentItemValue )
		{
			uiCurrentInventoryIndex = static_cast<UINT8>(currentItemValue);
			FindPositionOfPersInvSlider();
			fReDrawScreenFlag = TRUE;
		}
	}
}

void RenderSliderBarForPersonnelInventory( void )
{
	HVOBJECT hHandle;

	// render slider bar for personnel
	GetVideoObject(&hHandle, guiPersonnelInventory );
	BltVideoObject( FRAME_BUFFER, hHandle, 5,( INT16 ) ( X_OF_PERSONNEL_SCROLL_REGION ), ( INT16 ) ( guiSliderPosition + Y_OF_PERSONNEL_SCROLL_REGION), VO_BLT_SRCTRANSPARENCY,NULL);
}

void PersonnelDataButtonCallback( GUI_BUTTON *btn, INT32 reason )
{
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		UINT32	uiButton = btn->IDNum;

		fReDrawScreenFlag=TRUE;

		for ( int i = 0; i < PERSONNEL_NUM_BTN; ++i )
		{
			ButtonList[giPersonnelATMStartButton[i]]->uiFlags &= ~(BUTTON_CLICKED_ON);

			if ( uiButton == giPersonnelATMStartButton[i] )
				gubPersonnelInfoState = i;
		}
		
		btn->uiFlags |= BUTTON_CLICKED_ON;
	}
}

INT32 GetFundsOnMerc( TacticalActor *pSoldier )
{
	std::uint64_t currentAmount = 0;
	// run through mercs pockets, if any money in them, add to total

	// error check
	if( pSoldier == NULL )
	{
		return 0;
	}

	// run through grunts pockets and count all the spare change
	const std::size_t invsize = pSoldier->inventory().size();
	for (std::size_t currentPocket = 0; currentPocket < invsize;
		++currentPocket)
	{
		const auto& object = pSoldier->inventory()[currentPocket];
		if (!object.exists() || !PersonnelRosterModel::IsValidIndex(
			object.usItem, MAXITEMS) ||
			Item[object.usItem].usItemClass != IC_MONEY) continue;
		currentAmount += object[0]->data.money.uiMoneyAmount;
	}

	return PersonnelRosterModel::ClampCurrency(currentAmount);
}

void UpDateStateOfStartButton( void )
{
	if (!showPersonnelButtons || gPersonnelAtmResources.empty() ||
		gubPersonnelInfoState >= PERSONNEL_NUM_BTN)
		return;

	for ( int i = 0; i < PERSONNEL_NUM_BTN; ++i )
		ButtonList[giPersonnelATMStartButton[i]]->uiFlags &= ~(BUTTON_CLICKED_ON);

	ButtonList[giPersonnelATMStartButton[gubPersonnelInfoState]]->uiFlags |= BUTTON_CLICKED_ON;

	// if in current mercs and the currently selected guy is valid, enable button, else disable it
	if (fCurrentTeamMode && gCurrentRosterCursor.hasSelection() &&
		gCurrentRosterCursor.selected() < currentTeamList.size())
	{
		for ( int i = 0; i < PERSONNEL_NUM_BTN; ++i )
			EnableButton( giPersonnelATMStartButton[ i ] );

		SoldierID iId = currentTeamList[gCurrentRosterCursor.selected()];
		TacticalActor* soldier =
			GetJa2SoldierRepository().resolve(iId.i);

		if (iId != NOBODY && soldier &&
			soldier->assignment().current() == ASSIGNMENT_POW )
		{
			DisableButton( giPersonnelATMStartButton[ PERSONNEL_INV_BTN ] );

			if ( gubPersonnelInfoState == PERSONNEL_INV_BTN )
			{
				gubPersonnelInfoState = PERSONNEL_STAT_BTN;
				fPausedReDrawScreenFlag = TRUE;
			}
		}
	}
	else
	{
		// disable button
		for ( int i = 0; i < PERSONNEL_NUM_BTN; ++i )
			DisableButton( giPersonnelATMStartButton[i] );
	}
}

void DisplayAmountOnCurrentMerc( void )
{
	// will display the amount that the merc is carrying on him or herself
	TacticalActor *pSoldier = NULL;
	std::wstring sString{};
	INT16 sX, sY;

	if (!fCurrentTeamMode || !gCurrentRosterCursor.hasSelection() ||
		gCurrentRosterCursor.selected() >= currentTeamList.size()) {
		pSoldier = NULL;
	} else {
		// set soldier
		pSoldier = GetJa2SoldierRepository().resolve(
			currentTeamList[gCurrentRosterCursor.selected()].i);
	}

	sString = FormatMoney( GetFundsOnMerc( pSoldier ) );

	// set font
	SetFont( ATM_FONT );

	// set back and foreground
	SetFontForeground( FONT_WHITE );
	SetFontBackground( FONT_BLACK );

	// right justify
	FindFontRightCoordinates( ATM_DISPLAY_X, ATM_DISPLAY_Y, ATM_DISPLAY_WIDTH, ATM_DISPLAY_HEIGHT, sString.data(), ATM_FONT, &sX, &sY);

	// print string
	mprintf(sX, sY, L"%s", sString.c_str());
}

void HandlePersonnelKeyboard( void )
{
	INT32 iCounter = 0;
	INT32 iValue = 0;
	CHAR16 sZero[ 2 ] = L"0";

	InputAtom					InputEvent;
	POINT	MousePos;

	MousePos.x = gusMouseXPos;
	MousePos.y = gusMouseYPos;

	//while (DequeueSpecificEvent(&InputEvent, KEY_DOWN|KEY_UP|KEY_REPEAT))
	while (DequeueEvent(&InputEvent) == TRUE)
	{
		if ( InputEvent.usEvent == KEY_DOWN )
		{
			switch (InputEvent.usParam)
			{
				case UPARROW:
				case 'w':
					if ( fCurrentTeamMode )
					{
						if( gubPersonnelInfoState > PERSONNEL_STAT_BTN )
							gubPersonnelInfoState--;
						else
							gubPersonnelInfoState = PERSONNEL_NUM_BTN - 1;

						fReDrawScreenFlag = TRUE;

						uiCurrentInventoryIndex = 0;
						guiSliderPosition = 0;

						//if the selected merc is valid, and they are a POW, change to the inventory display
						TacticalActor* selectedSoldier =
							gCurrentRosterCursor.hasSelection() &&
							gCurrentRosterCursor.selected() < currentTeamList.size()
								? GetJa2SoldierRepository().resolve(currentTeamList[
									gCurrentRosterCursor.selected()].i) : NULL;
						if( selectedSoldier &&
							selectedSoldier->assignment().current() ==
								ASSIGNMENT_POW &&
							gubPersonnelInfoState == PERSONNEL_INV_BTN)
							gubPersonnelInfoState = PERSONNEL_EMPLOYMENT_BTN;

						fPausedReDrawScreenFlag = TRUE;
					}
				break;
				case DNARROW:
				case 's':
					if ( fCurrentTeamMode )
					{
						if( gubPersonnelInfoState < PERSONNEL_NUM_BTN-1 )
							gubPersonnelInfoState++;
						else
							gubPersonnelInfoState = PERSONNEL_STAT_BTN;

						fReDrawScreenFlag = TRUE;

						uiCurrentInventoryIndex = 0;
						guiSliderPosition = 0;

						//if the selected merc is valid, and they are a POW, change to the inventory display
						TacticalActor* selectedSoldier =
							gCurrentRosterCursor.hasSelection() &&
							gCurrentRosterCursor.selected() < currentTeamList.size()
								? GetJa2SoldierRepository().resolve(currentTeamList[
									gCurrentRosterCursor.selected()].i) : NULL;
						if( selectedSoldier &&
							selectedSoldier->assignment().current() ==
								ASSIGNMENT_POW &&
							gubPersonnelInfoState == PERSONNEL_INV_BTN)
							gubPersonnelInfoState = PERSONNEL_STAT_BTN;
						
						fPausedReDrawScreenFlag = TRUE;
					}
				break;
				case LEFTARROW:
				case 'a':
					fReDrawScreenFlag = TRUE;
					PrevPersonnelFace( );
					uiCurrentInventoryIndex = 0;
					guiSliderPosition = 0;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case RIGHTARROW:
				case 'd':
					fReDrawScreenFlag = TRUE;
					NextPersonnelFace( );
					uiCurrentInventoryIndex = 0;
					guiSliderPosition = 0;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case SHIFT_LEFTARROW:
				case 'A':
					fReDrawScreenFlag = TRUE;
					PrevPersonnelFacePage();
					uiCurrentInventoryIndex = 0;
					guiSliderPosition = 0;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case SHIFT_RIGHTARROW:
				case 'D':
					fReDrawScreenFlag = TRUE;
					NextPersonnelFacePage();
					uiCurrentInventoryIndex = 0;
					guiSliderPosition = 0;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case SHIFT_TAB:
					if ( !fCurrentTeamMode )
					{
						fCurrentTeamMode = TRUE;
						gCurrentRosterCursor.reset(currentTeamList.size());
					}
					else
					{
						fCurrentTeamMode = FALSE;
						RefreshDepartedRoster();
						gDepartedRosterCursor.reset(gDepartedRoster.size());
						//Switch the panel on the right to be the stat panel
						gubPersonnelInfoState = PERSONNEL_STAT_BTN;
					}
					SetPersonnelButtonStates();
					fReDrawScreenFlag = TRUE;
					fPausedReDrawScreenFlag = TRUE;
				break;
				default:
					HandleKeyBoardShortCutsForLapTop( InputEvent.usEvent, InputEvent.usParam, InputEvent.usKeyState );
			}
		}
		else if( InputEvent.usEvent == KEY_REPEAT )
		{
			switch (InputEvent.usParam)
			{
				case LEFTARROW:
				case 'a':
					fReDrawScreenFlag = TRUE;
					PrevPersonnelFace( );
					uiCurrentInventoryIndex = 0;
					guiSliderPosition = 0;
					fPausedReDrawScreenFlag = TRUE;
				break;
				case RIGHTARROW:
				case 'd':
					fReDrawScreenFlag = TRUE;
					NextPersonnelFace( );
					uiCurrentInventoryIndex = 0;
					guiSliderPosition = 0;
					fPausedReDrawScreenFlag = TRUE;
				break;
			}
		}
	}
}

void DisplayEmploymentinformation( SoldierID iId, INT32 iSlot )
{
	const CampaignMercenaryPolicy mercenaryPolicy(
		GetGameContext().capabilities());
	INT32 iCounter=0;
	CHAR16 sString[50];
	CHAR16 sStringA[ 50 ];
	INT16 sX, sY;
	UINT32 uiHits = 0;
	HVOBJECT hHandle;

	// SANDRO - remove the regions
	for( INT8 i = 0; i < 13; i++ )
	{
		ClearPersonnelTraitRegion(i);
	}

	TacticalActor *pSoldier =
		GetJa2SoldierRepository().resolve(iId.i);
	if ( !pSoldier )
		return;
	const MERCPROFILESTRUCT *pMercProfile = ProfileFor(pSoldier);
	if (!pMercProfile) return;
	const STRUCT_Records *pRecords = &pMercProfile->records;

	if( pSoldier->status().flags() & SOLDIER_VEHICLE )
	{
		return;
	}

	// display the stats for a char
	for ( iCounter = 0; iCounter < MAX_STATS; ++iCounter )
	{
		const INT32 x = pPersonnelScreenPoints[iCounter].x + (iSlot * TEXT_BOX_WIDTH);
		const INT32 y = pPersonnelScreenPoints[iCounter].y;

		switch(iCounter)
		{
		//Remaining Contract:
		case 0:
		{

			if ( mercenaryPolicy.usesUnfinishedBusinessRules() )
			{
				PersonnelRosterModel::CopyText(sString,
					gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION]);
				mprintf(x, y, L"%s", pPersonnelScreenStrings[PRSNL_TXT_CURRENT_CONTRACT]);
			}
			else
			{
			static const UINT32 uiMinutesInDay = 24 * 60;

			if ( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC ||
				mercenaryPolicy.isProfile(
					pSoldier->identity().profile(), CampaignProfileCode::Role::Slay) )
			{
				INT32 iTimeLeftOnContract = CalcTimeLeftOnMercContract( pSoldier );

				//if the merc is in transit
				if( pSoldier->assignment().current() == IN_TRANSIT )
				{
					//and if the time left on the contract is greater than the contract time
					if( iTimeLeftOnContract > (INT32)( pSoldier->employment().totalLength() * uiMinutesInDay ) )
					{
						iTimeLeftOnContract = ( pSoldier->employment().totalLength() * uiMinutesInDay );
					}
				}

				UINT32 days = iTimeLeftOnContract / uiMinutesInDay;
				UINT32 hours = (iTimeLeftOnContract % uiMinutesInDay) / 60;

				// if there is going to be a both days and hours left on the contract
				if( days > 0)
				{
					sgp_swprintf(sString, std::size(sString), L"%d%s %d%s / %d%s", days, gpStrategicString[STR_PB_DAYS_ABBREVIATION], hours, gpStrategicString[STR_PB_HOURS_ABBREVIATION], pSoldier->employment().totalLength(), gpStrategicString[STR_PB_DAYS_ABBREVIATION] );
					mprintf( x, y, L"%s", pPersonnelScreenStrings[PRSNL_TXT_CURRENT_CONTRACT] );
				}

				//else there is under a day left
				else
				{
					//DEF: removed 2/7/99
					sgp_swprintf(sString, std::size(sString), L"%d%s / %d%s", hours, gpStrategicString[ STR_PB_HOURS_ABBREVIATION ], pSoldier->employment().totalLength(), gpStrategicString[ STR_PB_DAYS_ABBREVIATION ]);
					mprintf( x, y, L"%s", pPersonnelScreenStrings[PRSNL_TXT_CURRENT_CONTRACT] );
				}

			}
			else
			{
				PersonnelRosterModel::CopyText(sString,
					gpStrategicString[STR_PB_NOTAPPLICABLE_ABBREVIATION]);
				mprintf( x, y, L"%s", pPersonnelScreenStrings[PRSNL_TXT_CURRENT_CONTRACT] );
			}
			}
			FindFontRightCoordinates( (INT16)(x + Prsnl_DATA_OffSetX), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
			mprintf( sX, y, L"%s", sString );
		}
		break;

//		case 11:
//		case 19:
		case 1:

			// total contract time served
			mprintf( x, y, L"%s", pPersonnelScreenStrings[PRSNL_TXT_TOTAL_SERVICE] );

			//./DEF 2/4/99: total service days used to be calced as 'days -1'

			sgp_swprintf(sString, std::size(sString), L"%d %s",pMercProfile->usTotalDaysServed, gpStrategicString[ STR_PB_DAYS_ABBREVIATION ] );

			FindFontRightCoordinates( (INT16)(x + Prsnl_DATA_OffSetX), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
			mprintf(sX,y,L"%s", sString);
		break;

//		case 13:
		case 3:
		// cost (PRSNL_TXT_TOTAL_COST)

/*
			if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC)
			{
				UINT32 uiDailyCost = 0;

				if( pSoldier->employment().lastContractType() == CONTRACT_EXTEND_2_WEEK )
				{
					// 2 week contract
					uiDailyCost = pMercProfile->uiBiWeeklySalary / 14;
				}
				else if( pSoldier->employment().lastContractType() == CONTRACT_EXTEND_1_WEEK )
				{
					// 1 week contract
					uiDailyCost = pMercProfile->uiWeeklySalary / 7;
				}
				else
				{
					uiDailyCost = pMercProfile->sSalary;
				}

//				sgp_swprintf(sString, std::size(sString), L"%d",uiDailyCost * pSoldier->employment().totalLength() );
				sgp_swprintf(sString, std::size(sString), L"%d", pMercProfile->uiTotalCostToDate );
			}
			else if( pSoldier->employment().mercenaryType() == MERC_TYPE__MERC)
			{
//					sgp_swprintf(sString, std::size(sString), L"%d",pMercProfile->sSalary * pMercProfile->iMercMercContractLength );
					sgp_swprintf(sString, std::size(sString), L"%d", pMercProfile->uiTotalCostToDate );
			}
			else
			{
				//Display a $0 amount
//				sgp_swprintf(sString, std::size(sString), L"%s", L"0" );

				sgp_swprintf(sString, std::size(sString), L"%d", pMercProfile->uiTotalCostToDate );
			}
*/
				sgp_swprintf(sString, std::size(sString), L"%s", FormatMoney(pMercProfile->uiTotalCostToDate).data() );

/*
DEF:3/19/99:
			if( pSoldier->employment().mercenaryType() == MERC_TYPE__MERC )
			{
			sgp_swprintf(sStringA, std::size(sStringA), L"%s", pPersonnelScreenStrings[ PRSNL_TXT_UNPAID_AMOUNT ] );
			}
			else
*/
			{
				sgp_swprintf(sStringA, std::size(sStringA), L"%s", pPersonnelScreenStrings[ PRSNL_TXT_TOTAL_COST ]	);
			}

			FindFontRightCoordinates( (INT16)(x + Prsnl_DATA_OffSetX), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
			mprintf( x, y, L"%s", sStringA );

			// print contract cost
			mprintf( sX, y, L"%s", sString );

			if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC)
			{
				if( pSoldier->employment().lastContractType() == CONTRACT_EXTEND_2_WEEK )
				{
					// 2 week contract
					sgp_swprintf(sString, std::size(sString), L"%s", FormatMoney( pMercProfile->uiBiWeeklySalary / 14 ).data() );
				}
				else if( pSoldier->employment().lastContractType() == CONTRACT_EXTEND_1_WEEK )
				{
					// 1 week contract
					sgp_swprintf(sString, std::size(sString), L"%s", FormatMoney( pMercProfile->uiWeeklySalary / 7 ).data() );
				}
				else
				{
					// daily rate
					sgp_swprintf(sString, std::size(sString), L"%s", FormatMoney( pMercProfile->sSalary ).data() );
				}
			}
			else
			{
				sgp_swprintf(sString, std::size(sString), L"%s", FormatMoney( pMercProfile->sSalary ).data() );
			}

			FindFontRightCoordinates( (INT16)(x + Prsnl_DATA_OffSetX), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );

			iCounter++;

			// now print daily rate
			mprintf( sX, pPersonnelScreenPoints[iCounter + 1].y, L"%s", sString );
			mprintf( pPersonnelScreenPoints[iCounter + 1].x + (iSlot * TEXT_BOX_WIDTH), pPersonnelScreenPoints[iCounter + 1].y, L"%s", pPersonnelScreenStrings[PRSNL_TXT_DAILY_COST] );

			break;

		case 5:
		// medical deposit

			//if its a merc merc, display the salary oweing
			if( pSoldier->employment().mercenaryType() == MERC_TYPE__MERC )
			{
				mprintf((INT16)(pPersonnelScreenPoints[iCounter-1].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter-1].y,L"%s", pPersonnelScreenStrings[PRSNL_TXT_UNPAID_AMOUNT]);

				sgp_swprintf(sString, std::size(sString), L"%s", FormatMoney( pMercProfile->sSalary * pMercProfile->iMercMercContractLength ).data() );

				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter-1].x+(iSlot*TEXT_BOX_WIDTH)+Prsnl_DATA_OffSetX),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
				mprintf(sX,pPersonnelScreenPoints[iCounter-1].y,L"%s", sString);
			}
			else
			{
				mprintf((INT16)(pPersonnelScreenPoints[iCounter-1].x+(iSlot*TEXT_BOX_WIDTH)),pPersonnelScreenPoints[iCounter-1].y,L"%s", pPersonnelScreenStrings[PRSNL_TXT_MED_DEPOSIT]);

				sgp_swprintf(sString, std::size(sString), L"%s", FormatMoney(pMercProfile->sMedicalDepositAmount).data());

				FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[iCounter-1].x+(iSlot*TEXT_BOX_WIDTH)+Prsnl_DATA_OffSetX),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
				mprintf(sX,pPersonnelScreenPoints[iCounter-1].y,L"%s", sString);
			}


		break;

		/////////////////////////////////////////////////////////////////////////////////////////////////
		// SANDRO - show num kills, hit percentage, times wounded etc. in here instead of stats table

		case 14:
		// kills
			mprintf((INT16)(pPersonnelScreenPoints[20].x+(iSlot*TEXT_BOX_WIDTH)),(pPersonnelScreenPoints[20].y - 12),L"%s", pPersonnelScreenStrings[PRSNL_TXT_KILLS]);

			sgp_swprintf(sString, std::size(sString), L"%d",(pRecords->usKillsElites + pRecords->usKillsRegulars + pRecords->usKillsAdmins + pRecords->usKillsHostiles + pRecords->usKillsCreatures + pRecords->usKillsZombies + pRecords->usKillsTanks + pRecords->usKillsOthers));

			FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[20].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
			mprintf(sX,(pPersonnelScreenPoints[20].y - 12),L"%s", sString);

			GetVideoObject(&hHandle, guiQMark);
			BltVideoObject( FRAME_BUFFER, hHandle, 0,(pPersonnelScreenPoints[20].x + 148), ( pPersonnelScreenPoints[20].y - 13 ), VO_BLT_SRCTRANSPARENCY,NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(7);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[7], (UINT16)( pPersonnelScreenPoints[20].x + 147 ), (UINT16)( pPersonnelScreenPoints[20].y - 14 ),
							(UINT16)( pPersonnelScreenPoints[20].x + 166 ), (UINT16)(pPersonnelScreenPoints[20].y - 3), MSYS_PRIORITY_HIGH,
								MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(7);
			// Assign the text
			AssignPersonnelKillsHelpText( pSoldier->identity().profile() );

		break;
		case 15:
			// assists
			mprintf((INT16)(pPersonnelScreenPoints[21].x+(iSlot*TEXT_BOX_WIDTH)),(pPersonnelScreenPoints[21].y - 10),L"%s", pPersonnelScreenStrings[PRSNL_TXT_ASSISTS]);
			sgp_swprintf(sString, std::size(sString), L"%d",(pRecords->usAssistsMercs + pRecords->usAssistsMilitia + pRecords->usAssistsOthers));
			FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[21].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
			mprintf(sX,(pPersonnelScreenPoints[21].y - 10),L"%s", sString);
		
			GetVideoObject(&hHandle, guiQMark);
			BltVideoObject( FRAME_BUFFER, hHandle, 0,(pPersonnelScreenPoints[21].x + 148), ( pPersonnelScreenPoints[21].y - 11 ), VO_BLT_SRCTRANSPARENCY,NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(8);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[8], (UINT16)( pPersonnelScreenPoints[21].x + 147 ), (UINT16)( pPersonnelScreenPoints[21].y - 12 ),
							(UINT16)( pPersonnelScreenPoints[21].x + 166 ), (UINT16)(pPersonnelScreenPoints[21].y - 1), MSYS_PRIORITY_HIGH,
								MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(8);
			// Assign the text
			AssignPersonnelAssistsHelpText( pSoldier->identity().profile() );

		break;
		case 16:
		{
			// shots/hits
			mprintf( (INT16)(pPersonnelScreenPoints[22].x + (iSlot * TEXT_BOX_WIDTH)), (pPersonnelScreenPoints[22].y - 8), L"%s", pPersonnelScreenStrings[PRSNL_TXT_HIT_PERCENTAGE] );
			uiHits = (UINT32)pRecords->usShotsHit;
			uiHits *= 100;

			// check we have shot at least once
			UINT32 uiAttacks = pRecords->usShotsFired + pRecords->usMissilesLaunched + pRecords->usGrenadesThrown + pRecords->usKnivesThrown + pRecords->usBladeAttacks + pRecords->usHtHAttacks;
			if ( uiAttacks > 0 )
			{
				uiHits /= uiAttacks;
				if ( uiHits > 100 )
					uiHits = 100;
			}
			else
			{
				// no, set hit % to 0
				uiHits = 0;
			}


			sgp_swprintf(sString, std::size(sString), L"%d %%%%", uiHits );
			FindFontRightCoordinates( (INT16)(pPersonnelScreenPoints[22].x + (iSlot * TEXT_BOX_WIDTH)), 0, TEXT_BOX_WIDTH - 20, 0, sString, PERS_FONT, &sX, &sY );
			sX += StringPixLength( sSpecialCharacters[0], PERS_FONT );
			mprintf( sX, (pPersonnelScreenPoints[22].y - 8), L"%s", sString );

			GetVideoObject( &hHandle, guiQMark );
			BltVideoObject( FRAME_BUFFER, hHandle, 0, (pPersonnelScreenPoints[22].x + 148), (pPersonnelScreenPoints[22].y - 9), VO_BLT_SRCTRANSPARENCY, NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(9);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[9], (UINT16)(pPersonnelScreenPoints[22].x + 147), (UINT16)(pPersonnelScreenPoints[22].y - 10),
				(UINT16)(pPersonnelScreenPoints[22].x + 166), (UINT16)(pPersonnelScreenPoints[22].y + 1), MSYS_PRIORITY_HIGH,
				MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(9);
			// Assign the text
			AssignPersonnelHitPercentageHelpText( pSoldier->identity().profile() );
		}
		break;
		case 17:
			// Achievements
			mprintf((INT16)(pPersonnelScreenPoints[23].x+(iSlot*TEXT_BOX_WIDTH)),(pPersonnelScreenPoints[23].y - 6),L"%s", pPersonnelScreenStrings[PRSNL_TXT_ACHIEVEMNTS]);
			sgp_swprintf(sString, std::size(sString), L"%d %%%%", CalculateMercsAchievementPercentage( pSoldier->identity().profile() ));
			FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[23].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
			sX += StringPixLength( sSpecialCharacters[0],	PERS_FONT );
			mprintf(sX,(pPersonnelScreenPoints[23].y - 6),L"%s", sString);
			
			GetVideoObject(&hHandle, guiQMark);
			BltVideoObject( FRAME_BUFFER, hHandle, 0,(pPersonnelScreenPoints[23].x + 148), ( pPersonnelScreenPoints[23].y - 7 ), VO_BLT_SRCTRANSPARENCY,NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(10);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[10], (UINT16)( pPersonnelScreenPoints[23].x + 147 ), (UINT16)( pPersonnelScreenPoints[23].y - 8 ),
							(UINT16)( pPersonnelScreenPoints[23].x + 166 ), (UINT16)(pPersonnelScreenPoints[23].y + 3), MSYS_PRIORITY_HIGH,
								MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(10);
			// Assign the text
			AssignPersonnelAchievementsHelpText( pSoldier->identity().profile() );

		break;
		case 18:
			// battles
			mprintf((INT16)(pPersonnelScreenPoints[24].x+(iSlot*TEXT_BOX_WIDTH)),(pPersonnelScreenPoints[24].y - 4),L"%s", pPersonnelScreenStrings[PRSNL_TXT_BATTLES]);
			sgp_swprintf(sString, std::size(sString), L"%d",(pRecords->usBattlesTactical + pRecords->usBattlesAutoresolve));
			FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[24].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
			mprintf(sX,(pPersonnelScreenPoints[24].y - 4),L"%s", sString);
			
			GetVideoObject(&hHandle, guiQMark);
			BltVideoObject( FRAME_BUFFER, hHandle, 0,(pPersonnelScreenPoints[24].x + 148), ( pPersonnelScreenPoints[24].y - 5 ), VO_BLT_SRCTRANSPARENCY,NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(11);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[11], (UINT16)( pPersonnelScreenPoints[24].x + 147 ), (UINT16)( pPersonnelScreenPoints[24].y - 6 ),
							(UINT16)( pPersonnelScreenPoints[24].x + 166 ), (UINT16)(pPersonnelScreenPoints[24].y + 5), MSYS_PRIORITY_HIGH,
								MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(11);
			// Assign the text
			AssignPersonnelBattlesHelpText( pSoldier->identity().profile() );

		break;
		case 19:
			// wounds
			mprintf((INT16)(pPersonnelScreenPoints[25].x+(iSlot*TEXT_BOX_WIDTH)),(pPersonnelScreenPoints[25].y - 2),L"%s", pPersonnelScreenStrings[PRSNL_TXT_TIMES_WOUNDED]);
			sgp_swprintf(sString, std::size(sString), L"%d",(pRecords->usTimesWoundedShot + pRecords->usTimesWoundedStabbed + (pRecords->usTimesWoundedPunched/2) + pRecords->usTimesWoundedBlasted));
			FindFontRightCoordinates((INT16)(pPersonnelScreenPoints[25].x+(iSlot*TEXT_BOX_WIDTH)),0,TEXT_BOX_WIDTH-20,0,sString, PERS_FONT,	&sX, &sY);
			mprintf(sX,(pPersonnelScreenPoints[25].y - 2),L"%s", sString);
			
			GetVideoObject(&hHandle, guiQMark);
			BltVideoObject( FRAME_BUFFER, hHandle, 0,(pPersonnelScreenPoints[25].x + 148), ( pPersonnelScreenPoints[25].y - 3 ), VO_BLT_SRCTRANSPARENCY,NULL );

			// Add specific region for fast help window
			ClearPersonnelTraitRegion(12);
			MSYS_DefineRegion( &gSkillTraitHelpTextRegion[12], (UINT16)( pPersonnelScreenPoints[25].x + 147 ), (UINT16)( pPersonnelScreenPoints[25].y - 4 ),
							(UINT16)( pPersonnelScreenPoints[25].x + 166 ), (UINT16)(pPersonnelScreenPoints[25].y + 7), MSYS_PRIORITY_HIGH,
								MSYS_NO_CURSOR, MSYS_NO_CALLBACK, NULL );
			PublishPersonnelTraitRegion(12);
			// Assign the text
			AssignPersonnelWoundsHelpText( pSoldier->identity().profile() );

		break;

		/////////////////////////////////////////////////////////////////////////////////////////////////
		}
	}
}

// AIM merc:	Returns the amount of time left on mercs contract
// MERC merc: Returns the amount of time the merc has worked
// IMP merc:	Returns the amount of time the merc has worked
// else:			returns -1
INT32 CalcTimeLeftOnMercContract( TacticalActor *pSoldier )
{
	INT32 iTimeLeftOnContract = -1;
	if (!pSoldier) return iTimeLeftOnContract;

	if(pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC)
	{
		iTimeLeftOnContract = pSoldier->employment().endTime()-GetWorldTotalMin();

		if( iTimeLeftOnContract < 0 )
			iTimeLeftOnContract = 0;
	}
	else if( pSoldier->employment().mercenaryType() == MERC_TYPE__MERC)
	{
		const MERCPROFILESTRUCT* profile = ProfileFor(pSoldier);
		if (profile) iTimeLeftOnContract = profile->iMercMercContractLength;
	}

	else if( pSoldier->employment().mercenaryType() == MERC_TYPE__PLAYER_CHARACTER )
	{
		iTimeLeftOnContract = pSoldier->employment().totalLength();
	}

	else
	{
		iTimeLeftOnContract = -1;
	}

	return( iTimeLeftOnContract );
}

// SANDRO - Popup text windows for traits
void AssignPersonnelSkillTraitHelpText(UINT8 ubTraitNumber,
	BOOLEAN fExpertLevel, BOOLEAN fRegMale, CHAR16 (&apStr)[5000])
{
	//CHAR16	apStr[ 5000 ];
	CHAR16	atStr[ 1500 ];

	if( gGameOptions.fNewTraitSystem )
	{
		switch( ubTraitNumber )
		{
			case AUTO_WEAPONS_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubAWBonusCtHAssaultRifles != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsAutoWeapons[0], ( gSkillTraitValues.ubAWBonusCtHAssaultRifles * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAWBonusCtHSMGs != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsAutoWeapons[1], ( gSkillTraitValues.ubAWBonusCtHSMGs * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAWBonusCtHLMGs != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsAutoWeapons[2], ( gSkillTraitValues.ubAWBonusCtHLMGs * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAWFiringSpeedBonusLMGs != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsAutoWeapons[3], ( gSkillTraitValues.ubAWFiringSpeedBonusLMGs * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAWPercentReadyLMGReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsAutoWeapons[4], ( gSkillTraitValues.ubAWPercentReadyLMGReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAWAutoFirePenaltyReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsAutoWeapons[5], ( gSkillTraitValues.ubAWAutoFirePenaltyReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAWUnwantedBulletsReduction > 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsAutoWeapons[6]);
					AppendPersonnelText( apStr, atStr );
				}
				// Flugente: focus is a skill that can be used by multiple traits. For simplicity, the ini values are in the sniper trait section
				if ( gSkillTraitValues.ubSNFocusRadius != 0 && gSkillTraitValues.sSNFocusInterruptBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[19], gSkillTraitValues.sSNFocusInterruptBonus );
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case HEAVY_WEAPONS_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubHWGrenadeLaunchersAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsHeavyWeapons[0], ( gSkillTraitValues.ubHWGrenadeLaunchersAPsReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubHWRocketLaunchersAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsHeavyWeapons[1], ( gSkillTraitValues.ubHWRocketLaunchersAPsReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubHWBonusCtHGrenadeLaunchers != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsHeavyWeapons[2], ( gSkillTraitValues.ubHWBonusCtHGrenadeLaunchers * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubHWBonusCtHRocketLaunchers != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsHeavyWeapons[3], ( gSkillTraitValues.ubHWBonusCtHRocketLaunchers * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubHWMortarAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsHeavyWeapons[4], ( gSkillTraitValues.ubHWMortarAPsReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubHWMortarCtHPenaltyReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsHeavyWeapons[5], ( gSkillTraitValues.ubHWMortarCtHPenaltyReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubHWDamageTanksBonusPercent != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsHeavyWeapons[6], ( gSkillTraitValues.ubHWDamageTanksBonusPercent * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubHWDamageBonusPercentForHW != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsHeavyWeapons[7], ( gSkillTraitValues.ubHWDamageBonusPercentForHW * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				// Flugente: focus is a skill that can be used by multiple traits. For simplicity, the ini values are in the sniper trait section
				if ( gSkillTraitValues.ubSNFocusRadius != 0 && gSkillTraitValues.sSNFocusInterruptBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[19], gSkillTraitValues.sSNFocusInterruptBonus );
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case SNIPER_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubSNBonusCtHRifles != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[0], ( gSkillTraitValues.ubSNBonusCtHRifles * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSNBonusCtHSniperRifles != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[1], ( gSkillTraitValues.ubSNBonusCtHSniperRifles * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSNEffRangeToTargetReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[2], ( gSkillTraitValues.ubSNEffRangeToTargetReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSNAimingBonusPerClick != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[3], ( gSkillTraitValues.ubSNAimingBonusPerClick * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSNDamageBonusPerClick != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[4], ( gSkillTraitValues.ubSNDamageBonusPerClick * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					if( gSkillTraitValues.ubSNDamageBonusFromNumClicks == 0)
					{
						AppendPersonnelText( apStr, gzIMPMajorTraitsHelpTextsSniper[5] );
						AppendPersonnelText( apStr, gzIMPMajorTraitsHelpTextsSniper[6] );
					}
					else if( gSkillTraitValues.ubSNDamageBonusFromNumClicks == 1 )
					{
						AppendPersonnelText( atStr, gzIMPMajorTraitsHelpTextsSniper[6] );
					}
					else
					{
						AppendPersonnelText( atStr, gzIMPMajorTraitsHelpTextsSniper[6] );
						AppendPersonnelText( atStr, gzIMPMajorTraitsHelpTextsSniper[gSkillTraitValues.ubSNDamageBonusFromNumClicks + 4] );
					}
					AppendPersonnelText( atStr, L"\n" );
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSNChamberRoundAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[14], ( gSkillTraitValues.ubSNChamberRoundAPsReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSNAimClicksAdded != 0 )
				{
					if( UsingNewCTHSystem() == true )
					{
						if( gSkillTraitValues.ubSNAimClicksAdded == 1 && !fExpertLevel )
							sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsSniper[17]);
						else
							sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[18], ( gSkillTraitValues.ubSNAimClicksAdded * (fExpertLevel ? 2 : 1)));
					}
					else
					{
						if( gSkillTraitValues.ubSNAimClicksAdded == 1 && !fExpertLevel )
							sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsSniper[15]);
						else
							sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[16], ( gSkillTraitValues.ubSNAimClicksAdded * (fExpertLevel ? 2 : 1)));
					}
					AppendPersonnelText( apStr, atStr );
				}

				if ( gSkillTraitValues.ubSNFocusRadius != 0 && gSkillTraitValues.sSNFocusInterruptBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[19], gSkillTraitValues.sSNFocusInterruptBonus );
					AppendPersonnelText( apStr, atStr );
				}

				break;
			}
			case RANGER_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubRABonusCtHRifles != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsRanger[0], ( gSkillTraitValues.ubRABonusCtHRifles * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubRABonusCtHShotguns != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsRanger[1], ( gSkillTraitValues.ubRABonusCtHShotguns * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubRAPumpShotgunsAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsRanger[2], ( gSkillTraitValues.ubRAPumpShotgunsAPsReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubRAFiringSpeedBonusShotguns != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsRanger[3], ( gSkillTraitValues.ubRAFiringSpeedBonusShotguns * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubRAReloadSpeedShotgunsManual != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsRanger[7], ( gSkillTraitValues.ubRAReloadSpeedShotgunsManual * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubRAAimClicksAdded != 0 )
				{
					if( gSkillTraitValues.ubRAAimClicksAdded == 1 && !fExpertLevel )
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsRanger[4], (UINT8)( gSkillTraitValues.ubRAAimClicksAdded * (fExpertLevel ? 2 : 1) ) );
					else
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsRanger[5], (UINT8)( gSkillTraitValues.ubRAAimClicksAdded * (fExpertLevel ? 2 : 1) ) );

					AppendPersonnelText( apStr, atStr );
					// half of the above bonus also applies to rifles
					if( (gSkillTraitValues.ubRAAimClicksAdded >= 2 && !fExpertLevel) || (gSkillTraitValues.ubRAAimClicksAdded == 1 && fExpertLevel) )
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsRanger[8], (UINT8)( gSkillTraitValues.ubRAAimClicksAdded * (fExpertLevel ? 2 : 1) / 2.0f));
					else
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsRanger[9], (UINT8)( gSkillTraitValues.ubRAAimClicksAdded * (fExpertLevel ? 2 : 1) / 2.0f));

					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubRAEffectiveRangeBonusShotguns != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsRanger[6], ( gSkillTraitValues.ubRAEffectiveRangeBonusShotguns * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				// Flugente: focus is a skill that can be used by multiple traits. For simplicity, the ini values are in the sniper trait section
				if ( gSkillTraitValues.ubSNFocusRadius != 0 && gSkillTraitValues.sSNFocusInterruptBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[19], gSkillTraitValues.sSNFocusInterruptBonus );
					AppendPersonnelText( apStr, atStr );
				}

				break;
			}
			case GUNSLINGER_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubGSFiringSpeedBonusPistols != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsGunslinger[0], ( gSkillTraitValues.ubGSFiringSpeedBonusPistols * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubGSEffectiveRangeBonusPistols != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsGunslinger[1], ( gSkillTraitValues.ubGSEffectiveRangeBonusPistols * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubGSBonusCtHPistols != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsGunslinger[2], ( gSkillTraitValues.ubGSBonusCtHPistols * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubGSBonusCtHMachinePistols != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsGunslinger[3], ( gSkillTraitValues.ubGSBonusCtHMachinePistols * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					if( gSkillTraitValues.ubGSCtHMPExcludeAuto )
						AppendPersonnelText( atStr, gzIMPMajorTraitsHelpTextsGunslinger[4] );
					AppendPersonnelText( atStr, L"\n");
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubGSAimingBonusPerClick != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsGunslinger[5], ( gSkillTraitValues.ubGSAimingBonusPerClick * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubGSPercentReadyPistolsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsGunslinger[6], ( gSkillTraitValues.ubGSPercentReadyPistolsReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubGSRealoadSpeedHandgunsBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsGunslinger[7], ( gSkillTraitValues.ubGSRealoadSpeedHandgunsBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubGSAimClicksAdded != 0 )
				{
					if( gSkillTraitValues.ubGSAimClicksAdded == 1 && !fExpertLevel )
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsGunslinger[8], ( gSkillTraitValues.ubGSAimClicksAdded * (fExpertLevel ? 2 : 1)));
					else
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsGunslinger[9], ( gSkillTraitValues.ubGSAimClicksAdded * (fExpertLevel ? 2 : 1)));

					AppendPersonnelText( apStr, atStr );
				}
				// Flugente: focus is a skill that can be used by multiple traits. For simplicity, the ini values are in the sniper trait section
				if ( gSkillTraitValues.ubSNFocusRadius != 0 && gSkillTraitValues.sSNFocusInterruptBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSniper[19], gSkillTraitValues.sSNFocusInterruptBonus );
					AppendPersonnelText( apStr, atStr );
				}

				// Flugente: can we fan the hammer on certain guns (it's effectively a hidden mode of revolvers)?
				if ( gSkillTraitValues.fCanFanTheHammer )
				{
					sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsGunslinger[10] );
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case MARTIAL_ARTS_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubMAPunchAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[0], ( gSkillTraitValues.ubMAPunchAPsReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMABonusCtHBareHands != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[1], ( gSkillTraitValues.ubMABonusCtHBareHands * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMABonusCtHBrassKnuckles != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[2], ( gSkillTraitValues.ubMABonusCtHBrassKnuckles * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMABonusDamageHandToHand != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[3], ( gSkillTraitValues.ubMABonusDamageHandToHand * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMABonusBreathDamageHandToHand != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[4], ( gSkillTraitValues.ubMABonusBreathDamageHandToHand * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.usMALostBreathRegainPenalty != 0 )
				{
					if( (gSkillTraitValues.usMALostBreathRegainPenalty * (fExpertLevel ? 2 : 1)) <= 25)
						sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsMartialArts[5]);
					else if( (gSkillTraitValues.usMALostBreathRegainPenalty * (fExpertLevel ? 2 : 1)) <= 50)
						sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsMartialArts[6]);
					else if( (gSkillTraitValues.usMALostBreathRegainPenalty * (fExpertLevel ? 2 : 1)) <= 100)
						sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsMartialArts[7]);
					else if( (gSkillTraitValues.usMALostBreathRegainPenalty * (fExpertLevel ? 2 : 1)) <= 200)
						sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsMartialArts[8]);
					else if( (gSkillTraitValues.usMALostBreathRegainPenalty * (fExpertLevel ? 2 : 1)) <= 400)
						sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsMartialArts[9]);
					else if( (gSkillTraitValues.usMALostBreathRegainPenalty * (fExpertLevel ? 2 : 1)) <= 700)
						sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsMartialArts[10]);
					else
						sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsMartialArts[11]);

					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.usMAAimedPunchDamageBonus != 0 )
				{
					if ( !fRegMale || (gSkillTraitValues.fPermitExtraAnimationsOnlyToMA && !fExpertLevel) )
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[12], ( gSkillTraitValues.usMAAimedPunchDamageBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					else
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[13], ( gSkillTraitValues.usMAAimedPunchDamageBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);

					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMAChanceToDodgeHtH != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[14], ( gSkillTraitValues.ubMAChanceToDodgeHtH * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMAOnTopCTDHtHBareHanded != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[15], ( gSkillTraitValues.ubMAOnTopCTDHtHBareHanded * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					if( gSkillTraitValues.ubMAOnTopCTDHtHBrassKnuckles == gSkillTraitValues.ubMAOnTopCTDHtHBareHanded ) 
						AppendPersonnelText( apStr, gzIMPMajorTraitsHelpTextsMartialArts[16] );
					else if( gSkillTraitValues.ubMAOnTopCTDHtHBrassKnuckles > 0 )
					{
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[17], ( gSkillTraitValues.ubMAOnTopCTDHtHBrassKnuckles * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
						AppendPersonnelText( apStr, atStr );
					}
					AppendPersonnelText( apStr, L"\n" );
				}
				else if( gSkillTraitValues.ubMAOnTopCTDHtHBrassKnuckles != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[18], ( gSkillTraitValues.ubMAOnTopCTDHtHBrassKnuckles * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMAChanceToDodgeMelee != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[19], ( gSkillTraitValues.ubMAChanceToDodgeMelee * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMAReducedAPsToSteal != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[20], ( gSkillTraitValues.ubMAReducedAPsToSteal * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMAAPsChangeStanceReduction != 0 && ( gSkillTraitValues.ubMAAPsChangeStanceReduction == gSkillTraitValues.ubMAApsTurnAroundReduction == gSkillTraitValues.ubMAAPsClimbOrJumpReduction ))
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[21], ( gSkillTraitValues.ubMAAPsChangeStanceReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				else 
				{
					if( gSkillTraitValues.ubMAAPsChangeStanceReduction != 0 )
					{
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[22], ( gSkillTraitValues.ubMAAPsChangeStanceReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
						AppendPersonnelText( apStr, atStr );
					}
					if( gSkillTraitValues.ubMAApsTurnAroundReduction != 0 )
					{
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[23], ( gSkillTraitValues.ubMAApsTurnAroundReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
						AppendPersonnelText( apStr, atStr );
					}
					if( gSkillTraitValues.ubMAAPsClimbOrJumpReduction != 0 )
					{
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[24], ( gSkillTraitValues.ubMAAPsClimbOrJumpReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
						AppendPersonnelText( apStr, atStr );
					}
				}
				if( gSkillTraitValues.ubMAReducedAPsRegisteredWhenMoving != 0 && UsingImprovedInterruptSystem() )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[27], ( gSkillTraitValues.ubMAReducedAPsRegisteredWhenMoving * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMAChanceToCkickDoors != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsMartialArts[25], ( gSkillTraitValues.ubMAChanceToCkickDoors * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}

				if ( fRegMale &&
					((gSkillTraitValues.fPermitExtraAnimationsOnlyToMA && fExpertLevel) ||
					!gSkillTraitValues.fPermitExtraAnimationsOnlyToMA ))
				{
					sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsMartialArts[26]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case SQUADLEADER_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubSLBonusAPsPercent != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSquadleader[0], ( gSkillTraitValues.ubSLBonusAPsPercent * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSLEffectiveLevelInRadius != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSquadleader[1], ( gSkillTraitValues.ubSLEffectiveLevelInRadius * (fExpertLevel ? 2 : 1)), (fExpertLevel ? gzMercSkillTextNew[ubTraitNumber + NEWTRAIT_MERCSKILL_EXPERTOFFSET] : gzMercSkillTextNew[ubTraitNumber]));
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSLEffectiveLevelAsStandby != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSquadleader[2], ( gSkillTraitValues.ubSLEffectiveLevelAsStandby * (fExpertLevel ? 2 : 1)));
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSLCollectiveInterruptsBonus != 0 && UsingImprovedInterruptSystem() )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSquadleader[11], ( gSkillTraitValues.ubSLCollectiveInterruptsBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSLOverallSuppresionBonusPercent != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSquadleader[3], ( gSkillTraitValues.ubSLOverallSuppresionBonusPercent * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0], (fExpertLevel ? gzMercSkillTextNew[ubTraitNumber + NEWTRAIT_MERCSKILL_EXPERTOFFSET] : gzMercSkillTextNew[ubTraitNumber]));
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSLMoraleGainBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSquadleader[4], ( gSkillTraitValues.ubSLMoraleGainBonus * (fExpertLevel ? 2 : 1)));
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSLMoraleLossReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSquadleader[5], ( gSkillTraitValues.ubSLMoraleLossReduction * (fExpertLevel ? 2 : 1)));
					AppendPersonnelText( apStr, atStr );
				}
							
				sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSquadleader[6], gSkillTraitValues.usSLRadiusNormal);
				AppendPersonnelText( apStr, atStr );
				if( gSkillTraitValues.usSLRadiusExtendedEar > gSkillTraitValues.usSLRadiusNormal )
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSquadleader[7], gSkillTraitValues.usSLRadiusExtendedEar);

				AppendPersonnelText( apStr, atStr );
				AppendPersonnelText( apStr, L"\n" );

				if( gSkillTraitValues.ubSLMaxBonuses > 1 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSquadleader[8], gSkillTraitValues.ubSLMaxBonuses ) ;
					AppendPersonnelText( apStr, atStr );
				}
				
				if( gSkillTraitValues.ubSLFearResistance != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSquadleader[9], ( gSkillTraitValues.ubSLFearResistance * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0], (fExpertLevel ? gzMercSkillTextNew[ubTraitNumber + NEWTRAIT_MERCSKILL_EXPERTOFFSET] : gzMercSkillTextNew[ubTraitNumber]));
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSLDeathMoralelossMultiplier != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSquadleader[10], (1 + ( gSkillTraitValues.ubSLDeathMoralelossMultiplier * (fExpertLevel ? 2 : 1))), (fExpertLevel ? gzMercSkillTextNew[ubTraitNumber + NEWTRAIT_MERCSKILL_EXPERTOFFSET] : gzMercSkillTextNew[ubTraitNumber]));
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case TECHNICIAN_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.usTERepairSpeedBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsTechnician[0], ( gSkillTraitValues.usTERepairSpeedBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.usTELockpickingBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsTechnician[1], ( gSkillTraitValues.usTELockpickingBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.usTEDisarmElTrapBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsTechnician[2], ( gSkillTraitValues.usTEDisarmElTrapBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.usTEAttachingItemsBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsTechnician[3], ( gSkillTraitValues.usTEAttachingItemsBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTEUnjamGunBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsTechnician[4], ( gSkillTraitValues.ubTEUnjamGunBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTERepairElectronicsPenaltyReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsTechnician[5], ( gSkillTraitValues.ubTERepairElectronicsPenaltyReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTEChanceToDetectTrapsBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsTechnician[6], ( gSkillTraitValues.ubTEChanceToDetectTrapsBonus * (fExpertLevel ? 2 : 1)));
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTECtHControlledRobotBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsTechnician[7], ( gSkillTraitValues.ubTECtHControlledRobotBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0], (fExpertLevel ? gzMercSkillTextNew[ubTraitNumber + NEWTRAIT_MERCSKILL_EXPERTOFFSET] : gzMercSkillTextNew[ubTraitNumber]));
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTETraitsNumToRepairRobot == 2 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsTechnician[8], (fExpertLevel ? gzMercSkillTextNew[ubTraitNumber + NEWTRAIT_MERCSKILL_EXPERTOFFSET] : gzMercSkillTextNew[ubTraitNumber]));
					AppendPersonnelText( apStr, atStr );
				}
				else if( gSkillTraitValues.ubTETraitsNumToRepairRobot == 1 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsTechnician[8], (fExpertLevel ? gzMercSkillTextNew[ubTraitNumber + NEWTRAIT_MERCSKILL_EXPERTOFFSET] : gzMercSkillTextNew[ubTraitNumber]));
					AppendPersonnelText( apStr, atStr );

					if( gSkillTraitValues.ubTERepairRobotPenaltyReduction != 0 && fExpertLevel)
					{
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsTechnician[9], ( gSkillTraitValues.ubTERepairRobotPenaltyReduction * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
						AppendPersonnelText( apStr, atStr );
					}
				}
				if ( gGameExternalOptions.fAdvRepairSystem && gSkillTraitValues.fTETraitsCanRestoreItemThreshold )
				{
					sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsTechnician[10] );
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case DOCTOR_NT:
			{
				BOOLEAN fCanSurgery = FALSE;
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubDONumberTraitsNeededForSurgery != 0 && ((gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentOnTop) > 0))
				{
					if( gSkillTraitValues.ubDONumberTraitsNeededForSurgery <= (fExpertLevel ? 2 : 1))
					{
						fCanSurgery = TRUE;
						sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsDoctor[0]);
						AppendPersonnelText( apStr, atStr );
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsDoctor[1], (gSkillTraitValues.ubDOSurgeryHealPercentBase + ( gSkillTraitValues.ubDOSurgeryHealPercentOnTop * (fExpertLevel ? 2 : 1))), sSpecialCharacters[0]);
						AppendPersonnelText( apStr, atStr );
						if( gSkillTraitValues.usDOSurgeryMedBagConsumption >= 60 )
							AppendPersonnelText( apStr, gzIMPMajorTraitsHelpTextsDoctor[2] );

						AppendPersonnelText( apStr, L"\n" );

						if ( gSkillTraitValues.ubDOSurgeryHealPercentBloodbag )
						{
							sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsDoctor[10], gSkillTraitValues.ubDOSurgeryHealPercentBloodbag, sSpecialCharacters[0] );
							AppendPersonnelText( apStr, atStr );
						}
					}
				}
				if( (gSkillTraitValues.usDORepairStatsRateBasic + gSkillTraitValues.usDORepairStatsRateOnTop) > 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsDoctor[3]);
					AppendPersonnelText( apStr, atStr );
					if( fCanSurgery )
						AppendPersonnelText( apStr, gzIMPMajorTraitsHelpTextsDoctor[4] );

					AppendPersonnelText( apStr, gzIMPMajorTraitsHelpTextsDoctor[5] );
				}
				if( gSkillTraitValues.usDODoctorAssignmentBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsDoctor[6], ( gSkillTraitValues.usDODoctorAssignmentBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubDOBandagingSpeedPercent != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsDoctor[7], ( gSkillTraitValues.ubDOBandagingSpeedPercent * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubDONaturalRegenBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsDoctor[8], ( gSkillTraitValues.ubDONaturalRegenBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					if( gSkillTraitValues.ubDOMaxRegenBonuses > 1 )
					{
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsDoctor[9], gSkillTraitValues.ubDOMaxRegenBonuses ) ;
						AppendPersonnelText( apStr, atStr );
					}
					AppendPersonnelText( apStr, L"\n" );
				}
				break;
			}
			case AMBIDEXTROUS_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubAMPenaltyDoubleReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsAmbidextrous[0], gSkillTraitValues.ubAMPenaltyDoubleReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAMReloadSpeedMagazines != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsAmbidextrous[1], gSkillTraitValues.ubAMReloadSpeedMagazines, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAMReloadSpeedLoose != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsAmbidextrous[2], gSkillTraitValues.ubAMReloadSpeedLoose, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAMPickItemsAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsAmbidextrous[3], gSkillTraitValues.ubAMPickItemsAPsReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAMWorkBackpackAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsAmbidextrous[4], gSkillTraitValues.ubAMWorkBackpackAPsReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAMHandleDoorsAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsAmbidextrous[5], gSkillTraitValues.ubAMHandleDoorsAPsReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAMHandleBombsAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsAmbidextrous[6], gSkillTraitValues.ubAMHandleBombsAPsReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubAMAttachingItemsAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsAmbidextrous[7], gSkillTraitValues.ubAMAttachingItemsAPsReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case MELEE_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubMEBladesAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsMelee[0], gSkillTraitValues.ubMEBladesAPsReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMECtHBladesBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsMelee[1], gSkillTraitValues.ubMECtHBladesBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMECtHBluntBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsMelee[2], gSkillTraitValues.ubMECtHBluntBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMEDamageBonusBlades != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsMelee[3], gSkillTraitValues.ubMEDamageBonusBlades, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMEDamageBonusBlunt != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsMelee[4], gSkillTraitValues.ubMEDamageBonusBlunt, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.usMEAimedMeleeAttackDamageBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsMelee[5], gSkillTraitValues.usMEAimedMeleeAttackDamageBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMEDodgeBladesBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsMelee[6], gSkillTraitValues.ubMEDodgeBladesBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMECtDBladesOnTopWithBladeInHands != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsMelee[7], gSkillTraitValues.ubMECtDBladesOnTopWithBladeInHands, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMEDodgeBluntBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsMelee[8], gSkillTraitValues.ubMEDodgeBluntBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubMECtDBluntOnTopWithBladeInHands != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsMelee[9], gSkillTraitValues.ubMECtDBluntOnTopWithBladeInHands, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case THROWING_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubTHBladesAPsReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[0], gSkillTraitValues.ubTHBladesAPsReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTHBladesMaxRange != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[1], gSkillTraitValues.ubTHBladesMaxRange, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTHBladesCtHBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[2], gSkillTraitValues.ubTHBladesCtHBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTHBladesCtHBonusPerClick != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[3], gSkillTraitValues.ubTHBladesCtHBonusPerClick, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTHBladesDamageBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[4], gSkillTraitValues.ubTHBladesDamageBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTHBladesDamageBonusPerClick != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[5], gSkillTraitValues.ubTHBladesDamageBonusPerClick, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTHBladesSilentCriticalHitChance != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[6], gSkillTraitValues.ubTHBladesSilentCriticalHitChance, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTHBladesCriticalHitMultiplierBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[7], gSkillTraitValues.ubTHBladesCriticalHitMultiplierBonus);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTHBladesAimClicksAdded != 0 )
				{
					if( gSkillTraitValues.ubTHBladesAimClicksAdded == 1 )
						sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[8], gSkillTraitValues.ubTHBladesAimClicksAdded );
					else
						sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[9], gSkillTraitValues.ubTHBladesAimClicksAdded );

					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTHAPsNeededToThrowGrenadesReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[10], gSkillTraitValues.ubTHAPsNeededToThrowGrenadesReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTHMaxRangeToThrowGrenades != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[11], gSkillTraitValues.ubTHMaxRangeToThrowGrenades, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTHCtHWhenThrowingGrenades != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsThrowing[12], gSkillTraitValues.ubTHCtHWhenThrowingGrenades, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case NIGHT_OPS_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubNOeSightRangeBonusInDark != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsNightOps[0], gSkillTraitValues.ubNOeSightRangeBonusInDark, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubNOHearingRangeBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsNightOps[1], gSkillTraitValues.ubNOHearingRangeBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubNOHearingRangeBonusInDark != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsNightOps[2], gSkillTraitValues.ubNOHearingRangeBonusInDark, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubNOIterruptsBonusInDark != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsNightOps[3], gSkillTraitValues.ubNOIterruptsBonusInDark, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubNONeedForSleepReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsNightOps[4], gSkillTraitValues.ubNONeedForSleepReduction);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case STEALTHY_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubSTStealthModeSpeedBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsStealthy[0], gSkillTraitValues.ubSTStealthModeSpeedBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSTBonusToMoveQuietly != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsStealthy[1], gSkillTraitValues.ubSTBonusToMoveQuietly, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSTStealthBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsStealthy[2], gSkillTraitValues.ubSTStealthBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSTReducedAPsRegistered != 0 && UsingImprovedInterruptSystem() )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsStealthy[4], gSkillTraitValues.ubSTReducedAPsRegistered, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSTStealthPenaltyForMovingReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsStealthy[3], gSkillTraitValues.ubSTStealthPenaltyForMovingReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case ATHLETICS_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubATAPsMovementReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsAthletics[0], gSkillTraitValues.ubATAPsMovementReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubATBPsMovementReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsAthletics[1], gSkillTraitValues.ubATBPsMovementReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case BODYBUILDING_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubBBDamageResistance != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsBodybuilding[0], gSkillTraitValues.ubBBDamageResistance, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubBBCarryWeightBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsBodybuilding[1], gSkillTraitValues.ubBBCarryWeightBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubBBBreathLossForHtHImpactReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsBodybuilding[2], gSkillTraitValues.ubBBBreathLossForHtHImpactReduction, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.usBBIncreasedNeededDamageToFallDown != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsBodybuilding[3], gSkillTraitValues.usBBIncreasedNeededDamageToFallDown, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case DEMOLITIONS_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubDEDamageOfBombsAndMines != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsDemolitions[0], gSkillTraitValues.ubDEDamageOfBombsAndMines, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubDEAttachDetonatorCheckBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsDemolitions[1], gSkillTraitValues.ubDEAttachDetonatorCheckBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubDEPlantAndRemoveBombCheckBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsDemolitions[2], gSkillTraitValues.ubDEPlantAndRemoveBombCheckBonus, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubDEPlacedBombLevelBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsDemolitions[3], gSkillTraitValues.ubDEPlacedBombLevelBonus);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubDEShapedChargeDamageMultiplier != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsDemolitions[4], gSkillTraitValues.ubDEShapedChargeDamageMultiplier);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case TEACHING_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubTGBonusToTrainMilitia != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsTeaching[0], gSkillTraitValues.ubTGBonusToTrainMilitia, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTGEffectiveLDRToTrainMilitia != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsTeaching[1], gSkillTraitValues.ubTGEffectiveLDRToTrainMilitia, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTGBonusToTeachOtherMercs != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsTeaching[2], gSkillTraitValues.ubTGBonusToTeachOtherMercs, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTGEffectiveSkillValueForTeaching != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsTeaching[3], gSkillTraitValues.ubTGEffectiveSkillValueForTeaching);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubTGBonusOnPractising != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsTeaching[4], gSkillTraitValues.ubTGBonusOnPractising, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case SCOUTING_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gSkillTraitValues.ubSCSightRangebonusWithScopes != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsScouting[0], gSkillTraitValues.ubSCSightRangebonusWithScopes, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.usSCSightRangebonusWithBinoculars != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsScouting[1], gSkillTraitValues.usSCSightRangebonusWithBinoculars, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSCTunnelVisionReducedWithBinoculars != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsScouting[2], gSkillTraitValues.ubSCTunnelVisionReducedWithBinoculars, sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.fSCCanDetectEnemyPresenseAround  )
				{
					if( gSkillTraitValues.fSCCanDetermineEnemyNumbersAround )
					{
						sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMinorTraitsHelpTextsScouting[3]);
						AppendPersonnelText( apStr, atStr );
					}
					else
					{
						sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMinorTraitsHelpTextsScouting[4]);
						AppendPersonnelText( apStr, atStr );
					}
				}
				if( gSkillTraitValues.fSCPreventsTheEnemyToAmbushMercs )
				{
					sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMinorTraitsHelpTextsScouting[5]);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.fSCPreventsBloodcatsAmbushes )
				{
					sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMinorTraitsHelpTextsScouting[6]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case COVERT_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsCovertOps[0]);
				AppendPersonnelText( apStr, atStr );

				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsCovertOps[1]);
				AppendPersonnelText( apStr, atStr );

				if ( !fExpertLevel )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsCovertOps[2], gSkillTraitValues.sCOCloseDetectionRange);
					AppendPersonnelText( apStr, atStr );
				}

				sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsCovertOps[3], gSkillTraitValues.sCOCloseDetectionRangeSoldierCorpse);
				AppendPersonnelText( apStr, atStr );

				sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsCovertOps[4], ( gSkillTraitValues.sCOMeleeCTHBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
				AppendPersonnelText( apStr, atStr );

				sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsCovertOps[5], ( gSkillTraitValues.sCoMeleeInstakillBonus * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
				AppendPersonnelText( apStr, atStr );
				
				INT16 apreduction =  ( gSkillTraitValues.sCODisguiseAPReduction * (fExpertLevel ? 2 : 1));
				sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsCovertOps[6], apreduction, sSpecialCharacters[0]);
				AppendPersonnelText( apStr, atStr );

				if ( gSkillTraitValues.fCOTurncoats )
				{
					sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsCovertOps[7] );
					AppendPersonnelText( apStr, atStr );
				}

				break;
			}

			// Flugente: Radio Operator
			case RADIO_OPERATOR_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsRadioOperator[0]);
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsRadioOperator[1]);
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsRadioOperator[2]);
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsRadioOperator[3]);
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsRadioOperator[4]);
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsRadioOperator[5]);
				AppendPersonnelText( apStr, atStr );
				break;
			}
			case SNITCH_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMinorTraitsHelpTextsSnitch[0]);
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMinorTraitsHelpTextsSnitch[1]);
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMinorTraitsHelpTextsSnitch[2]);
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMinorTraitsHelpTextsSnitch[3]);
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMinorTraitsHelpTextsSnitch[4]);
				AppendPersonnelText( apStr, atStr );
				if( gSkillTraitValues.ubSNTPassiveReputationGain )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsSnitch[5],gSkillTraitValues.ubSNTPassiveReputationGain);
					AppendPersonnelText( apStr, atStr );
				}
				if( gSkillTraitValues.ubSNTHearingRangeBonus )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMinorTraitsHelpTextsSnitch[6],gSkillTraitValues.ubSNTHearingRangeBonus);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case SURVIVAL_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );

				if ( gSkillTraitValues.ubSVGroupTimeSpentForTravellingFoot != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSurvival[0], gSkillTraitValues.ubSVGroupTimeSpentForTravellingFoot, sSpecialCharacters[0] );
					AppendPersonnelText( apStr, atStr );
				}
				if ( gSkillTraitValues.ubSVGroupTimeSpentForTravellingVehicle != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSurvival[1], gSkillTraitValues.ubSVGroupTimeSpentForTravellingVehicle, sSpecialCharacters[0] );
					AppendPersonnelText( apStr, atStr );
				}
				if ( gSkillTraitValues.ubSVBreathForTravellingReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSurvival[2], gSkillTraitValues.ubSVBreathForTravellingReduction, sSpecialCharacters[0] );
					AppendPersonnelText( apStr, atStr );
				}
				if ( gSkillTraitValues.dSVWeatherPenaltiesReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSurvival[3], (UINT8)(100 * gSkillTraitValues.dSVWeatherPenaltiesReduction), sSpecialCharacters[0] );
					AppendPersonnelText( apStr, atStr );
				}

				if ( gSkillTraitValues.ubSVCamoWornountSpeedReduction != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSurvival[4], gSkillTraitValues.ubSVCamoWornountSpeedReduction, sSpecialCharacters[0] );
					AppendPersonnelText( apStr, atStr );
				}

				if ( gSkillTraitValues.usSVTrackerMaxRange && gSkillTraitValues.usSVTrackerAbility )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSurvival[5], (gSkillTraitValues.usSVTrackerAbility * gSkillTraitValues.usSVTrackerMaxRange / 100) );
					AppendPersonnelText( apStr, atStr );
				}

				if ( gGameExternalOptions.fDisease )
				{
					if ( gSkillTraitValues.usSVDiseaseResistance != 0 )
					{
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSurvival[6], gSkillTraitValues.usSVDiseaseResistance > 0 ? L"+" : L"", gSkillTraitValues.usSVDiseaseResistance );
						AppendPersonnelText( apStr, atStr );
					}
				}

				if ( UsingFoodSystem() )
				{
					if ( gSkillTraitValues.sSVFoodConsumption != 0 )
					{
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSurvival[7], gSkillTraitValues.sSVFoodConsumption > 0 ? L"+" : L"", gSkillTraitValues.sSVFoodConsumption );
						AppendPersonnelText( apStr, atStr );
					}

					if ( gSkillTraitValues.sSVDrinkConsumption != 0 )
					{
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSurvival[8], gSkillTraitValues.sSVDrinkConsumption > 0 ? L"+" : L"", gSkillTraitValues.sSVDrinkConsumption );
						AppendPersonnelText( apStr, atStr );
					}
				}

				if ( gGameExternalOptions.gfAllowSnakes )
				{
					if ( gSkillTraitValues.usSVSnakeDefense > 0 )
					{
						sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSurvival[9], gSkillTraitValues.usSVSnakeDefense );
						AppendPersonnelText( apStr, atStr );
					}
				}

				if ( gSkillTraitValues.ubSVCamoEffectivenessBonus != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPMajorTraitsHelpTextsSurvival[10], gSkillTraitValues.ubSVCamoEffectivenessBonus );
					AppendPersonnelText( apStr, atStr );
				}

				break;
			}
			case NO_SKILLTRAIT_NT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsNone[0] );
				AppendPersonnelText( apStr, atStr );
				break;
			}
		}
	}
	else
	{
		switch( ubTraitNumber )
		{
			case LOCKPICKING_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gbSkillTraitBonus[LOCKPICKING_OT] != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[0], ( gbSkillTraitBonus[LOCKPICKING_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case HANDTOHAND_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gbSkillTraitBonus[HANDTOHAND_OT] != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[1], (  gbSkillTraitBonus[HANDTOHAND_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[2], (  3 * gbSkillTraitBonus[HANDTOHAND_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[3], (  gbSkillTraitBonus[HANDTOHAND_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case ELECTRONICS_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPOldSkillTraitsHelpTexts[4] );
				AppendPersonnelText( apStr, atStr );
				break;
			}
			case NIGHTOPS_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[5], (fExpertLevel ? 2 : 1));
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[6], (fExpertLevel ? 2 : 1));
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[7], (fExpertLevel ? 2 : 1));
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[8], (fExpertLevel ? 2 : 1));
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[9], (fExpertLevel ? 2 : 1));
				AppendPersonnelText( apStr, atStr );
				break;
			}
			case THROWING_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gbSkillTraitBonus[THROWING_OT] != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[10], (  gbSkillTraitBonus[THROWING_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[11], (  gbSkillTraitBonus[THROWING_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[12], (  10 * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case TEACHING_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gGameExternalOptions.ubTeachBonusToTrain != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[13], (  gGameExternalOptions.ubTeachBonusToTrain * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				if( (gGameExternalOptions.usTeacherTraitEffectOnLeadership - 100) > 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[14], (  (gGameExternalOptions.usTeacherTraitEffectOnLeadership - 100) * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case HEAVY_WEAPS_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gbSkillTraitBonus[HEAVY_WEAPS_OT] != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[15], (  gbSkillTraitBonus[HEAVY_WEAPS_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case AUTO_WEAPS_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[16], (  2 * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPOldSkillTraitsHelpTexts[17] );
				AppendPersonnelText( apStr, atStr );
				break;
			}
			case STEALTHY_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[18], (  25 * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
				AppendPersonnelText( apStr, atStr );
				if( gGameExternalOptions.ubStealthTraitCoverValue != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[19], (  gGameExternalOptions.ubStealthTraitCoverValue * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case AMBIDEXT_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPOldSkillTraitsHelpTexts[20] );
				AppendPersonnelText( apStr, atStr );
				break;
			}
			case MARTIALARTS_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gbSkillTraitBonus[MARTIALARTS_OT] != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[28], (  gbSkillTraitBonus[MARTIALARTS_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[29], (  gbSkillTraitBonus[MARTIALARTS_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[30], (  gbSkillTraitBonus[MARTIALARTS_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[31], (  gbSkillTraitBonus[MARTIALARTS_OT] * 2 / 3 * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[32], (  gbSkillTraitBonus[MARTIALARTS_OT] / 2 * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPOldSkillTraitsHelpTexts[33]);
				AppendPersonnelText( apStr, atStr );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPOldSkillTraitsHelpTexts[34]);
				AppendPersonnelText( apStr, atStr );
				break;
			}
			case KNIFING_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gbSkillTraitBonus[KNIFING_OT] != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[21], (  gbSkillTraitBonus[KNIFING_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[22], (  gbSkillTraitBonus[KNIFING_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[23], (  gbSkillTraitBonus[KNIFING_OT] / 3 * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[24], (  gbSkillTraitBonus[KNIFING_OT] / 2 * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				break;
			}
			case PROF_SNIPER_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				if( gbSkillTraitBonus[PROF_SNIPER_OT] != 0 )
				{
					sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[25], (  gbSkillTraitBonus[PROF_SNIPER_OT] * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
					AppendPersonnelText( apStr, atStr );
				}
				sgp_swprintf(atStr, std::size(atStr), gzIMPOldSkillTraitsHelpTexts[26], (  10 * (fExpertLevel ? 2 : 1)), sSpecialCharacters[0]);
				AppendPersonnelText( apStr, atStr );
				break;
			}
			case CAMOUFLAGED_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPOldSkillTraitsHelpTexts[27] );
				AppendPersonnelText( apStr, atStr );
				break;
			}
			case NO_SKILLTRAIT_OT:
			{
				sgp_swprintf( apStr, 5000, L"%s", L"" );
				sgp_swprintf(atStr, std::size(atStr), L"%s", gzIMPMajorTraitsHelpTextsNone[0] );
				AppendPersonnelText( apStr, atStr );
				break;
			}
		}
	}
}

// SANDRO - Popup text windows for character 
void AssignPersonnelCharacterTraitHelpText( UINT8 ubCharacterNumber )
{
	if (!PersonnelRosterModel::IsValidIndex(
		ubCharacterNumber, NUM_CHAR_TRAITS)) return;
	CHAR16	apStr[ 1000 ];

	sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
	sgp_swprintf(apStr, std::size(apStr), L"%s", gzIMPNewCharacterTraitsHelpTexts[ubCharacterNumber] );

	// Set region help text
	SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[5]), apStr );
	SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[5], MSYS_NO_CALLBACK );
}


// SANDRO - Popup text windows for disability 
void AssignPersonnelDisabilityHelpText( UINT8 ubDisabilityNumber )
{
	if (!PersonnelRosterModel::IsValidIndex(
		ubDisabilityNumber, NUM_DISABILITIES)) return;
	CHAR16	apStr[ 500 ];

	sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
	sgp_swprintf(apStr, std::size(apStr), L"%s", gzIMPDisabilitiesHelpTexts[ubDisabilityNumber] );

	// Set region help text
	SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[6]), apStr );
	SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[6], MSYS_NO_CALLBACK );
}

void AssignPersonnelMultipleDisabilityHelpText( const TacticalActor* pSoldier )
{
	if (!pSoldier) return;
	CHAR16	apStr[1000];
	CHAR16 atStr[500];
	sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
	for ( UINT8 i = NO_DISABILITY + 1; i < NUM_DISABILITIES; ++i )
	{
		if ( DoesMercHaveDisability( pSoldier, i ) )
		{
			sgp_swprintf(atStr, std::size(atStr), L"%s\n",
				gzIMPDisabilityTraitText[i]);
			AppendPersonnelText(apStr, atStr);
			sgp_swprintf(atStr, std::size(atStr), L"%s\n\n",
				gzIMPDisabilitiesHelpTexts[i]);
			AppendPersonnelText(apStr, atStr);
		}
	}

	// Set region help text
	SetRegionFastHelpText( &( gSkillTraitHelpTextRegion[6] ), apStr );
	SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[6], MSYS_NO_CALLBACK );
}

void AssignPersonnelKillsHelpText( INT32 ubProfile )
{
	if (!PersonnelRosterModel::IsValidProfileId(ubProfile, NUM_PROFILES)) return;
	CHAR16	apStr[ 1000 ];
	CHAR16	atStr[ 150 ];

	sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
	if (gMercProfiles[ubProfile].records.usKillsElites > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 0 ], gMercProfiles[ubProfile].records.usKillsElites );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usKillsRegulars > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 1 ], gMercProfiles[ubProfile].records.usKillsRegulars );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usKillsAdmins > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 2 ], gMercProfiles[ubProfile].records.usKillsAdmins );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usKillsHostiles > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 3 ], gMercProfiles[ubProfile].records.usKillsHostiles );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usKillsCreatures > 0 || fShowRecordsIfZero)
	{
		// WANNE: Only display the monster info, when we play with monsters!
		if (gGameOptions.ubGameStyle == STYLE_SCIFI && gGameExternalOptions.fEnableCrepitus)
		{
			sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 4 ], gMercProfiles[ubProfile].records.usKillsCreatures );
			AppendPersonnelText( apStr, atStr );
		}
	}
	if (gMercProfiles[ubProfile].records.usKillsTanks > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 5 ], gMercProfiles[ubProfile].records.usKillsTanks );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usKillsOthers > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 6 ], gMercProfiles[ubProfile].records.usKillsOthers );
		AppendPersonnelText( apStr, atStr );
	}

	if (gGameSettings.fOptions[TOPTION_ZOMBIES] )
	{
		if (gMercProfiles[ubProfile].records.usKillsZombies > 0 || fShowRecordsIfZero)
		{
			sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 46 ], gMercProfiles[ubProfile].records.usKillsZombies );
			AppendPersonnelText( apStr, atStr );
		}
	}

	// Set region help text
	SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[7]), apStr );
	SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[7], MSYS_NO_CALLBACK );
}

void AssignPersonnelAssistsHelpText( INT32 ubProfile )
{
	if (!PersonnelRosterModel::IsValidProfileId(ubProfile, NUM_PROFILES)) return;
	CHAR16	apStr[ 350 ];
	CHAR16	atStr[ 80 ];

	sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
	if (gMercProfiles[ubProfile].records.usAssistsMercs > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 7 ], gMercProfiles[ubProfile].records.usAssistsMercs );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usAssistsMilitia > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 8 ], gMercProfiles[ubProfile].records.usAssistsMilitia );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usAssistsOthers > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 9 ], gMercProfiles[ubProfile].records.usAssistsOthers );
		AppendPersonnelText( apStr, atStr );
	}

	// Set region help text
	SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[8]), apStr );
	SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[8], MSYS_NO_CALLBACK );
}

void AssignPersonnelHitPercentageHelpText( INT32 ubProfile )
{
	if (!PersonnelRosterModel::IsValidProfileId(ubProfile, NUM_PROFILES)) return;
	CHAR16	apStr[ 1000 ];
	CHAR16	atStr[ 150 ];

	sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
	if (gMercProfiles[ubProfile].records.usShotsFired > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 10 ], gMercProfiles[ubProfile].records.usShotsFired );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usMissilesLaunched > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 11 ], gMercProfiles[ubProfile].records.usMissilesLaunched );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usGrenadesThrown > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 12 ], gMercProfiles[ubProfile].records.usGrenadesThrown );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usKnivesThrown > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 13 ], gMercProfiles[ubProfile].records.usKnivesThrown );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usBladeAttacks > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 14 ], gMercProfiles[ubProfile].records.usBladeAttacks );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usHtHAttacks > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 15 ], gMercProfiles[ubProfile].records.usHtHAttacks );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usShotsHit > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 16 ], gMercProfiles[ubProfile].records.usShotsHit );
		AppendPersonnelText( apStr, atStr );
	}
	if ( gMercProfiles[ubProfile].records.usDamageDealt > 0 || fShowRecordsIfZero )
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[52], gMercProfiles[ubProfile].records.usDamageDealt );
		AppendPersonnelText( apStr, atStr );
	}

	// Set region help text
	SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[9]), apStr );
	SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[9], MSYS_NO_CALLBACK );
}

void AssignPersonnelAchievementsHelpText( INT32 ubProfile )
{
	if (!PersonnelRosterModel::IsValidProfileId(ubProfile, NUM_PROFILES)) return;
	CHAR16	apStr[ 1000 ];
	CHAR16	atStr[ 80 ];

	sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
	if (gMercProfiles[ubProfile].records.usLocksPicked > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 17 ], gMercProfiles[ubProfile].records.usLocksPicked );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usLocksBreached > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 18 ], gMercProfiles[ubProfile].records.usLocksBreached );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usTrapsRemoved > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 19 ], gMercProfiles[ubProfile].records.usTrapsRemoved );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usExpDetonated > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 20 ], gMercProfiles[ubProfile].records.usExpDetonated );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usItemsRepaired > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 21 ], gMercProfiles[ubProfile].records.usItemsRepaired );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usItemsCombined > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 22 ], gMercProfiles[ubProfile].records.usItemsCombined );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usItemsStolen > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 23 ], gMercProfiles[ubProfile].records.usItemsStolen );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usMilitiaTrained > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 24 ], gMercProfiles[ubProfile].records.usMilitiaTrained );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usMercsBandaged > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 25 ], gMercProfiles[ubProfile].records.usMercsBandaged );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usSurgeriesMade > 0 || fShowRecordsIfZero)
	{
		if ( gGameOptions.fNewTraitSystem )
		{
			switch( gSkillTraitValues.ubDONumberTraitsNeededForSurgery )
			{
				case 0: 
					sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 26 ], gMercProfiles[ubProfile].records.usSurgeriesMade );
					AppendPersonnelText( apStr, atStr );
					break;
				case 1: 
					if ( ProfileHasSkillTrait( ubProfile, DOCTOR_NT ) > 0 )
					{
						sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 26 ], gMercProfiles[ubProfile].records.usSurgeriesMade );
						AppendPersonnelText( apStr, atStr );
					}
					break;
				case 2: 
					if ( ProfileHasSkillTrait( ubProfile, DOCTOR_NT ) > 1 )
					{
						sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 26 ], gMercProfiles[ubProfile].records.usSurgeriesMade );
						AppendPersonnelText( apStr, atStr );
					}
					break;
				default:
					break;
			}
		}
	}
	if ( gMercProfiles[ubProfile].records.usPointsHealed /100 > 0 || fShowRecordsIfZero )
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[53], gMercProfiles[ubProfile].records.usPointsHealed / 100 );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usNPCsDiscovered > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 27 ], gMercProfiles[ubProfile].records.usNPCsDiscovered );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usSectorsDiscovered > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 28 ], gMercProfiles[ubProfile].records.usSectorsDiscovered );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usAmbushesExperienced > 0 || fShowRecordsIfZero)
	{
		if ( gGameOptions.fNewTraitSystem && ( ProfileHasSkillTrait( ubProfile, SCOUTING_NT ) > 0 ) )
		{
			sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 29 ], gMercProfiles[ubProfile].records.usAmbushesExperienced );
			AppendPersonnelText( apStr, atStr );
		}
	}
	if (gMercProfiles[ubProfile].records.ubQuestsHandled > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 30 ], gMercProfiles[ubProfile].records.ubQuestsHandled );
		AppendPersonnelText( apStr, atStr );
	}
	if ( gMercProfiles[ubProfile].records.usInterrogations > 0 || fShowRecordsIfZero )
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[49], gMercProfiles[ubProfile].records.usInterrogations );
		AppendPersonnelText( apStr, atStr );
	}

	// Set region help text
	SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[10]), apStr );
	SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[10], MSYS_NO_CALLBACK );
}

void AssignPersonnelBattlesHelpText( INT32 ubProfile )
{
	if (!PersonnelRosterModel::IsValidProfileId(ubProfile, NUM_PROFILES)) return;
	CHAR16	apStr[ 400 ];
	CHAR16	atStr[ 80 ];

	sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
	if (gMercProfiles[ubProfile].records.usBattlesTactical > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 31 ], gMercProfiles[ubProfile].records.usBattlesTactical );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usBattlesAutoresolve > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 32 ], gMercProfiles[ubProfile].records.usBattlesAutoresolve );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usBattlesRetreated > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 33 ], gMercProfiles[ubProfile].records.usBattlesRetreated );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usAmbushesExperienced > 0 || fShowRecordsIfZero)
	{		
		if (!( gGameOptions.fNewTraitSystem && ( ProfileHasSkillTrait( ubProfile, SCOUTING_NT ) > 0 ) ))
		{
			sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 34 ], gMercProfiles[ubProfile].records.usAmbushesExperienced );
			AppendPersonnelText( apStr, atStr );
		}
	}
	if (gMercProfiles[ubProfile].records.usLargestBattleFought > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 35 ], gMercProfiles[ubProfile].records.usLargestBattleFought );
		AppendPersonnelText( apStr, atStr );
	}

	// Set region help text
	SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[11]), apStr );
	SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[11], MSYS_NO_CALLBACK );
	
	return;
}

void AssignPersonnelWoundsHelpText( INT32 ubProfile )
{
	if (!PersonnelRosterModel::IsValidProfileId(ubProfile, NUM_PROFILES)) return;
	CHAR16	apStr[ 500 ];
	CHAR16	atStr[ 80 ];

	sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
	if (gMercProfiles[ubProfile].records.usTimesWoundedShot > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 36 ], gMercProfiles[ubProfile].records.usTimesWoundedShot );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usTimesWoundedStabbed > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 37 ], gMercProfiles[ubProfile].records.usTimesWoundedStabbed );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usTimesWoundedPunched > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 38 ], gMercProfiles[ubProfile].records.usTimesWoundedPunched );
		AppendPersonnelText( apStr, atStr );
	}
	if (gMercProfiles[ubProfile].records.usTimesWoundedBlasted > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 39 ], gMercProfiles[ubProfile].records.usTimesWoundedBlasted );
		AppendPersonnelText( apStr, atStr );
	}
	
	if (gMercProfiles[ubProfile].records.usTimesSurgeryUndergoed > 0 || fShowRecordsIfZero)
	{
		if ( gGameOptions.fNewTraitSystem )
		{
			sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 41 ], gMercProfiles[ubProfile].records.usTimesSurgeryUndergoed );
			AppendPersonnelText( apStr, atStr );
		}
	}

	if (gMercProfiles[ubProfile].records.usFacilityAccidents > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 42 ], gMercProfiles[ubProfile].records.usFacilityAccidents );
		AppendPersonnelText( apStr, atStr );
	}

	// WANNE: Moved to the end
	if (gMercProfiles[ubProfile].records.usTimesStatDamaged > 0 || fShowRecordsIfZero)
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[ 40 ], gMercProfiles[ubProfile].records.usTimesStatDamaged );
		AppendPersonnelText( apStr, atStr );
	}

	if ( gMercProfiles[ubProfile].records.usTimesInfected > 0 || fShowRecordsIfZero )
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[50], gMercProfiles[ubProfile].records.usTimesInfected );
		AppendPersonnelText( apStr, atStr );
	}

	if ( gMercProfiles[ubProfile].records.usDamageTaken > 0 || fShowRecordsIfZero )
	{
		sgp_swprintf(atStr, std::size(atStr), pPersonnelRecordsHelpTexts[51], gMercProfiles[ubProfile].records.usDamageTaken );
		AppendPersonnelText( apStr, atStr );
	}

	// Set region help text
	SetRegionFastHelpText( &(gSkillTraitHelpTextRegion[12]), apStr );
	SetRegionHelpEndCallback( &gSkillTraitHelpTextRegion[12], MSYS_NO_CALLBACK );
}

INT8 CalculateMercsAchievementPercentage( INT32 ubProfile )
{
	if (!PersonnelRosterModel::IsValidProfileId(ubProfile, NUM_PROFILES)) return 0;
	TacticalActor *pTeamSoldier;
	std::uint64_t uiMercPoints;
	std::uint64_t ulTotalMercPoints = 0;

	// run through active soldiers
	SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	const SoldierID lastid = gTacticalStatus.Team[gbPlayerNum].bLastID;
	for ( ; id <= lastid; ++id)
	{
		pTeamSoldier =
			GetJa2SoldierRepository().resolve(id.i);
		if ( !pTeamSoldier )
			continue;
		// Only count stats of merc (not vehicles)
		if ( !( pTeamSoldier->status().flags() & SOLDIER_VEHICLE ) && !AM_A_ROBOT( pTeamSoldier ) )
		{
				if (pTeamSoldier->roster().active() &&
					pTeamSoldier->vitals().health() > 0 &&
					PersonnelRosterModel::IsValidProfileId(
						pTeamSoldier->identity().profile(), NUM_PROFILES))
			{
				const STRUCT_Records &records = gMercProfiles[pTeamSoldier->identity().profile()].records;

				// get total value of all mercs, adjust by importance
				ulTotalMercPoints += 
					( records.usLocksPicked )
					+ 
					( records.usLocksBreached )
					+ 
					( records.usTrapsRemoved *3/2)
					+ 
					( records.usExpDetonated *3/2)
					+ 
					( records.usItemsRepaired /2)
					+ 
					( records.usItemsCombined *2)
					+ 
					( records.usItemsStolen )
					+ 
					( records.usMercsBandaged *3/4)
					+ 
					( records.usSurgeriesMade *3/2)
					+ 
					( records.usNPCsDiscovered *4/3)
					+ 
					( records.usSectorsDiscovered )
					+ 
					( records.usMilitiaTrained /4)
					+ 
					( records.ubQuestsHandled *2)
					+
					( records.usInterrogations);
			}
		}
	}

	// Now get points of our mercs
	const STRUCT_Records &records = gMercProfiles[ubProfile].records;
	uiMercPoints =
		( records.usLocksPicked )
		+ 
		( records.usLocksBreached )
		+ 
		( records.usTrapsRemoved *3/2)
		+ 
		( records.usExpDetonated *3/2)
		+ 
		( records.usItemsRepaired /2)
		+ 
		( records.usItemsCombined *2)
		+ 
		( records.usItemsStolen )
		+ 
		( records.usMercsBandaged *3/4)
		+ 
		( records.usSurgeriesMade *3/2)
		+ 
		( records.usNPCsDiscovered *4/3)
		+ 
		( records.usSectorsDiscovered )
		+ 
		( records.usMilitiaTrained /4)
		+ 
		( records.ubQuestsHandled *2)
		+
		( records.usInterrogations );

	// Calculate percentage
	if( ulTotalMercPoints != 0 )
	{
		const std::uint64_t percentage =
			(uiMercPoints * 100 + ulTotalMercPoints / 2) /
			ulTotalMercPoints;
		return static_cast<INT8>(std::min<std::uint64_t>(100, percentage));
	}
	else
		return( 0 );
}

// Flugente: personality info
void AssignPersonalityHelpText( const TacticalActor* pSoldier, MOUSE_REGION* pMouseregion )
{
	CHAR16	apStr[ 4500 ];
	CHAR16	atStr[  260 ];

	sgp_swprintf(apStr, std::size(apStr), L"%s", L"" );
	
	const MERCPROFILESTRUCT* profile = ProfileFor(pSoldier);
	if (profile &&
		PersonnelRosterModel::IsValidIndex(profile->bAppearance, NUM_APPEARANCES) &&
		PersonnelRosterModel::IsValidIndex(profile->bAppearanceCareLevel, NUM_CARELEVELS) &&
		PersonnelRosterModel::IsValidIndex(profile->bRefinement, NUM_REFINEMENT) &&
		PersonnelRosterModel::IsValidIndex(profile->bRefinementCareLevel, NUM_CARELEVELS) &&
		PersonnelRosterModel::IsValidIndex(profile->bNationality, NUM_NATIONALITIES) &&
		(profile->bHatedNationality < 0 || PersonnelRosterModel::IsValidIndex(
			profile->bHatedNationality, NUM_NATIONALITIES)) &&
		PersonnelRosterModel::IsValidIndex(profile->bHatedNationalityCareLevel, NUM_CARELEVELS) &&
		PersonnelRosterModel::IsValidIndex(profile->bRacist, NUM_RACIST) &&
		PersonnelRosterModel::IsValidIndex(profile->bRace, NUM_RACES) &&
		PersonnelRosterModel::IsValidIndex(profile->bSexist, NUM_SEXIST))
	{
		sgp_swprintf(atStr, std::size(atStr), L"- %s %s %s %s %s\n", szPersonalityDisplayText[0], szAppearanceText[profile->bAppearance], szPersonalityDisplayText[1], szCareLevelText[profile->bAppearanceCareLevel], szPersonalityDisplayText[2] );
		AppendPersonnelText( apStr, atStr );

		sgp_swprintf(atStr, std::size(atStr), L"- %s %s %s %s %s\n", szPersonalityDisplayText[3], szRefinementText[profile->bRefinement], szPersonalityDisplayText[4], szCareLevelText[profile->bRefinementCareLevel], szPersonalityDisplayText[5] );
		AppendPersonnelText( apStr, atStr );

		if (profile->bHatedNationality < 0)
			sgp_swprintf(atStr, std::size(atStr), L"- %s %s %s\n", szPersonalityDisplayText[6], szNationalityText[profile->bNationality], szNationalityText_Special[0] );
		else
			sgp_swprintf(atStr, std::size(atStr), L"- %s %s %s %s %s.\n", szPersonalityDisplayText[6], szNationalityText[profile->bNationality], szPersonalityDisplayText[7], szNationalityText[profile->bHatedNationality], szCareLevelText[profile->bHatedNationalityCareLevel] );
		AppendPersonnelText( apStr, atStr );

		sgp_swprintf(atStr, std::size(atStr), L"- %s %s %s-%s %s\n", szPersonalityDisplayText[6], szRacistText[profile->bRacist], szPersonalityDisplayText[9], szRaceText[profile->bRace], szPersonalityDisplayText[10] );
		AppendPersonnelText( apStr, atStr );

		sgp_swprintf(atStr, std::size(atStr), L"- %s %s.\n", szPersonalityDisplayText[6], szSexistText[profile->bSexist] );
		AppendPersonnelText( apStr, atStr );
	}

	if ( pMouseregion )
	{
		// Set region help text
		SetRegionFastHelpText( pMouseregion, apStr );
		SetRegionHelpEndCallback( pMouseregion, MSYS_NO_CALLBACK );
	}
}
