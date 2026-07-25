#ifndef JA2_TACTICAL_WORLD_ITEM_HOST_H
#define JA2_TACTICAL_WORLD_ITEM_HOST_H

#include <cstdint>

#include <Engine/Adapters/JA2/TacticalWorldItem.h>
#include <Engine/Adapters/JA2/TacticalWorldItemDirectory.h>

class WORLDITEM;

// Composition gateways between the engine-owned identity directory and JA2's
// reusable gWorldItems vector. WORLDITEM retains only the compatibility mirror
// of its current incarnation; the directory owns liveness and sequencing.
void BindJa2TacticalWorldItemDirectory(
	TacticalWorldItemDirectory& directory) noexcept;
TacticalWorldItemDirectory& GetJa2TacticalWorldItemDirectory() noexcept;

std::uint32_t IssueJa2TacticalWorldItemIncarnation() noexcept;
bool AssignJa2TacticalWorldItemIdentity(std::uint32_t slot) noexcept;
bool AdoptJa2TacticalWorldItem(std::uint32_t slot) noexcept;
bool ReleaseJa2TacticalWorldItem(std::uint32_t slot) noexcept;
void ResetJa2TacticalWorldItemDirectory() noexcept;
void RebuildJa2TacticalWorldItemDirectory() noexcept;

TacticalWorldItemId GetJa2TacticalWorldItemId(std::uint32_t slot) noexcept;
WORLDITEM* ResolveJa2TacticalWorldItem(TacticalWorldItemId item) noexcept;

#endif
