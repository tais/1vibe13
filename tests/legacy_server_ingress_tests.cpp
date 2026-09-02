#include <cstdint>
#include <cstdio>
#include <cstring>

#include "Multiplayer/LegacyServerIngress.h"

using namespace ja2::mp;

static int gFailures = 0;
#define CHECK(condition, message) do { \
	if (!(condition)) { \
		++gFailures; \
		std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); \
	} else { \
		std::printf("ok   %s\n", message); \
	} \
} while (0)

static bool ParseFailsWithoutMutation(const char* text, std::size_t bytes)
{
	std::uint16_t value = 0x4a32u;
	return !ParseLegacyTransferSetId(text, bytes, value) && value == 0x4a32u;
}

static LegacyExplosiveRecord ExplosiveRecord(
	std::uint8_t team, std::uint32_t worldIndex, std::size_t slot = 0)
{
	LegacyExplosiveRecord record;
	record.key = LegacyExplosiveKey{team, worldIndex};
	record.planterConnection = ConnectionId{100 + slot};
	record.planterSlot = slot;
	record.planterActor = static_cast<std::uint16_t>(120 + slot);
	record.grid = 1000 + worldIndex;
	record.level = static_cast<std::uint8_t>(worldIndex % 2);
	record.item = static_cast<std::uint16_t>(200 + worldIndex % 100);
	return record;
}

