#include "laptop.h"
#include "email.h"
#include "CampaignLaptopCommunicationsPolicy.h"
#include "GameContext.h"
#include "LaptopEmailListModel.h"
#include "LaptopPageResourceOwner.h"
#include "LaptopRecordFile.h"
#include "Utilities.h"
#include "WCheck.h"
#include "DEBUG.H"
#include "WordWrap.h"
#include "Encrypted File.h"
#include "Cursors.h"
#include "Soldier Profile.h"
#include "CharProfile.h"
#include "IMP Compile Character.h"
#include "IMP Portraits.h"
#include "Game Clock.h"
#include "AimMembers.h"
#include "random.h"
#include "Text.h"
#include "TextCatalog.h"
#include "LaptopSave.h"
#include "PostalService.h"
#include "faces.h"
#include "GameSettings.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include "soldier profile type.h"
#include "message.h"

#include "Ja25_Tactical.h"
#include "Ja25 Strategic Ai.h"
#include "ub_config.h"

constexpr const char* EMAIL_EDT_FILE_JA25 = "BINARYDATA\\Email25.edt";
constexpr const char* EMAIL_EDT_FILE_JA2 = "BINARYDATA\\Email.edt";

namespace
{
using CommunicationsPolicy = CampaignLaptopCommunicationsPolicy;

CommunicationsPolicy CurrentCommunicationsPolicy()
{
	return CommunicationsPolicy(GetGameContext().capabilities());
}

bool UsesArulcoEmailCatalog(UINT8 emailVersion)
{
	return emailVersion == TYPE_EMAIL_BOBBY_R_EMAIL_JA2_EDT ||
		emailVersion == TYPE_EMAIL_INSURANCE_COMPANY_EMAIL_JA2_EDT ||
		emailVersion == TYPE_EMAIL_DEAD_MERC_AIM_SITE_EMAIL_JA2_EDT;
}

bool UsesCampaignEmailCatalog(UINT8 emailVersion)
{
	return emailVersion == TYPE_EMAIL_EMAIL_EDT ||
		emailVersion == TYPE_EMAIL_EMAIL_EDT_NAME_MERC;
}

bool LoadEmailRecord(
	UINT8 emailVersion, INT32 messageOffset, CHAR16* destination)
{
	if (!destination ||
		!LaptopEmailListModel::CanStoreUnsigned16(messageOffset)) return false;
	destination[0] = L'\0';
	if (UsesArulcoEmailCatalog(emailVersion))
	{
		LoadEncryptedDataFromFile(EMAIL_EDT_FILE_JA2, destination,
			MAIL_STRING_SIZE * messageOffset, MAIL_STRING_SIZE);
		return true;
	}
	if (!UsesCampaignEmailCatalog(emailVersion)) return false;

	const CommunicationsPolicy policy = CurrentCommunicationsPolicy();
	const char* campaignFile =
		policy.usesUnfinishedBusinessCatalog() && FileExists(EMAIL_EDT_FILE_JA25)
			? EMAIL_EDT_FILE_JA25
			: EMAIL_EDT_FILE_JA2;
	LoadEncryptedDataFromFile(campaignFile, destination,
		MAIL_STRING_SIZE * messageOffset, MAIL_STRING_SIZE);
	return true;
}
}

using namespace std;

//static EmailPtr pEmailList;
EmailPtr pEmailList;
static PagePtr	pPageList;
static INT32 iLastPage=-1;
static INT32 iCurrentPage=0;
INT32 iDeleteId=0;
BOOLEAN fUnReadMailFlag=FALSE;
BOOLEAN fOldUnreadFlag=TRUE;
BOOLEAN fNewMailFlag=FALSE;
BOOLEAN fOldNewMailFlag=FALSE;
BOOLEAN fDisplayMessageFlag=FALSE;
BOOLEAN fOldDisplayMessageFlag=FALSE;
BOOLEAN fDeleteMailFlag=FALSE;
BOOLEAN fReDrawMessageFlag = FALSE;
BOOLEAN fOnLastPageFlag=FALSE;
BOOLEAN fOpenMostRecentUnReadFlag = FALSE;
INT32 iViewerPositionY=0;

INT32 giMessageId = -1;
INT32 giPrevMessageId = -1;
INT32 giMessagePage = -1;
INT32 giNumberOfPagesToCurrentEmail = -1;
UINT32 guiEmailWarning;

#define EMAIL_TOP_BAR_HEIGHT 22

#define LIST_MIDDLE_COUNT 18
// object positions
#define TITLE_X 0+LAPTOP_SCREEN_UL_X
#define TITLE_Y 0+LAPTOP_SCREEN_UL_Y

#define STAMP_X								LAPTOP_SCREEN_UL_X
#define STAMP_Y								LAPTOP_SCREEN_UL_Y
/*
#define TOP_X 0+LAPTOP_SCREEN_UL_X
#define TOP_Y 62+LAPTOP_SCREEN_UL_Y

#define BOTTOM_X 0+LAPTOP_SCREEN_UL_X
#define BOTTOM_Y 359+LAPTOP_SCREEN_UL_Y
*/
#define MIDDLE_X 0+LAPTOP_SCREEN_UL_X
#define MIDDLE_Y							iScreenHeightOffset + 72 + EMAIL_TOP_BAR_HEIGHT
#define MIDDLE_WIDTH 19


// new graphics
#define EMAIL_LIST_WINDOW_Y					iScreenHeightOffset + 22
#define EMAIL_TITLE_BAR_X					iScreenWidthOffset + 5

// email columns
#define SENDER_X LAPTOP_SCREEN_UL_X+65
#define SENDER_WIDTH 246-158

#define DATE_X LAPTOP_SCREEN_UL_X+428
#define DATE_WIDTH 592-527

#define SUBJECT_X LAPTOP_SCREEN_UL_X+175
#define SUBJECT_WIDTH						254
#define INDIC_X								iScreenWidthOffset + 128
#define INDIC_WIDTH 155-123
#define INDIC_HEIGHT 145-128

#define LINE_WIDTH 592-121

#define MESSAGE_WIDTH						528 - 125
#define MESSAGE_COLOR FONT_BLACK
#define MESSAGE_GAP 2

#define MESSAGE_HEADER_WIDTH 209-151
#define MESSAGE_HEADER_X VIEWER_X+4

#define VIEWER_HEAD_X						iScreenWidthOffset + 140
#define VIEWER_HEAD_Y						iScreenHeightOffset + 9
#define VIEWER_HEAD_WIDTH 445-VIEWER_HEAD_X
#define MAX_BUTTON_COUNT 1
#define VIEWER_WIDTH 500
#define VIEWER_HEIGHT 195

#define MESSAGEX_X							iScreenWidthOffset + 425
#define MESSAGEX_Y							iScreenHeightOffset + 6

#define EMAIL_WARNING_X						iScreenWidthOffset + 210
#define EMAIL_WARNING_Y						iScreenHeightOffset + 140
#define EMAIL_WARNING_WIDTH 254
#define EMAIL_WARNING_HEIGHT 138

#define NEW_BTN_X EMAIL_WARNING_X +(338-245)
#define NEW_BTN_Y EMAIL_WARNING_Y +(278-195)

#define EMAIL_TEXT_FONT				FONT10ARIAL
#define MESSAGE_FONT					EMAIL_TEXT_FONT
#define EMAIL_HEADER_FONT			FONT14ARIAL
#define EMAIL_WARNING_FONT		FONT12ARIAL


// the max number of pages to an email
#define MAX_NUMBER_EMAIL_PAGES 100

#define NEXT_PAGE_X LAPTOP_UL_X + 562
#define NEXT_PAGE_Y							iScreenHeightOffset + 51

#define PREVIOUS_PAGE_X NEXT_PAGE_X - 21
#define PREVIOUS_PAGE_Y NEXT_PAGE_Y

#define ENVELOPE_BOX_X						iScreenWidthOffset + 116

#define FROM_BOX_X							iScreenWidthOffset + 166
#define FROM_BOX_WIDTH 246-160

#define SUBJECT_BOX_X						iScreenWidthOffset + 276
#define SUBJECT_BOX_WIDTH 528-249

#define DATE_BOX_X							iScreenWidthOffset + 530
#define DATE_BOX_WIDTH 594-530

#define FROM_BOX_Y							iScreenHeightOffset + 51 + EMAIL_TOP_BAR_HEIGHT
#define TOP_HEIGHT 118-95

#define EMAIL_TITLE_FONT FONT14ARIAL
#define EMAIL_TITLE_X						iScreenWidthOffset + 140
#define EMAIL_TITLE_Y						iScreenHeightOffset + 33
#define VIEWER_MESSAGE_BODY_START_Y VIEWER_Y+72
#define MIN_MESSAGE_HEIGHT_IN_LINES 5


#define INDENT_Y_OFFSET 310
#define INDENT_X_OFFSET 325
#define INDENT_X_WIDTH						544 - 481

// the position of the page number being displayed in the email program
#define PAGE_NUMBER_X						iScreenWidthOffset + 516
#define PAGE_NUMBER_Y						iScreenHeightOffset + 58

// defines for location of message 'title'/'headers'

#define MESSAGE_FROM_Y VIEWER_Y+28

#define MESSAGE_DATE_Y MESSAGE_FROM_Y

#define MESSAGE_SUBJECT_Y MESSAGE_DATE_Y+16


#define SUBJECT_LINE_X VIEWER_X+47
#define SUBJECT_LINE_Y VIEWER_Y+42
#define SUBJECT_LINE_WIDTH 278-47

//max number of lines can be shown in an opened email messagebox
#define MAX_EMAIL_LINES 20
// maximum size of a email message page, so not to overrun the bottom of the screen
#define MAX_EMAIL_MESSAGE_PAGE_SIZE ( GetFontHeight( MESSAGE_FONT ) + MESSAGE_GAP ) * MAX_EMAIL_LINES

enum{
	PREVIOUS_BUTTON=0,
	NEXT_BUTTON,
};


// X button position
#define BUTTON_X							VIEWER_X + 396
#define BUTTON_Y							VIEWER_Y + 3
#define BUTTON_LOWER_Y						BUTTON_Y + 22
#define PREVIOUS_PAGE_BUTTON_X				VIEWER_X + 302
#define NEXT_PAGE_BUTTON_X					VIEWER_X +395
#define DELETE_BUTTON_X						NEXT_PAGE_BUTTON_X
#define LOWER_BUTTON_Y						BUTTON_Y + 299


BOOLEAN fSortDateUpwards = FALSE;
BOOLEAN fSortSenderUpwards = FALSE;
BOOLEAN fSortSubjectUpwards = FALSE;

// mouse regions
MOUSE_REGION pEmailRegions[MAX_MESSAGES_PAGE];
extern	MOUSE_REGION pScreenMask; // symbol already defined in laptop.cpp (jonathanl)
MOUSE_REGION pDeleteScreenMask;
MOUSE_REGION pMailViewMessageRegion;

// the email info struct to speed up email
EmailPageInfoStruct pEmailPageInfo[ MAX_NUMBER_EMAIL_PAGES ];

//buttons
INT32 giMessageButton[MAX_BUTTON_COUNT];
INT32 giMessageButtonImage[MAX_BUTTON_COUNT];
INT32 giDeleteMailButton[2];
INT32 giDeleteMailButtonImage[2];
INT32 giSortButton[4];
INT32 giSortButtonImage[4];
INT32 giNewMailButton[1];
INT32 giNewMailButtonImage[1];
INT32 giMailMessageButtons[3];
INT32 giMailMessageButtonsImage[3];
INT32 giMailPageButtons[ 2 ];
INT32 giMailPageButtonsImage[ 2 ];


// the message record list, for the currently displayed message
RecordPtr pMessageRecordList=NULL;

// video handles
UINT32 guiEmailTitle;
UINT32 guiEmailStamp;
UINT32 guiEmailBackground;
UINT32 guiEmailIndicator;
UINT32 guiEmailMessage;
UINT32 guiMAILDIVIDER;

INT16 giCurrentIMPSlot = PLAYER_GENERATED_CHARACTER_ID;
EMAIL_MERC_AVAILABLE_VALUES EmailMercAvailableText[NUM_PROFILES];
EMAIL_MERC_LEVEL_UP_VALUES EmailMercLevelUpText[NUM_PROFILES];
EMAIL_OTHER_VALUES EmailOtherText[EMAIL_INDEX];
BOOLEAN ReadXMLEmail = TRUE; // TRUE - read email from XML, FALSE - read email from EDT
EMAIL_TYPE gEmailT[EMAIL_VAL];
std::vector<EMAIL_XML> gEmails{};
BOOLEAN SaveNewEmailDataToSaveGameFile( HWFILE hFile );
BOOLEAN LoadNewEmailDataFromLoadGameFile( HWFILE hFile );

namespace
{
LaptopPageResourceOwner gEmailPageResources;
LaptopPageResourceOwner gEmailMessageResources;
LaptopPageResourceOwner gEmailNewMailResources;
LaptopPageResourceOwner gEmailDeleteResources;
bool gEmailRecordBuildFailed = false;

template<std::size_t Capacity>
bool CopyEmailText(CHAR16 (&destination)[Capacity],
	const CHAR16* source) noexcept
{
	return LaptopEmailListModel::CopyText(destination, source);
}

template<std::size_t Capacity>
bool AppendEmailText(CHAR16 (&destination)[Capacity],
	const CHAR16* source) noexcept
{
	return LaptopEmailListModel::AppendText(destination, source);
}

void FreeEmailNodes(EmailPtr head) noexcept
{
	while (head)
	{
		EmailPtr next = head->Next;
		if (head->pSubject) MemFree(head->pSubject);
		MemFree(head);
		head = next;
	}
}

void FreeEmailPages(PagePtr head) noexcept
{
	while (head)
	{
		PagePtr next = head->Next;
		MemFree(head);
		head = next;
	}
}

class EmailPageListOwner
{
public:
	~EmailPageListOwner() { FreeEmailPages(head_); }
	EmailPageListOwner(const EmailPageListOwner&) = delete;
	EmailPageListOwner& operator=(const EmailPageListOwner&) = delete;
	EmailPageListOwner() = default;

	bool append(INT32 messageId)
	{
		if (!tail_ || usedOnTail_ == MAX_MESSAGES_PAGE)
		{
			PagePtr page = static_cast<PagePtr>(MemAlloc(sizeof(Page)));
			if (!page) return false;
			std::fill_n(page->iIds, MAX_MESSAGES_PAGE, -1);
			page->iPageId = static_cast<INT32>(pageCount_);
			page->Prev = tail_;
			page->Next = nullptr;
			if (tail_) tail_->Next = page;
			else head_ = page;
			tail_ = page;
			usedOnTail_ = 0;
			++pageCount_;
		}
		tail_->iIds[usedOnTail_++] = messageId;
		return true;
	}

	PagePtr release() noexcept
	{
		PagePtr result = head_;
		head_ = nullptr;
		tail_ = nullptr;
		usedOnTail_ = 0;
		pageCount_ = 0;
		return result;
	}

	std::size_t pageCount() const noexcept { return pageCount_; }

private:
	PagePtr head_ = nullptr;
	PagePtr tail_ = nullptr;
	std::size_t usedOnTail_ = 0;
	std::size_t pageCount_ = 0;
};

bool BuildEmailPages(EmailPtr head, EmailPtr excluded,
	EmailPtr appended, EmailPageListOwner& pages)
{
	for (EmailPtr email = head; email; email = email->Next)
	{
		if (email != excluded && !pages.append(email->iId)) return false;
	}
	return !appended || pages.append(appended->iId);
}

std::size_t EmailMessageCount(EmailPtr head) noexcept
{
	std::size_t count = 0;
	for (; head; head = head->Next) ++count;
	return count;
}

void CommitEmailPages(EmailPageListOwner& pages, std::size_t messageCount)
{
	const std::size_t pageCount = pages.pageCount();
	PagePtr oldPages = pPageList;
	pPageList = pages.release();
	iLastPage = pageCount == 0 ? -1 : static_cast<INT32>(pageCount - 1);
	iCurrentPage = static_cast<INT32>(
		LaptopEmailListModel::NormalizeInboxPage(
			static_cast<std::size_t>(std::max(iCurrentPage, 0)),
			messageCount, MAX_MESSAGES_PAGE));
	FreeEmailPages(oldPages);
}

PagePtr FindEmailPage(INT32 pageId) noexcept
{
	if (pageId < 0) return nullptr;
	for (PagePtr page = pPageList; page; page = page->Next)
	{
		if (page->iPageId == pageId) return page;
	}
	return nullptr;
}

struct EmailNodeDeleter
{
	void operator()(EmailPtr email) const noexcept
	{
		if (!email) return;
		if (email->pSubject) MemFree(email->pSubject);
		MemFree(email);
	}
};

using UniqueEmailNode = std::unique_ptr<Email, EmailNodeDeleter>;

bool IsProfileBackedEmail(UINT8 emailType) noexcept
{
	return emailType == TYPE_EMAIL_AIM_AVAILABLE ||
		emailType == TYPE_EMAIL_MERC_LEVEL_UP ||
		emailType == TYPE_EMAIL_EMAIL_EDT_NAME_MERC;
}
}

// the enumeration of headers
enum{
	FROM_HEADER=0,
	SUBJECT_HEADER,
	RECD_HEADER,
};


// position of header text on the email list
#define FROM_X									iScreenWidthOffset + 205
#define FROM_Y FROM_BOX_Y + 5
#define SUBJECTHEAD_X							iScreenWidthOffset + 368
#define RECD_X									iScreenWidthOffset + 550


// current line in the email list that is highlighted, -1 is no line highlighted
INT32 iHighLightLine=-1;

// whther or not we need to redraw the new mail box
BOOLEAN fReDrawNewMailFlag = FALSE;
INT32 giNumberOfMessageToEmail = 0;
INT32 iTotalHeight = 0;

// function list
void SwapMessages(INT32 iIdA, INT32 iIdB);
void PlaceMessagesinPages();
BOOLEAN	fFirstTime=TRUE;
void EmailBtnCallBack(MOUSE_REGION * pRegion, INT32 iReason );
void EmailMvtCallBack(MOUSE_REGION * pRegion, INT32 iReason );
void PreviousRegionButtonCallback(GUI_BUTTON *btn,INT32 reason);
void NextRegionButtonCallback(GUI_BUTTON *btn,INT32 reason);
void SetUnNewMessages();
INT32 DisplayEmailMessage(EmailPtr pMail);
void AddDeleteRegionsToMessageRegion(INT32 iViewerY);
void DeleteEmail();
BOOLEAN DisplayDeleteNotice(EmailPtr pMail);
void CreateDestroyDeleteNoticeMailButton();
void DisplayTextOnTitleBar( void );
void DisplayEmailMessageSubjectDateFromLines( EmailPtr pMail, INT32 iViewerY );
void DrawEmailMessageDisplayTitleText( INT32 iViewerY );
BOOLEAN CreateMailScreenButtons(LaptopPageResourceOwner& owner);
void DrawLineDividers( void );
void FromCallback(GUI_BUTTON *btn, INT32 iReason );
void SubjectCallback(GUI_BUTTON *btn,	INT32 iReason );
void DateCallback(GUI_BUTTON *btn,	INT32 iReason );
void ReadCallback(GUI_BUTTON *btn,	INT32 iReason );
void BtnPreviousEmailPageCallback(GUI_BUTTON *btn,INT32 reason);
void BtnNextEmailPageCallback(GUI_BUTTON *btn,INT32 reason);
void ViewMessageRegionCallBack( MOUSE_REGION * pRegion, INT32 iReason );
void DisplayEmailList();
void ClearOutEmailMessageRecordsList( void );
void AddEmailRecordToList(const CHAR16* pString);
void UpDateMessageRecordList( void );
void HandleAnySpecialEmailMessageEvents(INT32 iMessageId );
BOOLEAN HandleMailSpecialMessages( UINT16 usMessageId, INT32 *iResults,	EmailPtr pMail );
void HandleUnfinishedBusinessMailSpecialMessages(
	INT32* iResults, EmailPtr pMail);
void AddBobbyREmailJA2(INT32 iMessageOffset, INT32 iMessageLength, UINT8 ubSender, INT32 iDate, INT32 iCurrentIMPPosition, INT16 iCurrentShipmentDestinationID, UINT8 EmailType );
void HandleIMPCharProfileResultsMessage(	void );
void HandleEmailViewerButtonStates( void );
void BtnDeleteCallback(GUI_BUTTON *btn, INT32 iReason );
void UpdateStatusOfNextPreviousButtons( void );
void DisplayWhichPageOfEmailProgramIsDisplayed( void );
void OpenMostRecentUnreadEmail( void );
BOOLEAN DisplayNumberOfPagesToThisEmail( INT32 iViewerY );
INT32 GetNumberOfPagesToEmail( );
void PreProcessEmail( EmailPtr pMail );
void ModifyInsuranceEmails( UINT16 usMessageId, INT32 *iResults, EmailPtr pMail );
BOOLEAN ReplaceMercNameAndAmountWithProperData(
	CHAR16 (&pFinishedString)[MAIL_STRING_SIZE], EmailPtr pMail);
extern INT16 gusCurShipmentDestinationID;
extern CPostalService gPostalService;


static BOOLEAN CreateNextPreviousEmailPageButtons(
	LaptopPageResourceOwner& owner);

static BOOLEAN InitializeMouseRegions(LaptopPageResourceOwner& owner)
{
	INT32 iCounter=0;

	// init mouseregions
	for(iCounter=0; iCounter <MAX_MESSAGES_PAGE; iCounter++)
	{
		MSYS_DefineRegion(&pEmailRegions[iCounter],MIDDLE_X ,((INT16)(MIDDLE_Y+iCounter*MIDDLE_WIDTH)), MIDDLE_X+LINE_WIDTH ,(INT16)(MIDDLE_Y+iCounter*MIDDLE_WIDTH+MIDDLE_WIDTH),
			MSYS_PRIORITY_NORMAL+2,MSYS_NO_CURSOR, EmailMvtCallBack, EmailBtnCallBack );
		if (!owner.addRegion(pEmailRegions[iCounter])) return FALSE;
		MSYS_SetRegionUserData(&pEmailRegions[iCounter],0,iCounter);
	}

	return CreateNextPreviousEmailPageButtons(owner);
}
void GameInitEmail()
{
	pEmailList=NULL;
	pPageList=NULL;

	iLastPage=-1;

	iCurrentPage=0;
	iDeleteId=0;

	// reset display message flag
	fDisplayMessageFlag=FALSE;

	// reset page being displayed
	giMessagePage = 0;
}

