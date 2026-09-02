#ifndef JA2_FULL_ENGINE_COOP_CLIENT_TACTICAL_PRESENTATION_H
#define JA2_FULL_ENGINE_COOP_CLIENT_TACTICAL_PRESENTATION_H

#include <Multiplayer/CoopTacticalProtocol.h>

#include <array>
#include <cstddef>
#include <cstdint>

struct FullEngineCoopClientTacticalPlotBounds
{
	std::int32_t x = 0;
	std::int32_t y = 0;
	std::int32_t width = 0;
	std::int32_t height = 0;
};

struct FullEngineCoopClientTacticalPlotMarker
{
	TacticalEntityId actor;
	std::int32_t screenX = 0;
	std::int32_t screenY = 0;
	std::uint16_t column = 0;
	std::uint16_t row = 0;
	std::uint16_t profile = 0;
	std::int16_t actionPoints = 0;
	std::int16_t life = 0;
	std::int16_t maximumLife = 0;
	std::int8_t level = 0;
	std::uint8_t direction = 0;
	TacticalStance stance = TacticalStance::Unknown;
	bool assigned = false;
	bool selected = false;
	bool interruptActionEligible = false;
};

// Allocation-free render model for a passive, worldless co-op client. The
// diamond is an exact projection of authority-published logical grid positions;
// it intentionally claims no terrain, visibility, pathing, or line-of-sight.
// Markers are restricted to the assigned actors' team because tactical v3 does
// not yet carry a peer-specific visibility projection for hostile actors. This
// is a presentation posture, not wire confidentiality: the replica still holds
// the authority's complete current baseline.
struct FullEngineCoopClientTacticalPresentation
{
	FullEngineCoopClientTacticalPlotBounds bounds;
	TacticalWorldDimensions dimensions;
	std::array<FullEngineCoopClientTacticalPlotMarker,
		CoopSession::MaximumCoopTacticalSnapshotActors> markers{};
	std::size_t markerCount = 0;
	std::uint8_t friendlyTeam = 0;
	bool hasFriendlyTeam = false;
	TacticalInterruptPhase interruptPhase = TacticalInterruptPhase::None;
	std::uint64_t interruptSerial = 0;
};

enum class FullEngineCoopClientTacticalPresentationResult : std::uint8_t
{
	Success,
	InvalidInput,
	InvalidSnapshot,
	InvalidAssignments,
	CapacityReached
};

// Transactional: output changes only on Success. assignedActors must be in
// strict identity order and must resolve to spatially present actors on one
// team. Transient unplaced nonassigned actors are safely omitted.
FullEngineCoopClientTacticalPresentationResult
BuildFullEngineCoopClientTacticalPresentation(
	const TacticalWorldSnapshot& snapshot,
	const TacticalEntityId* assignedActors,
	std::size_t assignedActorCount,
	TacticalEntityId selectedActor,
	FullEngineCoopClientTacticalPlotBounds bounds,
	FullEngineCoopClientTacticalPresentation& output) noexcept;

#endif
