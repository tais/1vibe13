#ifndef JA2_DEDICATED_COOP_MISSION_POLICY_H
#define JA2_DEDICATED_COOP_MISSION_POLICY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

inline constexpr std::size_t DedicatedCoopStarterRosterSize = 4;
inline constexpr std::size_t MaximumDedicatedCoopStarterCandidates = 256;
inline constexpr std::size_t MaximumDedicatedCoopEstablishedSectorCandidates =
	256;

enum class DedicatedCoopStarterCampaignState : std::uint8_t
{
	Ineligible,
	UntouchedInitial,
	PreparedInitial,
	EstablishedCold
};

struct DedicatedCoopStarterCampaignEvidence
{
	bool gameJustStarted = false;
	bool initialWorldTime = false;
	bool noWorldSector = false;
	bool tacticalWorldUnloaded = false;
	bool starterEnvironmentValid = false;
	std::size_t activePlayerMercs = 0;
	std::size_t validEstablishedMercs = 0;
	std::size_t validPreparedMercs = 0;
	std::size_t unexpectedActivePlayerActors = 0;
	std::size_t delayedHiringEvents = 0;
	std::size_t matchedPreparedEvents = 0;
};

inline constexpr bool ComputeDedicatedCoopStarterArrivalMinute(
	std::uint32_t gameStartingSeconds,
	std::uint32_t firstArrivalDelaySeconds,
	std::uint32_t& minute) noexcept
{
	const std::uint64_t seconds =
		static_cast<std::uint64_t>(gameStartingSeconds) +
		static_cast<std::uint64_t>(firstArrivalDelaySeconds);
	if (seconds > std::numeric_limits<std::uint32_t>::max()) return false;
	const std::uint64_t computed = seconds / 60u;
	if (computed >
		static_cast<std::uint64_t>(
			std::numeric_limits<std::uint32_t>::max()) / 60u)
	{
		return false;
	}
	minute = static_cast<std::uint32_t>(computed);
	return true;
}

// Durable checkpoint classes are recognized only from save-owned gameplay
// facts. The classifier never repairs an ambiguous state: an empty initial
// campaign must have no delayed hires, a prepared one must have exactly one
// validated arrival event for every roster member, and only an unambiguously
// non-initial cold campaign with a live player merc is considered established.
inline constexpr DedicatedCoopStarterCampaignState
ClassifyDedicatedCoopStarterCampaign(
	const DedicatedCoopStarterCampaignEvidence& evidence) noexcept
{
	if (!evidence.noWorldSector || !evidence.tacticalWorldUnloaded)
	{
		return DedicatedCoopStarterCampaignState::Ineligible;
	}
	// Either initial marker by itself is still an ambiguous initial campaign.
	// It may not fall through into the broader established-campaign branch.
	if (evidence.gameJustStarted || evidence.initialWorldTime)
	{
		if (!evidence.gameJustStarted || !evidence.initialWorldTime ||
			!evidence.starterEnvironmentValid ||
			evidence.unexpectedActivePlayerActors != 0)
		{
			return DedicatedCoopStarterCampaignState::Ineligible;
		}
		if (evidence.activePlayerMercs == 0 &&
			evidence.validPreparedMercs == 0 &&
			evidence.delayedHiringEvents == 0 &&
			evidence.matchedPreparedEvents == 0)
		{
			return DedicatedCoopStarterCampaignState::UntouchedInitial;
		}
		if (evidence.activePlayerMercs == DedicatedCoopStarterRosterSize &&
			evidence.validPreparedMercs == DedicatedCoopStarterRosterSize &&
			evidence.delayedHiringEvents == DedicatedCoopStarterRosterSize &&
			evidence.matchedPreparedEvents == DedicatedCoopStarterRosterSize)
		{
			return DedicatedCoopStarterCampaignState::PreparedInitial;
		}
		return DedicatedCoopStarterCampaignState::Ineligible;
	}
	if (evidence.activePlayerMercs != 0 &&
		evidence.validEstablishedMercs != 0)
	{
		return DedicatedCoopStarterCampaignState::EstablishedCold;
	}
	return DedicatedCoopStarterCampaignState::Ineligible;
}