BOOLEAN EnterEmail()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner staged;

	gEmailMessageResources.clear();
	gEmailDeleteResources.clear();
	gEmailPageResources.clear();
	fOldDisplayMessageFlag = FALSE;

	iCurrentPage = static_cast<INT32>(
		LaptopEmailListModel::NormalizeInboxPage(
			static_cast<std::size_t>(
				std::max(LaptopSaveInfo.iCurrentEmailPage, 0)),
			EmailMessageCount(pEmailList), MAX_MESSAGES_PAGE));

	// title bar
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\programtitlebar.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiEmailTitle)) return FALSE;

	// the list background
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\Mailwindow.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiEmailBackground)) return FALSE;

	// the indication/notification box
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\MailIndicator.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiEmailIndicator)) return FALSE;

	// the message background
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\emailviewer.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiEmailMessage)) return FALSE;

	// the message background
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\maillistdivider.sti", VObjectDesc.ImageFile);
	if (!staged.addVideoObject(&VObjectDesc, guiMAILDIVIDER)) return FALSE;

	//AddEmail(IMP_EMAIL_PROFILE_RESULTS, IMP_EMAIL_PROFILE_RESULTS_LENGTH, IMP_PROFILE_RESULTS, GetWorldTotalMin( ), -1, -1 );
	// initialize mouse regions
	if (!InitializeMouseRegions(staged)) return FALSE;

	// create buttons
	if (!CreateMailScreenButtons(staged)) return FALSE;
	gEmailPageResources = std::move(staged);

	// marks these buttons dirty
	MarkButtonsDirty( );

	// no longer fitrst time in email
	fFirstTime = FALSE;

	// reset current page of the message being displayed
	giMessagePage = 0;

	// render email background and text
	RenderEmail();
	
	//AddEmail( MERC_REPLY_GRIZZLY, MERC_REPLY_LENGTH_GRIZZLY, GRIZZLY_MAIL, GetWorldTotalMin(), -1, -1 );
	//RenderButtons( );
	
	return( TRUE );
}

void ExitEmail()
{
	LaptopSaveInfo.iCurrentEmailPage = iCurrentPage;

	// clear out message record list
	ClearOutEmailMessageRecordsList( );

	// displayed message?...get rid of it
	fDisplayMessageFlag = FALSE;
	fOldDisplayMessageFlag = FALSE;
	gEmailMessageResources.clear();
	giMessageId = -1;
	fReDrawMessageFlag = TRUE;

	// delete mail notice?...get rid of it
	fDeleteMailFlag=FALSE;
	gEmailDeleteResources.clear();

	// reset flags of new messages
	SetUnNewMessages();

	gEmailPageResources.clear();
}

void HandleEmail( void )
{
	INT32 iViewerY = 0;
	static BOOLEAN fEmailListBeenDrawAlready = FALSE;
	//RenderButtonsFastHelp( );
	
	// check if email message record list needs to be updated
	UpDateMessageRecordList( );

	// does email list need to be draw, or can be drawn
	if( ( (!fDisplayMessageFlag)&&(!fNewMailFlag) && ( !fDeleteMailFlag ) )&&( fEmailListBeenDrawAlready == FALSE ) )
	{
		DisplayEmailList();
		fEmailListBeenDrawAlready = TRUE;
	}
	// if the message flag, show message
	else if((fDisplayMessageFlag)&&(fReDrawMessageFlag))
	{
		// redisplay list
		DisplayEmailList();

		// this simply redraws message without button manipulation
		DisplayEmailMessage(GetEmailMessage(giMessageId));
		fEmailListBeenDrawAlready = FALSE;
	}
	else if((fDisplayMessageFlag)&&(!fOldDisplayMessageFlag))
	{
		// redisplay list
		DisplayEmailList();

		// this simply redraws message with button manipulation
		iViewerY = DisplayEmailMessage(GetEmailMessage(giMessageId));
		AddDeleteRegionsToMessageRegion( iViewerY );
		fEmailListBeenDrawAlready = FALSE;

	}

	// not displaying anymore?
	if( ( fDisplayMessageFlag == FALSE ) && ( fOldDisplayMessageFlag ) )
	{
		// then clear it out
		ClearOutEmailMessageRecordsList( );
	}
	
	// if new message is being displayed...check to see if it's buttons need to be created or destroyed
	AddDeleteRegionsToMessageRegion( 0 );

	// same with delete notice
	CreateDestroyDeleteNoticeMailButton();

	// if delete notice needs to be displayed?...display it
	if(fDeleteMailFlag)
		DisplayDeleteNotice(GetEmailMessage(iDeleteId));
	
	// update buttons
	HandleEmailViewerButtonStates( );

	// redraw screen
	//ReDraw();

	// handle buttons states
	UpdateStatusOfNextPreviousButtons( );

	if( fOpenMostRecentUnReadFlag == TRUE )
	{
		// enter email due to email icon on program panel
		OpenMostRecentUnreadEmail( );
		fOpenMostRecentUnReadFlag = FALSE;

	}
}

void RenderEmail( void )
{
	HVOBJECT hHandle;

	// get and blt the email list background
	GetVideoObject( &hHandle, guiEmailBackground );

	BltVideoObject( FRAME_BUFFER, hHandle, 0,LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_UL_Y + 22, VO_BLT_SRCTRANSPARENCY,NULL);
	
	// get and blt the email title bar
	GetVideoObject( &hHandle, guiEmailTitle );
	BltVideoObject( FRAME_BUFFER, hHandle, 0,LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_UL_Y - 2, VO_BLT_SRCTRANSPARENCY,NULL );

	// show text on titlebar
	DisplayTextOnTitleBar( );

	// redraw list if no graphics are being displayed on top of it
	//if((!fDisplayMessageFlag)&&(!fNewMailFlag))
	//{
	DisplayEmailList( );
	//}

	// redraw line dividers
	DrawLineDividers( );
	
	// show next/prev page buttons depending if there are next/prev page
	// draw headers for buttons
	// display border
	GetVideoObject(&hHandle, guiLaptopBACKGROUND);

	BltVideoObject(FRAME_BUFFER, hHandle, 0, iScreenWidthOffset + 108, iScreenHeightOffset + 23, VO_BLT_SRCTRANSPARENCY,NULL);
	
	ReDisplayBoxes( );

	BlitTitleBarIcons(	);
	
	// show which page we are on
	DisplayWhichPageOfEmailProgramIsDisplayed( );
	
	InvalidateRegion(0,0,SCREEN_WIDTH, SCREEN_HEIGHT);
	// invalidate region to force update
}

//--

void AddEmailWithSpecialData(INT32 iMessageOffset, INT32 iMessageLength, UINT8 ubSender, INT32 iDate, INT32 iFirstData, UINT32 uiSecondData, UINT8 EmailType, UINT32 EmailAIM, UINT16 EnumEmailXML)
{
	if (EnumEmailXML != static_cast<UINT16>(XML_NOEMAIL) && !gEmails.empty())
	{
		AddEmailWithSpecialDataXML(EnumEmailXML, iDate, -1, -1, false, iFirstData, uiSecondData, iMessageOffset, -1, -1, -1);
	}
	else
	{
		CHAR16 pSubject[MAIL_STRING_SIZE]{};
		Email FakeEmail{};
		// starts at iSubjectOffset amd goes iSubjectLength, reading in string
		LoadEmailRecord(EmailType, iMessageOffset, pSubject);

		//Make a fake email that will contain the codes ( ie the merc ID )
		FakeEmail.iFirstData = iFirstData;
		FakeEmail.uiSecondData = uiSecondData;

		//Replace the $mercname$ with the actual mercname
		ReplaceMercNameAndAmountWithProperData(pSubject, &FakeEmail);

		// add message to list
		AddEmailMessage(iMessageOffset, iMessageLength, pSubject, iDate, ubSender, FALSE, iFirstData, uiSecondData, 0, 0, 0, 0, -1, -1, EmailType, EmailAIM);

		// if we are in fact in the laptop, redraw icons, might be change in mail status
		if ( fCurrentlyInLaptop == TRUE )
		{
			// redraw icons, might be new mail
			DrawLapTopIcons();
		}
	}
	return;
}

//--- XML Read Mail ---
void AddEmailWithSpecialDataXML(INT32 iMessageOffset, INT32 iDate, INT32 iCurrentIMPPosition, INT16 iCurrentShipmentDestinationID, BOOLEAN alreadyRead, INT32 iFirstData, UINT32 uiSecondData, INT32 iThirdData, INT32 iFourthData, UINT32 uiFifthData, UINT32 uiSixData)
{
	CHAR16 pSubject[MAIL_STRING_SIZE]{};
	Email FakeEmail{};
	
	if (LaptopEmailListModel::IsIndexInRange(
			iMessageOffset, gEmails.size()))
	{
		auto& email = gEmails[iMessageOffset];
		if (email.Messages.size() >
			std::numeric_limits<UINT16>::max()) return;
		CopyEmailText(pSubject, email.Subject);
	
		//Make a fake email that will contain the codes ( ie the merc ID )
		FakeEmail.iFirstData = iFirstData;
		FakeEmail.uiSecondData = uiSecondData;

		//Replace the $mercname$ with the actual mercname
		ReplaceMercNameAndAmountWithProperData( pSubject, &FakeEmail );

		// add message to list
		AddEmailMessage(iMessageOffset,
			static_cast<INT32>(email.Messages.size()), pSubject, iDate,
			email.Sender, alreadyRead, iFirstData, uiSecondData, iThirdData,
			iFourthData, uiFifthData, uiSixData, iCurrentIMPPosition,
			iCurrentShipmentDestinationID, TYPE_EMAIL_XML, TYPE_E_NONE);

		// if we are in fact in the laptop, redraw icons, might be change in mail status
		if( fCurrentlyInLaptop == TRUE )
		{
			// redraw icons, might be new mail
			DrawLapTopIcons();
		}
	}
	else
	{
		ScreenMsg(FONT_LTRED, MSG_INTERFACE, L"%s%d %s", L"Tried to add email #", iMessageOffset, L"but could not find it in Emails.xml! If playing unmodded 1.13, please report this at https://github.com/1dot13/source/issues");
	}

	return;
}

void AddPreReadEmailTypeXML( INT32 iMessageOffset, INT32 iMessageLength, UINT8 ubSender, INT32 iDate, UINT8 EmailType )
{
	CHAR16 pSubject[320]{};
	CopyEmailText(pSubject, L"None");
	if (!LaptopEmailListModel::IsIndexInRange(ubSender, NUM_PROFILES) ||
		!LaptopEmailListModel::IsIndexInRange(iMessageLength, NUM_PROFILES))
		return;

	if ( EmailType == TYPE_EMAIL_AIM_AVAILABLE )
	{
		if (EmailMercAvailableText[ubSender].szSubject[0] != L'\0')
			CopyEmailText(pSubject,
				EmailMercAvailableText[ubSender].szSubject);
	}
	else if ( EmailType == TYPE_EMAIL_MERC_LEVEL_UP )
	{
		if (EmailMercLevelUpText[ubSender].szSubject[0] != L'\0')
			CopyEmailText(pSubject, EmailMercLevelUpText[ubSender].szSubject);
	}

	// add message to list
	AddEmailMessage( iMessageOffset,iMessageLength, pSubject, iDate, ubSender, TRUE, 0, 0, 0, 0, 0, 0, -1, -1 , EmailType, TYPE_E_NONE );

	// if we are in fact int he laptop, redraw icons, might be change in mail status

	if( fCurrentlyInLaptop )
	{
		// redraw icons, might be new mail
		DrawLapTopIcons();
	}
}

void AddEmailTypeXML( INT32 iMessageOffset, INT32 iMessageLength, UINT8 ubSender, INT32 iDate, INT32 iCurrentIMPPosition, UINT8 EmailType )
{
	CHAR16 pSubject[320]{};
	CopyEmailText(pSubject, L"None");
	if (!LaptopEmailListModel::IsIndexInRange(ubSender, NUM_PROFILES) ||
		!LaptopEmailListModel::IsIndexInRange(iMessageLength, NUM_PROFILES))
		return;

	if ( EmailType == TYPE_EMAIL_AIM_AVAILABLE )
	{
		if (EmailMercAvailableText[ubSender].szSubject[0] != L'\0')
			CopyEmailText(pSubject,
				EmailMercAvailableText[ubSender].szSubject);
	}
	else if ( EmailType == TYPE_EMAIL_MERC_LEVEL_UP )
	{
		if (EmailMercLevelUpText[ubSender].szSubject[0] != L'\0')
			CopyEmailText(pSubject, EmailMercLevelUpText[ubSender].szSubject);
	}

	AddEmailMessage(iMessageOffset,iMessageLength, pSubject, iDate, ubSender, FALSE, 0, 0, 0, 0, 0, 0, iCurrentIMPPosition, -1, EmailType, TYPE_E_NONE);

	// if we are in fact in the laptop, redraw icons, might be change in mail status
	if( fCurrentlyInLaptop )
	{
		// redraw icons, might be new mail
		DrawLapTopIcons();
	}
}

void AddEmailFromXML(INT32 iMessageOffset, INT32 iDate, INT32 iCurrentIMPPosition, INT16 iCurrentShipmentDestinationID, BOOLEAN alreadyRead, INT32 iFirstData, UINT32 uiSecondData, INT32 iThirdData, INT32 iFourthData, UINT32 uiFifthData, UINT32 uiSixData)
{
	if (LaptopEmailListModel::IsIndexInRange(
			iMessageOffset, gEmails.size()))
	{
		auto& email = gEmails[iMessageOffset];
		if (email.Messages.size() >
			std::numeric_limits<UINT16>::max()) return;
		AddEmailMessage(iMessageOffset,
			static_cast<INT32>(email.Messages.size()), email.Subject, iDate,
			email.Sender, alreadyRead, iFirstData, uiSecondData, iThirdData,
			iFourthData, uiFifthData, uiSixData, iCurrentIMPPosition,
			iCurrentShipmentDestinationID, TYPE_EMAIL_XML, TYPE_E_NONE);

		// if we are in fact in the laptop, redraw icons, might be change in mail status
		if ( fCurrentlyInLaptop == TRUE )
		{
			// redraw icons, might be new mail
			DrawLapTopIcons();
		}
	}
	else
	{
		ScreenMsg(FONT_LTRED, MSG_INTERFACE, L"%s%d %s", L"Tried to add email #", iMessageOffset, L"but could not find it in Emails.xml! If playing unmodded 1.13, please report this at https://github.com/1dot13/source/issues");
	}
}


void AddBobbyREmailJA2(INT32 iMessageOffset, INT32 iMessageLength, UINT8 ubSender, INT32 iDate, INT32 iCurrentIMPPosition, INT16 iCurrentShipmentDestinationID, UINT8 EmailType )
{
	CHAR16 pSubject[MAIL_STRING_SIZE]{};
	
	LoadEncryptedDataFromFile(EMAIL_EDT_FILE_JA2, pSubject, MAIL_STRING_SIZE*(iMessageOffset), MAIL_STRING_SIZE);
	
	// add message to list
    AddEmailMessage(iMessageOffset, iMessageLength, pSubject, iDate, ubSender, FALSE, 0, 0, -1, -1, -1, -1, iCurrentIMPPosition, iCurrentShipmentDestinationID, EmailType, TYPE_EMAIL_BOBBY_R_L1);

	// if we are in fact int he laptop, redraw icons, might be change in mail status

	if( fCurrentlyInLaptop == TRUE )
	{
		// redraw icons, might be new mail
		DrawLapTopIcons();
	}

	return;
}

//--- End XML Read Mail ---

void AddEmailWFMercAvailable(INT32 iMessageOffset, INT32 iMessageLength, UINT8 ubSender, INT32 iDate, INT32 iCurrentIMPPosition, UINT8 EmailType)
{
	CHAR16 pSubject[320]{};
	const auto subjectLine =
		LaptopEmailListModel::WildfireSubjectLine(iMessageLength);
	if (subjectLine == LaptopEmailListModel::NoIndex ||
		!LaptopEmailListModel::IsIndexInRange(ubSender, NUM_PROFILES)) return;
	CopyEmailText(pSubject, New113AIMMercMailTexts[subjectLine]);
	
	AddEmailMessage(iMessageOffset,iMessageLength, pSubject, iDate, ubSender, FALSE, 0, 0, 0, 0, 0, 0, iCurrentIMPPosition, -1 , EmailType, TYPE_E_NONE);

	// if we are in fact int he laptop, redraw icons, might be change in mail status

	if( fCurrentlyInLaptop == TRUE )
	{
		// redraw icons, might be new mail
		DrawLapTopIcons();
	}

	return;
}

void AddEmail(INT32 iMessageOffset, INT32 iMessageLength, UINT8 ubSender, INT32 iDate, INT32 iCurrentIMPPosition, INT16 iCurrentShipmentDestinationID, UINT8 EmailType, UINT16 EnumEmailXML)
{
	    CHAR16 pSubject[MAIL_STRING_SIZE]{};

	if ( EnumEmailXML != static_cast<UINT16>(XML_NOEMAIL) && gEmails.size() > 0 )
	{
		AddEmailFromXML(EnumEmailXML, iDate, iCurrentIMPPosition, iCurrentShipmentDestinationID, false, -1, -1, iMessageOffset, -1, -1, -1);
	}
	else
	{
	        LoadEmailRecord(EmailType, iMessageOffset, pSubject);
	        // add message to list
	        AddEmailMessage(iMessageOffset, iMessageLength, pSubject, iDate, ubSender, FALSE, 0, 0, 0, 0, 0, 0, iCurrentIMPPosition, iCurrentShipmentDestinationID, EmailType, TYPE_E_NONE);

        // if we are in fact in the laptop, redraw icons, might be change in mail status
        if ( fCurrentlyInLaptop == TRUE )
        {
            // redraw icons, might be new mail
            DrawLapTopIcons();
        }
    }
    return;
}

void AddPreReadEmail(INT32 iMessageOffset, INT32 iMessageLength, UINT8 ubSender, INT32 iDate, UINT8 EmailType)
{
	    CHAR16 pSubject[MAIL_STRING_SIZE]{};
	    LoadEmailRecord(EmailType, iMessageOffset, pSubject);
	    // add message to list
	    AddEmailMessage(iMessageOffset, iMessageLength, pSubject, iDate, ubSender, TRUE, 0, 0, 0, 0, 0, 0, -1, -1, EmailType, TYPE_E_NONE);

    // if we are in fact in the laptop, redraw icons, might be change in mail status
    if ( fCurrentlyInLaptop == TRUE )
    {
        // redraw icons, might be new mail
        DrawLapTopIcons();
    }

    return;
}

void AddEmailMessage(INT32 iMessageOffset, INT32 iMessageLength,STR16 pSubject, INT32 iDate, UINT8 ubSender, BOOLEAN fAlreadyRead, INT32 iFirstData, UINT32 uiSecondData, INT32 iThirdData, INT32 iFourthData, UINT32 uiFifthData, UINT32 uiSixData, INT32 iCurrentIMPPosition, INT16 iCurrentShipmentDestinationID, UINT8 EmailType, UINT32 EmailAIM )
{
	std::size_t subjectLength = 0;
	if (!LaptopEmailListModel::BoundedLength(
			pSubject, MAIL_STRING_SIZE - 1, subjectLength) ||
		!LaptopEmailListModel::CanStoreUnsigned16(iMessageOffset) ||
		!LaptopEmailListModel::CanStoreUnsigned16(iMessageLength) ||
		(IsProfileBackedEmail(EmailType) &&
			!LaptopEmailListModel::IsIndexInRange(ubSender, NUM_PROFILES)))
	{
		return;
	}

	EmailPtr tail = nullptr;
	std::size_t messageCount = 0;
	INT32 greatestId = -1;
	for (EmailPtr email = pEmailList; email; email = email->Next)
	{
		tail = email;
		++messageCount;
		if (email->iId > greatestId) greatestId = email->iId;
	}
	if (!LaptopEmailListModel::CanAppendMessage(
			messageCount, greatestId, EMAIL_VAL)) return;

	UniqueEmailNode stagedEmail(
		static_cast<EmailPtr>(MemAlloc(sizeof(Email))));
	if (!stagedEmail) return;
	std::memset(stagedEmail.get(), 0, sizeof(Email));
	stagedEmail->pSubject = static_cast<CHAR16*>(
		MemAlloc((subjectLength + 1) * sizeof(CHAR16)));
	if (!stagedEmail->pSubject) return;
	std::copy_n(pSubject, subjectLength + 1, stagedEmail->pSubject);

	stagedEmail->EmailVersion = EmailType;
	stagedEmail->EmailType = EmailAIM;
	stagedEmail->usOffset = static_cast<UINT16>(iMessageOffset);
	stagedEmail->usLength = static_cast<UINT16>(iMessageLength);
	stagedEmail->iCurrentIMPPosition = iCurrentIMPPosition;
	stagedEmail->iCurrentShipmentDestinationID =
		iCurrentShipmentDestinationID;
	stagedEmail->iId = LaptopEmailListModel::NextMessageId(greatestId);
	stagedEmail->iDate = iDate;
	stagedEmail->ubSender = ubSender;
	stagedEmail->iFirstData = iFirstData;
	stagedEmail->uiSecondData = uiSecondData;
	stagedEmail->iThirdData = iThirdData;
	stagedEmail->iFourthData = iFourthData;
	stagedEmail->uiFifthData = uiFifthData;
	stagedEmail->uiSixData = uiSixData;
	stagedEmail->fRead = fAlreadyRead;
	stagedEmail->fNew = TRUE;

	EmailPageListOwner stagedPages;
	if (!BuildEmailPages(
			pEmailList, nullptr, stagedEmail.get(), stagedPages)) return;

	EmailPtr publishedEmail = stagedEmail.release();
	publishedEmail->Prev = tail;
	publishedEmail->Next = nullptr;
	if (tail) tail->Next = publishedEmail;
	else pEmailList = publishedEmail;
	CommitEmailPages(stagedPages, messageCount + 1);

	gEmailT[static_cast<UINT32>(publishedEmail->iId)].EmailVersion = EmailType;
	gEmailT[static_cast<UINT32>(publishedEmail->iId)].EmailType = EmailAIM;
	fNewMailFlag = TRUE;
}


BOOLEAN RemoveEmailMessage(INT32 iId)
{
	EmailPtr email = GetEmailMessage(iId);
	if (!email) return FALSE;

	const std::size_t oldCount = EmailMessageCount(pEmailList);
	EmailPageListOwner stagedPages;
	if (!BuildEmailPages(pEmailList, email, nullptr, stagedPages)) return FALSE;

	if (email->Prev) email->Prev->Next = email->Next;
	else pEmailList = email->Next;
	if (email->Next) email->Next->Prev = email->Prev;
	CommitEmailPages(stagedPages, oldCount - 1);

	if (LaptopEmailListModel::IsIndexInRange(iId, EMAIL_VAL))
		gEmailT[static_cast<UINT32>(iId)] = {};
	email->Next = nullptr;
	email->Prev = nullptr;
	EmailNodeDeleter{}(email);
	return TRUE;
}

EmailPtr GetEmailMessage(INT32 iId)
{
	EmailPtr pEmail=pEmailList;
	// return pointer to message with iId

	// invalid id
	if(iId==-1)
		return NULL;

	// invalid list
	if( pEmail == NULL )
	{
		return NULL;
	}

	// look for message
	while( (pEmail->iId !=iId)&&(pEmail->Next) )
		pEmail=pEmail->Next;

	if( ( pEmail->iId != iId ) && ( pEmail->Next == NULL ) )
	{
		pEmail = NULL;
	}

	// no message, or is there?
	if(!pEmail)
		return NULL;
	else
		return pEmail;
}


