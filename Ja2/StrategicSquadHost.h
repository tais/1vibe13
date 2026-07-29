#ifndef JA2_STRATEGIC_SQUAD_HOST_H
#define JA2_STRATEGIC_SQUAD_HOST_H

#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/TacticalEntity.h>

class SOLDIERTYPE;

inline constexpr std::size_t kJa2StrategicSquadCount = 40;
inline constexpr std::size_t kJa2StrategicSquadCapacity = 10;

// Runtime squad membership retains exact tactical entity identities. Legacy
// squad rules remain in Tactical/Squads.cpp; this host owns only the bounded,
// pointer-free membership projection and resolves records at use time.
void ResetJa2StrategicSquadRosters() noexcept;

std::size_t Ja2StrategicSquadSize(std::size_t squad) noexcept;
TacticalEntityId GetJa2StrategicSquadActor(
	std::size_t squad, std::size_t slot) noexcept;
SOLDIERTYPE* ResolveJa2StrategicSquadActor(
	std::size_t squad, std::size_t slot) noexcept;

std::int32_t AddJa2StrategicSquadActor(
	std::size_t squad, TacticalEntityId actor) noexcept;
bool AssignJa2StrategicSquadActor(
	std::size_t squad,
	std::size_t slot,
	TacticalEntityId actor) noexcept;
bool RemoveJa2StrategicSquadActor(
	std::size_t squad, TacticalEntityId actor) noexcept;
bool RemoveJa2StrategicSquadActor(TacticalEntityId actor) noexcept;

bool CompactJa2StrategicSquad(std::size_t squad) noexcept;
bool SortJa2StrategicSquadByIdentity(std::size_t squad) noexcept;

// Whole-record swaps retain fixed legacy addresses. Rebinding by canonical
// repository slot preserves that historical behavior without retaining raw
// pointers or stale incarnations.
void RebindJa2StrategicSquadRostersAfterRecordSwap() noexcept;

#endif
