#include "TacticalActorSkills.h"
#include "TacticalActorCovertOps.h"
#include "TacticalActorDragging.h"
#include "TacticalActorRadio.h"
#include "TacticalActorSpotting.h"
#include "TacticalActorTurncoats.h"
#include "TacticalActorWorldPlacement.h"

#include "Animation Control.h"
#include "Campaign.h"
#include "Campaign Types.h"
#include "Food.h"
#include "Game Clock.h"
#include "GameSettings.h"
#include "Handle UI.h"
#include "IMP Skill Trait.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Map Information.h"
#include "Overhead.h"
#include "Points.h"
#include "Queen Command.h"
#include "Soldier Control.h"
#include "Soldier macros.h"
#include "Squads.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "Vehicles.h"
#include "ai.h"
#include "message.h"
#include "opplist.h"

#include <cstdint>
#include <string>

extern SECTOR_EXT_DATA SectorExternalData[256][4];

// Check whether an actor can use a trait skill. Optional action-point checks
// are kept at this boundary so strategic callers do not need tactical state.
bool TacticalActorSkills::canUse(
	TacticalActor& actor,
	std::int32_t skill,
	bool checkForActionPoints,
	std::int32_t targetGridNo)
{
	auto* const self = &actor;

	if (skill < SKILLS_FIRST || skill >= SKILLS_MAX)
		return false;

	if (checkForActionPoints)
	{
		if (actor.collapseState().tactical())
			return false;
	}

	bool canuse = false;

	switch (skill)
	{
	// radio operator
	case SKILLS_RADIO_ARTILLERY:
		if ((!checkForActionPoints ||
			 EnoughPoints(
				 self,
				 APBPConstants[AP_RADIO],
				 APBPConstants[BP_RADIO],
				 FALSE)) &&
			TacticalActorRadio::canUse(
				actor,
				checkForActionPoints))
		{
			// we also have to check wether we can really order a strike from a sector
			UINT32 sector = 0;
			if (TacticalActorRadio::canOrderAnyArtilleryStrike(
					actor,
					&sector))
			{
				canuse = true;
			}
		}
		break;

	case SKILLS_RADIO_JAM:
	case SKILLS_RADIO_SCAN_FOR_JAM:
	case SKILLS_RADIO_LISTEN:
	case SKILLS_RADIO_CALLREINFORCEMENTS:
		if ((!checkForActionPoints ||
			 EnoughPoints(
				 self,
				 APBPConstants[AP_RADIO],
				 APBPConstants[BP_RADIO],
				 FALSE)) &&
			TacticalActorRadio::canUse(
				actor,
				checkForActionPoints))
		{
			canuse = true;
		}
		break;

	case SKILLS_RADIO_TURNOFF:
		if ((!checkForActionPoints ||
			 EnoughPoints(
				 self,
				 APBPConstants[AP_RADIO],
				 APBPConstants[BP_RADIO],
				 FALSE)) &&
			(TacticalActorRadio::isJamming(actor) ||
			 TacticalActorRadio::isScanning(actor) ||
			 TacticalActorRadio::isListening(actor)))
		{
			canuse = true;
		}
		break;

	case SKILLS_RADIO_ACTIVATE_TURNCOATS_ALL:
		if ( ( !checkForActionPoints || EnoughPoints( self, APBPConstants[AP_RADIO], APBPConstants[BP_RADIO], FALSE ) )
			&& TacticalActorRadio::canUse(actor, checkForActionPoints)
			&& gSkillTraitValues.fCOTurncoats
			&& !gbWorldSectorZ
			&& gTacticalStatus.ubInterruptPending == DISABLED_INTERRUPT
			&& IsFreeSlotAvailable( MILITIA_TEAM ) )
			canuse = true;
		break;

	case SKILLS_INTEL_CONCEAL:
	case SKILLS_INTEL_GATHERINTEL:
		// in order to conceal, we need:
		// - enemy team not aware of us (otherwise we could use this skill to instantly escape from combat)
		// - an enemy presence (otherwise, why bother)
		// - we must be alone (otherwise player could start combat again, at which point we'd need to appear from thin air)
		// - no militia present (same reason)
		// - no hostile civilians or creatures
		// - valid disguise
		{
			canuse = true;

			// we might already be on assignment, so be careful here
			INT8 sectorz = actor.deployment().sectorZ();
			if (SPY_LOCATION(actor.assignment().current()))
				sectorz = max( 0, sectorz - 10 );
			if (actor.deployment().sectorX() < 1 ||
				actor.deployment().sectorX() >= MAP_WORLD_X - 1 ||
				actor.deployment().sectorY() < 1 ||
				actor.deployment().sectorY() >= MAP_WORLD_Y - 1 ||
				sectorz < 0 ||
				sectorz >= 4)
			{
				return false;
			}

			// if we are disguised as a civilian, but there is a curfew here, don't allow that
			if (actor.featureFlags().primaryFlags() &
				SOLDIER_COVERT_CIV)
			{
				// civilians are suspicious if they are found in certain sectors. Especially at night
				// sector specific value:
				// 0 - civilians are always ok
				// 1 - civilians are suspicious at night
				// 2 - civilians are always suspicious
				// if underground, we still use the surface value

				UINT8 ubSectorId = SECTOR(
					actor.deployment().sectorX(),
					actor.deployment().sectorY());
				UINT8 sectordata = SectorExternalData[ubSectorId][sectorz].usCurfewValue;

				if ( sectordata > 1 )
					canuse = false;
				// is it night?
				else if ( sectordata == 1 && GetTimeOfDayAmbientLightLevel() < NORMAL_LIGHTLEVEL_DAY + 2 )
					canuse = false;
			}

			if (canuse &&
				NumEnemiesInAnySector(
					actor.deployment().sectorX(),
					actor.deployment().sectorY(),
					sectorz) > 0 &&
				NumPlayerTeamMembersInSector(
					actor.deployment().sectorX(),
					actor.deployment().sectorY(),
					actor.deployment().sectorZ()) == 1 &&
				(sectorz ||
				 NumNonPlayerTeamMembersInSector(
					 actor.deployment().sectorX(),
					 actor.deployment().sectorY(),
					 MILITIA_TEAM) == 0) &&
				TacticalActorCovertOps::seemsLegitimate(
					actor,
					actor.identity().id()))
			{
				// additional checks if we are in the currently loaded sector
				if (actor.deployment().sectorX() == gWorldSectorX &&
					actor.deployment().sectorY() == gWorldSectorY &&
					actor.deployment().sectorZ() == gbWorldSectorZ)
				{
					if ( gTacticalStatus.Team[ENEMY_TEAM].bAwareOfOpposition ||
						( IsJa2TacticalCombatActive() ) ||
						HostileCiviliansPresent() ||
						HostileCreaturesPresent() )
					{
						canuse = false;
					}
				}
			}
			else
			{
				canuse = false;
			}
		}
		break;

	case SKILLS_CREATE_TURNCOAT:
		// in order to try to create a turncoat, we need:
		// - a non-profile, not-already-turncoat enemy soldier
		// - enemy team not aware of us
		// - valid disguise
		// - enough AP to talk
		{
			TacticalActor* pSoldier =
				TileIsOutOfBounds(targetGridNo)
					? nullptr
					: SimpleFindSoldier(
						targetGridNo,
						gsInterfaceLevel);
			if ( pSoldier
				&& TacticalActorTurncoats::inPositionForAttempt(
					actor,
					pSoldier->identity().id())
				&& (!checkForActionPoints ||
					EnoughPoints(
						self,
						APBPConstants[AP_TALK],
						0,
						FALSE)))
			{
				canuse = true;
			}
		}
		break;

	case SKILLS_ACTIVATE_TURNCOATS:
		// not during an interrupt
		if ( gSkillTraitValues.fCOTurncoats
			&& !gbWorldSectorZ
			&& gTacticalStatus.ubInterruptPending == DISABLED_INTERRUPT
			&& IsFreeSlotAvailable( MILITIA_TEAM ) )
		{
			TacticalActor* pSoldier =
				TileIsOutOfBounds(targetGridNo)
					? nullptr
					: SimpleFindSoldier(
						targetGridNo,
						gsInterfaceLevel);
			if ( pSoldier
				&& pSoldier->roster().team() == ENEMY_TEAM
				&& pSoldier->identity().profile() == NO_PROFILE
				&& ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT )
				&& SOLDIER_CLASS_ENEMY( pSoldier->roster().soldierClass() ) )
			{
				canuse = true;
			}
		}
		break;

	case SKILLS_ACTIVATE_TURNCOATS_ALL:
		// not during an interrupt
		if ( gSkillTraitValues.fCOTurncoats
			&& !gbWorldSectorZ
			&& gTacticalStatus.ubInterruptPending == DISABLED_INTERRUPT
			&& !gSkillTraitValues.fCOTurncoats_SectorActivationRequiresRadioOperator
			&& IsFreeSlotAvailable( MILITIA_TEAM ) )
		{
			canuse = true;
		}
		break;

	case SKILLS_DISGUISE_APPLY_DISGUISE:
	case SKILLS_DISGUISE_REMOVE_CLOTHES:
		if (IS_MERC_BODY_TYPE(self) &&
			!(actor.featureFlags().primaryFlags() &
			  (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER)))
		{
			canuse = true;
		}
		break;

	case SKILLS_DISGUISE_REMOVE_DISGUISE:
	case SKILLS_DISGUISE_TEST_DISGUISE:
		if (IS_MERC_BODY_TYPE(self) &&
			(actor.featureFlags().primaryFlags() &
			 (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER)))
		{
			canuse = true;
		}
		break;

	case SKILLS_SPOTTER:
		if ((!checkForActionPoints ||
			 EnoughPoints(
				 self,
				 APBPConstants[AP_SPOTTER],
				 0,
				 FALSE)) &&
			TacticalActorSpotting::canSpot(actor))
		{
			canuse = true;
		}
		break;

	case SKILLS_FOCUS:
		// requires sniper trait, an aimed gun and only works on gridnos in our direction
		if (gGameOptions.fNewTraitSystem &&
			(HAS_SKILL_TRAIT(self, AUTO_WEAPONS_NT) ||
			 HAS_SKILL_TRAIT(self, HEAVY_WEAPONS_NT) ||
			 HAS_SKILL_TRAIT(self, SNIPER_NT) ||
			 HAS_SKILL_TRAIT(self, RANGER_NT) ||
			 HAS_SKILL_TRAIT(self, GUNSLINGER_NT)) &&
			HANDPOS < actor.inventory().size() &&
			actor.inventory()[HANDPOS].exists() &&
			actor.inventory()[HANDPOS].usItem < MAXITEMS &&
			(Item[actor.inventory()[HANDPOS].usItem].usItemClass &
			 (IC_GUN | IC_LAUNCHER)) &&
			WeaponReady(self) &&
			!TileIsOutOfBounds(actor.position().gridNo()) &&
			!TileIsOutOfBounds(targetGridNo) &&
			actor.position().direction() ==
				GetDirectionFromGridNo(targetGridNo, self))
		{
			canuse = true;
		}
		break;

	case SKILLS_DRAG:

		// TODO: a better check would be whether we can drag anything at the moment - CanDrag is more used for a specific person
		// sevenfm: added AP check to crouch before starting to drag
		if ((!checkForActionPoints ||
			 EnoughPoints(
				 self,
				 GetAPsToStartDrag(self, targetGridNo),
				 0,
				 FALSE)) &&
			TacticalActorDragging::canDrag(actor))
		{
			canuse = true;
		}
		break;

	case SKILLS_FILL_CANTEENS:
		if ( !((GetCurrentScreen() != GAME_SCREEN && GetCurrentScreen() != MSG_BOX_SCREEN) || (IsJa2TacticalCombatActive()) || gTacticalStatus.fEnemyInSector || gusSelectedSoldier == NOBODY) )
			canuse = true;
		break;

	default:
		break;
	}

	return canuse;
}

