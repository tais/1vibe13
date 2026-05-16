// SDL3-backed video surface manager. Phase 5 second slice.
//
// Ports the SGPVSurface plumbing out of the DirectDraw-driven
// vsurface.cpp: surface manager (linked list keyed by index), surface
// creation (empty + from-file via HIMAGE), lock/unlock returning the
// heap pixel buffer, transparency colour, palette, primary surface
// wrappers, and the SurfaceData + ClipRectangle helpers.
//
// Each SGPVSurface owns a heap byte buffer (UINT16* for 16bpp, UINT8*
// for 8bpp) instead of a DirectDraw surface. The hVSurface->pSurfaceData
// field that legacy code used to hold a LPDIRECTDRAWSURFACE2 now holds
// that heap pointer directly.
//
// Still stubbed in sgp_portable_stubs.cpp:
//   - Blitters: BltVideoSurface, BltVideoSurfaceToVideoSurface,
//     BltStretchVideoSurface, BltVSurfaceUsingDD,
//     ColorFillVideoSurfaceArea, ImageFillVideoSurfaceArea,
//     ShadowVideoSurfaceRect{,Image,UsingLowPercentTable}.
//   - Restore/Backup logic (no DD restore semantics on SDL3 path).

#include "types.h"
#include "vsurface.h"
#include "vobject_blitters.h"
#include "himage.h"
#include "video.h"
#include "MemMan.h"
#include "WCheck.h"
#include "DEBUG.H"

#include <cstdlib>
#include <cstring>
#include <map>

// g_SurfaceRectangle is defined by vobject_blitters.cpp; pulled in
// here as extern so the SurfaceData registry can keep the per-surface
// clipping rectangle in sync.
extern std::map<UINT32, ClipRectangle> g_SurfaceRectangle;

// iUseWinFonts is a JA2 flag controlling whether GDI-rendered text is
// composed onto the locked surface. Declared in Ja2/local.h.
extern int iUseWinFonts;

// CurrentSurface is declared in sgp_portable_globals.cpp; LockVideoSurface
// has to update it when iUseWinFonts is set so WinFont knows which
// surface to draw into.
extern UINT32 CurrentSurface;

namespace SurfaceData
{
	typedef void* tSurface;
	std::map<tID, tSurface>  _surfaceID;
	std::map<tSurface, BYTE*> _surfaceData;
	std::map<BYTE*, tID>     _surfaceOfData;

	void RegisterSurface(tID surfaceID, HVSURFACE surface)
	{
		SurfaceData::_surfaceID[surfaceID] = surface;
		g_SurfaceRectangle[surfaceID].SetRect(surface->usWidth, surface->usHeight);
	}
	void UnRegisterSurface(tID surfaceID)
	{
		std::map<tID, tSurface>::iterator it = SurfaceData::_surfaceID.find(surfaceID);
		if (it != SurfaceData::_surfaceID.end())
		{
			g_SurfaceRectangle.erase(it->first);
			SurfaceData::_surfaceID.erase(it);
		}
	}
	void UnRegisterSurface(HVSURFACE surface)
	{
		std::map<tID, tSurface>::iterator it = SurfaceData::_surfaceID.begin();
		for (; it != SurfaceData::_surfaceID.end(); ++it)
		{
			if (it->second == surface)
			{
				SurfaceData::_surfaceID.erase(it);
				return;
			}
		}
	}

	BYTE* SetSurfaceData(tID surfaceID, BYTE* data)
	{
		std::map<tID, tSurface>::iterator sit = SurfaceData::_surfaceID.find(surfaceID);
		if (sit != SurfaceData::_surfaceID.end())
		{
			SurfaceData::_surfaceData[sit->second] = data;
			SurfaceData::_surfaceOfData[data] = surfaceID;
			return data;
		}
		SGP_THROW(L"Unregistered surface ID");
	}
	BYTE* SetSurfaceData(HVSURFACE surface, BYTE* data)
	{
		std::map<tID, tSurface>::iterator sit = SurfaceData::_surfaceID.begin();
		for (; sit != SurfaceData::_surfaceID.end(); ++sit)
		{
			// NB: legacy vsurface.cpp had `sit->second = surface` (single =)
			// here, which is almost certainly a bug -- it assigns rather
			// than compares. Preserved as `==` so the call actually checks
			// surface identity the way the function name and docs imply.
			if (sit->second == surface)
			{
				SurfaceData::_surfaceData[sit->second] = data;
				SurfaceData::_surfaceOfData[data] = sit->first;
				return data;
			}
		}
		SGP_THROW(L"Unregistered surface");
	}
	void ReleaseSurfaceData(tID surfaceID)
	{
		std::map<tID, tSurface>::iterator sit = SurfaceData::_surfaceID.find(surfaceID);
		if (sit != SurfaceData::_surfaceID.end())
		{
			std::map<tSurface, BYTE*>::iterator dit = SurfaceData::_surfaceData.find(sit->second);
			if (dit != SurfaceData::_surfaceData.end())
			{
				std::map<BYTE*, tID>::iterator it = SurfaceData::_surfaceOfData.find(dit->second);
				if (it != SurfaceData::_surfaceOfData.end())
				{
					SurfaceData::_surfaceOfData.erase(it);
				}
				SurfaceData::_surfaceData.erase(dit);
			}
		}
	}
	void ReleaseSurfaceData(HVSURFACE surface)
	{
		std::map<tSurface, BYTE*>::iterator dit = SurfaceData::_surfaceData.find(surface);
		if (dit != SurfaceData::_surfaceData.end())
		{
			std::map<BYTE*, tID>::iterator it = SurfaceData::_surfaceOfData.find(dit->second);
			if (it != SurfaceData::_surfaceOfData.end())
			{
				_surfaceOfData.erase(it);
			}
			SurfaceData::_surfaceData.erase(dit);
		}
	}

