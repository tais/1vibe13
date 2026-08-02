#include "TacticalActorModifiers.h"

#include "TacticalActorConditions.h"
#include "TacticalActorDisease.h"
#include "TacticalActorRadio.h"
#include "TacticalActorRobotics.h"

#include "Animation Control.h"
#include "Campaign Types.h"
#include "Animation Data.h"
#include "Disease.h"
#include "Drugs And Alcohol.h"
#include "Game Clock.h"
#include "GameSettings.h"
#include "Handle Items.h"
#include "IMP Skill Trait.h"
#include "Interface.h"
#include "Items.h"
#include "Overhead.h"
#include "SkillCheck.h"
#include "TacticalActor.h"
#include "Soldier Profile.h"
#include "Soldier macros.h"
#include "Vehicles.h"
#include "Weapons.h"
#include "tiledef.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace
{
const BACKGROUND_VALUES* findActorBackground(const TacticalActor& actor)
{
	if (!UsingBackGroundSystem())
		return nullptr;

	const auto profile = actor.identity().profile();
	if (profile == NO_PROFILE || profile >= NUM_PROFILES)
		return nullptr;

	const auto background = gMercProfiles[profile].usBackground;
	if (background >= NUM_BACKGROUND)
		return nullptr;

	return &zBackground[background];
}

bool hasValidStrategicSector(const TacticalActor& actor)
{
	return actor.deployment().sectorX() >= MINIMUM_VALID_X_COORDINATE &&
		actor.deployment().sectorX() <= MAXIMUM_VALID_X_COORDINATE &&
		actor.deployment().sectorY() >= MINIMUM_VALID_Y_COORDINATE &&
		actor.deployment().sectorY() <= MAXIMUM_VALID_Y_COORDINATE;
}
}

std::int32_t TacticalActorModifiers::damageResistance(
	TacticalActor& actor,
	bool autoResolve,
	bool calculateBreathLoss)
{
	auto* const self = &actor;
	INT32 resistance = 0;
	FLOAT breathmodifiermilitia = 1.0;
	FLOAT breathmodifierspecialNPC = 2.0;

	if (calculateBreathLoss)
	{
		breathmodifiermilitia = 0.75;
		breathmodifierspecialNPC = 1.0;
	}

	// SANDRO - Damage resistance for Militia
	if (!autoResolve)
	{
		if ( self->roster().soldierClass() == SOLDIER_CLASS_GREEN_MILITIA && gGameExternalOptions.bGreenMilitiaDamageResistance != 0 )
			resistance += (INT32)(gGameExternalOptions.bGreenMilitiaDamageResistance / breathmodifiermilitia);
		else if ( self->roster().soldierClass() == SOLDIER_CLASS_REG_MILITIA && gGameExternalOptions.bRegularMilitiaDamageResistance != 0 )
			resistance += (INT32)(gGameExternalOptions.bRegularMilitiaDamageResistance / breathmodifiermilitia);
		else if ( self->roster().soldierClass() == SOLDIER_CLASS_ELITE_MILITIA && gGameExternalOptions.bVeteranMilitiaDamageResistance != 0 )
			resistance += (INT32)(gGameExternalOptions.bVeteranMilitiaDamageResistance / breathmodifiermilitia);
		// bonus for enemy too
		else if ( (self->roster().soldierClass() == SOLDIER_CLASS_ADMINISTRATOR || self->roster().soldierClass() == SOLDIER_CLASS_BANDIT ) && gGameExternalOptions.sEnemyAdminDamageResistance != 0 )
			resistance += gGameExternalOptions.sEnemyAdminDamageResistance;
		else if ( self->roster().soldierClass() == SOLDIER_CLASS_ARMY && gGameExternalOptions.sEnemyRegularDamageResistance != 0 )
			resistance += gGameExternalOptions.sEnemyRegularDamageResistance;
		else if ( self->roster().soldierClass() == SOLDIER_CLASS_ELITE && gGameExternalOptions.sEnemyEliteDamageResistance != 0 )
			resistance += gGameExternalOptions.sEnemyEliteDamageResistance;
		else if (TacticalActorConditions::isZombie(actor))
		{
			if (calculateBreathLoss)
				resistance += gGameExternalOptions.sEnemyZombieBreathDamageResistance;
			else
				resistance += gGameExternalOptions.sEnemyZombieDamageResistance;
		}
	}
	//////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////
	// SANDRO - option to make special NPCs stronger - damage resistance
	if ( gGameExternalOptions.usSpecialNPCStronger > 0 )
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
			resistance += (INT32)(gGameExternalOptions.usSpecialNPCStronger / breathmodifierspecialNPC);
			break;
		}
	}
	////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////
	// STOMP traits - Bodybuilding damage resistance
	if ( gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT( self, BODYBUILDING_NT ) )
		resistance += gSkillTraitValues.ubBBDamageResistance;
	////////////////////////////////////////////////////////////////////////////////////

	// Flugente: drugs can now have an effect on damage resistance
	resistance += self->drugState().magnitude(DRUG_EFFECT_PHYS_RES);

	resistance += backgroundValue(actor, BG_RESI_PHYSICAL);

	// frozen targets go down HARD
	if ( self->skillState().cooldown(SOLDIER_COOLDOWN_CRYO) )
		resistance -= 1000;

	// resistance is between -100% and 95%
	resistance = max( -1000, resistance );
	resistance = min( 95, resistance );

	return(resistance);
}

