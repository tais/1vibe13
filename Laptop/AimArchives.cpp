
	#include "laptop.h"
	#include "AimArchives.h"
	#include "aim.h"
	#include "WordWrap.h"
	#include "Utilities.h"
	#include "WCheck.h"
	#include "Text.h"
	#include "LaptopPageResourceOwner.h"
	#include "LaptopSafety.h"
	#include "AimWebsiteLayout.h"

#include "Soldier Profile.h"


#define		AIM_ALUMNI_TITLE_FONT					FONT14ARIAL
#define		AIM_ALUMNI_TITLE_COLOR				AIM_GREEN

#define		AIM_ALUMNI_POPUP_FONT					FONT10ARIAL
#define		AIM_ALUMNI_POPUP_COLOR				FONT_MCOLOR_WHITE

#define		AIM_ALUMNI_POPUP_NAME_FONT		FONT12ARIAL
#define		AIM_ALUMNI_POPUP_NAME_COLOR		FONT_MCOLOR_WHITE

#define		AIM_ALUMNI_NAME_FONT					FONT12ARIAL
#define		AIM_ALUMNI_NAME_COLOR					FONT_MCOLOR_WHITE
#define		AIM_ALUMNI_PAGE_FONT					FONT14ARIAL
#define		AIM_ALUMNI_PAGE_COLOR_UP			FONT_MCOLOR_DKWHITE
#define		AIM_ALUMNI_PAGE_COLOR_DOWN		138

constexpr std::size_t MAX_NUMBER_OLD_MERCS_ON_PAGE =
	AimWebsiteLayoutModel::kArchivePageCapacity;
constexpr std::size_t NUM_AIM_ARCHIVE_PAGES =
	AimWebsiteLayoutModel::kArchivePageCount;

UINT32		guiAlumniFrame;
UINT32		guiOldAim;
UINT32		guiAlumniPopUp;
UINT32		guiPopUpPic;
UINT32		guiDoneButton;

UINT8			gubPageNum;
UINT8			gubDrawOldMerc;
UINT8			gfDrawPopUpBox=FALSE;
BOOLEAN		gfDestroyPopUpBox;
BOOLEAN		gfFaceMouseRegionsActive;
//BOOLEAN		gfDestroyDoneRegion;
BOOLEAN		gfReDrawScreen=FALSE;

BOOLEAN		AimArchivesSubPagesVisitedFlag[NUM_AIM_ARCHIVE_PAGES] = {};

	//Mouse Regions

//Face regions
MOUSE_REGION		gMercAlumniFaceMouseRegions[ MAX_NUMBER_OLD_MERCS_ON_PAGE ];
void SelectAlumniFaceRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//Done region
MOUSE_REGION		gDoneRegion;
void SelectAlumniDoneRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason );

//Previous Button
void		BtnAlumniPageButtonCallback(GUI_BUTTON *btn,INT32 reason);
UINT32	guiAlumniPageButton;
INT32		guiAlumniPageButtonImage;

BOOLEAN pageEnabled[NUM_AIM_ARCHIVE_PAGES];

void DisplayAlumniOldMercPopUp();
void DestroyPopUpBox();
void InitAlumniFaceRegions();
void RemoveAimAlumniFaceRegion();
void CreateDestroyDoneMouseRegion(const LaptopLayoutModel::Rect* bounds);
void ChangingAimArchiveSubPage( UINT8 ubSubPageNumber );

BOOLEAN vOldMerc[NUM_PROFILES];
OLD_MERC_ARCHIVES_VALUES gAimOldArchives[NUM_PROFILES];

namespace
{
LaptopPageResourceOwner gAimArchivesResources;
LaptopPageResourceOwner gAimArchivesFaceResources;
LaptopPageResourceOwner gAimArchivesDoneResources;

AimWebsiteLayoutModel::ArchiveLayout CurrentAimArchiveLayout()
{
	return AimWebsiteLayoutModel::MakeArchiveLayout(
		{iScreenWidthOffset, iScreenHeightOffset,
		 IMAGE_OFFSET_X, IMAGE_OFFSET_Y, LAPTOP_SCREEN_WEB_DELTA_Y});
}
}