	BYTE* SetApplicationData(BYTE* data)
	{
		tID id = (tID)(data);
		SurfaceData::_surfaceOfData[data] = id;
		return data;
	}
	void ReleaseApplicationData(BYTE* data)
	{
		tID id = (tID)(data);
		std::map<BYTE*, tID>::iterator it = SurfaceData::_surfaceOfData.find(data);
		if (it != SurfaceData::_surfaceOfData.end())
		{
			g_SurfaceRectangle.erase(it->second);
			SurfaceData::_surfaceOfData.erase(it);
		}
		return ReleaseSurfaceData(id);
	}

	tID GetSurfaceID(BYTE* data)
	{
		std::map<BYTE*, tID>::iterator it = SurfaceData::_surfaceOfData.find(data);
		if (it != SurfaceData::_surfaceOfData.end())
		{
			return it->second;
		}
		return 0;
	}
} // namespace SurfaceData

ClipRectangle::ClipRectangle()
{
	cr.iLeft = 0;
	cr.iTop = 0;
	cr.iRight = 0;
	cr.iBottom = 0;
}

void ClipRectangle::SetRect(SGPRect const& rect)
{
	Set(rect.iLeft, rect.iTop, rect.iRight, rect.iBottom);
}
void ClipRectangle::SetRect(unsigned int w, unsigned int h, int x, int y)
{
	Set(x, y, x + (int)w, y + (int)h);
}
void ClipRectangle::Set(int x1, int y1, int x2, int y2)
{
	cr.iLeft = x1;
	cr.iRight = x2;
	cr.iTop = y1;
	cr.iBottom = y2;
}

ClipRectangle::ClipType ClipRectangle::Clip(int& x, int& y, unsigned int& w, unsigned int& h)
{
	int right = x + (int)w - 1;
	int bottom = y + (int)h - 1;
	ClipType ct;
	if ((ct = Clip(x, y, right, bottom)) == PartialClip)
	{
		w = right - x + 1;
		h = bottom - y + 1;
	}
	return ct;
}
ClipRectangle::ClipType ClipRectangle::Clip(int& x1, int& y1, int& x2, int& y2)
{
	if ((x1 >= cr.iLeft) &&
	    (x2 <= cr.iRight) &&
	    (y1 >= cr.iTop) &&
	    (y2 <= cr.iBottom))
	{
		return NoClip;
	}
	if ((x1 > cr.iRight) ||
	    (x2 < cr.iLeft) ||
	    (y1 > cr.iBottom) ||
	    (y2 < cr.iTop))
	{
		return FullClip;
	}
	if (x1 < cr.iLeft)   x1 = cr.iLeft;
	if (x2 > cr.iRight)  x2 = cr.iRight;
	if (y1 < cr.iTop)    y1 = cr.iTop;
	if (y2 > cr.iBottom) y2 = cr.iBottom;
	return PartialClip;
}

///////////////////////////////////////////////////////////////////////////////
// SGPVSurface manager
///////////////////////////////////////////////////////////////////////////////

INT32 giMemUsedInSurfaces = 0;

HVSURFACE ghPrimary    = nullptr;
HVSURFACE ghBackBuffer = nullptr;
HVSURFACE ghFrameBuffer = nullptr;
HVSURFACE ghMouseBuffer = nullptr;

namespace {

struct VSURFACE_NODE
{
	HVSURFACE hVSurface;
	UINT32    uiIndex;
	VSURFACE_NODE* prev;
	VSURFACE_NODE* next;
};

VSURFACE_NODE* gpVSurfaceHead = nullptr;
VSURFACE_NODE* gpVSurfaceTail = nullptr;
UINT32 guiVSurfaceIndex = 0;
UINT32 guiVSurfaceSize = 0;
UINT32 guiVSurfaceTotalAdded = 0;

UINT32 BytesPerPixelFor(UINT8 bpp)
{
	return (bpp <= 8) ? 1u : 2u;  // SDL3 path is 8bpp source or RGB565 dest only
}

UINT32 BufferBytes(UINT16 w, UINT16 h, UINT8 bpp)
{
	return (UINT32)w * (UINT32)h * BytesPerPixelFor(bpp);
}

// Buffer ownership: most surfaces malloc/free their own pSurfaceData.
// The primary/back/frame/mouse "reserved" surfaces wrap buffers owned
// by sdl_video.cpp instead -- their VSURFACE_RESERVED_SURFACE flag
// tells the destructor not to free.

void FreeSurfaceBuffer(HVSURFACE s)
{
	if (!s) return;
	if (!(s->fFlags & VSURFACE_RESERVED_SURFACE) && s->pSurfaceData)
	{
		std::free(s->pSurfaceData);
	}
	s->pSurfaceData = nullptr;
}

void FreePalette(HVSURFACE s)
{
	if (!s) return;
	if (s->pPalette)      { std::free(s->pPalette);      s->pPalette = nullptr; }
	if (s->p16BPPPalette) { MemFree(s->p16BPPPalette);    s->p16BPPPalette = nullptr; }
}

// Build an HVSURFACE wrapper around a buffer we do or don't own.
HVSURFACE NewSurface(UINT16 w, UINT16 h, UINT8 bpp, void* externalBuffer)
{
	HVSURFACE s = new SGPVSurface{};
	s->usHeight = h;
	s->usWidth  = w;
	s->ubBitDepth = (bpp > 16) ? 16 : bpp;
	if (externalBuffer)
	{
		s->pSurfaceData = externalBuffer;
		s->fFlags = VSURFACE_RESERVED_SURFACE;
	}
	else
	{
		s->pSurfaceData = std::calloc(1, BufferBytes(w, h, s->ubBitDepth));
		s->fFlags = 0;
		giMemUsedInSurfaces += (INT32)BufferBytes(w, h, s->ubBitDepth);
	}
	s->pSurfaceData1     = nullptr;
	s->pSavedSurfaceData = nullptr;
	s->pSavedSurfaceData1 = nullptr;
	s->pPalette          = nullptr;
	s->p16BPPPalette     = nullptr;
	s->TransparentColor  = FROMRGB(0, 0, 0);
	s->pClipper          = nullptr;
	return s;
}

} // namespace