static void TestLegacyExplosiveLedger()
{
	LegacyExplosiveLedger ledger;
	CHECK(ledger.size() == 0 && ledger.countForSlot(0) == 0 &&
		ledger.countForSlot(LegacyArenaClientCapacity) == 0,
		"new explosive ledger and out-of-range slot counts are empty");

	const LegacyExplosiveRecord planted = ExplosiveRecord(7, 42, 1);
	CHECK(ledger.insert(planted) ==
		LegacyExplosiveInsertDisposition::Inserted,
		"valid planted explosive enters the bounded ledger");
	const LegacyExplosiveRecord* found = ledger.lookup(planted.key);
	CHECK(found && found->key == planted.key &&
		found->planterConnection == planted.planterConnection &&
		found->planterSlot == planted.planterSlot &&
		found->planterActor == planted.planterActor &&
		found->grid == planted.grid && found->level == planted.level &&
		found->item == planted.item,
		"lookup retains the complete planter and bomb metadata");
	CHECK(ledger.size() == 1 && ledger.countForSlot(1) == 1,
		"insert updates total and per-slot counts exactly once");

	LegacyExplosiveRecord conflicting = planted;
	conflicting.planterConnection = ConnectionId{999};
	conflicting.grid = 7777;
	CHECK(ledger.insert(conflicting) ==
		LegacyExplosiveInsertDisposition::Duplicate && ledger.size() == 1 &&
		ledger.lookup(planted.key)->planterConnection ==
			planted.planterConnection &&
		ledger.lookup(planted.key)->grid == planted.grid,
		"duplicate key preserves the original connection identity and metadata");

	const LegacyExplosiveRecord sameIndexOtherTeam =
		ExplosiveRecord(8, planted.key.creatorWorldIndex, 2);
	CHECK(ledger.insert(sameIndexOtherTeam) ==
		LegacyExplosiveInsertDisposition::Inserted && ledger.size() == 2 &&
		ledger.lookup(sameIndexOtherTeam.key) != nullptr,
		"creator world indices are namespaced by their origin wire team");

	LegacyExplosiveRecord invalid = ExplosiveRecord(7, 99);
	invalid.key.originTeam = 0;
	CHECK(ledger.insert(invalid) == LegacyExplosiveInsertDisposition::Invalid,
		"team zero cannot name a canonical planted explosive");
	invalid = ExplosiveRecord(10, 99);
	CHECK(ledger.insert(invalid) == LegacyExplosiveInsertDisposition::Invalid,
		"origin teams beyond the legacy arena range are rejected");
	invalid = ExplosiveRecord(7, 99);
	invalid.planterConnection = NoConnection;
	CHECK(ledger.insert(invalid) == LegacyExplosiveInsertDisposition::Invalid,
		"empty planter connection is rejected");
	invalid.planterConnection = AnyConnection;
	CHECK(ledger.insert(invalid) == LegacyExplosiveInsertDisposition::Invalid,
		"broadcast wildcard cannot own a planted explosive");
	invalid = ExplosiveRecord(7, 99);
	invalid.planterSlot = LegacyArenaClientCapacity;
	CHECK(ledger.insert(invalid) == LegacyExplosiveInsertDisposition::Invalid,
		"out-of-range planter slot is rejected");
	invalid = ExplosiveRecord(7, 99);
	invalid.planterActor = InvalidLegacyExplosiveActor;
	CHECK(ledger.insert(invalid) == LegacyExplosiveInsertDisposition::Invalid &&
		ledger.size() == 2,
		"invalid planter actor is rejected without mutating counts");
	CHECK(!ledger.lookup(LegacyExplosiveKey{0, 42}) &&
		!ledger.lookup(LegacyExplosiveKey{7, 999999}),
		"invalid and unknown explosive keys do not resolve");

	LegacyExplosiveRecord untouched = ExplosiveRecord(9, 900, 3);
	CHECK((!ledger.consume(LegacyExplosiveKey{7, 999999}, &untouched) &&
		untouched.key == LegacyExplosiveKey{9, 900}),
		"missing consume leaves its output record unchanged");
	LegacyExplosiveRecord consumed;
	CHECK(ledger.consume(planted.key, &consumed) &&
		consumed.key == planted.key &&
		consumed.planterConnection == planted.planterConnection &&
		consumed.planterSlot == planted.planterSlot &&
		consumed.planterActor == planted.planterActor &&
		consumed.grid == planted.grid && consumed.level == planted.level &&
		consumed.item == planted.item && !ledger.lookup(planted.key) &&
		ledger.size() == 1 && ledger.countForSlot(1) == 0,
		"consume returns metadata, retires one key, and updates its counts");
	CHECK(!ledger.consume(planted.key),
		"an already consumed explosive cannot be consumed twice");

	LegacyExplosiveLedger perSlotLedger;
	bool filledOneSlot = true;
	for (std::size_t index = 0;
		index < LegacyExplosiveLedgerPerSlotCapacity; ++index)
	{
		filledOneSlot = filledOneSlot &&
			perSlotLedger.insert(ExplosiveRecord(
				6, static_cast<std::uint32_t>(index), 0)) ==
				LegacyExplosiveInsertDisposition::Inserted;
	}
	const LegacyExplosiveRecord firstAtCapacity = ExplosiveRecord(6, 0, 0);
	CHECK(filledOneSlot &&
		perSlotLedger.size() == LegacyExplosiveLedgerPerSlotCapacity &&
		perSlotLedger.countForSlot(0) ==
			LegacyExplosiveLedgerPerSlotCapacity,
		"one planter slot can retain exactly its fixed 256-record quota");
	CHECK(perSlotLedger.insert(firstAtCapacity) ==
		LegacyExplosiveInsertDisposition::Duplicate,
		"duplicate remains distinguishable when its planter quota is full");
	CHECK(perSlotLedger.insert(ExplosiveRecord(
			6, static_cast<std::uint32_t>(
				LegacyExplosiveLedgerPerSlotCapacity), 0)) ==
		LegacyExplosiveInsertDisposition::Full,
		"a unique explosive beyond the planter quota is rejected as full");
	CHECK(perSlotLedger.consume(LegacyExplosiveKey{6, 7}) &&
		perSlotLedger.insert(ExplosiveRecord(
			6, static_cast<std::uint32_t>(
				LegacyExplosiveLedgerPerSlotCapacity), 0)) ==
			LegacyExplosiveInsertDisposition::Inserted &&
		perSlotLedger.size() == LegacyExplosiveLedgerPerSlotCapacity,
		"consume immediately reclaims one planter quota and storage entry");

	LegacyExplosiveLedger fullLedger;
	bool filledAllSlots = true;
	for (std::size_t slot = 0; slot < LegacyArenaClientCapacity; ++slot)
	{
		for (std::size_t index = 0;
			index < LegacyExplosiveLedgerPerSlotCapacity; ++index)
		{
			filledAllSlots = filledAllSlots &&
				fullLedger.insert(ExplosiveRecord(
					static_cast<std::uint8_t>(slot + 6),
					static_cast<std::uint32_t>(slot * 10000 + index),
					slot)) == LegacyExplosiveInsertDisposition::Inserted;
		}
	}
	bool everySlotAtQuota = true;
	for (std::size_t slot = 0; slot < LegacyArenaClientCapacity; ++slot)
		everySlotAtQuota = everySlotAtQuota &&
			fullLedger.countForSlot(slot) ==
				LegacyExplosiveLedgerPerSlotCapacity;
	CHECK(filledAllSlots && everySlotAtQuota &&
		fullLedger.size() == LegacyExplosiveLedgerCapacity,
		"all four quotas fill the fixed 1024-record ledger exactly");
	CHECK(fullLedger.insert(ExplosiveRecord(6, 50000, 0)) ==
		LegacyExplosiveInsertDisposition::Full,
		"a unique insertion into the full ledger fails closed");
	CHECK(fullLedger.insert(ExplosiveRecord(6, 0, 0)) ==
		LegacyExplosiveInsertDisposition::Duplicate,
		"duplicate remains distinguishable at total capacity");
	CHECK(fullLedger.consume(LegacyExplosiveKey{8, 20007}) &&
		fullLedger.insert(ExplosiveRecord(8, 50000, 2)) ==
			LegacyExplosiveInsertDisposition::Inserted &&
		fullLedger.size() == LegacyExplosiveLedgerCapacity &&
		fullLedger.countForSlot(2) ==
			LegacyExplosiveLedgerPerSlotCapacity,
		"consuming from a full ledger reclaims total and matching slot capacity");

	fullLedger.clear();
	bool allCountsCleared = fullLedger.size() == 0;
	for (std::size_t slot = 0; slot < LegacyArenaClientCapacity; ++slot)
		allCountsCleared = allCountsCleared &&
			fullLedger.countForSlot(slot) == 0;
	CHECK(allCountsCleared &&
		!fullLedger.lookup(LegacyExplosiveKey{6, 0}) &&
		fullLedger.insert(ExplosiveRecord(6, 0, 0)) ==
			LegacyExplosiveInsertDisposition::Inserted,
		"clear retires every record and restores all ledger capacity");
}

