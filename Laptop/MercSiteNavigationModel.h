#ifndef LAPTOP_MERC_SITE_NAVIGATION_MODEL_H
#define LAPTOP_MERC_SITE_NAVIGATION_MODEL_H

#include <cstddef>

// Dependency-free bounds used by the legacy M.E.R.C. page adapters. Persisted
// selection values and alternate-profile skips must never manufacture an
// index when the configured site is empty or step below the first entry.
constexpr std::size_t ClampMercSiteIndex(
	std::size_t index, std::size_t availableCount)
{
	return availableCount == 0 ? 0 :
		(index < availableCount ? index : availableCount - 1);
}

constexpr std::size_t SkipMercSiteAlternatePredecessor(
	std::size_t index, bool hasAlternateProfile)
{
	return hasAlternateProfile && index > 0 ? index - 1 : index;
}

#endif
