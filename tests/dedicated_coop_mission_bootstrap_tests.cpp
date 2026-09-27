#include "Ja2/DedicatedCoopMissionPolicy.h"
#include "Ja2/DedicatedCoopPostCombatCheckpointPolicy.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace
{
int failures = 0;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::fprintf(stderr, "FAIL: %s\n", message);
	++failures;
}

DedicatedCoopStarterCandidate Candidate(
	std::uint8_t profile,
	std::uint32_t charge,
	bool aim = true,
	bool hireable = true,
	bool healthy = true)
{
	return {profile, charge, aim, hireable, healthy};
}

void TestCanonicalCompleteSelection()
{
	const std::array<DedicatedCoopStarterCandidate, 9> candidates{{
		Candidate(40, 900),
		Candidate(7, 300),
		Candidate(2, 100),
		Candidate(9, 300),
		Candidate(4, 200),
		Candidate(1, 50, false),
		Candidate(3, 25, true, false),
		Candidate(5, 10, true, true, false),
		Candidate(2, 150),
	}};
	const DedicatedCoopStarterSelection selected =
		SelectDedicatedCoopStarterRoster(candidates.data(), candidates.size(),
			2000, DedicatedCoopStarterRosterSize);
	Check(static_cast<bool>(selected), "complete roster should be selected");
	Check(selected.profiles == std::array<std::uint8_t, 4>{{2, 4, 7, 9}},
		"selection must sort by charge then profile and deduplicate profiles");
	Check(selected.totalCharge == 900,
		"selection must report the complete canonical roster charge");
}

void TestInputOrderDoesNotMatter()
{
	const std::array<DedicatedCoopStarterCandidate, 6> forward{{
		Candidate(11, 500), Candidate(3, 100), Candidate(8, 300),
		Candidate(1, 100), Candidate(9, 400), Candidate(7, 200)}};
	const std::array<DedicatedCoopStarterCandidate, 6> reverse{{
		forward[5], forward[4], forward[3],
		forward[2], forward[1], forward[0]}};
	const DedicatedCoopStarterSelection first =
		SelectDedicatedCoopStarterRoster(forward.data(), forward.size(),
			5000, DedicatedCoopStarterRosterSize);
	const DedicatedCoopStarterSelection second =
		SelectDedicatedCoopStarterRoster(reverse.data(), reverse.size(),
			5000, DedicatedCoopStarterRosterSize);
	Check(first && second, "both content orders should produce a roster");
	Check(first.profiles == second.profiles &&
		first.totalCharge == second.totalCharge,
		"content/UI order must not affect the starter roster");
	Check(first.profiles == std::array<std::uint8_t, 4>{{1, 3, 7, 8}},
		"equal charges must use profile id as the deterministic tie-break");
}

void TestFailClosedWithoutPartialRoster()
{
	const std::array<DedicatedCoopStarterCandidate, 4> candidates{{
		Candidate(1, 100), Candidate(2, 200),
		Candidate(3, 300), Candidate(4, 400)}};

	DedicatedCoopStarterSelection selected =
		SelectDedicatedCoopStarterRoster(candidates.data(), candidates.size(),
			999, DedicatedCoopStarterRosterSize);
	Check(selected.error ==
		DedicatedCoopStarterSelectionError::InsufficientFunds,
		"one-unit budget deficit must reject the whole roster");
	Check(selected.profiles == std::array<std::uint8_t, 4>{},
		"funding failure must expose no partial roster");

	selected = SelectDedicatedCoopStarterRoster(
		candidates.data(), candidates.size(), 1000,
		DedicatedCoopStarterRosterSize - 1);
	Check(selected.error ==
		DedicatedCoopStarterSelectionError::TeamCapacityTooSmall,
		"team capacity must cover every admitted authority peer");

	const std::array<DedicatedCoopStarterCandidate, 4> onlyThree{{
		Candidate(1, 100), Candidate(2, 200), Candidate(3, 300),
		Candidate(4, 400, true, false)}};
	selected = SelectDedicatedCoopStarterRoster(
		onlyThree.data(), onlyThree.size(), 1000,
		DedicatedCoopStarterRosterSize);
	Check(selected.error ==
		DedicatedCoopStarterSelectionError::InsufficientCandidates,
		"an ineligible fourth merc must reject the whole roster");
}

void TestMalformedAndUnrepresentableInput()
{
	DedicatedCoopStarterSelection selected =
		SelectDedicatedCoopStarterRoster(nullptr, 1, 1000,
			DedicatedCoopStarterRosterSize);
	Check(selected.error == DedicatedCoopStarterSelectionError::InvalidInput,
		"nonzero null input must fail closed");

	const std::array<DedicatedCoopStarterCandidate, 5> candidates{{
		Candidate(1, static_cast<std::uint32_t>(
			std::numeric_limits<std::int32_t>::max()) + 1u),
		Candidate(2, 100), Candidate(3, 200), Candidate(4, 300),
		Candidate(5, 400)}};
	selected = SelectDedicatedCoopStarterRoster(
		candidates.data(), candidates.size(), 1000,
		DedicatedCoopStarterRosterSize);
	Check(selected &&
		selected.profiles == std::array<std::uint8_t, 4>{{2, 3, 4, 5}},
		"unrepresentable finance transactions must never enter the roster");
}

DedicatedCoopStarterCampaignEvidence InitialEvidence()
{
	DedicatedCoopStarterCampaignEvidence evidence;
	evidence.gameJustStarted = true;
	evidence.initialWorldTime = true;
	evidence.noWorldSector = true;
	evidence.tacticalWorldUnloaded = true;
	evidence.starterEnvironmentValid = true;
	return evidence;
}

