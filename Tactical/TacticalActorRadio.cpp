#include "TacticalActorRadio.h"
#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorEquipment.h"
#include "TacticalActorSkills.h"
#include "TacticalActorTurncoats.h"
#include "TacticalActorStateFlags.h"

#include "Animation Control.h"
#include "Campaign.h"
#include "Campaign Types.h"
#include "CampaignStats.h"
#include "Dialogue Control.h"
#include "Game Clock.h"
#include "GameSettings.h"
#include "IMP Skill Trait.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Map Information.h"
#include "Overhead.h"
#include "Points.h"
#include "Queen Command.h"
#include "TacticalActor.h"
#include "Soldier Profile.h"
#include "Soldier macros.h"
#include "SoldierRepository.h"
#include "Sound Control.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "Town Militia.h"
#include "Weapons.h"
#include "message.h"
#include "random.h"
#include "strategicmap.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
bool isRadioRobot(const TacticalActor& actor)
{
	const auto profile = actor.identity().profile();
	return profile < NUM_PROFILES &&
		gMercProfiles[profile].ubBodyType == ROBOTNOWEAPON;
}

OBJECTTYPE* radioObject(TacticalActor& actor)
{
	const bool radioRobot = isRadioRobot(actor);
	const bool playerUsesNewInventory =
		actor.roster().team() == OUR_TEAM &&
		UsingNewInventorySystem();
	const bool usesDedicatedSlot =
		radioRobot || playerUsesNewInventory;
	std::size_t slot = actor.inventory().size();
	if (radioRobot)
	{
		slot = ROBOT_UTILITY_SLOT;
	}
	else if (playerUsesNewInventory)
	{
		slot = CPACKPOCKPOS;
	}

	if (slot < actor.inventory().size())
	{
		OBJECTTYPE& object = actor.inventory()[slot];
		if (object.exists() &&
			object.usItem < MAXITEMS &&
			HasItemFlag(object.usItem, RADIO_SET))
		{
			return &object;
		}
	}

	if (usesDedicatedSlot)
		return nullptr;

	OBJECTTYPE* const object =
		TacticalActorEquipment::objectWithFlag(actor, RADIO_SET);
	return object != nullptr &&
			object->exists() &&
			object->usItem < MAXITEMS &&
			HasItemFlag(object->usItem, RADIO_SET)
		? object
		: nullptr;
}
}

bool TacticalActorRadio::canUse(
	TacticalActor& actor,
	bool checkForActionPoints)
{
	auto* const self = &actor;

	// An upgraded robot does not need the radio-operator trait.
	if (isRadioRobot(actor))
	{
		return radioObject(actor) != nullptr &&
			(!checkForActionPoints ||
			 EnoughPoints(
				 self,
				 APBPConstants[AP_RADIO],
				 0,
				 FALSE));
	}

	// only radio operators can use this equipment
	if (!NUM_SKILL_TRAITS(self, RADIO_OPERATOR_NT))
		return false;

	// if we check whether we have enough AP, exit if we don't
	if (checkForActionPoints &&
		!EnoughPoints(
			self,
			APBPConstants[AP_RADIO],
			0,
			FALSE))
	{
		return false;
	}

	return radioObject(actor) != nullptr;
}

bool TacticalActorRadio::use(TacticalActor& actor)
{
	auto* const self = &actor;
	if (!canUse(actor, false))
	{
		reportFailure(actor);
		return false;
	}

	bool success = false;

	OBJECTTYPE* const object = radioObject(actor);
	if (object != nullptr && !object->objectStack.empty())
	{
		// status % chance of success
		if (Chance((*object)[0]->data.objectStatus))
			success = true;
	}

	if (actor.roster().inSector() &&
		(actor.identity().bodyType() == REGMALE ||
		 actor.identity().bodyType() == BIGMALE) &&
		actor.animationPlayback().state() < NUMANIMATIONSTATES)
	{
		switch (gAnimControl[actor.animationPlayback().state()].ubEndHeight)
		{
		case ANIM_STAND:
			TacticalActorAnimationTransitions::initializeAnimation(actor, AI_RADIO, 0, FALSE);
			break;

		case ANIM_CROUCH:
			TacticalActorAnimationTransitions::initializeAnimation(actor, AI_CR_RADIO, 0, FALSE);
			break;
		}
	}

	DeductPoints(
		self,
		APBPConstants[AP_RADIO],
		APBPConstants[BP_RADIO],
		0);

	// we gain a bit of experience... - even more if we are the one who began the communication
	StatChange(self, EXPERAMT, actor.roster().inSector() ? 8 : 4, TRUE);
	StatChange(self, MECHANAMT, 1, TRUE);

	if (!success)
	{
		reportFailure(actor);
		return false;
	}

	return true;
}