std::int8_t TacticalActorModifiers::hearingBonus(TacticalActor& actor)
{
	auto* const self = &actor;
	INT8 bonus = 0;

	INT8 bSlot = FindHearingAid(self);
	if ( bSlot != -1 )
	{
		// at 81-100% adds +5, at 61-80% adds +4, at 41-60% adds +3, etc.
		bonus += GetHearingRangeBonus(self);	// pSoldier->inventory()[bSlot][0]->data.objectStatus / 20 + 1;
	}

	if (DoesMercHaveDisability(self, DEAF))
		bonus -= 5;

	if ( NightTime( ) )
		bonus += backgroundValue(actor, BG_PERC_HEARING_NIGHT);
	else
		bonus += backgroundValue(actor, BG_PERC_HEARING_DAY);

	if (TacticalActorRadio::isListening(actor))
		bonus += gSkillTraitValues.sVOListeningHearingBonus;

	return bonus;
}

std::int16_t TacticalActorModifiers::sightRangeBonus(TacticalActor& actor)
{
	auto* const self = &actor;
	INT16 bonus = 0;

	if (DoesMercHaveDisability(self, SHORTSIGHTED))
		bonus -= 10;

	if ( (gGameExternalOptions.usLowerVisionWhileRunning == 1) || ( gGameExternalOptions.usLowerVisionWhileRunning == 2 && self->roster().team() == gbPlayerNum ) )
	{
		// Flugente: We have to decide depending on the animation we have, otherwise we can cause bugs if we do this after being hit by an explosion etc.
		switch (self->animationPlayback().state())
		{
		case RUNNING:
		case RUNNING_W_PISTOL:
			bonus -= 25;
			break;
		}
	}

	return bonus;
}

// can we process prisoners in this sector?
std::uint32_t TacticalActorModifiers::surrenderStrength(
	TacticalActor& actor)
{
	auto* const self = &actor;

	if (self->vitals().health() < OKLIFE ||
		self->vitals().maximumHealth() <= 0 ||
		self->assignment().isAsleep() ||
		self->collapseState().tactical() ||
		(self->featureFlags().primaryFlags() & SOLDIER_POW))
	{
		return 0;
	}

	UINT32 value =
		100 +
		10 * EffectiveExpLevel(self) +
		EffectiveStrength(self, FALSE) +
		3 * EffectiveMarksmanship(self) +
		EffectiveLeadership(self) / 4;

	ReducePointsForFatigue(self, &value);

	value =
		value *
		self->vitals().health() /
		self->vitals().maximumHealth();

	value =
		value *
		(5 + sqrt((double)max(1, self->morale().morale()))) /
		15;

	// adjust for type of soldier
	if ( self->roster().soldierClass() == SOLDIER_CLASS_ELITE || self->roster().soldierClass() == SOLDIER_CLASS_ELITE_MILITIA || self->roster().soldierClass() == SOLDIER_CLASS_ROBOT )
		value *= 1.5f;
	else if ( self->roster().soldierClass() == SOLDIER_CLASS_ADMINISTRATOR || self->roster().soldierClass() == SOLDIER_CLASS_GREEN_MILITIA || self->roster().soldierClass() == SOLDIER_CLASS_BANDIT )
		value *= 0.75f;

	// tanks won't surrender that easy
	if (ARMED_VEHICLE(self))
		value *= 10;

	return value;
}

