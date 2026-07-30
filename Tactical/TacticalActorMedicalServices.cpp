#include "TacticalActorMedicalServices.h"
#include "TacticalActorMedicalTreatment.h"

#include "Animation Control.h"
#include "Assignments.h"
#include "Dialogue Control.h"
#include "GameSettings.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Overhead.h"
#include "Soldier Control.h"
#include "Soldier macros.h"
#include "SoldierRepository.h"
#include "TacticalWorldAdapter.h"
#include "ai.h"
#include "connect.h"
#include "worldman.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
bool hasValidAnimationState(
	const TacticalActor& actor) noexcept
{
	return actor.animationPlayback().state() <
		NUMANIMATIONSTATES;
}

bool isLiveEnemyMedic(
	const TacticalActor& actor) noexcept
{
	return actor.roster().active() &&
		actor.roster().inSector() &&
		actor.roster().team() == ENEMY_TEAM &&
		actor.vitals().health() > 0 &&
		!ARMED_VEHICLE(&actor) &&
		!ENEMYROBOT(&actor);
}

bool hasBoundedInventory(
	const TacticalActor& actor) noexcept
{
	if (actor.inventory().size() < NUM_INV_SLOTS)
		return false;

	for (std::size_t slot = 0;
		 slot < actor.inventory().size();
		 ++slot)
	{
		const OBJECTTYPE& object = actor.inventory()[slot];
		if (object.usItem >= MAXITEMS ||
			(object.ubNumberOfObjects != 0 &&
			 object.objectStack.empty()))
		{
			return false;
		}
	}
	return true;
}

bool hasUsableHandKit(TacticalActor& medic)
{
	if (!hasBoundedInventory(medic) ||
		!MakeSureMedKitIsInHand(&medic, true) ||
		!hasBoundedInventory(medic) ||
		HANDPOS >= medic.inventory().size())
	{
		return false;
	}

	const OBJECTTYPE& kit = medic.inventory()[HANDPOS];
	return kit.exists() &&
		kit.usItem < MAXITEMS &&
		!kit.objectStack.empty();
}

void playEndAidAnimation(
	TacticalActor& medic,
	bool requested)
{
	if (!requested ||
		medic.vitals().health() < OKLIFE ||
		medic.vitals().breath() <= 0 ||
		!medic.roster().active() ||
		!medic.roster().inSector() ||
		!IsJa2TacticalWorldLoaded() ||
		!hasValidAnimationState(medic))
	{
		return;
	}

	const std::uint16_t animation =
		gAnimControl[medic.animationPlayback().state()]
				.ubEndHeight == ANIM_PRONE
			? END_AID_PRN
			: END_AID;
	if (!is_networked)
		medic.EVENT_InitNewSoldierAnim(animation, 0, FALSE);
	else
		medic.ChangeSoldierState(animation, 0, FALSE);
}

std::uint16_t boundedDrainPoints(
	std::uint32_t usedPoints) noexcept
{
	const double factor =
		std::max(
			0.0,
			static_cast<double>(
				gGameExternalOptions
					.dEnemyMedicMedKitDrainFactor));
	const double drain =
		static_cast<double>(usedPoints) * factor;
	return static_cast<std::uint16_t>(
		std::min(
			drain,
			static_cast<double>(
				std::numeric_limits<std::uint16_t>::max())));
}

bool prepareTreatment(TacticalActor& medic)
{
	if (!isLiveEnemyMedic(medic) ||
		!IsJa2TacticalWorldLoaded() ||
		TileIsOutOfBounds(medic.position().gridNo()) ||
		medic.position().level() < FIRST_LEVEL ||
		medic.position().level() > SECOND_LEVEL ||
		!hasValidAnimationState(medic) ||
		!hasUsableHandKit(medic))
	{
		return false;
	}

	if (gAnimControl[medic.animationPlayback().state()]
			.ubEndHeight == ANIM_CROUCH)
	{
		medic.SoldierGotoStationaryStance();
		medic.EVENT_InitNewSoldierAnim(
			START_AID,
			0,
			FALSE);
	}

	return true;
}

void alert(TacticalActor& actor) noexcept
{
	actor.aiBehavior().alertStatus() =
		std::max<INT8>(
			actor.aiBehavior().alertStatus(),
			static_cast<INT8>(STATUS_RED));
}
}

bool TacticalActorMedicalServices::canTreatForAi(
	TacticalActor& medic)
{
	if (!gGameExternalOptions.fEnemyRoles ||
		!gGameExternalOptions.fEnemyMedics ||
		!isLiveEnemyMedic(medic) ||
		!hasBoundedInventory(medic) ||
		!HAS_SKILL_TRAIT(&medic, DOCTOR_NT))
	{
		return false;
	}

	return FindFirstAidKit(&medic) != NO_SLOT ||
		FindMedKit(&medic) != NO_SLOT;
}

