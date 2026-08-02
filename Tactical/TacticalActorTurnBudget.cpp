#include "TacticalActorTurnBudget.h"

#include "Animation Control.h"
#include "Drugs And Alcohol.h"
#include "GameInitOptionsScreen.h"
#include "GameSettings.h"
#include "Items.h"
#include "Points.h"
#include "SkillCheck.h"
#include "TacticalActor.h"
#include "Soldier Functions.h"
#include "Soldier Profile.h"
#include "Soldier macros.h"
#include "Strategic Status.h"
#include "TacticalActorModifiers.h"
#include "Vehicles.h"
#include "opplist.h"
#include "soldier profile type.h"

#include <cstddef>
#include <cstdint>

namespace
{
constexpr std::size_t MaximumAttachmentDepth = 8;

bool hasBoundedObject(
	const OBJECTTYPE& object,
	std::size_t depth = 0) noexcept
{
	if (object.usItem >= MAXITEMS ||
		(object.exists() && object.objectStack.empty()))
	{
		return false;
	}

	for (const StackedObjectData& stacked : object.objectStack)
	{
		if (!stacked.attachments.empty() &&
			depth >= MaximumAttachmentDepth)
		{
			return false;
		}

		for (const OBJECTTYPE& attachment : stacked.attachments)
		{
			if (!hasBoundedObject(attachment, depth + 1))
				return false;
		}
	}
	return true;
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
		if (!hasBoundedObject(actor.inventory()[slot]))
			return false;
	}
	return true;
}

bool hasBoundedVehicleContext(
	const TacticalActor& actor) noexcept
{
	if (!(actor.status().flags() & SOLDIER_VEHICLE))
		return true;

	return pVehicleList != nullptr &&
		VehicleIdIsValid(
			actor.vehicleState().tacticalVehicleId());
}

bool hasBoundedBudgetContext(
	const TacticalActor& actor) noexcept
{
	const std::uint8_t profile = actor.identity().profile();
	return actor.identity().bodyType() < TOTALBODYTYPES &&
		actor.roster().team() >= 0 &&
		actor.roster().team() < MAXTEAMS &&
		(profile == NO_PROFILE || profile < NUM_PROFILES) &&
		gGameOptions.ubDifficultyLevel < MAX_DIF_LEVEL &&
		(!actor.vitals().alive() ||
		 actor.vitals().maximumHealth() > 0) &&
		hasBoundedInventory(actor) &&
		hasBoundedVehicleContext(actor);
}
}

