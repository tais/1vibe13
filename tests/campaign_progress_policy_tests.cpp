#include "CampaignProgressPolicy.h"

#include <array>
#include <cstdint>
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

constexpr int LegacySurfaceSector(int row, int column)
{
	return (row - 1) * 16 + column - 1;
}

std::uint8_t ExpectedLegacyUnfinishedBusinessProgress(int rawKey)
{
	// Mirror every active label from the former source switch as an int. The
	// exhaustive caller loop below deliberately limits inputs to legacy INT8.
	switch (rawKey)
	{
		case LegacySurfaceSector(8, 7): return 44;
		case LegacySurfaceSector(8, 8): return 45;
		case LegacySurfaceSector(8, 9): return 55;
		case LegacySurfaceSector(8, 10): return 58;
		case LegacySurfaceSector(9, 9): return 60;
		case LegacySurfaceSector(9, 10): return 63;
		case LegacySurfaceSector(9, 11): return 65;
		case LegacySurfaceSector(9, 12): return 68;
		case LegacySurfaceSector(9, 13): return 70;
		case LegacySurfaceSector(10, 11):
		case LegacySurfaceSector(10, 12): return 70;
		case LegacySurfaceSector(10, 13): return 75;
		default: return 50;
	}
}

template <typename Probe>
std::uint8_t RouteProgress(
	const CampaignProgressPolicy& policy, Probe&& unfinishedBusinessProbe)
{
	if (policy.usesUnfinishedBusinessProgress())
		return policy.unfinishedBusinessProgress(unfinishedBusinessProbe());
	return 0;
}

template <typename Probe>
bool RouteScientistAwol(
	const CampaignProgressPolicy& policy,
	std::uint8_t currentProgress,
	std::uint8_t previousHighestProgress,
	Probe&& arulcoThresholdProbe)
{
	return !policy.usesUnfinishedBusinessProgress() &&
		policy.shouldStartScientistAwolMeanwhile(
			currentProgress,
			previousHighestProgress,
			arulcoThresholdProbe());
}
}

int main()
{
	GameCapabilities arulcoCapabilities;
	const CampaignProgressPolicy arulco(arulcoCapabilities);

	GameCapabilities unfinishedBusinessCapabilities;
	unfinishedBusinessCapabilities.campaign =
		GameCampaign::UnfinishedBusiness;
	const CampaignProgressPolicy unfinishedBusiness(
		unfinishedBusinessCapabilities);

	GameCapabilities unfinishedBusinessEditorCapabilities =
		unfinishedBusinessCapabilities;
	unfinishedBusinessEditorCapabilities.editor = true;
	const CampaignProgressPolicy unfinishedBusinessEditor(
		unfinishedBusinessEditorCapabilities);

	Check(!arulco.usesUnfinishedBusinessProgress() &&
		unfinishedBusiness.usesUnfinishedBusinessProgress() &&
		unfinishedBusinessEditor.usesUnfinishedBusinessProgress(),
		"immutable campaign capability, not editor host identity, selects progress");

	int unfinishedBusinessProbeCalls = 0;
	Check(RouteProgress(arulco, [&]() {
			++unfinishedBusinessProbeCalls;
			return std::int8_t{0};
		}) == 0 && unfinishedBusinessProbeCalls == 0,
		"Arulco short-circuits the UB-only strategic progress probe");
	Check(RouteProgress(unfinishedBusiness, [&]() {
			++unfinishedBusinessProbeCalls;
			return std::int8_t{0};
		}) == 50 && unfinishedBusinessProbeCalls == 1,
		"UB evaluates its strategic progress probe exactly once");

	for (int rawKey = -128; rawKey <= 127; ++rawKey)
	{
		Check(unfinishedBusiness.unfinishedBusinessProgress(
				static_cast<std::int8_t>(rawKey)) ==
			ExpectedLegacyUnfinishedBusinessProgress(rawKey),
			"UB preserves every effective signed legacy progress-key result");
	}
	Check(unfinishedBusiness.unfinishedBusinessProgress(118) == 44 &&
		unfinishedBusiness.unfinishedBusinessProgress(-120) == 50,
		"UB keeps reachable H7 progress and the signed I9 fallback explicit");

	int arulcoThresholdProbeCalls = 0;
	Check(!RouteScientistAwol(unfinishedBusiness, 35, 34, [&]() {
			++arulcoThresholdProbeCalls;
			return std::uint32_t{35};
		}) && arulcoThresholdProbeCalls == 0,
		"UB short-circuits Arulco's scientist-AWOL threshold read");
	Check(RouteScientistAwol(arulco, 35, 34, [&]() {
			++arulcoThresholdProbeCalls;
			return std::uint32_t{35};
		}) && arulcoThresholdProbeCalls == 1,
		"Arulco evaluates its scientist-AWOL threshold exactly once");

	constexpr std::array<std::uint8_t, 7> ProgressSamples = {
		0, 1, 34, 35, 36, 100, 255};
	constexpr std::array<std::uint32_t, 9> ThresholdSamples = {
		0, 1, 34, 35, 36, 100, 255, 256, UINT32_MAX};
	for (const std::uint8_t current : ProgressSamples)
	for (const std::uint8_t previous : ProgressSamples)
	for (const std::uint32_t threshold : ThresholdSamples)
	{
		const bool crossed = current > previous &&
			current >= threshold && previous < threshold;
		Check(arulco.shouldStartScientistAwolMeanwhile(
				current, previous, threshold) == crossed,
			"Arulco preserves the scientist-AWOL threshold truth table");
		Check(!unfinishedBusiness.shouldStartScientistAwolMeanwhile(
				current, previous, threshold),
			"UB never starts Arulco's scientist-AWOL meanwhile scene");
	}

	std::cout << "campaign progress policy tests passed\n";
	return 0;
}
