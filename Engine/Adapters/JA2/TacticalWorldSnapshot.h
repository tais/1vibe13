#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SNAPSHOT_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SNAPSHOT_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

#include <Engine/Adapters/JA2/TacticalEntity.h>

enum class TacticalStance : std::uint8_t
{
	Unknown,
	Standing,
	Crouched,
	Prone
};

struct TacticalSectorSnapshot
{
	std::int16_t x = 0;
	std::int16_t y = 0;
	std::int8_t z = -1;
	bool loaded = false;
};

enum class TacticalInterruptPhase : std::uint8_t
{
	None = 0,
	Resolving = 1,
	Active = 2
};

struct TacticalTurnSnapshot
{
	bool turnBased = false;
	bool inCombat = false;
	std::uint8_t activeTeam = 0;
	std::uint64_t serial = 0;
	// Canonical public projection of the authority's input-busy predicate. Raw
	// pending-action counts and interrupt kinds remain private to the live host.
	bool commandsBlocked = false;
	TacticalInterruptPhase interruptPhase = TacticalInterruptPhase::None;
	std::uint64_t interruptSerial = 0;
};

// Exact logical tile extent of one loaded tactical world. These dimensions are
// authority data: passive clients must not consult their cold local JA2 world
// globals to interpret replicated grid numbers. JA2's supported enlarged-map
// ceiling is part of this adapter contract and keeps rows * columns in int32.
struct TacticalWorldDimensions
{
	static constexpr std::uint16_t MaximumColumns = 2000;
	static constexpr std::uint16_t MaximumRows = 2000;

	std::uint16_t columns = 0;
	std::uint16_t rows = 0;

	bool valid() const noexcept
	{
		return columns != 0 && rows != 0 &&
			columns <= MaximumColumns && rows <= MaximumRows;
	}

	bool contains(std::int32_t grid) const noexcept
	{
		return valid() && grid >= 0 &&
			static_cast<std::uint32_t>(grid) <
				static_cast<std::uint32_t>(columns) * rows;
	}
};

// Bounded public projection of the first object in one fixed tactical
// combat-equipment slot (helmet, vest, legs, or either hand).
// ammunitionState distinguishes an ammo-bearing object with no loaded rounds
// from an ordinary item whose union-backed ammunition fields are meaningless.
struct TacticalHandItemSnapshot
{
	std::uint16_t item = 0;
	std::uint8_t quantity = 0;
	std::int16_t condition = 0;
	std::uint16_t ammunitionItem = 0;
	std::uint16_t ammunitionCount = 0;
	std::int16_t ammunitionCondition = 0;
	bool ammunitionState = false;
	bool chambered = false;

	bool valid() const noexcept
	{
		if (item == 0)
			return quantity == 0 && condition == 0 &&
				ammunitionItem == 0 && ammunitionCount == 0 &&
				ammunitionCondition == 0 && !ammunitionState && !chambered;
		if (quantity == 0) return false;
		return ammunitionState ||
			(ammunitionItem == 0 && ammunitionCount == 0 &&
			 ammunitionCondition == 0 && !chambered);
	}

	bool operator==(const TacticalHandItemSnapshot& other) const noexcept
	{
		return item == other.item && quantity == other.quantity &&
			condition == other.condition &&
			ammunitionItem == other.ammunitionItem &&
			ammunitionCount == other.ammunitionCount &&
			ammunitionCondition == other.ammunitionCondition &&
			ammunitionState == other.ammunitionState &&
			chambered == other.chambered;
	}

	bool operator!=(const TacticalHandItemSnapshot& other) const noexcept
	{
		return !(*this == other);
	}
};

struct TacticalActorLoadoutSnapshot
{
	TacticalHandItemSnapshot helmet;
	TacticalHandItemSnapshot vest;
	TacticalHandItemSnapshot legs;
	TacticalHandItemSnapshot primaryHand;
	TacticalHandItemSnapshot secondaryHand;

