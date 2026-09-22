#include "CoopCampaignHireAuthority.h"
#include "CoopCampaignActionAuthority.h"
#include <cstdio>
#include <stdexcept>

using namespace CoopSession;
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
PeerIdentity Identity(unsigned n) { PeerIdentity value{}; value[0] = static_cast<std::uint8_t>(n); return value; }
CoopCampaignEconomy Economy()
{
	CoopCampaignEconomy value;
	value.sessionEpoch = 1; value.revision = 2; value.available = true; value.balance = 10000;
	value.mercenaryLimit = 32; value.mercenaryCount = value.rosterCount = 1;
	value.roster[0].actor = {0, 1}; value.roster[0].profile = 21; value.roster[0].x = 9; value.roster[0].y = 1;
	value.roster[0].group = {1, 1}; value.roster[0].contractEndMinutes = 3000;
	return value;
}
CoopCampaignAimQuotes Quotes()
{
	CoopCampaignAimQuotes value;
	value.sessionEpoch = 1; value.revision = 3; value.economyRevision = 2; value.available = true;
	value.arrivalMinutes = 1800; value.landingX = 9; value.landingY = 1; value.quoteCount = 2;
	auto& q = value.quotes[0]; q.profile = 0; q.status = CoopCampaignAimQuoteStatus::Available;
	q.gearAvailable = true; q.salary = {100, 600, 1100}; q.medicalDeposit = 300; q.gearCost = 50;
	q.total = {400, 450, 900, 950, 1400, 1450};
	value.quotes[1].profile = 254; value.quotes[1].status = CoopCampaignAimQuoteStatus::Unsupported;
	return value;
}
CoopCampaignHireRequest Request()
{
	CoopCampaignHireRequest value;
	value.sessionEpoch = 1; value.controlRevision = 1; value.economyRevision = 2; value.quoteRevision = 3;
	value.requestId = 4; value.profile = 0; value.days = 7; value.buyGear = true;
	return value;
}
void EconomyCodec()
{
	auto source = Economy(); source.balance = INT32_MIN;
	CoopCampaignEconomyBytes bytes; std::size_t size = 0; CoopCampaignEconomy decoded;
	CHECK(EncodeCoopCampaignEconomy(source, bytes, size) && size == 72 && bytes[24] == 0 && bytes[27] == 128 &&
		bytes[40] == 0 && bytes[42] == 21 && bytes[44] == 1 && bytes[48] == 128 && bytes[64] == 1 &&
		DecodeCoopCampaignEconomy(bytes.data(), size, decoded) && SameCoopCampaignEconomy(source, decoded),
		"signed balance and actor slot zero have explicit portable representation");
	for (std::size_t length = 0; length <= size + 1; ++length)
		if (length != size) CHECK(!DecodeCoopCampaignEconomy(bytes.data(), length, decoded) && SameCoopCampaignEconomy(source, decoded),
			"truncated/extended economy snapshot is transactional");
	for (unsigned at : {0u,1u,2u,3u,4u,5u,6u,7u,34u,35u,36u,37u,38u,39u,53u,54u,55u,65u,66u,67u})
	{
		auto bad = bytes; bad[at] ^= 0xff;
		CHECK(!DecodeCoopCampaignEconomy(bad.data(), size, decoded) && SameCoopCampaignEconomy(source, decoded), "economy header/reserved corruption cannot replace roster");
	}
	for (unsigned fault = 0; fault < 11; ++fault)
	{
		auto bad = source;
		if (fault == 0) bad.rosterCount = 261;
		if (fault == 1) bad.roster[0].actor.incarnation = 0;
		if (fault == 2) bad.roster[0].profile = 256;
		if (fault == 3) bad.roster[0].x = 0;
		if (fault == 4) bad.roster[0].group.incarnation = 0;
		if (fault == 5) bad.roster[0].pendingHire = true;
		if (fault == 6) { bad.roster[0].betweenSectors = true; bad.roster[0].inSector = true; }
		if (fault == 7) bad.mercenaryCount = 2;
		if (fault == 8) { bad.available = false; bad.rosterCount = 0; }
		if (fault == 9) bad.mercenaryCount = 0;
		if (fault == 10) bad.roster[0].contractEndMinutes = UINT32_MAX;
		const auto saved = bytes; auto savedSize = size;
		CHECK(!EncodeCoopCampaignEconomy(bad, bytes, size) && bytes == saved && size == savedSize, "invalid native roster cannot replace encoded state");
	}
	source = Economy(); source.rosterCount = 260; source.mercenaryCount = source.mercenaryLimit = 254;
	for (std::size_t i = 0; i < source.rosterCount; ++i)
	{
		auto& actor = source.roster[i]; actor = source.roster[0]; actor.actor = {static_cast<std::uint16_t>(i), 2};
		actor.profile = i < 254 ? static_cast<std::uint16_t>(i) : 255; actor.vehicle = i >= 254;
	}
	source.roster[0].pendingHire = true; source.roster[0].arrivalMinutes = 2000; source.roster[0].group = {};
	CHECK(EncodeCoopCampaignEconomy(source, bytes, size) && size == MaximumCoopCampaignEconomyWireSize &&
		DecodeCoopCampaignEconomy(bytes.data(), size, decoded) && SameCoopCampaignEconomy(source, decoded),
		"maximum roster includes groupless IN_TRANSIT hires and six vehicles with reserved-byte profiles");
	CHECK(size < 65536, "complete economy remains below the authenticated transport limit");
	for (unsigned count : {261u,65535u})
	{
		auto bad = bytes; CampaignEconomyWire::Put(bad.data() + 28, count, 2);
		CHECK(!DecodeCoopCampaignEconomy(bad.data(), size, decoded) && SameCoopCampaignEconomy(source, decoded),
			"oversized wire roster count rejects before reading actors and preserves prior snapshot");
	}
	auto invalidCapacity = source; invalidCapacity.roster[253].vehicle = true; --invalidCapacity.mercenaryCount;
	CHECK(!ValidCoopCampaignEconomy(invalidCapacity), "a seventh vehicle cannot fit by reducing the mercenary count");
	invalidCapacity = source; invalidCapacity.roster[254].vehicle = false; ++invalidCapacity.mercenaryCount;
	CHECK(!ValidCoopCampaignEconomy(invalidCapacity), "a 255th mercenary cannot fit by reducing the vehicle count");
	source.roster[1].actor.slot = 0;
	CHECK(!ValidCoopCampaignEconomy(source), "duplicate actor slots are rejected even with differing incarnations");
	CHECK(!DecodeCoopCampaignEconomy(nullptr, size, decoded), "null economy input");
}
void QuotesCodec()
{
	auto source = Quotes(); CoopCampaignAimQuotesBytes bytes; CoopCampaignAimQuotes decoded; std::size_t size = 0;
	CHECK(EncodeCoopCampaignAimQuotes(source, bytes, size) && size == 152 && bytes[32] == 8 && bytes[33] == 7 &&
		bytes[56] == 100 && bytes[72] == 50 && bytes[76] == 144 && bytes[77] == 1 &&
		DecodeCoopCampaignAimQuotes(bytes.data(), size, decoded) && SameCoopCampaignAimQuotes(source, decoded), "quote components and totals use fixed little-endian signed words");
	for (std::size_t length = 0; length <= size + 1; ++length)
		if (length != size) CHECK(!DecodeCoopCampaignAimQuotes(bytes.data(), length, decoded) && SameCoopCampaignAimQuotes(source, decoded), "quote lengths are exact and transactional");
	for (unsigned at : {0u,1u,2u,3u,4u,5u,6u,7u,40u,41u,42u,43u,44u,45u,46u,47u,53u,54u,55u,105u,106u,107u})
	{
		auto bad = bytes; bad[at] ^= 0xff;
		CHECK(!DecodeCoopCampaignAimQuotes(bad.data(), size, decoded) && SameCoopCampaignAimQuotes(source, decoded), "malformed quote headers and padding cannot replace offers");
	}
	for (unsigned fault = 0; fault < 12; ++fault)
	{
		auto bad = source; auto& q = bad.quotes[0];
		if (fault == 0) q.total[5]++;
		if (fault == 1) q.salary[0] = -1;
		if (fault == 2) q.salary[2] = INT32_MAX;
		if (fault == 3) q.medicalDeposit = -1;
		if (fault == 4) q.gearCost = INT32_MAX;
		if (fault == 5) q.gearAvailable = false;
		if (fault == 6) q.willingnessReason = 4;
		if (fault == 7) q.profile = 255;
		if (fault == 8) bad.quotes[1].profile = q.profile;
		if (fault == 9) bad.quoteCount = 256;
		if (fault == 10) bad.economyRevision = 0;
		if (fault == 11) bad.available = false;
		auto saved = bytes; auto savedSize = size;
		CHECK(!EncodeCoopCampaignAimQuotes(bad, bytes, size) && bytes == saved && size == savedSize, "overflow, noncanonical or incoherent quote must not publish");
	}
	auto q = source.quotes[0]; q.gearCost = 0; q.total = {400,400,900,900,1400,1400};
	CHECK(ValidCoopCampaignAimQuote(q), "explicitly available free gear is a valid distinct choice");
	q.gearAvailable = false; q.total = {400,0,900,0,1400,0};
	CHECK(ValidCoopCampaignAimQuote(q), "unavailable gear has canonical zero unused totals");
	for (unsigned reason = 0; reason <= 7; ++reason)
	{
		q = {}; q.profile = 42; q.status = CoopCampaignAimQuoteStatus::Unwilling; q.willingnessReason = reason;
		CHECK(ValidCoopCampaignAimQuote(q) == (reason == 1 || reason >= 4), "unwilling row must carry actual refusal reason and no money payload");
	}
	q.gearAvailable = true; CHECK(!ValidCoopCampaignAimQuote(q), "unavailable row cannot retain a gear choice");
	source = Quotes(); source.quoteCount = 255;
	for (std::size_t i = 0; i < source.quoteCount; ++i) { source.quotes[i] = {}; source.quotes[i].profile = static_cast<std::uint16_t>(i); }
	CHECK(EncodeCoopCampaignAimQuotes(source, bytes, size) && size == MaximumCoopCampaignAimQuotesWireSize &&
		DecodeCoopCampaignAimQuotes(bytes.data(), size, decoded) && decoded.quoteCount == 255, "all native AIM profile IDs fit bounded deterministic rows");
	CHECK(size < 65536, "complete AIM quote set remains below the authenticated transport limit");
	for (unsigned count : {256u,65535u})
	{
		auto bad = bytes; CampaignEconomyWire::Put(bad.data() + 38, count, 2);
		CHECK(!DecodeCoopCampaignAimQuotes(bad.data(), size, decoded) && SameCoopCampaignAimQuotes(source, decoded),
			"oversized wire quote count rejects before reading offers and preserves prior snapshot");
	}
	CHECK(!DecodeCoopCampaignAimQuotes(nullptr, size, decoded), "null quote input");
}
void SnapshotRevisions()
{
	CoopCampaignEconomyLedger economy; CoopCampaignAimQuotesLedger quotes;
	CHECK(!economy.observe(Economy()) && !quotes.observe(Quotes()) && !economy.beginSession(0), "unstarted snapshot ledgers reject observations");
	CHECK(economy.beginSession(5) && quotes.beginSession(5) && !economy.beginSession(6), "ledger session is explicit and cannot be overwritten");
	CHECK(economy.observe(Economy()) && economy.value().revision == 1, "first economy observed at revision one");
	auto e = economy.value(); e.sessionEpoch = 999; e.revision = 999;
	CHECK(economy.observe(e) && economy.value().revision == 1 && economy.value().sessionEpoch == 5, "copied stamps cannot change session/content revision");
	auto q = Quotes(); q.economyRevision = 1;
	CHECK(quotes.observe(q) && quotes.value().revision == 1, "quotes bind to completed economy observation");
	e.roster[0].contractEndMinutes++;
	CHECK(economy.observe(e) && economy.value().revision == 2 && economy.value().balance == 10000, "roster-only mutation advances economic barrier without spending");
	q.economyRevision = 2;
	CHECK(quotes.observe(q) && quotes.value().revision == 2, "identical offers rebind and advance after roster mutation");
	q.quotes[0].willingnessReason = 2;
	CHECK(quotes.observe(q) && quotes.value().revision == 3, "buddy willingness change invalidates previous quote even when totals unchanged");
	q.quotes[0].gearCost = 0; q.quotes[0].total = {400,400,900,900,1400,1400};
	CHECK(quotes.observe(q) && quotes.value().revision == 4, "native gear quote update advances quote barrier");
	q.economyRevision = 1; const auto before = quotes.value();
	CHECK(!quotes.observe(q) && SameCoopCampaignAimQuotes(before, quotes.value()), "economic binding cannot regress");
	q = quotes.value(); q.available = false; q.quoteCount = 0; q.arrivalMinutes = 0; q.landingX = q.landingY = 0;
	CHECK(quotes.observe(q) && !quotes.value().available && quotes.value().quoteCount == 0 && quotes.value().revision == 5,
		"unavailable full replacement clears prior offers");
	CHECK(quotes.value().quotes[0].salary[0] == 0 && quotes.value().quotes[0].status == CoopCampaignAimQuoteStatus::Unavailable,
		"unused old quote array contents are cleared on unavailable replacement");
	e = economy.value(); e.available = false; e.balance = 0; e.mercenaryCount = e.mercenaryLimit = e.rosterCount = 0;
	CHECK(economy.observe(e) && !economy.value().available && economy.value().rosterCount == 0 && economy.value().balance == 0,
		"unavailable full replacement clears balance and roster");
	CHECK(!economy.value().roster[0].actor.valid() && !economy.value().roster[0].profile,
		"unused old roster array contents are cleared on unavailable replacement");
	economy.clear(); quotes.clear(); CHECK(economy.beginSession(6) && quotes.beginSession(6), "explicit new session resets ledgers");
}
void SnapshotRevisionExhaustion()
{
	CoopCampaignEconomyLedger economy; CoopCampaignAimQuotesLedger quotes;
	CHECK(economy.beginSession(1) && economy.observe(Economy()) && quotes.beginSession(1) && quotes.observe(Quotes()),
		"revision exhaustion fixture starts with valid observations");
	// Reach the otherwise impractical last revision without adding a shipping
	// revision-injection API. These objects are non-const test fixtures.
	const_cast<CoopCampaignEconomy&>(economy.value()).revision = UINT64_MAX;
	const_cast<CoopCampaignAimQuotes&>(quotes.value()).revision = UINT64_MAX;
	const auto oldEconomy = economy.value(); const auto oldQuotes = quotes.value();
	CHECK(economy.observe(oldEconomy) && quotes.observe(oldQuotes), "unchanged observations remain valid at the final revision");
	auto changedEconomy = oldEconomy; --changedEconomy.balance;
	CHECK(!economy.observe(changedEconomy) && SameCoopCampaignEconomy(oldEconomy,economy.value()),
		"economic revision cannot wrap or replace the last published balance");
	auto changedQuotes = oldQuotes; ++changedQuotes.arrivalMinutes;
	CHECK(!quotes.observe(changedQuotes) && SameCoopCampaignAimQuotes(oldQuotes,quotes.value()),
		"quote content cannot wrap or replace the last published offers");
	changedQuotes = oldQuotes; ++changedQuotes.economyRevision;
	CHECK(!quotes.observe(changedQuotes) && SameCoopCampaignAimQuotes(oldQuotes,quotes.value()),
		"quote economy rebinding cannot bypass exhausted quote revision");
}
void HireCodec()
{
	auto source = Request(); source.sessionEpoch = UINT64_C(0x0102030405060708);
	CoopCampaignHireRequestBytes bytes{}, expected{{'J','2','H','Q', static_cast<std::uint8_t>(CurrentProtocolVersion),
		static_cast<std::uint8_t>(CurrentProtocolVersion >> 8),7,1,8,7,6,5,4,3,2,1,
		1,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
	CHECK(EncodeCoopCampaignHireRequest(source, bytes) && bytes == expected, "exact hire request contains no client price, destination or claimed identity");
	CoopCampaignHireRequest decoded = source;
	for (std::size_t length = 0; length <= bytes.size() + 1; ++length)
		if (length != bytes.size()) CHECK(!DecodeCoopCampaignHireRequest(bytes.data(), length, decoded) && SameCoopCampaignHireRequest(source, decoded), "nonexact hire request length is transactional");
	for (unsigned at : {0u,1u,2u,3u,4u,5u,6u,7u,49u,50u,51u,52u,53u,54u,55u})
	{
		auto bad = bytes; bad[at] ^= 0xff;
		CHECK(!DecodeCoopCampaignHireRequest(bad.data(), bad.size(), decoded) && SameCoopCampaignHireRequest(source, decoded), "malformed contract and padding cannot replace request");
	}
	for (unsigned at : {8u,16u,24u,32u,40u})
	{
		auto bad = bytes; std::fill(bad.begin() + at, bad.begin() + at + 8, 0);
		CHECK(!DecodeCoopCampaignHireRequest(bad.data(), bad.size(), decoded), "zero binding and replay ID rejected");
	}
	for (unsigned days : {1u,7u,14u}) for (bool gear : {false,true})
	{
		source.days = days; source.buyGear = gear;
		CHECK(EncodeCoopCampaignHireRequest(source, bytes) && DecodeCoopCampaignHireRequest(bytes.data(), bytes.size(), decoded) &&
			SameCoopCampaignHireRequest(source, decoded) && CoopCampaignHireChoiceIndex(days, gear) < 6, "all six native contract choices roundtrip");
	}
	CHECK(CoopCampaignHireChoiceIndex(2, false) == 6 && !DecodeCoopCampaignHireRequest(nullptr, 56, decoded), "invalid contract never silently selects a price");
	for (unsigned outcome = 1; outcome <= 11; ++outcome)
	{
		CoopCampaignHireResult result; result.request = source; result.controlRevision = 5; result.economyRevision = 4; result.quoteRevision = 7;
		result.outcome = static_cast<CoopCampaignHireOutcome>(outcome); result.nativeAttempted = outcome == 1 || outcome == 9;
		if (outcome == 1) { result.actor = {0, 0x04030201}; result.chargedTotal = 1400; result.nativeDetail = 0x0102; }
		CoopCampaignHireResultBytes encoded; CoopCampaignHireResult out;
		CHECK(EncodeCoopCampaignHireResult(result, encoded) && encoded[82] == outcome && DecodeCoopCampaignHireResult(encoded.data(), encoded.size(), out) &&
			SameCoopCampaignHireRequest(source, out.request) && out.outcome == result.outcome && out.nativeAttempted == result.nativeAttempted &&
			out.controlRevision == result.controlRevision && out.economyRevision == result.economyRevision && out.quoteRevision == result.quoteRevision &&
			out.actor == result.actor && out.chargedTotal == result.chargedTotal && out.nativeDetail == result.nativeDetail,
			"every terminal receipt preserves its eligibility stamps and explicit native attempt boundary");
		CHECK(encoded[56] == 5 && encoded[64] == 4 && encoded[72] == 7, "receipt revisions have independent fixed wire positions");
		if (outcome == 1) CHECK(encoded[84] == 1 && encoded[87] == 4 && encoded[88] == 120 && encoded[89] == 5 && encoded[92] == 2 && encoded[93] == 1, "native identity, debit and detail have explicit widths");
		for (std::size_t length = 0; length <= encoded.size() + 1; ++length)
			if (length != encoded.size()) CHECK(!DecodeCoopCampaignHireResult(encoded.data(), length, out) && out.outcome == result.outcome, "result size rejection preserves prior receipt");
		for (unsigned at : {0u,1u,2u,3u,4u,5u,6u,7u,50u,51u,52u,53u,54u,55u,82u,83u,94u,95u})
		{
			auto bad = encoded; bad[at] ^= 0xff;
			CHECK(!DecodeCoopCampaignHireResult(bad.data(), bad.size(), out) && out.outcome == result.outcome, "invalid result header and flags cannot replace receipt");
		}
		if (outcome != 1) for (unsigned at : {80u,81u,84u,85u,86u,87u,88u,89u,90u,91u})
		{
			auto bad = encoded; bad[at] = 1; CHECK(!DecodeCoopCampaignHireResult(bad.data(), bad.size(), out), "unused actor/debit fields must be zero in rejected receipts");
		}
		if (result.nativeAttempted)
		{
			result.economyRevision = source.economyRevision - 1; CHECK(!ValidCoopCampaignHireResult(result), "attempted receipt cannot regress its eligibility economy stamp");
			result.economyRevision = source.economyRevision; result.quoteRevision = source.quoteRevision - 1;
			CHECK(!ValidCoopCampaignHireResult(result), "attempted receipt cannot regress its eligibility quote stamp");
			result.quoteRevision = source.quoteRevision; result.controlRevision = source.controlRevision;
			CHECK(!ValidCoopCampaignHireResult(result), "native attempt must consume shared barrier");
		}
		else { result.nativeDetail = 1; CHECK(!ValidCoopCampaignHireResult(result), "preflight receipt cannot claim native detail"); }
	}
}
struct Fixture
{
	PeerIdentity identities[2]{Identity(1),Identity(2)};
	CoopCampaignHireAuthority::Peer peers[2]{{identities[0],TransportPeer{1}},{identities[1],TransportPeer{2}}};
	CoopCampaignStatusLedger status;
	CoopCampaignEconomy economy = Economy(); CoopCampaignAimQuotes quotes = Quotes(); CoopCampaignHireAuthority authority;
	Fixture()
	{
		CoopCampaignStatus clock; clock.phase = CoopCampaignPhase::Strategic;
		CHECK(status.beginSession(1) && status.observe(clock, identities, 2) && authority.reconcile(peers, 2), "two admitted campaign peers");
	}
	CoopCampaignHireRequest request(std::uint64_t id = 1) const
	{
		auto r = Request(); r.controlRevision = status.value().timeControlRevision; r.economyRevision = economy.revision;
		r.quoteRevision = quotes.revision; r.requestId = id; return r;
	}
	CoopCampaignHireOutcome validate(CoopCampaignHireRequest r) const
	{ return ValidateCoopCampaignHireRequest(r, status.value(), economy, quotes, true, true, true); }
};
void Eligibility()
{
	using O = CoopCampaignHireOutcome; Fixture f; auto r = f.request();
	CHECK(f.validate(r) == O::Applied && f.status.value().timeLeader != f.peers[1].identity, "either ready player can hire independently of time leadership");
	CHECK(ValidateCoopCampaignHireRequest(r,f.status.value(),f.economy,f.quotes,false,true,true) == O::NotReady &&
		ValidateCoopCampaignHireRequest(r,f.status.value(),f.economy,f.quotes,true,false,true) == O::Unauthorized &&
		ValidateCoopCampaignHireRequest(r,f.status.value(),f.economy,f.quotes,true,true,false) == O::Unavailable, "readiness, authorization and native context are independent gates");
	for (unsigned fault = 0; fault < 4; ++fault)
	{
		auto bad = r;
		if (fault == 0) ++bad.sessionEpoch; if (fault == 1) ++bad.controlRevision;
		if (fault == 2) ++bad.economyRevision; if (fault == 3) ++bad.quoteRevision;
		CHECK(f.validate(bad) == (fault == 3 ? O::StaleQuote : O::Stale), "exact epoch, shared control, economic and offer revision barriers");
	}
	f.quotes.economyRevision++; CHECK(f.validate(r) == O::StaleQuote, "mixed native capture generations cannot authorize hiring"); f.quotes = Quotes();
	f.economy.balance = 949; CHECK(f.validate(r) == O::InsufficientFunds, "salary, deposit and selected gear are included in affordability");
	f.economy.balance = 950; CHECK(f.validate(r) == O::Applied, "exact funds are sufficient");
	f.economy.roster[0].profile = r.profile; f.economy.roster[0].pendingHire = true; f.economy.roster[0].arrivalMinutes = 1800; f.economy.roster[0].group = {};
	CHECK(f.validate(r) == O::AlreadyHired, "pending native hire prevents duplicate profile before it joins a strategic group"); f.economy = Economy();
	f.economy.mercenaryLimit = 1; CHECK(f.validate(r) == O::Unavailable, "native team capacity gates eligibility"); f.economy = Economy();
	auto& q = f.quotes.quotes[0]; q.gearAvailable = false; q.gearCost = 0; q.total = {400,0,900,0,1400,0};
	CHECK(f.validate(r) == O::Unsupported, "no gear choice cannot be inferred from a zero quote");
	r.buyGear = false; CHECK(f.validate(r) == O::Applied, "salary-only contract remains available");
	q = {}; q.status = CoopCampaignAimQuoteStatus::Unwilling; q.willingnessReason = 4;
	CHECK(f.validate(r) == O::Unavailable, "native willingness refusal cannot be overridden by shared policy");
	q.status = CoopCampaignAimQuoteStatus::AlreadyHired; q.willingnessReason = 0; CHECK(f.validate(r) == O::AlreadyHired, "server profile status can reject duplicate outside copied friendly roster");
	q.status = CoopCampaignAimQuoteStatus::Unsupported; CHECK(f.validate(r) == O::Unsupported, "unrepresentable native price is explicit unsupported");
}
void ReceiptsAndBarriers()
{
	using O = CoopCampaignHireOutcome; Fixture f; unsigned hires = 0, debits = 0, events = 0;
	auto apply = [&](const CoopCampaignHireRequest& r) {
		CHECK(f.status.value().timeControlRevision > r.controlRevision && f.authority.deliveries()[1].pending,
			"receipt and shared barrier reserved before constructing actor, debiting and scheduling");
		++hires; ++debits; ++events; return CoopCampaignHireNativeResult{O::Applied,{5,9},950,17};
	};
	const auto first = f.request();
	CHECK(f.authority.submit(first,f.peers[1],true,true,true,f.status,f.economy,f.quotes,apply) && hires == 1, "nonleader executes shared native hire");
	const auto original = f.authority.deliveries()[1].result;
	CHECK(original.nativeAttempted && original.controlRevision == first.controlRevision + 1 && original.actor == TacticalEntityId({5,9}) && original.chargedTotal == 950,
		"receipt captures exact native identity and server debit at consumed revision");
	CHECK(f.authority.submit(first,f.peers[0],true,true,true,f.status,f.economy,f.quotes,apply) && hires == 1 && f.authority.deliveries()[0].result.outcome == O::Stale,
		"competing peer cannot spend against consumed shared barrier");
	CHECK(!f.authority.submit(f.request(2),f.peers[1],true,true,true,f.status,f.economy,f.quotes,apply), "undelivered receipt applies bounded backpressure");
	f.authority.delivered(1); f.economy.roster[0].profile = first.profile; ++f.economy.revision;
	f.quotes = {}; f.quotes.sessionEpoch = 1; f.quotes.revision = 8; f.quotes.economyRevision = f.economy.revision;
	CHECK(f.authority.submit(first,f.peers[1],false,false,false,f.status,f.economy,f.quotes,apply) && hires == 1 && debits == 1 && events == 1 &&
		f.authority.deliveries()[1].result.outcome == O::Applied && f.authority.deliveries()[1].result.quoteRevision == original.quoteRevision,
		"lost historical receipt replays through changed roster and unavailable quotes without a second debit/event");
	CoopCampaignHireResultBytes encoded; CHECK(EncodeCoopCampaignHireResult(f.authority.deliveries()[1].result,encoded), "historical receipt is valid without current old quote");
	f.authority.delivered(1); auto conflict = first; conflict.days = 14;
	CHECK(!f.authority.submit(conflict,f.peers[1],true,true,true,f.status,f.economy,f.quotes,apply), "same replay ID with different terms never executes");
	conflict = f.request(); CHECK(!f.authority.submit(conflict,f.peers[1],true,true,true,f.status,f.economy,f.quotes,apply), "old ID cannot be revived with refreshed stamps");
	f.economy = Economy(); f.quotes = Quotes(); f.authority.delivered(0);
	CoopCampaignActionAuthority actions;
	CoopCampaignActionAuthority::Peer actionPeers[2]{{f.identities[0],TransportPeer{1}},{f.identities[1],TransportPeer{2}}};
	CHECK(actions.reconcile(actionPeers,2), "separate travel receipt ledger shares native control state");
	CoopCampaignGroups groups; groups.sessionEpoch = groups.revision = 1; groups.available = true; groups.groupCount = groups.memberCount = 1;
	groups.groups[0].id = {1,1}; groups.groups[0].x = 9; groups.groups[0].y = 1; groups.groups[0].memberCount = 1; groups.members[0].actor = {0,1};
	CoopCampaignActionRequest travel; travel.sessionEpoch = 1; travel.controlRevision = first.controlRevision; travel.groupsRevision = travel.requestId = 1;
	travel.group = {1,1}; travel.destinationX = 10; travel.destinationY = 1; unsigned moves = 0;
	CHECK(actions.submit(travel,actionPeers[0],true,true,true,f.status,groups,[&](const CoopCampaignActionRequest&) { ++moves; return CoopCampaignActionNativeResult{CoopCampaignActionOutcome::Applied,0}; }) &&
		actions.deliveries()[0].result.outcome == CoopCampaignActionOutcome::Stale && !moves, "hire invalidates a concurrent travel against the same shared barrier");
	auto next = f.request(2); CHECK(f.status.consumeTimeControlRevision(next.controlRevision), "separate time control consumes same revision");
	CHECK(f.authority.submit(next,f.peers[1],true,true,true,f.status,f.economy,f.quotes,apply) && f.authority.deliveries()[1].result.outcome == O::Stale && hires == 1,
		"time mutation invalidates previously prepared hire");
}
void TransportAndFailure()
{
	using O = CoopCampaignHireOutcome; Fixture f; unsigned calls = 0;
	auto nativeReject = [&](const CoopCampaignHireRequest&) { ++calls; return CoopCampaignHireNativeResult{O::NativeRejected,{},0,73}; };
	auto r = f.request(UINT64_MAX);
	CHECK(f.authority.submit(r,f.peers[0],true,true,true,f.status,f.economy,f.quotes,nativeReject) && calls == 1 &&
		f.authority.deliveries()[0].result.nativeAttempted && f.authority.deliveries()[0].result.nativeDetail == 73 && !f.authority.failed(), "native rejection consumes barrier but is terminal and retryable with new request");
	f.authority.delivered(0);
	CHECK(!f.authority.submit(f.request(1),f.peers[0],true,true,true,f.status,f.economy,f.quotes,nativeReject) && calls == 1, "request ID exhaustion never wraps within a transport");
	CHECK(f.authority.reconcile(f.peers,2) && f.authority.submit(r,f.peers[0],false,false,false,f.status,f.economy,f.quotes,nativeReject) && calls == 1,
		"same-transport resync retains terminal high-water receipt");
	const auto old = f.peers[0]; f.peers[0].transport = TransportPeer{3};
	CHECK(f.authority.reconcile(f.peers,2) && !f.authority.submit(f.request(),old,true,true,true,f.status,f.economy,f.quotes,nativeReject) &&
		f.authority.submit(f.request(),f.peers[0],true,true,true,f.status,f.economy,f.quotes,nativeReject) && calls == 2, "replacement transport starts own sequence and rejects old attributed traffic");
	CoopCampaignHireAuthority::Peer duplicates[2]{f.peers[0],f.peers[1]}; duplicates[1].transport = duplicates[0].transport;
	CHECK(!f.authority.reconcile(duplicates,2) && !f.authority.reconcile(nullptr,1) && !f.authority.reconcile(f.peers,5), "invalid reconciliation cannot replace live receipt table");
	f.authority.delivered(0);
	auto fail = [&](const CoopCampaignHireRequest&) { ++calls; return CoopCampaignHireNativeResult{O::Failed,{},0,91}; };
	CHECK(f.authority.submit(f.request(2),f.peers[0],true,true,true,f.status,f.economy,f.quotes,fail) && calls == 3 && f.authority.failed(), "partial native failure latches fail-stop");
	f.authority.delivered(0); const auto before = f.status.value().timeControlRevision;
	CHECK(f.authority.submit(f.request(3),f.peers[0],true,true,true,f.status,f.economy,f.quotes,nativeReject) && calls == 3 &&
		!f.authority.deliveries()[0].result.nativeAttempted && f.status.value().timeControlRevision == before, "failed host cannot attempt later hiring or consume another barrier");
	CHECK(f.authority.reconcile(nullptr,0) && f.authority.reconcile(f.peers,2) && f.authority.failed(), "connection churn cannot clear partial native failure");
	for (unsigned fault = 0; fault < 5; ++fault)
	{
		Fixture broken;
		auto bad = [&](const CoopCampaignHireRequest&) -> CoopCampaignHireNativeResult {
			if (fault == 0) throw std::runtime_error("native failure");
			if (fault == 1) return {O::Applied,{2,3},949,0};
			if (fault == 2) return {O::Applied,{},950,0};
			if (fault == 3) return {O::Stale,{},0,0};
			return {O::NativeRejected,{2,3},0,0};
		};
		CHECK(broken.authority.submit(broken.request(),broken.peers[0],true,true,true,broken.status,broken.economy,broken.quotes,bad) && broken.authority.failed() &&
			broken.authority.deliveries()[0].result.outcome == O::Failed && broken.authority.deliveries()[0].result.nativeAttempted &&
			broken.authority.deliveries()[0].result.actor == TacticalEntityId{} && !broken.authority.deliveries()[0].result.chargedTotal,
			"exception, wrong price, missing identity or contradictory native result fail closed after consumed barrier");
	}
	Fixture exhausted;
	const_cast<CoopCampaignStatus&>(exhausted.status.value()).timeControlRevision = UINT64_MAX;
	CHECK(exhausted.authority.submit(exhausted.request(),exhausted.peers[0],true,true,true,exhausted.status,exhausted.economy,exhausted.quotes,nativeReject) &&
		exhausted.authority.failed() && !exhausted.authority.deliveries()[0].result.nativeAttempted && calls == 3 &&
		ValidCoopCampaignHireResult(exhausted.authority.deliveries()[0].result), "barrier exhaustion fails before native call with valid explicit unattempted receipt");
}
}
int main()
{
	EconomyCodec(); QuotesCodec(); SnapshotRevisions(); SnapshotRevisionExhaustion(); HireCodec(); Eligibility(); ReceiptsAndBarriers(); TransportAndFailure();
	return failures ? 1 : 0;
}
