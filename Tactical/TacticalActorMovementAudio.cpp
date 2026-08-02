#include "TacticalActorMovementAudio.h"

#include "Animation Control.h"
#include "Overhead.h"
#include "Sound Control.h"
#include "TacticalActor.h"
#include "TacticalActorMobility.h"
#include "TacticalActorStateFlags.h"
#include "TacticalWorldAdapter.h"
#include "Vehicles.h"
#include "random.h"
#include "soundman.h"
#include "tiledef.h"

namespace
{
VEHICLETYPE* resolveVehicle(const TacticalActor& actor)
{
	const INT32 vehicleId =
		actor.vehicleState().tacticalVehicleId();
	if (pVehicleList == nullptr ||
		vehicleId < 0 ||
		vehicleId >= ubNumberOfVehicles ||
		pVehicleList[vehicleId].fValid == FALSE)
	{
		return nullptr;
	}
	return &pVehicleList[vehicleId];
}
}

void TacticalActorMovementAudio::setVehicleMovement(
	TacticalActor& actor,
	bool enabled)
{
	VEHICLETYPE* vehicle = resolveVehicle(actor);
	if (vehicle == nullptr)
		return;
	if (enabled)
	{
		// Movement samples are emitted with footsteps; this transition only
		// owns stopping a retained vehicle loop from older content.
		return;
	}

	if (vehicle->iMovementSoundID != NO_SAMPLE)
	{
		SoundStop(vehicle->iMovementSoundID);
		vehicle->iMovementSoundID = NO_SAMPLE;
	}
}

namespace
{
void playFootstepUnchecked(TacticalActor& actor)
{
	UINT8 randomSound;
	INT8 volume = MIDVOLUME;
	UINT32 soundBase = WALK_LEFT_OUT;
	UINT8 randomMax = 4;

	if (!(actor.status().flags() & SOLDIER_VEHICLE))
	{
		if (actor.animationPlayback().state() == HOPFENCE ||
			actor.animationPlayback().state() == JUMPWINDOWS)
		{
			volume = HIGHVOLUME;
		}

		if (actor.status().flags() & SOLDIER_ROBOT)
		{
			PlaySoldierJA2Sample(
				actor.identity().id(),
				ROBOT_BEEP,
				RATE_11025,
				SoundVolume(volume, actor.position().gridNo()),
				1,
				SoundDir(actor.position().gridNo()),
				TRUE);
			return;
		}

		if (actor.animationPlayback().state() == CRAWLING)
		{
			soundBase = CRAWL_1;
		}
		else if (actor.position().terrainType() == FLAT_FLOOR)
		{
			soundBase = WALK_LEFT_IN;
		}
		else if (actor.position().terrainType() == DIRT_ROAD ||
			actor.position().terrainType() == PAVED_ROAD)
		{
			soundBase = WALK_LEFT_ROAD;
		}
		else if (TacticalActorMobility::inShallowWater(actor))
		{
			soundBase = WATER_WALK1_IN;
			randomMax = 2;
		}
		else if (TacticalActorMobility::inDeepWater(actor))
		{
			soundBase = SWIM_1;
			randomMax = 2;
		}

		do
		{
			randomSound = static_cast<UINT8>(Random(randomMax));
		}
		while (randomSound == actor.audio().lastFootstepVariant());

		actor.audio().recordFootstepVariant(randomSound);
		if (!IsJa2TacticalCombatActive() &&
			actor.identity().id() != gusSelectedSoldier)
		{
			volume = LOWVOLUME;
		}

		PlaySoldierJA2Sample(
			actor.identity().id(),
			soundBase + actor.audio().lastFootstepVariant(),
			RATE_11025,
			SoundVolume(volume, actor.position().gridNo()),
			1,
			SoundDir(actor.position().gridNo()),
			TRUE);
		return;
	}

	if (actor.animationPlayback().state() == RUNNING)
		volume = HIGHVOLUME;

	VEHICLETYPE* vehicle = resolveVehicle(actor);
	if (vehicle != nullptr)
	{
		PlaySoldierJA2Sample(
			actor.identity().id(),
			vehicle->iMoveSound,
			RATE_11025,
			SoundVolume(volume, actor.position().gridNo()),
			1,
			SoundDir(actor.position().gridNo()),
			TRUE);
	}
}
}

void TacticalActorMovementAudio::playFootstep(
	TacticalActor& actor,
	bool ignoreStealth)
{
	if (!ignoreStealth && actor.movement().stealthMode())
		return;
	playFootstepUnchecked(actor);
}

void HandleVehicleMovementSound(TacticalActor* actor, BOOLEAN enabled)
{
	if (actor != nullptr)
		TacticalActorMovementAudio::setVehicleMovement(
			*actor,
			enabled != FALSE);
}

void PlaySoldierFootstepSound(TacticalActor* actor)
{
	if (actor != nullptr)
		TacticalActorMovementAudio::playFootstep(*actor);
}

void PlayStealthySoldierFootstepSound(TacticalActor* actor)
{
	if (actor != nullptr)
		TacticalActorMovementAudio::playFootstep(*actor, true);
}
