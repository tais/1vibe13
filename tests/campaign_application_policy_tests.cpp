#include "CampaignApplicationPolicy.h"

#include <string_view>
#include <vector>

namespace
{
	using Trace = std::vector<std::string_view>;

	Trace TraceOptionsBootstrap(
		const CampaignApplicationPolicy& policy)
	{
		Trace trace{"rebel-settings"};
		if (policy.shouldLoadUnfinishedBusinessOptions())
			trace.emplace_back("ub-options");
		trace.emplace_back("visibility-ranges");
		return trace;
	}
}

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
		arulco.shouldLoadUnfinishedBusinessOptions() ||
		!arulco.usesLocalizedArulcoMercData() ||
		!arulco.usesArulcoMerchantRoster() ||
		!arulco.hasMeanwhileScenes() ||
		arulco.runsUnfinishedBusinessTacticalHooks() ||
		arulco.usesUnfinishedBusinessUndergroundLoadScreens() ||
		!arulco.shouldLoadExternalEnemyDeployment(false))
		return 1;

	if (!unfinishedBusiness.usesUnfinishedBusinessContent() ||
		!unfinishedBusiness.shouldLoadUnfinishedBusinessOptions() ||
		unfinishedBusiness.usesLocalizedArulcoMercData() ||
		unfinishedBusiness.usesArulcoMerchantRoster() ||
		unfinishedBusiness.hasMeanwhileScenes() ||
		!unfinishedBusiness.runsUnfinishedBusinessTacticalHooks() ||
		!unfinishedBusiness.usesUnfinishedBusinessUndergroundLoadScreens() ||
		unfinishedBusiness.shouldLoadExternalEnemyDeployment(false) ||
		!unfinishedBusiness.shouldLoadExternalEnemyDeployment(true))
		return 2;


	for (const bool editor : {false, true})
	{
		GameCapabilities arulcoHost;
		arulcoHost.editor = editor;
		const CampaignApplicationPolicy arulcoPolicy(arulcoHost);
		if (arulcoPolicy.shouldLoadUnfinishedBusinessOptions() ||
			TraceOptionsBootstrap(arulcoPolicy) !=
				Trace{"rebel-settings", "visibility-ranges"})
			return 3;

		GameCapabilities unfinishedBusinessHost;
		unfinishedBusinessHost.campaign =
			GameCampaign::UnfinishedBusiness;
		unfinishedBusinessHost.editor = editor;
		const CampaignApplicationPolicy unfinishedBusinessPolicy(
			unfinishedBusinessHost);
		if (!unfinishedBusinessPolicy.shouldLoadUnfinishedBusinessOptions() ||
			TraceOptionsBootstrap(unfinishedBusinessPolicy) !=
				Trace{"rebel-settings", "ub-options", "visibility-ranges"})
			return 4;
	}

	return 0;
}
