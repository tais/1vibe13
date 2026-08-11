#ifndef JA2_CAMPAIGN_DOOR_POLICY_H
#define JA2_CAMPAIGN_DOOR_POLICY_H

#include "GameCapabilities.h"

// Campaign-specific door behavior is content policy, not executable identity.
// Both built-in paths stay available to the common tactical implementation.
class CampaignDoorPolicy
{
public:
	explicit constexpr CampaignDoorPolicy(GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignDoorPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignDoorPolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessTunnelGate() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	constexpr bool usesOpenDoorCostWhenForcing() const noexcept
	{
		return usesUnfinishedBusinessTunnelGate();
	}

	// In UB, a tunnel-gate quote consumes the force attempt. Arulco has no
	// campaign quote at this boundary and always performs the normal attempt.
	constexpr bool shouldAttemptForceDoor(
		bool tunnelQuoteHandled) const noexcept
	{
		return !usesUnfinishedBusinessTunnelGate() || !tunnelQuoteHandled;
	}

	// The tactical door menu uses the same UB tunnel-gate interception as a
	// direct force attempt. Keeping this named at the menu boundary makes it
	// explicit that Arulco never calls the UB quote hook.
	constexpr bool shouldAttemptDoorMenuAction(
		bool tunnelQuoteHandled) const noexcept
	{
		return shouldAttemptForceDoor(tunnelQuoteHandled);
	}

	// The established UB branch curses only when its tunnel quote handled the
	// failed unlock. Arulco retains its ordinary random-curse opportunity.
	constexpr bool shouldOfferFailedUnlockCurse(
		bool tunnelQuoteHandled) const noexcept
	{
		return !usesUnfinishedBusinessTunnelGate() || tunnelQuoteHandled;
	}

private:
	GameCampaign campaign_;
};

static_assert(CampaignDoorPolicy(GameCampaign::Arulco)
	.shouldAttemptForceDoor(true));
static_assert(!CampaignDoorPolicy(GameCampaign::UnfinishedBusiness)
	.shouldAttemptForceDoor(true));
static_assert(CampaignDoorPolicy(GameCampaign::UnfinishedBusiness)
	.shouldAttemptForceDoor(false));
static_assert(CampaignDoorPolicy(GameCampaign::Arulco)
	.shouldOfferFailedUnlockCurse(false));
static_assert(!CampaignDoorPolicy(GameCampaign::UnfinishedBusiness)
	.shouldOfferFailedUnlockCurse(false));

#endif
