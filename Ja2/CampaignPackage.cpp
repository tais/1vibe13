#include "CampaignPackage.h"

LegacyCampaignPackage::LegacyCampaignPackage(GameCapabilities capabilities)
	: capabilities_(capabilities),
	  descriptor_{
		ContentManifest{
			capabilities.isUnfinishedBusiness() ? "ja2.unfinished-business" : "ja2.arulco",
			"1.13",
			ContentApiVersion{1, 0}
		},
		PackageKind::Campaign
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

LegacyCampaignPackage& GetCompiledCampaignPackage()
{
	static LegacyCampaignPackage package(GetCompiledGameCapabilities());
	return package;
}
