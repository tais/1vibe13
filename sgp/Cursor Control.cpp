#include "Cursor Control.h"

#include <array>
#include <limits>
#include <memory>

#include "video.h"


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Cursor Database
//
///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN gfCursorDatabaseInit = FALSE;

CursorFileData *gpCursorFileDatabase = nullptr;
CursorData		*gpCursorDatabase = nullptr;
INT16					gsGlobalCursorYOffset = 0;
INT16					gsCurMouseOffsetX = 0;
INT16 				gsCurMouseOffsetY = 0;
UINT16				gsCurMouseHeight = 0;
UINT16				gsCurMouseWidth = 0;
std::size_t			gusNumDataFiles = 0;
std::size_t			gusNumCursorData = 0;
UINT32				guiExternVo;
UINT16				gusExternVoSubIndex;
UINT32				guiExtern2Vo;
UINT16				gusExtern2VoSubIndex;
UINT32				guiOldSetCursor = VIDEO_NO_CURSOR;
UINT32				guiDelayTimer = 0;


MOUSEBLT_HOOK				gMouseBltOverride = NULL;

namespace
{
	struct ImageReleaser
	{
		void operator()(image_type* image) const noexcept
		{
			if (image != nullptr) DestroyImage(image);
		}
	};

	using ImageOwner = std::unique_ptr<image_type, ImageReleaser>;

	class CursorFileLoadTransaction
	{
	public:
		explicit CursorFileLoadTransaction(CursorFileData* files) noexcept
			: files_(files)
		{
		}

		~CursorFileLoadTransaction() noexcept
		{
			if (committed_ || files_ == nullptr) return;
			while (snapshotCount_ != 0)
			{
				const FileSnapshot& snapshot = snapshots_[--snapshotCount_];
				CursorFileData& current = files_[snapshot.index];
				if ((snapshot.value.ubFlags & USE_EXTERN_VO_CURSOR) == 0 &&
					current.uiIndex != 0 &&
					(current.fLoaded != snapshot.value.fLoaded ||
						current.uiIndex != snapshot.value.uiIndex ||
						current.hVObject != snapshot.value.hVObject))
				{
					DeleteVideoObjectFromIndex(current.uiIndex);
				}
				current = snapshot.value;
			}
		}

		CursorFileLoadTransaction(const CursorFileLoadTransaction&) = delete;
		CursorFileLoadTransaction& operator=(
			const CursorFileLoadTransaction&) = delete;

		bool track(std::size_t index) noexcept
		{
			for (std::size_t saved = 0; saved < snapshotCount_; ++saved)
				if (snapshots_[saved].index == index) return true;
			if (snapshotCount_ == snapshots_.size()) return false;
			snapshots_[snapshotCount_++] = {index, files_[index]};
			return true;
		}

		void commit() noexcept { committed_ = true; }

	private:
		struct FileSnapshot
		{
			std::size_t index = 0;
			CursorFileData value{};
		};

		CursorFileData* files_ = nullptr;
		std::array<FileSnapshot, MAX_COMPOSITES> snapshots_{};
		std::size_t snapshotCount_ = 0;
		bool committed_ = false;
	};

	bool IsCursorIndex(UINT32 cursor) noexcept
	{
		return gpCursorDatabase != nullptr && cursor < gusNumCursorData;
	}

	bool IsCursorFileIndex(UINT32 file) noexcept
	{
		return gpCursorFileDatabase != nullptr && file < gusNumDataFiles;
	}

	bool HasValidCompositeCount(const CursorData& cursor) noexcept
	{
		return cursor.usNumComposites != 0 &&
			cursor.usNumComposites <= MAX_COMPOSITES;
	}

	bool TryGetRegion(HVOBJECT object, UINT16 subIndex, ETRLEObject& region)
	{
		return GetVideoObjectETRLEProperties(object, &region, subIndex) != FALSE;
	}
}



BOOLEAN BltToMouseCursorFromVObject( HVOBJECT hVObject, UINT16 usVideoObjectSubIndex, INT16 usXPos, INT16 usYPos )
{
	ETRLEObject region{};
	if (!TryGetRegion(hVObject, usVideoObjectSubIndex, region)) return FALSE;
	return BltVideoObject(
		MOUSE_BUFFER,
		hVObject,
		usVideoObjectSubIndex,
		usXPos,
		usYPos,
		VO_BLT_SRCTRANSPARENCY,
		NULL);
}

