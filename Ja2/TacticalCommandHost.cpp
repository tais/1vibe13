#include "TacticalCommandHost.h"

#include <array>
#include <limits>
#include <string>
#include <type_traits>
#include <variant>

#include "Animation Control.h"
#include "GameContext.h"
#include "Map Information.h"
#include "Overhead.h"
#include "Simulation Commands.h"
#include "Structure Internals.h"
#include "worlddef.h"

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

bool HasValidLegacyDomain(const SimulationCommand& command) noexcept
{
	if (command.valueless_by_exception()) return false;
	return std::visit([](const auto& value) noexcept {
		using Command = typename std::decay<decltype(value)>::type;
		if constexpr (std::is_same<Command, EndTurnCommand>::value)
		{
			return value.nextTeam < MAXTEAMS;
		}
		else if constexpr (std::is_same<Command, ChangeStanceCommand>::value)
		{
			return value.soldier.slot < TOTAL_SOLDIERS &&
				(value.stance == ANIM_STAND || value.stance == ANIM_CROUCH ||
				 value.stance == ANIM_PRONE);
		}
		else if constexpr (std::is_same<Command, BeginFireWeaponCommand>::value)
		{
			return value.soldier.slot < TOTAL_SOLDIERS &&
				value.targetGrid >= 0 && value.targetGrid < WORLD_MAX &&
				(value.targetLevel == FIRST_LEVEL || value.targetLevel == SECOND_LEVEL) &&
				value.targetCubeLevel >= 0 && value.targetCubeLevel <= PROFILE_Z_SIZE;
		}
		else if constexpr (std::is_same<Command, MoveToGridCommand>::value)
		{
			return value.soldier.slot < TOTAL_SOLDIERS &&
				value.destinationGrid >= 0 && value.destinationGrid < WORLD_MAX &&
				value.movementMode < NUMANIMATIONSTATES &&
				(gAnimControl[value.movementMode].uiFlags & ANIM_MOVING) != 0;
		}
		return false;
	}, command);
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

class Ja2TacticalCommandHost final : public PackageEventSink
{
private:
	static constexpr std::size_t MaximumPendingCommands = 1024;
	static constexpr std::size_t MaximumCommandsPerFrame = 64;
	static constexpr std::size_t MaximumDiagnosticCommands = 128;
	static constexpr std::size_t MaximumOwnerBytes = 256;

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
		game_ = &game;
		return true;
	}

	void publish(PackageEvent event) noexcept override
	{
		if (!RequiresCommandCancellation(event.kind)) return;
		IncrementSaturated(diagnostics_.lifecycleCancellationEvents);
		diagnostics_.lastCancellation = inbox_.cancelPackage(event.packageId);
		if (diagnostics_.lastCancellation)
			AddSaturated(
				diagnostics_.cancelledRequests,
				diagnostics_.lastCancellation.cancelled);
		else
			IncrementSaturated(diagnostics_.cancellationFailures);
		cancelAuthoritative(event.packageId);
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

		// A retained authoritative command always gets the complete bounded
		// budget. Even a successful retry consumes this safe frame so recovery
		// cannot combine backlog work with another admission batch.
		if (diagnostics_.authoritativeBackpressure || !game.commands().empty())
		{
			diagnostics_.authoritativeBackpressure = true;
			IncrementSaturated(diagnostics_.backpressureFrames);
			processAuthoritative(game, maximumCommands);
			return;
		}

		diagnostics_.lastDrain = inbox_.drain(
			[&](const TacticalCommandRequest& request) {
				if (!game.packages().isActive(request.packageId))
				{
					IncrementSaturated(diagnostics_.inactiveOwnerRejections);
					return TacticalCommandDisposition::Reject;
				}
				if (!HasValidLegacyDomain(request.command))
				{
					IncrementSaturated(diagnostics_.semanticRejections);
					return TacticalCommandDisposition::Reject;
				}
				if (!HasTacticalExecutionContext(request.command))
				{
					IncrementSaturated(diagnostics_.contextRejections);
					return TacticalCommandDisposition::Reject;
				}
				if (game.commands().sequenceExhausted())
				{
					IncrementSaturated(diagnostics_.commandSequenceRejections);
					return TacticalCommandDisposition::Reject;
				}
				if (trackedCount_ >= tracked_.size())
					return TacticalCommandDisposition::Defer;
				// Copy ownership before authoritative submission. An allocation
				// failure is caught by the inbox while no second queue was changed.
				TrackedCommand staged;
				staged.packageId = request.packageId;
				// Allocation/sequence exceptions are deliberately left for the
				// inbox to catch. Its FIFO front remains queued for a later retry.
				staged.sequence =
					game.submitCommand(diagnostics_.simulationTick, request.command);
				tracked_[trackedCount_++] = std::move(staged);
				return TacticalCommandDisposition::Accept;
			});
		processAuthoritative(game, maximumCommands);
	}

	Ja2TacticalCommandHostDiagnostics diagnostics() const noexcept
	{
		return diagnostics_;
	}

private:
	struct TrackedCommand
	{
		std::uint64_t sequence = 0;
		std::string packageId;
	};

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
			diagnostics_.authoritativeBackpressure =
				diagnostics_.lastProcessing.status != CommandProcessStatus::Completed;
			if (diagnostics_.lastProcessing.status ==
				CommandProcessStatus::BudgetExhausted)
				IncrementSaturated(diagnostics_.budgetExhaustions);
		}
		catch (...)
		{
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
			tracked_[index] = std::move(tracked_[trackedCount_ - 1]);
			tracked_[trackedCount_ - 1] = TrackedCommand{};
			--trackedCount_;
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
			tracked_[index] = std::move(tracked_[trackedCount_ - 1]);
			tracked_[trackedCount_ - 1] = TrackedCommand{};
			--trackedCount_;
		}
	}

	TacticalCommandInbox inbox_;
	std::array<TrackedCommand, MaximumCommandsPerFrame> tracked_;
	std::size_t trackedCount_ = 0;
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
