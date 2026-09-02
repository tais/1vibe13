#include "CoopTacticalProtocol.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>
#include <vector>

using namespace CoopSession;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { if (!(condition)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); } } while (0)

static_assert(CoopTacticalCommonHeaderWireSize == 48);
static_assert(CoopTacticalIntentReceiptWireSize == 96);
static_assert(CoopTacticalBaselineHeaderWireSize == 76);
static_assert(CoopTacticalBaselineAckWireSize == 88);
static_assert(CoopTacticalDeltaHeaderWireSize == 72);
static_assert(CoopTacticalDeltaAckWireSize == 80);
static_assert(CoopTacticalResyncRequestWireSize == 88);
static_assert(MaximumCoopTacticalBaselinePayloadWireSize == 30773);
static_assert(MaximumCoopTacticalBaselineWireSize == 32385);
static_assert(MaximumCoopTacticalDeltaEvents == 3074);
static_assert(MaximumCoopTacticalDeltaPayloadWireSize == 62034);
static_assert(MaximumCoopTacticalDeltaWireSize == 62106);

PeerIdentity Identity(std::uint8_t seed)
{
	PeerIdentity identity{};
	for (std::size_t index = 0; index < identity.size(); ++index)
		identity[index] = static_cast<std::uint8_t>(seed + index);
	return identity;
}

CoopTacticalStateIdentity State(
	std::uint64_t revision = UINT64_C(0x2827262524232221),
	std::uint64_t turn = UINT64_C(0x3837363534333231))
{
	return CoopTacticalStateIdentity{
		CoopTacticalWireVersion, CurrentProtocolVersion,
		UINT64_C(0x0807060504030201),
		UINT64_C(0x1817161514131211), revision, turn};
}

TacticalActorSnapshot Actor()
{
	TacticalActorSnapshot actor;
	actor.id = TacticalEntityId{5, 9};
	actor.team = 1;
	actor.profile = 17;
	actor.grid = 1234;
	actor.level = 0;
	actor.direction = 3;
	actor.animation = 44;
	actor.stance = TacticalStance::Crouched;
	actor.actionPoints = 19;
	actor.life = 77;
	actor.maximumLife = 88;
	actor.breath = 66;
	actor.maximumBreath = 99;
	actor.active = true;
	actor.inSector = true;
	actor.hostileToPlayerTeam = true;
	actor.loadout.helmet = TacticalHandItemSnapshot{
		40, 1, 92, 0, 0, 0, false, false};
	actor.loadout.vest = TacticalHandItemSnapshot{
		41, 1, 81, 0, 0, 0, false, false};
	actor.loadout.legs = TacticalHandItemSnapshot{
		42, 1, 70, 0, 0, 0, false, false};
	actor.loadout.primaryHand = TacticalHandItemSnapshot{
		10, 1, 84, 20, 7, -12, true, true};
	actor.loadout.secondaryHand = TacticalHandItemSnapshot{
		30, 1, 73, 0, 0, 0, false, false};
	return actor;
}

TacticalWorldSnapshot Snapshot(const CoopTacticalStateIdentity& state)
{
	TacticalWorldSnapshot snapshot;
	std::vector<TacticalActorSnapshot> actors{Actor()};
	std::vector<TacticalDoorSnapshot> doors{
		TacticalDoorSnapshot{1235, 71, false}};
	CHECK(TacticalWorldSnapshot::create(
		state.worldGeneration, TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 2, 0, true},
		TacticalTurnSnapshot{true, true, 0, state.turnSerial, true},
		std::move(actors), std::move(doors), snapshot) ==
			TacticalSnapshotCreateError::None,
		"snapshot fixture is valid");
	return snapshot;
}

TacticalWorldDelta Delta(std::uint64_t generation)
{
	TacticalWorldDelta delta;
	delta.previousEpoch = generation;
	delta.currentEpoch = generation;
	delta.events.push_back(TacticalActorMovedEvent{
		TacticalEntityId{5, 9}, 1234, 1240, 0, 0, 3, 4});
	return delta;
}

bool SameState(const CoopTacticalStateIdentity& left,
	const CoopTacticalStateIdentity& right)
{
	return left.protocolVersion == right.protocolVersion &&
		left.wireVersion == right.wireVersion &&
		left.sessionEpoch == right.sessionEpoch &&
		left.worldGeneration == right.worldGeneration &&
		left.revision == right.revision &&
		left.turnSerial == right.turnSerial;
}

void WriteU32(std::vector<std::uint8_t>& bytes,
	std::size_t offset, std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes,
	std::size_t offset)
{
	std::uint32_t value = 0;
	for (unsigned shift = 0; shift < 32; shift += 8)
		value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
	return value;
}

void TestChecksum()
{
	const std::array<std::uint8_t, 3> abc{{'a', 'b', 'c'}};
	CHECK(CoopTacticalPayloadChecksum(abc.data(), abc.size()) ==
		UINT32_C(0x1a47e90b), "payload checksum pins FNV-1a bytes");
	CHECK(CoopTacticalPayloadChecksum(nullptr, 0) == UINT32_C(2166136261),
		"empty checksum is canonical FNV offset basis");
	CHECK(CoopTacticalPayloadChecksum(nullptr, 1) == 0,
		"nonempty null checksum input is rejected");
}

