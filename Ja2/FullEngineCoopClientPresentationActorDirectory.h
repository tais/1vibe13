#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_ACTOR_DIRECTORY_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_ACTOR_DIRECTORY_H

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

// JA2's legacy renderer and tactical panels resolve soldiers through numeric
// repository slots.  A passive authoritative client therefore needs an
// explicit, incarnation-aware directory between those process-local slots and
// the server identities carried by a committed snapshot.  The directory owns
// no TacticalActor and performs no renderer mutation; it only produces a
// bounded detach-before-upsert plan for a presentation-owned actor bank.
inline constexpr std::size_t
	MaximumFullEngineCoopClientPresentationActorSlots = 2048;

enum class FullEngineCoopClientPresentationActorDirectoryResult : std::uint8_t
{
	Success,
	InvalidSnapshot,
	InvalidCapacity,
	ActorSlotOutOfRange,
	DuplicateActorSlot,
	InvalidActorGrid,
	StalePlan
};

enum class FullEngineCoopClientPresentationActorOperationKind : std::uint8_t
{
	Detach,
	Attach,
	Refresh
};

struct FullEngineCoopClientPresentationActorOperation
{
	FullEngineCoopClientPresentationActorOperationKind kind =
		FullEngineCoopClientPresentationActorOperationKind::Detach;
	TacticalEntityId authorityActor;
	std::uint16_t localSlot = 0;
};

class FullEngineCoopClientPresentationActorDirectory;

class FullEngineCoopClientPresentationActorDirectoryPlan final
{
public:
	std::uint64_t worldGeneration() const noexcept
	{
		return worldGeneration_;
	}
	std::size_t localSlotCapacity() const noexcept
	{
		return localSlotCapacity_;
	}
	std::size_t operationCount() const noexcept { return operationCount_; }
	const FullEngineCoopClientPresentationActorOperation* operation(
		std::size_t index) const noexcept
	{
		return index < operationCount_ ? &operations_[index] : nullptr;
	}
	TacticalEntityId desiredActorAt(std::size_t localSlot) const noexcept
	{
		return localSlot < localSlotCapacity_ ? desired_[localSlot] :
			TacticalEntityId{};
	}

private:
	friend class FullEngineCoopClientPresentationActorDirectory;
	const FullEngineCoopClientPresentationActorDirectory* origin_ = nullptr;

	std::array<TacticalEntityId,
		MaximumFullEngineCoopClientPresentationActorSlots> desired_{};
	std::array<FullEngineCoopClientPresentationActorOperation,
		MaximumFullEngineCoopClientPresentationActorSlots * 2> operations_{};
	std::uint64_t worldGeneration_ = 0;
	std::uint64_t baseStateSerial_ = 0;
	std::size_t localSlotCapacity_ = 0;
	std::size_t operationCount_ = 0;
	bool valid_ = false;
};

class FullEngineCoopClientPresentationActorDirectory final
{
public:
	FullEngineCoopClientPresentationActorDirectory() = default;
	FullEngineCoopClientPresentationActorDirectory(
		const FullEngineCoopClientPresentationActorDirectory&) = delete;
	FullEngineCoopClientPresentationActorDirectory& operator=(
		const FullEngineCoopClientPresentationActorDirectory&) = delete;
	FullEngineCoopClientPresentationActorDirectory(
		FullEngineCoopClientPresentationActorDirectory&&) = delete;
	FullEngineCoopClientPresentationActorDirectory& operator=(
		FullEngineCoopClientPresentationActorDirectory&&) = delete;

	// Transactional and allocation-free.  Failure leaves output untouched.
	// Only active in-sector actors become renderer proxies. A dead actor with
	// the canonical absent pose and NOWHERE grid has had its native graphic
	// removed and releases its old proxy. Every actor ID still participates in
	// duplicate-slot and local-capacity validation.
	FullEngineCoopClientPresentationActorDirectoryResult stage(
		const TacticalWorldSnapshot& snapshot,
		std::size_t localSlotCapacity,
		FullEngineCoopClientPresentationActorDirectoryPlan& output) const
		noexcept;

	// Commit only after every operation has succeeded against the hidden proxy
	// bank. A plan belongs to this exact directory and must not outlive it.
	// Older-state and foreign-directory plans are rejected. The directory stays
	// in place so copying/moving cannot bypass state-serial invalidation. No
	// lifetime protection is promised after destruction/address reuse.
	FullEngineCoopClientPresentationActorDirectoryResult commit(
		const FullEngineCoopClientPresentationActorDirectoryPlan& plan) noexcept;

	void reset() noexcept;

	std::uint64_t worldGeneration() const noexcept
	{
		return worldGeneration_;
	}
	std::size_t localSlotCapacity() const noexcept
	{
		return localSlotCapacity_;
	}
	TacticalEntityId authorityActorAt(std::size_t localSlot) const noexcept
	{
		return localSlot < localSlotCapacity_ ? actors_[localSlot] :
			TacticalEntityId{};
	}
	std::uint16_t localSlotFor(TacticalEntityId authorityActor) const noexcept
	{
		return authorityActor.valid() &&
			authorityActor.slot < localSlotCapacity_ &&
			actors_[authorityActor.slot] == authorityActor
			? authorityActor.slot : std::numeric_limits<std::uint16_t>::max();
	}

private:
	std::array<TacticalEntityId,
		MaximumFullEngineCoopClientPresentationActorSlots> actors_{};
	std::uint64_t worldGeneration_ = 0;
	std::uint64_t stateSerial_ = 1;
	std::size_t localSlotCapacity_ = 0;
};

#endif
