#ifndef JA2_DEDICATED_COOP_MISSION_BOOTSTRAP_H
#define JA2_DEDICATED_COOP_MISSION_BOOTSTRAP_H

#include "DedicatedCoopMissionPolicy.h"

#include <array>
#include <cstddef>
#include <cstdint>

enum class DedicatedCoopMissionBootstrapError : std::uint8_t
{
	None,
	InvalidInitialCampaign,
	TacticalWorldAlreadyLoaded,
	InvalidArrivalSector,
	NoHostileEncounter,
	TeamCapacityTooSmall,
	InsufficientStarterCandidates,
	InsufficientFunds,
	StarterChargeInvalid,
	HireFailed,
	MapScreenNotReady,
	NoEligibleEstablishedSector,
	TacticalEntryFailed,
	TacticalPostconditionFailed
};

struct DedicatedCoopMissionPreparationResult
{
	DedicatedCoopMissionBootstrapError error =
		DedicatedCoopMissionBootstrapError::InvalidInitialCampaign;
	std::array<std::uint8_t, DedicatedCoopStarterRosterSize> profiles{};
	std::uint64_t totalCharge = 0;

	explicit operator bool() const noexcept
	{
		return error == DedicatedCoopMissionBootstrapError::None;
	}
};

// Observational classification used when opening a cold checkpoint. Prepared
// means the complete seven-day A.I.M. roster is still in transit and every
// actor has exactly one matching normal delayed-hiring event; established
// means a non-initial, world-free campaign still has a live player merc. No
// state is changed while classifying.
DedicatedCoopStarterCampaignState
InspectDedicatedCoopStarterCampaign() noexcept;

const char* DedicatedCoopStarterCampaignStateName(
	DedicatedCoopStarterCampaignState state) noexcept;

// Installs a complete starter roster only into the exact untouched initial
// strategic state. All validation and roster selection precede the first hire.
// A legacy failure after that point is terminal to the caller and is never
// advertised as rollback-safe.
DedicatedCoopMissionPreparationResult
PrepareDedicatedCoopStarterMission() noexcept;

// True only after the normal map screen has completed its initialization and
// the cold campaign still has no tactical world.
bool IsDedicatedCoopStarterMissionMapReady() noexcept;

// Uses the ordinary first-arrival strategic/tactical path. The hired actors
// arrive through the existing strategic-event and helicopter machinery.
DedicatedCoopMissionBootstrapError
LaunchDedicatedCoopStarterMission() noexcept;

// Selects only from hostile save-owned sectors containing an eligible on-foot
// player actor, in deterministic coordinate order. Peaceful campaigns remain
// cold; no client-selected map coordinate participates in this command.
DedicatedCoopMissionBootstrapError
LaunchDedicatedCoopEstablishedMission() noexcept;

// Counts the same live player actors used by dedicated co-op assignment.
std::size_t CountDedicatedCoopControllableActors() noexcept;

const char* DedicatedCoopMissionBootstrapErrorName(
	DedicatedCoopMissionBootstrapError error) noexcept;

#endif
