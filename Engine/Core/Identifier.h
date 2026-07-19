#ifndef ENGINE_CORE_IDENTIFIER_H
#define ENGINE_CORE_IDENTIFIER_H

#include <string>

// Stable ASCII identifier shared by content manifests and trusted provenance.
// Keeping this deliberately narrower than a filesystem path makes identifiers
// safe to log, compare, and use as package overlay keys on every host.
inline bool IsValidEngineIdentifier(const std::string& identifier)
{
	if (identifier.empty()) return false;
	for (char value : identifier)
	{
		const bool valid = (value >= 'a' && value <= 'z') ||
			(value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
			value == '.' || value == '_' || value == '-';
		if (!valid) return false;
	}
	return true;
}

#endif
