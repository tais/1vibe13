#ifndef JA2_TACTICAL_ENTITY_HOST_H
#define JA2_TACTICAL_ENTITY_HOST_H

#include <cstdint>

#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <Engine/Adapters/JA2/TacticalEntityDirectory.h>

class SOLDIERTYPE;

// Composition and compatibility gateways between the pointer-free runtime
// directory and JA2's fixed SOLDIERTYPE/MercPtrs pool.
void BindJa2TacticalEntityDirectory(TacticalEntityDirectory& directory) noexcept;
TacticalEntityDirectory& GetJa2TacticalEntityDirectory() noexcept;

std::uint32_t IssueJa2TacticalEntityIncarnation() noexcept;
std::uint32_t NextJa2TacticalEntityIncarnation() noexcept;
void RestoreJa2TacticalEntityIncarnationSequence(
	std::uint32_t nextIncarnation) noexcept;

bool AdoptJa2TacticalEntity(SOLDIERTYPE& soldier) noexcept;
bool ReleaseJa2TacticalEntity(const SOLDIERTYPE& soldier) noexcept;
void ResetJa2TacticalEntityDirectory() noexcept;
void RebuildJa2TacticalEntityDirectory() noexcept;

TacticalEntityId GetJa2TacticalEntityId(std::uint16_t slot) noexcept;
SOLDIERTYPE* ResolveJa2TacticalEntity(TacticalEntityId entity) noexcept;

#endif
