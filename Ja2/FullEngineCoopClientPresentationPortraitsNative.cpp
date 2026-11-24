#include "FullEngineCoopClientPresentationPortraits.h"

#include "FullEngineCoopClientPresentationPassiveUi.h"
#include "himage.h"
#include "local.h"
#include "vobject.h"
#include "vobject_blitters.h"
#include "vsurface.h"

#include <cstring>

namespace
{
bool ValidPortrait(HVOBJECT object) noexcept
{
	if (!object || object->ubBitDepth != 8 || !object->pETRLEObject ||
		!object->usNumberOfObjects || !object->pPixData || !object->pPaletteEntry ||
		!object->p16BPPPalette || !object->pShadeCurrent) return false;
	const auto& frame = object->pETRLEObject[0];
	if (frame.uiDataOffset > object->uiSizePixData ||
		frame.uiDataLength > object->uiSizePixData - frame.uiDataOffset) return false;
	return IsValidFullEngineCoopClientPortraitFrame(
		static_cast<const std::uint8_t*>(object->pPixData) + frame.uiDataOffset,
		frame.uiDataLength, frame.usWidth, frame.usHeight);
}

bool Load(const char* path, std::uint32_t& handle, void*) noexcept
{
	if (!path || std::strlen(path) >= FullEngineCoopClientPortraitPathBytes) return false;
	UINT32 candidate = 0;
	bool loaded = false;
	try
	{
		VOBJECT_DESC description{};
		description.fCreateFlags = VOBJECT_CREATE_FROMFILE;
		std::strcpy(description.ImageFile, path);
		loaded = AddVideoObject(&description, &candidate) != FALSE;
		HVOBJECT object = nullptr;
		if (loaded && GetVideoObject(&object, candidate) && ValidPortrait(object))
		{ handle = candidate; return true; }
	}
	catch (...) {}
	if (loaded) DeleteVideoObjectFromIndex(candidate);
	return false;
}

void Release(std::uint32_t handle, void*) noexcept
{
	(void)DeleteVideoObjectFromIndex(handle);
}

bool Blit(std::uint32_t handle, const FullEngineCoopClientPassiveUiRect& face, void*) noexcept
{
	if (!face.valid() || face.right > SCREEN_WIDTH || face.bottom > SCREEN_HEIGHT ||
		face.right - face.left != 48 || face.bottom - face.top != 43) return false;
	HVOBJECT object = nullptr;
	if (!GetVideoObject(&object, handle) || !ValidPortrait(object)) return false;
	const auto& frame = object->pETRLEObject[0];
	const SGPRect clip{face.left, face.top, face.right, face.bottom};
	struct ClipScope
	{
		SGPRect previous = ClippingRect;
		explicit ClipScope(const SGPRect& rectangle) noexcept { ClippingRect = rectangle; }
		~ClipScope() noexcept { ClippingRect = previous; }
	};
	const ClipScope scopedClip(clip);
	try
	{
		return BltVideoObjectEffectToSurface(FRAME_BUFFER, object, 0,
			face.left + (48 - frame.usWidth) / 2 - frame.sOffsetX,
			face.top + (43 - frame.usHeight) / 2 - frame.sOffsetY,
			VOBJECT_DRAW_SOURCE_TRANSPARENCY, &clip) != FALSE;
	}
	catch (...) { return false; }
}
}

FullEngineCoopClientPortraitServices MakeFullEngineCoopClientPortraitNativeServices() noexcept
{
	return {Load, Release, Blit, nullptr};
}
