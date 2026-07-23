#include "DirectDraw Calls.h"
#include <stdio.h>
#include "DEBUG.H"
#include "video.h"
#include "himage.h"
#include "vobject.h"
#include "WCheck.h"
#include "vobject_blitters.h"
#include "sgp.h"

#include <Engine/Adapters/Legacy/LegacyRenderCommandGateway.h>
#include <Engine/Adapters/Legacy/LegacyRenderSurfaceGateway.h>
#include <Engine/Adapters/Legacy/PlatformRenderSurfaceAccess.h>
#include <Engine/Adapters/Legacy/PlatformVideoObjectBackend.h>
#include <Engine/Core/StableResourceRegistry.h>
#include <Engine/Core/UniqueResourcePtr.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>

// ******************************************************************************
//
// Video Object SGP Module
//
// Video Objects are used to contain any imagery which requires blitting. The data
// is contained within a Direct Draw surface. Palette information is in both
// a Direct Draw Palette and a 16BPP palette structure for 8->16 BPP Blits.
// Blitting is done via Direct Draw as well as custum blitters. Regions are
// used to define local coordinates within the surface
//
// Second Revision: Dec 10, 1996, Andrew Emmons
//
// *******************************************************************************


// *******************************************************************************
// Defines
// *******************************************************************************

// This define is sent to CreateList SGP function. It dynamically re-sizes if
// the list gets larger
#define DEFAULT_VIDEO_OBJECT_LIST_SIZE	10

#define COMPRESS_TRANSPARENT				0x80
#define COMPRESS_RUN_MASK						0x7F

// *******************************************************************************
// External Functions and variables
// *******************************************************************************

// *******************************************************************************
// LOCAL functions
// *******************************************************************************


// *******************************************************************************
// LOCAL global variables
// *******************************************************************************



BOOLEAN	gfVideoObjectsInit=FALSE;

namespace
{
struct VideoObjectReleaser
{
	void operator()(SGPVObject* object) const { DeleteVideoObject(object); }
};

struct VideoObjectResource
{
	explicit VideoObjectResource(HVOBJECT object)
		: object(object)
	{
	}

	UniqueResourcePtr<SGPVObject, VideoObjectReleaser> object;
#ifdef SGP_VIDEO_DEBUGGING
	std::string name;
	std::string code;
#endif
};

using VideoObjectRegistry = StableResourceRegistry<VideoObjectResource, UINT32>;

struct RenderImageResource
{
	HVOBJECT object = nullptr;
};

using RenderImageRegistry =
	StableResourceRegistry<RenderImageResource, RenderImageId>;

// Render-image IDs deliberately live above the 32-bit compatibility-manager
// range. This lets every CreateVideoObject allocation participate in engine
// commands without changing the sequential UINT32 handles exposed by the
// legacy manager.
RenderImageRegistry gRenderImages(RenderImageRegistry::Limits{
	RenderImageId{1} << 32, 1,
	std::numeric_limits<RenderImageId>::max()});
std::unordered_map<HVOBJECT, RenderImageId> gRenderImageHandles;

VideoObjectRegistry gVideoObjects(
	VideoObjectRegistry::Limits{1, 1, 0xffffffefu});
std::unordered_map<HVOBJECT, UINT32> gVideoObjectHandles;

RenderImageCompositeMode CompositeModeFor(UINT32 flags)
{
	// Preserve the established priority in BltVideoObjectToBuffer: the
	// transparency bit wins when combined with any other legacy flag.
	if (flags & VO_BLT_SRCTRANSPARENCY)
		return RenderImageCompositeMode::SourceTransparency;
	if (flags & VO_BLT_SHADOW)
		return RenderImageCompositeMode::Shadow;
	return RenderImageCompositeMode::Opaque;
}

std::optional<UINT32> LegacyFlagsFor(RenderImageCompositeMode mode)
{
	switch (mode)
	{
	case RenderImageCompositeMode::Opaque: return 0;
	case RenderImageCompositeMode::SourceTransparency:
		return VO_BLT_SRCTRANSPARENCY;
	case RenderImageCompositeMode::Shadow: return VO_BLT_SHADOW;
	case RenderImageCompositeMode::Intensity: return std::nullopt;
	}
	return std::nullopt;
}

bool FindVideoObjectHandle(HVOBJECT object, UINT32& handle)
{
	const auto found = gVideoObjectHandles.find(object);
	if (found == gVideoObjectHandles.end()) return false;
	handle = found->second;
	return true;
}

bool RegisterRenderImage(HVOBJECT object)
{
	if (!object) return false;
	std::optional<RenderImageId> image;
	try
	{
		image = gRenderImages.insert(RenderImageResource{object});
	}
	catch (...)
	{
		return false;
	}
	if (!image) return false;
	try
	{
		const auto inserted = gRenderImageHandles.emplace(object, *image);
		if (inserted.second) return true;
	}
	catch (...)
	{
	}
	(void)gRenderImages.erase(*image);
	return false;
}

void UnregisterRenderImage(HVOBJECT object)
{
	const auto found = gRenderImageHandles.find(object);
	if (found == gRenderImageHandles.end()) return;
	const RenderImageId image = found->second;
	gRenderImageHandles.erase(found);
	(void)gRenderImages.erase(image);
}

bool FindRenderImage(HVOBJECT object, RenderImageId& image)
{
	const auto found = gRenderImageHandles.find(object);
	if (found == gRenderImageHandles.end()) return false;
	image = found->second;
	return true;
}

HVOBJECT ResolveRenderImage(RenderImageId image)
{
	if (image <= std::numeric_limits<UINT32>::max())
	{
		VideoObjectResource* const managed =
			gVideoObjects.find(static_cast<UINT32>(image));
		if (managed) return managed->object.get();
	}
	RenderImageResource* const resource = gRenderImages.find(image);
	return resource ? resource->object : nullptr;
}

HVOBJECT FinishVideoObjectCreation(HVOBJECT object)
{
	if (RegisterRenderImage(object)) return object;
	(void)DeleteVideoObject(object);
	return nullptr;
}

PIXEL LegacyPixelFor(const RenderColor& color) noexcept
{
	if constexpr (sizeof(PIXEL) == sizeof(std::uint32_t))
	{
		return static_cast<PIXEL>(
			(static_cast<std::uint32_t>(color.alpha) << 24) |
			(static_cast<std::uint32_t>(color.red) << 16) |
			(static_cast<std::uint32_t>(color.green) << 8) |
			static_cast<std::uint32_t>(color.blue));
	}
	else
	{
		return Get16BPPColor(FROMRGB(
			color.red, color.green, color.blue));
	}
}

class ClippingRegionLease
{
public:
	explicit ClippingRegionLease(const RenderSurfaceRegion& region)
		: previous_(ClippingRect)
	{
		ClippingRect = SGPRect{
			region.left, region.top, region.right, region.bottom};
	}

	~ClippingRegionLease()
	{
		ClippingRect = previous_;
	}

	ClippingRegionLease(const ClippingRegionLease&) = delete;
	ClippingRegionLease& operator=(const ClippingRegionLease&) = delete;

private:
	SGPRect previous_;
};

class PlatformSurfaceMappingLease
{
public:
	explicit PlatformSurfaceMappingLease(RenderSurfaceId surface)
		: surface_(surface),
		  mapped_(GetPlatformRenderSurfaceAccess().map(surface_, mapping_))
	{
	}

	~PlatformSurfaceMappingLease() noexcept
	{
		if (!mapped_) return;
		try
		{
			GetPlatformRenderSurfaceAccess().unmap(surface_);
		}
		catch (...)
		{
		}
	}

	PlatformSurfaceMappingLease(const PlatformSurfaceMappingLease&) = delete;
	PlatformSurfaceMappingLease& operator=(
		const PlatformSurfaceMappingLease&) = delete;

