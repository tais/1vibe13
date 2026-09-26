#ifndef JA2_CAMPAIGN_AIM_HIRE_H
#define JA2_CAMPAIGN_AIM_HIRE_H

#include <cstdint>
#include <Engine/Adapters/JA2/TacticalEntity.h>

struct CampaignAimHireRequest
{
	std::uint32_t profile = 0;
	std::uint32_t contractDays = 0;
	bool copyProfileEquipment = false;
};

struct CampaignAimHirePlan
{
	CampaignAimHireRequest request{};
	std::uint8_t landingX = 0;
	std::uint8_t landingY = 0;
	std::uint32_t arrivalMinute = 0;
	std::uint32_t contractEndMinute = 0;
};

struct CampaignAimHireArrival
{
	std::uint8_t landingX = 0;
	std::uint8_t landingY = 0;
	std::uint32_t arrivalMinute = 0;
};

enum class CampaignAimHireError : std::uint8_t
{
	None,
	UnsupportedCampaignState,
	InvalidProfile,
	InvalidContract,
	UnsupportedEquipment,
	Unavailable,
	DuplicateProfile,
	InvalidTeam,
	CapacityReached,
	InvalidProfileState,
	InvalidLandingZone,
	TimeOutOfRange,
	CreationFailed,
	EventSchedulingFailed,
	PostconditionFailed,
	NativeFailure
};

struct CampaignAimHireResult
{
	CampaignAimHireError error = CampaignAimHireError::NativeFailure;
	bool mutationMayHaveStarted = false;
	TacticalEntityId actor{};
	std::uint32_t arrivalMinute = 0;

	explicit operator bool() const noexcept
	{
		return error == CampaignAimHireError::None && actor.valid();
	}
};

// Common native arrival context, independent of any profile's availability or
// team capacity. Failure preserves arrivalOut.
CampaignAimHireError ReadCampaignAimHireArrival(
	CampaignAimHireArrival& arrivalOut) noexcept;

// Read-only native eligibility and timing for established, worldless Arulco
// AIM hires. Failure preserves planOut. This is neither willingness nor a
// financial quote; the plan is observation, not a reusable authorization.
CampaignAimHireError PrepareCampaignAimHire(
	const CampaignAimHireRequest& request,
	CampaignAimHirePlan& planOut) noexcept;

// Campaign-thread operation; rechecks current eligibility before calling the
// same constructor and accounting-free hire logic as the legacy UI. Optional
// equipment is currently unsupported: copyProfileEquipment=true is rejected
// before mutation until native item transfer has a checked completion boundary.
// Creates an IN_TRANSIT actor and one checked delayed arrival event, without
// debiting money, recording hiring history or marking profile equipment paid.
//
// Any failure after mutationMayHaveStarted requires the authority to fail-stop.
// There is no rollback. A false flag does not authorize retry after any earlier
// mutation by the caller (for example, another step in a paid hire).
CampaignAimHireResult HireAimMercChecked(
	const CampaignAimHireRequest& request) noexcept;

#endif
