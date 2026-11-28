#pragma once

#include <array>
#include <cstdint>

class TacticalActor;
enum class TacticalTraversalKind : std::uint8_t; // SimulationCommand.h

namespace TacticalActorTraversal
{
	enum class PlayerTraversalRefusal : std::uint8_t
	{
		None,
		Robot,
		Collapsed,
		Backpack,
		WindowsDisabled,
		NotEnoughPoints,
		Nothing
	};
	struct PlayerTraversalChoice
	{
		std::array<TacticalTraversalKind, 4> kinds{};
		std::uint8_t count = 0;
		PlayerTraversalRefusal refusal = PlayerTraversalRefusal::Nothing;
	};
	// The stock J key (climb a roof up or down, jump a fence, climb a wall) or,
	// with `window`, Shift+J (jump through a window), as a choice: every option
	// the merc has where it stands, in the order the key tries them, each gated
	// by the key's own checks (no robot, able to crouch, no backpack, enough
	// points). The key dispatches the options in order until one starts. With
	// none, `refusal` says why.
	[[nodiscard]] PlayerTraversalChoice choosePlayerTraversal(TacticalActor& actor, bool window);
	[[nodiscard]] bool beginRoofClimb(TacticalActor& actor);
	[[nodiscard]] bool beginRoofDescent(TacticalActor& actor);
	[[nodiscard]] bool beginFenceJump(TacticalActor& actor);
	// True consumes the path step, either starting its jump or entering the
	// native blocked-landing wait. A missing/mismatched fence fails unchanged.
	[[nodiscard]] bool beginPathFenceJump(TacticalActor& actor, bool replicate);
	[[nodiscard]] bool beginWallClimb(TacticalActor& actor);
	[[nodiscard]] bool beginWindowJump(TacticalActor& actor);
}
