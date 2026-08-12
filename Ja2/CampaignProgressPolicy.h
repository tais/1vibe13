#ifndef JA2_CAMPAIGN_PROGRESS_POLICY_H
#define JA2_CAMPAIGN_PROGRESS_POLICY_H

#include "GameCapabilities.h"

#include <cstdint>

namespace CampaignProgressPolicyDetail
{
constexpr int LegacySurfaceSector(int row, int column) noexcept
{
	return (row - 1) * 16 + column - 1;
}
}

// Campaign progress calculation and progress-triggered story notifications are
// content policy. The policy is value-only so every application host can keep
// both established paths available and select from immutable capabilities.
class CampaignProgressPolicy
{
public:
	explicit constexpr CampaignProgressPolicy(GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignProgressPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignProgressPolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessProgress() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	// Preserve the effective legacy INT8 switch exactly. The JA25 provider
	// returns a signed table key; the later surface-sector constants therefore
	// remain outside that key's domain and retain the established 50% fallback.
	constexpr std::uint8_t unfinishedBusinessProgress(
		std::int8_t furthestSectorPlayerOwns) const noexcept
	{
		switch (furthestSectorPlayerOwns)
		{
			case CampaignProgressPolicyDetail::LegacySurfaceSector(8, 7):
				return 44;
			case CampaignProgressPolicyDetail::LegacySurfaceSector(8, 8):
				return 45;
			case CampaignProgressPolicyDetail::LegacySurfaceSector(8, 9):
				return 55;
			case CampaignProgressPolicyDetail::LegacySurfaceSector(8, 10):
				return 58;
			default:
				return 50;
		}
	}

	constexpr bool shouldStartScientistAwolMeanwhile(
		std::uint8_t currentProgress,
		std::uint8_t previousHighestProgress,
		std::uint32_t threshold) const noexcept
	{
		return !usesUnfinishedBusinessProgress() &&
			currentProgress > previousHighestProgress &&
			currentProgress >= threshold &&
			previousHighestProgress < threshold;
	}

private:
	GameCampaign campaign_;
};

static_assert(!CampaignProgressPolicy(GameCampaign::Arulco)
	.usesUnfinishedBusinessProgress());
static_assert(CampaignProgressPolicy(GameCampaign::UnfinishedBusiness)
	.usesUnfinishedBusinessProgress());
static_assert(CampaignProgressPolicy(GameCampaign::UnfinishedBusiness)
	.unfinishedBusinessProgress(118) == 44);
static_assert(CampaignProgressPolicy(GameCampaign::UnfinishedBusiness)
	.unfinishedBusinessProgress(0) == 50);
static_assert(CampaignProgressPolicy(GameCampaign::Arulco)
	.shouldStartScientistAwolMeanwhile(35, 34, 35));
static_assert(!CampaignProgressPolicy(GameCampaign::UnfinishedBusiness)
	.shouldStartScientistAwolMeanwhile(35, 34, 35));

#endif
