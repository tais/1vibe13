#ifndef JA2_CAMPAIGN_LUA_GLOBAL_POLICY_H
#define JA2_CAMPAIGN_LUA_GLOBAL_POLICY_H

#include "GameCapabilities.h"

#include <cstdint>

struct CampaignLuaDefaultArrivalSector
{
	std::uint8_t x;
	std::uint8_t y;
};

// Lua's global-setting adapter preserves the legacy per-campaign symbol
// surface while selecting it from the live campaign. Its original five named
// decisions mirror the former JA2UB branches in Luaglobal.cpp; the arrival
// decisions retain LuaInitNPCs' two legacy truth paths, and the callback
// decision keeps its three public registration groups on the same boundary.
class CampaignLuaGlobalPolicy
{
public:
	explicit constexpr CampaignLuaGlobalPolicy(GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignLuaGlobalPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignLuaGlobalPolicy(capabilities.campaign)
	{
	}

	constexpr bool exportsUnfinishedBusinessDifficultyAliases() const noexcept
	{
		return isUnfinishedBusiness();
	}

	constexpr bool usesUnfinishedBusinessArrivalGrid() const noexcept
	{
		return isUnfinishedBusiness();
	}

	constexpr bool exportsUnfinishedBusinessScenarioGlobals() const noexcept
	{
		return isUnfinishedBusiness();
	}

	constexpr bool exportsUnfinishedBusinessTestGlobal() const noexcept
	{
		return isUnfinishedBusiness();
	}

	constexpr bool exportsUnfinishedBusinessCharacterAndItemGlobals()
		const noexcept
	{
		return isUnfinishedBusiness();
	}

	constexpr bool registersUnfinishedBusinessCallbacks() const noexcept
	{
		return isUnfinishedBusiness();
	}

	constexpr bool mirrorsDefaultArrivalSectorToUnfinishedBusinessState()
		const noexcept
	{
		return isUnfinishedBusiness();
	}

	constexpr CampaignLuaDefaultArrivalSector invalidDefaultArrivalSector()
		const noexcept
	{
		return isUnfinishedBusiness()
			? CampaignLuaDefaultArrivalSector{7, 8}
			: CampaignLuaDefaultArrivalSector{9, 1};
	}

private:
	constexpr bool isUnfinishedBusiness() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	GameCampaign campaign_;
};

static_assert(!CampaignLuaGlobalPolicy(GameCampaign::Arulco)
	.exportsUnfinishedBusinessDifficultyAliases());
static_assert(CampaignLuaGlobalPolicy(GameCampaign::UnfinishedBusiness)
	.usesUnfinishedBusinessArrivalGrid());
static_assert(!CampaignLuaGlobalPolicy(GameCampaign::Arulco)
	.exportsUnfinishedBusinessScenarioGlobals());
static_assert(CampaignLuaGlobalPolicy(GameCampaign::UnfinishedBusiness)
	.exportsUnfinishedBusinessTestGlobal());
static_assert(!CampaignLuaGlobalPolicy(GameCampaign::Arulco)
	.exportsUnfinishedBusinessCharacterAndItemGlobals());
static_assert(!CampaignLuaGlobalPolicy(GameCampaign::Arulco)
	.registersUnfinishedBusinessCallbacks());
static_assert(CampaignLuaGlobalPolicy(GameCampaign::UnfinishedBusiness)
	.registersUnfinishedBusinessCallbacks());
static_assert(!CampaignLuaGlobalPolicy(static_cast<GameCampaign>(255))
	.registersUnfinishedBusinessCallbacks());
static_assert(!CampaignLuaGlobalPolicy(GameCampaign::Arulco)
	.mirrorsDefaultArrivalSectorToUnfinishedBusinessState());
static_assert(CampaignLuaGlobalPolicy(GameCampaign::UnfinishedBusiness)
	.mirrorsDefaultArrivalSectorToUnfinishedBusinessState());
static_assert(CampaignLuaGlobalPolicy(GameCampaign::Arulco)
	.invalidDefaultArrivalSector().x == 9);
static_assert(CampaignLuaGlobalPolicy(GameCampaign::UnfinishedBusiness)
	.invalidDefaultArrivalSector().y == 8);

#endif