bool TacticalActorMedicalServices::treatAdjacentForAi(
	TacticalActor& medic)
{
	if (!isLiveEnemyMedic(medic) ||
		!IsJa2TacticalWorldLoaded() ||
		TileIsOutOfBounds(medic.position().gridNo()) ||
		medic.position().level() < FIRST_LEVEL ||
		medic.position().level() > SECOND_LEVEL ||
		medic.position().direction() >=
			NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	const std::int32_t targetGrid =
		NewGridNo(
			medic.position().gridNo(),
			DirectionInc(medic.position().direction()));
	if (TileIsOutOfBounds(targetGrid))
		return false;

	const SoldierID targetId =
		WhoIsThere2(targetGrid, medic.position().level());
	if (targetId == NOBODY)
		return false;

	TacticalActor* const patient =
		GetJa2SoldierRepository().resolve(targetId);
	if (!patient ||
		!patient->roster().active() ||
		!patient->roster().inSector() ||
		patient->roster().team() != ENEMY_TEAM ||
		patient->vitals().health() <= 0 ||
		TileIsOutOfBounds(patient->position().gridNo()) ||
		patient->position().level() < FIRST_LEVEL ||
		patient->position().level() > SECOND_LEVEL ||
		!hasValidAnimationState(*patient) ||
		ARMED_VEHICLE(patient) ||
		ENEMYROBOT(patient) ||
		!patient->vitals().hasHealableInjury() ||
		!prepareTreatment(medic))
	{
		return false;
	}

	if (patient->vitals().health() >= OKLIFE &&
		patient->vitals().breath() >= OKBREATH &&
		!patient->collapseState().tactical())
	{
		patient->SoldierGotoStationaryStance();
	}

	medic.vitals().beginSurgery();
	OBJECTTYPE& kit = medic.inventory()[HANDPOS];
	const std::uint16_t kitPoints = TotalPoints(&kit);
	const std::int8_t oldLife = patient->vitals().health();
	const std::uint32_t usedPoints =
		TacticalActorMedicalTreatment::treatInSector(
			medic,
			*patient,
			kitPoints,
			kitPoints);
	UseKitPoints(
		&kit,
		boundedDrainPoints(usedPoints),
		&medic);

	patient->damageDisplay().displayFlag() = TRUE;
	patient->combatResult().accumulatedDamage() -=
		patient->vitals().health() - oldLife;
	alert(medic);
	alert(*patient);
	return true;
}

bool TacticalActorMedicalServices::treatSelfForAi(
	TacticalActor& medic)
{
	if (!medic.vitals().hasHealableInjury() ||
		!prepareTreatment(medic))
	{
		return false;
	}

	medic.vitals().beginSurgery();
	OBJECTTYPE& kit = medic.inventory()[HANDPOS];
	const std::uint16_t kitPoints = TotalPoints(&kit);
	const std::int8_t oldLife = medic.vitals().health();
	const std::uint32_t usedPoints =
		TacticalActorMedicalTreatment::treatInSector(
			medic,
			medic,
			kitPoints,
			kitPoints);
	UseKitPoints(
		&kit,
		boundedDrainPoints(usedPoints),
		&medic);

	medic.damageDisplay().displayFlag() = TRUE;
	medic.combatResult().accumulatedDamage() -=
		medic.vitals().health() - oldLife;
	alert(medic);
	return true;
}

void TacticalActorMedicalServices::cancelReceiving(
	TacticalActor& patient,
	bool playEndAnimation)
{
	auto& repository = GetJa2SoldierRepository();
	for (std::size_t index = 0;
		 index < repository.capacity();
		 ++index)
	{
		TacticalActor* const medic =
			repository.resolve(index);
		if (!medic ||
			!medic->roster().active() ||
			medic->service().partner() !=
				patient.identity().id())
		{
			continue;
		}

		patient.service().removeProvider();
		if (medic->vitals().isUndergoingSurgery())
			medic->vitals().finishSurgery();
		medic->service().finishProviding();

		if (gTacticalStatus.fAutoBandageMode)
		{
			patient.service().clearAutoBandagingMedic();
			ActionDone(medic);
		}
		else
		{
			playEndAidAnimation(
				*medic,
				playEndAnimation);
		}

		AdditionalTacticalCharacterDialogue_CallsLua(
			medic,
			ADE_BANDAGE_PERFORM_END,
			patient.identity().profile());
		AdditionalTacticalCharacterDialogue_CallsLua(
			&patient,
			ADE_BANDAGE_RECEIVE_END,
			medic->identity().profile());
		medic->featureFlags().secondaryFlags() &=
			~SOLDIER_SURGERY_BOOSTED;
	}

	patient.service().clearProviders();
	patient.service().clearAutoBandagingMedic();
}

void TacticalActorMedicalServices::cancelProviding(
	TacticalActor& medic,
	bool playEndAnimation)
{
	if (!medic.service().hasPartner())
	{
		if (medic.vitals().isUndergoingSurgery())
			medic.vitals().finishSurgery();
		medic.featureFlags().secondaryFlags() &=
			~SOLDIER_SURGERY_BOOSTED;
		return;
	}

	TacticalActor* const patient =
		GetJa2SoldierRepository().resolve(
			medic.service().partner());
	medic.service().finishProviding();
	if (medic.vitals().isUndergoingSurgery())
		medic.vitals().finishSurgery();

	if (patient)
	{
		patient->service().removeProvider();
		if (gTacticalStatus.fAutoBandageMode)
		{
			patient->service().clearAutoBandagingMedic();
			ActionDone(&medic);
		}
		else
		{
			playEndAidAnimation(
				medic,
				playEndAnimation);
		}
	}

	medic.featureFlags().secondaryFlags() &=
		~SOLDIER_SURGERY_BOOSTED;
}
