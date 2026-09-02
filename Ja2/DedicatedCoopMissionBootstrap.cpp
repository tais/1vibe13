#include "DedicatedCoopMissionBootstrap.h"

#include "CampaignApplicationPolicy.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "TacticalWorldAdapter.h"

#include "AimFacialIndex.h"
#include "Game Clock.h"
#include "Game Event Hook.h"
#include "Game Events.h"
#include "LaptopSave.h"
#include "Map Screen Interface.h"
#include "Map Screen Helicopter.h"
#include "Merc Contract.h"
#include "Merc Hiring.h"
#include "Overhead.h"
#include "Queen Command.h"
#include "Soldier Profile.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorEmploymentTypes.h"
#include "TacticalActorStateFlags.h"
#include "Soldier macros.h"
#include "Tactical Save.h"
#include "Assignments.h"
#include "aim.h"
#include "finances.h"
#include "history.h"
#include "screenids.h"
#include "strategic.h"
#include "strategicmap.h"
#include "mapscreen.h"

#include "CoopAdmission.h"

#include <array>
#include <cstdint>
#include <limits>

extern BOOLEAN fInMapMode;

namespace
{
constexpr std::int16_t StarterContractDays = 7;

DedicatedCoopMissionBootstrapError TranslateSelectionError(
	DedicatedCoopStarterSelectionError error) noexcept
{
	switch (error)
	{
		case DedicatedCoopStarterSelectionError::None:
			return DedicatedCoopMissionBootstrapError::None;
		case DedicatedCoopStarterSelectionError::TeamCapacityTooSmall:
			return DedicatedCoopMissionBootstrapError::TeamCapacityTooSmall;
		case DedicatedCoopStarterSelectionError::InsufficientCandidates:
			return DedicatedCoopMissionBootstrapError::
				InsufficientStarterCandidates;
		case DedicatedCoopStarterSelectionError::InsufficientFunds:
			return DedicatedCoopMissionBootstrapError::InsufficientFunds;
		case DedicatedCoopStarterSelectionError::InvalidInput:
			return DedicatedCoopMissionBootstrapError::StarterChargeInvalid;
	}
	return DedicatedCoopMissionBootstrapError::StarterChargeInvalid;
}

bool ValidInitialArrivalSector() noexcept
{
	return gGameExternalOptions.ubDefaultArrivalSectorX >= 1 &&
		gGameExternalOptions.ubDefaultArrivalSectorX <= 16 &&
		gGameExternalOptions.ubDefaultArrivalSectorY >= 1 &&
		gGameExternalOptions.ubDefaultArrivalSectorY <= 16 &&
		gsMercArriveSectorX ==
			gGameExternalOptions.ubDefaultArrivalSectorX &&
		gsMercArriveSectorY ==
			gGameExternalOptions.ubDefaultArrivalSectorY;
}

bool ExpectedStarterArrivalMinute(std::uint32_t& minute) noexcept
{
	return ComputeDedicatedCoopStarterArrivalMinute(
		gGameExternalOptions.iGameStartingTime,
		gGameExternalOptions.iFirstArrivalDelay, minute);
}

bool EligibleEstablishedActor(
	TacticalActor& actor,
	SoldierID expectedId) noexcept
{
	const std::int16_t x = actor.deployment().sectorX();
	const std::int16_t y = actor.deployment().sectorY();
	const std::int8_t z = actor.deployment().sectorZ();
	const std::int8_t assignment = actor.assignment().current();
	const std::uint32_t flags = actor.status().flags();
	return actor.identity().id() == expectedId &&
		actor.roster().active() != FALSE &&
		actor.roster().team() == gbPlayerNum &&
		actor.vitals().health() >= OKLIFE &&
		DedicatedCoopEstablishedActorRoleEligible(
			assignment < ON_DUTY,
			(flags & SOLDIER_VEHICLE) != 0,
			(flags & SOLDIER_DRIVER) != 0,
			(flags & SOLDIER_PASSENGER) != 0) &&
		actor.skillState().cooldown(SOLDIER_COOLDOWN_CRYO) == 0 &&
		!actor.deployment().isBetweenSectors() &&
		!SoldierAboardAirborneHeli(&actor) &&
		x >= 1 && x <= 16 && y >= 1 && y <= 16 && z >= 0 && z <= 3 &&
		GetSectorFlagStatus(x, y, static_cast<std::uint8_t>(z),
			SF_ALREADY_VISITED) != FALSE;
}

bool CalculateHireCharge(
	std::uint8_t profileId,
	std::uint32_t& total,
	std::uint32_t& medicalDeposit) noexcept
{
	const MERCPROFILESTRUCT& profile = gMercProfiles[profileId];
	medicalDeposit = profile.bMedicalDeposit
		? profile.sMedicalDepositAmount
		: 0;
	std::uint64_t charge = profile.uiWeeklySalary;
	const CampaignApplicationPolicy campaignPolicy(
		GetGameContext().capabilities());
	if (!campaignPolicy.usesUnfinishedBusinessContent())
	{
		charge += medicalDeposit;
		charge += profile.usOptionalGearCost;
	}
	if (charge > static_cast<std::uint64_t>(
			std::numeric_limits<std::int32_t>::max()) ||
		medicalDeposit > charge)
	{
		return false;
	}
	total = static_cast<std::uint32_t>(charge);
	return true;
}

bool HireStarter(
	std::uint8_t profileId,
	std::uint32_t totalCharge,
	std::uint32_t arrivalMinute) noexcept
{
	MERCPROFILESTRUCT& profile = gMercProfiles[profileId];
	const std::uint32_t medicalDeposit = profile.bMedicalDeposit
		? profile.sMedicalDepositAmount
		: 0;
	if (medicalDeposit > totalCharge) return false;

	MERC_HIRE_STRUCT hire{};
	hire.ubProfileID = profileId;
	hire.sSectorX = gsMercArriveSectorX;
	hire.sSectorY = gsMercArriveSectorY;
	hire.bSectorZ = 0;
	hire.iTotalContractLength = StarterContractDays;
	hire.fCopyProfileItemsOver = TRUE;
	// HireMerc copies this into the actor before its initial-game branch
	// canonicalizes the hire request and schedules the delayed-arrival event.
	// Supplying that same canonical minute keeps actor and event durable state
	// identical across the first checkpoint and a later cold reload.
	hire.uiTimeTillMercArrives = arrivalMinute;
	hire.ubInsertionCode = INSERTION_CODE_ARRIVING_GAME;
	hire.fUseLandingZoneForArrival = TRUE;
	if (HireMerc(&hire) != MERC_HIRE_OK) return false;

	// Mirror a normal A.I.M. equipment purchase only after the hire succeeds.
	// The preflight retained the original equipment cost for the transaction.
	profile.ubMiscFlags |= PROFILE_MISC_FLAG_ALREADY_USED_ITEMS;
	profile.usOptionalGearCost = 0;
	const std::uint32_t hiredCharge = totalCharge - medicalDeposit;
	AddTransactionToPlayersBook(HIRED_MERC, profileId, GetWorldTotalMin(),
		-static_cast<std::int32_t>(hiredCharge));
	if (medicalDeposit != 0)
	{
		AddTransactionToPlayersBook(MEDICAL_DEPOSIT, profileId,
			GetWorldTotalMin(), -static_cast<std::int32_t>(medicalDeposit));
	}
	AddHistoryToPlayersLog(HISTORY_HIRED_MERC_FROM_AIM, profileId,
		GetWorldTotalMin(), -1, -1);
	return true;
}
}