BOOLEAN DeleteVideoSurface(HVSURFACE hVSurface)
{
	if (!hVSurface) return FALSE;
	if (!(hVSurface->fFlags & VSURFACE_RESERVED_SURFACE))
	{
		giMemUsedInSurfaces -= (INT32)BufferBytes(hVSurface->usWidth,
		                                          hVSurface->usHeight,
		                                          hVSurface->ubBitDepth);
	}
	FreeSurfaceBuffer(hVSurface);
	FreePalette(hVSurface);
	delete hVSurface;
	return TRUE;
}

static void DeletePrimaryVideoSurfaces()
{
	if (ghPrimary)    { SurfaceData::UnRegisterSurface(ghPrimary);    DeleteVideoSurface(ghPrimary);    ghPrimary    = nullptr; }
	if (ghBackBuffer) { SurfaceData::UnRegisterSurface(ghBackBuffer); DeleteVideoSurface(ghBackBuffer); ghBackBuffer = nullptr; }
	if (ghFrameBuffer){ SurfaceData::UnRegisterSurface(ghFrameBuffer);DeleteVideoSurface(ghFrameBuffer);ghFrameBuffer= nullptr; }
	if (ghMouseBuffer){ SurfaceData::UnRegisterSurface(ghMouseBuffer);DeleteVideoSurface(ghMouseBuffer);ghMouseBuffer= nullptr; }
}

BOOLEAN SetPrimaryVideoSurfaces()
{
	DeletePrimaryVideoSurfaces();

	extern UINT16 SCREEN_WIDTH;
	extern UINT16 SCREEN_HEIGHT;

	UINT32 pitch = 0;
	void* primBuf  = LockPrimarySurface(&pitch); UnlockPrimarySurface();
	void* backBuf  = LockBackBuffer(&pitch);     UnlockBackBuffer();
	void* frameBuf = LockFrameBuffer(&pitch);    UnlockFrameBuffer();
	void* mouseBuf = LockMouseBuffer(&pitch);    UnlockMouseBuffer();
	if (!primBuf || !backBuf || !frameBuf || !mouseBuf) return FALSE;

	ghPrimary     = NewSurface(SCREEN_WIDTH, SCREEN_HEIGHT, 16, primBuf);
	ghBackBuffer  = NewSurface(SCREEN_WIDTH, SCREEN_HEIGHT, 16, backBuf);
	ghFrameBuffer = NewSurface(SCREEN_WIDTH, SCREEN_HEIGHT, 16, frameBuf);
	ghMouseBuffer = NewSurface(MAX_CURSOR_WIDTH, MAX_CURSOR_HEIGHT, 16, mouseBuf);

	SurfaceData::RegisterSurface(PRIMARY_SURFACE, ghPrimary);
	SurfaceData::RegisterSurface(BACKBUFFER,      ghBackBuffer);
	SurfaceData::RegisterSurface(FRAME_BUFFER,    ghFrameBuffer);
	SurfaceData::RegisterSurface(MOUSE_BUFFER,    ghMouseBuffer);
	return TRUE;
}

BOOLEAN InitializeVideoSurfaceManager()
{
	Assert(!gpVSurfaceHead);
	Assert(!gpVSurfaceTail);
	RegisterDebugTopic(TOPIC_VIDEOSURFACE, "Video Surface Manager");
	gpVSurfaceHead = gpVSurfaceTail = nullptr;
	giMemUsedInSurfaces = 0;
	if (!SetPrimaryVideoSurfaces())
	{
		DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_1, String("Could not create primary surfaces"));
		return FALSE;
	}
	return TRUE;
}

BOOLEAN ShutdownVideoSurfaceManager()
{
	DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_0, "Shutting down the Video Surface manager");
	DeletePrimaryVideoSurfaces();
	while (gpVSurfaceHead)
	{
		VSURFACE_NODE* curr = gpVSurfaceHead;
		gpVSurfaceHead = gpVSurfaceHead->next;
		DeleteVideoSurface(curr->hVSurface);
		MemFree(curr);
	}
	gpVSurfaceHead = nullptr;
	gpVSurfaceTail = nullptr;
	guiVSurfaceIndex = 0;
	guiVSurfaceSize = 0;
	guiVSurfaceTotalAdded = 0;
	UnRegisterDebugTopic(TOPIC_VIDEOSURFACE, "Video Objects");
	return TRUE;
}

BOOLEAN RestoreVideoSurfaces() { return TRUE; }
BOOLEAN RestoreVideoSurface(HVSURFACE) { return TRUE; }

// Forward decls of surface-loading helpers that need access to file-IO
// types from himage.h. Implementations below.
BOOLEAN SetVideoSurfaceDataFromHImage(HVSURFACE hVSurface, HIMAGE hImage,
                                      UINT16 usX, UINT16 usY, SGPRect* pSrcRect);
BOOLEAN SetVideoSurfacePalette(HVSURFACE hVSurface, SGPPaletteEntry* pSrcPalette);