	explicit operator bool() const { return mapped_; }
	const MutableRenderSurface& mapping() const { return mapping_; }

private:
	RenderSurfaceId surface_;
	MutableRenderSurface mapping_;
	bool mapped_ = false;
};

bool SubmitVideoObjectDraw(
	UINT32 destination,
	UINT32 image,
	UINT16 frame,
	INT32 destinationX,
	INT32 destinationY,
	UINT32 flags)
{
	SGPRect clipping;
	GetClippingRect(&clipping);
	return DrawLegacyRenderImage(RenderImageDrawCommand{
		destination,
		image,
		frame,
		RenderSurfacePoint{destinationX, destinationY},
		RenderSurfaceRegion{
			clipping.iLeft, clipping.iTop,
			clipping.iRight, clipping.iBottom},
		CompositeModeFor(flags)});
}

bool SubmitVideoObjectEffectDraw(
	UINT32 destination,
	HVOBJECT object,
	UINT16 frame,
	INT32 destinationX,
	INT32 destinationY,
	RenderImageCompositeMode mode,
	const SGPRect* clipping)
{
	if (!object || object->ubBitDepth != 8 ||
		frame >= object->usNumberOfObjects ||
		!object->pETRLEObject || !object->pPixData ||
		(mode == RenderImageCompositeMode::SourceTransparency &&
			!object->pShadeCurrent))
		return false;
	switch (mode)
	{
	case RenderImageCompositeMode::SourceTransparency:
	case RenderImageCompositeMode::Shadow:
	case RenderImageCompositeMode::Intensity:
		break;
	default:
		return false;
	}
	RenderImageId image = 0;
	if (!FindRenderImage(object, image)) return false;

	SGPRect currentClipping;
	if (!clipping)
	{
		GetClippingRect(&currentClipping);
		clipping = &currentClipping;
	}
	return DrawLegacyRenderImage(RenderImageDrawCommand{
		destination,
		image,
		frame,
		RenderSurfacePoint{destinationX, destinationY},
		RenderSurfaceRegion{
			clipping->iLeft, clipping->iTop,
			clipping->iRight, clipping->iBottom},
		mode});
}

bool SubmitVideoObjectDepthDraw(
	UINT32 destination,
	HVOBJECT object,
	UINT16 frame,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 depth,
	BOOLEAN writeDepth,
	RenderImageDepthEffect effect,
	const SGPRect* clipping)
{
	if (!object || object->ubBitDepth != 8 ||
		frame >= object->usNumberOfObjects ||
		!object->pETRLEObject || !object->pPixData ||
		(effect == RenderImageDepthEffect::SourcePalette &&
			!object->pShadeCurrent))
		return false;
	RenderDepthCompareMode comparison;
	switch (effect)
	{
	case RenderImageDepthEffect::SourcePalette:
		comparison = RenderDepthCompareMode::GreaterOrEqual;
		break;
	case RenderImageDepthEffect::ShadeDestination:
	case RenderImageDepthEffect::IntensifyDestination:
		comparison = RenderDepthCompareMode::Greater;
		break;
	default:
		return false;
	}
	RenderImageId image = 0;
	if (!FindRenderImage(object, image)) return false;
	const RenderSurfaceId depthSurface =
		GetLegacyRenderSurfaceAccess().surfaceFor(
			RenderSurfaceRole::DepthBuffer);
	if (depthSurface == 0) return false;

	SGPRect currentClipping;
	if (!clipping)
	{
		GetClippingRect(&currentClipping);
		clipping = &currentClipping;
	}
	return DrawLegacyRenderImageDepth(RenderImageDepthDrawCommand{
		destination,
		depthSurface,
		image,
		frame,
		RenderSurfacePoint{destinationX, destinationY},
		RenderSurfaceRegion{
			clipping->iLeft, clipping->iTop,
			clipping->iRight, clipping->iBottom},
		depth,
		comparison,
		writeDepth != FALSE ?
			RenderDepthWriteMode::ReplaceOnPass :
			RenderDepthWriteMode::Preserve,
		effect});
}

bool SubmitVideoObjectOutline(
	UINT32 destination,
	UINT32 image,
	UINT16 frame,
	INT32 destinationX,
	INT32 destinationY,
	RenderImageOutlineMode mode,
	PIXEL color,
	BOOLEAN drawOutline)
{
	SGPRect clipping;
	GetClippingRect(&clipping);
	const RenderColor commandColor =
		mode == RenderImageOutlineMode::Color ?
			DecodeLegacyRenderColor(
				static_cast<std::uint32_t>(color)) :
			RenderColor{};
	return DrawLegacyRenderImageOutline(RenderImageOutlineCommand{
		destination,
		image,
		frame,
		RenderSurfacePoint{destinationX, destinationY},
		RenderSurfaceRegion{
			clipping.iLeft, clipping.iTop,
			clipping.iRight, clipping.iBottom},
		mode,
		commandColor,
		mode == RenderImageOutlineMode::Color &&
			drawOutline != FALSE});
}

bool SubmitVideoObjectOutlineDraw(
	UINT32 destination,
	HVOBJECT object,
	UINT16 frame,
	INT32 destinationX,
	INT32 destinationY,
	RenderImageOutlineMode mode,
	PIXEL color,
	BOOLEAN drawOutline,
	const SGPRect* clipping)
{
	if (!object || object->ubBitDepth != 8 ||
		frame >= object->usNumberOfObjects ||
		!object->pETRLEObject || !object->pPixData ||
		(mode == RenderImageOutlineMode::Color &&
			!object->pShadeCurrent))
		return false;
	switch (mode)
	{
	case RenderImageOutlineMode::Color:
	case RenderImageOutlineMode::Shadow:
		break;
	default:
		return false;
	}
	RenderImageId image = 0;
	if (!FindRenderImage(object, image)) return false;

	SGPRect currentClipping;
	if (!clipping)
	{
		GetClippingRect(&currentClipping);
		clipping = &currentClipping;
	}
	return DrawLegacyRenderImageOutline(RenderImageOutlineCommand{
		destination,
		image,
		frame,
		RenderSurfacePoint{destinationX, destinationY},
		RenderSurfaceRegion{
			clipping->iLeft, clipping->iTop,
			clipping->iRight, clipping->iBottom},
		mode,
		mode == RenderImageOutlineMode::Color ?
			DecodeLegacyRenderColor(
				static_cast<std::uint32_t>(color)) :
			RenderColor{},
		mode == RenderImageOutlineMode::Color &&
			drawOutline != FALSE});
}

bool SubmitVideoObjectDepthOutlineDraw(
	UINT32 destination,
	HVOBJECT object,
	UINT16 frame,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 depth,
	BOOLEAN writeDepth,
	RenderDepthCompareMode comparison,
	RenderImageDepthOutlineVisibility visibility,
	PIXEL color,
	BOOLEAN drawOutline,
	const SGPRect* clipping)
{
	if (!object || object->ubBitDepth != 8 ||
		frame >= object->usNumberOfObjects ||
		!object->pETRLEObject || !object->pPixData ||
		!object->pShadeCurrent)
		return false;
	switch (comparison)
	{
	case RenderDepthCompareMode::GreaterOrEqual:
	case RenderDepthCompareMode::Greater:
		break;
	default:
		return false;
	}
	switch (visibility)
	{
	case RenderImageDepthOutlineVisibility::VisibleOnly:
		if (comparison != RenderDepthCompareMode::GreaterOrEqual)
			return false;
		break;
	case RenderImageDepthOutlineVisibility::PixelateWhenObscured:
		if (writeDepth == FALSE) return false;
		break;
	default:
		return false;
	}
	RenderImageId image = 0;
	if (!FindRenderImage(object, image)) return false;
	const RenderSurfaceId depthSurface =
		GetLegacyRenderSurfaceAccess().surfaceFor(
			RenderSurfaceRole::DepthBuffer);
	if (depthSurface == 0) return false;

	SGPRect currentClipping;
	if (!clipping)
	{
		GetClippingRect(&currentClipping);
		clipping = &currentClipping;
	}
	return DrawLegacyRenderImageDepthOutline(
		RenderImageDepthOutlineCommand{
			destination,
			depthSurface,
			image,
			frame,
			RenderSurfacePoint{destinationX, destinationY},
			RenderSurfaceRegion{
				clipping->iLeft, clipping->iTop,
				clipping->iRight, clipping->iBottom},
			depth,
			comparison,
			writeDepth != FALSE ?
				RenderDepthWriteMode::ReplaceOnPass :
				RenderDepthWriteMode::Preserve,
			visibility,
			DecodeLegacyRenderColor(
				static_cast<std::uint32_t>(color)),
			drawOutline != FALSE});
}

BOOLEAN DrawVideoObjectToSurface(
	UINT32 destination,
	HVOBJECT object,
	UINT16 frame,
	INT32 destinationX,
	INT32 destinationY,
	UINT32 flags,
	blt_fx* effects)
{
	UINT32 pitch = 0;
	PIXEL* const buffer =
		reinterpret_cast<PIXEL*>(LockVideoSurface(destination, &pitch));
	if (!buffer) return FALSE;

	BOOLEAN drawn = FALSE;
	try
	{
		drawn = BltVideoObjectToBuffer(
			buffer, pitch, object, frame,
			destinationX, destinationY, flags, effects);
	}
	catch (...)
	{
		UnLockVideoSurface(destination);
		throw;
	}
	UnLockVideoSurface(destination);
	return drawn;
}

BOOLEAN Draw8BitVideoObjectOutlineToSurface(
	UINT32 destination,
	HVOBJECT object,
	UINT16 frame,
	INT32 destinationX,
	INT32 destinationY,
	RenderImageOutlineMode mode,
	PIXEL color,
	BOOLEAN drawOutline)
{
	UINT32 pitch = 0;
	PIXEL* const buffer =
		reinterpret_cast<PIXEL*>(LockVideoSurface(destination, &pitch));
	if (!buffer) return FALSE;

	try
	{
		const bool clipped =
			BltIsClipped(
				object, destinationX, destinationY,
				frame, &ClippingRect) != FALSE;
		switch (mode)
		{
		case RenderImageOutlineMode::Color:
			if (clipped)
			{
				Blt8BPPDataTo16BPPBufferOutlineClip(
					buffer, pitch, object,
					destinationX, destinationY, frame,
					color, drawOutline, &ClippingRect);
			}
			else
			{
				Blt8BPPDataTo16BPPBufferOutline(
					buffer, pitch, object,
					destinationX, destinationY, frame,
					color, drawOutline);
			}
			break;
		case RenderImageOutlineMode::Shadow:
			if (clipped)
			{
				Blt8BPPDataTo16BPPBufferOutlineShadowClip(
					buffer, pitch, object,
					destinationX, destinationY, frame,
					&ClippingRect);
			}
			else
			{
				Blt8BPPDataTo16BPPBufferOutlineShadow(
					buffer, pitch, object,
					destinationX, destinationY, frame);
			}
			break;
		default:
			UnLockVideoSurface(destination);
			return FALSE;
		}
	}
	catch (...)
	{
		UnLockVideoSurface(destination);
		throw;
	}
	UnLockVideoSurface(destination);
	return TRUE;
}

BOOLEAN DrawManagedVideoObjectOutlineShadowToSurface(
	UINT32 destination,
	HVOBJECT object,
	UINT16 frame,
	INT32 destinationX,
	INT32 destinationY)
{
	if (object->ubBitDepth == 8)
	{
		return Draw8BitVideoObjectOutlineToSurface(
			destination, object, frame,
			destinationX, destinationY,
			RenderImageOutlineMode::Shadow, 0, FALSE);
	}

	UINT32 pitch = 0;
	PIXEL* const buffer =
		reinterpret_cast<PIXEL*>(LockVideoSurface(destination, &pitch));
	if (!buffer) return FALSE;

	try
	{
		SixteenBPPObjectInfo& image = object->p16BPPObject[0];
		if (object->ubBitDepth == 16)
		{
			Blt16BPPTo16BPPTransShadow(
				buffer, pitch, image.p16BPPData,
				image.usWidth * sizeof(PIXEL),
				destinationX, destinationY,
				0, 0, image.usWidth, image.usHeight,
				PixFromColor16(0x1f));
		}
		else if (object->ubBitDepth == 32)
		{
			Blt32BPPTo16BPPTransShadow(
				buffer, pitch,
				reinterpret_cast<UINT32*>(image.p16BPPData),
				image.usWidth * sizeof(UINT32),
				destinationX, destinationY,
				0, 0, image.usWidth, image.usHeight);
		}
		else
		{
			UnLockVideoSurface(destination);
			return FALSE;
		}
	}
	catch (...)
	{
		UnLockVideoSurface(destination);
		throw;
	}
	UnLockVideoSurface(destination);
	return TRUE;
}
}

UINT32				guiVObjectIndex = 1;
UINT32				guiVObjectSize = 0;
UINT32				guiVObjectTotalAdded = 0;

#ifdef _DEBUG
enum
{
	DEBUGSTR_NONE,
	DEBUGSTR_SETVIDEOOBJECTTRANSPARENCY,
	DEBUGSTR_BLTVIDEOOBJECTFROMINDEX,
	DEBUGSTR_SETOBJECTHANDLESHADE,
	DEBUGSTR_GETVIDEOOBJECTETRLESUBREGIONPROPERTIES,
	DEBUGSTR_GETVIDEOOBJECTETRLEPROPERTIESFROMINDEX,
	DEBUGSTR_SETVIDEOOBJECTPALETTE8BPP,
	DEBUGSTR_GETVIDEOOBJECTPALETTE16BPP,
	DEBUGSTR_COPYVIDEOOBJECTPALETTE16BPP,
	DEBUGSTR_BLTVIDEOOBJECTOUTLINEFROMINDEX,
	DEBUGSTR_BLTVIDEOOBJECTOUTLINESHADOWFROMINDEX,
	DEBUGSTR_DELETEVIDEOOBJECTFROMINDEX
};

UINT8 gubVODebugCode = 0;

void CheckValidVObjectIndex( UINT32 uiIndex );
#endif

// **************************************************************
//
// Video Object Manager functions
//
// **************************************************************

#ifdef _MSC_VER
int filter(unsigned int code, struct _EXCEPTION_POINTERS *ep)
{
	puts("in filter.");
	if (code == EXCEPTION_ACCESS_VIOLATION) {
		puts("caught AV as expected.");
		return EXCEPTION_EXECUTE_HANDLER;
	}

	else
	{
		puts("didn't catch AV, unexpected.");
		return EXCEPTION_CONTINUE_SEARCH;
	};
}
#endif

BOOLEAN InitializeVideoObjectManager( )
{
	if (gfVideoObjectsInit) return TRUE;
	//Shouldn't be calling this if the video object manager already exists.
	//Call shutdown first...
	if (!gVideoObjects.empty() || !gVideoObjectHandles.empty()) return FALSE;
	RegisterDebugTopic(TOPIC_VIDEOOBJECT, "Video Object Manager");
	gfVideoObjectsInit=TRUE;
	return TRUE ;
}

BOOLEAN ShutdownVideoObjectManager( )
{
	gVideoObjectHandles.clear();
	gVideoObjects.clear();
	guiVObjectIndex = 1;
	guiVObjectSize = 0;
	guiVObjectTotalAdded = 0;
	if (gfVideoObjectsInit)
		UnRegisterDebugTopic(TOPIC_VIDEOOBJECT, "Video Object Manager");
	gfVideoObjectsInit=FALSE;
	return TRUE;
}

UINT32 CountVideoObjectNodes()
{
	return static_cast<UINT32>(gVideoObjects.size());
}

BOOLEAN AddStandardVideoObject( VOBJECT_DESC *pVObjectDesc, UINT32 *puiIndex )
{

	HVOBJECT hVObject;

	// Assertions
	Assert( puiIndex );
	Assert( pVObjectDesc );

	// Create video object
	hVObject = CreateVideoObject( pVObjectDesc );

	if( !hVObject )
	{
		// Video Object will set error condition.
		return FALSE ;
	}

	// Set transparency to default
	SetVideoObjectTransparencyColor( hVObject, FROMRGB( 0, 0, 0 ) );

	std::optional<UINT32> handle;
	try
	{
		handle = gVideoObjects.insert(VideoObjectResource(hVObject));
	}
	catch (...)
	{
		return FALSE;
	}
	if (!handle) return FALSE;
	try
	{
		const auto reverse =
			gVideoObjectHandles.emplace(hVObject, *handle);
		if (!reverse.second)
		{
			(void)gVideoObjects.erase(*handle);
			return FALSE;
		}
	}
	catch (...)
	{
		(void)gVideoObjects.erase(*handle);
		return FALSE;
	}

	*puiIndex = *handle;
	guiVObjectIndex = gVideoObjects.nextHandle();
	guiVObjectSize = static_cast<UINT32>(gVideoObjects.size());
	++guiVObjectTotalAdded;

	#ifdef JA2TESTVERSION
		if( CountVideoObjectNodes() != guiVObjectSize )
		{
			guiVObjectSize = guiVObjectSize;
		}
	#endif

	return TRUE ;
}


BOOLEAN SetVideoObjectTransparency( UINT32 uiIndex, COLORVAL TransColor )
{
	HVOBJECT hVObject;

	// Get video object
	#ifdef _DEBUG
		gubVODebugCode = DEBUGSTR_SETVIDEOOBJECTTRANSPARENCY;
	#endif
	CHECKF( GetVideoObject( &hVObject, uiIndex ) );

	// Set transparency
	SetVideoObjectTransparencyColor( hVObject, TransColor );

	return( TRUE );
}


BOOLEAN GetVideoObject( HVOBJECT *hVObject, UINT32 uiIndex )
{

	#ifdef _DEBUG
		CheckValidVObjectIndex( uiIndex );
	#endif

	VideoObjectResource* const resource = gVideoObjects.find(uiIndex);
	if (resource)
	{
		*hVObject = resource->object.get();
		return TRUE;
	}
	*hVObject = NULL;
	return FALSE;
}

