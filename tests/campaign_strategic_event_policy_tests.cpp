#include "CampaignStrategicEventPolicy.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	constexpr std::array<CampaignStrategicEvent, 9> ArulcoEvents{
		CampaignStrategicEvent::MercIntroductionEmail,
		CampaignStrategicEvent::MeanwhileScene,
		CampaignStrategicEvent::MercSiteBackOnline,
		CampaignStrategicEvent::PmcIntroductionEmail,
		CampaignStrategicEvent::KingpinBountyInitial,
		CampaignStrategicEvent::KingpinBountyKilledThem,
		CampaignStrategicEvent::KingpinBountyTimePassed,
		CampaignStrategicEvent::MilitiaRosterEmail,
		CampaignStrategicEvent::IntelEnricoEmail};

	constexpr std::array<CampaignStrategicEvent, 5>
		UnfinishedBusinessEvents{
			CampaignStrategicEvent::InitialSectorAttack,
			CampaignStrategicEvent::DelayedMercQuote,
			CampaignStrategicEvent::DelayedSomeoneInSectorMessage,
			CampaignStrategicEvent::SectorH8Warning,
			CampaignStrategicEvent::EnricoUnderstandingEmail};

	constexpr std::array<CampaignStrategicEvent, 14> AllEvents{
		CampaignStrategicEvent::MercIntroductionEmail,
		CampaignStrategicEvent::MeanwhileScene,
		CampaignStrategicEvent::MercSiteBackOnline,
		CampaignStrategicEvent::InitialSectorAttack,
		CampaignStrategicEvent::DelayedMercQuote,
		CampaignStrategicEvent::DelayedSomeoneInSectorMessage,
		CampaignStrategicEvent::SectorH8Warning,
		CampaignStrategicEvent::EnricoUnderstandingEmail,
		CampaignStrategicEvent::PmcIntroductionEmail,
		CampaignStrategicEvent::KingpinBountyInitial,
		CampaignStrategicEvent::KingpinBountyKilledThem,
		CampaignStrategicEvent::KingpinBountyTimePassed,
		CampaignStrategicEvent::MilitiaRosterEmail,
		CampaignStrategicEvent::IntelEnricoEmail};

	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	template <std::size_t Size>
	constexpr bool Contains(
		const std::array<CampaignStrategicEvent, Size>& events,
		CampaignStrategicEvent event)
	{
		for (const CampaignStrategicEvent candidate : events)
		{
			if (candidate == event)
				return true;
		}
		return false;
	}

	struct CountingProbe
	{
		bool value;
		int& calls;

		bool operator()()
		{
			++calls;
			return value;
		}
	};

	struct TracedProbe
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

	template <typename EffectProbe>
	bool RouteEffect(const CampaignStrategicEventPolicy& policy,
		CampaignStrategicEvent event, EffectProbe&& effect)
	{
		return policy.handles(event) && effect();
	}

	template <typename OptionProbe, typename EffectProbe>
	bool RouteInitialSectorAttack(
		const CampaignStrategicEventPolicy& policy,
		OptionProbe&& optionEnabled, EffectProbe&& effect)
	{
		return policy.handles(
			CampaignStrategicEvent::InitialSectorAttack) &&
			optionEnabled() && effect();
	}

	template <typename DelayProbe, typename EffectProbe>
	bool RouteMeanwhile(const CampaignStrategicEventPolicy& policy,
		DelayProbe&& delayForBattle, EffectProbe&& effect)
	{
		return policy.handles(CampaignStrategicEvent::MeanwhileScene) &&
			!delayForBattle() && effect();
	}
}

