#pragma once

class TacticalActor;

namespace TacticalActorTraversal
{
	[[nodiscard]] bool beginRoofClimb(TacticalActor& actor);
	[[nodiscard]] bool beginRoofDescent(TacticalActor& actor);
	[[nodiscard]] bool beginFenceJump(TacticalActor& actor);
	[[nodiscard]] bool beginWallClimb(TacticalActor& actor);
	[[nodiscard]] bool beginWindowJump(TacticalActor& actor);
}