std::int8_t TacticalActorModifiers::traitChanceToHitModifier(
	TacticalActor& actor,
	std::uint16_t item,
	std::int16_t aimTime,
	std::uint8_t targetProfile)
{
	auto* const self = &actor;
	if (item >= MAXITEMS)
		return 0;

	INT8 modifier = 0;

	// Modify for traits
	if ( gGameOptions.fNewTraitSystem )
	{
		// Bonus for heavy weapons moved here from above to get instant CtH bonus and not marksmanship bonus,
		// which is supressed by weapon condition
		if (ItemIsRocketLauncher(item) || ItemIsSingleShotRocketLauncher(item))
		{
			modifier += gSkillTraitValues.bCtHModifierRocketLaunchers; // -25% for untrained mercs !!!

			if ( HAS_SKILL_TRAIT( self, HEAVY_WEAPONS_NT ) )
				modifier += gSkillTraitValues.ubHWBonusCtHRocketLaunchers * NUM_SKILL_TRAITS( self, HEAVY_WEAPONS_NT ); // +25% per trait
		}
		// Added CtH bonus for Gunslinger trait on pistols and machine-pistols
		else if ( Weapon[item].ubWeaponType == GUN_PISTOL )
		{
			modifier += gSkillTraitValues.bCtHModifierPistols; // -5% for untrained mercs.

			// this bonus is applied only on single shots!
			if ( HAS_SKILL_TRAIT( self, GUNSLINGER_NT ) && self->fireControl().burstCounter() == 0 && self->fireControl().autofireShots() == 0 )
				modifier += gSkillTraitValues.ubGSBonusCtHPistols * NUM_SKILL_TRAITS( self, GUNSLINGER_NT ); // +10% per trait
		}
		else if ( Weapon[item].ubWeaponType == GUN_M_PISTOL )
		{
			modifier += gSkillTraitValues.bCtHModifierMachinePistols; // -5% for untrained mercs.

			// this bonus is applied only on single shots!
			if ( HAS_SKILL_TRAIT( self, GUNSLINGER_NT ) && ((self->fireControl().burstCounter() == 0 && self->fireControl().autofireShots() == 0) || !gSkillTraitValues.ubGSCtHMPExcludeAuto) )
				modifier += gSkillTraitValues.ubGSBonusCtHMachinePistols * NUM_SKILL_TRAITS( self, GUNSLINGER_NT ); // +5% per trait
		}
		// Added CtH bonus for Machinegunner skill on assault rifles, SMGs and LMGs
		else if ( Weapon[item].ubWeaponType == GUN_AS_RIFLE )
		{
			modifier += gSkillTraitValues.bCtHModifierAssaultRifles; // -5% for untrained mercs.

			if ( HAS_SKILL_TRAIT( self, AUTO_WEAPONS_NT ) )
				modifier += gSkillTraitValues.ubAWBonusCtHAssaultRifles * NUM_SKILL_TRAITS( self, AUTO_WEAPONS_NT ); // +5% per trait
		}
		else if ( Weapon[item].ubWeaponType == GUN_SMG )
		{
			modifier += gSkillTraitValues.bCtHModifierSMGs; // -5% for untrained mercs.

			if ( HAS_SKILL_TRAIT( self, AUTO_WEAPONS_NT ) )
				modifier += gSkillTraitValues.ubAWBonusCtHSMGs * NUM_SKILL_TRAITS( self, AUTO_WEAPONS_NT ); // +5% per trait
		}
		else if ( Weapon[item].ubWeaponType == GUN_LMG )
		{
			modifier += gSkillTraitValues.bCtHModifierLMGs; // -10% for untrained mercs.

			if ( HAS_SKILL_TRAIT( self, AUTO_WEAPONS_NT ) )
				modifier += gSkillTraitValues.ubAWBonusCtHLMGs * NUM_SKILL_TRAITS( self, AUTO_WEAPONS_NT ); // +5% per trait
		}
		// Added CtH bonus for Gunslinger trait on pistols and machine-pistols
		else if ( Weapon[item].ubWeaponType == GUN_SN_RIFLE )
		{
			modifier += gSkillTraitValues.bCtHModifierSniperRifles; // -5% for untrained mercs.

			// this bonus is applied only on single shots!
			if ( HAS_SKILL_TRAIT( self, SNIPER_NT ) && self->fireControl().burstCounter() == 0 && self->fireControl().autofireShots() == 0 )
				modifier += gSkillTraitValues.ubSNBonusCtHSniperRifles * NUM_SKILL_TRAITS( self, SNIPER_NT ); // +5% per trait
		}
		// Added CtH bonus for Ranger skill on rifles and shotguns
		else if ( Weapon[item].ubWeaponType == GUN_RIFLE )
		{
			modifier += gSkillTraitValues.bCtHModifierRifles; // -5% for untrained mercs.

			// this bonus is applied only on single shots!
			if ( HAS_SKILL_TRAIT( self, RANGER_NT ) && self->fireControl().burstCounter() == 0 && self->fireControl().autofireShots() == 0 )
				modifier += gSkillTraitValues.ubRABonusCtHRifles * NUM_SKILL_TRAITS( self, RANGER_NT ); // +5% per trait
			//CHRISL: Why wouldn't sniper training include standard rifles which are often used as "poor-man sniper rifles"
			// this bonus is applied only on single shots!
			if ( HAS_SKILL_TRAIT( self, SNIPER_NT ) && self->fireControl().burstCounter() == 0 && self->fireControl().autofireShots() == 0 )
				modifier += gSkillTraitValues.ubSNBonusCtHRifles * NUM_SKILL_TRAITS( self, SNIPER_NT ); // +5% per trait
		}
		else if ( Weapon[item].ubWeaponType == GUN_SHOTGUN )
		{
			modifier += gSkillTraitValues.bCtHModifierShotguns; // -5% for untrained mercs.

			if ( HAS_SKILL_TRAIT( self, RANGER_NT ) )
				modifier += gSkillTraitValues.ubRABonusCtHShotguns * NUM_SKILL_TRAITS( self, RANGER_NT ); // +10% per trait
		}

		// Added small CtH penalty for robot if controller hasn't the Technician trait
		if ( AM_A_ROBOT( self ) )
		{
			modifier += gSkillTraitValues.bCtHModifierRobot; // -10%

			TacticalActor* robotController =
				TacticalActorRobotics::controller(*self);
			if ( robotController != nullptr &&
				 HAS_SKILL_TRAIT( robotController, TECHNICIAN_NT ) )
			{
				modifier +=
					gSkillTraitValues.ubTECtHControlledRobotBonus *
					NUM_SKILL_TRAITS(
						robotController,
						TECHNICIAN_NT); // +10% per trait
			}
		}

		// Added character traits influence
		if ( self->identity().profile() != NO_PROFILE &&
			self->identity().profile() < NUM_PROFILES )
		{
			// Sociable - better performance in groups
			if ( DoesMercHavePersonality( self, CHAR_TRAIT_SOCIABLE ) )
			{
				INT8 bNumMercs = CheckMercsNearForCharTraits( self->identity().profile(), CHAR_TRAIT_SOCIABLE );
				if ( bNumMercs > 2 )
					modifier += 5;
				else if ( bNumMercs > 0 )
					modifier += 2;
			}
			// Loner - better performance when alone
			else if ( DoesMercHavePersonality( self, CHAR_TRAIT_LONER ) )
			{
				INT8 bNumMercs = CheckMercsNearForCharTraits( self->identity().profile(), CHAR_TRAIT_LONER );
				if ( bNumMercs == 0 )
					modifier += 5;
				else if ( bNumMercs <= 1 )
					modifier += 2;
			}
			// Aggressive - bonus on bursts/autofire
			else if ( DoesMercHavePersonality( self, CHAR_TRAIT_AGGRESSIVE ) )
			{
				if ( (self->fireControl().burstCounter() || self->fireControl().autofireShots()) && !aimTime )
					modifier += 10;
			}
			// Show-off - better performance if some babes around to impress
			else if ( DoesMercHavePersonality( self, CHAR_TRAIT_SHOWOFF ) )
			{
				INT8 bNumMercs = CheckMercsNearForCharTraits( self->identity().profile(), CHAR_TRAIT_SHOWOFF );
				if ( bNumMercs > 1 )
					modifier += 5;
				else if ( bNumMercs > 0 )
					modifier += 2;
			}
			// Added disabilities
			if ( self->identity().profile() != NO_PROFILE &&
				self->identity().profile() < NUM_PROFILES )
			{
				// Heat intolerant penalty
				if ( MercIsHot( self ) )
				{
					modifier -= 15;
				}
				// Small penalty for fear of insects in tropical sectors
				// Flugente: drugs can temporarily cause a merc get a new disability
				else if ( DoesMercHaveDisability( self, FEAR_OF_INSECTS ) && MercIsInTropicalSector( self ) )
				{
					// fear of insects, and we are in tropical sector
					modifier -= 5;
				}
			}
		}

		// Dauntless - penalty for not taking proper cover
		if (targetProfile != NO_PROFILE &&
			targetProfile < NUM_PROFILES)
		{
			if ( gMercProfiles[targetProfile].bCharacterTrait == CHAR_TRAIT_DAUNTLESS )
				modifier += 5;
		}
	}
	else
	{
		// This rather illogical bonus for psychotic characters applies only with old traits.
		if ( DoesMercHaveDisability( self, PSYCHO ) )
		{
			modifier += AIM_BONUS_PSYCHO;
		}
	}

	return modifier;
}

