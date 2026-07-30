#include "TacticalActorExplosives.h"

#include "Explosion Control.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Soldier Control.h"
#include "Soldier Functions.h"
#include "Sound Control.h"
#include "TacticalWorldAdapter.h"
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

	actor.SoldierTakeDamage(
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
		SoldierCollapse(&actor);
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
}
