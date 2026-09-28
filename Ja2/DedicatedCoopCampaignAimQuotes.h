#ifndef JA2_DEDICATED_COOP_CAMPAIGN_AIM_QUOTES_H
#define JA2_DEDICATED_COOP_CAMPAIGN_AIM_QUOTES_H

#include "CoopCampaignEconomy.h"

// Read-only offers for the server's current profile inventory and three AIM
// contract durations. Requires the freshly observed, stamped economy; rejects
// a changed roster/balance instead of pairing prices with stale funds. No kit
// selection, dialogue, profile equipment copying or hiring takes place here.
// Output remains untouched on failure. The quotes ledger owns its stamps.
const char* CaptureDedicatedCoopCampaignAimQuotes(
	const CoopSession::CoopCampaignEconomy& economy,
	CoopSession::CoopCampaignAimQuotes& output) noexcept;

#endif