HVSURFACE CreateVideoSurface(VSURFACE_DESC* desc)
{
	if (!desc) return nullptr;
	HIMAGE hImage = nullptr;
	UINT16 usWidth = 0, usHeight = 0;
	UINT8 ubBitDepth = 0;

	if (desc->fCreateFlags & VSURFACE_CREATE_FROMFILE)
	{
		ImageFileType::TestOrder order = ImageFileType::JPC_FALLBACK;
		if (desc->fCreateFlags & VSURFACE_CREATE_FROMJPC) order = ImageFileType::JPC;
		else if (desc->fCreateFlags & VSURFACE_CREATE_FROMJPC_FALLBACK) order = ImageFileType::JPC_FALLBACK;
		else if (desc->fCreateFlags & VSURFACE_CREATE_FROMPNG) order = ImageFileType::PNG;
		else if (desc->fCreateFlags & VSURFACE_CREATE_FROMPNG_FALLBACK) order = ImageFileType::PNG_FALLBACK;

		SGP_THROW_IFFALSE(
			hImage = CreateImage(desc->ImageFile, IMAGE_ALLIMAGEDATA, order),
			_BS(L"Could not create video surface from file : ") << vfs::String(desc->ImageFile) << _BS::wget);
		if (!hImage) return nullptr;
		usWidth    = hImage->usWidth;
		usHeight   = hImage->usHeight;
		ubBitDepth = hImage->ubBitDepth;
	}
	else
	{
		usWidth    = (UINT16)desc->usWidth;
		usHeight   = (UINT16)desc->usHeight;
		ubBitDepth = desc->ubBitDepth;
	}
	Assert(usWidth > 0 && usHeight > 0);
	Assert(ubBitDepth == 8 || ubBitDepth == 16 || ubBitDepth == 24 || ubBitDepth == 32);

	HVSURFACE hVSurface = NewSurface(usWidth, usHeight, ubBitDepth, nullptr);

	if (desc->fCreateFlags & VSURFACE_CREATE_FROMFILE)
	{
		SGPRect tempRect{ 0, 0, hImage->usWidth - 1, hImage->usHeight - 1 };
		SetVideoSurfaceDataFromHImage(hVSurface, hImage, 0, 0, &tempRect);
		if (hImage->ubBitDepth == 8)
		{
			SetVideoSurfacePalette(hVSurface, hImage->pPalette);
		}
		DestroyImage(hImage);
	}

	DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_3, String("Success in Creating Video Surface"));
	return hVSurface;
}

BOOLEAN AddStandardVideoSurface(VSURFACE_DESC* pVSurfaceDesc, UINT32* puiIndex)
{
	Assert(puiIndex);
	Assert(pVSurfaceDesc);
	HVSURFACE hVSurface = CreateVideoSurface(pVSurfaceDesc);
	if (!hVSurface) return FALSE;

	SetVideoSurfaceTransparencyColor(hVSurface, FROMRGB(0, 0, 0));

	if (gpVSurfaceHead)
	{
		gpVSurfaceTail->next = (VSURFACE_NODE*)MemAlloc(sizeof(VSURFACE_NODE));
		Assert(gpVSurfaceTail->next);
		gpVSurfaceTail->next->prev = gpVSurfaceTail;
		gpVSurfaceTail->next->next = nullptr;
		gpVSurfaceTail = gpVSurfaceTail->next;
	}
	else
	{
		gpVSurfaceHead = (VSURFACE_NODE*)MemAlloc(sizeof(VSURFACE_NODE));
		Assert(gpVSurfaceHead);
		gpVSurfaceHead->prev = gpVSurfaceHead->next = nullptr;
		gpVSurfaceTail = gpVSurfaceHead;
	}
	gpVSurfaceTail->hVSurface = hVSurface;
	gpVSurfaceTail->uiIndex = (guiVSurfaceIndex += 2);
	*puiIndex = gpVSurfaceTail->uiIndex;
	SurfaceData::RegisterSurface(*puiIndex, hVSurface);
	Assert(guiVSurfaceIndex < 0xfffffff0u);
	guiVSurfaceSize++;
	guiVSurfaceTotalAdded++;
	return TRUE;
}

static VSURFACE_NODE* FindNodeByIndex(UINT32 uiIndex)
{
	for (VSURFACE_NODE* curr = gpVSurfaceHead; curr; curr = curr->next)
	{
		if (curr->uiIndex == uiIndex) return curr;
	}
	return nullptr;
}

BOOLEAN GetVideoSurface(HVSURFACE* hVSurface, UINT32 uiIndex)
{
	Assert(hVSurface);
	switch (uiIndex)
	{
	case PRIMARY_SURFACE: *hVSurface = ghPrimary;     return ghPrimary    != nullptr;
	case BACKBUFFER:      *hVSurface = ghBackBuffer;  return ghBackBuffer != nullptr;
	case FRAME_BUFFER:    *hVSurface = ghFrameBuffer; return ghFrameBuffer!= nullptr;
	case MOUSE_BUFFER:    *hVSurface = ghMouseBuffer; return ghMouseBuffer!= nullptr;
	default: break;
	}
	VSURFACE_NODE* curr = FindNodeByIndex(uiIndex);
	if (!curr) return FALSE;
	*hVSurface = curr->hVSurface;
	return TRUE;
}

BOOLEAN DeleteVideoSurfaceFromIndex(UINT32 uiIndex)
{
	VSURFACE_NODE* curr = FindNodeByIndex(uiIndex);
	if (!curr) return FALSE;
	SurfaceData::UnRegisterSurface(uiIndex);
	if (curr == gpVSurfaceHead) gpVSurfaceHead = curr->next;
	if (curr == gpVSurfaceTail) gpVSurfaceTail = curr->prev;
	if (curr->prev) curr->prev->next = curr->next;
	if (curr->next) curr->next->prev = curr->prev;
	DeleteVideoSurface(curr->hVSurface);
	MemFree(curr);
	guiVSurfaceSize--;
	return TRUE;
}

BOOLEAN GetVideoSurfaceDescription(UINT32 uiIndex, UINT16* usWidth, UINT16* usHeight, UINT8* ubBitDepth)
{
	HVSURFACE s = nullptr;
	if (!GetVideoSurface(&s, uiIndex) || !s) return FALSE;
	if (usWidth)    *usWidth    = s->usWidth;
	if (usHeight)   *usHeight   = s->usHeight;
	if (ubBitDepth) *ubBitDepth = s->ubBitDepth;
	return TRUE;
}

BYTE* LockVideoSurfaceBuffer(HVSURFACE hVSurface, UINT32* pPitch)
{
	Assert(hVSurface != nullptr);
	Assert(pPitch != nullptr);
	*pPitch = hVSurface->usWidth * BytesPerPixelFor(hVSurface->ubBitDepth);
	return (BYTE*)hVSurface->pSurfaceData;
}

