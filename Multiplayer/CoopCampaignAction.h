#ifndef MULTIPLAYER_COOP_CAMPAIGN_ACTION_H
#define MULTIPLAYER_COOP_CAMPAIGN_ACTION_H

#include "CoopCampaignGroups.h"
#include "CoopCampaignStatus.h"

namespace CoopSession
{
inline constexpr const char* CoopCampaignActionRequestMessageName = "coop.campaign.action.request";
inline constexpr const char* CoopCampaignActionResultMessageName = "coop.campaign.action.result";
inline constexpr std::size_t CoopCampaignActionRequestWireSize = 56;
inline constexpr std::size_t CoopCampaignActionResultWireSize = 80;
enum class CoopCampaignAction : std::uint8_t
{
	Travel = 1, AcknowledgeArrival = 2, StopArrival = 3,
	EnterArrivalForced = 4, EnterArrivalSpread = 5, RetreatArrival = 6,
	DeclineSurrender = 7, AcceptSurrender = 8, AcknowledgeBattleNotice = 9, SkipMeanwhile = 10,
	ReturnToMap = 11, EnterSector = 12, TakeExitGrid = 13,
	// One merc's assignment on the strategic map: decision packs the merc
	// (slot | incarnation << 16), destinationX the co-op assignment code (1-8),
	// destinationY its parameter.
	SetAssignment = 14,
	// Contracts: decision packs the merc as for SetAssignment. ExtendContract's
	// destinationX is the length (1 = one day, 2 = one week, 3 = two weeks).
	ExtendContract = 15, DismissMerc = 16,
	// The time controls: the leader hands them to another ready player, or a
	// ready player takes them while the leader is offline. Carries no fields.
	TransferTimeLead = 17
};
enum class CoopCampaignActionOutcome : std::uint8_t
{
	Applied = 1, NotReady = 2, Unauthorized = 3, Stale = 4,
	Unavailable = 5, NativeRejected = 6, Unsupported = 7, Failed = 8
};
struct CoopCampaignActionRequest
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, groupsRevision = 0, requestId = 0, decision = 0;
	StrategicGroupId group{};
	std::uint8_t destinationX = 0, destinationY = 0;
	CoopCampaignAction action = CoopCampaignAction::Travel;
};
struct CoopCampaignActionResult
{
	CoopCampaignActionRequest request;
	std::uint64_t controlRevision = 0, groupsRevision = 0;
	CoopCampaignActionOutcome outcome = CoopCampaignActionOutcome::Unavailable;
	// Action-specific native enum, captured at execution rather than inferred
	// from a later state. Policy/readiness/stale/unavailable outcomes use zero.
	std::uint16_t nativeDetail = 0;
};
struct CoopCampaignActionNativeResult
{
	CoopCampaignActionOutcome outcome = CoopCampaignActionOutcome::Failed;
	std::uint16_t nativeDetail = 0;
};
using CoopCampaignActionRequestBytes = std::array<std::uint8_t, CoopCampaignActionRequestWireSize>;
using CoopCampaignActionResultBytes = std::array<std::uint8_t, CoopCampaignActionResultWireSize>;

