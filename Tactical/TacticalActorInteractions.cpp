#include "TacticalActorInteractions.h"

#include "Animation Control.h"
#include "Arms Dealer Init.h"
#include "Boxing.h"
#include "Campaign.h"
#include "Campaign Types.h"
#include "Civ Quotes.h"
#include "Dialogue Control.h"
#include "Disease.h"
#include "Drugs And Alcohol.h"
#include "EditorMercs.h"
#include "GameSettings.h"
#include "Handle Items.h"
#include "Handle UI.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Overhead.h"
#include "Points.h"
#include "ShopKeeper Interface.h"
#include "SkillCheck.h"
#include "Soldier Control.h"
#include "Soldier Functions.h"
#include "Soldier Profile.h"
#include "SoldierRepository.h"
#include "TacticalActorConditions.h"
#include "TacticalActorCovertOps.h"
#include "TacticalActorDisease.h"
#include "TacticalActorExplosives.h"
#include "TacticalActorModifiers.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "Town Militia.h"
#include "Strategic Town Loyalty.h"
#include "NPC.h"
#include "ai.h"
#include "connect.h"
#include "interface Dialogue.h"
#include "message.h"
#include "opplist.h"
#include "random.h"
#include "strategic.h"
#include "strategicmap.h"
#include "worldman.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

extern void ReduceAttachmentsOnGunForNonPlayerChars(
	TacticalActor* actor,
	OBJECTTYPE* object);

