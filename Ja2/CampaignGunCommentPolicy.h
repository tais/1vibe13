#ifndef JA2_CAMPAIGN_GUN_COMMENT_POLICY_H
#define JA2_CAMPAIGN_GUN_COMMENT_POLICY_H

#include "GameCapabilities.h"

// JA25's new-gun dialogue is campaign content, not executable identity. The
// policy is value-only so every tactical host can retain the implementation
// while keeping Arulco entirely outside its item and dialogue probes.
class CampaignGunCommentPolicy
{
public:
	explicit constexpr CampaignGunCommentPolicy(GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignGunCommentPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignGunCommentPolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessGunComments() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

private:
	GameCampaign campaign_;
};

static_assert(!CampaignGunCommentPolicy(GameCampaign::Arulco)
	.usesUnfinishedBusinessGunComments());
static_assert(CampaignGunCommentPolicy(GameCampaign::UnfinishedBusiness)
	.usesUnfinishedBusinessGunComments());

#endif
