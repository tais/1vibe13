#ifndef JA2_CAMPAIGN_LAPTOP_CONTENT_POLICY_H
#define JA2_CAMPAIGN_LAPTOP_CONTENT_POLICY_H

#include "GameCapabilities.h"

#include <cstdint>

// Runtime-owned campaign choices for the Laptop file viewer and history text.
// The legacy pages keep responsibility for asset probing, loading, and
// rendering; this value-only policy selects the complete catalog record after
// being told whether an optional UB asset is installed.
class CampaignLaptopContentPolicy
{
public:
	struct BriefingCatalog
	{
		const char* path;
		std::uint16_t recordCount;
	};

	struct QuestTextRecord
	{
		const char* path;
		std::uint16_t recordIndex;
	};

	explicit constexpr CampaignLaptopContentPolicy(
		GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignLaptopContentPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignLaptopContentPolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessContent() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	static constexpr const char* arulcoBriefingPath() noexcept
	{
		return "BINARYDATA\\RIS.edt";
	}

	static constexpr const char* unfinishedBusinessBriefingPath() noexcept
	{
		return "BINARYDATA\\RIS25.edt";
	}

	constexpr BriefingCatalog briefingCatalog(
		bool unfinishedBusinessBriefingAvailable) const noexcept
	{
		if (usesUnfinishedBusinessContent())
		{
			return {
				unfinishedBusinessBriefingAvailable
					? unfinishedBusinessBriefingPath()
					: arulcoBriefingPath(),
				39};
		}
		return {arulcoBriefingPath(), 68};
	}

	static constexpr const char* arulcoMapPath() noexcept
	{
		return "LAPTOP\\ArucoFilesMap.sti";
	}

	static constexpr const char* unfinishedBusinessMapPath() noexcept
	{
		return "LAPTOP\\TraconaMap.sti";
	}

	constexpr const char* filesMapPath(
		bool unfinishedBusinessMapAvailable) const noexcept
	{
		return usesUnfinishedBusinessContent() &&
			unfinishedBusinessMapAvailable
			? unfinishedBusinessMapPath()
			: arulcoMapPath();
	}

	constexpr const char* biographyPicturePath(
		std::uint32_t page) const noexcept
	{
		if (usesUnfinishedBusinessContent()) return nullptr;
		if (page == 4) return "LAPTOP\\Enrico_Y.sti";
		if (page == 5) return "LAPTOP\\Enrico_W.sti";
		return nullptr;
	}

	static constexpr const char* arulcoQuestTextPath() noexcept
	{
		return "BINARYDATA\\quests.edt";
	}

	static constexpr const char* unfinishedBusinessQuestTextPath() noexcept
	{
		return "BINARYDATA\\quests25.edt";
	}

	constexpr QuestTextRecord questTextRecord(
		std::uint8_t quest, bool completed,
		bool unfinishedBusinessQuestTextAvailable) const noexcept
	{
		const char* path = usesUnfinishedBusinessContent() &&
			unfinishedBusinessQuestTextAvailable
			? unfinishedBusinessQuestTextPath()
			: arulcoQuestTextPath();
		return {path, static_cast<std::uint16_t>(
			static_cast<std::uint16_t>(quest) * 2 + (completed ? 1 : 0))};
	}

private:
	GameCampaign campaign_;
};

static_assert(CampaignLaptopContentPolicy(GameCampaign::Arulco)
	.briefingCatalog(true).recordCount == 68);
static_assert(CampaignLaptopContentPolicy(GameCampaign::UnfinishedBusiness)
	.briefingCatalog(true).recordCount == 39);
static_assert(CampaignLaptopContentPolicy(GameCampaign::UnfinishedBusiness)
	.questTextRecord(7, true, false).recordIndex == 15);

#endif
