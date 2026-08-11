#ifndef ENGINE_ADAPTERS_JA2_ENGINE_RUNTIME_H
#define ENGINE_ADAPTERS_JA2_ENGINE_RUNTIME_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>

#include <Engine/Adapters/JA2/CampaignClockScheduler.h>
#include <Engine/Adapters/JA2/CampaignClockService.h>
#include <Engine/Adapters/JA2/CampaignEventQueue.h>
#include <Engine/Adapters/JA2/CommandReplay.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Adapters/JA2/SimulationCommandExecutor.h>
#include <Engine/Adapters/JA2/StrategicGroupDirectory.h>
#include <Engine/Adapters/JA2/TacticalEntityDirectory.h>
#include <Engine/Adapters/JA2/TacticalInventoryUiSession.h>
#include <Engine/Adapters/JA2/TacticalWorldItemDirectory.h>
#include <Engine/Adapters/JA2/TacticalWorldSession.h>
#include <Engine/Core/CommandStream.h>
#include <Engine/Core/EngineHost.h>

// JA2 simulation-command extension of the reusable EngineHost. Keeping this
// compatibility type separate lets non-game tools consume the composition root
// without importing soldier, tactical-turn, command codec, or replay types.
template<typename ScreenId = std::uint32_t>
class EngineRuntime : public EngineHost<ScreenId>
{
	struct LegacyBraceConstructionTag {};

public:
	using Host = EngineHost<ScreenId>;

	// Preserve the EngineServices/default-runtime meaning of the established
	// `EngineRuntime<>({})` spelling alongside the named host-options overload.
	explicit EngineRuntime(std::initializer_list<LegacyBraceConstructionTag>)
		: EngineRuntime(EngineServices{})
	{
	}

	explicit EngineRuntime(
		EngineHostOptions options,
		EngineServices services = EngineServices::defaults(),
		PackageEventSink& packageEvents = NullPackageEventSink::instance())
		: Host(std::move(options), services, packageEvents),
		  commandReplay_(this->persistence())
	{
	}

	explicit EngineRuntime(
		EngineServices services = EngineServices::defaults(),
		ContentApiVersion supportedContentApi = CurrentContentApiVersion,
		PackageEventSink& packageEvents = NullPackageEventSink::instance(),
		RuntimeCapabilities hostCapabilities = {})
		: Host(services, supportedContentApi, packageEvents, std::move(hostCapabilities)),
		  commandReplay_(this->persistence())
	{
	}

	EngineRuntime(const EngineRuntime&) = delete;
	EngineRuntime& operator=(const EngineRuntime&) = delete;
	EngineRuntime(EngineRuntime&&) = delete;
	EngineRuntime& operator=(EngineRuntime&&) = delete;

	DeterministicCommandQueue<SimulationCommand>& commands() { return commandStream_.queue(); }
	const DeterministicCommandQueue<SimulationCommand>& commands() const { return commandStream_.queue(); }
	CommandJournal<SimulationCommand>& commandJournal() { return commandStream_.journal(); }
	const CommandJournal<SimulationCommand>& commandJournal() const { return commandStream_.journal(); }
	// Binding is stable for the runtime lifetime. The executor is host-owned and
	// must outlive this EngineRuntime; rebinding to another world is rejected.
	bool bindSimulationCommandExecutor(
		SimulationCommandExecutor& executor) noexcept
	{
		if (commandExecutor_ != &NullSimulationCommandExecutor::instance() &&
			commandExecutor_ != &executor)
			return false;
		commandExecutor_ = &executor;
		return true;
	}
	bool hasSimulationCommandExecutor() const noexcept
	{
		return commandExecutor_ != &NullSimulationCommandExecutor::instance();
	}
	SimulationCommandExecutor& simulationCommandExecutor() noexcept
	{
		return *commandExecutor_;
	}
	const SimulationCommandExecutor& simulationCommandExecutor() const noexcept
	{
		return *commandExecutor_;
	}
	bool commandExecutionActive() const noexcept
	{
		return commandExecutionActive_;
	}

