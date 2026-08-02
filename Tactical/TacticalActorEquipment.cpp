#include "TacticalActorEquipment.h"
#include "TacticalActorBattleSounds.h"
#include "TacticalActorModifiers.h"

#include "Animation Control.h"
#include "Campaign Types.h"
#include "Food.h"
#include "Game Clock.h"
#include "GameSettings.h"
#include "Interface.h"
#include "Interface Panels.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "LightEffects.h"
#include "LOS.h"
#include "Map Information.h"
#include "Overhead.h"
#include "PATHAI.H"
#include "Points.h"
#include "Soldier Control.h"
#include "SoldierRepository.h"
#include "Sound Control.h"
#include "Tactical Save.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "Weapons.h"
#include "lighting.h"
#include "message.h"
#include "opplist.h"
#include "random.h"
#include "renderworld.h"
#include "soundman.h"
#include "tiledef.h"
#include "worldman.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

extern void HandleItemCooldownFunctions( OBJECTTYPE* itemStack, INT32 deltaSeconds, BOOLEAN isUnderground = TRUE );
static bool addBestFlashlight(TacticalActor& actor);

namespace
{
void destroyEquippedRiotShield(TacticalActor& actor)
{
	auto* const self = &actor;
	OBJECTTYPE* shield =
		TacticalActorEquipment::equippedRiotShield(actor);

	if (!shield)
		return;

	const UINT16 brokenShield =
		Item[shield->usItem].usBuddyItem;
	if (brokenShield != NOTHING &&
		brokenShield < MAXITEMS &&
		!TileIsOutOfBounds(self->position().gridNo()))
	{
		CreateItem(brokenShield, 100, shield);

		// A broken shield belongs on the ground, not in the actor's hand.
		AddItemToPool(
			self->position().gridNo(),
			shield,
			1,
			self->position().level(),
			0,
			-1);

		NotifySoldiersToLookforItems();
	}

	DeleteObj(shield);

	ScreenMsg(
		FONT_MCOLOR_LTYELLOW,
		MSG_INTERFACE,
		New113Message[MSG113_SHIELD_DESTROYED],
		self->GetName());

	DirtyMercPanelInterface(self, DIRTYLEVEL2);
	TacticalActorBattleSounds::play(*self, BATTLE_SOUND_CURSE1);
}
}

bool TacticalActorEquipment::carriesTwoHandedWeapon(
	const TacticalActor& actor)
{
	const UINT16 item = actor.inventory()[HANDPOS].usItem;

	return actor.inventory()[HANDPOS].exists() &&
		item < MAXITEMS &&
		ItemIsTwoHanded(item);
}

// Flugente: Cool down/decay all items in inventory
void TacticalActorEquipment::coolDownInventory(TacticalActor& actor)
{
	// if we have any active flashlights (in our hands for simplicity), drain their batteries
	// do this check for both hands
	// we do not lower a battery's status all the time - as an INT8, it would reach 0 way to fast. Instead we only have 5% chance of doing so, thereby increasing a battery's life
	if ( Chance( 5 ) )
	{
		const std::size_t flashlightSlotEnd =
			std::min(
				actor.inventory().size(),
				static_cast<std::size_t>(VESTPOCKPOS));
		for (std::size_t slot = HANDPOS;
			 slot < flashlightSlotEnd;
			 ++slot)
		{
			OBJECTTYPE* object = &actor.inventory()[slot];

			if (!object->exists() || object->usItem >= MAXITEMS)
				continue;

			OBJECTTYPE* battery = FindAttachedBatteries(object);
			if (!battery || !battery->exists())
				continue;

			bool flashlightFound =
				Item[object->usItem].usFlashLightRange > 0;

			if (!flashlightFound)
			{
				for (const auto& attachment : (*object)[0]->attachments)
				{
					if (attachment.exists() &&
						attachment.usItem < MAXITEMS &&
						Item[attachment.usItem].usFlashLightRange)
					{
						flashlightFound = true;
						break;
					}
				}
			}

			if (flashlightFound)
			{
				if ((*battery)[0]->data.objectStatus <= 1)
				{
					battery->RemoveObjectsFromStack(1);
					if (!battery->exists())
						object->RemoveAttachment(battery);
				}
				else
				{
					--(*battery)[0]->data.objectStatus;
				}
			}
		}
	}

	// handle flashlight. This is necessary in this location, as we need to do this at least once per turn
	refreshFlashlights(actor);

	if ( !gGameExternalOptions.fWeaponOverheating && !UsingFoodSystem() )
		return;

	constexpr std::int32_t secondsPassed = 5;
	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		HandleItemCooldownFunctions(&actor.inventory()[slot], secondsPassed);
	}
}

// Flugente: return weapon currently used
OBJECTTYPE* TacticalActorEquipment::usedWeapon(
	const TacticalActor& actor,
	OBJECTTYPE* object)
{
	if (!object)
		return nullptr;

	if ( actor.attackSelection().weaponMode() == WM_ATTACHED_UB ||
		 actor.attackSelection().weaponMode() == WM_ATTACHED_UB_BURST ||
		 actor.attackSelection().weaponMode() == WM_ATTACHED_UB_AUTO )
	{
		OBJECTTYPE* pObjUnderBarrel = FindAttachedWeapon(object, IC_GUN);

		if ( pObjUnderBarrel )
			return(pObjUnderBarrel);
	}
	else if (actor.attackSelection().weaponMode() == WM_ATTACHED_BAYONET)
	{
		OBJECTTYPE* pObjUnderBarrel = FindAttachedWeapon(object, IC_BLADE);

		if ( pObjUnderBarrel )
			return(pObjUnderBarrel);
	}

	return object;
}