bool TacticalActorModifiers::hasBackgroundFlag(
	const TacticalActor& actor,
	std::uint64_t flag)
{
	const auto* background = findActorBackground(actor);
	return background != nullptr && (background->uiFlags & flag) != 0;
}

std::int16_t TacticalActorModifiers::backgroundValue(
	const TacticalActor& actor,
	std::uint16_t property)
{
	const auto* background = findActorBackground(actor);
	if (background == nullptr || property >= BG_MAX)
		return 0;

	return background->value[property];
}

const std::vector<std::int16_t>& TacticalActorModifiers::backgroundValues(
	const TacticalActor& actor,
	BackgroundVectorTypes property)
{
	static const std::vector<std::int16_t> emptyValues;

	const auto* background = findActorBackground(actor);
	if (background == nullptr)
		return emptyValues;

	const auto values = background->valueVectors.find(property);
	return values != background->valueVectors.end()
		? values->second
		: emptyValues;
}

std::int8_t TacticalActorModifiers::suppressionResistanceBonus(
	const TacticalActor& actor)
{
	int bonus = backgroundValue(actor, BG_RESI_SUPPRESSION);

	if (actor.roster().team() == ENEMY_TEAM)
	{
		UINT8 officerType = OFFICER_NONE;
		if (HighestEnemyOfficersInSector(officerType))
		{
			bonus +=
				gGameExternalOptions.sEnemyOfficerSuppressionResistanceBonus *
				officerType;
		}
	}

	return static_cast<std::int8_t>(min(100, max(-100, bonus)));
}

