#ifndef ENGINE_CORE_IDENTIFIER_H
#define ENGINE_CORE_IDENTIFIER_H

#include <string>

// Stable ASCII identifier shared by content manifests and trusted provenance.
// Keeping this deliberately narrower than a filesystem path makes identifiers
// safe to log, compare, and use as package overlay keys on every host.
bool IsValidEngineIdentifier(const std::string& identifier);

#endif