inline constexpr bool DedicatedCoopStarterLaunchReady(
	std::size_t campaignReadyPeers,
	bool gatherGraceElapsed,
	bool strategicMapReady) noexcept
{
	return strategicMapReady && campaignReadyPeers != 0 &&
		(campaignReadyPeers >= DedicatedCoopStarterRosterSize ||
			gatherGraceElapsed);
}

// Direct co-op control is deliberately narrower than the legacy
// OK_CONTROLLABLE_MERC predicate.  The current wire protocol has no vehicle
// enter/leave command, so a vehicle body, driver, passenger, or actor assigned
// to the VEHICLE duty must never become a tactical authority subject.
inline constexpr bool DedicatedCoopEstablishedActorRoleEligible(
	bool ordinarySquadAssignment,
	bool vehicleBody,
	bool driver,
	bool passenger) noexcept
{
	return ordinarySquadAssignment && !vehicleBody && !driver && !passenger;
}

struct DedicatedCoopPostCombatReturnEvidence
{
	bool missionPlayable = false;
	bool hostileWorldArmed = false;
	bool worldLoaded = false;
	bool gameScreen = false;
	bool validWorldSector = false;
	bool lastBattleWon = false;
	bool enemyInSector = true;
	bool enemiesRemaining = true;
	bool combatActive = true;
	bool tacticalActionsPending = true;
	bool interruptPending = true;
	bool bulletsPending = true;
	bool explosionsPending = true;
	bool dialogueActive = true;
	bool dialogueQueued = true;
	bool triggerTimerPending = true;
	bool autoResolveActive = true;
	bool autoResolvePending = true;
	bool meanwhileActive = true;
	bool meanwhilePending = true;
	bool tacticalTraversal = true;
	bool autoBandageActive = true;
	bool boxingActive = true;
	bool saveLoadActive = true;
	bool uiTransitionPending = true;
	bool customTimerPending = true;
	bool temporarySchedulePending = true;
};

// This is intentionally stricter than the victory flag alone.  The flag is
// serialized and can be stale on resume; the process-local hostile-world arm
// and the full quiescence proof make the return a one-world committed action.
// Temporary schedules are the one deliberate exception: the native strategic
// unload owns and destroys that tactical-only state, and checkpoint eligibility
// verifies that it is gone after the world has been torn down.
inline constexpr bool DedicatedCoopPostCombatReturnReady(
	const DedicatedCoopPostCombatReturnEvidence& evidence) noexcept
{
	return evidence.missionPlayable && evidence.hostileWorldArmed &&
		evidence.worldLoaded && evidence.gameScreen &&
		evidence.validWorldSector && evidence.lastBattleWon &&
		!evidence.enemyInSector && !evidence.enemiesRemaining &&
		!evidence.combatActive && !evidence.tacticalActionsPending &&
		!evidence.interruptPending && !evidence.bulletsPending &&
		!evidence.explosionsPending && !evidence.dialogueActive &&
		!evidence.dialogueQueued && !evidence.triggerTimerPending &&
		!evidence.autoResolveActive && !evidence.autoResolvePending &&
		!evidence.meanwhileActive && !evidence.meanwhilePending &&
		!evidence.tacticalTraversal && !evidence.autoBandageActive &&
		!evidence.boxingActive && !evidence.saveLoadActive &&
		!evidence.uiTransitionPending && !evidence.customTimerPending;
}

enum class DedicatedCoopPostCombatReturnStep : std::uint8_t
{
	ResumePlayable,
	WaitForFreshBoundary,
	UnloadWorld
};

