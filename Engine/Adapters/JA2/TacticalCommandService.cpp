#include <Engine/Adapters/JA2/TacticalCommandService.h>

#include <algorithm>
#include <limits>
#include <utility>

#include <Engine/Core/Identifier.h>

namespace
{
TacticalCommandSubmissionError ValidateSubmission(
	const std::string& packageId, const SimulationCommand& command,
	std::size_t maximumOwnerBytes)
{
	if (packageId.size() > maximumOwnerBytes ||
		!IsValidEngineIdentifier(packageId))
		return TacticalCommandSubmissionError::InvalidOwner;
	if (!IsStructurallyValidSimulationCommand(command))
		return TacticalCommandSubmissionError::InvalidCommand;
	return TacticalCommandSubmissionError::None;
}

constexpr std::size_t NullMaximumOwnerBytes = 256;
}

TacticalCommandInbox::TacticalCommandInbox(
	TacticalCommandInboxLimits limits) noexcept
	: limits_(limits), sequenceExhausted_(limits.maximumRequestId == 0)
{
}

void TacticalCommandInbox::SaturatingIncrement(std::uint64_t& value) noexcept
{
	if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

void TacticalCommandInbox::SaturatingAdd(
	std::uint64_t& value, std::size_t amount) noexcept
{
	const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
	const std::uint64_t remaining = maximum - value;
	if (amount >= remaining)
		value = maximum;
	else
		value += static_cast<std::uint64_t>(amount);
}

TacticalCommandSubmissionResult TacticalCommandInbox::submit(
	const std::string& packageId, const SimulationCommand& command) noexcept
{
	const TacticalCommandSubmissionError validation =
		ValidateSubmission(packageId, command, limits_.maximumOwnerBytes);
	if (validation != TacticalCommandSubmissionError::None)
		return TacticalCommandSubmissionResult{validation, 0};
	if (pending_.size() >= limits_.maximumPending)
		return TacticalCommandSubmissionResult{
			TacticalCommandSubmissionError::CapacityReached, 0};
	if (sequenceExhausted_)
		return TacticalCommandSubmissionResult{
			TacticalCommandSubmissionError::SequenceExhausted, 0};

	const std::uint64_t requestId = nextRequestId_;
	try
	{
		pending_.push_back(TacticalCommandRequest{
			requestId, packageId, command});
	}
	catch (...)
	{
		return TacticalCommandSubmissionResult{
			TacticalCommandSubmissionError::AllocationFailure, 0};
	}

	if (requestId == limits_.maximumRequestId)
	{
		nextRequestId_ = 0;
		sequenceExhausted_ = true;
	}
	else
		++nextRequestId_;
	SaturatingIncrement(counters_.submitted);
	return TacticalCommandSubmissionResult{
		TacticalCommandSubmissionError::None, requestId};
}

TacticalCommandCancellationResult TacticalCommandInbox::cancelPackage(
	const std::string& packageId,
	TacticalCommandCancellationSink* sink) noexcept
{
	if (packageId.size() > limits_.maximumOwnerBytes ||
		!IsValidEngineIdentifier(packageId))
		return TacticalCommandCancellationResult{
			TacticalCommandCancellationError::InvalidOwner, 0};
	if (draining_)
		return TacticalCommandCancellationResult{
			TacticalCommandCancellationError::DrainInProgress, 0};
	if (cancelling_)
		return TacticalCommandCancellationResult{
			TacticalCommandCancellationError::CancellationInProgress, 0};

	struct CancellationGuard
	{
		explicit CancellationGuard(bool& active) : active_(active) { active_ = true; }
		~CancellationGuard() { active_ = false; }
		bool& active_;
	} guard(cancelling_);

	// The cancellation sink may submit new work. Reacquire deque positions after
	// every callback and visit only the prefix present on entry, so append-time
	// iterator invalidation cannot become memory corruption and new work remains
	// eligible for a later explicit cancellation.
	std::size_t remainingInitial = pending_.size();
	std::size_t index = 0;
	std::size_t cancelled = 0;
	while (index < remainingInitial)
	{
		if (pending_[index].packageId != packageId)
		{
			++index;
			continue;
		}
		if (sink) sink->commandCancelled(pending_[index]);
		pending_.erase(pending_.begin() + static_cast<std::ptrdiff_t>(index));
		--remainingInitial;
		++cancelled;
	}
	SaturatingAdd(counters_.cancelled, cancelled);
	return TacticalCommandCancellationResult{
		TacticalCommandCancellationError::None, cancelled};
}

TacticalCommandInboxSummary TacticalCommandInbox::summary() const noexcept
{
	TacticalCommandInboxSummary result = counters_;
	result.pending = pending_.size();
	result.nextRequestId = nextRequestId_;
	result.sequenceExhausted = sequenceExhausted_;
	result.draining = draining_;
	return result;
}

TacticalCommandSnapshotError TacticalCommandInbox::snapshot(
	TacticalCommandInboxSnapshot& output) const noexcept
{
	try
	{
		TacticalCommandInboxSnapshot captured;
		captured.limits = limits_;
		captured.summary = summary();
		const std::size_t count =
			std::min(pending_.size(), limits_.maximumDiagnosticEntries);
		captured.pending.reserve(count);
		for (std::size_t index = 0; index < count; ++index)
			captured.pending.push_back(pending_[index]);
		captured.omitted = pending_.size() - count;
		output = std::move(captured);
		return TacticalCommandSnapshotError::None;
	}
	catch (...)
	{
		return TacticalCommandSnapshotError::AllocationFailure;
	}
}

TacticalCommandInbox& TacticalCommandInbox::disabled()
{
	static TacticalCommandInbox inbox(TacticalCommandInboxLimits{
		0, 0, 0, NullMaximumOwnerBytes,
		std::numeric_limits<std::uint64_t>::max()});
	return inbox;
}

TacticalCommandSubmissionResult NullTacticalCommandService::submit(
	const std::string& packageId, const SimulationCommand& command) noexcept
{
	const TacticalCommandSubmissionError validation =
		ValidateSubmission(packageId, command, NullMaximumOwnerBytes);
	if (validation != TacticalCommandSubmissionError::None)
		return TacticalCommandSubmissionResult{validation, 0};
	return TacticalCommandSubmissionResult{
		TacticalCommandSubmissionError::CapacityReached, 0};
}

TacticalCommandCancellationResult NullTacticalCommandService::cancelPackage(
	const std::string& packageId) noexcept
{
	if (packageId.size() > NullMaximumOwnerBytes ||
		!IsValidEngineIdentifier(packageId))
		return TacticalCommandCancellationResult{
			TacticalCommandCancellationError::InvalidOwner, 0};
	return TacticalCommandCancellationResult{
		TacticalCommandCancellationError::None, 0};
}

TacticalCommandInboxLimits NullTacticalCommandService::limits() const noexcept
{
	return TacticalCommandInboxLimits{
		0, 0, 0, NullMaximumOwnerBytes,
		std::numeric_limits<std::uint64_t>::max()};
}

TacticalCommandInboxSummary NullTacticalCommandService::summary() const noexcept
{
	return TacticalCommandInboxSummary{};
}

TacticalCommandSnapshotError NullTacticalCommandService::snapshot(
	TacticalCommandInboxSnapshot& output) const noexcept
{
	TacticalCommandInboxSnapshot captured;
	captured.limits = limits();
	captured.summary = summary();
	output = std::move(captured);
	return TacticalCommandSnapshotError::None;
}
