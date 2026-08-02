#include "TacticalActorBattleSounds.h"
#include "TacticalActorDamageFeedback.h"

#include "Animation Control.h"
#include "Interface.h"
#include "TacticalActor.h"
#include "Soldier Profile Constants.h"
#include "Soldier Profile.h"
#include "Timer Control.h"
#include "faces.h"
#include "gameloop.h"
#include "screenids.h"

#include <cstdint>

namespace
{
constexpr std::uint8_t battleSoundSetCount = 8;

bool hasValidFeedbackState(
	const TacticalActor& actor) noexcept
{
	const std::uint8_t profile =
		actor.identity().profile();
	const std::int32_t faceIndex =
		actor.renderBindings().faceIndex();
	return actor.identity().bodyType() < TOTALBODYTYPES &&
		(profile == NO_PROFILE || profile < NUM_PROFILES) &&
		actor.dialogue().battleSoundSet() <
			battleSoundSetCount &&
		faceIndex >= -1 &&
		faceIndex < NUM_FACE_SLOTS;
}
}

bool TacticalActorDamageFeedback::presentHit(
	TacticalActor& actor)
{
	if (!hasValidFeedbackState(actor))
		return false;

	if ((GetJA2Clock() -
		 actor.vitals().lastBleedGruntAt()) > 1000)
	{
		actor.vitals().lastBleedGruntAt() =
			GetJA2Clock();
		(void)TacticalActorBattleSounds::play(actor, BATTLE_SOUND_HIT1);
	}

	const std::uint32_t currentScreen =
		GetCurrentScreen();
	if ((actor.roster().inSector() &&
		 currentScreen == GAME_SCREEN) ||
		currentScreen != GAME_SCREEN)
	{
		actor.uiPresentation().startPortraitFlash();
		actor.uiPresentation().portraitFlashFrame() =
			FLASH_PORTRAIT_STARTSHADE;
		actor.timing().start(
			SoldierTimingComponent::Timer::PortraitFlash,
			FLASH_PORTRAIT_DELAY);
	}

	return true;
}