static void SortMessages(INT32 iCriteria)
{
    EmailPtr pA = pEmailList;
    EmailPtr pB = nullptr;

    // no messages to sort?
    if ( pA == NULL )
    {
        return;
    }

    // nothing here either?
    if ( !pA->Next )
        return;

    switch ( iCriteria )
    {
        case RECEIVED:
            while ( pA )
            {
                // set B to next in A
                pB = pA->Next;
                while ( pB )
                {

                    if ( fSortDateUpwards )
                    {
                        // if date is lesser, swap
                        if ( pA->iDate > pB->iDate )
                            SwapMessages(pA->iId, pB->iId);
                    }
                    else
                    {
                        // if date is lesser, swap
                        if ( pA->iDate < pB->iDate )
                            SwapMessages(pA->iId, pB->iId);
                    }


                    // next in B's list
                    pB = pB->Next;
                }

                // next in A's List
                pA = pA->Next;
            }
            break;
        case SENDER:
            while ( pA )
            {
                pB = pA->Next;
                while ( pB )
                {
                    // lesser string?...need sorting
                    if ( fSortSenderUpwards )
                    {
                        if ( (wcscmp(pSenderNameList[pA->ubSender], pSenderNameList[pB->ubSender])) < 0 )
                            SwapMessages(pA->iId, pB->iId);
                    }
                    else
                    {
                        if ( (wcscmp(pSenderNameList[pA->ubSender], pSenderNameList[pB->ubSender])) > 0 )
                            SwapMessages(pA->iId, pB->iId);
                    }
                    // next in B's list
                    pB = pB->Next;
                }
                // next in A's List
                pA = pA->Next;
            }
            break;
        case SUBJECT:
            while ( pA )
            {
                pB = pA->Next;
                while ( pB )
                {
                    // lesser string?...need sorting
                    if ( fSortSubjectUpwards )
                    {
                        if ( (wcscmp(pA->pSubject, pB->pSubject)) < 0 )
                            SwapMessages(pA->iId, pB->iId);
                    }
                    else
                    {
                        if ( (wcscmp(pA->pSubject, pB->pSubject)) > 0 )
                            SwapMessages(pA->iId, pB->iId);
                    }
                    // next in B's list
                    pB = pB->Next;
                }
                // next in A's List
                pA = pA->Next;
            }
            break;

        case READ:
            while ( pA )
            {
                pB = pA->Next;
                while ( pB )
                {
                    // one read and another not?...need sorting
                    if ( (pA->fRead) && (!(pB->fRead)) )
                        SwapMessages(pA->iId, pB->iId);

                    // next in B's list
                    pB = pB->Next;
                }
                // next in A's List
                pA = pA->Next;
            }
            break;
    }

    // place new list into pages of email
    //PlaceMessagesinPages();

    // redraw the screen
    fReDrawScreenFlag = TRUE;
}

void SwapMessages(INT32 iIdA, INT32 iIdB)
{
    // Swap the two messages' payload between their list nodes. The list pointers
    // (Next/Prev) stay put; only the message fields move.
    EmailPtr pA = pEmailList;
    EmailPtr pB = pEmailList;

    if ( !pEmailList || !pEmailList->Next )
        return;

    while ( pA && pA->iId != iIdA )
        pA = pA->Next;
    while ( pB && pB->iId != iIdB )
        pB = pB->Next;
    if ( !pA || !pB )   // id not found -> was a NULL walk-off
        return;

    // Swap the same payload fields the original did, plus the pSubject POINTER.
    // Swapping the pointer -- instead of wcscpy'ing subject contents between the
    // two exact-size heap buffers -- removes the heap overflow that happened
    // whenever the subjects differed in length, and drops the leaky 128-char
    // scratch buffer (also leaked on the early return above). EmailVersion and
    // ubSender move with pSubject, so AIM/level-up subjects stay consistent with
    // their type without the canonical-subject rewrites.
    std::swap( pA->iId,          pB->iId );
    std::swap( pA->fRead,        pB->fRead );
    std::swap( pA->fNew,         pB->fNew );
    std::swap( pA->usOffset,     pB->usOffset );
    std::swap( pA->EmailVersion, pB->EmailVersion );
    std::swap( pA->usLength,     pB->usLength );
    std::swap( pA->iDate,        pB->iDate );
    std::swap( pA->ubSender,     pB->ubSender );
    std::swap( pA->pSubject,     pB->pSubject );
	std::swap( pA->EmailType,     pB->EmailType );
	std::swap( pA->iFirstData,    pB->iFirstData );
	std::swap( pA->uiSecondData,  pB->uiSecondData );
	std::swap( pA->iThirdData,    pB->iThirdData );
	std::swap( pA->iFourthData,   pB->iFourthData );
	std::swap( pA->uiFifthData,   pB->uiFifthData );
	std::swap( pA->uiSixData,     pB->uiSixData );
	std::swap( pA->iCurrentIMPPosition, pB->iCurrentIMPPosition );
	std::swap( pA->iCurrentShipmentDestinationID,
		pB->iCurrentShipmentDestinationID );
}

static void ClearPages()
{
	FreeEmailPages(pPageList);
	pPageList=NULL;
	iLastPage=-1;
}

void PlaceMessagesinPages()
{
	EmailPageListOwner stagedPages;
	if (!BuildEmailPages(pEmailList, nullptr, nullptr, stagedPages)) return;
	CommitEmailPages(stagedPages, EmailMessageCount(pEmailList));
}

static void DrawLetterIcon(INT32 iCounter, BOOLEAN fRead)
{
	HVOBJECT hHandle;
	// will draw the icon for letter in mail list depending if the mail has been read or not

	// grab video object
	GetVideoObject(&hHandle, guiEmailIndicator);

	// is it read or not?
	if(fRead)
		BltVideoObject(FRAME_BUFFER, hHandle, 0,INDIC_X, (MIDDLE_Y+iCounter*MIDDLE_WIDTH+2), VO_BLT_SRCTRANSPARENCY,NULL);
	else
		BltVideoObject(FRAME_BUFFER, hHandle, 1,INDIC_X, (MIDDLE_Y+iCounter*MIDDLE_WIDTH+2), VO_BLT_SRCTRANSPARENCY,NULL);
}

static void DrawSubject(INT32 iCounter, STR16 pSubject, BOOLEAN fRead)
{
	CHAR16 pTempSubject[320];
	
	// draw subject line of mail being viewed in viewer

	// lock buffer to prevent overwrite
	SetFontDestBuffer(FRAME_BUFFER, SUBJECT_X , ((UINT16)(MIDDLE_Y+iCounter*MIDDLE_WIDTH)) , SUBJECT_X	+ SUBJECT_WIDTH , ( ( UINT16 ) ( MIDDLE_Y + iCounter * MIDDLE_WIDTH ) ) + MIDDLE_WIDTH,	FALSE	);
	SetFontShadow(NO_SHADOW);
	SetFontForeground( FONT_BLACK );
	SetFontBackground( FONT_BLACK );
	
	CopyEmailText(pTempSubject, pSubject);

	if( fRead )
	{
		//if the subject will be too long, cap it, and add the '...'
		if( StringPixLength( pTempSubject, MESSAGE_FONT ) >= SUBJECT_WIDTH - 10 )
		{
			ReduceStringLength( pTempSubject, SUBJECT_WIDTH - 10, MESSAGE_FONT );
		}

	// display string subject
	IanDisplayWrappedString(SUBJECT_X, (( UINT16 )( 4 + MIDDLE_Y + iCounter * MIDDLE_WIDTH ) ) , SUBJECT_WIDTH, MESSAGE_GAP, MESSAGE_FONT, MESSAGE_COLOR ,pTempSubject,0 ,FALSE ,LEFT_JUSTIFIED );
	}
	else
	{
		//if the subject will be too long, cap it, and add the '...'
		if( StringPixLength( pTempSubject, FONT10ARIALBOLD ) >= SUBJECT_WIDTH - 10 )
		{
			ReduceStringLength( pTempSubject, SUBJECT_WIDTH - 10, FONT10ARIALBOLD );
		}

		// display string subject
	IanDisplayWrappedString(SUBJECT_X, (( UINT16 )( 4 + MIDDLE_Y + iCounter * MIDDLE_WIDTH ) ) , SUBJECT_WIDTH, MESSAGE_GAP, FONT10ARIALBOLD, MESSAGE_COLOR ,pTempSubject,0 ,FALSE ,LEFT_JUSTIFIED );

	}
	SetFontShadow(DEFAULT_SHADOW);
	// reset font dest buffer
	SetFontDestBuffer(FRAME_BUFFER, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, FALSE	);
}

static void DrawSender(INT32 iCounter, UINT8 ubSender, BOOLEAN fRead, UINT8 EmailType)
{
	// draw name of sender in mail viewer
	SetFontShadow(NO_SHADOW);

	SetFontShadow(NO_SHADOW);
	SetFontForeground( FONT_BLACK );
	SetFontBackground( FONT_BLACK );

	if( fRead )
	{
		SetFont( MESSAGE_FONT );
	}
	else
	{
		SetFont( FONT10ARIALBOLD );
	}

	if ( EmailType == TYPE_EMAIL_AIM_AVAILABLE || EmailType == TYPE_EMAIL_MERC_LEVEL_UP )
	{
		if (LaptopEmailListModel::IsIndexInRange(ubSender, NUM_PROFILES) &&
			gMercProfiles[ubSender].zNickname[0] != L'\0')
		mprintf(SENDER_X,(( UINT16 )( 4 + MIDDLE_Y + iCounter * MIDDLE_WIDTH ) ) ,L"%s", gMercProfiles[ ubSender ].zNickname);
		else
		mprintf(SENDER_X,(( UINT16 )( 4 + MIDDLE_Y + iCounter * MIDDLE_WIDTH ) ) ,L"%s", L"None");
	}
	else if ( EmailType == TYPE_EMAIL_EMAIL_EDT || EmailType == TYPE_EMAIL_BOBBY_R || EmailType == TYPE_EMAIL_BOBBY_R_EMAIL_JA2_EDT ||  EmailType == TYPE_EMAIL_INSURANCE_COMPANY_EMAIL_JA2_EDT || EmailType == TYPE_EMAIL_DEAD_MERC_AIM_SITE_EMAIL_JA2_EDT || EmailType == TYPE_EMAIL_XML )
	{
		mprintf(SENDER_X,(( UINT16 )( 4 + MIDDLE_Y + iCounter * MIDDLE_WIDTH ) ) ,L"%s", pSenderNameList[ubSender]);
	}
	else if ( EmailType == TYPE_EMAIL_EMAIL_EDT_NAME_MERC )
	{
		if (LaptopEmailListModel::IsIndexInRange(ubSender, NUM_PROFILES) &&
			gMercProfiles[ubSender].zNickname[0] != L'\0')
		mprintf(SENDER_X,(( UINT16 )( 4 + MIDDLE_Y + iCounter * MIDDLE_WIDTH ) ) ,L"%s", gMercProfiles[ ubSender ].zNickname);
		else
		mprintf(SENDER_X,(( UINT16 )( 4 + MIDDLE_Y + iCounter * MIDDLE_WIDTH ) ) ,L"%s", L"None");
	}

	SetFont( MESSAGE_FONT );
	SetFontShadow(DEFAULT_SHADOW);
}

static void DrawDate(INT32 iCounter, INT32 iDate, BOOLEAN fRead)
{
	CHAR16 sString[20];

	SetFontShadow(NO_SHADOW);
	SetFontForeground( FONT_BLACK );
	SetFontBackground( FONT_BLACK );

	if( fRead )
	{
		SetFont( MESSAGE_FONT );
	}
	else
	{
		SetFont( FONT10ARIALBOLD );
	}
	// draw date of message being displayed in mail viewer
	sgp_swprintf(sString, 20, L"%s %d", pDayStrings[0],
		iDate / (24 * 60));
	mprintf(DATE_X,(( UINT16 )( 4 + MIDDLE_Y + iCounter * MIDDLE_WIDTH ) ),L"%s", sString);

	SetFont( MESSAGE_FONT );
	SetFontShadow(DEFAULT_SHADOW);
}

void DisplayEmailList()
{
	INT32 iCounter=0;
	PagePtr pPage = nullptr;
	EmailPtr pEmail=NULL;

	iCurrentPage = static_cast<INT32>(
		LaptopEmailListModel::NormalizeInboxPage(
			static_cast<std::size_t>(std::max(iCurrentPage, 0)),
			EmailMessageCount(pEmailList), MAX_MESSAGES_PAGE));
	pPage = FindEmailPage(iCurrentPage);
	if (!pPage) return;

	// now we have current page, display it
	pEmail=GetEmailMessage(pPage->iIds[iCounter]);
	SetFontShadow(NO_SHADOW);
	SetFont(EMAIL_TEXT_FONT);
	
	// draw each line of the list for this page
	while(pEmail)
	{
		// highlighted message, set text of message in list to blue
		if(iCounter==iHighLightLine)
		{
			SetFontForeground(FONT_BLUE);
		}
		else if(pEmail->fRead)
		{
			// message has been read, reset color to black
			SetFontForeground(FONT_BLACK);
	 //SetFontBackground(FONT_BLACK);

		}
		else
		{
			// defualt, message is not read, set font red
			SetFontForeground(FONT_RED);
	 //SetFontBackground(FONT_BLACK);
		}
		SetFontBackground(FONT_BLACK);

		//draw the icon, sender, date, subject
		DrawLetterIcon(iCounter,pEmail->fRead );
		DrawSubject(iCounter, pEmail->pSubject, pEmail->fRead );
		
		if ( pEmail->EmailVersion == TYPE_EMAIL_AIM_AVAILABLE || pEmail->EmailVersion == TYPE_EMAIL_MERC_LEVEL_UP )
			DrawSender(iCounter, pEmail->ubSender, pEmail->fRead, pEmail->EmailVersion);
		else if ( pEmail->EmailVersion == TYPE_EMAIL_EMAIL_EDT || pEmail->EmailVersion == TYPE_EMAIL_EMAIL_EDT_NAME_MERC || pEmail->EmailVersion == TYPE_EMAIL_BOBBY_R || pEmail->EmailVersion == TYPE_EMAIL_BOBBY_R_EMAIL_JA2_EDT || pEmail->EmailVersion == TYPE_EMAIL_INSURANCE_COMPANY_EMAIL_JA2_EDT || pEmail->EmailVersion == TYPE_EMAIL_DEAD_MERC_AIM_SITE_EMAIL_JA2_EDT )
			DrawSender(iCounter, pEmail->ubSender, pEmail->fRead, pEmail->EmailVersion);
		else if ( pEmail->EmailVersion == TYPE_EMAIL_XML )
			DrawSender(iCounter, pEmail->ubSender, pEmail->fRead, pEmail->EmailVersion);

		DrawDate(iCounter, pEmail->iDate, pEmail->fRead );

		++iCounter;

		// too many messages onthis page, reset pEmail, so no more are drawn
		if(iCounter >= MAX_MESSAGES_PAGE)
			pEmail=NULL;
		else
			pEmail=GetEmailMessage(pPage->iIds[iCounter]);
	}
	
	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_LR_Y);

	SetFontShadow(DEFAULT_SHADOW);
}

void LookForUnread()
{
	BOOLEAN fStatusOfNewEmailFlag = fUnReadMailFlag;

	// simply runrs through list of messages, if any unread, set unread flag

	EmailPtr pA=pEmailList;

	// reset unread flag
	fUnReadMailFlag=FALSE;

	// look for unread mail
	while(pA)
	{
		// unread mail found, set flag
		if(!(pA->fRead))
	 fUnReadMailFlag=TRUE;
		pA=pA->Next;
	}

	if( fStatusOfNewEmailFlag != fUnReadMailFlag )
	{
		//Since there is no new email, get rid of the hepl text
		CreateFileAndNewEmailIconFastHelpText( LAPTOP_BN_HLP_TXT_YOU_HAVE_NEW_MAIL, (BOOLEAN )!fUnReadMailFlag );
	}

	return;
}

void EmailBtnCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
 INT32 iCount;
 PagePtr pPage = FindEmailPage(iCurrentPage);
 INT32 iId=0;
 if (iReason & MSYS_CALLBACK_REASON_INIT)
 {
	return;
 }
 if(fDisplayMessageFlag)
	return;
 if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
 {

	// error check
	iCount=MSYS_GetRegionUserData(pRegion, 0);
	if (!LaptopEmailListModel::IsIndexInRange(
			iCount, MAX_MESSAGES_PAGE)) return;
	// check for valid email
	// find surrent page
	if(!pPage)
		return;
	// get id for element iCount
	iId=pPage->iIds[iCount];

	// invalid message
	if(iId==-1)
	{
		fDisplayMessageFlag=FALSE;
		return;
	}
	// Get email and display
	fDisplayMessageFlag=TRUE;
	giMessagePage = 0;
	giPrevMessageId = giMessageId;
	giMessageId=iId;


 }
 else if(iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
 {
	iCount=MSYS_GetRegionUserData(pRegion, 0);
	if (!LaptopEmailListModel::IsIndexInRange(
			iCount, MAX_MESSAGES_PAGE))
	{
		HandleRightButtonUpEvent();
		return;
	}

	// error check
	if(!pPage)
	{
		HandleRightButtonUpEvent( );
		return;
	}

 	giMessagePage = 0;

	// get id for element iCount
	iId=pPage->iIds[iCount];
	if(!GetEmailMessage(iId))
	{
		// no mail here, handle right button up event
		HandleRightButtonUpEvent( );
		return;
	}
	else
	{
		fDeleteMailFlag=TRUE;
		iDeleteId=iId;
		//DisplayDeleteNotice(GetEmailMessage(iDeleteId));
		//DeleteEmail();
	}
 }
}
void EmailMvtCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
		return;
	}
	if(fDisplayMessageFlag)
		return;
	if (iReason == MSYS_CALLBACK_REASON_MOVE)
	{

		// set highlight to current regions data, this is the message to display
	iHighLightLine=MSYS_GetRegionUserData(pRegion, 0);
	}
	if (iReason == MSYS_CALLBACK_REASON_LOST_MOUSE )
	{

		// reset highlight line to invalid message
	iHighLightLine=-1;
	}
}

static void BtnMessageXCallback(GUI_BUTTON *btn,INT32 reason)
{
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if((reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )||(reason & MSYS_CALLBACK_REASON_RBUTTON_DWN))
	{
		btn->uiFlags |= BUTTON_CLICKED_ON;
	}
	else if((reason & MSYS_CALLBACK_REASON_LBUTTON_UP )||(reason & MSYS_CALLBACK_REASON_RBUTTON_UP))
	{
		if(btn->uiFlags& BUTTON_CLICKED_ON)
		{
			// X button has been pressed and let up, this means to stop displaying the currently displayed message

			// reset display message flag
			fDisplayMessageFlag=FALSE;

			// reset button flag
			btn->uiFlags &= ~BUTTON_CLICKED_ON;

			// reset page being displayed
			giMessagePage = 0;

			// redraw icons
			DrawLapTopIcons();

			// force update of entire screen
			fPausedReDrawScreenFlag=TRUE;

			// rerender email
			//RenderEmail();
		}
	}
}
void SetUnNewMessages()
{
	// on exit from the mailer, set all new messages as 'un'new
	EmailPtr pEmail=pEmailList;
	
	// run through the list of messages and add to pages
	while(pEmail)
	{
		pEmail->fNew=FALSE;
		pEmail=pEmail->Next;
	}
	return;
}

