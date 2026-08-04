#ifndef LAPTOP_IMP_PAGE_RESOURCE_OWNER_H
#define LAPTOP_IMP_PAGE_RESOURCE_OWNER_H

#include "LaptopPageResourceOwner.h"
#include "LaptopUiStateModel.h"

#include <cstdio>
#include <iterator>
#include <utility>

inline LaptopPageResourceOwner& GetImpPageResourceOwner()
{
	static LaptopPageResourceOwner owner;
	return owner;
}

inline void BeginImpPageResources()
{
	GetImpPageResourceOwner().clear();
}

inline BOOLEAN AddImpPageVideoObject(
	VOBJECT_DESC* description, UINT32* publishedValue)
{
	if (publishedValue && GetImpPageResourceOwner().addVideoObject(
		description, *publishedValue)) return TRUE;
	GetImpPageResourceOwner().clear();
	return FALSE;
}

inline void DeleteImpPageVideoObject(UINT32 value)
{
	GetImpPageResourceOwner().removeVideoObject(value);
}

inline INT32 LoadImpPageButtonImage(STR8 filename, INT32 grayed,
	INT32 offNormal, INT32 offHilite, INT32 onNormal, INT32 onHilite)
{
	INT32 publishedValue = -1;
	if (GetImpPageResourceOwner().addButtonImage(LoadButtonImageOwned(
		filename, grayed, offNormal, offHilite, onNormal, onHilite),
		publishedValue)) return publishedValue;
	GetImpPageResourceOwner().clear();
	return -1;
}

inline void DeleteImpPageButtonImage(INT32 value)
{
	GetImpPageResourceOwner().removeButtonImage(value);
}

inline INT32 CreateImpPageQuickButton(UINT32 image, INT16 x, INT16 y,
	INT32 type, INT16 priority, GUI_CALLBACK moveCallback,
	GUI_CALLBACK clickCallback)
{
	INT32 publishedValue = -1;
	const INT32 button = QuickCreateButton(image, x, y, type, priority,
		moveCallback, clickCallback);
	if (GetImpPageResourceOwner().addButton(button, publishedValue))
		return publishedValue;
	GetImpPageResourceOwner().clear();
	return -1;
}

inline INT32 CreateImpPageIconAndTextButton(INT32 image, const STR16 text,
	UINT32 font, INT16 foreground, INT16 shadow, INT16 downForeground,
	INT16 downShadow, INT8 justification, INT16 x, INT16 y, INT32 type,
	INT16 priority, GUI_CALLBACK moveCallback, GUI_CALLBACK clickCallback)
{
	INT32 publishedValue = -1;
	const INT32 button = CreateIconAndTextButton(image, text, font,
		foreground, shadow, downForeground, downShadow, justification,
		x, y, type, priority, moveCallback, clickCallback);
	if (GetImpPageResourceOwner().addButton(button, publishedValue))
		return publishedValue;
	GetImpPageResourceOwner().clear();
	return -1;
}

inline void DeleteImpPageButton(INT32 value)
{
	GetImpPageResourceOwner().removeButton(value);
}

inline void AddImpPageRegion(MOUSE_REGION* region)
{
	if (!region || GetImpPageResourceOwner().addRegion(*region)) return;
	GetImpPageResourceOwner().clear();
}

inline void DeleteImpPageRegion(MOUSE_REGION* region)
{
	if (region) GetImpPageResourceOwner().removeRegion(*region);
}

// Keep the legacy page implementations reviewable while routing their
// existing calls through one exact owner. The underlying SGP declarations are
// already visible through LaptopPageResourceOwner before these redirects.
#undef AddVideoObject
#define AddVideoObject AddImpPageVideoObject
#define DeleteVideoObjectFromIndex DeleteImpPageVideoObject
#define LoadButtonImage LoadImpPageButtonImage
#define UnloadButtonImage DeleteImpPageButtonImage
#define QuickCreateButton CreateImpPageQuickButton
#define CreateIconAndTextButton CreateImpPageIconAndTextButton
#define RemoveButton DeleteImpPageButton
#define MSYS_AddRegion AddImpPageRegion
#define MSYS_RemoveRegion DeleteImpPageRegion

#endif
