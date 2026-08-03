#ifndef JA2_CAMPAIGN_APPLICATION_POLICY_H
#define JA2_CAMPAIGN_APPLICATION_POLICY_H

#include "GameCapabilities.h"

// Application-shell decisions that historically depended on which campaign
// executable compiled a call site. Keeping them in this value-only policy
// makes both built-in campaigns available to common startup and tactical code.
class CampaignApplicationPolicy
{
public:
	explicit constexpr CampaignApplicationPolicy(
		GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignApplicationPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignApplicationPolicy(capabilities.campaign)
	{
	}

	constexpr GameCampaign campaign() const noexcept { return campaign_; }

	constexpr bool usesUnfinishedBusinessContent() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	constexpr bool usesLocalizedArulcoMercData() const noexcept
	{
		return !usesUnfinishedBusinessContent();
	}

	constexpr bool usesArulcoMerchantRoster() const noexcept
	{
		return !usesUnfinishedBusinessContent();
	}

	constexpr bool hasMeanwhileScenes() const noexcept
	{
		return !usesUnfinishedBusinessContent();
	}

	constexpr bool runsUnfinishedBusinessTacticalHooks() const noexcept
	{
		return usesUnfinishedBusinessContent();
	}

	constexpr bool usesUnfinishedBusinessUndergroundLoadScreens() const noexcept
	{
		return usesUnfinishedBusinessContent();
	}

	constexpr bool shouldLoadExternalEnemyDeployment(
		bool unfinishedBusinessEnemyXmlEnabled) const noexcept
	{
		return !usesUnfinishedBusinessContent() ||
			unfinishedBusinessEnemyXmlEnabled;
	}

private:
	GameCampaign campaign_;
};

static_assert(CampaignApplicationPolicy(GameCampaign::Arulco)
	.hasMeanwhileScenes());
static_assert(!CampaignApplicationPolicy(GameCampaign::UnfinishedBusiness)
	.hasMeanwhileScenes());
static_assert(CampaignApplicationPolicy(GameCampaign::Arulco)
	.shouldLoadExternalEnemyDeployment(false));
static_assert(!CampaignApplicationPolicy(GameCampaign::UnfinishedBusiness)
	.shouldLoadExternalEnemyDeployment(false));
static_assert(CampaignApplicationPolicy(GameCampaign::UnfinishedBusiness)
	.shouldLoadExternalEnemyDeployment(true));

#endif
