#ifndef ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_CODEC_H
#define ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_CODEC_H

#include <cstdint>
#include <vector>

#include <Engine/Core/CommandJournal.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>

using RecordedSimulationCommand = CommandJournalRecord<SimulationCommand>;

inline constexpr std::uint16_t SimulationCommandJournalWireVersion = 7;
inline constexpr std::uint16_t OldestSimulationCommandJournalWireVersion = 1;

enum class SimulationCommandJournalDecodeResult
{
	Success,
	Invalid,
	UnsupportedVersion,
	TooManyRecords
};

// Stable value codec for diagnostics, replay capture, and future network
// transport. Encoding always produces version 7 and requires resolved actor
// identities. Decoding also accepts versions 1 through 6; a version-1 slot-only
// stance reference becomes {slot, 0}, explicitly marking it legacy-unresolved.
// Move commands were introduced in version 3; version 4 adds explicit origin
// and pending-action policy while version-3 moves retain the legacy UI/clear
// defaults. Facing, stealth, and stop-movement commands were introduced in
// version 5. Weapon-mode, scope-mode, and reload commands were introduced in
// version 6. Typed tactical traversal was introduced in version 7. Variant
// indexes are never serialized.
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
