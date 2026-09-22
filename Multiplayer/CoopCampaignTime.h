#ifndef MULTIPLAYER_COOP_CAMPAIGN_TIME_H
#define MULTIPLAYER_COOP_CAMPAIGN_TIME_H

#include "CoopCampaignStatus.h"

namespace CoopSession
{
inline constexpr const char* CoopCampaignTimeRequestMessageName = "coop.campaign.time.request";
inline constexpr const char* CoopCampaignTimeResultMessageName = "coop.campaign.time.result";
inline constexpr std::size_t CoopCampaignTimeRequestWireSize = 32;
inline constexpr std::size_t CoopCampaignTimeResultWireSize = 40;
// Absolute settings only: no toggle, arbitrary rate, clock warp, or tactical pause.
enum class CoopCampaignTimeAction : std::uint8_t { Pause = 1, FiveMinutes = 2, ThirtyMinutes = 3, SixtyMinutes = 4 };
enum class CoopCampaignTimeOutcome : std::uint8_t
{
	Applied = 1, NotLeader = 2, Unavailable = 3, Stale = 4, NativeBlocked = 5, NotReady = 6
};
struct CoopCampaignTimeRequest
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, requestId = 0;
	CoopCampaignTimeAction action = CoopCampaignTimeAction::Pause;
};
struct CoopCampaignTimeResult
{
	CoopCampaignTimeRequest request;
	std::uint64_t controlRevision = 0;
	CoopCampaignTimeOutcome outcome = CoopCampaignTimeOutcome::Unavailable;
};
using CoopCampaignTimeRequestBytes = std::array<std::uint8_t, CoopCampaignTimeRequestWireSize>;
using CoopCampaignTimeResultBytes = std::array<std::uint8_t, CoopCampaignTimeResultWireSize>;
inline bool ValidCoopCampaignTimeAction(CoopCampaignTimeAction action) noexcept
{
	return static_cast<unsigned>(action) >= 1 && static_cast<unsigned>(action) <= 4;
}
inline bool ValidCoopCampaignTimeRequest(const CoopCampaignTimeRequest& request) noexcept
{
	return request.sessionEpoch && request.controlRevision && request.requestId && ValidCoopCampaignTimeAction(request.action);
}
inline bool SameCoopCampaignTimeRequest(const CoopCampaignTimeRequest& a, const CoopCampaignTimeRequest& b) noexcept
{
	return a.sessionEpoch == b.sessionEpoch && a.controlRevision == b.controlRevision &&
		a.requestId == b.requestId && a.action == b.action;
}
inline void PutCoopCampaignTimeWord(std::uint8_t* bytes, std::uint64_t word, unsigned count) noexcept
{
	for (unsigned i = 0; i < count; ++i) bytes[i] = static_cast<std::uint8_t>(word >> (8 * i));
}
inline std::uint64_t GetCoopCampaignTimeWord(const std::uint8_t* bytes, unsigned count) noexcept
{
	std::uint64_t word = 0;
	for (unsigned i = 0; i < count; ++i) word |= static_cast<std::uint64_t>(bytes[i]) << (8 * i);
	return word;
}
inline bool EncodeCoopCampaignTimeRequest(const CoopCampaignTimeRequest& request, CoopCampaignTimeRequestBytes& output) noexcept
{
	if (!ValidCoopCampaignTimeRequest(request)) return false;
	CoopCampaignTimeRequestBytes bytes{{'J','2','T','Q'}};
	PutCoopCampaignTimeWord(bytes.data() + 4, CurrentProtocolVersion, 2);
	bytes[6] = static_cast<std::uint8_t>(request.action);
	PutCoopCampaignTimeWord(bytes.data() + 8, request.sessionEpoch, 8);
	PutCoopCampaignTimeWord(bytes.data() + 16, request.controlRevision, 8);
	PutCoopCampaignTimeWord(bytes.data() + 24, request.requestId, 8);
	output = bytes;
	return true;
}
inline bool DecodeCoopCampaignTimeRequest(const std::uint8_t* bytes, std::size_t size, CoopCampaignTimeRequest& output) noexcept
{
	if (!bytes || size != CoopCampaignTimeRequestWireSize || bytes[0] != 'J' || bytes[1] != '2' ||
		bytes[2] != 'T' || bytes[3] != 'Q' || bytes[7] || GetCoopCampaignTimeWord(bytes + 4, 2) != CurrentProtocolVersion) return false;
	CoopCampaignTimeRequest value;
	value.action = static_cast<CoopCampaignTimeAction>(bytes[6]);
	value.sessionEpoch = GetCoopCampaignTimeWord(bytes + 8, 8);
	value.controlRevision = GetCoopCampaignTimeWord(bytes + 16, 8);
	value.requestId = GetCoopCampaignTimeWord(bytes + 24, 8);
	if (!ValidCoopCampaignTimeRequest(value)) return false;
	output = value;
	return true;
}
inline bool EncodeCoopCampaignTimeResult(const CoopCampaignTimeResult& result, CoopCampaignTimeResultBytes& output) noexcept
{
	const auto outcome = static_cast<unsigned>(result.outcome);
	CoopCampaignTimeRequestBytes request;
	if (!EncodeCoopCampaignTimeRequest(result.request, request) || !result.controlRevision || outcome < 1 || outcome > 6 ||
		(result.outcome == CoopCampaignTimeOutcome::Applied && result.controlRevision <= result.request.controlRevision)) return false;
	CoopCampaignTimeResultBytes bytes{};
	std::copy(request.begin(), request.end(), bytes.begin());
	bytes[3] = 'R'; bytes[7] = static_cast<std::uint8_t>(result.outcome);
	PutCoopCampaignTimeWord(bytes.data() + 32, result.controlRevision, 8);
	output = bytes;
	return true;
}
inline bool DecodeCoopCampaignTimeResult(const std::uint8_t* bytes, std::size_t size, CoopCampaignTimeResult& output) noexcept
{
	if (!bytes || size != CoopCampaignTimeResultWireSize || bytes[3] != 'R' || bytes[7] < 1 || bytes[7] > 6) return false;
	CoopCampaignTimeRequestBytes request;
	std::copy(bytes, bytes + request.size(), request.begin());
	request[3] = 'Q'; request[7] = 0;
	CoopCampaignTimeResult value;
	value.outcome = static_cast<CoopCampaignTimeOutcome>(bytes[7]);
	value.controlRevision = GetCoopCampaignTimeWord(bytes + 32, 8);
	if (!DecodeCoopCampaignTimeRequest(request.data(), request.size(), value.request) || !value.controlRevision ||
		(value.outcome == CoopCampaignTimeOutcome::Applied && value.controlRevision <= value.request.controlRevision)) return false;
	output = value;
	return true;
}

// The caller supplies the transport-authenticated identity and readiness from
// BOTH the pre-poll gate and the current campaign synchronizer. This grants no
// other campaign or mercenary privileges. Applied here means eligible to try
// the native guard, not yet executed.
inline CoopCampaignTimeOutcome ValidateCoopCampaignTimeRequest(const CoopCampaignTimeRequest& request,
	const CoopCampaignStatus& status, const PeerIdentity& peer, bool ready, bool worldlessStrategic) noexcept
{
	if (!ready || IsZero(peer)) return CoopCampaignTimeOutcome::NotReady;
	if (!ValidCoopCampaignTimeRequest(request) || !ValidCoopCampaignStatus(status) || request.sessionEpoch != status.sessionEpoch ||
		request.controlRevision != status.timeControlRevision) return CoopCampaignTimeOutcome::Stale;
	if (peer != status.timeLeader || !status.timeLeaderReady) return CoopCampaignTimeOutcome::NotLeader;
	if (status.phase != CoopCampaignPhase::Strategic || !worldlessStrategic) return CoopCampaignTimeOutcome::Unavailable;
	if (status.arrival.decision) return CoopCampaignTimeOutcome::NativeBlocked;
	return CoopCampaignTimeOutcome::Applied;
}
}
#endif
