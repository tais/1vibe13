#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorTurnLifecycle
{
	void beginTurn(
		TacticalActor& actor,
		bool fromRealTime,
		std::int32_t realTimeCounter);
}
