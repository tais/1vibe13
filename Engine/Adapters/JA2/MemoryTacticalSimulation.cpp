#include <Engine/Adapters/JA2/MemoryTacticalSimulation.h>

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>
#include <variant>

MemoryTacticalSimulation::MemoryTacticalSimulation(
	TacticalSimulationLimits limits)
	: limits_(limits)
{}

TacticalSimulationResetError MemoryTacticalSimulation::reset(
	TacticalSimulationSnapshot snapshot) noexcept
{
	if (snapshot.actors.size() > limits_.maximumActors)
		return TacticalSimulationResetError::TooManyActors;
	if (snapshot.shots.size() > limits_.maximumShots)
		return TacticalSimulationResetError::TooManyShots;

	for (const TacticalSimulationActorState& actor : snapshot.actors)
	{
		if (!actor.id.valid())
			return TacticalSimulationResetError::InvalidActor;
		if (!IsValidTacticalDirection(actor.direction))
			return TacticalSimulationResetError::InvalidDirection;
	}
	std::sort(
		snapshot.actors.begin(), snapshot.actors.end(),
		[](const TacticalSimulationActorState& left,
			const TacticalSimulationActorState& right) {
			return left.id < right.id;
		});
	for (std::size_t index = 1; index < snapshot.actors.size(); ++index)
		if (snapshot.actors[index - 1].id == snapshot.actors[index].id)
			return TacticalSimulationResetError::DuplicateActor;

	for (const TacticalSimulationShot& shot : snapshot.shots)
	{
		if (!IsValidTacticalSimulationFireKind(shot.kind))
			return TacticalSimulationResetError::InvalidShotKind;
		if (!shot.soldier.valid())
			return TacticalSimulationResetError::UnknownShotActor;
		const auto actor = std::lower_bound(
			snapshot.actors.begin(), snapshot.actors.end(),
			shot.soldier,
			[](const TacticalSimulationActorState& candidate,
				TacticalEntityId sought) {
				return candidate.id < sought;
			});
		if (actor == snapshot.actors.end() ||
			actor->id != shot.soldier)
			return TacticalSimulationResetError::UnknownShotActor;
	}

	try
	{
		// Reserve before publication so every accepted command within the
		// configured ceilings is allocation-free and reset remains transactional.
		snapshot.actors.reserve(limits_.maximumActors);
		snapshot.shots.reserve(limits_.maximumShots);
	}
	catch (...)
	{
		return TacticalSimulationResetError::AllocationFailure;
	}
	static_assert(
		std::is_nothrow_move_assignable<TacticalSimulationSnapshot>::value,
		"transactional tactical publication requires a no-throw state swap");
	snapshot_ = std::move(snapshot);
	return TacticalSimulationResetError::None;
}

void MemoryTacticalSimulation::clear() noexcept
{
	snapshot_.currentTeam = 0;
	snapshot_.inCombat = false;
	snapshot_.completedTurns = 0;
	snapshot_.actors.clear();
	snapshot_.shots.clear();
}

TacticalSimulationActorState* MemoryTacticalSimulation::findActor(
	TacticalEntityId id) noexcept
{
	const auto actor = std::lower_bound(
		snapshot_.actors.begin(), snapshot_.actors.end(), id,
		[](const TacticalSimulationActorState& candidate,
			TacticalEntityId sought) {
			return candidate.id < sought;
		});
	return actor != snapshot_.actors.end() && actor->id == id
		? &*actor
		: nullptr;
}

bool MemoryTacticalSimulation::recordShot(
	TacticalSimulationShot shot) noexcept
{
	if (snapshot_.shots.size() >= limits_.maximumShots) return false;
	static_assert(
		std::is_nothrow_move_constructible<TacticalSimulationShot>::value,
		"reserved tactical shot recording must not throw");
	snapshot_.shots.push_back(std::move(shot));
	return true;
}