INT32 DisplayEmailMessage(EmailPtr pMail)
{
	HVOBJECT hHandle;
	INT32 iCounter;
//	CHAR16 pString[MAIL_STRING_SIZE/2 + 1];
	CHAR16 pString[MAIL_STRING_SIZE]{};
	INT32 iOffSet=0;
	RecordPtr pTempRecord;
	BOOLEAN fDonePrintingMessage = FALSE;
	
	if(!pMail)
		return 0;

	iOffSet=(INT32)pMail->usOffset;

	// reset redraw email message flag
	fReDrawMessageFlag = FALSE;

	// we KNOW the player is going to "read" this, so mark it as so
	pMail->fRead=TRUE;

	giCurrentIMPSlot = pMail->iCurrentIMPPosition;

	// draw text for title bar
	//DisplayWrappedString(VIEWER_X+VIEWER_HEAD_X+4, VIEWER_Y+VIEWER_HEAD_Y+4, VIEWER_HEAD_WIDTH, MESSAGE_GAP, MESSAGE_FONT, MESSAGE_COLOR, pString, 0,FALSE,0);

	// is there any special event meant for this mail?..if so, handle it
	if ( pMail->EmailVersion == TYPE_EMAIL_EMAIL_EDT )
		HandleAnySpecialEmailMessageEvents( iOffSet );
	else if ( pMail->EmailVersion == TYPE_EMAIL_XML )
		HandleAnySpecialEmailMessageEvents(pMail->iThirdData);

	const UINT16 specialMessageId = pMail->EmailVersion == TYPE_EMAIL_XML
		? static_cast<UINT16>(pMail->iThirdData)
		: static_cast<UINT16>(iOffSet);
	HandleMailSpecialMessages(
		specialMessageId, &iViewerPositionY, pMail);
	
	PreProcessEmail( pMail );
	
	// blt in top line of message as a blank graphic
	// get a handle to the bitmap of EMAIL VIEWER Background
	GetVideoObject( &hHandle, guiEmailMessage );

	// place the graphic on the frame buffer
	BltVideoObject( FRAME_BUFFER, hHandle, 1,VIEWER_X, VIEWER_MESSAGE_BODY_START_Y + iViewerPositionY, VO_BLT_SRCTRANSPARENCY,NULL );
	BltVideoObject( FRAME_BUFFER, hHandle, 1,VIEWER_X, VIEWER_MESSAGE_BODY_START_Y + GetFontHeight( MESSAGE_FONT ) + iViewerPositionY, VO_BLT_SRCTRANSPARENCY,NULL );

	// set shadow
	SetFontShadow(NO_SHADOW);

	// get a handle to the bitmap of EMAIL VIEWER
	GetVideoObject(&hHandle, guiEmailMessage);

	// place the graphic on the frame buffer
	BltVideoObject(FRAME_BUFFER, hHandle, 0,VIEWER_X, VIEWER_Y + iViewerPositionY, VO_BLT_SRCTRANSPARENCY,NULL);
	
	// the icon for the title of this box
	GetVideoObject( &hHandle, guiTITLEBARICONS );
	BltVideoObject( FRAME_BUFFER, hHandle, 0,VIEWER_X + 5, VIEWER_Y + iViewerPositionY + 2, VO_BLT_SRCTRANSPARENCY,NULL );

	// display header text
	DisplayEmailMessageSubjectDateFromLines( pMail, iViewerPositionY );

	// display title text
	DrawEmailMessageDisplayTitleText( iViewerPositionY );
	
	// now blit the text background based on height
	for (iCounter=2; iCounter < ( ( iTotalHeight ) / ( GetFontHeight( MESSAGE_FONT ) ) ); iCounter++ )
	{
	// get a handle to the bitmap of EMAIL VIEWER Background
	GetVideoObject( &hHandle, guiEmailMessage );

	// place the graphic on the frame buffer
	BltVideoObject( FRAME_BUFFER, hHandle, 1,VIEWER_X, iViewerPositionY + VIEWER_MESSAGE_BODY_START_Y+( (GetFontHeight( MESSAGE_FONT ) ) * ( iCounter )), VO_BLT_SRCTRANSPARENCY,NULL );
	}

	// now the bottom piece to the message viewer
	GetVideoObject( &hHandle, guiEmailMessage );

	if( giNumberOfPagesToCurrentEmail <= 2 )
	{
		// place the graphic on the frame buffer
		BltVideoObject( FRAME_BUFFER, hHandle, 2,VIEWER_X, iViewerPositionY + VIEWER_MESSAGE_BODY_START_Y+( ( GetFontHeight( MESSAGE_FONT ) ) * ( iCounter )), VO_BLT_SRCTRANSPARENCY,NULL );
	}
	else
	{
		// place the graphic on the frame buffer
		BltVideoObject( FRAME_BUFFER, hHandle, 3,VIEWER_X, iViewerPositionY + VIEWER_MESSAGE_BODY_START_Y+( ( GetFontHeight( MESSAGE_FONT ) ) * ( iCounter )), VO_BLT_SRCTRANSPARENCY,NULL );
	}

	INT32 iHeight = GetFontHeight(MESSAGE_FONT);

	// draw body of text. Any particular email can encompass more than one "record" in the
	// email file. Draw each record (length is number of records)

	// now place the text

	// reset shadow
	SetFontShadow( NO_SHADOW );

	giMessagePage = static_cast<INT32>(
		LaptopEmailListModel::NormalizeBodyPage(
			static_cast<std::size_t>(std::max(giMessagePage, 0)),
			static_cast<std::size_t>(
				std::max(giNumberOfPagesToCurrentEmail, 0)),
			MAX_NUMBER_EMAIL_PAGES));
	pTempRecord = pEmailPageInfo[ giMessagePage ].pFirstRecord;

	if( pTempRecord )
	{
		while( fDonePrintingMessage == FALSE )
		{
			// copy over string
			CopyEmailText(pString, pTempRecord->pRecord);

			// get the height of the string, ONLY!...must redisplay ON TOP OF background graphic
			iHeight += IanDisplayWrappedString(VIEWER_X + 9, ( UINT16 )( VIEWER_MESSAGE_BODY_START_Y + iHeight + iViewerPositionY), MESSAGE_WIDTH, MESSAGE_GAP, MESSAGE_FONT, MESSAGE_COLOR,pString,0,FALSE, IAN_WRAP_NO_SHADOW);

			// increment email record ptr
			pTempRecord = pTempRecord->Next;

			if( pTempRecord == NULL )
			{
				fDonePrintingMessage = TRUE;
			}
			else if ((pTempRecord ==
					pEmailPageInfo[giMessagePage].pLastRecord) &&
				LaptopEmailListModel::HasNextBodyPage(
					static_cast<std::size_t>(giMessagePage),
					static_cast<std::size_t>(std::max(
						giNumberOfPagesToCurrentEmail, 0)),
					MAX_NUMBER_EMAIL_PAGES))
			{
				fDonePrintingMessage = TRUE;
			}
		}
	}

	/*
	if(iTotalHeight < MAX_EMAIL_MESSAGE_PAGE_SIZE)
	{
		fOnLastPageFlag = TRUE;
	while( pTempRecord )
		{
		// copy over string
		CopyEmailText(pString, pTempRecord->pRecord);

	 // get the height of the string, ONLY!...must redisplay ON TOP OF background graphic
	 iHeight += IanDisplayWrappedString(VIEWER_X + MESSAGE_X + 4, ( UINT16 )( VIEWER_MESSAGE_BODY_START_Y + iHeight + iViewerPositionY), MESSAGE_WIDTH, MESSAGE_GAP, MESSAGE_FONT, MESSAGE_COLOR,pString,0,FALSE, IAN_WRAP_NO_SHADOW);

			// increment email record ptr
		pTempRecord = pTempRecord->Next;
		}


	}
	else
	{

		iYPositionOnPage = 0;
		// go to the right record
		pTempRecord = GetFirstRecordOnThisPage( pMessageRecordList, MESSAGE_FONT, MESSAGE_WIDTH, MESSAGE_GAP, giMessagePage, MAX_EMAIL_MESSAGE_PAGE_SIZE );
	while( pTempRecord )
		{
			// copy over string
		CopyEmailText(pString, pTempRecord->pRecord);

			if( pString[ 0 ] == 0 )
			{
				// on last page
				fOnLastPageFlag = TRUE;
			}


			if( ( iYPositionOnPage + IanWrappedStringHeight(0, 0, MESSAGE_WIDTH, MESSAGE_GAP,
															MESSAGE_FONT, 0, pTempRecord->pRecord,
															0, 0, 0 ) )	<= MAX_EMAIL_MESSAGE_PAGE_SIZE	)
			{
	 	// now print it
		 iYPositionOnPage += IanDisplayWrappedString(VIEWER_X + MESSAGE_X + 4, ( UINT16 )( VIEWER_MESSAGE_BODY_START_Y + 10 +iYPositionOnPage + iViewerPositionY), MESSAGE_WIDTH, MESSAGE_GAP, MESSAGE_FONT, MESSAGE_COLOR,pString,0,FALSE, IAN_WRAP_NO_SHADOW);
				fGoingOffCurrentPage = FALSE;
			}
			else
			{
				// gonna get cut off...end now
				fGoingOffCurrentPage = TRUE;
			}


			pTempRecord = pTempRecord->Next;


			if( ( pTempRecord == NULL ) && ( fGoingOffCurrentPage == FALSE ) )
			{
				// on last page
				fOnLastPageFlag = TRUE;
			}
			else
			{
				fOnLastPageFlag = FALSE;
			}

			// record get cut off?...end now

			if( fGoingOffCurrentPage == TRUE )
			{
				pTempRecord = NULL;
			}
		}

	}

	*/
	// show number of pages to this email
	DisplayNumberOfPagesToThisEmail( iViewerPositionY );

	// mark this area dirty
	InvalidateRegion( LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_LR_Y );
	
	// reset shadow
	SetFontShadow( DEFAULT_SHADOW );
	
	return iViewerPositionY;
}

static void BtnNewOkback(GUI_BUTTON *btn,INT32 reason)
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
			fNewMailFlag=FALSE;
		}
	}
}

void AddDeleteRegionsToMessageRegion(INT32 iViewerY)
{
	if (fDisplayMessageFlag && gEmailMessageResources.empty())
	{
		LaptopPageResourceOwner staged;

		// add X button
		if (!staged.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\X.sti", -1, 0, -1, 1, -1),
			giMessageButtonImage[0])) return;
		if (!staged.addButton(QuickCreateButton(giMessageButtonImage[0],
			BUTTON_X + 2, (INT16)(BUTTON_Y + (INT16)iViewerY + 1),
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
										(GUI_CALLBACK)BtnMessageXCallback),
			giMessageButton[0])) return;
		SetButtonCursor(giMessageButton[0], CURSOR_LAPTOP_SCREEN);

		if( giNumberOfPagesToCurrentEmail > 2 )
		{
			// add next and previous mail page buttons
			if (!staged.addButtonImage(LoadButtonImageOwned(
				"LAPTOP\\NewMailButtons.sti", -1, 0, -1, 3, -1),
				giMailMessageButtonsImage[0])) return;
			if (!staged.addButton(QuickCreateButton(
				giMailMessageButtonsImage[0], PREVIOUS_PAGE_BUTTON_X,
				(INT16)(LOWER_BUTTON_Y + (INT16)iViewerY + 2),
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
										(GUI_CALLBACK)BtnPreviousEmailPageCallback),
				giMailMessageButtons[0])) return;

			if (!staged.addButtonImage(LoadButtonImageOwned(
				"LAPTOP\\NewMailButtons.sti", -1, 1, -1, 4, -1),
				giMailMessageButtonsImage[1])) return;
			if (!staged.addButton(QuickCreateButton(
				giMailMessageButtonsImage[1], NEXT_PAGE_BUTTON_X,
				(INT16)(LOWER_BUTTON_Y + (INT16)iViewerY + 2),
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
										(GUI_CALLBACK)BtnNextEmailPageCallback),
				giMailMessageButtons[1])) return;

			SetButtonCursor(giMailMessageButtons[0], CURSOR_LAPTOP_SCREEN);
			SetButtonCursor(giMailMessageButtons[1], CURSOR_LAPTOP_SCREEN);
		}

		// add delete message button
		if (!staged.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\NewMailButtons.sti", -1, 2, -1, 5, -1),
			giMailMessageButtonsImage[2])) return;
		if (!staged.addButton(QuickCreateButton(giMailMessageButtonsImage[2],
			DELETE_BUTTON_X,
			(INT16)(BUTTON_LOWER_Y + (INT16)iViewerY + 2),
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
										(GUI_CALLBACK)BtnDeleteCallback),
			giMailMessageButtons[2])) return;

		// set cursors
		SetButtonCursor(giMailMessageButtons[2], CURSOR_LAPTOP_SCREEN);

		// set up email message region
		MSYS_DefineRegion( &pMailViewMessageRegion, VIEWER_X + 2, (INT16) ( VIEWER_Y + (INT16)iViewerY + 2), VIEWER_X + 416, (INT16) ( VIEWER_Y + (INT16)iViewerY + 72 + iTotalHeight), MSYS_PRIORITY_HIGH,
						CURSOR_LAPTOP_SCREEN, MSYS_NO_CALLBACK, ViewMessageRegionCallBack );
		if (!staged.addRegion(pMailViewMessageRegion)) return;
		gEmailMessageResources = std::move(staged);
		fOldDisplayMessageFlag = TRUE;

		// force update of screen
		fReDrawScreenFlag=TRUE;
	}
	else if (!fDisplayMessageFlag && !gEmailMessageResources.empty())
	{
		gEmailMessageResources.clear();
		fOldDisplayMessageFlag = FALSE;

		// force update of screen
		fReDrawScreenFlag=TRUE;
	}
}

void CreateDestroyNewMailButton()
{
	// check if we are video conferencing, if so, do nothing
	if( gubVideoConferencingMode != 0 )
	{
		gEmailNewMailResources.clear();
		return;
	}

	if (fNewMailFlag && gEmailNewMailResources.empty())
	{
		LaptopPageResourceOwner staged;

		// load image and setup button
		if (!staged.addButtonImage(LoadButtonImageOwned(
			"LAPTOP\\YesNoButtons.sti", -1, 0, -1, 1, -1),
			giNewMailButtonImage[0])) return;
		if (!staged.addButton(QuickCreateButton(giNewMailButtonImage[0],
			NEW_BTN_X + 10, NEW_BTN_Y,
											BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST-2,
											(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
											(GUI_CALLBACK)BtnNewOkback),
			giNewMailButton[0])) return;

		// set cursor
		SetButtonCursor(giNewMailButton[0], CURSOR_LAPTOP_SCREEN);

		// set up screen mask region
		MSYS_DefineRegion(&pScreenMask, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, MSYS_PRIORITY_HIGHEST-3,
						CURSOR_LAPTOP_SCREEN, MSYS_NO_CALLBACK, LapTopScreenCallBack);
		if (!staged.addRegion(pScreenMask)) return;
		gEmailNewMailResources = std::move(staged);
		MarkAButtonDirty(	giNewMailButton[0] );
		fReDrawScreenFlag = TRUE;
	}
	else if (!fNewMailFlag && !gEmailNewMailResources.empty())
	{
		gEmailNewMailResources.clear();

		//re draw screen
		// redraw screen
		fPausedReDrawScreenFlag=TRUE;
	}
}

BOOLEAN DisplayNewMailBox( void )
{
	HVOBJECT hHandle;
	// will display a new mail box whenever new mail has arrived

	// check if we are video conferencing, if so, do nothing
	if( gubVideoConferencingMode != 0 )
	{
		return( FALSE );
	}

	// just stopped displaying box, reset old flag
	if( ( !fNewMailFlag ) && ( fOldNewMailFlag ) )
	{
		fOldNewMailFlag=FALSE;
		return ( FALSE );
	}

	// not even set, leave NOW!
	if( !fNewMailFlag )
		return ( FALSE );

	// is set but already drawn, LEAVE NOW!
	//if( ( fNewMailFlag ) && ( fOldNewMailFlag ) )
	//	return ( FALSE );
	
	GetVideoObject( &hHandle, guiEmailWarning );
	BltVideoObject( FRAME_BUFFER, hHandle, 0,EMAIL_WARNING_X, EMAIL_WARNING_Y, VO_BLT_SRCTRANSPARENCY,NULL );
	
	// the icon for the title of this box
	GetVideoObject( &hHandle, guiTITLEBARICONS );
	BltVideoObject( FRAME_BUFFER, hHandle, 0,EMAIL_WARNING_X + 5, EMAIL_WARNING_Y + 2, VO_BLT_SRCTRANSPARENCY,NULL );

	// font stuff
	SetFont( EMAIL_HEADER_FONT );
	SetFontForeground( FONT_WHITE );
	SetFontBackground( FONT_BLACK );
	SetFontShadow( DEFAULT_SHADOW );

	// print warning
	mprintf(EMAIL_WARNING_X + 30, EMAIL_WARNING_Y + 8, L"%s",
		i18n::GetCompiledTextPack().text(i18n::TextKey::EmailTitle).data());

	// font stuff
	SetFontShadow( NO_SHADOW );
	SetFont( EMAIL_WARNING_FONT );
	SetFontForeground( FONT_BLACK );

	// printf warning string
	mprintf(EMAIL_WARNING_X + 60, EMAIL_WARNING_Y + 63, L"%s", pNewMailStrings[0] );
	DrawLapTopIcons( );

	// invalidate region
	InvalidateRegion( EMAIL_WARNING_X, EMAIL_WARNING_Y, EMAIL_WARNING_X + 270, EMAIL_WARNING_Y + 200 );

	// mark button
	if (!gEmailNewMailResources.empty())
		MarkAButtonDirty(giNewMailButton[0]);

	// reset shadow
	SetFontShadow( DEFAULT_SHADOW );

	// redraw icons

	// set box as displayed
	fOldNewMailFlag=TRUE;

	// return
	return ( TRUE );
}

void ReDrawNewMailBox( void )
{
	// this function will check to see if the new mail region needs to be redrawn
	if( fReDrawNewMailFlag == TRUE )
	{
		if( fNewMailFlag )
		{
			// set display flag back to orginal
			fNewMailFlag = FALSE;

			// display new mail box
			DisplayNewMailBox( );

			// dirty buttons
			if (!gEmailNewMailResources.empty())
				MarkAButtonDirty(giNewMailButton[0]);

			// set display flag back to orginal
			fNewMailFlag = TRUE;

			// time to redraw
			DisplayNewMailBox( );
		}
		
		// reset flag for redraw
		fReDrawNewMailFlag = FALSE;
	}
}

void NextRegionButtonCallback(GUI_BUTTON *btn,INT32 reason )
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

		// not on last page, move ahead one
			if(iCurrentPage <iLastPage)
			{
				iCurrentPage++;
				RenderEmail();
				MarkButtonsDirty( );
			}
		}
 }
 else if (reason & MSYS_CALLBACK_REASON_RBUTTON_UP)
 {
	// nothing yet
 }
}

void BtnPreviousEmailPageCallback(GUI_BUTTON *btn,INT32 reason)
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
			if( giMessagePage > 0 )
			{
				giMessagePage--;
			}

			btn->uiFlags&=~(BUTTON_CLICKED_ON);

			RenderEmail();
			MarkButtonsDirty( );
		}
	}
	else if (reason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
	// nothing yet
	}
}

void BtnNextEmailPageCallback(GUI_BUTTON *btn,INT32 reason)
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
	// not on last page, move ahead one
		btn->uiFlags&=~(BUTTON_CLICKED_ON);

	if (!fOnLastPageFlag &&
		LaptopEmailListModel::HasNextBodyPage(
			static_cast<std::size_t>(std::max(giMessagePage, 0)),
			static_cast<std::size_t>(
				std::max(giNumberOfPagesToCurrentEmail, 0)),
			MAX_NUMBER_EMAIL_PAGES))
		{
			++giMessagePage;
		}

		MarkButtonsDirty( );
		fReDrawScreenFlag = TRUE;
	}
	else if (reason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
	// nothing yet
	}
}

void PreviousRegionButtonCallback(GUI_BUTTON *btn,INT32 reason)
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
			// if we are not on forst page, more back one
			if(iCurrentPage>0)
			{
				iCurrentPage--;
				RenderEmail();
				MarkButtonsDirty( );
			}
		}
 }
 else if (reason & MSYS_CALLBACK_REASON_RBUTTON_UP)
 {
	// nothing yet
 }
}

static void BtnDeleteNoback(GUI_BUTTON *btn,INT32 reason)
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
		fDeleteMailFlag=FALSE;
		fReDrawScreenFlag=TRUE;
		}
	}
}

static void BtnDeleteYesback(GUI_BUTTON *btn,INT32 reason)
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
		fReDrawScreenFlag=TRUE;
		DeleteEmail();

		}
	}
}

void CreateDestroyDeleteNoticeMailButton()
{
 if (fDeleteMailFlag && gEmailDeleteResources.empty())
 {
	LaptopPageResourceOwner staged;
	// confirm delete email buttons

	// YES button
	if (!staged.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\YesNoButtons.sti", -1, 0, -1, 1, -1),
		giDeleteMailButtonImage[0])) return;
	if (!staged.addButton(QuickCreateButton(giDeleteMailButtonImage[0],
		NEW_BTN_X + 1, NEW_BTN_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 2,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
										(GUI_CALLBACK)BtnDeleteYesback),
		giDeleteMailButton[0])) return;

	// NO button
	if (!staged.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\YesNoButtons.sti", -1, 2, -1, 3, -1),
		giDeleteMailButtonImage[1])) return;
	if (!staged.addButton(QuickCreateButton(giDeleteMailButtonImage[1],
		NEW_BTN_X + 40, NEW_BTN_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 2,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
										(GUI_CALLBACK)BtnDeleteNoback),
		giDeleteMailButton[1])) return;

	// set up cursors
	SetButtonCursor(giDeleteMailButton[0], CURSOR_LAPTOP_SCREEN);
	SetButtonCursor(giDeleteMailButton[1], CURSOR_LAPTOP_SCREEN);

	// set up screen mask to prevent other actions while delete mail box is destroyed
	MSYS_DefineRegion(&pDeleteScreenMask,0, 0,SCREEN_WIDTH, SCREEN_HEIGHT,
		MSYS_PRIORITY_HIGHEST-3,CURSOR_LAPTOP_SCREEN, MSYS_NO_CALLBACK, LapTopScreenCallBack);
	if (!staged.addRegion(pDeleteScreenMask)) return;
	gEmailDeleteResources = std::move(staged);

	// force update
	fReDrawScreenFlag = TRUE;

 }
 else if (!fDeleteMailFlag && !gEmailDeleteResources.empty())
 {

	gEmailDeleteResources.clear();

	// force refresh
	fReDrawScreenFlag=TRUE;
 }
}

BOOLEAN DisplayDeleteNotice(EmailPtr pMail)
{
	HVOBJECT hHandle;
	// will display a delete mail box whenever delete mail has arrived
	if(!fDeleteMailFlag)
		return(FALSE);

	if( !fReDrawScreenFlag )
	{
		// no redraw flag, leave
		return( FALSE );
	}

	// error check.. no valid message passed
	if( pMail == NULL )
	{
		return ( FALSE );
	}
	
	// load graphics

	GetVideoObject(&hHandle, guiEmailWarning);
	BltVideoObject(FRAME_BUFFER, hHandle, 0,EMAIL_WARNING_X, EMAIL_WARNING_Y, VO_BLT_SRCTRANSPARENCY,NULL);
	
	// font stuff
	SetFont( EMAIL_HEADER_FONT );
	SetFontForeground( FONT_WHITE );
	SetFontBackground( FONT_BLACK );
	SetFontShadow( DEFAULT_SHADOW );

	// the icon for the title of this box
	GetVideoObject( &hHandle, guiTITLEBARICONS );
	BltVideoObject( FRAME_BUFFER, hHandle, 0,EMAIL_WARNING_X + 5, EMAIL_WARNING_Y + 2, VO_BLT_SRCTRANSPARENCY,NULL );

	// title
	mprintf(EMAIL_WARNING_X + 30, EMAIL_WARNING_Y + 8, L"%s",
		i18n::GetCompiledTextPack().text(i18n::TextKey::EmailTitle).data());

	// shadow, font, and foreground
	SetFontShadow( NO_SHADOW );
	SetFont( EMAIL_WARNING_FONT );
	SetFontForeground( FONT_BLACK );

	// draw text based on mail being read or not
	if((pMail->fRead))
		mprintf(EMAIL_WARNING_X + 95 , EMAIL_WARNING_Y + 65,L"%s", pDeleteMailStrings[0]);
	else
		mprintf(EMAIL_WARNING_X + 70, EMAIL_WARNING_Y + 65,L"%s", pDeleteMailStrings[1]);
	
	// invalidate screen area, for refresh

	if( ! fNewMailFlag )
	{
		// draw buttons
	MarkButtonsDirty( );
	InvalidateRegion(EMAIL_WARNING_X, EMAIL_WARNING_Y ,EMAIL_WARNING_X+EMAIL_WARNING_WIDTH,EMAIL_WARNING_Y+EMAIL_WARNING_HEIGHT);
	}

	// reset font shadow
	SetFontShadow(DEFAULT_SHADOW);

	return ( TRUE );
}

void DeleteEmail()
{
	// error check, invalid mail, or not time to delete mail
	if((iDeleteId==-1)||(!fDeleteMailFlag))
		return;
	// remove the message
	if (!RemoveEmailMessage(iDeleteId)) return;

	// stop displaying message, if so
	fDisplayMessageFlag = FALSE;

	// redraw icons (if deleted message was last unread, remove checkmark)
	DrawLapTopIcons();

	// rerender mail list
	RenderEmail();

	// nolong time to delete mail
	fDeleteMailFlag=FALSE;
	fReDrawScreenFlag=TRUE;
	// refresh screen (get rid of dialog box image)
	//ReDraw();

	// invalidate
	InvalidateRegion(0,0,SCREEN_WIDTH, SCREEN_HEIGHT);
}


void FromCallback(GUI_BUTTON *btn, INT32 iReason )
{
 if (iReason & MSYS_CALLBACK_REASON_INIT)
 {
	return;
 }
 if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
 {
	// sort messages based on sender name, then replace into pages of email
	fSortSenderUpwards = !fSortSenderUpwards;

	SortMessages(SENDER);

	//SpecifyButtonIcon( giSortButton[1] , giArrowsForEmail, UINT16 usVideoObjectIndex,	INT8 bXOffset, INT8 bYOffset, TRUE );

	PlaceMessagesinPages();
	btn->uiFlags&= ~(BUTTON_CLICKED_ON);
 }

 else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
 {
	// nothing yet
 }
}

void SubjectCallback(GUI_BUTTON *btn, INT32 iReason )
{
 if (iReason & MSYS_CALLBACK_REASON_INIT)
 {
	return;
 }
 if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
 {
	// sort message on subject and reorder list
	fSortSubjectUpwards = !fSortSubjectUpwards;

	SortMessages(SUBJECT);
	PlaceMessagesinPages();
		
	btn->uiFlags&= ~(BUTTON_CLICKED_ON);
 }
 else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
 {
	// nothing yet
 }

}

void BtnDeleteCallback(GUI_BUTTON *btn, INT32 iReason )
{
 if (iReason & MSYS_CALLBACK_REASON_INIT)
 {
	return;
 }
 if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
 {

	btn->uiFlags&= ~(BUTTON_CLICKED_ON);
	iDeleteId = giMessageId;
	fDeleteMailFlag = TRUE;

 }
 else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
 {
	// nothing yet
 }
}

void DateCallback(GUI_BUTTON *btn, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
		return;
	}
	if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		// sort messages based on date recieved and reorder lsit
		fSortDateUpwards = !fSortDateUpwards;
		SortMessages(RECEIVED);
		PlaceMessagesinPages();

		btn->uiFlags&= ~(BUTTON_CLICKED_ON);
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
		// nothing yet
	}
}

