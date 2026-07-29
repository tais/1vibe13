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

struct TacticalTurnSnapshot
{
	bool turnBased = false;
	bool inCombat = false;
	std::uint8_t activeTeam = 0;
	std::uint64_t serial = 0;
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
};

enum class TacticalSnapshotCreateError
{
	None,
	InvalidEpoch,
	TooManyActors,
	InvalidEntity,
	DuplicateEntity,
	UnorderedEntity
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

	// Capture adapters can retain their collection scratch and let the output
	// retain its actor allocation. Validation and sorting finish before output is
	// touched; reserve is the only throwing output operation and has the strong
	// exception guarantee. A successful first call reserves the configured actor
	// ceiling, so later captures within that ceiling require no heap allocation.
	static TacticalSnapshotCreateError createReusable(
		std::uint64_t epoch,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot>& actorScratch,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors)
	{
		if (epoch == 0) return TacticalSnapshotCreateError::InvalidEpoch;
		if (actorScratch.size() > maximumActors)
			return TacticalSnapshotCreateError::TooManyActors;
		for (const TacticalActorSnapshot& actor : actorScratch)
			if (!actor.id.valid()) return TacticalSnapshotCreateError::InvalidEntity;

		std::sort(actorScratch.begin(), actorScratch.end(),
			[](const TacticalActorSnapshot& left, const TacticalActorSnapshot& right) {
				return left.id < right.id;
			});
		for (std::size_t index = 1; index < actorScratch.size(); ++index)
			if (actorScratch[index - 1].id == actorScratch[index].id)
				return TacticalSnapshotCreateError::DuplicateEntity;

		static_assert(std::is_nothrow_copy_constructible<TacticalActorSnapshot>::value,
			"reusable tactical capture requires non-throwing actor copies");
		output.actors_.reserve(maximumActors);
		output.actors_.clear();
		for (const TacticalActorSnapshot& actor : actorScratch)
			output.actors_.push_back(actor);
		output.epoch_ = epoch;
		output.sector_ = sector;
		output.turn_ = turn;
		return TacticalSnapshotCreateError::None;
	}

	// JA2's live slot scan already produces strict TacticalEntityId order. This
	// path validates that contract, then exchanges the caller's completed scratch
	// buffer with the output instead of sorting and copying every actor. The
	// caller's scratch is empty after success and retains the output's previous
	// allocation for the next capture.
	static TacticalSnapshotCreateError createReusableOrdered(
		std::uint64_t epoch,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot>& actorScratch,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors)
	{
		if (epoch == 0) return TacticalSnapshotCreateError::InvalidEpoch;
		if (actorScratch.size() > maximumActors)
			return TacticalSnapshotCreateError::TooManyActors;
		for (std::size_t index = 0; index < actorScratch.size(); ++index)
		{
			const TacticalActorSnapshot& actor = actorScratch[index];
			if (!actor.id.valid()) return TacticalSnapshotCreateError::InvalidEntity;
			if (index == 0) continue;
			const TacticalEntityId previous = actorScratch[index - 1].id;
			if (previous == actor.id)
				return TacticalSnapshotCreateError::DuplicateEntity;
			if (!(previous < actor.id))
				return TacticalSnapshotCreateError::UnorderedEntity;
		}

		// Reserve before changing observable values so allocation failure keeps the
		// old snapshot intact. Both buffers retain the configured ceiling after the
		// first successful exchange.
		output.actors_.reserve(maximumActors);
		output.actors_.swap(actorScratch);
		actorScratch.clear();
		output.epoch_ = epoch;
		output.sector_ = sector;
		output.turn_ = turn;
		return TacticalSnapshotCreateError::None;
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
			output.actors_.reserve(actors_.size());
			output.actors_.clear();
			for (const TacticalActorSnapshot& actor : actors_)
				output.actors_.push_back(actor);
			output.epoch_ = epoch_;
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
