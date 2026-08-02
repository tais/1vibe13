#include "TacticalActorCovertOps.h"
#include "TacticalActorAppearance.h"
#include "TacticalActorAssignments.h"
#include "TacticalActorConditions.h"
#include "TacticalActorEquipment.h"

#include "Animation Control.h"
#include "Campaign.h"
#include "CampaignStats.h"
#include "Campaign Types.h"
#include "Dialogue Control.h"
#include "Drugs And Alcohol.h"
#include "GameSettings.h"
#include "Game Clock.h"
#include "IMP Skill Trait.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "LOS.h"
#include "Map Information.h"
#include "Morale.h"
#include "Overhead.h"
#include "Points.h"
#include "Rotting Corpses.h"
#include "SkillCheck.h"
#include "TacticalActor.h"
#include "Soldier Profile.h"
#include "Soldier macros.h"
#include "SoldierRepository.h"
#include "Text.h"
#include "Vehicles.h"
#include "Weapons.h"
#include "ai.h"
#include "message.h"
#include "opplist.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

extern SECTOR_EXT_DATA SectorExternalData[256][4];

extern INT16 uiNIVSlotType[NUM_INV_SLOTS];

namespace
{
bool hasValidStrategicSector(const TacticalActor& actor) noexcept
{
	return actor.deployment().sectorX() >=
			MINIMUM_VALID_X_COORDINATE &&
		actor.deployment().sectorX() <=
			MAXIMUM_VALID_X_COORDINATE &&
		actor.deployment().sectorY() >=
			MINIMUM_VALID_Y_COORDINATE &&
		actor.deployment().sectorY() <=
			MAXIMUM_VALID_Y_COORDINATE;
}
}

// do we look like a civilian?
bool TacticalActorCovertOps::looksLikeCivilian(TacticalActor& actor)
{
	auto* const self = &actor;

	// if we have any camo: not covert
	if ( GetWornCamo( self ) > 0 || GetWornUrbanCamo( self ) > 0 || GetWornDesertCamo( self ) > 0 || GetWornSnowCamo( self ) > 0 )
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_CAMOFOUND], self->GetName( ) );
		return FALSE;
	}

	if ( UsingNewInventorySystem( ) )
	{
		INT8 invsize = (INT8)self->inventory().size( );
		for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )									// ... for all items in our inventory ...
		{
			if ( self->inventory()[bLoop].exists( ) )
			{
				/*// if we have a back pack: not covert
				if ( bLoop == BPACKPOCKPOS )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_BACKPACKFOUND], self->GetName( ) );
					return FALSE;
				}*/

				// do not check the LBE itself (we already checked for camo above)
				if ( bLoop >= VESTPOCKPOS && bLoop <= CPACKPOCKPOS )
					continue;

				// seriously? a corpse? of course this is suspicious!
				if ( HasItemFlag( self->inventory()[bLoop].usItem, CORPSE ) )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_CARRYCORPSEFOUND], self->GetName( ) );
					return FALSE;
				}

				BOOLEAN checkfurther = FALSE;

				// guns/launchers in our hands will always be noticed, even if covert
				if ( (Item[self->inventory()[bLoop].usItem].usItemClass & (IC_GUN | IC_LAUNCHER)) && (bLoop == HANDPOS || bLoop == SECONDHANDPOS) )
					checkfurther = TRUE;
				// further checks it item is not covert. This means that a gun that has that tag will not be detected if its inside a pocket!
				else if ( !HasItemFlag( self->inventory()[bLoop].usItem, COVERT ) )
				{
					checkfurther = TRUE;

					// visible slots are always checked if not covert
					if ( bLoop == HANDPOS || bLoop == SECONDHANDPOS || bLoop == GUNSLINGPOCKPOS || bLoop == KNIFEPOCKPOS || bLoop == HELMETPOS || bLoop == VESTPOS || bLoop == LEGPOS || bLoop == HEAD1POS || bLoop == HEAD2POS )
						;
					else
					{
						// check for the pocket the item is in
						// item will be detected if someone looks - check for the LBE item that gave us this slot. If that one is covert, this item is also covert
						UINT8 checkslot = 0;
						switch ( uiNIVSlotType[bLoop] )
						{
						case 2:
							// this is worn LBE gear itself
							break;
						case 3:
							checkslot = VESTPOCKPOS;
							break;
						case 4:
							if ( bLoop == MEDPOCK3POS || bLoop == SMALLPOCK11POS || bLoop == SMALLPOCK12POS || bLoop == SMALLPOCK13POS || bLoop == SMALLPOCK14POS )
								checkslot = LTHIGHPOCKPOS;
							else
								checkslot = RTHIGHPOCKPOS;
							break;
						case 5:
							checkslot = CPACKPOCKPOS;
							break;
						case 6:
							checkslot = BPACKPOCKPOS;
							break;
						default:
							{
								//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ITEM_SUSPICIOUS], self->GetName(), Item[self->inventory()[bLoop].usItem].szItemName );
								//return FALSE;
							}
							break;
						}

						// found a slot to check for LBE
						if ( checkslot > 0 )
						{
							// if LBE is covert
							if ( self->inventory()[checkslot].exists() && HasItemFlag( self->inventory()[checkslot].usItem, COVERT ) )
								// pass for this item
								checkfurther = FALSE;
						}
					}
				}

				if ( checkfurther )
				{
					// if that item is a gun, explosives, military armour or facewear, we're screwed
					if ( (Item[self->inventory()[bLoop].usItem].usItemClass & (IC_WEAPON | IC_GRENADE | IC_BOMB)) ||
						 ((Item[self->inventory()[bLoop].usItem].usItemClass & (IC_ARMOUR)) && !ItemIsLeatherJacket(self->inventory()[bLoop].usItem) && Armour[Item[self->inventory()[bLoop].usItem].ubClassIndex].ubProtection > 10) ||
						 (Item[self->inventory()[bLoop].usItem].nightvisionrangebonus > 0 || Item[self->inventory()[bLoop].usItem].hearingrangebonus > 0)
						 )
					{
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_MILITARYGEARFOUND], self->GetName( ), Item[self->inventory()[bLoop].usItem].szItemName );
						return FALSE;
					}
				}
			}
		}
	}
	else	// old inventory system. No LBE here, nothing fancy
	{
		INT8 invsize = (INT8)self->inventory().size( );
		for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )									// ... for all items in our inventory ...
		{
			if ( self->inventory()[bLoop].exists( ) )
			{
				if ( !HasItemFlag( self->inventory()[bLoop].usItem, COVERT ) )
				{
					// if that item is a gun, explosives, military armour or facewear, we're screwed
					if ( (Item[self->inventory()[bLoop].usItem].usItemClass & (IC_WEAPON | IC_GRENADE | IC_BOMB)) ||
						 ((Item[self->inventory()[bLoop].usItem].usItemClass & (IC_ARMOUR)) && !ItemIsLeatherJacket(self->inventory()[bLoop].usItem) && Armour[Item[self->inventory()[bLoop].usItem].ubClassIndex].ubProtection > 10) ||
						 (Item[self->inventory()[bLoop].usItem].nightvisionrangebonus > 0 || Item[self->inventory()[bLoop].usItem].hearingrangebonus > 0)
						 )
					{
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_MILITARYGEARFOUND], self->GetName( ), Item[self->inventory()[bLoop].usItem].szItemName );
						return FALSE;
					}
				}
			}
		}
	}

	return TRUE;
}

