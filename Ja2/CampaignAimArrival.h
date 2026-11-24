#ifndef JA2_CAMPAIGN_AIM_ARRIVAL_H
#define JA2_CAMPAIGN_AIM_ARRIVAL_H

#include <cstdint>
#include <Engine/Adapters/JA2/CampaignEventQueue.h>
#include <Engine/Adapters/JA2/StrategicGroup.h>
#include <Engine/Adapters/JA2/TacticalEntity.h>

struct CampaignAimArrivalRequest
{
	CampaignEventId event{};
	TacticalEntityId actor{};
};

enum class CampaignAimArrivalError : std::uint8_t
{
	None,
	UnsupportedCampaignState,
	InvalidActor,
	InvalidProfile,
	UnsupportedProfile,
	InvalidEvent,
	InvalidPendingState,
	InvalidLandingZone,
	NoSafeLandingZone,
	TimeOutOfRange,
	SquadAssignmentFailed,
	EventSchedulingFailed,
	PostconditionFailed,
	NativeFailure
};

struct CampaignAimArrivalResult
{
	CampaignAimArrivalError error = CampaignAimArrivalError::NativeFailure;
	bool mutationMayHaveStarted = false;
	TacticalEntityId actor{};
	StrategicGroupId group{};
	std::uint8_t landingX = 0, landingY = 0;
	std::uint32_t contractEndMinute = 0;

	explicit operator bool() const noexcept
	{
		return error == CampaignAimArrivalError::None && actor.valid() && group.valid();
	}
};

// Executes the shared native arrival callback for one exact currently due
// delayed-hire event and its exact live ordinary AIM actor, in an established
// worldless Arulco campaign. John Kulba's missed-flight lifecycle is excluded.
// The event remains owned by the native dispatcher, which may retire it only
// after success. Repeated application cannot arrive or assign the actor twice.
//
// Suppresses the modal reroute notice and immediate ScreenMsg arrival notice.
// Native contract, squad/group, dialogue/opinion and Lua effects are retained;
// the normal campaign pump must drain their state before a checkpoint.
//
// Any failure is terminal for the dispatching authority: the dispatcher may
// already have changed the clock or begun its event even when this function's
// mutationMayHaveStarted is false. No rollback or retry is provided.
CampaignAimArrivalResult ArriveAimMercChecked(
	const CampaignAimArrivalRequest& request) noexcept;

#endif
