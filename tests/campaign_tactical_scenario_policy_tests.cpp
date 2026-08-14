#include "CampaignTacticalScenarioPolicy.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>

namespace
{
	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	CampaignTacticalScenarioContent SentinelContent()
	{
		CampaignTacticalScenarioContent content;
		content.fanSector = {101, 102, 103};
		content.fanGrids.values = {
			1101, 1102, 1103, 1104, 1105, 1106, 1107, 1108, 1109};
		content.missileLaunchSector = {201, 202, 203};
		content.tunnelExplosionSectors = {301, {302, 303}};
		content.fortifiedDoorSector = {401, 402, 403};
		content.mine.surfaceSector = {501, 502, 503};
		content.mine.entranceGrid = 5101;
		content.mine.collapsedEntranceGrid = 5102;
		content.mine.surfaceExitGrids.values = {5103, 5104};
		content.mine.undergroundSector = {601, 602, 603};
		content.mine.undergroundEntranceGrids.values = {6101, 6102};
		return content;
	}

	template<typename ContentProbe>
	CampaignTacticalScenarioPolicy::PowerGeneratorSwitchDecision RouteSwitch(
		const CampaignTacticalScenarioPolicy& policy,
		const CampaignTacticalSector& currentSector,
		ContentProbe&& readContent)
	{
		if (!policy.usesUnfinishedBusinessScenario())
			return CampaignTacticalScenarioPolicy::
				PowerGeneratorSwitchDecision::None;
		return policy.powerGeneratorSwitchDecision(
			readContent(), currentSector);
	}

	template<typename OptionProbe, typename Effect>
	void RouteTunnelEnemyAddition(
		const CampaignTacticalScenarioPolicy& policy,
		OptionProbe&& readLiveOption,
		Effect&& addEnemies)
	{
		if (!policy.usesUnfinishedBusinessScenario()) return;
		if (!policy.shouldAddEnemiesToTunnelMaps(readLiveOption())) return;
		addEnemies();
	}
}