// do we look like a soldier?
bool TacticalActorCovertOps::looksLikeSoldier(TacticalActor& actor)
{
	auto* const self = &actor;

	INT8 invsize = (INT8)self->inventory().size( );
	for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )									// ... for all items in our inventory ...
	{
		if ( self->inventory()[bLoop].exists( ) )
		{
			// seriously? a corpse? of course this is suspicious!
			if ( HasItemFlag( self->inventory()[bLoop].usItem, CORPSE ) )
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_CARRYCORPSEFOUND], self->GetName( ) );
				return FALSE;
			}
		}
	}

	return TRUE;
}

std::int8_t TacticalActorCovertOps::uniformType(TacticalActor& actor)
{
	auto* const self = &actor;

	// we determine wether we are currently wearing civilian or military clothes
	for ( UINT8 i = UNIFORM_ENEMY_ADMIN; i < NUM_UNIFORMS; ++i )
	{
		// both parts have to fit. We cant mix different uniforms and get soldier disguise
		if ( COMPARE_PALETTEREP_ID( self->renderState().vestPalette(), gUniformColors[i].vest ) && COMPARE_PALETTEREP_ID( self->renderState().pantsPalette(), gUniformColors[i].pants ) )
		{
			return i;
		}
	}

	return -1;
}

// is our equipment too good for a soldier?
bool TacticalActorCovertOps::equipmentTooGood(TacticalActor& actor, bool closeLook)
{
	auto* const self = &actor;

	// if militia is equipped from sector inventory(and thu by the player itself), then its item selection is no longer bound to any progress calculation
	// we thus cannot check for equipment - the only way to find out is to look at this guy sharply, and to eventually realise that this gear did not come from the player
	if ( gGameExternalOptions.fMilitiaUseSectorInventory && TacticalActorConditions::isAssassin(*self) )
		return FALSE;

	INT8 uniformtype = uniformType(actor);
	if (uniformtype < UNIFORM_ENEMY_ADMIN ||
		uniformtype >= NUM_UNIFORMS)
	{
		// Without a recognized uniform, equipment is already suspicious. Avoid
		// consulting campaign progress before that bounded result is known.
		if (IsJa2TacticalWorldLoaded() &&
			szCovertTextStr[STR_COVERT_UNIFORM_NOORDER] != nullptr)
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_UNIFORM_NOORDER], self->GetName( ) );
		}
		return TRUE;
	}

	// check the guns in our hands and rifle sling
	// alert if we have more than 2, any of them has too much attachments or they are way too cool
	UINT8 numberofguns = 0;
	UINT8 ubCurrentProgress = CurrentPlayerProgressPercentage( );
	UINT8 maxcoolnessallowed = 1 + ubCurrentProgress / 10;

	// adjust max coolness depending on uniform
	// enemy spies get a small bonus here
	switch ( uniformtype )
	{
	case UNIFORM_ENEMY_ADMIN:
		maxcoolnessallowed += 1;
		break;
	case UNIFORM_ENEMY_TROOP:
	case UNIFORM_MILITIA_ROOKIE:
		maxcoolnessallowed += 2;
		break;
	case UNIFORM_ENEMY_ELITE:
	case UNIFORM_MILITIA_REGULAR:
		maxcoolnessallowed += 3;
		break;
	case UNIFORM_MILITIA_ELITE:
		maxcoolnessallowed += 4;
		break;
	default:
		break;
	}

	if ( UsingNewInventorySystem( ) )
	{
		INT8 invsize = (INT8)self->inventory().size( );
		for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )									// ... for all items in our inventory ...
		{
			if ( self->inventory()[bLoop].exists( ) )
			{
				// if we have a back pack: not covert
				if ( bLoop == BPACKPOCKPOS )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_BACKPACKFOUND], self->GetName( ) );
					return TRUE;
				}

				// guns/launchers in our hands will always be noticed, even if covert, so we need to check them later
				if ( bLoop == HANDPOS || bLoop == SECONDHANDPOS )
					;
				// other covert items are simply ignored
				else if ( HasItemFlag( self->inventory()[bLoop].usItem, COVERT ) )
					continue;
				// further checks it item is not covert. This means that an item that has that tag will not be detected if it is inside a pocket!
				else if ( (bLoop == GUNSLINGPOCKPOS || bLoop == HELMETPOS || bLoop == VESTPOS || bLoop == LEGPOS || bLoop == HEAD1POS || bLoop == HEAD2POS || bLoop == KNIFEPOCKPOS) )
					;
				else
				{
					// if we're not that close, we won't even see this, so don't check
					if ( !closeLook )
						continue;

					// item will be detected if someone looks - check for the LBE item that gave us this slot. If that one is covert, this item is also covert
					UINT8 checkslot = 0;
					switch ( uiNIVSlotType[bLoop] )
					{
					case 2:
						// this is worn LBE gear itself
						break;
					case 3:
						checkslot = VESTPOCKPOS;
						break;
					case 4:
						if ( bLoop == MEDPOCK3POS || bLoop == SMALLPOCK11POS || bLoop == SMALLPOCK12POS || bLoop == SMALLPOCK13POS || bLoop == SMALLPOCK14POS )
							checkslot = LTHIGHPOCKPOS;
						else
							checkslot = RTHIGHPOCKPOS;
						break;
					case 5:
						checkslot = CPACKPOCKPOS;
						break;
					default:
					{
							   //ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ITEM_SUSPICIOUS], self->GetName(), Item[self->inventory()[bLoop].usItem].szItemName );
							   //return FALSE;
					}
						break;
					}

					// found a slot to check for LBE
					if ( checkslot > 0 )
					{
						// if LBE is covert
						if ( self->inventory()[checkslot].exists( ) && HasItemFlag( self->inventory()[checkslot].usItem, COVERT ) )
							// pass for this item
							continue;
					}
				}

				// if that item is a gun, explosives, military armour or facewear, investigate further
				if ( (Item[self->inventory()[bLoop].usItem].usItemClass & (IC_GUN | IC_LAUNCHER | IC_ARMOUR | IC_FACE)) )
				{
					if ( Item[self->inventory()[bLoop].usItem].usItemClass & (IC_GUN | IC_LAUNCHER) && !HasItemFlag( self->inventory()[bLoop].usItem, COVERT ) )
					{
						++numberofguns;

						if ( numberofguns > 2 )
						{
							ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOOMANYGUNS], self->GetName( ) );
							return TRUE;
						}
					}

					OBJECTTYPE * pObj = &(self->inventory()[bLoop]);								// ... get pointer for this item ...

					if ( pObj != NULL )
					{
						for ( INT16 i = 0; i < pObj->ubNumberOfObjects; ++i )				// ... there might be multiple items here (item stack), so for each one ...
						{
							// loop over every item and its attachments
							if ( Item[pObj->usItem].ubCoolness > maxcoolnessallowed )
							{
								ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ITEMSTOOGOOD], self->GetName( ), Item[pObj->usItem].szItemName, pCountryNames[COUNTRY_NOUN] );
								return TRUE;
							}

							UINT8 numberofattachments = 0;
							// for every objects, we also have to check wether there are weapon attachments (eg. underbarrel grenade launchers), and cool them down too
							attachmentList::iterator iterend = (*pObj)[i]->attachments.end( );
							for ( attachmentList::iterator iter = (*pObj)[i]->attachments.begin( ); iter != iterend; ++iter )
							{
								if ( iter->exists( ) )
								{
									// loop over every item and its attachments
									if ( Item[iter->usItem].ubCoolness > maxcoolnessallowed )
									{
										ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ITEMSTOOGOOD], self->GetName( ), Item[iter->usItem].szItemName, pCountryNames[COUNTRY_NOUN] );
										return TRUE;
									}

									++numberofattachments;

									// no ordinary soldier is allowed that many attachments -> not covert
									if ( closeLook && numberofattachments > gGameExternalOptions.iMaxEnemyAttachments )
									{
										ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOOMANYATTACHMENTS], self->GetName( ), Item[pObj->usItem].szItemName );
										return TRUE;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	else	// old inventory system. No LBE here, nothing fancy
	{
		INT8 invsize = (INT8)self->inventory().size( );
		for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )									// ... for all items in our inventory ...
		{
			if ( self->inventory()[bLoop].exists( ) )
			{
				// if that item is a gun, explosives, military armour or facewear, investigate further
				if ( !HasItemFlag( self->inventory()[bLoop].usItem, COVERT ) && (Item[self->inventory()[bLoop].usItem].usItemClass & (IC_GUN | IC_LAUNCHER | IC_ARMOUR | IC_FACE)) )
				{
					if ( (Item[self->inventory()[bLoop].usItem].usItemClass & (IC_GUN | IC_LAUNCHER)) )
					{
						++numberofguns;

						if ( numberofguns > 2 )
						{
							ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOOMANYGUNS], self->GetName( ) );
							return TRUE;
						}

						OBJECTTYPE * pObj = &(self->inventory()[bLoop]);								// ... get pointer for this item ...

						if ( pObj != NULL )
						{
							for ( INT16 i = 0; i < pObj->ubNumberOfObjects; ++i )				// ... there might be multiple items here (item stack), so for each one ...
							{
								// loop over every item and its attachments
								if ( Item[pObj->usItem].ubCoolness > maxcoolnessallowed )
								{
									ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ITEMSTOOGOOD], self->GetName( ), Item[pObj->usItem].szItemName, pCountryNames[COUNTRY_NOUN] );
									return TRUE;
								}

								UINT8 numberofattachments = 0;
								// for every objects, we also have to check wether there are weapon attachments (eg. underbarrel grenade launchers), and cool them down too
								attachmentList::iterator iterend = (*pObj)[i]->attachments.end( );
								for ( attachmentList::iterator iter = (*pObj)[i]->attachments.begin( ); iter != iterend; ++iter )
								{
									if ( iter->exists( ) )
									{
										// loop over every item and its attachments
										if ( Item[iter->usItem].ubCoolness > maxcoolnessallowed )
										{
											ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ITEMSTOOGOOD], self->GetName( ), Item[iter->usItem].szItemName, pCountryNames[COUNTRY_NOUN] );
											return TRUE;
										}

										++numberofattachments;
									}
								}

								// no ordinary soldier is allowed that many attachments > not covert
								if ( closeLook && numberofattachments > gGameExternalOptions.iMaxEnemyAttachments )
								{
									ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOOMANYATTACHMENTS], self->GetName( ), Item[pObj->usItem].szItemName );
									return TRUE;
								}
							}
						}
					}
				}
			}
		}
	}

	return FALSE;
}


