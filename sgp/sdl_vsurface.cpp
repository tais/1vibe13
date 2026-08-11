// SDL3-backed video surface manager. Phase 5 second slice.
//
// Ports the SGPVSurface plumbing out of the DirectDraw-driven
// vsurface.cpp: surface manager (linked list keyed by index), surface
// creation (empty + from-file via HIMAGE), lock/unlock returning the
// heap pixel buffer, transparency colour, palette, primary surface
// wrappers, and the SurfaceData + ClipRectangle helpers.
//
// Each SGPVSurface owns a heap byte buffer (native PIXEL for renderable
// surfaces, UINT8 for indexed sources) instead of a DirectDraw surface. The
// pSurfaceData field now holds that allocation directly.
//
// Packed pointer blitters and legacy image tiling are implemented below.
// Numeric fills, copies, stretching, and shading enter through the engine
// RenderCommandSink and map back into this storage manager. Restore/Backup
// stays a no-op because heap surfaces have no DirectDraw restore semantics.

#include "types.h"
#include "vobject.h"  // VO_BLT_SRCTRANSPARENCY
#include "vsurface.h"
#include "vobject_blitters.h"
#include "himage.h"
#include "video.h"
#include "MemMan.h"
#include "render_palette_registry.h"
#include "WCheck.h"
#include "DEBUG.H"

#include <Engine/Core/StableResourceRegistry.h>
#include <Engine/Core/UniqueResourcePtr.h>
#include <Engine/Adapters/Legacy/PlatformVideoSurfaceBackend.h>

#include <algorithm>
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

// iUseWinFonts is a JA2 flag controlling whether portable scalable text is
// composed onto the locked surface. Declared in Ja2/local.h.
extern int iUseWinFonts;

// CurrentSurface is retained as a legacy "last mapped surface" compatibility
// value. Portable font buffer calls resolve their exact surface through
// SurfaceData instead of relying on this global.
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

struct PlatformVideoSurfaceMapping
{
	std::uint8_t* pixels = nullptr;
	std::uint32_t pitchBytes = 0;
	std::size_t count = 0;
};

std::map<std::uint32_t, PlatformVideoSurfaceMapping>
	gPlatformVideoSurfaceMappings;

std::size_t BytesPerPixelFor(UINT8 bpp)
{
	// 8bpp source surfaces stay one byte; renderable surfaces use the active
	// native PIXEL width (four bytes in the shipped ARGB8888 runtime).
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
	if (s->p16BPPPalette)
	{
		UnregisterLegacyRenderPalette(s->p16BPPPalette);
		MemFree(s->p16BPPPalette);
		s->p16BPPPalette = nullptr;
	}
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

	// Replacing a wrapper while its pixels are mapped would silently invalidate
	// the caller's surface identity. The depth-buffer adapter already enforces
	// this lifetime rule; colour surfaces follow the same contract.
	if (!gPlatformVideoSurfaceMappings.empty()) return FALSE;

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
	if (!gPlatformVideoSurfaceMappings.empty()) return FALSE;
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

BOOLEAN RestoreVideoSurface(HVSURFACE hVSurface)
{
	// Heap-backed surfaces cannot be "lost" like DirectDraw video memory, so
	// restoration is validation rather than a copy from a shadow allocation.
	return hVSurface && hVSurface->pSurfaceData &&
		hVSurface->usWidth != 0 && hVSurface->usHeight != 0 &&
		IsSupportedBitDepth(hVSurface->ubBitDepth);
}

BOOLEAN RestoreVideoSurfaces()
{
	if (!gVideoSurfaceManagerInitialized ||
		!RestoreVideoSurface(ghPrimary) ||
		!RestoreVideoSurface(ghBackBuffer) ||
		!RestoreVideoSurface(ghFrameBuffer) ||
		!RestoreVideoSurface(ghMouseBuffer))
		return FALSE;
	bool valid = true;
	gVideoSurfaces.forEach(
		[&](UINT32, OwnedVideoSurface& surface)
		{
			if (!RestoreVideoSurface(surface.get())) valid = false;
		});
	return valid ? TRUE : FALSE;
}

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
	if (!hVSurface) return FALSE;
	*hVSurface = nullptr;
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
	if (!FindSurfaceByIndex(uiIndex) ||
		gPlatformVideoSurfaceMappings.find(uiIndex) !=
			gPlatformVideoSurfaceMappings.end())
		return FALSE;
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
	if (!hVSurface || !pPitch || !hVSurface->pSurfaceData)
		return nullptr;
	*pPitch = hVSurface->usWidth * BytesPerPixelFor(hVSurface->ubBitDepth);
	return (BYTE*)hVSurface->pSurfaceData;
}