static void TestLegacySharedExplosiveClaims()
{
	LegacySharedExplosiveClaims claims;
	CHECK(claims.size() == 0 && claims.countForSlot(0) == 0 &&
		claims.countForSlot(LegacyArenaClientCapacity) == 0 &&
		!claims.contains(0),
		"new shared explosive claim set is empty");
	CHECK(claims.claim(0, 1) ==
			LegacySharedExplosiveClaimDisposition::Claimed &&
		claims.contains(0) && claims.size() == 1 &&
		claims.countForSlot(1) == 1,
		"map item zero can be claimed once in the shared namespace");
	CHECK(claims.claim(0, 2) ==
			LegacySharedExplosiveClaimDisposition::Duplicate &&
		claims.size() == 1 && claims.countForSlot(1) == 1 &&
		claims.countForSlot(2) == 0,
		"a second slot cannot replay or steal an existing shared claim");
	CHECK(claims.claim(LegacySharedExplosiveWorldIndexLimit, 0) ==
			LegacySharedExplosiveClaimDisposition::Invalid &&
		claims.claim(1, LegacyArenaClientCapacity) ==
			LegacySharedExplosiveClaimDisposition::Invalid &&
		!claims.contains(LegacySharedExplosiveWorldIndexLimit),
		"oversized indices and invalid claimant slots fail without mutation");

	LegacySharedExplosiveClaims perSlot;
	bool filledSlot = true;
	for (std::size_t index = 0;
		index < LegacySharedExplosiveClaimPerSlotCapacity; ++index)
	{
		filledSlot = filledSlot &&
			perSlot.claim(static_cast<std::uint32_t>(index), 0) ==
				LegacySharedExplosiveClaimDisposition::Claimed;
	}
	CHECK(filledSlot &&
		perSlot.size() == LegacySharedExplosiveClaimPerSlotCapacity &&
		perSlot.countForSlot(0) ==
			LegacySharedExplosiveClaimPerSlotCapacity,
		"one sender fills exactly its bounded shared-claim quota");
	CHECK(perSlot.claim(0, 0) ==
			LegacySharedExplosiveClaimDisposition::Duplicate &&
		perSlot.claim(
			static_cast<std::uint32_t>(
				LegacySharedExplosiveClaimPerSlotCapacity), 0) ==
			LegacySharedExplosiveClaimDisposition::Full,
		"duplicates remain distinct from a full per-slot claim quota");

	LegacySharedExplosiveClaims full;
	bool filledAll = true;
	for (std::size_t slot = 0; slot < LegacyArenaClientCapacity; ++slot)
	{
		for (std::size_t index = 0;
			index < LegacySharedExplosiveClaimPerSlotCapacity; ++index)
		{
			const std::uint32_t worldIndex = static_cast<std::uint32_t>(
				slot * LegacySharedExplosiveClaimPerSlotCapacity + index);
			filledAll = filledAll &&
				full.claim(worldIndex, slot) ==
					LegacySharedExplosiveClaimDisposition::Claimed;
		}
	}
	CHECK(filledAll && full.size() == LegacySharedExplosiveClaimCapacity,
		"all sender quotas fill the fixed shared-claim set exactly");
	CHECK(full.claim(0, 0) ==
			LegacySharedExplosiveClaimDisposition::Duplicate &&
		full.claim(
			static_cast<std::uint32_t>(LegacySharedExplosiveClaimCapacity), 0) ==
			LegacySharedExplosiveClaimDisposition::Full,
		"a full claim set remains replay-safe and rejects new claims");
	full.clear();
	bool countsCleared = full.size() == 0 && !full.contains(0);
	for (std::size_t slot = 0; slot < LegacyArenaClientCapacity; ++slot)
		countsCleared = countsCleared && full.countForSlot(slot) == 0;
	CHECK(countsCleared &&
		full.claim(17, 3) ==
			LegacySharedExplosiveClaimDisposition::Claimed,
		"session reset clears tombstones and restores all shared-claim capacity");
}