int main()
{
	const CampaignStrategicEventPolicy arulco(
		GameCapabilities{GameCampaign::Arulco, false});
	const CampaignStrategicEventPolicy unfinishedBusiness(
		GameCapabilities{GameCampaign::UnfinishedBusiness, false});
	const CampaignStrategicEventPolicy arulcoEditor(
		GameCapabilities{GameCampaign::Arulco, true});
	const CampaignStrategicEventPolicy unfinishedBusinessEditor(
		GameCapabilities{GameCampaign::UnfinishedBusiness, true});

	Check(ArulcoEvents.size() + UnfinishedBusinessEvents.size() ==
		AllEvents.size(),
		"all fourteen strategic event kinds are classified exactly once");
	for (std::size_t index = 0; index < AllEvents.size(); ++index)
	{
		for (std::size_t other = index + 1; other < AllEvents.size(); ++other)
		{
			Check(AllEvents[index] != AllEvents[other],
				"all fourteen strategic event kinds are classified exactly once");
		}

		const CampaignStrategicEvent event = AllEvents[index];
		const bool arulcoOwns = Contains(ArulcoEvents, event);
		const bool unfinishedBusinessOwns =
			Contains(UnfinishedBusinessEvents, event);
		Check(arulcoOwns != unfinishedBusinessOwns,
			"all fourteen strategic event kinds are classified exactly once");
		Check(arulco.handles(event) == arulcoOwns,
			"Arulco routes only its nine strategic event kinds");
		Check(unfinishedBusiness.handles(event) == unfinishedBusinessOwns,
			"UB routes only its five strategic event kinds");
		Check(arulcoEditor.handles(event) == arulco.handles(event) &&
			unfinishedBusinessEditor.handles(event) ==
				unfinishedBusiness.handles(event),
			"editor capability does not change campaign event routing");

		for (const bool effectValue : {false, true})
		{
			int arulcoEffectCalls = 0;
			const bool arulcoResult = RouteEffect(arulco, event,
				CountingProbe{effectValue, arulcoEffectCalls});
			Check(arulcoEffectCalls == (arulcoOwns ? 1 : 0) &&
				arulcoResult == (arulcoOwns && effectValue),
				"rejected campaigns never evaluate event effects");

			int unfinishedBusinessEffectCalls = 0;
			const bool unfinishedBusinessResult = RouteEffect(
				unfinishedBusiness, event,
				CountingProbe{effectValue, unfinishedBusinessEffectCalls});
			Check(unfinishedBusinessEffectCalls ==
					(unfinishedBusinessOwns ? 1 : 0) &&
				unfinishedBusinessResult ==
					(unfinishedBusinessOwns && effectValue),
				"rejected campaigns never evaluate event effects");
		}
	}

	const auto unknownEvent = static_cast<CampaignStrategicEvent>(
		static_cast<std::uint8_t>(CampaignStrategicEvent::Count) + 1);
	Check(!arulco.handles(CampaignStrategicEvent::Count) &&
		!unfinishedBusiness.handles(CampaignStrategicEvent::Count) &&
		!arulco.handles(unknownEvent) &&
		!unfinishedBusiness.handles(unknownEvent),
		"unknown strategic event kinds are unavailable");

	for (const bool optionEnabled : {false, true})
	{
		std::vector<std::string> arulcoTrace;
		Check(!RouteInitialSectorAttack(arulco,
			TracedProbe{"option", optionEnabled, arulcoTrace},
			TracedProbe{"effect", true, arulcoTrace}) &&
			arulcoTrace.empty(),
			"initial-sector option remains behind the UB event gate");

		std::vector<std::string> unfinishedBusinessTrace;
		Check(RouteInitialSectorAttack(unfinishedBusiness,
			TracedProbe{"option", optionEnabled, unfinishedBusinessTrace},
			TracedProbe{"effect", true, unfinishedBusinessTrace}) ==
			optionEnabled,
			"initial-sector effect remains behind the live option");
		const std::vector<std::string> expectedTrace = optionEnabled
			? std::vector<std::string>{"option", "effect"}
			: std::vector<std::string>{"option"};
		Check(unfinishedBusinessTrace == expectedTrace,
			"initial-sector effect remains behind the live option");
	}

	for (const bool delayedForBattle : {false, true})
	{
		std::vector<std::string> unfinishedBusinessTrace;
		Check(!RouteMeanwhile(unfinishedBusiness,
			TracedProbe{"delay", delayedForBattle, unfinishedBusinessTrace},
			TracedProbe{"effect", true, unfinishedBusinessTrace}) &&
			unfinishedBusinessTrace.empty(),
			"meanwhile delay remains behind the Arulco event gate");

		std::vector<std::string> arulcoTrace;
		Check(RouteMeanwhile(arulco,
			TracedProbe{"delay", delayedForBattle, arulcoTrace},
			TracedProbe{"effect", true, arulcoTrace}) ==
			!delayedForBattle,
			"Arulco keeps delay before meanwhile effects");
		const std::vector<std::string> expectedTrace = delayedForBattle
			? std::vector<std::string>{"delay"}
			: std::vector<std::string>{"delay", "effect"};
		Check(arulcoTrace == expectedTrace,
			"Arulco keeps delay before meanwhile effects");
	}

	std::cout << "campaign strategic-event policy tests passed\n";
	return 0;
}
