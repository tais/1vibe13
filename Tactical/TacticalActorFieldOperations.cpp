#include "TacticalActorFieldOperations.h"

#include "Animation Control.h"
#include "Assignments.h"
#include "GameSettings.h"
#include "Handle Items.h"
#include "Handle UI.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Keys.h"
#include "LuaInitNPCs.h"
#include "Overhead.h"
#include "Points.h"
#include "Queen Command.h"
#include "Rotting Corpses.h"
#include "Soldier Control.h"
#include "SoldierRepository.h"
#include "Sound Control.h"
#include "Structure Wrap.h"
#include "TacticalActorConditions.h"
#include "TacticalActorLongActions.h"
#include "TacticalActorModifiers.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "ai.h"
#include "connect.h"
#include "message.h"
#include "strategicmap.h"
#include "structure.h"
#include "worldman.h"

#include <algorithm>

namespace
{
bool hasValidAnimationState(
	const TacticalActor& actor) noexcept
{
	return actor.animationPlayback().state() <
		NUMANIMATIONSTATES;
}

bool hasValidWorldPosition(
	const TacticalActor& actor) noexcept
{
	return !TileIsOutOfBounds(actor.position().gridNo()) &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL;
}

bool isLiveWorldActor(
	const TacticalActor& actor) noexcept
{
	return IsJa2TacticalWorldLoaded() &&
		actor.roster().active() &&
		actor.roster().inSector() &&
		hasValidWorldPosition(actor) &&
		hasValidAnimationState(actor);
}

bool hasValidTarget(
	const TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction) noexcept
{
	return isLiveWorldActor(actor) &&
		!TileIsOutOfBounds(targetGrid) &&
		direction < NUM_WORLD_DIRECTIONS;
}

const OBJECTTYPE* usableHandItem(
	const TacticalActor& actor) noexcept
{
	if (HANDPOS >= actor.inventory().size())
		return nullptr;

	const OBJECTTYPE& object = actor.inventory()[HANDPOS];
	if (!object.exists() ||
		object.usItem >= MAXITEMS ||
		object.objectStack.empty())
	{
		return nullptr;
	}
	return &object;
}

OBJECTTYPE* usableHandItem(TacticalActor& actor) noexcept
{
	return const_cast<OBJECTTYPE*>(
		usableHandItem(
			static_cast<const TacticalActor&>(actor)));
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

bool isWindowBreakingTool(
	const OBJECTTYPE& object) noexcept
{
	if (object.usItem >= MAXITEMS ||
		object.objectStack.empty() ||
		object[0]->data.objectStatus < USABLE)
	{
		return false;
	}

	const UINT32 itemClass = Item[object.usItem].usItemClass;
	return (ItemIsCrowbar(object.usItem) &&
			(itemClass & IC_PUNCH)) ||
		((itemClass & IC_GUN) &&
		 ItemIsTwoHanded(object.usItem) &&
		 ItemIsMetal(object.usItem));
}
}

bool TacticalActorFieldOperations::beginFenceCutting(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	const OBJECTTYPE* const tool = usableHandItem(actor);
	if (!hasValidTarget(actor, targetGrid, direction) ||
		!tool ||
		!ItemIsWirecutters(tool->usItem) ||
		!IsCuttableWireFenceAtGridNo(targetGrid))
	{
		releaseUi(actor);
		return false;
	}

	faceDirection(actor, direction);
	actor.targeting().gridNo() = targetGrid;
	playNetworkAwareAnimation(actor, CUTTING_FENCE);
	return true;
}

bool TacticalActorFieldOperations::beginRepair(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	const OBJECTTYPE* const tool = usableHandItem(actor);
	if (!hasValidTarget(actor, targetGrid, direction) ||
		!tool ||
		!ItemIsToolkit(tool->usItem))
	{
		releaseUi(actor);
		return false;
	}

	UINT16 targetId = NOBODY;
	const UINT8 repairTarget =
		IsRepairableStructAtGridNo(targetGrid, &targetId);
	if (repairTarget < 1 || repairTarget > 3)
	{
		releaseUi(actor);
		return false;
	}

	faceDirection(actor, direction);
	actor.EVENT_InitNewSoldierAnim(
		GOTO_REPAIRMAN,
		0,
		FALSE);
	if (IsJa2TacticalCombatActive())
	{
		releaseUi(actor);
		return false;
	}

	switch (repairTarget)
	{
	case 1:
		SetSoldierAssignment(
			&actor,
			REPAIR,
			FALSE,
			TRUE,
			-1);
		break;
	case 2:
		SetSoldierAssignment(
			&actor,
			REPAIR,
			FALSE,
			FALSE,
			targetId);
		break;
	case 3:
		SetSoldierAssignment(
			&actor,
			REPAIR,
			TRUE,
			FALSE,
			-1);
		break;
	}
	return true;
}

bool TacticalActorFieldOperations::beginRefuel(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	const OBJECTTYPE* const fuel = usableHandItem(actor);
	UINT16 targetId = NOBODY;
	if (!hasValidTarget(actor, targetGrid, direction) ||
		!fuel ||
		!ItemIsGascan(fuel->usItem) ||
		!IsRefuelableStructAtGridNo(
			targetGrid,
			&targetId))
	{
		releaseUi(actor);
		return false;
	}

	TacticalActor* const vehicle =
		GetJa2SoldierRepository().resolve(targetId);
	if (!vehicle ||
		!vehicle->roster().active() ||
		!vehicle->roster().inSector() ||
		vehicle->position().gridNo() != targetGrid ||
		vehicle->position().level() != FIRST_LEVEL ||
		!(vehicle->status().flags() & SOLDIER_VEHICLE))
	{
		releaseUi(actor);
		return false;
	}

	faceDirection(actor, direction);
	actor.EVENT_InitNewSoldierAnim(
		REFUEL_VEHICLE,
		0,
		FALSE);
	return true;
}

bool TacticalActorFieldOperations::beginCorpseBloodCollection(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	const OBJECTTYPE* const jar = usableHandItem(actor);
	if (!hasValidTarget(actor, targetGrid, direction) ||
		!jar ||
		!ItemIsJar(jar->usItem))
	{
		releaseUi(actor);
		return false;
	}

	ROTTING_CORPSE* const corpse =
		GetCorpseAtGridNo(
			targetGrid,
			actor.position().level());
	if (!corpse)
	{
		actor.DoMercBattleSound(BATTLE_SOUND_NOTHING);
		releaseUi(actor);
		return false;
	}

	actor.pendingAction().quaternaryData() = corpse->iID;
	faceDirection(actor, direction);
	actor.EVENT_InitNewSoldierAnim(
		TAKE_BLOOD_FROM_CORPSE,
		0,
		FALSE);
	return true;
}

bool TacticalActorFieldOperations::attachDoorAlarm(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	OBJECTTYPE* const alarm = usableHandItem(actor);
	if (!hasValidTarget(actor, targetGrid, direction) ||
		!alarm ||
		!ItemIsCanAndString(alarm->usItem))
	{
		releaseUi(actor);
		return false;
	}

	STRUCTURE* const door =
		FindStructure(targetGrid, STRUCTURE_ANYDOOR);
	if (!door)
	{
		releaseUi(actor);
		return false;
	}

	if (!(door->fFlags & STRUCTURE_OPEN))
		ModifyDoorStatus(targetGrid, FALSE, FALSE);
	else
		ModifyDoorStatus(targetGrid, TRUE, TRUE);

	DOOR_STATUS* const status = GetDoorStatus(targetGrid);
	if (!status)
	{
		releaseUi(actor);
		return false;
	}

	status->ubFlags |= DOOR_HAS_TIN_CAN;
	faceDirection(actor, direction);
	actor.EVENT_InitNewSoldierAnim(
		ATTACH_CAN_TO_STRING,
		0,
		FALSE);
	alarm->RemoveObjectsFromStack(1);
	fInterfacePanelDirty = DIRTYLEVEL2;
	return true;
}

bool TacticalActorFieldOperations::beginFortification(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	OBJECTTYPE* const tool = usableHandItem(actor);
	if (!hasValidTarget(actor, targetGrid, direction) ||
		!tool)
	{
		releaseUi(actor);
		return false;
	}

	if (!gGameExternalOptions
			.fFortificationAllowInHostileSector &&
		gWorldSectorX > 0 &&
		gWorldSectorY > 0 &&
		NumEnemiesInAnySector(
			gWorldSectorX,
			gWorldSectorY,
			gbWorldSectorZ) > 0)
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			New113Message[MSG113_CANNOT_BUILD]);
		releaseUi(actor);
		return false;
	}

	const bool canConstruct =
		IsStructureConstructItem(
			tool->usItem,
			targetGrid,
			&actor);
	const bool canDeconstruct =
		IsStructureDeconstructItem(
			tool->usItem,
			targetGrid,
			&actor);
	if (!canConstruct && !canDeconstruct)
	{
		actor.DoMercBattleSound(BATTLE_SOUND_NOTHING);
		releaseUi(actor);
		return false;
	}

	STRUCTURE* const structure =
		FindStructure(targetGrid, STRUCTURE_GENERIC);
	const UINT8 action =
		!structure && canConstruct
			? MTA_FORTIFY
			: MTA_REMOVE_FORTIFY;
	if ((action == MTA_REMOVE_FORTIFY &&
		 !canDeconstruct) ||
		!TacticalActorLongActions::start(
			actor,
			action,
			targetGrid))
	{
		actor.DoMercBattleSound(BATTLE_SOUND_NOTHING);
		releaseUi(actor);
		return false;
	}

	faceDirection(actor, direction);
	playNetworkAwareAnimation(actor, CUTTING_FENCE);
	return true;
}