std::uint16_t TacticalActorEquipment::usedWeaponNumber(
	const TacticalActor& actor,
	OBJECTTYPE* object)
{
	if (!object)
		return NOTHING;

	if ( actor.attackSelection().weaponMode() == WM_ATTACHED_UB ||
		 actor.attackSelection().weaponMode() == WM_ATTACHED_UB_BURST ||
		 actor.attackSelection().weaponMode() == WM_ATTACHED_UB_AUTO )
	{
		UINT16 weaponnr = GetAttachedWeapon(object, IC_GUN);

		if ( weaponnr != NONE )
			return(weaponnr);
	}
	else if (actor.attackSelection().weaponMode() == WM_ATTACHED_BAYONET)
	{
		UINT16 weaponnr = GetAttachedWeapon(object, IC_BLADE);

		if ( weaponnr != NONE )
			return(weaponnr);
	}

	return object->usItem;
}

// Flugente: do we currently provide ammo (pAmmoSlot) for someone else's
// (pubId) gun (pGunSlot)?
bool TacticalActorEquipment::externalFeeding(
	TacticalActor& actor,
	SoldierID* pubId1,
	std::uint16_t* pGunSlot1,
	std::uint16_t* pAmmoSlot1,
	SoldierID* pubId2,
	std::uint16_t* pGunSlot2,
	std::uint16_t* pAmmoSlot2)
{
	if (!pubId1 || !pGunSlot1 || !pAmmoSlot1 ||
		!pubId2 || !pGunSlot2 || !pAmmoSlot2)
		return false;

	auto* const self = &actor;

	// make sure we have to check this...
	if ( gGameExternalOptions.ubExternalFeeding == 0 )
		return false;

	//  basic check if we are up to this task
	if ( !self->roster().active() || !self->roster().inSector() || self->vitals().health() < OKLIFE )
		return(FALSE);

	// this is odd - invalid GridNo... well, no feeding then
	if ( TileIsOutOfBounds( self->position().gridNo() ) )
		return(FALSE);

	BOOLEAN	isFeeding = FALSE;

	UINT16 usGunItem = 0;
	UINT8  usGunCalibre = 0;
	UINT8  usGunAmmoType = 0;

	UINT16 usAmmoItem = 0;
	UINT8  usAmmoCalibre = 0;
	UINT8  usAmmoAmmoType = 0;

	UINT16 usMagIndex = 0;

	BOOLEAN firstgunfound = FALSE;

	// do this check for both hands
	UINT16 firstslot = HANDPOS;
	UINT16 lastslot = VESTPOCKPOS;
	for ( UINT16 invpos = firstslot; invpos < lastslot; ++invpos )
	{
		// do we have ammo in our hands?
		OBJECTTYPE* pAmmoObj = &(self->inventory()[invpos]);

		if (!pAmmoObj ||
			!pAmmoObj->exists() ||
			pAmmoObj->usItem >= MAXITEMS ||
			Item[pAmmoObj->usItem].usItemClass != IC_AMMO ||
			(*pAmmoObj)[0]->data.ubShotsLeft <= 0)
			// can't use this, end
			continue;

		usAmmoItem = pAmmoObj->usItem;

		if ( !HasItemFlag( usAmmoItem, AMMO_BELT ) )
			continue;

		usMagIndex = Item[usAmmoItem].ubClassIndex;
		if (usMagIndex > MAXITEMS)
			continue;

		usAmmoCalibre = Magazine[usMagIndex].ubCalibre;
		usAmmoAmmoType = Magazine[usMagIndex].ubAmmoType;

		// our current stance is important
		UINT8 usOurStance = gAnimControl[self->animationPlayback().state()].ubEndHeight;

		// we will check wether one of our teammates is on the gridno we face
		INT32 nextGridNoinSight = NewGridNo( self->position().gridNo(), DirectionInc( self->position().direction() ) );

		TacticalActor* pTeamSoldier = NULL;
		SoldierID  cnt = gTacticalStatus.Team[self->roster().team()].bFirstID;
		SoldierID  lastid = gTacticalStatus.Team[self->roster().team()].bLastID;
		for ( ; cnt < lastid; ++cnt )
		{
			pTeamSoldier =
				GetJa2SoldierRepository().resolve( cnt );
			// check if teamsoldier exists in this sector
			if ( !pTeamSoldier || !pTeamSoldier->roster().active() || !pTeamSoldier->roster().inSector() || pTeamSoldier->deployment().sectorX() != self->deployment().sectorX() || pTeamSoldier->deployment().sectorY() != self->deployment().sectorY() || pTeamSoldier->deployment().sectorZ() != self->deployment().sectorZ() )
				continue;

			// check if both soldiers are on the same level
			if ( self->position().level() != pTeamSoldier->position().level() )
				continue;

			// determine wether we can physically provide ammo to our teammate.
			// check the stance, prone on standing (both ways) doesn't work
			if ( usOurStance == ANIM_STAND )
			{
				if ( gAnimControl[pTeamSoldier->animationPlayback().state()].ubEndHeight != ANIM_STAND && gAnimControl[pTeamSoldier->animationPlayback().state()].ubEndHeight != ANIM_CROUCH )
					continue;
			}
			else if ( usOurStance == ANIM_PRONE )
			{
				if ( gAnimControl[pTeamSoldier->animationPlayback().state()].ubEndHeight != ANIM_PRONE && gAnimControl[pTeamSoldier->animationPlayback().state()].ubEndHeight != ANIM_CROUCH )
					continue;
			}

			// check if we look at our teammate, or look the same way he does, or in the direction between
			BOOLEAN fPositioningOkay = FALSE;
			// the other person must be near
			if ( SpacesAway( self->position().gridNo(), pTeamSoldier->position().gridNo() ) == 0 )
			{
				// same tile -> its ourself -> ok
				fPositioningOkay = TRUE;
			}
			else if ( SpacesAway( self->position().gridNo(), pTeamSoldier->position().gridNo() ) == 1 )
			{
				// we look at him -> ok
				if ( nextGridNoinSight == pTeamSoldier->position().gridNo() )
					fPositioningOkay = TRUE;
				else
				{
					// if we look at the same tile, then that's okay too
					INT32 teamsoldiernextGridNoinSight = NewGridNo( pTeamSoldier->position().gridNo(), DirectionInc( pTeamSoldier->position().direction() ) );

					if ( nextGridNoinSight == teamsoldiernextGridNoinSight )
						fPositioningOkay = TRUE;
					else
					{
						// if we both look in the same direction...
						INT8 teammatedirection = pTeamSoldier->position().direction();
						INT8 ourdirection = self->position().direction();

						if ( teammatedirection == ourdirection )
						{
							// if the angle between our teammates sightline and the direct line from us to him is 90 degrees, then we are also able to supply
							INT8 ourrightdirection = (ourdirection + 2) % NUM_WORLD_DIRECTIONS;
							INT8 ourleftdirection =
								(ourdirection + NUM_WORLD_DIRECTIONS - 2) %
								NUM_WORLD_DIRECTIONS;

							if ( NewGridNo( self->position().gridNo(), DirectionInc( ourrightdirection ) ) == pTeamSoldier->position().gridNo() || NewGridNo( self->position().gridNo(), DirectionInc( ourleftdirection ) ) == pTeamSoldier->position().gridNo() )
								fPositioningOkay = TRUE;
						}
					}
				}
			}

			if ( !fPositioningOkay )
				continue;

			// ok, we are facing a teammate. Check if he has a gun in any hand that still has ammo left
			UINT16 pTeamSoldierfirstslot = HANDPOS;
			UINT16 pTeamSoldierlastslot = VESTPOCKPOS;
			for ( UINT16 teamsoldierinvpos = pTeamSoldierfirstslot; teamsoldierinvpos < pTeamSoldierlastslot; ++teamsoldierinvpos )
			{
				OBJECTTYPE* pObjInHands = &(pTeamSoldier->inventory()[teamsoldierinvpos]);
				if (pObjInHands &&
					pObjInHands->exists() &&
					pObjInHands->usItem < MAXITEMS &&
					Item[pObjInHands->usItem].usItemClass == IC_GUN &&
					(HasItemFlag(pObjInHands->usItem, BELT_FED) ||
					 HasAttachmentOfClass(pObjInHands, AC_FEEDER)) &&
					(*pObjInHands)[0]->data.gun.ubGunShotsLeft > 0)
				{
					// remember the caliber and type of ammo. They all have to fit
					usGunItem = pObjInHands->usItem;

					usGunCalibre = Weapon[usGunItem].ubCalibre;
					usGunAmmoType = (*pObjInHands)[0]->data.gun.ubGunAmmoType;

					if ( usGunCalibre == usAmmoCalibre && /*usGunMagSize == usAmmoMagSize &&*/ usGunAmmoType == usAmmoAmmoType )
					{
						// same calibre, same magsize, same ammotype. We can serve this guy
						if ( !firstgunfound )
						{
							firstgunfound = TRUE;
							(*pubId1) = cnt;
							(*pGunSlot1) = teamsoldierinvpos;
							(*pAmmoSlot1) = invpos;
							isFeeding = TRUE;
							break;
						}
						else
						{
							(*pubId2) = cnt;
							(*pGunSlot2) = teamsoldierinvpos;
							(*pAmmoSlot2) = invpos;
							isFeeding = TRUE;

							// we really found a second gun. we can only serve 2 guns maximum. lets end this
							return(isFeeding);
						}
					}
				}
			}
		}
	}

	// if set to 1, we do not wether we feed ourself from our inventory
	if ( gGameExternalOptions.ubExternalFeeding < 2 )
		return(isFeeding);

	// if we reach this point, we have checked all our teammates, and we do not provide external feeding for any of them
	// it is possible that we provide external feeding for OURSELF (think of ammo belts in a dedicated LBE slot, or of a gun that requires a separate energy source)
	// first, determine wether we need external feeding for our gun. We do this for both hands, as it is thinkable that someone has 2 one-handed guns with external feeding

	// this determines which slots we'll search for ammo
	UINT16 firstslotforammo = MEDPOCK1POS;
	UINT16 lastslotforammo = MEDPOCK3POS;

	// for robots and AI-controlled soldiers (who don't have any LBE gear), we put a change in here so that ALL their slots are checked for ammo
	if ( self->roster().team() != gbPlayerNum ||
		(self->status().flags() & SOLDIER_ROBOT) )
	{
		firstslotforammo = HANDPOS;
		lastslotforammo = NUM_INV_SLOTS;
	}
	else
	{
		// as a merc, the only slots that are valid for external feeding are the 2 medium-sized slots on a vest (because I say so). And that only if the vest is allowed to do that, which we will now check:
		if (!self->inventory()[VESTPOCKPOS].exists() ||
			self->inventory()[VESTPOCKPOS].usItem >= MAXITEMS ||
			!HasItemFlag(
				self->inventory()[VESTPOCKPOS].usItem,
				AMMO_BELT_VEST))
			return(isFeeding);
	}

	UINT16 searchgunfirstslot = HANDPOS;
	UINT16 searchgunlastslot = VESTPOCKPOS;
	for ( UINT16 invpos = searchgunfirstslot; invpos < searchgunlastslot; ++invpos )
	{
		// check our hands for guns
		OBJECTTYPE* pObj = &(self->inventory()[invpos]);

		UINT16 usGunItem = pObj->usItem;

		if (!pObj ||
			!pObj->exists() ||
			usGunItem >= MAXITEMS ||
			Item[usGunItem].usItemClass != IC_GUN ||
			!(HasItemFlag(usGunItem, BELT_FED) ||
			  HasAttachmentOfClass(pObj, AC_FEEDER)) ||
			(*pObj)[0]->data.gun.ubGunShotsLeft <= 0)
			// can't use this, end
			continue;

		// remember the caliber and type of ammo. They all have to fit
		usGunCalibre = Weapon[usGunItem].ubCalibre;
		usGunAmmoType = (*pObj)[0]->data.gun.ubGunAmmoType;

		// now check the inventory for an ammo belt. If we are not from the player team or a robot, we will search the entire inventory
		for ( UINT16 bLoop = firstslotforammo; bLoop < lastslotforammo; ++bLoop )
		{
			if (self->inventory()[bLoop].exists())
			{
				OBJECTTYPE* pAmmoObj = &self->inventory()[bLoop];

				if (pAmmoObj->usItem < MAXITEMS)
				{
					//if ( pAmmoObj->ubNumberOfObjects == 1 )
					{
						usAmmoItem = pAmmoObj->usItem;

						if (Item[usAmmoItem].usItemClass == IC_AMMO &&
							HasItemFlag(usAmmoItem, AMMO_BELT))
						{
							// remember the caliber and type of ammo. They all have to fit
							usMagIndex = Item[usAmmoItem].ubClassIndex;
							if (usMagIndex > MAXITEMS)
								continue;

							usAmmoCalibre = Magazine[usMagIndex].ubCalibre;
							usAmmoAmmoType = Magazine[usMagIndex].ubAmmoType;

							if ( usGunCalibre == usAmmoCalibre && usGunAmmoType == usAmmoAmmoType )
							{
								// same calibre, same ammotype. We can serve this guy
								if ( !firstgunfound )
								{
									firstgunfound = TRUE;
									(*pubId1) = self->identity().id();
									(*pGunSlot1) = invpos;
									(*pAmmoSlot1) = bLoop;
									isFeeding = TRUE;
									break;
								}
								else
								{
									(*pubId2) = self->identity().id();
									(*pGunSlot2) = invpos;
									(*pAmmoSlot2) = bLoop;
									isFeeding = TRUE;

									// we really found a second gun. we can only serve 2 guns maximum. lets end this
									return(isFeeding);
								}
							}
						}
					}
				}
			}
		}
	}

	return(isFeeding);
}

