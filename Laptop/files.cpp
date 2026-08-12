#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "CampaignLaptopContentPolicy.h"
#include "GameContext.h"
#include "LaptopPageResourceOwner.h"
#include "LaptopRecordFile.h"
#include "LaptopUiStateModel.h"

	#include "builddefines.h"
	#include <stdio.h>
	#include <algorithm>
	#include <cstdio>
	#include <array>
	#include <limits>
	#include <list>
	#include <vector>
	#include "laptop.h"
	#include "files.h"
	#include "Game Clock.h"
	#include "Utilities.h"
	#include "WCheck.h"
	#include "DEBUG.H"
	#include "WordWrap.h"
	#include "Encrypted File.h"
	#include "Cursors.h"
	#include "email.h"
	#include "Text.h"
	#include "TextCatalog.h"
	// HEADROCK PROFEX: This is required to display the proper facial image.
	#include "Soldier Profile.h"
	#include "GameSettings.h"
	#include "XML.h"
	#include "expat.h"
	#include "Debug Control.h"
#define TOP_X														LAPTOP_SCREEN_UL_X
#define TOP_Y														LAPTOP_SCREEN_UL_Y
#define BLOCK_FILE_HEIGHT								10
#define BOX_HEIGHT											14
#define TITLE_X														iScreenWidthOffset + 140
#define TITLE_Y														iScreenHeightOffset + 33
#define TEXT_X														iScreenWidthOffset + 140
#define PAGE_SIZE												22
#define FILES_TITLE_FONT								FONT14ARIAL
#define FILES_TEXT_FONT												FONT10ARIAL
#define BLOCK_HEIGHT										10
#define FILES_SENDER_TEXT_X							TOP_X + 15
#define MAX_FILES_LIST_LENGTH						28
#define NUMBER_OF_FILES_IN_FILE_MANAGER 20
#define FILE_VIEWER_X												iScreenWidthOffset + 236
#define FILE_VIEWER_Y												iScreenHeightOffset + 85
#define FILE_VIEWER_WIDTH								598 - 240
#define FILE_GAP												2
#define FILE_TEXT_COLOR									FONT_BLACK
#define FILE_STRING_SIZE								400
#define MAX_FILES_PAGE									MAX_FILES_LIST_LENGTH
#define FILES_LIST_X										FILES_SENDER_TEXT_X
#define FILES_LIST_Y										( 9 * BLOCK_HEIGHT )
#define FILES_LIST_WIDTH								100

#define MAX_FILE_MESSAGE_PAGE_SIZE			325
#define MAX_TEXT_FILE_MESSAGE_PAGE_SIZE		(350)
#define VIEWER_MESSAGE_BODY_START_Y			FILES_LIST_Y
#define PREVIOUS_FILE_PAGE_BUTTON_X									iScreenWidthOffset + 553
#define PREVIOUS_FILE_PAGE_BUTTON_Y									iScreenHeightOffset + 53
#define NEXT_FILE_PAGE_BUTTON_X										iScreenWidthOffset + 577
#define NEXT_FILE_PAGE_BUTTON_Y					PREVIOUS_FILE_PAGE_BUTTON_Y

#define	FILES_COUNTER_1_WIDTH						7
#define	FILES_COUNTER_2_WIDTH						43
#define	FILES_COUNTER_3_WIDTH						45

namespace
{
using ContentPolicy = CampaignLaptopContentPolicy;

ContentPolicy CurrentLaptopContentPolicy()
{
	return ContentPolicy(GetGameContext().capabilities());
}

const char* CurrentFilesMapPath()
{
	const ContentPolicy policy = CurrentLaptopContentPolicy();
	return policy.filesMapPath(
		FileExists(ContentPolicy::unfinishedBusinessMapPath()));
}

struct PersistedFileRecord
{
	UINT8 code = 0;
	UINT32 date = 0;
	CHAR8 firstPicture[128] = {};
	CHAR8 secondPicture[128] = {};
	UINT8 format = 0;
	BOOLEAN read = FALSE;
};

constexpr UINT32 kPersistedFileRecordSize =
	sizeof(UINT8) + sizeof(UINT32) + 128 + 128 + sizeof(UINT8) +
	sizeof(BOOLEAN);

LaptopPageResourceOwner gFilesPageResources;

class ScopedDefaultFontShadow
{
public:
	~ScopedDefaultFontShadow() { SetFontShadow(DEFAULT_SHADOW); }
};

BOOLEAN GetVideoFrame(UINT32 handle, UINT16 frame, HVOBJECT* object)
{
	if (!object)
		return FALSE;

	*object = nullptr;
	return GetVideoObject(object, handle) && *object &&
		frame < (*object)->usNumberOfObjects;
}
}

// the highlighted line
INT32 iHighLightFileLine=-1;


// the files record list
FilesUnitPtr pFilesListHead=NULL;

namespace
{
constexpr UINT32 kInvalidFileRecordId =
	std::numeric_limits<UINT32>::max();

void DestroyFilesList(FilesUnitPtr& head)
{
	while (head)
	{
		FilesUnitPtr record = head;
		head = head->Next;
		if (record->pPicFileNameList[0])
			MemFree(record->pPicFileNameList[0]);
		if (record->pPicFileNameList[1])
			MemFree(record->pPicFileNameList[1]);
		MemFree(record);
	}
}

bool AppendFilesRecord(FilesUnitPtr& head, UINT8 code, UINT32 date,
	UINT8 format, const CHAR8* firstPicture, const CHAR8* secondPicture,
	BOOLEAN read, UINT32& id, bool rejectDuplicate = false)
{
	FilesUnitPtr tail = head;
	std::size_t recordCount = 0;
	while (tail)
	{
		++recordCount;
		if (tail->ubCode == code)
		{
			if (rejectDuplicate) return false;
			id = tail->uiIdNumber;
			return true;
		}
		if (!tail->Next) break;
		tail = tail->Next;
	}
	if (recordCount >= MAX_FILES_LIST_LENGTH) return false;

	id = tail ? tail->uiIdNumber + 1 : 0;
	if (tail && id == 0) return false;
	FilesUnitPtr record =
		static_cast<FilesUnitPtr>(MemAlloc(sizeof(FilesUnit)));
	if (!record) return false;
	record->Next = NULL;
	record->ubCode = code;
	record->uiDate = date;
	record->uiIdNumber = id;
	record->ubFormat = format;
	record->fRead = read;
	record->pPicFileNameList[0] = NULL;
	record->pPicFileNameList[1] = NULL;

	const CHAR8* pictures[2] = {firstPicture, secondPicture};
	for (UINT8 index = 0; index < 2; ++index)
	{
		if (!pictures[index] || pictures[index][0] == '\0') continue;
		const std::size_t length = strlen(pictures[index]);
		record->pPicFileNameList[index] =
			static_cast<CHAR8*>(MemAlloc(length + 1));
		if (!record->pPicFileNameList[index])
		{
			if (record->pPicFileNameList[0])
				MemFree(record->pPicFileNameList[0]);
			MemFree(record);
			return false;
		}
		memcpy(record->pPicFileNameList[index], pictures[index], length);
		record->pPicFileNameList[index][length] = '\0';
	}

	if (tail) tail->Next = record;
	else head = record;
	return true;
}
}

FileStringPtr pFileStringList = NULL;

// are we in files mode
BOOLEAN fInFilesMode=FALSE;
BOOLEAN fOnLastFilesPageFlag = FALSE;


//. did we enter due to new file icon?
BOOLEAN fEnteredFileViewerFromNewFileIcon = FALSE;
BOOLEAN fWaitAFrame = FALSE;

// are there any new files
BOOLEAN fNewFilesInFileViewer = FALSE;

// graphics handles
extern UINT32 guiTITLE; // symbol already defined in laptop.cpp (jonathanl)
UINT32 guiFileBack;
extern UINT32 guiTOP; // symbol already defined in finances.cpp (jonathanl)
UINT32 guiHIGHLIGHT;


// currewnt page of multipage files we are on
INT32 giFilesPage = 0;
// strings

#define SLAY_LENGTH 12
#define ENRICO_LENGTH 0


UINT8 ubFileRecordsLength[]={
	ENRICO_LENGTH,
	SLAY_LENGTH,
	SLAY_LENGTH,
	SLAY_LENGTH,
	SLAY_LENGTH,
	SLAY_LENGTH,
	SLAY_LENGTH,
};

UINT16 ubFileOffsets[]={
	0,
	ENRICO_LENGTH,
	SLAY_LENGTH + ENRICO_LENGTH,
	2 * SLAY_LENGTH + ENRICO_LENGTH,
	3 * SLAY_LENGTH + ENRICO_LENGTH,
	4 * SLAY_LENGTH + ENRICO_LENGTH,
	5 * SLAY_LENGTH + ENRICO_LENGTH,
};


UINT16 usProfileIdsForTerroristFiles[]={
	0, // no body
	112, // elgin
	64, // slay
	82, // mom
	83, // imposter
	110, // tiff
	111, // t-rex
	112, // elgin
};
// buttons for next and previous pages
UINT32 giFilesPageButtons[ 2 ];
INT32 giFilesPageButtonsImage[ 2 ] = {-1, -1};


// the previous and next pages buttons

enum{
	PREVIOUS_FILES_PAGE_BUTTON=0,
	NEXT_FILES_PAGE_BUTTON,
};
// mouse regions
MOUSE_REGION pFilesRegions[MAX_FILES_PAGE];



// function definitions
void RenderFilesBackGround( void );
BOOLEAN LoadFiles(LaptopPageResourceOwner& owner);
UINT32 ProcessAndEnterAFilesRecord(UINT8 ubCode, UINT32 uiDate,
	UINT8 ubFormat, const CHAR8* pFirstPicFile,
	const CHAR8* pSecondPicFile, BOOLEAN fRead);
