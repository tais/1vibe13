#include "CampaignDoorPolicy.h"

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
	const CampaignDoorPolicy arulco(GameCampaign::Arulco);
	const CampaignDoorPolicy unfinishedBusiness(
		GameCampaign::UnfinishedBusiness);

	Check(!arulco.usesUnfinishedBusinessTunnelGate(),
		"Arulco has no UB tunnel-gate content");
	Check(unfinishedBusiness.usesUnfinishedBusinessTunnelGate(),
		"UB enables its tunnel-gate content at runtime");
	Check(!arulco.usesOpenDoorCostWhenForcing(),
		"Arulco forcing retains the boot-door AP cost");
	Check(unfinishedBusiness.usesOpenDoorCostWhenForcing(),
		"UB forcing retains its open-door AP cost");

	Check(arulco.shouldAttemptForceDoor(true) &&
		arulco.shouldAttemptForceDoor(false),
		"Arulco always performs the ordinary force attempt");
	Check(!unfinishedBusiness.shouldAttemptForceDoor(true) &&
		unfinishedBusiness.shouldAttemptForceDoor(false),
		"UB tunnel dialogue alone consumes a force attempt");
	Check(arulco.shouldAttemptDoorMenuAction(true) &&
		arulco.shouldAttemptDoorMenuAction(false),
		"Arulco door-menu actions never invoke the UB tunnel gate");
	Check(!unfinishedBusiness.shouldAttemptDoorMenuAction(true) &&
		unfinishedBusiness.shouldAttemptDoorMenuAction(false),
		"UB tunnel dialogue consumes each intercepted door-menu action");

	Check(arulco.shouldOfferFailedUnlockCurse(true) &&
		arulco.shouldOfferFailedUnlockCurse(false),
		"Arulco always retains its failed-unlock curse opportunity");
	Check(unfinishedBusiness.shouldOfferFailedUnlockCurse(true) &&
		!unfinishedBusiness.shouldOfferFailedUnlockCurse(false),
		"UB offers the curse only after its tunnel quote handles the failure");

	std::cout << "campaign door policy tests passed\n";
	return 0;
}