void TestReceiptCodec()
{
	CoopTacticalIntentReceipt receipt;
	receipt.state = State();
	receipt.peerIdentity = Identity(0x40);
	receipt.commandId = UINT64_C(0x5857565554535250);
	receipt.nextExpectedCommandId = UINT64_C(0x5857565554535251);
	receipt.authoritativeSequence = UINT64_C(0x6867666564636261);
	receipt.simulationTick = UINT64_C(0x7877767574737271);
	receipt.status = CoopTacticalIntentReceiptStatus::Applied;
	receipt.reason = CoopTacticalIntentReceiptReason::None;

	CoopTacticalIntentReceiptBytes bytes{};
	CHECK(EncodeCoopTacticalIntentReceipt(receipt, bytes) ==
		CoopTacticalCodecResult::Success, "receipt encodes");
	CHECK(bytes[0] == 'J' && bytes[1] == '2' && bytes[2] == 'C' &&
		bytes[3] == 'R', "receipt magic is exact");
	CHECK(bytes[4] == 3 && bytes[5] == 0 && bytes[6] == 7 &&
		bytes[7] == 0 && bytes[8] == 3 && bytes[9] == 0,
		"receipt wire/protocol versions and terminal fields are exact");
	for (std::size_t index = 0; index < 8; ++index)
	{
		CHECK(bytes[16 + index] == index + 1,
			"receipt session epoch is little endian");
		CHECK(bytes[56 + index] == index + 0x11,
			"receipt world generation is little endian");
		CHECK(bytes[64 + index] == index + 0x21,
			"receipt revision is little endian");
		CHECK(bytes[72 + index] == index + 0x31,
			"receipt turn is little endian");
		CHECK(bytes[40 + index] == static_cast<std::uint8_t>(
			(receipt.commandId >> (index * 8U)) & UINT64_C(0xff)),
			"receipt command is little endian");
		CHECK(bytes[48 + index] == index + 0x51,
			"receipt next expected command is little endian");
		CHECK(bytes[80 + index] == index + 0x61,
			"receipt authority sequence is little endian");
		CHECK(bytes[88 + index] == index + 0x71,
			"receipt simulation tick is little endian");
	}
	for (std::size_t index = 0; index < 16; ++index)
		CHECK(bytes[24 + index] == index + 0x40,
			"receipt peer identity occupies exact range");
	for (std::size_t index = 10; index < 16; ++index)
		CHECK(bytes[index] == 0, "receipt reserve is zero");

	CoopTacticalIntentReceipt decoded;
	CHECK(DecodeCoopTacticalIntentReceipt(bytes.data(), bytes.size(), decoded) ==
		CoopTacticalCodecResult::Success, "golden receipt decodes");
	CHECK(SameState(decoded.state, receipt.state) &&
		decoded.peerIdentity == receipt.peerIdentity &&
		decoded.commandId == receipt.commandId &&
		decoded.nextExpectedCommandId == receipt.nextExpectedCommandId &&
		decoded.authoritativeSequence == receipt.authoritativeSequence &&
		decoded.simulationTick == receipt.simulationTick &&
		decoded.status == receipt.status && decoded.reason == receipt.reason,
		"receipt round trip preserves every field");

	CoopTacticalIntentReceipt sentinel = receipt;
	sentinel.commandId = 7;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopTacticalIntentReceipt output = sentinel;
		CHECK(DecodeCoopTacticalIntentReceipt(bytes.data(), size, output) !=
			CoopTacticalCodecResult::Success,
			"every truncated receipt is rejected");
		CHECK(output.commandId == sentinel.commandId,
			"failed receipt decode is transactional");
	}
	CHECK(DecodeCoopTacticalIntentReceipt(nullptr, bytes.size(), decoded) ==
		CoopTacticalCodecResult::Invalid, "null receipt is rejected");
	auto malformed = bytes;
	malformed[4] = 4;
	CHECK(DecodeCoopTacticalIntentReceipt(
		malformed.data(), malformed.size(), decoded) ==
		CoopTacticalCodecResult::UnsupportedVersion,
		"unsupported receipt version is explicit");
	malformed = bytes;
	malformed[8] = 0xff;
	CHECK(DecodeCoopTacticalIntentReceipt(
		malformed.data(), malformed.size(), decoded) ==
		CoopTacticalCodecResult::Invalid,
		"unknown receipt status is rejected");
	malformed = bytes;
	malformed[10] = 1;
	CHECK(DecodeCoopTacticalIntentReceipt(
		malformed.data(), malformed.size(), decoded) ==
		CoopTacticalCodecResult::Invalid,
		"nonzero receipt reserve is rejected");

	CoopTacticalIntentReceipt invalid = receipt;
	invalid.status = CoopTacticalIntentReceiptStatus::Rejected;
	CHECK(EncodeCoopTacticalIntentReceipt(invalid, bytes) ==
		CoopTacticalCodecResult::Invalid,
		"rejected receipt requires reason and zero authority sequence");
	invalid.authoritativeSequence = 0;
	invalid.reason = CoopTacticalIntentReceiptReason::GameplayRejected;
	CHECK(EncodeCoopTacticalIntentReceipt(invalid, bytes) ==
		CoopTacticalCodecResult::Success,
		"canonical pre-execution rejection encodes");
	invalid.nextExpectedCommandId++;
	CHECK(EncodeCoopTacticalIntentReceipt(invalid, bytes) ==
		CoopTacticalCodecResult::Invalid,
		"receipt rejects a cursor other than the exact post-command value");
	invalid = receipt;
	invalid.commandId = std::numeric_limits<std::uint64_t>::max();
	invalid.nextExpectedCommandId = 0;
	CHECK(EncodeCoopTacticalIntentReceipt(invalid, bytes) ==
		CoopTacticalCodecResult::Success,
		"maximum command encodes only with the exhausted cursor sentinel");
	invalid = receipt;
	invalid.status = CoopTacticalIntentReceiptStatus::Rejected;
	invalid.reason =
		CoopTacticalIntentReceiptReason::InvalidCommandSequence;
	invalid.authoritativeSequence = 0;
	invalid.nextExpectedCommandId = 7;
	CHECK(EncodeCoopTacticalIntentReceipt(invalid, bytes) ==
		CoopTacticalCodecResult::Success,
		"non-consuming sequence rejection reports the authoritative cursor");
	invalid.reason =
		CoopTacticalIntentReceiptReason::InboxSequenceExhausted;
	invalid.nextExpectedCommandId = 0;
	CHECK(EncodeCoopTacticalIntentReceipt(invalid, bytes) ==
		CoopTacticalCodecResult::Success,
		"exhausted sequence rejection reports the zero cursor sentinel");
	invalid = receipt;
	invalid.status = CoopTacticalIntentReceiptStatus::Rejected;
	invalid.reason =
		CoopTacticalIntentReceiptReason::AuthoritySequenceExhausted;
	invalid.authoritativeSequence = 0;
	CHECK(EncodeCoopTacticalIntentReceipt(invalid, bytes) ==
		CoopTacticalCodecResult::Success,
		"authority exhaustion is a normal consuming terminal rejection");
	invalid.nextExpectedCommandId = 0;
	CHECK(EncodeCoopTacticalIntentReceipt(invalid, bytes) ==
		CoopTacticalCodecResult::Invalid,
		"authority exhaustion cannot report a non-consuming zero cursor");
	invalid.commandId = (std::numeric_limits<std::uint64_t>::max)();
	CHECK(EncodeCoopTacticalIntentReceipt(invalid, bytes) ==
		CoopTacticalCodecResult::Success,
		"a consumed maximum command may pair authority exhaustion with zero");
}

