#ifndef JA2_CAMPAIGN_AIM_WILLINGNESS_H
#define JA2_CAMPAIGN_AIM_WILLINGNESS_H

#include "CampaignAimWillingnessPolicy.h"

// Fresh, read-only native facts for one AIM willingness decision. This neither
// stops/starts dialogue nor checks price, availability, capacity or hiring
// authorization. The caller still validates those separately before hiring.
// Invalid profile IDs leave output untouched and never access native profiles.
bool ReadCampaignAimWillingness(std::uint32_t profileId,
	CampaignAimWillingnessDecision& output) noexcept;

#endif