namespace
{
bool hasLiveInteractionContext(
	const TacticalActor& actor) noexcept
{
	return IsJa2TacticalWorldLoaded() &&
		actor.roster().active() &&
		actor.roster().inSector() &&
		actor.vitals().health() >= OKLIFE &&
		!TileIsOutOfBounds(actor.position().gridNo()) &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL &&
		actor.animationPlayback().state() <
			NUMANIMATIONSTATES;
}

bool hasLiveConversationContext(
	const TacticalActor& actor,
	const TacticalActor& target) noexcept
{
	const auto validActor = [](const TacticalActor& candidate) {
		const std::uint8_t profile =
			candidate.identity().profile();
		return candidate.roster().active() &&
			candidate.roster().inSector() &&
			candidate.identity().id().i < TOTAL_SOLDIERS &&
			candidate.roster().team() >= 0 &&
			candidate.roster().team() < MAXTEAMS &&
			candidate.identity().bodyType() < TOTALBODYTYPES &&
			(profile == NO_PROFILE || profile < NUM_PROFILES) &&
			!TileIsOutOfBounds(candidate.position().gridNo()) &&
			candidate.position().level() >= FIRST_LEVEL &&
			candidate.position().level() <= SECOND_LEVEL &&
			candidate.position().direction() <
				NUM_WORLD_DIRECTIONS &&
			candidate.animationPlayback().state() <
				NUMANIMATIONSTATES;
	};

	return IsJa2TacticalWorldLoaded() &&
		&actor != &target &&
		actor.identity().id() != target.identity().id() &&
		validActor(actor) &&
		validActor(target);
}

bool hasValidStrategicSector(
	const TacticalActor& actor) noexcept
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

bool traderContextIsValid(
	const TacticalActor& trader) noexcept
{
	if (!trader.interaction().isNonNpcTrader())
		return true;

	const std::int32_t dealer =
		trader.interaction().nonNpcTraderId();
	if (dealer < 0 ||
		static_cast<std::size_t>(dealer) >=
			armsDealerInfo.size() ||
		!hasValidStrategicSector(trader))
	{
		return false;
	}

	const std::int32_t strategicIndex =
		CALCULATE_STRATEGIC_INDEX(
			trader.deployment().sectorX(),
			trader.deployment().sectorY());
	const std::uint8_t town =
		StrategicMap[strategicIndex].bNameId;
	return town < MAX_TOWNS;
}

void tryRecruitVolunteer(
	TacticalActor& recruiter,
	TacticalActor& target)
{
	if (recruiter.roster().team() != OUR_TEAM ||
		target.roster().team() != CIV_TEAM ||
		target.identity().profile() != NO_PROFILE ||
		target.roster().civilianGroup() != NON_CIV_GROUP ||
		target.identity().bodyType() > DRESSCIV ||
		target.vitals().maximumHealth() <= 0 ||
		target.collapseState().tactical() ||
		target.collapseState().breathTriggered() ||
		target.vitals().health() !=
			target.vitals().maximumHealth() ||
		!target.aiBehavior().neutral() ||
		!hasValidStrategicSector(target))
	{
		return;
	}

	const std::uint8_t sector = SECTOR(
		target.deployment().sectorX(),
		target.deployment().sectorY());
	if (sector >= SectorInfo.size())
		return;

	SectorInfo[sector].usSectorInfoFlag |=
		SECTORINFO_VOLUNTEERS_RECENTLY_RECRUITED;

	if (!(target.featureFlags().secondaryFlags() &
		  SOLDIER_POTENTIAL_VOLUNTEER) ||
		!SectorOursAndPeaceful(
			target.deployment().sectorX(),
			target.deployment().sectorY(),
			target.deployment().sectorZ()))
	{
		return;
	}

	const std::int32_t strategicIndex =
		CALCULATE_STRATEGIC_INDEX(
			target.deployment().sectorX(),
			target.deployment().sectorY());
	const std::uint8_t town =
		StrategicMap[strategicIndex].bNameId;
	if (town >= MAX_TOWNS ||
		(town != BLANK_SECTOR &&
		 gTownLoyalty[town].ubRating <
			gGameExternalOptions.iMinLoyaltyToTrain) ||
		recruiter.identity().profile() >= NUM_PROFILES)
	{
		return;
	}

	float leadershipFactor =
		EffectiveLeadership(&recruiter) / 100.0f;
	if (DoesMercHavePersonality(
			&recruiter,
			CHAR_TRAIT_ASSERTIVE))
	{
		leadershipFactor *= 1.05f;
	}

	const float recruitModifier =
		(100 + TacticalActorModifiers::backgroundValue(
				recruiter,
				BG_PERC_APPROACH_RECRUIT)) /
		100.0f;
	const float rating =
		leadershipFactor *
		recruitModifier *
		gMercProfiles[recruiter.identity().profile()]
			.usApproachFactor[3];
	if (rating <= 70.0f)
		return;

	target.featureFlags().secondaryFlags() &=
		~SOLDIER_POTENTIAL_VOLUNTEER;
	target.roster().civilianGroup() =
		VOLUNTEER_CIV_GROUP;
	AddVolunteers(1);
	StatChange(&recruiter, LDRAMT, 8, TRUE);
	StatChange(&recruiter, EXPERAMT, 5, TRUE);
}

void abandonBoxingForConversation(
	std::uint8_t result)
{
	if (result != MSG_BOX_RETURN_YES)
		return;

	SetBoxingState(DISQUALIFIED);
	TriggerNPCRecord(DARREN, 21);
}

bool hasValidIntent(
	const TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction) noexcept
{
	return hasLiveInteractionContext(actor) &&
		!TileIsOutOfBounds(targetGrid) &&
		direction < NUM_WORLD_DIRECTIONS;
}

OBJECTTYPE* usableHandItem(TacticalActor& actor) noexcept
{
	if (HANDPOS >= actor.inventory().size())
		return nullptr;

	OBJECTTYPE& object = actor.inventory()[HANDPOS];
	if (!object.exists() ||
		object.usItem >= MAXITEMS ||
		object.objectStack.empty() ||
		!object[0])
	{
		return nullptr;
	}
	return &object;
}

TacticalActor* resolvePersonAt(
	const TacticalActor& actor,
	std::int32_t targetGrid) noexcept
{
	if (TileIsOutOfBounds(targetGrid))
		return nullptr;

	const SoldierID personId =
		WhoIsThere2(
			targetGrid,
			actor.position().level());
	TacticalActor* const person =
		GetJa2SoldierRepository().resolve(personId);
	if (!person ||
		!person->roster().active() ||
		!person->roster().inSector() ||
		person->position().gridNo() != targetGrid ||
		person->position().level() != actor.position().level())
	{
		return nullptr;
	}
	return person;
}

void releaseUi(TacticalActor& actor)
{
	UnSetUIBusy(actor.identity().id());
}

void faceDirection(
	TacticalActor& actor,
	std::uint8_t direction)
{
	if (actor.position().direction() == direction)
		return;

	actor.status().flags() |=
		SOLDIER_LOOK_NEXT_TURNSOLDIER;
	actor.EVENT_SetSoldierDesiredDirection(direction);
	actor.EVENT_SetSoldierDirection(direction);
}

void playNetworkAwareAnimation(
	TacticalActor& actor,
	std::uint16_t animation)
{
	if (!is_networked)
		actor.EVENT_InitNewSoldierAnim(animation, 0, FALSE);
	else
		actor.ChangeSoldierState(animation, 0, 0);
}

bool beginBombFallback(TacticalActor& actor)
{
	OBJECTTYPE* const object = usableHandItem(actor);
	if (!object ||
		Item[object->usItem].usItemClass != IC_BOMB)
	{
		return false;
	}

	return TacticalActorExplosives::beginBombPlacement(actor);
}

void dropCaptiveHandItem(
	TacticalActor& captive,
	std::size_t slot)
{
	if (slot >= captive.inventory().size())
		return;

	OBJECTTYPE& object = captive.inventory()[slot];
	if (!object.exists() ||
		(object.fFlags & OBJECT_UNDROPPABLE))
	{
		return;
	}

	if (object.usItem >= MAXITEMS ||
		object.objectStack.empty())
	{
		DeleteObj(&object);
		return;
	}

	const INT8 visible =
		captive.roster().team() == gbPlayerNum ? 1 : 0;
	const UINT16 itemFlags =
		captive.roster().team() == ENEMY_TEAM
			? 0
			: WORLD_ITEM_DROPPED_FROM_ENEMY;
	if (UsingNewAttachmentSystem())
	{
		ReduceAttachmentsOnGunForNonPlayerChars(
			&captive,
			&object);
	}
	AddItemToPool(
		captive.position().gridNo(),
		&object,
		visible,
		captive.position().level(),
		itemFlags,
		-1);
	DeleteObj(&object);
}
}