	bool valid() const noexcept
	{
		return helmet.valid() && vest.valid() && legs.valid() &&
			primaryHand.valid() && secondaryHand.valid();
	}

	bool operator==(const TacticalActorLoadoutSnapshot& other) const noexcept
	{
		return helmet == other.helmet && vest == other.vest &&
			legs == other.legs && primaryHand == other.primaryHand &&
			secondaryHand == other.secondaryHand;
	}

	bool operator!=(const TacticalActorLoadoutSnapshot& other) const noexcept
	{
		return !(*this == other);
	}
};

// Pointer-free view of one TacticalActor. Numeric team/profile/animation values
// remain adapter data so Core and package code never depend on legacy headers.
struct TacticalActorSnapshot
{
	TacticalEntityId id;
	std::uint8_t team = 0;
	std::uint16_t profile = 0;
	std::int32_t grid = -1;
	std::int8_t level = 0;
	std::uint8_t direction = 0;
	std::uint16_t animation = 0;
	TacticalStance stance = TacticalStance::Unknown;
	std::int16_t actionPoints = 0;
	std::int16_t life = 0;
	std::int16_t maximumLife = 0;
	std::int16_t breath = 0;
	std::int16_t maximumBreath = 0;
	bool active = false;
	bool inSector = false;
	// Canonical JA2 authority predicate (OK_ENEMY_MERC), projected so passive
	// clients never infer hostility from numeric team membership.
	bool hostileToPlayerTeam = false;
	bool interruptActionEligible = false;
	TacticalActorLoadoutSnapshot loadout;
};

inline bool IsValidTacticalInterruptPhase(TacticalInterruptPhase phase) noexcept
{
	return phase == TacticalInterruptPhase::None ||
		phase == TacticalInterruptPhase::Resolving ||
		phase == TacticalInterruptPhase::Active;
}

inline bool IsValidTacticalInterruptState(
	const TacticalTurnSnapshot& turn) noexcept
{
	return IsValidTacticalInterruptPhase(turn.interruptPhase) &&
		(turn.interruptPhase == TacticalInterruptPhase::None ||
		 (turn.turnBased && turn.inCombat &&
		  (turn.interruptPhase != TacticalInterruptPhase::Active ||
		   turn.interruptSerial != 0)));
}

inline bool IsValidTacticalInterruptEligibility(
	const TacticalActorSnapshot& actor,
	const TacticalTurnSnapshot& turn) noexcept
{
	return !actor.interruptActionEligible ||
		(turn.interruptPhase == TacticalInterruptPhase::Active && actor.active &&
		 actor.inSector && actor.team == turn.activeTeam);
}

// Public door state deliberately excludes lock, key, trap, perceived-state,
// and structure-database details. baseGrid is the logical identity for the
// current world generation; structureId is an ephemeral optimistic token that
// changes when JA2 swaps the open/closed partner structure.
struct TacticalDoorSnapshot
{
	std::int32_t baseGrid = -1;
	std::uint16_t structureId = 0;
	bool open = false;
};

enum class TacticalSnapshotCreateError
{
	None,
	InvalidEpoch,
	InvalidDimensions,
	InvalidTurn,
	TooManyActors,
	InvalidEntity,
	DuplicateEntity,
	UnorderedEntity,
	TooManyDoors,
	InvalidDoor,
	DuplicateDoor,
	UnorderedDoor
};

// Immutable, generation-stamped tactical state for packages, diagnostics, and
// deterministic diffing. Construction sorts by stable entity identity and is
// transactional: rejected input leaves the caller's previous snapshot intact.
class TacticalWorldSnapshot
{
public:
	static constexpr std::size_t DefaultMaximumActors = 4096;
	static constexpr std::size_t DefaultMaximumDoors = 1024;

