#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SNAPSHOT_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SNAPSHOT_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

struct TacticalTurnSnapshot
{
	bool turnBased = false;
	bool inCombat = false;
	std::uint8_t activeTeam = 0;
	std::uint64_t serial = 0;
};

// Pointer-free view of one SOLDIERTYPE. Numeric team/profile/animation values
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
};

enum class TacticalSnapshotCreateError
{
	None,
	InvalidEpoch,
	TooManyActors,
	InvalidEntity,
	DuplicateEntity
};

// Immutable, generation-stamped tactical state for packages, diagnostics, and
// deterministic diffing. Construction sorts by stable entity identity and is
// transactional: rejected input leaves the caller's previous snapshot intact.
class TacticalWorldSnapshot
{
public:
	static constexpr std::size_t DefaultMaximumActors = 4096;

	static TacticalSnapshotCreateError create(
		std::uint64_t epoch,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot> actors,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors)
	{
		if (epoch == 0) return TacticalSnapshotCreateError::InvalidEpoch;
		if (actors.size() > maximumActors) return TacticalSnapshotCreateError::TooManyActors;
		for (const TacticalActorSnapshot& actor : actors)
			if (!actor.id.valid()) return TacticalSnapshotCreateError::InvalidEntity;

		std::sort(actors.begin(), actors.end(),
			[](const TacticalActorSnapshot& left, const TacticalActorSnapshot& right) {
				return left.id < right.id;
			});
		for (std::size_t index = 1; index < actors.size(); ++index)
			if (actors[index - 1].id == actors[index].id)
				return TacticalSnapshotCreateError::DuplicateEntity;

		TacticalWorldSnapshot accepted;
		accepted.epoch_ = epoch;
		accepted.sector_ = sector;
		accepted.turn_ = turn;
		accepted.actors_ = std::move(actors);
		output = std::move(accepted);
		return TacticalSnapshotCreateError::None;
	}

	std::uint64_t epoch() const { return epoch_; }
	const TacticalSectorSnapshot& sector() const { return sector_; }
	const TacticalTurnSnapshot& turn() const { return turn_; }
	const std::vector<TacticalActorSnapshot>& actors() const { return actors_; }

	const TacticalActorSnapshot* find(TacticalEntityId id) const
	{
		const auto actor = std::lower_bound(
			actors_.begin(), actors_.end(), id,
			[](const TacticalActorSnapshot& candidate, TacticalEntityId sought) {
				return candidate.id < sought;
			});
		return actor != actors_.end() && actor->id == id ? &*actor : nullptr;
	}

private:
	std::uint64_t epoch_ = 0;
	TacticalSectorSnapshot sector_;
	TacticalTurnSnapshot turn_;
	std::vector<TacticalActorSnapshot> actors_;
};

#endif
