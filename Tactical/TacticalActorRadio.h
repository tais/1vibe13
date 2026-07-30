#pragma once

#include <cstdint>

class TacticalActor;
struct SoldierID;

namespace TacticalActorRadio
{
	[[nodiscard]] bool canUse(
		TacticalActor& actor,
		bool checkForActionPoints = true);
	[[nodiscard]] bool use(TacticalActor& actor);

	[[nodiscard]] bool canOrderAnyArtilleryStrike(
		TacticalActor& actor,
		std::uint32_t* sectorId);
	[[nodiscard]] bool orderArtilleryStrike(
		TacticalActor& actor,
		std::uint32_t sectorId,
		std::int32_t targetGridNo,
		std::uint8_t team);

	[[nodiscard]] bool isJamming(TacticalActor& actor);
	[[nodiscard]] bool startJamming(TacticalActor& actor);
	[[nodiscard]] bool isScanning(TacticalActor& actor);
	[[nodiscard]] bool startScanning(TacticalActor& actor);
	[[nodiscard]] bool isListening(TacticalActor& actor);
	[[nodiscard]] bool startListening(TacticalActor& actor);
	[[nodiscard]] bool callReinforcements(
		TacticalActor& actor,
		std::uint32_t sourceSector,
		std::uint16_t number);
	bool switchOff(TacticalActor& actor) noexcept;
	[[nodiscard]] bool orderAllTurncoats(TacticalActor& actor);
	void reportFailure(TacticalActor& actor);

	[[nodiscard]] bool operatorSignal(
		SoldierID owner,
		std::int32_t* targetGridNo);
	[[nodiscard]] bool isValidArtillerySector(
		std::int16_t sectorX,
		std::int16_t sectorY,
		std::int8_t sectorZ,
		std::uint8_t team);
	[[nodiscard]] bool sectorJammed();
	[[nodiscard]] bool playerTeamScanning();
}
