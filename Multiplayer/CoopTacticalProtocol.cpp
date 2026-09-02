#include "CoopTacticalProtocol.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace CoopSession
{
namespace
{
constexpr std::uint8_t TacticalWireMagic[4] = {'J', '2', 'C', 'T'};
constexpr std::uint8_t TacticalReceiptMagic[4] = {'J', '2', 'C', 'R'};

void WriteU16(std::uint8_t*& output, std::uint16_t value) noexcept
{
	*output++ = static_cast<std::uint8_t>(value);
	*output++ = static_cast<std::uint8_t>(value >> 8);
}

void WriteU32(std::uint8_t*& output, std::uint32_t value) noexcept
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		*output++ = static_cast<std::uint8_t>(value >> shift);
}

void WriteU64(std::uint8_t*& output, std::uint64_t value) noexcept
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		*output++ = static_cast<std::uint8_t>(value >> shift);
}

std::uint16_t ReadU16(const std::uint8_t*& input) noexcept
{
	const std::uint16_t value = static_cast<std::uint16_t>(input[0]) |
		(static_cast<std::uint16_t>(input[1]) << 8);
	input += 2;
	return value;
}

std::uint32_t ReadU32(const std::uint8_t*& input) noexcept
{
	std::uint32_t value = 0;
	for (unsigned shift = 0; shift < 32; shift += 8)
		value |= static_cast<std::uint32_t>(*input++) << shift;
	return value;
}

std::uint64_t ReadU64(const std::uint8_t*& input) noexcept
{
	std::uint64_t value = 0;
	for (unsigned shift = 0; shift < 64; shift += 8)
		value |= static_cast<std::uint64_t>(*input++) << shift;
	return value;
}

void WriteCommonHeader(
	std::uint8_t*& output,
	CoopTacticalWireMessageKind kind,
	const CoopTacticalStateIdentity& state) noexcept
{
	std::copy(TacticalWireMagic, TacticalWireMagic + 4, output);
	output += 4;
	WriteU16(output, state.wireVersion);
	WriteU16(output, state.protocolVersion);
	*output++ = static_cast<std::uint8_t>(kind);
	for (unsigned index = 0; index < 7; ++index) *output++ = 0;
	WriteU64(output, state.sessionEpoch);
	WriteU64(output, state.worldGeneration);
	WriteU64(output, state.revision);
	WriteU64(output, state.turnSerial);
}

CoopTacticalCodecResult ReadCommonHeader(
	const std::uint8_t* bytes,
	std::size_t size,
	CoopTacticalWireMessageKind expectedKind,
	CoopTacticalStateIdentity& state,
	const std::uint8_t*& input) noexcept
{
	if (bytes == nullptr || size < CoopTacticalCommonHeaderWireSize)
		return CoopTacticalCodecResult::Invalid;
	if (!std::equal(TacticalWireMagic, TacticalWireMagic + 4, bytes))
		return CoopTacticalCodecResult::Invalid;
	input = bytes + 4;
	state.wireVersion = ReadU16(input);
	if (state.wireVersion != CoopTacticalWireVersion)
		return CoopTacticalCodecResult::UnsupportedVersion;
	state.protocolVersion = ReadU16(input);
	if (state.protocolVersion != CurrentProtocolVersion)
		return CoopTacticalCodecResult::UnsupportedVersion;
	const CoopTacticalWireMessageKind kind =
		static_cast<CoopTacticalWireMessageKind>(*input++);
	if (!IsKnownCoopTacticalWireMessageKind(kind) || kind != expectedKind)
		return CoopTacticalCodecResult::WrongMessageKind;
	for (unsigned index = 0; index < 7; ++index)
		if (*input++ != 0) return CoopTacticalCodecResult::Invalid;
	state.sessionEpoch = ReadU64(input);
	state.worldGeneration = ReadU64(input);
	state.revision = ReadU64(input);
	state.turnSerial = ReadU64(input);
	return IsValidCoopTacticalStateIdentity(state)
		? CoopTacticalCodecResult::Success
		: CoopTacticalCodecResult::Invalid;
}

