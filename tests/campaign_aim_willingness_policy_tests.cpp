#include "CampaignAimWillingnessPolicy.h"

#include <cstdio>
#include <limits>

namespace
{
using Input = CampaignAimWillingnessInput;
using Reason = CampaignAimWillingnessReason;
int failures = 0;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::fprintf(stderr, "FAIL: %s\n", message);
	++failures;
}

void Expect(const Input& input, Reason reason, std::uint8_t relation,
	bool accepted, const char* message)
{
	const auto decision = DecideCampaignAimWillingness(input);
	Check(decision.reason == reason && decision.relation == relation &&
		decision.accepted() == accepted && decision.waitForDismissal() == !accepted,
		message);
}

void TestOrdinaryAcceptanceAndRefusals()
{
	Input input;
	Expect(input, Reason::Willing, CampaignAimNoRelation, true, "no objections accepts silently");
	input.reputationTooBad = true;
	Expect(input, Reason::Reputation, CampaignAimNoRelation, false, "reputation refusal waits for dismissal");
	input.deathRateTooHigh = true;
	Expect(input, Reason::DeathRate, CampaignAimNoRelation, false, "death rate takes precedence over reputation");
	input.moraleHangover = true;
	input.hatedAliveOnTeam.fill(true);
	input.buddiesAliveOnTeam.fill(true);
	Expect(input, Reason::MoraleHangover, CampaignAimNoRelation, false, "morale hangover rejects even with all buddies present");
}

void TestEachHatedAndBuddyDialogue()
{
	for (std::uint8_t hated = 0; hated < CampaignAimAllRelations; ++hated)
	{
		Input input;
		input.hatedAliveOnTeam[hated] = true;
		input.deathRateTooHigh = input.reputationTooBad = true;
		if (hated < CampaignAimOrdinaryRelations)
		{
			input.hatedToleranceHours[hated] = 23;
			Expect(input, Reason::HatedMerc, hated, false, "each ordinary hated merc keeps its refusal quote index");
			input.hatedToleranceHours[hated] = 24;
			Expect(input, Reason::ToleratedHatred, hated, true, "24-hour hatred tolerance accepts before death/reputation checks");
			input.hatedToleranceHours[hated] = -1;
			Expect(input, Reason::HatedMerc, hated, false, "negative hatred tolerance does not become a large unsigned allowance");
		}
		else Expect(input, Reason::LearnedHatred, hated, false, "mature learned hatred has its separate refusal quote");

		for (std::uint8_t buddy = 0; buddy < CampaignAimAllRelations; ++buddy)
		{
			input.buddiesAliveOnTeam.fill(false);
			input.buddiesAliveOnTeam[buddy] = true;
			Expect(input, Reason::BuddyOverride, buddy, true, "every buddy position overrides every hated position with its own quote");
		}
	}
	for (std::uint8_t buddy = 0; buddy < CampaignAimAllRelations; ++buddy)
	{
		Input input;
		input.buddiesAliveOnTeam[buddy] = true;
		input.deathRateTooHigh = input.reputationTooBad = true;
		Expect(input, Reason::Willing, CampaignAimNoRelation, true, "buddy without a hated merc bypasses risks without an override quote");
	}
}

void TestFirstRelationPrecedence()
{
	Input input;
	input.hatedAliveOnTeam.fill(true);
	input.hatedToleranceHours[0] = 24;
	Expect(input, Reason::ToleratedHatred, 0, true, "first tolerated hatred returns before a later severe or learned hatred");
	input.hatedToleranceHours[0] = 23;
	input.hatedToleranceHours[1] = 24;
	Expect(input, Reason::HatedMerc, 0, false, "first severe hatred is not overridden by a later tolerated hatred");
	input.buddiesAliveOnTeam[4] = input.buddiesAliveOnTeam[2] = true;
	Expect(input, Reason::BuddyOverride, 2, true, "first present buddy determines the override dialogue");
	input.hatedAliveOnTeam.fill(false);
	input.hatedAliveOnTeam[4] = true;
	input.buddiesAliveOnTeam.fill(false);
	Expect(input, Reason::HatedMerc, 4, false, "absent or dead earlier relations do not mask the present hated merc");
}

void TestLearnedCounterBoundaries()
{
	const std::int8_t counters[] = {-128, -1, 0, 1, 127};
	for (const auto counter : counters)
	{
		Input input;
		input.hatedAliveOnTeam[5] = true;
		input.learnedHateCount = counter;
		Expect(input, counter > 0 ? Reason::Willing : Reason::LearnedHatred,
			counter > 0 ? CampaignAimNoRelation : 5, counter > 0,
			"learned hate matures at zero and remains mature for signed-negative counters");
		input = {};
		input.buddiesAliveOnTeam[5] = true;
		input.learnedLikeCount = counter;
		input.deathRateTooHigh = true;
		Expect(input, counter > 0 ? Reason::DeathRate : Reason::Willing,
			CampaignAimNoRelation, counter <= 0,
			"learned buddy risk exemption uses the same signed counter boundary");
		input.hatedAliveOnTeam[0] = true;
		Expect(input, counter > 0 ? Reason::HatedMerc : Reason::BuddyOverride,
			counter > 0 ? 0 : 5, counter <= 0,
			"learned buddy override quote is unavailable before the counter matures");
	}
}

void TestRelationReaderRejectsSentinelsBeforeIndexing()
{
	// Native NUM_PROFILES is 255. IsMercDead indexes directly, so the prior
	// UINT8 relation followed by '< 0' allowed an absent 255 to read past it.
	std::array<bool, 255> aliveOnTeam{};
	aliveOnTeam[0] = aliveOnTeam[254] = true;
	unsigned reads = 0;
	const auto read = [&](std::uint32_t id) { ++reads; return aliveOnTeam[id]; };
	const std::int32_t invalid[] = {-1, std::numeric_limits<std::int32_t>::min(),
		255, 256, std::numeric_limits<std::int32_t>::max()};
	for (const auto id : invalid)
		Check(!ReadCampaignAimRelationPresence(id, aliveOnTeam.size(), read) && reads == 0,
			"absent, negative and out-of-range relations never reach the profile reader");
	Check(!ReadCampaignAimRelationPresence(0, 0, read) && reads == 0,
		"empty profile table never reaches the reader");
	Check(ReadCampaignAimRelationPresence(0, aliveOnTeam.size(), read) &&
		ReadCampaignAimRelationPresence(254, aliveOnTeam.size(), read) && reads == 2,
		"first and final valid profile IDs remain usable");
	Check(!ReadCampaignAimRelationPresence(1, aliveOnTeam.size(), read) && reads == 3,
		"valid but absent/dead relations retain the reader's false result");
}
}

int main()
{
	TestOrdinaryAcceptanceAndRefusals();
	TestEachHatedAndBuddyDialogue();
	TestFirstRelationPrecedence();
	TestLearnedCounterBoundaries();
	TestRelationReaderRejectsSentinelsBeforeIndexing();
	return failures ? 1 : 0;
}