// Flugente: return first found object with a specific flag from our inventory
OBJECTTYPE* TacticalActorEquipment::objectWithFlag(
	TacticalActor& actor,
	std::uint64_t flag)
{
	const auto inventorySize = actor.inventory().size();

	for (std::size_t slot = 0; slot < inventorySize; ++slot)
	{
		if (actor.inventory()[slot].exists() &&
			actor.inventory()[slot].usItem < MAXITEMS &&
			HasItemFlag(actor.inventory()[slot].usItem, flag))
		{
			return &actor.inventory()[slot];
		}
	}

	return nullptr;
}

// Flugente: scuba gear
bool TacticalActorEquipment::usesScubaGear(const TacticalActor& actor)
{
	if (!TERRAIN_IS_HIGH_WATER(actor.position().terrainType()) ||
		actor.position().level() > 0)
		return false;

	// do we wear a scuba mask?
	if (!(actor.inventory()[HEAD1POS].exists() &&
		  actor.inventory()[HEAD1POS].usItem < MAXITEMS &&
		  HasItemFlag(actor.inventory()[HEAD1POS].usItem, SCUBA_MASK)) &&
		!(actor.inventory()[HEAD2POS].exists() &&
		  actor.inventory()[HEAD2POS].usItem < MAXITEMS &&
		  HasItemFlag(actor.inventory()[HEAD2POS].usItem, SCUBA_MASK)))
		return false;

	if (!(actor.inventory()[CPACKPOCKPOS].exists() &&
		  actor.inventory()[CPACKPOCKPOS].usItem < MAXITEMS &&
		  HasItemFlag(actor.inventory()[CPACKPOCKPOS].usItem, SCUBA_BOTTLE)) &&
		!(actor.inventory()[BPACKPOCKPOS].exists() &&
		  actor.inventory()[BPACKPOCKPOS].usItem < MAXITEMS &&
		  HasItemFlag(actor.inventory()[BPACKPOCKPOS].usItem, SCUBA_BOTTLE)))
		return false;

	return true;
}

