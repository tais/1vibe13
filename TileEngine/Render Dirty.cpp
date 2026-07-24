#include "builddefines.h"

	#include "worlddef.h"
	#include "Render Dirty.h"
	#include "sysutil.h"
	#include "vobject_blitters.h"

#include <cstddef>
#include <limits>

#ifdef JA2BETAVERSION
#include "message.h"
#endif

// Forward Declarations
extern INT16 gsVIEWPORT_START_X;
extern INT16 gsVIEWPORT_START_Y;
extern INT16 gsVIEWPORT_WINDOW_START_Y;
extern INT16 gsVIEWPORT_WINDOW_END_Y;
extern INT16 gsVIEWPORT_END_X;
extern INT16 gsVIEWPORT_END_Y;
extern UINT16* gpZBuffer;
extern BOOLEAN	gfScrollInertia;


#define		DIRTY_QUEUES			200
#define		BACKGROUND_BUFFERS		1500 //was 500
#define		VIDEO_OVERLAYS			100


BACKGROUND_SAVE	gBackSaves[BACKGROUND_BUFFERS];
UINT32 guiNumBackSaves=0;

VIDEO_OVERLAY	gVideoOverlays[ VIDEO_OVERLAYS ];
UINT32 guiNumVideoOverlays=0;


namespace
{
INT32 gRenderDirtyAllocationCountdown = -1;
void* gLastRenderDirtyAllocation = nullptr;
UINT16 gTestOverlayCharacterWidth = 0;
UINT16 gTestOverlayTextHeight = 0;

void* AllocateRenderDirtyBuffer(UINT32 size)
{
	if (gRenderDirtyAllocationCountdown == 0) return nullptr;
	if (gRenderDirtyAllocationCountdown > 0)
		--gRenderDirtyAllocationCountdown;
	gLastRenderDirtyAllocation = MemAlloc(size);
	return gLastRenderDirtyAllocation;
}

bool IsBackgroundIndex(INT32 index)
{
	return index >= 0 && index < BACKGROUND_BUFFERS;
}

bool IsOverlayIndex(INT32 index)
{
	return index >= 0 && index < VIDEO_OVERLAYS;
}

bool CheckedBufferSize(INT32 width, INT32 height, std::size_t bytesPerPixel,
	UINT32& bufferSize)
{
	bufferSize = 0;
	if (width <= 0 || height <= 0 || bytesPerPixel == 0) return false;
	const std::size_t w = static_cast<std::size_t>(width);
	const std::size_t h = static_cast<std::size_t>(height);
	if (w > std::numeric_limits<std::size_t>::max() / h) return false;
	const std::size_t pixels = w * h;
	if (pixels > std::numeric_limits<std::size_t>::max() / bytesPerPixel)
		return false;
	const std::size_t bytes = pixels * bytesPerPixel;
	if (bytes > std::numeric_limits<UINT32>::max()) return false;
	bufferSize = static_cast<UINT32>(bytes);
	return true;
}

bool CheckedCoordinateEnd(INT16 start, UINT16 extent, INT16& end)
{
	const INT32 candidate = static_cast<INT32>(start) + extent;
	if (candidate > std::numeric_limits<INT16>::max()) return false;
	end = static_cast<INT16>(candidate);
	return true;
}

void CopyOverlayText(VIDEO_OVERLAY& overlay, const CHAR16* text)
{
	if (!text) return;
	const std::size_t capacity = sizeof(overlay.zText) / sizeof(CHAR16);
	wcsncpy(overlay.zText, text, capacity - 1);
	overlay.zText[capacity - 1] = L'\0';
}

bool HasOverlayText(const CHAR16* text)
{
	return text != nullptr;
}

UINT16 MeasureOverlayTextWidth(const CHAR16* text, UINT32 font)
{
	if (gTestOverlayCharacterWidth != 0)
	{
		const std::size_t length = wcslen(text);
		const std::size_t width = length * gTestOverlayCharacterWidth;
		return static_cast<UINT16>(__min(width,
			static_cast<std::size_t>(std::numeric_limits<UINT16>::max())));
	}
	return static_cast<UINT16>(StringPixLength(text, font));
}

UINT16 MeasureOverlayTextHeight(UINT32 font)
{
	if (gTestOverlayTextHeight != 0) return gTestOverlayTextHeight;
	return GetFontHeight(font);
}

bool RegisterApplicationBuffer(INT16* buffer, INT32 width, INT32 height)
{
	if (!buffer || width <= 0 || height <= 0) return false;
	BYTE* const data = reinterpret_cast<BYTE*>(buffer);
	try
	{
		if (!SurfaceData::SetApplicationData(data)) return false;
		const SurfaceData::tID id = SurfaceData::GetSurfaceID(data);
		if (id != 0 && SetSurfaceClipRectangle(id,
			static_cast<unsigned int>(width),
			static_cast<unsigned int>(height)))
		{
			return true;
		}
	}
	catch (...)
	{
	}
	SurfaceData::ReleaseApplicationData(data);
	return false;
}

void ReleaseApplicationBuffer(INT16* buffer, bool freeMemory)
{
	if (!buffer) return;
	SurfaceData::ReleaseApplicationData(reinterpret_cast<BYTE*>(buffer));
	if (freeMemory) MemFree(buffer);
}

void ResetBackgroundSave(BACKGROUND_SAVE& save)
{
	INT16* const saveArea = save.pSaveArea;
	INT16* const zSaveArea = save.pZSaveArea;
	ReleaseApplicationBuffer(saveArea, save.fFreeMemory != FALSE);
	if (zSaveArea != saveArea)
	{
		// Z save areas are always allocated by RegisterBackgroundRect,
		// even when the caller supplies the pixel save area.
		ReleaseApplicationBuffer(zSaveArea, true);
	}
	memset(&save, 0, sizeof(save));
}

void ResetOverlaySaveArea(VIDEO_OVERLAY& overlay)
{
	ReleaseApplicationBuffer(overlay.pSaveArea, true);
	overlay.pSaveArea = nullptr;
	overlay.fActivelySaving = FALSE;
}
}

namespace RenderDirtyTestHooks
{
void FailAllocationAfter(INT32 successfulAllocations)
{
	gRenderDirtyAllocationCountdown = successfulAllocations;
	gLastRenderDirtyAllocation = nullptr;
}

void ResetAllocationFailure()
{
	gRenderDirtyAllocationCountdown = -1;
}

BYTE* LastAllocation()
{
	return reinterpret_cast<BYTE*>(gLastRenderDirtyAllocation);
}

void UseFixedTextMetrics(UINT16 characterWidth, UINT16 textHeight)
{
	gTestOverlayCharacterWidth = characterWidth;
	gTestOverlayTextHeight = textHeight;
}

void ResetTextMetrics()
{
	gTestOverlayCharacterWidth = 0;
	gTestOverlayTextHeight = 0;
}

bool NullOverlayTextIsNoOp()
{
	VIDEO_OVERLAY overlay{};
	overlay.zText[0] = L'X';
	CopyOverlayText(overlay, nullptr);
	return overlay.zText[0] == L'X';
}
}


void AllocateVideoOverlayArea( UINT32 uiCount );
void SaveVideoOverlayArea( UINT32 uiSrcBuffer, UINT32 uiCount );

