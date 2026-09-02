#include "CoopCampaignBootstrapProtocol.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

using namespace CoopSession;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); \
		++failures; \
	} \
} while (false)

static_assert(CoopCampaignBootstrapWireSize == 128,
	"campaign bootstrap wire size is frozen");
static_assert(noexcept(IsValidCoopCampaignBootstrapDescriptor(
	std::declval<const CoopCampaignBootstrapDescriptor&>())));
static_assert(noexcept(SameCoopCampaignBootstrapDescriptor(
	std::declval<const CoopCampaignBootstrapDescriptor&>(),
	std::declval<const CoopCampaignBootstrapDescriptor&>())));
static_assert(noexcept(EncodeCoopCampaignBootstrap(
	std::declval<const CoopCampaignBootstrapDescriptor&>(),
	std::declval<CoopCampaignBootstrapBytes&>())));
static_assert(noexcept(DecodeCoopCampaignBootstrap(nullptr, 0,
	std::declval<CoopCampaignBootstrapDescriptor&>())));

constexpr std::size_t ProtocolOffset = 8;
constexpr std::size_t SessionEpochOffset = 12;
constexpr std::size_t CampaignSeedOffset = 20;
constexpr std::size_t CampaignIdentityOffset = 28;
constexpr std::size_t RuntimeSchemaOffset = 60;
constexpr std::size_t RuntimeHighOffset = 64;
constexpr std::size_t RuntimeLowOffset = 72;
constexpr std::size_t ContentManifestOffset = 80;
constexpr std::size_t ChecksumOffset = 112;