void UnLockVideoSurfaceBuffer(HVSURFACE /*hVSurface*/) {}

static void ReleasePlatformVideoSurfacePixels(
	std::uint32_t uiVSurface, HVSURFACE surface)
{
	switch (uiVSurface)
	{
	case PRIMARY_SURFACE: UnlockPrimarySurface(); return;
	case BACKBUFFER: UnlockBackBuffer(); return;
	case FRAME_BUFFER: UnlockFrameBuffer(); return;
	case MOUSE_BUFFER: UnlockMouseBuffer(); return;
	default: break;
	}
	if (surface) UnLockVideoSurfaceBuffer(surface);
}

std::uint8_t* PlatformVideoSurfaceMap(
	std::uint32_t uiVSurface, std::uint32_t& pitchBytes)
{
	if (iUseWinFonts) CurrentSurface = uiVSurface;
	auto active = gPlatformVideoSurfaceMappings.find(uiVSurface);
	if (active != gPlatformVideoSurfaceMappings.end())
	{
		if (active->second.count ==
			std::numeric_limits<std::size_t>::max())
			return nullptr;
		++active->second.count;
		pitchBytes = active->second.pitchBytes;
		return active->second.pixels;
	}

	HVSURFACE surface = nullptr;
	std::uint8_t* pixels = nullptr;
	try
	{
		switch (uiVSurface)
		{
		case PRIMARY_SURFACE:
			pixels = static_cast<std::uint8_t*>(
				LockPrimarySurface(&pitchBytes));
			break;
		case BACKBUFFER:
			pixels = static_cast<std::uint8_t*>(
				LockBackBuffer(&pitchBytes));
			break;
		case FRAME_BUFFER:
			pixels = static_cast<std::uint8_t*>(
				LockFrameBuffer(&pitchBytes));
			break;
		case MOUSE_BUFFER:
			pixels = static_cast<std::uint8_t*>(
				LockMouseBuffer(&pitchBytes));
			break;
		default:
			surface = FindSurfaceByIndex(uiVSurface);
			if (surface)
				pixels = LockVideoSurfaceBuffer(surface, &pitchBytes);
			break;
		}
		if (!pixels || pitchBytes == 0)
		{
			ReleasePlatformVideoSurfacePixels(uiVSurface, surface);
			return nullptr;
		}
		pixels = SurfaceData::SetSurfaceData(uiVSurface, pixels);
		if (!pixels)
		{
			ReleasePlatformVideoSurfacePixels(uiVSurface, surface);
			return nullptr;
		}
		const auto inserted = gPlatformVideoSurfaceMappings.emplace(
			uiVSurface,
			PlatformVideoSurfaceMapping{pixels, pitchBytes, 1});
		if (!inserted.second)
		{
			SurfaceData::ReleaseSurfaceData(uiVSurface);
			ReleasePlatformVideoSurfacePixels(uiVSurface, surface);
			return nullptr;
		}
		return pixels;
	}
	catch (...)
	{
		SurfaceData::ReleaseSurfaceData(uiVSurface);
		ReleasePlatformVideoSurfacePixels(uiVSurface, surface);
		return nullptr;
	}
}

