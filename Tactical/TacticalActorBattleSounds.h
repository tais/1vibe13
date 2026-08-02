#pragma once

#include "types.h"

#include <cstdint>

class TacticalActor;

// Stable battle-sound indices used by quote tables and installed voice data.
enum
{
	BATTLE_SOUND_OK1,
	BATTLE_SOUND_COOL1,
	BATTLE_SOUND_CURSE1,
	BATTLE_SOUND_HIT1,
	BATTLE_SOUND_LAUGH1,
	BATTLE_SOUND_ATTN1,
	BATTLE_SOUND_DIE1,
	BATTLE_SOUND_HUMM,
	BATTLE_SOUND_NOTHING,
	BATTLE_SOUND_GOTIT,
	BATTLE_SOUND_LOWMARALE_OK1,
	BATTLE_SOUND_LOWMARALE_ATTN1,
	BATTLE_SOUND_LOCKED,
	BATTLE_SOUND_ENEMY,
	BATTLE_SOUND_PUNCH,
	BATTLE_SOUND_KNIFE,
	NUM_MERC_BATTLE_SOUNDS
};

enum
{
	BATTLE_SND_LOWER_VOLUME = 1,
};

namespace TacticalActorBattleSounds
{
	bool preload(TacticalActor& actor, bool remove);
	bool play(
		TacticalActor& actor,
		std::uint8_t soundId);
	bool playWithCode(
		TacticalActor& actor,
		std::uint8_t soundId,
		std::int8_t specialCode);
}

// Legacy adapter retained for source and link compatibility.
BOOLEAN PreloadSoldierBattleSounds(TacticalActor* actor, BOOLEAN remove);
