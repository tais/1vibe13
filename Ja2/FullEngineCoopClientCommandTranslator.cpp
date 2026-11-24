#include "FullEngineCoopClientCommandTranslator.h"

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <type_traits>
#include <utility>
#include <variant>

namespace
{
// Established JA2 animation-height values carried by ChangeStanceCommand.
// The dedicated host independently ratchets the same values at its boundary.
constexpr std::uint8_t Ja2StandingStance = 6;
constexpr std::uint8_t Ja2CrouchedStance = 3;
constexpr std::uint8_t Ja2ProneStance = 1;

bool ValidView(const FullEngineCoopClientControllerView& view) noexcept
{
	return view.snapshot != nullptr && view.snapshot->sector().loaded &&
		view.assignedActorCount <=
			CoopSession::MaximumCoopTacticalAssignedActors &&
		(view.assignedActorCount == 0 || view.assignedActors != nullptr);
}

bool Assigned(const FullEngineCoopClientControllerView& view,
	TacticalEntityId actor) noexcept
{
	if (!actor.valid() || !ValidView(view)) return false;
	for (std::size_t index = 0; index < view.assignedActorCount; ++index)
		if (view.assignedActors[index] == actor) return true;
	return false;
}

bool ActionsEnabled(const FullEngineCoopClientControllerView& view,
	TacticalEntityId actorId) noexcept
{
	if (!Assigned(view, actorId) || view.resynchronizing ||
		view.outstandingCommandId != 0)
		return false;
	const TacticalActorSnapshot* const actor = view.snapshot->find(actorId);
	if (actor == nullptr || !actor->active || !actor->inSector ||
		actor->life <= 0)
		return false;
	const TacticalTurnSnapshot& turn = view.snapshot->turn();
	if (turn.commandsBlocked) return false;
	switch (turn.interruptPhase)
	{
		case TacticalInterruptPhase::None:
			return !turn.turnBased || !turn.inCombat ||
				turn.activeTeam == actor->team;
		case TacticalInterruptPhase::Resolving:
			return false;
		case TacticalInterruptPhase::Active:
			return turn.turnBased && turn.inCombat &&
				turn.interruptSerial != 0 &&
				actor->interruptActionEligible;
	}
	return false;
}

template <typename Command>
bool SelectedActorCommand(const Command& command,
	TacticalEntityId selectedActor) noexcept
{
	return command.soldier == selectedActor;
}

FullEngineCoopClientIntentRequest Request(
	TacticalEntityId actor,
	CoopSession::TacticalIntentPayload payload) noexcept
{
	FullEngineCoopClientIntentRequest request;
	request.actor = actor;
	request.payload = std::move(payload);
	request.valid = true;
	return request;
}
}

