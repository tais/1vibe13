#ifndef JA2_CAMPAIGN_LAPTOP_COMMUNICATIONS_POLICY_H
#define JA2_CAMPAIGN_LAPTOP_COMMUNICATIONS_POLICY_H

#include "GameCapabilities.h"

#include <cstdint>

// Runtime-owned campaign choices for email, insurance, and shipment notices.
// Laptop keeps responsibility for loading text and presenting messages; this
// value-only policy owns the legacy record identities and availability rules.
class CampaignLaptopCommunicationsPolicy
{
public:
	enum class Catalog : std::uint8_t
	{
		Arulco,
		UnfinishedBusiness
	};

	enum class InsuranceNotice : std::uint8_t
	{
		Payment,
		FirstInvestigation,
		RepeatInvestigation,
		InvestigationComplete,
		VerySuspiciousFraud,
		SuspiciousDeathFraud
	};

	// These values deliberately match EMAIL_TYPE's existing substitution tags.
	// Keeping them here avoids importing Laptop UI declarations into the policy.
	enum class Substitution : std::uint8_t
	{
		None = 0,
		AimDeathNotice = 1,
		AimNoRefund = 3,
		InsuranceVerySuspiciousFraud = 6,
		InsurancePayment = 7,
		InsuranceFirstInvestigation = 8,
		InsuranceRepeatInvestigation = 9,
		InsuranceInvestigationComplete = 10,
		BobbyShipment = 11
	};

	struct EmailRecord
	{
		std::uint16_t offset;
		std::uint16_t length;
		Catalog catalog;
		Substitution substitution;
		bool available;
	};

	explicit constexpr CampaignLaptopCommunicationsPolicy(
		GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignLaptopCommunicationsPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignLaptopCommunicationsPolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessCatalog() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	constexpr Catalog campaignCatalog() const noexcept
	{
		return usesUnfinishedBusinessCatalog()
			? Catalog::UnfinishedBusiness
			: Catalog::Arulco;
	}

	constexpr bool insuranceAvailable(
		bool laptopQuestComplete,
		bool laptopQuestEnabled,
		bool insuranceLinkEnabled) const noexcept
	{
		return !usesUnfinishedBusinessCatalog() ||
			((laptopQuestComplete || !laptopQuestEnabled) &&
				insuranceLinkEnabled);
	}

	constexpr bool bobbyShipmentNoticeAvailable(
		bool bobbySiteEnabled) const noexcept
	{
		return !usesUnfinishedBusinessCatalog() || bobbySiteEnabled;
	}

	constexpr bool johnKulbaShipmentNoticeAvailable() const noexcept
	{
		return !usesUnfinishedBusinessCatalog();
	}

	constexpr EmailRecord insuranceRecord(InsuranceNotice notice) const noexcept
	{
		const Catalog catalog = Catalog::Arulco;
		switch (notice)
		{
			case InsuranceNotice::Payment:
				return {170, 3, catalog,
					usesUnfinishedBusinessCatalog()
						? Substitution::InsurancePayment
						: Substitution::None,
					true};
			case InsuranceNotice::FirstInvestigation:
				return {173, 3, catalog,
					usesUnfinishedBusinessCatalog()
						? Substitution::InsuranceFirstInvestigation
						: Substitution::None,
					true};
			case InsuranceNotice::RepeatInvestigation:
				return {179, 3, catalog,
					usesUnfinishedBusinessCatalog()
						? Substitution::InsuranceRepeatInvestigation
						: Substitution::None,
					true};
			case InsuranceNotice::InvestigationComplete:
				return {176, 3, catalog,
					usesUnfinishedBusinessCatalog()
						? Substitution::InsuranceInvestigationComplete
						: Substitution::None,
					true};
			case InsuranceNotice::VerySuspiciousFraud:
				return {211, 3, catalog,
					usesUnfinishedBusinessCatalog()
						? Substitution::InsuranceVerySuspiciousFraud
						: Substitution::None,
					true};
			case InsuranceNotice::SuspiciousDeathFraud:
				return {267, 3, catalog, Substitution::None,
					!usesUnfinishedBusinessCatalog()};
		}
		return {0, 0, catalog, Substitution::None, false};
	}

	constexpr EmailRecord bobbyShipmentRecord() const noexcept
	{
		return {198, 4, Catalog::Arulco,
			usesUnfinishedBusinessCatalog()
				? Substitution::BobbyShipment
				: Substitution::None,
			true};
	}

	constexpr EmailRecord johnKulbaShipmentRecord() const noexcept
	{
		return {202, 4, Catalog::Arulco, Substitution::None,
			johnKulbaShipmentNoticeAvailable()};
	}

	constexpr EmailRecord deadMercNoticeRecord() const noexcept
	{
		return {206, 5, Catalog::Arulco,
			usesUnfinishedBusinessCatalog()
				? Substitution::AimDeathNotice
				: Substitution::None,
			true};
	}

	constexpr EmailRecord aimNoRefundRecord() const noexcept
	{
		return {217, 3, Catalog::Arulco,
			usesUnfinishedBusinessCatalog()
				? Substitution::AimNoRefund
				: Substitution::None,
			true};
	}

	constexpr std::uint16_t impIntroOffset() const noexcept
	{
		return usesUnfinishedBusinessCatalog() ? 83 : 0;
	}

	constexpr std::uint16_t impReminderOffset() const noexcept
	{
		return usesUnfinishedBusinessCatalog() ? 93 : 13;
	}

	constexpr std::uint16_t impProfileResultsOffset() const noexcept
	{
		return usesUnfinishedBusinessCatalog() ? 198 : 29;
	}

	constexpr bool isImpProfileResultsMessage(
		std::uint16_t offset, bool campaignCatalogEmail) const noexcept
	{
		return campaignCatalogEmail && offset == impProfileResultsOffset();
	}

	constexpr bool isMakeContactMessage(std::uint16_t offset) const noexcept
	{
		return usesUnfinishedBusinessCatalog() && offset == 10;
	}

private:
	GameCampaign campaign_;
};

static_assert(CampaignLaptopCommunicationsPolicy(GameCampaign::Arulco)
	.insuranceRecord(CampaignLaptopCommunicationsPolicy::InsuranceNotice::Payment)
	.offset == 170);
static_assert(!CampaignLaptopCommunicationsPolicy(
	GameCampaign::UnfinishedBusiness).johnKulbaShipmentNoticeAvailable());
static_assert(CampaignLaptopCommunicationsPolicy(
	GameCampaign::UnfinishedBusiness).impProfileResultsOffset() == 198);

#endif