// Copies a rectangle of 16-bit Z-buffer values.  The Z-buffer stays 16bpp
// regardless of the screen pixel depth, so it can't use Blt16BPPTo16BPP
// (which moves PIXEL-sized elements once SGP_PIXEL_DEPTH==32).
static void BltZRectCopy(UINT16 *pDest, UINT32 uiDestPitchBYTES, UINT16 *pSrc, UINT32 uiSrcPitchBYTES,
                         INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos,
                         UINT32 uiWidth, UINT32 uiHeight)
{
	UINT8 *pD = (UINT8 *)pDest + iDestYPos * uiDestPitchBYTES + iDestXPos * sizeof(UINT16);
	UINT8 *pS = (UINT8 *)pSrc  + iSrcYPos  * uiSrcPitchBYTES  + iSrcXPos  * sizeof(UINT16);
	for (UINT32 y = 0; y < uiHeight; ++y)
	{
		memcpy(pD, pS, uiWidth * sizeof(UINT16));
		pD += uiDestPitchBYTES;
		pS += uiSrcPitchBYTES;
	}
}

//BACKGROUND_SAVE	gTopmostSaves[BACKGROUND_BUFFERS];
//UINT32 guiNumTopmostSaves=0;

// do zmiany
//SGPRect		gDirtyClipRect = { 0, 0, 2560, 1600 };
SGPRect		gDirtyClipRect = { 0, 0, 0, 0 }; 

BOOLEAN		gfViewportDirty=FALSE;


BOOLEAN InitializeBaseDirtyRectQueue( )
{
	gDirtyClipRect.iLeft = 0;
	gDirtyClipRect.iTop = 0;
	gDirtyClipRect.iRight = SCREEN_WIDTH;
	gDirtyClipRect.iBottom = SCREEN_HEIGHT;

	return( TRUE );
}

void ShutdownBaseDirtyRectQueue( )
{

}

void AddBaseDirtyRect( INT32 iLeft, INT32 iTop, INT32 iRight, INT32 iBottom )
{
	SGPRect aRect;

	if ( iLeft < 0 )
	{
		iLeft = 0;
	}
	if ( iLeft > SCREEN_WIDTH )
	{
		iLeft = SCREEN_WIDTH;
	}

	if ( iTop < 0 )
	{
		iTop = 0;
	}
	if ( iTop > SCREEN_HEIGHT )
	{
		iTop = SCREEN_HEIGHT;
	}

	if ( iRight < 0 )
	{
		iRight = 0;
	}
	if ( iRight > SCREEN_WIDTH )
	{
		iRight = SCREEN_WIDTH;
	}

	if ( iBottom < 0 )
	{
		iBottom = 0;
	}
	if ( iBottom > SCREEN_HEIGHT )
	{
		iBottom = SCREEN_HEIGHT;
	}

	if ( ( iRight - iLeft ) == 0 || ( iBottom - iTop ) == 0 )
	{
		return;
	}


	if( (iLeft==gsVIEWPORT_START_X) &&
		(iRight==gsVIEWPORT_END_X) &&
		(iTop==gsVIEWPORT_WINDOW_START_Y) &&
		(iBottom==gsVIEWPORT_WINDOW_END_Y))
	{
		gfViewportDirty=TRUE;
		return;
	}

	// Add to list
	aRect.iLeft		= iLeft;
	aRect.iTop		= iTop;
	aRect.iRight	= iRight;
	aRect.iBottom	= iBottom;

	InvalidateRegionEx( aRect.iLeft, aRect.iTop, aRect.iRight, aRect.iBottom, 0 );
}

BOOLEAN ExecuteBaseDirtyRectQueue( )
{
	if(gfViewportDirty)
	{
		//InvalidateRegion(gsVIEWPORT_START_X, gsVIEWPORT_START_Y, gsVIEWPORT_END_X, gsVIEWPORT_END_Y);
		InvalidateScreen( );
		EmptyDirtyRectQueue();
		gfViewportDirty=FALSE;
		return(TRUE);
	}

	return( TRUE );
}

BOOLEAN EmptyDirtyRectQueue( )
{
	return( TRUE );
}


INT32 GetFreeBackgroundBuffer(void)
{
	UINT32 uiCount;

	for(uiCount=0; uiCount < guiNumBackSaves; uiCount++)
	{
		if((gBackSaves[uiCount].fAllocated==FALSE) && (gBackSaves[uiCount].fFilled==FALSE))
			return((INT32)uiCount);
	}

	if(guiNumBackSaves < BACKGROUND_BUFFERS)
		return((INT32)guiNumBackSaves++);
#ifdef JA2BETAVERSION
	else
	{
		//else display an error message
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("ERROR! GetFreeBackgroundBuffer(): Trying to allocate more saves then there is room:	GetCurrentScreen() = %d", GetCurrentScreen() ) );
	}
#endif
	return(-1);
}

void RecountBackgrounds(void)
{
	guiNumBackSaves = 0;
	for(INT32 uiCount = BACKGROUND_BUFFERS - 1; uiCount >= 0; uiCount--)
	{
		if(gBackSaves[uiCount].fAllocated || gBackSaves[uiCount].fFilled ||
			gBackSaves[uiCount].pSaveArea || gBackSaves[uiCount].pZSaveArea)
		{
			guiNumBackSaves=(UINT32)(uiCount+1);
			break;
		}
	}
}