bool TacticalActorEquipment::dropSectorEquipment(TacticalActor& actor)
{
	auto* const self = &actor;

	// not if we already dropped the gear
	if (self->featureFlags().primaryFlags() & SOLDIER_EQUIPMENT_DROPPED)
		return false;

	// set marker: we are about to drop our gear
	self->featureFlags().primaryFlags() |= SOLDIER_EQUIPMENT_DROPPED;

	const std::size_t inventorySize =
		std::min(
			self->inventory().size(),
			static_cast<std::size_t>(NUM_INV_SLOTS));

	auto shouldDrop = [](const OBJECTTYPE& object)
	{
		return object.exists() &&
			object.usItem < MAXITEMS &&
			!(object.fFlags & OBJECT_UNDROPPABLE) &&
			!ItemIsUndroppableByDefault(object.usItem) &&
			(object[0]->data.sObjectFlag & TAKEN_BY_MILITIA);
	};

	const bool actorIsInLoadedSector =
		self->deployment().sectorX() == gWorldSectorX &&
		self->deployment().sectorY() == gWorldSectorY &&
		self->deployment().sectorZ() == gbWorldSectorZ;
	if (actorIsInLoadedSector)
	{
		INT32 placementGrid = self->position().gridNo();
		if (placementGrid == NOWHERE)
			placementGrid = RandomGridNo();

		if (Water(placementGrid, self->position().level()))
			placementGrid = gMapInformation.sCenterGridNo;

		for (std::size_t slot = 0; slot < inventorySize; ++slot)
		{
			OBJECTTYPE& object = self->inventory()[slot];
			if (shouldDrop(object))
			{
				object[0]->data.sObjectFlag &= ~TAKEN_BY_MILITIA;

				// if we are not replacing ammo, unload gun prior to dropping it
				if (!gGameExternalOptions.fMilitiaUseSectorInventory_Ammo &&
					(Item[object.usItem].usItemClass & IC_GUN))
				{
					object[0]->data.gun.ubGunShotsLeft = 0;
				}

				AddItemToPool( placementGrid, &object, 1, self->position().level(), (WOLRD_ITEM_FIND_SWEETSPOT_FROM_GRIDNO | WORLD_ITEM_REACHABLE), -1 );
				DeleteObj(&object);
			}
		}
	}
	else
	{
		OBJECTTYPE pObject[NUM_INV_SLOTS];
		UINT32 counter = 0;

		for (std::size_t slot = 0; slot < inventorySize; ++slot)
		{
			OBJECTTYPE& object = self->inventory()[slot];
			if (shouldDrop(object))
			{
				object[0]->data.sObjectFlag &= ~TAKEN_BY_MILITIA;

				// if we are not replacing ammo, unload gun prior to dropping it
				if (!gGameExternalOptions.fMilitiaUseSectorInventory_Ammo &&
					(Item[object.usItem].usItemClass & IC_GUN))
				{
					object[0]->data.gun.ubGunShotsLeft = 0;
				}

				pObject[counter++] = object;

				DeleteObj(&object);
			}
		}

		AddItemsToUnLoadedSector( self->deployment().sectorX(), self->deployment().sectorY(), self->deployment().sectorZ(), RandomGridNo( ), counter, pObject, 0, WORLD_ITEM_REACHABLE, 0, 1, FALSE );
	}

	return true;
}

