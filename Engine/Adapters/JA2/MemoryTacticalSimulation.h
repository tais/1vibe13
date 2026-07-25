#ifndef ENGINE_ADAPTERS_JA2_MEMORY_TACTICAL_SIMULATION_H
#define ENGINE_ADAPTERS_JA2_MEMORY_TACTICAL_SIMULATION_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/SimulationCommandExecutor.h>
#include <Engine/Adapters/JA2/TacticalEntity.h>

struct TacticalSimulationActorState
{
	TacticalEntityId id;
	std::int32_t grid = 0;
	std::int16_t positionX = 0;
	std::int16_t positionY = 0;
	// Command vocabulary intentionally carries the host's stance and movement
	// values. The memory executor preserves them without importing JA2 headers.
	std::uint8_t stance = 0;
	std::uint8_t direction = 0;
	bool stealth = false;
	bool stopped = true;

	friend bool operator==(
		const TacticalSimulationActorState& left,
		const TacticalSimulationActorState& right)
	{
		return left.id == right.id &&
			left.grid == right.grid &&
			left.positionX == right.positionX &&
			left.positionY == right.positionY &&
			left.stance == right.stance &&
			left.direction == right.direction &&
			left.stealth == right.stealth &&
			left.stopped == right.stopped;
	}
};

enum class TacticalSimulationFireKind : std::uint8_t
{
	CurrentSelection,
	CapturedSelection,
	ReplicatedSelection
};

constexpr bool IsValidTacticalSimulationFireKind(
	TacticalSimulationFireKind kind) noexcept
{
	switch (kind)
	{
		case TacticalSimulationFireKind::CurrentSelection:
		case TacticalSimulationFireKind::CapturedSelection:
		case TacticalSimulationFireKind::ReplicatedSelection:
			return true;
	}
	return false;
}

struct TacticalSimulationShot
{
	TacticalEntityId soldier;
	std::int32_t targetGrid = 0;
	std::int8_t targetLevel = 0;
	std::int8_t targetCubeLevel = 0;
	std::uint8_t attackingHand = 0;
	std::uint32_t attackingWeapon = 0;
	TacticalSimulationFireKind kind =
		TacticalSimulationFireKind::CurrentSelection;
	std::uint64_t tick = 0;
	std::uint64_t sequence = 0;

	friend bool operator==(
		const TacticalSimulationShot& left,
		const TacticalSimulationShot& right)
	{
		return left.soldier == right.soldier &&
			left.targetGrid == right.targetGrid &&
			left.targetLevel == right.targetLevel &&
			left.targetCubeLevel == right.targetCubeLevel &&
			left.attackingHand == right.attackingHand &&
			left.attackingWeapon == right.attackingWeapon &&
			left.kind == right.kind &&
			left.tick == right.tick &&
			left.sequence == right.sequence;
	}
};

struct TacticalSimulationSnapshot
{
	std::uint8_t currentTeam = 0;
	bool inCombat = false;
	std::uint32_t completedTurns = 0;
	std::vector<TacticalSimulationActorState> actors;
	std::vector<TacticalSimulationShot> shots;

	friend bool operator==(
		const TacticalSimulationSnapshot& left,
		const TacticalSimulationSnapshot& right)
	{
		return left.currentTeam == right.currentTeam &&
			left.inCombat == right.inCombat &&
			left.completedTurns == right.completedTurns &&
			left.actors == right.actors &&
			left.shots == right.shots;
	}
};

struct TacticalSimulationLimits
{
	static constexpr std::size_t DefaultMaximumActors = 4096;
	static constexpr std::size_t DefaultMaximumShots = 16384;

	std::size_t maximumActors = DefaultMaximumActors;
	std::size_t maximumShots = DefaultMaximumShots;
};

enum class TacticalSimulationResetError
{
	None,
	TooManyActors,
	TooManyShots,
	InvalidActor,
	DuplicateActor,
	InvalidDirection,
	InvalidShotKind,
	UnknownShotActor,
	AllocationFailure
};

// Deterministic, data-free tactical executor for replay tools, package tests,
// and headless hosts. It is deliberately a bounded state model rather than a
// second implementation of JA2 combat rules: unsupported commands are
// discarded, while the portable command subset produces stable value state.
class MemoryTacticalSimulation final : public SimulationCommandExecutor
{
public:
	explicit MemoryTacticalSimulation(
		TacticalSimulationLimits limits = {});

	TacticalSimulationResetError reset(
		TacticalSimulationSnapshot snapshot) noexcept;
	void clear() noexcept;
	void clearShots() noexcept { snapshot_.shots.clear(); }

	const TacticalSimulationSnapshot& snapshot() const noexcept
	{
		return snapshot_;
	}

	const TacticalSimulationLimits& limits() const noexcept
	{
		return limits_;
	}

	CommandDisposition execute(
		const SimulationCommand& command,
		std::uint64_t tick,
		std::uint64_t sequence) override;

private:
	TacticalSimulationActorState* findActor(TacticalEntityId id) noexcept;
	bool recordShot(TacticalSimulationShot shot) noexcept;

	TacticalSimulationLimits limits_;
	TacticalSimulationSnapshot snapshot_;
};

#endif