std::int16_t TacticalActorTurnBudget::calculateTurnGrant(
	TacticalActor& actor)
{
	TacticalActor* const self = &actor;
	SoldierVitalsComponent& soldierVitals = self->vitals();
	if (!soldierVitals.alive())
		return 0;
	if (self->collapseState().sleepDrugCounter() > 0 &&
		self->collapseState().tactical())
	{
		return 0;
	}
	if (!hasBoundedBudgetContext(actor))
		return 0;

	std::int16_t points = 20 +
		((10 * EffectiveExpLevel(self) +
		  3 * EffectiveAgility(self, FALSE) +
		  2 * soldierVitals.maximumHealth() +
		  2 * EffectiveDexterity(self, FALSE) + 5) / 10);
	points += GetGearAPBonus(self);

	const std::int8_t bandage =
		soldierVitals.maximumHealth() -
		soldierVitals.health() -
		soldierVitals.bleeding();
	if (soldierVitals.health() < soldierVitals.maximumHealth())
	{
		points -=
			(2 * points *
			 (soldierVitals.maximumHealth() -
			  soldierVitals.health() + bandage / 2)) /
			(3 * soldierVitals.maximumHealth());
	}

	if (soldierVitals.breath() < 100 &&
		!(self->status().flags() & SOLDIER_VEHICLE) &&
		!AM_A_ROBOT(self))
	{
		points -= points * (100 - soldierVitals.breath()) / 200;
	}
	if (self->movementMetrics().carriedWeightAtTurnStart() > 100)
	{
		points = static_cast<std::uint8_t>(
			static_cast<std::uint32_t>(points) * 100 /
			self->movementMetrics().carriedWeightAtTurnStart());
	}

	points = DynamicAdjustAPConstants(points, points);
	if (points < APBPConstants[AP_MINIMUM])
		points = APBPConstants[AP_MINIMUM];

	const std::int16_t maximumPoints =
		gubMaxActionPoints[self->identity().bodyType()];
	if (points > maximumPoints)
		points = maximumPoints;

	if (self->identity().bodyType() == BLOODCAT)
	{
		points = points * APBPConstants[AP_YOUNG_MONST_FACTOR] / 10;
	}
	else if (self->status().flags() & SOLDIER_MONSTER)
	{
		if (self->identity().bodyType() == YAF_MONSTER ||
			self->identity().bodyType() == YAM_MONSTER ||
			self->identity().bodyType() == INFANT_MONSTER)
		{
			points =
				points * APBPConstants[AP_YOUNG_MONST_FACTOR] / 10;
		}
		if (self->morale().frenzied())
		{
			points =
				points * APBPConstants[AP_MONST_FRENZY_FACTOR] / 10;
		}
	}
	else if (self->status().flags() & SOLDIER_VEHICLE)
	{
		AdjustVehicleAPs(self, &points);
	}
	else if (gGameOptions.fNewTraitSystem &&
		IS_MERC_BODY_TYPE(self) &&
		(self->roster().team() == ENEMY_TEAM ||
		 self->roster().team() == MILITIA_TEAM ||
		 self->roster().team() == gbPlayerNum))
	{
		points += points *
			gSkillTraitValues.ubSLBonusAPsPercent *
			GetSquadleadersCountInVicinity(self, FALSE, FALSE) /
			100;
	}

	if (self->identity().profile() != NO_PROFILE)
	{
		if (DoesMercHaveDisability(self, CLAUSTROPHOBIC) &&
			gbWorldSectorZ > 0)
		{
			points = points * APBPConstants[AP_CLAUSTROPHOBE] / 10;
		}
		else if (DoesMercHaveDisability(self, FEAR_OF_INSECTS) &&
			MercSeesCreature(self))
		{
			points =
				points * APBPConstants[AP_AFRAID_OF_INSECTS] / 10;
		}
		else if (DoesMercHaveDisability(self, HEAT_INTOLERANT) &&
			MercIsInTropicalSector(self))
		{
			points = points * 9 / 10;
		}
		else if (DoesMercHaveDisability(self, AFRAID_OF_HEIGHTS) &&
			self->position().level() > 0)
		{
			points = points * 9 / 10;
		}
	}

	HandleAPEffectDueToDrugs(self, &points);
	if (self->roster().team() == ENEMY_TEAM)
	{
		points += zDiffSetting[gGameOptions.ubDifficultyLevel]
			.iEnemyAPBonus;
	}
	else if (self->roster().team() == MILITIA_TEAM)
	{
		if (self->roster().soldierClass() ==
				SOLDIER_CLASS_GREEN_MILITIA &&
			gGameExternalOptions.bGreenMilitiaAPsBonus != 0)
		{
			points += gGameExternalOptions.bGreenMilitiaAPsBonus;
		}
		else if (self->roster().soldierClass() ==
					 SOLDIER_CLASS_REG_MILITIA &&
				 gGameExternalOptions.bRegularMilitiaAPsBonus != 0)
		{
			points += gGameExternalOptions.bRegularMilitiaAPsBonus;
		}
		else if (self->roster().soldierClass() ==
					 SOLDIER_CLASS_ELITE_MILITIA &&
				 gGameExternalOptions.bVeteranMilitiaAPsBonus != 0)
		{
			points += gGameExternalOptions.bVeteranMilitiaAPsBonus;
		}
	}
	else if (self->roster().team() == gbPlayerNum)
	{
		points += gGameExternalOptions.iPlayerAPBonus;
	}

	points = static_cast<std::int16_t>(
		points *
		(100 + TacticalActorModifiers::actionPointBonus(*self)) /
		100);
	if (self->identity().profile() != NO_PROFILE &&
		gGameExternalOptions.usSpecialNPCStronger > 0)
	{
		switch (self->identity().profile())
		{
		case CARMEN:
		case QUEEN:
		case JOE:
		case ANNIE:
		case CHRIS:
		case KINGPIN:
		case TIFFANY:
		case T_REX:
		case DRUGGIST:
		case GENERAL:
		case JIM:
		case JACK:
		case OLAF:
		case RAY:
		case OLGA:
		case TYRONE:
		case MIKE:
			points += points *
				gGameExternalOptions.usSpecialNPCStronger / 400;
			break;
		default:
			break;
		}
	}

	if (gTacticalStatus.bBoxingState == BOXING ||
		gTacticalStatus.bBoxingState == PRE_BOXING)
	{
		points /= 2;
	}
	return points;
}

