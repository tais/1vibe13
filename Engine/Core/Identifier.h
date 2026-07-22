#ifndef ENGINE_CORE_IDENTIFIER_H
#define ENGINE_CORE_IDENTIFIER_H

#include <cstddef>
#include <string>

// Public identifiers and opaque version labels cross queues, archives, logs,
// and package registries. Bound metadata independently of payload ceilings.
constexpr std::size_t MaximumEngineIdentifierBytes = 256;
constexpr std::size_t MaximumEngineVersionBytes = 256;

// Stable ASCII identifier shared by content manifests and trusted provenance.
// Keeping this deliberately narrower than a filesystem path makes identifiers
// safe to log, compare, and use as package overlay keys on every host.
bool IsValidEngineIdentifier(const std::string& identifier) noexcept;

#endif
