#include "CampaignStrategicSectorScriptPolicy.h"

#include <cstdlib>
#include <iostream>
#include <type_traits>

namespace
{
	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	CampaignStrategicSectorScriptContent SentinelContent()
	{
		CampaignStrategicSectorScriptContent content;
		content.fanSector = {101, 102, 103};
		content.missileLaunchSector = {201, 202, 203};
		content.fortifiedDoorSector = {301, 302, 303};
		content.firstTunnelSector = {401, 402, 403};
		content.gateTunnelSector = {501, 502, 503};
		content.guardPostSector = {601, 602, 603};
		content.firstTownSector = {701, 702, 703};
		content.town2Sector = {801, 802, 803};
		content.town3Sector = {901, 902, 903};
		content.i9QuoteSector = {1001, 1002, 0};
		content.h10QuoteSector = {1101, 1102, 0};
		content.guardPostMoney = {1201, 1202, 1203, 1204};
		content.firstTownMoney = {{
			{1301, 1302, 1303, 1304},
			{1401, 1402, 1403, 1404}
		}};
		content.fortifiedDoorGrid = 1501;
		content.town2RoofMoves = {{{1601, 1602}, {1603, 1603}}};
		content.town3RoofMoves = {{{1701, 1701}}};
		return content;
	}

	template<typename ContentProbe>
	CampaignStrategicSectorScriptPolicy::UnloadAction RouteUnload(
		const CampaignStrategicSectorScriptPolicy& policy,
		const CampaignTacticalSector& sector,
		ContentProbe&& readContent)
	{
		if (!policy.usesUnfinishedBusinessSectorScript())
			return CampaignStrategicSectorScriptPolicy::UnloadAction::None;
		return policy.unloadAction(readContent(), sector);
	}

	template<typename QuestProbe, typename FixedProbe,
		typename LaptopProbe, typename ContentProbe>
	void RouteEmailInvocation(
		const CampaignStrategicSectorScriptPolicy& policy,
		QuestProbe&& questIncomplete,
		FixedProbe&& laptopJustFixed,
		LaptopProbe&& laptopQuestEnabled,
		ContentProbe&& readContent)
	{
		if (!policy.usesUnfinishedBusinessSectorScript()) return;
		if (questIncomplete() && !laptopJustFixed() &&
			laptopQuestEnabled()) return;
		readContent();
	}
}

