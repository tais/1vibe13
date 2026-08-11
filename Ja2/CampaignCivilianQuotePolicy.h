#ifndef JA2_CAMPAIGN_CIVILIAN_QUOTE_POLICY_H
#define JA2_CAMPAIGN_CIVILIAN_QUOTE_POLICY_H

#include "GameCapabilities.h"

#include <cstdint>

// Civilian dialogue is campaign content selected from the live campaign, not
// from the executable that happened to host it. The legacy quote identifiers
// remain inputs/outputs of the tactical adapter; this policy owns only the
// campaign truth table.
class CampaignCivilianQuotePolicy
{
public:
	explicit constexpr CampaignCivilianQuotePolicy(
		GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignCivilianQuotePolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignCivilianQuotePolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessQuoteCatalogue() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	constexpr bool completesSurrenderOfferAfterQuote() const noexcept
	{
		return !usesUnfinishedBusinessQuoteCatalogue();
	}

	constexpr std::uint8_t civilianGroupQuoteBoundary(
		std::uint8_t arulcoBoundary,
		std::uint8_t unfinishedBusinessBoundary) const noexcept
	{
		return usesUnfinishedBusinessQuoteCatalogue()
			? unfinishedBusinessBoundary
			: arulcoBoundary;
	}

	constexpr bool discardsUnavailableQuote(
		std::uint16_t quoteId) const noexcept
	{
		return usesUnfinishedBusinessQuoteCatalogue() && quoteId == 255U;
	}

private:
	GameCampaign campaign_;
};

static_assert(CampaignCivilianQuotePolicy(GameCampaign::Arulco)
	.completesSurrenderOfferAfterQuote());
static_assert(!CampaignCivilianQuotePolicy(
	GameCampaign::UnfinishedBusiness).completesSurrenderOfferAfterQuote());
static_assert(CampaignCivilianQuotePolicy(
	GameCampaign::UnfinishedBusiness).discardsUnavailableQuote(255U));

#endif