namespace
{
bool PlatformVideoObjectDestinationEffect(
	const RenderImageDrawCommand& command,
	HVOBJECT source,
	bool intensity)
{
	if (!source || source->ubBitDepth != 8 ||
		command.frame >= source->usNumberOfObjects ||
		!source->pETRLEObject || !source->pPixData)
		return false;
	const UINT16 frame = static_cast<UINT16>(command.frame);
	const ETRLEObject& image = source->pETRLEObject[frame];
	if (image.usWidth == 0 || image.usHeight == 0 ||
		image.uiDataOffset >= source->uiSizePixData)
		return false;

	PlatformSurfaceMappingLease destination(command.destination);
	if (!destination) return false;
	const MutableRenderSurface& mapping = destination.mapping();
	if (!IsValidRenderSurfaceMapping(mapping) ||
		mapping.description.contentBitDepth != 16 ||
		RenderPixelBytes(mapping.description.format) != sizeof(PIXEL) ||
		mapping.pitchBytes > std::numeric_limits<UINT32>::max() ||
		mapping.description.width >
			static_cast<std::uint32_t>(
				std::numeric_limits<INT32>::max()) ||
		mapping.description.height >
			static_cast<std::uint32_t>(
				std::numeric_limits<INT32>::max()))
		return false;

	const std::int64_t clipLeft = std::max<std::int64_t>(
		0, command.clippingRegion.left);
	const std::int64_t clipTop = std::max<std::int64_t>(
		0, command.clippingRegion.top);
	const std::int64_t clipRight = std::min<std::int64_t>(
		mapping.description.width, command.clippingRegion.right);
	const std::int64_t clipBottom = std::min<std::int64_t>(
		mapping.description.height, command.clippingRegion.bottom);
	if (clipLeft >= clipRight || clipTop >= clipBottom) return true;

	const std::int64_t imageLeft =
		static_cast<std::int64_t>(command.destinationOrigin.x) +
		image.sOffsetX;
	const std::int64_t imageTop =
		static_cast<std::int64_t>(command.destinationOrigin.y) +
		image.sOffsetY;
	const std::int64_t imageRight = imageLeft + image.usWidth;
	const std::int64_t imageBottom = imageTop + image.usHeight;
	if (imageLeft >= clipRight || imageTop >= clipBottom ||
		imageRight <= clipLeft || imageBottom <= clipTop)
		return true;
	if (imageLeft < std::numeric_limits<INT32>::min() ||
		imageTop < std::numeric_limits<INT32>::min() ||
		imageRight > std::numeric_limits<INT32>::max() ||
		imageBottom > std::numeric_limits<INT32>::max())
		return false;

	PIXEL* const pixels = reinterpret_cast<PIXEL*>(mapping.pixels);
	const UINT32 pitch = static_cast<UINT32>(mapping.pitchBytes);
	const bool fullyInside =
		imageLeft >= clipLeft && imageTop >= clipTop &&
		imageRight <= clipRight && imageBottom <= clipBottom;
	if (fullyInside)
	{
		return intensity ?
			Blt8BPPDataTo16BPPBufferIntensity(
				pixels, pitch, source,
				command.destinationOrigin.x,
				command.destinationOrigin.y, frame) != FALSE :
			Blt8BPPDataTo16BPPBufferShadow(
				pixels, pitch, source,
				command.destinationOrigin.x,
				command.destinationOrigin.y, frame) != FALSE;
	}

	SGPRect clipping{
		static_cast<INT32>(clipLeft),
		static_cast<INT32>(clipTop),
		static_cast<INT32>(clipRight),
		static_cast<INT32>(clipBottom)};
	return intensity ?
		Blt8BPPDataTo16BPPBufferIntensityClip(
			pixels, pitch, source,
			command.destinationOrigin.x,
			command.destinationOrigin.y, frame, &clipping) != FALSE :
		Blt8BPPDataTo16BPPBufferShadowClip(
			pixels, pitch, source,
			command.destinationOrigin.x,
			command.destinationOrigin.y, frame, &clipping) != FALSE;
}
}

bool PlatformVideoObjectDraw(const RenderImageDrawCommand& command)
{
	if (command.destination == 0 || command.image == 0 ||
		command.destination > std::numeric_limits<UINT32>::max() ||
		command.frame > std::numeric_limits<UINT16>::max())
		return false;
	HVOBJECT const source = ResolveRenderImage(command.image);
	if (!source) return false;

	switch (command.mode)
	{
	case RenderImageCompositeMode::Shadow:
		return PlatformVideoObjectDestinationEffect(
			command, source, false);
	case RenderImageCompositeMode::Intensity:
		return PlatformVideoObjectDestinationEffect(
			command, source, true);
	case RenderImageCompositeMode::Opaque:
	case RenderImageCompositeMode::SourceTransparency:
		break;
	default:
		return false;
	}
	const std::optional<UINT32> flags = LegacyFlagsFor(command.mode);
	if (!flags) return false;

	ClippingRegionLease clipping(command.clippingRegion);
	return DrawVideoObjectToSurface(
		static_cast<UINT32>(command.destination),
		source,
		static_cast<UINT16>(command.frame),
		command.destinationOrigin.x,
		command.destinationOrigin.y,
		*flags,
		nullptr) != FALSE;
}

bool PlatformVideoObjectDepthDraw(
	const RenderImageDepthDrawCommand& command)
{
	if (command.destination == 0 || command.depthSurface == 0 ||
		command.image == 0 ||
		command.destination > std::numeric_limits<UINT32>::max() ||
		command.depthSurface > std::numeric_limits<UINT32>::max() ||
		command.frame > std::numeric_limits<UINT16>::max())
		return false;
	switch (command.effect)
	{
	case RenderImageDepthEffect::SourcePalette:
		if (command.comparison !=
			RenderDepthCompareMode::GreaterOrEqual)
			return false;
		break;
	case RenderImageDepthEffect::ShadeDestination:
	case RenderImageDepthEffect::IntensifyDestination:
		if (command.comparison != RenderDepthCompareMode::Greater)
			return false;
		break;
	default:
		return false;
	}
	switch (command.depthWrite)
	{
	case RenderDepthWriteMode::Preserve:
	case RenderDepthWriteMode::ReplaceOnPass:
		break;
	default:
		return false;
	}

	HVOBJECT const source = ResolveRenderImage(command.image);
	if (!source || source->ubBitDepth != 8 ||
		command.frame >= source->usNumberOfObjects ||
		!source->pETRLEObject || !source->pPixData ||
		(command.effect == RenderImageDepthEffect::SourcePalette &&
			!source->pShadeCurrent))
		return false;
	const UINT16 frame = static_cast<UINT16>(command.frame);
	const ETRLEObject& image = source->pETRLEObject[frame];
	if (image.usWidth == 0 || image.usHeight == 0 ||
		image.uiDataOffset >= source->uiSizePixData)
		return false;

	PlatformSurfaceMappingLease destination(command.destination);
	if (!destination) return false;
	PlatformSurfaceMappingLease depth(command.depthSurface);
	if (!depth) return false;
	const MutableRenderSurface& destinationMapping =
		destination.mapping();
	const MutableRenderSurface& depthMapping = depth.mapping();
	if (!IsValidRenderSurfaceMapping(destinationMapping) ||
		!IsValidRenderSurfaceMapping(depthMapping) ||
		destinationMapping.description.contentBitDepth != 16 ||
		RenderPixelBytes(destinationMapping.description.format) !=
			sizeof(PIXEL) ||
		depthMapping.description.format != RenderPixelFormat::Depth16 ||
		depthMapping.description.contentBitDepth != 16 ||
		destinationMapping.description.width !=
			depthMapping.description.width ||
		destinationMapping.description.height !=
			depthMapping.description.height ||
		destinationMapping.pitchBytes != depthMapping.pitchBytes ||
		destinationMapping.pitchBytes >
			std::numeric_limits<UINT32>::max() ||
		destinationMapping.description.width >
			static_cast<std::uint32_t>(
				std::numeric_limits<INT32>::max()) ||
		destinationMapping.description.height >
			static_cast<std::uint32_t>(
				std::numeric_limits<INT32>::max()))
		return false;

	const std::int64_t clipLeft = std::max<std::int64_t>(
		0, command.clippingRegion.left);
	const std::int64_t clipTop = std::max<std::int64_t>(
		0, command.clippingRegion.top);
	const std::int64_t clipRight = std::min<std::int64_t>(
		destinationMapping.description.width,
		command.clippingRegion.right);
	const std::int64_t clipBottom = std::min<std::int64_t>(
		destinationMapping.description.height,
		command.clippingRegion.bottom);
	if (clipLeft >= clipRight || clipTop >= clipBottom) return true;

	const std::int64_t imageLeft =
		static_cast<std::int64_t>(command.destinationOrigin.x) +
		image.sOffsetX;
	const std::int64_t imageTop =
		static_cast<std::int64_t>(command.destinationOrigin.y) +
		image.sOffsetY;
	const std::int64_t imageRight = imageLeft + image.usWidth;
	const std::int64_t imageBottom = imageTop + image.usHeight;
	if (imageLeft >= clipRight || imageTop >= clipBottom ||
		imageRight <= clipLeft || imageBottom <= clipTop)
		return true;
	if (imageLeft < std::numeric_limits<INT32>::min() ||
		imageTop < std::numeric_limits<INT32>::min() ||
		imageRight > std::numeric_limits<INT32>::max() ||
		imageBottom > std::numeric_limits<INT32>::max())
		return false;

	SGPRect clipping{
		static_cast<INT32>(clipLeft),
		static_cast<INT32>(clipTop),
		static_cast<INT32>(clipRight),
		static_cast<INT32>(clipBottom)};
	PIXEL* const destinationPixels =
		reinterpret_cast<PIXEL*>(destinationMapping.pixels);
	UINT16* const depthPixels =
		reinterpret_cast<UINT16*>(depthMapping.pixels);
	const UINT32 pitch =
		static_cast<UINT32>(destinationMapping.pitchBytes);
	const bool replaceDepth =
		command.depthWrite == RenderDepthWriteMode::ReplaceOnPass;
	switch (command.effect)
	{
	case RenderImageDepthEffect::SourcePalette:
		return replaceDepth ?
			Blt8BPPDataTo16BPPBufferTransZClip(
				destinationPixels, pitch, depthPixels, command.depth,
				source, command.destinationOrigin.x,
				command.destinationOrigin.y, frame, &clipping) != FALSE :
			Blt8BPPDataTo16BPPBufferTransZNBClip(
				destinationPixels, pitch, depthPixels, command.depth,
				source, command.destinationOrigin.x,
				command.destinationOrigin.y, frame, &clipping) != FALSE;
	case RenderImageDepthEffect::ShadeDestination:
		return replaceDepth ?
			Blt8BPPDataTo16BPPBufferShadowZClip(
				destinationPixels, pitch, depthPixels, command.depth,
				source, command.destinationOrigin.x,
				command.destinationOrigin.y, frame, &clipping) != FALSE :
			Blt8BPPDataTo16BPPBufferShadowZNBClip(
				destinationPixels, pitch, depthPixels, command.depth,
				source, command.destinationOrigin.x,
				command.destinationOrigin.y, frame, &clipping) != FALSE;
	case RenderImageDepthEffect::IntensifyDestination:
		return replaceDepth ?
			Blt8BPPDataTo16BPPBufferIntensityZClip(
				destinationPixels, pitch, depthPixels, command.depth,
				source, command.destinationOrigin.x,
				command.destinationOrigin.y, frame, &clipping) != FALSE :
			Blt8BPPDataTo16BPPBufferIntensityZNBClip(
				destinationPixels, pitch, depthPixels, command.depth,
				source, command.destinationOrigin.x,
				command.destinationOrigin.y, frame, &clipping) != FALSE;
	default:
		return false;
	}
}

bool PlatformVideoObjectOutline(
	const RenderImageOutlineCommand& command)
{
	if (command.destination == 0 || command.image == 0 ||
		command.destination > std::numeric_limits<UINT32>::max() ||
		command.frame > std::numeric_limits<UINT16>::max())
		return false;

	HVOBJECT const source = ResolveRenderImage(command.image);
	if (!source) return false;

	const UINT16 frame = static_cast<UINT16>(command.frame);
	switch (command.mode)
	{
	case RenderImageOutlineMode::Color:
		if (source->ubBitDepth != 8 ||
			frame >= source->usNumberOfObjects ||
			!source->pETRLEObject)
			return false;
		break;
	case RenderImageOutlineMode::Shadow:
		if (source->ubBitDepth == 8)
		{
			if (frame >= source->usNumberOfObjects ||
				!source->pETRLEObject)
				return false;
		}
		else if (source->ubBitDepth == 16 ||
			source->ubBitDepth == 32)
		{
			if (frame >= source->usNumberOf16BPPObjects ||
				!source->p16BPPObject)
				return false;
		}
		else
		{
			return false;
		}
		break;
	default:
		return false;
	}

	ClippingRegionLease clipping(command.clippingRegion);
	if (command.mode == RenderImageOutlineMode::Color)
	{
		return Draw8BitVideoObjectOutlineToSurface(
			static_cast<UINT32>(command.destination),
			source,
			frame,
			command.destinationOrigin.x,
			command.destinationOrigin.y,
			command.mode,
			LegacyPixelFor(command.color),
			command.drawOutline ? TRUE : FALSE) != FALSE;
	}
	return DrawManagedVideoObjectOutlineShadowToSurface(
		static_cast<UINT32>(command.destination),
		source,
		frame,
		command.destinationOrigin.x,
		command.destinationOrigin.y) != FALSE;
}

