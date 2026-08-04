#include "CampaignLaptopContentPolicy.h"
#include "GameContext.h"
#include "LaptopPageResourceOwner.h"
#include "LaptopRecordFile.h"
#include "LaptopRecordPageModel.h"

	#include "laptop.h"
	#include "history.h"
	#include "Game Clock.h"
	#include "Utilities.h"
	#include "DEBUG.H"
	#include "WordWrap.h"
	#include "Encrypted File.h"
	#include "Cursors.h"
	#include "Soldier Profile.h"
	#include "Soldier Profile Constants.h"
	#include "strategicmap.h"
	#include "QuestText.h"
	#include "Quests.h"
	#include "Text.h"
	#include "message.h"
	#include "LaptopSave.h"

#include "connect.h"

#include <limits>

#define TOP_X											LAPTOP_SCREEN_UL_X
#define TOP_Y											LAPTOP_SCREEN_UL_Y
#define BLOCK_HIST_HEIGHT					10
#define BOX_HEIGHT								14
#define TOP_DIVLINE_Y									iScreenHeightOffset + 101
#define DIVLINE_X										iScreenWidthOffset + 130
#define MID_DIVLINE_Y									iScreenHeightOffset + 155
#define BOT_DIVLINE_Y									iScreenHeightOffset + 204
#define TITLE_X											iScreenWidthOffset + 140
#define TITLE_Y											iScreenHeightOffset + 33
#define TEXT_X											iScreenWidthOffset + 140
#define PAGE_SIZE									22
#define RECORD_Y									TOP_DIVLINE_Y
#define RECORD_HISTORY_WIDTH			200
#define PAGE_NUMBER_X							TOP_X+20
#define PAGE_NUMBER_Y							TOP_Y+33
#define HISTORY_DATE_X						PAGE_NUMBER_X+85
#define HISTORY_DATE_Y						PAGE_NUMBER_Y
#define RECORD_LOCATION_WIDTH							142

#define HISTORY_HEADER_FONT FONT14ARIAL
#define HISTORY_TEXT_FONT FONT12ARIAL
#define RECORD_DATE_X TOP_X+10
#define RECORD_DATE_WIDTH								31
#define RECORD_HEADER_Y									iScreenHeightOffset + 90


#define NUM_RECORDS_PER_PAGE PAGE_SIZE
#define SIZE_OF_HISTORY_FILE_RECORD ( sizeof( UINT8 ) + sizeof( UINT8 ) + sizeof( UINT32 ) + sizeof( UINT16 ) + sizeof( UINT16 ) + sizeof( UINT8 ) + sizeof( UINT8 ) )

// button positions
#define	FIRST_PAGE_X									iScreenWidthOffset + 505
#define NEXT_BTN_X										iScreenWidthOffset + 553//577
#define PREV_BTN_X										iScreenWidthOffset + 529//553
#define	LAST_PAGE_X										iScreenWidthOffset + 577
#define BTN_Y											iScreenHeightOffset + 53

// graphics handles
UINT32 guiHistoryTitle;
//UINT32 guiGREYFRAME;
UINT32 guiHistoryTop;
//UINT32 guiMIDDLE;
//UINT32 guiBOTTOM;
//UINT32 guiLINE;
UINT32 guiHistoryLongLine;
UINT32 guiHistoryShadeLine;
//UINT32 guiVERTLINE;
//UINT32 guiBIGBOX;

namespace
{
using ContentPolicy = CampaignLaptopContentPolicy;
constexpr LaptopRecordPageModel::FileLayout HistoryFileLayout{
	0, SIZE_OF_HISTORY_FILE_RECORD};
constexpr UINT32 InvalidHistoryRecordId = UINT32_MAX;

struct HistoryRecordData
{
	UINT8 code = 0;
	UINT8 secondCode = 0;
	UINT32 date = 0;
	INT16 sectorX = 0;
	INT16 sectorY = 0;
	INT8 sectorZ = 0;
	UINT8 color = 0;
};

bool ReadHistoryRecordExact(HWFILE file, HistoryRecordData& record)
{
	return ReadLaptopFileExact(file, &record.code, sizeof(record.code)) &&
		ReadLaptopFileExact(file, &record.secondCode,
			sizeof(record.secondCode)) &&
		ReadLaptopFileExact(file, &record.date, sizeof(record.date)) &&
		ReadLaptopFileExact(file, &record.sectorX, sizeof(record.sectorX)) &&
		ReadLaptopFileExact(file, &record.sectorY, sizeof(record.sectorY)) &&
		ReadLaptopFileExact(file, &record.sectorZ, sizeof(record.sectorZ)) &&
		ReadLaptopFileExact(file, &record.color, sizeof(record.color));
}

bool WriteHistoryRecordExact(HWFILE file, const HistoryUnit& record)
{
	return WriteLaptopFileExact(file, &record.ubCode, sizeof(record.ubCode)) &&
		WriteLaptopFileExact(file, &record.ubSecondCode,
			sizeof(record.ubSecondCode)) &&
		WriteLaptopFileExact(file, &record.uiDate, sizeof(record.uiDate)) &&
		WriteLaptopFileExact(file, &record.sSectorX, sizeof(record.sSectorX)) &&
		WriteLaptopFileExact(file, &record.sSectorY, sizeof(record.sSectorY)) &&
		WriteLaptopFileExact(file, &record.bSectorZ, sizeof(record.bSectorZ)) &&
		WriteLaptopFileExact(file, &record.ubColor, sizeof(record.ubColor));
}

std::size_t HistoryRecordCountOnDisk()
{
	if (!FileExists(HISTORY_DATA_FILE)) return 0;
	ScopedLaptopFile file(FileOpen(HISTORY_DATA_FILE,
		FILE_OPEN_EXISTING | FILE_ACCESS_READ, FALSE));
	if (!file) return 0;
	return LaptopRecordPageModel::RecordCount(
		FileGetSize(file.Get()), HistoryFileLayout);
}

std::size_t HistoryRecordPageCountOnDisk()
{
	return LaptopRecordPageModel::PageCount(
		HistoryRecordCountOnDisk(), NUM_RECORDS_PER_PAGE);
}

ContentPolicy CurrentLaptopContentPolicy()
{
	return ContentPolicy(GetGameContext().capabilities());
}

ContentPolicy::QuestTextRecord CurrentQuestTextRecord(
	UINT8 quest, bool completed)
{
	const ContentPolicy policy = CurrentLaptopContentPolicy();
	return policy.questTextRecord(quest, completed,
		FileExists(ContentPolicy::unfinishedBusinessQuestTextPath()));
}
}

enum{
	PREV_PAGE_BUTTON=0,
	NEXT_PAGE_BUTTON,
	FIRST_PAGE_BUTTON,
	LAST_PAGE_BUTTON,
};

HISTORY_VALUES HistoryName[500];

// the page flipping buttons
INT32 giHistoryButton[4];
INT32 giHistoryButtonImage[4];
LaptopPageResourceOwner gHistoryPageResources;
BOOLEAN fInHistoryMode=FALSE;


// current page displayed
INT32 iCurrentHistoryPage=1;



