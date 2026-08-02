#include "TacticalActorLighting.h"

#include "Animation Control.h"
#include "DEBUG.H"
#include "GameSettings.h"
#include "Font Control.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Overhead.h"
#include "SoldierRepository.h"
#include "Soldier Profile Constants.h"
#include "TacticalActor.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "environment.h"
#include "lighting.h"
#include "message.h"
#include "renderworld.h"
#include "soldier profile type.h"
#include "worldman.h"

#include <cstddef>
#include <cstdint>

namespace
{
constexpr std::size_t MaximumAttachmentDepth = 8;

bool hasBoundedObject(
	const OBJECTTYPE& object,
	std::size_t depth = 0) noexcept
{
	if (object.usItem >= MAXITEMS ||
		(object.exists() && object.objectStack.empty()))
	{
		return false;
	}

	for (const StackedObjectData& stacked : object.objectStack)
	{
		if (!stacked.attachments.empty() &&
			depth >= MaximumAttachmentDepth)
		{
			return false;
		}

		for (const OBJECTTYPE& attachment : stacked.attachments)
		{
			if (!hasBoundedObject(attachment, depth + 1))
				return false;
		}
	}
	return true;
}

bool hasBoundedInventory(
	const TacticalActor& actor) noexcept
{
	if (actor.inventory().size() < NUM_INV_SLOTS)
		return false;

	for (std::size_t slot = 0;
		 slot < actor.inventory().size();
		 ++slot)
	{
		if (!hasBoundedObject(actor.inventory()[slot]))
			return false;
	}
	return true;
}

bool hasLiveLightContext(
	const TacticalActor& actor) noexcept
{
	const std::uint8_t profile = actor.identity().profile();
	return IsJa2TacticalWorldLoaded() &&
		actor.roster().active() &&
		actor.roster().inSector() &&
		actor.identity().id().i < TOTAL_SOLDIERS &&
		actor.roster().team() >= 0 &&
		actor.roster().team() < MAXTEAMS &&
		actor.identity().bodyType() < TOTALBODYTYPES &&
		(profile == NO_PROFILE || profile < NUM_PROFILES) &&
		!TileIsOutOfBounds(actor.position().gridNo()) &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL &&
		actor.animationPlayback().state() < NUMANIMATIONSTATES &&
		hasBoundedInventory(actor);
}

bool hasValidLightHandle(
	const TacticalActor& actor) noexcept
{
	return !actor.renderState().hasLightSprite() ||
		(actor.renderState().lightSprite() >= 0 &&
		 actor.renderState().lightSprite() < MAX_LIGHT_SPRITES);
}

const char* lightTemplateFor(
	TacticalActor& actor)
{
	const std::int16_t visionRangeBonus =
		GetTotalVisionRangeBonus(
			&actor,
			NORMAL_LIGHTLEVEL_NIGHT);
	if (visionRangeBonus >= UVGOGGLES_BONUS)
		return "Light4";
	if (visionRangeBonus >= NIGHTSIGHTGOGGLES_BONUS)
		return "Light3";
	return "Light2";
}
}

bool TacticalActorLighting::createPersonalLight(
	TacticalActor& actor)
{
	if (!hasLiveLightContext(actor) ||
		actor.roster().team() != gbPlayerNum ||
		!hasValidLightHandle(actor))
	{
		return false;
	}

	if (actor.renderState().hasLightSprite())
		return true;

	const std::int32_t sprite =
		LightSpriteCreate(lightTemplateFor(actor), 0);
	if (sprite < 0 || sprite >= MAX_LIGHT_SPRITES)
	{
		DebugMsg(
			TOPIC_JA2,
			DBG_LEVEL_0,
			String("Soldier: Failed loading light"));
		actor.renderState().clearLightSprite();
		return false;
	}

	actor.renderState().lightSprite() = sprite;
	LightSprites[sprite].uiFlags |= MERC_LIGHT;
	if (actor.position().level() != FIRST_LEVEL)
		(void)LightSpriteRoofStatus(sprite, TRUE);
	return true;
}

