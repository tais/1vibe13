#include <Engine/Adapters/JA2/TacticalWorldObserver.h>

#include <limits>

namespace
{
TacticalWorldObserverUpdateResult MapCaptureFailure(TacticalWorldCaptureResult result)
{
	switch (result)
	{
		case TacticalWorldCaptureResult::Unavailable:
			return TacticalWorldObserverUpdateResult::SourceUnavailable;
		case TacticalWorldCaptureResult::CapacityReached:
			return TacticalWorldObserverUpdateResult::SourceCapacityReached;
		case TacticalWorldCaptureResult::AllocationFailure:
			return TacticalWorldObserverUpdateResult::SourceAllocationFailure;
		case TacticalWorldCaptureResult::AdapterFailure:
			return TacticalWorldObserverUpdateResult::SourceAdapterFailure;
		case TacticalWorldCaptureResult::Success:
			break;
	}
	return TacticalWorldObserverUpdateResult::SourceAdapterFailure;
}
}

TacticalWorldObserver::TacticalWorldObserver(
	TacticalWorldService& source, TacticalWorldObserverLimits limits) noexcept
	: source_(source), limits_(limits)
{
}

TacticalWorldObserverUpdateResult TacticalWorldObserver::update() noexcept
{
	const std::size_t scratchIndex = hasPublication_ ? 1 - activePublication_ : 0;
	Publication& accepted = publications_[scratchIndex];
	const TacticalWorldCaptureResult captureResult = source_.capture(accepted.snapshot);
	if (captureResult != TacticalWorldCaptureResult::Success)
	{
		if (captureResult == TacticalWorldCaptureResult::Unavailable) reset();
		return MapCaptureFailure(captureResult);
	}
	if (accepted.snapshot.epoch() == 0)
		return TacticalWorldObserverUpdateResult::InvalidSnapshot;
	if (accepted.snapshot.actors().size() > limits_.maximumActors)
		return TacticalWorldObserverUpdateResult::ActorCapacityReached;
	if (accepted.snapshot.doors().size() > limits_.maximumDoors)
		return TacticalWorldObserverUpdateResult::DoorCapacityReached;
	const std::uint64_t previousSerial = hasPublication_
		? publications_[activePublication_].serial
		: 0;

	if (!hasPublication_)
	{
		accepted.status = TacticalWorldPublicationStatus::Baseline;
		accepted.serial = 1;
		accepted.delta.previousEpoch = accepted.snapshot.epoch();
		accepted.delta.currentEpoch = accepted.snapshot.epoch();
		accepted.delta.events.clear();
		activePublication_ = scratchIndex;
		hasPublication_ = true;
		return TacticalWorldObserverUpdateResult::PublishedBaseline;
	}

	const Publication& previous = publications_[activePublication_];
	const TacticalWorldDiffResult diffResult = DiffTacticalWorldSnapshots(
		previous.snapshot, accepted.snapshot, limits_.maximumEvents, accepted.delta);
	switch (diffResult)
	{
		case TacticalWorldDiffResult::InvalidSnapshot:
			return TacticalWorldObserverUpdateResult::InvalidSnapshot;
		case TacticalWorldDiffResult::CapacityReached:
			return TacticalWorldObserverUpdateResult::EventCapacityReached;
		case TacticalWorldDiffResult::AllocationFailure:
			return TacticalWorldObserverUpdateResult::AllocationFailure;
		case TacticalWorldDiffResult::Success:
			break;
	}
	if (accepted.delta.events.empty())
		return TacticalWorldObserverUpdateResult::Unchanged;
	if (previousSerial == std::numeric_limits<std::uint64_t>::max())
		return TacticalWorldObserverUpdateResult::SerialExhausted;

	accepted.status = TacticalWorldPublicationStatus::Delta;
	accepted.serial = previousSerial + 1;
	activePublication_ = scratchIndex;
	return TacticalWorldObserverUpdateResult::PublishedDelta;
}

void TacticalWorldObserver::reset() noexcept
{
	// Logical invalidation is enough: neither slot is observable while
	// hasPublication_ is false. Retaining both slots preserves the actor and
	// event allocations for the next world without exposing stale state.
	hasPublication_ = false;
	activePublication_ = 0;
}

TacticalWorldPublicationView TacticalWorldObserver::latest() const noexcept
{
	if (!hasPublication_) return {};
	const Publication& publication = publications_[activePublication_];
	return TacticalWorldPublicationView{
		publication.status, publication.serial,
		&publication.snapshot, &publication.delta};
}