bool TacticalActorInteractions::beginItemTransfer(
	TacticalActor& actor)
{
	if (!hasLiveInteractionContext(actor))
		return false;

	switch (gAnimControl[actor.animationPlayback().state()].ubHeight)
	{
	case ANIM_STAND:
		actor.aiPlanning().action() = AI_ACTION_PENDING_ACTION;
		actor.EVENT_InitNewSoldierAnim(DROP_ITEM, 0, FALSE);
		return true;

	case ANIM_CROUCH:
	case ANIM_PRONE:
		SoldierHandleDropItem(&actor);
		actor.SoldierGotoStationaryStance();
		ActionDone(&actor);
		return true;

	default:
		return false;
	}
}

bool TacticalActorInteractions::startConversation(
	TacticalActor& actor,
	TacticalActor& target,
	bool validate)
{
	if (!hasLiveConversationContext(actor, target) ||
		!traderContextIsValid(target))
	{
		return false;
	}

	if (validate &&
		(!IsValidTalkableNPC(
			target.identity().id(),
			FALSE,
			FALSE,
			FALSE) ||
		 PythSpacesAway(
			 actor.position().gridNo(),
			 target.position().gridNo()) >
			 NPC_TALK_RADIUS * 2))
	{
		return false;
	}

	const std::int16_t actionPointCost =
		APBPConstants[AP_TALK];
	if (!IsJa2TacticalCombatActive() ||
		(gTacticalStatus.uiFlags & REALTIME))
	{
		std::int16_t targetX = 0;
		std::int16_t targetY = 0;
		ConvertGridNoToXY(
			target.position().gridNo(),
			&targetX,
			&targetY);
		const std::int16_t facingDirection =
			GetDirectionFromXY(
				targetX,
				targetY,
				&actor);
		if (facingDirection < 0 ||
			facingDirection >= NUM_WORLD_DIRECTIONS)
		{
			return false;
		}

		SendSoldierSetDesiredDirectionEvent(
			&actor,
			facingDirection);
		SendSoldierSetDesiredDirectionEvent(
			&target,
			gOppositeDirection[facingDirection]);
		actor.EVENT_StopMerc(
			actor.position().gridNo(),
			actor.position().direction());
	}

	if (GetCivType(&target) != CIV_TYPE_NA)
	{
		if (target.roster().team() == MILITIA_TEAM &&
			gGameExternalOptions
				.fAllowTacticalMilitiaCommand &&
			actor.roster().side() ==
				target.roster().side())
		{
			PopupMilitiaControlMenu(
				GetJa2TacticalEntityId(target));
			return false;
		}

		if ((gSkillTraitValues.fCOTurncoats ||
			 gGameExternalOptions.fEnemyCanSurrender ||
			 gGameExternalOptions.fPlayerCanAsktoSurrender) &&
			TacticalActorConditions::canBeCaptured(target))
		{
			if (gTacticalStatus.bBoxingState != NOT_BOXING)
			{
				DoMessageBox(
					MSG_BOX_BASIC_STYLE,
					Message[STR_ABANDON_FIGHT],
					GAME_SCREEN,
					static_cast<std::uint8_t>(
						MSG_BOX_FLAG_YESNO),
					abandonBoxingForConversation,
					nullptr);
				return false;
			}
			HandleSurrenderOffer(&target);
			return false;
		}

		if (target.interaction().isNonNpcTrader())
		{
			DeductPoints(
				&actor,
				actionPointCost,
				0,
				UNTRIGGERED_INTERRUPT);

			const std::int32_t strategicIndex =
				CALCULATE_STRATEGIC_INDEX(
					target.deployment().sectorX(),
					target.deployment().sectorY());
			const std::uint8_t town =
				StrategicMap[strategicIndex].bNameId;
			const std::size_t dealer =
				static_cast<std::size_t>(
					target.interaction()
						.nonNpcTraderId());

			if (!target.aiBehavior().neutral() &&
				!(actor.featureFlags().primaryFlags() &
				  SOLDIER_COVERT_SOLDIER))
			{
				ScreenMsg(
					FONT_MCOLOR_LTYELLOW,
					MSG_UI_FEEDBACK,
					szNonProfileMerchantText[0]);
			}
			else if (target.collapseState().tactical() ||
				 target.collapseState().breathTriggered() ||
				 target.vitals().health() < OKLIFE)
			{
				ScreenMsg(
					FONT_MCOLOR_LTYELLOW,
					MSG_UI_FEEDBACK,
					szNonProfileMerchantText[1]);
			}
			else if (IsJa2TacticalCombatActive())
			{
				ScreenMsg(
					FONT_MCOLOR_LTYELLOW,
					MSG_UI_FEEDBACK,
					szNonProfileMerchantText[2]);
			}
			else if (town != BLANK_SECTOR &&
				 gTownLoyalty[town].ubRating <
					armsDealerInfo[dealer]
						.nonprofile_loyaltyrequired &&
				 !(actor.featureFlags().primaryFlags() &
				   SOLDIER_COVERT_SOLDIER))
			{
				ScreenMsg(
					FONT_MCOLOR_LTYELLOW,
					MSG_UI_FEEDBACK,
					szNonProfileMerchantText[3]);
			}
			else
			{
				DeductPoints(
					&actor,
					actionPointCost,
					0,
					UNTRIGGERED_INTERRUPT);
				EnterShopKeeperInterfaceScreen_NonNPC(
					static_cast<std::int8_t>(dealer),
					target.identity().id());
			}
			return false;
		}

		DeductPoints(
			&actor,
			actionPointCost,
			0,
			UNTRIGGERED_INTERRUPT);
		tryRecruitVolunteer(actor, target);
		StartCivQuote(&target);
		return false;
	}

	DeductPoints(
		&actor,
		actionPointCost,
		0,
		UNTRIGGERED_INTERRUPT);
	if (target.identity().profile() != NO_PROFILE &&
		target.employment().mercenaryType() ==
			MERC_TYPE__EPC)
	{
		return InitiateConversation(
			&target,
			&actor,
			APPROACH_EPC_WHO_IS_RECRUITED,
			0) != FALSE;
	}

	if (target.aiBehavior().neutral())
	{
		if (TacticalActorConditions::isAssassin(target))
		{
			DeleteTalkingMenu();
			DebugAI(
				AI_MSG_INFO,
				&target,
				String(
					"CancelAIAction: assasin: start talking"));
			CancelAIAction(&target, TRUE);
			AddToShouldBecomeHostileOrSayQuoteList(
				target.identity().id());
		}
		else
		{
			return InitiateConversation(
				&target,
				&actor,
				NPC_INITIAL_QUOTE,
				0) != FALSE;
		}
	}
	else
	{
		return InitiateConversation(
			&target,
			&actor,
			APPROACH_ENEMY_NPC_QUOTE,
			0) != FALSE;
	}

	return true;
}