bool TacticalActorRadio::canOrderAnyArtilleryStrike(
	TacticalActor& actor,
	std::uint32_t* sectorId)
{
	if (sectorId == nullptr || !gSkillTraitValues.fROAllowArtillery)
		return false;

	if (actor.deployment().sectorZ())
		return false;

	// if we are AI-controlled, we have to wait for our timer to run out
	if (actor.roster().team() != gbPlayerNum &&
		actor.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY))
	{
		return false;
	}

	// check wether we can call artillery from the 4 adjacent sectors
	for (UINT8 i = 0; i < 4; ++i)
	{
		INT16 loopX = actor.deployment().sectorX();
		INT16 loopY = actor.deployment().sectorY();

		if ( i == 0 )		++loopY;
		else if ( i == 1 )	++loopX;
		else if ( i == 2 )	--loopY;
		else if ( i == 3 )	--loopX;

		if ( loopX < 1 || loopX >= MAP_WORLD_X - 1 || loopY < 1 || loopY >= MAP_WORLD_Y - 1 )
			continue;

		// as the player team can order artillery from the militia, we have to check that too.
		if (isValidArtillerySector(
				loopX,
				loopY,
				actor.deployment().sectorZ(),
				actor.roster().team()) ||
			(actor.roster().team() == gbPlayerNum &&
			 isValidArtillerySector(
				 loopX,
				 loopY,
				 actor.deployment().sectorZ(),
				 MILITIA_TEAM)))
		{
			*sectorId = static_cast<std::uint32_t>(SECTOR(loopX, loopY));
			return true;
		}
	}

	return false;
}

