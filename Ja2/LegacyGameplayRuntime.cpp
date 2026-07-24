#include "LegacyGameplayRuntime.h"

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
class CompiledGameplayBootstrapHooks final
	: public LegacyGameplayBootstrapHooks
{
public:
	bool loadRulesContent(const GameCapabilities&) override
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

	bool startCampaignRuntime(const GameCapabilities& capabilities) override
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
}

LegacyGameplayRuntime::LegacyGameplayRuntime(
	GameCapabilities capabilities, LegacyGameplayBootstrapHooks& bootstrapHooks)
	: capabilities_(capabilities), bootstrapHooks_(bootstrapHooks)
{
}

bool LegacyGameplayRuntime::loadRulesContent()
{
	if (contentLoaded_) return true;
	if (contentLoadAttempted_) return false;
	contentLoadAttempted_ = true;
	bootstrapFailure_ = nullptr;
	try
	{
		if (!bootstrapHooks_.loadRulesContent(capabilities_)) return false;
		contentLoaded_ = true;
		return true;
	}
	catch (...)
	{
		bootstrapFailure_ = std::current_exception();
		return false;
	}
}

bool LegacyGameplayRuntime::startCampaignRuntime()
{
	if (runtimeStarted_) return true;
	if (runtimeStartAttempted_) return false;
	runtimeStartAttempted_ = true;
	bootstrapFailure_ = nullptr;
	try
	{
		if (!bootstrapHooks_.startCampaignRuntime(capabilities_)) return false;
		runtimeStarted_ = true;
		return true;
	}
	catch (...)
	{
		bootstrapFailure_ = std::current_exception();
		return false;
	}
}

void LegacyGameplayRuntime::shutdownRulesContent()
{
	// Legacy gameplay tables and text remain process-lifetime state until their
	// consumers no longer retain raw pointers into the loaded collections.
}

void LegacyGameplayRuntime::shutdownCampaignRuntime()
{
	// Legacy grid and Lua globals are likewise process-lifetime. Package
	// shutdown owns their ordering without pretending hot-unload is safe.
}

void LegacyGameplayRuntime::rethrowBootstrapFailure()
{
	if (!bootstrapFailure_) return;
	const std::exception_ptr failure = bootstrapFailure_;
	bootstrapFailure_ = nullptr;
	std::rethrow_exception(failure);
}

LegacyGameplayRuntime& GetCompiledGameplayRuntime()
{
	static CompiledGameplayBootstrapHooks hooks;
	static LegacyGameplayRuntime runtime(GetCompiledGameCapabilities(), hooks);
	return runtime;
}