// the History record list
HistoryUnitPtr pHistoryListHead=NULL;

// current History record (the one at the top of the current page)
HistoryUnitPtr pCurrentHistory=NULL;


// number of persisted history pages (the display remains page one when empty)
std::size_t gHistoryRecordPageCount = 0;

// function definitions
BOOLEAN LoadHistory(LaptopPageResourceOwner& owner);
void RenderHistoryBackGround( void );
BOOLEAN CreateHistoryButtons(LaptopPageResourceOwner& owner);
void DrawHistoryTitleText( void );
UINT32 ProcessAndEnterAHistoryRecord( UINT8 ubCode, UINT32 uiDate, UINT8 ubSecondCode, INT16 sSectorX, INT16 sSectorY, INT8 bSectorZ, UINT8 ubColor );
BOOLEAN OpenAndReadHistoryFile( void );
BOOLEAN OpenAndWriteHistoryFile( void );
void ClearHistoryList( void );
void DisplayHistoryListHeaders( void );
void DisplayHistoryListBackground( void );
void DrawAPageofHistoryRecords( void );
void DisplayPageNumberAndDateRange( void );
void ProcessHistoryTransactionString(CHAR16 *pString, HistoryUnitPtr pHistory);
void SetHistoryButtonStates( void );
BOOLEAN LoadInHistoryRecords( UINT32 uiPage );
BOOLEAN LoadNextHistoryPage( void );
BOOLEAN LoadPreviousHistoryPage( void );
BOOLEAN AppendHistoryToEndOfFile(const HistoryUnit& historyRecord);
void		GetQuestStartedString( UINT8 ubQuestValue, CHAR16 *sQuestString );
void		GetQuestEndedString( UINT8 ubQuestValue, CHAR16 *sQuestString );


#ifdef JA2TESTVERSION
void PerformCheckOnHistoryRecord( UINT32 uiErrorCode, INT16 sSectorX, INT16 sSectorY, INT8 bSectorZ );
#endif


// callbacks
void BtnHistoryDisplayNextPageCallBack(GUI_BUTTON *btn,INT32 reason);
void BtnHistoryDisplayPrevPageCallBack(GUI_BUTTON *btn,INT32 reason);
void BtnHistoryFirstLastPageCallBack(GUI_BUTTON *btn,INT32 reason);

namespace
{
UINT32 AddPersistedHistoryRecord(UINT8 code, UINT8 secondCode,
	UINT32 date, INT16 sectorX, INT16 sectorY, UINT8 color)
{
	const std::size_t existingRecords = HistoryRecordCountOnDisk();
	ClearHistoryList();
	const UINT32 id = ProcessAndEnterAHistoryRecord(
		code, date, secondCode, sectorX, sectorY, 0, color);
	if (id == InvalidHistoryRecordId || !pHistoryListHead)
	{
		ClearHistoryList();
		Assert(0);
		return InvalidHistoryRecordId;
	}
	pHistoryListHead->uiIdNumber = static_cast<UINT32>(existingRecords);
	if (!AppendHistoryToEndOfFile(*pHistoryListHead))
	{
		ClearHistoryList();
		Assert(0);
		return InvalidHistoryRecordId;
	}

	ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE,
		pMessageStrings[MSG_HISTORY_UPDATED]);
	gHistoryRecordPageCount = HistoryRecordPageCountOnDisk();
	if (fInHistoryMode)
	{
		iCurrentHistoryPage = static_cast<INT32>(
			LaptopRecordPageModel::NormalizeOneBasedPage(
				static_cast<std::size_t>(iCurrentHistoryPage < 1
					? 1 : iCurrentHistoryPage),
				gHistoryRecordPageCount));
		LoadInHistoryRecords(static_cast<UINT32>(iCurrentHistoryPage));
		SetHistoryButtonStates();
		fReDrawScreenFlag = TRUE;
	}
	else
	{
		ClearHistoryList();
	}
	return static_cast<UINT32>(existingRecords);
}
}

UINT32 SetHistoryFact( UINT8 ubCode, UINT8 ubSecondCode, UINT32 uiDate, INT16 sSectorX, INT16 sSectorY )
{
	return AddPersistedHistoryRecord(ubCode, ubSecondCode, uiDate,
		sSectorX, sSectorY, ubCode == HISTORY_QUEST_FINISHED ? 0 : 1);
}


UINT32 AddHistoryToPlayersLog(UINT8 ubCode, UINT8 ubSecondCode, UINT32 uiDate, INT16 sSectorX, INT16 sSectorY)
{
	return AddPersistedHistoryRecord(ubCode, ubSecondCode, uiDate,
		sSectorX, sSectorY, 0);
}


void GameInitHistory()
{
	if( ( FileExists( HISTORY_DATA_FILE ) ) )
	{
	// unlink history file
	FileDelete( HISTORY_DATA_FILE );
	}

	if (!is_networked)
		AddHistoryToPlayersLog(HISTORY_ACCEPTED_ASSIGNMENT_FROM_ENRICO, 0, GetWorldTotalMin( ), -1, -1);
}

void EnterHistory()
{
	LaptopPageResourceOwner staged;
	gHistoryPageResources.clear();
	fInHistoryMode=FALSE;

	// load the graphics
	if (!LoadHistory(staged)) return;

	// create History buttons
	if (!CreateHistoryButtons(staged)) return;
	gHistoryPageResources = std::move(staged);

	// Normalize stale saved pages before exposing the staged page resources.
	gHistoryRecordPageCount = HistoryRecordPageCountOnDisk();
	iCurrentHistoryPage = static_cast<INT32>(
		LaptopRecordPageModel::NormalizeOneBasedPage(
			static_cast<std::size_t>(LaptopSaveInfo.iCurrentHistoryPage > 0
				? LaptopSaveInfo.iCurrentHistoryPage : 1),
			gHistoryRecordPageCount));
	ClearHistoryList();
	if (gHistoryRecordPageCount > 0 &&
		!LoadInHistoryRecords(static_cast<UINT32>(iCurrentHistoryPage)))
	{
		iCurrentHistoryPage = 1;
		ClearHistoryList();
	}


	// render hbackground
	RenderHistory( );


	// set the fact we are in the history viewer
	fInHistoryMode=TRUE;

	// build Historys list
	//OpenAndReadHistoryFile( );

	// force redraw of the entire screen
	//fReDrawScreenFlag=TRUE;

	// set inital states
	SetHistoryButtonStates( );

	return;
}

void ExitHistory()
{
	LaptopSaveInfo.iCurrentHistoryPage = iCurrentHistoryPage;

	// not in History system anymore
	fInHistoryMode=FALSE;


	// write out history list to file
	//OpenAndWriteHistoryFile( );

	gHistoryPageResources.clear();

	ClearHistoryList( );


	return;
}

void HandleHistory()
{
	// DEF 2/5/99 Dont need to update EVERY FRAME!!!!
	// check and update status of buttons
//	SetHistoryButtonStates( );
}

