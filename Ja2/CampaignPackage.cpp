#include "CampaignPackage.h"

#include "Init.h"
#include "LuaInitNPCs.h"
#include "Text.h"
#include "XML.h"
#include "Item Types.h"
#include "Weapons.h"

#ifdef JA2UB
#include "Ja25_Tactical.h"
#endif

namespace
{
class CompiledCampaignBootstrapHooks final : public LegacyCampaignBootstrapHooks
{
public:
	bool loadContent(const GameCapabilities&) override
	{
		if (!LoadExternalGameplayData(TABLEDATA_DIRECTORY, false))
			return false;

		// Once Magazines.xml is parsed, retain the legacy item back-reference
		// before any extension package receives its LoadContent callback.
		for (UINT32 index = 0; index < gMAXITEMS_READ; ++index)
		{
			if (Item[index].usItemClass == IC_AMMO)
				Magazine[Item[index].ubClassIndex].uiIndex = Item[index].uiIndex;
		}

		LoadAllExternalText();
		return true;
	}

	bool startRuntime(const GameCapabilities& capabilities) override
	{
#ifdef JA2UB
		if (capabilities.isUnfinishedBusiness())
			InitGridNoUB();
#else
		(void)capabilities;
#endif
		IniLuaGlobal();
		return true;
	}
};

LegacyCampaignBootstrapHooks& GetCompiledCampaignBootstrapHooks()
{
	static CompiledCampaignBootstrapHooks hooks;
	return hooks;
}
}

LegacyCampaignPackage::LegacyCampaignPackage(GameCapabilities capabilities)
	: LegacyCampaignPackage(
		capabilities, GetCompiledCampaignBootstrapHooks())
{
}

LegacyCampaignPackage::LegacyCampaignPackage(GameCapabilities capabilities,
	LegacyCampaignBootstrapHooks& bootstrapHooks)
	: capabilities_(capabilities),
	  descriptor_{
		ContentManifest{
			capabilities.isUnfinishedBusiness() ? "ja2.unfinished-business" : "ja2.arulco",
			"1.13",
			ContentApiVersion{1, 0}
		},
		PackageKind::Campaign,
		{capabilities.isUnfinishedBusiness()
			? GameCapability::CampaignUnfinishedBusiness
			: GameCapability::CampaignArulco}
	  },
	  bootstrapHooks_(bootstrapHooks)
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
			return true;
		case PackageBootstrapPhase::LoadContent:
			return bootstrapHooks_.loadContent(capabilities_);
		case PackageBootstrapPhase::StartRuntime:
			return bootstrapHooks_.startRuntime(capabilities_);
	}
	return false;
}

void LegacyCampaignPackage::shutdown(
	PackageBootstrapContext&, PackageBootstrapPhase)
{
	// Legacy gameplay tables, text, grids, and Lua globals are process-lifetime
	// state. The package owns their startup order but intentionally does not
	// pretend they can be hot-unloaded during lifecycle rollback/shutdown.
}

LegacyCampaignPackage& GetCompiledCampaignPackage()
{
	static LegacyCampaignPackage package(GetCompiledGameCapabilities());
	return package;
}