// are we in covert mode? we need to have the correct flag set, and not wear anything suspicious, or behave in a suspicious way
bool TacticalActorCovertOps::seemsLegitimate(TacticalActor& actor, SoldierID observerId)
{
	auto* const self = &actor;

	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(observerId);

	if ( !pSoldier )
		return TRUE;

	// rftr: turncoats ignore suspicious people/behaviour
	if (gSkillTraitValues.fCOTurncoats && (pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT))
		return TRUE;

	// if we don't have the Flag: not covert
	// important: no messages up to this point. the function will get called a lot, up to this point there is nothing unusual
	if ( !(self->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER)) )
		return FALSE;

	// if we perform suspicious actions, we are easier to uncover for a short time (but not by ourselves if we test the disguise)
	if ( observerId != self->identity().id() && self->featureFlags().primaryFlags() & SOLDIER_COVERT_TEMPORARY_OVERT )
	{
		// if enough time has passed, or we have spend enough AP, lose the flag
		if ( self->skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_APS) == 0 || GetWorldTotalSeconds( ) >= self->skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_SECONDS) )
		{
			self->skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_SECONDS) = 0;
			self->skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_APS) = 0;
			self->featureFlags().primaryFlags() &= ~SOLDIER_COVERT_TEMPORARY_OVERT;
		}
		else
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ACTIVITIES], self->GetName( ) );
			return FALSE;
		}
	}

	// if we are trying to dress like a civilian, but aren't successful: not covert
	if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_CIV && !looksLikeCivilian(actor) )
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_NO_CIV], self->GetName( ) );
		return FALSE;
	}

	// if we are trying to dress like a soldier, but aren't successful: not covert
	if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER && !looksLikeSoldier(actor) )
	{
		return FALSE;
	}

	UINT8 covertlevel = NUM_SKILL_TRAITS( self, COVERT_NT );	// our level in covert operations
	INT32 distance = PythSpacesAway( self->position().gridNo(), pSoldier->position().gridNo() );

	// if we are closer than this, our cover will always break if we do not have the skill
	// if we have the skill, our cover will blow if we dress up as a soldier, but not if we are dressed like a civilian
	INT32 discoverrange = gSkillTraitValues.sCOCloseDetectionRange;

	if ( observerId != self->identity().id() && distance < discoverrange )
	{
		switch ( covertlevel )
		{
		case 2:
			// a covert ops expert can get as close as he wants, even dressed up as a soldier, without arousing suspicion
			// exceptions: we are discovered if we are close and bleeding, or if we are drunk while dressed as a soldier
			{
				// if we are openly bleeding: not covert
				if ( gSkillTraitValues.fCODetectIfBleeding && self->vitals().bleeding() > 0 )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_BLEEDING], self->GetName( ) );
					return FALSE;
				}

				if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER && GetDrunkLevel( self ) != SOBER )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_DRUNKEN_SOLDIER], self->GetName( ) );
					return FALSE;
				}
			}
			break;
		case 1:
			// at lvl covert ops, we can be discovered if we are too close to the enemy and bleed or dressed up as a soldier
			// however, if we are dressed up as a civilian, we can get as close as we like, we won't be discovered
			{
				// if we are openly bleeding: not covert
				if ( gSkillTraitValues.fCODetectIfBleeding && self->vitals().bleeding() > 0 )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_BLEEDING], self->GetName( ) );
					return FALSE;
				}

				if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOO_CLOSE], self->GetName( ) );
					return FALSE;
				}
			}
			break;
		case 0:
		default:
			// without the covert ops skill, we can only dress up as civilians. We will be discovered if we get too close to the enemy
			// exception: special NPCs and EPCs can still get close (the Kulbas, for example, ARE civilians, so they apply)
			if ( (self->featureFlags().primaryFlags() & SOLDIER_COVERT_NPC_SPECIAL) == 0 )
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOO_CLOSE], self->GetName( ) );
				return FALSE;
			}
			break;
		}

		// if we are disguised as a soldier, elites and officers can uncover us if they are close
		if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER && distance < gSkillTraitValues.usCOEliteUncoverRadius && EffectiveExpLevel( pSoldier ) >= EffectiveExpLevel( self ) + covertlevel )
		{
			// officers can uncover us even if we are disguised as an elite
			if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_ENEMY_OFFICER )
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOO_CLOSE_TO_OFFICER], self->GetName( ) );
				return FALSE;
			}

			// elites uncover us if we a disguised as an admin or regular
			if ( pSoldier->roster().soldierClass() == SOLDIER_CLASS_ELITE && uniformType(actor) < UNIFORM_ENEMY_ELITE )
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOO_CLOSE_TO_ELITE], self->GetName( ) );
				return FALSE;
			}
		}
	}

	if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_CIV )
	{
		// civilians are suspicious if they are found in certain sectors. Especially at night
		// sector specific value:
		// 0 - civilians are always ok
		// 1 - civilians are suspicious at night
		// 2 - civilians are always suspicious
		// if underground, we still use the surface value

		UINT8 ubSectorId = SECTOR( self->deployment().sectorX(), self->deployment().sectorY() );
		UINT8 sectordata = SectorExternalData[ubSectorId][0].usCurfewValue;

		if ( sectordata > 1 )
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_CURFEW_BROKEN], self->GetName( ) );
			return FALSE;
		}
		// is it night?
		else if ( sectordata == 1 && GetTimeOfDayAmbientLightLevel( ) < NORMAL_LIGHTLEVEL_DAY + 2 )
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_CURFEW_BROKEN_NIGHT], self->GetName( ) );
			return FALSE;
		}

		// do this check only if we are in the currently loaded sector
		if ( self->deployment().sectorX() == gWorldSectorX && self->deployment().sectorY() == gWorldSectorY && self->deployment().sectorZ() == gbWorldSectorZ )
		{
			// check whether we are around a fresh corpse - this will make us much more suspicious
			INT32				cnt;
			ROTTING_CORPSE *	pCorpse;
			for ( cnt = 0; cnt < giNumRottingCorpse; ++cnt )
			{
				pCorpse = &( gRottingCorpse[cnt] );

				if ( pCorpse && pCorpse->fActivated && pCorpse->def.ubAIWarningValue > 0 && PythSpacesAway( self->position().gridNo(), pCorpse->def.sGridNo ) <= 5 )
				{
					// check: is this corpse that of an ally of the observing soldier?
					BOOLEAN fCorpseOFAlly = FALSE;
					if ( pSoldier->roster().team() == ENEMY_TEAM )
					{
						// check wether corpse was one of soldier's allies
						for ( UINT8 i = UNIFORM_ENEMY_ADMIN; i <= UNIFORM_ENEMY_ELITE; ++i )
						{
							if ( COMPARE_PALETTEREP_ID( pCorpse->def.VestPal, gUniformColors[i].vest ) && COMPARE_PALETTEREP_ID( pCorpse->def.PantsPal, gUniformColors[i].pants ) )
							{
								fCorpseOFAlly = TRUE;
								break;
							}
						}
					}
					else if ( pSoldier->roster().team() == OUR_TEAM || pSoldier->roster().team() == MILITIA_TEAM )
					{
						// check wether corpse was one of soldier's allies
						for ( UINT8 i = UNIFORM_MILITIA_ROOKIE; i <= UNIFORM_MILITIA_ELITE; ++i )
						{
							if ( COMPARE_PALETTEREP_ID( pCorpse->def.VestPal, gUniformColors[i].vest ) && COMPARE_PALETTEREP_ID( pCorpse->def.PantsPal, gUniformColors[i].pants ) )
							{
								fCorpseOFAlly = TRUE;
								break;
							}
						}
					}

					// a corpse was found near our position. If the soldier observing us can see it, he will be alarmed
					if ( fCorpseOFAlly && SoldierTo3DLocationLineOfSightTest( pSoldier, pCorpse->def.sGridNo, pCorpse->def.bLevel, 3, TRUE, CALC_FROM_WANTED_DIR ) )
					{
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_NEAR_CORPSE], self->GetName() );
						return FALSE;
					}
				}
			}
		}
	}

	if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER )
	{
		// if our equipment is too good, that is suspicious... not covert!
		if ( equipmentTooGood(actor, distance < discoverrange) )
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_SUSPICIOUS_EQUIPMENT], self->GetName( ) );
			return FALSE;
		}

		// do this check only if we are in the currently loaded sector
		if ( self->deployment().sectorX() == gWorldSectorX && self->deployment().sectorY() == gWorldSectorY && self->deployment().sectorZ() == gbWorldSectorZ )
		{
			TacticalActor* target =
				GetJa2SoldierRepository().resolve(
					self->targeting().targetId() );

			// are we targeting a buddy of our observer?
			if ( target != nullptr && target->roster().team() == pSoldier->roster().team() )
			{
				// if we are aiming at a soldier, others will notice our intent... not covert!
				if ( WeaponReady( self ) )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TARGETTING_SOLDIER], self->GetName(), target->GetName() );
					return FALSE;
				}
			}

			// even as a soldier, we will be caught around fresh corpses
			// assassins will not be uncovered around corpses, as the AI cannot willingly evade them... one could 'ward' against assassins by surrounding yourself with fresh corpses
			if ( distance < gSkillTraitValues.sCOCloseDetectionRangeSoldierCorpse && !TacticalActorConditions::isAssassin(*self) )
			{
				// check whether we are around a fresh corpse - this will make us much more suspicious
				// I deem this necessary, to avoid cheap exploits by nefarious players :-)
				INT32				cnt;
				ROTTING_CORPSE *	pCorpse;
				for ( cnt = 0; cnt < giNumRottingCorpse; ++cnt )
				{
					pCorpse = &( gRottingCorpse[cnt] );

					if ( pCorpse && pCorpse->fActivated && pCorpse->def.ubAIWarningValue > 0 && PythSpacesAway( self->position().gridNo(), pCorpse->def.sGridNo ) <= 5 )
					{
						// check: is this corpse that of an ally of the observing soldier?
						BOOLEAN fCorpseOFAlly = FALSE;
						if ( pSoldier->roster().team() == ENEMY_TEAM )
						{
							// check wether corpse was one of soldier's allies
							for ( UINT8 i = UNIFORM_ENEMY_ADMIN; i <= UNIFORM_ENEMY_ELITE; ++i )
							{
								if ( COMPARE_PALETTEREP_ID( pCorpse->def.VestPal, gUniformColors[i].vest ) && COMPARE_PALETTEREP_ID( pCorpse->def.PantsPal, gUniformColors[i].pants ) )
								{
									fCorpseOFAlly = TRUE;
									break;
								}
							}
						}
						else if ( pSoldier->roster().team() == OUR_TEAM || pSoldier->roster().team() == MILITIA_TEAM )
						{
							// check wether corpse was one of soldier's allies
							for ( UINT8 i = UNIFORM_MILITIA_ROOKIE; i <= UNIFORM_MILITIA_ELITE; ++i )
							{
								if ( COMPARE_PALETTEREP_ID( pCorpse->def.VestPal, gUniformColors[i].vest ) && COMPARE_PALETTEREP_ID( pCorpse->def.PantsPal, gUniformColors[i].pants ) )
								{
									fCorpseOFAlly = TRUE;
									break;
								}
							}
						}

						// a corpse was found near our position. If the soldier observing us can see it, he will be alarmed
						if ( fCorpseOFAlly && SoldierTo3DLocationLineOfSightTest( pSoldier, pCorpse->def.sGridNo, pCorpse->def.bLevel, 3, TRUE, CALC_FROM_WANTED_DIR ) )
						{
							ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_NEAR_CORPSE], self->GetName() );
							return FALSE;
						}
					}
				}
			}
		}
	}

	// uncover if merc is using flashlight and alert is raised
	if ( pSoldier->roster().team() == ENEMY_TEAM &&
		 pSoldier->aiBehavior().alertStatus() >= STATUS_RED &&
		 (NightTime( ) || self->deployment().sectorZ() > 0) &&
		 TacticalActorEquipment::bestEquippedFlashlightRange(*self) > 0 )
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"%s has a flashlight!", self->GetName( ) );
		return FALSE;
	}

	return TRUE;
}

