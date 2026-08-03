#ifndef SGP_BUTTON_RESOURCE_HANDLE_H
#define SGP_BUTTON_RESOURCE_HANDLE_H

#include "../Engine/Core/UniqueResourceHandle.h"
#include "Button System.h"

#include <cstdint>

struct ButtonImageHandleTag {};
struct ButtonHandleTag {};

struct ButtonImageHandleReleaser
{
	void operator()(std::int32_t value) const { UnloadButtonImage(value); }
};

struct ButtonHandleReleaser
{
	void operator()(std::int32_t value) const { RemoveButton(value); }
};

using UniqueButtonImageHandle = UniqueResourceHandle<ButtonImageHandleTag,
	ButtonImageHandleReleaser, std::int32_t, -1>;
using UniqueButtonHandle = UniqueResourceHandle<ButtonHandleTag,
	ButtonHandleReleaser, std::int32_t, -1>;

inline UniqueButtonImageHandle LoadButtonImageOwned(STR8 filename,
	INT32 grayed, INT32 offNormal, INT32 offHilite,
	INT32 onNormal, INT32 onHilite)
{
	return UniqueButtonImageHandle(LoadButtonImage(filename, grayed,
		offNormal, offHilite, onNormal, onHilite));
}

#endif