bool TacticalActorRadio::orderArtilleryStrike(
	TacticalActor& actor,
	std::uint32_t sectorId,
	std::int32_t targetGridNo,
	std::uint8_t team)
{
	auto* const self = &actor;

	if (sectorId > UINT8_MAX ||
		(team != OUR_TEAM &&
		 team != ENEMY_TEAM &&
		 team != MILITIA_TEAM) ||
		TileIsOutOfBounds(targetGridNo))
	{
		return false;
	}

	if (!TacticalActorSkills::canUse(
			actor,
			SKILLS_RADIO_ARTILLERY,
			true))
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_CANNOT_USE_SKILL] );
		return false;
	}

	// Radio eligibility is separate from the sector-wide jamming state.
	if (sectorJammed())
	{
		// only display message and play sound on our team - no need to signify to player that AI is trying to call in artillery
		if (team == OUR_TEAM || team == MILITIA_TEAM)
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_RADIO_JAMMED_NO_COMMUNICATION] );

			PlayJA2SampleFromFile(
				"Sounds\\radioerror.wav",
				RATE_11025,
				SoundVolume(MIDVOLUME, actor.position().gridNo()),
				1,
				SoundDir(actor.position().gridNo()));
		}

		return false;
	}

	// sector number is in UINT32, even though INT16 would be normal
	const auto compactSector = static_cast<UINT8>(sectorId);
	INT16 sSectorX = SECTORX(compactSector);
	INT16 sSectorY = SECTORY(compactSector);

	// just to make sure...
	if (!isValidArtillerySector(
			sSectorX,
			sSectorY,
			actor.deployment().sectorZ(),
			team))
	{
		return false;
	}

	// use the radio, this handles animation, batteries etc.
	if (!use(actor))
		return false;

	// determine from where the shells will come
	INT32 sStartingGridNo = gMapInformation.sNorthGridNo;
	if (sSectorX < actor.deployment().sectorX())
		sStartingGridNo = gMapInformation.sWestGridNo;
	else if (sSectorX > actor.deployment().sectorX())
		sStartingGridNo = gMapInformation.sEastGridNo;
	else if (sSectorY > actor.deployment().sectorY())
		sStartingGridNo = gMapInformation.sSouthGridNo;

	if ( sStartingGridNo == -1 )
		sStartingGridNo = gMapInformation.sCenterGridNo;

	if ( TileIsOutOfBounds( sStartingGridNo ) )
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_INCORRECT_GRIDNO_ARTILLERY] );
		return false;
	}

	// Locate item indices for Signal and HE shells defined by the active MOD. Evade usage of hard-code values.
	static UINT16 usSignalShellIndex = NOTHING;
	static UINT16 usHeShellIndex = NOTHING;
	if (usSignalShellIndex == NOTHING ||
		usSignalShellIndex >= MAXITEMS ||
		!HasItemFlag(usSignalShellIndex, SIGNAL_SHELL) ||
		usHeShellIndex == NOTHING ||
		usHeShellIndex >= MAXITEMS)
	{
		UINT16 findSignalShellIndex = 1700;  // try default Signal Shell item in 1.13
		UINT16 findHeShellIndex = 140;       // try default HE Shell item in 1.13
		if ((findSignalShellIndex >= MAXITEMS ||
			 HasItemFlag(findSignalShellIndex, SIGNAL_SHELL) == FALSE) &&
			(GetFirstItemWithFlag(
				 &findSignalShellIndex,
				 SIGNAL_SHELL) == FALSE ||
			 findSignalShellIndex >= MAXITEMS))
		{
			ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_NO_SIGNAL_SHELL]);
			return false;
		}
		UINT16 mortarIndex = GetLauncherFromLaunchable(findSignalShellIndex);
		if (mortarIndex >= MAXITEMS)
		{
			ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_NO_DEFAULT_SHELL]);
			return false;
		}
		if (findHeShellIndex >= MAXITEMS ||
			mortarIndex != GetLauncherFromLaunchable(findHeShellIndex))
		{
			findHeShellIndex = GetLaunchableOfExplosionType(mortarIndex, EXPLOSV_NORMAL);
		}
		if (findHeShellIndex == NOTHING ||
			findHeShellIndex >= MAXITEMS)
		{
			ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_NO_DEFAULT_SHELL]);
			return false;
		}
		// at this point both shells were found and are OK, so set it to static variables and never touch anymore:
		usSignalShellIndex = findSignalShellIndex;
		usHeShellIndex = findHeShellIndex;
	}

	// if a strike is ordered from the ENEMY_TEAM or MILITIA_TEAM, the number of mortars depends on the number of enemies/militia in that sector
	// number of waves depends on the number and quality of enemies/soldiers
	// only HE shells will be fired this way
	if (team == ENEMY_TEAM || team == MILITIA_TEAM)
	{
		if (gSkillTraitValues.usVOMortarCountDivisor == 0 ||
			gSkillTraitValues.usVOMortarShellDivisor == 0)
		{
			return false;
		}

		std::int64_t nummortars = 0;  // number of mortars determines size of wave (1 - 4)
		std::int64_t numwaves = 0;    // number of waves
		std::int64_t numshells = 0;   // number of shells
		const std::int64_t numwavesMax =
			Explosive[Item[usSignalShellIndex].ubClassIndex]
				.ubDuration;

		SECTORINFO *pSector = &SectorInfo[SECTOR( sSectorX, sSectorY )];

		if (team == ENEMY_TEAM)
		{
			// we also have to account for mobile groups
			GROUP *pGroup = gpGroupList;
			while ( pGroup )
			{
				if (pGroup->usGroupTeam == team &&
					!pGroup->fVehicle &&
					pGroup->ubSectorX == sSectorX &&
					pGroup->ubSectorY == sSectorY)
				{
					nummortars += pGroup->ubGroupSize;
					numshells +=
						static_cast<std::int64_t>(
							gSkillTraitValues.usVOMortarPointsTroop) *
						pGroup->ubGroupSize;
				}
				pGroup = pGroup->next;
			}

			nummortars += pSector->ubNumAdmins + pSector->ubNumTroops + pSector->ubNumElites;
			nummortars /= gSkillTraitValues.usVOMortarCountDivisor;
			numshells +=
				static_cast<std::int64_t>(
					gSkillTraitValues.usVOMortarPointsAdmin) *
					pSector->ubNumAdmins +
				static_cast<std::int64_t>(
					gSkillTraitValues.usVOMortarPointsTroop) *
					pSector->ubNumTroops +
				static_cast<std::int64_t>(
					gSkillTraitValues.usVOMortarPointsElite) *
					pSector->ubNumElites;
		}
		else if (team == MILITIA_TEAM)
		{
			UINT8 militia_green = MilitiaInSectorOfRank( sSectorX, sSectorY, GREEN_MILITIA );
			UINT8 militia_troop = MilitiaInSectorOfRank( sSectorX, sSectorY, REGULAR_MILITIA );
			UINT8 militia_elite = MilitiaInSectorOfRank( sSectorX, sSectorY, ELITE_MILITIA );

			nummortars = (militia_green + militia_troop + militia_elite) / gSkillTraitValues.usVOMortarCountDivisor;
			numshells =
				static_cast<std::int64_t>(
					gSkillTraitValues.usVOMortarPointsAdmin) *
					militia_green +
				static_cast<std::int64_t>(
					gSkillTraitValues.usVOMortarPointsTroop) *
					militia_troop +
				static_cast<std::int64_t>(
					gSkillTraitValues.usVOMortarPointsElite) *
					militia_elite;
		}

		// turn number of mortar points into number of shells; in case of "militia use sector ammo" option, numshells
		// represents max potential shells militia can shot for this artillery strike.
		numshells = numshells / gSkillTraitValues.usVOMortarShellDivisor;

		if (numshells <= 0)
		{
			if (team == MILITIA_TEAM)  // player does not care if enemy team has not enough points to strike
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_NOT_ENOUGH_MORTAR_SHELLS] );
			return false;
		}

		if (nummortars <= 0)
		{
			if (team == MILITIA_TEAM)  // player does not care if enemy team has not enough men to strike
				ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_NO_MORTARS]);
			return false;
		}

		numwaves = std::max<std::int64_t>(
			1,
			numshells / nummortars);
		if (gSkillTraitValues.fROArtilleryDistributedOverTurns)  // if delay between waves is enabled, we shouldn't overextend, so trim to
			numwaves = std::min(numwaves, numwavesMax);          // signal duration; it doesn't matter if delay is disabled.
		numwaves = std::min<std::int64_t>(
			numwaves,
			std::numeric_limits<INT16>::max());

		// send a signal shell at first. This marks the area that the shells will come in
		ArtilleryStrike(
			usSignalShellIndex,
			actor.identity().id() + 2,
			sStartingGridNo,
			targetGridNo);

		// we just 'plant' the mortar shells as bombs. We time them so that they will be fired at the beginning of the next turn
		// for every 'wave' of shells, we just plant one and then clone them when firing
		// create mortar shell item
		OBJECTTYPE shellobj;
		if (!CreateItem(usHeShellIndex, 100, &shellobj) ||
			shellobj.objectStack.empty())
		{
			return false;
		}

		shellobj.fFlags |= OBJECT_ARMED_BOMB;
		shellobj[0]->data.misc.bDetonatorType = BOMB_TIMED;
		shellobj[0]->data.misc.usBombItem = shellobj.usItem;
		shellobj[0]->data.misc.ubBombOwner = actor.identity().id() + 2;

		// delay in RT is one turn. In TB we have to make that 2 turns, as otherwise the attack can happen instantly.
		// Also use 2 if we are AI, otherwise the shells will fly immediately at the player's turn, giving him no chance to react (blame the way turns are handled)

		shellobj[0]->data.misc.bDelay = 1;
		if (team == ENEMY_TEAM || !(IsJa2TacticalTurnBasedCombat()))
			shellobj[0]->data.misc.bDelay += 1;

		// now set special flags - we simply abuse the ubWireNetworkFlag
		switch ( nummortars )
		{
		case 1:
			shellobj[0]->data.ubWireNetworkFlag = ARTILLERY_STRIKE_COUNT_1;
			break;

		case 2:
			shellobj[0]->data.ubWireNetworkFlag = ARTILLERY_STRIKE_COUNT_2;
			break;

		case 3:
			shellobj[0]->data.ubWireNetworkFlag = (ARTILLERY_STRIKE_COUNT_1 | ARTILLERY_STRIKE_COUNT_2);
			break;

		case 4:
		default:
			shellobj[0]->data.ubWireNetworkFlag = ARTILLERY_STRIKE_COUNT_4;
			break;
		}

		for (std::int64_t wave = 0; wave < numwaves; ++wave)
		{
			AddItemToPool( sStartingGridNo, &shellobj, HIDDEN_ITEM, 1, WORLD_ITEM_ARMED_BOMB, 0 );

			// if option is set, delay each wave by one turn
			if (gSkillTraitValues.fROArtilleryDistributedOverTurns &&
				shellobj[0]->data.misc.bDelay <
					std::numeric_limits<INT8>::max())
			{
				shellobj[0]->data.misc.bDelay += 1;
			}
		}

		// update the sector Artillery time
		pSector->uiTimeAIArtillerywasOrdered = GetWorldTotalMin( );

		// extra xp for succesfully ordering an artillery strike
		StatChange(self, EXPERAMT, 10, TRUE);

		// we add a bit to the counter, thus the AI has to wait a bit between ordering strikes (otherwise they'll instantly order all available strikes)
		actor.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY) = 2;
	}
	else if (team == OUR_TEAM)
	{
		// if we call a strike from our mercs, everything gets more complicated. We don't calculate the number of mortars or shells as an estimate, we have to search the inventory
		// of every merc fit for shelling in that sector for mortars and shells. But thanks to this, we can also other shell-types, like mustard or phosphor
		// Sector validation already proved that somebody has a radio set and
		// somebody has a mortar.
		// sadly, we have to run over this 2 times. On the first run, we have to search for all mortar items and remember them (there can be different mortar systems, can't fire a 40mm shell with a 60mm mortar)

		// as of 2013-09-25, I say it is no longer necessary to fire a signal shell first. The player can fire a signal shell (by mortar or hand) manually to mark one or more targets if he wants
		// if he does not do so, active vox operators will be targetted. Who knows, the vox operator might be doing a heroic last stand for all we know...
		//BOOLEAN signalshellfired = FALSE;
		const UINT8 maxFiringMortarsAmount = 5;
		SoldierID radiooperatorID = NOBODY;
		UINT8 mortaritemcnt = 0;
		UINT16 mortararray[maxFiringMortarsAmount] = { 0 };

		TacticalActor* pSoldier = NULL;
		SoldierID cnt = gTacticalStatus.Team[team].bFirstID;
		const SoldierID lastid = gTacticalStatus.Team[team].bLastID;
		for (;
			 cnt <= lastid &&
			 mortaritemcnt < maxFiringMortarsAmount;
			 ++cnt)
		{
			pSoldier = GetJa2SoldierRepository().resolve( cnt );
			// check if soldier exists in this sector
			if (!pSoldier ||
				!pSoldier->roster().active() ||
				pSoldier->deployment().sectorX() != sSectorX ||
				pSoldier->deployment().sectorY() != sSectorY ||
				pSoldier->deployment().sectorZ() !=
					actor.deployment().sectorZ() ||
				pSoldier->assignment().current() > ON_DUTY)
			{
				continue;
			}

			if (canUse(*pSoldier))
				radiooperatorID = cnt;

			for (std::size_t slot = 0;
				 slot < pSoldier->inventory().size() &&
				 mortaritemcnt < maxFiringMortarsAmount;
				 ++slot)
			{
				const OBJECTTYPE& object = pSoldier->inventory()[slot];
				if (object.exists() &&
					object.usItem < MAXITEMS &&
					ItemIsMortar(object.usItem))
				{
					// if not already in list, remember this mortar
					bool alreadyInList = false;
					for (UINT8 i = 0; i < mortaritemcnt; ++i)
						if (mortararray[i] == object.usItem)
						{
							alreadyInList = true;
							break;
						}

					if (!alreadyInList)
						mortararray[mortaritemcnt++] = object.usItem;
				}
			}
		}

		// safety check, this shouldn't be happening
		if ( !mortaritemcnt )
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_NO_MORTARS] );
			return false;
		}

		// depending on wether the mortars have ammunition, a radio operator will give a different dialogue
		BOOLEAN shellsfired = FALSE;

		// second loop: check for all mortar shells and 'fire' them
		cnt = gTacticalStatus.Team[team].bFirstID;
		for (; cnt <= lastid; ++cnt)
		{
			pSoldier = GetJa2SoldierRepository().resolve( cnt );
			// check if soldier exists in this sector
			if (!pSoldier ||
				!pSoldier->roster().active() ||
				pSoldier->deployment().sectorX() != sSectorX ||
				pSoldier->deployment().sectorY() != sSectorY ||
				pSoldier->deployment().sectorZ() !=
					actor.deployment().sectorZ() ||
				pSoldier->assignment().current() > ON_DUTY)
			{
				continue;
			}

			INT8 shelldelay = 1;
			// In realtime the player could choose to put down a bomb right before a turn expires, so add 1 to the setting in RT
			if ( !(IsJa2TacticalTurnBasedCombat()) )
				++shelldelay;

			for (std::size_t slot = 0;
				 slot < pSoldier->inventory().size();
				 ++slot)
			{
				OBJECTTYPE& inventoryObject =
					pSoldier->inventory()[slot];
				if (inventoryObject.exists() &&
					inventoryObject.usItem < MAXITEMS)
				{
					if (ItemIsMortar(inventoryObject.usItem))
					{
						OBJECTTYPE* pAttObj =
							FindAttachmentByClass(
								&inventoryObject,
								IC_BOMB);

						// as of 2013-09-25, also fire these, as they are no longer necessary for a barrage
						// only fire if not signal shell, we already fired one, no need to do so again
						if (pAttObj &&
							pAttObj->exists() &&
							pAttObj->usItem < MAXITEMS &&
							HasItemFlag(
								pAttObj->usItem,
								SIGNAL_SHELL) == FALSE)
						{
							// if option is set, delay each wave by one turn
							if (gSkillTraitValues.fROArtilleryDistributedOverTurns &&
								shelldelay <
									std::numeric_limits<INT8>::max())
							{
								++shelldelay;
							}

							// create mortar shell item
							OBJECTTYPE shellobj;
							if (!CreateItem(
									pAttObj->usItem,
									100,
									&shellobj) ||
								shellobj.objectStack.empty())
							{
								continue;
							}

							// plant bomb data
							shellobj.fFlags |= OBJECT_ARMED_BOMB;
							shellobj[0]->data.misc.bDetonatorType = BOMB_TIMED;
							shellobj[0]->data.misc.usBombItem = shellobj.usItem;
							shellobj[0]->data.misc.ubBombOwner =
								actor.identity().id() + 2;

							shellobj[0]->data.misc.bDelay = shelldelay;

							shellobj[0]->data.ubWireNetworkFlag = ARTILLERY_STRIKE_COUNT_1;

							AddItemToPool( sStartingGridNo, &shellobj, HIDDEN_ITEM, 1, WORLD_ITEM_ARMED_BOMB, 0 );

							shellsfired = TRUE;

							DeductAmmo(pSoldier, &inventoryObject);
						}
					}

					if (Item[inventoryObject.usItem].usItemClass ==
						IC_BOMB)
					{
						// found a bomb - if this fits any found mortar, fire it
						for ( UINT8 i = 0; i < mortaritemcnt; ++i )
						{
							if (ValidLaunchable(
									inventoryObject.usItem,
									mortararray[i]))
							{
								OBJECTTYPE* pShellObj = &inventoryObject;

								// only fire if not signal shell, we already fired one, no need to do so again
								if (!HasItemFlag(
										pShellObj->usItem,
										SIGNAL_SHELL))
								{
									// if option is set, delay each wave by one turn
									if (gSkillTraitValues.fROArtilleryDistributedOverTurns &&
										shelldelay <
											std::numeric_limits<INT8>::max())
									{
										++shelldelay;
									}

									// create mortar shell item
									OBJECTTYPE shellobj;
									if (!CreateItem(
											pShellObj->usItem,
											100,
											&shellobj) ||
										shellobj.objectStack.empty())
									{
										continue;
									}

									// plant bomb data
									shellobj.fFlags |= OBJECT_ARMED_BOMB;
									shellobj[0]->data.misc.bDetonatorType = BOMB_TIMED;
									shellobj[0]->data.misc.usBombItem = shellobj.usItem;
									shellobj[0]->data.misc.ubBombOwner =
										actor.identity().id() + 2;

									shellobj[0]->data.misc.bDelay = shelldelay;

									shellobj[0]->data.ubWireNetworkFlag = ARTILLERY_STRIKE_COUNT_1;

									const std::size_t shellCount =
										std::min<std::size_t>(
											pShellObj->ubNumberOfObjects,
											pShellObj->objectStack.size());
									for (std::size_t shell = 0;
										 shell < shellCount;
										 ++shell)
									{
										AddItemToPool( sStartingGridNo, &shellobj, HIDDEN_ITEM, 1, WORLD_ITEM_ARMED_BOMB, 0 );

										shellsfired = TRUE;
									}

									// remove the shells: Delete object
									DeleteObj( pShellObj );
									break;
								}
							}
						}
					}
				}
			}
		}

		pSoldier =
			GetJa2SoldierRepository().resolve(
				radiooperatorID );
		if ( pSoldier != nullptr )
		{
			// also drain the other guy's radio batteries
			(void)use(*pSoldier);

			if ( shellsfired )
				TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_DO_BATTLE_SND, BATTLE_SOUND_OK1, 500 );
			else
				DelayedTacticalCharacterDialogue( pSoldier, QUOTE_OUT_OF_AMMO );
		}

		if ( shellsfired )
		{
			// extra xp for succesfully ordering an artillery strike
			StatChange(self, EXPERAMT, 10, TRUE);

			// we add a bit to the counter, thus the AI has to wait a bit between ordering strikes (otherwise they'll instantly order all available strikes)
			actor.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY) = 2;
		}
	}
	else
		// how did this even happen?
		return false;

	if (team == ENEMY_TEAM)
		gCurrentIncident.usIncidentFlags |= INCIDENT_ARTILLERY_ENEMY;
	else
		gCurrentIncident.usIncidentFlags |= INCIDENT_ARTILLERY_PLAYERSIDE;

	return true;
}

