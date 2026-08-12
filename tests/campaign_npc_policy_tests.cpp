#include "CampaignNpcPolicy.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void Check(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	std::exit(1);
}

template <typename Gate>
void CheckShortCircuit(
	Gate gate, bool expectedGate, const char* message)
{
	int probeCount = 0;
	const bool result = gate() && [&probeCount]() {
		++probeCount;
		return true;
	}();
	Check(result == expectedGate && probeCount == (expectedGate ? 1 : 0),
		message);
}
}

int main()
{
	const CampaignNpcPolicy arulco(GameCampaign::Arulco);
	const CampaignNpcPolicy unfinishedBusiness(
		GameCampaign::UnfinishedBusiness);

	Check(!arulco.usesUnfinishedBusinessNpcQuoteFiles() &&
		unfinishedBusiness.usesUnfinishedBusinessNpcQuoteFiles(),
		"only UB evaluates its dedicated NPC quote-profile list");
	Check(arulco.usesMeanwhileNpcQuoteOverrides() &&
		!unfinishedBusiness.usesMeanwhileNpcQuoteOverrides(),
		"only Arulco evaluates meanwhile NPC quote overrides");
	for (const bool arulcoHerveFallbackSelected : {false, true})
	{
		Check(arulco.shouldApplyGeneralNpcQuoteRouting(
			arulcoHerveFallbackSelected) == !arulcoHerveFallbackSelected,
			"Arulco keeps the Herve fallback at the head of its else-chain");
		Check(unfinishedBusiness.shouldApplyGeneralNpcQuoteRouting(
			arulcoHerveFallbackSelected),
			"UB keeps its independent general NPC quote-routing pass");
	}
	Check(!arulco.runsMorrisHurtPlayerTurnHook() &&
		unfinishedBusiness.runsMorrisHurtPlayerTurnHook(),
		"only UB evaluates the Morris hurt-player turn hook");
	Check(arulco.allowsEnemySurrenderOffers() &&
		!unfinishedBusiness.allowsEnemySurrenderOffers(),
		"only Arulco evaluates enemy surrender offers");

	for (const std::uint32_t version :
		std::array<std::uint32_t, 4>{0U, 91U, 92U,
			std::numeric_limits<std::uint32_t>::max()})
	{
		Check(arulco.shouldRefreshAuntieNpcScriptRecord(version) ==
			(version < 92U),
			"Arulco preserves the exact pre-92 Auntie refresh boundary");
		Check(!unfinishedBusiness.shouldRefreshAuntieNpcScriptRecord(version),
			"UB never refreshes the Arulco Auntie script record");
	}

	CheckShortCircuit(
		[&arulco] { return arulco.usesUnfinishedBusinessNpcQuoteFiles(); },
		false, "Arulco does not evaluate UB quote-profile state");
	CheckShortCircuit(
		[&unfinishedBusiness] {
			return unfinishedBusiness.usesUnfinishedBusinessNpcQuoteFiles();
		}, true, "UB evaluates its quote-profile state");
	CheckShortCircuit(
		[&unfinishedBusiness] {
			return unfinishedBusiness.usesMeanwhileNpcQuoteOverrides();
		}, false, "UB does not evaluate Arulco meanwhile state");
	CheckShortCircuit(
		[&arulco] { return arulco.runsMorrisHurtPlayerTurnHook(); },
		false, "Arulco does not evaluate Morris state");
	CheckShortCircuit(
		[&unfinishedBusiness] {
			return unfinishedBusiness.allowsEnemySurrenderOffers();
		}, false, "UB does not evaluate Arulco surrender state");

	std::cout << "campaign NPC policy tests passed\n";
	return 0;
}
