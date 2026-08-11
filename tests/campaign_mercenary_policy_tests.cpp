#include "CampaignMercenaryPolicy.h"

#include <array>
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
		!arulco.includesDevinInNpcContractGroup() ||
		!arulco.allowsDismissalFromSector(13) ||
		!arulco.allowsDismissalFromSector(14) ||
		arulco.dismissalRefusalQuote(true) !=
			CampaignMercenaryPolicy::DismissalRefusalQuote::AnsweringMachine ||
		arulco.dismissalRefusalQuote(false) !=
			CampaignMercenaryPolicy::DismissalRefusalQuote::RefusingOrder)
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
		unfinishedBusiness.includesDevinInNpcContractGroup() ||
		!unfinishedBusiness.allowsDismissalFromSector(13) ||
		unfinishedBusiness.allowsDismissalFromSector(14) ||
		unfinishedBusiness.dismissalRefusalQuote(true) !=
			CampaignMercenaryPolicy::DismissalRefusalQuote::AnsweringMachine ||
		unfinishedBusiness.dismissalRefusalQuote(false) !=
			CampaignMercenaryPolicy::DismissalRefusalQuote::RefusingOrder)
		return 2;

	constexpr std::array<std::uint8_t, 8> arulcoProfiles =
		{57, 58, 59, 60, 61, 62, 63, 64};
	constexpr std::array<std::uint8_t, 8> unfinishedBusinessProfiles =
		{58, 59, 60, 61, 62, 63, 64, 65};
	constexpr std::array<CampaignProfileCode::Role, 8> profileRoles = {
		CampaignProfileCode::Role::Miguel,
		CampaignProfileCode::Role::Carlos,
		CampaignProfileCode::Role::Ira,
		CampaignProfileCode::Role::Dimitri,
		CampaignProfileCode::Role::Devin,
		CampaignProfileCode::Role::Robot,
		CampaignProfileCode::Role::Hamous,
		CampaignProfileCode::Role::Slay};
	for (std::size_t role = 0; role < profileRoles.size(); ++role)
	{
		const auto profileRole = profileRoles[role];
		const std::uint8_t arulcoProfile = arulco.profile(profileRole);
		const std::uint8_t unfinishedBusinessProfile =
			unfinishedBusiness.profile(profileRole);
		if (arulcoProfile != arulcoProfiles[role] ||
			unfinishedBusinessProfile != unfinishedBusinessProfiles[role] ||
			!arulco.isProfile(arulcoProfile, profileRole) ||
			!unfinishedBusiness.isProfile(
				unfinishedBusinessProfile, profileRole) ||
			arulco.isProfile(unfinishedBusinessProfile, profileRole) ||
			unfinishedBusiness.isProfile(arulcoProfile, profileRole))
			return 3;
	}

	return 0;
}
