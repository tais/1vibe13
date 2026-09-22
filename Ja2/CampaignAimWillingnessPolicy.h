#ifndef JA2_CAMPAIGN_AIM_WILLINGNESS_POLICY_H
#define JA2_CAMPAIGN_AIM_WILLINGNESS_POLICY_H

#include <array>
#include <cstddef>
#include <cstdint>

inline constexpr std::size_t CampaignAimOrdinaryRelations = 5;
inline constexpr std::size_t CampaignAimAllRelations = 6;
inline constexpr std::uint8_t CampaignAimNoRelation = 6;

// Normalize absent/signed legacy relations before the reader can index a
// profile array. In the native profile table, NUM_PROFILES itself is absent.
template<class ReadPresence>
constexpr bool ReadCampaignAimRelationPresence(std::int32_t profile,
	std::uint32_t profileCount, const ReadPresence& readPresence)
{
	return profile >= 0 && static_cast<std::uint32_t>(profile) < profileCount &&
		readPresence(static_cast<std::uint32_t>(profile));
}

// Copied facts only. The last relation is the learned one; a positive counter
// means it has not become a buddy/foe yet, while zero and negative counts have.
struct CampaignAimWillingnessInput
{
	std::array<bool, CampaignAimAllRelations> hatedAliveOnTeam{};
	std::array<bool, CampaignAimAllRelations> buddiesAliveOnTeam{};
	std::array<std::int8_t, CampaignAimOrdinaryRelations> hatedToleranceHours{};
	std::int8_t learnedHateCount = 0;
	std::int8_t learnedLikeCount = 0;
	bool moraleHangover = false;
	bool deathRateTooHigh = false;
	bool reputationTooBad = false;
};

enum class CampaignAimWillingnessReason : std::uint8_t
{
	Willing,
	MoraleHangover,
	BuddyOverride,
	ToleratedHatred,
	HatedMerc,
	LearnedHatred,
	DeathRate,
	Reputation
};

struct CampaignAimWillingnessDecision
{
	CampaignAimWillingnessReason reason = CampaignAimWillingnessReason::Willing;
	std::uint8_t relation = CampaignAimNoRelation;

	constexpr bool accepted() const noexcept
	{
		return reason == CampaignAimWillingnessReason::Willing ||
			reason == CampaignAimWillingnessReason::BuddyOverride ||
			reason == CampaignAimWillingnessReason::ToleratedHatred;
	}

	constexpr bool waitForDismissal() const noexcept { return !accepted(); }
};

inline constexpr std::uint8_t FirstCampaignAimBuddy(
	const CampaignAimWillingnessInput& input) noexcept
{
	for (std::uint8_t buddy = 0; buddy < CampaignAimAllRelations; ++buddy)
		if (input.buddiesAliveOnTeam[buddy] &&
			(buddy < CampaignAimOrdinaryRelations || input.learnedLikeCount <= 0))
			return buddy;
	return CampaignAimNoRelation;
}

// Preserve the native early-return order: morale, first present foe with a
// buddy/tolerance override, then death rate and reputation if no buddy exists.
inline constexpr CampaignAimWillingnessDecision DecideCampaignAimWillingness(
	const CampaignAimWillingnessInput& input) noexcept
{
	using Reason = CampaignAimWillingnessReason;
	if (input.moraleHangover) return {Reason::MoraleHangover};
	const auto buddy = FirstCampaignAimBuddy(input);
	for (std::uint8_t hated = 0; hated < CampaignAimAllRelations; ++hated)
	{
		if (!input.hatedAliveOnTeam[hated] ||
			(hated == CampaignAimOrdinaryRelations && input.learnedHateCount > 0))
			continue;
		if (buddy != CampaignAimNoRelation) return {Reason::BuddyOverride, buddy};
		if (hated == CampaignAimOrdinaryRelations) return {Reason::LearnedHatred, hated};
		return {input.hatedToleranceHours[hated] >= 24 ? Reason::ToleratedHatred :
			Reason::HatedMerc, hated};
	}
	if (buddy == CampaignAimNoRelation)
	{
		if (input.deathRateTooHigh) return {Reason::DeathRate};
		if (input.reputationTooBad) return {Reason::Reputation};
	}
	return {};
}

#endif