std::int16_t TacticalActorModifiers::meleeDamageBonus(
	const TacticalActor& actor)
{
	return backgroundValue(actor, BG_PERC_DAMAGE_MELEE);
}

std::int16_t TacticalActorModifiers::actionPointBonus(
	const TacticalActor& actor)
{
	INT16 bonus = 0;

	if (actor.featureFlags().primaryFlags() & SOLDIER_AIRDROP_TURN)
		bonus += backgroundValue(actor, BG_AIRDROP);

	if (actor.featureFlags().primaryFlags() & SOLDIER_ASSAULT_BONUS)
		bonus += backgroundValue(actor, BG_ASSAULT);

	if (hasValidStrategicSector(actor))
	{
		const UINT8 sector = static_cast<UINT8>(
			SECTOR(
				actor.deployment().sectorX(),
				actor.deployment().sectorY()));
		const UINT8 traverseType =
			SectorInfo[sector].ubTraversability[THROUGH_STRATEGIC_MOVE];

		switch (traverseType)
		{
		case NS_RIVER:
		case EW_RIVER:
			bonus += backgroundValue(actor, BG_RIVER);
			break;
		case COASTAL:
		case COASTAL_ROAD:
			bonus += backgroundValue(actor, BG_COASTAL);
			break;
		case TROPICS_SAM_SITE:
			bonus += backgroundValue(actor, BG_COASTAL);
			bonus += backgroundValue(actor, BG_TROPICAL);
			break;
		case TROPICS:
		case TROPICS_ROAD:
			bonus += backgroundValue(actor, BG_TROPICAL);
			break;
		case PLAINS:
		case PLAINS_ROAD:
		case FARMLAND:
		case FARMLAND_ROAD:
			bonus += backgroundValue(actor, BG_PLAINS);
			break;
		case DENSE:
		case DENSE_ROAD:
			bonus += backgroundValue(actor, BG_FOREST);
			break;
		case HILLS:
		case HILLS_ROAD:
			bonus += backgroundValue(actor, BG_MOUNTAIN);
			break;
		case SWAMP:
		case SWAMP_ROAD:
			bonus += backgroundValue(actor, BG_SWAMP);
			break;
		case SAND:
		case SAND_ROAD:
		case SAND_SAM_SITE:
			bonus += backgroundValue(actor, BG_DESERT);
			break;
		case TOWN:
		case CAMBRIA_HOSPITAL_SITE:
		case DRASSEN_AIRPORT_SITE:
		case MEDUNA_AIRPORT_SITE:
			bonus += backgroundValue(actor, BG_URBAN);
			break;
		default:
			break;
		}
	}

	if (actor.position().level())
		bonus += backgroundValue(actor, BG_HEIGHT);

	INT16 diseaseEffect = 0;
	for (int disease = 0; disease < NUM_DISEASES; ++disease)
	{
		diseaseEffect +=
			Disease[disease].sEffAP *
			TacticalActorDisease::magnitude(actor, disease);
	}

	return bonus + diseaseEffect;
}

