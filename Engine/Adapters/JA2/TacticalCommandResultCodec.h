#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_COMMAND_RESULT_CODEC_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_COMMAND_RESULT_CODEC_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/TacticalCommandResult.h>

inline constexpr std::uint16_t TacticalCommandResultWireVersion = 1;
inline constexpr std::size_t MaximumTacticalCommandResultOwnerBytes = 256;

enum class TacticalCommandResultEncodeError
{
	None,
	Invalid,
	AllocationFailure
};

enum class TacticalCommandResultDecodeError
{
	None,
	Invalid,
	UnsupportedVersion,
	AllocationFailure
};

// Stable little-endian single-record transport used by runtime messages,
// diagnostics, and external SDK consumers. Both operations publish output only
// after the complete record validates.
TacticalCommandResultEncodeError EncodeTacticalCommandResult(
	const TacticalCommandResult& result,
	std::vector<std::uint8_t>& bytes) noexcept;

TacticalCommandResultDecodeError DecodeTacticalCommandResult(
	const std::vector<std::uint8_t>& bytes,
	TacticalCommandResult& result) noexcept;

#endif
