#ifndef JA2_DEDICATED_COOP_CAMPAIGN_ECONOMY_H
#define JA2_DEDICATED_COOP_CAMPAIGN_ECONOMY_H

#include "CoopCampaignEconomy.h"

// Campaign-thread observations of the actual ledger and complete friendly
// roster, including mercenaries whose delayed arrival has not run yet. No
// local laptop state, dialogue or time control is invoked. Failure preserves
// output; the session publishes a fresh unavailable observation instead.
// Session/revision stamps remain the caller's responsibility.
// inSector preserves the native saved actor flag, including its historical
// value after TrashWorld; it does not prove that a tactical world is loaded.
const char* CaptureDedicatedCoopCampaignEconomy(
	CoopSession::CoopCampaignEconomy& output) noexcept;

#endif
