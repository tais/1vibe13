#ifndef ENGINE_ADAPTERS_JA2_ENGINE_RUNTIME_H
#define ENGINE_ADAPTERS_JA2_ENGINE_RUNTIME_H

#include <cstdint>
#include <initializer_list>
#include <utility>

#include <Engine/Adapters/JA2/CommandReplay.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Adapters/JA2/TacticalEntityDirectory.h>
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
	CommandReplayService& commandReplay() { return commandReplay_; }
	const CommandReplayService& commandReplay() const { return commandReplay_; }
	TacticalEntityDirectory& tacticalEntityDirectory() { return tacticalEntityDirectory_; }
	const TacticalEntityDirectory& tacticalEntityDirectory() const { return tacticalEntityDirectory_; }
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
	TacticalEntityDirectory tacticalEntityDirectory_;
	TacticalWorldSession tacticalWorldSession_;
	CommandReplayService commandReplay_;
	CommandStream<SimulationCommand> commandStream_;
};

#endif
