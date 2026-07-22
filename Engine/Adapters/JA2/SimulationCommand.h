#ifndef ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_H
#define ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_H

#include <cstdint>
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

// A closed, value-only command set keeps the deterministic queue independent
// from JA2 globals and pointers. New commands extend this variant while their
// legacy executors remain in the compatibility layer during migration.
using SimulationCommand = std::variant<
	EndTurnCommand,
	ChangeStanceCommand,
	BeginFireWeaponCommand,
	MoveToGridCommand>;

#endif