void TestBaselineCodec()
{
	CoopTacticalBaseline baseline;
	baseline.state = State();
	baseline.baselineId = UINT64_C(0x4847464544434241);
	baseline.nextExpectedCommandId = UINT64_C(0x605f5e5d5c5b5a59);
	baseline.snapshot = Snapshot(baseline.state);
	baseline.assignedActors.push_back(Actor().id);

	std::vector<std::uint8_t> bytes;
	CHECK(EncodeCoopTacticalBaseline(baseline, bytes) ==
		CoopTacticalCodecResult::Success, "baseline encodes");
	CHECK(bytes.size() == CoopTacticalBaselineHeaderWireSize +
		6 +
		EncodedTacticalWorldSnapshotHeaderBytes +
		EncodedTacticalActorSnapshotBytes +
		EncodedTacticalDoorSnapshotBytes,
		"baseline has exact outer plus inner bytes");
	CHECK(bytes[8] == 2 && bytes[9] == 0,
		"baseline kind and reserve are exact");
	for (std::size_t index = 0; index < 8; ++index)
		CHECK(bytes[48 + index] == index + 0x41,
			"baseline ID is little endian");
	CHECK(bytes[56] == 1 && bytes[57] == 0 && bytes[58] == 0 &&
		bytes[59] == 0, "baseline assignment count and reserve are exact");
	CHECK(ReadU32(bytes, 60) == bytes.size() -
		CoopTacticalBaselineHeaderWireSize - 6,
		"baseline payload length is exact");
	CHECK(ReadU32(bytes, 64) == CoopTacticalPayloadChecksum(
		bytes.data() + CoopTacticalBaselineHeaderWireSize,
		bytes.size() - CoopTacticalBaselineHeaderWireSize),
		"baseline checksum covers assignments and exact nested bytes");
	for (std::size_t index = 0; index < 8; ++index)
		CHECK(bytes[68 + index] == index + 0x59,
			"baseline command cursor is little endian");
	CHECK(bytes[76] == 5 && bytes[77] == 0 && bytes[78] == 9 &&
		bytes[79] == 0 && bytes[80] == 0 && bytes[81] == 0,
		"baseline assigned actor is fixed-width little endian");

	CoopTacticalBaseline decoded;
	CHECK(DecodeCoopTacticalBaseline(bytes, decoded) ==
		CoopTacticalCodecResult::Success, "golden baseline decodes");
	CHECK(SameState(decoded.state, baseline.state) &&
		decoded.baselineId == baseline.baselineId &&
		decoded.payloadChecksum == ReadU32(bytes, 64) &&
		decoded.nextExpectedCommandId == baseline.nextExpectedCommandId &&
		decoded.assignedActors == baseline.assignedActors &&
		decoded.snapshot.epoch() == baseline.snapshot.epoch() &&
		decoded.snapshot.dimensions().columns == 160 &&
		decoded.snapshot.dimensions().rows == 160 &&
		decoded.snapshot.turn().serial == baseline.snapshot.turn().serial &&
		decoded.snapshot.turn().commandsBlocked &&
		decoded.snapshot.actors().size() == 1 &&
		decoded.snapshot.actors()[0].grid == 1234 &&
		decoded.snapshot.actors()[0].hostileToPlayerTeam &&
		decoded.snapshot.actors()[0].loadout.helmet.item == 40 &&
		decoded.snapshot.actors()[0].loadout.vest.item == 41 &&
		decoded.snapshot.actors()[0].loadout.legs.item == 42 &&
		decoded.snapshot.actors()[0].loadout == Actor().loadout &&
		decoded.snapshot.doors().size() == 1 &&
		decoded.snapshot.doors()[0].baseGrid == 1235 &&
		decoded.snapshot.doors()[0].structureId == 71 &&
		!decoded.snapshot.doors()[0].open,
		"baseline round trip preserves identity and snapshot");

	CoopTacticalBaseline sentinel = decoded;
	sentinel.baselineId = 99;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopTacticalBaseline output = sentinel;
		CHECK(DecodeCoopTacticalBaseline(bytes.data(), size, output) !=
			CoopTacticalCodecResult::Success,
			"every truncated baseline is rejected");
		CHECK(output.baselineId == sentinel.baselineId,
			"failed baseline decode is transactional");
	}
	std::vector<std::uint8_t> malformed = bytes;
	malformed.push_back(0);
	CHECK(DecodeCoopTacticalBaseline(malformed, decoded) ==
		CoopTacticalCodecResult::Invalid,
		"extended baseline is rejected");
	malformed = bytes;
	malformed.back() ^= 1;
	CHECK(DecodeCoopTacticalBaseline(malformed, decoded) ==
		CoopTacticalCodecResult::ChecksumMismatch,
		"baseline payload corruption fails integrity");
	malformed = bytes;
	const std::size_t nestedSnapshot =
		CoopTacticalBaselineHeaderWireSize + baseline.assignedActors.size() * 6;
	malformed[nestedSnapshot + 4] = 1;
	malformed[nestedSnapshot + 5] = 0;
	WriteU32(malformed, 64, CoopTacticalPayloadChecksum(
		malformed.data() + CoopTacticalBaselineHeaderWireSize,
		malformed.size() - CoopTacticalBaselineHeaderWireSize));
	CHECK(DecodeCoopTacticalBaseline(malformed, decoded) ==
		CoopTacticalCodecResult::InvalidPayload,
		"a checksummed unsupported inner snapshot fails closed");
	malformed = bytes;
	// Inner snapshot epoch starts at payload offset 6. Change it while repairing
	// the checksum so the outer/inner generation equality is independently tested.
	malformed[CoopTacticalBaselineHeaderWireSize + 6 + 6] ^= 1;
	WriteU32(malformed, 64, CoopTacticalPayloadChecksum(
		malformed.data() + CoopTacticalBaselineHeaderWireSize,
		malformed.size() - CoopTacticalBaselineHeaderWireSize));
	CHECK(DecodeCoopTacticalBaseline(malformed, decoded) ==
		CoopTacticalCodecResult::InvalidPayload,
		"baseline rejects a checksummed foreign world generation");
	malformed = bytes;
	for (std::size_t offset = 78; offset < 82; ++offset)
		malformed[offset] = 0;
	WriteU32(malformed, 64, CoopTacticalPayloadChecksum(
		malformed.data() + CoopTacticalBaselineHeaderWireSize,
		malformed.size() - CoopTacticalBaselineHeaderWireSize));
	CHECK(DecodeCoopTacticalBaseline(malformed, decoded) ==
		CoopTacticalCodecResult::Invalid,
		"baseline rejects an invalid assigned entity before payload decode");
	malformed.assign(CoopTacticalBaselineHeaderWireSize, 0);
	std::copy(bytes.begin(), bytes.begin() +
		CoopTacticalBaselineHeaderWireSize, malformed.begin());
	WriteU32(malformed, 60, static_cast<std::uint32_t>(
		MaximumCoopTacticalBaselinePayloadWireSize + 1));
	CHECK(DecodeCoopTacticalBaseline(malformed, decoded) ==
		CoopTacticalCodecResult::PayloadTooLarge,
		"declared over-limit baseline is rejected before allocation");

	baseline.payloadChecksum = ReadU32(bytes, 64) ^ 1u;
	std::vector<std::uint8_t> retained{1, 2, 3};
	CHECK(EncodeCoopTacticalBaseline(baseline, retained) ==
		CoopTacticalCodecResult::ChecksumMismatch,
		"encoder rejects a conflicting supplied baseline checksum");
	CHECK(retained == std::vector<std::uint8_t>({1, 2, 3}),
		"failed baseline encode is transactional");
	baseline.payloadChecksum = 0;
	baseline.assignedActors.push_back(baseline.assignedActors.front());
	CHECK(EncodeCoopTacticalBaseline(baseline, retained) ==
		CoopTacticalCodecResult::Invalid,
		"baseline assigned entities must be strictly ordered and unique");
}

