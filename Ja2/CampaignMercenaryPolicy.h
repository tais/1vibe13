#ifndef JA2_CAMPAIGN_MERCENARY_POLICY_H
#define JA2_CAMPAIGN_MERCENARY_POLICY_H

#include "CampaignProfileCodes.h"
#include "GameCapabilities.h"

// Runtime decisions for profile loading and the mercenary lifecycle. Profile
// bytes remain campaign data: callers resolve semantic roles through this
// policy instead of depending on the executable that compiled them.
class CampaignMercenaryPolicy
{
public:
	enum class DismissalRefusalQuote : std::uint8_t
	{
		AnsweringMachine,
		RefusingOrder
	};

	explicit constexpr CampaignMercenaryPolicy(GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignMercenaryPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignMercenaryPolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessRules() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	constexpr const char* primaryProfileDataFile() const noexcept
	{
		return usesUnfinishedBusinessRules()
			? "BINARYDATA\\JA25PROF.DAT"
			: "BINARYDATA\\Prof.dat";
	}

	constexpr const char* fallbackProfileDataFile() const noexcept
	{
		return "BINARYDATA\\Prof.dat";
	}

	constexpr int initialAssignmentChanceMultiplier() const noexcept
	{
		return usesUnfinishedBusinessRules() ? 3 : 5;
	}

	constexpr std::uint8_t profile(CampaignProfileCode::Role role) const noexcept
	{
		return CampaignProfileCode::profile(campaign_, role);
	}

	constexpr bool isProfile(
		std::uint8_t rawProfile, CampaignProfileCode::Role role) const noexcept
	{
		return CampaignProfileCode::matches(campaign_, role, rawProfile);
	}

	constexpr bool givesUnfinishedBusinessHireGear() const noexcept
	{
		return usesUnfinishedBusinessRules();
	}

	constexpr bool givesInitialArulcoLetter() const noexcept
	{
		return !usesUnfinishedBusinessRules();
	}

	constexpr bool usesGroundArrival(bool inGameHelicopter) const noexcept
	{
		return usesUnfinishedBusinessRules() && !inGameHelicopter;
	}

	constexpr int helicopterDropGridNo(
		int arulcoGridNo, int unfinishedBusinessGridNo) const noexcept
	{
		return usesUnfinishedBusinessRules()
			? unfinishedBusinessGridNo
			: arulcoGridNo;
	}

	constexpr bool shouldStartArrivalHelicopter(
		bool usesChopperInsertion,
		bool isAtDefaultArrivalSector,
		bool inGameHelicopter) const noexcept
	{
		if (usesChopperInsertion)
			return false;
		return !usesUnfinishedBusinessRules() ||
			(isAtDefaultArrivalSector && inGameHelicopter);
	}

	constexpr bool usesGridInsertionForOffscreenArrival() const noexcept
	{
		return usesUnfinishedBusinessRules();
	}

	constexpr bool shouldPlayReachedDestinationQuote(
		bool firstUnfinishedBusinessCrash) const noexcept
	{
		return !usesUnfinishedBusinessRules() ||
			!firstUnfinishedBusinessCrash;
	}

	constexpr bool shouldSkipBuddyArrivalHandling(
		bool arrivalGetupPending) const noexcept
	{
		return usesUnfinishedBusinessRules() && arrivalGetupPending;
	}

	constexpr bool setsStartDayForEveryHire() const noexcept
	{
		return usesUnfinishedBusinessRules();
	}

	constexpr bool runsJohnKulbaArrivalDelay() const noexcept
	{
		return !usesUnfinishedBusinessRules();
	}

	constexpr bool excludesRecruitedSlayFromTerrorists() const noexcept
	{
		return !usesUnfinishedBusinessRules();
	}

	constexpr bool triggersIraRecruitmentRecord() const noexcept
	{
		return !usesUnfinishedBusinessRules();
	}

	constexpr bool playsNpcRecruitmentTeamQuote() const noexcept
	{
		return usesUnfinishedBusinessRules();
	}

	constexpr bool notifiesSpeckOfLarryRelapse() const noexcept
	{
		return !usesUnfinishedBusinessRules();
	}

	constexpr bool usesUnfinishedBusinessSectorCoolness() const noexcept
	{
		return usesUnfinishedBusinessRules();
	}

	constexpr bool shouldSendAimAvailabilityEmail(
		bool laptopQuestEnabled,
		bool laptopQuestInProgress) const noexcept
	{
		return !usesUnfinishedBusinessRules() ||
			(laptopQuestEnabled && !laptopQuestInProgress);
	}

	constexpr int aimAvailabilityEmailOffset() const noexcept
	{
		return usesUnfinishedBusinessRules() ? 98 : 58;
	}

	constexpr int aimAvailabilityEmailLength() const noexcept
	{
		return 2;
	}

	constexpr bool shouldSendMedicalDepositEmail(
		bool laptopQuestEnabled,
		bool laptopQuestDone,
		bool deadMercEmailEnabled) const noexcept
	{
		return !usesUnfinishedBusinessRules() ||
			((!laptopQuestEnabled || laptopQuestDone) &&
				deadMercEmailEnabled);
	}

	constexpr bool usesUnfinishedBusinessMedicalDepositEmail() const noexcept
	{
		return usesUnfinishedBusinessRules();
	}

	constexpr bool runsSlayDailyEvent() const noexcept
	{
		return !usesUnfinishedBusinessRules();
	}

	constexpr bool includesDevinInNpcContractGroup() const noexcept
	{
		return !usesUnfinishedBusinessRules();
	}

	// UB's tunnel sequence prevents dismissing a merc from sector column 14
	// onward. Arulco has no campaign dismissal restriction.
	constexpr bool allowsDismissalFromSector(int sectorX) const noexcept
	{
		return !usesUnfinishedBusinessRules() || sectorX < 14;
	}

	constexpr DismissalRefusalQuote dismissalRefusalQuote(
		bool qualifiedMerc) const noexcept
	{
		return qualifiedMerc
			? DismissalRefusalQuote::AnsweringMachine
			: DismissalRefusalQuote::RefusingOrder;
	}

private:
	GameCampaign campaign_;
};

static_assert(CampaignMercenaryPolicy(GameCampaign::Arulco)
	.initialAssignmentChanceMultiplier() == 5);
static_assert(CampaignMercenaryPolicy(GameCampaign::UnfinishedBusiness)
	.initialAssignmentChanceMultiplier() == 3);
static_assert(CampaignMercenaryPolicy(GameCampaign::Arulco)
	.profile(CampaignProfileCode::Role::Miguel) == 57);
static_assert(CampaignMercenaryPolicy(GameCampaign::UnfinishedBusiness)
	.profile(CampaignProfileCode::Role::Miguel) == 58);
static_assert(CampaignMercenaryPolicy(GameCampaign::Arulco)
	.allowsDismissalFromSector(14));
static_assert(!CampaignMercenaryPolicy(GameCampaign::UnfinishedBusiness)
	.allowsDismissalFromSector(14));

#endif