void TestColdStarterCampaignClassification()
{
	DedicatedCoopStarterCampaignEvidence evidence = InitialEvidence();
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::UntouchedInitial,
		"an exact empty initial campaign must be bootstrap-eligible");

	evidence.activePlayerMercs = DedicatedCoopStarterRosterSize;
	evidence.validPreparedMercs = DedicatedCoopStarterRosterSize;
	evidence.delayedHiringEvents = DedicatedCoopStarterRosterSize;
	evidence.matchedPreparedEvents = DedicatedCoopStarterRosterSize;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::PreparedInitial,
		"the complete durable in-transit roster must be restartable");

	--evidence.matchedPreparedEvents;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"a prepared roster with a missing or duplicate arrival event must fail");
	evidence = InitialEvidence();
	evidence.delayedHiringEvents = 1;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"an ostensibly empty campaign with a delayed hire must not be mutated");
	evidence = InitialEvidence();
	evidence.activePlayerMercs = 1;
	evidence.validEstablishedMercs = 1;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"a partial initial roster must not fall through as established");
	evidence = InitialEvidence();
	evidence.initialWorldTime = false;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"a contradictory initial marker must fail closed");

	evidence = InitialEvidence();
	evidence.gameJustStarted = false;
	evidence.initialWorldTime = false;
	evidence.starterEnvironmentValid = false;
	evidence.activePlayerMercs = 3;
	evidence.validEstablishedMercs = 1;
	evidence.validEstablishedRosterMercs = 3;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::EstablishedCold,
		"a cold established campaign with a live player merc must resume");
	evidence.validEstablishedMercs = 0;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::EstablishedStrategicCold,
		"a fully validated wounded roster resumes cold without tactical launch");
	evidence.validEstablishedRosterMercs = 0;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"a cold non-initial campaign without a valid supported roster must fail closed");
}

void TestEstablishedResumeRequiresCompleteCohort()
{
	DedicatedCoopStarterCampaignEvidence evidence;
	evidence.noWorldSector = evidence.tacticalWorldUnloaded = true;
	evidence.activePlayerMercs = evidence.validEstablishedRosterMercs = 4;
	evidence.travelingEstablishedMercs = 4;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::EstablishedStrategicCold,
		"a fully validated traveling cohort may resume without processing arrival events");
	evidence.travelingEstablishedMercs = 2;
	evidence.validEstablishedMercs = 2;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::EstablishedStrategicCold,
		"healthy stationary mercs must not auto-launch while their cohort travels");
	evidence.travelingEstablishedMercs = 0;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::EstablishedCold,
		"a completely stationary supported cohort retains ordinary tactical selection");
	// Previous code deliberately admitted stationary mixed rosters through the
	// one healthy launchable member. Do not make this bounded extension revoke
	// that support just because another historical member is dead/POW/on duty.
	evidence.validEstablishedRosterMercs = 1;
	evidence.validEstablishedMercs = 1;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::EstablishedCold,
		"healthy plus dead, POW, or non-squad stationary roster retains prior resume eligibility");
	evidence.unexpectedActivePlayerActors = 1;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::EstablishedCold,
		"a stationary vehicle record does not revoke an existing healthy foot actor's launch eligibility");
	evidence.unexpectedActivePlayerActors = 0;
	evidence.travelingEstablishedMercs = 1;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"an observed unvalidated traveler prevents falling through to legacy stationary launch");
	evidence.travelingEstablishedMercs = 0;
	evidence.validEstablishedMercs = 0;
	evidence.validEstablishedRosterMercs = 3;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"wounded-only resume does not silently add support for mixed dead or duty rosters");
	evidence.validEstablishedRosterMercs = 4;
	evidence.validEstablishedMercs = 2;
	evidence.travelingEstablishedMercs = 2;
	const DedicatedCoopStarterCampaignEvidence valid = evidence;
	for (std::size_t count : {0u, 3u, 5u})
	{
		evidence = valid;
		evidence.validEstablishedRosterMercs = count;
		Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
			DedicatedCoopStarterCampaignState::Ineligible,
			"missing, extra, or empty roster accounting cannot hide behind a healthy merc");
	}
	evidence = valid;
	evidence.travelingEstablishedMercs = 3;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"launchable and traveling counts must be disjoint subsets of the exact cohort");
	evidence = valid;
	evidence.validEstablishedMercs = 5;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"launchable actors cannot exceed the validated roster");
	evidence = valid;
	evidence.unexpectedActivePlayerActors = 1;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"an unexpected vehicle or foreign-team actor remains unsupported");
	evidence = valid;
	evidence.gameJustStarted = true;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"strategic roster evidence never repairs an ambiguous initial marker");
	evidence = valid;
	evidence.activePlayerMercs = evidence.validEstablishedRosterMercs =
		MaximumDedicatedCoopEstablishedSectorCandidates + 1;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"established roster accounting is bounded by native capture capacity");
}

DedicatedCoopPendingAimHireEvidence PendingAimHireEvidence()
{
	DedicatedCoopPendingAimHireEvidence actor;
	actor.exactIdentity = actor.activePlayer = actor.aimProfile =
		actor.profileAwaitingArrival = actor.humanBody = actor.inTransit =
		actor.usesLandingZone = actor.arrivingGameInsertion =
		actor.contractTypeMatches = actor.medicalDepositMatches = actor.insuranceReset = true;
	actor.life = actor.maximumLife = 80;
	actor.contractDays = 7;
	actor.contractEndMinute = 14100;
	actor.timeCanSignElsewhere = 2000;
	actor.x = actor.landingX = 9;
	actor.y = actor.landingY = 1;
	actor.z = 0;
	actor.arrivalMinute = 2610;
	actor.worldSeconds = 120000;
	actor.arrivalEvents = actor.matchingArrivalEvents = 1;
	return actor;
}