inline bool ValidCoopCampaignAction(CoopCampaignAction action) noexcept
{
	return action >= CoopCampaignAction::Travel && action <= CoopCampaignAction::TransferTimeLead;
}
inline bool ValidCoopCampaignActionRequest(const CoopCampaignActionRequest& request) noexcept
{
	if (!request.sessionEpoch || !request.controlRevision || !request.groupsRevision || !request.requestId ||
		!ValidCoopCampaignAction(request.action)) return false;
	// Travel names a destination; EnterSector names the group's own sector as an
	// explicit confirmation of what the client displayed.
	if (request.action == CoopCampaignAction::Travel || request.action == CoopCampaignAction::EnterSector)
		return !request.decision && request.group.valid() && ValidCoopCampaignSector(request.destinationX, request.destinationY);
	// An exit grid moves one exact group out of the exact secured sector.
	if (request.action == CoopCampaignAction::TakeExitGrid)
		return request.decision && request.group.valid() && !request.destinationX && !request.destinationY;
	const bool packedMerc = (request.decision & 0xFFFFu) != 0xFFFFu && (request.decision >> 16) != 0 &&
		(request.decision >> 48) == 0 && !request.group.slot && !request.group.incarnation;
	if (request.action == CoopCampaignAction::SetAssignment)
		return packedMerc && request.destinationX >= 1 && request.destinationX <= 8;
	if (request.action == CoopCampaignAction::ExtendContract)
		return packedMerc && request.destinationX >= 1 && request.destinationX <= 3 && !request.destinationY;
	if (request.action == CoopCampaignAction::DismissMerc)
		return packedMerc && !request.destinationX && !request.destinationY;
	if (request.action == CoopCampaignAction::TransferTimeLead)
		return !request.decision && !request.group.slot && !request.group.incarnation &&
			!request.destinationX && !request.destinationY;
	return request.decision && !request.group.slot && !request.group.incarnation && !request.destinationX && !request.destinationY;
}
inline bool SameCoopCampaignActionRequest(const CoopCampaignActionRequest& a, const CoopCampaignActionRequest& b) noexcept
{
	return a.sessionEpoch == b.sessionEpoch && a.controlRevision == b.controlRevision && a.groupsRevision == b.groupsRevision &&
		a.requestId == b.requestId && a.decision == b.decision && a.group == b.group && a.destinationX == b.destinationX &&
		a.destinationY == b.destinationY && a.action == b.action;
}
inline void PutCoopCampaignActionWord(std::uint8_t* bytes, std::uint64_t word, unsigned count) noexcept
{
	for (unsigned i = 0; i < count; ++i) bytes[i] = static_cast<std::uint8_t>(word >> (8 * i));
}
inline std::uint64_t GetCoopCampaignActionWord(const std::uint8_t* bytes, unsigned count) noexcept
{
	std::uint64_t word = 0;
	for (unsigned i = 0; i < count; ++i) word |= static_cast<std::uint64_t>(bytes[i]) << (8 * i);
	return word;
}
inline bool EncodeCoopCampaignActionRequest(const CoopCampaignActionRequest& request, CoopCampaignActionRequestBytes& output) noexcept
{
	if (!ValidCoopCampaignActionRequest(request)) return false;
	CoopCampaignActionRequestBytes bytes{{'J','2','A','Q'}};
	PutCoopCampaignActionWord(bytes.data() + 4, CurrentProtocolVersion, 2);
	bytes[6] = static_cast<std::uint8_t>(request.action);
	PutCoopCampaignActionWord(bytes.data() + 8, request.sessionEpoch, 8);
	PutCoopCampaignActionWord(bytes.data() + 16, request.controlRevision, 8);
	PutCoopCampaignActionWord(bytes.data() + 24, request.groupsRevision, 8);
	PutCoopCampaignActionWord(bytes.data() + 32, request.requestId, 8);
	PutCoopCampaignActionWord(bytes.data() + 40, request.decision, 8);
	bytes[48] = request.group.slot; bytes[49] = request.destinationX; bytes[50] = request.destinationY;
	PutCoopCampaignActionWord(bytes.data() + 52, request.group.incarnation, 4);
	output = bytes;
	return true;
}
inline bool DecodeCoopCampaignActionRequest(const std::uint8_t* bytes, std::size_t size, CoopCampaignActionRequest& output) noexcept
{
	if (!bytes || size != CoopCampaignActionRequestWireSize || bytes[0] != 'J' || bytes[1] != '2' || bytes[2] != 'A' ||
		bytes[3] != 'Q' || bytes[7] || bytes[51] || GetCoopCampaignActionWord(bytes + 4, 2) != CurrentProtocolVersion) return false;
	CoopCampaignActionRequest value;
	value.action = static_cast<CoopCampaignAction>(bytes[6]);
	value.sessionEpoch = GetCoopCampaignActionWord(bytes + 8, 8);
	value.controlRevision = GetCoopCampaignActionWord(bytes + 16, 8);
	value.groupsRevision = GetCoopCampaignActionWord(bytes + 24, 8);
	value.requestId = GetCoopCampaignActionWord(bytes + 32, 8);
	value.decision = GetCoopCampaignActionWord(bytes + 40, 8);
	value.group = {bytes[48], static_cast<std::uint32_t>(GetCoopCampaignActionWord(bytes + 52, 4))};
	value.destinationX = bytes[49]; value.destinationY = bytes[50];
	if (!ValidCoopCampaignActionRequest(value)) return false;
	output = value;
	return true;
}
inline bool ValidCoopCampaignActionResult(const CoopCampaignActionResult& result) noexcept
{
	return ValidCoopCampaignActionRequest(result.request) && result.controlRevision && result.groupsRevision &&
		result.outcome >= CoopCampaignActionOutcome::Applied && result.outcome <= CoopCampaignActionOutcome::Failed &&
		(result.outcome != CoopCampaignActionOutcome::Applied || result.controlRevision > result.request.controlRevision) &&
		(result.outcome != CoopCampaignActionOutcome::NativeRejected || result.controlRevision > result.request.controlRevision) &&
		(!result.nativeDetail || result.outcome == CoopCampaignActionOutcome::Applied ||
		 result.outcome == CoopCampaignActionOutcome::NativeRejected || result.outcome == CoopCampaignActionOutcome::Unsupported ||
		 result.outcome == CoopCampaignActionOutcome::Failed);
}
inline bool EncodeCoopCampaignActionResult(const CoopCampaignActionResult& result, CoopCampaignActionResultBytes& output) noexcept
{
	if (!ValidCoopCampaignActionResult(result)) return false;
	CoopCampaignActionRequestBytes request;
	if (!EncodeCoopCampaignActionRequest(result.request, request)) return false;
	CoopCampaignActionResultBytes bytes{};
	std::copy(request.begin(), request.end(), bytes.begin());
	bytes[3] = 'R'; bytes[7] = static_cast<std::uint8_t>(result.outcome);
	PutCoopCampaignActionWord(bytes.data() + 56, result.controlRevision, 8);
	PutCoopCampaignActionWord(bytes.data() + 64, result.groupsRevision, 8);
	PutCoopCampaignActionWord(bytes.data() + 72, result.nativeDetail, 2);
	output = bytes;
	return true;
}
inline bool DecodeCoopCampaignActionResult(const std::uint8_t* bytes, std::size_t size, CoopCampaignActionResult& output) noexcept
{
	if (!bytes || size != CoopCampaignActionResultWireSize || bytes[3] != 'R') return false;
	for (std::size_t i = 74; i < CoopCampaignActionResultWireSize; ++i) if (bytes[i]) return false;
	CoopCampaignActionRequestBytes request;
	std::copy(bytes, bytes + request.size(), request.begin());
	request[3] = 'Q'; request[7] = 0;
	CoopCampaignActionResult value;
	value.outcome = static_cast<CoopCampaignActionOutcome>(bytes[7]);
	value.controlRevision = GetCoopCampaignActionWord(bytes + 56, 8);
	value.groupsRevision = GetCoopCampaignActionWord(bytes + 64, 8);
	value.nativeDetail = static_cast<std::uint16_t>(GetCoopCampaignActionWord(bytes + 72, 2));
	if (!DecodeCoopCampaignActionRequest(request.data(), request.size(), value.request) || !ValidCoopCampaignActionResult(value)) return false;
	output = value;
	return true;
}

