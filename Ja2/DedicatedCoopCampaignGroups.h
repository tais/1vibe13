#ifndef JA2_DEDICATED_COOP_CAMPAIGN_GROUPS_H
#define JA2_DEDICATED_COOP_CAMPAIGN_GROUPS_H
#include "CoopCampaignGroups.h"

// Main-thread, read-only capture of friendly native movement groups. Returns
// nullptr on success, a static diagnostic on failure. Output is transactional;
// the caller publishes unavailable on failure, never a partial/stale roster.
const char* CaptureDedicatedCoopCampaignGroups(CoopSession::CoopCampaignGroups& output) noexcept;
#endif
