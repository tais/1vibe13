#ifndef JA2_CAMPAIGN_AIM_SITE_POLICY_H
#define JA2_CAMPAIGN_AIM_SITE_POLICY_H

#include "GameCapabilities.h"

#include <cstdint>
#include <limits>

// Runtime choices for A.I.M.'s links and member pages. The laptop UI retains
// its established text, artwork, profile, finance, and save records while this
// value-only policy selects link availability and contract presentation.
class CampaignAimSitePolicy
{
public:
	enum class ContractLength : std::uint8_t
	{
		OneDay,
		OneWeek,
		TwoWeeks
	};

	struct ContractQuote
	{
		std::uint32_t salary = 0;
		std::uint32_t medicalDeposit = 0;
		std::uint32_t equipmentCost = 0;
		std::uint32_t total = 0;
	};

	enum class ContractQuoteError : std::uint8_t
	{
		None,
		InvalidContractLength,
		AmountOutOfRange
	};

	explicit constexpr CampaignAimSitePolicy(GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignAimSitePolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignAimSitePolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessSite() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	constexpr bool linkEnabled(
		bool unfinishedBusinessSetting) const noexcept
	{
		return !usesUnfinishedBusinessSite() || unfinishedBusinessSetting;
	}

	constexpr bool usesMissionFee() const noexcept
	{
		return usesUnfinishedBusinessSite();
	}

	constexpr bool showsSalaryBreakdown() const noexcept
	{
		return !usesUnfinishedBusinessSite();
	}

	constexpr bool showsOneTimeFeeOffer() const noexcept
	{
		return usesUnfinishedBusinessSite();
	}

	constexpr bool showsSelectionLights() const noexcept
	{
		return !usesUnfinishedBusinessSite();
	}

	constexpr bool forcesEquipmentPurchase() const noexcept
	{
		return usesUnfinishedBusinessSite();
	}

	constexpr bool hidesContractAndEquipmentButtons() const noexcept
	{
		return usesUnfinishedBusinessSite();
	}

	constexpr bool appendsMedicalDepositLabel() const noexcept
	{
		return !usesUnfinishedBusinessSite();
	}

	// Checked values for a native finance transaction, without reading or
	// changing campaign state. Failure leaves the caller's quote untouched.
	// Unfinished Business charges its weekly mission fee once; the included
	// equipment and any profile deposit do not become separate charges.
	constexpr ContractQuoteError quoteContract(
		std::uint32_t dailySalary,
		std::uint32_t weeklySalary,
		std::uint32_t biWeeklySalary,
		std::uint32_t medicalDeposit,
		std::uint32_t equipmentCost,
		std::uint8_t selectedContractLength,
		bool chargeMedicalDeposit,
		bool buyEquipment,
		ContractQuote& output) const noexcept
	{
		ContractQuote quote;
		switch (static_cast<ContractLength>(selectedContractLength))
		{
			case ContractLength::OneDay: quote.salary = dailySalary; break;
			case ContractLength::OneWeek: quote.salary = weeklySalary; break;
			case ContractLength::TwoWeeks: quote.salary = biWeeklySalary; break;
			default: return ContractQuoteError::InvalidContractLength;
		}
		if (usesMissionFee())
			quote.salary = weeklySalary;
		else
		{
			quote.medicalDeposit = chargeMedicalDeposit ? medicalDeposit : 0;
			quote.equipmentCost = buyEquipment ? equipmentCost : 0;
		}
		const std::uint64_t total = static_cast<std::uint64_t>(quote.salary) +
			quote.medicalDeposit + quote.equipmentCost;
		if (total > static_cast<std::uint64_t>(
				std::numeric_limits<std::int32_t>::max()))
			return ContractQuoteError::AmountOutOfRange;
		quote.total = static_cast<std::uint32_t>(total);
		output = quote;
		return ContractQuoteError::None;
	}

	constexpr std::uint32_t contractCharge(
		std::uint32_t dailySalary,
		std::uint32_t weeklySalary,
		std::uint32_t biWeeklySalary,
		std::uint32_t medicalDeposit,
		std::uint32_t equipmentCost,
		std::uint8_t selectedContractLength,
		bool chargeMedicalDeposit,
		bool buyEquipment) const noexcept
	{
		if (usesMissionFee())
			return weeklySalary;

		std::uint32_t charge = 0;
		switch (static_cast<ContractLength>(selectedContractLength))
		{
			case ContractLength::OneDay:
				charge = dailySalary;
				break;
			case ContractLength::OneWeek:
				charge = weeklySalary;
				break;
			case ContractLength::TwoWeeks:
				charge = biWeeklySalary;
				break;
		}

		if (chargeMedicalDeposit)
			charge += medicalDeposit;
		if (buyEquipment)
			charge += equipmentCost;
		return charge;
	}

private:
	GameCampaign campaign_;
};

static_assert(CampaignAimSitePolicy(GameCampaign::Arulco)
	.linkEnabled(false));
static_assert(!CampaignAimSitePolicy(GameCampaign::UnfinishedBusiness)
	.linkEnabled(false));
static_assert(CampaignAimSitePolicy(GameCampaign::UnfinishedBusiness)
	.contractCharge(100, 700, 1400, 250, 300, 0, true, true) == 700);

#endif