bool IsValidReceipt(const CoopTacticalIntentReceipt& receipt) noexcept
{
	const std::uint64_t commandAfter = receipt.commandId ==
		std::numeric_limits<std::uint64_t>::max()
		? 0 : receipt.commandId + 1;
	const bool reportsNonConsumingCursor = receipt.status ==
		CoopTacticalIntentReceiptStatus::Rejected &&
		(receipt.reason ==
			CoopTacticalIntentReceiptReason::InvalidCommandSequence ||
		 receipt.reason ==
			CoopTacticalIntentReceiptReason::InboxSequenceExhausted);
	if (!IsValidCoopTacticalStateIdentity(receipt.state) ||
		IsZero(receipt.peerIdentity) || receipt.commandId == 0 ||
		(!reportsNonConsumingCursor &&
			receipt.nextExpectedCommandId != commandAfter) ||
		(receipt.reason ==
			CoopTacticalIntentReceiptReason::InvalidCommandSequence &&
			receipt.nextExpectedCommandId == 0) ||
		(receipt.reason ==
			CoopTacticalIntentReceiptReason::InboxSequenceExhausted &&
			receipt.nextExpectedCommandId != 0) ||
		!IsKnownCoopTacticalIntentReceiptStatus(receipt.status) ||
		!IsKnownCoopTacticalIntentReceiptReason(receipt.reason))
		return false;

	switch (receipt.status)
	{
		case CoopTacticalIntentReceiptStatus::Queued:
			return receipt.reason == CoopTacticalIntentReceiptReason::None;
		case CoopTacticalIntentReceiptStatus::Rejected:
			return receipt.authoritativeSequence == 0 &&
				receipt.reason != CoopTacticalIntentReceiptReason::None;
		case CoopTacticalIntentReceiptStatus::Applied:
			return receipt.reason == CoopTacticalIntentReceiptReason::None;
		case CoopTacticalIntentReceiptStatus::Discarded:
			return receipt.reason ==
				CoopTacticalIntentReceiptReason::AuthoritativeDiscard;
		case CoopTacticalIntentReceiptStatus::Cancelled:
			return receipt.reason ==
				CoopTacticalIntentReceiptReason::SessionEnded;
	}
	return false;
}

bool ValidAssignedActors(
	const std::vector<TacticalEntityId>& actors) noexcept
{
	if (actors.size() > MaximumCoopTacticalAssignedActors) return false;
	for (std::size_t index = 0; index < actors.size(); ++index)
	{
		if (!actors[index].valid()) return false;
		if (index != 0 && !(actors[index - 1] < actors[index])) return false;
	}
	return true;
}

void WriteEntity(std::uint8_t*& output, TacticalEntityId entity) noexcept
{
	WriteU16(output, entity.slot);
	WriteU32(output, entity.incarnation);
}

TacticalEntityId ReadEntity(const std::uint8_t*& input) noexcept
{
	TacticalEntityId entity;
	entity.slot = ReadU16(input);
	entity.incarnation = ReadU32(input);
	return entity;
}

const TacticalEntityId* EventActor(const TacticalWorldEvent& event) noexcept
{
	switch (event.index())
	{
		case 3: return &std::get<TacticalActorEnteredEvent>(event).actor.id;
		case 4: return &std::get<TacticalActorLeftEvent>(event).actor;
		case 5: return &std::get<TacticalActorMovedEvent>(event).actor;
		case 6: return &std::get<TacticalActorStanceChangedEvent>(event).actor;
		case 7: return &std::get<TacticalActorVitalsChangedEvent>(event).actor;
		case 8: return &std::get<TacticalActorLoadoutChangedEvent>(event).actor;
		default: return nullptr;
	}
}

const std::int32_t* EventDoorGrid(const TacticalWorldEvent& event) noexcept
{
	switch (event.index())
	{
		case 9: return &std::get<TacticalDoorEnteredEvent>(event).door.baseGrid;
		case 10: return &std::get<TacticalDoorLeftEvent>(event).baseGrid;
		case 11: return &std::get<TacticalDoorChangedEvent>(event).previous.baseGrid;
		default: return nullptr;
	}
}

bool IsCanonicalDelta(
	const TacticalWorldDelta& delta,
	std::uint64_t resultingTurnSerial) noexcept
{
	std::size_t previousKind = 0;
	bool havePrevious = false;
	const TacticalEntityId* previousActor = nullptr;
	const std::int32_t* previousDoorGrid = nullptr;
	for (const TacticalWorldEvent& event : delta.events)
	{
		if (event.valueless_by_exception() || event.index() == 0)
			return false;
		if (havePrevious && event.index() < previousKind) return false;
		if (havePrevious && event.index() == previousKind)
		{
			if (event.index() < 3) return false;
			const TacticalEntityId* actor = EventActor(event);
			const std::int32_t* doorGrid = EventDoorGrid(event);
			if ((actor == nullptr || previousActor == nullptr ||
				 !(*previousActor < *actor)) &&
				(doorGrid == nullptr || previousDoorGrid == nullptr ||
				 *previousDoorGrid >= *doorGrid))
				return false;
		}
		else
		{
			previousActor = nullptr;
			previousDoorGrid = nullptr;
		}
		if (event.index() == 2 &&
			std::get<TacticalTurnChangedEvent>(event).current.serial !=
				resultingTurnSerial)
			return false;
		previousKind = event.index();
		previousActor = EventActor(event);
		previousDoorGrid = EventDoorGrid(event);
		havePrevious = true;
	}
	return true;
}

CoopTacticalCodecResult MapSnapshotEncodeResult(
	TacticalWorldSnapshotEncodeResult result) noexcept
{
	switch (result)
	{
		case TacticalWorldSnapshotEncodeResult::Success:
			return CoopTacticalCodecResult::Success;
		case TacticalWorldSnapshotEncodeResult::TooManyActors:
		case TacticalWorldSnapshotEncodeResult::TooManyDoors:
			return CoopTacticalCodecResult::PayloadTooLarge;
		case TacticalWorldSnapshotEncodeResult::AllocationFailure:
			return CoopTacticalCodecResult::AllocationFailure;
		case TacticalWorldSnapshotEncodeResult::Invalid:
			return CoopTacticalCodecResult::InvalidPayload;
	}
	return CoopTacticalCodecResult::InvalidPayload;
}