void PlatformVideoSurfaceUnmap(std::uint32_t uiVSurface)
{
	const auto active = gPlatformVideoSurfaceMappings.find(uiVSurface);
	if (active == gPlatformVideoSurfaceMappings.end() ||
		active->second.count == 0)
		return;
	if (--active->second.count != 0) return;
	gPlatformVideoSurfaceMappings.erase(active);
	SurfaceData::ReleaseSurfaceData(uiVSurface);
	HVSURFACE const surface = FindSurfaceByIndex(uiVSurface);
	ReleasePlatformVideoSurfacePixels(uiVSurface, surface);
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
	PIXEL* replacement = nullptr;
	try
	{
		replacement = Create16BPPPalette(pSrcPalette);
	}
	catch (...)
	{
		return FALSE;
	}
	if (!replacement) return FALSE;

	SGPPaletteEntry* palette =
		static_cast<SGPPaletteEntry*>(hVSurface->pPalette);
	if (!hVSurface->pPalette)
	{
		palette = static_cast<SGPPaletteEntry*>(
			std::malloc(sizeof(SGPPaletteEntry) * 256));
		if (!palette)
		{
			UnregisterLegacyRenderPalette(replacement);
			MemFree(replacement);
			return FALSE;
		}
	}
	std::memcpy(
		palette, pSrcPalette, sizeof(SGPPaletteEntry) * 256);

	PIXEL* const previous = hVSurface->p16BPPPalette;
	hVSurface->pPalette = palette;
	hVSurface->p16BPPPalette = replacement;
	if (previous)
	{
		UnregisterLegacyRenderPalette(previous);
		MemFree(previous);
	}
	return TRUE;
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
	if (!hVSurface || !hImage || !pSrcRect) return FALSE;

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
	if (fBufferBPP == 0) return FALSE;

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
	try
	{
		hVSurface->RegionList.push_back(*pNewRegion);
		return TRUE;
	}
	catch (...)
	{
		return FALSE;
	}
}

BOOLEAN AddVSurfaceRegionAtIndex(
	HVSURFACE hVSurface, UINT16 usIndex, VSURFACE_REGION* pNewRegion)
{
	if (!hVSurface || !pNewRegion ||
		usIndex >= hVSurface->RegionList.size())
		return FALSE;
	try
	{
		hVSurface->RegionList.insert(
			hVSurface->RegionList.begin() + usIndex, *pNewRegion);
		return TRUE;
	}
	catch (...)
	{
		return FALSE;
	}
}

