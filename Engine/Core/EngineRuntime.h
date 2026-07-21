#ifndef ENGINE_CORE_ENGINE_RUNTIME_H
#define ENGINE_CORE_ENGINE_RUNTIME_H

#include <cstdint>
#include <utility>

#include <Engine/Core/CommandJournal.h>
#include <Engine/Core/CommandReplay.h>
#include <Engine/Core/DeterministicCommandQueue.h>
#include <Engine/Core/EngineHost.h>
#include <Engine/Core/SimulationCommand.h>

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

	DeterministicCommandQueue<SimulationCommand>& commands() { return commands_; }
	const DeterministicCommandQueue<SimulationCommand>& commands() const { return commands_; }
	CommandJournal<SimulationCommand>& commandJournal() { return commandJournal_; }
	const CommandJournal<SimulationCommand>& commandJournal() const { return commandJournal_; }
	CommandReplayService& commandReplay() { return commandReplay_; }
	const CommandReplayService& commandReplay() const { return commandReplay_; }

	CommandReplaySaveResult saveCommandReplay(const std::string& path) const noexcept
	{
		try
		{
			return commandReplay_.save(path, SimulationCommandReplay{
				commandJournal_.snapshot(), commandJournal_.droppedCount()});
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
		if (!commands_.enqueueRecordedBatch(batch))
			return CommandReplayStageResult::SequenceConflict;
		for (const RecordedSimulationCommand& record : replay.records)
			commandJournal_.recordSubmission(
				record.tick, record.sequence, record.command);
		return CommandReplayStageResult::Success;
	}

	std::uint64_t submitCommand(std::uint64_t tick, SimulationCommand command)
	{
		SimulationCommand recorded = command;
		const std::uint64_t sequence = commands_.enqueue(tick, std::move(command));
		commandJournal_.recordSubmission(tick, sequence, std::move(recorded));
		return sequence;
	}

	bool submitRecordedCommand(
		std::uint64_t tick, std::uint64_t sequence, SimulationCommand command)
	{
		SimulationCommand recorded = command;
		if (!commands_.enqueueRecorded(tick, sequence, std::move(command))) return false;
		commandJournal_.recordSubmission(tick, sequence, std::move(recorded));
		return true;
	}

private:
	CommandReplayService commandReplay_;
	DeterministicCommandQueue<SimulationCommand> commands_;
	CommandJournal<SimulationCommand> commandJournal_;
};

#endif
