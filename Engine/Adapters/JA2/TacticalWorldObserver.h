#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_OBSERVER_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_OBSERVER_H

#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/TacticalWorldDelta.h>
#include <Engine/Adapters/JA2/TacticalWorldService.h>

inline constexpr const char* TacticalWorldObserverServiceId =
	"ja2.tactical-world-observer";
inline constexpr EngineServiceVersion TacticalWorldObserverServiceVersion{1, 0};

struct TacticalWorldObserverLimits
{
	std::size_t maximumActors = TacticalWorldSnapshot::DefaultMaximumActors;
	std::size_t maximumEvents = TacticalWorldSnapshot::DefaultMaximumActors * 3 + 2;
};

enum class TacticalWorldObserverUpdateResult
{
	PublishedBaseline,
	PublishedDelta,
	SourceUnavailable,
	SourceCapacityReached,
	SourceAllocationFailure,
	SourceAdapterFailure,
	InvalidSnapshot,
	ActorCapacityReached,
	EventCapacityReached,
	AllocationFailure,
	SerialExhausted
};

enum class TacticalWorldPublicationStatus
{
	Unavailable,
	Baseline,
	Delta
};

// A read-only view of one transactionally accepted observer publication. The
// pointers remain valid until the observer's next successful update() or its
// destruction. Packages should consume or copy the view on the same main
// thread safe-frame boundary that drives the observer.
struct TacticalWorldPublicationView
{
	TacticalWorldPublicationStatus status = TacticalWorldPublicationStatus::Unavailable;
	std::uint64_t serial = 0;
	const TacticalWorldSnapshot* snapshot = nullptr;
	const TacticalWorldDelta* delta = nullptr;

	explicit operator bool() const
	{
		return status != TacticalWorldPublicationStatus::Unavailable &&
			serial != 0 && snapshot && delta;
	}
};

// Versioned package-facing service. It is deliberately read-only: only the
// host-owned observer may capture and publish tactical state.
class TacticalWorldObserverService
{
public:
	virtual ~TacticalWorldObserverService() = default;
	virtual TacticalWorldPublicationView latest() const noexcept = 0;
};

inline EngineServiceRegistrationError RegisterTacticalWorldObserverService(
	ServiceCatalog& catalog, TacticalWorldObserverService& service) noexcept
{
	return catalog.registerService<TacticalWorldObserverService>(
		TacticalWorldObserverServiceId, TacticalWorldObserverServiceVersion, service);
}

class NullTacticalWorldObserverService final : public TacticalWorldObserverService
{
public:
	static NullTacticalWorldObserverService& instance()
	{
		static NullTacticalWorldObserverService service;
		return service;
	}

	TacticalWorldPublicationView latest() const noexcept override
	{
		return {};
	}

private:
	NullTacticalWorldObserverService() = default;
};

// Main-thread observer driven explicitly by the host at a safe frame boundary.
// It never starts a worker or mutates the source service. Failed captures and
// diffs leave the complete last good publication (snapshot, delta, and serial)
// untouched.
class TacticalWorldObserver final : public TacticalWorldObserverService
{
public:
	explicit TacticalWorldObserver(
		TacticalWorldService& source,
		TacticalWorldObserverLimits limits = {}) noexcept;

	TacticalWorldObserverUpdateResult update() noexcept;
	TacticalWorldPublicationView latest() const noexcept override;
	const TacticalWorldObserverLimits& limits() const noexcept { return limits_; }

private:
	struct Publication
	{
		TacticalWorldPublicationStatus status = TacticalWorldPublicationStatus::Unavailable;
		std::uint64_t serial = 0;
		TacticalWorldSnapshot snapshot;
		TacticalWorldDelta delta;
	};

	TacticalWorldService& source_;
	TacticalWorldObserverLimits limits_;
	Publication publication_;
};

#endif