void WriteU16(CoopCampaignBootstrapBytes& bytes, std::size_t offset,
	std::uint16_t value)
{
	bytes[offset] = static_cast<std::uint8_t>(value);
	bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void WriteU32(CoopCampaignBootstrapBytes& bytes, std::size_t offset,
	std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

std::uint32_t DescriptorChecksum(
	const CoopCampaignBootstrapBytes& bytes)
{
	std::uint32_t checksum = UINT32_C(2166136261);
	for (std::size_t index = 0; index < ChecksumOffset; ++index)
	{
		checksum ^= bytes[index];
		checksum *= UINT32_C(16777619);
	}
	return checksum;
}

void Rechecksum(CoopCampaignBootstrapBytes& bytes)
{
	WriteU32(bytes, ChecksumOffset, DescriptorChecksum(bytes));
}

CoopCampaignBootstrapDescriptor Fixture()
{
	CoopCampaignBootstrapDescriptor descriptor;
	descriptor.sessionEpoch = UINT64_C(0x0102030405060708);
	descriptor.campaignSeed = UINT64_C(0x3837363534333231);
	const bool identityReady = ComputeCoopCampaignIdentitySha256(
		"shared_01", descriptor.campaignSeed,
		descriptor.campaignIdentitySha256);
	CHECK(identityReady, "fixture campaign identity hashes");
	descriptor.runtimeFingerprint = {
		UINT32_C(0x0a0b0c0d), UINT64_C(0x1122334455667788),
		UINT64_C(0x99aabbccddeeff00)};
	for (std::size_t index = 0;
		index < descriptor.contentManifestSha256.size(); ++index)
	{
		descriptor.contentManifestSha256[index] =
			static_cast<std::uint8_t>(0xa0 + index);
	}
	return descriptor;
}

CoopCampaignBootstrapDescriptor Sentinel()
{
	CoopCampaignBootstrapDescriptor descriptor = Fixture();
	descriptor.sessionEpoch = 77;
	descriptor.campaignSeed = 88;
	CHECK(ComputeCoopCampaignIdentitySha256("sentinel", 88,
		descriptor.campaignIdentitySha256),
		"sentinel campaign identity hashes");
	descriptor.runtimeFingerprint = {3, 4, 5};
	descriptor.contentManifestSha256.fill(0x5a);
	return descriptor;
}

void ExpectDecodeFailure(const CoopCampaignBootstrapBytes& bytes,
	CoopCampaignBootstrapDecodeResult expected, const char* message)
{
	const CoopCampaignBootstrapDescriptor sentinel = Sentinel();
	CoopCampaignBootstrapDescriptor output = sentinel;
	CHECK(DecodeCoopCampaignBootstrap(bytes.data(), bytes.size(), output) ==
		expected, message);
	CHECK(SameCoopCampaignBootstrapDescriptor(output, sentinel),
		"failed bootstrap decode preserves output transactionally");
}

void TestGoldenVector()
{
	CHECK(std::string(CoopCampaignBootstrapMessageName) ==
		"coop.server.campaign-bootstrap",
		"campaign bootstrap message name is frozen");
	const CoopCampaignBootstrapDescriptor descriptor = Fixture();
	CHECK(IsValidCoopCampaignBootstrapDescriptor(descriptor),
		"complete campaign bootstrap descriptor is valid");

	CoopCampaignBootstrapBytes bytes{};
	CHECK(EncodeCoopCampaignBootstrap(descriptor, bytes),
		"complete campaign bootstrap descriptor encodes");
	const CoopCampaignBootstrapBytes expected{{
		0x4a, 0x32, 0x43, 0x42, 0x01, 0x00, 0x01, 0x00,
		0x07, 0x00, 0x00, 0x00,
		0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
		0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
		0x44, 0x2f, 0x73, 0xac, 0x09, 0x1d, 0x0d, 0x20,
		0x10, 0x67, 0x08, 0x56, 0xda, 0xa7, 0x3c, 0x16,
		0x7d, 0xe2, 0xf0, 0x75, 0x5d, 0xc8, 0x5b, 0x03,
		0xe3, 0x5e, 0x9d, 0x41, 0xdf, 0x29, 0x95, 0x66,
		0x0d, 0x0c, 0x0b, 0x0a,
		0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
		0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99,
		0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
		0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
		0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
		0x4f, 0x95, 0xab, 0x20,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	}};
	CHECK(bytes == expected,
		"campaign bootstrap pins the canonical 128-byte wire image");
	CHECK(DescriptorChecksum(bytes) == UINT32_C(0x20ab954f),
		"descriptor checksum pins FNV-1a over bytes 0 through 111");

	CoopCampaignBootstrapDescriptor decoded;
	CHECK(DecodeCoopCampaignBootstrap(bytes.data(), bytes.size(), decoded) ==
		CoopCampaignBootstrapDecodeResult::Success &&
		SameCoopCampaignBootstrapDescriptor(decoded, descriptor),
		"campaign bootstrap round trips every immutable field");
}

void TestZeroSeedAndFingerprintDomains()
{
	CoopCampaignBootstrapDescriptor descriptor = Fixture();
	descriptor.campaignSeed = 0;
	CHECK(ComputeCoopCampaignIdentitySha256("shared_01", 0,
		descriptor.campaignIdentitySha256),
		"zero-seed campaign identity hashes");
	descriptor.runtimeFingerprint.high = 0;
	descriptor.runtimeFingerprint.low = 0;
	CHECK(IsValidCoopCampaignBootstrapDescriptor(descriptor),
		"zero seed and zero fingerprint payload words remain valid");
	CoopCampaignBootstrapBytes bytes{};
	CHECK(EncodeCoopCampaignBootstrap(descriptor, bytes),
		"zero campaign seed encodes");
	CoopCampaignBootstrapDescriptor decoded;
	CHECK(DecodeCoopCampaignBootstrap(bytes.data(), bytes.size(), decoded) ==
		CoopCampaignBootstrapDecodeResult::Success &&
		SameCoopCampaignBootstrapDescriptor(decoded, descriptor),
		"zero campaign seed decodes without becoming a missing sentinel");
}

void TestExactDescriptorComparison()
{
	const CoopCampaignBootstrapDescriptor expected = Fixture();
	CHECK(SameCoopCampaignBootstrapDescriptor(expected, expected),
		"identical bootstrap descriptors compare equal");

	CoopCampaignBootstrapDescriptor altered = expected;
	altered.protocolVersion = static_cast<std::uint16_t>(
		expected.protocolVersion + 1);
	CHECK(!SameCoopCampaignBootstrapDescriptor(expected, altered),
		"protocol mismatch is exact");
	altered = expected;
	++altered.sessionEpoch;
	CHECK(!SameCoopCampaignBootstrapDescriptor(expected, altered),
		"session mismatch is exact");
	altered = expected;
	++altered.campaignSeed;
	CHECK(!SameCoopCampaignBootstrapDescriptor(expected, altered),
		"campaign seed mismatch is exact");
	altered = expected;
	altered.campaignIdentitySha256[0] ^= 1;
	CHECK(!SameCoopCampaignBootstrapDescriptor(expected, altered),
		"campaign identity mismatch is exact");
	altered = expected;
	++altered.runtimeFingerprint.schema;
	CHECK(!SameCoopCampaignBootstrapDescriptor(expected, altered),
		"runtime schema mismatch is exact");
	altered = expected;
	++altered.runtimeFingerprint.high;
	CHECK(!SameCoopCampaignBootstrapDescriptor(expected, altered),
		"runtime high word mismatch is exact");
	altered = expected;
	++altered.runtimeFingerprint.low;
	CHECK(!SameCoopCampaignBootstrapDescriptor(expected, altered),
		"runtime low word mismatch is exact");
	altered = expected;
	altered.contentManifestSha256[0] ^= 1;
	CHECK(!SameCoopCampaignBootstrapDescriptor(expected, altered),
		"content manifest mismatch is exact");
}

void TestEncodeValidationIsTransactional()
{
	const CoopCampaignBootstrapDescriptor fixture = Fixture();
	CoopCampaignBootstrapBytes output{};
	output.fill(0xa5);
	const CoopCampaignBootstrapBytes unchanged = output;

	auto rejects = [&](CoopCampaignBootstrapDescriptor invalid,
		const char* message) {
		CHECK(!IsValidCoopCampaignBootstrapDescriptor(invalid), message);
		CHECK(!EncodeCoopCampaignBootstrap(invalid, output) &&
			output == unchanged,
			"invalid bootstrap encoding preserves output transactionally");
	};

	CoopCampaignBootstrapDescriptor invalid = fixture;
	invalid.protocolVersion = 0;
	rejects(invalid, "zero protocol is invalid");
	invalid = fixture;
	invalid.protocolVersion = static_cast<std::uint16_t>(
		CurrentProtocolVersion + 1);
	rejects(invalid, "foreign protocol is invalid");
	invalid = fixture;
	invalid.sessionEpoch = 0;
	rejects(invalid, "zero session epoch is invalid");
	invalid = fixture;
	invalid.campaignIdentitySha256.fill(0);
	rejects(invalid, "zero campaign identity is invalid");
	invalid = fixture;
	invalid.runtimeFingerprint.schema = 0;
	rejects(invalid, "zero runtime schema is invalid");
	invalid = fixture;
	invalid.contentManifestSha256.fill(0);
	rejects(invalid, "zero content manifest is invalid");
}

void TestDecodeEnvelopeAndBounds()
{
	CoopCampaignBootstrapBytes bytes{};
	CHECK(EncodeCoopCampaignBootstrap(Fixture(), bytes),
		"decode-malformation fixture encodes");
	const CoopCampaignBootstrapDescriptor sentinel = Sentinel();
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopCampaignBootstrapDescriptor output = sentinel;
		CHECK(DecodeCoopCampaignBootstrap(bytes.data(), size, output) ==
			CoopCampaignBootstrapDecodeResult::WrongSize &&
			SameCoopCampaignBootstrapDescriptor(output, sentinel),
			"every truncated bootstrap is rejected transactionally");
	}
	CoopCampaignBootstrapDescriptor output = sentinel;
	CHECK(DecodeCoopCampaignBootstrap(nullptr, bytes.size(), output) ==
		CoopCampaignBootstrapDecodeResult::WrongSize &&
		SameCoopCampaignBootstrapDescriptor(output, sentinel),
		"null bootstrap input is rejected transactionally");
	std::array<std::uint8_t, CoopCampaignBootstrapWireSize + 1> oversized{};
	std::copy(bytes.begin(), bytes.end(), oversized.begin());
	CHECK(DecodeCoopCampaignBootstrap(oversized.data(), oversized.size(),
		output) == CoopCampaignBootstrapDecodeResult::WrongSize &&
		SameCoopCampaignBootstrapDescriptor(output, sentinel),
		"oversized bootstrap is rejected transactionally");

	for (std::size_t index = 0; index < 4; ++index)
	{
		CoopCampaignBootstrapBytes malformed = bytes;
		malformed[index] ^= 1;
		ExpectDecodeFailure(malformed,
			CoopCampaignBootstrapDecodeResult::WrongMagic,
			"every magic-byte corruption is rejected");
	}
	for (std::size_t index : {std::size_t(4), std::size_t(5)})
	{
		CoopCampaignBootstrapBytes malformed = bytes;
		malformed[index] ^= 1;
		ExpectDecodeFailure(malformed,
			CoopCampaignBootstrapDecodeResult::UnsupportedWireVersion,
			"every wire-version-byte corruption is rejected");
	}
	CoopCampaignBootstrapBytes malformed = bytes;
	malformed[6] = 2;
	ExpectDecodeFailure(malformed,
		CoopCampaignBootstrapDecodeResult::WrongMessageKind,
		"foreign bootstrap message kind is rejected");

	const std::array<std::size_t, 15> reserved{{
		7, 10, 11, 116, 117, 118, 119, 120, 121, 122, 123, 124,
		125, 126, 127}};
	for (const std::size_t index : reserved)
	{
		malformed = bytes;
		malformed[index] = 1;
		ExpectDecodeFailure(malformed,
			CoopCampaignBootstrapDecodeResult::NonZeroReserved,
			"every bootstrap reserved byte is canonical zero");
	}
	for (std::size_t index = ChecksumOffset;
		index < ChecksumOffset + sizeof(std::uint32_t); ++index)
	{
		malformed = bytes;
		malformed[index] ^= 1;
		ExpectDecodeFailure(malformed,
			CoopCampaignBootstrapDecodeResult::ChecksumMismatch,
			"every descriptor-checksum-byte corruption is rejected");
	}
}

void TestChecksumCoversEverySemanticField()
{
	CoopCampaignBootstrapBytes bytes{};
	CHECK(EncodeCoopCampaignBootstrap(Fixture(), bytes),
		"checksum-coverage fixture encodes");
	const std::array<std::size_t, 8> fields{{
		ProtocolOffset, SessionEpochOffset, CampaignSeedOffset,
		CampaignIdentityOffset, RuntimeSchemaOffset, RuntimeHighOffset,
		RuntimeLowOffset, ContentManifestOffset}};
	for (const std::size_t offset : fields)
	{
		CoopCampaignBootstrapBytes malformed = bytes;
		malformed[offset] ^= 0x80;
		ExpectDecodeFailure(malformed,
			CoopCampaignBootstrapDecodeResult::ChecksumMismatch,
			"FNV-1a covers every semantic descriptor field");
	}
}

void TestRechecksCanonicalSemanticsAfterChecksum()
{
	CoopCampaignBootstrapBytes bytes{};
	CHECK(EncodeCoopCampaignBootstrap(Fixture(), bytes),
		"semantic-malformation fixture encodes");

	CoopCampaignBootstrapBytes malformed = bytes;
	WriteU16(malformed, ProtocolOffset,
		static_cast<std::uint16_t>(CurrentProtocolVersion + 1));
	Rechecksum(malformed);
	ExpectDecodeFailure(malformed,
		CoopCampaignBootstrapDecodeResult::UnsupportedProtocol,
		"checksummed foreign protocol is rejected");

	malformed = bytes;
	std::fill(malformed.begin() + SessionEpochOffset,
		malformed.begin() + CampaignSeedOffset, 0);
	Rechecksum(malformed);
	ExpectDecodeFailure(malformed,
		CoopCampaignBootstrapDecodeResult::InvalidSemanticValue,
		"checksummed zero session epoch is rejected");

	malformed = bytes;
	std::fill(malformed.begin() + CampaignIdentityOffset,
		malformed.begin() + RuntimeSchemaOffset, 0);
	Rechecksum(malformed);
	ExpectDecodeFailure(malformed,
		CoopCampaignBootstrapDecodeResult::InvalidSemanticValue,
		"checksummed zero campaign identity is rejected");

	malformed = bytes;
	std::fill(malformed.begin() + RuntimeSchemaOffset,
		malformed.begin() + RuntimeHighOffset, 0);
	Rechecksum(malformed);
	ExpectDecodeFailure(malformed,
		CoopCampaignBootstrapDecodeResult::InvalidSemanticValue,
		"checksummed zero runtime schema is rejected");

	malformed = bytes;
	std::fill(malformed.begin() + ContentManifestOffset,
		malformed.begin() + ChecksumOffset, 0);
	Rechecksum(malformed);
	ExpectDecodeFailure(malformed,
		CoopCampaignBootstrapDecodeResult::InvalidSemanticValue,
		"checksummed zero content manifest is rejected");
}

void TestValidChangedFieldsStillRequireExactMatch()
{
	const CoopCampaignBootstrapDescriptor expected = Fixture();
	CoopCampaignBootstrapBytes original{};
	CHECK(EncodeCoopCampaignBootstrap(expected, original),
		"field-mismatch fixture encodes");

	auto decodesButDiffers = [&](CoopCampaignBootstrapBytes changed,
		const char* message) {
		Rechecksum(changed);
		CoopCampaignBootstrapDescriptor decoded;
		CHECK(DecodeCoopCampaignBootstrap(changed.data(), changed.size(),
			decoded) == CoopCampaignBootstrapDecodeResult::Success &&
			!SameCoopCampaignBootstrapDescriptor(decoded, expected), message);
	};

	CoopCampaignBootstrapBytes changed = original;
	changed[SessionEpochOffset] ^= 1;
	decodesButDiffers(changed,
		"valid changed session is decoded but exact-match rejected");
	changed = original;
	changed[CampaignSeedOffset] ^= 1;
	decodesButDiffers(changed,
		"valid changed seed is decoded but exact-match rejected");
	changed = original;
	changed[CampaignIdentityOffset] ^= 1;
	decodesButDiffers(changed,
		"valid changed campaign identity is exact-match rejected");
	changed = original;
	changed[RuntimeSchemaOffset] ^= 1;
	decodesButDiffers(changed,
		"valid changed runtime schema is exact-match rejected");
	changed = original;
	changed[RuntimeHighOffset] ^= 1;
	decodesButDiffers(changed,
		"valid changed runtime high word is exact-match rejected");
	changed = original;
	changed[RuntimeLowOffset] ^= 1;
	decodesButDiffers(changed,
		"valid changed runtime low word is exact-match rejected");
	changed = original;
	changed[ContentManifestOffset] ^= 1;
	decodesButDiffers(changed,
		"valid changed content identity is exact-match rejected");
}
}

int main()
{
	TestGoldenVector();
	TestZeroSeedAndFingerprintDomains();
	TestExactDescriptorComparison();
	TestEncodeValidationIsTransactional();
	TestDecodeEnvelopeAndBounds();
	TestChecksumCoversEverySemanticField();
	TestRechecksCanonicalSemanticsAfterChecksum();
	TestValidChangedFieldsStillRequireExactMatch();
	if (failures == 0)
		std::printf("all co-op campaign bootstrap protocol tests passed\n");
	return failures == 0 ? 0 : 1;
}
