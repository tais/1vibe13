#ifndef MULTIPLAYER_COOP_CAMPAIGN_ARRIVAL_H
#define MULTIPLAYER_COOP_CAMPAIGN_ARRIVAL_H

#include <cstdint>

namespace CoopSession
{
enum class CoopCampaignArrivalKind : std::uint8_t
{
	None, WildernessNpc, CoordinateAttack, Battle
};
enum class CoopCampaignArrivalStage : std::uint8_t
{
	None, Pending, Prepared, ReinforcementsRequired, Unsupported, Failed
};

// The front of the server's native decision queue, not a command, an enemy
// roster or an authorization grant. Native choices describe the preparation
// result; each eventual action still needs policy and fresh native validation.
struct CoopCampaignArrival
{
	std::uint64_t decision = 0;
	CoopCampaignArrivalKind kind = CoopCampaignArrivalKind::None;
	CoopCampaignArrivalStage stage = CoopCampaignArrivalStage::None;
	std::uint8_t x = 0, y = 0, z = 0, encounterCode = 0;
	std::uint16_t pendingCount = 0, involvedMercs = 0, uninvolvedMercs = 0;
	bool finalDestination = false;
	bool nativeAutoResolve = false, nativeEnterSector = false, nativeRetreat = false, nativePlacement = false;
};

inline bool SameCoopCampaignArrival(const CoopCampaignArrival& a, const CoopCampaignArrival& b) noexcept
{
	return a.decision == b.decision && a.kind == b.kind && a.stage == b.stage &&
		a.x == b.x && a.y == b.y && a.z == b.z && a.encounterCode == b.encounterCode &&
		a.pendingCount == b.pendingCount && a.involvedMercs == b.involvedMercs && a.uninvolvedMercs == b.uninvolvedMercs &&
		a.finalDestination == b.finalDestination && a.nativeAutoResolve == b.nativeAutoResolve &&
		a.nativeEnterSector == b.nativeEnterSector && a.nativeRetreat == b.nativeRetreat && a.nativePlacement == b.nativePlacement;
}

inline bool ValidCoopCampaignArrival(const CoopCampaignArrival& value) noexcept
{
	if (!value.decision) return SameCoopCampaignArrival(value, {});
	if (value.kind < CoopCampaignArrivalKind::WildernessNpc || value.kind > CoopCampaignArrivalKind::Battle ||
		value.stage < CoopCampaignArrivalStage::Pending || value.stage > CoopCampaignArrivalStage::Failed ||
		value.x < 1 || value.x > 16 || value.y < 1 || value.y > 16 || value.z > 3 ||
		!value.pendingCount || value.pendingCount > 256 ||
		(value.finalDestination && value.kind != CoopCampaignArrivalKind::WildernessNpc)) return false;
	if (value.kind != CoopCampaignArrivalKind::Battle && (value.encounterCode ||
		value.stage == CoopCampaignArrivalStage::Prepared || value.stage == CoopCampaignArrivalStage::ReinforcementsRequired)) return false;
	if (value.stage == CoopCampaignArrivalStage::Prepared)
		return value.involvedMercs && value.involvedMercs + value.uninvolvedMercs <= 256 &&
			(!value.nativePlacement || value.nativeEnterSector);
	return !value.involvedMercs && !value.uninvolvedMercs && !value.nativeAutoResolve &&
		!value.nativeEnterSector && !value.nativeRetreat && !value.nativePlacement;
}

// IDs are monotonically assigned by the native queue. Once cleared, an old
// decision must not reappear under a newer status revision. Retain a high-water
// mark through no-decision observations; reset it only with the transport/session.
inline bool ValidCoopCampaignArrivalReplacement(const CoopCampaignArrival& previous,
	const CoopCampaignArrival& candidate, std::uint64_t lastDecision) noexcept
{
	if (!ValidCoopCampaignArrival(candidate)) return false;
	if (!candidate.decision || candidate.decision > lastDecision) return true;
	return candidate.decision == lastDecision && candidate.decision == previous.decision &&
		candidate.kind == previous.kind && candidate.x == previous.x && candidate.y == previous.y &&
		candidate.z == previous.z && candidate.finalDestination == previous.finalDestination;
}
}
#endif
