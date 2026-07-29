#include <Engine/Adapters/JA2/TacticalEntityRoster.h>
#include <Engine/Adapters/JA2/TacticalWorldObserver.h>
#include <Engine/Adapters/JA2/TacticalWorldService.h>
#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <optional>
#include <variant>
#include <vector>

namespace allocation_probe
{
std::atomic<bool> enabled{false};
std::atomic<std::size_t> count{0};

void* allocate(std::size_t size)
{
	void* memory = std::malloc(size == 0 ? 1 : size);
	if (!memory) throw std::bad_alloc();
	if (enabled.load(std::memory_order_relaxed))
		count.fetch_add(1, std::memory_order_relaxed);
	return memory;
}
}

void* operator new(std::size_t size)
{
	return allocation_probe::allocate(size);
}

void* operator new[](std::size_t size)
{
	return allocation_probe::allocate(size);
}

void operator delete(void* memory) noexcept
{
	std::free(memory);
}

void operator delete[](void* memory) noexcept
{
	std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
	std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
	std::free(memory);
}

namespace
{
int failures = 0;

void check(bool condition, const char* message)
{
	if (!condition)
	{
		++failures;
		std::printf("FAIL  %s\n", message);
		return;
	}
	std::printf("ok    %s\n", message);
}

class ReusableTacticalWorldService final : public TacticalWorldService
{
public:
	explicit ReusableTacticalWorldService(std::size_t actorCount)
		: maximumActors_(actorCount)
	{
		actorScratch_.reserve(actorCount);
		actors_.reserve(actorCount);
		for (std::size_t slot = 0; slot < actorCount; ++slot)
		{
			actors_.push_back(TacticalActorSnapshot{
				TacticalEntityId{
					static_cast<std::uint16_t>(slot),
					static_cast<std::uint32_t>(1000 + slot)},
				0, static_cast<std::uint16_t>(slot),
				static_cast<std::int32_t>(100 + slot), 0, 0, 1,
				TacticalStance::Standing, 20, 80, 80, 90, 90,
				true, true});
		}
	}

	void advance() noexcept
	{
		++frame_;
		TacticalActorSnapshot& actor = actors_.front();
		++actor.grid;
		actor.direction = static_cast<std::uint8_t>(frame_ % 8);
		actor.actionPoints = static_cast<std::int16_t>(20 + frame_ % 40);
	}

	void toggleStance() noexcept
	{
		TacticalActorSnapshot& actor = actors_.front();
		actor.stance = actor.stance == TacticalStance::Standing
			? TacticalStance::Crouched
			: TacticalStance::Standing;
		++actor.animation;
	}

	void fail(TacticalWorldCaptureResult result) noexcept
	{
		result_ = result;
	}

	void beginWorld(std::uint64_t epoch) noexcept
	{
		epoch_ = epoch;
		frame_ = 1;
		result_ = TacticalWorldCaptureResult::Success;
	}

	TacticalWorldCaptureResult capture(TacticalWorldSnapshot& output) noexcept override
	{
		if (result_ != TacticalWorldCaptureResult::Success) return result_;
		try
		{
			actorScratch_.clear();
			for (const TacticalActorSnapshot& actor : actors_)
				actorScratch_.push_back(actor);
			const TacticalSnapshotCreateError result =
				TacticalWorldSnapshot::createReusableOrdered(
					epoch_,
					TacticalSectorSnapshot{1, 1, 0, true},
					TacticalTurnSnapshot{true, true, 0, frame_},
					actorScratch_, output, maximumActors_);
			if (result == TacticalSnapshotCreateError::TooManyActors)
				return TacticalWorldCaptureResult::CapacityReached;
			return result == TacticalSnapshotCreateError::None
				? TacticalWorldCaptureResult::Success
				: TacticalWorldCaptureResult::AdapterFailure;
		}
		catch (...)
		{
			return TacticalWorldCaptureResult::AllocationFailure;
		}
	}

private:
	std::size_t maximumActors_;
	std::uint64_t epoch_ = 77;
	std::uint64_t frame_ = 1;
	std::vector<TacticalActorSnapshot> actors_;
	std::vector<TacticalActorSnapshot> actorScratch_;
	TacticalWorldCaptureResult result_ = TacticalWorldCaptureResult::Success;
};

bool HasDeterministicSteadyStateEvents(const TacticalWorldPublicationView& publication)
{
	return publication && publication.delta->events.size() == 3 &&
		std::holds_alternative<TacticalTurnChangedEvent>(publication.delta->events[0]) &&
		std::holds_alternative<TacticalActorMovedEvent>(publication.delta->events[1]) &&
		std::holds_alternative<TacticalActorVitalsChangedEvent>(publication.delta->events[2]);
}
}

