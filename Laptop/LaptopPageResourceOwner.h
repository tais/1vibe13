#ifndef LAPTOP_PAGE_RESOURCE_OWNER_H
#define LAPTOP_PAGE_RESOURCE_OWNER_H

#include "ButtonResourceHandle.h"
#include "MouseRegionResourceHandle.h"
#include <Engine/Core/ResourceHandleSet.h>
#include "VideoResourceHandle.h"

// Legacy Laptop pages still render through numeric handles and static mouse
// regions. This owner keeps those compatibility values non-owning while
// staging and releasing the underlying resources transactionally.
class LaptopPageResourceOwner
{
public:
	LaptopPageResourceOwner() = default;
	LaptopPageResourceOwner(const LaptopPageResourceOwner&) = delete;
	LaptopPageResourceOwner& operator=(const LaptopPageResourceOwner&) = delete;
	LaptopPageResourceOwner(LaptopPageResourceOwner&&) noexcept = default;
	LaptopPageResourceOwner& operator=(LaptopPageResourceOwner&&) noexcept = default;

	bool addVideoObject(VOBJECT_DESC* description, UINT32& publishedValue)
	{
		return videoObjects_.add(
			AddVideoObjectOwned(description), publishedValue);
	}

	bool addVideoSurface(VSURFACE_DESC* description, UINT32& publishedValue)
	{
		return videoSurfaces_.add(
			AddVideoSurfaceOwned(description), publishedValue);
	}

	bool addButtonImage(UniqueButtonImageHandle image, INT32& publishedValue)
	{
		return buttonImages_.add(std::move(image), publishedValue);
	}

	template <typename PublishedValue>
	bool addButton(INT32 button, PublishedValue& publishedValue)
	{
		INT32 ownedValue = -1;
		if (!buttons_.add(UniqueButtonHandle(button), ownedValue)) return false;
		publishedValue = static_cast<PublishedValue>(ownedValue);
		return true;
	}

	bool addRegion(MOUSE_REGION& region)
	{
		return regions_.add(RegisterMouseRegionOwned(&region));
	}

	void clear()
	{
		regions_.clear();
		buttons_.clear();
		buttonImages_.clear();
		videoSurfaces_.clear();
		videoObjects_.clear();
	}

	bool empty() const
	{
		return regions_.empty() && buttons_.empty() &&
			buttonImages_.empty() && videoSurfaces_.empty() &&
			videoObjects_.empty();
	}

private:
	ResourceHandleSet<UniqueVideoObjectHandle> videoObjects_;
	ResourceHandleSet<UniqueVideoSurfaceHandle> videoSurfaces_;
	ResourceHandleSet<UniqueButtonImageHandle> buttonImages_;
	ResourceHandleSet<UniqueButtonHandle> buttons_;
	ResourceHandleSet<UniqueMouseRegionRegistration> regions_;
};

#endif