DedicatedCoopStarterCampaignState
InspectDedicatedCoopStarterCampaign() noexcept
{
	try
	{
		DedicatedCoopStarterCampaignEvidence evidence;
		evidence.gameJustStarted =
			gTacticalStatus.fDidGameJustStart != FALSE;
		evidence.initialWorldTime = GetWorldTotalSeconds() ==
			static_cast<std::uint32_t>(
				gGameExternalOptions.iGameStartingTime);
		evidence.noWorldSector =
			gWorldSectorX == 0 && gWorldSectorY == 0 && gbWorldSectorZ == -1;
		evidence.tacticalWorldUnloaded = !IsJa2TacticalWorldLoaded();
		evidence.activePlayerMercs = NumberOfMercsOnPlayerTeam();

		std::uint32_t expectedArrivalMinute = 0;
		evidence.starterEnvironmentValid =
			ValidInitialArrivalSector() &&
			ExpectedStarterArrivalMinute(expectedArrivalMinute) &&
			NumEnemiesInAnySector(
				gGameExternalOptions.ubDefaultArrivalSectorX,
				gGameExternalOptions.ubDefaultArrivalSectorY, 0) != 0;

		std::array<std::uint16_t, DedicatedCoopStarterRosterSize> actorIds{};
		std::array<std::uint8_t, DedicatedCoopStarterRosterSize> profiles{};
		std::array<std::size_t, DedicatedCoopStarterRosterSize> eventMatches{};
		std::size_t inspectedActors = 0;
		for (SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
			id <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++id)
		{
			TacticalActor* const actor = GetJa2SoldierRepository().resolve(id);
			if (actor == nullptr || actor->roster().active() == FALSE) continue;
			if (actor->roster().team() != gbPlayerNum ||
				(actor->status().flags() & SOLDIER_VEHICLE) != 0)
			{
				++evidence.unexpectedActivePlayerActors;
				continue;
			}
			if (EligibleEstablishedActor(*actor, id))
			{
				++evidence.validEstablishedMercs;
			}
			if (inspectedActors == DedicatedCoopStarterRosterSize) continue;
			const std::uint8_t profileId = actor->identity().profile();
			bool duplicateProfile = false;
			for (std::size_t index = 0; index < inspectedActors; ++index)
				duplicateProfile = duplicateProfile ||
					profiles[index] == profileId;
			const bool validProfile = profileId < NUM_PROFILES &&
				!duplicateProfile &&
				gMercProfiles[profileId].Type == PROFILETYPE_AIM &&
				gMercProfiles[profileId].bMercStatus ==
					MERC_HIRED_BUT_NOT_ARRIVED_YET &&
				(gMercProfiles[profileId].ubMiscFlags &
					PROFILE_MISC_FLAG_ALREADY_USED_ITEMS) != 0 &&
				gMercProfiles[profileId].usOptionalGearCost == 0;
			const bool validActor = evidence.starterEnvironmentValid &&
				validProfile &&
				actor->roster().inSector() == FALSE &&
				actor->vitals().health() >= OKLIFE &&
				actor->assignment().current() == IN_TRANSIT &&
				actor->employment().totalLength() == StarterContractDays &&
				actor->employment().mercenaryType() == MERC_TYPE__AIM_MERC &&
				actor->employment().lastContractType() ==
					CONTRACT_EXTEND_1_WEEK &&
				actor->deployment().sectorX() == gsMercArriveSectorX &&
				actor->deployment().sectorY() == gsMercArriveSectorY &&
				actor->deployment().sectorZ() == 0 &&
				actor->deployment().arrivalTime() == expectedArrivalMinute &&
				actor->deployment().usesLandingZoneForArrival() &&
				(actor->deployment().strategicInsertionCode() ==
						INSERTION_CODE_CHOPPER ||
					actor->deployment().strategicInsertionCode() ==
						INSERTION_CODE_GRIDNO ||
					actor->deployment().strategicInsertionCode() ==
						INSERTION_CODE_ARRIVING_GAME);
			actorIds[inspectedActors] = actor->identity().id().i;
			profiles[inspectedActors] = profileId;
			++inspectedActors;
			if (validActor) ++evidence.validPreparedMercs;
		}

		for (const STRATEGICEVENT* event = GetStrategicEventListHead();
			event != nullptr; event = event->next)
		{
			if (event->ubCallbackID != EVENT_DELAYED_HIRING_OF_MERC) continue;
			++evidence.delayedHiringEvents;
			for (std::size_t index = 0; index < inspectedActors; ++index)
			{
				if (event->uiParam == actorIds[index] &&
					event->uiTimeStamp == expectedArrivalMinute * NUM_SEC_IN_MIN &&
					event->ubEventType == ONETIME_EVENT &&
					event->uiTimeOffset == 0 && event->ubFlags == 0)
				{
					++eventMatches[index];
				}
			}
		}
		for (std::size_t index = 0; index < inspectedActors; ++index)
			if (eventMatches[index] == 1)
				++evidence.matchedPreparedEvents;

		return ClassifyDedicatedCoopStarterCampaign(evidence);
	}
	catch (...)
	{
		return DedicatedCoopStarterCampaignState::Ineligible;
	}
}

