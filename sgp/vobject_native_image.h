#ifndef SGP_VOBJECT_NATIVE_IMAGE_H
#define SGP_VOBJECT_NATIVE_IMAGE_H

#include "vobject.h"

// Transactionally imports one true-colour HIMAGE frame into native video-object
// storage. The HIMAGE remains owned by the caller.
BOOLEAN ImportNativeVideoObjectImage(HVOBJECT object, HIMAGE image);

#endif
