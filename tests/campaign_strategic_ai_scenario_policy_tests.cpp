#include "CampaignMapScreenPolicy.h"
#include "CampaignStrategicAiScenarioPolicy.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <vector>

namespace
{
	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	template<typename BuiltInProbe, typename CustomProbe>
	bool HasAdvancedPastH8(
		const CampaignStrategicAiScenarioPolicy& policy,
		BuiltInProbe&& builtInProbe,
		CustomProbe&& customProbe)
	{
		switch (policy.h8AdvanceSource())
		{
		case CampaignStrategicAiScenarioPolicy::H8AdvanceSource::
			BuiltInGuardPost:
			return builtInProbe();
		case CampaignStrategicAiScenarioPolicy::H8AdvanceSource::
			DefaultArrivalSector:
			return customProbe();
		case CampaignStrategicAiScenarioPolicy::H8AdvanceSource::None:
			return false;
		}
		return false;
	}

	template<typename BuiltInProbe, typename CustomProbe,
		typename PlayerProbe>
	bool HasVisitedComplex(
		const CampaignStrategicAiScenarioPolicy& policy,
		BuiltInProbe&& builtInProbe,
		CustomProbe&& customProbe,
		PlayerProbe&& playerProbe)
	{
		switch (policy.complexHistorySource())
		{
		case CampaignStrategicAiScenarioPolicy::ComplexHistorySource::
			BuiltInSectorAi:
			if (builtInProbe()) return true;
			else if (playerProbe()) return true;
			break;
		case CampaignStrategicAiScenarioPolicy::ComplexHistorySource::
			StrategicSector:
			if (customProbe()) return true;
			else if (playerProbe()) return true;
			break;
		case CampaignStrategicAiScenarioPolicy::ComplexHistorySource::None:
			break;
		}
		return false;
	}

	template<typename OriginProbe, typename Effect>
	void RouteStrategicAiEffect(
		GameCampaign campaign,
		OriginProbe&& readOrigin,
		Effect&& effect)
	{
		const CampaignMapScreenPolicy campaignPolicy(campaign);
		if (!campaignPolicy.runsUnfinishedBusinessStrategicAi()) return;
		const CampaignStrategicAiScenarioPolicy scenarioPolicy(
			readOrigin());
		if (scenarioPolicy.usesBuiltInSectorAi()) effect();
	}
}

int main()
{
	using Policy = CampaignStrategicAiScenarioPolicy;
	using H8Source = Policy::H8AdvanceSource;
	using ComplexSource = Policy::ComplexHistorySource;

	static_assert(std::is_trivially_copyable_v<CampaignScenarioOrigin>);
	static_assert(std::is_trivially_copyable_v<Policy>);
	static_assert(sizeof(CampaignScenarioOrigin) == sizeof(std::uint8_t));
	static_assert(sizeof(Policy) == sizeof(std::uint8_t));

	for (unsigned rawValue = 0; rawValue <= 0xff; ++rawValue)
	{
		const auto byte = static_cast<std::uint8_t>(rawValue);
		const CampaignScenarioOrigin origin =
			CampaignScenarioOrigin::fromLegacyByte(byte);
		const Policy policy(origin);
		Check(origin.legacyByte() == byte,
			"all 256 legacy bytes round-trip without canonicalizing unknown origins");
		Check(policy.usesBuiltInSectorAi() == (rawValue == 1),
			"only raw one enables built-in JA25 sector AI");
		Check(policy.usesCustomScenario() == (rawValue == 0),
			"only raw zero selects custom scenario state");
		Check(policy.h8AdvanceSource() ==
			(rawValue == 1 ? H8Source::BuiltInGuardPost :
			 rawValue == 0 ? H8Source::DefaultArrivalSector :
			 H8Source::None),
			"every raw byte has an exact H8 advancement source");
		Check(policy.complexHistorySource() ==
			(rawValue == 1 ? ComplexSource::BuiltInSectorAi :
			 rawValue == 0 ? ComplexSource::StrategicSector :
			 ComplexSource::None),
			"every raw byte has an exact complex-history source");
	}

	const Policy builtIn(
		CampaignScenarioOrigin::fromLegacyByte(1));
	const Policy custom(
		CampaignScenarioOrigin::fromLegacyByte(0));
	const Policy unknown(
		CampaignScenarioOrigin::fromLegacyByte(0xff));

	int builtInReads = 0;
	int customReads = 0;
	Check(HasAdvancedPastH8(builtIn,
			[&]() { ++builtInReads; return true; },
			[&]() { ++customReads; return true; }) &&
		builtInReads == 1 && customReads == 0 &&
		HasAdvancedPastH8(custom,
			[&]() { ++builtInReads; return true; },
			[&]() { ++customReads; return true; }) &&
		builtInReads == 1 && customReads == 1 &&
		!HasAdvancedPastH8(unknown,
			[&]() { ++builtInReads; return true; },
			[&]() { ++customReads; return true; }) &&
		builtInReads == 1 && customReads == 1,
		"H8 routing reads only the selected advancement source");

	std::vector<int> probeOrder;
	Check(HasVisitedComplex(builtIn,
			[&]() { probeOrder.push_back(1); return false; },
			[&]() { probeOrder.push_back(2); return false; },
			[&]() { probeOrder.push_back(3); return true; }) &&
		probeOrder == std::vector<int>({1, 3}),
		"built-in complex history keeps sector-AI before player probe order");
	probeOrder.clear();
	Check(HasVisitedComplex(custom,
			[&]() { probeOrder.push_back(1); return false; },
			[&]() { probeOrder.push_back(2); return false; },
			[&]() { probeOrder.push_back(3); return true; }) &&
		probeOrder == std::vector<int>({2, 3}),
		"custom complex history keeps strategic-sector before player probe order");
	probeOrder.clear();
	Check(!HasVisitedComplex(unknown,
			[&]() { probeOrder.push_back(1); return true; },
			[&]() { probeOrder.push_back(2); return true; },
			[&]() { probeOrder.push_back(3); return true; }) &&
		probeOrder.empty(),
		"an unknown origin touches neither complex probe and retains false");

	int originReads = 0;
	int effects = 0;
	auto readBuiltIn = [&]() {
		++originReads;
		return CampaignScenarioOrigin::fromLegacyByte(1);
	};
	RouteStrategicAiEffect(
		GameCampaign::Arulco, readBuiltIn, [&]() { ++effects; });
	Check(originReads == 0 && effects == 0,
		"Arulco left-gates the live scenario-origin read");
	RouteStrategicAiEffect(
		GameCampaign::UnfinishedBusiness,
		readBuiltIn, [&]() { ++effects; });
	Check(originReads == 1 && effects == 1,
		"UB reads the live origin once and runs built-in effects only for raw one");
	RouteStrategicAiEffect(
		GameCampaign::UnfinishedBusiness,
		[&]() {
			++originReads;
			return CampaignScenarioOrigin::fromLegacyByte(0);
		},
		[&]() { ++effects; });
	Check(originReads == 2 && effects == 1,
		"custom UB content suppresses built-in effects after one live read");

	Check(sizeof(Policy) == 1,
		"origin policy remains a one-byte trivially copyable value boundary");
	return 0;
}
