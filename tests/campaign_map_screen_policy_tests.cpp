#include "CampaignMapScreenPolicy.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	struct BoolProbe
	{
		const char* name;
		bool value;
		std::vector<std::string>& trace;

		bool operator()()
		{
			trace.emplace_back(name);
			return value;
		}
	};

	template <typename PendingMeanwhileProbe,
		typename ArrivalSectorProbe, typename JerryPermissionProbe>
	bool AllowsTimeCompressionAfterGenericChecks(
		const CampaignMapScreenPolicy& policy,
		PendingMeanwhileProbe&& pendingMeanwhile,
		ArrivalSectorProbe&& hasEnteredArrivalSector,
		JerryPermissionProbe&& jerryAllowsCompression)
	{
		if (policy.hasMeanwhileScenes() && pendingMeanwhile())
			return false;
		if (policy.usesJerryMiloGuidance() &&
			!hasEnteredArrivalSector())
		{
			return jerryAllowsCompression();
		}
		return true;
	}

	template <typename PendingMeanwhileProbe>
	bool AllowsExitAfterDialogueChecks(
		const CampaignMapScreenPolicy& policy,
		bool introDestination, bool mainMenuDestination,
		PendingMeanwhileProbe&& pendingMeanwhile)
	{
		if (introDestination && !policy.allowsIntroScreenExit())
			return false;
		if (mainMenuDestination)
			return true;
		return !policy.hasMeanwhileScenes() || !pendingMeanwhile();
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
	Check(!arulco.allowsIntroScreenExit() &&
		unfinishedBusiness.allowsIntroScreenExit(),
		"only UB permits the intro-screen exit");
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

	for (const bool pendingMeanwhile : {false, true})
	{
		for (const bool hasEnteredArrivalSector : {false, true})
		{
			for (const bool jerryAllowsCompression : {false, true})
			{
				std::vector<std::string> arulcoTrace;
				const bool arulcoAllows =
					AllowsTimeCompressionAfterGenericChecks(
						arulco,
						BoolProbe{"pending", pendingMeanwhile,
							arulcoTrace},
						BoolProbe{"entered", hasEnteredArrivalSector,
							arulcoTrace},
						BoolProbe{"jerry", jerryAllowsCompression,
							arulcoTrace});
				Check(arulcoAllows == !pendingMeanwhile &&
					arulcoTrace == std::vector<std::string>{"pending"},
					"Arulco pending meanwhile gate runs before map guidance");
				Check(arulcoTrace.size() == 1,
					"Arulco map availability never probes UB arrival or Jerry state");

				std::vector<std::string> unfinishedBusinessTrace;
				const bool unfinishedBusinessAllows =
					AllowsTimeCompressionAfterGenericChecks(
						unfinishedBusiness,
						BoolProbe{"pending", pendingMeanwhile,
							unfinishedBusinessTrace},
						BoolProbe{"entered", hasEnteredArrivalSector,
							unfinishedBusinessTrace},
						BoolProbe{"jerry", jerryAllowsCompression,
							unfinishedBusinessTrace});
				const bool expectedUnfinishedBusinessResult =
					hasEnteredArrivalSector || jerryAllowsCompression;
				const std::vector<std::string> expectedUnfinishedBusinessTrace =
					hasEnteredArrivalSector
						? std::vector<std::string>{"entered"}
						: std::vector<std::string>{"entered", "jerry"};
				Check(unfinishedBusinessAllows ==
					expectedUnfinishedBusinessResult,
					"UB follows entered-sector state before Jerry permission");
				Check(unfinishedBusinessTrace ==
					expectedUnfinishedBusinessTrace,
					"UB probes arrival before Jerry and skips Jerry after entry");
				Check(unfinishedBusinessTrace.front() != "pending",
					"UB map availability never probes Arulco meanwhile state");
			}
		}
	}

	for (const bool pendingMeanwhile : {false, true})
	{
		std::vector<std::string> arulcoIntroTrace;
		Check(!AllowsExitAfterDialogueChecks(
			arulco, true, false,
			BoolProbe{"pending", pendingMeanwhile, arulcoIntroTrace}) &&
			arulcoIntroTrace.empty(),
			"Arulco rejects intro exit before pending meanwhile state");

		std::vector<std::string> arulcoMainMenuTrace;
		Check(AllowsExitAfterDialogueChecks(
			arulco, false, true,
			BoolProbe{"pending", pendingMeanwhile, arulcoMainMenuTrace}) &&
			arulcoMainMenuTrace.empty(),
			"main-menu exit stays ahead of pending meanwhile state");

		std::vector<std::string> arulcoOrdinaryTrace;
		Check(AllowsExitAfterDialogueChecks(
			arulco, false, false,
			BoolProbe{"pending", pendingMeanwhile, arulcoOrdinaryTrace}) ==
			!pendingMeanwhile &&
			arulcoOrdinaryTrace == std::vector<std::string>{"pending"},
			"Arulco ordinary exit follows pending meanwhile state once");

		for (const bool introDestination : {false, true})
		{
			std::vector<std::string> unfinishedBusinessTrace;
			Check(AllowsExitAfterDialogueChecks(
				unfinishedBusiness, introDestination, false,
				BoolProbe{"pending", pendingMeanwhile,
					unfinishedBusinessTrace}) &&
				unfinishedBusinessTrace.empty(),
				"UB exits without probing Arulco pending meanwhile state");
		}
	}

	std::cout << "campaign map-screen policy tests passed\n";
	return 0;
}
