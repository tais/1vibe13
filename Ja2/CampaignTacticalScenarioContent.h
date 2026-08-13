#ifndef JA2_CAMPAIGN_TACTICAL_SCENARIO_CONTENT_H
#define JA2_CAMPAIGN_TACTICAL_SCENARIO_CONTENT_H

#include <array>
#include <cstddef>
#include <cstdint>

// Campaign-authored tactical coordinates are values. Keeping their shape here
// prevents tactical consumers from depending on the legacy UB option record.
struct CampaignTacticalSector
{
	std::uint32_t x = 0;
	std::uint32_t y = 0;
	std::uint32_t z = 0;

	constexpr bool operator==(const CampaignTacticalSector& other) const noexcept
	{
		return x == other.x && y == other.y && z == other.z;
	}

	constexpr bool operator!=(const CampaignTacticalSector& other) const noexcept
	{
		return !(*this == other);
	}
};

template<std::size_t Size>
struct CampaignTacticalGridSet
{
	std::array<std::uint32_t, Size> values{};

	constexpr bool contains(std::uint32_t grid) const noexcept
	{
		for (const std::uint32_t candidate : values)
		{
			if (candidate == grid) return true;
		}
		return false;
	}

	constexpr std::uint32_t operator[](std::size_t index) const noexcept
	{
		return values[index];
	}
};

using CampaignTacticalFanGridSet = CampaignTacticalGridSet<9>;
using CampaignTacticalGridPair = CampaignTacticalGridSet<2>;

// The tunnel explosion story flag intentionally applies only at depth one.
// The two authored Y coordinates share one X coordinate in the legacy data.
struct CampaignTacticalTunnelSectorSet
{
	std::uint32_t x = 0;
	std::array<std::uint32_t, 2> y{};

	constexpr bool contains(const CampaignTacticalSector& sector) const noexcept
	{
		return sector.x == x && sector.z == 1 &&
			(sector.y == y[0] || sector.y == y[1]);
	}
};

struct CampaignTacticalMineContent
{
	CampaignTacticalSector surfaceSector{};
	std::uint32_t entranceGrid = 0;
	std::uint32_t collapsedEntranceGrid = 0;
	CampaignTacticalGridPair surfaceExitGrids{};
	CampaignTacticalSector undergroundSector{};
	CampaignTacticalGridPair undergroundEntranceGrids{};
};

struct CampaignTacticalScenarioContent
{
	CampaignTacticalSector fanSector{};
	CampaignTacticalFanGridSet fanGrids{};
	CampaignTacticalSector missileLaunchSector{};
	CampaignTacticalTunnelSectorSet tunnelExplosionSectors{};
	CampaignTacticalSector fortifiedDoorSector{};
	CampaignTacticalMineContent mine{};
};

// These application adapters deliberately return values. The legacy option
// record remains private at this boundary and can be replaced by package content
// without changing either tactical consumer.
CampaignTacticalScenarioContent ReadCampaignTacticalScenarioContent();
bool ReadCampaignHandleAddingEnemiesToTunnelMaps();

#endif