void RenderHistory( void )
{
	//render the background to the display
	RenderHistoryBackGround( );

	// the title bar text
	DrawHistoryTitleText( );

	// the actual lists background
	DisplayHistoryListBackground( );

	// the headers to each column
	DisplayHistoryListHeaders( );

	// render the currentpage of records
	DrawAPageofHistoryRecords( );

	// stuff at top of page, the date range and page numbers
	DisplayPageNumberAndDateRange( );

	// title bar icon
	BlitTitleBarIcons(	);

	return;
}


BOOLEAN LoadHistory(LaptopPageResourceOwner& owner)
{
	VOBJECT_DESC	VObjectDesc;
	// load History video objects into memory

	// title bar
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\programtitlebar.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiHistoryTitle)) return FALSE;

	// top portion of the screen background
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\historywindow.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiHistoryTop)) return FALSE;


	// shaded line
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\historylines.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc,
		guiHistoryShadeLine)) return FALSE;

	// black divider line - long ( 480 length)
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\divisionline480.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc,
		guiHistoryLongLine)) return FALSE;

	return (TRUE);
}

void RenderHistoryBackGround( void )
{
	// render generic background for history system
	HVOBJECT hHandle;

	// get title bar object
	GetVideoObject(&hHandle, guiHistoryTitle);

	// blt title bar to screen
	BltVideoObject(FRAME_BUFFER, hHandle, 0,TOP_X, TOP_Y -2 , VO_BLT_SRCTRANSPARENCY,NULL);


	// get and blt the top part of the screen, video object and blt to screen
	GetVideoObject(&hHandle, guiHistoryTop);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,TOP_X, TOP_Y + 22, VO_BLT_SRCTRANSPARENCY,NULL);

	// display background for history list
	DisplayHistoryListBackground( );
		return;
}

void DrawHistoryTitleText( void )
{
	// setup the font stuff
	SetFont(HISTORY_HEADER_FONT);
	SetFontForeground(FONT_WHITE);
	SetFontBackground(FONT_BLACK);
	SetFontShadow(DEFAULT_SHADOW);

	// draw the pages title
	mprintf(TITLE_X, TITLE_Y, L"%s", pHistoryTitle[0]);

	return;
}

BOOLEAN CreateHistoryButtons(LaptopPageResourceOwner& owner)
{

	// the prev page button
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\arrows.sti", -1, 0, -1, 1, -1),
		giHistoryButtonImage[PREV_PAGE_BUTTON])) return FALSE;
	if (!owner.addButton(QuickCreateButton( giHistoryButtonImage[PREV_PAGE_BUTTON], PREV_BTN_X, BTN_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)BtnHistoryDisplayPrevPageCallBack),
		giHistoryButton[PREV_PAGE_BUTTON])) return FALSE;

	// the next page button
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\arrows.sti", -1, 6, -1, 7, -1),
		giHistoryButtonImage[NEXT_PAGE_BUTTON])) return FALSE;
	if (!owner.addButton(QuickCreateButton( giHistoryButtonImage[NEXT_PAGE_BUTTON], NEXT_BTN_X, BTN_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
											(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)BtnHistoryDisplayNextPageCallBack),
		giHistoryButton[NEXT_PAGE_BUTTON])) return FALSE;

	//button to go to the first page
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\arrows.sti", -1, 3, -1, 4, -1),
		giHistoryButtonImage[FIRST_PAGE_BUTTON])) return FALSE;
	if (!owner.addButton(QuickCreateButton( giHistoryButtonImage[FIRST_PAGE_BUTTON], FIRST_PAGE_X, BTN_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
											(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)BtnHistoryFirstLastPageCallBack),
		giHistoryButton[FIRST_PAGE_BUTTON])) return FALSE;
	
	MSYS_SetBtnUserData( giHistoryButton[FIRST_PAGE_BUTTON], 0, 0 );

	//button to go to the last page
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\arrows.sti", -1, 9, -1, 10, -1),
		giHistoryButtonImage[LAST_PAGE_BUTTON])) return FALSE;
	if (!owner.addButton(QuickCreateButton( giHistoryButtonImage[LAST_PAGE_BUTTON], LAST_PAGE_X, BTN_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
											(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)BtnHistoryFirstLastPageCallBack),
		giHistoryButton[LAST_PAGE_BUTTON])) return FALSE;

	MSYS_SetBtnUserData( giHistoryButton[LAST_PAGE_BUTTON], 0, 1 );

	// set buttons
	SetButtonCursor(giHistoryButton[0], CURSOR_LAPTOP_SCREEN);
	SetButtonCursor(giHistoryButton[1], CURSOR_LAPTOP_SCREEN);
	SetButtonCursor(giHistoryButton[2], CURSOR_LAPTOP_SCREEN);
	SetButtonCursor(giHistoryButton[3], CURSOR_LAPTOP_SCREEN);

	return TRUE;
}

void BtnHistoryDisplayPrevPageCallBack(GUI_BUTTON *btn,INT32 reason)
{
	// force redraw
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		fReDrawScreenFlag=TRUE;
	}


	// force redraw
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		fReDrawScreenFlag=TRUE;
		btn->uiFlags&=~(BUTTON_CLICKED_ON);
		// this page is > 0, there are pages before it, decrement

		if(iCurrentHistoryPage > 0)
		{
			LoadPreviousHistoryPage( );
			//iCurrentHistoryPage--;
			DrawAPageofHistoryRecords( );
		}

		// set new state
		SetHistoryButtonStates( );
	}


}

void BtnHistoryDisplayNextPageCallBack(GUI_BUTTON *btn,INT32 reason)
{

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		fReDrawScreenFlag=TRUE;
	}


	// force redraw
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		// increment currentPage
		btn->uiFlags&=~(BUTTON_CLICKED_ON);
		LoadNextHistoryPage( );
		// set new state
		SetHistoryButtonStates( );
		fReDrawScreenFlag=TRUE;
	}



}

void BtnHistoryFirstLastPageCallBack(GUI_BUTTON *btn,INT32 reason)
{
	// force redraw
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		fReDrawScreenFlag=TRUE;
	}

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		UINT32	uiButton = MSYS_GetBtnUserData( btn, 0 );

		btn->uiFlags&=~(BUTTON_CLICKED_ON);

		// clear out old list of records, and load in previous page worth of records
		ClearHistoryList( );

		//if its the first page button
		if( uiButton == 0 )
		{
			iCurrentHistoryPage = 1;
			if (gHistoryRecordPageCount > 0)
				LoadInHistoryRecords(iCurrentHistoryPage);
			DrawAPageofHistoryRecords( );
		}

		//else its the last page button
		else
		{
			iCurrentHistoryPage = static_cast<INT32>(
				gHistoryRecordPageCount > 0 ? gHistoryRecordPageCount : 1);
			if (gHistoryRecordPageCount > 0)
				LoadInHistoryRecords(static_cast<UINT32>(iCurrentHistoryPage));
		}

		// set button state
		SetHistoryButtonStates( );

		pCurrentHistory=pHistoryListHead;
		// redraw screen
		fReDrawScreenFlag=TRUE;
	}
}


