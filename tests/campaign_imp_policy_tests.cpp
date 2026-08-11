#include "CampaignImpPolicy.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

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
	using Decision = CampaignImpPolicy::ActivationDecision;
	const CampaignImpPolicy arulco(GameCampaign::Arulco);
	const CampaignImpPolicy unfinishedBusiness(
		GameCampaign::UnfinishedBusiness);

	Check(!arulco.usesUnfinishedBusinessImpRules() &&
		unfinishedBusiness.usesUnfinishedBusinessImpRules(),
		"only UB enables its IMP content rules");

	for (const bool matchesJa2 : {false, true})
	for (const bool matchesUb : {false, true})
	for (const std::uint8_t laptopState : {
		std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2},
		std::uint8_t{255}})
	for (const bool ja2Enabled : {false, true})
	for (const bool ubEnabled : {false, true})
	{
		const Decision expectedArulco = matchesJa2
			? (laptopState < 2
				? Decision::Authorized
				: Decision::KnownButUnavailable)
			: Decision::Invalid;
		Check(arulco.classifyActivation(
			matchesJa2, matchesUb, laptopState, ja2Enabled, ubEnabled) ==
			expectedArulco,
			"Arulco preserves its XEP624 and new-laptop truth table");

		Decision expectedUb = Decision::KnownButUnavailable;
		if ((ja2Enabled && matchesJa2) ||
			(ubEnabled && matchesUb && laptopState < 2))
			expectedUb = Decision::Authorized;
		else if ((ja2Enabled && !matchesJa2) ||
			(ubEnabled && !matchesUb))
			expectedUb = Decision::Invalid;
		Check(unfinishedBusiness.classifyActivation(
			matchesJa2, matchesUb, laptopState, ja2Enabled, ubEnabled) ==
			expectedUb,
			"UB preserves enabled-pass and legacy-precedence truth table");
	}

	Check(std::string_view(arulco.impTextResource(false)) ==
			"BINARYDATA\\IMPText.EDT" &&
		std::string_view(arulco.impTextResource(true)) ==
			"BINARYDATA\\IMPText.EDT",
		"Arulco always selects its established IMP text resource");
	Check(std::string_view(unfinishedBusiness.impTextResource(true)) ==
			"BINARYDATA\\IMPText25.edt" &&
		std::string_view(unfinishedBusiness.impTextResource(false)) ==
			"BINARYDATA\\IMPText.edt",
		"UB selects IMPText25 with its established IMPText fallback");

	std::cout << "campaign IMP policy tests passed\n";
	return 0;
}