// The transport supplies identity; authorization is an explicit campaign policy
// decision, independent of both admission readiness and time leadership. Applied
// here means eligible for fresh native validation, never an executed command.
inline CoopCampaignActionOutcome ValidateCoopCampaignActionRequest(const CoopCampaignActionRequest& request,
	const CoopCampaignStatus& status, const CoopCampaignGroups& groups,
	bool ready, bool authorized, bool worldlessStrategic) noexcept
{
	using Outcome = CoopCampaignActionOutcome;
	if (!ready) return Outcome::NotReady;
	if (!authorized) return Outcome::Unauthorized;
	if (!ValidCoopCampaignActionRequest(request) || !ValidCoopCampaignStatus(status) || !ValidCoopCampaignGroups(groups) ||
		request.sessionEpoch != status.sessionEpoch || request.sessionEpoch != groups.sessionEpoch ||
		request.controlRevision != status.timeControlRevision || request.groupsRevision != groups.revision) return Outcome::Stale;
	// Who may move the time controls depends on the requesting peer; the
	// peer-aware overload below decides that.
	if (request.action == CoopCampaignAction::TransferTimeLead) return Outcome::Applied;
	if (request.action == CoopCampaignAction::SkipMeanwhile)
		return status.meanwhile.id && request.decision == status.meanwhile.id ? Outcome::Applied : Outcome::Stale;
	if (status.meanwhile.id) return Outcome::Unavailable;
	// Any ready player may end peaceful play in a secured sector, exactly like
	// ordering shared travel. Pending battle decisions keep the sector open.
	if (request.action == CoopCampaignAction::ReturnToMap)
	{
		if (status.phase != CoopCampaignPhase::Tactical || !status.securedSector ||
			status.surrenderOffer || status.battleNotice.id) return Outcome::Unavailable;
		return request.decision == status.securedSector ? Outcome::Applied : Outcome::Stale;
	}
	if (request.action == CoopCampaignAction::TakeExitGrid)
	{
		if (status.phase != CoopCampaignPhase::Tactical || !status.securedSector ||
			status.surrenderOffer || status.battleNotice.id || !groups.available) return Outcome::Unavailable;
		if (request.decision != status.securedSector) return Outcome::Stale;
		for (std::size_t i = 0; i < groups.groupCount; ++i)
		{
			const auto& group = groups.groups[i];
			if (group.id != request.group) continue;
			if (group.betweenSectors || group.destinationX) return Outcome::Unavailable;
			// Native rules decide whether a member stands on an exit grid.
			return group.vehicle ? Outcome::Unsupported : Outcome::Applied;
		}
		return Outcome::Stale;
	}
	if (request.action == CoopCampaignAction::AcknowledgeBattleNotice)
	{
		if (status.phase != CoopCampaignPhase::Tactical) return Outcome::Unavailable;
		return request.decision == status.battleNotice.id ? Outcome::Applied : Outcome::Stale;
	}
	if (request.action == CoopCampaignAction::DeclineSurrender || request.action == CoopCampaignAction::AcceptSurrender)
	{
		if (status.phase != CoopCampaignPhase::Tactical) return Outcome::Unavailable;
		return request.decision == status.surrenderOffer ? Outcome::Applied : Outcome::Stale;
	}
	if (status.phase != CoopCampaignPhase::Strategic || !worldlessStrategic || status.surrenderOffer) return Outcome::Unavailable;
	if (request.action == CoopCampaignAction::EnterSector)
	{
		// Load the group's own player-held sector for peaceful play, including an
		// underground level. Native rules decide whether it is free of hostiles.
		if (!groups.available || status.arrival.decision) return Outcome::Unavailable;
		for (std::size_t i = 0; i < groups.groupCount; ++i)
		{
			const auto& group = groups.groups[i];
			if (group.id != request.group) continue;
			if (group.betweenSectors || group.destinationX) return Outcome::Unavailable;
			if (group.vehicle) return Outcome::Unsupported;
			return group.x == request.destinationX && group.y == request.destinationY ? Outcome::Applied : Outcome::Stale;
		}
		return Outcome::Stale;
	}
	// Any ready player may reassign any merc on the strategic map, as with
	// shared travel; the native gates decide whether that merc can take it.
	if (request.action == CoopCampaignAction::SetAssignment || request.action == CoopCampaignAction::ExtendContract ||
		request.action == CoopCampaignAction::DismissMerc)
		return status.arrival.decision ? Outcome::Unavailable : Outcome::Applied;
	if (request.action == CoopCampaignAction::Travel)
	{
		if (!groups.available || status.arrival.decision) return Outcome::Unavailable;
		for (std::size_t i = 0; i < groups.groupCount; ++i)
		{
			const auto& group = groups.groups[i];
			if (group.id != request.group) continue;
			if (group.betweenSectors) return Outcome::Unavailable;
			if (group.z || group.vehicle) return Outcome::Unsupported;
			const int dx = static_cast<int>(request.destinationX) - group.x;
			const int dy = static_cast<int>(request.destinationY) - group.y;
			return ((dx == 0 && (dy == 1 || dy == -1)) || (dy == 0 && (dx == 1 || dx == -1))) ? Outcome::Applied : Outcome::Unsupported;
		}
		return Outcome::Stale;
	}
	const auto& arrival = status.arrival;
	if (request.decision != arrival.decision) return Outcome::Stale;
	if (arrival.stage == CoopCampaignArrivalStage::Failed) return Outcome::Failed;
	if (request.action == CoopCampaignAction::AcknowledgeArrival || request.action == CoopCampaignAction::StopArrival)
		return arrival.kind == CoopCampaignArrivalKind::WildernessNpc && arrival.stage == CoopCampaignArrivalStage::Pending &&
			(request.action == CoopCampaignAction::StopArrival || arrival.finalDestination) ? Outcome::Applied : Outcome::Unsupported;
	if (arrival.kind != CoopCampaignArrivalKind::Battle || arrival.stage != CoopCampaignArrivalStage::Prepared) return Outcome::Unsupported;
	if (arrival.pendingCount != 1) return Outcome::Unavailable;
	if (request.action == CoopCampaignAction::RetreatArrival)
		return arrival.nativeRetreat ? Outcome::Applied : Outcome::Unsupported;
	if (!arrival.nativeEnterSector) return Outcome::Unsupported;
	// Forced insertion is available only when native rules require no deployment;
	// an explicit Spread choice handles the supported native deployment path.
	if (request.action == CoopCampaignAction::EnterArrivalForced && arrival.nativePlacement) return Outcome::Unsupported;
	if (request.action == CoopCampaignAction::EnterArrivalSpread && !arrival.nativePlacement) return Outcome::Unsupported;
	return Outcome::Applied;
}
inline CoopCampaignActionOutcome ValidateCoopCampaignActionRequest(const CoopCampaignActionRequest& request,
	const CoopCampaignStatus& status, const CoopCampaignGroups& groups, const PeerIdentity& peer,
	bool ready, bool authorized, bool worldlessStrategic) noexcept
{
	const auto outcome = ValidateCoopCampaignActionRequest(request, status, groups, ready && !IsZero(peer), authorized,
		worldlessStrategic);
	if (outcome != CoopCampaignActionOutcome::Applied || request.action != CoopCampaignAction::TransferTimeLead) return outcome;
	// The leader may hand the controls to another ready player; anyone else may
	// take them only while the leader is not ready (offline or retired).
	if (peer == status.timeLeader) return status.readyPeers >= 2 ? outcome : CoopCampaignActionOutcome::Unavailable;
	return status.timeLeaderReady ? CoopCampaignActionOutcome::Unauthorized : outcome;
}
}
#endif