UINT32 ProcessAndEnterAHistoryRecord( UINT8 ubCode, UINT32 uiDate, UINT8 ubSecondCode, INT16 sSectorX, INT16 sSectorY, INT8 bSectorZ , UINT8 ubColor )
{
	UINT32 uiId=0;
	HistoryUnitPtr pHistory=pHistoryListHead;

 	// add to History list
	if(pHistory)
	{
		// go to end of list
		while(pHistory->Next)
			pHistory=pHistory->Next;

		// alloc space
		pHistory->Next = (history *) MemAlloc(sizeof(HistoryUnit));
		if (!pHistory->Next) return InvalidHistoryRecordId;

		// increment id number
		uiId = pHistory->uiIdNumber + 1;

		// set up information passed
		pHistory = pHistory->Next;
		pHistory->Next = NULL;
		pHistory->ubCode = ubCode;
		pHistory->ubSecondCode = ubSecondCode;
		pHistory->uiDate = uiDate;
		pHistory->uiIdNumber = uiId;
		pHistory->sSectorX = sSectorX;
		pHistory->sSectorY = sSectorY;
		pHistory->bSectorZ = bSectorZ;
		pHistory->ubColor = ubColor;

	}
	else
	{
		// alloc space
		pHistory = (HistoryUnitPtr) MemAlloc(sizeof(HistoryUnit));
		if (!pHistory) return InvalidHistoryRecordId;

		// setup info passed
		pHistory->Next = NULL;
		pHistory->ubCode = ubCode;
		pHistory->ubSecondCode = ubSecondCode;
		pHistory->uiDate = uiDate;
		pHistory->uiIdNumber = uiId;
		pHistoryListHead = pHistory;
		pHistory->sSectorX = sSectorX;
		pHistory->sSectorY = sSectorY;
		pHistory->bSectorZ = bSectorZ;
		pHistory->ubColor = ubColor;

	}

	return uiId;
}


BOOLEAN OpenAndReadHistoryFile( void )
{
	ClearHistoryList();
	if (!FileExists(HISTORY_DATA_FILE)) return TRUE;
	ScopedLaptopFile file(FileOpen(HISTORY_DATA_FILE,
		FILE_OPEN_EXISTING | FILE_ACCESS_READ, FALSE));
	if (!file) return FALSE;
	const std::size_t fileBytes = FileGetSize(file.Get());
	if (!LaptopRecordPageModel::IsWellFormedFile(
		fileBytes, HistoryFileLayout)) return FALSE;
	const std::size_t count = LaptopRecordPageModel::RecordCount(
		fileBytes, HistoryFileLayout);
	for (std::size_t index = 0; index < count; ++index)
	{
		HistoryRecordData record;
		if (!ReadHistoryRecordExact(file.Get(), record))
		{
			ClearHistoryList();
			return FALSE;
		}

		#ifdef JA2TESTVERSION
		PerformCheckOnHistoryRecord(1, record.sectorX,
			record.sectorY, record.sectorZ);
		#endif

		if (ProcessAndEnterAHistoryRecord(record.code, record.date,
			record.secondCode, record.sectorX, record.sectorY,
			record.sectorZ, record.color) == InvalidHistoryRecordId)
		{
			ClearHistoryList();
			return FALSE;
		}
	}
	pCurrentHistory = pHistoryListHead;
	return TRUE;
}

BOOLEAN OpenAndWriteHistoryFile( void )
{
	if (!FileExists(HISTORY_DATA_FILE)) return FALSE;
	ScopedLaptopFile file(FileOpen(HISTORY_DATA_FILE,
		FILE_OPEN_EXISTING | FILE_ACCESS_WRITE, FALSE));
	if (!file) return FALSE;
	const std::size_t recordCount = LaptopRecordPageModel::RecordCount(
		FileGetSize(file.Get()), HistoryFileLayout);
	std::size_t listCount = 0;
	for (HistoryUnitPtr record = pHistoryListHead;
		record; record = record->Next) ++listCount;
	if (recordCount != listCount ||
		!LaptopRecordPageModel::IsWellFormedFile(
			FileGetSize(file.Get()), HistoryFileLayout) ||
		!FileSeek(file.Get(), 0, FILE_SEEK_FROM_START)) return FALSE;

	for (HistoryUnitPtr record = pHistoryListHead;
		record; record = record->Next)
	{
		#ifdef JA2TESTVERSION
		PerformCheckOnHistoryRecord(2, record->sSectorX,
			record->sSectorY, record->bSectorZ);
		#endif
		if (!WriteHistoryRecordExact(file.Get(), *record)) return FALSE;
	}
	ClearHistoryList();
	return TRUE;
}


void ClearHistoryList( void )
{
	// remove each element from list of transactions

	HistoryUnitPtr pHistoryList=pHistoryListHead;
	HistoryUnitPtr pHistoryNode;

	// while there are elements in the list left, delete them
	while( pHistoryList )
	{
	// set node to list head
		pHistoryNode=pHistoryList;

		// set list head to next node
		pHistoryList=pHistoryList->Next;

		// delete current node
		MemFree(pHistoryNode);
	}
	pHistoryListHead=NULL;
	pCurrentHistory=NULL;

	return;
}

void DisplayHistoryListHeaders( void )
{
	// this procedure will display the headers to each column in History
	INT16 usX, usY;

	// font stuff
	SetFont(HISTORY_TEXT_FONT);
	SetFontForeground(FONT_BLACK);
	SetFontBackground(FONT_BLACK);
	SetFontShadow(NO_SHADOW);

	// the date header
	FindFontCenterCoordinates(RECORD_DATE_X + 5,0,RECORD_DATE_WIDTH,0, pHistoryHeaders[0], HISTORY_TEXT_FONT,&usX, &usY);
	mprintf(usX, RECORD_HEADER_Y, L"%s", pHistoryHeaders[0]);

	// the date header
	FindFontCenterCoordinates(RECORD_DATE_X + RECORD_DATE_WIDTH + 5,0,RECORD_LOCATION_WIDTH,0, pHistoryHeaders[ 3 ], HISTORY_TEXT_FONT,&usX, &usY);
	mprintf(usX, RECORD_HEADER_Y, L"%s", pHistoryHeaders[3]);

	// event header
	FindFontCenterCoordinates(RECORD_DATE_X + RECORD_DATE_WIDTH + RECORD_LOCATION_WIDTH + 5,0,RECORD_LOCATION_WIDTH,0, pHistoryHeaders[ 3 ], HISTORY_TEXT_FONT,&usX, &usY);
	mprintf(usX, RECORD_HEADER_Y, L"%s", pHistoryHeaders[4]);
	// reset shadow
	SetFontShadow(DEFAULT_SHADOW);
	return;
}


