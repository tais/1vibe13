#ifndef ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_CODEC_H
#define ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_CODEC_H

#include <cstdint>
#include <vector>

#include <Engine/Core/CommandJournal.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>

using RecordedSimulationCommand = CommandJournalRecord<SimulationCommand>;

inline constexpr std::uint16_t SimulationCommandJournalWireVersion = 4;

enum class SimulationCommandJournalDecodeResult
{
	Success,
	Invalid,
	UnsupportedVersion,
	TooManyRecords
};

// Stable value codec for diagnostics, replay capture, and future network
// transport. The format has no shipped compatibility contract yet, so there is
// exactly one current layout. The header keeps an explicit version field for a
// future genuinely published format; unsupported versions fail closed rather
// than accumulating speculative migration code. Resolved actor identities are
// mandatory and variant indexes are never serialized.
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
