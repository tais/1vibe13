#include "CoopHandshakeProtocol.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

using namespace CoopSession;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL: %s\n", message); \
		++failures; \
	} \
} while (false)

CoopServerHello Fixture()
{
	CoopServerHello hello;
	hello.sessionEpoch = UINT64_C(0x0102030405060708);
	hello.runtimeFingerprint = {
		UINT32_C(0x0a0b0c0d), UINT64_C(0x1122334455667788),
		UINT64_C(0x99aabbccddeeff00)};
	for (std::size_t index = 0;
		index < hello.contentManifestSha256.size(); ++index)
	{
		hello.contentManifestSha256[index] =
			static_cast<std::uint8_t>(0xa0 + index);
	}
	return hello;
}

void TestExactServerHello()
{
	const CoopServerHello hello = Fixture();
	CoopServerHelloBytes bytes{};
	CHECK(EncodeCoopServerHello(hello, bytes),
		"complete server hello encodes");
	const CoopServerHelloBytes expected{{
		0x4a, 0x32, 0x43, 0x48, 0x01, 0x00, 0x01, 0x00,
		0x07, 0x00, 0x00, 0x00,
		0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
		0x0d, 0x0c, 0x0b, 0x0a,
		0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
		0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99,
		0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
		0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
		0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf
	}};
	CHECK(bytes == expected,
		"extracted codec preserves the pinned 72-byte wire image");

	CoopServerHello decoded;
	CHECK(DecodeCoopServerHello(bytes.data(), bytes.size(), decoded) ==
		CoopServerHelloDecodeResult::Success &&
		decoded.protocolVersion == hello.protocolVersion &&
		decoded.sessionEpoch == hello.sessionEpoch &&
		decoded.runtimeFingerprint == hello.runtimeFingerprint &&
		decoded.contentManifestSha256 == hello.contentManifestSha256,
		"data-free handshake codec round trips every compatibility field");
}

void TestFailClosedServerHello()
{
	const CoopServerHello hello = Fixture();
	CoopServerHelloBytes bytes{};
	CHECK(EncodeCoopServerHello(hello, bytes),
		"malformation fixture encodes");
	CoopServerHello unchanged = hello;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopServerHello output = unchanged;
		CHECK(DecodeCoopServerHello(bytes.data(), size, output) ==
			CoopServerHelloDecodeResult::WrongSize &&
			output.sessionEpoch == unchanged.sessionEpoch,
			"every truncated hello is rejected transactionally");
	}
	CoopServerHelloBytes malformed = bytes;
	malformed[0] ^= 1;
	CHECK(DecodeCoopServerHello(malformed.data(), malformed.size(), unchanged) ==
		CoopServerHelloDecodeResult::WrongMagic,
		"hello rejects wrong magic");
	malformed = bytes;
	malformed[4] = 2;
	CHECK(DecodeCoopServerHello(malformed.data(), malformed.size(), unchanged) ==
		CoopServerHelloDecodeResult::UnsupportedWireVersion,
		"hello rejects a foreign wire version");
	malformed = bytes;
	malformed[6] = 2;
	CHECK(DecodeCoopServerHello(malformed.data(), malformed.size(), unchanged) ==
		CoopServerHelloDecodeResult::WrongMessageKind,
		"hello rejects a foreign message kind");
	for (std::size_t reserved : {std::size_t(7), std::size_t(10)})
	{
		malformed = bytes;
		malformed[reserved] = 1;
		CHECK(DecodeCoopServerHello(
			malformed.data(), malformed.size(), unchanged) ==
				CoopServerHelloDecodeResult::NonZeroReserved,
			"hello rejects every reserved field");
	}

	CoopServerHello invalid = hello;
	invalid.contentManifestSha256.fill(0);
	CoopServerHelloBytes output = bytes;
	CHECK(!EncodeCoopServerHello(invalid, output) && output == bytes,
		"invalid hello encoding is transactional");
	malformed = bytes;
	for (std::size_t index = 40; index < malformed.size(); ++index)
		malformed[index] = 0;
	CHECK(DecodeCoopServerHello(malformed.data(), malformed.size(), unchanged) ==
		CoopServerHelloDecodeResult::InvalidSemanticValue,
		"hello requires a nonzero content identity");
}
}

int main()
{
	TestExactServerHello();
	TestFailClosedServerHello();
	if (failures == 0)
		std::printf("all co-op handshake protocol tests passed\n");
	return failures == 0 ? 0 : 1;
}