// Use a skill. Revalidate here because the selected actor or target can change
// while a tactical menu remains open.
bool TacticalActorSkills::use(
	TacticalActor& actor,
	std::uint32_t skill,
	std::int32_t targetGridNo,
	std::uint32_t targetId)
{
	auto* const self = &actor;

	if (skill >= SKILLS_MAX ||
		!canUse(
			actor,
			static_cast<std::int32_t>(skill),
			true,
			targetGridNo))
	{
		if (IsJa2TacticalWorldLoaded() &&
			New113Message[MSG113_CANNOT_USE_SKILL] != nullptr)
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_CANNOT_USE_SKILL] );
		}
		return false;
	}

	switch (skill)
	{
	// radio operator
	// the call for SKILLS_RADIO_ARTILLERY is only used by the AI
	case SKILLS_RADIO_ARTILLERY:
		{
			UINT32 sector = 0;
			if (TacticalActorRadio::canOrderAnyArtilleryStrike(
					actor,
					&sector))
			{
				return TacticalActorRadio::orderArtilleryStrike(
					actor,
					sector,
					targetGridNo,
					actor.roster().team());
			}
		}
		break;

	case SKILLS_RADIO_JAM:
		return TacticalActorRadio::startJamming(actor);

	case SKILLS_RADIO_SCAN_FOR_JAM:
		return TacticalActorRadio::startScanning(actor);

	case SKILLS_RADIO_LISTEN:
		return TacticalActorRadio::startListening(actor);

	case SKILLS_RADIO_CALLREINFORCEMENTS:
		// called separately
		// Reinforcement selection calls TacticalActorRadio directly.
		break;

	case SKILLS_RADIO_TURNOFF:
		return TacticalActorRadio::switchOff(actor);

	case SKILLS_RADIO_ACTIVATE_TURNCOATS_ALL:
		return TacticalActorRadio::orderAllTurncoats(actor);

	case SKILLS_INTEL_CONCEAL:
	case SKILLS_INTEL_GATHERINTEL:
		{
			// ATE: Patch fix If in a vehicle, remove from vehicle...
			TakeSoldierOutOfVehicle(self);

			// we store our location and later retrieve it, as the gridno will be set to NOWHERE
			actor.longAction().rememberContextGrid(
				actor.position().gridNo());

			// remove from squad
			RemoveCharacterFromSquads(self);

			ChangeSoldiersAssignment(
				self,
				CONCEALED + skill - SKILLS_INTEL_CONCEAL);

			// Remove soldier's graphic
			(void)TacticalActorWorldPlacement::removeFromGrid(actor);

			UpdateMercsInSector( gWorldSectorX, gWorldSectorY, gbWorldSectorZ );

			CheckForEndOfBattle( FALSE );

			CheckAndHandleUnloadingOfCurrentWorld();

			return true;
		}

	case SKILLS_CREATE_TURNCOAT:
		TacticalActorTurncoats::attempt(
			static_cast<SoldierID>(targetId));
		return true;

	case SKILLS_ACTIVATE_TURNCOATS:
		return TacticalActorTurncoats::orderOne(
			static_cast<SoldierID>(targetId));

	case SKILLS_ACTIVATE_TURNCOATS_ALL:
		TacticalActorTurncoats::orderAll();
		return true;

	case SKILLS_DISGUISE_APPLY_DISGUISE:
		TacticalActorCovertOps::disguise(actor);
		TacticalActorCovertOps::runSelfTest(actor);
		return true;

	case SKILLS_DISGUISE_REMOVE_DISGUISE:
		TacticalActorCovertOps::loseDisguise(actor);
		return true;

	case SKILLS_DISGUISE_TEST_DISGUISE:
		TacticalActorCovertOps::runSelfTest(actor);
		return true;

	case SKILLS_DISGUISE_REMOVE_CLOTHES:
		TacticalActorCovertOps::strip(actor);
		return true;

	case SKILLS_SPOTTER:
		return TacticalActorSpotting::startSpotting(
			actor,
			targetGridNo);

	case SKILLS_FOCUS:
		// activating skill on same gridno again deactivates it
		if ((actor.featureFlags().secondaryFlags() &
			 SOLDIER_TRAIT_FOCUS) &&
			actor.skillState().focusGrid() == targetGridNo)
		{
			actor.featureFlags().secondaryFlags() &=
				~SOLDIER_TRAIT_FOCUS;
			actor.skillState().clearFocus();

			return false;
		}
		else
		{
			actor.featureFlags().secondaryFlags() |=
				SOLDIER_TRAIT_FOCUS;
			actor.skillState().focusOn(targetGridNo);

			return true;
		}

	case SKILLS_DRAG:
		// sevenfm: change to crouch before dragging
		if (actor.animationPlayback().state() >=
			NUMANIMATIONSTATES)
		{
			return false;
		}
		if (gAnimControl[actor.animationPlayback().state()]
				.ubEndHeight != ANIM_CROUCH)
		{
			HandleStanceChangeFromUIKeys(ANIM_CROUCH);
		}
		if (targetGridNo != NOWHERE)
			TacticalActorDragging::dragStructure(
				actor,
				targetGridNo);
		else if (targetId < NOBODY)
			TacticalActorDragging::dragPerson(
				actor,
				targetId);
		else
			TacticalActorDragging::dragCorpse(
				actor,
				targetId - NOBODY);

		return true;

	case SKILLS_FILL_CANTEENS:
		SectorFillCanteens();
		break;

	default:
		break;
	}

	return false;
}

