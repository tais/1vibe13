#include "Ja2/DedicatedCoopMissionPolicy.h"

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
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::EstablishedCold,
		"a cold established campaign with a live player merc must resume");
	evidence.validEstablishedMercs = 0;
	Check(ClassifyDedicatedCoopStarterCampaign(evidence) ==
		DedicatedCoopStarterCampaignState::Ineligible,
		"a cold non-initial campaign without a valid merc must fail closed");
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
	TestCampaignReadyGatherGate();
	TestEstablishedSectorSelection();
	TestEstablishedSectorSelectionFailsClosed();
	TestEstablishedActorRolePolicy();
	TestPostCombatReturnRequiresEveryQuiescenceFact();
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