void OpenAndReadFilesFile( void );
BOOLEAN OpenAndWriteFilesFile( void );
void ClearFilesList( void );
void DrawFilesTitleText( void );
void DrawFilesListBackGround( void );
void DisplayFilesList( void );
BOOLEAN OpenAndWriteFilesFile( void );
void DisplayFileMessage( void );
BOOLEAN InitializeFilesMouseRegions(LaptopPageResourceOwner& owner);
BOOLEAN DisplayFormattedText( void );


// buttons
BOOLEAN CreateButtonsForFilesPage(LaptopPageResourceOwner& owner);
void HandleFileViewerButtonStates( void );


// open new files for viewing
void OpenFirstUnreadFile( void );
void CheckForUnreadFiles( void );



// file string structure manipulations
void ClearFileStringList( void );
BOOLEAN AddStringToFilesList( STR16 pString );
BOOLEAN HandleSpecialFiles( UINT8 ubFormat );
BOOLEAN HandleSpecialTerroristFile( INT32 iFileNumber, STR sPictureName );

BOOLEAN HandleMissionBriefingFiles( UINT8 ubFormat ); //mission briefing by Jazz
FileRecordWidthPtr CreateWidthRecordsForMissionBriefingFile( void );

// callbacks
void FilesBtnCallBack(MOUSE_REGION * pRegion, INT32 iReason );
void BtnPreviousFilePageCallback(GUI_BUTTON *btn,INT32 reason);
void BtnNextFilePageCallback(GUI_BUTTON *btn,INT32 reason);

// file width manipulation
void ClearOutWidthRecordsList( FileRecordWidthPtr pFileRecordWidthList );
FileRecordWidthPtr CreateWidthRecordsForAruloIntelFile( void );
FileRecordWidthPtr CreateWidthRecordsForTerroristFile( void );
FileRecordWidthPtr CreateRecordWidth( 	INT32 iRecordNumber, INT32 iRecordWidth, INT32 iRecordHeightAdjustment, UINT8 ubFlags );

// sun_alf: functionality of additional files processing
#define ADDFILES_NAME_MAX_LENGTH    (16)
#define ADDFILES_PATH_MAX_LENGTH    (128)
typedef struct
{
	CHAR16		name[ADDFILES_NAME_MAX_LENGTH + 1];  // in-game displayed name of file
	CHAR8		path[ADDFILES_PATH_MAX_LENGTH + 1];  // local path to file under "/Laptop" dir
	BOOLEAN		atInit;     // is the file available from game init
	UINT8		font;       // font code
	UINT8		textColor;  // text color code
	UINT8		bkgdColor;  // text background color code
	UINT8       fileCode;   // file code to integrate with vanilla files (starts with ADDITIONAL_FILE_0)
} AdditionalFiles_Descriptor;

typedef enum
{
	ADDFILES_ELEMENT_NONE = 0,
	ADDFILES_ELEMENT_ADDITIONALFILES,
	ADDFILES_ELEMENT_FILE,
	ADDFILES_ELEMENT,
} AdditionalFiles_ParseStage;

typedef struct
{
	AdditionalFiles_ParseStage curElement;
	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH + 1];
	UINT32		currentDepth;
	UINT32		maxReadDepth;
	UINT8		fileCode;

	AdditionalFiles_Descriptor parsedData;
} AdditionalFiles_ParseData;

static BOOLEAN HandleAdditionalTextFile( FilesUnitPtr file );

static std::list<AdditionalFiles_Descriptor> g_AdditionalFilesList;


static AdditionalFiles_Descriptor* AdditionalFiles_GetDescriptor( UINT8 ubCode )
{
	for ( AdditionalFiles_Descriptor &item : g_AdditionalFilesList )
	{
		if ( item.fileCode == ubCode )
		{
			return &item;
		}
	}
	return NULL;
}

static AdditionalFiles_Descriptor* AdditionalFiles_LoadTextFile( FilesUnitPtr file )
{
	AdditionalFiles_Descriptor *descr = AdditionalFiles_GetDescriptor( file->ubCode );

	if ( descr )
	{
		ClearFileStringList();

		const std::string fileName =
			TABLEDATA_DIRECTORY TABLEDATA_LAPTOP_DIRECTORY +
			std::string(descr->path);
		if ( FileExists( fileName.c_str() ) )
		{
			HWFILE hFile = FileOpen( fileName.c_str(), FILE_ACCESS_READ, FALSE );
			if ( hFile )
			{
				std::string utf8Line;
				while ( FileReadLine( hFile, &utf8Line ) )
				{
					std::wstring wcharLine = utf8_to_wstring( utf8Line );
					if (!AddStringToFilesList((STR16)wcharLine.c_str()))
					{
						FileClose(hFile);
						ClearFileStringList();
						return NULL;
					}
				}
				FileClose( hFile );
			}
		}
	}

	return descr;
}

static INT32 AdditionalFiles_GetFontHandler( UINT8 font )
{
	INT32 iFont = (INT32)font;
	if ( IsFontLoaded( iFont ) == FALSE )
		iFont = FILES_TEXT_FONT;

	return iFont;
}

static BOOLEAN AdditionalFiles_IsValid( AdditionalFiles_Descriptor *descr )
{
	BOOLEAN result = FALSE;
	const std::string fileName =
		TABLEDATA_DIRECTORY TABLEDATA_LAPTOP_DIRECTORY +
		std::string(descr->path);
	BOOLEAN exists = FileExists( fileName.c_str() );

	if ( exists && wcslen( descr->name ) > 0 &&
		(INT32)descr->font <= MAX_FONTS &&
		ADDITIONAL_FILE_0 <= descr->fileCode && descr->fileCode <= ADDITIONAL_FILE_MAX )
	{
		result = TRUE;
	}
		
	return result;
}

// Process the opening tag in this expat callback.
static void XMLCALL AdditionalFiles_StartElementHandler( void *userData, const XML_Char *name, const XML_Char **atts )
{
	AdditionalFiles_ParseData *pData = (AdditionalFiles_ParseData*) userData;

	if ( pData->currentDepth <= pData->maxReadDepth )
	{
		if ( strcmp( name, "AdditionalFiles" ) == 0 && pData->curElement == ADDFILES_ELEMENT_NONE )
		{
			pData->fileCode = ADDITIONAL_FILE_0;  // assign ID to each <File> entry
			pData->curElement = ADDFILES_ELEMENT_ADDITIONALFILES;
			pData->maxReadDepth++;
		}
		else if ( strcmp( name, "File" ) == 0 && pData->curElement == ADDFILES_ELEMENT_ADDITIONALFILES )
		{
			memset( &pData->parsedData, 0, sizeof(pData->parsedData) );
			pData->curElement = ADDFILES_ELEMENT_FILE;
			pData->maxReadDepth++;
		}
		else if ( pData->curElement == ADDFILES_ELEMENT_FILE &&
			(strcmp( name, "Name" ) == 0 ||
			strcmp( name, "Path" ) == 0 ||
			strcmp( name, "AtInit" ) == 0 ||
			strcmp( name, "Font" ) == 0 ||
			strcmp( name, "TextColor" ) == 0 ||
			strcmp( name, "BkgdColor" ) == 0) )
		{
			pData->curElement = ADDFILES_ELEMENT;
			pData->maxReadDepth++;
		}
		pData->szCharData[0] = '\0';
	}
	pData->currentDepth++;
}

// Process any text content in this callback.
static void XMLCALL AdditionalFiles_CharacterDataHandler( void *userData, const XML_Char *str, int len )
{
	AdditionalFiles_ParseData *pData = (AdditionalFiles_ParseData*) userData;

	if (pData->currentDepth > pData->maxReadDepth || !str || len <= 0)
		return;
	const std::size_t currentLength = strlen(pData->szCharData);
	if (currentLength >= MAX_CHAR_DATA_LENGTH) return;
	const std::size_t copyLength = std::min(
		static_cast<std::size_t>(len),
		static_cast<std::size_t>(MAX_CHAR_DATA_LENGTH) - currentLength);
	memcpy(pData->szCharData + currentLength, str, copyLength);
	pData->szCharData[currentLength + copyLength] = '\0';
}

