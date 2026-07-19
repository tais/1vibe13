#ifndef SGP_VIDEO_RESOURCE_HANDLE_H
#define SGP_VIDEO_RESOURCE_HANDLE_H

#include "ResourceHandle.h"
#include "vobject.h"
#include "vsurface.h"

struct VideoObjectHandleTag {};
struct VideoSurfaceHandleTag {};

struct VideoObjectHandleReleaser
{
	void operator()(UINT32 value) const { DeleteVideoObjectFromIndex(value); }
};

struct VideoSurfaceHandleReleaser
{
	void operator()(UINT32 value) const { DeleteVideoSurfaceFromIndex(value); }
};

using UniqueVideoObjectHandle = UniqueResourceHandle<VideoObjectHandleTag, VideoObjectHandleReleaser>;
using UniqueVideoSurfaceHandle = UniqueResourceHandle<VideoSurfaceHandleTag, VideoSurfaceHandleReleaser>;

inline UniqueVideoObjectHandle AddVideoObjectOwned(VOBJECT_DESC* description)
{
	UINT32 value = 0;
	return AddVideoObject(description, &value) ? UniqueVideoObjectHandle(value) : UniqueVideoObjectHandle();
}

inline UniqueVideoSurfaceHandle AddVideoSurfaceOwned(VSURFACE_DESC* description)
{
	UINT32 value = 0;
	return AddVideoSurface(description, &value) ? UniqueVideoSurfaceHandle(value) : UniqueVideoSurfaceHandle();
}

#endif