FullEngineCoopClientCommandTranslationResult
TranslateFullEngineCoopClientUiCommand(
	const SimulationCommand& command,
	const FullEngineCoopClientControllerView& view,
	TacticalEntityId selectedActor,
	FullEngineCoopClientIntentRequest& output) noexcept
{
	using Result = FullEngineCoopClientCommandTranslationResult;
	if (!ValidView(view) || !selectedActor.valid() ||
		!Assigned(view, selectedActor) ||
		view.snapshot->find(selectedActor) == nullptr)
		return Result::InvalidView;
	if (!IsStructurallyValidSimulationCommand(command))
		return Result::InvalidCommand;
	if (!std::visit([](const auto& value) noexcept {
			return value.source == SimulationCommandSource::LocalPlayer;
		}, command))
		return Result::WrongSource;
	if (!ActionsEnabled(view, selectedActor))
		return Result::ActionsDisabled;

	FullEngineCoopClientIntentRequest translated;
	Result result = Result::Unsupported;
	std::visit([&](const auto& value) noexcept {
		using Command = typename std::decay<decltype(value)>::type;
		if constexpr (std::is_same<Command, MoveToGridCommand>::value)
		{
			if (!SelectedActorCommand(value, selectedActor))
			{
				result = Result::ActorNotSelected;
				return;
			}
			const TacticalActorSnapshot* const actor =
				view.snapshot->find(selectedActor);
			const TacticalTurnSnapshot& turn = view.snapshot->turn();
			if (value.authority != TacticalCommandAuthorityPolicy::Legacy ||
				value.origin != TacticalMoveOrigin::PlayerUi ||
				value.pendingAction != TacticalPendingActionPolicy::Clear ||
				!view.snapshot->dimensions().contains(value.destinationGrid) ||
				(turn.turnBased && turn.inCombat &&
				 (actor == nullptr || actor->actionPoints <= 0)))
			{
				result = Result::InvalidCommand;
				return;
			}
			translated = Request(selectedActor,
				CoopSession::MoveTacticalIntent{value.destinationGrid,
					value.movementMode, value.reverse});
			result = Result::Success;
		}
		else if constexpr (std::is_same<Command, SetFacingCommand>::value)
		{
			if (!SelectedActorCommand(value, selectedActor))
			{
				result = Result::ActorNotSelected;
				return;
			}
			if (value.eventPolicy != TacticalEventPolicy::Replicated ||
				value.authority != TacticalCommandAuthorityPolicy::Legacy)
			{
				result = Result::InvalidCommand;
				return;
			}
			translated = Request(selectedActor,
				CoopSession::FaceTacticalIntent{value.direction});
			result = Result::Success;
		}
		else if constexpr (std::is_same<Command, ChangeStanceCommand>::value)
		{
			if (!SelectedActorCommand(value, selectedActor))
			{
				result = Result::ActorNotSelected;
				return;
			}
			if (value.eventPolicy != TacticalEventPolicy::Replicated ||
				value.authority != TacticalCommandAuthorityPolicy::Legacy)
			{
				result = Result::InvalidCommand;
				return;
			}
			CoopSession::TacticalIntentStance stance;
			switch (value.stance)
			{
				case Ja2StandingStance:
					stance = CoopSession::TacticalIntentStance::Standing;
					break;
				case Ja2CrouchedStance:
					stance = CoopSession::TacticalIntentStance::Crouched;
					break;
				case Ja2ProneStance:
					stance = CoopSession::TacticalIntentStance::Prone;
					break;
				default:
					result = Result::InvalidCommand;
					return;
			}
			translated = Request(selectedActor,
				CoopSession::StanceTacticalIntent{stance});
			result = Result::Success;
		}
		else if constexpr (std::is_same<Command, StopMovementCommand>::value)
		{
			if (!SelectedActorCommand(value, selectedActor))
			{
				result = Result::ActorNotSelected;
				return;
			}
			if (value.authority != TacticalCommandAuthorityPolicy::Legacy)
			{
				result = Result::InvalidCommand;
				return;
			}
			translated = Request(selectedActor,
				CoopSession::StopTacticalIntent{});
			result = Result::Success;
		}
		else if constexpr (std::is_same<Command, ReloadWeaponCommand>::value)
		{
			if (!SelectedActorCommand(value, selectedActor))
			{
				result = Result::ActorNotSelected;
				return;
			}
			if (value.authority != TacticalCommandAuthorityPolicy::Legacy)
			{
				result = Result::InvalidCommand;
				return;
			}
			// The authority deliberately selects and revalidates weapon/ammo;
			// reloadEvenIfNotEmpty is a local UI preference, not trusted input.
			translated = Request(selectedActor,
				CoopSession::ReloadTacticalIntent{});
			result = Result::Success;
		}
		else if constexpr (std::is_same<Command, EndTurnCommand>::value)
		{
			const TacticalTurnSnapshot& turn = view.snapshot->turn();
			const TacticalActorSnapshot* const actor =
				view.snapshot->find(selectedActor);
			if (!turn.turnBased || !turn.inCombat || actor == nullptr ||
				actor->team == static_cast<std::uint8_t>(-1) ||
				value.nextTeam != static_cast<std::uint8_t>(actor->team + 1) ||
				value.authority != TacticalCommandAuthorityPolicy::Legacy)
			{
				result = Result::InvalidCommand;
				return;
			}
			if (turn.interruptPhase == TacticalInterruptPhase::Active)
				translated = Request(selectedActor,
					CoopSession::PassInterruptTacticalIntent{
						turn.interruptSerial});
			else
				translated = Request(selectedActor,
					CoopSession::EndTurnTacticalIntent{});
			result = Result::Success;
		}
	}, command);
	if (result == Result::Success) output = std::move(translated);
	return result;
}
