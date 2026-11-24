#ifndef MULTIPLAYER_COOP_CAMPAIGN_ECONOMY_H
#define MULTIPLAYER_COOP_CAMPAIGN_ECONOMY_H

#include "CoopCampaignGroups.h"
#include <limits>

namespace CoopSession
{
inline constexpr const char* CoopCampaignEconomyMessageName = "coop.campaign.economy";
inline constexpr const char* CoopCampaignAimQuotesMessageName = "coop.campaign.aim.quotes";
inline constexpr std::size_t MaximumCoopCampaignRosterActors = 260;
inline constexpr std::size_t MaximumCoopCampaignAimQuotes = 255;
inline constexpr std::size_t CoopCampaignEconomyHeaderSize = 40;
inline constexpr std::size_t CoopCampaignRosterActorWireSize = 32;
inline constexpr std::size_t MaximumCoopCampaignEconomyWireSize = CoopCampaignEconomyHeaderSize + MaximumCoopCampaignRosterActors * CoopCampaignRosterActorWireSize;
inline constexpr std::size_t CoopCampaignAimQuotesHeaderSize = 48;
inline constexpr std::size_t CoopCampaignAimQuoteWireSize = 52;
inline constexpr std::size_t MaximumCoopCampaignAimQuotesWireSize = CoopCampaignAimQuotesHeaderSize + MaximumCoopCampaignAimQuotes * CoopCampaignAimQuoteWireSize;

struct CoopCampaignRosterActor
{
	TacticalEntityId actor{};
	std::uint16_t profile = 0;
	std::int8_t assignment = 0;
	std::uint8_t x = 0, y = 0, z = 0;
	bool pendingHire = false, betweenSectors = false, inSector = false, vehicle = false;
	std::uint32_t arrivalMinutes = 0, contractEndMinutes = 0;
	StrategicGroupId group{};
};
struct CoopCampaignEconomy
{
	std::uint64_t sessionEpoch = 0, revision = 0;
	bool available = false;
	std::int32_t balance = 0;
	std::uint16_t mercenaryCount = 0, mercenaryLimit = 0, rosterCount = 0;
	std::array<CoopCampaignRosterActor, MaximumCoopCampaignRosterActors> roster{};
};
enum class CoopCampaignAimQuoteStatus : std::uint8_t
{
	Available = 1, Unavailable = 2, AlreadyHired = 3, Unwilling = 4, TeamFull = 5, Unsupported = 6
};
struct CoopCampaignAimQuote
{
	std::uint16_t profile = 0;
	CoopCampaignAimQuoteStatus status = CoopCampaignAimQuoteStatus::Unavailable;
	// Copied CampaignAimWillingnessReason value. No relation/profile traversal
	// or native willingness evaluation takes place in the protocol layer.
	std::uint8_t willingnessReason = 0;
	bool gearAvailable = false;
	std::array<std::int32_t, 3> salary{}; // 1, 7, 14 days
	std::int32_t medicalDeposit = 0, gearCost = 0;
	std::array<std::int32_t, 6> total{}; // day index * 2 + buyGear
};
struct CoopCampaignAimQuotes
{
	std::uint64_t sessionEpoch = 0, revision = 0, economyRevision = 0;
	bool available = false;
	std::uint32_t arrivalMinutes = 0;
	std::uint8_t landingX = 0, landingY = 0;
	std::uint16_t quoteCount = 0;
	std::array<CoopCampaignAimQuote, MaximumCoopCampaignAimQuotes> quotes{};
};
using CoopCampaignEconomyBytes = std::array<std::uint8_t, MaximumCoopCampaignEconomyWireSize>;
using CoopCampaignAimQuotesBytes = std::array<std::uint8_t, MaximumCoopCampaignAimQuotesWireSize>;

namespace CampaignEconomyWire
{
inline void Put(std::uint8_t* bytes, std::uint64_t value, unsigned count) noexcept
{
	for (unsigned i = 0; i < count; ++i) bytes[i] = static_cast<std::uint8_t>(value >> (8 * i));
}
inline std::uint64_t Get(const std::uint8_t* bytes, unsigned count) noexcept
{
	std::uint64_t value = 0;
	for (unsigned i = 0; i < count; ++i) value |= static_cast<std::uint64_t>(bytes[i]) << (8 * i);
	return value;
}
inline std::int32_t Signed32(const std::uint8_t* bytes) noexcept
{
	const auto value = Get(bytes, 4);
	return static_cast<std::int32_t>(value < UINT64_C(0x80000000) ? static_cast<std::int64_t>(value) :
		static_cast<std::int64_t>(value) - INT64_C(0x100000000));
}
inline bool Zero(const std::uint8_t* bytes, std::size_t size) noexcept
{
	for (std::size_t i = 0; i < size; ++i) if (bytes[i]) return false;
	return true;
}
}

inline bool ValidCoopCampaignEconomy(const CoopCampaignEconomy& value) noexcept
{
	if (!value.sessionEpoch || !value.revision || value.rosterCount > value.roster.size() ||
		value.mercenaryCount > value.rosterCount || value.mercenaryLimit > 254) return false;
	if (!value.available) return !value.balance && !value.mercenaryCount && !value.mercenaryLimit && !value.rosterCount;
	if (!value.mercenaryLimit) return false;
	std::size_t mercenaries = 0, vehicles = 0;
	for (std::size_t i = 0; i < value.rosterCount; ++i)
	{
		const auto& actor = value.roster[i];
		if (!actor.actor.valid() || actor.profile > 255 || (i && value.roster[i - 1].actor.slot >= actor.actor.slot) ||
			!ValidCoopCampaignSector(actor.x, actor.y) || actor.z > 3 ||
			(actor.inSector && (actor.pendingHire || actor.betweenSectors)) ||
			(actor.pendingHire ? !actor.arrivalMinutes : actor.arrivalMinutes != 0) || actor.contractEndMinutes > INT32_MAX ||
			((actor.group.slot != 0) != (actor.group.incarnation != 0))) return false;
		if (actor.vehicle) ++vehicles; else ++mercenaries;
	}
	return mercenaries == value.mercenaryCount && mercenaries <= 254 && vehicles <= 6;
}
inline bool ValidCoopCampaignAimQuote(const CoopCampaignAimQuote& quote) noexcept
{
	const auto status = static_cast<unsigned>(quote.status);
	if (quote.profile > 254 || status < 1 || status > 6 || quote.willingnessReason > 7) return false;
	const bool willing = quote.willingnessReason == 0 || quote.willingnessReason == 2 || quote.willingnessReason == 3;
	if (quote.status != CoopCampaignAimQuoteStatus::Available)
	{
		if (quote.status == CoopCampaignAimQuoteStatus::Unwilling ? willing : quote.willingnessReason != 0) return false;
		if (quote.gearAvailable || quote.medicalDeposit || quote.gearCost) return false;
		for (const auto value : quote.salary) if (value) return false;
		for (const auto value : quote.total) if (value) return false;
		return true;
	}
	if (!willing || quote.medicalDeposit < 0 || quote.gearCost < 0 || (!quote.gearAvailable && quote.gearCost)) return false;
	for (std::size_t i = 0; i < quote.salary.size(); ++i)
	{
		if (quote.salary[i] < 0) return false;
		const std::int64_t base = static_cast<std::int64_t>(quote.salary[i]) + quote.medicalDeposit;
		const std::int64_t withGear = base + quote.gearCost;
		if (base > INT32_MAX || withGear > INT32_MAX || quote.total[i * 2] != base ||
			quote.total[i * 2 + 1] != (quote.gearAvailable ? withGear : 0)) return false;
	}
	return true;
}
inline bool ValidCoopCampaignAimQuotes(const CoopCampaignAimQuotes& value) noexcept
{
	if (!value.sessionEpoch || !value.revision || !value.economyRevision || value.quoteCount > value.quotes.size()) return false;
	if (!value.available) return !value.arrivalMinutes && !value.landingX && !value.landingY && !value.quoteCount;
	if (!value.arrivalMinutes || !ValidCoopCampaignSector(value.landingX, value.landingY)) return false;
	for (std::size_t i = 0; i < value.quoteCount; ++i)
		if (!ValidCoopCampaignAimQuote(value.quotes[i]) || (i && value.quotes[i - 1].profile >= value.quotes[i].profile)) return false;
	return true;
}

// Content equality intentionally ignores transport/session ledger stamps.
inline bool SameCoopCampaignEconomyContent(const CoopCampaignEconomy& a, const CoopCampaignEconomy& b) noexcept
{
	if (a.available != b.available || a.balance != b.balance || a.mercenaryCount != b.mercenaryCount ||
		a.mercenaryLimit != b.mercenaryLimit || a.rosterCount != b.rosterCount || a.rosterCount > a.roster.size()) return false;
	for (std::size_t i = 0; i < a.rosterCount; ++i)
	{
		const auto& x = a.roster[i]; const auto& y = b.roster[i];
		if (x.actor != y.actor || x.profile != y.profile || x.assignment != y.assignment || x.x != y.x || x.y != y.y || x.z != y.z ||
			x.pendingHire != y.pendingHire || x.betweenSectors != y.betweenSectors || x.inSector != y.inSector || x.vehicle != y.vehicle ||
			x.arrivalMinutes != y.arrivalMinutes || x.contractEndMinutes != y.contractEndMinutes || x.group != y.group) return false;
	}
	return true;
}
inline bool SameCoopCampaignAimQuotesContent(const CoopCampaignAimQuotes& a, const CoopCampaignAimQuotes& b) noexcept
{
	if (a.available != b.available || a.arrivalMinutes != b.arrivalMinutes || a.landingX != b.landingX || a.landingY != b.landingY ||
		a.quoteCount != b.quoteCount || a.quoteCount > a.quotes.size()) return false;
	for (std::size_t i = 0; i < a.quoteCount; ++i)
	{
		const auto& x = a.quotes[i]; const auto& y = b.quotes[i];
		if (x.profile != y.profile || x.status != y.status || x.willingnessReason != y.willingnessReason ||
			x.gearAvailable != y.gearAvailable || x.salary != y.salary || x.medicalDeposit != y.medicalDeposit ||
			x.gearCost != y.gearCost || x.total != y.total) return false;
	}
	return true;
}
inline bool SameCoopCampaignEconomy(const CoopCampaignEconomy& a, const CoopCampaignEconomy& b) noexcept
{
	return a.sessionEpoch == b.sessionEpoch && a.revision == b.revision && SameCoopCampaignEconomyContent(a, b);
}
inline bool SameCoopCampaignAimQuotes(const CoopCampaignAimQuotes& a, const CoopCampaignAimQuotes& b) noexcept
{
	return a.sessionEpoch == b.sessionEpoch && a.revision == b.revision && a.economyRevision == b.economyRevision &&
		SameCoopCampaignAimQuotesContent(a, b);
}

inline bool EncodeCoopCampaignEconomy(const CoopCampaignEconomy& value, CoopCampaignEconomyBytes& output, std::size_t& size) noexcept
{
	if (!ValidCoopCampaignEconomy(value)) return false;
	using namespace CampaignEconomyWire;
	CoopCampaignEconomyBytes bytes{{'J','2','E','C'}};
	Put(bytes.data() + 4, CurrentProtocolVersion, 2); bytes[6] = 1; bytes[7] = value.available ? 1 : 0;
	Put(bytes.data() + 8, value.sessionEpoch, 8); Put(bytes.data() + 16, value.revision, 8);
	Put(bytes.data() + 24, static_cast<std::uint32_t>(value.balance), 4);
	Put(bytes.data() + 28, value.rosterCount, 2); Put(bytes.data() + 30, value.mercenaryCount, 2); Put(bytes.data() + 32, value.mercenaryLimit, 2);
	std::size_t at = CoopCampaignEconomyHeaderSize;
	for (std::size_t i = 0; i < value.rosterCount; ++i, at += CoopCampaignRosterActorWireSize)
	{
		const auto& a = value.roster[i];
		Put(bytes.data() + at, a.actor.slot, 2); Put(bytes.data() + at + 2, a.profile, 2); Put(bytes.data() + at + 4, a.actor.incarnation, 4);
		bytes[at + 8] = static_cast<std::uint8_t>(static_cast<int>(a.assignment) + 128);
		bytes[at + 9] = (a.pendingHire ? 1 : 0) | (a.betweenSectors ? 2 : 0) | (a.inSector ? 4 : 0) | (a.vehicle ? 8 : 0);
		bytes[at + 10] = a.x; bytes[at + 11] = a.y; bytes[at + 12] = a.z;
		Put(bytes.data() + at + 16, a.arrivalMinutes, 4); Put(bytes.data() + at + 20, a.contractEndMinutes, 4);
		bytes[at + 24] = a.group.slot; Put(bytes.data() + at + 28, a.group.incarnation, 4);
	}
	output = bytes; size = at; return true;
}
inline bool DecodeCoopCampaignEconomy(const std::uint8_t* bytes, std::size_t size, CoopCampaignEconomy& output) noexcept
{
	using namespace CampaignEconomyWire;
	if (!bytes || size < CoopCampaignEconomyHeaderSize || size > MaximumCoopCampaignEconomyWireSize ||
		bytes[0] != 'J' || bytes[1] != '2' || bytes[2] != 'E' || bytes[3] != 'C' ||
		Get(bytes + 4, 2) != CurrentProtocolVersion || bytes[6] != 1 || bytes[7] > 1 || !Zero(bytes + 34, 6)) return false;
	CoopCampaignEconomy value;
	value.sessionEpoch = Get(bytes + 8, 8); value.revision = Get(bytes + 16, 8); value.available = bytes[7] != 0;
	value.balance = Signed32(bytes + 24); value.rosterCount = static_cast<std::uint16_t>(Get(bytes + 28, 2));
	value.mercenaryCount = static_cast<std::uint16_t>(Get(bytes + 30, 2)); value.mercenaryLimit = static_cast<std::uint16_t>(Get(bytes + 32, 2));
	if (value.rosterCount > value.roster.size() || size != CoopCampaignEconomyHeaderSize + value.rosterCount * CoopCampaignRosterActorWireSize) return false;
	std::size_t at = CoopCampaignEconomyHeaderSize;
	for (std::size_t i = 0; i < value.rosterCount; ++i, at += CoopCampaignRosterActorWireSize)
	{
		if ((bytes[at + 9] & ~15u) || !Zero(bytes + at + 13, 3) || !Zero(bytes + at + 25, 3)) return false;
		auto& a = value.roster[i];
		a.actor = {static_cast<std::uint16_t>(Get(bytes + at, 2)), static_cast<std::uint32_t>(Get(bytes + at + 4, 4))};
		a.profile = static_cast<std::uint16_t>(Get(bytes + at + 2, 2)); a.assignment = static_cast<std::int8_t>(static_cast<int>(bytes[at + 8]) - 128);
		a.pendingHire = (bytes[at + 9] & 1) != 0; a.betweenSectors = (bytes[at + 9] & 2) != 0;
		a.inSector = (bytes[at + 9] & 4) != 0; a.vehicle = (bytes[at + 9] & 8) != 0;
		a.x = bytes[at + 10]; a.y = bytes[at + 11]; a.z = bytes[at + 12];
		a.arrivalMinutes = static_cast<std::uint32_t>(Get(bytes + at + 16, 4)); a.contractEndMinutes = static_cast<std::uint32_t>(Get(bytes + at + 20, 4));
		a.group = {bytes[at + 24], static_cast<std::uint32_t>(Get(bytes + at + 28, 4))};
	}
	if (!ValidCoopCampaignEconomy(value)) return false;
	output = value; return true;
}
inline bool EncodeCoopCampaignAimQuotes(const CoopCampaignAimQuotes& value, CoopCampaignAimQuotesBytes& output, std::size_t& size) noexcept
{
	if (!ValidCoopCampaignAimQuotes(value)) return false;
	using namespace CampaignEconomyWire;
	CoopCampaignAimQuotesBytes bytes{{'J','2','A','Q'}};
	Put(bytes.data() + 4, CurrentProtocolVersion, 2); bytes[6] = 1; bytes[7] = value.available ? 1 : 0;
	Put(bytes.data() + 8, value.sessionEpoch, 8); Put(bytes.data() + 16, value.revision, 8); Put(bytes.data() + 24, value.economyRevision, 8);
	Put(bytes.data() + 32, value.arrivalMinutes, 4); bytes[36] = value.landingX; bytes[37] = value.landingY; Put(bytes.data() + 38, value.quoteCount, 2);
	std::size_t at = CoopCampaignAimQuotesHeaderSize;
	for (std::size_t i = 0; i < value.quoteCount; ++i, at += CoopCampaignAimQuoteWireSize)
	{
		const auto& q = value.quotes[i];
		Put(bytes.data() + at, q.profile, 2); bytes[at + 2] = static_cast<std::uint8_t>(q.status);
		bytes[at + 3] = q.willingnessReason; bytes[at + 4] = q.gearAvailable ? 1 : 0;
		for (unsigned j = 0; j < 3; ++j) Put(bytes.data() + at + 8 + j * 4, q.salary[j], 4);
		Put(bytes.data() + at + 20, q.medicalDeposit, 4); Put(bytes.data() + at + 24, q.gearCost, 4);
		for (unsigned j = 0; j < 6; ++j) Put(bytes.data() + at + 28 + j * 4, q.total[j], 4);
	}
	output = bytes; size = at; return true;
}
inline bool DecodeCoopCampaignAimQuotes(const std::uint8_t* bytes, std::size_t size, CoopCampaignAimQuotes& output) noexcept
{
	using namespace CampaignEconomyWire;
	if (!bytes || size < CoopCampaignAimQuotesHeaderSize || size > MaximumCoopCampaignAimQuotesWireSize ||
		bytes[0] != 'J' || bytes[1] != '2' || bytes[2] != 'A' || bytes[3] != 'Q' ||
		Get(bytes + 4, 2) != CurrentProtocolVersion || bytes[6] != 1 || bytes[7] > 1 || !Zero(bytes + 40, 8)) return false;
	CoopCampaignAimQuotes value;
	value.sessionEpoch = Get(bytes + 8, 8); value.revision = Get(bytes + 16, 8); value.economyRevision = Get(bytes + 24, 8); value.available = bytes[7] != 0;
	value.arrivalMinutes = static_cast<std::uint32_t>(Get(bytes + 32, 4)); value.landingX = bytes[36]; value.landingY = bytes[37];
	value.quoteCount = static_cast<std::uint16_t>(Get(bytes + 38, 2));
	if (value.quoteCount > value.quotes.size() || size != CoopCampaignAimQuotesHeaderSize + value.quoteCount * CoopCampaignAimQuoteWireSize) return false;
	std::size_t at = CoopCampaignAimQuotesHeaderSize;
	for (std::size_t i = 0; i < value.quoteCount; ++i, at += CoopCampaignAimQuoteWireSize)
	{
		if (bytes[at + 4] > 1 || !Zero(bytes + at + 5, 3)) return false;
		auto& q = value.quotes[i];
		q.profile = static_cast<std::uint16_t>(Get(bytes + at, 2)); q.status = static_cast<CoopCampaignAimQuoteStatus>(bytes[at + 2]);
		q.willingnessReason = bytes[at + 3]; q.gearAvailable = bytes[at + 4] != 0;
		for (unsigned j = 0; j < 3; ++j) q.salary[j] = Signed32(bytes + at + 8 + j * 4);
		q.medicalDeposit = Signed32(bytes + at + 20); q.gearCost = Signed32(bytes + at + 24);
		for (unsigned j = 0; j < 6; ++j) q.total[j] = Signed32(bytes + at + 28 + j * 4);
	}
	if (!ValidCoopCampaignAimQuotes(value)) return false;
	output = value; return true;
}

class CoopCampaignEconomyLedger
{
public:
	bool beginSession(std::uint64_t epoch) noexcept { if (!epoch || value_.sessionEpoch) return false; value_ = {}; value_.sessionEpoch = epoch; return true; }
	void clear() noexcept { value_ = {}; }
	const CoopCampaignEconomy& value() const noexcept { return value_; }
	bool observe(CoopCampaignEconomy captured) noexcept
	{
		if (!value_.sessionEpoch) return false;
		captured.sessionEpoch = value_.sessionEpoch; captured.revision = value_.revision ? value_.revision : 1;
		if (!ValidCoopCampaignEconomy(captured)) return false;
		std::fill(captured.roster.begin() + captured.rosterCount, captured.roster.end(), CoopCampaignRosterActor{});
		if (value_.revision && !SameCoopCampaignEconomyContent(value_, captured))
		{ if (captured.revision == UINT64_MAX) return false; ++captured.revision; }
		value_ = captured; return true;
	}
private:
	CoopCampaignEconomy value_;
};
class CoopCampaignAimQuotesLedger
{
public:
	bool beginSession(std::uint64_t epoch) noexcept { if (!epoch || value_.sessionEpoch) return false; value_ = {}; value_.sessionEpoch = epoch; return true; }
	void clear() noexcept { value_ = {}; }
	const CoopCampaignAimQuotes& value() const noexcept { return value_; }
	bool observe(CoopCampaignAimQuotes captured) noexcept
	{
		if (!value_.sessionEpoch || (value_.revision && captured.economyRevision < value_.economyRevision)) return false;
		captured.sessionEpoch = value_.sessionEpoch; captured.revision = value_.revision ? value_.revision : 1;
		if (!ValidCoopCampaignAimQuotes(captured)) return false;
		std::fill(captured.quotes.begin() + captured.quoteCount, captured.quotes.end(), CoopCampaignAimQuote{});
		if (value_.revision && (captured.economyRevision != value_.economyRevision || !SameCoopCampaignAimQuotesContent(value_, captured)))
		{ if (captured.revision == UINT64_MAX) return false; ++captured.revision; }
		value_ = captured; return true;
	}
private:
	CoopCampaignAimQuotes value_;
};
}
#endif
