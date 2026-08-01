#pragma once

class TacticalActor;

namespace TacticalActorConditionPresentation
{
	void appendFoodDescription(
		TacticalActor& actor,
		wchar_t* destination);
	void appendDiseaseDescription(
		TacticalActor& actor,
		wchar_t* destination,
		bool fullDescription = false);
	void appendSleepDescription(
		TacticalActor& actor,
		wchar_t* destination);
	void appendSummary(
		TacticalActor& actor,
		wchar_t* destination,
		bool fullDescription = false);
}
