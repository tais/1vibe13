	#include "laptop.h"
	#include "finances.h"
	#include "LaptopPageResourceOwner.h"
	#include "LaptopRecordFile.h"
	#include "LaptopRecordPageModel.h"
	#include "Game Clock.h"
	#include "Utilities.h"
	#include "DEBUG.H"
	#include "WordWrap.h"
	#include "Encrypted File.h"
	#include "Cursors.h"
	#include "Soldier Profile.h"
	#include "Soldier Profile Constants.h"
	#include "Text.h"
	#include "Strategic Mines.h"
	#include "LaptopSave.h"
	#include "Campaign Types.h"
	#include "strategicmap.h"
	// HEADROCK HAM 3.6: Added facilities for calculating hourly expenses
	#include "Facilities.h"
	// HEADROCK HAM 3.6: Militia upkeep
	#include "Town Militia.h"
	#include "CampaignStats.h"		// added by Flugente
	#include "DynamicDialogue.h"	// added by Flugente
#include "GameSettings.h"

#include <string>
#include <vector>


// the global defines

// graphical positions
#define TOP_X									LAPTOP_SCREEN_UL_X
#define TOP_Y LAPTOP_SCREEN_UL_Y
#define BLOCK_HEIGHT 10
#define TOP_DIVLINE_Y							iScreenHeightOffset + 102
#define DIVLINE_X								iScreenWidthOffset + 130
#define MID_DIVLINE_Y							iScreenHeightOffset + 205
#define BOT_DIVLINE_Y							iScreenHeightOffset + 180
#define MID_DIVLINE_Y2							iScreenHeightOffset + 263 + 20
#define BOT_DIVLINE_Y2 MID_DIVLINE_Y2 + MID_DIVLINE_Y - BOT_DIVLINE_Y
#define TITLE_X									iScreenWidthOffset + 140
#define TITLE_Y									iScreenHeightOffset + 33
#define TEXT_X									iScreenWidthOffset + 140
#define PAGE_SIZE 17

// yesterdyas/todays income and balance text positions
#define YESTERDAYS_INCOME						iScreenHeightOffset + 114
#define YESTERDAYS_OTHER						iScreenHeightOffset + 138
#define YESTERDAYS_DEBITS						iScreenHeightOffset + 162
#define YESTERDAYS_BALANCE						iScreenHeightOffset + 188
#define TODAYS_INCOME							iScreenHeightOffset + 215
#define TODAYS_OTHER							iScreenHeightOffset + 239
#define TODAYS_DEBITS							iScreenHeightOffset + 263
#define TODAYS_CURRENT_BALANCE					iScreenHeightOffset + 263 + 28
#define TODAYS_CURRENT_FORCAST_INCOME			iScreenHeightOffset + 330
#define TODAYS_CURRENT_FORCAST_BALANCE			iScreenHeightOffset + 354
//#define SUMMARY_NUMBERS_X
#define FINANCE_HEADER_FONT FONT14ARIAL
#define FINANCE_TEXT_FONT FONT12ARIAL
#define NUM_RECORDS_PER_PAGE PAGE_SIZE

// records text positions
#define RECORD_CREDIT_WIDTH 106-47
#define RECORD_DEBIT_WIDTH RECORD_CREDIT_WIDTH
#define RECORD_DATE_X TOP_X+10
#define RECORD_TRANSACTION_X RECORD_DATE_X+RECORD_DATE_WIDTH
#define RECORD_TRANSACTION_WIDTH 500-280
#define RECORD_DEBIT_X RECORD_TRANSACTION_X+RECORD_TRANSACTION_WIDTH
#define RECORD_CREDIT_X RECORD_DEBIT_X+RECORD_DEBIT_WIDTH
#define RECORD_Y								iScreenHeightOffset + 107-10
#define RECORD_DATE_WIDTH 47
#define RECORD_BALANCE_X RECORD_DATE_X+385
#define RECORD_BALANCE_WIDTH 479-385
#define RECORD_HEADER_Y							iScreenHeightOffset + 90


#define PAGE_NUMBER_X							TOP_X+297
#define PAGE_NUMBER_Y				TOP_Y+33


// BUTTON defines
enum{
	PREV_PAGE_BUTTON=0,
	NEXT_PAGE_BUTTON,
	FIRST_PAGE_BUTTON,
	LAST_PAGE_BUTTON,
};


// button positions

#define	FIRST_PAGE_X		iScreenWidthOffset + 505
#define NEXT_BTN_X			iScreenWidthOffset + 553//577
#define PREV_BTN_X			iScreenWidthOffset + 529//553
#define	LAST_PAGE_X			iScreenWidthOffset + 577
#define BTN_Y				iScreenHeightOffset + 53



// sizeof one record
#define RECORD_SIZE ( sizeof( UINT32 ) + sizeof( INT32 ) + sizeof( INT32 ) + sizeof( UINT8 ) + sizeof( UINT8 ) )

namespace
{
constexpr LaptopRecordPageModel::FileLayout FinanceFileLayout{
	sizeof(INT32), RECORD_SIZE};
constexpr UINT32 InvalidFinanceRecordId = UINT32_MAX;

struct FinanceRecordData
{
	UINT8 code = 0;
	UINT8 secondCode = 0;
	UINT32 date = 0;
	INT32 amount = 0;
	INT32 balanceToDate = 0;
};

bool ReadFinanceRecordExact(HWFILE file, FinanceRecordData& record)
{
	return ReadLaptopFileExact(file, &record.code, sizeof(record.code)) &&
		ReadLaptopFileExact(file, &record.secondCode,
			sizeof(record.secondCode)) &&
		ReadLaptopFileExact(file, &record.date, sizeof(record.date)) &&
		ReadLaptopFileExact(file, &record.amount, sizeof(record.amount)) &&
		ReadLaptopFileExact(file, &record.balanceToDate,
			sizeof(record.balanceToDate));
}

bool LoadFinanceLedger(std::vector<FinanceRecordData>& records)
{
	records.clear();
	if (!FileExists(FINANCES_DATA_FILE)) return true;
	ScopedLaptopFile file(FileOpen(FINANCES_DATA_FILE,
		FILE_OPEN_EXISTING | FILE_ACCESS_READ, FALSE));
	if (!file) return false;
	const std::size_t fileBytes = FileGetSize(file.Get());
	if (!LaptopRecordPageModel::IsWellFormedFile(
		fileBytes, FinanceFileLayout)) return false;
	INT32 ignoredBalance = 0;
	if (!ReadLaptopFileExact(file.Get(), &ignoredBalance,
		sizeof(ignoredBalance))) return false;
	const std::size_t count = LaptopRecordPageModel::RecordCount(
		fileBytes, FinanceFileLayout);
	records.resize(count);
	for (FinanceRecordData& record : records)
	{
		if (!ReadFinanceRecordExact(file.Get(), record))
		{
			records.clear();
			return false;
		}
	}
	return true;
}

std::size_t FinanceRecordCountOnDisk()
{
	if (!FileExists(FINANCES_DATA_FILE)) return 0;
	ScopedLaptopFile file(FileOpen(FINANCES_DATA_FILE,
		FILE_OPEN_EXISTING | FILE_ACCESS_READ, FALSE));
	if (!file) return 0;
	return LaptopRecordPageModel::RecordCount(
		FileGetSize(file.Get()), FinanceFileLayout);
}

std::wstring FormatFinanceMagnitude(INT32 amount)
{
	return L"$" + std::to_wstring(LaptopRecordPageModel::Magnitude(amount));
}
}




// the financial record list
FinanceUnitPtr pFinanceListHead=NULL;

// current players balance
//INT32 iCurrentBalance=0;

// current page displayed
INT32 iCurrentPage=0;

// current financial record (the one at the top of the current page)
FinanceUnitPtr pCurrentFinance=NULL;

// video object id's
UINT32 guiTITLE;
UINT32 guiGREYFRAME;
UINT32 guiTOP;
UINT32 guiMIDDLE;
UINT32 guiBOTTOM;
UINT32 guiLINE;
UINT32 guiLONGLINE;
UINT32 guiLISTCOLUMNS;

// are in the financial system right now?
BOOLEAN fInFinancialMode=FALSE;
extern BOOLEAN fMapScreenBottomDirty;


// the last page loaded
UINT32 guiLastPageLoaded = 0;

// number of persisted transaction pages (the summary is page zero)
std::size_t gFinanceRecordPageCount = 0;