bool TacticalActorInteractions::stopChatting(
	TacticalActor& actor)
{
	if (!actor.interaction().chatting())
		return false;

	TacticalActor* const chatPartner =
		GetJa2SoldierRepository().resolve(
			actor.interaction().chatPartner());
	if (chatPartner)
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			TacticalStr[DISTRACT_STOP_STR],
			actor.GetName(),
			chatPartner->GetName());

		if (chatPartner == &actor ||
			chatPartner->interaction().chatPartner() ==
				actor.identity().id())
		{
			chatPartner->interaction().endChat();
		}
	}

	actor.interaction().endChat();
	return true;
}

bool TacticalActorInteractions::beginGivingItem(
	TacticalActor& actor)
{
	TacticalActor* target = nullptr;
	OBJECTTYPE* const pendingObject =
		actor.pendingItem().object();
	const std::int32_t targetGrid =
		actor.pendingAction().secondaryData();
	const std::int32_t rawDirection =
		actor.pendingAction().tertiaryData();
	const std::uint32_t rawTarget =
		actor.pendingAction().quaternaryData();

	if (!hasLiveInteractionContext(actor) ||
		TileIsOutOfBounds(targetGrid) ||
		rawDirection < 0 ||
		rawDirection >= NUM_WORLD_DIRECTIONS ||
		rawTarget >= TOTAL_SOLDIERS ||
		!pendingObject ||
		!pendingObject->exists() ||
		pendingObject->usItem >= MAXITEMS ||
		pendingObject->objectStack.empty() ||
		!VerifyGiveItem(&actor, &target) ||
		!target ||
		!target->roster().active() ||
		!target->roster().inSector() ||
		target->identity().id() != SoldierID{rawTarget} ||
		target->position().gridNo() != targetGrid ||
		target->position().level() != actor.position().level())
	{
		UnSetEngagedInConvFromPCAction(&actor);
		actor.pendingItem().clearObject();
		releaseUi(actor);
		return false;
	}

	const auto direction =
		static_cast<std::uint8_t>(rawDirection);
	actor.pathing().desiredDirection() = direction;
	actor.position().direction() = direction;
	actor.EVENT_InitNewSoldierAnim(GIVE_ITEM, 0, FALSE);
	return true;
}

