#pragma once

class TacticalActor;

namespace TacticalActorTraversal
{
	[[nodiscard]] bool beginRoofClimb(TacticalActor& actor);
	[[nodiscard]] bool beginRoofDescent(TacticalActor& actor);
	[[nodiscard]] bool beginFenceJump(TacticalActor& actor);
	// True consumes the path step, either starting its jump or entering the
	// native blocked-landing wait. A missing/mismatched fence fails unchanged.
	[[nodiscard]] bool beginPathFenceJump(TacticalActor& actor, bool replicate);
	[[nodiscard]] bool beginWallClimb(TacticalActor& actor);
	[[nodiscard]] bool beginWindowJump(TacticalActor& actor);
}