// do we recognize someone else as a combatant?
bool TacticalActorCovertOps::recognizesCombatant(TacticalActor& actor, SoldierID targetId)
{
	auto* const self = &actor;

	// this will only work with the new trait system
	if ( !gGameOptions.fNewTraitSystem )
		return TRUE;

	if ( targetId == NOBODY )
		return TRUE;

	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(targetId);

	if ( !pSoldier )
		return TRUE;

	// zombies don't care about disguises
	if ( TacticalActorConditions::isZombie(*self) )
		return TRUE;

	// not in covert mode: we recognize him
	if ( (pSoldier->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER)) == 0 )
		return TRUE;

	// neutral characters just dont care
	if ( self->aiBehavior().neutral() )
		return TRUE;

	// check for for vehicles and creatures... weird things happen
	if ( IsVehicle( pSoldier ) || pSoldier->roster().team() == CREATURE_TEAM || self->roster().team() == CREATURE_TEAM )
		return TRUE;

	// if from same team, do not uncover
	if ( self->roster().team() == pSoldier->roster().team() || self->roster().side() == pSoldier->roster().side() )
		return TRUE;

	// hack: if this is attacking us at this very moment by punching, do not recognize him...
	// this resolves the problem that we attack someone from behind and kill him instantly, but the game mechanic forces him to turn before
	// only allow this if we are not yet alerted (we are surprised, so we don't recognize him in the moment of the attack)
	// also: only allow if he's next to us
	if ( self->aiBehavior().alertStatus() < STATUS_RED && pSoldier->targeting().targetId() == self->identity().id() )
	{
		INT32 nextGridNoinSight = NewGridNo( pSoldier->position().gridNo(), DirectionInc( pSoldier->position().direction() ) );
		if ( nextGridNoinSight == self->position().gridNo() && self->position().level() == pSoldier->position().level() )
		{
			if ( pSoldier->animationPlayback().state() == PUNCH )
				return FALSE;
			else if ( pSoldier->animationPlayback().state() == PUNCH_BREATH )
				return TRUE;
		}
	}

	// campaign stats
	if ( pSoldier->roster().team() == ENEMY_TEAM )
		gCurrentIncident.usIncidentFlags |= INCIDENT_SPYACTION_ENEMY;
	else
		gCurrentIncident.usIncidentFlags |= INCIDENT_SPYACTION_PLAYERSIDE;

	// do we recognize this guy as an enemy?
	if ( !seemsLegitimate(*pSoldier, self->identity().id()) )
	{
		// aha, he/she's a spy! Blow cover
		if ( pSoldier->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER) )
		{
			loseDisguise(*pSoldier);

			if ( gSkillTraitValues.fCOStripIfUncovered )
				strip(*pSoldier);

			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_UNCOVERED], self->GetName(), pSoldier->GetName()  );

			// we have uncovered a spy! Get alerted, if we aren't already
			if ( self->aiBehavior().alertStatus() < STATUS_BLACK )
				self->aiBehavior().alertStatus() = STATUS_BLACK;

			// reset our sight of this guy
			self->awareness().opponentKnowledge()[pSoldier->identity().id()] = NOT_HEARD_OR_SEEN;

			ManSeesMan( self, pSoldier, pSoldier->position().gridNo(), pSoldier->position().level(), 0, 0 );

			// campaign stats
			gCurrentIncident.usIncidentFlags |= INCIDENT_SPYACTION_UNCOVERED;
		}

		return TRUE;
	}

	return FALSE;
}