bool TacticalActorFieldOperations::performInteractiveAction(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint16_t expectedAction)
{
	if (!isLiveWorldActor(actor) ||
		TileIsOutOfBounds(targetGrid) ||
		expectedAction <= INTERACTIVE_STRUCTURE_NO_ACTION ||
		expectedAction >= INTERACTIVE_STRUCTURE_TYPE_MAX ||
		gMaxInteractiveStructureRead >
			INTERACTIVE_STRUCTURE_MAX)
	{
		releaseUi(actor);
		return false;
	}

	UINT16 structureIndex = 0;
	const UINT16 possibleAction =
		InteractiveActionPossibleAtGridNo(
			targetGrid,
			actor.position().level(),
			structureIndex);
	if (possibleAction != expectedAction ||
		structureIndex >=
			gMaxInteractiveStructureRead ||
		structureIndex >= INTERACTIVE_STRUCTURE_MAX)
	{
		releaseUi(actor);
		return false;
	}

	const UINT16 skill =
		TacticalActorModifiers::interactiveActionSkill(
			actor,
			possibleAction);
	const INT32 difficulty =
		gInteractiveStructure[structureIndex].difficulty;
	const INT32 luaActionId =
		gInteractiveStructure[structureIndex].luaactionid;

	switch (possibleAction)
	{
	case INTERACTIVE_STRUCTURE_HACKABLE:
		PlayJA2SampleFromFile(
			"Sounds\\keyboard_typing.wav",
			RATE_11025,
			SoundVolume(
				MIDVOLUME,
				actor.position().gridNo()),
			1,
			SoundDir(actor.position().gridNo()));
		break;
	case INTERACTIVE_STRUCTURE_READFILE:
		PlayJA2SampleFromFile(
			"Sounds\\book_pageturn1.wav",
			RATE_11025,
			SoundVolume(
				MIDVOLUME,
				actor.position().gridNo()),
			1,
			SoundDir(actor.position().gridNo()));
		break;
	default:
		break;
	}

	if (possibleAction == INTERACTIVE_STRUCTURE_HACKABLE)
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			szInteractiveActionText[0],
			actor.GetName());
		return TacticalActorLongActions::start(
			actor,
			MTA_HACK,
			targetGrid);
	}

	if (luaActionId >= 0)
	{
		LuaHandleInteractiveActionResult(
			gWorldSectorX,
			gWorldSectorY,
			gbWorldSectorZ,
			targetGrid,
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
			targetGrid,
			actor.identity().id(),
			skill >= difficulty);
	}
	return true;
}