	static TacticalSnapshotCreateError create(
		std::uint64_t epoch,
		TacticalWorldDimensions dimensions,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot> actors,
		std::vector<TacticalDoorSnapshot> doors,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors,
		std::size_t maximumDoors = DefaultMaximumDoors)
	{
		if (epoch == 0) return TacticalSnapshotCreateError::InvalidEpoch;
		if (!dimensions.valid())
			return TacticalSnapshotCreateError::InvalidDimensions;
		if (!IsValidTacticalInterruptState(turn))
			return TacticalSnapshotCreateError::InvalidTurn;
		if (actors.size() > maximumActors) return TacticalSnapshotCreateError::TooManyActors;
		if (doors.size() > maximumDoors) return TacticalSnapshotCreateError::TooManyDoors;
		for (const TacticalActorSnapshot& actor : actors)
			if (!actor.id.valid() || !actor.loadout.valid() ||
				!IsValidTacticalInterruptEligibility(actor, turn))
				return TacticalSnapshotCreateError::InvalidEntity;
		for (const TacticalDoorSnapshot& door : doors)
			if (!dimensions.contains(door.baseGrid) || door.structureId == 0)
				return TacticalSnapshotCreateError::InvalidDoor;

		std::sort(actors.begin(), actors.end(),
			[](const TacticalActorSnapshot& left, const TacticalActorSnapshot& right) {
				return left.id < right.id;
			});
		for (std::size_t index = 1; index < actors.size(); ++index)
			if (actors[index - 1].id == actors[index].id)
				return TacticalSnapshotCreateError::DuplicateEntity;
		std::sort(doors.begin(), doors.end(),
			[](const TacticalDoorSnapshot& left, const TacticalDoorSnapshot& right) {
				return left.baseGrid < right.baseGrid;
			});
		for (std::size_t index = 1; index < doors.size(); ++index)
			if (doors[index - 1].baseGrid == doors[index].baseGrid)
				return TacticalSnapshotCreateError::DuplicateDoor;

		TacticalWorldSnapshot accepted;
		accepted.epoch_ = epoch;
		accepted.dimensions_ = dimensions;
		accepted.sector_ = sector;
		accepted.turn_ = turn;
		accepted.actors_ = std::move(actors);
		accepted.doors_ = std::move(doors);
		output = std::move(accepted);
		return TacticalSnapshotCreateError::None;
	}

	static TacticalSnapshotCreateError create(
		std::uint64_t epoch,
		TacticalWorldDimensions dimensions,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot> actors,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors)
	{
		return create(epoch, dimensions, sector, turn, std::move(actors), {},
			output, maximumActors, DefaultMaximumDoors);
	}

