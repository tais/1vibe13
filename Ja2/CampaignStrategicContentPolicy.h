#ifndef JA2_CAMPAIGN_STRATEGIC_CONTENT_POLICY_H
#define JA2_CAMPAIGN_STRATEGIC_CONTENT_POLICY_H

#include "GameCapabilities.h"

#include <cstdint>

// Strategic systems retain ownership of their state and effects. This closed
// vocabulary identifies only the campaign-authored effects that were formerly
// selected by the host's JA2UB compile definition.
enum class CampaignStrategicContentEffect : std::uint8_t
{
	FirstBattleTownLoyalty,
	CreatureReleaseMeanwhile,
	CreatureMeanwhileReset,
	EnricoProgressEmails,
	ContinueMilitiaTrainingDialogue,
	SpeckEmployeeDeathReaction,
	Count
};

class CampaignStrategicContentPolicy
{
public:
	explicit constexpr CampaignStrategicContentPolicy(
		GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignStrategicContentPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignStrategicContentPolicy(capabilities.campaign)
	{
	}

	constexpr bool owns(CampaignStrategicContentEffect effect) const noexcept
	{
		switch (effect)
		{
		case CampaignStrategicContentEffect::FirstBattleTownLoyalty:
		case CampaignStrategicContentEffect::CreatureReleaseMeanwhile:
		case CampaignStrategicContentEffect::CreatureMeanwhileReset:
		case CampaignStrategicContentEffect::EnricoProgressEmails:
		case CampaignStrategicContentEffect::ContinueMilitiaTrainingDialogue:
		case CampaignStrategicContentEffect::SpeckEmployeeDeathReaction:
			return campaign_ == GameCampaign::Arulco;

		case CampaignStrategicContentEffect::Count:
			return false;
		}
		return false;
	}

	constexpr bool handlesFirstBattleTownLoyalty() const noexcept
	{
		return owns(
			CampaignStrategicContentEffect::FirstBattleTownLoyalty);
	}

	constexpr bool playsCreatureReleaseMeanwhile() const noexcept
	{
		return owns(
			CampaignStrategicContentEffect::CreatureReleaseMeanwhile);
	}

	constexpr bool resetsCreatureMeanwhileState() const noexcept
	{
		return owns(
			CampaignStrategicContentEffect::CreatureMeanwhileReset);
	}

	constexpr bool runsEnricoProgressEmailCycle() const noexcept
	{
		return owns(CampaignStrategicContentEffect::EnricoProgressEmails);
	}

	constexpr bool promptsContinuedMilitiaTraining() const noexcept
	{
		return owns(
			CampaignStrategicContentEffect::ContinueMilitiaTrainingDialogue);
	}

	constexpr bool notifiesSpeckOfEmployeeDeath() const noexcept
	{
		return owns(
			CampaignStrategicContentEffect::SpeckEmployeeDeathReaction);
	}

private:
	GameCampaign campaign_;
};

static_assert(static_cast<std::uint8_t>(
	CampaignStrategicContentEffect::Count) == 6);
static_assert(CampaignStrategicContentPolicy(GameCampaign::Arulco)
	.runsEnricoProgressEmailCycle());
static_assert(!CampaignStrategicContentPolicy(
	GameCampaign::UnfinishedBusiness).runsEnricoProgressEmailCycle());

#endif