void TestPendingAimHirePolicy()
{
	const auto valid = PendingAimHireEvidence();
	Check(DedicatedCoopPendingAimHireEligible(valid),
		"an established native pending AIM hire retains its exact pre-arrival contract");
	for (int days : {1, 7, 14})
	{
		for (bool overnight : {false, true})
		{
			auto actor = valid;
			actor.contractDays = days;
			actor.arrivalMinute = overnight ? 3330 : 2610;
			actor.contractEndMinute = 1440 + (days + 1) * 1440 + (overnight ? 420 : 1140);
			actor.timeCanSignElsewhere = days == 14 ? actor.contractEndMinute : 2000;
			Check(DedicatedCoopPendingAimHireEligible(actor),
				"one, seven and fourteen day pre-arrival contracts preserve same-day and overnight formulas");
		}
	}
	for (bool DedicatedCoopPendingAimHireEvidence::* member : {
		&DedicatedCoopPendingAimHireEvidence::exactIdentity,
		&DedicatedCoopPendingAimHireEvidence::activePlayer,
		&DedicatedCoopPendingAimHireEvidence::aimProfile,
		&DedicatedCoopPendingAimHireEvidence::profileAwaitingArrival,
		&DedicatedCoopPendingAimHireEvidence::humanBody,
		&DedicatedCoopPendingAimHireEvidence::inTransit,
		&DedicatedCoopPendingAimHireEvidence::usesLandingZone,
		&DedicatedCoopPendingAimHireEvidence::arrivingGameInsertion,
		&DedicatedCoopPendingAimHireEvidence::contractTypeMatches,
		&DedicatedCoopPendingAimHireEvidence::medicalDepositMatches,
		&DedicatedCoopPendingAimHireEvidence::insuranceReset})
	{
		auto actor = valid;
		actor.*member = false;
		Check(!DedicatedCoopPendingAimHireEligible(actor),
			"pending resume requires every identity, employment, health and insertion fact");
	}
	for (bool DedicatedCoopPendingAimHireEvidence::* member : {
		&DedicatedCoopPendingAimHireEvidence::unsupportedRole,
		&DedicatedCoopPendingAimHireEvidence::inSector,
		&DedicatedCoopPendingAimHireEvidence::betweenSectors})
	{
		auto actor = valid;
		actor.*member = true;
		Check(!DedicatedCoopPendingAimHireEligible(actor),
			"pending arrivals cannot already have an unsupported tactical or traveling role");
	}
	for (int change = 0; change < 17; ++change)
	{
		auto actor = valid;
		switch (change)
		{
		case 0: actor.life = 14; break;
		case 1: actor.maximumLife = 79; break;
		case 2: actor.maximumLife = 101; break;
		case 3: actor.contractDays = -1; break;
		case 4: actor.contractDays = 2; break;
		case 5: actor.contractEndMinute = -1; break;
		case 6: ++actor.contractEndMinute; break;
		case 7: actor.timeCanSignElsewhere = -1; break;
		case 8: ++actor.timeCanSignElsewhere; break;
		case 9: actor.x = 0; break;
		case 10: actor.landingY = 2; break;
		case 11: actor.z = 1; break;
		case 12: actor.groupSlot = 7; break;
		case 13: actor.arrivalMinute = std::numeric_limits<std::uint32_t>::max() / 60u + 1; break;
		case 14: actor.arrivalMinute = 0; break;
		case 15: actor.arrivalEvents = 2; break;
		case 16: actor.matchingArrivalEvents = 0; break;
		}
		Check(!DedicatedCoopPendingAimHireEligible(actor),
			"invalid signed employment, coordinate, time or duplicate event evidence rejects pending resume");
	}
	auto actor = valid;
	actor.worldSeconds = actor.arrivalMinute * 60;
	Check(DedicatedCoopPendingAimHireEligible(actor),
		"a precisely due pending hire can remain paused without executing its event");
	++actor.worldSeconds;
	Check(!DedicatedCoopPendingAimHireEligible(actor),
		"overdue pending hires cannot be silently resumed past their native callback boundary");
	actor = valid;
	actor.contractDays = 14;
	actor.contractEndMinute += 7 * 1440;
	Check(!DedicatedCoopPendingAimHireEligible(actor),
		"a fourteen day hire must preserve its native exclusive-contract endpoint");
}