const char* DedicatedCoopStarterCampaignStateName(
	DedicatedCoopStarterCampaignState state) noexcept
{
	switch (state)
	{
		case DedicatedCoopStarterCampaignState::Ineligible:
			return "ineligible";
		case DedicatedCoopStarterCampaignState::UntouchedInitial:
			return "untouched-initial";
		case DedicatedCoopStarterCampaignState::PreparedInitial:
			return "prepared-initial";
		case DedicatedCoopStarterCampaignState::EstablishedCold:
			return "established-cold";
	}
	return "unknown";
}

DedicatedCoopMissionPreparationResult
PrepareDedicatedCoopStarterMission() noexcept
{
	DedicatedCoopMissionPreparationResult result;
	try
	{
		if (!gTacticalStatus.fDidGameJustStart ||
			NumberOfMercsOnPlayerTeam() != 0 ||
			GetWorldTotalSeconds() !=
				static_cast<std::uint32_t>(
					gGameExternalOptions.iGameStartingTime) ||
			gWorldSectorX != 0 || gWorldSectorY != 0 || gbWorldSectorZ != -1)
		{
			return result;
		}
		if (IsJa2TacticalWorldLoaded())
		{
			result.error = DedicatedCoopMissionBootstrapError::
				TacticalWorldAlreadyLoaded;
			return result;
		}
		if (!ValidInitialArrivalSector())
		{
			result.error =
				DedicatedCoopMissionBootstrapError::InvalidArrivalSector;
			return result;
		}
		if (NumEnemiesInAnySector(
				gGameExternalOptions.ubDefaultArrivalSectorX,
				gGameExternalOptions.ubDefaultArrivalSectorY, 0) == 0)
		{
			result.error =
				DedicatedCoopMissionBootstrapError::NoHostileEncounter;
			return result;
		}
		if (InspectDedicatedCoopStarterCampaign() !=
			DedicatedCoopStarterCampaignState::UntouchedInitial)
		{
			return result;
		}

		std::array<DedicatedCoopStarterCandidate,
			MaximumDedicatedCoopStarterCandidates> candidates{};
		std::array<std::uint32_t, MaximumDedicatedCoopStarterCandidates>
			chargesByProfile{};
		std::size_t candidateCount = 0;
		for (std::size_t index = 0;
			index < static_cast<std::size_t>(MAX_NUMBER_MERCS); ++index)
		{
			const std::uint8_t profileId = AimMercArray[index];
			if (profileId == std::numeric_limits<std::uint8_t>::max()) continue;
			std::uint32_t charge = 0;
			std::uint32_t medicalDeposit = 0;
			const bool chargeValid =
				CalculateHireCharge(profileId, charge, medicalDeposit);
			chargesByProfile[profileId] = charge;
			DedicatedCoopStarterCandidate& candidate =
				candidates[candidateCount++];
			candidate.profileId = profileId;
			candidate.hireCharge = charge;
			candidate.aimProfile =
				gMercProfiles[profileId].Type == PROFILETYPE_AIM;
			candidate.hireable = chargeValid && IsMercHireable(profileId);
			candidate.healthy = gMercProfiles[profileId].bLife >= OKLIFE;
		}

		const std::size_t teamCapacity = OUR_TEAM_SIZE_NO_VEHICLE > 0
			? static_cast<std::size_t>(OUR_TEAM_SIZE_NO_VEHICLE)
			: 0;
		const std::uint64_t availableFunds =
			LaptopSaveInfo.iCurrentBalance >= 0
			? static_cast<std::uint64_t>(LaptopSaveInfo.iCurrentBalance)
			: 0;
		const DedicatedCoopStarterSelection selection =
			SelectDedicatedCoopStarterRoster(candidates.data(), candidateCount,
				availableFunds, teamCapacity);
		if (!selection)
		{
			result.error = TranslateSelectionError(selection.error);
			return result;
		}

		static_assert(DedicatedCoopStarterRosterSize ==
			CoopSession::MaximumAuthorityPeers,
			"starter roster must cover every admitted authority peer");
		std::uint32_t arrivalMinute = 0;
		if (!ExpectedStarterArrivalMinute(arrivalMinute))
		{
			result.error = DedicatedCoopMissionBootstrapError::
				InvalidInitialCampaign;
			return result;
		}
		for (const std::uint8_t profileId : selection.profiles)
		{
			if (!HireStarter(
					profileId, chargesByProfile[profileId], arrivalMinute))
			{
				result.error = DedicatedCoopMissionBootstrapError::HireFailed;
				return result;
			}
		}
		if (NumberOfMercsOnPlayerTeam() != DedicatedCoopStarterRosterSize ||
			InspectDedicatedCoopStarterCampaign() !=
				DedicatedCoopStarterCampaignState::PreparedInitial)
		{
			result.error = DedicatedCoopMissionBootstrapError::HireFailed;
			return result;
		}

		result.error = DedicatedCoopMissionBootstrapError::None;
		result.profiles = selection.profiles;
		result.totalCharge = selection.totalCharge;
		return result;
	}
	catch (...)
	{
		result.error = DedicatedCoopMissionBootstrapError::HireFailed;
		return result;
	}
}