bool PlatformVideoObjectDepthOutline(
	const RenderImageDepthOutlineCommand& command)
{
	if (command.destination == 0 || command.depthSurface == 0 ||
		command.image == 0 ||
		command.destination > std::numeric_limits<UINT32>::max() ||
		command.depthSurface > std::numeric_limits<UINT32>::max() ||
		command.frame > std::numeric_limits<UINT16>::max())
		return false;
	switch (command.depthWrite)
	{
	case RenderDepthWriteMode::Preserve:
	case RenderDepthWriteMode::ReplaceOnPass:
		break;
	default:
		return false;
	}
	switch (command.visibility)
	{
	case RenderImageDepthOutlineVisibility::VisibleOnly:
		if (command.comparison !=
			RenderDepthCompareMode::GreaterOrEqual)
			return false;
		break;
	case RenderImageDepthOutlineVisibility::PixelateWhenObscured:
		if ((command.comparison !=
				RenderDepthCompareMode::GreaterOrEqual &&
			 command.comparison != RenderDepthCompareMode::Greater) ||
			command.depthWrite != RenderDepthWriteMode::ReplaceOnPass)
			return false;
		break;
	default:
		return false;
	}

	HVOBJECT const source = ResolveRenderImage(command.image);
	if (!source || source->ubBitDepth != 8 ||
		command.frame >= source->usNumberOfObjects ||
		!source->pETRLEObject || !source->pPixData ||
		!source->pShadeCurrent)
		return false;
	const UINT16 frame = static_cast<UINT16>(command.frame);
	const ETRLEObject& image = source->pETRLEObject[frame];
	if (image.usWidth == 0 || image.usHeight == 0 ||
		image.uiDataOffset >= source->uiSizePixData)
		return false;

	PlatformSurfaceMappingLease destination(command.destination);
	if (!destination) return false;
	PlatformSurfaceMappingLease depth(command.depthSurface);
	if (!depth) return false;
	const MutableRenderSurface& destinationMapping =
		destination.mapping();
	const MutableRenderSurface& depthMapping = depth.mapping();
	if (!IsValidRenderSurfaceMapping(destinationMapping) ||
		!IsValidRenderSurfaceMapping(depthMapping) ||
		destinationMapping.description.contentBitDepth != 16 ||
		RenderPixelBytes(destinationMapping.description.format) !=
			sizeof(PIXEL) ||
		depthMapping.description.format != RenderPixelFormat::Depth16 ||
		depthMapping.description.contentBitDepth != 16 ||
		destinationMapping.description.width !=
			depthMapping.description.width ||
		destinationMapping.description.height !=
			depthMapping.description.height ||
		destinationMapping.pitchBytes != depthMapping.pitchBytes ||
		destinationMapping.pitchBytes >
			std::numeric_limits<UINT32>::max() ||
		destinationMapping.description.width >
			static_cast<std::uint32_t>(
				std::numeric_limits<INT32>::max()) ||
		destinationMapping.description.height >
			static_cast<std::uint32_t>(
				std::numeric_limits<INT32>::max()))
		return false;

	const std::int64_t clipLeft = std::max<std::int64_t>(
		0, command.clippingRegion.left);
	const std::int64_t clipTop = std::max<std::int64_t>(
		0, command.clippingRegion.top);
	const std::int64_t clipRight = std::min<std::int64_t>(
		destinationMapping.description.width,
		command.clippingRegion.right);
	const std::int64_t clipBottom = std::min<std::int64_t>(
		destinationMapping.description.height,
		command.clippingRegion.bottom);
	if (clipLeft >= clipRight || clipTop >= clipBottom) return true;

	const std::int64_t imageLeft =
		static_cast<std::int64_t>(command.destinationOrigin.x) +
		image.sOffsetX;
	const std::int64_t imageTop =
		static_cast<std::int64_t>(command.destinationOrigin.y) +
		image.sOffsetY;
	const std::int64_t imageRight = imageLeft + image.usWidth;
	const std::int64_t imageBottom = imageTop + image.usHeight;
	if (imageLeft >= clipRight || imageTop >= clipBottom ||
		imageRight <= clipLeft || imageBottom <= clipTop)
		return true;
	if (imageLeft < std::numeric_limits<INT32>::min() ||
		imageTop < std::numeric_limits<INT32>::min() ||
		imageRight > std::numeric_limits<INT32>::max() ||
		imageBottom > std::numeric_limits<INT32>::max())
		return false;

	const bool fullyInside =
		imageLeft >= clipLeft && imageTop >= clipTop &&
		imageRight <= clipRight && imageBottom <= clipBottom;
	SGPRect clipping{
		static_cast<INT32>(clipLeft),
		static_cast<INT32>(clipTop),
		static_cast<INT32>(clipRight),
		static_cast<INT32>(clipBottom)};
	PIXEL* const destinationPixels =
		reinterpret_cast<PIXEL*>(destinationMapping.pixels);
	UINT16* const depthPixels =
		reinterpret_cast<UINT16*>(depthMapping.pixels);
	const UINT32 pitch =
		static_cast<UINT32>(destinationMapping.pitchBytes);
	const PIXEL outlineColor = LegacyPixelFor(command.color);
	const BOOLEAN drawOutline = command.drawOutline ? TRUE : FALSE;

	switch (command.visibility)
	{
	case RenderImageDepthOutlineVisibility::VisibleOnly:
		if (command.depthWrite == RenderDepthWriteMode::Preserve)
		{
			if (!fullyInside) return false;
			return Blt8BPPDataTo16BPPBufferOutlineZNB(
				destinationPixels, pitch, depthPixels, command.depth,
				source, command.destinationOrigin.x,
				command.destinationOrigin.y, frame,
				outlineColor, drawOutline) != FALSE;
		}
		return fullyInside ?
			Blt8BPPDataTo16BPPBufferOutlineZ(
				destinationPixels, pitch, depthPixels, command.depth,
				source, command.destinationOrigin.x,
				command.destinationOrigin.y, frame,
				outlineColor, drawOutline) != FALSE :
			Blt8BPPDataTo16BPPBufferOutlineZClip(
				destinationPixels, pitch, depthPixels, command.depth,
				source, command.destinationOrigin.x,
				command.destinationOrigin.y, frame,
				outlineColor, drawOutline, &clipping) != FALSE;
	case RenderImageDepthOutlineVisibility::PixelateWhenObscured:
		if (command.comparison == RenderDepthCompareMode::Greater)
		{
			if (!fullyInside) return false;
			return Blt8BPPDataTo16BPPBufferOutlineZPixelateObscured(
				destinationPixels, pitch, depthPixels, command.depth,
				source, command.destinationOrigin.x,
				command.destinationOrigin.y, frame,
				outlineColor, drawOutline) != FALSE;
		}
		return Blt8BPPDataTo16BPPBufferOutlineZPixelateObscuredClip(
			destinationPixels, pitch, depthPixels, command.depth,
			source, command.destinationOrigin.x,
			command.destinationOrigin.y, frame,
			outlineColor, drawOutline, &clipping) != FALSE;
	default:
		return false;
	}
}

BOOLEAN BltVideoObjectFromIndex(UINT32 uiDestVSurface, UINT32 uiSrcVObject, UINT16 usRegionIndex, INT32 iDestX, INT32 iDestY, UINT32 fBltFlags, blt_fx *pBltFx )
{
	#ifdef _DEBUG
		gubVODebugCode = DEBUGSTR_BLTVIDEOOBJECTFROMINDEX;
	#endif
	HVOBJECT source = nullptr;
	if (!GetVideoObject(&source, uiSrcVObject)) return FALSE;
	(void)source;
	(void)pBltFx; // The established high-level blitter does not consume effects.
	return SubmitVideoObjectDraw(
		uiDestVSurface, uiSrcVObject, usRegionIndex,
		iDestX, iDestY, fBltFlags) ? TRUE : FALSE;
}


BOOLEAN DeleteVideoObjectFromIndex( UINT32 uiVObject	)
{
	#ifdef _DEBUG
		gubVODebugCode = DEBUGSTR_DELETEVIDEOOBJECTFROMINDEX;
		CheckValidVObjectIndex( uiVObject );
	#endif
	VideoObjectResource* const resource = gVideoObjects.find(uiVObject);
	if (!resource) return FALSE;
	gVideoObjectHandles.erase(resource->object.get());
	if (!gVideoObjects.erase(uiVObject)) return FALSE;
	guiVObjectSize = static_cast<UINT32>(gVideoObjects.size());
	return TRUE;
}



// Given an index to the dest and src vobject contained in ghVideoObjects
// Based on flags, blit accordingly
// There are two types, a BltFast and a Blt. BltFast is 10% faster, uses no
// clipping lists
BOOLEAN BltVideoObject(	UINT32	uiDestVSurface,
												HVOBJECT hSrcVObject,
												UINT16 usRegionIndex,
												INT32	iDestX,
												INT32	iDestY,
												UINT32 fBltFlags,
												blt_fx *pBltFx )
{

	UINT32 image = 0;
	if (FindVideoObjectHandle(hSrcVObject, image))
	{
		(void)pBltFx;
		return SubmitVideoObjectDraw(
			uiDestVSurface, image, usRegionIndex,
			iDestX, iDestY, fBltFlags) ? TRUE : FALSE;
	}

	// Button art, editor previews, and other application-owned images may not
	// belong to the stable manager yet. Keep those pointer-owned objects on the
	// exact compatibility implementation until their ownership is migrated.
	return DrawVideoObjectToSurface(
		uiDestVSurface, hSrcVObject, usRegionIndex,
		iDestX, iDestY, fBltFlags, pBltFx);
}

BOOLEAN BltVideoObject(UINT32 uiDestVSurface, HVOBJECT hSrcVObject, UINT16 usRegionIndex, SGPRectangle Region, UINT32 fBltFlags, blt_fx* pBltFx)
{
	return BltVideoObject(uiDestVSurface, hSrcVObject, usRegionIndex, Region.x, Region.y, fBltFlags, pBltFx);
}

BOOLEAN BltVideoObjectDepthToSurface(
	UINT32 uiDestVSurface,
	HVOBJECT hSrcVObject,
	UINT16 usRegionIndex,
	INT32 iDestX,
	INT32 iDestY,
	UINT16 usDepth,
	BOOLEAN fWriteDepth,
	const SGPRect* pClipRegion)
{
	return SubmitVideoObjectDepthDraw(
		uiDestVSurface, hSrcVObject, usRegionIndex,
		iDestX, iDestY, usDepth, fWriteDepth,
		RenderImageDepthEffect::SourcePalette, pClipRegion) ?
		TRUE : FALSE;
}

BOOLEAN BltVideoObjectEffectToSurface(
	UINT32 uiDestVSurface,
	HVOBJECT hSrcVObject,
	UINT16 usRegionIndex,
	INT32 iDestX,
	INT32 iDestY,
	VideoObjectDrawEffect effect,
	const SGPRect* pClipRegion)
{
	RenderImageCompositeMode mode;
	switch (effect)
	{
	case VOBJECT_DRAW_SOURCE_TRANSPARENCY:
		mode = RenderImageCompositeMode::SourceTransparency;
		break;
	case VOBJECT_DRAW_SHADE_DESTINATION:
		mode = RenderImageCompositeMode::Shadow;
		break;
	case VOBJECT_DRAW_INTENSIFY_DESTINATION:
		mode = RenderImageCompositeMode::Intensity;
		break;
	default:
		return FALSE;
	}
	return SubmitVideoObjectEffectDraw(
		uiDestVSurface, hSrcVObject, usRegionIndex,
		iDestX, iDestY, mode, pClipRegion) ? TRUE : FALSE;
}

