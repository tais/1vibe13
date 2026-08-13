#include "Dialogue Effect.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <queue>
#include <type_traits>

namespace
{
void Require(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	std::exit(1);
}
}

int main()
{
	static_assert(std::is_same_v<decltype(LegacyDialogueEffect::flags),
		std::uint32_t>);
	static_assert(std::is_same_v<decltype(StatChangeDialogueEffect::increased),
		bool>);
	static_assert(std::is_same_v<decltype(StatChangeDialogueEffect::points),
		std::int16_t>);
	static_assert(std::is_same_v<decltype(StatChangeDialogueEffect::stat),
		std::uint8_t>);
	static_assert(kLegacyStatChangeDialogueEffectFlag == 0x00400000u);

	const DialogueEffectPayload empty{};
	Require(std::holds_alternative<LegacyDialogueEffect>(empty) &&
		LegacyFlagsForDialogueEffect(empty) == 0,
		"a default queue item remains an ordinary zero-flag dialogue");

	const std::uint32_t composite =
		0x00000800u | 0x00400000u | 0x80000000u;
	const DialogueEffectPayload legacy{LegacyDialogueEffect{composite}};
	Require(LegacyFlagsForDialogueEffect(legacy) == composite &&
		GetStatChangeDialogueEffect(legacy) == nullptr,
		"the raw compatibility lane preserves arbitrary composite flags");

	const DialogueEffectPayload typed{StatChangeDialogueEffect{
		true, std::numeric_limits<std::int16_t>::min(),
		std::numeric_limits<std::uint8_t>::max()}};
	const StatChangeDialogueEffect* stat =
		GetStatChangeDialogueEffect(typed);
	Require(LegacyFlagsForDialogueEffect(typed) ==
			kLegacyStatChangeDialogueEffectFlag &&
		stat && stat->increased &&
		stat->points == std::numeric_limits<std::int16_t>::min() &&
		stat->stat == std::numeric_limits<std::uint8_t>::max(),
		"the typed lane retains the complete legacy stat payload domains");
	for (int increasedValue = 0; increasedValue <= 1; ++increasedValue)
	{
		const bool expectedIncreased = increasedValue != 0;
		const DialogueEffectPayload typedIncrease{StatChangeDialogueEffect{
			expectedIncreased, 1, 1}};
		const StatChangeDialogueEffect* typedIncreaseEffect =
			GetStatChangeDialogueEffect(typedIncrease);
		Require(typedIncreaseEffect &&
			typedIncreaseEffect->increased == expectedIncreased &&
			typedIncreaseEffect->points == 1 &&
			typedIncreaseEffect->stat == 1,
			"both boolean states traverse a legal typed payload");
	}
	for (std::int32_t point = std::numeric_limits<std::int16_t>::min();
		point <= std::numeric_limits<std::int16_t>::max(); ++point)
	{
		const auto expectedPoint = static_cast<std::int16_t>(point);
		const bool expectedIncreased =
			(point - std::numeric_limits<std::int16_t>::min()) % 2 != 0;
		const auto expectedStat = static_cast<std::uint8_t>(
			point - std::numeric_limits<std::int16_t>::min());
		const DialogueEffectPayload typedPoint{StatChangeDialogueEffect{
			expectedIncreased, expectedPoint, expectedStat}};
		const StatChangeDialogueEffect* typedPointEffect =
			GetStatChangeDialogueEffect(typedPoint);
		Require(LegacyFlagsForDialogueEffect(typedPoint) ==
				kLegacyStatChangeDialogueEffectFlag &&
			typedPointEffect &&
			typedPointEffect->increased == expectedIncreased &&
			typedPointEffect->points == expectedPoint &&
			typedPointEffect->stat == expectedStat,
			"every int16 point value traverses the typed payload");

		const auto legacySlot = static_cast<std::uint32_t>(expectedPoint);
		Require(static_cast<std::int16_t>(legacySlot) == expectedPoint,
			"typed points match the complete legacy uint32/int16 round trip");
	}
	for (std::uint32_t statValue = 0;
		statValue <= std::numeric_limits<std::uint8_t>::max(); ++statValue)
	{
		const auto expectedStat = static_cast<std::uint8_t>(statValue);
		const bool expectedIncreased = statValue % 2 != 0;
		const auto expectedPoint = static_cast<std::int16_t>(statValue);
		const DialogueEffectPayload typedStat{StatChangeDialogueEffect{
			expectedIncreased, expectedPoint, expectedStat}};
		const StatChangeDialogueEffect* typedStatEffect =
			GetStatChangeDialogueEffect(typedStat);
		Require(LegacyFlagsForDialogueEffect(typedStat) ==
				kLegacyStatChangeDialogueEffectFlag &&
			typedStatEffect &&
			typedStatEffect->increased == expectedIncreased &&
			typedStatEffect->points == expectedPoint &&
			typedStatEffect->stat == expectedStat,
			"every uint8 stat ID traverses the typed payload");

		const auto legacySlot = static_cast<std::uint32_t>(expectedStat);
		Require(static_cast<std::uint8_t>(legacySlot) == expectedStat,
			"typed stat IDs match the complete legacy uint32/uint8 round trip");
	}

	std::queue<DialogueEffectPayload> retained;
	retained.push(legacy);
	retained.push(typed);
	Require(LegacyFlagsForDialogueEffect(retained.front()) == composite,
		"queue retention preserves the raw event first");
	retained.pop();
	stat = GetStatChangeDialogueEffect(retained.front());
	Require(stat && stat->points ==
			std::numeric_limits<std::int16_t>::min(),
		"queue retention preserves typed payload identity and value");

	return 0;
}
