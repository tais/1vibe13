#ifndef MULTIPLAYER_COOP_CAMPAIGN_MEANWHILE_H
#define MULTIPLAYER_COOP_CAMPAIGN_MEANWHILE_H

#include <cstdint>

namespace CoopSession
{
enum class CoopCampaignMeanwhileScene : std::uint8_t
{
	None = 0, FirstBattle = 1, Drassen = 2, Cambria = 3, Alma = 4, Grumm = 5,
	Chitzena = 6, NorthwestSam = 7, NortheastSam = 8, CentralSam = 9,
	Flowers = 10, LostTown = 11, Interrogation = 12, Creatures = 13,
	Helicopter = 14, Scientist = 15, Meduna = 16, Balime = 17
};
struct CoopCampaignMeanwhileNotice
{
	std::uint64_t id = 0;
	CoopCampaignMeanwhileScene scene = CoopCampaignMeanwhileScene::None;
};
inline bool ValidCoopCampaignMeanwhileNotice(const CoopCampaignMeanwhileNotice& value) noexcept
{
	return value.id ? value.scene >= CoopCampaignMeanwhileScene::FirstBattle && value.scene <= CoopCampaignMeanwhileScene::Balime
		: value.scene == CoopCampaignMeanwhileScene::None;
}
inline bool SameCoopCampaignMeanwhileNotice(const CoopCampaignMeanwhileNotice& a, const CoopCampaignMeanwhileNotice& b) noexcept
{ return a.id == b.id && a.scene == b.scene; }
inline bool ValidCoopCampaignMeanwhileReplacement(const CoopCampaignMeanwhileNotice& previous,
	const CoopCampaignMeanwhileNotice& next, std::uint64_t lastId) noexcept
{
	return ValidCoopCampaignMeanwhileNotice(next) && (!next.id ||
		(next.id == previous.id ? SameCoopCampaignMeanwhileNotice(previous, next) : next.id > lastId));
}
}
#endif
