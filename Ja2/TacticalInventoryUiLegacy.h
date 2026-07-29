#ifndef JA2_TACTICAL_INVENTORY_UI_LEGACY_H
#define JA2_TACTICAL_INVENTORY_UI_LEGACY_H

#include "TacticalInventoryUiHost.h"

class TacticalActor;

// Compatibility consumers that still operate on JA2 soldier records resolve
// the stable UI-session identity only at the point of use. New producers store
// TacticalEntityId through TacticalInventoryUiHost.h.
TacticalActor* ResolveJa2TacticalInventoryActor(
	TacticalInventoryActorRole role) noexcept;

TacticalActor* GetSMCurrentMerc() noexcept;
TacticalActor* GetItemPointerSoldier() noexcept;
TacticalActor* GetItemDescSoldier() noexcept;
TacticalActor* GetAttachSoldier() noexcept;
TacticalActor* GetItemPopupSoldier() noexcept;
TacticalActor* GetItemPickupActor() noexcept;
TacticalActor* GetItemPickupOpponent() noexcept;

#endif