BOOLEAN BltToMouseCursorFromVObjectWithOutline(
	HVOBJECT hVObject, UINT16 usVideoObjectSubIndex, INT16, INT16 )
{
	ETRLEObject region{};
	if (!TryGetRegion(hVObject, usVideoObjectSubIndex, region)) return FALSE;

	INT16 sXPos = 0;
	INT16 sYPos = 0;

	// Remove offsets...
	sXPos -= region.sOffsetX;
	sYPos -= region.sOffsetY;

	// Center!
	sXPos += ( ( gsCurMouseWidth - region.usWidth ) / 2 );
	sYPos += ( ( gsCurMouseHeight - region.usHeight ) / 2 );

	return BltVideoObjectOutline(
		MOUSE_BUFFER,
		hVObject,
		usVideoObjectSubIndex,
		sXPos,
		sYPos,
		Get16BPPColor(FROMRGB(0, 255, 0)),
		TRUE);
}


// THESE TWO PARAMETERS MUST POINT TO STATIC OR GLOBAL DATA, NOT AUTOMATIC VARIABLES
void InitCursorDatabase(
	CursorFileData *pCursorFileData,
	CursorData *pCursorData,
	std::size_t dataFileCount,
	std::size_t cursorCount )
{
	if (gfCursorDatabaseInit) CursorDatabaseClear();
	gpCursorFileDatabase = pCursorFileData;
	gpCursorDatabase = pCursorData;
	gusNumDataFiles = dataFileCount;
	gusNumCursorData = cursorCount;
	guiOldSetCursor = VIDEO_NO_CURSOR;
	guiDelayTimer = 0;
	gsCurMouseOffsetX = 0;
	gsCurMouseOffsetY = 0;
	gsCurMouseHeight = 0;
	gsCurMouseWidth = 0;
	gfCursorDatabaseInit =
		pCursorFileData != nullptr && pCursorData != nullptr &&
		dataFileCount != 0 && cursorCount != 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Cursor Handlers
//
///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN LoadCursorData(UINT32 uiCursorIndex)
{
	if (!gfCursorDatabaseInit || !IsCursorIndex(uiCursorIndex)) return FALSE;

	// Load cursor data will load all data required for the cursor specified by this index
	CursorData		*pCurData;
	CursorImage		*pCurImage;
	UINT32				cnt;
	UINT16				sMaxHeight = 0;
	UINT16				sMaxWidth = 0;
	ETRLEObject region{};

	pCurData = &( gpCursorDatabase[ uiCursorIndex ] );
	if (!HasValidCompositeCount(*pCurData)) return FALSE;
	CursorData stagedCursor = *pCurData;
	pCurData = &stagedCursor;
	CursorFileLoadTransaction fileLoads(gpCursorFileDatabase);

	for ( cnt = 0; cnt < pCurData->usNumComposites; cnt++ )
	{

		pCurImage = &( pCurData->Composites[ cnt ] );
		if (!IsCursorFileIndex(pCurImage->uiFileIndex)) return FALSE;
		CursorFileData& cursorFile =
			gpCursorFileDatabase[pCurImage->uiFileIndex];

		if ( cursorFile.fLoaded == FALSE )
		{
			if (!fileLoads.track(pCurImage->uiFileIndex)) return FALSE;
			//
			// The file containing the video object hasn't been loaded yet. Let's load it now
			//
			VOBJECT_DESC VideoObjectDescription{};
			// FIRST LOAD AS AN HIMAGE SO WE CAN GET AUX DATA!
			AuxObjectData *pAuxData;


			// ATE: First check if we are using an extern vo cursor...
			if ( cursorFile.ubFlags & USE_EXTERN_VO_CURSOR )
			{
				if ( cursorFile.hVObject == NULL ) return FALSE;

			}
			else
			{
				ImageOwner hImage(CreateImage(
					(CHAR8 *)cursorFile.ubFilename, IMAGE_ALLDATA));
				if (!hImage)
				{
					return( FALSE );
				}

				VideoObjectDescription.fCreateFlags = VOBJECT_CREATE_FROMHIMAGE;
				VideoObjectDescription.hImage = hImage.get();

				if ( !AddVideoObject(
						&VideoObjectDescription, &cursorFile.uiIndex) )
				{
					return( FALSE );
				}

				// Check for animated tile
				if (hImage->uiAppDataSize >= sizeof(AuxObjectData) &&
					hImage->pAppData != nullptr)
				{
					// Valid auxiliary data, so get # od frames from data
					pAuxData = ( AuxObjectData* ) hImage->pAppData;

					if ( pAuxData->fFlags & AUX_ANIMATED_TILE )
					{
						cursorFile.ubFlags |= ANIMATED_CURSOR;
						cursorFile.ubNumberOfFrames = pAuxData->ubNumberOfFrames;
					}
				}

				// Save hVObject....
				if (!GetVideoObject(&cursorFile.hVObject, cursorFile.uiIndex) ||
					cursorFile.hVObject == nullptr)
				{
					DeleteVideoObjectFromIndex(cursorFile.uiIndex);
					cursorFile.uiIndex = 0;
					cursorFile.hVObject = nullptr;
					return FALSE;
				}

			}

			cursorFile.fLoaded = TRUE;

		}

		// Get ETRLE Data for this video object
		if (!TryGetRegion(cursorFile.hVObject, pCurImage->uiSubIndex, region))
			return FALSE;
		if ((cursorFile.ubFlags & ANIMATED_CURSOR) != 0 &&
			(cursorFile.ubNumberOfFrames == 0 ||
				cursorFile.ubNumberOfFrames > cursorFile.hVObject->usNumberOfObjects))
			return FALSE;
		if ((cursorFile.ubFlags & ANIMATED_CURSOR) != 0 &&
			pCurImage->uiCurrentFrame >= cursorFile.ubNumberOfFrames)
		{
			pCurImage->uiCurrentFrame = 0;
		}

		if ( region.usHeight > sMaxHeight	)
		{
			sMaxHeight = region.usHeight;
		}

		if ( region.usWidth	> sMaxWidth )
		{
			sMaxWidth = region.usWidth;
		}

	}


	pCurData->usHeight = sMaxHeight;
	pCurData->usWidth = sMaxWidth;
	if (pCurData->usHeight == 0 || pCurData->usWidth == 0 ||
		pCurData->usHeight > MAX_CURSOR_HEIGHT ||
		pCurData->usWidth > MAX_CURSOR_WIDTH)
		return FALSE;


	if ( pCurData->sOffsetX == CENTER_CURSOR )
	{
		pCurData->sOffsetX = ( pCurData->usWidth / 2 );
	}
	if ( pCurData->sOffsetX == RIGHT_CURSOR )
	{
		pCurData->sOffsetX = static_cast<INT16>(pCurData->usWidth);
	}
	if ( pCurData->sOffsetX == LEFT_CURSOR )
	{
		pCurData->sOffsetX = 0;
	}

	if ( pCurData->sOffsetY == CENTER_CURSOR )
	{
		pCurData->sOffsetY = ( pCurData->usHeight / 2 );
	}
	if ( pCurData->sOffsetY == BOTTOM_CURSOR )
	{
		pCurData->sOffsetY = static_cast<INT16>(pCurData->usHeight);
	}
	if ( pCurData->sOffsetY == TOP_CURSOR )
	{
		pCurData->sOffsetY = 0;
	}


	// Adjust relative offsets
	for ( cnt = 0; cnt < pCurData->usNumComposites; cnt++ )
	{
		pCurImage = &( pCurData->Composites[ cnt ] );

		// Get ETRLE Data for this video object
		if (!IsCursorFileIndex(pCurImage->uiFileIndex) ||
			!TryGetRegion(
				gpCursorFileDatabase[pCurImage->uiFileIndex].hVObject,
				pCurImage->uiSubIndex,
				region))
			return FALSE;

		if ( pCurImage->usPosX == CENTER_SUBCURSOR )
		{
			pCurImage->usPosX = pCurData->sOffsetX - ( region.usWidth / 2 );
		}

		if ( pCurImage->usPosY == CENTER_SUBCURSOR )
		{
			pCurImage->usPosY = pCurData->sOffsetY - ( region.usHeight / 2 );
		}

	}

	gpCursorDatabase[uiCursorIndex] = stagedCursor;
	gsCurMouseOffsetX = stagedCursor.sOffsetX;
	gsCurMouseOffsetY = stagedCursor.sOffsetY;
	gsCurMouseHeight = stagedCursor.usHeight;
	gsCurMouseWidth = stagedCursor.usWidth;
	fileLoads.commit();

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void UnloadCursorData(UINT32 uiCursorIndex)
{
	if (!gfCursorDatabaseInit || !IsCursorIndex(uiCursorIndex)) return;

	// This function will unload add data used for this cursor
	//
	// Ok, first we make sure that the video object file is indeed loaded. Once this is verified, we will
	// move on to the deletion
	//
	CursorData		*pCurData;
	CursorImage		*pCurImage;
	UINT32				cnt;

	pCurData = &( gpCursorDatabase[ uiCursorIndex ] );
	if (!HasValidCompositeCount(*pCurData)) return;

	for ( cnt = 0; cnt < pCurData->usNumComposites; cnt++ )
	{

		pCurImage = &( pCurData->Composites[ cnt ] );
		if (!IsCursorFileIndex(pCurImage->uiFileIndex)) continue;

		if ( gpCursorFileDatabase[ pCurImage->uiFileIndex ].fLoaded )
		{
			if ( !( gpCursorFileDatabase[ pCurImage->uiFileIndex ].ubFlags & USE_EXTERN_VO_CURSOR ) )
			{
				DeleteVideoObjectFromIndex( gpCursorFileDatabase[ pCurImage->uiFileIndex ].uiIndex);
				gpCursorFileDatabase[ pCurImage->uiFileIndex ].uiIndex = 0;
				gpCursorFileDatabase[ pCurImage->uiFileIndex ].hVObject = nullptr;
			}
			gpCursorFileDatabase[ pCurImage->uiFileIndex ].fLoaded = FALSE;
		}

	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void	CursorDatabaseClear(void)
{
	if (!gfCursorDatabaseInit || gpCursorFileDatabase == nullptr) return;

	for (std::size_t uiIndex = 0; uiIndex < gusNumDataFiles; uiIndex++)
	{
		CursorFileData& file = gpCursorFileDatabase[uiIndex];
		if (file.fLoaded == TRUE &&
			(file.ubFlags & USE_EXTERN_VO_CURSOR) == 0)
		{
			DeleteVideoObjectFromIndex(file.uiIndex);
		}
		// Owned handles have just been destroyed; borrowed handles are simply
		// forgotten. Neither kind may survive a database lifetime boundary.
		file.uiIndex = 0;
		file.hVObject = nullptr;
		file.fLoaded = FALSE;
	}
	guiOldSetCursor = VIDEO_NO_CURSOR;
	guiDelayTimer = 0;
	gsCurMouseOffsetX = 0;
	gsCurMouseOffsetY = 0;
	gsCurMouseHeight = 0;
	gsCurMouseWidth = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN SetCurrentCursorFromDatabase( UINT32 uiCursorIndex	)
{

	BOOLEAN				ReturnValue = TRUE;
	UINT16				usSubIndex;
	CursorData		*pCurData;
	CursorImage		*pCurImage;
	UINT32				cnt;
	INT16					sCenterValX, sCenterValY;
	HVOBJECT			hVObject = nullptr;
	ETRLEObject region{};
	INT32					effectiveHeight = 0;
	INT32					effectiveWidth = 0;

	if ( gfCursorDatabaseInit )
	{
		if (uiCursorIndex != VIDEO_NO_CURSOR &&
			uiCursorIndex != EXTERN_CURSOR &&
			uiCursorIndex != EXTERN2_CURSOR &&
			!IsCursorIndex(uiCursorIndex))
			return FALSE;

		// If the current cursor is the first index, disable cursors

		if ( uiCursorIndex == VIDEO_NO_CURSOR )
		{
			EraseMouseCursor( );

			SetMouseCursorProperties( 0, 0, 5, 5 );
			DirtyCursor( );

			//EnableCursor( FALSE );

		}
		else
		{
			// CHECK FOR EXTERN CURSOR
			if ( uiCursorIndex == EXTERN_CURSOR || uiCursorIndex == EXTERN2_CURSOR	)
			{
				INT16 sSubX, sSubY;
				HVOBJECT			hVObjectTemp = nullptr;
				ETRLEObject secondaryRegion{};

				if ( uiCursorIndex == EXTERN2_CURSOR )
				{
					// Get ETRLE values
					if (!GetVideoObject(&hVObject, guiExtern2Vo) ||
						!TryGetRegion(hVObject, gusExtern2VoSubIndex, region))
						return FALSE;
					if (!GetVideoObject(&hVObjectTemp, guiExternVo) ||
						!TryGetRegion(
							hVObjectTemp, gusExternVoSubIndex, secondaryRegion))
						return FALSE;
				}
				else
				{
					// Get ETRLE values
					if (!GetVideoObject(&hVObject, guiExternVo) ||
						!TryGetRegion(hVObject, gusExternVoSubIndex, region))
						return FALSE;
				}

				// Determine center
				sCenterValX = 0;
				sCenterValY = 0;

				// Effective height
				effectiveHeight =
					static_cast<INT32>(region.usHeight) + region.sOffsetY;
				effectiveWidth =
					static_cast<INT32>(region.usWidth) + region.sOffsetX;
				if (effectiveHeight <= 0 || effectiveWidth <= 0 ||
					effectiveHeight > MAX_CURSOR_HEIGHT ||
					effectiveWidth > MAX_CURSOR_WIDTH)
					return FALSE;
				if (!EraseMouseCursor()) return FALSE;


				// ATE: Check for extern 2nd...
				if ( uiCursorIndex == EXTERN2_CURSOR )
				{
					if (!BltVideoObjectOutlineFromIndex(
							MOUSE_BUFFER, guiExtern2Vo, gusExtern2VoSubIndex,
							0, 0, 0, FALSE))
						return FALSE;

					sSubX = ( region.usWidth - secondaryRegion.usWidth -
						secondaryRegion.sOffsetX ) / 2;
					sSubY = ( region.usHeight - secondaryRegion.usHeight -
						secondaryRegion.sOffsetY ) / 2;

					if (!BltVideoObjectOutlineFromIndex(
							MOUSE_BUFFER, guiExternVo, gusExternVoSubIndex,
							sSubX, sSubY, 0, FALSE))
						return FALSE;

				}
				else
				{
					if (!BltVideoObjectOutlineFromIndex(
							MOUSE_BUFFER, guiExternVo, gusExternVoSubIndex,
							0, 0, 0, FALSE))
						return FALSE;
				}

				// Hook into hook function
				if ( gMouseBltOverride != NULL )
				{
					gMouseBltOverride( );
				}


				if (!SetMouseCursorProperties(
						static_cast<INT16>(effectiveWidth / 2),
						static_cast<INT16>(effectiveHeight / 2),
						static_cast<UINT16>(effectiveHeight),
						static_cast<UINT16>(effectiveWidth)))
					return FALSE;
				DirtyCursor( );

			}
			else
			{
				pCurData = &( gpCursorDatabase[ uiCursorIndex ] );

				// First check if we are a differnet curosr...
				if ( uiCursorIndex != guiOldSetCursor )
				{
					// OK, check if we are a delay cursor...
					if ( pCurData->bFlags & DELAY_START_CURSOR )
					{
						guiDelayTimer = GetTickCount( );
					}
				}

				guiOldSetCursor = uiCursorIndex;

				// Olny update if delay timer has elapsed...
				if ( pCurData->bFlags & DELAY_START_CURSOR )
				{
					if ( ( GetTickCount( ) - guiDelayTimer ) < 1000 )
					{
						EraseMouseCursor( );

						SetMouseCursorProperties( 0, 0, 5, 5 );
						DirtyCursor( );

						return( TRUE );
					}
				}


					//
					// Call LoadCursorData to make sure that the video object is loaded
					//
					if (!LoadCursorData(uiCursorIndex)) return FALSE;
					pCurData = &( gpCursorDatabase[ uiCursorIndex ] );

					if (!HasValidCompositeCount(*pCurData)) return FALSE;
					std::array<UINT16, MAX_COMPOSITES> resolvedSubIndices{};
					for (cnt = 0; cnt < pCurData->usNumComposites; ++cnt)
					{
						pCurImage = &( pCurData->Composites[cnt] );
						if (!IsCursorFileIndex(pCurImage->uiFileIndex)) return FALSE;
						const CursorFileData& file =
							gpCursorFileDatabase[pCurImage->uiFileIndex];
						if (!file.fLoaded || file.hVObject == nullptr) return FALSE;
						resolvedSubIndices[cnt] =
							(file.ubFlags & ANIMATED_CURSOR) != 0 ?
								static_cast<UINT16>(pCurImage->uiCurrentFrame) :
								pCurImage->uiSubIndex;
						if (!TryGetRegion(
								file.hVObject, resolvedSubIndices[cnt], region))
							return FALSE;
					}

					// Erase only after every composite and resolved frame validates.
					if (!EraseMouseCursor()) return FALSE;
					// NOW ACCOMODATE COMPOSITE CURSORS
					for ( cnt = 0; cnt < pCurData->usNumComposites; cnt++ )
				{

					// Check if we are a flashing cursor!
					if ( pCurData->bFlags & CURSOR_TO_FLASH )
					{
						if ( cnt <= 1 )
						{
							if ( pCurData->bFlashIndex != cnt )
							{
								continue;
							}
						}
					}
					// Check if we are a sub cursor!
					// IN this case, do all frames but
					// skip the 1st or second!

					if ( pCurData->bFlags & CURSOR_TO_SUB_CONDITIONALLY )
					{
						if ( pCurData->bFlags & CURSOR_TO_FLASH )
						{
							if ( cnt <= 1 )
							{
								if ( pCurData->bFlashIndex != cnt )
								{
									continue;
								}
							}
						}
						else if ( pCurData->bFlags & CURSOR_TO_FLASH2 )
						{
							if ( cnt <= 2 && cnt > 0 )
							{
								if ( pCurData->bFlashIndex != cnt )
								{
									continue;
								}
							}
						}
						else
						{
							if ( cnt <= 1 )
							{
								if ( pCurData->bFlashIndex != cnt )
								{
									continue;
								}
							}
						}
					}

					pCurImage = &( pCurData->Composites[ cnt ] );
					if (!IsCursorFileIndex(pCurImage->uiFileIndex)) return FALSE;

						// Use the prevalidated animated/static sub-index.
						usSubIndex = resolvedSubIndices[cnt];

					if ( pCurImage->usPosX != HIDE_SUBCURSOR && pCurImage->usPosY != HIDE_SUBCURSOR )
					{
						// Blit cursor at position in mouse buffer
						if ( gpCursorFileDatabase[ pCurImage->uiFileIndex].ubFlags & USE_OUTLINE_BLITTER )
						{
							ReturnValue = BltToMouseCursorFromVObjectWithOutline( gpCursorFileDatabase[ pCurImage->uiFileIndex ].hVObject , usSubIndex, pCurImage->usPosX, pCurImage->usPosY );
						}
						else
						{
							ReturnValue = BltToMouseCursorFromVObject( gpCursorFileDatabase[ pCurImage->uiFileIndex ].hVObject , usSubIndex, pCurImage->usPosX, pCurImage->usPosY );
						}
						if ( !ReturnValue )
						{
							return( FALSE );
						}

					}

					//if ( pCurData->bFlags & CURSOR_TO_FLASH )
					//{
					//	break;
					//}
				}

				// Hook into hook function
				if ( gMouseBltOverride != NULL )
				{
					gMouseBltOverride( );
				}


				sCenterValX = pCurData->sOffsetX;
				sCenterValY = pCurData->sOffsetY;

				if (!SetMouseCursorProperties(
						sCenterValX,
						(INT16)(sCenterValY + gsGlobalCursorYOffset),
						pCurData->usHeight,
						pCurData->usWidth))
					return FALSE;
				DirtyCursor( );
			}
		}
	}
	else
	{
		if ( uiCursorIndex == VIDEO_NO_CURSOR )
		{
			EraseMouseCursor( );

			SetMouseCursorProperties( 0, 0, 5, 5 );
			DirtyCursor( );

			//EnableCursor( FALSE );

		}
		else
		{

			if (uiCursorIndex > std::numeric_limits<UINT16>::max()) return FALSE;
			ReturnValue = SetCurrentCursor((UINT16)uiCursorIndex, 0, 0);
		}
	}

	return ( ReturnValue );

}

void SetMouseBltHook( MOUSEBLT_HOOK pMouseBltOverride )
{
	gMouseBltOverride = pMouseBltOverride;
}


// Sets an external video object as cursor file data....
BOOLEAN SetExternVOData( UINT32 uiCursorIndex, HVOBJECT hVObject, UINT16 usSubIndex )
{
	if (!gfCursorDatabaseInit || !IsCursorIndex(uiCursorIndex)) return FALSE;
	ETRLEObject region{};
	if (!TryGetRegion(hVObject, usSubIndex, region)) return FALSE;

	CursorData& cursor = gpCursorDatabase[uiCursorIndex];
	if (!HasValidCompositeCount(cursor)) return FALSE;
	struct ExternalFileSnapshot
	{
		UINT32 index = 0;
		CursorFileData value{};
	};
	std::array<ExternalFileSnapshot, MAX_COMPOSITES> fileSnapshots{};
	std::size_t snapshotCount = 0;
	for (UINT32 index = 0; index < cursor.usNumComposites; ++index)
	{
		const CursorImage& image = cursor.Composites[index];
		if (!IsCursorFileIndex(image.uiFileIndex)) return FALSE;
		const CursorFileData& file = gpCursorFileDatabase[image.uiFileIndex];
		if ((file.ubFlags & USE_EXTERN_VO_CURSOR) == 0) continue;

		bool alreadyTracked = false;
		for (std::size_t saved = 0; saved < snapshotCount; ++saved)
		{
			if (fileSnapshots[saved].index == image.uiFileIndex)
			{
				alreadyTracked = true;
				break;
			}
		}
		if (!alreadyTracked)
		{
			fileSnapshots[snapshotCount++] = {image.uiFileIndex, file};
		}
	}
	if (snapshotCount == 0) return FALSE;

	const CursorData cursorSnapshot = cursor;
	for (UINT32 index = 0; index < cursor.usNumComposites; ++index)
	{
		CursorImage& image = cursor.Composites[index];
		CursorFileData& file = gpCursorFileDatabase[image.uiFileIndex];

		if (file.ubFlags & USE_EXTERN_VO_CURSOR)
		{
			file.hVObject = hVObject;
			image.uiSubIndex = usSubIndex;
		}
	}
	if (LoadCursorData(uiCursorIndex)) return TRUE;

	// Replacing a borrowed video object must be transactional too. A regular
	// companion surface can still fail to load after the borrowed object has
	// validated, so restore the previous pointer, load state, and subindices.
	cursor = cursorSnapshot;
	while (snapshotCount != 0)
	{
		const ExternalFileSnapshot& snapshot = fileSnapshots[--snapshotCount];
		gpCursorFileDatabase[snapshot.index] = snapshot.value;
	}
	return FALSE;
}


void RemoveExternVOData( UINT32 uiCursorIndex )
{
	if (!gfCursorDatabaseInit || !IsCursorIndex(uiCursorIndex)) return;
	CursorData& cursor = gpCursorDatabase[uiCursorIndex];
	if (!HasValidCompositeCount(cursor)) return;
	for (UINT32 index = 0; index < cursor.usNumComposites; ++index)
		if (!IsCursorFileIndex(cursor.Composites[index].uiFileIndex)) return;

	for (UINT32 index = 0; index < cursor.usNumComposites; ++index)
	{
		CursorImage& image = cursor.Composites[index];
		CursorFileData& file = gpCursorFileDatabase[image.uiFileIndex];

		if (file.ubFlags & USE_EXTERN_VO_CURSOR)
		{
			// Borrowed objects are never destroyed here. Mark the cache entry
			// unavailable before dropping the pointer; regular companion assets
			// remain owned and reusable by the cursor database.
			file.fLoaded = FALSE;
			file.hVObject = nullptr;
		}

	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
