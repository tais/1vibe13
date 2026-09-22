#ifndef MULTIPLAYER_COOP_CAMPAIGN_STATUS_H
#define MULTIPLAYER_COOP_CAMPAIGN_STATUS_H

#include "CoopSessionProtocol.h"
#include "CoopCampaignArrival.h"

#include <algorithm>
#include <limits>

namespace CoopSession
{
inline constexpr const char* CoopCampaignStatusMessageName = "coop.campaign.status";
inline constexpr std::size_t CoopCampaignStatusWireSize = 96;
inline constexpr std::size_t MaximumCampaignStatusReadyPeers = 4;
enum class CoopCampaignPhase : std::uint8_t
{
	Starting = 1, Strategic = 2, Tactical = 3, Transition = 4
};

// Observations, not clock commands. A remembered compression mode does not
// prove that native time is advancing: combat/events can stop the scheduler.
struct CoopCampaignStatus
{
	std::uint64_t sessionEpoch = 0;
	std::uint64_t revision = 0;
	std::uint32_t worldSeconds = 0;
	CoopCampaignPhase phase = CoopCampaignPhase::Starting;
	std::int8_t compressionMode = 0;
	bool gamePaused = true;
	bool pauseLocked = false;
	bool compressionActive = false;
	bool timeInterrupted = false;
	PeerIdentity timeLeader{};
	std::uint64_t leadershipRevision = 0;
	bool timeLeaderReady = false;
	std::uint8_t readyPeers = 0;
	// Changes on clock-control state transitions and consumed time requests,
	// not on ordinary clock ticks. An old resume must never undo a newer pause.
	std::uint64_t timeControlRevision = 1;
	CoopCampaignArrival arrival{};
};
using CoopCampaignStatusBytes = std::array<std::uint8_t, CoopCampaignStatusWireSize>;

inline bool ValidCoopCampaignStatus(const CoopCampaignStatus& value) noexcept
{
	const auto phase = static_cast<unsigned>(value.phase);
	return value.sessionEpoch != 0 && value.revision != 0 && value.timeControlRevision != 0 && phase >= 1 && phase <= 4 &&
		value.compressionMode >= -1 && value.compressionMode <= 5 &&
		value.readyPeers <= MaximumCampaignStatusReadyPeers &&
		(IsZero(value.timeLeader) == (value.leadershipRevision == 0)) &&
		(!value.timeLeaderReady || (!IsZero(value.timeLeader) && value.readyPeers != 0)) &&
		(!value.compressionActive || (!value.gamePaused && value.compressionMode != 0)) &&
		ValidCoopCampaignArrival(value.arrival) && (!value.arrival.decision ||
			(value.gamePaused && !value.compressionActive &&
			 (value.phase == CoopCampaignPhase::Starting || value.phase == CoopCampaignPhase::Strategic)));
}

inline bool SameCoopCampaignTimeState(const CoopCampaignStatus& a, const CoopCampaignStatus& b) noexcept
{
	return a.phase == b.phase && a.compressionMode == b.compressionMode &&
		a.gamePaused == b.gamePaused && a.pauseLocked == b.pauseLocked &&
		a.compressionActive == b.compressionActive && a.timeInterrupted == b.timeInterrupted &&
		a.timeLeader == b.timeLeader && a.leadershipRevision == b.leadershipRevision &&
		a.timeLeaderReady == b.timeLeaderReady && SameCoopCampaignArrival(a.arrival, b.arrival);
}

inline bool SameCoopCampaignStatus(const CoopCampaignStatus& a, const CoopCampaignStatus& b) noexcept
{
	return SameCoopCampaignTimeState(a, b) && a.sessionEpoch == b.sessionEpoch && a.revision == b.revision &&
		a.worldSeconds == b.worldSeconds && a.readyPeers == b.readyPeers &&
		a.timeControlRevision == b.timeControlRevision;
}

// Fixed little-endian record. Transactional output and explicit reserved bytes
// keep malformed/partial data from replacing the last coherent observation.
inline bool EncodeCoopCampaignStatus(const CoopCampaignStatus& value, CoopCampaignStatusBytes& output) noexcept
{
	if (!ValidCoopCampaignStatus(value)) return false;
	CoopCampaignStatusBytes bytes{};
	bytes[0] = 'J'; bytes[1] = '2'; bytes[2] = 'C'; bytes[3] = 'T';
	const auto put = [&](std::size_t at, std::uint64_t number, unsigned count) {
		for (unsigned i = 0; i < count; ++i) bytes[at + i] = static_cast<std::uint8_t>(number >> (8 * i));
	};
	put(4, CurrentProtocolVersion, 2);
	bytes[6] = 1;
	put(8, value.sessionEpoch, 8); put(16, value.revision, 8); put(24, value.worldSeconds, 4);
	bytes[28] = static_cast<std::uint8_t>(value.phase);
	// Offset encoding avoids implementation-defined unsigned-to-signed casts.
	bytes[29] = static_cast<std::uint8_t>(value.compressionMode + 1);
	bytes[30] = (value.gamePaused ? 1 : 0) | (value.pauseLocked ? 2 : 0) |
		(value.compressionActive ? 4 : 0) | (value.timeInterrupted ? 8 : 0) | (value.timeLeaderReady ? 16 : 0);
	bytes[31] = value.readyPeers;
	std::copy(value.timeLeader.begin(), value.timeLeader.end(), bytes.begin() + 32);
	put(48, value.leadershipRevision, 8);
	put(56, value.timeControlRevision, 8);
	const auto& arrival = value.arrival;
	put(64, arrival.decision, 8);
	bytes[72] = static_cast<std::uint8_t>(arrival.kind); bytes[73] = static_cast<std::uint8_t>(arrival.stage);
	bytes[74] = (arrival.finalDestination ? 1 : 0) | (arrival.nativeAutoResolve ? 2 : 0) |
		(arrival.nativeEnterSector ? 4 : 0) | (arrival.nativeRetreat ? 8 : 0) | (arrival.nativePlacement ? 16 : 0);
	bytes[75] = arrival.x; bytes[76] = arrival.y; bytes[77] = arrival.z; bytes[78] = arrival.encounterCode;
	put(80, arrival.pendingCount, 2); put(82, arrival.involvedMercs, 2); put(84, arrival.uninvolvedMercs, 2);
	output = bytes;
	return true;
}

inline bool DecodeCoopCampaignStatus(const std::uint8_t* bytes, std::size_t size, CoopCampaignStatus& output) noexcept
{
	if (!bytes || size != CoopCampaignStatusWireSize || bytes[0] != 'J' || bytes[1] != '2' ||
		bytes[2] != 'C' || bytes[3] != 'T' || bytes[6] != 1 || bytes[7] || bytes[29] > 6 || (bytes[30] & ~31u) ||
		(bytes[74] & ~31u) || bytes[79]) return false;
	for (std::size_t i = 86; i < CoopCampaignStatusWireSize; ++i) if (bytes[i]) return false;
	const auto get = [&](std::size_t at, unsigned count) {
		std::uint64_t number = 0;
		for (unsigned i = 0; i < count; ++i) number |= static_cast<std::uint64_t>(bytes[at + i]) << (8 * i);
		return number;
	};
	if (get(4, 2) != CurrentProtocolVersion) return false;
	CoopCampaignStatus value;
	value.sessionEpoch = get(8, 8); value.revision = get(16, 8); value.worldSeconds = static_cast<std::uint32_t>(get(24, 4));
	value.phase = static_cast<CoopCampaignPhase>(bytes[28]);
	value.compressionMode = static_cast<std::int8_t>(static_cast<int>(bytes[29]) - 1);
	value.gamePaused = (bytes[30] & 1) != 0; value.pauseLocked = (bytes[30] & 2) != 0;
	value.compressionActive = (bytes[30] & 4) != 0; value.timeInterrupted = (bytes[30] & 8) != 0;
	value.timeLeaderReady = (bytes[30] & 16) != 0; value.readyPeers = bytes[31];
	std::copy(bytes + 32, bytes + 48, value.timeLeader.begin());
	value.leadershipRevision = get(48, 8);
	value.timeControlRevision = get(56, 8);
	auto& arrival = value.arrival;
	arrival.decision = get(64, 8);
	arrival.kind = static_cast<CoopCampaignArrivalKind>(bytes[72]);
	arrival.stage = static_cast<CoopCampaignArrivalStage>(bytes[73]);
	arrival.finalDestination = (bytes[74] & 1) != 0; arrival.nativeAutoResolve = (bytes[74] & 2) != 0;
	arrival.nativeEnterSector = (bytes[74] & 4) != 0; arrival.nativeRetreat = (bytes[74] & 8) != 0;
	arrival.nativePlacement = (bytes[74] & 16) != 0;
	arrival.x = bytes[75]; arrival.y = bytes[76]; arrival.z = bytes[77]; arrival.encounterCode = bytes[78];
	arrival.pendingCount = static_cast<std::uint16_t>(get(80, 2));
	arrival.involvedMercs = static_cast<std::uint16_t>(get(82, 2)); arrival.uninvolvedMercs = static_cast<std::uint16_t>(get(84, 2));
	if (!ValidCoopCampaignStatus(value)) return false;
	output = value;
	return true;
}

// Main-thread authority ledger, independent of tactical world/merc ownership.
// The first observed ready cohort selects its lowest identity on a tie. An
// offline/retired leader is retained, never silently replaced by another peer.
// This assigns no hiring, spending or tactical-command authority.
class CoopCampaignStatusLedger
{
public:
	bool beginSession(std::uint64_t epoch) noexcept
	{
		if (!epoch || value_.sessionEpoch) return false;
		value_ = {}; value_.sessionEpoch = epoch; lastArrivalDecision_ = 0;
		return true;
	}
	void clear() noexcept { value_ = {}; lastArrivalDecision_ = 0; }
	const CoopCampaignStatus& value() const noexcept { return value_; }
	bool consumeTimeControlRevision(std::uint64_t expected) noexcept
	{
		if (!value_.revision || expected != value_.timeControlRevision ||
			value_.revision == UINT64_MAX || expected == UINT64_MAX) return false;
		++value_.timeControlRevision;
		++value_.revision;
		return true;
	}
	bool observe(CoopCampaignStatus clock, const PeerIdentity* ready, std::size_t count) noexcept
	{
		if (!value_.sessionEpoch || count > MaximumCampaignStatusReadyPeers || (count && !ready)) return false;
		for (std::size_t i = 0; i < count; ++i)
			if (IsZero(ready[i]) || (i && !(ready[i - 1] < ready[i]))) return false;
		clock.sessionEpoch = value_.sessionEpoch;
		clock.revision = value_.revision ? value_.revision : 1;
		clock.timeLeader = value_.timeLeader;
		clock.leadershipRevision = value_.leadershipRevision;
		if (IsZero(clock.timeLeader) && count)
		{
			clock.timeLeader = ready[0]; clock.leadershipRevision = 1;
		}
		clock.readyPeers = static_cast<std::uint8_t>(count);
		clock.timeLeaderReady = count && std::find(ready, ready + count, clock.timeLeader) != ready + count;
		clock.timeControlRevision = value_.timeControlRevision;
		if (value_.revision && !SameCoopCampaignTimeState(value_, clock))
		{
			if (clock.timeControlRevision == UINT64_MAX) return false;
			++clock.timeControlRevision;
		}
		if (!ValidCoopCampaignStatus(clock) || (value_.revision && clock.worldSeconds < value_.worldSeconds) ||
			!ValidCoopCampaignArrivalReplacement(value_.arrival, clock.arrival, lastArrivalDecision_)) return false;
		if (value_.revision && !SameCoopCampaignStatus(value_, clock))
		{
			if (value_.revision == std::numeric_limits<std::uint64_t>::max()) return false;
			++clock.revision;
		}
		value_ = clock;
		lastArrivalDecision_ = std::max(lastArrivalDecision_, clock.arrival.decision);
		return true;
	}
private:
	CoopCampaignStatus value_;
	std::uint64_t lastArrivalDecision_ = 0;
};
}
#endif