void DisplayHistoryListBackground( void )
{
	// this function will display the History list display background
	HVOBJECT hHandle;
	INT32 iCounter=0;



	// get shaded line object
	GetVideoObject(&hHandle, guiHistoryShadeLine);
	for(iCounter=0; iCounter <11; iCounter++)
	{
	// blt title bar to screen
	BltVideoObject(FRAME_BUFFER, hHandle, 0,TOP_X + 15, (TOP_DIVLINE_Y + BOX_HEIGHT * 2 * iCounter), VO_BLT_SRCTRANSPARENCY,NULL);
	}

	// the long hortizontal line int he records list display region
	GetVideoObject(&hHandle, guiHistoryLongLine);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,TOP_X + 9, (TOP_DIVLINE_Y ), VO_BLT_SRCTRANSPARENCY,NULL);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,TOP_X + 9, (TOP_DIVLINE_Y + BOX_HEIGHT * 2 * 11	), VO_BLT_SRCTRANSPARENCY,NULL);


	return;
}

void DrawHistoryRecordsText( void )
{
	// draws the text of the records
	HistoryUnitPtr pCurHistory=pHistoryListHead;
	CHAR16 sString[512];
	INT32 iCounter=0;
	INT16 usX, usY;
	INT16 sX =0, sY =0;

	// setup the font stuff
	SetFont(HISTORY_TEXT_FONT);
	SetFontForeground(FONT_BLACK);
	SetFontBackground(FONT_BLACK);
	SetFontShadow(NO_SHADOW);

	// error check
	if( !pCurHistory)
		return;


	// loop through record list
	for( ; iCounter <NUM_RECORDS_PER_PAGE; ++iCounter)
	{
		if( pCurHistory->ubColor == 0 )
		{
			SetFontForeground(FONT_BLACK);
		}
		else
		{
			SetFontForeground(FONT_RED);
		}

		// get and write the date
		swprintf(sString, L"%d", ( pCurHistory->uiDate / ( 24 * 60 ) ) );
		FindFontCenterCoordinates(RECORD_DATE_X + 5, 0, RECORD_DATE_WIDTH,0, sString, HISTORY_TEXT_FONT,&usX, &usY);
		mprintf(usX, RECORD_Y + ( iCounter * ( BOX_HEIGHT ) ) + 3,
			L"%s", sString);

		// now the actual history text
		ProcessHistoryTransactionString(sString, pCurHistory);
		mprintf(RECORD_DATE_X + RECORD_LOCATION_WIDTH +RECORD_DATE_WIDTH + 15,
			RECORD_Y + ( iCounter * ( BOX_HEIGHT ) ) + 3, L"%s", sString );
		
		// no location
		if( ( pCurHistory->sSectorX == -1 )||( pCurHistory->sSectorY == -1 ) )
		{
			FindFontCenterCoordinates( RECORD_DATE_X + RECORD_DATE_WIDTH, 0,RECORD_LOCATION_WIDTH + 10, 0,	pHistoryLocations[0] ,HISTORY_TEXT_FONT, &sX, &sY );
			mprintf(sX, RECORD_Y + ( iCounter * ( BOX_HEIGHT ) ) + 3,
				L"%s", pHistoryLocations[0] );
		}
		else
		{
			GetSectorIDString( pCurHistory->sSectorX, pCurHistory->sSectorY, pCurHistory->bSectorZ, sString, TRUE );
			FindFontCenterCoordinates( RECORD_DATE_X + RECORD_DATE_WIDTH, 0, RECORD_LOCATION_WIDTH + 10, 0,	sString ,HISTORY_TEXT_FONT, &sX, &sY );

			ReduceStringLength( sString, RECORD_LOCATION_WIDTH + 10, HISTORY_TEXT_FONT );

			mprintf(sX, RECORD_Y + ( iCounter * ( BOX_HEIGHT ) ) + 3,
				L"%s", sString );
		}

		// restore font color
		SetFontForeground(FONT_BLACK);

		// next History
		pCurHistory = pCurHistory->Next;

		// last page, no Historys left, return
		if( ! pCurHistory )
		{
			// restore shadow
			SetFontShadow(DEFAULT_SHADOW);
			return;
		}
	}

	// restore shadow
	SetFontShadow(DEFAULT_SHADOW);
}


void DrawAPageofHistoryRecords( void )
{
	// this procedure will draw a series of history records to the screen
	pCurrentHistory=pHistoryListHead;

	// (re-)render background

	// the title bar text
	DrawHistoryTitleText( );

	// the actual lists background
	DisplayHistoryListBackground( );

	// the headers to each column
	DisplayHistoryListHeaders( );


	// error check
	if(iCurrentHistoryPage==-1)
	{
		iCurrentHistoryPage=0;
	}


	// current page is found, render	from here
	DrawHistoryRecordsText( );

	// update page numbers, and date ranges
	DisplayPageNumberAndDateRange( );

	return;
}

void DisplayPageNumberAndDateRange( void )
{
	// this function will go through the list of 'histories' starting at current until end or
	// MAX_PER_PAGE...it will get the date range and the page number
	INT32 iCounter=0;
	UINT32 uiLastDate;
	HistoryUnitPtr pTempHistory;
	CHAR16 sString[50];



	// setup the font stuff
	SetFont(HISTORY_TEXT_FONT);
	SetFontForeground(FONT_BLACK);
	SetFontBackground(FONT_BLACK);
	SetFontShadow(NO_SHADOW);

	if( !pCurrentHistory )
	{
	swprintf( sString, L"%s %d / %d",pHistoryHeaders[1], 1, 1 );
	mprintf(PAGE_NUMBER_X, PAGE_NUMBER_Y, L"%s", sString);

	swprintf( sString, L"%s %d - %d",pHistoryHeaders[2], 1 , 1 );
	mprintf(HISTORY_DATE_X, HISTORY_DATE_Y, L"%s", sString);

	// reset shadow
	SetFontShadow(DEFAULT_SHADOW);

		return;
	}

	uiLastDate=pCurrentHistory->uiDate;

/*
	// find last page
	while(pTempHistory)
	{
		iCounter++;
		pTempHistory=pTempHistory->Next;
	}

	// set last page
	iLastPage=iCounter/NUM_RECORDS_PER_PAGE;
*/

	// set temp to current, to get last date
	pTempHistory=pCurrentHistory;

	// reset counter
	iCounter=0;

	// run through list until end or num_records, which ever first
	while((pTempHistory)&&(iCounter < NUM_RECORDS_PER_PAGE))
	{
		uiLastDate=pTempHistory->uiDate;
		iCounter++;

		pTempHistory = pTempHistory->Next;
	}



	// get the last page

	const std::size_t displayedPageCount =
		gHistoryRecordPageCount > 0 ? gHistoryRecordPageCount : 1;
	swprintf( sString, L"%s %d / %d",pHistoryHeaders[1],
		iCurrentHistoryPage, static_cast<INT32>(displayedPageCount));
	mprintf(PAGE_NUMBER_X, PAGE_NUMBER_Y, L"%s", sString);

	swprintf( sString, L"%s %d - %d",pHistoryHeaders[2], pCurrentHistory->uiDate / ( 24 * 60 ) , uiLastDate/( 24 * 60 ) );
	mprintf(HISTORY_DATE_X, HISTORY_DATE_Y, L"%s", sString);


	// reset shadow
	SetFontShadow(DEFAULT_SHADOW);

	return;
}


