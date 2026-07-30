#include "TacticalActorLongActions.h"

#include "Animation Control.h"
#include "Campaign.h"
#include "connect.h"
#include "Handle Items.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "LuaInitNPCs.h"
#include "Overhead.h"
#include "Points.h"
#include "strategicmap.h"
#include "Soldier Control.h"
#include "Structure Wrap.h"
#include "TacticalActorModifiers.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "message.h"

#include <cstdint>

namespace
{
bool requiresTool(std::uint8_t action) noexcept
{
	return action == MTA_FORTIFY ||
		action == MTA_REMOVE_FORTIFY;
}

bool validateLiveActor(const TacticalActor& actor) noexcept
{
	return actor.roster().active() &&
		actor.roster().inSector() &&
		actor.vitals().health() >= OKLIFE &&
		!actor.collapseState().tactical() &&
		IsJa2TacticalWorldLoaded() &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL &&
		!TileIsOutOfBounds(actor.position().gridNo());
}
}

std::uint8_t TacticalActorLongActions::current(
	const TacticalActor& actor) noexcept
{
	return actor.longAction().action();
}

bool TacticalActorLongActions::start(
	TacticalActor& actor,
	std::uint8_t action,
	std::int32_t contextGrid)
{
	if (!validateLiveActor(actor) ||
		TileIsOutOfBounds(contextGrid) ||
		action <= MTA_NONE ||
		action >= NUM_MTA)
	{
		return false;
	}

	if (requiresTool(action) &&
		actor.position().level() != 0)
	{
		return false;
	}

	if (actor.longAction().active())
		cancel(actor, false);

	actor.longAction().begin(
		action,
		contextGrid,
		GetAPsForMultiTurnAction(&actor, action));

	if (!IsJa2TacticalTurnBasedCombat())
		update(actor);

	return true;
}

void TacticalActorLongActions::cancel(
	TacticalActor& actor,
	bool finished)
{
	const std::uint8_t action = actor.longAction().action();
	if (!finished &&
		action > MTA_NONE &&
		action < NUM_MTA)
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			szMTATextStr[STR_MTA_CANCEL],
			actor.GetName(),
			szMTATextStr[action]);
	}

	actor.longAction().clear();
}

