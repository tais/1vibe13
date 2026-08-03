#include "CampaignAimSitePolicy.h"

#include <cstdint>

int main()
{
	const CampaignAimSitePolicy arulco(GameCampaign::Arulco);
	const CampaignAimSitePolicy unfinishedBusiness(
		GameCampaign::UnfinishedBusiness);

	if (arulco.usesUnfinishedBusinessSite() ||
		!arulco.linkEnabled(false) ||
		!arulco.linkEnabled(true) ||
		arulco.usesMissionFee() ||
		!arulco.showsSalaryBreakdown() ||
		arulco.showsOneTimeFeeOffer() ||
		!arulco.showsSelectionLights() ||
		arulco.forcesEquipmentPurchase() ||
		arulco.hidesContractAndEquipmentButtons() ||
		!arulco.appendsMedicalDepositLabel())
		return 1;

	if (!unfinishedBusiness.usesUnfinishedBusinessSite() ||
		unfinishedBusiness.linkEnabled(false) ||
		!unfinishedBusiness.linkEnabled(true) ||
		!unfinishedBusiness.usesMissionFee() ||
		unfinishedBusiness.showsSalaryBreakdown() ||
		!unfinishedBusiness.showsOneTimeFeeOffer() ||
		unfinishedBusiness.showsSelectionLights() ||
		!unfinishedBusiness.forcesEquipmentPurchase() ||
		!unfinishedBusiness.hidesContractAndEquipmentButtons() ||
		unfinishedBusiness.appendsMedicalDepositLabel())
		return 2;

	constexpr auto oneDay = static_cast<std::uint8_t>(
		CampaignAimSitePolicy::ContractLength::OneDay);
	constexpr auto oneWeek = static_cast<std::uint8_t>(
		CampaignAimSitePolicy::ContractLength::OneWeek);
	constexpr auto twoWeeks = static_cast<std::uint8_t>(
		CampaignAimSitePolicy::ContractLength::TwoWeeks);

	if (arulco.contractCharge(100, 700, 1400, 250, 300,
			oneDay, false, false) != 100 ||
		arulco.contractCharge(100, 700, 1400, 250, 300,
			oneWeek, true, false) != 950 ||
		arulco.contractCharge(100, 700, 1400, 250, 300,
			twoWeeks, false, true) != 1700 ||
		arulco.contractCharge(100, 700, 1400, 250, 300,
			255, true, true) != 550)
		return 3;

	if (unfinishedBusiness.contractCharge(100, 700, 1400, 250, 300,
			oneDay, true, true) != 700 ||
		unfinishedBusiness.contractCharge(100, 700, 1400, 250, 300,
			twoWeeks, false, false) != 700)
		return 4;

	return 0;
}
