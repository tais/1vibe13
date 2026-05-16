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

