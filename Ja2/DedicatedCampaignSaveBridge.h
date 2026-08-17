#ifndef JA2_DEDICATED_CAMPAIGN_SAVE_BRIDGE_H
#define JA2_DEDICATED_CAMPAIGN_SAVE_BRIDGE_H

#include "DedicatedCampaignStore.h"

// Fixed VFS-logical scratch names keep the legacy serializer away from native
// campaign-store paths. They contain no directory separators and map only to
// the two reserved legacy end-turn slot identities (6 and 7).
const char* DedicatedCampaignLogicalScratch(
	DedicatedCampaignSlot slot) noexcept;
bool IsDedicatedCampaignPersistenceRequested() noexcept;

// Implemented by SaveLoadGame.cpp. These are deliberately narrower than an
// arbitrary-path save API: callers can select only campaign slot A or B.
bool SaveDedicatedCampaignGame(DedicatedCampaignSlot slot) noexcept;
bool ValidateDedicatedCampaignGame(DedicatedCampaignSlot slot) noexcept;
bool LoadDedicatedCampaignGame(DedicatedCampaignSlot slot) noexcept;

#endif