void UnLockVideoSurfaceBuffer(HVSURFACE /*hVSurface*/) {}

BYTE* LockVideoSurface(UINT32 uiVSurface, UINT32* puiPitch)
{
	if (iUseWinFonts) CurrentSurface = uiVSurface;
	switch (uiVSurface)
	{
	case PRIMARY_SURFACE: return SurfaceData::SetSurfaceData(uiVSurface, (BYTE*)LockPrimarySurface(puiPitch));
	case BACKBUFFER:      return SurfaceData::SetSurfaceData(uiVSurface, (BYTE*)LockBackBuffer(puiPitch));
	case FRAME_BUFFER:    return SurfaceData::SetSurfaceData(uiVSurface, (BYTE*)LockFrameBuffer(puiPitch));
	case MOUSE_BUFFER:    return SurfaceData::SetSurfaceData(uiVSurface, (BYTE*)LockMouseBuffer(puiPitch));
	default: break;
	}
	VSURFACE_NODE* curr = FindNodeByIndex(uiVSurface);
	if (!curr) return nullptr;
	return SurfaceData::SetSurfaceData(uiVSurface, LockVideoSurfaceBuffer(curr->hVSurface, puiPitch));
}

void UnLockVideoSurface(UINT32 uiVSurface)
{
	SurfaceData::ReleaseSurfaceData(uiVSurface);
	switch (uiVSurface)
	{
	case PRIMARY_SURFACE: UnlockPrimarySurface(); return;
	case BACKBUFFER:      UnlockBackBuffer();     return;
	case FRAME_BUFFER:    UnlockFrameBuffer();    return;
	case MOUSE_BUFFER:    UnlockMouseBuffer();    return;
	default: break;
	}
	VSURFACE_NODE* curr = FindNodeByIndex(uiVSurface);
	if (!curr) return;
	UnLockVideoSurfaceBuffer(curr->hVSurface);
}

BOOLEAN SetVideoSurfaceTransparencyColor(HVSURFACE hVSurface, COLORVAL TransColor)
{
	if (!hVSurface) return FALSE;
	hVSurface->TransparentColor = TransColor;
	return TRUE;
}

BOOLEAN SetVideoSurfaceTransparency(UINT32 uiIndex, COLORVAL TransColor)
{
	HVSURFACE s = nullptr;
	if (!GetVideoSurface(&s, uiIndex) || !s) return FALSE;
	return SetVideoSurfaceTransparencyColor(s, TransColor);
}

BOOLEAN SetVideoSurfacePalette(HVSURFACE hVSurface, SGPPaletteEntry* pSrcPalette)
{
	if (!hVSurface || !pSrcPalette) return FALSE;
	if (!hVSurface->pPalette)
	{
		hVSurface->pPalette = std::malloc(sizeof(SGPPaletteEntry) * 256);
		if (!hVSurface->pPalette) return FALSE;
	}
	std::memcpy(hVSurface->pPalette, pSrcPalette, sizeof(SGPPaletteEntry) * 256);

	if (hVSurface->p16BPPPalette) MemFree(hVSurface->p16BPPPalette);
	hVSurface->p16BPPPalette = Create16BPPPalette(pSrcPalette);
	return hVSurface->p16BPPPalette != nullptr;
}

BOOLEAN GetVSurfacePaletteEntries(HVSURFACE hVSurface, SGPPaletteEntry* pPalette)
{
	if (!hVSurface || !pPalette || !hVSurface->pPalette) return FALSE;
	std::memcpy(pPalette, hVSurface->pPalette, sizeof(SGPPaletteEntry) * 256);
	return TRUE;
}

// Copy an HIMAGE's bitmap data into the surface's pixel buffer.
// Uses CopyImageToBuffer which knows how to fan out 8bpp source into
// either an 8bpp or 16bpp destination (palette-converted).
BOOLEAN SetVideoSurfaceDataFromHImage(HVSURFACE hVSurface, HIMAGE hImage,
                                      UINT16 usX, UINT16 usY, SGPRect* pSrcRect)
{
	Assert(hVSurface);
	Assert(hImage);
	CHECKF(hImage->usWidth  >= hVSurface->usWidth);
	CHECKF(hImage->usHeight >= hVSurface->usHeight);

	UINT32 fBufferBPP = 0;
	if (hImage->ubBitDepth != hVSurface->ubBitDepth)
	{
		if (hImage->ubBitDepth == 8  && hVSurface->ubBitDepth == 16) fBufferBPP = BUFFER_16BPP;
		else if ((hImage->ubBitDepth == 24 || hImage->ubBitDepth == 32) && hVSurface->ubBitDepth == 16) fBufferBPP = BUFFER_16BPP;
	}
	else
	{
		fBufferBPP = (hImage->ubBitDepth == 8) ? BUFFER_8BPP : BUFFER_16BPP;
	}
	Assert(fBufferBPP != 0);

	UINT32 uiPitch = 0;
	BYTE* pDest = LockVideoSurfaceBuffer(hVSurface, &uiPitch);
	if (!pDest) return FALSE;
	BOOLEAN ok = CopyImageToBuffer(hImage, fBufferBPP, pDest,
	                               hVSurface->usWidth, hVSurface->usHeight,
	                               usX, usY, pSrcRect);
	UnLockVideoSurfaceBuffer(hVSurface);
	return ok;
}

HVSURFACE GetPrimaryVideoSurface()    { return ghPrimary; }
HVSURFACE GetBackBufferVideoSurface() { return ghBackBuffer; }

// Region machinery (RegionList on the surface itself). Minimal
// pass-throughs; the game uses these for hit-tested HVSURFACE sub-
// regions (cursor sets, etc.).
BOOLEAN AddVSurfaceRegion(HVSURFACE hVSurface, VSURFACE_REGION* pNewRegion)
{
	if (!hVSurface || !pNewRegion) return FALSE;
	hVSurface->RegionList.push_back(*pNewRegion);
	return TRUE;
}