// Admission is already stopped when this decision is evaluated. Gameplay
// evidence is therefore qualitatively different from queue drainage: a
// regressed victory/world predicate must reopen the live session, while a
// still-valid victory may wait locally for the final authoritative queues to
// drain before mutation.
inline constexpr DedicatedCoopPostCombatReturnStep
EvaluateDedicatedCoopPostCombatReturnStep(
	bool gameplayEvidenceReady,
	bool freshLocalBoundary) noexcept
{
	if (!gameplayEvidenceReady)
		return DedicatedCoopPostCombatReturnStep::ResumePlayable;
	return freshLocalBoundary
		? DedicatedCoopPostCombatReturnStep::UnloadWorld
		: DedicatedCoopPostCombatReturnStep::WaitForFreshBoundary;
}

// Native JA2 owns the defeat teardown and can retire the tactical world before
// the dedicated runtime observes a stable defeat predicate.  Once a launched
// hostile mission loses that world, admission must remain closed until the
// resulting strategic state has been checkpointed.  Otherwise the runtime
// reopens a worldless tactical session while still considering it playable.
inline constexpr bool DedicatedCoopWorldDrainRequiresStrategicCheckpoint(
	bool hostileWorldArmed,
	bool tacticalMissionActive) noexcept
{
	return hostileWorldArmed && tacticalMissionActive;
}

struct DedicatedCoopEstablishedSectorCandidate
{
	std::int16_t x = 0;
	std::int16_t y = 0;
	std::int8_t z = -1;
	bool eligible = false;
	bool hostile = false;
};

enum class DedicatedCoopEstablishedSectorSelectionError : std::uint8_t
{
	None,
	InvalidInput,
	NoEligibleSector
};

struct DedicatedCoopEstablishedSectorSelection
{
	DedicatedCoopEstablishedSectorSelectionError error =
		DedicatedCoopEstablishedSectorSelectionError::InvalidInput;
	std::int16_t x = 0;
	std::int16_t y = 0;
	std::int8_t z = -1;
	bool hostile = false;

	explicit constexpr operator bool() const noexcept
	{
		return error == DedicatedCoopEstablishedSectorSelectionError::None;
	}
};

// Chooses one hostile save-owned sector that contains a tactically eligible
// player actor. Peaceful sectors stay worldless until a strategic intent/exit
// protocol exists; ties use canonical surface/depth, row, then column order and
// are independent of repository iteration order.
inline DedicatedCoopEstablishedSectorSelection
SelectDedicatedCoopEstablishedSector(
	const DedicatedCoopEstablishedSectorCandidate* candidates,
	std::size_t candidateCount) noexcept
{
	DedicatedCoopEstablishedSectorSelection selected;
	if ((candidateCount != 0 && candidates == nullptr) ||
		candidateCount > MaximumDedicatedCoopEstablishedSectorCandidates)
	{
		return selected;
	}

	bool found = false;
	for (std::size_t index = 0; index < candidateCount; ++index)
	{
		const DedicatedCoopEstablishedSectorCandidate& candidate =
			candidates[index];
		if (!candidate.eligible || !candidate.hostile ||
			candidate.x < 1 || candidate.x > 16 ||
			candidate.y < 1 || candidate.y > 16 ||
			candidate.z < 0 || candidate.z > 3)
		{
			continue;
		}
		const bool better = !found || candidate.z < selected.z ||
			(candidate.z == selected.z &&
				(candidate.y < selected.y ||
					(candidate.y == selected.y &&
						candidate.x < selected.x)));
		if (!better) continue;
		selected.x = candidate.x;
		selected.y = candidate.y;
		selected.z = candidate.z;
		selected.hostile = candidate.hostile;
		found = true;
	}
	selected.error = found
		? DedicatedCoopEstablishedSectorSelectionError::None
		: DedicatedCoopEstablishedSectorSelectionError::NoEligibleSector;
	return selected;
}

struct DedicatedCoopStarterCandidate
{
	std::uint8_t profileId = 0;
	std::uint32_t hireCharge = 0;
	bool aimProfile = false;
	bool hireable = false;
	bool healthy = false;
};

enum class DedicatedCoopStarterSelectionError : std::uint8_t
{
	None,
	InvalidInput,
	TeamCapacityTooSmall,
	InsufficientCandidates,
	InsufficientFunds
};