int main()
{
	TestLegacyExplosiveLedger();
	TestLegacySharedExplosiveClaims();

	LegacyAdmissionRegistry registry;
	CHECK(registry.admit(NoConnection).disposition ==
		LegacyAdmissionDisposition::InvalidSender,
		"empty transport cannot enter the player registry");
	CHECK(registry.admit(AnyConnection).disposition ==
		LegacyAdmissionDisposition::InvalidSender,
		"broadcast wildcard cannot enter the player registry");

	for (std::uint64_t id = 1; id <= LegacyArenaClientCapacity; ++id)
	{
		const LegacyAdmissionSelection result = registry.admit(ConnectionId{id});
		CHECK(result.disposition == LegacyAdmissionDisposition::Assign &&
			result.slot == id - 1,
			"first four distinct transports receive stable sequential slots");
	}

	const LegacyAdmissionSelection duplicate = registry.admit(ConnectionId{2});
	CHECK(duplicate.disposition ==
		LegacyAdmissionDisposition::AlreadyRegistered && duplicate.slot == 1,
		"duplicate settings request reuses its existing slot");
	for (std::uint64_t id = 1; id <= LegacyArenaClientCapacity; ++id)
	{
		CHECK(registry.connection(static_cast<std::size_t>(id - 1)) ==
			ConnectionId{id},
			"duplicate admission leaves every existing slot unchanged");
	}

	const LegacyAdmissionSelection full = registry.admit(ConnectionId{5});
	CHECK(full.disposition == LegacyAdmissionDisposition::Full &&
		full.slot == InvalidLegacyAdmissionSlot &&
		!registry.contains(ConnectionId{5}),
		"fifth transport is rejected without exposing or mutating an invalid slot");
	CHECK(!registry.remove(ConnectionId{99}),
		"unknown transport cannot remove a player slot");
	CHECK(registry.remove(ConnectionId{3}) && !registry.contains(ConnectionId{3}),
		"registered transport removal frees exactly its own slot");
	const LegacyAdmissionSelection replacement = registry.admit(ConnectionId{5});
	CHECK(replacement.disposition == LegacyAdmissionDisposition::Assign &&
		replacement.slot == 2,
		"replacement transport receives the first genuinely free slot");
	registry.clear();
	for (std::size_t slot = 0; slot < LegacyArenaClientCapacity; ++slot)
		CHECK(!registry.connection(slot), "registry clear retires every slot");
	CHECK(!registry.connection(LegacyArenaClientCapacity),
		"out-of-range registry lookup returns the empty sentinel");

	LegacyAdmissionRegistry reservedHostRegistry;
	const LegacyAdmissionSelection remoteFirst =
		reservedHostRegistry.admitFrom(ConnectionId{11}, 1);
	const LegacyAdmissionSelection hostLater =
		reservedHostRegistry.admitAt(ConnectionId{12}, 0);
	CHECK(remoteFirst.disposition == LegacyAdmissionDisposition::Assign &&
		remoteFirst.slot == 1 &&
		hostLater.disposition == LegacyAdmissionDisposition::Assign &&
		hostLater.slot == 0,
		"reserved host slot survives a remote-first admission race");
	CHECK(reservedHostRegistry.admitAt(ConnectionId{13}, 0).disposition ==
		LegacyAdmissionDisposition::Full &&
		reservedHostRegistry.admitFrom(ConnectionId{11}, 1).disposition ==
		LegacyAdmissionDisposition::AlreadyRegistered,
		"reserved-slot assignment rejects replacement and keeps duplicates stable");
	CHECK(reservedHostRegistry.admitFrom(ConnectionId{14},
		LegacyArenaClientCapacity).disposition ==
		LegacyAdmissionDisposition::InvalidSender,
		"admission rejects an empty eligible-slot range");

	const unsigned char payload[4] = {1, 2, 3, 4};
	CHECK(LegacyMessageHasExactPayload(payload, 4, 4),
		"fixed packet accepts its exact payload size");
	CHECK(!LegacyMessageHasExactPayload(payload, 3, 4) &&
		!LegacyMessageHasExactPayload(payload, 5, 4),
		"fixed packet rejects truncation and trailing bytes");
	CHECK(!LegacyMessageHasExactPayload(nullptr, 4, 4),
		"nonempty fixed packet rejects null storage");
	CHECK(LegacyMessageHasExactPayload(nullptr, 0, 0),
		"zero-byte request accepts an empty payload");

	struct ValidSetId { const char* text; std::uint16_t value; };
	const ValidSetId valid[] = {
		{"0", 0}, {"1", 1}, {"42", 42}, {"65534", 65534},
	};
	for (const ValidSetId& sample : valid)
	{
		std::uint16_t parsed = 0xffffu;
		CHECK(ParseLegacyTransferSetId(
			sample.text, std::strlen(sample.text) + 1, parsed) &&
			parsed == sample.value,
			"canonical transfer-set identifier parses exactly");
	}

	const char missingTerminator[] = {'4', '2'};
	const char embeddedTerminator[] = {'4', '\0', '2', '\0'};
	CHECK(ParseFailsWithoutMutation(nullptr, 0),
		"null transfer-set identifier is rejected transactionally");
	CHECK(ParseFailsWithoutMutation("", 1),
		"empty transfer-set identifier is rejected");
	CHECK(ParseFailsWithoutMutation(missingTerminator, sizeof(missingTerminator)),
		"unterminated transfer-set identifier is rejected");
	CHECK(ParseFailsWithoutMutation(embeddedTerminator, sizeof(embeddedTerminator)),
		"embedded terminator and trailing text are rejected");
	CHECK(ParseFailsWithoutMutation("00", 3) &&
		ParseFailsWithoutMutation("01", 3),
		"noncanonical leading-zero identifiers are rejected");
	CHECK(ParseFailsWithoutMutation("-1", 3) &&
		ParseFailsWithoutMutation("+1", 3) &&
		ParseFailsWithoutMutation("1x", 3),
		"signed and nondecimal identifiers are rejected");
	CHECK(ParseFailsWithoutMutation("65535", 6) &&
		ParseFailsWithoutMutation("65536", 6),
		"reserved and overflowing transfer-set identifiers are rejected");

	CHECK(!LegacySignedIndexInRange(-1, 10) &&
		LegacySignedIndexInRange(0, 10) &&
		LegacySignedIndexInRange(9, 10) &&
		!LegacySignedIndexInRange(10, 10),
		"signed wire indices are bounded before array access");

	CHECK(LegacyAdmissionSlotOwnsActorTeam(0, true, 0) &&
		LegacyAdmissionSlotOwnsActorTeam(0, true, 5) &&
		!LegacyAdmissionSlotOwnsActorTeam(0, true, 6) &&
		!LegacyAdmissionSlotOwnsActorTeam(0, false, 0) &&
		LegacyAdmissionSlotOwnsActorTeam(0, false, 6),
		"only the authenticated embedded slot owns host and engine AI teams");
	CHECK(LegacyAdmissionSlotOwnsActorTeam(1, false, 7) &&
		LegacyAdmissionSlotOwnsActorTeam(2, false, 8) &&
		LegacyAdmissionSlotOwnsActorTeam(3, false, 9) &&
		!LegacyAdmissionSlotOwnsActorTeam(1, false, 8) &&
		!LegacyAdmissionSlotOwnsActorTeam(4, false, 10) &&
		!LegacyAdmissionSlotOwnsActorTeam(1, false, -1),
		"remote slots own only their exact mapped tactical actor team");

	if (gFailures)
		std::printf("%d failure(s)\n", gFailures);
	return gFailures ? 1 : 0;
}