// loose covert property
void TacticalActorCovertOps::loseDisguise(TacticalActor& actor)
{
	auto* const self = &actor;

	// loose any covert flags
	self->featureFlags().primaryFlags() &= ~(SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER | SOLDIER_COVERT_NPC_SPECIAL);

	// rehandle sight for everybody
	TacticalActor*		pSoldier;
	SoldierID  iLoop = gTacticalStatus.Team[OUR_TEAM].bFirstID;
	for ( ; iLoop <= gTacticalStatus.Team[CIV_TEAM].bLastID; ++iLoop )
	{
		pSoldier = GetJa2SoldierRepository().resolve( iLoop );
		if ( pSoldier == nullptr )
		{
			continue;
		}

		if ( pSoldier->roster().active() && pSoldier->roster().inSector() && pSoldier->vitals().health() > 0 )
		{
			RecalculateOppCntsDueToNoLongerNeutral( pSoldier );
		}
	}
}

void TacticalActorCovertOps::disguise(TacticalActor& actor)
{
	auto* const self = &actor;

	// this will only work with the new trait system
	if (!gGameOptions.fNewTraitSystem)
		return;

	// check if we already disguised
	if( self->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER | SOLDIER_COVERT_NPC_SPECIAL) )
		return;

	// check that soldier is active and in sector
	if ( !self->roster().active() || !self->roster().inSector() )
		return;

	// if this flag is set, do not apply the disguise properties
	if ( self->featureFlags().secondaryFlags() & SOLDIER_COVERT_NOREDISGUISE )
		return;

	applyCovert(actor, FALSE);
}

