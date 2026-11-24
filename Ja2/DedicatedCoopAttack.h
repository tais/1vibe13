#ifndef JA2_DEDICATED_COOP_ATTACK_H
#define JA2_DEDICATED_COOP_ATTACK_H

#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <cstdint>

class TacticalActor;

enum class DedicatedCoopAttackOutcome
{
	Untracked,
	Pending,
	Completed,
	Interrupted,
	Invalid
};

// One native attack chain at a time. Retain its terminal record until the
// corresponding remote receipt has crossed the outbound boundary. Sequence
// zero is valid; occupancy is independent of the sequence value.
class DedicatedCoopAttackState final
{
public:
	DedicatedCoopAttackState() = default;
	~DedicatedCoopAttackState();
	DedicatedCoopAttackState(const DedicatedCoopAttackState&) = delete;
	DedicatedCoopAttackState& operator=(const DedicatedCoopAttackState&) = delete;
	const char* failure() const noexcept { return failure_; }
	void reset() noexcept;

private:
	friend bool BeginDedicatedCoopAttack(std::uint64_t, TacticalActor&) noexcept;
	friend void AbandonDedicatedCoopAttack(std::uint64_t) noexcept;
	friend bool ResolveDedicatedCoopAttackOwner(TacticalActor*&) noexcept;
	friend void CompleteDedicatedCoopAttack(TacticalActor&) noexcept;
	friend void RecordDedicatedCoopAttackAmmunitionUse(TacticalActor&, std::uint16_t) noexcept;
	friend bool DedicatedCoopAttackPending() noexcept;
	friend DedicatedCoopAttackOutcome CaptureDedicatedCoopAttackOutcome(
		std::uint64_t, std::uint64_t, TacticalEntityId) noexcept;
	friend bool ReleaseDedicatedCoopAttack(std::uint64_t, std::uint64_t, TacticalEntityId) noexcept;
	bool occupied_ = false;
	std::uint64_t sequence_ = 0;
	std::uint64_t world_ = 0;
	TacticalEntityId actor_{};
	std::uint16_t handItem_ = 0;
	std::uint16_t ammunition_ = 0;
	bool roundCommitted_ = false;
	DedicatedCoopAttackOutcome outcome_ = DedicatedCoopAttackOutcome::Untracked;
	const char* failure_ = nullptr;
};

bool BindDedicatedCoopAttackState(DedicatedCoopAttackState&) noexcept;
void UnbindDedicatedCoopAttackState(DedicatedCoopAttackState&) noexcept;
bool DedicatedCoopAttackPending() noexcept;
bool BeginDedicatedCoopAttack(std::uint64_t sequence, TacticalActor& actor) noexcept;
void AbandonDedicatedCoopAttack(std::uint64_t sequence) noexcept;
// true means this chain belongs to the co-op tracker, including a failed
// resolution. A null output must never fall back to the UI-selected actor.
bool ResolveDedicatedCoopAttackOwner(TacticalActor*& actor) noexcept;
void CompleteDedicatedCoopAttack(TacticalActor& actor) noexcept;
// Called by native gun ammunition accounting, including external feeding.
// This observes a native discharge; it does not claim a hit or kill.
void RecordDedicatedCoopAttackAmmunitionUse(TacticalActor& actor, std::uint16_t item) noexcept;
DedicatedCoopAttackOutcome CaptureDedicatedCoopAttackOutcome(
	std::uint64_t sequence, std::uint64_t world, TacticalEntityId actor) noexcept;
bool ReleaseDedicatedCoopAttack(
	std::uint64_t sequence, std::uint64_t world, TacticalEntityId actor) noexcept;

#endif
