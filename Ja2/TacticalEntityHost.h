#ifndef JA2_TACTICAL_ENTITY_HOST_H
#define JA2_TACTICAL_ENTITY_HOST_H

#include <cstdint>

#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <Engine/Adapters/JA2/TacticalEntityDirectory.h>

class SOLDIERTYPE;

// Composition gateways between the pointer-free runtime directory and JA2's
// fixed SOLDIERTYPE compatibility pool. The directory owns the
// incarnation sequence and latest public actor projection directly; no
// independently synchronized identity or package-facing state path remains.
void BindJa2TacticalEntityDirectory(
	TacticalEntityDirectory& directory) noexcept;
TacticalEntityDirectory& GetJa2TacticalEntityDirectory() noexcept;

std::uint32_t IssueJa2TacticalEntityIncarnation() noexcept;
std::uint32_t NextJa2TacticalEntityIncarnation() noexcept;
void RestoreJa2TacticalEntityIncarnationSequence(
	std::uint32_t nextIncarnation) noexcept;

bool AdoptJa2TacticalEntity(SOLDIERTYPE& soldier) noexcept;
bool ReleaseJa2TacticalEntity(const SOLDIERTYPE& soldier) noexcept;
bool SynchronizeJa2TacticalEntityState(
	const SOLDIERTYPE& soldier) noexcept;
bool SynchronizeJa2TacticalEntityStates() noexcept;
void ResetJa2TacticalEntityDirectory() noexcept;
void RebuildJa2TacticalEntityDirectory() noexcept;
bool SwapJa2TacticalEntitySlots(
	std::uint16_t firstSlot, std::uint16_t secondSlot);

TacticalEntityId GetJa2TacticalEntityId(std::uint16_t slot) noexcept;
TacticalEntityId GetJa2TacticalEntityId(
	const SOLDIERTYPE& soldier) noexcept;
SOLDIERTYPE* ResolveJa2TacticalEntity(TacticalEntityId entity) noexcept;

// Legacy callbacks often outlive the stack frame that selected an actor. This
// value-only reference retains the reusable slot plus its incarnation and
// resolves only while that exact SOLDIERTYPE is still live.
class Ja2TacticalEntityReference
{
public:
	bool capture(const SOLDIERTYPE* soldier) noexcept;
	SOLDIERTYPE* resolve() const noexcept;
	SOLDIERTYPE* consume() noexcept;

	void reset() noexcept { entity_ = {}; }
	TacticalEntityId identity() const noexcept { return entity_; }
	bool valid() const noexcept { return entity_.valid(); }

private:
	TacticalEntityId entity_{};
};

#endif
