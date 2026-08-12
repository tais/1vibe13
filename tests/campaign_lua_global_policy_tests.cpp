#include "CampaignLuaGlobalPolicy.h"

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

template <typename Gate>
void CheckUnfinishedBusinessOnly(
	Gate gate, bool expectedGate, const char* message)
{
	int unfinishedBusinessReadCount = 0;
	const bool result = gate() && [&unfinishedBusinessReadCount]() {
		++unfinishedBusinessReadCount;
		return true;
	}();
	Check(result == expectedGate &&
		unfinishedBusinessReadCount == (expectedGate ? 1 : 0), message);
}
}

int main()
{
	const CampaignLuaGlobalPolicy arulco(GameCampaign::Arulco);
	const CampaignLuaGlobalPolicy unfinishedBusiness(
		GameCampaign::UnfinishedBusiness);

	CheckUnfinishedBusinessOnly(
		[&arulco] {
			return arulco.exportsUnfinishedBusinessDifficultyAliases();
		}, false, "Arulco skips the UB-gated early difficulty writes");
	CheckUnfinishedBusinessOnly(
		[&unfinishedBusiness] {
			return unfinishedBusiness
				.exportsUnfinishedBusinessDifficultyAliases();
		}, true, "UB publishes both established difficulty aliases");

	int arulcoArrivalReads = 0;
	int unfinishedBusinessArrivalReads = 0;
	if (arulco.usesUnfinishedBusinessArrivalGrid())
		++unfinishedBusinessArrivalReads;
	else
		++arulcoArrivalReads;
	Check(arulcoArrivalReads == 1 && unfinishedBusinessArrivalReads == 0,
		"Arulco reads only its initial mercenary arrival grid");
	arulcoArrivalReads = 0;
	unfinishedBusinessArrivalReads = 0;
	if (unfinishedBusiness.usesUnfinishedBusinessArrivalGrid())
		++unfinishedBusinessArrivalReads;
	else
		++arulcoArrivalReads;
	Check(arulcoArrivalReads == 0 && unfinishedBusinessArrivalReads == 1,
		"UB reads only its LOCATEGRIDNO arrival grid");

	CheckUnfinishedBusinessOnly(
		[&arulco] {
			return arulco.exportsUnfinishedBusinessScenarioGlobals();
		}, false, "Arulco does not read UB scenario globals");
	CheckUnfinishedBusinessOnly(
		[&unfinishedBusiness] {
			return unfinishedBusiness
				.exportsUnfinishedBusinessScenarioGlobals();
		}, true, "UB publishes the established scenario globals");
	CheckUnfinishedBusinessOnly(
		[&arulco] {
			return arulco.exportsUnfinishedBusinessTestGlobal();
		}, false, "Arulco does not read the UB test global");
	CheckUnfinishedBusinessOnly(
		[&unfinishedBusiness] {
			return unfinishedBusiness.exportsUnfinishedBusinessTestGlobal();
		}, true, "UB publishes the established test global");
	CheckUnfinishedBusinessOnly(
		[&arulco] {
			return arulco
				.exportsUnfinishedBusinessCharacterAndItemGlobals();
		}, false, "Arulco does not read UB character or item globals");
	CheckUnfinishedBusinessOnly(
		[&unfinishedBusiness] {
			return unfinishedBusiness
				.exportsUnfinishedBusinessCharacterAndItemGlobals();
		}, true, "UB publishes the established character and item globals");

	std::cout << "campaign Lua-global policy tests passed\n";
	return 0;
}