BOOLEAN ClearAllVSurfaceRegions(HVSURFACE hVSurface)
{
	if (!hVSurface) return FALSE;
	hVSurface->RegionList.clear();
	return TRUE;
}

BOOLEAN GetVSurfaceRegion(HVSURFACE hVSurface, UINT16 usIndex, VSURFACE_REGION* aRegion)
{
	if (!hVSurface || !aRegion || usIndex >= hVSurface->RegionList.size()) return FALSE;
	*aRegion = hVSurface->RegionList[usIndex];
	return TRUE;
}

BOOLEAN GetNumRegions(HVSURFACE hVSurface, UINT32* puiNumRegions)
{
	if (!hVSurface || !puiNumRegions) return FALSE;
	*puiNumRegions = (UINT32)hVSurface->RegionList.size();
	return TRUE;
}

BOOLEAN ReplaceVSurfaceRegion(HVSURFACE hVSurface, UINT16 usIndex, VSURFACE_REGION* aRegion)
{
	if (!hVSurface || !aRegion || usIndex >= hVSurface->RegionList.size()) return FALSE;
	hVSurface->RegionList[usIndex] = *aRegion;
	return TRUE;
}

BOOLEAN AddVideoSurfaceRegion(UINT32 uiIndex, VSURFACE_REGION* pNewRegion)
{
	HVSURFACE s = nullptr;
	if (!GetVideoSurface(&s, uiIndex)) return FALSE;
	return AddVSurfaceRegion(s, pNewRegion);
}

BOOLEAN MakeVSurfaceFromVObject(UINT32 /*uiVObject*/, UINT16 /*usSubIndex*/, UINT32* /*puiVSurface*/)
{
	// Not exercised in the current path; stub returning failure.
	return FALSE;
}

BOOLEAN PixelateVideoSurfaceRect(UINT32, INT32, INT32, INT32, INT32)
{
	// Stop-gap; the real implementation needs the alpha blitters from
	// Phase 6.
	return FALSE;
}

BOOLEAN SetClipList(HVSURFACE, SGPRect*, UINT16) { return TRUE; }

///////////////////////////////////////////////////////////////////////////////
// Blitters
///////////////////////////////////////////////////////////////////////////////
//
// All blitting on the SDL3 path is CPU-side. The DD-era "UsingDD"
// function name is preserved as an alias so legacy call sites don't
// have to be touched -- it's just a CPU memcpy / color-keyed loop now.

namespace {

// Clip a src-rect blit against the destination surface bounds.
// On entry, SrcRect / DestRect are integer pixel coords. On exit,
// they're adjusted so the copy is fully in-bounds, or returns false
// if there's nothing to copy.
bool ClipBlitRect(HVSURFACE hDst, HVSURFACE hSrc,
                  INT32& iDestX, INT32& iDestY,
                  INT32& iSrcL, INT32& iSrcT, INT32& iSrcR, INT32& iSrcB)
{
	const INT32 dW = (INT32)hDst->usWidth;
	const INT32 dH = (INT32)hDst->usHeight;
	(void)hSrc;
	INT32 w = iSrcR - iSrcL;
	INT32 h = iSrcB - iSrcT;
	if (w <= 0 || h <= 0) return false;
	if (iDestX >= dW || iDestY >= dH) return false;
	if (iDestX + w <= 0 || iDestY + h <= 0) return false;
	if (iDestX + w > dW) { INT32 d = (iDestX + w) - dW; iSrcR -= d; }
	if (iDestY + h > dH) { INT32 d = (iDestY + h) - dH; iSrcB -= d; }
	if (iDestX < 0)      { iSrcL -= iDestX; iDestX = 0; }
	if (iDestY < 0)      { iSrcT -= iDestY; iDestY = 0; }
	return (iSrcR > iSrcL) && (iSrcB > iSrcT);
}

void Blit16_Opaque(UINT16* dst, UINT32 dstPitchBytes,
                   const UINT16* src, UINT32 srcPitchBytes,
                   INT32 dstX, INT32 dstY,
                   INT32 srcX, INT32 srcY,
                   INT32 w, INT32 h)
{
	UINT8* dstRow = (UINT8*)dst + dstY * dstPitchBytes + dstX * 2;
	const UINT8* srcRow = (const UINT8*)src + srcY * srcPitchBytes + srcX * 2;
	for (INT32 y = 0; y < h; ++y)
	{
		std::memcpy(dstRow, srcRow, (size_t)w * 2);
		dstRow += dstPitchBytes;
		srcRow += srcPitchBytes;
	}
}

void Blit16_ColorKey(UINT16* dst, UINT32 dstPitchBytes,
                     const UINT16* src, UINT32 srcPitchBytes,
                     INT32 dstX, INT32 dstY,
                     INT32 srcX, INT32 srcY,
                     INT32 w, INT32 h, UINT16 key)
{
	for (INT32 y = 0; y < h; ++y)
	{
		UINT16* dstP = (UINT16*)((UINT8*)dst + (dstY + y) * dstPitchBytes) + dstX;
		const UINT16* srcP = (const UINT16*)((const UINT8*)src + (srcY + y) * srcPitchBytes) + srcX;
		for (INT32 x = 0; x < w; ++x)
		{
			UINT16 v = srcP[x];
			if (v != key) dstP[x] = v;
		}
	}
}

void Blit8_Opaque(UINT8* dst, UINT32 dstPitchBytes,
                  const UINT8* src, UINT32 srcPitchBytes,
                  INT32 dstX, INT32 dstY,
                  INT32 srcX, INT32 srcY,
                  INT32 w, INT32 h)
{
	UINT8* dstRow = dst + dstY * dstPitchBytes + dstX;
	const UINT8* srcRow = src + srcY * srcPitchBytes + srcX;
	for (INT32 y = 0; y < h; ++y)
	{
		std::memcpy(dstRow, srcRow, (size_t)w);
		dstRow += dstPitchBytes;
		srcRow += srcPitchBytes;
	}
}

void FillRect16(UINT16* dst, UINT32 dstPitchBytes,
                INT32 x, INT32 y, INT32 w, INT32 h, UINT16 colour)
{
	for (INT32 yy = 0; yy < h; ++yy)
	{
		UINT16* row = (UINT16*)((UINT8*)dst + (y + yy) * dstPitchBytes) + x;
		for (INT32 xx = 0; xx < w; ++xx) row[xx] = colour;
	}
}

} // namespace

