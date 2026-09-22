#include "CampaignAimWillingness.h"
#include "Soldier Profile.h"
#include "Soldier Add.h"
#include "Merc Hiring.h"
#include "Strategic Status.h"

bool ReadCampaignAimWillingness(std::uint32_t profileId,
	CampaignAimWillingnessDecision& output) noexcept
{
	if (profileId >= NUM_PROFILES) return false;
	const auto& profile = gMercProfiles[profileId];
	CampaignAimWillingnessInput input;
	input.moraleHangover = profile.ubDaysOfMoraleHangover > 0;
	if (!input.moraleHangover)
	{
		input.learnedHateCount = profile.bLearnToHateCount;
		input.learnedLikeCount = profile.bLearnToLikeCount;
		const auto aliveOnTeam = [](std::uint32_t id) {
			return !IsMercDead(static_cast<UINT8>(id)) &&
				IsMercOnTeam(static_cast<UINT8>(id), FALSE, FALSE);
		};
		for (std::size_t i = 0; i < CampaignAimAllRelations; ++i)
		{
			const bool learned = i == CampaignAimOrdinaryRelations;
			const auto hated = learned ? profile.bLearnToHate : profile.bHated[i];
			const auto buddy = learned ? profile.bLearnToLike : profile.bBuddy[i];
			input.hatedAliveOnTeam[i] = (!learned || input.learnedHateCount <= 0) &&
				ReadCampaignAimRelationPresence(hated, NUM_PROFILES, aliveOnTeam);
			input.buddiesAliveOnTeam[i] = (!learned || input.learnedLikeCount <= 0) &&
				ReadCampaignAimRelationPresence(buddy, NUM_PROFILES, aliveOnTeam);
			if (!learned) input.hatedToleranceHours[i] = profile.bHatedTime[i];
		}
		// Preserve native early returns, including tolerated hatred. Risk
		// readers are only relevant when no relationship decided the result.
		if (DecideCampaignAimWillingness(input).reason == CampaignAimWillingnessReason::Willing &&
			FirstCampaignAimBuddy(input) == CampaignAimNoRelation)
		{
			input.deathRateTooHigh = MercThinksDeathRateTooHigh(static_cast<UINT8>(profileId));
			if (!input.deathRateTooHigh)
				input.reputationTooBad = MercThinksBadReputationTooHigh(static_cast<UINT8>(profileId));
		}
	}
	output = DecideCampaignAimWillingness(input);
	return true;
}
