#ifndef TACTICAL_DISEASE_TYPES_H
#define TACTICAL_DISEASE_TYPES_H

#include "types.h"

// Persistent soldier disease arrays and the global rule table share this
// established capacity. Changing it requires an explicit save-schema change.
inline constexpr UINT8 NUM_DISEASES = 20;

#endif