struct DedicatedCoopStarterSelection
{
	DedicatedCoopStarterSelectionError error =
		DedicatedCoopStarterSelectionError::InvalidInput;
	std::array<std::uint8_t, DedicatedCoopStarterRosterSize> profiles{};
	std::uint64_t totalCharge = 0;

	explicit constexpr operator bool() const noexcept
	{
		return error == DedicatedCoopStarterSelectionError::None;
	}
};

// Choose a complete starter roster before legacy hiring mutates any campaign
// state. Duplicate profile records collapse to their lowest declared charge;
// the canonical order is charge, then profile id. A caller either receives the
// entire fixed-size roster within budget or no profiles at all.
inline DedicatedCoopStarterSelection SelectDedicatedCoopStarterRoster(
	const DedicatedCoopStarterCandidate* candidates,
	std::size_t candidateCount,
	std::uint64_t availableFunds,
	std::size_t availableTeamSlots) noexcept
{
	DedicatedCoopStarterSelection result;
	if ((candidateCount != 0 && candidates == nullptr) ||
		candidateCount > MaximumDedicatedCoopStarterCandidates)
	{
		return result;
	}
	if (availableTeamSlots < DedicatedCoopStarterRosterSize)
	{
		result.error =
			DedicatedCoopStarterSelectionError::TeamCapacityTooSmall;
		return result;
	}

	struct EligibleCandidate
	{
		std::uint8_t profileId = 0;
		std::uint32_t hireCharge = 0;
	};
	std::array<EligibleCandidate, MaximumDedicatedCoopStarterCandidates>
		eligible{};
	std::size_t eligibleCount = 0;

	// Scan profile ids rather than trusting content order. This both deduplicates
	// malformed availability lists and makes selection independent of UI sort.
	for (std::uint16_t profile = 0;
		profile <= std::numeric_limits<std::uint8_t>::max(); ++profile)
	{
		bool found = false;
		std::uint32_t lowestCharge =
			std::numeric_limits<std::uint32_t>::max();
		for (std::size_t index = 0; index < candidateCount; ++index)
		{
			const DedicatedCoopStarterCandidate& candidate = candidates[index];
			if (candidate.profileId != profile || !candidate.aimProfile ||
				!candidate.hireable || !candidate.healthy ||
				candidate.hireCharge >
					static_cast<std::uint32_t>(
						std::numeric_limits<std::int32_t>::max()))
			{
				continue;
			}
			found = true;
			if (candidate.hireCharge < lowestCharge)
				lowestCharge = candidate.hireCharge;
		}
		if (found)
			eligible[eligibleCount++] = {
				static_cast<std::uint8_t>(profile), lowestCharge};
	}

	if (eligibleCount < DedicatedCoopStarterRosterSize)
	{
		result.error =
			DedicatedCoopStarterSelectionError::InsufficientCandidates;
		return result;
	}

	// The bounded list makes insertion sort simple, allocation-free, and stable.
	for (std::size_t index = 1; index < eligibleCount; ++index)
	{
		const EligibleCandidate value = eligible[index];
		std::size_t destination = index;
		while (destination != 0)
		{
			const EligibleCandidate& previous = eligible[destination - 1];
			if (previous.hireCharge < value.hireCharge ||
				(previous.hireCharge == value.hireCharge &&
					previous.profileId < value.profileId))
			{
				break;
			}
			eligible[destination] = previous;
			--destination;
		}
		eligible[destination] = value;
	}

	std::uint64_t total = 0;
	for (std::size_t index = 0;
		index < DedicatedCoopStarterRosterSize; ++index)
	{
		result.profiles[index] = eligible[index].profileId;
		total += eligible[index].hireCharge;
	}
	result.totalCharge = total;
	if (total > availableFunds)
	{
		result.profiles = {};
		result.error = DedicatedCoopStarterSelectionError::InsufficientFunds;
		return result;
	}

	result.error = DedicatedCoopStarterSelectionError::None;
	return result;
}

#endif
