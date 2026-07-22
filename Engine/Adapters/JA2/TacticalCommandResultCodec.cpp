#include <Engine/Adapters/JA2/TacticalCommandResultCodec.h>

#include <utility>

#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/Identifier.h>

namespace
{
constexpr std::uint32_t TacticalCommandResultMagic = 0x31524354u; // "TCR1"

bool IsValidStatus(TacticalCommandTerminalStatus status) noexcept
{
	switch (status)
	{
		case TacticalCommandTerminalStatus::Rejected:
		case TacticalCommandTerminalStatus::Applied:
		case TacticalCommandTerminalStatus::Discarded:
		case TacticalCommandTerminalStatus::Cancelled:
			return true;
	}
	return false;
}

bool IsValidReason(TacticalCommandTerminalReason reason) noexcept
{
	switch (reason)
	{
		case TacticalCommandTerminalReason::None:
		case TacticalCommandTerminalReason::InactiveOwner:
		case TacticalCommandTerminalReason::InvalidDomain:
		case TacticalCommandTerminalReason::UnavailableContext:
		case TacticalCommandTerminalReason::SequenceExhausted:
		case TacticalCommandTerminalReason::PackageTeardown:
		case TacticalCommandTerminalReason::AuthoritativeDiscard:
			return true;
	}
	return false;
}

bool IsValidResult(const TacticalCommandResult& result) noexcept
{
	if (result.packageId.size() > MaximumTacticalCommandResultOwnerBytes ||
		!IsValidEngineIdentifier(result.packageId) || result.requestId == 0 ||
		!IsValidStatus(result.status) || !IsValidReason(result.reason)) return false;
	switch (result.status)
	{
		case TacticalCommandTerminalStatus::Rejected:
			return result.authoritativeSequence == 0 &&
				result.reason != TacticalCommandTerminalReason::None;
		case TacticalCommandTerminalStatus::Applied:
			return result.authoritativeSequence != 0 &&
				result.reason == TacticalCommandTerminalReason::None;
		case TacticalCommandTerminalStatus::Discarded:
			return result.authoritativeSequence != 0 &&
				result.reason == TacticalCommandTerminalReason::AuthoritativeDiscard;
		case TacticalCommandTerminalStatus::Cancelled:
			return result.reason == TacticalCommandTerminalReason::PackageTeardown;
	}
	return false;
}
}

TacticalCommandResultEncodeError EncodeTacticalCommandResult(
	const TacticalCommandResult& result,
	std::vector<std::uint8_t>& bytes) noexcept
{
	if (!IsValidResult(result)) return TacticalCommandResultEncodeError::Invalid;
	try
	{
		BinaryWriter writer;
		WritePersistenceHeader(writer, PersistenceHeader{
			TacticalCommandResultMagic, TacticalCommandResultWireVersion});
		writer.writeString(result.packageId);
		writer.writeU64(result.requestId);
		writer.writeU64(result.authoritativeSequence);
		writer.writeU64(result.simulationTick);
		writer.writeU8(static_cast<std::uint8_t>(result.status));
		writer.writeU8(static_cast<std::uint8_t>(result.reason));
		std::vector<std::uint8_t> encoded = writer.take();
		bytes = std::move(encoded);
		return TacticalCommandResultEncodeError::None;
	}
	catch (...)
	{
		return TacticalCommandResultEncodeError::AllocationFailure;
	}
}

TacticalCommandResultDecodeError DecodeTacticalCommandResult(
	const std::vector<std::uint8_t>& bytes,
	TacticalCommandResult& result) noexcept
{
	try
	{
		BinaryReader reader(bytes);
		PersistenceHeader header{};
		if (!reader.readU32(header.magic) || !reader.readU16(header.version) ||
			header.magic != TacticalCommandResultMagic)
			return TacticalCommandResultDecodeError::Invalid;
		if (header.version != TacticalCommandResultWireVersion)
			return TacticalCommandResultDecodeError::UnsupportedVersion;

		TacticalCommandResult decoded;
		std::uint8_t status = 0;
		std::uint8_t reason = 0;
		if (!reader.readStringBounded(
				decoded.packageId, MaximumTacticalCommandResultOwnerBytes) ||
			!reader.readU64(decoded.requestId) ||
			!reader.readU64(decoded.authoritativeSequence) ||
			!reader.readU64(decoded.simulationTick) ||
			!reader.readU8(status) || !reader.readU8(reason) ||
			reader.remaining() != 0)
			return TacticalCommandResultDecodeError::Invalid;
		decoded.status = static_cast<TacticalCommandTerminalStatus>(status);
		decoded.reason = static_cast<TacticalCommandTerminalReason>(reason);
		if (!IsValidResult(decoded)) return TacticalCommandResultDecodeError::Invalid;
		result = std::move(decoded);
		return TacticalCommandResultDecodeError::None;
	}
	catch (...)
	{
		return TacticalCommandResultDecodeError::AllocationFailure;
	}
}