bool IsDedicatedCoopStarterMissionMapReady() noexcept
{
	return GetCurrentScreen() == MAP_SCREEN && fInMapMode != FALSE &&
		!IsJa2TacticalWorldLoaded();
}

DedicatedCoopMissionBootstrapError
LaunchDedicatedCoopStarterMission() noexcept
{
	try
	{
		if (!IsDedicatedCoopStarterMissionMapReady())
			return DedicatedCoopMissionBootstrapError::MapScreenNotReady;
		if (!gTacticalStatus.fDidGameJustStart ||
			NumberOfMercsOnPlayerTeam() != DedicatedCoopStarterRosterSize)
		{
			return DedicatedCoopMissionBootstrapError::InvalidInitialCampaign;
		}
		if (!ValidInitialArrivalSector())
			return DedicatedCoopMissionBootstrapError::InvalidArrivalSector;
		if (NumEnemiesInAnySector(
				gGameExternalOptions.ubDefaultArrivalSectorX,
				gGameExternalOptions.ubDefaultArrivalSectorY, 0) == 0)
		{
			return DedicatedCoopMissionBootstrapError::NoHostileEncounter;
		}
		if (!HandleTimeCompressWithTeamJackedInAndGearedToGo())
			return DedicatedCoopMissionBootstrapError::TacticalEntryFailed;
		if (!IsJa2TacticalWorldLoaded() ||
			gWorldSectorX != gGameExternalOptions.ubDefaultArrivalSectorX ||
			gWorldSectorY != gGameExternalOptions.ubDefaultArrivalSectorY ||
			gbWorldSectorZ != 0 || NumEnemyInSector() == 0)
		{
			return DedicatedCoopMissionBootstrapError::
				TacticalPostconditionFailed;
		}
		return DedicatedCoopMissionBootstrapError::None;
	}
	catch (...)
	{
		return DedicatedCoopMissionBootstrapError::TacticalEntryFailed;
	}
}

