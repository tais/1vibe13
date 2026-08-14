#ifndef JA2_DIALOGUE_EFFECT_H
#define JA2_DIALOGUE_EFFECT_H

#include <cstdint>
#include <variant>

// The legacy dialogue queue accepts arbitrary bitmasks from installed Lua
// content. Keep that compatibility lane explicit while moving first-party
// effects to typed payloads one behaviorally closed effect at a time.
struct LegacyDialogueEffect
{
	std::uint32_t flags{};
};

struct StatChangeDialogueEffect
{
	bool increased{};
	std::int16_t points{};
	std::uint8_t stat{};
};

using DialogueEffectPayload =
	std::variant<LegacyDialogueEffect, StatChangeDialogueEffect>;

inline constexpr std::uint32_t kLegacyStatChangeDialogueEffectFlag =
	0x00400000u;

inline std::uint32_t LegacyFlagsForDialogueEffect(
	const DialogueEffectPayload& effect) noexcept
{
	if (const auto* legacy = std::get_if<LegacyDialogueEffect>(&effect))
	{
		return legacy->flags;
	}

	return kLegacyStatChangeDialogueEffectFlag;
}

inline const StatChangeDialogueEffect* GetStatChangeDialogueEffect(
	const DialogueEffectPayload& effect) noexcept
{
	return std::get_if<StatChangeDialogueEffect>(&effect);
}

#endif
