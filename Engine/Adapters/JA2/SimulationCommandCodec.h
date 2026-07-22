#ifndef ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_CODEC_H
#define ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_CODEC_H

#include <cstdint>
#include <vector>

#include <Engine/Core/CommandJournal.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>

using RecordedSimulationCommand = CommandJournalRecord<SimulationCommand>;

inline constexpr std::uint16_t SimulationCommandJournalWireVersion = 2;
inline constexpr std::uint16_t OldestSimulationCommandJournalWireVersion = 1;

enum class SimulationCommandJournalDecodeResult
{
	Success,
	Invalid,
	UnsupportedVersion,
	TooManyRecords
};

// Stable value codec for diagnostics, replay capture, and future network
// transport. Encoding always produces version 2 and requires resolved actor
// identities. Decoding also accepts version 1; its slot-only stance reference
// becomes {slot, 0}, explicitly marking it legacy-unresolved. Variant indexes
// are never serialized.
//
// Both operations are transactional: rejected input leaves the caller's
// previous output untouched.
bool EncodeSimulationCommandJournal(
	const std::vector<RecordedSimulationCommand>& records,
	std::uint64_t droppedCount,
	std::vector<std::uint8_t>& bytes);

SimulationCommandJournalDecodeResult DecodeSimulationCommandJournal(
	const std::vector<std::uint8_t>& bytes,
	std::vector<RecordedSimulationCommand>& records,
	std::uint64_t& droppedCount);

#endif