CoopTacticalCodecResult MapSnapshotDecodeResult(
	TacticalWorldSnapshotDecodeResult result) noexcept
{
	switch (result)
	{
		case TacticalWorldSnapshotDecodeResult::Success:
			return CoopTacticalCodecResult::Success;
		case TacticalWorldSnapshotDecodeResult::UnsupportedVersion:
			return CoopTacticalCodecResult::InvalidPayload;
		case TacticalWorldSnapshotDecodeResult::TooManyActors:
		case TacticalWorldSnapshotDecodeResult::TooManyDoors:
			return CoopTacticalCodecResult::PayloadTooLarge;
		case TacticalWorldSnapshotDecodeResult::AllocationFailure:
			return CoopTacticalCodecResult::AllocationFailure;
		case TacticalWorldSnapshotDecodeResult::Invalid:
			return CoopTacticalCodecResult::InvalidPayload;
	}
	return CoopTacticalCodecResult::InvalidPayload;
}

CoopTacticalCodecResult MapDeltaEncodeResult(
	TacticalWorldDeltaEncodeResult result) noexcept
{
	switch (result)
	{
		case TacticalWorldDeltaEncodeResult::Success:
			return CoopTacticalCodecResult::Success;
		case TacticalWorldDeltaEncodeResult::TooManyEvents:
			return CoopTacticalCodecResult::PayloadTooLarge;
		case TacticalWorldDeltaEncodeResult::AllocationFailure:
			return CoopTacticalCodecResult::AllocationFailure;
		case TacticalWorldDeltaEncodeResult::Invalid:
			return CoopTacticalCodecResult::InvalidPayload;
	}
	return CoopTacticalCodecResult::InvalidPayload;
}

CoopTacticalCodecResult MapDeltaDecodeResult(
	TacticalWorldDeltaDecodeResult result) noexcept
{
	switch (result)
	{
		case TacticalWorldDeltaDecodeResult::Success:
			return CoopTacticalCodecResult::Success;
		case TacticalWorldDeltaDecodeResult::UnsupportedVersion:
			return CoopTacticalCodecResult::InvalidPayload;
		case TacticalWorldDeltaDecodeResult::TooManyEvents:
			return CoopTacticalCodecResult::PayloadTooLarge;
		case TacticalWorldDeltaDecodeResult::AllocationFailure:
			return CoopTacticalCodecResult::AllocationFailure;
		case TacticalWorldDeltaDecodeResult::Invalid:
			return CoopTacticalCodecResult::InvalidPayload;
	}
	return CoopTacticalCodecResult::InvalidPayload;
}
}

bool IsKnownCoopTacticalWireMessageKind(
	CoopTacticalWireMessageKind kind) noexcept
{
	switch (kind)
	{
		case CoopTacticalWireMessageKind::IntentReceipt:
		case CoopTacticalWireMessageKind::Baseline:
		case CoopTacticalWireMessageKind::BaselineAck:
		case CoopTacticalWireMessageKind::Delta:
		case CoopTacticalWireMessageKind::DeltaAck:
		case CoopTacticalWireMessageKind::ResyncRequest:
			return true;
	}
	return false;
}

bool IsKnownCoopTacticalResyncReason(
	CoopTacticalResyncReason reason) noexcept
{
	switch (reason)
	{
		case CoopTacticalResyncReason::DeltaSequenceGap:
		case CoopTacticalResyncReason::PayloadChecksumMismatch:
		case CoopTacticalResyncReason::StateMismatch:
		case CoopTacticalResyncReason::ReplicaRejected:
		case CoopTacticalResyncReason::InvalidEnvelope:
		case CoopTacticalResyncReason::BaselineRejected:
			return true;
	}
	return false;
}

bool IsKnownCoopTacticalIntentReceiptStatus(
	CoopTacticalIntentReceiptStatus status) noexcept
{
	switch (status)
	{
		case CoopTacticalIntentReceiptStatus::Queued:
		case CoopTacticalIntentReceiptStatus::Rejected:
		case CoopTacticalIntentReceiptStatus::Applied:
		case CoopTacticalIntentReceiptStatus::Discarded:
		case CoopTacticalIntentReceiptStatus::Cancelled:
			return true;
	}
	return false;
}

