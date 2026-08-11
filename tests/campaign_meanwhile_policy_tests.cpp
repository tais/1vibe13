#include "CampaignApplicationPolicy.h"

#include <cstdlib>
#include <iostream>

namespace
{
	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	struct MeanwhileProbe
	{
		bool reportedActive = false;
		bool called = false;

		bool operator()()
		{
			called = true;
			return reportedActive;
		}
	};

	template <typename Probe>
	bool SceneActive(const CampaignApplicationPolicy& policy, Probe&& probe)
	{
		return policy.hasMeanwhileScenes() && probe();
	}

	template <typename Probe>
	bool AllowsMartialArtistIdle(const CampaignApplicationPolicy& policy,
		bool martialArtist, Probe&& probe)
	{
		return martialArtist &&
			(!policy.hasMeanwhileScenes() || !probe());
	}

	template <typename Probe>
	bool AllowsDialogueOrDeathSound(const CampaignApplicationPolicy& policy,
		Probe&& probe)
	{
		return !policy.hasMeanwhileScenes() || !probe();
	}

	template <typename Probe>
	bool TriggersQueenResponseOrRadarSuppression(
		const CampaignApplicationPolicy& policy, Probe&& probe)
	{
		return policy.hasMeanwhileScenes() && probe();
	}

	template <typename Probe>
	bool MeleeHits(const CampaignApplicationPolicy& policy, bool ordinaryHit,
		Probe&& probe)
	{
		return ordinaryHit ||
			(policy.hasMeanwhileScenes() && probe());
	}
}

int main()
{
	const CampaignApplicationPolicy arulco(GameCampaign::Arulco);
	const CampaignApplicationPolicy unfinishedBusiness(
		GameCampaign::UnfinishedBusiness);
	const CampaignApplicationPolicy arulcoEditor(
		GameCapabilities{GameCampaign::Arulco, true});
	const CampaignApplicationPolicy unfinishedBusinessEditor(
		GameCapabilities{GameCampaign::UnfinishedBusiness, true});
	Check(arulcoEditor.hasMeanwhileScenes(),
		"Arulco editor retains the Arulco meanwhile policy");
	Check(!unfinishedBusinessEditor.hasMeanwhileScenes(),
		"UB editor retains the no-meanwhile policy");

	for (const bool reportedActive : {false, true})
	{
		MeanwhileProbe arulcoProbe{reportedActive};
		const bool arulcoMeanwhile = SceneActive(arulco, arulcoProbe);
		Check(arulcoProbe.called,
			"Arulco queries the live meanwhile state");
		Check(arulcoMeanwhile == reportedActive,
			"Arulco follows the live meanwhile state exactly");

		MeanwhileProbe unfinishedBusinessProbe{reportedActive};
		const bool unfinishedBusinessMeanwhile = SceneActive(
			unfinishedBusiness, unfinishedBusinessProbe);
		Check(!unfinishedBusinessProbe.called,
			"Converted UB scene gate never queries Arulco's meanwhile state");
		Check(!unfinishedBusinessMeanwhile,
			"UB never activates an Arulco meanwhile scene");

		for (const bool martialArtist : {false, true})
		{
			MeanwhileProbe arulcoMartialProbe{reportedActive};
			Check(AllowsMartialArtistIdle(arulco, martialArtist,
				arulcoMartialProbe) ==
				(martialArtist && !reportedActive),
				"Arulco suppresses the martial-artist idle only in a meanwhile");
			Check(arulcoMartialProbe.called == martialArtist,
				"Arulco martial idle preserves its left-hand short circuit");

			MeanwhileProbe unfinishedBusinessMartialProbe{reportedActive};
			Check(AllowsMartialArtistIdle(unfinishedBusiness, martialArtist,
				unfinishedBusinessMartialProbe) ==
				martialArtist,
				"UB always follows the martial-artist idle decision");
			Check(!unfinishedBusinessMartialProbe.called,
				"UB martial idle never reads meanwhile state");
		}

		MeanwhileProbe arulcoDialogueProbe{reportedActive};
		Check(AllowsDialogueOrDeathSound(arulco, arulcoDialogueProbe) ==
			!reportedActive,
			"Arulco allows dialogue and death sounds outside a meanwhile");
		Check(arulcoDialogueProbe.called,
			"Arulco dialogue and death sounds query live meanwhile state");
		MeanwhileProbe unfinishedBusinessDialogueProbe{reportedActive};
		Check(AllowsDialogueOrDeathSound(unfinishedBusiness,
			unfinishedBusinessDialogueProbe),
			"UB allows dialogue and death sounds without a meanwhile probe");
		Check(!unfinishedBusinessDialogueProbe.called,
			"UB dialogue and death sounds never read meanwhile state");

		MeanwhileProbe arulcoSceneEffectProbe{reportedActive};
		Check(TriggersQueenResponseOrRadarSuppression(arulco,
			arulcoSceneEffectProbe) == reportedActive,
			"Arulco queen response and radar suppression follow the meanwhile");
		Check(arulcoSceneEffectProbe.called,
			"Arulco queen response and radar query live meanwhile state");
		MeanwhileProbe unfinishedBusinessSceneEffectProbe{reportedActive};
		Check(!TriggersQueenResponseOrRadarSuppression(unfinishedBusiness,
			unfinishedBusinessSceneEffectProbe),
			"UB never triggers the queen response or radar suppression");
		Check(!unfinishedBusinessSceneEffectProbe.called,
			"UB queen response and radar never read meanwhile state");

		for (const bool ordinaryHit : {false, true})
		{
			MeanwhileProbe arulcoMeleeProbe{reportedActive};
			Check(MeleeHits(arulco, ordinaryHit, arulcoMeleeProbe) ==
				(ordinaryHit || reportedActive),
				"Arulco meanwhile melee preserves its forced-hit rule");
			Check(arulcoMeleeProbe.called == !ordinaryHit,
				"Arulco melee preserves the ordinary-hit short circuit");

			MeanwhileProbe unfinishedBusinessMeleeProbe{reportedActive};
			Check(MeleeHits(unfinishedBusiness, ordinaryHit,
				unfinishedBusinessMeleeProbe) == ordinaryHit,
				"UB melee follows only the ordinary hit roll");
			Check(!unfinishedBusinessMeleeProbe.called,
				"UB melee never reads meanwhile state");
		}
	}

	std::cout << "campaign meanwhile policy tests passed\n";
	return 0;
}
