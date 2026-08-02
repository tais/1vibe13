#include "TacticalInventoryUiLegacy.h"

#include "TacticalActor.h"
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

bool ValidActorRole(TacticalInventoryActorRole role) noexcept
{
	return static_cast<std::uint8_t>(role) <
		static_cast<std::uint8_t>(TacticalInventoryActorRole::Count);
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
	TacticalEntityId actor) noexcept
{
	if (!ValidActorRole(role)) return false;
	if (!actor.valid() || !ResolveJa2TacticalEntity(actor)) return false;
	return BoundSession()->setActor(role, actor);
}

TacticalEntityId GetJa2TacticalInventoryActor(
	TacticalInventoryActorRole role) noexcept
{
	return BoundSession()->actor(role);
}

bool CopyJa2TacticalInventoryActor(
	TacticalInventoryActorRole source,
	TacticalInventoryActorRole destination) noexcept
{
	if (!ValidActorRole(source) || !ValidActorRole(destination))
		return false;
	const TacticalEntityId actor = GetJa2TacticalInventoryActor(source);
	if (!actor.valid())
	{
		ClearJa2TacticalInventoryActor(destination);
		return true;
	}
	if (!SetJa2TacticalInventoryActor(destination, actor))
	{
		ClearJa2TacticalInventoryActor(destination);
		return false;
	}
	return true;
}

TacticalActor* ResolveJa2TacticalInventoryActor(
	TacticalInventoryActorRole role) noexcept
{
	return ResolveJa2TacticalEntity(GetJa2TacticalInventoryActor(role));
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
	bool Set##Name(TacticalEntityId actor) noexcept \
	{ \
		return SetJa2TacticalInventoryActor(Role, actor); \
	} \
	void Clear##Name() noexcept \
	{ \
		ClearJa2TacticalInventoryActor(Role); \
	} \
	TacticalActor* Get##Name() noexcept \
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
