#include "CampaignQuestPolicy.h"

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

template <typename FactProbe>
std::uint8_t UpdateCampaignFact(
	const CampaignQuestPolicy& policy,
	std::uint8_t previousValue,
	FactProbe&& factProbe)
{
	if (policy.evaluatesArulcoFactRules())
		previousValue = factProbe();
	return previousValue;
}

template <typename QuestProbe, typename OptionProbe, typename Effect>
bool RouteLaptopQuestCompletion(
	const CampaignQuestPolicy& policy,
	QuestProbe&& isFixLaptopQuest,
	OptionProbe&& laptopQuestEnabled,
	Effect&& effect)
{
	if (policy.hasLaptopQuestCompletionEffects() &&
		isFixLaptopQuest() && laptopQuestEnabled())
	{
		effect();
		return true;
	}
	return false;
}

template <typename PowProbe>
bool RoutePrisonerOfWarQuest(
	const CampaignQuestPolicy& policy, PowProbe&& powProbe)
{
	return policy.supportsPrisonerOfWarQuests() && powProbe();
}
}

int main()
{
	GameCapabilities arulcoCapabilities;
	const CampaignQuestPolicy arulco(arulcoCapabilities);
	GameCapabilities arulcoEditorCapabilities = arulcoCapabilities;
	arulcoEditorCapabilities.editor = true;
	const CampaignQuestPolicy arulcoEditor(arulcoEditorCapabilities);

	GameCapabilities unfinishedBusinessCapabilities;
	unfinishedBusinessCapabilities.campaign =
		GameCampaign::UnfinishedBusiness;
	const CampaignQuestPolicy unfinishedBusiness(
		unfinishedBusinessCapabilities);
	GameCapabilities unfinishedBusinessEditorCapabilities =
		unfinishedBusinessCapabilities;
	unfinishedBusinessEditorCapabilities.editor = true;
	const CampaignQuestPolicy unfinishedBusinessEditor(
		unfinishedBusinessEditorCapabilities);

	Check(arulco.evaluatesArulcoFactRules() &&
		arulcoEditor.evaluatesArulcoFactRules() &&
		!unfinishedBusiness.evaluatesArulcoFactRules() &&
		!unfinishedBusinessEditor.evaluatesArulcoFactRules(),
		"immutable campaign capability, not editor identity, selects fact rules");

	constexpr std::array<std::uint8_t, 4> FactValues = {
		0, 1, 0x7f, 0xff};
	for (const std::uint8_t previousValue : FactValues)
	for (const std::uint8_t evaluatedValue : FactValues)
	{
		int arulcoFactReads = 0;
		Check(UpdateCampaignFact(arulco, previousValue, [&]() {
			++arulcoFactReads;
			return evaluatedValue;
		}) == evaluatedValue && arulcoFactReads == 1,
			"Arulco evaluates and assigns each guarded fact exactly once");

		int unfinishedBusinessFactReads = 0;
		Check(UpdateCampaignFact(
				unfinishedBusiness, previousValue, [&]() {
					++unfinishedBusinessFactReads;
					return evaluatedValue;
				}) == previousValue && unfinishedBusinessFactReads == 0,
			"UB leaves every guarded fact unchanged without evaluating it");
	}
	constexpr std::array<bool, 2> BooleanValues = {false, true};

	Check(arulco.killDeidrannaReward() == 25 &&
		unfinishedBusiness.killDeidrannaReward() == 4,
		"Deidranna completion keeps the exact Arulco 25 and UB 4 rewards");
	Check(arulco.initialQuest() ==
			CampaignQuestPolicy::InitialQuest::DeliverLetter &&
		unfinishedBusiness.initialQuest() ==
			CampaignQuestPolicy::InitialQuest::DestroyMissiles,
		"initial quest selection keeps deliver-letter versus destroy-missiles");

	for (const bool isFixLaptopQuest : BooleanValues)
	for (const bool laptopQuestEnabled : BooleanValues)
	{
		int arulcoQuestReads = 0;
		int arulcoOptionReads = 0;
		int arulcoEffects = 0;
		Check(!RouteLaptopQuestCompletion(
				arulco,
				[&]() { ++arulcoQuestReads; return isFixLaptopQuest; },
				[&]() { ++arulcoOptionReads; return laptopQuestEnabled; },
				[&]() { ++arulcoEffects; }) &&
			arulcoQuestReads == 0 && arulcoOptionReads == 0 &&
			arulcoEffects == 0,
			"Arulco short-circuits the UB quest and option reads");

		int ubQuestReads = 0;
		int ubOptionReads = 0;
		int ubEffects = 0;
		const bool routed = RouteLaptopQuestCompletion(
			unfinishedBusiness,
			[&]() { ++ubQuestReads; return isFixLaptopQuest; },
			[&]() { ++ubOptionReads; return laptopQuestEnabled; },
			[&]() { ++ubEffects; });
		const bool expected = isFixLaptopQuest && laptopQuestEnabled;
		Check(routed == expected && ubQuestReads == 1 &&
			ubOptionReads == (isFixLaptopQuest ? 1 : 0) &&
			ubEffects == (expected ? 1 : 0),
			"UB preserves campaign, quest, option, then effect evaluation order");
	}

	for (const bool powStateMatches : BooleanValues)
	{
		int arulcoPowReads = 0;
		Check(RoutePrisonerOfWarQuest(arulco, [&]() {
			++arulcoPowReads;
			return powStateMatches;
		}) == powStateMatches && arulcoPowReads == 1,
			"Arulco retains POW quest processing");

		int ubPowReads = 0;
		Check(!RoutePrisonerOfWarQuest(unfinishedBusiness, [&]() {
			++ubPowReads;
			return powStateMatches;
		}) && ubPowReads == 0,
			"UB returns before POW quest state is evaluated");
	}

	Check(!arulco.hasLaptopQuestCompletionEffects() &&
		unfinishedBusiness.hasLaptopQuestCompletionEffects() &&
		arulcoEditor.supportsPrisonerOfWarQuests() &&
		!unfinishedBusinessEditor.supportsPrisonerOfWarQuests(),
		"editor hosts retain their selected campaign quest behavior");

	std::cout << "campaign quest policy tests passed\n";
	return 0;
}
