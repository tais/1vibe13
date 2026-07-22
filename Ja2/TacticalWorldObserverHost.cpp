#include "TacticalWorldObserverHost.h"

#include <limits>

#include "Overhead.h"
#include "TacticalWorldAdapter.h"

namespace
{
class Ja2TacticalWorldObserverHost
{
public:
	Ja2TacticalWorldObserverHost() noexcept
		: observer_(GetJa2TacticalWorldAdapter(), TacticalWorldObserverLimits{
			TOTAL_SOLDIERS, TOTAL_SOLDIERS * 3 + 2})
	{
	}

	TacticalWorldObserverService& service() noexcept
	{
		return observer_;
	}

	void updateAtSafeFrame() noexcept
	{
		lastUpdate_ = observer_.update();
		if (safeFrameUpdates_ != std::numeric_limits<std::uint64_t>::max())
			++safeFrameUpdates_;
	}

	Ja2TacticalWorldObserverDiagnostics diagnostics() const noexcept
	{
		const TacticalWorldPublicationView publication = observer_.latest();
		return Ja2TacticalWorldObserverDiagnostics{
			lastUpdate_, publication ? publication.serial : 0, safeFrameUpdates_};
	}

private:
	TacticalWorldObserver observer_;
	TacticalWorldObserverUpdateResult lastUpdate_ =
		TacticalWorldObserverUpdateResult::SourceUnavailable;
	std::uint64_t safeFrameUpdates_ = 0;
};

Ja2TacticalWorldObserverHost& GetObserverHost() noexcept
{
	static Ja2TacticalWorldObserverHost host;
	return host;
}
}

TacticalWorldObserverService& GetJa2TacticalWorldObserverService() noexcept
{
	return GetObserverHost().service();
}

void UpdateJa2TacticalWorldObserverAtSafeFrame() noexcept
{
	GetObserverHost().updateAtSafeFrame();
}

Ja2TacticalWorldObserverDiagnostics GetJa2TacticalWorldObserverDiagnostics() noexcept
{
	return GetObserverHost().diagnostics();
}
