#ifndef JA2_CAMPAIGN_PACKAGE_H
#define JA2_CAMPAIGN_PACKAGE_H

#include <exception>

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

// Process-lifetime compatibility bridge used by whichever campaign package is
// selected at startup. The built-in manifest and a disk-discovered data
// campaign are peers in PackageRegistry; both drive the same compiled table,
// text, grid, and Lua bootstrap without duplicating that legacy state.
class LegacyCampaignRuntime
{
public:
	LegacyCampaignRuntime(GameCapabilities capabilities,
		LegacyCampaignBootstrapHooks& bootstrapHooks);

	bool bootstrap(PackageBootstrapPhase phase);
	void shutdown(PackageBootstrapPhase phase);
	void rethrowBootstrapFailure();
	const GameCapabilities& capabilities() const { return capabilities_; }

private:
	GameCapabilities capabilities_;
	LegacyCampaignBootstrapHooks& bootstrapHooks_;
	bool contentLoadAttempted_ = false;
	bool contentLoaded_ = false;
	bool runtimeStartAttempted_ = false;
	bool runtimeStarted_ = false;
	std::exception_ptr bootstrapFailure_;
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
	void rethrowBootstrapFailure();
	bool active() const { return active_; }
	const GameCapabilities& capabilities() const { return runtime_.capabilities(); }
	LegacyCampaignRuntime& runtime() { return runtime_; }

private:
	LegacyCampaignRuntime runtime_;
	PackageDescriptor descriptor_;
	bool active_ = false;
};

LegacyCampaignPackage& GetCompiledCampaignPackage();
LegacyCampaignRuntime& GetCompiledCampaignRuntime();

#endif
