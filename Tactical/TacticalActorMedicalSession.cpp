#include "TacticalActorMedicalSession.h"

#include "TacticalActorRouteExecution.h"

#include "Animation Control.h"
#include "Dialogue Control.h"
#include "Drugs And Alcohol.h"
#include "GameSettings.h"
#include "Handle UI.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "NPC.h"
#include "Overhead.h"
#include "Points.h"
#include "Soldier Control.h"
#include "Soldier Profile.h"
#include "SoldierRepository.h"
#include "TacticalActorMedicalServices.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "connect.h"
#include "message.h"
#include "worldman.h"

#include <algorithm>
#include <limits>

namespace
{
bool hasValidAnimationState(
	const TacticalActor& actor) noexcept
{
	return actor.animationPlayback().state() <
		NUMANIMATIONSTATES;
}

bool hasValidTacticalPosition(
	const TacticalActor& actor) noexcept
{
	return !TileIsOutOfBounds(actor.position().gridNo()) &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL;
}

bool hasUsableHandKit(
	const TacticalActor& medic) noexcept
{
	if (HANDPOS >= medic.inventory().size())
		return false;

	const OBJECTTYPE& kit = medic.inventory()[HANDPOS];
	return kit.exists() &&
		kit.usItem < MAXITEMS &&
		!kit.objectStack.empty() &&
		Item[kit.usItem].usItemClass == IC_MEDKIT;
}

void rejectFirstAid(TacticalActor& medic)
{
	if (medic.vitals().isUndergoingSurgery())
		medic.vitals().finishSurgery();
	UnSetUIBusy(medic.identity().id());
}

bool mayPerformSurgery(
	TacticalActor& medic,
	const TacticalActor& patient)
{
	const UINT8 bodyType = patient.identity().bodyType();
	return gGameOptions.fNewTraitSystem &&
		NUM_SKILL_TRAITS(&medic, DOCTOR_NT) >=
			gSkillTraitValues
				.ubDONumberTraitsNeededForSurgery &&
		ItemIsMedicalKit(
			medic.inventory()[HANDPOS].usItem) &&
		(patient.roster().team() == OUR_TEAM ||
		 patient.roster().team() == MILITIA_TEAM) &&
		(bodyType <= REGFEMALE ||
		 (bodyType >= FATCIV &&
		  bodyType <= CRIPPLECIV)) &&
		patient.vitals().healableInjury() >= 100 &&
		patient.identity().id() != medic.identity().id() &&
		gTacticalStatus
				.ubLastRequesterSurgeryTargetID ==
			patient.identity().id();
}

bool patientRefusesFirstAid(
	const TacticalActor& medic,
	TacticalActor& patient)
{
	if (patient.roster().team() == gbPlayerNum)
		return false;

	bool refused = false;
	const UINT8 profile = patient.identity().profile();
	if (profile < NUM_PROFILES &&
		!(gMercProfiles[profile].ubMiscFlags &
			PROFILE_MISC_FLAG_RECRUITED) &&
		(gMercProfiles[profile].Type == PROFILETYPE_RPC ||
		 gMercProfiles[profile].Type == PROFILETYPE_NPC))
	{
		refused = PCDoesFirstAidOnNPC(profile);
	}

	// Captured opponents accept treatment. Other conscious hostiles retain
	// the established refusal behavior.
	if (!refused &&
		!patient.aiBehavior().neutral() &&
		patient.vitals().health() >= OKLIFE &&
		patient.roster().side() != medic.roster().side() &&
		!(patient.featureFlags().primaryFlags() &
			SOLDIER_POW))
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_UI_FEEDBACK,
			Message[STR_REFUSE_FIRSTAID]);
		refused = true;
	}

	return refused;
}

void beginAidAnimation(
	TacticalActor& medic,
	const TacticalActor& patient,
	std::uint8_t direction)
{
	const bool bothProne =
		gAnimControl[medic.animationPlayback().state()]
				.ubEndHeight == ANIM_PRONE &&
		gAnimControl[patient.animationPlayback().state()]
				.ubEndHeight == ANIM_PRONE &&
		!medic.vitals().isUndergoingSurgery();

	if (medic.position().direction() != direction)
	{
		medic.status().flags() |=
			SOLDIER_LOOK_NEXT_TURNSOLDIER;
		medic.EVENT_SetSoldierDesiredDirection(direction);
		medic.EVENT_SetSoldierDirection(direction);
	}

	const UINT16 animation =
		bothProne ? START_AID_PRN : START_AID;
	if (!is_networked)
		medic.EVENT_InitNewSoldierAnim(animation, 0, FALSE);
	else
		medic.ChangeSoldierState(animation, 0, 0);
}
}

