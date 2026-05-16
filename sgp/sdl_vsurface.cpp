// SDL3-backed video surface manager. Phase 5 second slice.
//
// This slice ports the two pure-C++ pieces of vsurface.cpp that have
// zero DirectDraw dependency: ClipRectangle (clipping geometry) and
// the SurfaceData namespace (a couple of std::map registries that
// map between application data pointers and surface IDs). Both lift
// verbatim from the legacy file.
//
// Still stubbed in sgp_non_win32_stubs.cpp until later slices:
//   - The SGPVSurface linked-list manager (AddStandardVideoSurface /
//     GetVideoSurface / DeleteVideoSurfaceFromIndex /
//     InitializeVideoSurfaceManager / ShutdownVideoSurfaceManager)
//   - All blitters: BltVideoSurface*, BltStretchVideoSurface,
//     BltVideoSurfaceToVideoSurface, BltVSurfaceUsingDD
//   - ColorFillVideoSurfaceArea / ImageFillVideoSurfaceArea
//   - ShadowVideoSurfaceRect / ShadowVideoSurfaceImage /
//     ShadowVideoSurfaceRectUsingLowPercentTable
//   - LockVideoSurface / UnLockVideoSurface
//   - SetVideoSurfaceTransparency / GetVSurfacePaletteEntries

#ifndef _WIN32

#include "types.h"
#include "vsurface.h"
#include "vobject_blitters.h"
#include "DEBUG.H"

#include <map>

// g_SurfaceRectangle is defined by vobject_blitters.cpp; pulled in
// here as extern so the SurfaceData registry can keep the per-surface
// clipping rectangle in sync.
extern std::map<UINT32, ClipRectangle> g_SurfaceRectangle;

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

#endif // !_WIN32