BOOLEAN BltVideoObjectDepthMaskToSurface(
	UINT32 uiDestVSurface,
	HVOBJECT hSrcVObject,
	UINT16 usRegionIndex,
	INT32 iDestX,
	INT32 iDestY,
	UINT16 usDepth,
	BOOLEAN fWriteDepth,
	VideoObjectDepthMaskEffect effect,
	const SGPRect* pClipRegion)
{
	RenderImageDepthEffect commandEffect;
	switch (effect)
	{
	case VOBJECT_DEPTH_MASK_SHADOW:
		commandEffect = RenderImageDepthEffect::ShadeDestination;
		break;
	case VOBJECT_DEPTH_MASK_INTENSITY:
		commandEffect = RenderImageDepthEffect::IntensifyDestination;
		break;
	default:
		return FALSE;
	}
	return SubmitVideoObjectDepthDraw(
		uiDestVSurface, hSrcVObject, usRegionIndex,
		iDestX, iDestY, usDepth, fWriteDepth,
		commandEffect, pClipRegion) ? TRUE : FALSE;
}

BOOLEAN BltVideoObjectOutlineToSurface(
	UINT32 uiDestVSurface,
	HVOBJECT hSrcVObject,
	UINT16 usRegionIndex,
	INT32 iDestX,
	INT32 iDestY,
	VideoObjectOutlineEffect effect,
	PIXEL usOutlineColor,
	BOOLEAN fDrawOutline,
	const SGPRect* pClipRegion)
{
	RenderImageOutlineMode mode;
	switch (effect)
	{
	case VOBJECT_OUTLINE_COLOR:
		mode = RenderImageOutlineMode::Color;
		break;
	case VOBJECT_OUTLINE_SHADE_DESTINATION:
		mode = RenderImageOutlineMode::Shadow;
		break;
	default:
		return FALSE;
	}
	return SubmitVideoObjectOutlineDraw(
		uiDestVSurface, hSrcVObject, usRegionIndex,
		iDestX, iDestY, mode, usOutlineColor,
		fDrawOutline, pClipRegion) ? TRUE : FALSE;
}

BOOLEAN BltVideoObjectDepthOutlineToSurface(
	UINT32 uiDestVSurface,
	HVOBJECT hSrcVObject,
	UINT16 usRegionIndex,
	INT32 iDestX,
	INT32 iDestY,
	UINT16 usDepth,
	BOOLEAN fWriteDepth,
	VideoObjectDepthComparison comparison,
	VideoObjectDepthOutlineVisibility visibility,
	PIXEL usOutlineColor,
	BOOLEAN fDrawOutline,
	const SGPRect* pClipRegion)
{
	RenderDepthCompareMode commandComparison;
	switch (comparison)
	{
	case VOBJECT_DEPTH_GREATER_OR_EQUAL:
		commandComparison = RenderDepthCompareMode::GreaterOrEqual;
		break;
	case VOBJECT_DEPTH_GREATER:
		commandComparison = RenderDepthCompareMode::Greater;
		break;
	default:
		return FALSE;
	}
	RenderImageDepthOutlineVisibility commandVisibility;
	switch (visibility)
	{
	case VOBJECT_DEPTH_OUTLINE_VISIBLE_ONLY:
		commandVisibility =
			RenderImageDepthOutlineVisibility::VisibleOnly;
		break;
	case VOBJECT_DEPTH_OUTLINE_PIXELATE_WHEN_OBSCURED:
		commandVisibility =
			RenderImageDepthOutlineVisibility::PixelateWhenObscured;
		break;
	default:
		return FALSE;
	}
	return SubmitVideoObjectDepthOutlineDraw(
		uiDestVSurface, hSrcVObject, usRegionIndex,
		iDestX, iDestY, usDepth, fWriteDepth,
		commandComparison, commandVisibility,
		usOutlineColor, fDrawOutline, pClipRegion) ? TRUE : FALSE;
}
// *******************************************************************************
// Video Object Manipulation Functions
// *******************************************************************************


HVOBJECT CreateVideoObject( VOBJECT_DESC *VObjectDesc )
{
	HVOBJECT						hVObject;
	HIMAGE							hImage;
	ETRLEData						TempETRLEData;
//	UINT32							count;

	// Allocate memory for video object data and initialize
	hVObject = (HVOBJECT) MemAlloc( sizeof( SGPVObject ) );
	CHECKF( hVObject != NULL );
	memset( hVObject, 0, sizeof( SGPVObject ) );

	// default of all members of the vobject is 0

	// Check creation options
//	do
//	{
		if ( VObjectDesc->fCreateFlags & VOBJECT_CREATE_FROMFILE || VObjectDesc->fCreateFlags & VOBJECT_CREATE_FROMHIMAGE )
		{
			if ( VObjectDesc->fCreateFlags & VOBJECT_CREATE_FROMFILE )
			{
				ImageFileType::TestOrder order = ImageFileType::JPC_FALLBACK;

				if(VObjectDesc->fCreateFlags & VOBJECT_CREATE_FROMJPC) {
					order = ImageFileType::JPC;
				} else if(VObjectDesc->fCreateFlags & VOBJECT_CREATE_FROMJPC_FALLBACK) {
					order = ImageFileType::JPC_FALLBACK;
				} else if(VObjectDesc->fCreateFlags & VOBJECT_CREATE_FROMPNG) {
					order = ImageFileType::PNG_FALLBACK;
				} else if(VObjectDesc->fCreateFlags & VOBJECT_CREATE_FROMPNG_FALLBACK) {
					order = ImageFileType::PNG_FALLBACK;
				}
				// Create himage object from file
				hImage = CreateImage( VObjectDesc->ImageFile, IMAGE_ALLIMAGEDATA, order );

				if ( hImage == NULL )
				{
						MemFree( hVObject );
						DbgMessage( TOPIC_VIDEOOBJECT, DBG_LEVEL_2, "Invalid Image Filename given" );
						return( NULL );
				}
			}
			else
			{ // create video object from provided hImage
				hImage = VObjectDesc->hImage;
				if ( hImage == NULL )
				{
						MemFree( hVObject );
						DbgMessage( TOPIC_VIDEOOBJECT, DBG_LEVEL_2, "Invalid hImage pointer given" );
						return( NULL );
				}
			}

			if(hImage->ubBitDepth == 32)
			{
				SGP_THROW_IFFALSE(hImage->usNumberOfObjects > 0, L"bad himage");
				
				// create one 16bpp object (that contains 32bpp data)
				hVObject->p16BPPObject = (SixteenBPPObjectInfo*)MemAlloc(sizeof(SixteenBPPObjectInfo));
				if(!hVObject->p16BPPObject)
				{
					SGP_THROW(L"bad alloc");
				}
				memset(hVObject->p16BPPObject, 0, sizeof(SixteenBPPObjectInfo));

				int SIZE = hImage->pETRLEObject[0].usHeight * hImage->pETRLEObject[0].usWidth * sizeof(UINT32);
				hVObject->p16BPPObject->p16BPPData = (PIXEL *)MemAlloc(SIZE); // UINT32*
				if(!hVObject->p16BPPObject->p16BPPData)
				{
					MemFree(hVObject->p16BPPObject);
					SGP_THROW(L"bad alloc");
				}
				memcpy(hVObject->p16BPPObject->p16BPPData, hImage->p32BPPData, SIZE);

				hVObject->p16BPPObject->sOffsetX		= hImage->pETRLEObject[0].sOffsetX;
				hVObject->p16BPPObject->sOffsetY		= hImage->pETRLEObject[0].sOffsetY;
				hVObject->p16BPPObject->ubShadeLevel	= 0;
				hVObject->p16BPPObject->usHeight		= hImage->pETRLEObject[0].usHeight;
				hVObject->p16BPPObject->usWidth			= hImage->pETRLEObject[0].usWidth;
				hVObject->p16BPPObject->usRegionIndex	= 0;

				hVObject->usNumberOf16BPPObjects = 1;
				hVObject->ubBitDepth = hImage->ubBitDepth;
				strncpy(hVObject->ImageFile, hImage->ImageFile, SGPFILENAME_LEN);

				if ( VObjectDesc->fCreateFlags & VOBJECT_CREATE_FROMFILE )
				{
					DestroyImage( hImage );
				}

				return FinishVideoObjectCreation(hVObject);
			}
			else if(hImage->ubBitDepth == 16)
			{
				SGP_THROW_IFFALSE(hImage->usNumberOfObjects > 0, L"bad himage");

				// create one 16bpp object (that contains 32bpp data)
				hVObject->p16BPPObject = (SixteenBPPObjectInfo*)MemAlloc(sizeof(SixteenBPPObjectInfo));
				if(!hVObject->p16BPPObject)
				{
					SGP_THROW(L"bad alloc");
				}
				memset(hVObject->p16BPPObject, 0, sizeof(SixteenBPPObjectInfo));

				const UINT32 PIXELS = (UINT32)hImage->pETRLEObject[0].usHeight * hImage->pETRLEObject[0].usWidth;
				UINT32 SIZE = PIXELS * sizeof(PIXEL);
				hVObject->p16BPPObject->p16BPPData = (PIXEL *)MemAlloc(SIZE);
				if(!hVObject->p16BPPObject->p16BPPData)
				{
					MemFree(hVObject->p16BPPObject);
					SGP_THROW(L"bad alloc");
				}
				// p16BPPData is a native PIXEL (ARGB8888) buffer that the 32bpp
				// blitters read 4 bytes at a time. The source is 2-byte RGB565, so
				// expand it once here instead of a raw memcpy -- a byte copy would
				// both under-allocate (sizeof(UINT16)) and feed packed 565 to code
				// that reads 32-bit pixels (heap over-read + scrambled colour).
				for (UINT32 i = 0; i < PIXELS; ++i)
					hVObject->p16BPPObject->p16BPPData[i] = PixFromColor16(hImage->p16BPPData[i]);

				hVObject->p16BPPObject->sOffsetX		= hImage->pETRLEObject[0].sOffsetX;
				hVObject->p16BPPObject->sOffsetY		= hImage->pETRLEObject[0].sOffsetY;
				hVObject->p16BPPObject->ubShadeLevel	= 0;
				hVObject->p16BPPObject->usHeight		= hImage->pETRLEObject[0].usHeight;
				hVObject->p16BPPObject->usWidth			= hImage->pETRLEObject[0].usWidth;
				hVObject->p16BPPObject->usRegionIndex	= 0;

				hVObject->usNumberOf16BPPObjects = 1;
				hVObject->ubBitDepth = hImage->ubBitDepth;
				strncpy(hVObject->ImageFile, hImage->ImageFile, SGPFILENAME_LEN);

				if ( VObjectDesc->fCreateFlags & VOBJECT_CREATE_FROMFILE )
				{
					DestroyImage( hImage );
				}

				return FinishVideoObjectCreation(hVObject);
			}

			// Check if returned himage is TRLE compressed - return error if not
			if ( ! (hImage->fFlags & IMAGE_TRLECOMPRESSED ) )
			{
					MemFree( hVObject );
					DbgMessage( TOPIC_VIDEOOBJECT, DBG_LEVEL_2, "Invalid Image format given." );
					if (VObjectDesc->fCreateFlags & VOBJECT_CREATE_FROMFILE)
						DestroyImage(hImage);
					return( NULL );
			}

			// Set values from himage
			hVObject->ubBitDepth				= hImage->ubBitDepth;

			// Get TRLE data
			if ( !GetETRLEImageData( hImage, &TempETRLEData ) )
			{
				MemFree( hVObject );
				if (VObjectDesc->fCreateFlags & VOBJECT_CREATE_FROMFILE)
					DestroyImage(hImage);
				return( NULL );
			}

			// Set values
			hVObject->usNumberOfObjects	= TempETRLEData.usNumberOfObjects;
			hVObject->pETRLEObject		= TempETRLEData.pETRLEObject;
			hVObject->pPixData			= TempETRLEData.pPixData;
			hVObject->uiSizePixData		= TempETRLEData.uiSizePixData;
			strncpy(hVObject->ImageFile, hImage->ImageFile, SGPFILENAME_LEN);

			// Set palette from himage
			if ( hImage->ubBitDepth == 8 )
			{
				hVObject->pShade8=ubColorTables[DEFAULT_SHADE_LEVEL];
				hVObject->pGlow8=ubColorTables[0];

				SetVideoObjectPalette( hVObject, hImage->pPalette );

			}

			if ( VObjectDesc->fCreateFlags & VOBJECT_CREATE_FROMFILE )
			{
				// Delete himage object
				DestroyImage( hImage );
			}
	//		break;
		}
		else
		{
			MemFree( hVObject );
			DbgMessage( TOPIC_VIDEOOBJECT, DBG_LEVEL_2, "Invalid VObject creation flags given." );
			return( NULL );
		}

		// If here, no special options given, use structure given in paraneters
		// TO DO:

//	}
//	while( FALSE );

	// All is well
//	DbgMessage( TOPIC_VIDEOOBJECT, DBG_LEVEL_3, String("Success in Creating Video Object" ) );

	return FinishVideoObjectCreation(hVObject);
}