bool IsKnownCoopTacticalIntentReceiptReason(
	CoopTacticalIntentReceiptReason reason) noexcept
{
	switch (reason)
	{
		case CoopTacticalIntentReceiptReason::None:
		case CoopTacticalIntentReceiptReason::MalformedIntent:
		case CoopTacticalIntentReceiptReason::NotAdmitted:
		case CoopTacticalIntentReceiptReason::SessionMismatch:
		case CoopTacticalIntentReceiptReason::WorldMismatch:
		case CoopTacticalIntentReceiptReason::RevisionMismatch:
		case CoopTacticalIntentReceiptReason::TurnMismatch:
		case CoopTacticalIntentReceiptReason::InvalidCommandSequence:
		case CoopTacticalIntentReceiptReason::ActorNotOwned:
		case CoopTacticalIntentReceiptReason::NotBaselineReady:
		case CoopTacticalIntentReceiptReason::ActorUnavailable:
		case CoopTacticalIntentReceiptReason::WrongTeam:
		case CoopTacticalIntentReceiptReason::GameplayRejected:
		case CoopTacticalIntentReceiptReason::InboxCapacityReached:
		case CoopTacticalIntentReceiptReason::InboxSequenceExhausted:
		case CoopTacticalIntentReceiptReason::AllocationFailure:
		case CoopTacticalIntentReceiptReason::QueueUnavailable:
		case CoopTacticalIntentReceiptReason::UnavailableContext:
		case CoopTacticalIntentReceiptReason::AuthoritativeDiscard:
		case CoopTacticalIntentReceiptReason::SessionEnded:
		case CoopTacticalIntentReceiptReason::AuthoritySequenceExhausted:
			return true;
	}
	return false;
}

bool IsValidCoopTacticalStateIdentity(
	const CoopTacticalStateIdentity& identity) noexcept
{
	return identity.wireVersion == CoopTacticalWireVersion &&
		identity.protocolVersion == CurrentProtocolVersion &&
		identity.sessionEpoch != 0 && identity.worldGeneration != 0 &&
		identity.revision != 0 && identity.turnSerial != 0;
}

std::uint32_t CoopTacticalPayloadChecksum(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (bytes == nullptr && size != 0) return 0;
	std::uint32_t checksum = 2166136261u;
	for (std::size_t index = 0; index < size; ++index)
	{
		checksum ^= bytes[index];
		checksum *= 16777619u;
	}
	return checksum;
}

CoopTacticalCodecResult EncodeCoopTacticalIntentReceipt(
	const CoopTacticalIntentReceipt& receipt,
	CoopTacticalIntentReceiptBytes& bytes) noexcept
{
	if (!IsValidReceipt(receipt)) return CoopTacticalCodecResult::Invalid;
	CoopTacticalIntentReceiptBytes encoded{};
	std::uint8_t* output = encoded.data();
	std::copy(TacticalReceiptMagic, TacticalReceiptMagic + 4, output);
	output += 4;
	WriteU16(output, receipt.state.wireVersion);
	WriteU16(output, receipt.state.protocolVersion);
	*output++ = static_cast<std::uint8_t>(receipt.status);
	*output++ = static_cast<std::uint8_t>(receipt.reason);
	for (unsigned index = 0; index < 6; ++index) *output++ = 0;
	WriteU64(output, receipt.state.sessionEpoch);
	std::copy(receipt.peerIdentity.begin(), receipt.peerIdentity.end(), output);
	output += receipt.peerIdentity.size();
	WriteU64(output, receipt.commandId);
	WriteU64(output, receipt.nextExpectedCommandId);
	WriteU64(output, receipt.state.worldGeneration);
	WriteU64(output, receipt.state.revision);
	WriteU64(output, receipt.state.turnSerial);
	WriteU64(output, receipt.authoritativeSequence);
	WriteU64(output, receipt.simulationTick);
	if (output != encoded.data() + encoded.size())
		return CoopTacticalCodecResult::Invalid;
	bytes = encoded;
	return CoopTacticalCodecResult::Success;
}

CoopTacticalCodecResult DecodeCoopTacticalIntentReceipt(
	const std::uint8_t* bytes,
	std::size_t size,
	CoopTacticalIntentReceipt& receipt) noexcept
{
	if (bytes == nullptr || size != CoopTacticalIntentReceiptWireSize)
		return CoopTacticalCodecResult::Invalid;
	if (!std::equal(TacticalReceiptMagic, TacticalReceiptMagic + 4, bytes))
		return CoopTacticalCodecResult::Invalid;
	CoopTacticalIntentReceipt decoded;
	const std::uint8_t* input = bytes + 4;
	decoded.state.wireVersion = ReadU16(input);
	if (decoded.state.wireVersion != CoopTacticalWireVersion)
		return CoopTacticalCodecResult::UnsupportedVersion;
	decoded.state.protocolVersion = ReadU16(input);
	if (decoded.state.protocolVersion != CurrentProtocolVersion)
		return CoopTacticalCodecResult::UnsupportedVersion;
	decoded.status = static_cast<CoopTacticalIntentReceiptStatus>(*input++);
	decoded.reason = static_cast<CoopTacticalIntentReceiptReason>(*input++);
	for (unsigned index = 0; index < 6; ++index)
		if (*input++ != 0) return CoopTacticalCodecResult::Invalid;
	decoded.state.sessionEpoch = ReadU64(input);
	std::copy(input, input + decoded.peerIdentity.size(),
		decoded.peerIdentity.begin());
	input += decoded.peerIdentity.size();
	decoded.commandId = ReadU64(input);
	decoded.nextExpectedCommandId = ReadU64(input);
	decoded.state.worldGeneration = ReadU64(input);
	decoded.state.revision = ReadU64(input);
	decoded.state.turnSerial = ReadU64(input);
	decoded.authoritativeSequence = ReadU64(input);
	decoded.simulationTick = ReadU64(input);
	if (input != bytes + CoopTacticalIntentReceiptWireSize ||
		!IsValidReceipt(decoded))
		return CoopTacticalCodecResult::Invalid;
	receipt = decoded;
	return CoopTacticalCodecResult::Success;
}

