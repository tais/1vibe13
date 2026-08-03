#ifndef JA2_CAMPAIGN_DEALER_POLICY_H
#define JA2_CAMPAIGN_DEALER_POLICY_H

#include "GameCapabilities.h"

// Dealer names are campaign content identities, not persisted inventory
// indices. The legacy save and merchant-XML layouts use raw slots whose
// meanings differ between Arulco and Unfinished Business.
enum class CampaignDealer
{
	Tony,
	Franz,
	Keith,
	Jake,
	Gabby,
	Devin,
	Howard,
	Sam,
	Frank,
	BarBro1,
	BarBro2,
	BarBro3,
	BarBro4,
	Micky,
	Arnie,
	Fredo,
	Perko,
	Raul,
	Elgin,
	Manny,
	Betty,
	Tina,
	None
};

class CampaignDealerPolicy
{
public:
	static constexpr int InvalidDealerId = -1;

	explicit constexpr CampaignDealerPolicy(GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignDealerPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignDealerPolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessRoster() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	constexpr int dealerId(CampaignDealer dealer) const noexcept
	{
		for (int rawDealerId = 0; rawDealerId < OriginalDealerCount;
			++rawDealerId)
		{
			if (dealerAt(rawDealerId) == dealer)
				return rawDealerId;
		}
		return InvalidDealerId;
	}

	constexpr CampaignDealer dealerAt(int rawDealerId) const noexcept
	{
		if (rawDealerId < 0 || rawDealerId >= OriginalDealerCount)
			return CampaignDealer::None;
		return usesUnfinishedBusinessRoster()
			? UnfinishedBusinessRoster[rawDealerId]
			: ArulcoRoster[rawDealerId];
	}

	constexpr bool isDealer(
		int rawDealerId, CampaignDealer dealer) const noexcept
	{
		return dealerAt(rawDealerId) == dealer;
	}

private:
	static constexpr int OriginalDealerCount = 20;

	inline static constexpr CampaignDealer ArulcoRoster[OriginalDealerCount] = {
		CampaignDealer::Tony, CampaignDealer::Franz,
		CampaignDealer::Keith, CampaignDealer::Jake,
		CampaignDealer::Gabby, CampaignDealer::Devin,
		CampaignDealer::Howard, CampaignDealer::Sam,
		CampaignDealer::Frank, CampaignDealer::BarBro1,
		CampaignDealer::BarBro2, CampaignDealer::BarBro3,
		CampaignDealer::BarBro4, CampaignDealer::Micky,
		CampaignDealer::Arnie, CampaignDealer::Fredo,
		CampaignDealer::Perko, CampaignDealer::Elgin,
		CampaignDealer::Manny, CampaignDealer::Tina};

	inline static constexpr CampaignDealer
		UnfinishedBusinessRoster[OriginalDealerCount] = {
			CampaignDealer::Tony, CampaignDealer::Franz,
			CampaignDealer::Keith, CampaignDealer::Jake,
			CampaignDealer::Gabby, CampaignDealer::Howard,
			CampaignDealer::Sam, CampaignDealer::Frank,
			CampaignDealer::BarBro1, CampaignDealer::BarBro2,
			CampaignDealer::BarBro3, CampaignDealer::BarBro4,
			CampaignDealer::Micky, CampaignDealer::Arnie,
			CampaignDealer::Fredo, CampaignDealer::Raul,
			CampaignDealer::Elgin, CampaignDealer::Manny,
			CampaignDealer::Betty, CampaignDealer::Tina};

	GameCampaign campaign_;
};

static_assert(CampaignDealerPolicy(GameCampaign::Arulco)
	.dealerId(CampaignDealer::Devin) == 5);
static_assert(CampaignDealerPolicy(GameCampaign::UnfinishedBusiness)
	.dealerId(CampaignDealer::Howard) == 5);
static_assert(CampaignDealerPolicy(GameCampaign::Arulco)
	.dealerId(CampaignDealer::Perko) == 16);
static_assert(CampaignDealerPolicy(GameCampaign::UnfinishedBusiness)
	.dealerId(CampaignDealer::Raul) == 15);
static_assert(CampaignDealerPolicy(GameCampaign::Arulco)
	.dealerId(CampaignDealer::Betty) == CampaignDealerPolicy::InvalidDealerId);
static_assert(CampaignDealerPolicy(GameCampaign::UnfinishedBusiness)
	.dealerId(CampaignDealer::Devin) == CampaignDealerPolicy::InvalidDealerId);

#endif