// Process the closing tag in this expat callback.
static void XMLCALL AdditionalFiles_EndElementHandler( void *userData, const XML_Char *name )
{
	AdditionalFiles_ParseData *pData = (AdditionalFiles_ParseData*) userData;

	if ( pData->currentDepth <= pData->maxReadDepth )
	{
		if ( pData->curElement == ADDFILES_ELEMENT_ADDITIONALFILES && strcmp( name, "AdditionalFiles" ) == 0 )
		{
			pData->curElement = ADDFILES_ELEMENT_NONE;
		}
		else if ( pData->curElement == ADDFILES_ELEMENT_FILE && strcmp( name, "File" ) == 0 )
		{
			pData->parsedData.fileCode = pData->fileCode++;
			if ( AdditionalFiles_IsValid( &pData->parsedData ) == TRUE )
			{
				g_AdditionalFilesList.push_back( pData->parsedData );
			}

			pData->curElement = ADDFILES_ELEMENT_ADDITIONALFILES;
		}
		else if ( pData->curElement == ADDFILES_ELEMENT )
		{
			if ( strcmp( name, "Name" ) == 0 )
			{
				std::string strName( pData->szCharData );
				std::wstring wstrName = utf8_to_wstring( strName );
				LaptopUiStateModel::CopyText(
					pData->parsedData.name, wstrName.c_str());
			}
			else if ( strcmp( name, "Path" ) == 0 )
				LaptopUiStateModel::CopyText(
					pData->parsedData.path, pData->szCharData);
			else if ( strcmp( name, "AtInit" ) == 0 )
				pData->parsedData.atInit = (BOOLEAN)atol( pData->szCharData );
			else if ( strcmp( name, "Font" ) == 0 )
				pData->parsedData.font = (UINT8)atol( pData->szCharData );
			else if ( strcmp( name, "TextColor" ) == 0 )
				pData->parsedData.textColor = (UINT8)atol( pData->szCharData );
			else if ( strcmp( name, "BkgdColor" ) == 0 )
				pData->parsedData.bkgdColor = (UINT8)atol( pData->szCharData );
			
			pData->curElement = ADDFILES_ELEMENT_FILE;
		}

		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

// Parse AdditionalFiles.xml and build list of file descriptors
static void LocateAdditionalFiles( )
{
	g_AdditionalFilesList.clear();

	const std::string fileName =
		TABLEDATA_DIRECTORY TABLEDATA_LAPTOP_DIRECTORY
		LAPTOPADDITIONALFILESFILENAME;
	AdditionalFiles_ParseData pData;
	memset( &pData, 0, sizeof( pData ) );
	const LegacyXmlCallbacks callbacks{
		&pData, AdditionalFiles_StartElementHandler,
		AdditionalFiles_EndElementHandler,
		AdditionalFiles_CharacterDataHandler};
	const LegacyXmlResult result =
		ParseLegacyXmlFile(fileName.c_str(), callbacks);
	if (!result && result.status != LegacyXmlStatus::NotFound &&
		result.status != LegacyXmlStatus::ReadError)
	{
		const auto message = FormatLegacyXmlFailure(fileName.c_str(), result);
		LiveMessage(message.data());
	}
}


UINT32 AddFilesToPlayersLog(UINT8 ubCode, UINT32 uiDate, UINT8 ubFormat, STR8 pFirstPicFile, STR8 pSecondPicFile )
{
	// adds Files item to player's log(Files List), returns unique id number of it
	// outside of the Files system(the code in this .c file), this is the only function you'll ever need
	UINT32 uiId=0;

	// if not in Files mode, read in from file
	if(!fInFilesMode)
	OpenAndReadFilesFile( );

	// process the actual data
	uiId = ProcessAndEnterAFilesRecord(ubCode, uiDate, ubFormat ,pFirstPicFile, pSecondPicFile, FALSE );
	if (uiId == kInvalidFileRecordId) return uiId;

	// set unread flag, if nessacary
	CheckForUnreadFiles( );

	// write out to file if not in Files mode
	if(!fInFilesMode)
	OpenAndWriteFilesFile( );

	// return unique id of this transaction
	return uiId;
}

void GameInitFiles( )
{
	if ( FileExists( FILES_DAT_FILE ) == TRUE )
	{
		FileDelete( FILES_DAT_FILE );
	}

	ClearFilesList( );

	// add background check by RIS
	AddFilesToPlayersLog( ENRICO_BACKGROUND, 0,255, NULL, NULL );

	// additional files: add all available from game beginning files
	LocateAdditionalFiles();
	for ( AdditionalFiles_Descriptor item : g_AdditionalFilesList )
	{
		if ( item.atInit )
			AddFilesToPlayersLog( item.fileCode, 0, FFORMAT_ADDITIONAL_TEXT, NULL, NULL );
	}

	//mission briefing by Jazz
	//AddFilesToPlayersLog( MISSION_BRIEFING, 0,4, NULL, NULL );
}

void EnterFiles()
{
	LaptopPageResourceOwner stagedResources;

	// load grpahics for files system
	if (!LoadFiles(stagedResources) ||
		!InitializeFilesMouseRegions(stagedResources) ||
		!CreateButtonsForFilesPage(stagedResources))
		return;
	gFilesPageResources = std::move(stagedResources);

	//AddFilesToPlayersLog(1, 0, 0,"LAPTOP\\portrait.sti", "LAPTOP\\portrait.sti");
	//AddFilesToPlayersLog(0, 0, 3,"LAPTOP\\portrait.sti", "LAPTOP\\portrait.sti");
	//AddFilesToPlayersLog(2, 0, 1,"LAPTOP\\portrait.sti", "LAPTOP\\portrait.sti");
	// in files mode now, set the fact
	fInFilesMode=TRUE;

	// now set start states
	HandleFileViewerButtonStates( );

	// Re-locate additional files: parse AdditionalFiles.xml and build descriptors. Processing from scratch on each Laptop
	// files mode opening come handy for modders, also it follows such rule of this (files.cpp) module.
	LocateAdditionalFiles( );

	// build files list
	OpenAndReadFilesFile( );

	// render files system
	RenderFiles( );

	// entered due to icon
	if( fEnteredFileViewerFromNewFileIcon == TRUE )
	{
	OpenFirstUnreadFile( );
		fEnteredFileViewerFromNewFileIcon = FALSE;
	}


}

void ExitFiles()
{
	// write files list out to disk
	OpenAndWriteFilesFile( );

	fInFilesMode = FALSE;

	gFilesPageResources.clear();

	// clear and allow to re-load all the stuff at next Laptop opening
	g_AdditionalFilesList.clear();
}

void HandleFiles()
{
	CheckForUnreadFiles( );
}

void RenderFiles()
{
	HVOBJECT hHandle;


	// render the background
	RenderFilesBackGround(	);

	// draw the title bars text
	DrawFilesTitleText( );

	// the columns
	DrawFilesListBackGround( );

	// display the list of senders
	DisplayFilesList( );

	// draw the highlighted file
	DisplayFileMessage( );

	// title bar icon
	BlitTitleBarIcons(	);


	// display border
	if (GetVideoFrame(guiLaptopBACKGROUND, 0, &hHandle))
		BltVideoObject(FRAME_BUFFER, hHandle, 0, iScreenWidthOffset + 108,
			iScreenHeightOffset + 23, VO_BLT_SRCTRANSPARENCY, NULL);

}


void RenderFilesBackGround( void )
{
	// render generic background for file system
	HVOBJECT hHandle;

	// get title bar object
	if (GetVideoFrame(guiTITLE, 0, &hHandle))
		BltVideoObject(FRAME_BUFFER, hHandle, 0, TOP_X, TOP_Y - 2,
			VO_BLT_SRCTRANSPARENCY, NULL);

	// get and blt the top part of the screen, video object and blt to screen
	if (GetVideoFrame(guiTOP, 0, &hHandle))
		BltVideoObject(FRAME_BUFFER, hHandle, 0, TOP_X, TOP_Y + 22,
			VO_BLT_SRCTRANSPARENCY, NULL);



		return;
}

void DrawFilesTitleText( void )
{
	// setup the font stuff
	SetFont(FILES_TITLE_FONT);
	SetFontForeground(FONT_WHITE);
	SetFontBackground(FONT_BLACK);
	// reset shadow
	SetFontShadow(DEFAULT_SHADOW);

	// draw the pages title
	mprintf(TITLE_X, TITLE_Y, L"%s",
		i18n::GetCompiledTextPack().text(i18n::TextKey::FilesTitle).data());


	return;
}


BOOLEAN LoadFiles(LaptopPageResourceOwner& owner)
{
	VOBJECT_DESC	VObjectDesc;
	// load files video objects into memory

	// title bar
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\programtitlebar.sti", VObjectDesc.ImageFile);
	CHECKF(owner.addVideoObject(&VObjectDesc, guiTITLE));

	// top portion of the screen background
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\fileviewer.sti", VObjectDesc.ImageFile);
	CHECKF(owner.addVideoObject(&VObjectDesc, guiTOP));


	// the highlight
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\highlight.sti", VObjectDesc.ImageFile);
	CHECKF(owner.addVideoObject(&VObjectDesc, guiHIGHLIGHT));

		// top portion of the screen background
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\fileviewerwhite.sti", VObjectDesc.ImageFile);
	CHECKF(owner.addVideoObject(&VObjectDesc, guiFileBack));

	return (TRUE);
}

UINT32 ProcessAndEnterAFilesRecord(UINT8 ubCode, UINT32 uiDate,
	UINT8 ubFormat, const CHAR8* pFirstPicFile,
	const CHAR8* pSecondPicFile, BOOLEAN fRead)
{
	UINT32 id = kInvalidFileRecordId;
	if (!AppendFilesRecord(pFilesListHead, ubCode, uiDate, ubFormat,
		pFirstPicFile, pSecondPicFile, fRead, id))
		return kInvalidFileRecordId;
	return id;
}

void OpenAndReadFilesFile( void )
{
	// no file, return
	if ( ! (FileExists( FILES_DAT_FILE ) ) )
		return;

	ScopedLaptopFile file(FileOpen(FILES_DAT_FILE,
		FILE_OPEN_EXISTING | FILE_ACCESS_READ, FALSE));
	if (!file) return;
	const UINT32 fileSize = FileGetSize(file.Get());
	if (fileSize % kPersistedFileRecordSize != 0) return;
	if (fileSize / kPersistedFileRecordSize > MAX_FILES_LIST_LENGTH) return;

	FilesUnitPtr pendingHead = NULL;
	for (UINT32 offset = 0; offset < fileSize;
		offset += kPersistedFileRecordSize)
	{
		PersistedFileRecord record{};
		if (!ReadLaptopFileExact(file.Get(), &record.code,
				sizeof(record.code)) ||
			!ReadLaptopFileExact(file.Get(), &record.date,
				sizeof(record.date)) ||
			!ReadLaptopFileExact(file.Get(), record.firstPicture,
				sizeof(record.firstPicture)) ||
			!ReadLaptopFileExact(file.Get(), record.secondPicture,
				sizeof(record.secondPicture)) ||
			!ReadLaptopFileExact(file.Get(), &record.format,
				sizeof(record.format)) ||
			!ReadLaptopFileExact(file.Get(), &record.read,
				sizeof(record.read)))
		{
			DestroyFilesList(pendingHead);
			return;
		}
		record.firstPicture[127] = '\0';
		record.secondPicture[127] = '\0';
		UINT32 id = kInvalidFileRecordId;
		if (!AppendFilesRecord(pendingHead, record.code, record.date,
			record.format, record.firstPicture, record.secondPicture,
			record.read, id, true))
		{
			DestroyFilesList(pendingHead);
			return;
		}
	}

	DestroyFilesList(pFilesListHead);
	pFilesListHead = pendingHead;
}


BOOLEAN OpenAndWriteFilesFile( void )
{
	FilesUnitPtr pFilesList=pFilesListHead;

	// open file
	ScopedLaptopFile file(FileOpen(FILES_DAT_FILE,
		FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE));

	// if no file exits, do nothing
	if(!file)
	{
		return ( FALSE );
	}
	// write info, while there are elements left in the list
	while(pFilesList)
	{
		PersistedFileRecord record{};
		record.code = pFilesList->ubCode;
		record.date = pFilesList->uiDate;
		record.format = pFilesList->ubFormat;
		record.read = pFilesList->fRead;
		if (pFilesList->pPicFileNameList[0])
			LaptopUiStateModel::CopyText(record.firstPicture,
				pFilesList->pPicFileNameList[0]);
		if (pFilesList->pPicFileNameList[1])
			LaptopUiStateModel::CopyText(record.secondPicture,
				pFilesList->pPicFileNameList[1]);
		// now write date and amount, and code
		if (!WriteLaptopFileExact(file.Get(), &record.code,
				sizeof(record.code)) ||
			!WriteLaptopFileExact(file.Get(), &record.date,
				sizeof(record.date)) ||
			!WriteLaptopFileExact(file.Get(), record.firstPicture,
				sizeof(record.firstPicture)) ||
			!WriteLaptopFileExact(file.Get(), record.secondPicture,
				sizeof(record.secondPicture)) ||
			!WriteLaptopFileExact(file.Get(), &record.format,
				sizeof(record.format)) ||
			!WriteLaptopFileExact(file.Get(), &record.read,
				sizeof(record.read)))
			return FALSE;

		// next element in list
		pFilesList = pFilesList->Next;

	}

	file.Close();
	// clear out the old list
	ClearFilesList( );

	return ( TRUE );
}

void ClearFilesList( void )
{
	DestroyFilesList(pFilesListHead);
	return;
}

void DrawFilesListBackGround( void )
{
	// proceudre will draw the background for the list of files
	 // HVOBJECT hHandle;

	// now the columns



	return;

}


void DisplayFilesList( void )
{
	// this function will run through the list of files of files and display the 'sender'
	FilesUnitPtr pFilesList=pFilesListHead;
	INT32 iCounter=0;
	HVOBJECT hHandle;


	// font stuff
	SetFont(FILES_TEXT_FONT);
	SetFontForeground(FONT_BLACK);
	SetFontBackground(FONT_BLACK);
	SetFontShadow(NO_SHADOW);

	// runt hrough list displaying 'sender'
	while((pFilesList))//&&(iCounter < MAX_FILES_LIST_LENGTH))
	{
		if (iCounter==iHighLightFileLine)
		{
			// render highlight
			if (GetVideoFrame(guiHIGHLIGHT, 0, &hHandle))
				BltVideoObject(FRAME_BUFFER, hHandle, 0,
					FILES_SENDER_TEXT_X - 5,
					iScreenHeightOffset + ((iCounter + 9) * BLOCK_HEIGHT) +
						(iCounter * 2) - 4,
					VO_BLT_SRCTRANSPARENCY, NULL);
		}

		if ( pFilesList->ubCode <= LAST_JA2_VANILLA_FILE )
		{
			mprintf(FILES_SENDER_TEXT_X,
				iScreenHeightOffset + ((iCounter + 9) * BLOCK_HEIGHT) +
					(iCounter * 2) - 2,
				L"%s", pFilesSenderList[pFilesList->ubCode]);
		}
		else  // additional file case
		{
			AdditionalFiles_Descriptor *descr = AdditionalFiles_GetDescriptor( pFilesList->ubCode );
			if ( descr )
				mprintf(FILES_SENDER_TEXT_X,
					iScreenHeightOffset + ((iCounter + 9) * BLOCK_HEIGHT) +
						(iCounter * 2) - 2,
					L"%s", descr->name);
		}

		iCounter++;
		pFilesList=pFilesList->Next;
	}

	// reset shadow
	SetFontShadow(DEFAULT_SHADOW);

	return;

}



void DisplayFileMessage( void )
{





	// get the currently selected message
	if(iHighLightFileLine!=-1)
	{
		// display text
	DisplayFormattedText( );
	}
	else
	{
		HandleFileViewerButtonStates( );
	}

	// reset shadow
	SetFontShadow(DEFAULT_SHADOW);

	return;
}


BOOLEAN InitializeFilesMouseRegions(LaptopPageResourceOwner& owner)
{
	INT32 iCounter=0;
	// init mouseregions
	for(iCounter=0; iCounter <MAX_FILES_PAGE; iCounter++)
	{
	MSYS_DefineRegion(&pFilesRegions[iCounter],FILES_LIST_X ,(INT16)iScreenHeightOffset + (FILES_LIST_Y + iCounter * ( BLOCK_HEIGHT + 2 ) ), FILES_LIST_X + FILES_LIST_WIDTH ,(INT16)iScreenHeightOffset + (FILES_LIST_Y + ( iCounter + 1 ) * ( BLOCK_HEIGHT + 2 ) ),
			MSYS_PRIORITY_NORMAL+2,MSYS_NO_CURSOR, MSYS_NO_CALLBACK, FilesBtnCallBack );
		CHECKF(owner.addRegion(pFilesRegions[iCounter]));
		MSYS_SetRegionUserData(&pFilesRegions[iCounter],0,iCounter);
	}


	return TRUE;
}

void FilesBtnCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	INT32 iFileId = -1;
	INT32 iCounter = 0;
	FilesUnitPtr pFilesList=pFilesListHead;


	if (iReason & MSYS_CALLBACK_REASON_INIT)
	{
	return;
	}


	if(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{

		// left button
	iFileId = MSYS_GetRegionUserData(pRegion, 0);

	// reset iHighLightListLine
	iHighLightFileLine = -1;
	if (iFileId < 0) return;


	// make sure is a valid
	while( pFilesList )
	{

		// if iCounter = iFileId, is a valid file
	 if( iCounter == iFileId )
		{
			giFilesPage = 0;
			iHighLightFileLine = iFileId;
		}

		// next element in list
		pFilesList = pFilesList->Next;

		// increment counter
		iCounter++;
	}

	fReDrawScreenFlag=TRUE;


	return;
	}

}


BOOLEAN DisplayFormattedText( void )
{
	FilesUnitPtr pFilesList=pFilesListHead;

	UINT16 usFirstWidth = 0;
	UINT16 usFirstHeight = 0;
	UINT16 usSecondWidth;
	UINT16 usSecondHeight;
	UINT16 firstImageOffset = 0;
	INT32 combinedWidth = 0;
	UINT32 uiCounter = 0;
	UINT32 uiLength = 0;
	UINT32 uiHeight = 0;
	UINT32 uiOffSet = 0;
	CHAR16 sString[2048];
	HVOBJECT hHandle;
	UniqueVideoObjectHandle firstTempPicture;
	UniqueVideoObjectHandle secondTempPicture;
	VOBJECT_DESC	VObjectDesc;
	INT16 usFreeSpace = 0;
	static INT32 iOldMessageCode = 0;

	fWaitAFrame = FALSE;

	// get the file that was highlighted
	for ( INT32 i = 0; i < iHighLightFileLine; i++ )
	{
		if (!pFilesList) return FALSE;
		pFilesList = pFilesList->Next;
	}
	if (!pFilesList) return FALSE;

	// set file as read
	pFilesList->fRead = TRUE;

	// clear the file string structure list
	// get file background object
	if (!GetVideoFrame(guiFileBack, 0, &hHandle)) return FALSE;

	// blt background to screen
	BltVideoObject(FRAME_BUFFER, hHandle, 0, FILE_VIEWER_X, FILE_VIEWER_Y - 4, VO_BLT_SRCTRANSPARENCY,NULL);

	// get the offset in the file
	if ( pFilesList->ubCode <= LAST_JA2_VANILLA_FILE )
	{
		uiOffSet = ubFileOffsets[pFilesList->ubCode];
		uiLength = ubFileRecordsLength[pFilesList->ubCode];
	}

	// no shadow
	SetFontShadow(NO_SHADOW);
	ScopedDefaultFontShadow restoreFontShadow;

	switch( pFilesList->ubFormat )
	{
		case 0:
			// no format, all text
			while(uiLength > uiCounter)
			{
				// read one record from file manager file
				if (!LoadEncryptedDataFromFile("BINARYDATA\\Files.edt",
					sString, FILE_STRING_SIZE * (uiOffSet + uiCounter) * 2,
					FILE_STRING_SIZE * 2)) return FALSE;

				// display string and get height
				uiHeight += IanDisplayWrappedString(FILE_VIEWER_X + 4, ( UINT16 )( FILE_VIEWER_Y + uiHeight ), FILE_VIEWER_WIDTH, FILE_GAP, FILES_TEXT_FONT, FILE_TEXT_COLOR, sString,0,FALSE,0);

				// increment file record counter
				uiCounter++;
			}
			break;

		case 1:

			// second format, one picture, all text below

		// load graphic
			if (!pFilesList->pPicFileNameList[0]) return FALSE;
			VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
		FilenameForBPP( pFilesList->pPicFileNameList[ 0 ], VObjectDesc.ImageFile );
			firstTempPicture = AddVideoObjectOwned(&VObjectDesc);
			CHECKF(firstTempPicture);

		if (!GetVideoObjectETRLESubregionProperties(firstTempPicture.get(), 0,
			&usFirstWidth, &usFirstHeight)) return FALSE;


			// get file background object
			if (!GetVideoFrame(firstTempPicture.get(), 0, &hHandle)) return FALSE;

		// blt background to screen
		firstImageOffset = usFirstWidth < FILE_VIEWER_WIDTH
			? (FILE_VIEWER_WIDTH - usFirstWidth) / 2 : 0;
		BltVideoObject(FRAME_BUFFER, hHandle, 0,
			FILE_VIEWER_X + 4 + firstImageOffset, FILE_VIEWER_Y + 10,
			VO_BLT_SRCTRANSPARENCY, NULL);

			uiHeight = usFirstHeight + 20;


			while(uiLength > uiCounter)
			{
				// read one record from file manager file
				if (!LoadEncryptedDataFromFile("BINARYDATA\\Files.edt",
					sString, FILE_STRING_SIZE * (uiOffSet + uiCounter) * 2,
					FILE_STRING_SIZE * 2)) return FALSE;

				// display string and get height
				uiHeight += IanDisplayWrappedString(FILE_VIEWER_X + 4, ( UINT16 )( FILE_VIEWER_Y + uiHeight ), FILE_VIEWER_WIDTH, FILE_GAP, FILES_TEXT_FONT, FILE_TEXT_COLOR, sString,0,FALSE,0);

				// increment file record counter
				uiCounter++;
			}

		break;

		case 2:

			// third format, two pictures, side by side with all text below

			// load first graphic
			if (!pFilesList->pPicFileNameList[0] ||
				!pFilesList->pPicFileNameList[1]) return FALSE;
			VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
		FilenameForBPP( pFilesList->pPicFileNameList[ 0 ], VObjectDesc.ImageFile );
			firstTempPicture = AddVideoObjectOwned(&VObjectDesc);
			CHECKF(firstTempPicture);

			// load second graphic
			VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
		FilenameForBPP( pFilesList->pPicFileNameList[ 1 ] , VObjectDesc.ImageFile );
			secondTempPicture = AddVideoObjectOwned(&VObjectDesc);
			CHECKF(secondTempPicture);

		if (!GetVideoObjectETRLESubregionProperties(firstTempPicture.get(), 0,
				&usFirstWidth, &usFirstHeight) ||
			!GetVideoObjectETRLESubregionProperties(secondTempPicture.get(), 0,
				&usSecondWidth, &usSecondHeight)) return FALSE;

		// Keep oversized optional artwork from underflowing unsigned layout.
			combinedWidth = static_cast<INT32>(usFirstWidth) +
				static_cast<INT32>(usSecondWidth);
			usFreeSpace = static_cast<INT16>(std::max<INT32>(0,
				static_cast<INT32>(FILE_VIEWER_WIDTH) - combinedWidth) / 3);
		// get file background object
		if (!GetVideoFrame(firstTempPicture.get(), 0, &hHandle)) return FALSE;


		// blt background to screen
		BltVideoObject(FRAME_BUFFER, hHandle, 0, FILE_VIEWER_X + usFreeSpace, FILE_VIEWER_Y + 10, VO_BLT_SRCTRANSPARENCY,NULL);

		// get file background object
		if (!GetVideoFrame(secondTempPicture.get(), 0, &hHandle)) return FALSE;

			// get position for second picture
			usFreeSpace *= 2;
			usFreeSpace += usFirstWidth;

			// blt background to screen
		BltVideoObject(FRAME_BUFFER, hHandle, 0, FILE_VIEWER_X + usFreeSpace, FILE_VIEWER_Y + 10, VO_BLT_SRCTRANSPARENCY,NULL);


			// put in text
			uiHeight = usFirstHeight + 20;


			while(uiLength > uiCounter)
			{
				// read one record from file manager file
				if (!LoadEncryptedDataFromFile("BINARYDATA\\Files.edt",
					sString, FILE_STRING_SIZE * (uiOffSet + uiCounter) * 2,
					FILE_STRING_SIZE * 2)) return FALSE;

				// display string and get height
				uiHeight += IanDisplayWrappedString(FILE_VIEWER_X + 4, ( UINT16 )( FILE_VIEWER_Y + uiHeight ), FILE_VIEWER_WIDTH, FILE_GAP, FILES_TEXT_FONT, FILE_TEXT_COLOR, sString,0,FALSE,0);

				// increment file record counter
				uiCounter++;
			}


		break;

		case 3:
			// picture on the left, with text on right and below
			// load first graphic
			if (!HandleSpecialTerroristFile(pFilesList->ubCode,
				pFilesList->pPicFileNameList[0])) return FALSE;
			break;

		case 4:
			// picture on the left, with text on right and below
			// load first graphic
			if (!HandleMissionBriefingFiles(pFilesList->ubFormat))
				return FALSE;
			break;

		case FFORMAT_ADDITIONAL_TEXT:
			if (!HandleAdditionalTextFile(pFilesList)) return FALSE;
			break;

		default:
			if (!HandleSpecialFiles(pFilesList->ubFormat)) return FALSE;
	}

	HandleFileViewerButtonStates( );
	SetFontShadow(DEFAULT_SHADOW);

	return ( TRUE );
}

//Mission briefing by Jazz

FileRecordWidthPtr CreateWidthRecordsForMissionBriefingFile( void )
{
	// this fucntion will create the width list for the Arulco intelligence file
	FileRecordWidthPtr pTempRecord = NULL;
	FileRecordWidthPtr pRecordListHead = NULL;


		// first record width
//	pTempRecord = CreateRecordWidth( 7, 350, 200,0 );
	pTempRecord = CreateRecordWidth( FILES_COUNTER_1_WIDTH, 200, 0,0 );
	if (!pTempRecord) return NULL;

	// set up head of list now
	pRecordListHead = pTempRecord;

	// next record
//	pTempRecord->Next = CreateRecordWidth( 43, 200,0, 0 );
	pTempRecord->Next = CreateRecordWidth( FILES_COUNTER_2_WIDTH, 200,0, 0 );
	if (!pTempRecord->Next)
	{
		ClearOutWidthRecordsList(pRecordListHead);
		return NULL;
	}
	pTempRecord = pTempRecord->Next;

	// and the next..
//	pTempRecord->Next = CreateRecordWidth( 45, 200,0, 0 );
	pTempRecord->Next = CreateRecordWidth( FILES_COUNTER_3_WIDTH, 200,0, 0 );
	if (!pTempRecord->Next)
	{
		ClearOutWidthRecordsList(pRecordListHead);
		return NULL;
	}

	return( pRecordListHead );

}

BOOLEAN HandleMissionBriefingFiles( UINT8 ubFormat )
{
	INT32 iCounter = 0;
	CHAR16 sString[2048];
	FileStringPtr pTempString = NULL ;
	FileStringPtr pLocatorString = NULL;
	INT32 iYPositionOnPage = 0;
	INT32 iFileLineWidth = 0;
	INT32 iFileStartX = 0;
	UINT32 uiFlags = 0;
	UINT32 uiFont = 0;
	BOOLEAN fGoingOffCurrentPage = FALSE;
	FileRecordWidthPtr WidthList = NULL;


	HVOBJECT hHandle;
	VOBJECT_DESC VObjectDesc;


	ClearFileStringList( );

	switch( ubFormat )
	{
		case( 4 ):
			// load data
			// read one record from file manager file

			WidthList = CreateWidthRecordsForMissionBriefingFile( );
			if (!WidthList) return FALSE;
			while( iCounter < 250 )
			{
				if (!LoadEncryptedDataFromFile("binarydata\\MissionBriefing.EDT",
					sString, FILE_STRING_SIZE * iCounter * 2,
					FILE_STRING_SIZE * 2))
				{
					ClearOutWidthRecordsList(WidthList);
					ClearFileStringList();
					return FALSE;
				}
				
				if (!AddStringToFilesList(sString))
				{
					ClearOutWidthRecordsList(WidthList);
					ClearFileStringList();
					return FALSE;
				}
				iCounter++;
			}

			pTempString = pFileStringList;


		iYPositionOnPage = 0;
			iCounter = 0;
			pLocatorString = pTempString;

			pTempString = GetFirstStringOnThisPage( pFileStringList,FILES_TEXT_FONT,	350, FILE_GAP, giFilesPage, MAX_FILE_MESSAGE_PAGE_SIZE, WidthList);

			// find out where this string is
			while( pLocatorString != pTempString )
			{
				iCounter++;
				pLocatorString = pLocatorString->Next;
			}


			// move through list and display
			while( pTempString )
			{
				uiFlags = IAN_WRAP_NO_SHADOW;
						// copy over string
				LaptopUiStateModel::CopyText(sString, pTempString->pString);

				if( sString[ 0 ] == 0 )
				{
					// on last page
					fOnLastFilesPageFlag = TRUE;
				}


				// set up font
				uiFont = FILES_TEXT_FONT;
				if( giFilesPage == 0 )
				{
				switch( iCounter )
					{
				 case( 0 ):
						uiFont = FILES_TITLE_FONT;
					break;

					}
				}
				
				if( iCounter == 0 )
				{			
				// reset width
				iFileLineWidth = 350;
				iFileStartX = (UINT16) ( FILE_VIEWER_X +	10 );
				}
				else
				{
				// reset width
				iFileLineWidth = 350;
				iFileStartX = (UINT16) ( FILE_VIEWER_X +	10 );
				}

				if( ( iYPositionOnPage + IanWrappedStringHeight(0, 0, ( UINT16 )iFileLineWidth, FILE_GAP,
															uiFont, 0, sString,
															0, 0, 0 ) )	< MAX_FILE_MESSAGE_PAGE_SIZE	)
				{
	 		// now print it
			iYPositionOnPage += ( INT32 )IanDisplayWrappedString((UINT16) ( iFileStartX ), ( UINT16 )( FILE_VIEWER_Y + iYPositionOnPage), ( INT16 )iFileLineWidth, FILE_GAP, uiFont, FILE_TEXT_COLOR, sString,0,FALSE, uiFlags );

				fGoingOffCurrentPage = FALSE;
				}
			else
				{
				// gonna get cut off...end now
				fGoingOffCurrentPage = TRUE;
				}

				pTempString = pTempString->Next;

				if( pTempString == NULL )
				{
					// on last page
					fOnLastFilesPageFlag = TRUE;
				}
				else
				{
					fOnLastFilesPageFlag = FALSE;
				}

				// going over the edge, stop now
				if( fGoingOffCurrentPage == TRUE )
				{
					pTempString = NULL;
				}
				iCounter++;
			}
			ClearOutWidthRecordsList( WidthList );
			ClearFileStringList( );
		break;
	}

	// place pictures
	// page 1 picture of country
	if( giFilesPage == 0 )
	{
		// title bar
		VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
		FilenameForBPP(CurrentFilesMapPath(), VObjectDesc.ImageFile);
		UniqueVideoObjectHandle picture = AddVideoObjectOwned(&VObjectDesc);
		CHECKF(picture);

		// get title bar object
	CHECKF(GetVideoFrame(picture.get(), 0, &hHandle));

	// blt title bar to screen
	BltVideoObject(FRAME_BUFFER, hHandle, 0,iScreenWidthOffset + 300, iScreenHeightOffset + 270, VO_BLT_SRCTRANSPARENCY,NULL);

	}
	else 	if( giFilesPage == 1 )
	{
		// title bar
		VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
		FilenameForBPP(CurrentFilesMapPath(), VObjectDesc.ImageFile);
		UniqueVideoObjectHandle picture = AddVideoObjectOwned(&VObjectDesc);
		CHECKF(picture);

		// get title bar object
	CHECKF(GetVideoFrame(picture.get(), 0, &hHandle));

	// blt title bar to screen
	BltVideoObject(FRAME_BUFFER, hHandle, 0,iScreenWidthOffset + 300, iScreenHeightOffset + 270, VO_BLT_SRCTRANSPARENCY,NULL);

	}

	return ( TRUE );
}

static BOOLEAN HandleAdditionalTextFile( FilesUnitPtr file )
{
	AdditionalFiles_Descriptor *descr = AdditionalFiles_LoadTextFile( file );

	if ( descr == NULL )
		return FALSE;

	// set up font
	INT32 uiFont = AdditionalFiles_GetFontHandler( descr->font );
	UINT8 ubFontColor = descr->textColor;
	UINT8 ubFontBgColor = descr->bkgdColor;
	UINT32 uiFlags = IAN_WRAP_NO_SHADOW;
	
	// Create list of width(s). Actually, text file view needs only one of it.
	FileRecordWidthPtr widthList = CreateRecordWidth( 0, FILE_VIEWER_WIDTH, 0, 0 );
	if (!widthList)
	{
		ClearFileStringList();
		return FALSE;
	}
	FileStringPtr pFileString = GetFirstStringOnThisPage( pFileStringList,
			uiFont, FILE_VIEWER_WIDTH, FILE_GAP, giFilesPage, MAX_TEXT_FILE_MESSAGE_PAGE_SIZE, widthList );

	// move through list and display
	UINT16 usPositionOnPage = 0;
	while ( pFileString )
	{
		UINT16 drawnStringHeight = IanWrappedStringHeight( FILE_VIEWER_X + 4, FILE_VIEWER_Y + usPositionOnPage,
				FILE_VIEWER_WIDTH, FILE_GAP, uiFont, ubFontColor, pFileString->pString, ubFontBgColor, FALSE, uiFlags );

		// if the string we are going to draw fits current page, then draw it, otherwise stop drawing current page
		if ( usPositionOnPage + drawnStringHeight < MAX_TEXT_FILE_MESSAGE_PAGE_SIZE )
		{
			usPositionOnPage += IanDisplayWrappedString( FILE_VIEWER_X + 4, FILE_VIEWER_Y + usPositionOnPage,
					FILE_VIEWER_WIDTH, FILE_GAP, uiFont, ubFontColor, pFileString->pString, ubFontBgColor, FALSE, uiFlags );
		}
		else
		{
			break;
		}

		pFileString = pFileString->Next;
	}

	if ( pFileString == NULL )
	{
		fOnLastFilesPageFlag = TRUE;
	}
	else
	{
		fOnLastFilesPageFlag = FALSE;
	}

	ClearOutWidthRecordsList( widthList );

	return TRUE;
}

//-------------------------------------------

BOOLEAN HandleSpecialFiles( UINT8 ubFormat )
{
	INT32 iCounter = 0;
	CHAR16 sString[2048];
	FileStringPtr pTempString = NULL ;
	FileStringPtr pLocatorString = NULL;
	INT32 iYPositionOnPage = 0;
	INT32 iFileLineWidth = 0;
	INT32 iFileStartX = 0;
	UINT32 uiFlags = 0;
	UINT32 uiFont = 0;
	BOOLEAN fGoingOffCurrentPage = FALSE;
	FileRecordWidthPtr WidthList = NULL;


	HVOBJECT hHandle;
	VOBJECT_DESC VObjectDesc;


	ClearFileStringList( );

	switch( ubFormat )
	{
		case( 255 ):
			// load data
			// read one record from file manager file

		const ContentPolicy policy = CurrentLaptopContentPolicy();
		const ContentPolicy::BriefingCatalog briefing = policy.briefingCatalog(
			FileExists(ContentPolicy::unfinishedBusinessBriefingPath()));
		WidthList = CreateWidthRecordsForAruloIntelFile( );
		if (!WidthList) return FALSE;
		while( iCounter < briefing.recordCount )
			{
			if (!LoadEncryptedDataFromFile(briefing.path, sString,
				FILE_STRING_SIZE * iCounter * 2, FILE_STRING_SIZE * 2))
			{
				ClearOutWidthRecordsList(WidthList);
				ClearFileStringList();
				return FALSE;
			}
				if (!AddStringToFilesList(sString))
				{
					ClearOutWidthRecordsList(WidthList);
					ClearFileStringList();
					return FALSE;
				}
				iCounter++;
			}

			pTempString = pFileStringList;


		iYPositionOnPage = 0;
			iCounter = 0;
			pLocatorString = pTempString;

			pTempString = GetFirstStringOnThisPage( pFileStringList,FILES_TEXT_FONT,	350, FILE_GAP, giFilesPage, MAX_FILE_MESSAGE_PAGE_SIZE, WidthList);

			// find out where this string is
			while( pLocatorString != pTempString )
			{
				iCounter++;
				pLocatorString = pLocatorString->Next;
			}


			// move through list and display
			while( pTempString )
			{
				uiFlags = IAN_WRAP_NO_SHADOW;
						// copy over string
				LaptopUiStateModel::CopyText(sString, pTempString->pString);

				if( sString[ 0 ] == 0 )
				{
					// on last page
					fOnLastFilesPageFlag = TRUE;
				}


				// set up font
				uiFont = FILES_TEXT_FONT;
				if( giFilesPage == 0 )
				{
				switch( iCounter )
					{
				 case( 0 ):
						uiFont = FILES_TITLE_FONT;
					break;

					}
				}

				// based on the record we are at, selected X start position and the width to wrap the line, to fit around pictures

				if( iCounter == 0 )
				{
					// title
					iFileLineWidth = 350;
					iFileStartX = (UINT16) ( FILE_VIEWER_X	+	10 );

				}
				else if( iCounter == 1 )
				{
					// opening on first page
					iFileLineWidth = 350;
					iFileStartX = (UINT16) ( FILE_VIEWER_X	+	10 );

				}
				else if( ( iCounter > 1) &&( iCounter < FILES_COUNTER_1_WIDTH ) )
				{
					iFileLineWidth = 350;
					iFileStartX = (UINT16) ( FILE_VIEWER_X	+	10 );

				}
				else if( iCounter == FILES_COUNTER_1_WIDTH )
				{
					if( giFilesPage == 0 )
					{
						iYPositionOnPage += ( MAX_FILE_MESSAGE_PAGE_SIZE - iYPositionOnPage );
					}
					iFileLineWidth = 350;
					iFileStartX = (UINT16) ( FILE_VIEWER_X	+	10 );
				}

				else if( iCounter == FILES_COUNTER_2_WIDTH )
				{
					iFileLineWidth = 200;
					iFileStartX = (UINT16) ( FILE_VIEWER_X	+	150 );
				}
				else if( iCounter == FILES_COUNTER_3_WIDTH )
				{
					iFileLineWidth = 200;
					iFileStartX = (UINT16) ( FILE_VIEWER_X	+	150 );
				}

				else
				{
					iFileLineWidth = 350;
					iFileStartX = (UINT16) ( FILE_VIEWER_X +	10 );
				}
				// not far enough, advance

				if( ( iYPositionOnPage + IanWrappedStringHeight(0, 0, ( UINT16 )iFileLineWidth, FILE_GAP,
															uiFont, 0, sString,
															0, 0, 0 ) )	< MAX_FILE_MESSAGE_PAGE_SIZE	)
				{
	 		// now print it
			iYPositionOnPage += ( INT32 )IanDisplayWrappedString((UINT16) ( iFileStartX ), ( UINT16 )( FILE_VIEWER_Y + iYPositionOnPage), ( INT16 )iFileLineWidth, FILE_GAP, uiFont, FILE_TEXT_COLOR, sString,0,FALSE, uiFlags );

				fGoingOffCurrentPage = FALSE;
				}
			else
				{
				// gonna get cut off...end now
				fGoingOffCurrentPage = TRUE;
				}

				pTempString = pTempString->Next;

				if( pTempString == NULL )
				{
					// on last page
					fOnLastFilesPageFlag = TRUE;
				}
				else
				{
					fOnLastFilesPageFlag = FALSE;
				}

				// going over the edge, stop now
				if( fGoingOffCurrentPage == TRUE )
				{
					pTempString = NULL;
				}
				iCounter++;
			}
			ClearOutWidthRecordsList( WidthList );
			ClearFileStringList( );
		break;
	}

	// place pictures
	// page 1 picture of country
	if( giFilesPage == 0 )
	{
		// title bar
		VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
		FilenameForBPP(CurrentFilesMapPath(), VObjectDesc.ImageFile);
		UniqueVideoObjectHandle picture = AddVideoObjectOwned(&VObjectDesc);
		CHECKF(picture);

		// get title bar object
	CHECKF(GetVideoFrame(picture.get(), 0, &hHandle));

	// blt title bar to screen
	BltVideoObject(FRAME_BUFFER, hHandle, 0,iScreenWidthOffset + 300, iScreenHeightOffset + 270, VO_BLT_SRCTRANSPARENCY,NULL);

	}
	else if( const char* biographyPicture =
		CurrentLaptopContentPolicy().biographyPicturePath(giFilesPage) )
	{
		VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
		FilenameForBPP(biographyPicture, VObjectDesc.ImageFile);
		UniqueVideoObjectHandle picture = AddVideoObjectOwned(&VObjectDesc);
		CHECKF(picture);

		// get title bar object
		CHECKF(GetVideoFrame(picture.get(), 0, &hHandle));

	// blt title bar to screen
	BltVideoObject(FRAME_BUFFER, hHandle, 0,iScreenWidthOffset + 260,
		giFilesPage == 4 ? iScreenHeightOffset + 225 : iScreenHeightOffset + 85,
		VO_BLT_SRCTRANSPARENCY,NULL);


	}
	return ( TRUE );
}


BOOLEAN AddStringToFilesList( STR16 pString )
{
	if (!pString) return FALSE;

	FileStringPtr pFileString;
	FileStringPtr pTempString = pFileStringList;

	// create string structure
	pFileString = (FileStringPtr) MemAlloc( sizeof( FileString ) );
	if (!pFileString) return FALSE;


	// Allocate the terminator in CHAR16 units as well. The former +2 byte
	// allocation overflowed on platforms where CHAR16 is wider than two bytes.
	const std::size_t length = wcslen(pString);
	pFileString->pString = static_cast<CHAR16*>(
		MemAlloc((length + 1) * sizeof(CHAR16)));
	if (!pFileString->pString)
	{
		MemFree(pFileString);
		return FALSE;
	}
	memcpy(pFileString->pString, pString, length * sizeof(CHAR16));
	pFileString->pString[length] = 0;

	// set Next to NULL

	pFileString->Next = NULL;
	if( pFileStringList == NULL )
	{
		pFileStringList = pFileString;
	}
	else
	{
		while( pTempString->Next )
		{
			pTempString = pTempString->Next;
		}
		pTempString->Next = pFileString;
	}


	return TRUE;
}


void ClearFileStringList( void )
{
	FileStringPtr pFileString = pFileStringList;
	while (pFileString)
	{
		FileStringPtr next = pFileString->Next;
		MemFree(pFileString->pString);
		MemFree(pFileString);
		pFileString = next;
	}

	pFileStringList = NULL;
}


BOOLEAN CreateButtonsForFilesPage(LaptopPageResourceOwner& owner)
{
	// will create buttons for the files page
	CHECKF(owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\arrows.sti", -1, 0, -1, 1, -1),
		giFilesPageButtonsImage[0]));
	CHECKF(owner.addButton(QuickCreateButton( giFilesPageButtonsImage[0], PREVIOUS_FILE_PAGE_BUTTON_X,	PREVIOUS_FILE_PAGE_BUTTON_Y,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)BtnPreviousFilePageCallback ),
		giFilesPageButtons[0]));

	CHECKF(owner.addButtonImage(LoadButtonImageOwned(
		"LAPTOP\\arrows.sti", -1, 6, -1, 7, -1),
		giFilesPageButtonsImage[1]));
	CHECKF(owner.addButton(QuickCreateButton( giFilesPageButtonsImage[ 1 ], NEXT_FILE_PAGE_BUTTON_X,	NEXT_FILE_PAGE_BUTTON_Y ,
										BUTTON_TOGGLE, MSYS_PRIORITY_HIGHEST - 1,
										(GUI_CALLBACK)BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)BtnNextFilePageCallback ),
		giFilesPageButtons[1]));

	SetButtonCursor(giFilesPageButtons[ 0 ], CURSOR_LAPTOP_SCREEN);
	SetButtonCursor(giFilesPageButtons[ 1 ], CURSOR_LAPTOP_SCREEN);

	return TRUE;
}