bool TacticalActorLongActions::update(TacticalActor& actor)
{
	SoldierLongActionComponent& state = actor.longAction();
	if (!state.active())
		return false;

	if (!validateLiveActor(actor) ||
		TileIsOutOfBounds(state.contextGrid()))
	{
		cancel(actor, false);
		return false;
	}

	const std::uint8_t action = state.action();
	if (action <= MTA_NONE || action >= NUM_MTA)
	{
		cancel(actor, false);
		return false;
	}

	if (requiresTool(action))
	{
		const std::uint16_t animationState =
			actor.animationPlayback().state();
		if (animationState >= NUMANIMATIONSTATES)
		{
			cancel(actor, false);
			return false;
		}
		if (gAnimControl[animationState].ubEndHeight !=
			ANIM_CROUCH)
		{
			return true;
		}
	}

	OBJECTTYPE* tool = nullptr;
	if (requiresTool(action))
	{
		if (HANDPOS >= actor.inventory().size())
		{
			cancel(actor, false);
			return false;
		}

		tool = &actor.inventory()[HANDPOS];
		if (!tool->exists() ||
			tool->usItem >= MAXITEMS ||
			tool->objectStack.empty() ||
			actor.position().direction() >=
				NUM_WORLD_DIRECTIONS)
		{
			cancel(actor, false);
			return false;
		}

		const std::int32_t facingGrid =
			NewGridNo(
				actor.position().gridNo(),
				DirectionInc(
					actor.position().direction()));
		if (TileIsOutOfBounds(facingGrid) ||
			state.contextGrid() != facingGrid)
		{
			cancel(actor, false);
			return false;
		}
	}

	std::int16_t entireActionPointCost = 0;
	std::int16_t entireBreathPointCost = 0;
	bool actionStillValid = true;
	std::uint16_t hackStructureIndex =
		INTERACTIVE_STRUCTURE_MAX;

	switch (action)
	{
	case MTA_FORTIFY:
		entireActionPointCost =
			GetAPsForMultiTurnAction(
				&actor,
				MTA_FORTIFY);
		entireBreathPointCost =
			APBPConstants[BP_FORTIFICATION];
		actionStillValid =
			IsFortificationPossibleAtGridNo(
				state.contextGrid()) &&
			IsStructureConstructItem(
				tool->usItem,
				state.contextGrid(),
				&actor);
		break;

	case MTA_REMOVE_FORTIFY:
		entireActionPointCost =
			GetAPsForMultiTurnAction(
				&actor,
				MTA_REMOVE_FORTIFY);
		entireBreathPointCost =
			APBPConstants[BP_REMOVE_FORTIFICATION];
		actionStillValid =
			IsStructureDeconstructItem(
				tool->usItem,
				state.contextGrid(),
				&actor);
		break;

	case MTA_HACK:
		entireActionPointCost =
			GetAPsForMultiTurnAction(&actor, MTA_HACK);
		actionStillValid =
			TacticalActorModifiers::interactiveActionSkill(
				actor,
				INTERACTIVE_STRUCTURE_HACKABLE) != 0 &&
			InteractiveActionPossibleAtGridNo(
				state.contextGrid(),
				actor.position().level(),
				hackStructureIndex) ==
				INTERACTIVE_STRUCTURE_HACKABLE &&
			hackStructureIndex <
				INTERACTIVE_STRUCTURE_MAX;
		break;

	default:
		actionStillValid = false;
		break;
	}

	if (!actionStillValid)
	{
		cancel(actor, false);
		return false;
	}

	if (action == MTA_FORTIFY ||
		action == MTA_REMOVE_FORTIFY)
	{
		if (!IsJa2TacticalTurnBasedCombat())
		{
			state.completeCost();
		}
		else
		{
			if (!is_networked)
			{
				actor.EVENT_InitNewSoldierAnim(
					CUTTING_FENCE,
					0,
					FALSE);
			}
			else
			{
				actor.ChangeSoldierState(
					CUTTING_FENCE,
					0,
					FALSE);
			}
			state.consumeActionPoints(
				APBPConstants[AP_USEWIRECUTTERS]);
		}
	}
	else if (
		action == MTA_HACK &&
		!IsJa2TacticalTurnBasedCombat())
	{
		state.completeCost();
	}

	if (state.remainingActionPoints() <=
		actor.actionPoints().current())
	{
		switch (action)
		{
		case MTA_FORTIFY:
			if (BuildFortification(
					state.contextGrid(),
					&actor,
					tool))
			{
				StatChange(&actor, STRAMT, 4, TRUE);
				StatChange(&actor, HEALTHAMT, 2, TRUE);
			}
			break;

		case MTA_REMOVE_FORTIFY:
			if (RemoveFortification(
					state.contextGrid(),
					&actor,
					tool))
			{
				StatChange(&actor, STRAMT, 3, TRUE);
				StatChange(&actor, HEALTHAMT, 2, TRUE);
			}
			break;

		case MTA_HACK:
		{
			const std::uint16_t possibleAction =
				InteractiveActionPossibleAtGridNo(
					state.contextGrid(),
					actor.position().level(),
					hackStructureIndex);
			if (hackStructureIndex >=
				INTERACTIVE_STRUCTURE_MAX)
			{
				cancel(actor, false);
				return false;
			}

			const std::uint16_t skill =
				TacticalActorModifiers::
					interactiveActionSkill(
						actor,
						possibleAction);
			const std::int32_t difficulty =
				gInteractiveStructure[
					hackStructureIndex]
					.difficulty;
			const std::int32_t luaActionId =
				gInteractiveStructure[
					hackStructureIndex]
					.luaactionid;
			const bool success =
				possibleAction ==
					INTERACTIVE_STRUCTURE_HACKABLE &&
				skill >= difficulty;

			if (luaActionId >= 0)
			{
				LuaHandleInteractiveActionResult(
					gWorldSectorX,
					gWorldSectorY,
					gbWorldSectorZ,
					state.contextGrid(),
					actor.position().level(),
					actor.identity().id(),
					possibleAction,
					luaActionId,
					difficulty,
					skill);
			}
			else
			{
				DoInteractiveActionDefaultResult(
					state.contextGrid(),
					actor.identity().id(),
					success);
			}
			break;
		}
		}

		if (entireActionPointCost > 0)
		{
			DeductPoints(
				&actor,
				state.remainingActionPoints(),
				entireBreathPointCost *
					state.remainingActionPoints() /
					entireActionPointCost,
				0);
		}

		cancel(actor, true);
	}
	else if (actor.actionPoints().current() > 0)
	{
		if (entireActionPointCost <= 0)
		{
			cancel(actor, false);
			return false;
		}

		const std::int16_t spentActionPoints =
			actor.actionPoints().current();
		DeductPoints(
			&actor,
			spentActionPoints,
			entireBreathPointCost *
				spentActionPoints /
				entireActionPointCost,
			0);
		state.consumeActionPoints(spentActionPoints);
	}

	return true;
}