void TestBaselineAckCodec()
{
	CoopTacticalBaselineAck acknowledgement;
	acknowledgement.state = State();
	acknowledgement.peerIdentity = Identity(0x20);
	acknowledgement.baselineId = UINT64_C(0x4847464544434241);
	acknowledgement.payloadChecksum = UINT32_C(0x52515049);
	acknowledgement.nextExpectedCommandId =
		UINT64_C(0x605f5e5d5c5b5a59);
	CoopTacticalBaselineAckBytes bytes{};
	CHECK(EncodeCoopTacticalBaselineAck(acknowledgement, bytes) ==
		CoopTacticalCodecResult::Success, "baseline ACK encodes");
	CHECK(bytes[8] == 3 && bytes[72] == 0x49 && bytes[73] == 0x50 &&
		bytes[74] == 0x51 && bytes[75] == 0x52 && bytes[76] == 0 &&
		bytes[79] == 0 && bytes[80] == 0x59 && bytes[87] == 0x60,
		"baseline ACK checksum, reserve and command cursor are exact");
	CoopTacticalBaselineAck decoded;
	CHECK(DecodeCoopTacticalBaselineAck(bytes.data(), bytes.size(), decoded) ==
		CoopTacticalCodecResult::Success &&
		decoded.peerIdentity == acknowledgement.peerIdentity &&
		decoded.nextExpectedCommandId ==
			acknowledgement.nextExpectedCommandId,
		"baseline ACK round trip succeeds");
	for (std::size_t size = 0; size < bytes.size(); ++size)
		CHECK(DecodeCoopTacticalBaselineAck(bytes.data(), size, decoded) !=
			CoopTacticalCodecResult::Success,
			"every truncated baseline ACK is rejected");
	auto malformed = bytes;
	malformed[79] = 1;
	CHECK(DecodeCoopTacticalBaselineAck(
		malformed.data(), malformed.size(), decoded) ==
		CoopTacticalCodecResult::Invalid,
		"nonzero baseline ACK reserve is rejected");
	acknowledgement.nextExpectedCommandId = 0;
	CHECK(EncodeCoopTacticalBaselineAck(acknowledgement, bytes) ==
		CoopTacticalCodecResult::Success &&
		DecodeCoopTacticalBaselineAck(bytes.data(), bytes.size(), decoded) ==
			CoopTacticalCodecResult::Success &&
		decoded.nextExpectedCommandId == 0,
		"zero baseline ACK cursor is the canonical exhausted sentinel");
}