DedicatedCoopMissionBootstrapError
LaunchDedicatedCoopEstablishedMission() noexcept
{
	try
	{
		if (!IsDedicatedCoopStarterMissionMapReady())
			return DedicatedCoopMissionBootstrapError::MapScreenNotReady;

		std::array<DedicatedCoopEstablishedSectorCandidate,
			MaximumDedicatedCoopEstablishedSectorCandidates> candidates{};
		std::size_t candidateCount = 0;
		bool hasEligibleActor = false;
		for (SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
			id <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++id)
		{
			TacticalActor* const actor = GetJa2SoldierRepository().resolve(id);
			if (actor == nullptr) continue;
			if (candidateCount == candidates.size())
				return DedicatedCoopMissionBootstrapError::
					NoEligibleEstablishedSector;
			DedicatedCoopEstablishedSectorCandidate& candidate =
				candidates[candidateCount++];
			candidate.x = actor->deployment().sectorX();
			candidate.y = actor->deployment().sectorY();
			candidate.z = actor->deployment().sectorZ();
			candidate.eligible = EligibleEstablishedActor(*actor, id);
			hasEligibleActor = hasEligibleActor || candidate.eligible;
			candidate.hostile = candidate.eligible &&
				NumHostilesInSector(candidate.x, candidate.y, candidate.z) != 0;
		}

		const DedicatedCoopEstablishedSectorSelection selected =
			SelectDedicatedCoopEstablishedSector(
				candidates.data(), candidateCount);
		if (!selected)
			return hasEligibleActor
				? DedicatedCoopMissionBootstrapError::NoHostileEncounter
				: DedicatedCoopMissionBootstrapError::
					NoEligibleEstablishedSector;

		// CanGoToTacticalInSector consults the canonical map selection as well as
		// the requested coordinates. Publish the server-selected coordinate first,
		// then revalidate through the ordinary strategic gate before loading.
		ChangeSelectedMapSector(selected.x, selected.y, selected.z);
		if (!CanGoToTacticalInSector(selected.x, selected.y,
			static_cast<std::uint8_t>(selected.z)))
		{
			return DedicatedCoopMissionBootstrapError::
				NoEligibleEstablishedSector;
		}
		if (!SetCurrentWorldSector(selected.x, selected.y, selected.z))
			return DedicatedCoopMissionBootstrapError::TacticalEntryFailed;
		if (!IsJa2TacticalWorldLoaded() || gWorldSectorX != selected.x ||
			gWorldSectorY != selected.y || gbWorldSectorZ != selected.z ||
			CountDedicatedCoopControllableActors() == 0)
		{
			return DedicatedCoopMissionBootstrapError::
				TacticalPostconditionFailed;
		}
		return DedicatedCoopMissionBootstrapError::None;
	}
	catch (...)
	{
		return DedicatedCoopMissionBootstrapError::TacticalEntryFailed;
	}
}