void ProcessHistoryTransactionString(CHAR16 *pString, HistoryUnitPtr pHistory)
{
	CHAR16 sString[ 128 ];
	const std::size_t profile = LaptopRecordPageModel::BoundedIndex(
		pHistory->ubSecondCode, NUM_PROFILES);
	const std::size_t town = LaptopRecordPageModel::BoundedIndex(
		pHistory->ubSecondCode, MAX_TOWNS);
	const std::size_t quest = LaptopRecordPageModel::BoundedIndex(
		pHistory->ubSecondCode, MAX_QUESTS);

	switch( pHistory->ubCode)
	{
		case HISTORY_ENTERED_HISTORY_MODE:
			sgp_swprintf(pString, 512, L"%s",
				HistoryName[HISTORY_ENTERED_HISTORY_MODE].sHistory);
			break;

		case HISTORY_HIRED_MERC_FROM_AIM:
			//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_HIRED_MERC_FROM_AIM ], gMercProfiles[pHistory->ubSecondCode].zName	);
			sgp_swprintf(pString, 512,HistoryName[ HISTORY_HIRED_MERC_FROM_AIM ].sHistory, gMercProfiles[profile].zName	);
			break;

		case HISTORY_MERC_KILLED:
			if( pHistory->ubSecondCode != NO_PROFILE )
				//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_MERC_KILLED ], gMercProfiles[pHistory->ubSecondCode].zName );
				sgp_swprintf(pString, 512,HistoryName[ HISTORY_MERC_KILLED ].sHistory, gMercProfiles[profile].zName );
			else
			{
				//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_MERC_KILLED ], L"ERROR!!!	NO_PROFILE" );
				sgp_swprintf(pString, 512,HistoryName[ HISTORY_MERC_KILLED ].sHistory, L"ERROR!!!	NO_PROFILE" );
			}
			break;

		case HISTORY_HIRED_MERC_FROM_MERC:
			//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_HIRED_MERC_FROM_MERC ],	gMercProfiles[pHistory->ubSecondCode].zName );
			sgp_swprintf(pString, 512,HistoryName[ HISTORY_HIRED_MERC_FROM_MERC ].sHistory,	gMercProfiles[profile].zName );
			break;

		case HISTORY_SETTLED_ACCOUNTS_AT_MERC:
			sgp_swprintf(pString, 512, L"%s",
				HistoryName[HISTORY_SETTLED_ACCOUNTS_AT_MERC].sHistory);
			break;
		case HISTORY_ACCEPTED_ASSIGNMENT_FROM_ENRICO:
			sgp_swprintf(pString, 512, L"%s",
				HistoryName[HISTORY_ACCEPTED_ASSIGNMENT_FROM_ENRICO].sHistory);
			break;
		case( HISTORY_CHARACTER_GENERATED ):
			sgp_swprintf(pString, 512, L"%s",
				HistoryName[HISTORY_CHARACTER_GENERATED].sHistory);
		break;
		case( HISTORY_PURCHASED_INSURANCE ):
			//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_PURCHASED_INSURANCE ], gMercProfiles[pHistory->ubSecondCode].zNickname );
			sgp_swprintf(pString, 512,HistoryName[ HISTORY_PURCHASED_INSURANCE ].sHistory, gMercProfiles[profile].zNickname );
		break;
		case( HISTORY_CANCELLED_INSURANCE ):
			//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_CANCELLED_INSURANCE ], gMercProfiles[pHistory->ubSecondCode].zNickname );
			sgp_swprintf(pString, 512,HistoryName[ HISTORY_CANCELLED_INSURANCE ].sHistory, gMercProfiles[profile].zNickname );
		break;
		case( HISTORY_INSURANCE_CLAIM_PAYOUT ):
			//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_INSURANCE_CLAIM_PAYOUT ], gMercProfiles[pHistory->ubSecondCode].zNickname );
			sgp_swprintf(pString, 512,HistoryName[ HISTORY_INSURANCE_CLAIM_PAYOUT ].sHistory, gMercProfiles[profile].zNickname );
		break;

		case HISTORY_EXTENDED_CONTRACT_1_DAY:
			//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_EXTENDED_CONTRACT_1_DAY ], gMercProfiles[pHistory->ubSecondCode].zNickname );
			sgp_swprintf(pString, 512,HistoryName[ HISTORY_EXTENDED_CONTRACT_1_DAY ].sHistory, gMercProfiles[profile].zNickname );
			break;

		case HISTORY_EXTENDED_CONTRACT_1_WEEK:
			//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_EXTENDED_CONTRACT_1_WEEK ], gMercProfiles[pHistory->ubSecondCode].zNickname );
			sgp_swprintf(pString, 512,HistoryName[ HISTORY_EXTENDED_CONTRACT_1_WEEK ].sHistory, gMercProfiles[profile].zNickname );
			break;

		case HISTORY_EXTENDED_CONTRACT_2_WEEK:
			//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_EXTENDED_CONTRACT_2_WEEK ], gMercProfiles[pHistory->ubSecondCode].zNickname );
			sgp_swprintf(pString, 512,HistoryName[ HISTORY_EXTENDED_CONTRACT_2_WEEK ].sHistory, gMercProfiles[profile].zNickname );
			break;

		case( HISTORY_MERC_FIRED ):
			//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_MERC_FIRED ], gMercProfiles[pHistory->ubSecondCode].zNickname );
			sgp_swprintf(pString, 512,HistoryName[ HISTORY_MERC_FIRED ].sHistory, gMercProfiles[profile].zNickname );
		break;

		case( HISTORY_MERC_QUIT ):
			//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_MERC_QUIT ], gMercProfiles[pHistory->ubSecondCode].zNickname );
			sgp_swprintf(pString, 512,HistoryName[ HISTORY_MERC_QUIT ].sHistory, gMercProfiles[profile].zNickname );
		break;

		case( HISTORY_QUEST_STARTED ):
			GetQuestStartedString(static_cast<UINT8>(quest), sString);
			sgp_swprintf(pString, 512, L"%s", sString);

		break;
		case( HISTORY_QUEST_FINISHED ):
			GetQuestEndedString(static_cast<UINT8>(quest), sString);
			sgp_swprintf(pString, 512, L"%s", sString);

		break;
		case( HISTORY_TALKED_TO_MINER ):
			//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_TALKED_TO_MINER ], pTownNames[ pHistory->ubSecondCode ] );
			sgp_swprintf(pString, 512,HistoryName[ HISTORY_TALKED_TO_MINER ].sHistory, pTownNames[town] );
		break;
		case( HISTORY_LIBERATED_TOWN ):
			//sgp_swprintf(pString, 512,pHistoryStrings[ HISTORY_LIBERATED_TOWN ], pTownNames[ pHistory->ubSecondCode ] );
			sgp_swprintf(pString, 512,HistoryName[ HISTORY_LIBERATED_TOWN ].sHistory, pTownNames[town] );
			break;
		case( HISTORY_CHEAT_ENABLED ):
			sgp_swprintf(pString, 512, L"%s",
				HistoryName[HISTORY_CHEAT_ENABLED].sHistory);
			break;
		case HISTORY_TALKED_TO_FATHER_WALKER:
			sgp_swprintf(pString, 512, L"%s",
				HistoryName[HISTORY_TALKED_TO_FATHER_WALKER].sHistory);
			break;
		case HISTORY_MERC_MARRIED_OFF:
			//sgp_swprintf( pString, 512,pHistoryStrings[ HISTORY_MERC_MARRIED_OFF ], gMercProfiles[pHistory->ubSecondCode].zNickname );
			sgp_swprintf( pString, 512,HistoryName[ HISTORY_MERC_MARRIED_OFF ].sHistory, gMercProfiles[profile].zNickname );
			break;
		case HISTORY_MERC_CONTRACT_EXPIRED:
			//sgp_swprintf( pString, 512,pHistoryStrings[ HISTORY_MERC_CONTRACT_EXPIRED ], gMercProfiles[pHistory->ubSecondCode].zName );
			sgp_swprintf( pString, 512,HistoryName[ HISTORY_MERC_CONTRACT_EXPIRED ].sHistory, gMercProfiles[profile].zName );
			break;
		case HISTORY_RPC_JOINED_TEAM:
			//sgp_swprintf( pString, 512,pHistoryStrings[ HISTORY_RPC_JOINED_TEAM ], gMercProfiles[pHistory->ubSecondCode].zName );
			sgp_swprintf( pString, 512,HistoryName[ HISTORY_RPC_JOINED_TEAM ].sHistory, gMercProfiles[profile].zName );
			break;
		case HISTORY_ENRICO_COMPLAINED:
			sgp_swprintf(pString, 512, L"%s",
				HistoryName[HISTORY_ENRICO_COMPLAINED].sHistory);
			break;
		case HISTORY_MINE_RUNNING_OUT:
		case HISTORY_MINE_RAN_OUT:
		case HISTORY_MINE_SHUTDOWN:
		case HISTORY_MINE_REOPENED:
			// all the same format
			//sgp_swprintf(pString, 512,pHistoryStrings[ pHistory->ubCode ], pTownNames[ pHistory->ubSecondCode ] );
			sgp_swprintf(pString, 512,HistoryName[ pHistory->ubCode ].sHistory, pTownNames[town] );
			break;
		case HISTORY_LOST_BOXING:
		case HISTORY_WON_BOXING:
		case HISTORY_DISQUALIFIED_BOXING:
		case HISTORY_NPC_KILLED:
		case HISTORY_MERC_KILLED_CHARACTER:
			//sgp_swprintf( pString, 512,pHistoryStrings[ pHistory->ubCode ], gMercProfiles[pHistory->ubSecondCode].zNickname );
			sgp_swprintf( pString, 512,HistoryName[ pHistory->ubCode ].sHistory, gMercProfiles[profile].zNickname );
			break;

		// ALL SIMPLE HISTORY LOG MSGS, NO PARAMS
		case HISTORY_FOUND_MONEY:
		case HISTORY_ASSASSIN:
		case HISTORY_DISCOVERED_TIXA:
		case HISTORY_DISCOVERED_ORTA:
		case HISTORY_GOT_ROCKET_RIFLES:
		case HISTORY_DEIDRANNA_DEAD_BODIES:
		case HISTORY_BOXING_MATCHES:
		case HISTORY_SOMETHING_IN_MINES:
		case HISTORY_DEVIN:
		case HISTORY_MIKE:
		case HISTORY_TONY:
		case HISTORY_KROTT:
		case HISTORY_KYLE:
		case HISTORY_MADLAB:
		case HISTORY_GABBY:
		case HISTORY_KEITH_OUT_OF_BUSINESS:
		case HISTORY_HOWARD_CYANIDE:
		case HISTORY_KEITH:
		case HISTORY_HOWARD:
		case HISTORY_PERKO:
		case HISTORY_SAM:
		case HISTORY_FRANZ:
		case HISTORY_ARNOLD:
		case HISTORY_FREDO:
		case HISTORY_RICHGUY_BALIME:
		case HISTORY_JAKE:
		case HISTORY_BUM_KEYCARD:
		case HISTORY_WALTER:
		case HISTORY_DAVE:
		case HISTORY_PABLO:
		case HISTORY_KINGPIN_MONEY:
		//VARIOUS BATTLE CONDITIONS
		case HISTORY_LOSTTOWNSECTOR:
		case HISTORY_DEFENDEDTOWNSECTOR:
		case HISTORY_LOSTBATTLE:
		case HISTORY_WONBATTLE:
		case HISTORY_FATALAMBUSH:
		case HISTORY_WIPEDOUTENEMYAMBUSH:
		case HISTORY_UNSUCCESSFULATTACK:
		case HISTORY_SUCCESSFULATTACK:
		case HISTORY_CREATURESATTACKED:
		case HISTORY_KILLEDBYBLOODCATS:
		case HISTORY_SLAUGHTEREDBLOODCATS:
		case HISTORY_GAVE_CARMEN_HEAD:
		case HISTORY_SLAY_MYSTERIOUSLY_LEFT:
		case HISTORY_WALDO:
		case HISTORY_HELICOPTER_REPAIR_STARTED:
		case HISTORY_INTERCEPTED_TRANSPORT_GROUP:
			//sgp_swprintf( pString, 512,pHistoryStrings[ pHistory->ubCode ], pHistory->ubSecondCode );
			sgp_swprintf( pString, 512,HistoryName[ pHistory->ubCode ].sHistory, pHistory->ubSecondCode );
			break;
		default:
			sgp_swprintf( pString, 512,L"missing text, kinda" );
			break;

	}
}


