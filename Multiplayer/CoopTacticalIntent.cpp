#include "CoopTacticalIntent.h"

#include <algorithm>
#include <type_traits>
#include <utility>

namespace CoopSession
{
namespace
{
constexpr std::uint8_t TacticalIntentMagic[4] = {'J', '2', 'C', 'I'};

void WriteU16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
	bytes.push_back(static_cast<std::uint8_t>(value));
	bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void WriteU32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

void WriteU64(std::vector<std::uint8_t>& bytes, std::uint64_t value)
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

void WriteI32(std::vector<std::uint8_t>& bytes, std::int32_t value)
{
	WriteU32(bytes, static_cast<std::uint32_t>(value));
}

bool ReadU16(
	const std::uint8_t*& input,
	const std::uint8_t* end,
	std::uint16_t& value) noexcept
{
	if (static_cast<std::size_t>(end - input) < 2) return false;
	value = static_cast<std::uint16_t>(input[0]) |
		(static_cast<std::uint16_t>(input[1]) << 8);
	input += 2;
	return true;
}

bool ReadU32(
	const std::uint8_t*& input,
	const std::uint8_t* end,
	std::uint32_t& value) noexcept
{
	if (static_cast<std::size_t>(end - input) < 4) return false;
	value = 0;
	for (unsigned shift = 0; shift < 32; shift += 8)
		value |= static_cast<std::uint32_t>(*input++) << shift;
	return true;
}

bool ReadU64(
	const std::uint8_t*& input,
	const std::uint8_t* end,
	std::uint64_t& value) noexcept
{
	if (static_cast<std::size_t>(end - input) < 8) return false;
	value = 0;
	for (unsigned shift = 0; shift < 64; shift += 8)
		value |= static_cast<std::uint64_t>(*input++) << shift;
	return true;
}

bool ReadI32(
	const std::uint8_t*& input,
	const std::uint8_t* end,
	std::int32_t& value) noexcept
{
	std::uint32_t encoded = 0;
	if (!ReadU32(input, end, encoded)) return false;
	value = encoded <= 0x7fffffffu
		? static_cast<std::int32_t>(encoded)
		: static_cast<std::int32_t>(-1 -
			static_cast<std::int64_t>(0xffffffffu - encoded));
	return true;
}

std::size_t PayloadSize(TacticalIntentKind kind) noexcept
{
	switch (kind)
	{
		case TacticalIntentKind::Move: return 7;
		case TacticalIntentKind::Face: return 1;
		case TacticalIntentKind::Stance: return 1;
		case TacticalIntentKind::AimedFirearmAttack: return 7;
		case TacticalIntentKind::DoorOpenClose: return 7;
		case TacticalIntentKind::PassInterrupt: return 8;
		case TacticalIntentKind::Stop:
		case TacticalIntentKind::EndTurn:
		case TacticalIntentKind::Reload: return 0;
	}
	return MaximumTacticalIntentPayloadWireSize + 1;
}
}

bool IsKnownTacticalIntentKind(TacticalIntentKind kind) noexcept
{
	switch (kind)
	{
		case TacticalIntentKind::Move:
		case TacticalIntentKind::Face:
		case TacticalIntentKind::Stance:
		case TacticalIntentKind::Stop:
		case TacticalIntentKind::EndTurn:
		case TacticalIntentKind::AimedFirearmAttack:
		case TacticalIntentKind::Reload:
		case TacticalIntentKind::DoorOpenClose:
		case TacticalIntentKind::PassInterrupt:
			return true;
	}
	return false;
}

bool IsKnownTacticalIntentStance(TacticalIntentStance stance) noexcept
{
	switch (stance)
	{
		case TacticalIntentStance::Standing:
		case TacticalIntentStance::Crouched:
		case TacticalIntentStance::Prone:
			return true;
	}
	return false;
}

TacticalIntentKind KindOf(const TacticalIntentPayload& payload) noexcept
{
	if (payload.valueless_by_exception())
		return static_cast<TacticalIntentKind>(0);
	return std::visit([](const auto& value) noexcept {
		using Payload = typename std::decay<decltype(value)>::type;
		if constexpr (std::is_same<Payload, MoveTacticalIntent>::value)
			return TacticalIntentKind::Move;
		if constexpr (std::is_same<Payload, FaceTacticalIntent>::value)
			return TacticalIntentKind::Face;
		if constexpr (std::is_same<Payload, StanceTacticalIntent>::value)
			return TacticalIntentKind::Stance;
		if constexpr (std::is_same<Payload, StopTacticalIntent>::value)
			return TacticalIntentKind::Stop;
		if constexpr (std::is_same<Payload, EndTurnTacticalIntent>::value)
			return TacticalIntentKind::EndTurn;
		if constexpr (std::is_same<Payload,
			AimedFirearmAttackTacticalIntent>::value)
			return TacticalIntentKind::AimedFirearmAttack;
		if constexpr (std::is_same<Payload, ReloadTacticalIntent>::value)
			return TacticalIntentKind::Reload;
		if constexpr (std::is_same<Payload,
			DoorOpenCloseTacticalIntent>::value)
			return TacticalIntentKind::DoorOpenClose;
		return TacticalIntentKind::PassInterrupt;
	}, payload);
}

bool IsStructurallyValidTacticalIntent(const TacticalIntent& intent) noexcept
{
	if (intent.protocolVersion != TacticalIntentWireVersion ||
		intent.sessionEpoch == 0 || IsZero(intent.claimedPeerIdentity) ||
		intent.commandId == 0 || intent.worldGeneration == 0 ||
		intent.baseRevision == 0 || intent.turnSerial == 0 ||
		!intent.actor.valid() || intent.payload.valueless_by_exception())
		return false;

	return std::visit([](const auto& payload) noexcept {
		using Payload = typename std::decay<decltype(payload)>::type;
		if constexpr (std::is_same<Payload, MoveTacticalIntent>::value)
			return payload.destinationGrid >= 0;
		if constexpr (std::is_same<Payload, FaceTacticalIntent>::value)
			return payload.direction < 8;
		if constexpr (std::is_same<Payload, StanceTacticalIntent>::value)
			return IsKnownTacticalIntentStance(payload.stance);
		if constexpr (std::is_same<Payload,
			AimedFirearmAttackTacticalIntent>::value)
			return payload.target.valid() &&
				payload.aimTime <= MaximumTacticalFirearmAimTime;
		if constexpr (std::is_same<Payload,
			DoorOpenCloseTacticalIntent>::value)
			return payload.baseGrid >= 0 && payload.structureId != 0;
		if constexpr (std::is_same<Payload,
			PassInterruptTacticalIntent>::value)
			return payload.interruptSerial != 0;
		return true;
	}, intent.payload);
}

TacticalIntentCodecResult EncodeTacticalIntent(
	const TacticalIntent& intent,
	std::vector<std::uint8_t>& bytes) noexcept
{
	if (!IsStructurallyValidTacticalIntent(intent))
		return TacticalIntentCodecResult::Invalid;
	const TacticalIntentKind kind = KindOf(intent.payload);
	const std::size_t payloadSize = PayloadSize(kind);
	if (!IsKnownTacticalIntentKind(kind) ||
		payloadSize > MaximumTacticalIntentPayloadWireSize)
		return TacticalIntentCodecResult::Invalid;

	try
	{
		std::vector<std::uint8_t> encoded;
		encoded.reserve(TacticalIntentHeaderWireSize + payloadSize);
		encoded.insert(encoded.end(), TacticalIntentMagic,
			TacticalIntentMagic + 4);
		WriteU16(encoded, intent.protocolVersion);
		encoded.push_back(static_cast<std::uint8_t>(kind));
		encoded.push_back(0);
		WriteU64(encoded, intent.sessionEpoch);
		encoded.insert(encoded.end(), intent.claimedPeerIdentity.begin(),
			intent.claimedPeerIdentity.end());
		WriteU64(encoded, intent.commandId);
		WriteU64(encoded, intent.worldGeneration);
		WriteU64(encoded, intent.baseRevision);
		WriteU64(encoded, intent.turnSerial);
		WriteU16(encoded, intent.actor.slot);
		WriteU32(encoded, intent.actor.incarnation);
		WriteU16(encoded, static_cast<std::uint16_t>(payloadSize));

		std::visit([&encoded](const auto& payload) {
			using Payload = typename std::decay<decltype(payload)>::type;
			if constexpr (std::is_same<Payload, MoveTacticalIntent>::value)
			{
				WriteI32(encoded, payload.destinationGrid);
				WriteU16(encoded, payload.movementMode);
				encoded.push_back(payload.reverse ? 1u : 0u);
			}
			else if constexpr (std::is_same<Payload, FaceTacticalIntent>::value)
				encoded.push_back(payload.direction);
			else if constexpr (std::is_same<Payload, StanceTacticalIntent>::value)
				encoded.push_back(static_cast<std::uint8_t>(payload.stance));
			else if constexpr (std::is_same<Payload,
				AimedFirearmAttackTacticalIntent>::value)
			{
				WriteU16(encoded, payload.target.slot);
				WriteU32(encoded, payload.target.incarnation);
				encoded.push_back(payload.aimTime);
			}
			else if constexpr (std::is_same<Payload,
				DoorOpenCloseTacticalIntent>::value)
			{
				WriteI32(encoded, payload.baseGrid);
				WriteU16(encoded, payload.structureId);
				encoded.push_back(payload.desiredOpen ? 1u : 0u);
			}
			else if constexpr (std::is_same<Payload,
				PassInterruptTacticalIntent>::value)
				WriteU64(encoded, payload.interruptSerial);
		}, intent.payload);

		if (encoded.size() != TacticalIntentHeaderWireSize + payloadSize)
			return TacticalIntentCodecResult::Invalid;
		bytes = std::move(encoded);
		return TacticalIntentCodecResult::Success;
	}
	catch (...)
	{
		return TacticalIntentCodecResult::AllocationFailure;
	}
}

TacticalIntentCodecResult DecodeTacticalIntent(
	const std::uint8_t* bytes,
	std::size_t size,
	TacticalIntent& intent) noexcept
{
	if (bytes == nullptr || size < TacticalIntentHeaderWireSize ||
		size > MaximumTacticalIntentWireSize)
		return TacticalIntentCodecResult::Invalid;
	if (!std::equal(TacticalIntentMagic, TacticalIntentMagic + 4, bytes))
		return TacticalIntentCodecResult::Invalid;

	const std::uint8_t* input = bytes + 4;
	const std::uint8_t* end = bytes + size;
	TacticalIntent decoded;
	if (!ReadU16(input, end, decoded.protocolVersion))
		return TacticalIntentCodecResult::Invalid;
	if (decoded.protocolVersion != TacticalIntentWireVersion)
		return TacticalIntentCodecResult::UnsupportedVersion;
	if (end - input < 2) return TacticalIntentCodecResult::Invalid;
	const TacticalIntentKind kind =
		static_cast<TacticalIntentKind>(*input++);
	if (!IsKnownTacticalIntentKind(kind) || *input++ != 0)
		return TacticalIntentCodecResult::Invalid;
	if (!ReadU64(input, end, decoded.sessionEpoch) || end - input < 16)
		return TacticalIntentCodecResult::Invalid;
	std::copy(input, input + 16, decoded.claimedPeerIdentity.begin());
	input += 16;
	std::uint16_t payloadSize = 0;
	if (!ReadU64(input, end, decoded.commandId) ||
		!ReadU64(input, end, decoded.worldGeneration) ||
		!ReadU64(input, end, decoded.baseRevision) ||
		!ReadU64(input, end, decoded.turnSerial) ||
		!ReadU16(input, end, decoded.actor.slot) ||
		!ReadU32(input, end, decoded.actor.incarnation) ||
		!ReadU16(input, end, payloadSize) ||
		payloadSize != PayloadSize(kind) ||
		static_cast<std::size_t>(end - input) != payloadSize)
		return TacticalIntentCodecResult::Invalid;

	switch (kind)
	{
		case TacticalIntentKind::Move:
		{
			MoveTacticalIntent payload;
			std::uint8_t reverse = 0;
			if (!ReadI32(input, end, payload.destinationGrid) ||
				!ReadU16(input, end, payload.movementMode) || input == end)
				return TacticalIntentCodecResult::Invalid;
			reverse = *input++;
			if (reverse > 1) return TacticalIntentCodecResult::Invalid;
			payload.reverse = reverse != 0;
			decoded.payload = payload;
			break;
		}
		case TacticalIntentKind::Face:
			decoded.payload = FaceTacticalIntent{*input++};
			break;
		case TacticalIntentKind::Stance:
			decoded.payload = StanceTacticalIntent{
				static_cast<TacticalIntentStance>(*input++)};
			break;
		case TacticalIntentKind::Stop:
			decoded.payload = StopTacticalIntent{};
			break;
		case TacticalIntentKind::EndTurn:
			decoded.payload = EndTurnTacticalIntent{};
			break;
		case TacticalIntentKind::Reload:
			decoded.payload = ReloadTacticalIntent{};
			break;
		case TacticalIntentKind::AimedFirearmAttack:
		{
			AimedFirearmAttackTacticalIntent payload;
			if (!ReadU16(input, end, payload.target.slot) ||
				!ReadU32(input, end, payload.target.incarnation) ||
				input == end)
				return TacticalIntentCodecResult::Invalid;
			payload.aimTime = *input++;
			decoded.payload = payload;
			break;
		}
		case TacticalIntentKind::DoorOpenClose:
		{
			DoorOpenCloseTacticalIntent payload;
			if (!ReadI32(input, end, payload.baseGrid) ||
				!ReadU16(input, end, payload.structureId) || input == end)
				return TacticalIntentCodecResult::Invalid;
			const std::uint8_t desiredOpen = *input++;
			if (desiredOpen > 1) return TacticalIntentCodecResult::Invalid;
			payload.desiredOpen = desiredOpen != 0;
			decoded.payload = payload;
			break;
		}
		case TacticalIntentKind::PassInterrupt:
		{
			PassInterruptTacticalIntent payload;
			if (!ReadU64(input, end, payload.interruptSerial))
				return TacticalIntentCodecResult::Invalid;
			decoded.payload = payload;
			break;
		}
	}

	if (input != end || !IsStructurallyValidTacticalIntent(decoded))
		return TacticalIntentCodecResult::Invalid;
	intent = std::move(decoded);
	return TacticalIntentCodecResult::Success;
}
}
