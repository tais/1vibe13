#include "CampaignMercenaryPolicy.h"

#include <cstring>

int main()
{
	const CampaignMercenaryPolicy arulco(GameCampaign::Arulco);
	GameCapabilities unfinishedBusinessCapabilities;
	unfinishedBusinessCapabilities.campaign =
		GameCampaign::UnfinishedBusiness;
	const CampaignMercenaryPolicy unfinishedBusiness(
		unfinishedBusinessCapabilities);

	if (arulco.usesUnfinishedBusinessRules() ||
		std::strcmp(arulco.primaryProfileDataFile(),
			"BINARYDATA\\Prof.dat") != 0 ||
		std::strcmp(arulco.fallbackProfileDataFile(),
			"BINARYDATA\\Prof.dat") != 0 ||
		arulco.initialAssignmentChanceMultiplier() != 5 ||
		!arulco.givesInitialArulcoLetter() ||
		arulco.givesUnfinishedBusinessHireGear() ||
		arulco.usesGroundArrival(false) ||
		arulco.helicopterDropGridNo(101, 202) != 101 ||
		!arulco.shouldStartArrivalHelicopter(false, false, false) ||
		arulco.shouldStartArrivalHelicopter(true, true, true) ||
		arulco.usesGridInsertionForOffscreenArrival() ||
		!arulco.shouldPlayReachedDestinationQuote(true) ||
		arulco.shouldSkipBuddyArrivalHandling(true) ||
		arulco.setsStartDayForEveryHire() ||
		!arulco.runsJohnKulbaArrivalDelay() ||
		!arulco.excludesRecruitedSlayFromTerrorists() ||
		!arulco.triggersIraRecruitmentRecord() ||
		arulco.playsNpcRecruitmentTeamQuote() ||
		!arulco.notifiesSpeckOfLarryRelapse() ||
		arulco.usesUnfinishedBusinessSectorCoolness() ||
		!arulco.shouldSendAimAvailabilityEmail(false, true) ||
		arulco.aimAvailabilityEmailOffset() != 58 ||
		arulco.aimAvailabilityEmailLength() != 2 ||
		!arulco.shouldSendMedicalDepositEmail(true, false, false) ||
		arulco.usesUnfinishedBusinessMedicalDepositEmail() ||
		!arulco.runsSlayDailyEvent() ||
		!arulco.includesDevinInNpcContractGroup())
		return 1;

	if (!unfinishedBusiness.usesUnfinishedBusinessRules() ||
		std::strcmp(unfinishedBusiness.primaryProfileDataFile(),
			"BINARYDATA\\JA25PROF.DAT") != 0 ||
		std::strcmp(unfinishedBusiness.fallbackProfileDataFile(),
			"BINARYDATA\\Prof.dat") != 0 ||
		unfinishedBusiness.initialAssignmentChanceMultiplier() != 3 ||
		unfinishedBusiness.givesInitialArulcoLetter() ||
		!unfinishedBusiness.givesUnfinishedBusinessHireGear() ||
		!unfinishedBusiness.usesGroundArrival(false) ||
		unfinishedBusiness.usesGroundArrival(true) ||
		unfinishedBusiness.helicopterDropGridNo(101, 202) != 202 ||
		!unfinishedBusiness.shouldStartArrivalHelicopter(false, true, true) ||
		unfinishedBusiness.shouldStartArrivalHelicopter(false, false, true) ||
		unfinishedBusiness.shouldStartArrivalHelicopter(false, true, false) ||
		!unfinishedBusiness.usesGridInsertionForOffscreenArrival() ||
		unfinishedBusiness.shouldPlayReachedDestinationQuote(true) ||
		!unfinishedBusiness.shouldPlayReachedDestinationQuote(false) ||
		!unfinishedBusiness.shouldSkipBuddyArrivalHandling(true) ||
		!unfinishedBusiness.setsStartDayForEveryHire() ||
		unfinishedBusiness.runsJohnKulbaArrivalDelay() ||
		unfinishedBusiness.excludesRecruitedSlayFromTerrorists() ||
		unfinishedBusiness.triggersIraRecruitmentRecord() ||
		!unfinishedBusiness.playsNpcRecruitmentTeamQuote() ||
		unfinishedBusiness.notifiesSpeckOfLarryRelapse() ||
		!unfinishedBusiness.usesUnfinishedBusinessSectorCoolness() ||
		unfinishedBusiness.shouldSendAimAvailabilityEmail(false, false) ||
		unfinishedBusiness.shouldSendAimAvailabilityEmail(true, true) ||
		!unfinishedBusiness.shouldSendAimAvailabilityEmail(true, false) ||
		unfinishedBusiness.aimAvailabilityEmailOffset() != 98 ||
		unfinishedBusiness.aimAvailabilityEmailLength() != 2 ||
		unfinishedBusiness.shouldSendMedicalDepositEmail(true, false, true) ||
		unfinishedBusiness.shouldSendMedicalDepositEmail(false, false, false) ||
		!unfinishedBusiness.shouldSendMedicalDepositEmail(false, false, true) ||
		!unfinishedBusiness.shouldSendMedicalDepositEmail(true, true, true) ||
		!unfinishedBusiness.usesUnfinishedBusinessMedicalDepositEmail() ||
		unfinishedBusiness.runsSlayDailyEvent() ||
		unfinishedBusiness.includesDevinInNpcContractGroup())
		return 2;

	for (int role = static_cast<int>(CampaignProfileCode::Role::Miguel);
		role <= static_cast<int>(CampaignProfileCode::Role::Slay); ++role)
	{
		const auto profileRole = static_cast<CampaignProfileCode::Role>(role);
		const std::uint8_t arulcoProfile = arulco.profile(profileRole);
		const std::uint8_t unfinishedBusinessProfile =
			unfinishedBusiness.profile(profileRole);
		if (unfinishedBusinessProfile != arulcoProfile + 1 ||
			!arulco.isProfile(arulcoProfile, profileRole) ||
			!unfinishedBusiness.isProfile(
				unfinishedBusinessProfile, profileRole) ||
			arulco.isProfile(unfinishedBusinessProfile, profileRole) ||
			unfinishedBusiness.isProfile(arulcoProfile, profileRole))
			return 3;
	}

	return 0;
}
