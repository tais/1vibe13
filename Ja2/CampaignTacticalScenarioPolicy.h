#ifndef JA2_CAMPAIGN_TACTICAL_SCENARIO_POLICY_H
#define JA2_CAMPAIGN_TACTICAL_SCENARIO_POLICY_H

#include "CampaignTacticalScenarioContent.h"
#include "GameCapabilities.h"

#include <cstdint>

class CampaignTacticalScenarioPolicy
{
public:
	enum class PowerGeneratorSwitchDecision
	{
		None,
		InspectFan,
		LaunchMissiles
	};

	explicit constexpr CampaignTacticalScenarioPolicy(
		GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignTacticalScenarioPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignTacticalScenarioPolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessScenario() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	constexpr PowerGeneratorSwitchDecision powerGeneratorSwitchDecision(
		const CampaignTacticalScenarioContent& content,
		const CampaignTacticalSector& currentSector) const noexcept
	{
		if (!usesUnfinishedBusinessScenario())
			return PowerGeneratorSwitchDecision::None;
		if (currentSector == content.fanSector)
			return PowerGeneratorSwitchDecision::InspectFan;
		if (currentSector == content.missileLaunchSector)
			return PowerGeneratorSwitchDecision::LaunchMissiles;
		return PowerGeneratorSwitchDecision::None;
	}

	constexpr bool isFanGraphic(
		const CampaignTacticalScenarioContent& content,
		const CampaignTacticalSector& currentSector,
		std::uint32_t grid) const noexcept
	{
		return usesUnfinishedBusinessScenario() &&
			currentSector == content.fanSector &&
			content.fanGrids.contains(grid);
	}

	constexpr bool recordsTunnelExplosion(
		const CampaignTacticalScenarioContent& content,
		const CampaignTacticalSector& currentSector) const noexcept
	{
		return usesUnfinishedBusinessScenario() &&
			content.tunnelExplosionSectors.contains(currentSector);
	}

	constexpr bool isFortifiedDoorSector(
		const CampaignTacticalScenarioContent& content,
		const CampaignTacticalSector& currentSector) const noexcept
	{
		return usesUnfinishedBusinessScenario() &&
			currentSector == content.fortifiedDoorSector;
	}

	constexpr bool isMineEntrance(
		const CampaignTacticalScenarioContent& content,
		const CampaignTacticalSector& currentSector,
		std::uint32_t grid) const noexcept
	{
		return usesUnfinishedBusinessScenario() &&
			currentSector == content.mine.surfaceSector &&
			grid == content.mine.entranceGrid;
	}

	constexpr bool shouldAddEnemiesToTunnelMaps(
		bool liveOptionEnabled) const noexcept
	{
		return usesUnfinishedBusinessScenario() && liveOptionEnabled;
	}

private:
	GameCampaign campaign_;
};

static_assert(!CampaignTacticalScenarioPolicy(GameCampaign::Arulco)
	.usesUnfinishedBusinessScenario());
static_assert(CampaignTacticalScenarioPolicy(GameCampaign::UnfinishedBusiness)
	.usesUnfinishedBusinessScenario());
static_assert(!CampaignTacticalScenarioPolicy(GameCampaign::Arulco)
	.shouldAddEnemiesToTunnelMaps(true));
static_assert(CampaignTacticalScenarioPolicy(GameCampaign::UnfinishedBusiness)
	.shouldAddEnemiesToTunnelMaps(true));

#endif