int main()
{
	static_assert(std::is_same_v<
		decltype(CampaignTacticalSector{}.x), std::uint32_t>);

	using Decision =
		CampaignTacticalScenarioPolicy::PowerGeneratorSwitchDecision;
	const CampaignTacticalScenarioContent content = SentinelContent();
	const CampaignTacticalScenarioPolicy arulco(GameCampaign::Arulco);
	const CampaignTacticalScenarioPolicy unfinishedBusiness(
		GameCampaign::UnfinishedBusiness);

	Check(unfinishedBusiness.powerGeneratorSwitchDecision(
		content, content.fanSector) == Decision::InspectFan,
		"the typed fan sector selects fan inspection");
	Check(unfinishedBusiness.powerGeneratorSwitchDecision(
		content, content.missileLaunchSector) == Decision::LaunchMissiles,
		"the distinct typed missile sector selects missile launch");
	Check(unfinishedBusiness.powerGeneratorSwitchDecision(
		content, {901, 902, 903}) == Decision::None,
		"an unrelated sector selects no power-generator action");

	CampaignTacticalScenarioContent unsignedDomainContent = content;
	const std::uint32_t unsignedSentinel =
		std::numeric_limits<std::uint32_t>::max();
	unsignedDomainContent.fanSector = {
		unsignedSentinel, unsignedSentinel, unsignedSentinel};
	const CampaignTacticalSector projectedNegativeWorldSector{
		static_cast<std::uint32_t>(std::int16_t{-1}),
		static_cast<std::uint32_t>(std::int16_t{-1}),
		static_cast<std::uint32_t>(std::int8_t{-1})};
	Check(unfinishedBusiness.powerGeneratorSwitchDecision(
		unsignedDomainContent, projectedNegativeWorldSector) ==
			Decision::InspectFan,
		"signed negative world sentinels retain legacy unsigned comparison behavior");

	for (const std::uint32_t fanGrid : content.fanGrids.values)
	{
		Check(unfinishedBusiness.isFanGraphic(
			content, content.fanSector, fanGrid),
			"each of the nine authored fan grids is retained");
	}
	Check(!unfinishedBusiness.isFanGraphic(
		content, content.fanSector, 1199),
		"a nonmember grid is not treated as fan geometry");
	Check(!unfinishedBusiness.isFanGraphic(
		content, content.missileLaunchSector, content.fanGrids[0]),
		"fan membership is scoped to the typed fan sector");

	Check(unfinishedBusiness.recordsTunnelExplosion(
		content, {301, 302, 1}) &&
		unfinishedBusiness.recordsTunnelExplosion(
			content, {301, 303, 1}),
		"both authored tunnel Y alternatives retain hard-coded depth one");
	Check(!unfinishedBusiness.recordsTunnelExplosion(
		content, {301, 302, 0}) &&
		!unfinishedBusiness.recordsTunnelExplosion(
			content, {301, 303, 2}),
		"the tunnel story flag rejects every depth other than one");
	Check(!unfinishedBusiness.recordsTunnelExplosion(
		content, {300, 302, 1}),
		"the tunnel story flag retains its authored X coordinate");

	Check(unfinishedBusiness.isFortifiedDoorSector(
		content, content.fortifiedDoorSector) &&
		!unfinishedBusiness.isFortifiedDoorSector(
			content, {401, 402, 404}),
		"fortified-door dialogue uses the complete typed sector");
	Check(unfinishedBusiness.isMineEntrance(
		content, content.mine.surfaceSector, content.mine.entranceGrid) &&
		!unfinishedBusiness.isMineEntrance(
			content, content.mine.surfaceSector, 5199) &&
		!unfinishedBusiness.isMineEntrance(
			content, content.mine.undergroundSector,
			content.mine.entranceGrid),
		"mine entrance recognition requires its surface sector and grid");
	Check(content.mine.collapsedEntranceGrid == 5102 &&
		content.mine.surfaceExitGrids[0] == 5103 &&
		content.mine.surfaceExitGrids[1] == 5104 &&
		content.mine.undergroundSector ==
			CampaignTacticalSector{601, 602, 603} &&
		content.mine.undergroundEntranceGrids[0] == 6101 &&
		content.mine.undergroundEntranceGrids[1] == 6102,
		"mine replacement retains distinct surface and underground targets");

	int arulcoContentReads = 0;
	Check(RouteSwitch(arulco, content.fanSector, [&]() {
		++arulcoContentReads;
		return content;
	}) == Decision::None && arulcoContentReads == 0,
		"Arulco selects no scenario action without reading UB content");

	int liveOptionReads = 0;
	int enemyAdditions = 0;
	bool liveOption = false;
	RouteTunnelEnemyAddition(unfinishedBusiness, [&]() {
		++liveOptionReads;
		return liveOption;
	}, [&]() { ++enemyAdditions; });
	liveOption = true;
	RouteTunnelEnemyAddition(unfinishedBusiness, [&]() {
		++liveOptionReads;
		return liveOption;
	}, [&]() { ++enemyAdditions; });
	Check(liveOptionReads == 2 && enemyAdditions == 1,
		"each tunnel-enemy decision samples the current false/true option state");

	liveOptionReads = 0;
	enemyAdditions = 0;
	RouteTunnelEnemyAddition(arulco, [&]() {
		++liveOptionReads;
		return true;
	}, [&]() { ++enemyAdditions; });
	Check(liveOptionReads == 0 && enemyAdditions == 0,
		"Arulco neither reads the UB option nor touches the tunnel effect");

	Check(arulco.powerGeneratorSwitchDecision(
			content, content.fanSector) == Decision::None &&
		!arulco.isFanGraphic(content, content.fanSector,
			content.fanGrids[0]) &&
		!arulco.recordsTunnelExplosion(content, {301, 302, 1}) &&
		!arulco.isFortifiedDoorSector(content, content.fortifiedDoorSector) &&
		!arulco.isMineEntrance(content, content.mine.surfaceSector,
			content.mine.entranceGrid),
		"all typed tactical-scenario decisions remain disabled in Arulco");

	std::cout << "campaign tactical-scenario policy tests passed\n";
	return 0;
}