std::int8_t TacticalActorModifiers::fearResistanceBonus(
	const TacticalActor& actor)
{
	const int bonus = backgroundValue(actor, BG_RESI_FEAR);
	return static_cast<std::int8_t>(min(100, max(-100, bonus)));
}

float TacticalActorModifiers::moraleModifier(const TacticalActor& actor)
{
	FLOAT modifier = 1.0f;

	UINT8 officerType = OFFICER_NONE;
	if (actor.roster().team() == ENEMY_TEAM &&
		HighestEnemyOfficersInSector(officerType))
	{
		modifier +=
			gGameExternalOptions.dEnemyOfficerMoraleModifier * officerType;
	}

	if (gGameExternalOptions.fDisease)
	{
		FLOAT diseaseEffect = 1.0f;
		for (int disease = 0; disease < NUM_DISEASES; ++disease)
		{
			diseaseEffect *=
				1.0f -
				(1.0f - Disease[disease].moralemodifier) *
					TacticalActorDisease::magnitude(actor, disease);
		}

		modifier *= diseaseEffect;
	}

	return modifier;
}

std::int16_t TacticalActorModifiers::interruptModifier(
	TacticalActor& actor)
{
	INT16 bonus = 0;

	// Radio listening divides the actor's attention.
	if (TacticalActorRadio::isListening(actor))
		bonus -= 3;

	// Roping down without a matching background consumes most attention.
	if ((actor.featureFlags().primaryFlags() & SOLDIER_AIRDROP_TURN) &&
		backgroundValue(actor, BG_AIRDROP) <= 0)
	{
		bonus -= 8;
	}

	return bonus;
}

// Flugente: assumed character weight (without any items)
float TacticalActorModifiers::bodyWeight(
	const TacticalActor& actor)
{
	switch (actor.identity().bodyType())
	{
	case REGMALE:
	case MANCIV:
		return 85.0f;

	case BIGMALE:
	case STOCKYMALE:
		return 110.0f;

	case REGFEMALE:
		return 75.0f;

	case FATCIV:
		return 100.0f;

	case MINICIV:
	case DRESSCIV:
		return 60.0f;

	case HATKIDCIV:
	case KIDCIV:
		return 40.0f;

	case CRIPPLECIV:
		return 75.0f;
	}

	return 80.0f;
}

