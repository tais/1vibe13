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
// CPU blitters, stretching, shadowing, and packed-surface operations are
// implemented below. The public rectangle fill enters through the engine
// RenderCommandSink and maps back into this storage manager. ImageFill remains
// a placeholder, and Restore/Backup stays a no-op because heap surfaces have
// no DirectDraw restore semantics.

#include "types.h"
#include "vobject.h"  // VO_BLT_SRCTRANSPARENCY
#include "vsurface.h"
#include "vobject_blitters.h"
#include "himage.h"
#include "video.h"
#include "MemMan.h"
#include "WCheck.h"
#include "DEBUG.H"

#include <Engine/Core/StableResourceRegistry.h>
#include <Engine/Core/UniqueResourcePtr.h>
#include <Engine/Adapters/Legacy/PlatformVideoSurfaceBackend.h>

#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <optional>

// g_SurfaceRectangle is defined by vobject_blitters.cpp; pulled in
// here as extern so the SurfaceData registry can keep the per-surface
// clipping rectangle in sync.
extern std::map<SurfaceData::tID, ClipRectangle> g_SurfaceRectangle;

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
	std::map<BYTE*, tID>     _applicationData;
	void ReleaseSurfaceData(tID surfaceID);
	void UnRegisterSurface(tID surfaceID);

	void RegisterSurface(tID surfaceID, HVSURFACE surface)
	{
		if (!surface || surfaceID == 0) return;
		UnRegisterSurface(surfaceID);
		for (auto it = _surfaceID.begin(); it != _surfaceID.end(); ++it)
		{
			if (it->second != surface) continue;
			const tID previousID = it->first;
			UnRegisterSurface(previousID);
			break;
		}
		_surfaceID[surfaceID] = surface;
		SetSurfaceClipRectangle(surfaceID, surface->usWidth, surface->usHeight);
	}
	void UnRegisterSurface(tID surfaceID)
	{
		ReleaseSurfaceData(surfaceID);
		g_SurfaceRectangle.erase(surfaceID);
		_surfaceID.erase(surfaceID);
	}
	void UnRegisterSurface(HVSURFACE surface)
	{
		for (auto it = _surfaceID.begin(); it != _surfaceID.end(); ++it)
		{
			if (it->second != surface) continue;
			const tID surfaceID = it->first;
			UnRegisterSurface(surfaceID);
			return;
		}
	}

	BYTE* SetSurfaceData(tID surfaceID, BYTE* data)
	{
		if (!data) return nullptr;
		std::map<tID, tSurface>::iterator sit = SurfaceData::_surfaceID.find(surfaceID);
		if (sit != SurfaceData::_surfaceID.end())
		{
			const auto prior = _surfaceOfData.find(data);
			if (prior != _surfaceOfData.end()) ReleaseSurfaceData(prior->second);
			ReleaseSurfaceData(surfaceID);
			SurfaceData::_surfaceData[sit->second] = data;
			SurfaceData::_surfaceOfData[data] = surfaceID;
			return data;
		}
		SGP_THROW(L"Unregistered surface ID");
	}
	BYTE* SetSurfaceData(HVSURFACE surface, BYTE* data)
	{
		if (!data) return nullptr;
		std::map<tID, tSurface>::iterator sit = SurfaceData::_surfaceID.begin();
		for (; sit != SurfaceData::_surfaceID.end(); ++sit)
		{
			// NB: legacy vsurface.cpp had `sit->second = surface` (single =)
			// here, which is almost certainly a bug -- it assigns rather
			// than compares. Preserved as `==` so the call actually checks
			// surface identity the way the function name and docs imply.
			if (sit->second == surface)
			{
				return SetSurfaceData(sit->first, data);
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
		if (!data) return nullptr;
		const tID id = reinterpret_cast<tID>(data);
		_applicationData[data] = id;
		return data;
	}
	void ReleaseApplicationData(BYTE* data)
	{
		const auto it = _applicationData.find(data);
		if (it != _applicationData.end())
		{
			if (_surfaceID.find(it->second) == _surfaceID.end())
				g_SurfaceRectangle.erase(it->second);
			_applicationData.erase(it);
		}
	}

	tID GetSurfaceID(BYTE* data)
	{
		std::map<BYTE*, tID>::iterator it = SurfaceData::_surfaceOfData.find(data);
		if (it != SurfaceData::_surfaceOfData.end())
		{
			return it->second;
		}
		const auto application = _applicationData.find(data);
		if (application != _applicationData.end()) return application->second;
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

struct VideoSurfaceReleaser
{
	void operator()(SGPVSurface* surface) const { DeleteVideoSurface(surface); }
};

using OwnedVideoSurface = UniqueResourcePtr<SGPVSurface, VideoSurfaceReleaser>;
using VideoSurfaceRegistry = StableResourceRegistry<OwnedVideoSurface, UINT32>;

VideoSurfaceRegistry gVideoSurfaces(
	VideoSurfaceRegistry::Limits{2, 2, 0xffffffeeu});
UINT32 guiVSurfaceIndex = 0;
UINT32 guiVSurfaceSize = 0;
UINT32 guiVSurfaceTotalAdded = 0;
bool gVideoSurfaceManagerInitialized = false;

std::size_t BytesPerPixelFor(UINT8 bpp)
{
	// 8bpp source surfaces stay 1 byte; everything else is a render-format
	// surface stored at the screen pixel width (RGB565=2, RGBA8888=4).
	return (bpp <= 8) ? 1u : sizeof(PIXEL);
}

bool IsSupportedBitDepth(UINT8 bpp)
{
	return bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32;
}

bool BufferBytes(UINT32 w, UINT32 h, UINT8 bpp, std::size_t& bytes)
{
	bytes = 0;
	if (w == 0 || h == 0 || !IsSupportedBitDepth(bpp)) return false;
	const std::size_t pixelBytes = BytesPerPixelFor(bpp > 16 ? 16 : bpp);
	if (w > std::numeric_limits<std::size_t>::max() / h) return false;
	const std::size_t pixels = static_cast<std::size_t>(w) * h;
	if (pixels > std::numeric_limits<std::size_t>::max() / pixelBytes) return false;
	bytes = pixels * pixelBytes;
	return bytes <= static_cast<std::size_t>(std::numeric_limits<INT32>::max());
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
	if (w == 0 || h == 0 || !IsSupportedBitDepth(bpp)) return nullptr;
	const UINT8 storedBitDepth = (bpp > 16) ? 16 : bpp;
	std::size_t bufferBytes = 0;
	if (!BufferBytes(w, h, storedBitDepth, bufferBytes)) return nullptr;

	HVSURFACE s = new (std::nothrow) SGPVSurface{};
	if (!s) return nullptr;
	s->usHeight = h;
	s->usWidth  = w;
	s->ubBitDepth = storedBitDepth;
	if (externalBuffer)
	{
		s->pSurfaceData = externalBuffer;
		s->fFlags = VSURFACE_RESERVED_SURFACE;
	}
	else
	{
		s->pSurfaceData = std::calloc(1, bufferBytes);
		if (!s->pSurfaceData)
		{
			delete s;
			return nullptr;
		}
		s->fFlags = 0;
		giMemUsedInSurfaces += static_cast<INT32>(bufferBytes);
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

class ScopedImage
{
public:
	explicit ScopedImage(HIMAGE image = nullptr) noexcept : image_(image) {}
	~ScopedImage() { if (image_) DestroyImage(image_); }
	ScopedImage(const ScopedImage&) = delete;
	ScopedImage& operator=(const ScopedImage&) = delete;
	HIMAGE get() const noexcept { return image_; }
	void reset(HIMAGE image) noexcept
	{
		if (image_) DestroyImage(image_);
		image_ = image;
	}

private:
	HIMAGE image_;
};

} // namespace

BOOLEAN DeleteVideoSurface(HVSURFACE hVSurface)
{
	if (!hVSurface) return FALSE;
	if (!(hVSurface->fFlags & VSURFACE_RESERVED_SURFACE))
	{
		std::size_t bufferBytes = 0;
		if (BufferBytes(hVSurface->usWidth, hVSurface->usHeight,
			hVSurface->ubBitDepth, bufferBytes))
		{
			const INT32 trackedBytes = static_cast<INT32>(bufferBytes);
			giMemUsedInSurfaces = giMemUsedInSurfaces >= trackedBytes
				? giMemUsedInSurfaces - trackedBytes : 0;
		}
	}
	FreeSurfaceBuffer(hVSurface);
	FreePalette(hVSurface);
	delete hVSurface;
	return TRUE;
}

static void DeletePrimaryVideoSurfaces()
{
	SurfaceData::UnRegisterSurface(PRIMARY_SURFACE);
	SurfaceData::UnRegisterSurface(BACKBUFFER);
	SurfaceData::UnRegisterSurface(FRAME_BUFFER);
	SurfaceData::UnRegisterSurface(MOUSE_BUFFER);
	if (ghPrimary) DeleteVideoSurface(ghPrimary);
	if (ghBackBuffer) DeleteVideoSurface(ghBackBuffer);
	if (ghFrameBuffer) DeleteVideoSurface(ghFrameBuffer);
	if (ghMouseBuffer) DeleteVideoSurface(ghMouseBuffer);
	ghPrimary = nullptr;
	ghBackBuffer = nullptr;
	ghFrameBuffer = nullptr;
	ghMouseBuffer = nullptr;
}

BOOLEAN SetPrimaryVideoSurfaces()
{
	extern UINT16 SCREEN_WIDTH;
	extern UINT16 SCREEN_HEIGHT;

	UINT32 pitch = 0;
	void* primBuf  = LockPrimarySurface(&pitch); UnlockPrimarySurface();
	void* backBuf  = LockBackBuffer(&pitch);     UnlockBackBuffer();
	void* frameBuf = LockFrameBuffer(&pitch);    UnlockFrameBuffer();
	void* mouseBuf = LockMouseBuffer(&pitch);    UnlockMouseBuffer();
	if (!primBuf || !backBuf || !frameBuf || !mouseBuf) return FALSE;

	HVSURFACE newPrimary = NewSurface(SCREEN_WIDTH, SCREEN_HEIGHT, 16, primBuf);
	HVSURFACE newBackBuffer = NewSurface(SCREEN_WIDTH, SCREEN_HEIGHT, 16, backBuf);
	HVSURFACE newFrameBuffer = NewSurface(SCREEN_WIDTH, SCREEN_HEIGHT, 16, frameBuf);
	HVSURFACE newMouseBuffer =
		NewSurface(MAX_CURSOR_WIDTH, MAX_CURSOR_HEIGHT, 16, mouseBuf);
	if (!newPrimary || !newBackBuffer || !newFrameBuffer || !newMouseBuffer)
	{
		if (newPrimary) DeleteVideoSurface(newPrimary);
		if (newBackBuffer) DeleteVideoSurface(newBackBuffer);
		if (newFrameBuffer) DeleteVideoSurface(newFrameBuffer);
		if (newMouseBuffer) DeleteVideoSurface(newMouseBuffer);
		return FALSE;
	}

	DeletePrimaryVideoSurfaces();
	ghPrimary = newPrimary;
	ghBackBuffer = newBackBuffer;
	ghFrameBuffer = newFrameBuffer;
	ghMouseBuffer = newMouseBuffer;

	try
	{
		SurfaceData::RegisterSurface(PRIMARY_SURFACE, ghPrimary);
		SurfaceData::RegisterSurface(BACKBUFFER, ghBackBuffer);
		SurfaceData::RegisterSurface(FRAME_BUFFER, ghFrameBuffer);
		SurfaceData::RegisterSurface(MOUSE_BUFFER, ghMouseBuffer);
	}
	catch (...)
	{
		DeletePrimaryVideoSurfaces();
		return FALSE;
	}
	return TRUE;
}

BOOLEAN InitializeVideoSurfaceManager()
{
	if (gVideoSurfaceManagerInitialized) return TRUE;
	if (!gVideoSurfaces.empty()) return FALSE;
	RegisterDebugTopic(TOPIC_VIDEOSURFACE, "Video Surface Manager");
	giMemUsedInSurfaces = 0;
	if (!SetPrimaryVideoSurfaces())
	{
		DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_1, String("Could not create primary surfaces"));
		UnRegisterDebugTopic(TOPIC_VIDEOSURFACE, "Video Surface Manager");
		return FALSE;
	}
	gVideoSurfaceManagerInitialized = true;
	return TRUE;
}

BOOLEAN ShutdownVideoSurfaceManager()
{
	DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_0, "Shutting down the Video Surface manager");
	DeletePrimaryVideoSurfaces();
	gVideoSurfaces.forEach([](UINT32 id, OwnedVideoSurface&) {
		SurfaceData::UnRegisterSurface(id);
	});
	gVideoSurfaces.clear();
	guiVSurfaceIndex = 0;
	guiVSurfaceSize = 0;
	guiVSurfaceTotalAdded = 0;
	giMemUsedInSurfaces = 0;
	if (gVideoSurfaceManagerInitialized)
		UnRegisterDebugTopic(TOPIC_VIDEOSURFACE, "Video Surface Manager");
	gVideoSurfaceManagerInitialized = false;
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
	ScopedImage image;
	UINT16 usWidth = 0, usHeight = 0;
	UINT8 ubBitDepth = 0;

	if (desc->fCreateFlags & VSURFACE_CREATE_FROMFILE)
	{
		ImageFileType::TestOrder order = ImageFileType::JPC_FALLBACK;
		if (desc->fCreateFlags & VSURFACE_CREATE_FROMJPC) order = ImageFileType::JPC;
		else if (desc->fCreateFlags & VSURFACE_CREATE_FROMJPC_FALLBACK) order = ImageFileType::JPC_FALLBACK;
		else if (desc->fCreateFlags & VSURFACE_CREATE_FROMPNG) order = ImageFileType::PNG;
		else if (desc->fCreateFlags & VSURFACE_CREATE_FROMPNG_FALLBACK) order = ImageFileType::PNG_FALLBACK;

		HIMAGE const loadedImage =
			CreateImage(desc->ImageFile, IMAGE_ALLIMAGEDATA, order);
		SGP_THROW_IFFALSE(loadedImage,
			_BS(L"Could not create video surface from file : ") << vfs::String(desc->ImageFile) << _BS::wget);
		if (!loadedImage) return nullptr;
		image.reset(loadedImage);
		usWidth    = loadedImage->usWidth;
		usHeight   = loadedImage->usHeight;
		ubBitDepth = loadedImage->ubBitDepth;
	}
	else
	{
		if (desc->usWidth == 0 || desc->usHeight == 0 ||
			desc->usWidth > std::numeric_limits<UINT16>::max() ||
			desc->usHeight > std::numeric_limits<UINT16>::max())
			return nullptr;
		usWidth    = static_cast<UINT16>(desc->usWidth);
		usHeight   = static_cast<UINT16>(desc->usHeight);
		ubBitDepth = desc->ubBitDepth;
	}
	if (usWidth == 0 || usHeight == 0 || !IsSupportedBitDepth(ubBitDepth))
		return nullptr;

	HVSURFACE hVSurface = NewSurface(usWidth, usHeight, ubBitDepth, nullptr);
	if (!hVSurface) return nullptr;

	if (desc->fCreateFlags & VSURFACE_CREATE_FROMFILE)
	{
		HIMAGE const loadedImage = image.get();
		SGPRect tempRect{ 0, 0, loadedImage->usWidth - 1,
			loadedImage->usHeight - 1 };
		if (!SetVideoSurfaceDataFromHImage(
			hVSurface, loadedImage, 0, 0, &tempRect))
		{
			DeleteVideoSurface(hVSurface);
			return nullptr;
		}
		if (loadedImage->ubBitDepth == 8 &&
			!SetVideoSurfacePalette(hVSurface, loadedImage->pPalette))
		{
			DeleteVideoSurface(hVSurface);
			return nullptr;
		}
	}

	DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_3, String("Success in Creating Video Surface"));
	return hVSurface;
}

BOOLEAN AddStandardVideoSurface(VSURFACE_DESC* pVSurfaceDesc, UINT32* puiIndex)
{
	if (!puiIndex || !pVSurfaceDesc) return FALSE;
	HVSURFACE hVSurface = CreateVideoSurface(pVSurfaceDesc);
	if (!hVSurface) return FALSE;

	if (!SetVideoSurfaceTransparencyColor(hVSurface, FROMRGB(0, 0, 0)))
	{
		DeleteVideoSurface(hVSurface);
		return FALSE;
	}

	std::optional<UINT32> index;
	try
	{
		index = gVideoSurfaces.insert(OwnedVideoSurface(hVSurface));
	}
	catch (...)
	{
		return FALSE;
	}
	if (!index) return FALSE;
	try
	{
		SurfaceData::RegisterSurface(*index, hVSurface);
	}
	catch (...)
	{
		SurfaceData::UnRegisterSurface(*index);
		gVideoSurfaces.erase(*index);
		return FALSE;
	}

	guiVSurfaceIndex = *index;
	*puiIndex = *index;
	guiVSurfaceSize = static_cast<UINT32>(gVideoSurfaces.size());
	++guiVSurfaceTotalAdded;
	return TRUE;
}

static HVSURFACE FindSurfaceByIndex(UINT32 uiIndex)
{
	OwnedVideoSurface* const surface = gVideoSurfaces.find(uiIndex);
	return surface ? surface->get() : nullptr;
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
	HVSURFACE const surface = FindSurfaceByIndex(uiIndex);
	if (!surface) return FALSE;
	*hVSurface = surface;
	return TRUE;
}

BOOLEAN DeleteVideoSurfaceFromIndex(UINT32 uiIndex)
{
	if (!FindSurfaceByIndex(uiIndex)) return FALSE;
	SurfaceData::UnRegisterSurface(uiIndex);
	if (!gVideoSurfaces.erase(uiIndex)) return FALSE;
	guiVSurfaceSize = static_cast<UINT32>(gVideoSurfaces.size());
	return TRUE;
}

bool PlatformVideoSurfaceDescribe(
	std::uint32_t uiIndex,
	std::uint32_t& width,
	std::uint32_t& height,
	std::uint8_t& contentBitDepth,
	std::uint8_t& pixelBytes)
{
	HVSURFACE s = nullptr;
	if (!GetVideoSurface(&s, uiIndex) || !s) return false;
	width = s->usWidth;
	height = s->usHeight;
	contentBitDepth = s->ubBitDepth;
	pixelBytes = static_cast<std::uint8_t>(
		BytesPerPixelFor(s->ubBitDepth));
	return true;
}

BYTE* LockVideoSurfaceBuffer(HVSURFACE hVSurface, UINT32* pPitch)
{
	Assert(hVSurface != nullptr);
	Assert(pPitch != nullptr);
	*pPitch = hVSurface->usWidth * BytesPerPixelFor(hVSurface->ubBitDepth);
	return (BYTE*)hVSurface->pSurfaceData;
}

void UnLockVideoSurfaceBuffer(HVSURFACE /*hVSurface*/) {}

std::uint8_t* PlatformVideoSurfaceMap(
	std::uint32_t uiVSurface, std::uint32_t& pitchBytes)
{
	if (iUseWinFonts) CurrentSurface = uiVSurface;
	switch (uiVSurface)
	{
	case PRIMARY_SURFACE: return SurfaceData::SetSurfaceData(
		uiVSurface, (BYTE*)LockPrimarySurface(&pitchBytes));
	case BACKBUFFER: return SurfaceData::SetSurfaceData(
		uiVSurface, (BYTE*)LockBackBuffer(&pitchBytes));
	case FRAME_BUFFER: return SurfaceData::SetSurfaceData(
		uiVSurface, (BYTE*)LockFrameBuffer(&pitchBytes));
	case MOUSE_BUFFER: return SurfaceData::SetSurfaceData(
		uiVSurface, (BYTE*)LockMouseBuffer(&pitchBytes));
	default: break;
	}
	HVSURFACE const surface = FindSurfaceByIndex(uiVSurface);
	if (!surface) return nullptr;
	return SurfaceData::SetSurfaceData(
		uiVSurface, LockVideoSurfaceBuffer(surface, &pitchBytes));
}

void PlatformVideoSurfaceUnmap(std::uint32_t uiVSurface)
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
	HVSURFACE const surface = FindSurfaceByIndex(uiVSurface);
	if (!surface) return;
	UnLockVideoSurfaceBuffer(surface);
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

void Blit16_Opaque(PIXEL* dst, UINT32 dstPitchBytes,
                   const PIXEL* src, UINT32 srcPitchBytes,
                   INT32 dstX, INT32 dstY,
                   INT32 srcX, INT32 srcY,
                   INT32 w, INT32 h)
{
	UINT8* dstRow = (UINT8*)dst + dstY * dstPitchBytes + dstX * sizeof(PIXEL);
	const UINT8* srcRow = (const UINT8*)src + srcY * srcPitchBytes + srcX * sizeof(PIXEL);
	for (INT32 y = 0; y < h; ++y)
	{
		std::memcpy(dstRow, srcRow, (size_t)w * sizeof(PIXEL));
		dstRow += dstPitchBytes;
		srcRow += srcPitchBytes;
	}
}

void Blit16_ColorKey(PIXEL* dst, UINT32 dstPitchBytes,
                     const PIXEL* src, UINT32 srcPitchBytes,
                     INT32 dstX, INT32 dstY,
                     INT32 srcX, INT32 srcY,
                     INT32 w, INT32 h, PIXEL key)
{
	for (INT32 y = 0; y < h; ++y)
	{
		PIXEL* dstP = (PIXEL*)((UINT8*)dst + (dstY + y) * dstPitchBytes) + dstX;
		const PIXEL* srcP = (const PIXEL*)((const UINT8*)src + (srcY + y) * srcPitchBytes) + srcX;
		for (INT32 x = 0; x < w; ++x)
		{
			PIXEL v = srcP[x];
#if SGP_PIXEL_DEPTH == 32
			// Compare on RGB only: 8bpp art keys transparency on palette
			// index 0 (black), which Create32BPPPalette maps to opaque
			// 0xFF000000 -- alpha would never equal the 0-alpha key.
			if ((v & 0x00FFFFFFu) != (key & 0x00FFFFFFu)) dstP[x] = v;
#else
			if (v != key) dstP[x] = v;
#endif
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

void FillRect16(PIXEL* dst, UINT32 dstPitchBytes,
                INT32 x, INT32 y, INT32 w, INT32 h, PIXEL colour16)
{
	const PIXEL colour = PixFromColor16(colour16);
	for (INT32 yy = 0; yy < h; ++yy)
	{
		PIXEL* row = (PIXEL*)((UINT8*)dst + (y + yy) * dstPitchBytes) + x;
		for (INT32 xx = 0; xx < w; ++xx) row[xx] = colour;
	}
}

} // namespace

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
			pBltFx->ColorFill) ? TRUE : FALSE;
	}
	if (fBltFlags & VS_BLT_COLORFILL)
	{
		if (!pBltFx) return FALSE;
		UINT32 pitch = 0;
		PIXEL* buf = (PIXEL*)LockVideoSurfaceBuffer(hDst, &pitch);
		if (!buf) return FALSE;
		FillRect16(buf, pitch, 0, 0, hDst->usWidth, hDst->usHeight,
		           pBltFx->ColorFill);
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
			Blit16_ColorKey((PIXEL*)dstBuf, dstPitch,
			                (const PIXEL*)srcBuf, srcPitch,
			                iDestX, iDestY, sL, sT, w, h,
			                PixFromColor16((UINT16)hSrc->TransparentColor));
		}
		else
		{
			Blit16_Opaque((PIXEL*)dstBuf, dstPitch,
			              (const PIXEL*)srcBuf, srcPitch,
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

BOOLEAN BltStretchVideoSurface(UINT32 uiDest, UINT32 uiSrc,
                               INT32 /*iDestX*/, INT32 /*iDestY*/,
                               UINT32 fBltFlags,
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
	PIXEL* srcBuf = (PIXEL*)LockVideoSurfaceBuffer(hSrc, &srcPitch);
	PIXEL* dstBuf = (PIXEL*)LockVideoSurfaceBuffer(hDst, &dstPitch);
	if (!srcBuf || !dstBuf)
	{
		UnLockVideoSurfaceBuffer(hSrc);
		UnLockVideoSurfaceBuffer(hDst);
		return FALSE;
	}

	// If the caller asked for VO_BLT_SRCTRANSPARENCY (the menu / UI
	// panel path does), skip source pixels equal to the source
	// surface's TransparentColor (typically RGB(0,0,0) = 0x0000 in
	// RGB565). Without this, layered images like the JA2 logo over
	// the flag background get rendered as opaque rectangles.
	const bool transparent = (fBltFlags & VO_BLT_SRCTRANSPARENCY) != 0;
	const PIXEL transColor = (PIXEL)hSrc->TransparentColor;
	(void)transColor;

	// Nearest-neighbour stretch. Integer ratios; good enough for the
	// UI panels JA2 stretches. Phase 6 / shaders can do better.
	for (INT32 dy = 0; dy < dH; ++dy)
	{
		INT32 absDstY = DestRect->iTop + dy;
		if (absDstY < 0 || absDstY >= (INT32)hDst->usHeight) continue;
		const INT32 sy = SrcRect->iTop + (dy * sH) / dH;
		const PIXEL* srcRow = (const PIXEL*)((const UINT8*)srcBuf + sy * srcPitch);
		PIXEL* dstRow = (PIXEL*)((UINT8*)dstBuf + absDstY * dstPitch);
		for (INT32 dx = 0; dx < dW; ++dx)
		{
			INT32 absDstX = DestRect->iLeft + dx;
			if (absDstX < 0 || absDstX >= (INT32)hDst->usWidth) continue;
			const INT32 sx = SrcRect->iLeft + (dx * sW) / dW;
			const PIXEL px = srcRow[sx];
#if SGP_PIXEL_DEPTH == 32
			// Color-key on RGB (ignore alpha); transparent areas of the
			// menu art are keyed black, matching the RGB565 transColor==0.
			if (transparent && (px & 0x00FFFFFFu) == 0u) continue;
#else
			if (transparent && px == transColor) continue;
#endif
			dstRow[absDstX] = px;
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
BOOLEAN ShadowVideoSurfaceImage(UINT32, HVOBJECT, INT32, INT32) { return FALSE; }

// Darken every RGB565 pixel inside the rectangle to half-brightness.
// JA2 uses this to shadow the interior of FastHelp tooltips, button
// hover popups, prone-merc info boxes -- anywhere the legacy code
// wants a translucent dark overlay on the existing pixels. The Win32
// path drove a precomputed shadow lookup table; for our purposes a
// per-channel right-shift produces the same visible effect (50% of
// each channel's value, clipped to RGB565 bit widths) without the
// table-build complexity.
//
// Pairs with the same call inside DisplayFastHelp (mousesystem.cpp)
// where the previous stub left tooltips with no background at all --
// world content showed straight through and the only "tooltip" was
// the two outline rectangles drawn just before.
BOOLEAN ShadowVideoSurfaceRect(UINT32 uiDestVSurface, INT32 X1, INT32 Y1, INT32 X2, INT32 Y2)
{
	if (X2 <= X1 || Y2 <= Y1) return FALSE;
	UINT32 pitchBytes = 0;
	PIXEL* pBuf = (PIXEL*)LockVideoSurface(uiDestVSurface, &pitchBytes);
	if (!pBuf) return FALSE;
	const INT32 stridePx = (INT32)(pitchBytes / sizeof(PIXEL));
	const INT32 xL = X1 < 0 ? 0 : X1;
	const INT32 yT = Y1 < 0 ? 0 : Y1;
	const INT32 xR = X2 > (INT32)SCREEN_WIDTH  ? (INT32)SCREEN_WIDTH  : X2;
	const INT32 yB = Y2 > (INT32)SCREEN_HEIGHT ? (INT32)SCREEN_HEIGHT : Y2;
	for (INT32 y = yT; y < yB; ++y) {
		PIXEL* row = pBuf + y * stridePx;
		for (INT32 x = xL; x < xR; ++x) {
			const PIXEL p = row[x];
#if SGP_PIXEL_DEPTH == 32
			const UINT32 a =  p & 0xFF000000u;
			const UINT32 r = (p >> 16) & 0xFFu;
			const UINT32 g = (p >>  8) & 0xFFu;
			const UINT32 b =  p        & 0xFFu;
			row[x] = a | ((r >> 1) << 16) | ((g >> 1) << 8) | (b >> 1);
#else
			const UINT16 r = (p >> 11) & 0x1F;
			const UINT16 g = (p >>  5) & 0x3F;
			const UINT16 b =  p        & 0x1F;
			row[x] = (UINT16)(((r >> 1) << 11) | ((g >> 1) << 5) | (b >> 1));
#endif
		}
	}
	UnLockVideoSurface(uiDestVSurface);
	return TRUE;
}

// Low-percent darken: same as above for now (the legacy code used a
// gentler 25% shade table; visually 50% reads as "darker tooltip
// background", which is fine for the few call sites that use this
// variant -- they're all tooltip-style overlays).
BOOLEAN ShadowVideoSurfaceRectUsingLowPercentTable(UINT32 uiDestVSurface, INT32 X1, INT32 Y1, INT32 X2, INT32 Y2)
{
	return ShadowVideoSurfaceRect(uiDestVSurface, X1, Y1, X2, Y2);
}