BOOLEAN ColorFillVideoSurfaceArea(UINT32 uiDestVSurface,
                                  INT32 iDestX1, INT32 iDestY1,
                                  INT32 iDestX2, INT32 iDestY2,
                                  UINT16 Color16BPP)
{
	HVSURFACE hDst = nullptr;
	if (!GetVideoSurface(&hDst, uiDestVSurface) || !hDst) return FALSE;
	if (hDst->ubBitDepth != 16) return FALSE;

	// Normalise + clip.
	if (iDestX2 < iDestX1) std::swap(iDestX1, iDestX2);
	if (iDestY2 < iDestY1) std::swap(iDestY1, iDestY2);
	if (iDestX1 < 0) iDestX1 = 0;
	if (iDestY1 < 0) iDestY1 = 0;
	if (iDestX2 > (INT32)hDst->usWidth)  iDestX2 = hDst->usWidth;
	if (iDestY2 > (INT32)hDst->usHeight) iDestY2 = hDst->usHeight;
	if (iDestX1 >= iDestX2 || iDestY1 >= iDestY2) return TRUE;

	UINT32 dstPitch = 0;
	UINT16* dstBuf = (UINT16*)LockVideoSurfaceBuffer(hDst, &dstPitch);
	if (!dstBuf) return FALSE;
	FillRect16(dstBuf, dstPitch, iDestX1, iDestY1,
	           iDestX2 - iDestX1, iDestY2 - iDestY1, Color16BPP);
	UnLockVideoSurfaceBuffer(hDst);
	return TRUE;
}

BOOLEAN BltVideoSurfaceToVideoSurface(HVSURFACE hDst, HVSURFACE hSrc,
                                      UINT16 usIndex, INT32 iDestX, INT32 iDestY,
                                      INT32 fBltFlags, blt_vs_fx* pBltFx)
{
	if (!hDst || !hSrc) return FALSE;
	if (hDst->ubBitDepth != hSrc->ubBitDepth) return FALSE;

	// Color-fill rect via the dedicated entry point.
	if (fBltFlags & VS_BLT_COLORFILLRECT)
	{
		if (!pBltFx) return FALSE;
		return ColorFillVideoSurfaceArea(
			PRIMARY_SURFACE, // unused -- we go through GetVideoSurface dispatch below
			pBltFx->FillRect.iLeft, pBltFx->FillRect.iTop,
			pBltFx->FillRect.iRight, pBltFx->FillRect.iBottom,
			(UINT16)pBltFx->ColorFill) ? TRUE : FALSE;
	}
	if (fBltFlags & VS_BLT_COLORFILL)
	{
		if (!pBltFx) return FALSE;
		UINT32 pitch = 0;
		UINT16* buf = (UINT16*)LockVideoSurfaceBuffer(hDst, &pitch);
		if (!buf) return FALSE;
		FillRect16(buf, pitch, 0, 0, hDst->usWidth, hDst->usHeight,
		           (UINT16)pBltFx->ColorFill);
		UnLockVideoSurfaceBuffer(hDst);
		return TRUE;
	}

	// Source rect: VS_BLT_SRCREGION (region table index), VS_BLT_SRCSUBRECT
	// (rect in pBltFx->SrcRect), or default (whole source surface).
	INT32 sL, sT, sR, sB;
	if (fBltFlags & VS_BLT_SRCREGION)
	{
		VSURFACE_REGION region{};
		if (!GetVSurfaceRegion(hSrc, usIndex, &region)) return FALSE;
		sL = region.RegionCoords.iLeft;
		sT = region.RegionCoords.iTop;
		sR = region.RegionCoords.iRight;
		sB = region.RegionCoords.iBottom;
	}
	else if (fBltFlags & VS_BLT_SRCSUBRECT)
	{
		if (!pBltFx) return FALSE;
		sL = pBltFx->SrcRect.iLeft;
		sT = pBltFx->SrcRect.iTop;
		sR = pBltFx->SrcRect.iRight;
		sB = pBltFx->SrcRect.iBottom;
	}
	else
	{
		sL = 0; sT = 0; sR = hSrc->usWidth; sB = hSrc->usHeight;
	}

	if (!ClipBlitRect(hDst, hSrc, iDestX, iDestY, sL, sT, sR, sB)) return TRUE;

	UINT32 srcPitch = 0, dstPitch = 0;
	BYTE* srcBuf = LockVideoSurfaceBuffer(hSrc, &srcPitch);
	BYTE* dstBuf = LockVideoSurfaceBuffer(hDst, &dstPitch);
	if (!srcBuf || !dstBuf)
	{
		UnLockVideoSurfaceBuffer(hSrc);
		UnLockVideoSurfaceBuffer(hDst);
		return FALSE;
	}

	const INT32 w = sR - sL;
	const INT32 h = sB - sT;
	if (hDst->ubBitDepth == 16)
	{
		if (fBltFlags & VS_BLT_USECOLORKEY)
		{
			Blit16_ColorKey((UINT16*)dstBuf, dstPitch,
			                (const UINT16*)srcBuf, srcPitch,
			                iDestX, iDestY, sL, sT, w, h,
			                (UINT16)hSrc->TransparentColor);
		}
		else
		{
			Blit16_Opaque((UINT16*)dstBuf, dstPitch,
			              (const UINT16*)srcBuf, srcPitch,
			              iDestX, iDestY, sL, sT, w, h);
		}
	}
	else if (hDst->ubBitDepth == 8)
	{
		Blit8_Opaque((UINT8*)dstBuf, dstPitch,
		             (const UINT8*)srcBuf, srcPitch,
		             iDestX, iDestY, sL, sT, w, h);
	}

	UnLockVideoSurfaceBuffer(hSrc);
	UnLockVideoSurfaceBuffer(hDst);
	return TRUE;
}