// finance screen buttons
INT32 giFinanceButton[4];
INT32 giFinanceButtonImage[4];
LaptopPageResourceOwner gFinancePageResources;

// internal functions
UINT32 ProcessAndEnterAFinacialRecord( UINT8 ubCode, UINT32 uiDate, INT32 iAmount, UINT8 ubSecondCode, INT32 iBalanceToDate);
void RenderBackGround( void );
BOOLEAN LoadFinances(LaptopPageResourceOwner& owner);
void DrawSummary( void );
void DrawSummaryLines( void );
void DrawFinanceTitleText( void );
void InvalidateLapTopScreen( void );
void DrawSummaryText( void );
INT32 GetCurrentBalance( void );
void ClearFinanceList( void );
void DrawAPageOfRecords( void );
void DrawRecordsBackGround( void );
void DrawRecordsText( void );
void DrawRecordsColumnHeadersText( void );
void BtnFinanceDisplayNextPageCallBack(GUI_BUTTON *btn,INT32 reason);
void BtnFinanceFirstLastPageCallBack(GUI_BUTTON *btn,INT32 reason);
void BtnFinanceDisplayPrevPageCallBack(GUI_BUTTON *btn,INT32 reason);
BOOLEAN CreateFinanceButtons(LaptopPageResourceOwner& owner);
void ProcessTransactionString(CHAR16 *pString, FinanceUnitPtr pFinance);
void DisplayFinancePageNumberAndDateRange( void );
BOOLEAN GetBalanceFromDisk( void );
BOOLEAN PersistFinanceTransaction(
	const FinanceUnit& financeRecord, INT32 balance);
void SetLastPageInRecords( void );
BOOLEAN LoadInRecords( UINT32 uiPage );
BOOLEAN LoadPreviousPage( void );
BOOLEAN LoadNextPage( void );

INT32 GetPreviousDaysIncome( void );
INT32 GetPreviousDaysBalance( void );

void SetFinanceButtonStates( void );
INT32 GetTodaysBalance( void );
INT32 GetTodaysDebits( void );
INT32 GetYesterdaysOtherDeposits( void );
INT32 GetTodaysOtherDeposits( void );
INT32 GetYesterdaysDebits( void );


UINT32 AddTransactionToPlayersBook (UINT8 ubCode, UINT8 ubSecondCode, UINT32 uiDate, INT32 iAmount)
{
	// adds transaction to player's book(Financial List), returns unique id number of it
	// outside of the financial system(the code in this .c file), this is the only function you'll ever need

	if (!GetBalanceFromDisk() ||
		!LaptopRecordPageModel::CanApplyBalanceChange(
			LaptopSaveInfo.iCurrentBalance, iAmount))
	{
		Assert(0);
		return InvalidFinanceRecordId;
	}
	const INT32 newBalance = static_cast<INT32>(
		static_cast<INT64>(LaptopSaveInfo.iCurrentBalance) + iAmount);

	ClearFinanceList();
	const UINT32 uiId = ProcessAndEnterAFinacialRecord(
		ubCode, uiDate, iAmount, ubSecondCode, newBalance);
	if (uiId == InvalidFinanceRecordId || !pFinanceListHead ||
		!PersistFinanceTransaction(*pFinanceListHead, newBalance))
	{
		ClearFinanceList();
		Assert(0);
		return InvalidFinanceRecordId;
	}

	// Publish campaign state only after the complete ledger record is durable.
	LaptopSaveInfo.iCurrentBalance = newBalance;
	if (gGameExternalOptions.fDynamicOpinions)
		HandleDynamicOpinionOnContractExtension(ubCode, ubSecondCode);
	if (iAmount < 0 && ubSecondCode < NUM_PROFILES &&
		(ubCode == HIRED_MERC || ubCode == IMP_PROFILE ||
		ubCode == PAYMENT_TO_NPC ||
		ubCode == EXTENDED_CONTRACT_BY_1_DAY ||
		ubCode == EXTENDED_CONTRACT_BY_1_WEEK ||
		ubCode == EXTENDED_CONTRACT_BY_2_WEEKS))
	{
		const UINT64 updatedCost =
			static_cast<UINT64>(gMercProfiles[ubSecondCode].uiTotalCostToDate) +
			static_cast<UINT64>(-static_cast<INT64>(iAmount));
		gMercProfiles[ubSecondCode].uiTotalCostToDate = static_cast<UINT32>(
			updatedCost > UINT32_MAX ? UINT32_MAX : updatedCost);
	}

	// set number of pages
	SetLastPageInRecords( );

	if( !fInFinancialMode )
	{
		ClearFinanceList( );
	}
	else
	{
		iCurrentPage = static_cast<INT32>(
			LaptopRecordPageModel::NormalizeZeroBasedPage(
				static_cast<std::size_t>(iCurrentPage < 0 ? 0 : iCurrentPage),
				gFinanceRecordPageCount + 1));
		ClearFinanceList();
		if (iCurrentPage > 0 &&
			!LoadInRecords(static_cast<UINT32>(iCurrentPage)))
		{
			iCurrentPage = 0;
		}
		SetFinanceButtonStates( );

		// force update
		fPausedReDrawScreenFlag = TRUE;
	}

	// Flugente: campaign stats
	if ( ubCode == ANONYMOUS_DEPOSIT )
		gCampaignStats.AddMoneyEarned(CAMPAIGN_MONEY_START, iAmount );
	else if ( ubCode == DEPOSIT_FROM_GOLD_MINE || ubCode == DEPOSIT_FROM_SILVER_MINE )
		gCampaignStats.AddMoneyEarned(CAMPAIGN_MONEY_MINES, iAmount );
	else if ( ubCode == SOLD_ITEMS )
		gCampaignStats.AddMoneyEarned(CAMPAIGN_MONEY_TRADE, iAmount );
	else
		gCampaignStats.AddMoneyEarned(CAMPAIGN_MONEY_ETC, iAmount );

	fMapScreenBottomDirty = TRUE;

	// return unique id of this transaction
	return uiId;
}

FinanceUnitPtr GetFinance(UINT32 uiId)
{
 FinanceUnitPtr pFinance=pFinanceListHead;

 // get a finance object and return a pointer to it, the obtaining of the
 // finance object is via a unique ID the programmer must store
 // , it is returned on addition of a financial transaction

 // error check
 if(!pFinance)
	return ( NULL );

 // look for finance object with Id
 while(pFinance)
 {
	if(pFinance->uiIdNumber == uiId)
		break;

	// next finance record
	pFinance = pFinance->Next;
 }

 return (pFinance);
}

UINT32 GetTotalDebits()
{
	// returns the total of the debits
	UINT32 uiDebits=0;
	FinanceUnitPtr pFinance=pFinanceListHead;

	// run to end of list
	while(pFinance)
	{
		// if a debit, add to debit total
		if(pFinance->iAmount > 0)
			uiDebits+=( (UINT32) (pFinance->iAmount));

		// next finance record
		pFinance=pFinance->Next;
	}

	return uiDebits;
}

UINT32 GetTotalCredits()
{
 	// returns the total of the credits
	UINT32 uiCredits = 0;
	FinanceUnitPtr pFinance=pFinanceListHead;

	// run to end of list
	while( pFinance )
	{
		// if a credit, add to credit total
		if( pFinance->iAmount < 0 )
			uiCredits += ( (UINT32) ( pFinance->iAmount ));

		// next finance record
		pFinance = pFinance->Next;
	}

	return uiCredits;
}

UINT32 GetDayCredits(UINT32 usDayNumber)
{
	// returns the total of the credits for day( note resolution of usDayNumber is days)
	UINT32 uiCredits = 0;
	FinanceUnitPtr pFinance = pFinanceListHead;

	while( pFinance )
	{
		// if a credit and it occurs on day passed
		if(( pFinance->iAmount < 0)&&( (pFinance->uiDate / (60*24)) ==usDayNumber ))
			uiCredits+=((UINT32)(pFinance->iAmount));

		// next finance record
		pFinance=pFinance->Next;
	}

	return uiCredits;
}

UINT32 GetDayDebits(UINT32 usDayNumber)
{
	// returns the total of the debits
	UINT32 uiDebits=0;
	FinanceUnitPtr pFinance=pFinanceListHead;

	while(pFinance)
	{
		if(( pFinance->iAmount > 0 )&&( (pFinance->uiDate / (60*24) ) ==usDayNumber ) )
			uiDebits += ( (UINT32) (pFinance->iAmount));

	// next finance record
		pFinance=pFinance->Next;
	}

	return uiDebits;
}