void DrawHistoryLocation( INT16 sSectorX, INT16 sSectorY )
{
	// will draw the location of the history event


	return;
}


void SetHistoryButtonStates( void )
{
	const bool hasPrevious = iCurrentHistoryPage > 1;
	const bool hasNext = iCurrentHistoryPage >= 1 &&
		static_cast<std::size_t>(iCurrentHistoryPage) <
			gHistoryRecordPageCount;
	if (hasPrevious)
	{
		EnableButton(giHistoryButton[PREV_PAGE_BUTTON]);
		EnableButton(giHistoryButton[FIRST_PAGE_BUTTON]);
	}
	else
	{
		DisableButton(giHistoryButton[PREV_PAGE_BUTTON]);
		DisableButton(giHistoryButton[FIRST_PAGE_BUTTON]);
	}
	if (hasNext)
	{
		EnableButton(giHistoryButton[NEXT_PAGE_BUTTON]);
		EnableButton(giHistoryButton[LAST_PAGE_BUTTON]);
	}
	else
	{
		DisableButton(giHistoryButton[NEXT_PAGE_BUTTON]);
		DisableButton(giHistoryButton[LAST_PAGE_BUTTON]);
	}
}


BOOLEAN LoadInHistoryRecords( UINT32 uiPage )
{
	ClearHistoryList();
	if (uiPage == 0 || !FileExists(HISTORY_DATA_FILE)) return FALSE;
	ScopedLaptopFile file(FileOpen(HISTORY_DATA_FILE,
		FILE_OPEN_EXISTING | FILE_ACCESS_READ, FALSE));
	if (!file) return FALSE;
	const std::size_t fileBytes = FileGetSize(file.Get());
	if (!LaptopRecordPageModel::IsWellFormedFile(
		fileBytes, HistoryFileLayout)) return FALSE;
	const std::size_t recordCount = LaptopRecordPageModel::RecordCount(
		fileBytes, HistoryFileLayout);
	const std::size_t pageIndex = static_cast<std::size_t>(uiPage - 1);
	const std::size_t pageCount = LaptopRecordPageModel::PageCount(
		recordCount, NUM_RECORDS_PER_PAGE);
	if (!LaptopRecordPageModel::HasZeroBasedPage(pageIndex, pageCount))
		return FALSE;
	const std::size_t offset = LaptopRecordPageModel::PageByteOffset(
		pageIndex, NUM_RECORDS_PER_PAGE, HistoryFileLayout);
	if (offset == LaptopRecordPageModel::NoOffset ||
		offset > std::numeric_limits<UINT32>::max() ||
		!FileSeek(file.Get(), static_cast<UINT32>(offset),
			FILE_SEEK_FROM_START)) return FALSE;
	const std::size_t recordsOnPage = LaptopRecordPageModel::RecordsOnPage(
		recordCount, pageIndex, NUM_RECORDS_PER_PAGE);
	for (std::size_t index = 0; index < recordsOnPage; ++index)
	{
		HistoryRecordData record;
		if (!ReadHistoryRecordExact(file.Get(), record) ||
			ProcessAndEnterAHistoryRecord(record.code, record.date,
				record.secondCode, record.sectorX, record.sectorY,
				record.sectorZ, record.color) == InvalidHistoryRecordId)
		{
			ClearHistoryList();
			return FALSE;
		}

		#ifdef JA2TESTVERSION
		PerformCheckOnHistoryRecord(3, record.sectorX,
			record.sectorY, record.sectorZ);
		#endif
	}
	pCurrentHistory = pHistoryListHead;
	return pCurrentHistory != NULL;
}


