#ifndef ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_H
#define ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_H

#include <cstdint>
#include <type_traits>
#include <variant>

#include <Engine/Adapters/JA2/TacticalEntity.h>

enum class SimulationCommandSource : std::uint8_t
{
	LocalPlayer,
	NetworkPeer,
	System,
	Replay
};

// Keep source validation beside the public wire vocabulary so package
// ingress, codecs, and application executors cannot silently disagree when a
// new producer is added.
constexpr bool IsValidSimulationCommandSource(
	SimulationCommandSource source) noexcept
{
	switch (source)
	{
		case SimulationCommandSource::LocalPlayer:
		case SimulationCommandSource::NetworkPeer:
		case SimulationCommandSource::System:
		case SimulationCommandSource::Replay:
			return true;
	}
	return false;
}

struct EndTurnCommand
{
	std::uint8_t nextTeam;
	SimulationCommandSource source;
};

struct ChangeStanceCommand
{
	// Incarnation zero is reserved for decoded version-1 journals and is never
	// emitted by the version-2 encoder. Executors must deliberately resolve or
	// reject such a legacy-unresolved reference before changing live state.
	TacticalEntityId soldier;
	std::uint8_t stance;
	SimulationCommandSource source;
};

struct BeginFireWeaponCommand
{
	TacticalEntityId soldier;
	std::int32_t targetGrid;
	std::int8_t targetLevel;
	std::int8_t targetCubeLevel;
	SimulationCommandSource source;
};

// Preserve the legacy fFromUI modes as explicit replay/network vocabulary.
// Values are intentionally identical to EVENT_InternalGetNewSoldierPath's
// established 0/1/2/3 policy.
enum class TacticalMoveOrigin : std::uint8_t
{
	System = 0,
	PlayerUi = 1,
	ContinueMovement = 2,
	TeamAwareUi = 3
};

constexpr bool IsValidTacticalMoveOrigin(TacticalMoveOrigin origin) noexcept
{
	switch (origin)
	{
		case TacticalMoveOrigin::System:
		case TacticalMoveOrigin::PlayerUi:
		case TacticalMoveOrigin::ContinueMovement:
		case TacticalMoveOrigin::TeamAwareUi:
			return true;
	}
	return false;
}

enum class TacticalPendingActionPolicy : std::uint8_t
{
	Preserve,
	Clear
};

constexpr bool IsValidTacticalPendingActionPolicy(
	TacticalPendingActionPolicy policy) noexcept
{
	switch (policy)
	{
		case TacticalPendingActionPolicy::Preserve:
		case TacticalPendingActionPolicy::Clear:
			return true;
	}
	return false;
}

struct MoveToGridCommand
{
	TacticalEntityId soldier;
	std::int32_t destinationGrid;
	std::uint16_t movementMode;
	bool reverse;
	bool forceRestart;
	SimulationCommandSource source;
	// Appended defaults retain source compatibility for existing aggregate
	// initializers and preserve the original synchronous UI behavior.
	TacticalMoveOrigin origin = TacticalMoveOrigin::PlayerUi;
	TacticalPendingActionPolicy pendingAction =
		TacticalPendingActionPolicy::Clear;
};

struct SetFacingCommand
{
	TacticalEntityId soldier;
	std::uint8_t direction;
	SimulationCommandSource source;
};

inline constexpr std::uint8_t TacticalDirectionCount = 8;

constexpr bool IsValidTacticalDirection(std::uint8_t direction) noexcept
{
	return direction < TacticalDirectionCount;
}

struct SetStealthModeCommand
{
	TacticalEntityId soldier;
	bool enabled;
	SimulationCommandSource source;
};

struct StopMovementCommand
{
	TacticalEntityId soldier;
	SimulationCommandSource source;
};

struct CycleWeaponModeCommand
{
	TacticalEntityId soldier;
	SimulationCommandSource source;
};

// A target grid lets the compatibility executor retain the merc's current aim
// level where possible. -1 is the stable value-only spelling of "no target".
inline constexpr std::int32_t TacticalNoTargetGrid = -1;

struct CycleScopeModeCommand
{
	TacticalEntityId soldier;
	std::int32_t targetGrid;
	SimulationCommandSource source;
};

struct ReloadWeaponCommand
{
	TacticalEntityId soldier;
	bool reloadEvenIfNotEmpty;
	SimulationCommandSource source;
};

// A closed, value-only command set keeps the deterministic queue independent
// from JA2 globals and pointers. New commands extend this variant while their
// legacy executors remain in the compatibility layer during migration.
using SimulationCommand = std::variant<
	EndTurnCommand,
	ChangeStanceCommand,
	BeginFireWeaponCommand,
	MoveToGridCommand,
	SetFacingCommand,
	SetStealthModeCommand,
	StopMovementCommand,
	CycleWeaponModeCommand,
	CycleScopeModeCommand,
	ReloadWeaponCommand>;

// Shared transport/admission validation deliberately covers only the public
// value shape. Application-specific ranges and live-world policy belong to the
// JA2 executor. Keeping this visitor here prevents codecs and package ingress
// from acquiring subtly different command allowlists as the vocabulary grows.
inline bool IsStructurallyValidSimulationCommand(
	const SimulationCommand& command) noexcept
{
	if (command.valueless_by_exception()) return false;
	return std::visit([](const auto& value) noexcept {
		using Command = typename std::decay<decltype(value)>::type;
		if (!IsValidSimulationCommandSource(value.source)) return false;
		if constexpr (std::is_same<Command, EndTurnCommand>::value)
		{
			return true;
		}
		else
		{
			if (!value.soldier.valid()) return false;
			if constexpr (std::is_same<Command, MoveToGridCommand>::value)
				return IsValidTacticalMoveOrigin(value.origin) &&
					IsValidTacticalPendingActionPolicy(value.pendingAction);
			if constexpr (std::is_same<Command, SetFacingCommand>::value)
				return IsValidTacticalDirection(value.direction);
			return true;
		}
	}, command);
}

#endif
