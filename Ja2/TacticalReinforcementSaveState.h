#ifndef JA2_TACTICAL_REINFORCEMENT_SAVE_STATE_H
#define JA2_TACTICAL_REINFORCEMENT_SAVE_STATE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Optional native-domain extension; the legacy domain prefix stays unchanged.
constexpr std::uint32_t TacticalReinforcementSaveSection = 0x464e4952u; // "RINF"
constexpr std::uint32_t TacticalReinforcementSaveVersion = 1;
constexpr std::size_t TacticalReinforcementSaveBytes = 24;

struct TacticalReinforcementSaveState
{
	std::uint32_t turnCounter = 0;
	std::uint32_t enemyTurn = 0;
	std::uint32_t enemyArrived = 0;
	std::uint32_t militiaTurn = 0;
	std::uint32_t militiaArrived = 0;

	friend bool operator==(const TacticalReinforcementSaveState& a,
		const TacticalReinforcementSaveState& b) noexcept
	{
		return a.turnCounter == b.turnCounter && a.enemyTurn == b.enemyTurn &&
			a.enemyArrived == b.enemyArrived && a.militiaTurn == b.militiaTurn &&
			a.militiaArrived == b.militiaArrived;
	}
};

inline std::array<std::uint8_t, TacticalReinforcementSaveBytes>
EncodeTacticalReinforcementSaveState(const TacticalReinforcementSaveState& state) noexcept
{
	const std::array<std::uint32_t, 6> words{{TacticalReinforcementSaveVersion,
		state.turnCounter, state.enemyTurn, state.enemyArrived,
		state.militiaTurn, state.militiaArrived}};
	std::array<std::uint8_t, TacticalReinforcementSaveBytes> bytes{};
	for (std::size_t word = 0; word < words.size(); ++word)
		for (std::size_t byte = 0; byte < 4; ++byte)
			bytes[word * 4 + byte] = static_cast<std::uint8_t>(words[word] >> (byte * 8));
	return bytes;
}

inline bool DecodeTacticalReinforcementSaveState(const std::vector<std::uint8_t>& bytes,
	TacticalReinforcementSaveState& state) noexcept
{
	if (bytes.size() != TacticalReinforcementSaveBytes) return false;
	std::array<std::uint32_t, 6> words{};
	for (std::size_t word = 0; word < words.size(); ++word)
		for (std::size_t byte = 0; byte < 4; ++byte)
			words[word] |= static_cast<std::uint32_t>(bytes[word * 4 + byte]) << (byte * 8);
	if (words[0] != TacticalReinforcementSaveVersion) return false;
	state = {words[1], words[2], words[3], words[4], words[5]};
	return true;
}

#endif
