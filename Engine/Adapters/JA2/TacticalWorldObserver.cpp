#include <Engine/Adapters/JA2/TacticalWorldObserver.h>

#include <limits>
#include <utility>

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
	TacticalWorldSnapshot captured;
	const TacticalWorldCaptureResult captureResult = source_.capture(captured);
	if (captureResult != TacticalWorldCaptureResult::Success)
		return MapCaptureFailure(captureResult);
	if (captured.epoch() == 0)
		return TacticalWorldObserverUpdateResult::InvalidSnapshot;
	if (captured.actors().size() > limits_.maximumActors)
		return TacticalWorldObserverUpdateResult::ActorCapacityReached;
	if (publication_.serial == std::numeric_limits<std::uint64_t>::max())
		return TacticalWorldObserverUpdateResult::SerialExhausted;

	Publication accepted;
	accepted.serial = publication_.serial + 1;
	accepted.snapshot = std::move(captured);

	if (publication_.status == TacticalWorldPublicationStatus::Unavailable)
	{
		accepted.status = TacticalWorldPublicationStatus::Baseline;
		accepted.delta.previousEpoch = accepted.snapshot.epoch();
		accepted.delta.currentEpoch = accepted.snapshot.epoch();
		publication_ = std::move(accepted);
		return TacticalWorldObserverUpdateResult::PublishedBaseline;
	}

	const TacticalWorldDiffResult diffResult = DiffTacticalWorldSnapshots(
		publication_.snapshot, accepted.snapshot, limits_.maximumEvents, accepted.delta);
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

	accepted.status = TacticalWorldPublicationStatus::Delta;
	publication_ = std::move(accepted);
	return TacticalWorldObserverUpdateResult::PublishedDelta;
}

TacticalWorldPublicationView TacticalWorldObserver::latest() const noexcept
{
	if (publication_.status == TacticalWorldPublicationStatus::Unavailable)
		return {};
	return TacticalWorldPublicationView{
		publication_.status, publication_.serial,
		&publication_.snapshot, &publication_.delta};
}
