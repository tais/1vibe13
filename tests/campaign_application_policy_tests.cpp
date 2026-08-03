#include "CampaignApplicationPolicy.h"

int main()
{
	GameCapabilities arulcoCapabilities;
	const CampaignApplicationPolicy arulco(arulcoCapabilities);

	GameCapabilities unfinishedBusinessCapabilities;
	unfinishedBusinessCapabilities.campaign =
		GameCampaign::UnfinishedBusiness;
	const CampaignApplicationPolicy unfinishedBusiness(
		unfinishedBusinessCapabilities);

	if (arulco.usesUnfinishedBusinessContent() ||
		!arulco.usesLocalizedArulcoMercData() ||
		!arulco.usesArulcoMerchantRoster() ||
		!arulco.hasMeanwhileScenes() ||
		arulco.runsUnfinishedBusinessTacticalHooks() ||
		arulco.usesUnfinishedBusinessUndergroundLoadScreens() ||
		!arulco.shouldLoadExternalEnemyDeployment(false))
		return 1;

	if (!unfinishedBusiness.usesUnfinishedBusinessContent() ||
		unfinishedBusiness.usesLocalizedArulcoMercData() ||
		unfinishedBusiness.usesArulcoMerchantRoster() ||
		unfinishedBusiness.hasMeanwhileScenes() ||
		!unfinishedBusiness.runsUnfinishedBusinessTacticalHooks() ||
		!unfinishedBusiness.usesUnfinishedBusinessUndergroundLoadScreens() ||
		unfinishedBusiness.shouldLoadExternalEnemyDeployment(false) ||
		!unfinishedBusiness.shouldLoadExternalEnemyDeployment(true))
		return 2;

	return 0;
}
