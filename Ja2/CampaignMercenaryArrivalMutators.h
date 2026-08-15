#ifndef JA2_CAMPAIGN_MERCENARY_ARRIVAL_MUTATORS_H
#define JA2_CAMPAIGN_MERCENARY_ARRIVAL_MUTATORS_H

#include <cstdint>

// Lua-authored arrival configuration crosses into the application through
// semantic commands. The strong slot type prevents the caller from treating
// the seven helicopter grids as an unbounded legacy array.
enum class CampaignMercenaryHelicopterArrivalSlot : std::uint8_t
{
	First,
	Second,
	Third,
	Fourth,
	Fifth,
	Sixth,
	Seventh
};

// Jerry's script command has a three-way grid contract: positive values set
// the requested grid, negative values select the legacy fallback, and zero
// leaves the current grid untouched. Keep that decision dependency-free so it
// can be tested without exposing the mutable UB option record.
enum class CampaignMercenaryJerryGridUpdate : std::uint8_t
{
	Preserve,
	Requested,
	LegacyFallback
};

struct CampaignMercenaryJerryMutation
{
	bool includesJerry = false;
	CampaignMercenaryJerryGridUpdate gridUpdate =
		CampaignMercenaryJerryGridUpdate::Preserve;
	std::uint32_t gridNo = 0;
};

inline constexpr std::uint32_t CampaignMercenaryLegacyJerryGridNo = 15943;

constexpr CampaignMercenaryJerryMutation PlanCampaignMercenaryJerryMutation(
	bool includesJerry, std::int32_t requestedGridNo) noexcept
{
	if (requestedGridNo > 0)
	{
		return {includesJerry,
			CampaignMercenaryJerryGridUpdate::Requested,
			static_cast<std::uint32_t>(requestedGridNo)};
	}
	if (requestedGridNo < 0)
	{
		return {includesJerry,
			CampaignMercenaryJerryGridUpdate::LegacyFallback,
			CampaignMercenaryLegacyJerryGridNo};
	}
	return {includesJerry, CampaignMercenaryJerryGridUpdate::Preserve, 0};
}

// These application-owned mutators copy values into the legacy record. They
// never return it, expose a reference, or accept a generic field selector.
void SetCampaignMercenaryHelicopterArrivalGrid(
	CampaignMercenaryHelicopterArrivalSlot slot, std::uint32_t gridNo);
void SetCampaignMercenaryJerryArrivalGrid(std::uint32_t gridNo);
void SetCampaignMercenaryLaptopQuestEnabled(bool enabled);
void ConfigureCampaignMercenaryJerry(
	bool includesJerry, std::int32_t requestedGridNo);
void SetCampaignMercenaryJerryQuotesEnabled(bool enabled);
void SetCampaignMercenaryHelicopterCrashEnabled(bool enabled);
void SetCampaignMercenaryHelicopterEnabled(bool enabled);
void SetCampaignMercenaryOffscreenArrivalGrid(std::uint32_t gridNo);

#endif
