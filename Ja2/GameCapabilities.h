#ifndef JA2_GAME_CAPABILITIES_H
#define JA2_GAME_CAPABILITIES_H

enum class GameCampaign
{
	Arulco,
	UnfinishedBusiness
};

struct GameCapabilities
{
	GameCampaign campaign = GameCampaign::Arulco;
	bool editor = false;

	bool isUnfinishedBusiness() const { return campaign == GameCampaign::UnfinishedBusiness; }
	bool isEditor() const { return editor; }
};

GameCapabilities GetCompiledGameCapabilities();

#endif