void GameInitAimArchives()
{

}

void EnterInitAimArchives()
{
	gfDrawPopUpBox=FALSE;
	gfDestroyPopUpBox = FALSE;

	memset(AimArchivesSubPagesVisitedFlag, 0,
		sizeof(AimArchivesSubPagesVisitedFlag));
	AimArchivesSubPagesVisitedFlag[0] = TRUE;
}


BOOLEAN EnterAimArchives()
{
	VOBJECT_DESC	VObjectDesc;
	LaptopPageResourceOwner stagedResources;
	const auto layout = CurrentAimArchiveLayout();
	std::size_t enabledPageCount = 0;

	gAimArchivesResources.clear();
	RemoveAimAlumniFaceRegion();
	CreateDestroyDoneMouseRegion(nullptr);

	for(std::size_t i = 0; i < NUM_PROFILES; ++i)
	{
		vOldMerc[i] = gAimOldArchives[i].FaceID != -1;
	}
	for (std::size_t page = 0; page < NUM_AIM_ARCHIVE_PAGES; ++page)
	{
		pageEnabled[page] = AimWebsiteLayoutModel::ArchivePageHasVisible(
			vOldMerc, NUM_PROFILES, page);
		if (pageEnabled[page]) ++enabledPageCount;
	}
	
	
	gfDrawPopUpBox=FALSE;
	gfDestroyPopUpBox=FALSE;

	gubPageNum = IsValidLaptopIndex(
		NUM_AIM_ARCHIVE_PAGES, giCurrentSubPage)
		? static_cast<UINT8>(giCurrentSubPage) : 0;
	if (!pageEnabled[gubPageNum])
		gubPageNum = static_cast<UINT8>(
			AimWebsiteLayoutModel::NextArchivePage(
				gubPageNum, pageEnabled, NUM_AIM_ARCHIVE_PAGES));

	// load the Alumni Frame and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\AlumniFrame.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiAlumniFrame));

	// load the 1st set of faces and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\Old_Aim.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiOldAim));

	// load the PopupPic graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\PopupPicFrame.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiPopUpPic));

		// load the AlumniPopUp graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\AlumniPopUp.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiAlumniPopUp));

		// load the Done Button graphic and add it
	VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("LAPTOP\\DoneButton.sti", VObjectDesc.ImageFile);
	CHECKF(stagedResources.addVideoObject(&VObjectDesc, guiDoneButton));

	if (enabledPageCount > 1)
	{
		CHECKF(stagedResources.addButtonImage(
			LoadButtonImageOwned("LAPTOP\\BottomButtons2.sti", -1,0,-1,1,-1),
			guiAlumniPageButtonImage));
		const INT32 pageButton = CreateIconAndTextButton( guiAlumniPageButtonImage, AimAlumniText[5], AIM_ALUMNI_PAGE_FONT,
												AIM_ALUMNI_PAGE_COLOR_UP, DEFAULT_SHADOW,
												AIM_ALUMNI_PAGE_COLOR_DOWN, DEFAULT_SHADOW,
												TEXT_CJUSTIFIED,
												layout.pageButton.x, layout.pageButton.y, BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
												DEFAULT_MOVE_CALLBACK, BtnAlumniPageButtonCallback);
		CHECKF(stagedResources.addButton(pageButton, guiAlumniPageButton));
		SetButtonCursor(guiAlumniPageButton, CURSOR_WWW);
		MSYS_SetBtnUserData(guiAlumniPageButton, 0, 0);
	}

	CHECKF(InitAimDefaults());
	if (!InitAimMenuBar())
	{
		RemoveAimDefaults();
		return FALSE;
	}

	gAimArchivesResources = std::move(stagedResources);
	InitAlumniFaceRegions();

	RenderAimArchives();
	return(TRUE);
}

void ExitAimArchives()
{
//	UINT16 i;

	RemoveAimAlumniFaceRegion();
	gAimArchivesResources.clear();

	RemoveAimDefaults();
	ExitAimMenuBar();
	giCurrentSubPage = gubPageNum;

	CreateDestroyDoneMouseRegion(nullptr);
	gfDestroyPopUpBox = FALSE;
	gfDrawPopUpBox = FALSE;
}

