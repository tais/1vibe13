#include "CampaignRuntimeBootstrap.h"
#include "RulesContentBootstrap.h"

#include "Init.h"
#include "LuaInitNPCs.h"
#include "Text.h"
#include "XML.h"
#include "Item Types.h"
#include "Weapons.h"
#include "DEBUG.H"
#include "Ja25_Tactical.h"

namespace
{
class CompiledGameplayBootstrapAdapters final
	: public RulesContentBootstrapHost,
	  public CampaignRuntimeBootstrapHost
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
		if (capabilities.isUnfinishedBusiness())
			InitGridNoUB();
		IniLuaGlobal();
		return true;
	}
};

CompiledGameplayBootstrapAdapters& GetCompiledGameplayBootstrapAdapters()
{
	static CompiledGameplayBootstrapAdapters adapters;
	return adapters;
}
}

RulesContentBootstrapHost& GetCompiledRulesContentBootstrapHost()
{
	return GetCompiledGameplayBootstrapAdapters();
}

CampaignRuntimeBootstrapHost& GetCompiledCampaignRuntimeBootstrapHost()
{
	return GetCompiledGameplayBootstrapAdapters();
}
