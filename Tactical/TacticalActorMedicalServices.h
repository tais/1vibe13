#pragma once

class TacticalActor;

namespace TacticalActorMedicalServices
{
	[[nodiscard]] bool canTreatForAi(
		TacticalActor& medic);
	bool treatAdjacentForAi(TacticalActor& medic);
	bool treatSelfForAi(TacticalActor& medic);

	void cancelReceiving(
		TacticalActor& patient,
		bool playEndAnimation = true);
	void cancelProviding(
		TacticalActor& medic,
		bool playEndAnimation = true);
}
