#ifndef JA2_CAMPAIGN_PACKAGE_H
#define JA2_CAMPAIGN_PACKAGE_H

#include "CampaignRuntimeBootstrap.h"
#include <Engine/Core/PackageApi.h>

class LegacyCampaignPackage final : public EnginePackage
{
public:
	LegacyCampaignPackage(GameCapabilities capabilities,
		CampaignRuntimeBootstrapHost& bootstrapHost);

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override;
	void deactivate() noexcept override;
	bool bootstrap(PackageBootstrapContext& context,
		PackageBootstrapPhase phase) override;
	void shutdown(PackageBootstrapContext& context,
		PackageBootstrapPhase phase) override;
	bool active() const { return active_; }
	const GameCapabilities& capabilities() const { return capabilities_; }

private:
	GameCapabilities capabilities_;
	CampaignRuntimeBootstrapHost& bootstrapHost_;
	PackageDescriptor descriptor_;
	bool active_ = false;
	bool runtimeStartAttempted_ = false;
	bool runtimeStarted_ = false;
};

LegacyCampaignPackage& GetCompiledCampaignPackage();

#endif