void TestDeltaCodec()
{
	CoopTacticalDelta delta;
	delta.state = State(41, 12);
	delta.deltaId = UINT64_C(0x4847464544434241);
	delta.baseRevision = 40;
	delta.delta = Delta(delta.state.worldGeneration);
	std::vector<std::uint8_t> bytes;
	CHECK(EncodeCoopTacticalDelta(delta, bytes) ==
		CoopTacticalCodecResult::Success, "delta encodes");
	CHECK(bytes[8] == 4 && bytes[9] == 0,
		"delta kind and reserve are exact");
	for (std::size_t index = 0; index < 8; ++index)
		CHECK(bytes[48 + index] == index + 0x41,
			"delta ID is little endian");
	CHECK(ReadU32(bytes, 64) == bytes.size() -
		CoopTacticalDeltaHeaderWireSize,
		"delta payload length is exact");
	CHECK(ReadU32(bytes, 68) == CoopTacticalPayloadChecksum(
		bytes.data() + CoopTacticalDeltaHeaderWireSize,
		bytes.size() - CoopTacticalDeltaHeaderWireSize),
		"delta checksum covers exact nested bytes");

	CoopTacticalDelta decoded;
	CHECK(DecodeCoopTacticalDelta(bytes, decoded) ==
		CoopTacticalCodecResult::Success, "golden delta decodes");
	CHECK(SameState(decoded.state, delta.state) &&
		decoded.deltaId == delta.deltaId &&
		decoded.baseRevision == delta.baseRevision &&
		decoded.delta.events.size() == 1 &&
		std::holds_alternative<TacticalActorMovedEvent>(
			decoded.delta.events[0]) &&
		std::get<TacticalActorMovedEvent>(decoded.delta.events[0]).currentGrid == 1240,
		"delta round trip preserves identity and event");

	CoopTacticalDelta commandBlocked;
	commandBlocked.state = State(42, 12);
	commandBlocked.deltaId = 2;
	commandBlocked.baseRevision = 41;
	commandBlocked.delta.previousEpoch =
		commandBlocked.state.worldGeneration;
	commandBlocked.delta.currentEpoch =
		commandBlocked.state.worldGeneration;
	commandBlocked.delta.events.push_back(TacticalTurnChangedEvent{
		TacticalTurnSnapshot{true, true, 0, 12, false},
		TacticalTurnSnapshot{true, true, 0, 12, true}});
	auto& interruptChange = std::get<TacticalTurnChangedEvent>(
		commandBlocked.delta.events.back());
	interruptChange.previous.interruptPhase = TacticalInterruptPhase::Resolving;
	interruptChange.current.interruptPhase = TacticalInterruptPhase::Active;
	interruptChange.current.interruptSerial = 44;
	std::vector<std::uint8_t> commandBlockedBytes;
	CoopTacticalDelta decodedCommandBlocked;
	CHECK(EncodeCoopTacticalDelta(commandBlocked, commandBlockedBytes) ==
			CoopTacticalCodecResult::Success &&
		DecodeCoopTacticalDelta(
			commandBlockedBytes, decodedCommandBlocked) ==
			CoopTacticalCodecResult::Success &&
		decodedCommandBlocked.state.turnSerial == 12 &&
		decodedCommandBlocked.delta.events.size() == 1 &&
		std::holds_alternative<TacticalTurnChangedEvent>(
			decodedCommandBlocked.delta.events[0]) &&
		std::get<TacticalTurnChangedEvent>(
			decodedCommandBlocked.delta.events[0]).current.commandsBlocked &&
		std::get<TacticalTurnChangedEvent>(
			decodedCommandBlocked.delta.events[0]).current.interruptPhase ==
			TacticalInterruptPhase::Active &&
		std::get<TacticalTurnChangedEvent>(
			decodedCommandBlocked.delta.events[0]).current.interruptSerial == 44,
		"co-op envelope round-trips busy and interrupt turn metadata");

	CoopTacticalDelta sentinel = decoded;
	sentinel.deltaId = 99;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopTacticalDelta output = sentinel;
		CHECK(DecodeCoopTacticalDelta(bytes.data(), size, output) !=
			CoopTacticalCodecResult::Success,
			"every truncated delta is rejected");
		CHECK(output.deltaId == sentinel.deltaId,
			"failed delta decode is transactional");
	}
	std::vector<std::uint8_t> malformed = bytes;
	malformed.back() ^= 1;
	CHECK(DecodeCoopTacticalDelta(malformed, decoded) ==
		CoopTacticalCodecResult::ChecksumMismatch,
		"delta payload corruption fails integrity");
	malformed = bytes;
	malformed[CoopTacticalDeltaHeaderWireSize + 6] ^= 1;
	WriteU32(malformed, 68, CoopTacticalPayloadChecksum(
		malformed.data() + CoopTacticalDeltaHeaderWireSize,
		malformed.size() - CoopTacticalDeltaHeaderWireSize));
	CHECK(DecodeCoopTacticalDelta(malformed, decoded) ==
		CoopTacticalCodecResult::InvalidPayload,
		"delta rejects a checksummed foreign world generation");
	malformed.assign(CoopTacticalDeltaHeaderWireSize, 0);
	std::copy(bytes.begin(), bytes.begin() +
		CoopTacticalDeltaHeaderWireSize, malformed.begin());
	WriteU32(malformed, 64, static_cast<std::uint32_t>(
		MaximumCoopTacticalDeltaPayloadWireSize + 1));
	CHECK(DecodeCoopTacticalDelta(malformed, decoded) ==
		CoopTacticalCodecResult::PayloadTooLarge,
		"declared over-limit delta is rejected before allocation");

	delta.baseRevision = delta.state.revision;
	std::vector<std::uint8_t> retained{9};
	CHECK(EncodeCoopTacticalDelta(delta, retained) ==
		CoopTacticalCodecResult::Invalid,
		"delta requires a strictly earlier base revision");
	CHECK(retained == std::vector<std::uint8_t>({9}),
		"failed delta encode is transactional");
}