void ReadCallback(GUI_BUTTON *btn, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
		return;
	}

	if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		// sort messages based on date recieved and reorder lsit
		SortMessages(READ);
		PlaceMessagesinPages();

		btn->uiFlags&= ~(BUTTON_CLICKED_ON);
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
		// nothing yet
	}
}

void ViewMessageRegionCallBack( MOUSE_REGION * pRegion, INT32 iReason )
{
	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
		return;
	}
	if( iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		// simulate X button has been pressed and let up, this means to stop displaying the currently displayed message

		// reset display message flag
		fDisplayMessageFlag=FALSE;

		// reset page being displayed
		giMessagePage = 0;

		// redraw icons
		DrawLapTopIcons();

		// force update of entire screen
		fPausedReDrawScreenFlag=TRUE;

		// rerender email
		//RenderEmail();
	}
	else if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
		// delete message
		iDeleteId = giMessageId;
		fDeleteMailFlag = TRUE;
	}
}

void DisplayTextOnTitleBar( void )
{
	// draw email screen title text

	// font stuff
	SetFont( EMAIL_TITLE_FONT );
	SetFontForeground( FONT_WHITE );
	SetFontBackground( FONT_BLACK );

	// printf the title
	mprintf(EMAIL_TITLE_X, EMAIL_TITLE_Y, L"%s",
		i18n::GetCompiledTextPack().text(i18n::TextKey::EmailTitle).data());

	// reset the shadow
}

BOOLEAN CreateMailScreenButtons(LaptopPageResourceOwner& owner)
{
	// create sort buttons, right now - not finished

	// read sort
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\mailbuttons.sti", -1, 0, -1, 4, -1),
		giSortButtonImage[0])) return FALSE;
	if (!owner.addButton(QuickCreateButton(giSortButtonImage[0],
		ENVELOPE_BOX_X, FROM_BOX_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
										(GUI_CALLBACK)ReadCallback),
		giSortButton[0])) return FALSE;
	SetButtonCursor(giSortButton[0], CURSOR_LAPTOP_SCREEN);

	// subject sort
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\mailbuttons.sti", -1, 1, -1, 5, -1),
		giSortButtonImage[1])) return FALSE;
	if (!owner.addButton(QuickCreateButton(giSortButtonImage[1],
		FROM_BOX_X, FROM_BOX_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
										(GUI_CALLBACK)FromCallback),
		giSortButton[1])) return FALSE;
	SetButtonCursor(giSortButton[1], CURSOR_LAPTOP_SCREEN);
	SpecifyFullButtonTextAttributes( giSortButton[1], pEmailHeaders[FROM_HEADER], EMAIL_WARNING_FONT,
																		FONT_BLACK, FONT_BLACK,
																			FONT_BLACK, FONT_BLACK, TEXT_CJUSTIFIED );

	// sender sort
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\mailbuttons.sti", -1, 2, -1, 6, -1),
		giSortButtonImage[2])) return FALSE;
	if (!owner.addButton(QuickCreateButton(giSortButtonImage[2],
		SUBJECT_BOX_X, FROM_BOX_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
										(GUI_CALLBACK)SubjectCallback),
		giSortButton[2])) return FALSE;
	SetButtonCursor(giSortButton[2], CURSOR_LAPTOP_SCREEN);
	SpecifyFullButtonTextAttributes( giSortButton[2], pEmailHeaders[SUBJECT_HEADER], EMAIL_WARNING_FONT,
																		FONT_BLACK, FONT_BLACK,
																			FONT_BLACK, FONT_BLACK, TEXT_CJUSTIFIED );

	// date sort
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\mailbuttons.sti", -1, 3, -1, 7, -1),
		giSortButtonImage[3])) return FALSE;
	if (!owner.addButton(QuickCreateButton(giSortButtonImage[3],
		DATE_BOX_X, FROM_BOX_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
										(GUI_CALLBACK)DateCallback),
		giSortButton[3])) return FALSE;
	SetButtonCursor(giSortButton[3], CURSOR_LAPTOP_SCREEN);
	SpecifyFullButtonTextAttributes( giSortButton[3], pEmailHeaders[RECD_HEADER], EMAIL_WARNING_FONT,
																				FONT_BLACK, FONT_BLACK,
																					FONT_BLACK, FONT_BLACK, TEXT_CJUSTIFIED );
	return TRUE;
}

void DisplayEmailMessageSubjectDateFromLines( EmailPtr pMail , INT32 iViewerY)
{
	// this procedure will draw the title/headers to From, Subject, Date fields in the display
	// message box
	INT16 usX, usY;
	CHAR16 sString[100];

	// font stuff
	SetFont(MESSAGE_FONT);
	SetFontForeground(FONT_BLACK);
	SetFontBackground(FONT_BLACK);
	SetFontShadow(NO_SHADOW);
	
	// all headers, but not info are right justified

	// print from
	FindFontRightCoordinates( MESSAGE_HEADER_X-20, ( INT16 ) ( MESSAGE_FROM_Y + (INT16)iViewerY ) ,	MESSAGE_HEADER_WIDTH, ( INT16 ) ( MESSAGE_FROM_Y + GetFontHeight ( MESSAGE_FONT ) ) ,pEmailHeaders[0] ,MESSAGE_FONT, &usX, &usY);
	mprintf( usX, MESSAGE_FROM_Y + (UINT16)iViewerY, L"%s", pEmailHeaders[0]);

	// the actual from info
	if ( pMail->EmailVersion == TYPE_EMAIL_AIM_AVAILABLE || pMail->EmailVersion == TYPE_EMAIL_MERC_LEVEL_UP )
	{
		if (LaptopEmailListModel::IsIndexInRange(
				pMail->ubSender, NUM_PROFILES) &&
			gMercProfiles[pMail->ubSender].zNickname[0] != L'\0')
		mprintf( MESSAGE_HEADER_X+MESSAGE_HEADER_WIDTH-13, MESSAGE_FROM_Y + iViewerY, L"%s", gMercProfiles[ pMail->ubSender ].zNickname);
		else
		mprintf( MESSAGE_HEADER_X+MESSAGE_HEADER_WIDTH-13, MESSAGE_FROM_Y + iViewerY, L"%s", L"None");
	}		
	else if ( pMail->EmailVersion == TYPE_EMAIL_EMAIL_EDT || pMail->EmailVersion == TYPE_EMAIL_BOBBY_R || pMail->EmailVersion == TYPE_EMAIL_BOBBY_R_EMAIL_JA2_EDT || pMail->EmailVersion == TYPE_EMAIL_INSURANCE_COMPANY_EMAIL_JA2_EDT || pMail->EmailVersion == TYPE_EMAIL_DEAD_MERC_AIM_SITE_EMAIL_JA2_EDT || pMail->EmailVersion == TYPE_EMAIL_XML)
	{
		mprintf( MESSAGE_HEADER_X+MESSAGE_HEADER_WIDTH-13, MESSAGE_FROM_Y + iViewerY, L"%s", pSenderNameList[pMail->ubSender]);
	}
	else if ( pMail->EmailVersion == TYPE_EMAIL_EMAIL_EDT_NAME_MERC )
	{
		if (LaptopEmailListModel::IsIndexInRange(
				pMail->ubSender, NUM_PROFILES) &&
			gMercProfiles[pMail->ubSender].zNickname[0] != L'\0')
		mprintf( MESSAGE_HEADER_X+MESSAGE_HEADER_WIDTH-13, MESSAGE_FROM_Y + iViewerY, L"%s", gMercProfiles[ pMail->ubSender ].zNickname);
		else
		mprintf( MESSAGE_HEADER_X+MESSAGE_HEADER_WIDTH-13, MESSAGE_FROM_Y + iViewerY, L"%s", L"None");
	}
	
	// print date
	FindFontRightCoordinates( MESSAGE_HEADER_X+168, ( INT16 ) ( MESSAGE_DATE_Y + (UINT16)iViewerY ),	MESSAGE_HEADER_WIDTH, ( INT16 ) ( MESSAGE_DATE_Y + GetFontHeight ( MESSAGE_FONT ) ) ,pEmailHeaders[2] ,MESSAGE_FONT, &usX, &usY);
	mprintf( usX, MESSAGE_DATE_Y+ (UINT16)iViewerY , L"%s", pEmailHeaders[2]);

	// the actual date info
	sgp_swprintf(sString, 100, L"%d", pMail->iDate / (24 * 60));
	mprintf( MESSAGE_HEADER_X+235, MESSAGE_DATE_Y + (UINT16)iViewerY, L"%s", sString);



	// print subject
	FindFontRightCoordinates( MESSAGE_HEADER_X-20, MESSAGE_SUBJECT_Y ,	MESSAGE_HEADER_WIDTH, ( INT16 ) (MESSAGE_SUBJECT_Y + GetFontHeight ( MESSAGE_FONT )),pEmailHeaders[1] ,MESSAGE_FONT, &usX, &usY);
	mprintf( usX, MESSAGE_SUBJECT_Y + (UINT16)iViewerY, L"%s", pEmailHeaders[1]);

 	// the actual subject info
	//mprintf( , MESSAGE_SUBJECT_Y, pMail->pSubject);
	IanDisplayWrappedString(SUBJECT_LINE_X+2, (INT16) ( SUBJECT_LINE_Y+2 + (UINT16)iViewerY ), SUBJECT_LINE_WIDTH, MESSAGE_GAP, MESSAGE_FONT, MESSAGE_COLOR,pMail->pSubject,0,FALSE,0);


	// reset shadow
	SetFontShadow(DEFAULT_SHADOW);
	return;
}


void DrawEmailMessageDisplayTitleText( INT32 iViewerY )
{
	// this procedure will display the title of the email message display box

	// font stuff
	SetFont( EMAIL_HEADER_FONT );
	SetFontForeground( FONT_WHITE );
	SetFontBackground( FONT_BLACK );

	// dsiplay mail viewer title on message viewer
	mprintf(VIEWER_X + 30, VIEWER_Y + 8 + (UINT16)iViewerY, L"%s",
		i18n::GetCompiledTextPack().text(i18n::TextKey::EmailTitle).data());

	return;
}

void DrawLineDividers( void )
{
	// this function draws divider lines between lines of text
	INT32 iCounter=0;
	HVOBJECT hHandle;

	for(iCounter=1; iCounter < 19; iCounter++)
	{
	GetVideoObject( &hHandle, guiMAILDIVIDER );
	BltVideoObject(FRAME_BUFFER, hHandle, 0,INDIC_X-10, (MIDDLE_Y+iCounter*MIDDLE_WIDTH - 1), VO_BLT_SRCTRANSPARENCY,NULL);
	}


	return;
}


void ClearOutEmailMessageRecordsList( void )
{
	RecordPtr pTempRecord;
	INT32 iCounter = 0;

	// runt hrough list freeing records up
	while(pMessageRecordList)
	{
	// set temp to current
	pTempRecord = pMessageRecordList;

		// next element
		pMessageRecordList = pMessageRecordList->Next;

		MemFree( pTempRecord );
	}

	for( iCounter = 0; iCounter < MAX_NUMBER_EMAIL_PAGES; iCounter++ )
	{
		pEmailPageInfo[ iCounter ].pFirstRecord = NULL;
		pEmailPageInfo[ iCounter ].pLastRecord = NULL;
		pEmailPageInfo[ iCounter ].iPageNumber = iCounter;
	}

	// null out list
	pMessageRecordList = NULL;
	gEmailRecordBuildFailed = false;
	giMessagePage = 0;
	giNumberOfPagesToCurrentEmail = 1;
	fOnLastPageFlag = TRUE;

	return;
}

void AddEmailRecordToList(const CHAR16* pString)
{
	if (gEmailRecordBuildFailed) return;
	RecordPtr staged = static_cast<RecordPtr>(MemAlloc(sizeof(Record)));
	if (!staged)
	{
		gEmailRecordBuildFailed = true;
		return;
	}
	staged->Next = nullptr;
	if (!CopyEmailText(staged->pRecord, pString))
	{
		MemFree(staged);
		gEmailRecordBuildFailed = true;
		return;
	}

	if (!pMessageRecordList)
	{
		pMessageRecordList = staged;
		return;
	}
	RecordPtr tail = pMessageRecordList;
	while (tail->Next) tail = tail->Next;
	tail->Next = staged;
}

void UpDateMessageRecordList( void )
{
	// simply checks to see if old and new message ids are the same, if so, do nothing
	// otherwise clear list

	if( giMessageId != giPrevMessageId )
	{
		// if chenged, clear list
		ClearOutEmailMessageRecordsList( );

		// set prev to current
		giPrevMessageId = giMessageId;
	}
}

void HandleAnySpecialEmailMessageEvents(INT32 iMessageId )
{
	const CommunicationsPolicy policy = CurrentCommunicationsPolicy();
	if (policy.isMakeContactMessage(static_cast<UINT16>(iMessageId)))
	{
		if (!(gJa25SaveStruct.ubEmailFromSectorFlag &
			SECTOR_EMAIL__ANOTHER_SECTOR))
		{
			AddStrategicEvent(EVENT_SEND_ENRICO_UNDERSTANDING_EMAIL,
				GetWorldTotalMin() + (2 * 60) + Random(120), 0);
		}
	}

	if (iMessageId == policy.impReminderOffset() ||
		iMessageId == policy.impIntroOffset())
	{
		SetBookMark(IMP_BOOKMARK);
	}
}

void ReDisplayBoxes( void )
{
	// the email message itself
	if(fDisplayMessageFlag)
	{
		// this simply redraws message with button manipulation
		DisplayEmailMessage(GetEmailMessage(giMessageId));
	}

	if(fDeleteMailFlag)
	{
		// delete message, redisplay
		DisplayDeleteNotice(GetEmailMessage(iDeleteId));
	}

	if(fNewMailFlag)
	{
		// if new mail, redisplay box
		DisplayNewMailBox( );
	}
}

void HandleUnfinishedBusinessMailSpecialMessages(
	INT32* iResults, EmailPtr pMail)
{
	const CommunicationsPolicy policy = CurrentCommunicationsPolicy();
	if (!policy.usesUnfinishedBusinessCatalog()) return;

	const auto matches = [pMail](
		const CommunicationsPolicy::EmailRecord& record)
	{
		return (pMail->EmailVersion == TYPE_EMAIL_XML &&
			pMail->iThirdData == record.offset) ||
			(pMail->EmailVersion != TYPE_EMAIL_XML &&
				pMail->EmailType == static_cast<UINT8>(record.substitution));
	};

	const auto payment = policy.insuranceRecord(
		CommunicationsPolicy::InsuranceNotice::Payment);
	const auto fraud = policy.insuranceRecord(
		CommunicationsPolicy::InsuranceNotice::VerySuspiciousFraud);
	const auto death = policy.deadMercNoticeRecord();
	const auto noRefund = policy.aimNoRefundRecord();
	if ((matches(payment) || matches(fraud)) &&
		gGameUBOptions.LaptopLinkInsurance == TRUE)
	{
		ModifyInsuranceEmails(
			static_cast<UINT16>(pMail->EmailVersion == TYPE_EMAIL_XML
				? pMail->iThirdData : pMail->usOffset),
			iResults, pMail);
		return;
	}
	if ((matches(death) || matches(noRefund)) &&
		gGameUBOptions.fDeadMerc == TRUE)
	{
		ModifyInsuranceEmails(
			static_cast<UINT16>(pMail->EmailVersion == TYPE_EMAIL_XML
				? pMail->iThirdData : pMail->usOffset),
			iResults, pMail);
		return;
	}

	const auto shipment = policy.bobbyShipmentRecord();
	if (!matches(shipment) || gGameUBOptions.fBobbyRSite != TRUE ||
		pMessageRecordList)
	{
		return;
	}

	gusCurShipmentDestinationID = pMail->iCurrentShipmentDestinationID;
	for (UINT16 line = 0; line < shipment.length; ++line)
	{
		wstring mail;
		if (pMail->EmailVersion == TYPE_EMAIL_XML &&
			pMail->usOffset < gEmails.size() &&
			line < gEmails[pMail->usOffset].Messages.size())
		{
			mail = gEmails[pMail->usOffset].Messages[line];
		}
		else
		{
			CHAR16 mailRecord[MAIL_STRING_SIZE]{};
			LoadEncryptedDataFromFile(EMAIL_EDT_FILE_JA2, mailRecord,
				MAIL_STRING_SIZE * (shipment.offset + line), MAIL_STRING_SIZE);
			mail = mailRecord;
		}

		const wstring placeholder = L"$DESTINATIONNAME$";
		const auto placeholderPosition = mail.find(placeholder);
		if (placeholderPosition != wstring::npos &&
			gusCurShipmentDestinationID > 0)
		{
			RefToDestinationStruct destination =
				gPostalService.GetDestination(gusCurShipmentDestinationID);
			if (destination.usID == gusCurShipmentDestinationID)
			{
				mail.replace(placeholderPosition, placeholder.length(),
					destination.wstrName);
			}
		}
		AddEmailRecordToList(mail.c_str());
	}
	giPrevMessageId = giMessageId;
}

BOOLEAN HandleMailSpecialMessages( UINT16 usMessageId, INT32 *iResults, EmailPtr pMail )
{
	BOOLEAN fSpecialCase = FALSE;
	const CommunicationsPolicy policy = CurrentCommunicationsPolicy();

	// UB's IMP result and Bobby shipment records both use offset 198. The mail
	// version is therefore part of the identity, not just the numeric record.
	if (policy.isImpProfileResultsMessage(usMessageId,
		pMail->EmailVersion == TYPE_EMAIL_EMAIL_EDT))
	{
		HandleIMPCharProfileResultsMessage();
		return TRUE;
	}

	if (policy.usesUnfinishedBusinessCatalog())
	{
		HandleUnfinishedBusinessMailSpecialMessages(iResults, pMail);
		return FALSE;
	}

	// this procedure will handle special cases of email messages that are not stored in email.edt, or need special processing
	switch( usMessageId )
	{
		case JA2_EMAIL_MERC_INTRO:
			SetBookMark( MERC_BOOKMARK );
			fReDrawScreenFlag = TRUE;
			break;
		
		case JA2_EMAIL_INSURANCE_PAYMENT:
		case JA2_EMAIL_INSURANCE_SUSPICIOUS:
		case JA2_EMAIL_INSURANCE_SUSPICIOUS_REPEAT:
		case JA2_EMAIL_INSURANCE_INVESTIGATION_OVER:
		case JA2_EMAIL_INSURANCE_POLICY_VIOLATION:
		case JA2_EMAIL_INSURANCE_ONE_HOUR_FRAUD:
		case JA2_EMAIL_MERC_DIED_OTHER_ASSIGNMENT:
		case JA2_EMAIL_AIM_MEDICAL_DEPOSIT_REFUND:
		case JA2_EMAIL_AIM_MEDICAL_DEPOSIT_NO_REFUND:
		case JA2_EMAIL_AIM_MEDICAL_DEPOSIT_PARTIAL_REFUND:
			ModifyInsuranceEmails(usMessageId, iResults, pMail);
			break;

		case JA2_EMAIL_MERC_NEW_SITE_ADDRESS:
			//Set the book mark so the player can access the site
			SetBookMark( MERC_BOOKMARK );
			break;
					
		//Dealtar's Airport Externalization
		case JA2_EMAIL_BOBBYR_SHIPMENT_ARRIVED:
			if (!pMessageRecordList)
			{
				wstring wstrMail;
				wstring::size_type index;
				CHAR16 szMail[MAIL_STRING_SIZE]{};

				// WANNE.MAIL: Fix
				gusCurShipmentDestinationID = -1;	// Reset
				gusCurShipmentDestinationID = pMail->iCurrentShipmentDestinationID;

				// Loop through each line of the shipment EDT-file
				for (int i = 0; i < pMail->usLength; ++i)
				{
					wstrMail.clear();
					if (pMail->EmailVersion == TYPE_EMAIL_XML &&
						pMail->usOffset < gEmails.size() &&
						static_cast<size_t>(i) <
							gEmails[pMail->usOffset].Messages.size())
					{
						wstrMail = gEmails[pMail->usOffset].Messages[i];
					}
					else
					{
						LoadEncryptedDataFromFile(EMAIL_EDT_FILE_JA2, szMail,
							MAIL_STRING_SIZE * usMessageId, MAIL_STRING_SIZE);
						wstrMail = szMail;
					}

/*			if (FileExists(EMAIL_EDT_FILE_JA25))
			{
			LoadEncryptedDataFromFile(EMAIL_EDT_FILE_JA25, szMail, MAIL_STRING_SIZE *usMessageId, MAIL_STRING_SIZE);
			}
			else
			{
			LoadEncryptedDataFromFile(EMAIL_EDT_FILE_JA2, szMail, MAIL_STRING_SIZE *usMessageId, MAIL_STRING_SIZE);
			}
*/
					index = wstrMail.find(L"$DESTINATIONNAME$");

					// WANNE.MAIL: Fix
					if (gusCurShipmentDestinationID > 0)
					{
						if (index != wstring::npos)
						{
							RefToDestinationStruct ds =
								gPostalService.GetDestination(gusCurShipmentDestinationID);
							if (ds.usID == gusCurShipmentDestinationID)
							{
								wstrMail.erase(index,
									wcslen(L"$DESTINATIONNAME$"));
								wstrMail.insert(index, ds.wstrName.c_str());
							}
						}
					}

					AddEmailRecordToList(wstrMail.c_str());

					++usMessageId;
				}
			}
			giPrevMessageId = giMessageId;
		break;

		case JA2_EMAIL_PMC_INTRO:
			SetBookMark( PMC_BOOKMARK );
			fReDrawScreenFlag = TRUE;
			break;

		case JA2_EMAIL_MILITIA_ROSTER_INTRO:
			SetBookMark( MILITIAROSTER_BOOKMARK );
			fReDrawScreenFlag = TRUE;
			break;

		case JA2_EMAIL_INTEL_INTRO:
			SetBookMark( INTELMARKET_BOOKMARK );
			fReDrawScreenFlag = TRUE;
			break;
	}

	return fSpecialCase;
}
#define IMP_RESULTS_INTRO_LENGTH 9