INT32 GetTotalToDay( INT32 sTimeInMins )
{
	// gets the total amount to this day
	UINT32 uiTotal = 0;
	FinanceUnitPtr pFinance = pFinanceListHead;

	while(pFinance)
	{
		if(((INT32)( pFinance->uiDate / (60*24)) <= sTimeInMins/(24*60) ))
			uiTotal += ((UINT32)(pFinance->iAmount));

	// next finance record
		pFinance=pFinance->Next;
	}

	return uiTotal;
}
INT32 GetYesterdaysIncome( void )
{
	// get income for yesterday
	return ( GetDayDebits(( ( GetWorldTotalMin() - (24*60) ) / (24*60) )) + GetDayCredits(( (UINT32) ( GetWorldTotalMin() -(24*60) )/ (24*60) )));
}

INT32 GetCurrentBalance( void )
{
	// get balance to this minute
	return ( LaptopSaveInfo.iCurrentBalance );

	// return(GetTotalDebits((GetWorldTotalMin()))+GetTotalCredits((GetWorldTotalMin())));
}

INT32 GetTodaysIncome( void )
{
 // get income
 return ( GetCurrentBalance() - GetTotalToDay( GetWorldTotalMin() - ( 24*60 ) ));
}


INT32 GetProjectedTotalDailyIncome( void )
{
	// return total	projected income, including what is earned today already

	// CJC: I DON'T THINK SO!
	// The point is:	PredictIncomeFromPlayerMines isn't dependant on the time of day
	// (anymore) and this would report income of 0 at midnight!
	/*
	if (GetWorldMinutesInDay() <= 0)
	{
		return ( 0 );
	}
	*/
	// look at we earned today

	// then there is how many deposits have been made, now look at how many mines we have, thier rate, amount of ore left and predict if we still
	// had these mines how much more would we get?

	// HEADROCK HAM 3.6: Facilities can make you money, and this is figured into your daily income.
	
	return LaptopRecordPageModel::SaturatingAdd(
		PredictIncomeFromPlayerMines(TRUE),
		LaptopRecordPageModel::SaturatingMultiply(
			15, GetTotalFacilityHourlyCosts(TRUE)));
}

// HEADROCK HAM 3.6: Predict expenses today. Takes into account facilities, and in the future merc contracts.
INT32 GetProjectedExpenses( void )
{
	INT32 total = LaptopRecordPageModel::SaturatingMultiply(
		GetTotalFacilityHourlyCosts(FALSE), 15);
	total = LaptopRecordPageModel::SaturatingAdd(
		total, GetTotalContractExpenses());
	return LaptopRecordPageModel::SaturatingAddUnsigned(
		total, guiTotalUpkeepForMilitia);
}

INT32 GetProjectedBalance( void )
{
	// return the projected balance for tommorow - total for today plus the total income, projected.
	return LaptopRecordPageModel::SaturatingAdd(
		GetProjectedTotalDailyIncome(), GetCurrentBalance());
}

INT32 GetConfidenceValue()
{
	// return confidence that the projected income is infact correct
	return(( ( GetWorldMinutesInDay()*100 ) / (60*24) ));
}

void GameInitFinances()
{
	// initialize finances on game start up
	// unlink Finances data file
	if( (FileExists( FINANCES_DATA_FILE ) ) )
	{
		FileDelete( FINANCES_DATA_FILE );
	}
	GetBalanceFromDisk( );
}

void EnterFinances()
{
 //entry into finanacial system, load graphics, set variables..draw screen once
 // set the fact we are in the financial display system
	LaptopPageResourceOwner staged;
	gFinancePageResources.clear();
	fInFinancialMode=FALSE;

	// build finances list
	//OpenAndReadFinancesFile( );

	// get the balance
	GetBalanceFromDisk( );

	// clear the list
	ClearFinanceList( );

	// force redraw of the entire screen
	fReDrawScreenFlag=TRUE;

	// set number of pages
	SetLastPageInRecords( );
	iCurrentPage = static_cast<INT32>(
		LaptopRecordPageModel::NormalizeZeroBasedPage(
			static_cast<std::size_t>(
				LaptopSaveInfo.iCurrentFinancesPage < 0
					? 0 : LaptopSaveInfo.iCurrentFinancesPage),
			gFinanceRecordPageCount + 1));
	if (iCurrentPage > 0 &&
		!LoadInRecords(static_cast<UINT32>(iCurrentPage)))
	{
		iCurrentPage = 0;
		ClearFinanceList();
	}

	// load graphics into memory
	if (!LoadFinances(staged))
	{
		ClearFinanceList();
		return;
	}

	// create buttons
	if (!CreateFinanceButtons(staged))
	{
		ClearFinanceList();
		return;
	}
	gFinancePageResources = std::move(staged);
	fInFinancialMode=TRUE;

	// set button state
	SetFinanceButtonStates( );

	// draw finance
	RenderFinances( );

//	DrawSummary( );

	// draw page number
	DisplayFinancePageNumberAndDateRange( );



	//InvalidateRegion(0,0,640,480);
	return;
}

void ExitFinances( void )
{
	LaptopSaveInfo.iCurrentFinancesPage = iCurrentPage;


	// not in finance system anymore
	fInFinancialMode=FALSE;

	// clear out list
	ClearFinanceList( );

	gFinancePageResources.clear();
	return;

}

void HandleFinances( void )
{

}

void RenderFinances( void )
{
	HVOBJECT hHandle;

	// draw background
	RenderBackGround();

	// if we are on the first page, draw the summary
	if(iCurrentPage==0)
	DrawSummary( );
	else
	DrawAPageOfRecords( );



	//title
	DrawFinanceTitleText( );

	// draw pages and dates
	DisplayFinancePageNumberAndDateRange( );


		// display border
	GetVideoObject(&hHandle, guiLaptopBACKGROUND);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,iScreenWidthOffset + 108, iScreenHeightOffset + 23, VO_BLT_SRCTRANSPARENCY,NULL);


	// title bar icon
	BlitTitleBarIcons(	);



	return;
}

BOOLEAN LoadFinances(LaptopPageResourceOwner& owner)
{
	VOBJECT_DESC	VObjectDesc;
	// load Finance video objects into memory

	// title bar
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\programtitlebar.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiTITLE)) return FALSE;

	// top portion of the screen background
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\Financeswindow.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiTOP)) return FALSE;

	// black divider line - long ( 480 length)
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\divisionline480.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiLONGLINE)) return FALSE;

	// the records columns
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\recordcolumns.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiLISTCOLUMNS)) return FALSE;

	// black divider line - long ( 480 length)
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\divisionline.sti", VObjectDesc.ImageFile);
	if (!owner.addVideoObject(&VObjectDesc, guiLINE)) return FALSE;

	return (TRUE);
}

void RenderBackGround( void )
{
	// render generic background for Finance system
	HVOBJECT hHandle;

	// get title bar object
	GetVideoObject(&hHandle, guiTITLE);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,TOP_X, TOP_Y - 2, VO_BLT_SRCTRANSPARENCY,NULL);

	// get and blt the top part of the screen, video object and blt to screen
	GetVideoObject(&hHandle, guiTOP);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,TOP_X, TOP_Y + 22, VO_BLT_SRCTRANSPARENCY,NULL);
	DrawFinanceTitleText( );
	return;
}




void DrawSummary( void )
{
	// draw day's summary to screen
	DrawSummaryLines( );
	DrawSummaryText( );
	DrawFinanceTitleText( );
	return;
}

void DrawSummaryLines( void )
{
	// draw divider lines on screen
	HVOBJECT hHandle;

	// the summary LINE object handle
	GetVideoObject(&hHandle, guiLINE);

	// blit summary LINE object to screen
	BltVideoObject(FRAME_BUFFER, hHandle, 0,DIVLINE_X, TOP_DIVLINE_Y, VO_BLT_SRCTRANSPARENCY,NULL);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,DIVLINE_X, TOP_DIVLINE_Y+2, VO_BLT_SRCTRANSPARENCY,NULL);
	//BltVideoObject(FRAME_BUFFER, hHandle, 0,DIVLINE_X, MID_DIVLINE_Y, VO_BLT_SRCTRANSPARENCY,NULL);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,DIVLINE_X, BOT_DIVLINE_Y, VO_BLT_SRCTRANSPARENCY,NULL);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,DIVLINE_X, MID_DIVLINE_Y2, VO_BLT_SRCTRANSPARENCY,NULL);
	//BltVideoObject(FRAME_BUFFER, hHandle, 0,DIVLINE_X, BOT_DIVLINE_Y2, VO_BLT_SRCTRANSPARENCY,NULL);


	return;
}

