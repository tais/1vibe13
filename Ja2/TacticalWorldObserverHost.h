#ifndef JA2_TACTICAL_WORLD_OBSERVER_HOST_H
#define JA2_TACTICAL_WORLD_OBSERVER_HOST_H

#include <cstdint>

#include <Engine/Adapters/JA2/TacticalWorldObserver.h>

struct Ja2TacticalWorldObserverDiagnostics
{
	TacticalWorldObserverUpdateResult lastUpdate =
		TacticalWorldObserverUpdateResult::SourceUnavailable;
	std::uint64_t publicationSerial = 0;
	std::uint64_t safeFrameUpdates = 0;
};

// Application-owned read-only service registered with the production engine
// composition root before packages activate.
TacticalWorldObserverService& GetJa2TacticalWorldObserverService() noexcept;

// The production GameLoop invokes this exactly once after all legacy work at
// its completed-frame boundary. It performs no logging or publication beyond
// the observer's capture/diff contract.
void UpdateJa2TacticalWorldObserverAtSafeFrame() noexcept;

Ja2TacticalWorldObserverDiagnostics GetJa2TacticalWorldObserverDiagnostics() noexcept;

#endif