void TestDeltaAckCodec()
{
	CoopTacticalDeltaAck acknowledgement;
	acknowledgement.state = State(41, 12);
	acknowledgement.peerIdentity = Identity(0x30);
	acknowledgement.deltaId = UINT64_C(0x4847464544434241);
	acknowledgement.payloadChecksum = UINT32_C(0x52515049);
	CoopTacticalDeltaAckBytes bytes{};
	CHECK(EncodeCoopTacticalDeltaAck(acknowledgement, bytes) ==
		CoopTacticalCodecResult::Success, "delta ACK encodes");
	CHECK(bytes[8] == 5 && bytes[76] == 0 && bytes[79] == 0,
		"delta ACK kind and reserve are exact");
	CoopTacticalDeltaAck decoded;
	CHECK(DecodeCoopTacticalDeltaAck(bytes.data(), bytes.size(), decoded) ==
		CoopTacticalCodecResult::Success &&
		decoded.peerIdentity == acknowledgement.peerIdentity,
		"delta ACK round trip succeeds");
	for (std::size_t size = 0; size < bytes.size(); ++size)
		CHECK(DecodeCoopTacticalDeltaAck(bytes.data(), size, decoded) !=
			CoopTacticalCodecResult::Success,
			"every truncated delta ACK is rejected");
	auto malformed = bytes;
	malformed[76] = 1;
	CHECK(DecodeCoopTacticalDeltaAck(
		malformed.data(), malformed.size(), decoded) ==
		CoopTacticalCodecResult::Invalid,
		"nonzero delta ACK reserve is rejected");
}

void TestResyncRequestCodec()
{
	CoopTacticalResyncRequest request;
	request.acceptedState = State();
	request.requestId = UINT64_C(0x4847464544434241);
	request.acceptedBaselineId = UINT64_C(0x5857565554535251);
	request.lastAppliedDeltaId = UINT64_C(0x6867666564636261);
	request.lastPayloadChecksum = UINT32_C(0x74737271);
	request.reason = CoopTacticalResyncReason::ReplicaRejected;
	request.nextExpectedCommandId = UINT64_C(0x8887868584838281);
	CoopTacticalResyncRequestBytes bytes{};
	CHECK(EncodeCoopTacticalResyncRequest(request, bytes) ==
		CoopTacticalCodecResult::Success,
		"canonical tactical resync request encodes");
	const CoopTacticalResyncRequestBytes golden{{
		0x4a, 0x32, 0x43, 0x54, 0x03, 0x00, 0x07, 0x00,
		0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
		0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
		0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
		0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
		0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
		0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
		0x71, 0x72, 0x73, 0x74, 0x04, 0x00, 0x00, 0x00,
		0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88}};
	CHECK(bytes == golden,
		"tactical resync request has one fixed little-endian representation");

	CoopTacticalResyncRequest decoded;
	decoded.requestId = 999;
	CHECK(DecodeCoopTacticalResyncRequest(bytes.data(), bytes.size(), decoded) ==
			CoopTacticalCodecResult::Success &&
		SameState(decoded.acceptedState, request.acceptedState) &&
		decoded.requestId == request.requestId &&
		decoded.acceptedBaselineId == request.acceptedBaselineId &&
		decoded.lastAppliedDeltaId == request.lastAppliedDeltaId &&
		decoded.lastPayloadChecksum == request.lastPayloadChecksum &&
		decoded.reason == request.reason &&
		decoded.nextExpectedCommandId == request.nextExpectedCommandId,
		"tactical resync request round trips every checkpoint field");

	bool rejectsEveryTruncation = true;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopTacticalResyncRequest retained;
		retained.requestId = 999;
		if (DecodeCoopTacticalResyncRequest(bytes.data(), size, retained) ==
				CoopTacticalCodecResult::Success || retained.requestId != 999)
			rejectsEveryTruncation = false;
	}
	CHECK(rejectsEveryTruncation,
		"every truncated resync request is rejected transactionally");

	auto malformed = bytes;
	CoopTacticalResyncRequest retained;
	retained.requestId = 999;
	malformed[77] = 1;
	CHECK(DecodeCoopTacticalResyncRequest(malformed.data(), malformed.size(),
		retained) == CoopTacticalCodecResult::Invalid &&
		retained.requestId == 999,
		"resync reserved bytes are rejected transactionally");
	malformed = bytes;
	malformed[76] = 0;
	CHECK(DecodeCoopTacticalResyncRequest(malformed.data(), malformed.size(),
		retained) == CoopTacticalCodecResult::Invalid,
		"unknown resync reasons are rejected");
	malformed = bytes;
	malformed[48] = malformed[49] = malformed[50] = malformed[51] = 0;
	malformed[52] = malformed[53] = malformed[54] = malformed[55] = 0;
	CHECK(DecodeCoopTacticalResyncRequest(malformed.data(), malformed.size(),
		retained) == CoopTacticalCodecResult::Invalid,
		"zero resync request identity is rejected");
	malformed = bytes;
	malformed[56] = malformed[57] = malformed[58] = malformed[59] = 0;
	malformed[60] = malformed[61] = malformed[62] = malformed[63] = 0;
	CHECK(DecodeCoopTacticalResyncRequest(malformed.data(), malformed.size(),
		retained) == CoopTacticalCodecResult::Invalid,
		"zero accepted baseline identity is rejected");
	malformed = bytes;
	malformed[8] = static_cast<std::uint8_t>(
		CoopTacticalWireMessageKind::DeltaAck);
	CHECK(DecodeCoopTacticalResyncRequest(malformed.data(), malformed.size(),
		retained) == CoopTacticalCodecResult::WrongMessageKind,
		"another tactical message kind cannot decode as resync");
	malformed = bytes;
	malformed[4] = 1;
	CHECK(DecodeCoopTacticalResyncRequest(malformed.data(), malformed.size(),
		retained) == CoopTacticalCodecResult::UnsupportedVersion,
		"old tactical wire versions cannot decode the resync contract");
	CHECK(DecodeCoopTacticalResyncRequest(nullptr, bytes.size(), retained) ==
		CoopTacticalCodecResult::Invalid,
		"null resync bytes are rejected");
	std::vector<std::uint8_t> trailing(bytes.begin(), bytes.end());
	trailing.push_back(0);
	CHECK(DecodeCoopTacticalResyncRequest(trailing.data(), trailing.size(),
		retained) == CoopTacticalCodecResult::Invalid &&
		retained.requestId == 999,
		"trailing resync bytes are rejected transactionally");

	for (std::uint8_t value = 1; value <= 6; ++value)
		CHECK(IsKnownCoopTacticalResyncReason(
			static_cast<CoopTacticalResyncReason>(value)),
			"every published resync reason is recognized");
	CHECK(!IsKnownCoopTacticalResyncReason(
		static_cast<CoopTacticalResyncReason>(7)),
		"unpublished resync reasons are rejected");

	CoopTacticalResyncRequestBytes unchanged{};
	unchanged.fill(0xaa);
	request.requestId = 0;
	CHECK(EncodeCoopTacticalResyncRequest(request, unchanged) ==
			CoopTacticalCodecResult::Invalid && unchanged[0] == 0xaa,
		"invalid resync encode preserves caller bytes");
	request.requestId = 1;
	request.acceptedBaselineId = 0;
	CHECK(EncodeCoopTacticalResyncRequest(request, unchanged) ==
		CoopTacticalCodecResult::Invalid,
		"resync encode requires an accepted baseline");
	request.acceptedBaselineId = 1;
	request.reason = static_cast<CoopTacticalResyncReason>(255);
	CHECK(EncodeCoopTacticalResyncRequest(request, unchanged) ==
		CoopTacticalCodecResult::Invalid,
		"resync encode rejects unknown reasons");
}