INT32 RegisterBackgroundRect(UINT32 uiFlags, INT16 *pSaveArea, INT16 sLeft, INT16 sTop, INT16 sRight, INT16 sBottom)
{
	// Don't register if we are rendering and we are below the viewport
	//if ( sTop >= gsVIEWPORT_WINDOW_END_Y )
	//{
	//	return(-1 );
	//}
	//only time this shoudl be true is tactical..fix for games saved in broken state
	extern UINT32 guiTacticalInterfaceFlags;
	if (guiTacticalInterfaceFlags & 0x00000001)
	{
		if(gDirtyClipRect.iBottom < SCREEN_HEIGHT)
		{
			gDirtyClipRect.iBottom = SCREEN_HEIGHT;
		}
	}
	if (sRight <= sLeft || sBottom <= sTop ||
		gDirtyClipRect.iRight <= gDirtyClipRect.iLeft ||
		gDirtyClipRect.iBottom <= gDirtyClipRect.iTop)
	{
		return -1;
	}

	const INT32 clippedLeft = __max(static_cast<INT32>(sLeft), gDirtyClipRect.iLeft);
	const INT32 clippedTop = __max(static_cast<INT32>(sTop), gDirtyClipRect.iTop);
	const INT32 clippedRight = __min(static_cast<INT32>(sRight), gDirtyClipRect.iRight);
	const INT32 clippedBottom = __min(static_cast<INT32>(sBottom), gDirtyClipRect.iBottom);
	if (clippedRight <= clippedLeft || clippedBottom <= clippedTop) return -1;

	const INT32 width = clippedRight - clippedLeft;
	const INT32 height = clippedBottom - clippedTop;
	if (width > std::numeric_limits<INT16>::max() ||
		height > std::numeric_limits<INT16>::max()) return -1;
	UINT32 pixelBufferSize = 0;
	UINT32 zBufferSize = 0;
	if ((uiFlags & BGND_FLAG_SAVERECT) &&
		!CheckedBufferSize(width, height, sizeof(PIXEL), pixelBufferSize))
	{
		return -1;
	}
	if ((uiFlags & BGND_FLAG_SAVE_Z) &&
		!CheckedBufferSize(width, height, sizeof(UINT16), zBufferSize))
	{
		return -1;
	}

	BACKGROUND_SAVE staged{};
	if (uiFlags & BGND_FLAG_SAVERECT)
	{
		staged.pSaveArea = pSaveArea;
		if (!staged.pSaveArea)
		{
			staged.pSaveArea =
				static_cast<INT16*>(AllocateRenderDirtyBuffer(pixelBufferSize));
			if (!staged.pSaveArea) return -1;
			staged.fFreeMemory = TRUE;
		}
		if (!RegisterApplicationBuffer(staged.pSaveArea, width, height))
		{
			ResetBackgroundSave(staged);
			return -1;
		}
	}
	if (uiFlags & BGND_FLAG_SAVE_Z)
	{
		staged.pZSaveArea =
			static_cast<INT16*>(AllocateRenderDirtyBuffer(zBufferSize));
		if (!staged.pZSaveArea ||
			!RegisterApplicationBuffer(staged.pZSaveArea, width, height))
		{
			ResetBackgroundSave(staged);
			return -1;
		}
		staged.fZBuffer = TRUE;
	}

	const INT32 iBackIndex = GetFreeBackgroundBuffer();
	if (iBackIndex == -1)
	{
		ResetBackgroundSave(staged);
		return -1;
	}

	ResetBackgroundSave(gBackSaves[iBackIndex]);
	staged.fAllocated = TRUE;
	staged.uiFlags = uiFlags;
	staged.sLeft = static_cast<INT16>(clippedLeft);
	staged.sTop = static_cast<INT16>(clippedTop);
	staged.sRight = static_cast<INT16>(clippedRight);
	staged.sBottom = static_cast<INT16>(clippedBottom);
	staged.sWidth = static_cast<INT16>(width);
	staged.sHeight = static_cast<INT16>(height);
	gBackSaves[iBackIndex] = staged;
	return iBackIndex;
}

void SetBackgroundRectFilled( UINT32 uiBackgroundID )
{
	if (uiBackgroundID >= BACKGROUND_BUFFERS ||
		!gBackSaves[uiBackgroundID].fAllocated) return;
	gBackSaves[uiBackgroundID].fFilled=TRUE;

	AddBaseDirtyRect(gBackSaves[uiBackgroundID].sLeft, gBackSaves[uiBackgroundID].sTop,
						gBackSaves[uiBackgroundID].sRight, gBackSaves[uiBackgroundID].sBottom);

}

BOOLEAN RestoreBackgroundRects(void)
{
	UINT32 uiCount, uiDestPitchBYTES, uiSrcPitchBYTES;
	UINT8	*pDestBuf, *pSrcBuf;

	pDestBuf = LockVideoSurface(guiRENDERBUFFER, &uiDestPitchBYTES);
	pSrcBuf = LockVideoSurface(guiSAVEBUFFER, &uiSrcPitchBYTES);

	for(uiCount=0; uiCount < guiNumBackSaves; uiCount++)
	{
		if(gBackSaves[uiCount].fFilled && ( !gBackSaves[uiCount].fDisabled) )
		{

			if ( gBackSaves[uiCount].uiFlags & BGND_FLAG_SAVERECT )
			{
				if ( gBackSaves[uiCount].pSaveArea != NULL )
				{
					Blt16BPPTo16BPP( (PIXEL *)pDestBuf, uiDestPitchBYTES, (PIXEL *)gBackSaves[uiCount].pSaveArea, gBackSaves[uiCount].sWidth*sizeof(PIXEL),
								gBackSaves[uiCount].sLeft , gBackSaves[uiCount].sTop,
								0, 0,
								gBackSaves[uiCount].sWidth, gBackSaves[uiCount].sHeight);

					AddBaseDirtyRect(gBackSaves[uiCount].sLeft, gBackSaves[uiCount].sTop,
													gBackSaves[uiCount].sRight, gBackSaves[uiCount].sBottom);
				}
			}
			else if ( gBackSaves[uiCount].uiFlags & BGND_FLAG_SAVE_Z )
			{
				if ( gBackSaves[uiCount].fZBuffer )
				{
					BltZRectCopy( (UINT16*)gpZBuffer, uiDestPitchBYTES, (UINT16 *)gBackSaves[uiCount].pZSaveArea, gBackSaves[uiCount].sWidth*sizeof(UINT16),
								gBackSaves[uiCount].sLeft , gBackSaves[uiCount].sTop,
								0, 0,
								gBackSaves[uiCount].sWidth, gBackSaves[uiCount].sHeight);
				}
			}
			else
			{
				Blt16BPPTo16BPP((PIXEL *)pDestBuf, uiDestPitchBYTES,
							(PIXEL *)pSrcBuf, uiSrcPitchBYTES,
							gBackSaves[uiCount].sLeft , gBackSaves[uiCount].sTop,
							gBackSaves[uiCount].sLeft , gBackSaves[uiCount].sTop,
							gBackSaves[uiCount].sWidth, gBackSaves[uiCount].sHeight);

				AddBaseDirtyRect(gBackSaves[uiCount].sLeft, gBackSaves[uiCount].sTop,
									gBackSaves[uiCount].sRight, gBackSaves[uiCount].sBottom);
			}
		}
	}

	UnLockVideoSurface(guiRENDERBUFFER);
	UnLockVideoSurface(guiSAVEBUFFER);

	EmptyBackgroundRects( );

	return(TRUE);
}


BOOLEAN EmptyBackgroundRects(void)
{
	for(UINT32 uiCount=0; uiCount < guiNumBackSaves; uiCount++)
	{
		if(gBackSaves[uiCount].fFilled)
		{
			gBackSaves[uiCount].fFilled=FALSE;
		}

		if (!gBackSaves[uiCount].fAllocated ||
			(gBackSaves[uiCount].uiFlags & BGND_FLAG_SINGLE) ||
			gBackSaves[uiCount].fPendingDelete)
		{
			ResetBackgroundSave(gBackSaves[uiCount]);
		}
	}

	RecountBackgrounds();

	return(TRUE);
}