bool TacticalActorRadio::isJamming(TacticalActor& actor)
{
	if (actor.featureFlags().primaryFlags() &
		SOLDIER_RADIO_OPERATOR_JAMMING)
	{
		if (canUse(actor, false))
			return true;

		// A lost or broken radio immediately ends the mode.
		actor.featureFlags().primaryFlags() &=
			~SOLDIER_RADIO_OPERATOR_JAMMING;
	}

	return false;
}

bool TacticalActorRadio::startJamming(TacticalActor& actor)
{
	// not possible if already jamming
	if (actor.featureFlags().primaryFlags() &
		SOLDIER_RADIO_OPERATOR_JAMMING)
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_ALREADY_JAMMING] );
		return false;
	}

	// use the radio, this handles animation, batteries etc.
	if (!use(actor))
		return false;

	// stop other radio activities
	switchOff(actor);

	// add flag
	actor.featureFlags().primaryFlags() |=
		SOLDIER_RADIO_OPERATOR_JAMMING;

	// play sound
	PlayJA2SampleFromFile(
		"Sounds\\radioerror2.wav",
		RATE_11025,
		SoundVolume(MIDVOLUME, actor.position().gridNo()),
		1,
		SoundDir(actor.position().gridNo()));

	return true;
}

bool TacticalActorRadio::isScanning(TacticalActor& actor)
{
	if (actor.featureFlags().primaryFlags() &
		SOLDIER_RADIO_OPERATOR_SCANNING)
	{
		if (canUse(actor, false))
			return true;

		actor.featureFlags().primaryFlags() &=
			~SOLDIER_RADIO_OPERATOR_SCANNING;
	}

	return false;
}