bool TacticalActorLighting::recreatePersonalLight(
	TacticalActor& actor)
{
	if (!hasLiveLightContext(actor) ||
		actor.roster().team() != gbPlayerNum)
	{
		return false;
	}

	(void)destroyPersonalLight(actor);
	return createPersonalLight(actor);
}

bool TacticalActorLighting::destroyPersonalLight(
	TacticalActor& actor) noexcept
{
	if (!actor.renderState().hasLightSprite())
		return true;

	const std::int32_t sprite =
		actor.renderState().lightSprite();
	actor.renderState().clearLightSprite();
	if (sprite < 0 || sprite >= MAX_LIGHT_SPRITES)
		return false;

	(void)LightSpriteDestroy(sprite);
	return true;
}

bool TacticalActorLighting::positionPersonalLight(
	TacticalActor& actor)
{
	if (!hasLiveLightContext(actor) ||
		actor.roster().team() != gbPlayerNum ||
		actor.vitals().health() < OKLIFE ||
		ubAmbientLightLevel < MIN_AMB_LEVEL_FOR_MERC_LIGHTS ||
		!gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT])
	{
		return false;
	}

	const std::int32_t worldX =
		actor.position().worldXInt();
	const std::int32_t worldY =
		actor.position().worldYInt();
	if (worldX < 0 || worldY < 0)
		return false;

	const std::int32_t tileX = worldX / CELL_X_SIZE;
	const std::int32_t tileY = worldY / CELL_Y_SIZE;
	if (tileX < 0 || tileX >= WORLD_COLS ||
		tileY < 0 || tileY >= WORLD_ROWS)
	{
		return false;
	}

	if (!actor.renderState().hasLightSprite() &&
		!createPersonalLight(actor))
	{
		return false;
	}
	if (!hasValidLightHandle(actor))
		return false;

	const std::int32_t sprite =
		actor.renderState().lightSprite();
	const bool powered = LightSpritePower(sprite, TRUE);
	const bool markedFake = LightSpriteFake(sprite);
	const bool positioned = LightSpritePosition(
		sprite,
		static_cast<std::int16_t>(tileX),
		static_cast<std::int16_t>(tileY));
	return powered && markedFake && positioned;
}

bool TacticalActorLighting::setPersonalLightLevel(
	TacticalActor& actor) noexcept
{
	if (!hasLiveLightContext(actor) ||
		actor.roster().team() != gbPlayerNum)
	{
		return false;
	}

	LEVELNODE* const mercHead =
		GetMapElement(
			static_cast<std::uint32_t>(
				actor.position().gridNo()))
			.pMercHead;
	if (mercHead == nullptr)
		return false;

	LEVELNODE& mercNode = *mercHead;
	mercNode.ubShadeLevel = 3;
	mercNode.ubSumLights = 5;
	mercNode.ubMaxLights = 5;
	mercNode.ubNaturalShadeLevel = 5;
	return true;
}

namespace
{
void enableDisableSoldierLightEffects(BOOLEAN enableLights)
{
	for (SoldierID id = gTacticalStatus.Team[OUR_TEAM].bFirstID;
		 id <= gTacticalStatus.Team[OUR_TEAM].bLastID;
		 ++id)
	{
		TacticalActor* const actor = GetJa2SoldierRepository().resolve(id);
		if (actor == nullptr || !actor->roster().active() ||
			!actor->roster().inSector() || actor->vitals().health() < OKLIFE)
		{
			continue;
		}

		if (enableLights)
		{
			(void)TacticalActorLighting::positionPersonalLight(*actor);
		}
		else
		{
			(void)TacticalActorLighting::destroyPersonalLight(*actor);
			(void)TacticalActorLighting::setPersonalLightLevel(*actor);
		}
	}
}
}

void HandlePlayerTogglingLightEffects(BOOLEAN toggleValue)
{
	if (toggleValue)
	{
		const bool lightsEnabled =
			gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT] != FALSE;
		gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT] =
			lightsEnabled ? FALSE : TRUE;
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			pMessageStrings[
				lightsEnabled
					? MSG_MERC_CASTS_LIGHT_OFF
					: MSG_MERC_CASTS_LIGHT_ON]);
	}

	enableDisableSoldierLightEffects(
		gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT]);
	SetRenderFlags(RENDER_FLAG_FULL);
}