void TestEstablishedPendingHireCohort()
{
	DedicatedCoopStarterCampaignEvidence evidence;
	evidence.noWorldSector = evidence.tacticalWorldUnloaded = true;
	evidence.activePlayerMercs = 3;
	evidence.validEstablishedRosterMercs = evidence.validEstablishedMercs = 2;
	evidence.observedPendingHireActors = evidence.validPendingAimHireActors =
		evidence.delayedHiringEvents = 1;
	evidence.pendingHireCohortConsistent = true;
	const auto valid = evidence;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::EstablishedStrategicCold,
		"fully validated stationary roster plus pending AIM hire resumes paused without tactical launch");
	evidence.travelingEstablishedMercs = 2;
	evidence.validEstablishedMercs = 0;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::EstablishedStrategicCold,
		"fully validated traveling roster and pending AIM cohort share paused strategic resume");
	for (int change = 0; change < 11; ++change)
	{
		evidence = valid;
		switch (change)
		{
		case 0: evidence.pendingHireCohortConsistent = false; break;
		case 1: evidence.validPendingAimHireActors = 0; break;
		case 2: evidence.observedPendingHireActors = 2; break;
		case 3: evidence.delayedHiringEvents = 0; break;
		case 4: evidence.delayedHiringEvents = 2; break;
		case 5: evidence.validEstablishedRosterMercs = 1; break;
		case 6: evidence.unexpectedActivePlayerActors = 1; break;
		case 7: evidence.gameJustStarted = true; break;
		case 8: evidence.initialWorldTime = true; break;
		case 9: evidence.travelingEstablishedMercs = 1; break;
		case 10: evidence.activePlayerMercs = 1;
			evidence.validEstablishedRosterMercs = evidence.validEstablishedMercs = 0; break;
		}
		Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
			DedicatedCoopStarterCampaignState::Ineligible,
			"healthy stationary merc cannot hide incomplete, unsupported or ambiguous pending cohort");
	}
	evidence = valid;
	evidence.observedPendingHireActors = evidence.validPendingAimHireActors = 0;
	evidence.activePlayerMercs = 2;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"an orphan delayed event alone blocks historical healthy stationary fallthrough");
}

void TestCompletedDeathPendingHireCohort()
{
	DedicatedCoopCompletedDeathEvidence dead;
	dead.exactIdentity = dead.activePlayer = dead.realProfile = dead.profileDead =
		dead.humanBody = dead.deathAssignment = dead.deathFlag = dead.deathUiComplete =
		dead.noLiveMembership = dead.visitedSector = true;
	dead.maximumLife = 90; dead.x = 9; dead.y = 1; dead.z = 0;
	dead.contractEndMinute = 13740;
	Check(DedicatedCoopCompletedDeathEligible(dead),
		"completed native death retains an active employment record and its unexpired contract");
	for (bool DedicatedCoopCompletedDeathEvidence::* member : {
		&DedicatedCoopCompletedDeathEvidence::exactIdentity,
		&DedicatedCoopCompletedDeathEvidence::activePlayer,
		&DedicatedCoopCompletedDeathEvidence::realProfile,
		&DedicatedCoopCompletedDeathEvidence::profileDead,
		&DedicatedCoopCompletedDeathEvidence::humanBody,
		&DedicatedCoopCompletedDeathEvidence::deathAssignment,
		&DedicatedCoopCompletedDeathEvidence::deathFlag,
		&DedicatedCoopCompletedDeathEvidence::deathUiComplete,
		&DedicatedCoopCompletedDeathEvidence::noLiveMembership,
		&DedicatedCoopCompletedDeathEvidence::visitedSector})
	{
		auto altered = dead; altered.*member = false;
		Check(!DedicatedCoopCompletedDeathEligible(altered), "death requires every completed native lifecycle proof");
	}
	for (int change = 0; change < 14; ++change)
	{
		auto altered = dead;
		switch (change)
		{
		case 0: altered.unsupportedRole = true; break;
		case 1: altered.airborne = true; break;
		case 2: altered.betweenSectors = true; break;
		case 3: altered.life = 1; break;
		case 4: altered.maximumLife = 0; break;
		case 5: altered.breath = 1; break;
		case 6: altered.maximumBreath = 1; break;
		case 7: altered.contractEndMinute = -1; break;
		case 8: altered.groupSlot = 1; break;
		case 9: altered.x = 0; break;
		case 10: altered.x = 17; break;
		case 11: altered.y = 17; break;
		case 12: altered.z = -1; break;
		case 13: altered.z = 4; break;
		}
		Check(!DedicatedCoopCompletedDeathEligible(altered), "partial death and unsupported retained deployment reject");
	}
	DedicatedCoopStarterCampaignEvidence cohort;
	cohort.noWorldSector = cohort.tacticalWorldUnloaded = cohort.pendingHireCohortConsistent = true;
	cohort.activePlayerMercs = 3;
	cohort.validEstablishedRosterMercs = cohort.validEstablishedMercs = 1;
	cohort.validCompletedDeadMercs = 1;
	cohort.observedPendingHireActors = cohort.validPendingAimHireActors = cohort.delayedHiringEvents = 1;
	Check(ClassifyDedicatedCoopStarterCampaign(cohort) == DedicatedCoopStarterCampaignState::EstablishedStrategicCold,
		"one living ordinary merc, one completed death and a pending hire resume paused together");
	const auto valid = cohort;
	for (int change = 0; change < 10; ++change)
	{
		cohort = valid;
		switch (change)
		{
		case 0: cohort.validCompletedDeadMercs = 0; break;
		case 1: cohort.validCompletedDeadMercs = 2; break;
		case 2: cohort.validCompletedDeadMercs = std::numeric_limits<std::size_t>::max(); break;
		case 3: cohort.activePlayerMercs = 0; break;
		case 4: cohort.validPendingAimHireActors = std::numeric_limits<std::size_t>::max(); break;
		case 5: cohort.validEstablishedRosterMercs = cohort.validEstablishedMercs = 0;
			cohort.validCompletedDeadMercs = 1; break;
		case 6: cohort.travelingEstablishedMercs = 2; break;
		case 7: cohort.pendingHireCohortConsistent = false; break;
		case 8: cohort.delayedHiringEvents = 2; break;
		case 9: cohort.observedPendingHireActors = 2; break;
		}
		Check(ClassifyDedicatedCoopStarterCampaign(cohort) == DedicatedCoopStarterCampaignState::Ineligible,
			"completed-death allowance cannot hide missing living members, bad pending events or overflow");
	}
	cohort = valid;
	cohort.validEstablishedRosterMercs = cohort.validEstablishedMercs = 0;
	cohort.validCompletedDeadMercs = 2;
	Check(ClassifyDedicatedCoopStarterCampaign(cohort) == DedicatedCoopStarterCampaignState::EstablishedStrategicCold,
		"completed deaths and a paid pending arrival can resume paused without a living squad");
	cohort.pendingHireCohortConsistent = false;
	Check(ClassifyDedicatedCoopStarterCampaign(cohort) == DedicatedCoopStarterCampaignState::Ineligible,
		"absence of a living squad cannot bypass exact native death and pending-hire evidence");
	cohort = valid;
	cohort.activePlayerMercs = MaximumDedicatedCoopEstablishedSectorCandidates;
	cohort.validCompletedDeadMercs = cohort.activePlayerMercs - 2;
	Check(ClassifyDedicatedCoopStarterCampaign(cohort) == DedicatedCoopStarterCampaignState::EstablishedStrategicCold,
		"exact bounded roster capacity includes terminal retained records without adding their counts");
	++cohort.activePlayerMercs; ++cohort.validCompletedDeadMercs;
	Check(ClassifyDedicatedCoopStarterCampaign(cohort) == DedicatedCoopStarterCampaignState::Ineligible,
		"retained death records cannot exceed the existing native roster bound");
}