bool TacticalActorRadio::startScanning(TacticalActor& actor)
{
	// not possible if already scanning
	if (actor.featureFlags().primaryFlags() &
		SOLDIER_RADIO_OPERATOR_SCANNING)
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_ALREADY_SCANNING] );
		return false;
	}

	// use the radio, this handles animation, batteries etc.
	if (!use(actor))
		return false;

	// stop other radio activities
	switchOff(actor);

	// add flag
	actor.featureFlags().primaryFlags() |=
		SOLDIER_RADIO_OPERATOR_SCANNING;

	// play sound
	PlayJA2SampleFromFile(
		"Sounds\\scan1.wav",
		RATE_11025,
		SoundVolume(MIDVOLUME, actor.position().gridNo()),
		1,
		SoundDir(actor.position().gridNo()));

	return true;
}

bool TacticalActorRadio::isListening(TacticalActor& actor)
{
	if (!(actor.featureFlags().primaryFlags() &
		  SOLDIER_RADIO_OPERATOR_LISTENING))
	{
		return false;
	}

	if (canUse(actor, false))
		return true;

	actor.featureFlags().primaryFlags() &=
		~SOLDIER_RADIO_OPERATOR_LISTENING;
	return false;
}

bool TacticalActorRadio::startListening(TacticalActor& actor)
{
	// not possible if already scanning
	if (actor.featureFlags().primaryFlags() &
		SOLDIER_RADIO_OPERATOR_LISTENING)
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_ALREADY_LISTENING] );
		return false;
	}

	// use the radio, this handles animation, batteries etc.
	if (!use(actor))
		return false;

	// stop other radio activities
	switchOff(actor);

	// add flag
	actor.featureFlags().primaryFlags() |=
		SOLDIER_RADIO_OPERATOR_LISTENING;

	// play sound
	PlayJA2SampleFromFile(
		"Sounds\\scan1.wav",
		RATE_11025,
		SoundVolume(MIDVOLUME, actor.position().gridNo()),
		1,
		SoundDir(actor.position().gridNo()));

	return true;
}