void HandleAimArchives()
{
	if( gfReDrawScreen )
	{
//		RenderAimArchives();
		fPausedReDrawScreenFlag = TRUE;

		gfReDrawScreen = FALSE;
	}
	if( gfDestroyPopUpBox )
	{
		gfDestroyPopUpBox = FALSE;

		CreateDestroyDoneMouseRegion(nullptr);
		InitAlumniFaceRegions();
		gfDestroyPopUpBox = FALSE;
	}
}

void RenderAimArchives()
{
	HVOBJECT	hFrameHandle;
	HVOBJECT	hFaceHandle;
	const auto layout = CurrentAimArchiveLayout();

	DrawAimDefaults();
	DisableAimButton();

	DrawTextToScreen(AimAlumniText[AIM_ALUMNI_ALUMNI],
		layout.title.origin.x, layout.title.origin.y, layout.title.width,
		AIM_ALUMNI_TITLE_FONT, AIM_ALUMNI_TITLE_COLOR, FONT_MCOLOR_BLACK,
		FALSE, CENTER_JUSTIFIED);

	GetVideoObject(&hFrameHandle, guiAlumniFrame);
	GetVideoObject(&hFaceHandle, guiOldAim);
	for (std::size_t slot = 0; slot < layout.grid.capacity(); ++slot)
	{
		const std::size_t profile = AimWebsiteLayoutModel::ArchiveProfileIndex(
			gubPageNum, slot, NUM_PROFILES);
		if (profile == NUM_PROFILES || !vOldMerc[profile])
			continue;
		const auto frame = layout.grid.frame(slot);
		const auto face = layout.grid.face(slot);
		const auto nickname = layout.grid.nickname(slot);
		BltVideoObject(FRAME_BUFFER, hFaceHandle,
			gAimOldArchives[profile].FaceID, face.x, face.y,
			VO_BLT_SRCTRANSPARENCY, NULL);
		BltVideoObject(FRAME_BUFFER, hFrameHandle, 0, frame.x, frame.y,
			VO_BLT_SRCTRANSPARENCY, NULL);
		DrawTextToScreen(gAimOldArchives[profile].szNickName,
			nickname.origin.x, nickname.origin.y, nickname.width,
			AIM_ALUMNI_NAME_FONT, AIM_ALUMNI_NAME_COLOR,
			FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	}

	if( gfDrawPopUpBox )
	{
		DisplayAlumniOldMercPopUp();
		RemoveAimAlumniFaceRegion();
	}


	MarkButtonsDirty( );

	RenderWWWProgramTitleBar( );

	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}





void SelectAlumniFaceRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (!(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP))
		return;
	const std::size_t slot = MSYS_GetRegionUserData(pRegion, 0);
	const std::size_t profile = AimWebsiteLayoutModel::ArchiveProfileIndex(
		gubPageNum, slot, NUM_PROFILES);
	if (profile == NUM_PROFILES || !vOldMerc[profile])
		return;
	gubDrawOldMerc = static_cast<UINT8>(profile);
	gfDrawPopUpBox = TRUE;
	gfReDrawScreen = TRUE;
}


void BtnAlumniPageButtonCallback(GUI_BUTTON *btn,INT32 reason)
{
	const auto invalidation =
		CurrentAimArchiveLayout().pageControlsInvalidation;
	if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if (btn->uiFlags & BUTTON_CLICKED_ON)
		{
			btn->uiFlags &= (~BUTTON_CLICKED_ON );
			const std::size_t nextPage =
				AimWebsiteLayoutModel::NextArchivePage(
					gubPageNum, pageEnabled, NUM_AIM_ARCHIVE_PAGES);
			if (nextPage == gubPageNum)
				return;

			RemoveAimAlumniFaceRegion();
			ChangingAimArchiveSubPage(static_cast<UINT8>(nextPage));
			gubPageNum = static_cast<UINT8>(nextPage);
			gfReDrawScreen = TRUE;
			gfDestroyPopUpBox = TRUE;
			gfDrawPopUpBox = FALSE;
			InvalidateRegion(invalidation.x, invalidation.y,
				invalidation.right(), invalidation.bottom());
		}
	}
	if(reason & MSYS_CALLBACK_REASON_LOST_MOUSE)
	{
		btn->uiFlags &= (~BUTTON_CLICKED_ON );
		InvalidateRegion(invalidation.x, invalidation.y,
			invalidation.right(), invalidation.bottom());
	}
}


