#ifndef JA2_CAMPAIGN_LUA_GLOBAL_POLICY_H
#define JA2_CAMPAIGN_LUA_GLOBAL_POLICY_H

#include "GameCapabilities.h"

// Lua's global-setting adapter preserves the legacy per-campaign symbol
// surface while selecting it from the live campaign. The five named decisions
// mirror the five former JA2UB compile-time branches in Luaglobal.cpp.
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

#endif
