#ifndef ENGINE_ADAPTERS_JA2_ENGINE_RUNTIME_H
#define ENGINE_ADAPTERS_JA2_ENGINE_RUNTIME_H

#include <cstdint>
#include <utility>

#include <Engine/Adapters/JA2/CommandReplay.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Core/CommandStream.h>
#include <Engine/Core/EngineHost.h>

// JA2 simulation-command extension of the reusable EngineHost. Keeping this
// compatibility type separate lets non-game tools consume the composition root
// without importing soldier, tactical-turn, command codec, or replay types.
template<typename ScreenId = std::uint32_t>
class EngineRuntime : public EngineHost<ScreenId>
{
public:
	using Host = EngineHost<ScreenId>;

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
	CommandReplayService& commandReplay() { return commandReplay_; }
	const CommandReplayService& commandReplay() const { return commandReplay_; }

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
		std::vector<ScheduledCommand<SimulationCommand>> batch;
		batch.reserve(replay.records.size());
		for (const RecordedSimulationCommand& record : replay.records)
			batch.push_back({record.tick, record.sequence, record.command});
		if (!commandStream_.stageRecordedBatch(batch))
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
	CommandReplayService commandReplay_;
	CommandStream<SimulationCommand> commandStream_;
};

#endif