void DisplayAlumniOldMercPopUp()
{
	HVOBJECT	hAlumniPopUpHandle;
	HVOBJECT	hDoneHandle;
	HVOBJECT	hFacePaneHandle;
	HVOBJECT	hFaceHandle;
	const auto layout = CurrentAimArchiveLayout();
	const auto& popup = layout.popup;
	if (gubDrawOldMerc >= NUM_PROFILES || !vOldMerc[gubDrawOldMerc])
		return;

	GetVideoObject(&hAlumniPopUpHandle, guiAlumniPopUp);
	GetVideoObject(&hDoneHandle, guiDoneButton);
	GetVideoObject(&hFacePaneHandle, guiPopUpPic);
	GetVideoObject(&hFaceHandle, guiOldAim);

	const std::size_t descriptionLines = StringPixLength(
		gAimOldArchives[gubDrawOldMerc].szBio,
		AIM_ALUMNI_POPUP_FONT) / popup.textWidth;
	const std::size_t middleSections = 11 + descriptionLines;

	const auto top = popup.section(0);
	const auto topShadow = popup.shadow(0);
	ShadowVideoSurfaceRect(FRAME_BUFFER,
		topShadow.x, topShadow.y, topShadow.right(), topShadow.bottom());
	BltVideoObject(FRAME_BUFFER, hAlumniPopUpHandle, 0, top.x, top.y,
		VO_BLT_SRCTRANSPARENCY, NULL);
	for(std::size_t section = 1; section <= middleSections; ++section)
	{
		const auto position = popup.section(section);
		const auto shadow = popup.shadow(section);
		ShadowVideoSurfaceRect(FRAME_BUFFER,
			shadow.x, shadow.y, shadow.right(), shadow.bottom());
		BltVideoObject(FRAME_BUFFER, hAlumniPopUpHandle, 1,
			position.x, position.y, VO_BLT_SRCTRANSPARENCY, NULL);
	}
	const auto bottom = popup.section(middleSections + 1);
	const auto bottomShadow = popup.shadow(middleSections + 1);
	const auto doneButton = popup.doneButton(middleSections);
	const auto doneHitbox = popup.doneHitbox(middleSections);
	ShadowVideoSurfaceRect(FRAME_BUFFER, bottomShadow.x, bottomShadow.y,
		bottomShadow.right(), bottomShadow.bottom());
	BltVideoObject(FRAME_BUFFER, hAlumniPopUpHandle, 2,
		bottom.x, bottom.y, VO_BLT_SRCTRANSPARENCY, NULL);
	BltVideoObject(FRAME_BUFFER, hDoneHandle, 0,
		doneButton.x, doneButton.y, VO_BLT_SRCTRANSPARENCY, NULL);
	DrawTextToScreen(AimAlumniText[AIM_ALUMNI_DONE],
		doneButton.x + 1, doneButton.y + 3, doneButton.width,
		AIM_ALUMNI_POPUP_NAME_FONT, AIM_ALUMNI_POPUP_NAME_COLOR,
		FONT_MCOLOR_BLACK, FALSE, CENTER_JUSTIFIED);
	CreateDestroyDoneMouseRegion(&doneHitbox);

	BltVideoObject(FRAME_BUFFER, hFacePaneHandle, 0,
		popup.facePanel.x, popup.facePanel.y, VO_BLT_SRCTRANSPARENCY, NULL);
	BltVideoObject(FRAME_BUFFER, hFaceHandle,
		gAimOldArchives[gubDrawOldMerc].FaceID,
		popup.facePanel.x + 1, popup.facePanel.y + 1,
		VO_BLT_SRCTRANSPARENCY, NULL);
	DrawTextToScreen(gAimOldArchives[gubDrawOldMerc].szName,
		popup.name.x, popup.name.y, 0, AIM_ALUMNI_POPUP_NAME_FONT,
		AIM_ALUMNI_POPUP_NAME_COLOR, FONT_MCOLOR_BLACK, FALSE,
		LEFT_JUSTIFIED);
	DisplayWrappedString(popup.description.origin.x,
		popup.description.origin.y, popup.description.width, 2,
		AIM_ALUMNI_POPUP_FONT, AIM_ALUMNI_POPUP_COLOR,
		gAimOldArchives[gubDrawOldMerc].szBio, FONT_MCOLOR_BLACK,
		FALSE, LEFT_JUSTIFIED);


	InvalidateRegion(LAPTOP_SCREEN_UL_X,LAPTOP_SCREEN_WEB_UL_Y,LAPTOP_SCREEN_LR_X,LAPTOP_SCREEN_WEB_LR_Y);
}