bool TacticalActorInteractions::handcuffPerson(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	OBJECTTYPE* const handcuffs = usableHandItem(actor);
	TacticalActor* target =
		hasValidIntent(actor, targetGrid, direction)
			? resolvePersonAt(actor, targetGrid)
			: nullptr;
	if (!gGameExternalOptions.fAllowPrisonerSystem ||
		!handcuffs ||
		!HasItemFlag(handcuffs->usItem, HANDCUFFS) ||
		!target ||
		target == &actor ||
		!TacticalActorConditions::canBeCaptured(*target))
	{
		actor.DoMercBattleSound(BATTLE_SOUND_NOTHING);
		releaseUi(actor);
		return false;
	}

	bool success =
		target->assignment().isAsleep() ||
		target->collapseState().tactical();
	if (!success)
	{
		std::uint32_t attackerRating =
			10 * EffectiveExpLevel(&actor) +
			EffectiveStrength(&actor, FALSE) +
			2 * EffectiveDexterity(&actor, FALSE) +
			EffectiveAgility(&actor, FALSE);
		std::uint32_t defenderRating =
			10 * EffectiveExpLevel(target) +
			2 * EffectiveStrength(target, FALSE) +
			2 * EffectiveDexterity(target, FALSE) +
			2 * EffectiveAgility(target, FALSE);

		if (gGameOptions.fNewTraitSystem)
		{
			attackerRating +=
				25 * NUM_SKILL_TRAITS(
					&actor,
					MARTIAL_ARTS_NT) +
				10 * HAS_SKILL_TRAIT(
					&actor,
					MELEE_NT);
			defenderRating +=
				25 * NUM_SKILL_TRAITS(
					target,
					MARTIAL_ARTS_NT) +
				10 * HAS_SKILL_TRAIT(
					target,
					MELEE_NT);
		}
		else
		{
			attackerRating +=
				25 * NUM_SKILL_TRAITS(
					&actor,
					MARTIALARTS_OT) +
				25 * NUM_SKILL_TRAITS(
					&actor,
					HANDTOHAND_OT) +
				10 * HAS_SKILL_TRAIT(
					&actor,
					KNIFING_OT);
			defenderRating +=
				25 * NUM_SKILL_TRAITS(
					target,
					MARTIALARTS_OT) +
				25 * NUM_SKILL_TRAITS(
					target,
					HANDTOHAND_OT) +
				10 * HAS_SKILL_TRAIT(
					target,
					KNIFING_OT);
		}

		ReducePointsForFatigue(&actor, &attackerRating);
		ReducePointsForFatigue(target, &defenderRating);
		success =
			Random(attackerRating) >
			Random(defenderRating) + 100;
	}

	faceDirection(actor, direction);
	actor.EVENT_InitNewSoldierAnim(
		RELOAD_ROBOT,
		0,
		FALSE);

	if (success)
	{
		target->featureFlags().primaryFlags() |= SOLDIER_POW;
		RemoveManAsTarget(target);
		dropCaptiveHandItem(*target, HANDPOS);
		dropCaptiveHandItem(*target, SECONDHANDPOS);

		if (Item[handcuffs->usItem].usItemClass == IC_KIT)
		{
			UseKitPoints(handcuffs, 10, target);
		}
		else
		{
			AutoPlaceObject(target, handcuffs, FALSE);
			DeleteObj(handcuffs);
		}

		StatChange(&actor, STRAMT, 2, TRUE);
		StatChange(&actor, DEXTAMT, 3, TRUE);
		StatChange(&actor, EXPERAMT, 2, TRUE);
		DeductPoints(
			&actor,
			GetAPsToHandcuff(&actor, targetGrid),
			APBPConstants[BP_HANDCUFF],
			AFTERACTION_INTERRUPT);
		CheckForEndOfBattle(FALSE);
		return true;
	}

	StatChange(&actor, DEXTAMT, 2, TRUE);
	DeductPoints(
		&actor,
		GetAPsToHandcuff(&actor, targetGrid),
		APBPConstants[BP_HANDCUFF],
		AFTERACTION_INTERRUPT);
	actor.DoMercBattleSound(BATTLE_SOUND_CURSE1);
	if (actor.featureFlags().primaryFlags() &
		(SOLDIER_COVERT_CIV |
		 SOLDIER_COVERT_SOLDIER))
	{
		TacticalActorCovertOps::loseDisguise(actor);
		if (gSkillTraitValues.fCOStripIfUncovered)
			TacticalActorCovertOps::strip(actor);

		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			szCovertTextStr[STR_COVERT_ACTIVITIES],
			actor.GetName());
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			szCovertTextStr[STR_COVERT_UNCOVERED],
			target->GetName(),
			actor.GetName());
		target->aiBehavior().alertStatus() =
			std::max<INT8>(
				target->aiBehavior().alertStatus(),
				STATUS_RED);
		ProcessImplicationsOfPCAttack(
			&actor,
			&target,
			REASON_NORMAL_ATTACK);
	}
	return true;
}

