#include "CampaignStrategicContentPolicy.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
	constexpr std::array<CampaignStrategicContentEffect, 6> AllEffects{
		CampaignStrategicContentEffect::FirstBattleTownLoyalty,
		CampaignStrategicContentEffect::CreatureReleaseMeanwhile,
		CampaignStrategicContentEffect::CreatureMeanwhileReset,
		CampaignStrategicContentEffect::EnricoProgressEmails,
		CampaignStrategicContentEffect::ContinueMilitiaTrainingDialogue,
		CampaignStrategicContentEffect::SpeckEmployeeDeathReaction};

	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	struct TraceProbe
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

	struct TraceEffect
	{
		const char* name;
		std::vector<std::string>& trace;

		void operator()()
		{
			trace.emplace_back(name);
		}
	};

	template <typename CommonFactEffect, typename CommonSectorEffect,
		typename LoyaltyEffect>
	void CompleteFirstBattle(
		const CampaignStrategicContentPolicy& policy,
		CommonFactEffect&& commonFact,
		CommonSectorEffect&& commonSector,
		LoyaltyEffect&& loyalty)
	{
		commonFact();
		commonSector();
		if (policy.handlesFirstBattleTownLoyalty())
			loyalty();
	}

	template <typename PlayMeanwhileProbe, typename ScenePlayedProbe,
		typename OptionProbe, typename ReleaseEffect, typename PublishEffect>
	bool TryCreatureRelease(
		const CampaignStrategicContentPolicy& policy,
		PlayMeanwhileProbe&& playMeanwhile,
		ScenePlayedProbe&& scenePlayed,
		OptionProbe&& optionEnabled,
		ReleaseEffect&& release,
		PublishEffect&& publishPlayed)
	{
		if (policy.playsCreatureReleaseMeanwhile() &&
			playMeanwhile() && !scenePlayed() && optionEnabled())
		{
			release();
			publishPlayed();
			return true;
		}
		return false;
	}

	template <typename LairResetEffect, typename SceneResetEffect,
		typename FlagResetEffect>
	void ResetCreatureQuest(
		const CampaignStrategicContentPolicy& policy,
		LairResetEffect&& resetLair,
		SceneResetEffect&& resetScene,
		FlagResetEffect&& resetFlag)
	{
		resetLair();
		if (policy.resetsCreatureMeanwhileState())
		{
			resetScene();
			resetFlag();
		}
	}

	template <typename CurrentProgressProbe, typename HighestProgressProbe,
		typename EmailCycleProbe, typename DailyResetEffect>
	void RunEnricoDailyUpdate(
		const CampaignStrategicContentPolicy& policy,
		CurrentProgressProbe&& currentProgress,
		HighestProgressProbe&& highestProgress,
		EmailCycleProbe&& emailCycleStopsFunction,
		DailyResetEffect&& dailyReset)
	{
		(void)currentProgress();
		(void)highestProgress();
		if (policy.runsEnricoProgressEmailCycle() &&
			emailCycleStopsFunction())
		{
			return;
		}
		dailyReset();
	}

	template <typename ActiveProbe, typename PublicationEffect,
		typename DialogueEffect>
	void ContinueMilitiaTraining(
		const CampaignStrategicContentPolicy& policy,
		ActiveProbe&& active,
		PublicationEffect&& publishContinuation,
		DialogueEffect&& dialogue)
	{
		if (!active()) return;
		publishContinuation();
		if (policy.promptsContinuedMilitiaTraining())
			dialogue();
	}

	template <typename BuddyCommentEffect, typename SpeckProfileProbe,
		typename MercEmploymentProbe, typename SpeckEffect>
	void HandleEmployeeDeathComment(
		const CampaignStrategicContentPolicy& policy,
		BuddyCommentEffect&& buddyComment,
		SpeckProfileProbe&& isSpeck,
		MercEmploymentProbe&& isMercEmployee,
		SpeckEffect&& speckReaction)
	{
		buddyComment();
		if (policy.notifiesSpeckOfEmployeeDeath() &&
			isSpeck() && isMercEmployee())
		{
			speckReaction();
		}
	}
}