void DestroyPopUpBox()
{
	gfDestroyPopUpBox = FALSE;
	RenderAimArchives();
}



void InitAlumniFaceRegions()
{
	LaptopPageResourceOwner stagedResources;
	const auto layout = CurrentAimArchiveLayout();
	if(gfFaceMouseRegionsActive)
		return;

	for (std::size_t slot = 0; slot < layout.grid.capacity(); ++slot)
	{
		const std::size_t profile = AimWebsiteLayoutModel::ArchiveProfileIndex(
			gubPageNum, slot, NUM_PROFILES);
		if (profile == NUM_PROFILES || !vOldMerc[profile])
			continue;
		const auto bounds = layout.grid.hitbox(slot);
		MSYS_DefineRegion(&gMercAlumniFaceMouseRegions[slot],
			bounds.x, bounds.y, bounds.right(), bounds.bottom(),
			MSYS_PRIORITY_HIGH, CURSOR_WWW, MSYS_NO_CALLBACK,
			SelectAlumniFaceRegionCallBack);
		if (!stagedResources.addRegion(gMercAlumniFaceMouseRegions[slot]))
			return;
		MSYS_SetRegionUserData(&gMercAlumniFaceMouseRegions[slot], 0, slot);
	}

	gAimArchivesFaceResources = std::move(stagedResources);
	gfFaceMouseRegionsActive = TRUE;
}

void RemoveAimAlumniFaceRegion()
{
	if(!gfFaceMouseRegionsActive)
		return;

	gAimArchivesFaceResources.clear();
	gfFaceMouseRegionsActive = FALSE;
}




void CreateDestroyDoneMouseRegion(const LaptopLayoutModel::Rect* bounds)
{
	static BOOLEAN DoneRegionCreated=FALSE;

	if (!DoneRegionCreated && bounds)
	{
		MSYS_DefineRegion(&gDoneRegion,
			bounds->x, bounds->y, bounds->right(), bounds->bottom(),
			MSYS_PRIORITY_HIGH, CURSOR_WWW, MSYS_NO_CALLBACK,
			SelectAlumniDoneRegionCallBack);
		DoneRegionCreated =
			gAimArchivesDoneResources.addRegion(gDoneRegion);
	}

	if (DoneRegionCreated && !bounds)
	{
		gAimArchivesDoneResources.clear();
		DoneRegionCreated = FALSE;
	}
}


void SelectAlumniDoneRegionCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	if (!(iReason & MSYS_CALLBACK_REASON_LBUTTON_UP))
		return;
	gfDestroyPopUpBox = TRUE;
	gfDrawPopUpBox = FALSE;
	gfReDrawScreen = TRUE;
}

void ChangingAimArchiveSubPage( UINT8 ubSubPageNumber )
{
	if (!IsValidLaptopIndex(NUM_AIM_ARCHIVE_PAGES, ubSubPageNumber))
		return;

	fLoadPendingFlag = TRUE;

	if( AimArchivesSubPagesVisitedFlag[ ubSubPageNumber ] == FALSE )
	{
		fConnectingToSubPage = TRUE;
		fFastLoadFlag = FALSE;

		AimArchivesSubPagesVisitedFlag[ ubSubPageNumber ] = TRUE;
	}
	else
	{
		fConnectingToSubPage = TRUE;
		fFastLoadFlag = TRUE;
	}
}









