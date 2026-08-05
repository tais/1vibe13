#ifndef LAPTOP_IMP_PAGE_RESOURCE_OWNER_H
#define LAPTOP_IMP_PAGE_RESOURCE_OWNER_H

#include "LaptopPageResourceOwner.h"
#include "ImpPageResourceState.h"

inline LaptopPageResourceOwner& GetImpPageResourceOwner()
{
	static LaptopPageResourceOwner owner;
	return owner;
}

inline void FailImpPageResources()
{
	GetImpPageResourceOwner().clear();
	GetImpPageResourceTransactionState().fail();
}

inline void BeginImpPageResources()
{
	GetImpPageResourceOwner().clear();
	GetImpPageResourceTransactionState().begin();
}

inline BOOLEAN AddImpPageVideoObject(
	VOBJECT_DESC* description, UINT32* publishedValue)
{
	if (!GetImpPageResourceTransactionState().canAcquire()) return FALSE;
	if (publishedValue && GetImpPageResourceOwner().addVideoObject(
		description, *publishedValue)) return TRUE;
	FailImpPageResources();
	return FALSE;
}

inline void DeleteImpPageVideoObject(UINT32 value)
{
	GetImpPageResourceOwner().removeVideoObject(value);
}

inline INT32 LoadImpPageButtonImage(STR8 filename, INT32 grayed,
	INT32 offNormal, INT32 offHilite, INT32 onNormal, INT32 onHilite)
{
	if (!GetImpPageResourceTransactionState().canAcquire()) return -1;
	INT32 publishedValue = -1;
	if (GetImpPageResourceOwner().addButtonImage(LoadButtonImageOwned(
		filename, grayed, offNormal, offHilite, onNormal, onHilite),
		publishedValue)) return publishedValue;
	FailImpPageResources();
	return -1;
}

inline INT32 UseLoadedImpPageButtonImage(INT32 loadedImage, INT32 grayed,
	INT32 offNormal, INT32 offHilite, INT32 onNormal, INT32 onHilite)
{
	if (!GetImpPageResourceTransactionState().canAcquire()) return -1;
	INT32 publishedValue = -1;
	if (GetImpPageResourceOwner().addButtonImage(UseLoadedButtonImageOwned(
		loadedImage, grayed, offNormal, offHilite, onNormal, onHilite),
		publishedValue)) return publishedValue;
	FailImpPageResources();
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
	if (!GetImpPageResourceTransactionState().canAcquire()) return -1;
	INT32 publishedValue = -1;
	const INT32 button = QuickCreateButton(image, x, y, type, priority,
		moveCallback, clickCallback);
	if (GetImpPageResourceOwner().addButton(button, publishedValue))
		return publishedValue;
	FailImpPageResources();
	return -1;
}

inline INT32 CreateImpPageIconAndTextButton(INT32 image, const STR16 text,
	UINT32 font, INT16 foreground, INT16 shadow, INT16 downForeground,
	INT16 downShadow, INT8 justification, INT16 x, INT16 y, INT32 type,
	INT16 priority, GUI_CALLBACK moveCallback, GUI_CALLBACK clickCallback)
{
	if (!GetImpPageResourceTransactionState().canAcquire()) return -1;
	INT32 publishedValue = -1;
	const INT32 button = CreateIconAndTextButton(image, text, font,
		foreground, shadow, downForeground, downShadow, justification,
		x, y, type, priority, moveCallback, clickCallback);
	if (GetImpPageResourceOwner().addButton(button, publishedValue))
		return publishedValue;
	FailImpPageResources();
	return -1;
}

inline void DeleteImpPageButton(INT32 value)
{
	GetImpPageResourceOwner().removeButton(value);
}

inline void SetImpPageButtonClicked(INT32 value, bool clicked)
{
	GUI_BUTTON* button = GetButtonPtr(value);
	if (!button) return;
	if (clicked)
		button->uiFlags |= BUTTON_CLICKED_ON;
	else
		button->uiFlags &= ~BUTTON_CLICKED_ON;
}

inline void AddImpPageRegion(MOUSE_REGION* region)
{
	if (!GetImpPageResourceTransactionState().canAcquire()) return;
	if (region && GetImpPageResourceOwner().addRegion(*region)) return;
	FailImpPageResources();
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
#define UseLoadedButtonImage UseLoadedImpPageButtonImage
#define UnloadButtonImage DeleteImpPageButtonImage
#define QuickCreateButton CreateImpPageQuickButton
#define CreateIconAndTextButton CreateImpPageIconAndTextButton
#define RemoveButton DeleteImpPageButton
#define MSYS_AddRegion AddImpPageRegion
#define MSYS_RemoveRegion DeleteImpPageRegion

#endif