#define IMP_RESULTS_PERSONALITY_INTRO IMP_RESULTS_INTRO_LENGTH	// 0 - 9
#define IMP_RESULTS_PERSONALITY_INTRO_LENGTH 5
#define IMP_PERSONALITY_NORMAL IMP_RESULTS_PERSONALITY_INTRO + IMP_RESULTS_PERSONALITY_INTRO_LENGTH	// 14 - 18
#define IMP_PERSONALITY_LENGTH 4
#define IMP_PERSONALITY_HEAT IMP_PERSONALITY_NORMAL + IMP_PERSONALITY_LENGTH				// 19 - 22
#define IMP_PERSONALITY_NERVOUS IMP_PERSONALITY_HEAT + IMP_PERSONALITY_LENGTH				// 23 - 26
#define IMP_PERSONALITY_CLAUSTROPHOBIC IMP_PERSONALITY_NERVOUS + IMP_PERSONALITY_LENGTH		// 27 - 30
#define IMP_PERSONALITY_NONSWIMMER IMP_PERSONALITY_CLAUSTROPHOBIC + IMP_PERSONALITY_LENGTH	// 31 - 34
#define IMP_PERSONALITY_FEAR_OF_INSECTS IMP_PERSONALITY_NONSWIMMER + IMP_PERSONALITY_LENGTH
#define IMP_PERSONALITY_FORGETFUL IMP_PERSONALITY_FEAR_OF_INSECTS + IMP_PERSONALITY_LENGTH + 1
#define IMP_PERSONALITY_PSYCHO IMP_PERSONALITY_FORGETFUL + IMP_PERSONALITY_LENGTH
#define IMP_RESULTS_ATTITUDE_INTRO IMP_PERSONALITY_PSYCHO + IMP_PERSONALITY_LENGTH + 1
#define IMP_RESULTS_ATTITUDE_LENGTH 5
#define IMP_ATTITUDE_LENGTH 5
#define IMP_ATTITUDE_NORMAL IMP_RESULTS_ATTITUDE_INTRO + IMP_RESULTS_ATTITUDE_LENGTH
#define IMP_ATTITUDE_FRIENDLY IMP_ATTITUDE_NORMAL + IMP_ATTITUDE_LENGTH
#define IMP_ATTITUDE_LONER IMP_ATTITUDE_FRIENDLY + IMP_ATTITUDE_LENGTH + 1
#define IMP_ATTITUDE_OPTIMIST IMP_ATTITUDE_LONER + IMP_ATTITUDE_LENGTH + 1
#define IMP_ATTITUDE_PESSIMIST IMP_ATTITUDE_OPTIMIST + IMP_ATTITUDE_LENGTH + 1
#define IMP_ATTITUDE_AGGRESSIVE IMP_ATTITUDE_PESSIMIST + IMP_ATTITUDE_LENGTH + 1
#define IMP_ATTITUDE_ARROGANT IMP_ATTITUDE_AGGRESSIVE + IMP_ATTITUDE_LENGTH + 1		/// 88
#define IMP_ATTITUDE_ASSHOLE IMP_ATTITUDE_ARROGANT + IMP_ATTITUDE_LENGTH + 1
#define IMP_ATTITUDE_COWARD IMP_ATTITUDE_ASSHOLE + IMP_ATTITUDE_LENGTH
#define IMP_RESULTS_SKILLS IMP_ATTITUDE_COWARD + IMP_ATTITUDE_LENGTH + 1
#define IMP_RESULTS_SKILLS_LENGTH 7
#define IMP_SKILLS_IMPERIAL_SKILLS IMP_RESULTS_SKILLS + IMP_RESULTS_SKILLS_LENGTH + 1
#define IMP_SKILLS_IMPERIAL_MARK IMP_SKILLS_IMPERIAL_SKILLS + 1
#define IMP_SKILLS_IMPERIAL_MECH IMP_SKILLS_IMPERIAL_SKILLS + 2
#define IMP_SKILLS_IMPERIAL_EXPL IMP_SKILLS_IMPERIAL_SKILLS + 3
#define IMP_SKILLS_IMPERIAL_MED	IMP_SKILLS_IMPERIAL_SKILLS + 4

#define IMP_SKILLS_NEED_TRAIN_SKILLS IMP_SKILLS_IMPERIAL_MED + 1
#define IMP_SKILLS_NEED_TRAIN_MARK IMP_SKILLS_NEED_TRAIN_SKILLS + 1		// 119
#define IMP_SKILLS_NEED_TRAIN_MECH IMP_SKILLS_NEED_TRAIN_SKILLS + 2
#define IMP_SKILLS_NEED_TRAIN_EXPL IMP_SKILLS_NEED_TRAIN_SKILLS + 3
#define IMP_SKILLS_NEED_TRAIN_MED IMP_SKILLS_NEED_TRAIN_SKILLS + 4

#define IMP_SKILLS_NO_SKILL IMP_SKILLS_NEED_TRAIN_MED + 1
#define IMP_SKILLS_NO_SKILL_MARK	IMP_SKILLS_NO_SKILL + 1		// 124
#define IMP_SKILLS_NO_SKILL_MECH	IMP_SKILLS_NO_SKILL + 2
#define IMP_SKILLS_NO_SKILL_EXPL	IMP_SKILLS_NO_SKILL + 3
#define IMP_SKILLS_NO_SKILL_MED	IMP_SKILLS_NO_SKILL + 4		// 127

#define IMP_SKILLS_SPECIAL_INTRO IMP_SKILLS_NO_SKILL_MED + 1	// 128, 129
#define IMP_SKILLS_SPECIAL_INTRO_LENGTH 2
#define IMP_SKILLS_SPECIAL_LOCK IMP_SKILLS_SPECIAL_INTRO + IMP_SKILLS_SPECIAL_INTRO_LENGTH		// 130
#define IMP_SKILLS_SPECIAL_HAND IMP_SKILLS_SPECIAL_LOCK + 1
#define IMP_SKILLS_SPECIAL_ELEC IMP_SKILLS_SPECIAL_HAND + 1
#define IMP_SKILLS_SPECIAL_NIGHT IMP_SKILLS_SPECIAL_ELEC + 1
#define IMP_SKILLS_SPECIAL_THROW IMP_SKILLS_SPECIAL_NIGHT + 1
#define IMP_SKILLS_SPECIAL_TEACH IMP_SKILLS_SPECIAL_THROW + 1
#define IMP_SKILLS_SPECIAL_HEAVY IMP_SKILLS_SPECIAL_TEACH + 1
#define IMP_SKILLS_SPECIAL_AUTO IMP_SKILLS_SPECIAL_HEAVY + 1				// 137
#define IMP_SKILLS_SPECIAL_STEALTH IMP_SKILLS_SPECIAL_AUTO + 1
#define IMP_SKILLS_SPECIAL_AMBI IMP_SKILLS_SPECIAL_STEALTH + 1
#define IMP_SKILLS_SPECIAL_THIEF IMP_SKILLS_SPECIAL_AMBI + 1
#define IMP_SKILLS_SPECIAL_MARTIAL IMP_SKILLS_SPECIAL_THIEF + 1
#define IMP_SKILLS_SPECIAL_KNIFE IMP_SKILLS_SPECIAL_MARTIAL + 1				// 142

// WANNE: These 2 skills are missing in the email!
// The problem is, that there is no description in binarydata\impass.edt
#define IMP_SKILLS_SPECIAL_SNIPER IMP_SKILLS_SPECIAL_KNIFE + 1
#define IMP_SKILLS_SPECIAL_CAMOUFLAGED IMP_SKILLS_SPECIAL_SNIPER + 1

#define IMP_RESULTS_PHYSICAL IMP_SKILLS_SPECIAL_KNIFE + 1
#define IMP_RESULTS_PHYSICAL_LENGTH 7

#define IMP_PHYSICAL_SUPER IMP_RESULTS_PHYSICAL + IMP_RESULTS_PHYSICAL_LENGTH
#define IMP_PHYSICAL_SUPER_LENGTH 1

#define IMP_PHYSICAL_SUPER_HEALTH IMP_PHYSICAL_SUPER + IMP_PHYSICAL_SUPER_LENGTH
#define IMP_PHYSICAL_SUPER_AGILITY IMP_PHYSICAL_SUPER_HEALTH + 1
#define IMP_PHYSICAL_SUPER_DEXTERITY IMP_PHYSICAL_SUPER_AGILITY + 1
#define IMP_PHYSICAL_SUPER_STRENGTH IMP_PHYSICAL_SUPER_DEXTERITY + 1
#define IMP_PHYSICAL_SUPER_LEADERSHIP IMP_PHYSICAL_SUPER_STRENGTH + 1
#define IMP_PHYSICAL_SUPER_WISDOM IMP_PHYSICAL_SUPER_LEADERSHIP + 1

#define IMP_PHYSICAL_LOW IMP_PHYSICAL_SUPER_WISDOM + 1
#define IMP_PHYSICAL_LOW_LENGTH 1

#define IMP_PHYSICAL_LOW_HEALTH IMP_PHYSICAL_LOW + IMP_PHYSICAL_LOW_LENGTH
#define IMP_PHYSICAL_LOW_AGILITY IMP_PHYSICAL_LOW_HEALTH + 1
#define IMP_PHYSICAL_LOW_DEXTERITY IMP_PHYSICAL_LOW_AGILITY + 2
#define IMP_PHYSICAL_LOW_STRENGTH IMP_PHYSICAL_LOW_DEXTERITY + 1
#define IMP_PHYSICAL_LOW_LEADERSHIP IMP_PHYSICAL_LOW_STRENGTH + 1
#define IMP_PHYSICAL_LOW_WISDOM IMP_PHYSICAL_LOW_LEADERSHIP + 1


#define IMP_PHYSICAL_VERY_LOW IMP_PHYSICAL_LOW_WISDOM + 1
#define IMP_PHYSICAL_VERY_LOW_LENGTH 1

#define IMP_PHYSICAL_VERY_LOW_HEALTH IMP_PHYSICAL_VERY_LOW + IMP_PHYSICAL_VERY_LOW_LENGTH
#define IMP_PHYSICAL_VERY_LOW_AGILITY IMP_PHYSICAL_VERY_LOW_HEALTH + 1
#define IMP_PHYSICAL_VERY_LOW_DEXTERITY IMP_PHYSICAL_VERY_LOW_AGILITY + 1
#define IMP_PHYSICAL_VERY_LOW_STRENGTH IMP_PHYSICAL_VERY_LOW_DEXTERITY + 1
#define IMP_PHYSICAL_VERY_LOW_LEADERSHIP IMP_PHYSICAL_VERY_LOW_STRENGTH + 1
#define IMP_PHYSICAL_VERY_LOW_WISDOM IMP_PHYSICAL_VERY_LOW_LEADERSHIP + 1


#define IMP_PHYSICAL_END IMP_PHYSICAL_VERY_LOW_WISDOM + 1
#define IMP_PHYSICAL_END_LENGTH 3

#define IMP_RESULTS_PORTRAIT	IMP_PHYSICAL_END + IMP_PHYSICAL_END_LENGTH
#define IMP_RESULTS_PORTRAIT_LENGTH 6


#define IMP_PORTRAIT_MALE_1 IMP_RESULTS_PORTRAIT + IMP_RESULTS_PORTRAIT_LENGTH
#define IMP_PORTRAIT_MALE_2 IMP_PORTRAIT_MALE_1 + 4
#define IMP_PORTRAIT_MALE_3 IMP_PORTRAIT_MALE_2 + 4
#define IMP_PORTRAIT_MALE_4 IMP_PORTRAIT_MALE_3 + 4
#define IMP_PORTRAIT_MALE_5 IMP_PORTRAIT_MALE_4 + 4
#define IMP_PORTRAIT_MALE_6 IMP_PORTRAIT_MALE_5 + 4

#define IMP_PORTRAIT_FEMALE_1 IMP_PORTRAIT_MALE_6 + 4
#define IMP_PORTRAIT_FEMALE_2 IMP_PORTRAIT_FEMALE_1 + 4
#define IMP_PORTRAIT_FEMALE_3 IMP_PORTRAIT_FEMALE_2 + 4
#define IMP_PORTRAIT_FEMALE_4 IMP_PORTRAIT_FEMALE_3 + 4
#define IMP_PORTRAIT_FEMALE_5 IMP_PORTRAIT_FEMALE_4 + 4

#define IMP_RESULTS_END IMP_PORTRAIT_FEMALE_5 + 5
#define IMP_RESULTS_END_LENGTH 3

