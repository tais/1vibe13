#include "TacticalActorDamageResolution.h"
#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorExplosives.h"
#include "TacticalActorRecovery.h"
#include "TacticalActorRouteExecution.h"

#include "Animation Control.h"
#include "Explosion Control.h"
#include "Handle Items.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "Soldier Functions.h"
#include "Sound Control.h"
#include "TacticalWorldAdapter.h"
#include "World Items.h"
#include "World Tile Map.h"
#include "random.h"
#include "worlddef.h"

#include <algorithm>
#include <cstddef>

namespace
{
void halveStatus(INT16& status)
{
	status = std::max<INT16>(1, status / 2);
}

bool hasLiveActionContext(
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

OBJECTTYPE* handObject(TacticalActor& actor) noexcept
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

bool isPlaceableExplosive(OBJECTTYPE& object)
{
	const UINT16 item = object.usItem;
	return Item[item].usItemClass == IC_BOMB ||
		Item[item].ubCursor == BOMBCURS ||
		(Item[item].ubCursor == INVALIDCURS &&
		 HasAttachmentOfClass(
			 &object,
			 AC_DETONATOR | AC_REMOTEDET | AC_DEFUSE));
}
}

namespace TacticalActorExplosives
{
void degradeInventoryAfterExplosion(TacticalActor& actor)
{
	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		OBJECTTYPE& object = actor.inventory()[slot];
		if (!object.exists() || object.objectStack.empty())
			continue;

		const std::size_t stackSize = std::min<std::size_t>(
			object.ubNumberOfObjects,
			object.objectStack.size());
		for (std::size_t index = 0; index < stackSize; ++index)
		{
			StackedObjectData* const stackedObject =
				object[static_cast<unsigned int>(index)];
			if (!stackedObject)
				continue;

			halveStatus(stackedObject->data.objectStatus);

			for (OBJECTTYPE& attachment : stackedObject->attachments)
			{
				if (!attachment.exists() ||
					attachment.objectStack.empty())
				{
					continue;
				}

				halveStatus(attachment[0]->data.objectStatus);
				halveStatus(attachment[0]->data.sRepairThreshold);
			}
		}
	}
}

void applyInventoryExplosion(TacticalActor& actor)
{
	degradeInventoryAfterExplosion(actor);

	const INT8 oldLife = actor.vitals().health();

	INT16 damage = static_cast<INT16>(30 + Random(20));
	if (actor.vitals().health() - damage < 0)
		damage = oldLife;

	// A lethal hit on a standing actor must still pass through collapse so the
	// established death handling can finish the animation state correctly.
	if (oldLife >= OKLIFE && oldLife <= damage)
		damage -= static_cast<INT16>(5 + Random(5));

	INT16 breathDamage = static_cast<INT16>(500 + Random(1500));
	if (actor.vitals().breath() - breathDamage < 0)
		breathDamage = actor.vitals().breath();

	PlayJA2SampleFromFile(
		"Sounds\\Explode1.wav",
		RATE_11025,
		HIGHVOLUME,
		1,
		MIDDLEPAN);

	TacticalActorDamageResolution::takeDamage(actor,
		0,
		damage,
		breathDamage,
		TAKE_DAMAGE_EXPLOSION,
		actor.identity().id(),
		actor.position().gridNo(),
		0,
		TRUE);

	if (actor.vitals().health() <= 0)
	{
		HandleTakeDamageDeath(&actor, oldLife, TAKE_DAMAGE_BLOODLOSS);
	}
	else if (actor.vitals().health() < OKLIFE &&
			 !actor.collapseState().collapsed())
	{
		(void)TacticalActorRecovery::collapse(actor);
	}
}

bool selfDetonate(TacticalActor& actor)
{
	if (!(actor.status().flags() & SOLDIER_UNDERAICONTROL))
		return false;

	const INT32 plannedItem = actor.aiPlanning().actionData();
	if (plannedItem <= NOTHING || plannedItem >= MAXITEMS)
		return false;

	const INT32 gridNo = actor.position().gridNo();
	if (!IsJa2TacticalWorldLoaded() ||
		TileIsOutOfBounds(gridNo) ||
		static_cast<UINT32>(gridNo) >= GetWorldTileMapSize() ||
		actor.position().direction() >= NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		OBJECTTYPE& object = actor.inventory()[slot];
		if (!object.exists() ||
			object.usItem >= MAXITEMS ||
			object.usItem != static_cast<UINT16>(plannedItem))
		{
			continue;
		}

		IgniteExplosion(
			actor.identity().id(),
			actor.position().worldXInt(),
			actor.position().worldYInt(),
			GetMapElement(static_cast<UINT32>(gridNo)).sHeight,
			gridNo,
			object.usItem,
			actor.position().level(),
			actor.position().direction());

		DeleteObj(&object);
		return true;
	}

	return false;
}

bool beginBombPlacement(TacticalActor& actor)
{
	OBJECTTYPE* const object = handObject(actor);
	const std::int32_t targetGrid =
		actor.pendingAction().secondaryData();
	if (!hasLiveActionContext(actor) ||
		TileIsOutOfBounds(targetGrid) ||
		!object ||
		!isPlaceableExplosive(*object))
	{
		return false;
	}

	const std::uint16_t animation =
		actor.animationPlayback().state();
	if (gAnimControl[animation].ubHeight == ANIM_STAND)
	{
		TacticalActorAnimationTransitions::initializeAnimation(actor,
			PLANT_BOMB,
			0,
			FALSE);
	}
	else
	{
		HandleSoldierDropBomb(&actor, targetGrid);
		(void)TacticalActorRouteExecution::settleIntoStationaryStance(actor);
	}
	return true;
}

bool beginTripwireDisarm(
	TacticalActor& actor,
	std::int32_t gridNo,
	std::int32_t worldItemIndex)
{
	if (!hasLiveActionContext(actor) ||
		TileIsOutOfBounds(gridNo) ||
		worldItemIndex < 0 ||
		static_cast<std::size_t>(worldItemIndex) >=
			gWorldItems.size())
	{
		return false;
	}

	WORLDITEM& worldItem =
		gWorldItems[static_cast<std::size_t>(worldItemIndex)];
	OBJECTTYPE& object = worldItem.object;
	if (!worldItem.fExists ||
		worldItem.sGridNo != gridNo ||
		worldItem.ubLevel != actor.position().level() ||
		!object.exists() ||
		object.usItem >= MAXITEMS ||
		object.objectStack.empty() ||
		!object[0] ||
		!(object.fFlags & OBJECT_ARMED_BOMB) ||
		!ItemIsTripwire(object.usItem))
	{
		return false;
	}

	const std::uint16_t animation =
		actor.animationPlayback().state();
	if (gAnimControl[animation].ubHeight == ANIM_STAND)
	{
		TacticalActorAnimationTransitions::initializeAnimation(actor,
			CROUCHING,
			0,
			FALSE);
	}
	else
	{
		HandleSoldierDefuseTripwire(
			&actor,
			gridNo,
			worldItemIndex);
		(void)TacticalActorRouteExecution::settleIntoStationaryStance(actor);
	}
	return true;
}

bool beginDetonatorUse(TacticalActor& actor)
{
	OBJECTTYPE* const object = handObject(actor);
	const std::int32_t targetGrid =
		actor.pendingAction().secondaryData();
	if (!hasLiveActionContext(actor) ||
		TileIsOutOfBounds(targetGrid) ||
		!object ||
		Item[object->usItem].ubCursor != REMOTECURS ||
		ItemHasXRay(object->usItem))
	{
		return false;
	}

	const std::uint16_t animation =
		actor.animationPlayback().state();
	if (gAnimControl[animation].ubHeight == ANIM_STAND)
	{
		TacticalActorAnimationTransitions::initializeAnimation(actor,
			USE_REMOTE,
			0,
			FALSE);
	}
	else
	{
		HandleSoldierUseRemote(&actor, targetGrid);
	}
	return true;
}
}