int main()
{
	Check(AllEffects.size() == static_cast<std::size_t>(
		CampaignStrategicContentEffect::Count),
		"all six strategic content effects are classified exactly once");
	for (std::size_t index = 0; index < AllEffects.size(); ++index)
	{
		for (std::size_t other = index + 1; other < AllEffects.size(); ++other)
		{
			Check(AllEffects[index] != AllEffects[other],
				"all six strategic content effects are classified exactly once");
		}
	}

	for (const bool editor : {false, true})
	{
		const CampaignStrategicContentPolicy arulco(
			GameCapabilities{GameCampaign::Arulco, editor});
		const CampaignStrategicContentPolicy unfinishedBusiness(
			GameCapabilities{GameCampaign::UnfinishedBusiness, editor});
		for (const CampaignStrategicContentEffect effect : AllEffects)
		{
			Check(arulco.owns(effect) &&
				!unfinishedBusiness.owns(effect),
				"UB short-circuits every Arulco-only strategic content effect");
		}
		Check(arulco.handlesFirstBattleTownLoyalty() &&
			arulco.playsCreatureReleaseMeanwhile() &&
			arulco.resetsCreatureMeanwhileState() &&
			arulco.runsEnricoProgressEmailCycle() &&
			arulco.promptsContinuedMilitiaTraining() &&
			arulco.notifiesSpeckOfEmployeeDeath() &&
			!unfinishedBusiness.handlesFirstBattleTownLoyalty() &&
			!unfinishedBusiness.playsCreatureReleaseMeanwhile() &&
			!unfinishedBusiness.resetsCreatureMeanwhileState() &&
			!unfinishedBusiness.runsEnricoProgressEmailCycle() &&
			!unfinishedBusiness.promptsContinuedMilitiaTraining() &&
			!unfinishedBusiness.notifiesSpeckOfEmployeeDeath(),
			"editor capability does not change strategic content ownership");
	}

	const CampaignStrategicContentPolicy arulco(GameCampaign::Arulco);
	const CampaignStrategicContentPolicy unfinishedBusiness(
		GameCampaign::UnfinishedBusiness);
	const auto unknownEffect = static_cast<CampaignStrategicContentEffect>(
		static_cast<std::uint8_t>(CampaignStrategicContentEffect::Count) + 1);
	Check(!arulco.owns(CampaignStrategicContentEffect::Count) &&
		!unfinishedBusiness.owns(CampaignStrategicContentEffect::Count) &&
		!arulco.owns(unknownEffect) &&
		!unfinishedBusiness.owns(unknownEffect),
		"unknown strategic content effects are unavailable");
	const auto unknownCampaign = static_cast<GameCampaign>(
		static_cast<std::underlying_type_t<GameCampaign>>(
			GameCampaign::UnfinishedBusiness) + 1);
	for (const bool editor : {false, true})
	{
		const CampaignStrategicContentPolicy unknown(
			GameCapabilities{unknownCampaign, editor});
		for (const CampaignStrategicContentEffect effect : AllEffects)
		{
			Check(!unknown.owns(effect),
				"unknown campaigns own no strategic content");
		}
	}
	Check(std::is_trivially_copyable<CampaignStrategicContentPolicy>::value &&
		sizeof(CampaignStrategicContentPolicy) == sizeof(GameCampaign),
		"strategic content policy remains a single trivially copyable campaign value");

	for (const CampaignStrategicContentPolicy* policy :
			{&arulco, &unfinishedBusiness})
	{
		std::vector<std::string> trace;
		CompleteFirstBattle(*policy,
			TraceEffect{"fact", trace}, TraceEffect{"sector", trace},
			TraceEffect{"loyalty", trace});
		const std::vector<std::string> expected = policy == &arulco
			? std::vector<std::string>{"fact", "sector", "loyalty"}
			: std::vector<std::string>{"fact", "sector"};
		Check(trace == expected,
			"first-battle common effects precede optional town loyalty");
	}

	for (const CampaignStrategicContentPolicy* policy :
			{&arulco, &unfinishedBusiness})
	{
		for (const bool playMeanwhile : {false, true})
		for (const bool scenePlayed : {false, true})
		for (const bool optionEnabled : {false, true})
		{
			std::vector<std::string> trace;
			const bool released = TryCreatureRelease(*policy,
				TraceProbe{"meanwhile", playMeanwhile, trace},
				TraceProbe{"scene", scenePlayed, trace},
				TraceProbe{"option", optionEnabled, trace},
				TraceEffect{"release", trace},
				TraceEffect{"publish", trace});
			const bool expectedRelease = policy == &arulco &&
				playMeanwhile && !scenePlayed && optionEnabled;
			Check(released == expectedRelease,
				"creature release truth table remains exact");
			std::vector<std::string> expectedTrace;
			if (policy == &arulco)
			{
				expectedTrace.emplace_back("meanwhile");
				if (playMeanwhile)
				{
					expectedTrace.emplace_back("scene");
					if (!scenePlayed)
					{
						expectedTrace.emplace_back("option");
						if (optionEnabled)
						{
							expectedTrace.emplace_back("release");
							expectedTrace.emplace_back("publish");
						}
					}
				}
			}
			Check(trace == expectedTrace,
				"creature release keeps campaign, meanwhile, scene, option, effect, then publication order");
		}
	}

	for (const CampaignStrategicContentPolicy* policy :
			{&arulco, &unfinishedBusiness})
	{
		std::vector<std::string> trace;
		ResetCreatureQuest(*policy,
			TraceEffect{"lair", trace}, TraceEffect{"scene", trace},
			TraceEffect{"flag", trace});
		const std::vector<std::string> expected = policy == &arulco
			? std::vector<std::string>{"lair", "scene", "flag"}
			: std::vector<std::string>{"lair"};
		Check(trace == expected,
			"creature reset keeps lair reset before Arulco meanwhile state");
	}

	for (const CampaignStrategicContentPolicy* policy :
			{&arulco, &unfinishedBusiness})
	for (const bool emailStopsFunction : {false, true})
	{
		std::vector<std::string> trace;
		RunEnricoDailyUpdate(*policy,
			TraceProbe{"current", true, trace},
			TraceProbe{"highest", true, trace},
			TraceProbe{"email", emailStopsFunction, trace},
			TraceEffect{"reset", trace});
		std::vector<std::string> expected{"current", "highest"};
		if (policy == &arulco)
		{
			expected.emplace_back("email");
			if (!emailStopsFunction) expected.emplace_back("reset");
		}
		else
		{
			expected.emplace_back("reset");
		}
		Check(trace == expected,
			"Enrico progress probes remain common and campaign email returns still precede daily reset");
	}

	for (const CampaignStrategicContentPolicy* policy :
			{&arulco, &unfinishedBusiness})
	for (const bool active : {false, true})
	{
		std::vector<std::string> trace;
		ContinueMilitiaTraining(*policy,
			TraceProbe{"active", active, trace},
			TraceEffect{"publish", trace}, TraceEffect{"dialogue", trace});
		std::vector<std::string> expected{"active"};
		if (active)
		{
			expected.emplace_back("publish");
			if (policy == &arulco) expected.emplace_back("dialogue");
		}
		Check(trace == expected,
			"militia continuation publication precedes its optional dialogue");
	}

	for (const CampaignStrategicContentPolicy* policy :
			{&arulco, &unfinishedBusiness})
	for (const bool isSpeck : {false, true})
	for (const bool isMercEmployee : {false, true})
	{
		std::vector<std::string> trace;
		HandleEmployeeDeathComment(*policy,
			TraceEffect{"buddy", trace},
			TraceProbe{"profile", isSpeck, trace},
			TraceProbe{"employment", isMercEmployee, trace},
			TraceEffect{"speck", trace});
		std::vector<std::string> expected{"buddy"};
		if (policy == &arulco)
		{
			expected.emplace_back("profile");
			if (isSpeck)
			{
				expected.emplace_back("employment");
				if (isMercEmployee) expected.emplace_back("speck");
			}
		}
		Check(trace == expected,
			"Speck reaction stays after buddy comments and left-gates profile and employment probes");
	}

	std::cout << "campaign strategic-content policy tests passed\n";
	return 0;
}
