#include "FullEngineCoopClientTacticalPresentation.h"

#include <algorithm>
#include <limits>

namespace
{
bool ValidBounds(const FullEngineCoopClientTacticalPlotBounds& bounds) noexcept
{
	return bounds.x >= 0 && bounds.y >= 0 &&
		bounds.width >= 3 && bounds.height >= 3 &&
		bounds.x <= std::numeric_limits<std::int32_t>::max() - bounds.width &&
		bounds.y <= std::numeric_limits<std::int32_t>::max() - bounds.height;
}

bool SpatiallyPresent(const TacticalActorSnapshot& actor,
	const TacticalWorldDimensions& dimensions) noexcept
{
	return actor.id.valid() && actor.active && actor.inSector &&
		dimensions.contains(actor.grid) && actor.direction < 8;
}

bool ValidStance(TacticalStance stance) noexcept
{
	switch (stance)
	{
		case TacticalStance::Unknown:
		case TacticalStance::Standing:
		case TacticalStance::Crouched:
		case TacticalStance::Prone:
			return true;
	}
	return false;
}

bool ValidInterruptPhase(TacticalInterruptPhase phase) noexcept
{
	switch (phase)
	{
		case TacticalInterruptPhase::None:
		case TacticalInterruptPhase::Resolving:
		case TacticalInterruptPhase::Active:
			return true;
	}
	return false;
}

bool Assigned(const TacticalEntityId* actors, std::size_t count,
	TacticalEntityId actor) noexcept
{
	return std::binary_search(actors, actors + count, actor);
}

std::int32_t RoundedProjection(std::int32_t origin,
	std::int32_t extent, std::uint64_t numerator,
	std::uint64_t denominator) noexcept
{
	return origin + static_cast<std::int32_t>(
		(numerator * static_cast<std::uint64_t>(extent) + denominator / 2) /
		denominator);
}

void Project(std::uint16_t column, std::uint16_t row,
	const TacticalWorldDimensions& dimensions,
	const FullEngineCoopClientTacticalPlotBounds& bounds,
	std::int32_t& x, std::int32_t& y) noexcept
{
	const std::uint64_t columnDenominator =
		dimensions.columns > 1 ? dimensions.columns - 1 : 1;
	const std::uint64_t rowDenominator =
		dimensions.rows > 1 ? dimensions.rows - 1 : 1;
	const std::uint64_t product = columnDenominator * rowDenominator;
	const std::uint64_t denominator = 2 * product;
	const std::uint64_t columnTerm =
		static_cast<std::uint64_t>(column) * rowDenominator;
	const std::uint64_t rowTerm =
		static_cast<std::uint64_t>(row) * columnDenominator;
	const std::uint64_t xNumerator = product + columnTerm - rowTerm;
	const std::uint64_t yNumerator = columnTerm + rowTerm;
	x = RoundedProjection(bounds.x, bounds.width - 1,
		xNumerator, denominator);
	y = RoundedProjection(bounds.y, bounds.height - 1,
		yNumerator, denominator);
}
}

FullEngineCoopClientTacticalPresentationResult
BuildFullEngineCoopClientTacticalPresentation(
	const TacticalWorldSnapshot& snapshot,
	const TacticalEntityId* assignedActors,
	std::size_t assignedActorCount,
	TacticalEntityId selectedActor,
	FullEngineCoopClientTacticalPlotBounds bounds,
	FullEngineCoopClientTacticalPresentation& output) noexcept
{
	if (!ValidBounds(bounds) ||
		(assignedActorCount != 0 && assignedActors == nullptr) ||
		assignedActorCount > CoopSession::MaximumCoopTacticalAssignedActors)
		return FullEngineCoopClientTacticalPresentationResult::InvalidInput;
	if (snapshot.epoch() == 0 || !snapshot.sector().loaded ||
		!snapshot.dimensions().valid() ||
		!ValidInterruptPhase(snapshot.turn().interruptPhase) ||
		snapshot.actors().size() >
			CoopSession::MaximumCoopTacticalSnapshotActors)
		return FullEngineCoopClientTacticalPresentationResult::InvalidSnapshot;

	FullEngineCoopClientTacticalPresentation accepted;
	accepted.bounds = bounds;
	accepted.dimensions = snapshot.dimensions();
	accepted.interruptPhase = snapshot.turn().interruptPhase;
	accepted.interruptSerial = snapshot.turn().interruptSerial;
	for (std::size_t index = 0; index < assignedActorCount; ++index)
	{
		if (!assignedActors[index].valid() ||
			(index != 0 && !(assignedActors[index - 1] < assignedActors[index])))
			return FullEngineCoopClientTacticalPresentationResult::InvalidAssignments;
		const TacticalActorSnapshot* const actor =
			snapshot.find(assignedActors[index]);
		if (actor == nullptr ||
			!SpatiallyPresent(*actor, snapshot.dimensions()) ||
			!ValidStance(actor->stance) ||
			(accepted.hasFriendlyTeam && actor->team != accepted.friendlyTeam))
			return FullEngineCoopClientTacticalPresentationResult::InvalidAssignments;
		accepted.friendlyTeam = actor->team;
		accepted.hasFriendlyTeam = true;
	}

	TacticalEntityId previous{};
	bool havePrevious = false;
	for (const TacticalActorSnapshot& actor : snapshot.actors())
	{
		if (!actor.id.valid() || !actor.active || !actor.inSector ||
			(havePrevious && !(previous < actor.id)))
			return FullEngineCoopClientTacticalPresentationResult::InvalidSnapshot;
		previous = actor.id;
		havePrevious = true;
		// Actors can transiently retain in-sector membership while their legacy
		// placement is NOWHERE. They remain valid replica records but have no
		// honest point on this plot until a later authoritative movement delta.
		if (!SpatiallyPresent(actor, snapshot.dimensions()) ||
			!ValidStance(actor.stance))
			continue;
		if (!accepted.hasFriendlyTeam || actor.team != accepted.friendlyTeam)
			continue;
		if (accepted.markerCount == accepted.markers.size())
			return FullEngineCoopClientTacticalPresentationResult::CapacityReached;

		FullEngineCoopClientTacticalPlotMarker marker;
		marker.actor = actor.id;
		marker.column = static_cast<std::uint16_t>(
			actor.grid % snapshot.dimensions().columns);
		marker.row = static_cast<std::uint16_t>(
			actor.grid / snapshot.dimensions().columns);
		Project(marker.column, marker.row, snapshot.dimensions(), bounds,
			marker.screenX, marker.screenY);
		marker.profile = actor.profile;
		marker.actionPoints = actor.actionPoints;
		marker.life = actor.life;
		marker.maximumLife = actor.maximumLife;
		marker.level = actor.level;
		marker.direction = actor.direction;
		marker.stance = actor.stance;
		marker.assigned = Assigned(
			assignedActors, assignedActorCount, actor.id);
		marker.selected = actor.id == selectedActor;
		marker.interruptActionEligible = actor.interruptActionEligible;
		accepted.markers[accepted.markerCount++] = marker;
	}

	output = accepted;
	return FullEngineCoopClientTacticalPresentationResult::Success;
}
