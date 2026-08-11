#ifndef JA2_CAMPAIGN_MAP_SCREEN_POLICY_H
#define JA2_CAMPAIGN_MAP_SCREEN_POLICY_H

#include "GameCapabilities.h"

// Campaign-specific strategic-map behavior is content policy, not executable
// identity. Every host keeps both paths available and selects one from the
// live campaign capability.
class CampaignMapScreenPolicy
{
public:
	explicit constexpr CampaignMapScreenPolicy(GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignMapScreenPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignMapScreenPolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessMapRules() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	constexpr bool usesJerryMiloGuidance() const noexcept
	{
		return usesUnfinishedBusinessMapRules();
	}

	constexpr bool shouldRebuildCustomMapList(
		bool rebuildRequested) const noexcept
	{
		return usesUnfinishedBusinessMapRules() && rebuildRequested;
	}

	constexpr bool shouldPumpJerryMiloQuotes(
		bool jerryQuotesEnabled) const noexcept
	{
		return usesUnfinishedBusinessMapRules() && jerryQuotesEnabled;
	}

	constexpr bool hasMeanwhileScenes() const noexcept
	{
		return !usesUnfinishedBusinessMapRules();
	}

	constexpr bool treatsSanMonaAsUnimportant() const noexcept
	{
		return !usesUnfinishedBusinessMapRules();
	}

	constexpr bool usesUnfinishedBusinessLossDialogue() const noexcept
	{
		return usesUnfinishedBusinessMapRules();
	}

	constexpr bool usesConfigurableMapBorderButtons() const noexcept
	{
		return usesUnfinishedBusinessMapRules();
	}

	constexpr bool shouldDisableAutoResolve(
		bool autoResolveConfigured) const noexcept
	{
		return usesUnfinishedBusinessMapRules() && !autoResolveConfigured;
	}

	constexpr bool runsUnfinishedBusinessStrategicAi() const noexcept
	{
		return usesUnfinishedBusinessMapRules();
	}

	constexpr bool shouldCheckHelicopterCampaignLoss(
		bool inGameHelicopterCrash) const noexcept
	{
		return usesUnfinishedBusinessMapRules() && inGameHelicopterCrash;
	}

private:
	GameCampaign campaign_;
};

static_assert(!CampaignMapScreenPolicy(GameCampaign::Arulco)
	.usesJerryMiloGuidance());
static_assert(CampaignMapScreenPolicy(GameCampaign::UnfinishedBusiness)
	.usesJerryMiloGuidance());
static_assert(!CampaignMapScreenPolicy(GameCampaign::Arulco)
	.shouldRebuildCustomMapList(true));
static_assert(CampaignMapScreenPolicy(GameCampaign::UnfinishedBusiness)
	.shouldRebuildCustomMapList(true));
static_assert(CampaignMapScreenPolicy(GameCampaign::Arulco)
	.hasMeanwhileScenes());
static_assert(!CampaignMapScreenPolicy(GameCampaign::UnfinishedBusiness)
	.hasMeanwhileScenes());
static_assert(CampaignMapScreenPolicy(GameCampaign::Arulco)
	.treatsSanMonaAsUnimportant());
static_assert(CampaignMapScreenPolicy(GameCampaign::UnfinishedBusiness)
	.shouldDisableAutoResolve(false));

#endif