bool TacticalActorFieldOperations::beginRobotReload(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	if (!hasValidTarget(actor, targetGrid, direction))
	{
		releaseUi(actor);
		return false;
	}

	const SoldierID robotId =
		WhoIsThere2(
			targetGrid,
			actor.position().level());
	TacticalActor* const robot =
		GetJa2SoldierRepository().resolve(robotId);
	if (!robot ||
		!robot->roster().active() ||
		!robot->roster().inSector() ||
		robot->position().gridNo() != targetGrid ||
		robot->position().level() !=
			actor.position().level() ||
		!(robot->status().flags() & SOLDIER_ROBOT))
	{
		releaseUi(actor);
		return false;
	}

	faceDirection(actor, direction);
	actor.EVENT_InitNewSoldierAnim(
		RELOAD_ROBOT,
		0,
		FALSE);
	return true;
}

bool TacticalActorFieldOperations::canBreakWindow(
	const TacticalActor& actor)
{
	if (!isLiveWorldActor(actor) ||
		actor.vitals().health() < OKLIFE ||
		TacticalActorConditions::isUnconscious(actor) ||
		actor.identity().bodyType() > REGFEMALE ||
		actor.position().direction() >=
			NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	const OBJECTTYPE* const tool = usableHandItem(actor);
	if (!tool || !isWindowBreakingTool(*tool))
		return false;

	INT32 windowGrid = actor.position().gridNo();
	if (actor.position().direction() == NORTH ||
		actor.position().direction() == WEST)
	{
		windowGrid = NewGridNo(
			actor.position().gridNo(),
			DirectionInc(
				actor.position().direction()));
	}
	if (TileIsOutOfBounds(windowGrid))
		return false;

	if (!IsJumpableWindowPresentAtGridNo(
			windowGrid,
			actor.position().direction(),
			TRUE) ||
		IsJumpableWindowPresentAtGridNo(
			windowGrid,
			actor.position().direction(),
			FALSE))
	{
		return false;
	}

	const STRUCTURE* const window =
		FindStructure(
			windowGrid,
			STRUCTURE_WALLNWINDOW);
	return window &&
		!(window->fFlags & STRUCTURE_OPEN);
}

bool TacticalActorFieldOperations::breakWindow(
	TacticalActor& actor)
{
	if (!canBreakWindow(actor))
		return false;

	const OBJECTTYPE* const tool = usableHandItem(actor);
	if (!tool)
		return false;

	actor.attackSelection().weapon() = tool->usItem;
	actor.aiPlanning().action() = AI_ACTION_KNIFE_STAB;
	actor.aiPlanning().actionData() =
		actor.position().gridNo();
	actor.pendingAction().clearAction();
	actor.targeting().gridNo() =
		actor.position().gridNo();
	actor.targeting().level() =
		actor.position().level();
	actor.targeting().targetId() = NOBODY;
	actor.EVENT_InitNewSoldierAnim(
		CROWBAR_ATTACK,
		0,
		FALSE);
	SetUIBusy(actor.identity().id());
	DeductPoints(
		&actor,
		GetAPsToBreakWindow(&actor, FALSE),
		BP_USE_CROWBAR);
	return true;
}