// Flugente: order reinforcements from src sector to target sector
extern BOOLEAN CallMilitiaReinforcements( INT16 sTargetMapX, INT16 sTargetMapY, INT16 sSrcMapX, INT16 sSrcMapY, UINT16 sNumber );

bool TacticalActorRadio::callReinforcements(
	TacticalActor& actor,
	std::uint32_t sourceSector,
	std::uint16_t number)
{
	if (!gGameExternalOptions.gfAllowReinforcements ||
		sourceSector > UINT8_MAX ||
		number == 0)
	{
		return false;
	}

	// use the radio, this handles animation, batteries etc.
	if (!use(actor))
		return false;

	// Radio eligibility is separate from the sector-wide jamming state.
	if (sectorJammed())
	{
		// The radio-use path handles its own failure feedback.
		return false;
	}

	// Flugente: order reinforcements from src sector to target sector
	if (CallMilitiaReinforcements(
			actor.deployment().sectorX(),
			actor.deployment().sectorY(),
			SECTORX(static_cast<UINT8>(sourceSector)),
			SECTORY(static_cast<UINT8>(sourceSector)),
			number))
	{
		CHAR16 pStr2[128];
		GetSectorIDString(
			SECTORX(static_cast<UINT8>(sourceSector)),
			SECTORY(static_cast<UINT8>(sourceSector)),
			0,
			pStr2,
			FALSE);

		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			New113Message[MSG113_ORDERS_REINFORCEMENTS],
			actor.identity().name(),
			pStr2);

		// play sound
		PlayJA2SampleFromFile(
			"Sounds\\scan1.wav",
			RATE_11025,
			SoundVolume(MIDVOLUME, actor.position().gridNo()),
			1,
			SoundDir(actor.position().gridNo()));

		return true;
	}

	return false;
}

