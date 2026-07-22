#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_COMMAND_RESULT_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_COMMAND_RESULT_H

#include <cstdint>
#include <string>

enum class TacticalCommandTerminalStatus : std::uint8_t
{
	Rejected = 1,
	Applied = 2,
	Discarded = 3,
	Cancelled = 4
};

// Stable adapter-level reasons. They deliberately describe the command
// boundary rather than exposing legacy animation, soldier, or UI enums.
enum class TacticalCommandTerminalReason : std::uint8_t
{
	None = 0,
	InactiveOwner = 1,
	InvalidDomain = 2,
	UnavailableContext = 3,
	SequenceExhausted = 4,
	PackageTeardown = 5,
	AuthoritativeDiscard = 6
};

// Pointer-free terminal receipt for one accepted package request. Rejected
// requests publish sequence zero; applied/discarded sequence zero remains valid
// because it is the first identifier in a fresh authoritative command stream.
// Status, rather than a sequence sentinel, identifies pre-admission rejection.
struct TacticalCommandResult
{
	std::string packageId;
	std::uint64_t requestId = 0;
	std::uint64_t authoritativeSequence = 0;
	std::uint64_t simulationTick = 0;
	TacticalCommandTerminalStatus status =
		TacticalCommandTerminalStatus::Rejected;
	TacticalCommandTerminalReason reason =
		TacticalCommandTerminalReason::InvalidDomain;
};

#endif
