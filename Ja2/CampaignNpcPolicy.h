#ifndef JA2_CAMPAIGN_NPC_POLICY_H
#define JA2_CAMPAIGN_NPC_POLICY_H

#include "GameCapabilities.h"

#include <cstdint>

// NPC script loading and tactical AI campaign choices belong to the live
// campaign. Callers keep legacy state probes to the right of these gates so an
// inactive campaign never evaluates the other campaign's globals.
class CampaignNpcPolicy
{
public:
	explicit constexpr CampaignNpcPolicy(GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignNpcPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignNpcPolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessNpcQuoteFiles() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	// Arulco's PETER/ALBERTO/CARLO fallback was the head of an else-chain.
	// UB compiled a second independent if and always continued through general
	// profile routing, even after that fallback ran.
	constexpr bool shouldApplyGeneralNpcQuoteRouting(
		bool arulcoHerveFallbackSelected) const noexcept
	{
		return usesUnfinishedBusinessNpcQuoteFiles() ||
			!arulcoHerveFallbackSelected;
	}

	constexpr bool usesMeanwhileNpcQuoteOverrides() const noexcept
	{
		return !usesUnfinishedBusinessNpcQuoteFiles();
	}

	constexpr bool shouldRefreshAuntieNpcScriptRecord(
		std::uint32_t saveGameVersion) const noexcept
	{
		return usesMeanwhileNpcQuoteOverrides() && saveGameVersion < 92U;
	}

	constexpr bool runsMorrisHurtPlayerTurnHook() const noexcept
	{
		return usesUnfinishedBusinessNpcQuoteFiles();
	}

	constexpr bool allowsEnemySurrenderOffers() const noexcept
	{
		return !usesUnfinishedBusinessNpcQuoteFiles();
	}

private:
	GameCampaign campaign_;
};

static_assert(CampaignNpcPolicy(GameCampaign::Arulco)
	.usesMeanwhileNpcQuoteOverrides());
static_assert(!CampaignNpcPolicy(GameCampaign::Arulco)
	.shouldApplyGeneralNpcQuoteRouting(true));
static_assert(CampaignNpcPolicy(GameCampaign::UnfinishedBusiness)
	.shouldApplyGeneralNpcQuoteRouting(true));
static_assert(CampaignNpcPolicy(GameCampaign::Arulco)
	.shouldRefreshAuntieNpcScriptRecord(91U));
static_assert(!CampaignNpcPolicy(GameCampaign::Arulco)
	.shouldRefreshAuntieNpcScriptRecord(92U));
static_assert(CampaignNpcPolicy(GameCampaign::UnfinishedBusiness)
	.runsMorrisHurtPlayerTurnHook());
static_assert(!CampaignNpcPolicy(GameCampaign::UnfinishedBusiness)
	.allowsEnemySurrenderOffers());

#endif
