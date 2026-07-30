#pragma once

#include <cstdint>

class TacticalActor;
struct SoldierID;

namespace TacticalActorCovertOps
{
	[[nodiscard]] bool looksLikeCivilian(TacticalActor& actor);
	[[nodiscard]] bool looksLikeSoldier(TacticalActor& actor);
	[[nodiscard]] std::int8_t uniformType(TacticalActor& actor);
	[[nodiscard]] bool equipmentTooGood(TacticalActor& actor, bool closeLook);
	[[nodiscard]] bool seemsLegitimate(TacticalActor& actor, SoldierID observerId);
	[[nodiscard]] bool recognizesCombatant(TacticalActor& actor, SoldierID targetId);

	void loseDisguise(TacticalActor& actor);
	void disguise(TacticalActor& actor);
	void applyCovert(TacticalActor& actor, bool withMessage);
	void strip(TacticalActor& actor);
	void runSelfTest(TacticalActor& actor);

	[[nodiscard]] std::uint8_t uncoverRisk(TacticalActor& actor);
	[[nodiscard]] float intelGain(TacticalActor& actor);
}