void TestExplicitPayloadCeilings()
{
	const CoopTacticalStateIdentity state = State(50, 9);
	std::vector<TacticalActorSnapshot> actors;
	actors.reserve(MaximumCoopTacticalSnapshotActors + 1);
	for (std::size_t index = 0;
		index <= MaximumCoopTacticalSnapshotActors; ++index)
	{
		TacticalActorSnapshot actor = Actor();
		actor.id = TacticalEntityId{
			static_cast<std::uint16_t>(index + 1), 1};
		actors.push_back(actor);
	}
	std::vector<TacticalActorSnapshot> maximumActors(
		actors.begin(), actors.end() - 1);
	std::vector<TacticalDoorSnapshot> maximumDoors;
	maximumDoors.reserve(MaximumCoopTacticalSnapshotDoors);
	for (std::size_t index = 0;
		index < MaximumCoopTacticalSnapshotDoors; ++index)
		maximumDoors.push_back(TacticalDoorSnapshot{
			static_cast<std::int32_t>(index),
			static_cast<std::uint16_t>(index + 1), (index & 1u) != 0});
	CoopTacticalBaseline baseline;
	baseline.state = state;
	baseline.baselineId = 1;
	CHECK(TacticalWorldSnapshot::create(state.worldGeneration,
		TacticalWorldDimensions{160, 160}, {},
		TacticalTurnSnapshot{true, true, 0, state.turnSerial},
		std::move(maximumActors), std::move(maximumDoors), baseline.snapshot) ==
		TacticalSnapshotCreateError::None,
		"maximum live baseline fixture is valid");
	std::vector<std::uint8_t> bytes;
	CHECK(EncodeCoopTacticalBaseline(baseline, bytes) ==
		CoopTacticalCodecResult::Success &&
		bytes.size() == CoopTacticalBaselineHeaderWireSize +
			MaximumCoopTacticalBaselinePayloadWireSize,
		"exact 256-actor/1024-door baseline reaches the payload ceiling");
	CHECK(TacticalWorldSnapshot::create(state.worldGeneration,
		TacticalWorldDimensions{160, 160}, {},
		TacticalTurnSnapshot{true, true, 0, state.turnSerial},
		std::move(actors), baseline.snapshot) ==
		TacticalSnapshotCreateError::None,
		"wider SDK snapshot fixture remains constructible");
	const std::vector<std::uint8_t> retained = bytes;
	CHECK(EncodeCoopTacticalBaseline(baseline, bytes) ==
		CoopTacticalCodecResult::PayloadTooLarge && bytes == retained,
		"257th actor is rejected transactionally at co-op boundary");

	CoopTacticalDelta delta;
	delta.state = state;
	delta.deltaId = 1;
	delta.baseRevision = 49;
	delta.delta.previousEpoch = state.worldGeneration;
	delta.delta.currentEpoch = state.worldGeneration;
	delta.delta.events.reserve(MaximumCoopTacticalDeltaEvents + 1);
	for (std::size_t index = 0;
		index < MaximumCoopTacticalDeltaEvents; ++index)
		delta.delta.events.push_back(TacticalActorLeftEvent{
			TacticalEntityId{static_cast<std::uint16_t>(index + 1), 1}});
	CHECK(EncodeCoopTacticalDelta(delta, bytes) ==
		CoopTacticalCodecResult::Success,
		"exact bounded delta event ceiling encodes");
	delta.delta.events.push_back(TacticalActorLeftEvent{
		TacticalEntityId{
			static_cast<std::uint16_t>(MaximumCoopTacticalDeltaEvents + 1), 1}});
	CHECK(EncodeCoopTacticalDelta(delta, bytes) ==
		CoopTacticalCodecResult::PayloadTooLarge,
		"first event beyond delta ceiling is rejected");
}