void TacticalActorCovertOps::applyCovert(TacticalActor& actor, bool withMessage)
{
	auto* const self = &actor;

	// check that we have correct clothes
	if ( self->featureFlags().primaryFlags() & SOLDIER_NEW_VEST && self->featureFlags().primaryFlags() & SOLDIER_NEW_PANTS )
	{
		// first, remove the covert flags, and then reapply the correct ones, in case we switch between civilian and military clothes
		self->featureFlags().primaryFlags() &= ~(SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER);

		// if we apply the disguise property, remove the marker that we don't want this to happen
		// the idea is that if we explicitly remove a disguise, but not our new colours, we don't want to regain the disguise
		// we can then lose this marker again if we explicitly put on a disguise
		self->featureFlags().secondaryFlags() &= ~SOLDIER_COVERT_NOREDISGUISE;

		// we can only disguise successfully if we are not seen
		if ( !EnemySeenSoldierRecently( self ) )
		{
			// we now have to determine wether we are currently wearing civilian or military clothes
			for ( UINT8 i = UNIFORM_ENEMY_ADMIN; i <= UNIFORM_ENEMY_ELITE; ++i )
			{
				// both parts have to fit. We cant mix different uniforms and get soldier disguise
				if ( COMPARE_PALETTEREP_ID( self->renderState().vestPalette(), gUniformColors[i].vest ) && COMPARE_PALETTEREP_ID( self->renderState().pantsPalette(), gUniformColors[i].pants ) )
				{
					self->featureFlags().primaryFlags() |= SOLDIER_COVERT_SOLDIER;

					if ( withMessage && self->roster().team() == OUR_TEAM )
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_DISGUISED_AS_SOLDIER], self->GetName( ) );

					break;
				}
			}

			// if not dressed as a soldier, we must be dressed as a civilian
			if ( !(self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER) )
			{
				self->featureFlags().primaryFlags() |= SOLDIER_COVERT_CIV;

				if ( withMessage && self->roster().team() == OUR_TEAM )
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_DISGUISED_AS_CIVILIAN], self->GetName( ) );
			}
		}

		// reevaluate sight - otherwise we could hide by changing clothes in plain sight!
		OtherTeamsLookForMan( self );
	}
}