	// Capture adapters can retain their collection scratch and let the output
	// retain its actor allocation. Validation and sorting finish before output is
	// touched; reserve is the only throwing output operation and has the strong
	// exception guarantee. A successful first call reserves the configured actor
	// ceiling, so later captures within that ceiling require no heap allocation.
	static TacticalSnapshotCreateError createReusable(
		std::uint64_t epoch,
		TacticalWorldDimensions dimensions,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot>& actorScratch,
		std::vector<TacticalDoorSnapshot>& doorScratch,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors,
		std::size_t maximumDoors = DefaultMaximumDoors)
	{
		if (epoch == 0) return TacticalSnapshotCreateError::InvalidEpoch;
		if (!dimensions.valid())
			return TacticalSnapshotCreateError::InvalidDimensions;
		if (!IsValidTacticalInterruptState(turn))
			return TacticalSnapshotCreateError::InvalidTurn;
		if (actorScratch.size() > maximumActors)
			return TacticalSnapshotCreateError::TooManyActors;
		if (doorScratch.size() > maximumDoors)
			return TacticalSnapshotCreateError::TooManyDoors;
		for (const TacticalActorSnapshot& actor : actorScratch)
			if (!actor.id.valid() || !actor.loadout.valid() ||
				!IsValidTacticalInterruptEligibility(actor, turn))
				return TacticalSnapshotCreateError::InvalidEntity;
		for (const TacticalDoorSnapshot& door : doorScratch)
			if (!dimensions.contains(door.baseGrid) || door.structureId == 0)
				return TacticalSnapshotCreateError::InvalidDoor;

		std::sort(actorScratch.begin(), actorScratch.end(),
			[](const TacticalActorSnapshot& left, const TacticalActorSnapshot& right) {
				return left.id < right.id;
			});
		for (std::size_t index = 1; index < actorScratch.size(); ++index)
			if (actorScratch[index - 1].id == actorScratch[index].id)
				return TacticalSnapshotCreateError::DuplicateEntity;
		std::sort(doorScratch.begin(), doorScratch.end(),
			[](const TacticalDoorSnapshot& left, const TacticalDoorSnapshot& right) {
				return left.baseGrid < right.baseGrid;
			});
		for (std::size_t index = 1; index < doorScratch.size(); ++index)
			if (doorScratch[index - 1].baseGrid == doorScratch[index].baseGrid)
				return TacticalSnapshotCreateError::DuplicateDoor;

		static_assert(std::is_nothrow_copy_constructible<TacticalActorSnapshot>::value,
			"reusable tactical capture requires non-throwing actor copies");
		static_assert(std::is_nothrow_copy_constructible<TacticalDoorSnapshot>::value,
			"reusable tactical capture requires non-throwing door copies");
		output.actors_.reserve(maximumActors);
		output.doors_.reserve(maximumDoors);
		output.actors_.clear();
		output.doors_.clear();
		for (const TacticalActorSnapshot& actor : actorScratch)
			output.actors_.push_back(actor);
		for (const TacticalDoorSnapshot& door : doorScratch)
			output.doors_.push_back(door);
		output.epoch_ = epoch;
		output.dimensions_ = dimensions;
		output.sector_ = sector;
		output.turn_ = turn;
		return TacticalSnapshotCreateError::None;
	}

	static TacticalSnapshotCreateError createReusable(
		std::uint64_t epoch,
		TacticalWorldDimensions dimensions,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot>& actorScratch,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors)
	{
		std::vector<TacticalDoorSnapshot> doors;
		return createReusable(epoch, dimensions, sector, turn, actorScratch,
			doors, output, maximumActors, DefaultMaximumDoors);
	}

	// JA2's live slot scan already produces strict TacticalEntityId order. This
	// path validates that contract, then exchanges the caller's completed scratch
	// buffer with the output instead of sorting and copying every actor. The
	// caller's scratch is empty after success and retains the output's previous
	// allocation for the next capture.
	static TacticalSnapshotCreateError createReusableOrdered(
		std::uint64_t epoch,
		TacticalWorldDimensions dimensions,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot>& actorScratch,
		std::vector<TacticalDoorSnapshot>& doorScratch,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors,
		std::size_t maximumDoors = DefaultMaximumDoors)
	{
		if (epoch == 0) return TacticalSnapshotCreateError::InvalidEpoch;
		if (!dimensions.valid())
			return TacticalSnapshotCreateError::InvalidDimensions;
		if (!IsValidTacticalInterruptState(turn))
			return TacticalSnapshotCreateError::InvalidTurn;
		if (actorScratch.size() > maximumActors)
			return TacticalSnapshotCreateError::TooManyActors;
		if (doorScratch.size() > maximumDoors)
			return TacticalSnapshotCreateError::TooManyDoors;
		for (std::size_t index = 0; index < actorScratch.size(); ++index)
		{
			const TacticalActorSnapshot& actor = actorScratch[index];
			if (!actor.id.valid() || !actor.loadout.valid() ||
				!IsValidTacticalInterruptEligibility(actor, turn))
				return TacticalSnapshotCreateError::InvalidEntity;
			if (index == 0) continue;
			const TacticalEntityId previous = actorScratch[index - 1].id;
			if (previous == actor.id)
				return TacticalSnapshotCreateError::DuplicateEntity;
			if (!(previous < actor.id))
				return TacticalSnapshotCreateError::UnorderedEntity;
		}
		for (std::size_t index = 0; index < doorScratch.size(); ++index)
		{
			const TacticalDoorSnapshot& door = doorScratch[index];
			if (!dimensions.contains(door.baseGrid) || door.structureId == 0)
				return TacticalSnapshotCreateError::InvalidDoor;
			if (index == 0) continue;
			const std::int32_t previous = doorScratch[index - 1].baseGrid;
			if (previous == door.baseGrid)
				return TacticalSnapshotCreateError::DuplicateDoor;
			if (previous > door.baseGrid)
				return TacticalSnapshotCreateError::UnorderedDoor;
		}

		// Reserve before changing observable values so allocation failure keeps the
		// old snapshot intact. Both buffers retain the configured ceiling after the
		// first successful exchange.
		output.actors_.reserve(maximumActors);
		output.doors_.reserve(maximumDoors);
		output.actors_.swap(actorScratch);
		output.doors_.swap(doorScratch);
		actorScratch.clear();
		doorScratch.clear();
		output.epoch_ = epoch;
		output.dimensions_ = dimensions;
		output.sector_ = sector;
		output.turn_ = turn;
		return TacticalSnapshotCreateError::None;
	}