void TestDisjointDoorSetsReachTheExactCategoryAwareDeltaBound()
{
	const CoopTacticalStateIdentity state = State(77, 19);
	std::vector<TacticalActorSnapshot> previousActors;
	std::vector<TacticalActorSnapshot> currentActors;
	previousActors.reserve(MaximumCoopTacticalSnapshotActors);
	currentActors.reserve(MaximumCoopTacticalSnapshotActors);
	for (std::size_t index = 0;
		index < MaximumCoopTacticalSnapshotActors; ++index)
	{
		TacticalActorSnapshot previous = Actor();
		previous.id = TacticalEntityId{
			static_cast<std::uint16_t>(index + 1), 1};
		previous.grid = static_cast<std::int32_t>(4000 + index);
		previous.level = 0;
		previous.direction = 1;
		previous.stance = TacticalStance::Standing;
		previous.animation = 10;
		previous.actionPoints = 20;
		previous.life = 80;
		previous.maximumLife = 90;
		previous.breath = 70;
		previous.maximumBreath = 100;
		previous.hostileToPlayerTeam = false;
		previous.loadout.helmet = TacticalHandItemSnapshot{
			40, 1, 90, 0, 0, 0, false, false};
		previous.loadout.vest = TacticalHandItemSnapshot{
			41, 1, 80, 0, 0, 0, false, false};
		previous.loadout.legs = TacticalHandItemSnapshot{
			42, 1, 70, 0, 0, 0, false, false};
		previous.loadout.primaryHand = TacticalHandItemSnapshot{
			10, 1, 90, 20, 30, 100, true, true};
		previous.loadout.secondaryHand = TacticalHandItemSnapshot{
			30, 1, 80, 0, 0, 0, false, false};
		TacticalActorSnapshot current = previous;
		current.grid++;
		current.level = 1;
		current.direction = 2;
		current.stance = TacticalStance::Crouched;
		current.animation = 11;
		current.actionPoints = 19;
		current.life = 79;
		current.maximumLife = 89;
		current.breath = 69;
		current.maximumBreath = 99;
		current.hostileToPlayerTeam = true;
		current.loadout.helmet = TacticalHandItemSnapshot{
			43, 1, 89, 0, 0, 0, false, false};
		current.loadout.vest = TacticalHandItemSnapshot{
			44, 1, 79, 0, 0, 0, false, false};
		current.loadout.legs = TacticalHandItemSnapshot{
			45, 1, 69, 0, 0, 0, false, false};
		current.loadout.primaryHand = TacticalHandItemSnapshot{
			10, 1, 89, 20, 29, -7, true, false};
		current.loadout.secondaryHand = TacticalHandItemSnapshot{
			31, 1, 79, 0, 0, 0, false, false};
		previousActors.push_back(previous);
		currentActors.push_back(current);
	}
	std::vector<TacticalDoorSnapshot> previousDoors;
	std::vector<TacticalDoorSnapshot> currentDoors;
	previousDoors.reserve(MaximumCoopTacticalSnapshotDoors);
	currentDoors.reserve(MaximumCoopTacticalSnapshotDoors);
	for (std::size_t index = 0;
		index < MaximumCoopTacticalSnapshotDoors; ++index)
	{
		previousDoors.push_back(TacticalDoorSnapshot{
			static_cast<std::int32_t>(index),
			static_cast<std::uint16_t>(index + 1), false});
		currentDoors.push_back(TacticalDoorSnapshot{
			static_cast<std::int32_t>(2000 + index),
			static_cast<std::uint16_t>(index + 1), true});
	}
	TacticalWorldSnapshot previous;
	TacticalWorldSnapshot current;
	CHECK(TacticalWorldSnapshot::create(state.worldGeneration,
		TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{1, 1, 0, true},
		TacticalTurnSnapshot{true, true, 0, 18},
		std::move(previousActors), std::move(previousDoors), previous,
		MaximumCoopTacticalSnapshotActors,
		MaximumCoopTacticalSnapshotDoors) == TacticalSnapshotCreateError::None,
		"maximum previous disjoint-door snapshot is valid");
	CHECK(TacticalWorldSnapshot::create(state.worldGeneration,
		TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{2, 1, 0, true},
		TacticalTurnSnapshot{true, true, 1, state.turnSerial},
		std::move(currentActors), std::move(currentDoors), current,
		MaximumCoopTacticalSnapshotActors,
		MaximumCoopTacticalSnapshotDoors) == TacticalSnapshotCreateError::None,
		"maximum current disjoint-door snapshot is valid");

	TacticalWorldDelta inner;
	CHECK(DiffTacticalWorldSnapshots(previous, current,
		MaximumCoopTacticalDeltaEvents, inner) ==
			TacticalWorldDiffResult::Success &&
		inner.events.size() == MaximumCoopTacticalDeltaEvents,
		"two individually valid disjoint door sets fit the exact event bound");
	std::vector<std::uint8_t> innerBytes;
	CHECK(EncodeTacticalWorldDelta(inner, innerBytes,
		MaximumCoopTacticalDeltaEvents) ==
			TacticalWorldDeltaEncodeResult::Success &&
		innerBytes.size() == MaximumCoopTacticalDeltaPayloadWireSize,
		"category-aware maximum delta reaches exactly 61,504 bytes");

	CoopTacticalDelta envelope;
	envelope.state = state;
	envelope.deltaId = 5;
	envelope.baseRevision = state.revision - 1;
	envelope.delta = std::move(inner);
	std::vector<std::uint8_t> envelopeBytes;
	CHECK(EncodeCoopTacticalDelta(envelope, envelopeBytes) ==
		CoopTacticalCodecResult::Success &&
		envelopeBytes.size() == MaximumCoopTacticalDeltaWireSize &&
		envelopeBytes.size() < MaximumCoopTacticalPayloadWireSize,
		"maximum legal delta envelope stays below the 64-KiB transport ceiling");
	CoopTacticalDelta decoded;
	CHECK(DecodeCoopTacticalDelta(envelopeBytes, decoded) ==
		CoopTacticalCodecResult::Success &&
		decoded.delta.events.size() == MaximumCoopTacticalDeltaEvents &&
		std::holds_alternative<TacticalDoorEnteredEvent>(
			decoded.delta.events[2 + 4 * MaximumCoopTacticalSnapshotActors]) &&
		std::holds_alternative<TacticalDoorLeftEvent>(
			decoded.delta.events[2 + 4 * MaximumCoopTacticalSnapshotActors +
				MaximumCoopTacticalSnapshotDoors]),
		"maximum disjoint-set envelope round trips in canonical category order");
}
}

int main()
{
	TestChecksum();
	TestReceiptCodec();
	TestBaselineCodec();
	TestBaselineAckCodec();
	TestDeltaCodec();
	TestDeltaAckCodec();
	TestResyncRequestCodec();
	TestExplicitPayloadCeilings();
	TestDisjointDoorSetsReachTheExactCategoryAwareDeltaBound();
	if (failures == 0)
		std::printf("all coop tactical protocol tests passed\n");
	return failures == 0 ? 0 : 1;
}