// sevenfm: take item from inventory to HANDPOS
bool TacticalActorEquipment::takeItemIntoHand(
	TacticalActor& actor,
	std::uint16_t item)
{
	if (!UsingNewInventorySystem() ||
		IsJa2TacticalTurnBasedCombat() ||
		item == NOTHING ||
		item >= MAXITEMS ||
		actor.inventory()[HANDPOS].exists())
	{
		return false;
	}

	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		if (actor.inventory()[slot].exists() &&
			actor.inventory()[slot].usItem == item)
		{
			actor.inventory()[slot].MoveThisObjectTo(
				actor.inventory()[HANDPOS],
				1,
				&actor);
			return true;
		}
	}

	return false;
}

// sevenfm: take item from inventory to HANDPOS
bool TacticalActorEquipment::takeBombIntoHand(
	TacticalActor& actor,
	std::uint16_t item)
{
	if (!UsingNewInventorySystem() ||
		item == NOTHING ||
		item >= MAXITEMS ||
		actor.inventory()[HANDPOS].exists())
	{
		return false;
	}

	// search for item with same id
	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		if (actor.inventory()[slot].exists() &&
			actor.inventory()[slot].usItem == item)
		{
			actor.inventory()[slot].MoveThisObjectTo(
				actor.inventory()[HANDPOS],
				1,
				&actor);
			return true;
		}
	}

	// search for any item with class IC_BOMB
	// take tripwire-activated item only if used item is tripwire activated
	const bool requestedTripwireActivation =
		ItemHasTripwireActivation(item);
	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		const UINT16 candidate = actor.inventory()[slot].usItem;
		if (actor.inventory()[slot].exists() &&
			candidate < MAXITEMS &&
			Item[candidate].usItemClass == IC_BOMB &&
			Item[candidate].ubCursor == BOMBCURS &&
			!ItemIsTripwire(candidate) &&
			static_cast<bool>(ItemHasTripwireActivation(candidate)) ==
				requestedTripwireActivation)
		{
			actor.inventory()[slot].MoveThisObjectTo(
				actor.inventory()[HANDPOS],
				1,
				&actor);
			return true;
		}
	}

	return false;
}

