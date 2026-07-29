#ifndef JA2_TACTICAL_INVENTORY_UI_HOST_H
#define JA2_TACTICAL_INVENTORY_UI_HOST_H

#include <Engine/Adapters/JA2/TacticalInventoryUiSession.h>

void BindJa2TacticalInventoryUiSession(
	TacticalInventoryUiSession& session) noexcept;
TacticalInventoryUiSession& GetJa2TacticalInventoryUiSession() noexcept;

bool SetJa2TacticalInventoryActor(
	TacticalInventoryActorRole role,
	TacticalEntityId actor) noexcept;
TacticalEntityId GetJa2TacticalInventoryActor(
	TacticalInventoryActorRole role) noexcept;
bool CopyJa2TacticalInventoryActor(
	TacticalInventoryActorRole source,
	TacticalInventoryActorRole destination) noexcept;
bool HasJa2TacticalInventoryActorContext(
	TacticalInventoryActorRole role) noexcept;
void ClearJa2TacticalInventoryActor(
	TacticalInventoryActorRole role) noexcept;
void ResetTacticalInventoryUiActorContexts() noexcept;

// Role-specific producer helpers keep mutation sites concise while retaining a
// complete generation-aware identity. Raw JA2 record resolution is isolated
// in TacticalInventoryUiLegacy.h.
bool SetSMCurrentMerc(TacticalEntityId actor) noexcept;
void ClearSMCurrentMerc() noexcept;
bool SetItemPointerSoldier(TacticalEntityId actor) noexcept;
void ClearItemPointerSoldier() noexcept;
bool SetItemDescSoldier(TacticalEntityId actor) noexcept;
void ClearItemDescSoldier() noexcept;
bool SetAttachSoldier(TacticalEntityId actor) noexcept;
void ClearAttachSoldier() noexcept;
bool SetItemPopupSoldier(TacticalEntityId actor) noexcept;
void ClearItemPopupSoldier() noexcept;
bool SetItemPickupActor(TacticalEntityId actor) noexcept;
void ClearItemPickupActor() noexcept;
bool SetItemPickupOpponent(TacticalEntityId actor) noexcept;
void ClearItemPickupOpponent() noexcept;

#endif