CoopTacticalCodecResult EncodeCoopTacticalBaseline(
	const CoopTacticalBaseline& baseline,
	std::vector<std::uint8_t>& bytes) noexcept
{
	if (baseline.assignedActors.size() > MaximumCoopTacticalAssignedActors)
		return CoopTacticalCodecResult::PayloadTooLarge;
	if (!IsValidCoopTacticalStateIdentity(baseline.state) ||
		baseline.baselineId == 0 ||
		!ValidAssignedActors(baseline.assignedActors) ||
		baseline.snapshot.epoch() != baseline.state.worldGeneration ||
		baseline.snapshot.turn().serial != baseline.state.turnSerial)
		return CoopTacticalCodecResult::Invalid;
	for (TacticalEntityId actor : baseline.assignedActors)
		if (baseline.snapshot.find(actor) == nullptr)
			return CoopTacticalCodecResult::Invalid;

	std::vector<std::uint8_t> payload;
	const CoopTacticalCodecResult payloadResult = MapSnapshotEncodeResult(
		EncodeTacticalWorldSnapshot(baseline.snapshot, payload,
			MaximumCoopTacticalSnapshotActors,
			MaximumCoopTacticalSnapshotDoors));
	if (payloadResult != CoopTacticalCodecResult::Success)
		return payloadResult;
	if (payload.empty() ||
		payload.size() > MaximumCoopTacticalBaselinePayloadWireSize ||
		payload.size() > std::numeric_limits<std::uint32_t>::max())
		return CoopTacticalCodecResult::PayloadTooLarge;
	try
	{
		std::vector<std::uint8_t> encoded(
			CoopTacticalBaselineHeaderWireSize +
			baseline.assignedActors.size() * 6 + payload.size());
		std::uint8_t* output = encoded.data();
		WriteCommonHeader(output,
			CoopTacticalWireMessageKind::Baseline, baseline.state);
		WriteU64(output, baseline.baselineId);
		WriteU16(output,
			static_cast<std::uint16_t>(baseline.assignedActors.size()));
		WriteU16(output, 0);
		WriteU32(output, static_cast<std::uint32_t>(payload.size()));
		WriteU32(output, 0);
		WriteU64(output, baseline.nextExpectedCommandId);
		for (TacticalEntityId actor : baseline.assignedActors)
			WriteEntity(output, actor);
		std::copy(payload.begin(), payload.end(), output);
		const std::uint32_t checksum = CoopTacticalPayloadChecksum(
			encoded.data() + CoopTacticalBaselineHeaderWireSize,
			encoded.size() - CoopTacticalBaselineHeaderWireSize);
		if (baseline.payloadChecksum != 0 &&
			baseline.payloadChecksum != checksum)
			return CoopTacticalCodecResult::ChecksumMismatch;
		std::uint8_t* checksumOutput = encoded.data() + 64;
		WriteU32(checksumOutput, checksum);
		bytes = std::move(encoded);
		return CoopTacticalCodecResult::Success;
	}
	catch (...)
	{
		return CoopTacticalCodecResult::AllocationFailure;
	}
}

