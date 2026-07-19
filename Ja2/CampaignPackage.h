#ifndef JA2_CAMPAIGN_PACKAGE_H
#define JA2_CAMPAIGN_PACKAGE_H

#include "GameCapabilities.h"
#include <Engine/Core/PackageApi.h>

class LegacyCampaignPackage final : public EnginePackage
{
public:
	explicit LegacyCampaignPackage(GameCapabilities capabilities);

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() override;
	void deactivate() override;
	bool active() const { return active_; }
	const GameCapabilities& capabilities() const { return capabilities_; }

private:
	GameCapabilities capabilities_;
	PackageDescriptor descriptor_;
	bool active_ = false;
};

LegacyCampaignPackage& GetCompiledCampaignPackage();

#endif