// undisguise or take off any clothes item and switch back to original clothes
// no - this function does not do what you think it does. Leave Fox alone, you perv.
void TacticalActorCovertOps::strip(TacticalActor& actor)
{
	auto* const self = &actor;

	// if covert, loose that ability
	if ( self->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER) )
	{
		loseDisguise(actor);

		// if we explicitly lose the disguise property, add a flag so that we aren't redisguised again immediately
		self->featureFlags().secondaryFlags() |= SOLDIER_COVERT_NOREDISGUISE;
	}
	// if already not covert, take off clothes
	else if ( self->featureFlags().primaryFlags() & (SOLDIER_NEW_VEST|SOLDIER_NEW_PANTS) )
	{
		// if we have undamaged clothes, spawn them, the graphic will be removed anyway
		if ( (self->featureFlags().primaryFlags() & SOLDIER_NEW_VEST) && !(self->featureFlags().primaryFlags() & SOLDIER_DAMAGED_VEST) )
		{
			UINT16 vestitem = 0;
			if ( GetFirstClothesItemWithSpecificData( &vestitem, self->renderState().vestPalette(), "blank" ) )
			{
				CreateItem( vestitem, 100, &gTempObject );
				if ( !AutoPlaceObject( self, &gTempObject, FALSE ) )
					AddItemToPool( self->position().gridNo(), &gTempObject, 1, self->position().level(), 0, -1 );
			}
			else
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_NO_CLOTHES_ITEM] );
		}

		if ( (self->featureFlags().primaryFlags() & SOLDIER_NEW_PANTS) && !(self->featureFlags().primaryFlags() & SOLDIER_DAMAGED_PANTS) )
		{
			UINT16 pantsitem = 0;
			if ( GetFirstClothesItemWithSpecificData( &pantsitem, "blank", self->renderState().pantsPalette() ) )
			{
				CreateItem( pantsitem, 100, &gTempObject );
				if ( !AutoPlaceObject( self, &gTempObject, FALSE ) )
					AddItemToPool(self->position().gridNo(), &gTempObject, 1, self->position().level(), 0, -1);
			}
			else
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_NO_CLOTHES_ITEM] );
		}

		// loose any clothes flags
		self->featureFlags().primaryFlags() &= ~(SOLDIER_NEW_VEST | SOLDIER_NEW_PANTS);

		// show our true colours
		UINT16 usPaletteAnimSurface = LoadSoldierAnimationSurface( self, self->animationPlayback().state() );

		if ( usPaletteAnimSurface != INVALID_ANIMATION_SURFACE )
		{
			if ( self->roster().team() == OUR_TEAM )
			{
				UINT8				ubProfileIndex;
				MERCPROFILESTRUCT * pProfile;

				ubProfileIndex = self->identity().profile();
				pProfile = &(gMercProfiles[ubProfileIndex]);

				SET_PALETTEREP_ID( self->renderState().vestPalette(), pProfile->VEST );
				SET_PALETTEREP_ID( self->renderState().pantsPalette(), pProfile->PANTS );
			}
			else if ( self->featureFlags().primaryFlags() & SOLDIER_ASSASSIN )
			{
				SET_PALETTEREP_ID( self->renderState().vestPalette(), gUniformColors[UNIFORM_ENEMY_ELITE].vest );
				SET_PALETTEREP_ID( self->renderState().pantsPalette(), gUniformColors[UNIFORM_ENEMY_ELITE].pants );
			}

			// Use palette from HVOBJECT, then use substitution for pants, etc
			memcpy( self->palette().base8(), gAnimSurfaceDatabase[usPaletteAnimSurface].hVideoObject->pPaletteEntry, sizeof(SGPPaletteEntry) * 256 );

			SetPaletteReplacement( self->palette().base8(), self->renderState().headPalette() );
			SetPaletteReplacement( self->palette().base8(), self->renderState().vestPalette() );
			SetPaletteReplacement( self->palette().base8(), self->renderState().pantsPalette() );
			SetPaletteReplacement( self->palette().base8(), self->renderState().skinPalette() );

			(void)TacticalActorAppearance::rebuildPalettes(*self);
		}
	}
	else
	{
		// if the player is an annoying little perv, tell them so, girls!
		// Flugente: additional dialogue
		AdditionalTacticalCharacterDialogue_CallsLua(self, ADE_SEXUALHARASSMENT );
		self->morale().morale() = max( 0, self->morale().morale() - 1 );
	}
}