	CommandProcessingResult executeCommandsThrough(
		std::uint64_t tick,
		SimulationCommandExecutionSink* sink = nullptr)
	{
		if (commandExecutionActive_)
		{
			CommandProcessingResult nested;
			nested.status = CommandProcessStatus::QueueChanged;
			return nested;
		}
		CommandExecutionScope executionScope{commandExecutionActive_};
		return executeCommands(
			[tick](auto& queue, auto&& handler, auto&& observer) {
				return ProcessCommandsThrough(
					queue, tick,
					std::forward<decltype(handler)>(handler),
					std::forward<decltype(observer)>(observer));
			},
			sink);
	}

	CommandProcessingResult executeCommandsThrough(
		std::uint64_t tick,
		std::size_t maximumCommands,
		SimulationCommandExecutionSink* sink = nullptr)
	{
		if (commandExecutionActive_)
		{
			CommandProcessingResult nested;
			nested.status = CommandProcessStatus::QueueChanged;
			return nested;
		}
		CommandExecutionScope executionScope{commandExecutionActive_};
		return executeCommands(
			[tick, maximumCommands](
				auto& queue, auto&& handler, auto&& observer) {
				return ProcessCommandsThrough(
					queue, tick, maximumCommands,
					std::forward<decltype(handler)>(handler),
					std::forward<decltype(observer)>(observer));
			},
			sink);
	}

	ExpectedCommandProcessingResult executeExpectedCommandThrough(
		std::uint64_t tick,
		std::uint64_t sequence,
		SimulationCommandExecutionSink* sink = nullptr)
	{
		if (commandExecutionActive_)
		{
			ExpectedCommandProcessingResult nested;
			nested.status = ExpectedCommandProcessStatus::QueueChanged;
			nested.expectedSequence = sequence;
			return nested;
		}
		CommandExecutionScope executionScope{commandExecutionActive_};
		return executeCommands(
			[tick, sequence](
				auto& queue, auto&& handler, auto&& observer) {
				return ProcessExpectedNextCommandThrough(
					queue, tick, sequence,
					std::forward<decltype(handler)>(handler),
					std::forward<decltype(observer)>(observer));
			},
			sink);
	}

	CommandReplayService& commandReplay() { return commandReplay_; }
	const CommandReplayService& commandReplay() const { return commandReplay_; }
	CampaignClockSession& campaignClockSession() { return campaignClockSession_; }
	const CampaignClockSession& campaignClockSession() const { return campaignClockSession_; }
	CampaignClockScheduler& campaignClockScheduler() { return campaignClockScheduler_; }
	const CampaignClockScheduler& campaignClockScheduler() const
	{
		return campaignClockScheduler_;
	}
	CampaignClockService& campaignClockService() { return campaignClockService_; }
	const CampaignClockService& campaignClockService() const { return campaignClockService_; }
	CampaignEventQueue& campaignEventQueue() { return campaignEventQueue_; }
	const CampaignEventQueue& campaignEventQueue() const { return campaignEventQueue_; }
	StrategicGroupDirectory& strategicGroupDirectory()
	{
		return strategicGroupDirectory_;
	}
	const StrategicGroupDirectory& strategicGroupDirectory() const
	{
		return strategicGroupDirectory_;
	}
	TacticalEntityDirectory& tacticalEntityDirectory() { return tacticalEntityDirectory_; }
	const TacticalEntityDirectory& tacticalEntityDirectory() const { return tacticalEntityDirectory_; }
	TacticalInventoryUiSession& tacticalInventoryUiSession()
	{
		return tacticalInventoryUiSession_;
	}
	const TacticalInventoryUiSession& tacticalInventoryUiSession() const
	{
		return tacticalInventoryUiSession_;
	}
	TacticalWorldItemDirectory& tacticalWorldItemDirectory()
	{
		return tacticalWorldItemDirectory_;
	}
	const TacticalWorldItemDirectory& tacticalWorldItemDirectory() const
	{
		return tacticalWorldItemDirectory_;
	}
	TacticalWorldSession& tacticalWorldSession() { return tacticalWorldSession_; }
	const TacticalWorldSession& tacticalWorldSession() const { return tacticalWorldSession_; }

