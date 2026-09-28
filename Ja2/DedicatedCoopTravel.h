#ifndef JA2_DEDICATED_COOP_TRAVEL_H
#define JA2_DEDICATED_COOP_TRAVEL_H

#include <Engine/Adapters/JA2/StrategicGroup.h>
#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <array>
#include <cstdint>

enum class DedicatedCoopTravelPlanCode : std::uint8_t
{
	Planned, HostUnavailable, InvalidDestination, GroupUnavailable, Busy,
	UnsupportedGroup, NativeRejected, NativeContextUnavailable, NoRoute, InvalidRoute
};
struct DedicatedCoopTravelPlanningResult
{
	DedicatedCoopTravelPlanCode code = DedicatedCoopTravelPlanCode::HostUnavailable;
	TacticalEntityId blockedActor{};
	std::int8_t nativeError = 0;
};
struct DedicatedCoopTravelSector
{
	std::uint8_t x = 0, y = 0;
	std::uint32_t minutesFromPrevious = 0;
};
struct DedicatedCoopTravelPlan
{
	StrategicGroupId group{};
	std::uint32_t observedWorldSeconds = 0, totalMinutes = 0;
	std::uint16_t sectorCount = 0, memberCount = 0;
	// Includes the origin. Each subsequent entry is one native adjacent leg.
	std::array<DedicatedCoopTravelSector, 256> sectors{};
	std::array<TacticalEntityId, 256> members{};
};

// Main-thread, read-only preflight of an idle, surface, on-foot group in an
// established worldless campaign. Checks actual members, not GUI selections.
// Native terrain/weight/trait/quest/fatigue rules remain authoritative. Does not
// grant peer authority, reserve a route, create events or initiate movement.
// Every failure leaves output unchanged; callers must revalidate before commit.
DedicatedCoopTravelPlanningResult PlanDedicatedCoopTravel(StrategicGroupId group,
	std::uint8_t destinationX, std::uint8_t destinationY, DedicatedCoopTravelPlan& output) noexcept;

enum class DedicatedCoopTravelStartCode : std::uint8_t
{
	Started, PreflightRejected, AdjacentSectorRequired, NativeInteractionRequired,
	EventConflict, AllocationFailure, EventQueueFailure
};
struct DedicatedCoopTravelStartResult
{
	DedicatedCoopTravelStartCode code = DedicatedCoopTravelStartCode::PreflightRejected;
	DedicatedCoopTravelPlanningResult preflight{};
	std::uint32_t arrivalMinutes = 0;
};
// Native main-thread commit, not a peer authority grant. Replans from current
// state; accepts only a single adjacent leg for now. Crossing-group delays and
// tactical/pre-battle contexts require a separate native interaction boundary.
// All resources/events are prepared before mutation. A rejection starts no
// movement and preserves existing events/identities, actor paths and clocks.
// Success schedules the normal native arrival callback, without advancing time.
DedicatedCoopTravelStartResult StartDedicatedCoopTravel(StrategicGroupId group,
	std::uint8_t destinationX, std::uint8_t destinationY) noexcept;
#endif