BOOLEAN SaveBackgroundRects(void)
{
	UINT32 uiCount, uiDestPitchBYTES, uiSrcPitchBYTES;
	UINT8	*pDestBuf, *pSrcBuf;

	pSrcBuf = LockVideoSurface(guiRENDERBUFFER, &uiDestPitchBYTES );
	pDestBuf = LockVideoSurface(guiSAVEBUFFER, &uiSrcPitchBYTES);

	for(uiCount=0; uiCount < guiNumBackSaves; uiCount++)
	{
		if(gBackSaves[uiCount].fAllocated && ( !gBackSaves[uiCount].fDisabled) )
		{
			if ( gBackSaves[uiCount].uiFlags & BGND_FLAG_SAVERECT )
			{
				if ( gBackSaves[uiCount].pSaveArea != NULL )
				{
					Blt16BPPTo16BPP((PIXEL *)gBackSaves[uiCount].pSaveArea, gBackSaves[uiCount].sWidth*sizeof(PIXEL),
							(PIXEL *)pSrcBuf, uiDestPitchBYTES,
							0, 0,
							gBackSaves[uiCount].sLeft , gBackSaves[uiCount].sTop,
							gBackSaves[uiCount].sWidth, gBackSaves[uiCount].sHeight);

				}
			}
			else if(gBackSaves[uiCount].fZBuffer)
			{
				BltZRectCopy((UINT16 *)gBackSaves[uiCount].pZSaveArea, gBackSaves[uiCount].sWidth*sizeof(UINT16),
							(UINT16 *)gpZBuffer, uiDestPitchBYTES,
							0, 0,
							gBackSaves[uiCount].sLeft , gBackSaves[uiCount].sTop,
							gBackSaves[uiCount].sWidth, gBackSaves[uiCount].sHeight);
			}
			else
			{
				AddBaseDirtyRect(gBackSaves[uiCount].sLeft, gBackSaves[uiCount].sTop,
									gBackSaves[uiCount].sRight, gBackSaves[uiCount].sBottom);
			}

			gBackSaves[uiCount].fFilled=TRUE;
		}
	}

	UnLockVideoSurface(guiRENDERBUFFER);
	UnLockVideoSurface(guiSAVEBUFFER);

	return(TRUE);
}


BOOLEAN FreeBackgroundRect(INT32 iIndex)
{
	if (!IsBackgroundIndex(iIndex)) return FALSE;
	BACKGROUND_SAVE& save = gBackSaves[iIndex];
	if (!save.fAllocated && !save.fFilled && !save.pSaveArea && !save.pZSaveArea)
		return FALSE;
	save.fAllocated=FALSE;
	if (!save.fFilled) ResetBackgroundSave(save);
	RecountBackgrounds();
	return TRUE;
}

BOOLEAN FreeBackgroundRectPending(INT32 iIndex)
{
	if (!IsBackgroundIndex(iIndex) || !gBackSaves[iIndex].fAllocated)
		return FALSE;
	gBackSaves[iIndex].fPendingDelete = TRUE;
	return TRUE;
}


BOOLEAN FreeBackgroundRectNow(INT32 uiCount)
{
	if (!IsBackgroundIndex(uiCount)) return FALSE;
	ResetBackgroundSave(gBackSaves[uiCount]);
	RecountBackgrounds();
	return TRUE;
}

BOOLEAN FreeBackgroundRectType(UINT32 uiFlags)
{
UINT32 uiCount;

	for(uiCount=0; uiCount < guiNumBackSaves; uiCount++)
	{
		if(gBackSaves[uiCount].uiFlags&uiFlags)
		{
			ResetBackgroundSave(gBackSaves[uiCount]);
		}
	}

	RecountBackgrounds();

	return(TRUE);
}


BOOLEAN InitializeBackgroundRects(void)
{
	for (BACKGROUND_SAVE& save : gBackSaves) ResetBackgroundSave(save);
	guiNumBackSaves = 0;
	return( TRUE );
}

BOOLEAN InvalidateBackgroundRects(void)
{
	UINT32 uiCount;

	for(uiCount=0; uiCount < guiNumBackSaves; uiCount++)
		gBackSaves[uiCount].fFilled=FALSE;

	return(TRUE);
}


BOOLEAN ShutdownBackgroundRects(void)
{
	for (BACKGROUND_SAVE& save : gBackSaves) ResetBackgroundSave(save);
	guiNumBackSaves = 0;
	return(TRUE);
}

void DisableBackgroundRect( INT32 iIndex, BOOLEAN fDisabled )
{
	if (!IsBackgroundIndex(iIndex) || !gBackSaves[iIndex].fAllocated) return;
	gBackSaves[iIndex].fDisabled = fDisabled;
}

BOOLEAN UpdateSaveBuffer(void)
{
	UINT32 uiDestPitchBYTES, uiSrcPitchBYTES;
	UINT8	*pDestBuf, *pSrcBuf;
	UINT16 usWidth, usHeight;
	UINT8	ubBitDepth;

	// Update saved buffer - do for the viewport size ony!
	GetCurrentVideoSettings( &usWidth, &usHeight, &ubBitDepth );

	pSrcBuf = LockVideoSurface(guiRENDERBUFFER, &uiSrcPitchBYTES);
	pDestBuf = LockVideoSurface(guiSAVEBUFFER, &uiDestPitchBYTES);

	Blt16BPPTo16BPP((PIXEL *)pDestBuf, uiDestPitchBYTES,
				(PIXEL *)pSrcBuf, uiSrcPitchBYTES,
				0, gsVIEWPORT_WINDOW_START_Y, 0, gsVIEWPORT_WINDOW_START_Y, usWidth, ( gsVIEWPORT_WINDOW_END_Y - gsVIEWPORT_WINDOW_START_Y )	);

	UnLockVideoSurface(guiRENDERBUFFER);
	UnLockVideoSurface(guiSAVEBUFFER);

	return(TRUE);
}


BOOLEAN RestoreExternBackgroundRect( INT16 sLeft, INT16 sTop, INT16 sWidth, INT16 sHeight )
{
	UINT32 uiDestPitchBYTES, uiSrcPitchBYTES;
	UINT8	*pDestBuf, *pSrcBuf;

//Heinz (18.01.2009): fixed RUNTIME ERROR when user use screen resolution 640x480
	//Assert( ( sLeft >= 0 ) && ( sTop >= 0 ) && ( sLeft + sWidth <= SCREEN_WIDTH ) && ( sTop + sHeight <= SCREEN_HEIGHT ) );
	if(sLeft < 0) sLeft = 0;
	if(sTop <0 ) sTop = 0;
	if(sLeft > SCREEN_WIDTH) sLeft = SCREEN_WIDTH;
	if(sTop > SCREEN_HEIGHT) sTop = SCREEN_HEIGHT;
	if(sLeft + sWidth > SCREEN_WIDTH) sWidth = SCREEN_WIDTH - sLeft;
	if(sTop + sHeight > SCREEN_HEIGHT) sHeight = SCREEN_HEIGHT - sTop;

	pDestBuf = LockVideoSurface(guiRENDERBUFFER, &uiDestPitchBYTES);
	pSrcBuf = LockVideoSurface(guiSAVEBUFFER, &uiSrcPitchBYTES);

	Blt16BPPTo16BPP((PIXEL *)pDestBuf, uiDestPitchBYTES,
				(PIXEL *)pSrcBuf, uiSrcPitchBYTES,
				sLeft , sTop,
				sLeft , sTop,
				sWidth, sHeight);
	UnLockVideoSurface(guiRENDERBUFFER);
	UnLockVideoSurface(guiSAVEBUFFER);

	// Add rect to frame buffer queue
	InvalidateRegionEx( sLeft, sTop, (sLeft + sWidth), ( sTop + sHeight ), 0 );

	return(TRUE);
}

BOOLEAN RestoreExternBackgroundRect(SGPRectangle rect)
{
	return RestoreExternBackgroundRect(rect.x, rect.y, rect.width, rect.height);
}