void DrawAPageOfRecords( void )
{
	// this procedure will draw a series of financial records to the screen
	pCurrentFinance=pFinanceListHead;

	// (re-)render background
	DrawRecordsBackGround( );

	// error check
	if(iCurrentPage==-1)
		return;


	// current page is found, render	from here
	DrawRecordsText( );
	DisplayFinancePageNumberAndDateRange( );
	return;
}

void DrawRecordsBackGround( void )
{
	// proceudre will draw the background for the list of financial records
	INT32 iCounter=6;
	HVOBJECT hHandle;

	// render the generic background
	RenderBackGround( );


	// now the columns
	for( ; iCounter <35; iCounter++)
	{
		// get and blt middle background to screen
	GetVideoObject(&hHandle, guiLISTCOLUMNS);
	BltVideoObject(FRAME_BUFFER, hHandle, 0, TOP_X + 10, TOP_Y + 18 + ( iCounter * BLOCK_HEIGHT ) + 1, VO_BLT_SRCTRANSPARENCY,NULL);
	}

	// the divisorLines
	GetVideoObject(&hHandle, guiLONGLINE);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,TOP_X + 10, TOP_Y + 17 + ( 6 * ( BLOCK_HEIGHT ) ), VO_BLT_SRCTRANSPARENCY,NULL);
	GetVideoObject(&hHandle, guiLONGLINE);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,TOP_X + 10, TOP_Y + 19 + ( 6 * ( BLOCK_HEIGHT ) ) , VO_BLT_SRCTRANSPARENCY,NULL);
	GetVideoObject(&hHandle, guiLONGLINE);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,TOP_X + 10, TOP_Y + 19 + ( ( iCounter	) * ( BLOCK_HEIGHT ) ) , VO_BLT_SRCTRANSPARENCY,NULL);


	// the header text
	DrawRecordsColumnHeadersText( );

	return;

}

void DrawRecordsColumnHeadersText( void )
{
	// write the headers text for each column
	INT16 usX, usY;

	// font stuff
	SetFont(FINANCE_TEXT_FONT);
	SetFontForeground(FONT_BLACK);
	SetFontBackground(FONT_BLACK);
	SetFontShadow(NO_SHADOW);

	// the date header
	FindFontCenterCoordinates(RECORD_DATE_X,0,RECORD_DATE_WIDTH,0, pFinanceHeaders[0], FINANCE_TEXT_FONT,&usX, &usY);
	mprintf(usX, RECORD_HEADER_Y, L"%s", pFinanceHeaders[0]);

	// debit header
	FindFontCenterCoordinates(RECORD_DEBIT_X,0,RECORD_DEBIT_WIDTH,0, pFinanceHeaders[1], FINANCE_TEXT_FONT,&usX, &usY);
	mprintf(usX, RECORD_HEADER_Y, L"%s", pFinanceHeaders[1]);

	// credit header
	FindFontCenterCoordinates(RECORD_CREDIT_X,0,RECORD_CREDIT_WIDTH,0, pFinanceHeaders[2], FINANCE_TEXT_FONT,&usX, &usY);
	mprintf(usX, RECORD_HEADER_Y, L"%s", pFinanceHeaders[2]);

	// balance header
	FindFontCenterCoordinates(RECORD_BALANCE_X,0,RECORD_BALANCE_WIDTH,0, pFinanceHeaders[4], FINANCE_TEXT_FONT,&usX, &usY);
	mprintf(usX, RECORD_HEADER_Y, L"%s", pFinanceHeaders[4]);

	// transaction header
	FindFontCenterCoordinates(RECORD_TRANSACTION_X,0,RECORD_TRANSACTION_WIDTH,0, pFinanceHeaders[3], FINANCE_TEXT_FONT,&usX, &usY);
	mprintf(usX, RECORD_HEADER_Y, L"%s", pFinanceHeaders[3]);

	SetFontShadow(DEFAULT_SHADOW);
	return;
}

void DrawRecordsText( void )
{
    Assert(pFinanceListHead);

	// draws the text of the records
	FinanceUnitPtr pCurFinance=pCurrentFinance;
	FinanceUnitPtr pTempFinance=pFinanceListHead;
	CHAR16 sString[512];
	INT32 iCounter=0;
	INT16 usX, usY;
	INT32 iBalance=0;

	// setup the font stuff
	SetFont(FINANCE_TEXT_FONT);
	SetFontForeground(FONT_BLACK);
	SetFontBackground(FONT_BLACK);
	SetFontShadow(NO_SHADOW);


	// anything to print
	if( pCurrentFinance == NULL )
	{
		// nothing to print
		return;
	}

	// get balance to this point
	while( pTempFinance !=pCurFinance)
	{
		// increment balance by amount of transaction
	iBalance += pTempFinance->iAmount;

		// next element
		pTempFinance = pTempFinance->Next;
	}

	// loop through record list
	for( ; iCounter <NUM_RECORDS_PER_PAGE; iCounter++)
	{
		// get and write the date
		swprintf(sString, L"%d", pCurFinance->uiDate / ( 24*60 ) );



		FindFontCenterCoordinates(RECORD_DATE_X,0,RECORD_DATE_WIDTH,0, sString, FINANCE_TEXT_FONT,&usX, &usY);
		mprintf(usX, 12+RECORD_Y +
			(iCounter * (GetFontHeight(FINANCE_TEXT_FONT) + 6)), L"%s", sString);

		// get and write debit/ credit
		if(pCurFinance->iAmount >=0)
		{
			// increase in asset - debit
	 swprintf(sString, L"%s", FormatMoney(pCurFinance->iAmount).data());
		FindFontCenterCoordinates(RECORD_DEBIT_X,0,RECORD_DEBIT_WIDTH,0, sString, FINANCE_TEXT_FONT,&usX, &usY);
		mprintf(usX, 12+RECORD_Y +
			(iCounter * (GetFontHeight(FINANCE_TEXT_FONT) + 6)), L"%s", sString);
		}
		else
		{
			// decrease in asset - credit
	 swprintf(sString, L"%s", FormatFinanceMagnitude(pCurFinance->iAmount).data());
		SetFontForeground(FONT_RED);

		FindFontCenterCoordinates(RECORD_CREDIT_X ,0 , RECORD_CREDIT_WIDTH,0, sString, FINANCE_TEXT_FONT,&usX, &usY);
		mprintf(usX, 12+RECORD_Y +
			(iCounter * (GetFontHeight(FINANCE_TEXT_FONT) + 6)), L"%s", sString);
		SetFontForeground(FONT_BLACK);
		}

		// the balance to this point
	iBalance = pCurFinance->iBalanceToDate;

		// set font based on balance
		if(iBalance >=0)
		{
		SetFontForeground(FONT_BLACK);
		}
		else
		{
		SetFontForeground(FONT_RED);
			}

		// transaction string
		ProcessTransactionString(sString, pCurFinance);
	FindFontCenterCoordinates(RECORD_TRANSACTION_X,0,RECORD_TRANSACTION_WIDTH,0, sString, FINANCE_TEXT_FONT,&usX, &usY);
		mprintf(usX, 12+RECORD_Y +
			(iCounter * (GetFontHeight(FINANCE_TEXT_FONT) + 6)), L"%s", sString);


		// print the balance string
	swprintf(sString, L"%s", FormatFinanceMagnitude(iBalance).data());
		FindFontCenterCoordinates(RECORD_BALANCE_X,0,RECORD_BALANCE_WIDTH,0, sString, FINANCE_TEXT_FONT,&usX, &usY);
		mprintf(usX, 12+RECORD_Y +
			(iCounter * (GetFontHeight(FINANCE_TEXT_FONT) + 6)), L"%s", sString);

		// restore font color
		SetFontForeground(FONT_BLACK);

		// next finance
		pCurFinance = pCurFinance->Next;

		// last page, no finances left, return
		if( ! pCurFinance )
		{

			// restore shadow
		SetFontShadow(DEFAULT_SHADOW);
			return;
		}

	}

	// restore shadow
	SetFontShadow(DEFAULT_SHADOW);
	return;
}
void DrawFinanceTitleText( void )
{
	// setup the font stuff
	SetFont(FINANCE_HEADER_FONT);
	SetFontForeground(FONT_WHITE);
	SetFontBackground(FONT_BLACK);
	// reset shadow
	SetFontShadow(DEFAULT_SHADOW);

	// draw the pages title
	mprintf(TITLE_X, TITLE_Y, L"%s", pFinanceTitle[0]);


	return;
}

