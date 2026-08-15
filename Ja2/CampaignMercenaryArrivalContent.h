#ifndef JA2_CAMPAIGN_MERCENARY_ARRIVAL_CONTENT_H
#define JA2_CAMPAIGN_MERCENARY_ARRIVAL_CONTENT_H

#include <array>
#include <cstddef>
#include <cstdint>

// Immutable campaign-authored values used while hiring and placing arriving
// mercenaries. The application adapter returns a fresh value for each routed
// invocation; repeated decisions in that invocation deliberately observe one
// frozen projection across intervening actor, quest, and random effects.
// Tactical code never retains or exposes the mutable legacy UB option record.
struct CampaignMercenaryArrivalContent
{
	static constexpr std::size_t HelicopterEntryCount = 7;

	std::array<std::uint32_t, HelicopterEntryCount>
		initialHelicopterGridNos{};
	std::array<std::int16_t, HelicopterEntryCount>
		initialHelicopterRandomTimes{};

	bool includesJerry = false;
	bool inGameHelicopter = false;
	bool inGameHelicopterCrash = false;
	std::uint32_t jerryGridNo = 0;
	bool laptopQuestEnabled = false;
	std::uint32_t offscreenArrivalGridNo = 0;
};

// Application-owned read-through adapter. It is intentionally value-returning
// and uncached; campaign policy must gate it before tactical code requests UB
// content. Future configuration mutations remain separate ub_config-owned APIs
// rather than exposing the legacy record through this value contract.
CampaignMercenaryArrivalContent ReadCampaignMercenaryArrivalContent();

#endif