// callbacks
void BtnPreviousFilePageCallback(GUI_BUTTON *btn,INT32 reason)
{
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{

		if( fWaitAFrame == TRUE )
		{
			return;
		}

		if(!(btn->uiFlags & BUTTON_CLICKED_ON))
		{
			btn->uiFlags|=(BUTTON_CLICKED_ON);
		}

	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{

		if( fWaitAFrame == TRUE )
		{
			return;
		}

		if((btn->uiFlags & BUTTON_CLICKED_ON))
		{

			if( giFilesPage > 0 )
			{
			giFilesPage--;
				fWaitAFrame = TRUE;
			}

			fReDrawScreenFlag = TRUE;
			btn->uiFlags&=~(BUTTON_CLICKED_ON);
			MarkButtonsDirty( );
		}
	}

	return;
}


void BtnNextFilePageCallback(GUI_BUTTON *btn,INT32 reason)
{
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		if( fWaitAFrame == TRUE )
		{
			return;
		}

		if(!(btn->uiFlags & BUTTON_CLICKED_ON))
		{
			btn->uiFlags|=(BUTTON_CLICKED_ON);
		}

	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if( fWaitAFrame == TRUE )
		{
			return;
		}

		if((btn->uiFlags & BUTTON_CLICKED_ON))
		{

			if(	( fOnLastFilesPageFlag ) == FALSE )
			{
				fWaitAFrame = TRUE;
				giFilesPage++;
			}

			fReDrawScreenFlag = TRUE;
			btn->uiFlags&=~(BUTTON_CLICKED_ON);
			MarkButtonsDirty( );
		}
	}

	return;
}