void InvalidateLapTopScreen( void )
{
	// invalidates blit region to force refresh of screen

	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_LR_Y);

	return;
}

void DrawSummaryText( void )
{
	INT16 usX, usY;
	std::wstring tmp{};
	INT32 iBalance = 0;


	// setup the font stuff
	SetFont(FINANCE_TEXT_FONT);
	SetFontForeground(FONT_BLACK);
	SetFontBackground(FONT_BLACK);
	SetFontShadow(NO_SHADOW);

	// draw summary text to the screen
	mprintf(TEXT_X, YESTERDAYS_INCOME, L"%s", pFinanceSummary[2]);
	mprintf(TEXT_X, YESTERDAYS_OTHER, L"%s", pFinanceSummary[3]);
	mprintf(TEXT_X, YESTERDAYS_DEBITS, L"%s", pFinanceSummary[4]);
	mprintf(TEXT_X, YESTERDAYS_BALANCE, L"%s", pFinanceSummary[5]);
	mprintf(TEXT_X, TODAYS_INCOME, L"%s", pFinanceSummary[6]);
	mprintf(TEXT_X, TODAYS_OTHER, L"%s", pFinanceSummary[7]);
	mprintf(TEXT_X, TODAYS_DEBITS, L"%s", pFinanceSummary[8]);
	mprintf(TEXT_X, TODAYS_CURRENT_BALANCE, L"%s", pFinanceSummary[9]);
	mprintf(TEXT_X, TODAYS_CURRENT_FORCAST_INCOME, L"%s", pFinanceSummary[10]);
	mprintf(TEXT_X, TODAYS_CURRENT_FORCAST_BALANCE, L"%s", pFinanceSummary[11]);

	// draw the actual numbers



	// yesterdays income
	iBalance =	GetPreviousDaysIncome( );
	tmp = FormatMoney(iBalance);
	FindFontRightCoordinates(0,0,iScreenWidthOffset + 580,0, tmp.data(), FINANCE_TEXT_FONT, &usX, &usY);

	mprintf(usX, YESTERDAYS_INCOME, L"%s", tmp.data());

	SetFontForeground( FONT_BLACK );

	// yesterdays other
	iBalance =	GetYesterdaysOtherDeposits( );
	tmp = FormatMoney(iBalance);
	FindFontRightCoordinates(0,0,iScreenWidthOffset + 580,0, tmp.data(),FINANCE_TEXT_FONT, &usX, &usY);

	mprintf(usX, YESTERDAYS_OTHER, L"%s", tmp.data());

	SetFontForeground( FONT_RED );

	// yesterdays debits
	if( iBalance < 0 )
	{
		SetFontForeground( FONT_RED );
		iBalance *= -1;
	}
	tmp = FormatMoney(iBalance);

	FindFontRightCoordinates(0,0,iScreenWidthOffset + 580,0, tmp.data(),FINANCE_TEXT_FONT, &usX, &usY);

	mprintf(usX, YESTERDAYS_DEBITS, L"%s", tmp.data());

	SetFontForeground( FONT_BLACK );

	// yesterdays balance..ending balance..so todays balance then
	iBalance =	GetTodaysBalance( );
	if( iBalance < 0 )
	{
		SetFontForeground( FONT_RED );
		iBalance *= -1;
	}
	tmp = FormatMoney(iBalance);

	FindFontRightCoordinates(0,0,iScreenWidthOffset + 580,0, tmp.data(),FINANCE_TEXT_FONT, &usX, &usY);

	mprintf(usX, YESTERDAYS_BALANCE, L"%s", tmp.data());

	SetFontForeground( FONT_BLACK );

	// todays income
	iBalance =	GetTodaysDaysIncome( );
	tmp = FormatMoney(iBalance);

	FindFontRightCoordinates(0,0,iScreenWidthOffset + 580,0,tmp.data(),FINANCE_TEXT_FONT, &usX, &usY);

	mprintf(usX, TODAYS_INCOME, L"%s", tmp.data());

	SetFontForeground( FONT_BLACK );

	// todays other
	iBalance =	GetTodaysOtherDeposits( );
	tmp = FormatMoney(iBalance);

	FindFontRightCoordinates(0,0,iScreenWidthOffset + 580,0,tmp.data(),FINANCE_TEXT_FONT, &usX, &usY);

	mprintf(usX, TODAYS_OTHER, L"%s", tmp.data());

	SetFontForeground( FONT_RED );

	// todays debits
	iBalance =	GetTodaysDebits( );

	// absolute value
	if( iBalance < 0 )
	{
		iBalance *= ( -1 );
	}

	tmp = FormatMoney(iBalance);

	FindFontRightCoordinates(0,0,iScreenWidthOffset + 580,0,tmp.data(),FINANCE_TEXT_FONT, &usX, &usY);

	mprintf(usX, TODAYS_DEBITS, L"%s", tmp.data());

	SetFontForeground( FONT_BLACK );

	// todays current balance
	iBalance = GetCurrentBalance( );
	if( iBalance < 0 )
	{
		SetFontForeground( FONT_RED );
		iBalance *= -1;
	}

	tmp = FormatMoney(iBalance);
	FindFontRightCoordinates(0,0,iScreenWidthOffset + 580,0,tmp.data(),FINANCE_TEXT_FONT, &usX, &usY);
	mprintf(usX, TODAYS_CURRENT_BALANCE, L"%s", tmp.data());
	SetFontForeground( FONT_BLACK );


	// todays forcast income
	iBalance =	GetProjectedTotalDailyIncome( );
	tmp = FormatMoney(iBalance);

	FindFontRightCoordinates(0,0,iScreenWidthOffset + 580,0,tmp.data(),FINANCE_TEXT_FONT, &usX, &usY);

	mprintf(usX, TODAYS_CURRENT_FORCAST_INCOME, L"%s", tmp.data());

	SetFontForeground( FONT_BLACK );


	// todays forcast balance
	iBalance = GetCurrentBalance( ) + GetProjectedTotalDailyIncome( );
	if( iBalance < 0 )
	{
		SetFontForeground( FONT_RED );
		iBalance *= -1;
	}

	tmp = FormatMoney(iBalance);
	FindFontRightCoordinates(0,0,iScreenWidthOffset + 580,0,tmp.data(),FINANCE_TEXT_FONT, &usX, &usY);
	mprintf(usX, TODAYS_CURRENT_FORCAST_BALANCE, L"%s", tmp.data());
	SetFontForeground( FONT_BLACK );



	// reset the shadow
	SetFontShadow(DEFAULT_SHADOW);

	return;
}


void ClearFinanceList( void )
{
	// remove each element from list of transactions
	FinanceUnitPtr pFinanceList=pFinanceListHead;
	FinanceUnitPtr pFinanceNode;

	// while there are elements in the list left, delete them
	while( pFinanceList )
	{
	// set node to list head
		pFinanceNode=pFinanceList;

		// set list head to next node
		pFinanceList=pFinanceList->Next;

		// delete current node
		MemFree(pFinanceNode);
	}
	pCurrentFinance = NULL;
	pFinanceListHead = NULL;
	return;
}


UINT32 ProcessAndEnterAFinacialRecord( UINT8 ubCode, UINT32 uiDate, INT32 iAmount, UINT8 ubSecondCode, INT32 iBalanceToDate )
{
	UINT32 uiId = 0;
	FinanceUnitPtr pFinance=pFinanceListHead;

 	// add to finance list
	if(pFinance)
	{
		// go to end of list
		while(pFinance->Next)
			pFinance=pFinance->Next;

		// alloc space
		pFinance->Next = (finance *) MemAlloc(sizeof(FinanceUnit));
		if (!pFinance->Next) return InvalidFinanceRecordId;

		// increment id number
		uiId = pFinance->uiIdNumber + 1;

		// set up information passed
		pFinance = pFinance->Next;
		pFinance->Next = NULL;
		pFinance->ubCode = ubCode;
	pFinance->ubSecondCode = ubSecondCode;
		pFinance->uiDate = uiDate;
		pFinance->iAmount = iAmount;
	pFinance->uiIdNumber = uiId;
		pFinance->iBalanceToDate = iBalanceToDate;


	}
	else
	{
		// alloc space
		// HEADROCK HAM 3.6: Fix by Warmsteel to prevent repetitive entries on finance list. Next line commented out.
		// uiId = ReadInLastElementOfFinanceListAndReturnIdNumber( );
		pFinance = (FinanceUnitPtr) MemAlloc(sizeof(FinanceUnit));
		if (!pFinance) return InvalidFinanceRecordId;

		// setup info passed
		pFinance->Next = NULL;
		pFinance->ubCode = ubCode;
	pFinance->ubSecondCode = ubSecondCode;
		pFinance->uiDate = uiDate;
		pFinance->iAmount= iAmount;
	pFinance->uiIdNumber = uiId;
		pFinance->iBalanceToDate = iBalanceToDate;
	pFinanceListHead = pFinance;
	}
	pCurrentFinance = pFinanceListHead;

	return uiId;
}

