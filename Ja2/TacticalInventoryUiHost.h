#ifndef JA2_TACTICAL_INVENTORY_UI_HOST_H
#define JA2_TACTICAL_INVENTORY_UI_HOST_H

#include <Engine/Adapters/JA2/TacticalInventoryUiSession.h>

class SOLDIERTYPE;

void BindJa2TacticalInventoryUiSession(
	TacticalInventoryUiSession& session) noexcept;
TacticalInventoryUiSession& GetJa2TacticalInventoryUiSession() noexcept;

bool SetJa2TacticalInventoryActor(
	TacticalInventoryActorRole role,
	SOLDIERTYPE* soldier) noexcept;
SOLDIERTYPE* ResolveJa2TacticalInventoryActor(
	TacticalInventoryActorRole role) noexcept;
bool HasJa2TacticalInventoryActorContext(
	TacticalInventoryActorRole role) noexcept;
void ClearJa2TacticalInventoryActor(
	TacticalInventoryActorRole role) noexcept;
void ResetTacticalInventoryUiActorContexts() noexcept;

bool SetSMCurrentMerc(SOLDIERTYPE* soldier) noexcept;
SOLDIERTYPE* GetSMCurrentMerc() noexcept;
bool SetItemPointerSoldier(SOLDIERTYPE* soldier) noexcept;
SOLDIERTYPE* GetItemPointerSoldier() noexcept;
bool SetItemDescSoldier(SOLDIERTYPE* soldier) noexcept;
SOLDIERTYPE* GetItemDescSoldier() noexcept;
bool SetAttachSoldier(SOLDIERTYPE* soldier) noexcept;
SOLDIERTYPE* GetAttachSoldier() noexcept;
bool SetItemPopupSoldier(SOLDIERTYPE* soldier) noexcept;
SOLDIERTYPE* GetItemPopupSoldier() noexcept;
bool SetItemPickupActor(SOLDIERTYPE* soldier) noexcept;
SOLDIERTYPE* GetItemPickupActor() noexcept;
bool SetItemPickupOpponent(SOLDIERTYPE* soldier) noexcept;
SOLDIERTYPE* GetItemPickupOpponent() noexcept;

#endif