void HandleFileViewerButtonStates( void )
{
	// handle state of email viewer buttons

	if( iHighLightFileLine == -1 )
	{
		// not displaying message, leave
		DisableButton( giFilesPageButtons[ 0 ] );
		DisableButton( giFilesPageButtons[ 1 ] );
		ButtonList[ giFilesPageButtons[ 0 ] ]->uiFlags &= ~( BUTTON_CLICKED_ON );
		ButtonList[ giFilesPageButtons[ 1 ] ]->uiFlags &= ~( BUTTON_CLICKED_ON );


		return;
	}

	// turn off previous page button
	if( giFilesPage == 0 )
	{
		DisableButton( giFilesPageButtons[ 0 ] );
		ButtonList[ giFilesPageButtons[ 0 ] ]->uiFlags &= ~( BUTTON_CLICKED_ON );

	}
	else
	{
		EnableButton( giFilesPageButtons[ 0 ] );
	}


	// turn off next page button
	if( fOnLastFilesPageFlag == TRUE )
	{
		DisableButton( giFilesPageButtons[ 1 ] );
		ButtonList[ giFilesPageButtons[ 1 ] ]->uiFlags &= ~( BUTTON_CLICKED_ON );
	}
	else
	{
		EnableButton( giFilesPageButtons[ 1 ] );
	}

	return;

}