BOOLEAN CreateFinanceButtons(LaptopPageResourceOwner& owner)
{
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\arrows.sti", -1, 0, -1, 1, -1),
		giFinanceButtonImage[PREV_PAGE_BUTTON])) return FALSE;
	if (!owner.addButton(QuickCreateButton( giFinanceButtonImage[PREV_PAGE_BUTTON], PREV_BTN_X, BTN_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)BtnFinanceDisplayPrevPageCallBack),
		giFinanceButton[PREV_PAGE_BUTTON])) return FALSE;


	if (!owner.addButtonImage(UniqueButtonImageHandle(UseLoadedButtonImage(
		giFinanceButtonImage[PREV_PAGE_BUTTON], -1, 6, -1, 7, -1)),
		giFinanceButtonImage[NEXT_PAGE_BUTTON])) return FALSE;
	if (!owner.addButton(QuickCreateButton( giFinanceButtonImage[NEXT_PAGE_BUTTON], NEXT_BTN_X, BTN_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)BtnFinanceDisplayNextPageCallBack),
		giFinanceButton[NEXT_PAGE_BUTTON])) return FALSE;


	//button to go to the first page
	if (!owner.addButtonImage(UniqueButtonImageHandle(UseLoadedButtonImage(
		giFinanceButtonImage[PREV_PAGE_BUTTON], -1, 3, -1, 4, -1)),
		giFinanceButtonImage[FIRST_PAGE_BUTTON])) return FALSE;
	if (!owner.addButton(QuickCreateButton( giFinanceButtonImage[FIRST_PAGE_BUTTON], FIRST_PAGE_X, BTN_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)BtnFinanceFirstLastPageCallBack),
		giFinanceButton[FIRST_PAGE_BUTTON])) return FALSE;

	MSYS_SetBtnUserData( giFinanceButton[FIRST_PAGE_BUTTON], 0, 0 );

	//button to go to the last page
	if (!owner.addButtonImage(UniqueButtonImageHandle(UseLoadedButtonImage(
		giFinanceButtonImage[PREV_PAGE_BUTTON], -1, 9, -1, 10, -1)),
		giFinanceButtonImage[LAST_PAGE_BUTTON])) return FALSE;
	if (!owner.addButton(QuickCreateButton( giFinanceButtonImage[LAST_PAGE_BUTTON], LAST_PAGE_X, BTN_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)BtnFinanceFirstLastPageCallBack),
		giFinanceButton[LAST_PAGE_BUTTON])) return FALSE;
	
	MSYS_SetBtnUserData( giFinanceButton[LAST_PAGE_BUTTON], 0, 1 );

	// set buttons
	SetButtonCursor(giFinanceButton[0], CURSOR_LAPTOP_SCREEN);
	SetButtonCursor(giFinanceButton[1], CURSOR_LAPTOP_SCREEN);
	SetButtonCursor(giFinanceButton[2], CURSOR_LAPTOP_SCREEN);
	SetButtonCursor(giFinanceButton[3], CURSOR_LAPTOP_SCREEN);
	return TRUE;
}
void BtnFinanceDisplayPrevPageCallBack(GUI_BUTTON *btn,INT32 reason)
{

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{

		btn->uiFlags&=~(BUTTON_CLICKED_ON);

		// if greater than page zero, we can move back, decrement iCurrentPage counter
		LoadPreviousPage( );
		pCurrentFinance=pFinanceListHead;

		// set button state
	SetFinanceButtonStates( );
		fReDrawScreenFlag=TRUE;
	}

}

void BtnFinanceDisplayNextPageCallBack(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
	 btn->uiFlags&=~(BUTTON_CLICKED_ON);
		// increment currentPage
	 LoadNextPage( );

		// set button state
	SetFinanceButtonStates( );

		pCurrentFinance=pFinanceListHead;
		// redraw screen
		fReDrawScreenFlag=TRUE;
	}
}

void BtnFinanceFirstLastPageCallBack(GUI_BUTTON *btn,INT32 reason)
{
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		UINT32	uiButton = MSYS_GetBtnUserData( btn, 0 );

		btn->uiFlags&=~(BUTTON_CLICKED_ON);

		//if its the first page button
		if( uiButton == 0 )
		{
			ClearFinanceList();
			iCurrentPage = 0;
		}

		//else its the last page button
		else
		{
			if (gFinanceRecordPageCount != 0 &&
				LoadInRecords(static_cast<UINT32>(gFinanceRecordPageCount)))
			{
				iCurrentPage = static_cast<INT32>(
					gFinanceRecordPageCount);
			}
		}

		// set button state
		SetFinanceButtonStates( );

		pCurrentFinance=pFinanceListHead;
		// redraw screen
		fReDrawScreenFlag=TRUE;
	}
}


