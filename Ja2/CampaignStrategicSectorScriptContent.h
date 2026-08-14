#ifndef JA2_CAMPAIGN_STRATEGIC_SECTOR_SCRIPT_CONTENT_H
#define JA2_CAMPAIGN_STRATEGIC_SECTOR_SCRIPT_CONTENT_H

#include "CampaignTacticalScenarioContent.h"

#include <array>
#include <cstdint>

struct CampaignStrategicMoneyDrop
{
	std::uint32_t grid = 0;
	std::uint32_t easyAmount = 0;
	std::uint32_t mediumAmount = 0;
	std::uint32_t hardAmount = 0;
};

struct CampaignStrategicGridMove
{
	std::uint32_t sourceGrid = 0;
	std::uint32_t destinationGrid = 0;

	constexpr bool operator==(
		const CampaignStrategicGridMove& other) const noexcept
	{
		return sourceGrid == other.sourceGrid &&
			destinationGrid == other.destinationGrid;
	}
};

// Immutable campaign-authored values used by the contiguous strategic sector
// script. The application adapter returns a fresh value for each invocation;
// save data, live switches, actors, dialogue, and effects stay in their owners.
struct CampaignStrategicSectorScriptContent
{
	// These three sectors are shared with the tactical scenario projection.
	CampaignTacticalSector fanSector{};
	CampaignTacticalSector missileLaunchSector{};
	CampaignTacticalSector fortifiedDoorSector{};

	CampaignTacticalSector firstTunnelSector{};
	CampaignTacticalSector gateTunnelSector{};
	CampaignTacticalSector guardPostSector{};
	CampaignTacticalSector firstTownSector{};
	CampaignTacticalSector town2Sector{};
	CampaignTacticalSector town3Sector{};
	CampaignTacticalSector i9QuoteSector{};
	CampaignTacticalSector h10QuoteSector{};

	CampaignStrategicMoneyDrop guardPostMoney{};
	std::array<CampaignStrategicMoneyDrop, 2> firstTownMoney{};
	std::uint32_t fortifiedDoorGrid = 0;
	std::array<CampaignStrategicGridMove, 2> town2RoofMoves{};
	std::array<CampaignStrategicGridMove, 1> town3RoofMoves{};

	// Keep the exact six-entry legacy quote table. Its final entry deliberately
	// combines the guard-post X/Y with the first-tunnel Z and therefore falls
	// through to the generic quote path rather than identifying the tunnel.
	constexpr std::array<CampaignTacticalSector, 6>
	playerQuoteSectors() const noexcept
	{
		return {{
			guardPostSector,
			i9QuoteSector,
			h10QuoteSector,
			firstTownSector,
			fanSector,
			{guardPostSector.x, guardPostSector.y, firstTunnelSector.z}
		}};
	}

	static constexpr CampaignTacticalSector
	legacyTownEmailFallbackSector() noexcept
	{
		return {12, 9, 0};
	}
};

// Application-owned read-through adapter. It deliberately returns a value and
// never caches the mutable legacy option record.
CampaignStrategicSectorScriptContent
ReadCampaignStrategicSectorScriptContent();

#endif