BOOLEAN RestoreExternBackgroundRectGivenID( INT32 iBack )
{
	UINT32 uiDestPitchBYTES, uiSrcPitchBYTES;
	INT16 sLeft, sTop, sWidth, sHeight;
	UINT8	*pDestBuf, *pSrcBuf;

	if (!IsBackgroundIndex(iBack) || !gBackSaves[iBack].fAllocated)
	{
		return( FALSE );
	}

	sLeft	= gBackSaves[iBack].sLeft;
	sTop	= gBackSaves[iBack].sTop;
	sWidth	= gBackSaves[iBack].sWidth;
	sHeight	= gBackSaves[iBack].sHeight;

	Assert( ( sLeft >= 0 ) && ( sTop >= 0 ) && ( sLeft + sWidth <= SCREEN_WIDTH ) && ( sTop + sHeight <= SCREEN_HEIGHT ) );

	pDestBuf = LockVideoSurface(guiRENDERBUFFER, &uiDestPitchBYTES);
	pSrcBuf = LockVideoSurface(guiSAVEBUFFER, &uiSrcPitchBYTES);

	Blt16BPPTo16BPP((PIXEL *)pDestBuf, uiDestPitchBYTES,
				(PIXEL *)pSrcBuf, uiSrcPitchBYTES,
				sLeft , sTop,
				sLeft , sTop,
				sWidth, sHeight);
	UnLockVideoSurface(guiRENDERBUFFER);
	UnLockVideoSurface(guiSAVEBUFFER);

	// Add rect to frame buffer queue
	InvalidateRegionEx( sLeft, sTop, (sLeft + sWidth), ( sTop + sHeight ), 0 );

	return(TRUE);
}

BOOLEAN CopyExternBackgroundRect( INT16 sLeft, INT16 sTop, INT16 sWidth, INT16 sHeight )
{
	UINT32 uiDestPitchBYTES, uiSrcPitchBYTES;
	UINT8	*pDestBuf, *pSrcBuf;

	Assert( ( sLeft >= 0 ) && ( sTop >= 0 ) && ( sLeft + sWidth <= SCREEN_WIDTH ) && ( sTop + sHeight <= SCREEN_HEIGHT ) );

	pDestBuf = LockVideoSurface(guiSAVEBUFFER, &uiDestPitchBYTES);
	pSrcBuf = LockVideoSurface(guiRENDERBUFFER, &uiSrcPitchBYTES);

	Blt16BPPTo16BPP((PIXEL *)pDestBuf, uiDestPitchBYTES,
				(PIXEL *)pSrcBuf, uiSrcPitchBYTES,
				sLeft , sTop,
				sLeft , sTop,
				sWidth, sHeight);
	UnLockVideoSurface(guiSAVEBUFFER);
	UnLockVideoSurface(guiRENDERBUFFER);

	return(TRUE);
}

//*****************************************************************************
// gprintfdirty
//
//		Dirties a single-frame rect exactly the size needed to save the
// background for a given call to gprintf. Note that this must be called before
// the backgrounds are saved, and before the actual call to gprintf that writes
// to the video buffer.
//
//*****************************************************************************
UINT16 gprintfdirty(INT16 x, INT16 y, STR16 pFontString, ...)
{
	va_list argptr;
	CHAR16	string[512];
	UINT16 uiStringLength, uiStringHeight;
	INT32 iBack;

	Assert(pFontString!=NULL);

	va_start(argptr, pFontString);			// Set up variable argument pointer
	vswprintf(string, pFontString, argptr);	// process gprintf string (get output str)
	va_end(argptr);

	uiStringLength=StringPixLength(string, FontDefault);
	uiStringHeight=GetFontHeight(FontDefault);

	if ( uiStringLength > 0 )
	{
		iBack = RegisterBackgroundRect(BGND_FLAG_SINGLE, NULL, x, y, (INT16)(x + uiStringLength), (INT16)(y + uiStringHeight));

		if ( iBack != -1 )
		{
			SetBackgroundRectFilled( iBack );
		}
	}

	return(uiStringLength);
}

UINT16 gprintfinvalidate(INT16 x, INT16 y, STR16 pFontString, ...)
{
	va_list argptr;
	CHAR16	string[512];
	UINT16 uiStringLength, uiStringHeight;

	Assert(pFontString!=NULL);

	va_start(argptr, pFontString);			// Set up variable argument pointer
	vswprintf(string, pFontString, argptr);	// process gprintf string (get output str)
	va_end(argptr);

	uiStringLength=StringPixLength(string, FontDefault);
	uiStringHeight=GetFontHeight(FontDefault);

	if ( uiStringLength > 0 )
	{
		InvalidateRegionEx( x, y, (INT16)(x + uiStringLength), (INT16)(y + uiStringHeight), 0 );
	}
	return(uiStringLength);
}

UINT16 gprintfRestore(INT16 x, INT16 y, STR16 pFontString, ...)
{
	va_list argptr;
	CHAR16	string[512];
	UINT16 uiStringLength, uiStringHeight;

	Assert(pFontString!=NULL);

	va_start(argptr, pFontString);			// Set up variable argument pointer
	vswprintf(string, pFontString, argptr);	// process gprintf string (get output str)
	va_end(argptr);

	uiStringLength=StringPixLength(string, FontDefault);
	uiStringHeight=GetFontHeight(FontDefault);

	if ( uiStringLength > 0 )
	{
		RestoreExternBackgroundRect( x, y, uiStringLength, uiStringHeight );
	}

	return(uiStringLength);
}

// OVERLAY STUFF
INT32 GetFreeVideoOverlay(void)
{
	UINT32 uiCount;

	for(uiCount=0; uiCount < guiNumVideoOverlays; uiCount++)
	{
		if (!gVideoOverlays[uiCount].fAllocated)
			return((INT32)uiCount);
	}

	if( guiNumVideoOverlays < VIDEO_OVERLAYS )
		return((INT32)guiNumVideoOverlays);

	return(-1);
}

void RecountVideoOverlays(void)
{
	guiNumVideoOverlays = 0;
	for(INT32 uiCount = VIDEO_OVERLAYS - 1; uiCount >= 0; uiCount--)
	{
		if((gVideoOverlays[uiCount].fAllocated) )
		{
			guiNumVideoOverlays=(UINT32)(uiCount+1);
			break;
		}
	}
}

