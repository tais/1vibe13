#include "DedicatedCoopAttack.h"
#include "DedicatedCoopRuntime.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "Items.h"
#include "connect.h"

namespace
{
DedicatedCoopAttackState* bound = nullptr;
bool NativeWeapon(const TacticalActor& actor, std::uint16_t& item,
	std::uint16_t& ammunition) noexcept
{
	const auto& hand = actor.inventory()[HANDPOS];
	if (!hand.exists() || hand.ubNumberOfObjects != 1 || hand.usItem >= MAXITEMS ||
		Item[hand.usItem].usItemClass != IC_GUN) return false;
	item = hand.usItem;
	ammunition = hand[0]->data.gun.ubGunShotsLeft;
	return true;
}
}

DedicatedCoopAttackState::~DedicatedCoopAttackState()
{ UnbindDedicatedCoopAttackState(*this); }
void DedicatedCoopAttackState::reset() noexcept
{
	occupied_ = false; sequence_ = world_ = 0; actor_ = {};
	handItem_ = ammunition_ = 0; roundCommitted_ = false;
	outcome_ = DedicatedCoopAttackOutcome::Untracked; failure_ = nullptr;
}
bool BindDedicatedCoopAttackState(DedicatedCoopAttackState& state) noexcept
{
	if (bound && bound != &state) return false;
	bound = &state;
	return true;
}
void UnbindDedicatedCoopAttackState(DedicatedCoopAttackState& state) noexcept
{ if (bound == &state) bound = nullptr; }
bool DedicatedCoopAttackPending() noexcept
{
	return bound && (bound->failure_ || (bound->occupied_ &&
		bound->outcome_ == DedicatedCoopAttackOutcome::Pending));
}

bool BeginDedicatedCoopAttack(std::uint64_t sequence, TacticalActor& actor) noexcept
{
	// Unbound seams leave ordinary single-player and legacy multiplayer alone.
	if (!bound) return true;
	if (bound->failure_ || bound->occupied_ || !IsDedicatedCoopProcess() ||
		is_networked || is_client || is_server) return false;
	const auto& world = CaptureJa2TacticalWorld();
	const auto id = GetJa2TacticalEntityId(actor);
	std::uint16_t item = 0, ammunition = 0;
	if (!world.loaded || !world.worldGeneration || world.turn.pendingCombatActions != 0 || !id.valid() ||
		ResolveJa2TacticalEntity(id) != &actor || !actor.roster().active() ||
		!actor.roster().inSector() || !NativeWeapon(actor, item, ammunition) ||
		!ammunition) return false;
	bound->occupied_ = true; bound->sequence_ = sequence;
	bound->world_ = world.worldGeneration; bound->actor_ = id;
	bound->handItem_ = item; bound->ammunition_ = ammunition;
	bound->outcome_ = DedicatedCoopAttackOutcome::Pending;
	return true;
}

void AbandonDedicatedCoopAttack(std::uint64_t sequence) noexcept
{
	if (!bound || !bound->occupied_ || bound->sequence_ != sequence) return;
	// This is only the synchronous pre-fire HandleItem refusal. A native
	// completion already observed here is evidence that execution did mutate.
	if (bound->outcome_ != DedicatedCoopAttackOutcome::Pending)
	{ bound->failure_ = "refused native attack already completed"; return; }
	TacticalActor* actor = nullptr;
	if (!ResolveDedicatedCoopAttackOwner(actor) || !actor) return;
	std::uint16_t item = 0, ammunition = 0;
	if (!NativeWeapon(*actor, item, ammunition) || item != bound->handItem_ ||
		ammunition != bound->ammunition_ || bound->roundCommitted_ || GetJa2PendingTacticalCombatActions() != 0 ||
		actor->animationActivity().turningToShoot())
	{ bound->failure_ = "refused native attack retained effects"; return; }
	bound->reset();
}

bool ResolveDedicatedCoopAttackOwner(TacticalActor*& actor) noexcept
{
	actor = nullptr;
	if (!bound || !bound->occupied_ ||
		bound->outcome_ != DedicatedCoopAttackOutcome::Pending) return false;
	if (bound->failure_) return true;
	const auto& world = CaptureJa2TacticalWorld();
	if (!world.loaded || world.worldGeneration != bound->world_ ||
		!(actor = ResolveJa2TacticalEntity(bound->actor_)))
	{
		actor = nullptr;
		bound->failure_ = "native attack owner left its exact world or incarnation";
	}
	return true;
}

void CompleteDedicatedCoopAttack(TacticalActor& actor) noexcept
{
	if (!bound || !bound->occupied_ || bound->failure_ ||
		bound->outcome_ != DedicatedCoopAttackOutcome::Pending) return;
	TacticalActor* exact = nullptr;
	if (!ResolveDedicatedCoopAttackOwner(exact) || exact != &actor)
	{ bound->failure_ = "native attack completed for a different actor"; return; }
	if (GetJa2PendingTacticalCombatActions() != 0) return;
	std::uint16_t item = 0, ammunition = 0;
	if (!NativeWeapon(actor, item, ammunition) || item != bound->handItem_ ||
		ammunition > bound->ammunition_ ||
		(ammunition < bound->ammunition_ && !bound->roundCommitted_))
	{ bound->failure_ = "native attack weapon changed before completion"; return; }
	bound->outcome_ = bound->roundCommitted_
		? DedicatedCoopAttackOutcome::Completed : DedicatedCoopAttackOutcome::Interrupted;
}

void RecordDedicatedCoopAttackAmmunitionUse(TacticalActor& actor, std::uint16_t item) noexcept
{
	if (!bound || !bound->occupied_ || bound->outcome_ != DedicatedCoopAttackOutcome::Pending ||
		GetJa2TacticalEntityId(actor) != bound->actor_) return;
	TacticalActor* exact = nullptr;
	if (!ResolveDedicatedCoopAttackOwner(exact) || exact != &actor || item >= MAXITEMS || Item[item].usItemClass != IC_GUN)
	{ bound->failure_ = "native attack discharged a different weapon or actor"; return; }
	bound->roundCommitted_ = true;
}

DedicatedCoopAttackOutcome CaptureDedicatedCoopAttackOutcome(
	std::uint64_t sequence, std::uint64_t world, TacticalEntityId actor) noexcept
{
	if (!bound) return DedicatedCoopAttackOutcome::Untracked;
	if (bound->failure_ || !bound->occupied_ || bound->sequence_ != sequence ||
		bound->world_ != world || bound->actor_ != actor)
		return DedicatedCoopAttackOutcome::Invalid;
	return bound->outcome_;
}
bool ReleaseDedicatedCoopAttack(
	std::uint64_t sequence, std::uint64_t world, TacticalEntityId actor) noexcept
{
	if (!bound) return true;
	const auto outcome = CaptureDedicatedCoopAttackOutcome(sequence, world, actor);
	if (outcome != DedicatedCoopAttackOutcome::Completed &&
		outcome != DedicatedCoopAttackOutcome::Interrupted) return false;
	bound->reset();
	return true;
}