// check wether our disguise is any good
void TacticalActorCovertOps::runSelfTest(TacticalActor& actor)
{
	auto* const self = &actor;

	if ( seemsLegitimate(actor, self->identity().id()) )
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TEST_OK], self->GetName( ) );
	else
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TEST_FAIL], self->GetName( ) );
}

// Flugente: spy assignments
extern UINT32 gCoolnessBySector[256];

std::uint8_t TacticalActorCovertOps::uncoverRisk(TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE || ( self->featureFlags().primaryFlags() & SOLDIER_POW ) )
		return 0;

	if ( !SPY_LOCATION(self->assignment().current()) )
		return 100;
	if (!hasValidStrategicSector(actor))
		return 0;

	// base value:
	// 15% level
	// 15% stealth
	// 70% covert trait
	UINT32 val = 15 * EffectiveExpLevel ( self, FALSE )
		+ 1.5f * GetWornStealth( self )
		+ 350 * NUM_SKILL_TRAITS( self, COVERT_NT );

	ReducePointsForFatigue( self, &val );

	// personality/disability modifiers
	FLOAT modifier = 1.0f;
	if ( DoesMercHaveDisability( self, NERVOUS ) )					modifier -= 0.05f;

	if ( DoesMercHavePersonality( self, CHAR_TRAIT_SOCIABLE ) )		modifier += 0.05f;
	if ( DoesMercHavePersonality( self, CHAR_TRAIT_COWARD ) )		modifier -= 0.05f;

	// personal value in [0; 100]
	int personalvalue = (FLOAT)(val * modifier) / 10.0f;
	personalvalue = min( 100, max( 0, personalvalue ) );

	// if we do this disguised as a soldier, risk will be much higher, as we are under much more scrutiny. This makes up for the increased gain in soldier disguise
	// less risk if we are asleep, just hiding or forced to hide
	UINT8 typemultiplier = ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER ) ? 5 : 2;
	if ( ( self->assignment().current() == CONCEALED ) || self->assignment().isAsleep() || self->skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) > 10 )
		typemultiplier = 1;

	// we now take the sector coolness as a measurement of how important the sector is, and thus how intel we gain
	// correct outliers - value in[0; 100]
	UINT32 sectorvalue = typemultiplier * min( 20, gCoolnessBySector[SECTOR( self->deployment().sectorX(), self->deployment().sectorY() )] );

	UINT8 totalvalue = sectorvalue * ( 110 - personalvalue ) / 100;
	totalvalue = min(100, max(0, totalvalue ) );

	// A most awesome merc in Meduna palace, disguised as a soldier, would have a value of 1.05 * 4. 63 * 4 = 10.649 at this point.
	// This would be the place where we modify our intel gain rate.

	return totalvalue;
}

float TacticalActorCovertOps::intelGain(TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE || ( self->featureFlags().primaryFlags() & SOLDIER_POW ) )
		return 0.0f;

	// if not on correct assignments, no gain
	if ( self->assignment().current() != GATHERINTEL )
		return 0.0f;
	if (!hasValidStrategicSector(actor))
		return 0.0f;

	// if we're asleep, or on a penalty, we accomplish nothing
	if ( self->assignment().isAsleep() || self->skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) > 10 )
		return 0.0f;

	// the covert trait isn't that important in determining the intel gain. It is much more important in mitigating the risk of exposure, however
	// base value:
	// 50% wisdom
	// 10% level
	// 5% scout trait
	// 15% covert trait
	// 20% snitch trait
	UINT32 val = 5 * EffectiveWisdom( self )
		+ 10 * EffectiveExpLevel ( self, FALSE )
		+ 50 * NUM_SKILL_TRAITS( self, SCOUTING_NT )
		+ 75 * NUM_SKILL_TRAITS( self, COVERT_NT )
		+ 200 * NUM_SKILL_TRAITS( self, SNITCH_NT );

	ReducePointsForFatigue( self, &val );

	// personality/disability modifiers
	FLOAT modifier = 1.0f;
	if ( DoesMercHaveDisability( self, FORGETFUL ) )	modifier -= 0.15f;
	if ( DoesMercHaveDisability( self, PSYCHO ) )		modifier -= 0.05f;

	if ( DoesMercHavePersonality( self, CHAR_TRAIT_SOCIABLE ) )		modifier += 0.10f;
	if ( DoesMercHavePersonality( self, CHAR_TRAIT_LONER ) )		modifier -= 0.10f;
	if ( DoesMercHavePersonality( self, CHAR_TRAIT_ASSERTIVE ) )	modifier += 0.05f;
	if ( DoesMercHavePersonality( self, CHAR_TRAIT_PRIMITIVE ) )	modifier -= 0.10f;

	FLOAT personalvalue = (FLOAT)(val * modifier) / 1000.0f;

	// we now take the sector coolness as a measurement of how important the sector is, and thus how intel we gain
	// correct outliers
	UINT32 ubLocationModifier = 1 + max(2, min(20, gCoolnessBySector[SECTOR( self->deployment().sectorX(), self->deployment().sectorY() )] ) );

	// in order not to make the differences to great, alter these values - will now be in [0.6; 4.63]
	FLOAT sectorvalue = log( (FLOAT)ubLocationModifier );
	sectorvalue *= sectorvalue / 2.0f;

	FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*self);

	FLOAT totalvalue = personalvalue * sectorvalue * administrationmodifier;

	// if we do this disguised as a soldier, we get more info
	if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER )
		totalvalue *= 2;

	// A most awesome merc in Meduna palace, disguised as a soldier, would have a value of 1.15 * 4.63 * 2 = 10.649 at this point.
	// This would be the place where we modify our intel gain rate.

	return totalvalue;
}
