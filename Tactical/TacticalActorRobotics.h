#pragma once

class TacticalActor;

namespace TacticalActorRobotics
{
	[[nodiscard]] TacticalActor* controller(
		TacticalActor& robot) noexcept;
	[[nodiscard]] bool canBeControlled(
		TacticalActor& robot) noexcept;
	[[nodiscard]] bool isControlling(
		TacticalActor& controller) noexcept;

	void refreshControllerForRobot(
		TacticalActor& robot) noexcept;
	void refreshRobotsForController(
		TacticalActor& controller) noexcept;
}