CoopTacticalCodecResult DecodeCoopTacticalBaseline(
	const std::uint8_t* bytes,
	std::size_t size,
	CoopTacticalBaseline& baseline) noexcept
{
	if (size < CoopTacticalBaselineHeaderWireSize ||
		size > MaximumCoopTacticalBaselineWireSize)
		return size > MaximumCoopTacticalBaselineWireSize
			? CoopTacticalCodecResult::PayloadTooLarge
			: CoopTacticalCodecResult::Invalid;
	CoopTacticalBaseline decoded;
	const std::uint8_t* input = nullptr;
	const CoopTacticalCodecResult header = ReadCommonHeader(
		bytes, size, CoopTacticalWireMessageKind::Baseline,
		decoded.state, input);
	if (header != CoopTacticalCodecResult::Success) return header;
	decoded.baselineId = ReadU64(input);
	const std::uint16_t assignedActorCount = ReadU16(input);
	const std::uint16_t reserved = ReadU16(input);
	const std::uint32_t payloadSize = ReadU32(input);
	decoded.payloadChecksum = ReadU32(input);
	decoded.nextExpectedCommandId = ReadU64(input);
	if (assignedActorCount > MaximumCoopTacticalAssignedActors)
		return CoopTacticalCodecResult::PayloadTooLarge;
	if (decoded.baselineId == 0 || reserved != 0 ||
		payloadSize == 0 ||
		payloadSize > MaximumCoopTacticalBaselinePayloadWireSize ||
		static_cast<std::size_t>(bytes + size - input) !=
			static_cast<std::size_t>(assignedActorCount) * 6 + payloadSize)
		return payloadSize > MaximumCoopTacticalBaselinePayloadWireSize
			? CoopTacticalCodecResult::PayloadTooLarge
			: CoopTacticalCodecResult::Invalid;

	try
	{
		const std::size_t bodySize =
			static_cast<std::size_t>(assignedActorCount) * 6 + payloadSize;
		if (CoopTacticalPayloadChecksum(input, bodySize) !=
			decoded.payloadChecksum)
			return CoopTacticalCodecResult::ChecksumMismatch;
		decoded.assignedActors.reserve(assignedActorCount);
		for (std::uint16_t index = 0; index < assignedActorCount; ++index)
		{
			const TacticalEntityId actor = ReadEntity(input);
			if (!actor.valid() ||
				(index != 0 && !(decoded.assignedActors.back() < actor)))
				return CoopTacticalCodecResult::Invalid;
			decoded.assignedActors.push_back(actor);
		}
		std::vector<std::uint8_t> payload(input, input + payloadSize);
		const CoopTacticalCodecResult payloadResult = MapSnapshotDecodeResult(
			DecodeTacticalWorldSnapshot(payload, decoded.snapshot,
				MaximumCoopTacticalSnapshotActors,
				MaximumCoopTacticalSnapshotDoors));
		if (payloadResult != CoopTacticalCodecResult::Success)
			return payloadResult;
		if (decoded.snapshot.epoch() != decoded.state.worldGeneration ||
			decoded.snapshot.turn().serial != decoded.state.turnSerial)
			return CoopTacticalCodecResult::InvalidPayload;
		for (TacticalEntityId actor : decoded.assignedActors)
			if (decoded.snapshot.find(actor) == nullptr)
				return CoopTacticalCodecResult::InvalidPayload;
		std::vector<std::uint8_t> canonical;
		const CoopTacticalCodecResult canonicalResult = MapSnapshotEncodeResult(
			EncodeTacticalWorldSnapshot(decoded.snapshot, canonical,
				MaximumCoopTacticalSnapshotActors,
				MaximumCoopTacticalSnapshotDoors));
		if (canonicalResult != CoopTacticalCodecResult::Success)
			return canonicalResult;
		if (canonical != payload)
			return CoopTacticalCodecResult::InvalidPayload;
		baseline = std::move(decoded);
		return CoopTacticalCodecResult::Success;
	}
	catch (...)
	{
		return CoopTacticalCodecResult::AllocationFailure;
	}
}

CoopTacticalCodecResult EncodeCoopTacticalBaselineAck(
	const CoopTacticalBaselineAck& acknowledgement,
	CoopTacticalBaselineAckBytes& bytes) noexcept
{
	if (!IsValidCoopTacticalStateIdentity(acknowledgement.state) ||
		IsZero(acknowledgement.peerIdentity) ||
		acknowledgement.baselineId == 0)
		return CoopTacticalCodecResult::Invalid;
	CoopTacticalBaselineAckBytes encoded{};
	std::uint8_t* output = encoded.data();
	WriteCommonHeader(output, CoopTacticalWireMessageKind::BaselineAck,
		acknowledgement.state);
	std::copy(acknowledgement.peerIdentity.begin(),
		acknowledgement.peerIdentity.end(), output);
	output += acknowledgement.peerIdentity.size();
	WriteU64(output, acknowledgement.baselineId);
	WriteU32(output, acknowledgement.payloadChecksum);
	WriteU32(output, 0);
	WriteU64(output, acknowledgement.nextExpectedCommandId);
	bytes = encoded;
	return CoopTacticalCodecResult::Success;
}

CoopTacticalCodecResult DecodeCoopTacticalBaselineAck(
	const std::uint8_t* bytes,
	std::size_t size,
	CoopTacticalBaselineAck& acknowledgement) noexcept
{
	if (size != CoopTacticalBaselineAckWireSize)
		return CoopTacticalCodecResult::Invalid;
	CoopTacticalBaselineAck decoded;
	const std::uint8_t* input = nullptr;
	const CoopTacticalCodecResult header = ReadCommonHeader(
		bytes, size, CoopTacticalWireMessageKind::BaselineAck,
		decoded.state, input);
	if (header != CoopTacticalCodecResult::Success) return header;
	std::copy(input, input + decoded.peerIdentity.size(),
		decoded.peerIdentity.begin());
	input += decoded.peerIdentity.size();
	decoded.baselineId = ReadU64(input);
	decoded.payloadChecksum = ReadU32(input);
	if (ReadU32(input) != 0)
		return CoopTacticalCodecResult::Invalid;
	decoded.nextExpectedCommandId = ReadU64(input);
	if (IsZero(decoded.peerIdentity) ||
		decoded.baselineId == 0 ||
		input != bytes + CoopTacticalBaselineAckWireSize)
		return CoopTacticalCodecResult::Invalid;
	acknowledgement = decoded;
	return CoopTacticalCodecResult::Success;
}