// Flugente: switch the hand item for a gunsling weapon, pistol, or knife.
bool TacticalActorEquipment::switchWeapon(
	TacticalActor& actor,
	bool knife,
	bool sidearm)
{
	auto* const self = &actor;
	const std::size_t inventorySize = self->inventory().size();

	auto finish = [&](bool swapped)
	{
		fCharacterInfoPanelDirty = TRUE;
		fInterfacePanelDirty = DIRTYLEVEL2;
		refreshFlashlights(actor);
		return swapped;
	};

	if (inventorySize <= SECONDHANDPOS)
		return finish(false);

	const std::size_t pocketSearch =
		UsingNewInventorySystem() ? GUNSLINGPOCKPOS : BIGPOCK1POS;
	if (pocketSearch >= inventorySize)
		return finish(false);

	auto hasItemClass =
		[&](std::size_t slot, std::uint32_t itemClass, bool singleOnly)
		{
			if (slot >= inventorySize)
				return false;

			const OBJECTTYPE& object = self->inventory()[slot];
			return object.exists() &&
				object.usItem < MAXITEMS &&
				(Item[object.usItem].usItemClass & itemClass) &&
				(!singleOnly || object.ubNumberOfObjects == 1);
		};

	auto isHandgun = [&](std::size_t slot)
	{
		if (!hasItemClass(slot, IC_GUN, false))
			return false;

		const auto weaponIndex =
			Item[self->inventory()[slot].usItem].ubClassIndex;
		return weaponIndex < MAXITEMS &&
			Weapon[weaponIndex].ubWeaponClass == HANDGUNCLASS;
	};

	auto findFirst = [&](auto predicate)
	{
		for (std::size_t slot = pocketSearch; slot < inventorySize; ++slot)
		{
			if (predicate(slot))
				return static_cast<int>(slot);
		}
		return static_cast<int>(NO_SLOT);
	};

	int retrieveSlot = NO_SLOT;
	if (UsingNewInventorySystem())
		retrieveSlot = GUNSLINGPOCKPOS;
	else
		retrieveSlot = findFirst(
			[&](std::size_t slot)
			{
				return hasItemClass(slot, IC_GUN, true);
			});

	if (knife)
	{
		const std::uint32_t desiredClass =
			hasItemClass(HANDPOS, IC_BLADE, false) ? IC_GUN : IC_BLADE;
		const int candidate = findFirst(
			[&](std::size_t slot)
			{
				return hasItemClass(slot, desiredClass, true);
			});
		if (candidate != NO_SLOT)
			retrieveSlot = candidate;
	}
	else if (sidearm)
	{
		const bool handAlreadyHasSidearm = isHandgun(HANDPOS);
		const int candidate = findFirst(
			[&](std::size_t slot)
			{
				return hasItemClass(slot, IC_GUN, true) &&
					isHandgun(slot) != handAlreadyHasSidearm;
			});
		if (candidate != NO_SLOT)
			retrieveSlot = candidate;
	}

	if (retrieveSlot == NO_SLOT ||
		static_cast<std::size_t>(retrieveSlot) >= inventorySize)
	{
		return finish(false);
	}

	if (!self->inventory()[HANDPOS].exists() &&
		!self->inventory()[retrieveSlot].exists())
	{
		return finish(false);
	}

	const OBJECTTYPE& handObject =
		self->inventory()[HANDPOS];
	const OBJECTTYPE& retrievedObject =
		self->inventory()[retrieveSlot];
	if ((handObject.exists() && handObject.usItem >= MAXITEMS) ||
		(retrievedObject.exists() &&
		 retrievedObject.usItem >= MAXITEMS))
	{
		return finish(false);
	}

	int handStorageSlot = HANDPOS;
	for (std::size_t slot = pocketSearch; slot < inventorySize; ++slot)
	{
		if (CanItemFitInPosition(
				self,
				&self->inventory()[HANDPOS],
				static_cast<INT8>(slot),
				FALSE) &&
			(static_cast<int>(slot) == retrieveSlot ||
			 !self->inventory()[slot].exists()))
		{
			handStorageSlot = static_cast<int>(slot);
			break;
		}
	}

	const bool handCanMove =
		!(handStorageSlot == HANDPOS &&
		  self->inventory()[HANDPOS].exists()) &&
		(CanItemFitInPosition(
			self,
			&self->inventory()[HANDPOS],
			static_cast<INT8>(handStorageSlot),
			FALSE) ||
		 (!self->inventory()[HANDPOS].exists() &&
		  !self->inventory()[SECONDHANDPOS].exists()));

	const bool retrievedObjectIsTwoHanded =
		retrievedObject.exists() &&
		ItemIsTwoHanded(retrievedObject.usItem);
	const bool retrievedObjectCanMove =
		!(retrievedObjectIsTwoHanded &&
		  self->inventory()[SECONDHANDPOS].exists()) &&
		(CanItemFitInPosition(
			self,
			&self->inventory()[retrieveSlot],
			HANDPOS,
			FALSE) ||
		 !retrievedObject.exists());

	if (!handCanMove || !retrievedObjectCanMove)
		return finish(false);

	std::int32_t actionPointCost = 0;
	if (UsingInventoryCostsAPSystem())
	{
		if (retrievedObject.exists())
		{
			actionPointCost += GetInvMovementCost(
				&self->inventory()[retrieveSlot],
				retrieveSlot,
				HANDPOS);
		}

		if (self->inventory()[HANDPOS].exists())
		{
			actionPointCost += GetInvMovementCost(
				&self->inventory()[HANDPOS],
				HANDPOS,
				handStorageSlot);
		}

		actionPointCost =
			(actionPointCost *
			 (100 + TacticalActorModifiers::backgroundValue(
				 actor,
				 BG_INVENTORY))) /
			100;
		actionPointCost = min(32767, max(0, actionPointCost));

		if (self->actionPoints().current() < actionPointCost)
		{
			CHAR16 output[512];
			swprintf(
				output,
				New113Message[MSG113_INVENTORY_APS_INSUFFICIENT],
				actionPointCost,
				self->actionPoints().current());
			ScreenMsg(
				FONT_MCOLOR_LTYELLOW,
				MSG_INTERFACE,
				output);
			return finish(false);
		}

		DeductPoints(
			self,
			static_cast<INT16>(actionPointCost),
			0);
	}

	const UINT16 oldHandItem =
		self->inventory()[HANDPOS].exists()
			? self->inventory()[HANDPOS].usItem
			: NOTHING;
	const UINT16 newHandItem =
		self->inventory()[retrieveSlot].exists()
			? self->inventory()[retrieveSlot].usItem
			: NOTHING;

	SwapObjs(
		&self->inventory()[HANDPOS],
		&self->inventory()[retrieveSlot]);

	if (handStorageSlot != retrieveSlot &&
		handStorageSlot != HANDPOS)
	{
		SwapObjs(
			&self->inventory()[retrieveSlot],
			&self->inventory()[handStorageSlot]);
	}

	HandleTacticalEffectsOfEquipmentChange(
		self,
		HANDPOS,
		oldHandItem,
		newHandItem);

	return finish(true);
}

