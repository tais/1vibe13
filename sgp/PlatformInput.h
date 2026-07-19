#ifndef SGP_PLATFORM_INPUT_H
#define SGP_PLATFORM_INPUT_H

#include <Engine/Core/InputSource.h>

// Adapter over the existing JA2 input atom queue. SDL remains responsible for
// translating native events into that queue during the compatibility phase.
InputSource& GetPlatformInputSource();

#endif