bool TacticalActorTurnBudget::refreshForTurn(
	TacticalActor& actor)
{
	if (!hasBoundedBudgetContext(actor))
		return false;

	TacticalActor* const self = &actor;
	if (gTacticalStatus.bBoxingState == BOXING ||
		gTacticalStatus.bBoxingState == PRE_BOXING)
	{
		if (self->actionPoints().current() >
			APBPConstants[MAX_AP_CARRIED] / 2)
		{
			self->actionPoints().current() =
				APBPConstants[MAX_AP_CARRIED] / 2;
		}
	}
	else if (self->actionPoints().current() >
		APBPConstants[MAX_AP_CARRIED])
	{
		self->actionPoints().current() =
			APBPConstants[MAX_AP_CARRIED];
	}

	self->actionPoints().current() += calculateTurnGrant(*self);
	if (self->actionPoints().current() < APBPConstants[AP_MIN_LIMIT])
	{
		self->actionPoints().current() = APBPConstants[AP_MIN_LIMIT];
	}

	if (!self->drugState().magnitude(DRUG_EFFECT_AP) &&
		!self->drugState().magnitude(DRUG_EFFECT_AGI))
	{
		std::uint16_t maximumPoints =
			gubMaxActionPoints[self->identity().bodyType()];
		if (gGameOptions.fNewTraitSystem &&
			IS_MERC_BODY_TYPE(self) &&
			(self->roster().team() == ENEMY_TEAM ||
			 self->roster().team() == MILITIA_TEAM ||
			 self->roster().team() == gbPlayerNum))
		{
			maximumPoints += maximumPoints *
				gSkillTraitValues.ubSLBonusAPsPercent *
				GetSquadleadersCountInVicinity(self, FALSE, FALSE) /
				100;
		}

		if (self->roster().team() == ENEMY_TEAM)
		{
			maximumPoints +=
				zDiffSetting[gGameOptions.ubDifficultyLevel]
					.iEnemyAPBonus;
		}
		else if (self->roster().team() == MILITIA_TEAM)
		{
			if (self->roster().soldierClass() ==
					SOLDIER_CLASS_GREEN_MILITIA &&
				gGameExternalOptions.bGreenMilitiaAPsBonus != 0)
			{
				maximumPoints +=
					gGameExternalOptions.bGreenMilitiaAPsBonus;
			}
			else if (self->roster().soldierClass() ==
						 SOLDIER_CLASS_REG_MILITIA &&
					 gGameExternalOptions.bRegularMilitiaAPsBonus != 0)
			{
				maximumPoints +=
					gGameExternalOptions.bRegularMilitiaAPsBonus;
			}
			else if (self->roster().soldierClass() ==
						 SOLDIER_CLASS_ELITE_MILITIA &&
					 gGameExternalOptions.bVeteranMilitiaAPsBonus != 0)
			{
				maximumPoints +=
					gGameExternalOptions.bVeteranMilitiaAPsBonus;
			}
		}
		else if (self->roster().team() == gbPlayerNum)
		{
			maximumPoints += gGameExternalOptions.iPlayerAPBonus;
		}

		maximumPoints = static_cast<std::int16_t>(
			maximumPoints *
			(100 + TacticalActorModifiers::actionPointBonus(*self)) /
			100);
		if (self->identity().profile() != NO_PROFILE &&
			gGameExternalOptions.usSpecialNPCStronger > 0)
		{
			switch (self->identity().profile())
			{
			case CARMEN:
			case QUEEN:
			case JOE:
			case ANNIE:
			case CHRIS:
			case KINGPIN:
			case TIFFANY:
			case T_REX:
			case DRUGGIST:
			case GENERAL:
			case JIM:
			case JACK:
			case OLAF:
			case RAY:
			case OLGA:
			case TYRONE:
			case MIKE:
				maximumPoints += maximumPoints *
					gGameExternalOptions.usSpecialNPCStronger / 400;
				break;
			default:
				break;
			}
		}

		if (self->actionPoints().current() > maximumPoints)
			self->actionPoints().current() = maximumPoints;
	}

	self->actionPoints().snapshotTurnStart();
	if (self->featureFlags().primaryFlags() & SOLDIER_NO_AP)
	{
		self->featureFlags().primaryFlags() &= ~SOLDIER_NO_AP;
		self->actionPoints().clear();
	}
	return true;
}