FileRecordWidthPtr CreateRecordWidth( 	INT32 iRecordNumber, INT32 iRecordWidth, INT32 iRecordHeightAdjustment, UINT8 ubFlags)
{
	FileRecordWidthPtr pTempRecord = NULL;

	// allocs and inits a width info record for the multipage file viewer...this will tell the procedure that does inital computation on which record is the start of the current page
	// how wide special records are ( ones that share space with pictures )
	pTempRecord = (FileRecordWidthPtr) MemAlloc( sizeof( FileRecordWidth ) );
	if (!pTempRecord) return NULL;

	pTempRecord->Next = NULL;
	pTempRecord->iRecordNumber = iRecordNumber;
	pTempRecord->iRecordWidth = iRecordWidth;
	pTempRecord->iRecordHeightAdjustment = iRecordHeightAdjustment;
	pTempRecord->ubFlags = ubFlags;

	return ( pTempRecord );
}

FileRecordWidthPtr CreateWidthRecordsForAruloIntelFile( void )
{
	// this fucntion will create the width list for the Arulco intelligence file
	FileRecordWidthPtr pTempRecord = NULL;
	FileRecordWidthPtr pRecordListHead = NULL;


		// first record width
//	pTempRecord = CreateRecordWidth( 7, 350, 200,0 );
	pTempRecord = CreateRecordWidth( FILES_COUNTER_1_WIDTH, 350, MAX_FILE_MESSAGE_PAGE_SIZE,0 );
	if (!pTempRecord) return NULL;

	// set up head of list now
	pRecordListHead = pTempRecord;

	// next record
//	pTempRecord->Next = CreateRecordWidth( 43, 200,0, 0 );
	pTempRecord->Next = CreateRecordWidth( FILES_COUNTER_2_WIDTH, 200,0, 0 );
	if (!pTempRecord->Next)
	{
		ClearOutWidthRecordsList(pRecordListHead);
		return NULL;
	}
	pTempRecord = pTempRecord->Next;

	// and the next..
//	pTempRecord->Next = CreateRecordWidth( 45, 200,0, 0 );
	pTempRecord->Next = CreateRecordWidth( FILES_COUNTER_3_WIDTH, 200,0, 0 );
	if (!pTempRecord->Next)
	{
		ClearOutWidthRecordsList(pRecordListHead);
		return NULL;
	}

	return( pRecordListHead );

}