void TestLivingWoundedRosterPolicy()
{
	DedicatedCoopEstablishedActorEvidence actor;
	actor.exactIdentity = actor.activePlayer = actor.ordinarySquadAssignment =
		actor.visitedSector = true;
	actor.life = 5;
	actor.maximumLife = 90;
	actor.x = 9;
	actor.y = 1;
	actor.z = 0;
	Check(DedicatedCoopEstablishedRosterActorEligible(actor),
		"living wounded on-foot squad actors remain durable campaign members");
	const auto valid = actor;
	for (int life : {0, -1, 91})
	{
		actor = valid;
		actor.life = life;
		Check(!DedicatedCoopEstablishedRosterActorEligible(actor),
			"dead or inconsistent vital state is not broadened into supported resume");
	}
	for (bool DedicatedCoopEstablishedActorEvidence::* member : {
		&DedicatedCoopEstablishedActorEvidence::exactIdentity,
		&DedicatedCoopEstablishedActorEvidence::activePlayer,
		&DedicatedCoopEstablishedActorEvidence::ordinarySquadAssignment,
		&DedicatedCoopEstablishedActorEvidence::visitedSector})
	{
		actor = valid;
		actor.*member = false;
		Check(!DedicatedCoopEstablishedRosterActorEligible(actor),
			"wounded support does not remove identity, team, squad, or sector proof");
	}
	for (bool DedicatedCoopEstablishedActorEvidence::* member : {
		&DedicatedCoopEstablishedActorEvidence::vehicleBody,
		&DedicatedCoopEstablishedActorEvidence::driver,
		&DedicatedCoopEstablishedActorEvidence::passenger,
		&DedicatedCoopEstablishedActorEvidence::airborne})
	{
		actor = valid;
		actor.*member = true;
		Check(!DedicatedCoopEstablishedRosterActorEligible(actor),
			"vehicle state remains outside cold on-foot resume");
	}
	actor = valid;
	actor.x = 17;
	Check(!DedicatedCoopEstablishedRosterActorEligible(actor),
		"wounded actors still require an in-world strategic coordinate");
}

DedicatedCoopEstablishedGroupEvidence TravelingGroupEvidence()
{
	DedicatedCoopEstablishedGroupEvidence group;
	group.identityValid = group.playerFootGroup = group.membersConsistent = true;
	group.declaredMembers = group.observedMembers = 4;
	group.actorMatches = 1;
	group.x = 9;
	group.y = 1;
	group.z = 0;
	group.nextX = 9;
	group.nextY = 2;
	group.betweenSectors = group.actorBetweenSectors = true;
	group.traverseMinutes = 89;
	group.arrivalMinutes = 2000;
	group.worldSeconds = 115000;
	group.arrivalEvents = group.matchingArrivalEvents = 1;
	return group;
}