bool TacticalActorInteractions::applyItemToPerson(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	if (!hasValidIntent(actor, targetGrid, direction))
	{
		releaseUi(actor);
		return false;
	}

	TacticalActor* target =
		resolvePersonAt(actor, targetGrid);
	OBJECTTYPE* const object = usableHandItem(actor);
	if (!target)
	{
		const bool startedBomb = beginBombFallback(actor);
		if (!startedBomb)
			releaseUi(actor);
		return startedBomb;
	}
	if (!object)
	{
		actor.DoMercBattleSound(BATTLE_SOUND_NOTHING);
		releaseUi(actor);
		return false;
	}

	const UINT16 item = object->usItem;
	if (!ItemCanBeAppliedToOthers(item))
	{
		DeductPoints(
			&actor,
			GetAPsToApplyItem(&actor, targetGrid),
			APBPConstants[BP_APPLYITEM],
			AFTERACTION_INTERRUPT);
		actor.DoMercBattleSound(BATTLE_SOUND_CURSE1);
		return true;
	}

	bool success = true;
	if (actor.roster().side() != target->roster().side() &&
		!target->collapseState().tactical())
	{
		const std::uint32_t attackerValue =
			30 +
			4 * EffectiveExpLevel(&actor) +
			EffectiveDexterity(&actor, FALSE) +
			20 * HAS_SKILL_TRAIT(
				&actor,
				STEALTHY_NT);
		const std::int32_t alertModifier =
			100 *
			(static_cast<std::int32_t>(
				 target->aiBehavior().alertStatus()) -
			 1);
		const std::uint32_t defenderValue =
			static_cast<std::uint32_t>(
				std::max<std::int32_t>(
					1,
					100 +
						3 * EffectiveExpLevel(target) +
						alertModifier));
		const std::uint32_t weight =
			std::max<std::uint32_t>(
				1,
				object->GetWeightOfObjectInStack(0));
		success =
			Random(
				std::max<std::uint32_t>(
					1,
					attackerValue / weight)) >
			Random(defenderValue);
		if (!success)
		{
			if (actor.featureFlags().primaryFlags() &
				(SOLDIER_COVERT_CIV |
				 SOLDIER_COVERT_SOLDIER))
			{
				TacticalActorCovertOps::loseDisguise(actor);
				if (gSkillTraitValues.fCOStripIfUncovered)
					TacticalActorCovertOps::strip(actor);

				ScreenMsg(
					FONT_MCOLOR_LTYELLOW,
					MSG_INTERFACE,
					szCovertTextStr[
						STR_COVERT_APPLYITEM_STEAL_FAIL],
					actor.GetName(),
					target->GetName());
				ScreenMsg(
					FONT_MCOLOR_LTYELLOW,
					MSG_INTERFACE,
					szCovertTextStr[
						STR_COVERT_UNCOVERED],
					target->GetName(),
					actor.GetName());
			}

			target->aiBehavior().alertStatus() =
				std::max<INT8>(
					target->aiBehavior().alertStatus(),
					STATUS_RED);
			ProcessImplicationsOfPCAttack(
				&actor,
				&target,
				REASON_NORMAL_ATTACK);
		}
	}

	if (success)
	{
		if (ItemIsGasmask(item))
		{
			const INT8 gasMaskSlot = FindGasMask(target);
			if (gasMaskSlot == NO_SLOT ||
				(gasMaskSlot != HEAD1POS &&
				 gasMaskSlot != HEAD2POS))
			{
				if (!target->inventory()[HEAD1POS].exists())
				success = PlaceObject(target, HEAD1POS, object);
				else if (!target->inventory()[HEAD2POS].exists())
				success = PlaceObject(target, HEAD2POS, object);
				else
				{
					AddItemToPool(
						target->position().gridNo(),
						&target->inventory()[HEAD2POS],
						1,
						target->position().level(),
						0,
						-1);
					success =
						PlaceObject(
							target,
							HEAD2POS,
							object);
				}
			}
			else
				success = false;
		}
		else if (Item[item].usItemClass == IC_BOMB)
			success = AutoPlaceObject(target, object, FALSE);
		else
			success =
				ApplyConsumable(
					target,
					object,
					TRUE,
					TRUE) == TRUE;
	}

	DeductPoints(
		&actor,
		GetAPsToApplyItem(&actor, targetGrid),
		APBPConstants[BP_APPLYITEM],
		AFTERACTION_INTERRUPT);
	if (!success)
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			New113Message[MSG113_COULD_NOT_APPLY],
			actor.GetName(),
			Item[item].szLongItemName,
			target->GetName());
	}
	else
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			New113Message[MSG113_X_APPLY_Y_TO_Z],
			actor.GetName(),
			Item[item].szLongItemName,
			target->GetName());
		actor.DoMercBattleSound(BATTLE_SOUND_COOL1);
	}
	return true;
}