	CommandReplaySaveResult saveCommandReplay(const std::string& path) const noexcept
	{
		try
		{
			return commandReplay_.save(path, SimulationCommandReplay{
				commandStream_.journal().snapshot(),
				commandStream_.journal().droppedCount()});
		}
		catch (...)
		{
			return CommandReplaySaveResult::StorageError;
		}
	}

	CommandReplayLoadResult loadCommandReplay(
		const std::string& path, SimulationCommandReplay& replay) const noexcept
	{
		return commandReplay_.load(path, replay);
	}

	CommandReplayStageResult stageCommandReplay(const SimulationCommandReplay& replay)
	{
		if (replay.droppedCount != 0)
			return CommandReplayStageResult::IncompleteCapture;
		std::vector<ScheduledCommand<SimulationCommand>> recordedBatch;
		recordedBatch.reserve(replay.records.size());
		for (const RecordedSimulationCommand& record : replay.records)
		{
			if (!IsStructurallyValidSimulationCommand(record.command) ||
				!IsStructurallyValidSimulationCommand(
					SimulationCommandPlaybackPolicy::executionCommand(
						record.command)))
				return CommandReplayStageResult::Invalid;
			recordedBatch.push_back(
				{record.tick, record.sequence, record.command});
		}
		if (!commandStream_.stageRecordedPlaybackBatch(recordedBatch))
			return CommandReplayStageResult::SequenceConflict;
		return CommandReplayStageResult::Success;
	}

	std::uint64_t submitCommand(std::uint64_t tick, SimulationCommand command)
	{
		return commandStream_.submit(tick, std::move(command));
	}

	bool submitRecordedCommand(
		std::uint64_t tick, std::uint64_t sequence, SimulationCommand command)
	{
		return commandStream_.submitRecorded(tick, sequence, std::move(command));
	}

private:
	class CommandExecutionScope
	{
	public:
		explicit CommandExecutionScope(bool& active) noexcept
			: active_(active)
		{
			active_ = true;
		}

		~CommandExecutionScope()
		{
			active_ = false;
		}

		CommandExecutionScope(const CommandExecutionScope&) = delete;
		CommandExecutionScope& operator=(
			const CommandExecutionScope&) = delete;

	private:
		bool& active_;
	};

	template<typename Process>
	auto executeCommands(
		Process&& process,
		SimulationCommandExecutionSink* sink)
	{
		return process(
			commandStream_.queue(),
			[this](
				const SimulationCommand& command,
				std::uint64_t tick,
				std::uint64_t sequence) {
				return commandExecutor_->execute(command, tick, sequence);
			},
			[this, sink](
				const SimulationCommand& command,
				std::uint64_t tick,
				std::uint64_t sequence,
				CommandDisposition disposition) {
				commandStream_.journal().recordDisposition(
					sequence, disposition);
				if (sink)
					sink->commandProcessed(
						command, tick, sequence, disposition);
			});
	}

	CampaignClockSession campaignClockSession_;
	CampaignClockScheduler campaignClockScheduler_;
	CampaignClockSessionService campaignClockService_{campaignClockSession_};
	CampaignEventQueue campaignEventQueue_;
	StrategicGroupDirectory strategicGroupDirectory_;
	TacticalEntityDirectory tacticalEntityDirectory_;
	TacticalInventoryUiSession tacticalInventoryUiSession_;
	TacticalWorldItemDirectory tacticalWorldItemDirectory_;
	TacticalWorldSession tacticalWorldSession_;
	CommandReplayService commandReplay_;
	CommandStream<SimulationCommand, SimulationCommandPlaybackPolicy>
		commandStream_;
	SimulationCommandExecutor* commandExecutor_ =
		&NullSimulationCommandExecutor::instance();
	bool commandExecutionActive_ = false;
};

#endif