BOOLEAN AddVSurfaceRegions(
	HVSURFACE hVSurface, VSURFACE_REGION** ppNewRegions,
	UINT16 uiNumRegions)
{
	if (!hVSurface) return FALSE;
	if (uiNumRegions == 0) return TRUE;
	if (!ppNewRegions) return FALSE;
	for (UINT16 index = 0; index < uiNumRegions; ++index)
	{
		if (!ppNewRegions[index]) return FALSE;
	}

	// Publish the group as one transaction. A failed allocation or malformed
	// later pointer must not leave an arbitrary prefix installed.
	try
	{
		std::vector<VSURFACE_REGION> staged = hVSurface->RegionList;
		if (uiNumRegions > staged.max_size() - staged.size())
			return FALSE;
		staged.reserve(staged.size() + uiNumRegions);
		for (UINT16 index = 0; index < uiNumRegions; ++index)
			staged.push_back(*ppNewRegions[index]);
		hVSurface->RegionList.swap(staged);
		return TRUE;
	}
	catch (...)
	{
		return FALSE;
	}
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
	if (hVSurface->RegionList.size() >
		std::numeric_limits<UINT32>::max())
		return FALSE;
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

BOOLEAN MakeVSurfaceFromVObject(
	UINT32 uiVObject, UINT16 usSubIndex, UINT32* puiVSurface)
{
	if (!puiVSurface) return FALSE;

	HVOBJECT source = nullptr;
	if (!GetVideoObject(&source, uiVObject) || !source ||
		!source->pETRLEObject ||
		usSubIndex >= source->usNumberOfObjects)
		return FALSE;
	const ETRLEObject& region = source->pETRLEObject[usSubIndex];
	if (region.usWidth == 0 || region.usHeight == 0) return FALSE;

	VSURFACE_DESC description{};
	description.fCreateFlags = VSURFACE_CREATE_DEFAULT;
	description.usWidth = region.usWidth;
	description.usHeight = region.usHeight;
	description.ubBitDepth = 16;

	UINT32 surface = 0;
	if (!AddStandardVideoSurface(&description, &surface))
		return FALSE;
	if (!BltVideoObjectFromIndex(
		surface, uiVObject, usSubIndex, 0, 0,
		VO_BLT_SRCTRANSPARENCY, nullptr))
	{
		DeleteVideoSurfaceFromIndex(surface);
		return FALSE;
	}

	*puiVSurface = surface;
	return TRUE;
}

BOOLEAN PixelateVideoSurfaceRect(
	UINT32 destination, INT32 x1, INT32 y1, INT32 x2, INT32 y2)
{
	HVSURFACE surface = nullptr;
	if (!GetVideoSurface(&surface, destination) || !surface ||
		surface->ubBitDepth <= 8 || x1 > x2 || y1 > y2)
		return FALSE;

	SGPRect previousClip{};
	GetClippingRect(&previousClip);
	SGPRect surfaceClip{
		std::max<INT32>(previousClip.iLeft, 0),
		std::max<INT32>(previousClip.iTop, 0),
		std::min<INT32>(
			previousClip.iRight,
			static_cast<INT32>(surface->usWidth)),
		std::min<INT32>(
			previousClip.iBottom,
			static_cast<INT32>(surface->usHeight))};
	if (surfaceClip.iLeft >= surfaceClip.iRight ||
		surfaceClip.iTop >= surfaceClip.iBottom)
		return FALSE;

	UINT8 pattern[8][8] = {
		{0, 1, 0, 1, 0, 1, 0, 1},
		{1, 0, 1, 0, 1, 0, 1, 0},
		{0, 1, 0, 1, 0, 1, 0, 1},
		{1, 0, 1, 0, 1, 0, 1, 0},
		{0, 1, 0, 1, 0, 1, 0, 1},
		{1, 0, 1, 0, 1, 0, 1, 0},
		{0, 1, 0, 1, 0, 1, 0, 1},
		{1, 0, 1, 0, 1, 0, 1, 0}};
	SGPRect area{x1, y1, x2, y2};
	UINT32 pitch = 0;
	PIXEL* const pixels =
		reinterpret_cast<PIXEL*>(LockVideoSurface(destination, &pitch));
	if (!pixels) return FALSE;

	SetClippingRect(&surfaceClip);
	const BOOLEAN result = Blt16BPPBufferPixelateRect(
		pixels, pitch, &area, pattern);
	SetClippingRect(&previousClip);
	UnLockVideoSurface(destination);
	return result;
}

BOOLEAN SetClipList(HVSURFACE hVSurface, SGPRect* regionData, UINT16 numRegions)
{
	if (!hVSurface || !regionData || numRegions == 0) return FALSE;

	// This was a DirectDraw multi-rectangle clipper attached to subsequent
	// hardware blits. SDL heap surfaces have no equivalent persistent object,
	// and the CPU/engine commands currently accept one rectangular clip.
	// Reporting failure is intentional: claiming success while ignoring a
	// mod-provided clip list can draw outside every requested region.
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// Blitters
///////////////////////////////////////////////////////////////////////////////
//
// All blitting on the SDL3 path is CPU-side. The DD-era "UsingDD"
// function name is preserved as an alias so legacy call sites don't
// have to be touched -- it's just a CPU memcpy / color-keyed loop now.

namespace {

struct ClippedBlit
{
	INT32 destinationX = 0;
	INT32 destinationY = 0;
	INT32 sourceX = 0;
	INT32 sourceY = 0;
	INT32 width = 0;
	INT32 height = 0;
	bool mirrorHorizontally = false;
};

// Resolve a half-open source rectangle and destination origin without doing
// signed 32-bit additions. For mirrored copies, sourceX is the rightmost
// source pixel corresponding to the leftmost visible destination pixel.
bool ResolveBlit(
	HVSURFACE destination, HVSURFACE source,
	INT32 destinationX, INT32 destinationY,
	INT32 sourceLeft, INT32 sourceTop,
	INT32 sourceRight, INT32 sourceBottom,
	bool mirrorHorizontally, ClippedBlit& result)
{
	if (!destination || !source) return false;
	const std::int64_t width =
		static_cast<std::int64_t>(sourceRight) - sourceLeft;
	const std::int64_t height =
		static_cast<std::int64_t>(sourceBottom) - sourceTop;
	if (width <= 0 || height <= 0) return false;

	std::int64_t firstColumn = 0;
	std::int64_t lastColumn = width;
	firstColumn = std::max<std::int64_t>(
		firstColumn, -static_cast<std::int64_t>(destinationX));
	lastColumn = std::min<std::int64_t>(
		lastColumn,
		static_cast<std::int64_t>(destination->usWidth) - destinationX);
	if (mirrorHorizontally)
	{
		firstColumn = std::max<std::int64_t>(
			firstColumn,
			static_cast<std::int64_t>(sourceRight) - source->usWidth);
		lastColumn = std::min<std::int64_t>(
			lastColumn, static_cast<std::int64_t>(sourceRight));
	}
	else
	{
		firstColumn = std::max<std::int64_t>(
			firstColumn, -static_cast<std::int64_t>(sourceLeft));
		lastColumn = std::min<std::int64_t>(
			lastColumn,
			static_cast<std::int64_t>(source->usWidth) - sourceLeft);
	}

	std::int64_t firstRow = 0;
	std::int64_t lastRow = height;
	firstRow = std::max<std::int64_t>(
		firstRow, -static_cast<std::int64_t>(destinationY));
	lastRow = std::min<std::int64_t>(
		lastRow,
		static_cast<std::int64_t>(destination->usHeight) - destinationY);
	firstRow = std::max<std::int64_t>(
		firstRow, -static_cast<std::int64_t>(sourceTop));
	lastRow = std::min<std::int64_t>(
		lastRow,
		static_cast<std::int64_t>(source->usHeight) - sourceTop);
	if (firstColumn >= lastColumn || firstRow >= lastRow) return false;

	result.destinationX = static_cast<INT32>(
		static_cast<std::int64_t>(destinationX) + firstColumn);
	result.destinationY = static_cast<INT32>(
		static_cast<std::int64_t>(destinationY) + firstRow);
	result.sourceX = static_cast<INT32>(
		mirrorHorizontally
			? static_cast<std::int64_t>(sourceRight) - 1 - firstColumn
			: static_cast<std::int64_t>(sourceLeft) + firstColumn);
	result.sourceY = static_cast<INT32>(
		static_cast<std::int64_t>(sourceTop) + firstRow);
	result.width = static_cast<INT32>(lastColumn - firstColumn);
	result.height = static_cast<INT32>(lastRow - firstRow);
	result.mirrorHorizontally = mirrorHorizontally;
	return true;
}

template <typename Pixel>
bool MatchesColorKey(Pixel pixel, Pixel key)
{
	if constexpr (sizeof(Pixel) == sizeof(std::uint32_t))
	{
		return (static_cast<std::uint32_t>(pixel) & 0x00ffffffu) ==
			(static_cast<std::uint32_t>(key) & 0x00ffffffu);
	}
	return pixel == key;
}

template <typename Pixel>
bool BlitPixels(
	Pixel* destination, UINT32 destinationPitch,
	const Pixel* source, UINT32 sourcePitch,
	const ClippedBlit& area,
	bool useSourceKey, Pixel sourceKey,
	bool useDestinationKey, Pixel destinationKey)
{
	if (!destination || !source ||
		destinationPitch < sizeof(Pixel) ||
		sourcePitch < sizeof(Pixel))
		return false;
	const bool aliases = destination == source;

	if (!area.mirrorHorizontally &&
		!useSourceKey && !useDestinationKey)
	{
		const std::size_t rowBytes =
			static_cast<std::size_t>(area.width) * sizeof(Pixel);
		const bool bottomUp =
			aliases && area.destinationY > area.sourceY;
		for (INT32 iteration = 0; iteration < area.height; ++iteration)
		{
			const INT32 row = bottomUp
				? area.height - iteration - 1 : iteration;
			const Pixel* sourceRow = reinterpret_cast<const Pixel*>(
				reinterpret_cast<const UINT8*>(source) +
					static_cast<std::size_t>(area.sourceY + row) *
						sourcePitch) +
				area.sourceX;
			Pixel* destinationRow = reinterpret_cast<Pixel*>(
				reinterpret_cast<UINT8*>(destination) +
					static_cast<std::size_t>(area.destinationY + row) *
						destinationPitch) +
				area.destinationX;
			if (aliases)
				std::memmove(destinationRow, sourceRow, rowBytes);
			else
				std::memcpy(destinationRow, sourceRow, rowBytes);
		}
		return true;
	}

	std::vector<Pixel> mirroredSource;
	if (aliases && area.mirrorHorizontally)
	{
		try
		{
			mirroredSource.resize(static_cast<std::size_t>(area.width));
		}
		catch (...)
		{
			return false;
		}
	}

	const bool bottomUp = aliases && area.destinationY > area.sourceY;
	const bool rightToLeft =
		aliases && !area.mirrorHorizontally &&
		area.destinationY == area.sourceY &&
		area.destinationX > area.sourceX;
	for (INT32 rowIteration = 0;
		rowIteration < area.height; ++rowIteration)
	{
		const INT32 row = bottomUp
			? area.height - rowIteration - 1 : rowIteration;
		const Pixel* sourceRow = reinterpret_cast<const Pixel*>(
			reinterpret_cast<const UINT8*>(source) +
				static_cast<std::size_t>(area.sourceY + row) *
					sourcePitch);
		Pixel* destinationRow = reinterpret_cast<Pixel*>(
			reinterpret_cast<UINT8*>(destination) +
				static_cast<std::size_t>(area.destinationY + row) *
					destinationPitch);
		if (!mirroredSource.empty())
		{
			for (INT32 column = 0; column < area.width; ++column)
				mirroredSource[static_cast<std::size_t>(column)] =
					sourceRow[area.sourceX - column];
		}

		for (INT32 columnIteration = 0;
			columnIteration < area.width; ++columnIteration)
		{
			const INT32 column = rightToLeft
				? area.width - columnIteration - 1 : columnIteration;
			const Pixel value = !mirroredSource.empty()
				? mirroredSource[static_cast<std::size_t>(column)]
				: sourceRow[
					area.mirrorHorizontally
						? area.sourceX - column
						: area.sourceX + column];
			if (useSourceKey && MatchesColorKey(value, sourceKey))
				continue;
			Pixel& target =
				destinationRow[area.destinationX + column];
			if (useDestinationKey &&
				!MatchesColorKey(target, destinationKey))
				continue;
			target = value;
		}
	}
	return true;
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

void FillRect8(UINT8* destination, UINT32 pitchBytes,
	INT32 x, INT32 y, INT32 width, INT32 height, UINT8 color)
{
	for (INT32 row = 0; row < height; ++row)
	{
		std::memset(
			destination +
				static_cast<std::size_t>(y + row) * pitchBytes + x,
			color, static_cast<std::size_t>(width));
	}
}

BOOLEAN FillSurfaceDirect(
	HVSURFACE destination, const SGPRect& requested, COLORVAL color)
{
	if (!destination) return FALSE;
	std::int64_t left = std::min<std::int64_t>(
		requested.iLeft, requested.iRight);
	std::int64_t right = std::max<std::int64_t>(
		requested.iLeft, requested.iRight);
	std::int64_t top = std::min<std::int64_t>(
		requested.iTop, requested.iBottom);
	std::int64_t bottom = std::max<std::int64_t>(
		requested.iTop, requested.iBottom);
	left = std::max<std::int64_t>(left, 0);
	top = std::max<std::int64_t>(top, 0);
	right = std::min<std::int64_t>(right, destination->usWidth);
	bottom = std::min<std::int64_t>(bottom, destination->usHeight);
	if (left >= right || top >= bottom) return TRUE;

	UINT32 pitch = 0;
	BYTE* const pixels = LockVideoSurfaceBuffer(destination, &pitch);
	if (!pixels) return FALSE;
	BOOLEAN result = FALSE;
	if (destination->ubBitDepth == 16)
	{
		FillRect16(
			reinterpret_cast<PIXEL*>(pixels), pitch,
			static_cast<INT32>(left), static_cast<INT32>(top),
			static_cast<INT32>(right - left),
			static_cast<INT32>(bottom - top),
			static_cast<PIXEL>(color));
		result = TRUE;
	}
	else if (destination->ubBitDepth == 8)
	{
		FillRect8(
			pixels, pitch,
			static_cast<INT32>(left), static_cast<INT32>(top),
			static_cast<INT32>(right - left),
			static_cast<INT32>(bottom - top),
			static_cast<UINT8>(color));
		result = TRUE;
	}
	UnLockVideoSurfaceBuffer(destination);
	return result;
}

} // namespace

BOOLEAN BltVideoSurfaceToVideoSurface(HVSURFACE hDst, HVSURFACE hSrc,
                                      UINT16 usIndex, INT32 iDestX, INT32 iDestY,
                                      INT32 fBltFlags, blt_vs_fx* pBltFx)
{
	if (!hDst) return FALSE;
	if ((fBltFlags & VS_BLT_SRCREGION) &&
		(fBltFlags & VS_BLT_SRCSUBRECT))
		return FALSE;
	if ((fBltFlags & VS_BLT_COLORFILL) &&
		(fBltFlags & VS_BLT_COLORFILLRECT))
		return FALSE;

	if (fBltFlags & VS_BLT_DESTREGION)
	{
		if (!pBltFx) return FALSE;
		VSURFACE_REGION region{};
		if (!GetVSurfaceRegion(hDst, pBltFx->DestRegion, &region))
			return FALSE;
		iDestX = region.RegionCoords.iLeft;
		iDestY = region.RegionCoords.iTop;
	}

	if (fBltFlags & VS_BLT_COLORFILLRECT)
	{
		if (!pBltFx) return FALSE;
		return FillSurfaceDirect(
			hDst, pBltFx->FillRect, pBltFx->ColorFill);
	}
	if (fBltFlags & VS_BLT_COLORFILL)
	{
		if (!pBltFx) return FALSE;
		const SGPRect wholeSurface{
			0, 0, hDst->usWidth, hDst->usHeight};
		return FillSurfaceDirect(
			hDst, wholeSurface, pBltFx->ColorFill);
	}
	if (!hSrc || hDst->ubBitDepth != hSrc->ubBitDepth) return FALSE;

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

	ClippedBlit area;
	if (!ResolveBlit(
		hDst, hSrc, iDestX, iDestY, sL, sT, sR, sB,
		(fBltFlags & VS_BLT_MIRROR_Y) != 0, area))
		return TRUE;

	UINT32 srcPitch = 0, dstPitch = 0;
	BYTE* srcBuf = LockVideoSurfaceBuffer(hSrc, &srcPitch);
	BYTE* dstBuf = LockVideoSurfaceBuffer(hDst, &dstPitch);
	if (!srcBuf || !dstBuf)
	{
		UnLockVideoSurfaceBuffer(hSrc);
		UnLockVideoSurfaceBuffer(hDst);
		return FALSE;
	}

	const bool useSourceKey =
		(fBltFlags & VS_BLT_USECOLORKEY) != 0;
	const bool useDestinationKey =
		(fBltFlags & VS_BLT_USEDESTCOLORKEY) != 0;
	BOOLEAN result = FALSE;
	if (hDst->ubBitDepth == 16)
	{
		result = BlitPixels(
			reinterpret_cast<PIXEL*>(dstBuf), dstPitch,
			reinterpret_cast<const PIXEL*>(srcBuf), srcPitch,
			area,
			useSourceKey,
			PixFromColor16(static_cast<UINT16>(
				hSrc->TransparentColor)),
			useDestinationKey,
			PixFromColor16(static_cast<UINT16>(
				hDst->TransparentColor))) ? TRUE : FALSE;
	}
	else if (hDst->ubBitDepth == 8)
	{
		result = BlitPixels(
			dstBuf, dstPitch, srcBuf, srcPitch, area,
			useSourceKey,
			static_cast<UINT8>(hSrc->TransparentColor),
			useDestinationKey,
			static_cast<UINT8>(hDst->TransparentColor)) ? TRUE : FALSE;
	}

	UnLockVideoSurfaceBuffer(hSrc);
	UnLockVideoSurfaceBuffer(hDst);
	return result;
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

BOOLEAN ImageFillVideoSurfaceArea(
	UINT32 destination,
	INT32 destinationLeft,
	INT32 destinationTop,
	INT32 destinationRight,
	INT32 destinationBottom,
	HVOBJECT background,
	UINT16 index,
	INT16 offsetX,
	INT16 offsetY)
{
	if (!background || !background->pETRLEObject ||
		index >= background->usNumberOfObjects ||
		destinationRight <= destinationLeft ||
		destinationBottom <= destinationTop)
		return FALSE;

	HVSURFACE destinationSurface = nullptr;
	if (!GetVideoSurface(&destinationSurface, destination) ||
		!destinationSurface)
		return FALSE;

	const ETRLEObject& tile = background->pETRLEObject[index];
	const std::int64_t periodX =
		static_cast<std::int64_t>(tile.usWidth) + tile.sOffsetX;
	const std::int64_t periodY =
		static_cast<std::int64_t>(tile.usHeight) + tile.sOffsetY;
	if (periodX <= 0 || periodY <= 0) return FALSE;

	SGPRect oldClip;
	GetClippingRect(&oldClip);
	if (oldClip.iLeft >= oldClip.iRight ||
		oldClip.iTop >= oldClip.iBottom)
	{
		oldClip = SGPRect{
			0, 0,
			static_cast<INT32>(destinationSurface->usWidth),
			static_cast<INT32>(destinationSurface->usHeight)};
	}
	SGPRect fillClip{
		std::max<INT32>(
			std::max<INT32>(destinationLeft, oldClip.iLeft), 0),
		std::max<INT32>(
			std::max<INT32>(destinationTop, oldClip.iTop), 0),
		std::min<INT32>(
			std::min<INT32>(destinationRight, oldClip.iRight),
			destinationSurface->usWidth),
		std::min<INT32>(
			std::min<INT32>(destinationBottom, oldClip.iBottom),
			destinationSurface->usHeight)};
	if (fillClip.iLeft >= fillClip.iRight ||
		fillClip.iTop >= fillClip.iBottom)
		return FALSE;

	class ClipRestore
	{
	public:
		explicit ClipRestore(const SGPRect& clip) : clip_(clip) {}
		~ClipRestore() { SetClippingRect(&clip_); }

	private:
		SGPRect clip_;
	} restore(oldClip);
	SetClippingRect(&fillClip);

	auto normalizeOffset = [](std::int64_t offset, std::int64_t period)
	{
		offset %= period;
		if (offset >= 0) offset -= period;
		return offset;
	};
	const std::int64_t firstX =
		static_cast<std::int64_t>(destinationLeft) +
		normalizeOffset(offsetX, periodX);
	const std::int64_t firstY =
		static_cast<std::int64_t>(destinationTop) +
		normalizeOffset(offsetY, periodY);
	auto advanceToVisible = [](
		std::int64_t first,
		std::int64_t visible,
		std::int64_t period)
	{
		if (first < visible)
			first += ((visible - first) / period) * period;
		return first;
	};
	const std::int64_t visibleFirstX =
		advanceToVisible(firstX, fillClip.iLeft, periodX);
	const std::int64_t visibleFirstY =
		advanceToVisible(firstY, fillClip.iTop, periodY);
	const std::int64_t lastX =
		static_cast<std::int64_t>(fillClip.iRight) - tile.sOffsetX;
	const std::int64_t lastY =
		static_cast<std::int64_t>(fillClip.iBottom) - tile.sOffsetY;
	for (std::int64_t y = visibleFirstY; y < lastY; y += periodY)
	{
		if (y < std::numeric_limits<INT32>::min() ||
			y > std::numeric_limits<INT32>::max())
			continue;
		for (std::int64_t x = visibleFirstX;
			x < lastX; x += periodX)
		{
			if (x < std::numeric_limits<INT32>::min() ||
				x > std::numeric_limits<INT32>::max())
				continue;
			if (!BltVideoObject(
				destination,
				background,
				index,
				static_cast<INT32>(x),
				static_cast<INT32>(y),
				VO_BLT_SRCTRANSPARENCY,
				nullptr))
				return FALSE;
		}
	}
	return TRUE;
}

BOOLEAN ShadowVideoSurfaceImage(
	UINT32 destination,
	HVOBJECT image,
	INT32 positionX,
	INT32 positionY)
{
	if (!image || !image->pETRLEObject ||
		image->usNumberOfObjects == 0)
		return FALSE;

	const ETRLEObject& object = image->pETRLEObject[0];
	const std::int64_t left = positionX;
	const std::int64_t top = positionY;
	const std::int64_t right = left + object.usWidth;
	const std::int64_t bottom = top + object.usHeight;
	if (left < std::numeric_limits<INT32>::min() ||
		top < std::numeric_limits<INT32>::min() ||
		right + 3 > std::numeric_limits<INT32>::max() ||
		bottom + 3 > std::numeric_limits<INT32>::max())
		return FALSE;

	(void)ShadowVideoSurfaceRect(
		destination,
		static_cast<INT32>(left + 3),
		static_cast<INT32>(bottom),
		static_cast<INT32>(right),
		static_cast<INT32>(bottom + 3));
	(void)ShadowVideoSurfaceRect(
		destination,
		static_cast<INT32>(right),
		static_cast<INT32>(top + 3),
		static_cast<INT32>(right + 3),
		static_cast<INT32>(bottom));
	// The original API reports success once a valid image is accepted; callers
	// do not use the individual clipped shadow results.
	return TRUE;
}