BOOLEAN BltVideoSurface(UINT32 uiDest, UINT32 uiSrc, UINT16 usRegionIndex,
                        INT32 iDestX, INT32 iDestY, UINT32 fBltFlags,
                        blt_vs_fx* pBltFx)
{
	HVSURFACE hDst = nullptr, hSrc = nullptr;
	if (!GetVideoSurface(&hDst, uiDest) || !hDst) return FALSE;
	if (!GetVideoSurface(&hSrc, uiSrc)  || !hSrc) return FALSE;
	return BltVideoSurfaceToVideoSurface(hDst, hSrc, usRegionIndex,
	                                     iDestX, iDestY,
	                                     (INT32)fBltFlags, pBltFx);
}

BOOLEAN BltStretchVideoSurface(UINT32 uiDest, UINT32 uiSrc,
                               INT32 /*iDestX*/, INT32 /*iDestY*/,
                               UINT32 /*fBltFlags*/,
                               SGPRect* SrcRect, SGPRect* DestRect)
{
	if (!SrcRect || !DestRect) return FALSE;
	HVSURFACE hDst = nullptr, hSrc = nullptr;
	if (!GetVideoSurface(&hDst, uiDest) || !hDst) return FALSE;
	if (!GetVideoSurface(&hSrc, uiSrc)  || !hSrc) return FALSE;
	if (hDst->ubBitDepth != 16 || hSrc->ubBitDepth != 16) return FALSE;

	const INT32 sW = SrcRect->iRight - SrcRect->iLeft;
	const INT32 sH = SrcRect->iBottom - SrcRect->iTop;
	const INT32 dW = DestRect->iRight - DestRect->iLeft;
	const INT32 dH = DestRect->iBottom - DestRect->iTop;
	if (sW <= 0 || sH <= 0 || dW <= 0 || dH <= 0) return TRUE;

	UINT32 srcPitch = 0, dstPitch = 0;
	UINT16* srcBuf = (UINT16*)LockVideoSurfaceBuffer(hSrc, &srcPitch);
	UINT16* dstBuf = (UINT16*)LockVideoSurfaceBuffer(hDst, &dstPitch);
	if (!srcBuf || !dstBuf)
	{
		UnLockVideoSurfaceBuffer(hSrc);
		UnLockVideoSurfaceBuffer(hDst);
		return FALSE;
	}

	// Nearest-neighbour stretch. Integer ratios; good enough for the
	// UI panels JA2 stretches. Phase 6 / shaders can do better.
	for (INT32 dy = 0; dy < dH; ++dy)
	{
		INT32 absDstY = DestRect->iTop + dy;
		if (absDstY < 0 || absDstY >= (INT32)hDst->usHeight) continue;
		const INT32 sy = SrcRect->iTop + (dy * sH) / dH;
		const UINT16* srcRow = (const UINT16*)((const UINT8*)srcBuf + sy * srcPitch);
		UINT16* dstRow = (UINT16*)((UINT8*)dstBuf + absDstY * dstPitch);
		for (INT32 dx = 0; dx < dW; ++dx)
		{
			INT32 absDstX = DestRect->iLeft + dx;
			if (absDstX < 0 || absDstX >= (INT32)hDst->usWidth) continue;
			const INT32 sx = SrcRect->iLeft + (dx * sW) / dW;
			dstRow[absDstX] = srcRow[sx];
		}
	}

	UnLockVideoSurfaceBuffer(hSrc);
	UnLockVideoSurfaceBuffer(hDst);
	return TRUE;
}

// BltVSurfaceUsingDD: the "UsingDD" name is vestigial -- the SDL3 path
// has no DirectDraw blits, so this is just an alias for the CPU blit.
// RECT* parameter (Win32 type) preserved for ABI; mapped to the same
// fields BltVideoSurfaceToVideoSurface expects.
BOOLEAN BltVSurfaceUsingDD(HVSURFACE hDst, HVSURFACE hSrc, UINT32 fBltFlags,
                           INT32 iDestX, INT32 iDestY, RECT* SrcRect)
{
	blt_vs_fx fx{};
	UINT32 flags = fBltFlags;
	if (SrcRect)
	{
		fx.SrcRect.iLeft   = SrcRect->left;
		fx.SrcRect.iTop    = SrcRect->top;
		fx.SrcRect.iRight  = SrcRect->right;
		fx.SrcRect.iBottom = SrcRect->bottom;
		flags |= VS_BLT_SRCSUBRECT;
	}
	return BltVideoSurfaceToVideoSurface(hDst, hSrc, 0, iDestX, iDestY,
	                                     (INT32)flags, &fx);
}

// Image-tile fill, alpha shadow, and the low-percent-table darken
// operations need the same blender math the inline-asm blocks in
// vobject_blitters.cpp provide. They're not exercised yet in the
// macOS startup path; once a real game loop drives them the real
// implementations come in alongside the Phase 6 RGB565-asm
// retirement.
BOOLEAN ImageFillVideoSurfaceArea(UINT32, INT32, INT32, INT32, INT32,
                                  HVOBJECT, UINT16, INT16, INT16) { return FALSE; }
BOOLEAN ShadowVideoSurfaceRect(UINT32, INT32, INT32, INT32, INT32) { return FALSE; }
BOOLEAN ShadowVideoSurfaceImage(UINT32, HVOBJECT, INT32, INT32) { return FALSE; }
BOOLEAN ShadowVideoSurfaceRectUsingLowPercentTable(UINT32, INT32, INT32, INT32, INT32) { return FALSE; }

