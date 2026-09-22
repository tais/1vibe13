#include "CampaignAimSitePolicy.h"

#include <cstdint>
#include <cstdio>
#include <limits>

namespace
{
using Policy = CampaignAimSitePolicy;
using Quote = Policy::ContractQuote;
using Error = Policy::ContractQuoteError;
int failures = 0;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::fprintf(stderr, "FAIL: %s\n", message);
	++failures;
}

constexpr bool SameQuote(const Quote& left, const Quote& right)
{
	return left.salary == right.salary &&
		left.medicalDeposit == right.medicalDeposit &&
		left.equipmentCost == right.equipmentCost && left.total == right.total;
}

void TestCheckedContractChoices()
{
	const Policy arulco(GameCampaign::Arulco);
	const std::uint32_t salaries[] = {100, 630, 1120};
	for (std::uint8_t duration = 0; duration < 3; ++duration)
		for (unsigned choices = 0; choices < 4; ++choices)
		{
			const bool deposit = (choices & 1) != 0;
			const bool equipment = (choices & 2) != 0;
			Quote quote{11, 22, 33, 66};
			Check(arulco.quoteContract(100, 630, 1120, 250, 300, duration,
				deposit, equipment, quote) == Error::None,
				"all ordinary contract lengths and component choices are supported");
			Check(SameQuote(quote, {salaries[duration], deposit ? 250u : 0u,
				equipment ? 300u : 0u,
				salaries[duration] + (deposit ? 250u : 0u) + (equipment ? 300u : 0u)}),
				"quote separates salary, charged deposit, purchased equipment and total");
		}
}

void TestInvalidDurationPreservesOutput()
{
	const Quote retained{11, 22, 33, 66};
	const GameCampaign campaigns[] = {GameCampaign::Arulco, GameCampaign::UnfinishedBusiness};
	for (const auto campaign : campaigns)
		for (unsigned duration = 3; duration <= UINT8_MAX; ++duration)
		{
			Quote quote = retained;
			Check(Policy(campaign).quoteContract(100, 700, 1400, 250, 300,
				static_cast<std::uint8_t>(duration), true, true, quote) ==
				Error::InvalidContractLength && SameQuote(quote, retained),
				"every invalid duration is rejected without publishing a partial quote, including UB");
		}
}

void TestNativeFinanceBoundaries()
{
	const Policy arulco(GameCampaign::Arulco);
	constexpr auto limit = static_cast<std::uint32_t>(
		std::numeric_limits<std::int32_t>::max());
	constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
	Quote quote;
	Check(arulco.quoteContract(limit - 2, 0, 0, 1, 1, 0, true, true, quote) ==
		Error::None && SameQuote(quote, {limit - 2, 1, 1, limit}),
		"exact native finance maximum is representable with every component");
	Check(arulco.quoteContract(0, maximum, maximum, limit, maximum, 0, true, false,
		quote) == Error::None && SameQuote(quote, {0, limit, 0, limit}),
		"unselected salary and equipment values do not enter the quote");
	Check(arulco.quoteContract(0, maximum, maximum, maximum, limit, 0, false, true,
		quote) == Error::None && SameQuote(quote, {0, 0, limit, limit}),
		"uncharged medical deposit does not enter the quote");
	Check(arulco.quoteContract(0, 0, 0, 0, 0, 0, true, true, quote) == Error::None &&
		SameQuote(quote, {}), "zero charges are valid and fully replace a previous quote");

	const std::uint32_t excessive[][3] = {
		{limit + 1, 0, 0}, {limit, 1, 0}, {limit, 0, 1},
		{0, limit + 1, 0}, {0, 0, limit + 1},
		{maximum, 1, 0}, {maximum, maximum, 2}, {maximum, maximum, maximum}};
	const Quote retained{11, 22, 33, 66};
	for (const auto& charges : excessive)
	{
		quote = retained;
		Check(arulco.quoteContract(charges[0], 0, 0, charges[1], charges[2],
			0, true, true, quote) == Error::AmountOutOfRange &&
			SameQuote(quote, retained),
			"signed finance overflow and wrapping uint32 sums preserve output");
	}
}

void TestCheckedMissionFee()
{
	const Policy unfinishedBusiness(GameCampaign::UnfinishedBusiness);
	constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
	constexpr auto limit = static_cast<std::uint32_t>(
		std::numeric_limits<std::int32_t>::max());
	for (std::uint8_t duration = 0; duration < 3; ++duration)
		for (unsigned choices = 0; choices < 4; ++choices)
		{
			Quote quote{11, 22, 33, 66};
			Check(unfinishedBusiness.quoteContract(maximum, limit, maximum,
				maximum, maximum, duration, (choices & 1) != 0, (choices & 2) != 0,
				quote) == Error::None && SameQuote(quote, {limit, 0, 0, limit}),
				"UB uses the weekly mission fee once, without separate deposit or equipment charges");
		}
	const Quote retained{11, 22, 33, 66};
	Quote quote = retained;
	Check(unfinishedBusiness.quoteContract(1, limit + 1, 2, 0, 0, 0, false,
		false, quote) == Error::AmountOutOfRange && SameQuote(quote, retained),
		"UB validates the actual mission fee against the native finance limit");
}
}

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

	TestCheckedContractChoices();
	TestInvalidDurationPreservesOutput();
	TestNativeFinanceBoundaries();
	TestCheckedMissionFee();
	return failures ? 5 : 0;
}