CommandDisposition MemoryTacticalSimulation::execute(
	const SimulationCommand& command,
	std::uint64_t tick,
	std::uint64_t sequence)
{
	if (!IsStructurallyValidSimulationCommand(command))
		return CommandDisposition::Discard;

	return std::visit([this, tick, sequence](const auto& value) {
		using Command = typename std::decay<decltype(value)>::type;
		if constexpr (
			std::is_same<Command, EndTurnCommand>::value ||
			std::is_same<Command, SynchronizeTurnCommand>::value)
		{
			if (snapshot_.completedTurns ==
				std::numeric_limits<std::uint32_t>::max())
				return CommandDisposition::Discard;
			snapshot_.currentTeam = value.nextTeam;
			if constexpr (
				std::is_same<Command, SynchronizeTurnCommand>::value)
				snapshot_.inCombat =
					snapshot_.inCombat || value.enterCombat;
			++snapshot_.completedTurns;
			return CommandDisposition::Applied;
		}
		else if constexpr (
			std::is_same<Command, BulkReloadWeaponsCommand>::value)
		{
			// Inventory stacks and world-item ammunition deliberately remain
			// outside the portable reference model.
			return CommandDisposition::Discard;
		}
		else if constexpr (
			std::is_same<Command, ApplyWeaponConfigurationCommand>::value)
		{
			// Inventory, attachment, UI and JA2 retaliation policy deliberately
			// remain outside the portable reference model.
			return CommandDisposition::Discard;
		}
		else
		{
			TacticalSimulationActorState* actor =
				findActor(value.soldier);
			if (!actor) return CommandDisposition::Discard;

			if constexpr (std::is_same<Command, ChangeStanceCommand>::value)
			{
				actor->stance = value.stance;
				return CommandDisposition::Applied;
			}
			else if constexpr (
				std::is_same<Command, MoveToGridCommand>::value)
			{
				actor->grid = value.destinationGrid;
				actor->stopped = false;
				return CommandDisposition::Applied;
			}
			else if constexpr (
				std::is_same<Command, SetFacingCommand>::value)
			{
				actor->direction = value.direction;
				return CommandDisposition::Applied;
			}
			else if constexpr (
				std::is_same<Command, BeginFireWeaponCommand>::value)
			{
				return recordShot(TacticalSimulationShot{
					value.soldier,
					value.targetGrid,
					value.targetLevel,
					value.targetCubeLevel,
					0,
					0,
					TacticalSimulationFireKind::CurrentSelection,
					tick,
					sequence})
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<
					Command,
					BeginSelectedFireWeaponCommand>::value)
			{
				return recordShot(TacticalSimulationShot{
					value.soldier,
					value.targetGrid,
					value.targetLevel,
					value.targetCubeLevel,
					value.attackingHand,
					value.attackingWeapon,
					TacticalSimulationFireKind::CapturedSelection,
					tick,
					sequence})
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, SynchronizeActorFireCommand>::value)
			{
				return recordShot(TacticalSimulationShot{
					value.soldier,
					value.targetGrid,
					value.targetLevel,
					value.targetCubeLevel,
					0,
					value.attackingWeapon,
					TacticalSimulationFireKind::ReplicatedSelection,
					tick,
					sequence})
					? CommandDisposition::Applied
					: CommandDisposition::Discard;
			}
			else if constexpr (
				std::is_same<Command, SynchronizeActorPathCommand>::value)
			{
				actor->grid = value.reportedGrid;
				actor->stopped = false;
				return CommandDisposition::Applied;
			}
			else if constexpr (
				std::is_same<Command, SetStealthModeCommand>::value)
			{
				actor->stealth = value.enabled;
				return CommandDisposition::Applied;
			}
			else if constexpr (
				std::is_same<Command, StopMovementCommand>::value)
			{
				actor->stopped = true;
				return CommandDisposition::Applied;
			}
			else if constexpr (
				std::is_same<Command, SynchronizeActorStopCommand>::value)
			{
				actor->grid = value.reportedGrid;
				actor->positionX = value.positionX;
				actor->positionY = value.positionY;
				actor->direction = value.direction;
				actor->stopped = value.stop;
				return CommandDisposition::Applied;
			}
			else
			{
				return CommandDisposition::Discard;
			}
		}
	}, command);
}
