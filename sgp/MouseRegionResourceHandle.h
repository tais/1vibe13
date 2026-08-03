#ifndef SGP_MOUSE_REGION_RESOURCE_HANDLE_H
#define SGP_MOUSE_REGION_RESOURCE_HANDLE_H

#include "../Engine/Core/UniqueResourcePtr.h"
#include "mousesystem.h"

struct MouseRegionRegistrationReleaser
{
	void operator()(MOUSE_REGION* region) const { MSYS_RemoveRegion(region); }
};

using UniqueMouseRegionRegistration =
	UniqueResourcePtr<MOUSE_REGION, MouseRegionRegistrationReleaser>;

inline UniqueMouseRegionRegistration RegisterMouseRegionOwned(
	MOUSE_REGION* region)
{
	if (!region) return {};
	MSYS_AddRegion(region);
	return UniqueMouseRegionRegistration(region);
}

#endif
