#include "TacticalActorCrowBehavior.h"

#include "Animation Control.h"
#include "Isometric Utils.h"
#include "Overhead.h"
#include "Rotting Corpses.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "Tile Animation.h"

#include <cstring>

void HandleCrowShadowVisibility(TacticalActor* actor)
{
	if (actor->identity().bodyType() == CROW &&
		actor->animationPlayback().state() == CROW_FLY &&
		actor->renderBindings().animationTile() != nullptr)
	{
		HideAniTile(
			actor->renderBindings().animationTile(),
			actor->awareness().lastRenderedVisibility() == -1);
	}
}

void HandleCrowShadowNewGridNo(TacticalActor* actor)
{
	ANITILE_PARAMS parameters{};

	if (actor->identity().bodyType() != CROW)
		return;

	if (actor->renderBindings().animationTile() != nullptr)
	{
		DeleteAniTile(actor->renderBindings().animationTile());
		actor->renderBindings().animationTile() = nullptr;
	}

	if (TileIsOutOfBounds(actor->position().gridNo()) ||
		actor->animationPlayback().state() != CROW_FLY)
	{
		return;
	}

	parameters.sGridNo = actor->position().gridNo();
	parameters.ubLevelID = ANI_SHADOW_LEVEL;
	parameters.sDelay = actor->animationPlayback().delay();
	parameters.sStartFrame = 0;
	parameters.uiFlags = ANITILE_CACHEDTILE | ANITILE_FORWARD |
		ANITILE_LOOPING | ANITILE_USE_DIRECTION_FOR_START_FRAME;
	parameters.sX = actor->position().worldXInt();
	parameters.sY = actor->position().worldYInt();
	parameters.sZ = 0;
	std::strcpy(parameters.zCachedFile, "TILECACHE\\FLY_SHDW.STI");
	parameters.uiUserData3 = actor->position().direction();

	actor->renderBindings().animationTile() = CreateAnimationTile(&parameters);
	HandleCrowShadowVisibility(actor);
}

void HandleCrowShadowRemoveGridNo(TacticalActor* actor)
{
	if (actor->identity().bodyType() == CROW &&
		actor->animationPlayback().state() == CROW_FLY &&
		actor->renderBindings().animationTile() != nullptr)
	{
		DeleteAniTile(actor->renderBindings().animationTile());
		actor->renderBindings().animationTile() = nullptr;
	}
}

void HandleCrowShadowNewDirection(TacticalActor* actor)
{
	if (actor->identity().bodyType() == CROW &&
		actor->animationPlayback().state() == CROW_FLY &&
		actor->renderBindings().animationTile() != nullptr)
	{
		actor->renderBindings().animationTile()->uiUserData3 =
			actor->position().direction();
	}
}

void HandleCrowShadowNewPosition(TacticalActor* actor)
{
	if (actor->identity().bodyType() == CROW &&
		actor->animationPlayback().state() == CROW_FLY &&
		actor->renderBindings().animationTile() != nullptr)
	{
		actor->renderBindings().animationTile()->sRelativeX =
			actor->position().worldXInt();
		actor->renderBindings().animationTile()->sRelativeY =
			actor->position().worldYInt();
	}
}

void CrowsFlyAway(std::uint8_t team)
{
	for (SoldierID id = gTacticalStatus.Team[team].bFirstID;
		 id <= gTacticalStatus.Team[team].bLastID;
		 ++id)
	{
		TacticalActor* actor = GetJa2SoldierRepository().resolve(id);
		if (actor != nullptr && actor->roster().active() &&
			actor->roster().inSector() &&
			actor->identity().bodyType() == CROW &&
			actor->animationPlayback().state() != CROW_FLY)
		{
			HandleCrowFlyAway(actor);
		}
	}
}
