#include "CampaignPackage.h"

LegacyCampaignPackage::LegacyCampaignPackage(LegacyGameplayRuntime& runtime)
	: runtime_(runtime),
	  descriptor_{
		ContentManifest{
			runtime.capabilities().isUnfinishedBusiness()
				? "ja2.unfinished-business" : "ja2.arulco",
			"1.13",
			ContentApiVersion{1, 2},
			{{GamePackage::Rules113, GamePackage::Rules113Version}}
		},
		PackageKind::Campaign,
		{runtime.capabilities().isUnfinishedBusiness()
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
			return runtime_.startCampaignRuntime();
	}
	return false;
}

void LegacyCampaignPackage::shutdown(
	PackageBootstrapContext&, PackageBootstrapPhase phase)
{
	if (phase == PackageBootstrapPhase::StartRuntime)
		runtime_.shutdownCampaignRuntime();
}

LegacyCampaignPackage& GetCompiledCampaignPackage()
{
	static LegacyCampaignPackage package(GetCompiledGameplayRuntime());
	return package;
}