bool TacticalActorRadio::switchOff(TacticalActor& actor) noexcept
{
	// erasing the flags is enough
	actor.featureFlags().primaryFlags() &=
		~(SOLDIER_RADIO_OPERATOR_JAMMING |
		  SOLDIER_RADIO_OPERATOR_SCANNING |
		  SOLDIER_RADIO_OPERATOR_LISTENING);

	return true;
}

bool TacticalActorRadio::orderAllTurncoats(TacticalActor& actor)
{
	// not possible if already scanning
	if (!gSkillTraitValues.fCOTurncoats)
		return false;

	// use the radio, this handles animation, batteries etc.
	if (!use(actor))
		return false;

	TacticalActorTurncoats::orderAll();

	return true;
}

// display and error sound used either when the radio set fails or the sector is jammed - the player knows of the error, but cannot be sure of the cause
void TacticalActorRadio::reportFailure(TacticalActor& actor)
{
	// only display message and play sound if on player team
	if (actor.roster().team() == gbPlayerNum &&
		actor.roster().inSector() &&
		IsJa2TacticalWorldLoaded())
	{
		if (New113Message[MSG113_RADIO_ACTION_FAILED] != nullptr)
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_RADIO_ACTION_FAILED] );
		}

		PlayJA2SampleFromFile(
			"Sounds\\radioerror.wav",
			RATE_11025,
			SoundVolume(MIDVOLUME, actor.position().gridNo()),
			1,
			SoundDir(actor.position().gridNo()));
	}
}


// Flugente: enemy roles

// Flugente: boxing fix: this shall be the only location where the boxing flag gets removed (easier debugging)

bool TacticalActorRadio::operatorSignal(
	SoldierID ownerId,
	std::int32_t* targetGridNo)
{
	if (targetGridNo == nullptr)
		return false;

	const UINT16 owner = static_cast<UINT16>(ownerId);

	// get the 'real owner'
	if ( owner > 1 )
	{
		// a merc planted this - if he's a radio operator, use his gridno
		TacticalActor* pSoldier =
			GetJa2SoldierRepository().resolve(
				owner - 2 );

		if (pSoldier &&
			canUse(*pSoldier, false) &&
			pSoldier->roster().active() &&
			pSoldier->roster().inSector() &&
			pSoldier->deployment().sectorX() == gWorldSectorX &&
			pSoldier->deployment().sectorY() == gWorldSectorY &&
			pSoldier->deployment().sectorZ() == gbWorldSectorZ &&
			!TileIsOutOfBounds(pSoldier->position().gridNo()))
		{
			*targetGridNo = pSoldier->position().gridNo();
			//pSoldier->roster().side();
			return true;
		}
	}
	// check for the side that ordered this
	else
	{
		UINT8 bTeam = MILITIA_TEAM;
		if ( owner != 0 )
			bTeam = ENEMY_TEAM;

		TacticalActor* pSoldier = NULL;
		SoldierID cnt = gTacticalStatus.Team[bTeam].bFirstID;
		const SoldierID lastid = gTacticalStatus.Team[bTeam].bLastID;
		for (; cnt <= lastid; ++cnt)
		{
			pSoldier =
				GetJa2SoldierRepository().resolve(
					cnt );
			if (pSoldier &&
				canUse(*pSoldier, false) &&
				pSoldier->roster().active() &&
				pSoldier->roster().inSector() &&
				pSoldier->deployment().sectorX() == gWorldSectorX &&
				pSoldier->deployment().sectorY() == gWorldSectorY &&
				pSoldier->deployment().sectorZ() == gbWorldSectorZ &&
				!TileIsOutOfBounds(pSoldier->position().gridNo()))
			{
				*targetGridNo = pSoldier->position().gridNo();
				//pSoldier->roster().side();
				return true;
			}
		}
	}

	return false;
}

