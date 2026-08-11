#include "CampaignCivilianQuotePolicy.h"

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
	const CampaignCivilianQuotePolicy arulco(GameCampaign::Arulco);
	const CampaignCivilianQuotePolicy unfinishedBusiness(
		GameCampaign::UnfinishedBusiness);

	Check(!arulco.usesUnfinishedBusinessQuoteCatalogue() &&
		unfinishedBusiness.usesUnfinishedBusinessQuoteCatalogue(),
		"only UB selects the extended civilian quote catalogue");
	Check(arulco.completesSurrenderOfferAfterQuote() &&
		!unfinishedBusiness.completesSurrenderOfferAfterQuote(),
		"only Arulco completes surrender after closing the quote");
	Check(arulco.civilianGroupQuoteBoundary(14, 19) == 14 &&
		unfinishedBusiness.civilianGroupQuoteBoundary(14, 19) == 19,
		"each campaign retains its exact dedicated-group boundary");

	for (const std::uint16_t quoteId : {0U, 254U})
	{
		Check(!arulco.discardsUnavailableQuote(quoteId) &&
			!unfinishedBusiness.discardsUnavailableQuote(quoteId),
			"ordinary quote identifiers remain available in both campaigns");
	}
	Check(!arulco.discardsUnavailableQuote(255U) &&
		unfinishedBusiness.discardsUnavailableQuote(255U),
		"only UB discards its unavailable quote sentinel");

	std::cout << "campaign civilian quote policy tests passed\n";
	return 0;
}