// Palette setting is expensive, need to set both DDPalette and create 16BPP palette
BOOLEAN SetVideoObjectPalette( HVOBJECT hVObject, SGPPaletteEntry *pSrcPalette )
{

	Assert( hVObject != NULL );
	Assert( pSrcPalette != NULL );

	// Create palette object if not already done so
	if ( hVObject->pPaletteEntry == NULL )
	{
		// Create palette
		hVObject->pPaletteEntry = (SGPPaletteEntry *) MemAlloc( sizeof( SGPPaletteEntry ) * 256 );
		CHECKF( hVObject->pPaletteEntry != NULL );

		// Copy src into palette
		memcpy( hVObject->pPaletteEntry, pSrcPalette, sizeof( SGPPaletteEntry ) * 256 );

	}
	else
	{
		// Just Change entries
		memcpy( hVObject->pPaletteEntry, pSrcPalette, sizeof( SGPPaletteEntry ) * 256 );
	}

	// Delete 16BPP Palette if one exists
	if ( hVObject->p16BPPPalette != NULL )
	{
		MemFree( hVObject->p16BPPPalette );
		hVObject->p16BPPPalette = NULL;
	}

	// Create 16BPP Palette
	hVObject->p16BPPPalette = Create16BPPPalette( pSrcPalette );
	hVObject->pShadeCurrent = hVObject->p16BPPPalette;

//	DbgMessage(TOPIC_VIDEOOBJECT, DBG_LEVEL_3, String("Video Object Palette change successfull" ));
	return( TRUE );
}

// Transparency needs to take RGB value and find best fit and place it into DD Surface
// colorkey value.
BOOLEAN SetVideoObjectTransparencyColor( HVOBJECT hVObject, COLORVAL TransColor )
{

	// Assertions
	Assert( hVObject != NULL );

	//Set trans color into video object
	hVObject->TransparentColor = TransColor;

	return( TRUE );
}


// Deletes all palettes, surfaces and region data
BOOLEAN DeleteVideoObject( HVOBJECT hVObject )
{
	UINT16			usLoop;

	// Assertions
	CHECKF( hVObject != NULL );

	// Retire the opaque renderer identity before releasing any backing memory,
	// so a stale command can never resolve a partially destroyed image.
	UnregisterRenderImage(hVObject);

	DestroyObjectPaletteTables(hVObject);

	// Release palette
	if ( hVObject->pPaletteEntry != NULL )
	{
		MemFree( hVObject->pPaletteEntry );
//		hVObject->pPaletteEntry = NULL;
	}


	if ( hVObject->pPixData != NULL )
	{
		MemFree( hVObject->pPixData );
//		hVObject->pPixData = NULL;
	}

	if ( hVObject->pETRLEObject != NULL )
	{
		MemFree( hVObject->pETRLEObject );
//		hVObject->pETRLEObject = NULL;
	}

	if ( hVObject->ppZStripInfo != NULL )
	{
		for (usLoop = 0; usLoop < hVObject->usNumberOfObjects; usLoop++)
		{
			if (hVObject->ppZStripInfo[usLoop] != NULL)
			{
				MemFree( hVObject->ppZStripInfo[usLoop]->pbZChange );
				MemFree( hVObject->ppZStripInfo[usLoop] );
			}
		}
		MemFree( hVObject->ppZStripInfo );
//		hVObject->ppZStripInfo = NULL;
	}

	if ( hVObject->usNumberOf16BPPObjects > 0)
	{
		for( usLoop = 0; usLoop < hVObject->usNumberOf16BPPObjects; usLoop++)
		{
			MemFree( hVObject->p16BPPObject[usLoop].p16BPPData );
		}
		MemFree( hVObject->p16BPPObject );
	}

	// Release object
	MemFree( hVObject );

	return( TRUE );
}

/**********************************************************************************************
 CreateObjectPaletteTables

		Creates the shading tables for 8-bit brushes. One highlight table is created, based on
	the object-type, 3 brightening tables, 1 normal, and 11 darkening tables. The entries are
	created iteratively, rather than in a loop to allow hand-tweaking of the values. If you
	change the HVOBJECT_SHADE_TABLES symbol, remember to add/delete entries here, it won't
	adjust automagically.

**********************************************************************************************/

UINT16 CreateObjectPaletteTables(HVOBJECT pObj, UINT32 uiType)
{
UINT32 count;

		// this creates the highlight table. Specify the glow-type when creating the tables
		// through uiType, symbols are from VOBJECT.H
	for( count = 0; count < 16; count++ )
	{
		if ( (count == 4) && (pObj->p16BPPPalette == pObj->pShades[ count ]) )
			pObj->pShades[ count ] = NULL;
		else if ( pObj->pShades[ count ] != NULL )
		{
			MemFree( pObj->pShades[ count ] );
			pObj->pShades[ count ] = NULL;
		}
	}

		switch(uiType)
		{
			case HVOBJECT_GLOW_GREEN:	// green glow
				pObj->pShades[0]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 0, 255, 0, TRUE);
				break;
			case HVOBJECT_GLOW_BLUE:	// blue glow
				pObj->pShades[0]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 0, 0, 255, TRUE);
				break;
			case HVOBJECT_GLOW_YELLOW:	// yellow glow
				pObj->pShades[0]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 255, 255, 0, TRUE);
				break;
			case HVOBJECT_GLOW_RED:	// red glow
				pObj->pShades[0]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 255, 0, 0, TRUE);
				break;
		}

		// these are the brightening tables, 115%-150% brighter than original
		pObj->pShades[1]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 293, 293, 293, FALSE);
		pObj->pShades[2]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 281, 281, 281, FALSE);
		pObj->pShades[3]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 268, 268, 268, FALSE);

		// palette 4 is the non-modified palette.
		// if the standard one has already been made, we'll use it
		if(pObj->p16BPPPalette!=NULL)
			pObj->pShades[4]=pObj->p16BPPPalette;
		else
		{
			// or create our own, and assign it to the standard one
			pObj->pShades[4]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 255, 255, 255, FALSE);
			pObj->p16BPPPalette=pObj->pShades[4];
		}

		// the rest are darkening tables, right down to all-black.
		pObj->pShades[5]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 195, 195, 195, FALSE);
		pObj->pShades[6]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 165, 165, 165, FALSE);
		pObj->pShades[7]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 135, 135, 135, FALSE);
		pObj->pShades[8]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 105, 105, 105, FALSE);
		pObj->pShades[9]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 75, 75, 75, FALSE);
		pObj->pShades[10]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 45, 45, 45, FALSE);
		pObj->pShades[11]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 36, 36, 36, FALSE);
		pObj->pShades[12]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 27, 27, 27, FALSE);
		pObj->pShades[13]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 18, 18, 18, FALSE);
		pObj->pShades[14]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 9, 9, 9, FALSE);
		pObj->pShades[15]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 0, 0, 0, FALSE);

		// Set current shade table to neutral color
		pObj->pShadeCurrent=pObj->pShades[4];

		// check to make sure every table got a palette
		for(count=0; (count < HVOBJECT_SHADE_TABLES) && (pObj->pShades[count]!=NULL); count++);


		// return the result of the check
		return(count==HVOBJECT_SHADE_TABLES);
}

// *******************************************************************
//
// Blitting Functions
//
// *******************************************************************

// High level blit function encapsulates ALL effects and BPP
BOOLEAN BltVideoObjectToBuffer( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, UINT16 usIndex, INT32 iDestX, INT32 iDestY, INT32 fBltFlags, blt_fx *pBltFx )
{
	CHAR8 errorText[512];
	// Sometimes an exception is thrown in that method.
//BF	__try
	{
	// Assertions
	//Assert( pBuffer != NULL );
	SGP_THROW_IFFALSE( pBuffer != NULL, L"No Destination Buffer" );

	if ( hSrcVObject == NULL )
	{
		// Missing graphic (e.g. an AIM-page sprite that failed to load): skip the
		// blit instead of raising the fatal error screen and locking the laptop.
		return( FALSE );
	}

	SixteenBPPObjectInfo *image = NULL;
	// Check For Flags and bit depths
	switch( hSrcVObject->ubBitDepth )
	{
			case 32:
				if ( !(usIndex < hSrcVObject->usNumberOf16BPPObjects) )
				{
					sprintf(errorText, "Video object index is larger than the number of images. Filename: %s", hSrcVObject->ImageFile);
					SGP_THROW(errorText);
				}
				image = &hSrcVObject->p16BPPObject[usIndex];
				Blt32BPPTo16BPPTrans( pBuffer, uiDestPitchBYTES, 
					(UINT32*)image->p16BPPData, image->usWidth * sizeof(UINT32),
					iDestX, iDestY, 
					0, 0, image->usWidth, image->usHeight); 
				break;

			case 16:
				if ( !(usIndex < hSrcVObject->usNumberOf16BPPObjects) )
				{
					sprintf(errorText, "Video object index is larger than the number of images. Filename: %s", hSrcVObject->ImageFile);
					SGP_THROW(errorText);
				}
				image = &hSrcVObject->p16BPPObject[usIndex];
				if ( fBltFlags & VO_BLT_SRCTRANSPARENCY	)
				{
					// p16BPPData is now native ARGB8888 (expanded at load), so the
					// source pitch is sizeof(PIXEL) and the colour key must be the
					// expanded form of the original RGB565 transparent value (0x1F).
					Blt16BPPTo16BPPTrans( pBuffer, uiDestPitchBYTES,
						image->p16BPPData, image->usWidth * sizeof(PIXEL),
						iDestX, iDestY,
						0, 0, image->usWidth, image->usHeight, PixFromColor16(0x1F) ); // 0x1f = 5 bits of blue
				}
				else
				{
					Blt16BPPTo16BPP( pBuffer, uiDestPitchBYTES,
						image->p16BPPData, image->usWidth * sizeof(PIXEL),
						iDestX, iDestY,
						0, 0, image->usWidth, image->usHeight );
				}

				break;

			case 8:
				if ( !(hSrcVObject->usNumberOfObjects > usIndex) )
				{
					sprintf(errorText, "Video object index is larger than the number of sub images. Filename: %s", hSrcVObject->ImageFile);
					SGP_THROW(errorText);
				}
				// Switch based on flags given
				do
				{
					if ( fBltFlags & VO_BLT_SRCTRANSPARENCY	)
					{
						if(BltIsClipped(hSrcVObject, iDestX, iDestY, usIndex, &ClippingRect))
							Blt8BPPDataTo16BPPBufferTransparentClip( pBuffer, uiDestPitchBYTES, hSrcVObject, iDestX, iDestY, usIndex, &ClippingRect);
						else
							Blt8BPPDataTo16BPPBufferTransparent( pBuffer, uiDestPitchBYTES, hSrcVObject, iDestX, iDestY, usIndex );
						break;
					}
					else if ( fBltFlags & VO_BLT_SHADOW	)
					{
						if(BltIsClipped(hSrcVObject, iDestX, iDestY, usIndex, &ClippingRect))
							Blt8BPPDataTo16BPPBufferShadowClip( pBuffer, uiDestPitchBYTES, hSrcVObject, iDestX, iDestY, usIndex, &ClippingRect);
						else
							Blt8BPPDataTo16BPPBufferShadow( pBuffer, uiDestPitchBYTES, hSrcVObject, iDestX, iDestY, usIndex );
						break;
					}

					// Use default blitter here
					//Blt8BPPDataTo16BPPBuffer( hDestVObject, hSrcVObject, (UINT16)iDestX, (UINT16)iDestY, (SGPRect*)&SrcRect );

				} while( FALSE );

				break;
	}

	return( TRUE );
	}
	//BF
	//__except(filter(GetExceptionCode(), GetExceptionInformation()))
	//{
	//	return ( TRUE );
	//}
}

BOOLEAN PixelateVideoObjectRect(	UINT32	uiDestVSurface, INT32 X1, INT32 Y1, INT32 X2, INT32 Y2)
{
	PIXEL *pBuffer;
	UINT32 uiPitch;
	SGPRect	area;
	UINT8 uiPattern[8][8]={	{0,1,0,1,0,1,0,1},
													{1,0,1,0,1,0,1,0},
													{0,1,0,1,0,1,0,1},
													{1,0,1,0,1,0,1,0},
													{0,1,0,1,0,1,0,1},
													{1,0,1,0,1,0,1,0},
													{0,1,0,1,0,1,0,1},
													{1,0,1,0,1,0,1,0}};

	// Lock video surface
	pBuffer = (PIXEL *)LockVideoSurface( uiDestVSurface, &uiPitch );

	if ( pBuffer == NULL )
	{
		return( FALSE );
	}

	area.iTop=Y1;
	area.iBottom=Y2;
	area.iLeft=X1;
	area.iRight=X2;

	// Now we have the video object and surface, call the shadow function
	if(!Blt16BPPBufferPixelateRect(pBuffer, uiPitch, &area, uiPattern))
	{
		UnLockVideoSurface( uiDestVSurface );
		// Blit has failed if false returned
		return( FALSE );
	}

	// Mark as dirty if it's the backbuffer
	//if ( uiDestVSurface == BACKBUFFER )
	//{
	//	InvalidateBackbuffer( );
	//}

	UnLockVideoSurface( uiDestVSurface );
	return( TRUE );
}


