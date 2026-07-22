#include "TacticalCommandHost.h"

#include <array>
#include <limits>
#include <string>
#include <type_traits>
#include <variant>

#include "GameContext.h"
#include "Map Information.h"
#include "Overhead.h"
#include "Simulation Commands.h"

namespace
{
void IncrementSaturated(std::uint64_t& value) noexcept
{
	if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

void AddSaturated(std::uint64_t& value, std::size_t amount) noexcept
{
	const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
	const std::uint64_t available = maximum - value;
	if (amount >= available)
		value = maximum;
	else
		value += static_cast<std::uint64_t>(amount);
}

bool RequiresCommandCancellation(PackageEventKind kind) noexcept
{
	switch (kind)
	{
		case PackageEventKind::BootstrapFailed:
		case PackageEventKind::BootstrapRollbackCompleted:
		case PackageEventKind::BootstrapRollbackFailed:
		case PackageEventKind::ShutdownCompleted:
		case PackageEventKind::ShutdownFailed:
		case PackageEventKind::Deactivated:
		case PackageEventKind::Unregistered:
			return true;
		case PackageEventKind::Registered:
		case PackageEventKind::Activated:
		case PackageEventKind::BootstrapCompleted:
			return false;
	}
	return false;
}

bool HasTacticalExecutionContext(const SimulationCommand& command) noexcept
{
	if (!gfWorldLoaded || command.valueless_by_exception()) return false;
	return std::visit([](const auto& value) noexcept {
		using Command = typename std::decay<decltype(value)>::type;
		if constexpr (std::is_same<Command, EndTurnCommand>::value)
		{
			constexpr UINT32 RequiredFlags = TURNBASED | INCOMBAT;
			return (gTacticalStatus.uiFlags & RequiredFlags) == RequiredFlags &&
				gTacticalStatus.ubCurrentTeam < MAXTEAMS;
		}
		return true;
	}, command);
}

class Ja2TacticalCommandHost final
	: public PackageEventSink,
	  private SimulationCommandExecutionSink,
	  private TacticalCommandCancellationSink
{
private:
	static constexpr std::size_t MaximumPendingCommands = 1024;
	static constexpr std::size_t MaximumCommandsPerFrame = 64;
	static constexpr std::size_t MaximumDiagnosticCommands = 128;
	static constexpr std::size_t MaximumOwnerBytes = 256;
	// One complete prior receipt backlog, one full package inbox cancellation,
	// and the currently authoritative frame all remain finite and independent
	// of allocator or runtime-message pressure.
	static constexpr std::size_t MaximumPendingReceipts =
		MaximumPendingCommands * 2 + MaximumCommandsPerFrame;
	// Normal admission may consume the prior backlog and current-frame shares,
	// but must leave room to terminally cancel a completely full package inbox.
	static constexpr std::size_t MaximumAdmittedReceiptObligations =
		MaximumPendingReceipts - MaximumPendingCommands;

public:
	Ja2TacticalCommandHost() noexcept
		: inbox_(TacticalCommandInboxLimits{
			MaximumPendingCommands, MaximumCommandsPerFrame,
			MaximumDiagnosticCommands, MaximumOwnerBytes,
			std::numeric_limits<std::uint64_t>::max()})
	{
	}

	TacticalCommandService& service() noexcept { return inbox_; }
	bool bind(GameContext& game) noexcept
	{
		if (game_ && game_ != &game)
		{
			IncrementSaturated(diagnostics_.bindingFailures);
			return false;
		}
		if (!BindSimulationCommandExecutionSink(*this))
		{
			IncrementSaturated(diagnostics_.bindingFailures);
			return false;
		}
		game_ = &game;
		BeginSimulationCommandFrameBudget(
			game.frameDriver().completedFrames() + 1, MaximumCommandsPerFrame);
		return true;
	}

	void publish(PackageEvent event) noexcept override
	{
		if (!RequiresCommandCancellation(event.kind)) return;
		IncrementSaturated(diagnostics_.lifecycleCancellationEvents);
		if (game_) flushReceipts(*game_);
		diagnostics_.lastCancellation = inbox_.cancelPackage(event.packageId, this);
		if (diagnostics_.lastCancellation)
			AddSaturated(
				diagnostics_.cancelledRequests,
				diagnostics_.lastCancellation.cancelled);
		else if (diagnostics_.lastCancellation.error ==
				TacticalCommandCancellationError::DrainInProgress ||
			diagnostics_.lastCancellation.error ==
				TacticalCommandCancellationError::CancellationInProgress)
		{
			if (queueDeferredCancellation(event.packageId))
				IncrementSaturated(diagnostics_.deferredCancellations);
			else
			{
				IncrementSaturated(diagnostics_.deferredCancellationDrops);
				IncrementSaturated(diagnostics_.cancellationFailures);
			}
		}
		else
			IncrementSaturated(diagnostics_.cancellationFailures);
		cancelAuthoritative(event.packageId);
		if (game_) flushReceipts(*game_);
	}

	void drainAtSafeFrame(
		GameContext& game, std::size_t maximumCommands) noexcept
	{
		if (game_ != &game)
		{
			IncrementSaturated(diagnostics_.bindingFailures);
			return;
		}
		IncrementSaturated(diagnostics_.safeFrameCalls);
		diagnostics_.simulationTick =
			game.runtime().simulationTicks().completedTickSequence();
		flushReceipts(game);
		flushDeferredCancellations();
		flushReceipts(game);
		const bool explicitlyNoExecution = maximumCommands == 0;
		maximumCommands =
			RemainingSimulationCommandFrameBudget(maximumCommands);

		// A retained authoritative command always gets the complete bounded
		// budget. Even a successful retry consumes this safe frame so recovery
		// cannot combine backlog work with another admission batch.
		if (diagnostics_.authoritativeBackpressure || !game.commands().empty())
		{
			diagnostics_.authoritativeBackpressure = true;
			IncrementSaturated(diagnostics_.backpressureFrames);
			processAuthoritative(game, maximumCommands);
			flushReceipts(game);
			return;
		}
		// An explicit zero-budget composition test may still stage authoritative
		// ownership for cancellation/backpressure coverage. In production a frame
		// exhausted by synchronous player commands leaves inbox work untouched.
		if (maximumCommands == 0 && !explicitlyNoExecution)
		{
			diagnostics_.lastDrain = TacticalCommandDrainResult{};
			IncrementSaturated(diagnostics_.budgetExhaustions);
			return;
		}
		const std::size_t admissionLimit = maximumCommands == 0
			? inbox_.limits().maximumPerDrain : maximumCommands;
		std::size_t admittedCommands = 0;
		diagnostics_.lastDrain = inbox_.drain(
			[&](const TacticalCommandRequest& request) {
				if (admittedCommands >= admissionLimit)
					return TacticalCommandDisposition::Defer;
				if (!canAdmitReceiptObligation())
				{
					IncrementSaturated(diagnostics_.receiptCapacityDeferrals);
					return TacticalCommandDisposition::Defer;
				}
				if (!game.packages().isActive(request.packageId))
				{
					if (!queueRequestReceipt(
							request, TacticalCommandTerminalStatus::Rejected,
							TacticalCommandTerminalReason::InactiveOwner, 0))
						return TacticalCommandDisposition::Defer;
					IncrementSaturated(diagnostics_.inactiveOwnerRejections);
					return TacticalCommandDisposition::Reject;
				}
				if (ValidateSimulationCommandDomain(request.command) !=
					SimulationCommandDomainError::None)
				{
					if (!queueRequestReceipt(
							request, TacticalCommandTerminalStatus::Rejected,
							TacticalCommandTerminalReason::InvalidDomain, 0))
						return TacticalCommandDisposition::Defer;
					IncrementSaturated(diagnostics_.semanticRejections);
					return TacticalCommandDisposition::Reject;
				}
				if (!HasTacticalExecutionContext(request.command))
				{
					if (!queueRequestReceipt(
							request, TacticalCommandTerminalStatus::Rejected,
							TacticalCommandTerminalReason::UnavailableContext, 0))
						return TacticalCommandDisposition::Defer;
					IncrementSaturated(diagnostics_.contextRejections);
					return TacticalCommandDisposition::Reject;
				}
				if (game.commands().sequenceExhausted())
				{
					if (!queueRequestReceipt(
							request, TacticalCommandTerminalStatus::Rejected,
							TacticalCommandTerminalReason::SequenceExhausted, 0))
						return TacticalCommandDisposition::Defer;
					IncrementSaturated(diagnostics_.commandSequenceRejections);
					return TacticalCommandDisposition::Reject;
				}
				if (trackedCount_ >= tracked_.size())
					return TacticalCommandDisposition::Defer;
				// Copy ownership before authoritative submission. An allocation
				// failure is caught by the inbox while no second queue was changed.
				TrackedCommand staged;
				staged.packageId = request.packageId;
				staged.requestId = request.requestId;
				staged.simulationTick = diagnostics_.simulationTick;
				// Allocation/sequence exceptions are deliberately left for the
				// inbox to catch. Its FIFO front remains queued for a later retry.
				staged.sequence =
					game.submitCommand(diagnostics_.simulationTick, request.command);
				tracked_[trackedCount_++] = std::move(staged);
				++admittedCommands;
				return TacticalCommandDisposition::Accept;
			});
		flushDeferredCancellations();
		processAuthoritative(game, maximumCommands);
		flushReceipts(game);
	}

	Ja2TacticalCommandHostDiagnostics diagnostics() const noexcept
	{
		Ja2TacticalCommandHostDiagnostics captured = diagnostics_;
		captured.pendingReceipts = pendingReceiptCount_;
		captured.trackedCommands = trackedCount_;
		captured.pendingDeferredCancellations = deferredCancellationCount_;
		return captured;
	}

private:
	struct TrackedCommand
	{
		std::uint64_t sequence = 0;
		std::uint64_t requestId = 0;
		std::uint64_t simulationTick = 0;
		std::string packageId;
	};

	struct PendingReceipt
	{
		TacticalCommandResult result;
		PreparedTacticalCommandResultMessage prepared;
		bool preparedOnce = false;
	};

	bool canAdmitReceiptObligation() const noexcept
	{
		return pendingReceiptCount_ + trackedCount_ <
			MaximumAdmittedReceiptObligations;
	}

	bool queueDeferredCancellation(const std::string& packageId) noexcept
	{
		for (std::size_t offset = 0; offset < deferredCancellationCount_; ++offset)
		{
			const std::size_t index =
				(deferredCancellationHead_ + offset) %
				deferredCancellationOwners_.size();
			if (deferredCancellationOwners_[index] == packageId) return true;
		}
		if (deferredCancellationCount_ >= deferredCancellationOwners_.size())
			return false;
		try
		{
			const std::size_t tail =
				(deferredCancellationHead_ + deferredCancellationCount_) %
				deferredCancellationOwners_.size();
			deferredCancellationOwners_[tail] = packageId;
			++deferredCancellationCount_;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	void popDeferredCancellation() noexcept
	{
		if (deferredCancellationCount_ == 0) return;
		deferredCancellationOwners_[deferredCancellationHead_].clear();
		deferredCancellationHead_ =
			(deferredCancellationHead_ + 1) % deferredCancellationOwners_.size();
		--deferredCancellationCount_;
	}

	void flushDeferredCancellations() noexcept
	{
		while (deferredCancellationCount_ != 0)
		{
			const std::string& packageId =
				deferredCancellationOwners_[deferredCancellationHead_];
			diagnostics_.lastCancellation = inbox_.cancelPackage(packageId, this);
			if (diagnostics_.lastCancellation.error ==
					TacticalCommandCancellationError::DrainInProgress ||
				diagnostics_.lastCancellation.error ==
					TacticalCommandCancellationError::CancellationInProgress)
			{
				IncrementSaturated(diagnostics_.deferredCancellationRetries);
				return;
			}
			if (diagnostics_.lastCancellation)
				AddSaturated(
					diagnostics_.cancelledRequests,
					diagnostics_.lastCancellation.cancelled);
			else
				IncrementSaturated(diagnostics_.cancellationFailures);
			cancelAuthoritative(packageId);
			popDeferredCancellation();
		}
	}

	bool hasReceiptStorage() const noexcept
	{
		return pendingReceiptCount_ + trackedCount_ < pendingReceipts_.size();
	}

	bool queueReceipt(TacticalCommandResult result) noexcept
	{
		if (pendingReceiptCount_ >= pendingReceipts_.size()) return false;
		PendingReceipt& pending = pendingReceipts_[
			(pendingReceiptHead_ + pendingReceiptCount_) % pendingReceipts_.size()];
		pending.result = std::move(result);
		pending.prepared = PreparedTacticalCommandResultMessage{};
		pending.preparedOnce = false;
		++pendingReceiptCount_;
		IncrementSaturated(diagnostics_.receiptsQueued);
		return true;
	}

	bool queueRequestReceipt(
		const TacticalCommandRequest& request,
		TacticalCommandTerminalStatus status,
		TacticalCommandTerminalReason reason,
		std::uint64_t authoritativeSequence) noexcept
	{
		if (!hasReceiptStorage()) return false;
		try
		{
			TacticalCommandResult result;
			result.packageId = request.packageId;
			result.requestId = request.requestId;
			result.authoritativeSequence = authoritativeSequence;
			result.simulationTick = diagnostics_.simulationTick;
			result.status = status;
			result.reason = reason;
			return queueReceipt(std::move(result));
		}
		catch (...)
		{
			IncrementSaturated(diagnostics_.receiptPreparationFailures);
			return false;
		}
	}

	void popReceipt() noexcept
	{
		if (pendingReceiptCount_ == 0) return;
		pendingReceipts_[pendingReceiptHead_] = PendingReceipt{};
		pendingReceiptHead_ =
			(pendingReceiptHead_ + 1) % pendingReceipts_.size();
		--pendingReceiptCount_;
	}

	void flushReceipts(GameContext& game) noexcept
	{
		TacticalCommandResultPublisher publisher(game.runtimeMessages());
		while (pendingReceiptCount_ != 0)
		{
			PendingReceipt& pending = pendingReceipts_[pendingReceiptHead_];
			if (!pending.preparedOnce)
			{
				const TacticalCommandResultPublishError prepared =
					publisher.prepare(pending.result, pending.prepared);
				diagnostics_.lastReceiptPublishError = prepared;
				if (prepared != TacticalCommandResultPublishError::None)
				{
					IncrementSaturated(diagnostics_.receiptPreparationFailures);
					if (prepared == TacticalCommandResultPublishError::CodecAllocationFailure ||
						prepared == TacticalCommandResultPublishError::MessageAllocationFailure)
					{
						IncrementSaturated(diagnostics_.receiptRetryFrames);
						return;
					}
					IncrementSaturated(diagnostics_.receiptDrops);
					popReceipt();
					continue;
				}
				pending.preparedOnce = true;
			}

			const TacticalCommandResultPublishResult published =
				publisher.publishPrepared(pending.prepared);
			diagnostics_.lastReceiptPublishError = published.error;
			if (published)
			{
				IncrementSaturated(diagnostics_.receiptsPublished);
				popReceipt();
				continue;
			}
			IncrementSaturated(diagnostics_.receiptPublishFailures);
			if (published.error == TacticalCommandResultPublishError::QueueFull ||
				published.error == TacticalCommandResultPublishError::MessageAllocationFailure)
			{
				IncrementSaturated(diagnostics_.receiptRetryFrames);
				return;
			}
			IncrementSaturated(diagnostics_.receiptDrops);
			popReceipt();
		}
	}

	void finishTracked(
		std::size_t index,
		std::uint64_t tick,
		TacticalCommandTerminalStatus status,
		TacticalCommandTerminalReason reason) noexcept
	{
		TrackedCommand completed = std::move(tracked_[index]);
		--trackedCount_;
		if (index != trackedCount_)
			tracked_[index] = std::move(tracked_[trackedCount_]);
		tracked_[trackedCount_] = TrackedCommand{};

		TacticalCommandResult result;
		result.packageId = std::move(completed.packageId);
		result.requestId = completed.requestId;
		result.authoritativeSequence = completed.sequence;
		result.simulationTick = tick;
		result.status = status;
		result.reason = reason;
		if (!queueReceipt(std::move(result)))
			IncrementSaturated(diagnostics_.receiptDrops);
	}

	void commandProcessed(
		const SimulationCommand&,
		std::uint64_t tick,
		std::uint64_t sequence,
		CommandDisposition disposition) noexcept override
	{
		if (disposition == CommandDisposition::Retry) return;
		for (std::size_t index = 0; index < trackedCount_; ++index)
		{
			if (tracked_[index].sequence != sequence) continue;
			finishTracked(
				index, tick,
				disposition == CommandDisposition::Applied
					? TacticalCommandTerminalStatus::Applied
					: TacticalCommandTerminalStatus::Discarded,
				disposition == CommandDisposition::Applied
					? TacticalCommandTerminalReason::None
					: TacticalCommandTerminalReason::AuthoritativeDiscard);
			return;
		}
	}

	void commandCancelled(
		const TacticalCommandRequest& request) noexcept override
	{
		if (!queueRequestReceipt(
				request, TacticalCommandTerminalStatus::Cancelled,
				TacticalCommandTerminalReason::PackageTeardown, 0))
			IncrementSaturated(diagnostics_.receiptDrops);
	}

	void processAuthoritative(
		GameContext& game, std::size_t maximumCommands) noexcept
	{
		IncrementSaturated(diagnostics_.processingAttempts);
		diagnostics_.lastProcessingThrew = false;
		diagnostics_.lastProcessing = CommandProcessingResult{};
		try
		{
			diagnostics_.lastProcessing = ExecuteSimulationCommandsThrough(
				diagnostics_.simulationTick, maximumCommands);
			std::size_t consumed = diagnostics_.lastProcessing.applied +
				diagnostics_.lastProcessing.discarded;
			if (diagnostics_.lastProcessing.status == CommandProcessStatus::Blocked ||
				diagnostics_.lastProcessing.status == CommandProcessStatus::QueueChanged)
				++consumed;
			ConsumeSimulationCommandFrameBudget(consumed);
			diagnostics_.authoritativeBackpressure =
				diagnostics_.lastProcessing.status != CommandProcessStatus::Completed;
			if (diagnostics_.lastProcessing.status ==
				CommandProcessStatus::BudgetExhausted)
				IncrementSaturated(diagnostics_.budgetExhaustions);
		}
		catch (...)
		{
			// The processor deliberately retains the throwing command. Pessimistically
			// consume the offered budget so exception recovery cannot overrun the frame.
			ConsumeSimulationCommandFrameBudget(maximumCommands);
			diagnostics_.lastProcessingThrew = true;
			diagnostics_.authoritativeBackpressure = true;
			IncrementSaturated(diagnostics_.processingFailures);
		}
		pruneProcessed(game);
	}

	void pruneProcessed(GameContext& game) noexcept
	{
		std::size_t index = 0;
		while (index < trackedCount_)
		{
			if (game.commands().containsSequence(tracked_[index].sequence))
			{
				++index;
				continue;
			}
			finishTracked(
				index, tracked_[index].simulationTick,
				TacticalCommandTerminalStatus::Discarded,
				TacticalCommandTerminalReason::AuthoritativeDiscard);
		}
	}

	void cancelAuthoritative(const std::string& packageId) noexcept
	{
		if (!game_) return;
		std::size_t index = 0;
		while (index < trackedCount_)
		{
			if (tracked_[index].packageId != packageId)
			{
				++index;
				continue;
			}
			const std::uint64_t sequence = tracked_[index].sequence;
			if (game_->commands().acknowledge(sequence))
			{
				game_->commandJournal().recordDisposition(
					sequence, CommandDisposition::Discard);
				IncrementSaturated(diagnostics_.cancelledAuthoritativeCommands);
			}
			finishTracked(
				index, diagnostics_.simulationTick,
				TacticalCommandTerminalStatus::Cancelled,
				TacticalCommandTerminalReason::PackageTeardown);
		}
	}

	TacticalCommandInbox inbox_;
	std::array<TrackedCommand, MaximumCommandsPerFrame> tracked_;
	std::size_t trackedCount_ = 0;
	std::array<PendingReceipt, MaximumPendingReceipts> pendingReceipts_;
	std::size_t pendingReceiptHead_ = 0;
	std::size_t pendingReceiptCount_ = 0;
	std::array<std::string, MaximumPendingCommands> deferredCancellationOwners_;
	std::size_t deferredCancellationHead_ = 0;
	std::size_t deferredCancellationCount_ = 0;
	GameContext* game_ = nullptr;
	Ja2TacticalCommandHostDiagnostics diagnostics_;
};

Ja2TacticalCommandHost& GetCommandHost() noexcept
{
	static Ja2TacticalCommandHost host;
	return host;
}
}

TacticalCommandService& GetJa2TacticalCommandService() noexcept
{
	return GetCommandHost().service();
}

PackageEventSink& GetJa2TacticalCommandPackageEventSink() noexcept
{
	return GetCommandHost();
}

bool BindJa2TacticalCommandHost(GameContext& game) noexcept
{
	return GetCommandHost().bind(game);
}

void BeginJa2TacticalCommandFrame(GameContext& game) noexcept
{
	BeginSimulationCommandFrameBudget(
		game.frameDriver().completedFrames() + 1,
		GetCommandHost().service().limits().maximumPerDrain);
}

void DrainJa2TacticalCommandsAtSafeFrame(GameContext& game) noexcept
{
	GetCommandHost().drainAtSafeFrame(
		game, GetCommandHost().service().limits().maximumPerDrain);
}

void DrainJa2TacticalCommandsAtSafeFrame(
	GameContext& game, std::size_t maximumCommands) noexcept
{
	GetCommandHost().drainAtSafeFrame(game, maximumCommands);
}

Ja2TacticalCommandHostDiagnostics GetJa2TacticalCommandHostDiagnostics() noexcept
{
	return GetCommandHost().diagnostics();
}