int main()
{
	using Policy = CampaignStrategicSectorScriptPolicy;
	using Unload = Policy::UnloadAction;
	using Saved = Policy::SavedMapAction;
	using Fresh = Policy::FreshMapAction;
	using Roof = Policy::RoofAction;

	static_assert(std::is_trivially_copyable_v<
		CampaignStrategicSectorScriptContent>);
	static_assert(std::is_same_v<
		decltype(CampaignStrategicMoneyDrop{}.grid), std::uint32_t>);

	const CampaignStrategicSectorScriptContent content = SentinelContent();
	const Policy arulco(GameCampaign::Arulco);
	const Policy unfinishedBusiness(GameCampaign::UnfinishedBusiness);

	const auto quoteSectors = content.playerQuoteSectors();
	Check(quoteSectors[0] == content.guardPostSector &&
		quoteSectors[1] == content.i9QuoteSector &&
		quoteSectors[2] == content.h10QuoteSector &&
		quoteSectors[3] == content.firstTownSector &&
		quoteSectors[4] == content.fanSector,
		"the six quote entries retain guard, I9, H10, town, then fan ordering");
	Check(quoteSectors[5] == CampaignTacticalSector{
			content.guardPostSector.x, content.guardPostSector.y,
			content.firstTunnelSector.z} &&
		quoteSectors[5] != content.firstTunnelSector,
		"the final quote entry retains guard-post X/Y and tunnel-Z fallback");

	Check(unfinishedBusiness.isConfiguredTownEmailSector(
			content, {content.town2Sector.x, content.town2Sector.y, 999}),
		"the configured town email trigger deliberately ignores depth");
	Check(unfinishedBusiness.isLegacyTownEmailFallbackSector({12, 9, 0}) &&
		!unfinishedBusiness.isLegacyTownEmailFallbackSector({12, 9, 1}) &&
		!unfinishedBusiness.isLegacyTownEmailFallbackSector({11, 9, 0}),
		"the literal 12,9,0 email fallback remains exact");
	Check(unfinishedBusiness.isFanSector(content, content.fanSector) &&
		unfinishedBusiness.isFirstTunnelSector(
			content, content.firstTunnelSector),
		"email replay retains complete fan and first-tunnel sectors");
	Check(unfinishedBusiness.isManuelPlacementSector(
			content, content.i9QuoteSector) &&
		unfinishedBusiness.isManuelPlacementSector(
			content, content.h10QuoteSector) &&
		!unfinishedBusiness.isManuelPlacementSector(
			content, content.firstTownSector),
		"Manuel placement retains H10-before-I9 alternatives without broadening");

	Check(unfinishedBusiness.unloadAction(
			content, content.fanSector) == Unload::PowerGenerator &&
		unfinishedBusiness.unloadAction(
			content, content.firstTunnelSector) == Unload::FirstTunnel &&
		unfinishedBusiness.unloadAction(
			content, content.guardPostSector) == Unload::None,
		"unload routing remains power generator, first tunnel, then none");

	Check(unfinishedBusiness.savedMapAction(
			content, content.fanSector) == Saved::PowerGenerator &&
		unfinishedBusiness.savedMapAction(
			content, content.firstTunnelSector) == Saved::FirstTunnel &&
		unfinishedBusiness.savedMapAction(
			content, content.missileLaunchSector) == Saved::MissileControl &&
		unfinishedBusiness.savedMapAction(
			content, content.guardPostSector) == Saved::None,
		"saved-map routing retains fan, tunnel, missile, then none order");

	Check(unfinishedBusiness.freshMapAction(
			content, content.guardPostSector, true) == Fresh::DefaultArrival,
		"the default arrival empty branch still shadows sector scripts");
	Check(unfinishedBusiness.freshMapAction(
			content, content.guardPostSector, false) == Fresh::GuardPostMoney,
		"the guard-post money action retains first campaign priority");
	Check(unfinishedBusiness.freshMapAction(
			content,
			{content.firstTownSector.x, content.firstTownSector.y,
			 content.firstTownSector.x}, false) == Fresh::FirstTownMoney &&
		unfinishedBusiness.freshMapAction(
			content, content.firstTownSector, false) == Fresh::None,
		"the first-town money trigger preserves the legacy X-as-Z bug");
	Check(unfinishedBusiness.freshMapAction(
			content, content.fanSector, false) == Fresh::PowerGenerator &&
		unfinishedBusiness.freshMapAction(
			content, content.firstTunnelSector, false) == Fresh::FirstTunnel &&
		unfinishedBusiness.freshMapAction(
			content, content.gateTunnelSector, false) == Fresh::GateTunnel &&
		unfinishedBusiness.freshMapAction(
			content, content.fortifiedDoorSector, false) ==
				Fresh::FortifiedDoor &&
		unfinishedBusiness.freshMapAction(
			content, content.missileLaunchSector, false) ==
				Fresh::MissileControl &&
		unfinishedBusiness.freshMapAction(
			content, {1801, 1802, 1803}, false) == Fresh::None,
		"fresh-map routing exhaustively retains fan, tunnels, door, missile, and none");

	Check(unfinishedBusiness.roofAction(
			content, content.town2Sector) == Roof::Town2 &&
		unfinishedBusiness.roofAction(
			content, content.town3Sector) == Roof::Town3 &&
		unfinishedBusiness.roofAction(
			content, content.guardPostSector) == Roof::None,
		"roof routing retains town2 before town3 and unrelated none");
	Check(content.town2RoofMoves[0] == CampaignStrategicGridMove{1601, 1602} &&
		content.town2RoofMoves[1] == CampaignStrategicGridMove{1603, 1603} &&
		content.town3RoofMoves[0] == CampaignStrategicGridMove{1701, 1701},
		"roof moves retain 1a-to-1b plus the 2a-to-2a and 3a-to-3a aliases");
	Check(content.guardPostMoney.grid == 1201 &&
		content.guardPostMoney.easyAmount == 1202 &&
		content.guardPostMoney.mediumAmount == 1203 &&
		content.guardPostMoney.hardAmount == 1204 &&
		content.firstTownMoney[0].grid == 1301 &&
		content.firstTownMoney[1].grid == 1401 &&
		content.fortifiedDoorGrid == 1501,
		"money difficulty fields, two-drop order, and fortified-door grid remain distinct");

	int contentReads = 0;
	Check(RouteUnload(arulco, content.fanSector, [&]() {
		++contentReads;
		return content;
	}) == Unload::None && contentReads == 0,
		"Arulco left-gates the sector script before reading content");

	int emailOrder = 0;
	RouteEmailInvocation(unfinishedBusiness,
		[&]() { Check(++emailOrder == 1, "quest probe order"); return true; },
		[&]() { Check(++emailOrder == 2, "fixed probe order"); return false; },
		[&]() { Check(++emailOrder == 3, "live laptop probe order"); return false; },
		[&]() { Check(++emailOrder == 4, "snapshot probe order"); });
	Check(emailOrder == 4,
		"email routing keeps quest, fixed, live laptop, then snapshot order");
	emailOrder = 0;
	RouteEmailInvocation(unfinishedBusiness,
		[&]() { ++emailOrder; return true; },
		[&]() { ++emailOrder; return false; },
		[&]() { ++emailOrder; return true; },
		[&]() { emailOrder += 100; });
	Check(emailOrder == 3,
		"an enabled live laptop quest suppresses the immutable content snapshot");
	emailOrder = 0;
	RouteEmailInvocation(arulco,
		[&]() { ++emailOrder; return true; },
		[&]() { ++emailOrder; return false; },
		[&]() { ++emailOrder; return true; },
		[&]() { ++emailOrder; });
	Check(emailOrder == 0,
		"Arulco evaluates neither laptop state nor strategic content");

	Check(arulco.unloadAction(content, content.fanSector) == Unload::None &&
		arulco.savedMapAction(content, content.fanSector) == Saved::None &&
		arulco.freshMapAction(
			content, content.guardPostSector, false) == Fresh::None &&
		arulco.roofAction(content, content.town2Sector) == Roof::None &&
		!arulco.isConfiguredTownEmailSector(content, content.town2Sector) &&
		!arulco.isLegacyTownEmailFallbackSector({12, 9, 0}) &&
		!arulco.isManuelPlacementSector(content, content.i9QuoteSector),
		"every strategic sector-script decision is disabled in Arulco");

	std::cout << "campaign strategic sector-script policy tests passed\n";
	return 0;
}