void HandleIMPCharProfileResultsMessage(void)
{
	// special case, IMP profile return
	INT32 iCounter=0;
	CHAR16 pString[MAIL_STRING_SIZE];
	INT32 iOffSet=0;
	RecordPtr pTempRecord;
	INT32 iEndOfSection =0;
	INT32 iRand = 0;
	BOOLEAN fSufficientMechSkill = FALSE, fSufficientMarkSkill = FALSE, fSufficientMedSkill = FALSE, fSufficientExplSkill = FALSE;
	BOOLEAN fSufficientHlth = FALSE, fSufficientStr = FALSE, fSufficientWis = FALSE, fSufficientAgi = FALSE, fSufficientDex = FALSE, fSufficientLdr = FALSE;

	INT16 iCurrentIMPSlot = giCurrentIMPSlot;
	if (!LaptopEmailListModel::IsIndexInRange(
			iCurrentIMPSlot, NUM_PROFILES)) return;

	iRand = Random( 32767 );

	// set record ptr to head of list
	pTempRecord=pMessageRecordList;

	// load intro
	iEndOfSection = IMP_RESULTS_INTRO_LENGTH;

	// list doesn't exist, reload
	if( !pTempRecord )
	{
		while(iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// have to place players name into string for first record
			if( iCounter == 0)
			{
				if (!AppendEmailText(pString, L" ") ||
					!AppendEmailText(
						pString, gMercProfiles[iCurrentIMPSlot].zName))
					gEmailRecordBuildFailed = true;
			}

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}

		// now the personality intro
		iOffSet = IMP_RESULTS_PERSONALITY_INTRO;
		iEndOfSection = IMP_RESULTS_PERSONALITY_INTRO_LENGTH + 1;
		iCounter = 0;

		while(iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}

		// personality itself
		switch( gMercProfiles[ iCurrentIMPSlot ].bDisability)
		{
			// normal as can be
			case( NO_DISABILITY ):
				iOffSet = IMP_PERSONALITY_NORMAL;
				break;
			case( HEAT_INTOLERANT ):
				iOffSet = IMP_PERSONALITY_HEAT;
				break;
			case( NERVOUS ):
				iOffSet = IMP_PERSONALITY_NERVOUS;
				break;
			case( CLAUSTROPHOBIC ):
				iOffSet = IMP_PERSONALITY_CLAUSTROPHOBIC;
				break;
			case( NONSWIMMER ):
				iOffSet = IMP_PERSONALITY_NONSWIMMER;
				break;
			case( FEAR_OF_INSECTS ):
				iOffSet = IMP_PERSONALITY_FEAR_OF_INSECTS;
				break;
			case( FORGETFUL ):
				iOffSet = IMP_PERSONALITY_FORGETFUL;
				break;
			case( PSYCHO ):
				iOffSet = IMP_PERSONALITY_PSYCHO;
				break;
		}

		// Flugente: new personalities do not get their text from .edt files
		if ( gMercProfiles[ iCurrentIMPSlot ].bDisability == DEAF )
		{
			CopyEmailText(pString, gzIMPDisabilityTraitEmailTextDeaf[0]);

			AddEmailRecordToList( pString );

			CopyEmailText(pString, gzIMPDisabilityTraitEmailTextDeaf[1]);

			AddEmailRecordToList( pString );
		}
		else if ( gMercProfiles[ iCurrentIMPSlot ].bDisability == SHORTSIGHTED )
		{
			CopyEmailText(pString, gzIMPDisabilityTraitEmailTextShortSighted[0]);

			AddEmailRecordToList( pString );

			CopyEmailText(pString, gzIMPDisabilityTraitEmailTextShortSighted[1]);

			AddEmailRecordToList( pString );
		}
		else if ( gMercProfiles[iCurrentIMPSlot].bDisability == HEMOPHILIAC )
		{
			CopyEmailText(pString, gzIMPDisabilityTraitEmailTextHemophiliac[0]);

			AddEmailRecordToList( pString );

			CopyEmailText(pString, gzIMPDisabilityTraitEmailTextHemophiliac[1]);

			AddEmailRecordToList( pString );
		}
		else if ( gMercProfiles[iCurrentIMPSlot].bDisability == AFRAID_OF_HEIGHTS )
		{
			CopyEmailText(pString, gzIMPDisabilityTraitEmailTextAfraidOfHeights[0]);

			AddEmailRecordToList( pString );

			CopyEmailText(pString, gzIMPDisabilityTraitEmailTextAfraidOfHeights[1]);

			AddEmailRecordToList( pString );
		}
		else if ( gMercProfiles[iCurrentIMPSlot].bDisability == SELF_HARM )
		{
			CopyEmailText(pString, gzIMPDisabilityTraitEmailTextSelfHarm[0]);

			AddEmailRecordToList( pString );

			CopyEmailText(pString, gzIMPDisabilityTraitEmailTextSelfHarm[1]);

			AddEmailRecordToList( pString );
		}
		else
		{
			// personality tick
			//	DEF: removed 1/12/99, cause it was changing the length of email that were already calculated
			//LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + Random( IMP_PERSONALITY_LENGTH - 1 ) + 1 ), MAIL_STRING_SIZE );
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + 1 ), MAIL_STRING_SIZE );
			// add to list
			AddEmailRecordToList( pString );

			// personality paragraph
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + IMP_PERSONALITY_LENGTH ), MAIL_STRING_SIZE );
			// add to list
			AddEmailRecordToList( pString );

			// extra paragraph for bugs
			if( gMercProfiles[ iCurrentIMPSlot ].bDisability == FEAR_OF_INSECTS )
			{
				// personality paragraph
				LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + IMP_PERSONALITY_LENGTH + 1 ), MAIL_STRING_SIZE );
				// add to list
				AddEmailRecordToList( pString );
			}
		}

		// attitude intro
		// now the personality intro
		iOffSet = IMP_RESULTS_ATTITUDE_INTRO;
		iEndOfSection = IMP_RESULTS_ATTITUDE_LENGTH;
		iCounter = 0;

		while(iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}
		
		// WANNE: Old trait system: attitudes
		if (!gGameOptions.fNewTraitSystem)
		{
			// personality itself
			switch( gMercProfiles[ iCurrentIMPSlot ].bAttitude )
			{
				// normal as can be
				case( ATT_NORMAL ):
					iOffSet = IMP_ATTITUDE_NORMAL;
					break;
				case( ATT_FRIENDLY ):
					iOffSet = IMP_ATTITUDE_FRIENDLY;
					break;
				case( ATT_LONER ):
					iOffSet = IMP_ATTITUDE_LONER;
					break;
				case( ATT_OPTIMIST ):
					iOffSet = IMP_ATTITUDE_OPTIMIST;
					break;
				case( ATT_PESSIMIST ):
					iOffSet = IMP_ATTITUDE_PESSIMIST;
					break;
				case( ATT_AGGRESSIVE ):
					iOffSet = IMP_ATTITUDE_AGGRESSIVE;
					break;
				case( ATT_ARROGANT ):
					iOffSet = IMP_ATTITUDE_ARROGANT;
					break;
				case( ATT_ASSHOLE ):
					iOffSet = IMP_ATTITUDE_ASSHOLE;
					break;
				case( ATT_COWARD ):
					iOffSet = IMP_ATTITUDE_COWARD;
					break;
			}

			// attitude title
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet ), MAIL_STRING_SIZE );
		}
		// WANNE: New trait system: Character traits
		else
		{
			// WANNE: Try to map "new character traits" to matching old attitudes, so we have meaningful text in IMP mail
			switch( gMercProfiles[ iCurrentIMPSlot ].bCharacterTrait )
			{
				case (CHAR_TRAIT_NORMAL):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_NORMAL]);
					iOffSet = IMP_ATTITUDE_NORMAL;
					break;
				case (CHAR_TRAIT_SOCIABLE):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_SOCIABLE]);
					iOffSet = IMP_ATTITUDE_FRIENDLY;
					break;
				case (CHAR_TRAIT_LONER):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_LONER]);
					iOffSet = IMP_ATTITUDE_LONER;
					break;
				case (CHAR_TRAIT_OPTIMIST):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_OPTIMIST]);
					iOffSet = IMP_ATTITUDE_OPTIMIST;
					break;
				case (CHAR_TRAIT_ASSERTIVE):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_ASSERTIVE]);
					iOffSet = IMP_ATTITUDE_OPTIMIST;
					break;
				case (CHAR_TRAIT_INTELLECTUAL):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_INTELLECTUAL]);
					iOffSet = IMP_ATTITUDE_FRIENDLY;
					break;
				case (CHAR_TRAIT_PRIMITIVE):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_PRIMITIVE]);
					iOffSet = IMP_ATTITUDE_ARROGANT;
					break;
				case (CHAR_TRAIT_AGGRESSIVE):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_AGGRESSIVE]);
					iOffSet = IMP_ATTITUDE_AGGRESSIVE;
					break;
				case (CHAR_TRAIT_PHLEGMATIC):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_PHLEGMATIC]);
					iOffSet = IMP_ATTITUDE_PESSIMIST;
					break;
				case (CHAR_TRAIT_DAUNTLESS):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_DAUNTLESS]);
					iOffSet = IMP_ATTITUDE_AGGRESSIVE;
					break;
				case (CHAR_TRAIT_PACIFIST):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_PACIFIST]);
					iOffSet = IMP_ATTITUDE_COWARD;
					break;
				case (CHAR_TRAIT_MALICIOUS):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_MALICIOUS]);
					iOffSet = IMP_ATTITUDE_ASSHOLE;
					break;
				case (CHAR_TRAIT_SHOWOFF):
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_SHOWOFF]);
					iOffSet = IMP_ATTITUDE_ARROGANT;
					break;
				case CHAR_TRAIT_COWARD:
					CopyEmailText(pString, gzIMPCharacterTraitText[CHAR_TRAIT_COWARD]);
					iOffSet = IMP_ATTITUDE_COWARD;
					break;
			}

			if (!AppendEmailText(pString, L". ±"))
				gEmailRecordBuildFailed = true;
		}
		
		// add to list
		AddEmailRecordToList( pString );

		// attitude tick
		//DEF: removed 1/12/99, cause it was changing the length of email that were already calculated
		//LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + Random( IMP_ATTITUDE_LENGTH - 2 ) + 1 ), MAIL_STRING_SIZE );
		LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + 1 ), MAIL_STRING_SIZE );
		// add to list
		AddEmailRecordToList( pString );

		// attitude paragraph
		LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + IMP_ATTITUDE_LENGTH - 1 ), MAIL_STRING_SIZE );
		// add to list
		AddEmailRecordToList( pString );

		//check for second paragraph
		if( iOffSet != IMP_ATTITUDE_NORMAL )
		{
			// attitude paragraph
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + IMP_ATTITUDE_LENGTH ), MAIL_STRING_SIZE );
			// add to list
			AddEmailRecordToList( pString );
		}

		// skills
		// now the skills intro
		iOffSet = IMP_RESULTS_SKILLS;
		iEndOfSection = IMP_RESULTS_SKILLS_LENGTH;
		iCounter = 0;

		while(iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}

		// imperial skills
		iOffSet = IMP_SKILLS_IMPERIAL_SKILLS;
		iEndOfSection = 0;
		iCounter = 0;

		// marksmanship
		if ( gMercProfiles[ iCurrentIMPSlot ].bMarksmanship >= SUPER_SKILL_VALUE )
		{
			fSufficientMarkSkill = TRUE;
			iEndOfSection = 1;
		}

		// medical
		if ( gMercProfiles[ iCurrentIMPSlot ].bMedical >= SUPER_SKILL_VALUE )
		{
			fSufficientMedSkill = TRUE;
			iEndOfSection = 1;
		}

		// mechanical
		if ( gMercProfiles[ iCurrentIMPSlot ].bMechanical >= SUPER_SKILL_VALUE )
		{
			fSufficientMechSkill = TRUE;
			iEndOfSection = 1;
		}

		if ( gMercProfiles[ iCurrentIMPSlot ].bExplosive >= SUPER_SKILL_VALUE )
		{
			fSufficientExplSkill = TRUE;
			iEndOfSection = 1;
		}

		while (iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}

		// now handle skills
		if ( fSufficientMarkSkill )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_IMPERIAL_MARK	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if ( fSufficientMedSkill )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_IMPERIAL_MED	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if ( fSufficientMechSkill )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_IMPERIAL_MECH	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		// explosives
		if ( fSufficientExplSkill )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_IMPERIAL_EXPL	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		fSufficientMechSkill = FALSE;
		fSufficientMarkSkill = FALSE;
		fSufficientExplSkill = FALSE;
		fSufficientMedSkill = FALSE;

		// imperial skills
		iOffSet = IMP_SKILLS_NEED_TRAIN_SKILLS;
		iEndOfSection = 0;
		iCounter = 0;

		// now the needs training values
		if( ( gMercProfiles[ iCurrentIMPSlot ].bMarksmanship > NO_CHANCE_IN_HELL_SKILL_VALUE ) &&( gMercProfiles[ iCurrentIMPSlot ].bMarksmanship <= NEEDS_TRAINING_SKILL_VALUE ) )
		{
			fSufficientMarkSkill = TRUE;
			iEndOfSection = 1;
		}

		if( ( gMercProfiles[ iCurrentIMPSlot ].bMedical > NO_CHANCE_IN_HELL_SKILL_VALUE ) &&( gMercProfiles[ iCurrentIMPSlot ].bMedical <= NEEDS_TRAINING_SKILL_VALUE ) )
		{
			fSufficientMedSkill = TRUE;
			iEndOfSection = 1;
		}

		if( ( gMercProfiles[ iCurrentIMPSlot ].bMechanical > NO_CHANCE_IN_HELL_SKILL_VALUE ) &&( gMercProfiles[ iCurrentIMPSlot ].bMechanical <= NEEDS_TRAINING_SKILL_VALUE ) )
		{
			fSufficientMechSkill = TRUE;
			iEndOfSection = 1;
		}

		if( ( gMercProfiles[ iCurrentIMPSlot ].bExplosive > NO_CHANCE_IN_HELL_SKILL_VALUE ) &&( gMercProfiles[ iCurrentIMPSlot ].bExplosive <= NEEDS_TRAINING_SKILL_VALUE ) )
		{
			fSufficientExplSkill = TRUE;
			iEndOfSection = 1;
		}

		while(iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}

		if( fSufficientMarkSkill )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_NEED_TRAIN_MARK	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientMedSkill )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_NEED_TRAIN_MED	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientMechSkill )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_NEED_TRAIN_MECH ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientExplSkill )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_NEED_TRAIN_EXPL ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		fSufficientMechSkill = FALSE;
		fSufficientMarkSkill = FALSE;
		fSufficientExplSkill = FALSE;
		fSufficientMedSkill = FALSE;

		// and the no chance in hell of doing anything useful values

		// no skill
		iOffSet = IMP_SKILLS_NO_SKILL;
		iEndOfSection = 0;
		iCounter = 0;

		if( gMercProfiles[ iCurrentIMPSlot ].bMarksmanship <= NO_CHANCE_IN_HELL_SKILL_VALUE )
		{
			fSufficientMarkSkill = TRUE;
			iEndOfSection = 1;
		}

		if( gMercProfiles[ iCurrentIMPSlot ].bMedical <= NO_CHANCE_IN_HELL_SKILL_VALUE )
		{
			fSufficientMedSkill = TRUE;
			iEndOfSection = 1;
		}

		if( gMercProfiles[ iCurrentIMPSlot ].bMechanical <= NO_CHANCE_IN_HELL_SKILL_VALUE )
		{
			fSufficientMechSkill = TRUE;
			iEndOfSection = 1;
		}

		if( gMercProfiles[ iCurrentIMPSlot ].bExplosive <= NO_CHANCE_IN_HELL_SKILL_VALUE )
		{
			fSufficientExplSkill = TRUE;
			iEndOfSection = 1;
		}

		while(iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}

		if( fSufficientMechSkill )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_NO_SKILL_MECH ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientMarkSkill )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_NO_SKILL_MARK ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientMedSkill )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_NO_SKILL_MED ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}
		if( fSufficientExplSkill )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_NO_SKILL_EXPL ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		// now the specialized skills
		// imperial skills
		iOffSet = IMP_SKILLS_SPECIAL_INTRO;
		iEndOfSection = IMP_SKILLS_SPECIAL_INTRO_LENGTH;
		iCounter = 0;

		while(iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}

	///////////////////////////////////////////////////////////////////////////////
	// SANDRO - switch for old/new traits
	if ( gGameOptions.fNewTraitSystem )
	{
		// Auto Weapons
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, AUTO_WEAPONS_NT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_AUTO ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Heavy Weapons
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, HEAVY_WEAPONS_NT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_HEAVY ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Sniper
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, SNIPER_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[0]);
			AddEmailRecordToList( pString );
		}
		// Ranger
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, RANGER_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[2]);
			AddEmailRecordToList( pString );
		}
		// Gunslinger
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, GUNSLINGER_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[3]);
			AddEmailRecordToList( pString );
		}
		// Martial Artist
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, MARTIAL_ARTS_NT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_MARTIAL ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Squadleader
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, SQUADLEADER_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[4]);
			AddEmailRecordToList( pString );
		}
		// Technician
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, TECHNICIAN_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[5]);
			AddEmailRecordToList( pString );
		}
		// Doctor
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, DOCTOR_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[6]);
			AddEmailRecordToList( pString );
		}
		// Ambidextrous
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, AMBIDEXTROUS_NT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_AMBI ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Melee
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, MELEE_NT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_KNIFE ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Throwing
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, THROWING_NT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_THROW ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Night Ops
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, NIGHT_OPS_NT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_NIGHT ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Stealthy
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, STEALTHY_NT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_STEALTH ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Athletics
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, ATHLETICS_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[7]);
			AddEmailRecordToList( pString );
		}
		// Bodybuilding
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, BODYBUILDING_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[8]);
			AddEmailRecordToList( pString );
		}
		// Demolitions
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, DEMOLITIONS_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[9]);
			AddEmailRecordToList( pString );
		}
		// Teaching
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, TEACHING_NT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_TEACH ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Scouting
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, SCOUTING_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[10]);
			AddEmailRecordToList( pString );
		}
		// Covert ops
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, COVERT_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[11]);
			AddEmailRecordToList( pString );
		}
		// Radio Operator
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, RADIO_OPERATOR_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[12]);
			AddEmailRecordToList( pString );
		}
		// Survival
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, SURVIVAL_NT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[13]);
			AddEmailRecordToList( pString );
		}
	}
	else
	{
		// Lockpick
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, LOCKPICKING_OT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_LOCK ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Hand to Hand
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, HANDTOHAND_OT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_HAND ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Electronics
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, ELECTRONICS_OT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_ELEC ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Night Ops
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, NIGHTOPS_OT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_NIGHT ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Throwing
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, THROWING_OT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_THROW ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Teaching
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, TEACHING_OT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_TEACH ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Heavy Weapons
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, HEAVY_WEAPS_OT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_HEAVY ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Auto Weapons
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, AUTO_WEAPS_OT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_AUTO ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Stealthy
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, STEALTHY_OT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_STEALTH ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Ambidextrous
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, AMBIDEXT_OT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_AMBI ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Thief
		/*if( ( gMercProfiles[ iCurrentIMPSlot ].bSkillTrait == THIEF_OT )||( gMercProfiles[ iCurrentIMPSlot ].bSkillTrait2 == THIEF_OT ) )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_THIEF ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}*/
		// Martial Arts
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, MARTIALARTS_OT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_MARTIAL ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Knifing
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, KNIFING_OT ) > 0 )
		{
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_SKILLS_SPECIAL_KNIFE ), MAIL_STRING_SIZE );
			AddEmailRecordToList( pString );
		}
		// Sniper
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, PROF_SNIPER_OT ) > 0 )
		{
			CopyEmailText(pString, MissingIMPSkillsDescriptions[0]);
			// add to list
			AddEmailRecordToList( pString );
		}
		// Camouflage
		if ( ProfileHasSkillTrait( iCurrentIMPSlot, CAMOUFLAGED_OT ) > 0 )
		{
			if ( gGameExternalOptions.fShowCamouflageFaces == TRUE )
			{
				gCamoFace[iCurrentIMPSlot].gCamoface = TRUE;
				gCamoFace[iCurrentIMPSlot].gUrbanCamoface = FALSE;
				gCamoFace[iCurrentIMPSlot].gDesertCamoface = FALSE;
				gCamoFace[iCurrentIMPSlot].gSnowCamoface = FALSE;
			}	
			CopyEmailText(pString, MissingIMPSkillsDescriptions[1]);
			AddEmailRecordToList( pString );
		}
	}

	///////////////////////////////////////////////////////////////////////////////

	// now the physical
	// imperial physical
	iOffSet = IMP_RESULTS_PHYSICAL;
	iEndOfSection = IMP_RESULTS_PHYSICAL_LENGTH;
	iCounter = 0;

	while(iEndOfSection > iCounter)
	{
		// read one record from email file
		LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

		// add to list
		AddEmailRecordToList( pString );

		// increment email record counter
		iCounter++;
	}

	// super physical
	iOffSet = IMP_PHYSICAL_SUPER;
	iEndOfSection = 0;
	iCounter = 0;

	// health
	if(	gMercProfiles[ iCurrentIMPSlot ].bLife >= SUPER_STAT_VALUE )
	{
		fSufficientHlth = TRUE;
		iEndOfSection = 1;
	}

	// dex
	if( gMercProfiles[ iCurrentIMPSlot ].bDexterity >= SUPER_STAT_VALUE )
	{
		fSufficientDex = TRUE;
		iEndOfSection = 1;
	}

	// agility
	if( gMercProfiles[ iCurrentIMPSlot ].bAgility >= SUPER_STAT_VALUE )
	{
		fSufficientAgi	= TRUE;
		iEndOfSection = 1;
	}

	// strength
	if( gMercProfiles[ iCurrentIMPSlot ].bStrength >= SUPER_STAT_VALUE )
	{
		fSufficientStr = TRUE;
		iEndOfSection = 1;
	}

	// wisdom
	if( gMercProfiles[ iCurrentIMPSlot ].bWisdom >= SUPER_STAT_VALUE )
	{
		fSufficientWis = TRUE;
		iEndOfSection =1;
	}

	// leadership
	if( gMercProfiles[ iCurrentIMPSlot ].bLeadership >= SUPER_STAT_VALUE )
	{
		fSufficientLdr = TRUE;
		iEndOfSection = 1;
	}

	while(iEndOfSection > iCounter)
	{
		// read one record from email file
		LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

		// add to list
		AddEmailRecordToList( pString );

		// increment email record counter
		iCounter++;
	}

	if( fSufficientHlth )
	{
		// read one record from email file
		LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_SUPER_HEALTH	), MAIL_STRING_SIZE );

		// add to list
		AddEmailRecordToList( pString );
	}


	if( fSufficientDex )
	{
		// read one record from email file
		LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_SUPER_DEXTERITY	), MAIL_STRING_SIZE );

		// add to list
		AddEmailRecordToList( pString );
	}

	if( fSufficientStr )
	{
		// read one record from email file
		LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_SUPER_STRENGTH	), MAIL_STRING_SIZE );

		// add to list
		AddEmailRecordToList( pString );
	}

	if( fSufficientAgi )
	{
		// read one record from email file
		LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_SUPER_AGILITY	), MAIL_STRING_SIZE );

		// add to list
		AddEmailRecordToList( pString );
	}

	if( fSufficientWis )
	{
		// read one record from email file
		LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_SUPER_WISDOM	), MAIL_STRING_SIZE );

		// add to list
		AddEmailRecordToList( pString );
	}

	if( fSufficientLdr )
	{
		// read one record from email file
		LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_SUPER_LEADERSHIP	), MAIL_STRING_SIZE );

		// add to list
		AddEmailRecordToList( pString );
	}

	fSufficientHlth = FALSE;
	fSufficientStr = FALSE;
	fSufficientWis = FALSE;
	fSufficientAgi = FALSE;
	fSufficientDex = FALSE;
	fSufficientLdr = FALSE;

	// now the low attributes
	// super physical
	iOffSet = IMP_PHYSICAL_LOW;
	iEndOfSection = 0;
	iCounter = 0;

		// health
		if(	( gMercProfiles[ iCurrentIMPSlot ].bLife < NEEDS_TRAINING_STAT_VALUE ) &&( gMercProfiles[ iCurrentIMPSlot ].bLife > NO_CHANCE_IN_HELL_STAT_VALUE ) )
		{
			fSufficientHlth = TRUE;
			iEndOfSection = 1;
		}

		// strength
		if( (gMercProfiles[ iCurrentIMPSlot ].bStrength < NEEDS_TRAINING_STAT_VALUE )&&( gMercProfiles[ iCurrentIMPSlot ].bStrength > NO_CHANCE_IN_HELL_STAT_VALUE ) )
		{
			fSufficientStr = TRUE;
			iEndOfSection = 1;
		}

		// agility
		if( (gMercProfiles[ iCurrentIMPSlot ].bAgility < NEEDS_TRAINING_STAT_VALUE )&&( gMercProfiles[ iCurrentIMPSlot ].bAgility <= NO_CHANCE_IN_HELL_STAT_VALUE ) )
		{
			fSufficientAgi = TRUE;
			iEndOfSection = 1;
		}

		// wisdom
		if( (gMercProfiles[ iCurrentIMPSlot ].bWisdom < NEEDS_TRAINING_STAT_VALUE)&&( gMercProfiles[ iCurrentIMPSlot ].bWisdom > NO_CHANCE_IN_HELL_STAT_VALUE ) )
		{
			fSufficientWis = TRUE;
			iEndOfSection = 1;
		}

		// leadership
		if( (gMercProfiles[ iCurrentIMPSlot ].bLeadership < NEEDS_TRAINING_STAT_VALUE)&&( gMercProfiles[ iCurrentIMPSlot ].bLeadership > NO_CHANCE_IN_HELL_STAT_VALUE ) )
		{
			fSufficientLdr = TRUE;
			iEndOfSection = 1;
		}

		// dex
		if( (gMercProfiles[ iCurrentIMPSlot ].bDexterity < NEEDS_TRAINING_STAT_VALUE )&&( gMercProfiles[ iCurrentIMPSlot ].bDexterity > NO_CHANCE_IN_HELL_STAT_VALUE ) )
		{
			fSufficientDex = TRUE;
			iEndOfSection = 1;
		}

		while(iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}

		if( fSufficientHlth )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_LOW_HEALTH	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientDex )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_LOW_DEXTERITY	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientStr )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_LOW_STRENGTH	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientAgi )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_LOW_AGILITY	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientWis )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_LOW_WISDOM	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientLdr )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_LOW_LEADERSHIP	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		// very low physical
		iOffSet = IMP_PHYSICAL_VERY_LOW;
		iEndOfSection = 0;
		iCounter = 0;

		fSufficientHlth = FALSE;
		fSufficientStr = FALSE;
		fSufficientWis = FALSE;
		fSufficientAgi = FALSE;
		fSufficientDex = FALSE;
		fSufficientLdr = FALSE;

		// health
		if(	gMercProfiles[ iCurrentIMPSlot ].bLife <= NO_CHANCE_IN_HELL_STAT_VALUE )
		{
			fSufficientHlth = TRUE;
			iEndOfSection =1;
		}

		// dex
		if( gMercProfiles[ iCurrentIMPSlot ].bDexterity <= NO_CHANCE_IN_HELL_STAT_VALUE )
		{
			fSufficientDex = TRUE;
			iEndOfSection =1;
		}

		// strength
		if( gMercProfiles[ iCurrentIMPSlot ].bStrength <= NO_CHANCE_IN_HELL_STAT_VALUE )
		{
			fSufficientStr = TRUE;
			iEndOfSection = 1;
		}

		// agility
		if( gMercProfiles[ iCurrentIMPSlot ].bAgility <= NO_CHANCE_IN_HELL_STAT_VALUE )
		{
			fSufficientAgi = TRUE;
			iEndOfSection = 1;
		}

		// wisdom
		if( gMercProfiles[ iCurrentIMPSlot ].bWisdom <= NO_CHANCE_IN_HELL_STAT_VALUE )
		{
			fSufficientWis = TRUE;
			iEndOfSection =1;
		}

		while(iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}

		if( fSufficientHlth )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_VERY_LOW_HEALTH	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientDex )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_VERY_LOW_DEXTERITY	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientStr )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_VERY_LOW_STRENGTH	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientAgi )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_VERY_LOW_AGILITY	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		if( fSufficientWis )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_VERY_LOW_WISDOM	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		// leadership
		if( gMercProfiles[ iCurrentIMPSlot ].bLeadership <= NO_CHANCE_IN_HELL_STAT_VALUE )
		{
			fSufficientLdr = TRUE;
		}

		if( fSufficientLdr )
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( IMP_PHYSICAL_VERY_LOW_LEADERSHIP	), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );
		}

		// very low physical
		iOffSet = IMP_RESULTS_PORTRAIT;
		iEndOfSection = IMP_RESULTS_PORTRAIT_LENGTH;
		iCounter = 0;

		while(iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}

		// portraits
		switch( iPortraitNumber )
		{
			case( 0 ):
				if( fCharacterIsMale )
					iOffSet = IMP_PORTRAIT_MALE_1;
				else
					iOffSet = IMP_PORTRAIT_FEMALE_1;
				break;
			case( 1 ):
				if( fCharacterIsMale )
					iOffSet = IMP_PORTRAIT_MALE_2;
				else
					iOffSet = IMP_PORTRAIT_FEMALE_2;
				break;
			case( 2 ):
				if( fCharacterIsMale )
					iOffSet = IMP_PORTRAIT_MALE_3;
				else
					iOffSet = IMP_PORTRAIT_FEMALE_3;
				break;
			case( 3 ):
				if( fCharacterIsMale )
					iOffSet = IMP_PORTRAIT_MALE_4;
				else
					iOffSet = IMP_PORTRAIT_FEMALE_4;
				break;
			case( 4 ):
				if( fCharacterIsMale )
					iOffSet = IMP_PORTRAIT_MALE_5;
				else
					iOffSet = IMP_PORTRAIT_FEMALE_4;
				break;
			case( 5 ):
				if( fCharacterIsMale )
					iOffSet = IMP_PORTRAIT_MALE_5;
				else
					iOffSet = IMP_PORTRAIT_FEMALE_5;
				break;
			case( 6 ):
				if( fCharacterIsMale )
					iOffSet = IMP_PORTRAIT_MALE_6;
				else
					iOffSet = IMP_PORTRAIT_FEMALE_5;
				break;
			case( 7 ):
				if( fCharacterIsMale )
					iOffSet = IMP_PORTRAIT_MALE_6;
				else
					iOffSet = IMP_PORTRAIT_FEMALE_3;
				break;
			case( 8 ):
				if( fCharacterIsMale )
					iOffSet = IMP_PORTRAIT_MALE_4;
				else
					iOffSet = IMP_PORTRAIT_FEMALE_4;
				break;
			case( 9 ):
				if( fCharacterIsMale )
					iOffSet = IMP_PORTRAIT_MALE_5;
				else
					iOffSet = IMP_PORTRAIT_FEMALE_5;
				break;
			default:
				if( fCharacterIsMale )
					iOffSet = IMP_PORTRAIT_MALE_1;
				else
					iOffSet = IMP_PORTRAIT_FEMALE_1;
				break;
		}

		if( ( iRand % 2 ) == 0 )
		{
			iOffSet += 2;
		}

		iEndOfSection = 2;
		iCounter = 0;

		while(iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}

		iOffSet = IMP_RESULTS_END;
		iEndOfSection = IMP_RESULTS_END_LENGTH;
		iCounter = 0;

		while(iEndOfSection > iCounter)
		{
			// read one record from email file
			LoadEncryptedDataFromFile( "BINARYDATA\\Impass.edt", pString, MAIL_STRING_SIZE * ( iOffSet + iCounter ), MAIL_STRING_SIZE );

			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			iCounter++;
		}

		giPrevMessageId = giMessageId;

	}

}

void HandleEmailViewerButtonStates( void )
{
	// handle state of email viewer buttons

	if (fDisplayMessageFlag == FALSE || gEmailMessageResources.empty())
	{
		// not displaying message, leave
		return;
	}



	if(	giNumberOfPagesToCurrentEmail <= 2 )
	{
		return;
	}

	// turn off previous page button
	if( giMessagePage == 0 )
	{
		DisableButton( giMailMessageButtons[ 0 ] );
	}
	else
	{
		EnableButton( giMailMessageButtons[ 0 ] );
	}


	// turn off next page button
	if (!LaptopEmailListModel::HasNextBodyPage(
			static_cast<std::size_t>(std::max(giMessagePage, 0)),
			static_cast<std::size_t>(
				std::max(giNumberOfPagesToCurrentEmail, 0)),
			MAX_NUMBER_EMAIL_PAGES))
	{
		DisableButton( giMailMessageButtons[ 1 ] );
	}
	else
	{
		EnableButton( giMailMessageButtons[ 1 ] );
	}

	return;

}


static BOOLEAN CreateNextPreviousEmailPageButtons(
	LaptopPageResourceOwner& owner)
{

	// this function will create the buttons to advance and go back email pages

	// next button
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\NewMailButtons.sti", -1, 1, -1, 4, -1),
		giMailPageButtonsImage[0])) return FALSE;
	if (!owner.addButton(QuickCreateButton(giMailPageButtonsImage[0],
		NEXT_PAGE_X, NEXT_PAGE_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
										(GUI_CALLBACK)NextRegionButtonCallback),
		giMailPageButtons[0])) return FALSE;
	SetButtonCursor(giMailPageButtons[0], CURSOR_LAPTOP_SCREEN);

	// previous button
	if (!owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\NewMailButtons.sti", -1, 0, -1, 3, -1),
		giMailPageButtonsImage[1])) return FALSE;
	if (!owner.addButton(QuickCreateButton(giMailPageButtonsImage[1],
		PREVIOUS_PAGE_X, NEXT_PAGE_Y,
									BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
									(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback,
									(GUI_CALLBACK)PreviousRegionButtonCallback),
		giMailPageButtons[1])) return FALSE;
	SetButtonCursor(giMailPageButtons[1], CURSOR_LAPTOP_SCREEN);

	/*
	// set up disable methods
	SpecifyDisabledButtonStyle( giMailPageButtons[1], DISABLED_STYLE_SHADED );
	SpecifyDisabledButtonStyle( giMailPageButtons[0], DISABLED_STYLE_SHADED );
*/

	return TRUE;
}


void UpdateStatusOfNextPreviousButtons( void )
{
	if (gEmailPageResources.empty()) return;

	// set the states of the page advance buttons

	DisableButton( giMailPageButtons[ 0 ]);
	DisableButton( giMailPageButtons[ 1 ]);

	if( iCurrentPage > 0 )
	{
		EnableButton( giMailPageButtons[ 1 ]);
	}

	if( iCurrentPage < iLastPage )
	{
		EnableButton( giMailPageButtons[ 0 ] );
	}
}


void DisplayWhichPageOfEmailProgramIsDisplayed( void )
{
	// will draw the number of the email program we are viewing right now
	CHAR16 sString[ 10 ];

	// font stuff
	SetFont(MESSAGE_FONT);
	SetFontForeground(FONT_BLACK);
	SetFontBackground(FONT_BLACK);
	SetFontShadow(NO_SHADOW);

	// page number
	if( iLastPage < 0 )
		sgp_swprintf(sString, 10, L"%d / %d", 1, 1);
	else
		sgp_swprintf(sString, 10, L"%d / %d",
			iCurrentPage + 1, iLastPage + 1);

	// print it
	mprintf( PAGE_NUMBER_X ,PAGE_NUMBER_Y, L"%s", sString );

	// restore shadow
	SetFontShadow( DEFAULT_SHADOW );

	return;
}