FileRecordWidthPtr CreateWidthRecordsForTerroristFile( void )
{
	// this fucntion will create the width list for the Arulco intelligence file
	FileRecordWidthPtr pTempRecord = NULL;
	FileRecordWidthPtr pRecordListHead = NULL;


		// first record width
	pTempRecord = CreateRecordWidth( 4, 170, 0,0 );
	if (!pTempRecord) return NULL;

	// set up head of list now
	pRecordListHead = pTempRecord;

	// next record
	pTempRecord->Next = CreateRecordWidth( 5, 170,0, 0 );
	if (!pTempRecord->Next)
	{
		ClearOutWidthRecordsList(pRecordListHead);
		return NULL;
	}
	pTempRecord = pTempRecord->Next;

	pTempRecord->Next = CreateRecordWidth( 6, 170,0, 0 );
	if (!pTempRecord->Next)
	{
		ClearOutWidthRecordsList(pRecordListHead);
		return NULL;
	}

	return( pRecordListHead );

}


void ClearOutWidthRecordsList( FileRecordWidthPtr pFileRecordWidthList )
{
	FileRecordWidthPtr pTempRecord = NULL;
	FileRecordWidthPtr pDeleteRecord = NULL;

	// set up to head of the list
	pTempRecord = pFileRecordWidthList;

	// error check
	if( pFileRecordWidthList == NULL )
	{
		return;
	}

	while( pTempRecord->Next )
	{
		// set up delete record
		pDeleteRecord = pTempRecord;

		// move to next record
		pTempRecord = pTempRecord->Next;

		MemFree( pDeleteRecord );
	}

	// now get the last element
	MemFree( pTempRecord );

	// null out passed ptr
	pFileRecordWidthList = NULL;


	return;
}


