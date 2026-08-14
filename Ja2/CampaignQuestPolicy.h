#ifndef JA2_CAMPAIGN_QUEST_POLICY_H
#define JA2_CAMPAIGN_QUEST_POLICY_H

#include "GameCapabilities.h"

#include <cstdint>

// Quest/fact availability and rewards are campaign content, not executable
// identity. The policy owns values only: quest state, option reads, profiles,
// email effects, and strategic mutations remain in their established owners.
class CampaignQuestPolicy
{
public:
	enum class InitialQuest : std::uint8_t
	{
		DeliverLetter,
		DestroyMissiles
	};

	explicit constexpr CampaignQuestPolicy(GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignQuestPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignQuestPolicy(capabilities.campaign)
	{
	}

	// UB has no corresponding content for these fact cases. Preserve the
	// legacy behavior by leaving their existing gubFact values untouched.
	constexpr bool evaluatesArulcoFactRules() const noexcept
	{
		return campaign_ == GameCampaign::Arulco;
	}

	constexpr std::int8_t killDeidrannaReward() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness ? 4 : 25;
	}

	constexpr bool hasLaptopQuestCompletionEffects() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	constexpr InitialQuest initialQuest() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness
			? InitialQuest::DestroyMissiles
			: InitialQuest::DeliverLetter;
	}

	constexpr bool supportsPrisonerOfWarQuests() const noexcept
	{
		return campaign_ == GameCampaign::Arulco;
	}

private:
	GameCampaign campaign_;
};

static_assert(CampaignQuestPolicy(GameCampaign::Arulco)
	.evaluatesArulcoFactRules());
static_assert(!CampaignQuestPolicy(GameCampaign::UnfinishedBusiness)
	.evaluatesArulcoFactRules());
static_assert(CampaignQuestPolicy(GameCampaign::Arulco)
	.killDeidrannaReward() == 25);
static_assert(CampaignQuestPolicy(GameCampaign::UnfinishedBusiness)
	.killDeidrannaReward() == 4);
static_assert(CampaignQuestPolicy(GameCampaign::Arulco).initialQuest() ==
	CampaignQuestPolicy::InitialQuest::DeliverLetter);
static_assert(CampaignQuestPolicy(GameCampaign::UnfinishedBusiness)
	.initialQuest() == CampaignQuestPolicy::InitialQuest::DestroyMissiles);
static_assert(CampaignQuestPolicy(GameCampaign::Arulco)
	.supportsPrisonerOfWarQuests());
static_assert(!CampaignQuestPolicy(GameCampaign::UnfinishedBusiness)
	.supportsPrisonerOfWarQuests());

#endif