int main()
{
	constexpr std::size_t ActorCount = 16;
	constexpr std::size_t EventLimit = 3;
	TacticalEntityRoster roster(ActorCount);
	std::array<TacticalEntityId, ActorCount> rosterActors;
	for (std::size_t slot = 0; slot < ActorCount; ++slot)
	{
		rosterActors[slot] = TacticalEntityId{
			static_cast<std::uint16_t>(slot),
			static_cast<std::uint32_t>(2000 + slot)};
	}
	bool stableRoster = true;
	allocation_probe::count.store(0, std::memory_order_relaxed);
	allocation_probe::enabled.store(true, std::memory_order_relaxed);
	for (std::size_t slot = 0; slot < ActorCount; ++slot)
	{
		const auto inserted = roster.insert(rosterActors[slot]);
		if (!inserted || *inserted != slot)
		{
			stableRoster = false;
			break;
		}
	}
	for (std::size_t iteration = 0;
		stableRoster && iteration < 4096; ++iteration)
	{
		const std::size_t slot = iteration % ActorCount;
		if (!roster.erase(rosterActors[slot]) ||
			roster.insert(rosterActors[slot]) !=
				std::optional<TacticalEntityRoster::Slot>{slot})
		{
			stableRoster = false;
		}
	}
	allocation_probe::enabled.store(false, std::memory_order_relaxed);
	check(stableRoster && roster.full() &&
		allocation_probe::count.load(std::memory_order_relaxed) == 0,
		"preallocated tactical roster mutation performs zero heap allocations");

	ReusableTacticalWorldService source(ActorCount);
	TacticalWorldObserver observer(
		source, TacticalWorldObserverLimits{ActorCount, EventLimit});

	check(observer.update() == TacticalWorldObserverUpdateResult::PublishedBaseline,
		"the reusable observer establishes its first publication slot");
	const TacticalWorldPublicationView baseline = observer.latest();
	if (!baseline) return 1;
	const TacticalWorldSnapshot* slotZeroSnapshot = baseline.snapshot;

	source.advance();
	check(observer.update() == TacticalWorldObserverUpdateResult::PublishedDelta,
		"the reusable observer warms its second publication slot");
	const TacticalWorldPublicationView slotOne = observer.latest();
	const TacticalWorldSnapshot* slotOneSnapshot = slotOne.snapshot;
	const TacticalWorldDelta* slotOneDelta = slotOne.delta;
	const TacticalWorldEvent* slotOneEvents = slotOne.delta->events.data();

	source.advance();
	check(observer.update() == TacticalWorldObserverUpdateResult::PublishedDelta,
		"the reusable observer warms diff storage in its first publication slot");
	const TacticalWorldPublicationView slotZero = observer.latest();
	const TacticalWorldDelta* slotZeroDelta = slotZero.delta;
	const TacticalWorldEvent* slotZeroEvents = slotZero.delta->events.data();
	check(slotZero.snapshot == slotZeroSnapshot && slotZeroSnapshot != slotOneSnapshot &&
		slotZero.delta != slotOneDelta && HasDeterministicSteadyStateEvents(slotZero),
		"successful updates alternate two independently owned publication buffers");

	allocation_probe::count.store(0, std::memory_order_relaxed);
	allocation_probe::enabled.store(true, std::memory_order_relaxed);
	const TacticalWorldObserverUpdateResult unchangedResult = observer.update();
	allocation_probe::enabled.store(false, std::memory_order_relaxed);
	const TacticalWorldPublicationView afterUnchanged = observer.latest();
	check(unchangedResult == TacticalWorldObserverUpdateResult::Unchanged &&
		afterUnchanged.snapshot == slotZero.snapshot &&
		afterUnchanged.delta == slotZero.delta &&
		afterUnchanged.serial == slotZero.serial &&
		allocation_probe::count.load(std::memory_order_relaxed) == 0,
		"unchanged ordered captures allocate nothing and retain the last publication");
	constexpr std::size_t Iterations = 4096;
	bool stableLoop = true;
	allocation_probe::count.store(0, std::memory_order_relaxed);
	allocation_probe::enabled.store(true, std::memory_order_relaxed);
	for (std::size_t iteration = 0; iteration < Iterations; ++iteration)
	{
		source.advance();
		if (observer.update() != TacticalWorldObserverUpdateResult::PublishedDelta)
		{
			stableLoop = false;
			break;
		}
		const TacticalWorldPublicationView publication = observer.latest();
		const bool usesSlotOne = iteration % 2 == 0;
		const TacticalWorldSnapshot* expectedSnapshot =
			usesSlotOne ? slotOneSnapshot : slotZeroSnapshot;
		const TacticalWorldDelta* expectedDelta =
			usesSlotOne ? slotOneDelta : slotZeroDelta;
		const TacticalWorldEvent* expectedEvents =
			usesSlotOne ? slotOneEvents : slotZeroEvents;
		if (publication.snapshot != expectedSnapshot ||
			publication.delta != expectedDelta ||
			publication.snapshot->actors().size() != ActorCount ||
			publication.snapshot->actors().capacity() < ActorCount ||
			publication.delta->events.data() != expectedEvents ||
			publication.serial != iteration + 4 ||
			!HasDeterministicSteadyStateEvents(publication))
		{
			stableLoop = false;
			break;
		}
	}
	allocation_probe::enabled.store(false, std::memory_order_relaxed);
	const std::size_t steadyStateAllocations =
		allocation_probe::count.load(std::memory_order_relaxed);
	check(stableLoop && steadyStateAllocations == 0,
		"4096 changing tactical observations perform zero heap allocations after warmup");

	const TacticalWorldPublicationView lastGood = observer.latest();
	const std::uint64_t lastGoodSerial = lastGood.serial;
	const std::int32_t lastGoodGrid = lastGood.snapshot->actors().front().grid;
	const TacticalActorSnapshot* lastGoodActors = lastGood.snapshot->actors().data();
	const TacticalWorldEvent* lastGoodEvents = lastGood.delta->events.data();
	source.advance();
	source.toggleStance();
	check(observer.update() == TacticalWorldObserverUpdateResult::EventCapacityReached,
		"an over-capacity diff is rejected after reusable capture");
	TacticalWorldPublicationView preserved = observer.latest();
	check(preserved.snapshot == lastGood.snapshot && preserved.delta == lastGood.delta &&
		preserved.serial == lastGoodSerial &&
		preserved.snapshot->actors().data() == lastGoodActors &&
		preserved.delta->events.data() == lastGoodEvents &&
		preserved.snapshot->actors().front().grid == lastGoodGrid &&
		HasDeterministicSteadyStateEvents(preserved),
		"diff failure preserves the complete last-good view and its backing storage");

	source.fail(TacticalWorldCaptureResult::AdapterFailure);
	check(observer.update() == TacticalWorldObserverUpdateResult::SourceAdapterFailure,
		"a source failure is reported without attempting publication");
	preserved = observer.latest();
	check(preserved.snapshot == lastGood.snapshot && preserved.delta == lastGood.delta &&
		preserved.serial == lastGoodSerial &&
		preserved.snapshot->actors().data() == lastGoodActors &&
		preserved.delta->events.data() == lastGoodEvents &&
		preserved.snapshot->actors().front().grid == lastGoodGrid,
		"capture failure keeps prior publication views valid and structurally stable");

	source.fail(TacticalWorldCaptureResult::Unavailable);
	check(observer.update() == TacticalWorldObserverUpdateResult::SourceUnavailable &&
		!observer.latest(),
		"world unavailability invalidates the old publication and its serial");
	source.beginWorld(88);
	bool resetLoop = true;
	allocation_probe::count.store(0, std::memory_order_relaxed);
	allocation_probe::enabled.store(true, std::memory_order_relaxed);
	if (observer.update() != TacticalWorldObserverUpdateResult::PublishedBaseline)
		resetLoop = false;
	TacticalWorldPublicationView resetPublication = observer.latest();
	if (!resetPublication || resetPublication.serial != 1 ||
		resetPublication.snapshot->epoch() != 88 ||
		resetPublication.snapshot->turn().serial == 0 ||
		resetPublication.snapshot != slotZeroSnapshot ||
		resetPublication.snapshot->actors().capacity() < ActorCount)
		resetLoop = false;
	source.advance();
	if (observer.update() != TacticalWorldObserverUpdateResult::PublishedDelta)
		resetLoop = false;
	resetPublication = observer.latest();
	if (!resetPublication || resetPublication.serial != 2 ||
		resetPublication.snapshot != slotOneSnapshot ||
		resetPublication.delta != slotOneDelta ||
		resetPublication.snapshot->actors().capacity() < ActorCount ||
		resetPublication.delta->events.data() != slotOneEvents ||
		!HasDeterministicSteadyStateEvents(resetPublication))
		resetLoop = false;
	constexpr std::size_t PostResetIterations = 1024;
	for (std::size_t iteration = 0; iteration < PostResetIterations; ++iteration)
	{
		source.advance();
		if (observer.update() != TacticalWorldObserverUpdateResult::PublishedDelta)
		{
			resetLoop = false;
			break;
		}
		resetPublication = observer.latest();
		const bool usesSlotZero = iteration % 2 == 0;
		if (resetPublication.serial != iteration + 3 ||
			resetPublication.snapshot !=
				(usesSlotZero ? slotZeroSnapshot : slotOneSnapshot) ||
			resetPublication.delta !=
				(usesSlotZero ? slotZeroDelta : slotOneDelta) ||
			resetPublication.snapshot->actors().capacity() < ActorCount ||
			resetPublication.delta->events.data() !=
				(usesSlotZero ? slotZeroEvents : slotOneEvents) ||
			!HasDeterministicSteadyStateEvents(resetPublication))
		{
			resetLoop = false;
			break;
		}
	}
	source.beginWorld(89);
	if (observer.update() != TacticalWorldObserverUpdateResult::PublishedDelta)
		resetLoop = false;
	resetPublication = observer.latest();
	if (!resetPublication ||
		resetPublication.serial != PostResetIterations + 3 ||
		resetPublication.snapshot != slotZeroSnapshot ||
		resetPublication.delta != slotZeroDelta ||
		resetPublication.snapshot->epoch() != 89 ||
		resetPublication.snapshot->turn().serial == 0 ||
		resetPublication.delta->events.data() != slotZeroEvents ||
		resetPublication.delta->events.size() != 1 ||
		!std::holds_alternative<TacticalWorldResetEvent>(
			resetPublication.delta->events[0]))
		resetLoop = false;
	else
	{
		const TacticalWorldResetEvent& reset =
			std::get<TacticalWorldResetEvent>(resetPublication.delta->events[0]);
		if (reset.previousEpoch != 88 || reset.currentEpoch != 89)
			resetLoop = false;
	}
	allocation_probe::enabled.store(false, std::memory_order_relaxed);
	const std::size_t postResetAllocations =
		allocation_probe::count.load(std::memory_order_relaxed);
	check(resetLoop && postResetAllocations == 0,
		"world baselines and nonzero identity resets retain warmed observer buffers");

	return failures == 0 ? 0 : 1;
}