std::size_t CountDedicatedCoopControllableActors() noexcept
{
	if (!IsJa2TacticalWorldLoaded()) return 0;
	std::size_t count = 0;
	for (SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
		id <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++id)
	{
		TacticalActor* const actor = GetJa2SoldierRepository().resolve(id);
		if (actor == nullptr || !OK_CONTROLLABLE_MERC(actor)) continue;
		const std::uint32_t flags = actor->status().flags();
		if (DedicatedCoopEstablishedActorRoleEligible(
			actor->assignment().current() < ON_DUTY,
			(flags & SOLDIER_VEHICLE) != 0,
			(flags & SOLDIER_DRIVER) != 0,
			(flags & SOLDIER_PASSENGER) != 0))
		{
			++count;
		}
	}
	return count;
}

const char* DedicatedCoopMissionBootstrapErrorName(
	DedicatedCoopMissionBootstrapError error) noexcept
{
	switch (error)
	{
		case DedicatedCoopMissionBootstrapError::None: return "none";
		case DedicatedCoopMissionBootstrapError::InvalidInitialCampaign:
			return "campaign is not the untouched initial strategic state";
		case DedicatedCoopMissionBootstrapError::TacticalWorldAlreadyLoaded:
			return "tactical world already loaded";
		case DedicatedCoopMissionBootstrapError::InvalidArrivalSector:
			return "invalid configured arrival sector";
		case DedicatedCoopMissionBootstrapError::NoHostileEncounter:
			return "configured arrival sector has no hostile encounter";
		case DedicatedCoopMissionBootstrapError::TeamCapacityTooSmall:
			return "player team cannot hold the complete starter roster";
		case DedicatedCoopMissionBootstrapError::InsufficientStarterCandidates:
			return "insufficient eligible A.I.M. starter candidates";
		case DedicatedCoopMissionBootstrapError::InsufficientFunds:
			return "insufficient campaign funds for the starter roster";
		case DedicatedCoopMissionBootstrapError::StarterChargeInvalid:
			return "starter roster charge is not representable";
		case DedicatedCoopMissionBootstrapError::HireFailed:
			return "legacy mercenary hire failed";
		case DedicatedCoopMissionBootstrapError::MapScreenNotReady:
			return "strategic map screen is not ready";
		case DedicatedCoopMissionBootstrapError::NoEligibleEstablishedSector:
			return "established campaign has no eligible occupied tactical sector";
		case DedicatedCoopMissionBootstrapError::TacticalEntryFailed:
			return "tactical sector entry failed";
		case DedicatedCoopMissionBootstrapError::TacticalPostconditionFailed:
			return "tactical sector entry postcondition failed";
	}
	return "unknown dedicated co-op mission bootstrap error";
}
