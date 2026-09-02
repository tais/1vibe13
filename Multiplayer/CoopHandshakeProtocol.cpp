#include "CoopHandshakeProtocol.h"

#include <algorithm>

namespace CoopSession
{
namespace
{
constexpr std::uint8_t ServerHelloMagic[4] = {'J', '2', 'C', 'H'};
constexpr std::uint8_t ServerHelloMessageKind = 1;

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

bool ValidHello(const CoopServerHello& hello) noexcept
{
	return hello.protocolVersion != 0 && hello.sessionEpoch != 0 &&
		hello.runtimeFingerprint.schema != 0 &&
		!IsZero(hello.contentManifestSha256);
}
}

bool EncodeCoopServerHello(
	const CoopServerHello& hello, CoopServerHelloBytes& bytes) noexcept
{
	if (!ValidHello(hello)) return false;
	CoopServerHelloBytes encoded{};
	std::uint8_t* output = encoded.data();
	std::copy(ServerHelloMagic, ServerHelloMagic + 4, output);
	output += 4;
	WriteU16(output, CoopServerHelloWireVersion);
	*output++ = ServerHelloMessageKind;
	*output++ = 0;
	WriteU16(output, hello.protocolVersion);
	WriteU16(output, 0);
	WriteU64(output, hello.sessionEpoch);
	WriteU32(output, hello.runtimeFingerprint.schema);
	WriteU64(output, hello.runtimeFingerprint.high);
	WriteU64(output, hello.runtimeFingerprint.low);
	std::copy(hello.contentManifestSha256.begin(),
		hello.contentManifestSha256.end(), output);
	bytes = encoded;
	return true;
}

CoopServerHelloDecodeResult DecodeCoopServerHello(
	const std::uint8_t* bytes, std::size_t size,
	CoopServerHello& hello) noexcept
{
	if (bytes == nullptr || size != CoopServerHelloWireSize)
		return CoopServerHelloDecodeResult::WrongSize;
	if (!std::equal(ServerHelloMagic, ServerHelloMagic + 4, bytes))
		return CoopServerHelloDecodeResult::WrongMagic;
	const std::uint8_t* input = bytes + 4;
	if (ReadU16(input) != CoopServerHelloWireVersion)
		return CoopServerHelloDecodeResult::UnsupportedWireVersion;
	if (*input++ != ServerHelloMessageKind)
		return CoopServerHelloDecodeResult::WrongMessageKind;
	if (*input++ != 0)
		return CoopServerHelloDecodeResult::NonZeroReserved;

	CoopServerHello decoded;
	decoded.protocolVersion = ReadU16(input);
	if (ReadU16(input) != 0)
		return CoopServerHelloDecodeResult::NonZeroReserved;
	decoded.sessionEpoch = ReadU64(input);
	decoded.runtimeFingerprint.schema = ReadU32(input);
	decoded.runtimeFingerprint.high = ReadU64(input);
	decoded.runtimeFingerprint.low = ReadU64(input);
	std::copy(input, input + decoded.contentManifestSha256.size(),
		decoded.contentManifestSha256.begin());
	if (!ValidHello(decoded))
		return CoopServerHelloDecodeResult::InvalidSemanticValue;
	hello = decoded;
	return CoopServerHelloDecodeResult::Success;
}
}