void OpenMostRecentUnreadEmail( void )
{
	// will open the most recent email the player has recieved and not read
	INT32 iMostRecentMailId = -1;
	EmailPtr pB=pEmailList;
	UINT32 mostRecentDate = 0;
	bool foundUnread = false;

	while( pB )
	{
		if (pB->fRead == FALSE && LaptopEmailListModel::IsMoreRecent(
			pB->iDate, pB->iId, foundUnread, mostRecentDate,
			iMostRecentMailId))
		{
			iMostRecentMailId = pB->iId;
			mostRecentDate = pB->iDate;
			foundUnread = true;
		}

		// next in B's list
	pB=pB->Next;
	}

	// set up id
	giMessageId = iMostRecentMailId;

	// valid message, show it
	if( giMessageId != -1 )
	{
		fDisplayMessageFlag = TRUE;
	}

	return;
}


BOOLEAN DisplayNumberOfPagesToThisEmail( INT32 iViewerY )
{
	// display the indent for the display of pages to this email..along with the current page/number of pages
	INT16 sX = 0, sY = 0;
	CHAR16 sString[ 32 ];


	// parse current page and max number of pages to email
	const auto bodyPageCount = LaptopEmailListModel::BodyPageCount(
		static_cast<std::size_t>(
			std::max(giNumberOfPagesToCurrentEmail, 0)),
		MAX_NUMBER_EMAIL_PAGES);
	sgp_swprintf(sString, 32, L"%d / %d", giMessagePage + 1,
		static_cast<INT32>(std::max<std::size_t>(bodyPageCount, 1)));

	SetFont( FONT12ARIAL );
	SetFontForeground( FONT_BLACK );
	SetFontBackground( FONT_BLACK );

	// turn off the shadows
	SetFontShadow(NO_SHADOW);

	SetFontDestBuffer(FRAME_BUFFER, 0 , 0 , SCREEN_WIDTH, SCREEN_HEIGHT,	FALSE	);

	FindFontCenterCoordinates(VIEWER_X + INDENT_X_OFFSET, 0,INDENT_X_WIDTH, 0, sString, FONT12ARIAL, &sX, &sY);
	mprintf( sX, VIEWER_Y + iViewerY + INDENT_Y_OFFSET - 2, L"%s", sString );


	// restore shadows
	SetFontShadow( DEFAULT_SHADOW );

	return ( TRUE );
}


INT32 GetNumberOfPagesToEmail( )
{
	RecordPtr pTempRecord;
	INT32 iNumberOfPagesToEmail = 0;


	// set temp record to head of list
	pTempRecord=pMessageRecordList;

	// run through messages, and find out how many
	while (pTempRecord &&
		iNumberOfPagesToEmail < MAX_NUMBER_EMAIL_PAGES)
	{
		pTempRecord = GetFirstRecordOnThisPage( pMessageRecordList, MESSAGE_FONT, MESSAGE_WIDTH, MESSAGE_GAP, iNumberOfPagesToEmail, MAX_EMAIL_MESSAGE_PAGE_SIZE );
		iNumberOfPagesToEmail++;
	}


	return( iNumberOfPagesToEmail );
}


void ShutDownEmailList()
{
	for (UINT32 cnt = 0; cnt < EMAIL_VAL; ++cnt)
	{
		gEmailT[cnt] = {};
	}
	FreeEmailNodes(pEmailList);
	pEmailList = NULL;
	ClearPages();
}

BOOLEAN ReplaceEmailListFromSavedGame(EmailPtr loadedEmailList)
{
	EmailPageListOwner stagedPages;
	if (!BuildEmailPages(
			loadedEmailList, nullptr, nullptr, stagedPages)) return FALSE;

	EmailPtr oldEmailList = pEmailList;
	pEmailList = loadedEmailList;
	CommitEmailPages(stagedPages, EmailMessageCount(loadedEmailList));
	FreeEmailNodes(oldEmailList);
	giMessageId = -1;
	giPrevMessageId = -1;
	giMessagePage = 0;
	ClearOutEmailMessageRecordsList();
	return TRUE;
}

// Pre Process the mail, when clicking on a mail in the mail list
void PreProcessEmail( EmailPtr pMail )
{
	if (!pMail) return;
	RecordPtr pTempRecord = nullptr;
	RecordPtr pCurrentRecord = nullptr;
	RecordPtr pLastRecord = nullptr;
	RecordPtr pTempList = nullptr;
	CHAR16 pString[MAIL_STRING_SIZE]{};
	INT32 iCounter = 0, iHeight = 0, iOffSet = 0;
	UINT16 recordsToLoad = pMail->usLength;
	BOOLEAN fGoingOffCurrentPage = FALSE;
	INT32 iYPositionOnPage = 0;

	iOffSet=(INT32)pMail->usOffset;
	const bool isImpProfileResults = CurrentCommunicationsPolicy()
		.isImpProfileResultsMessage(pMail->usOffset,
			pMail->EmailVersion == TYPE_EMAIL_EMAIL_EDT);

	// set record ptr to head of list
	pTempRecord=pMessageRecordList;

	if( pEmailPageInfo[ 0 ].pFirstRecord != NULL )
	{
		// already processed
		return;
	}
	if (gEmailRecordBuildFailed)
	{
		ClearOutEmailMessageRecordsList();
		return;
	}

	// WANNE: Get the text and replace name!
	int iNew113MERCMerc = 0;
	int iNew113AIMMerc = 0;

	int iEmailMERCMessage = 0;
	int iEmailAIMMessage = 0;
	
	if ( pMail->EmailVersion == TYPE_EMAIL_EMAIL_EDT )
	{	
		if (pMail->usLength == MERC_UP_LEVEL_GASTON || pMail->usLength == MERC_UP_LEVEL_STOGIE ||
			pMail->usLength == MERC_UP_LEVEL_TEX || pMail->usLength == MERC_UP_LEVEL_BIGGENS)
		{
			iNew113MERCMerc = pMail->usLength;
			recordsToLoad = 2;
		}
		else if (pMail->usLength >= 170 && pMail->usLength <= 177)
		{
			iNew113AIMMerc = pMail->usLength;
			recordsToLoad = 2;
		}
	}
	else if (pMail->EmailVersion == TYPE_EMAIL_EMAIL_EDT_NAME_MERC &&
		pMail->usLength >= 170)
	{
		iNew113AIMMerc = pMail->usLength;
		recordsToLoad = 2;
	}
	else if ( pMail->EmailVersion == TYPE_EMAIL_MERC_LEVEL_UP )
	{
		iEmailMERCMessage = pMail->usLength;
		recordsToLoad = 2;
	}	
	else if ( pMail->EmailVersion == TYPE_EMAIL_BOBBY_R )
	{
		recordsToLoad = 4;
	}	
	else if ( pMail->EmailVersion == TYPE_EMAIL_AIM_AVAILABLE )
	{
		iEmailAIMMessage = pMail->usLength;
		recordsToLoad = 2;
	}	

	// list doesn't exist, reload
	if( !pTempRecord )
	{
		while(recordsToLoad > iCounter)
		{
			pString[0] = L'\0';
			if (!LoadEmailRecord(
					pMail->EmailVersion, iOffSet + iCounter, pString) &&
				pMail->EmailVersion == TYPE_EMAIL_XML)
			{
				if ( pMail->usOffset < gEmails.size() && (size_t)iCounter < gEmails[pMail->usOffset].Messages.size() )
					CopyEmailText(pString,
						gEmails[pMail->usOffset].Messages[iCounter].c_str());
			}

			// ----------------
			// New MERC Merc
			// ----------------
			// WANNE: We have a new 1.13 MERC merc (Text, Gaston, Stogie or Biggens)
			if (iNew113MERCMerc != 0 )
			{
				// WANNE: TODO: Replace "Biff" with the name of the 1.13 merc
				if (iCounter == 1 && pMail->EmailVersion == TYPE_EMAIL_EMAIL_EDT )
				{
					pString[0] = L'\0';
					if (iNew113MERCMerc == MERC_UP_LEVEL_GASTON)
					{
						CopyEmailText(pString, New113MERCMercMailTexts[0]);
					}
					else if (iNew113MERCMerc == MERC_UP_LEVEL_STOGIE)
					{
						CopyEmailText(pString, New113MERCMercMailTexts[1]);
					}
					else if (iNew113MERCMerc == MERC_UP_LEVEL_TEX)
					{
						CopyEmailText(pString, New113MERCMercMailTexts[2]);
					}
					else if (iNew113MERCMerc == MERC_UP_LEVEL_BIGGENS)
					{
						CopyEmailText(pString, New113MERCMercMailTexts[3]);
					}
				}
			}

			// ----------------
			// New AIM Merc
			// ----------------
			// WANNE: We have a new 1.13 AIM Wildfire merc
			if (iNew113AIMMerc != 0)
			{				
				pString[0] = L'\0';

				// Only output the mail text, not the subject, cause we already have the subject as text
				if (iCounter == 1 &&
					(pMail->EmailVersion == TYPE_EMAIL_EMAIL_EDT ||
					 pMail->EmailVersion == TYPE_EMAIL_EMAIL_EDT_NAME_MERC))
				{
					if (iNew113AIMMerc == 170)
					{
						CopyEmailText(pString, New113AIMMercMailTexts[1]);
					}
					else if (iNew113AIMMerc == 171)
					{
						CopyEmailText(pString, New113AIMMercMailTexts[3]);
					}
					else if (iNew113AIMMerc == 172)
					{
						CopyEmailText(pString, New113AIMMercMailTexts[5]);
					}
					else if (iNew113AIMMerc == 173)
					{
						CopyEmailText(pString, New113AIMMercMailTexts[7]);
					}
					else if (iNew113AIMMerc == 174)
					{
						CopyEmailText(pString, New113AIMMercMailTexts[9]);
					}
					else if (iNew113AIMMerc == 175)
					{
						CopyEmailText(pString, New113AIMMercMailTexts[11]);
					}
					else if (iNew113AIMMerc == 176)
					{
						CopyEmailText(pString, New113AIMMercMailTexts[13]);
					}
					else if (iNew113AIMMerc == 177)
					{
						CopyEmailText(pString, New113AIMMercMailTexts[15]);
					}
					// Additional Generic Merc mail message
					else
					{
						CopyEmailText(pString, New113AIMMercMailTexts[17]);
					}
				}
			}

			if (iCounter == 1)
			{
				if ( pMail->EmailVersion == TYPE_EMAIL_AIM_AVAILABLE )
				{
					pString[0] = L'\0';
					if (LaptopEmailListModel::IsIndexInRange(
							iEmailAIMMessage, NUM_PROFILES))
						CopyEmailText(pString,
							EmailMercAvailableText[iEmailAIMMessage].szMessage);
				}
				else if ( pMail->EmailVersion == TYPE_EMAIL_MERC_LEVEL_UP)
				{
					pString[0] = L'\0';
					if (LaptopEmailListModel::IsIndexInRange(
							iEmailMERCMessage, NUM_PROFILES))
						CopyEmailText(pString,
							EmailMercLevelUpText[iEmailMERCMessage].szMessage);
				}
				/*
				else if ( pMail->EmailVersion == TYPE_EMAIL_BOBBY_R)
				{
					pString[0] = L'\0';
					CopyEmailText(pString, EmailBobbyRText[0]);
				}
				*/
			}
		
			// add to list
			AddEmailRecordToList( pString );

			// increment email record counter
			++iCounter;
		}

		if (gEmailRecordBuildFailed)
		{
			ClearOutEmailMessageRecordsList();
			return;
		}
		giPrevMessageId = giMessageId;
	}

	// set record ptr to head of list
	pTempRecord=pMessageRecordList;

//def removed
	// pass the subject line
	if (pTempRecord && !isImpProfileResults &&
		pMail->EmailVersion != TYPE_EMAIL_XML)
	{
		pTempRecord = pTempRecord->Next;
	}

	// get number of pages to this email
	giNumberOfPagesToCurrentEmail = GetNumberOfPagesToEmail( );
	
	while( pTempRecord )
	{
		// copy over string
		CopyEmailText(pString, pTempRecord->pRecord);

		// get the height of the string, ONLY!...must redisplay ON TOP OF background graphic
		iHeight += IanWrappedStringHeight(VIEWER_X + 9, ( UINT16 )( VIEWER_MESSAGE_BODY_START_Y + iHeight + GetFontHeight(MESSAGE_FONT)), MESSAGE_WIDTH, MESSAGE_GAP, MESSAGE_FONT, MESSAGE_COLOR,pString,0,FALSE,0);

		// next message record string
		pTempRecord = pTempRecord->Next;
	}

	iViewerPositionY = ( 261 - iHeight ) / 2;

	if ( iViewerPositionY < 0 )
	{
		iViewerPositionY = 0;
	}

	// set total height to height of records displayed
	iTotalHeight=iHeight;

	// if the message background is less than MIN_MESSAGE_HEIGHT_IN_LINES, set to that number
	if( ( iTotalHeight / GetFontHeight( MESSAGE_FONT ) ) < MIN_MESSAGE_HEIGHT_IN_LINES)
	{
		iTotalHeight=GetFontHeight( MESSAGE_FONT ) * MIN_MESSAGE_HEIGHT_IN_LINES;
	}

	if(iTotalHeight > MAX_EMAIL_MESSAGE_PAGE_SIZE)
	{
		// if message to big to fit on page
		iTotalHeight = MAX_EMAIL_MESSAGE_PAGE_SIZE + 10;
	}
	else
	{
		iTotalHeight += 10;
	}

	pTempRecord=pMessageRecordList;

	if( iTotalHeight < MAX_EMAIL_MESSAGE_PAGE_SIZE )
	{
		fOnLastPageFlag = TRUE;

		if (pTempRecord && !isImpProfileResults &&
			pMail->EmailVersion != TYPE_EMAIL_XML)
		{
			pTempRecord = pTempRecord->Next;
		}

/*
//Def removed
		if( pTempRecord )
		{
			pTempRecord = pTempRecord->Next;
		}
*/

		pEmailPageInfo[ 0 ].pFirstRecord = pTempRecord ;
		pEmailPageInfo[ 0 ].iPageNumber = 0;
		
		while( pTempRecord )
		{
			pCurrentRecord = pTempRecord;

			// increment email record ptr
			pTempRecord = pTempRecord->Next;
		}

		// only one record to this email?..then set next to null
		if( pCurrentRecord == pEmailPageInfo[ 0 ].pFirstRecord )
		{
			pCurrentRecord = NULL;
		}

		// set up the last record for the page
		pEmailPageInfo[ 0 ].pLastRecord = pCurrentRecord;

		// now set up the next page
		pEmailPageInfo[ 1 ].pFirstRecord = NULL;
		pEmailPageInfo[ 1 ].pLastRecord = NULL;
		pEmailPageInfo[ 1 ].iPageNumber = 1;
	}
	else
	{
		fOnLastPageFlag = FALSE;
		pTempList = pMessageRecordList;

		if (pTempList && !isImpProfileResults &&
			pMail->EmailVersion != TYPE_EMAIL_XML)
		{
			pTempList = pTempList->Next;
		}

/*
//def removed
		// skip the subject
		if( pTempList )
		{
			pTempList = pTempList->Next;
		}

*/
		iCounter = 0;

		// more than one page
		//for( iCounter = 0; iCounter < giNumberOfPagesToCurrentEmail; iCounter++ )
		while (iCounter < MAX_NUMBER_EMAIL_PAGES - 1 &&
			(pTempRecord = GetFirstRecordOnThisPage(pTempList,
				MESSAGE_FONT, MESSAGE_WIDTH, MESSAGE_GAP, iCounter,
				MAX_EMAIL_MESSAGE_PAGE_SIZE)) != NULL)
		{
			iYPositionOnPage = 0;

			pEmailPageInfo[ iCounter ].pFirstRecord = pTempRecord;
			pEmailPageInfo[ iCounter ].iPageNumber = iCounter;
			pLastRecord = NULL;

			// go to the right record
			while( pTempRecord )
			{
				// copy over string
				CopyEmailText(pString, pTempRecord->pRecord);

				if( pString[ 0 ] == 0 )
				{
					// on last page
					fOnLastPageFlag = TRUE;
				}

				if( ( iYPositionOnPage + IanWrappedStringHeight(0, 0, MESSAGE_WIDTH, MESSAGE_GAP,
																	MESSAGE_FONT, 0, pTempRecord->pRecord,
																0, 0, 0 ) )	<= MAX_EMAIL_MESSAGE_PAGE_SIZE	)
				{
	 			// now print it

					iYPositionOnPage += IanWrappedStringHeight(VIEWER_X + 9, ( UINT16 )( VIEWER_MESSAGE_BODY_START_Y + 10 +iYPositionOnPage + iViewerPositionY), MESSAGE_WIDTH, MESSAGE_GAP, MESSAGE_FONT, MESSAGE_COLOR,pString,0,FALSE, IAN_WRAP_NO_SHADOW);
					fGoingOffCurrentPage = FALSE;
				}
				else
				{
					// gonna get cut off...end now
					fGoingOffCurrentPage = TRUE;
				}
				
				pCurrentRecord = pTempRecord;
				pTempRecord = pTempRecord->Next;

				if( fGoingOffCurrentPage == FALSE )
				{
					pLastRecord = pTempRecord;
				}
				// record get cut off?...end now

				if( fGoingOffCurrentPage == TRUE )
				{
					// An indivisible record taller than a page still owns
					// exactly this page. Keep the following record as the
					// exclusive end marker instead of rendering the rest too.
					if (!pLastRecord && pCurrentRecord ==
						pEmailPageInfo[iCounter].pFirstRecord)
					{
						pLastRecord = pTempRecord;
					}
					pTempRecord = NULL;
				}
			}

			if( pLastRecord == pEmailPageInfo[ iCounter ].pFirstRecord )
			{
				pLastRecord = NULL;
			}

			pEmailPageInfo[ iCounter ].pLastRecord = pLastRecord;
			++iCounter;
		}

		pEmailPageInfo[ iCounter ].pFirstRecord = NULL;
		pEmailPageInfo[ iCounter ].pLastRecord = NULL;
		pEmailPageInfo[ iCounter ].iPageNumber = iCounter;
	}
}


void ModifyInsuranceEmails( UINT16 usMessageId, INT32 *iResults, EmailPtr pMail )
{
	RecordPtr pTempRecord;
//	CHAR16 pString[MAIL_STRING_SIZE/2 + 1];
	CHAR16 pString[MAIL_STRING_SIZE];
	UINT8	ubCnt;
	
	// Replace the name in the subject line
	// set record ptr to head of list
	pTempRecord=pMessageRecordList;

	// list doesn't exist, reload
	if( !pTempRecord )
	{
	        for ( ubCnt = 0; ubCnt < pMail->usLength; ubCnt++ )
		{
			pString[0] = L'\0';
			if (pMail->EmailVersion == TYPE_EMAIL_XML &&
				pMail->usOffset < gEmails.size() &&
				ubCnt < gEmails[pMail->usOffset].Messages.size())
			{
				CopyEmailText(pString,
					gEmails[pMail->usOffset].Messages[ubCnt].c_str());
			}
			else
			{
				LoadEmailRecord(pMail->EmailVersion, usMessageId, pString);
			}
			//Replace the $MERCNAME$ and $AMOUNT$ with the mercs name and the amountm if the string contains the keywords.
			ReplaceMercNameAndAmountWithProperData( pString, pMail );

			// add to list
			AddEmailRecordToList( pString );

			++usMessageId;
		}
	}

	giPrevMessageId = giMessageId;
}

BOOLEAN ReplaceMercNameAndAmountWithProperData(
	CHAR16 (&pFinishedString)[MAIL_STRING_SIZE], EmailPtr pMail)
{
	if (!pMail) return FALSE;
	std::wstring result(pFinishedString);
	const std::wstring mercName =
		LaptopEmailListModel::IsIndexInRange(
			pMail->uiSecondData, NUM_PROFILES)
		? gMercProfiles[pMail->uiSecondData].zName : L"";
	const std::wstring amount = FormatMoney(pMail->iFirstData);

	const auto replaceAll = [&result](const std::wstring& token,
		const std::wstring& replacement)
	{
		std::size_t position = 0;
		while ((position = result.find(token, position)) != std::wstring::npos)
		{
			if (result.size() - token.size() + replacement.size() >=
				MAIL_STRING_SIZE) return false;
			result.replace(position, token.size(), replacement);
			position += replacement.size();
		}
		return true;
	};

	if (!replaceAll(L"$MERCNAME$", mercName) ||
		!replaceAll(L"$AMOUN$", amount)) return FALSE;
	return CopyEmailText(pFinishedString, result.c_str()) ? TRUE : FALSE;
}

void AddAllEmails()
{	
	if (CurrentCommunicationsPolicy().usesUnfinishedBusinessCatalog()) return;

	const auto date = GetWorldTotalMin();
	const auto money = 1000000; // 1 MILLION DOLLARS!
	const auto mercID = 0;
	for ( size_t i = 0; i < gEmails.size(); i++ )
	{
		// Insurance emails
		if ( i >= XML_INSURANCE_APPROVED && i <= XML_INSURANCE_POLICYVIOLATION )
		{
			AddEmailWithSpecialDataXML(i, date, -1, -1, false, money,
				mercID, JA2_EMAIL_INSURANCE_PAYMENT, -1, -1, -1);
		}
		// BR Shipment arrival
		else if ( i == XML_BR_SHIPMENTARRIVAL )
		{
			AddEmailFromXML(i, date, -1, 1, false, -1, -1,
				JA2_EMAIL_BOBBYR_SHIPMENT_ARRIVED, -1, -1, -1);
		}
		// AIM Notification of death
		// Medical deposit refunds
		else if ( i >= XML_AIM_NOTICE_OF_DEATH && i <= XML_AIM_NOREFUND )
		{
			const auto money = 1000000; // 1 MILLION DOLLARS!
			const auto mercID = 0;
			AddEmailWithSpecialDataXML(i, date, -1, -1, false, money,
				mercID, JA2_EMAIL_MERC_DIED_OTHER_ASSIGNMENT, -1, -1, -1);
		}
		else
		{
			AddEmailFromXML(i, date, -1, -1, false, 0, 0, 0, 0, 0, 0);
		}
	}
}

BOOLEAN SaveNewEmailDataToSaveGameFile( HWFILE hFile )
{
	return WriteLaptopFileExact(hFile, &gEmailT, sizeof(gEmailT))
		? TRUE : FALSE;
}

BOOLEAN LoadNewEmailDataFromLoadGameFile( HWFILE hFile )
{
	const std::size_t emailCount = EmailMessageCount(pEmailList);
	if (emailCount > EMAIL_VAL) return FALSE;
	EMAIL_TYPE loadedEmailTypes[EMAIL_VAL]{};
	if (!ReadLaptopFileExact(
			hFile, &loadedEmailTypes, sizeof(loadedEmailTypes))) return FALSE;
	std::copy_n(loadedEmailTypes, EMAIL_VAL, gEmailT);
	UINT32 uiNumOfEmails = 0;
	for (EmailPtr pEmail = pEmailList; pEmail; pEmail = pEmail->Next)
	{
		pEmail->EmailVersion = gEmailT[uiNumOfEmails].EmailVersion;
		pEmail->EmailType = gEmailT[uiNumOfEmails].EmailType;
		++uiNumOfEmails;
	}

	return( TRUE );
}