/**********************************************************************************************
 DestroyObjectPaletteTables

	Destroys the palette tables of a video object. All memory is deallocated, and
	the pointers set to NULL. Be careful not to try and blit this object until new
	tables are calculated, or things WILL go boom.

**********************************************************************************************/
BOOLEAN DestroyObjectPaletteTables(HVOBJECT hVObject)
{
UINT32 x;
BOOLEAN f16BitPal;

	for ( x = 0; x < HVOBJECT_SHADE_TABLES; x++ )
	{
		if ( !( hVObject->fFlags & VOBJECT_FLAG_SHADETABLE_SHARED ) )
		{
			if ( hVObject->pShades[x] != NULL )
			{
				if ( hVObject->pShades[x] == hVObject->p16BPPPalette )
					f16BitPal = TRUE;
				else
					f16BitPal = FALSE;

				MemFree( hVObject->pShades[x] );
				hVObject->pShades[x] = NULL;

				if ( f16BitPal )
					hVObject->p16BPPPalette = NULL;
			}
		}
	}

	if ( hVObject->p16BPPPalette != NULL )
	{
		MemFree( hVObject->p16BPPPalette );
		hVObject->p16BPPPalette = NULL;
	}

	hVObject->pShadeCurrent=NULL;
	hVObject->pGlow=NULL;

	return(TRUE);

}



UINT16 SetObjectShade(HVOBJECT pObj, UINT32 uiShade)
{
	Assert(pObj!=NULL);
	Assert(uiShade >= 0);
	Assert(uiShade < HVOBJECT_SHADE_TABLES);

	if(pObj->pShades[uiShade]==NULL)
	{
	DbgMessage(TOPIC_VIDEOOBJECT, DBG_LEVEL_2, String("Attempt to set shade level to NULL table"));
		return(FALSE);
	}

	pObj->pShadeCurrent=pObj->pShades[uiShade];
	return(TRUE);
}

UINT16 SetObjectHandleShade(UINT32 uiHandle, UINT32 uiShade)
{
	HVOBJECT hObj;

	#ifdef _DEBUG
		gubVODebugCode = DEBUGSTR_SETOBJECTHANDLESHADE;
	#endif
	if(!GetVideoObject(&hObj, uiHandle))
	{
	DbgMessage(TOPIC_VIDEOOBJECT, DBG_LEVEL_2, String("Invalid object handle for setting shade level"));
		return(FALSE);
	}
	return(SetObjectShade(hObj, uiShade));
}

/*
UINT16 FillObjectRect(UINT32 iObj, INT32 x1, INT32 y1, INT32 x2, INT32 y2, COLORVAL color32)
{
PIXEL	*pBuffer;
UINT32	uiPitch;
//HVSURFACE pSurface;

	// Lock video surface
	pBuffer = (PIXEL *)LockVideoSurface(iObj, &uiPitch );
	//UnLockVideoSurface(iObj);


	if (pBuffer == NULL)
		return( FALSE );

	FillRect16BPP(pBuffer, uiPitch, x1, y1, x2, y2, Get16BPPColor(color32));

	// Mark as dirty if it's the backbuffer
	if(iObj == BACKBUFFER)
		InvalidateBackbuffer();

	UnLockVideoSurface(iObj);
}

*/


/********************************************************************************************
	GetETRLEPixelValue

	Given a VOBJECT and ETRLE image index, retrieves the value of the pixel located at the
	given image coordinates. The value returned is an 8-bit palette index
********************************************************************************************/
BOOLEAN GetETRLEPixelValue( UINT8 * pDest, HVOBJECT hVObject, UINT16 usETRLEIndex, UINT16 usX, UINT16 usY )
{
	UINT8 *					pCurrent;
	UINT16					usLoopX = 0;
	UINT16					usLoopY = 0;
	UINT16					ubRunLength;
	ETRLEObject *		pETRLEObject;

	// Do a bunch of checks
	CHECKF( hVObject != NULL );
	CHECKF( usETRLEIndex < hVObject->usNumberOfObjects );

	pETRLEObject = &(hVObject->pETRLEObject[usETRLEIndex]);

	CHECKF( usX < pETRLEObject->usWidth );
	CHECKF( usY < pETRLEObject->usHeight );

	// Assuming everything's okay, go ahead and look...
	pCurrent = &((UINT8 *)hVObject->pPixData)[pETRLEObject->uiDataOffset];

	// Skip past all uninteresting scanlines
	while( usLoopY < usY )
	{
		while( *pCurrent != 0 )
		{
			if (*pCurrent & COMPRESS_TRANSPARENT)
			{
				pCurrent++;
			}
			else
			{
				pCurrent += *pCurrent & COMPRESS_RUN_MASK;
			}
		}
		usLoopY++;
	}

	// Now look in this scanline for the appropriate byte
	do
	{
		ubRunLength = *pCurrent & COMPRESS_RUN_MASK;

		if (*pCurrent & COMPRESS_TRANSPARENT)
		{
			if (usLoopX + ubRunLength >= usX)
			{
				*pDest = 0;
				return( TRUE );
			}
			else
			{
				pCurrent++;
			}
		}
		else
		{
			if (usLoopX + ubRunLength >= usX)
			{
				// skip to the correct byte; skip at least 1 to get past the byte defining the run
				pCurrent += (usX - usLoopX) + 1;
				*pDest = *pCurrent;
				return( TRUE );
			}
			else
			{
				pCurrent += ubRunLength + 1;
			}
		}
		usLoopX += ubRunLength;
	}
	while( usLoopX < usX );
	// huh???
	return( FALSE );
}

BOOLEAN GetVideoObjectETRLEProperties( HVOBJECT hVObject, ETRLEObject *pETRLEObject, UINT16 usIndex )
{
	CHECKF( usIndex >= 0 );
	CHECKF( usIndex < hVObject->usNumberOfObjects );

	memcpy( pETRLEObject, &( hVObject->pETRLEObject[ usIndex ] ), sizeof( ETRLEObject ) );

	return( TRUE );

}

BOOLEAN GetVideoObjectETRLESubregionProperties( UINT32 uiVideoObject, UINT16 usIndex, UINT16 *pusWidth, UINT16 *pusHeight )
{
	HVOBJECT							hVObject;
	ETRLEObject						ETRLEObject;

	// Get video object
	#ifdef _DEBUG
		gubVODebugCode = DEBUGSTR_GETVIDEOOBJECTETRLESUBREGIONPROPERTIES;
	#endif
	CHECKF( GetVideoObject( &hVObject, uiVideoObject ) );

	CHECKF( GetVideoObjectETRLEProperties( hVObject, &ETRLEObject, usIndex ) );

	*pusWidth = ETRLEObject.usWidth;
	*pusHeight = ETRLEObject.usHeight;

	return( TRUE );
}


BOOLEAN GetVideoObjectETRLEPropertiesFromIndex( UINT32 uiVideoObject, ETRLEObject *pETRLEObject, UINT16 usIndex )
{
	HVOBJECT							hVObject;

	// Get video object
	#ifdef _DEBUG
		gubVODebugCode = DEBUGSTR_GETVIDEOOBJECTETRLEPROPERTIESFROMINDEX;
	#endif
	CHECKF( GetVideoObject( &hVObject, uiVideoObject ) );

	CHECKF( GetVideoObjectETRLEProperties( hVObject, pETRLEObject, usIndex ) );

	return( TRUE );
}

BOOLEAN SetVideoObjectPalette8BPP(INT32 uiVideoObject, SGPPaletteEntry *pPal8)
{
	HVOBJECT							hVObject;

	// Get video object
	#ifdef _DEBUG
		gubVODebugCode = DEBUGSTR_SETVIDEOOBJECTPALETTE8BPP;
	#endif
	CHECKF( GetVideoObject( &hVObject, uiVideoObject ) );

	return( SetVideoObjectPalette( hVObject, pPal8 ) );
}


BOOLEAN GetVideoObjectPalette16BPP(INT32 uiVideoObject, PIXEL **ppPal16)
{
	HVOBJECT							hVObject;

	// Get video object
	#ifdef _DEBUG
		gubVODebugCode = DEBUGSTR_GETVIDEOOBJECTPALETTE16BPP;
	#endif
	CHECKF( GetVideoObject( &hVObject, uiVideoObject ) );

	*ppPal16 = hVObject->p16BPPPalette;

	return( TRUE );
}

BOOLEAN CopyVideoObjectPalette16BPP(INT32 uiVideoObject, UINT16 *ppPal16)
{
	HVOBJECT							hVObject;

	// Get video object
	#ifdef _DEBUG
		gubVODebugCode = DEBUGSTR_COPYVIDEOOBJECTPALETTE16BPP;
	#endif
	CHECKF( GetVideoObject( &hVObject, uiVideoObject ) );

	memcpy(ppPal16, hVObject->p16BPPPalette, 256*2);

	return( TRUE );
}

BOOLEAN CheckFor16BPPRegion( HVOBJECT hVObject, UINT16 usRegionIndex, UINT8 ubShadeLevel, UINT16 * pusIndex )
{
	UINT16					usLoop;
	SixteenBPPObjectInfo *	p16BPPObject;

	if (hVObject->usNumberOf16BPPObjects > 0)
	{
		for (usLoop = 0; usLoop < hVObject->usNumberOf16BPPObjects; usLoop++)
		{
			p16BPPObject = &(hVObject->p16BPPObject[usLoop]);
			if (p16BPPObject->usRegionIndex == usRegionIndex && p16BPPObject->ubShadeLevel == ubShadeLevel)
			{
				if (pusIndex != NULL)
				{
					*pusIndex = usLoop;
				}
				return( TRUE );
			}
		}
	}
	return( FALSE );
}

BOOLEAN ConvertVObjectRegionTo16BPP( HVOBJECT hVObject, UINT16 usRegionIndex, UINT8 ubShadeLevel )
{
	SixteenBPPObjectInfo *	p16BPPObject;
	UINT8 *					pInput;
	UINT8 *					pOutput;
	PIXEL *				p16BPPPalette;
	UINT32					uiDataLoop;
	UINT32					uiDataLength;
	UINT8					ubRunLoop;
	//UINT8					ubRunLength;
	INT8					bData;
	UINT32					uiLen;

	// check for existing 16BPP region and then allocate memory
	if (usRegionIndex >= hVObject->usNumberOfObjects || ubShadeLevel >= HVOBJECT_SHADE_TABLES)
	{
		return( FALSE );
	}
	if (CheckFor16BPPRegion( hVObject, usRegionIndex, ubShadeLevel, NULL) == TRUE)
	{
		// it already exists; no need to do anything!
		return( TRUE );
	}

	if (hVObject->usNumberOf16BPPObjects > 0)
	{
		// have to reallocate memory
		hVObject->p16BPPObject = (SixteenBPPObjectInfo *) MemRealloc( hVObject->p16BPPObject, sizeof( SixteenBPPObjectInfo ) * (hVObject->usNumberOf16BPPObjects + 1) );
	}
	else
	{
		// allocate memory for the first 16BPPObject
		hVObject->p16BPPObject = (SixteenBPPObjectInfo *) MemAlloc( sizeof( SixteenBPPObjectInfo ) );
	}
	if (hVObject->p16BPPObject == NULL)
	{
		hVObject->usNumberOf16BPPObjects = 0;
		return( FALSE );
	}

	// the new object is the last one in the array
	p16BPPObject = &(hVObject->p16BPPObject[hVObject->usNumberOf16BPPObjects]);

	// need twice as much memory because of going from 8 to 16 bits
	p16BPPObject->p16BPPData = (PIXEL *) MemAlloc( hVObject->pETRLEObject[usRegionIndex].uiDataLength * 2 );
	if (p16BPPObject->p16BPPData == NULL)
	{
		return( FALSE );
	}

	p16BPPObject->usRegionIndex = usRegionIndex;
	p16BPPObject->ubShadeLevel = ubShadeLevel;
	p16BPPObject->usHeight = hVObject->pETRLEObject[ usRegionIndex ].usHeight;
	p16BPPObject->usWidth = hVObject->pETRLEObject[ usRegionIndex ].usWidth;
	p16BPPObject->sOffsetX=hVObject->pETRLEObject[ usRegionIndex ].sOffsetX;
	p16BPPObject->sOffsetY=hVObject->pETRLEObject[ usRegionIndex ].sOffsetY;

	// get the palette
	p16BPPPalette = hVObject->pShades[ubShadeLevel];
	pInput = (UINT8 *) hVObject->pPixData + hVObject->pETRLEObject[ usRegionIndex ].uiDataOffset;

	uiDataLength=hVObject->pETRLEObject[usRegionIndex].uiDataLength;

	// now actually do the conversion

	uiLen=0;
	pOutput = (UINT8 *)p16BPPObject->p16BPPData;
	for (uiDataLoop = 0; uiDataLoop < uiDataLength; uiDataLoop++)
	{
		bData= *pInput;
		if(bData&0x80)
		{
			// transparent
			*pOutput = *pInput;
			pOutput++;
			pInput++;
			//uiDataLoop++;
			uiLen+=(UINT32)(bData&0x7f);
		}
		else if(bData > 0)
		{
			// nontransparent
			*pOutput = *pInput;
			pOutput++;
			pInput++;
			//uiDataLoop++;
			for(ubRunLoop=0; ubRunLoop < bData; ubRunLoop++)
			{
				*((UINT16 *)pOutput) = p16BPPPalette[*pInput];
				pOutput++;
				pOutput++;
				pInput++;
				uiDataLoop++;
			}
			uiLen+=(UINT32)bData;
		}
		else
		{
			// eol
			*pOutput = *pInput;
			pOutput++;
			pInput++;
			//uiDataLoop++;
			if(uiLen!=p16BPPObject->usWidth)
		 DbgMessage(TOPIC_VIDEOOBJECT, DBG_LEVEL_1, String( "Actual pixel width different from header width" ));
			uiLen=0;
		}

		// copy the run-length byte
	/*	*pOutput = *pInput;
		pOutput++;
		if (((*pInput) & COMPRESS_TRANSPARENT) == 0 && *pInput > 0)
		{
			// non-transparent run; deal with the pixel data
			ubRunLoop = 0;
			ubRunLength = ((*pInput) & COMPRESS_RUN_LIMIT);
			// skip to the next input byte
			pInput++;
			for (ubRunLoop = 0; ubRunLoop < ubRunLength; ubRunLoop++)
			{
				*((UINT16 *)pOutput) = p16BPPPalette[*pInput];
				// advance two bytes in output, one in input
				pOutput++;
				pOutput++;
				pInput++;
				uiDataLoop++;
			}
		}
		else
		{
			// transparent run or end of scanline; skip to the next input byte
			pInput++;
		} */
	}
	hVObject->usNumberOf16BPPObjects++;
	return( TRUE );
}