CoopTacticalCodecResult EncodeCoopTacticalDelta(
	const CoopTacticalDelta& delta,
	std::vector<std::uint8_t>& bytes) noexcept
{
	if (!IsValidCoopTacticalStateIdentity(delta.state) ||
		delta.deltaId == 0 || delta.baseRevision == 0 ||
		delta.baseRevision >= delta.state.revision ||
		delta.delta.previousEpoch != delta.state.worldGeneration ||
		delta.delta.currentEpoch != delta.state.worldGeneration ||
		!IsCanonicalDelta(delta.delta, delta.state.turnSerial))
		return CoopTacticalCodecResult::Invalid;

	std::vector<std::uint8_t> payload;
	const CoopTacticalCodecResult payloadResult = MapDeltaEncodeResult(
		EncodeTacticalWorldDelta(delta.delta, payload,
			MaximumCoopTacticalDeltaEvents));
	if (payloadResult != CoopTacticalCodecResult::Success)
		return payloadResult;
	if (payload.empty() ||
		payload.size() > MaximumCoopTacticalDeltaPayloadWireSize ||
		payload.size() > std::numeric_limits<std::uint32_t>::max())
		return CoopTacticalCodecResult::PayloadTooLarge;
	const std::uint32_t checksum = CoopTacticalPayloadChecksum(
		payload.data(), payload.size());
	if (delta.payloadChecksum != 0 && delta.payloadChecksum != checksum)
		return CoopTacticalCodecResult::ChecksumMismatch;

	try
	{
		std::vector<std::uint8_t> encoded(
			CoopTacticalDeltaHeaderWireSize + payload.size());
		std::uint8_t* output = encoded.data();
		WriteCommonHeader(output,
			CoopTacticalWireMessageKind::Delta, delta.state);
		WriteU64(output, delta.deltaId);
		WriteU64(output, delta.baseRevision);
		WriteU32(output, static_cast<std::uint32_t>(payload.size()));
		WriteU32(output, checksum);
		std::copy(payload.begin(), payload.end(), output);
		bytes = std::move(encoded);
		return CoopTacticalCodecResult::Success;
	}
	catch (...)
	{
		return CoopTacticalCodecResult::AllocationFailure;
	}
}

CoopTacticalCodecResult DecodeCoopTacticalDelta(
	const std::uint8_t* bytes,
	std::size_t size,
	CoopTacticalDelta& delta) noexcept
{
	if (size < CoopTacticalDeltaHeaderWireSize ||
		size > MaximumCoopTacticalDeltaWireSize)
		return size > MaximumCoopTacticalDeltaWireSize
			? CoopTacticalCodecResult::PayloadTooLarge
			: CoopTacticalCodecResult::Invalid;
	CoopTacticalDelta decoded;
	const std::uint8_t* input = nullptr;
	const CoopTacticalCodecResult header = ReadCommonHeader(
		bytes, size, CoopTacticalWireMessageKind::Delta,
		decoded.state, input);
	if (header != CoopTacticalCodecResult::Success) return header;
	decoded.deltaId = ReadU64(input);
	decoded.baseRevision = ReadU64(input);
	const std::uint32_t payloadSize = ReadU32(input);
	decoded.payloadChecksum = ReadU32(input);
	if (decoded.deltaId == 0 || decoded.baseRevision == 0 ||
		decoded.baseRevision >= decoded.state.revision || payloadSize == 0 ||
		payloadSize > MaximumCoopTacticalDeltaPayloadWireSize ||
		static_cast<std::size_t>(bytes + size - input) != payloadSize)
		return payloadSize > MaximumCoopTacticalDeltaPayloadWireSize
			? CoopTacticalCodecResult::PayloadTooLarge
			: CoopTacticalCodecResult::Invalid;
	if (CoopTacticalPayloadChecksum(input, payloadSize) !=
		decoded.payloadChecksum)
		return CoopTacticalCodecResult::ChecksumMismatch;

	try
	{
		std::vector<std::uint8_t> payload(input, input + payloadSize);
		const CoopTacticalCodecResult payloadResult = MapDeltaDecodeResult(
			DecodeTacticalWorldDelta(payload, decoded.delta,
				MaximumCoopTacticalDeltaEvents));
		if (payloadResult != CoopTacticalCodecResult::Success)
			return payloadResult;
		if (decoded.delta.previousEpoch != decoded.state.worldGeneration ||
			decoded.delta.currentEpoch != decoded.state.worldGeneration ||
			!IsCanonicalDelta(decoded.delta, decoded.state.turnSerial))
			return CoopTacticalCodecResult::InvalidPayload;
		std::vector<std::uint8_t> canonical;
		const CoopTacticalCodecResult canonicalResult = MapDeltaEncodeResult(
			EncodeTacticalWorldDelta(decoded.delta, canonical,
				MaximumCoopTacticalDeltaEvents));
		if (canonicalResult != CoopTacticalCodecResult::Success)
			return canonicalResult;
		if (canonical != payload)
			return CoopTacticalCodecResult::InvalidPayload;
		delta = std::move(decoded);
		return CoopTacticalCodecResult::Success;
	}
	catch (...)
	{
		return CoopTacticalCodecResult::AllocationFailure;
	}
}