// Return a skill description or a synthesized list of unmet requirements.
// The legacy fixed destination buffer could overflow when translated strings
// were longer than the English originals.
const wchar_t* TacticalActorSkills::description(
	TacticalActor& actor,
	std::int32_t skill,
	std::int32_t targetGridNo)
{
	static thread_local std::wstring descriptionText;

	if (skill < SKILLS_FIRST || skill >= SKILLS_MAX)
	{
		descriptionText.clear();
		return descriptionText.c_str();
	}

	if (canUse(actor, skill, true, targetGridNo))
	{
		const auto* const description =
			pTraitSkillsMenuDescStrings[skill];
		return description != nullptr ? description : L"";
	}

	const auto* const requirementHeader =
		pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_REQ];
	descriptionText.assign(
		requirementHeader != nullptr ? requirementHeader : L"");

	const auto appendFormatted =
		[](
			const wchar_t* format,
			auto... arguments)
		{
			if (format == nullptr)
				return;

			CHAR16 formatted[200] = {};
			if (swprintf(formatted, format, arguments...) >= 0)
				descriptionText.append(formatted);
		};

	if (skill >= SKILLS_RADIO_FIRST &&
		skill <= SKILLS_RADIO_LAST)
	{
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_AP],
			APBPConstants[AP_RADIO]);
	}

	switch (skill)
	{
	// radio operator
	case SKILLS_RADIO_ARTILLERY:
	case SKILLS_RADIO_JAM:
	case SKILLS_RADIO_SCAN_FOR_JAM:
	case SKILLS_RADIO_LISTEN:
	case SKILLS_RADIO_CALLREINFORCEMENTS:
	case SKILLS_RADIO_TURNOFF:
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_TXT],
			gzMercSkillTextNew[RADIO_OPERATOR_NT]);
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_TXT],
			New113Message[MSG113_WORKING_RADIO_SET]);
		break;

	case SKILLS_RADIO_ACTIVATE_TURNCOATS_ALL:
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_TXT],
			gzMercSkillTextNew[RADIO_OPERATOR_NT]);
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_TXT],
			New113Message[MSG113_WORKING_RADIO_SET]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_NOT_DURING_INTERRUPT]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_TURNED_ENEMY]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_SURFACELEVEL]);
		break;

	case SKILLS_INTEL_CONCEAL:
	case SKILLS_INTEL_GATHERINTEL:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_ENEMYSECTOR]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_SINGLEMERC]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_NOALARM]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_DISGUISE_CIV_OR_MIL]);
		break;

	case SKILLS_CREATE_TURNCOAT:
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_ENEMY]);
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_NOALARM]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_DISGUISE_CIV_OR_MIL]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_SURFACELEVEL]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_STRATEGIC_SUSPICION]);
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_AP],
			APBPConstants[AP_TALK]);
		break;

	case SKILLS_ACTIVATE_TURNCOATS:
	case SKILLS_ACTIVATE_TURNCOATS_ALL:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_NOT_DURING_INTERRUPT]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_TURNED_ENEMY]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_SURFACELEVEL]);
		break;

	case SKILLS_DISGUISE_APPLY_DISGUISE:
	case SKILLS_DISGUISE_REMOVE_CLOTHES:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_NOT_DISGUISED]);
		break;

	case SKILLS_DISGUISE_REMOVE_DISGUISE:
	case SKILLS_DISGUISE_TEST_DISGUISE:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_DISGUISE_CIV_OR_MIL]);
		break;

	case SKILLS_SPOTTER:
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_AP],
			APBPConstants[AP_SPOTTER]);
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_TXT],
			New113Message[MSG113_BINOCULAR]);
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_TXT],
			New113Message[MSG113_PATIENCE]);
		break;

	case SKILLS_FOCUS:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_GUNTRAIT]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_AIMEDGUN]);
		break;

	case SKILLS_DRAG:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_PRONEPERSONORCORPSE]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_FREEHANDS]);
		break;

	case SKILLS_FILL_CANTEENS:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_NOT_IN_COMBAT]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_FRIENDLY_SECTOR]);
		break;

	default:
		break;
	}

	return descriptionText.c_str();
}
