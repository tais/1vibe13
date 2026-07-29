#ifndef JA2_TACTICAL_ENTITY_HOST_H
#define JA2_TACTICAL_ENTITY_HOST_H

#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <Engine/Adapters/JA2/TacticalEntityDirectory.h>

class TacticalActor;

// Composition gateways between the pointer-free runtime directory and JA2's
// fixed TacticalActor compatibility pool. The directory owns the
// incarnation sequence and latest public actor projection directly; no
// independently synchronized identity or package-facing state path remains.
void BindJa2TacticalEntityDirectory(
	TacticalEntityDirectory& directory) noexcept;
TacticalEntityDirectory& GetJa2TacticalEntityDirectory() noexcept;

std::uint32_t IssueJa2TacticalEntityIncarnation() noexcept;
std::uint32_t NextJa2TacticalEntityIncarnation() noexcept;
void RestoreJa2TacticalEntityIncarnationSequence(
	std::uint32_t nextIncarnation) noexcept;

bool AdoptJa2TacticalEntity(TacticalActor& soldier) noexcept;
bool ReleaseJa2TacticalEntity(const TacticalActor& soldier) noexcept;
bool SynchronizeJa2TacticalEntityState(
	const TacticalActor& soldier) noexcept;
bool SynchronizeJa2TacticalEntityStates() noexcept;
void ResetJa2TacticalEntityDirectory() noexcept;
void RebuildJa2TacticalEntityDirectory() noexcept;
bool SwapJa2TacticalEntitySlots(
	std::uint16_t firstSlot, std::uint16_t secondSlot);

TacticalEntityId GetJa2TacticalEntityId(std::uint16_t slot) noexcept;
TacticalEntityId GetJa2TacticalEntityId(
	const TacticalActor& soldier) noexcept;
TacticalActor* ResolveJa2TacticalEntity(TacticalEntityId entity) noexcept;

// Ordered scheduler membership is runtime-only and deliberately separate
// from both the complete entity directory and strategic squad membership.
// These accessors retain the legacy sparse-slot traversal order without
// exposing process-global TacticalActor pointer arrays.
void ResetJa2TacticalActorRosters() noexcept;

std::size_t Ja2ActiveTacticalActorSlotCount() noexcept;
std::size_t Ja2AwayTacticalActorSlotCount() noexcept;
TacticalActor* ResolveJa2ActiveTacticalActorSlot(
	std::size_t rosterSlot) noexcept;
TacticalActor* ResolveJa2AwayTacticalActorSlot(
	std::size_t rosterSlot) noexcept;

std::int32_t AddJa2ActiveTacticalActor(TacticalEntityId actor) noexcept;
std::int32_t AddJa2AwayTacticalActor(TacticalEntityId actor) noexcept;
bool RemoveJa2ActiveTacticalActor(TacticalEntityId actor) noexcept;
bool RemoveJa2AwayTacticalActor(TacticalEntityId actor) noexcept;

// Legacy callbacks often outlive the stack frame that selected an actor. This
// value-only reference retains the reusable slot plus its incarnation and
// resolves only while that exact TacticalActor is still live. Producers must
// cross the canonical record boundary before retention; the reference itself
// never accepts a raw record.
class Ja2TacticalEntityReference
{
public:
	bool capture(TacticalEntityId entity) noexcept;
	TacticalActor* resolve() const noexcept;
	TacticalActor* consume() noexcept;

	void reset() noexcept { entity_ = {}; }
	TacticalEntityId identity() const noexcept { return entity_; }
	bool valid() const noexcept { return entity_.valid(); }

private:
	TacticalEntityId entity_{};
};

#endif
