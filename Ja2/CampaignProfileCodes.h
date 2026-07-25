#ifndef JA2_CAMPAIGN_PROFILE_CODES_H
#define JA2_CAMPAIGN_PROFILE_CODES_H

#include "GameCapabilities.h"

#include <cstdint>

// The original campaign data assigns different numeric slots to the short
// profile range around Miguel. Keep those bytes unchanged and resolve their
// semantic role from the selected campaign at runtime.
namespace CampaignProfileCode
{
enum class Role : std::uint8_t
{
	Miguel,
	Carlos,
	Ira,
	Dimitri,
	Devin,
	Robot,
	Hamous,
	Slay
};

constexpr std::uint8_t arulcoProfile(Role role) noexcept
{
	return static_cast<std::uint8_t>(
		57u + static_cast<std::uint8_t>(role));
}

constexpr std::uint8_t unfinishedBusinessProfile(Role role) noexcept
{
	return static_cast<std::uint8_t>(arulcoProfile(role) + 1u);
}

constexpr std::uint8_t profile(
	GameCampaign campaign, Role role) noexcept
{
	return campaign == GameCampaign::UnfinishedBusiness
		? unfinishedBusinessProfile(role)
		: arulcoProfile(role);
}

constexpr bool matches(
	GameCampaign campaign, Role role, std::uint8_t rawProfile) noexcept
{
	return profile(campaign, role) == rawProfile;
}

inline constexpr std::uint8_t ArulcoGaston = 165;
inline constexpr std::uint8_t ArulcoStogie = 166;

static_assert(profile(GameCampaign::Arulco, Role::Miguel) == 57);
static_assert(profile(GameCampaign::UnfinishedBusiness, Role::Miguel) == 58);
static_assert(profile(GameCampaign::Arulco, Role::Robot) == 62);
static_assert(profile(GameCampaign::UnfinishedBusiness, Role::Robot) == 63);
static_assert(profile(GameCampaign::Arulco, Role::Slay) == 64);
static_assert(profile(GameCampaign::UnfinishedBusiness, Role::Slay) == 65);
}

#endif
