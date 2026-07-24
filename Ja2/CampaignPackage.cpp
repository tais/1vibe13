#include "CampaignPackage.h"

LegacyCampaignPackage::LegacyCampaignPackage(
	GameCapabilities capabilities,
	CampaignRuntimeBootstrapHost& bootstrapHost)
	: capabilities_(capabilities),
	  bootstrapHost_(bootstrapHost),
	  descriptor_{
		ContentManifest{
			capabilities.isUnfinishedBusiness()
				? "ja2.unfinished-business" : "ja2.arulco",
			"1.13",
			ContentApiVersion{1, 2},
			{{GamePackage::Rules113, GamePackage::Rules113Version}}
		},
		PackageKind::Campaign,
		{capabilities.isUnfinishedBusiness()
			? GameCapability::CampaignUnfinishedBusiness
			: GameCapability::CampaignArulco}
	  }
{
}

bool LegacyCampaignPackage::activate() noexcept
{
	if (active_) return false;
	active_ = true;
	return true;
}

void LegacyCampaignPackage::deactivate() noexcept
{
	active_ = false;
}

bool LegacyCampaignPackage::bootstrap(
	PackageBootstrapContext&, PackageBootstrapPhase phase)
{
	switch (phase)
	{
		case PackageBootstrapPhase::Configure:
		case PackageBootstrapPhase::LoadContent:
			return true;
		case PackageBootstrapPhase::StartRuntime:
			if (runtimeStarted_) return true;
			if (runtimeStartAttempted_) return false;
			runtimeStartAttempted_ = true;
			if (!bootstrapHost_.startCampaignRuntime(capabilities_)) return false;
			runtimeStarted_ = true;
			return true;
	}
	return false;
}

void LegacyCampaignPackage::shutdown(
	PackageBootstrapContext&, PackageBootstrapPhase phase)
{
	(void)phase;
	// Grid and Lua globals remain process-lifetime until their consumers have
	// explicit teardown ownership. The package still owns their startup order.
}

LegacyCampaignPackage& GetCompiledCampaignPackage()
{
	static LegacyCampaignPackage package(
		GetCompiledGameCapabilities(),
		GetCompiledCampaignRuntimeBootstrapHost());
	return package;
}
