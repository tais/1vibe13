#include "GameCapabilities.h"

GameCapabilities GetCompiledGameCapabilities()
{
	GameCapabilities capabilities;
#ifdef JA2UB
	capabilities.campaign = GameCampaign::UnfinishedBusiness;
#endif
#ifdef JA2EDITOR
	capabilities.editor = true;
#endif
	return capabilities;
}