bool TacticalActorInteractions::collectBloodFromPerson(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	if (!hasValidIntent(actor, targetGrid, direction))
	{
		releaseUi(actor);
		return false;
	}

	TacticalActor* const donor =
		resolvePersonAt(actor, targetGrid);
	OBJECTTYPE* const emptyBag = usableHandItem(actor);
	if (!donor)
	{
		const bool startedBomb = beginBombFallback(actor);
		if (!startedBomb)
			releaseUi(actor);
		return startedBomb;
	}
	if (donor == &actor ||
		!emptyBag ||
		!HasItemFlag(emptyBag->usItem, EMPTY_BLOOD_BAG) ||
		!TacticalActorConditions::canDonateBlood(*donor))
	{
		actor.DoMercBattleSound(BATTLE_SOUND_NOTHING);
		releaseUi(actor);
		return false;
	}

	static UINT16 bloodBagItem = 1757;
	if (bloodBagItem >= MAXITEMS ||
		!HasItemFlag(bloodBagItem, BLOOD_BAG))
	{
		UINT16 replacement = NOTHING;
		if (!GetFirstItemWithFlag(&replacement, BLOOD_BAG) ||
			replacement >= MAXITEMS)
		{
			ScreenMsg(
				FONT_MCOLOR_LTYELLOW,
				MSG_INTERFACE,
				L"Error: no blood bag item found in Items.xml!");
			releaseUi(actor);
			return false;
		}
		bloodBagItem = replacement;
	}

	OBJECTTYPE fullBag;
	if (!CreateItem(bloodBagItem, 100, &fullBag) ||
		!fullBag.exists() ||
		fullBag.objectStack.empty() ||
		!fullBag[0])
	{
		releaseUi(actor);
		return false;
	}

	DeleteObj(emptyBag);
	if (donor->condition().infected(0))
		fullBag[0]->data.sObjectFlag |= INFECTED;

	if (!AutoPlaceObject(&actor, &fullBag, FALSE))
	{
		AddItemToPool(
			donor->position().gridNo(),
			&fullBag,
			VISIBLE,
			actor.position().level(),
			0,
			-1);
	}

	const INT32 healableInjury =
		donor->vitals().healableInjury();
	const INT8 bleeding = donor->vitals().bleeding();
	donor->SoldierTakeDamage(
		0,
		TacticalActorConditions::bloodDonationAmount,
		0,
		TAKE_DAMAGE_BLOODLOSS,
		NOBODY,
		targetGrid,
		0,
		TRUE);
	donor->vitals().healableInjury() = healableInjury;
	donor->vitals().bleeding() = bleeding;

	DeductPoints(
		&actor,
		GetAPsToFillBloodbag(&actor, targetGrid),
		APBPConstants[BP_FILLBLOODBAG],
		AFTERACTION_INTERRUPT);
	return true;
}

