#include "DedicatedCoopConversation.h"
#include "DedicatedCoopMoveDiagnostic.h"
#include "DedicatedCoopInventoryDiagnostic.h"
#include "DedicatedCoopAttack.h"
#include "DedicatedCoopTacticalHost.h"
#include "DedicatedCoopMissionPolicy.h"
#include "DedicatedCoopSurrender.h"
#include "DedicatedCoopBattleNotice.h"
#include "DedicatedCoopMeanwhile.h"

#include "DedicatedServerOptions.h"
#include "GameContext.h"
#include "SoldierRepository.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "TacticalWorldAdapter.h"

#include "Simulation Commands.h"

#include "Animation Control.h"
#include "Assignments.h"
#include "PATHAI.H"
#include "Points.h"
#include "Overhead.h"
#include "Soldier macros.h"
#include "TacticalActor.h"
#include "TacticalActorMobility.h"
#include "TacticalActorRouteExecution.h"
#include "TacticalActorStateFlags.h"

#include <limits>
#include <algorithm>

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
	captured.sharedDecisionPending = DedicatedCoopSurrenderPending() || DedicatedCoopBattleNoticePending() || DedicatedCoopMeanwhilePending() || DedicatedCoopAttackPending();
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

bool DedicatedCoopTacticalJa2LiveState::canBeginMoveToGrid(
	TacticalEntityId actorId,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse) const noexcept
{
	TacticalMoveDiagnostic diagnostic;
	diagnostic.actorSlot = actorId.slot; diagnostic.incarnation = actorId.incarnation;
	diagnostic.destination = destinationGrid; diagnostic.movementMode = movementMode; diagnostic.reverse = reverse;
	if (!onMainThread())
	{
		diagnostic.reason = TacticalMoveFailure::WrongThread;
		TraceDedicatedCoopMoveRejection("admission", diagnostic);
		return false;
	}
	TacticalActor* actor = ResolveJa2TacticalEntity(actorId);
	if (!actor)
	{
		diagnostic.reason = TacticalMoveFailure::ActorUnavailable;
		TraceDedicatedCoopMoveRejection("admission", diagnostic);
		return false;
	}
	const bool accepted = TacticalActorRouteExecution::canBeginMoveToGrid(
		*actor, destinationGrid, movementMode, reverse, &diagnostic);
	if (!accepted) TraceDedicatedCoopMoveRejection("admission", diagnostic);
	return accepted;
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

bool DedicatedCoopTacticalJa2LiveState::prepareBeginFirstAid(
	TacticalEntityId actor, TacticalEntityId target,
	BeginFirstAidCommand& command) const noexcept
{
	return onMainThread() && PrepareBeginFirstAidCommand(actor, target, command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareTalk(const CoopSession::PeerIdentity& peer,
	TacticalEntityId actor, TacticalEntityId target,
	StartConversationCommand& command) noexcept
{
	if (!onMainThread()) return false;
	const DedicatedCoopTalkCode code = PrepareDedicatedCoopTalk(actor, target, command);
	if (code != DedicatedCoopTalkCode::Prepared)
	{
		// Opt-in (JA2_COOP_INVENTORY_DIAGNOSTIC=1): which talk rule refused.
		if (DedicatedCoopInventoryDiagnosticEnabled())
			(void)std::fprintf(stderr, "[coop-inventory] stage=talk-%u actor=%u:%u target=%u:%u\n",
				unsigned(code), unsigned(actor.slot), unsigned(actor.incarnation),
				unsigned(target.slot), unsigned(target.incarnation));
		return false;
	}
	NoteDedicatedCoopConversationRequest(peer, actor, target);
	return true;
}

bool DedicatedCoopTacticalJa2LiveState::prepareConverse(const CoopSession::PeerIdentity& peer,
	TacticalEntityId actor, std::uint32_t conversation, std::uint8_t approach,
	std::uint64_t worldGeneration, AuthoritativeConverseCommand& command) const noexcept
{
	// Only the exact published conversation's merc answers, and only for its
	// initiator (any owner of that merc for a natively opened panel; ownership
	// of the actor was already checked by the intent authority).
	// A scripted NPC-to-NPC scene may be closed (only) by any player, with one
	// of their own mercs as the commanding actor.
	const auto current = CurrentDedicatedCoopConversation();
	if (!onMainThread() || !current.conversation || conversation != current.conversation || worldGeneration == 0)
		return false;
	if (current.npcSource ? approach != 0 :
		(actor != current.merc || (!CoopSession::IsZero(current.initiator) && current.initiator != peer)))
		return false;
	command = AuthoritativeConverseCommand{};
	command.soldier = actor;
	command.expectedWorldGeneration = worldGeneration;
	command.npcProfile = current.npcProfile;
	command.sourceProfile = current.mercProfile;
	command.approach = approach;
	return IsStructurallyValidAuthoritativeConverseCommand(command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareThrow(
	TacticalEntityId actor,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	AuthoritativeThrowCommand& command) const noexcept
{
	return onMainThread() && PrepareAuthoritativeThrowCommand(actor, targetGrid, targetLevel, command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareAttachItem(
	TacticalEntityId actor,
	std::uint8_t sourceSlot,
	std::uint8_t targetSlot,
	AuthoritativeAttachItemCommand& command) const noexcept
{
	return onMainThread() && PrepareAuthoritativeAttachItemCommand(actor, sourceSlot, targetSlot, command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareStealth(
	TacticalEntityId actor,
	bool expectedStealth,
	AuthoritativeStealthCommand& command) const noexcept
{
	return onMainThread() && PrepareAuthoritativeStealthCommand(actor, expectedStealth, command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareUseItem(
	TacticalEntityId actor,
	std::uint8_t slot,
	AuthoritativeUseItemCommand& command) const noexcept
{
	return onMainThread() && PrepareAuthoritativeUseItemCommand(actor, slot, command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareMeleeAttack(
	TacticalEntityId actor,
	TacticalEntityId target,
	AuthoritativeMeleeAttackCommand& command) const noexcept
{
	return onMainThread() && PrepareAuthoritativeMeleeAttackCommand(actor, target, command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareTraverse(
	TacticalEntityId actor,
	bool window,
	AuthoritativeTraverseCommand& command) const noexcept
{
	return onMainThread() && PrepareAuthoritativeTraverseCommand(actor, window, command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareWeaponMode(
	TacticalEntityId actor,
	std::uint8_t expectedWeaponMode,
	AuthoritativeWeaponModeCommand& command) const noexcept
{
	return onMainThread() && PrepareAuthoritativeWeaponModeCommand(actor, expectedWeaponMode, command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareDoorAction(
	TacticalEntityId actor,
	TacticalWorldObjectId object,
	std::uint8_t action,
	AuthoritativeDoorActionCommand& command) const noexcept
{
	return onMainThread() && PrepareAuthoritativeDoorActionCommand(actor, object, action, command);
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

bool DedicatedCoopTacticalJa2LiveState::captureInventory(TacticalEntityId actor,
	std::uint64_t worldGeneration, CoopSession::CoopOwnerInventorySnapshot& inventory) const noexcept
{
	return onMainThread() && inventoryAuthority_.capture(actor, worldGeneration, inventory);
}

bool DedicatedCoopTacticalJa2LiveState::prepareSwapInventorySlots(
	TacticalEntityId actor, std::uint16_t sourceSlot, std::uint16_t destinationSlot,
	std::uint64_t expectedInventoryRevision, SwapInventorySlotsCommand& command) const noexcept
{
	DedicatedCoopInventoryDiagnostic diagnostic;
	diagnostic.actorSlot = actor.slot; diagnostic.incarnation = actor.incarnation;
	diagnostic.sourceSlot = sourceSlot; diagnostic.destinationSlot = destinationSlot;
	diagnostic.expected = expectedInventoryRevision;
	const auto reject = [&](const char* stage) {
		TraceDedicatedCoopInventoryRejection(stage, diagnostic);
		return false;
	};
	if (!onMainThread()) return reject("admission.thread");
	if (sourceSlot >= 256 || destinationSlot >= 256 || expectedInventoryRevision == 0)
		return reject("admission.input");
	CoopSession::CoopOwnerInventorySnapshot current;
	const auto world = GetJa2TacticalWorldAdapter().liveTurnIdentity();
	if (!world) return reject("admission.world");
	diagnostic.world = world.worldGeneration; diagnostic.turn = world.serial;
	if (!inventoryAuthority_.capture(actor, world.worldGeneration, current))
		return reject("admission.capture");
	diagnostic.observed = current.inventoryRevision;
	if (current.inventoryRevision != expectedInventoryRevision)
		return reject("admission.revision");
	if (!PrepareSwapInventorySlotsCommand(actor, static_cast<std::uint8_t>(sourceSlot),
		static_cast<std::uint8_t>(destinationSlot), command))
		return reject("admission.native");
	return true;
}

bool DedicatedCoopTacticalJa2LiveState::prepareTransferInventoryItem(
	TacticalEntityId actor, TacticalEntityId target,
	std::uint16_t sourceSlot, std::uint16_t destinationSlot,
	std::uint64_t expectedInventoryRevision,
	std::uint64_t expectedTargetInventoryRevision,
	TransferInventoryItemCommand& command) const noexcept
{
	if (!onMainThread() || actor == target || sourceSlot >= 256 || destinationSlot >= 256 ||
		expectedInventoryRevision == 0 || expectedTargetInventoryRevision == 0) return false;
	CoopSession::CoopOwnerInventorySnapshot sourceInventory;
	CoopSession::CoopOwnerInventorySnapshot targetInventory;
	const auto world = GetJa2TacticalWorldAdapter().liveTurnIdentity();
	return world && inventoryAuthority_.capture(actor, world.worldGeneration, sourceInventory) &&
		sourceInventory.inventoryRevision == expectedInventoryRevision &&
		inventoryAuthority_.capture(target, world.worldGeneration, targetInventory) &&
		targetInventory.inventoryRevision == expectedTargetInventoryRevision &&
		PrepareTransferInventoryItemCommand(actor, target, static_cast<std::uint8_t>(sourceSlot),
			static_cast<std::uint8_t>(destinationSlot), command);
}

bool DedicatedCoopTacticalJa2LiveState::preparePickupWorldItem(
	TacticalEntityId actor, TacticalWorldItemId item, std::uint16_t destinationSlot,
	std::uint64_t expectedInventoryRevision, AuthoritativePickupWorldItemCommand& command) const noexcept
{
	if (!onMainThread() || !item.valid() || destinationSlot >= 256 ||
		expectedInventoryRevision == 0) return false;
	CoopSession::CoopOwnerInventorySnapshot current;
	const auto world = GetJa2TacticalWorldAdapter().liveTurnIdentity();
	return world && inventoryAuthority_.capture(actor, world.worldGeneration, current) &&
		current.inventoryRevision == expectedInventoryRevision &&
		std::any_of(current.groundItems.begin(), current.groundItems.end(),
			[item](const auto& entry) { return entry.id == item; }) &&
		PrepareAuthoritativePickupWorldItemCommand(actor, item,
			static_cast<std::uint8_t>(destinationSlot), command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareDropInventoryItem(
	TacticalEntityId actor, std::uint16_t sourceSlot,
	std::uint64_t expectedInventoryRevision, AuthoritativeDropInventoryItemCommand& command) const noexcept
{
	if (!onMainThread() || sourceSlot < 14 || sourceSlot >= 55 ||
		expectedInventoryRevision == 0) return false;
	CoopSession::CoopOwnerInventorySnapshot current;
	const auto world = GetJa2TacticalWorldAdapter().liveTurnIdentity();
	return world && inventoryAuthority_.capture(actor, world.worldGeneration, current) &&
		current.inventoryRevision == expectedInventoryRevision &&
		PrepareAuthoritativeDropInventoryItemCommand(actor, static_cast<std::uint8_t>(sourceSlot), command);
}

bool DedicatedCoopTacticalJa2LiveState::prepareGiveItem(
	TacticalEntityId actor, TacticalEntityId target, std::uint16_t sourceSlot,
	std::uint64_t expectedInventoryRevision, AuthoritativeGiveItemCommand& command) const noexcept
{
	if (!onMainThread() || sourceSlot >= 256 ||
		!IsSupportedAuthoritativeGiveSlot(static_cast<std::uint8_t>(sourceSlot)) ||
		expectedInventoryRevision == 0) return false;
	CoopSession::CoopOwnerInventorySnapshot current;
	const auto world = GetJa2TacticalWorldAdapter().liveTurnIdentity();
	return world && inventoryAuthority_.capture(actor, world.worldGeneration, current) &&
		current.inventoryRevision == expectedInventoryRevision &&
		PrepareAuthoritativeGiveItemCommand(actor, target, static_cast<std::uint8_t>(sourceSlot), command);
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

DedicatedCoopAttackOutcome DedicatedCoopTacticalJa2LiveState::captureAttackOutcome(
	std::uint64_t sequence, std::uint64_t world, TacticalEntityId actor) const noexcept
{ return CaptureDedicatedCoopAttackOutcome(sequence, world, actor); }
void DedicatedCoopTacticalJa2LiveState::releaseAttackOutcome(
	std::uint64_t sequence, std::uint64_t world, TacticalEntityId actor) noexcept
{ (void)ReleaseDedicatedCoopAttack(sequence, world, actor); }
