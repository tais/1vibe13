#ifndef ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_CODEC_H
#define ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_CODEC_H

#include <cstdint>
#include <vector>

#include <Engine/Core/CommandJournal.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>

using RecordedSimulationCommand = CommandJournalRecord<SimulationCommand>;

enum class SimulationCommandJournalDecodeResult
{
	Success,
	Invalid,
	UnsupportedVersion,
	TooManyRecords
};

// Stable value codec for diagnostics, replay capture, and future network
// transport. Variant indexes are never serialized; explicit tags keep newer
// command alternatives from silently changing old recordings.
bool EncodeSimulationCommandJournal(
	const std::vector<RecordedSimulationCommand>& records,
	std::uint64_t droppedCount,
	std::vector<std::uint8_t>& bytes);

SimulationCommandJournalDecodeResult DecodeSimulationCommandJournal(
	const std::vector<std::uint8_t>& bytes,
	std::vector<RecordedSimulationCommand>& records,
	std::uint64_t& droppedCount);

#endif