void TacticalActorEquipment::refreshFlashlights(TacticalActor& actor)
{
	auto* const self = &actor;

	// no more need to redo this check
	self->featureFlags().primaryFlags() &= ~SOLDIER_REDOFLASHLIGHT;

	// we must be active and in a sector (not travelling) in a valid position
	if (!self->roster().active() ||
		!self->roster().inSector() ||
		TileIsOutOfBounds(self->position().gridNo()) ||
		self->position().direction() >= NUM_WORLD_DIRECTIONS ||
		self->animationPlayback().state() >= NUMANIMATIONSTATES)
	{
		return;
	}

	// no flashlight stuff if it isn't night, and we aren't underground
	if ( !NightTime( ) && !gbWorldSectorZ )
		return;

	// take note of wether we changed light
	BOOLEAN fLightChanged = FALSE;

	// remove existing lights we 'own'
	if (self->featureFlags().primaryFlags() & SOLDIER_LIGHT_OWNER)
	{
		RemovePersonalLights(self->identity().id());

		self->featureFlags().primaryFlags() &= ~SOLDIER_LIGHT_OWNER;

		fLightChanged = TRUE;
	}

	if (addBestFlashlight(actor))
	{
		// take note: we own a light source
		self->featureFlags().primaryFlags() |= SOLDIER_LIGHT_OWNER;

		fLightChanged = TRUE;
	}

	if ( fLightChanged )
	{
		// refresh sight for everybody
		AllTeamsLookForAll( TRUE );

		SetRenderFlags( RENDER_FLAG_FULL );
	}
}

std::uint8_t TacticalActorEquipment::bestEquippedFlashlightRange(
	TacticalActor& actor)
{
	UINT8 bestrange = 0;

	// do this check for both hands
	const std::size_t flashlightSlotEnd =
		std::min(
			actor.inventory().size(),
			static_cast<std::size_t>(VESTPOCKPOS));
	for (std::size_t slot = HANDPOS; slot < flashlightSlotEnd; ++slot)
	{
		OBJECTTYPE* pObj = &actor.inventory()[slot];

		if (!pObj->exists() || pObj->usItem >= MAXITEMS)
			// can't use this, end
			continue;

		// due to our attachment system, flashlights on guns do not require the batteries to be attached to the flashlight itself - anywhere will do
		if ( !FindAttachedBatteries( pObj ) )
			continue;

		if ( Item[pObj->usItem].usFlashLightRange )
		{
			bestrange = max( bestrange, Item[pObj->usItem].usFlashLightRange );
		}

		attachmentList::iterator iterend = (*pObj)[0]->attachments.end( );
		for ( attachmentList::iterator iter = (*pObj)[0]->attachments.begin( ); iter != iterend; ++iter )
		{
			if (iter->exists() &&
				iter->usItem < MAXITEMS &&
				Item[iter->usItem].usFlashLightRange)
				bestrange = max( bestrange, Item[iter->usItem].usFlashLightRange );
		}
	}

	return(bestrange);
}

bool TacticalActorEquipment::hasMortar(const TacticalActor& actor)
{
	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		const OBJECTTYPE& object = actor.inventory()[slot];
		if (object.exists() &&
			object.usItem < MAXITEMS &&
			ItemIsMortar(object.usItem))
		{
			return true;
		}
	}

	return false;
}

bool TacticalActorEquipment::hasSniperRifle(
	const TacticalActor& actor)
{
	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		const OBJECTTYPE& object = actor.inventory()[slot];
		if (object.exists() &&
			object.usItem < MAXITEMS &&
			Item[object.usItem].usItemClass == IC_GUN &&
			Item[object.usItem].ubClassIndex < MAXITEMS &&
			Weapon[Item[object.usItem].ubClassIndex].ubWeaponType ==
				GUN_SN_RIFLE)
		{
			return true;
		}
	}

	return false;
}

bool TacticalActorEquipment::hasItem(
	const TacticalActor& actor,
	std::uint16_t item)
{
	for (std::size_t slot = 0, inventorySize = actor.inventory().size();
		 slot < inventorySize;
		 ++slot)
	{
		if (actor.inventory()[slot].exists() &&
			actor.inventory()[slot].usItem == item)
			return true;
	}

	return false;
}

// Flugente: riot shields
OBJECTTYPE* TacticalActorEquipment::equippedRiotShield(
	TacticalActor& actor)
{
	OBJECTTYPE* object = nullptr;

	if (actor.inventory()[HANDPOS].exists() &&
		actor.inventory()[HANDPOS].usItem < MAXITEMS &&
		Item[actor.inventory()[HANDPOS].usItem].usRiotShieldStrength > 0)
		object = &actor.inventory()[HANDPOS];

	if (actor.inventory()[SECONDHANDPOS].exists() &&
		actor.inventory()[SECONDHANDPOS].usItem < MAXITEMS &&
		Item[actor.inventory()[SECONDHANDPOS].usItem].usRiotShieldStrength > 0)
		object = &actor.inventory()[SECONDHANDPOS];

	return object;
}

bool TacticalActorEquipment::hasEquippedRiotShield(
	TacticalActor& actor)
{
	// shield is not erect if prone
	if (actor.animationPlayback().state() >= NUMANIMATIONSTATES ||
		gAnimControl[actor.animationPlayback().state()].ubEndHeight == ANIM_PRONE)
		return false;

	// no shield while swimming
	if (TERRAIN_IS_HIGH_WATER(actor.position().terrainType()))
		return false;

	return equippedRiotShield(actor) != nullptr;
}

bool TacticalActorEquipment::damageRiotShield(
	TacticalActor& actor,
	std::int32_t damage)
{
	if (damage < 0)
		return false;

	auto* const self = &actor;
	OBJECTTYPE* shield = equippedRiotShield(actor);
	if (!shield)
		return false;

	if (!TileIsOutOfBounds(self->position().gridNo()))
	{
		PlayJA2Sample(
			static_cast<UINT32>(S_METAL_IMPACT1 + Random(3)),
			RATE_11025,
			SoundVolume(MIDVOLUME, self->position().gridNo()),
			1,
			SoundDir(self->position().gridNo()));
	}

	const std::int32_t currentStatus =
		(*shield)[0]->data.objectStatus;
	if (damage == 0 && currentStatus > 0)
		return true;

	if (currentStatus <= 0 || damage >= currentStatus)
	{
		destroyEquippedRiotShield(actor);
	}
	else
	{
		(*shield)[0]->data.objectStatus =
			static_cast<decltype((*shield)[0]->data.objectStatus)>(
				currentStatus - damage);
	}

	return true;
}