INT32 RegisterVideoOverlay( UINT32 uiFlags, VIDEO_OVERLAY_DESC *pTopmostDesc )
{
	if (!pTopmostDesc || !pTopmostDesc->BltCallback) return -1;
	INT32 iBackIndex;
	UINT16 uiStringLength, uiStringHeight;

	if ( uiFlags & VOVERLAY_DIRTYBYTEXT )
	{
		// Get dims by supplied text
		if (!HasOverlayText(pTopmostDesc->pzText)) return -1;
		uiStringLength = MeasureOverlayTextWidth(
			pTopmostDesc->pzText, pTopmostDesc->uiFontID);
		uiStringHeight = MeasureOverlayTextHeight(pTopmostDesc->uiFontID);
		INT16 textRight = 0;
		INT16 textBottom = 0;
		if (!CheckedCoordinateEnd(pTopmostDesc->sLeft, uiStringLength,
				textRight) ||
			!CheckedCoordinateEnd(pTopmostDesc->sTop, uiStringHeight,
				textBottom)) return -1;
		iBackIndex = RegisterBackgroundRect(BGND_FLAG_PERMANENT, NULL,
			pTopmostDesc->sLeft, pTopmostDesc->sTop, textRight, textBottom);
	}
	else
	{
		// Register background
		iBackIndex = RegisterBackgroundRect( BGND_FLAG_PERMANENT, NULL, pTopmostDesc->sLeft, pTopmostDesc->sTop, pTopmostDesc->sRight, pTopmostDesc->sBottom );
	}

	if ( iBackIndex == -1 )
	{
		return( -1 );
	}

	// Get next free topmost blitter index
	const INT32 iBlitterIndex = GetFreeVideoOverlay();
	if (iBlitterIndex == -1)
	{
		FreeBackgroundRectNow(iBackIndex);
		return -1;
	}

	// Init new blitter
	VIDEO_OVERLAY& overlay = gVideoOverlays[iBlitterIndex];
	ResetOverlaySaveArea(overlay);
	memset(&overlay, 0, sizeof(overlay));

	overlay.uiFlags = uiFlags;
	overlay.fAllocated = 2;
	overlay.uiBackground = iBackIndex;
	overlay.pBackground = &gBackSaves[iBackIndex];
	overlay.BltCallback = pTopmostDesc->BltCallback;

	// Update blitter info
	// Set update flags to zero since we are forcing all updates
	if (!UpdateVideoOverlay(pTopmostDesc, iBlitterIndex, TRUE))
	{
		memset(&overlay, 0, sizeof(overlay));
		FreeBackgroundRectNow(iBackIndex);
		return -1;
	}
	pTopmostDesc->uiFlags = 0;

	// Set disabled flag to true
	if ( uiFlags & VOVERLAY_STARTDISABLED )
	{
		overlay.fDisabled = TRUE;
		DisableBackgroundRect(overlay.uiBackground, TRUE);
	}

	overlay.uiDestBuff = FRAME_BUFFER;
	if (static_cast<UINT32>(iBlitterIndex) == guiNumVideoOverlays)
		++guiNumVideoOverlays;

	//DebugMsg( TOPIC_JA2, DBG_LEVEL_0, String( "Register Overlay %d %S", iBlitterIndex, gVideoOverlays[ iBlitterIndex ].zText ) );

	return( iBlitterIndex );
}

void SetVideoOverlayPendingDelete( INT32 iVideoOverlay )
{
	if (IsOverlayIndex(iVideoOverlay) &&
		gVideoOverlays[iVideoOverlay].fAllocated)
	{
		gVideoOverlays[ iVideoOverlay ].fDeletionPending = TRUE;
	}
}

void RemoveVideoOverlay( INT32 iVideoOverlay )
{
	if (IsOverlayIndex(iVideoOverlay) && gVideoOverlays[iVideoOverlay].fAllocated)
	{
		// Check if we are actively scrolling
		if ( gVideoOverlays[ iVideoOverlay ].fActivelySaving )
		{
	//		DebugMsg( TOPIC_JA2, DBG_LEVEL_0, String( "Overlay Actively saving %d %S", iVideoOverlay, gVideoOverlays[ iVideoOverlay ].zText ) );

			gVideoOverlays[ iVideoOverlay ].fDeletionPending = TRUE;
		}
		else
		{
			//RestoreExternBackgroundRectGivenID( gVideoOverlays[ iVideoOverlay ].uiBackground );

			// Remove background
			FreeBackgroundRect( gVideoOverlays[ iVideoOverlay ].uiBackground );

			//DebugMsg( TOPIC_JA2, DBG_LEVEL_0, String( "Delete Overlay %d %S", iVideoOverlay, gVideoOverlays[ iVideoOverlay ].zText ) );

			// Remove save buffer if not done so
			ResetOverlaySaveArea(gVideoOverlays[iVideoOverlay]);
			memset(&gVideoOverlays[iVideoOverlay], 0,
				sizeof(gVideoOverlays[iVideoOverlay]));
			RecountVideoOverlays();
		}
	}
}

BOOLEAN UpdateVideoOverlay( VIDEO_OVERLAY_DESC *pTopmostDesc, UINT32 iBlitterIndex, BOOLEAN fForceAll )
{
	if (!pTopmostDesc || iBlitterIndex >= VIDEO_OVERLAYS) return FALSE;
	VIDEO_OVERLAY& overlay = gVideoOverlays[iBlitterIndex];
	if (!overlay.fAllocated) return FALSE;
	VIDEO_OVERLAY staged = overlay;
	INT32 replacementBackground = -1;

	if (fForceAll)
	{
		staged.uiFontID = pTopmostDesc->uiFontID;
		staged.sX = pTopmostDesc->sX;
		staged.sY = pTopmostDesc->sY;
		staged.ubFontBack = pTopmostDesc->ubFontBack;
		staged.ubFontFore = pTopmostDesc->ubFontFore;
		CopyOverlayText(staged, pTopmostDesc->pzText);
	}
	else
	{
		const UINT32 uiFlags = pTopmostDesc->uiFlags;
		if (uiFlags & VOVERLAY_DESC_TEXT)
			CopyOverlayText(staged, pTopmostDesc->pzText);

		if (uiFlags & VOVERLAY_DESC_FONT)
		{
			staged.uiFontID = pTopmostDesc->uiFontID;
			staged.ubFontBack = pTopmostDesc->ubFontBack;
			staged.ubFontFore = pTopmostDesc->ubFontFore;
		}

		if (uiFlags & VOVERLAY_DESC_DISABLED)
			staged.fDisabled = pTopmostDesc->fDisabled;

		if ((uiFlags & VOVERLAY_DESC_POSITION) &&
			(staged.uiFlags & VOVERLAY_DIRTYBYTEXT))
		{
			const UINT16 stringLength =
				MeasureOverlayTextWidth(staged.zText, staged.uiFontID);
			const UINT16 stringHeight =
				MeasureOverlayTextHeight(staged.uiFontID);
			INT16 textRight = 0;
			INT16 textBottom = 0;
			if (!CheckedCoordinateEnd(pTopmostDesc->sLeft, stringLength,
					textRight) ||
				!CheckedCoordinateEnd(pTopmostDesc->sTop, stringHeight,
					textBottom)) return FALSE;
			replacementBackground = RegisterBackgroundRect(
				BGND_FLAG_PERMANENT, NULL,
				pTopmostDesc->sLeft, pTopmostDesc->sTop,
				textRight, textBottom);
			if (replacementBackground == -1) return FALSE;
			staged.uiBackground = replacementBackground;
			staged.pBackground = &gBackSaves[replacementBackground];
			staged.sX = pTopmostDesc->sX;
			staged.sY = pTopmostDesc->sY;
		}
	}

	const INT32 previousBackground = overlay.uiBackground;
	if (replacementBackground != -1)
	{
		// The scroll-save buffer is dimensioned from the background rectangle.
		// A replacement must never retain storage with the old pitch/extent.
		ResetOverlaySaveArea(overlay);
		staged.pSaveArea = nullptr;
		staged.fActivelySaving = FALSE;
	}
	overlay = staged;
	if (replacementBackground != -1)
		FreeBackgroundRectPending(previousBackground);
	if (!fForceAll && (pTopmostDesc->uiFlags & VOVERLAY_DESC_DISABLED))
		DisableBackgroundRect(overlay.uiBackground, overlay.fDisabled);
	return TRUE;
}