bool TacticalActorRadio::isValidArtillerySector(
	std::int16_t sectorX,
	std::int16_t sectorY,
	std::int8_t sectorZ,
	std::uint8_t team)
{
	if (team != ENEMY_TEAM &&
		team != MILITIA_TEAM &&
		team != OUR_TEAM)
	{
		return false;
	}
	if ((team == ENEMY_TEAM || team == MILITIA_TEAM) &&
		(gSkillTraitValues.usVOMortarCountDivisor == 0 ||
		 gSkillTraitValues.usVOMortarShellDivisor == 0))
	{
		return false;
	}

	// is the sector valid?
	if (sectorZ != 0 ||
		sectorX < 1 ||
		sectorX >= MAP_WORLD_X - 1 ||
		sectorY < 1 ||
		sectorY >= MAP_WORLD_Y - 1)
	{
		return false;
	}

	UINT16 usEnemies = (UINT16)NumNonPlayerTeamMembersInSector( sectorX, sectorY, ENEMY_TEAM );
	UINT16 usMilitia = (UINT16)NumNonPlayerTeamMembersInSector( sectorX, sectorY, MILITIA_TEAM );
	UINT16 usMercs = (UINT16)PlayerMercsInSector( (UINT8)sectorX, (UINT8)sectorY, (UINT8)sectorZ );

	SECTORINFO *pSectorInfo = &(SectorInfo[SECTOR( sectorX, sectorY )]);

	// sector must be free of members of an opposing team
	if (team == ENEMY_TEAM)
	{
		if ( !usEnemies || usMilitia || usMercs )
			return false;

		// there have to be enough guys here to fire at least one shot
		if ( usEnemies < gSkillTraitValues.usVOMortarCountDivisor )
			return false;

		const std::uint64_t availableShellPoints =
			static_cast<std::uint64_t>(usEnemies) *
			gSkillTraitValues.usVOMortarPointsAdmin;
		const std::uint64_t requiredShellPoints =
			static_cast<std::uint64_t>(
				gSkillTraitValues.usVOMortarShellDivisor) *
			(usEnemies /
			 gSkillTraitValues.usVOMortarCountDivisor);
		if (availableShellPoints < requiredShellPoints)
			return false;

		// cannot fire if artillery was used recently
		if ( GetWorldTotalMin( ) < pSectorInfo->uiTimeAIArtillerywasOrdered + gSkillTraitValues.bVOArtillerySectorFrequency )
			return false;
	}
	else if (team == MILITIA_TEAM)
	{
		if ( usEnemies || !usMilitia )
			return false;

		// there have to be enough guys here to fire at least one shot
		if ( usMilitia < gSkillTraitValues.usVOMortarCountDivisor )
			return false;

		const std::uint64_t availableShellPoints =
			static_cast<std::uint64_t>(usMilitia) *
			gSkillTraitValues.usVOMortarPointsAdmin;
		const std::uint64_t requiredShellPoints =
			static_cast<std::uint64_t>(
				gSkillTraitValues.usVOMortarShellDivisor) *
			(usMilitia /
			 gSkillTraitValues.usVOMortarCountDivisor);
		if (availableShellPoints < requiredShellPoints)
			return false;

		// cannot fire if artillery was used recently
		if ( GetWorldTotalMin( ) < pSectorInfo->uiTimeAIArtillerywasOrdered + gSkillTraitValues.bVOArtillerySectorFrequency )
			return false;
	}
	else if (team == OUR_TEAM)
	{
		if ( usEnemies || !usMercs )
			return false;

		// we can relay orders only if someone in the sector has a working radio set and a mortar
		BOOLEAN activeradio = FALSE;
		BOOLEAN mortarfound = FALSE;
		TacticalActor* pSoldier = NULL;
		SoldierID cnt = gTacticalStatus.Team[team].bFirstID;
		const SoldierID lastid = gTacticalStatus.Team[team].bLastID;
		for (; cnt <= lastid; ++cnt)
		{
			pSoldier =
				GetJa2SoldierRepository().resolve(
					cnt );
			// check if soldier exists in this sector, and is on duty
			if (!pSoldier ||
				!pSoldier->roster().active() ||
				pSoldier->deployment().sectorX() != sectorX ||
				pSoldier->deployment().sectorY() != sectorY ||
				pSoldier->deployment().sectorZ() != sectorZ ||
				pSoldier->assignment().current() > ON_DUTY)
				continue;

			if (canUse(*pSoldier, false))
				activeradio = TRUE;

			if (TacticalActorEquipment::hasMortar(*pSoldier))
				mortarfound = TRUE;
		}

		if ( !activeradio || !mortarfound )
			return false;
	}

	return true;
}

bool TacticalActorRadio::sectorJammed()
{
	// check every soldier: are we jamming frequencies?
	TacticalActor* pSoldier = NULL;
	SoldierID  cnt = gTacticalStatus.Team[OUR_TEAM].bFirstID;
	SoldierID  lastid = MAX_NUM_SOLDIERS;
	for ( ; cnt < lastid; ++cnt )
	{
		pSoldier =
			GetJa2SoldierRepository().resolve(
				cnt );
		if (pSoldier != nullptr &&
			pSoldier->deployment().sectorX() == gWorldSectorX &&
			pSoldier->deployment().sectorY() == gWorldSectorY &&
			pSoldier->deployment().sectorZ() == gbWorldSectorZ &&
			pSoldier->vitals().health() > 0 &&
			isJamming(*pSoldier))
		{
			return true;
		}
	}

	return false;
}

bool TacticalActorRadio::playerTeamScanning()
{
	// check every soldier: are we jamming frequencies?
	TacticalActor* pSoldier = NULL;
	SoldierID  cnt = gTacticalStatus.Team[OUR_TEAM].bFirstID;
	SoldierID  lastid = gTacticalStatus.Team[OUR_TEAM].bLastID;
	for (; cnt <= lastid; ++cnt)
	{
		pSoldier =
			GetJa2SoldierRepository().resolve(
				cnt );
		if (pSoldier != nullptr &&
			pSoldier->deployment().sectorX() == gWorldSectorX &&
			pSoldier->deployment().sectorY() == gWorldSectorY &&
			pSoldier->deployment().sectorZ() == gbWorldSectorZ &&
			pSoldier->vitals().health() > 0 &&
			isScanning(*pSoldier))
		{
			return true;
		}
	}

	return false;
}
