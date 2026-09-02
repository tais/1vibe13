#include "CoopCampaignBootstrapProtocol.h"

#include <algorithm>

namespace CoopSession
{
namespace
{
constexpr std::uint8_t CampaignBootstrapMagic[4] = {'J', '2', 'C', 'B'};
constexpr std::uint8_t CampaignBootstrapMessageKind = 1;
constexpr std::size_t DescriptorChecksumOffset = 112;
constexpr std::size_t TrailingReservedOffset = 116;
constexpr std::uint32_t Fnv1aOffsetBasis = UINT32_C(2166136261);
constexpr std::uint32_t Fnv1aPrime = UINT32_C(16777619);

static_assert(DescriptorChecksumOffset + sizeof(std::uint32_t) ==
	TrailingReservedOffset,
	"campaign bootstrap checksum layout changed");
static_assert(TrailingReservedOffset + 12 == CoopCampaignBootstrapWireSize,
	"campaign bootstrap reserved layout changed");

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

template<typename Hash>
bool IsZeroHash(const Hash& hash) noexcept
{
	for (const std::uint8_t byte : hash)
		if (byte != 0) return false;
	return true;
}

std::uint32_t DescriptorChecksum(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (bytes == nullptr && size != 0) return 0;
	std::uint32_t checksum = Fnv1aOffsetBasis;
	for (std::size_t index = 0; index < size; ++index)
	{
		checksum ^= bytes[index];
		checksum *= Fnv1aPrime;
	}
	return checksum;
}

bool HasCanonicalReservedBytes(
	const CoopCampaignBootstrapBytes& bytes) noexcept
{
	if (bytes[7] != 0 || bytes[10] != 0 || bytes[11] != 0)
		return false;
	for (std::size_t index = TrailingReservedOffset;
		index < bytes.size(); ++index)
	{
		if (bytes[index] != 0) return false;
	}
	return true;
}
}

bool IsValidCoopCampaignBootstrapDescriptor(
	const CoopCampaignBootstrapDescriptor& descriptor) noexcept
{
	return descriptor.protocolVersion == CurrentProtocolVersion &&
		descriptor.sessionEpoch != 0 &&
		!IsZeroHash(descriptor.campaignIdentitySha256) &&
		descriptor.runtimeFingerprint.schema != 0 &&
		!IsZeroHash(descriptor.contentManifestSha256);
}

bool SameCoopCampaignBootstrapDescriptor(
	const CoopCampaignBootstrapDescriptor& left,
	const CoopCampaignBootstrapDescriptor& right) noexcept
{
	return left.protocolVersion == right.protocolVersion &&
		left.sessionEpoch == right.sessionEpoch &&
		left.campaignSeed == right.campaignSeed &&
		left.campaignIdentitySha256 == right.campaignIdentitySha256 &&
		left.runtimeFingerprint == right.runtimeFingerprint &&
		left.contentManifestSha256 == right.contentManifestSha256;
}

bool EncodeCoopCampaignBootstrap(
	const CoopCampaignBootstrapDescriptor& descriptor,
	CoopCampaignBootstrapBytes& bytes) noexcept
{
	if (!IsValidCoopCampaignBootstrapDescriptor(descriptor)) return false;

	CoopCampaignBootstrapBytes encoded{};
	std::uint8_t* output = encoded.data();
	std::copy(CampaignBootstrapMagic, CampaignBootstrapMagic + 4, output);
	output += 4;
	WriteU16(output, CoopCampaignBootstrapWireVersion);
	*output++ = CampaignBootstrapMessageKind;
	*output++ = 0;
	WriteU16(output, descriptor.protocolVersion);
	WriteU16(output, 0);
	WriteU64(output, descriptor.sessionEpoch);
	WriteU64(output, descriptor.campaignSeed);
	std::copy(descriptor.campaignIdentitySha256.begin(),
		descriptor.campaignIdentitySha256.end(), output);
	output += descriptor.campaignIdentitySha256.size();
	WriteU32(output, descriptor.runtimeFingerprint.schema);
	WriteU64(output, descriptor.runtimeFingerprint.high);
	WriteU64(output, descriptor.runtimeFingerprint.low);
	std::copy(descriptor.contentManifestSha256.begin(),
		descriptor.contentManifestSha256.end(), output);
	output += descriptor.contentManifestSha256.size();
	WriteU32(output, DescriptorChecksum(
		encoded.data(), DescriptorChecksumOffset));
	bytes = encoded;
	return true;
}

CoopCampaignBootstrapDecodeResult DecodeCoopCampaignBootstrap(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignBootstrapDescriptor& descriptor) noexcept
{
	if (bytes == nullptr || size != CoopCampaignBootstrapWireSize)
		return CoopCampaignBootstrapDecodeResult::WrongSize;
	if (!std::equal(CampaignBootstrapMagic,
		CampaignBootstrapMagic + 4, bytes))
		return CoopCampaignBootstrapDecodeResult::WrongMagic;

	CoopCampaignBootstrapBytes canonical{};
	std::copy(bytes, bytes + size, canonical.begin());
	const std::uint8_t* input = canonical.data() + 4;
	if (ReadU16(input) != CoopCampaignBootstrapWireVersion)
		return CoopCampaignBootstrapDecodeResult::UnsupportedWireVersion;
	if (*input++ != CampaignBootstrapMessageKind)
		return CoopCampaignBootstrapDecodeResult::WrongMessageKind;
	if (!HasCanonicalReservedBytes(canonical))
		return CoopCampaignBootstrapDecodeResult::NonZeroReserved;
	++input;

	const std::uint32_t expectedChecksum = DescriptorChecksum(
		canonical.data(), DescriptorChecksumOffset);
	const std::uint8_t* checksumInput =
		canonical.data() + DescriptorChecksumOffset;
	if (ReadU32(checksumInput) != expectedChecksum)
		return CoopCampaignBootstrapDecodeResult::ChecksumMismatch;

	CoopCampaignBootstrapDescriptor decoded;
	decoded.protocolVersion = ReadU16(input);
	(void)ReadU16(input);
	decoded.sessionEpoch = ReadU64(input);
	decoded.campaignSeed = ReadU64(input);
	std::copy(input, input + decoded.campaignIdentitySha256.size(),
		decoded.campaignIdentitySha256.begin());
	input += decoded.campaignIdentitySha256.size();
	decoded.runtimeFingerprint.schema = ReadU32(input);
	decoded.runtimeFingerprint.high = ReadU64(input);
	decoded.runtimeFingerprint.low = ReadU64(input);
	std::copy(input, input + decoded.contentManifestSha256.size(),
		decoded.contentManifestSha256.begin());

	if (decoded.protocolVersion != CurrentProtocolVersion)
		return CoopCampaignBootstrapDecodeResult::UnsupportedProtocol;
	if (!IsValidCoopCampaignBootstrapDescriptor(decoded))
		return CoopCampaignBootstrapDecodeResult::InvalidSemanticValue;
	descriptor = decoded;
	return CoopCampaignBootstrapDecodeResult::Success;
}
}
