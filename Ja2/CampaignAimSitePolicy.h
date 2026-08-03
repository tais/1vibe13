#ifndef JA2_CAMPAIGN_AIM_SITE_POLICY_H
#define JA2_CAMPAIGN_AIM_SITE_POLICY_H

#include "GameCapabilities.h"

#include <cstdint>

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