void TestEstablishedGroupTravelConsistency()
{
	const auto valid = TravelingGroupEvidence();
	Check(DedicatedCoopEstablishedGroupConsistent(valid),
		"an exact live foot group and one matching pending arrival may resume");
	for (bool DedicatedCoopEstablishedGroupEvidence::* member : {
		&DedicatedCoopEstablishedGroupEvidence::identityValid,
		&DedicatedCoopEstablishedGroupEvidence::playerFootGroup,
		&DedicatedCoopEstablishedGroupEvidence::membersConsistent,
		&DedicatedCoopEstablishedGroupEvidence::actorBetweenSectors})
	{
		auto group = valid;
		group.*member = false;
		Check(!DedicatedCoopEstablishedGroupConsistent(group),
			"stale group identity, foreign/vehicle group, member mismatch, or transit mismatch fails");
	}
	for (std::size_t count : {0u, 3u, 5u})
	{
		auto group = valid;
		group.observedMembers = count;
		Check(!DedicatedCoopEstablishedGroupConsistent(group),
			"native group membership must match the exact declared size");
	}
	for (std::size_t count : {0u, 2u})
	{
		auto group = valid;
		group.actorMatches = count;
		Check(!DedicatedCoopEstablishedGroupConsistent(group),
			"the actor must occur exactly once in its native group");
		group = valid;
		group.arrivalEvents = count;
		Check(!DedicatedCoopEstablishedGroupConsistent(group),
			"missing or duplicate arrival events cannot be silently repaired");
	}
	for (std::uint32_t arrival : {0u, 1000u,
		std::numeric_limits<std::uint32_t>::max() / 60u + 1u})
	{
		auto group = valid;
		group.arrivalMinutes = arrival;
		Check(!DedicatedCoopEstablishedGroupConsistent(group),
			"zero, overdue, or overflowing travel arrival time fails closed");
	}
	auto group = valid;
	group.nextX = 10;
	Check(!DedicatedCoopEstablishedGroupConsistent(group),
		"a pending native next-sector movement must be orthogonally adjacent");
	group = valid;
	group.matchingArrivalEvents = 0;
	Check(!DedicatedCoopEstablishedGroupConsistent(group),
		"one malformed arrival is not equivalent to one valid arrival");
	group = valid;
	group.worldSeconds = group.arrivalMinutes * 60u;
	Check(DedicatedCoopEstablishedGroupConsistent(group),
		"an arrival due exactly at the paused boundary remains pending without being executed");
	group = valid;
	group.traverseMinutes = 0;
	Check(!DedicatedCoopEstablishedGroupConsistent(group),
		"travel requires a nonzero native traversal duration");
	group = valid;
	group.traverseMinutes = std::numeric_limits<std::uint32_t>::max();
	Check(!DedicatedCoopEstablishedGroupConsistent(group),
		"the native impassable movement sentinel is not a valid travel duration");
	group = valid;
	group.z = 1;
	Check(!DedicatedCoopEstablishedGroupConsistent(group),
		"this extension does not authorize underground tactical traversal resumes");
	group = valid;
	group.delayedHiringEvents = 1;
	Check(!DedicatedCoopEstablishedGroupConsistent(group),
		"ordinary strategic travel cannot retain a contradictory initial hiring event");
	group = valid;
	group.betweenSectors = group.actorBetweenSectors = false;
	Check(!DedicatedCoopEstablishedGroupConsistent(group),
		"stationary actors cannot retain a pending group-arrival event");
	group.arrivalEvents = group.matchingArrivalEvents = 0;
	Check(DedicatedCoopEstablishedGroupConsistent(group),
		"stationary groups may retain harmless historical travel fields without events");
}

void TestEstablishedArrivalEventIdentity()
{
	Check(DedicatedCoopEstablishedArrivalMatches(7, 7, 120000, 2000, true, 0, 0),
		"exact one-time native arrival identity and minute-to-second conversion match");
	Check(DedicatedCoopEstablishedArrivalTargetsGroup(263, 7) &&
		!DedicatedCoopEstablishedArrivalMatches(263, 7, 120000, 2000, true, 0, 0),
		"high parameter bits alias the native callback but cannot satisfy exact group identity");
	Check(!DedicatedCoopEstablishedArrivalTargetsGroup(7, 0) &&
		!DedicatedCoopEstablishedArrivalTargetsGroup(7, 263) &&
		!DedicatedCoopEstablishedArrivalMatches(0, 0, 120000, 2000, true, 0, 0),
		"group identity cannot be absent or outside the native one-byte domain");
	Check(!DedicatedCoopEstablishedArrivalMatches(7, 7, 120001, 2000, true, 0, 0) &&
		!DedicatedCoopEstablishedArrivalMatches(7, 7, 120000, 2000, false, 0, 0) &&
		!DedicatedCoopEstablishedArrivalMatches(7, 7, 120000, 2000, true, 1, 0) &&
		!DedicatedCoopEstablishedArrivalMatches(7, 7, 120000, 2000, true, 0, 2),
		"wrong timestamp, queued/repeating event, delay, or pending-deletion flag fails exact arrival proof");
}

void TestCampaignReadyGatherGate()
{
	Check(!DedicatedCoopStarterLaunchReady(0, true, true),
		"zero ready peers must never launch the mission");
	Check(!DedicatedCoopStarterLaunchReady(1, false, true),
		"the first peer must receive the bounded gather grace");
	Check(DedicatedCoopStarterLaunchReady(1, true, true),
		"one ready peer may launch after the gather grace");
	Check(DedicatedCoopStarterLaunchReady(
		DedicatedCoopStarterRosterSize, false, true),
		"a full peer roster should launch without waiting out the grace");
	Check(!DedicatedCoopStarterLaunchReady(
		DedicatedCoopStarterRosterSize, true, false),
		"peer readiness must not bypass strategic map initialization");
}

void TestEstablishedSectorSelection()
{
	const std::array<DedicatedCoopEstablishedSectorCandidate, 7> candidates{{
		{9, 9, 0, true, false},
		{4, 2, 1, true, true},
		{7, 3, 0, true, true},
		{5, 2, 0, true, true},
		{2, 2, 0, false, true},
		{0, 1, 0, true, true},
		{8, 8, -1, true, true},
	}};
	const DedicatedCoopEstablishedSectorSelection selected =
		SelectDedicatedCoopEstablishedSector(
			candidates.data(), candidates.size());
	Check(selected && selected.hostile && selected.x == 5 &&
		selected.y == 2 && selected.z == 0,
		"established entry must prefer a hostile eligible sector in canonical order");

	const std::array<DedicatedCoopEstablishedSectorCandidate, 3> reordered{{
		candidates[2], candidates[3], candidates[1]}};
	const DedicatedCoopEstablishedSectorSelection same =
		SelectDedicatedCoopEstablishedSector(
			reordered.data(), reordered.size());
	Check(same && same.x == selected.x && same.y == selected.y &&
		same.z == selected.z,
		"repository iteration order must not affect established sector entry");

	const DedicatedCoopEstablishedSectorCandidate peaceful{8, 7, 2, true,
		false};
	const DedicatedCoopEstablishedSectorSelection fallback =
		SelectDedicatedCoopEstablishedSector(&peaceful, 1);
	Check(!fallback && fallback.error ==
		DedicatedCoopEstablishedSectorSelectionError::NoEligibleSector,
		"a peaceful occupied sector must remain worldless without an exit intent");
}