// Flugente: chance to defeat a water snake instead of being hit by it
std::uint8_t TacticalActorModifiers::waterSnakeDefenseChance(
	TacticalActor& actor)
{
	auto* const self = &actor;

	// base evasion chance is 5%
	INT16 val = 5;

	if ( gGameOptions.fNewTraitSystem )
		val +=
			gSkillTraitValues.usSVSnakeDefense *
			NUM_SKILL_TRAITS(self, SURVIVAL_NT);

	val += backgroundValue(actor, BG_SNAKEDEFENSE);

	// bonus if we have a knife, extra if it is in our hands
	for (size_t slot = 0, inventorySize = self->inventory().size();
		slot < inventorySize;
		++slot)
	{
		if (self->inventory()[slot].exists())
		{
			OBJECTTYPE* object = &self->inventory()[slot];

			if ((*object)[0]->data.objectStatus >= USABLE &&
				Item[object->usItem].usItemClass == IC_BLADE)
			{
				if (slot == HANDPOS || slot == SECONDHANDPOS)
					val += 25;
				else
					val += 15;

				break;
			}
		}
	}

	// chance is lowered if we are in deep water
	if (TERRAIN_IS_DEEP_WATER(self->position().terrainType()))
		val = max(0, val - 10);

	return static_cast<std::uint8_t>(min(100, max(0, val)));
}

// Flugente: interactive actions
std::uint16_t TacticalActorModifiers::interactiveActionSkill(
	TacticalActor& actor,
	std::uint16_t type)
{
	auto* const self = &actor;

	switch (type)
	{
		case INTERACTIVE_STRUCTURE_HACKABLE:
		{
			if (self->identity().profile() == ROBOT || IsVehicle(self))
				return 0;

			UINT16 skill = backgroundValue(actor, BG_HACKERSKILL);

			// without the background property, we cannot hack at all
			if ( !skill )
				return 0;

			FLOAT bestmodifier = 1.0f;

			for (size_t slot = 0, inventorySize = self->inventory().size();
				slot < inventorySize;
				++slot)
			{
				if (self->inventory()[slot].exists() &&
					Item[self->inventory()[slot].usItem].usHackingModifier)
				{
					OBJECTTYPE* object = &self->inventory()[slot];
					for (INT16 itemIndex = 0;
						itemIndex < object->ubNumberOfObjects;
						++itemIndex)
					{
						const FLOAT modifier =
							1.0f +
							(Item[self->inventory()[slot].usItem].usHackingModifier *
							 (*object)[itemIndex]->data.objectStatus) /
								10000.0f;

						if (modifier > bestmodifier)
							bestmodifier = modifier;
					}
				}
			}

			return (UINT16)(skill * bestmodifier);
		}
		break;

		case INTERACTIVE_STRUCTURE_READFILE:
		{
			if (self->identity().profile() == ROBOT || IsVehicle(self))
				return 0;

			// reading is governed by wisdom
			return self->statistics().wisdom();
		}
		break;

		case INTERACTIVE_STRUCTURE_WATERTAP:
		{
			if (self->identity().profile() == ROBOT || IsVehicle(self))
				return 0;

			// we are pros at drinking water
			return 100;
		}
		break;

		case INTERACTIVE_STRUCTURE_SODAMACHINE:
		{
			if (self->identity().profile() == ROBOT || IsVehicle(self))
				return 0;

			// we are pros at buying from a vending machine
			return 100;
		}
		break;

		case INTERACTIVE_STRUCTURE_MINIGAME:
		{
			if (self->identity().profile() == ROBOT || IsVehicle(self))
				return 0;

			// we are pros at playing games
			return 100;
		}
		break;

		case INTERACTIVE_STRUCTURE_VARIOUS:
		{
			if (self->identity().profile() == ROBOT || IsVehicle(self))
				return 0;

			// no idea what we're doing, but we're probably good at it
			return 100;
		}
		break;

		default:
			break;
	}

	return 0;
}

