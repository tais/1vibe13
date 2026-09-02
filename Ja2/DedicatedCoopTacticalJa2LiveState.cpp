#include "DedicatedCoopTacticalHost.h"
#include "DedicatedCoopMissionPolicy.h"

#include "DedicatedServerOptions.h"
#include "GameContext.h"
#include "SoldierRepository.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "TacticalWorldAdapter.h"

#include "Simulation Commands.h"

#include "Animation Control.h"
#include "Assignments.h"
#include "Overhead.h"
#include "Soldier macros.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"

#include <limits>

extern BOOLEAN gfDedicatedServer;
extern bool is_client;
extern bool is_networked;
extern bool is_server;

static_assert(ANIM_STAND == 6 && ANIM_CROUCH == 3 && ANIM_PRONE == 1,
	"dedicated co-op stance translation no longer matches JA2");
static_assert(MAXTEAMS == 11,
	"dedicated co-op tactical team policy no longer matches JA2");

namespace
{
constexpr std::uint8_t InvalidTacticalTeam =
	std::numeric_limits<std::uint8_t>::max();

DedicatedCoopTacticalActorState InspectJa2Actor(
	TacticalEntityId actorId) noexcept
{
	DedicatedCoopTacticalActorState state;
	TacticalActor* actor = ResolveJa2TacticalEntity(actorId);
	if (!actor) return state;
	state.exactIdentity = true;
	state.active = actor->roster().active() != FALSE;
	state.inSector = actor->roster().inSector() != FALSE;
	state.playerTeam = actor->roster().team() == gbPlayerNum;
	const std::uint32_t flags = actor->status().flags();
	state.controllable = OK_CONTROLLABLE_MERC(actor) != FALSE &&
		DedicatedCoopEstablishedActorRoleEligible(
			actor->assignment().current() < ON_DUTY,
			(flags & SOLDIER_VEHICLE) != 0,
			(flags & SOLDIER_DRIVER) != 0,
			(flags & SOLDIER_PASSENGER) != 0);
	state.interruptActionEligible =
		IsJa2TacticalInterruptActorEligible(actorId);
	return state;
}

bool ProjectInterruptPhase(
	Ja2TacticalInterruptPhase source,
	TacticalInterruptPhase& destination) noexcept
{
	switch (source)
	{
		case Ja2TacticalInterruptPhase::None:
			destination = TacticalInterruptPhase::None;
			return true;
		case Ja2TacticalInterruptPhase::Resolving:
			destination = TacticalInterruptPhase::Resolving;
			return true;
		case Ja2TacticalInterruptPhase::Active:
			destination = TacticalInterruptPhase::Active;
			return true;
	}
	return false;
}
}

DedicatedCoopTacticalJa2LiveState::DedicatedCoopTacticalJa2LiveState(
	GameContext& game) noexcept
	: game_(&game), mainThread_(std::this_thread::get_id())
{
}

bool DedicatedCoopTacticalJa2LiveState::onMainThread() const noexcept
{
	return std::this_thread::get_id() == mainThread_;
}

bool DedicatedCoopTacticalJa2LiveState::dedicatedCoopActive() const noexcept
{
	const DedicatedServerOptions& options = GetDedicatedServerOptions();
	return gfDedicatedServer != FALSE && options.enabled &&
		options.mode == DedicatedServerMode::Coop;
}

bool DedicatedCoopTacticalJa2LiveState::campaignPackageActive(
	const std::string& packageId) const noexcept
{
	return game_ != nullptr && !packageId.empty() &&
		game_->packages().activeCampaign() == packageId &&
		game_->packages().isActive(packageId);
}

bool DedicatedCoopTacticalJa2LiveState::legacyNetworkingActive() const noexcept
{
	return is_networked || is_client || is_server;
}

bool DedicatedCoopTacticalJa2LiveState::captureTurn(
	DedicatedCoopTacticalTurnState& state) const noexcept
{
	if (!onMainThread()) return false;
	const TacticalWorldSession::Snapshot& world = CaptureJa2TacticalWorld();
	DedicatedCoopTacticalTurnState captured;
	captured.worldLoaded = world.loaded;
	captured.worldGeneration = world.worldGeneration;
	captured.turnSerial = world.turnSerial;
	captured.turnBased = world.turn.turnBased;
	captured.inCombat = world.turn.inCombat;
	captured.currentTeam = world.turn.currentTeam;
	captured.playerTeam = gbPlayerNum;
	captured.nextTeam = gbPlayerNum < MAXTEAMS - 1
		? static_cast<std::uint8_t>(gbPlayerNum + 1)
		: InvalidTacticalTeam;
	captured.pendingCombatActions = world.turn.pendingCombatActions;
	const Ja2TacticalInterruptProjection interrupt =
		CaptureJa2TacticalInterruptProjection();
	if (!ProjectInterruptPhase(interrupt.phase, captured.interruptPhase))
		return false;
	captured.interruptPending =
		captured.interruptPhase == TacticalInterruptPhase::Resolving;
	captured.interruptSerial = interrupt.serial;
	state = captured;
	return true;
}

bool DedicatedCoopTacticalJa2LiveState::captureActor(
	TacticalEntityId actor,
	DedicatedCoopTacticalActorState& state) const noexcept
{
	if (!onMainThread()) return false;
	state = InspectJa2Actor(actor);
	return true;
}

bool DedicatedCoopTacticalJa2LiveState::prepareAimedFirearmAttack(
	TacticalEntityId actor,
	TacticalEntityId target,
	std::uint8_t aimTime,
	AimedFirearmAttackCommand& command) const noexcept
{
	return onMainThread() && PrepareAimedFirearmAttackCommand(
		actor, target, aimTime, command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareReloadWeapon(
	TacticalEntityId actor,
	ReloadWeaponCommand& command) const noexcept
{
	return onMainThread() && PrepareReloadWeaponCommand(actor, command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareDoorOpenClose(
	TacticalEntityId actor,
	TacticalWorldObjectId object,
	bool desiredOpen,
	AuthoritativeDoorOpenCloseCommand& command) const noexcept
{
	return onMainThread() && PrepareAuthoritativeDoorOpenCloseCommand(
		actor, object, desiredOpen, command);
}

bool DedicatedCoopTacticalJa2LiveState::collectControllableActors(
	DedicatedCoopTacticalActorList& actors,
	std::size_t& count) const noexcept
{
	if (!onMainThread() || !IsJa2TacticalWorldLoaded()) return false;
	DedicatedCoopTacticalActorList captured{};
	std::size_t capturedCount = 0;
	const Ja2SoldierRepository& repository = GetJa2SoldierRepository();
	for (std::size_t slot = 0; slot < repository.capacity(); ++slot)
	{
		if (slot > std::numeric_limits<std::uint16_t>::max()) return false;
		const TacticalEntityId actor = GetJa2TacticalEntityId(
			static_cast<std::uint16_t>(slot));
		if (!actor.valid()) continue;
		const DedicatedCoopTacticalActorState state = InspectJa2Actor(actor);
		if (!state.exactIdentity || !state.active || !state.inSector ||
			!state.playerTeam || !state.controllable)
			continue;
		if (capturedCount == captured.size()) return false;
		captured[capturedCount++] = actor;
	}
	actors = captured;
	count = capturedCount;
	return true;
}