CoopTacticalCodecResult EncodeCoopTacticalDeltaAck(
	const CoopTacticalDeltaAck& acknowledgement,
	CoopTacticalDeltaAckBytes& bytes) noexcept
{
	if (!IsValidCoopTacticalStateIdentity(acknowledgement.state) ||
		IsZero(acknowledgement.peerIdentity) ||
		acknowledgement.deltaId == 0)
		return CoopTacticalCodecResult::Invalid;
	CoopTacticalDeltaAckBytes encoded{};
	std::uint8_t* output = encoded.data();
	WriteCommonHeader(output, CoopTacticalWireMessageKind::DeltaAck,
		acknowledgement.state);
	std::copy(acknowledgement.peerIdentity.begin(),
		acknowledgement.peerIdentity.end(), output);
	output += acknowledgement.peerIdentity.size();
	WriteU64(output, acknowledgement.deltaId);
	WriteU32(output, acknowledgement.payloadChecksum);
	WriteU32(output, 0);
	bytes = encoded;
	return CoopTacticalCodecResult::Success;
}

CoopTacticalCodecResult DecodeCoopTacticalDeltaAck(
	const std::uint8_t* bytes,
	std::size_t size,
	CoopTacticalDeltaAck& acknowledgement) noexcept
{
	if (size != CoopTacticalDeltaAckWireSize)
		return CoopTacticalCodecResult::Invalid;
	CoopTacticalDeltaAck decoded;
	const std::uint8_t* input = nullptr;
	const CoopTacticalCodecResult header = ReadCommonHeader(
		bytes, size, CoopTacticalWireMessageKind::DeltaAck,
		decoded.state, input);
	if (header != CoopTacticalCodecResult::Success) return header;
	std::copy(input, input + decoded.peerIdentity.size(),
		decoded.peerIdentity.begin());
	input += decoded.peerIdentity.size();
	decoded.deltaId = ReadU64(input);
	decoded.payloadChecksum = ReadU32(input);
	if (ReadU32(input) != 0 || IsZero(decoded.peerIdentity) ||
		decoded.deltaId == 0 ||
		input != bytes + CoopTacticalDeltaAckWireSize)
		return CoopTacticalCodecResult::Invalid;
	acknowledgement = decoded;
	return CoopTacticalCodecResult::Success;
}

CoopTacticalCodecResult EncodeCoopTacticalResyncRequest(
	const CoopTacticalResyncRequest& request,
	CoopTacticalResyncRequestBytes& bytes) noexcept
{
	if (!IsValidCoopTacticalStateIdentity(request.acceptedState) ||
		request.requestId == 0 || request.acceptedBaselineId == 0 ||
		!IsKnownCoopTacticalResyncReason(request.reason))
		return CoopTacticalCodecResult::Invalid;
	CoopTacticalResyncRequestBytes encoded{};
	std::uint8_t* output = encoded.data();
	WriteCommonHeader(output, CoopTacticalWireMessageKind::ResyncRequest,
		request.acceptedState);
	WriteU64(output, request.requestId);
	WriteU64(output, request.acceptedBaselineId);
	WriteU64(output, request.lastAppliedDeltaId);
	WriteU32(output, request.lastPayloadChecksum);
	*output++ = static_cast<std::uint8_t>(request.reason);
	for (unsigned index = 0; index < 3; ++index) *output++ = 0;
	WriteU64(output, request.nextExpectedCommandId);
	bytes = encoded;
	return CoopTacticalCodecResult::Success;
}

CoopTacticalCodecResult DecodeCoopTacticalResyncRequest(
	const std::uint8_t* bytes, std::size_t size,
	CoopTacticalResyncRequest& request) noexcept
{
	if (size != CoopTacticalResyncRequestWireSize)
		return CoopTacticalCodecResult::Invalid;
	CoopTacticalResyncRequest decoded;
	const std::uint8_t* input = nullptr;
	const CoopTacticalCodecResult header = ReadCommonHeader(
		bytes, size, CoopTacticalWireMessageKind::ResyncRequest,
		decoded.acceptedState, input);
	if (header != CoopTacticalCodecResult::Success) return header;
	decoded.requestId = ReadU64(input);
	decoded.acceptedBaselineId = ReadU64(input);
	decoded.lastAppliedDeltaId = ReadU64(input);
	decoded.lastPayloadChecksum = ReadU32(input);
	decoded.reason = static_cast<CoopTacticalResyncReason>(*input++);
	for (unsigned index = 0; index < 3; ++index)
		if (*input++ != 0) return CoopTacticalCodecResult::Invalid;
	decoded.nextExpectedCommandId = ReadU64(input);
	if (decoded.requestId == 0 || decoded.acceptedBaselineId == 0 ||
		!IsKnownCoopTacticalResyncReason(decoded.reason) ||
		input != bytes + CoopTacticalResyncRequestWireSize)
		return CoopTacticalCodecResult::Invalid;
	request = decoded;
	return CoopTacticalCodecResult::Success;
}
}
