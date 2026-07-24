#ifndef JA2_RULES_PACKAGE_H
#define JA2_RULES_PACKAGE_H

#include "RulesContentBootstrap.h"

#include <Engine/Core/PackageApi.h>

// The compiled 1.13 rules compatibility layer is a real package dependency of
// every built-in or data campaign. It owns legacy table/text loading; campaign
// packages retain campaign identity, assets, and runtime-specific startup.
class LegacyRulesPackage final : public EnginePackage
{
public:
	LegacyRulesPackage(GameCapabilities capabilities,
		RulesContentBootstrapHost& bootstrapHost);

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override;
	void deactivate() noexcept override;
	bool bootstrap(PackageBootstrapContext& context,
		PackageBootstrapPhase phase) override;
	void shutdown(PackageBootstrapContext& context,
		PackageBootstrapPhase phase) override;
	bool active() const { return active_; }

private:
	GameCapabilities capabilities_;
	RulesContentBootstrapHost& bootstrapHost_;
	PackageDescriptor descriptor_;
	bool active_ = false;
	bool contentLoadAttempted_ = false;
	bool contentLoaded_ = false;
};

LegacyRulesPackage& GetCompiledRulesPackage();

#endif
