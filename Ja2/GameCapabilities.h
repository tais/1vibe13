#ifndef JA2_GAME_CAPABILITIES_H
#define JA2_GAME_CAPABILITIES_H

namespace GameCapability
{
constexpr const char* CampaignArulco = "campaign.ja2";
constexpr const char* CampaignUnfinishedBusiness = "campaign.unfinished-business";
constexpr const char* Rules113 = "rules.ja2-1.13";
constexpr const char* HostCampaignArulco = "host.campaign-family.ja2";
constexpr const char* HostCampaignUnfinishedBusiness =
	"host.campaign-family.unfinished-business";
constexpr const char* ApplicationMapEditor = "application.map-editor";
}

namespace GamePackage
{
constexpr const char* Rules113 = "ja2.1.13";
constexpr const char* Rules113Version = "1.13";
}

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
