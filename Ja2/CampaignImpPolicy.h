#ifndef JA2_CAMPAIGN_IMP_POLICY_H
#define JA2_CAMPAIGN_IMP_POLICY_H

#include "GameCapabilities.h"

#include <cstdint>

// IMP activation and text-resource selection are campaign content policy, not
// executable identity. Both built-in paths remain available in every host.
class CampaignImpPolicy
{
public:
	enum class ActivationDecision : std::uint8_t
	{
		Authorized,
		Invalid,
		KnownButUnavailable
	};

	explicit constexpr CampaignImpPolicy(GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignImpPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignImpPolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessImpRules() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	// This deliberately preserves the established UB precedence: an enabled
	// JA2 pass is accepted regardless of gfNewGameLaptop, while the UB pass is
	// subject to the legacy '< 2' gate. Disabled passes do not make an input
	// invalid; they leave it known-but-unavailable.
	constexpr ActivationDecision classifyActivation(
		bool matchesJa2Pass,
		bool matchesUnfinishedBusinessPass,
		std::uint8_t newGameLaptopState,
		bool ja2PassEnabled,
		bool unfinishedBusinessPassEnabled) const noexcept
	{
		if (!usesUnfinishedBusinessImpRules())
		{
			if (matchesJa2Pass && newGameLaptopState < 2)
				return ActivationDecision::Authorized;
			return matchesJa2Pass
				? ActivationDecision::KnownButUnavailable
				: ActivationDecision::Invalid;
		}

		if ((ja2PassEnabled && matchesJa2Pass) ||
			(unfinishedBusinessPassEnabled &&
				matchesUnfinishedBusinessPass && newGameLaptopState < 2))
			return ActivationDecision::Authorized;

		if ((ja2PassEnabled && !matchesJa2Pass) ||
			(unfinishedBusinessPassEnabled &&
				!matchesUnfinishedBusinessPass))
			return ActivationDecision::Invalid;

		return ActivationDecision::KnownButUnavailable;
	}

	constexpr const char* impTextResource(
		bool unfinishedBusinessResourceExists) const noexcept
	{
		if (!usesUnfinishedBusinessImpRules())
			return "BINARYDATA\\IMPText.EDT";
		return unfinishedBusinessResourceExists
			? "BINARYDATA\\IMPText25.edt"
			: "BINARYDATA\\IMPText.edt";
	}

private:
	GameCampaign campaign_;
};

static_assert(CampaignImpPolicy(GameCampaign::Arulco).classifyActivation(
	true, false, 1, false, false) ==
	CampaignImpPolicy::ActivationDecision::Authorized);
static_assert(CampaignImpPolicy(GameCampaign::Arulco).classifyActivation(
	true, false, 2, false, false) ==
	CampaignImpPolicy::ActivationDecision::KnownButUnavailable);
static_assert(CampaignImpPolicy(GameCampaign::UnfinishedBusiness)
	.classifyActivation(true, false, 2, true, false) ==
	CampaignImpPolicy::ActivationDecision::Authorized);
static_assert(CampaignImpPolicy(GameCampaign::UnfinishedBusiness)
	.classifyActivation(false, true, 2, false, true) ==
	CampaignImpPolicy::ActivationDecision::KnownButUnavailable);

#endif