void TestEstablishedActorRolePolicy()
{
	Check(DedicatedCoopEstablishedActorRoleEligible(true, false, false, false),
		"an ordinary on-foot squad actor should be eligible");
	Check(!DedicatedCoopEstablishedActorRoleEligible(false, false, false, false),
		"a non-squad duty assignment must be rejected");
	Check(!DedicatedCoopEstablishedActorRoleEligible(true, true, false, false),
		"a vehicle body must not be a direct co-op actor");
	Check(!DedicatedCoopEstablishedActorRoleEligible(true, false, true, false),
		"a vehicle driver must not be a direct co-op actor");
	Check(!DedicatedCoopEstablishedActorRoleEligible(true, false, false, true),
		"a vehicle passenger must not be a direct co-op actor");
}

DedicatedCoopPostCombatReturnEvidence ReadyPostCombatEvidence()
{
	DedicatedCoopPostCombatReturnEvidence evidence;
	evidence.missionPlayable = true;
	evidence.hostileWorldArmed = true;
	evidence.worldLoaded = true;
	evidence.gameScreen = true;
	evidence.validWorldSector = true;
	evidence.lastBattleWon = true;
	evidence.enemyInSector = false;
	evidence.enemiesRemaining = false;
	evidence.combatActive = false;
	evidence.tacticalActionsPending = false;
	evidence.interruptPending = false;
	evidence.bulletsPending = false;
	evidence.explosionsPending = false;
	evidence.dialogueActive = false;
	evidence.dialogueQueued = false;
	evidence.triggerTimerPending = false;
	evidence.autoResolveActive = false;
	evidence.autoResolvePending = false;
	evidence.meanwhileActive = false;
	evidence.meanwhilePending = false;
	evidence.tacticalTraversal = false;
	evidence.autoBandageActive = false;
	evidence.boxingActive = false;
	evidence.saveLoadActive = false;
	evidence.uiTransitionPending = false;
	evidence.customTimerPending = false;
	evidence.temporarySchedulePending = false;
	return evidence;
}

void TestPostCombatReturnRequiresEveryQuiescenceFact()
{
	const DedicatedCoopPostCombatReturnEvidence ready =
		ReadyPostCombatEvidence();
	Check(DedicatedCoopPostCombatReturnReady(ready),
		"a committed hostile victory with every queue drained should return");

	const std::array<bool DedicatedCoopPostCombatReturnEvidence::*, 6>
		requiredTrue{{
			&DedicatedCoopPostCombatReturnEvidence::missionPlayable,
			&DedicatedCoopPostCombatReturnEvidence::hostileWorldArmed,
			&DedicatedCoopPostCombatReturnEvidence::worldLoaded,
			&DedicatedCoopPostCombatReturnEvidence::gameScreen,
			&DedicatedCoopPostCombatReturnEvidence::validWorldSector,
			&DedicatedCoopPostCombatReturnEvidence::lastBattleWon,
		}};
	for (bool DedicatedCoopPostCombatReturnEvidence::* member : requiredTrue)
	{
		DedicatedCoopPostCombatReturnEvidence blocked = ready;
		blocked.*member = false;
		Check(!DedicatedCoopPostCombatReturnReady(blocked),
			"every positive post-combat fact must be required");
	}

	const std::array<bool DedicatedCoopPostCombatReturnEvidence::*, 20>
		requiredFalse{{
			&DedicatedCoopPostCombatReturnEvidence::enemyInSector,
			&DedicatedCoopPostCombatReturnEvidence::enemiesRemaining,
			&DedicatedCoopPostCombatReturnEvidence::combatActive,
			&DedicatedCoopPostCombatReturnEvidence::tacticalActionsPending,
			&DedicatedCoopPostCombatReturnEvidence::interruptPending,
			&DedicatedCoopPostCombatReturnEvidence::bulletsPending,
			&DedicatedCoopPostCombatReturnEvidence::explosionsPending,
			&DedicatedCoopPostCombatReturnEvidence::dialogueActive,
			&DedicatedCoopPostCombatReturnEvidence::dialogueQueued,
			&DedicatedCoopPostCombatReturnEvidence::triggerTimerPending,
			&DedicatedCoopPostCombatReturnEvidence::autoResolveActive,
			&DedicatedCoopPostCombatReturnEvidence::autoResolvePending,
			&DedicatedCoopPostCombatReturnEvidence::meanwhileActive,
			&DedicatedCoopPostCombatReturnEvidence::meanwhilePending,
			&DedicatedCoopPostCombatReturnEvidence::tacticalTraversal,
			&DedicatedCoopPostCombatReturnEvidence::autoBandageActive,
			&DedicatedCoopPostCombatReturnEvidence::boxingActive,
			&DedicatedCoopPostCombatReturnEvidence::saveLoadActive,
			&DedicatedCoopPostCombatReturnEvidence::uiTransitionPending,
			&DedicatedCoopPostCombatReturnEvidence::customTimerPending,
		}};
	for (bool DedicatedCoopPostCombatReturnEvidence::* member : requiredFalse)
	{
		DedicatedCoopPostCombatReturnEvidence blocked = ready;
		blocked.*member = true;
		Check(!DedicatedCoopPostCombatReturnReady(blocked),
			"every post-combat hazard must block strategic return");
	}
	DedicatedCoopPostCombatReturnEvidence temporarySchedule = ready;
	temporarySchedule.temporarySchedulePending = true;
	Check(DedicatedCoopPostCombatReturnReady(temporarySchedule),
		"native tactical teardown may retire temporary schedules before the cold checkpoint");

	Check(EvaluateDedicatedCoopPostCombatReturnStep(false, false) ==
			DedicatedCoopPostCombatReturnStep::ResumePlayable &&
		EvaluateDedicatedCoopPostCombatReturnStep(false, true) ==
			DedicatedCoopPostCombatReturnStep::ResumePlayable,
		"regressed victory evidence must reopen gameplay regardless of drain state");
	Check(EvaluateDedicatedCoopPostCombatReturnStep(true, false) ==
			DedicatedCoopPostCombatReturnStep::WaitForFreshBoundary &&
		EvaluateDedicatedCoopPostCombatReturnStep(true, true) ==
			DedicatedCoopPostCombatReturnStep::UnloadWorld,
		"stable victory evidence waits only for the final local boundary before unload");

	Check(DedicatedCoopWorldDrainRequiresStrategicCheckpoint(true, true),
		"an armed mission world drain must preserve native defeat state in a checkpoint");
	Check(!DedicatedCoopWorldDrainRequiresStrategicCheckpoint(false, true) &&
		!DedicatedCoopWorldDrainRequiresStrategicCheckpoint(true, false),
		"unarmed or non-mission world drains must retain the generic restart path");
}