BOOLEAN BltVideoObjectOutlineFromIndex(UINT32 uiDestVSurface, UINT32 uiSrcVObject, UINT16 usIndex, INT32 iDestX, INT32 iDestY, PIXEL s16BPPColor, BOOLEAN fDoOutline )
{
	#ifdef _DEBUG
		gubVODebugCode = DEBUGSTR_BLTVIDEOOBJECTOUTLINEFROMINDEX;
	#endif
	HVOBJECT source = nullptr;
	if (!GetVideoObject(&source, uiSrcVObject)) return FALSE;
	(void)source;
	return SubmitVideoObjectOutline(
		uiDestVSurface, uiSrcVObject, usIndex,
		iDestX, iDestY,
		RenderImageOutlineMode::Color,
		s16BPPColor, fDoOutline) ? TRUE : FALSE;
}

BOOLEAN BltVideoObjectOutline(UINT32 uiDestVSurface, HVOBJECT hSrcVObject, UINT16 usIndex, INT32 iDestX, INT32 iDestY, PIXEL s16BPPColor, BOOLEAN fDoOutline )
{
	UINT32 image = 0;
	if (hSrcVObject && hSrcVObject->ubBitDepth == 8 &&
		FindVideoObjectHandle(hSrcVObject, image))
	{
		return SubmitVideoObjectOutline(
			uiDestVSurface, image, usIndex,
			iDestX, iDestY,
			RenderImageOutlineMode::Color,
			s16BPPColor, fDoOutline) ? TRUE : FALSE;
	}
	return Draw8BitVideoObjectOutlineToSurface(
		uiDestVSurface, hSrcVObject, usIndex,
		iDestX, iDestY,
		RenderImageOutlineMode::Color,
		s16BPPColor, fDoOutline);
}



BOOLEAN BltVideoObjectOutlineShadowFromIndex(UINT32 uiDestVSurface, UINT32 uiSrcVObject, UINT16 usIndex, INT32 iDestX, INT32 iDestY )
{
	#ifdef _DEBUG
		gubVODebugCode = DEBUGSTR_BLTVIDEOOBJECTOUTLINESHADOWFROMINDEX;
	#endif
	HVOBJECT source = nullptr;
	if (!GetVideoObject(&source, uiSrcVObject)) return FALSE;
	(void)source;
	return SubmitVideoObjectOutline(
		uiDestVSurface, uiSrcVObject, usIndex,
		iDestX, iDestY,
		RenderImageOutlineMode::Shadow,
		0, FALSE) ? TRUE : FALSE;
}

BOOLEAN BltVideoObjectOutlineShadow(UINT32 uiDestVSurface, HVOBJECT hSrcVObject, UINT16 usIndex, INT32 iDestX, INT32 iDestY )
{
	UINT32 image = 0;
	if (hSrcVObject && hSrcVObject->ubBitDepth == 8 &&
		FindVideoObjectHandle(hSrcVObject, image))
	{
		return SubmitVideoObjectOutline(
			uiDestVSurface, image, usIndex,
			iDestX, iDestY,
			RenderImageOutlineMode::Shadow,
			0, FALSE) ? TRUE : FALSE;
	}
	return Draw8BitVideoObjectOutlineToSurface(
		uiDestVSurface, hSrcVObject, usIndex,
		iDestX, iDestY,
		RenderImageOutlineMode::Shadow,
		0, FALSE);
}

#ifdef _DEBUG
void CheckValidVObjectIndex( UINT32 uiIndex )
{
	BOOLEAN fAssertError = FALSE;
	if( uiIndex == 0 || uiIndex == 0xffffffff )
	{ //-1 index -- deleted
		fAssertError = TRUE;
	}
	if( uiIndex >= 0xfffffff0 )
	{ //the 0xfffffff0+ values remain reserved for special surfaces
		fAssertError = TRUE;
	}

	if( fAssertError )
	{
		CHAR8 str[60];
		switch( gubVODebugCode )
		{
			case DEBUGSTR_SETVIDEOOBJECTTRANSPARENCY:
				sprintf( str, "SetVideoObjectTransparency" );
				break;
			case DEBUGSTR_BLTVIDEOOBJECTFROMINDEX:
				sprintf( str, "BltVideoObjectFromIndex" );
				break;
			case DEBUGSTR_SETOBJECTHANDLESHADE:
				sprintf( str, "SetObjectHandleShade" );
				break;
			case DEBUGSTR_GETVIDEOOBJECTETRLESUBREGIONPROPERTIES:
				sprintf( str, "GetVideoObjectETRLESubRegionProperties" );
				break;
			case DEBUGSTR_GETVIDEOOBJECTETRLEPROPERTIESFROMINDEX:
				sprintf( str, "GetVideoObjectETRLEPropertiesFromIndex" );
				break;
			case DEBUGSTR_SETVIDEOOBJECTPALETTE8BPP:
				sprintf( str, "SetVideoObjectPalette8BPP" );
				break;
			case DEBUGSTR_GETVIDEOOBJECTPALETTE16BPP:
				sprintf( str, "GetVideoObjectPalette16BPP" );
				break;
			case DEBUGSTR_COPYVIDEOOBJECTPALETTE16BPP:
				sprintf( str, "CopyVideoObjectPalette16BPP" );
				break;
			case DEBUGSTR_BLTVIDEOOBJECTOUTLINEFROMINDEX:
				sprintf( str, "BltVideoObjectOutlineFromIndex" );
				break;
			case DEBUGSTR_BLTVIDEOOBJECTOUTLINESHADOWFROMINDEX:
				sprintf( str, "BltVideoObjectOutlineShadowFromIndex" );
				break;
			case DEBUGSTR_DELETEVIDEOOBJECTFROMINDEX:
				sprintf( str, "DeleteVideoObjectFromIndex" );
				break;
			case DEBUGSTR_NONE:
			default:
				sprintf( str, "GetVideoObject" );
				break;
		}
	}
}
#endif

#ifdef SGP_VIDEO_DEBUGGING

typedef struct DUMPFILENAME
{
	CHAR8 str[256];
}DUMPFILENAME;

void DumpVObjectInfoIntoFile( const STR8 filename, BOOLEAN fAppend )
{
	FILE *fp;
	DUMPFILENAME *pName, *pCode;
	UINT32 *puiCounter;
	CHAR8 tempName[ 256 ];
	CHAR8 tempCode[ 256 ];
	UINT32 i, uiUniqueID;
	BOOLEAN fFound;
	if( !guiVObjectSize )
	{
		return;
	}

	if( fAppend )
	{
		fp = fopen( filename, "a" );
	}
	else
	{
		fp = fopen( filename, "w" );
	}
	Assert( fp );

	//Allocate enough strings and counters for each node.
	pName = (DUMPFILENAME*)MemAlloc( sizeof( DUMPFILENAME ) * guiVObjectSize );
	pCode = (DUMPFILENAME*)MemAlloc( sizeof( DUMPFILENAME ) * guiVObjectSize );
	memset( pName, 0, sizeof( DUMPFILENAME ) * guiVObjectSize );
	memset( pCode, 0, sizeof( DUMPFILENAME ) * guiVObjectSize );
	puiCounter = (UINT32*)MemAlloc( 4 * guiVObjectSize );
	memset( puiCounter, 0, 4 * guiVObjectSize );

	//Loop through the list and record every unique filename and count them
	uiUniqueID = 0;
	gVideoObjects.forEach([&](UINT32, const VideoObjectResource& resource) {
		strcpy( tempName, resource.name.c_str() );
		strcpy( tempCode, resource.code.c_str() );
		fFound = FALSE;
		for( i = 0; i < uiUniqueID; i++ )
		{
			if( !_stricmp( tempName, pName[i].str ) && !_stricmp( tempCode, pCode[i].str ) )
			{ //same string
				fFound = TRUE;
				(puiCounter[ i ])++;
				break;
			}
		}
		if( !fFound )
		{
			strcpy( pName[i].str, tempName );
			strcpy( pCode[i].str, tempCode );
			(puiCounter[ i ])++;
			uiUniqueID++;
		}
	});

	//Now dump the info.
	fprintf( fp, "-----------------------------------------------\n" );
	fprintf( fp, "%d unique vObject names exist in %d VObjects\n", uiUniqueID, guiVObjectSize );
	fprintf( fp, "-----------------------------------------------\n\n" );
	for( i = 0; i < uiUniqueID; i++ )
	{
		fprintf( fp, "%d occurrences of %s\n", puiCounter[i], pName[i].str );
		fprintf( fp, "%s\n\n", pCode[i].str );
	}
	fprintf( fp, "\n-----------------------------------------------\n\n" );

	//Free all memory associated with this operation.
	MemFree( pName );
	MemFree( pCode );
	MemFree( puiCounter );
	fclose( fp );
}

//Debug wrapper for adding vObjects
BOOLEAN _AddAndRecordVObject( VOBJECT_DESC *VObjectDesc, UINT32 *uiIndex, UINT32 uiLineNum, const STR8 pSourceFile )
{
	CHAR8 str[256];
	if( !AddStandardVideoObject( VObjectDesc, uiIndex ) )
	{
		return FALSE;
	}

	VideoObjectResource* const resource = gVideoObjects.find(*uiIndex);
	if (!resource) return FALSE;
	resource->name = VObjectDesc->ImageFile;
	sprintf( str, "%s -- line(%d)", pSourceFile, uiLineNum );
	resource->code = str;

	return TRUE;
}

void PerformVideoInfoDumpIntoFile( const STR8 filename, BOOLEAN fAppend )
{
	DumpVObjectInfoIntoFile( filename, fAppend );
	DumpVSurfaceInfoIntoFile( filename, TRUE );
}

#endif

// Flugente: retrieve width and height of video object
void GetVideoObjectDimensions( HVOBJECT hSrcVObject, UINT16 usIndex, UINT16& rusWidth, UINT16& rusHeight )
{
	rusWidth = 0;
	rusHeight = 0;

	if ( hSrcVObject == NULL )
		return;

	switch ( hSrcVObject->ubBitDepth )
	{
	case 32:
	case 16:
		if ( usIndex < hSrcVObject->usNumberOf16BPPObjects )
		{
			rusWidth = hSrcVObject->p16BPPObject[usIndex].usWidth;
			rusHeight = hSrcVObject->p16BPPObject[usIndex].usHeight;
		}
		break;

	case 8:
		if ( usIndex < hSrcVObject->usNumberOfObjects )
		{
			rusWidth = hSrcVObject->pETRLEObject[usIndex].usWidth;
			rusHeight = hSrcVObject->pETRLEObject[usIndex].usHeight;
		}
		break;
	}
}