std::int16_t TacticalActorMedicalSession::beginActionPointCost(
	TacticalActor& medic)
{
	if (!hasValidAnimationState(medic))
		return 0;

	const std::int32_t cost =
		static_cast<std::int32_t>(
			GetAPsToChangeStance(
				&medic,
				ANIM_CROUCH)) +
		static_cast<std::int32_t>(
			APBPConstants[AP_START_FIRST_AID]);
	return static_cast<std::int16_t>(
		std::clamp<std::int32_t>(
			cost,
			0,
			std::numeric_limits<std::int16_t>::max()));
}

bool TacticalActorMedicalSession::beginFirstAid(
	TacticalActor& medic,
	std::int32_t patientGrid,
	std::uint8_t direction)
{
	if (!IsJa2TacticalWorldLoaded() ||
		TileIsOutOfBounds(patientGrid) ||
		direction >= NUM_WORLD_DIRECTIONS ||
		!medic.roster().active() ||
		!medic.roster().inSector() ||
		!hasValidTacticalPosition(medic) ||
		!hasValidAnimationState(medic) ||
		!hasUsableHandKit(medic))
	{
		rejectFirstAid(medic);
		return false;
	}

	const SoldierID patientId =
		WhoIsThere2(patientGrid, medic.position().level());
	if (patientId == NOBODY)
	{
		rejectFirstAid(medic);
		return false;
	}

	TacticalActor* const patient =
		GetJa2SoldierRepository().resolve(patientId);
	if (!patient ||
		!patient->roster().active() ||
		!patient->roster().inSector() ||
		patient->position().gridNo() != patientGrid ||
		patient->position().level() !=
			medic.position().level() ||
		!hasValidTacticalPosition(*patient) ||
		!hasValidAnimationState(*patient))
	{
		rejectFirstAid(medic);
		return false;
	}

	const bool performSurgery =
		mayPerformSurgery(medic, *patient);
	const OBJECTTYPE& selectedKit =
		medic.inventory()[HANDPOS];
	const UINT16 selectedKitItem = selectedKit.usItem;
	const UINT16 selectedKitStatus =
		selectedKit[0]->data.objectStatus;
	medic.vitals().finishSurgery();

	if (patientRefusesFirstAid(medic, *patient))
	{
		rejectFirstAid(medic);
		return false;
	}

	// A medic may provide only one treatment service at a time.
	TacticalActorMedicalServices::cancelProviding(
		medic,
		false);
	if (performSurgery)
		medic.vitals().beginSurgery();
	beginAidAnimation(medic, *patient, direction);

	medic.targeting().gridNo() = patientGrid;
	medic.service().beginProvidingTo(patientId);
	patient->service().addProvider();

	if (patient->identity().id() !=
			medic.identity().id() &&
		!patient->collapseState().tactical())
	{
		(void)TacticalActorRouteExecution::settleIntoStationaryStance(*patient);
	}

	AdditionalTacticalCharacterDialogue_CallsLua(
		&medic,
		ADE_BANDAGE_PERFORM_BEGIN,
		patient->identity().profile());
	AdditionalTacticalCharacterDialogue_CallsLua(
		patient,
		ADE_BANDAGE_RECEIVE_BEGIN,
		medic.identity().profile());

	ApplyDrugs_New(
		patient,
		selectedKitItem,
		selectedKitStatus);
	return true;
}

bool TacticalActorMedicalSession::resumeProvidingAnimation(
	TacticalActor& medic)
{
	if (!medic.service().hasPartner() ||
		medic.vitals().health() < OKLIFE ||
		medic.vitals().breath() <= 0 ||
		!hasValidAnimationState(medic))
	{
		return false;
	}

	const UINT16 animation =
		gAnimControl[medic.animationPlayback().state()]
				.ubEndHeight == ANIM_PRONE
			? GIVING_AID_PRN
			: GIVING_AID;
	if (!is_networked)
		medic.EVENT_InitNewSoldierAnim(animation, 0, FALSE);
	else
		medic.ChangeSoldierState(animation, 0, 0);
	return true;
}
