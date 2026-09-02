#include "OsAdmissionTokenSource.h"

#include <array>
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

void TestOsSecureBytes()
{
	CHECK(FillOsSecureRandomBytes(nullptr, 0),
		"zero-byte secure request is a no-op");
	CHECK(!FillOsSecureRandomBytes(nullptr, 1),
		"nonempty secure request rejects a null destination");

	std::array<std::uint8_t, 64> first{};
	std::array<std::uint8_t, 64> second{};
	CHECK(FillOsSecureRandomBytes(first.data(), first.size()) &&
		FillOsSecureRandomBytes(second.data(), second.size()),
		"operating-system cryptographic source fills bounded buffers");
	CHECK(first != second,
		"independent operating-system random reads do not repeat");
}

void TestAdmissionCredentialsAndEpochs()
{
	OsAdmissionTokenSource source;
	PeerIdentity firstIdentity{};
	ReconnectToken firstToken{};
	PeerIdentity secondIdentity{};
	ReconnectToken secondToken{};
	CHECK(source.issue(firstIdentity, firstToken) &&
		source.issue(secondIdentity, secondToken),
		"production source issues two admission credentials");
	CHECK(!IsZero(firstIdentity) && !IsZero(firstToken) &&
		!IsZero(secondIdentity) && !IsZero(secondToken),
		"production credentials are never zero sentinels");
	CHECK(firstIdentity != secondIdentity && firstToken != secondToken,
		"production credentials use fresh bearer material");

	std::uint64_t firstEpoch = 0;
	std::uint64_t secondEpoch = 0;
	CHECK(source.issueSessionEpoch(firstEpoch) &&
		source.issueSessionEpoch(secondEpoch),
		"production source issues two server session epochs");
	CHECK(firstEpoch != 0 && secondEpoch != 0 && firstEpoch != secondEpoch,
		"server session epochs are fresh nonzero random values");
}
}

int main()
{
	TestOsSecureBytes();
	TestAdmissionCredentialsAndEpochs();
	if (failures != 0)
	{
		std::printf("%d OS admission token source test(s) failed\n", failures);
		return 1;
	}
	std::puts("all OS admission token source tests passed");
	return 0;
}
