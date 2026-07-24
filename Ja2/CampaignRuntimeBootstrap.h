#ifndef JA2_CAMPAIGN_RUNTIME_BOOTSTRAP_H
#define JA2_CAMPAIGN_RUNTIME_BOOTSTRAP_H

#include "GameCapabilities.h"

// Application port for process-lifetime campaign startup that has not yet
// become data-owned. The selected campaign package owns one-shot admission.
class CampaignRuntimeBootstrapHost
{
public:
	virtual ~CampaignRuntimeBootstrapHost() = default;
	virtual bool startCampaignRuntime(const GameCapabilities& capabilities) = 0;
};

CampaignRuntimeBootstrapHost& GetCompiledCampaignRuntimeBootstrapHost();

#endif