	static TacticalSnapshotCreateError createReusableOrdered(
		std::uint64_t epoch,
		TacticalWorldDimensions dimensions,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot>& actorScratch,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors)
	{
		std::vector<TacticalDoorSnapshot> doors;
		return createReusableOrdered(epoch, dimensions, sector, turn,
			actorScratch, doors, output, maximumActors, DefaultMaximumDoors);
	}

	// Copy into caller-owned reusable storage without exposing mutable snapshot
	// internals. Allocation failure leaves all observable output values intact.
	bool copyTo(TacticalWorldSnapshot& output) const noexcept
	{
		if (&output == this) return true;
		try
		{
			static_assert(std::is_nothrow_copy_constructible<TacticalActorSnapshot>::value,
				"reusable tactical copies require non-throwing actor copies");
			static_assert(std::is_nothrow_copy_constructible<TacticalDoorSnapshot>::value,
				"reusable tactical copies require non-throwing door copies");
			output.actors_.reserve(actors_.size());
			output.doors_.reserve(doors_.size());
			output.actors_.clear();
			output.doors_.clear();
			for (const TacticalActorSnapshot& actor : actors_)
				output.actors_.push_back(actor);
			for (const TacticalDoorSnapshot& door : doors_)
				output.doors_.push_back(door);
			output.epoch_ = epoch_;
			output.dimensions_ = dimensions_;
			output.sector_ = sector_;
			output.turn_ = turn_;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	std::uint64_t epoch() const { return epoch_; }
	const TacticalWorldDimensions& dimensions() const { return dimensions_; }
	const TacticalSectorSnapshot& sector() const { return sector_; }
	const TacticalTurnSnapshot& turn() const { return turn_; }
	const std::vector<TacticalActorSnapshot>& actors() const { return actors_; }
	const std::vector<TacticalDoorSnapshot>& doors() const { return doors_; }

	const TacticalActorSnapshot* find(TacticalEntityId id) const
	{
		const auto actor = std::lower_bound(
			actors_.begin(), actors_.end(), id,
			[](const TacticalActorSnapshot& candidate, TacticalEntityId sought) {
				return candidate.id < sought;
			});
		return actor != actors_.end() && actor->id == id ? &*actor : nullptr;
	}

	const TacticalDoorSnapshot* findDoor(std::int32_t baseGrid) const
	{
		const auto door = std::lower_bound(
			doors_.begin(), doors_.end(), baseGrid,
			[](const TacticalDoorSnapshot& candidate, std::int32_t sought) {
				return candidate.baseGrid < sought;
			});
		return door != doors_.end() && door->baseGrid == baseGrid
			? &*door : nullptr;
	}

private:
	std::uint64_t epoch_ = 0;
	TacticalWorldDimensions dimensions_;
	TacticalSectorSnapshot sector_;
	TacticalTurnSnapshot turn_;
	std::vector<TacticalActorSnapshot> actors_;
	std::vector<TacticalDoorSnapshot> doors_;
};

#endif
