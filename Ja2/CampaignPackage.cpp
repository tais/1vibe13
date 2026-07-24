#include "CampaignPackage.h"

#include "Init.h"
#include "LuaInitNPCs.h"
#include "Text.h"
#include "XML.h"
#include "Item Types.h"
#include "Weapons.h"
#include "DEBUG.H"

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
		BOOLEAN loaded = FALSE;
		SGP_TRYCATCH_RETHROW(
			loaded = LoadExternalGameplayData(TABLEDATA_DIRECTORY, false),
			L"Loading external data failed");
		if (!loaded)
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
	: runtime_(capabilities, bootstrapHooks),
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
	return runtime_.bootstrap(phase);
}

void LegacyCampaignPackage::shutdown(
	PackageBootstrapContext&, PackageBootstrapPhase phase)
{
	runtime_.shutdown(phase);
}

void LegacyCampaignPackage::rethrowBootstrapFailure()
{
	runtime_.rethrowBootstrapFailure();
}

LegacyCampaignRuntime::LegacyCampaignRuntime(
	GameCapabilities capabilities, LegacyCampaignBootstrapHooks& bootstrapHooks)
	: capabilities_(capabilities), bootstrapHooks_(bootstrapHooks)
{
}

bool LegacyCampaignRuntime::bootstrap(PackageBootstrapPhase phase)
{
	switch (phase)
	{
		case PackageBootstrapPhase::Configure:
			return true;
		case PackageBootstrapPhase::LoadContent:
			if (contentLoaded_) return true;
			if (contentLoadAttempted_) return false;
			contentLoadAttempted_ = true;
			bootstrapFailure_ = nullptr;
			try
			{
				if (!bootstrapHooks_.loadContent(capabilities_)) return false;
				contentLoaded_ = true;
				return true;
			}
			catch (...)
			{
				bootstrapFailure_ = std::current_exception();
				return false;
			}
		case PackageBootstrapPhase::StartRuntime:
			if (runtimeStarted_) return true;
			if (runtimeStartAttempted_) return false;
			runtimeStartAttempted_ = true;
			bootstrapFailure_ = nullptr;
			try
			{
				if (!bootstrapHooks_.startRuntime(capabilities_)) return false;
				runtimeStarted_ = true;
				return true;
			}
			catch (...)
			{
				bootstrapFailure_ = std::current_exception();
				return false;
			}
	}
	return false;
}

void LegacyCampaignRuntime::shutdown(PackageBootstrapPhase)
{
	// Legacy gameplay tables, text, grids, and Lua globals are process-lifetime
	// state. The package owns their startup order but intentionally does not
	// pretend they can be hot-unloaded during lifecycle rollback/shutdown.
}

void LegacyCampaignRuntime::rethrowBootstrapFailure()
{
	if (!bootstrapFailure_) return;
	const std::exception_ptr failure = bootstrapFailure_;
	bootstrapFailure_ = nullptr;
	std::rethrow_exception(failure);
}

LegacyCampaignPackage& GetCompiledCampaignPackage()
{
	static LegacyCampaignPackage package(GetCompiledGameCapabilities());
	return package;
}

LegacyCampaignRuntime& GetCompiledCampaignRuntime()
{
	return GetCompiledCampaignPackage().runtime();
}