// Flugente: those with the <scrounging> background occasionally steal money
// from the locals
std::uint8_t TacticalActorModifiers::thiefStealMoneyChance(
	TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE || ( self->featureFlags().primaryFlags() & SOLDIER_POW ) )
		return 0;

	UINT32 val = 1 * EffectiveAgility( self, FALSE ) + 8 * EffectiveDexterity( self, FALSE ) + 10 * EffectiveExpLevel( self, FALSE );

	// personality/disability modifiers
	FLOAT persmodifier = 1.0f;
	//if ( DoesMercHaveDisability( this, HEAT_INTOLERANT ) )		persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, NERVOUS ) )				persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, CLAUSTROPHOBIC ) )		persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, NONSWIMMER ) )			persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, FEAR_OF_INSECTS ) )		persmodifier -= 0.20f;
	if ( DoesMercHaveDisability( self, FORGETFUL ) )			persmodifier -= 0.12f;
	//if ( DoesMercHaveDisability( this, PSYCHO ) )				persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, DEAF ) )					persmodifier -= 0.15f;
	if ( DoesMercHaveDisability( self, SHORTSIGHTED ) )			persmodifier -= 0.30f;
	//if ( DoesMercHaveDisability( this, HEMOPHILIAC ) )			persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, AFRAID_OF_HEIGHTS ) )	persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, SELF_HARM ) )			persmodifier -= 0.20f;

	if ( gGameOptions.fNewTraitSystem )
	{
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_SOCIABLE ) )		persmodifier += 0.25f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_LONER ) )		persmodifier -= 0.05f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_OPTIMIST ) )		persmodifier += 0.05f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_ASSERTIVE ) )	persmodifier += 0.15f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_INTELLECTUAL ) )	persmodifier += 0.15f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_PRIMITIVE ) )	persmodifier -= 0.15f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_AGGRESSIVE ) )	persmodifier -= 0.15f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_PHLEGMATIC ) )	persmodifier -= 0.05f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_DAUNTLESS ) )	persmodifier -= 0.13f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_PACIFIST ) )		persmodifier -= 0.03f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_MALICIOUS ) )	persmodifier -= 0.13f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_SHOWOFF ) )		persmodifier -= 0.08f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_COWARD ) )		persmodifier -= 0.25f;
	}

	UINT32 totalvalue =
		static_cast<UINT32>(
			max(0.0f, val * persmodifier / 10.0f));

	ReducePointsForFatigue(self, &totalvalue);

	totalvalue = min(static_cast<UINT32>(100), totalvalue);

	return static_cast<std::uint8_t>(totalvalue);
}

std::uint8_t TacticalActorModifiers::thiefEvadeDetectionChance(
	TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE )
		return 0;

	// the theoretical unboosted maximum is 1100, yet we treat it like 1000 - effectively you can boost stealth gear to give you a serious edge
	UINT32 val = 250 + 5 * EffectiveExpLevel( self, FALSE ) + 5 * EffectiveAgility( self, FALSE ) + 3 * GetWornStealth( self );

	ReducePointsForFatigue(self, &val);

	// personality/disability modifiers
	FLOAT persmodifier = 1.0f;
	//if ( DoesMercHaveDisability( this, HEAT_INTOLERANT ) )		persmodifier -= 0.20f;
	if ( DoesMercHaveDisability( self, NERVOUS ) )				persmodifier -= 0.04f;
	//if ( DoesMercHaveDisability( this, CLAUSTROPHOBIC ) )		persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, NONSWIMMER ) )			persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, FEAR_OF_INSECTS ) )		persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, FORGETFUL ) )			persmodifier -= 0.50f;
	//if ( DoesMercHaveDisability( this, PSYCHO ) )				persmodifier -= 0.20f;
	if ( DoesMercHaveDisability( self, DEAF ) )					persmodifier -= 0.06f;
	//if ( DoesMercHaveDisability( this, SHORTSIGHTED ) )			persmodifier -= 0.40f;
	//if ( DoesMercHaveDisability( this, HEMOPHILIAC ) )			persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, AFRAID_OF_HEIGHTS ) )	persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, SELF_HARM ) )			persmodifier -= 0.20f;

	UINT32 totalvalue =
		static_cast<UINT32>(
			max(0.0f, val * persmodifier / 10.0f));

	ReducePointsForFatigue(self, &totalvalue);

	totalvalue = min(static_cast<UINT32>(100), totalvalue);

	return static_cast<std::uint8_t>(totalvalue);
}