void ProcessTransactionString(CHAR16 *pString, FinanceUnitPtr pFinance)
{
	const std::size_t profile = LaptopRecordPageModel::BoundedIndex(
		pFinance->ubSecondCode, NUM_PROFILES);
	switch( pFinance->ubCode)
	{
		case ACCRUED_INTEREST:
			sgp_swprintf(pString, 512,L"%s", pTransactionText[ACCRUED_INTEREST]);
			break;

		case ANONYMOUS_DEPOSIT:
			sgp_swprintf(pString, 512,L"%s", pTransactionText[ANONYMOUS_DEPOSIT]);
			break;

		case TRANSACTION_FEE:
			sgp_swprintf(pString, 512,L"%s", pTransactionText[TRANSACTION_FEE]);
			break;

		case HIRED_MERC:
			sgp_swprintf(pString, 512,pMessageStrings[ MSG_HIRED_MERC ], gMercProfiles[profile].zNickname );
			break;

		case BOBBYR_PURCHASE:
			sgp_swprintf(pString, 512,L"%s", pTransactionText[ BOBBYR_PURCHASE ]);
			break;

		case PAY_SPECK_FOR_MERC:
			sgp_swprintf(pString, 512,L"%s", pTransactionText[ PAY_SPECK_FOR_MERC ]);
			break;

		case MEDICAL_DEPOSIT:
			sgp_swprintf(pString, 512,pTransactionText[ MEDICAL_DEPOSIT ] , gMercProfiles[profile].zNickname);
			break;

		case IMP_PROFILE:
			sgp_swprintf(pString, 512,L"%s", pTransactionText[ IMP_PROFILE ] );
			break;

		case PURCHASED_INSURANCE:
			sgp_swprintf(pString, 512,pTransactionText[ PURCHASED_INSURANCE ], gMercProfiles[profile].zNickname );
			break;

		case REDUCED_INSURANCE:
			sgp_swprintf(pString, 512, pTransactionText[ REDUCED_INSURANCE ], gMercProfiles[profile].zNickname );
			break;

		case EXTENDED_INSURANCE:
			sgp_swprintf(pString, 512,pTransactionText[ EXTENDED_INSURANCE ], gMercProfiles[profile].zNickname );
			break;

		case CANCELLED_INSURANCE:
			sgp_swprintf(pString, 512,pTransactionText[ CANCELLED_INSURANCE ], gMercProfiles[profile].zNickname );
			break;

		case INSURANCE_PAYOUT:
			sgp_swprintf(pString, 512,pTransactionText[ INSURANCE_PAYOUT ], gMercProfiles[profile].zNickname);
			break;

		case EXTENDED_CONTRACT_BY_1_DAY:
			sgp_swprintf(pString, 512,pTransactionAlternateText[ 1 ], gMercProfiles[profile].zNickname );
			break;

		case EXTENDED_CONTRACT_BY_1_WEEK:
			sgp_swprintf(pString, 512,pTransactionAlternateText[ 2 ], gMercProfiles[profile].zNickname );
			break;

		case EXTENDED_CONTRACT_BY_2_WEEKS:
			sgp_swprintf(pString, 512,pTransactionAlternateText[ 3 ],	gMercProfiles[profile].zNickname );
			break;

		case DEPOSIT_FROM_GOLD_MINE:
		case DEPOSIT_FROM_SILVER_MINE:
			sgp_swprintf( pString, 512, pTransactionText[ 16 ] );
			break;

		case PURCHASED_FLOWERS:
			sgp_swprintf(pString, 512,L"%s", pTransactionText[ PURCHASED_FLOWERS ] );
			break;

		case FULL_MEDICAL_REFUND:
			sgp_swprintf(pString, 512,pTransactionText[ FULL_MEDICAL_REFUND ], gMercProfiles[profile].zNickname);
			break;

		case PARTIAL_MEDICAL_REFUND:
			sgp_swprintf(pString, 512,pTransactionText[ PARTIAL_MEDICAL_REFUND ],	gMercProfiles[profile].zNickname);
			break;

		case NO_MEDICAL_REFUND:
			sgp_swprintf(pString, 512,pTransactionText[ NO_MEDICAL_REFUND ], gMercProfiles[profile].zNickname);
			break;

		case TRANSFER_FUNDS_TO_MERC:
			sgp_swprintf(pString, 512, pTransactionText[ TRANSFER_FUNDS_TO_MERC ],	gMercProfiles[profile].zNickname);
			break;
		case TRANSFER_FUNDS_FROM_MERC:
			sgp_swprintf(pString, 512,pTransactionText[ TRANSFER_FUNDS_FROM_MERC ], gMercProfiles[profile].zNickname);
			break;
		case 	PAYMENT_TO_NPC:
			sgp_swprintf(pString, 512,pTransactionText[ PAYMENT_TO_NPC ], gMercProfiles[profile].zNickname );
			break;
		case( TRAIN_TOWN_MILITIA ):
			{
				CHAR16 str[ 128 ];
				UINT8 ubSectorX;
				UINT8 ubSectorY;
				ubSectorX = (UINT8)SECTORX( pFinance->ubSecondCode );
				ubSectorY = (UINT8)SECTORY( pFinance->ubSecondCode );
				GetSectorIDString( ubSectorX, ubSectorY, 0, str, TRUE );
				sgp_swprintf(pString, 512,pTransactionText[ TRAIN_TOWN_MILITIA ], str );
			}
			break;

		case( SOLD_ITEMS ):
			sgp_swprintf(pString, 512,L"%s", pTransactionText[ SOLD_ITEMS ] );
			break;

		case( PURCHASED_ITEM_FROM_DEALER ):
			sgp_swprintf(pString, 512,pTransactionText[ PURCHASED_ITEM_FROM_DEALER ],	gMercProfiles[profile].zNickname );
			break;

		case( MERC_DEPOSITED_MONEY_TO_PLAYER_ACCOUNT ):
			sgp_swprintf(pString, 512,pTransactionText[ MERC_DEPOSITED_MONEY_TO_PLAYER_ACCOUNT ],	gMercProfiles[profile].zNickname );
			break;

		// HEADROCK HAM 3.6: Paid for Facility Use
		case FACILITY_OPERATIONS:
		case MILITIA_UPKEEP:
		case PRISONER_RANSOM:
		case WHO_SUBSCRIPTION:
		case PMC_CONTRACT:
		case SAM_REPAIR:
		case WORKERS_TRAINED:
			sgp_swprintf( pString, 512, L"%s", pTransactionText[pFinance->ubCode] );
			break;

		case PROMOTE_MILITIA:
			{
				CHAR16 str[128];
				UINT8 ubSectorX;
				UINT8 ubSectorY;
				ubSectorX = (UINT8)SECTORX( pFinance->ubSecondCode );
				ubSectorY = (UINT8)SECTORY( pFinance->ubSecondCode );
				GetSectorIDString( ubSectorX, ubSectorY, 0, str, TRUE );
				sgp_swprintf( pString, 512, pTransactionText[PROMOTE_MILITIA], str );
			}
			break;

		case MINI_EVENT:
			sgp_swprintf(pString, 512,L"%s", pTransactionText[MINI_EVENT]);
			break;

		case REBEL_COMMAND:
			sgp_swprintf(pString, 512,L"%s", pTransactionText[REBEL_COMMAND]);
			break;

		case REBEL_COMMAND_SPENDING:
			sgp_swprintf(pString, 512,L"%s", pTransactionText[REBEL_COMMAND_SPENDING]);
			break;

		case REBEL_COMMAND_BOUNTY_PAYOUT:
			sgp_swprintf(pString, 512,L"%s", pTransactionText[REBEL_COMMAND_BOUNTY_PAYOUT]);
			break;

		default:
			sgp_swprintf(pString, 512, L"missing finance text");
			break;
	}
}


void DisplayFinancePageNumberAndDateRange( void )
{
	CHAR16 sString[50];


	// setup the font stuff
	SetFont(FINANCE_TEXT_FONT);
	SetFontForeground(FONT_BLACK);
	SetFontBackground(FONT_BLACK);
	SetFontShadow(NO_SHADOW);

	if( !pCurrentFinance )
	{
		pCurrentFinance = pFinanceListHead;
		if( !pCurrentFinance )
		{
			swprintf( sString, L"%s %d / %d",pFinanceHeaders[5],
				iCurrentPage + 1,
				static_cast<INT32>(gFinanceRecordPageCount + 1));
				mprintf(PAGE_NUMBER_X, PAGE_NUMBER_Y, L"%s", sString);
			SetFontShadow(DEFAULT_SHADOW);
			return;
		}
	}

	swprintf( sString, L"%s %d / %d",pFinanceHeaders[5],
		iCurrentPage + 1,
		static_cast<INT32>(gFinanceRecordPageCount + 1));
	mprintf(PAGE_NUMBER_X, PAGE_NUMBER_Y, L"%s", sString);

	// reset shadow
	SetFontShadow(DEFAULT_SHADOW);
}


BOOLEAN GetBalanceFromDisk( void )
{
	if(!FileExists(FINANCES_DATA_FILE))
		return TRUE;

	ScopedLaptopFile file(FileOpen(FINANCES_DATA_FILE,
		FILE_OPEN_EXISTING | FILE_ACCESS_READ, FALSE));
	if (!file || !LaptopRecordPageModel::IsWellFormedFile(
		FileGetSize(file.Get()), FinanceFileLayout)) return FALSE;
	INT32 loadedBalance = 0;
	if (!ReadLaptopFileExact(file.Get(), &loadedBalance,
		sizeof(loadedBalance))) return FALSE;
	LaptopSaveInfo.iCurrentBalance = loadedBalance;
	return TRUE;
}

BOOLEAN PersistFinanceTransaction(
	const FinanceUnit& financeRecord, INT32 balance)
{
	ScopedLaptopFile file(FileOpen(FINANCES_DATA_FILE,
		FILE_ACCESS_WRITE | FILE_OPEN_ALWAYS, FALSE));
	if (!file || !LaptopRecordPageModel::IsWellFormedFile(
		FileGetSize(file.Get()), FinanceFileLayout) ||
		!FileSeek(file.Get(), 0, FILE_SEEK_FROM_START) ||
		!WriteLaptopFileExact(file.Get(), &balance, sizeof(balance)) ||
		!FileSeek(file.Get(), 0, FILE_SEEK_FROM_END)) return FALSE;
	return WriteLaptopFileExact(file.Get(), &financeRecord.ubCode,
			sizeof(financeRecord.ubCode)) &&
		WriteLaptopFileExact(file.Get(), &financeRecord.ubSecondCode,
			sizeof(financeRecord.ubSecondCode)) &&
		WriteLaptopFileExact(file.Get(), &financeRecord.uiDate,
			sizeof(financeRecord.uiDate)) &&
		WriteLaptopFileExact(file.Get(), &financeRecord.iAmount,
			sizeof(financeRecord.iAmount)) &&
		WriteLaptopFileExact(file.Get(), &financeRecord.iBalanceToDate,
			sizeof(financeRecord.iBalanceToDate));
}

void SetLastPageInRecords( void )
{
	gFinanceRecordPageCount = LaptopRecordPageModel::PageCount(
		FinanceRecordCountOnDisk(), NUM_RECORDS_PER_PAGE);
}