bool TacticalActorEquipment::removeOneItem(
	TacticalActor& actor,
	std::uint16_t item)
{
	if (item == NOTHING)
		return false;

	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		OBJECTTYPE& object = actor.inventory()[slot];
		if (!object.exists() || object.usItem != item)
			continue;

		object.RemoveObjectsFromStack(1);

		if (!object.exists())
			DeleteObj(&object);

		return true;
	}

	return false;
}

static bool addBestFlashlight(TacticalActor& actor)
{
	auto* const self = &actor;

    // not possible to get this bonus on a roof, due to our lighting system
    if ( self->position().level() != 0 )
    {
        return false;
    }

    UINT8 maxRange =
		TacticalActorEquipment::bestEquippedFlashlightRange(actor);
    if ( maxRange < 1 )
    {
        return false;
    }

    // we don't use the flashlight to run better at night (light up our shoes), we use it to find enemies!
    UINT8 minRange = 4;
    if ( minRange > maxRange )
    {
        minRange = maxRange;
    }

    float maxAngle = 45;
    maxAngle *= PI / 180 / 2; // convert to rad and halven

    auto forward = DirectionInc(self->position().direction());
    auto left = DirectionInc(DirectionIfTurnedClockwise(self->position().direction(), 6));
    auto leftLeft = DirectionInc(DirectionIfTurnedClockwise(self->position().direction(), 5));
    auto right = DirectionInc(DirectionIfTurnedClockwise(self->position().direction(), 2));
    auto rightRight = DirectionInc(DirectionIfTurnedClockwise(self->position().direction(), 3));

    bool isDiagonal = self->position().direction() == NORTHEAST || self->position().direction() == NORTHWEST || self->position().direction() == SOUTHEAST || self->position().direction() == SOUTHWEST;

	struct position_2d
	{
        INT16 x, y;

		position_2d(INT32 gridNo)
		{
			ConvertGridNoToXY(gridNo, &x, &y);
		}
		position_2d(INT16 _x, INT16 _y) : x{_x}, y{_y}
        {
        }
	};
	struct vector_2d
	{
        INT16 dx, dy;
        float length;

		vector_2d(INT8 direction)
		{
			ConvertDirectionToVectorInXY(direction, &dx, &dy);
            length = CalcLength(dx, dy);
		}
		vector_2d(position_2d from, position_2d to)
		{
			dx = to.x - from.x;
			dy = to.y - from.y;
            length = CalcLength(dx, dy);
		}
		vector_2d(INT16 _dx, INT16 _dy) : dx{_dx}, dy{_dy}
		{
			length = CalcLength(dx, dy);
		}

		float GetAngle( vector_2d other )
		{
			const float denominator = length * other.length;
			if (denominator <= 0.0f)
				return 0.0f;

			const float cosine = std::max(
				-1.0f,
				std::min(
					1.0f,
					static_cast<float>(dx * other.dx + dy * other.dy) /
						denominator));
			return acos(cosine);
		}

        static float CalcLength(float dx, float dy)
        {
            return sqrt(powf(dx, 2) + powf(dy, 2));
        }
	};

	position_2d soldierPos(self->position().gridNo());
    vector_2d soldierDir(self->position().direction());

    auto is_in_area = [&](INT32 sGridNoToTest) -> bool
    {
        vector_2d v(soldierPos, position_2d(sGridNoToTest));

		if (v.length > maxRange)
		{
			return false;
		}

        if (v.length < minRange)
        {
            return false;
        }

        auto coneAngle = soldierDir.GetAngle( v );
        if (coneAngle > maxAngle)
        {
            return false;
        }

        return true;
    };

    auto add_light_if_in_line_of_sight = [&, self]( INT32 sGridNoToTest, bool allowSkip ) -> void
    {
        if (allowSkip) // improve performance by skipping 3/4 of the lights
        {
            INT16 sXPos, sYPos;
            ConvertGridNoToXY( sGridNoToTest, &sXPos, &sYPos );
            if (!(sXPos % 2 == 0 && sYPos % 2 == 0))
            {
                return;
            }
        }

        if ( SoldierToVirtualSoldierLineOfSightTest( self, sGridNoToTest, self->position().level(), gAnimControl[self->animationPlayback().state()].ubEndHeight, false, NO_DISTANCE_LIMIT ) )
        {
            CreatePersonalLight( sGridNoToTest, self->identity().id() );
        }
    };

    auto travel_direction_to_add_light = [&]( INT32 startingGridNo, INT16 directionIncrementer )
    {
        for ( auto currentGridNo = startingGridNo; !OutOfBounds( currentGridNo, -1 ) && is_in_area( currentGridNo ); currentGridNo += directionIncrementer )
        {
            add_light_if_in_line_of_sight( currentGridNo, true);
        }
    };

    for ( auto currentGridNo = self->position().gridNo(); !OutOfBounds( currentGridNo, -1 ); currentGridNo += forward )
    {
		vector_2d v(soldierPos, position_2d(currentGridNo));
        if ( v.length < minRange )
        {
            continue;
        }
		else if (v.length > maxRange)
		{
			break;
		}

        add_light_if_in_line_of_sight( currentGridNo, false );

        travel_direction_to_add_light( currentGridNo, left );
        travel_direction_to_add_light( currentGridNo, right );

        if ( isDiagonal )
        {
            travel_direction_to_add_light( NewGridNo( currentGridNo, leftLeft ), left );
            travel_direction_to_add_light( NewGridNo( currentGridNo, rightRight ), right );
        }
    }

    return true;
}
