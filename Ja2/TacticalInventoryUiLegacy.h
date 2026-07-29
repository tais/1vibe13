#ifndef JA2_TACTICAL_INVENTORY_UI_LEGACY_H
#define JA2_TACTICAL_INVENTORY_UI_LEGACY_H

#include "TacticalInventoryUiHost.h"

class SOLDIERTYPE;

// Compatibility consumers that still operate on JA2 soldier records resolve
// the stable UI-session identity only at the point of use. New producers store
// TacticalEntityId through TacticalInventoryUiHost.h.
SOLDIERTYPE* ResolveJa2TacticalInventoryActor(
	TacticalInventoryActorRole role) noexcept;

SOLDIERTYPE* GetSMCurrentMerc() noexcept;
SOLDIERTYPE* GetItemPointerSoldier() noexcept;
SOLDIERTYPE* GetItemDescSoldier() noexcept;
SOLDIERTYPE* GetAttachSoldier() noexcept;
SOLDIERTYPE* GetItemPopupSoldier() noexcept;
SOLDIERTYPE* GetItemPickupActor() noexcept;
SOLDIERTYPE* GetItemPickupOpponent() noexcept;

#endif