bool TacticalActorInteractions::applySplintToPerson(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	OBJECTTYPE* const splint = usableHandItem(actor);
	TacticalActor* const target =
		hasValidIntent(actor, targetGrid, direction)
			? resolvePersonAt(actor, targetGrid)
			: nullptr;
	if (!target ||
		target == &actor ||
		!splint ||
		!HasItemFlag(splint->usItem, MEDICAL_SPLINT) ||
		!TacticalActorDisease::canReceiveSplint(*target) ||
		!((gGameOptions.fNewTraitSystem &&
		   NUM_SKILL_TRAITS(&actor, DOCTOR_NT) > 0) ||
		  (!gGameOptions.fNewTraitSystem &&
		   EffectiveMedical(&actor) >= 50)))
	{
		actor.DoMercBattleSound(BATTLE_SOUND_NOTHING);
		releaseUi(actor);
		return false;
	}

	const UINT16 item = splint->usItem;
	DeleteObj(splint);

	bool addToArm = true;
	bool addToLeg = true;
	for (int disease = 0;
			disease < NUM_DISEASES;
			++disease)
	{
		if (!target->condition().infected(disease))
			continue;

		if (addToArm &&
			(Disease[disease].usDiseaseProperties &
			 DISEASE_PROPERTY_LIMITED_USE_ARMS) &&
			!target->condition().hasDiseaseFlag(
				disease,
				TacticalActorDisease::armSplintFlag))
		{
			target->condition().markDiseaseFlag(
				disease,
				TacticalActorDisease::armSplintFlag);
			addToLeg = false;
		}

		if (addToLeg &&
			(Disease[disease].usDiseaseProperties &
			 DISEASE_PROPERTY_LIMITED_USE_LEGS) &&
			!target->condition().hasDiseaseFlag(
				disease,
				TacticalActorDisease::legSplintFlag))
		{
			target->condition().markDiseaseFlag(
				disease,
				TacticalActorDisease::legSplintFlag);
			addToArm = false;
		}
	}

	DeductPoints(
		&actor,
		GetAPsToApplyItem(&actor, targetGrid),
		APBPConstants[BP_APPLYITEM],
		AFTERACTION_INTERRUPT);
	ScreenMsg(
		FONT_MCOLOR_LTYELLOW,
		MSG_INTERFACE,
		New113Message[MSG113_X_APPLY_Y_TO_Z],
		actor.GetName(),
		Item[item].szLongItemName,
		target->GetName());
	playNetworkAwareAnimation(actor, CUTTING_FENCE);
	return true;
}
