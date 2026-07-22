#ifndef JA2_CAMPAIGN_PACKAGE_H
#define JA2_CAMPAIGN_PACKAGE_H

#include "GameCapabilities.h"
#include <Engine/Core/PackageApi.h>

// Campaign-specific legacy bootstrap remains behind this narrow adapter so the
// compiled package can participate in the engine lifecycle and headless tests
// without exposing JA2 globals through Engine/Core.
class LegacyCampaignBootstrapHooks
{
public:
	virtual ~LegacyCampaignBootstrapHooks() = default;
	virtual bool loadContent(const GameCapabilities& capabilities) = 0;
	virtual bool startRuntime(const GameCapabilities& capabilities) = 0;
};

class LegacyCampaignPackage final : public EnginePackage
{
public:
	explicit LegacyCampaignPackage(GameCapabilities capabilities);
	LegacyCampaignPackage(GameCapabilities capabilities,
		LegacyCampaignBootstrapHooks& bootstrapHooks);

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
	PackageDescriptor descriptor_;
	LegacyCampaignBootstrapHooks& bootstrapHooks_;
	bool active_ = false;
};

LegacyCampaignPackage& GetCompiledCampaignPackage();

#endif
