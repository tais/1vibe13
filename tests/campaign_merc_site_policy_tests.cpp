#include "CampaignMercSitePolicy.h"

#include <array>
#include <cstdint>

int main()
{
	const CampaignMercSitePolicy arulco(GameCampaign::Arulco);
	const CampaignMercSitePolicy unfinishedBusiness(
		GameCampaign::UnfinishedBusiness);

	if (arulco.usesUnfinishedBusinessSite() ||
		arulco.createsAccountAtGameStart() ||
		!arulco.hasAccountManagement() ||
		!arulco.usesDeferredBilling() ||
		arulco.showsSpecialOffer() ||
		arulco.firstEquipmentKitIsFree(0) ||
		arulco.chargesEquipmentUpFront() ||
		!arulco.marksPurchasedEquipmentUnpaid() ||
		!arulco.requiresAvailableSpeckForDialogue() ||
		!arulco.supportsServerOutage() ||
		!arulco.supportsArulcoRecruitableMercs() ||
		arulco.usesImportantUnfinishedBusinessQuotes() ||
		arulco.contractRate(125, 900) != 125 ||
		arulco.initialHireCharge(900, 300, 1) != 0 ||
		arulco.firstVisitIntroFirst() != 0 ||
		arulco.firstVisitIntroLast() != 8 ||
		arulco.randomQuoteCount() != 19)
		return 1;

	if (!unfinishedBusiness.usesUnfinishedBusinessSite() ||
		!unfinishedBusiness.createsAccountAtGameStart() ||
		unfinishedBusiness.hasAccountManagement() ||
		unfinishedBusiness.usesDeferredBilling() ||
		!unfinishedBusiness.showsSpecialOffer() ||
		!unfinishedBusiness.firstEquipmentKitIsFree(0) ||
		unfinishedBusiness.firstEquipmentKitIsFree(1) ||
		!unfinishedBusiness.chargesEquipmentUpFront() ||
		unfinishedBusiness.marksPurchasedEquipmentUnpaid() ||
		unfinishedBusiness.requiresAvailableSpeckForDialogue() ||
		unfinishedBusiness.supportsServerOutage() ||
		unfinishedBusiness.supportsArulcoRecruitableMercs() ||
		!unfinishedBusiness.usesImportantUnfinishedBusinessQuotes() ||
		unfinishedBusiness.contractRate(125, 900) != 900 ||
		unfinishedBusiness.initialHireCharge(900, 300, 0) != 900 ||
		unfinishedBusiness.initialHireCharge(900, 300, 1) != 1200 ||
		unfinishedBusiness.firstVisitIntroFirst() != 76 ||
		unfinishedBusiness.firstVisitIntroLast() != 82 ||
		unfinishedBusiness.randomQuoteCount() != 20)
		return 2;

	constexpr std::array<CampaignSpeckQuoteCode::Role, 8> roles = {
		CampaignSpeckQuoteCode::Role::AdvertiseGaston,
		CampaignSpeckQuoteCode::Role::AdvertiseStogie,
		CampaignSpeckQuoteCode::Role::GastonDead,
		CampaignSpeckQuoteCode::Role::StogieDead,
		CampaignSpeckQuoteCode::Role::PlayerHiresGaston,
		CampaignSpeckQuoteCode::Role::PlayerHiresStogie,
		CampaignSpeckQuoteCode::Role::RandomChitChat1,
		CampaignSpeckQuoteCode::Role::RandomChitChat2};
	constexpr std::array<std::uint16_t, 8> arulcoQuotes =
		{76, 77, 78, 79, 80, 81, 82, 83};
	constexpr std::array<std::uint16_t, 8> unfinishedBusinessQuotes =
		{94, 95, 96, 97, 98, 99, 100, 101};
	for (std::size_t index = 0; index < roles.size(); ++index)
	{
		if (arulco.quote(roles[index]) != arulcoQuotes[index] ||
			unfinishedBusiness.quote(roles[index]) !=
				unfinishedBusinessQuotes[index])
			return 3;
	}

	constexpr std::array<std::uint16_t, 19> expectedArulcoRandomQuotes = {
		82, 83, 76, 77, 47, 48, 49, 50, 51, 52,
		53, 54, 55, 56, 58, 59, 60, 61, 84};
	for (std::size_t index = 0;
		index < expectedArulcoRandomQuotes.size(); ++index)
	{
		if (arulco.randomQuote(index) != expectedArulcoRandomQuotes[index])
			return 4;
	}

	constexpr std::array<std::uint16_t, 20>
		expectedUnfinishedBusinessRandomQuotes = {
			103, 100, 101, 102, 94, 95, 47, 48, 49, 50,
			51, 52, 53, 54, 55, 56, 58, 59, 60, 61};
	for (std::size_t index = 0;
		index < expectedUnfinishedBusinessRandomQuotes.size(); ++index)
	{
		if (unfinishedBusiness.randomQuote(index) !=
			expectedUnfinishedBusinessRandomQuotes[index])
			return 5;
	}

	return 0;
}
