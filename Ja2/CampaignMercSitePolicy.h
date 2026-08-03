#ifndef JA2_CAMPAIGN_MERC_SITE_POLICY_H
#define JA2_CAMPAIGN_MERC_SITE_POLICY_H

#include "CampaignSpeckQuoteCodes.h"
#include "GameCapabilities.h"

#include <array>
#include <cstddef>
#include <cstdint>

// Runtime choices for the M.E.R.C. laptop site. The page implementations keep
// their established text, image, profile, finance, and save records while this
// value-only policy selects account, pricing, equipment, and speech behavior.
class CampaignMercSitePolicy
{
public:
	explicit constexpr CampaignMercSitePolicy(GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignMercSitePolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignMercSitePolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessSite() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	constexpr bool createsAccountAtGameStart() const noexcept
	{
		return usesUnfinishedBusinessSite();
	}

	constexpr bool hasAccountManagement() const noexcept
	{
		return !usesUnfinishedBusinessSite();
	}

	constexpr bool usesDeferredBilling() const noexcept
	{
		return !usesUnfinishedBusinessSite();
	}

	constexpr bool showsSpecialOffer() const noexcept
	{
		return usesUnfinishedBusinessSite();
	}

	constexpr bool firstEquipmentKitIsFree(
		std::uint8_t selectedKit) const noexcept
	{
		return usesUnfinishedBusinessSite() && selectedKit == 0;
	}

	constexpr bool chargesEquipmentUpFront() const noexcept
	{
		return usesUnfinishedBusinessSite();
	}

	constexpr bool marksPurchasedEquipmentUnpaid() const noexcept
	{
		return !chargesEquipmentUpFront();
	}

	constexpr bool requiresAvailableSpeckForDialogue() const noexcept
	{
		return !usesUnfinishedBusinessSite();
	}

	constexpr bool supportsServerOutage() const noexcept
	{
		return !usesUnfinishedBusinessSite();
	}

	constexpr bool supportsArulcoRecruitableMercs() const noexcept
	{
		return !usesUnfinishedBusinessSite();
	}

	constexpr bool usesImportantUnfinishedBusinessQuotes() const noexcept
	{
		return usesUnfinishedBusinessSite();
	}

	constexpr std::uint32_t contractRate(
		std::uint32_t arulcoDailySalary,
		std::uint32_t unfinishedBusinessMissionFee) const noexcept
	{
		return usesUnfinishedBusinessSite()
			? unfinishedBusinessMissionFee
			: arulcoDailySalary;
	}

	constexpr std::uint32_t initialHireCharge(
		std::uint32_t missionFee,
		std::uint32_t equipmentCost,
		std::uint8_t selectedKit) const noexcept
	{
		if (!chargesEquipmentUpFront())
			return 0;
		return missionFee + (selectedKit == 0 ? 0 : equipmentCost);
	}

	constexpr std::uint16_t quote(
		CampaignSpeckQuoteCode::Role role) const noexcept
	{
		return CampaignSpeckQuoteCode::quote(campaign_, role);
	}

	constexpr std::uint16_t firstVisitIntroFirst() const noexcept
	{
		return usesUnfinishedBusinessSite()
			? CampaignSpeckQuoteCode::UnfinishedBusiness::FirstVisitIntroFirst
			: CampaignSpeckQuoteCode::Arulco::FirstVisitIntroFirst;
	}

	constexpr std::uint16_t firstVisitIntroLast() const noexcept
	{
		return usesUnfinishedBusinessSite()
			? CampaignSpeckQuoteCode::UnfinishedBusiness::FirstVisitIntroLast
			: CampaignSpeckQuoteCode::Arulco::FirstVisitIntroLast;
	}

	constexpr std::size_t randomQuoteCount() const noexcept
	{
		return usesUnfinishedBusinessSite()
			? unfinishedBusinessRandomQuotes_.size()
			: arulcoRandomQuotes_.size();
	}

	constexpr std::uint16_t randomQuote(std::size_t index) const noexcept
	{
		return usesUnfinishedBusinessSite()
			? unfinishedBusinessRandomQuotes_[index]
			: arulcoRandomQuotes_[index];
	}

private:
	inline static constexpr std::array<std::uint16_t, 19>
		arulcoRandomQuotes_ = {
			CampaignSpeckQuoteCode::arulcoQuote(
				CampaignSpeckQuoteCode::Role::RandomChitChat1),
			CampaignSpeckQuoteCode::arulcoQuote(
				CampaignSpeckQuoteCode::Role::RandomChitChat2),
			CampaignSpeckQuoteCode::arulcoQuote(
				CampaignSpeckQuoteCode::Role::AdvertiseGaston),
			CampaignSpeckQuoteCode::arulcoQuote(
				CampaignSpeckQuoteCode::Role::AdvertiseStogie),
			CampaignSpeckQuoteCode::Shared::SellsBiff,
			CampaignSpeckQuoteCode::Shared::SellsHaywire,
			CampaignSpeckQuoteCode::Shared::SellsGasket,
			CampaignSpeckQuoteCode::Shared::SellsRazor,
			CampaignSpeckQuoteCode::Shared::SellsFlo,
			CampaignSpeckQuoteCode::Shared::SellsGumpy,
			CampaignSpeckQuoteCode::Shared::SellsLarry,
			CampaignSpeckQuoteCode::Shared::SellsCougar,
			CampaignSpeckQuoteCode::Shared::SellsNumb,
			CampaignSpeckQuoteCode::Shared::SellsBubba,
			CampaignSpeckQuoteCode::Shared::AimSlander1,
			CampaignSpeckQuoteCode::Shared::AimSlander2,
			CampaignSpeckQuoteCode::Shared::AimSlander3,
			CampaignSpeckQuoteCode::Shared::AimSlander4,
			CampaignSpeckQuoteCode::Arulco::SellsHimself};

	inline static constexpr std::array<std::uint16_t, 20>
		unfinishedBusinessRandomQuotes_ = {
			CampaignSpeckQuoteCode::UnfinishedBusiness::BiffDeadWhenImporting,
			CampaignSpeckQuoteCode::unfinishedBusinessQuote(
				CampaignSpeckQuoteCode::Role::RandomChitChat1),
			CampaignSpeckQuoteCode::unfinishedBusinessQuote(
				CampaignSpeckQuoteCode::Role::RandomChitChat2),
			CampaignSpeckQuoteCode::UnfinishedBusiness::RandomChitChat3,
			CampaignSpeckQuoteCode::unfinishedBusinessQuote(
				CampaignSpeckQuoteCode::Role::AdvertiseGaston),
			CampaignSpeckQuoteCode::unfinishedBusinessQuote(
				CampaignSpeckQuoteCode::Role::AdvertiseStogie),
			CampaignSpeckQuoteCode::Shared::SellsBiff,
			CampaignSpeckQuoteCode::Shared::SellsHaywire,
			CampaignSpeckQuoteCode::Shared::SellsGasket,
			CampaignSpeckQuoteCode::Shared::SellsRazor,
			CampaignSpeckQuoteCode::Shared::SellsFlo,
			CampaignSpeckQuoteCode::Shared::SellsGumpy,
			CampaignSpeckQuoteCode::Shared::SellsLarry,
			CampaignSpeckQuoteCode::Shared::SellsCougar,
			CampaignSpeckQuoteCode::Shared::SellsNumb,
			CampaignSpeckQuoteCode::Shared::SellsBubba,
			CampaignSpeckQuoteCode::Shared::AimSlander1,
			CampaignSpeckQuoteCode::Shared::AimSlander2,
			CampaignSpeckQuoteCode::Shared::AimSlander3,
			CampaignSpeckQuoteCode::Shared::AimSlander4};

	GameCampaign campaign_;
};

static_assert(CampaignMercSitePolicy(GameCampaign::Arulco)
	.hasAccountManagement());
static_assert(CampaignMercSitePolicy(GameCampaign::UnfinishedBusiness)
	.firstEquipmentKitIsFree(0));
static_assert(CampaignMercSitePolicy(GameCampaign::Arulco)
	.randomQuoteCount() == 19);
static_assert(CampaignMercSitePolicy(GameCampaign::UnfinishedBusiness)
	.randomQuoteCount() == 20);

#endif
