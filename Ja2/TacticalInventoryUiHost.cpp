#include "TacticalInventoryUiHost.h"

#include "Soldier Control.h"
#include "TacticalEntityHost.h"

namespace
{
TacticalInventoryUiSession& StandaloneSession() noexcept
{
	static TacticalInventoryUiSession session;
	return session;
}

TacticalInventoryUiSession*& BoundSession() noexcept
{
	static TacticalInventoryUiSession* session = &StandaloneSession();
	return session;
}
}

void BindJa2TacticalInventoryUiSession(
	TacticalInventoryUiSession& session) noexcept
{
	if (BoundSession() != &session)
		session = *BoundSession();
	BoundSession() = &session;
}

TacticalInventoryUiSession& GetJa2TacticalInventoryUiSession() noexcept
{
	return *BoundSession();
}

bool SetJa2TacticalInventoryActor(
	TacticalInventoryActorRole role,
	SOLDIERTYPE* soldier) noexcept
{
	if (static_cast<std::uint8_t>(role) >=
		static_cast<std::uint8_t>(TacticalInventoryActorRole::Count))
		return false;
	if (!soldier)
	{
		BoundSession()->clearActor(role);
		return true;
	}
	const TacticalEntityId actor = GetJa2TacticalEntityId(
		static_cast<std::uint16_t>(soldier->identity().id()));
	if (!actor.valid() || ResolveJa2TacticalEntity(actor) != soldier)
		return false;
	return BoundSession()->setActor(role, actor);
}

SOLDIERTYPE* ResolveJa2TacticalInventoryActor(
	TacticalInventoryActorRole role) noexcept
{
	return ResolveJa2TacticalEntity(BoundSession()->actor(role));
}

bool HasJa2TacticalInventoryActorContext(
	TacticalInventoryActorRole role) noexcept
{
	return BoundSession()->hasActor(role);
}

void ClearJa2TacticalInventoryActor(
	TacticalInventoryActorRole role) noexcept
{
	BoundSession()->clearActor(role);
}

void ResetTacticalInventoryUiActorContexts() noexcept
{
	BoundSession()->reset();
}

#define JA2_INVENTORY_ACTOR_ACCESSORS(Name, Role) \
	bool Set##Name(SOLDIERTYPE* soldier) noexcept \
	{ \
		return SetJa2TacticalInventoryActor(Role, soldier); \
	} \
	SOLDIERTYPE* Get##Name() noexcept \
	{ \
		return ResolveJa2TacticalInventoryActor(Role); \
	}

JA2_INVENTORY_ACTOR_ACCESSORS(
	SMCurrentMerc, TacticalInventoryActorRole::SelectedMerc)
JA2_INVENTORY_ACTOR_ACCESSORS(
	ItemPointerSoldier, TacticalInventoryActorRole::ItemCursorOwner)
JA2_INVENTORY_ACTOR_ACCESSORS(
	ItemDescSoldier, TacticalInventoryActorRole::ItemDescriptionOwner)
JA2_INVENTORY_ACTOR_ACCESSORS(
	AttachSoldier, TacticalInventoryActorRole::AttachmentOwner)
JA2_INVENTORY_ACTOR_ACCESSORS(
	ItemPopupSoldier, TacticalInventoryActorRole::ItemPopupOwner)
JA2_INVENTORY_ACTOR_ACCESSORS(
	ItemPickupActor, TacticalInventoryActorRole::PickupActor)
JA2_INVENTORY_ACTOR_ACCESSORS(
	ItemPickupOpponent, TacticalInventoryActorRole::PickupOpponent)

#undef JA2_INVENTORY_ACTOR_ACCESSORS
