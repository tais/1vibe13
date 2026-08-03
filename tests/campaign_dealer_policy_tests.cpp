#include "CampaignDealerPolicy.h"

#include <array>

namespace
{
struct DealerSlot
{
	CampaignDealer dealer;
	int slot;
};

template <std::size_t Size>
bool CheckRoster(
	const CampaignDealerPolicy& policy,
	const std::array<DealerSlot, Size>& roster)
{
	for (const DealerSlot& entry : roster)
	{
		if (policy.dealerId(entry.dealer) != entry.slot ||
			policy.dealerAt(entry.slot) != entry.dealer ||
			!policy.isDealer(entry.slot, entry.dealer))
			return false;
	}
	return true;
}
}

int main()
{
	constexpr std::array<DealerSlot, 20> arulcoRoster{{
		{CampaignDealer::Tony, 0}, {CampaignDealer::Franz, 1},
		{CampaignDealer::Keith, 2}, {CampaignDealer::Jake, 3},
		{CampaignDealer::Gabby, 4}, {CampaignDealer::Devin, 5},
		{CampaignDealer::Howard, 6}, {CampaignDealer::Sam, 7},
		{CampaignDealer::Frank, 8}, {CampaignDealer::BarBro1, 9},
		{CampaignDealer::BarBro2, 10}, {CampaignDealer::BarBro3, 11},
		{CampaignDealer::BarBro4, 12}, {CampaignDealer::Micky, 13},
		{CampaignDealer::Arnie, 14}, {CampaignDealer::Fredo, 15},
		{CampaignDealer::Perko, 16}, {CampaignDealer::Elgin, 17},
		{CampaignDealer::Manny, 18}, {CampaignDealer::Tina, 19}}};
	constexpr std::array<DealerSlot, 20> unfinishedBusinessRoster{{
		{CampaignDealer::Tony, 0}, {CampaignDealer::Franz, 1},
		{CampaignDealer::Keith, 2}, {CampaignDealer::Jake, 3},
		{CampaignDealer::Gabby, 4}, {CampaignDealer::Howard, 5},
		{CampaignDealer::Sam, 6}, {CampaignDealer::Frank, 7},
		{CampaignDealer::BarBro1, 8}, {CampaignDealer::BarBro2, 9},
		{CampaignDealer::BarBro3, 10}, {CampaignDealer::BarBro4, 11},
		{CampaignDealer::Micky, 12}, {CampaignDealer::Arnie, 13},
		{CampaignDealer::Fredo, 14}, {CampaignDealer::Raul, 15},
		{CampaignDealer::Elgin, 16}, {CampaignDealer::Manny, 17},
		{CampaignDealer::Betty, 18}, {CampaignDealer::Tina, 19}}};

	const CampaignDealerPolicy arulco(GameCampaign::Arulco);
	GameCapabilities unfinishedBusinessCapabilities;
	unfinishedBusinessCapabilities.campaign =
		GameCampaign::UnfinishedBusiness;
	const CampaignDealerPolicy unfinishedBusiness(
		unfinishedBusinessCapabilities);

	if (arulco.usesUnfinishedBusinessRoster() ||
		!unfinishedBusiness.usesUnfinishedBusinessRoster() ||
		!CheckRoster(arulco, arulcoRoster) ||
		!CheckRoster(unfinishedBusiness, unfinishedBusinessRoster))
		return 1;

	if (arulco.dealerId(CampaignDealer::Raul) !=
			CampaignDealerPolicy::InvalidDealerId ||
		arulco.dealerId(CampaignDealer::Betty) !=
			CampaignDealerPolicy::InvalidDealerId ||
		unfinishedBusiness.dealerId(CampaignDealer::Devin) !=
			CampaignDealerPolicy::InvalidDealerId ||
		unfinishedBusiness.dealerId(CampaignDealer::Perko) !=
			CampaignDealerPolicy::InvalidDealerId)
		return 2;

	if (arulco.dealerAt(-1) != CampaignDealer::None ||
		arulco.dealerAt(20) != CampaignDealer::None ||
		unfinishedBusiness.dealerAt(79) != CampaignDealer::None)
		return 3;

	return 0;
}