BOOLEAN LoadPreviousPage( void )
{
	if (iCurrentPage <= 0) return FALSE;
	if (iCurrentPage == 1)
	{
		ClearFinanceList();
		iCurrentPage = 0;
		return TRUE;
	}

	const UINT32 previousPage = static_cast<UINT32>(iCurrentPage - 1);
	ClearFinanceList();
	if (LoadInRecords(previousPage))
	{
		iCurrentPage--;
		return TRUE;
	}
	LoadInRecords(static_cast<UINT32>(iCurrentPage));
	return FALSE;
}

BOOLEAN LoadNextPage( void )
{
	if (iCurrentPage < 0 ||
		static_cast<std::size_t>(iCurrentPage) >= gFinanceRecordPageCount)
		return FALSE;
	const UINT32 nextPage = static_cast<UINT32>(iCurrentPage + 1);
	ClearFinanceList();
	if (LoadInRecords(nextPage))
	{
		iCurrentPage++;
		return TRUE;
	}
	if (iCurrentPage > 0)
		LoadInRecords(static_cast<UINT32>(iCurrentPage));
	return FALSE;
}

BOOLEAN LoadInRecords( UINT32 uiPage )
{
	if (uiPage == 0 || !FileExists(FINANCES_DATA_FILE)) return FALSE;
	ScopedLaptopFile file(FileOpen(FINANCES_DATA_FILE,
		FILE_OPEN_EXISTING | FILE_ACCESS_READ, FALSE));
	if (!file) return FALSE;
	const std::size_t fileBytes = FileGetSize(file.Get());
	if (!LaptopRecordPageModel::IsWellFormedFile(
		fileBytes, FinanceFileLayout)) return FALSE;
	const std::size_t recordCount =
		LaptopRecordPageModel::RecordCount(fileBytes, FinanceFileLayout);
	const std::size_t page = static_cast<std::size_t>(uiPage - 1);
	const std::size_t pageCount = LaptopRecordPageModel::PageCount(
		recordCount, NUM_RECORDS_PER_PAGE);
	if (!LaptopRecordPageModel::HasZeroBasedPage(page, pageCount))
		return FALSE;
	const std::size_t offset = LaptopRecordPageModel::PageByteOffset(
		page, NUM_RECORDS_PER_PAGE, FinanceFileLayout);
	if (offset == LaptopRecordPageModel::NoOffset || offset > UINT32_MAX ||
		!FileSeek(file.Get(), static_cast<UINT32>(offset),
			FILE_SEEK_FROM_START)) return FALSE;

	ClearFinanceList();
	const std::size_t recordsToRead = LaptopRecordPageModel::RecordsOnPage(
		recordCount, page, NUM_RECORDS_PER_PAGE);
	for (std::size_t index = 0; index < recordsToRead; ++index)
	{
		FinanceRecordData record;
		if (!ReadFinanceRecordExact(file.Get(), record) ||
			ProcessAndEnterAFinacialRecord(record.code, record.date,
				record.amount, record.secondCode,
				record.balanceToDate) == InvalidFinanceRecordId)
		{
			ClearFinanceList();
			return FALSE;
		}
	}
	pCurrentFinance = pFinanceListHead;
	return pCurrentFinance != NULL;
}

INT32 GetPreviousDaysBalance( void )
{
	std::vector<FinanceRecordData> records;
	if (!LoadFinanceLedger(records)) return 0;
	const UINT32 now = GetWorldTotalMin();
	for (auto record = records.rbegin(); record != records.rend(); ++record)
	{
		if (LaptopRecordPageModel::IsRecordOnDayOffset(
			record->date, now, 2, 1500)) return record->balanceToDate;
	}
	return 0;
}



INT32 GetTodaysBalance( void )
{
	std::vector<FinanceRecordData> records;
	if (!LoadFinanceLedger(records)) return 0;
	const UINT32 now = GetWorldTotalMin();
	for (auto record = records.rbegin(); record != records.rend(); ++record)
	{
		if (LaptopRecordPageModel::IsRecordOnDayOffset(
			record->date, now, 1, 1500)) return record->balanceToDate;
	}
	return 0;
}



INT32 GetPreviousDaysIncome( void )
{
	std::vector<FinanceRecordData> records;
	if (!LoadFinanceLedger(records)) return 0;
	const UINT32 now = GetWorldTotalMin();
	INT32 total = 0;
	for (const FinanceRecordData& record : records)
	{
		if (LaptopRecordPageModel::IsRecordOnDayOffset(
			record.date, now, 1, 1500) &&
			(record.code == DEPOSIT_FROM_GOLD_MINE ||
				record.code == DEPOSIT_FROM_SILVER_MINE))
		{
			total = LaptopRecordPageModel::SaturatingAdd(total, record.amount);
		}
	}
	return total;
}


INT32 GetTodaysDaysIncome( void )
{
	std::vector<FinanceRecordData> records;
	if (!LoadFinanceLedger(records)) return 0;
	const UINT32 now = GetWorldTotalMin();
	INT32 total = 0;
	for (const FinanceRecordData& record : records)
	{
		if (LaptopRecordPageModel::IsRecordOnDayOffset(
			record.date, now, 0, 1500) &&
			(record.code == DEPOSIT_FROM_GOLD_MINE ||
				record.code == DEPOSIT_FROM_SILVER_MINE))
		{
			total = LaptopRecordPageModel::SaturatingAdd(total, record.amount);
		}
	}
	return total;
}

void SetFinanceButtonStates( void )
{
	const bool hasPrevious = iCurrentPage > 0;
	const bool hasNext = iCurrentPage >= 0 &&
		static_cast<std::size_t>(iCurrentPage) < gFinanceRecordPageCount;
	if (hasPrevious)
	{
		EnableButton(giFinanceButton[PREV_PAGE_BUTTON]);
		EnableButton(giFinanceButton[FIRST_PAGE_BUTTON]);
	}
	else
	{
		DisableButton(giFinanceButton[PREV_PAGE_BUTTON]);
		DisableButton(giFinanceButton[FIRST_PAGE_BUTTON]);
	}
	if (hasNext)
	{
		EnableButton(giFinanceButton[NEXT_PAGE_BUTTON]);
		EnableButton(giFinanceButton[LAST_PAGE_BUTTON]);
	}
	else
	{
		DisableButton(giFinanceButton[NEXT_PAGE_BUTTON]);
		DisableButton(giFinanceButton[LAST_PAGE_BUTTON]);
	}
}


INT32 GetTodaysOtherDeposits( void )
{
	std::vector<FinanceRecordData> records;
	if (!LoadFinanceLedger(records)) return 0;
	const UINT32 now = GetWorldTotalMin();
	INT32 total = 0;
	for (const FinanceRecordData& record : records)
	{
		if (LaptopRecordPageModel::IsRecordOnDayOffset(
			record.date, now, 0, 1500) && record.amount > 0 &&
			record.code != DEPOSIT_FROM_GOLD_MINE &&
			record.code != DEPOSIT_FROM_SILVER_MINE)
		{
			total = LaptopRecordPageModel::SaturatingAdd(total, record.amount);
		}
	}
	return total;
}


INT32 GetYesterdaysOtherDeposits( void )
{
	std::vector<FinanceRecordData> records;
	if (!LoadFinanceLedger(records)) return 0;
	const UINT32 now = GetWorldTotalMin();
	INT32 total = 0;
	for (const FinanceRecordData& record : records)
	{
		if (LaptopRecordPageModel::IsRecordOnDayOffset(
			record.date, now, 1, 1500) && record.amount > 0 &&
			record.code != DEPOSIT_FROM_GOLD_MINE &&
			record.code != DEPOSIT_FROM_SILVER_MINE)
		{
			total = LaptopRecordPageModel::SaturatingAdd(total, record.amount);
		}
	}
	return total;
}


INT32 GetTodaysDebits( void )
{
	INT32 result = LaptopRecordPageModel::SaturatingSubtract(
		GetCurrentBalance(), GetTodaysBalance());
	result = LaptopRecordPageModel::SaturatingSubtract(
		result, GetTodaysDaysIncome());
	return LaptopRecordPageModel::SaturatingSubtract(
		result, GetTodaysOtherDeposits());
}

INT32 GetYesterdaysDebits( void )
{
	INT32 result = LaptopRecordPageModel::SaturatingSubtract(
		GetTodaysBalance(), GetPreviousDaysBalance());
	result = LaptopRecordPageModel::SaturatingSubtract(
		result, GetPreviousDaysIncome());
	return LaptopRecordPageModel::SaturatingSubtract(
		result, GetYesterdaysOtherDeposits());
}

