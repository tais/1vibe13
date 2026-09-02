#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_OBSERVER_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_OBSERVER_H

#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/TacticalWorldDelta.h>
#include <Engine/Adapters/JA2/TacticalWorldService.h>

inline constexpr const char* TacticalWorldObserverServiceId =
	"ja2.tactical-world-observer";
inline constexpr EngineServiceVersion TacticalWorldObserverServiceVersion{2, 0};

struct TacticalWorldObserverLimits
{
	std::size_t maximumActors = TacticalWorldSnapshot::DefaultMaximumActors;
	std::size_t maximumEvents = TacticalWorldSnapshot::DefaultMaximumActors * 4 +
		TacticalWorldSnapshot::DefaultMaximumDoors * 2 + 2;
	// Appended to retain the established two-field aggregate initialization.
	std::size_t maximumDoors = TacticalWorldSnapshot::DefaultMaximumDoors;
};

enum class TacticalWorldObserverUpdateResult
{
	PublishedBaseline = 0,
	PublishedDelta = 1,
	SourceUnavailable = 2,
	SourceCapacityReached = 3,
	SourceAllocationFailure = 4,
	SourceAdapterFailure = 5,
	InvalidSnapshot = 6,
	ActorCapacityReached = 7,
	EventCapacityReached = 8,
	AllocationFailure = 9,
	SerialExhausted = 10,
	Unchanged = 11,
	DoorCapacityReached = 12
};

static_assert(static_cast<int>(TacticalWorldObserverUpdateResult::PublishedBaseline) == 0 &&
	static_cast<int>(TacticalWorldObserverUpdateResult::PublishedDelta) == 1 &&
	static_cast<int>(TacticalWorldObserverUpdateResult::SourceUnavailable) == 2 &&
	static_cast<int>(TacticalWorldObserverUpdateResult::SourceCapacityReached) == 3 &&
	static_cast<int>(TacticalWorldObserverUpdateResult::SourceAllocationFailure) == 4 &&
	static_cast<int>(TacticalWorldObserverUpdateResult::SourceAdapterFailure) == 5 &&
	static_cast<int>(TacticalWorldObserverUpdateResult::InvalidSnapshot) == 6 &&
	static_cast<int>(TacticalWorldObserverUpdateResult::ActorCapacityReached) == 7 &&
	static_cast<int>(TacticalWorldObserverUpdateResult::EventCapacityReached) == 8 &&
	static_cast<int>(TacticalWorldObserverUpdateResult::AllocationFailure) == 9 &&
	static_cast<int>(TacticalWorldObserverUpdateResult::SerialExhausted) == 10 &&
	static_cast<int>(TacticalWorldObserverUpdateResult::Unchanged) == 11 &&
	static_cast<int>(TacticalWorldObserverUpdateResult::DoorCapacityReached) == 12,
	"tactical observer SDK result values are a stable compatibility contract");

enum class TacticalWorldPublicationStatus
{
	Unavailable,
	Baseline,
	Delta
};

// A read-only view of one transactionally accepted observer publication. The
// pointers remain valid until the observer's next successful update(), reset,
// source-unavailable update, or destruction. Packages should consume or copy
// the view on the same main-thread safe-frame boundary that drives the observer.
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

inline constexpr EngineServiceContract<TacticalWorldObserverService>
	TacticalWorldObserverServiceContract{
		TacticalWorldObserverServiceId, TacticalWorldObserverServiceVersion};

inline EngineServiceRegistrationError RegisterTacticalWorldObserverService(
	ServiceCatalog& catalog, TacticalWorldObserverService& service) noexcept
{
	return catalog.registerService(TacticalWorldObserverServiceContract, service);
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
// It never starts a worker or mutates the source service. Explicit source
// failures and rejected diffs leave the complete last good publication
// untouched. Source unavailability is a world-lifecycle boundary and clears
// the publication so packages cannot mistake an unloaded world for live state.
// Successful updates alternate between two owned publication slots; resets
// invalidate them logically while retaining their allocations, so optimized
// sources and the delta builder remain allocation-free after warmup.
class TacticalWorldObserver final : public TacticalWorldObserverService
{
public:
	explicit TacticalWorldObserver(
		TacticalWorldService& source,
		TacticalWorldObserverLimits limits = {}) noexcept;

	TacticalWorldObserverUpdateResult update() noexcept;
	void reset() noexcept;
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
	Publication publications_[2];
	std::size_t activePublication_ = 0;
	bool hasPublication_ = false;
};

#endif