// FUnctions for entrie array of blitters
void ExecuteVideoOverlays( )
{
	UINT32 uiCount;

	for(uiCount=0; uiCount < guiNumVideoOverlays; uiCount++)
	{
		if( gVideoOverlays[uiCount].fAllocated )
		{
			if ( !gVideoOverlays[uiCount].fDisabled )
			{
				// If we are scrolling but havn't saved yet, don't!
				if ( !gVideoOverlays[uiCount].fActivelySaving && gfScrollInertia > 0 )
				{
					continue;
				}

				// ATE: Wait a frame before executing!
				if ( gVideoOverlays[uiCount].fAllocated == 1 &&
					gVideoOverlays[uiCount].BltCallback )
				{
					// Call Blit Function
					(*(gVideoOverlays[uiCount].BltCallback ) ) ( &(gVideoOverlays[uiCount]) );
				}
				else if ( gVideoOverlays[uiCount].fAllocated == 2 )
				{
					gVideoOverlays[uiCount].fAllocated = 1;
				}
			}

			// Remove if pending
			//if ( gVideoOverlays[uiCount].fDeletionPending )
			//{
			//	RemoveVideoOverlay( uiCount );
			//}
		}
	}

}

void ExecuteVideoOverlaysToAlternateBuffer( UINT32 uiNewDestBuffer )
{
	UINT32	uiCount;
	UINT32	uiOldDestBuffer;

	for(uiCount=0; uiCount < guiNumVideoOverlays; uiCount++)
	{
		if( gVideoOverlays[uiCount].fAllocated && !gVideoOverlays[uiCount].fDisabled )
		{
			if ( gVideoOverlays[uiCount].fActivelySaving &&
				gVideoOverlays[uiCount].BltCallback )
			{
				uiOldDestBuffer =	gVideoOverlays[uiCount].uiDestBuff;

				gVideoOverlays[uiCount].uiDestBuff = uiNewDestBuffer;

				// Call Blit Function
				(*(gVideoOverlays[uiCount].BltCallback ) ) ( &(gVideoOverlays[uiCount]) );

				gVideoOverlays[uiCount].uiDestBuff = uiOldDestBuffer;
			}
		}
	}
}

void AllocateVideoOverlaysArea( )
{
	for(UINT32 uiCount=0; uiCount < guiNumVideoOverlays; uiCount++)
	{
		if( gVideoOverlays[uiCount].fAllocated && !gVideoOverlays[uiCount].fDisabled )
		{
			AllocateVideoOverlayArea(uiCount);
		}
	}
}

void AllocateVideoOverlayArea( UINT32 uiCount )
{
	if (uiCount >= VIDEO_OVERLAYS) return;
	VIDEO_OVERLAY& overlay = gVideoOverlays[uiCount];
	if (overlay.fAllocated && !overlay.fDisabled)
	{
		if (overlay.pSaveArea)
		{
			overlay.fActivelySaving = TRUE;
			return;
		}
		const INT32 iBackIndex = overlay.uiBackground;
		if (!IsBackgroundIndex(iBackIndex) ||
			!gBackSaves[iBackIndex].fAllocated) return;
		UINT32 bufferSize = 0;
		if (!CheckedBufferSize(gBackSaves[iBackIndex].sWidth,
			gBackSaves[iBackIndex].sHeight, sizeof(PIXEL), bufferSize)) return;
		INT16* const saveArea =
			static_cast<INT16*>(AllocateRenderDirtyBuffer(bufferSize));
		if (!saveArea) return;
		if (!RegisterApplicationBuffer(saveArea,
			gBackSaves[iBackIndex].sWidth,
			gBackSaves[iBackIndex].sHeight))
		{
			MemFree(saveArea);
			return;
		}
		overlay.pSaveArea = saveArea;
		overlay.fActivelySaving = TRUE;
	}
}

void SaveVideoOverlaysArea( UINT32 uiSrcBuffer )
{
	UINT32 uiCount;
	INT32 iBackIndex;
	UINT32 uiSrcPitchBYTES;
	UINT8	*pSrcBuf;

	pSrcBuf = LockVideoSurface( uiSrcBuffer, &uiSrcPitchBYTES );
	if (!pSrcBuf) return;

	for(uiCount=0; uiCount < guiNumVideoOverlays; uiCount++)
	{
		if( gVideoOverlays[uiCount].fAllocated && !gVideoOverlays[uiCount].fDisabled )
			{
				iBackIndex = gVideoOverlays[uiCount].uiBackground;
				if (!IsBackgroundIndex(iBackIndex) ||
					!gBackSaves[iBackIndex].fAllocated) continue;

			// OK, if our saved area is null, allocate it here!
			if ( gVideoOverlays[uiCount].pSaveArea == NULL )
			{
				AllocateVideoOverlayArea( uiCount );
			}

			if ( gVideoOverlays[uiCount].pSaveArea != NULL && gBackSaves[iBackIndex].sHeight > 0 && gBackSaves[iBackIndex].sWidth > 0)
			{
				// Save data from frame buffer!				
				Blt16BPPTo16BPP(
					(PIXEL *)gVideoOverlays[uiCount].pSaveArea,
					gBackSaves[iBackIndex].sWidth * sizeof(PIXEL),
					(PIXEL *)pSrcBuf,
					uiSrcPitchBYTES,
					0, 
					0,
					gBackSaves[iBackIndex].sLeft, 
					gBackSaves[iBackIndex].sTop,
					gBackSaves[iBackIndex].sWidth, 
					gBackSaves[iBackIndex].sHeight
				);				
			}
		}
	}
	UnLockVideoSurface( uiSrcBuffer );
}

void SaveVideoOverlayArea( UINT32 uiSrcBuffer, UINT32 uiCount )
{
	UINT32 iBackIndex;
	UINT32 uiSrcPitchBYTES;
	UINT8	*pSrcBuf;

	if (uiCount >= VIDEO_OVERLAYS) return;
	pSrcBuf = LockVideoSurface( uiSrcBuffer, &uiSrcPitchBYTES );
	if (!pSrcBuf) return;

	if( gVideoOverlays[uiCount].fAllocated && !gVideoOverlays[uiCount].fDisabled )
	{
		// OK, if our saved area is null, allocate it here!
		if ( gVideoOverlays[uiCount].pSaveArea == NULL )
		{
			AllocateVideoOverlayArea( uiCount );
		}

		const INT32 backgroundIndex = gVideoOverlays[uiCount].uiBackground;
		if (gVideoOverlays[uiCount].pSaveArea != NULL &&
			IsBackgroundIndex(backgroundIndex) &&
			gBackSaves[backgroundIndex].fAllocated)
		{
			iBackIndex = static_cast<UINT32>(backgroundIndex);

			// Save data from frame buffer!
			Blt16BPPTo16BPP((PIXEL *)gVideoOverlays[uiCount].pSaveArea, gBackSaves[ iBackIndex ].sWidth*sizeof(PIXEL),
						(PIXEL *)pSrcBuf, uiSrcPitchBYTES,
						0, 0,
						gBackSaves[ iBackIndex ].sLeft , gBackSaves[ iBackIndex ].sTop,
						gBackSaves[ iBackIndex ].sWidth, gBackSaves[ iBackIndex ].sHeight );
		}
	}
	UnLockVideoSurface( uiSrcBuffer );
}

