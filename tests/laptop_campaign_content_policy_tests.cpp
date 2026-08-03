#include "CampaignLaptopContentPolicy.h"

#include <iostream>
#include <string_view>

namespace
{
int failures = 0;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	++failures;
}
}

int main()
{
	using Policy = CampaignLaptopContentPolicy;
	const Policy arulco(GameCampaign::Arulco);
	const Policy unfinishedBusiness(GameCampaign::UnfinishedBusiness);

	const auto arulcoBriefing = arulco.briefingCatalog(true);
	Check(std::string_view(arulcoBriefing.path) ==
		"BINARYDATA\\RIS.edt" && arulcoBriefing.recordCount == 68,
		"Arulco retains all 68 RIS.edt briefing records");

	const auto ubBriefing = unfinishedBusiness.briefingCatalog(true);
	Check(std::string_view(ubBriefing.path) ==
		"BINARYDATA\\RIS25.edt" && ubBriefing.recordCount == 39,
		"UB selects all 39 RIS25.edt briefing records");
	const auto ubBriefingFallback =
		unfinishedBusiness.briefingCatalog(false);
	Check(std::string_view(ubBriefingFallback.path) ==
		"BINARYDATA\\RIS.edt" && ubBriefingFallback.recordCount == 39,
		"missing RIS25.edt falls back to RIS.edt without changing UB pagination");

	Check(std::string_view(arulco.filesMapPath(true)) ==
		"LAPTOP\\ArucoFilesMap.sti",
		"Arulco ignores an installed UB map asset");
	Check(std::string_view(unfinishedBusiness.filesMapPath(true)) ==
		"LAPTOP\\TraconaMap.sti",
		"UB selects the installed Tracona map");
	Check(std::string_view(unfinishedBusiness.filesMapPath(false)) ==
		"LAPTOP\\ArucoFilesMap.sti",
		"missing Tracona map falls back to the Arulco map");

	Check(std::string_view(arulco.biographyPicturePath(4)) ==
		"LAPTOP\\Enrico_Y.sti" &&
		std::string_view(arulco.biographyPicturePath(5)) ==
			"LAPTOP\\Enrico_W.sti",
		"Arulco retains both Enrico biography pictures");
	Check(arulco.biographyPicturePath(3) == nullptr &&
		unfinishedBusiness.biographyPicturePath(4) == nullptr &&
		unfinishedBusiness.biographyPicturePath(5) == nullptr,
		"only Arulco biography pages 4 and 5 select extra artwork");

	const auto arulcoQuestStart = arulco.questTextRecord(7, false, true);
	const auto arulcoQuestEnd = arulco.questTextRecord(7, true, true);
	Check(std::string_view(arulcoQuestStart.path) ==
		"BINARYDATA\\quests.edt" && arulcoQuestStart.recordIndex == 14 &&
		arulcoQuestEnd.recordIndex == 15,
		"Arulco quest start/end select the paired even/odd records");

	const auto ubQuestEnd =
		unfinishedBusiness.questTextRecord(7, true, true);
	Check(std::string_view(ubQuestEnd.path) ==
		"BINARYDATA\\quests25.edt" && ubQuestEnd.recordIndex == 15,
		"UB selects the installed quest-completion record");
	const auto ubQuestEndFallback =
		unfinishedBusiness.questTextRecord(7, true, false);
	Check(std::string_view(ubQuestEndFallback.path) ==
		"BINARYDATA\\quests.edt" && ubQuestEndFallback.recordIndex == 15,
		"missing quests25.edt falls back to the completion record, not start");

	return failures == 0 ? 0 : 1;
}