void TestPostCombatCheckpointWaitsForNativeDialogue()
{
	using Reason = DedicatedCheckpointEligibilityReason;
	using Step = DedicatedCoopPostCombatCheckpointStep;
	// Native loss can unload the world before screen exit completes, then leave
	// a talking face followed by a queued death reaction on the ready map.
	Check(EvaluateDedicatedCoopPostCombatCheckpointStep(false, Reason::None, false) == Step::WaitForNativeExit &&
		EvaluateDedicatedCoopPostCombatCheckpointStep(true, Reason::DialogueActive, false) == Step::WaitForNativeExit &&
		EvaluateDedicatedCoopPostCombatCheckpointStep(true, Reason::DialogueQueueNotDrained, false) == Step::WaitForNativeExit &&
		EvaluateDedicatedCoopPostCombatCheckpointStep(true, Reason::None, false) == Step::Commit,
		"post-loss exit waits for native screen and dialogue completion before committing");
	for (unsigned raw = 0; raw <= std::numeric_limits<std::uint8_t>::max(); ++raw)
	{
		const auto reason = static_cast<Reason>(raw);
		if (reason != Reason::None && reason != Reason::DialogueActive && reason != Reason::DialogueQueueNotDrained)
			Check(EvaluateDedicatedCoopPostCombatCheckpointStep(true, reason, false) == Step::Reject,
				"other checkpoint hazards and unknown eligibility reasons cannot authorize or defer the required save");
		Check(EvaluateDedicatedCoopPostCombatCheckpointStep(false, reason, true) == Step::TimedOut &&
			EvaluateDedicatedCoopPostCombatCheckpointStep(true, reason, true) == Step::TimedOut,
			"the same deadline bounds missing map transitions and dialogue cleanup without reopening admission");
	}
}

void TestEstablishedSectorSelectionFailsClosed()
{
	Check(SelectDedicatedCoopEstablishedSector(nullptr, 1).error ==
		DedicatedCoopEstablishedSectorSelectionError::InvalidInput,
		"nonempty null established sector input must fail closed");
	const DedicatedCoopEstablishedSectorCandidate invalid{17, 1, 0, true,
		true};
	Check(SelectDedicatedCoopEstablishedSector(&invalid, 1).error ==
		DedicatedCoopEstablishedSectorSelectionError::NoEligibleSector,
		"out-of-world established sector evidence must not be selected");
}

void TestCanonicalArrivalMinute()
{
	std::uint32_t minute = 0;
	Check(ComputeDedicatedCoopStarterArrivalMinute(101400, 21600, minute) &&
		minute == 2050,
		"hire actors and delayed events must share the canonical arrival minute");
	Check(!ComputeDedicatedCoopStarterArrivalMinute(
		std::numeric_limits<std::uint32_t>::max(),
		std::numeric_limits<std::uint32_t>::max(), minute),
		"an arrival timestamp that cannot become an event second must fail");
}
}

int main()
{
	TestCanonicalCompleteSelection();
	TestInputOrderDoesNotMatter();
	TestFailClosedWithoutPartialRoster();
	TestMalformedAndUnrepresentableInput();
	TestColdStarterCampaignClassification();
	TestEstablishedResumeRequiresCompleteCohort();
	TestPendingAimHirePolicy();
	TestEstablishedPendingHireCohort();
	TestCompletedDeathPendingHireCohort();
	TestLivingWoundedRosterPolicy();
	TestEstablishedGroupTravelConsistency();
	TestEstablishedArrivalEventIdentity();
	TestCampaignReadyGatherGate();
	TestEstablishedSectorSelection();
	TestEstablishedSectorSelectionFailsClosed();
	TestEstablishedActorRolePolicy();
	TestPostCombatReturnRequiresEveryQuiescenceFact();
	TestPostCombatCheckpointWaitsForNativeDialogue();
	TestCanonicalArrivalMinute();
	if (failures != 0)
	{
		std::fprintf(stderr,
			"dedicated co-op mission policy tests: %d failure(s)\n", failures);
		return 1;
	}
	std::puts("dedicated co-op mission policy tests: ok");
	return 0;
}