BOOLEAN LoadNextHistoryPage( void )
{
	if (iCurrentHistoryPage < 1 ||
		static_cast<std::size_t>(iCurrentHistoryPage) >=
			gHistoryRecordPageCount) return FALSE;
	const INT32 previousPage = iCurrentHistoryPage;
	const UINT32 nextPage = static_cast<UINT32>(previousPage + 1);
	if (LoadInHistoryRecords(nextPage))
	{
		iCurrentHistoryPage = static_cast<INT32>(nextPage);
		return TRUE;
	}
	LoadInHistoryRecords(static_cast<UINT32>(previousPage));
	return FALSE;
}


BOOLEAN LoadPreviousHistoryPage( void )
{
	if (iCurrentHistoryPage <= 1) return FALSE;
	const INT32 previousPage = iCurrentHistoryPage;
	const UINT32 targetPage = static_cast<UINT32>(previousPage - 1);
	if (LoadInHistoryRecords(targetPage))
	{
		iCurrentHistoryPage = static_cast<INT32>(targetPage);
		return TRUE;
	}
	LoadInHistoryRecords(static_cast<UINT32>(previousPage));
	return FALSE;
}


BOOLEAN AppendHistoryToEndOfFile(const HistoryUnit& historyRecord)
{
	ScopedLaptopFile file(FileOpen(HISTORY_DATA_FILE,
		FILE_ACCESS_WRITE | FILE_OPEN_ALWAYS, FALSE));
	if (!file || !LaptopRecordPageModel::IsWellFormedFile(
		FileGetSize(file.Get()), HistoryFileLayout) ||
		!FileSeek(file.Get(), 0, FILE_SEEK_FROM_END)) return FALSE;

		#ifdef JA2TESTVERSION
		PerformCheckOnHistoryRecord(5, historyRecord.sSectorX,
			historyRecord.sSectorY, historyRecord.bSectorZ);
		#endif
	return WriteHistoryRecordExact(file.Get(), historyRecord);
}

void ResetHistoryFact( UINT8 ubCode, INT16 sSectorX, INT16 sSectorY )
{
	if (!OpenAndReadHistoryFile())
	{
		ClearHistoryList();
		Assert(0);
		return;
	}
	BOOLEAN found = FALSE;
	for (HistoryUnitPtr record = pHistoryListHead;
		record; record = record->Next)
	{
		if (record->ubSecondCode == ubCode &&
			record->ubCode == HISTORY_QUEST_STARTED)
		{
			record->ubColor = 0;
			found = TRUE;
			break;
		}
	}
	if (found)
	{
		if (!OpenAndWriteHistoryFile())
		{
			ClearHistoryList();
			Assert(0);
			return;
		}
	}
	else
	{
		ClearHistoryList();
	}
	SetHistoryFact(HISTORY_QUEST_FINISHED, ubCode,
		GetWorldTotalMin(), sSectorX, sSectorY);
}


UINT32 GetTimeQuestWasStarted( UINT8 ubCode )
{
	const INT32 displayedPage = iCurrentHistoryPage;
	if (!OpenAndReadHistoryFile()) return 0;
	UINT32 startTime = 0;
	for (HistoryUnitPtr record = pHistoryListHead;
		record; record = record->Next)
	{
		if (record->ubSecondCode == ubCode &&
			record->ubCode == HISTORY_QUEST_STARTED)
		{
			startTime = record->uiDate;
			break;
		}
	}
	ClearHistoryList();
	if (fInHistoryMode && gHistoryRecordPageCount > 0)
	{
		iCurrentHistoryPage = static_cast<INT32>(
			LaptopRecordPageModel::NormalizeOneBasedPage(
				static_cast<std::size_t>(displayedPage < 1 ? 1 : displayedPage),
				gHistoryRecordPageCount));
		LoadInHistoryRecords(static_cast<UINT32>(iCurrentHistoryPage));
	}
	return startTime;
}

void GetQuestStartedString( UINT8 ubQuestValue, CHAR16 *sQuestString )
{
	const ContentPolicy::QuestTextRecord record =
		CurrentQuestTextRecord(ubQuestValue, false);
	LoadEncryptedDataFromFile(record.path, sQuestString,
		160 * record.recordIndex, 160);
}


void GetQuestEndedString( UINT8 ubQuestValue, CHAR16 *sQuestString )
{
	const ContentPolicy::QuestTextRecord record =
		CurrentQuestTextRecord(ubQuestValue, true);
	LoadEncryptedDataFromFile(record.path, sQuestString,
		160 * record.recordIndex, 160);
}


#ifdef JA2TESTVERSION
void PerformCheckOnHistoryRecord( UINT32 uiErrorCode, INT16 sSectorX, INT16 sSectorY, INT8 bSectorZ )
{
	CHAR	zString[512];

	if( sSectorX > 16 || sSectorY > 16 || bSectorZ > 3 || sSectorX < -1 || sSectorY < -1 || bSectorZ < 0 )
	{
		sprintf( zString, "History page is pooched, please remember what you were just doing, send your latest save to dave, and tell him this number, Error #%d.", uiErrorCode );
		AssertMsg( 0, zString );
	}
}
#endif

