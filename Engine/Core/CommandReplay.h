#ifndef ENGINE_CORE_COMMAND_REPLAY_H
#define ENGINE_CORE_COMMAND_REPLAY_H

#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/SimulationCommandCodec.h>

struct SimulationCommandReplay
{
	std::vector<RecordedSimulationCommand> records;
	std::uint64_t droppedCount = 0;
};

enum class CommandReplaySaveResult
{
	Success,
	Invalid,
	TooLarge,
	StorageError
};

enum class CommandReplayLoadResult
{
	Success,
	NotFound,
	Invalid,
	UnsupportedVersion,
	TooLarge,
	IntegrityFailure,
	StorageError
};

enum class CommandReplayStageResult
{
	Success,
	IncompleteCapture,
	SequenceConflict
};

// Durable transport for deterministic simulation input captures. The command
// codec owns the command schema while PersistenceService owns storage limits,
// record versioning, and integrity. Loading is transactional: a failed read
// never publishes a partial replay.
class CommandReplayService
{
public:
	explicit CommandReplayService(PersistenceService& persistence)
		: persistence_(persistence) {}

	CommandReplaySaveResult save(
		const std::string& path, const SimulationCommandReplay& replay) const noexcept;
	CommandReplayLoadResult load(
		const std::string& path, SimulationCommandReplay& replay) const noexcept;

private:
	PersistenceService& persistence_;
};

#endif
