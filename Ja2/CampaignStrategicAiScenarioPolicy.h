#ifndef JA2_CAMPAIGN_STRATEGIC_AI_SCENARIO_POLICY_H
#define JA2_CAMPAIGN_STRATEGIC_AI_SCENARIO_POLICY_H

#include <cstdint>

// This byte describes which scenario supplied JA25's strategic-AI state. It
// is deliberately separate from GameCampaign: both known values belong to an
// Unfinished Business campaign. Retaining the raw byte also preserves the
// legacy behavior of noncanonical save values, which match neither branch.
class CampaignScenarioOrigin
{
public:
	static constexpr CampaignScenarioOrigin fromLegacyByte(
		std::uint8_t value) noexcept
	{
		return CampaignScenarioOrigin(value);
	}

	constexpr std::uint8_t legacyByte() const noexcept
	{
		return value_;
	}

	constexpr bool isOriginalUnfinishedBusiness() const noexcept
	{
		return value_ == 1;
	}

	constexpr bool isCustomScenario() const noexcept
	{
		return value_ == 0;
	}

private:
	explicit constexpr CampaignScenarioOrigin(std::uint8_t value) noexcept
		: value_(value)
	{
	}

	std::uint8_t value_;
};

class CampaignStrategicAiScenarioPolicy
{
public:
	enum class H8AdvanceSource
	{
		None,
		BuiltInGuardPost,
		DefaultArrivalSector
	};

	enum class ComplexHistorySource
	{
		None,
		BuiltInSectorAi,
		StrategicSector
	};

	explicit constexpr CampaignStrategicAiScenarioPolicy(
		CampaignScenarioOrigin origin) noexcept
		: origin_(origin)
	{
	}

	constexpr bool usesBuiltInSectorAi() const noexcept
	{
		return origin_.isOriginalUnfinishedBusiness();
	}

	constexpr bool usesCustomScenario() const noexcept
	{
		return origin_.isCustomScenario();
	}

	constexpr H8AdvanceSource h8AdvanceSource() const noexcept
	{
		if (usesBuiltInSectorAi())
			return H8AdvanceSource::BuiltInGuardPost;
		if (usesCustomScenario())
			return H8AdvanceSource::DefaultArrivalSector;
		return H8AdvanceSource::None;
	}

	constexpr ComplexHistorySource complexHistorySource() const noexcept
	{
		if (usesBuiltInSectorAi())
			return ComplexHistorySource::BuiltInSectorAi;
		if (usesCustomScenario())
			return ComplexHistorySource::StrategicSector;
		return ComplexHistorySource::None;
	}

private:
	CampaignScenarioOrigin origin_;
};

// Application-owned live adapters. Scenario origin is restored from saves and
// reloaded with UB options, so callers must not cache it across invocations.
CampaignScenarioOrigin ReadCampaignScenarioOrigin();
void SetCampaignScenarioOrigin(CampaignScenarioOrigin origin);

static_assert(CampaignScenarioOrigin::fromLegacyByte(1)
	.isOriginalUnfinishedBusiness());
static_assert(CampaignScenarioOrigin::fromLegacyByte(0).isCustomScenario());
static_assert(!CampaignScenarioOrigin::fromLegacyByte(0xff)
	.isOriginalUnfinishedBusiness());
static_assert(!CampaignScenarioOrigin::fromLegacyByte(0xff).isCustomScenario());

#endif