void DeleteVideoOverlaysArea( )
{
	UINT32 uiCount;

	for(uiCount=0; uiCount < guiNumVideoOverlays; uiCount++)
	{
		if(gVideoOverlays[uiCount].fAllocated)
		{
			ResetOverlaySaveArea(gVideoOverlays[uiCount]);

			//DebugMsg( TOPIC_JA2, DBG_LEVEL_0, String( "Removing Overlay Actively saving %d %S", uiCount, gVideoOverlays[ uiCount ].zText ) );

			// Remove if pending
			if ( gVideoOverlays[uiCount].fDeletionPending )
			{
				RemoveVideoOverlay( uiCount );
			}
		}
	}
}

BOOLEAN RestoreShiftedVideoOverlays( INT16 sShiftX, INT16 sShiftY )
{
	UINT32 uiCount, uiDestPitchBYTES;
	UINT8	*pDestBuf;
	INT32 iBackIndex;

	INT32	ClipX1, ClipY1, ClipX2, ClipY2;
	INT32	uiLeftSkip, uiRightSkip, uiTopSkip, uiBottomSkip;
	UINT32	usHeight, usWidth;
	INT32	iTempX, iTempY;
	INT16	sLeft, sTop, sRight, sBottom;

	ClipX1= 0;
	ClipY1= gsVIEWPORT_WINDOW_START_Y;
	ClipX2= SCREEN_WIDTH;
	ClipY2= gsVIEWPORT_WINDOW_END_Y - 1;


	pDestBuf = LockVideoSurface( BACKBUFFER, &uiDestPitchBYTES);

	for(uiCount=0; uiCount <	guiNumVideoOverlays; uiCount++)
	{
		if( gVideoOverlays[uiCount].fAllocated && !gVideoOverlays[uiCount].fDisabled )
		{
				iBackIndex = gVideoOverlays[uiCount].uiBackground;
					if (!IsBackgroundIndex(iBackIndex) ||
					!gBackSaves[iBackIndex].fAllocated) continue;

				if ( gVideoOverlays[uiCount].pSaveArea != NULL )
				{
					// Get restore background values
					sLeft			= gBackSaves[ iBackIndex ].sLeft;
					sTop		= gBackSaves[ iBackIndex ].sTop;
					sRight		= gBackSaves[ iBackIndex ].sRight;
					sBottom		= gBackSaves[ iBackIndex ].sBottom;
					usHeight	= gBackSaves[ iBackIndex ].sHeight;
					usWidth	= gBackSaves[ iBackIndex ].sWidth;

					// Clip!!
					iTempX = sLeft + sShiftX;
					iTempY = sTop + sShiftY;

					// Clip to rect
					uiLeftSkip=__min( ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
					uiRightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
					uiTopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
					uiBottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

					// check if whole thing is clipped
					if((uiLeftSkip >=(INT32)usWidth) || (uiRightSkip >=(INT32)usWidth))
						continue;

					// check if whole thing is clipped
					if((uiTopSkip >=(INT32)usHeight) || (uiBottomSkip >=(INT32)usHeight))
						continue;

					// Set re-set values given based on clipping
					sLeft	= iTempX + (INT16)uiLeftSkip;
					sTop	 = iTempY + (INT16)uiTopSkip;
					sRight	= sRight + sShiftX - (INT16)uiRightSkip;
					sBottom	= sBottom + sShiftY - (INT16)uiBottomSkip;

					usHeight = sBottom - sTop;
					usWidth	= sRight -	sLeft;

					Blt16BPPTo16BPP((PIXEL *)pDestBuf, uiDestPitchBYTES,
								(PIXEL *)gVideoOverlays[uiCount].pSaveArea, gBackSaves[ iBackIndex ].sWidth*sizeof(PIXEL),
								sLeft, sTop,
								uiLeftSkip, uiTopSkip,
								usWidth, usHeight );

					// Once done, check for pending deletion
					if ( gVideoOverlays[uiCount].fDeletionPending )
					{
						RemoveVideoOverlay( uiCount );
					}
				}
		}
	}

	UnLockVideoSurface( BACKBUFFER );

	return(TRUE);
}

BOOLEAN SetOverlayUserData( INT32 iVideoOverlay, UINT8 ubNum, UINT32 uiData )
{
	if (!IsOverlayIndex(iVideoOverlay) ||
		!gVideoOverlays[iVideoOverlay].fAllocated)
	{
		return( FALSE );
	}

	if ( ubNum > 4 )
	{
		return( FALSE );
	}

	gVideoOverlays[ iVideoOverlay ].uiUserData[ ubNum ] = uiData;

	return ( TRUE );
}


// Common callbacks for topmost blitters
void BlitMFont( VIDEO_OVERLAY *pBlitter )
{
	UINT8	*pDestBuf;
	UINT32 uiDestPitchBYTES;

	pDestBuf = LockVideoSurface( pBlitter->uiDestBuff, &uiDestPitchBYTES);

	SetFont( pBlitter->uiFontID );
	SetFontBackground( pBlitter->ubFontBack );
	SetFontForeground( pBlitter->ubFontFore );

	mprintf_buffer( pDestBuf, uiDestPitchBYTES, pBlitter->uiFontID, pBlitter->sX, pBlitter->sY, pBlitter->zText );

	UnLockVideoSurface( pBlitter->uiDestBuff );
}

BOOLEAN BlitBufferToBuffer(UINT32 uiSrcBuffer, UINT32 uiDestBuffer, UINT16 usSrcX, UINT16 usSrcY, UINT16 usWidth, UINT16 usHeight)
{
	UINT32 uiDestPitchBYTES, uiSrcPitchBYTES;
	UINT8	*pDestBuf, *pSrcBuf;
	BOOLEAN fRetVal;

	pDestBuf = LockVideoSurface(uiDestBuffer, &uiDestPitchBYTES);
	pSrcBuf = LockVideoSurface(uiSrcBuffer, &uiSrcPitchBYTES);

	fRetVal = Blt16BPPTo16BPP( (PIXEL *)pDestBuf, uiDestPitchBYTES, (PIXEL *)pSrcBuf, uiSrcPitchBYTES,
			usSrcX, usSrcY,
			usSrcX, usSrcY,
			usWidth, usHeight);

	UnLockVideoSurface(uiDestBuffer);
	UnLockVideoSurface(uiSrcBuffer);

	return( fRetVal );
}

void EnableVideoOverlay( BOOLEAN fEnable, INT32 iOverlayIndex )
{
	VIDEO_OVERLAY_DESC		VideoOverlayDesc;

	memset( &VideoOverlayDesc, 0, sizeof( VideoOverlayDesc ) );

	// enable or disable
	VideoOverlayDesc.fDisabled	= !fEnable;

	// go play with enable/disable state
	VideoOverlayDesc.uiFlags	= VOVERLAY_DESC_DISABLED;

	UpdateVideoOverlay( &VideoOverlayDesc, iOverlayIndex, FALSE );
}