void OpenFirstUnreadFile( void )
{
	// open the first unread file in the list
	INT32 iCounter = 0;
	FilesUnitPtr pFilesList=pFilesListHead;

	// make sure is a valid
	while( pFilesList )
	{

		// if iCounter = iFileId, is a valid file
	 if(	pFilesList->fRead == FALSE )
		{
			iHighLightFileLine = iCounter;
			break;
		}

		// next element in list
		pFilesList = pFilesList->Next;

		// increment counter
		iCounter++;
	}

	return;
}


void CheckForUnreadFiles( void )
{
	BOOLEAN	fStatusOfNewFileFlag = fNewFilesInFileViewer;

	// willc heck for any unread files and set flag if any
	FilesUnitPtr pFilesList=pFilesListHead;

	fNewFilesInFileViewer = FALSE;


	while( pFilesList )
	{
		// unread?...if so, set flag
		if( pFilesList->fRead == FALSE )
		{
			fNewFilesInFileViewer = TRUE;
		}
		// next element in list
		pFilesList = pFilesList->Next;
	}

	//if the old flag and the new flag arent the same, either create or destory the fast help region
	if( fNewFilesInFileViewer != fStatusOfNewFileFlag )
	{
		CreateFileAndNewEmailIconFastHelpText( LAPTOP_BN_HLP_TXT_YOU_HAVE_NEW_FILE, (BOOLEAN)!fNewFilesInFileViewer );
	}
}

BOOLEAN HandleSpecialTerroristFile( INT32 iFileNumber, STR sPictureName )
{

	INT32 iCounter = 0;
	CHAR16 sString[2048];
	FileStringPtr pTempString = NULL ;
	FileStringPtr pLocatorString = NULL;
	INT32 iYPositionOnPage = 0;
	INT32 iFileLineWidth = 0;
	INT32 iFileStartX = 0;
	UINT32 uiFlags = 0;
	UINT32 uiFont = 0;
	BOOLEAN fGoingOffCurrentPage = FALSE;
	FileRecordWidthPtr WidthList = NULL;
	INT32 iOffset = 0;
	HVOBJECT hHandle;
	VOBJECT_DESC VObjectDesc;
	CHAR sTemp[ 128 ];

	if ( iFileNumber < 0 || iFileNumber > LAST_JA2_VANILLA_FILE )   // bounds the 7-element ubFileOffsets/ubFileRecordsLength + the +1 profile-id array below
		return( FALSE );

	iOffset = ubFileOffsets[ iFileNumber ] ;

	// grab width list
	WidthList = CreateWidthRecordsForTerroristFile( );
	if (!WidthList) return FALSE;


	while( iCounter < ubFileRecordsLength[ iFileNumber ] )
	{
		if (!LoadEncryptedDataFromFile("BINARYDATA\\files.EDT", sString,
			FILE_STRING_SIZE * (iOffset + iCounter) * 2,
			FILE_STRING_SIZE * 2))
		{
			ClearOutWidthRecordsList(WidthList);
			ClearFileStringList();
			return FALSE;
		}
		if (!AddStringToFilesList(sString))
		{
			ClearOutWidthRecordsList(WidthList);
			ClearFileStringList();
			return FALSE;
		}
		iCounter++;
	}

	pTempString = pFileStringList;


	iYPositionOnPage = 0;
	iCounter = 0;
	pLocatorString = pTempString;

	pTempString = GetFirstStringOnThisPage( pFileStringList,FILES_TEXT_FONT,	350, FILE_GAP, giFilesPage, MAX_FILE_MESSAGE_PAGE_SIZE, WidthList);

		// find out where this string is
		while( pLocatorString != pTempString )
		{
			iCounter++;
			pLocatorString = pLocatorString->Next;
		}


		// move through list and display
		while( pTempString )
		{
			uiFlags = IAN_WRAP_NO_SHADOW;
					// copy over string
			LaptopUiStateModel::CopyText(sString, pTempString->pString);

			if( sString[ 0 ] == 0 )
			{
				// on last page
				fOnLastFilesPageFlag = TRUE;
			}


			// set up font
			uiFont = FILES_TEXT_FONT;
			if( giFilesPage == 0 )
			{
				switch( iCounter )
				{
				case( 0 ):
						uiFont = FILES_TITLE_FONT;
				break;

				}
			}

			if( ( iCounter > 3 ) && ( iCounter < 7 ) )
			{
				iFileLineWidth = 170;
				iFileStartX = (UINT16) ( FILE_VIEWER_X	+	180 );
			}
			else
			{
				// reset width
				iFileLineWidth = 350;
				iFileStartX = (UINT16) ( FILE_VIEWER_X +	10 );
			}

			// based on the record we are at, selected X start position and the width to wrap the line, to fit around pictures
			if( ( iYPositionOnPage + IanWrappedStringHeight(0, 0, ( UINT16 )iFileLineWidth, FILE_GAP,
															uiFont, 0, sString,
														0, 0, 0 ) )	< MAX_FILE_MESSAGE_PAGE_SIZE	)
			{
	 	// now print it
			iYPositionOnPage += ( INT32 )IanDisplayWrappedString((UINT16) ( iFileStartX ), ( UINT16 )( FILE_VIEWER_Y + iYPositionOnPage), ( INT16 )iFileLineWidth, FILE_GAP, uiFont, FILE_TEXT_COLOR, sString,0,FALSE, uiFlags );

				fGoingOffCurrentPage = FALSE;
			}
			else
			{
				// gonna get cut off...end now
				fGoingOffCurrentPage = TRUE;
			}

			pTempString = pTempString->Next;

			if( ( pTempString == NULL ) && ( fGoingOffCurrentPage == FALSE ) )
			{
				// on last page
				fOnLastFilesPageFlag = TRUE;
			}
			else
			{
				fOnLastFilesPageFlag = FALSE;
			}

			// going over the edge, stop now
			if( fGoingOffCurrentPage == TRUE )
			{
				pTempString = NULL;
			}

			// show picture
			if( ( giFilesPage == 0 ) && ( iCounter == 5 ) )
			{
				const UINT16 profileId =
					usProfileIdsForTerroristFiles[iFileNumber + 1];
				if (!LaptopUiStateModel::IsValidIndex(NUM_PROFILES, profileId))
				{
					ClearOutWidthRecordsList(WidthList);
					ClearFileStringList();
					return FALSE;
				}
				if (gGameExternalOptions.fReadProfileDataFromXML)
				{
					// HEADROCK PROFEX: Do not read direct profile number, instead, look inside the profile for a ubFaceIndex value.
					std::snprintf(sTemp, sizeof(sTemp),
						"FACES\\BIGFACES\\%02d.sti",
						gMercProfiles[profileId].ubFaceIndex);
				}
				else
				{
					std::snprintf(sTemp, sizeof(sTemp),
						"FACES\\BIGFACES\\%02d.sti", profileId);
				}

				VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
				FilenameForBPP(sTemp, VObjectDesc.ImageFile);
				UniqueVideoObjectHandle picture = AddVideoObjectOwned(&VObjectDesc);
				CHECKF(picture);

				//Blt face to screen to
				CHECKF(GetVideoFrame(picture.get(), 0, &hHandle));

//def: 3/24/99
//				BltVideoObject(FRAME_BUFFER, hHandle, 0,( INT16 ) (	FILE_VIEWER_X +	30 ), ( INT16 ) ( iYPositionOnPage + 5), VO_BLT_SRCTRANSPARENCY,NULL);
				BltVideoObject(FRAME_BUFFER, hHandle, 0,( INT16 ) (	FILE_VIEWER_X +	30 ), ( INT16 ) ( iScreenHeightOffset + iYPositionOnPage + 21), VO_BLT_SRCTRANSPARENCY,NULL);

				VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
				FilenameForBPP("LAPTOP\\InterceptBorder.sti", VObjectDesc.ImageFile);
				picture = AddVideoObjectOwned(&VObjectDesc);
				CHECKF(picture);

				//Blt face to screen to
				CHECKF(GetVideoFrame(picture.get(), 0, &hHandle));

				BltVideoObject(FRAME_BUFFER, hHandle, 0,( INT16 ) (	FILE_VIEWER_X +	25 ), ( INT16 ) ( iScreenHeightOffset + iYPositionOnPage + 16 ), VO_BLT_SRCTRANSPARENCY,NULL);

			}

			iCounter++;
		}



		ClearOutWidthRecordsList( WidthList );
		ClearFileStringList( );

		return( TRUE );
}

// add a file about this terrorist
BOOLEAN AddFileAboutTerrorist( INT32 iProfileId )
{
	INT32 iCounter = 0;

	for( iCounter = 1; iCounter < 7; iCounter++ )
	{
		if( usProfileIdsForTerroristFiles[ iCounter ] == iProfileId )
		{
			// checked, and this file is there
				AddFilesToPlayersLog( ( UINT8 )iCounter, 0, 3, NULL, NULL );
				return( TRUE );
		}
	}

	return( FALSE );
}







