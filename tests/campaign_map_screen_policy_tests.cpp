#include "CampaignMapScreenPolicy.h"

#include <cstdlib>
#include <iostream>

namespace
{
	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main()
{
	const CampaignMapScreenPolicy arulco(GameCampaign::Arulco);
	const CampaignMapScreenPolicy unfinishedBusiness(
		GameCampaign::UnfinishedBusiness);

	Check(!arulco.usesUnfinishedBusinessMapRules() &&
		unfinishedBusiness.usesUnfinishedBusinessMapRules(),
		"only UB enables its strategic map rules");
	Check(!arulco.usesJerryMiloGuidance() &&
		unfinishedBusiness.usesJerryMiloGuidance(),
		"only UB enables Jerry Milo map guidance");
	Check(arulco.hasMeanwhileScenes() &&
		!unfinishedBusiness.hasMeanwhileScenes(),
		"only Arulco checks for meanwhile scenes on the map screen");
	Check(arulco.treatsSanMonaAsUnimportant() &&
		!unfinishedBusiness.treatsSanMonaAsUnimportant(),
		"only Arulco suppresses the San Mona town-loss notification");
	Check(!arulco.usesUnfinishedBusinessLossDialogue() &&
		unfinishedBusiness.usesUnfinishedBusinessLossDialogue(),
		"only UB plays its contested-town loss dialogue");
	Check(!arulco.usesConfigurableMapBorderButtons() &&
		unfinishedBusiness.usesConfigurableMapBorderButtons(),
		"only UB applies its configurable map-border buttons");
	Check(!arulco.runsUnfinishedBusinessStrategicAi() &&
		unfinishedBusiness.runsUnfinishedBusinessStrategicAi(),
		"only UB runs its strategic AI from the map screen");

	for (const bool requested : {false, true})
	{
		Check(!arulco.shouldRebuildCustomMapList(requested),
			"Arulco never rebuilds the UB custom-map list");
		Check(unfinishedBusiness.shouldRebuildCustomMapList(requested) ==
			requested,
			"UB rebuilds custom maps exactly when requested");
	}

	for (const bool enabled : {false, true})
	{
		Check(!arulco.shouldPumpJerryMiloQuotes(enabled),
			"Arulco never pumps Jerry Milo quotes");
		Check(unfinishedBusiness.shouldPumpJerryMiloQuotes(enabled) ==
			enabled,
			"UB pumps Jerry Milo quotes exactly when enabled");

		Check(!arulco.shouldCheckHelicopterCampaignLoss(enabled),
			"Arulco never checks the UB helicopter-loss path");
		Check(unfinishedBusiness.shouldCheckHelicopterCampaignLoss(enabled) ==
			enabled,
			"UB checks campaign loss exactly after its helicopter crash");

		Check(!arulco.shouldDisableAutoResolve(enabled),
			"Arulco auto-resolve availability ignores the UB option");
		Check(unfinishedBusiness.shouldDisableAutoResolve(enabled) == !enabled,
			"UB disables auto-resolve exactly when configured off");
	}

	std::cout << "campaign map-screen policy tests passed\n";
	return 0;
}
